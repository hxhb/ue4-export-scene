// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExportScene.h"
#include "ExportSceneEdMode.h"
#include "FlibExportSceneEditorHelper.h"

#include "EditorStyleSet.h"
#include "ExportSceneStyle.h"
#include "ExportSceneCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"

static const FName ExportSceneTabName("ExportScene");

#define LOCTEXT_NAMESPACE "FExportSceneModule"

void FExportSceneModule::StartupModule()
{

	FEditorModeRegistry::Get().RegisterMode<FExportSceneEdMode>(FExportSceneEdMode::EM_ExportSceneEdModeId, LOCTEXT("ExportSceneEdModeName", "ExportSceneEdMode"), FSlateIcon(FEditorStyle::GetStyleSetName(), "DeviceDetails.Share"), true);

}

void FExportSceneModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FExportSceneEdMode::EM_ExportSceneEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FExportSceneModule, ExportScene)