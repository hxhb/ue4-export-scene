// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneEditorHelper.h"
#include "FlibExportSceneHelper.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Editor/EditorEngine.h"
#include "DesktopPlatformModule.h"
#include "IPluginManager.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FExportSceneModule"

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
	UWorld* World = UFlibExportSceneEditorHelper::GetEditorWorld();
	FString MapName = World->GetMapName();
	UClass* Class = World->GetClass();
	UObject* Outer = World->GetOuter();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	FString PluginPath = FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("ExportScene"))->GetBaseDir());
	
	FString OutPath;
	if (DesktopPlatform)
	{
		const bool bOpened = DesktopPlatform->OpenDirectoryDialog(
			nullptr,
			LOCTEXT("SaveSceneInfo", "Save Exported Scene Information").ToString(),
			PluginPath,
			OutPath
		);
		if (!OutPath.IsEmpty() && FPaths::DirectoryExists(OutPath))
		{
			FString CurrentTime = FDateTime::Now().ToString();
			FString ExportFilePath = FPaths::Combine(OutPath, MapName + TEXT("ExportScene") + CurrentTime + TEXT(".txt"));

			FString ReceiveSerializeData = UFlibExportSceneHelper::ExportSceneActors(World);

			bool bSaveStatus = false;
			if (!ReceiveSerializeData.IsEmpty())
			{
				bSaveStatus = FFileHelper::SaveStringToFile(ReceiveSerializeData, *ExportFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
		}
	}
	
	return false;
}


#undef LOCTEXT_NAMESPACE