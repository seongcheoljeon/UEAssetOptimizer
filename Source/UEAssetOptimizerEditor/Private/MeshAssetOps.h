// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
//
// Helpers shared by ULODGenerator and UAlphaWrapper for writing FMeshDescription
// data into UStaticMesh assets. Centralizes the "configure source model + commit
// MeshDescription + create-or-overwrite asset + finalize" pattern that both
// pipelines repeat.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FStaticMeshSourceModel;
struct FMeshDescription;

namespace UEAOpt
{
	/**
	 * Apply our standard "explicit-attributes" build settings: don't recompute
	 * normals/tangents (we ship them ourselves), no MikkTSpace, no lightmap UV
	 * generation, no reversed index buffer. ScreenSize.Default = ScreenFraction.
	 */
	void ConfigureSourceModelDefaults(FStaticMeshSourceModel& SM, float ScreenFraction);

	/**
	 * Configure source model + commit a MeshDescription into the given LOD slot.
	 *
	 * NOTE: Caller is responsible for `SetNumSourceModels(N)` ahead of time so
	 * that LODIndex is valid. This helper does NOT resize the source-model array.
	 */
	void CommitLOD(UStaticMesh* Mesh, int32 LODIndex, FMeshDescription&& MD, float ScreenFraction);

	/**
	 * Find or create a sibling UStaticMesh asset named `<Source>_Suffix` in the
	 * source's package folder. `bOutIsNew` is set to true when a fresh asset
	 * was created, false when an existing same-named asset was reused.
	 *
	 * Caller is responsible for: material slot setup, source model count,
	 * MeshDescription commits, and final FinalizeAsset.
	 */
	UStaticMesh* CreateOrOverwriteSiblingAsset(
		const UStaticMesh* Source,
		const FString& Suffix,
		bool& bOutIsNew);

	/**
	 * Finalize after all MeshDescription commits: PostEditChange + Build +
	 * MarkPackageDirty + (if bIsNew) AssetRegistry::AssetCreated.
	 */
	void FinalizeAsset(UStaticMesh* Mesh, bool bIsNew, bool bSilentBuild = false);
}
