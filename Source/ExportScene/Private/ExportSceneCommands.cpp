// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExportSceneCommands.h"

#define LOCTEXT_NAMESPACE "FExportSceneModule"

void FExportSceneCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "ExportScene", "Execute ExportScene action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
