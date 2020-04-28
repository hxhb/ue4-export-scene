// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlibExportSceneEditorHelper.generated.h"

/**
 * 
 */
UCLASS()
class EXPORTSCENE_API UFlibExportSceneEditorHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static UWorld* GetEditorWorld();
	static bool ExportEditorScene();
};
