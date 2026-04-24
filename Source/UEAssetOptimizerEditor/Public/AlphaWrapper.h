// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AlphaWrapper.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EAlphaWrapPurpose : uint8
{
	Collision   UMETA(DisplayName = "Physics Collision"),
	BakingCage  UMETA(DisplayName = "Texture Baking Cage"),
	Cleanup     UMETA(DisplayName = "Watertight Cleanup"),
	Custom      UMETA(DisplayName = "Custom"),
};

USTRUCT(BlueprintType)
struct UEASSETOPTIMIZEREDITOR_API FAlphaWrapParams
{
	GENERATED_BODY()

	/** Wrap purpose; presets RelativeAlpha/RelativeOffset unless set to Custom. */
	UPROPERTY(EditAnywhere, Category = "AlphaWrap")
	EAlphaWrapPurpose Purpose = EAlphaWrapPurpose::Collision;

	/** Feature size = bbox diagonal / RelativeAlpha. Bigger = coarser. */
	UPROPERTY(EditAnywhere, Category = "AlphaWrap",
		meta = (ClampMin = "1.0", EditCondition = "Purpose == EAlphaWrapPurpose::Custom"))
	float RelativeAlpha = 20.0f;

	/** Shell thickness = bbox diagonal / RelativeOffset. Bigger = tighter to input. */
	UPROPERTY(EditAnywhere, Category = "AlphaWrap",
		meta = (ClampMin = "1.0", EditCondition = "Purpose == EAlphaWrapPurpose::Custom"))
	float RelativeOffset = 600.0f;

	/**
	 * If true, run CGAL isotropic remeshing on the wrap output.
	 * Smoothes alpha_wrap_3's sampling micro-facets but **increases triangle
	 * count** (target edge = alpha/2). Off by default because most use cases
	 * (collision, baking cage) prefer the lower-triangle raw wrap and the
	 * patchy shading under smooth lighting is cosmetic. Enable only if the
	 * wrap will be viewed/shaded in-scene.
	 */
	UPROPERTY(EditAnywhere, Category = "AlphaWrap")
	bool bEnableRemeshing = false;
};

/**
 * Produces a watertight shrink-wrapped UStaticMesh using CGAL::alpha_wrap_3.
 *
 * Stub in Sprint 1; full implementation in Sprint 2.
 */
UCLASS()
class UEASSETOPTIMIZEREDITOR_API UAlphaWrapper : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a new asset `<Source>_Wrap` next to the source. Returns the new mesh. */
	UFUNCTION(BlueprintCallable, Category = "UEAssetOptimizer|AlphaWrap")
	static UStaticMesh* CreateAlphaWrap(UStaticMesh* Source, const FAlphaWrapParams& Params);
};
