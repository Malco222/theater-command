#include "Campaign/CampaignMapActor.h"

#include "Campaign/CampaignMiniatures.h"
#include "Campaign/StreetLife.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

/*
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 2, street cars. Brian, 25 November
 * 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks". The reckoning is
 * Campaign/StreetLife.h, with its cloud test in Tools/street_life.)
 *
 * A city on the table has its square and its two avenues, and nothing moved in them. With "tc.Map.StreetLife 1" a
 * street car works each arm of the avenues of the cities near the eye: she stands at her stop by the square, pulls
 * away, runs out to the end of the arm, stands at her terminus and comes back. She is the same at both ends and never
 * turns. An avenue is one car wide and the great church or the capital's own building stands on its line in the
 * square, so no car crosses the square: each arm is a line of its own, and nothing can meet.
 *
 * Where they are and are not:
 *   - only in a city or a capital (the town's builder says where each arm's cobble is laid unbroken from the square:
 *     FMiniatureMesh::Township, OutAvenues), and not in one packed into a small province;
 *   - only near what the eye looks at and only when it is low, for a car is small; never under the haze;
 *   - not in a city under siege: its cars are in their sheds.
 * A car grows onto her street and off it over most of a second, as a convoy does on its road, and never jumps. She
 * follows the lie of the ground.
 *
 * Decoration only. No rule reads it, nothing of it is saved, and it cannot be picked up: the pieces of the game are as
 * they were. Off as it comes, until Brian has seen the Mac's film of it; "-TheaterStreetLife" turns it on for that film.
 */

namespace
{
	TAutoConsoleVariable<int32> CVarStreetLife(
		TEXT("tc.Map.StreetLife"), 0,
		TEXT("1: a street car works each avenue arm of the cities near the eye, when it is low (from her stop by the square to the arm's end and back; none under the haze or in a city under siege); 0: the streets as before. Release 2; off as it comes."));
	TAutoConsoleVariable<int32> CVarStreetLifeMost(
		TEXT("tc.Map.StreetLifeMost"), 16,
		TEXT("With tc.Map.StreetLife: the most street cars on the streets at once, 0 to 32."));

	bool StreetLifeWanted()
	{
		static const bool bStreetLifeAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterStreetLife"));
		return bStreetLifeAsked || CVarStreetLife.GetValueOnGameThread() != 0;
	}

	/** Above this the cars are put away: from the working height (2,600) one is about eleven pixels long, and from here under seven. */
	const float StreetCarsBelow = 4200.0f;
	/** Seconds a car takes to grow onto her street, or off it. */
	const double StreetCarGrows = 0.8;
	/** How far over the cobbles her floor rides, in the table's units: clear of a length of cobble laid level on sloping ground. */
	const double StreetCarRides = 0.45;

	/**
	 * The car, her length along +X and her wheels at the model's foot: an underframe, a body in her line's colour
	 * below the windows and cream above, a dark band of windows down each side, a grey roof with a raised lantern, a
	 * bow collector over her middle and a lamp at each end. The same at both ends. No number and no lettering.
	 */
	void StreetCarModel(FMiniatureMesh& Model, double U, int32 Livery)
	{
		static const TCHAR* const StreetCarPaints[4] = { TEXT("8A2F2A"), TEXT("2F5D45"), TEXT("2F4E7A"), TEXT("B8922E") };
		const FLinearColor Panel(FColor::FromHex(StreetCarPaints[FMath::Clamp(Livery, 0, 3)])), Cream(FColor::FromHex(TEXT("E6DCC4")));
		const FLinearColor Glass(FColor::FromHex(TEXT("2E3538"))), RoofGrey(FColor::FromHex(TEXT("5E5A58"))), Iron(FColor::FromHex(TEXT("2B2A28")));
		const FLinearColor Lamp(FColor::FromHex(TEXT("F2E6B8"))), Lantern(FColor::FromHex(TEXT("767270")));
		const double Long = TheaterStreetLife::CarLong * U, Wide = TheaterStreetLife::CarWide * U, High = TheaterStreetLife::CarHigh * U;
		Model.Box(FVector(0.0, 0.0, 0.07 * High), FVector(0.84 * Long, 0.8 * Wide, 0.14 * High), Iron);
		Model.Box(FVector(0.0, 0.0, 0.34 * High), FVector(Long, Wide, 0.4 * High), Panel);
		Model.Box(FVector(0.0, 0.0, 0.7 * High), FVector(Long, Wide, 0.32 * High), Cream);
		Model.Box(FVector(0.0, 0.0, 0.7 * High), FVector(0.86 * Long, 1.03 * Wide, 0.2 * High), Glass);
		Model.Box(FVector(0.0, 0.0, 0.7 * High), FVector(1.01 * Long, 0.6 * Wide, 0.2 * High), Glass);
		Model.Box(FVector(0.0, 0.0, 0.89 * High), FVector(0.98 * Long, 0.94 * Wide, 0.06 * High), RoofGrey);
		Model.Box(FVector(0.0, 0.0, 0.95 * High), FVector(0.7 * Long, 0.5 * Wide, 0.06 * High), Lantern);
		for (const double End : { -1.0, 1.0 })
		{
			Model.Box(FVector(End * 0.505 * Long, 0.0, 0.42 * High), FVector(0.012 * Long, 0.22 * Wide, 0.08 * High), Lamp);
			Model.Rod(FVector(End * 0.1 * Long, 0.0, 0.98 * High), FVector(0.0, 0.0, 1.42 * High), 0.012 * Long, Iron, 4);
		}
		Model.Rod(FVector(0.0, -0.36 * Wide, 1.42 * High), FVector(0.0, 0.36 * Wide, 1.42 * High), 0.012 * Long, Iron, 4);
	}
}

void ACampaignMapActor::ParkStreetCars()
{
	for (FStreetLine& Each : StreetLines)
	{
		if (Each.Car)
		{
			Each.Car->DestroyComponent();
			Each.Car = nullptr;
		}
		Each.bManned = false;
		Each.Shown = 0.0f;
	}
	StreetCarsOut = 0;
}

void ACampaignMapActor::UpdateStreetLife(const FVector& CameraFocus, float CameraDistance)
{
	UWorld* World = GetWorld();
	if (!StreetLifeWanted() || !World || !Data.IsLoaded())
	{
		if (StreetCarsOut > 0)
		{
			ParkStreetCars();
		}
		return;
	}
	THEATER_SPEED_SCOPE(UpdateStreetLife);
	if (bStreetLinesStale)
	{
		bStreetLinesStale = false;
		// The towns were built again. They are built again for many reasons: where every avenue arm is where it was,
		// the lines stand and no car is disturbed. Otherwise the lines are laid afresh.
		if (!TownAvenues.OrderIndependentCompareEqual(StreetAvenuesLaid))
		{
			ParkStreetCars();
			StreetAvenuesLaid = TownAvenues;
			StreetLines.Reset();
			int32 Cities = 0;
			for (const FCampaignRegion& Region : Data.Regions)
			{
				const TArray<FVector2D>* const Arms = TownAvenues.Find(Region.Id);
				if (!Arms)
				{
					continue;
				}
				int32 Laid = 0;
				for (int32 Arm = 0; Arm + 1 < Arms->Num(); Arm += 2)
				{
					const FVector2D& From = (*Arms)[Arm];
					const FVector2D& To = (*Arms)[Arm + 1];
					const TheaterStreetLife::FLine Line = TheaterStreetLife::Lay(TheaterStreetLife::FSpot{ From.X, From.Y }, TheaterStreetLife::FSpot{ To.X, To.Y }, Region.Id * 4 + Arm / 2);
					if (Line.bOpen)
					{
						FStreetLine& Kept = StreetLines.AddDefaulted_GetRef();
						Kept.Region = Region.Id;
						Kept.Laid = Line;
						++Laid;
					}
				}
				Cities += Laid > 0 ? 1 : 0;
			}
			UE_LOG(LogTheater, Display, TEXT("Street life (tc.Map.StreetLife): %d street car lines in %d cities"), StreetLines.Num(), Cities);
		}
	}
	const double Now = World->GetTimeSeconds();
	const double U = UnitScale;
	const FVector2D Looking = WorldToMap(CameraFocus);
	const bool bLow = CameraDistance < StreetCarsBelow;
	const double Reach = FMath::Clamp(0.5 * CameraDistance / U, 30.0, 120.0);
	const int32 Most = FMath::Clamp(CVarStreetLifeMost.GetValueOnGameThread(), 0, 32);
	const float Grown = static_cast<float>(World->GetDeltaSeconds() / StreetCarGrows);
	// (seam-4, as the trains: a city a little way over the chart's seam is near the eye too, and its cars are shown in
	// the turn of the world nearest the eye. Only while both sides are drawn.)
	const bool bBothSides = OverSeam();
	for (int32 Index = 0; Index < StreetLines.Num(); ++Index)
	{
		FStreetLine& Line = StreetLines[Index];
		const double SeenX = bBothSides ? NearX(Line.Laid.From.X, Looking.X) : Line.Laid.From.X;
		const bool bWanted = bLow && FVector2D::DistSquared(FVector2D(SeenX, Line.Laid.From.Y), Looking) < Reach * Reach
			&& !HazeSet.Contains(Line.Region) && !ShownSieges.Contains(Line.Region);
		if (bWanted && !Line.bManned && StreetCarsOut < Most)
		{
			FMiniatureMesh Model;
			StreetCarModel(Model, U, (Line.Region + Index) % 4);
			Line.Car = NewObject<UProceduralMeshComponent>(this);
			Line.Car->SetupAttachment(Root);
			Line.Car->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Line.Car->SetCastShadow(false); // (a thing that moves and throws a shadow has every shadow near it drawn again each frame)
			Line.Car->RegisterComponent();
			Line.Car->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			Line.Car->SetMaterial(0, SceneryMaterial);
			Line.Car->SetVisibility(false); // (until her first place is set, below)
			Line.bManned = true;
			Line.Shown = 0.0f;
			++StreetCarsOut;
		}
		if (!Line.bManned || !Line.Car)
		{
			continue;
		}
		Line.Shown = FMath::Clamp(Line.Shown + (bWanted ? Grown : -Grown), 0.0f, 1.0f);
		if (!bWanted && Line.Shown <= 0.0f)
		{
			Line.Car->DestroyComponent();
			Line.Car = nullptr;
			Line.bManned = false;
			--StreetCarsOut;
			continue;
		}
		// Where the clock says she is, standing on the ground at both her ends.
		const TheaterStreetLife::FCar Her = TheaterStreetLife::Where(Line.Laid, Now);
		const double Shift = SeenX - Line.Laid.From.X; // (a whole number of worlds east or west, or nothing)
		const double HalfLong = 0.5 * TheaterStreetLife::CarLong;
		const FVector Fore = SurfacePoint(Her.At.X + Her.Along.X * HalfLong + Shift, Her.At.Y + Her.Along.Y * HalfLong, 0.012);
		const FVector Aft = SurfacePoint(Her.At.X - Her.Along.X * HalfLong + Shift, Her.At.Y - Her.Along.Y * HalfLong, 0.012);
		const double Climb = FMath::RadiansToDegrees(FMath::Atan2(Fore.Z - Aft.Z, FMath::Max(FVector::Dist2D(Fore, Aft), 1.0)));
		const FRotator Lies(Climb, FMath::RadiansToDegrees(FMath::Atan2(Her.Along.X, -Her.Along.Y)), 0.0);
		Line.Car->SetRelativeLocationAndRotation((Fore + Aft) * 0.5 + FVector(0.0, 0.0, StreetCarRides), Lies);
		Line.Car->SetRelativeScale3D(FVector(FMath::Max(static_cast<double>(Line.Shown), 0.02)));
		Line.Car->SetVisibility(Line.Shown > 0.02f);
	}
}
