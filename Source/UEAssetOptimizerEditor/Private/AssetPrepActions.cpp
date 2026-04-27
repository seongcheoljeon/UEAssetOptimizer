// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "AssetPrepActions.h"
#include "UEAssetOptimizerEditor.h"
#include "LODGenerator.h"
#include "AlphaWrapper.h"
#include "ModalParamDialog.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "Engine/StaticMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/ScopedSlowTask.h"
#include "MeshDescription.h"

#define LOCTEXT_NAMESPACE "UEAssetOptimizer.AssetPrepActions"

FDelegateHandle FAssetPrepActions::ContentBrowserExtenderHandle;

namespace
{
	// ----------------- Helpers -----------------

	/**
	 * Validate selected assets, dropping anything that isn't a usable
	 * UStaticMesh with LOD0 MeshDescription. Logs reasons for excluded entries.
	 * Returns the kept meshes in selection order.
	 */
	TArray<UStaticMesh*> FilterStaticMeshes(const TArray<FAssetData>& Selected)
	{
		TArray<UStaticMesh*> Out;
		Out.Reserve(Selected.Num());
		for (const FAssetData& A : Selected)
		{
			UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset());
			if (!M)
			{
				continue; // not a static mesh (filter happens earlier too, but be safe)
			}
			if (!M->GetMeshDescription(0))
			{
				UE_LOG(LogUEAssetOptimizer, Warning,
					TEXT("Skipping %s: no MeshDescription on LOD0"),
					*M->GetName());
				continue;
			}
			Out.Add(M);
		}
		return Out;
	}

	/** Post a single in-editor toast notification with the given completion state. */
	void Notify(const FText& Message, SNotificationItem::ECompletionState State, float ExpireSec = 5.f)
	{
		FNotificationInfo Info(Message);
		Info.bUseSuccessFailIcons = true;
		Info.bUseLargeFont        = false;
		Info.FadeInDuration       = 0.3f;
		Info.FadeOutDuration      = 1.0f;
		Info.ExpireDuration       = ExpireSec;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	/**
	 * Post a summary toast for a batch operation. Picks completion state based
	 * on success/failure/cancel counts. Hyperlink jumps to Output Log so the
	 * user can read per-mesh details when something failed.
	 */
	void NotifySummary(const FText& OpName, int32 OK, int32 Fail, int32 Canceled)
	{
		const int32 Total = OK + Fail + Canceled;
		FText Message;
		SNotificationItem::ECompletionState State = SNotificationItem::CS_Success;

		if (Canceled > 0 && OK == 0 && Fail == 0)
		{
			Message = FText::Format(LOCTEXT("CanceledNoop", "{0}: canceled"), OpName);
			State   = SNotificationItem::CS_None;
		}
		else if (Canceled > 0)
		{
			Message = FText::Format(
				LOCTEXT("CanceledPartial", "{0}: canceled after {1} / {2} ({3} ok, {4} failed)"),
				OpName, FText::AsNumber(OK + Fail), FText::AsNumber(Total),
				FText::AsNumber(OK), FText::AsNumber(Fail));
			State = (Fail > 0) ? SNotificationItem::CS_Fail : SNotificationItem::CS_None;
		}
		else if (Fail == 0)
		{
			Message = FText::Format(LOCTEXT("AllOK", "{0}: {1} mesh(es) processed"),
				OpName, FText::AsNumber(OK));
			State = SNotificationItem::CS_Success;
		}
		else if (OK == 0)
		{
			Message = FText::Format(LOCTEXT("AllFail", "{0}: all {1} failed (see Output Log)"),
				OpName, FText::AsNumber(Fail));
			State = SNotificationItem::CS_Fail;
		}
		else
		{
			Message = FText::Format(LOCTEXT("Mixed", "{0}: {1} ok, {2} failed (see Output Log)"),
				OpName, FText::AsNumber(OK), FText::AsNumber(Fail));
			State = SNotificationItem::CS_Fail;
		}

		FNotificationInfo Info(Message);
		Info.bUseSuccessFailIcons = true;
		Info.bUseLargeFont        = false;
		Info.FadeInDuration       = 0.3f;
		Info.FadeOutDuration      = 1.0f;
		Info.ExpireDuration       = 6.f;
		if (Fail > 0)
		{
			// Hyperlink to "open output log" so the user can read details.
			Info.Hyperlink     = FSimpleDelegate::CreateLambda([]() {
				FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));
			});
			Info.HyperlinkText = LOCTEXT("OpenOutputLog", "Open Output Log");
		}
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	// ----------------- Menu extender -----------------

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
	const TArray<UStaticMesh*> Meshes = FilterStaticMeshes(SelectedAssets);
	if (Meshes.Num() == 0)
	{
		Notify(LOCTEXT("NoMeshLOD", "Generate LODs: no usable mesh in selection"),
		       SNotificationItem::CS_Fail);
		return;
	}

	FLODGenerationParams Params;
	const FText Title = FText::Format(
		LOCTEXT("LODDialogTitle", "Generate LODs ({0} mesh{0}|plural(one=,other=es) selected)"),
		FText::AsNumber(Meshes.Num()));
	if (!UEAOpt::ShowModalParamDialog(Title, Params))
	{
		return; // user canceled
	}

	int32 OK = 0, Fail = 0, Canceled = 0;
	{
		FScopedSlowTask Slow(static_cast<float>(Meshes.Num()),
			LOCTEXT("LODProgressLabel", "Generating LODs..."));
		Slow.MakeDialog(/*bShowCancelButton*/ true);

		for (int32 i = 0; i < Meshes.Num(); ++i)
		{
			if (Slow.ShouldCancel())
			{
				Canceled = Meshes.Num() - i;
				break;
			}
			Slow.EnterProgressFrame(1.f, FText::Format(
				LOCTEXT("LODFrameFmt", "{0} / {1}: {2}"),
				FText::AsNumber(i + 1), FText::AsNumber(Meshes.Num()),
				FText::FromName(Meshes[i]->GetFName())));

			if (ULODGenerator::GenerateLODs(Meshes[i], Params))
			{
				++OK;
			}
			else
			{
				++Fail;
			}
		}
	}

	NotifySummary(LOCTEXT("OpLOD", "Generate LODs"), OK, Fail, Canceled);
}

void FAssetPrepActions::OnAlphaWrapClicked(TArray<FAssetData> SelectedAssets)
{
	const TArray<UStaticMesh*> Meshes = FilterStaticMeshes(SelectedAssets);
	if (Meshes.Num() == 0)
	{
		Notify(LOCTEXT("NoMeshWrap", "Alpha Wrap: no usable mesh in selection"),
		       SNotificationItem::CS_Fail);
		return;
	}

	FAlphaWrapParams Params;
	const FText Title = FText::Format(
		LOCTEXT("WrapDialogTitle", "Alpha Wrap ({0} mesh{0}|plural(one=,other=es) selected)"),
		FText::AsNumber(Meshes.Num()));
	if (!UEAOpt::ShowModalParamDialog(Title, Params))
	{
		return;
	}

	int32 OK = 0, Fail = 0, Canceled = 0;
	{
		FScopedSlowTask Slow(static_cast<float>(Meshes.Num()),
			LOCTEXT("WrapProgressLabel", "Running Alpha Wrap..."));
		Slow.MakeDialog(/*bShowCancelButton*/ true);

		for (int32 i = 0; i < Meshes.Num(); ++i)
		{
			if (Slow.ShouldCancel())
			{
				Canceled = Meshes.Num() - i;
				break;
			}
			Slow.EnterProgressFrame(1.f, FText::Format(
				LOCTEXT("WrapFrameFmt", "{0} / {1}: {2}"),
				FText::AsNumber(i + 1), FText::AsNumber(Meshes.Num()),
				FText::FromName(Meshes[i]->GetFName())));

			if (UAlphaWrapper::CreateAlphaWrap(Meshes[i], Params) != nullptr)
			{
				++OK;
			}
			else
			{
				++Fail;
			}
		}
	}

	NotifySummary(LOCTEXT("OpWrap", "Alpha Wrap"), OK, Fail, Canceled);
}

#undef LOCTEXT_NAMESPACE
