#include "Campaign/CampaignMapActor.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/PortLife.h"
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
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 1, busy ports. Brian, 25 November
 * 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks". The reckoning is
 * Campaign/PortLife.h, with its cloud test in Tools/port_life.)
 *
 * A port on the table has its quay, its cranes, its lighthouse and two ships alongside, and none of it moved. With
 * "tc.Map.PortLife 1" a small merchant ship works each port near the eye, and two a city's or a capital's: she lies
 * off the quay with her head to sea, weighs, runs down the fairway, turns in the offing, comes back up the other side
 * of it, slows up the harbour, rounds up and lies again. Her wake is laid each frame along the water she has crossed,
 * and fades as the way comes off her. In a country at war she is painted grey.
 *
 * Where they are and are not:
 *   - only at a port whose town has a quay, and only where the fairway has water and room all the way round (found
 *     once after the towns are built, one port a frame; the greater ports first, and a port whose fairway would cross
 *     a greater one's has none);
 *   - only near what the eye looks at, as the trains and the lorry convoys are, and never under the haze;
 *   - never where a fleet lies on the fairway: the little ships are put away until she has gone.
 * A port's ships grow onto the water and off it over most of a second, as a convoy does on its road, and never jump.
 *
 * Decoration only. No rule reads it, nothing of it is saved, and it cannot be picked up: the pieces of the game are as
 * they were. Off as it comes, until Brian has seen the Mac's film of it; "-TheaterPortLife" turns it on for that film.
 */

namespace
{
	TAutoConsoleVariable<int32> CVarPortLife(
		TEXT("tc.Map.PortLife"), 0,
		TEXT("1: small merchant ships come and go at the ports near the eye (out along the fairway, round in the offing, back and round up off the quay; a wake along the water crossed; none under the haze or where a fleet lies); 0: the ports as before. Release 2; off as it comes."));
	TAutoConsoleVariable<int32> CVarPortLifeMost(
		TEXT("tc.Map.PortLifeMost"), 12,
		TEXT("With tc.Map.PortLife: the most little ships on the water at once, 0 to 24."));

	bool PortLifeWanted()
	{
		static const bool bPortLifeAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterPortLife"));
		return bPortLifeAsked || CVarPortLife.GetValueOnGameThread() != 0;
	}

	/** One of them, in the map's units: two thirds as long as the ships that lie alongside a quay. */
	const double PortShipLong = 2.7, PortShipBeam = 0.21 * PortShipLong;
	/** Two ports whose rounds come nearer than this: only the greater is worked. A fleet lying nearer a round than the second puts its ships away. */
	const double PortFairwayRoom = 5.0, PortFleetRoom = 8.0;
	/** Seconds a port's ships take to grow onto the water, or off it. */
	const double PortShipsGrow = 0.8;

	/**
	 * The little ship, her bows toward +X and her keel on the water at the middle of the model: a hull with a raised
	 * forecastle and poop, two hatches, a house and a funnel aft of amidships, two masts with their derricks. Three
	 * liveries in peace (black with a red funnel, dark grey with a buff one, red-brown with a black one) and one in a
	 * country at war: grey all over. No flag and no marking.
	 */
	void PortShipModel(FMiniatureMesh& Model, double U, int32 Livery)
	{
		static const TCHAR* const HullPaints[4] = { TEXT("2B2D31"), TEXT("3A3F45"), TEXT("5A2F26"), TEXT("6E747A") };
		static const TCHAR* const FunnelPaints[4] = { TEXT("9A3A2E"), TEXT("C9A86A"), TEXT("23252A"), TEXT("6A7076") };
		static const TCHAR* const HousePaints[4] = { TEXT("EFE9DC"), TEXT("E6DCC4"), TEXT("EFE9DC"), TEXT("7C8288") };
		const int32 Kind = FMath::Clamp(Livery, 0, 3);
		const FLinearColor HullPaint(FColor::FromHex(HullPaints[Kind])), FunnelPaint(FColor::FromHex(FunnelPaints[Kind])), HousePaint(FColor::FromHex(HousePaints[Kind]));
		const FLinearColor Planks(FColor::FromHex(TEXT("8E7B5A"))), Tarpaulin(FColor::FromHex(Kind == 3 ? TEXT("5C6268") : TEXT("4A4F45")));
		const FLinearColor Spar(FColor::FromHex(Kind == 3 ? TEXT("6E747A") : TEXT("6B5236"))), Soot(FColor::FromHex(TEXT("23252A")));
		const FLinearColor BoatPaint(HousePaint.R * 0.9f, HousePaint.G * 0.9f, HousePaint.B * 0.9f); // (her two boats, a shade under her house)
		const double Long = PortShipLong * U, Beam = 0.21;
		const FVector Keel = FVector::ZeroVector;
		auto Plan = [](double Wide) -> TArray<FVector2D>
		{
			return { { 0.5, 0.0 }, { 0.3, Wide * 0.42 }, { 0.05, Wide * 0.5 }, { -0.38, Wide * 0.46 }, { -0.5, Wide * 0.3 },
				{ -0.5, -Wide * 0.3 }, { -0.38, -Wide * 0.46 }, { 0.05, -Wide * 0.5 }, { 0.3, -Wide * 0.42 } };
		};
		Model.Slab(Keel, Long, 0.0, Plan(Beam), 0.0, 0.07, HullPaint);
		Model.Slab(Keel, Long, 0.0, Plan(Beam * 0.86), 0.07, 0.078, Planks);
		// (her forecastle and her poop, a little higher than her well deck)
		Model.Slab(Keel, Long, 0.0, { { 0.5, 0.0 }, { 0.3, Beam * 0.42 }, { 0.24, Beam * 0.45 }, { 0.24, -Beam * 0.45 }, { 0.3, -Beam * 0.42 } }, 0.07, 0.1, HullPaint);
		Model.Slab(Keel, Long, 0.0, { { -0.36, Beam * 0.46 }, { -0.5, Beam * 0.3 }, { -0.5, -Beam * 0.3 }, { -0.36, -Beam * 0.46 } }, 0.07, 0.095, HullPaint);
		for (const double Hatch : { 0.16, 0.0 })
		{
			Model.Box(FVector(Hatch * Long, 0.0, 0.086 * Long), FVector(0.11, 0.09, 0.016) * Long, Tarpaulin);
		}
		Model.Box(FVector(-0.19 * Long, 0.0, 0.113 * Long), FVector(0.2, 0.15, 0.07) * Long, HousePaint);
		Model.Box(FVector(-0.12 * Long, 0.0, 0.166 * Long), FVector(0.06, 0.17, 0.036) * Long, HousePaint);
		Model.Pillar(FVector(-0.22 * Long, 0.0, 0.148 * Long), 0.03 * Long, 0.026 * Long, 0.1 * Long, FunnelPaint, 8);
		if (Kind < 2)
		{
			Model.Pillar(FVector(-0.22 * Long, 0.0, 0.248 * Long), 0.027 * Long, 0.027 * Long, 0.02 * Long, Soot, 8);
		}
		for (const double Side : { -1.0, 1.0 })
		{
			Model.Box(FVector(-0.25 * Long, Side * 0.055 * Long, 0.158 * Long), FVector(0.07, 0.022, 0.02) * Long, BoatPaint);
		}
		for (const double Mast : { 0.08, -0.33 })
		{
			const FVector Foot(Mast * Long, 0.0, 0.078 * Long);
			Model.Pillar(Foot, 0.008 * Long, 0.005 * Long, 0.21 * Long, Spar, 5);
			Model.Rod(Foot + FVector(0.0, 0.0, 0.04 * Long), Foot + FVector(0.09 * Long, 0.0, 0.1 * Long), 0.005 * Long, Spar, 4);
			if (Mast > 0.0)
			{
				Model.Rod(Foot + FVector(0.0, 0.0, 0.04 * Long), Foot + FVector(-0.07 * Long, 0.0, 0.1 * Long), 0.005 * Long, Spar, 4);
			}
		}
	}

	/** Her wake as LayWake lays one for a ship at the middle heading +X, in the map's units: laid along her course each frame. */
	const FMiniatureMesh::FWakeMesh& PortWakePattern()
	{
		static const FMiniatureMesh::FWakeMesh Pattern = []()
		{
			FMiniatureMesh::FWakeMesh Laid;
			FMiniatureMesh::LayWake(Laid, FVector::ZeroVector, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), PortShipLong, PortShipBeam, 0.6, -1.0, -1.0);
			return Laid;
		}();
		return Pattern;
	}
}

void ACampaignMapActor::ParkPortCraft(int32 Roads)
{
	for (int32 Index = PortCraft.Num() - 1; Index >= 0; --Index)
	{
		if (Roads == INDEX_NONE || PortCraft[Index].Roads == Roads)
		{
			if (PortCraft[Index].Hull)
			{
				PortCraft[Index].Hull->DestroyComponent();
			}
			if (PortCraft[Index].Foam)
			{
				PortCraft[Index].Foam->DestroyComponent();
			}
			PortCraft.RemoveAt(Index);
		}
	}
	if (PortRoads.IsValidIndex(Roads))
	{
		PortRoads[Roads].bManned = false;
		PortRoads[Roads].Shown = 0.0f;
	}
	else if (Roads == INDEX_NONE)
	{
		for (FPortRoads& Each : PortRoads)
		{
			Each.bManned = false;
			Each.Shown = 0.0f;
		}
	}
}

void ACampaignMapActor::FindPortRoads()
{
	if (bPortRoadsStale)
	{
		bPortRoadsStale = false;
		// The towns were built again. They are built again for many reasons (a works wrecked, a public building going
		// up): where every quay stands where it stood, the fairways stand too and no ship is disturbed. Otherwise every
		// port's fairway is looked for afresh, the greater ports first.
		if (!TownQuays.OrderIndependentCompareEqual(PortQuaysFound))
		{
			PortQuaysFound = TownQuays;
			ParkPortCraft(INDEX_NONE);
			PortRoads.Reset();
			PortsToFind.Reset();
			PortsNext = 0;
			PortsDry = 0;
			PortsCrowded = 0;
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (const FCampaignRegion& Region : Data.Regions)
				{
					const bool bGreat = Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2 || Region.Factories >= 2;
					if (bGreat == (Pass == 0) && TownQuays.Contains(Region.Id))
					{
						PortsToFind.Add(Region.Id);
					}
				}
			}
		}
	}
	if (PortsNext >= PortsToFind.Num())
	{
		return;
	}
	// One port a frame: looking for water all round a fairway, on every bearing it may take, is some thousands of looks
	// at the ground where a coast is close.
	const int32 RegionId = PortsToFind[PortsNext++];
	const FVector2D* const Quay = TownQuays.Find(RegionId);
	if (Quay && Data.Regions.IsValidIndex(RegionId))
	{
		const FCampaignRegion& Region = Data.Regions[RegionId];
		const FVector2D Seaward = *Quay - Region.Center;
		const double Bearing = FMath::RadiansToDegrees(FMath::Atan2(Seaward.X, -Seaward.Y));
		// (water: the sea, inside the chart's frame)
		auto Wet = [this](double X, double Y)
		{
			return Y > 2.0 && Y < Data.Height - 2.0 && (Data.bWorld || (X > 4.0 && X < Data.Width - 2.0)) && GroundAt(X, Y) <= 0.0f;
		};
		const TheaterPortLife::FRoads Laid = TheaterPortLife::Find(TheaterPortLife::FSpot{ Quay->X, Quay->Y }, Bearing, RegionId, Wet);
		bool bCrowded = false;
		for (const FPortRoads& Other : PortRoads)
		{
			if (Laid.bOpen && TheaterPortLife::Foul(Laid, Other.Laid, PortFairwayRoom))
			{
				bCrowded = true;
				break;
			}
		}
		if (!Laid.bOpen)
		{
			++PortsDry;
		}
		else if (bCrowded)
		{
			++PortsCrowded;
		}
		else
		{
			FPortRoads& Kept = PortRoads.AddDefaulted_GetRef();
			Kept.Region = RegionId;
			Kept.Ships = (Region.bCapital || Region.Terrain == TEXT("urban") || Region.VictoryPoints >= 2 || Region.Factories >= 2) ? 2 : 1;
			Kept.Laid = Laid;
		}
	}
	if (PortsNext >= PortsToFind.Num())
	{
		int32 Ships = 0;
		for (const FPortRoads& Each : PortRoads)
		{
			Ships += Each.Ships;
		}
		UE_LOG(LogTheater, Display, TEXT("Port life (tc.Map.PortLife): %d ports with a quay; %d have their roads, for %d little ships in all; %d have no water for a fairway; %d give way to a greater port beside them"),
			PortRoads.Num() + PortsDry + PortsCrowded, PortRoads.Num(), Ships, PortsDry, PortsCrowded);
	}
}

void ACampaignMapActor::UpdatePortLife(const FVector& CameraFocus, float CameraDistance)
{
	UWorld* World = GetWorld();
	if (!PortLifeWanted() || !World || !Data.IsLoaded())
	{
		if (PortCraft.Num() > 0)
		{
			ParkPortCraft(INDEX_NONE);
		}
		return;
	}
	THEATER_SPEED_SCOPE(UpdatePortLife);
	FindPortRoads();
	const double Now = World->GetTimeSeconds();
	const double U = UnitScale;
	const FVector2D Looking = WorldToMap(CameraFocus);
	// From far off the little ships are too small to see, and are put away, as the trains are.
	const bool bNear = CameraDistance < 9000.0f;
	const double Reach = FMath::Clamp(0.5 * CameraDistance / U, 40.0, 200.0);
	const int32 Most = FMath::Clamp(CVarPortLifeMost.GetValueOnGameThread(), 0, 24);
	const float Grown = static_cast<float>(World->GetDeltaSeconds() / PortShipsGrow);
	// (seam-4, as the trains: a port a little way over the chart's seam is near the eye too, and its ships are shown in
	// the turn of the world nearest the eye. Only while both sides are drawn.)
	const bool bBothSides = OverSeam();
	auto SeenAt = [this, bBothSides, &Looking](double MapX) { return bBothSides ? NearX(MapX, Looking.X) : MapX; };

	// The fleets, looked for on the fairways once a second.
	TArray<FVector2D, TInlineAllocator<32>> Fleets;
	const bool bLookForFleets = Now - PortFleetsLooked > 1.0 || Now < PortFleetsLooked;
	if (bLookForFleets)
	{
		PortFleetsLooked = Now;
		for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : FleetPieces)
		{
			if (Pair.Value)
			{
				Fleets.Add(WorldToMap(Pair.Value->GetActorLocation()));
			}
		}
	}

	const FMiniatureMesh::FWakeMesh& Pattern = PortWakePattern();

	// The ships go out at the ports near the eye, and are taken off the water where it has left or a fleet has come.
	for (int32 Index = 0; Index < PortRoads.Num(); ++Index)
	{
		FPortRoads& Roads = PortRoads[Index];
		const FVector2D Quay(Roads.Laid.Quay.X, Roads.Laid.Quay.Y);
		const bool bClose = bNear && FVector2D::DistSquared(FVector2D(SeenAt(Quay.X), Quay.Y), Looking) < Reach * Reach && !HazeSet.Contains(Roads.Region);
		if (bLookForFleets && (bClose || Roads.bManned))
		{
			Roads.bFouled = false;
			const double Round = Roads.Laid.Far + TheaterPortLife::Half + PortFleetRoom;
			for (const FVector2D& Fleet : Fleets)
			{
				const FVector2D Lies(bBothSides ? NearX(Fleet.X, Quay.X) : Fleet.X, Fleet.Y);
				if (FVector2D::DistSquared(Lies, Quay) < Round * Round && TheaterPortLife::Off(Roads.Laid, TheaterPortLife::FSpot{ Lies.X, Lies.Y }) < PortFleetRoom)
				{
					Roads.bFouled = true;
					break;
				}
			}
		}
		const bool bWanted = bClose && !Roads.bFouled;
		if (bWanted && !Roads.bManned && PortCraft.Num() + Roads.Ships <= Most)
		{
			// (in a country at war, which blacks out its towns by night, a merchant ship is painted grey)
			const bool bAtWar = Data.Regions.IsValidIndex(Roads.Region) && BlackoutTags.Contains(Data.Regions[Roads.Region].Owner);
			for (int32 Place = 0; Place < Roads.Ships; ++Place)
			{
				FMiniatureMesh Model;
				PortShipModel(Model, U, bAtWar ? 3 : (Roads.Region + Place) % 3);
				FPortCraft& Craft = PortCraft.AddDefaulted_GetRef();
				Craft.Roads = Index;
				Craft.Place = Place;
				Craft.Hull = NewObject<UProceduralMeshComponent>(this);
				Craft.Hull->SetupAttachment(Root);
				Craft.Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Craft.Hull->SetCastShadow(false); // (a thing that moves and throws a shadow has every shadow near it drawn again each frame)
				Craft.Hull->RegisterComponent();
				Craft.Hull->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
				Craft.Hull->SetMaterial(0, SceneryMaterial);
				Craft.Hull->SetVisibility(false); // (until her first place is set, below)
				if (WakeInk)
				{
					Craft.Foam = NewObject<UProceduralMeshComponent>(this);
					Craft.Foam->SetupAttachment(Root);
					Craft.Foam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Craft.Foam->SetCastShadow(false);
					Craft.Foam->RegisterComponent();
					Craft.Foam->CreateMeshSection(0, Pattern.Vertices, Pattern.Triangles, Pattern.Normals, Pattern.UVs, Pattern.Colors, TArray<FProcMeshTangent>(), false);
					Craft.Foam->SetMaterial(0, WakeInk);
					Craft.Foam->SetVisibility(false); // (until it is laid on her course)
				}
			}
			Roads.bManned = true;
			Roads.Shown = 0.0f;
		}
		if (Roads.bManned)
		{
			Roads.Shown = FMath::Clamp(Roads.Shown + (bWanted ? Grown : -Grown), 0.0f, 1.0f);
			if (!bWanted && Roads.Shown <= 0.0f)
			{
				ParkPortCraft(Index);
			}
		}
	}

	// Each ship where the clock says she is, and her wake along the water she has crossed.
	static const FVector2D WakeSpan = []()
	{
		// (how far ahead of her middle the pattern's foremost point lies, and how far astern its last)
		double Fore = 0.0, Aft = 0.0;
		for (const FVector& Point : PortWakePattern().Vertices)
		{
			Fore = FMath::Max(Fore, Point.X);
			Aft = FMath::Max(Aft, -Point.X);
		}
		return FVector2D(Fore + 0.05, Aft + 0.05);
	}();
	const int32 Frames = 48;
	FVector FrameAt[Frames + 1], FrameBeam[Frames + 1];
	float FrameMade[Frames + 1];
	TArray<FVector> WakeLaidNow;
	TArray<FColor> WakeInks;
	for (FPortCraft& Craft : PortCraft)
	{
		if (!Craft.Hull || !PortRoads.IsValidIndex(Craft.Roads))
		{
			continue;
		}
		const FPortRoads& Roads = PortRoads[Craft.Roads];
		const TheaterPortLife::FAfloat Her = TheaterPortLife::Where(Roads.Laid, Now, Craft.Place, Roads.Ships);
		const double Shift = SeenAt(Roads.Laid.Quay.X) - Roads.Laid.Quay.X; // (a whole number of worlds east or west, or nothing)
		// (she rides the water a little, each in her own time)
		const double Odd = Craft.Roads * 1.7 + Craft.Place * 2.9;
		const FRotator Rides(0.8 * FMath::Sin(Now * 1.1 + Odd), FMath::RadiansToDegrees(FMath::Atan2(Her.Ahead.X, -Her.Ahead.Y)), 1.5 * FMath::Sin(Now * 0.83 + Odd * 1.3));
		Craft.Hull->SetRelativeLocationAndRotation(MapToWorld(Her.At.X + Shift, Her.At.Y, 0.02), Rides);
		Craft.Hull->SetRelativeScale3D(FVector(FMath::Max(static_cast<double>(Roads.Shown), 0.02)));
		Craft.Hull->SetVisibility(Roads.Shown > 0.02f);
		if (!Craft.Foam)
		{
			continue;
		}
		// Her wake: as strong as the way she makes, and none while she lies.
		const float Strong = Her.bLying ? 0.0f : Roads.Shown * static_cast<float>(FMath::SmoothStep(0.12, 0.45, Her.Pace));
		if (Strong <= 0.01f)
		{
			if (Craft.bWakeShown)
			{
				Craft.bWakeShown = false;
				Craft.Foam->SetVisibility(false);
			}
			continue;
		}
		// (the water ahead of her and astern, at stations along her course; each point of the pattern is laid between two)
		for (int32 K = 0; K <= Frames; ++K)
		{
			const double Back = FMath::Lerp(-WakeSpan.X, WakeSpan.Y, static_cast<double>(K) / Frames);
			TheaterPortLife::FSpot Spot, Bows;
			FrameMade[K] = static_cast<float>(TheaterPortLife::Astern(Roads.Laid, Her, Back, Spot, Bows));
			FrameAt[K] = MapToWorld(Spot.X + Shift, Spot.Y, 0.02) + FVector(0.0, 0.0, 0.012 * U);
			FrameBeam[K] = FVector(-Bows.X, -Bows.Y, 0.0) * U; // (to starboard, on the table: north is +X and east +Y there, east +X and south +Y on the map)
		}
		const int32 Points = Pattern.Vertices.Num();
		WakeLaidNow.SetNumUninitialized(Points);
		WakeInks.SetNumUninitialized(Points);
		for (int32 K = 0; K < Points; ++K)
		{
			const FVector& Point = Pattern.Vertices[K];
			const double Along = FMath::Clamp((-Point.X + WakeSpan.X) / (WakeSpan.X + WakeSpan.Y), 0.0, 1.0) * Frames;
			const int32 First = FMath::Min(FMath::FloorToInt32(Along), Frames - 1);
			const double Share = Along - First;
			WakeLaidNow[K] = FMath::Lerp(FrameAt[First], FrameAt[First + 1], Share) + FMath::Lerp(FrameBeam[First], FrameBeam[First + 1], Share) * Point.Y;
			FColor Ink = Pattern.Colors[K];
			Ink.A = static_cast<uint8>(FMath::RoundToInt32(Ink.A * Strong * FMath::Lerp(FrameMade[First], FrameMade[First + 1], static_cast<float>(Share))));
			WakeInks[K] = Ink;
		}
		Craft.Foam->UpdateMeshSection(0, WakeLaidNow, TArray<FVector>(), TArray<FVector2D>(), WakeInks, TArray<FProcMeshTangent>());
		if (!Craft.bWakeShown)
		{
			Craft.bWakeShown = true;
			Craft.Foam->SetVisibility(true);
		}
	}
}
