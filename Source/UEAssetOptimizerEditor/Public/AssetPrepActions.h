// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"

struct FAssetData;

/**
 * Registers right-click context menu entries for UStaticMesh assets:
 *   - "Generate LODs..."
 *   - "Alpha Wrap..."
 *
 * Lifecycle owned by the editor module's StartupModule / ShutdownModule.
 */
class FAssetPrepActions
{
public:
	static void Register();
	static void Unregister();

	// Invoked by menu callbacks registered in Register(). Public because
	// the menu extender lambdas live in an anonymous namespace in the .cpp
	// and therefore cannot reach private statics.
	static void OnGenerateLODsClicked(TArray<FAssetData> SelectedAssets);
	static void OnAlphaWrapClicked(TArray<FAssetData> SelectedAssets);

private:
	static FDelegateHandle ContentBrowserExtenderHandle;
};
