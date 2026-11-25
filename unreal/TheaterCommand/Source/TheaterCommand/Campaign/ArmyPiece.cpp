#include "Campaign/ArmyPiece.h"

#include "Campaign/ArmyCarry.h"
#include "Campaign/BargainLayer.h" // R2 bargains hook
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignUnitTable.h"
#include "Campaign/ShipRide.h"
#include "Campaign/ShipWay.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Campaign/TheaterSpeed.h"
#include "Campaign/TheaterStillMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	// (Release 2, a fleet under way, second part; Brian, 25 November 2026: "I want to keep upleveling the graphics and
	// smoothness of how they move and act to make it feel as real as possible".) Each ship of a fleet on the campaign
	// map has a small mesh of her own, heels outward as she turns and rides the swell where the eye is near
	// (Campaign/ShipRide.h). Off as it comes, until Brian has seen the Mac's film of it; "-TheaterShipRide" turns it on
	// for that film, switch or no switch. The rest are its measures, for the Mac to try without a build.
	TAutoConsoleVariable<int32> CVarShipRide(
		TEXT("tc.Map.ShipRide"), 0,
		TEXT("1: each ship of a fleet on the campaign map has a mesh of her own, heels outward as she turns on a ship's course (tc.Map.ShipWay) and rides the swell where the eye is near. 0: a fleet is one mesh, as before. Fleets are remade the next time the map shows them."));
	TAutoConsoleVariable<float> CVarShipHeel(
		TEXT("tc.Map.ShipHeel"), 1.0f,
		TEXT("With tc.Map.ShipRide: how far a ship heels in her turn, as a multiple of her own (a battleship 5 degrees, a destroyer 9). 0 to 2; over 1 a lee rail may go under."));
	TAutoConsoleVariable<float> CVarShipSwell(
		TEXT("tc.Map.ShipSwell"), 1.0f,
		TEXT("With tc.Map.ShipRide: how much a ship pitches, rolls and heaves in the swell, as a multiple of her own. 0 to 3."));
	TAutoConsoleVariable<float> CVarShipRideReach(
		TEXT("tc.Map.ShipRideReach"), 3000.0f,
		TEXT("With tc.Map.ShipRide: the eye's distance from what it looks at under which ships heel and ride. Further off they lie level and cost nothing."));

	bool ShipRideWanted()
	{
		static const bool bShipRideAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterShipRide"));
		return bShipRideAsked || CVarShipRide.GetValueOnGameThread() != 0;
	}

	// (Release 2, armies with weight; Brian, 25 November 2026: "I want this to visually wow people on how smooth and
	// realistic and detailed grand it looks".) An army piece is carried as a heavy thing is: along a road with its
	// corners rounded, taken up before it goes and let down the last of the way, its foot hanging back from the hand
	// and swinging on, and it rocks once as it lands (Campaign/ArmyCarry.h). Off as it comes, until Brian has seen the
	// Mac's film of it; "-TheaterArmyWeight" turns it on for that film, switch or no switch. The rest are its measures.
	TAutoConsoleVariable<int32> CVarArmyWeight(
		TEXT("tc.Map.ArmyWeight"), 0,
		TEXT("1: an army piece on the campaign map is carried with weight: a rounded road, a lift and a drop, a lean as the hand carries it, a rock and sized dust as it lands. 0: carried as before. Read when a piece is next moved."));
	TAutoConsoleVariable<float> CVarArmyLean(
		TEXT("tc.Map.ArmyLean"), 1.0f,
		TEXT("With tc.Map.ArmyWeight: how far a piece leans as it is carried and rocks as it lands, as a multiple of its own (8 degrees at the most). 0 to 2."));
	TAutoConsoleVariable<float> CVarArmyRound(
		TEXT("tc.Map.ArmyRound"), 1.0f,
		TEXT("With tc.Map.ArmyWeight: how much of each corner of its road is rounded, as a multiple of the usual (four fifths of half the shorter leg, and no further back than nine units). 0 to 3; 0 is the road corner to corner; over 1 the curve may also reach further back, a wider sweep."));
	TAutoConsoleVariable<float> CVarArmyTime(
		TEXT("tc.Map.ArmyTime"), 1.0f,
		TEXT("With tc.Map.ArmyWeight: how long a march takes, as a multiple of what it takes without. 0.7 to 1.6."));

	/** A piece's hang as Campaign/ArmyCarry.h keeps it, from the eight numbers the piece keeps, and back again. */
	TheaterArmyCarry::FHang HangOf(const double* Kept)
	{
		TheaterArmyCarry::FHang Hang;
		Hang.X = Kept[0];
		Hang.Y = Kept[1];
		Hang.RateX = Kept[2];
		Hang.RateY = Kept[3];
		Hang.HandRateX = Kept[4];
		Hang.HandRateY = Kept[5];
		Hang.PushX = Kept[6];
		Hang.PushY = Kept[7];
		return Hang;
	}

	void KeepHang(double* Kept, const TheaterArmyCarry::FHang& Hang)
	{
		Kept[0] = Hang.X;
		Kept[1] = Hang.Y;
		Kept[2] = Hang.RateX;
		Kept[3] = Hang.RateY;
		Kept[4] = Hang.HandRateX;
		Kept[5] = Hang.HandRateY;
		Kept[6] = Hang.PushX;
		Kept[7] = Hang.PushY;
	}

	/**
	 * The turn that stands a piece leaning as its hang has it, facing a yaw on the table. OutRise: how far its middle
	 * rises when it turns about the low edge of a base of this radius (nothing in the air: a radius of nought).
	 */
	FQuat LeaningTurn(const double* Kept, double Reach, double BaseAcross, float Faces, double& OutRise)
	{
		const double LeanBy = FMath::Clamp(static_cast<double>(CVarArmyLean.GetValueOnGameThread()), 0.0, 2.0);
		TheaterArmyCarry::FLean Leans = TheaterArmyCarry::Lean(HangOf(Kept), Reach, TheaterArmyCarry::MostLean);
		Leans.X *= LeanBy;
		Leans.Y *= LeanBy;
		const TheaterArmyCarry::FSet Stands = TheaterArmyCarry::Set(Leans, BaseAcross);
		OutRise = Stands.Rise;
		return FQuat::FindBetweenNormals(FVector::UpVector, FVector(Stands.UpX, Stands.UpY, Stands.UpZ)) * FRotator(0.0f, Faces, 0.0f).Quaternion();
	}
}

AArmyPiece::AArmyPiece()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while on the march
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMeshComponent* AArmyPiece::AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, UMaterialInterface* Material, USceneComponent* Parent)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetupAttachment(Parent ? Parent : Root.Get());
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Size / 100.0); // the engine's basic shapes are one metre across
	Part->RegisterComponent();
	Parts.Add(Part);
	return Part;
}

bool AArmyPiece::HasCorps(int32 CorpsId) const
{
	return Corps.ContainsByPredicate([CorpsId](const FViewCorps& C) { return C.Id == CorpsId; });
}

void AArmyPiece::SetArmy(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 InRegionId, const TArray<FViewCorps>& InCorps)
{
	RegionId = InRegionId;
	NationTag = Nation.Tag;
	Corps = InCorps;
	Divisions = 0;
	Men = 0;
	General.Reset();
	FString NewSignature = Nation.Tag;
	bool bArmoured = false, bHeavyCorps = false;
	for (const FViewCorps& Unit : Corps)
	{
		Divisions += Unit.Divisions;
		Men += Unit.Men;
		if (General.IsEmpty() && !Unit.General.IsEmpty())
		{
			General = Unit.General; // the first general named is the senior one
		}
		NewSignature += FString::Printf(TEXT("|%d%s%s%s"), Unit.Id, Unit.bDim ? TEXT("?") : TEXT(""), Unit.Stale > 0 ? TEXT("~") : TEXT(""), *Unit.Ally); // (what is known of it is part of how it looks)
		if (!Unit.Vol.IsEmpty())
		{
			NewSignature += TEXT("^") + Unit.Vol; // R2 bargains hook: a legion's home pennant is part of how it looks (never in release 1)
		}
		bHeavyCorps |= Unit.Elite == TEXT("heavy");
		bArmoured |= Unit.Kind == TEXT("arm") || Unit.Elite == TEXT("heavy");
	}
	// (Which tank its armour stands as is part of how it looks. The line tank changes with the years and with what
	// its country has designed, and a heavy corps' own tank with the years: the KV-1 takes her cast turret in 1942.
	// Without this a piece kept last year's tank until something else about it changed.)
	if (bArmoured)
	{
		NewSignature += TEXT("|") + FMiniatureMesh::MainTankOf(Nation.Tag);
		if (bHeavyCorps)
		{
			NewSignature += TEXT("|") + FMiniatureMesh::HeavyTankOf(Nation.Tag);
		}
	}
	// What it is about, for the dressing round its base: entrenched or digging, in camp, supplied or cut off.
	{
		int32 Known = 0, Dug = 0, Done = 0, Digging = 0, Starved = 0, Own = 0, Resting = 0, Tired = 0;
		for (const FViewCorps& Unit : Corps)
		{
			if (Unit.bDim || Unit.Stale > 0)
			{
				continue; // (only what is known of it as it is today)
			}
			++Known;
			Dug += Unit.bDug ? 1 : 0;
			Done += Unit.bWorksDone ? 1 : 0;
			Digging += !Unit.bDug && Unit.Posture == TEXT("dig") ? 1 : 0;
			Starved += Unit.bSupplied ? 0 : 1;
			if (Unit.Rest >= 0.0f)
			{
				++Own;
				Resting += Unit.bResting ? 1 : 0;
				Tired += Unit.Rest < 0.95f ? 1 : 0;
			}
		}
		// (Version 58: an earth bank once at least half its corps are dug in, and wire and sandbags once their works
		// are finished, which takes some months of standing still.)
		WorksState = Known == 0 ? 0 : Done * 2 >= Known ? 2 : (Dug + Digging) * 2 >= Known ? 1 : 0;
		// (a camp is pitched by an army that is getting something from its rest: one already fresh simply stands)
		bCamp = Own > 0 && Resting == Own && Tired > 0;
		SupplyState = Known == 0 ? 0 : Starved > 0 ? 2 : 1;
		NewSignature += FString::Printf(TEXT("|w%dc%ds%d"), WorksState, bCamp ? 1 : 0, SupplyState);
	}
	// (Release 2: camps you can see.) How far its camp has got, by the months of fieldworks the rules count, and the
	// figure on the spade tag beside its plate. As with the works above, it is what at least half its known corps have
	// got to; a garrison has its town's walls and pitches no camp, and fortress troops man works of their own.
	{
		TArray<float, TInlineAllocator<8>> Months; // (for each corps counted: its months of works, 0 begun with none to show yet, -1 not dug at all)
		bool bCounted = false;
		for (const FViewCorps& Unit : Corps)
		{
			if (Unit.bDim || Unit.Stale > 0 || Unit.bInTown || Unit.Kind == TEXT("fort"))
			{
				continue;
			}
			bCounted |= Unit.Works >= 0.0f;
			if (Unit.bDug)
			{
				// (where the months are not counted, to be dug in is to have finished)
				Months.Add(Unit.Works >= 0.0f ? Unit.Works : Unit.bWorksDone ? static_cast<float>(FullWorks) : static_cast<float>(Unit.DugFor));
			}
			else
			{
				Months.Add(Unit.bDigging || Unit.Posture == TEXT("dig") ? 0.0f : -1.0f);
			}
		}
		CampStage = 0;
		CampFigure = 0;
		if (Months.Num() > 0)
		{
			Months.Sort([](float Left, float Right) { return Left > Right; });
			const float Had = Months[(Months.Num() + 1) / 2 - 1];
			if (Had >= 0.0f)
			{
				const int32 Whole = FMath::FloorToInt32(Had + 0.05f); // (the rules call works finished at five hundredths short of the full count)
				CampStage = Had >= FullWorks - 0.05f ? 3 : Whole >= 2 ? 2 : 1;
				CampFigure = bCounted ? FMath::Clamp(Whole, 0, FullWorks) : 0;
			}
		}
		NewSignature += FString::Printf(TEXT("|camp%d.%d"), CampStage, CampFigure);
	}
	// A flag that shows its wear: a battered army flies a torn and faded flag, by the strength its known corps have
	// left. ("-TheaterFlagWear=2" wears every flag, for a picture.)
	{
		float Left = 0.0f;
		int32 Counted = 0;
		for (const FViewCorps& Unit : Corps)
		{
			if (!Unit.bDim && Unit.Stale <= 0)
			{
				Left += FMath::Clamp(Unit.Strength, 0.0f, 1.0f);
				++Counted;
			}
		}
		const float Mean = Counted > 0 ? Left / Counted : 1.0f;
		FlagWear = Mean < 0.45f ? 2 : Mean < 0.72f ? 1 : 0;
		// ("-TheaterFlagWear=2" wears every flag for a picture; "-TheaterFlagWear=2:GER" one country's only)
		static int32 Forced = -2;
		static FString ForcedFor;
		if (Forced == -2)
		{
			Forced = -1;
			FString Asked, Count;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFlagWear="), Asked, false))
			{
				if (!Asked.Split(TEXT(":"), &Count, &ForcedFor))
				{
					Count = Asked;
				}
				Forced = FCString::Atoi(*Count);
			}
		}
		if (Forced >= 0 && (ForcedFor.IsEmpty() || ForcedFor == Nation.Tag))
		{
			FlagWear = FMath::Clamp(Forced, 0, 2);
		}
		NewSignature += FString::Printf(TEXT("|f%d"), FlagWear);
	}
	// The standard carries a streamer for every honour its corps have won.
	int32 HonourCount = 0;
	for (const FViewCorps& Unit : Corps)
	{
		HonourCount += Unit.Honours.Num();
	}
	NewSignature += FString::Printf(TEXT("|%dh"), HonourCount);
	// (Release 2, nicknames.ts) The name lettered over the flag, where the names are shown: of the corps that carry one,
	// the one with the most honours. (What is not known of an enemy's corps carries no name.)
	EarnedName.Reset();
	if (bNamesShown && What == 0 && !bGlimpsed)
	{
		int32 Most = -1;
		for (const FViewCorps& Unit : Corps)
		{
			if (!Unit.Nickname.IsEmpty() && !Unit.bDim && Unit.Honours.Num() > Most)
			{
				Most = Unit.Honours.Num();
				EarnedName = Unit.Nickname;
			}
		}
	}
	if (!EarnedName.IsEmpty())
	{
		NewSignature += TEXT("|n:") + EarnedName;
	}
	// (Release 2, the marked army; marked.ts) The town an enemy army took from him, lettered on a red tag on its pole.
	// (The rules give it only where he knows who the army is, and only with the switch on.)
	MarkedTown.Reset();
	if (What == 0 && !bGlimpsed)
	{
		for (const FViewCorps& Unit : Corps)
		{
			if (!Unit.Marked.IsEmpty() && !Unit.bDim)
			{
				MarkedTown = Unit.Marked;
				break;
			}
		}
	}
	if (!MarkedTown.IsEmpty())
	{
		NewSignature += TEXT("|mk:") + MarkedTown;
	}
	// (Release 2, new units ready; Brian, 24 November 2026 18:30Z) One of his corps handed over as this month opened:
	// a brass NEW tag where an enemy's red mark would hang, for its first month (the option "New units ready").
	bTagNew = false;
	if (What == 0 && !bGlimpsed && bReadyTags && MarkedTown.IsEmpty())
	{
		for (const FViewCorps& Unit : Corps)
		{
			bTagNew |= Unit.bReady && !Unit.bDim;
		}
	}
	if (bTagNew)
	{
		NewSignature += TEXT("|new");
	}
	// The standard shows the army's strength to the nearest thousand, so it is remade when that changes.
	NewSignature += FString::Printf(TEXT("|%dk"), FMath::RoundToInt32(Men / 1000.0f));
	NewSignature += bGlimpsed ? TEXT("|glimpsed") : TEXT(""); // (how much of it is drawn is part of how it looks)
	NewSignature += bOnSnow && UnitTable::WinterWash(Nation.Tag) ? TEXT("|snow") : TEXT(""); // (and whether its vehicles are whitewashed)
	if (NewSignature != Signature)
	{
		Signature = NewSignature;
		Rebuild(Map, Nation);
	}
}

void AArmyPiece::DressAs(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 Works, bool bInCamp, int32 Supply, int32 Stage)
{
	if (What != 0)
	{
		return;
	}
	WorksState = Works;
	bCamp = bInCamp;
	SupplyState = Supply;
	// (left out, the camp follows the works: still digging is part of a trench, entrenched the works finished)
	CampStage = FMath::Clamp(Stage >= 0 ? Stage : Works >= 2 ? 3 : Works == 1 ? 2 : 0, 0, 3);
	CampFigure = CampStage == 3 ? FullWorks : CampStage; // (as the grand campaign counts them: a month, two, then the full count)
	Signature += TEXT("|dressed");
	Rebuild(Map, Nation);
	if (Map)
	{
		Map->BuildCamps(); // (the camp on the ground is the map's to lay: it knows the ground)
	}
}

bool AArmyPiece::bFlockNear = true;
bool AArmyPiece::bNamesShown = false;
bool AArmyPiece::bReadyTags = true;
bool AArmyPiece::bCampsNear = true;
int32 AArmyPiece::FullWorks = 3;

void AArmyPiece::LayCamp(const FString& Made, const FMiniatureMesh* Solid, const FMiniatureMesh* Trodden, UMaterialInterface* SolidPaint, UMaterialInterface* TroddenInk)
{
	CampMade = Made;
	if (Camp)
	{
		Camp->DestroyComponent();
		Camp = nullptr;
	}
	const bool bSolid = Solid && Solid->Triangles.Num() > 0 && SolidPaint;
	const bool bTrodden = Trodden && Trodden->Triangles.Num() > 0 && TroddenInk;
	if (!bSolid && !bTrodden)
	{
		return;
	}
	// One still model for the whole camp, its points already where they stand in the world: it is set in the world's
	// own frame and not the piece's, which turns to face the eye and is lifted when it is picked up.
	Camp = UTheaterStillMesh::Make(this, TEXT("Camp"));
	Camp->SetupAttachment(Root);
	Camp->SetUsingAbsoluteLocation(true);
	Camp->SetUsingAbsoluteRotation(true);
	Camp->SetUsingAbsoluteScale(true);
	Camp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Camp->SetCastShadow(bCastsShadows);
	Camp->RegisterComponent();
	int32 Section = 0;
	if (bSolid)
	{
		Camp->CreateMeshSection(Section, Solid->Vertices, Solid->Triangles, Solid->Normals, TArray<FVector2D>(), Solid->Colors, TArray<FProcMeshTangent>(), false);
		Camp->SetMaterial(Section, SolidPaint);
		++Section;
	}
	if (bTrodden)
	{
		Camp->CreateMeshSection(Section, Trodden->Vertices, Trodden->Triangles, Trodden->Normals, TArray<FVector2D>(), Trodden->Colors, TArray<FProcMeshTangent>(), false);
		Camp->SetMaterial(Section, TroddenInk);
	}
	Camp->SetVisibility(bCampsNear);
}

void AArmyPiece::SetCampShown(bool bShown)
{
	if (Camp && Camp->IsVisible() != bShown)
	{
		Camp->SetVisibility(bShown);
	}
}

void AArmyPiece::SetFlockShown(bool bShown)
{
	if (Flock && Flock->IsVisible() != bShown)
	{
		Flock->SetVisibility(bShown);
	}
	if (FarEarth && FarEarth->IsVisible() == bShown)
	{
		FarEarth->SetVisibility(!bShown);
	}
}

void AArmyPiece::SetAtAnchor(bool bInHarbour)
{
	bAtAnchor = bInHarbour;
	if (Standard)
	{
		Standard->SetVisibility(!bAtAnchor, true); // (the pole, the cloth and the plate with it)
	}
	if (What == 1)
	{
		SetSize(bAtAnchor ? 0.55f : 1.0f);
	}
}

bool AArmyPiece::GetColours(FVector& OutMiddle, double& OutRadius) const
{
	if (IsHidden() || !Cloth || !Cloth->IsVisible())
	{
		return false;
	}
	OutMiddle = Cloth->Bounds.Origin;
	OutRadius = Cloth->Bounds.SphereRadius;
	return OutRadius > 0.0;
}

FVector AArmyPiece::GetCampFire() const
{
	// (as it is placed when the piece is built: forward-left of the tents, on the viewer's right of the base)
	return GetActorTransform().TransformPosition(CampFireAt);
}

void AArmyPiece::SetFleet(ACampaignMapActor* Map, const FCampaignNation& Nation, const FViewFleet& InFleet)
{
	What = 1;
	RegionId = INDEX_NONE;
	NationTag = Nation.Tag;
	Fleet = InFleet;
	// (a fleet that puts to sea or comes into harbour is remade: it has a wake at sea and none in port)
	// (release 2, a fleet under way, second part: and when tc.Map.ShipRide is turned on or off, for her ships are then meshes of their own, or one)
	const FString NewSignature = FString::Printf(TEXT("%s|fleet|%d|%d|%d|%d|%d|%d|%s|%d|%s%s"), *Nation.Tag, InFleet.Id,
		InFleet.Battleships, InFleet.Carriers, InFleet.Cruisers, InFleet.Destroyers, InFleet.Submarines, *InFleet.Stance, InFleet.bReport ? 1 : 0, *InFleet.SizeWord,
		ShipRideWanted() ? TEXT("|ride") : TEXT(""));
	if (NewSignature != Signature)
	{
		Signature = NewSignature;
		Rebuild(Map, Nation);
	}
}

void AArmyPiece::SetAirFleet(ACampaignMapActor* Map, const FCampaignNation& Nation, const FViewAirFleet& InWing)
{
	What = 2;
	RegionId = INDEX_NONE;
	NationTag = Nation.Tag;
	Wing = InWing;
	// Rebuilt when its strength changes by a round fifty, not for every machine lost.
	const FString NewSignature = FString::Printf(TEXT("%s|air|%d|%d|%d"), *Nation.Tag, InWing.Id, InWing.Fighters / 50, InWing.Bombers / 50);
	if (NewSignature != Signature)
	{
		Signature = NewSignature;
		Rebuild(Map, Nation);
	}
}

void AArmyPiece::Rebuild(ACampaignMapActor* Map, const FCampaignNation& Nation)
{
	// (An army standing on snow, of a country that whitewashes its vehicles: the painter is told for as long as this
	// piece is built, and told again that it is over whichever way the building ends.)
	UnitTable::SetWash(What == 0 && bOnSnow && UnitTable::WinterWash(Nation.Tag));
	ON_SCOPE_EXIT { UnitTable::SetWash(false); };
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	SelectionRing = nullptr;
	if (Body)
	{
		Body->DestroyComponent();
		Body = nullptr;
	}
	// (Release 2, a fleet under way, second part.) Her ships' own meshes go with the body. How each stood is kept, so
	// that a fleet remade in the middle of her turn does not snap upright.
	const TArray<FHullRide> HullsWere = MoveTemp(HullRides);
	HullRides.Reset();
	for (UProceduralMeshComponent* HullMesh : Hulls)
	{
		if (HullMesh)
		{
			HullMesh->DestroyComponent();
		}
	}
	Hulls.Reset();
	bHullsLevel = true;
	if (Flock)
	{
		Flock->DestroyComponent();
		Flock = nullptr;
	}
	if (FarEarth)
	{
		FarEarth->DestroyComponent();
		FarEarth = nullptr;
	}
	FMiniatureMesh Turf; // (the flock and tufts on the base: a mesh of their own, shown only close to)
	FMiniatureMesh DarkEarth; // (and the darker earth that lies over the base's ground from working height)
	if (Cloth)
	{
		Cloth->DestroyComponent();
		Cloth = nullptr;
	}
	if (StrengthLabel)
	{
		StrengthLabel->DestroyComponent();
		StrengthLabel = nullptr;
	}
	if (WorksLabel)
	{
		WorksLabel->DestroyComponent();
		WorksLabel = nullptr;
	}
	if (NameLabel)
	{
		NameLabel->DestroyComponent();
		NameLabel = nullptr;
	}
	if (MarkLabel)
	{
		MarkLabel->DestroyComponent();
		MarkLabel = nullptr;
	}
	if (Mast)
	{
		Mast->DestroyComponent();
		Mast = nullptr;
	}
	if (Standard)
	{
		Standard->DestroyComponent();
		Standard = nullptr;
	}

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	const FLinearColor Colour(Nation.Color);
	const FLinearColor Dark = Colour * 0.55f;
	const FLinearColor Light = FMath::Lerp(Colour, FLinearColor::White, 0.45f);
	UMaterialInterface* Paint = Map->GetSolidMaterial(Colour, 0.5f, 0.0f);
	UMaterialInterface* DarkPaint = Map->GetSolidMaterial(Dark, 0.5f, 0.0f);
	// A banner has to be read from any side, so it carries a little light of its own.
	UMaterialInterface* Banner = Map->GetSolidMaterial(Colour, 0.6f, 0.0f, 0.55f);
	UMaterialInterface* BannerBand = Map->GetSolidMaterial(Light, 0.6f, 0.0f, 0.55f);
	UMaterialInterface* Iron = Map->GetSolidMaterial(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f, 1.0f);
	UMaterialInterface* Brass = Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("C9A227"))), 0.3f, 1.0f);
	UMaterialInterface* Glow = Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("FFD27A"))), 0.4f, 0.0f, 2.5f);

	const double U = Map->UnitScale * 1.4; // one map unit, with the pieces a little over life size so they read from far off
	LiftHeight = 3.2 * U;

	// The ring that shows when the piece is picked up: a thin bright disc a little wider than the base.
	SelectionRing = AddPart(Cylinder, FVector(0.0, 0.0, 0.06 * U), FVector(12.6 * U, 12.6 * U, 0.12 * U), Glow); // (a little wider than the base and its rim)
	SelectionRing->SetCastShadow(false);
	ShowRing();

	// Everything solid about the piece goes into one mesh.
	FMiniatureMesh Model;
	const FLinearColor IronPaint(FColor::FromHex(TEXT("1C1C1F")));
	const FLinearColor BrassPaint(FColor::FromHex(TEXT("C9A227")));

	double CounterWidth = 8.6 * U;
	double Top = 0.14 * U;
	double Size = 1.0;
	FString Figure;
	if (What == 1)
	{
		// A fleet: its ships lie on the water, with no base under them.
		Top = 0.02 * U;
		CounterWidth = 9.5 * U;
		if (Fleet.bReport)
		{
			// Known only from a report: a plotting marker on the water, as on an admiral's chart, under the flag of
			// whoever it is said to be. No ships are shown, for nobody has counted them.
			const FLinearColor Marker(FColor::FromHex(TEXT("3A3F45")));
			Model.Pillar(FVector(0.0, 0.0, Top), 1.5 * U, 1.25 * U, 0.34 * U, Marker, 6);
			Model.Pillar(FVector(0.0, 0.0, Top + 0.34 * U), 0.9 * U, 0.0, 0.5 * U, Colour, 6);
		}
		else
		{
			Model.Fleet(Fleet.Battleships, Fleet.Carriers, Fleet.Cruisers, Fleet.Destroyers, Fleet.Submarines, U, Colour, NationTag, Fleet.Stance != TEXT("port"));
		}
		Size = Fleet.bReport ? 0.9 : FMath::Clamp(0.8 + 0.012 * Fleet.Ships(), 0.9, 1.7);
		// Its plate says how many ships, and what they are about when it is not a plain patrol.
		Figure = Fleet.bReport ? (Fleet.SizeWord == TEXT("Submarines") ? FString(TEXT("? subs")) : Fleet.SizeWord == TEXT("A fleet") ? FString(TEXT("? fleet")) : Fleet.SizeWord == TEXT("A squadron") ? FString(TEXT("? squadron")) : FString(TEXT("? ships")))
			: Fleet.Stance == TEXT("escort") ? FString::Printf(TEXT("%d escort"), Fleet.Ships())
			: Fleet.Stance == TEXT("hunt") ? FString::Printf(TEXT("%d hunting"), Fleet.Ships())
			: Fleet.Stance == TEXT("raid") ? FString::Printf(TEXT("%d raiding"), Fleet.Ships())
			: FString::Printf(TEXT("%d ship%s"), Fleet.Ships(), Fleet.Ships() == 1 ? TEXT("") : TEXT("s")); // (1.5.1: "1 ship", Brian's hand-check list)
		SelectionRing->SetVisibility(false);
	}
	else if (What == 2)
	{
		// An air fleet: its aircraft over a patch of airfield.
		// (Art fix 8: under half the size it was. It is kept only where the table shows no airfields to click,
		// in the scenarios: where it does, the airfield itself says who flies from it.)
		const double Small = 0.46;
		Model.AirFleet(Wing.Fighters, Wing.Bombers, U * Small, Colour, NationTag);
		Top = 0.2 * U * Small;
		CounterWidth = 8.6 * U * Small;
		Size = 0.62 * FMath::Clamp(0.8 + (Wing.Fighters + Wing.Bombers) / 2500.0, 0.9, 1.3);
		Figure = FText::AsNumber(Wing.Fighters + Wing.Bombers).ToString();
	}
	else
	{
		// A counter for each corps, stacked: the taller the stack, the bigger the army.
		// (Art fix 8: the base is a fifth wider than it was, and stands on a rim.)
		// (rim and all it comes to a fifth more than the base was across, and no more: from working height the
		// flags must still lead the eye, not the discs under them)
		CounterWidth *= 1.06;
		// (the counters are painted wood, not enamel: the nation's colour with its saturation capped and its
		// brightness brought down, so that the flag above leads the eye and not the disc; and a broad brass edge
		// that still reads from working height)
		FLinearColor Seen = FLinearColor(Colour.ToFColor(true).R / 255.0f, Colour.ToFColor(true).G / 255.0f, Colour.ToFColor(true).B / 255.0f).LinearRGBToHSV();
		// (the caps are on the paint; under the lamp it comes out near saturation 0.55 and brightness 0.42 on the screen)
		Seen.G = FMath::Min(Seen.G, 0.42f);
		Seen.B = FMath::Min(Seen.B, 0.4f);
		const FLinearColor Painted = Seen.HSVToLinearRGB();
		const FLinearColor Wood(FColor(static_cast<uint8>(Painted.R * 255.0f), static_cast<uint8>(Painted.G * 255.0f), static_cast<uint8>(Painted.B * 255.0f)));
		const FLinearColor WoodDark = Wood * 0.6f;
		Model.Pillar(FVector(0.0, 0.0, 0.0), CounterWidth * 0.5 + 0.72 * U, CounterWidth * 0.5 + 0.62 * U, Top + 0.04 * U, WoodDark, 30);
		Model.Pillar(FVector(0.0, 0.0, Top + 0.04 * U), CounterWidth * 0.5 + 0.62 * U, CounterWidth * 0.5 + 0.5 * U, 0.12 * U, FLinearColor(FColor::FromHex(TEXT("846C2E"))), 30); // (old brass: it must not be taken for the bright ring of a piece that is picked)
		const int32 Counters = FMath::Clamp(Corps.Num(), 1, 7);
		const double CounterHeight = 0.5 * U, CounterGap = 0.1 * U;
		for (int32 i = 0; i < Counters; ++i)
		{
			Model.Pillar(FVector(0.0, 0.0, Top), CounterWidth * 0.5, CounterWidth * 0.5 - 0.08 * U, CounterHeight, i % 2 == 0 ? Wood : WoodDark, 30);
			Top += CounterHeight + CounterGap;
		}
		Top -= CounterGap;
		if (!FMiniatureMesh::bOldPieces)
		{
			// The top of the base is ground, as a wargamer bases his figures (art fix 12): trodden earth, with flock
			// of two greens scattered on it in patches and a few tufts of longer grass. (The counters under it keep
			// the nation's colour, and the brass rim under them.)
			FRandomStream Scatter(GetTypeHash(NationTag) * 31 + RegionId * 7 + 3);
			// (the earth is pale, trodden and dry, so that the figures stand out against it from working height)
			const FLinearColor Earth(FColor::FromHex(TEXT("806A4C"))), EarthPale(FColor::FromHex(TEXT("917A58"))), FlockDark(FColor::FromHex(TEXT("3E4A26"))), FlockPale(FColor::FromHex(TEXT("4F5A2C"))), Straw(FColor::FromHex(TEXT("8C8448")));
			// (a ring of the counter's own painted wood is left showing round the ground, so that the base still says whose it is from above)
			// (the ring of painted wood is a quarter of the base's radius)
			const double Ground = 0.07 * U, Reach = CounterWidth * 0.5 * 0.75;
			Model.Pillar(FVector(0.0, 0.0, Top), Reach, Reach - 0.05 * U, Ground, Earth, 30);
			DarkEarth.Pillar(FVector(0.0, 0.0, Top + Ground), Reach - 0.05 * U, Reach - 0.06 * U, 0.012 * U, FLinearColor(FColor::FromHex(TEXT("6C5A40"))), 30);
			for (int32 K = 0; K < 26; ++K)
			{
				const double Angle = Scatter.FRandRange(0.0f, 2.0f * PI), Out = Reach * FMath::Sqrt(Scatter.FRand()) * 0.9;
				const double Wide = Scatter.FRandRange(0.3f, 0.8f) * U;
				const FVector At(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, Top + Ground);
				const int32 Sort = K % 5;
				(Sort == 0 ? Model : Turf).Pillar(At, FMath::Min(Wide, Reach - Out), FMath::Min(Wide, Reach - Out) * 0.7, 0.02 * U + 0.003 * U * K, Sort == 0 ? EarthPale : Sort % 2 == 1 ? FlockDark : FlockPale, 11);
				if (Sort == 3)
				{
					// (a tuft: three blades of dry grass)
					for (int32 Blade = 0; Blade < 3; ++Blade)
					{
						const double Lean = Blade * 2.1 + Angle;
						Turf.Rod(At, At + FVector(FMath::Cos(Lean) * 0.12 * U, FMath::Sin(Lean) * 0.12 * U, 0.34 * U), 0.03 * U, Straw, 3);
					}
				}
			}
			Top += Ground + 0.03 * U;
		}

		// On the counters stand figures for the kinds of troops the army holds: riflemen, armour, lorries, horse.
		TArray<FString> Kinds;
		for (const FViewCorps& Unit : Corps)
		{
			// A heavy tank corps shows its heavy tanks, whatever the rest of the army drives.
			Kinds.Add(Unit.Elite == TEXT("heavy") ? FString(TEXT("heavy")) : Unit.Kind);
		}
		Model.Force(Kinds, CounterWidth * 0.5, Top, U, Colour, NationTag);

		// The dressing round the base, which says what the army is about without a click. (The piece is always
		// turned so that -X is toward whoever looks at the table, +Y to his right.)
		// (Release 2: camps you can see. The works are no longer a ring of sandbags and wire round the front of the
		// base, which faced the eye and not the enemy: an army that stands and digs grows a camp on the ground round
		// it, its trench and wire on the side toward the enemy, which the map lays (ACampaignMapActor::BuildCamps).
		// What stays here turns with the piece, and keeps inside the ring that camp leaves clear.)
		const double Rim = CounterWidth * 0.5;
		if (bCamp)
		{
			// In camp: a row of ridge tents to the right of the base, their open ends to the viewer, and a fire. (An
			// army whose camp is pitched round it has its tents there, and keeps only its fire here.)
			const FLinearColor Tent(FColor::FromHex(TEXT("D2C8A4"))), TentShade(FColor::FromHex(TEXT("B8AD86"))), Log(FColor::FromHex(TEXT("4A3A2A"))), Flame(FColor::FromHex(TEXT("FF8A2A"))), Ember(FColor::FromHex(TEXT("FFD060")));
			for (int32 K = 0; K < 3 && CampStage == 0; ++K)
			{
				Model.House(FVector((-2.4 + K * 2.1) * U, Rim + (2.3 + 0.3 * (K % 2)) * U, 0.0), 1.9 * U, 1.5 * U, 0.06 * U, 1.1 * U, K % 2 == 0 ? Tent : TentShade, K % 2 == 0 ? TentShade : Tent, 0.0);
			}
			const FVector Fire(-4.4 * U, Rim + 2.0 * U, 0.0);
			CampFireAt = Fire + FVector(0.0, 0.0, 0.6 * U);
			for (int32 K = 0; K < 3; ++K)
			{
				const double A = K * 2.094;
				Model.Rod(Fire + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * 0.38 * U, Fire - FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * 0.2 * U + FVector(0.0, 0.0, 0.2 * U), 0.07 * U, Log, 4);
			}
			Model.Pillar(Fire + FVector(0.0, 0.0, 0.08 * U), 0.3 * U, 0.0, 0.85 * U, Flame, 6);
			Model.Pillar(Fire + FVector(-0.04 * U, 0.0, 0.08 * U), 0.16 * U, 0.0, 0.5 * U, Ember, 5);
		}
		if (SupplyState == 1)
		{
			// Supplied: a lorry of its column drawn up on the left, and a dump of crates and drums.
			const FLinearColor Crate(FColor::FromHex(TEXT("7A6444"))), Drum(FColor::FromHex(TEXT("4F5A4A")));
			Model.Lorry(FVector(-0.2 * U, -(Rim + 1.9 * U), 0.0), 1.3 * U, Colour, 12.0);
			Model.Box(FVector(-2.5 * U, -(Rim + 1.5 * U), 0.22 * U), FVector(0.6, 0.6, 0.44) * U, Crate, 20.0);
			Model.Box(FVector(-3.1 * U, -(Rim + 1.1 * U), 0.22 * U), FVector(0.6, 0.6, 0.44) * U, Crate, -10.0);
			Model.Box(FVector(-2.8 * U, -(Rim + 1.3 * U), 0.64 * U), FVector(0.55, 0.55, 0.4) * U, Crate, 35.0);
			Model.Pillar(FVector(-3.3 * U, -(Rim + 1.9 * U), 0.0), 0.24 * U, 0.24 * U, 0.6 * U, Drum, 8);
		}
		else if (SupplyState == 2)
		{
			// Cut off: no column comes up. A barred red board on a post, and the last crates lying empty.
			const FLinearColor Post(FColor::FromHex(TEXT("3A342C"))), Red(FColor::FromHex(TEXT("C4261D"))), White(FColor::FromHex(TEXT("F0ECE0"))), Crate(FColor::FromHex(TEXT("7A6444")));
			const FVector Foot(-1.6 * U, -(Rim + 1.6 * U), 0.0);
			Model.Rod(Foot, Foot + FVector(0.0, 0.0, 2.6 * U), 0.09 * U, Post, 5);
			Model.Box(Foot + FVector(-0.12 * U, 0.0, 2.9 * U), FVector(0.1, 1.9, 1.3) * U, Red);
			Model.Box(Foot + FVector(-0.19 * U, 0.0, 2.9 * U), FVector(0.06, 1.5, 0.3) * U, White);
			Model.Box(FVector(-3.0 * U, -(Rim + 1.3 * U), 0.2 * U), FVector(0.6, 0.6, 0.4) * U, Crate, 25.0, 38.0);
			Model.Box(FVector(-2.3 * U, -(Rim + 2.3 * U), 0.14 * U), FVector(0.6, 0.6, 0.28) * U, Crate, -30.0, 0.0);
		}

		// The bigger the army, the bigger its flag; and its strength is written, in thousands of men, on a dark
		// plate under the flag.
		Size = FMath::Clamp(0.82 + 0.26 * FMath::Sqrt(Men / 30000.0), 0.9, 2.0);
		Figure = Men >= 995000 ? FString::Printf(TEXT("%.1fM"), Men / 1000000.0) : FString::Printf(TEXT("%dk"), FMath::Max(1, FMath::RoundToInt32(Men / 1000.0)));
		// An army only glimpsed through the fog of war: its flag, how many corps, and no more.
		int32 Dim = 0;
		bool bStale = false;
		for (const FViewCorps& Unit : Corps)
		{
			Dim += Unit.bDim ? 1 : 0;
			bStale |= Unit.Stale > 0;
		}
		if (Corps.Num() > 0 && Dim == Corps.Num())
		{
			Size = 1.0;
			Figure = Corps.Num() == 1 ? FString(TEXT("?")) : FString::Printf(TEXT("%d ?"), Corps.Num());
		}
		else if (Dim > 0)
		{
			// Some of its corps are not identified: what is known, and that there is more.
			Figure = Figure + TEXT(" +?");
		}
		else if (bStale)
		{
			Figure = Figure + TEXT("?"); // known from reports gone stale (a mark that cannot be misread as a minus at map size)
		}
		if (bGlimpsed)
		{
			// (Ruling 154: at a glimpse the tag is the question mark alone. How many corps is on the box when he points at the flag.)
			Size = 1.0;
			Figure = TEXT("?");
		}
	}

	// The standard: a pole at the back of the base, the nation's flag of the period, and a brass finial.
	// (Art fix 8: the flag is three quarters the size it was. The plate under it keeps its lettering.)
	const double FlagWidth = 6.6 * 0.75 * U * Size, FlagHeight = FlagWidth / 1.5;
	const double PoleHeight = (What == 2 ? 7.0 : 8.4) * U + FlagHeight; // (an air group's plate hangs clear above its aircraft)
	const FVector Pole(CounterWidth * 0.32, -CounterWidth * 0.3, 0.0); // behind the figures, and to the left, so the flag flies over the piece
	// (The standard is a thing of its own, standing on the foot of its pole: see the header.)
	StandardFoot = Pole + FVector(0.0, 0.0, Top);
	Standard = NewObject<USceneComponent>(this);
	Standard->SetupAttachment(Root);
	Standard->SetRelativeLocation(StandardFoot);
	Standard->RegisterComponent();
	FMiniatureMesh Staff;
	if (bGlimpsed)
	{
		// (Ruling 154; the art director's review of the agent piece.) The pole is gone before it reaches the ground: it
		// runs whole to just under its tag and thins away to nothing a little below it, on any ground. (It thinned over
		// most of its length, and from close to it still read as a pole standing on whatever lay at its foot.)
		const double TagHigh = FMath::Min(FlagHeight * 0.56, 2.7 * U);
		const double UnderTag = PoleHeight - FlagHeight - 0.65 * U - TagHigh - 0.4 * U;
		Staff.Pillar(FVector(0.0, 0.0, UnderTag), 0.2 * U, 0.2 * U, PoleHeight - UnderTag, IronPaint, 8);
		Staff.Pillar(FVector(0.0, 0.0, UnderTag - 2.4 * U), 0.02 * U, 0.2 * U, 2.4 * U, IronPaint, 8);
	}
	else
	{
		Staff.Pillar(FVector::ZeroVector, 0.2 * U, 0.2 * U, PoleHeight, IronPaint, 8);
	}
	Staff.Ball(FVector(0.0, 0.0, PoleHeight + 0.4 * U), FVector(1.1 * U), BrassPaint, 6, 10);
	const double Hoist = 0.2 * U; // where the cloth begins, just clear of the pole
	const FVector FlagAt = Pole + FVector(0.0, Hoist + FlagWidth * 0.5, Top + PoleHeight - FlagHeight * 0.5 - 0.3 * U);
	if (UMaterialInterface* Colours = Map->GetFlagMaterial(Nation.Tag, What == 0 ? FlagWear : 0, bGlimpsed))
	{
		// The cloth is a sheet with a slow ripple in it that grows toward the free end, as a flag hangs in a light
		// air. Pieces are turned so that the viewer looks along their +X: the cloth faces back along -X, the pole
		// on the viewer's left, which is the side a flag is read from.
		const int32 Wear = What == 0 ? FlagWear : 0;
		const int32 Across = Wear > 0 ? 30 : 18, Down = Wear > 0 ? 18 : 6;
		const double Ripple = 0.045 * FlagWidth, Waves = 1.3, Droop = 0.035 * FlagHeight;
		const double Phase = (RegionId * 37 % 100) * 0.0628; // no two neighbours hang alike
		// A flag's wear is told by its shape and by soot, not by fading (the art director's ruling: a faded
		// black-white-red read as Poland's). Worn: the fly frayed, and one hole shot through it. In tatters: the
		// fly torn into strips for a third of its length, each strip hanging by itself and torn off where it was
		// torn off; three holes; and soot. The holes are ragged: they are cut a triangle at a time.
		FRandomStream Tatter(RegionId * 977 + 31);
		const int32 Period = 4; // (in tatters: three rows of cloth to a strip, then a row torn out)
		TArray<int32> RowEnd, TearAt; // (for each row of cells: the first cell that is gone)
		TArray<double> StripHang;
		for (int32 K = 0; K <= Down / Period; ++K)
		{
			StripHang.Add(Wear == 2 ? 0.03 + 0.11 * Tatter.FRand() : 0.0);
		}
		{
			int32 StripEnd = Across;
			for (int32 J = 0; J < Down; ++J)
			{
				if (Wear == 2 && J % Period == 0)
				{
					StripEnd = Across - Tatter.RandRange(0, 5); // (where this strip was torn off)
				}
				RowEnd.Add(Wear == 0 ? Across : Wear == 1 ? Across - Tatter.RandRange(0, 2) : StripEnd - Tatter.RandRange(0, 1));
				TearAt.Add(Wear == 2 && J % Period == Period - 1 ? FMath::RoundToInt32(Across * (0.63 + 0.08 * Tatter.FRand())) : Across);
			}
		}
		struct FShot { double I, J, RI, RJ; };
		TArray<FShot> Shots;
		for (int32 Shot = 0; Shot < (Wear == 0 ? 0 : Wear == 1 ? 1 : 3); ++Shot)
		{
			// (A hole never falls on the device that says whose flag it is: flags carry theirs in the middle or in the
			// upper corner by the staff, so the holes fall toward the fly, or low down by the staff.)
			const bool bFly = Shot == 0 ? Wear == 1 : Shot == 2;
			Shots.Add({ bFly ? Tatter.FRandRange(Across * 0.76f, Across * 0.86f) : Tatter.FRandRange(Across * 0.1f, Across * 0.24f),
				bFly ? Tatter.FRandRange(3.0f, Down - 3.0f) : Tatter.FRandRange(Down * 0.6f, Down - 3.0f),
				Tatter.FRandRange(1.1f, 1.9f) * (Shot == 0 && Wear == 2 ? 1.3 : 1.0), Tatter.FRandRange(0.9f, 1.5f) });
		}
		auto Holed = [&Shots, &Tatter](double I, double J)
		{
			for (const FShot& Shot : Shots)
			{
				if (FMath::Sqrt(FMath::Square((I - Shot.I) / Shot.RI) + FMath::Square((J - Shot.J) / Shot.RJ)) + 0.45 * (Tatter.FRand() - 0.5) < 1.0)
				{
					return true;
				}
			}
			return false;
		};
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		TArray<FLinearColor> Soot;
		const double TornFrom = 0.62;
		for (int32 J = 0; J <= Down; ++J)
		{
			for (int32 I = 0; I <= Across; ++I)
			{
				const double S = static_cast<double>(I) / Across, T = static_cast<double>(J) / Down;
				const double Angle = UE_DOUBLE_TWO_PI * Waves * S + Phase + 0.8 * T;
				// (a strip torn free hangs lower the farther out it runs, each by its own amount)
				const double Hang = StripHang[FMath::Min(J / Period, StripHang.Num() - 1)] * FMath::Square(FMath::Max(S - TornFrom, 0.0) / (1.0 - TornFrom)) * FlagHeight;
				Vertices.Add(FVector(Pole.X + Ripple * S * FMath::Sin(Angle), Pole.Y + Hoist + S * FlagWidth, FlagAt.Z + FlagHeight * (0.5 - T) - Droop * S * S - Hang));
				const FVector Along(Ripple * (FMath::Sin(Angle) + S * UE_DOUBLE_TWO_PI * Waves * FMath::Cos(Angle)), FlagWidth, -2.0 * Droop * S);
				const FVector Downward(Ripple * S * 0.8 * FMath::Cos(Angle), 0.0, -FlagHeight);
				Normals.Add((Along ^ Downward).GetSafeNormal());
				Tangents.Add(FProcMeshTangent(Along.GetSafeNormal(), false));
				UVs.Add(FVector2D(S, T));
				// Soot: round every hole, and (in tatters) along the fly, in smudges.
				double Dirt = 0.0;
				for (const FShot& Shot : Shots)
				{
					const double Far = FMath::Sqrt(FMath::Square((I - Shot.I) / Shot.RI) + FMath::Square((J - Shot.J) / Shot.RJ));
					Dirt = FMath::Max(Dirt, (Wear == 2 ? 0.5 : 0.3) * (1.0 - FMath::SmoothStep(1.0, 2.6, Far)));
				}
				if (Wear == 2)
				{
					Dirt = FMath::Max(Dirt, 0.42 * FMath::SmoothStep(0.5, 1.0, S) * (0.55 + 0.45 * FMath::Sin(S * 23.0 + T * 17.0 + Phase)));
				}
				const float Clean = static_cast<float>(1.0 - Dirt);
				Soot.Add(FLinearColor(Clean, Clean * 0.98f, Clean * 0.95f, 1.0f));
			}
		}
		for (int32 J = 0; J < Down; ++J)
		{
			for (int32 I = 0; I < Across; ++I)
			{
				const int32 End = FMath::Min(RowEnd[J], TearAt[J]);
				if (I >= End)
				{
					continue; // torn away
				}
				const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
				if (Wear == 0)
				{
					Triangles.Append({ A, C, B, B, C, D });
					continue;
				}
				if (!Holed(I + 0.33, J + 0.33))
				{
					Triangles.Append({ A, C, B });
				}
				// (the last cell of a row keeps one corner of itself only: a frayed end, not a cut one)
				if (I + 1 < End && !Holed(I + 0.67, J + 0.67))
				{
					Triangles.Append({ B, C, D });
				}
			}
		}
		Cloth = UTheaterStillMesh::Make(this, TEXT("Cloth"));
		Cloth->SetupAttachment(Standard);
		Cloth->SetRelativeLocation(-StandardFoot); // (its points are written in the piece's own space)
		Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cloth->RegisterComponent();
		Cloth->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Wear > 0 ? Soot : TArray<FLinearColor>(), Tangents, false, false);
		Cloth->SetMaterial(0, Colours);
	}
	else
	{
		// No flag material: a plain banner in the nation's colour, with a paler band top and bottom.
		AddPart(Cube, FlagAt - StandardFoot, FVector(0.16 * U, FlagWidth, FlagHeight), Banner, Standard);
		AddPart(Cube, FlagAt - StandardFoot + FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand, Standard);
		AddPart(Cube, FlagAt - StandardFoot - FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand, Standard);
	}
	PoleFoot = Pole;
	PoleTop = Top + PoleHeight;
	BodyTop = Top + 3.2 * U;
	BaseRadius = CounterWidth * 0.5;
	FlagCenter = FlagAt;
	FlagRadius = FlagWidth * 0.55;

	// The strength plate, hung from the pole under the flag, edged in brass.
	const double PlateHeight = FMath::Min(FlagHeight * 0.56, 2.7 * U); // (the same height as before the flag was made smaller)
	const double PlateWidth = PlateHeight * (0.95 + 0.5 * Figure.Len());
	const FVector PlateAt = Pole + FVector(0.0, Hoist + PlateWidth * 0.5, FlagAt.Z - FlagHeight * 0.5 - 0.35 * U - PlateHeight * 0.5);
	Staff.Box(PlateAt - StandardFoot, FVector(0.2 * U, PlateWidth, PlateHeight), FLinearColor(FColor::FromHex(TEXT("15130F"))));
	Staff.Box(PlateAt - StandardFoot + FVector(0.0, 0.0, PlateHeight * 0.5), FVector(0.26 * U, PlateWidth + 0.1 * U, 0.14 * U), BrassPaint);
	Staff.Box(PlateAt - StandardFoot - FVector(0.0, 0.0, PlateHeight * 0.5), FVector(0.26 * U, PlateWidth + 0.1 * U, 0.14 * U), BrassPaint);

	// (Release 2: camps you can see.) Beside the plate, a smaller one in the same dark and brass for an army that has
	// stood and dug: a spade, the months its works have had, and a tick once they are finished (the art director's
	// paintover). The figure is lettered as the plate's is, further down; the spade and the tick are moulded on.
	FString TagFigure; // (the months, or nothing where none are counted or none show yet)
	FVector TagFigureAt = FVector::ZeroVector;
	double TagHigh = 0.0;
	if (What == 0 && !bGlimpsed && CampStage > 0)
	{
		TagHigh = PlateHeight * 0.8;
		TagFigure = CampFigure > 0 ? FString::FromInt(CampFigure) : FString();
		const bool bTick = CampStage >= 3;
		const double Pad = 0.2 * TagHigh, Gap = 0.12 * TagHigh, SpadeWide = 0.4 * TagHigh, Letters = 0.52 * TagHigh * TagFigure.Len(), TickWide = 0.56 * TagHigh;
		const double TagWide = Pad + SpadeWide + (Letters > 0.0 ? Gap + Letters : 0.0) + (bTick ? Gap + TickWide : 0.0) + Pad;
		const double From = Pole.Y + Hoist + PlateWidth + 0.3 * PlateHeight; // (its near edge, a little way along from the plate's far one)
		const FVector TagAt(Pole.X, From + TagWide * 0.5, PlateAt.Z);
		Staff.Box(TagAt - StandardFoot, FVector(0.2 * U, TagWide, TagHigh), FLinearColor(FColor::FromHex(TEXT("15130F"))));
		// (edged in brass all round, where the plate has it top and bottom only: it reads as a thing of its own beside it)
		for (const double Edge : { -0.5, 0.5 })
		{
			Staff.Box(TagAt - StandardFoot + FVector(0.0, 0.0, TagHigh * Edge), FVector(0.26 * U, TagWide + 0.1 * U, 0.12 * U), BrassPaint);
			Staff.Box(TagAt - StandardFoot + FVector(0.0, TagWide * Edge, 0.0), FVector(0.26 * U, 0.12 * U, TagHigh + 0.1 * U), BrassPaint);
		}
		// The spade and the tick lie on the plate's face, which looks back along -X: across it is +Y, up it +Z.
		const double FaceX = Pole.X - 0.13 * U;
		const FVector Out(-1.0, 0.0, 0.0);
		auto OnFace = [&](double Across, double Up) { return FVector(FaceX, Across, PlateAt.Z + Up) - StandardFoot; };
		auto Panel = [&](double Left, double Right, double Low, double High, const FColor& Ink)
		{
			Staff.Quad(OnFace(Left, Low), OnFace(Right, Low), OnFace(Right, High), OnFace(Left, High), Out, Ink);
		};
		const FColor Brassy = BrassPaint.ToFColor(true);
		const double Spade = From + Pad + SpadeWide * 0.5;
		Panel(Spade - 0.16 * TagHigh, Spade + 0.16 * TagHigh, 0.26 * TagHigh, 0.33 * TagHigh, Brassy); // the grip
		Panel(Spade - 0.035 * TagHigh, Spade + 0.035 * TagHigh, -0.02 * TagHigh, 0.27 * TagHigh, Brassy); // the shaft
		Panel(Spade - 0.14 * TagHigh, Spade + 0.14 * TagHigh, -0.2 * TagHigh, 0.0, Brassy); // the blade
		{
			const int32 BladeLeft = Staff.Add(OnFace(Spade - 0.14 * TagHigh, -0.2 * TagHigh), Out, Brassy);
			const int32 BladeRight = Staff.Add(OnFace(Spade + 0.14 * TagHigh, -0.2 * TagHigh), Out, Brassy);
			const int32 BladePoint = Staff.Add(OnFace(Spade, -0.33 * TagHigh), Out, Brassy);
			Staff.Face(BladeLeft, BladeRight, BladePoint, Out); // and its point
		}
		TagFigureAt = FVector(Pole.X - 0.16 * U, From + Pad + SpadeWide + Gap + Letters * 0.5, PlateAt.Z);
		if (bTick)
		{
			// The tick, finished: a short stroke down and a long one up, in a soft green.
			const FColor TickInk = FLinearColor(FColor::FromHex(TEXT("8CC27A"))).ToFColor(true);
			const double TickFrom = From + TagWide - Pad - TickWide;
			const FVector2D Stroke[3] = { FVector2D(TickFrom, 0.0), FVector2D(TickFrom + 0.32 * TickWide, -0.2 * TagHigh), FVector2D(TickFrom + TickWide, 0.24 * TagHigh) };
			for (int32 K = 0; K < 2; ++K)
			{
				const FVector2D Way = (Stroke[K + 1] - Stroke[K]).GetSafeNormal();
				const FVector2D Side = FVector2D(-Way.Y, Way.X) * (0.06 * TagHigh);
				const FVector2D Start = Stroke[K] - Way * (0.04 * TagHigh), Close = Stroke[K + 1] + Way * (0.04 * TagHigh);
				Staff.Quad(OnFace(Start.X - Side.X, Start.Y - Side.Y), OnFace(Close.X - Side.X, Close.Y - Side.Y), OnFace(Close.X + Side.X, Close.Y + Side.Y), OnFace(Start.X + Side.X, Start.Y + Side.Y), Out, TickInk);
			}
		}
	}

	// ---- (Release 2, the marked army; marked.ts; Brian, 24 November 2026 18:01Z) An enemy army that took one of his
	// towns wears a red tag on its pole until he beats it: hung on the side away from its flag, its top level with the
	// flag's, swallow-tailed at its free end, edged in near-black, and lettered with the town in cream capitals. A long
	// name is lettered smaller rather than run wider than the flag.
	// (Release 2, new units ready) His own new army wears the same tag in brass, lettered NEW in near-black.
	if (What == 0 && !bGlimpsed && (!MarkedTown.IsEmpty() || bTagNew))
	{
		const bool bMarkNew = MarkedTown.IsEmpty();
		const FString MarkShown = bMarkNew ? FString(TEXT("NEW")) : MarkedTown.ToUpper();
		const double MarkLetterWanted = PlateHeight * 0.5;
		const double MarkWanted = MarkLetterWanted * (0.9 + 0.62 * MarkShown.Len());
		const double MarkWide = FMath::Min(MarkWanted, FlagWidth * 1.1);
		const double MarkLetter = MarkLetterWanted * MarkWide / MarkWanted;
		const double MarkHigh = FMath::Max(MarkLetter * 1.55, MarkLetterWanted * 1.2);
		const double MarkTail = MarkHigh * 0.45;
		const double MarkNear = Pole.Y - 0.2 * U; // (against the pole)
		const FVector MarkAt(Pole.X, MarkNear - MarkWide * 0.5, FlagAt.Z + FlagHeight * 0.5 - MarkHigh * 0.5);
		const FLinearColor MarkRed = bMarkNew ? BrassPaint : FLinearColor(FColor(176, 30, 26)); // (brass for his own new army)
		const FLinearColor MarkEdge(FColor::FromHex(TEXT("15130F")));
		Staff.Box(MarkAt - StandardFoot, FVector(0.2 * U, MarkWide, MarkHigh), MarkRed);
		for (const double MarkEdgeAt : { -0.5, 0.5 })
		{
			Staff.Box(MarkAt - StandardFoot + FVector(0.0, 0.0, MarkHigh * MarkEdgeAt), FVector(0.24 * U, MarkWide + 0.06 * U, 0.1 * U), MarkEdge);
		}
		// The swallow tail: two red points beyond the free end, on the face toward the viewer (-X).
		const double MarkFace = Pole.X - 0.1 * U, MarkEnd = MarkNear - MarkWide;
		const FVector MarkOut(-1.0, 0.0, 0.0);
		const FColor MarkRedPoint = MarkRed.ToFColor(true);
		auto MarkPoint = [&](double Across, double Up) { return Staff.Add(FVector(MarkFace, Across, MarkAt.Z + Up) - StandardFoot, MarkOut, MarkRedPoint); };
		{
			const int32 TopIn = MarkPoint(MarkEnd + 0.01 * U, MarkHigh * 0.5), TopOut = MarkPoint(MarkEnd - MarkTail, MarkHigh * 0.5), Notch = MarkPoint(MarkEnd + 0.01 * U, 0.0);
			Staff.Face(TopIn, TopOut, Notch, MarkOut);
			const int32 LowIn = MarkPoint(MarkEnd + 0.01 * U, -MarkHigh * 0.5), LowOut = MarkPoint(MarkEnd - MarkTail, -MarkHigh * 0.5), Middle = MarkPoint(MarkEnd + 0.01 * U, 0.0);
			Staff.Face(Middle, LowOut, LowIn, MarkOut);
		}
		MarkLabel = NewObject<UTextRenderComponent>(this);
		MarkLabel->SetupAttachment(Standard);
		MarkLabel->SetText(FText::FromString(MarkShown));
		MarkLabel->SetWorldSize(static_cast<float>(MarkLetter));
		MarkLabel->SetTextRenderColor(FColor::FromHex(bMarkNew ? TEXT("15130F") : TEXT("F4F0E4")));
		if (UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")))
		{
			MarkLabel->SetTextMaterial(Unlit);
		}
		MarkLabel->SetHorizontalAlignment(EHTA_Center);
		MarkLabel->SetVerticalAlignment(EVRTA_TextCenter);
		MarkLabel->SetCastShadow(false);
		MarkLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarkLabel->SetRelativeLocation(MarkAt - StandardFoot - FVector(0.16 * U, 0.0, 0.0));
		MarkLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		MarkLabel->RegisterComponent();
	}
	// ---- (end release 2)

	// A small pennant under the plate for each ally whose corps march with this army.
	if (Cloth && What == 0)
	{
		TArray<FString> Allies;
		for (const FViewCorps& Unit : Corps)
		{
			if (!Unit.Ally.IsEmpty())
			{
				Allies.AddUnique(Unit.Ally);
			}
		}
		const double PennantWide = FlagWidth * 0.46, PennantHigh = PennantWide / 1.5;
		for (int32 Index = 0; Index < Allies.Num() && Index < 3; ++Index)
		{
			UMaterialInterface* Theirs = Map->GetFlagMaterial(Allies[Index]);
			if (!Theirs)
			{
				continue;
			}
			const double TopZ = PlateAt.Z - PlateHeight * 0.5 - 0.45 * U - Index * (PennantHigh + 0.3 * U);
			const double Near = Pole.Y + Hoist, Far = Near + PennantWide;
			const TArray<FVector> Corners = { FVector(Pole.X, Near, TopZ), FVector(Pole.X, Far, TopZ), FVector(Pole.X, Near, TopZ - PennantHigh), FVector(Pole.X, Far, TopZ - PennantHigh) };
			TArray<FVector> Facing;
			Facing.Init(FVector(-1.0, 0.0, 0.0), 4);
			const TArray<FVector2D> Cover = { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(1.0, 1.0) };
			TArray<FProcMeshTangent> Lie;
			Lie.Init(FProcMeshTangent(FVector(0.0, 1.0, 0.0), false), 4);
			Cloth->CreateMeshSection_LinearColor(Index + 1, Corners, { 0, 2, 1, 1, 2, 3 }, Facing, Cover, TArray<FLinearColor>(), Lie, false);
			Cloth->SetMaterial(Index + 1, Theirs);
		}
	}
	// R2 bargains hook: a volunteer legion's home pennant on the same pole, under the ally pennants (no corps has a home abroad in release 1)
	if (Cloth && What == 0 && !bGlimpsed)
	{
		BargainLayer::HangPennant(Map, Cloth, Corps, Pole.X, Pole.Y + Hoist, PlateAt.Z - PlateHeight * 0.5 - 0.45 * U, FlagWidth, U);
	}

	// The mesh is done: hand it over.
	Body = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), TEXT("Body"))); // (not still: its wake's sections are shown and hidden as it marches)
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->RegisterComponent();
	if (!bGlimpsed) // (ruling 154: an army he only glimpses has no base and no pieces)
	{
		// (Release 2, a fleet under way, second part: tc.Map.ShipRide.) Each ship of a fleet in a small mesh of her own,
		// about her own middle on the water, so that she can heel in her turn and ride the swell (Campaign/ShipRide.h);
		// her wake stays flat, in the body. Only where every point of the model is some ship's and every corner of a
		// ship's triangles is a point of her own: anything else is the one mesh, as before.
		int32 Covered = 0;
		bool bHulls = What == 1 && Model.Ships.Num() > 0 && ShipRideWanted()
			&& Model.Normals.Num() == Model.Vertices.Num() && Model.Colors.Num() == Model.Vertices.Num();
		for (int32 ShipIndex = 0; bHulls && ShipIndex < Model.Ships.Num(); ++ShipIndex)
		{
			const FMiniatureMesh::FShip& OneShip = Model.Ships[ShipIndex];
			bHulls = OneShip.FirstVertex >= 0 && OneShip.VertexCount > 0 && OneShip.FirstVertex + OneShip.VertexCount <= Model.Vertices.Num()
				&& OneShip.FirstIndex >= 0 && OneShip.IndexCount > 0 && OneShip.FirstIndex + OneShip.IndexCount <= Model.Triangles.Num();
			for (int32 CornerIndex = 0; bHulls && CornerIndex < OneShip.IndexCount; ++CornerIndex)
			{
				const int32 PointIndex = Model.Triangles[OneShip.FirstIndex + CornerIndex] - OneShip.FirstVertex;
				bHulls = PointIndex >= 0 && PointIndex < OneShip.VertexCount;
			}
			Covered += OneShip.VertexCount;
		}
		bHulls = bHulls && Covered == Model.Vertices.Num();
		if (bHulls)
		{
			for (int32 ShipIndex = 0; ShipIndex < Model.Ships.Num(); ++ShipIndex)
			{
				const FMiniatureMesh::FShip& OneShip = Model.Ships[ShipIndex];
				TArray<FVector> HullPoints, HullFacing;
				TArray<FColor> HullPaint;
				TArray<int32> HullFaces;
				HullPoints.Reserve(OneShip.VertexCount);
				HullFacing.Reserve(OneShip.VertexCount);
				HullPaint.Reserve(OneShip.VertexCount);
				HullFaces.Reserve(OneShip.IndexCount);
				for (int32 PointIndex = 0; PointIndex < OneShip.VertexCount; ++PointIndex)
				{
					HullPoints.Add(Model.Vertices[OneShip.FirstVertex + PointIndex] - OneShip.Middle); // (about her own middle)
					HullFacing.Add(Model.Normals[OneShip.FirstVertex + PointIndex]);
					HullPaint.Add(Model.Colors[OneShip.FirstVertex + PointIndex]);
				}
				for (int32 CornerIndex = 0; CornerIndex < OneShip.IndexCount; ++CornerIndex)
				{
					HullFaces.Add(Model.Triangles[OneShip.FirstIndex + CornerIndex] - OneShip.FirstVertex);
				}
				UProceduralMeshComponent* HullMesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), TEXT("Hull")));
				HullMesh->SetupAttachment(Root);
				HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				HullMesh->SetRelativeLocation(OneShip.Middle);
				HullMesh->RegisterComponent();
				HullMesh->CreateMeshSection(0, HullPoints, HullFaces, HullFacing, TArray<FVector2D>(), HullPaint, TArray<FProcMeshTangent>(), false);
				HullMesh->SetMaterial(0, Map->GetPieceMaterial());
				HullMesh->SetCastShadow(bCastsShadows);
				Hulls.Add(HullMesh);
				FHullRide& Ride = HullRides.AddDefaulted_GetRef();
				Ride.Middle = OneShip.Middle;
				Ride.Ahead = FVector2D(OneShip.Ahead.X, OneShip.Ahead.Y);
				Ride.Long = OneShip.Long;
				Ride.Beam = OneShip.Beam;
				Ride.Class = OneShip.Class;
				Ride.Phase = 1.7 * ShipIndex + 0.61 * (Fleet.Id % 53); // (no two ships of a fleet, and no two fleets side by side, move as one)
				if (HullsWere.IsValidIndex(ShipIndex) && HullsWere[ShipIndex].Class == Ride.Class)
				{
					Ride.HeelAngle = HullsWere[ShipIndex].HeelAngle;
					Ride.HeelRate = HullsWere[ShipIndex].HeelRate;
				}
			}
			RideHulls(0.0, 0.0, 0.0); // (set as they stood, with no time gone)
		}
		else
		{
			Body->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
			Body->SetMaterial(0, Map->GetPieceMaterial());
		}
	}
	// (a fleet at sea: its wake, foam lying on the water. Two of them, on patrol and on passage: the one that is not wanted is hidden.)
	bHasWake = false;
	WakeLaid.Reset();
	bWakeBent = false; // (a new mesh: its wake is as it was built)
	if (Model.Wake[0].Vertices.Num() > 0 && Map->GetWakeMaterial())
	{
		for (int32 Passage = 0; Passage < 2; ++Passage)
		{
			const FMiniatureMesh::FWakeMesh& Foam = Model.Wake[Passage];
			Body->CreateMeshSection(1 + Passage, Foam.Vertices, Foam.Triangles, Foam.Normals, Foam.UVs, Foam.Colors, TArray<FProcMeshTangent>(), false);
			Body->SetMaterial(1 + Passage, Map->GetWakeMaterial());
		}
		// (Release 2, a fleet under way.) The passage wake is kept as it was built, to be laid along her course. Her
		// ships lie in echelon about her middle, the sternmost stern nine of the piece's units astern of it (the
		// battleship's: FMiniatureMesh::Fleet): from there forward the piece turns as one.
		WakeLaid = Model.Wake[1].Vertices;
		WakeStiff = 9.0 * U;
		WakeReach = WakeStiff;
		for (const FVector& Point : WakeLaid)
		{
			WakeReach = FMath::Max(WakeReach, -Point.Y);
		}
		bHasWake = true;
		Body->SetMeshSectionVisible(1, !bMarching);
		Body->SetMeshSectionVisible(2, bMarching);
	}
	if (Turf.Vertices.Num() > 0 && !bGlimpsed)
	{
		Flock = UTheaterStillMesh::Make(this, TEXT("Flock"));
		Flock->SetupAttachment(Root);
		Flock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flock->RegisterComponent();
		Flock->CreateMeshSection(0, Turf.Vertices, Turf.Triangles, Turf.Normals, TArray<FVector2D>(), Turf.Colors, TArray<FProcMeshTangent>(), false);
		Flock->SetMaterial(0, Map->GetPieceMaterial());
		Flock->SetCastShadow(false);
		Flock->SetVisibility(bFlockNear);
	}
	if (DarkEarth.Vertices.Num() > 0 && !bGlimpsed)
	{
		FarEarth = UTheaterStillMesh::Make(this, TEXT("FarEarth"));
		FarEarth->SetupAttachment(Root);
		FarEarth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FarEarth->RegisterComponent();
		FarEarth->CreateMeshSection(0, DarkEarth.Vertices, DarkEarth.Triangles, DarkEarth.Normals, TArray<FVector2D>(), DarkEarth.Colors, TArray<FProcMeshTangent>(), false);
		FarEarth->SetMaterial(0, Map->GetPieceMaterial());
		FarEarth->SetCastShadow(false);
		FarEarth->SetVisibility(!bFlockNear);
	}
	Mast = UTheaterStillMesh::Make(this, TEXT("Mast"));
	Mast->SetupAttachment(Standard);
	Mast->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mast->RegisterComponent();
	Mast->CreateMeshSection(0, Staff.Vertices, Staff.Triangles, Staff.Normals, TArray<FVector2D>(), Staff.Colors, TArray<FProcMeshTangent>(), false);
	Mast->SetMaterial(0, Map->GetPieceMaterial());

	// Honours: a two-coloured streamer for each, one under another below the plate.
	int32 Streamer = 0;
	const double StreamerLength = 3.4 * U, StreamerDepth = 0.62 * U;
	for (const FViewCorps& Unit : Corps)
	{
		for (const FHonourRibbon& Honour : Unit.Honours)
		{
			if (Streamer >= 6)
			{
				break;
			}
			const double Z = PlateAt.Z - PlateHeight * 0.5 - 0.6 * U - Streamer * (StreamerDepth + 0.16 * U);
			AddPart(Cube, Pole - StandardFoot + FVector(0.0, Hoist + StreamerLength * 0.25, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.First), 0.6f, 0.0f, 0.6f), Standard)->SetCastShadow(false);
			AddPart(Cube, Pole - StandardFoot + FVector(0.0, Hoist + StreamerLength * 0.75, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.Second), 0.6f, 0.0f, 0.6f), Standard)->SetCastShadow(false);
			++Streamer;
		}
	}

	// The figure. The lettering faces back along -X, toward the viewer.
	StrengthLabel = NewObject<UTextRenderComponent>(this);
	StrengthLabel->SetupAttachment(Standard);
	StrengthLabel->SetText(FText::FromString(Figure));
	StrengthLabel->SetWorldSize(static_cast<float>(PlateHeight * 0.86));
	StrengthLabel->SetTextRenderColor(FColor::FromHex(TEXT("F4F0E4")));
	// The side of the plate that faces the viewer is often the side away from the lamp, so the figure
	// is drawn in lettering that does not take the light: it reads the same from every side.
	if (UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")))
	{
		StrengthLabel->SetTextMaterial(Unlit);
	}
	StrengthLabel->SetHorizontalAlignment(EHTA_Center);
	StrengthLabel->SetVerticalAlignment(EVRTA_TextCenter);
	StrengthLabel->SetCastShadow(false);
	StrengthLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StrengthLabel->SetRelativeLocation(PlateAt - StandardFoot - FVector(0.16 * U, 0.0, 0.0));
	StrengthLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	StrengthLabel->RegisterComponent();
	// The months on the spade tag, lettered as the strength is.
	if (!TagFigure.IsEmpty())
	{
		WorksLabel = NewObject<UTextRenderComponent>(this);
		WorksLabel->SetupAttachment(Standard);
		WorksLabel->SetText(FText::FromString(TagFigure));
		WorksLabel->SetWorldSize(static_cast<float>(TagHigh * 0.86));
		WorksLabel->SetTextRenderColor(FColor::FromHex(TEXT("F4F0E4")));
		if (UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")))
		{
			WorksLabel->SetTextMaterial(Unlit);
		}
		WorksLabel->SetHorizontalAlignment(EHTA_Center);
		WorksLabel->SetVerticalAlignment(EVRTA_TextCenter);
		WorksLabel->SetCastShadow(false);
		WorksLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorksLabel->SetRelativeLocation(TagFigureAt - StandardFoot);
		WorksLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		WorksLabel->RegisterComponent();
	}
	// (Release 2, nicknames.ts) The name its corps have earned, in quotes, on a dark scroll with brass edges over the top
	// of the flag, clear of the finial: lettered in old gold, so that it is never read as the strength. A long name is
	// lettered smaller rather than run wider than the flag and a half, which would lie over the next army's flag.
	if (!EarnedName.IsEmpty() && What == 0)
	{
		const FString Shown = FString::Printf(TEXT("\"%s\""), *EarnedName); // (straight quotes: the engine's own text font is sure to have them)
		const double Letter = PlateHeight * 0.6;
		const double Wanted = Letter * (0.9 + 0.56 * Shown.Len());
		const double Widest = FlagWidth * 1.5;
		const double Scroll = FMath::Min(Wanted, Widest);
		const double Lettered = Letter * Scroll / Wanted;
		const double High = FMath::Max(Lettered * 1.45, Letter * 0.95);
		const FVector ScrollAt = Pole + FVector(0.0, Hoist + FlagWidth * 0.5, Top + PoleHeight + 1.9 * U + High * 0.5);
		AddPart(Cube, ScrollAt - StandardFoot, FVector(0.16 * U, Scroll + 0.3 * U, High), Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("15130F"))), 0.7f, 0.0f, 0.6f), Standard)->SetCastShadow(false);
		for (const double Edge : { -1.0, 1.0 })
		{
			AddPart(Cube, ScrollAt - StandardFoot + FVector(0.0, 0.0, Edge * High * 0.5), FVector(0.22 * U, Scroll + 0.4 * U, 0.12 * U),
				Map->GetSolidMaterial(BrassPaint, 0.4f, 0.8f, 0.5f), Standard)->SetCastShadow(false);
		}
		NameLabel = NewObject<UTextRenderComponent>(this);
		NameLabel->SetupAttachment(Standard);
		NameLabel->SetText(FText::FromString(Shown));
		NameLabel->SetWorldSize(static_cast<float>(Lettered));
		NameLabel->SetTextRenderColor(FColor::FromHex(TEXT("E9C77B")));
		if (UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")))
		{
			NameLabel->SetTextMaterial(Unlit);
		}
		NameLabel->SetHorizontalAlignment(EHTA_Center);
		NameLabel->SetVerticalAlignment(EVRTA_TextCenter);
		NameLabel->SetCastShadow(false);
		NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NameLabel->SetRelativeLocation(ScrollAt - StandardFoot - FVector(0.12 * U, 0.0, 0.0));
		NameLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		NameLabel->RegisterComponent();
	}
	ApplyStandard(); // (a piece rebuilt stands as its neighbours stand)
}

void AArmyPiece::SetStandard(float Scale, float TiltDegrees)
{
	if (FMath::Abs(Scale - StandardScale) < 0.004f && FMath::Abs(TiltDegrees - StandardTilt) < 0.2f)
	{
		return;
	}
	StandardScale = Scale;
	StandardTilt = TiltDegrees;
	ApplyStandard();
}

void AArmyPiece::SetSize(float Size)
{
	if (FMath::Abs(GetActorScale3D().X - Size) > 0.001)
	{
		SetActorScale3D(FVector(Size));
		ApplyStandard();
	}
}

void AArmyPiece::ApplyStandard()
{
	if (Standard)
	{
		const double Own = FMath::Max(GetActorScale3D().X, 0.1);
		Standard->SetRelativeScale3D(FVector(StandardScale / Own));
		// (leaning back, away from whoever looks: its face, which looks back along -X, turns up toward him)
		Standard->SetRelativeRotation(FRotator(-StandardTilt, 0.0f, 0.0f));
		// (a standard drawn larger than life throws no shadow: a shadow that size would not be the flag's own,
		// and from high up it lay across the map as a dark bar beside every army)
		const bool bShadow = StandardScale < 1.12f && !bGlimpsed; // (and a flag at a glimpse throws none)
		if (Cloth && Cloth->CastShadow != bShadow)
		{
			Cloth->SetCastShadow(bShadow);
		}
		if (Mast && Mast->CastShadow != bShadow)
		{
			Mast->SetCastShadow(bShadow);
		}
	}
}

FVector AArmyPiece::OnStandard(const FVector& Point) const
{
	const double Own = FMath::Max(GetActorScale3D().X, 0.1);
	return StandardFoot + FRotator(-StandardTilt, 0.0f, 0.0f).RotateVector((Point - StandardFoot) * (StandardScale / Own));
}

void AArmyPiece::ShowRing()
{
	if (SelectionRing)
	{
		SelectionRing->SetVisibility(bSelected || bHovered);
	}
}

FVector AArmyPiece::HomeLocation() const
{
	// A piece in the hand is held a little off the table.
	return RestLocation + FVector(0.0, 0.0, bSelected ? LiftHeight * 0.3 : 0.0);
}

FVector AArmyPiece::Shown(const FVector& Place) const
{
	return bRoundToEye && RoundSpan > 0.0 ? FVector(Place.X, Place.Y + FMath::RoundToDouble((RoundEye - Place.Y) / RoundSpan) * RoundSpan, Place.Z) : Place;
}

FVector AArmyPiece::GetReckonedLocation() const
{
	const FVector At = GetActorLocation();
	return bRoundToEye && RoundSpan > 0.0 ? FVector(At.X, At.Y + FMath::RoundToDouble((RestLocation.Y - At.Y) / RoundSpan) * RoundSpan, At.Z) : At;
}

void AArmyPiece::LookFrom(double EyeAlong, bool bOn)
{
	const bool bWas = bRoundToEye;
	RoundEye = EyeAlong;
	bRoundToEye = bOn && RoundSpan > 0.0;
	if (bRoundToEye)
	{
		// (A piece now more than half a world from the eye comes round to the eye's side. It is never in sight as it
		// does: the high view reaches about a seventh of the way round.)
		const FVector At = GetActorLocation();
		const double Round = FMath::RoundToDouble((RoundEye - At.Y) / RoundSpan) * RoundSpan;
		if (Round != 0.0)
		{
			SetActorLocation(FVector(At.X, At.Y + Round, At.Z));
		}
	}
	else if (bWas && !bMarching)
	{
		SetActorLocation(HomeLocation()); // (put off: back to where it is laid)
	}
}

void AArmyPiece::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	ShowRing();
	if (!bMarching)
	{
		SetActorLocation(Shown(HomeLocation()));
	}
}

void AArmyPiece::SetHovered(bool bInHovered)
{
	bHovered = bInHovered;
	ShowRing();
}

void AArmyPiece::SetRestLocation(const FVector& Location)
{
	RestLocation = Location;
	if (!bMarching)
	{
		SetActorLocation(Shown(HomeLocation()));
	}
}

bool AArmyPiece::HitByRay(const FVector& Origin, const FVector& Direction, double& OutDistance) const
{
	const FVector End = Origin + Direction * 200000.0;
	const FTransform& Place = GetActorTransform();
	auto Near = [&](const FVector& A, const FVector& B, double Radius) -> bool
	{
		FVector OnRay, OnPart;
		FMath::SegmentDistToSegmentSafe(Origin, End, A, B, OnRay, OnPart);
		if (FVector::DistSquared(OnRay, OnPart) > Radius * Radius)
		{
			return false;
		}
		OutDistance = FVector::Dist(Origin, OnRay);
		return true;
	};
	// The colours first (they are what the eye goes to), then the pole and the stack of counters.
	const FVector Flag = Place.TransformPosition(OnStandard(FlagCenter));
	return Near(Flag, Flag, FlagRadius * StandardScale)
		|| Near(Place.TransformPosition(PoleFoot), Place.TransformPosition(OnStandard(PoleFoot + FVector(0.0, 0.0, PoleTop))), BaseRadius * 0.3)
		|| Near(Place.TransformPosition(FVector::ZeroVector), Place.TransformPosition(FVector(0.0, 0.0, BodyTop)), BaseRadius);
}

bool AArmyPiece::ColoursHitByRay(const FVector& Origin, const FVector& Direction) const
{
	// (1.5.1 and release 2; Brian, 24 November 2026: "i cant click on the london airfield".) The colours alone, as HitByRay
	// measures them: inside an airfield's ring another country's piece keeps the pointer only on its flag.
	const FVector End = Origin + Direction * 200000.0;
	const FVector Flag = GetActorTransform().TransformPosition(OnStandard(FlagCenter));
	FVector OnRay, OnPart;
	FMath::SegmentDistToSegmentSafe(Origin, End, Flag, Flag, OnRay, OnPart);
	return FVector::DistSquared(OnRay, OnPart) <= FMath::Square(FlagRadius * StandardScale);
}

void AArmyPiece::Carry(const FVector& GroundPoint)
{
	bMarching = false;
	// (Release 2, armies with weight.) In the hand it sways as the hand moves it, so it is ticked there: Sway. Never a fleet.
	const bool bSwaysInHand = What != 1 && CarriedWithWeight();
	SetActorTickEnabled(bSwaysInHand);
	if (!bInHand)
	{
		bInHand = true;
		LevelHulls(); // (a fleet in the hand is off the water: her ships lie level)
		HandWas = GroundPoint; // (taken up: the hand begins here)
	}
	HandNow = GroundPoint;
	if (bSwaysInHand)
	{
		if (FVector::DistSquared2D(HandWas, HandNow) > FMath::Square(12.0 * LiftHeight))
		{
			HandWas = HandNow; // (the table came round under the hand, or the hand jumped: not a throw)
		}
		bHangs = true;
		StandLeaning(GroundPoint + FVector(0.0, 0.0, LiftHeight), false);
		return;
	}
	if (bHangs)
	{
		StandStraight(); // (the switch was turned off while it leaned)
	}
	SetActorLocation(GroundPoint + FVector(0.0, 0.0, LiftHeight));
}

void AArmyPiece::ReturnToRest()
{
	// (the far side: from where it is in its own reckoning, which is where it is shown on any table with edges)
	const FVector From = GetReckonedLocation();
	bInHand = false; // (let go)
	if (!bMarching && !From.Equals(HomeLocation(), 1.0))
	{
		MarchAlong({ From, RestLocation }, false, false);
	}
}

void AArmyPiece::MarchAlong(const TArray<FVector>& Waypoints, bool bInRemoveOnArrival, bool bLifted)
{
	if (Waypoints.Num() < 2)
	{
		if (bInRemoveOnArrival)
		{
			Destroy();
		}
		return;
	}
	Route = Waypoints;
	bInHand = false; // (set down, or sent on her way)
	// (Release 2, a fleet under way: a march, or a sail corner to corner, is no course of her own. SailCourse sets it after.)
	SailAt.Reset();
	SailHead.Reset();
	SailRan.Reset();
	StraightenWake();
	if (RoundSpan > 0.0)
	{
		// (Release 2, the far side.) The short way round: each point is counted in the turn of the world nearest the
		// one before it, so the way may run on past the seam. Tick brings the piece itself round as it crosses.
		for (int32 i = 1; i < Route.Num(); ++i)
		{
			Route[i].Y += FMath::RoundToDouble((Route[i - 1].Y - Route[i].Y) / RoundSpan) * RoundSpan;
		}
	}
	bSailing = false; // (SailAlong sets it again after)
	// (Release 2, armies with weight.) Never a fleet: she has a ship's way of her own (SailCourse), and her ships their ride.
	bWeighty = What != 1 && CarriedWithWeight();
	if (!bWeighty && bHangs)
	{
		StandStraight(); // (the switch was turned off while it leaned)
	}
	if (bWeighty && bLifted && Route.Num() >= 3)
	{
		// The hand does not carry it to the middle of each province and turn there: it cuts each corner on a curve.
		std::vector<TheaterArmyCarry::FSpot> Road, Rounded;
		Road.reserve(static_cast<size_t>(Route.Num()));
		for (const FVector& Corner : Route)
		{
			Road.push_back(TheaterArmyCarry::FSpot{ Corner.X, Corner.Y, Corner.Z });
		}
		// (Over 1 the measure also lets the curve reach further back along each leg: a wider sweep, for the Mac's film to try.)
		const double RoundBy = FMath::Clamp(static_cast<double>(CVarArmyRound.GetValueOnGameThread()), 0.0, 3.0);
		TheaterArmyCarry::Round(Road, 0.8 * RoundBy, 9.0 * FMath::Max(RoundBy, 1.0) * LiftHeight / 3.2, Rounded);
		if (Rounded.size() >= 2)
		{
			Route.Reset(static_cast<int32>(Rounded.size()));
			for (const TheaterArmyCarry::FSpot& Spot : Rounded)
			{
				Route.Add(FVector(Spot.X, Spot.Y, Spot.Z));
			}
		}
	}
	RouteMarks.Reset(Route.Num());
	double Length = 0.0;
	RouteMarks.Add(0.0);
	for (int32 i = 1; i < Route.Num(); ++i)
	{
		Length += FVector::Dist(Route[i - 1], Route[i]);
		RouteMarks.Add(Length);
	}
	// A short hop takes most of a second; a long march not much more than two. Setting down from the hand is quick.
	bLiftOnMarch = bLifted;
	MarchTime = bLifted ? FMath::Clamp(0.55 + Length / 5200.0, 0.75, 2.4) : FMath::Clamp(0.22 + Length / 9000.0, 0.28, 0.7);
	if (bWeighty)
	{
		MarchTime *= FMath::Clamp(static_cast<double>(CVarArmyTime.GetValueOnGameThread()), 0.7, 1.6);
		bHangs = true;
		const FVector Whole = Route.Last() - Route[0];
		CameAlong = FVector2D(Whole.X, Whole.Y).GetSafeNormal();
	}
	MarchClock = 0.0;
	bMarching = true;
	bRemoveOnArrival = bInRemoveOnArrival;
	if (bHasWake && Body)
	{
		// (a fleet on passage between seas throws a longer wake)
		Body->SetMeshSectionVisible(1, false);
		Body->SetMeshSectionVisible(2, true);
	}
	SetActorLocation(Shown(Route[0]));
	SetActorTickEnabled(true);
}

void AArmyPiece::SailAlong(const TArray<FVector>& Waypoints, float TableYaw)
{
	MarchAlong(Waypoints, false, false);
	if (!bMarching || RouteMarks.Num() < 2)
	{
		return;
	}
	bSailing = true;
	SailEndYaw = TableYaw;
	// (slower than a piece carried by hand: the eye should follow the ships out of one sea and into the next)
	MarchTime = FMath::Clamp(1.1 + RouteMarks.Last() / 3800.0, 1.4, 3.6);
}

void AArmyPiece::SailCourse(const TArray<FVector>& At, const TArray<FVector2D>& Head, const TArray<double>& Ran, double Each, float TableYaw)
{
	if (At.Num() < 2 || Head.Num() != At.Num() || Ran.Num() != At.Num() || Each <= 0.0)
	{
		return;
	}
	// (as a piece glides: under way, not lifted, its passage wake shown)
	MarchAlong({ At[0], At.Last() }, false, false);
	if (!bMarching)
	{
		return;
	}
	bSailing = true;
	SailEndYaw = TableYaw;
	SailLaidYaw = TableYaw;
	SailAt = At;
	SailHead = Head;
	SailRan = Ran;
	SailEach = Each;
	MarchTime = (At.Num() - 1) * Each;
	// (Second part: the furthest she goes in a tick is her full speed, which her ships' heel is measured by.)
	SailFull = 0.0;
	for (int32 K = 1; K < At.Num(); ++K)
	{
		SailFull = FMath::Max(SailFull, FVector::Dist2D(At[K - 1], At[K]));
	}
}

void AArmyPiece::BendWake(double Ran, const FVector2D& Bows, double Progress)
{
	if (!bHasWake || !Body || WakeLaid.Num() == 0 || SailAt.Num() < 2)
	{
		return;
	}
	// The water astern of her sterns, station by station back along her course, and each point of the wake laid on it.
	const double Scale = GetActorScale3D().X;
	static TheaterShipWay::FAstern Astern; // (the game's thread only)
	TheaterShipWay::LookAstern(SailAt.GetData(), SailHead.GetData(), SailRan.GetData(), SailAt.Num(), Ran, FMath::Max((WakeReach - WakeStiff) * Scale, 1.0), 128, Astern);
	const double Settle = TheaterShipWay::Straighten(Progress);
	const TheaterShipWay::FSpot Head{ Bows.X, Bows.Y };
	WakeBent.SetNumUninitialized(WakeLaid.Num());
	for (int32 K = 0; K < WakeLaid.Num(); ++K)
	{
		double X = 0.0, Y = 0.0;
		TheaterShipWay::Trail(Astern, Head, Scale, Settle, WakeStiff, WakeLaid[K].X, WakeLaid[K].Y, X, Y);
		WakeBent[K] = FVector(X, Y, WakeLaid[K].Z);
	}
	Body->UpdateMeshSection(2, WakeBent, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
	bWakeBent = true;
}

void AArmyPiece::StraightenWake()
{
	if (bWakeBent && Body && WakeLaid.Num() > 0)
	{
		Body->UpdateMeshSection(2, WakeLaid, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
	}
	bWakeBent = false;
}

void AArmyPiece::SetEye(double EyeDistance, bool bByFocus)
{
	if (Hulls.Num() == 0)
	{
		bRideNear = false;
		return;
	}
	bRideNear = bByFocus && !IsHidden() && EyeDistance < FMath::Max(static_cast<double>(CVarShipRideReach.GetValueOnGameThread()), 0.0);
	if (bRideNear && !bInHand && !bSelected && !IsActorTickEnabled())
	{
		// (lying still, she is ticked while there is a swell to see: Tick turns it off again. Not one in the hand or held
		// off the table, who would have her tick turned on here and off there at every look: the 3D pre-check's note.)
		SetActorTickEnabled(true);
	}
}

bool AArmyPiece::RideHulls(double Seconds, double Turn, double Pace)
{
	if (Hulls.Num() == 0 || Hulls.Num() != HullRides.Num())
	{
		return false;
	}
	// Only where the eye is near, and only on the water: not in the hand, nor held off the table, nor carried over it.
	const bool bSee = bRideNear && !bInHand && !bSelected && (!bMarching || bSailing);
	const double Span = FMath::Clamp(Seconds, 0.0, 0.25);
	RideMuch = FMath::Clamp(RideMuch + (bSee ? 1.2 : -1.2) * Span, 0.0, 1.0);
	if (!bSee && RideMuch <= 0.0)
	{
		LevelHulls(); // (nobody is near enough to see: she lies level, and costs nothing)
		return false;
	}
	const double HeelBy = FMath::Clamp(static_cast<double>(CVarShipHeel.GetValueOnGameThread()), 0.0, 2.0);
	// (a fleet at anchor by her port lies in sheltered water)
	const double SwellBy = FMath::Clamp(static_cast<double>(CVarShipSwell.GetValueOnGameThread()), 0.0, 3.0) * (bAtAnchor ? 0.35 : 1.0);
	const UWorld* World = GetWorld();
	const double Clock = World ? World->GetTimeSeconds() : 0.0;
	for (int32 Index = 0; Index < Hulls.Num(); ++Index)
	{
		FHullRide& Ride = HullRides[Index];
		TheaterShipRide::FKind Kind = TheaterShipRide::KindOf(Ride.Class);
		Kind.Heel *= HeelBy;
		Kind.Pitch *= SwellBy;
		Kind.Roll *= SwellBy;
		Kind.Heave *= SwellBy;
		// Her heel: toward what her turn and her speed ask of her while the eye is near, and back upright as it leaves.
		TheaterShipRide::FHeel Heel;
		Heel.Angle = Ride.HeelAngle;
		Heel.Rate = Ride.HeelRate;
		TheaterShipRide::Lean(Heel, bSee ? TheaterShipRide::HeelWanted(Kind, Turn, Pace) : 0.0, Span, Kind);
		Ride.HeelAngle = Heel.Angle;
		Ride.HeelRate = Heel.Rate;
		const TheaterShipRide::FSea Sea = TheaterShipRide::Swell(Kind, Clock, Ride.Phase, RideMuch);
		const TheaterShipRide::FSet Lie = TheaterShipRide::Set(Kind, Ride.Long, Ride.Beam, Ride.Ahead.X, Ride.Ahead.Y, Ride.HeelAngle, Sea, RideMuch);
		if (UProceduralMeshComponent* HullMesh = Hulls[Index])
		{
			// (about her own middle on the water: her masts along Lie's up, with no turn of her head in it)
			HullMesh->SetRelativeLocationAndRotation(Ride.Middle + FVector(0.0, 0.0, Lie.Rise), FQuat::FindBetweenNormals(FVector::UpVector, FVector(Lie.UpX, Lie.UpY, Lie.UpZ)));
		}
	}
	bHullsLevel = false;
	return true;
}

void AArmyPiece::LevelHulls()
{
	RideMuch = 0.0;
	for (FHullRide& Ride : HullRides)
	{
		Ride.HeelAngle = 0.0;
		Ride.HeelRate = 0.0;
	}
	if (!bHullsLevel)
	{
		for (int32 Index = 0; Index < Hulls.Num() && Index < HullRides.Num(); ++Index)
		{
			if (UProceduralMeshComponent* HullMesh = Hulls[Index])
			{
				HullMesh->SetRelativeLocationAndRotation(HullRides[Index].Middle, FQuat::Identity);
			}
		}
		bHullsLevel = true;
	}
}

void AArmyPiece::FaceTable(float Yaw)
{
	SailEndYaw = Yaw;
	if (!IsSailing())
	{
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

void AArmyPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMarching)
	{
		// (Release 2, a fleet under way, second part.) A fleet lying still is ticked only for as long as her ships have
		// something to show: the last of a heel, or the swell while the eye is near. (Armies with weight.) And a piece
		// only while it sways in the hand or rocks after a set-down.
		const bool bRides = RideHulls(DeltaSeconds, 0.0, 0.0);
		const bool bSways = Sway(DeltaSeconds);
		if (!bRides && !bSways)
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	MarchClock += DeltaSeconds;
	const double Progress = FMath::Clamp(MarchClock / MarchTime, 0.0, 1.0);
	// Ease away from the start and into the finish, as a hand moves a piece.
	const double Eased = Progress * Progress * (3.0 - 2.0 * Progress);
	// (Release 2, a fleet under way.) On a course of her own she is where her times say, and heads as they say: the
	// course has her weight in it already.
	const bool bCourse = bSailing && SailAt.Num() >= 2;
	FVector Position;
	FVector2D CourseBows(0.0, 1.0);
	double CourseRan = 0.0;
	// (Second part.) How fast her head swings, to starboard positive, in radians a second, and her speed as a share of
	// her full speed: what her ships heel by. Nothing under the old sail, corner to corner: there they only ride.
	double CourseTurn = 0.0, CoursePace = 0.0;
	int32 Segment = 1;
	TheaterArmyCarry::FHeld Held;
	if (bCourse)
	{
		const double Beat = FMath::Clamp(MarchClock / SailEach, 0.0, static_cast<double>(SailAt.Num() - 1));
		const int32 Before = FMath::Min(FMath::FloorToInt32(Beat), SailAt.Num() - 2);
		const double Between = Beat - Before;
		Position = FMath::Lerp(SailAt[Before], SailAt[Before + 1], Between);
		const FVector2D Mixed = SailHead[Before] + (SailHead[Before + 1] - SailHead[Before]) * Between;
		CourseBows = Mixed.SizeSquared() > 1.0e-8 ? Mixed.GetSafeNormal() : SailHead[Before + 1];
		CourseRan = FMath::Lerp(SailRan[Before], SailRan[Before + 1], Between);
		if (Hulls.Num() > 0)
		{
			const double HeadBefore = FMath::Atan2(-SailHead[Before].X, SailHead[Before].Y), HeadAfter = FMath::Atan2(-SailHead[Before + 1].X, SailHead[Before + 1].Y);
			CourseTurn = FMath::Atan2(FMath::Sin(HeadAfter - HeadBefore), FMath::Cos(HeadAfter - HeadBefore)) / SailEach;
			CoursePace = SailFull > 1.0e-6 ? FVector::Dist2D(SailAt[Before], SailAt[Before + 1]) / SailFull : 0.0;
		}
	}
	else
	{
		// (Armies with weight: where the hand has it, by the carry and not by one ease from end to end.)
		if (bWeighty)
		{
			Held = bLiftOnMarch ? TheaterArmyCarry::Lifted(Progress, MarchTime) : TheaterArmyCarry::FromHand(Progress);
		}
		const double Target = (bWeighty ? Held.Along : Eased) * RouteMarks.Last();
		while (Segment < RouteMarks.Num() - 1 && RouteMarks[Segment] < Target)
		{
			++Segment;
		}
		const double Span = FMath::Max(RouteMarks[Segment] - RouteMarks[Segment - 1], 1.0e-3);
		Position = FMath::Lerp(Route[Segment - 1], Route[Segment], (Target - RouteMarks[Segment - 1]) / Span);
	}
	// Picked up, carried, set down.
	if (bLiftOnMarch)
	{
		Position.Z += LiftHeight * (bWeighty ? Held.High : FMath::Pow(FMath::Sin(Progress * UE_DOUBLE_PI), 0.55));
	}
	else if (bWeighty && !bCourse && Route.Num() == 2)
	{
		Position.Z = Route[1].Z + (Route[0].Z - Route[1].Z) * Held.High; // (set down from the hand: over its place first, then let down)
	}
	// A piece that is still in the hand when it arrives comes to rest a little off the table.
	Position += (HomeLocation() - RestLocation) * Eased;
	if (RoundSpan > 0.0 && !bRoundToEye && (bSailing || bLiftOnMarch))
	{
		// (Release 2, the far side.) Kept in its own turn of the world: over the seam, it comes round, and is seen to
		// leave on the one side and to come in on the other. (Not a piece gliding back from the hand, which may have
		// been carried a little over the seam: it glides straight back as it was seen to go.)
		Position.Y -= FMath::FloorToDouble((Position.Y - RoundWest) / RoundSpan) * RoundSpan;
	}
	// (Seam-2: or shown in the turn nearest the eye, where the map has said so. Then it does not leave the table at the
	// seam at all: it sails on across it, and the eye comes round with it.)
	if (bWeighty && !bCourse)
	{
		// (Armies with weight.) Its hang over this frame, with the hand where the road and the carry have it: the foot
		// hangs back as the hand gathers speed, swings on as it slows, and swings wide round a bend.
		auto HandAt = [this](double Time)
		{
			const double Share = FMath::Clamp(Time / MarchTime, 0.0, 1.0);
			const double Gone = (bLiftOnMarch ? TheaterArmyCarry::Lifted(Share, MarchTime).Along : TheaterArmyCarry::FromHand(Share).Along) * RouteMarks.Last();
			int32 Leg = 1;
			while (Leg < RouteMarks.Num() - 1 && RouteMarks[Leg] < Gone)
			{
				++Leg;
			}
			const double LegLong = FMath::Max(RouteMarks[Leg] - RouteMarks[Leg - 1], 1.0e-3);
			const FVector Hand = FMath::Lerp(Route[Leg - 1], Route[Leg], (Gone - RouteMarks[Leg - 1]) / LegLong);
			return TheaterArmyCarry::FSpot{ Hand.X, Hand.Y, 0.0 };
		};
		TheaterArmyCarry::FHang Hang = HangOf(HangKept);
		const double Reach = TheaterArmyCarry::HangsOn * LiftHeight / 3.2;
		const double Until = FMath::Min(MarchClock, MarchTime), Since = FMath::Max(Until - static_cast<double>(DeltaSeconds), 0.0);
		if (bLiftOnMarch)
		{
			TheaterArmyCarry::Carried(Hang, HandAt, Since, Until, MarchTime, Reach);
		}
		else
		{
			TheaterArmyCarry::Swing(Hang, HandAt, Since, Until, TheaterArmyCarry::ComingDown, Reach);
		}
		KeepHang(HangKept, Hang);
		if (FMath::Square(Hang.HandRateX) + FMath::Square(Hang.HandRateY) > 1.0)
		{
			CameAlong = FVector2D(Hang.HandRateX, Hang.HandRateY).GetSafeNormal();
		}
		StandLeaning(Shown(Position), false);
	}
	else
	{
		SetActorLocation(Shown(Position));
	}
	if (bCourse)
	{
		// Her bows as her course has them (a fleet's model has its bows toward its own +Y). The table may have been
		// turned to a new eye since the course was laid: what it has turned by comes on over the last fifth of her time,
		// so she still comes to rest facing it, with no snap.
		const double Late = FMath::SmoothStep(0.8, 1.0, Progress);
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(-CourseBows.X, CourseBows.Y)) + FRotator::NormalizeAxis(SailEndYaw - SailLaidYaw) * Late;
		SetActorRotation(FRotator(0.0, Yaw, 0.0));
		const double Turned = FMath::DegreesToRadians(Yaw);
		BendWake(CourseRan, FVector2D(-FMath::Sin(Turned), FMath::Cos(Turned)), Progress);
	}
	else if (bSailing)
	{
		// Bows the way she goes (a fleet's model has its bows toward its own +Y), and round to face the table again as
		// she comes in: turned a little each frame, never snapped.
		const FVector Heading = Route[Segment] - Route[Segment - 1];
		double Want = SailEndYaw;
		if (Progress < 0.84 && Heading.SizeSquared2D() > 1.0e-4)
		{
			Want = FMath::RadiansToDegrees(FMath::Atan2(-Heading.X, Heading.Y));
		}
		const double Now = GetActorRotation().Yaw;
		const double Turn = FRotator::NormalizeAxis(Want - Now);
		// (The last turn, round to the table, quickens as the time left runs out: on the shortest sail it is all but
		// done by the time she comes in, and what is left at the end is too little to see snap.)
		const double Rate = Progress < 0.84 ? 5.0 : FMath::Max(5.0, 3.0 / FMath::Max(0.05, MarchTime * (1.0 - Progress)));
		SetActorRotation(FRotator(0.0, Now + Turn * FMath::Min(1.0, DeltaSeconds * Rate), 0.0));
	}
	if (Hulls.Num() > 0)
	{
		RideHulls(DeltaSeconds, CourseTurn, CoursePace); // (second part: her ships heel outward as her head swings, and ride the swell)
	}

	if (Progress >= 1.0)
	{
		bMarching = false;
		// (her ships may have a heel still to lose, or the swell to ride: the next tick says. And a piece set down with
		// weight has its rock to make.)
		SetActorTickEnabled((Hulls.Num() > 0 || bWeighty) && !bRemoveOnArrival);
		if (bSailing)
		{
			bSailing = false;
			SetActorRotation(FRotator(0.0f, SailEndYaw, 0.0f));
		}
		StraightenWake();
		if (bHasWake && Body)
		{
			Body->SetMeshSectionVisible(1, true);
			Body->SetMeshSectionVisible(2, false);
		}
		if (bRemoveOnArrival)
		{
			Destroy();
		}
		else if (bWeighty)
		{
			// (Armies with weight.) It lands: its top is thrown on the way it came, and it rocks once (Sway). A piece
			// still in the hand as it arrives stays a little off the table, and nothing knocks it.
			if (!bSelected)
			{
				TheaterArmyCarry::FHang Hang = HangOf(HangKept);
				TheaterArmyCarry::Kick(Hang, CameAlong.X, CameAlong.Y, TheaterArmyCarry::Landing, TheaterArmyCarry::HangsOn * LiftHeight / 3.2);
				KeepHang(HangKept, Hang);
			}
			bHangs = true;
			StandLeaning(Shown(HomeLocation()), !bSelected);
		}
		else
		{
			SetActorLocation(Shown(HomeLocation()));
		}
	}
}

bool AArmyPiece::CarriedWithWeight()
{
	static const bool bArmyWeightAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterArmyWeight"));
	return bArmyWeightAsked || CVarArmyWeight.GetValueOnGameThread() != 0;
}

float AArmyPiece::FacingNow()
{
	// The map turns every piece to face the eye by setting it upright at a new yaw. So a piece that stands upright
	// faces as the map last said; one that leans faces as it did when it began to.
	const FRotator Turned = GetActorRotation();
	if (!bStoodKnown || (FMath::Abs(Turned.Pitch) < 0.01 && FMath::Abs(Turned.Roll) < 0.01))
	{
		StoodYaw = static_cast<float>(Turned.Yaw);
		bStoodKnown = true;
	}
	return StoodYaw;
}

void AArmyPiece::StandLeaning(const FVector& Place, bool bOnTable)
{
	const float Faces = FacingNow();
	// (on the table it turns about the low edge of its base, which stays where it is: so its middle rises a little)
	double Risen = 0.0;
	const FQuat Leaning = LeaningTurn(HangKept, TheaterArmyCarry::HangsOn * LiftHeight / 3.2, bOnTable ? BaseRadius * GetActorScale3D().X : 0.0, Faces, Risen);
	SetActorLocationAndRotation(Place + FVector(0.0, 0.0, Risen), Leaning);
}

void AArmyPiece::FaceEye(float Yaw)
{
	StoodYaw = Yaw;
	bStoodKnown = true;
	if (!bHangs)
	{
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
		return;
	}
	// (it leans: the same lean, facing the new way. Its place is not touched: the rise does not change with the facing.)
	double Risen = 0.0;
	SetActorRotation(LeaningTurn(HangKept, TheaterArmyCarry::HangsOn * LiftHeight / 3.2, 0.0, Yaw, Risen));
}

void AArmyPiece::StandStraight()
{
	const float Faces = FacingNow();
	for (double& Kept : HangKept)
	{
		Kept = 0.0;
	}
	bHangs = false;
	SetActorRotation(FRotator(0.0f, Faces, 0.0f));
}

bool AArmyPiece::Sway(double Seconds)
{
	if (!bHangs)
	{
		return false;
	}
	if (What == 1 || !CarriedWithWeight())
	{
		StandStraight(); // (the switch was turned off while it leaned)
		if (!bInHand)
		{
			SetActorLocation(Shown(HomeLocation())); // (it may have been standing on the edge of its base)
		}
		return false;
	}
	TheaterArmyCarry::FHang Hang = HangOf(HangKept);
	const double Reach = TheaterArmyCarry::HangsOn * LiftHeight / 3.2;
	const double Span = FMath::Clamp(Seconds, 0.0, 0.25);
	if (bInHand)
	{
		// In the hand: its foot swings under the hand as the hand moves it about. (While it is held it is ticked: one piece.)
		const FVector From = HandWas, To = HandNow;
		auto HandAt = [From, To, Span](double Time)
		{
			const FVector Hand = FMath::Lerp(From, To, Span > 1.0e-6 ? FMath::Clamp(Time / Span, 0.0, 1.0) : 1.0);
			return TheaterArmyCarry::FSpot{ Hand.X, Hand.Y, 0.0 };
		};
		TheaterArmyCarry::Swing(Hang, HandAt, 0.0, Span, TheaterArmyCarry::InTheAir, Reach);
		KeepHang(HangKept, Hang);
		HandWas = HandNow;
		StandLeaning(HandNow + FVector(0.0, 0.0, LiftHeight), false);
		return true;
	}
	// On the table (or held just off it, selected): it rocks, quick and soon over, and is still.
	auto Stays = [](double) { return TheaterArmyCarry::FSpot{}; };
	TheaterArmyCarry::Swing(Hang, Stays, 0.0, Span, TheaterArmyCarry::OnTheTable, Reach);
	const FVector Home = Shown(HomeLocation());
	if (TheaterArmyCarry::Still(Hang, Reach))
	{
		StandStraight();
		SetActorLocation(Home);
		return false;
	}
	KeepHang(HangKept, Hang);
	StandLeaning(Home, !bSelected);
	return true;
}

void AArmyPiece::TakeWeight(const FVector& Along)
{
	if (What == 1 || bMarching || bInHand || bSelected || bGlimpsed || !CarriedWithWeight())
	{
		return;
	}
	TheaterArmyCarry::FHang Hang = HangOf(HangKept);
	TheaterArmyCarry::Kick(Hang, Along.X, Along.Y, 0.7 * TheaterArmyCarry::Landing, TheaterArmyCarry::HangsOn * LiftHeight / 3.2);
	KeepHang(HangKept, Hang);
	bHangs = true;
	SetActorTickEnabled(true);
}

void AArmyPiece::SetShadows(bool bCast)
{
	if (bCast != bCastsShadows)
	{
		bCastsShadows = bCast;
		if (Body)
		{
			Body->SetCastShadow(bCast);
			TheaterSpeed::NoteMade(TheaterSpeed::EMade::ShadowFlips); // (speed log: its whole model is handed to the draw thread again)
		}
		for (UProceduralMeshComponent* HullMesh : Hulls)
		{
			if (HullMesh)
			{
				HullMesh->SetCastShadow(bCast); // (a fleet's ships, where each has a mesh of her own)
			}
		}
		if (Camp)
		{
			Camp->SetCastShadow(bCast); // (its camp with it)
		}
	}
}
