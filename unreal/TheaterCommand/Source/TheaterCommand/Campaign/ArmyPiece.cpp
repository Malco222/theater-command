#include "Campaign/ArmyPiece.h"

#include "Campaign/CampaignMapActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AArmyPiece::AArmyPiece()
{
	PrimaryActorTick.bCanEverTick = false;
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
	return Part;
}

void AArmyPiece::Build(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 InRegionId, const TArray<FCampaignCorps>& InCorps)
{
	RegionId = InRegionId;
	NationTag = Nation.Tag;
	Corps = InCorps;
	Divisions = 0;
	General.Reset();
	for (const FCampaignCorps& Unit : Corps)
	{
		Divisions += Unit.Divisions;
		if (General.IsEmpty() && !Unit.General.IsEmpty())
		{
			General = Unit.General; // the first general named is the senior one
		}
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
	UMaterialInterface* Banner = Map->GetSolidMaterial(Colour, 0.6f, 0.0f, 0.3f);
	UMaterialInterface* BannerBand = Map->GetSolidMaterial(Light, 0.6f, 0.0f, 0.3f);
	UMaterialInterface* Iron = Map->GetSolidMaterial(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f, 1.0f);
	UMaterialInterface* Brass = Map->GetSolidMaterial(FLinearColor(FColor::FromHex(TEXT("C9A227"))), 0.3f, 1.0f);

	const double U = Map->UnitScale * 1.4; // one map unit, with the pieces a little over life size so they read from far off

	// A counter for each corps, stacked: the taller the stack, the bigger the army.
	const int32 Counters = FMath::Clamp(Corps.Num(), 1, 7);
	const double CounterHeight = 0.75 * U, CounterGap = 0.12 * U, CounterWidth = 7.5 * U;
	double Top = 0.0;
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
