#pragma once
#include "Serialization/DuplicatedDataWriter.h"

namespace FBlueprintSupportEx
{
		/**
	 * Defined in BlueprintSupport.cpp
	 * Duplicates all fields of a class in depth-first order. It makes sure that everything contained
	 * in a class is duplicated before the class itself, as well as all function parameters before the
	 * function itself.
	 *
	 * @param	StructToDuplicate			Instance of the struct that is about to be duplicated
	 * @param	Writer						duplicate writer instance to write the duplicated data to
	 */
	void DuplicateAllFields(UStruct* StructToDuplicate, FDuplicateDataWriter& Writer)
	{
		// This is a very simple fake topological-sort to make sure everything contained in the class
		// is processed before the class itself is, and each function parameter is processed before the function
		if (StructToDuplicate)
		{
			// Make sure each field gets allocated into the array
			for (TFieldIterator<UField> FieldIt(StructToDuplicate, EFieldIteratorFlags::ExcludeSuper); FieldIt; ++FieldIt)
			{
				UField* Field = *FieldIt;

				// Make sure functions also do their parameters and children first
				if (UFunction* Function = dynamic_cast<UFunction*>(Field))
				{
					for (TFieldIterator<UField> FunctionFieldIt(Function, EFieldIteratorFlags::ExcludeSuper); FunctionFieldIt; ++FunctionFieldIt)
					{
						UField* InnerField = *FunctionFieldIt;
						Writer.GetDuplicatedObject(InnerField);
					}
				}

				Writer.GetDuplicatedObject(Field);
			}
		}
	}
};