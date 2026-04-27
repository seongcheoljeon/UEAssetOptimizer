// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LODGenerator.generated.h"

class UStaticMesh;

/** Where to place the generated LOD levels. */
UENUM(BlueprintType)
enum class ELODOutputMode : uint8
{
	/** Append LOD1..N to the source UStaticMesh's own LOD slots (LOD0 unchanged). */
	InPlace    UMETA(DisplayName = "Modify Source Mesh"),
	/** Create a new <Source>_LODs asset containing LOD0..N. Source is untouched. */
	NewAsset   UMETA(DisplayName = "Create <Source>_LODs Asset"),
};

USTRUCT(BlueprintType)
struct UEASSETOPTIMIZEREDITOR_API FLODGenerationParams
{
	GENERATED_BODY()

	/** Where to write the generated LOD levels. */
	UPROPERTY(EditAnywhere, Category = "LOD|Output")
	ELODOutputMode OutputMode = ELODOutputMode::InPlace;

	/**
	 * Target triangle ratios per additional LOD. LOD0 is always the source
	 * mesh (implicitly 1.0). Entries become LOD1, LOD2, ... in order.
	 * Example: {0.5, 0.25, 0.1} -> 3 extra LODs at 50% / 25% / 10% triangle count.
	 */
	UPROPERTY(EditAnywhere, Category = "LOD")
	TArray<float> TargetRatios = { 0.5f, 0.25f, 0.1f };

	/**
	 * Maximum geometric error budget for meshopt_simplifyWithAttributes.
	 * Default 1.0 (relative to mesh AABB) is intentionally generous so the
	 * algorithm hits TargetRatios first instead of stalling on the error cap.
	 * In practice meshoptimizer still produces high-quality reductions because
	 * the per-edge collapse cost is the *lowest* error available; an open
	 * budget just lets it keep collapsing until the index target is met.
	 *
	 * Lower this only if you need a hard quality floor that overrides ratios
	 * (typical values: 0.05–0.1 for visible-LOD use; 0.01 only for "nearly
	 * indistinguishable" reductions). Toggle bAbsoluteError to treat the
	 * value as world-space distance instead.
	 */
	UPROPERTY(EditAnywhere, Category = "LOD", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float TargetError = 1.0f;

	/**
	 * Lock open-mesh border edges during simplification (meshopt_SimplifyLockBorder).
	 * **Off by default**: we feed meshoptimizer a wedge-deduped mesh, where every
	 * UV seam already manifests as a topological border. With this on, those
	 * fake borders prevent simplification from making meaningful progress.
	 * Attribute weights on normals/UVs already preserve seams via error penalty.
	 * Turn on only if you have a true open-shell mesh and want hard boundary
	 * preservation.
	 */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bLockBorders = false;

	/** Apply meshopt_simplifySloppy to the final LOD (dramatically lower quality, much faster). */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bSloppyLastLOD = false;

	/** After simplification, run meshopt_optimizeVertexCache + optimizeVertexFetch. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bOptimizeVertexOrder = true;

	/** Interpret TargetError as absolute world units instead of relative (meshopt_SimplifyErrorAbsolute). */
	UPROPERTY(EditAnywhere, Category = "LOD|Advanced")
	bool bAbsoluteError = false;
};

/**
 * Generates LOD meshes on a UStaticMesh using meshoptimizer.
 *
 * Invoked from the Content Browser right-click menu (see FAssetPrepActions).
 * Depending on FLODGenerationParams::OutputMode, the generated LODs are
 * either appended to the source mesh or written into a sibling <Source>_LODs
 * asset.
 */
UCLASS()
class UEASSETOPTIMIZEREDITOR_API ULODGenerator : public UObject
{
	GENERATED_BODY()

public:
	/** Entry point. Returns true on success (at least one LOD generated + written). */
	UFUNCTION(BlueprintCallable, Category = "UEAssetOptimizer|LOD")
	static bool GenerateLODs(UStaticMesh* Source, const FLODGenerationParams& Params);
};
