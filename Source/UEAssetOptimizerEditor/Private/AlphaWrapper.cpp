// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "AlphaWrapper.h"
#include "UEAssetOptimizerEditor.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Containers/Map.h"
#include "HAL/PlatformTime.h"

#include <array>
#include <vector>

#include "CGALIncludes.h"  // MUST go through this wrapper (Boost/UE macro isolation)
#include "MeshAssetOps.h"

namespace UEAOpt
{
	/**
	 * UE UStaticMesh (LOD0) -> CGAL triangle soup (vector of points + vector of
	 * face index triples). Mirrors the input shape used by CGAL's official
	 * `triangle_soup_wrap.cpp` example.
	 *
	 * Triangle soup is preferred over CGAL::Surface_mesh as the input form
	 * because alpha_wrap_3's primary use case is "defective 3D data" (CGAL's own
	 * wording): self-intersections, non-manifold edges, inconsistent winding,
	 * disconnected islands. Surface_mesh::add_face refuses non-manifold faces
	 * (silently dropping them and biasing the wrap), but the soup overload
	 * accepts everything and lets the algorithm handle robustness internally.
	 *
	 * Only degenerate triangles (two equal vertex indices in one face) are
	 * dropped here — those are not 2D simplices and CGAL has no use for them.
	 */
	static bool BuildTriangleSoupFromStaticMesh(
		const UStaticMesh* Source,
		std::vector<CGALPoint3>& OutPoints,
		std::vector<std::array<std::size_t, 3>>& OutFaces)
	{
#if WITH_EDITOR
		if (!Source)
		{
			return false;
		}

		const FMeshDescription* MD = Source->GetMeshDescription(0);
		if (!MD)
		{
			UE_LOG(LogUEAssetOptimizer, Warning,
				TEXT("BuildTriangleSoup: no MeshDescription on %s"),
				*Source->GetName());
			return false;
		}

		FStaticMeshConstAttributes Attr(*MD);
		TVertexAttributesConstRef<FVector3f> Positions = Attr.GetVertexPositions();

		const int32 NumVerts = MD->Vertices().Num();
		const int32 NumTris  = MD->Triangles().Num();
		if (NumVerts == 0 || NumTris == 0)
		{
			UE_LOG(LogUEAssetOptimizer, Warning,
				TEXT("BuildTriangleSoup: empty mesh on %s (V=%d T=%d)"),
				*Source->GetName(), NumVerts, NumTris);
			return false;
		}

		OutPoints.clear();
		OutFaces.clear();
		OutPoints.reserve(NumVerts);
		OutFaces.reserve(NumTris);

		// FVertexID is an opaque int32 wrapper; use its raw value as a key into
		// a flat array indexed by FMeshDescription's vertex array index.
		TMap<int32, std::size_t> VertexMap;
		VertexMap.Reserve(NumVerts);

		for (const FVertexID VertexID : MD->Vertices().GetElementIDs())
		{
			const FVector3f P = Positions[VertexID];
			VertexMap.Add(VertexID.GetValue(), OutPoints.size());
			OutPoints.emplace_back(
				static_cast<double>(P.X),
				static_cast<double>(P.Y),
				static_cast<double>(P.Z));
		}

		int32 SkippedDegenerate = 0;
		for (const FTriangleID TriangleID : MD->Triangles().GetElementIDs())
		{
			TArrayView<const FVertexID> TriVerts = MD->GetTriangleVertices(TriangleID);

			const std::size_t i0 = VertexMap[TriVerts[0].GetValue()];
			const std::size_t i1 = VertexMap[TriVerts[1].GetValue()];
			const std::size_t i2 = VertexMap[TriVerts[2].GetValue()];

			if (i0 == i1 || i1 == i2 || i0 == i2)
			{
				++SkippedDegenerate;
				continue;
			}

			OutFaces.push_back({ i0, i1, i2 });
		}

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("BuildTriangleSoup(%s): V=%d F=%d (skipped degenerate=%d)"),
			*Source->GetName(),
			(int32)OutPoints.size(),
			(int32)OutFaces.size(),
			SkippedDegenerate);

		return !OutFaces.empty();
#else
		return false;
#endif
	}

	/**
	 * CGAL Surface_mesh -> FMeshDescription, with explicit area-weighted
	 * smooth vertex normals.
	 *
	 * Two-pass over faces:
	 *   Pass 1: create UE vertices, accumulate unnormalized face normals into
	 *           per-vertex sums (cross-product magnitude = 2 * face area, so
	 *           summing gives area-weighted blending for free).
	 *   Pass 2: normalize accumulated normals, create vertex instances with
	 *           those normals, emit polygons with UE (CW) winding.
	 *
	 * We compute normals ourselves (and set bRecomputeNormals=false on the
	 * StaticMesh build settings) because UE's recomputation produced dark
	 * shading artifacts on alpha_wrap_3 output — likely due to near-zero-area
	 * triangles contributing to default smoothing.
	 */
	static void BuildMeshDescriptionFromCGAL(const CGALSurfaceMesh& InMesh, FMeshDescription& OutMD)
	{
		using VertexIndex = CGALSurfaceMesh::Vertex_index;

		FStaticMeshAttributes Attr(OutMD);
		Attr.Register();

		TVertexAttributesRef<FVector3f>         Positions      = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals        = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents       = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float>     BinormalSigns  = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs            = Attr.GetVertexInstanceUVs();
		TPolygonGroupAttributesRef<FName>       PGMatSlotNames = Attr.GetPolygonGroupMaterialSlotNames();

		OutMD.ReserveNewVertices          (static_cast<int32>(InMesh.number_of_vertices()));
		OutMD.ReserveNewVertexInstances   (static_cast<int32>(InMesh.number_of_faces()) * 3);
		OutMD.ReserveNewTriangles         (static_cast<int32>(InMesh.number_of_faces()));
		OutMD.ReserveNewPolygons          (static_cast<int32>(InMesh.number_of_faces()));

		const FPolygonGroupID PGID = OutMD.CreatePolygonGroup();
		PGMatSlotNames[PGID] = FName(TEXT("Default"));

		// --- Pass 0: create UE vertices keyed by CGAL vertex index ---
		TMap<uint32, FVertexID> VtxMap;
		TMap<uint32, FVector3f> AccumNormal;
		VtxMap     .Reserve(static_cast<int32>(InMesh.number_of_vertices()));
		AccumNormal.Reserve(static_cast<int32>(InMesh.number_of_vertices()));

		for (const VertexIndex V : InMesh.vertices())
		{
			const auto& P = InMesh.point(V);
			const FVertexID NewVID = OutMD.CreateVertex();
			Positions[NewVID] = FVector3f(
				static_cast<float>(CGAL::to_double(P.x())),
				static_cast<float>(CGAL::to_double(P.y())),
				static_cast<float>(CGAL::to_double(P.z())));
			VtxMap     .Add(static_cast<uint32>(V), NewVID);
			AccumNormal.Add(static_cast<uint32>(V), FVector3f::ZeroVector);
		}

		// Extract CCW vertex triple for a triangular face. Returns false if face
		// is not a triangle (alpha_wrap_3 shouldn't produce n-gons but be safe).
		auto GetTriVerts = [&](const CGALSurfaceMesh::Face_index F,
		                       VertexIndex& V0, VertexIndex& V1, VertexIndex& V2) -> bool
		{
			const auto H0 = InMesh.halfedge(F);
			const auto H1 = InMesh.next(H0);
			const auto H2 = InMesh.next(H1);
			if (InMesh.next(H2) != H0) return false;
			V0 = InMesh.source(H0);
			V1 = InMesh.source(H1);
			V2 = InMesh.source(H2);
			return true;
		};

		// --- Pass 1: accumulate area-weighted face normals per vertex ---
		int32 NonTriSkipped = 0;
		for (const auto F : InMesh.faces())
		{
			VertexIndex V0, V1, V2;
			if (!GetTriVerts(F, V0, V1, V2)) { ++NonTriSkipped; continue; }

			const FVector3f P0 = Positions[VtxMap[static_cast<uint32>(V0)]];
			const FVector3f P1 = Positions[VtxMap[static_cast<uint32>(V1)]];
			const FVector3f P2 = Positions[VtxMap[static_cast<uint32>(V2)]];

			// Outward face normal: CGAL Surface_mesh halfedges are ordered CCW
			// when viewed from outside, so (P1-P0) x (P2-P0) points outward by
			// the right-hand rule. The pass-2 winding swap (V0, V2, V1) only
			// affects UE's front/back-face determination -- vertex normals are
			// independent of winding and must point outward relative to the
			// surface for lighting (NdotL) to read positive. Earlier code had
			// E1/E2 swapped, producing inward normals and a black mesh under
			// lit shading.
			//
			// Cross product is unnormalized so |FN| = 2 * face area, giving
			// area-weighted blending when summed across incident faces.
			const FVector3f E1 = P1 - P0;
			const FVector3f E2 = P2 - P0;
			const FVector3f FN = FVector3f::CrossProduct(E1, E2);

			AccumNormal[static_cast<uint32>(V0)] += FN;
			AccumNormal[static_cast<uint32>(V1)] += FN;
			AccumNormal[static_cast<uint32>(V2)] += FN;
		}

		// Normalize accumulated normals.
		for (auto& Pair : AccumNormal)
		{
			const float Len = Pair.Value.Length();
			if (Len > KINDA_SMALL_NUMBER)
			{
				Pair.Value /= Len;
			}
			else
			{
				// Isolated / degenerate vertex. Give it an arbitrary up-vector
				// so downstream shading doesn't read a zero normal.
				Pair.Value = FVector3f(0.f, 0.f, 1.f);
			}
		}

		// --- Pass 2: create vertex instances + polygons with UE winding ---
		// Build an arbitrary tangent frame from the normal. UE warns if tangents
		// are left zero; we have no UVs so MikkTSpace can't derive them, and
		// the wrap's intended uses (collision / cage) don't need real tangents.
		// Any orthonormal basis silences the warning without affecting shading
		// (which only uses the normal in a UV-less workflow).
		auto ArbitraryTangent = [](const FVector3f& N) -> FVector3f
		{
			const FVector3f Ref = (FMath::Abs(N.X) < 0.9f)
				? FVector3f(1.f, 0.f, 0.f)
				: FVector3f(0.f, 1.f, 0.f);
			return FVector3f::CrossProduct(N, Ref).GetSafeNormal(
				KINDA_SMALL_NUMBER, FVector3f(1.f, 0.f, 0.f));
		};

		auto CreateVI = [&](VertexIndex CVtx) -> FVertexInstanceID
		{
			const FVertexID VID = VtxMap[static_cast<uint32>(CVtx)];
			const FVertexInstanceID VIID = OutMD.CreateVertexInstance(VID);
			const FVector3f N = AccumNormal[static_cast<uint32>(CVtx)];
			Normals      [VIID] = N;
			Tangents     [VIID] = ArbitraryTangent(N);
			BinormalSigns[VIID] = 1.0f;
			UVs          [VIID] = FVector2f::ZeroVector;
			return VIID;
		};

		for (const auto F : InMesh.faces())
		{
			VertexIndex V0, V1, V2;
			if (!GetTriVerts(F, V0, V1, V2)) continue;  // already counted

			// CGAL gives CCW (V0, V1, V2); UE expects CW for front-face, so
			// emit as (V0, V2, V1).
			const FVertexInstanceID VIa = CreateVI(V0);
			const FVertexInstanceID VIb = CreateVI(V2);
			const FVertexInstanceID VIc = CreateVI(V1);
			const FVertexInstanceID Tri[3] = { VIa, VIb, VIc };
			OutMD.CreatePolygon(PGID, TArrayView<const FVertexInstanceID>(Tri, 3));
		}

		if (NonTriSkipped > 0)
		{
			UE_LOG(LogUEAssetOptimizer, Warning,
				TEXT("BuildMeshDescriptionFromCGAL: skipped %d non-triangular face(s)"),
				NonTriSkipped);
		}
	}

	/**
	 * Create a new UStaticMesh asset `<SourceName>_Wrap` in the source asset's
	 * package folder, populated from the given MeshDescription.
	 */
	static UStaticMesh* CreateWrappedAsset(const UStaticMesh* Source, FMeshDescription&& MD)
	{
#if WITH_EDITOR
		if (!Source)
		{
			return nullptr;
		}

		bool bIsNew = false;
		UStaticMesh* NewMesh = CreateOrOverwriteSiblingAsset(Source, TEXT("_Wrap"), bIsNew);
		if (!NewMesh)
		{
			return nullptr;
		}

		// Inherit the source mesh's first material so the wrap renders with the
		// same look when placed in a scene without user assignment. Only the
		// first slot is used because the wrap has a single polygon group.
		NewMesh->GetStaticMaterials().Reset();
		const TArray<FStaticMaterial>& SrcMaterials = Source->GetStaticMaterials();
		if (SrcMaterials.Num() > 0)
		{
			NewMesh->GetStaticMaterials().Add(SrcMaterials[0]);
		}
		else
		{
			NewMesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Default"), TEXT("Default")));
		}

		// Ensure LOD0 source model exists, then commit MeshDescription.
		if (NewMesh->GetNumSourceModels() == 0)
		{
			NewMesh->AddSourceModel();
		}
		CommitLOD(NewMesh, 0, MoveTemp(MD), 1.f);

		FinalizeAsset(NewMesh, bIsNew);

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("CreateWrappedAsset: %s -> %s (%s)"),
			*Source->GetName(), *NewMesh->GetName(),
			bIsNew ? TEXT("new") : TEXT("overwritten"));

		return NewMesh;
#else
		return nullptr;
#endif
	}
}

UStaticMesh* UAlphaWrapper::CreateAlphaWrap(UStaticMesh* Source, const FAlphaWrapParams& Params)
{
	if (!Source)
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("CreateAlphaWrap: null source mesh"));
		return nullptr;
	}

	// 1. UE -> CGAL
	std::vector<UEAOpt::CGALPoint3> InPoints;
	std::vector<std::array<std::size_t, 3>> InFaces;
	bool bConverted = false;
	try
	{
		bConverted = UEAOpt::BuildTriangleSoupFromStaticMesh(Source, InPoints, InFaces);
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogUEAssetOptimizer, Error,
			TEXT("CreateAlphaWrap: triangle soup build threw: %s"),
			UTF8_TO_TCHAR(e.what()));
		return nullptr;
	}
	if (!bConverted)
	{
		return nullptr;
	}

	// 2. Resolve absolute alpha / offset from bbox diagonal.
	// Match CGAL's triangle_soup_wrap.cpp pattern: accumulate Bbox_3 from points
	// directly. We don't build a Surface_mesh as input, so we can't use
	// Polygon_mesh_processing::bbox().
	double DiagonalLength = 0.0;
	try
	{
		CGAL::Bbox_3 Bbox;
		for (const auto& P : InPoints)
		{
			Bbox += P.bbox();
		}
		const double dx = Bbox.xmax() - Bbox.xmin();
		const double dy = Bbox.ymax() - Bbox.ymin();
		const double dz = Bbox.zmax() - Bbox.zmin();
		DiagonalLength = FMath::Sqrt(dx * dx + dy * dy + dz * dz);
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogUEAssetOptimizer, Error,
			TEXT("CreateAlphaWrap: bbox threw: %s"), UTF8_TO_TCHAR(e.what()));
		return nullptr;
	}

	double RelAlpha  = Params.RelativeAlpha;
	double RelOffset = Params.RelativeOffset;
	switch (Params.Purpose)
	{
	case EAlphaWrapPurpose::Collision:   RelAlpha = 15.0; RelOffset = 500.0; break;
	case EAlphaWrapPurpose::BakingCage:  RelAlpha = 25.0; RelOffset = 800.0; break;
	case EAlphaWrapPurpose::Cleanup:     RelAlpha = 20.0; RelOffset = 600.0; break;
	case EAlphaWrapPurpose::Custom:      /* use Params.* as given */        break;
	}
	const double AbsAlpha  = DiagonalLength / RelAlpha;
	const double AbsOffset = DiagonalLength / RelOffset;

	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("CreateAlphaWrap: %s | purpose=%d | bbox diagonal=%.3f cm | alpha=%.3f offset=%.3f"),
		*Source->GetName(),
		static_cast<int32>(Params.Purpose),
		DiagonalLength, AbsAlpha, AbsOffset);

	// 3. Run alpha_wrap_3 (triangle soup overload).
	UEAOpt::CGALSurfaceMesh WrappedMesh;
	const double WrapStart = FPlatformTime::Seconds();
	try
	{
		CGAL::alpha_wrap_3(InPoints, InFaces, AbsAlpha, AbsOffset, WrappedMesh);
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogUEAssetOptimizer, Error,
			TEXT("CreateAlphaWrap: alpha_wrap_3 threw: %s"), UTF8_TO_TCHAR(e.what()));
		return nullptr;
	}
	const double WrapElapsed = FPlatformTime::Seconds() - WrapStart;

	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("alpha_wrap_3 done in %.2fs: in V=%d F=%d -> out V=%d F=%d"),
		WrapElapsed,
		(int32)InPoints.size(), (int32)InFaces.size(),
		(int32)WrappedMesh.number_of_vertices(), (int32)WrappedMesh.number_of_faces());

	if (WrappedMesh.number_of_faces() == 0)
	{
		UE_LOG(LogUEAssetOptimizer, Warning,
			TEXT("CreateAlphaWrap: wrapped mesh is empty — alpha/offset may be too coarse"));
		return nullptr;
	}

	// 3b. Optional: isotropic remeshing to smooth out alpha_wrap_3's sampling
	// artifacts (visible as micro-patches under smooth shading). Target edge
	// length of alpha/2 balances smoothness against triangle count growth.
	if (Params.bEnableRemeshing)
	{
		const double TargetEdge = AbsAlpha * 0.5;
		const unsigned int NbIter = 3;
		const int32 BeforeV = (int32)WrappedMesh.number_of_vertices();
		const int32 BeforeF = (int32)WrappedMesh.number_of_faces();
		const double RemeshStart = FPlatformTime::Seconds();
		try
		{
			CGAL::Polygon_mesh_processing::isotropic_remeshing(
				CGAL::faces(WrappedMesh),
				TargetEdge,
				WrappedMesh,
				CGAL::parameters::number_of_iterations(NbIter));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogUEAssetOptimizer, Warning,
				TEXT("CreateAlphaWrap: isotropic_remeshing threw: %s (continuing with unremeshed wrap)"),
				UTF8_TO_TCHAR(e.what()));
		}
		const double RemeshElapsed = FPlatformTime::Seconds() - RemeshStart;

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("isotropic_remeshing %.2fs (iter=%u target edge=%.3f): V=%d->%d F=%d->%d"),
			RemeshElapsed, NbIter, TargetEdge,
			BeforeV, (int32)WrappedMesh.number_of_vertices(),
			BeforeF, (int32)WrappedMesh.number_of_faces());
	}

	// 4. CGAL -> FMeshDescription.
	FMeshDescription OutMD;
	const double ConvStart = FPlatformTime::Seconds();
	UEAOpt::BuildMeshDescriptionFromCGAL(WrappedMesh, OutMD);
	const double ConvElapsed = FPlatformTime::Seconds() - ConvStart;

	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("CGAL->MeshDescription in %.3fs: V=%d VI=%d T=%d"),
		ConvElapsed,
		OutMD.Vertices().Num(),
		OutMD.VertexInstances().Num(),
		OutMD.Triangles().Num());

	// 5. Create new <Source>_Wrap UStaticMesh asset.
	UStaticMesh* NewMesh = UEAOpt::CreateWrappedAsset(Source, MoveTemp(OutMD));
	return NewMesh;
}
