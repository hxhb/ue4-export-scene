#include "ExportSceneEdModeTookit.h"
#include "ExportSceneEdMode.h"

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

void FExportSceneEdModeToolkit::ShowExportPanel()
{
	UE_LOG(LogTemp, Log, TEXT("Show Export Panel"));
}

void FExportSceneEdModeToolkit::ShowImportPanel()
{
	UE_LOG(LogTemp, Log, TEXT("Show Import Panel"));
}
