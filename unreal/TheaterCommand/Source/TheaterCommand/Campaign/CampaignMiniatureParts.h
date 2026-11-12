#pragma once

#include "CoreMinimal.h"

struct FMiniatureMesh;

/** Shared by the files that build the miniatures. */
namespace MiniatureParts
{
	/** A point of a figure, set out from where the figure stands and turned with it. */
	struct FPlacing
	{
		FVector Where;
		double Scale;
		double Cos, Sin;

		FPlacing(const FVector& InWhere, double InScale, double TurnDegrees)
			: Where(InWhere), Scale(InScale), Cos(FMath::Cos(FMath::DegreesToRadians(TurnDegrees))), Sin(FMath::Sin(FMath::DegreesToRadians(TurnDegrees)))
		{
		}

		/** Forward is toward the figure's front, Right to its right, Up above the ground. */
		FVector At(double Forward, double Right, double Up) const
		{
			// A figure that is not turned faces along -X, and so has -Y on its right hand.
			const double X = -Forward, Y = -Right;
			return Where + FVector(X * Cos - Y * Sin, X * Sin + Y * Cos, Up) * Scale;
		}

		/** A direction given the same way, as a unit vector. */
		FVector Toward(double Forward, double Right, double Up) const
		{
			const double X = -Forward, Y = -Right;
			return FVector(X * Cos - Y * Sin, X * Sin + Y * Cos, Up).GetSafeNormal();
		}
	};

	/** A model that can be asked for by name. */
	struct FModel
	{
		const TCHAR* Name;
		void (*Build)(struct ::FMiniatureMesh&);
		/** A line about it, with its real measurements; and whose it is. */
		const TCHAR* Title;
		const TCHAR* Nation;
		/** Whether it is built in the paint of whichever nation asks ("Truck:SOV"). */
		bool bAnyNation = false;
		/** How much room it needs in a row, in metres; and how high it flies, for an aircraft (0 for anything that stands on the ground). */
		double Room = 12.0;
		double Aloft = 0.0;
	};

	/** The nation a model that is the same for several nations is being built for ("Truck:SOV", "Bomber:ITA"). */
	FString& NationWanted();

	/** The aircraft (CampaignAircraft.cpp). */
	TConstArrayView<FModel> AircraftModels();
}
