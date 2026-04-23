// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LODGenerator.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct UEASSETOPTIMIZEREDITOR_API FLODGenerationParams
{
	GENERATED_BODY()

	/** Target triangle ratios per LOD. LOD0 is implicitly 1.0 (source). */
	UPROPERTY(EditAnywhere, Category = "LOD")
	TArray<float> TargetRatios = { 0.5f, 0.25f, 0.1f };

	/** Max allowed geometric error as a fraction of the mesh's bounding sphere radius. */
	UPROPERTY(EditAnywhere, Category = "LOD", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetError = 0.01f;

	/** Keep border / UV seam edges locked during simplification. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bLockBorders = true;

	/** Apply sloppy simplification to the final LOD (dramatically lower quality). */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bSloppyLastLOD = false;

	/** Optimize the vertex cache and fetch order after simplification. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	bool bOptimizeVertexOrder = true;
};

/**
 * Generates LOD meshes on a UStaticMesh using meshoptimizer.
 *
 * Called from the Asset Action (right-click) menu. This stub is wired in
 * Sprint 3; for now it logs and returns false.
 */
UCLASS()
class UEASSETOPTIMIZEREDITOR_API ULODGenerator : public UObject
{
	GENERATED_BODY()

public:
	/** Entry point. Fills the mesh's LOD slots in place. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "UEAssetOptimizer|LOD")
	static bool GenerateLODs(UStaticMesh* Source, const FLODGenerationParams& Params);
};
