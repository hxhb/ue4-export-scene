// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneEditorHelper.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Editor/EditorEngine.h"

UWorld* UFlibExportSceneEditorHelper::GetEditorWorld()
{
	UWorld* World = NULL;

	auto WorldList = GEngine->GetWorldContexts();
	for (int32 i = 0; i < WorldList.Num(); ++i)
	{
		UWorld* local_World = WorldList[i].World();

		if (UKismetSystemLibrary::IsValid(local_World) && local_World->WorldType == EWorldType::Editor)
		{
			World = local_World;
			break;
		}

	}

	return World;
}

bool UFlibExportSceneEditorHelper::ExportEditorScene()
{

	return false;
}
