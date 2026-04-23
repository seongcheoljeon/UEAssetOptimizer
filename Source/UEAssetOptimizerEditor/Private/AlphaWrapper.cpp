// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "AlphaWrapper.h"
#include "UEAssetOptimizerEditor.h"
#include "Engine/StaticMesh.h"

// Intentionally NOT including CGALIncludes.h until Sprint 2 — keeping this
// translation unit free of CGAL symbols while the ThirdParty scaffold is empty.

UStaticMesh* UAlphaWrapper::CreateAlphaWrap(UStaticMesh* Source, const FAlphaWrapParams& Params)
{
	if (!Source)
	{
		UE_LOG(LogUEAssetOptimizer, Warning, TEXT("CreateAlphaWrap: null source mesh"));
		return nullptr;
	}

	UE_LOG(LogUEAssetOptimizer, Log,
		TEXT("CreateAlphaWrap stub: mesh=%s, purpose=%d"),
		*Source->GetName(), static_cast<int32>(Params.Purpose));

	// TODO(Sprint 2): CGAL integration
	// 1. Include "CGALIncludes.h"
	// 2. Extract triangles from Source LOD0 into a CGAL::Surface_mesh
	// 3. Compute bbox diagonal -> resolve absolute alpha/offset
	// 4. CGAL::alpha_wrap_3(mesh, alpha, offset, wrapped)
	// 5. Convert wrapped -> MeshDescription -> new UStaticMesh asset
	// 6. Save with suffix "_Wrap"
	return nullptr;
}
