// The groups' earned colours on the scene's own shapes (release 2; Brian, 25 November 2026 03:27Z: "earned nicknames as
// they complete missions. i also want them to get unique colorings or markings like the tuskegee airmen"). Included by
// AirBattle.cpp alone: the shapes (AirParts::FShape and the map that keeps them) are its own, so this handles them as
// whatever it is given, and everything else is in AirBattleUnitColours.cpp and Battle/AirUnitColours.
//
// A coloured shape is its model's shape copied whole (its bay, its airscrews, its kill slots and its frame with it) and
// the look laid on the copy's points and triangles; kept beside the plain one under the plain one's key and the look's
// ("Bf109:GER|colours GER 3 t2 day"), its far shape under that and "|far", so that every aircraft of a group, and of any
// group with the same look on the same model, draws and is copied as one. Off, or for an aircraft with no look, it
// returns the plain key at once and the aircraft is made exactly as before.

#pragma once

#include "ProceduralMeshComponent.h"

template <typename TShapes>
FString AAirBattle::UnitColoursShape(int32 Key, UProceduralMeshComponent* Mesh, const FString& Shape, TShapes& Shapes)
{
	bool bFirst = false;
	if (!UnitColoursSwitch(bFirst))
	{
		return Shape;
	}
	if (bFirst)
	{
		// (the shapes coloured for an earlier battle let go: this one's looks are its own)
		TArray<FString> Old;
		for (const auto& Pair : Shapes)
		{
			if (Pair.Key.Contains(TEXT("|colours ")))
			{
				Old.Add(Pair.Key);
			}
		}
		for (const FString& Each : Old)
		{
			Shapes.Remove(Each);
		}
	}
	const auto* Plain = Shapes.Find(Shape);
	FString Look;
	if (!Plain || Plain->Kind.IsEmpty() || !UnitColoursFor(Key, Plain->Kind, Look))
	{
		return Shape;
	}
	const FString Coated = Shape + TEXT("|colours ") + Look;
	if (!Shapes.Contains(Coated))
	{
		// (copies first, then kept: adding to the map may move what is in it)
		auto Near = *Plain;
		if (!UnitColoursLay(false, Near.Middle, Near.Fit, Near.Points, Near.Triangles, Near.Normals, Near.Colors))
		{
			return Shape;
		}
		const auto* PlainFar = Shapes.Find(Shape + TEXT("|far"));
		if (PlainFar)
		{
			auto Far = *PlainFar;
			// (a far shape that does not take the look is left out, and the near one is drawn at every distance: never a
			// far shape in plain paint under a coloured near one)
			if (UnitColoursLay(true, Near.Middle, Near.Fit, Far.Points, Far.Triangles, Far.Normals, Far.Colors))
			{
				Shapes.Add(Coated + TEXT("|far"), MoveTemp(Far));
			}
		}
		Shapes.Add(Coated, MoveTemp(Near));
	}
	// Its mesh's first section made again, as PlaneMesh made it, from the coloured shape (the far one where there is one).
	const auto* Far = Shapes.Find(Coated + TEXT("|far"));
	const auto* First = Far ? Far : Shapes.Find(Coated);
	if (Mesh && First && First->Points.Num() > 0)
	{
		Mesh->CreateMeshSection(0, First->Points, First->Triangles, First->Normals, TArray<FVector2D>(), First->Colors, TArray<FProcMeshTangent>(), false);
	}
	return Coated;
}
