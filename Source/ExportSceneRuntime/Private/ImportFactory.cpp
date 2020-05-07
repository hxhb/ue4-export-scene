//#include "ImportFactory.h"
//#include "Engine/World.h"
//#include "ParamParser.hpp"
//#include "Misc/PackageName.h"
//#include "Materials/MaterialInterface.h"
//#include "GameFramework/DefaultPhysicsVolume.h"
//
//UImportFactory::UImportFactory(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//	SupportedClass = UWorld::StaticClass();
//	Formats.Add(TEXT("t3d;Unreal World"));
//
//	bCreateNew = false;
//	bText = true;
//	bEditorImport = false;
//}
//
//
//UObject* UImportFactory::FactoryCreateText
//(
//	UClass*				Class,
//	UObject*			InParent,
//	FName				Name,
//	EObjectFlags		Flags,
//	UObject*			Context,
//	const TCHAR*		Type,
//	const TCHAR*&		Buffer,
//	const TCHAR*		BufferEnd,
//	FFeedbackContext*	Warn
//)
//{
//	// GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);
//
//	UWorld* World = GWorld;
//	//@todo locked levels - if lock state is persistent, do we need to check for whether the level is locked?
//#ifdef MULTI_LEVEL_IMPORT
//	// this level is the current level for pasting. If we get a named level, not for pasting, we will look up the level, and overwrite this
//	ULevel*				OldCurrentLevel = World->GetCurrentLevel();
//	check(OldCurrentLevel);
//#endif
//
//	UPackage* RootMapPackage = Cast<UPackage>(InParent);
//	TMap<FString, UPackage*> MapPackages;
//	TMap<AActor*, AActor*> MapActors;
//	// Assumes data is being imported over top of a new, valid map.
//	FParse::Next(&Buffer);
//	if (GetBEGIN(&Buffer, TEXT("MAP")))
//	{
//		if (ULevel* Level = Cast<ULevel>(InParent))
//		{
//			World = Level->GetWorld();
//		}
//
//		if (RootMapPackage)
//		{
//			FString MapName;
//			if (FParse::Value(Buffer, TEXT("Name="), MapName))
//			{
//				// Advance the buffer
//				Buffer += FCString::Strlen(TEXT("Name="));
//				Buffer += MapName.Len();
//				// Check to make sure that there are no naming conflicts
//				if (RootMapPackage->Rename(*MapName, nullptr, REN_Test | REN_ForceNoResetLoaders))
//				{
//					// Rename it!
//					RootMapPackage->Rename(*MapName, nullptr, REN_ForceNoResetLoaders);
//				}
//				else
//				{
//					//Warn->Logf(ELogVerbosity::Warning, TEXT("The Root map package name : '%s', conflicts with the existing object : '%s'"), *RootMapPackage->GetFullName(), *MapName);
//					//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
//					return nullptr;
//				}
//
//				// Stick it in the package map
//				MapPackages.Add(MapName, RootMapPackage);
//			}
//		}
//	}
//	else
//	{
//		return World;
//	}
//
//	bool bIsExpectingNewMapTag = false;
//
//	// Unselect all actors.
//	//if (GWorld == World)
//	//{
//	//	GEditor->SelectNone(false, false);
//
//	//	// Mark us importing a T3D (only from a file, not from copy/paste).
//	//	GEditor->IsImportingT3D = (FCString::Stricmp(Type, TEXT("paste")) != 0) && (FCString::Stricmp(Type, TEXT("move")) != 0);
//	//	GIsImportingT3D = GEditor->IsImportingT3D;
//	//}
//
//	// We need to detect if the .t3d file is the entire level or just selected actors, because we
//	// don't want to replace the WorldSettings and BuildBrush if they already exist. To know if we
//	// can skip the WorldSettings and BuilderBrush (which will always be the first two actors if the entire
//	// level was exported), we make sure the first actor is a WorldSettings, if it is, and we already had
//	// a WorldSettings, then we skip the builder brush
//	// In other words, if we are importing a full level into a full level, we don't want to import
//	// the WorldSettings and BuildBrush
//	bool bShouldSkipImportSpecialActors = false;
//	bool bHitLevelToken = false;
//
//	FString MapPackageText;
//
//	int32 ActorIndex = 0;
//
//	//@todo locked levels - what needs to happen here?
//
//
//	// Maintain a list of a new actors and the text they were created from.
//	TMap<AActor*, FString> NewActorMap;
//	// TMap< FString, AGroupActor* > NewGroups; // Key=The orig actor's group's name, Value=The new actor's group.
//
//	// Maintain a lookup for the new actors, keyed by their source FName.
//	TMap<FName, AActor*> NewActorsFNames;
//
//	// Maintain a lookup from existing to new actors, used when replacing internal references when copy+pasting / duplicating
//	TMap<AActor*, AActor*> ExistingToNewMap;
//
//	// Maintain a lookup of the new actors to their parent and socket attachment if provided.
//	struct FAttachmentDetail
//	{
//		const FName ParentName;
//		const FName SocketName;
//		FAttachmentDetail(const FName InParentName, const FName InSocketName) : ParentName(InParentName), SocketName(InSocketName) {}
//	};
//	TMap<AActor*, FAttachmentDetail> NewActorsAttachmentMap;
//
//	FString StrLine;
//	while (FParse::Line(&Buffer, StrLine))
//	{
//		const TCHAR* Str = *StrLine;
//
//		if (GetBEGIN(&Str, TEXT("ACTOR")))
//		{
//			UClass* TempClass;
//			if (ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE))
//			{
//				// Get actor name.
//				FName ActorUniqueName(NAME_None);
//				FName ActorSourceName(NAME_None);
//				FParse::Value(Str, TEXT("NAME="), ActorSourceName);
//				ActorUniqueName = ActorSourceName;
//				// Make sure this name is unique.
//				AActor* Found = nullptr;
//				if (ActorUniqueName != NAME_None)
//				{
//					// look in the current level for the same named actor
//					Found = FindObject<AActor>(World->GetCurrentLevel(), *ActorUniqueName.ToString());
//				}
//				if (Found)
//				{
//					ActorUniqueName = MakeUniqueObjectName(World->GetCurrentLevel(), TempClass, ActorUniqueName);
//				}
//
//				// Get parent name for attachment.
//				FName ActorParentName(NAME_None);
//				FParse::Value(Str, TEXT("ParentActor="), ActorParentName);
//
//				// Get socket name for attachment.
//				FName ActorParentSocket(NAME_None);
//				FParse::Value(Str, TEXT("SocketName="), ActorParentSocket);
//
//				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
//				FString ArchetypeName;
//				AActor* Archetype = nullptr;
//				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
//				{
//					// if given a name, break it up along the ' so separate the class from the name
//					FString ObjectClass;
//					FString ObjectPath;
//					if (FPackageName::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
//					{
//						// find the class
//						UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
//						if (ArchetypeClass)
//						{
//							if (ArchetypeClass->IsChildOf(AActor::StaticClass()))
//							{
//								// if we had the class, find the archetype
//								Archetype = Cast<AActor>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
//							}
//							else
//							{
//								UE_LOG(LogTemp, Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"), Str, *ObjectClass);
//								//Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"),Str, *ObjectClass);
//							}
//						}
//					}
//				}
//
//				if (TempClass->IsChildOf(AWorldSettings::StaticClass()))
//				{
//					// if we see a WorldSettings, then we are importing an entire level, so if we
//					// are importing into an existing level, then we should not import the next actor
//					// which will be the builder brush
//					check(ActorIndex == 0);
//
//					// if we have any actors, then we are importing into an existing level
//					if (World->GetCurrentLevel()->Actors.Num())
//					{
//						check(World->GetCurrentLevel()->Actors[0]->IsA(AWorldSettings::StaticClass()));
//
//						// full level into full level, skip the first two actors
//						bShouldSkipImportSpecialActors = true;
//					}
//				}
//
//				// Get property text.
//				FString PropText, PropertyLine;
//				while
//					(GetEND(&Buffer, TEXT("ACTOR")) == 0
//						&& FParse::Line(&Buffer, PropertyLine))
//				{
//					PropText += *PropertyLine;
//					PropText += TEXT("\r\n");
//				}
//
//				if (!(bShouldSkipImportSpecialActors && ActorIndex < 2))
//				{
//					// Don't import the default physics volume, as it doesn't have a UModel associated with it
//					// and thus will not import properly.
//					if (!TempClass->IsChildOf(ADefaultPhysicsVolume::StaticClass()))
//					{
//						// Create a new actor.
//						FActorSpawnParameters SpawnInfo;
//						SpawnInfo.Name = ActorUniqueName;
//						SpawnInfo.Template = Archetype;
//						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
//						AActor* NewActor = World->SpawnActor(TempClass, nullptr, nullptr, SpawnInfo);
//
//						if (NewActor)
//						{
//							// Store the new actor and the text it should be initialized with.
//							NewActorMap.Add(NewActor, *PropText);
//
//							// Store the copy to original actor mapping
//							MapActors.Add(NewActor, Found);
//
//							// Store the new actor against its source actor name (not the one that may have been made unique)
//							if (ActorSourceName != NAME_None)
//							{
//								NewActorsFNames.Add(ActorSourceName, NewActor);
//								if (Found)
//								{
//									ExistingToNewMap.Add(Found, NewActor);
//								}
//							}
//
//							// Store the new actor with its parent's FName, and socket FName if applicable
//							if (ActorParentName != NAME_None)
//							{
//								NewActorsAttachmentMap.Add(NewActor, FAttachmentDetail(ActorParentName, ActorParentSocket));
//							}
//						}
//					}
//				}
//
//				// increment the number of actors we imported
//				ActorIndex++;
//			}
//		}
//	}
//
//	if (MapPackageText.Len() > 0)
//	{
//		UImportFactory* PackageFactory = NewObject<UImportFactory>();
//		check(PackageFactory);
//
//		FName NewPackageName(*(RootMapPackage->GetName()));
//
//		const TCHAR* MapPkg_BufferStart = *MapPackageText;
//		const TCHAR* MapPkg_BufferEnd = MapPkg_BufferStart + MapPackageText.Len();
//		PackageFactory->FactoryCreateText(UPackage::StaticClass(), nullptr, NewPackageName, RF_NoFlags, 0, TEXT("T3D"), MapPkg_BufferStart, MapPkg_BufferEnd, Warn);
//	}
//
//	for (auto& ActorMapElement : NewActorMap)
//	{
//		AActor* Actor = ActorMapElement.Key;
//
//		// Import properties if the new actor is 
//		bool		bActorChanged = false;
//		FString&	PropText = ActorMapElement.Value;
//		if (Actor->ShouldImport(&PropText, bIsMoveToStreamingLevel))
//		{
//			// Actor->PreEditChange(nullptr);
//			// ImportObjectProperties((uint8*)Actor, *PropText, Actor->GetClass(), Actor, Actor, Warn, 0, INDEX_NONE, NULL, &ExistingToNewMap);
//			bActorChanged = true;
//
//			//if (GWorld == World)
//			//{
//			//	GEditor->SelectActor(Actor, true, false, true);
//			//}
//		}
//		else // This actor is new, but rejected to import its properties, so just delete...
//		{
//			Actor->Destroy();
//		}
//
//	}
//
//	// Pass 2: Sort out any attachment parenting on the new actors now that all actors have the correct properties set
//	for (auto It = MapActors.CreateIterator(); It; ++It)
//	{
//		AActor* const Actor = It.Key();
//
//		// Fixup parenting
//		FAttachmentDetail* ActorAttachmentDetail = NewActorsAttachmentMap.Find(Actor);
//		if (ActorAttachmentDetail != nullptr)
//		{
//			AActor* ActorParent = nullptr;
//			// Try to find the new copy of the parent
//			AActor** NewActorParent = NewActorsFNames.Find(ActorAttachmentDetail->ParentName);
//			if (NewActorParent != nullptr)
//			{
//				ActorParent = *NewActorParent;
//			}
//			// Try to find an already existing parent
//			if (ActorParent == nullptr)
//			{
//				ActorParent = FindObject<AActor>(World->GetCurrentLevel(), *ActorAttachmentDetail->ParentName.ToString());
//			}
//			// Parent the actors
//			if (GWorld == World && ActorParent != nullptr)
//			{
//				// Make sure our parent isn't selected (would cause GEditor->ParentActors to fail)
//				const bool bParentWasSelected = ActorParent->IsSelected();
//				//if (bParentWasSelected)
//				//{
//				//	GEditor->SelectActor(ActorParent, false, false, true);
//				//}
//
//				//GEditor->ParentActors(ActorParent, Actor, ActorAttachmentDetail->SocketName);
//
//				//if (bParentWasSelected)
//				//{
//				//	GEditor->SelectActor(ActorParent, true, false, true);
//				//}
//			}
//		}
//	}
//}
//
//	return World;
//}
