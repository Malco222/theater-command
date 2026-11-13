#include "Campaign/CampaignEnvironment.h"

#include "Campaign/CampaignMapActor.h"
#include "EngineUtils.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "UObject/ConstructorHelpers.h"

ACampaignEnvironment::ACampaignEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	// Pictures are taken by day unless night is asked for ("-TheaterNight"), so that they can be compared.
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNight")))
	{
		TimeKeeping = 2;
		Night = 1.0f;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), DayClockHeld))
	{
		TimeKeeping = 1;
	}

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// North is +X and east is +Y. The lamp stands low in the west-south-west, behind the left shoulder of
	// someone looking north across the table: pieces are lit from the front and their shadows fall away
	// to the right, and the low angle rakes across the relief.
	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-42.0f, 69.0f, 0.0f));
	KeyLight->SetIntensity(6.0f);
	KeyLight->bUseTemperature = true;
	KeyLight->Temperature = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")) ? 5200.0f : 4900.0f; // a warm lamp over the table
	KeyLight->SetCastShadows(true);
	// "-TheaterOldLook" on the command line leaves the newer lighting off, for a picture to compare against.
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		// A lamp is a broad light: the edges of its shadows are soft, and softer the farther they fall.
		KeyLight->LightSourceAngle = 1.5f;
		// Small things throw small shadows where they touch: a wheel on its base, a rifle across a chest.
		KeyLight->ContactShadowLength = 0.03f;
		KeyLight->SetIntensity(7.0f);
	}
	KeyLight->ForwardShadingPriority = 1;
	KeyLight->DynamicShadowCascades = 4;
	KeyLight->DynamicShadowDistanceMovableLight = 60000.0f;

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeRotation(FRotator(-40.0f, -105.0f, 0.0f));
	FillLight->SetIntensity(FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")) ? 0.6f : 0.75f); // the cool light of the room, filling the shadows
	FillLight->ForwardShadingPriority = 0;
	FillLight->bUseTemperature = true;
	FillLight->Temperature = 8500.0f;
	FillLight->SetCastShadows(false);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = SLS_SpecifiedCubemap;
	static ConstructorHelpers::FObjectFinder<UTextureCube> Ambient(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
	if (Ambient.Succeeded())
	{
		SkyLight->Cubemap = Ambient.Object;
	}
	// Less light from all round and more from the lamp, so that the lit side and the shaded side of a model differ
	// and its shape can be read.
	SkyLight->SetIntensity(FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")) ? 1.0f : 0.72f);
	SkyLight->bLowerHemisphereIsBlack = false;
}

void ACampaignEnvironment::SetSeason(float Winter, float Summer)
{
	SeasonWinter = Winter;
	SeasonSummer = Summer;
	ApplyLight();
}

int32 ACampaignEnvironment::NextTimeKeeping()
{
	TimeKeeping = (TimeKeeping + 1) % 3;
	return TimeKeeping;
}

void ACampaignEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// One day of the room in about twelve minutes: most of it daylight, a short dusk, the night, a short dawn.
	const double DayLasts = 720.0;
	if (TimeKeeping == 0)
	{
		DayClock = FMath::Fmod(DayClock + DeltaSeconds / DayLasts, 1.0);
	}
	// (the day is the first 0.58 of it, then dusk to 0.68, night to 0.9, and dawn)
	float Wanted = 0.0f;
	if (TimeKeeping == 2)
	{
		Wanted = 1.0f;
	}
	else if (TimeKeeping == 0)
	{
		const float Clock = static_cast<float>(DayClock);
		Wanted = Clock < 0.58f ? 0.0f : Clock < 0.68f ? FMath::SmoothStep(0.58f, 0.68f, Clock) : Clock < 0.9f ? 1.0f : 1.0f - FMath::SmoothStep(0.9f, 1.0f, Clock);
	}
	// A held time is come to over a few seconds, not at a jump.
	const float Next = FMath::FInterpTo(Night, Wanted, DeltaSeconds, 0.9f);
	if (FMath::Abs(Next - Night) > 0.0015f || (Next != Night && FMath::IsNearlyEqual(Next, Wanted, 0.002f)))
	{
		Night = FMath::IsNearlyEqual(Next, Wanted, 0.002f) ? Wanted : Next;
		ApplyLight();
	}
}

void ACampaignEnvironment::ApplyLight()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		return;
	}
	// The lamp is warm at 4900 K: winter takes it toward a cold white and takes a little off it; summer warms it.
	// At night the lamp is all there is: warmer and a little lower, with the cool light of the windows gone,
	// so the table stands in a pool of lamplight and the shadows deepen.
	KeyLight->SetTemperature(4900.0f + 1300.0f * SeasonWinter * (1.0f - Night) - 250.0f * SeasonSummer - 900.0f * Night);
	KeyLight->SetIntensity((7.0f - 0.9f * SeasonWinter + 0.3f * SeasonSummer) * (1.0f - 0.2f * Night));
	FillLight->SetIntensity((0.75f + 0.25f * SeasonWinter) * (1.0f - 0.72f * Night));
	SkyLight->SetIntensity(0.72f * (1.0f - 0.55f * Night));
	// And the towns on the map light their windows.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACampaignMapActor> It(World); It; ++It)
		{
			It->SetNight(Night);
		}
	}
}
