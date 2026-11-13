#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

AArmyPiece::AArmyPiece()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while on the march
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMeshComponent* AArmyPiece::AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, UMaterialInterface* Material)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetupAttachment(Root);
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
	for (const FViewCorps& Unit : Corps)
	{
		Divisions += Unit.Divisions;
		Men += Unit.Men;
		if (General.IsEmpty() && !Unit.General.IsEmpty())
		{
			General = Unit.General; // the first general named is the senior one
		}
		NewSignature += FString::Printf(TEXT("|%d%s%s%s"), Unit.Id, Unit.bDim ? TEXT("?") : TEXT(""), Unit.Stale > 0 ? TEXT("~") : TEXT(""), *Unit.Ally); // (what is known of it is part of how it looks)
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
	// The standard carries a streamer for every honour its corps have won.
	int32 HonourCount = 0;
	for (const FViewCorps& Unit : Corps)
	{
		HonourCount += Unit.Honours.Num();
	}
	NewSignature += FString::Printf(TEXT("|%dh"), HonourCount);
	// The standard shows the army's strength to the nearest thousand, so it is remade when that changes.
	NewSignature += FString::Printf(TEXT("|%dk"), FMath::RoundToInt32(Men / 1000.0f));
	if (NewSignature != Signature)
	{
		Signature = NewSignature;
		Rebuild(Map, Nation);
	}
}

void AArmyPiece::DressAs(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 Works, bool bInCamp, int32 Supply)
{
	if (What != 0)
	{
		return;
	}
	WorksState = Works;
	bCamp = bInCamp;
	SupplyState = Supply;
	Signature += TEXT("|dressed");
	Rebuild(Map, Nation);
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
	const FString NewSignature = FString::Printf(TEXT("%s|fleet|%d|%d|%d|%d|%d|%d|%s|%d|%s"), *Nation.Tag, InFleet.Id,
		InFleet.Battleships, InFleet.Carriers, InFleet.Cruisers, InFleet.Destroyers, InFleet.Submarines, *InFleet.Stance, InFleet.bReport ? 1 : 0, *InFleet.SizeWord);
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
	SelectionRing = AddPart(Cylinder, FVector(0.0, 0.0, 0.06 * U), FVector(11.2 * U, 11.2 * U, 0.12 * U), Glow);
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
			: FString::Printf(TEXT("%d ships"), Fleet.Ships());
		SelectionRing->SetVisibility(false);
	}
	else if (What == 2)
	{
		// An air fleet: its aircraft over a patch of airfield.
		Model.AirFleet(Wing.Fighters, Wing.Bombers, U, Colour, NationTag);
		Top = 0.2 * U;
		Size = FMath::Clamp(0.8 + (Wing.Fighters + Wing.Bombers) / 2500.0, 0.9, 1.6);
		Figure = FText::AsNumber(Wing.Fighters + Wing.Bombers).ToString();
	}
	else
	{
		// A counter for each corps, stacked: the taller the stack, the bigger the army.
		const int32 Counters = FMath::Clamp(Corps.Num(), 1, 7);
		const double CounterHeight = 0.5 * U, CounterGap = 0.1 * U;
		for (int32 i = 0; i < Counters; ++i)
		{
			Model.Pillar(FVector(0.0, 0.0, Top), CounterWidth * 0.5, CounterWidth * 0.5 - 0.08 * U, CounterHeight, i % 2 == 0 ? Colour : Dark, 30);
			Top += CounterHeight + CounterGap;
		}
		Top -= CounterGap;

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
		const double Rim = CounterWidth * 0.5;
		auto Round = [](double FromFront, double Out) { const double A = FMath::DegreesToRadians(FromFront); return FVector(-FMath::Cos(A) * Out, FMath::Sin(A) * Out, 0.0); };
		if (WorksState > 0)
		{
			// Dug in: a breastwork of sandbags round the front of the base, two courses high, and wire before it.
			// Still digging: one short course across the front and a heap of spoil.
			const FLinearColor Bag(FColor::FromHex(TEXT("C2B280"))), BagDark(FColor::FromHex(TEXT("A99B6C"))), Stake(FColor::FromHex(TEXT("3A342C"))), Wire(FColor::FromHex(TEXT("55585C"))), Spoil(FColor::FromHex(TEXT("6B5338")));
			const double Sweep = WorksState == 2 ? 118.0 : 46.0;
			const int32 Bags = WorksState == 2 ? 13 : 5;
			for (int32 Course = 0; Course < (WorksState == 2 ? 2 : 1); ++Course)
			{
				for (int32 K = 0; K < Bags - Course; ++K)
				{
					const double Angle = -Sweep + (K + 0.5 * Course) * (2.0 * Sweep / (Bags - 1));
					const FVector At = Round(Angle, Rim + 0.85 * U) + FVector(0.0, 0.0, (0.24 + 0.42 * Course) * U);
					Model.Box(At, FVector(0.62, 1.3, 0.44) * U, (K + Course) % 2 == 0 ? Bag : BagDark, FMath::RadiansToDegrees(FMath::Atan2(At.Y, At.X)));
				}
			}
			if (WorksState == 2)
			{
				FVector Last = FVector::ZeroVector;
				for (int32 K = 0; K < 8; ++K)
				{
					const double Angle = -104.0 + K * (208.0 / 7.0);
					const FVector Foot = Round(Angle, Rim + 2.15 * U);
					const FVector Along = Round(Angle + 90.0, 0.34 * U);
					Model.Rod(Foot - Along, Foot + Along + FVector(0.0, 0.0, 0.8 * U), 0.05 * U, Stake, 4);
					Model.Rod(Foot + Along, Foot - Along + FVector(0.0, 0.0, 0.8 * U), 0.05 * U, Stake, 4);
					const FVector Top2 = Foot + FVector(0.0, 0.0, 0.5 * U);
					if (K > 0)
					{
						Model.Rod(Last, Top2, 0.028 * U, Wire, 4);
						Model.Rod(Last + FVector(0.0, 0.0, 0.26 * U), Top2 + FVector(0.0, 0.0, 0.26 * U), 0.028 * U, Wire, 4);
					}
					Last = Top2;
				}
			}
			else
			{
				Model.Ball(Round(-70.0, Rim + 1.3 * U) + FVector(0.0, 0.0, 0.12 * U), FVector(1.5, 1.1, 0.7) * U, Spoil, 4, 8);
			}
		}
		if (bCamp)
		{
			// In camp: a row of ridge tents to the right of the base, their open ends to the viewer, and a fire.
			const FLinearColor Tent(FColor::FromHex(TEXT("D2C8A4"))), TentShade(FColor::FromHex(TEXT("B8AD86"))), Log(FColor::FromHex(TEXT("4A3A2A"))), Flame(FColor::FromHex(TEXT("FF8A2A"))), Ember(FColor::FromHex(TEXT("FFD060")));
			for (int32 K = 0; K < 3; ++K)
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
	}

	// The standard: a pole at the back of the base, the nation's flag of the period, and a brass finial.
	const double FlagWidth = 6.6 * U * Size, FlagHeight = FlagWidth / 1.5;
	const double PoleHeight = 9.0 * U + FlagHeight;
	const FVector Pole(CounterWidth * 0.32, -CounterWidth * 0.3, 0.0); // behind the figures, and to the left, so the flag flies over the piece
	Model.Pillar(Pole + FVector(0.0, 0.0, Top), 0.2 * U, 0.2 * U, PoleHeight, IronPaint, 8);
	Model.Ball(Pole + FVector(0.0, 0.0, Top + PoleHeight + 0.4 * U), FVector(1.1 * U), BrassPaint, 6, 10);
	const double Hoist = 0.2 * U; // where the cloth begins, just clear of the pole
	const FVector FlagAt = Pole + FVector(0.0, Hoist + FlagWidth * 0.5, Top + PoleHeight - FlagHeight * 0.5 - 0.3 * U);
	if (UMaterialInterface* Colours = Map->GetFlagMaterial(Nation.Tag))
	{
		// The cloth is a sheet with a slow ripple in it that grows toward the free end, as a flag hangs in a light
		// air. Pieces are turned so that the viewer looks along their +X: the cloth faces back along -X, the pole
		// on the viewer's left, which is the side a flag is read from.
		const int32 Across = 18, Down = 6;
		const double Ripple = 0.045 * FlagWidth, Waves = 1.3, Droop = 0.035 * FlagHeight;
		const double Phase = (RegionId * 37 % 100) * 0.0628; // no two neighbours hang alike
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		for (int32 J = 0; J <= Down; ++J)
		{
			for (int32 I = 0; I <= Across; ++I)
			{
				const double S = static_cast<double>(I) / Across, T = static_cast<double>(J) / Down;
				const double Angle = UE_DOUBLE_TWO_PI * Waves * S + Phase + 0.8 * T;
				Vertices.Add(FVector(Pole.X + Ripple * S * FMath::Sin(Angle), Pole.Y + Hoist + S * FlagWidth, FlagAt.Z + FlagHeight * (0.5 - T) - Droop * S * S));
				const FVector Along(Ripple * (FMath::Sin(Angle) + S * UE_DOUBLE_TWO_PI * Waves * FMath::Cos(Angle)), FlagWidth, -2.0 * Droop * S);
				const FVector Downward(Ripple * S * 0.8 * FMath::Cos(Angle), 0.0, -FlagHeight);
				Normals.Add((Along ^ Downward).GetSafeNormal());
				Tangents.Add(FProcMeshTangent(Along.GetSafeNormal(), false));
				UVs.Add(FVector2D(S, T));
			}
		}
		for (int32 J = 0; J < Down; ++J)
		{
			for (int32 I = 0; I < Across; ++I)
			{
				const int32 A = J * (Across + 1) + I, B = A + 1, C = A + Across + 1, D = C + 1;
				Triangles.Append({ A, C, B, B, C, D });
			}
		}
		Cloth = NewObject<UProceduralMeshComponent>(this);
		Cloth->SetupAttachment(Root);
		Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cloth->RegisterComponent();
		Cloth->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);
		Cloth->SetMaterial(0, Colours);
	}
	else
	{
		// No flag material: a plain banner in the nation's colour, with a paler band top and bottom.
		AddPart(Cube, FlagAt, FVector(0.16 * U, FlagWidth, FlagHeight), Banner);
		AddPart(Cube, FlagAt + FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand);
		AddPart(Cube, FlagAt - FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand);
	}
	PoleFoot = Pole;
	PoleTop = Top + PoleHeight;
	BodyTop = Top + 3.2 * U;
	BaseRadius = CounterWidth * 0.5;
	FlagCenter = FlagAt;
	FlagRadius = FlagWidth * 0.55;

	// The strength plate, hung from the pole under the flag, edged in brass.
	const double PlateHeight = FMath::Min(FlagHeight * 0.42, 2.7 * U);
	const double PlateWidth = PlateHeight * (0.95 + 0.5 * Figure.Len());
	const FVector PlateAt = Pole + FVector(0.0, Hoist + PlateWidth * 0.5, FlagAt.Z - FlagHeight * 0.5 - 0.35 * U - PlateHeight * 0.5);
	Model.Box(PlateAt, FVector(0.2 * U, PlateWidth, PlateHeight), FLinearColor(FColor::FromHex(TEXT("15130F"))));
	Model.Box(PlateAt + FVector(0.0, 0.0, PlateHeight * 0.5), FVector(0.26 * U, PlateWidth + 0.1 * U, 0.14 * U), BrassPaint);
	Model.Box(PlateAt - FVector(0.0, 0.0, PlateHeight * 0.5), FVector(0.26 * U, PlateWidth + 0.1 * U, 0.14 * U), BrassPaint);

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

	// The mesh is done: hand it over.
	Body = NewObject<UProceduralMeshComponent>(this);
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->RegisterComponent();
	Body->CreateMeshSection(0, Model.Vertices, Model.Triangles, Model.Normals, TArray<FVector2D>(), Model.Colors, TArray<FProcMeshTangent>(), false);
	Body->SetMaterial(0, Map->GetPieceMaterial());

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
			AddPart(Cube, Pole + FVector(0.0, Hoist + StreamerLength * 0.25, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.First), 0.6f, 0.0f, 0.6f))->SetCastShadow(false);
			AddPart(Cube, Pole + FVector(0.0, Hoist + StreamerLength * 0.75, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.Second), 0.6f, 0.0f, 0.6f))->SetCastShadow(false);
			++Streamer;
		}
	}

	// The figure. The lettering faces back along -X, toward the viewer.
	StrengthLabel = NewObject<UTextRenderComponent>(this);
	StrengthLabel->SetupAttachment(Root);
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
	StrengthLabel->SetRelativeLocation(PlateAt - FVector(0.16 * U, 0.0, 0.0));
	StrengthLabel->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	StrengthLabel->RegisterComponent();
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

void AArmyPiece::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	ShowRing();
	if (!bMarching)
	{
		SetActorLocation(HomeLocation());
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
		SetActorLocation(HomeLocation());
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
	const FVector Flag = Place.TransformPosition(FlagCenter);
	return Near(Flag, Flag, FlagRadius)
		|| Near(Place.TransformPosition(PoleFoot), Place.TransformPosition(PoleFoot + FVector(0.0, 0.0, PoleTop)), BaseRadius * 0.3)
		|| Near(Place.TransformPosition(FVector::ZeroVector), Place.TransformPosition(FVector(0.0, 0.0, BodyTop)), BaseRadius);
}

void AArmyPiece::Carry(const FVector& GroundPoint)
{
	bMarching = false;
	SetActorTickEnabled(false);
	SetActorLocation(GroundPoint + FVector(0.0, 0.0, LiftHeight));
}

void AArmyPiece::ReturnToRest()
{
	if (!bMarching && !GetActorLocation().Equals(HomeLocation(), 1.0))
	{
		MarchAlong({ GetActorLocation(), RestLocation }, false, false);
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
	MarchClock = 0.0;
	bMarching = true;
	bRemoveOnArrival = bInRemoveOnArrival;
	SetActorLocation(Route[0]);
	SetActorTickEnabled(true);
}

void AArmyPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMarching)
	{
		SetActorTickEnabled(false);
		return;
	}

	MarchClock += DeltaSeconds;
	const double Progress = FMath::Clamp(MarchClock / MarchTime, 0.0, 1.0);
	// Ease away from the start and into the finish, as a hand moves a piece.
	const double Eased = Progress * Progress * (3.0 - 2.0 * Progress);
	const double Target = Eased * RouteMarks.Last();

	int32 Segment = 1;
	while (Segment < RouteMarks.Num() - 1 && RouteMarks[Segment] < Target)
	{
		++Segment;
	}
	const double Span = FMath::Max(RouteMarks[Segment] - RouteMarks[Segment - 1], 1.0e-3);
	FVector Position = FMath::Lerp(Route[Segment - 1], Route[Segment], (Target - RouteMarks[Segment - 1]) / Span);
	// Picked up, carried, set down.
	if (bLiftOnMarch)
	{
		Position.Z += LiftHeight * FMath::Pow(FMath::Sin(Progress * UE_DOUBLE_PI), 0.55);
	}
	// A piece that is still in the hand when it arrives comes to rest a little off the table.
	Position += (HomeLocation() - RestLocation) * Eased;
	SetActorLocation(Position);

	if (Progress >= 1.0)
	{
		bMarching = false;
		SetActorTickEnabled(false);
		if (bRemoveOnArrival)
		{
			Destroy();
		}
		else
		{
			SetActorLocation(HomeLocation());
		}
	}
}

void AArmyPiece::SetShadows(bool bCast)
{
	if (bCast != bCastsShadows)
	{
		bCastsShadows = bCast;
		if (Body)
		{
			Body->SetCastShadow(bCast);
		}
	}
}
