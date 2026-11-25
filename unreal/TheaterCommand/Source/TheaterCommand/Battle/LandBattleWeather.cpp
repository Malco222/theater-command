// Rain and snow seen falling in the ground battle (release 2, the look bible's G10: Brian, 25 November, "better overall
// graphics"). The battle's own weather, as the campaign drew it and the recording carries it (its weather word and its
// sky: no new draw, so the same battle always has the same weather), read by LandWeather into what falls and how hard,
// and the day's changes for it: the drops round the eye (Effects/WeatherFall, M_LandFall), the haze thicker and the
// light duller (HideTheTable), the open ground wet with water standing in its hollows (LandField's Field::Rain), the
// ground, the hedges and the steel with a wet sheen, the rain heard (LandSound's rain bed). Looks only: nothing in any
// fight reads it.
//
// "-TheaterLandFalling" turns it on (the ship list's "falling" row), and "-TheaterLandFallingSnow" with it lets snow fall
// too (its own row, "fallingsnow": rain and snow pass the Mac apart); "-TheaterLandFall=rain,0.8" (or "snow,0.5", a
// strength alone, or "none") sets what falls whatever the day and either switch, for the Mac's stills; the numbers are
// Content/Effects/land-weather.json's.

#include "Battle/LandBattle.h"

#include "Battle/LandBattleState.h"
#include "Battle/LandWeather.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"

#include <algorithm>
#include <functional>
#include <string>

namespace
{
	std::string NarrowOf(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	FString WideOf(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	LandWeather::Book BookOf(const FWeatherFall* Fall)
	{
		if (!Fall)
		{
			return LandWeather::Book();
		}
		return [Fall](const char* Path, double& Out) { return Fall->Number(Path, Out); };
	}

	/** "-TheaterLandFall=rain,0.8": what falls, whatever the day (empty where not asked). */
	void ForcedOf(LandWeather::Asked& A)
	{
		FString Text;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFall="), Text, false))
		{
			LandWeather::ReadForced(NarrowOf(Text.TrimStartAndEnd()), A.ForcedKind, A.ForcedStrength);
		}
	}

	/** The day's sky as HideTheTable lays it: the recording's, or "-TheaterLandWeather=cover,deck,base"'s. */
	void SkyOf(const FLandBattleState& S, LandWeather::Asked& A)
	{
		double Cover = S.Rec.Weather.Cover;
		double Deck = S.Rec.Weather.Kind == "deck" ? 1.0 : 0.0;
		FString Text;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWeather="), Text, false))
		{
			TArray<FString> Parts;
			Text.ParseIntoArray(Parts, TEXT(","), false);
			if (Parts.Num() > 0 && !Parts[0].TrimStartAndEnd().IsEmpty())
			{
				Cover = FCString::Atod(*Parts[0]);
			}
			if (Parts.Num() > 1 && !Parts[1].TrimStartAndEnd().IsEmpty())
			{
				Deck = FCString::Atod(*Parts[1]);
			}
		}
		A.Cover = FMath::Clamp(FMath::IsFinite(Cover) ? Cover : 0.0, 0.0, 1.0);
		A.bDeck = Deck >= 0.5 || A.Cover >= 0.8;
	}

	/** The campaign's weather word over the field, as Open lays it ("-TheaterLandWeatherWord=" over the recording's). */
	std::string WordOf(const FLandBattleState& S)
	{
		std::string Word = S.Rec.WeatherWord;
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLandWeatherWord="), Asked))
		{
			Word = NarrowOf(Asked.TrimStartAndEnd().ToLower());
		}
		return Word;
	}

	bool FallingAsked()
	{
		FString Forced;
		return FParse::Param(FCommandLine::Get(), TEXT("TheaterLandFalling")) || FParse::Value(FCommandLine::Get(), TEXT("TheaterLandFall="), Forced, false);
	}

	/** Whether snow may fall too ("-TheaterLandFallingSnow", the ship list's "fallingsnow" row): rain and snow pass the
	 * Mac apart, so falling snow has its own switch, with "-TheaterLandFalling" (the art director, round 2). */
	bool SnowFallingAsked()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("TheaterLandFallingSnow"));
	}
}

double ALandBattle::RainOnField(const FString& Word, const FString& Look)
{
	bFalling = FallingAsked();
	if (!bFalling || !State.IsValid())
	{
		UE_LOG(LogTheater, Log, TEXT("Land battle: rain and snow seen falling off (-TheaterLandFalling turns them on, release 2)"));
		return 0.0;
	}
	if (!Fall)
	{
		Fall = MakeUnique<FWeatherFall>();
		Fall->LoadRecipe();
	}
	LandWeather::Asked A;
	A.Word = NarrowOf(Word.TrimStartAndEnd().ToLower());
	A.Look = NarrowOf(Look);
	SkyOf(*State, A);
	ForcedOf(A);
	const double Wet = LandWeather::RainOnField(A, BookOf(Fall.Get()));
	if (Wet > 0.0)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: rain falls over the field: its open ground %.2f wet (the effects' mud, water in its hollows)"), Wet);
	}
	return Wet;
}

void ALandBattle::PickWeather()
{
	if (!bFalling || !State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	LandWeather::Asked A;
	A.Word = WordOf(S);
	SkyOf(S, A);
	if (S.Field)
	{
		A.SnowLying = S.Field->SnowLying();
		A.Look = S.Field->LookWord();
	}
	ForcedOf(A);
	A.bSnowFalls = SnowFallingAsked();
	S.Weather = LandWeather::Pick(A, BookOf(Fall.Get()));
	const LandWeather::Weather& W = S.Weather;
	if (W.Kind == WeatherFall::Kind::None)
	{
		UE_LOG(LogTheater, Display, TEXT("Land battle: rain and snow seen falling (-TheaterLandFalling): %s"), *WideOf(W.Why));
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Land battle: rain and snow seen falling (-TheaterLandFalling): %s; %s at %.2f, the haze %.2f times as thick and %.0f%% of the way to %.2f, %.2f, %.2f, the light %.2f of the day's%s"),
		*WideOf(W.Why), ANSI_TO_TCHAR(WeatherFall::NameOf(W.Kind)), W.Strength, W.HazeTimes, W.HazeToward * 100.0, W.HazeInk[0], W.HazeInk[1], W.HazeInk[2], W.LightTimes,
		W.Kind == WeatherFall::Kind::Rain ? *FString::Printf(TEXT(", the open ground %.2f wet, the rain heard at %.2f"), W.Wet, W.Sound) : TEXT(""));
	// The rain heard (LandSound's rain bed, the mix's beds.place.rain).
	S.Mix.RainShare = W.Sound;
	// Wet things shine (M_LandSurface's and M_MapSurface's Roughness and Specular): the ground, the hedges', the trees'
	// and the clutter's paint, and the steel's. Snow leaves them dry.
	auto Wetted = [](UMaterialInstanceDynamic* Ink, double Rough, double Specular)
	{
		if (Ink && Rough >= 0.0 && Specular >= 0.0)
		{
			Ink->SetScalarParameterValue(TEXT("Roughness"), static_cast<float>(FMath::Clamp(Rough, 0.05, 1.0)));
			Ink->SetScalarParameterValue(TEXT("Specular"), static_cast<float>(FMath::Clamp(Specular, 0.0, 1.0)));
		}
	};
	Wetted(GroundInk, W.GroundRough, W.GroundSpecular);
	for (UMaterialInstanceDynamic* Ink : { ThingInk.Get(), BladeInk.Get(), GrainInk.Get(), BushInk.Get(), WoodInk.Get(), LivelyInk.Get(), PalmInk.Get() })
	{
		Wetted(Ink, W.ThingRough, W.ThingSpecular);
	}
	Wetted(SteelInk, W.SteelRough, W.SteelSpecular);
}

void ALandBattle::MakeFall()
{
	if (!bFalling || !Fall || !State.IsValid())
	{
		return;
	}
	const FLandBattleState& S = *State;
	const LandWeather::Weather& W = S.Weather;
	if (W.Kind == WeatherFall::Kind::None)
	{
		return;
	}
	// (the same battle, the same drops: the seed from the recording, by FNV-1a over its head, the same on every
	// machine; std::hash is not, libc++'s and libstdc++'s differ)
	uint64 Seed = 0xCBF29CE484222325ull;
	for (size_t K = 0; K < std::min<size_t>(S.RecText.size(), 4096); ++K)
	{
		Seed = (Seed ^ static_cast<uint8>(S.RecText[K])) * 0x100000001B3ull;
	}
	Seed ^= static_cast<uint64>(S.RecText.size()) * 0x9E3779B97F4A7C15ull;
	if (!Fall->Setup(Root, UnitsPerMetre, Seed))
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: the drops are not drawn (see above); the haze, the light and the wet ground are laid for the %s"), ANSI_TO_TCHAR(WeatherFall::NameOf(W.Kind)));
		return;
	}
	// The wind (metres a second; the battle's, as the effects kit's smoke drifts), turned into the world's own axes.
	const FVector Blowing = S.Rec.Weather.bWind ? WorldWay(S.Rec.Weather.WindX, S.Rec.Weather.WindY, 0.0) : WorldWay(2.6, 1.2, 0.0);
	Fall->SetWind(Blowing.X, Blowing.Y);
	// The drops' colour: the haze behind them as the day lights it, a little brighter (a drop catches the sky's light).
	const float InkTimes = static_cast<float>(W.InkOverHaze);
	Fall->SetInk(FLinearColor(FallInk.R * InkTimes, FallInk.G * InkTimes, FallInk.B * InkTimes, 1.0f)); // (its alpha kept 1)
	Fall->SetLight(1.0);
	Fall->SetFall(W.Kind, W.Strength);
	UE_LOG(LogTheater, Display, TEXT("Land battle: %s"), *Fall->Said());
}

void ALandBattle::DrawFall(const FVector& EyeWorld, float RealSeconds)
{
	if (!Fall || !Fall->IsFalling() || !State.IsValid())
	{
		return;
	}
	const FLandBattleState& S = *State;
	double ViewW = 1600.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewW = FMath::Max(64.0, Size.X);
	}
	const double Lens = Eye && Eye->GetCameraComponent() ? Eye->GetCameraComponent()->FieldOfView : 70.0;
	// (the drops hang still while the battle is held, and fall through the night's own shots, which run on their own clock)
	const bool bRunning = !S.bCard && (S.NightNow.bNight || S.SpeedNow > 0.0);
	Fall->Tick(EyeWorld, Lens, ViewW, RealSeconds, bRunning);
}
