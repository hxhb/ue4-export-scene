// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneHelper.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Exporters/Exporter.h"
#include "AssetData.h"
#include "PropertyPortFlags.h"
#include "AssetRegistryModule.h"

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

FString UFlibExportSceneHelper::ExportSceneActors(UWorld* InWorld)
{
	// Export the actors.
	FStringOutputDevice Ar;
	const FSceneActorExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice(&Context, InWorld, NULL, Ar, TEXT("copy"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified);
	FPlatformApplicationMisc::ClipboardCopy(*Ar);

	return MoveTemp(Ar);
}

