// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "UEAssetOptimizerEditor.h"
#include "AssetPrepActions.h"

DEFINE_LOG_CATEGORY(LogUEAssetOptimizer);

#define LOCTEXT_NAMESPACE "FUEAssetOptimizerEditorModule"

void FUEAssetOptimizerEditorModule::StartupModule()
{
	UE_LOG(LogUEAssetOptimizer, Log, TEXT("UEAssetOptimizerEditor: startup"));
	FAssetPrepActions::Register();
}

void FUEAssetOptimizerEditorModule::ShutdownModule()
{
	FAssetPrepActions::Unregister();
	UE_LOG(LogUEAssetOptimizer, Log, TEXT("UEAssetOptimizerEditor: shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEAssetOptimizerEditorModule, UEAssetOptimizerEditor)
