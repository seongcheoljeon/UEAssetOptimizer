// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "BatchOperation.h"
#include "UEAssetOptimizerEditor.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "UEAssetOptimizer.BatchOperation"

namespace UEAOpt
{
	TArray<UStaticMesh*> FilterStaticMeshes(const TArray<FAssetData>& Selected)
	{
		TArray<UStaticMesh*> Out;
		Out.Reserve(Selected.Num());
		for (const FAssetData& A : Selected)
		{
			UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset());
			if (!M)
			{
				continue;
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

	void NotifyError(const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.bUseSuccessFailIcons = true;
		Info.bUseLargeFont        = false;
		Info.FadeInDuration       = 0.3f;
		Info.FadeOutDuration      = 1.0f;
		Info.ExpireDuration       = 5.f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}

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
			Info.Hyperlink = FSimpleDelegate::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));
			});
			Info.HyperlinkText = LOCTEXT("OpenOutputLog", "Open Output Log");
		}
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}
}

#undef LOCTEXT_NAMESPACE
