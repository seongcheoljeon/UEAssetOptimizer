// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUEAssetOptimizer, Log, All);

class FUEAssetOptimizerEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
