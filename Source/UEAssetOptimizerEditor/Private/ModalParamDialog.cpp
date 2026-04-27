// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#include "ModalParamDialog.h"

#include "Editor.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UEAssetOptimizer.ModalParamDialog"

namespace UEAOpt
{
	bool ShowModalParamDialog_Internal(const FText& Title, const UScriptStruct* StructDef, uint8* Data)
	{
		if (!GEditor || !StructDef || !Data)
		{
			return false;
		}

		// Wrap the caller's struct memory without copying. FStructOnScope
		// constructed from (UScriptStruct*, uint8*) does NOT take ownership
		// of the memory, so any edits made through the property editor land
		// directly in the caller's instance.
		TSharedRef<FStructOnScope> Scope = MakeShared<FStructOnScope>(StructDef, Data);

		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch       = false;
		DetailsViewArgs.bHideSelectionTip  = true;
		DetailsViewArgs.NameAreaSettings   = FDetailsViewArgs::HideNameArea;

		FStructureDetailsViewArgs StructureViewArgs;
		// Defaults are fine: shows nested object/struct fields as expected.

		TSharedRef<IStructureDetailsView> StructureDetailsView =
			PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, Scope);

		bool bAccepted = false;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(420.f, 360.f))
			.SupportsMinimize(false)
			.SupportsMaximize(false)
			.SizingRule(ESizingRule::UserSized);

		Window->SetContent(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			.Padding(8.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				  .FillHeight(1.f)
				  [
					  StructureDetailsView->GetWidget().ToSharedRef()
				  ]
				+ SVerticalBox::Slot()
				  .AutoHeight()
				  .Padding(0.f, 8.f, 0.f, 0.f)
				  .HAlign(HAlign_Right)
				  [
					  SNew(SHorizontalBox)
					  + SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(20.f, 4.f))
							.Text(LOCTEXT("OK", "OK"))
							.OnClicked_Lambda([&bAccepted, Window]()
							{
								bAccepted = true;
								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
						]
					  + SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(14.f, 4.f))
							.Text(LOCTEXT("Cancel", "Cancel"))
							.OnClicked_Lambda([Window]()
							{
								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
						]
				  ]
			]
		);

		GEditor->EditorAddModalWindow(Window);
		return bAccepted;
	}
}

#undef LOCTEXT_NAMESPACE
