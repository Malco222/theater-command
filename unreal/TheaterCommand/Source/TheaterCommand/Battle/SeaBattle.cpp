#include "Battle/SeaBattle.h"

#include "Battle/AirStage.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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
#include "UnrealClient.h"

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
	static constexpr int32 FireIds = 810000, FunnelIds = 820000, FallIds = 830000, OilIds = 840000;
	/** Seconds the camera stands on a shot's subject before its moment (watched; a picture stands on it from the last). */
	static constexpr double CutBefore = 4.0;
	/** How long a wake stays on the sea (seconds), and how long a stopped torpedo's track fades. */
	static constexpr double WakeLife = 100.0, TrackFade = 30.0;
	static const FLinearColor HazeInk(0.58f, 0.67f, 0.76f);
	static const FColor Foam(226, 232, 234);
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
	// (no funnel smoke unless asked for, until the kit has a "funnel" kind: the air trail behind a ship is a fence of
	// streaks, the first Mac frames of 21 November; the effects thread advises none in anything Brian sees)
	bFunnels = FParse::Param(FCommandLine::Get(), TEXT("TheaterSeaFunnels"));
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

	// The effects kit, drawn out to the air distance (a fleet action is watched from far off).
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
		Effects->UseAirDistance();
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
		}
	}

	PlanShots();
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
	}
	// "-TheaterSeaBattleFrom=300": the action from there (what lasts of the time before is kept, as a picture run's skip).
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
		}
	}
	Feed(S.BattleT);
	Beds();
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
		Stage->Steer(Ship.Key, At, Face, Ship.Heel, Ship.Pitch);
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
			FWakePoint& Made = Path.AddDefaulted_GetRef();
			Made.P = Stern;
			Made.T = Now;
			Made.Strength = Ship.Wake;
			Made.Beam = Ship.Beam;
		}
	}
}

void ASeaBattle::DrawMarks()
{
	FSeaBattleState& S = *State;
	const double Now = S.BattleT;
	// (a little over the sea's face, under every hull's waterline)
	const double Lift = 0.3;
	TArray<FVector> V;
	TArray<int32> Tri;
	TArray<FVector> Up;
	TArray<FColor> Paint;
	auto Point = [&](const FVector2D& P, double Alpha)
	{
		V.Add(World(P.X, P.Y, Lift));
		Up.Add(FVector::UpVector);
		FColor Foam = SeaParts::Foam;
		Foam.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Alpha * 255.0), 0, 255));
		Paint.Add(Foam);
	};
	// (each triangle the way round that faces up, as FMiniatureMesh::Face lays them)
	auto Face = [&](int32 A, int32 B, int32 D)
	{
		const FVector Facing = (V[B] - V[D]) ^ (V[A] - V[D]);
		if (Facing.Z >= 0.0)
		{
			Tri.Append({ A, B, D });
		}
		else
		{
			Tri.Append({ A, D, B });
		}
	};
	// A ribbon along a line of points: each point's middle, its half width and its strength, soft at both edges.
	auto Ribbon = [&](const TArray<FVector2D>& Mids, const TArray<double>& Halves, const TArray<double>& Alphas)
	{
		const int32 Count = Mids.Num();
		if (Count < 2 || Halves.Num() != Count || Alphas.Num() != Count)
		{
			return;
		}
		const int32 First = V.Num();
		for (int32 K = 0; K < Count; ++K)
		{
			const FVector2D Along = (Mids[FMath::Min(K + 1, Count - 1)] - Mids[FMath::Max(K - 1, 0)]).GetSafeNormal();
			const FVector2D Across(Along.Y, -Along.X);
			Point(Mids[K] - Across * Halves[K], 0.0);
			Point(Mids[K], Alphas[K]);
			Point(Mids[K] + Across * Halves[K], 0.0);
		}
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const int32 A = First + K * 3, B = A + 3;
			Face(A, A + 1, B + 1);
			Face(A, B + 1, B);
			Face(A + 1, A + 2, B + 2);
			Face(A + 1, B + 2, B + 1);
		}
	};
	TSet<int32> Afloat;
	for (const SeaShow::ShipOut& Ship : S.Show.Ships())
	{
		Afloat.Add(Ship.Key);
		const double Long = DrawnLong.Contains(Ship.Key) ? DrawnLong[Ship.Key] : Ship.Long;
		const FVector2D At(Ship.At.X, Ship.At.Y), Bow = SeaParts::Ahead(Ship.Heading), Right = SeaParts::Starboard(Ship.Heading);
		const bool bMaking = Ship.Doing != SeaShow::Fate::Sinking || Ship.Down < 0.4;
		// Her bow wave: two arms from her stem, close along her sides and then away at about 20 degrees, by her speed.
		if (bMaking && Ship.Wake > 0.12)
		{
			const FVector2D Stem = At + Bow * (Long * 0.5);
			const double Strength = Ship.Wake * WakeStrength;
			for (const double Hand : { -1.0, 1.0 })
			{
				const TArray<FVector2D> Arm = {
					Stem,
					Stem - Bow * (Long * 0.45) + Right * (Hand * Ship.Beam * 0.62),
					Stem - Bow * (Long * 1.15) + Right * (Hand * (Ship.Beam * 0.5 + Long * 0.3)),
				};
				Ribbon(Arm, TArray<double>{ 1.5, 3.5, 7.0 }, TArray<double>{ 0.75 * Strength, 0.55 * Strength, 0.0 });
			}
		}
	}
	// The wakes: each ship's stern path, the newest at her stern now, spreading and fading as the water settles.
	for (auto It = Wakes.CreateIterator(); It; ++It)
	{
		TArray<FWakePoint>& Path = It.Value();
		int32 Old = 0;
		while (Old < Path.Num() && Now - Path[Old].T > SeaParts::WakeLife)
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
		TArray<FVector2D> Mids;
		TArray<double> Halves, Alphas;
		for (int32 K = 0; K < Path.Num(); ++K)
		{
			const FWakePoint& W = Path[K];
			// (a wake broken where time jumped, as a run started part way through: never one straight strip over it)
			if (K > 0 && W.T - Path[K - 1].T > 6.0)
			{
				Ribbon(Mids, Halves, Alphas);
				Mids.Reset();
				Halves.Reset();
				Alphas.Reset();
			}
			const double Age = FMath::Max(0.0, Now - W.T);
			const double Fade = FMath::Square(1.0 - FMath::Min(Age / SeaParts::WakeLife, 1.0));
			Mids.Add(W.P);
			// (the white water spreads slowly, to under two beams each side: wider reads as a beam of light from a low eye)
			Halves.Add(FMath::Min(W.Beam * 0.55 + Age * 0.45, FMath::Min(W.Beam * 1.8, 60.0)));
			Alphas.Add(FMath::Clamp(W.Strength * WakeStrength * 0.6 * Fade, 0.0, 1.0));
		}
		if (const SeaShow::ShipOut* Ship = S.Show.Ship(It.Key()); Ship && (Ship->Doing != SeaShow::Fate::Sinking || Ship->Down < 0.4))
		{
			const double Long = DrawnLong.Contains(It.Key()) ? DrawnLong[It.Key()] : Ship->Long;
			const FVector2D Stern = FVector2D(Ship->At.X, Ship->At.Y) - SeaParts::Ahead(Ship->Heading) * (Long * 0.48);
			if (FVector2D::Distance(Mids.Last(), Stern) > 1.0)
			{
				Mids.Add(Stern);
				Halves.Add(Ship->Beam * 0.45);
				Alphas.Add(FMath::Clamp(Ship->Wake * WakeStrength * 0.7, 0.0, 1.0));
			}
		}
		Ribbon(Mids, Halves, Alphas);
	}
	// The torpedo tracks: a line of bubbles from where each was fired to its head, brightest behind the head; a track
	// whose torpedo has struck or run out fades.
	for (auto It = Tracks.CreateIterator(); It; ++It)
	{
		const FTrackLine& Line = It.Value();
		const double Left = Line.StopT < 0.0 ? 1.0 : 1.0 - (Now - Line.StopT) / SeaParts::TrackFade;
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
		const int32 Steps = FMath::Clamp(FMath::CeilToInt32(Run / 120.0), 1, 40);
		TArray<FVector2D> Mids;
		TArray<double> Halves, Alphas;
		for (int32 K = 0; K <= Steps; ++K)
		{
			const double Share = static_cast<double>(K) / Steps; // (0 at its head, 1 where it was fired)
			Mids.Add(Line.Head + (Line.From - Line.Head) * Share);
			Halves.Add(1.4 + 3.5 * Share);
			Alphas.Add(FMath::Clamp(0.6 * Left * FMath::Exp(-Share * Run / 1500.0) * WakeStrength, 0.0, 1.0));
		}
		Ribbon(Mids, Halves, Alphas);
	}
	if (!Marks)
	{
		return;
	}
	if (V.Num() == 0)
	{
		Marks->ClearMeshSection(0);
		return;
	}
	Marks->CreateMeshSection(0, V, Tri, Up, TArray<FVector2D>(), Paint, TArray<FProcMeshTangent>(), false);
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
	// Every few seconds, the funnels that smoke: the nearest ships steaming in their lines (the kit's trails are few).
	if (bFunnels && Now - FunnelsChosenAt >= 4.0)
	{
		FunnelsChosenAt = Now;
		TArray<TPair<double, int32>> ByNear;
		for (const SeaShow::ShipOut& Ship : S.Show.Ships())
		{
			const double Away = FVector::Dist(World(Ship.At.X, Ship.At.Y, 0.0), EyeAt) / UnitsPerMetre;
			if (Ship.Doing == SeaShow::Fate::Line && Ship.Funnel > 0.3 && Away < 7000.0)
			{
				ByNear.Add(TPair<double, int32>(Away, Ship.Key));
			}
		}
		ByNear.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
		FunnelsChosen.Reset();
		for (int32 K = 0; K < ByNear.Num() && K < 8; ++K)
		{
			FunnelsChosen.Add(ByNear[K].Value);
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
		// Her funnels' smoke, by how hard she steams (the kit's engine smoke, only with "-TheaterSeaFunnels" for now).
		if (FunnelsChosen.Contains(Ship.Key) && Ship.Doing == SeaShow::Fate::Line && !Ship.bBurning)
		{
			const int32 Id = SeaParts::FunnelIds + Ship.Key;
			const FVector2D Stack = At + Bow * (Long * 0.03);
			const FVector Spot = World(Stack.X, Stack.Y, Ship.Beam * 0.9);
			if (FunnelTrails.Contains(Id))
			{
				Kit->TrailMove(Now, Id, Spot, Velocity);
			}
			else
			{
				Kit->TrailStart(Now, Id, Spot, Velocity, TEXT("smoke"), static_cast<float>(Ship.Beam * 0.5));
				FunnelTrails.Add(Id);
			}
			Funnels.Add(Id);
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
	FShot Fleets, Line, Broadside, Fall, Struck, Run, Strike, Down, Burning;
	for (FShot* Each : { &Fleets, &Line, &Broadside, &Fall, &Struck, &Run, &Strike, &Down, &Burning })
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
	double Contact = -1.0;
	bool bBigGuns = false;
	auto Rank = [](const std::string& Class) { return Class == "bb" ? 4 : Class == "cv" ? 3 : Class == "ca" ? 2 : 1; };
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
		for (const SeaShow::Fx& E : Trial.Effects)
		{
			const FVector2D At(E.At.X, E.At.Y);
			switch (E.Kind)
			{
			case SeaShow::FxKind::Salvo:
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
				if (Fall.At < 0.0 && Broadside.At >= 0.0 && T >= Broadside.At + 6.0)
				{
					Fall.At = T;
					Fall.Spot = At;
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
		Add(Fall, TEXT("fall-of-shot"), Fall.At + 1.2);
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
	Shots.RemoveAll([&S](const FShot& Shot) { return Shot.At < S.BeginT + 2.0; });
	Shots.StableSort([](const FShot& A, const FShot& B) { return A.At < B.At; });
	for (int32 K = 0; K < Shots.Num(); ++K)
	{
		Shots[K].At = FMath::Min(K > 0 ? FMath::Max(Shots[K].At, Shots[K - 1].At + 1.0) : Shots[K].At, S.EndT + 4.0);
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
		// From the firing side, through a long lens: some 900 m of sea round the fall of shot.
		P = Shot.Spot;
		int32 Target = -1;
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
		FVector2D Firing = P + FVector2D(0.0, -3000.0);
		if (Target >= 0)
		{
			Centre(1 - Target, Firing);
		}
		const FVector2D Way = (Firing - P).GetSafeNormal();
		const double Away = 2600.0;
		const FVector2D From = P + (Way.IsNearlyZero() ? FVector2D(0.0, -1.0) : Way) * Away;
		OutEye = World(From.X, From.Y, 28.0);
		OutLook = World(P.X, P.Y, 16.0);
		OutLens = FMath::RadiansToDegrees(2.0 * FMath::Atan(450.0 / Away));
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
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogTheater, Display, TEXT("Sea battle: picture %s at %.1f s (the eye %.0f m up, a %.0f degree lens)."), *Shot.Name, S.BattleT, EyeAt.Z / UnitsPerMetre, EyeLens);
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
			UE_LOG(LogTheater, Display, TEXT("Sea battle: on to %.0f s, 20 s before %s."), S.BattleT, *Shots[NextPicture].Name);
		}
	}
	StepTo(FMath::Min(S.BattleT + Dt * Speed, S.EndT + 12.0), false);
	for (int32 K = FlakLater.Num() - 1; K >= 0; --K)
	{
		if (FlakLater[K].At <= S.BattleT)
		{
			if (Effects)
			{
				Effects->AirFlak(S.BattleT, FlakLater[K].Where, FlakLater[K].Kind);
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
		Effects->End();
		Effects.Reset();
	}
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	State.Reset();
	Super::EndPlay(Reason);
}
