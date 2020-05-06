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
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FExportSceneStyle::Initialize();
	FExportSceneStyle::ReloadTextures();

	FExportSceneCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FExportSceneCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FExportSceneModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FExportSceneModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FExportSceneModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}

	FEditorModeRegistry::Get().RegisterMode<FExportSceneEdMode>(FExportSceneEdMode::EM_ExportSceneEdModeId, LOCTEXT("ExportSceneEdModeName", "ExportSceneEdMode"), FSlateIcon(FEditorStyle::GetStyleSetName(), "DeviceDetails.Share"), true);

}

void FExportSceneModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FExportSceneStyle::Shutdown();

	FExportSceneCommands::Unregister();

	FEditorModeRegistry::Get().UnregisterMode(FExportSceneEdMode::EM_ExportSceneEdModeId);
}

void FExportSceneModule::PluginButtonClicked()
{
	UFlibExportSceneEditorHelper::ExportEditorScene();
}

void FExportSceneModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FExportSceneCommands::Get().PluginAction);
}

void FExportSceneModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FExportSceneCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FExportSceneModule, ExportScene)