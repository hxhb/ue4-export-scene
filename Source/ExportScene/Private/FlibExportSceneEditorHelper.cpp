// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneEditorHelper.h"
#include "FlibExportSceneHelper.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Editor/EditorEngine.h"
#include "DesktopPlatformModule.h"
#include "IPluginManager.h"
#include "Misc/FileHelper.h"
#include "ImportFactory.h"

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

bool UFlibExportSceneEditorHelper::ExportEditorScene(UWorld* World,const TArray<FName>& InTags)
{
	// UWorld* World = UFlibExportSceneEditorHelper::GetEditorWorld();
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

			FString ReceiveSerializeData = UFlibExportSceneHelper::ExportSceneActors(World, InTags);

			bool bSaveStatus = false;
			if (!ReceiveSerializeData.IsEmpty())
			{
				bSaveStatus = FFileHelper::SaveStringToFile(ReceiveSerializeData, *ExportFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
		}
	}
	
	return false;
}


bool UFlibExportSceneEditorHelper::ImportEditorScene(UWorld* World, const FString& InExportWords)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	FString PluginPath = FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("ExportScene"))->GetBaseDir());

	FString OutPath;
	if (DesktopPlatform)
	{
		TArray<FString> OpenFilenames;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			nullptr,
			LOCTEXT("Open Export Scene file", "Open .txt").ToString(),
			FString(TEXT("")),
			TEXT(""),
			TEXT("ExportScene file (*.txt)|*.txt"),
			EFileDialogFlags::None,
			OpenFilenames
		);

		if (OpenFilenames.Num() > 0)
		{
			OutPath = FPaths::ConvertRelativePathToFull(OpenFilenames[0]);
		}
	}
	if (!OutPath.IsEmpty() && FPaths::FileExists(OutPath))
	{
		FString ReadContent;
		FFileHelper::LoadFileToString(ReadContent, *OutPath);
		if (!ReadContent.IsEmpty())
		{
			const TCHAR* Paste = *ReadContent;
			UImportFactory* ImportFactory = NewObject<UImportFactory>();
			ImportFactory->FactoryCreateText(ULevel::StaticClass(), World->GetCurrentLevel(), World->GetCurrentLevel()->GetFName(),
				RF_Transactional, NULL, TEXT("paste"), Paste, Paste + FCString::Strlen(Paste), NULL);
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE