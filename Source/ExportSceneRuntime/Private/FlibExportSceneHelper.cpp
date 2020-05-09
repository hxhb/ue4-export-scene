// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneHelper.h"
#include "SceneExporter.h"
#include "ImportFactory.h"
#include "FileHelper.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Exporters/Exporter.h"
#include "AssetData.h"
#include "PropertyPortFlags.h"
#include "AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"

UPackage* UFlibExportSceneHelper::GetPackageByLongPackageName(const FString& LongPackageName)
{
	UPackage* ResultPackage = NULL;
	TArray<FAssetData> AssetDatas;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssetsByPackageName(*LongPackageName, AssetDatas, true);

	if (!!AssetDatas.Num())
	{
		ResultPackage = AssetDatas[0].GetPackage();
	}
	return ResultPackage;
}

FString UFlibExportSceneHelper::ExportSceneActors(UWorld* InWorld, const TArray<FName>& InTags)
{
	// Export the actors.
	FStringOutputDevice Ar;
	const FSceneActorExportObjectInnerContext Context(InTags);

	UExporter::ExportToOutputDevice(&Context, InWorld, NULL, Ar, TEXT("EXPORT"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified);
	FPlatformApplicationMisc::ClipboardCopy(*Ar);

	return MoveTemp(Ar);
}


bool UFlibExportSceneHelper::LoadScene(UObject* WorldContextObject, const FString& InSceneInfoPath)
{
	bool status = true;
	UWorld* World = UGameplayStatics::GetGameInstance(WorldContextObject)->GetWorld();
	if (!InSceneInfoPath.IsEmpty() && FPaths::FileExists(InSceneInfoPath))
	{
		FString ReadContent;
		FFileHelper::LoadFileToString(ReadContent, *InSceneInfoPath);
		if (!ReadContent.IsEmpty())
		{
			const TCHAR* Paste = *ReadContent;
			UImportFactory* ImportFactory = NewObject<UImportFactory>();
			ImportFactory->FactoryCreateText(ULevel::StaticClass(), World->GetCurrentLevel(), World->GetCurrentLevel()->GetFName(),
				RF_Transactional, NULL, TEXT("paste"), Paste, Paste + FCString::Strlen(Paste), NULL);
			
		}
	}
	return status;
}

