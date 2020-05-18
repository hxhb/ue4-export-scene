#pragma once

#include "Paths.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
// #include "Factories/Factory.h"
#include "ImportFactory.generated.h"

/**
 * Import the entire default properties block for the class specified
 *
 * @param	Class		the class to import defaults for
 * @param	Text		buffer containing the text to be imported
 * @param	Warn		output device for log messages
 * @param	Depth		current nested subobject depth
 * @param	LineNumber	the starting line number for the defaultproperties block (used for log messages)
 *
 * @return	NULL if the default values couldn't be imported
 */

 /**
  * Parameters for ImportObjectProperties
  */
struct FImportObjectParamsEx
{
	/** the location to import the property values to */
	uint8*				DestData;

	/** pointer to a buffer containing the values that should be parsed and imported */
	const TCHAR*		SourceText;

	/** the struct for the data we're importing */
	UStruct*			ObjectStruct;

	/** the original object that ImportObjectProperties was called for.
		if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
		if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter */
	UObject*			SubobjectRoot;

	/** the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText */
	UObject*			SubobjectOuter;

	/** output device to use for log messages */
	FFeedbackContext*	Warn;

	/** current nesting level */
	int32					Depth;

	/** used when importing defaults during script compilation for tracking which line we're currently for the purposes of printing compile errors */
	int32					LineNumber;

	/** contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
		not necessary to specify a value when calling this function from other code */
	FObjectInstancingGraph* InInstanceGraph;

	/** provides a mapping from an existing actor to a new instance to which it should be remapped */
	const TMap<AActor*, AActor*>* ActorRemapper;

	/** True if we should call PreEditChange/PostEditChange on the object as it's imported.  Pass false here
		if you're going to do that on your own. */
	bool				bShouldCallEditChange;


	/** Constructor */
	FImportObjectParamsEx()
		: DestData(NULL),
		SourceText(NULL),
		ObjectStruct(NULL),
		SubobjectRoot(NULL),
		SubobjectOuter(NULL),
		Depth(0),
		LineNumber(INDEX_NONE),
		InInstanceGraph(NULL),
		ActorRemapper(NULL),
		bShouldCallEditChange(true)
	{
	}
};

class FDefaultPropertiesContextSupplierEx : public FContextSupplier
{
public:
	/** the current line number */
	int32 CurrentLine;

	/** the package we're processing */
	FString PackageName;

	/** the class we're processing */
	FString ClassName;

	FString GetContext()
	{
		return FString::Printf
		(
			TEXT("%sDevelopment/Src/%s/Classes/%s.h(%i)"),
			*FPaths::RootDir(),
			*PackageName,
			*ClassName,
			CurrentLine
		);
	}

	FDefaultPropertiesContextSupplierEx() {}
	FDefaultPropertiesContextSupplierEx(const TCHAR* Package, const TCHAR* Class, int32 StartingLine)
		: CurrentLine(StartingLine), PackageName(Package), ClassName(Class)
	{
	}

};

static FDefaultPropertiesContextSupplierEx* ContextSupplier = NULL;


UCLASS(MinimalAPI)
class UImportFactory :public UObject//  : public UFactory
{
	GENERATED_BODY()
public:
	UImportFactory(const FObjectInitializer& ObjectInitializer);
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateText(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd);
	//~ Begin UFactory Interface


	/**
	 * Parse and import text as property values for the object specified.
	 *
	 * @param	InParams	Parameters for object import; see declaration of FImportObjectParams.
	 *
	 * @return	NULL if the default values couldn't be imported
	 */
	static const TCHAR* ImportObjectProperties(FImportObjectParamsEx& InParams);

	/**
	 * Parse and import text as property values for the object specified.
	 *
	 * @param	DestData			the location to import the property values to
	 * @param	SourceText			pointer to a buffer containing the values that should be parsed and imported
	 * @param	ObjectStruct		the struct for the data we're importing
	 * @param	SubobjectRoot		the original object that ImportObjectProperties was called for.
	 *								if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
	 *								if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter
	 * @param	SubobjectOuter		the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText
	 * @param	Depth				current nesting level
	 * @param	LineNumber			used when importing defaults during script compilation for tracking which line the defaultproperties block begins on
	 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
	 *								not necessary to specify a value when calling this function from other code
	 * @param	ActorRemapper		used when duplicating actors to remap references from a source actor to the duplicated actor
	 *
	 * @return	NULL if the default values couldn't be imported
	 */
	static const TCHAR* ImportObjectProperties(
		uint8*				DestData,
		const TCHAR*		SourceText,
		UStruct*			ObjectStruct,
		UObject*			SubobjectRoot,
		UObject*			SubobjectOuter,
		int32					Depth,
		int32					LineNumber = INDEX_NONE,
		FObjectInstancingGraph* InstanceGraph = NULL,
		const TMap<AActor*, AActor*>* ActorRemapper = NULL
	);

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
	static const TCHAR* ImportProperties(
		uint8*						DestData,
		const TCHAR*				SourceText,
		UStruct*					ObjectStruct,
		UObject*					SubobjectRoot,
		UObject*					SubobjectOuter,
		int32						Depth,
		FObjectInstancingGraph&		InstanceGraph,
		const TMap<AActor*, AActor*>* ActorRemapper
	);
};
