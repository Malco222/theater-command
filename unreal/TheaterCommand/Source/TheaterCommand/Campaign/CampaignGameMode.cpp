#include "Campaign/CampaignGameMode.h"
#include "Campaign/TheaterSpeed.h"
#include "Battle/AirBattle.h"
#include "Battle/LandBattle.h"
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
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
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
		|| FCString::Strifind(FCommandLine::Get(), TEXT("TheaterStage=")) != nullptr
		|| FCString::Strifind(FCommandLine::Get(), TEXT("TheaterLandBattle=")) != nullptr
		|| FCString::Strifind(FCommandLine::Get(), TEXT("TheaterLandLive=")) != nullptr;
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

	// "-TheaterLandBattle=normandy-1944" plays a recorded ground battle in the 3D scene (release 2; the recordings are
	// Content/Data/LandBattles, written by Tools/land_record): with "-TheaterLandBattleShots=<folder>" it takes the art
	// director's pictures there and quits.
	FString LandKey;
	if (ALandBattle::WantsReplay(LandKey))
	{
		if (ALandBattle* Land = World->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Land->ReplayWhenReady(LandKey);
		}
	}

	// "-TheaterLandLive=normandy-1944" fights a set piece live in the same scene, the player its general (release 2):
	// "-TheaterLandSide=att" (the default), "def" or "none" (a film); "-TheaterLandNoDeploy" skips the setup step.
	FString LiveKey, LiveLook;
	if (LandKey.IsEmpty() && ALandBattle::WantsLive(LiveKey))
	{
		if (ALandBattle* Land = World->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Land->LiveWhenReady(LiveKey);
		}
	}
	// "-TheaterLandTicket=<file>": a campaign battle's look (TC.battleLook's answer) or ticket fought live the same way, the
	// test path for the campaign's battles opened in the land rules (release 2, the production lead's (a)).
	else if (LandKey.IsEmpty() && ALandBattle::WantsLiveTicket(LiveLook))
	{
		if (ALandBattle* Land = World->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Land->LiveTicketWhenReady(LiveLook);
		}
	}
}

void ACampaignGameMode::LayOutWar()
{
	THEATER_SPEED_SCOPE(LayOutWar); // (release 2, map run 7: the campaign's first laying, apart from a month's turn in the speed log)
	LaySteps.Reset();
	const bool bSteps = bLayInSteps;
	bLayInSteps = false;
	if (!Table || !Session.IsValid() || !Session->IsRunning())
	{
		return;
	}
	// (Release 2, tc.Speed.WarSpread: in map run 8 the war's first laying took 1.2 to 1.4 s of one frame.) The laying in
	// its steps, in the order it was always laid. Each reads the session's picture as the step is taken: nothing changes
	// it between them, for the opening's plate stands over the map and no order is taken until the war is entered.
	// The rivers and railways the rules know are laid on the table, then the war as it stands.
	LaySteps.Emplace([this]()
	{
		Table->SetLand(Session->Land());
	});
	LaySteps.Emplace([this]()
	{
		Table->SetTownPlots(Session->GetView().PageIds.Contains(TEXT("towns")));
		Table->SetTowns(Session->Towns());
	});
	LaySteps.Emplace([this]()
	{
		const FCampaignView& Now = Session->GetView();
		Table->SetInkAllowed(false); // (the table laid out as the campaign stands: nothing seeps in)
		Table->SetHolders(Now.RegionHolder, Now.League, Now.RegionOwner, Now.SiegePaint);
	});
	LaySteps.Emplace([this]()
	{
		const FCampaignView& Now = Session->GetView();
		Table->ShowArmies(Now, TArray<FCorpsMove>());
		Table->ShowAgents(Now); // (ruling 153: his agents, in a campaign taken up again)
	});
	LaySteps.Emplace([this]()
	{
		const FCampaignView& Now = Session->GetView();
		Table->ShowWorks(Now.BridgesDown, Now.RailsCut);
		Table->ShowCuts(Now.Cuts);
		Table->ShowWorksGoing(Now.Going);
		Table->ShowBuildings(Now.RegionWorks);
		Table->ShowAirfieldSigns(Now.Airfields);
		Table->ShowLanes(Now.Lanes);
		Table->ShowFreeCapitals(Now.FreeCapitals);
		Table->ShowShroud(Now.Unseen, Now.Glimpsed);
		Table->ShowMud(Now.bMud ? Now.WeatherGround : TArray<int32>());
	});
	LaySteps.Emplace([this]()
	{
		Table->SetSeason(Session->GetView().Turn);
	});
	LaySteps.Emplace([this]()
	{
		// (and what the war has left on the table: works in hand, scars, sieges, and who keeps the blackout)
		const FCampaignView& Now = Session->GetView();
		Table->ShowCivic(Now.Civic);
		Table->ShowScars(Now.Scars);
		Table->ShowSieges(Now.Sieges);
		Table->ShowBombed(Now.Bombed);
		Table->ShowWorksLeft(Now.WorksLeft);
		Table->ShowWrecked(Now.Wrecked);
		Table->SetWarAtNight(Now.Blackout, Now.Fronts);
		Table->ShowFrontLine(Now.Wars, Now.Human);
		Table->ShowFrontChurn(Now); // (release 2: churned ground between dug-in lines; nothing while tc.Map.FrontChurn is off)
		Table->ShowWrecks(Now.Wrecks, Now.Turn);
		Table->ShowIdle(Now.Cold);
		Table->ShowPrizes(Now.Prizes);
	});
	LaySteps.Emplace([this]()
	{
		// (release 2, map run 7: tc.Speed.SidesKept) the sides view laid out of sight now, in the load, not on the first press
		Table->WarmSides(Session->GetView().RegionStanding);
	});
	if (!bSteps)
	{
		for (const TFunction<void()>& Step : LaySteps)
		{
			Step();
		}
		LaySteps.Reset();
	}
}

bool ACampaignGameMode::LayNextStep()
{
	if (LaySteps.Num() == 0)
	{
		return false;
	}
	THEATER_SPEED_SCOPE(LayWarStep); // (tc.Speed.WarSpread: one step of a war's laying, a frame each)
	const TFunction<void()> Step = MoveTemp(LaySteps[0]);
	LaySteps.RemoveAt(0);
	if (Table && Session.IsValid() && Session->IsRunning())
	{
		Step();
	}
	return true;
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

void ACampaignGameMode::SaveCampaign(bool bAside)
{
	THEATER_SPEED_SCOPE(SaveCampaign); // (release 2, map run 7: the monthly save, written out, named in the speed log)
	if (!Session.IsValid() || !Session->IsRunning())
	{
		return;
	}
	const int32 Asked = ++SavesAsked;
	if (bAside && TheaterSpeed::SaveAside())
	{
		// (Release 2, tc.Speed.SaveAside.) The war written out on the rules' thread; its text onto the disk off this one.
		// A save asked after this one, at once or aside, is the one kept: this one is not written over it.
		const TWeakObjectPtr<ACampaignGameMode> Self(this);
		Session->SaveAside(true, [Self, Asked](FString Written)
		{
			ACampaignGameMode* Mode = Self.Get();
			if (Mode && Asked == Mode->SavesAsked && !Written.IsEmpty())
			{
				Mode->WriteSave(MoveTemp(Written));
			}
		});
		return;
	}
	const FString Text = Session->Save();
	WaitForSave(); // (a save still being written is finished first, so that it never lands over this one)
	if (!Text.IsEmpty())
	{
		FFileHelper::SaveStringToFile(Text, *SaveFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

void ACampaignGameMode::WriteSave(FString&& Text)
{
	THEATER_SPEED_SCOPE(SaveWrite);
	WaitForSave(); // (one at a time, in the order they were asked)
	const FString Path = SaveFile();
	// (Written beside the save and then put in its place: a game that stops part way through leaves the last save whole.)
	SaveWriting = Async(EAsyncExecution::ThreadPool, [Written = MoveTemp(Text), Path]()
	{
		const FString Beside = Path + TEXT(".writing");
		if (FFileHelper::SaveStringToFile(Written, *Beside, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			&& !IFileManager::Get().Move(*Path, *Beside, true, true))
		{
			FFileHelper::SaveStringToFile(Written, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM); // (it could not be put in place: written where it belongs)
		}
	});
}

void ACampaignGameMode::WaitForSave()
{
	if (SaveWriting.IsValid())
	{
		THEATER_SPEED_SCOPE(SaveWait);
		SaveWriting.Wait();
		SaveWriting = TFuture<void>();
	}
}

void ACampaignGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WaitForSave();
	Super::EndPlay(EndPlayReason);
}

FString ACampaignGameMode::DescribeSavedCampaign()
{
	FString Text;
	WaitForSave(); // (tc.Speed.SaveAside: the save is read once it is wholly written)
	if (!Session.IsValid() || !FFileHelper::LoadFileToString(Text, *SaveFile()))
	{
		return FString();
	}
	return Session->DescribeSave(Text);
}

FString ACampaignGameMode::SavedWarsFolder()
{
	// (a run that only takes pictures keeps its own, as it keeps its own campaign)
	FString Shots;
	const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bPictures ? TEXT("Saves-pictures") : TEXT("Saves"));
}

bool ACampaignGameMode::IsSavedWarId(const FString& Id)
{
	// (only a name this game made, "war-" and then figures and dashes: never a path to anywhere else)
	if (!Id.StartsWith(TEXT("war-")) || Id.Len() > 32)
	{
		return false;
	}
	for (int32 Index = 4; Index < Id.Len(); ++Index)
	{
		if (!FChar::IsDigit(Id[Index]) && Id[Index] != TEXT('-'))
		{
			return false;
		}
	}
	return true;
}

bool ACampaignGameMode::WriteSavedWar(const FString& Text, FSavedWar& Out)
{
	if (Text.IsEmpty() || !Session.IsValid())
	{
		return false;
	}
	const FString Folder = SavedWarsFolder();
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FDateTime Now = FDateTime::Now();
	const FString Stamp = Now.ToString(TEXT("war-%Y%m%d-%H%M%S"));
	FString Id = Stamp;
	for (int32 Second = 2; IFileManager::Get().FileExists(*(Folder / (Id + TEXT(".json")))); ++Second)
	{
		Id = FString::Printf(TEXT("%s-%d"), *Stamp, Second); // (two in the same second)
	}
	FString Line = Session->DescribeSave(Text);
	if (Line.IsEmpty())
	{
		Line = TEXT("A saved war");
	}
	if (!FFileHelper::SaveStringToFile(Text, *(Folder / (Id + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		// (the 1.5.1 pre-check's mend, 24 November 2026: a war that could not be written whole, the disk full, is not
		// left part-written, so the page never lists a war that will not load)
		IFileManager::Get().Delete(*(Folder / (Id + TEXT(".json"))), false, false, true);
		UE_LOG(LogTheater, Warning, TEXT("The war could not be saved to %s"), *(Folder / (Id + TEXT(".json"))));
		return false;
	}
	FFileHelper::SaveStringToFile(Line, *(Folder / (Id + TEXT(".txt"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	Out.Id = Id;
	Out.Line = Line;
	Out.When = Now;
	UE_LOG(LogTheater, Display, TEXT("War saved as %s: %s"), *Id, *Line);
	return true;
}

bool ACampaignGameMode::SaveWarAs(FSavedWar& Out)
{
	if (!Session.IsValid() || !Session->IsRunning())
	{
		return false;
	}
	return WriteSavedWar(Session->Save(), Out);
}

bool ACampaignGameMode::KeepAutosaveAsWar(FSavedWar& Out)
{
	FString Text;
	WaitForSave(); // (release 2, tc.Speed.SaveAside: the save is read once it is wholly written)
	return FFileHelper::LoadFileToString(Text, *SaveFile()) && WriteSavedWar(Text, Out);
}

TArray<FSavedWar> ACampaignGameMode::SavedWars() const
{
	TArray<FSavedWar> Out;
	const FString Folder = SavedWarsFolder();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("war-*.json")), true, false);
	for (const FString& File : Files)
	{
		FSavedWar War;
		War.Id = FPaths::GetBaseFilename(File);
		if (!IsSavedWarId(War.Id))
		{
			continue;
		}
		FFileHelper::LoadFileToString(War.Line, *(Folder / (War.Id + TEXT(".txt"))));
		War.Line.TrimStartAndEndInline();
		if (War.Line.IsEmpty())
		{
			War.Line = TEXT("A saved war");
		}
		// (when it was saved, from its name: the computer's own clock, as the player read it)
		const FString Digits = War.Id.Mid(4, 15);
		const int32 Year = FCString::Atoi(*Digits.Mid(0, 4)), Month = FCString::Atoi(*Digits.Mid(4, 2)), Day = FCString::Atoi(*Digits.Mid(6, 2));
		const int32 Hour = FCString::Atoi(*Digits.Mid(9, 2)), Minute = FCString::Atoi(*Digits.Mid(11, 2)), Second = FCString::Atoi(*Digits.Mid(13, 2));
		if (Digits.Len() == 15 && FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, 0))
		{
			War.When = FDateTime(Year, Month, Day, Hour, Minute, Second);
		}
		Out.Add(War);
	}
	Out.Sort([](const FSavedWar& A, const FSavedWar& B) { return A.When != B.When ? A.When > B.When : A.Id > B.Id; });
	return Out;
}

bool ACampaignGameMode::LoadSavedWar(const FString& Id, FString& Why)
{
	FString Text;
	if (!IsSavedWarId(Id) || !Session.IsValid() || !FFileHelper::LoadFileToString(Text, *(SavedWarsFolder() / (Id + TEXT(".json")))))
	{
		Why = TEXT("That saved war could not be read.");
		return false;
	}
	// (as CONTINUE: a war saved on the wider world is not laid on a table of Europe alone)
	if (Text.Contains(TEXT("\"world\":1")) && !FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld")))
	{
		Why = TEXT("That war was saved on the whole world's map, and this game is laid with Europe alone.");
		return false;
	}
	if (!Session->Load(Text))
	{
		Why = TEXT("That saved war could not be read.");
		return false;
	}
	UE_LOG(LogTheater, Display, TEXT("Saved war %s loaded"), *Id);
	LayOutWar();
	return true;
}

bool ACampaignGameMode::DeleteSavedWar(const FString& Id)
{
	if (!IsSavedWarId(Id))
	{
		return false;
	}
	const FString Folder = SavedWarsFolder();
	IFileManager::Get().Delete(*(Folder / (Id + TEXT(".txt"))), false, false, true);
	return IFileManager::Get().Delete(*(Folder / (Id + TEXT(".json"))), false, false, true);
}

int64 ACampaignGameMode::SavedWarsBytes() const
{
	const FString Folder = SavedWarsFolder();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("war-*")), true, false);
	int64 Bytes = 0;
	for (const FString& File : Files)
	{
		Bytes += FMath::Max<int64>(0, IFileManager::Get().FileSize(*(Folder / File)));
	}
	return Bytes;
}

int64 ACampaignGameMode::FreeDiskBytes() const
{
	uint64 Total = 0, Free = 0;
	return FPlatformMisc::GetDiskTotalAndFreeSpace(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), Total, Free) ? static_cast<int64>(Free) : -1;
}

bool ACampaignGameMode::IsAutosaveKept()
{
	FString Kept;
	WaitForSave(); // (release 2, tc.Speed.SaveAside: the save is read once it is wholly written)
	const FString Folder = SavedWarsFolder();
	const int64 Size = IFileManager::Get().FileSize(*SaveFile());
	if (Size <= 0)
	{
		return false;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("war-*.json")), true, false);
	for (const FString& File : Files)
	{
		// (only one as long as it is read word for word)
		FString Text;
		if (IFileManager::Get().FileSize(*(Folder / File)) == Size && FFileHelper::LoadFileToString(Text, *(Folder / File)))
		{
			if (Kept.IsEmpty() && !FFileHelper::LoadFileToString(Kept, *SaveFile()))
			{
				return false;
			}
			if (Text.Equals(Kept, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
	}
	return false;
}

bool ACampaignGameMode::ContinueCampaign()
{
	FString Text;
	WaitForSave(); // (tc.Speed.SaveAside: the save is read once it is wholly written)
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
		}
	}
	Music = PieceFor(TEXT("theme"));
	if (Music)
	{
		StartPiece(4.0f);
		UE_LOG(LogTheater, Display, TEXT("Music: %d made pieces and %d of the score's nine, the theme %s"), Pieces.Num(), ScorePlayers.Num(), Music->IsPlaying() ? TEXT("playing") : TEXT("not playing"));
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
	return FMath::Min(MusicVolume * Opening, 1.0f) * (bMusicDucked ? 0.3f : 1.0f);
}

void ACampaignGameMode::SetMusicMood(const FString& InMood)
{
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
