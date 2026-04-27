// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
//
// Generic "batch operation" pattern shared by the LOD generator and Alpha Wrap
// commands: filter selected assets to usable static meshes, show a modal
// param dialog, run a per-mesh operation under FScopedSlowTask, then post a
// summary toast. Lets each Asset Action handler shrink to a single
// RunBatchOperation<TParams>(...) call.
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "ModalParamDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/StaticMesh.h"

namespace UEAOpt
{
	/** Filter to UStaticMesh entries that have a usable LOD0 MeshDescription. */
	TArray<UStaticMesh*> FilterStaticMeshes(const TArray<FAssetData>& Selected);

	/** Post a CS_Fail toast (used by RunBatchOperation when no usable mesh remains). */
	void NotifyError(const FText& Message);

	/**
	 * Post a summary toast for a batch operation. Picks completion state from
	 * (OK, Fail, Canceled) counts; on any failure attaches a hyperlink that
	 * jumps to the Output Log.
	 */
	void NotifySummary(const FText& OpName, int32 OK, int32 Fail, int32 Canceled);

	/**
	 * Run a batch operation:
	 *   1. Filter SelectedAssets -> usable static meshes (NotifyError + bail if none).
	 *   2. Show a modal param dialog for TParams (Cancel = bail).
	 *   3. Loop with FScopedSlowTask (cancelable). Each iteration calls
	 *      OperationFn(Mesh, Params); bool return = success.
	 *   4. NotifySummary at the end.
	 *
	 * OperationFn signature: bool(UStaticMesh*, const TParams&).
	 */
	template <typename TParams, typename FOperation>
	void RunBatchOperation(
		const TArray<FAssetData>& SelectedAssets,
		const FText& OpName,
		const FText& DialogTitleFmt,   // {0} = mesh count
		const FText& ProgressLabel,
		FOperation OperationFn)
	{
		TArray<UStaticMesh*> Meshes = FilterStaticMeshes(SelectedAssets);
		if (Meshes.Num() == 0)
		{
			NotifyError(FText::Format(
				NSLOCTEXT("UEAssetOptimizer", "BatchNoMesh", "{0}: no usable mesh in selection"),
				OpName));
			return;
		}

		TParams Params;
		const FText Title = FText::Format(DialogTitleFmt, FText::AsNumber(Meshes.Num()));
		if (!ShowModalParamDialog(Title, Params))
		{
			return; // user canceled
		}

		int32 OK = 0, Fail = 0, Canceled = 0;
		{
			FScopedSlowTask Slow(static_cast<float>(Meshes.Num()), ProgressLabel);
			Slow.MakeDialog(/*bShowCancelButton*/ true);

			for (int32 i = 0; i < Meshes.Num(); ++i)
			{
				if (Slow.ShouldCancel())
				{
					Canceled = Meshes.Num() - i;
					break;
				}
				Slow.EnterProgressFrame(1.f, FText::Format(
					NSLOCTEXT("UEAssetOptimizer", "BatchFrame", "{0} / {1}: {2}"),
					FText::AsNumber(i + 1), FText::AsNumber(Meshes.Num()),
					FText::FromName(Meshes[i]->GetFName())));

				if (OperationFn(Meshes[i], Params))
				{
					++OK;
				}
				else
				{
					++Fail;
				}
			}
		}

		NotifySummary(OpName, OK, Fail, Canceled);
	}
}
