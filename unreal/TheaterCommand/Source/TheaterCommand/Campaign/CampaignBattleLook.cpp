#include "Campaign/CampaignBattleLook.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Scene.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"

namespace
{
	/** The numbers of a switch such as "-TheaterAirHaze=7000,,0.004": each one given takes the place of the one kept. */
	void Asked(const TCHAR* Switch, TArray<double*> Kept)
	{
		FString Text;
		if (!FParse::Value(FCommandLine::Get(), Switch, Text, false))
		{
			return;
		}
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		for (int32 K = 0; K < Parts.Num() && K < Kept.Num(); ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Kept[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
}

namespace
{
	// The battle's sky while it stands, and the lamps that were told to keep out of it.
	TWeakObjectPtr<USkyAtmosphereComponent> BattleSky;
	TArray<TPair<TWeakObjectPtr<UDirectionalLightComponent>, bool>> LampsKeptOut;
}

void BattleLook::AirSkyBegin(AActor& Scene, UDirectionalLightComponent* Sun)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNoAirSky")) || BattleSky.IsValid())
	{
		return;
	}
	// (Tuned on Pearl Harbor's first frames, 19 November 2026: the engine's own paling with distance, laid over the
	// scene's haze, whited out everything past the middle of the strategy view; at four tenths the far cloud keeps
	// about half its contrast and the horizon lies wholly in haze.)
	double Air = 1.0, Dust = 1.0, Far = 0.4, HazeOver = 1.0, Bright = 1.0;
	Asked(TEXT("TheaterAirSky="), { &Air, &Dust, &Far, &HazeOver, &Bright });
	// (Only the battle's sun lights the air: the table's other lamps are fills, and a second sun in the sky would show.)
	LampsKeptOut.Reset();
	if (UWorld* World = Scene.GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UDirectionalLightComponent*> Lamps;
			It->GetComponents(Lamps);
			for (UDirectionalLightComponent* Lamp : Lamps)
			{
				const bool bIsSun = Lamp == Sun;
				if (Lamp && Lamp->IsUsedAsAtmosphereSunLight() != bIsSun)
				{
					LampsKeptOut.Emplace(Lamp, Lamp->IsUsedAsAtmosphereSunLight());
					Lamp->SetAtmosphereSunLight(bIsSun);
				}
			}
		}
	}
	USkyAtmosphereComponent* Made = NewObject<USkyAtmosphereComponent>(&Scene, TEXT("BattleSky"));
	if (!Made)
	{
		return;
	}
	if (USceneComponent* Root = Scene.GetRootComponent())
	{
		Made->SetupAttachment(Root);
	}
	Made->RegisterComponent();
	Made->SetRayleighScatteringScale(static_cast<float>(0.0331 * Air));
	Made->SetMieScatteringScale(static_cast<float>(0.003996 * Dust));
	Made->SetAerialPespectiveViewDistanceScale(static_cast<float>(Far));
	Made->SetHeightFogContribution(static_cast<float>(HazeOver));
	Made->SetSkyLuminanceFactor(FLinearColor(static_cast<float>(Bright), static_cast<float>(Bright), static_cast<float>(Bright)));
	BattleSky = Made;
	UE_LOG(LogTheater, Display, TEXT("Battle look: a sky stands behind the air battle (air %.2f, dust %.2f, distance %.2f, haze over the sky %.2f, brightness %.2f); %d lamps were told to keep out of it or to light it"),
		Air, Dust, Far, HazeOver, Bright, LampsKeptOut.Num());
}

void BattleLook::AirSkyEnd()
{
	if (USkyAtmosphereComponent* Sky = BattleSky.Get())
	{
		Sky->DestroyComponent();
	}
	BattleSky.Reset();
	for (const TPair<TWeakObjectPtr<UDirectionalLightComponent>, bool>& Kept : LampsKeptOut)
	{
		if (UDirectionalLightComponent* Lamp = Kept.Key.Get())
		{
			Lamp->SetAtmosphereSunLight(Kept.Value);
		}
	}
	LampsKeptOut.Reset();
}

void BattleLook::Grade(FPostProcessSettings& Lens)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNoBattleGrade")))
	{
		return;
	}
	// (The art director's measure of the air battle's first frames, 19 November 2026: flat and blue, a span of 96 to
	// 137 between the darkest and brightest thirtieths against the grade's 160 to 190, blue over red by 11 to 28
	// against 3 to 9.)
	// (Tuned on Pearl Harbor's first frames against her figures: the strategy view's span came to 143 to 162 and
	// blue over red to 8 to 12; the close view's to 172 to 180 and 6 to 15.)
	double Contrast = 1.42, Darks = 0.55, Middles = 0.72, Brights = 1.0, Red = 1.08, Blue = 0.87, Stops = 0.0;
	Asked(TEXT("TheaterBattleGrade="), { &Contrast, &Darks, &Middles, &Brights, &Red, &Blue, &Stops });
	Lens.bOverride_AutoExposureMethod = true;
	Lens.AutoExposureMethod = AEM_Manual;
	Lens.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Lens.AutoExposureApplyPhysicalCameraExposure = false;
	Lens.bOverride_AutoExposureBias = true;
	Lens.AutoExposureBias = static_cast<float>(Stops);
	Lens.bOverride_ColorContrast = true;
	Lens.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.0);
	Lens.bOverride_ColorSaturationShadows = true;
	Lens.ColorSaturationShadows = FVector4(Darks, Darks, Darks, 1.0);
	Lens.bOverride_ColorSaturationMidtones = true;
	Lens.ColorSaturationMidtones = FVector4(Middles, Middles, Middles, 1.0);
	Lens.bOverride_ColorSaturationHighlights = true;
	Lens.ColorSaturationHighlights = FVector4(Brights, Brights, Brights, 1.0);
	Lens.bOverride_ColorGain = true;
	Lens.ColorGain = FVector4(Red, 1.0, Blue, 1.0);
	static bool bSaid = false;
	if (!bSaid)
	{
		bSaid = true;
		UE_LOG(LogTheater, Display, TEXT("Battle look: the grade is on this battle's camera: contrast %.2f; colour kept in the darks %.2f, the middle tones %.2f, the brights %.2f; red %.2f and blue %.2f; exposure fixed at %+.2f stops"),
			Contrast, Darks, Middles, Brights, Red, Blue, Stops);
	}
}

void BattleLook::AirHaze(UExponentialHeightFogComponent& Haze, double UnitsPerMetre)
{
	// (The scene's own haze began 2 km from the eye. Looking steeply down from the strategy camera the sea under the
	// clouds then sat almost at the clouds' own brightness: it begins farther off, beyond what lies under the eye.)
	// (and it is half as dense as the scene first set it, now that the sky's own air pales the distance as well)
	double Start = 7000.0, Density = 0.5 * Haze.FogDensity, Falloff = Haze.FogHeightFalloff, Most = Haze.FogMaxOpacity;
	Asked(TEXT("TheaterAirHaze="), { &Start, &Density, &Falloff, &Most });
	Haze.SetStartDistance(static_cast<float>(Start * UnitsPerMetre));
	Haze.SetFogDensity(static_cast<float>(Density));
	Haze.SetFogHeightFalloff(static_cast<float>(Falloff));
	Haze.SetFogMaxOpacity(static_cast<float>(Most));
	UE_LOG(LogTheater, Display, TEXT("Battle look: the air battle's haze begins %.0f m from the eye, density %.5f, falloff %.4f, at most %.2f"), Start, Density, Falloff, Most);
}
