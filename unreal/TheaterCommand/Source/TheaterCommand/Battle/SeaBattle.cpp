#include "Battle/SeaBattle.h"

#include "Battle/AirStage.h"
#include "Battle/SeaBattleScreen.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Battle/ShipGuns.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignFlags.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TheaterCommand.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SDPIScaler.h"

#include <cctype>
#include <string>
#include <utility>

/** What the scene keeps of the action: the recording, the playback core, and the battle's clock. */
struct FSeaBattleState
{
	explicit FSeaBattleState(uint64 Seed) : Show(Seed) {}
	SeaFeed::Recording Rec;
	SeaShow::Show Show;
	size_t Taken = 0;       // the recording's views handed to the show so far
	double BattleT = 0.0;   // the battle's clock now (seconds)
	double BeginT = 0.0, EndT = 0.0;
};

namespace SeaParts
{
	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	static std::string Narrow(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	/** A heading (degrees clockwise from north) as a way on the board (X east, Y north), and the way to her starboard. */
	static FVector2D Ahead(double HeadingDegrees)
	{
		const double R = FMath::DegreesToRadians(HeadingDegrees);
		return FVector2D(FMath::Sin(R), FMath::Cos(R));
	}

	static FVector2D Starboard(double HeadingDegrees)
	{
		const double R = FMath::DegreesToRadians(HeadingDegrees);
		return FVector2D(FMath::Cos(R), -FMath::Sin(R));
	}

	static float FromDb(double Gain)
	{
		return static_cast<float>(FMath::Pow(10.0, Gain / 20.0));
	}

	/** The kit's sea distance where it has one (the effects head 78f6edf: drawn out to 16 km, the far line's flashes,
	 * columns and fires seen 5 to 15 km off), else its air distance (4 km), so this builds on the kit before and after. */
	template <typename TKit>
	static void UseSeaDistance(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->UseSeaDistance(); })
		{
			Kit->UseSeaDistance();
		}
		else
		{
			Kit->UseAirDistance();
		}
	}

	/** Whether the kit has funnel smoke of its own (the effects head 52fffe1: the trail kind "funnel", whose TrailMove
	 * takes her power), so that the scene smokes from her funnels only where the kit can draw it, and builds on both. */
	template <typename TKit>
	static constexpr bool HasFunnelSmoke()
	{
		return requires(TKit* K, const FVector& V) { K->TrailMove(0.0, int32(0), V, V, 0.5f); };
	}

	/** A funnel's smoke this frame, with her power (0 to 1, her speed over her flank speed): only on a kit that has it. */
	template <typename TKit>
	static void FunnelMove(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, float Power)
	{
		if constexpr (HasFunnelSmoke<TKit>())
		{
			Kit->TrailMove(Now, Id, At, Velocity, Power);
		}
	}

	/** A puff of steam from a broken ship (the kit's Steam, where the kit has it; none on an older kit). */
	template <typename TKit>
	static void SteamPuff(TKit* Kit, int32 Id, const FVector& At, const FVector& Way, float Across, float Seconds)
	{
		if constexpr (requires(TKit* K, int32 I, const FVector& A, float C, const FString& W) { K->Steam(I, A, A, C, C, W); })
		{
			Kit->Steam(Id, At, Way, Across, Seconds, FString(TEXT("vent")));
		}
		else
		{
			(void)Kit;
			(void)Id;
			(void)At;
			(void)Way;
			(void)Across;
			(void)Seconds;
		}
	}

	/**
	 * Where the sun stands (degrees: its bearing clockwise from north, its height) at a latitude, in a month (0 January)
	 * and at an hour of the day (local sun time): the declination of the month's middle day and the hour's angle. The
	 * recordings carry the year and month, not the hour; the scene takes the hour from the battle's seed, by day.
	 */
	static void SunAt(double Latitude, int32 Month0, double Hour, double& OutAzimuth, double& OutElevation)
	{
		const double Day = 30.4 * FMath::Clamp(Month0, 0, 11) + 15.0;
		const double Decl = FMath::DegreesToRadians(23.44) * FMath::Sin(2.0 * UE_DOUBLE_PI * (284.0 + Day) / 365.0);
		const double Phi = FMath::DegreesToRadians(FMath::Clamp(Latitude, -80.0, 80.0));
		const double H = FMath::DegreesToRadians(15.0 * (Hour - 12.0));
		const double SinUp = FMath::Sin(Phi) * FMath::Sin(Decl) + FMath::Cos(Phi) * FMath::Cos(Decl) * FMath::Cos(H);
		OutElevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinUp, -1.0, 1.0)));
		const double Bearing = FMath::Atan2(-FMath::Sin(H) * FMath::Cos(Decl), FMath::Sin(Decl) * FMath::Cos(Phi) - FMath::Cos(Decl) * FMath::Sin(Phi) * FMath::Cos(H));
		OutAzimuth = FMath::Fmod(FMath::RadiansToDegrees(Bearing) + 360.0, 360.0);
	}

	/** The effects kit's size row for a column of water (the kit has no naval rows yet: the nearest it has). */
	static const TCHAR* SplashRow(const std::string& What)
	{
		return What == "heavy" ? TEXT("bomb500") : What == "medium" ? TEXT("bomb250") : TEXT("how150");
	}

	/** The effects kit's nation rows (us, gb, de, it, jp, su, as the air scene's KitNation) from a ship's nation (USA,
	 * UK, GER, ITA, JAP, SOV): empty for any other, which the kit draws in its default colour. */
	static FString KitNation(const std::string& Nation)
	{
		std::string Up;
		for (const char Ch : Nation) Up += static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
		if (Up == "USA" || Up == "US") return TEXT("us");
		if (Up == "UK" || Up == "GBR" || Up == "GB" || Up == "ENG") return TEXT("gb");
		if (Up == "GER" || Up == "DE") return TEXT("de");
		if (Up == "ITA" || Up == "IT") return TEXT("it");
		if (Up == "JAP" || Up == "JPN" || Up == "JP") return TEXT("jp");
		if (Up == "SOV" || Up == "USSR" || Up == "SU") return TEXT("su");
		return FString();
	}

	/** A ship's light guns firing at aircraft (Battle/ShipGuns' streams), in her nation's tracer colour where the kit
	 * takes the nation (as the air scene's AirGun); an older kit draws its one colour. */
	template <typename TKit>
	static void AirGun(TKit* Kit, double Now, int32 Gun, const FVector& Muzzle, const FVector& Aim, const FVector& Carry, const FString& Kind, float Rate, float Mv, const FString& Nation)
	{
		if constexpr (requires(TKit* K) { K->AirGun(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0f, FString()); })
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, Carry, Kind, Rate, Mv, Nation);
		}
		else
		{
			Kit->AirGun(Now, Gun, Muzzle, Aim, Carry, Kind, Rate, Mv);
		}
	}

	/** A value of the sound director's mix by its dotted path ("air.flak.soundEvery"): null where it has none. */
	static TSharedPtr<FJsonValue> MixAt(const TSharedPtr<FJsonObject>& Mix, const FString& Path)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> On = Mix;
		for (int32 K = 0; On.IsValid() && K < Steps.Num(); ++K)
		{
			const TSharedPtr<FJsonValue> Got = On->TryGetField(Steps[K]);
			if (!Got.IsValid() || K == Steps.Num() - 1)
			{
				return Got;
			}
			On = Got->Type == EJson::Object ? Got->AsObject() : TSharedPtr<FJsonObject>();
		}
		return nullptr;
	}

	/** The mix's take there: its word, or the Pick-th of its list (wrapping); empty where it has none. */
	static FString MixTake(const TSharedPtr<FJsonObject>& Mix, const FString& Path, int32 Pick = 0)
	{
		const TSharedPtr<FJsonValue> Got = MixAt(Mix, Path);
		if (Got.IsValid() && Got->Type == EJson::String)
		{
			return Got->AsString();
		}
		if (Got.IsValid() && Got->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& List = Got->AsArray();
			if (List.Num() > 0)
			{
				const TSharedPtr<FJsonValue>& One = List[FMath::Abs(Pick) % List.Num()];
				if (One.IsValid() && One->Type == EJson::String)
				{
					return One->AsString();
				}
			}
		}
		return FString();
	}

	/** The mix's number there; for a list, its last (a take's part [from, to] gives where it ends). */
	static double MixNumber(const TSharedPtr<FJsonObject>& Mix, const FString& Path, double Else)
	{
		const TSharedPtr<FJsonValue> Got = MixAt(Mix, Path);
		if (Got.IsValid() && Got->Type == EJson::Number)
		{
			return Got->AsNumber();
		}
		if (Got.IsValid() && Got->Type == EJson::Array && Got->AsArray().Num() > 0 && Got->AsArray().Last().IsValid() && Got->AsArray().Last()->Type == EJson::Number)
		{
			return Got->AsArray().Last()->AsNumber();
		}
		return Else;
	}

	/** The sound of a ship's guns by her class (the battle pack's takes). */
	static FName GunsOf(const std::string& Class)
	{
		return Class == "bb" ? FName(TEXT("ship_gun_heavy")) : Class == "dd" ? FName(TEXT("ship_guns")) : FName(TEXT("ship_gun_medium"));
	}

	/**
	 * The miniature kit's model for a strike's aircraft: a sculpted one of the name where Content/Models has it, else the
	 * kit's plain fighter or bomber in its navy's paint (as the air battle's ModelFor).
	 */
	static FString ModelName(const FString& Model, const FString& Nation, bool bFighter)
	{
		static const TMap<FString, FString> Known = {
			{ TEXT("B5N"), TEXT("Kate") }, { TEXT("D3A"), TEXT("Val") }, { TEXT("SBD"), TEXT("Dauntless") }, { TEXT("TBD"), TEXT("Devastator") },
			{ TEXT("TBF"), TEXT("Avenger") }, { TEXT("F4F"), TEXT("Wildcat") }, { TEXT("A6M"), TEXT("Zero") },
		};
		static TMap<FString, FString> Found;
		const FString Asked = Model + TEXT("|") + Nation + (bFighter ? TEXT("|f") : TEXT("|b"));
		if (const FString* Answered = Found.Find(Asked))
		{
			return *Answered;
		}
		const FString* Named = Known.Find(Model);
		const FString Name = Named ? *Named : Model;
		FMiniatureMesh Probe;
		const FString Out = !Name.IsEmpty() && !Name.Contains(TEXT(":")) && Probe.Vehicle(Name + TEXT(":") + Nation, FVector::ZeroVector, 1.0, 0.0, false)
			? Name + TEXT(":") + Nation : (bFighter ? TEXT("Fighter:") : TEXT("Bomber:")) + Nation;
		Found.Add(Asked, Out);
		return Out;
	}

	/** The haze's colour as the room had it (the engine's field has had two names). */
	template <typename TFog>
	static FLinearColor HazeColour(const TFog* Fog, const FLinearColor& Else)
	{
		if constexpr (requires(const TFog* F) { FLinearColor(F->FogInscatteringLuminance); })
		{
			return Fog->FogInscatteringLuminance;
		}
		else if constexpr (requires(const TFog* F) { FLinearColor(F->FogInscatteringColor); })
		{
			return Fog->FogInscatteringColor;
		}
		else
		{
			return Else;
		}
	}

	// The ids the scene gives the kit's lasting things, apart from each other and from the stage's (238000 and up).
	// (a ship's funnels: her key times four, and 0 to 2 a funnel each or 3 her one plume, from a million: out of the way)
	static constexpr int32 FireIds = 810000, FallIds = 830000, OilIds = 840000, FunnelIds = 1000000;
	// (a broken ship's smoke from her break, a trail, by her key; the steam from it, a puff at a time, by her key times 8)
	static constexpr int32 BreakIds = 1900000, BreakSteamIds = 2200000;
	/** Funnel smoke (the kit's 3,000 pieces are shared by everything): a plume a funnel on the NearFunnels ships nearest
	 * the eye, one plume a ship on the rest of the FunnelShips nearest, none beyond FunnelReach metres. At 25 knots a
	 * destroyer's plume is about 60 pieces near, 33 far; a capital ship's 36 and 30 (the effects thread, 21 November). */
	static constexpr int32 NearFunnels = 3, FunnelShips = 8;
	static constexpr double FunnelReach = 12000.0;
	/** Seconds the camera stands on a shot's subject before its moment (watched; a picture stands on it from the last). */
	static constexpr double CutBefore = 4.0;
	/** The longest a wake point is kept (seconds: each fades by how far astern it lies, WakeRun), how long a stopped
	 * torpedo's streak takes to go, and how long a streak is behind its torpedo (metres; the art director's 11-19 ruling:
	 * 100 to 200 m). */
	static constexpr double WakeLife = 240.0, TrackFade = 15.0, TrackStreak = 150.0;
	/** How far astern a ship's foam lasts (metres): three of her lengths, never under 300 m nor over 800 m (the art
	 * director, Samar r2e: "widens and breaks up over 300-800 m and fades to nothing"). */
	static double WakeRun(double Long)
	{
		return FMath::Clamp(3.0 * Long, 300.0, 800.0);
	}
	/** How whole a wake's foam still is (the material breaks up the rest) at Lengths of her length astern, Run her
	 * lengths to its end: the campaign map's passed curve, whole to half a length, about half left at one, a fifth at
	 * its end. */
	static double WakeWhole(double Lengths, double Run)
	{
		return Lengths < 0.5 ? FMath::Lerp(1.0, 0.58, Lengths / 0.5) : Lengths < 1.0 ? FMath::Lerp(0.58, 0.43, (Lengths - 0.5) / 0.5)
			: Lengths < 2.0 ? FMath::Lerp(0.43, 0.31, Lengths - 1.0) : FMath::Lerp(0.31, 0.2, FMath::Min((Lengths - 2.0) / FMath::Max(Run - 2.0, 0.1), 1.0));
	}
	/** A wake's half width at Lengths of her length astern: her stern's width, a beam by half a length, two at its end. */
	static double WakeHalf(double Beam, double Lengths, double Run)
	{
		return Beam * 0.5 * (Lengths < 0.5 ? FMath::Lerp(0.72, 1.0, Lengths / 0.5) : FMath::Lerp(1.0, 2.0, FMath::Min((Lengths - 0.5) / FMath::Max(Run - 0.5, 0.1), 1.0)));
	}
	/** A number from 0 to 1 that is the same each time for the same X (a foam edge's raggedness, an oil slick's). */
	static double Hash01(double X)
	{
		return FMath::Frac(FMath::Sin(X * 12.9898 + 78.233) * 43758.5453);
	}
	static const FLinearColor HazeInk(0.58f, 0.67f, 0.76f);
	static const FColor Foam(226, 232, 234);

	/** The haze at the horizon as the screen shows it, as pixel / 255 (not linearised): the band just above the sea in
	 * the Mac stills of 21 November (sea-r2b: the three battles at their hours of 8 in the morning to 3 in the
	 * afternoon, 221, 221, 223 in each). */
	static const FLinearColor SkyOnScreen(221.0f / 255.0f, 221.0f / 255.0f, 223.0f / 255.0f);

	/** The sky just over the horizon as a still shows it (SkyOnScreen), or "-TheaterSeaSky=red,green,blue" (0 to 255). */
	static FLinearColor SkyColour()
	{
		FLinearColor Sky = SkyOnScreen;
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaSky="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(","), false) == 3)
		{
			Sky.R = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[0]), 0.0, 255.0) / 255.0);
			Sky.G = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[1]), 0.0, 255.0) / 255.0);
			Sky.B = static_cast<float>(FMath::Clamp(FCString::Atod(*Bits[2]), 0.0, 255.0) / 255.0);
		}
		return Sky;
	}

	/** Tells the kit the horizon's haze, where the kit takes it (the effects head 86d71f2: smoke far off pales into it,
	 * a mid grey until it is told), so this builds on the kit before and after. "-TheaterSeaSky=red,green,blue" (0 to
	 * 255, as a still's pixel) tries another. The scene's sky and hour are set once a battle and have no weather yet,
	 * so it is told once. */
	template <typename TKit>
	static void SetSky(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->SetSky(FLinearColor()); })
		{
			Kit->SetSky(SkyColour());
		}
		else
		{
			(void)Kit;
		}
	}
}

ASeaBattle::ASeaBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

bool ASeaBattle::WantsReplay(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattle="), OutKey) && !OutKey.IsEmpty();
}

void ASeaBattle::ReplayWhenReady(const FString& InKey)
{
	PendingReplay = InKey;
	SetActorTickEnabled(true);
}

FVector ASeaBattle::World(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector ASeaBattle::WorldVector(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector ASeaBattle::BoardOf(const FVector& WorldAt) const
{
	return FVector(WorldAt.Y, WorldAt.X, WorldAt.Z) / UnitsPerMetre;
}

// ---------------------------------------------------------------------------------------------------------------
// Opening the scene.

bool ASeaBattle::BeginReplay(const FString& InKey)
{
	using namespace SeaParts;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleShots="), ShotFolder);
	float Pace = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleSpeed="), Pace);
	Speed = FMath::Clamp(static_cast<double>(Pace), 0.25, 4.0);
	// (funnel smoke wherever the kit has its own "funnel" kind; with a kit before it, none: the air trail borrowed for it
	// stood as a fence of streaks behind a ship, the first Mac frames of 21 November. "-TheaterSeaNoFunnels" for none.)
	bFunnels = SeaParts::HasFunnelSmoke<FBattleEffects>() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaNoFunnels"));
	{
		FString Asked;
		TArray<FString> Bits;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaWake="), Asked, false) && Asked.ParseIntoArray(Bits, TEXT(","), false) >= 1)
		{
			WakeBright = Bits[0].TrimStartAndEnd().IsEmpty() ? WakeBright : FMath::Clamp(FCString::Atod(*Bits[0]), 0.0, 4.0);
			if (Bits.Num() >= 2 && !Bits[1].TrimStartAndEnd().IsEmpty())
			{
				WakeStrength = FMath::Clamp(FCString::Atod(*Bits[1]), 0.0, 4.0);
			}
		}
	}
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/SeaBattles") / InKey + TEXT(".json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: no recorded action %s (is Content/Data/SeaBattles staged with the build?)"), *File);
		return false;
	}
	SeaFeed::Recording Rec;
	std::string Error;
	if (!SeaFeed::ReadRecording(Narrow(Text), Rec, Error) || Rec.Views.empty())
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: %s: %s"), *File, Error.empty() ? TEXT("no views") : *Wide(Error));
		return false;
	}
	const uint64 Seed = Rec.Seed ? Rec.Seed : 239;
	State = MakeShared<FSeaBattleState>(Seed);
	FSeaBattleState& S = *State;
	S.Rec = std::move(Rec);
	S.Show.SetField(S.Rec.Board);
	S.BeginT = S.Rec.Views.front().Head.T;
	S.EndT = S.Rec.Views.back().Head.T;
	S.BattleT = S.BeginT;
	RecordKey = InKey;
	UWorld* Here = GetWorld();
	if (!Here)
	{
		return false;
	}

	// The mix (the sound director's mix.json): the scene reads its numbers, never copies them.
	{
		FString MixText;
		if (FFileHelper::LoadFileToString(MixText, *(FPaths::ProjectContentDir() / TEXT("Data/Sound/mix.json"))))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MixText), Mix);
		}
		if (!Mix.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: Content/Data/Sound/mix.json is missing: the battle is silent."));
		}
	}

	// The effects kit, drawn out to the sea distance (a fleet action is watched from far off).
	Effects = MakeUnique<FBattleEffects>();
	// (its ground: a ship's deck where the point lies over one, so a hit that is not on armour bursts on her deck, not at
	// the sea inside her hull; the sea's face everywhere else)
	if (!Effects->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
		{
			FVector Deck;
			if (!bOnWater && Stage && Stage->DeckUnder(Point, Deck))
			{
				return Deck;
			}
			return FVector(Point.X, Point.Y, 0.0);
		}))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: the effects kit did not begin (its recipe or material is missing): no guns, columns or smoke are drawn."));
		Effects.Reset();
	}
	else
	{
		// (a fleet action is fought 10 to 15 km apart: the air battle's 4 km would leave the far line unlit)
		UseSeaDistance(Effects.Get());
		const SeaShow::Vec Blow = S.Show.Wind();
		Effects->SetWind(FVector(Blow.Y, Blow.X, 0.0));
	}

	// The sun of the battle's day: its latitude and month, at an hour of the day from its seed (8 in the morning to 3 in
	// the afternoon), or "-TheaterSeaHour=".
	double Hour = 8.0 + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;
	{
		float Asked = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaHour="), Asked) && Asked >= 5.0f && Asked <= 19.0f)
		{
			Hour = Asked;
		}
	}
	double SunAzimuth = 120.0, SunElevation = 30.0;
	SunAt(S.Rec.Board.Lat, S.Rec.Board.Month, Hour, SunAzimuth, SunElevation);
	SunElevation = FMath::Clamp(SunElevation, 12.0, 70.0);

	// The open sea and its ships: the Pearl Harbor stage's sea, hulls, hit looks and ends, raised with no land.
	Stage = Here->SpawnActor<AAirStage>(FVector::ZeroVector, FRotator::ZeroRotator);
	const double Middle = S.Rec.Board.Size * 0.5;
	if (Stage)
	{
		// (its sea the battle's own wind's, Brian's 23:37Z "make the water more realistic": the waves run the way it
		// blows, as long, as steep and as broken by whitecaps as its speed raises them; and the sky it mirrors low across
		// it, the sky a still shows over the horizon at about 0.97 of its light, the battle's grade bringing it back up)
		const SeaShow::Vec Blow = S.Show.Wind();
		const double Blowing = S.Show.WindSpeed();
		const FLinearColor Sky = SeaParts::SkyColour();
		Stage->SetSeaWind(Blowing > 0.0 ? FMath::Atan2(Blow.X, Blow.Y) : 0.0, Blowing, FLinearColor(Sky.R * 0.97f, Sky.G * 0.97f, Sky.B * 0.97f));
	}
	if (!Stage || !Stage->RaiseSea(FVector2D(Middle, Middle), SunAzimuth, SunElevation, UnitsPerMetre, Effects.Get(), Seed))
	{
		UE_LOG(LogTheater, Warning, TEXT("Sea battle: the open sea could not be raised."));
		return false;
	}
	if (Effects)
	{
		Effects->SetSun(Stage->TowardSun());
	}

	// The paint: the wakes and tracks see-through and unlit (M_Highlight, by each point's alpha), the aircraft as the
	// air battle's.
	{
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
		{
			MarkInk = UMaterialInstanceDynamic::Create(Glow, this);
			MarkInk->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(WakeBright));
		}
		// (the foam on the map's wake ink, its noise breaking it up as it ages: Brightness a little over the marks' to
		// match their white, as its foam colour is greyer; no west edge at sea)
		if (UMaterialInterface* Wake = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Wake.M_Wake")))
		{
			FoamInk = UMaterialInstanceDynamic::Create(Wake, this);
			FoamInk->SetScalarParameterValue(TEXT("Brightness"), static_cast<float>(WakeBright * 1.15));
			FoamInk->SetScalarParameterValue(TEXT("WestEdge"), -1.0e9f);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea battle: M_Wake is missing (run Tools/build_content.sh): the wakes are drawn whole, on M_Highlight."));
		}
		UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
		if (!Surface)
		{
			Surface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		PlaneInk = UMaterialInstanceDynamic::Create(Surface, this);
		PlaneInk->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
		PlaneInk->SetScalarParameterValue(TEXT("Specular"), 0.45f);
		PlaneInk->SetScalarParameterValue(TEXT("Grain"), 0.0f);
		PlaneInk->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
	}
	Marks = NewObject<UProceduralMeshComponent>(this, TEXT("SeaMarks"));
	Marks->SetupAttachment(Root);
	Marks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marks->SetCastShadow(false);
	Marks->bUseAsyncCooking = false;
	Marks->RegisterComponent();
	Foam = NewObject<UProceduralMeshComponent>(this, TEXT("SeaFoam"));
	Foam->SetupAttachment(Root);
	Foam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Foam->SetCastShadow(false);
	Foam->bUseAsyncCooking = false;
	Foam->RegisterComponent();

	Eye = Here->SpawnActor<ACameraActor>();
	if (Eye)
	{
		Eye->GetCameraComponent()->SetFieldOfView(50.0f);
		// (the whole window, whatever its shape: a camera actor keeps 16:9 and bars the rest)
		Eye->GetCameraComponent()->bConstrainAspectRatio = false;
		Eye->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Eye->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		// (the art director's battle grade, on battle cameras only)
		BattleLook::Grade(Eye->GetCameraComponent()->PostProcessSettings);
	}
	TakeTheRoom(SunAzimuth, SunElevation);
	if (Effects)
	{
		// (the sky is set: the kit's far smoke pales into the haze the screen shows at the horizon)
		SeaParts::SetSky(Effects.Get());
	}
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
		}
		if (AHUD* Hud = Controller->GetHUD())
		{
			bHudWasShown = Hud->bShowHUD;
			Hud->bShowHUD = false;
			// (the campaign's own frame is a screen layer of its own, under the bar: off too, as the air scene has it,
			// or it stands in every picture now that the pictures take the screen)
			if (ACampaignHUD* Frame = Cast<ACampaignHUD>(Hud))
			{
				Frame->ShowFrame(false);
			}
		}
	}

	PlanShots();
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
	}
	// "-TheaterSeaBattleFrom=300": the action from there (what lasts of the time before is kept, as a picture run's skip).
	bool bStartedLater = false;
	{
		float From = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleFrom="), From) && From > S.BeginT + 1.0)
		{
			const double To = FMath::Min(static_cast<double>(From), S.EndT);
			bSkipping = true;
			for (double T = S.BattleT + 1.0; T < To; T += 1.0)
			{
				StepTo(T, true);
			}
			StepTo(To, true);
			bSkipping = false;
			bStartedLater = true;
		}
	}
	Feed(S.BattleT);
	Beds();

	// The tug-of-war bar (release 2; the UI director's ruling of 21 November): the player's flag at its left end and the
	// enemy's at its right, the card's own flags (Campaign/CampaignFlags); level at the start, or where the action stands
	// when it starts later.
	{
		const bool bDefender = SeaFeed::SideOf(S.Rec.Views.front().Head.Me) == 1;
		auto Flag = [this](const std::string& Nation) -> UTexture2D*
		{
			const FString Tag = Wide(Nation);
			if (Tag.IsEmpty())
			{
				return nullptr;
			}
			TArray<FLinearColor> Pixels;
			CampaignFlags::Paint(Tag, FLinearColor(0.4f, 0.4f, 0.4f), Pixels);
			return CampaignFlags::MakeTexture(Pixels, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *(TEXT("SeaFlag_") + Tag)));
		};
		MyFlag = Flag(bDefender ? S.Rec.Board.DefNat : S.Rec.Board.AttNat);
		TheirFlag = Flag(bDefender ? S.Rec.Board.AttNat : S.Rec.Board.DefNat);
		BarKnot = 0.5;
		MoveKnot(0.0, bStartedLater);
		ShowScreen();
	}
	bBegun = true;
	SetActorTickEnabled(true);
	UE_LOG(LogTheater, Display, TEXT("Sea battle: %s, %s (%s), %d views, %.0f s, the sun at %.0f degrees and %.0f up at %.1f h, %d shots%s."), *RecordKey,
		*Wide(S.Rec.Board.Title), *Wide(S.Rec.Board.Date), static_cast<int32>(S.Rec.Views.size()), S.EndT - S.BeginT, SunAzimuth, SunElevation, Hour, Shots.Num(),
		ShotFolder.IsEmpty() ? TEXT("") : *(TEXT(", pictures into ") + ShotFolder));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// The room: the table put away, the battle's light, haze and sky; given back after.

void ASeaBattle::TakeTheRoom(double SunAzimuth, double SunElevation)
{
	if (bRoomTaken)
	{
		return;
	}
	bRoomTaken = true;
	const FVector Sun = Stage ? Stage->TowardSun() : FRotator(SunElevation, SunAzimuth, 0.0).Vector();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			if (!It->IsHidden())
			{
				KeptHidden.Add(*It);
				It->SetActorHiddenInGame(true);
			}
		}
		ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It);
		if (!Environment)
		{
			continue;
		}
		KeptRooms.Add(Environment);
		Environment->SetActorTickEnabled(false);
		// The haze of a battle seen from far off (the air battle's, the art director's ruling): none near the eye, thick
		// toward the horizon, so the near ships keep their darks and the far line greys into the sea's edge.
		TArray<UExponentialHeightFogComponent*> Fogs;
		Environment->GetComponents(Fogs);
		for (UExponentialHeightFogComponent* Fog : Fogs)
		{
			FKeptHaze& Kept = KeptHazes.AddDefaulted_GetRef();
			Kept.Haze = Fog;
			Kept.Start = Fog->StartDistance;
			Kept.Density = Fog->FogDensity;
			Kept.Falloff = Fog->FogHeightFalloff;
			Kept.MostOpacity = Fog->FogMaxOpacity;
			Kept.Colour = SeaParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
			Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));
			Fog->SetFogDensity(0.00045f);
			Fog->SetFogHeightFalloff(0.006f);
			Fog->SetFogMaxOpacity(0.8f);
			Fog->SetFogInscatteringColor(SeaParts::HazeInk);
			BattleLook::AirHaze(*Fog, UnitsPerMetre);
		}
		TArray<UDirectionalLightComponent*> Lights;
		Environment->GetComponents(Lights);
		UDirectionalLightComponent* Main = nullptr;
		for (UDirectionalLightComponent* Light : Lights)
		{
			FKeptLamp& Kept = KeptLamps.AddDefaulted_GetRef();
			Kept.Lamp = Light;
			Kept.Rotation = Light->GetRelativeRotation();
			Kept.Intensity = Light->Intensity;
			Kept.Colour = Light->GetLightColor();
			Kept.Temperature = Light->Temperature;
			Kept.SourceAngle = Light->LightSourceAngle;
			Kept.SunDisc = Light->AtmosphereSunDiskColorScale;
			if (Light->GetFName() == TEXT("KeyLight") || (!Main && Lights.Num() == 1))
			{
				Main = Light;
			}
		}
		if (!Main)
		{
			for (UDirectionalLightComponent* Light : Lights)
			{
				if (!Main || Light->Intensity > Main->Intensity)
				{
					Main = Light;
				}
			}
		}
		if (Main)
		{
			BattleKey = Main;
			Main->SetWorldRotation((-Sun).Rotation());
			// (a sky behind the battle, lit by its sun; taken away in GiveBackTheRoom)
			BattleLook::AirSkyBegin(*this, Main);
			// The sun a hard small disc, as the air battle's (the art director on build 1750): the true sun's half degree,
			// holding six thousandths of its light, the haze throwing it forward at 0.75.
			Main->LightSourceAngle = 0.53f;
			Main->AtmosphereSunDiskColorScale = FLinearColor(0.006f, 0.006f, 0.006f);
			Main->MarkRenderStateDirty();
			TArray<USkyAtmosphereComponent*> Atmospheres;
			GetComponents(Atmospheres);
			for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
			{
				Atmosphere->SetMieAnisotropy(0.75f);
			}
		}
		TArray<USkyLightComponent*> Skies;
		Environment->GetComponents(Skies);
		for (USkyLightComponent* Sky : Skies)
		{
			FKeptSky& Kept = KeptSkies.AddDefaulted_GetRef();
			Kept.Sky = Sky;
			Kept.Intensity = Sky->Intensity;
			Kept.Colour = Sky->GetLightColor();
		}
	}
	LightTheBattle();
}

void ASeaBattle::LightTheBattle()
{
	// The battle's day, as the air battle's: every lamp white, the key at the table's day strength and warmth, the
	// others at the fill's, the sky's light the day's; said again where the room has changed it.
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bMain = Light == BattleKey.Get();
		const float Want = bMain ? 7.0f : 0.75f;
		if (!Light->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Light->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		if (bMain && !FMath::IsNearlyEqual(Light->Temperature, 4900.0f, 1.0f))
		{
			Light->SetTemperature(4900.0f);
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		USkyLightComponent* Sky = Kept.Sky.Get();
		if (!Sky)
		{
			continue;
		}
		if (!Sky->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Sky->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Sky->Intensity, 0.72f, 0.001f))
		{
			Sky->SetIntensity(0.72f);
		}
	}
	// (the campaign's controller may stand a piece again under the pointer: put away again)
	for (TActorIterator<AArmyPiece> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			KeptHidden.AddUnique(*It);
			It->SetActorHiddenInGame(true);
		}
	}
}

void ASeaBattle::GiveBackTheRoom()
{
	if (!bRoomTaken)
	{
		return;
	}
	bRoomTaken = false;
	for (const FKeptHaze& Kept : KeptHazes)
	{
		if (UExponentialHeightFogComponent* Fog = Kept.Haze.Get())
		{
			Fog->SetStartDistance(Kept.Start);
			Fog->SetFogDensity(Kept.Density);
			Fog->SetFogHeightFalloff(Kept.Falloff);
			Fog->SetFogMaxOpacity(Kept.MostOpacity);
			Fog->SetFogInscatteringColor(Kept.Colour);
		}
	}
	BattleLook::AirSkyEnd();
	for (const FKeptLamp& Kept : KeptLamps)
	{
		if (UDirectionalLightComponent* Light = Kept.Lamp.Get())
		{
			Light->SetRelativeRotation(Kept.Rotation);
			Light->SetIntensity(Kept.Intensity);
			Light->SetLightColor(Kept.Colour);
			Light->SetTemperature(Kept.Temperature);
			Light->LightSourceAngle = Kept.SourceAngle;
			Light->AtmosphereSunDiskColorScale = Kept.SunDisc;
			Light->MarkRenderStateDirty();
		}
	}
	for (const FKeptSky& Kept : KeptSkies)
	{
		if (USkyLightComponent* Sky = Kept.Sky.Get())
		{
			Sky->SetIntensity(Kept.Intensity);
			Sky->SetLightColor(Kept.Colour);
		}
	}
	for (const TWeakObjectPtr<ACampaignEnvironment>& Room : KeptRooms)
	{
		if (Room.IsValid())
		{
			Room->SetActorTickEnabled(true);
		}
	}
	for (const TWeakObjectPtr<AActor>& Hidden : KeptHidden)
	{
		if (Hidden.IsValid())
		{
			Hidden->SetActorHiddenInGame(false);
		}
	}
	if (UWorld* Here = GetWorld())
	{
		if (APlayerController* Controller = Here->GetFirstPlayerController())
		{
			if (AHUD* Hud = Controller->GetHUD())
			{
				Hud->bShowHUD = bHudWasShown;
				if (ACampaignHUD* Frame = Cast<ACampaignHUD>(Hud))
				{
					Frame->ShowFrame(true);
				}
			}
		}
	}
	BattleKey.Reset();
	KeptHazes.Reset();
	KeptLamps.Reset();
	KeptSkies.Reset();
	KeptRooms.Reset();
	KeptHidden.Reset();
}

// ---------------------------------------------------------------------------------------------------------------
// The action, frame by frame.

void ASeaBattle::Feed(double T)
{
	FSeaBattleState& S = *State;
	auto TakeNext = [&S]()
	{
		const SeaFeed::Frame& F = S.Rec.Views[S.Taken++];
		S.Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
	};
	// (one view in hand ahead of what is shown, so the picture between views has somewhere to go: the cloud test's way)
	if (S.Taken == 0 && !S.Rec.Views.empty())
	{
		TakeNext();
	}
	while (S.Taken < S.Rec.Views.size() && S.Rec.Views[S.Taken - 1].Head.T <= T + 1e-9)
	{
		TakeNext();
	}
}

void ASeaBattle::StepTo(double T, bool bSkip)
{
	FSeaBattleState& S = *State;
	Feed(T);
	const FVector Seen = BoardOf(EyeAt);
	S.Show.Eye = SeaShow::Vec(Seen.X, Seen.Y, FMath::Max(Seen.Z, 2.0));
	S.Show.Advance(T);
	S.BattleT = T;
	LayAndSteer();
	LayWakes();
	PlayEffects(bSkip);
}

void ASeaBattle::LayAndSteer()
{
	FSeaBattleState& S = *State;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const FVector2D At(Ship.At.X, Ship.At.Y);
		const double Face = FMath::DegreesToRadians(Ship.Heading);
		if (!Laid.Contains(Ship.Key))
		{
			FStageShip Given;
			Given.Id = Ship.Key;
			Given.Class = SeaParts::Wide(Ship.Class);
			Given.Nation = SeaParts::Wide(Ship.Nation);
			Given.At = At;
			Given.Face = Face;
			Stage->SetShips(TArray<FStageShip>{ Given });
			Laid.Add(Ship.Key);
			double Long = Ship.Long;
			FVector Bow = FVector::ZeroVector;
			bool bHarbour = false;
			if (Stage->ShipLie(Ship.Key, Long, Bow, bHarbour))
			{
				DrawnLong.Add(Ship.Key, Long);
			}
		}
		Stage->Steer(Ship.Key, At, Face, Ship.Heel, Ship.Pitch, Ship.Heave);
		LastSeen.Add(Ship.Key, FVector4(At.X, At.Y, Ship.Heading, DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long));
	}
}

void ASeaBattle::PlayEffects(bool bSkip)
{
	using namespace SeaParts;
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	FBattleEffects* Kit = Effects.Get();
	// (the words, each said once a step: six carriers flying off alike at one moment are one line, "(6 times)")
	TArray<TPair<FString, int32>> Said;
	for (const SeaShow::Fx& E : S.Show.Effects)
	{
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		const FVector OnSea = World(E.At.X, E.At.Y, 0.0);
		// What lasts, played even while a picture run skips ahead: every hit on the ship it struck, a ship's end, the
		// torpedo tracks, oil burning, the words.
		switch (E.Kind)
		{
		case SeaShow::FxKind::ShellHit:
		case SeaShow::FxKind::BombHit:
			// Every hit the battle lands shows on the ship it struck (Brian, 21 November: the odds the rules', but a hit always
			// marks what it hit). Her looks (Battle/ShipLooks, by the shell's or the bomb's weight) keep her hurt, list,
			// fires and steam; in a stretch a picture run skips, they play from the moment it lands, so a ship hit while
			// the run skipped is as hurt in the next picture as she should be.
			Stage->Hit(E.Key, Wide(E.What), Now);
			break;
		case SeaShow::FxKind::TorpedoHit:
			// (its sound is the battle's own cue)
			Stage->Hit(E.Key, TEXT("torpedo"), Now);
			break;
		case SeaShow::FxKind::Sink:
			Stage->Sink(E.Key, Wide(E.What), E.Size > 0.5, Now);
			break;
		// (the torpedo tracks: a track begun while a run skips is drawn from where it was fired, its head where it is now)
		case SeaShow::FxKind::TrackStart:
		{
			FTrackLine& Line = Tracks.FindOrAdd(E.Key);
			Line.From = Line.Head = FVector2D(E.At.X, E.At.Y);
			Line.StopT = -1.0;
			// (a Japanese warship's torpedo from 1938 is the oxygen-driven Type 93: a third of the bubbles; the art director,
			// Samar r2e frame 07)
			Line.bOxygen = KitNation(E.Text) == TEXT("jp") && S.Rec.Board.Year >= 1938;
			break;
		}
		case SeaShow::FxKind::TrackMove:
			if (FTrackLine* Line = Tracks.Find(E.Key))
			{
				Line->Head = FVector2D(E.At.X, E.At.Y);
			}
			break;
		case SeaShow::FxKind::TrackStop:
			if (FTrackLine* Line = Tracks.Find(E.Key))
			{
				Line->Head = FVector2D(E.At.X, E.At.Y);
				Line->StopT = Now;
			}
			break;
		case SeaShow::FxKind::OilFire:
			// (where a ship went down the stage lights her own column as she goes: only a burning ship's is the scene's)
			if (E.What == "sunk")
			{
				break;
			}
			if (bSkip)
			{
				FOilLater& Oil = OilLater.AddDefaulted_GetRef();
				Oil.Id = OilIds + E.Key;
				Oil.At = FVector2D(E.At.X, E.At.Y);
				Oil.Reach = E.Size;
				Oil.Until = E.Until;
				Oil.Since = E.Since;
				Oil.LitAt = Now;
				Oil.Kind = Wide(E.What);
			}
			else if (Kit)
			{
				// (on the water where she caught fire, not on her deck: nothing moves an oil fire, and she limps on)
				TGuardValue<bool> Water(bOnWater, true);
				Kit->OilFire(OilIds + E.Key, OnSea, static_cast<float>(E.Size), Wide(E.What), E.Until, static_cast<float>(E.Since));
			}
			break;
		case SeaShow::FxKind::GunStop:
			// (a ship's light gun ceasing fire, kept while a run skips too, so no stream is left firing at nothing)
			GunStopped(E.Key);
			break;
		default:
			break;
		}
		if (E.Kind == SeaShow::FxKind::Dispatch || ((E.Kind == SeaShow::FxKind::Sink || E.Kind == SeaShow::FxKind::LeaveLine) && !E.Text.empty()))
		{
			const FString Words = Wide(E.Text);
			if (TPair<FString, int32>* Same = Said.FindByPredicate([&Words](const TPair<FString, int32>& Told) { return Told.Key == Words; }))
			{
				++Same->Value;
			}
			else
			{
				Said.Add(TPair<FString, int32>(Words, 1));
			}
		}
		if (bSkip)
		{
			continue;
		}
		switch (E.Kind)
		{
		case SeaShow::FxKind::Salvo:
		{
			// Her turrets' flashes along her deck, laid toward the target and up; her guns heard after the flash.
			if (const SeaShow::ShipOut* From = S.Show.Ship(E.Key); From && Kit)
			{
				const FVector2D Bow = Ahead(From->Heading);
				const FVector2D ToAim = (FVector2D(E.Aim.X, E.Aim.Y) - FVector2D(From->At.X, From->At.Y)).GetSafeNormal();
				const FVector Facing = (WorldVector(ToAim.X, ToAim.Y, 0.0).GetSafeNormal() + FVector(0.0, 0.0, 0.3)).GetSafeNormal();
				const double Long = DrawnLong.Contains(E.Key) ? DrawnLong[E.Key] : From->Long;
				const TCHAR* Gun = E.Size >= 180.0 ? TEXT("howitzer") : TEXT("tank_gun");
				TArray<double> Turrets;
				if (E.What == "bb")
				{
					Turrets = { 0.30, 0.17, -0.28 };
				}
				else if (E.What == "dd")
				{
					Turrets = { 0.30, -0.22 };
				}
				else
				{
					Turrets = { 0.27, -0.26 };
				}
				for (const double Along : Turrets)
				{
					const FVector2D Muzzle = FVector2D(From->At.X, From->At.Y) + Bow * (Along * Long) + ToAim * (From->Beam * 0.45);
					Kit->Muzzle(World(Muzzle.X, Muzzle.Y, FMath::Max(From->Beam * 0.35, 4.0)), Facing, Gun, TEXT("water"));
				}
			}
			Sound(GunsOf(E.What), At, TEXT("big_guns"));
			break;
		}
		case SeaShow::FxKind::Splash:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, SplashRow(E.What), TEXT("water"));
			}
			Sound(TEXT("shell_splash"), OnSea, TEXT("explosions"), 0.8f);
			break;
		case SeaShow::FxKind::ShellHit:
		case SeaShow::FxKind::BombHit:
			// (laid on her above, with what lasts)
			Sound(TEXT("shell_impact_near"), At, TEXT("explosions"));
			break;
		case SeaShow::FxKind::NearMiss:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, E.What == "bomb250" ? TEXT("bomb250") : TEXT("how105"), TEXT("water"));
			}
			Sound(TEXT("shell_splash"), OnSea, TEXT("explosions"));
			break;
		case SeaShow::FxKind::ScreenStart:
			if (!bScreensSaid)
			{
				bScreensSaid = true;
				UE_LOG(LogTheater, Display, TEXT("Sea battle: a smoke screen is laid (the effects kit has no smoke screen yet: not drawn)."));
			}
			break;
		case SeaShow::FxKind::Flak:
		{
			FFlakLater& Burst = FlakLater.AddDefaulted_GetRef();
			Burst.At = Now + E.Size;
			Burst.Where = At;
			Burst.Kind = Wide(E.What);
			break;
		}
		case SeaShow::FxKind::PlaneDown:
			if (Kit)
			{
				Kit->AirFall(Now, FallIds + E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), TEXT("burn"), 13.0f, 0.0);
				Falling.Add(E.Key, At);
			}
			break;
		case SeaShow::FxKind::Ditch:
			if (Kit)
			{
				Kit->ShellOfSize(OnSea, TEXT("how105"), TEXT("water"));
			}
			break;
		case SeaShow::FxKind::Sound:
			Sound(FName(*Wide(E.What)), At, TEXT("ships"));
			break;
		case SeaShow::FxKind::Gun:
			// A ship's light guns at the aircraft (Battle/ShipGuns, the historian's ship-air-defence.md B1/B2): each mount's
			// stream of tracer from her deck, laid each step while it fires, in her nation's colour.
			if (Kit)
			{
				AirGun(Kit, Now, E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), Wide(E.What), static_cast<float>(E.Rate),
				       static_cast<float>(E.Mv * UnitsPerMetre), KitNation(E.Text));
			}
			GunFired(E.Key, ShipGuns::KindSlot(E.What), At);
			break;
		case SeaShow::FxKind::ShipGun:
		{
			// A heavy gun's round at the aircraft (its burst, or its splash ahead of a torpedo plane, follows when it gets
			// there): the flash and smoke along the bore at her deck (muzzle.ship_aa; a main battery's, muzzle.ship_main),
			// and the boom (the mix's flak firing take, family big_guns) after its flash, no closer together than the mix's
			// soundEvery.
			if (Kit)
			{
				const FVector Bore = (World(E.Aim.X, E.Aim.Y, E.Aim.Z) - At).GetSafeNormal();
				Kit->Muzzle(At, Bore.IsNearlyZero() ? FVector::UpVector : Bore, E.Size >= 200.0 ? TEXT("ship_main") : TEXT("ship_aa"), TEXT("water"));
			}
			if (RealT - ShipGunHeardAt >= MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6))
			{
				ShipGunHeardAt = RealT;
				const FString Boom = MixTake(Mix, TEXT("air.flak.fire"), 0);
				const FString Family = MixTake(Mix, TEXT("air.flak.fireFamily"));
				Sound(FName(Boom.IsEmpty() ? TEXT("flak_heavy_fire_01") : *Boom), At, FName(Family.IsEmpty() ? TEXT("big_guns") : *Family));
			}
			break;
		}
		case SeaShow::FxKind::PlaneHit:
			// (rounds striking an aircraft: the kit's sparks and pieces, as the air scene's)
			if (Kit)
			{
				Kit->AirHit(Now, At, WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), Wide(E.What));
			}
			break;
		default:
			break;
		}
	}
	for (const TPair<FString, int32>& Told : Said)
	{
		if (Told.Value > 1)
		{
			UE_LOG(LogTheater, Display, TEXT("Sea battle %.0f s: %s (%d times)"), Now, *Told.Key, Told.Value);
		}
		else
		{
			UE_LOG(LogTheater, Display, TEXT("Sea battle %.0f s: %s"), Now, *Told.Key);
		}
	}
	S.Show.Effects.clear();
}

// ---------------------------------------------------------------------------------------------------------------
// Drawing: the strikes' aircraft, the wakes and tracks on the sea, the trails of smoke and fire.

void ASeaBattle::DrawPlanes()
{
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	FBattleEffects* Kit = Effects.Get();
	// (the models are built nose toward -X with the right hand at -Y: each is turned half round before its heading,
	// climb and bank, as the air battle's)
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	TSet<int32> Seen;
	for (const SeaShow::PlaneOut& P : S.Show.Planes())
	{
		if (!P.bShown)
		{
			continue;
		}
		Seen.Add(P.Key);
		UProceduralMeshComponent* Mesh = Planes.FindRef(P.Key);
		if (!Mesh)
		{
			const FString Name = SeaParts::ModelName(SeaParts::Wide(P.Model), SeaParts::Wide(P.Nation), P.Role == "cover");
			const int32* Index = ShapeOf.Find(Name);
			if (!Index)
			{
				FMiniatureMesh Made;
				Made.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0, false);
				FPlaneShape& Shape = Shapes.AddDefaulted_GetRef();
				if (Made.Vertices.Num() > 0)
				{
					const FVector Middle = FBox(Made.Vertices).GetCenter();
					Shape.Points.Reserve(Made.Vertices.Num());
					for (const FVector& Point : Made.Vertices)
					{
						Shape.Points.Add(Point - Middle);
					}
					Shape.Triangles = Made.Triangles;
					Shape.Normals = Made.Normals;
					Shape.Colors = Made.Colors;
				}
				Index = &ShapeOf.Add(Name, Shapes.Num() - 1);
			}
			const FPlaneShape& Shape = Shapes[*Index];
			Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("SeaPlane_%d"), P.Key)));
			Mesh->SetupAttachment(Root);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(true);
			Mesh->bUseAsyncCooking = false;
			Mesh->RegisterComponent();
			if (Shape.Points.Num() > 0)
			{
				Mesh->CreateMeshSection(0, Shape.Points, Shape.Triangles, Shape.Normals, TArray<FVector2D>(), Shape.Colors, TArray<FProcMeshTangent>(), false);
				Mesh->SetMaterial(0, PlaneInk);
			}
			Planes.Add(P.Key, Mesh);
		}
		const FVector Where = World(P.At.X, P.At.Y, P.At.Z);
		Mesh->SetWorldLocationAndRotation(Where, FRotator(P.Pitch, P.Heading, P.Bank).Quaternion() * HalfRound);
		if (P.bFalling && Kit && Falling.Contains(P.Key))
		{
			const FVector2D Way = SeaParts::Ahead(P.Heading);
			const double Climb = FMath::DegreesToRadians(P.Pitch);
			const FVector Velocity = WorldVector(Way.X * P.Speed * FMath::Cos(Climb), Way.Y * P.Speed * FMath::Cos(Climb), P.Speed * FMath::Sin(Climb));
			Kit->AirFallMove(Now, SeaParts::FallIds + P.Key, Where, Velocity);
			Falling.Add(P.Key, Where);
		}
	}
	for (auto It = Planes.CreateIterator(); It; ++It)
	{
		if (Seen.Contains(It.Key()))
		{
			continue;
		}
		if (It.Value())
		{
			It.Value()->DestroyComponent();
		}
		// (a falling aircraft gone: into the sea where it last was)
		if (const FVector* Last = Falling.Find(It.Key()))
		{
			if (Kit)
			{
				Kit->AirFallStop(SeaParts::FallIds + It.Key());
				Kit->ShellOfSize(FVector(Last->X, Last->Y, 0.0), TEXT("how105"), TEXT("water"));
			}
			Falling.Remove(It.Key());
		}
		It.RemoveCurrent();
	}
}

void ASeaBattle::LayWakes()
{
	// Each ship's wake: a point at her stern every 15 m she makes (or 2 s), laid every step, a picture run's skips too, so
	// the wake behind her after a skip is the one she made.
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		if (Ship.Doing == SeaShow::Fate::Sinking && Ship.Down >= 0.4)
		{
			continue;
		}
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const FVector2D Stern = FVector2D(Ship.At.X, Ship.At.Y) - SeaParts::Ahead(Ship.Heading) * (Long * 0.48);
		TArray<FWakePoint>& Path = Wakes.FindOrAdd(Ship.Key);
		if (Path.Num() == 0 || FVector2D::Distance(Path.Last().P, Stern) >= 15.0 || Now - Path.Last().T >= 2.0)
		{
			const double Along = Path.Num() == 0 ? 0.0 : Path.Last().S + FVector2D::Distance(Path.Last().P, Stern);
			FWakePoint& Made = Path.AddDefaulted_GetRef();
			Made.P = Stern;
			Made.T = Now;
			Made.Strength = Ship.Wake;
			Made.Beam = Ship.Beam;
			Made.Long = Long;
			Made.Speed = Ship.Speed;
			Made.S = Along;
		}
	}
}

void ASeaBattle::DrawMarks()
{
	using namespace SeaParts;
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	// The foam, on the wake ink where the build has it (each point's paint: red how white, green how whole, the material
	// breaking up the rest with its noise, blue 1 for the churned band under the foam, alpha how much; its texture place
	// in the ship's beams, along the water and across), else on the marks' ink, whole, as much as is whole.
	const bool bInk = FoamInk != nullptr;
	TArray<FVector> FV, FN;
	TArray<FVector2D> FU;
	TArray<FColor> FC;
	TArray<int32> FT;
	auto Byte = [](double X) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(X * 255.0), 0, 255)); };
	auto Foamy = [&](const FVector2D& P, double Z, const FVector2D& UV, double White, double Whole, double Much, bool bBand)
	{
		FV.Add(World(P.X, P.Y, Z));
		FN.Add(FVector::UpVector);
		FU.Add(UV);
		if (bInk)
		{
			FC.Add(FColor(Byte(White), Byte(Whole), bBand ? 255 : 0, Byte(Much)));
		}
		else
		{
			FColor Paint = SeaParts::Foam;
			Paint.A = bBand ? 0 : Byte(Much * White * (0.35 + 0.65 * Whole));
			FC.Add(Paint);
		}
		return FV.Num() - 1;
	};
	// (a quad A B C D round its edge, the way round that faces up, as FMiniatureMesh::Face lays them; a sheet standing
	// on the water both ways round where the ink shows one side only)
	auto Quad = [&](TArray<int32>& Into, const TArray<FVector>& At, int32 A, int32 B, int32 C, int32 D, bool bStanding)
	{
		const FVector Facing = (At[B] - At[A]) ^ (At[D] - At[A]);
		if (bStanding && !bInk)
		{
			Into.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
		}
		else if (Facing.Z <= 0.0)
		{
			Into.Append({ A, B, C, A, C, D });
		}
		else
		{
			Into.Append({ A, C, B, A, D, C });
		}
	};
	// A ribbon along a line of stations: each its middle, half width, place along the water (U, in beams) and five
	// columns across (Across, shares of the half width), each column with its height, wholeness and amount.
	struct FStation
	{
		FVector2D Mid = FVector2D::ZeroVector;
		double Half = 1.0, U = 0.0, Beam = 10.0, White = 1.0;
		double Z[5] = { 0.3, 0.3, 0.3, 0.3, 0.3 };
		double Whole[5] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
		double Much[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
	};
	auto Ribbon = [&](const TArray<FStation>& Line, const double (&Across)[5], bool bBand)
	{
		const int32 Count = Line.Num();
		if (Count < 2)
		{
			return;
		}
		const int32 First = FV.Num();
		for (int32 K = 0; K < Count; ++K)
		{
			const FStation& St = Line[K];
			const FVector2D Way = (Line[FMath::Min(K + 1, Count - 1)].Mid - Line[FMath::Max(K - 1, 0)].Mid).GetSafeNormal();
			const FVector2D Side(Way.Y, -Way.X);
			for (int32 C = 0; C < 5; ++C)
			{
				const double Off = St.Half * Across[C];
				Foamy(St.Mid + Side * Off, St.Z[C], FVector2D(St.U, Off / FMath::Max(St.Beam, 1.0)), St.White, St.Whole[C], St.Much[C], bBand);
			}
		}
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			for (int32 C = 0; C < 4; ++C)
			{
				const int32 A = First + K * 5 + C, B = A + 5;
				Quad(FT, FV, A, A + 1, B + 1, B, false);
			}
		}
	};
	static const double WakeAcross[5] = { -1.0, -0.8, 0.0, 0.8, 1.0 };
	static const double BandAcross[5] = { -1.25, -0.7, 0.0, 0.7, 1.25 };
	static const double ArmAcross[5] = { -1.0, -0.55, 0.0, 0.55, 1.0 };

	// The wakes (the campaign map's passed recipe, at the sea's own size; the art director on Samar r2e): from each
	// ship's stern a band of churned water and on it the foam, whitest and whole by the stern, widening to two beams,
	// breaking up and fading to nothing by WakeRun astern, never a crisp line of one width, never brighter far out than
	// at her stern. A point's place astern is how long ago she laid it times her speed then, so a ship that slows or
	// stops leaves her old wake to age on, and a stopped ship makes none.
	TSet<int32> Afloat;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		Afloat.Add(Ship.Key);
	}
	TArray<TArray<FStation>> Bands, Wakes2;
	for (auto It = Wakes.CreateIterator(); It; ++It)
	{
		TArray<FWakePoint>& Path = It.Value();
		int32 Old = 0;
		while (Old < Path.Num() && Now - Path[Old].T > WakeLife)
		{
			++Old;
		}
		if (Old > 0)
		{
			Path.RemoveAt(0, Old);
		}
		if (Path.Num() == 0)
		{
			if (!Afloat.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
			continue;
		}
		TArray<FWakePoint> Line = Path;
		if (const SeaShow::ShipOut* Ship = S.Show.Ship(It.Key()); Ship && (Ship->Doing != SeaShow::Fate::Sinking || Ship->Down < 0.4))
		{
			const double Long = DrawnLong.Contains(It.Key()) ? DrawnLong[It.Key()] : Ship->Long;
			const FVector2D Stern = FVector2D(Ship->At.X, Ship->At.Y) - Ahead(Ship->Heading) * (Long * 0.48);
			if (FVector2D::Distance(Line.Last().P, Stern) > 1.0)
			{
				FWakePoint Here = Line.Last();
				Here.S += FVector2D::Distance(Line.Last().P, Stern);
				Here.P = Stern;
				Here.T = Now;
				Here.Strength = Ship->Wake;
				Here.Speed = Ship->Speed;
				Line.Add(Here);
			}
		}
		// (newest first: from her stern astern; a wake broken where time jumped, as a run started part way through)
		TArray<FStation> Band, Top;
		auto Flush = [&]()
		{
			if (Band.Num() >= 2)
			{
				Bands.Add(Band);
			}
			if (Top.Num() >= 2)
			{
				Wakes2.Add(Top);
			}
			Band.Reset();
			Top.Reset();
		};
		bool bFoamDone = false, bBandDone = false;
		for (int32 K = Line.Num() - 1; K >= 0; --K)
		{
			const FWakePoint& W = Line[K];
			if (K < Line.Num() - 1 && Line[K + 1].T - W.T > 6.0)
			{
				Flush();
				bFoamDone = bBandDone = false;
			}
			const double Long = FMath::Max(W.Long, 20.0);
			const double Run = WakeRun(Long) / Long;
			const double Lengths = FMath::Max(0.0, Now - W.T) * FMath::Max(W.Speed, 1.5) / Long;
			// (how much wake she made there: none stopped, all of it from about three quarters of her flank speed)
			const double Made = FMath::Clamp(W.Strength * 1.4, 0.0, 1.0) * WakeStrength;
			const double Ragged = 1.0 + 0.18 * (Hash01(W.S * 0.37 + It.Key()) - 0.5) * FMath::Min(1.0, Lengths * 2.0);
			if (!bFoamDone)
			{
				const double Far = FMath::Min(Lengths / Run, 1.0);
				const double Whole = WakeWhole(Lengths, Run);
				const double Much = Made * (1.0 - 0.55 * Far) * FMath::Pow(FMath::Clamp((1.0 - Far) / 0.25, 0.0, 1.0), 1.5);
				FStation St;
				St.Mid = W.P;
				St.Beam = W.Beam;
				St.U = -W.S / FMath::Max(W.Beam, 1.0);
				St.Half = WakeHalf(W.Beam, Lengths, Run) * Ragged;
				St.White = 1.0 - 0.3 * Far;
				const double Wholes[5] = { Whole * 0.6, Whole * 0.94, Whole, Whole * 0.94, Whole * 0.6 };
				const double Muches[5] = { 0.0, Much, Much, Much, 0.0 };
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = Wholes[C];
					St.Much[C] = Muches[C];
				}
				Top.Add(St);
				bFoamDone = Far >= 1.0;
			}
			if (bInk && !bBandDone)
			{
				// (the churned band under the foam: the sea's own colour lifted, strongest a little astern of her, half a
				// length longer than the foam, thinning to nothing; a ship that follows sits in it, faded and broken)
				const double Far = FMath::Min(Lengths / (Run + 0.5), 1.0);
				const double Much = Made * 0.4 * FMath::Min(1.0, Far * 10.0 + 0.5) * FMath::Pow(1.0 - Far, 0.7);
				FStation St;
				St.Mid = W.P;
				St.Beam = W.Beam;
				St.U = -W.S / FMath::Max(W.Beam, 1.0);
				St.Half = WakeHalf(W.Beam, FMath::Min(Lengths, Run), Run) * Ragged;
				for (int32 C = 0; C < 5; ++C)
				{
					St.Z[C] = 0.2;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Band.Add(St);
				bBandDone = Far >= 1.0;
			}
			if (bFoamDone && (bBandDone || !bInk))
			{
				break;
			}
		}
		Flush();
	}
	// (the band under all the foam: laid first, so the foam is drawn over it)
	for (const TArray<FStation>& Line : Bands)
	{
		Ribbon(Line, BandAcross, true);
	}
	for (const TArray<FStation>& Line : Wakes2)
	{
		Ribbon(Line, WakeAcross, false);
	}

	// Each ship under way: her bow wave climbing her stem, the white water along her sides, the faint arms of her
	// wave, and the boil at her stern (the art director on Samar r2e, frame 01).
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const bool bMaking = Ship.Doing != SeaShow::Fate::Sinking || Ship.Down < 0.4;
		if (!bMaking || !Ship.bShown || Ship.Wake <= 0.12)
		{
			continue;
		}
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const double Beam = FMath::Max(Ship.Beam, 3.0);
		const double Strength = FMath::Clamp(Ship.Wake, 0.0, 1.0);
		const double Made = FMath::Clamp(Strength * 1.2, 0.0, 1.0) * WakeStrength;
		const FVector2D At(Ship.At.X, Ship.At.Y), Bow = Ahead(Ship.Heading), Right = Starboard(Ship.Heading);
		const FVector2D Stem = At + Bow * (Long * 0.5), Stern = At - Bow * (Long * 0.48);
		// (her half width X metres aft of her stem, as her hull narrows to it over the first quarter of her length)
		auto HullHalf = [Long, Beam](double X) { return 0.5 * Beam * FMath::Pow(FMath::Min(1.0, FMath::Max(X, 0.0) / (0.25 * Long)), 0.6); };
		// The bow wave: a sheet of white water standing against each side of her bow, highest just aft of the stem (a
		// destroyer's at flank speed about 3.5 m), curling out as it falls away aft over a third of her length.
		const double Rise = (0.4 + 3.0 * Strength * Strength) * FMath::Clamp(FMath::Sqrt(Long / 110.0), 0.8, 1.5);
		for (const double Hand : { -1.0, 1.0 })
		{
			const int32 Steps = 8;
			const int32 First = FV.Num();
			for (int32 K = 0; K <= Steps; ++K)
			{
				const double Share = static_cast<double>(K) / Steps;
				const double X = Share * 0.3 * Long, Ls = X / Long;
				const double Crest = Rise * (Ls < 0.06 ? FMath::Lerp(0.75, 1.0, Ls / 0.06) : FMath::Pow(FMath::Max(0.0, 1.0 - (Ls - 0.06) / 0.24), 1.3));
				const double Aft = 1.0 - Share * Share;
				const double Off = HullHalf(X) + 0.35 + 0.04 * Beam;
				const FVector2D Foot = Stem - Bow * X + Right * (Hand * Off);
				const double U = X / Beam;
				Foamy(Foot, 0.25, FVector2D(U, 0.0), 1.0, 1.0, 0.9 * Made * Aft, false);
				Foamy(Foot + Right * (Hand * 0.12 * Crest), 0.25 + 0.55 * Crest, FVector2D(U, 1.6 * Crest / Beam), 1.0, 0.85, 0.7 * Made * Aft, false);
				Foamy(Foot + Right * (Hand * 0.35 * Crest), 0.25 + Crest, FVector2D(U, 3.0 * Crest / Beam), 1.0, 0.6, 0.0, false);
			}
			for (int32 K = 0; K < Steps; ++K)
			{
				const int32 A = First + K * 3, B = A + 3;
				Quad(FT, FV, A, A + 1, B + 1, B, true);
				Quad(FT, FV, A + 1, A + 2, B + 2, B + 1, true);
			}
			// The white water along her side, from the bow wave's foot aft, thinning out by two thirds of her length.
			TArray<FStation> Side;
			for (int32 K = 0; K <= 6; ++K)
			{
				const double Share = static_cast<double>(K) / 6.0;
				const double X = Share * 0.7 * Long;
				const double Wide = 1.5 + 2.5 * Strength;
				FStation St;
				St.Mid = Stem - Bow * X + Right * (Hand * (HullHalf(X) + 0.3 + Wide * 0.5));
				St.Half = Wide * 0.5;
				St.Beam = Beam;
				St.U = X / Beam;
				const double Much = 0.55 * Made * (1.0 - Share);
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = C == 2 ? 0.75 : 0.55;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Side.Add(St);
			}
			Ribbon(Side, ArmAcross, false);
			// The arm of her wave, faint: from her stem, close along her side and then away at about 20 degrees.
			TArray<FStation> Arm;
			const FVector2D Points[3] = {
				Stem,
				Stem - Bow * (Long * 0.45) + Right * (Hand * Beam * 0.62),
				Stem - Bow * (Long * 1.15) + Right * (Hand * (Beam * 0.5 + Long * 0.3)),
			};
			const double Halves[3] = { 1.5, 3.5, 7.0 }, Muches[3] = { 0.35, 0.25, 0.0 };
			for (int32 K = 0; K < 3; ++K)
			{
				FStation St;
				St.Mid = Points[K];
				St.Half = Halves[K];
				St.Beam = Beam;
				St.U = K * 0.5 * Long / Beam;
				for (int32 C = 0; C < 5; ++C)
				{
					St.Whole[C] = 0.5;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Muches[K] * Made;
				}
				Arm.Add(St);
			}
			Ribbon(Arm, ArmAcross, false);
		}
		// The boil at her stern: her screws' white water heaped up a metre or two just astern of her, highest a little
		// aft of her stern and settling over two fifths of her length into the wake behind.
		{
			const double Heap = FMath::Min(0.5 + 1.8 * Strength * Strength, 2.6);
			const double Run = WakeRun(Long) / Long;
			const double Along0 = Wakes.Contains(Ship.Key) && Wakes[Ship.Key].Num() > 0 ? Wakes[Ship.Key].Last().S : 0.0;
			TArray<FStation> Boil;
			for (int32 K = 0; K <= 6; ++K)
			{
				const double Ls = 0.4 * K / 6.0;
				const double Bump = FMath::Max(0.0, FMath::Sin(UE_DOUBLE_PI * (Ls + 0.06) / 0.46));
				FStation St;
				St.Mid = Stern - Bow * (Ls * Long);
				St.Half = WakeHalf(Beam, Ls, Run) * 1.1;
				St.Beam = Beam;
				St.U = -(Along0 - Ls * Long) / Beam;
				const double Much = 0.95 * Made * FMath::Pow(1.0 - Ls / 0.4, 0.6);
				for (int32 C = 0; C < 5; ++C)
				{
					const double Across = ArmAcross[C];
					St.Z[C] = 0.35 + Heap * Bump * (1.0 - 0.85 * Across * Across);
					St.Whole[C] = C == 2 ? 1.0 : 0.85;
					St.Much[C] = C == 0 || C == 4 ? 0.0 : Much;
				}
				Boil.Add(St);
			}
			Ribbon(Boil, ArmAcross, false);
		}
	}

	// The torpedo tracks (the art director's ruling of 19 November, Samar r2e frame 07): no line from where each was
	// fired, but a streak of bubbles TrackStreak metres behind its (unseen) torpedo, brightest just behind the head,
	// widening three times and breaking up to nothing at its tail; an oxygen torpedo's a third as strong. A track whose
	// torpedo has struck or run out goes in TrackFade seconds.
	for (auto It = Tracks.CreateIterator(); It; ++It)
	{
		const FTrackLine& Line = It.Value();
		const double Left = Line.StopT < 0.0 ? 1.0 : 1.0 - (Now - Line.StopT) / TrackFade;
		if (Left <= 0.0)
		{
			It.RemoveCurrent();
			continue;
		}
		const double Run = FVector2D::Distance(Line.From, Line.Head);
		if (Run < 5.0)
		{
			continue;
		}
		const FVector2D Way = (Line.Head - Line.From) / Run;
		const double Streak = FMath::Min(TrackStreak, Run);
		const double Strength = 0.75 * Left * WakeStrength * (Line.bOxygen ? 1.0 / 3.0 : 1.0);
		const double Unit = 6.0; // (its "beam" for the ink's pattern: about a torpedo's bubbles across)
		TArray<FStation> Streak3;
		const int32 Steps = 12;
		for (int32 K = 0; K <= Steps; ++K)
		{
			const double Back = Streak * K / Steps; // (metres behind its head)
			const double Share = Back / Streak;
			const double Peak = Back < 8.0 ? FMath::Lerp(0.25, 1.0, Back / 8.0) : FMath::Pow(FMath::Max(0.0, 1.0 - (Back - 8.0) / (Streak - 8.0)), 1.4);
			FStation St;
			St.Mid = Line.Head - Way * Back;
			St.Half = 1.2 + 2.4 * Share;
			St.Beam = Unit;
			St.U = -(Run - Back) / Unit;
			const double Whole = FMath::Lerp(0.95, 0.25, Share);
			for (int32 C = 0; C < 5; ++C)
			{
				St.Whole[C] = C == 2 ? Whole : Whole * 0.6;
				St.Much[C] = C == 0 || C == 4 ? 0.0 : Strength * Peak;
			}
			Streak3.Add(St);
		}
		Ribbon(Streak3, WakeAcross, false);
	}

	// A ship broken in two (the art director, Samar r2e frame 08): the gap hidden in a boil of white water, heaviest in
	// the middle, a lighter ring where each half meets the sea, both going as she goes under; and her oil, a dark sheen
	// with a ragged, feathered edge spreading over the next minute, drawn out down the wind.
	TArray<FVector> DV, DN;
	TArray<FColor> DC;
	TArray<int32> DT;
	const SeaShow::Vec Blow = S.Show.Wind();
	const FVector2D Downwind = FVector2D(Blow.X, Blow.Y).GetSafeNormal();
	for (const TPair<int32, FBreakUp>& Pair : Breaks)
	{
		const FBreakUp& B = Pair.Value;
		const double Age = Now - B.T;
		if (Age < 0.0 || Age > 150.0)
		{
			continue;
		}
		const FVector2D Bow = Ahead(B.Heading), Right = Starboard(B.Heading);
		const double Going = 1.0 - FMath::Clamp((Age - 25.0) / 35.0, 0.0, 1.0);
		const int32 Round = 16;
		// (an ellipse round Middle, A along her and Bb across, in rings: each ring's share of the size and its paint)
		auto Disc = [&](const FVector2D& Middle, double A, double Bb, const TArray<double>& Rings, const TArray<double>& Muches, const TArray<double>& Wholes, double Heave, int32 Seed)
		{
			const int32 First = FV.Num();
			for (int32 Ring = 0; Ring < Rings.Num(); ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const double Angle = 2.0 * UE_DOUBLE_PI * K / Round;
					const double Rag = Ring == Rings.Num() - 1 ? 0.8 + 0.4 * Hash01(Seed * 7.0 + K) : 1.0;
					const FVector2D P = Middle + Bow * (FMath::Cos(Angle) * A * Rings[Ring] * Rag) + Right * (FMath::Sin(Angle) * Bb * Rings[Ring] * Rag);
					Foamy(P, 0.3 + Heave * (1.0 - Rings[Ring]), FVector2D(FMath::Cos(Angle) * A * Rings[Ring] / B.Beam, FMath::Sin(Angle) * Bb * Rings[Ring] / B.Beam),
						1.0, Wholes[Ring], Muches[Ring], false);
				}
			}
			for (int32 Ring = 0; Ring + 1 < Rings.Num(); ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const int32 A0 = First + Ring * Round + K, A1 = First + Ring * Round + (K + 1) % Round;
					Quad(FT, FV, A0, A1, A1 + Round, A0 + Round, false);
				}
			}
		};
		// The boil over the gap: a beam and a half across at once, three by twenty seconds, heaving a metre at first.
		const double Wide = B.Beam * (1.5 + 1.5 * FMath::Clamp(Age / 20.0, 0.0, 1.0));
		const double Boil = 0.95 * Going * FMath::Min(1.0, Age / 1.5 + 0.3);
		Disc(B.At, Wide * 1.2, Wide, { 0.05, 0.5, 1.0 }, { Boil, Boil * 0.85, 0.0 }, { 1.0, 0.8, 0.45 }, 1.0 * FMath::Max(0.0, 1.0 - Age / 30.0), Pair.Key);
		// The ring where each half meets the sea, about a quarter of her length either side of the break.
		for (const double End : { -1.0, 1.0 })
		{
			const FVector2D Middle = B.At + Bow * (End * B.Long * 0.27);
			const double Ring = 0.6 * Going;
			Disc(Middle, B.Long * 0.25, B.Beam * 0.9, { 0.75, 0.95, 1.15 }, { 0.0, Ring, 0.0 }, { 0.7, 0.6, 0.4 }, 0.0, Pair.Key * 3 + (End > 0.0 ? 1 : 2));
		}
		// Her oil: dark on the water, a little see-through, a ragged edge feathered over its outer third, spreading to
		// some five beams across over a minute, drawn out half again down the wind, and fading over the last minute.
		{
			const double Spread = B.Beam * (1.0 + 4.0 * FMath::Clamp(Age / 60.0, 0.0, 1.0)) * 0.5;
			const double Dark = 0.42 * FMath::Min(1.0, Age / 4.0) * FMath::Clamp((150.0 - Age) / 60.0, 0.0, 1.0);
			const FVector2D Middle = B.At + Downwind * (Spread * 0.4);
			const FVector2D Along = Downwind.IsNearlyZero() ? Bow : Downwind, Across(Along.Y, -Along.X);
			const double Rings[3] = { 0.0, 0.68, 1.0 }, Alphas[3] = { Dark, Dark, 0.0 };
			const int32 First = DV.Num();
			for (int32 Ring = 0; Ring < 3; ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const double Angle = 2.0 * UE_DOUBLE_PI * K / Round;
					const double Rag = 0.75 + 0.5 * Hash01(Pair.Key * 13.0 + K * (Ring + 1));
					const double Down = FMath::Cos(Angle) * (FMath::Cos(Angle) > 0.0 ? 1.5 : 1.0);
					const FVector2D P = Middle + Along * (Down * Spread * Rings[Ring] * Rag) + Across * (FMath::Sin(Angle) * Spread * Rings[Ring] * Rag);
					DV.Add(World(P.X, P.Y, 0.25));
					DN.Add(FVector::UpVector);
					DC.Add(FColor(10, 12, 14, Byte(Alphas[Ring])));
				}
			}
			for (int32 Ring = 0; Ring < 2; ++Ring)
			{
				for (int32 K = 0; K < Round; ++K)
				{
					const int32 A0 = First + Ring * Round + K, A1 = First + Ring * Round + (K + 1) % Round;
					Quad(DT, DV, A0, A1, A1 + Round, A0 + Round, false);
				}
			}
		}
	}

	if (Foam && bInk)
	{
		if (FV.Num() == 0)
		{
			Foam->ClearMeshSection(0);
		}
		else
		{
			Foam->CreateMeshSection(0, FV, FT, FN, FU, FC, TArray<FProcMeshTangent>(), false);
			Foam->SetMaterial(0, FoamInk);
		}
	}
	if (!Marks)
	{
		return;
	}
	// (the marks' mesh: the oil, and the foam too where there is no wake ink, the oil drawn first, under it)
	if (!bInk)
	{
		const int32 Base = DV.Num();
		DV.Append(FV);
		DN.Append(FN);
		DC.Append(FC);
		for (const int32 K : FT)
		{
			DT.Add(K + Base);
		}
	}
	if (DV.Num() == 0)
	{
		Marks->ClearMeshSection(0);
		return;
	}
	Marks->CreateMeshSection(0, DV, DT, DN, TArray<FVector2D>(), DC, TArray<FProcMeshTangent>(), false);
	if (MarkInk)
	{
		Marks->SetMaterial(0, MarkInk);
	}
}

void ASeaBattle::RunTrails()
{
	FSeaBattleState& S = *State;
	FBattleEffects* Kit = Effects.Get();
	if (!Kit)
	{
		return;
	}
	const double Now = S.BattleT;
	// Every few seconds, the ships whose funnels smoke: those afloat and under her own steam nearest the eye (a burning
	// ship's own smoke stands for it). A plume a funnel on the nearest, one a ship on the next (2 and 1); a ship keeps
	// what she has while she stays a little inside the count, so plumes are not stopped and started as ships pass.
	// (chosen again at once when the camera cuts, and the ships the shot is of first of all: the hero destroyer of the
	// funnels picture went without, Samar r2e frame 01)
	if (bFunnels && (Now - FunnelsChosenAt >= 4.0 || ShotNow != FunnelsShot))
	{
		FunnelsChosenAt = Now;
		FunnelsShot = ShotNow;
		int32 Hero = -1, Second = -1;
		if (Shots.IsValidIndex(ShotNow))
		{
			Hero = Shots[ShotNow].Subject;
			Second = Shots[ShotNow].Kind == ESeaShot::Funnels ? Shots[ShotNow].Other : -1;
		}
		TArray<TPair<double, int32>> ByNear;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (!Ship.bShown || Ship.bBurning || (Ship.Doing != SeaShow::Fate::Line && Ship.Doing != SeaShow::Fate::Limping))
			{
				continue;
			}
			const double Away = Ship.Key == Hero || Ship.Key == Second ? 0.0 : FVector::Dist(World(Ship.At.X, Ship.At.Y, 0.0), EyeAt) / UnitsPerMetre;
			if (Away < SeaParts::FunnelReach)
			{
				ByNear.Add(TPair<double, int32>(Away, Ship.Key));
			}
		}
		ByNear.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
		const TMap<int32, uint8> Had = MoveTemp(FunnelsChosen);
		FunnelsChosen.Reset();
		for (int32 K = 0; K < ByNear.Num(); ++K)
		{
			const uint8* Was = Had.Find(ByNear[K].Value);
			if (K < SeaParts::NearFunnels || (Was && *Was == 2 && K < SeaParts::NearFunnels + 1))
			{
				FunnelsChosen.Add(ByNear[K].Value, 2);
			}
			else if (K < SeaParts::FunnelShips || (Was && K < SeaParts::FunnelShips + 2))
			{
				FunnelsChosen.Add(ByNear[K].Value, 1);
			}
		}
	}
	TSet<int32> Fires, Funnels;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const FVector2D At(Ship.At.X, Ship.At.Y), Bow = SeaParts::Ahead(Ship.Heading);
		const FVector Velocity = WorldVector(Bow.X * Ship.Speed, Bow.Y * Ship.Speed, 0.0);
		// L4: a ship out of the fight burning as she limps away: flames on her deck, black smoke streaming off them.
		if (Ship.bBurning && Ship.Doing != SeaShow::Fate::Sinking)
		{
			const int32 Id = SeaParts::FireIds + Ship.Key;
			const FVector2D Seat = At + Bow * (Long * 0.08);
			const FVector Spot = World(Seat.X, Seat.Y, FMath::Min(Ship.Beam * 0.4, 8.0));
			if (FireTrails.Contains(Id))
			{
				Kit->TrailMove(Now, Id, Spot, Velocity);
			}
			else
			{
				Kit->TrailStart(Now, Id, Spot, Velocity, TEXT("fire"), static_cast<float>(Ship.Beam));
				FireTrails.Add(Id);
			}
			Fires.Add(Id);
		}
		// Her funnels' smoke (the kit's "funnel" kind): from each funnel's top as the stage finds it on her model, or,
		// a ship farther off, one plume from her after funnel as wide as all of hers; every frame, also when she lies
		// stopped (she smokes on, straight up). Power: her speed over her flank speed, as her wake has it; the kit
		// darkens it toward full power and gives a black surge when it rises fast.
		const uint8* Tier = FunnelsChosen.Find(Ship.Key);
		if (bFunnels && Tier && Ship.bShown && !Ship.bBurning && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping))
		{
			TArray<FVector4> Tops;
			if (!Stage || !Stage->FunnelTops(Ship.Key, Tops))
			{
				// (no funnels found on her model: one, where her looks put a funnel's steam)
				const FVector2D Stack = At + Bow * (Long * 0.03);
				const FVector Spot = World(Stack.X, Stack.Y, Ship.Beam * 0.9);
				Tops.Add(FVector4(Spot.X, Spot.Y, Spot.Z, FMath::Clamp(Ship.Beam * 0.28, 1.0, 14.0)));
			}
			const bool bOne = *Tier == 1 && Tops.Num() > 1;
			if (bOne)
			{
				// (her after funnel: the one farthest astern along her heading, in the world's axes)
				const FVector Ahead3 = WorldVector(Bow.X, Bow.Y, 0.0).GetSafeNormal();
				int32 Aft = 0;
				for (int32 K = 1; K < Tops.Num(); ++K)
				{
					if (FVector::DotProduct(FVector(Tops[K]), Ahead3) < FVector::DotProduct(FVector(Tops[Aft]), Ahead3))
					{
						Aft = K;
					}
				}
				const FVector4 After = Tops[Aft];
				const double Span = After.W * FMath::Sqrt(static_cast<double>(Tops.Num()));
				Tops.Reset();
				Tops.Add(FVector4(After.X, After.Y, After.Z, FMath::Min(Span, 14.0)));
			}
			const float Power = static_cast<float>(FMath::Clamp(Ship.Wake, 0.0, 1.0));
			for (int32 K = 0; K < Tops.Num() && K < 3; ++K)
			{
				const int32 Id = SeaParts::FunnelIds + Ship.Key * 4 + (bOne ? 3 : K);
				const FVector Spot(Tops[K].X, Tops[K].Y, Tops[K].Z);
				if (FunnelTrails.Contains(Id))
				{
					SeaParts::FunnelMove(Kit, Now, Id, Spot, Velocity, Power);
				}
				else
				{
					Kit->TrailStart(Now, Id, Spot, Velocity, TEXT("funnel"), static_cast<float>(Tops[K].W));
					FunnelTrails.Add(Id);
				}
				Funnels.Add(Id);
			}
		}
	}
	for (auto It = FireTrails.CreateIterator(); It; ++It)
	{
		if (!Fires.Contains(*It))
		{
			Kit->TrailStop(*It);
			It.RemoveCurrent();
		}
	}
	for (auto It = FunnelTrails.CreateIterator(); It; ++It)
	{
		if (!Funnels.Contains(*It))
		{
			Kit->TrailStop(*It);
			It.RemoveCurrent();
		}
	}
	// A ship broken in two (the art director, Samar r2e frame 08): white steam puffing from the gap, three to five
	// seconds each, for the first half minute, and black smoke pouring from it until her halves are under (the kit's
	// funnel smoke at its blackest: a surge at full power). Her boil, rings and oil are DrawMarks'.
	if (Stage)
	{
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			FVector Cut = FVector::ZeroVector;
			double When = 0.0;
			if (Ship.Doing != SeaShow::Fate::Sinking || Breaks.Contains(Ship.Key) || !Stage->BreakOf(Ship.Key, Cut, When))
			{
				continue;
			}
			FBreakUp& Made = Breaks.Add(Ship.Key);
			const FVector Board = BoardOf(Cut);
			Made.At = FVector2D(Board.X, Board.Y);
			Made.T = FMath::Min(When, Now);
			Made.Heading = Ship.Heading;
			Made.Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
			Made.Beam = Ship.Beam;
			Made.NextSteam = Made.T;
		}
	}
	for (TPair<int32, FBreakUp>& Pair : Breaks)
	{
		FBreakUp& B = Pair.Value;
		const double Age = Now - B.T;
		const FVector Gap = World(B.At.X, B.At.Y, 1.5);
		const int32 Id = SeaParts::BreakIds + Pair.Key;
		if (Age < 30.0 && Now >= B.NextSteam)
		{
			// (a new puff each time, under its own number: one number called again starts its puff over)
			const double Seconds = 3.0 + 2.0 * SeaParts::Hash01(Pair.Key * 5.0 + B.Puffs);
			SeaParts::SteamPuff(Kit, SeaParts::BreakSteamIds + Pair.Key * 8 + (B.Puffs % 8), Gap, FVector::UpVector, static_cast<float>(B.Beam * 0.45), static_cast<float>(Seconds));
			++B.Puffs;
			B.NextSteam = Now + Seconds * 0.8;
		}
		if (Age < 40.0)
		{
			if (B.bSmoke)
			{
				SeaParts::FunnelMove(Kit, Now, Id, Gap, FVector::ZeroVector, 1.0f);
			}
			else if (bFunnels)
			{
				// (from a fifth of full power, so the rise to full on the next frame reads as the kit's black surge)
				Kit->TrailStart(Now, Id, Gap, FVector::ZeroVector, TEXT("funnel"), static_cast<float>(FMath::Clamp(B.Beam * 0.5, 1.0, 14.0)));
				SeaParts::FunnelMove(Kit, Now, Id, Gap, FVector::ZeroVector, 0.2f);
				B.bSmoke = true;
				BreakTrails.Add(Id);
			}
		}
		else if (B.bSmoke)
		{
			Kit->TrailStop(Id);
			BreakTrails.Remove(Id);
			B.bSmoke = false;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The camera: the action's moments, found by playing it through once, and where the eye stands for each.

void ASeaBattle::PlanShots()
{
	FSeaBattleState& S = *State;
	Shots.Reset();
	// (the same show as the scene's: the same seed, the same views, handed over the same way)
	SeaShow::Show Trial(S.Rec.Seed ? S.Rec.Seed : 239);
	Trial.SetField(S.Rec.Board);
	const double Mid = S.Rec.Board.Size * 0.5;
	Trial.Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
	size_t Given = 0;
	auto TakeNext = [&Trial, &S, &Given]()
	{
		const SeaFeed::Frame& F = S.Rec.Views[Given++];
		Trial.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
	};
	TakeNext();
	FShot Fleets, Line, Broadside, Fall, Struck, Run, Strike, Down, Burning, Funnels;
	// (the fall of shot: the salvos of 180 mm and up lately fired, and the columns they threw about each ship)
	struct FSalvoSeen
	{
		double T = 0.0, Size = 0.0;
		int32 Side = -1;
		FVector2D Aim = FVector2D::ZeroVector;
	};
	struct FColumnSeen
	{
		double T = 0.0;
		int32 On = -1;
	};
	TArray<FSalvoSeen> SalvosSeen;
	TArray<FColumnSeen> ColumnsSeen;
	for (FShot* Each : { &Fleets, &Line, &Broadside, &Fall, &Struck, &Run, &Strike, &Down, &Burning, &Funnels })
	{
		Each->At = -1.0;
	}
	Fleets.Kind = ESeaShot::Fleets;
	Line.Kind = ESeaShot::Line;
	Broadside.Kind = ESeaShot::Broadside;
	Fall.Kind = ESeaShot::Rangefinder;
	Struck.Kind = ESeaShot::Hit;
	Run.Kind = ESeaShot::Tracks;
	Strike.Kind = ESeaShot::Strike;
	Down.Kind = ESeaShot::Sinking;
	Burning.Kind = ESeaShot::Burning;
	Funnels.Kind = ESeaShot::Funnels;
	double Contact = -1.0;
	bool bBigGuns = false;
	auto Rank = [](const std::string& Class) { return Class == "bb" ? 4 : Class == "cv" ? 3 : Class == "ca" ? 2 : 1; };
	// THE WATER'S CHECK SHEET ("-TheaterSeaWaterSheet", in place of the battle's shots; the art director's fleet water
	// targets, 21 November 23:44Z): a minute and a half in, the open sea from 28 m and from 60 m, looking away from the
	// sun and toward it, with the ships 1 to 3 km off; then ten seconds at two pictures a second of the smallest ship and
	// the biggest lying nearest her, side by side, to read their roll and pitch from the masthead.
	static const bool bWaterSheet = FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaWaterSheet"));
	const double SheetT = FMath::Min(S.BeginT + 90.0, FMath::Max(S.BeginT + 5.0, S.EndT - 30.0));
	FShot Ride;
	Ride.Kind = ESeaShot::Water;
	Ride.At = -1.0;
	for (double T = S.BeginT; T <= S.EndT + 1e-6; T += 0.25)
	{
		while (Given < S.Rec.Views.size() && S.Rec.Views[Given - 1].Head.T <= T + 1e-9)
		{
			TakeNext();
		}
		Trial.Advance(T);
		if (Contact < 0.0 && Trial.ContactAt() >= 0.0)
		{
			Contact = Trial.ContactAt();
		}
		if (bWaterSheet && Ride.At < 0.0 && T >= SheetT)
		{
			// (the smallest ship afloat in a line, and the biggest nearest her: a destroyer and a battleship, as near as the
			// battle has them)
			const SeaShow::ShipOut* Small = nullptr;
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				if (Ship.Doing == SeaShow::Fate::Line && (!Small || Ship.Long < Small->Long))
				{
					Small = &Ship;
				}
			}
			const SeaShow::ShipOut* Big = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				const double Away = Small ? FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), FVector2D(Small->At.X, Small->At.Y)) : 0.0;
				if (Small && Ship.Doing == SeaShow::Fate::Line && Ship.Long >= Small->Long + 40.0 && Away - Ship.Long * 10.0 < Best)
				{
					Best = Away - Ship.Long * 10.0;
					Big = &Ship;
				}
			}
			Ride.At = T;
			Ride.Subject = Small ? Small->Key : -1;
			Ride.Other = Big ? Big->Key : -1;
		}
		// The fleets: the attacker's biggest column, by its leader.
		if (Fleets.Subject < 0)
		{
			int32 Best = 0;
			for (const SeaShow::ShipOut& Ship : Trial.Ships())
			{
				if (Ship.Side == 0 && Ship.Slot == 0 && Rank(Ship.Class) > Best)
				{
					Best = Rank(Ship.Class);
					Fleets.Subject = Ship.Key;
					Fleets.Other = Ship.Piece;
				}
			}
		}
		// Their funnels (the art director, 21 November 19:45Z): a big ship and a destroyer of one side both under way at
		// 20 knots or more, within a kilometre of each other, side on; the first, from twenty seconds in.
		if (Funnels.At < 0.0 && T >= S.BeginT + 20.0)
		{
			for (const SeaShow::ShipOut& Big : Trial.Ships())
			{
				if (Funnels.At >= 0.0)
				{
					break;
				}
				if (Big.Doing != SeaShow::Fate::Line || Big.Speed < 10.0 || Rank(Big.Class) < 2)
				{
					continue;
				}
				for (const SeaShow::ShipOut& Small : Trial.Ships())
				{
					if (Small.Doing != SeaShow::Fate::Line || Small.Speed < 10.0 || Small.Class != "dd" || Small.Side != Big.Side ||
					    FVector2D(Small.At.X - Big.At.X, Small.At.Y - Big.At.Y).Size() > 1000.0)
					{
						continue;
					}
					Funnels.At = T;
					Funnels.Subject = Big.Key;
					Funnels.Other = Small.Key;
					break;
				}
			}
		}
		for (const SeaShow::Fx& E : Trial.Effects)
		{
			const FVector2D At(E.At.X, E.At.Y);
			switch (E.Kind)
			{
			case SeaShow::FxKind::Salvo:
				if (E.Size >= 180.0)
				{
					FSalvoSeen& Seen = SalvosSeen.AddDefaulted_GetRef();
					Seen.T = T;
					Seen.Size = E.Size;
					Seen.Side = E.Side;
					Seen.Aim = FVector2D(E.Aim.X, E.Aim.Y);
				}
				// (a battleship's first salvo, else the first of any; its fall of shot is looked for again after it)
				if (Broadside.At < 0.0 || (!bBigGuns && E.What == "bb"))
				{
					Fall.At = -1.0;
					const SeaShow::ShipOut* From = Trial.Ship(E.Key);
					Broadside.At = T;
					Broadside.Subject = E.Key;
					Broadside.Other = From ? From->Aim : -1;
					Broadside.Spot = FVector2D(E.Aim.X, E.Aim.Y);
					bBigGuns = E.What == "bb";
				}
				break;
			case SeaShow::FxKind::Line:
				// (the attacker's battle line as it forms, else the first line formed or turned: the ship plan's R1)
				if (Line.At < 0.0 || (Line.Other != 0 && E.Side == 0))
				{
					Line.At = T;
					Line.Subject = E.Key;
					Line.Other = E.Side;
					Line.Way = FVector2D(E.Vel.X, E.Vel.Y);
				}
				break;
			case SeaShow::FxKind::Splash:
				// The fall of shot (the art director, Samar r2e frame 06): a straddle, three or more columns of 180 mm and up
				// about one ship within three seconds, from salvos the other side fired at her in the last half minute;
				// none, and there is no such picture.
				if (Fall.At < 0.0 && Broadside.At >= 0.0 && T >= Broadside.At + 6.0 && E.Size >= 180.0)
				{
					const SeaShow::ShipOut* About = nullptr;
					double Best = TNumericLimits<double>::Max();
					for (const SeaShow::ShipOut& Ship : Trial.Ships())
					{
						const double Away = FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), At);
						if ((Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping) && Away <= Ship.Long * 0.5 + 300.0 && Away < Best)
						{
							Best = Away;
							About = &Ship;
						}
					}
					bool bAimed = false;
					for (const FSalvoSeen& Seen : SalvosSeen)
					{
						bAimed = bAimed || (About && Seen.Side != About->Side && T - Seen.T <= 30.0 && FVector2D::Distance(Seen.Aim, FVector2D(About->At.X, About->At.Y)) <= 600.0);
					}
					if (About && bAimed)
					{
						FColumnSeen& Seen = ColumnsSeen.AddDefaulted_GetRef();
						Seen.T = T;
						Seen.On = About->Key;
						int32 Count = 0;
						for (const FColumnSeen& Column : ColumnsSeen)
						{
							Count += Column.On == About->Key && T - Column.T <= 3.0 ? 1 : 0;
						}
						if (Count >= 3)
						{
							Fall.At = T;
							Fall.Subject = About->Key;
							Fall.Spot = FVector2D(About->At.X, About->At.Y);
						}
					}
				}
				break;
			case SeaShow::FxKind::ShellHit:
				if (Struck.At < 0.0)
				{
					Struck.At = T;
					Struck.Subject = E.Key;
				}
				break;
			case SeaShow::FxKind::TrackStart:
				if (Run.At < 0.0)
				{
					Run.At = T;
					Run.Spot = At;
					Run.Way = FVector2D(E.Vel.X, E.Vel.Y).GetSafeNormal();
				}
				break;
			case SeaShow::FxKind::BombHit:
			case SeaShow::FxKind::NearMiss:
				if (Strike.At < 0.0)
				{
					Strike.At = T;
					Strike.Subject = E.Kind == SeaShow::FxKind::BombHit ? E.Key : -1;
					Strike.Spot = At;
				}
				break;
			case SeaShow::FxKind::Sink:
				if (Down.At < 0.0)
				{
					Down.At = T;
					Down.Subject = E.Key;
				}
				break;
			case SeaShow::FxKind::LeaveLine:
				Burning.At = T;
				Burning.Subject = E.Key;
				break;
			case SeaShow::FxKind::OilFire:
				if (Burning.Subject < 0)
				{
					Burning.At = T;
					Burning.Spot = At;
				}
				break;
			default:
				break;
			}
		}
		Trial.Effects.clear();
		// (the eye the cloud test gives it: south of the middle of every piece)
		double X = 0.0, Y = 0.0;
		int32 Count = 0;
		for (const auto& Pair : Trial.Pieces())
		{
			X += Pair.second.X;
			Y += Pair.second.Y;
			++Count;
		}
		if (Count > 0)
		{
			Trial.Eye = SeaShow::Vec(X / Count, Y / Count - 3000.0, 900.0);
		}
	}
	// When each is taken: the fleets before the guns meet, a battleship's first broadside, its fall of shot a second after the
	// columns rise, a hit as it bursts, the tracks once they have run, a strike as it dives, a ship well into her end, the
	// whole battle at its height, and its end.
	auto Add = [this](FShot Shot, const TCHAR* Name, double At)
	{
		Shot.Name = Name;
		Shot.At = At;
		Shots.Add(Shot);
	};
	Add(Fleets, TEXT("the-fleets"), Contact >= 0.0 ? FMath::Max(S.BeginT + 12.0, Contact - 40.0) : S.BeginT + 30.0);
	if (Line.At >= 0.0)
	{
		// (twenty seconds on, the columns round onto the new course in line ahead; kept 8 s clear of the broadside, so a
		// watched cut holds each)
		double LineAt = Line.At + 20.0;
		if (Broadside.At >= 0.0 && FMath::Abs(LineAt - Broadside.At) < 8.0 && Broadside.At - 8.0 >= Line.At + 10.0)
		{
			LineAt = Broadside.At - 8.0;
		}
		Add(Line, TEXT("the-line"), LineAt);
	}
	if (Broadside.At >= 0.0)
	{
		Add(Broadside, TEXT("broadside"), Broadside.At + 0.2);
	}
	if (Fall.At >= 0.0)
	{
		// (a second and a half after the last column falls: all of them standing, the first near their height)
		Add(Fall, TEXT("fall-of-shot"), Fall.At + 1.5);
	}
	if (Struck.At >= 0.0)
	{
		Add(Struck, TEXT("a-hit"), Struck.At + 0.5);
	}
	if (Run.At >= 0.0)
	{
		Add(Run, TEXT("torpedo-tracks"), Run.At + 20.0);
	}
	if (Strike.At >= 0.0)
	{
		Add(Strike, TEXT("the-strike"), Strike.At - 2.5);
	}
	if (Down.At >= 0.0)
	{
		Add(Down, TEXT("going-down"), Down.At + 20.0);
	}
	if (Funnels.At >= 0.0)
	{
		Add(Funnels, TEXT("funnels"), Funnels.At + 2.0);
	}
	FShot Whole;
	Whole.Kind = ESeaShot::Overview;
	Add(Whole, TEXT("the-battle"), Contact >= 0.0 ? Contact + (S.EndT - Contact) * 0.45 : (S.BeginT + S.EndT) * 0.5);
	if (Burning.At >= 0.0)
	{
		Add(Burning, TEXT("the-end"), S.EndT - 8.0);
	}
	else
	{
		Add(Whole, TEXT("the-end"), S.EndT - 8.0);
	}
	if (bWaterSheet)
	{
		Shots.Reset();
		static const TCHAR* const Names[4] = { TEXT("water-28m-away-from-sun"), TEXT("water-28m-toward-sun"), TEXT("water-60m-away-from-sun"), TEXT("water-60m-toward-sun") };
		for (int32 K = 0; K < 4; ++K)
		{
			FShot Sea;
			Sea.Kind = ESeaShot::Water;
			Sea.Spot = FVector2D(K < 2 ? 28.0 : 60.0, K % 2 ? 1.0 : 0.0); // (its height, and whether toward the sun)
			Add(Sea, Names[K], SheetT + K * 1.0);
		}
		for (int32 K = 0; K < 20; ++K)
		{
			Ride.Spot = FVector2D(-1.0, 0.0); // (the pair, side on)
			Add(Ride, *FString::Printf(TEXT("ride-%02d"), K), SheetT + 6.0 + K * 0.5);
		}
	}
	Shots.RemoveAll([&S](const FShot& Shot) { return Shot.At < S.BeginT + 2.0; });
	Shots.StableSort([](const FShot& A, const FShot& B) { return A.At < B.At; });
	for (int32 K = 0; K < Shots.Num(); ++K)
	{
		Shots[K].At = FMath::Min(K > 0 ? FMath::Max(Shots[K].At, Shots[K - 1].At + (bWaterSheet ? 0.5 : 1.0)) : Shots[K].At, S.EndT + 4.0);
		Shots[K].Name = FString::Printf(TEXT("%02d-%s"), K + 1, *Shots[K].Name);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: shot %s at %.1f s (ship %d)."), *Shots[K].Name, Shots[K].At, Shots[K].Subject);
	}
}

bool ASeaBattle::ShotEye(const FShot& Shot, FVector& OutEye, FVector& OutLook, double& OutLens)
{
	using SeaParts::Ahead;
	using SeaParts::Starboard;
	FSeaBattleState& S = *State;
	auto Find = [this, &S](int32 Which, FVector2D& OutP, double& OutHeading, double& OutLong) -> bool
	{
		if (Which < 0)
		{
			return false;
		}
		if (const SeaShow::ShipOut* Ship = S.Show.Ship(Which))
		{
			OutP = FVector2D(Ship->At.X, Ship->At.Y);
			OutHeading = Ship->Heading;
			OutLong = DrawnLong.Contains(Which) ? DrawnLong[Which] : Ship->Long;
			return true;
		}
		// (a ship the show no longer draws, gone down or out of the fight: where she was last)
		if (const FVector4* Was = LastSeen.Find(Which))
		{
			OutP = FVector2D(Was->X, Was->Y);
			OutHeading = Was->Z;
			OutLong = Was->W;
			return true;
		}
		return false;
	};
	auto HerSide = [&S](int32 Which)
	{
		const SeaShow::ShipOut* Ship = S.Show.Ship(Which);
		return Ship ? Ship->Side : -1;
	};
	auto Centre = [&S](int32 Which, FVector2D& Out) -> bool
	{
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Side == Which && Ship.Doing == SeaShow::Fate::Line)
			{
				Sum += FVector2D(Ship.At.X, Ship.At.Y);
				++Count;
			}
		}
		if (Count == 0)
		{
			return false;
		}
		Out = Sum / static_cast<double>(Count);
		return true;
	};
	FVector2D P = FVector2D::ZeroVector;
	double Heading = 0.0, Long = 150.0;
	switch (Shot.Kind)
	{
	case ESeaShot::Fleets:
	{
		// Behind the column's quarter, looking up its line as it zig-zags.
		SeaShow::Vec Middle;
		double Face = 0.0;
		if (S.Show.PieceMiddle(Shot.Other, Middle, Face))
		{
			P = FVector2D(Middle.X, Middle.Y);
			Heading = FMath::RadiansToDegrees(Face);
		}
		else if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		const FVector2D From = P - Ahead(Heading) * 700.0 + Starboard(Heading) * 420.0, To = P + Ahead(Heading) * 250.0;
		OutEye = World(From.X, From.Y, 55.0);
		OutLook = World(To.X, To.Y, 0.0);
		OutLens = 45.0;
		return true;
	}
	case ESeaShot::Line:
	{
		// Ahead of the line's guide on its engaged side, low, looking back down the line as it steams in line ahead, its
		// guns trained out past the camera.
		const int32 Hers = Shot.Other;
		bool bGuide = Find(Shot.Subject, P, Heading, Long);
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Side != Hers || Ship.LineAt < 0)
			{
				continue;
			}
			Sum += FVector2D(Ship.At.X, Ship.At.Y);
			++Count;
			if (!bGuide && Ship.LineAt == 0)
			{
				// (the guide's leading ship gone: another of the guide's column)
				P = FVector2D(Ship.At.X, Ship.At.Y);
				Heading = Ship.Heading;
				Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
				bGuide = true;
			}
		}
		if (!bGuide)
		{
			return false;
		}
		const FVector2D Course = Ahead(Heading);
		// (the line's own axis, from its middle up to its guide: the course once it is in line ahead, a bearing line while
		// its rows are still coming round; the course where the line is one column)
		const FVector2D Middle = Count > 0 ? Sum / static_cast<double>(Count) : P - Course * 800.0;
		const double Spread = FVector2D::Distance(P, Middle);
		const FVector2D Axis = Spread > 2.0 * Long ? (P - Middle) / Spread : Course;
		FVector2D Out(Axis.Y, -Axis.X);
		FVector2D Foe;
		if (Hers >= 0 && Centre(1 - Hers, Foe) && FVector2D::DotProduct(Foe - P, Out) < 0.0)
		{
			Out = -Out;
		}
		// (out ahead of the guide along the axis and a little to the engaged side, looking a third of the way down the
		// line: the guide near the frame's edge, the line going away behind her)
		const FVector2D From = P + Axis * (Long * 5.0) + Out * (Long * 2.0);
		const FVector2D Down = P - Axis * (FMath::Max(Spread, 600.0) * 0.35);
		OutEye = World(From.X, From.Y, 20.0);
		OutLook = World(Down.X, Down.Y, 8.0);
		OutLens = 40.0;
		return true;
	}
	case ESeaShot::Broadside:
	{
		// On her far side from her target, low, looking over her toward the enemy: her guns against the sky.
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		FVector2D Target = Shot.Spot;
		double TargetHeading = 0.0, TargetLong = 0.0;
		if (!Find(Shot.Other, Target, TargetHeading, TargetLong))
		{
			const int32 Hers = HerSide(Shot.Subject);
			if (Hers < 0 || !Centre(1 - Hers, Target))
			{
				Target = Shot.Spot;
			}
		}
		const FVector2D To = (Target - P).GetSafeNormal();
		const FVector2D From = P - To * (Long * 0.95) - Ahead(Heading) * (Long * 0.55);
		const FVector2D Over = P + To * 2000.0;
		OutEye = World(From.X, From.Y, 24.0);
		OutLook = World(Over.X, Over.Y, 12.0);
		OutLens = 55.0;
		return true;
	}
	case ESeaShot::Rangefinder:
	{
		// From the firing side, 28 m up through a 19 degree lens (the art director, Samar r2e frame 06): the straddled
		// ship a third of the frame wide, as near as that puts the eye (never nearer than 700 m nor farther than 2.6 km),
		// looking a little over her waterline so the columns' 40 to 60 m stand in the frame.
		int32 Target = HerSide(Shot.Subject);
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
			double Best = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : S.Show.Ships())
			{
				const double Away = FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), P);
				if (Away < Best)
				{
					Best = Away;
					Target = Ship.Side;
				}
			}
		}
		FVector2D Firing = P + FVector2D(0.0, -3000.0);
		if (Target >= 0)
		{
			Centre(1 - Target, Firing);
		}
		FVector2D Way = (Firing - P).GetSafeNormal();
		Way = Way.IsNearlyZero() ? FVector2D(0.0, -1.0) : Way;
		const double Lens = 19.0;
		const double HalfTan = FMath::Tan(FMath::DegreesToRadians(Lens * 0.5));
		// (how wide she shows from there: her length across the line of sight, her beam along it)
		const double Side = FMath::Abs(FVector2D::DotProduct(Ahead(Heading), FVector2D(Way.Y, -Way.X)));
		const double Shows = Long * Side + 0.12 * Long * FMath::Sqrt(FMath::Max(0.0, 1.0 - Side * Side));
		const double Away = FMath::Clamp(3.0 * Shows / (2.0 * HalfTan), 700.0, 2600.0);
		const FVector2D From = P + Way * Away;
		OutEye = World(From.X, From.Y, 28.0);
		OutLook = World(P.X, P.Y, 22.0);
		OutLens = Lens;
		return true;
	}
	case ESeaShot::Hit:
	{
		// Off her side toward the enemy, where the shells came from.
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		FVector2D Toward = Starboard(Heading);
		FVector2D Foe;
		const int32 Hers = HerSide(Shot.Subject);
		if (Hers >= 0 && Centre(1 - Hers, Foe) && FVector2D::DotProduct(Foe - P, Toward) < 0.0)
		{
			Toward = -Toward;
		}
		const FVector2D From = P + Toward * (Long * 1.7) - Ahead(Heading) * (Long * 0.5);
		OutEye = World(From.X, From.Y, 30.0);
		OutLook = World(P.X, P.Y, 8.0);
		OutLens = 40.0;
		return true;
	}
	case ESeaShot::Tracks:
	{
		// Above and behind where the salvo was fired, looking down its tracks.
		const FVector2D Way = Shot.Way.IsNearlyZero() ? FVector2D(0.0, 1.0) : Shot.Way.GetSafeNormal();
		const FVector2D Aside(Way.Y, -Way.X);
		const FVector2D From = Shot.Spot - Way * 350.0 + Aside * 150.0, To = Shot.Spot + Way * 700.0;
		OutEye = World(From.X, From.Y, 260.0);
		OutLook = World(To.X, To.Y, 0.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Strike:
	{
		// Low, across her from the far side of her attackers: she low in the picture and the bombers coming down above
		// her, the lens as wide as both need. (The first Mac frames, 21 November: looking up into the air over her beam,
		// the dive was out of the picture and she was cut off at its foot.)
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
			Heading = 0.0;
		}
		const int32 Hers = HerSide(Shot.Subject);
		FVector Sum = FVector::ZeroVector;
		int32 Diving = 0;
		for (const SeaShow::PlaneOut& Plane : S.Show.Planes())
		{
			if (!Plane.bShown || Plane.bFalling || Plane.Role == "cover" || (Hers >= 0 && Plane.Side == Hers))
			{
				continue;
			}
			if (FVector2D(Plane.At.X - P.X, Plane.At.Y - P.Y).Size() > 4000.0)
			{
				continue;
			}
			Sum += FVector(Plane.At.X, Plane.At.Y, Plane.At.Z);
			++Diving;
		}
		const double EyeUp = 18.0;
		if (Diving == 0)
		{
			const FVector2D From = P + Starboard(Heading) * 650.0 - Ahead(Heading) * 250.0;
			OutEye = World(From.X, From.Y, EyeUp);
			OutLook = World(P.X, P.Y, 90.0);
			OutLens = 55.0;
			return true;
		}
		const FVector Them = Sum / static_cast<double>(Diving);
		FVector2D Toward(Them.X - P.X, Them.Y - P.Y);
		Toward = Toward.Size() < 1.0 ? Starboard(Heading) : Toward.GetSafeNormal();
		const FVector2D Aside(-Toward.Y, Toward.X);
		const FVector2D From = P - Toward * FMath::Max(500.0, Long * 2.2) + Aside * 160.0;
		// (the angles up from the eye to her waterline and to them: the picture's middle between the two, its height
		// both and a margin, at the pictures' 1.6 to 1)
		const double ToHer = FMath::Atan2(4.0 - EyeUp, FVector2D::Distance(From, P));
		const double ToThem = FMath::Atan2(Them.Z - EyeUp, FVector2D::Distance(From, FVector2D(Them.X, Them.Y)));
		const double Tall = FMath::Clamp((ToThem - ToHer) * 1.35 + FMath::DegreesToRadians(6.0), FMath::DegreesToRadians(18.0), FMath::DegreesToRadians(60.0));
		OutLens = FMath::Clamp(FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(Tall * 0.5) * 1.6)), 40.0, 85.0);
		const double Up = 0.5 * (ToHer + ToThem);
		const FVector2D Look = ((P + FVector2D(Them.X, Them.Y)) * 0.5 - From).GetSafeNormal();
		OutEye = World(From.X, From.Y, EyeUp);
		OutLook = World(From.X + Look.X * 1000.0 * FMath::Cos(Up), From.Y + Look.Y * 1000.0 * FMath::Cos(Up), EyeUp + 1000.0 * FMath::Sin(Up));
		return true;
	}
	case ESeaShot::Sinking:
	{
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			return false;
		}
		const FVector2D From = P + Starboard(Heading) * (Long * 2.6) + Ahead(Heading) * (Long * 0.9);
		OutEye = World(From.X, From.Y, 28.0);
		OutLook = World(P.X, P.Y, 2.0);
		OutLens = 38.0;
		return true;
	}
	case ESeaShot::Burning:
	{
		if (!Find(Shot.Subject, P, Heading, Long))
		{
			P = Shot.Spot;
		}
		const FVector2D From = P + FVector2D(700.0, -350.0);
		OutEye = World(From.X, From.Y, 70.0);
		OutLook = World(P.X, P.Y, 40.0);
		OutLens = 45.0;
		return true;
	}
	case ESeaShot::Funnels:
	{
		// Side on to the big ship from the destroyer's side of her, low, far enough out for both and their smoke astern.
		FVector2D Other;
		double OtherHeading = 0.0, OtherLong = 0.0;
		if (!Find(Shot.Subject, P, Heading, Long) || !Find(Shot.Other, Other, OtherHeading, OtherLong))
		{
			return false;
		}
		const FVector2D Middle = (P + Other) * 0.5;
		const double Way = FVector2D::DotProduct(Other - P, Starboard(Heading)) >= 0.0 ? 1.0 : -1.0;
		const double Apart = FVector2D::Distance(P, Other);
		const FVector2D From = Middle + Starboard(Heading) * (Way * FMath::Max(650.0, Apart * 1.2 + Long)) - Ahead(Heading) * (Long * 0.4);
		OutEye = World(From.X, From.Y, 16.0);
		OutLook = World(Middle.X, Middle.Y, 22.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Water:
	{
		// The water's check sheet (PlanShots): the eye 28 or 60 m up, 2 km short of the ships' middle, looking over them
		// away from the sun or toward it through a 50 degree lens, a little down, the horizon a little above the frame's
		// middle; or the pair side by side, from 1.2 km off on the sun's side, 30 m up.
		const FVector Lit = Stage ? Stage->TowardSun() : FVector(1.0, 0.0, 0.5);
		FVector2D Sun = FVector2D(Lit.Y, Lit.X).GetSafeNormal();
		Sun = Sun.IsNearlyZero() ? FVector2D(0.0, 1.0) : Sun;
		// (how far the nearest ship afloat lies from a point: the eye is never put among them)
		auto Clear = [&S](const FVector2D& At)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (const SeaShow::ShipOut& Ship : S.Show.Ships())
			{
				if (Ship.Doing != SeaShow::Fate::Sinking)
				{
					Nearest = FMath::Min(Nearest, FVector2D::Distance(FVector2D(Ship.At.X, Ship.At.Y), At));
				}
			}
			return Nearest;
		};
		if (Shot.Spot.X < 0.0)
		{
			FVector2D A, B;
			double HeadingA = 0.0, HeadingB = 0.0, LongA = 100.0, LongB = 200.0;
			if (!Find(Shot.Subject, A, HeadingA, LongA))
			{
				return false;
			}
			if (!Find(Shot.Other, B, HeadingB, LongB))
			{
				B = A;
				LongB = LongA;
			}
			const FVector2D Middle = (A + B) * 0.5;
			FVector2D Abeam = FVector2D(Ahead(HeadingB).Y, -Ahead(HeadingB).X);
			Abeam = FVector2D::DotProduct(Abeam, Sun) < 0.0 ? -Abeam : Abeam;
			const double Span = FVector2D::Distance(A, B) + 0.5 * (LongA + LongB) + 120.0;
			// (on her sunlit side 1.2 to 1.8 km off where no other ship lies within 600 m of the eye, else her shaded side,
			// else wherever the nearest ship lies farthest)
			FVector2D From = Middle + Abeam * 1200.0;
			double Off = 1200.0, Roomiest = -1.0;
			bool bFound = false;
			for (int32 Try = 0; Try < 6 && !bFound; ++Try)
			{
				const double Far = 1200.0 + 300.0 * (Try % 3);
				const FVector2D Here = Middle + (Try < 3 ? Abeam : -Abeam) * Far;
				const double Room = Clear(Here);
				bFound = Room >= 600.0;
				if (bFound || Room > Roomiest)
				{
					Roomiest = Room;
					From = Here;
					Off = Far;
				}
			}
			OutEye = World(From.X, From.Y, 30.0);
			OutLook = World(Middle.X, Middle.Y, 12.0);
			OutLens = FMath::Clamp(FMath::RadiansToDegrees(2.0 * FMath::Atan(0.5 * Span / Off)), 15.0, 55.0);
			return true;
		}
		FVector2D Sum = FVector2D::ZeroVector;
		int32 Count = 0;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			if (Ship.Doing == SeaShow::Fate::Line)
			{
				Sum += FVector2D(Ship.At.X, Ship.At.Y);
				++Count;
			}
		}
		const FVector2D Middle = Count ? Sum / static_cast<double>(Count) : FVector2D(S.Rec.Board.Size * 0.5, S.Rec.Board.Size * 0.5);
		const FVector2D Way = Shot.Spot.Y > 0.5 ? Sun : -Sun;
		// (2 km short of the ships' middle, and farther back until the nearest ship is a kilometre off)
		FVector2D From = Middle - Way * 2000.0;
		for (double Back = 2000.0; Back <= 6000.0 && Clear(From) < 1000.0; Back += 250.0)
		{
			From = Middle - Way * Back;
		}
		OutEye = World(From.X, From.Y, Shot.Spot.X);
		OutLook = World(Middle.X, Middle.Y, 0.0);
		OutLens = 50.0;
		return true;
	}
	case ESeaShot::Overview:
	default:
	{
		// High behind the attacker's ships and off to one side, both sides in the picture.
		FVector2D A, B;
		const bool bA = Centre(0, A), bB = Centre(1, B);
		if (!bA && !bB)
		{
			A = B = FVector2D(S.Rec.Board.Size * 0.5, S.Rec.Board.Size * 0.5);
		}
		else if (!bA)
		{
			A = B;
		}
		else if (!bB)
		{
			B = A;
		}
		const FVector2D Middle = (A + B) * 0.5;
		FVector2D Way = (B - A).GetSafeNormal();
		if (Way.IsNearlyZero())
		{
			Way = FVector2D(0.0, 1.0);
		}
		const FVector2D Aside(Way.Y, -Way.X);
		const FVector2D From = A - Way * 2200.0 + Aside * 1500.0;
		OutEye = World(From.X, From.Y, 1300.0);
		OutLook = World(Middle.X, Middle.Y, 0.0);
		OutLens = 55.0;
		return true;
	}
	}
}

void ASeaBattle::AimEye()
{
	if (!Eye || !State.IsValid())
	{
		return;
	}
	FSeaBattleState& S = *State;
	// A picture run stands on its next picture from the moment the last was taken; watched, the camera cuts to each
	// shot a few seconds before its moment.
	int32 Want = -1;
	if (!ShotFolder.IsEmpty())
	{
		Want = FMath::Min(NextPicture, Shots.Num() - 1);
	}
	else
	{
		for (int32 K = 0; K < Shots.Num(); ++K)
		{
			if (S.BattleT >= Shots[K].At - SeaParts::CutBefore)
			{
				Want = K;
			}
		}
		Want = Want < 0 && Shots.Num() > 0 ? 0 : Want;
	}
	FShot Whole;
	Whole.Kind = ESeaShot::Overview;
	FVector From = FVector::ZeroVector, To = FVector::ForwardVector;
	double Lens = 50.0;
	if (!Shots.IsValidIndex(Want) || !ShotEye(Shots[Want], From, To, Lens))
	{
		ShotEye(Whole, From, To, Lens);
	}
	From.Z = FMath::Max(From.Z, 3.0 * UnitsPerMetre);
	EyeAt = From;
	EyeFacing = (To - From).Rotation();
	EyeLens = Lens;
	ShotNow = Want;
	Eye->SetActorLocationAndRotation(EyeAt, EyeFacing);
	Eye->GetCameraComponent()->SetFieldOfView(static_cast<float>(EyeLens));
}

void ASeaBattle::TakePictures()
{
	if (ShotFolder.IsEmpty() || !State.IsValid())
	{
		return;
	}
	FSeaBattleState& S = *State;
	if (!Shots.IsValidIndex(NextPicture))
	{
		if (QuitAt < 0.0)
		{
			QuitAt = RealT + 1.5;
		}
		else if (RealT >= QuitAt)
		{
			Quit(TEXT("the pictures are taken"));
		}
		return;
	}
	FShot& Shot = Shots[NextPicture];
	if (PictureAt < 0.0 && S.BattleT >= Shot.At)
	{
		const FString Path = FPaths::Combine(ShotFolder, RecordKey + TEXT("-") + Shot.Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, true, false); // (with the screen: the bar is in the picture)
		// (and the funnels smoking, the shot's own ship's among them or not: the funnels picture of Samar r2e had none on
		// its hero)
		const uint8* Hers = Shot.Subject >= 0 ? FunnelsChosen.Find(Shot.Subject) : nullptr;
		UE_LOG(LogTheater, Display, TEXT("Sea battle: picture %s at %.1f s (the eye %.0f m up, a %.0f degree lens); %d funnel plumes, %d ships, the subject's %s."), *Shot.Name, S.BattleT,
			EyeAt.Z / UnitsPerMetre, EyeLens, FunnelTrails.Num(), FunnelsChosen.Num(), Hers ? (*Hers == 2 ? TEXT("from each funnel") : TEXT("one plume")) : TEXT("none"));
		PictureAt = RealT;
	}
	else if (PictureAt >= 0.0 && RealT >= PictureAt + 0.5)
	{
		// (half a second for the picture to be written before the camera moves on)
		Shot.bTaken = true;
		++NextPicture;
		PictureAt = -1.0;
	}
}

void ASeaBattle::Quit(const FString& Why)
{
	UE_LOG(LogTheater, Display, TEXT("Sea battle: %s; quitting."), *Why);
	SetActorTickEnabled(false);
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------------------------------------------------------
// The sound: the battle pack's takes, in the mix's distance families, each heard after its flash.

USoundBase* ASeaBattle::SoundOf(const FName& Name, bool bLoop)
{
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Name))
	{
		return Found->Get();
	}
	if (SoundsMissing.Contains(Name))
	{
		return nullptr;
	}
	const FString Text = Name.ToString();
	USoundBase* Made = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), *Text, *Text), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Made)
	{
		SoundsMissing.Add(Name);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: the sound %s is not in /Game/Campaign/Sounds/Real (Tools/build_sounds.sh brings it in)"), *Text);
		return nullptr;
	}
	if (bLoop)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Made))
		{
			Wave->bLooping = true;
		}
	}
	Sounds.Add(Name, Made);
	return Made;
}

USoundAttenuation* ASeaBattle::FamilyOf(const FName& Name)
{
	if (TObjectPtr<USoundAttenuation>* Found = Families.Find(Name))
	{
		return Found->Get();
	}
	const TSharedPtr<FJsonObject>* Distance = nullptr;
	const TSharedPtr<FJsonObject>* Family = nullptr;
	if (!Mix.IsValid() || !Mix->TryGetObjectField(TEXT("distance"), Distance) || !(*Distance)->TryGetObjectField(Name.ToString(), Family))
	{
		return nullptr;
	}
	// A distance family of the mix: full loudness to `full` metres, nothing past `silent`, the top taken off with
	// distance by its lpf points (as the air battle reads it).
	double Full = 50.0, Silent = 1500.0;
	(*Family)->TryGetNumberField(TEXT("full"), Full);
	(*Family)->TryGetNumberField(TEXT("silent"), Silent);
	USoundAttenuation* Made = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Made->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(Full * UnitsPerMetre, 0.0, 0.0);
	A.FalloffDistance = static_cast<float>(FMath::Max(1.0, Silent - Full) * UnitsPerMetre);
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	const TArray<TSharedPtr<FJsonValue>>* Lpf = nullptr;
	if ((*Family)->TryGetArrayField(TEXT("lpf"), Lpf) && Lpf->Num() >= 2)
	{
		const TArray<TSharedPtr<FJsonValue>>& Near = (*Lpf)[1]->AsArray();
		const TArray<TSharedPtr<FJsonValue>>& Far = Lpf->Last()->AsArray();
		if (Near.Num() == 2 && Far.Num() == 2)
		{
			A.bAttenuateWithLPF = true;
			A.LPFRadiusMin = static_cast<float>(Near[0]->AsNumber() * UnitsPerMetre);
			A.LPFRadiusMax = static_cast<float>(Far[0]->AsNumber() * UnitsPerMetre);
			A.LPFFrequencyAtMin = static_cast<float>(Near[1]->AsNumber());
			A.LPFFrequencyAtMax = static_cast<float>(Far[1]->AsNumber());
		}
	}
	Families.Add(Name, Made);
	return Made;
}

UAudioComponent* ASeaBattle::Sound(const FName& Name, const FVector& Where, const FName& Family, float Volume, bool bFlashFirst)
{
	if (bSkipping || Name.IsNone())
	{
		return nullptr;
	}
	if (bFlashFirst && Eye)
	{
		// (flash, then bang: sound crosses the water at 343 m a second; never held more than 8 s)
		const double Metres = FVector::Dist(Where, Eye->GetActorLocation()) / UnitsPerMetre;
		if (Metres > 150.0)
		{
			if (Later.Num() < 64)
			{
				FLater& Held = Later.AddDefaulted_GetRef();
				Held.At = RealT + FMath::Min(Metres / 343.0, 8.0) / Speed;
				Held.Name = Name;
				Held.Where = Where;
				Held.Family = Family;
				Held.Volume = Volume;
			}
			return nullptr;
		}
	}
	USoundBase* Played = SoundOf(Name);
	if (!Played)
	{
		return nullptr;
	}
	return UGameplayStatics::SpawnSoundAtLocation(this, Played, Where, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, FamilyOf(Family));
}

void ASeaBattle::GunFired(int32 Key, int32 Slot, const FVector& At)
{
	// Her light guns heard (mix.json air.gunsByCalibre, as the air scene hears a ship's): one battery for each ship and
	// kind of gun (ShipGuns::HeardKey), its takes laid end to end while any of its mounts fires, each only the mix's
	// part of its recording (the firing, without the range's echo), the next due as that part ends.
	const int32 Heard = ShipGuns::HeardKey(Key, Slot);
	GunsFiring.Add(Key, Heard);
	const double* Due = GunHeardAt.Find(Heard);
	if (Due && RealT < *Due)
	{
		return;
	}
	FString Take;
	double Cut = 0.0;
	if (Slot == 1 || Slot == 2)
	{
		Take = SeaParts::MixTake(Mix, TEXT("air.gunsByCalibre.cannon.take"));
		Cut = SeaParts::MixNumber(Mix, TEXT("air.gunsByCalibre.cannon.part"), 0.0);
	}
	else
	{
		Take = SeaParts::MixTake(Mix, TEXT("air.gunsByCalibre.heavy.handHeld"));
		if (!Take.IsEmpty())
		{
			Cut = SeaParts::MixNumber(Mix, FString(TEXT("air.gunsByCalibre.heavy.partByTake.")) + Take, 0.0);
		}
	}
	USoundBase* Wave = Take.IsEmpty() ? nullptr : SoundOf(FName(*Take));
	double Again = 0.9;
	if (Wave)
	{
		const double Whole = Wave->GetDuration();
		Again = Cut > 0.0 ? Cut : (Whole > 0.0 && Whole < 10.0 ? Whole : 0.9);
	}
	Again = FMath::Clamp(Again - 0.02, FMath::Max(0.3, SeaParts::MixNumber(Mix, TEXT("air.sameTakeGap"), 0.15)), 2.0);
	GunHeardAt.Add(Heard, RealT + Again);
	UAudioComponent* Played = Wave ? Sound(FName(*Take), At, TEXT("air_guns"), 1.0f, false) : nullptr;
	if (!Played)
	{
		return;
	}
	if (Cut > 0.0)
	{
		// (only the firing: faded where the mix's part ends)
		TWeakObjectPtr<UAudioComponent> Weak(Played);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->FadeOut(0.08f, 0.0f);
			}
		}), static_cast<float>(Cut), false);
	}
	GunTakes.Add(Heard, Played);
}

void ASeaBattle::GunStopped(int32 Key)
{
	if (Effects)
	{
		Effects->AirGunStop(Key);
	}
	int32 Heard = 0;
	if (!GunsFiring.RemoveAndCopyValue(Key, Heard))
	{
		return;
	}
	for (const TPair<int32, int32>& Other : GunsFiring)
	{
		if (Other.Value == Heard)
		{
			return;
		}
	}
	// (the battery's last mount silent: a take still playing faded at once, not left to run on after the guns stop)
	GunHeardAt.Remove(Heard);
	TWeakObjectPtr<UAudioComponent> Take;
	if (GunTakes.RemoveAndCopyValue(Heard, Take) && Take.IsValid())
	{
		Take->FadeOut(0.08f, 0.0f);
	}
}

void ASeaBattle::Beds()
{
	// The sound director's sea beds (mix.json sea.beds): the sea by day always, the battle's own bed from contact.
	if (!DayBed)
	{
		if (USoundBase* Day = SoundOf(TEXT("sea_day_loop"), true))
		{
			DayBed = UGameplayStatics::SpawnSound2D(this, Day, SeaParts::FromDb(-10.0), 1.0f, 0.0f, nullptr, false, false);
		}
	}
	if (!bBattleBedUp && State.IsValid() && State->Show.ContactAt() >= 0.0 && State->BattleT >= State->Show.ContactAt())
	{
		bBattleBedUp = true;
		if (USoundBase* Fight = SoundOf(TEXT("sea_battle_loop"), true))
		{
			BattleBed = UGameplayStatics::SpawnSound2D(this, Fight, SeaParts::FromDb(-6.0), 1.0f, 0.0f, nullptr, false, false);
			if (BattleBed)
			{
				BattleBed->FadeIn(4.0f, SeaParts::FromDb(-6.0));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Each frame.

void ASeaBattle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Here = GetWorld();
	if (!PendingReplay.IsEmpty())
	{
		if (Here && Here->GetTimeSeconds() > 2.0)
		{
			const FString Asked = PendingReplay;
			PendingReplay.Empty();
			if (!BeginReplay(Asked))
			{
				FString Folder;
				if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaBattleShots="), Folder))
				{
					Quit(TEXT("the recorded action could not be played"));
				}
				SetActorTickEnabled(false);
			}
		}
		return;
	}
	if (!bBegun || !State.IsValid() || !Stage)
	{
		return;
	}
	FSeaBattleState& S = *State;
	const double Dt = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	RealT += Dt;
	// A picture run skips the quiet stretch before its next picture, keeping what lasts (ships gone down, oil burning).
	bool bSkippedOn = false;
	if (!ShotFolder.IsEmpty() && Shots.IsValidIndex(NextPicture) && PictureAt < 0.0)
	{
		const double To = FMath::Min(Shots[NextPicture].At - 20.0, S.EndT);
		if (To - S.BattleT > 15.0)
		{
			bSkipping = true;
			for (double T = S.BattleT + 1.0; T < To; T += 1.0)
			{
				StepTo(T, true);
			}
			StepTo(To, true);
			bSkipping = false;
			if (Effects)
			{
				TGuardValue<bool> Water(bOnWater, true);
				for (const FOilLater& Oil : OilLater)
				{
					Effects->OilFire(Oil.Id, World(Oil.At.X, Oil.At.Y, 0.0), static_cast<float>(Oil.Reach), Oil.Kind, Oil.Until, static_cast<float>(Oil.Since + S.BattleT - Oil.LitAt));
				}
			}
			OilLater.Reset();
			FlakLater.Reset();
			Later.Reset();
			bSkippedOn = true;
			UE_LOG(LogTheater, Display, TEXT("Sea battle: on to %.0f s, 20 s before %s."), S.BattleT, *Shots[NextPicture].Name);
		}
	}
	StepTo(FMath::Min(S.BattleT + Dt * Speed, S.EndT + 12.0), false);
	MoveKnot(Dt, bSkippedOn);
	for (int32 K = FlakLater.Num() - 1; K >= 0; --K)
	{
		if (FlakLater[K].At <= S.BattleT)
		{
			if (Effects)
			{
				Effects->AirFlak(S.BattleT, FlakLater[K].Where, FlakLater[K].Kind);
			}
			// (the burst heard: the mix's air.flak burst, after its flash, the first of any closer together than its
			// soundEvery; the picture keeps them all)
			if (RealT - FlakHeardAt >= SeaParts::MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6))
			{
				FlakHeardAt = RealT;
				const FString Bang = SeaParts::MixTake(Mix, TEXT("air.flak.burst"), 0);
				const FString Family = SeaParts::MixTake(Mix, TEXT("air.flak.burstFamily"));
				Sound(FName(Bang.IsEmpty() ? TEXT("flak_burst_01") : *Bang), FlakLater[K].Where, FName(Family.IsEmpty() ? TEXT("explosions") : *Family), 0.8f);
			}
			FlakLater.RemoveAtSwap(K);
		}
	}
	Stage->Advance(S.BattleT);
	DrawPlanes();
	RunTrails();
	DrawMarks();
	AimEye();
	if (Effects && Eye)
	{
		int32 ViewW = 1600, ViewH = 1000;
		if (APlayerController* Controller = Here ? Here->GetFirstPlayerController() : nullptr)
		{
			Controller->GetViewportSize(ViewW, ViewH);
		}
		Effects->Tick(S.BattleT, Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW, FLinearColor::White, ViewH);
	}
	for (int32 K = Later.Num() - 1; K >= 0; --K)
	{
		if (Later[K].At <= RealT)
		{
			const FLater Due = Later[K];
			Later.RemoveAtSwap(K);
			Sound(Due.Name, Due.Where, Due.Family, Due.Volume, false);
		}
	}
	Beds();
	if (RealT - RoomCheckedAt >= 1.0)
	{
		RoomCheckedAt = RealT;
		LightTheBattle();
	}
	TakePictures();
	if (!ShotFolder.IsEmpty() && RealT > 1500.0)
	{
		Quit(TEXT("the picture run took more than 25 minutes"));
	}
}

void ASeaBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	HideScreen();
	for (UAudioComponent* Bed : { DayBed.Get(), BattleBed.Get() })
	{
		if (Bed)
		{
			Bed->Stop();
		}
	}
	if (Reason == EEndPlayReason::Destroyed)
	{
		GiveBackTheRoom(); // (taken down in the game: the table comes back as it was)
	}
	// (the stage first: its looks play on this scene's kit)
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	if (Effects)
	{
		for (const TPair<int32, int32>& Firing : GunsFiring)
		{
			Effects->AirGunStop(Firing.Key);
		}
		Effects->End();
		Effects.Reset();
	}
	GunsFiring.Reset();
	GunHeardAt.Reset();
	GunTakes.Reset();
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	State.Reset();
	Super::EndPlay(Reason);
}

bool ASeaBattle::GetBar(double& OutKnot, UTexture2D*& OutMine, UTexture2D*& OutTheirs) const
{
	if (!bBegun || !bBarGiven || !State.IsValid())
	{
		return false;
	}
	OutKnot = BarKnot;
	OutMine = MyFlag.Get();
	OutTheirs = TheirFlag.Get();
	return true;
}

void ASeaBattle::MoveKnot(double RealDt, bool bAtOnce)
{
	if (!State.IsValid())
	{
		return;
	}
	const FSeaBattleState& S = *State;
	// The view the picture has reached (never one ahead of it: the bar follows what is seen).
	size_t Upto = FMath::Min(S.Taken, S.Rec.Views.size());
	while (Upto > 1 && S.Rec.Views[Upto - 1].Head.T > S.BattleT + 1e-9)
	{
		--Upto;
	}
	const double Want = Upto > 0 ? SeaFeed::KnotOf(S.Rec.Views[Upto - 1].Head) : -1.0;
	bBarGiven = Want >= 0.0;
	if (!bBarGiven)
	{
		return;
	}
	// (a lag on the screen's own clock, not the battle's: at any pace the knot takes about a second and a half to arrive)
	constexpr double Lag = 0.5;
	BarKnot = bAtOnce ? Want : BarKnot + (Want - BarKnot) * (1.0 - FMath::Exp(-FMath::Max(RealDt, 0.0) / Lag));
}

void ASeaBattle::ShowScreen()
{
	if (!GEngine || !GEngine->GameViewport || Screen.IsValid())
	{
		return;
	}
	Screen = SNew(SSeaBattleScreen).Battle(this);
	// A quarter larger than Unreal's own game-screen size, as the air screen's (the art director on build 1734), so the
	// two battles' bars stand the same size; "-TheaterSeaTextScale=" tunes it. The screen takes no input: the scene's
	// own controls stay as they are.
	float Lettering = 1.25f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaTextScale="), Lettering);
	Lettering = FMath::Clamp(Lettering, 0.75f, 2.0f);
	ScreenFrame = SNew(SDPIScaler).DPIScale(Lettering)[Screen.ToSharedRef()];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenFrame.ToSharedRef(), 20);
}

void ASeaBattle::HideScreen()
{
	if (ScreenFrame.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenFrame.ToSharedRef());
	}
	ScreenFrame.Reset();
	Screen.Reset();
}
