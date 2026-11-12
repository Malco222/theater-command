#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignMapActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	General.Reset();
	FString NewSignature = Nation.Tag;
	for (const FViewCorps& Unit : Corps)
	{
		Divisions += Unit.Divisions;
		if (General.IsEmpty() && !Unit.General.IsEmpty())
		{
			General = Unit.General; // the first general named is the senior one
		}
		NewSignature += FString::Printf(TEXT("|%d"), Unit.Id);
	}
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
	const bool bWasSelected = SelectionRing && SelectionRing->IsVisible();
	SelectionRing = nullptr;

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	const FLinearColor Colour(Nation.Color);
	const FLinearColor Dark = Colour * 0.55f;
	const FLinearColor Light = FMath::Lerp(Colour, FLinearColor::White, 0.45f);
	UMaterialInterface* Paint = Map->GetSolidMaterial(Colour, 0.5f, 0.0f);
	UMaterialInterface* DarkPaint = Map->GetSolidMaterial(Dark, 0.5f, 0.0f);
	// A banner has to be read from any side, so it carries a little light of its own.
	UMaterialInterface* Banner = Map->GetSolidMaterial(Colour, 0.6f, 0.0f, 0.3f);
	UMaterialInterface* BannerBand = Map->GetSolidMaterial(Light, 0.6f, 0.0f, 0.3f);
	UMaterialInterface* Iron = Map->GetSolidMaterial(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f, 1.0f);
	UMaterialInterface* Brass = Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("C9A227"))), 0.3f, 1.0f);
	UMaterialInterface* Glow = Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("FFD27A"))), 0.4f, 0.0f, 2.5f);

	const double U = Map->UnitScale * 1.4; // one map unit, with the pieces a little over life size so they read from far off
	LiftHeight = 3.2 * U;

	// The ring that shows when the piece is picked up: a thin bright disc a little wider than the base.
	SelectionRing = AddPart(Cylinder, FVector(0.0, 0.0, 0.06 * U), FVector(10.2 * U, 10.2 * U, 0.12 * U), Glow);
	SelectionRing->SetCastShadow(false);
	SelectionRing->SetVisibility(bWasSelected);

	// A counter for each corps, stacked: the taller the stack, the bigger the army.
	const int32 Counters = FMath::Clamp(Corps.Num(), 1, 7);
	const double CounterHeight = 0.75 * U, CounterGap = 0.12 * U, CounterWidth = 7.5 * U;
	double Top = 0.14 * U;
	for (int32 i = 0; i < Counters; ++i)
	{
		AddPart(Cylinder, FVector(0.0, 0.0, Top + CounterHeight * 0.5), FVector(CounterWidth, CounterWidth, CounterHeight), i % 2 == 0 ? Paint : DarkPaint);
		Top += CounterHeight + CounterGap;
	}

	// The standard: a pole, the nation's colours with a band across them, and a brass finial.
	const double PoleHeight = 12.5 * U;
	AddPart(Cylinder, FVector(0.0, 0.0, Top + PoleHeight * 0.5), FVector(0.42 * U, 0.42 * U, PoleHeight), Iron);
	const double FlagWidth = 6.4 * U, FlagHeight = 4.2 * U;
	const FVector FlagCenter(0.0, FlagWidth * 0.5 + 0.2 * U, Top + PoleHeight - FlagHeight * 0.5 - 0.3 * U);
	AddPart(Cube, FlagCenter, FVector(0.16 * U, FlagWidth, FlagHeight), Banner);
	AddPart(Cube, FlagCenter, FVector(0.2 * U, FlagWidth, FlagHeight * 0.26), BannerBand);
	AddPart(Sphere, FVector(0.0, 0.0, Top + PoleHeight + 0.4 * U), FVector(1.1 * U, 1.1 * U, 1.1 * U), Brass);
}

void AArmyPiece::SetSelected(bool bInSelected)
{
	if (SelectionRing)
	{
		SelectionRing->SetVisibility(bInSelected);
	}
}

void AArmyPiece::SetRestLocation(const FVector& Location)
{
	RestLocation = Location;
	if (!bMarching)
	{
		SetActorLocation(Location);
	}
}

void AArmyPiece::MarchAlong(const TArray<FVector>& Waypoints, bool bInRemoveOnArrival)
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
	// A short hop takes most of a second; a long march not much more than two.
	MarchTime = FMath::Clamp(0.55 + Length / 5200.0, 0.75, 2.4);
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
	Position.Z += LiftHeight * FMath::Pow(FMath::Sin(Progress * UE_DOUBLE_PI), 0.55);
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
			SetActorLocation(RestLocation);
		}
	}
}
