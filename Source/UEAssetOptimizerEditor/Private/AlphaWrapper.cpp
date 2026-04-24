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

#include "CGALIncludes.h"  // MUST go through this wrapper (Boost/UE macro isolation)

namespace UEAOpt
{
	/**
	 * UE UStaticMesh (LOD0) -> CGAL Surface_mesh.
	 *
	 * Degenerate triangles (repeated vertex indices) and triangles CGAL refuses
	 * due to non-manifold local topology are counted and skipped — alpha_wrap_3
	 * is robust to incomplete input, which is the main reason we use it.
	 */
	static bool BuildCGALMeshFromStaticMesh(const UStaticMesh* Source, CGALSurfaceMesh& OutMesh)
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
				TEXT("BuildCGALMeshFromStaticMesh: no MeshDescription on %s"),
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
				TEXT("BuildCGALMeshFromStaticMesh: empty mesh on %s (V=%d T=%d)"),
				*Source->GetName(), NumVerts, NumTris);
			return false;
		}

		OutMesh.clear();
		OutMesh.reserve(NumVerts, 0, NumTris);

		TMap<int32, CGALSurfaceMesh::Vertex_index> VertexMap;
		VertexMap.Reserve(NumVerts);

		for (const FVertexID VertexID : MD->Vertices().GetElementIDs())
		{
			const FVector3f P = Positions[VertexID];
			const auto CGALIdx = OutMesh.add_vertex(CGALPoint3(
				static_cast<double>(P.X),
				static_cast<double>(P.Y),
				static_cast<double>(P.Z)));
			VertexMap.Add(VertexID.GetValue(), CGALIdx);
		}

		int32 SkippedDegenerate = 0;
		int32 SkippedNonManifold = 0;
		for (const FTriangleID TriangleID : MD->Triangles().GetElementIDs())
		{
			TArrayView<const FVertexID> TriVerts = MD->GetTriangleVertices(TriangleID);

			const auto v0 = VertexMap[TriVerts[0].GetValue()];
			const auto v1 = VertexMap[TriVerts[1].GetValue()];
			const auto v2 = VertexMap[TriVerts[2].GetValue()];

			if (v0 == v1 || v1 == v2 || v0 == v2)
			{
				++SkippedDegenerate;
				continue;
			}

			const auto Face = OutMesh.add_face(v0, v1, v2);
			if (Face == CGALSurfaceMesh::null_face())
			{
				++SkippedNonManifold;
			}
		}

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("BuildCGALMeshFromStaticMesh(%s): V=%d F=%d (skipped: degenerate=%d non-manifold=%d)"),
			*Source->GetName(),
			(int32)OutMesh.number_of_vertices(),
			(int32)OutMesh.number_of_faces(),
			SkippedDegenerate, SkippedNonManifold);

		return OutMesh.number_of_faces() > 0;
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

			// After winding reversal (V0, V2, V1) for UE's CW convention, the
			// face normal in UE space is cross(P2-P0, P1-P0). Not normalized
			// so magnitude naturally weights by face area in the sum.
			const FVector3f E1 = P2 - P0;
			const FVector3f E2 = P1 - P0;
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

		// Target package path: sibling of the source, named "<Source>_Wrap".
		const FString SourcePackageName = Source->GetOutermost()->GetName();
		const FString ParentPath        = FPackageName::GetLongPackagePath(SourcePackageName);
		const FString NewAssetName      = Source->GetName() + TEXT("_Wrap");
		const FString NewPackageName    = ParentPath / NewAssetName;

		// If an asset with that name already exists, overwrite its contents.
		UPackage* NewPackage = CreatePackage(*NewPackageName);
		NewPackage->FullyLoad();

		UStaticMesh* NewMesh = FindObject<UStaticMesh>(NewPackage, *NewAssetName);
		const bool bIsNew = (NewMesh == nullptr);
		if (bIsNew)
		{
			NewMesh = NewObject<UStaticMesh>(NewPackage, *NewAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		// Inherit the source mesh's first material so the wrap renders with the
		// same look when placed in a scene without user assignment. A nullptr
		// slot falls back to UE's dark DefaultMaterial, which makes the wrap
		// appear nearly black — not what users expect for a "copy" of the
		// source. Only the first slot is used because the wrap has a single
		// polygon group.
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

		// Source model (LOD0) with sensible defaults.
		if (NewMesh->GetNumSourceModels() == 0)
		{
			NewMesh->AddSourceModel();
		}
		FStaticMeshSourceModel& SourceModel = NewMesh->GetSourceModel(0);
		// We supply explicit area-weighted smooth normals from CGAL space, so
		// tell UE to keep them as-is. Tangents are still skipped because the
		// mesh has no UVs and MikkTSpace would warn.
		SourceModel.BuildSettings.bRecomputeNormals      = false;
		SourceModel.BuildSettings.bRecomputeTangents     = false;
		SourceModel.BuildSettings.bUseMikkTSpace         = false;
		SourceModel.BuildSettings.bGenerateLightmapUVs   = false;
		SourceModel.BuildSettings.bBuildReversedIndexBuffer = false;

		// Commit MeshDescription into the source model.
		FMeshDescription* DstMD = NewMesh->CreateMeshDescription(0, MoveTemp(MD));
		check(DstMD);
		NewMesh->CommitMeshDescription(0);

		// Build render data.
		NewMesh->PostEditChange();
		NewMesh->Build(/*bSilent*/ false);
		NewMesh->MarkPackageDirty();

		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(NewMesh);
		}

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
	UEAOpt::CGALSurfaceMesh InMesh;
	bool bConverted = false;
	try
	{
		bConverted = UEAOpt::BuildCGALMeshFromStaticMesh(Source, InMesh);
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogUEAssetOptimizer, Error,
			TEXT("CreateAlphaWrap: CGAL mesh build threw: %s"),
			UTF8_TO_TCHAR(e.what()));
		return nullptr;
	}
	if (!bConverted)
	{
		return nullptr;
	}

	// 2. Resolve absolute alpha / offset from bbox diagonal.
	double DiagonalLength = 0.0;
	try
	{
		const auto Bbox = CGAL::Polygon_mesh_processing::bbox(InMesh);
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

	// 3. Run alpha_wrap_3.
	UEAOpt::CGALSurfaceMesh WrappedMesh;
	const double WrapStart = FPlatformTime::Seconds();
	try
	{
		CGAL::alpha_wrap_3(InMesh, AbsAlpha, AbsOffset, WrappedMesh);
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
		(int32)InMesh.number_of_vertices(),    (int32)InMesh.number_of_faces(),
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
