#include "ImportPackageFactory.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "ParamParser.hpp"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

/*-----------------------------------------------------------------------------
	UImportPackageFactory.
-----------------------------------------------------------------------------*/
UImportPackageFactory::UImportPackageFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPackage::StaticClass();
	Formats.Add(TEXT("T3DPKG;Unreal Package"));

	bCreateNew = false;
	bText = true;
	bEditorImport = false;
}

UObject* UImportPackageFactory::FactoryCreateText(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	// GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);

	bool bSavedImportingT3D = GIsImportingT3D;
	// Mark us as importing a T3D.
	// GEditor->IsImportingT3D = true;
	GIsImportingT3D = true;

	if (InParent != nullptr)
	{
		return nullptr;
	}

	TMap<FString, UPackage*> MapPackages;
	bool bImportingMapPackage = false;

	UPackage* TopLevelPackage = nullptr;
	UPackage* RootMapPackage = nullptr;
	UWorld* World = GWorld;
	if (World)
	{
		RootMapPackage = World->GetOutermost();
	}

	if (RootMapPackage)
	{
		if (RootMapPackage->GetName() == Name.ToString())
		{
			// Loading into the Map package!
			MapPackages.Add(RootMapPackage->GetName(), RootMapPackage);
			TopLevelPackage = RootMapPackage;
			bImportingMapPackage = true;
		}
	}

	//// Unselect all actors.
	//GEditor->SelectNone(false, false);

	//// Mark us importing a T3D (only from a file, not from copy/paste).
	//GEditor->IsImportingT3D = FCString::Stricmp(Type, TEXT("paste")) != 0;
	//GIsImportingT3D = GEditor->IsImportingT3D;

	// Maintain a list of a new package objects and the text they were created from.
	TMap<UObject*, FString> NewPackageObjectMap;

	FString StrLine;
	while (FParse::Line(&Buffer, StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetBEGIN(&Str, TEXT("TOPLEVELPACKAGE")) && !bImportingMapPackage)
		{
			//Begin TopLevelPackage Class=Package Name=ExportTest_ORIG Archetype=Package'Core.Default__Package'
			UClass* TempClass;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE))
			{
				// Get actor name.
				FName PackageName(NAME_None);
				FParse::Value(Str, TEXT("NAME="), PackageName);

				if (FindObject<UPackage>(ANY_PACKAGE, *(PackageName.ToString())))
				{
					UE_LOG(LogTemp, Warning, TEXT("Package factory can only handle the map package or new packages!"));
					return nullptr;
				}
				TopLevelPackage = CreatePackage(nullptr, *(PackageName.ToString()));
				TopLevelPackage->SetFlags(RF_Standalone | RF_Public);
				MapPackages.Add(TopLevelPackage->GetName(), TopLevelPackage);

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				AActor* Archetype = nullptr;
				if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
				{
				}
			}
		}
		else if (GetBEGIN(&Str, TEXT("PACKAGE")))
		{
			FString ParentPackageName;
			FParse::Value(Str, TEXT("PARENTPACKAGE="), ParentPackageName);
			UClass* PkgClass;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), PkgClass, ANY_PACKAGE))
			{
				// Get the name of the object.
				FName NewPackageName(NAME_None);
				FParse::Value(Str, TEXT("NAME="), NewPackageName);

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				FString ArchetypeName;
				UPackage* Archetype = nullptr;
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
							if (ArchetypeClass->IsChildOf(UPackage::StaticClass()))
							{
								// if we had the class, find the archetype
								Archetype = Cast<UPackage>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
							}
							else
							{
								// Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Package"),Str, *ObjectClass);
							}
						}
					}

					UPackage* ParentPkg = nullptr;
					UPackage** ppParentPkg = MapPackages.Find(ParentPackageName);
					if (ppParentPkg)
					{
						ParentPkg = *ppParentPkg;
					}
					check(ParentPkg);

					auto NewPackage = NewObject<UPackage>(ParentPkg, NewPackageName, RF_NoFlags, Archetype);
					check(NewPackage);
					NewPackage->SetFlags(RF_Standalone | RF_Public);
					MapPackages.Add(NewPackageName.ToString(), NewPackage);
				}
			}
		}
	}

	for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		UObject* LoadObject = *ObjIt;

		if (LoadObject)
		{
			bool bModifiedObject = false;

			FString* PropText = NewPackageObjectMap.Find(LoadObject);
			if (PropText)
			{
				LoadObject->PreEditChange(nullptr);
				// ImportObjectProperties((uint8*)LoadObject, **PropText, LoadObject->GetClass(), LoadObject, LoadObject, Warn, 0);
				bModifiedObject = true;
			}

			if (bModifiedObject)
			{
				// Let the actor deal with having been imported, if desired.
				LoadObject->PostEditImport();
				// Notify actor its properties have changed.
				LoadObject->PostEditChange();
				LoadObject->SetFlags(RF_Standalone | RF_Public);
				LoadObject->MarkPackageDirty();
			}
		}
	}

	//// Mark us as no longer importing a T3D.
	//GEditor->IsImportingT3D = bSavedImportingT3D;
	//GIsImportingT3D = bSavedImportingT3D;

	//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, TopLevelPackage);

	return TopLevelPackage;
}