// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "MeshAssetOps.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

namespace UEAOpt
{
	void ConfigureSourceModelDefaults(FStaticMeshSourceModel& SM, float ScreenFraction)
	{
		SM.BuildSettings.bRecomputeNormals         = false;
		SM.BuildSettings.bRecomputeTangents        = false;
		SM.BuildSettings.bUseMikkTSpace            = false;
		SM.BuildSettings.bGenerateLightmapUVs      = false;
		SM.BuildSettings.bBuildReversedIndexBuffer = false;
		SM.ScreenSize.Default                      = ScreenFraction;
	}

	void CommitLOD(UStaticMesh* Mesh, int32 LODIndex, FMeshDescription&& MD, float ScreenFraction)
	{
#if WITH_EDITOR
		if (!Mesh)
		{
			return;
		}
		FStaticMeshSourceModel& SM = Mesh->GetSourceModel(LODIndex);
		ConfigureSourceModelDefaults(SM, ScreenFraction);

		FMeshDescription* DstMD = Mesh->CreateMeshDescription(LODIndex, MoveTemp(MD));
		check(DstMD);
		Mesh->CommitMeshDescription(LODIndex);
#endif
	}

	UStaticMesh* CreateOrOverwriteSiblingAsset(
		const UStaticMesh* Source,
		const FString& Suffix,
		bool& bOutIsNew)
	{
#if WITH_EDITOR
		bOutIsNew = false;
		if (!Source)
		{
			return nullptr;
		}

		const FString SourcePackageName = Source->GetOutermost()->GetName();
		const FString ParentPath        = FPackageName::GetLongPackagePath(SourcePackageName);
		const FString NewAssetName      = Source->GetName() + Suffix;
		const FString NewPackageName    = ParentPath / NewAssetName;

		UPackage* NewPackage = CreatePackage(*NewPackageName);
		NewPackage->FullyLoad();

		UStaticMesh* NewMesh = FindObject<UStaticMesh>(NewPackage, *NewAssetName);
		bOutIsNew = (NewMesh == nullptr);
		if (bOutIsNew)
		{
			NewMesh = NewObject<UStaticMesh>(NewPackage, *NewAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}
		return NewMesh;
#else
		bOutIsNew = false;
		return nullptr;
#endif
	}

	void FinalizeAsset(UStaticMesh* Mesh, bool bIsNew, bool bSilentBuild)
	{
#if WITH_EDITOR
		if (!Mesh)
		{
			return;
		}
		Mesh->PostEditChange();
		Mesh->Build(bSilentBuild);
		Mesh->MarkPackageDirty();
		if (bIsNew)
		{
			FAssetRegistryModule::AssetCreated(Mesh);
		}
#endif
	}
}
