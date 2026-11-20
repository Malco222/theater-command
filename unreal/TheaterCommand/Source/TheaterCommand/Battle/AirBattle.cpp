#include "Battle/AirBattle.h"

#include "Async/Async.h"
#include "Async/ParallelFor.h"

#include "Battle/AirBattleScreen.h"
#include "Battle/AirBattleState.h"
#include "Battle/AirGround.h"
#include "Battle/AirLand.h"
#include "Battle/AirScrew.h"
#include "Battle/AirStage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignBattleLook.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "Campaign/CampaignTextures.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>

/** The ground's pictures as they are painted aside (AAirBattle::PaintLand): the painter, its three pictures and each
 * one's smaller copies, ready for CampaignTextures::CreateFromLevels on the game thread. */
struct FAirLandMade
{
	std::unique_ptr<AirLand::Painter> Painter;
	std::vector<AirLand::Frame> Frames;
	TArray<CampaignTextures::FLevels> Levels;
	double Seconds = 0.0;
};

namespace AirParts
{
	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	/** The sheet of cloud shapes (AirSky::MakeFragments, some 190 ms of work): made once a run on the engine's pool of
	 * threads, begun as the first battle opens (Open) and waited for when its sky is laid, most of it done by then. */
	struct FSheetMaking
	{
		TFuture<void> Done;
		std::vector<uint8_t> Rgba;
		bool bBegun = false;
	};
	static FSheetMaking& SheetMaking()
	{
		static FSheetMaking Making;
		return Making;
	}
	static void BeginSheet()
	{
		FSheetMaking& Making = SheetMaking();
		if (!Making.bBegun)
		{
			Making.bBegun = true;
			Making.Done = Async(EAsyncExecution::ThreadPool, [&Making]() { AirSky::MakeFragments(Making.Rgba); });
		}
	}
	static const std::vector<uint8_t>& WaitSheet()
	{
		BeginSheet();
		FSheetMaking& Making = SheetMaking();
		if (Making.Done.IsValid())
		{
			Making.Done.Wait();
		}
		return Making.Rgba;
	}

	/** The painter's four numbers a point (AirSky::Painter::SetSprites) as the mesh's two texture coordinates: the
	 * point's place on the sheet, and how much of the sun is behind the eye and ahead. Count points; any the painter
	 * gave none, at the sheet's corner (no cloud there). */
	static void SheetPlaces(const std::vector<float>& From, TArray<FVector2D>& Places, TArray<FVector2D>& Ways, int32 Count)
	{
		Places.SetNumUninitialized(Count);
		Ways.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const size_t At = static_cast<size_t>(Index) * 4;
			if (At + 3 < From.size())
			{
				Places[Index] = FVector2D(From[At], From[At + 1]);
				Ways[Index] = FVector2D(From[At + 2], From[At + 3]);
			}
			else
			{
				Places[Index] = FVector2D::ZeroVector;
				Ways[Index] = FVector2D::ZeroVector;
			}
		}
	}

	/** The effects kit's nation rows (us, gb, de, it, jp, su, agreed with the effects thread 17:34Z) from a
	 * recording's nation (USA, UK, GER, ITA, JAP, SOV): empty for any other, which the kit draws in its default. */
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

	static std::string Narrow(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
	}

	/**
	 * The effects kit's calls that came after unreal a7f7f7a (the effects thread, kit heads d359a18 and 1295edc): made
	 * where the kit has them, so the scene builds on either kit. With the newer kit, an air scene draws its guns, hits
	 * and flames out to 4 km, and a burning aircraft's flame is sized to its span.
	 */
	template <typename TKit>
	static void UseAirDistance(TKit* Kit)
	{
		if constexpr (requires(TKit* K) { K->UseAirDistance(); })
		{
			Kit->UseAirDistance();
		}
	}

	template <typename TKit>
	static void TrailStart(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres)
	{
		if constexpr (requires(TKit* K) { K->TrailStart(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f); })
		{
			Kit->TrailStart(Now, Id, At, Velocity, Kind, SpanMetres);
		}
		else
		{
			Kit->TrailStart(Now, Id, At, Velocity, Kind);
		}
	}

	/** A gun firing, with its aircraft's nation where the kit takes it (the art director's G2, 15:38Z: Allied streams
	 * red-orange, German and Italian pale yellow-white, Japanese near white, Soviet green; the effects thread's call,
	 * the nation last, as KitNation names it: us, gb, de, it, jp, su). An older kit draws its one colour. */
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

	/** The kit from 1295edc (TrailStart with a span) keeps every flame on a full screen and sheds the farthest smoke
	 * first, so the scene may light 24 trails at once on it; 10 on an older kit (the pre-check, 06:08Z). */
	template <typename TKit>
	static constexpr bool HasTrailSpan(const TKit*)
	{
		return requires(TKit* K) { K->TrailStart(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f); };
	}

	/** The table's put-away (hook lines for CampaignMapActor in Battle/AIR-SCENE.md): while it is set, every piece stays
	 * off the table whatever the hover asks, and the table is busy, so the hover falls quiet. False where the map has
	 * none yet; the scene then keeps the pieces hidden itself. */
	template <typename TMap>
	static bool PutAway(TMap* Map, bool bAway)
	{
		if constexpr (requires(TMap* M) { M->SetPutAway(true); })
		{
			if (Map)
			{
				Map->SetPutAway(bAway);
				return true;
			}
		}
		return false;
	}

	/** The air battle's haze colour (the fog's inscattering, set by HideTheTable; the haze band takes it too). */
	static const FLinearColor AirHazeInk(0.55f, 0.66f, 0.78f);

	/** A haze's colour, by the name this engine gives it. */
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

	/** The kit's falling debris (kit head 44af184): whether this kit has it. */
	template <typename TKit>
	static constexpr bool HasAirFall(const TKit*)
	{
		return requires(TKit* K) { K->AirFall(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0); };
	}

	/** A lost aircraft's debris row: once as it falls (Kind explode, burn or spin; DeckZ the world height where its
	 * pieces leave sight), then every frame of a burning or spinning fall, then when the model is gone. */
	template <typename TKit>
	static void AirFall(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity, const FString& Kind, float SpanMetres, double DeckZ)
	{
		if constexpr (requires(TKit* K) { K->AirFall(0.0, 0, FVector::ZeroVector, FVector::ZeroVector, FString(), 0.0f, 0.0); })
		{
			Kit->AirFall(Now, Id, At, Velocity, Kind, SpanMetres, DeckZ);
		}
	}

	template <typename TKit>
	static void AirFallMove(TKit* Kit, double Now, int32 Id, const FVector& At, const FVector& Velocity)
	{
		if constexpr (requires(TKit* K) { K->AirFallMove(0.0, 0, FVector::ZeroVector, FVector::ZeroVector); })
		{
			Kit->AirFallMove(Now, Id, At, Velocity);
		}
	}

	template <typename TKit>
	static void AirFallStop(TKit* Kit, int32 Id)
	{
		if constexpr (requires(TKit* K) { K->AirFallStop(0); })
		{
			Kit->AirFallStop(Id);
		}
	}

	/** The way the campaign hides its own frame while a battle is up (CampaignHUD's ShowFrame, a hook line the 3D
	 * builder adds: see Battle/AIR-SCENE.md). Without it the frame stays under the battle's screen. */
	template <typename THud>
	static void ShowFrame(THud* Hud, bool bShow)
	{
		if constexpr (requires(THud* H) { H->ShowFrame(true); })
		{
			if (Hud)
			{
				Hud->ShowFrame(bShow);
			}
		}
	}

	/** A number from the mix, at a path of fields ("air.flak.soundEvery"), or Default. */
	static double MixNumber(const TSharedPtr<FJsonObject>& Mix, const FString& Path, double Default)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (int32 K = 0; At.IsValid() && K < Steps.Num(); ++K)
		{
			if (K == Steps.Num() - 1)
			{
				double Value = Default;
				return At->TryGetNumberField(Steps[K], Value) ? Value : Default;
			}
			const TSharedPtr<FJsonObject>* Next = nullptr;
			At = At->TryGetObjectField(Steps[K], Next) ? *Next : nullptr;
		}
		return Default;
	}

	static TSharedPtr<FJsonObject> MixObject(const TSharedPtr<FJsonObject>& Mix, const FString& Path)
	{
		TArray<FString> Steps;
		Path.ParseIntoArray(Steps, TEXT("."));
		TSharedPtr<FJsonObject> At = Mix;
		for (const FString& Step : Steps)
		{
			const TSharedPtr<FJsonObject>* Next = nullptr;
			if (!At.IsValid() || !At->TryGetObjectField(Step, Next))
			{
				return nullptr;
			}
			At = *Next;
		}
		return At;
	}

	/** A string, or one of a list of strings (picked by Pick), at a field of an object of the mix. */
	static FString MixTake(const TSharedPtr<FJsonObject>& Object, const FString& Field, int32 Pick = 0)
	{
		if (!Object.IsValid())
		{
			return FString();
		}
		FString One;
		if (Object->TryGetStringField(Field, One))
		{
			return One;
		}
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Object->TryGetArrayField(Field, List) && List->Num() > 0)
		{
			return (*List)[FMath::Abs(Pick) % List->Num()]->AsString();
		}
		return FString();
	}

	static double Smooth(double Real, double Seconds)
	{
		return 1.0 - FMath::Exp(-Real / FMath::Max(0.01, Seconds));
	}

	// The paint of the small things: a canopy's silk, an airman, torn skin, a bomb's casing.
	static const FLinearColor Silk(0.93f, 0.92f, 0.88f);
	static const FLinearColor Airman(0.16f, 0.14f, 0.11f);
	static const FLinearColor Skin(0.48f, 0.49f, 0.47f);
	static const FLinearColor Scorched(0.07f, 0.065f, 0.06f);
	static const FLinearColor Casing(0.12f, 0.13f, 0.12f);
}

AAirBattle::AAirBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

bool AAirBattle::WantsReplay(FString& OutKey)
{
	return FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattle="), OutKey) && !OutKey.IsEmpty();
}

void AAirBattle::ReplayWhenReady(const FString& Key)
{
	PendingReplay = Key;
	SetActorTickEnabled(true);
}

bool AAirBattle::BeginReplay(const FString& Key)
{
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattleShots="), ShotFolder);
	// (the builder's film of a picture run: "-TheaterAirBattleRideHold=40" stays on each ride 40 s after its picture)
	float Hold = 0.5f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirBattleRideHold="), Hold);
	RideHold = FMath::Clamp(static_cast<double>(Hold), 0.5, 300.0);
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/AirBattles") / Key + TEXT(".json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: no recorded battle %s (is Content/Data/AirBattles staged with the build?)"), *File);
		if (!ShotFolder.IsEmpty())
		{
			QuitAt = 0.0;
			SetActorTickEnabled(true);
		}
		return false;
	}
	SkyFinish(false); // (a turn of the clouds painted aside on a battle begun before: done before its state goes)
	State = MakeShared<FAirBattleState>();
	std::string Error;
	if (!AirFeed::ReadRecording(AirParts::Narrow(Text), State->Recording, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: %s: %s"), *File, *AirParts::Wide(Error));
		State.Reset();
		if (!ShotFolder.IsEmpty())
		{
			QuitAt = 0.0;
			SetActorTickEnabled(true);
		}
		return false;
	}
	FAirBattleState& S = *State;
	S.bReplay = true;
	S.Board = S.Recording.Board;
	S.Show = AirShow::Show(S.Recording.Seed ? S.Recording.Seed : 238);
	AirFeed::Apply(S.Board, S.Show);
	S.SkyPlace = AirFeed::PlacingOf(S.Board, S.Recording.Views.front().Groups, S.Recording.Seed);
	S.Desk.Begin(S.Board, S.Recording.Views.front().Head.Me);
	S.Card = S.Desk.CardOf(S.Recording.Card);
	S.BattleT = S.Recording.Views.front().Head.T;
	return Open(Key);
}

bool AAirBattle::BeginLive(FCampaignSession* InSession, const FString& Kind, int32 Target, const TArray<int32>& Ids)
{
	Session = InSession;
	if (!Session)
	{
		return false;
	}
	TArray<FString> Numbers;
	for (const int32 Id : Ids)
	{
		Numbers.Add(FString::FromInt(Id));
	}
	FString Reply, Error;
	if (!Session->CallBattle(TEXT("airStart"), FString::Printf(TEXT("[\"%s\",%d,[%s]]"), *Kind, Target, *FString::Join(Numbers, TEXT(","))), Reply, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.airStart failed: %s"), *Error);
		return false;
	}
	SkyFinish(false); // (a turn of the clouds painted aside on a battle begun before: done before its state goes)
	State = MakeShared<FAirBattleState>();
	FAirBattleState& S = *State;
	AirFeed::Json Opened;
	std::string Why;
	AirFeed::Frame First;
	if (!AirFeed::Parse(AirParts::Narrow(Reply), Opened, Why) || !AirFeed::ReadSky(Opened["sky"], S.Board) || !AirFeed::ReadView(Opened["view"], S.Board, First))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.airStart's answer could not be read (%s)"), *AirParts::Wide(Why));
		State.Reset();
		return false;
	}
	S.bReplay = false;
	AirFeed::Apply(S.Board, S.Show);
	S.SkyPlace = AirFeed::PlacingOf(S.Board, First.Groups, 0);
	S.Desk.Begin(S.Board, First.Head.Me);
	S.Show.Take(First.Head.T, First.Groups, First.Events, First.Ships);
	S.Desk.Take(First);
	S.BattleT = S.FedT = First.Head.T;
	// (the bridge's battle opens paused: it is run here a battle second at a time, ahead of what is shown)
	FString Ignored;
	Session->CallBattle(TEXT("battleSpeed"), TEXT("[1]"), Ignored, Error);
	if (!Open(TEXT("live")))
	{
		return false;
	}
	if (Stage && !First.ShipRows.empty())
	{
		TArray<FStageShip> Ships;
		for (const AirFeed::ShipRow& Row : First.ShipRows)
		{
			FStageShip& Ship = Ships.AddDefaulted_GetRef();
			Ship.Id = Row.Id;
			Ship.Class = AirParts::Wide(Row.Class);
			Ship.Nation = AirParts::Wide(Row.Nation);
			Ship.Name = AirParts::Wide(Row.Name);
			Ship.At = FVector2D(Row.X, Row.Y);
			Ship.Face = Row.Face;
			Ship.bDock = Row.bDock;
		}
		Stage->SetShips(Ships);
	}
	return true;
}

bool AAirBattle::Open(const FString& Why)
{
	UWorld* Here = GetWorld();
	if (!Here || !State.IsValid())
	{
		return false;
	}
	FAirBattleState& S = *State;
	// (the sheet of cloud shapes begun on the pool now, for LaySky; once a run)
	AirParts::BeginSheet();
	// The mix (the sound director's mix.json, carried in Content/Data/Sound): the scene reads its numbers, never copies them.
	{
		FString MixText;
		if (FFileHelper::LoadFileToString(MixText, *(FPaths::ProjectContentDir() / TEXT("Data/Sound/mix.json"))))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MixText), Mix);
		}
		if (!Mix.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: Content/Data/Sound/mix.json is missing: the battle is silent."));
		}
	}
	// The paint: the campaign's own surface, coloured by each point's colour, as the stage's ships are.
	{
		UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_MapSurface.M_MapSurface"));
		if (!Surface)
		{
			Surface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		auto Ink = [this, Surface](float Roughness, float Specular)
		{
			UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(Surface, this);
			Made->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Made->SetScalarParameterValue(TEXT("Specular"), Specular);
			Made->SetScalarParameterValue(TEXT("Grain"), 0.0f);
			Made->SetScalarParameterValue(TEXT("Tooth"), 0.0f);
			return Made;
		};
		PlaneInk = Ink(0.5f, 0.45f);
		PlainInk = Ink(0.85f, 0.2f);
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
		{
			DiscInk = UMaterialInstanceDynamic::Create(Glow, this);
			DiscInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		}
		float Turns = 1.9f;
		FParse::Value(FCommandLine::Get(), TEXT("TheaterScrewTurns="), Turns);
		ScrewTurns = FMath::Clamp(static_cast<double>(Turns), 0.0, 30.0);
	}
	// The effects: the kit's own, drawn out to the air distance, on the air clock (its mesh, the one it hangs on Root,
	// drawn behind the clouds while the eye is under them: DrawSky).
	const TArray<TObjectPtr<USceneComponent>> BeforeAir = Root->GetAttachChildren();
	Effects = MakeUnique<FBattleEffects>();
	if (!Effects->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
		{
			return FVector(Point.X, Point.Y, Stage && Stage->IsRaised() ? Stage->GroundZ(Point) : 0.0);
		}))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: the effects kit did not begin (its recipe or material is missing): no guns, smoke or flak are drawn."));
		Effects.Reset();
	}
	else
	{
		for (const TObjectPtr<USceneComponent>& Child : Root->GetAttachChildren())
		{
			UProceduralMeshComponent* Found = Cast<UProceduralMeshComponent>(Child.Get());
			if (Found && !BeforeAir.Contains(Child))
			{
				AirFxMesh = Found;
			}
		}
		AirParts::UseAirDistance(Effects.Get());
		S.Trails.Most = AirParts::HasTrailSpan(Effects.Get()) ? 24 : 10;
		Effects->SetWind(WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0) * (1.0 / 100.0));
		// The ground's own kit (bomb bursts, crashes, splashes, fires, the stage's oil), so the clouds can lie over it
		// while the air fight lies over them: its mesh is the one it hangs on Root.
		TArray<TObjectPtr<USceneComponent>> Before = Root->GetAttachChildren();
		GroundFx = MakeUnique<FBattleEffects>();
		if (GroundFx->Begin(Root, UnitsPerMetre, [this](const FVector& Point)
			{
				return FVector(Point.X, Point.Y, Stage && Stage->IsRaised() ? Stage->GroundZ(Point) : 0.0);
			}))
		{
			AirParts::UseAirDistance(GroundFx.Get());
			GroundFx->SetWind(WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0) * (1.0 / 100.0));
			for (const TObjectPtr<USceneComponent>& Child : Root->GetAttachChildren())
			{
				UProceduralMeshComponent* Found = Cast<UProceduralMeshComponent>(Child.Get());
				if (Found && !Before.Contains(Child))
				{
					GroundFxMesh = Found;
				}
			}
		}
		else
		{
			GroundFx.Reset();
		}
	}
	Raise();
	// The flight core's ground: the stage's hills; with no stage the board's own land, as LayGround lays it (the
	// pre-check on round 14: Bremen's bombs had burst as water columns); else the sea.
	if (Stage && Stage->IsRaised())
	{
		TWeakObjectPtr<AAirStage> Under(Stage);
		const double U = UnitsPerMetre;
		S.Show.Ground = [Under, U](double X, double Y)
		{
			return Under.IsValid() ? FMath::Max(0.0, Under->GroundZ(FVector(Y * U, X * U, 0.0)) / U) : 0.0;
		};
	}
	else
	{
		const std::shared_ptr<const AirGround::BoardLand> Land = std::make_shared<const AirGround::BoardLand>(S.Board.Land);
		if (!Land->IsEmpty())
		{
			S.Show.Ground = [Land](double X, double Y) { return Land->At(X, Y); };
			S.SkyPlace.GroundTop = AirGround::BoardLand::LandHigh;
		}
	}
	LaySky();
	LayZenith();
	LayHazeBand();
	Small = NewObject<UProceduralMeshComponent>(this, TEXT("AirSmallThings"));
	Small->SetupAttachment(Root);
	Small->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Small->SetCastShadow(false);
	Small->RegisterComponent();
	HideTheTable();
	Eye = Here->SpawnActor<ACameraActor>();
	if (Eye)
	{
		Eye->GetCameraComponent()->SetFieldOfView(55.0f);
		// (the whole window, whatever its shape: a camera actor keeps 16:9 and bars the rest)
		Eye->GetCameraComponent()->bConstrainAspectRatio = false;
		Eye->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Eye->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		// (the 3D builder, 19 November 2026: the art director's battle grade, on battle cameras only)
		BattleLook::Grade(Eye->GetCameraComponent()->PostProcessSettings);
	}
	{
		// The scene's own drawing size, for timing (build 1734: on Brian's 2881 by 1786 window the scene is drawn at the
		// campaign map's budget, some 1077 by 668, and filled out 2.7 times each way by Unreal's upscaler, which is the
		// softness of the Midway ships picture; the words are drawn at every pixel, which is why they stay sharp).
		// "-TheaterAirSize=85" draws the scene at 85 hundredths of the window's points each way while the battle
		// stands; without it the map's budget holds. The size it had comes back when the scene ends.
		int32 AirSize = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterAirSize="), AirSize) && AirSize >= 40 && AirSize <= 100)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
			{
				KeptScreenPercent = Variable->GetFloat();
				Variable->Set(static_cast<float>(AirSize), ECVF_SetByCode);
				UE_LOG(LogTheater, Display, TEXT("Air battle: the scene drawn at %d hundredths of the window's points (the map's %.0f)"), AirSize, KeptScreenPercent);
			}
		}
	}
	if (APlayerController* Controller = Here->GetFirstPlayerController())
	{
		if (Eye)
		{
			Controller->SetViewTarget(Eye);
		}
		AirParts::ShowFrame(Cast<ACampaignHUD>(Controller->GetHUD()), false);
	}
	// The camera starts behind his own raid, looking along its way.
	for (const auto& Pair : S.Show.Groups())
	{
		if (Pair.second.Side == S.Desk.MySideIndex() && !Pair.second.bFighter)
		{
			Yaw = FMath::RadiansToDegrees(Pair.second.Face);
			break;
		}
	}
	// The picture run: the art director's frames, then quit.
	if (!ShotFolder.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotFolder, true);
		Shots = {
			{ TEXT("01-approach"), -12.0, false, false, false },   // (seconds of real time before the formations meet)
			{ TEXT("02-the-meeting"), 6.0, false, false, false },  // (after they meet)
			{ TEXT("03-ride-a-fighter"), 22.0, true, false, false },
			{ TEXT("04-the-thick-of-it"), 45.0, false, false, false },
			{ TEXT("05-ride-again"), 70.0, true, false, false, true }, // (riding the other side)
			{ TEXT("06-the-ships"), 5.0, false, true, false },     // (after the first ship goes down)
			{ TEXT("07-the-fight-goes-on"), 120.0, false, false, false },
		};
	}
	ShowScreen();
	bBegun = true;
	SetActorTickEnabled(true);
	UE_LOG(LogTheater, Display, TEXT("Air battle: %s, %s (%s), %d views%s."), *Why, *AirParts::Wide(S.Board.Title), *AirParts::Wide(S.Board.Date),
		S.bReplay ? static_cast<int32>(S.Recording.Views.size()) : 1, ShotFolder.IsEmpty() ? TEXT("") : *(TEXT(", pictures into ") + ShotFolder));
	return true;
}

void AAirBattle::Raise()
{
	FAirBattleState& S = *State;
	if (!S.Board.StageKey.empty())
	{
		Stage = GetWorld()->SpawnActor<AAirStage>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Stage && Stage->Raise(AirParts::Wide(S.Board.StageKey), FVector2D(S.Board.StageAimX, S.Board.StageAimY), S.Board.StageTurn,
			FVector2D(S.Board.TargetX, S.Board.TargetY), UnitsPerMetre, GroundFx ? GroundFx.Get() : Effects.Get()))
		{
			if (Effects)
			{
				Effects->SetSun(Stage->TowardSun());
			}
			if (GroundFx)
			{
				GroundFx->SetSun(Stage->TowardSun());
			}
			// The ships as the battle lays them (the first view that carries them).
			if (S.bReplay)
			{
				for (const AirFeed::Frame& View : S.Recording.Views)
				{
					if (View.ShipRows.empty())
					{
						continue;
					}
					TArray<FStageShip> Ships;
					for (const AirFeed::ShipRow& Row : View.ShipRows)
					{
						FStageShip& Ship = Ships.AddDefaulted_GetRef();
						Ship.Id = Row.Id;
						Ship.Class = AirParts::Wide(Row.Class);
						Ship.Nation = AirParts::Wide(Row.Nation);
						Ship.Name = AirParts::Wide(Row.Name);
						Ship.At = FVector2D(Row.X, Row.Y);
						Ship.Face = Row.Face;
						Ship.bDock = Row.bDock;
					}
					Stage->SetShips(Ships);
					break;
				}
			}
			return;
		}
		UE_LOG(LogTheater, Warning, TEXT("Air battle: the stage %s could not be raised; the plain sea stands in."), *AirParts::Wide(S.Board.StageKey));
		if (Stage)
		{
			Stage->Destroy();
			Stage = nullptr;
		}
	}
	LayGround();
	// (the hour's sun: mid-morning, from the south-east, as the campaign's daylight raids)
	if (Effects)
	{
		Effects->SetSun(FRotator(30.0, 150.0, 0.0).Vector());
	}
	if (GroundFx)
	{
		GroundFx->SetSun(FRotator(30.0, 150.0, 0.0).Vector());
	}
}

void AAirBattle::LayGround()
{
	// A battle with no stage: the sea far past the board, the board's land laid flat on it (place.ground.land, rings of
	// board points, LandHigh over the sea: the flight core's ground there is the same, AirGround::BoardLand), and the
	// target's outline. The land is cut into triangles by ear clipping.
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	auto Sheet = [this](const TCHAR* Name, const TArray<FVector>& Points, const TArray<int32>& Triangles, const FColor& Paint)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		TArray<FVector> Normals;
		TArray<FColor> Colors;
		Normals.Init(FVector::UpVector, Points.Num());
		Colors.Init(Paint, Points.Num());
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, PlainInk);
		Ground.Add(Mesh);
	};
	// (each triangle the way round that faces up, as FMiniatureMesh lays them)
	auto Up = [](const TArray<FVector>& V, int32 A, int32 B, int32 C, TArray<int32>& Out)
	{
		const FVector Facing = (V[B] - V[C]) ^ (V[A] - V[C]);
		Out.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, C } : TArray<int32>{ A, C, B });
	};
	{
		const double Far = 300000.0 * U;
		const FVector Middle = World(S.Board.TargetX, S.Board.TargetY, 0.0);
		const TArray<FVector> V = { Middle + FVector(-Far, -Far, 0.0), Middle + FVector(Far, -Far, 0.0), Middle + FVector(Far, Far, 0.0), Middle + FVector(-Far, Far, 0.0) };
		TArray<int32> T;
		Up(V, 0, 1, 2, T);
		Up(V, 0, 2, 3, T);
		Sheet(TEXT("AirSea"), V, T, FColor(38, 62, 78));
	}
	int32 Ring = 0;
	for (const std::vector<std::pair<double, double>>& Land : S.Board.Land)
	{
		if (Land.size() < 3)
		{
			continue;
		}
		TArray<FVector> V;
		TArray<FVector2D> P;
		for (const std::pair<double, double>& Point : Land)
		{
			V.Add(World(Point.first, Point.second, AirGround::BoardLand::LandHigh));
			P.Add(FVector2D(Point.first, Point.second));
		}
		// Ear clipping: the ring's corners taken off one at a time where the triangle they make holds no other corner.
		double Area = 0.0;
		for (int32 K = 0; K < P.Num(); ++K)
		{
			const FVector2D& A = P[K];
			const FVector2D& B = P[(K + 1) % P.Num()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		const double Turn = Area >= 0.0 ? 1.0 : -1.0;
		TArray<int32> Left;
		for (int32 K = 0; K < P.Num(); ++K)
		{
			Left.Add(K);
		}
		TArray<int32> T;
		int32 Guard = 0;
		while (Left.Num() > 3 && Guard++ < 100000)
		{
			bool bCut = false;
			for (int32 K = 0; K < Left.Num(); ++K)
			{
				const int32 IA = Left[(K + Left.Num() - 1) % Left.Num()], IB = Left[K], IC = Left[(K + 1) % Left.Num()];
				const FVector2D A = P[IA], B = P[IB], C = P[IC];
				const double Cross = ((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)) * Turn;
				if (Cross <= 0.0)
				{
					continue;
				}
				bool bInside = false;
				for (const int32 IO : Left)
				{
					if (IO == IA || IO == IB || IO == IC)
					{
						continue;
					}
					const FVector2D O = P[IO];
					const double D1 = ((B.X - A.X) * (O.Y - A.Y) - (B.Y - A.Y) * (O.X - A.X)) * Turn;
					const double D2 = ((C.X - B.X) * (O.Y - B.Y) - (C.Y - B.Y) * (O.X - B.X)) * Turn;
					const double D3 = ((A.X - C.X) * (O.Y - C.Y) - (A.Y - C.Y) * (O.X - C.X)) * Turn;
					if (D1 >= 0.0 && D2 >= 0.0 && D3 >= 0.0)
					{
						bInside = true;
						break;
					}
				}
				if (bInside)
				{
					continue;
				}
				Up(V, IA, IB, IC, T);
				Left.RemoveAt(K);
				bCut = true;
				break;
			}
			if (!bCut)
			{
				break;
			}
		}
		if (Left.Num() == 3)
		{
			Up(V, Left[0], Left[1], Left[2], T);
		}
		Sheet(*FString::Printf(TEXT("AirLand_%d"), Ring++), V, T, FColor(92, 98, 70));
	}
	// The ground's pictures, for a raid over land with no set piece: northern Europe's country (not a desert's, nor a
	// Japanese-held target's, where the plain sheets stay; nor a set piece's that could not be raised).
	if (S.Board.StageKey.empty() && !S.Board.Land.empty() && S.Board.Terrain != "desert" && S.Board.DefNat != "JAP" && S.Board.DefNat != "JPN")
	{
		PaintLand();
	}
}

void AAirBattle::PaintLand()
{
	// The ground's pictures (Battle/AirLand): the board's land and rivers, the month, true north on the board, the
	// target's town plan from the book (Content/Data/AirGround/towns.json; a town not in it is made up), the scene's
	// sun for the baked shadows. Painted on the pool's threads; the plain sheets stand until LayLand lays them.
	const FAirBattleState& S = *State;
	AirLand::Place P;
	P.AimX = S.Board.TargetX;
	P.AimY = S.Board.TargetY;
	P.NorthX = S.Board.NorthX;
	P.NorthY = S.Board.NorthY;
	P.Terrain = S.Board.Terrain.empty() ? std::string("plains") : S.Board.Terrain;
	P.Name = S.Board.PlaceName;
	P.Month = S.Board.Month + 1; // (the feed's months count from 0)
	P.Seed = S.Recording.Seed ? S.Recording.Seed : 238;
	const FVector Sun = FRotator(30.0, 150.0, 0.0).Vector();
	P.SunX = Sun.Y;
	P.SunY = Sun.X;
	P.SunZ = Sun.Z;
	P.Land = S.Board.Land;
	P.Rivers = S.Board.Rivers;
	FString Book;
	FFileHelper::LoadFileToString(Book, *(FPaths::ProjectContentDir() / TEXT("Data/AirGround/towns.json")));
	const std::string Plans = TCHAR_TO_UTF8(*Book);
	if (!S.Board.bNorthGiven)
	{
		UE_LOG(LogTheater, Log, TEXT("Air battle ground: the recording gives no true north (place.ground.north); the town plan is laid with north up the board."));
	}
	LandMade = MakeShared<FAirLandMade, ESPMode::ThreadSafe>();
	LandAsked = FPlatformTime::Seconds();
	LandPainting = Async(EAsyncExecution::ThreadPool, [Made = LandMade, P, Plans]()
	{
		const double Began = FPlatformTime::Seconds();
		Made->Painter = std::make_unique<AirLand::Painter>(P, Plans);
		Made->Frames = AirLand::Frames(P);
		Made->Levels.SetNum(static_cast<int32>(Made->Frames.size()));
		for (int32 K = 0; K < Made->Levels.Num(); ++K)
		{
			const AirLand::Frame& F = Made->Frames[static_cast<size_t>(K)];
			std::vector<uint8_t> Rgba(static_cast<size_t>(F.W) * F.H * 4);
			std::vector<float> Heights(static_cast<size_t>(F.W) * F.H);
			// (in bands of 64 rows, every band on a thread of its own; the shadows when every row's heights are in)
			const int32 Bands = (F.H + 63) / 64;
			const AirLand::Painter& Painter = *Made->Painter;
			ParallelFor(Bands, [&](int32 B) { Painter.PaintRows(F, B * 64, (B + 1) * 64, Rgba.data(), Heights.data()); });
			if (AirLand::Painter::Shades(F))
			{
				ParallelFor(Bands, [&](int32 B) { Painter.ShadeRows(F, B * 64, (B + 1) * 64, Rgba.data(), Heights.data()); });
			}
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(F.W * F.H);
			for (int32 I = 0; I < Pixels.Num(); ++I)
			{
				const uint8_t* C = &Rgba[static_cast<size_t>(I) * 4];
				Pixels[I] = FColor(C[0], C[1], C[2], 255);
			}
			CampaignTextures::MakeLevels(F.W, F.H, Pixels, true, Made->Levels[K]);
		}
		Made->Seconds = FPlatformTime::Seconds() - Began;
	});
	// (a picture run waits for the ground, so that every picture has it)
	if (!ShotFolder.IsEmpty())
	{
		LandPainting.Wait();
		LandPainting = TFuture<void>();
		LayLand();
	}
}

void AAirBattle::LayLand()
{
	if (!LandMade.IsValid() || !LandMade->Painter || LandMade->Levels.Num() != static_cast<int32>(LandMade->Frames.size()))
	{
		return;
	}
	FAirLandMade& Made = *LandMade;
	const FAirBattleState& S = *State;
	UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Flag.M_Flag"));
	if (!Cloth)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle ground: M_Flag is missing; the plain ground stays."));
		LandMade.Reset();
		return;
	}
	// (the cloth of a standard, its picture the ground, with no light of its own: lit by the sun as the land is)
	auto InkOf = [this, Cloth](UTexture2D* Picture)
	{
		UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Cloth, this);
		Ink->SetTextureParameterValue(TEXT("Flag"), Picture);
		Ink->SetScalarParameterValue(TEXT("Glow"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Fade"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Drain"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Veiled"), 0.0f);
		Ink->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
		LandInks.Add(Ink);
		return Ink;
	};
	const AirGround::BoardLand Heights(S.Board.Land);
	auto High = [&Heights](double X, double Y) { return Heights.IsEmpty() ? AirGround::BoardLand::LandHigh : Heights.At(X, Y); };
	auto Up = [](const TArray<FVector>& V, int32 A, int32 B, int32 C, TArray<int32>& Out)
	{
		const FVector Facing = (V[B] - V[C]) ^ (V[A] - V[C]);
		Out.Append(Facing.Z >= 0.0 ? TArray<int32>{ A, B, C } : TArray<int32>{ A, C, B });
	};
	auto Sheet = [this](const FString& Name, const TArray<FVector>& Points, const TArray<int32>& Triangles, const TArray<FVector2D>& Uv,
	                    const TArray<FColor>& Colours, UMaterialInterface* Ink)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, *Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		Mesh->CreateMeshSection(0, Points, Triangles, Normals, Uv, Colours, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, Ink);
		return Mesh;
	};
	TArray<TObjectPtr<UProceduralMeshComponent>> Laid;
	// Each picture a grid of flat cells (the coast's 2 m step kept at the cells' corners), less the cells a finer picture
	// covers: the pictures are laid out so that a finer one's edges fall on the coarser one's cells.
	const double Cells[3] = { 512.0, 2048.0, 8192.0 };
	for (int32 K = 0; K < Made.Levels.Num(); ++K)
	{
		const AirLand::Frame& F = Made.Frames[static_cast<size_t>(K)];
		UTexture2D* Picture = CampaignTextures::CreateFromLevels(Made.Levels[K], false, MakeUniqueObjectName(this, UTexture2D::StaticClass(), *FString::Printf(TEXT("AirLandPicture_%d"), K)));
		if (!Picture)
		{
			continue;
		}
		LandPictures.Add(Picture);
		const double Cell = Cells[FMath::Min(K, 2)];
		const int32 NX = FMath::Max(1, FMath::RoundToInt((F.X1() - F.X0) / Cell)), NY = FMath::Max(1, FMath::RoundToInt((F.Y1() - F.Y0) / Cell));
		TArray<FVector> V;
		TArray<FVector2D> Uv;
		TArray<FColor> Colours;
		for (int32 J = 0; J <= NY; ++J)
		{
			for (int32 I = 0; I <= NX; ++I)
			{
				const double X = F.X0 + (F.X1() - F.X0) * I / NX, Y = F.Y0 + (F.Y1() - F.Y0) * J / NY;
				V.Add(World(X, Y, High(X, Y)));
				Uv.Add(FVector2D((X - F.X0) / (F.X1() - F.X0), (F.Y1() - Y) / (F.Y1() - F.Y0)));
				Colours.Add(FColor::White);
			}
		}
		const AirLand::Frame* Finer = K > 0 ? &Made.Frames[static_cast<size_t>(K - 1)] : nullptr;
		TArray<int32> T;
		for (int32 J = 0; J < NY; ++J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const double CX = F.X0 + (F.X1() - F.X0) * (I + 0.5) / NX, CY = F.Y0 + (F.Y1() - F.Y0) * (J + 0.5) / NY;
				if (Finer && Finer->Holds(CX, CY))
				{
					continue;
				}
				const int32 A = J * (NX + 1) + I;
				Up(V, A, A + 1, A + NX + 2, T);
				Up(V, A, A + NX + 2, A + NX + 1, T);
			}
		}
		Laid.Add(Sheet(FString::Printf(TEXT("AirLandSheet_%d"), K), V, T, Uv, Colours, InkOf(Picture)));
	}
	// Past the far picture to the sea sheet's rim: the colour of the far picture's edge, carried out (each point of the
	// edge the mean of the texels by it, the rim the mean of its whole side), on a picture of plain white.
	if (Made.Levels.Num() == 3 && LandPictures.Num() == 3)
	{
		const AirLand::Frame& F = Made.Frames[2];
		const TArray<FColor>& Px = Made.Levels[2].Each[0];
		auto Linear = [](uint8 C) { const double V = C / 255.0; return V <= 0.04045 ? V / 12.92 : FMath::Pow((V + 0.055) / 1.055, 2.4); };
		// (the mean of a box of texels, as linear light, by the pixel's column and row)
		auto Mean = [&](int32 I0, int32 J0, int32 I1, int32 J1, double Out[3])
		{
			Out[0] = Out[1] = Out[2] = 0.0;
			int32 N = 0;
			for (int32 J = FMath::Max(0, J0); J <= FMath::Min(F.H - 1, J1); ++J)
			{
				for (int32 I = FMath::Max(0, I0); I <= FMath::Min(F.W - 1, I1); ++I)
				{
					const FColor& C = Px[J * F.W + I];
					Out[0] += Linear(C.R);
					Out[1] += Linear(C.G);
					Out[2] += Linear(C.B);
					++N;
				}
			}
			for (int32 C = 0; C < 3; ++C)
			{
				Out[C] /= FMath::Max(1, N);
			}
		};
		auto Colour = [](const double L[3]) { return FColor(static_cast<uint8>(FMath::Clamp(L[0] * 255.0 + 0.5, 0.0, 255.0)), static_cast<uint8>(FMath::Clamp(L[1] * 255.0 + 0.5, 0.0, 255.0)), static_cast<uint8>(FMath::Clamp(L[2] * 255.0 + 0.5, 0.0, 255.0)), 255); };
		TArray<FColor> White;
		White.Init(FColor::White, 16);
		UTexture2D* Plain = CampaignTextures::Create(4, 4, White, true, false, MakeUniqueObjectName(this, UTexture2D::StaticClass(), TEXT("AirLandPlain")));
		const double Rim = 300000.0;
		const int32 Steps = 24;
		TArray<FVector> V;
		TArray<FVector2D> Uv;
		TArray<FColor> Colours;
		TArray<int32> T;
		// (the four sides, each from its corner to the next going round: its points along the edge, and out at the rim)
		const double Corners[5][2] = { { F.X0, F.Y0 }, { F.X1(), F.Y0 }, { F.X1(), F.Y1() }, { F.X0, F.Y1() }, { F.X0, F.Y0 } };
		const double MX = (F.X0 + F.X1()) * 0.5, MY = (F.Y0 + F.Y1()) * 0.5;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const double AX = Corners[Side][0], AY = Corners[Side][1], BX = Corners[Side + 1][0], BY = Corners[Side + 1][1];
			double Whole[3];
			auto TexelOf = [&F](double X, double Y, int32& I, int32& J) { I = FMath::Clamp(static_cast<int32>((X - F.X0) / F.Step), 0, F.W - 1); J = FMath::Clamp(static_cast<int32>((F.Y1() - Y) / F.Step), 0, F.H - 1); };
			{
				int32 IA, JA, IB, JB;
				TexelOf(AX, AY, IA, JA);
				TexelOf(BX, BY, IB, JB);
				Mean(FMath::Min(IA, IB) - 8, FMath::Min(JA, JB) - 8, FMath::Max(IA, IB) + 8, FMath::Max(JA, JB) + 8, Whole);
			}
			const int32 First = V.Num();
			for (int32 Q = 0; Q <= Steps; ++Q)
			{
				const double X = AX + (BX - AX) * Q / Steps, Y = AY + (BY - AY) * Q / Steps;
				int32 I, J;
				TexelOf(X, Y, I, J);
				double Near[3];
				Mean(I - 24, J - 24, I + 24, J + 24, Near);
				V.Add(World(X, Y, High(X, Y)));
				Uv.Add(FVector2D(0.5, 0.5));
				Colours.Add(Colour(Near));
				// (out from the picture's middle through the edge's point, to the rim's square)
				const double DX = X - MX, DY = Y - MY;
				const double Reach = Rim / FMath::Max(FMath::Abs(DX), FMath::Abs(DY));
				V.Add(World(MX + DX * Reach, MY + DY * Reach, 0.0));
				Uv.Add(FVector2D(0.5, 0.5));
				Colours.Add(Colour(Whole));
			}
			for (int32 Q = 0; Q < Steps; ++Q)
			{
				const int32 A = First + Q * 2;
				Up(V, A, A + 2, A + 3, T);
				Up(V, A, A + 3, A + 1, T);
			}
		}
		if (Plain)
		{
			LandPictures.Add(Plain);
			Laid.Add(Sheet(TEXT("AirLandRim"), V, T, Uv, Colours, InkOf(Plain)));
		}
	}
	// The plain sheets out of sight; the pictures' in their place.
	for (UProceduralMeshComponent* Old : Ground)
	{
		if (Old)
		{
			Old->SetVisibility(false);
		}
	}
	Ground.Append(Laid);
	const AirLand::Tally& Count = Made.Painter->Laid();
	UE_LOG(LogTheater, Log, TEXT("Air battle ground: %s (plan %s), %d villages, %d country roads, %d bridges, %d spires%s; painted aside in %.2f s, laid %.2f s after it was asked for."),
		*AirParts::Wide(S.Board.PlaceName), *AirParts::Wide(Count.Plan), Count.Villages, Count.Links, Count.Bridges, Count.Spires, Count.bCoast ? TEXT(", the coast") : TEXT(""),
		Made.Seconds, FPlatformTime::Seconds() - LandAsked);
	// (the pictures are on the drawing thread now: the copies here are let go, all but the near picture's own texels
	// and the painter, for the craters)
	if (Made.Levels.Num() > 0 && Made.Levels[0].Each.Num() > 0 && LandPictures.Num() > 0)
	{
		LandNear = MoveTemp(Made.Levels[0].Each[0]);
	}
	Made.Levels.Empty();
}

void AAirBattle::ScarLand(double X, double Y, double Kilograms)
{
	// (the pictures first: while the pool still paints, it is writing the painter and the frames)
	if (LandPictures.Num() == 0 || !LandMade.IsValid() || !LandMade->Painter || LandMade->Frames.empty())
	{
		return;
	}
	const AirLand::Frame& F = LandMade->Frames[0];
	if (!F.Holds(X, Y) || LandNear.Num() != F.W * F.H)
	{
		return;
	}
	int Box[4] = { 1, 0, 0, 0 };
	LandMade->Painter->Scar(F, X, Y, Kilograms, reinterpret_cast<uint8_t*>(LandNear.GetData()), true, Box);
	if (Box[0] <= Box[2] && Box[1] <= Box[3])
	{
		// (the picture and its three smaller copies, down to 64 m a pixel: above that a crater is lost in its pixel)
		CampaignTextures::UpdateBoxAll(LandPictures[0], LandNear, Box[0], Box[1], Box[2], Box[3], 3);
	}
}

void AAirBattle::HideTheTable()
{
	// The table and its armies are put out of sight; the air and the light are the battle's, by day. What the table
	// had is kept first, and put back by ShowTheTable (the pre-check, 06:08Z: the lamp's turn and strength and the
	// haze stayed the battle's after CONTINUE, since the room sets some of them only once).
	if (bTableHidden)
	{
		return;
	}
	bTableHidden = true;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (ACampaignMapActor* Map = Cast<ACampaignMapActor>(*It))
		{
			Table = Map;
			AirParts::PutAway(Map, true);
		}
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			if (It->IsA<AArmyPiece>() && !It->IsHidden())
			{
				KeptPieces.Add(*It);
			}
			It->SetActorHiddenInGame(true);
		}
		if (ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It))
		{
			KeptRooms.Add(Environment);
			Environment->SetActorTickEnabled(false);
			// Haze from 2 km (the art director, 19 November 04:40Z): a clear sky over the sea, the far formations soft.
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
				Kept.Colour = AirParts::HazeColour(Fog, FLinearColor(0.80f, 0.84f, 0.88f));
				// (thick low down and thin high up: looking down from bomber height the sea stays dark under the bright
				// cloud floor, the art director's 19 November 12:59Z; the horizon and the low air far off soft. The sky's
				// pictures, Tools/air_show/air_sky_picture.cpp, take the same numbers; in the game the battle look then
				// sets its own haze from these, CampaignBattleLook.cpp's AirHaze, which the pictures do not take)
				Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));
				Fog->SetFogDensity(0.00045f);
				Fog->SetFogHeightFalloff(0.006f);
				Fog->SetFogMaxOpacity(0.8f);
				Fog->SetFogInscatteringColor(AirParts::AirHazeInk);
				// (the 3D builder, 19 November 2026: the art director's ruling on the haze, thin under the eye)
				BattleLook::AirHaze(*Fog, UnitsPerMetre);
			}
			// The lamps: the key turned to the battle's sun, every lamp at its day strength and white (a battle opened
			// in the table's night kept the moon's), the sky's light the day's.
			TArray<UDirectionalLightComponent*> Lights;
			Environment->GetComponents(Lights);
			UDirectionalLightComponent* Key = nullptr;
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
				if (Light->GetFName() == TEXT("KeyLight") || (!Key && Lights.Num() == 1))
				{
					Key = Light;
				}
			}
			if (!Key)
			{
				for (UDirectionalLightComponent* Light : Lights)
				{
					if (!Key || Light->Intensity > Key->Intensity)
					{
						Key = Light;
					}
				}
			}
			if (Key)
			{
				BattleKey = Key;
				const FVector Sun = Stage && Stage->IsRaised() ? Stage->TowardSun() : FRotator(30.0, 150.0, 0.0).Vector();
				Key->SetWorldRotation((-Sun).Rotation());
				// (the 3D builder, 19 November 2026: a sky behind the battle, lit by its sun; taken away in ShowTheTable)
				BattleLook::AirSkyBegin(*this, Key);
				TheSun(*Key);
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
	}
	LightTheBattle();
}

void AAirBattle::TheSun(UDirectionalLightComponent& Key)
{
	// The sun a hard small disc (the art director on build 1750's Midway 05, 19 November 2026: a core of 25 pixels
	// or less, the glow back to the sky's colour within about 150). The table's lamp is a sun a degree and a half
	// wide, kept soft for the table's shadows, and the sky drew all of its light into that disc: some two thousand
	// times the picture's white, which the picture's bloom spread into a glow 700 pixels wide. The bloom spreads a
	// share of what the disc holds, however wide the disc is drawn, so here the disc holds six thousandths of it
	// (still near a hundred times the white, so a hard white disc, its glow under a hundredth of the white 150 pixels
	// out) and is drawn the true sun's half degree wide (eleven pixels across the middle of the close camera's 62
	// degrees). And the air's haze throws the sun's light forward at 0.75: the engine's 0.8 made a bright ring some
	// twelve degrees wide round the sun (before the disc held its six thousandths), and at a half the glow spread so
	// broad that, with the sun just out of the picture, it washed half the sky white (the art director on build 1760's
	// Midway 05: real haze is 0.75 to 0.8, which keeps the bright patch round the sun and the rest of the sky its
	// blue-grey). Game-wide curve and bloom stay as they are.
	// "-TheaterSun=0.53,0.006,0.75" (the disc's width in degrees, the share of the light it holds, how steeply forward
	// the haze throws the sun's light); each one given takes the place of its number here.
	double DiscWide = 0.53, DiscShare = 0.006, HazeForward = 0.75;
	FString SunAsked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSun="), SunAsked, false))
	{
		TArray<FString> Parts;
		SunAsked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &DiscWide, &DiscShare, &HazeForward };
		for (int32 K = 0; K < Parts.Num() && K < 3; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	DiscWide = FMath::Clamp(DiscWide, 0.1, 5.0);
	DiscShare = FMath::Clamp(DiscShare, 0.0, 1.0);
	HazeForward = FMath::Clamp(HazeForward, 0.0, 0.999);
	Key.LightSourceAngle = static_cast<float>(DiscWide);
	Key.AtmosphereSunDiskColorScale = FLinearColor(static_cast<float>(DiscShare), static_cast<float>(DiscShare), static_cast<float>(DiscShare));
	Key.MarkRenderStateDirty();
	// (The battle's sky is the battle look's, made on this actor in AirSkyBegin and taken away with it.)
	TArray<USkyAtmosphereComponent*> Atmospheres;
	GetComponents(Atmospheres);
	for (USkyAtmosphereComponent* Atmosphere : Atmospheres)
	{
		Atmosphere->SetMieAnisotropy(static_cast<float>(HazeForward));
	}
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sun a disc %.2f degrees wide holding %.4f of its light, the haze throwing it forward at %.2f (%d skies)"),
		DiscWide, DiscShare, HazeForward, Atmospheres.Num());
}

void AAirBattle::LightTheBattle()
{
	// The battle's day: every lamp white, the key (turned to the battle's sun) at the table's day strength and warmth
	// and the others at the fill's, the sky's light the day's. Said again every frame where anything differs: the room
	// can still light itself for the hour under the battle (a showpiece's picture retaken, a portrait, an answer
	// changing the season), and the battle's light must stand (the review, 06:50Z).
	for (const FKeptLamp& Kept : KeptLamps)
	{
		UDirectionalLightComponent* Light = Kept.Lamp.Get();
		if (!Light)
		{
			continue;
		}
		const bool bKey = Light == BattleKey.Get();
		const float Want = bKey ? 7.0f : 0.75f;
		if (!Light->GetLightColor().Equals(FLinearColor::White, 0.002f))
		{
			Light->SetLightColor(FLinearColor::White);
		}
		if (!FMath::IsNearlyEqual(Light->Intensity, Want, 0.001f))
		{
			Light->SetIntensity(Want);
		}
		if (bKey && !FMath::IsNearlyEqual(Light->Temperature, 4900.0f, 1.0f))
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
}

void AAirBattle::KeepTheTableHidden()
{
	// (the campaign's controller still ticks under the battle, before it: when the province under the pointer changes,
	// the map unfolds its crowds and stands every piece not folded into one, at the world's origin where the battle
	// stands. Until the map's put-away hook lines are in, each such piece is put away again before the frame is drawn)
	if (!bTableHidden)
	{
		return;
	}
	for (TActorIterator<AArmyPiece> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			It->SetActorHiddenInGame(true);
		}
	}
	if (Table.IsValid() && !Table->IsHidden())
	{
		Table->SetActorHiddenInGame(true);
	}
	LightTheBattle();
}

void AAirBattle::ShowTheTable()
{
	// The table as it was: its haze, its lamps, its own tick, and its pieces stood again with the crowds folded up.
	if (!bTableHidden)
	{
		return;
	}
	bTableHidden = false;
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
	BattleLook::AirSkyEnd(); // (the 3D builder, 19 November 2026: the battle's sky goes with the battle)
	for (const FKeptLamp& Kept : KeptLamps)
	{
		if (UDirectionalLightComponent* Light = Kept.Lamp.Get())
		{
			Light->SetRelativeRotation(Kept.Rotation);
			Light->SetIntensity(Kept.Intensity);
			Light->SetLightColor(Kept.Colour);
			if (!FMath::IsNearlyEqual(Light->Temperature, Kept.Temperature, 1.0f))
			{
				Light->SetTemperature(Kept.Temperature);
			}
			if (!FMath::IsNearlyEqual(Light->LightSourceAngle, Kept.SourceAngle) || !Light->AtmosphereSunDiskColorScale.Equals(Kept.SunDisc))
			{
				Light->LightSourceAngle = Kept.SourceAngle;
				Light->AtmosphereSunDiskColorScale = Kept.SunDisc;
				Light->MarkRenderStateDirty();
			}
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
	if (ACampaignMapActor* Map = Table.Get())
	{
		Map->SetActorHiddenInGame(false);
		// (the map's own way of standing its pieces again, which folds the crowds up: its put-away where the hook lines
		// are in, else its bare-ground switch off and on)
		if (!AirParts::PutAway(Map, false))
		{
			Map->SetPiecesShown(false);
			Map->SetPiecesShown(true);
		}
	}
	else
	{
		for (const TWeakObjectPtr<AActor>& Piece : KeptPieces)
		{
			if (Piece.IsValid())
			{
				Piece->SetActorHiddenInGame(false);
			}
		}
	}
	BattleKey.Reset();
	KeptHazes.Reset();
	KeptLamps.Reset();
	KeptSkies.Reset();
	KeptRooms.Reset();
	KeptPieces.Reset();
}

void AAirBattle::LaySky()
{
	// The day's clouds (AirSky, the art director's sky at bomber height): laid once on a square that repeats round the
	// eye, so there are clouds wherever the fight goes, lifted over the stage's hills where they are drawn; once they
	// are drawn, the flight core is told where the deck is, so that falls begun above it pass out of sight in it (where
	// a tile of it is drawn, and nowhere else: with no clouds drawn, every fall goes on down).
	SkyFinish(false); // (a turn painted aside reads the field laid again here)
	FAirBattleState& S = *State;
	S.Show.CloudTop = 0.0;
	S.Show.DeckAt = nullptr;
	S.Show.DeckReach = 0.0;
	const FVector Sun = Stage && Stage->IsRaised() ? Stage->TowardSun() : FRotator(30.0, 150.0, 0.0).Vector();
	S.SkyPlace.Sun = AirSky::Vec(Sun.Y, Sun.X, Sun.Z);
	S.SkyPlace.Ground = S.Show.Ground;
	S.Sky = AirSky::Lay(S.Board.Cloud, S.SkyPlace);
	// (the painter's own field and ground, for DrawSky's turns on the pool's threads: nothing they read is the frame's)
	S.SkyAside = S.Sky;
	// (the stage's ground copied for the pool; the board's own land reads nothing it changes, so it is handed on as it is)
	S.SkyAside.Ground = Stage && Stage->IsRaised() ? (S.Show.Ground ? Stage->GroundAside() : std::function<double(double, double)>()) : S.Show.Ground;
	const FString Moved = S.Sky.MovedWhy.empty() ? FString() : FString::Printf(TEXT(", moved %.0f m %s"), S.Sky.Moved, *AirParts::Wide(S.Sky.MovedWhy));
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sky, %s%s from %.0f to %.0f m%s; %d clouds of %d puffs, %.0f%% of the sky."),
		S.Board.Cloud.bGiven ? TEXT("") : TEXT("(none given) "), S.Sky.Sky.bDeck ? TEXT("a deck") : TEXT("scattered cumulus"), S.Sky.Base, S.Sky.Top,
		*Moved, static_cast<int32>(S.Sky.Clouds.size()), static_cast<int32>(S.Sky.Puffs.size()), S.Sky.Cover * 100.0);
	if (S.Sky.Puffs.empty())
	{
		return;
	}
	UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Highlight)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Highlight is missing (run Tools/build_content.sh): no clouds are drawn, and falls go on down."));
		return;
	}
	// The puffs drawn from the sheet of cloud shapes (the art director's brief, item 4: torn fragments of our own in
	// place of soft discs) by M_Cloud, which reads the sheet ("Sheet") by the points' first texture coordinate and the
	// sun's way on them by their second (Tools/build_content.py); without M_Cloud (the content not built again yet), the
	// soft discs on M_Highlight, as before. The shadows on the sea stay soft discs on M_Highlight.
	UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Cloud.M_Cloud"));
	if (CloudMaterial && !CloudSheet)
	{
		const std::vector<uint8_t>& Rgba = AirParts::WaitSheet();
		const int32 Side = AirSky::FragmentCells * AirSky::FragmentSize;
		if (Rgba.size() == static_cast<size_t>(Side) * static_cast<size_t>(Side) * 4)
		{
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(Side * Side);
			for (int32 Index = 0; Index < Side * Side; ++Index)
			{
				const size_t C = static_cast<size_t>(Index) * 4;
				Pixels[Index] = FColor(Rgba[C], Rgba[C + 1], Rgba[C + 2], Rgba[C + 3]);
			}
			// (plain numbers, not colours: twice each is the share of the cloud's light; the edge held, not repeated)
			CloudSheet = CampaignTextures::Create(Side, Side, Pixels, false, false, FName(TEXT("AirCloudSheet")));
		}
	}
	if (!CloudSheet)
	{
		if (CloudMaterial)
		{
			UE_LOG(LogTheater, Warning, TEXT("Air battle: the sheet of cloud shapes was not made: the clouds are soft discs."));
		}
		CloudMaterial = nullptr;
	}
	else if (!CloudMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Cloud is missing (run Tools/build_content.sh): the clouds are soft discs, not the sheet's shapes."));
	}
	bCloudShapes = CloudMaterial != nullptr;
	S.SkyRooms.SetSprites(bCloudShapes);
	Clouds = NewObject<UProceduralMeshComponent>(this, TEXT("AirClouds"));
	Clouds->bUseAsyncCooking = false;
	Clouds->SetupAttachment(Root);
	Clouds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Clouds->SetCastShadow(false);
	Clouds->RegisterComponent();
	CloudInk = UMaterialInstanceDynamic::Create(bCloudShapes ? CloudMaterial : Highlight, this);
	if (bCloudShapes)
	{
		CloudInk->SetTextureParameterValue(TEXT("Sheet"), CloudSheet);
	}
	CloudShadowInk = UMaterialInstanceDynamic::Create(Highlight, this);
	// (the clouds' sunlit tops a third brighter than their paint: on build 1732, through the battle grade, they read 226
	// to 231 at the 99.5th percentile of the picture, the art director's mark 238 to 245 with no more than about one
	// per cent of it at 250 or over (16:21Z). The paint's top, AirSky.cpp's SunSide, is already 252, so the lift is the
	// ink's; the painter paints the shaded sides, the bases and the shadows that much darker, so that they stand where
	// they were (her 16:31Z). "-TheaterCloudBright=1.2" tries another)
	float CloudBright = 1.3f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterCloudBright="), CloudBright);
	CloudBright = FMath::Clamp(CloudBright, 0.5f, 3.0f);
	CloudInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	CloudShadowInk->SetScalarParameterValue(TEXT("Brightness"), CloudBright);
	S.SkyRooms.SetInk(CloudBright);
	CloudShadows = NewObject<UProceduralMeshComponent>(this, TEXT("AirCloudShadows"));
	CloudShadows->bUseAsyncCooking = false;
	CloudShadows->SetupAttachment(Root);
	CloudShadows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudShadows->SetCastShadow(false);
	// (under everything see-through, whichever way DrawSky turns the rest: the air kit is at 0 or -30, the clouds at -10
	// or -20, the ground's kit at -20 or -10)
	CloudShadows->SetTranslucentSortPriority(-40);
	CloudShadows->RegisterComponent();
	// (the near clouds' own mesh, drawn after the far ones: DrawSky sets its order with theirs)
	NearClouds = NewObject<UProceduralMeshComponent>(this, TEXT("AirNearClouds"));
	NearClouds->bUseAsyncCooking = false;
	NearClouds->SetupAttachment(Root);
	NearClouds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NearClouds->SetCastShadow(false);
	NearClouds->RegisterComponent();
	SkyOrder = 0;
	SkyNearTurns = 0;
	SkyBothTurns = 0;
	SkyAtOnce = 0;
	SkyLate = 0;
	SkyPaintSeconds = 0.0;
	SkySendSeconds = 0.0;
	SkySlowest = 0.0;
	SkyClockAt = -1.0;
	SkyEyeWas = FVector(0.0, 0.0, -1e9);
	SkyStepWas = 0.0;
	bSkyAtOnce = FParse::Param(FCommandLine::Get(), TEXT("TheaterSkyAtOnce"));
	// The first turn of the puffs, to an eye behind the raid; after that DrawSky turns them as the eye moves.
	const AirSky::Vec Drift = S.Sky.DriftAt(S.Show.AirNow());
	const AirSky::Rgb Light = S.Board.bNight ? AirSky::Rgb(0.12f, 0.13f, 0.17f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
	S.SkyRooms.SetSweep(AirSky::Vec(0.0, 0.0, 0.0)); // (a sky laid again keeps no sweep from the last one's turns)
	S.SkyRooms.PaintBoth(S.SkyAside, AirSky::Vec(S.Board.TargetX, S.Board.TargetY - 6000.0, 5000.0), Drift, S.SkyPlace.Sun, Light);
	SkyLine = S.SkyRooms.Line();
	// Each room's mesh, its slots fixed (each keeps its triangles, only its points move as the eye does).
	auto Make = [this, &Drift](UProceduralMeshComponent* Mesh, const AirSky::Painter& Brush, int32 Room, const std::vector<AirSky::Vec>& From, const std::vector<uint8_t>& FromRgba,
		TArray<FVector>& Points, TArray<FColor>& Colours, const std::vector<float>* FromUvs, TArray<FVector2D>* Places, TArray<FVector2D>* Ways, UMaterialInterface* Ink)
	{
		const std::vector<int> Made = Brush.Triangles(Room);
		TArray<int32> Triangles;
		Triangles.Reserve(static_cast<int32>(Made.size()));
		for (const int Index : Made)
		{
			Triangles.Add(Index);
		}
		Points.SetNum(static_cast<int32>(From.size()));
		Colours.SetNum(static_cast<int32>(From.size()));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const AirSky::Vec& P = From[static_cast<size_t>(Index)];
			Points[Index] = World(P.X, P.Y, P.Z);
			const size_t C = static_cast<size_t>(Index) * 4;
			Colours[Index] = FColor(FromRgba[C], FromRgba[C + 1], FromRgba[C + 2], FromRgba[C + 3]); // (linear already, as the kit's)
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		Mesh->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
		if (Places && Ways && FromUvs)
		{
			// (the shapes: each point's place on the sheet, and the sun's way on it, behind the eye and ahead)
			AirParts::SheetPlaces(*FromUvs, *Places, *Ways, Points.Num());
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, *Places, *Ways, TArray<FVector2D>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		else
		{
			Mesh->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		}
		Mesh->SetMaterial(0, Ink);
	};
	AirSky::Rooms& SkyRooms = S.SkyRooms;
	const bool bShapes = bCloudShapes;
	Make(Clouds, SkyRooms.Far, SkyRooms.Far.RoomFor(S.SkyAside), SkyRooms.FarPoints, SkyRooms.FarRgba, CloudPoints, CloudColours, bShapes ? &SkyRooms.FarUvs : nullptr,
		bShapes ? &CloudPlaces : nullptr, bShapes ? &CloudWays : nullptr, CloudInk);
	Make(NearClouds, SkyRooms.Near, SkyRooms.NearRoom(), SkyRooms.NearPoints, SkyRooms.NearRgba, NearCloudPoints, NearCloudColours, bShapes ? &SkyRooms.NearUvs : nullptr,
		bShapes ? &NearCloudPlaces : nullptr, bShapes ? &NearCloudWays : nullptr, CloudInk);
	Make(CloudShadows, SkyRooms.Far, AirSky::Painter::MostShadows, SkyRooms.Shadows, SkyRooms.ShadowRgba, ShadowPoints, ShadowColours, nullptr, nullptr, nullptr, CloudShadowInk);
	// (drawn: now the deck is where falls pass out of sight; the field outlives the flight core's hold on it, both
	// being the battle state's)
	if (S.Sky.DeckTop > 0.0)
	{
		const AirSky::Field* Laid = &S.Sky;
		S.Show.CloudTop = S.Sky.DeckTop;
		S.Show.DeckAt = [Laid](double X, double Y)
		{
			AirShow::DeckSpan Here;
			Here.Hide = Laid->DeckAt(X, Y, &Here.Foot);
			return Here;
		};
		// (drawn, more than half, within five sixths of the clouds' reach from the eye: past it, falls go on down)
		S.Show.DeckReach = AirSky::Reach * 5.0 / 6.0;
		// (no tile's foot under the laid base: under it, under the deck for good)
		S.Show.DeckBase = S.Sky.Base;
	}
}

void AAirBattle::LayZenith()
{
	FAirBattleState& S = *State;
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterZenith="), Asked, false))
	{
		TArray<FString> Parts;
		Asked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &ZenithStrength, &ZenithFrom };
		for (int32 K = 0; K < Parts.Num() && K < 2; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	ZenithStrength = FMath::Clamp(ZenithStrength, 0.0, 3.0);
	ZenithFrom = FMath::Clamp(ZenithFrom, 0.0, 80.0);
	// (by night the sky is the night's; with the battle's own sky left out, the engine's is not lifted)
	if (S.Board.bNight || ZenithStrength <= 0.0 || FParse::Param(FCommandLine::Get(), TEXT("TheaterNoAirSky")))
	{
		return;
	}
	UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Glow.M_Glow"));
	if (!GlowMaterial)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Glow is missing (run Tools/build_content.sh): the sky looking up is the engine's alone."));
		return;
	}
	// A bowl 9 km round the eye, inside the haze's start (15 km), so no haze lies on it, and past every cloud and
	// aircraft near enough to matter: rings from ZenithFrom up, 48 a ring. Its colour is the lift's tint, its alpha
	// how much of it: none at ZenithFrom, all of it at the zenith, rising as the sky's own blue thins (the eye's path
	// through the air up there is shortest). The tint from the battle's grade worked back (contrast 1.42 about 0.18,
	// red 1.08 and blue 0.87, colour kept 0.55 to 0.72): to bring the zenith from (22, 27, 37) to about (55, 105, 185)
	// it wants about 0.01, 0.09 and 0.35 more light in red, green and blue, before the grade.
	// (The art director on build 1780, 20 November 2026: at 0.3 from 25 degrees the overhead read 43 to 50 at 7,500 m,
	// still a dusk; her mark stands at 90 to 110. Worked back through the grade from those two readings, 29 with none
	// and about 46 at 0.3, the mark wants some four times the light: 1.3, from 12 degrees so the blue begins lower.
	// The Mac's picture run tries 0.9, 1.3 and 1.8 for her to pick from.)
	const double Radius = 9000.0 * UnitsPerMetre;
	const int32 Around = 48;
	const double Rings[] = { 0.0, 0.08, 0.18, 0.3, 0.45, 0.6, 0.75, 0.88, 1.0 };
	const int32 RingCount = UE_ARRAY_COUNT(Rings);
	const double Low = FMath::DegreesToRadians(ZenithFrom);
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colours;
	for (int32 R = 0; R < RingCount; ++R)
	{
		const double Up = Low + (UE_DOUBLE_HALF_PI - Low) * Rings[R];
		const double Rise = (FMath::Sin(Up) - FMath::Sin(Low)) / FMath::Max(1e-6, 1.0 - FMath::Sin(Low));
		const uint8 Share = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(255.0 * FMath::Pow(FMath::Clamp(Rise, 0.0, 1.0), 1.5)), 0, 255));
		const int32 Count = R == RingCount - 1 ? 1 : Around;
		for (int32 K = 0; K < Count; ++K)
		{
			const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
			const FVector Way(FMath::Cos(Up) * FMath::Cos(Round), FMath::Cos(Up) * FMath::Sin(Round), FMath::Sin(Up));
			Points.Add(Way * Radius);
			Normals.Add(-Way);
			Colours.Add(FColor(6, 64, 255, Share));
		}
	}
	for (int32 R = 0; R + 1 < RingCount; ++R)
	{
		const int32 Here = R * Around;
		const int32 Next = (R + 1) * Around;
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 K2 = (K + 1) % Around;
			if (R + 1 == RingCount - 1)
			{
				Triangles.Append({ Here + K, Here + K2, Next });
			}
			else
			{
				Triangles.Append({ Here + K, Here + K2, Next + K2, Here + K, Next + K2, Next + K });
			}
		}
	}
	Zenith = NewObject<UProceduralMeshComponent>(this, TEXT("AirZenith"));
	Zenith->bUseAsyncCooking = false;
	Zenith->SetupAttachment(Root);
	Zenith->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zenith->SetCastShadow(false);
	// (behind every other see-through thing: the cloud shadows are at -40, the clouds and kits from -30 up)
	Zenith->SetTranslucentSortPriority(-50);
	Zenith->RegisterComponent();
	Zenith->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
	ZenithInk = UMaterialInstanceDynamic::Create(GlowMaterial, this);
	ZenithInk->SetScalarParameterValue(TEXT("Strength"), 0.0f);
	Zenith->SetMaterial(0, ZenithInk);
	ZenithShown = -1.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: the sky looking up lifted toward a deep blue, %.2f at 7,500 m, from %.0f degrees up."), ZenithStrength, ZenithFrom);
}

void AAirBattle::ZenithAt(const FVector& EyeWorld)
{
	if (!Zenith || !ZenithInk)
	{
		return;
	}
	Zenith->SetWorldLocation(EyeWorld);
	// (the air over the eye thins as it climbs, the engine's sky looking up darkening with it: the low haze keeps the
	// sky light below about 1,000 m (it thins by half every 1,200 m or so), so the lift is none there, rising evenly to
	// all of ZenithStrength at a B-17's 7,500 m, a little more higher still)
	const double High = FMath::Max(0.0, EyeWorld.Z / UnitsPerMetre);
	const double Want = ZenithStrength * FMath::Clamp((High - 1000.0) / 6500.0, 0.0, 1.2);
	if (FMath::Abs(Want - ZenithShown) > 0.002)
	{
		ZenithShown = Want;
		ZenithInk->SetScalarParameterValue(TEXT("Strength"), static_cast<float>(Want));
	}
}

void AAirBattle::LayHazeBand()
{
	FAirBattleState& S = *State;
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterHazeBand="), Asked, false))
	{
		TArray<FString> Parts;
		Asked.ParseIntoArray(Parts, TEXT(","), false);
		double* const Into[] = { &HazeBandStrength, &HazeBandFrom, &HazeBandFull };
		for (int32 K = 0; K < Parts.Num() && K < 3; ++K)
		{
			if (!Parts[K].TrimStartAndEnd().IsEmpty())
			{
				*Into[K] = FCString::Atod(*Parts[K]);
			}
		}
	}
	HazeBandStrength = FMath::Clamp(HazeBandStrength, 0.0, 3.0);
	HazeBandFrom = FMath::Max(0.0, HazeBandFrom);
	HazeBandFull = FMath::Max(HazeBandFrom + 1.0, HazeBandFull);
	// (the sea's square: the stage's, 200 km each way from the board's target (AirStage::BuildSea), or with no stage
	// the scene's own, 300 km (LayGround); both centred on the target)
	SeaMiddle = World(S.Board.TargetX, S.Board.TargetY, 0.0);
	SeaHalf = (Stage && Stage->IsRaised() ? 200000.0 : 300000.0) * UnitsPerMetre;
	if (S.Board.bNight || HazeBandStrength <= 0.0)
	{
		return;
	}
	UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight"));
	if (!Ink)
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: M_Highlight is missing (run Tools/build_content.sh): no haze band over the horizon."));
		return;
	}
	HazeBand = NewObject<UProceduralMeshComponent>(this, TEXT("AirHazeBand"));
	HazeBand->bUseAsyncCooking = false;
	HazeBand->SetupAttachment(Root);
	HazeBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HazeBand->SetCastShadow(false);
	// (behind every other see-through thing but the zenith's bowl, -50: the cloud shadows are at -40, the clouds and
	// kits from -30 up, so a cloud far out still draws over it)
	HazeBand->SetTranslucentSortPriority(-48);
	HazeBand->RegisterComponent();
	HazeBandInk = UMaterialInstanceDynamic::Create(Ink, this);
	HazeBandInk->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
	HazeBandInk->SetVectorParameterValue(TEXT("Tint"), AirParts::AirHazeInk * static_cast<float>(HazeBandStrength));
	HazeBandInk->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
	HazeBandLaidAt = FVector(1e30, 1e30, 1e30);
	HazeBandShown = -1.0;
	UE_LOG(LogTheater, Display, TEXT("Air battle: a haze band over the sea's far edge (the sea %.0f km each way), %.2f of the haze's brightness, from an eye %.0f m up, all of it from %.0f m."),
		SeaHalf / UnitsPerMetre / 1000.0, HazeBandStrength, HazeBandFrom, HazeBandFull);
}

void AAirBattle::HazeBandAt(const FVector& EyeWorld)
{
	if (!HazeBand || !HazeBandInk)
	{
		return;
	}
	const double U = UnitsPerMetre;
	const double High = EyeWorld.Z / U;
	const double Want = FMath::Clamp((High - HazeBandFrom) / (HazeBandFull - HazeBandFrom), 0.0, 1.0);
	if (FMath::Abs(Want - HazeBandShown) > 0.004)
	{
		HazeBandShown = Want;
		HazeBandInk->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(Want));
		HazeBand->SetVisibility(Want > 0.0);
	}
	if (Want <= 0.0)
	{
		return;
	}
	// (laid again when the eye has gone 250 m along or 100 m up or down: the edge's angle under the eye moves little)
	const FVector Moved = EyeWorld - HazeBandLaidAt;
	if (FVector2D(Moved.X, Moved.Y).Size() < 250.0 * U && FMath::Abs(Moved.Z) < 100.0 * U)
	{
		return;
	}
	HazeBandLaidAt = EyeWorld;
	HazeBand->SetWorldLocation(FVector(EyeWorld.X, EyeWorld.Y, 0.0));
	// Column by column round the eye: how far the sea's edge lies that way, and so how far under the level it shows
	// (Edge). The band stands at 0.6 of that distance, from the sea (Foot, under the edge, none of it there) up through
	// the edge (all of it from just under it to just over it) and thinning to none 7 degrees above it.
	struct FRow
	{
		double Over;  // degrees over the edge's angle (the first two rows are placed by the foot instead)
		uint8 Share;  // how much of the band
	};
	static const FRow Rows[] = { { 0.0, 0 }, { 0.0, 150 }, { -0.1, 255 }, { 0.4, 255 }, { 1.2, 180 }, { 2.2, 108 }, { 3.4, 52 }, { 5.0, 16 }, { 7.0, 0 } };
	const int32 RowCount = UE_ARRAY_COUNT(Rows);
	const int32 Around = 64;
	const double Ex = EyeWorld.X - SeaMiddle.X, Ey = EyeWorld.Y - SeaMiddle.Y;
	TArray<FVector> Points;
	TArray<FColor> Colours;
	Points.Reserve(Around * RowCount);
	Colours.Reserve(Around * RowCount);
	for (int32 K = 0; K < Around; ++K)
	{
		const double Round = 2.0 * UE_DOUBLE_PI * K / Around;
		const double Dx = FMath::Cos(Round), Dy = FMath::Sin(Round);
		// (the way out of the square from the eye: the nearer of its two sides that way)
		double Out = 1e18;
		if (FMath::Abs(Dx) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dx > 0.0 ? SeaHalf : -SeaHalf) - Ex) / Dx);
		}
		if (FMath::Abs(Dy) > 1e-9)
		{
			Out = FMath::Min(Out, ((Dy > 0.0 ? SeaHalf : -SeaHalf) - Ey) / Dy);
		}
		const double Far = FMath::Max(Out, 20000.0 * U) / U; // metres
		const double Stand = 0.6 * Far;
		const double Edge = -FMath::Atan2(FMath::Max(High, 1.0), Far);
		const double Foot = -FMath::Atan2(FMath::Max(High, 1.0), Stand);
		for (int32 R = 0; R < RowCount; ++R)
		{
			double Angle = Edge + FMath::DegreesToRadians(Rows[R].Over);
			if (R == 0)
			{
				Angle = Foot;
			}
			else if (R == 1)
			{
				Angle = 0.5 * (Foot + Edge + FMath::DegreesToRadians(Rows[2].Over));
			}
			const double Z = FMath::Max(0.0, High + Stand * FMath::Tan(Angle));
			Points.Add(FVector(Dx * Stand * U, Dy * Stand * U, Z * U));
			Colours.Add(FColor(255, 255, 255, Rows[R].Share));
		}
	}
	if (HazeBand->GetNumSections() == 0)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(Around * (RowCount - 1) * 12);
		for (int32 K = 0; K < Around; ++K)
		{
			const int32 K2 = (K + 1) % Around;
			for (int32 R = 0; R + 1 < RowCount; ++R)
			{
				const int32 A = K * RowCount + R, B = K2 * RowCount + R, C = K2 * RowCount + R + 1, D = K * RowCount + R + 1;
				// (both ways round: the band is seen from inside, whichever way the engine takes as its face)
				Triangles.Append({ A, B, C, A, C, D, A, C, B, A, D, C });
			}
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Points.Num());
		HazeBand->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
		HazeBand->SetMaterial(0, HazeBandInk);
	}
	else
	{
		HazeBand->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
	}
}

void AAirBattle::DrawSky()
{
	if (!Clouds || !Eye || !State.IsValid())
	{
		return;
	}
	FAirBattleState& S = *State;
	// The wind's carry (the battle's wind, as the effects kit is given it), on the air clock, so cumulus drift as the
	// smoke does (a deck stays where it was laid).
	const AirSky::Vec Drift = S.Sky.DriftAt(S.Show.AirNow());
	Clouds->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	if (NearClouds)
	{
		NearClouds->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	}
	if (CloudShadows)
	{
		CloudShadows->SetRelativeLocation(WorldVector(Drift.X, Drift.Y, 0.0));
	}
	const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
	const AirSky::Vec Seen(At.Y, At.X, At.Z);
	// See-through things are drawn one after another, not by depth. Seen from over the clouds' base, the ground's bursts
	// and fires lie under every cloud between, and the air fight over them; seen from under, the air fight (all of it
	// over the base: what is under it is the ground kit's) lies behind every cloud between, a gap showing it, and the
	// ground's over both (the pre-check, 19 November 13:07Z: tracer, flak and smoke showed in front of the clouds that
	// hid their aircraft). The base under the eye is the one the kits are split at (KitAt), with 50 m either side
	// before the order turns, so it does not flicker.
	const double UnderEye = S.Show.Ground ? S.Show.Ground(Seen.X, Seen.Y) : 0.0;
	const double EyeBase = FMath::Max(S.Sky.Base, UnderEye + 300.0) + (SkyOrder == 1 ? -50.0 : SkyOrder == 2 ? 50.0 : 0.0);
	const int32 Want = Seen.Z > EyeBase ? 1 : 2;
	if (Want != SkyOrder)
	{
		SkyOrder = Want;
		Clouds->SetTranslucentSortPriority(Want == 1 ? -10 : -20);
		// (the near clouds just after the far ones, before the kit drawn over them)
		if (NearClouds)
		{
			NearClouds->SetTranslucentSortPriority(Want == 1 ? -5 : -15);
		}
		if (GroundFxMesh.IsValid())
		{
			GroundFxMesh->SetTranslucentSortPriority(Want == 1 ? -20 : -10);
		}
		if (AirFxMesh.IsValid())
		{
			AirFxMesh->SetTranslucentSortPriority(Want == 1 ? 0 : -30);
		}
	}
	// Turned to the eye as far as it has moved: the near room as often as its puffs' turn shows, the far room (most of
	// the points) and the shadows only when the eye has moved far enough for a far puff's turn to show (AirSky::Rooms).
	// Before, all of it, 132,000 points, was sent 9 to 16 times a second: the builder's Mac run of 19 November, Midway's
	// stutter.
	// And painted aside (round 11): this thread asks the rooms what the eye's move needs (Wants); the painting, and the
	// points made over for the engine (SkyFill), are done on the engine's pool of threads while the frame goes on, and
	// the frame they are done this thread sends them (SkySend), all it does of a turn now. A far turn painted here was
	// most of Midway's slowest frame each five seconds (build 1758: 24 ms at the middle, 35 at the ninetieth). A turn
	// so shows a frame or a few late, a steady lag of the eye's move (2.5 m a frame at 150 m/s); when the eye jumps more
	// than SkyCut in a frame, and SkyCutJump times its last frame's move (a cut, not a glide), the sky is turned at once,
	// here, as before, the cut hiding it.
	// "-TheaterSkyAtOnce" turns it here always.
	const AirSky::Rgb Light = S.Board.bNight ? AirSky::Rgb(0.12f, 0.13f, 0.17f) : AirSky::Rgb(1.0f, 1.0f, 1.0f);
	const AirSky::Vec Sun = S.SkyPlace.Sun;
	const FVector EyeNow(Seen.X, Seen.Y, Seen.Z);
	const double Step = FVector::Dist(EyeNow, SkyEyeWas);
	const bool bCut = Step > SkyCut && (Step > SkyCutJump * SkyStepWas || Step > SkyCutFar);
	// (a turn painted aside stands on the picture while the eye goes on a few frames: the painters fade a puff by the
	// lens along that way (AirSky's SetSweep), so that in a glide of hundreds of metres a frame none shows as a disc
	// across the lens. The way's length: SkySweep frames of the lesser of this frame's step and the last one's, so one
	// long frame does not stretch it; none at a cut. The pre-check, round 12)
	const FVector Ahead = bCut ? FVector::ZeroVector : (EyeNow - SkyEyeWas).GetSafeNormal() * (FMath::Min(Step, SkyStepWas) * SkySweep);
	SkyEyeWas = EyeNow;
	SkyStepWas = Step;
	double ThisFrame = 0.0; // (what the sky cost this thread this frame)
	auto Count = [this](int32 What, double PaintSeconds, double SendSeconds)
	{
		(What == static_cast<int32>(AirSky::Rooms::Sent::Both) ? SkyBothTurns : SkyNearTurns) += 1;
		SkyPaintSeconds += PaintSeconds;
		SkySendSeconds += SendSeconds;
	};
	// (a turn painted aside and done: sent; one still painting waits for a later frame, but at a cut its painting is
	// finished here and sent, so that every turn painted is sent, and the rooms' line and their meshes stay one)
	if (SkyTurning.IsValid())
	{
		if (SkyTurning.IsReady() || bCut)
		{
			const double Began = FPlatformTime::Seconds();
			SkyTurning.Wait();
			SkyTurning = TFuture<void>();
			const double Waited = FPlatformTime::Seconds() - Began;
			const double Sending = SkySend(SkyTurningWhat);
			ThisFrame += Waited + Sending;
			Count(SkyTurningWhat, SkyTurningSeconds, Sending);
		}
		else
		{
			++SkyLate;
		}
	}
	if (!SkyTurning.IsValid())
	{
		AirSky::Rooms& SkyRooms = S.SkyRooms;
		const AirSky::Rooms::Sent What = SkyRooms.Wants(Seen, Drift);
		const int32 WhatNumber = static_cast<int32>(What);
		if (What != AirSky::Rooms::Sent::None && (bSkyAtOnce || bCut))
		{
			const double Began = FPlatformTime::Seconds();
			SkyRooms.SetSweep(AirSky::Vec(0.0, 0.0, 0.0));
			SkyRooms.Paint(What, S.SkyAside, Seen, Drift, Sun, Light);
			SkyFill(S, WhatNumber);
			const double Painted = FPlatformTime::Seconds() - Began;
			const double Sending = SkySend(WhatNumber);
			ThisFrame += Painted + Sending;
			Count(WhatNumber, Painted, Sending);
			++SkyAtOnce;
		}
		else if (What != AirSky::Rooms::Sent::None)
		{
			// (the state is held for the turn's length; the actor's points it writes are waited for in EndPlay and LaySky)
			SkyTurningWhat = WhatNumber;
			SkyRooms.SetSweep(AirSky::Vec(Ahead.X, Ahead.Y, Ahead.Z));
			SkyTurning = Async(EAsyncExecution::ThreadPool, [this, Keep = State, What, WhatNumber, Seen, Drift, Sun, Light]()
			{
				const double Began = FPlatformTime::Seconds();
				Keep->SkyRooms.Paint(What, Keep->SkyAside, Seen, Drift, Sun, Light);
				SkyFill(*Keep, WhatNumber);
				SkyTurningSeconds = FPlatformTime::Seconds() - Began;
			});
		}
	}
	SkySlowest = FMath::Max(SkySlowest, ThisFrame);
	// (the sky's clock, in the log every 5 s: how often each room turned, what painting a turn cost (on the pool's
	// thread, or here when turned at once) and what sending it cost this thread, the most it cost this thread in a frame,
	// how many turns were painted at once, and how many frames a turn was still painting; the render thread's share of
	// a send is not in it)
	const double Done = FPlatformTime::Seconds();
	if (SkyClockAt < 0.0)
	{
		SkyClockAt = Done;
	}
	else if (Done - SkyClockAt >= 5.0)
	{
		const int32 Turns = FMath::Max(1, SkyNearTurns + SkyBothTurns);
		UE_LOG(LogTheater, Log, TEXT("Air battle sky, %.1f s: %d near turns, %d far; painting %.2f ms a turn (%s), sending %.2f ms, the slowest %.2f ms (this thread's most in a frame); %d at once, %d frames a turn still painting; the rooms' line %.0f m."),
			Done - SkyClockAt, SkyNearTurns, SkyBothTurns, SkyPaintSeconds * 1000.0 / Turns, bSkyAtOnce ? TEXT("here") : TEXT("aside"), SkySendSeconds * 1000.0 / Turns,
			SkySlowest * 1000.0, SkyAtOnce, SkyLate, SkyLine);
		SkyNearTurns = 0;
		SkyBothTurns = 0;
		SkyAtOnce = 0;
		SkyLate = 0;
		SkyPaintSeconds = 0.0;
		SkySendSeconds = 0.0;
		SkySlowest = 0.0;
		SkyClockAt = Done;
	}
}

void AAirBattle::SkyFill(FAirBattleState& S, int32 What)
{
	// (on whichever thread painted the turn: the points into the engine's frame, the colours as they are, linear already)
	auto Fill = [this](const std::vector<AirSky::Vec>& From, const std::vector<uint8_t>& FromRgba, TArray<FVector>& Points, TArray<FColor>& Colours)
	{
		const int32 Count = FMath::Min3(Points.Num(), Colours.Num(), static_cast<int32>(FMath::Min(From.size(), FromRgba.size() / 4)));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const AirSky::Vec& P = From[static_cast<size_t>(Index)];
			Points[Index] = World(P.X, P.Y, P.Z);
			const size_t C = static_cast<size_t>(Index) * 4;
			Colours[Index] = FColor(FromRgba[C], FromRgba[C + 1], FromRgba[C + 2], FromRgba[C + 3]);
		}
	};
	AirSky::Rooms& SkyRooms = S.SkyRooms;
	if (What == static_cast<int32>(AirSky::Rooms::Sent::None))
	{
		return;
	}
	Fill(SkyRooms.NearPoints, SkyRooms.NearRgba, NearCloudPoints, NearCloudColours);
	if (bCloudShapes)
	{
		AirParts::SheetPlaces(SkyRooms.NearUvs, NearCloudPlaces, NearCloudWays, NearCloudPoints.Num());
	}
	if (What == static_cast<int32>(AirSky::Rooms::Sent::Both))
	{
		Fill(SkyRooms.FarPoints, SkyRooms.FarRgba, CloudPoints, CloudColours);
		if (bCloudShapes)
		{
			AirParts::SheetPlaces(SkyRooms.FarUvs, CloudPlaces, CloudWays, CloudPoints.Num());
		}
		Fill(SkyRooms.Shadows, SkyRooms.ShadowRgba, ShadowPoints, ShadowColours);
	}
}

double AAirBattle::SkySend(int32 What)
{
	// (this thread only, the turn painted and filled: each room's points and colours to its mesh)
	const double Began = FPlatformTime::Seconds();
	auto Send = [](UProceduralMeshComponent* Mesh, const TArray<FVector>& Points, const TArray<FColor>& Colours, const TArray<FVector2D>* Places, const TArray<FVector2D>* Ways)
	{
		if (Mesh && Points.Num() > 0)
		{
			if (Places && Ways && Places->Num() == Points.Num() && Ways->Num() == Points.Num())
			{
				Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), *Places, *Ways, TArray<FVector2D>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
			}
			else
			{
				Mesh->UpdateMeshSection(0, Points, TArray<FVector>(), TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
			}
		}
	};
	if (What != static_cast<int32>(AirSky::Rooms::Sent::None))
	{
		Send(NearClouds, NearCloudPoints, NearCloudColours, bCloudShapes ? &NearCloudPlaces : nullptr, bCloudShapes ? &NearCloudWays : nullptr);
	}
	if (What == static_cast<int32>(AirSky::Rooms::Sent::Both))
	{
		Send(Clouds, CloudPoints, CloudColours, bCloudShapes ? &CloudPlaces : nullptr, bCloudShapes ? &CloudWays : nullptr);
		Send(CloudShadows, ShadowPoints, ShadowColours, nullptr, nullptr);
	}
	// (the rooms are this thread's again: the line, for the log)
	if (State.IsValid())
	{
		SkyLine = State->SkyRooms.Line();
	}
	return FPlatformTime::Seconds() - Began;
}

void AAirBattle::SkyFinish(bool bSend)
{
	// (a turn painted aside writes this actor's points and reads the stage's ground: finished before either goes)
	if (SkyTurning.IsValid())
	{
		SkyTurning.Wait();
		SkyTurning = TFuture<void>();
		if (bSend)
		{
			SkySend(SkyTurningWhat);
		}
	}
}

FBattleEffects* AAirBattle::KitAt(const FVector& WorldAt, const FBattleEffects* InNow) const
{
	FBattleEffects* const Air = Effects ? Effects.Get() : GroundFx.Get();
	if (!GroundFx || !Clouds || !State.IsValid())
	{
		return Air;
	}
	// (under the clouds' base as drawn there, with the clouds drawn: the ground's kit, drawn under them when the eye is
	// over them; a cloud is lifted to 300 m over the ground under it)
	const FAirBattleState& S = *State;
	const double X = WorldAt.Y / UnitsPerMetre, Y = WorldAt.X / UnitsPerMetre, Z = WorldAt.Z / UnitsPerMetre;
	const double Floor = S.Show.Ground ? S.Show.Ground(X, Y) : 0.0;
	const double Base = FMath::Max(S.Sky.Base, Floor + 300.0);
	const double Margin = InNow == GroundFx.Get() ? 50.0 : InNow == Air ? -50.0 : 0.0;
	return Z < Base + Margin ? GroundFx.Get() : Air;
}

FBattleEffects* AAirBattle::GroundKit() const
{
	if (GroundFx && Clouds)
	{
		return GroundFx.Get();
	}
	return Effects ? Effects.Get() : GroundFx.Get();
}

FVector AAirBattle::World(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

FVector AAirBattle::WorldVector(double X, double Y, double Z) const
{
	return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre);
}

void AAirBattle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PendingReplay.IsEmpty())
	{
		if (GetWorld() && GetWorld()->GetTimeSeconds() > 2.0)
		{
			const FString Key = PendingReplay;
			PendingReplay.Empty();
			BeginReplay(Key);
		}
		return;
	}
	KeepTheTableHidden();
	if (LandPainting.IsValid() && LandPainting.IsReady())
	{
		LandPainting.Wait();
		LandPainting = TFuture<void>();
		LayLand();
	}
	const double Real = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);
	RealT += Real;
	if (QuitAt >= 0.0 && RealT >= QuitAt)
	{
		QuitAt = -1.0;
		UE_LOG(LogTheater, Display, TEXT("Air battle: %d pictures taken; quitting."), ShotsTaken);
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			UKismetSystemLibrary::QuitGame(this, Controller, EQuitPreference::Quit, false);
		}
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (!bBegun || !State.IsValid())
	{
		return;
	}
	FAirBattleState& S = *State;
	// The screen's clock: x1 is real time; the run-up and the run home at x10; held while a question stands or the card is up.
	S.Clock.bQuestion = S.Question != 0 || S.bCard;
	S.Clock.bFlyover = S.bFlying;
	const double Run = S.Clock.Step(Real, S.Desk.Head(), S.Board.Scale, S.ClockTag);
	const double End = S.bReplay ? S.Recording.Views.back().Head.T : 1e18;
	S.BattleT = FMath::Min(End, S.BattleT + Run);
	Feed();
	if (!S.bReplay)
	{
		S.BattleT = FMath::Min(S.BattleT, S.FedT);
	}
	if (Eye)
	{
		const FVector At = Eye->GetActorLocation() / UnitsPerMetre;
		S.Show.Eye = AirShow::Vec(At.Y, At.X, At.Z);
	}
	S.Show.Advance(S.BattleT);
	if (!S.bFlyTried && !S.Show.Groups().empty())
	{
		// The flyover before the battle, once its first groups stand ("-TheaterNoFlyover" goes straight to the battle).
		S.bFlyTried = true;
		if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterNoFlyover")) && S.Fly.Begin(S.Show.Groups(), S.Board.TargetX, S.Board.TargetY, S.Board.PlaceKind == "harbour"))
		{
			S.bFlying = true;
			S.FlyT = 0.0;
			S.GlideT = -1.0;
			S.FlyBars = 1.0;
			UE_LOG(LogTheater, Display, TEXT("Air battle: the flyover, %.1f s: the raid (group %d), the target, %s"), S.Fly.Length(), S.Fly.RaidGroup(),
				S.Fly.GuardGroup() >= 0 ? TEXT("the guard") : TEXT("no guard up"));
		}
	}
	S.Desk.Reach(S.Show.Shown());
	PlayEffects();
	S.Desk.Write(Real, S.Show.Groups(), S.ClockTag, S.Words);
	if (MetRealT < 0.0 && S.Desk.Head().Contact >= 0.0)
	{
		MetRealT = RealT;
	}
	// (the eye first: a cut to a new aircraft must draw it at its near shape on the very frame of the cut)
	AimEye(Real);
	Draw(Real);
	DrawSky();
	if (Eye)
	{
		ZenithAt(Eye->GetActorLocation());
		HazeBandAt(Eye->GetActorLocation());
	}
	if (Effects && Eye)
	{
		int32 ViewW = 1600;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			ViewW = FMath::Max(1, static_cast<int32>(Size.X));
		}
		FBattleJolt Jolt = Effects->Tick(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
		if (GroundFx)
		{
			const FBattleJolt Under = GroundFx->Tick(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
			Jolt.Offset += Under.Offset;
			Jolt.RollDegrees += Under.RollDegrees;
		}
		if (S.bClose)
		{
			// (the braced camera's jolt, in the close view only: the overview stays still)
			Eye->SetActorLocationAndRotation(Eye->GetActorLocation() + Jolt.Offset, Eye->GetActorRotation() + FRotator(0.0, 0.0, Jolt.RollDegrees));
		}
	}
	if (Stage)
	{
		Stage->Advance(S.Show.AirNow());
	}
	Play(Real);
	PlaceTags();
	if (S.NoteAge >= 0.0 && !S.Note.empty())
	{
		S.NoteAge += Real;
		if (S.NoteAge > 4.0)
		{
			S.Note.clear();
		}
	}
	// The battle's end: the over line shown, then the after card a few seconds on.
	if (S.Desk.Head().bOver && S.Show.Shown() >= S.Desk.Head().T - 1e-6)
	{
		if (!S.bOver)
		{
			S.bOver = true;
			S.OverRealT = 0.0;
			if (!S.bReplay && Session)
			{
				FString Reply, Error;
				AirFeed::Json Ended;
				std::string Why;
				AirFeed::After After;
				if (Session->CallBattle(TEXT("battleEnd"), TEXT("[]"), Reply, Error) && AirFeed::Parse(AirParts::Narrow(Reply), Ended, Why) && AirFeed::ReadAfter(Ended["after"], After))
				{
					S.Card = S.Desk.CardOf(After);
				}
			}
		}
		S.OverRealT += Real;
		if (S.OverRealT > 4.0)
		{
			S.bCard = true;
		}
	}
	TakeShots(Real);
}

void AAirBattle::Feed()
{
	FAirBattleState& S = *State;
	auto Take = [&S](const AirFeed::Frame& View)
	{
		S.Show.Take(View.Head.T, View.Groups, View.Events, View.Ships);
		S.Desk.Take(View);
		S.FedT = View.Head.T;
	};
	if (S.bReplay)
	{
		while (S.NextView < S.Recording.Views.size() && S.Recording.Views[S.NextView].Head.T <= S.BattleT + 1.0 + 1e-9)
		{
			Take(S.Recording.Views[S.NextView++]);
		}
		return;
	}
	// Live: the rules run a battle second ahead of what is shown, so the flight core always has the next view in hand.
	if (!Session || S.Desk.Head().bOver)
	{
		return;
	}
	const double Owed = S.BattleT + 1.0 - S.FedT;
	if (Owed <= 1e-6)
	{
		return;
	}
	FString Reply, Error;
	if (!Session->CallBattle(TEXT("battleTick"), FString::Printf(TEXT("[%.4f]"), Owed), Reply, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.battleTick failed: %s"), *Error);
		return;
	}
	AirFeed::Json Ticked;
	std::string Why;
	AirFeed::Frame View;
	if (AirFeed::Parse(AirParts::Narrow(Reply), Ticked, Why) && AirFeed::ReadView(Ticked, S.Board, View) && View.Head.T > S.FedT + 1e-9)
	{
		Take(View);
	}
}

FString AAirBattle::ModelFor(const FString& Model, const FString& Nation, bool bFighter) const
{
	// The battle's model names (the unit table's, and the carrier aircraft's), as the miniature kit knows them: a
	// sculpted model of the name where Content/Models has one, else the nation's plain fighter or bomber in its paint.
	static const TMap<FString, FString> Known = {
		{ TEXT("B5N"), TEXT("Kate") }, { TEXT("D3A"), TEXT("Val") }, { TEXT("SBD"), TEXT("Dauntless") }, { TEXT("TBD"), TEXT("Devastator") },
		{ TEXT("TBF"), TEXT("Avenger") }, { TEXT("F4F"), TEXT("Wildcat") }, { TEXT("G4M"), TEXT("Betty") }, { TEXT("A6M"), TEXT("Zero") },
		{ TEXT("P40"), TEXT("Warhawk") }, { TEXT("B-17"), TEXT("B17") }, { TEXT("P-40"), TEXT("Warhawk") },
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
	const FString Out = !Name.IsEmpty() && Probe.Vehicle(Name + TEXT(":") + Nation, FVector::ZeroVector, 1.0, 0.0, false)
		? Name + TEXT(":") + Nation : (bFighter ? TEXT("Fighter:") : TEXT("Bomber:")) + Nation;
	Found.Add(Asked, Out);
	return Out;
}

namespace AirParts
{
	/** An aircraft's shape, built once at its true size with its middle at its own origin; each aircraft has its own
	 * mesh of it. */
	struct FShape
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
		/** Its airscrews, their blades taken out of the shape to turn as discs of their own (in the shape's frame). */
		struct FScrewAt
		{
			FVector Hub = FVector::ZeroVector;
			double Radius = 0.0;
			int32 Blades = 3;
		};
		TArray<FScrewAt> Screws;
	};

	static TMap<FString, FShape>& Shapes()
	{
		static TMap<FString, FShape> Kept;
		return Kept;
	}

	static FShape ShapeOf(const FMiniatureMesh& Made, const FVector& Middle, double Fit)
	{
		FShape Fresh;
		Fresh.Points.Reserve(Made.Vertices.Num());
		for (const FVector& Point : Made.Vertices)
		{
			Fresh.Points.Add((Point - Middle) * Fit);
		}
		Fresh.Triangles = Made.Triangles;
		Fresh.Normals = Made.Normals;
		Fresh.Colors = Made.Colors;
		return Fresh;
	}

	/**
	 * A sculpted aircraft's airscrews (Battle/AirScrew), found by the paints its file gives the points the model was made
	 * from, in order: their blades are taken out of its near shape, which turns them as discs. Read wrong (its points
	 * not in the file's order, a screw that is not one) and the blades stay where they are, standing still.
	 */
	static void TakeScrews(FShape& Shape, const FMiniatureMesh& Made, const FString& Kind, const FVector& Middle, double Fit, double U)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Models") / (Kind + TEXT(".json")))))
		{
			return;
		}
		const std::string FileText(TCHAR_TO_UTF8(*Text));
		const std::vector<int> Paints = AirScrew::NearPaints(FileText);
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(FileText);
		if (Paints.empty() || static_cast<int32>(Paints.size()) > Made.Vertices.Num() || Shape.Triangles.Num() != Made.Triangles.Num())
		{
			return;
		}
		std::vector<AirScrew::Vec3> Points;
		std::vector<bool> Blade(Paints.size(), false);
		Points.reserve(Paints.size());
		for (size_t K = 0; K < Paints.size(); ++K)
		{
			const FVector& P = Made.Vertices[static_cast<int32>(K)];
			Points.push_back({ P.X, P.Y, P.Z });
			Blade[K] = Are.IsBlade(Paints[K]);
		}
		const std::vector<int> Triangles(Made.Triangles.GetData(), Made.Triangles.GetData() + Made.Triangles.Num());
		const AirScrew::Found Found = AirScrew::Find(Points, Blade, Triangles);
		if (Found.Screws.empty())
		{
			return;
		}
		for (const AirScrew::Screw& S : Found.Screws)
		{
			const double Metres = S.Radius / U;
			if (FMath::Abs(S.Axis.X) < 0.98 || Metres < 0.9 || Metres > 2.4 || S.Blades < 2 || S.Blades > 4)
			{
				return;
			}
		}
		TArray<int32> Kept;
		Kept.Reserve(Shape.Triangles.Num());
		// (the blades go, and with them the model's own painted blur disc where it has one: the turning disc stands
		// in for both, and the blur would draw solid here)
		auto IsBlur = [&](int32 Point) { return Point >= 0 && static_cast<size_t>(Point) < Paints.size() && Are.IsBlur(Paints[static_cast<size_t>(Point)]); };
		for (int32 T = 0; T + 2 < Shape.Triangles.Num(); T += 3)
		{
			const bool bBlur = IsBlur(Made.Triangles[T]) && IsBlur(Made.Triangles[T + 1]) && IsBlur(Made.Triangles[T + 2]);
			if (!Found.BladeTriangle[static_cast<size_t>(T / 3)] && !bBlur)
			{
				Kept.Append({ Shape.Triangles[T], Shape.Triangles[T + 1], Shape.Triangles[T + 2] });
			}
		}
		Shape.Triangles = MoveTemp(Kept);
		for (const AirScrew::Screw& S : Found.Screws)
		{
			FShape::FScrewAt At;
			At.Hub = (FVector(S.Hub.X, S.Hub.Y, S.Hub.Z) - Middle) * Fit;
			At.Radius = S.Radius * Fit;
			At.Blades = S.Blades;
			Shape.Screws.Add(At);
		}
	}

	/**
	 * An airscrew turning, as a film's camera sees it: a see-through dark disc in its own Y-Z plane (the shaft along X),
	 * clear at the boss and soft at the tips, darker where each blade sweeps, so that it is seen to turn. Both faces.
	 */
	static void DiscOf(double Radius, int32 Blades, TArray<FVector>& Points, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FColor>& Colours)
	{
		constexpr int32 Round = 48;
		const double Rings[4] = { 0.12, 0.5, 0.9, 1.0 };
		for (int32 Ring = 0; Ring < 4; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const double A = UE_DOUBLE_TWO_PI * K / Round;
				// (the sweep of each blade: a dark smear behind it, fading round to the next)
				const double Swept = FMath::Pow(0.5 + 0.5 * FMath::Cos(Blades * A), 5.0);
				const double Alpha = Ring == 0 || Ring == 3 ? 0.0 : (Ring == 1 ? 46.0 + 80.0 * Swept : 34.0 + 64.0 * Swept);
				Points.Add(FVector(0.0, FMath::Sin(A), FMath::Cos(A)) * Radius * Rings[Ring]);
				Normals.Add(FVector(1.0, 0.0, 0.0));
				Colours.Add(FColor(30, 30, 32, static_cast<uint8>(FMath::Clamp(Alpha, 0.0, 255.0))));
			}
		}
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const int32 A = Ring * Round + K, B = Ring * Round + (K + 1) % Round, C = A + Round, D = B + Round;
				Triangles.Append({ A, B, D, A, D, C });
				Triangles.Append({ A, D, B, A, C, D });
			}
		}
	}

	/**
	 * A stand-in's coat in the paint its navy flew (the art director's note on the first Mac run: three orange-yellow
	 * aircraft over Oahu, whose side could not be told). The plain machines wear their country's colour on the map; the
	 * carrier aircraft of 1941-42 wore the Japanese navy's grey, or the American navy's blue-grey. Every point whose
	 * colour is the coat's, or the coat's shaded, is painted the same shade of the new one; marks, glass, metal and the
	 * undersides keep theirs.
	 */
	static void Recoat(TArray<FColor>& Colors, const FColor& From, const FColor& To)
	{
		for (FColor& C : Colors)
		{
			const double R = C.R / FMath::Max(1.0, static_cast<double>(From.R));
			const double G = C.G / FMath::Max(1.0, static_cast<double>(From.G));
			const double B = C.B / FMath::Max(1.0, static_cast<double>(From.B));
			const double Mean = (R + G + B) / 3.0;
			if (Mean < 0.4 || Mean > 1.3 || FMath::Abs(R - Mean) > 0.07 || FMath::Abs(G - Mean) > 0.07 || FMath::Abs(B - Mean) > 0.07)
			{
				continue;
			}
			auto Shade = [Mean](uint8 Channel) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Channel * Mean), 0, 255)); };
			C = FColor(Shade(To.R), Shade(To.G), Shade(To.B), C.A);
		}
	}

	/** The navy paint a stand-in wears, by its nation and model; false if it keeps its own. */
	static bool NavyCoat(const FString& Nation, const FString& Model, FColor& Out)
	{
		if (Nation == TEXT("JAP") || Nation == TEXT("JPN"))
		{
			Out = FColor::FromHex(TEXT("A9B19F")); // (the Japanese navy's grey, as the battle models' Zero, Kate and Val)
			return true;
		}
		// (the recordings name them either way: "F4F" or "Wildcat", "SBD" or "Dauntless")
		static const TSet<FString> Carried = { TEXT("F4F"), TEXT("Wildcat"), TEXT("SBD"), TEXT("Dauntless"), TEXT("TBD"), TEXT("Devastator"), TEXT("TBF"), TEXT("Avenger") };
		if (Nation == TEXT("USA") && Carried.Contains(Model))
		{
			Out = FColor::FromHex(TEXT("566A78")); // (the American navy's blue-grey, as the battle models' Dauntless)
			return true;
		}
		return false;
	}
}

UProceduralMeshComponent* AAirBattle::PlaneMesh(int32 Key, const FString& Model, const FString& Nation, bool bFighter, double Span)
{
	if (TObjectPtr<UProceduralMeshComponent>* Found = Planes.Find(Key))
	{
		const FPlaneMade* Was = PlaneMade.Find(Key);
		if (*Found && Was && Was->Model == Model && Was->Span == Span)
		{
			return Found->Get();
		}
		// (made before a view named its model, the place's main one standing in: made again as its own)
		if (*Found)
		{
			(*Found)->DestroyComponent();
		}
		if (FPlaneMade* Old = PlaneMade.Find(Key))
		{
			for (const TWeakObjectPtr<UProceduralMeshComponent>& Disc : Old->Discs)
			{
				if (Disc.IsValid())
				{
					Disc->DestroyComponent();
				}
			}
		}
		Planes.Remove(Key);
		PlaneMade.Remove(Key);
	}
	// A sculpted model has two shapes: its far one (a few thousand points) for every aircraft, and its near one (tens
	// of thousands) made only for those that come near the eye (PlaneNear): two hundred aircraft at the near shape were
	// millions of points. A stand-in is sized to the battle's span, so each span is a shape of its own (a Kate is not a
	// Val's size), and wears its navy's paint.
	const FString Name = ModelFor(Model, Nation, bFighter);
	const bool bStandIn = Name.StartsWith(TEXT("Fighter:")) || Name.StartsWith(TEXT("Bomber:"));
	const FString ShapeKey = bStandIn ? FString::Printf(TEXT("%s@%.1f|%s"), *Name, Span, *Model) : Name;
	TMap<FString, AirParts::FShape>& Shapes = AirParts::Shapes();
	if (!Shapes.Contains(ShapeKey))
	{
		FMiniatureMesh Made;
		Made.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0, true);
		AirParts::FShape Near;
		if (Made.Vertices.Num() > 0)
		{
			// (to the battle's span: a stand-in's own size is the plain machine's, not this one's)
			const FBox Box(Made.Vertices);
			const FVector Middle = Box.GetCenter();
			const double Across = FMath::Max(1.0, Box.GetSize().Y);
			const double Fit = bStandIn ? Span * UnitsPerMetre / Across : 1.0;
			Near = AirParts::ShapeOf(Made, Middle, Fit);
			FString Kind, Whose;
			Name.Split(TEXT(":"), &Kind, &Whose);
			if (DiscInk)
			{
				AirParts::TakeScrews(Near, Made, Kind, Middle, Fit, UnitsPerMetre);
			}
			FColor Navy;
			const bool bNavy = bStandIn && AirParts::NavyCoat(Nation, Model, Navy);
			if (bNavy)
			{
				AirParts::Recoat(Near.Colors, FMiniatureMesh::VehicleColour(Nation).ToFColor(true), Navy);
			}
			// (the stand-ins too: the plain Fighter and Bomber models have far parts, and they are half of each battle)
			FMiniatureMesh Far;
			if (Far.Sculpted(Kind, Whose, FVector::ZeroVector, UnitsPerMetre, 0.0, true) && Far.Vertices.Num() > 0 && Far.Vertices.Num() < Made.Vertices.Num())
			{
				// (about the same middle and the same size as the near shape's, so that the change from one to the
				// other does not jump)
				AirParts::FShape Away = AirParts::ShapeOf(Far, Middle, Fit);
				if (bNavy)
				{
					AirParts::Recoat(Away.Colors, FMiniatureMesh::VehicleColour(Nation).ToFColor(true), Navy);
				}
				Shapes.Add(ShapeKey + TEXT("|far"), MoveTemp(Away));
			}
		}
		Shapes.Add(ShapeKey, MoveTemp(Near));
	}
	const AirParts::FShape* Far = Shapes.Find(ShapeKey + TEXT("|far"));
	const AirParts::FShape* First = Far ? Far : Shapes.Find(ShapeKey);
	// (a name of its own: one made again for its own model must not take the name of the one just taken down)
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirPlane_%d"), Key)));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->bUseAsyncCooking = false;
	Mesh->RegisterComponent();
	if (First && First->Points.Num() > 0)
	{
		Mesh->CreateMeshSection(0, First->Points, First->Triangles, First->Normals, TArray<FVector2D>(), First->Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, PlaneInk);
	}
	Planes.Add(Key, Mesh);
	FPlaneMade Made;
	Made.Model = Model;
	Made.Span = Span;
	Made.Shape = ShapeKey;
	Made.bTwoShapes = Far != nullptr;
	Made.Phase = static_cast<double>(GetTypeHash(Key) % 360u);
	PlaneMade.Add(Key, Made);
	return Mesh;
}

void AAirBattle::PlaneNear(int32 Key, UProceduralMeshComponent* Mesh, bool bNear)
{
	FPlaneMade* Made = PlaneMade.Find(Key);
	if (!Mesh || !Made || !Made->bTwoShapes || Made->bNear == bNear)
	{
		return;
	}
	if (bNear && !Made->bNearMade)
	{
		const AirParts::FShape* Near = AirParts::Shapes().Find(Made->Shape);
		if (!Near || Near->Points.Num() == 0)
		{
			return;
		}
		Mesh->CreateMeshSection(1, Near->Points, Near->Triangles, Near->Normals, TArray<FVector2D>(), Near->Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(1, PlaneInk);
		Made->bNearMade = true;
		// (its airscrews, each a disc of its own on the aircraft at its hub, turned about its shaft as it flies)
		for (const AirParts::FShape::FScrewAt& Screw : Near->Screws)
		{
			UProceduralMeshComponent* Disc = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("AirScrew_%d"), Key)));
			Disc->SetupAttachment(Mesh);
			Disc->SetMobility(EComponentMobility::Movable);
			Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Disc->SetCastShadow(false);
			Disc->bUseAsyncCooking = false;
			Disc->RegisterComponent();
			Disc->SetRelativeLocation(Screw.Hub);
			TArray<FVector> Points, Normals;
			TArray<int32> Triangles;
			TArray<FColor> Colours;
			AirParts::DiscOf(Screw.Radius, Screw.Blades, Points, Triangles, Normals, Colours);
			Disc->CreateMeshSection(0, Points, Triangles, Normals, TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>(), false);
			Disc->SetMaterial(0, DiscInk);
			Made->Discs.Add(Disc);
		}
	}
	Mesh->SetMeshSectionVisible(0, !bNear);
	Mesh->SetMeshSectionVisible(1, bNear);
	Made->bNear = bNear;
	ShowDiscs(*Made, bNear);
}

void AAirBattle::ShowDiscs(FPlaneMade& Made, bool bShow)
{
	for (const TWeakObjectPtr<UProceduralMeshComponent>& Disc : Made.Discs)
	{
		if (Disc.IsValid() && Disc->IsVisible() != bShow)
		{
			Disc->SetVisibility(bShow);
		}
	}
}

void AAirBattle::Draw(double Real)
{
	FAirBattleState& S = *State;
	// Every aircraft where the flight core has it: its nose along its path, banked into its turn. The models are built
	// nose toward -X with the right hand at -Y, so each is turned half round before its heading, climb and bank.
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	const double U = UnitsPerMetre;
	const FVector EyeAt = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	TSet<int32> Seen;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		Seen.Add(P.Key);
		// (laid each frame, so an aircraft's nation is always this battle's: the pre-check on round 16)
		GunNations.Add(P.Key, AirParts::KitNation(P.Nation));
		TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(P.Key);
		if (!P.bShown)
		{
			if (Have && *Have)
			{
				(*Have)->SetVisibility(false);
			}
			if (FPlaneMade* Hidden = PlaneMade.Find(P.Key))
			{
				ShowDiscs(*Hidden, false);
			}
			continue;
		}
		UProceduralMeshComponent* Mesh = PlaneMesh(P.Key, AirParts::Wide(P.Model), AirParts::Wide(P.Nation), P.bFighter, P.Span);
		if (!Mesh)
		{
			continue;
		}
		Mesh->SetVisibility(true);
		const FQuat Attitude = FRotator(P.Pitch, P.Heading, P.Bank).Quaternion() * HalfRound;
		const FVector Where = World(P.At.X, P.At.Y, P.At.Z);
		Mesh->SetWorldLocationAndRotation(Where, Attitude);
		// The near shape within 300 m of the eye (and for the ridden aircraft), the far one past 450 m.
		if (const FPlaneMade* Made = PlaneMade.Find(P.Key); Made && Made->bTwoShapes)
		{
			const double Away = FVector::Dist(Where, EyeAt) / U;
			const bool bRidden = S.bClose && S.Subject.Plane == P.Key;
			PlaneNear(P.Key, Mesh, bRidden || Away < (Made->bNear ? 450.0 : 300.0));
		}
		// Its airscrews turning while it is near: a dead engine's falling aircraft windmills slower.
		if (FPlaneMade* Turning = PlaneMade.Find(P.Key); Turning && Turning->bNear && Turning->Discs.Num() > 0)
		{
			ShowDiscs(*Turning, true); // (shown again with an aircraft that was out of sight while near)
			const double Turns = P.bFalling ? ScrewTurns * 0.3 : ScrewTurns;
			const double Angle = FMath::Fmod(RealT * Turns * 360.0 + Turning->Phase, 360.0);
			for (int32 K = 0; K < Turning->Discs.Num(); ++K)
			{
				if (UProceduralMeshComponent* Disc = Turning->Discs[K].Get())
				{
					Disc->SetRelativeRotation(FRotator(0.0, 0.0, Angle + K * 23.0));
				}
			}
		}
	}
	for (TPair<int32, TObjectPtr<UProceduralMeshComponent>>& Pair : Planes)
	{
		if (!Seen.Contains(Pair.Key) && Pair.Value)
		{
			Pair.Value->SetVisibility(false);
			if (FPlaneMade* Gone = PlaneMade.Find(Pair.Key))
			{
				ShowDiscs(*Gone, false);
			}
		}
	}
	// The small things, laid afresh each frame in one mesh: a parachute's canopy and its airman, a torn piece of a
	// wreck tumbling, a bomb or a torpedo falling.
	FMiniatureMesh Things;
	// (a real crew gets out, up to ten from a B-17, so three lost together may hang twenty canopies or more: past the
	// nearest MostChutes, those over 600 m from the eye are left out, the farthest first, as the trails are thinned)
	constexpr int32 MostChutes = 16;
	double ChuteFar = TNumericLimits<double>::Max();
	{
		TArray<double> Aways;
		for (const AirShow::Thing& T : S.Show.Things())
		{
			if (T.Kind == AirShow::ThingKind::Chute)
			{
				Aways.Add(FVector::Dist(World(T.At.X, T.At.Y, T.At.Z), EyeAt) / U);
			}
		}
		if (Aways.Num() > MostChutes)
		{
			Aways.Sort();
			ChuteFar = FMath::Max(600.0, Aways[MostChutes - 1]);
		}
	}
	for (const AirShow::Thing& T : S.Show.Things())
	{
		const FVector At = World(T.At.X, T.At.Y, T.At.Z);
		switch (T.Kind)
		{
		case AirShow::ThingKind::Chute:
			if (FVector::Dist(At, EyeAt) / U > ChuteFar)
			{
				break;
			}
			if (T.bOpen)
			{
				Things.Pillar(At + FVector(0.0, 0.0, 5.2 * U), 3.6 * U, 0.9 * U, 1.7 * U, AirParts::Silk, 14);
				for (const double Side : { -1.0, 1.0 })
				{
					Things.Rod(At + FVector(0.0, 0.0, 1.6 * U), At + FVector(Side * 3.2 * U, 0.0, 5.2 * U), 0.03 * U, AirParts::Silk, 3);
				}
			}
			else
			{
				Things.Limb(At + FVector(0.0, 0.0, 1.6 * U), At + FVector(0.0, 0.0, 4.0 * U), 0.15 * U, 0.35 * U, AirParts::Silk, 6);
			}
			Things.Limb(At, At + FVector(0.0, 0.0, 1.7 * U), 0.22 * U, 0.2 * U, AirParts::Airman, 6);
			break;
		case AirShow::ThingKind::Piece:
		{
			const double Turn = FMath::RadiansToDegrees(T.Spin * T.Age) + T.Key * 37.0;
			const double Side = FMath::Max(0.3, T.Size);
			Things.Box(At, FVector(Side * U, Side * 0.6 * U, 0.08 * U), (T.Key % 3 == 0) ? AirParts::Scorched : AirParts::Skin, Turn, Turn * 0.7);
			break;
		}
		case AirShow::ThingKind::Bomb:
		{
			const FVector Way = WorldVector(T.Vel.X, T.Vel.Y, T.Vel.Z).GetSafeNormal();
			const double Long = (T.bTorpedo ? 5.5 : 2.0) * U;
			Things.Rod(At - Way * Long * 0.5, At + Way * Long * 0.5, (T.bTorpedo ? 0.28 : 0.2) * U, AirParts::Casing, 6);
			break;
		}
		}
	}
	Small->ClearAllMeshSections();
	if (Things.Vertices.Num() > 0)
	{
		Small->CreateMeshSection(0, Things.Vertices, Things.Triangles, Things.Normals, TArray<FVector2D>(), Things.Colors, TArray<FProcMeshTangent>(), false);
		Small->SetMaterial(0, PlainInk);
	}
	// The blasts' torn flames: streaming on with what is left of the aircraft for a moment, then out.
	for (int32 K = Torn.Num() - 1; K >= 0; --K)
	{
		FTorn& Flame = Torn[K];
		Flame.At += Flame.Velocity * Real;
		Flame.Velocity *= FMath::Max(0.0, 1.0 - Real * 0.8);
		if (Flame.Kit)
		{
			Flame.Kit->TrailMove(S.Show.AirNow(), Flame.Key, Flame.At, Flame.Velocity);
		}
		if (RealT >= Flame.Until)
		{
			if (Flame.Kit)
			{
				Flame.Kit->TrailStop(Flame.Key);
			}
			Torn.RemoveAt(K);
		}
	}
}

void AAirBattle::PlayEffects()
{
	FAirBattleState& S = *State;
	const double Now = S.Show.AirNow();
	const double U = UnitsPerMetre;
	const FVector Listener = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	for (const AirShow::Fx& E : S.Show.Effects)
	{
		const FVector At = World(E.At.X, E.At.Y, E.At.Z);
		switch (E.Kind)
		{
		case AirShow::FxKind::Gun:
		{
			// (in the kit for the muzzle's height, kept while the gun fires unless it passes the clouds' base)
			FBattleEffects* const* Was = GunKits.Find(E.Key);
			FBattleEffects* const Before = Was ? *Was : nullptr;
			FBattleEffects* const Kit = KitAt(At, Before);
			if (Before && Before != Kit)
			{
				Before->AirGunStop(E.Key);
			}
			if (Kit)
			{
				GunKits.Add(E.Key, Kit);
				const FString* Nation = GunNations.Find(E.Key / 64); // (the gun's key names its aircraft: Key * 64 + gun)
				AirParts::AirGun(Kit, Now, E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Rate),
				                 static_cast<float>(E.Mv * U), Nation ? *Nation : FString());
			}
			// The burst heard (mix.json air.gunsByCalibre): a take laid on while the trigger is down, never the same
			// take under the mix's gap after itself.
			const double* Heard = GunHeardAt.Find(E.Key);
			if (!Heard || RealT - *Heard > 0.9)
			{
				GunHeardAt.Add(E.Key, RealT);
				const TSharedPtr<FJsonObject> Calibre = AirParts::MixObject(Mix, TEXT("air.gunsByCalibre"));
				FString Take;
				float Cut = 0.0f;
				if (E.What == "cannon20")
				{
					const TSharedPtr<FJsonObject> Cannon = AirParts::MixObject(Calibre, TEXT("cannon"));
					Take = AirParts::MixTake(Cannon, TEXT("take"));
					const TArray<TSharedPtr<FJsonValue>>* Part = nullptr;
					if (Cannon.IsValid() && Cannon->TryGetArrayField(TEXT("part"), Part) && Part->Num() == 2)
					{
						Cut = static_cast<float>((*Part)[1]->AsNumber());
					}
				}
				else if (E.What == "hmg")
				{
					Take = AirParts::MixTake(AirParts::MixObject(Calibre, TEXT("heavy")), E.Text == "fixed" ? TEXT("wing") : E.Text == "turret" ? TEXT("turret") : TEXT("handHeld"));
				}
				else
				{
					Take = AirParts::MixTake(AirParts::MixObject(Calibre, E.Text == "hand" ? TEXT("handHeldRifle") : TEXT("rifle")), TEXT("takes"), E.Key);
				}
				if (!Take.IsEmpty())
				{
					if (UAudioComponent* Played = Sound(*Take, At, TEXT("air_guns"), 1.0f, 0.0f, Cut))
					{
						// (its takes still playing kept, the finished ones let go)
						TArray<TWeakObjectPtr<UAudioComponent>> Had;
						GunTakes.MultiFind(E.Key, Had);
						GunTakes.Remove(E.Key);
						for (const TWeakObjectPtr<UAudioComponent>& Take_ : Had)
						{
							if (Take_.IsValid() && Take_->IsPlaying())
							{
								GunTakes.Add(E.Key, Take_);
							}
						}
						GunTakes.Add(E.Key, Played);
					}
				}
			}
			break;
		}
		case AirShow::FxKind::GunStop:
		{
			FBattleEffects* Kit = nullptr;
			if (GunKits.RemoveAndCopyValue(E.Key, Kit) && Kit)
			{
				Kit->AirGunStop(E.Key);
			}
			GunHeardAt.Remove(E.Key);
			// The burst heard stops with the gun (Brian 11-20 14:20Z: "after the bomber crew bails out, they can't keep
			// shooting"): a take still playing is faded out at once, not left to run to its end after the trigger is up.
			TArray<TWeakObjectPtr<UAudioComponent>> Playing;
			GunTakes.MultiFind(E.Key, Playing);
			for (const TWeakObjectPtr<UAudioComponent>& Take : Playing)
			{
				if (Take.IsValid())
				{
					Take->FadeOut(0.08f, 0.0f);
				}
			}
			GunTakes.Remove(E.Key);
			break;
		}
		case AirShow::FxKind::Hit:
			if (FBattleEffects* Kit = KitAt(At))
			{
				Kit->AirHit(Now, At, WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What));
			}
			break;
		case AirShow::FxKind::TrailStart:
		{
			// (the scene's room for trails, S.Trails: 24 on the newer kit, 10 on an older; a burning aircraft's own
			// trail first, taking the place of the oldest piece's when the room is full)
			// (in the kit for its height where it starts: over the clouds' base the air's, under it the ground's)
			int PutOut = 0;
			FBattleEffects* const Kit = KitAt(At);
			if (Kit && S.Trails.Start(E.Key, E.Text == "piece" || E.What == "streak", PutOut))
			{
				if (PutOut != 0)
				{
					FTrailOn Out;
					if (TrailKits.RemoveAndCopyValue(PutOut, Out) && Out.Kit)
					{
						Out.Kit->TrailStop(PutOut);
					}
				}
				FTrailOn On;
				On.Kit = Kit;
				On.Kind = AirParts::Wide(E.What);
				On.Span = static_cast<float>(E.Size);
				On.KindT = Now;
				if (const FTrailOn* Was = TrailKits.Find(E.Key))
				{
					// (lit again while it burns: its kind's own start kept, the same kind going on at its full look; in
					// the kit it is already in, with the base's 50 m either side)
					On.Kit = KitAt(At, Was->Kit);
					if (Was->Kind == On.Kind)
					{
						On.KindT = Was->KindT;
					}
					if (Was->Kit && Was->Kit != On.Kit)
					{
						Was->Kit->TrailStop(E.Key);
					}
				}
				TrailKits.Add(E.Key, On);
				AirParts::TrailStart(On.Kit, On.KindT, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), On.Kind, On.Span);
				if (On.KindT < Now)
				{
					On.Kit->TrailMove(Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
				}
			}
			break;
		}
		case AirShow::FxKind::TrailMove:
			if (S.Trails.On(E.Key))
			{
				if (FTrailOn* On = TrailKits.Find(E.Key))
				{
					const FVector Velocity = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
					FBattleEffects* const Kit = KitAt(At, On->Kit);
					if (Kit && Kit != On->Kit)
					{
						// (passing the clouds' base: it goes on in the other kit, and what it has left in the air fades
						// where it is; started there as of when its kind began, so it carries on at its full look and
						// does not light again, and laid on from here)
						if (On->Kit)
						{
							On->Kit->TrailStop(E.Key);
						}
						On->Kit = Kit;
						AirParts::TrailStart(Kit, On->KindT, E.Key, At, Velocity, On->Kind, On->Span);
						Kit->TrailMove(Now, E.Key, At, Velocity);
					}
					else if (On->Kit)
					{
						On->Kit->TrailMove(Now, E.Key, At, Velocity);
					}
				}
			}
			break;
		case AirShow::FxKind::TrailStop:
		{
			FTrailOn On;
			const bool bKept = TrailKits.RemoveAndCopyValue(E.Key, On);
			if (S.Trails.Stop(E.Key) && bKept && On.Kit)
			{
				On.Kit->TrailStop(E.Key);
			}
			break;
		}
		case AirShow::FxKind::Flak:
		{
			if (FBattleEffects* Kit = KitAt(At))
			{
				Kit->AirFlak(Now, At, AirParts::Wide(E.What));
			}
			// Flak heard when it is seen (handoff 10, gap 6): the burst at its point, after the flash by the distance
			// over the speed of sound (none within 150 m, at most 3 s), no closer together than the mix's soundEvery.
			const double Every = AirParts::MixNumber(Mix, TEXT("air.flak.soundEvery"), 0.6);
			if (RealT - FlakHeardAt >= Every)
			{
				FlakHeardAt = RealT;
				const double Metres = FVector::Dist(At, Listener) / U;
				const float After = Metres < 150.0 ? 0.0f : static_cast<float>(FMath::Min(3.0, Metres / 343.0));
				const FString Burst = AirParts::MixTake(AirParts::MixObject(Mix, TEXT("air.flak")), TEXT("burst"), static_cast<int32>(RealT * 7.0));
				Sound(Burst.IsEmpty() ? TEXT("flak_burst_01") : *Burst, At, TEXT("explosions"), 1.0f, After);
			}
			break;
		}
		case AirShow::FxKind::Fall:
			// The debris row (the art director, 04:40Z; the effects kit's AirFall): blown apart, every piece at once
			// round the kit's short torn fireball; burning or spinning, pieces shed over the fall. Only while the
			// pieces already falling leave the kit room (S.Debris): past that the blast below keeps the older flame.
			// (in the kit for the height where the fall begins, as its trail)
			// A pilot hit or a glide down goes down whole: no pieces shed (the air thread's ways down, Text the way).
			if (E.Text == "pilot" || E.Text == "glide")
			{
				break;
			}
			if (FBattleEffects* const Kit = KitAt(At))
			{
				if (AirParts::HasAirFall(Kit) && S.Debris.Take(Now, E))
				{
					AirParts::AirFall(Kit, Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Size), World(0.0, 0.0, E.Deck).Z);
					if (E.What == "explode")
					{
						BlastsTaken.Add(E.Key);
					}
					else
					{
						FallsOn.Add(E.Key, Kit);
					}
				}
			}
			break;
		case AirShow::FxKind::FallMove:
			if (FBattleEffects* const* Kit = FallsOn.Find(E.Key))
			{
				if (E.Deck < 0.0)
				{
					// (down a hole and under the deck: the row, told the deck, sheds no more)
					AirParts::AirFallStop(*Kit, E.Key);
					FallsOn.Remove(E.Key);
				}
				else
				{
					AirParts::AirFallMove(*Kit, Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
				}
			}
			break;
		case AirShow::FxKind::FallStop:
		{
			FBattleEffects* Kit = nullptr;
			if (FallsOn.RemoveAndCopyValue(E.Key, Kit) && Kit)
			{
				AirParts::AirFallStop(Kit, E.Key);
			}
			break;
		}
		case AirShow::FxKind::Blast:
		{
			// Blown apart (the art director, 03:14Z): a short torn flame streaming on with the wreck, never a ball, the
			// sparks and scraps of the hits; the pieces are the flight core's. Where the kit's AirFall took it, its
			// torn fireball and debris row are the blast, and this older flame is not laid over them.
			if (BlastsTaken.Remove(E.Key) > 0)
			{
				break;
			}
			if (FBattleEffects* const Kit = KitAt(At))
			{
				const FVector Velocity = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
				for (int32 K = 0; K < 4; ++K)
				{
					Kit->AirHit(Now, At + FMath::VRand() * E.Size * 0.2 * U, FMath::VRand(), Velocity, TEXT("cannon20"));
				}
				FTorn& Flame = Torn.AddDefaulted_GetRef();
				Flame.Key = 1 << 29 | (TornNext++ & 0xFFFF);
				Flame.At = At;
				Flame.Velocity = Velocity * 0.6;
				Flame.Until = RealT + 0.9;
				Flame.Kit = Kit;
				AirParts::TrailStart(Kit, Now, Flame.Key, At, Flame.Velocity, TEXT("fire"), static_cast<float>(E.Size));
			}
			break;
		}
		case AirShow::FxKind::Splash:
			if (FBattleEffects* Kit = GroundKit())
			{
				// (a ditching, a glide on to the sea under control: a skip and spray, never a bomb's column)
				Kit->ShellOfSize(At, E.What == "ditch" ? TEXT("light") : E.Size >= 8.0 ? TEXT("bomb250") : E.Size >= 3.0 ? TEXT("light") : TEXT("grenade"), TEXT("water"));
			}
			break;
		case AirShow::FxKind::Crash:
			if (FBattleEffects* Kit = GroundKit())
			{
				// (a belly landing: dust thrown up, no fire)
				const bool bBelly = E.What == "belly";
				Kit->ShellOfSize(At, !bBelly && E.Size >= 8.0 ? TEXT("bomb250") : TEXT("grenade"), TEXT("earth"));
				if (!bBelly && E.Size >= 8.0)
				{
					Kit->Burn(At, static_cast<float>(E.Size * 0.6), 90.0f);
				}
			}
			break;
		case AirShow::FxKind::BombHit:
			if (FBattleEffects* Kit = GroundKit())
			{
				Kit->ShellOfSize(At, E.Size >= 500.0 ? TEXT("bomb500") : TEXT("bomb250"), E.What == "water" ? TEXT("water") : TEXT("earth"));
			}
			if (E.What != "water")
			{
				ScarLand(E.At.X, E.At.Y, E.Size > 0.0 ? E.Size : 250.0);
			}
			break;
		case AirShow::FxKind::Sound:
		{
			// The event's cue at its point (handoff section 3), a far take for a blast past the explosions' farTakesFrom.
			FString Name = AirParts::Wide(E.What);
			FName Family = TEXT("aircraft");
			if (Name == TEXT("plane_explode") || Name == TEXT("plane_down") || Name == TEXT("bomb_stick_01"))
			{
				Family = TEXT("explosions");
				const double From = AirParts::MixNumber(Mix, TEXT("distance.explosions.farTakesFrom"), 150.0);
				if (Name == TEXT("plane_explode") && FVector::Dist(At, Listener) / U > From)
				{
					Name = (E.Key + static_cast<int32>(RealT)) % 2 ? TEXT("explosion_far_01") : TEXT("explosion_far_02");
				}
			}
			else if (Name == TEXT("parachute_open"))
			{
				Family = TEXT("body");
			}
			Sound(*Name, At, *Family.ToString());
			break;
		}
		case AirShow::FxKind::Dispatch:
			S.Desk.Say(E.What, E.Text, E.Side, E.Key);
			break;
		case AirShow::FxKind::ShipHit:
			if (Stage)
			{
				Stage->Hit(E.Key, AirParts::Wide(E.What), Now);
			}
			break;
		case AirShow::FxKind::ShipSunk:
			if (Stage)
			{
				Stage->Sink(E.Key, AirParts::Wide(E.What), E.Size >= 1.0, Now);
			}
			if (ShipRealT < 0.0)
			{
				ShipRealT = RealT;
				FirstSunk = E.Key;
			}
			break;
		case AirShow::FxKind::Smoke:
			if (FBattleEffects* Kit = GroundKit())
			{
				Kit->Burn(At, 40.0f, 900.0f);
			}
			break;
		}
	}
	S.Show.Effects.clear();
}

void AAirBattle::AimEye(double Real)
{
	if (!Eye)
	{
		return;
	}
	FAirBattleState& S = *State;
	const double U = UnitsPerMetre;
	UCameraComponent* Lens = Eye->GetCameraComponent();
	// The flyover before the battle: its shots, each eye kept 60 m over the ground; then the glide (below, after the
	// strategy camera has found its place).
	if (S.bFlying && S.GlideT < 0.0)
	{
		S.FlyT += Real;
		if (S.FlyT < S.Fly.Length())
		{
			const AirDesk::Flyover::Shot Shot = S.Fly.At(S.FlyT, S.Show.Groups(), S.Show.Planes());
			FVector FlyEye = World(Shot.Eye.X, Shot.Eye.Y, Shot.Eye.Z);
			const FVector FlyLook = World(Shot.Look.X, Shot.Look.Y, Shot.Look.Z);
			if (Stage && Stage->IsRaised())
			{
				FlyEye.Z = FMath::Max(FlyEye.Z, Stage->GroundZ(FlyEye) + 60.0 * U);
			}
			FlyEye.Z = FMath::Max(FlyEye.Z, 40.0 * U);
			Lens->SetFieldOfView(static_cast<float>(AirDesk::Flyover::Lens));
			Lens->PostProcessSettings.MotionBlurAmount = 0.0f;
			S.GlideEye = FlyEye;
			S.GlideFacing = (FlyLook - FlyEye).Rotation();
			Eye->SetActorLocationAndRotation(S.GlideEye, S.GlideFacing);
			return;
		}
		S.GlideT = 0.0;
		S.GlideLong = 2.5;
		bLookSet = false;
	}
	// The close view (handoff 10, gap 1): it rides a plane of the group in hand, or with none in hand the busiest fighter
	// near the middle of the picture; a fall, a straggle or a limp cuts to the next, never a swoop. (Not while the
	// flyover's glide runs, which is at this function's end: a ride asked for during the flyover, a C pressed while the
	// battle loaded or a ride picture come due, starts when the glide is done. The cloud pre-check's catch, 17:05Z.)
	if (S.bClose && !S.bFlying)
	{
		const std::vector<AirShow::PlaneOut>& Out = S.Show.Planes();
		auto Find = [&Out](int Key) -> const AirShow::PlaneOut*
		{
			for (const AirShow::PlaneOut& P : Out)
			{
				if (P.Key == Key)
				{
					return &P;
				}
			}
			return nullptr;
		};
		const AirShow::PlaneOut* Rider = Find(S.Subject.Plane);
		S.Subject.Since += Real * S.Board.Scale;
		const bool bLost = !Rider || !Rider->bShown || Rider->bFalling || Rider->bOnGround;
		const bool bStale = S.Hand.empty() && S.Subject.Since > 10.0;
		if (bLost || bStale)
		{
			AirDesk::Subject Next;
			if (!S.Hand.empty())
			{
				for (const AirShow::PlaneOut& P : Out)
				{
					if (P.Group == S.Hand.front() && P.bShown && !P.bFalling && !P.bOnGround && P.Key != S.Subject.Plane)
					{
						Next.Plane = P.Key;
						Next.Other = P.Target;
						break;
					}
				}
			}
			if (Next.Plane < 0)
			{
				// (his own side's aircraft first: the picture rides with his men; the other side's when his have none up,
				// or when a picture run's ride asks for theirs)
				std::vector<AirShow::PlaneOut> Ours;
				for (const AirShow::PlaneOut& P : Out)
				{
					if ((P.Side == S.Desk.MySideIndex()) != bRideOther)
					{
						Ours.push_back(P);
					}
				}
				// (a fighter closing on what it fights, so that is in the picture too; on a bomber, one coming at it
				// head-on most of all: Brian on the Bremen film, "where are the german fighters?")
				Next = AirDesk::ChooseRide(Ours, Out, S.Show.Eye, bLost ? -1 : S.Subject.Plane);
				if (Next.Plane < 0 && !bLost && (Rider->Side == S.Desk.MySideIndex()) != bRideOther)
				{
					S.Subject.Since = 0.0; // (his only aircraft up: the picture stays with it)
				}
				else if (Next.Plane < 0)
				{
					Next = AirDesk::Choose(Out, std::vector<int>(), S.Show.Eye, bLost ? -1 : S.Subject.Plane);
				}
			}
			if (Next.Plane >= 0)
			{
				S.Subject = Next;
				Rider = Find(Next.Plane);
				CloseEye = FVector::ZeroVector; // (a cut: no swoop from the last)
			}
		}
		if (Rider)
		{
			// (the art director's rules from the first Mac run: the ridden aircraft always in the picture and seen from
			// behind, the horizon never rolled past 15 degrees, little blur)
			const FQuat Attitude = FRotator(Rider->Pitch, Rider->Heading, Rider->Bank).Quaternion();
			const FVector Ahead = Attitude.GetForwardVector();
			const FVector Body = World(Rider->At.X, Rider->At.Y, Rider->At.Z);
			const AirShow::PlaneOut* Foe = Find(Rider->Target >= 0 ? Rider->Target : S.Subject.Other);
			// The way the eye trails: the aircraft's path, eased over a third of a second so that a jink or a snap roll
			// does not whip the picture round. The eye is set at its place from the aircraft each frame (never eased
			// toward it), so a fast aircraft does not draw away from it, nor a turning one swing past it.
			const bool bCut = CloseEye.IsZero();
			CloseAhead = bCut || CloseAhead.IsNearlyZero() ? Ahead : (CloseAhead + (Ahead - CloseAhead) * AirParts::Smooth(Real, 0.33)).GetSafeNormal();
			FVector Flat(CloseAhead.X, CloseAhead.Y, 0.0);
			Flat = Flat.IsNearlyZero() ? FVector(Ahead.X, Ahead.Y, 0.0).GetSafeNormal() : Flat.GetSafeNormal();
			const FVector FlatRight = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal();
			FVector Want, Look;
			if (Rider->bFighter)
			{
				// A fighter from 22 m behind along its path and 6 m above it, looking 80 m past it: the aircraft a little
				// under the middle of the picture, its back, its fin and the tops of its wings toward the eye.
				Want = Body - CloseAhead * 22.0 * U + FVector::UpVector * 6.0 * U;
				Look = Body + CloseAhead * 80.0 * U;
			}
			else
			{
				// A bomber from behind its tail and above it, a little out on the side the attack comes from, looking
				// a little past its nose: the whole bomber in the lower middle of the picture, both wingtips inside it,
				// the sky its attackers come through beyond it.
				const FVector Toward = Foe ? (World(Foe->At.X, Foe->At.Y, Foe->At.Z) - Body).GetSafeNormal() : FlatRight;
				const double Hand = FVector::DotProduct(Toward, FlatRight) >= 0.0 ? 1.0 : -1.0;
				const double Size = FMath::Max(12.0, Rider->Span);
				Want = Body - CloseAhead * Size * 1.8 * U + FlatRight * Hand * Size * 0.55 * U + FVector::UpVector * Size * 0.45 * U;
				Look = Body + CloseAhead * Size * 1.2 * U;
			}
			CloseEye = Want;
			FRotator Facing = (Look - CloseEye).Rotation();
			Facing.Roll = FMath::Clamp(Rider->Bank * 0.25, -15.0, 15.0);
			CloseFacing = bCut ? Facing : FMath::RInterpTo(CloseFacing, Facing, static_cast<float>(Real), 6.0f);
			Eye->SetActorLocationAndRotation(CloseEye, CloseFacing);
			Lens->SetFieldOfView(62.0f);
			Lens->PostProcessSettings.bOverride_MotionBlurAmount = true;
			Lens->PostProcessSettings.MotionBlurAmount = 0.12f;
			return;
		}
		S.bClose = false;
	}
	Lens->SetFieldOfView(55.0f);
	Lens->PostProcessSettings.MotionBlurAmount = 0.0f;
	// The strategy camera: on the group in hand, else the fight, else his own raid; turned by dragging, drawn in and out by the wheel.
	AirShow::Vec Focus(S.Board.TargetX, S.Board.TargetY, 3000.0);
	int Count = 0;
	AirShow::Vec Sum;
	for (const auto& Pair : S.Show.Groups())
	{
		const AirShow::Group& G = Pair.second;
		const bool bInHand = std::find(S.Hand.begin(), S.Hand.end(), G.Id) != S.Hand.end();
		if (G.Doing == AirShow::Act::Down || G.Doing == AirShow::Act::Idle)
		{
			continue;
		}
		if (!S.Hand.empty() ? bInHand : G.Doing == AirShow::Act::Fight)
		{
			Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt);
			++Count;
		}
	}
	if (Count == 0)
	{
		for (const auto& Pair : S.Show.Groups())
		{
			const AirShow::Group& G = Pair.second;
			if (G.Side == S.Desk.MySideIndex() && !G.bFighter && G.Doing != AirShow::Act::Down)
			{
				Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt);
				++Count;
			}
		}
	}
	if (Count > 0)
	{
		Focus = Sum * (1.0 / Count);
	}
	FVector Wanted = World(Focus.X, Focus.Y, Focus.Z);
	double Away = Distance, Down = Pitch, Heading = Yaw;
	// "The ships" picture: the overview cut to the ship that went down, from 1.6 km off and above, her neighbours and
	// the water round her in the frame (the art director's note: the first run framed the hills, no ship in sight).
	FVector ShipWorld;
	const bool bOnShip = ShipsLook >= 0 && Stage && Stage->IsRaised() && Stage->ShipAt(ShipsLook, ShipWorld);
	if (bOnShip)
	{
		Wanted = ShipWorld;
		Away = 1600.0;
		Down = -24.0;
		// At sea (the art director on build 1734, Midway 06: the carrier a small soft shape in a teal field): she fills
		// about a third of the picture's width, through a longer lens (35 degrees) from low enough that her smoke stands
		// up above her; the look a little over her waterline, so that she sits under the middle with her column over her.
		// She is seen from upwind (the art director on build 1760: from downwind her column leaned at the eye, its
		// nearest puffs huge and soft): the look 55 degrees off the wind's way, so her smoke streams away and across the
		// picture, of the two such eyes the one nearer her beam (she shows more of her length), and on a near tie her
		// sunlit side. In harbour the wide picture stands (Pearl Harbor 06: its columns across the harbour are what it is
		// for), its eye upwind and clear of them (below).
		double Long = 0.0;
		FVector Bow;
		bool bHarbour = true;
		if (Stage->ShipLie(ShipsLook, Long, Bow, bHarbour) && !bHarbour)
		{
			const double LensDegrees = 35.0;
			const FVector Abeam = FVector::CrossProduct(FVector::UpVector, Bow).GetSafeNormal();
			FVector Sun = Stage->TowardSun();
			Sun.Z = 0.0;
			Sun = Sun.GetSafeNormal();
			FVector Downwind = WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0).GetSafeNormal();
			if (Downwind.IsNearlyZero())
			{
				Downwind = -Abeam;
			}
			FVector Out = Abeam;
			double Best = -1e9;
			for (const double Turn : { 55.0, -55.0 })
			{
				// (the eye's way from her: upwind, turned Turn degrees, so the look runs that far off the wind's way)
				const FVector Way = (-Downwind).RotateAngleAxis(Turn, FVector::UpVector);
				const double Score = FMath::Abs(FVector::DotProduct(Way, Abeam)) + 0.25 * FMath::Max(0.0, FVector::DotProduct(Way, Sun));
				if (Score > Best)
				{
					Best = Score;
					Out = Way;
				}
			}
			// (seen at an angle off her length she shows that much of it, never less than a third; the frame is
			// 2 tan(lens/2) of the way wide)
			const double Shown = FMath::Max(FVector::CrossProduct(Out, Bow.GetSafeNormal()).Size(), 0.33) * Long;
			Away = FMath::Clamp(3.0 * Shown / (2.0 * FMath::Tan(FMath::DegreesToRadians(LensDegrees * 0.5))), 450.0, 1600.0);
			// (9 degrees down: the frame's top edge a little over the horizon, so her column stands against the sky and
			// not the sea; at 13 the top lay 2 to 3 degrees under it. The pre-check and the art director, 18:47Z)
			Down = -9.0;
			Heading = (-Out).Rotation().Yaw;
			Wanted = ShipWorld + FVector::UpVector * (0.12 * Long * U);
			Lens->SetFieldOfView(static_cast<float>(LensDegrees));
		}
		else if (Long > 0.0)
		{
			// In harbour (the art director on builds 1766 and 1770, Pearl Harbor 06: the eye stood inside a ship's
			// smoke column, the frame brown-black murk with almost no ship). The eye stands upwind of the row, 55 degrees
			// off the wind, so the columns lean away from it; at least 300 m clear of every column that stands or may
			// stand over the ships struck, sunk or moored beside her, from the picture's start to its end (chosen once,
			// held); and where the columns' spread leaves no such eye at 1.6 km, back and up until there is one. The
			// smoke is never thinned for it: the oil columns stay out of the near eye's fade (the effects rule).
			if (ShipsEyeFor != ShipsLook)
			{
				ShipsEyeFor = ShipsLook;
				const FVector Abeam = FVector::CrossProduct(FVector::UpVector, Bow).GetSafeNormal();
				FVector Sun = Stage->TowardSun();
				Sun.Z = 0.0;
				Sun = Sun.GetSafeNormal();
				FVector Downwind = WorldVector(AirSky::Wind.X, AirSky::Wind.Y, 0.0).GetSafeNormal();
				if (Downwind.IsNearlyZero())
				{
					Downwind = -Abeam;
				}
				TArray<FVector> Feet;
				Stage->SmokeFeet(ShipsLook, 800.0, Feet);
				// (a column: from its foot up 1.2 km, leaning downwind as it rises; its width grows with its height)
				auto Clear = [&Feet, &Downwind, U](const FVector& Eye_) -> double
				{
					double Least = 1e12;
					for (const FVector& Foot : Feet)
					{
						const FVector Top = Foot + (FVector::UpVector + Downwind * 0.8) * (1200.0 * U);
						const FVector Nearest = FMath::ClosestPointOnSegment(Eye_, Foot, Top);
						const double Up = (Nearest.Z - Foot.Z) / U;
						Least = FMath::Min(Least, FVector::Dist(Eye_, Nearest) / U - 0.12 * Up);
					}
					return Least;
				};
				// (the ways the eye may stand, upwind of the row and off the wind, best first: the one that shows her
				// broadside and is lit from behind the eye, then the others, nearer the wind and farther off it)
				TArray<TPair<double, FVector>> Ways;
				for (const double Turn : { 55.0, -55.0, 35.0, -35.0, 75.0, -75.0 })
				{
					const FVector Way = (-Downwind).RotateAngleAxis(Turn, FVector::UpVector);
					const double Score = FMath::Abs(FVector::DotProduct(Way, Abeam)) + 0.25 * FMath::Max(0.0, FVector::DotProduct(Way, Sun)) - 0.004 * (FMath::Abs(Turn) - 55.0) * (FMath::Abs(Turn) - 55.0) / 20.0;
					Ways.Add(TPair<double, FVector>(Score, Way));
				}
				Ways.Sort([](const TPair<double, FVector>& A, const TPair<double, FVector>& B) { return A.Key > B.Key; });
				// (clear of every column by 700 m, or by 0.35 of the eye's way to her, whichever is more: at 300 m on build
				// 1778 a near column out of focus still filled 45 per cent of the picture, the art director 17:54Z: 60 per
				// cent of it clear, no column inside the near eye's fade. The nearest eye that is, any of the ways)
				ShipsEyeHeading = (-Ways[0].Value).Rotation().Yaw;
				ShipsEyeAway = 1600.0;
				ShipsEyeDown = -24.0;
				bool bFound = false;
				for (int32 Step = 0; Step <= 16 && !bFound; ++Step)
				{
					const double Away_ = 1600.0 + Step * 250.0, Down_ = -24.0 - Step * 1.25;
					for (const TPair<double, FVector>& Way : Ways)
					{
						const double Heading_ = (-Way.Value).Rotation().Yaw;
						const FVector Eye_ = ShipWorld - FRotator(Down_, Heading_, 0.0).Vector() * Away_ * U;
						if (Clear(Eye_) >= FMath::Max(700.0, 0.35 * Away_))
						{
							ShipsEyeHeading = Heading_;
							ShipsEyeAway = Away_;
							ShipsEyeDown = Down_;
							bFound = true;
							break;
						}
					}
				}
				if (!bFound)
				{
					// (none clear: the farthest and steepest of the best way)
					ShipsEyeAway = 1600.0 + 16 * 250.0;
					ShipsEyeDown = -24.0 - 16 * 1.25;
				}
				UE_LOG(LogTheater, Display, TEXT("Air battle: the ships picture in harbour, %d columns %s: %.0f m off, %.1f degrees down, heading %.0f"), Feet.Num(),
					bFound ? TEXT("kept clear") : TEXT("not all kept clear"), ShipsEyeAway, -ShipsEyeDown, ShipsEyeHeading);
			}
			Away = ShipsEyeAway;
			Down = ShipsEyeDown;
			Heading = ShipsEyeHeading;
		}
	}
	LookAt = bLookSet && !bOnShip ? FMath::Lerp(LookAt, Wanted, AirParts::Smooth(Real, 1.2)) : Wanted;
	bLookSet = true;
	const FRotator Facing(Down, Heading, 0.0);
	FVector From = LookAt - Facing.Vector() * Away * U;
	if (Stage && Stage->IsRaised())
	{
		From.Z = FMath::Max(From.Z, Stage->GroundZ(From) + 40.0 * U);
	}
	From.Z = FMath::Max(From.Z, 40.0 * U);
	Eye->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	if (S.bFlying && S.GlideT >= 0.0)
	{
		// The glide from the flyover's last eye into the strategy view, eased in and out; the film's frame draws back.
		S.GlideT += Real;
		const double Into = FMath::Clamp(S.GlideT / FMath::Max(S.GlideLong, 0.1), 0.0, 1.0);
		const double Eased = Into * Into * (3.0 - 2.0 * Into);
		S.FlyBars = 1.0 - Eased;
		if (Into >= 1.0)
		{
			S.bFlying = false;
			S.FlyBars = 0.0;
			S.FlyEndRealT = RealT;
		}
		else
		{
			const FQuat Toward = FQuat::Slerp(S.GlideFacing.Quaternion(), (LookAt - From).Rotation().Quaternion(), Eased);
			Eye->SetActorLocationAndRotation(FMath::Lerp(S.GlideEye, From, Eased), Toward.Rotator());
			Lens->SetFieldOfView(static_cast<float>(FMath::Lerp(AirDesk::Flyover::Lens, 55.0, Eased)));
		}
	}
}

void AAirBattle::Orbit(const FVector2D& DeltaPixels)
{
	Yaw += DeltaPixels.X * 0.25;
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * 0.2, -80.0, 10.0);
}

void AAirBattle::Zoom(float Steps)
{
	Distance = FMath::Clamp(Distance * FMath::Pow(0.85, static_cast<double>(Steps)), 120.0, 20000.0);
}

void AAirBattle::PlaceTags()
{
	FAirBattleState& S = *State;
	S.TagAt.clear();
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		S.ViewportPixels = Size;
	}
	// Each tag's place over its formation. Groups never stack (b384(d)): the screen, which knows how wide each tag is
	// drawn, gathers tags that would touch into one block (SAirBattleScreen::OnPaint).
	for (const AirDesk::Tag& T : S.Words.Tags)
	{
		AirShow::Vec Middle;
		double Face = 0.0;
		if (T.bGone || !S.Show.Middle(T.Id, Middle, Face))
		{
			continue;
		}
		FVector2D OnScreen;
		if (Controller->ProjectWorldLocationToScreen(World(Middle.X, Middle.Y, Middle.Z + 80.0), OnScreen, false)
			&& OnScreen.X >= 0.0 && OnScreen.Y >= 0.0 && OnScreen.X <= S.ViewportPixels.X && OnScreen.Y <= S.ViewportPixels.Y)
		{
			S.TagAt[T.Id] = OnScreen;
		}
	}
}

int32 AAirBattle::TagUnder(const FVector2D& Pixel) const
{
	const FAirBattleState& S = *State;
	// A tag as drawn (a block's last line is its first tag's); else the formation nearest the click.
	for (auto Box = S.TagBoxes.rbegin(); Box != S.TagBoxes.rend(); ++Box)
	{
		if (S.TagAt.count(Box->Id) && Pixel.X >= Box->Min.X && Pixel.X <= Box->Max.X && Pixel.Y >= Box->Min.Y && Pixel.Y <= Box->Max.Y)
		{
			return Box->Id;
		}
	}
	int32 Best = -1;
	double BestD = 60.0;
	for (const auto& Pair : S.TagAt)
	{
		const double D = FVector2D::Distance(Pair.second, Pixel);
		if (D < BestD)
		{
			BestD = D;
			Best = Pair.first;
		}
	}
	return Best;
}

bool AAirBattle::BoardUnder(const FVector2D& Pixel, double Height, FVector2D& OutBoard) const
{
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector From, Way;
	if (!Controller || !Controller->DeprojectScreenPositionToWorld(static_cast<float>(Pixel.X), static_cast<float>(Pixel.Y), From, Way) || FMath::Abs(Way.Z) < 1e-4)
	{
		return false;
	}
	const double Along = (Height * UnitsPerMetre - From.Z) / Way.Z;
	if (Along <= 0.0)
	{
		return false;
	}
	const FVector Hit = From + Way * Along;
	OutBoard = FVector2D(Hit.Y / UnitsPerMetre, Hit.X / UnitsPerMetre);
	return true;
}

void AAirBattle::PickAt(const FVector2D& Pixel, bool bAdd)
{
	FAirBattleState& S = *State;
	const int32 Id = TagUnder(Pixel);
	const AirDesk::Tag* Tag = Id >= 0 ? S.TagOf(Id) : nullptr;
	if (!bAdd)
	{
		S.Hand.clear();
	}
	if (Tag && Tag->bMine && std::find(S.Hand.begin(), S.Hand.end(), Id) == S.Hand.end())
	{
		S.Hand.push_back(Id);
	}
}

void AAirBattle::OrderAt(const FVector2D& Pixel)
{
	FAirBattleState& S = *State;
	if (S.Hand.empty())
	{
		return;
	}
	const int32 Id = TagUnder(Pixel);
	const AirDesk::Tag* Tag = Id >= 0 ? S.TagOf(Id) : nullptr;
	if (Tag && !Tag->bMine)
	{
		Order(AirParts::Wide(S.Desk.Attack(S.Hand, Id)));
		return;
	}
	AirShow::Vec Middle;
	double Face = 0.0;
	const double Height = S.Show.Middle(S.Hand.front(), Middle, Face) ? Middle.Z : 4500.0;
	FVector2D Board;
	if (BoardUnder(Pixel, Height, Board))
	{
		Order(AirParts::Wide(S.Desk.Move(S.Hand, Board.X, Board.Y)));
	}
}

bool AAirBattle::Order(const FString& Command)
{
	FAirBattleState& S = *State;
	if (Command.IsEmpty())
	{
		return false;
	}
	if (S.bReplay || !Session)
	{
		S.Note = "A recorded battle: it plays as it was fought.";
		S.NoteAge = 0.0;
		return false;
	}
	FString Reply, Error;
	if (!Session->CallBattle(TEXT("battleOrder"), TEXT("[") + Command + TEXT("]"), Reply, Error))
	{
		UE_LOG(LogTheater, Warning, TEXT("Air battle: TC.battleOrder failed: %s"), *Error);
		return false;
	}
	if (!Reply.Contains(TEXT("true")))
	{
		S.Note = "That order cannot be given now.";
		S.NoteAge = 0.0;
		return false;
	}
	CampaignSounds::Play(this, TEXT("stamp"));
	return true;
}

void AAirBattle::Press(const FString& Button)
{
	FAirBattleState& S = *State;
	if (Button == TEXT("skip"))
	{
		// (a click or a key during the flyover: its shots end and a short glide takes the eye to the battle)
		if (S.bFlying && S.GlideT < 0.0)
		{
			S.GlideT = 0.0;
			S.GlideLong = 0.8;
			bLookSet = false;
		}
		return;
	}
	if (S.bCard)
	{
		if (Button == TEXT("continue"))
		{
			Finish();
		}
		return;
	}
	if (Button == TEXT("ride"))
	{
		S.bClose = !S.bClose;
		bRideOther = bRideOther && S.bClose; // (a ride taken off ends a ride of the other side's; the next is his own)
		S.Subject = AirDesk::Subject();
		CloseEye = FVector::ZeroVector;
		return;
	}
	if (S.Question != 0)
	{
		return;
	}
	const std::vector<int>& Hand = S.Hand;
	if (Button == TEXT("hold")) { Order(AirParts::Wide(S.Desk.Hold(Hand))); }
	else if (Button == TEXT("breakoff")) { Order(AirParts::Wide(S.Desk.BreakOff(Hand))); }
	else if (Button == TEXT("breakoffall")) { S.Question = 2; }
	else if (Button == TEXT("staff")) { S.Question = 1; }
	else if (Button == TEXT("hunt")) { Order(AirParts::Wide(S.Desk.Hunt(Hand))); }
	else if (Button == TEXT("jettison")) { Order(AirParts::Wide(S.Desk.Jettison(Hand))); }
	else if (Button == TEXT("low") || Button == TEXT("middle") || Button == TEXT("high"))
	{
		Order(AirParts::Wide(S.Desk.Band(Hand, Button == TEXT("low") ? 0 : Button == TEXT("middle") ? 1 : 2)));
	}
	else if (Button == TEXT("close") || Button == TEXT("ahead"))
	{
		// STAY CLOSE / RANGE AHEAD: over the nearest of his own bomber boxes.
		AirShow::Vec Middle;
		double Face = 0.0;
		int Box = -1;
		double Best = 1e18;
		if (!Hand.empty() && S.Show.Middle(Hand.front(), Middle, Face))
		{
			for (const int Id : S.MineOf(false))
			{
				AirShow::Vec Other;
				double OtherFace = 0.0;
				if (S.Show.Middle(Id, Other, OtherFace))
				{
					const double D = (Other.X - Middle.X) * (Other.X - Middle.X) + (Other.Y - Middle.Y) * (Other.Y - Middle.Y);
					if (D < Best)
					{
						Best = D;
						Box = Id;
					}
				}
			}
		}
		Order(AirParts::Wide(S.Desk.Escort(Hand, Box, Button == TEXT("close"))));
	}
}

void AAirBattle::SetSpeed(double Speed)
{
	FAirBattleState& S = *State;
	S.Clock.Speed = Speed;
}

void AAirBattle::Answer(bool bYes)
{
	FAirBattleState& S = *State;
	const int Asked = S.Question;
	S.Question = 0;
	if (!bYes)
	{
		return;
	}
	Order(AirParts::Wide(Asked == 1 ? S.Desk.HandOver() : S.Desk.BreakOffAll()));
}

void AAirBattle::Finish()
{
	// CONTINUE: the scene is taken down, the table shown again; the campaign's own after-battle steps are the HUD's.
	HideScreen();
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		AirParts::ShowFrame(Cast<ACampaignHUD>(Controller->GetHUD()), true);
		Controller->SetViewTarget(Controller->GetPawn());
	}
	ShowTheTable();
	Destroy();
}

void AAirBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	SkyFinish(false); // (a turn of the clouds painted aside: done before the stage and the meshes it touches go)
	HideScreen();
	if (KeptScreenPercent > 0.0f)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			Variable->Set(KeptScreenPercent, ECVF_SetByCode);
		}
		KeptScreenPercent = -1.0f;
	}
	if (Reason == EEndPlayReason::Destroyed)
	{
		ShowTheTable(); // (taken down without CONTINUE: the table still comes back as it was)
	}
	for (TPair<int32, TObjectPtr<UAudioComponent>>& Pair : Engines)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	if (Effects)
	{
		Effects->End();
		Effects.Reset();
	}
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	if (GroundFx)
	{
		GroundFx->End();
		GroundFx.Reset();
	}
	GroundFxMesh.Reset();
	AirFxMesh.Reset();
	if (Eye)
	{
		Eye->Destroy();
		Eye = nullptr;
	}
	Super::EndPlay(Reason);
}

void AAirBattle::ShowScreen()
{
	if (!GEngine || !GEngine->GameViewport || Screen.IsValid())
	{
		return;
	}
	Screen = SNew(SAirBattleScreen).Battle(this);
	// The words and the name tags a quarter larger than Unreal's own game-screen size, which already grows with the
	// window's height (the art director on build 1734: they were small at Brian's window; 1.3 at 1440 high and 1.24 at
	// his, so one number between). Everything on the screen grows together, so the dark plates still fit their words.
	// "-TheaterAirTextScale=" tunes it.
	float Lettering = 1.25f;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterAirTextScale="), Lettering);
	Lettering = FMath::Clamp(Lettering, 0.75f, 2.0f);
	ScreenFrame = SNew(SDPIScaler).DPIScale(Lettering)[Screen.ToSharedRef()];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenFrame.ToSharedRef(), 20);
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Screen);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->bShowMouseCursor = ShotFolder.IsEmpty();
		bOwnInput = true;
	}
}

void AAirBattle::HideScreen()
{
	if (ScreenFrame.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenFrame.ToSharedRef());
	}
	ScreenFrame.Reset();
	Screen.Reset();
	if (bOwnInput)
	{
		bOwnInput = false;
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			// (the campaign's own mode: the pointer stays in sight while a drag turns the table, and is never locked in)
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
	}
}

// ---- Sound -----------------------------------------------------------------------------------------------------

USoundBase* AAirBattle::SoundOf(const TCHAR* Name, bool bLoop)
{
	const FName Key(Name);
	if (TObjectPtr<USoundBase>* Found = Sounds.Find(Key))
	{
		return Found->Get();
	}
	if (SoundsMissing.Contains(Key))
	{
		return nullptr;
	}
	// The battle pack's recordings, brought in by Tools/build_sounds.sh from Sounds/Real to /Game/Campaign/Sounds/Real.
	USoundBase* Made = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Made)
	{
		SoundsMissing.Add(Key);
		UE_LOG(LogTheater, Display, TEXT("Air battle: the sound %s is not in /Game/Campaign/Sounds/Real (Tools/build_sounds.sh brings it in)"), Name);
		return nullptr;
	}
	if (bLoop)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Made))
		{
			Wave->bLooping = true;
		}
	}
	Sounds.Add(Key, Made);
	return Made;
}

USoundAttenuation* AAirBattle::FamilyOf(const TCHAR* Name)
{
	const FName Key(Name);
	if (TObjectPtr<USoundAttenuation>* Found = Families.Find(Key))
	{
		return Found->Get();
	}
	// A distance family of the mix (mix.json distance): full loudness to `full` metres, nothing past `silent`, the top
	// taken off with distance by its lpf points.
	const TSharedPtr<FJsonObject> Family = AirParts::MixObject(Mix, FString(TEXT("distance.")) + Name);
	if (!Family.IsValid())
	{
		return nullptr;
	}
	double Full = 50.0, Silent = 1500.0;
	Family->TryGetNumberField(TEXT("full"), Full);
	Family->TryGetNumberField(TEXT("silent"), Silent);
	USoundAttenuation* Made = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Made->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(Full * UnitsPerMetre, 0.0, 0.0);
	A.FalloffDistance = static_cast<float>(FMath::Max(1.0, Silent - Full) * UnitsPerMetre);
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	const TArray<TSharedPtr<FJsonValue>>* Lpf = nullptr;
	if (Family->TryGetArrayField(TEXT("lpf"), Lpf) && Lpf->Num() >= 2)
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
	Families.Add(Key, Made);
	return Made;
}

UAudioComponent* AAirBattle::Sound(const TCHAR* Name, const FVector& Where, const TCHAR* Family, float Volume, float After, float Cut)
{
	if (After > 0.0f)
	{
		Later.Add({ RealT + After, FName(Name), Where, FName(Family), Volume, Cut });
		return nullptr;
	}
	USoundBase* Played = SoundOf(Name);
	if (!Played)
	{
		return nullptr;
	}
	UAudioComponent* Player = UGameplayStatics::SpawnSoundAtLocation(this, Played, Where, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, FamilyOf(Family));
	if (Player && Cut > 0.0f)
	{
		// (only the first part of the take: the rest is the ground's echo, which the sky does not have)
		TWeakObjectPtr<UAudioComponent> Weak(Player);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (Weak.IsValid())
			{
				Weak->FadeOut(0.08f, 0.0f);
			}
		}), Cut, false);
	}
	return Player;
}

void AAirBattle::Play(double Real)
{
	(void)Real;
	FAirBattleState& S = *State;
	// The sounds held for their moment (a flak burst heard after its flash).
	for (int32 K = Later.Num() - 1; K >= 0; --K)
	{
		if (RealT >= Later[K].At)
		{
			const FLater Due = Later[K];
			Later.RemoveAt(K);
			Sound(*Due.Name.ToString(), Due.Where, *Due.Family.ToString(), Due.Volume, 0.0f, Due.Cut);
		}
	}
	EngineSounds();
	const FVector Listener = Eye ? Eye->GetActorLocation() : FVector::ZeroVector;
	// The flak guns on the ground while a box is in their ring (mix.json air.flak: a gun every `every` seconds).
	const TSharedPtr<FJsonObject> Flak = AirParts::MixObject(Mix, TEXT("air.flak"));
	bool bInRing = false;
	for (const auto& Pair : S.Show.Groups())
	{
		if (!Pair.second.Flak.empty())
		{
			bInRing = true;
			break;
		}
	}
	if (bInRing && Flak.IsValid() && RealT >= FlakGunsAt)
	{
		double Low = 0.8, High = 2.5;
		const TArray<TSharedPtr<FJsonValue>>* Every = nullptr;
		if (Flak->TryGetArrayField(TEXT("every"), Every) && Every->Num() == 2)
		{
			Low = (*Every)[0]->AsNumber();
			High = (*Every)[1]->AsNumber();
		}
		FlakGunsAt = RealT + FMath::FRandRange(Low, High);
		const FString Gun = AirParts::MixTake(Flak, TEXT("fire"), FMath::RandRange(0, 9));
		FString FireFamily = TEXT("big_guns");
		Flak->TryGetStringField(TEXT("fireFamily"), FireFamily);
		const FVector Battery = World(S.Board.TargetX + FMath::FRandRange(-3000.0, 3000.0), S.Board.TargetY + FMath::FRandRange(-3000.0, 3000.0), 10.0);
		Sound(*Gun, Battery, *FireFamily, 1.0f);
	}
	// A fighter group passing within 300 m of the listener: one pass take for the group, the nearest plane's (mix.json air.passes).
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		if (!P.bFighter || !P.bShown || P.bFalling)
		{
			continue;
		}
		const FVector At = World(P.At.X, P.At.Y, P.At.Z);
		if (FVector::Dist(At, Listener) > 300.0 * UnitsPerMetre)
		{
			continue;
		}
		const double* Heard = PassHeardAt.Find(P.Group);
		if (Heard && RealT - *Heard < 6.0)
		{
			continue;
		}
		PassHeardAt.Add(P.Group, RealT);
		Sound(TEXT("fighter_pass"), At, TEXT("aircraft"), 1.2f);
	}
}

void AAirBattle::EngineSounds()
{
	FAirBattleState& S = *State;
	const TSharedPtr<FJsonObject> Loops = AirParts::MixObject(Mix, TEXT("air.loops"));
	if (!Loops.IsValid())
	{
		return;
	}
	// Each formation's own engines at its middle, by its model (mix.json air.loops, SKY.cues.engines): the B-17 its
	// own drone; four-engined and twin-engined bombers theirs; every fighter, and the carriers' single-engined
	// bombers, which have no recording of their own, the fighter's.
	for (const auto& Pair : S.Show.Groups())
	{
		const AirShow::Group& G = Pair.second;
		TObjectPtr<UAudioComponent>* Have = Engines.Find(G.Id);
		AirShow::Vec Middle;
		double Face = 0.0;
		const bool bFlying = G.Doing != AirShow::Act::Down && G.Doing != AirShow::Act::Idle && S.Show.Middle(G.Id, Middle, Face);
		if (!bFlying)
		{
			if (Have && *Have)
			{
				(*Have)->FadeOut(2.0f, 0.0f);
				Engines.Remove(G.Id);
				EngineKind.Remove(G.Id);
			}
			continue;
		}
		const bool bFour = G.Model == "Lancaster" || G.Model == "Halifax" || G.Model == "B24";
		const bool bTwin = G.Model == "He111" || G.Model == "Betty" || G.Model == "G4M" || G.Model == "Bomber";
		const TCHAR* Kind = G.Model == "B17" ? TEXT("B-17") : bFour ? TEXT("fourEngined") : bTwin ? TEXT("twinEngined") : TEXT("fighter");
		if (Have)
		{
			const FName* Was = EngineKind.Find(G.Id);
			if (!Was || *Was != FName(Kind))
			{
				// (a group first seen with the place's stand-in model, its own named since: its own engines)
				if (*Have)
				{
					(*Have)->FadeOut(1.0f, 0.0f);
				}
				Engines.Remove(G.Id);
				EngineKind.Remove(G.Id);
				Have = nullptr;
			}
		}
		if (!Have)
		{
			EngineKind.Add(G.Id, FName(Kind));
			FString Loop;
			Loops->TryGetStringField(Kind, Loop);
			USoundBase* Played = Loop.IsEmpty() ? nullptr : SoundOf(*Loop, true);
			if (!Played)
			{
				Engines.Add(G.Id, nullptr);
				continue;
			}
			const float Loud = static_cast<float>(FMath::Pow(10.0, AirParts::MixNumber(Mix, TEXT("air.groupLoopDb"), 0.0) / 20.0));
			UAudioComponent* Player = UGameplayStatics::SpawnSoundAtLocation(this, Played, World(Middle.X, Middle.Y, Middle.Z), FRotator::ZeroRotator, Loud,
				G.bFighter ? 1.0f : 0.97f + 0.06f * static_cast<float>(G.Id % 3) / 2.0f, 0.0f, FamilyOf(TEXT("aircraft")), nullptr, false);
			Engines.Add(G.Id, Player);
			continue;
		}
		if (*Have)
		{
			(*Have)->SetWorldLocation(World(Middle.X, Middle.Y, Middle.Z));
		}
	}
}

// ---- The picture run -------------------------------------------------------------------------------------------

void AAirBattle::TakeShots(double Real)
{
	(void)Real;
	if (ShotFolder.IsEmpty() || QuitAt >= 0.0)
	{
		return;
	}
	FAirBattleState& S = *State;
	// The flyover's own pictures, one in each of its shots, on its own clock.
	if (S.bFlying && S.GlideT < 0.0)
	{
		const double RaidSeconds = AirDesk::Flyover::RaidLong, TargetSeconds = AirDesk::Flyover::TargetLong;
		const struct { const TCHAR* Name; double At; bool bGuard; } FlyShots[] = {
			{ TEXT("00-flyover-raid"), 3.0, false }, { TEXT("00-flyover-target"), RaidSeconds + 2.5, false }, { TEXT("00-flyover-guard"), RaidSeconds + TargetSeconds + 2.0, true } };
		for (int32 K = 0; K < 3; ++K)
		{
			if ((S.FlyPictures & (1 << K)) == 0 && S.FlyT >= FlyShots[K].At && (!FlyShots[K].bGuard || S.Fly.GuardGroup() >= 0))
			{
				S.FlyPictures |= 1 << K;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + FlyShots[K].Name + TEXT(".png")), true, false);
				++ShotsTaken;
			}
		}
	}
	for (FShot& Shot : Shots)
	{
		if (Shot.bTaken)
		{
			continue;
		}
		const double Base = Shot.bShips ? ShipRealT : MetRealT;
		if (Base < 0.0)
		{
			// (the approach is taken before the formations meet: the clock's x10 brings them together in some 20 s;
			// after the flyover, when there is one)
			if (Shot.At < 0.0 && !Shot.bShips && !S.bFlying && RealT >= FMath::Max(8.0, S.FlyEndRealT + 4.0))
			{
				Shot.bTaken = true;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
				++ShotsTaken;
			}
			continue;
		}
		const double Due = Base + Shot.At;
		if (Shot.bClose && ShipsLook >= 0 && RealT >= Due - 1.5)
		{
			// (the ships picture holds the eye: a ride picture due now waits until two seconds after it)
			Shot.At = RealT + 2.0 - Base;
			continue;
		}
		if (Shot.bClose && !S.bClose && RealT >= Due - 1.5)
		{
			bRideOther = Shot.bOther;
			Press(TEXT("ride"));
		}
		if (Shot.bShips && ShipsLook < 0 && FirstSunk >= 0 && RealT >= Due - 2.0)
		{
			if (S.bClose)
			{
				Press(TEXT("ride"));
				CloseOffAt = -1.0; // (the ride is off: its old time must not take the next ride off a tick after it begins)
			}
			ShipsLook = FirstSunk;
		}
		if (RealT >= Due)
		{
			Shot.bTaken = true;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
			++ShotsTaken;
			if (Shot.bClose)
			{
				CloseOffAt = RealT + RideHold;
			}
			if (Shot.bShips)
			{
				ShipsLookOffAt = RealT + 0.5;
			}
		}
	}
	if (ShipsLookOffAt >= 0.0 && RealT >= ShipsLookOffAt)
	{
		ShipsLookOffAt = -1.0;
		ShipsLook = -1;
		ShipsEyeFor = -1;
		bLookSet = false; // (a cut back to the fight)
	}
	if (CloseOffAt >= 0.0 && RealT >= CloseOffAt)
	{
		CloseOffAt = -1.0;
		if (S.bClose)
		{
			Press(TEXT("ride"));
		}
	}
	// The after card, then quit (or once the battle has run long past the last picture).
	if (S.bCard && !bCardShot)
	{
		bCardShot = true;
		CardShotAt = RealT + 1.0;
	}
	if (bCardShot && CardShotAt >= 0.0 && RealT >= CardShotAt)
	{
		CardShotAt = -1.0;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-08-after-card.png")), true, false);
		++ShotsTaken;
		QuitAt = RealT + 2.0;
	}
	if (RealT > 1500.0)
	{
		QuitAt = RealT + 1.0;
	}
}
