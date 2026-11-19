#include "Campaign/CampaignBattleLook.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/Scene.h"
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

void BattleLook::Grade(FPostProcessSettings& Lens)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterNoBattleGrade")))
	{
		return;
	}
	// (The art director's measure of the air battle's first frames, 19 November 2026: flat and blue, a span of 96 to
	// 137 between the darkest and brightest thirtieths against the grade's 160 to 190, blue over red by 11 to 28
	// against 3 to 9.)
	double Contrast = 1.25, Darks = 0.55, Middles = 0.72, Brights = 1.0, Red = 1.04, Blue = 0.93, Stops = 0.0;
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
	double Start = 7000.0, Density = Haze.FogDensity, Falloff = Haze.FogHeightFalloff, Most = Haze.FogMaxOpacity;
	Asked(TEXT("TheaterAirHaze="), { &Start, &Density, &Falloff, &Most });
	Haze.SetStartDistance(static_cast<float>(Start * UnitsPerMetre));
	Haze.SetFogDensity(static_cast<float>(Density));
	Haze.SetFogHeightFalloff(static_cast<float>(Falloff));
	Haze.SetFogMaxOpacity(static_cast<float>(Most));
	UE_LOG(LogTheater, Display, TEXT("Battle look: the air battle's haze begins %.0f m from the eye, density %.5f, falloff %.4f, at most %.2f"), Start, Density, Falloff, Most);
}
