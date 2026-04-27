// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "AssetPrepActions.h"
#include "UEAssetOptimizerEditor.h"
#include "LODGenerator.h"
#include "AlphaWrapper.h"
#include "BatchOperation.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "Engine/StaticMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "UEAssetOptimizer.AssetPrepActions"

FDelegateHandle FAssetPrepActions::ContentBrowserExtenderHandle;

namespace
{
	TSharedRef<FExtender> MakeMeshExtender(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender);

		bool bAnyStaticMesh = false;
		for (const FAssetData& A : SelectedAssets)
		{
			if (A.GetClass() == UStaticMesh::StaticClass())
			{
				bAnyStaticMesh = true;
				break;
			}
		}
		if (!bAnyStaticMesh)
		{
			return Extender;
		}

		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateLambda([SelectedAssets](FMenuBuilder& Builder)
			{
				Builder.BeginSection("UEAssetOptimizer", LOCTEXT("SectionLabel", "UEAssetOptimizer"));

				Builder.AddMenuEntry(
					LOCTEXT("GenerateLODs", "Generate LODs..."),
					LOCTEXT("GenerateLODsTooltip", "Create multiple LOD levels via meshoptimizer."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
					{
						FAssetPrepActions::OnGenerateLODsClicked(SelectedAssets);
					}))
				);

				Builder.AddMenuEntry(
					LOCTEXT("AlphaWrap", "Alpha Wrap..."),
					LOCTEXT("AlphaWrapTooltip", "Create a watertight shrink-wrap mesh via CGAL."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
					{
						FAssetPrepActions::OnAlphaWrapClicked(SelectedAssets);
					}))
				);

				Builder.EndSection();
			}));

		return Extender;
	}
}

void FAssetPrepActions::Register()
{
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& Extenders = CB.GetAllAssetViewContextMenuExtenders();
	FContentBrowserMenuExtender_SelectedAssets Delegate =
		FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&MakeMeshExtender);
	Extenders.Add(Delegate);
	ContentBrowserExtenderHandle = Delegate.GetHandle();
}

void FAssetPrepActions::Unregister()
{
	if (FContentBrowserModule* CB = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		CB->GetAllAssetViewContextMenuExtenders().RemoveAll(
			[](const FContentBrowserMenuExtender_SelectedAssets& D)
			{
				return D.GetHandle() == ContentBrowserExtenderHandle;
			});
	}
}

void FAssetPrepActions::OnGenerateLODsClicked(TArray<FAssetData> SelectedAssets)
{
	UEAOpt::RunBatchOperation<FLODGenerationParams>(
		SelectedAssets,
		LOCTEXT("OpLOD", "Generate LODs"),
		LOCTEXT("LODDialogTitle", "Generate LODs ({0} mesh(es) selected)"),
		LOCTEXT("LODProgressLabel", "Generating LODs..."),
		[](UStaticMesh* M, const FLODGenerationParams& P)
		{
			return ULODGenerator::GenerateLODs(M, P);
		});
}

void FAssetPrepActions::OnAlphaWrapClicked(TArray<FAssetData> SelectedAssets)
{
	UEAOpt::RunBatchOperation<FAlphaWrapParams>(
		SelectedAssets,
		LOCTEXT("OpWrap", "Alpha Wrap"),
		LOCTEXT("WrapDialogTitle", "Alpha Wrap ({0} mesh(es) selected)"),
		LOCTEXT("WrapProgressLabel", "Running Alpha Wrap..."),
		[](UStaticMesh* M, const FAlphaWrapParams& P)
		{
			return UAlphaWrapper::CreateAlphaWrap(M, P) != nullptr;
		});
}

#undef LOCTEXT_NAMESPACE
