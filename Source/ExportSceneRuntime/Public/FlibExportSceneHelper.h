// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlibExportSceneHelper.generated.h"

/**
 * 
 */
UCLASS()
class EXPORTSCENERUNTIME_API UFlibExportSceneHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static UPackage* GetPackageByLongPackageName(const FString& LongPackageName);

	static bool ParserLevel(UObject const* SourceObject, UObject* DestOuter, const FName DestName = NAME_None, EObjectFlags FlagMask = RF_AllFlags, UClass* DestClass = NULL, EDuplicateMode::Type DuplicateMode = EDuplicateMode::PIE, EInternalObjectFlags InternalFlagsMask = EInternalObjectFlags::AllFlags);

};
