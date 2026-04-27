// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "LODGenerator.h"
#include "UEAssetOptimizerEditor.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Crc.h"
#include "Containers/Map.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"

#include "MeshOptIncludes.h"

namespace UEAOpt
{
	// ---------------------------------------------------------------------
	// Flat wedge-deduped mesh for meshoptimizer consumption.
	// ---------------------------------------------------------------------
	struct FFlatMesh
	{
		TArray<FVector3f> Positions;   // per unique (pos, normal, uv) wedge
		TArray<FVector3f> Normals;     // parallel to Positions
		TArray<FVector2f> UVs;         // parallel to Positions (UV channel 0 only for v1)
		TArray<uint32>    Indices;     // triangle indices; Indices.Num() == 3 * NumTris
	};

	// Dedup key: a wedge is identified by its exact (pos, normal, uv) tuple.
	struct FWedgeKey
	{
		FVector3f P;
		FVector3f N;
		FVector2f UV;

		friend bool operator==(const FWedgeKey& A, const FWedgeKey& B)
		{
			return FMemory::Memcmp(&A, &B, sizeof(FWedgeKey)) == 0;
		}
	};
	static_assert(sizeof(FWedgeKey) == 3 * sizeof(float) * 2 + 2 * sizeof(float),
		"FWedgeKey must be tightly packed (no padding) for bitwise hash/equality");

	inline uint32 GetTypeHash(const FWedgeKey& K)
	{
		return FCrc::MemCrc32(&K, sizeof(FWedgeKey));
	}

	// ---------------------------------------------------------------------
	// T18: FMeshDescription -> FFlatMesh (wedge dedup by (pos, normal, uv))
	// ---------------------------------------------------------------------
	static bool BuildFlatMeshFromDescription(const FMeshDescription& MD, FFlatMesh& Out)
	{
		const int32 NumTris = MD.Triangles().Num();
		if (NumTris == 0)
		{
			return false;
		}

		const FStaticMeshConstAttributes Attr(MD);
		TVertexAttributesConstRef<FVector3f>         Positions = Attr.GetVertexPositions();
		TVertexInstanceAttributesConstRef<FVector3f> Normals   = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesConstRef<FVector2f> UVs       = Attr.GetVertexInstanceUVs();

		Out.Positions.Reset();
		Out.Normals  .Reset();
		Out.UVs      .Reset();
		Out.Indices  .Reset();
		Out.Positions.Reserve(MD.Vertices().Num());
		Out.Normals  .Reserve(MD.Vertices().Num());
		Out.UVs      .Reserve(MD.Vertices().Num());
		Out.Indices  .Reserve(NumTris * 3);

		TMap<FWedgeKey, uint32> Dedup;
		Dedup.Reserve(NumTris * 3);

		auto GetOrAddWedge = [&](FVertexInstanceID VIID) -> uint32
		{
			const FVertexID VID = MD.GetVertexInstanceVertex(VIID);
			FWedgeKey K{};
			K.P  = Positions[VID];
			K.N  = Normals  [VIID];
			K.UV = UVs      [VIID];
			if (const uint32* Existing = Dedup.Find(K))
			{
				return *Existing;
			}
			const uint32 NewIndex = static_cast<uint32>(Out.Positions.Num());
			Out.Positions.Add(K.P);
			Out.Normals  .Add(K.N);
			Out.UVs      .Add(K.UV);
			Dedup.Add(K, NewIndex);
			return NewIndex;
		};

		for (const FTriangleID TID : MD.Triangles().GetElementIDs())
		{
			const TArrayView<const FVertexInstanceID> TriVIs = MD.GetTriangleVertexInstances(TID);
			Out.Indices.Add(GetOrAddWedge(TriVIs[0]));
			Out.Indices.Add(GetOrAddWedge(TriVIs[1]));
			Out.Indices.Add(GetOrAddWedge(TriVIs[2]));
		}

		return true;
	}

	// ---------------------------------------------------------------------
	// T19: FFlatMesh -> FMeshDescription (reuses Sprint 2 attribute pattern)
	// ---------------------------------------------------------------------
	static void BuildDescriptionFromFlatMesh(const FFlatMesh& In, FMeshDescription& Out)
	{
		FStaticMeshAttributes Attr(Out);
		Attr.Register();

		TVertexAttributesRef<FVector3f>         Positions      = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals        = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents       = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float>     BinormalSigns  = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs            = Attr.GetVertexInstanceUVs();
		TPolygonGroupAttributesRef<FName>       PGMatSlotNames = Attr.GetPolygonGroupMaterialSlotNames();

		const int32 NumVerts = In.Positions.Num();
		const int32 NumTris  = In.Indices.Num() / 3;

		Out.ReserveNewVertices        (NumVerts);
		Out.ReserveNewVertexInstances (NumTris * 3);
		Out.ReserveNewTriangles       (NumTris);
		Out.ReserveNewPolygons        (NumTris);

		const FPolygonGroupID PGID = Out.CreatePolygonGroup();
		PGMatSlotNames[PGID] = FName(TEXT("Default"));

		TArray<FVertexID> VtxIDs;
		VtxIDs.Reserve(NumVerts);
		for (int32 i = 0; i < NumVerts; ++i)
		{
			const FVertexID VID = Out.CreateVertex();
			Positions[VID] = In.Positions[i];
			VtxIDs.Add(VID);
		}

		// Non-UV tangent frame (same strategy as Sprint 2 Alpha Wrap): silences
		// UE's "nearly zero tangents" build warning without depending on MikkTSpace.
		auto ArbitraryTangent = [](const FVector3f& N) -> FVector3f
		{
			const FVector3f Ref = (FMath::Abs(N.X) < 0.9f)
				? FVector3f(1.f, 0.f, 0.f)
				: FVector3f(0.f, 1.f, 0.f);
			return FVector3f::CrossProduct(N, Ref).GetSafeNormal(
				KINDA_SMALL_NUMBER, FVector3f(1.f, 0.f, 0.f));
		};

		for (int32 t = 0; t < NumTris; ++t)
		{
			const uint32 I0 = In.Indices[t * 3 + 0];
			const uint32 I1 = In.Indices[t * 3 + 1];
			const uint32 I2 = In.Indices[t * 3 + 2];

			const FVertexInstanceID VI0 = Out.CreateVertexInstance(VtxIDs[I0]);
			const FVertexInstanceID VI1 = Out.CreateVertexInstance(VtxIDs[I1]);
			const FVertexInstanceID VI2 = Out.CreateVertexInstance(VtxIDs[I2]);

			auto SetupVI = [&](FVertexInstanceID VI, uint32 Idx)
			{
				const FVector3f N = In.Normals[Idx];
				Normals      [VI] = N;
				Tangents     [VI] = ArbitraryTangent(N);
				BinormalSigns[VI] = 1.f;
				UVs          [VI] = In.UVs[Idx];
			};
			SetupVI(VI0, I0);
			SetupVI(VI1, I1);
			SetupVI(VI2, I2);

			const FVertexInstanceID Tri[3] = { VI0, VI1, VI2 };
			Out.CreatePolygon(PGID, TArrayView<const FVertexInstanceID>(Tri, 3));
		}
	}

	// ---------------------------------------------------------------------
	// T20: meshoptimizer simplify wrapper
	// ---------------------------------------------------------------------
	struct FSimplifyResult
	{
		int32  InputIndexCount  = 0;
		int32  OutputIndexCount = 0;
		int32  TargetIndexCount = 0;
		float  Error            = 0.f;
		double ElapsedSec       = 0.0;
		bool   bSloppy          = false;
	};

	static FSimplifyResult SimplifyFlatMesh(
		const FFlatMesh& In,
		float Ratio,
		const FLODGenerationParams& Params,
		bool bUseSloppy,
		TArray<uint32>& OutIndices)
	{
		FSimplifyResult R;
		R.InputIndexCount = In.Indices.Num();
		// Round target to multiple of 3 so partial triangles don't slip through.
		R.TargetIndexCount = FMath::Max(3,
			(FMath::RoundToInt(In.Indices.Num() * Ratio) / 3) * 3);
		R.bSloppy = bUseSloppy;

		OutIndices.SetNum(In.Indices.Num());

		// Interleave (normal.xyz, uv.xy) = 5 floats/vertex for simplifyWithAttributes.
		TArray<float> Attributes;
		if (!bUseSloppy)
		{
			Attributes.Reserve(In.Positions.Num() * 5);
			for (int32 i = 0; i < In.Positions.Num(); ++i)
			{
				const FVector3f& N = In.Normals[i];
				const FVector2f& U = In.UVs[i];
				Attributes.Add(N.X); Attributes.Add(N.Y); Attributes.Add(N.Z);
				Attributes.Add(U.X); Attributes.Add(U.Y);
			}
		}

		// Tuned weights: normals at unity, UVs at 0.5 (UVs are in [0..1], half
		// weight keeps them from dominating the simplification error metric).
		const float AttrWeights[5] = { 1.f, 1.f, 1.f, 0.5f, 0.5f };

		unsigned int Options = 0;
		if (Params.bLockBorders)   Options |= meshopt_SimplifyLockBorder;
		if (Params.bAbsoluteError) Options |= meshopt_SimplifyErrorAbsolute;

		float  ResultError = 0.f;
		size_t OutCount    = 0;
		const double Start = FPlatformTime::Seconds();

		if (bUseSloppy)
		{
			OutCount = meshopt_simplifySloppy(
				OutIndices.GetData(),
				reinterpret_cast<const unsigned int*>(In.Indices.GetData()), In.Indices.Num(),
				reinterpret_cast<const float*>(In.Positions.GetData()),
				In.Positions.Num(), sizeof(FVector3f),
				static_cast<size_t>(R.TargetIndexCount), Params.TargetError,
				&ResultError);
		}
		else
		{
			OutCount = meshopt_simplifyWithAttributes(
				OutIndices.GetData(),
				reinterpret_cast<const unsigned int*>(In.Indices.GetData()), In.Indices.Num(),
				reinterpret_cast<const float*>(In.Positions.GetData()),
				In.Positions.Num(), sizeof(FVector3f),
				Attributes.GetData(), 5 * sizeof(float),
				AttrWeights, 5,
				/*vertex_lock*/ nullptr,
				static_cast<size_t>(R.TargetIndexCount), Params.TargetError,
				Options, &ResultError);
		}

		R.ElapsedSec       = FPlatformTime::Seconds() - Start;
		R.OutputIndexCount = static_cast<int32>(OutCount);
		R.Error            = ResultError;
		OutIndices.SetNum(static_cast<int32>(OutCount));
		return R;
	}

	// Apply vertex-cache + vertex-fetch optimization. Trims the vertex buffer
	// to only the vertices referenced by Indices.
	static void OptimizeVertexOrder(FFlatMesh& InOut)
	{
		const int32 VertCount = InOut.Positions.Num();
		if (VertCount == 0 || InOut.Indices.Num() == 0)
		{
			return;
		}

		// 1. Index reorder for vertex-cache locality (in-place via tmp buffer).
		TArray<uint32> OptIndices;
		OptIndices.SetNum(InOut.Indices.Num());
		meshopt_optimizeVertexCache(
			OptIndices.GetData(),
			reinterpret_cast<const unsigned int*>(InOut.Indices.GetData()),
			InOut.Indices.Num(),
			static_cast<size_t>(VertCount));

		// 2. Remap table for vertex-fetch order (compacts unused vertices out).
		TArray<uint32> Remap;
		Remap.SetNum(VertCount);
		const size_t NewVertCount = meshopt_optimizeVertexFetchRemap(
			Remap.GetData(),
			reinterpret_cast<const unsigned int*>(OptIndices.GetData()),
			OptIndices.Num(),
			static_cast<size_t>(VertCount));

		// 3. Remap each vertex stream and the index buffer.
		TArray<FVector3f> NewPositions; NewPositions.SetNum(NewVertCount);
		TArray<FVector3f> NewNormals;   NewNormals  .SetNum(NewVertCount);
		TArray<FVector2f> NewUVs;       NewUVs      .SetNum(NewVertCount);
		TArray<uint32>    NewIndices;   NewIndices  .SetNum(OptIndices.Num());

		meshopt_remapVertexBuffer(NewPositions.GetData(), InOut.Positions.GetData(),
			static_cast<size_t>(VertCount), sizeof(FVector3f), Remap.GetData());
		meshopt_remapVertexBuffer(NewNormals.GetData(), InOut.Normals.GetData(),
			static_cast<size_t>(VertCount), sizeof(FVector3f), Remap.GetData());
		meshopt_remapVertexBuffer(NewUVs.GetData(), InOut.UVs.GetData(),
			static_cast<size_t>(VertCount), sizeof(FVector2f), Remap.GetData());
		meshopt_remapIndexBuffer(NewIndices.GetData(),
			reinterpret_cast<const unsigned int*>(OptIndices.GetData()),
			OptIndices.Num(), Remap.GetData());

		InOut.Positions = MoveTemp(NewPositions);
		InOut.Normals   = MoveTemp(NewNormals);
		InOut.UVs       = MoveTemp(NewUVs);
		InOut.Indices   = MoveTemp(NewIndices);
	}

	// ---------------------------------------------------------------------
	// T21: WriteLODsInPlace
	// ---------------------------------------------------------------------
	static bool WriteLODsInPlace(
		UStaticMesh* Source,
		TArray<FMeshDescription>&& LODs,
		const TArray<float>& Ratios)
	{
#if WITH_EDITOR
		if (!Source || LODs.Num() == 0 || LODs.Num() != Ratios.Num())
		{
			return false;
		}

		Source->Modify();

		const int32 TotalLODs = 1 + LODs.Num(); // LOD0 (original) + generated
		Source->SetNumSourceModels(TotalLODs);

		for (int32 i = 0; i < LODs.Num(); ++i)
		{
			const int32 LODIndex = i + 1;
			FStaticMeshSourceModel& SM = Source->GetSourceModel(LODIndex);

			SM.BuildSettings.bRecomputeNormals         = false;
			SM.BuildSettings.bRecomputeTangents        = false;
			SM.BuildSettings.bUseMikkTSpace            = false;
			SM.BuildSettings.bGenerateLightmapUVs      = false;
			SM.BuildSettings.bBuildReversedIndexBuffer = false;

			// sqrt(triangle_ratio) approximates pixel-coverage ratio for screen size.
			const float ScreenFrac = FMath::Sqrt(FMath::Clamp(Ratios[i], 0.01f, 1.f));
			SM.ScreenSize.Default = ScreenFrac;

			FMeshDescription* DstMD = Source->CreateMeshDescription(LODIndex, MoveTemp(LODs[i]));
			check(DstMD);
			Source->CommitMeshDescription(LODIndex);
		}

		Source->PostEditChange();
		Source->MarkPackageDirty();

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("WriteLODsInPlace: %s now has %d LODs"),
			*Source->GetName(), TotalLODs);
		return true;
#else
		return false;
#endif
	}

	// ---------------------------------------------------------------------
	// T22: CreateLODsAsset (sibling <Source>_LODs asset)
	// ---------------------------------------------------------------------
	static UStaticMesh* CreateLODsAsset(
		UStaticMesh* Source,
		TArray<FMeshDescription>&& LODs,
		const TArray<float>& Ratios)
	{
#if WITH_EDITOR
		if (!Source || LODs.Num() == 0 || LODs.Num() != Ratios.Num())
		{
			return nullptr;
		}

		const FString SourcePackageName = Source->GetOutermost()->GetName();
		const FString ParentPath        = FPackageName::GetLongPackagePath(SourcePackageName);
		const FString NewAssetName      = Source->GetName() + TEXT("_LODs");
		const FString NewPackageName    = ParentPath / NewAssetName;

		UPackage* NewPackage = CreatePackage(*NewPackageName);
		NewPackage->FullyLoad();

		UStaticMesh* NewMesh = FindObject<UStaticMesh>(NewPackage, *NewAssetName);
		const bool bIsNew = (NewMesh == nullptr);
		if (bIsNew)
		{
			NewMesh = NewObject<UStaticMesh>(NewPackage, *NewAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		// Inherit source's material slots (preserve slot order so polygon group
		// 0 still maps to the same material index).
		NewMesh->GetStaticMaterials().Reset();
		for (const FStaticMaterial& M : Source->GetStaticMaterials())
		{
			NewMesh->GetStaticMaterials().Add(M);
		}
		if (NewMesh->GetStaticMaterials().Num() == 0)
		{
			NewMesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Default"), TEXT("Default")));
		}

		const int32 TotalLODs = 1 + LODs.Num();
		NewMesh->SetNumSourceModels(TotalLODs);

		auto ConfigureSourceModel = [&](int32 Index, float ScreenFrac)
		{
			FStaticMeshSourceModel& SM = NewMesh->GetSourceModel(Index);
			SM.BuildSettings.bRecomputeNormals         = false;
			SM.BuildSettings.bRecomputeTangents        = false;
			SM.BuildSettings.bUseMikkTSpace            = false;
			SM.BuildSettings.bGenerateLightmapUVs      = false;
			SM.BuildSettings.bBuildReversedIndexBuffer = false;
			SM.ScreenSize.Default = ScreenFrac;
		};

		// LOD0: deep-copy of source LOD0 MeshDescription.
		if (const FMeshDescription* SrcMD = Source->GetMeshDescription(0))
		{
			FMeshDescription CopyMD = *SrcMD;
			ConfigureSourceModel(0, 1.f);
			FMeshDescription* DstMD = NewMesh->CreateMeshDescription(0, MoveTemp(CopyMD));
			check(DstMD);
			NewMesh->CommitMeshDescription(0);
		}

		// LOD1..N: generated.
		for (int32 i = 0; i < LODs.Num(); ++i)
		{
			const int32 LODIndex = i + 1;
			const float ScreenFrac = FMath::Sqrt(FMath::Clamp(Ratios[i], 0.01f, 1.f));
			ConfigureSourceModel(LODIndex, ScreenFrac);
			FMeshDescription* DstMD = NewMesh->CreateMeshDescription(LODIndex, MoveTemp(LODs[i]));
			check(DstMD);
			NewMesh->CommitMeshDescription(LODIndex);
		}

		NewMesh->PostEditChange();
		NewMesh->Build(/*bSilent*/ false);
		NewMesh->MarkPackageDirty();

		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(NewMesh);
		}

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("CreateLODsAsset: %s -> %s (%s, %d LODs)"),
			*Source->GetName(), *NewMesh->GetName(),
			bIsNew ? TEXT("new") : TEXT("overwritten"),
			TotalLODs);
		return NewMesh;
#else
		return nullptr;
#endif
	}
}

// =======================================================================
// Entry point
// =======================================================================
bool ULODGenerator::GenerateLODs(UStaticMesh* Source, const FLODGenerationParams& Params)
{
#if WITH_EDITOR
	if (!Source)
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("GenerateLODs: null source"));
		return false;
	}
	if (Params.TargetRatios.Num() == 0)
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("GenerateLODs: empty TargetRatios"));
		return false;
	}

	const FMeshDescription* SourceMD = Source->GetMeshDescription(0);
	if (!SourceMD)
	{
		UE_LOG(LogUEAssetOptimizer, Warning,
			TEXT("GenerateLODs: no MeshDescription on %s"), *Source->GetName());
		return false;
	}

	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("GenerateLODs: %s mode=%s ratios=%d"),
		*Source->GetName(),
		Params.OutputMode == ELODOutputMode::InPlace ? TEXT("InPlace") : TEXT("NewAsset"),
		Params.TargetRatios.Num());

	// Step 1: extract flat wedge-deduped mesh from source LOD0.
	UEAOpt::FFlatMesh FlatSource;
	if (!UEAOpt::BuildFlatMeshFromDescription(*SourceMD, FlatSource))
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("GenerateLODs: failed to build flat mesh"));
		return false;
	}
	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("FlatMesh: unique V=%d indices=%d (source)"),
		FlatSource.Positions.Num(), FlatSource.Indices.Num());

	// Step 2: simplify + optimize per ratio.
	TArray<FMeshDescription> LODDescriptions;
	LODDescriptions.Reserve(Params.TargetRatios.Num());

	for (int32 i = 0; i < Params.TargetRatios.Num(); ++i)
	{
		const float Ratio = FMath::Clamp(Params.TargetRatios[i], 0.001f, 1.0f);
		const bool  bUseSloppy = Params.bSloppyLastLOD && (i == Params.TargetRatios.Num() - 1);

		TArray<uint32> ReducedIndices;
		const UEAOpt::FSimplifyResult Res = UEAOpt::SimplifyFlatMesh(
			FlatSource, Ratio, Params, bUseSloppy, ReducedIndices);

		UE_LOG(LogUEAssetOptimizer, Log,
			TEXT("LOD%d %s: ratio=%.3f target=%d actual=%d error=%.4f elapsed=%.3fs"),
			i + 1, bUseSloppy ? TEXT("sloppy") : TEXT("attr"),
			Ratio, Res.TargetIndexCount, Res.OutputIndexCount,
			Res.Error, Res.ElapsedSec);

		if (Res.OutputIndexCount < 3)
		{
			UE_LOG(LogUEAssetOptimizer, Warning,
				TEXT("LOD%d produced fewer than 3 indices; skipping"), i + 1);
			continue;
		}

		// Build a reduced flat mesh by reusing source vertex streams + new indices.
		UEAOpt::FFlatMesh ReducedFlat;
		ReducedFlat.Positions = FlatSource.Positions;
		ReducedFlat.Normals   = FlatSource.Normals;
		ReducedFlat.UVs       = FlatSource.UVs;
		ReducedFlat.Indices   = MoveTemp(ReducedIndices);

		// Step 3: vertex cache + fetch optimization (also trims unused vertices).
		if (Params.bOptimizeVertexOrder)
		{
			UEAOpt::OptimizeVertexOrder(ReducedFlat);
		}

		// Step 4: rebuild MeshDescription for this LOD.
		FMeshDescription MD;
		UEAOpt::BuildDescriptionFromFlatMesh(ReducedFlat, MD);
		LODDescriptions.Add(MoveTemp(MD));
	}

	if (LODDescriptions.Num() == 0)
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("GenerateLODs: produced 0 LODs"));
		return false;
	}

	// Step 5: dispatch to selected output mode.
	// TargetRatios entries must match LODDescriptions count; we only skipped
	// entries that produced empty output, which is rare — preserve full array
	// for consistent screen-size assignment.
	TArray<float> UsedRatios = Params.TargetRatios;
	UsedRatios.SetNum(LODDescriptions.Num());

	if (Params.OutputMode == ELODOutputMode::InPlace)
	{
		return UEAOpt::WriteLODsInPlace(Source, MoveTemp(LODDescriptions), UsedRatios);
	}
	else
	{
		UStaticMesh* NewMesh = UEAOpt::CreateLODsAsset(
			Source, MoveTemp(LODDescriptions), UsedRatios);
		return NewMesh != nullptr;
	}
#else
	return false;
#endif
}
