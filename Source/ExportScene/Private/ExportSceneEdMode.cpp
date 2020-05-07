#include "ExportSceneEdMode.h"
#include "ExportSceneEdModeTookit.h"
#include "Toolkits/ToolkitManager.h"

const FEditorModeID FExportSceneEdMode::EM_ExportSceneEdModeId = TEXT("EM_ExportScene");

bool FExportSceneEdMode::bIsExportSceneSerialization = false;

FExportSceneEdMode::FExportSceneEdMode()
{

}

FExportSceneEdMode::~FExportSceneEdMode()
{

}

void FExportSceneEdMode::Enter()
{
	FEdMode::Enter();

	if (!Toolkit.IsValid() && UsesToolkits())
	{
		ShowTookit(0);
	}
}

void FExportSceneEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FExportSceneEdMode::UsesToolkits() const
{
	
	return true;
}

void FExportSceneEdMode::ShowTookit(int32 iMode)
{
	Toolkit = MakeShareable(new FExportSceneEdModeToolkit(iMode));
	Toolkit->Init(Owner->GetToolkitHost());
	static_cast<FExportSceneEdModeToolkit*>(Toolkit.Get())->OwnerEdMode = this;
}
