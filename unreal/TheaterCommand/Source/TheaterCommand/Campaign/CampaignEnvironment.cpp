#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "GameFramework/PlayerController.h"

#include "Campaign/CampaignMapActor.h"
#include "EngineUtils.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * The evening's light, as the art director ruled it in round 26 by its results at the resting evening: lit
	 * ground red to blue 1.7 to 1.9, shaded ground 0.95 to 1.1, the land's colour kept, every nation still its own.
	 * ("-TheaterDuskLook=rests,amberR,amberG,amberB,amber,dim,coolR,coolG,coolB,cool,sunR,sunG,sunB,shadeR,shadeG,shadeB"
	 * tries others without a rebuild.)
	 */
	struct FDuskLook
	{
		float Rests = 0.4f; // how far into the night the evening rests when a month ends
		FLinearColor Amber = FLinearColor(1.0f, 0.84f, 0.68f); // the lamp's light at the evening: only a little amber, since it falls on lit and shaded ground alike
		float AmberShare = 0.6f; // how far the lamp's light is that amber where the evening rests
		float Dim = 0.2f; // how much of the lamp's and the room's light is gone by then
		FLinearColor Cool = FLinearColor(0.5f, 0.66f, 1.0f); // the fill's and the sky's light at the evening
		float CoolShare = 0.9f;
		// (The land's own printed sun and shade at the evening carry most of it: gold on the slopes that face the
		// lamp, blue in the shade. Measured at working height over Silesia, by the art director's own measures: lit
		// ground red to blue 1.77, shaded 1.11, the map's median brightness a quarter, Germany against Poland four
		// fifths of what it is by day.)
		FLinearColor Sun = FLinearColor(1.0f, 0.64f, 0.2f);
		FLinearColor Shade = FLinearColor(0.2f, 0.43f, 0.88f);
	};
	const FDuskLook& DuskLook()
	{
		static FDuskLook Kept;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterDuskLook="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 10)
			{
				auto Number = [&Parts](int32 K) { return FCString::Atof(*Parts[K]); };
				Kept.Rests = Number(0);
				Kept.Amber = FLinearColor(Number(1), Number(2), Number(3));
				Kept.AmberShare = Number(4);
				Kept.Dim = Number(5);
				Kept.Cool = FLinearColor(Number(6), Number(7), Number(8));
				Kept.CoolShare = Number(9);
				if (Parts.Num() >= 16)
				{
					Kept.Sun = FLinearColor(Number(10), Number(11), Number(12));
					Kept.Shade = FLinearColor(Number(13), Number(14), Number(15));
				}
			}
		}
		return Kept;
	}
}

const ACampaignEnvironment::FMoonLook& ACampaignEnvironment::MoonLook()
{
	static FMoonLook Kept;
	static bool bRead = false;
	if (!bRead)
	{
		bRead = true;
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterMoonLook="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) >= 11)
		{
			auto Number = [&Parts](int32 K) { return FCString::Atof(*Parts[K]); };
			Kept.Key = Number(0);
			Kept.Fill = Number(1);
			Kept.Sky = Number(2);
			Kept.KeyColour = FLinearColor(Number(3), Number(4), Number(5));
			Kept.SkyColour = FLinearColor(Number(6), Number(7), Number(8));
			Kept.Haze = Number(9);
			Kept.Politics = Number(10);
			if (Parts.Num() >= 17)
			{
				Kept.Sun = FLinearColor(Number(11), Number(12), Number(13));
				Kept.Shade = FLinearColor(Number(14), Number(15), Number(16));
			}
			if (Parts.Num() >= 18)
			{
				Kept.HazeFloor = Number(17);
			}
			if (Parts.Num() >= 22)
			{
				Kept.Woods = Number(18);
				Kept.Trees = FLinearColor(Number(19), Number(20), Number(21));
			}
		}
	}
	return Kept;
}

float ACampaignEnvironment::DuskRests()
{
	return DuskLook().Rests;
}

float ACampaignEnvironment::DuskOf(float Night)
{
	// (nothing by day, all of it where the evening rests, gone again by twice that far into the night)
	const float Rests = FMath::Max(DuskLook().Rests, 0.05f);
	return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(1.0f - FMath::Abs(Night - Rests) / Rests, 0.0f, 1.0f));
}

float ACampaignEnvironment::MoonOf(float Night)
{
	return FMath::SmoothStep(DuskLook().Rests, 1.0f, Night);
}

FLinearColor ACampaignEnvironment_DuskSun() { return DuskLook().Sun; }
FLinearColor ACampaignEnvironment_DuskShade() { return DuskLook().Shade; }

ACampaignEnvironment::ACampaignEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	// Pictures are taken by day unless night is asked for ("-TheaterNight"), so that they can be compared.
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNight")))
	{
		TimeKeeping = 2;
		Night = 1.0f;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("TheaterDusk="), HeldNight))
	{
		// ("-TheaterDusk=0.5" holds the evening that comes when a month ends, for a picture of it)
		TimeKeeping = 2;
		HeldNight = FMath::Clamp(HeldNight, 0.0f, 1.0f);
		Night = HeldNight;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), DayClockHeld))
	{
		// ("-TheaterDayRuns" lets the room's day run while pictures are taken, as it does in play: for the evening that comes when a month ends)
		TimeKeeping = FParse::Param(FCommandLine::Get(), TEXT("TheaterDayRuns")) ? 0 : 1;
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
	FillLight->Temperature = 9800.0f; // (the shade is cool against the warm lamp)
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

	// The air: a plain haze (no volumes, nothing that costs), set each frame so that it begins where the eye is
	// looking and reaches about a third at the far edge of the view.
	Air = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Air"));
	Air->SetupAttachment(Root);
	Air->SetMobility(EComponentMobility::Movable);
	Air->FogHeightFalloff = 0.001f;
	Air->FogMaxOpacity = 0.36f;
	Air->FogDensity = 0.0f;
	Air->DirectionalInscatteringLuminance = FLinearColor::Black;
	Air->SetVisibility(!FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")) && !FParse::Param(FCommandLine::Get(), TEXT("TheaterNoAir")));
}

void ACampaignEnvironment::UpdateAir()
{
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ACampaignCameraPawn* Eye = Player ? Cast<ACampaignCameraPawn>(Player->GetPawn()) : nullptr;
	if (!Air || !Eye || !Player->PlayerCameraManager)
	{
		return;
	}
	// The haze begins three quarters of the way up the screen and is at its thickest at the far edge of the view.
	// How thick depends on how high the eye is: about a third from the top of the zoom, half of that at working
	// height, and next to nothing close to, where the far edge is not far.
	const float Middle = Eye->GetDistance();
	const float Down = FMath::DegreesToRadians(FMath::Clamp(-Player->PlayerCameraManager->GetCameraRotation().Pitch, 20.0f, 89.0f));
	int32 Wide = 1600, High = 1000;
	Player->GetViewportSize(Wide, High);
	const float HalfUp = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Eye->GetHorizontalFov() * 0.5f)) * High / FMath::Max(Wide, 1));
	const float FarEdge = Middle * FMath::Sin(Down) / FMath::Sin(FMath::Max(Down - HalfUp, 0.12f));
	const float Begins = Middle * FMath::Sin(Down) / FMath::Sin(FMath::Max(Down - FMath::Atan(0.5f * FMath::Tan(HalfUp)), 0.14f));
	float Thick = Middle > 9000.0f ? FMath::GetMappedRangeValueClamped(FVector2f(9000.0f, 14000.0f), FVector2f(0.17f, 0.35f), Middle)
		: FMath::GetMappedRangeValueClamped(FVector2f(4000.0f, 9000.0f), FVector2f(0.06f, 0.17f), Middle);
	{
		// Round a world the chart has a north edge with the table beyond it, at the far side of the view where the
		// air is thickest: a dark table under a third of haze went grey. There is no air over the table: as the
		// chart's north edge comes into the top of the view the air thins away, and is gone by the time the edge
		// has come a little way down the screen.
		static TWeakObjectPtr<ACampaignMapActor> Chart;
		if (!Chart.IsValid() && GetWorld())
		{
			TActorIterator<ACampaignMapActor> It(GetWorld());
			if (It)
			{
				Chart = *It;
			}
		}
		if (Chart.IsValid() && Chart->GetData().bWorld)
		{
			const double North = Chart->MapToWorld(0.0, Chart->GetData().WorldMinY).X;
			const FVector From = Player->PlayerCameraManager->GetCameraLocation();
			const double TopAt = From.X + FarEdge * FMath::Cos(FMath::Max(Down - HalfUp, 0.12f));
			const double BeginsAt = From.X + Begins * FMath::Cos(FMath::Max(Down - FMath::Atan(0.5f * FMath::Tan(HalfUp)), 0.14f));
			const float Over = static_cast<float>((TopAt - North) / FMath::Max(TopAt - BeginsAt, 1.0));
			Thick *= 1.0f - FMath::SmoothStep(0.0f, 0.15f, Over);
		}
	}
	static float Strength = -1.0f, Glow = 1.0f;
	if (Strength < 0.0f)
	{
		// ("-TheaterAir=0.5" tries another strength, "-TheaterAirGlow=2" another brightness)
		Strength = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterAir="), Strength);
		FParse::Value(FCommandLine::Get(), TEXT("TheaterAirGlow="), Glow);
	}
	// (the engine's fog thins light by two to the power of minus its density, in thousandths, times the way through it)
	const float Density = -FMath::Log2(1.0f - FMath::Clamp(Thick, 0.0f, 0.9f)) * 1000.0f / FMath::Max(FarEdge - Begins, 50.0f) * Strength;
	if (FMath::Abs(Begins - AirStart) > Begins * 0.01f || FMath::Abs(Density - AirDensity) > AirDensity * 0.02f)
	{
		AirStart = Begins;
		AirDensity = Density;
		Air->SetStartDistance(Begins);
		Air->SetFogDensity(Density);
	}
	if (FMath::Abs(Night - AirNight) > 0.01f)
	{
		AirNight = Night;
		// Pale blue-grey by day; by lamplight the far table simply falls away into the dark of the room.
		const FLinearColor Day(0.80f, 0.84f, 0.88f), Dark = bLamplight ? FLinearColor(0.035f, 0.04f, 0.055f) : FLinearColor(0.06f, 0.085f, 0.15f);
		Air->SetFogInscatteringColor(FMath::Lerp(Day, Dark, Night) * Glow);
	}
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
	// (Ruling 205(b): a night is an event, not a state. The day is the first five sixths of it; then dusk for about
	// half a minute, full night for about a minute, and dawn for about half a minute.)
	float Wanted = 0.0f;
	if (TimeKeeping == 2)
	{
		Wanted = HeldNight >= 0.0f ? HeldNight : 1.0f;
	}
	else if (TimeKeeping == 0)
	{
		const float Clock = static_cast<float>(DayClock);
		Wanted = Clock < 0.8333f ? 0.0f : Clock < 0.875f ? FMath::SmoothStep(0.8333f, 0.875f, Clock) : Clock < 0.9583f ? 1.0f : 1.0f - FMath::SmoothStep(0.9583f, 1.0f, Clock);
		// (the month has ended: evening, whatever the hour was, and night if it was night already)
		if (bMonthsEnd)
		{
			Wanted = FMath::Max(Wanted, DuskRests());
		}
	}
	// (a raid by night is playing: night, come to quickly, as the bombers come in)
	if (bRaidNight)
	{
		Wanted = 1.0f;
	}
	// A held time is come to over a few seconds, not at a jump.
	UpdateAir();
	const float Next = bRaidNight ? FMath::FInterpConstantTo(Night, Wanted, DeltaSeconds, 0.7f) : FMath::FInterpTo(Night, Wanted, DeltaSeconds, 0.9f);
	if (FMath::Abs(Next - Night) > 0.0015f || (Next != Night && FMath::IsNearlyEqual(Next, Wanted, 0.002f)))
	{
		Night = FMath::IsNearlyEqual(Next, Wanted, 0.002f) ? Wanted : Next;
		ApplyLight();
	}
}

void ACampaignEnvironment::SetMonthsEnd(bool bEnded)
{
	if (bEnded == bMonthsEnd)
	{
		return;
	}
	bMonthsEnd = bEnded;
	if (!bEnded && TimeKeeping == 0)
	{
		DayClock = 0.02; // (a new month: it is morning, and the room's day begins afresh)
	}
}

void ACampaignEnvironment::StudioLight(bool bOn)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		return;
	}
	if (bOn)
	{
		// A plain day, in no season: the lamp at its own warmth, the room's cool fill and the sky's white.
		// (The lamp's warmth is not touched, since a change of it remakes the light, and the turning showpiece has its
		// picture taken twenty times a second. Its colour is set instead to what takes it from the warmth it has to
		// the day's, which is the same light and costs nothing.)
		const FLinearColor Want = FLinearColor::MakeFromColorTemperature(4900.0f), Have = FLinearColor::MakeFromColorTemperature(KeyLight->Temperature);
		FLinearColor Shift(Want.R / FMath::Max(Have.R, 0.01f), Want.G / FMath::Max(Have.G, 0.01f), Want.B / FMath::Max(Have.B, 0.01f));
		const float Most = FMath::Max3(Shift.R, Shift.G, Shift.B);
		Shift = FLinearColor(Shift.R / Most, Shift.G / Most, Shift.B / Most);
		KeyLight->SetLightColor(Shift);
		KeyLight->SetIntensity(7.0f * Most);
		FillLight->SetIntensity(0.75f);
		SkyLight->SetIntensity(0.72f);
		SkyLight->SetLightColor(FLinearColor::White);
	}
	else
	{
		LightRoom();
	}
}

void ACampaignEnvironment::LightRoom()
{
	// The lamp is warm at 4900 K: winter takes it toward a cold white and takes a little off it; summer warms it.
	// At night the lamp is all there is: warmer and a little lower, with the cool light of the windows gone,
	// so the table stands in a pool of lamplight and the shadows deepen.
	const float DayWarmth = 4900.0f + 1300.0f * SeasonWinter - 250.0f * SeasonSummer, DayKey = 7.0f - 0.9f * SeasonWinter + 0.3f * SeasonSummer, DayFill = 0.75f + 0.25f * SeasonWinter;
	if (bLamplight)
	{
		KeyLight->SetTemperature(4900.0f + 1300.0f * SeasonWinter * (1.0f - Night) - 250.0f * SeasonSummer - 900.0f * Night);
		KeyLight->SetIntensity(DayKey * (1.0f - 0.2f * Night));
		KeyLight->SetLightColor(FLinearColor::White);
		FillLight->SetIntensity(DayFill * (1.0f - 0.72f * Night));
		SkyLight->SetIntensity(0.72f * (1.0f - 0.55f * Night));
		SkyLight->SetLightColor(FLinearColor::White);
	}
	else
	{
		// Night is moonlight: cool and low, from the same quarter as the day's light, so the hills keep their
		// shadows. Enough of it to read the table by, with the colour drained and blue.
		// (The blue is in the light itself and not in the lens, so that the lamps of the towns stay warm in it.)
		// (The evening before it is a light of its own: the lamp's light amber and lower, the fill and the sky gone
		// cool, so that the slopes the lamp lights go gold and the shade goes blue. The moon comes in after it.)
		const FDuskLook& Look = DuskLook();
		const float Dusk = DuskOf(Night), Moon = MoonOf(Night), Left = 1.0f - Look.Dim * Dusk;
		KeyLight->SetTemperature(FMath::Lerp(DayWarmth, 6500.0f, Moon));
		// (the land carries moon colours of its own to go with this light: see the map's SetNight. The key is kept
		// low and blue so that the yellow of autumn woods does not shine out of the dark like lamps.)
		const FMoonLook& ByMoon = MoonLook();
		KeyLight->SetIntensity(FMath::Lerp(DayKey, ByMoon.Key, Moon) * Left);
		KeyLight->SetLightColor(FMath::Lerp(FMath::Lerp(FLinearColor::White, Look.Amber, Look.AmberShare * Dusk), ByMoon.KeyColour, Moon));
		FillLight->SetIntensity(FMath::Lerp(DayFill, ByMoon.Fill, Moon) * Left);
		FillLight->SetLightColor(FMath::Lerp(FLinearColor::White, Look.Cool, Look.CoolShare * Dusk));
		SkyLight->SetIntensity(FMath::Lerp(0.72f, ByMoon.Sky, Moon) * Left);
		SkyLight->SetLightColor(FMath::Lerp(FMath::Lerp(FLinearColor::White, Look.Cool, Look.CoolShare * Dusk), ByMoon.SkyColour, Moon));
	}
}

void ACampaignEnvironment::ApplyLight()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		return;
	}
	LightRoom();
	// And the towns on the map light their windows, and the lens takes the colour of the hour.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACampaignMapActor> It(World); It; ++It)
		{
			It->SetNight(Night, bLamplight);
		}
		for (TActorIterator<ACampaignCameraPawn> It(World); It; ++It)
		{
			It->SetNight(Night, bLamplight);
		}
	}
	AirNight = -1.0f; // (the air takes the hour's colour too)
}
