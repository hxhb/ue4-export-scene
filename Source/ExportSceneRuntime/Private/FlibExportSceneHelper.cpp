// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibExportSceneHelper.h"
#include "Flib/FLibAssetManageHelperEx.h"
#include "Utils/FBlueprintSupportEx.hpp"

#include "PropertyPortFlags.h"
#include "Serialization/LargeMemoryData.h"
#include "UObject/UObjectAnnotation.h"
#include "Serialization/DuplicatedObject.h"
#include "Serialization/DuplicatedDataWriter.h"
#include "Blueprint/BlueprintSupport.h"
#include "AssetRegistryModule.h"
#include "UObject/Linker.h"



/*-----------------------------------------------------------------------------
   Duplicating Objects.
-----------------------------------------------------------------------------*/

struct FObjectDuplicationHelperMethods
{
	// Helper method intended to gather up all default subobjects that have already been created and prepare them for duplication.
	static void GatherDefaultSubobjectsForDuplication(UObject* SrcObject, UObject* DstObject, FUObjectAnnotationSparse<FDuplicatedObject, false>& DuplicatedObjectAnnotation, FDuplicateDataWriter& Writer)
	{
		TArray<UObject*> SrcDefaultSubobjects;
		SrcObject->GetDefaultSubobjects(SrcDefaultSubobjects);

		// Iterate over all default subobjects within the source object.
		for (UObject* SrcDefaultSubobject : SrcDefaultSubobjects)
		{
			if (SrcDefaultSubobject)
			{
				// Attempt to find a default subobject with the same name within the destination object.
				UObject* DupDefaultSubobject = DstObject->GetDefaultSubobjectByName(SrcDefaultSubobject->GetFName());
				if (DupDefaultSubobject)
				{
					// Map the duplicated default subobject to the source and register it for serialization.
					DuplicatedObjectAnnotation.AddAnnotation(SrcDefaultSubobject, FDuplicatedObject(DupDefaultSubobject));
					Writer.UnserializedObjects.Add(SrcDefaultSubobject);

					// Recursively gather any nested default subobjects that have already been constructed through CreateDefaultSubobject().
					GatherDefaultSubobjectsForDuplication(SrcDefaultSubobject, DupDefaultSubobject, DuplicatedObjectAnnotation, Writer);
				}
			}
		}
	}
};


UPackage* UFlibExportSceneHelper::GetPackageByLongPackageName(const FString& LongPackageName)
{
	UPackage* ResultPackage = NULL;
	TArray<FAssetData> AssetDatas;
	UFLibAssetManageHelperEx::GetSpecifyAssetData(LongPackageName, AssetDatas, true);

	if (!!AssetDatas.Num())
	{
		ResultPackage = AssetDatas[0].GetPackage();
	}
	return ResultPackage;
}


bool UFlibExportSceneHelper::ParserLevel(UObject const* SourceObject, UObject* DestOuter, const FName DestName /*= NAME_None*/, EObjectFlags FlagMask /*= RF_AllFlags*/, UClass* DestClass /*= NULL*/, EDuplicateMode::Type DuplicateMode /*= EDuplicateMode::PIE*/, EInternalObjectFlags InternalFlagsMask /*= EInternalObjectFlags::AllFlags*/)
{
	if (!IsAsyncLoading() && /*!IsLoading() &&*/ SourceObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		// Detach linker for the outer if it already exists, to avoid problems with PostLoad checking the Linker version
		ResetLoaders(DestOuter);
	}

	// @todo: handle const down the callstack.  for now, let higher level code use it and just cast it off
	FObjectDuplicationParameters Parameters(const_cast<UObject*>(SourceObject), DestOuter);
	{
		if (!DestName.IsNone())
		{
			Parameters.DestName = DestName;
		}
		else if (SourceObject->GetOuter() != DestOuter)
		{
			// try to keep the object name consistent if possible
			if (FindObjectFast<UObject>(DestOuter, SourceObject->GetFName()) == nullptr)
			{
				Parameters.DestName = SourceObject->GetFName();
			}
		}

		if (DestClass == NULL)
		{
			Parameters.DestClass = SourceObject->GetClass();
		}
		else
		{
			Parameters.DestClass = DestClass;
		}
		Parameters.FlagMask = FlagMask;
		Parameters.InternalFlagMask = InternalFlagsMask;
		Parameters.DuplicateMode = DuplicateMode;

		if (DuplicateMode == EDuplicateMode::PIE)
		{
			Parameters.PortFlags = PPF_DuplicateForPIE;
		}
	}

	// make sure the two classes are the same size, as this hopefully will mean they are serialization
// compatible. It's not a guarantee, but will help find errors
	checkf((Parameters.DestClass->GetPropertiesSize() >= Parameters.SourceObject->GetClass()->GetPropertiesSize()),
		TEXT("Source and destination class sizes differ.  Source: %s (%i)   Destination: %s (%i)"),
		*Parameters.SourceObject->GetClass()->GetName(), Parameters.SourceObject->GetClass()->GetPropertiesSize(),
		*Parameters.DestClass->GetName(), Parameters.DestClass->GetPropertiesSize());
	FObjectInstancingGraph InstanceGraph;

	if (!GIsDuplicatingClassForReinstancing)
	{
		// make sure we are not duplicating RF_RootSet as this flag is special
		// also make sure we are not duplicating the RF_ClassDefaultObject flag as this can only be set on the real CDO
		Parameters.FlagMask &= ~RF_ClassDefaultObject;
		Parameters.InternalFlagMask &= ~EInternalObjectFlags::RootSet;
	}

	// disable object and component instancing while we're duplicating objects, as we're going to instance components manually a little further below
	InstanceGraph.EnableSubobjectInstancing(false);

	// we set this flag so that the component instancing code doesn't think we're creating a new archetype, because when creating a new archetype,
	// the ObjectArchetype for instanced components is set to the ObjectArchetype of the source component, which in the case of duplication (or loading)
	// will be changing the archetype's ObjectArchetype to the wrong object (typically the CDO or something)
	InstanceGraph.SetLoadingObject(true);

	UObject* DupRootObject = Parameters.DuplicationSeed.FindRef(Parameters.SourceObject);
	if (DupRootObject == NULL)
	{
		DupRootObject = StaticConstructObject_Internal(Parameters.DestClass,
			Parameters.DestOuter,
			Parameters.DestName,
			Parameters.ApplyFlags | Parameters.SourceObject->GetMaskedFlags(Parameters.FlagMask),
			Parameters.ApplyInternalFlags | (Parameters.SourceObject->GetInternalFlags() & Parameters.InternalFlagMask),
			Parameters.SourceObject->GetArchetype()->GetClass() == Parameters.DestClass
			? Parameters.SourceObject->GetArchetype()
			: NULL,
			true,
			&InstanceGraph
		);
	}

	FLargeMemoryData ObjectData;

	FUObjectAnnotationSparse<FDuplicatedObject, false>  DuplicatedObjectAnnotation;

	// if seed objects were specified, add those to the DuplicatedObjects map now
	if (Parameters.DuplicationSeed.Num() > 0)
	{
		for (TMap<UObject*, UObject*>::TIterator It(Parameters.DuplicationSeed); It; ++It)
		{
			UObject* Src = It.Key();
			UObject* Dup = It.Value();
			checkSlow(Src);
			checkSlow(Dup);

			// create the DuplicateObjectInfo for this object
			DuplicatedObjectAnnotation.AddAnnotation(Src, FDuplicatedObject(Dup));
		}
	}

	// Read from the source object(s)
	FDuplicateDataWriter Writer(
		DuplicatedObjectAnnotation,	// Ref: Object annotation which stores the duplicated object for each source object
		ObjectData,					// Out: Serialized object data
		Parameters.SourceObject,	// Source object to copy
		DupRootObject,				// Destination object to copy into
		Parameters.FlagMask,		// Flags to be copied for duplicated objects
		Parameters.ApplyFlags,		// Flags to always set on duplicated objects
		Parameters.InternalFlagMask,		// Internal Flags to be copied for duplicated objects
		Parameters.ApplyInternalFlags,		// Internal Flags to always set on duplicated objects
		&InstanceGraph,				// Instancing graph
		Parameters.PortFlags);		// PortFlags

	TArray<UObject*> SerializedObjects;


	if (GIsDuplicatingClassForReinstancing)
	{
		FBlueprintSupportEx::DuplicateAllFields(dynamic_cast<UStruct*>(Parameters.SourceObject), Writer);
	}

	// Add default subobjects to the DuplicatedObjects map so they don't get recreated during serialization.
	FObjectDuplicationHelperMethods::GatherDefaultSubobjectsForDuplication(Parameters.SourceObject, DupRootObject, DuplicatedObjectAnnotation, Writer);

	InstanceGraph.SetDestinationRoot(DupRootObject);
	while (Writer.UnserializedObjects.Num())
	{
		UObject*	Object = Writer.UnserializedObjects.Pop();
		Object->Serialize(Writer);
		SerializedObjects.Add(Object);
	};


	return false;
}
