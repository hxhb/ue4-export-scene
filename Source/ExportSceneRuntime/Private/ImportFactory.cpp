#include "ImportFactory.h"
#include "Engine/World.h"
#include "ParamParser.hpp"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/PropertyPortFlags.h"
#include "FoliageType.h"
#include "InstancedFoliageActor.h"
#include "Serialization/ArchiveReplaceObjectRef.h"


static void RemapProperty(UProperty* Property, int32 Index, const TMap<AActor*, AActor*>& ActorRemapper, uint8* DestData)
{
	if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
	{
		// If there's a concrete index, use that, otherwise iterate all array members (for the case that this property is inside a struct, or there is exactly one element)
		const int32 Num = (Index == INDEX_NONE) ? ObjectProperty->ArrayDim : 1;
		const int32 StartIndex = (Index == INDEX_NONE) ? 0 : Index;
		for (int32 Count = 0; Count < Num; Count++)
		{
			uint8* PropertyAddr = ObjectProperty->ContainerPtrToValuePtr<uint8>(DestData, StartIndex + Count);
			AActor* Actor = Cast<AActor>(ObjectProperty->GetObjectPropertyValue(PropertyAddr));
			if (Actor)
			{
				AActor* const* RemappedObject = ActorRemapper.Find(Actor);
				if (RemappedObject && (*RemappedObject)->GetClass()->IsChildOf(ObjectProperty->PropertyClass))
				{
					ObjectProperty->SetObjectPropertyValue(PropertyAddr, *RemappedObject);
				}
			}

		}
	}
	else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(DestData));
		if (Index != INDEX_NONE)
		{
			RemapProperty(ArrayProperty->Inner, INDEX_NONE, ActorRemapper, ArrayHelper.GetRawPtr(Index));
		}
		else
		{
			for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ArrayIndex++)
			{
				RemapProperty(ArrayProperty->Inner, INDEX_NONE, ActorRemapper, ArrayHelper.GetRawPtr(ArrayIndex));
			}
		}
	}
	else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
	{
		if (Index != INDEX_NONE)
		{
			// If a concrete index was given, remap just that
			for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
			{
				RemapProperty(*It, INDEX_NONE, ActorRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Index));
			}
		}
		else
		{
			// If no concrete index was given, either the ArrayDim is 1 (i.e. not a static array), or the struct is within
			// a deeper structure (an array or another struct) and we cannot know which element was changed, so iterate through all elements.
			for (int32 Count = 0; Count < StructProperty->ArrayDim; Count++)
			{
				for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
				{
					RemapProperty(*It, INDEX_NONE, ActorRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Count));
				}
			}
		}
	}

}
UImportFactory::UImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


UObject* UImportFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd
)
{
	// GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);

	UWorld* World = GWorld;
	//@todo locked levels - if lock state is persistent, do we need to check for whether the level is locked?
#ifdef MULTI_LEVEL_IMPORT
	// this level is the current level for pasting. If we get a named level, not for pasting, we will look up the level, and overwrite this
	ULevel*				OldCurrentLevel = World->GetCurrentLevel();
	check(OldCurrentLevel);
#endif

	UPackage* RootMapPackage = Cast<UPackage>(InParent);
	TMap<FString, UPackage*> MapPackages;
	TMap<AActor*, AActor*> MapActors;
	// Assumes data is being imported over top of a new, valid map.
	FParse::Next(&Buffer);
	if (GetBEGIN(&Buffer, TEXT("MAP")))
	{
		if (ULevel* Level = Cast<ULevel>(InParent))
		{
			World = Level->GetWorld();
		}

		if (RootMapPackage)
		{
			FString MapName;
			if (FParse::Value(Buffer, TEXT("Name="), MapName))
			{
				// Advance the buffer
				Buffer += FCString::Strlen(TEXT("Name="));
				Buffer += MapName.Len();
				// Check to make sure that there are no naming conflicts
				if (RootMapPackage->Rename(*MapName, nullptr, REN_Test | REN_ForceNoResetLoaders))
				{
					// Rename it!
					RootMapPackage->Rename(*MapName, nullptr, REN_ForceNoResetLoaders);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("The Root map package name : '%s', conflicts with the existing object : '%s'"), *RootMapPackage->GetFullName(), *MapName);
					//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
					return nullptr;
				}

				// Stick it in the package map
				MapPackages.Add(MapName, RootMapPackage);
			}
		}
	}
	else
	{
		return World;
	}

	bool bIsExpectingNewMapTag = false;

	// We need to detect if the .t3d file is the entire level or just selected actors, because we
	// don't want to replace the WorldSettings and BuildBrush if they already exist. To know if we
	// can skip the WorldSettings and BuilderBrush (which will always be the first two actors if the entire
	// level was exported), we make sure the first actor is a WorldSettings, if it is, and we already had
	// a WorldSettings, then we skip the builder brush
	// In other words, if we are importing a full level into a full level, we don't want to import
	// the WorldSettings and BuildBrush
	bool bShouldSkipImportSpecialActors = false;
	bool bHitLevelToken = false;

	FString MapPackageText;

	int32 ActorIndex = 0;

	//@todo locked levels - what needs to happen here?


	// Maintain a list of a new actors and the text they were created from.
	TMap<AActor*, FString> NewActorMap;
	// TMap< FString, AGroupActor* > NewGroups; // Key=The orig actor's group's name, Value=The new actor's group.

	// Maintain a lookup for the new actors, keyed by their source FName.
	TMap<FName, AActor*> NewActorsFNames;

	// Maintain a lookup from existing to new actors, used when replacing internal references when copy+pasting / duplicating
	TMap<AActor*, AActor*> ExistingToNewMap;

	// Maintain a lookup of the new actors to their parent and socket attachment if provided.
	struct FAttachmentDetail
	{
		const FName ParentName;
		const FName SocketName;
		FAttachmentDetail(const FName InParentName, const FName InSocketName) : ParentName(InParentName), SocketName(InSocketName) {}
	};
	TMap<AActor*, FAttachmentDetail> NewActorsAttachmentMap;

	FString StrLine;
	while (FParse::Line(&Buffer, StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetBEGIN(&Str, TEXT("ACTOR")))
		{
			UClass* TempClass;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE))
			{
				// Get actor name.
				FName ActorUniqueName(NAME_None);
				FName ActorSourceName(NAME_None);
				FParse::Value(Str, TEXT("NAME="), ActorSourceName);
				ActorUniqueName = ActorSourceName;
				// Make sure this name is unique.
				AActor* Found = nullptr;
				if (ActorUniqueName != NAME_None)
				{
					// look in the current level for the same named actor
					Found = FindObject<AActor>(World->GetCurrentLevel(), *ActorUniqueName.ToString());
				}
				if (Found)
				{
					ActorUniqueName = MakeUniqueObjectName(World->GetCurrentLevel(), TempClass, ActorUniqueName);
				}

				// Get parent name for attachment.
				FName ActorParentName(NAME_None);
				FParse::Value(Str, TEXT("ParentActor="), ActorParentName);

				// Get socket name for attachment.
				FName ActorParentSocket(NAME_None);
				FParse::Value(Str, TEXT("SocketName="), ActorParentSocket);

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				AActor* Archetype = nullptr;
				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				{
					// if given a name, break it up along the ' so separate the class from the name
					FString ObjectClass;
					FString ObjectPath;
					if (FPackageName::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
					{
						// find the class
						UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
						if (ArchetypeClass)
						{
							if (ArchetypeClass->IsChildOf(AActor::StaticClass()))
							{
								// if we had the class, find the archetype
								Archetype = Cast<AActor>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"), Str, *ObjectClass);
							}
						}
					}
				}

				if (TempClass->IsChildOf(AWorldSettings::StaticClass()))
				{
					// if we see a WorldSettings, then we are importing an entire level, so if we
					// are importing into an existing level, then we should not import the next actor
					// which will be the builder brush
					check(ActorIndex == 0);

					// if we have any actors, then we are importing into an existing level
					if (World->GetCurrentLevel()->Actors.Num())
					{
						check(World->GetCurrentLevel()->Actors[0]->IsA(AWorldSettings::StaticClass()));

						// full level into full level, skip the first two actors
						bShouldSkipImportSpecialActors = true;
					}
				}

				// Get property text.
				FString PropText, PropertyLine;
				while
					(GetEND(&Buffer, TEXT("ACTOR")) == 0
						&& FParse::Line(&Buffer, PropertyLine))
				{
					PropText += *PropertyLine;
					PropText += TEXT("\r\n");
				}

				if (!(bShouldSkipImportSpecialActors && ActorIndex < 2))
				{
					// Don't import the default physics volume, as it doesn't have a UModel associated with it
					// and thus will not import properly.
					if (!TempClass->IsChildOf(ADefaultPhysicsVolume::StaticClass()))
					{
						// Create a new actor.
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.Name = ActorUniqueName;
						SpawnInfo.Template = Archetype;
						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						AActor* NewActor = World->SpawnActor(TempClass, nullptr, nullptr, SpawnInfo);

						if (NewActor)
						{
							// Store the new actor and the text it should be initialized with.
							NewActorMap.Add(NewActor, *PropText);

							// Store the copy to original actor mapping
							MapActors.Add(NewActor, Found);

							// Store the new actor against its source actor name (not the one that may have been made unique)
							if (ActorSourceName != NAME_None)
							{
								NewActorsFNames.Add(ActorSourceName, NewActor);
								if (Found)
								{
									ExistingToNewMap.Add(Found, NewActor);
								}
							}

							// Store the new actor with its parent's FName, and socket FName if applicable
							if (ActorParentName != NAME_None)
							{
								NewActorsAttachmentMap.Add(NewActor, FAttachmentDetail(ActorParentName, ActorParentSocket));
							}
						}
					}
				}

				// increment the number of actors we imported
				ActorIndex++;
			}
		}
	}


	for (const auto& ActorMapElement : NewActorMap)
	{
		AActor* Actor = ActorMapElement.Key;

		// Import properties if the new actor is 
		bool		bActorChanged = false;
		const FString&	PropText = ActorMapElement.Value;
		if (Actor)
		{
			UImportFactory::ImportObjectProperties((uint8*)Actor, *PropText, Actor->GetClass(), Actor, Actor, 0, INDEX_NONE, NULL, &ExistingToNewMap);
		}
	}

	return World;
}

const TCHAR* UImportFactory::ImportObjectProperties(FImportObjectParamsEx& InParams)
{
	FDefaultPropertiesContextSupplierEx Supplier;
	if (InParams.LineNumber != INDEX_NONE)
	{
		if (InParams.SubobjectRoot == NULL)
		{
			Supplier.PackageName = InParams.ObjectStruct->GetOwnerClass() ? InParams.ObjectStruct->GetOwnerClass()->GetOutermost()->GetName() : InParams.ObjectStruct->GetOutermost()->GetName();
			Supplier.ClassName = InParams.ObjectStruct->GetOwnerClass() ? InParams.ObjectStruct->GetOwnerClass()->GetName() : FName(NAME_None).ToString();
			Supplier.CurrentLine = InParams.LineNumber;

			ContextSupplier = &Supplier; //-V506
		}
		else
		{
			if (ContextSupplier != NULL)
			{
				ContextSupplier->CurrentLine = InParams.LineNumber;
			}
		}

	}

	//if (InParams.bShouldCallEditChange && InParams.SubobjectOuter != NULL)
	//{
	//	InParams.SubobjectOuter->PreEditChange(NULL);
	//}

	FObjectInstancingGraph* CurrentInstanceGraph = InParams.InInstanceGraph;
	if (InParams.SubobjectRoot != NULL && InParams.SubobjectRoot != UObject::StaticClass()->GetDefaultObject())
	{
		if (CurrentInstanceGraph == NULL)
		{
			CurrentInstanceGraph = new FObjectInstancingGraph;
		}
		CurrentInstanceGraph->SetDestinationRoot(InParams.SubobjectRoot);
	}

	FObjectInstancingGraph TempGraph;
	FObjectInstancingGraph& InstanceGraph = CurrentInstanceGraph ? *CurrentInstanceGraph : TempGraph;

	// Parse the object properties.
	const TCHAR* NewSourceText =
		ImportProperties(
			InParams.DestData,
			InParams.SourceText,
			InParams.ObjectStruct,
			InParams.SubobjectRoot,
			InParams.SubobjectOuter,
			InParams.Depth,
			InstanceGraph,
			InParams.ActorRemapper
		);

	if (InParams.SubobjectOuter != NULL)
	{
		check(InParams.SubobjectRoot);

		// Update the object properties to point to the newly imported component objects.
		// Templates inside classes never need to have components instanced.
		if (!InParams.SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject))
		{
			UObject* SubobjectArchetype = InParams.SubobjectOuter->GetArchetype();
			InParams.ObjectStruct->InstanceSubobjectTemplates(InParams.DestData, SubobjectArchetype, SubobjectArchetype->GetClass(),
				InParams.SubobjectOuter, &InstanceGraph);
		}
#if WITH_EDITOR
		if (InParams.bShouldCallEditChange)
		{
			// notify the object that it has just been imported
			InParams.SubobjectOuter->PostEditImport();

			// notify the object that it has been edited
			InParams.SubobjectOuter->PostEditChange();
		}
#endif
		InParams.SubobjectRoot->CheckDefaultSubobjects();
	}

	if (InParams.LineNumber != INDEX_NONE)
	{
		if (ContextSupplier == &Supplier)
		{
			ContextSupplier = NULL;
		}
	}

	// if we created the instance graph, delete it now
	if (CurrentInstanceGraph != NULL && InParams.InInstanceGraph == NULL)
	{
		delete CurrentInstanceGraph;
		CurrentInstanceGraph = NULL;
	}

	return NewSourceText;
}

const TCHAR* UImportFactory::ImportObjectProperties(uint8* DestData, const TCHAR* SourceText, UStruct* ObjectStruct, UObject* SubobjectRoot, UObject* SubobjectOuter, int32 Depth, int32 LineNumber /*= INDEX_NONE*/, FObjectInstancingGraph* InstanceGraph /*= NULL*/, const TMap<AActor*, AActor*>* ActorRemapper /*= NULL */)
{
	FImportObjectParamsEx Params;
	{
		Params.DestData = DestData;
		Params.SourceText = SourceText;
		Params.ObjectStruct = ObjectStruct;
		Params.SubobjectRoot = SubobjectRoot;
		Params.SubobjectOuter = SubobjectOuter;
		Params.Depth = Depth;
		Params.LineNumber = LineNumber;
		Params.InInstanceGraph = InstanceGraph;
		Params.ActorRemapper = ActorRemapper;

		// This implementation always calls PreEditChange/PostEditChange
		Params.bShouldCallEditChange = true;
	}

	return UImportFactory::ImportObjectProperties(Params);
}


/**
 * Parse and import text as property values for the object specified.  This function should never be called directly - use ImportObjectProperties instead.
 *
 * @param	ObjectStruct				the struct for the data we're importing
 * @param	DestData					the location to import the property values to
 * @param	SourceText					pointer to a buffer containing the values that should be parsed and imported
 * @param	SubobjectRoot					when dealing with nested subobjects, corresponds to the top-most outer that
 *										is not a subobject/template
 * @param	SubobjectOuter				the outer to use for creating subobjects/components. NULL when importing structdefaultproperties
 * @param	Depth						current nesting level
 * @param	InstanceGraph				contains the mappings of instanced objects and components to their templates
 * @param	ActorRemapper				a map of existing actors to new instances, used to replace internal references when a number of actors are copy+pasted
 *
 * @return	NULL if the default values couldn't be imported
 */
const TCHAR* UImportFactory::ImportProperties(
	uint8*						DestData,
	const TCHAR*				SourceText,
	UStruct*					ObjectStruct,
	UObject*					SubobjectRoot,
	UObject*					SubobjectOuter,
	int32						Depth,
	FObjectInstancingGraph&		InstanceGraph,
	const TMap<AActor*, AActor*>* ActorRemapper
)
{
	check(!GIsUCCMakeStandaloneHeaderGenerator);
	check(ObjectStruct != NULL);
	check(DestData != NULL);

	if (SourceText == NULL)
		return NULL;

	// Cannot create subobjects when importing struct defaults, or if SubobjectOuter (used as the Outer for any subobject declarations encountered) is NULL
	bool bSubObjectsAllowed = !ObjectStruct->IsA(UScriptStruct::StaticClass()) && SubobjectOuter != NULL;

	// true when DestData corresponds to a subobject in a class default object
	bool bSubObject = false;

	UClass* ComponentOwnerClass = NULL;

	if (bSubObjectsAllowed)
	{
		bSubObject = SubobjectRoot != NULL && SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject);
		if (SubobjectRoot == NULL)
		{
			SubobjectRoot = SubobjectOuter;
		}

		ComponentOwnerClass = SubobjectOuter != NULL
			? SubobjectOuter->IsA(UClass::StaticClass())
			? CastChecked<UClass>(SubobjectOuter)
			: SubobjectOuter->GetClass()
			: NULL;
	}


	// The PortFlags to use for all ImportText calls
	uint32 PortFlags = PPF_Delimited | PPF_CheckReferences;
#if WITH_EDITORONLY_DATA
	if (GIsImportingT3D)
	{
		PortFlags |= PPF_AttemptNonQualifiedSearch;
	}
#endif

	FString StrLine;

	TArray<FDefinedProperty> DefinedProperties;

	// Parse all objects stored in the actor.
	// Build list of all text properties.
	bool ImportedBrush = 0;
	int32 LinesConsumed = 0;
	while (FParse::LineExtended(&SourceText, StrLine, LinesConsumed, true))
	{
		// remove extra whitespace and optional semicolon from the end of the line
		{
			int32 Length = StrLine.Len();
			while (Length > 0 &&
				(StrLine[Length - 1] == TCHAR(';') || StrLine[Length - 1] == TCHAR(' ') || StrLine[Length - 1] == 9))
			{
				Length--;
			}
			if (Length != StrLine.Len())
			{
				StrLine = StrLine.Left(Length);
			}
		}

		if (ContextSupplier != NULL)
		{
			ContextSupplier->CurrentLine += LinesConsumed;
		}
		if (StrLine.Len() == 0)
		{
			continue;
		}

		const TCHAR* Str = *StrLine;

		//int32 NewLineNumber;
		//if (FParse::Value(Str, TEXT("linenumber="), NewLineNumber))
		//{
		//	if (ContextSupplier != NULL)
		//	{
		//		ContextSupplier->CurrentLine = NewLineNumber;
		//	}
		//}
		//else if (GetBEGIN(&Str, TEXT("Brush")) && ObjectStruct->IsChildOf(ABrush::StaticClass()))
		//{
		//	// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
		//	if (!bSubObjectsAllowed)
		//	{
		//      UE_LOG(LogTemp,Log,TEXT("BEGIN BRUSH: Subobjects are not allowed in this context"));
		//		return NULL;
		//	}

		//	// Parse brush on this line.
		//	TCHAR BrushName[NAME_SIZE];
		//	if (FParse::Value(Str, TEXT("Name="), BrushName, NAME_SIZE))
		//	{
		//		// If an initialized brush with this name already exists in the level, rename the existing one.
		//		// It is deemed to be initialized if it has a non-zero poly count.
		//		// If it is uninitialized, the existing object will have been created by a forward reference in the import text,
		//		// and it will now be redefined.  This relies on the behavior that NewObject<> will return an existing pointer
		//		// if an object with the same name and outer is passed.
		//		UModel* ExistingBrush = FindObject<UModel>(SubobjectRoot, BrushName);
		//		if (ExistingBrush && ExistingBrush->Polys && ExistingBrush->Polys->Element.Num() > 0)
		//		{
		//			ExistingBrush->Rename();
		//		}

		//		// Create model.
		//		UModelFactory* ModelFactory = NewObject<UModelFactory>();
		//		ModelFactory->FactoryCreateText(UModel::StaticClass(), SubobjectRoot, FName(BrushName, FNAME_Add), RF_NoFlags, NULL, TEXT("t3d"), SourceText, SourceText + FCString::Strlen(SourceText), Warn);
		//		ImportedBrush = 1;
		//	}
		//}
#if ENGINE_MINOR_VERSION > 422
		/*else */if (GetBEGIN(&Str, TEXT("Foliage")))
		{
			UFoliageType* SourceFoliageType;
			FName ComponentName;
			if (SubobjectRoot &&
				ParseObject<UFoliageType>(Str, TEXT("FoliageType="), SourceFoliageType, ANY_PACKAGE) &&
				FParse::Value(Str, TEXT("Component="), ComponentName))
			{
				UPrimitiveComponent* ActorComponent = FindObjectFast<UPrimitiveComponent>(SubobjectRoot, ComponentName);

				if (ActorComponent && ActorComponent->GetComponentLevel())
				{
					AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ActorComponent->GetComponentLevel(), true);

					FFoliageInfo* MeshInfo = nullptr;
					UFoliageType* FoliageType = IFA->AddFoliageType(SourceFoliageType, &MeshInfo);

					const TCHAR* StrPtr;
					FString TextLine;
					while (MeshInfo && FParse::Line(&SourceText, TextLine))
					{
						StrPtr = *TextLine;
						if (GetEND(&StrPtr, TEXT("Foliage")))
						{
							break;
						}

						// Parse the instance properties
						FFoliageInstance Instance;
						FString Temp;
						if (FParse::Value(StrPtr, TEXT("Location="), Temp, false))
						{
							GetFVECTOR(*Temp, Instance.Location);
						}
						if (FParse::Value(StrPtr, TEXT("Rotation="), Temp, false))
						{
							GetFROTATOR(*Temp, Instance.Rotation, 1);
						}
						if (FParse::Value(StrPtr, TEXT("PreAlignRotation="), Temp, false))
						{
							GetFROTATOR(*Temp, Instance.PreAlignRotation, 1);
						}
						if (FParse::Value(StrPtr, TEXT("DrawScale3D="), Temp, false))
						{
							GetFVECTOR(*Temp, Instance.DrawScale3D);
						}
						FParse::Value(StrPtr, TEXT("Flags="), Instance.Flags);

						// Add the instance
						MeshInfo->AddInstance(IFA, FoliageType, Instance, ActorComponent);
					}

					if (MeshInfo)
					{
						MeshInfo->Refresh(IFA, true, true);
					}
				}
			}
		}
		else 
#endif
		if (GetBEGIN(&Str, TEXT("Object")))
		{
			// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
			if (!bSubObjectsAllowed)
			{
				UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: Subobjects are not allowed in this context"));
				return NULL;
			}

			// Parse subobject default properties.
			// Note: default properties subobjects have compiled class as their Outer (used for localization).
			UClass*	TemplateClass = NULL;
			bool bInvalidClass = false;
			ParseObject<UClass>(Str, TEXT("Class="), TemplateClass, ANY_PACKAGE, &bInvalidClass);

			if (bInvalidClass)
			{
				UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: Invalid class specified: %s"), *StrLine);
				return NULL;
			}

			// parse the name of the template
			FName	TemplateName = NAME_None;
			FParse::Value(Str, TEXT("Name="), TemplateName);
			if (TemplateName == NAME_None)
			{
				UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: Must specify valid name for subobject/component: %s"), *StrLine);
				return NULL;
			}

			// points to the parent class's template subobject/component, if we are overriding a subobject/component declared in our parent class
			UObject* BaseTemplate = NULL;
			bool bRedefiningSubobject = false;
			if (TemplateClass)
			{
			}
			else
			{
				// next, verify that a template actually exists in the parent class
				UClass* ParentClass = ComponentOwnerClass->GetSuperClass();
				check(ParentClass);

				UObject* ParentCDO = ParentClass->GetDefaultObject();
				check(ParentCDO);

				BaseTemplate = StaticFindObjectFast(UObject::StaticClass(), SubobjectOuter, TemplateName);
				bRedefiningSubobject = (BaseTemplate != NULL);

				if (BaseTemplate == NULL)
				{
					BaseTemplate = StaticFindObjectFast(UObject::StaticClass(), ParentCDO, TemplateName);
				}

				if (BaseTemplate == NULL)
				{
					// wasn't found
					UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: No base template named %s found in parent class %s: %s"), *TemplateName.ToString(), *ParentClass->GetName(), *StrLine);
					return NULL;
				}

				TemplateClass = BaseTemplate->GetClass();
			}

			// because the outer won't be a default object

			checkSlow(TemplateClass != NULL);
			if (bRedefiningSubobject)
			{
				// since we're redefining an object in the same text block, only need to import properties again
				SourceText = ImportObjectProperties((uint8*)BaseTemplate, SourceText, TemplateClass, SubobjectRoot, BaseTemplate,
					Depth + 1, ContextSupplier ? ContextSupplier->CurrentLine : 0, &InstanceGraph, ActorRemapper);
			}
			else
			{
				UObject* Archetype = NULL;
				UObject* ComponentTemplate = NULL;

				// Since we are changing the class we can't use the Archetype,
				// however that is fine since we will have been editing the CDO anyways
				//if (!FBlueprintEditorUtils::IsAnonymousBlueprintClass(SubobjectOuter->GetClass()))
				//{
				//	// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				//	FString ArchetypeName;
				//	if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				//	{
				//		// if given a name, break it up along the ' so separate the class from the name
				//		FString ObjectClass;
				//		FString ObjectPath;
				//		if (FPackageName::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
				//		{
				//			// find the class
				//			UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
				//			if (ArchetypeClass)
				//			{
				//				ObjectPath = ObjectPath.TrimQuotes();

				//				UPackage* FindInPackage = (FPackageName::IsShortPackageName(ObjectPath) ? ANY_PACKAGE : nullptr);

				//				// if we had the class, find the archetype
				//				Archetype = StaticFindObject(ArchetypeClass, FindInPackage, *ObjectPath);
				//			}
				//		}
				//	}
				//}

				if (SubobjectOuter->HasAnyFlags(RF_ClassDefaultObject))
				{
					if (!Archetype) // if an archetype was specified explicitly, we will stick with that
					{
						Archetype = ComponentOwnerClass->GetDefaultSubobjectByName(TemplateName);
						if (Archetype)
						{
							if (BaseTemplate == NULL)
							{
								// BaseTemplate should only be NULL if the Begin Object line specified a class
								UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: The component name %s is already used (if you want to override the component, don't specify a class): %s"), *TemplateName.ToString(), *StrLine);
								return NULL;
							}

							// the component currently in the component template map and the base template should be the same
							checkf(Archetype == BaseTemplate, TEXT("OverrideComponent: '%s'   BaseTemplate: '%s'"), *Archetype->GetFullName(), *BaseTemplate->GetFullName());
						}
					}
				}
				else // handle the non-template case (subobjects and non-template components)
				{
					ComponentTemplate = FindObject<UObject>(SubobjectOuter, *TemplateName.ToString());
					if (ComponentTemplate != NULL)
					{
						// if we're overriding a subobject declared in a parent class, we should already have an object with that name that
						// was instanced when ComponentOwnerClass's CDO was initialized; if so, it's archetype should be the BaseTemplate.  If it
						// isn't, then there are two unrelated subobject definitions using the same name.
						if (ComponentTemplate->GetArchetype() != BaseTemplate)
						{
						}
						else if (BaseTemplate == NULL)
						{
							// BaseTemplate should only be NULL if the Begin Object line specified a class
							UE_LOG(LogTemp, Log, TEXT("BEGIN OBJECT: A subobject named %s is already declared in a parent class.  If you intended to override that subobject, don't specify a class in the derived subobject definition: %s"), *TemplateName.ToString(), *StrLine);
							return NULL;
						}
					}

				}

				// Propagate object flags to the sub object.
				EObjectFlags NewFlags = SubobjectOuter->GetMaskedFlags(RF_PropagateToSubObjects);

				if (!Archetype) // no override and we didn't find one from the class table, so go with the base
				{
					Archetype = BaseTemplate;
				}

				UObject* OldComponent = NULL;
				if (ComponentTemplate)
				{
					bool bIsOkToReuse = ComponentTemplate->GetClass() == TemplateClass
						&& ComponentTemplate->GetOuter() == SubobjectOuter
						&& ComponentTemplate->GetFName() == TemplateName
						&& (ComponentTemplate->GetArchetype() == Archetype || !Archetype);

					if (!bIsOkToReuse)
					{
						UE_LOG(LogTemp, Log, TEXT("Could not reuse component instance %s, name clash?"), *ComponentTemplate->GetFullName());
						ComponentTemplate->Rename(nullptr, nullptr, REN_DontCreateRedirectors); // just abandon the existing component, we are going to create
						OldComponent = ComponentTemplate;
						ComponentTemplate = NULL;
					}
				}


				if (!ComponentTemplate)
				{
					ComponentTemplate = NewObject<UObject>(
						SubobjectOuter,
						TemplateClass,
						TemplateName,
						NewFlags,
						Archetype,
						!!SubobjectOuter,
						&InstanceGraph
						);
					
				}
				else
				{
					// We do not want to set RF_Transactional for construction script created components, so we have to monkey with things here
					if (NewFlags & RF_Transactional)
					{
						UActorComponent* Component = Cast<UActorComponent>(ComponentTemplate);
						if (Component && Component->IsCreatedByConstructionScript())
						{
							NewFlags &= ~RF_Transactional;
						}
					}

					// Make sure desired flags are set - existing object could be pending kill
					ComponentTemplate->ClearFlags(RF_AllFlags);
					ComponentTemplate->ClearInternalFlags(EInternalObjectFlags::AllFlags);
					ComponentTemplate->SetFlags(NewFlags);
				}

				// replace all properties in this subobject outer' class that point to the original subobject with the new subobject
				TMap<UObject*, UObject*> ReplacementMap;
				if (Archetype)
				{
					checkSlow(ComponentTemplate->GetArchetype() == Archetype);
					ReplacementMap.Add(Archetype, ComponentTemplate);
					InstanceGraph.AddNewInstance(ComponentTemplate);
				}
				if (OldComponent)
				{
					ReplacementMap.Add(OldComponent, ComponentTemplate);
				}
				FArchiveReplaceObjectRef<UObject> ReplaceAr(SubobjectOuter, ReplacementMap, false, false, true);

				// import the properties for the subobject
				SourceText = ImportObjectProperties(
					(uint8*)ComponentTemplate,
					SourceText,
					TemplateClass,
					SubobjectRoot,
					ComponentTemplate,
					Depth + 1,
					ContextSupplier ? ContextSupplier->CurrentLine : 0,
					&InstanceGraph,
					ActorRemapper
				);
				if (ComponentTemplate && ComponentTemplate->GetClass()->IsChildOf(UActorComponent::StaticClass()))
				{
					UActorComponent* ComponentIns = Cast<UActorComponent>(ComponentTemplate);
					ComponentIns->OnComponentCreated();
					ComponentIns->ReregisterComponent();

				}
			}
		}
		else if (FParse::Command(&Str, TEXT("CustomProperties")))
		{
			check(SubobjectOuter);

			SubobjectOuter->ImportCustomProperties(Str,  /*Warn*/NULL);
		}
		else if (GetEND(&Str, TEXT("Actor")) || GetEND(&Str, TEXT("DefaultProperties")) || GetEND(&Str, TEXT("structdefaultproperties")) || (GetEND(&Str, TEXT("Object")) && Depth))
		{
			// End of properties.
			break;
		}
		else if (GetREMOVE(&Str, TEXT("Component")))
		{
			checkf(false, TEXT("Remove component is illegal in pasted text"));
		}
		else
		{
			// Property.
			UProperty::ImportSingleProperty(Str, DestData, ObjectStruct, SubobjectOuter, PortFlags, /*Warn*/NULL, DefinedProperties);
		}
	}

	if (ActorRemapper)
	{
		for (const auto& DefinedProperty : DefinedProperties)
		{
			RemapProperty(DefinedProperty.Property, DefinedProperty.Index, *ActorRemapper, DestData);
		}
	}

	// Prepare brush.
	//if (ImportedBrush && ObjectStruct->IsChildOf<ABrush>() && !ObjectStruct->IsChildOf<AVolume>())
	//{
	//	check(GIsEditor);
	//	ABrush* Actor = (ABrush*)DestData;
	//	check(Actor->GetBrushComponent());
	//	if (Actor->GetBrushComponent()->Mobility == EComponentMobility::Static)
	//	{
	//		// Prepare static brush.
	//		Actor->SetNotForClientOrServer();
	//	}
	//	else
	//	{
	//		// Prepare moving brush.
	//		FBSPOps::csgPrepMovingBrush(Actor);
	//	}
	//}

	return SourceText;
}
