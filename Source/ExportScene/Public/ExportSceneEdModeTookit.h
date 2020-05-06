#pragma once

#include "Toolkits/BaseToolkit.h"

class FExportSceneEdMode;

class FExportSceneEdModeToolkit : public FModeToolkit
{
public:

	FExportSceneEdModeToolkit(int32 iMode);

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

	void ShowExportPanel();
	void ShowImportPanel();
	FExportSceneEdMode* OwnerEdMode;
private:

	TSharedPtr<SWidget> ToolkitWidget;
};