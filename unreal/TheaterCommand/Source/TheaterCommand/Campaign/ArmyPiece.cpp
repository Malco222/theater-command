#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignMapActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
		NewSignature += FString::Printf(TEXT("|%d"), Unit.Id);
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
	SelectionRing = AddPart(Cylinder, FVector(0.0, 0.0, 0.06 * U), FVector(10.2 * U, 10.2 * U, 0.12 * U), Glow);
	SelectionRing->SetCastShadow(false);
	ShowRing();

	// A counter for each corps, stacked: the taller the stack, the bigger the army.
	const int32 Counters = FMath::Clamp(Corps.Num(), 1, 7);
	const double CounterHeight = 0.75 * U, CounterGap = 0.12 * U, CounterWidth = 7.5 * U;
	double Top = 0.14 * U;
	for (int32 i = 0; i < Counters; ++i)
	{
		AddPart(Cylinder, FVector(0.0, 0.0, Top + CounterHeight * 0.5), FVector(CounterWidth, CounterWidth, CounterHeight), i % 2 == 0 ? Paint : DarkPaint);
		Top += CounterHeight + CounterGap;
	}

	// The standard: a pole, the nation's colours, and a brass finial. The bigger the army, the bigger its
	// standard, and its strength is written across it in thousands of men.
	const double Size = FMath::Clamp(0.82 + 0.26 * FMath::Sqrt(Men / 30000.0), 0.9, 2.0);
	const double FlagWidth = 6.6 * U * Size, FlagHeight = 4.3 * U * Size;
	const double PoleHeight = 9.0 * U + FlagHeight;
	AddPart(Cylinder, FVector(0.0, 0.0, Top + PoleHeight * 0.5), FVector(0.42 * U, 0.42 * U, PoleHeight), Iron);
	const FVector FlagAt(0.0, FlagWidth * 0.5 + 0.2 * U, Top + PoleHeight - FlagHeight * 0.5 - 0.3 * U);
	AddPart(Cube, FlagAt, FVector(0.16 * U, FlagWidth, FlagHeight), Banner);
	// A band along the top and the bottom of the colours, leaving the middle clear for the figure.
	AddPart(Cube, FlagAt + FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand);
	AddPart(Cube, FlagAt - FVector(0.0, 0.0, FlagHeight * 0.41), FVector(0.2 * U, FlagWidth, FlagHeight * 0.14), BannerBand);
	AddPart(Sphere, FVector(0.0, 0.0, Top + PoleHeight + 0.4 * U), FVector(1.1 * U, 1.1 * U, 1.1 * U), Brass);
	PoleTop = Top + PoleHeight;
	BaseRadius = CounterWidth * 0.5;
	FlagCenter = FlagAt;
	FlagRadius = FlagWidth * 0.55;

	// Honours: a two-coloured streamer for each, one under another below the colours.
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
			const double Z = FlagAt.Z - FlagHeight * 0.5 - 0.5 * U - Streamer * (StreamerDepth + 0.16 * U);
			AddPart(Cube, FVector(0.0, 0.2 * U + StreamerLength * 0.25, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.First), 0.6f, 0.0f, 0.6f))->SetCastShadow(false);
			AddPart(Cube, FVector(0.0, 0.2 * U + StreamerLength * 0.75, Z), FVector(0.14 * U, StreamerLength * 0.5, StreamerDepth),
				Map->GetSolidMaterial(FLinearColor(Honour.Second), 0.6f, 0.0f, 0.6f))->SetCastShadow(false);
			++Streamer;
		}
	}

	// The figure. Pieces are turned so that the viewer looks along their +X, so the lettering faces back along -X.
	const FString Figure = Men >= 995000 ? FString::Printf(TEXT("%.1fM"), Men / 1000000.0) : FString::Printf(TEXT("%dk"), FMath::Max(1, FMath::RoundToInt32(Men / 1000.0)));
	const bool bPaleColours = Colour.GetLuminance() > 0.3f;
	StrengthLabel = NewObject<UTextRenderComponent>(this);
	StrengthLabel->SetupAttachment(Root);
	StrengthLabel->SetText(FText::FromString(Figure));
	StrengthLabel->SetWorldSize(static_cast<float>(FlagHeight * (Figure.Len() > 3 ? 0.5 : 0.62)));
	StrengthLabel->SetTextRenderColor(FColor::FromHex(bPaleColours ? TEXT("14110E") : TEXT("F4F0E4")));
	// The side of the standard that faces the viewer is often the side away from the lamp, so the figure
	// is drawn in lettering that does not take the light: it reads the same from every side.
	if (UMaterialInterface* Unlit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")))
	{
		StrengthLabel->SetTextMaterial(Unlit);
	}
	StrengthLabel->SetHorizontalAlignment(EHTA_Center);
	StrengthLabel->SetVerticalAlignment(EVRTA_TextCenter);
	StrengthLabel->SetCastShadow(false);
	StrengthLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StrengthLabel->SetRelativeLocation(FlagAt - FVector(0.14 * U, 0.0, 0.0));
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
		|| Near(Place.TransformPosition(FVector::ZeroVector), Place.TransformPosition(FVector(0.0, 0.0, PoleTop)), BaseRadius);
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
