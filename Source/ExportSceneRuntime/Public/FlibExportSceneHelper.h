// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/UObjectIterator.h"
#include "UnrealExporter.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlibExportSceneHelper.generated.h"


/*-----------------------------------------------------------------------------
   Actor adding/deleting functions.
-----------------------------------------------------------------------------*/

class FSceneActorExportObjectInnerContext : public FExportObjectInnerContext
{
public:
	FSceneActorExportObjectInnerContext(const TArray<FName>& InActorTags)
		//call the empty version of the base class
		: FExportObjectInnerContext(false)
	{
		// For each object . . .
		for (UObject* InnerObj : TObjectRange<UObject>(RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** IternalExcludeFlags */ EInternalObjectFlags::PendingKill))
		{
			UObject* OuterObj = InnerObj->GetOuter();

			//assume this is not part of a selected actor
			bool bIsChildOfSelectedActor = false;

			UObject* TestParent = OuterObj;
			while (TestParent)
			{
				AActor* TestParentAsActor = Cast<AActor>(TestParent);

				if (!TestParentAsActor)
					break;
				bIsChildOfSelectedActor = true;
				for (const auto& Tag : InActorTags)
				{
					if (!TestParentAsActor->ActorHasTag(Tag))
					{
						bIsChildOfSelectedActor = false;
						break;
					}
				}

				TestParent = TestParent->GetOuter();
			}

			if (bIsChildOfSelectedActor)
			{
				InnerList* Inners = ObjectToInnerMap.Find(OuterObj);
				if (Inners)
				{
					// Add object to existing inner list.
					Inners->Add(InnerObj);
				}
				else
				{
					// Create a new inner list for the outer object.
					InnerList& InnersForOuterObject = ObjectToInnerMap.Add(OuterObj, InnerList());
					InnersForOuterObject.Add(InnerObj);
				}
			}
		}
	}

};
/**
 * 
 */
UCLASS()
class EXPORTSCENERUNTIME_API UFlibExportSceneHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static UPackage* GetPackageByLongPackageName(const FString& LongPackageName);

	static FString ExportSceneActors(UWorld* InWorld,const TArray<FName>& InTags);

	static bool ImportEditorScene(UWorld* World, const FString& InExportWords);

	UFUNCTION(BlueprintCallable/*,meta=(CallableWithoutWorldContext,WorldContext="WorldContextObject")*/)
		static bool LoadScene(UObject* WorldContextObject,const FString& InSceneInfoPath);
};
