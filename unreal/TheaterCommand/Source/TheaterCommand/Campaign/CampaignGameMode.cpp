#include "Campaign/CampaignGameMode.h"
#include "Battle/AirBattle.h"
#include "Battle/AirStage.h"

#include "Campaign/CampaignFlags.h"
#include "ImageUtils.h"

#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignHUD.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPhotographer.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "TheaterCommand.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

ACampaignGameMode::ACampaignGameMode()
{
	DefaultPawnClass = ACampaignCameraPawn::StaticClass();
	PlayerControllerClass = ACampaignPlayerController::StaticClass();
	HUDClass = ACampaignHUD::StaticClass();
}

namespace
{
	void SetRenderSetting(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	/**
	 * The campaign table is one large model under steady light, seen from above. Unreal's bounced-light
	 * system (Lumen) costs about a third of every frame and adds nothing that can be seen here, so the
	 * campaign runs without it. Battles, which are full scenes, will choose for themselves.
	 */
	void ApplyCampaignRendering()
	{
		SetRenderSetting(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
		SetRenderSetting(TEXT("r.ReflectionMethod"), 2); // reflections from what is on screen
		// (But nothing is traced for them: the only thing on screen the rippled sea could mirror was a flag standing
		// over it, and that came out as a pale speckled patch on the water beside every fleet.)
		SetRenderSetting(TEXT("r.SSR.Quality"), 0);
		// The table is one very large model that is not yet in the form Unreal's shadow system likes best,
		// and from high above it says so on screen. The shadows are right; the note is switched off.
		SetRenderSetting(TEXT("r.Shadow.Virtual.AllowScreenOverflowMessages"), 0);
		// The upscaler one step below its costliest setting: the picture is the same to the eye and the frame is quicker.
		SetRenderSetting(TEXT("sg.AntiAliasingQuality"), 2);
	}

	/** Opens the game in a window that suits the screen it is on, unless the command line asks for a size ("-TheaterWindow=1600x1000"). */
	void ApplyWindow()
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!Settings)
		{
			return;
		}
		FIntPoint Size(0, 0);
		FString Asked;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterWindow="), Asked))
		{
			FString Width, Height;
			if (Asked.Split(TEXT("x"), &Width, &Height))
			{
				Size = FIntPoint(FCString::Atoi(*Width), FCString::Atoi(*Height));
			}
		}
		bool bSuited = false;
		if (Size.X < 640 || Size.Y < 400)
		{
			// (18 November 2026. The window took 86 and 84 hundredths of the screen. On Brian's Mac, whose screen is 1470
			// by 924 points under the menu bar, that is 1264 by 776: every card and every line of type at 78 hundredths of
			// the 1600 by 1000 frame the game is drawn and reviewed at, and the small type about seven points high. It
			// takes nearly all of the screen now, leaving room for the window's own title bar: 1440 by 878 there, and
			// the type an eighth larger. "-TheaterWindow=1264x776" opens it as it was.)
			const FIntPoint Desktop = Settings->GetDesktopResolution();
			// (no upper limit of its own: where the game is drawn at the panel's fine pixels the screen is counted in
			// those, 2940 by 1880 on his Mac, and a limit of 2240 by 1260 made the window a small one)
			Size.X = FMath::Max(FMath::RoundToInt32(Desktop.X * 0.98f), 1100);
			Size.Y = FMath::Max(FMath::RoundToInt32(Desktop.Y * 0.95f), 700);
			bSuited = true;
		}
		Settings->SetScreenResolution(Size);
		Settings->SetFullscreenMode(EWindowMode::Windowed);
		Settings->ApplyResolutionSettings(false);
		{
			// (said in the log, so that the size a player really plays at is known: the test pictures are 1600 by 1000,
			// and for a long time nobody had looked at the game at the size of his own window)
			const FIntPoint Desktop = Settings->GetDesktopResolution();
			UE_LOG(LogTheater, Display, TEXT("Window: the screen is %d by %d and the game's window %d by %d%s"), Desktop.X, Desktop.Y, Size.X, Size.Y,
				bSuited ? TEXT(" (98 and 95 hundredths of the screen)") : TEXT(" (asked for on the command line)"));
		}
	}
}

void ACampaignGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ApplyCampaignRendering();
	ApplyWindow();
	// (The engine writes messages of its own across the screen: "Preparing Shaders" the first time a paint is used,
	// and how to turn such messages off. They are not for a player: the game shows none. "-TheaterEngineMessages"
	// leaves them on.)
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterEngineMessages")))
	{
		GAreScreenMessagesEnabled = false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->SpawnActor<ACampaignEnvironment>(FVector::ZeroVector, FRotator::ZeroRotator);
	Table = World->SpawnActor<ACampaignMapActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Table && Table->BuildMap())
	{
		Session = MakeShared<FCampaignSession>();
		FString Error;
		if (!Session->Prepare(Error))
		{
			UE_LOG(LogTheater, Error, TEXT("The rules could not be started: %s"), *Error);
		}
		// Normally the opening screen asks which campaign to play. "-TheaterNation=FRA" on the command line skips it
		// and starts the grand campaign as that country in September 1939.
		FString Human;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterNation="), Human))
		{
			StartCampaign(Human, 1, TEXT("grand"));
		}
	}

	// "-TheaterFlags=<file.png>" on the command line saves every flag on one sheet, for checking them.
	FString FlagSheet;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFlags="), FlagSheet))
	{
		TArray<FString> Tags = CampaignFlags::KnownTags();
		Tags.Sort();
		const int32 PerRow = 7, Gap = 16, W = CampaignFlags::SizeX * 3 / 4, H = CampaignFlags::SizeY; // shown three by two
		const int32 SheetRows = (Tags.Num() + PerRow - 1) / PerRow;
		const int32 SheetW = PerRow * (W + Gap) + Gap, SheetH = SheetRows * (H + Gap) + Gap;
		TArray<FColor> Sheet;
		Sheet.Init(FColor(70, 70, 70), SheetW * SheetH);
		for (int32 Index = 0; Index < Tags.Num(); ++Index)
		{
			TArray<FLinearColor> Pixels;
			CampaignFlags::Paint(Tags[Index], FLinearColor::Gray, Pixels);
			const int32 X0 = Gap + (Index % PerRow) * (W + Gap), Y0 = Gap + (Index / PerRow) * (H + Gap);
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					Sheet[(Y0 + Y) * SheetW + X0 + X] = Pixels[Y * CampaignFlags::SizeX + X * CampaignFlags::SizeX / W].ToFColor(true);
				}
			}
		}
		FImageUtils::SaveImageByExtension(*FlagSheet, FImageView(Sheet.GetData(), SheetW, SheetH));
		UE_LOG(LogTheater, Display, TEXT("Flags: %s on %s"), *FString::Join(Tags, TEXT(" ")), *FlagSheet);
	}

	// "-TheaterShots=<folder>" on the command line takes a set of pictures of the table and quits.
	// (A scene that takes its own pictures, "-TheaterAirBattle=" or "-TheaterStage=", leaves the photographer at home.
	// Given beside them, "-TheaterShots=" still makes the run a picture run: its own save, the player's options and
	// campaign left alone.)
	FString ShotFolder;
	const bool bSceneTakesThem = FCString::Strifind(FCommandLine::Get(), TEXT("TheaterAirBattle=")) != nullptr
		|| FCString::Strifind(FCommandLine::Get(), TEXT("TheaterStage=")) != nullptr;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), ShotFolder) && !bSceneTakesThem)
	{
		if (ACampaignPhotographer* Photographer = World->SpawnActor<ACampaignPhotographer>())
		{
			Photographer->Begin(ShotFolder);
		}
	}

	// "-TheaterStage=oahu" (or "midway", "midway-fleet") raises a set piece's stage alone and plays a strike on it
	// (ruling 238): with "-TheaterStageShots=<folder>" it takes its pictures there and quits.
	FString StageKey;
	if (AAirStage::WantsTest(StageKey))
	{
		if (AAirStage* Stage = World->SpawnActor<AAirStage>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Stage->BeginTest(StageKey);
		}
	}

	// "-TheaterAirBattle=oahu" (or "midway-fleet") plays a recorded air battle in the 3D scene (ruling 238; the recordings
	// are Content/Data/AirBattles, written by tools/air-record): with "-TheaterAirBattleShots=<folder>" it takes the art
	// director's pictures there and quits. (No campaign-screen button until the playtester passes it: ruling 217.)
	FString AirKey;
	if (AAirBattle::WantsReplay(AirKey))
	{
		if (AAirBattle* Air = World->SpawnActor<AAirBattle>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Air->ReplayWhenReady(AirKey);
		}
	}
}

void ACampaignGameMode::LayOutWar()
{
	if (Table && Session.IsValid() && Session->IsRunning())
	{
		// The rivers and railways the rules know are laid on the table, then the war as it stands.
		Table->SetLand(Session->Land());
		Table->SetTownPlots(Session->GetView().PageIds.Contains(TEXT("towns")));
		Table->SetTowns(Session->Towns());
		Table->SetInkAllowed(false); // (the table laid out as the campaign stands: nothing seeps in)
		Table->SetHolders(Session->GetView().RegionHolder, Session->GetView().League, Session->GetView().RegionOwner, Session->GetView().SiegePaint);
		Table->ShowArmies(Session->GetView(), TArray<FCorpsMove>());
		Table->ShowAgents(Session->GetView()); // (ruling 153: his agents, in a campaign taken up again)
		Table->ShowWorks(Session->GetView().BridgesDown, Session->GetView().RailsCut);
		Table->ShowCuts(Session->GetView().Cuts);
		Table->ShowWorksGoing(Session->GetView().Going);
		Table->ShowBuildings(Session->GetView().RegionWorks);
		Table->ShowAirfieldSigns(Session->GetView().Airfields);
		Table->ShowLanes(Session->GetView().Lanes);
		Table->ShowFreeCapitals(Session->GetView().FreeCapitals);
		Table->ShowShroud(Session->GetView().Unseen, Session->GetView().Glimpsed);
		Table->ShowMud(Session->GetView().bMud ? Session->GetView().WeatherGround : TArray<int32>());
		Table->SetSeason(Session->GetView().Turn);
		// (and what the war has left on the table: works in hand, scars, sieges, and who keeps the blackout)
		Table->ShowCivic(Session->GetView().Civic);
		Table->ShowScars(Session->GetView().Scars);
		Table->ShowSieges(Session->GetView().Sieges);
		Table->ShowBombed(Session->GetView().Bombed);
		Table->ShowWorksLeft(Session->GetView().WorksLeft);
		Table->ShowWrecked(Session->GetView().Wrecked);
		Table->SetWarAtNight(Session->GetView().Blackout, Session->GetView().Fronts);
		Table->ShowFrontLine(Session->GetView().Wars, Session->GetView().Human);
		Table->ShowWrecks(Session->GetView().Wrecks, Session->GetView().Turn);
		Table->ShowIdle(Session->GetView().Cold);
		Table->ShowPrizes(Session->GetView().Prizes);
	}
}

bool ACampaignGameMode::StartCampaign(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, const FString& Level, const FString& Depth, int32 History, bool bFirstCommand)
{
	FString Error;
	// (Rules 84.3.3) bFirstCommand: the campaign the door's FIRST COMMAND opens, which the rules mark as its own.
	if (!Session.IsValid() || !Session->Begin(Human, From, ViewMode, Seed, Error, Level, Depth, History, bFirstCommand))
	{
		UE_LOG(LogTheater, Error, TEXT("The campaign could not start: %s"), *Error);
		return false;
	}
	LayOutWar();
	return true;
}

FString ACampaignGameMode::SaveFile()
{
	// A run that only takes pictures keeps its own save, so that it never writes over the player's campaign.
	FString Shots;
	const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bPictures ? TEXT("campaign-pictures.json") : TEXT("campaign.json"));
}

void ACampaignGameMode::SaveCampaign()
{
	if (!Session.IsValid() || !Session->IsRunning())
	{
		return;
	}
	const FString Text = Session->Save();
	if (!Text.IsEmpty())
	{
		FFileHelper::SaveStringToFile(Text, *SaveFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

FString ACampaignGameMode::DescribeSavedCampaign()
{
	FString Text;
	if (!Session.IsValid() || !FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		return FString();
	}
	return Session->DescribeSave(Text);
}

bool ACampaignGameMode::ContinueCampaign()
{
	FString Text;
	if (!Session.IsValid() || !FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		return false;
	}
	// (Rules version 74: a campaign saved on the wider world has provinces and seas a table laid with Europe alone
	// does not have. It is not continued there. The other way round is no trouble: a campaign saved on Europe
	// alone plays on, with the world beyond as its backdrop.)
	if (Text.Contains(TEXT("\"world\":1")) && !FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld")))
	{
		UE_LOG(LogTheater, Display, TEXT("The saved campaign is played on the wider world, and this table is laid with Europe alone: it is not continued."));
		return false;
	}
	if (!Session->Load(Text))
	{
		return false;
	}
	LayOutWar();
	return true;
}

bool ACampaignGameMode::RestoreCampaign(const FString& SaveText)
{
	if (!Session.IsValid() || !Session->Load(SaveText))
	{
		return false;
	}
	LayOutWar();
	return true;
}

void ACampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	// The music: original pieces on a loop, quiet enough to think over. They are made by Tools/build_music.sh
	// and are not kept in the repository, so a copy of the game without them simply plays in silence.
	// "-TheaterQuiet" plays them too softly to hear: for checking that they play without disturbing anyone.
	// (The design director's rulings 142 and 144, b478, 18 November 2026: with the real recorded sounds in, the music
	// comes down to about 0.15, so that the guns and engines are heard over it. It stood at 0.55.)
	MusicVolume = FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet")) ? 0.0005f : 0.15f;
	const TPair<const TCHAR*, const TCHAR*> Made[] = {
		{ TEXT("theme"), TEXT("/Game/Campaign/Music/Theme.Theme") },
		{ TEXT("battle"), TEXT("/Game/Campaign/Music/Battle.Battle") },
		{ TEXT("quiet"), TEXT("/Game/Campaign/Music/MapRoom.MapRoom") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Piece : Made)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Piece.Value))
		{
			UAudioComponent* Player = UGameplayStatics::SpawnSound2D(this, Sound, MusicVolume, 1.0f, 0.0f, nullptr, false, false);
			if (Player)
			{
				Player->bIsUISound = true; // keeps playing if the game is paused
				Player->Stop();
				Pieces.Add(Piece.Key, Player);
			}
		}
	}
	// (Brian, 18 November 2026: "when i played the 3d game i didnt hear my sound tracks i added".) The score: his five
	// songs and the three pieces written to go with them, placed as the music thread's README sets them out. The
	// Final Salute is the opening screen's theme; at peace The Map Room, A Vigil at the Ridge and The Long Walk Home
	// take turns; at war The Silent Front, Standing To and The Long Front (the third from 19 November 2026, with the
	// sound director's mix 3); in battle The Advance and Trench Line At Twilight; and
	// The Long Walk Home is played when one of the player's wars ends. They are brought in by Tools/build_music.sh
	// from Music/Score in the game's folder (they are large, and are not kept in the repository).
	const TPair<const TCHAR*, const TCHAR*> Scored[] = {
		{ TEXT("theme"), TEXT("theme") },
		{ TEXT("quiet"), TEXT("map_room") }, { TEXT("quiet"), TEXT("vigil") }, { TEXT("quiet"), TEXT("long_walk") },
		{ TEXT("war"), TEXT("silent_front") }, { TEXT("war"), TEXT("standing_to") }, { TEXT("war"), TEXT("long_front") },
		{ TEXT("battle"), TEXT("advance") }, { TEXT("battle"), TEXT("trench_line") },
		// (a battle scene's own: Zero Hour before contact, looped; the two stingers at the end. BeginBattleScore)
		{ TEXT("battle_wait"), TEXT("zero_hour") }, { TEXT("battle_won"), TEXT("battle_won") }, { TEXT("battle_lost"), TEXT("battle_lost") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Piece : Scored)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Music/Score/%s.%s"), Piece.Value, Piece.Value), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UAudioComponent* Player = Sound ? UGameplayStatics::SpawnSound2D(this, Sound, MusicVolume, 1.0f, 0.0f, nullptr, false, false) : nullptr;
		if (Player)
		{
			Player->bIsUISound = true;
			Player->Stop();
			const int32 Index = ScorePlayers.Add(Player);
			ScoreOf.FindOrAdd(Piece.Key).Add(Index);
			if (FCString::Strcmp(Piece.Value, TEXT("long_walk")) == 0)
			{
				HomePiece = Index;
			}
			else if (FCString::Strcmp(Piece.Value, TEXT("zero_hour")) == 0)
			{
				ZeroHourPiece = Index;
				// (Zero Hour waits on its last chord for contact: it loops, where the score's other pieces play once through)
				if (USoundWave* Wave = Cast<USoundWave>(Sound))
				{
					Wave->bLooping = true;
				}
			}
			else if (FCString::Strcmp(Piece.Value, TEXT("battle_won")) == 0)
			{
				WonPiece = Index;
			}
			else if (FCString::Strcmp(Piece.Value, TEXT("battle_lost")) == 0)
			{
				LostPiece = Index;
			}
		}
	}
	Music = PieceFor(TEXT("theme"));
	if (Music)
	{
		StartPiece(4.0f);
		UE_LOG(LogTheater, Display, TEXT("Music: %d made pieces and %d of the score's twelve, the theme %s"), Pieces.Num(), ScorePlayers.Num(), Music->IsPlaying() ? TEXT("playing") : TEXT("not playing"));
	}
	else
	{
		UE_LOG(LogTheater, Display, TEXT("No music found. Run Tools/build_music.sh to make it."));
	}
}

UAudioComponent* ACampaignGameMode::PieceFor(const FString& Mood) const
{
	if (const TArray<int32>* Theirs = ScoreOf.Find(Mood))
	{
		if (Theirs->Num() > 0)
		{
			return ScorePlayers[(*Theirs)[ScoreTurn.FindRef(Mood) % Theirs->Num()]];
		}
	}
	const TObjectPtr<UAudioComponent>* Made = Pieces.Find(Mood);
	return Made ? Made->Get() : nullptr;
}

void ACampaignGameMode::StartPiece(float FadeSeconds)
{
	if (!Music)
	{
		return;
	}
	Music->FadeIn(FadeSeconds, NowVolume());
	// (Its end is reckoned by the clock and its own length, not by asking whether it still plays: played too softly
	// to hear, as "-TheaterQuiet" plays it, a piece is put out at once by the engine, and the turn would spin.)
	const float Long = Music->Sound ? Music->Sound->GetDuration() : 0.0f;
	PieceEndsAt = FPlatformTime::Seconds() + (Long > 1.0f && Long < 3600.0f ? Long + 1.5 : 1.0e9);
}

float ACampaignGameMode::NowVolume() const
{
	// (The design director's ruling 142: in play the music sits at about 0.15, under the recorded guns and engines.
	// On the opening screen nothing else sounds, and the score's theme is to be clearly heard. Measured: The Final
	// Salute's recording stands 18.5 dB under full scale, where the synthesised theme it replaces stood 12.8 under
	// and played at 0.55; to be heard as loud as that was, it plays at its master's own level on the usual setting.)
	const float Opening = MusicMood == TEXT("theme") && ScoreOf.Contains(TEXT("theme")) && !bHomePlaying ? 6.7f : 1.0f;
	// (a battle scene's score at the level it asks for: under the fight's thick, back up in a lull)
	const float Battle = bBattleScore ? FMath::Pow(10.0f, BattleScoreDb / 20.0f) : 1.0f;
	return FMath::Min(MusicVolume * Opening, 1.0f) * (bMusicDucked ? 0.3f : 1.0f) * Battle;
}

void ACampaignGameMode::SetMusicMood(const FString& InMood)
{
	// (a battle scene's score holds the music while it runs: BeginBattleScore. Its pieces take their turns here, the
	// controller asking every frame.)
	if (bBattleScore)
	{
		const double Now = FPlatformTime::Seconds();
		if (bBattleEnding && Now > BattleScoreEndsAt)
		{
			bBattleScore = false;
			bBattleEnding = false;
			BattleScoreDb = 0.0f;
			MusicMood.Reset(); // (so the mood asked for now begins its piece)
		}
		else
		{
			if (!bBattleEnding && bBattleContact && bMusicOn && Music && ScoreOf.Contains(TEXT("battle")) && Now > PieceEndsAt)
			{
				++ScoreTurn.FindOrAdd(TEXT("battle"));
				Music = PieceFor(TEXT("battle"));
				StartPiece(2.0f);
			}
			return;
		}
	}
	// (the made pieces have none for the war map: without the score the theme stands for the war, as it did)
	const FString Mood = InMood == TEXT("war") && !ScoreOf.Contains(TEXT("war")) ? FString(TEXT("theme")) : InMood;
	if (bHomePlaying)
	{
		// (The Long Walk Home is heard out, unless a battle begins)
		if (Music && FPlatformTime::Seconds() <= PieceEndsAt && Mood != TEXT("battle"))
		{
			return;
		}
		bHomePlaying = false;
		MusicMood.Reset();
	}
	if (Mood == MusicMood)
	{
		// (the score's pieces are not loops: when one has ended, the next of its mood takes its turn)
		if (bMusicOn && Music && ScoreOf.Contains(Mood) && FPlatformTime::Seconds() > PieceEndsAt)
		{
			++ScoreTurn.FindOrAdd(Mood);
			Music = PieceFor(Mood);
			UE_LOG(LogTheater, Verbose, TEXT("Music: the piece has ended; %s takes its turn"), Music && Music->Sound ? *Music->Sound->GetName() : TEXT("nothing"));
			StartPiece(2.0f);
		}
		return;
	}
	// (a mood come back to takes up its next piece, so that a war map broken by battles does not begin the same one each time)
	if (ScoreOf.Contains(Mood) && ScoreTurn.Contains(Mood))
	{
		++ScoreTurn[Mood];
	}
	else
	{
		ScoreTurn.FindOrAdd(Mood);
	}
	UAudioComponent* Next = PieceFor(Mood);
	if (!Next)
	{
		return; // that piece has not been made: what is playing goes on
	}
	MusicMood = Mood;
	if (Music == Next)
	{
		return;
	}
	UE_LOG(LogTheater, Verbose, TEXT("Music: the mood is %s; %s begins, at %.2f"), *Mood, Next->Sound ? *Next->Sound->GetName() : TEXT("a piece"), NowVolume());
	if (Music && bMusicOn)
	{
		Music->FadeOut(Mood == TEXT("battle") ? 0.8f : 2.5f, 0.0f);
	}
	Music = Next;
	if (bMusicOn)
	{
		StartPiece(Mood == TEXT("battle") ? 0.6f : 3.0f);
	}
}

void ACampaignGameMode::BeginBattleScore()
{
	if (bBattleScore)
	{
		return;
	}
	UAudioComponent* Wait = ScorePlayers.IsValidIndex(ZeroHourPiece) ? ScorePlayers[ZeroHourPiece].Get() : nullptr;
	if (!Wait && !ScoreOf.Contains(TEXT("battle")))
	{
		// (without the score: the battle mood, as it was)
		SetMusicMood(TEXT("battle"));
		return;
	}
	bBattleScore = true;
	bBattleContact = false;
	bBattleEnding = false;
	bHomePlaying = false;
	BattleScoreDb = 0.0f;
	MusicMood = TEXT("battle");
	if (!Wait)
	{
		// (no Zero Hour: the battle's piece from the start)
		++ScoreTurn.FindOrAdd(TEXT("battle"));
		Wait = PieceFor(TEXT("battle"));
		bBattleContact = true;
	}
	UE_LOG(LogTheater, Display, TEXT("Music: a battle's score begins: %s"), Wait && Wait->Sound ? *Wait->Sound->GetName() : TEXT("nothing"));
	if (Music && Music != Wait && bMusicOn)
	{
		Music->FadeOut(0.8f, 0.0f);
	}
	Music = Wait;
	if (bMusicOn && Music)
	{
		StartPiece(0.6f);
		if (!bBattleContact)
		{
			PieceEndsAt = 1.0e18; // (Zero Hour loops until contact)
		}
	}
}

void ACampaignGameMode::BattleScoreContact(float Db, float CrossfadeSeconds)
{
	if (!bBattleScore || bBattleEnding)
	{
		return;
	}
	BattleScoreDb = Db;
	if (bBattleContact)
	{
		// (no Zero Hour was playing: the battle's piece only comes down to its level)
		if (Music && bMusicOn)
		{
			Music->AdjustVolume(FMath::Max(0.05f, CrossfadeSeconds), NowVolume());
		}
		return;
	}
	bBattleContact = true;
	if (!ScoreOf.Contains(TEXT("battle")))
	{
		// (Zero Hour without the battle's pieces: it plays on, at the battle's level)
		if (Music && bMusicOn)
		{
			Music->AdjustVolume(FMath::Max(0.05f, CrossfadeSeconds), NowVolume());
		}
		return;
	}
	++ScoreTurn.FindOrAdd(TEXT("battle"));
	UAudioComponent* Next = PieceFor(TEXT("battle"));
	UE_LOG(LogTheater, Display, TEXT("Music: first contact: %s comes in over %.1f s at %+.0f dB"), Next && Next->Sound ? *Next->Sound->GetName() : TEXT("nothing"), CrossfadeSeconds, Db);
	if (Music && Music != Next && bMusicOn)
	{
		Music->FadeOut(FMath::Max(0.05f, CrossfadeSeconds), 0.0f);
	}
	Music = Next;
	if (bMusicOn && Music)
	{
		StartPiece(FMath::Max(0.05f, CrossfadeSeconds));
	}
}

void ACampaignGameMode::SetBattleScoreLevel(float Db, float FadeSeconds)
{
	if (!bBattleScore || bBattleEnding || FMath::Abs(Db - BattleScoreDb) < 0.1f)
	{
		return;
	}
	BattleScoreDb = Db;
	if (Music && bMusicOn)
	{
		Music->AdjustVolume(FMath::Max(0.02f, FadeSeconds), NowVolume());
	}
}

void ACampaignGameMode::EndBattleScore(int32 Outcome)
{
	if (!bBattleScore || bBattleEnding)
	{
		return;
	}
	bBattleEnding = true;
	BattleScoreDb = 0.0f; // (the stinger at the music setting)
	const int32 Sting = Outcome == 1 ? WonPiece : Outcome == 0 ? LostPiece : INDEX_NONE;
	UAudioComponent* Stinger = ScorePlayers.IsValidIndex(Sting) ? ScorePlayers[Sting].Get() : nullptr;
	UE_LOG(LogTheater, Display, TEXT("Music: the battle is over (%s): %s"), Outcome == 1 ? TEXT("won") : Outcome == 0 ? TEXT("lost") : TEXT("neither"),
		Stinger && Stinger->Sound ? *Stinger->Sound->GetName() : TEXT("no stinger"));
	if (Music && bMusicOn && Music != Stinger)
	{
		Music->FadeOut(1.0f, 0.0f);
	}
	if (!Stinger)
	{
		BattleScoreEndsAt = FPlatformTime::Seconds() + 1.0;
		return;
	}
	Music = Stinger;
	const float Long = Stinger->Sound ? Stinger->Sound->GetDuration() : 0.0f;
	if (bMusicOn)
	{
		StartPiece(0.1f);
	}
	// (heard out, and the moods a second after it: mix.json music.end.musicBackAfterStinger)
	BattleScoreEndsAt = FPlatformTime::Seconds() + (Long > 0.5f && Long < 600.0f ? Long + 1.0 : 2.0);
}

void ACampaignGameMode::StopBattleScore()
{
	if (!bBattleScore || bBattleEnding)
	{
		return;
	}
	bBattleScore = false;
	BattleScoreDb = 0.0f;
	if (Music && bMusicOn)
	{
		Music->FadeOut(2.0f, 0.0f);
	}
	MusicMood.Reset(); // (the mood asked for next begins its piece)
}

void ACampaignGameMode::PlayHomecoming()
{
	if (!ScorePlayers.IsValidIndex(HomePiece) || MusicMood == TEXT("battle") || bHomePlaying)
	{
		return;
	}
	UAudioComponent* Home = ScorePlayers[HomePiece];
	if (Music && Music != Home && bMusicOn)
	{
		Music->FadeOut(2.0f, 0.0f);
	}
	bHomePlaying = true;
	Music = Home;
	UE_LOG(LogTheater, Verbose, TEXT("Music: a war of the player's has ended; The Long Walk Home begins"));
	if (bMusicOn)
	{
		StartPiece(2.5f);
	}
}

void ACampaignGameMode::DuckMusic(bool bDown)
{
	if (bDown == bMusicDucked)
	{
		return;
	}
	bMusicDucked = bDown;
	if (Music && bMusicOn)
	{
		Music->AdjustVolume(0.8f, NowVolume());
	}
}

void ACampaignGameMode::SetMusicLevel(int32 Level)
{
	MusicLevel = FMath::Clamp(Level, 0, 2);
	const float Loud[] = { 0.08f, 0.15f, 0.3f }; // (quiet, as ruled, loud: they stood at 0.26, 0.55 and 0.9 before the real sounds)
	MusicVolume = FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet")) ? 0.0005f : Loud[MusicLevel];
	if (Music && bMusicOn)
	{
		Music->AdjustVolume(0.4f, NowVolume());
	}
}

bool ACampaignGameMode::ToggleMusic()
{
	if (!Music)
	{
		return false;
	}
	bMusicOn = !bMusicOn;
	if (bMusicOn)
	{
		StartPiece(1.5f);
	}
	else
	{
		Music->FadeOut(1.0f, 0.0f);
	}
	return bMusicOn;
}
