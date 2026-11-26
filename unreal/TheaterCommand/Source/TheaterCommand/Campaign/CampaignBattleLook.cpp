#include "Campaign/CampaignBattleLook.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Scene.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
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
	// How far the engine reckoned the air's paling before the battle (it stops at 96 km unless told), to be put back.
	float PalingDepthBefore = -1.0f;
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
	// (The sixth number is how dark the world is beyond the scene's own sea, which is a flat sheet that ends short of
	// the round world's horizon: past its edge the air's own ground shows, and at the engine's pale grey it made a
	// line along the horizon five levels brighter than the sea. It is the sea's colour, at this share of it.)
	// (And the seventh is how many kilometres out the engine reckons the air's paling: it stops at 96 unless told.
	// Eleven tries on Pearl Harbor's first frames, 19 November 2026: the air's own paling at four tenths, sky and
	// paling brightened together by three tenths. A line four or five levels deep stayed along the horizon whatever
	// the air was given; it went when the scene's haze was made to swallow the far sea and the sky's foot alike: see
	// AirHaze.)
	// (The haze over the sky at a third, 20 November 2026: laid wholly over it, the whole upper frame was white fog at
	// 217 to 226 with no blue, which flattened every cloud under it; the art director's trade-cumulus brief, item 3: the
	// air above the inversion clean, the sky overhead blue, 150 to 185 graded, with a pale haze band low on the horizon.
	// The air battle's own haze band now stands over the sea's far edge, so the line along the horizon this once cured
	// stays hidden: AAirBattle::LayHazeBand.)
	double Air = 1.0, Dust = 1.0, Far = 0.4, HazeOver = 0.35, Bright = 1.3, Beyond = 1.0, Deep = 300.0;
	Asked(TEXT("TheaterAirSky="), { &Air, &Dust, &Far, &HazeOver, &Bright, &Beyond, &Deep });
	if (IConsoleVariable* Depth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyAtmosphere.AerialPerspectiveLUT.Depth")))
	{
		PalingDepthBefore = Depth->GetFloat();
		Depth->Set(static_cast<float>(Deep), ECVF_SetByCode);
	}
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
	Made->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(static_cast<float>(Bright), static_cast<float>(Bright), static_cast<float>(Bright)));
	Made->SetGroundAlbedo(FColor(static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(36.0 * Beyond), 0, 255)), static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(80.0 * Beyond), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(95.0 * Beyond), 0, 255))));
	BattleSky = Made;
	UE_LOG(LogTheater, Display, TEXT("Battle look: a sky stands behind the air battle (air %.2f, dust %.2f, distance %.2f, haze over the sky %.2f, brightness %.2f, the world beyond the sea %.2f, paling reckoned to %.0f km); %d lamps were told to keep out of it or to light it"),
		Air, Dust, Far, HazeOver, Bright, Beyond, Deep, LampsKeptOut.Num());
}

void BattleLook::AirSkyEnd()
{
	if (USkyAtmosphereComponent* Sky = BattleSky.Get())
	{
		Sky->DestroyComponent();
	}
	BattleSky.Reset();
	if (PalingDepthBefore > 0.0f)
	{
		if (IConsoleVariable* Depth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyAtmosphere.AerialPerspectiveLUT.Depth")))
		{
			Depth->Set(PalingDepthBefore, ECVF_SetByCode);
		}
		PalingDepthBefore = -1.0f;
	}
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

void BattleLook::AirHaze(UExponentialHeightFogComponent& Haze, double UnitsPerMetre, double Start, double Thicker)
{
	// (The scene's own haze began 2 km from the eye. Looking steeply down from the strategy camera the sea under the
	// clouds then sat almost at the clouds' own brightness. It begins 15 km off, beyond everything under the eye, and
	// from there it is twice as thick and hides wholly what is far enough: the far sea and the foot of the sky sink
	// into one haze with no line between them, the far cloud keeps under half its contrast, and the near sea is as
	// dark under its clouds as the day makes it. Her figures on the five first frames with it: span 171 to 188,
	// blue over red 4 to 16, saturation 0.07 to 0.14.)
	// (Over land with no stage it begins 7.5 km off and is 2.6 times as thick: the caller's, AAirBattle.)
	double Density = Thicker * Haze.FogDensity, Falloff = Haze.FogHeightFalloff, Most = 1.0;
	Asked(TEXT("TheaterAirHaze="), { &Start, &Density, &Falloff, &Most });
	Haze.SetStartDistance(static_cast<float>(Start * UnitsPerMetre));
	Haze.SetFogDensity(static_cast<float>(Density));
	Haze.SetFogHeightFalloff(static_cast<float>(Falloff));
	Haze.SetFogMaxOpacity(static_cast<float>(Most));
	UE_LOG(LogTheater, Display, TEXT("Battle look: the air battle's haze begins %.0f m from the eye, density %.5f, falloff %.4f, at most %.2f"), Start, Density, Falloff, Most);
}
