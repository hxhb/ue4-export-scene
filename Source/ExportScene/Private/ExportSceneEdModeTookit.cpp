#include "ExportSceneEdModeTookit.h"
#include "ExportSceneEdMode.h"
#include "FlibExportSceneEditorHelper.h"

FExportSceneEdModeToolkit::FExportSceneEdModeToolkit(int32 iMode)
{
	TMap<int32,TFunction<void(FExportSceneEdModeToolkit*)>> PanelMap;
	PanelMap.Add(0, &FExportSceneEdModeToolkit::ShowExportPanel);
	PanelMap.Add(1, &FExportSceneEdModeToolkit::ShowImportPanel);

	if (PanelMap.Contains(iMode))
	{
		(*PanelMap.Find(iMode))(this);
	}
}

FName FExportSceneEdModeToolkit::GetToolkitFName() const
{
	return FName(TEXT("ExportSceneEdMode"));
}

FText FExportSceneEdModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ExportSceneEdModeToolkit", "DisplayName", "ExportSceneEdMode Tool");
}

class FEdMode* FExportSceneEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FExportSceneEdMode::EM_ExportSceneEdModeId);
}


namespace
{
	static void DoExport()
	{
		UWorld* World = UFlibExportSceneEditorHelper::GetEditorWorld();
		UFlibExportSceneEditorHelper::ExportEditorScene(World, TArray<FName>{TEXT("Export")});
	}
	static void DoImport()
	{
		UWorld* World = UFlibExportSceneEditorHelper::GetEditorWorld();
		UFlibExportSceneEditorHelper::ImportEditorScene(World, TEXT(""));
	}
}
void FExportSceneEdModeToolkit::ShowExportPanel()
{
	SAssignNew(ToolkitWidget, SBorder)
	.VAlign(VAlign_Top)
	.Padding(FMargin(10.f, 0.f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(21.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Export"))
				.OnClicked_Lambda([&]()
				{
					DoExport();
					return FReply::Handled();
				})
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(21.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Import"))
				.OnClicked_Lambda([&]()
				{
					DoImport();
					return FReply::Handled();
				})
			]
		]
	];
}

void FExportSceneEdModeToolkit::ShowImportPanel()
{
	UE_LOG(LogTemp, Log, TEXT("Show Import Panel"));
}
