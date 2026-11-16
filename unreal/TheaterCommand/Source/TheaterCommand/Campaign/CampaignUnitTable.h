#pragma once

#include "CoreMinimal.h"

/**
 * (Brian, 16 November 2026: "make generic bombers and other units for other countries that don't have unique ones
 * like those listed". The design director's rulings 78 and 79.) The one table the game draws its models from: the art
 * director's, Content/Data/Campaign/unit-table.json. A country shows its own model where it has one, a shared one
 * where the record is plain, and otherwise the generic of that kind in its own paint with its own marking: no
 * country shows another country's machine or soldier. A tag that is not in the table takes the table's default.
 * Where the file cannot be read every answer is "not known", and the game draws as it did before.
 */
namespace UnitTable
{
	/** True once the table has been read and holds something. */
	bool IsLoaded();
	/** The head a country's soldiers wear: "stahl", "brodie", "adrian", "soviet", "pot", "round", "cap", "dome", "peaked", "sun" or "fieldcap". Empty if not known. */
	FString Head(const FString& Tag);
	/** The cloth of its soldiers, the paint of its vehicles and aircraft, the grey of its ships. False if not known. */
	bool Cloth(const FString& Tag, FLinearColor& Out);
	bool Paint(const FString& Tag, FLinearColor& Out);
	bool Hull(const FString& Tag, FLinearColor& Out);
	/** For a country with no marking drawn: the ring of its flag's colours, outer, middle and centre. Empty where its marking is drawn, or not known. */
	TArray<FLinearColor> Ring(const FString& Tag);
	/** True where the country's battleship (its cruiser) is the plain hull: it has no layout of its own and shares none by the record. */
	bool PlainBattleship(const FString& Tag);
	bool PlainCruiser(const FString& Tag);
}
