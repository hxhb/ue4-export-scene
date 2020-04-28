// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ExportSceneStyle.h"

class FExportSceneCommands : public TCommands<FExportSceneCommands>
{
public:

	FExportSceneCommands()
		: TCommands<FExportSceneCommands>(TEXT("ExportScene"), NSLOCTEXT("Contexts", "ExportScene", "ExportScene Plugin"), NAME_None, FExportSceneStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
