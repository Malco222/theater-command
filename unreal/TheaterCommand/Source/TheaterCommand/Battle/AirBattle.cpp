#include "Battle/AirBattle.h"

#include "Battle/AirBattleScreen.h"
#include "Battle/AirBattleState.h"
#include "Battle/AirStage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
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

#include <algorithm>
#include <cmath>

namespace AirParts
{
	static FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
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
	}
	// The effects: the kit's own, drawn out to the air distance, on the air clock.
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
		AirParts::UseAirDistance(Effects.Get());
		Effects->SetWind(FVector(-2.0, 3.0, 0.0) * UnitsPerMetre / 100.0);
	}
	Raise();
	// The flight core's ground: the stage's hills, or the sea.
	if (Stage && Stage->IsRaised())
	{
		TWeakObjectPtr<AAirStage> Under(Stage);
		const double U = UnitsPerMetre;
		S.Show.Ground = [Under, U](double X, double Y)
		{
			return Under.IsValid() ? FMath::Max(0.0, Under->GroundZ(FVector(Y * U, X * U, 0.0)) / U) : 0.0;
		};
	}
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
		Eye->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Eye->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
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
			{ TEXT("05-ride-again"), 70.0, true, false, false },
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
			FVector2D(S.Board.TargetX, S.Board.TargetY), UnitsPerMetre, Effects.Get()))
		{
			if (Effects)
			{
				Effects->SetSun(Stage->TowardSun());
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
	if (Effects)
	{
		// (the hour's sun: mid-morning, from the south-east, as the campaign's daylight raids)
		Effects->SetSun(FRotator(30.0, 150.0, 0.0).Vector());
	}
}

void AAirBattle::LayGround()
{
	// A battle with no stage: the sea far past the board, the board's land laid flat on it (place.ground.land, rings of
	// board points), and the target's outline. The land is cut into triangles by ear clipping.
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
			V.Add(World(Point.first, Point.second, 2.0));
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
}

void AAirBattle::HideTheTable()
{
	// The table and its armies are put out of sight; the air and the light are the battle's.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			It->SetActorHiddenInGame(true);
		}
		if (ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It))
		{
			Environment->SetActorTickEnabled(false);
			// Haze from 2 km (the art director, 19 November 04:40Z): a clear sky over the sea, the far formations soft.
			TArray<UExponentialHeightFogComponent*> Fogs;
			Environment->GetComponents(Fogs);
			for (UExponentialHeightFogComponent* Fog : Fogs)
			{
				Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));
				Fog->SetFogDensity(0.0005f);
				Fog->SetFogHeightFalloff(0.004f);
				Fog->SetFogMaxOpacity(0.8f);
				Fog->SetFogInscatteringColor(FLinearColor(0.55f, 0.66f, 0.78f));
			}
			TArray<UDirectionalLightComponent*> Lights;
			Environment->GetComponents(Lights);
			UDirectionalLightComponent* Brightest = nullptr;
			for (UDirectionalLightComponent* Light : Lights)
			{
				if (!Brightest || Light->Intensity > Brightest->Intensity)
				{
					Brightest = Light;
				}
			}
			if (Brightest)
			{
				const FVector Sun = Stage && Stage->IsRaised() ? Stage->TowardSun() : FRotator(30.0, 150.0, 0.0).Vector();
				Brightest->SetWorldRotation((-Sun).Rotation());
			}
		}
	}
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
	S.Desk.Reach(S.Show.Shown());
	PlayEffects();
	S.Desk.Write(Real, S.Show.Groups(), S.ClockTag, S.Words);
	if (MetRealT < 0.0 && S.Desk.Head().Contact >= 0.0)
	{
		MetRealT = RealT;
	}
	Draw(Real);
	AimEye(Real);
	if (Effects && Eye)
	{
		int32 ViewW = 1600;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			ViewW = FMath::Max(1, static_cast<int32>(Size.X));
		}
		const FBattleJolt Jolt = Effects->Tick(S.Show.AirNow(), Eye->GetActorLocation(), Eye->GetActorRotation(), Eye->GetCameraComponent()->FieldOfView, ViewW);
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

UProceduralMeshComponent* AAirBattle::PlaneMesh(int32 Key, const FString& Model, const FString& Nation, bool bFighter, double Span)
{
	if (TObjectPtr<UProceduralMeshComponent>* Found = Planes.Find(Key))
	{
		return Found->Get();
	}
	// Each model is built once, at its true size, its middle at its own origin; each aircraft has its own mesh of it.
	struct FBuilt
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colors;
	};
	static TMap<FString, FBuilt> Built;
	const FString Name = ModelFor(Model, Nation, bFighter);
	FBuilt* Shape = Built.Find(Name);
	if (!Shape)
	{
		FMiniatureMesh Made;
		Made.Vehicle(Name, FVector::ZeroVector, UnitsPerMetre, 0.0, true);
		FBuilt Fresh;
		if (Made.Vertices.Num() > 0)
		{
			// (to the battle's span: a stand-in's own size is the plain machine's, not this one's)
			const FBox Box(Made.Vertices);
			const FVector Middle = Box.GetCenter();
			const double Across = FMath::Max(1.0, Box.GetSize().Y);
			const double Fit = Name.StartsWith(TEXT("Fighter:")) || Name.StartsWith(TEXT("Bomber:")) ? Span * UnitsPerMetre / Across : 1.0;
			Fresh.Points.Reserve(Made.Vertices.Num());
			for (const FVector& Point : Made.Vertices)
			{
				Fresh.Points.Add((Point - Middle) * Fit);
			}
			Fresh.Triangles = Made.Triangles;
			Fresh.Normals = Made.Normals;
			Fresh.Colors = Made.Colors;
		}
		Shape = &Built.Add(Name, MoveTemp(Fresh));
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("AirPlane_%d"), Key));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->bUseAsyncCooking = false;
	Mesh->RegisterComponent();
	if (Shape->Points.Num() > 0)
	{
		Mesh->CreateMeshSection(0, Shape->Points, Shape->Triangles, Shape->Normals, TArray<FVector2D>(), Shape->Colors, TArray<FProcMeshTangent>(), false);
		Mesh->SetMaterial(0, PlaneInk);
	}
	Planes.Add(Key, Mesh);
	return Mesh;
}

void AAirBattle::Draw(double Real)
{
	FAirBattleState& S = *State;
	// Every aircraft where the flight core has it: its nose along its path, banked into its turn. The models are built
	// nose toward -X with the right hand at -Y, so each is turned half round before its heading, climb and bank.
	const FQuat HalfRound = FRotator(0.0, 180.0, 0.0).Quaternion();
	TSet<int32> Seen;
	for (const AirShow::PlaneOut& P : S.Show.Planes())
	{
		Seen.Add(P.Key);
		TObjectPtr<UProceduralMeshComponent>* Have = Planes.Find(P.Key);
		if (!P.bShown)
		{
			if (Have && *Have)
			{
				(*Have)->SetVisibility(false);
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
		Mesh->SetWorldLocationAndRotation(World(P.At.X, P.At.Y, P.At.Z), Attitude);
	}
	for (TPair<int32, TObjectPtr<UProceduralMeshComponent>>& Pair : Planes)
	{
		if (!Seen.Contains(Pair.Key) && Pair.Value)
		{
			Pair.Value->SetVisibility(false);
		}
	}
	// The small things, laid afresh each frame in one mesh: a parachute's canopy and its airman, a torn piece of a
	// wreck tumbling, a bomb or a torpedo falling.
	FMiniatureMesh Things;
	const double U = UnitsPerMetre;
	for (const AirShow::Thing& T : S.Show.Things())
	{
		const FVector At = World(T.At.X, T.At.Y, T.At.Z);
		switch (T.Kind)
		{
		case AirShow::ThingKind::Chute:
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
		if (Effects)
		{
			Effects->TrailMove(S.Show.AirNow(), Flame.Key, Flame.At, Flame.Velocity);
		}
		if (RealT >= Flame.Until)
		{
			if (Effects)
			{
				Effects->TrailStop(Flame.Key);
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
			if (Effects)
			{
				Effects->AirGun(Now, E.Key, At, World(E.Aim.X, E.Aim.Y, E.Aim.Z), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Rate), static_cast<float>(E.Mv * U));
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
					Sound(*Take, At, TEXT("air_guns"), 1.0f, 0.0f, Cut);
				}
			}
			break;
		}
		case AirShow::FxKind::GunStop:
			if (Effects)
			{
				Effects->AirGunStop(E.Key);
			}
			GunHeardAt.Remove(E.Key);
			break;
		case AirShow::FxKind::Hit:
			if (Effects)
			{
				Effects->AirHit(Now, At, WorldVector(E.Normal.X, E.Normal.Y, E.Normal.Z).GetSafeNormal(), WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What));
			}
			break;
		case AirShow::FxKind::TrailStart:
			// (the kit has room for about a dozen burning aircraft: past MostTrails a new trail is not lit)
			if (Effects && (TrailsOn.Contains(E.Key) || TrailsOn.Num() < MostTrails))
			{
				TrailsOn.Add(E.Key);
				AirParts::TrailStart(Effects.Get(), Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Size));
			}
			break;
		case AirShow::FxKind::TrailMove:
			if (Effects && TrailsOn.Contains(E.Key))
			{
				Effects->TrailMove(Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
			}
			break;
		case AirShow::FxKind::TrailStop:
			if (Effects && TrailsOn.Remove(E.Key) > 0)
			{
				Effects->TrailStop(E.Key);
			}
			break;
		case AirShow::FxKind::Flak:
		{
			if (Effects)
			{
				Effects->AirFlak(Now, At, AirParts::Wide(E.What));
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
			if (Effects && AirParts::HasAirFall(Effects.Get()) && S.Debris.Take(Now, E))
			{
				AirParts::AirFall(Effects.Get(), Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z), AirParts::Wide(E.What), static_cast<float>(E.Size), World(0.0, 0.0, E.Deck).Z);
				if (E.What == "explode")
				{
					BlastsTaken.Add(E.Key);
				}
				else
				{
					FallsOn.Add(E.Key);
				}
			}
			break;
		case AirShow::FxKind::FallMove:
			if (Effects && FallsOn.Contains(E.Key))
			{
				AirParts::AirFallMove(Effects.Get(), Now, E.Key, At, WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z));
			}
			break;
		case AirShow::FxKind::FallStop:
			if (Effects && FallsOn.Remove(E.Key) > 0)
			{
				AirParts::AirFallStop(Effects.Get(), E.Key);
			}
			break;
		case AirShow::FxKind::Blast:
		{
			// Blown apart (the art director, 03:14Z): a short torn flame streaming on with the wreck, never a ball, the
			// sparks and scraps of the hits; the pieces are the flight core's. Where the kit's AirFall took it, its
			// torn fireball and debris row are the blast, and this older flame is not laid over them.
			if (BlastsTaken.Remove(E.Key) > 0)
			{
				break;
			}
			if (Effects)
			{
				const FVector Velocity = WorldVector(E.Vel.X, E.Vel.Y, E.Vel.Z);
				for (int32 K = 0; K < 4; ++K)
				{
					Effects->AirHit(Now, At + FMath::VRand() * E.Size * 0.2 * U, FMath::VRand(), Velocity, TEXT("cannon20"));
				}
				FTorn& Flame = Torn.AddDefaulted_GetRef();
				Flame.Key = 1 << 29 | (TornNext++ & 0xFFFF);
				Flame.At = At;
				Flame.Velocity = Velocity * 0.6;
				Flame.Until = RealT + 0.9;
				AirParts::TrailStart(Effects.Get(), Now, Flame.Key, At, Flame.Velocity, TEXT("fire"), static_cast<float>(E.Size));
			}
			break;
		}
		case AirShow::FxKind::Splash:
			if (Effects)
			{
				Effects->ShellOfSize(At, E.Size >= 8.0 ? TEXT("bomb250") : E.Size >= 3.0 ? TEXT("light") : TEXT("grenade"), TEXT("water"));
			}
			break;
		case AirShow::FxKind::Crash:
			if (Effects)
			{
				Effects->ShellOfSize(At, E.Size >= 8.0 ? TEXT("bomb250") : TEXT("grenade"), TEXT("earth"));
				if (E.Size >= 8.0)
				{
					Effects->Burn(At, static_cast<float>(E.Size * 0.6), 90.0f);
				}
			}
			break;
		case AirShow::FxKind::BombHit:
			if (Effects)
			{
				Effects->ShellOfSize(At, E.Size >= 500.0 ? TEXT("bomb500") : TEXT("bomb250"), E.What == "water" ? TEXT("water") : TEXT("earth"));
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
			}
			break;
		case AirShow::FxKind::Smoke:
			if (Effects)
			{
				Effects->Burn(At, 40.0f, 900.0f);
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
	// The close view (handoff 10, gap 1): it rides a plane of the group in hand, or with none in hand the busiest fighter
	// near the middle of the picture; a fall, a straggle or a limp cuts to the next, never a swoop.
	if (S.bClose)
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
				Next = AirDesk::Choose(Out, std::vector<int>(), S.Show.Eye, bLost ? -1 : S.Subject.Plane);
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
			const FQuat Attitude = FRotator(Rider->Pitch, Rider->Heading, Rider->Bank).Quaternion();
			const FVector Ahead = Attitude.GetForwardVector(), Right = Attitude.GetRightVector(), Up = Attitude.GetUpVector();
			const FVector Body = World(Rider->At.X, Rider->At.Y, Rider->At.Z);
			const AirShow::PlaneOut* Foe = Find(Rider->Target >= 0 ? Rider->Target : S.Subject.Other);
			FVector Want, Look;
			if (Rider->bFighter)
			{
				// A fighter from 15 to 20 m behind, 3 m up and to one side, looking along its nose.
				Want = Body - Ahead * 17.0 * U + Up * 3.0 * U + Right * 1.5 * U;
				Look = Body + Ahead * 220.0 * U;
			}
			else
			{
				// A bomber from 10 m off its waist on the side the attack comes from, 3 m up, looking toward the attack.
				const FVector Toward = Foe ? (World(Foe->At.X, Foe->At.Y, Foe->At.Z) - Body).GetSafeNormal() : -Ahead;
				const double Side = FVector::DotProduct(Toward, Right) >= 0.0 ? 1.0 : -1.0;
				Want = Body + Right * Side * (Rider->Span * 0.5 + 10.0) * U + Up * 3.0 * U - Ahead * 2.0 * U;
				Look = Want + Toward * 300.0 * U;
			}
			const double Blend = CloseEye.IsZero() ? 1.0 : AirParts::Smooth(Real, 0.25);
			CloseEye = CloseEye.IsZero() ? Want : FMath::Lerp(CloseEye, Want, Blend);
			FRotator Facing = (Look - CloseEye).Rotation();
			Facing.Roll = FMath::Clamp(Rider->Bank / 3.0, -25.0, 25.0);
			CloseFacing = Blend >= 1.0 ? Facing : FMath::RInterpTo(CloseFacing, Facing, static_cast<float>(Real), 4.0f);
			Eye->SetActorLocationAndRotation(CloseEye, CloseFacing);
			Lens->SetFieldOfView(70.0f);
			Lens->PostProcessSettings.MotionBlurAmount = 0.4f;
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
	const FVector Wanted = World(Focus.X, Focus.Y, Focus.Z);
	LookAt = bLookSet ? FMath::Lerp(LookAt, Wanted, AirParts::Smooth(Real, 1.2)) : Wanted;
	bLookSet = true;
	const FRotator Facing(Pitch, Yaw, 0.0);
	FVector From = LookAt - Facing.Vector() * Distance * U;
	if (Stage && Stage->IsRaised())
	{
		From.Z = FMath::Max(From.Z, Stage->GroundZ(From) + 40.0 * U);
	}
	From.Z = FMath::Max(From.Z, 40.0 * U);
	Eye->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
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
	struct FPlaced { int Id; FVector2D At; };
	TArray<FPlaced> Placed;
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
			Placed.Add({ T.Id, OnScreen });
		}
	}
	// Groups never stack (b384(d)): tags within 40 px of one another fan out downward, each in its own row.
	Placed.Sort([](const FPlaced& A, const FPlaced& B) { return A.At.Y < B.At.Y; });
	for (int32 K = 1; K < Placed.Num(); ++K)
	{
		for (int32 J = 0; J < K; ++J)
		{
			if (FMath::Abs(Placed[K].At.X - Placed[J].At.X) < 160.0 && FMath::Abs(Placed[K].At.Y - Placed[J].At.Y) < 40.0)
			{
				Placed[K].At.Y = Placed[J].At.Y + 40.0;
			}
		}
	}
	for (const FPlaced& P : Placed)
	{
		S.TagAt[P.Id] = P.At;
	}
}

int32 AAirBattle::TagUnder(const FVector2D& Pixel) const
{
	const FAirBattleState& S = *State;
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
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ACampaignMapActor>() || It->IsA<AArmyPiece>())
		{
			It->SetActorHiddenInGame(false);
		}
		if (ACampaignEnvironment* Environment = Cast<ACampaignEnvironment>(*It))
		{
			Environment->SetActorTickEnabled(true);
		}
	}
	Destroy();
}

void AAirBattle::EndPlay(const EEndPlayReason::Type Reason)
{
	HideScreen();
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
	GEngine->GameViewport->AddViewportWidgetContent(Screen.ToSharedRef(), 20);
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
	if (Screen.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
	if (bOwnInput)
	{
		bOwnInput = false;
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Controller->SetInputMode(FInputModeGameAndUI());
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

void AAirBattle::Sound(const TCHAR* Name, const FVector& Where, const TCHAR* Family, float Volume, float After, float Cut)
{
	if (After > 0.0f)
	{
		Later.Add({ RealT + After, FName(Name), Where, FName(Family), Volume, Cut });
		return;
	}
	USoundBase* Played = SoundOf(Name);
	if (!Played)
	{
		return;
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
			}
			continue;
		}
		if (!Have)
		{
			const bool bFour = G.Model == "Lancaster" || G.Model == "Halifax" || G.Model == "B24";
			const bool bTwin = G.Model == "He111" || G.Model == "Betty" || G.Model == "G4M" || G.Model == "Bomber";
			const TCHAR* Kind = G.Model == "B17" ? TEXT("B-17") : bFour ? TEXT("fourEngined") : bTwin ? TEXT("twinEngined") : TEXT("fighter");
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
	for (FShot& Shot : Shots)
	{
		if (Shot.bTaken)
		{
			continue;
		}
		const double Base = Shot.bShips ? ShipRealT : MetRealT;
		if (Base < 0.0)
		{
			// (the approach is taken before the formations meet: the clock's x10 brings them together in some 20 s)
			if (Shot.At < 0.0 && !Shot.bShips && RealT >= 8.0)
			{
				Shot.bTaken = true;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
				++ShotsTaken;
			}
			continue;
		}
		const double Due = Base + Shot.At;
		if (Shot.bClose && !S.bClose && RealT >= Due - 1.5)
		{
			Press(TEXT("ride"));
		}
		if (RealT >= Due)
		{
			Shot.bTaken = true;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotFolder, AirParts::Wide(S.Recording.Key) + TEXT("-") + Shot.Name + TEXT(".png")), true, false);
			++ShotsTaken;
			if (Shot.bClose)
			{
				CloseOffAt = RealT + 0.5;
			}
		}
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
