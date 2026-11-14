#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignSounds.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignEnvironment.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignSession.h"
#include "TheaterCommand.h"
#include "HAL/FileManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Parse.h"

ACampaignPlayerController::ACampaignPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PrimaryActorTick.bCanEverTick = true;
}

ACampaignGameMode* ACampaignPlayerController::GetMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACampaignGameMode>() : nullptr;
}

FCampaignSession* ACampaignPlayerController::GetSession() const
{
	const ACampaignGameMode* Mode = GetMode();
	FCampaignSession* Session = Mode ? Mode->GetSession() : nullptr;
	return Session && Session->IsRunning() ? Session : nullptr;
}

int32 ACampaignPlayerController::NewSeed()
{
	// The development photographer wants the same war every time; a player wants a new one.
	FString Folder;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Folder))
	{
		return 1939;
	}
	return static_cast<int32>((FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond) % 900000) + 1;
}

void ACampaignPlayerController::RefreshOpening(bool bShow)
{
	ACampaignGameMode* Mode = GetMode();
	FCampaignSession* Rules = Mode ? Mode->GetSession() : nullptr;
	if (!Rules || !Map.IsValid())
	{
		return;
	}
	if (StartGroups.Num() == 0)
	{
		StartGroups = Rules->Starts();
	}
	Opening.Groups.Reset();
	for (const FStartGroup& Group : StartGroups)
	{
		FOpeningGroup& Out = Opening.Groups.AddDefaulted_GetRef();
		Out.Title = FText::FromString(Group.Title);
		for (const FStartSide& Side : Group.Sides)
		{
			FOpeningSide& Choice = Out.Sides.AddDefaulted_GetRef();
			Choice.Name = FText::FromString(Side.Name);
			Choice.Blurb = FText::FromString(Side.Blurb);
			Choice.Aims = FText::FromString(FString::Join(Side.Aims, TEXT("\n")));
			Choice.bWatch = Side.Nation.IsEmpty();
			Choice.Tag = Side.Nation;
			const FCampaignNation* Nation = Map->GetData().FindNation(Side.Nation);
			Choice.Color = Nation ? FLinearColor(Nation->Color) : FLinearColor(0.35f, 0.33f, 0.3f);
		}
	}

	// The grand campaign's days. Rules without one (the stand-in) leave the scenarios as the only page.
	Opening.Years.Reset();
	for (const FGrandYear& Year : Rules->GrandYears())
	{
		FOpeningYear& Out = Opening.Years.AddDefaulted_GetRef();
		Out.Year = FText::FromString(Year.Year);
		Out.Title = FText::FromString(Year.Title);
		Out.Note = FText::FromString(Year.Note);
		Out.From = Year.From;
	}
	// How hard it can be made; the fair level is offered unless another was picked before.
	Opening.Levels = Rules->GrandLevels();
	if (!Opening.Levels.IsValidIndex(Opening.LevelIndex))
	{
		Opening.LevelIndex = Opening.Levels.IndexOfByPredicate([Rules](const FGrandLevel& Level) { return Level.Id == Rules->FairLevel(); });
	}
	if (Opening.Years.Num() == 0)
	{
		Opening.Tab = 1;
	}
	else if (bShow && !Opening.bVisible)
	{
		// Each time the screen opens, a new war is on offer: new dice, and the list of countries read afresh.
		GrandSeed = NewSeed();
		Opening.YearIndex = FMath::Clamp(Opening.YearIndex, 0, Opening.Years.Num() - 1);
		LoadGrandNations();
	}

	Opening.SavedLine = FText::FromString(Mode->DescribeSavedCampaign());
	Opening.bCanReturn = Rules->IsRunning();
	Opening.bVisible = bShow;
	++Opening.Revision;
}

void ACampaignPlayerController::LoadGrandNations()
{
	ACampaignGameMode* Mode = GetMode();
	FCampaignSession* Rules = Mode ? Mode->GetSession() : nullptr;
	if (!Rules || !Opening.Years.IsValidIndex(Opening.YearIndex))
	{
		return;
	}
	const FString Chosen = Opening.Nations.IsValidIndex(Opening.NationIndex) ? Opening.Nations[Opening.NationIndex].Tag : FString();
	GrandNations = Rules->GrandNations(Opening.Years[Opening.YearIndex].From, GrandSeed);
	Opening.Nations.Reset();
	Opening.NationIndex = INDEX_NONE;
	bool bAmerica = false;
	for (const FGrandNation& Nation : GrandNations)
	{
		FOpeningNation& Out = Opening.Nations.AddDefaulted_GetRef();
		Out.Tag = Nation.Nation;
		Out.Name = FText::FromString(Nation.Name);
		Out.Blurb = FText::FromString(Nation.Blurb);
		Out.Standing = FText::FromString(Nation.Standing);
		Out.Strength = FText::FromString(FString::Printf(TEXT("%d corps, %d %s"), Nation.Corps, Nation.Regions, Nation.Regions == 1 ? TEXT("territory") : TEXT("territories")));
		Out.bMajor = Nation.bMajor;
		Out.bSmall = Nation.bSmall;
		Out.Abilities = Nation.Abilities;
		bAmerica |= Nation.Nation == TEXT("USA");
		if (Nation.Nation == Chosen)
		{
			Opening.NationIndex = Opening.Nations.Num() - 1; // the same country stays chosen when the year changes
		}
	}
	Opening.GrandNote = bAmerica ? FText::GetEmpty() : FText::FromString(
		TEXT("The United States is not on this map yet, so it is not on the list. When the world map is in, it will be here from the first day."));
}

void ACampaignPlayerController::ShowOpeningTab(int32 Tab)
{
	Opening.Tab = Tab == 2 ? 2 : Opening.Years.Num() == 0 ? 1 : FMath::Clamp(Tab, 0, 1);
	++Opening.Revision;
}

FString ACampaignPlayerController::OptionsFile()
{
	// (a run that only takes pictures keeps its own, as it keeps its own campaign)
	FString Shots;
	const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), bPictures ? TEXT("options-pictures.json") : TEXT("options.json"));
}

TArray<FGameOption> ACampaignPlayerController::GetOptions() const
{
	TArray<FGameOption> Out;
	auto Add = [&Out](const TCHAR* Id, const TCHAR* Name, const TCHAR* Blurb, const TCHAR* State, bool bOn)
	{
		FGameOption& Option = Out.AddDefaulted_GetRef();
		Option.Id = Id;
		Option.Name = FText::FromString(Name);
		Option.Blurb = FText::FromString(Blurb);
		Option.State = FText::FromString(State);
		Option.bOn = bOn;
	};
	const ACampaignGameMode* Mode = GetMode();
	const bool bMusic = Mode && Mode->IsMusicOn();
	const int32 Loud = Mode ? Mode->GetMusicLevel() : 1;
	int32 Keeping = 0;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ACampaignEnvironment> Room(World);
		Keeping = Room ? Room->GetTimeKeeping() : 0;
	}
	// The war cabinet (the grand campaign): a player who wants the war without the politics switches it off.
	// (This one belongs to the campaign and is saved with it, not with the other options. It stands first, where it
	// is seen without scrolling.)
	if (const FCampaignSession* Session = GetSession())
	{
		if (Session->IsRunning() && Session->GetView().bCabinetCan)
		{
			const bool bSits = !Session->GetView().bCabinetOff;
			Add(TEXT("cabinet"), TEXT("The war cabinet"), TEXT("Ministers, decrees, the cabinet's trust and its directives. Off: none of it, and nothing it moved is moved. It can be switched in any month, and this campaign remembers it."), bSits ? TEXT("ON") : TEXT("OFF"), bSits);
		}
	}
	Add(TEXT("music"), TEXT("Music"), TEXT("The game's own score, which follows the war. Key M."), bMusic ? TEXT("ON") : TEXT("OFF"), bMusic);
	Add(TEXT("loud"), TEXT("How loud the music plays"), TEXT("From soft, under the sounds of the table, to full."), Loud == 0 ? TEXT("SOFT") : Loud == 1 ? TEXT("MIDDLE") : TEXT("FULL"), bMusic);
	Add(TEXT("sounds"), TEXT("Sounds"), TEXT("Boots, engines, guns, aircraft and the click of every button. Key N."), CampaignSounds::IsOn() ? TEXT("ON") : TEXT("OFF"), CampaignSounds::IsOn());
	Add(TEXT("newsreels"), TEXT("Newsreels"), TEXT("A month's great stories shown as film when it ends. Key K; Space skips one."), bNewsreels ? TEXT("ON") : TEXT("OFF"), bNewsreels);
	Add(TEXT("narrator"), TEXT("The narrator"), TEXT("The game's voice: it reads the newsreels and the opening flight aloud, speaks for the adviser, and lets your armies answer."), bNarrator ? TEXT("ON") : TEXT("OFF"), bNarrator);
	Add(TEXT("briefing"), TEXT("The opening flight"), TEXT("A flight over the situation when a campaign begins, and a title card as each year opens."), bBriefings ? TEXT("ON") : TEXT("OFF"), bBriefings);
	Add(TEXT("adviser"), TEXT("The adviser"), TEXT("A staff officer who points out the first steps of a campaign."), bAdviserWanted ? TEXT("ON") : TEXT("OFF"), bAdviserWanted);
	Add(TEXT("watch"), TEXT("The other nations' month"), TEXT("At a month's end: watch every nation's moves, or only what touches you. Key V."), bWatchAll ? TEXT("EVERY MOVE") : TEXT("ONLY MINE"), bWatchAll);
	Add(TEXT("day"), TEXT("Day and night in the room"), TEXT("The day passes over the table in twelve minutes, or the hour is held. Key T."), Keeping == 0 ? TEXT("PASSING") : Keeping == 1 ? TEXT("ALWAYS DAY") : TEXT("ALWAYS NIGHT"), Keeping == 0);
	{
		bool bLamp = false;
		if (UWorld* World = GetWorld())
		{
			TActorIterator<ACampaignEnvironment> Room(World);
			bLamp = Room && Room->IsLamplight();
		}
		Add(TEXT("lamp"), TEXT("The night"), TEXT("Moonlight, cool and blue, with the blackout over countries at war. Or the room's own warm lamp."), bLamp ? TEXT("LAMPLIGHT") : TEXT("MOONLIGHT"), !bLamp);
	}
	{
		const int32 Strength = Map.IsValid() ? Map->GetColourStrength() : 1;
		Add(TEXT("colour"), TEXT("How strongly countries are coloured"), TEXT("Light shows more of the land itself, strong more of whose it is. Key Y has flat colours."), Strength == 0 ? TEXT("LIGHT") : Strength == 1 ? TEXT("MIDDLE") : TEXT("STRONG"), Strength == 2);
	}
	Add(TEXT("edge"), TEXT("Sliding the map at the window's edge"), TEXT("Rest the pointer at the window's very edge and the table slides that way."), bEdgeScroll ? TEXT("ON") : TEXT("OFF"), bEdgeScroll);
	return Out;
}

void ACampaignPlayerController::PressOption(const FString& Id)
{
	ACampaignGameMode* Mode = GetMode();
	if (Id == TEXT("music") && Mode && Mode->HasMusic())
	{
		Mode->ToggleMusic();
	}
	else if (Id == TEXT("loud") && Mode)
	{
		Mode->SetMusicLevel((Mode->GetMusicLevel() + 1) % 3);
	}
	else if (Id == TEXT("sounds"))
	{
		CampaignSounds::Toggle();
	}
	else if (Id == TEXT("newsreels"))
	{
		bNewsreels = !bNewsreels;
	}
	else if (Id == TEXT("narrator"))
	{
		bNarrator = !bNarrator;
		if (!bNarrator)
		{
			HushNarrator();
		}
	}
	else if (Id == TEXT("briefing"))
	{
		bBriefings = !bBriefings;
	}
	else if (Id == TEXT("adviser"))
	{
		bAdviserWanted = !bAdviserWanted;
		if (!bAdviserWanted && Adviser.bOn)
		{
			DismissAdviser();
		}
	}
	else if (Id == TEXT("watch"))
	{
		bWatchAll = !bWatchAll;
	}
	else if (Id == TEXT("day"))
	{
		if (UWorld* World = GetWorld())
		{
			TActorIterator<ACampaignEnvironment> Room(World);
			if (Room)
			{
				Room->NextTimeKeeping();
			}
		}
	}
	else if (Id == TEXT("edge"))
	{
		bEdgeScroll = !bEdgeScroll;
	}
	else if (Id == TEXT("lamp"))
	{
		if (UWorld* World = GetWorld())
		{
			TActorIterator<ACampaignEnvironment> Room(World);
			if (Room)
			{
				Room->SetLamplight(!Room->IsLamplight());
			}
		}
	}
	else if (Id == TEXT("colour") && Map.IsValid())
	{
		Map->SetColourStrength((Map->GetColourStrength() + 1) % 3);
	}
	else if (Id == TEXT("cabinet"))
	{
		if (FCampaignSession* Session = GetSession())
		{
			FString Note;
			const bool bSwitchOn = Session->GetView().bCabinetOff;
			if (Session->Act(bSwitchOn ? TEXT("cabinet:switch:on") : TEXT("cabinet:switch:off"), INDEX_NONE, Note))
			{
				ShowWar(Session->GetView(), TArray<FCorpsMove>());
				RefreshOrders();
			}
			if (!Note.IsEmpty())
			{
				Notify(Note, 6.0);
			}
		}
		return; // (kept by the campaign, not with the options)
	}
	SaveOptions();
}

void ACampaignPlayerController::SaveOptions() const
{
	const ACampaignGameMode* Mode = GetMode();
	int32 Keeping = 0;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ACampaignEnvironment> Room(World);
		Keeping = Room ? Room->GetTimeKeeping() : 0;
	}
	auto Word = [](bool bYes) { return bYes ? TEXT("true") : TEXT("false"); };
	bool bLampSaved = false;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ACampaignEnvironment> Room(World);
		bLampSaved = Room && Room->IsLamplight();
	}
	const FString Text = FString::Printf(
		TEXT("{\"music\":%s,\"loud\":%d,\"sounds\":%s,\"newsreels\":%s,\"narrator\":%s,\"adviser\":%s,\"watchAll\":%s,\"day\":%d,\"edge\":%s,\"briefing\":%s,\"colour\":%d,\"lamp\":%s,\"helped\":%s}\n"),
		Word(!Mode || Mode->IsMusicOn()), Mode ? Mode->GetMusicLevel() : 1, Word(CampaignSounds::IsOn()), Word(bNewsreels), Word(bNarrator), Word(bAdviserWanted), Word(bWatchAll), Keeping, Word(bEdgeScroll), Word(bBriefings), Map.IsValid() ? Map->GetColourStrength() : 1, Word(bLampSaved), Word(bHelpSeen));
	FFileHelper::SaveStringToFile(Text, *OptionsFile());
	// (the options page, if it is up, is drawn again)
	++const_cast<ACampaignPlayerController*>(this)->Opening.Revision;
}

void ACampaignPlayerController::LoadOptions()
{
	// (a run that takes pictures starts from the choices as they come, unless told to keep what the last one left)
	FString Pictures;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Pictures, false) && !FParse::Param(FCommandLine::Get(), TEXT("TheaterKeepOptions")))
	{
		return;
	}
	FString Text;
	TSharedPtr<FJsonObject> Kept;
	if (!FFileHelper::LoadFileToString(Text, *OptionsFile()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Kept) || !Kept.IsValid())
	{
		return; // nothing kept yet: everything is as it comes
	}
	auto Yes = [&Kept](const TCHAR* Name, bool bElse) { bool bValue = bElse; Kept->TryGetBoolField(Name, bValue); return bValue; };
	int32 Number = 0;
	if (ACampaignGameMode* Mode = GetMode())
	{
		if (Kept->TryGetNumberField(TEXT("loud"), Number))
		{
			Mode->SetMusicLevel(Number);
		}
		if (Mode->HasMusic() && Mode->IsMusicOn() != Yes(TEXT("music"), true))
		{
			Mode->ToggleMusic();
		}
	}
	CampaignSounds::SetOn(Yes(TEXT("sounds"), true));
	bNewsreels = Yes(TEXT("newsreels"), true);
	bNarrator = Yes(TEXT("narrator"), true);
	bAdviserWanted = Yes(TEXT("adviser"), true);
	bWatchAll = Yes(TEXT("watchAll"), true);
	bEdgeScroll = Yes(TEXT("edge"), true);
	bBriefings = Yes(TEXT("briefing"), true);
	bHelpSeen = Yes(TEXT("helped"), false);
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ACampaignEnvironment> Room(World);
		if (Room)
		{
			Room->SetLamplight(Yes(TEXT("lamp"), false));
		}
	}
	if (Kept->TryGetNumberField(TEXT("colour"), Number) && Map.IsValid())
	{
		Map->SetColourStrength(Number);
	}
	if (Kept->TryGetNumberField(TEXT("day"), Number))
	{
		// (a run that takes pictures holds the hour it was asked for, whatever is kept)
		FString Shots;
		if (UWorld* World = GetWorld(); World && !FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false))
		{
			TActorIterator<ACampaignEnvironment> Room(World);
			if (Room)
			{
				Room->SetTimeKeeping(Number);
			}
		}
	}
	++Opening.Revision;
}

void ACampaignPlayerController::PickGrandYear(int32 Index, bool bAtOnce)
{
	if (!Opening.Years.IsValidIndex(Index) || (Index == Opening.YearIndex && !Opening.bWorking))
	{
		return;
	}
	Opening.YearIndex = Index;
	++Opening.Revision;
	if (bAtOnce || Opening.Years[Index].From <= 1)
	{
		Opening.bWorking = false;
		LoadGrandNations();
	}
	else
	{
		// Say that history is being played before playing it: the screen is drawn first, the work done after.
		Opening.bWorking = true;
		WorkingFrames = 0;
	}
}

void ACampaignPlayerController::PickGrandNation(int32 Index)
{
	Opening.NationIndex = Opening.Nations.IsValidIndex(Index) ? Index : INDEX_NONE;
	++Opening.Revision;
}

void ACampaignPlayerController::PickGrandNation(const FString& Tag)
{
	PickGrandNation(Opening.Nations.IndexOfByPredicate([&Tag](const FOpeningNation& Nation) { return Nation.Tag == Tag; }));
}

void ACampaignPlayerController::PickLevel(int32 Index)
{
	if (Opening.Levels.IsValidIndex(Index) && Index != Opening.LevelIndex)
	{
		Opening.LevelIndex = Index;
		++Opening.Revision;
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
}

void ACampaignPlayerController::BeginGrandCampaign(bool bWatch)
{
	ACampaignGameMode* Mode = GetMode();
	if (!Mode || Opening.bWorking || !Opening.Years.IsValidIndex(Opening.YearIndex) || (!bWatch && !Opening.Nations.IsValidIndex(Opening.NationIndex)))
	{
		return;
	}
	const FString Human = bWatch ? FString() : Opening.Nations[Opening.NationIndex].Tag;
	const FString Level = Opening.Levels.IsValidIndex(Opening.LevelIndex) ? Opening.Levels[Opening.LevelIndex].Id : FString();
	if (Mode->StartCampaign(Human, Opening.Years[Opening.YearIndex].From, TEXT("grand"), GrandSeed, Level))
	{
		bFreshCampaign = true;
		EnterCampaign();
		CampaignSounds::Play(this, TEXT("bugle"), 0.6f);
	}
}

void ACampaignPlayerController::BeginCampaign(int32 GroupIndex, int32 SideIndex)
{
	ACampaignGameMode* Mode = GetMode();
	if (!Mode || !StartGroups.IsValidIndex(GroupIndex) || !StartGroups[GroupIndex].Sides.IsValidIndex(SideIndex))
	{
		return;
	}
	const FStartGroup& Group = StartGroups[GroupIndex];
	if (Mode->StartCampaign(Group.Sides[SideIndex].Nation, Group.From, Group.ViewMode, NewSeed()))
	{
		bFreshCampaign = true;
		EnterCampaign();
	}
}

void ACampaignPlayerController::ContinueCampaign()
{
	ACampaignGameMode* Mode = GetMode();
	if (Mode && Mode->ContinueCampaign())
	{
		EnterCampaign();
	}
}

void ACampaignPlayerController::OpenMenu()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	if (bWatching || FaceOff.bVisible)
	{
		return;
	}
	if (ACampaignGameMode* Mode = GetMode())
	{
		Mode->SaveCampaign();
	}
	RefreshOpening(true);
}

void ACampaignPlayerController::CloseMenu()
{
	if (Opening.bCanReturn)
	{
		Opening.bVisible = false;
	}
}

void ACampaignPlayerController::EnterCampaign()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	// A clean table: nothing in hand, nothing picked, no old news.
	Opening.bVisible = false;
	FaceOff = FFaceOff();
	CloseDocument();
	bWatching = false;
	ArmyRegion = INDEX_NONE;
	PickedCorps.Reset();
	RefreshOrders();
	SelectRegion(INDEX_NONE);
	DispatchText.Reset();
	DispatchUntil = 0.0;

	// Open on the player's capital, or stay where the view is when nobody is being led.
	const FCampaignView& View = Session->GetView();
	const FCampaignMapData& Data = Map->GetData();
	const FCampaignNation* Nation = Data.FindNation(View.Human);
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		for (const FCampaignRegion& Region : Data.Regions)
		{
			if (Region.bCapital && Region.Owner == View.Human)
			{
				Eye->GoTo(Map->MapToWorld(Region.Center.X, Region.Center.Y), 0.5f);
				break;
			}
		}
	}
	// The adviser stands by for the first months of a campaign with somebody at the table.
	{
		const int32 Next = Adviser.Revision + 1; // (so that the screen writes his list afresh)
		Adviser = FAdviser();
		bAdviserSpoke = false;
		Adviser.Revision = Next;
		Adviser.bOn = Nation != nullptr && View.Turn <= 2 && !bScripted && bAdviserWanted;
	}
	Notify(Nation
		? FString::Printf(TEXT("%s. You lead %s.%s"), *View.Date, *Nation->Name, View.bEve ? TEXT(" Place your army, then end the month.") : TEXT(""))
		: FString::Printf(TEXT("%s. End the month to watch the war."), *View.Date), 7.0);
	// A campaign just begun opens with a flight over the situation, a moment after the table is laid.
	BriefingAt = bFreshCampaign && Nation && bBriefings && !bScripted ? FPlatformTime::Seconds() + 1.4 : 0.0;
	bFreshCampaign = false;
	LastYear = FYearCard();
	WaitingReel.Reset();
}

void ACampaignPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HushNarrator();
	// Leaving the game keeps the campaign.
	if (ACampaignGameMode* Mode = GetMode())
	{
		Mode->SaveCampaign();
	}
	Super::EndPlay(EndPlayReason);
}

void ACampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	if (TActorIterator<ACampaignMapActor> It(GetWorld()); It)
	{
		Map = *It;
	}

	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		if (Map.IsValid() && Map->GetData().IsLoaded())
		{
			Eye->SetBounds(Map->GetWorldHalfExtent());
			// Open on central Europe, with Germany, Poland and France all in view.
			const int32* Start = Map->GetData().RegionByName.Find(TEXT("Frankfurt"));
			const FVector2D At = Start ? Map->GetData().Regions[*Start].Center : FVector2D(Map->GetData().Width * 0.42, Map->GetData().Height * 0.46);
			Eye->GoTo(Map->MapToWorld(At.X + 60.0, At.Y + 10.0), 0.56f, true);
		}
	}

	// The opening screen asks which campaign to play, unless one was started from the command line.
	RefreshOpening(GetSession() == nullptr);
}

void ACampaignPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ACampaignPlayerController::WheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ACampaignPlayerController::WheelDown);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACampaignPlayerController::Escape);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ACampaignPlayerController::EndMonth);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACampaignPlayerController::SpaceBar);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACampaignPlayerController::ToggleMusic);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ACampaignPlayerController::OpenGuide);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ACampaignPlayerController::ToggleEffects);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACampaignPlayerController::NextArmy);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ACampaignPlayerController::OpenForces);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACampaignPlayerController::OpenReports);
	InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ACampaignPlayerController::OpenGoals);
	InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ACampaignPlayerController::ToggleWealth);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ACampaignPlayerController::ToggleDayNight);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ACampaignPlayerController::ToggleNewsreels);
	InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ACampaignPlayerController::GoToCapital);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ACampaignPlayerController::GoToCapital);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ACampaignPlayerController::ToggleWatchAll);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACampaignPlayerController::OpenScreen1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACampaignPlayerController::OpenScreen2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACampaignPlayerController::OpenScreen3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACampaignPlayerController::OpenScreen4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACampaignPlayerController::OpenScreen5);
}

void ACampaignPlayerController::WheelUp()
{
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->ZoomBy(-1.0f);
	}
}

void ACampaignPlayerController::WheelDown()
{
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->ZoomBy(1.0f);
	}
}

void ACampaignPlayerController::SetStance(const FString& Posture)
{
	FCampaignSession* Session = GetSession();
	if (!Session || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0)
	{
		return;
	}
	Session->SetStance(PickedCorps, Posture);
	RefreshOrders();
	Notify(Posture == TEXT("dig") ? TEXT("They will dig in. After a whole month on this ground they defend a quarter stronger.")
		: Posture == TEXT("back") ? (IsAutoDig() ? TEXT("They will fall back whole if they are attacked at odds they cannot hold. An army told to give ground leaves its fieldworks.") : TEXT("They will fall back whole if they are attacked at odds they cannot hold."))
		: IsAutoDig() ? TEXT("They will stand and fight where they are. Left there for the whole month, without marching or attacking, they dig in: a line that is dug in and fed is hard to break.")
		: TEXT("They will stand and fight where they are."), IsAutoDig() ? 6.0 : 4.0);
}

void ACampaignPlayerController::ToggleRail()
{
	if (ArmyRegion == INDEX_NONE)
	{
		return;
	}
	bByRail = !bByRail;
	bBySea = false;
	RefreshOrders();
	if (bByRail)
	{
		Notify(Reach.Num() > 0 ? TEXT("By rail: right-click any ground tinted blue. It takes the corps' whole month.")
			: TEXT("None of these corps can go by rail this month: it takes a whole month's moves."), 5.0);
	}
}

void ACampaignPlayerController::ToggleSea()
{
	if (ArmyRegion == INDEX_NONE)
	{
		return;
	}
	bBySea = !bBySea;
	bByRail = false;
	RefreshOrders();
	if (bBySea)
	{
		Notify(Reach.Num() > 0 ? TEXT("By sea: ground tinted blue is a friendly port they can be shipped to, or an enemy coast they can land on. It takes the corps' whole month.")
			: TEXT("None of these corps can go by sea this month: they must stand in a port, with their whole month before them, and there must be shipping."), 6.0);
	}
}

void ACampaignPlayerController::ProvinceAction(const FString& Action)
{
	// An order to a fleet or an air fleet, from its panel.
	if (Action.StartsWith(TEXT("fleet:")) || Action.StartsWith(TEXT("air:")) || Action.StartsWith(TEXT("airbase:")))
	{
		if (FCampaignSession* Navy = GetSession())
		{
			FString Said;
			const bool bGiven = Navy->OrderForce(Action, Said);
			ShowWar(Navy->GetView(), TArray<FCorpsMove>());
			ShowForce(bShownFleet, ShownForce);
			Notify(Said, 4.0);
			if (bGiven)
			{
				CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
				CampaignSounds::PlayLater(this, Action.StartsWith(TEXT("fleet:")) ? TEXT("ship_guns") : (Action.Contains(TEXT(":bomb:")) || Action.Contains(TEXT(":night:")) ? TEXT("bomber_drone") : TEXT("fighter_pass")), 0.3f, 0.3f);
			}
		}
		return;
	}
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || Map->GetSelectedRegion() == INDEX_NONE)
	{
		return;
	}
	FString Note;
	TArray<FSortie> Sorties;
	const bool bDone = Session->OrderInProvince(Action, Map->GetSelectedRegion(), Note, &Sorties);
	if (bDone && Action.StartsWith(TEXT("work:")))
	{
		// The table shows it at once: the bridge down, the line cut; and it is heard.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		// A reconnaissance flight is seen to go out and come back.
		for (const FSortie& Sortie : Sorties)
		{
			Map->FlySortie(Sortie);
		}
		const bool bAssault = Action.StartsWith(TEXT("work:storm")) || Action.StartsWith(TEXT("work:sally"));
		if (bAssault)
		{
			// An assault on a town, or its garrison's sortie: the guns are seen and heard, and the march plays.
			Map->AddBattleSmoke(Map->GetSelectedRegion());
			BattleMusicUntil = FPlatformTime::Seconds() + 14.0;
			CampaignSounds::PlayLater(this, TEXT("field_gun"), 0.5f, 0.55f);
		}
		CampaignSounds::Play(this, Sorties.Num() > 0 ? TEXT("fighter_pass") : (bAssault || Action.StartsWith(TEXT("work:blow"))) ? TEXT("explosion") : TEXT("hammer"), 0.6f);
	}
	RefreshRegionPanel(Map->GetSelectedRegion());
	Notify(Note, 5.0);
}

void ACampaignPlayerController::OpenTownPlan()
{
	if (Map.IsValid() && Map->GetSelectedRegion() != INDEX_NONE)
	{
		OpenPage(FString::Printf(TEXT("town:%d"), Map->GetSelectedRegion()));
	}
}

void ACampaignPlayerController::CancelOrder(int32 OrderId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	Notify(Session->CancelOrder(OrderId) ? TEXT("The order is taken back and the money returned.") : TEXT("An order can only be taken back in the month it was placed."), 4.0);
	RefreshRegionPanel(Map->GetSelectedRegion());
}

namespace
{
	/** Money with its thousands marked off, and a sign in front when asked for. */
	FString Money(int32 Amount, bool bSigned = false)
	{
		const FString Figure = FText::AsNumber(FMath::Abs(Amount)).ToString();
		return Amount < 0 ? TEXT("\u2212") + Figure : (bSigned && Amount > 0 ? TEXT("+") + Figure : Figure);
	}
}

FText ACampaignPlayerController::GetTreasuryText() const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Session->GetView().Treasury.bValid)
	{
		return FText::GetEmpty();
	}
	const FTreasury& Treasury = Session->GetView().Treasury;
	FString Line = FString::Printf(TEXT("Treasury %s"), *Money(Treasury.Money));
	if (!Treasury.bBudget)
	{
		Line += FString::Printf(TEXT("   %s a month"), *Money(Treasury.Gross, true));
	}
	if (Treasury.Orders.Num() > 0)
	{
		Line += FString::Printf(TEXT("  \u00B7  %d on order"), Treasury.Orders.Num());
	}
	return FText::FromString(Line);
}

FText ACampaignPlayerController::GetBudgetText() const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Session->GetView().Treasury.bValid || !Session->GetView().Treasury.bBudget)
	{
		return FText::GetEmpty();
	}
	const FTreasury& Treasury = Session->GetView().Treasury;
	return FText::FromString(FString::Printf(TEXT("Each month: %s in, %s upkeep, %s left"),
		*Money(Treasury.Gross), *Money(Treasury.Upkeep), *Money(Treasury.Net, true)));
}

bool ACampaignPlayerController::IsInDebt() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().Treasury.bValid && Session->GetView().Treasury.Money < 0;
}

void ACampaignPlayerController::OpenLedger()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	ClosePage();
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseDiplomacy();
	CloseResearch();
	CloseGenerals();
	const int32 Revision = Ledger.Revision + 1;
	Ledger = Session->Ledger();
	Ledger.Revision = Revision;
	bLedgerOpen = Ledger.bValid;
}

FString ACampaignPlayerController::GetLedgerNationTag() const
{
	const FCampaignSession* Session = GetSession();
	return Session ? Session->GetView().Human : FString();
}

void ACampaignPlayerController::CloseLedger()
{
	bLedgerOpen = false;
}

void ACampaignPlayerController::OpenPage(const FString& Id)
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseDiplomacy();
	CloseResearch();
	CloseGenerals();
	const int32 Revision = Page.Revision + 1;
	Page = Session->Page(Id, SelectedRegion);
	Page.Revision = Revision;
	bPageOpen = Page.bValid;
	if (bPageOpen && Id == TEXT("airfield"))
	{
		AdviserSaw(3);
	}
}

void ACampaignPlayerController::ClosePage()
{
	bPageOpen = false;
}

void ACampaignPlayerController::PressPage(const FString& Action)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bPageOpen)
	{
		return;
	}
	if (Action.StartsWith(TEXT("goto:")))
	{
		// A way from the list of forces to one of them on the table: the view goes there, and it is picked up or opened.
		TArray<FString> Parts;
		Action.ParseIntoArray(Parts, TEXT(":"));
		const int32 Number = Parts.Num() >= 3 ? FCString::Atoi(*Parts[2]) : INDEX_NONE;
		ClosePage();
		ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
		if (Parts.Num() >= 3 && Parts[1] == TEXT("fleet") && Map.IsValid())
		{
			PickUpFleet(Number);
			const FVector2D At = Map->FleetPlace(Number);
			if (Eye)
			{
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Clamp(Eye->GetZoom(), 0.4f, 0.6f), false);
			}
		}
		else if (Parts.Num() >= 3 && Map.IsValid() && Map->GetData().Regions.IsValidIndex(Number))
		{
			const FVector2D At = Map->GetData().Regions[Number].Center;
			if (Eye)
			{
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
			}
			PutDownFleet();
			SelectRegion(Number);
			if (Parts[1] == TEXT("field"))
			{
				OpenPage(TEXT("airfield"));
			}
			else if (Parts[1] == TEXT("army") && PickUpArmyIn(Number))
			{
				CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
			}
		}
		return;
	}
	if (Action.StartsWith(TEXT("open:")))
	{
		// A door from one room to another: the raid planner from the airfield, and back.
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
		const int32 Opened = Page.Revision + 1;
		Page = Session->Page(Action.Mid(5), SelectedRegion);
		Page.Revision = Opened;
		bPageOpen = Page.bValid;
		return;
	}
	FString Note, Next;
	TArray<FSortie> Sorties;
	FActExtras Extras;
	const bool bOk = Session->Act(Action, SelectedRegion, Note, &Sorties, &Next, &Extras);
	if (bOk && Extras.Joined != INDEX_NONE && FleetInHand != INDEX_NONE)
	{
		FleetInHand = Extras.Joined; // the fleet in hand is part of another now: that one is in hand
	}
	CampaignSounds::Play(this, bOk ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (bOk)
	{
		// Whatever it was may have changed the table: the fog lifted, the world at war.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, Sorties.Num() > 0 ? 12.0 : 8.0);
	}
	if (bOk && Extras.Shell != INDEX_NONE && Map.IsValid() && Map->GetData().Regions.IsValidIndex(Extras.Shell))
	{
		// The fleet stands in and opens fire: the room is put away, and the shells are seen to fall on the coast.
		Map->AddBattleSmoke(Extras.Shell);
		CampaignSounds::Play(this, TEXT("ship_guns"), 0.6f);
		CampaignSounds::PlayLater(this, TEXT("explosion"), 0.9f, 0.4f);
		BattleMusicUntil = FPlatformTime::Seconds() + 8.0;
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			const FVector2D At = Map->GetData().Regions[Extras.Shell].Center;
			Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Clamp(Eye->GetZoom(), 0.3f, 0.45f));
		}
		ClosePage();
		return;
	}
	if (bOk && Sorties.Num() > 0 && Map.IsValid())
	{
		BattleMusicUntil = FPlatformTime::Seconds() + 13.0;
		// A raid is flown the moment it is ordered: the room is put away, and the player watches every group
		// take off from its own airfield, meet over the target and come home again.
		bool bBombers = false;
		FVector2D Middle = FVector2D::ZeroVector;
		for (const FSortie& Sortie : Sorties)
		{
			Map->FlySortie(Sortie);
			bBombers |= Sortie.bBombers;
			Middle += Map->SortieStart(Sortie);
		}
		Middle = (Middle / Sorties.Num() + Sorties[0].To) * 0.5;
		CampaignSounds::Play(this, bBombers ? TEXT("bomber_drone") : TEXT("fighter_pass"), 0.45f);
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			// Far enough back to hold the airfields and the target in one look.
			double Spread = 0.0;
			for (const FSortie& Sortie : Sorties)
			{
				Spread = FMath::Max(Spread, FVector2D::Distance(Map->SortieStart(Sortie), Sortie.To));
			}
			Eye->GoTo(Map->MapToWorld(Middle.X, Middle.Y), FMath::Clamp(static_cast<float>(0.3 + Spread / 420.0), 0.4f, 0.85f));
		}
		ClosePage();
		return;
	}
	if (bOk && !Next.IsEmpty() && Next != Page.Id)
	{
		// The rules say which room comes next: back to the group once it has its orders.
		const int32 Opened = Page.Revision + 1;
		const FOfficePage Then = Session->Page(Next, SelectedRegion);
		if (Then.bValid)
		{
			Page = Then;
			Page.Revision = Opened;
			return;
		}
	}
	const FString Id = Page.Id;
	const int32 Revision = Page.Revision + 1;
	Page = Session->Page(Id, SelectedRegion);
	Page.Revision = Revision;
	bPageOpen = Page.bValid;
}

void ACampaignPlayerController::GetPageLinks(TArray<FString>& OutIds, TArray<FString>& OutLabels) const
{
	if (const FCampaignSession* Session = GetSession())
	{
		OutIds = Session->GetView().PageIds;
		OutLabels = Session->GetView().PageLabels;
	}
}

void ACampaignPlayerController::OpenGenerals()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	ClosePage();
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseDiplomacy();
	CloseResearch();
	const int32 Revision = Staff.Revision + 1;
	Staff = Session->Generals();
	Staff.Revision = Revision;
	bGeneralsOpen = Staff.bValid;
}

void ACampaignPlayerController::CloseGenerals()
{
	bGeneralsOpen = false;
}

void ACampaignPlayerController::RefreshGenerals(bool bArmiesChanged)
{
	FCampaignSession* Session = GetSession();
	if (!Session)
	{
		return;
	}
	if (bArmiesChanged)
	{
		// The army in hand and the territory panel name their generals: set them out again.
		RefreshOrders();
		RefreshRegionPanel(SelectedRegion);
	}
	const int32 Revision = Staff.Revision + 1;
	Staff = Session->Generals();
	Staff.Revision = Revision;
	bGeneralsOpen = Staff.bValid;
}

void ACampaignPlayerController::AssignGeneral(int32 GeneralId, int32 CorpsId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bGeneralsOpen)
	{
		return;
	}
	const bool bOk = Session->AssignGeneral(GeneralId, CorpsId);
	if (bOk)
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
	}
	else
	{
		Notify(TEXT("He cannot take that command this month."));
	}
	RefreshGenerals(bOk);
}

void ACampaignPlayerController::PromoteOfficer(int32 CorpsId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bGeneralsOpen)
	{
		return;
	}
	FString Name;
	const bool bOk = Session->PromoteOfficer(CorpsId, Name);
	if (bOk)
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
		Notify(FString::Printf(TEXT("%s is promoted from the ranks to command the corps."), *Name), 6.0);
	}
	else
	{
		Notify(TEXT("No officer could be promoted there this month."));
	}
	RefreshGenerals(bOk);
}

void ACampaignPlayerController::SackGeneral(int32 GeneralId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bGeneralsOpen)
	{
		return;
	}
	if (SackAsked != GeneralId)
	{
		// The first press asks; the second does it.
		SackAsked = GeneralId;
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
		RefreshGenerals(false);
		return;
	}
	SackAsked = INDEX_NONE;
	FString Note;
	const bool bOk = Session->SackGeneral(GeneralId, Note);
	CampaignSounds::Play(this, bOk ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (!Note.IsEmpty())
	{
		Notify(Note, 7.0);
	}
	RefreshGenerals(bOk);
}

void ACampaignPlayerController::ToggleDrafts()
{
	FCampaignSession* Session = GetSession();
	if (!Session || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0)
	{
		return;
	}
	FString Note;
	const bool bOk = Session->SetDrafts(PickedCorps, Tray.bLean, Note); // (lean now: so take them; taking now: so take none)
	CampaignSounds::Play(this, bOk ? TEXT("stamp") : TEXT("click"), 0.5f);
	RefreshOrders();
	Notify(Note, 6.0);
}

bool ACampaignPlayerController::HasEvePlan() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().bEvePlan && Session->GetView().bHumanUp && !bWatching && !Opening.bVisible;
}

void ACampaignPlayerController::PlaceByPlan()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !HasEvePlan())
	{
		return;
	}
	PutDownArmy();
	FString Note;
	const bool bOk = Session->PlaceByPlan(Note);
	CampaignSounds::Play(this, bOk ? TEXT("train") : TEXT("click"), 0.6f);
	ShowWar(Session->GetView(), TArray<FCorpsMove>());
	RefreshOrders();
	Notify(Note, 7.0);
}

void ACampaignPlayerController::OpenHeroes()
{
	CloseDocument();
	OpenPage(TEXT("heroes"));
}

void ACampaignPlayerController::RelieveGeneral(int32 GeneralId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bGeneralsOpen)
	{
		return;
	}
	const bool bOk = Session->RelieveGeneral(GeneralId);
	if (bOk)
	{
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
	RefreshGenerals(bOk);
}

void ACampaignPlayerController::SpendSkill(int32 GeneralId, const FString& Skill)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bGeneralsOpen)
	{
		return;
	}
	if (Session->SpendSkill(GeneralId, Skill))
	{
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
	RefreshGenerals(false);
}

void ACampaignPlayerController::OpenResearch()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	ClosePage();
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseDiplomacy();
	CloseGenerals();
	const int32 Revision = Research.Revision + 1;
	Research = Session->Research();
	Research.Revision = Revision;
	bResearchOpen = Research.bValid;
}

void ACampaignPlayerController::CloseResearch()
{
	bResearchOpen = false;
}

void ACampaignPlayerController::SetResearchTeams(const FString& Field, int32 Teams)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bResearchOpen)
	{
		return;
	}
	if (Session->SetTeams(Field, Teams))
	{
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
	const int32 Revision = Research.Revision + 1;
	Research = Session->Research();
	Research.Revision = Revision;
	bResearchOpen = Research.bValid;
}

void ACampaignPlayerController::StartTech(const FString& Id)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bResearchOpen)
	{
		return;
	}
	FString Why;
	if (Session->StartTech(Id, Why))
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
	}
	else if (!Why.IsEmpty())
	{
		Notify(Why, 6.0);
	}
	const int32 Revision = Research.Revision + 1;
	Research = Session->Research();
	Research.Revision = Revision;
	bResearchOpen = Research.bValid;
}

void ACampaignPlayerController::StopTech(const FString& Id)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bResearchOpen)
	{
		return;
	}
	if (Session->StopTech(Id))
	{
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
	const int32 Revision = Research.Revision + 1;
	Research = Session->Research();
	Research.Revision = Revision;
	bResearchOpen = Research.bValid;
}

const FViewStock* ACampaignPlayerController::GetMen() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().bMen && !Session->GetView().Human.IsEmpty() ? &Session->GetView().Men : nullptr;
}

bool ACampaignPlayerController::IsAutoDig() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().bAutoDig;
}

float ACampaignPlayerController::GetMoonlight() const
{
	return Map.IsValid() ? Map->GetMoonlight() : 0.0f;
}

void ACampaignPlayerController::GetMapLetters(TArray<FMapLetter>& Out) const
{
	Out.Reset();
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Map.IsValid() || !GetSession() || !Eye || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	// Countries' names are for the view from high up, and fade out on the way down. The seas' are always there,
	// faint.
	const float Distance = Eye->GetDistance();
	const float Countries = FMath::GetMappedRangeValueClamped(FVector2f(10500.0f, 12500.0f), FVector2f(0.0f, 0.92f), Distance);
	const float Seas = FMath::GetMappedRangeValueClamped(FVector2f(2500.0f, 6000.0f), FVector2f(0.0f, 0.6f), Distance);
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	auto OnScreen = [this](const FVector2D& MapPoint, double Lift, FVector2D& OutScreen)
	{
		return ProjectWorldLocationToScreen(Map->SurfacePoint(MapPoint.X, MapPoint.Y, Lift), OutScreen, false);
	};
	const float Provinces = FMath::GetMappedRangeValueClamped(FVector2f(6500.0f, 8500.0f), FVector2f(0.8f, 0.0f), Distance);
	for (const ACampaignMapActor::FMapWord& Word : Map->GetMapWords())
	{
		if (Word.bRegion)
		{
			// A province's name: one piece of lettering lying on the ground, read from wherever the table is seen.
			FVector2D Screen, East, North;
			if (Provinces <= 0.01f || !OnScreen(Word.From, 0.3, Screen) || Screen.X < -80.0 || Screen.X > Width + 80.0 || Screen.Y < -40.0 || Screen.Y > Height + 40.0
				|| !OnScreen(Word.From + FVector2D(Word.Height, 0.0), 0.3, East) || !OnScreen(Word.From - FVector2D(0.0, Word.Height), 0.3, North))
			{
				continue;
			}
			FMapLetter& Name = Out.AddDefaulted_GetRef();
			Name.Whole = Word.Text;
			Name.At = Screen;
			Name.Along = East - Screen;
			Name.Up = North - Screen;
			if (Name.Along.X < 0.0)
			{
				// (the table has been turned right round: the name is turned with it, so it still reads)
				Name.Along = -Name.Along;
				Name.Up = -Name.Up;
			}
			Name.Alpha = Provinces;
			continue;
		}
		const float Alpha = Word.bSea ? Seas : Countries;
		const int32 Count = Word.Text.Len();
		if (Alpha <= 0.01f || Count == 0)
		{
			continue;
		}
		FVector2D Run = Word.To - Word.From;
		const double Long = Run.Size();
		if (Long < 0.01)
		{
			continue;
		}
		Run /= Long;
		// If the table has been turned so that the word would read backwards, it is lettered the other way round.
		FVector2D First, Last;
		if (!OnScreen(Word.From, 0.3, First) || !OnScreen(Word.To, 0.3, Last))
		{
			continue;
		}
		if (FMath::Max(First.X, Last.X) < -200.0 || FMath::Min(First.X, Last.X) > Width + 200.0 || FMath::Max(First.Y, Last.Y) < -200.0 || FMath::Min(First.Y, Last.Y) > Height + 200.0)
		{
			continue;
		}
		const bool bTurned = Last.X < First.X;
		const FVector2D Start = bTurned ? Word.To : Word.From;
		const FVector2D Way = bTurned ? -Run : Run;
		const FVector2D Top(Way.Y, -Way.X);
		for (int32 K = 0; K < Count; ++K)
		{
			const TCHAR Char = Word.Text[K];
			if (Char == TEXT(' '))
			{
				continue;
			}
			const double T = Count > 1 ? static_cast<double>(K) / (Count - 1) : 0.5;
			// Along a slight arc: the middle of the word stands off the straight line, and each letter leans with it.
			const FVector2D Place = Start + Way * (Long * T) + Top * (Word.Bulge * 4.0 * T * (1.0 - T));
			const double Lean = Word.Bulge * 4.0 * (1.0 - 2.0 * T) / Long;
			const FVector2D Tangent = (Way + Top * Lean).GetSafeNormal();
			const FVector2D Above(Tangent.Y, -Tangent.X);
			FVector2D Screen, ScreenAlong, ScreenUp;
			if (!OnScreen(Place, 0.3, Screen) || !OnScreen(Place + Tangent * Word.Height, 0.3, ScreenAlong) || !OnScreen(Place + Above * Word.Height, 0.3, ScreenUp))
			{
				continue;
			}
			FMapLetter& Letter = Out.AddDefaulted_GetRef();
			Letter.Char = Char;
			Letter.At = Screen;
			Letter.Along = ScreenAlong - Screen;
			Letter.Up = ScreenUp - Screen;
			Letter.Alpha = Alpha;
			Letter.bSea = Word.bSea;
		}
	}
}

void ACampaignPlayerController::GetColoursOnScreen(TArray<FBox2D>& Out) const
{
	Out.Reset();
	if (!Map.IsValid() || !PlayerCameraManager || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const FVector Up = PlayerCameraManager->GetCameraRotation().Quaternion().GetUpVector();
	for (const TObjectPtr<AArmyPiece>& Piece : Map->GetPieces())
	{
		FVector Middle;
		double Radius = 0.0;
		FVector2D At, Top;
		if (Piece && Piece->GetColours(Middle, Radius) && ProjectWorldLocationToScreen(Middle, At, false) && ProjectWorldLocationToScreen(Middle + Up * Radius, Top, false)
			&& At.X > -60.0 && At.X < Width + 60.0 && At.Y > -60.0 && At.Y < Height + 60.0)
		{
			// (the cloth, and the strength plate that hangs under it)
			const double Half = FMath::Clamp(FVector2D::Distance(At, Top) * 0.8, 6.0, 90.0);
			Out.Add(FBox2D(At - FVector2D(Half, Half * 0.75), At + FVector2D(Half, Half * 1.35)));
		}
	}
}

void ACampaignPlayerController::GetTownPlates(TArray<FTownPlate>& Out) const
{
	Out.Reset();
	const FCampaignSession* Session = GetSession();
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Map.IsValid() || !Session || !Eye || !PlayerCameraManager || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	// Capitals from any distance, towns from the middle distance, every place worth having closer in, and what
	// each place is worth only close up: the table is never papered over with plates.
	const float Distance = Eye->GetDistance();
	const int32 State = Distance > 12500.0f ? 0 : (Distance > 5500.0f ? 1 : 2);
	const bool bWorth = Distance <= 4200.0f;
	FVector Toward = -PlayerCameraManager->GetCameraRotation().Vector();
	Toward.Z = 0.0;
	Toward = Toward.GetSafeNormal();
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const FCampaignView& View = Session->GetView();
	const FCampaignMapData& Data = Map->GetData();
	for (const ACampaignMapActor::FPlateSpot& Spot : Map->GetPlateSpots())
	{
		if (Spot.Tier > State)
		{
			continue;
		}
		FVector2D Screen;
		if (!ProjectWorldLocationToScreen(Spot.World + Toward * Spot.Reach, Screen, false)
			|| Screen.X < -120.0 || Screen.X > Width + 120.0 || Screen.Y < -20.0 || Screen.Y > Height + 20.0)
		{
			continue;
		}
		FTownPlate& Plate = Out.AddDefaulted_GetRef();
		Plate.At = Screen;
		Plate.Name = Spot.Name;
		Plate.Line = bWorth && Map->IsWealthShown() ? Spot.Line : FString(); // (what a place pays is on the pointer's tip, and on its plate only in the wealth view)
		Plate.bCapital = Spot.Tier == 0;
		Plate.bFar = State == 0;
		Plate.Rank = Spot.Rank;
		const FString Holder = View.RegionHolder.IsValidIndex(Spot.Region) ? View.RegionHolder[Spot.Region] : (Data.Regions.IsValidIndex(Spot.Region) ? Data.Regions[Spot.Region].Owner : FString());
		if (const FCampaignNation* Nation = Data.FindNation(Holder))
		{
			Plate.Colour = FLinearColor(Nation->Color);
		}
	}
	Out.Sort([](const FTownPlate& A, const FTownPlate& B) { return A.Rank > B.Rank; });
	// The town a directive of the war cabinet names wears a small brass seal with the months left on it, at any height;
	// and the town the player has promised the cabinet wears one in wax red. (Where they are the same town, side by side.)
	for (int32 Which = 0; Which < 2; ++Which)
	{
		const int32 SealAt = Which == 0 ? View.SealRegion : View.VowRegion;
		if (SealAt == INDEX_NONE || !Data.Regions.IsValidIndex(SealAt))
		{
			continue;
		}
		const FVector2D Place = Data.Regions[SealAt].Center;
		FVector2D Screen;
		if (ProjectWorldLocationToScreen(Map->MapToWorld(Place.X, Place.Y, 0.0) + FVector(0.0, 0.0, 3.0 * Map->UnitScale), Screen, false)
			&& Screen.X > -40.0 && Screen.X < Width + 40.0 && Screen.Y > -20.0 && Screen.Y < Height + 20.0)
		{
			FTownPlate& Seal = Out.AddDefaulted_GetRef();
			Seal.At = Screen - FVector2D(Which == 1 && View.SealRegion == View.VowRegion ? -52.0 : 0.0, 44.0);
			Seal.Name = FString::FromInt(FMath::Max(Which == 0 ? View.SealLeft : View.VowLeft, 0));
			Seal.bSeal = true;
			Seal.bVow = Which == 1;
			Seal.Rank = 1000; // (before any name plate: nothing crowds it out)
		}
	}
	// An airfield with air groups based on it wears a small badge: an aircraft and how many groups fly from it, on
	// its holder's colour. From working height and closer; from far off the map is left clear. (The groups
	// themselves are seen by clicking the airfield: no aircraft piece stands on the map for them.)
	if (State >= 1)
	{
		for (const FViewAirfield& Field : View.Airfields)
		{
			FVector World;
			FVector2D Screen;
			if (Field.Groups <= 0 || !Map->GetAirfieldWorld(Field.Region, World) || !ProjectWorldLocationToScreen(World, Screen, false)
				|| Screen.X < -40.0 || Screen.X > Width + 40.0 || Screen.Y < -20.0 || Screen.Y > Height + 20.0)
			{
				continue;
			}
			FTownPlate& Badge = Out.AddDefaulted_GetRef();
			Badge.At = Screen - FVector2D(0.0, 30.0);
			Badge.Name = FString::FromInt(Field.Groups);
			Badge.bAirfield = true;
			Badge.Rank = -1;
			if (const FCampaignNation* Nation = Data.FindNation(Field.Nation))
			{
				Badge.Colour = FLinearColor(Nation->Color);
			}
		}
	}
}

FString ACampaignPlayerController::GetToDoText(int32 Index) const
{
	const FCampaignSession* Session = GetSession();
	const bool bShown = Session && Session->GetView().bHumanUp && !bWatching && !Opening.bVisible && Session->GetView().ToDo.IsValidIndex(Index);
	return bShown ? Session->GetView().ToDo[Index].Text : FString();
}

FString ACampaignPlayerController::GetToDoTip(int32 Index) const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().ToDo.IsValidIndex(Index) ? Session->GetView().ToDo[Index].Tip : FString();
}

void ACampaignPlayerController::PressToDo(int32 Index)
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Session->GetView().ToDo.IsValidIndex(Index))
	{
		return;
	}
	const FString Screen = Session->GetView().ToDo[Index].Screen;
	if (Screen.StartsWith(TEXT("page:")))
	{
		OpenPage(Screen.Mid(5));
	}
	else if (Screen.StartsWith(TEXT("goto:")) && Map.IsValid())
	{
		// (a place on the map: the view goes there and the province is picked)
		const int32 Where = FCString::Atoi(*Screen.Mid(5));
		if (Map->GetData().Regions.IsValidIndex(Where))
		{
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
			{
				const FVector2D At = Map->GetData().Regions[Where].Center;
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), 0.3f);
			}
			SelectRegion(Where);
			SetProvinceTab(2);
		}
	}
	else if (Screen == TEXT("generals"))
	{
		OpenGenerals();
	}
	else if (Screen == TEXT("research"))
	{
		OpenResearch();
	}
	else if (Screen == TEXT("arsenal"))
	{
		OpenArsenal();
	}
}

int32 ACampaignPlayerController::GetIdleTeams() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().bHumanUp ? Session->GetView().IdleTeams : 0;
}

const FViewStock* ACampaignPlayerController::GetStock(bool bOil) const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Session->GetView().bStores || Session->GetView().Human.IsEmpty())
	{
		return nullptr;
	}
	return bOil ? &Session->GetView().Oil : &Session->GetView().Supplies;
}

void ACampaignPlayerController::FoundResearchBureau()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bResearchOpen)
	{
		return;
	}
	if (Session->FoundBureau())
	{
		CampaignSounds::Play(this, TEXT("coins"), 0.6f);
		Notify(TEXT("A new design team is founded. Put it on a branch with a + button."), 6.0);
		ShowWar(Session->GetView(), TArray<FCorpsMove>()); // the treasury has paid for it
	}
	const int32 Revision = Research.Revision + 1;
	Research = Session->Research();
	Research.Revision = Revision;
	bResearchOpen = Research.bValid;
}

void ACampaignPlayerController::OpenDiplomacy()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	ClosePage();
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseResearch();
	CloseGenerals();
	PathAsked.Reset();
	const int32 Revision = Diplomacy.Revision + 1;
	Diplomacy = Session->Diplomacy();
	Diplomacy.Revision = Revision;
	bDiplomacyOpen = Diplomacy.bValid;
}

void ACampaignPlayerController::CloseDiplomacy()
{
	bDiplomacyOpen = false;
}

void ACampaignPlayerController::CourtNation(const FString& Tag)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bDiplomacyOpen)
	{
		return;
	}
	FString Note;
	const bool bOk = Session->Court(Tag, Note);
	if (bOk)
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
		// A country that is won over changes sides: the table is set out again as it now stands.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 8.0);
	}
	const int32 Revision = Diplomacy.Revision + 1;
	Diplomacy = Session->Diplomacy();
	Diplomacy.Revision = Revision;
	bDiplomacyOpen = Diplomacy.bValid;
}

void ACampaignPlayerController::PressPath(const FString& Id, const FString& Partner)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bDiplomacyOpen)
	{
		return;
	}
	const FString Key = Partner.IsEmpty() ? Id : Id + TEXT(":") + Partner;
	if (Id.IsEmpty() || PathAsked != Key)
	{
		// The first press only asks: an alliance, once joined, is not left.
		PathAsked = Id.IsEmpty() ? FString() : Key;
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
		++Diplomacy.Revision;
		return;
	}
	PathAsked.Reset();
	FString Note;
	if (Session->TakePath(Id, Partner, Note))
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
		CampaignSounds::PlayLater(this, TEXT("bell"), 0.5f, 0.5f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 9.0);
	}
	const int32 Revision = Diplomacy.Revision + 1;
	Diplomacy = Session->Diplomacy();
	Diplomacy.Revision = Revision;
	bDiplomacyOpen = Diplomacy.bValid;
}

void ACampaignPlayerController::InviteToLeague(const FString& Tag)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bDiplomacyOpen)
	{
		return;
	}
	FString Note;
	if (Session->InviteToLeague(Tag, Note))
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 8.0);
	}
	const int32 Revision = Diplomacy.Revision + 1;
	Diplomacy = Session->Diplomacy();
	Diplomacy.Revision = Revision;
	bDiplomacyOpen = Diplomacy.bValid;
}

void ACampaignPlayerController::AskWarOn(const FString& Tag)
{
	if (!bDiplomacyOpen)
	{
		return;
	}
	CloseDiplomacy();
	if (OfferWarOn(Tag, INDEX_NONE))
	{
		WarPrompt.bFromOffice = true;
	}
	else
	{
		Notify(TEXT("War cannot be declared on them this month."));
		OpenDiplomacy();
	}
}

void ACampaignPlayerController::OpenArsenal()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	FCampaignSession* Session = GetSession();
	if (!Session || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Session->GetView().Human.IsEmpty())
	{
		return;
	}
	ClosePage();
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseLedger();
	CloseDiplomacy();
	CloseResearch();
	CloseGenerals();
	// The province in mind is the one selected, if it is the player's own; works are built there and corps form there.
	const FCampaignView& View = Session->GetView();
	const bool bMine = View.RegionHolder.IsValidIndex(SelectedRegion) && View.RegionHolder[SelectedRegion] == View.Human;
	const int32 Revision = Arsenal.Revision + 1;
	Arsenal = Session->Arsenal(bMine ? SelectedRegion : INDEX_NONE);
	Arsenal.Revision = Revision;
	bArsenalOpen = Arsenal.bValid;
}

void ACampaignPlayerController::CloseArsenal()
{
	bArsenalOpen = false;
}

void ACampaignPlayerController::OrderFromArsenal(const FString& Key)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !bArsenalOpen)
	{
		return;
	}
	const FCampaignView& View = Session->GetView();
	const bool bMine = View.RegionHolder.IsValidIndex(SelectedRegion) && View.RegionHolder[SelectedRegion] == View.Human;
	FString Name;
	if (Session->OrderArsenal(Key, bMine ? SelectedRegion : INDEX_NONE, Name))
	{
		Notify(FString::Printf(TEXT("Ordered: %s."), *Name), 5.0);
		CampaignSounds::Play(this, TEXT("coins"), 0.6f);
		CampaignSounds::PlayLater(this, CampaignSounds::OfOrder(Key), 0.35f, 0.35f);
	}
	else
	{
		Notify(TEXT("That could not be ordered."), 4.0);
	}
	// The prices, the treasury and what is on order have all moved: read the arsenal again.
	const int32 Revision = Arsenal.Revision + 1;
	Arsenal = Session->Arsenal(bMine ? SelectedRegion : INDEX_NONE);
	Arsenal.Revision = Revision;
	RefreshRegionPanel(SelectedRegion);
}

void ACampaignPlayerController::ShowProvince(int32 RegionId)
{
	CloseLedger();
	if (!Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return;
	}
	PutDownArmy();
	SelectRegion(RegionId);
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		const FVector2D At = Map->GetData().Regions[RegionId].Center;
		Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.3f), false);
	}
}

void ACampaignPlayerController::OpenRecord(int32 CorpsId)
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	if (FCampaignSession* Session = GetSession())
	{
		Document = Session->Record(CorpsId);
		++DocumentRevision;
		if (!Document.bValid)
		{
			Notify(TEXT("No record is kept of that corps."));
		}
	}
}

void ACampaignPlayerController::OpenRoll()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	if (bWatching || FaceOff.bVisible || Opening.bVisible)
	{
		return;
	}
	if (FCampaignSession* Session = GetSession())
	{
		Document = Session->Roll();
		// From the roll, the way to what can be made of the honours at home.
		if (!Session->GetView().Human.IsEmpty())
		{
			Document.Door = TEXT("heroes");
			Document.DoorLabel = TEXT("BOND TOURS AND INSTRUCTORS");
		}
		++DocumentRevision;
	}
}

void ACampaignPlayerController::OpenGuide()
{
	EndNewsreel(); // (a screen is never opened over a newsreel: the reel gives way to it)
	if (bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible)
	{
		return;
	}
	if (Document.bValid && Document.Title == TEXT("How to play"))
	{
		CloseDocument(); // the same key puts the page away again
		return;
	}
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseDiplomacy();
	CloseResearch();
	CloseGenerals();
	FDocumentPage Guide;
	Guide.bValid = true;
	Guide.Title = TEXT("How to play");
	Guide.Subtitle = TEXT("Theater Command, 1939 to 1945. Esc, or H again, closes this page.");
	auto Add = [&Guide](const TCHAR* Heading, const TCHAR* Body)
	{
		FDocumentSection& Section = Guide.Sections.AddDefaulted_GetRef();
		Section.Heading = Heading;
		Section.Body = Body;
	};
	Add(TEXT("THE TABLE"),
		TEXT("You are looking down at a map table. Drag to move across it, scroll to zoom, and right-drag (or press Q and E) to turn the view. ")
		TEXT("Every flag is an army, a fleet or an air fleet, and the number under it is its strength. Click any territory to see who holds it and what it has."));
	Add(TEXT("A MONTH"),
		TEXT("Each turn is one month. Give your orders, then press END THE MONTH, or Enter. The other nations move, and the month's news is shown at the top right. ")
		TEXT("Every corps has a few moves each month: marching through friendly ground costs one, and entering enemy ground costs more."));
	Add(TEXT("MOVING AND ATTACKING"),
		TEXT("Click one of your flags to pick the army up. Ground it can march to lightens, ground it can attack turns red, and a neutral country's ground beside it turns amber. ")
		TEXT("Click where it should go and click again to send it, or drag the flag there. An attack first shows both armies and the odds: SEND THEM IN fights it, CALL IT OFF does not. Esc puts the army down.\n")
		TEXT("In the army's tray you can leave some corps behind, say what it does if attacked (stand, dig in or fall back), and send it BY RAIL along a working railway or BY SEA from a port.\n")
		TEXT("What stands round an army's base says what it is about: sandbags and wire when it is dug in behind its fieldworks (a short row while it is still digging), tents and a fire when it is in camp and resting, ")
		TEXT("a lorry and crates while supply reaches it, and a red barred board when it is cut off. The dotted line to where an army would go has a ring for each province on the way, and ends in red where there would be a battle."));
	Add(TEXT("WAR AND PEACE"),
		TEXT("Marching onto amber ground means declaring war. The game first tells you who that country's friends are and what will follow, and nothing happens unless you say yes.\n")
		TEXT("DIPLOMACY shows who is at war, where every neutral stands between the Axis and the Allies, and lets you court a neutral with political capital. ")
		TEXT("A small country chooses its own path there: join a side, stand alone, or found the League with a willing neighbour."));
	Add(TEXT("SPIES, THE FOG OF WAR AND THE LAST ACT"),
		TEXT("In the grand campaign you see only what your armies and your agents can see. A foreign army far off shows as a flag with a question mark, or not at all. ")
		TEXT("DIPLOMACY has a door to THE SECRET SERVICE, where you recruit agents, send them to the province selected on the map, and give them work: watching, reading a government's books, sabotage, or guarding at home. The fog can be lifted there if you would rather see everything.\n")
		TEXT("When the war between the alliances is decided, DIPLOMACY gains a door to THE LAST ACT. You can take the victory and keep the peace, or choose World Domination, where every nation still standing fights every other."));
	Add(TEXT("MONEY AND BUILDING"),
		TEXT("Every province you hold pays into your treasury each month. The treasury is at the top left: click it, or LEDGER, for the books.\n")
		TEXT("The ARSENAL sells corps, aircraft, ships and works such as factories, shipyards, airfields and anti-aircraft guns. Click one of your own territories first, so the game knows where to build. Works you build stand on the map."));
	Add(TEXT("OIL AND SUPPLIES"),
		TEXT("In the grand campaign a great power keeps stocks of oil and of supplies. The two gauges under the treasury show how full the depots are and how many months they will last; rest the mouse on one to read where it comes from and where it goes. ")
		TEXT("Tanks, aircraft and ships burn oil, and the people at home and the army eat supplies. Much of both comes by sea along the dashed lanes on the map: pale where cargoes get through, amber where raiders are at them, red where the lane is cut. The game warns you when a stock has six months left."));
	Add(TEXT("RESEARCH AND GENERALS"),
		TEXT("RESEARCH is where your research teams get their work. In the grand campaign it is a tree of technologies: pick one for each idle team, because a team never chooses for itself, and the button at the top says when some are idle. ")
		TEXT("Some technologies unlock orders in the arsenal, and medium and heavy tanks, long-range fighters and heavy bombers change the models on your pieces. In a historical scenario research keeps to history's dates, and you move design teams between branches instead.\n")
		TEXT("GENERALS lists your commanders. Give a waiting general a corps, promote an officer from the ranks, and spend the skill points your generals earn in battle. A corps with no general fights by the book."));
	Add(TEXT("RIVERS, RAILWAYS AND ENGINEERS"),
		TEXT("The great rivers are crossed at bridges. Blow the bridges and nothing on wheels or tracks can cross until engineers rebuild them. ")
		TEXT("Armies are fed along the railways, and troops beyond a cut line fight weaker until engineers relay it. Click a province to see the work that can be ordered there."));
	Add(TEXT("FINDING YOUR WAY ABOUT"),
		TEXT("The small map in the corner shows every country in its colour, your armies in white, your allies' in blue and the enemy's in red. Click or drag on it to look somewhere else. ")
		TEXT("Under it: FORCES (L) lists every army, fleet and airfield you command with a way to each; REPORT (R) is what every nation did last month; GOALS (O) is what winning means and how far you have got; ")
		TEXT("CAPITAL (C) looks at your capital; and MAP VIEWS (Y) steps through the map as it is, the wealth view (gold by what a province pays, so you can see what is worth taking) and friend and foe (your ground green, your allies' blue, your enemies' red). ")
		TEXT("Click a province to see its chief town and THE PRIZE: what taking it would be worth to you. Its RECRUIT and WORKS tabs hold what can be ordered there. ")
		TEXT("The line beside END THE MONTH says how many armies can still move: click it, or press Tab, to go to the next. ")
		TEXT("The room's day goes slowly by, and at night the towns on the map light their windows; T holds it at day or at night. ")
		TEXT("The date plate says when the weather is against you: in the mud of the eastern spring and autumn the ground there turns brown and every army on it is slowed, and in the eastern winter armies without winter kit fight and march badly. ")
		TEXT("In the grand campaign, ground under a shadow is ground you have no eyes on: what stands there is not known until your troops, your allies, an agent or a reconnaissance flight come near."));
	Add(TEXT("YOUR ARMIES' CARDS"),
		TEXT("Pick an army up and each corps is a card, with a small card for each of its divisions: its name, its strength and its stripes of experience. ")
		TEXT("Two bars belong to the whole corps. MORALE rises with victories, on home soil, with friends near and with rest, and falls with defeats, hunger and want of pay; it is worth up to fifteen parts in a hundred in battle. ")
		TEXT("Where an ally's corps stand beside your army, the tray offers to ASK them TO JOIN: they march and attack under your orders, keep their own flag, and go home when you SEND them BACK or leave them a month without your troops. ")
		TEXT("REST is spent by marching and fighting. A corps that neither moves nor fights for a whole month rests: it takes replacements half as fast again, and its morale and rest rise. Point at the bars to read why they stand where they do."));
	Add(TEXT("FLEETS"),
		TEXT("A fleet is picked up like an army: click its flag, and its ships are dealt as cards. The seas it can reach this month are ringed in brass. Right-click a ringed sea to send it there, or its own harbour to bring it home. ")
		TEXT("PATROL seeks out the enemy's warships and guards your convoys; RAID SHIPPING goes for merchant ships and keeps clear of a fight; HUNT SUBMARINES sweeps a sea with the fleet's destroyers. ")
		TEXT("In the grand campaign a fleet with heavy guns can shell an enemy coast: the coasts it can reach turn red, and a right-click on one shows what the guns would do to the troops there or to the city before you give the word. ")
		TEXT("ORDERS opens everything else: every sea in reach with the enemy known there, the shipping lanes it could escort and what that would save, and the way to split ships off as a squadron of their own or join another fleet. ")
		TEXT("Heavy ships want destroyers round them: a thinly screened fleet risks a torpedo. ")
		TEXT("A corps in a port can go BY SEA to a friendly port or land on an enemy coast. Before a landing the game tells you what it will meet, and asks."));
	Add(TEXT("THE AIR WAR"),
		TEXT("Aircraft belong to airfields, which stand on the map: a landing ground, an airfield, an air base. Click an airfield to open it and see the air groups based there. ")
		TEXT("A fighter group patrols its own sky, sticks with a bomber group as escort, or rests. Any group can fly one raid a month. To plan a raid you pick its kind and its target, and you are shown the chance of success and the losses to expect. ")
		TEXT("Then you choose which groups fly it, from one airfield or several: bombers that meet up lose fewer of their number. The raid is flown at once, and you watch it go. ")
		TEXT("When a month with great events ends, a newsreel tells them over the map: Space skips it, and K turns newsreels off or on. ")
		TEXT("Engineers lay out new airfields and enlarge old ones; the ARSENAL builds bigger ones. In a historical scenario air fleets are given a mission for the month instead."));
	if (FCampaignSession* Session = GetSession())
	{
		// What the player's own country does in its own way, from the rules.
		bool bActive = false;
		const TArray<FString> Lines = Session->Abilities(bActive);
		if (Lines.Num() > 0)
		{
			FDocumentSection& Section = Guide.Sections.AddDefaulted_GetRef();
			Section.Heading = TEXT("YOUR NATION'S ABILITIES");
			Section.Body = FString::Join(Lines, TEXT("\n"));
			if (!bActive)
			{
				Section.Body += TEXT("\nThese count in the grand campaign only: a historical scenario is fought as history fought it.");
			}
		}
	}
	Add(TEXT("SAVING, MUSIC AND SOUND"),
		TEXT("The game saves itself every month and when you leave. MENU lets you continue or begin another campaign. M turns the music on and off, and N the sound effects."));
	Document = Guide;
	++DocumentRevision;
}

void ACampaignPlayerController::CloseDocument()
{
	Document = FDocumentPage();
	++DocumentRevision;
}

void ACampaignPlayerController::ToggleMusic()
{
	if (ACampaignGameMode* Mode = GetMode())
	{
		if (Mode->HasMusic())
		{
			Notify(Mode->ToggleMusic() ? TEXT("Music on.") : TEXT("Music off. Press M to bring it back."), 3.0);
			SaveOptions();
		}
	}
}

void ACampaignPlayerController::ToggleEffects()
{
	Notify(CampaignSounds::Toggle() ? TEXT("Sound effects on.") : TEXT("Sound effects off. Press N to bring them back."), 3.0);
	SaveOptions();
}

void ACampaignPlayerController::SpaceBar()
{
	if (Newsreel.bOn)
	{
		EndNewsreel();
		return;
	}
	if (Announcements.Num() > 0)
	{
		DismissAnnouncement();
		return;
	}
	SkipWatching();
}

// The number keys open the screens of the top bar, and the same key puts one away again.
void ACampaignPlayerController::OpenScreen1() { if (bArsenalOpen) { CloseArsenal(); } else { OpenArsenal(); } }
void ACampaignPlayerController::OpenScreen2() { if (bGeneralsOpen) { CloseGenerals(); } else { OpenGenerals(); } }
void ACampaignPlayerController::OpenScreen3() { if (bResearchOpen) { CloseResearch(); } else { OpenResearch(); } }
void ACampaignPlayerController::OpenScreen4() { if (bDiplomacyOpen) { CloseDiplomacy(); } else { OpenDiplomacy(); } }
void ACampaignPlayerController::OpenScreen5() { if (bLedgerOpen) { CloseLedger(); } else { OpenLedger(); } }

int32 ACampaignPlayerController::ArmiesYetToMove() const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || bWatching || Opening.bVisible || !Session->GetView().bHumanUp)
	{
		return 0;
	}
	const FCampaignView& View = Session->GetView();
	TSet<int32> Places;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Nation == View.Human && Unit.bCanOrder && Unit.Moves > 0 && Unit.Kind != TEXT("fort"))
		{
			Places.Add(Unit.Region);
		}
	}
	return Places.Num();
}

void ACampaignPlayerController::OpenForces()
{
	if (bPageOpen && Page.Id == TEXT("forces"))
	{
		ClosePage();
		return;
	}
	OpenPage(TEXT("forces"));
}

namespace
{
	/** The voices this computer has, by name: asked of it once. */
	const TSet<FString>& InstalledVoices()
	{
		static TSet<FString> Names;
		static bool bAsked = false;
		if (!bAsked)
		{
			bAsked = true;
#if PLATFORM_MAC
			FString Listed, Errors;
			int32 Code = 0;
			if (FPlatformProcess::ExecProcess(TEXT("/usr/bin/say"), TEXT("-v ?"), &Code, &Listed, &Errors))
			{
				TArray<FString> Lines;
				Listed.ParseIntoArrayLines(Lines);
				for (const FString& Line : Lines)
				{
					// A line reads "Anna                de_DE    # Hallo! Ich heisse Anna.": the name is what stands before the language.
					int32 Hash = INDEX_NONE;
					FString Left = Line.FindChar(TEXT('#'), Hash) ? Line.Left(Hash) : Line;
					Left.TrimEndInline();
					int32 Space = INDEX_NONE;
					if (Left.FindLastChar(TEXT(' '), Space))
					{
						Names.Add(Left.Left(Space).TrimEnd());
					}
				}
			}
#endif
		}
		return Names;
	}

	/** True if the computer has this voice. (It lists some by a longer name: "Daniel (English (UK))".) */
	bool HasVoice(const FString& Name)
	{
		const FString Longer = Name + TEXT(" (");
		for (const FString& Had : InstalledVoices())
		{
			if (Had == Name || Had.StartsWith(Longer))
			{
				return true;
			}
		}
		return false;
	}

	/** A card's heading as it is said aloud: not shouted in capitals, and its dots read as pauses. */
	FString HeadingAloud(const FString& Heading)
	{
		FString Said = Heading;
		Said.ReplaceInline(TEXT("\u00B7"), TEXT(","));
		Said.ReplaceInline(TEXT("  ,"), TEXT(","));
		Said.ReplaceInline(TEXT(" ,"), TEXT(","));
		if (Said.Equals(Said.ToUpper(), ESearchCase::CaseSensitive) && Said.Len() > 1)
		{
			Said = Said.Left(1) + Said.Mid(1).ToLower();
		}
		return Said;
	}

	/** The voice an army of this country answers in: one that speaks its language, where the computer has one. */
	FString VoiceOfNation(const FString& Tag)
	{
		static const TMap<FString, FString> ByNation = {
			{ TEXT("GER"), TEXT("Anna") }, { TEXT("FRA"), TEXT("Jacques") }, { TEXT("VIC"), TEXT("Jacques") }, { TEXT("ITA"), TEXT("Alice") },
			{ TEXT("SOV"), TEXT("Milena") }, { TEXT("POL"), TEXT("Zosia") }, { TEXT("NED"), TEXT("Xander") }, { TEXT("BEL"), TEXT("Ellen") },
			{ TEXT("SWE"), TEXT("Alva") }, { TEXT("NOR"), TEXT("Nora") }, { TEXT("DEN"), TEXT("Sara") }, { TEXT("FIN"), TEXT("Satu") },
			{ TEXT("HUN"), TEXT("Tünde") }, { TEXT("ROM"), TEXT("Ioana") }, { TEXT("YUG"), TEXT("Lana") }, { TEXT("BUL"), TEXT("Daria") },
			{ TEXT("GRE"), TEXT("Melina") }, { TEXT("TUR"), TEXT("Yelda") }, { TEXT("ESP"), TEXT("Mónica") }, { TEXT("POR"), TEXT("Joana") },
			{ TEXT("USA"), TEXT("Samantha") },
		};
		const FString* Found = ByNation.Find(Tag);
		return Found ? *Found : FString(TEXT("Daniel"));
	}

	/** A year as it is said aloud: "nineteen forty". */
	FString YearAloud(const FString& Year)
	{
		static const TCHAR* Ones[] = { TEXT(""), TEXT("-one"), TEXT("-two"), TEXT("-three"), TEXT("-four"), TEXT("-five"), TEXT("-six"), TEXT("-seven"), TEXT("-eight"), TEXT("-nine") };
		const int32 Number = FCString::Atoi(*Year);
		if (Number >= 1930 && Number <= 1949)
		{
			return FString::Printf(TEXT("Nineteen %s%s"), Number < 1940 ? TEXT("thirty") : TEXT("forty"), Ones[Number % 10]);
		}
		return Year;
	}
}

void ACampaignPlayerController::PlayBriefing()
{
	FCampaignSession* Session = GetSession();
	if (!Session)
	{
		return;
	}
	FString Nation, Date;
	const TArray<FNewsCard> Stops = Session->Briefing(Nation, Date);
	if (Stops.Num() > 0)
	{
		PlayNewsreel(Date, Stops, 1);
	}
}

void ACampaignPlayerController::PlayYearCard(const FYearCard& Year)
{
	if (!Year.bValid)
	{
		return;
	}
	PlayNewsreel(Year.Year, TArray<FNewsCard>(), 2);
	Newsreel.Year = Year;
	Speak(YearAloud(Year.Year) + TEXT(". ") + HeadingAloud(Year.Title), TEXT("Daniel"), 160, 2);
}

void ACampaignPlayerController::PlayNewsreel(const FString& Date, const TArray<FNewsCard>& Cards, int32 Kind)
{
	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || !Map.IsValid() || (Cards.Num() == 0 && Kind != 2))
	{
		return;
	}
	PutDownArmy();
	PutDownFleet();
	SelectRegion(INDEX_NONE);
	Newsreel = FNewsreelShow();
	Newsreel.bOn = true;
	Newsreel.Date = Date;
	Newsreel.Cards = Cards;
	Newsreel.Index = -1;
	Newsreel.Kind = Kind;
	Newsreel.Until = FPlatformTime::Seconds() + (Kind == 2 ? 7.0 : Kind == 1 ? 3.0 : 3.4); // the title, while the band plays
	// The great moments are told when the reel is over.
	HeldAnnouncements = Announcements;
	Announcements.Reset();
	ReelReturnFocus = Eye->GetFocus();
	ReelReturnZoom = Eye->GetZoom();
	ReelReturnYaw = Eye->GetYaw();
	if (Kind == 0)
	{
		// The month's newsreel is film: black and white, and its title is shown over the whole map.
		Eye->SetNewsreel(true);
		const FCampaignMapData& Data = Map->GetData();
		Eye->GoTo(Map->MapToWorld(Data.Width * 0.5, Data.Height * 0.48), 0.92f);
		CampaignSounds::Play(this, TEXT("newsreel"), 0.6f);
	}
	else if (Kind == 1)
	{
		// The opening flight is in colour, and begins by rising from where the view stands.
		Eye->GoTo(ReelReturnFocus, FMath::Min(ReelReturnZoom + 0.16f, 0.9f));
	}
	else
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.5f);
	}
}

void ACampaignPlayerController::NextNewsCard()
{
	if (!Newsreel.bOn)
	{
		return;
	}
	++Newsreel.Index;
	if (!Newsreel.Cards.IsValidIndex(Newsreel.Index))
	{
		EndNewsreel();
		return;
	}
	const FNewsCard& Card = Newsreel.Cards[Newsreel.Index];
	// Long enough to be read out, and never rushed. (And a card stays until the voice has finished it: PlayerTick.)
	Newsreel.Until = FPlatformTime::Seconds() + (Newsreel.Kind == 1 ? FMath::Clamp(2.6 + Card.Text.Len() / 15.0, 5.5, 13.0) : FMath::Clamp(2.2 + Card.Text.Len() / 13.5, 5.5, 16.0));
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		const FCampaignMapData& Data = Map->GetData();
		if (Newsreel.Kind == 1)
		{
			Eye->SetHeading(Card.Turn);
		}
		if (Data.Regions.IsValidIndex(Card.Region))
		{
			const FVector2D At = Data.Regions[Card.Region].Center;
			Eye->GoTo(Map->MapToWorld(At.X, At.Y), Newsreel.Kind == 1 ? Card.Zoom : 0.36f);
		}
		else
		{
			Eye->GoTo(Map->MapToWorld(Data.Width * 0.5, Data.Height * 0.48), 0.8f);
		}
	}
	if (Newsreel.Kind == 0)
	{
		CampaignSounds::Play(this, TEXT("projector"), 0.35f);
		Narrate(Card.Text);
	}
	else
	{
		CampaignSounds::Play(this, TEXT("tick"), 0.5f, 0.8f);
		// The opening flight is told by a radio voice, a little slower than the newsreels.
		Speak(Card.Title.IsEmpty() ? Card.Text : HeadingAloud(Card.Title) + TEXT(". ") + Card.Text, TEXT("Daniel"), 172, 2);
	}
}

void ACampaignPlayerController::EndNewsreel()
{
	if (!Newsreel.bOn)
	{
		return;
	}
	Newsreel.bOn = false;
	HushNarrator();
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->SetNewsreel(false);
		if (Newsreel.Kind == 1)
		{
			Eye->SetHeading(ReelReturnYaw);
		}
		else
		{
			Eye->RotateBy(ReelReturnYaw - Eye->GetYaw()); // back the way the player was facing
		}
		Eye->GoTo(ReelReturnFocus, ReelReturnZoom);
	}
	Announcements = HeldAnnouncements;
	HeldAnnouncements.Reset();
	if (Newsreel.Kind == 2 && WaitingReel.Num() > 0)
	{
		// The year's title card has gone by: now the month's newsreel that waited behind it.
		const TArray<FNewsCard> Reel = MoveTemp(WaitingReel);
		WaitingReel.Reset();
		PlayNewsreel(WaitingReelDate, Reel);
		return;
	}
	if (Announcements.Num() > 0)
	{
		CampaignSounds::Play(this, Announcements[0].bGood ? TEXT("victory") : TEXT("defeat"), 0.55f);
	}
}

void ACampaignPlayerController::ToggleNewsreels()
{
	bNewsreels = !bNewsreels;
	if (!bNewsreels)
	{
		EndNewsreel();
	}
	Notify(bNewsreels ? TEXT("Newsreels on: the month's great stories are shown when it ends.") : TEXT("Newsreels off. Press K to bring them back."), 4.0);
	SaveOptions();
}

void ACampaignPlayerController::Narrate(const FString& Words)
{
	// The stories are read by the computer's own voice, and a new story cuts the last one short.
	Speak(Words, TEXT("Daniel"), 182, 2);
}

void ACampaignPlayerController::Speak(const FString& Words, const FString& Voice, int32 Rate, int32 Cut)
{
	if (Cut >= 2)
	{
		HushNarrator();
	}
	// Not while pictures are being taken, nor with the voice or the sound off. ("-TheaterVoiceTo=<folder>" has every
	// line written there as a sound file instead of spoken, pictures or no: a way to check the voice in silence.)
	FString ToFolder;
	const bool bToFiles = FParse::Value(FCommandLine::Get(), TEXT("TheaterVoiceTo="), ToFolder);
	if (!bNarrator || Words.TrimStartAndEnd().IsEmpty() || FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet")) || (!bToFiles && (bScripted || !CampaignSounds::IsOn())))
	{
		return;
	}
	FVoiceLine Line;
	Line.Words = Words;
	Line.Voice = Voice;
	Line.Rate = Rate;
	if (Cut == 1)
	{
		StopSpeaking();
		VoiceQueue.Insert(Line, 0);
	}
	else
	{
		VoiceQueue.Add(Line);
	}
	// (nobody waits for a long queue of old remarks)
	while (VoiceQueue.Num() > 4)
	{
		VoiceQueue.RemoveAt(Cut == 1 ? 1 : 0);
	}
	PumpVoice();
}

void ACampaignPlayerController::PumpVoice()
{
	if (VoiceQueue.Num() == 0 || IsSpeaking())
	{
		return;
	}
	const FVoiceLine Line = VoiceQueue[0];
	VoiceQueue.RemoveAt(0);
#if PLATFORM_MAC
	// The words go by way of a file, so that nothing in them can be taken for an instruction to the program.
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Voice"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FString File = Folder / TEXT("line.txt");
	FString Said = Line.Words;
	Said.ReplaceInline(TEXT("\n"), TEXT(" "));
	Said.ReplaceInline(TEXT("[["), TEXT(" ")); // (the speaking program reads double brackets as commands to itself)
	Said.ReplaceInline(TEXT("]]"), TEXT(" "));
	if (!FFileHelper::SaveStringToFile(Said, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return;
	}
	// A voice the computer has not got is replaced by the narrator's, and failing that by whatever it speaks with.
	const FString Voice = HasVoice(Line.Voice) ? Line.Voice : HasVoice(TEXT("Daniel")) ? FString(TEXT("Daniel")) : FString();
	FString Arguments = FString::Printf(TEXT("%s-r %d -f \"%s\""), Voice.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("-v \"%s\" "), *Voice), Line.Rate, *File);
	FString ToFolder;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterVoiceTo="), ToFolder))
	{
		IFileManager::Get().MakeDirectory(*ToFolder, true);
		Arguments += FString::Printf(TEXT(" -o \"%s\""), *(ToFolder / FString::Printf(TEXT("%02d-%s.aiff"), ++VoiceCount, Voice.IsEmpty() ? TEXT("voice") : *Voice)));
		UE_LOG(LogTheater, Display, TEXT("Voice %d (%s, %d words a minute): %s"), VoiceCount, *Voice, Line.Rate, *Said.Left(120));
	}
	Narrator = FPlatformProcess::CreateProc(TEXT("/usr/bin/say"), *Arguments, true, true, true, nullptr, 0, nullptr, nullptr);
#endif
}

bool ACampaignPlayerController::IsSpeaking()
{
	return Narrator.IsValid() && FPlatformProcess::IsProcRunning(Narrator);
}

void ACampaignPlayerController::StopSpeaking()
{
	if (Narrator.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(Narrator))
		{
			FPlatformProcess::TerminateProc(Narrator);
		}
		FPlatformProcess::CloseProc(Narrator);
		Narrator.Reset();
	}
}

void ACampaignPlayerController::HushNarrator()
{
	VoiceQueue.Reset();
	StopSpeaking();
}

void ACampaignPlayerController::SpeakAdviser()
{
	// The staff officer says the next of his first steps aloud, as it is written on his card.
	static const TCHAR* Steps[5] = {
		TEXT("Pick up an army. Click one of your flags."),
		TEXT("Now send it. Right-click where it should go."),
		TEXT("Look at a province. Click any ground to see what it holds."),
		TEXT("Open an airfield or a fleet. Click one to see your air groups and your ships."),
		TEXT("When your orders are given, end the month."),
	};
	if (!Adviser.bOn || Newsreel.bOn)
	{
		return;
	}
	for (int32 Step = 0; Step < 5; ++Step)
	{
		if (!Adviser.bDone[Step])
		{
			Speak(Steps[Step], TEXT("Daniel"), 176, 0);
			return;
		}
	}
}

void ACampaignPlayerController::ToggleDayNight()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACampaignEnvironment> It(World); It; ++It)
		{
			const int32 Now = It->NextTimeKeeping();
			Notify(Now == 0 ? TEXT("Day and night go by in the room again.") : Now == 1 ? TEXT("Held at day. Press T again for night.") : TEXT("Held at night, by lamplight. Press T again to let the day run."), 4.0);
		}
	}
	SaveOptions();
}

bool ACampaignPlayerController::IsWealthShown() const
{
	return Map.IsValid() && Map->IsWealthShown();
}

void ACampaignPlayerController::ToggleWealth()
{
	FCampaignSession* Session = GetSession();
	if (!Map.IsValid() || !Session)
	{
		return;
	}
	// One key steps through the views of the map: as it is, the countries in flat colour, by wealth, by friend and foe.
	const int32 Now = Map->IsWealthShown() ? 2 : Map->AreSidesShown() ? 3 : Map->AreColoursFlat() ? 1 : 0;
	const int32 Next = (Now + 1) % 4;
	Map->SetFlatColours(Next == 1);
	Map->ShowWealth(Next == 2);
	Map->ShowSides(Next == 3, Session->GetView().RegionStanding);
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	Notify(Next == 1 ? TEXT("Flat colours: every country painted whole in its own colour. Press Y for the wealth view.")
		: Next == 2 ? TEXT("The wealth view: the deeper the gold, the more a province pays. Press Y for friend and foe.")
		: Next == 3 ? TEXT("Friend and foe: your own ground green, your allies' blue, your enemies' red. Press Y to put it away.")
		: TEXT("The map as it is: each country's colour a stain over its own ground."), 5.0);
}

void ACampaignPlayerController::AdviserSaw(int32 Step)
{
	if (!Adviser.bOn || Step < 0 || Step >= 5 || Adviser.bDone[Step])
	{
		return;
	}
	Adviser.bDone[Step] = true;
	++Adviser.Revision;
	CampaignSounds::Play(this, TEXT("stamp"), 0.3f);
	bool bAll = true;
	for (bool bDone : Adviser.bDone)
	{
		bAll &= bDone;
	}
	if (bAll)
	{
		Adviser.bOn = false; // he has no more to say
		Notify(TEXT("That is the whole of it. H opens the guide whenever you want it."), 6.0);
		Speak(TEXT("That is the whole of it."), TEXT("Daniel"), 176, 0);
	}
	else
	{
		SpeakAdviser();
	}
}

void ACampaignPlayerController::AskAlliesToJoin()
{
	FCampaignSession* Session = GetSession();
	if (!Session || ArmyRegion == INDEX_NONE)
	{
		return;
	}
	FString Note;
	const int32 Here = ArmyRegion;
	const bool bJoined = Session->JoinArmy(Here, Note);
	CampaignSounds::Play(this, bJoined ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (bJoined)
	{
		// They are part of the army now: its cards are dealt again with theirs among them.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		PickUpArmyIn(Here);
		RefreshRegionPanel(SelectedRegion);
	}
	Notify(Note, 7.0);
}

void ACampaignPlayerController::SendBackCorps(int32 CorpsId)
{
	FCampaignSession* Session = GetSession();
	if (!Session)
	{
		return;
	}
	FString Note;
	const int32 Here = ArmyRegion;
	const bool bSent = Session->SendBack(CorpsId, Note);
	CampaignSounds::Play(this, bSent ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (bSent)
	{
		PickedCorps.Remove(CorpsId);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		if (Here != INDEX_NONE && !PickUpArmyIn(Here))
		{
			PutDownArmy();
		}
		RefreshRegionPanel(SelectedRegion);
	}
	Notify(Note, 6.0);
}

FString ACampaignPlayerController::GetPlayerTag() const
{
	const FCampaignSession* Session = GetSession();
	return Session ? Session->GetView().Human : FString();
}

void ACampaignPlayerController::SayWhy(const FString& Why)
{
	CampaignSounds::Play(this, TEXT("click"), 0.4f);
	Notify(Why.IsEmpty() ? FString(TEXT("That cannot be ordered now.")) : Why, 5.0);
}

FText ACampaignPlayerController::GetWeatherText() const
{
	const FCampaignSession* Session = GetSession();
	return Session && !Opening.bVisible ? FText::FromString(Session->GetView().WeatherText) : FText::GetEmpty();
}

void ACampaignPlayerController::OpenGoals()
{
	if (bPageOpen && Page.Id == TEXT("goals"))
	{
		ClosePage();
		return;
	}
	OpenPage(TEXT("goals"));
	if (!bPageOpen)
	{
		Notify(TEXT("This country has no set objectives: the war is yours to make of what you will."), 5.0);
	}
}

void ACampaignPlayerController::OpenReports()
{
	if (bPageOpen && Page.Id == TEXT("reports"))
	{
		ClosePage();
		return;
	}
	OpenPage(TEXT("reports"));
	if (!bPageOpen)
	{
		Notify(TEXT("There is no report yet: end a month first."), 4.0);
	}
}

void ACampaignPlayerController::GoToCapital()
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || bWatching)
	{
		return;
	}
	const FString& Mine = Session->GetView().Human;
	for (const FCampaignRegion& Region : Map->GetData().Regions)
	{
		if (Region.bCapital && Region.Owner == Mine)
		{
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
			{
				Eye->GoTo(Map->MapToWorld(Region.Center.X, Region.Center.Y), FMath::Min(Eye->GetZoom(), 0.45f), false);
			}
			SelectRegion(Region.Id);
			CampaignSounds::Play(this, TEXT("click"), 0.5f);
			return;
		}
	}
}

void ACampaignPlayerController::GetWatchOrder(TArray<FString>& OutTags, int32& OutNow) const
{
	OutTags.Reset();
	OutNow = INDEX_NONE;
	if (!bWatching)
	{
		return;
	}
	// The nations whose moves are being shown, in the order they made them; the one on the table now is marked.
	for (int32 Index = 0; Index < Month.Steps.Num(); ++Index)
	{
		const FString& Tag = Month.Steps[Index].Nation;
		if (OutTags.Num() == 0 || OutTags.Last() != Tag)
		{
			OutTags.Add(Tag);
		}
		if (Index == WatchIndex - 1)
		{
			OutNow = OutTags.Num() - 1;
		}
	}
}

void ACampaignPlayerController::ToggleWatchAll()
{
	bWatchAll = !bWatchAll;
	Notify(bWatchAll ? TEXT("At the end of a month you will watch every nation's moves and battles.") : TEXT("Quick months: you will be shown only what touches you. The rest is in the news."), 5.0);
	SaveOptions();
}

void ACampaignPlayerController::ShowCorpsModel(int32 CorpsId)
{
	if (CorpsId == ShowCorps || !Map.IsValid())
	{
		return;
	}
	const FTrayCard* Card = Tray.Cards.FindByPredicate([CorpsId](const FTrayCard& Each) { return Each.CorpsId == CorpsId; });
	if (!Card)
	{
		ShowCorps = INDEX_NONE;
		ShowTitle = FText::GetEmpty();
		Map->SetShowpiece(FString());
		return;
	}
	ShowCorps = CorpsId;
	const FString Model = ACampaignMapActor::ModelFor(Tray.Tag, Card->KindId);
	Map->SetShowpiece(Model);
	FString Title = Model.StartsWith(TEXT("Soldier:")) ? FString(TEXT("Infantry")) : Model.StartsWith(TEXT("Rider:")) ? FString(TEXT("Cavalry")) : Model.StartsWith(TEXT("Pioneer:")) ? FString(TEXT("Engineers")) : FMiniatureMesh::VehicleTitle(Model);
	if (Title.IsEmpty())
	{
		Title = Card->Kind.ToString();
	}
	ShowTitle = FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s"), *Card->Name.ToString(), *Title));
}

void ACampaignPlayerController::DismissAnnouncement()
{
	if (Announcements.Num() > 0)
	{
		// The view goes to the country's capital, so the player sees where it happened.
		const int32 Capital = Announcements[0].Region;
		if (Map.IsValid() && Map->GetData().Regions.IsValidIndex(Capital) && !bWatching)
		{
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
			{
				const FVector2D At = Map->GetData().Regions[Capital].Center;
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Clamp(Eye->GetZoom(), 0.4f, 0.6f), false);
			}
		}
		Announcements.RemoveAt(0);
		if (Announcements.Num() > 0)
		{
			CampaignSounds::Play(this, Announcements[0].bGood ? TEXT("victory") : TEXT("defeat"), 0.55f);
		}
	}
}

void ACampaignPlayerController::NextArmy()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible)
	{
		return;
	}
	// The provinces where the player has corps that can still move, in order; the one after the army in hand.
	const FCampaignView& View = Session->GetView();
	TArray<int32> Places;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Nation == View.Human && Unit.bCanOrder && Unit.Moves > 0)
		{
			Places.AddUnique(Unit.Region);
		}
	}
	if (Places.Num() == 0)
	{
		Notify(TEXT("Every army has used its month. Press Enter to end it."), 4.0);
		return;
	}
	Places.Sort();
	int32 Next = Places[0];
	for (int32 Place : Places)
	{
		if (Place > ArmyRegion)
		{
			Next = Place;
			break;
		}
	}
	SelectRegion(Next);
	PickUpArmyIn(Next);
	CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		const FVector2D At = Map->GetData().Regions[Next].Center;
		Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
	}
}

void ACampaignPlayerController::Escape()
{
	// Esc backs out one step: the watching, the face-off, the army in hand, then the territory.
	if (Newsreel.bOn)
	{
		EndNewsreel();
	}
	else if (Announcements.Num() > 0)
	{
		DismissAnnouncement();
	}
	else if (Document.bValid)
	{
		CloseDocument();
	}
	else if (Opening.bVisible)
	{
		CloseMenu();
	}
	else if (bWatching)
	{
		SkipWatching();
	}
	else if (bArsenalOpen)
	{
		CloseArsenal();
	}
	else if (bLedgerOpen)
	{
		CloseLedger();
	}
	else if (bDiplomacyOpen)
	{
		CloseDiplomacy();
	}
	else if (bResearchOpen)
	{
		CloseResearch();
	}
	else if (bGeneralsOpen)
	{
		CloseGenerals();
	}
	else if (bPageOpen)
	{
		ClosePage();
	}
	else if (WarPrompt.bVisible)
	{
		CloseWarPrompt();
	}
	else if (FaceOff.bVisible)
	{
		CloseFaceOff();
	}
	else if (ArmyRegion != INDEX_NONE)
	{
		PutDownArmy();
	}
	else if (FleetInHand != INDEX_NONE)
	{
		PutDownFleet();
	}
	else if (LookRegion != INDEX_NONE)
	{
		StopLooking();
	}
	else
	{
		SelectRegion(INDEX_NONE);
	}
}

bool ACampaignPlayerController::PointerOnTable(FVector& OutWorld) const
{
	FVector Origin, Direction;
	if (!Map.IsValid() || !DeprojectMousePositionToWorld(Origin, Direction) || Direction.Z > -0.01)
	{
		return false;
	}
	// Meet the water level first, then step down onto the ground that is actually there.
	double PlaneZ = 0.0;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		const double T = (PlaneZ - Origin.Z) / Direction.Z;
		OutWorld = Origin + Direction * T;
		const FVector2D P = Map->WorldToMap(OutWorld);
		PlaneZ = FMath::Max(0.0, static_cast<double>(Map->GetTerrain().HeightAt(P.X, P.Y))) * Map->UnitScale * Map->VerticalScale;
	}
	return true;
}

void ACampaignPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	PumpVoice();
	if (Adviser.bOn && !bAdviserSpoke && !Newsreel.bOn && BriefingAt <= 0.0 && !Opening.bVisible && Announcements.Num() == 0 && !bWatching)
	{
		bAdviserSpoke = true;
		SpeakAdviser();
	}

	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || !Map.IsValid())
	{
		return;
	}

	// The pointer crossing onto a button is heard: the lightest tick, once for each button.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const FWidgetPath Under = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows(), true);
		const void* Button = nullptr;
		static const FName ButtonType(TEXT("SButton")), OverlayType(TEXT("SCampaignOverlay"));
		// (the game's own screen lets the mouse through to the map, so it is in the way only when one of its panels is)
		bPointerOverPanel = false;
		for (int32 Index = Under.Widgets.Num() - 1; Index >= 0; --Index)
		{
			const TSharedRef<SWidget>& Widget = Under.Widgets[Index].Widget;
			if (!Button && Widget->GetType() == ButtonType)
			{
				Button = Widget->IsEnabled() ? &Widget.Get() : nullptr;
			}
			if (Widget->GetType() == OverlayType)
			{
				bPointerOverPanel = true;
				break;
			}
		}
		if (Button != HoveredButton)
		{
			HoveredButton = Button;
			if (Button)
			{
				CampaignSounds::Play(this, TEXT("tick"), 0.6f);
			}
		}
		// And every press of a button is felt: a lower, firmer tick as the mouse goes down on one.
		const bool bDown = Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		if (bDown && !bButtonDown && Button)
		{
			CampaignSounds::Play(this, TEXT("tick"), 1.0f, 0.7f);
		}
		bButtonDown = bDown;
	}

	// The standing choices are read once, when everything in the room is there to be told of them.
	if (!bOptionsRead)
	{
		bOptionsRead = true;
		LoadOptions();
	}

	// The music follows the war: the march while there is fighting to watch, the theme for a country at war,
	// the quiet piece for one at peace.
	if (ACampaignGameMode* Mode = GetMode())
	{
		const FCampaignSession* Now = GetSession();
		const bool bFighting = FPlatformTime::Seconds() < BattleMusicUntil || FaceOff.bVisible;
		Mode->SetMusicMood(bFighting ? TEXT("battle") : (Now && !Opening.bVisible && !Now->GetView().bAtWar && !Now->GetView().Human.IsEmpty()) ? TEXT("quiet") : TEXT("theme"));
		Mode->DuckMusic(Newsreel.bOn);
	}

	// The first time evening comes on in the room, it is said what is happening and how to hold the day.
	if (!bToldOfEvening && !bBusyWithScreens() && GetSession())
	{
		for (TActorIterator<ACampaignEnvironment> It(GetWorld()); It; ++It)
		{
			if (It->GetNight() > 0.25f)
			{
				bToldOfEvening = true;
				Notify(TEXT("Evening in the war room: the lamps are lit, and day will come round again. Press T to hold it at day."), 9.0);
			}
		}
	}

	// The newsreel: the view drifts slowly round the place while its story is told, then the next comes on.
	if (Newsreel.bOn)
	{
		if (Newsreel.Kind != 2)
		{
			Eye->RotateBy(DeltaTime * (Newsreel.Kind == 1 ? 1.2f : 1.8f));
		}
		// (the next card waits for the voice to finish this one, though not for ever)
		if (FPlatformTime::Seconds() >= Newsreel.Until && !(IsSpeaking() && FPlatformTime::Seconds() < Newsreel.Until + 8.0))
		{
			NextNewsCard();
		}
	}
	else if (BriefingAt > 0.0 && FPlatformTime::Seconds() >= BriefingAt)
	{
		BriefingAt = 0.0;
		if (!bBusyWithScreens() && GetSession())
		{
			PlayBriefing();
		}
	}

	// History being played for the opening screen: the screen has said so for a few frames; now do it.
	if (Opening.bWorking && ++WorkingFrames >= 3)
	{
		Opening.bWorking = false;
		LoadGrandNations();
		++Opening.Revision;
	}

	// Keys: W A S D or the arrows slide the view, Q and E turn it, + and - zoom.
	FVector2D Slide = FVector2D::ZeroVector;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Slide.Y += 1.0;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Slide.Y -= 1.0;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Slide.X += 1.0;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Slide.X -= 1.0;
	Eye->Pan(Slide, DeltaTime);
	if (IsInputKeyDown(EKeys::Q)) Eye->RotateBy(-70.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::E)) Eye->RotateBy(70.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::Equals) || IsInputKeyDown(EKeys::Add)) Eye->ZoomBy(-12.0f * DeltaTime);
	if (IsInputKeyDown(EKeys::Hyphen) || IsInputKeyDown(EKeys::Subtract)) Eye->ZoomBy(12.0f * DeltaTime);

	float MouseX = 0.0f, MouseY = 0.0f;
	const bool bHaveMouse = GetMousePosition(MouseX, MouseY);
	const FVector2D Mouse(MouseX, MouseY);
	const FVector2D MouseDelta = (bHaveMouse && bHaveLastMouse) ? Mouse - LastMouse : FVector2D::ZeroVector;

	int32 ViewWidth = 0, ViewHeight = 0;
	GetViewportSize(ViewWidth, ViewHeight);

	// A bigger window is not allowed to cost more: the picture is drawn at about the same number of
	// points whatever the window's size, and Unreal's upscaler fills in the rest.
	if (ViewWidth != SizedForWidth || ViewHeight != SizedForHeight)
	{
		SizedForWidth = ViewWidth;
		SizedForHeight = ViewHeight;
		if (ViewWidth > 0 && ViewHeight > 0)
		{
			const double Budget = 1250.0 * 780.0;
			const int32 Percent = FMath::Clamp(FMath::RoundToInt32(100.0 * FMath::Sqrt(Budget / (static_cast<double>(ViewWidth) * ViewHeight))), 50, 100);
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
			{
				Variable->Set(Percent, ECVF_SetByCode);
			}
		}
	}

	// What the pointer is over: a piece if it is on one (its counters, pole or colours), and the ground behind it.
	FVector PointerWorld;
	const bool bOnTable = bHaveMouse && PointerOnTable(PointerWorld);
	const int32 PointedRegion = bOnTable ? Map->RegionAtWorld(PointerWorld) : INDEX_NONE;
	PointerMap = bOnTable ? Map->WorldToMap(PointerWorld) : FVector2D::ZeroVector;
	bPointerOnWater = bOnTable && PointedRegion == INDEX_NONE;
	PointedField = bOnTable ? Map->AirfieldNear(PointerMap) : INDEX_NONE;
	AArmyPiece* PointedPiece = nullptr;
	{
		FVector RayOrigin, RayDirection;
		if (bHaveMouse && !bScripted && DeprojectMousePositionToWorld(RayOrigin, RayDirection))
		{
			PointedPiece = Map->PieceAtRay(RayOrigin, RayDirection);
		}
	}

	// The other nations' month plays itself out, a nation at a time.
	if (bWatching && FPlatformTime::Seconds() >= WatchNext)
	{
		WatchNextStep();
	}
	const bool bBusy = bWatching || Newsreel.bOn || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Document.bValid || bLedgerOpen || bArsenalOpen || bDiplomacyOpen || bResearchOpen || bGeneralsOpen || bPageOpen || Announcements.Num() > 0;

	// The pointer resting at the very edge of the window slides the table that way, as in any game of the kind.
	// (Only after a moment's rest there, so a pointer on its way out of the window does not tug the map along.)
	{
		FVector2D Push = FVector2D::ZeroVector;
		if (bEdgeScroll && bHaveMouse && !bScripted && !bBusy && !bLeftHeld && !bRightHeld && ViewWidth > 0 && ViewHeight > 0 && FSlateApplication::Get().IsActive())
		{
			const double Margin = 4.0;
			if (Mouse.X <= Margin) Push.X -= 1.0;
			if (Mouse.X >= ViewWidth - 1 - Margin) Push.X += 1.0;
			if (Mouse.Y <= Margin) Push.Y += 1.0;
			if (Mouse.Y >= ViewHeight - 1 - Margin) Push.Y -= 1.0;
		}
		EdgeRest = Push.IsZero() ? 0.0f : EdgeRest + DeltaTime;
		if (EdgeRest > 0.3f)
		{
			Eye->Pan(Push * 0.85, DeltaTime);
		}
	}

	// Right button: a press and drag turns and tilts the view; a press and release gives the army in hand its order.
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && bHaveMouse)
	{
		bRightHeld = true;
		bTurning = false;
		RightPressPosition = Mouse;
	}
	if (bRightHeld && IsInputKeyDown(EKeys::RightMouseButton) && bHaveMouse)
	{
		if (!bTurning && FVector2D::Distance(Mouse, RightPressPosition) > 6.0)
		{
			bTurning = true;
		}
		if (bTurning)
		{
			Eye->RotateBy(static_cast<float>(MouseDelta.X) * 0.3f);
			Eye->TiltBy(static_cast<float>(-MouseDelta.Y) * 0.2f);
		}
	}
	if (bRightHeld && !IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (!bTurning && !bBusy && ArmyRegion != INDEX_NONE && PointedRegion != INDEX_NONE)
		{
			OrderTo(PointedRegion);
		}
		else if (!bTurning && !bBusy && FleetInHand != INDEX_NONE && bOnTable)
		{
			// A fleet in hand is sent with a right-click, as an army is: to the sea under the pointer, or home
			// if it is its own harbour that is pointed at.
			const FCampaignSession* Now = GetSession();
			const FViewFleet* Held = Now ? Now->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
			if (bPointerOnWater)
			{
				OrderFleetTo(SeaNear(PointerMap));
			}
			else if (Held && PointedRegion == Held->Base)
			{
				FleetToHarbour();
			}
			else if (Held && FleetHand.bGrand && !FleetHand.bShelled && FleetHand.Coasts.Contains(PointedRegion))
			{
				// An enemy coast its guns can reach: the question is put first, with what the shelling would do.
				OpenPage(FString::Printf(TEXT("shell:%d:%d"), FleetInHand, PointedRegion));
			}
			else if (Held && Held->Bases.Contains(PointedRegion))
			{
				FleetToNewBase(PointedRegion);
			}
			else
			{
				Notify(TEXT("A fleet keeps to the sea: right-click one of the ringed seas, a friendly port tinted blue to make it the fleet's new home, or its own harbour to bring it in."), 6.0);
			}
		}
		bRightHeld = false;
		bTurning = false;
	}

	// Left button. On one of the player's flags: a press picks the army up and shows how far it can go; dragging
	// carries the flag, and letting go over ground it can reach sends it there. Anywhere else: a click picks a
	// territory (or, with an army in hand, sends the army there if it can reach it), and a drag pulls the map along.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHaveMouse)
	{
		bLeftHeld = true;
		bDragging = false;
		PressPosition = Mouse;
		HeldPiece.Reset();
		bHeldWasInHand = false;
		if (!bBusy)
		{
			const bool bAlreadyInHand = PointedPiece && ArmyRegion != INDEX_NONE && RingedPiece.Get() == PointedPiece;
			bHeldWasInHand = Hold(PointedPiece) && bAlreadyInHand;
		}
	}
	if (bLeftHeld && IsInputKeyDown(EKeys::LeftMouseButton) && bHaveMouse)
	{
		if (!bDragging && FVector2D::Distance(Mouse, PressPosition) > 6.0)
		{
			bDragging = true;
		}
		if (bDragging && HeldPiece.IsValid())
		{
			// The flag is in the hand: it follows the pointer over the table, and the table slides along when the
			// hand comes to the edge of the view, so an army can be carried farther than the screen shows.
			if (bOnTable)
			{
				HeldPiece->Carry(PointerWorld);
			}
			if (ViewWidth > 0 && ViewHeight > 0)
			{
				const double Margin = 0.045 * ViewHeight;
				FVector2D Push = FVector2D::ZeroVector;
				if (Mouse.X < Margin) Push.X -= 1.0;
				if (Mouse.X > ViewWidth - Margin) Push.X += 1.0;
				if (Mouse.Y < Margin) Push.Y += 1.0;
				if (Mouse.Y > ViewHeight - Margin) Push.Y -= 1.0;
				Eye->Pan(Push * 0.8, DeltaTime);
			}
		}
		else if (bDragging && ViewWidth > 0)
		{
			// How much table one pixel covers at the point being looked at.
			const double PerPixel = 2.0 * Eye->GetDistance() * FMath::Tan(FMath::DegreesToRadians(Eye->GetHorizontalFov() * 0.5)) / ViewWidth;
			const FRotator Heading(0.0f, Eye->GetYaw(), 0.0f);
			const FVector Forward = Heading.Vector();
			const FVector Right = FRotationMatrix(Heading).GetUnitAxis(EAxis::Y);
			const double Foreshorten = 1.0 / FMath::Max(0.3, FMath::Sin(FMath::DegreesToRadians(-Eye->GetPitch())));
			Eye->PanWorld(-Right * MouseDelta.X * PerPixel + Forward * MouseDelta.Y * PerPixel * Foreshorten);
		}
	}
	if (bLeftHeld && !IsInputKeyDown(EKeys::LeftMouseButton))
	{
		if (HeldPiece.IsValid())
		{
			if (bDragging)
			{
				DropArmy(PointedRegion, bOnTable);
			}
			else if (bHeldWasInHand)
			{
				// A plain click on the flag of the army already in hand puts it down again.
				PutDownArmy();
			}
			// Otherwise a plain click on the flag has already picked the army up.
		}
		else if (!bDragging && !bBusy)
		{
			// A second click on the same ground straight after the first goes there: the view centres on it, a little closer.
			const double ClickedAt = FPlatformTime::Seconds();
			const int32 Ground = PointedPiece && PointedPiece->IsArmy() ? PointedPiece->GetRegionId() : PointedRegion;
			// (not with an army or a fleet in hand: there a second click on the same ground is the order to go)
			if (ArmyRegion == INDEX_NONE && FleetInHand == INDEX_NONE && Ground != INDEX_NONE && Ground == LastClickRegion && ClickedAt - LastClickAt < 0.38 && Map->GetData().Regions.IsValidIndex(Ground))
			{
				const FVector2D At = Map->GetData().Regions[Ground].Center;
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.34f), false);
				LastClickRegion = INDEX_NONE;
			}
			else
			{
				LastClickRegion = Ground;
				LastClickAt = ClickedAt;
				ClickOnTable(PointedPiece, PointedRegion);
			}
		}
		bLeftHeld = false;
		bDragging = false;
		HeldPiece.Reset();
	}

	// A flag the pointer rests on shows it can be picked up.
	AArmyPiece* Offered = (!bBusy && !HeldPiece.IsValid() && CanPickUp(PointedPiece)) ? PointedPiece : nullptr;
	if (HoverPiece.Get() != Offered)
	{
		if (HoverPiece.IsValid())
		{
			HoverPiece->SetHovered(false);
		}
		HoverPiece = Offered;
		if (Offered)
		{
			Offered->SetHovered(true);
		}
	}
	// The pointer itself says what a right-click would do with what is in hand: cross-hairs over ground or a coast
	// that would be attacked, a barred circle over what is out of reach this month.
	EMouseCursor::Type Pointer = HeldPiece.IsValid() && bDragging ? EMouseCursor::GrabHandClosed : (Offered ? EMouseCursor::GrabHand : EMouseCursor::Default);
	if (Pointer == EMouseCursor::Default && !bBusy && !bScripted && !bTurning && !bPointerOverPanel && bOnTable)
	{
		if (ArmyRegion != INDEX_NONE && PointedRegion != INDEX_NONE && PointedRegion != ArmyRegion)
		{
			const FReachRegion* There = FindReach(PointedRegion);
			Pointer = !There ? EMouseCursor::SlashedCircle : (There->bAttack || There->IsWar()) ? EMouseCursor::Crosshairs : EMouseCursor::Default;
		}
		else if (FleetInHand != INDEX_NONE)
		{
			const FCampaignSession* Now = GetSession();
			const FViewFleet* Held = Now ? Now->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
			if (Held && bPointerOnWater)
			{
				Pointer = Held->Reach.Contains(SeaNear(PointerMap)) ? EMouseCursor::Default : EMouseCursor::SlashedCircle;
			}
			else if (Held && PointedRegion != INDEX_NONE)
			{
				const bool bShell = FleetHand.bGrand && !FleetHand.bShelled && FleetHand.Coasts.Contains(PointedRegion);
				Pointer = bShell ? EMouseCursor::Crosshairs : (PointedRegion == Held->Base || Held->Bases.Contains(PointedRegion)) ? EMouseCursor::Default : EMouseCursor::SlashedCircle;
			}
		}
	}
	CurrentMouseCursor = Pointer;

	// Whatever the pointer rests on is lit faintly, and with an army in hand the road to it is drawn.
	if (!bScripted && !bTurning && (!bDragging || HeldPiece.IsValid()))
	{
		Map->SetHoveredRegion(bBusy || bPointerOverPanel ? INDEX_NONE : PointedRegion); // (nor is the ground behind a panel lit)
		if (ArmyRegion != INDEX_NONE && !bBusy)
		{
			ShowRouteTo(PointedRegion);
		}
	}

	Map->SetShowProvinces(IsInputKeyDown(EKeys::P)); // (held: every province line at full strength)
	Map->UpdateForCamera(Eye->GetDistance(), Eye->GetYaw(), Eye->GetFocus());

	// A tip that follows the pointer: what the province under it is, whose, what it is worth and who stands there.
	// (no tip for the ground behind a panel the pointer is on)
	const bool bStagedTip = StagedHover != INDEX_NONE && Map->GetData().Regions.IsValidIndex(StagedHover);
	if (bStagedTip)
	{
		const FCampaignRegion& Staged = Map->GetData().Regions[StagedHover];
		ProjectWorldLocationToScreen(Map->MapToWorld(Staged.Centroid.X, Staged.Centroid.Y), StagedPointer);
	}
	// (while pictures are being taken the real pointer, wherever it happens to lie, says nothing)
	const int32 TipRegion = bStagedTip ? StagedHover : (bScripted || bBusy || !bHaveMouse || bLeftHeld || bRightHeld || bPointerOverPanel) ? INDEX_NONE : PointedRegion;
	const int32 TipField = (TipRegion == INDEX_NONE || bStagedTip) ? INDEX_NONE : PointedField;
	const int32 TipHand = ArmyRegion * 4 + (bByRail ? 1 : 0) + (bBySea ? 2 : 0);
	if (TipRegion != HoverTipRegion || TipField != HoverTipField || TipHand != HoverTipHand)
	{
		HoverTipRegion = TipRegion;
		HoverTipField = TipField;
		HoverTipHand = TipHand;
		HoverTip = FText::GetEmpty();
		FCampaignSession* Session = GetSession();
		if (Session && Map->GetData().Regions.IsValidIndex(TipRegion))
		{
			const FCampaignRegion& Region = Map->GetData().Regions[TipRegion];
			const FCampaignView& View = Session->GetView();
			const FString Holder = View.RegionHolder.IsValidIndex(TipRegion) ? View.RegionHolder[TipRegion] : Region.Owner;
			const FCampaignNation* Nation = Map->GetData().FindNation(Holder);
			const FString Standing = View.RegionStanding.IsValidIndex(TipRegion) ? View.RegionStanding[TipRegion] : FString();
			FString Line = FString::Printf(TEXT("%s  \u00B7  %s%s"), *Region.Name, Nation ? *Nation->Name : *Holder,
				Standing == TEXT("own") ? TEXT(" (yours)") : Standing == TEXT("enemy") ? TEXT(" (enemy)") : Standing == TEXT("ally") ? TEXT(" (ally)") : Standing == TEXT("neutral") ? TEXT(" (at peace)") : TEXT(""));
			TArray<FString> Worth;
			// What the place pays a month, where the rules have said; its victory points where they have not.
			if (const FTownInfo* Town = Map->TownOf(TipRegion))
			{
				if (Town->Worth > 0) { Worth.Add(FString::Printf(TEXT("pays %d a month"), Town->Worth)); }
			}
			else if (Region.VictoryPoints > 0) { Worth.Add(FString::Printf(TEXT("worth %d"), Region.VictoryPoints)); }
			if (Region.Factories > 0) { Worth.Add(FString::Printf(TEXT("%d %s"), Region.Factories, Region.Factories == 1 ? TEXT("factory") : TEXT("factories"))); }
			if (Region.bOil) { Worth.Add(TEXT("oil")); }
			if (Region.bPort) { Worth.Add(TEXT("harbour")); }
			if (Worth.Num() > 0)
			{
				Line += TEXT("\n") + FString::Join(Worth, TEXT("  \u00B7  "));
			}
			TMap<FString, FIntPoint> Forces; // by nation: corps, divisions
			bool bDim = false;
			for (const FViewCorps& Unit : View.Corps)
			{
				if (Unit.Region == TipRegion)
				{
					FIntPoint& Count = Forces.FindOrAdd(Unit.Nation);
					Count.X += 1;
					Count.Y += Unit.Divisions;
					bDim |= Unit.bDim;
				}
			}
			for (const TPair<FString, FIntPoint>& Force : Forces)
			{
				const FCampaignNation* Whose = Map->GetData().FindNation(Force.Key);
				Line += bDim && Force.Value.Y == 0
					? FString::Printf(TEXT("\n%s army: %d corps, strength unknown"), Whose ? *Whose->Adjective : *Force.Key, Force.Value.X)
					: FString::Printf(TEXT("\n%s army: %d corps, %d division%s"), Whose ? *Whose->Adjective : *Force.Key, Force.Value.X, Force.Value.Y, Force.Value.Y == 1 ? TEXT("") : TEXT("s"));
				// What stands round its base, in words: the same thing the table shows.
				if (const AArmyPiece* Stands = Map->FindPiece(TipRegion, Force.Key))
				{
					TArray<FString> About;
					// (where months of digging are counted, how far its works have got: attacks wear them down)
					float Works = -1.0f;
					for (const FViewCorps& Unit : View.Corps)
					{
						if (Unit.Region == TipRegion && Unit.Nation == Force.Key && Unit.bDug && !Unit.bDim && Unit.Works >= 0.0f)
						{
							Works = Works < 0.0f ? Unit.Works : FMath::Min(Works, Unit.Works);
						}
					}
					if (Works >= 0.0f)
					{
						About.Add(Works >= View.DugFull - 0.05f ? FString(TEXT("dug in, works finished"))
							: Works < 0.05f ? FString(TEXT("dug in, works battered flat"))
							: FString::Printf(TEXT("dug in, works %s of %d months"), *FString::SanitizeFloat(FMath::RoundToFloat(Works * 10.0f) / 10.0f, 0), View.DugFull));
					}
					else if (Stands->GetWorksState() == 2) { About.Add(TEXT("dug in")); }
					else if (Stands->GetWorksState() == 1) { About.Add(TEXT("digging in")); }
					if (Stands->IsInCamp()) { About.Add(TEXT("in camp, resting")); }
					if (Stands->GetSupplyState() == 2) { About.Add(TEXT("cut off from supply")); }
					if (About.Num() > 0)
					{
						Line += FString::Printf(TEXT(": %s"), *FString::Join(About, TEXT(", ")));
					}
				}
			}
			// Over an airfield: what it is and what stands on it, and that a click opens it.
			if (const FViewAirfield* Field = View.Airfields.FindByPredicate([TipField](const FViewAirfield& Each) { return Each.Region == TipField; }))
			{
				const FString Stands = Field->Groups < 0 ? FString(TEXT("what flies from it is not known"))
					: Field->Groups == 0 ? FString(TEXT("no aircraft based here"))
					: FString::Printf(TEXT("%d air group%s, %s aircraft%s"), Field->Groups, Field->Groups == 1 ? TEXT("") : TEXT("s"), *FText::AsNumber(Field->Aircraft).ToString(), Field->bCrowded ? TEXT(" (crowded)") : TEXT(""));
				Line = FString::Printf(TEXT("%s  \u00B7  %s\nServes %s aircraft: %s.\nClick to open the airfield."), *Field->Word.ToUpper(), *Region.Name, *FText::AsNumber(Field->Holds).ToString(), *Stands);
			}
			// With an army in hand: what a right-click here would do, and what it would cost.
			if (ArmyRegion != INDEX_NONE && TipRegion != ArmyRegion)
			{
				if (const FReachRegion* There = FindReach(TipRegion))
				{
					if (There->IsWar())
					{
						const FCampaignNation* Theirs = Map->GetData().FindNation(There->War);
						Line += FString::Printf(TEXT("\nRight-click: march in, which means war with %s. You are asked first."), Theirs ? *Theirs->Name : *There->War);
					}
					else if (There->bAttack)
					{
						if (bBySea)
						{
							Line += TEXT("\nRight-click: land here from the sea. You are shown what it will meet first.");
						}
						else
						{
							// The odds at a glance, before anything is ordered: as the staff reckon them from what is known.
							const FAttackPreview Glance = Session->PreviewAttack(PickedCorps, TipRegion);
							auto Say = [](float Odds) { return Odds >= 1.0f ? FString::Printf(TEXT("%.1f to 1 for you"), Odds) : FString::Printf(TEXT("%.1f to 1 against"), 1.0f / FMath::Max(Odds, 0.01f)); };
							if (!Glance.Refused.IsEmpty())
							{
								Line += TEXT("\n") + Glance.Refused;
							}
							else if (!Glance.bValid)
							{
								Line += TEXT("\nRight-click: attack. You are shown the odds first.");
							}
							else if (Glance.bExact)
							{
								Line += FString::Printf(TEXT("\nRight-click: attack. Odds %s."), *Say(Glance.Odds));
							}
							else
							{
								Line += FString::Printf(TEXT("\nRight-click: attack. The staff's estimate: between %s and %s."), *Say(Glance.OddsLow), *Say(Glance.OddsHigh));
							}
							if (Glance.bValid && Glance.Even > 1.25f)
							{
								Line += FString::Printf(TEXT(" Here it takes %.1f to 1 for an even chance."), Glance.Even);
							}
						}
					}
					else if (bByRail || bBySea)
					{
						Line += bByRail ? TEXT("\nRight-click: send them here by rail.") : TEXT("\nRight-click: send them here by sea.");
					}
					else
					{
						Line += FString::Printf(TEXT("\nRight-click: march here, %d week%s of the month."), FMath::Max(There->Cost, 1), There->Cost == 1 ? TEXT("") : TEXT("s"));
					}
				}
				else
				{
					Line += TEXT("\nOut of this army's reach this month.");
				}
			}
			if (FleetInHand != INDEX_NONE)
			{
				const FViewFleet* Held = View.Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
				if (Held && Held->Base == TipRegion)
				{
					Line += FString::Printf(TEXT("\nRight-click: %s returns to harbour here."), *Held->Name);
				}
				else if (Held && FleetHand.bGrand && !FleetHand.bShelled && FleetHand.Coasts.Contains(TipRegion))
				{
					Line += FString::Printf(TEXT("\nRight-click: %s stands in to shell this coast. You are shown what it would do first."), *Held->Name);
				}
				else if (Held && Held->Bases.Contains(TipRegion))
				{
					Line += FString::Printf(TEXT("\nRight-click: %s makes this its home port, and lies in harbour for the month."), *Held->Name);
				}
			}
			HoverTip = FText::FromString(Line);
		}
	}
	// With a fleet in hand, the sea under the pointer is marked, and what a right-click would do is said.
	{
		const int32 Sea = (FleetInHand != INDEX_NONE && !bBusy && bHaveMouse && bPointerOnWater && !bLeftHeld && !bRightHeld && !bPointerOverPanel) ? SeaNear(PointerMap) : INDEX_NONE;
		if (Sea != FleetHoverSea)
		{
			FleetHoverSea = Sea;
			FCampaignSession* Session = GetSession();
			const FViewFleet* Held = Session ? Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
			if (Held)
			{
				Map->ShowSeaReach(Held->Reach, Sea, Held->Id);
			}
			if (Held && Map->GetData().Seas.IsValidIndex(Sea))
			{
				const FString& Name = Map->GetData().Seas[Sea].Name;
				// What is known of the enemy there, and what a patrol would come to.
				FString Known;
				if (const FFleetSea* There = FleetHand.Seas.FindByPredicate([Sea](const FFleetSea& Each) { return Each.Zone == Sea; }))
				{
					Known = There->Surface > 0 || There->Subs > 0
						? FString::Printf(TEXT("\nEnemy there: %d surface warship%s, %d submarine%s."), There->Surface, There->Surface == 1 ? TEXT("") : TEXT("s"), There->Subs, There->Subs == 1 ? TEXT("") : TEXT("s"))
						: FString(TEXT("\nNo enemy ships are known there."));
					// (the fog at sea: forces only reported, and how well the sea has been searched)
					if (There->Reported > 0)
					{
						Known += FString::Printf(TEXT(" %d enemy force%s reported there and not identified."), There->Reported, There->Reported == 1 ? TEXT(" is") : TEXT("s are"));
					}
					if (!There->Search.IsEmpty())
					{
						Known += FString::Printf(TEXT(" %s%s."), *There->Search.Left(1).ToUpper(), *There->Search.Mid(1));
					}
					if (There->Meet == TEXT("shy"))
					{
						Known += TEXT(" Too strong for a patrol to face.");
					}
					if (There->Subs > 0 && There->Hunt > 0.005f)
					{
						Known += FString::Printf(TEXT(" Hunting would sink about %d%% of the submarines."), FMath::RoundToInt32(There->Hunt * 100.0f));
					}
					if (There->Torpedo >= 0.01f)
					{
						Known += FString::Printf(TEXT(" Torpedo risk to a heavy ship: %d%%."), FMath::RoundToInt32(There->Torpedo * 100.0f));
					}
				}
				HoverTip = FText::FromString(Held->Reach.Contains(Sea)
					? FString::Printf(TEXT("%s%s\nRight-click: %s sails here to %s."), *Name, *Known, *Held->Name, FleetNext == TEXT("raid") ? TEXT("raid merchant shipping") : FleetNext == TEXT("hunt") ? TEXT("hunt submarines") : TEXT("patrol"))
					: FString::Printf(TEXT("%s\nToo far for %s this month."), *Name, *Held->Name));
			}
			else if (TipRegion == INDEX_NONE)
			{
				HoverTip = FText::GetEmpty();
			}
		}
	}

	LastMouse = Mouse;
	bHaveLastMouse = bHaveMouse;
}

bool ACampaignPlayerController::CanPickUp(const AArmyPiece* Piece) const
{
	const FCampaignSession* Session = GetSession();
	if (!Piece || !Session || !Piece->IsArmy() || Piece->GetNationTag() != Session->GetView().Human)
	{
		return false; // (fleets and air fleets are given their orders from their panel, not carried)
	}
	// One of the player's, with at least one corps that can be given orders this month.
	for (const FViewCorps& Unit : Session->GetView().Corps)
	{
		if (Unit.Region == Piece->GetRegionId() && Unit.Nation == Piece->GetNationTag() && Unit.bCanOrder)
		{
			return true;
		}
	}
	return false;
}

bool ACampaignPlayerController::Hold(AArmyPiece* Piece)
{
	if (!CanPickUp(Piece))
	{
		return false;
	}
	const int32 RegionId = Piece->GetRegionId();
	// An army already in hand stays as it is: picking it up afresh would take back the corps told to stay behind.
	if (ArmyRegion != RegionId || RingedPiece.Get() != Piece)
	{
		SelectRegion(RegionId);
		if (!PickUpArmyIn(RegionId))
		{
			return false;
		}
		// The army answers: boots, hooves, lorries or tracks.
		TArray<FString> Kinds;
		for (const FViewCorps& Unit : Piece->GetCorps())
		{
			Kinds.Add(Unit.Kind);
		}
		const FCampaignSession* Session = GetSession();
		const TCHAR* Stir = CampaignSounds::OfArmy(Kinds, Session ? Session->GetView().Human : FString());
		CampaignSounds::Play(this, Stir, FCString::Strcmp(Stir, TEXT("marching")) == 0 || FCString::Strcmp(Stir, TEXT("horses")) == 0 ? 0.6f : 0.3f);
	}
	HeldPiece = RingedPiece.IsValid() ? RingedPiece.Get() : Piece;
	return true;
}

bool ACampaignPlayerController::HoldArmyIn(int32 RegionId)
{
	const FCampaignSession* Session = GetSession();
	return Session && Map.IsValid() && Hold(Map->FindPiece(RegionId, Session->GetView().Human));
}

void ACampaignPlayerController::CarryHeldOver(int32 RegionId)
{
	if (HeldPiece.IsValid() && Map.IsValid() && Map->GetData().Regions.IsValidIndex(RegionId))
	{
		const FVector2D At = Map->GetData().Regions[RegionId].Center;
		HeldPiece->Carry(Map->SurfacePoint(At.X, At.Y));
		Map->SetHoveredRegion(RegionId);
		ShowRouteTo(RegionId);
	}
}

void ACampaignPlayerController::DropHeldOver(int32 RegionId)
{
	DropArmy(RegionId, true);
	HeldPiece.Reset();
}

void ACampaignPlayerController::ShowForce(bool bFleet, int32 Id)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const FForcePanel Force = Session->Force(bFleet ? TEXT("fleet") : TEXT("air"), Id);
	if (!Force.bValid)
	{
		return;
	}
	// Whatever army was in hand is put down, and no territory is being looked at: the panel is this force's.
	PutDownArmy();
	SelectRegion(INDEX_NONE);
	ShownForce = Id;
	bShownFleet = bFleet;
	RegionPanel = FRegionPanelInfo();
	RegionPanel.Revision = ++PanelRevision;
	RegionPanel.bVisible = true;
	RegionPanel.Name = FText::FromString(Force.Name);
	RegionPanel.OwnerTag = Force.Nation;
	const FCampaignNation* Nation = Map->GetData().FindNation(Force.Nation);
	RegionPanel.OwnerLine = FText::FromString((Nation ? Nation->Name : Force.Nation) + (Force.bMine ? TEXT("  \u00B7  Yours") : TEXT("")));
	RegionPanel.Details = FText::FromString(bFleet ? TEXT("Fleet") : TEXT("Air fleet"));
	RegionPanel.Forces = FText::FromString(FString::Join(Force.Lines, TEXT("\n")));
	RegionPanel.OptionsTitle = FText::FromString(TEXT("ORDERS"));
	RegionPanel.Holdings = FText::FromString(Force.bMine ? (Force.Options.Num() > 0 ? TEXT("What it can be told to do this month:") : TEXT("")) : TEXT("Not yours to command."));
	RegionPanel.Options = Force.Options;
}

UTexture2D* ACampaignPlayerController::GetMinimapTexture() const
{
	return Map.IsValid() ? Map->GetMinimap() : nullptr;
}

bool ACampaignPlayerController::IsHelpShown() const
{
	// (a run that takes pictures shows the frame as a player who knows the game sees it, unless told otherwise)
	static const bool bPictures = FParse::Param(FCommandLine::Get(), TEXT("TheaterHelp")) ? false : FCString::Strifind(FCommandLine::Get(), TEXT("TheaterShots=")) != nullptr;
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterHelp"));
	return bHelpPinned || bAsked || (!bHelpSeen && !bPictures) || IsInputKeyDown(EKeys::Slash);
}

void ACampaignPlayerController::ToggleHelp()
{
	// Open, it is folded away, and the first month's showing with it; folded, it is kept open.
	const bool bOpen = bHelpPinned || !bHelpSeen;
	bHelpPinned = !bOpen;
	if (!bHelpSeen)
	{
		bHelpSeen = true;
		SaveOptions();
	}
}

bool ACampaignPlayerController::IsMapShown() const
{
	return GetSession() != nullptr && Map.IsValid() && !Opening.bVisible && !Newsreel.bOn;
}

void ACampaignPlayerController::GetMinimapMarks(TArray<FVector>& OutDots, FBox2D& OutLooking) const
{
	OutDots.Reset();
	OutLooking = FBox2D(ForceInit);
	const FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const FCampaignMapData& Data = Map->GetData();
	const FCampaignView& View = Session->GetView();
	// One mark for each province an army stands in, for each side that has one there.
	TSet<FIntPoint> Marked;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (!Data.Regions.IsValidIndex(Unit.Region))
		{
			continue;
		}
		const bool bMine = Unit.Nation == View.Human;
		const FString Standing = View.RegionStanding.IsValidIndex(Unit.Region) ? View.RegionStanding[Unit.Region] : FString();
		// (whose the army is, as nearly as the map can say: the player's own, or by where it stands)
		const int32 Whose = bMine ? 0 : (Standing == TEXT("enemy") ? 2 : Standing == TEXT("ally") || Standing == TEXT("own") ? 1 : 3);
		bool bAlready = false;
		Marked.Add(FIntPoint(Unit.Region, Whose), &bAlready);
		if (!bAlready)
		{
			const FVector2D At = Data.Regions[Unit.Region].Center;
			OutDots.Add(FVector(At.X / Data.Width, At.Y / Data.Height, Whose));
		}
	}
	if (const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		// About what the screen takes in: as wide as the view at the point looked at, and three fifths as deep.
		const FVector2D Middle = Map->WorldToMap(Eye->GetFocus());
		const double PerUnit = FVector::Dist(Map->MapToWorld(0.0, 0.0), Map->MapToWorld(1.0, 0.0));
		const double Wide = PerUnit > 0.0 ? 2.0 * Eye->GetDistance() * FMath::Tan(FMath::DegreesToRadians(Eye->GetHorizontalFov() * 0.5)) / PerUnit : 100.0;
		const FVector2D Half(Wide * 0.5 / Data.Width, Wide * 0.3 / Data.Height);
		const FVector2D Centre(Middle.X / Data.Width, Middle.Y / Data.Height);
		OutLooking = FBox2D(Centre - Half, Centre + Half);
	}
}

void ACampaignPlayerController::JumpToMapShare(const FVector2D& Share)
{
	if (!Map.IsValid())
	{
		return;
	}
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		const FCampaignMapData& Data = Map->GetData();
		Eye->GoTo(Map->MapToWorld(FMath::Clamp(Share.X, 0.0, 1.0) * Data.Width, FMath::Clamp(Share.Y, 0.0, 1.0) * Data.Height));
	}
}

int32 ACampaignPlayerController::SeaNear(const FVector2D& MapPoint) const
{
	// The rules keep the chart of which sea is where: asked again only when the pointer has moved a little.
	if (FVector2D::DistSquared(MapPoint, SeaAskedAt) < 4.0 && SeaAsked != INDEX_NONE)
	{
		return SeaAsked;
	}
	int32 Best = INDEX_NONE;
	if (FCampaignSession* Session = GetSession())
	{
		Best = Session->SeaAt(MapPoint);
	}
	if (Best == INDEX_NONE && Map.IsValid())
	{
		// (off the chart, or on the shore line: the sea whose middle is nearest)
		double Nearest = TNumericLimits<double>::Max();
		const TArray<FCampaignSeaZone>& Seas = Map->GetData().Seas;
		for (int32 Zone = 0; Zone < Seas.Num(); ++Zone)
		{
			const double Far = FVector2D::DistSquared(Seas[Zone].Center, MapPoint);
			if (Far < Nearest)
			{
				Nearest = Far;
				Best = Zone;
			}
		}
	}
	SeaAskedAt = MapPoint;
	SeaAsked = Best;
	return Best;
}

bool ACampaignPlayerController::PickUpFleet(int32 FleetId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return false;
	}
	const FCampaignView& View = Session->GetView();
	const FViewFleet* Fleet = View.Fleets.FindByPredicate([FleetId](const FViewFleet& Each) { return Each.Id == FleetId; });
	if (!Fleet || Fleet->Nation != View.Human)
	{
		return false;
	}
	PutDownArmy();
	LookRegion = INDEX_NONE;
	SelectRegion(INDEX_NONE);
	RegionPanel.bVisible = false;
	RegionPanel.Revision = ++PanelRevision;
	FleetInHand = FleetId;
	AdviserSaw(3);
	FleetNext = Fleet->Stance == TEXT("raid") ? TEXT("raid") : Fleet->Stance == TEXT("hunt") ? TEXT("hunt") : TEXT("patrol");
	FleetHoverSea = INDEX_NONE;
	CampaignSounds::Play(this, TEXT("ship_horn"), 0.35f);
	RefreshOrders();
	return true;
}

void ACampaignPlayerController::PutDownFleet()
{
	if (FleetInHand == INDEX_NONE)
	{
		return;
	}
	FleetInHand = INDEX_NONE;
	FleetHoverSea = INDEX_NONE;
	HoverTip = FText::GetEmpty();
	RefreshOrders();
}

void ACampaignPlayerController::LayOutFleet()
{
	FCampaignSession* Session = GetSession();
	const FCampaignView& View = Session->GetView();
	const FViewFleet* Fleet = View.Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
	if (!Fleet || Fleet->Nation != View.Human)
	{
		FleetInHand = INDEX_NONE; // sunk, or no longer the player's
		return;
	}
	const FCampaignNation* Nation = Map->GetData().FindNation(View.Human);
	const TArray<FCampaignSeaZone>& Seas = Map->GetData().Seas;
	const FString Sea = Seas.IsValidIndex(Fleet->Zone) ? Seas[Fleet->Zone].Name : FString(TEXT("the sea"));
	// The fleet as the rules describe it: what it is doing, who commands, the seas and coasts in its reach.
	FleetHand = Session->FleetInHand(FleetInHand);
	const FString Doing = FleetHand.bValid && !FleetHand.Doing.IsEmpty() ? FleetHand.Doing
		: Fleet->Stance == TEXT("port") ? FString::Printf(TEXT("In harbour at %s"), *Fleet->Port)
		: Fleet->Stance == TEXT("raid") ? FString::Printf(TEXT("Raiding shipping: %s"), *Sea) : FString::Printf(TEXT("On patrol: %s"), *Sea);
	Tray.bVisible = true;
	Tray.bFleet = true;
	Tray.bFleetGrand = FleetHand.bValid && FleetHand.bGrand;
	Tray.FleetAircraft = Fleet->WingFighters + Fleet->WingStrike;
	Tray.Color = Nation ? FLinearColor(Nation->Color) : FLinearColor::Gray;
	Tray.Tag = View.Human;
	Tray.Title = FText::FromString(Fleet->Name);
	Tray.Subtitle = FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d ship%s%s  \u00B7  can reach %d sea%s"), *Doing, Fleet->Ships(), Fleet->Ships() == 1 ? TEXT("") : TEXT("s"),
		FleetHand.Admiral.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  \u00B7  %s"), *FleetHand.Admiral), Fleet->Reach.Num(), Fleet->Reach.Num() == 1 ? TEXT("") : TEXT("s")));
	Tray.FleetWarning = FText::FromString(FleetHand.bShelled ? FString(TEXT("Its guns have spoken this month.")) : FleetHand.Screen);
	Tray.FleetStance = Fleet->Stance;
	if (FleetNext == TEXT("hunt") && !Tray.bFleetGrand)
	{
		FleetNext = TEXT("patrol");
	}
	Tray.FleetNext = FleetNext;
	Tray.PendingButton = FText::GetEmpty();
	Tray.Hint = FText::FromString(!View.bHumanUp ? TEXT("It is not your month.")
		: Tray.bFleetGrand && FleetHand.Coasts.Num() > 0
		? TEXT("Right-click a ringed sea to send it there, a coast in red to shell it, a port in blue to make it the fleet's home, or its own harbour to bring it in. ORDERS has the rest. Esc lets it go.")
		: TEXT("Right-click a ringed sea to send the fleet there, a port tinted blue to make it the fleet's new home, or its own harbour to bring it in. Esc lets it go."));
	// The friendly ports it could move to are tinted, as the ground an army can reach by rail is; and the enemy
	// coasts its guns could reach are red, as ground an army could attack is.
	{
		TArray<FReachRegion> Ports;
		for (int32 PortRegion : Fleet->Bases)
		{
			FReachRegion Entry;
			Entry.Region = PortRegion;
			Entry.Previous = Fleet->Base;
			Ports.Add(Entry);
		}
		if (!FleetHand.bShelled)
		{
			for (int32 Coast : FleetHand.Coasts)
			{
				FReachRegion Entry;
				Entry.Region = Coast;
				Entry.Previous = Fleet->Base;
				Entry.bAttack = true;
				Ports.Add(Entry);
			}
		}
		Map->SetReach(View.bHumanUp ? Ports : TArray<FReachRegion>(), true);
	}

	// A card for each ship. A great fleet has too many destroyers to show one by one: those are dealt in flotillas.
	struct FClass { int32 Count; const TCHAR* Key; const TCHAR* Model; const TCHAR* One; const TCHAR* Many; const TCHAR* Role; const TCHAR* Brief; bool bCapital; };
	const FClass Classes[] = {
		{ Fleet->Battleships, TEXT("bb"), TEXT("Battleship"), TEXT("Battleship"), TEXT("battleships"), TEXT("The line of battle: its guns decide who holds a sea."), TEXT("Fleet action"), true },
		{ Fleet->Carriers, TEXT("cv"), TEXT("Carrier"), TEXT("Aircraft carrier"), TEXT("carriers"), TEXT("Its aircraft strike before the guns are in range."), TEXT("Aircraft"), true },
		{ Fleet->Cruisers, TEXT("ca"), TEXT("Cruiser"), TEXT("Cruiser"), TEXT("cruisers"), TEXT("Scouts for the fleet, guards convoys and raids shipping."), TEXT("Scout and raider"), false },
		{ Fleet->Destroyers, TEXT("dd"), TEXT("Destroyer"), TEXT("Destroyer"), TEXT("destroyers"), TEXT("Screens the fleet, escorts convoys and hunts submarines."), TEXT("Screen and escort"), false },
		{ Fleet->Submarines, TEXT("ss"), TEXT("Submarine"), TEXT("Submarine"), TEXT("submarines"), TEXT("Sinks merchant ships, and slips away from the fleet."), TEXT("Commerce raider"), false },
	};
	const bool bAll = Fleet->Ships() <= 14;
	for (const FClass& Class : Classes)
	{
		if (Class.Count <= 0)
		{
			continue;
		}
		// What the class is for, in the rules' own words where they give them.
		const FFleetShipClass* Told = FleetHand.Classes.FindByPredicate([&Class](const FFleetShipClass& Each) { return Each.Class == Class.Key; });
		const FString Role = Told && !Told->Role.IsEmpty() ? Told->Role + (Told->Lost > 0 ? FString::Printf(TEXT(" %d lost since the war began."), Told->Lost) : FString()) : FString(Class.Role);
		const FString Brief = Told && !Told->Brief.IsEmpty() ? Told->Brief : FString(Class.Brief);
		const int32 CardCount = bAll ? Class.Count : FMath::Min(Class.Count, Class.bCapital ? 4 : 2);
		for (int32 Index = 0; Index < CardCount; ++Index)
		{
			FShipCard& Card = Tray.Ships.AddDefaulted_GetRef();
			Card.Model = FString::Printf(TEXT("%s:%s"), Class.Model, *View.Human);
			Card.Count = Class.Count / CardCount + (Index < Class.Count % CardCount ? 1 : 0);
			Card.Name = FText::FromString(Card.Count == 1 ? FString(Class.One) : FString::Printf(TEXT("%d %s"), Card.Count, Class.Many));
			Card.Role = FText::FromString(Role);
			Card.Brief = FText::FromString(Brief);
		}
	}
	if (Tray.Ships.Num() > 0)
	{
		ShowCorps = INDEX_NONE;
		ShowShipModel(Tray.Ships[0].Model, Tray.Ships[0].Name);
	}
	Map->ShowSeaReach(View.bHumanUp ? Fleet->Reach : TArray<int32>(), FleetHoverSea, Fleet->Id);
}

void ACampaignPlayerController::OpenFleetOrders()
{
	if (FleetInHand != INDEX_NONE)
	{
		OpenPage(FString::Printf(TEXT("fleet:%d"), FleetInHand));
	}
}

void ACampaignPlayerController::OpenFleetWings()
{
	if (FleetInHand != INDEX_NONE)
	{
		OpenPage(FString::Printf(TEXT("wings:%d"), FleetInHand));
	}
}

void ACampaignPlayerController::ShowShipModel(const FString& Model, const FText& Name)
{
	if (!Map.IsValid())
	{
		return;
	}
	Map->SetShowpiece(Model);
	ShowTitle = FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s"), *Tray.Title.ToString(), *Name.ToString()));
}

void ACampaignPlayerController::OrderFleetTo(int32 Zone)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !Map->GetData().Seas.IsValidIndex(Zone))
	{
		return;
	}
	const FViewFleet* Fleet = Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
	if (!Fleet)
	{
		return;
	}
	const FString Name = Fleet->Name, Sea = Map->GetData().Seas[Zone].Name;
	if (!Fleet->Reach.Contains(Zone))
	{
		CampaignSounds::Play(this, TEXT("click"), 0.5f);
		Notify(FString::Printf(TEXT("%s cannot reach the %s this month."), *Name, *Sea), 5.0);
		return;
	}
	FString Note;
	if (Session->OrderForce(FString::Printf(TEXT("fleet:%d:%d:%s"), FleetInHand, Zone, *FleetNext), Note))
	{
		CampaignSounds::Play(this, TEXT("ship_horn"), 0.45f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		Notify(FString::Printf(TEXT("%s sails for the %s, to %s."), *Name, *Sea, FleetNext == TEXT("raid") ? TEXT("raid merchant shipping") : FleetNext == TEXT("hunt") ? TEXT("hunt submarines") : TEXT("patrol")), 5.0);
	}
	else
	{
		Notify(Note.IsEmpty() ? FString(TEXT("That order cannot be given now.")) : Note, 5.0);
	}
}

void ACampaignPlayerController::SetFleetStance(const FString& Stance)
{
	FCampaignSession* Session = GetSession();
	if (!Session || FleetInHand == INDEX_NONE)
	{
		return;
	}
	FleetNext = Stance == TEXT("raid") ? TEXT("raid") : Stance == TEXT("hunt") ? TEXT("hunt") : TEXT("patrol");
	const FViewFleet* Fleet = Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	if (Fleet && Fleet->Stance != TEXT("port") && Fleet->Stance != FleetNext)
	{
		// At sea already: it changes what it is doing where it is.
		FString Note;
		const FString Name = Fleet->Name;
		if (Session->OrderForce(FString::Printf(TEXT("fleet:%d:%d:%s"), FleetInHand, Fleet->Zone, *FleetNext), Note))
		{
			ShowWar(Session->GetView(), TArray<FCorpsMove>());
			Notify(FString::Printf(TEXT("%s: %s."), *Name, FleetNext == TEXT("raid") ? TEXT("raiding merchant shipping, and keeping clear of the enemy fleet")
				: FleetNext == TEXT("hunt") ? TEXT("hunting submarines with its destroyers") : TEXT("on patrol, seeking the enemy's warships and guarding the convoys")), 5.0);
		}
	}
	RefreshOrders();
}

void ACampaignPlayerController::FleetToNewBase(int32 RegionId)
{
	FCampaignSession* Session = GetSession();
	const FViewFleet* Fleet = Session ? Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
	if (!Fleet || !Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return;
	}
	FString Note;
	const FString Name = Fleet->Name, Port = Map->GetData().Regions[RegionId].Name;
	if (Session->OrderForce(FString::Printf(TEXT("fleetbase:%d:%d"), FleetInHand, RegionId), Note))
	{
		CampaignSounds::Play(this, TEXT("ship_horn"), 0.45f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		Notify(FString::Printf(TEXT("%s moves its base to %s, and lies in harbour there for the month."), *Name, *Port), 6.0);
	}
	else
	{
		Notify(TEXT("The fleet cannot move its base there now."), 5.0);
	}
}

void ACampaignPlayerController::FleetToHarbour()
{
	FCampaignSession* Session = GetSession();
	const FViewFleet* Fleet = Session ? Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
	if (!Fleet)
	{
		return;
	}
	FString Note;
	const FString Name = Fleet->Name, Port = Fleet->Port;
	if (Session->OrderForce(FString::Printf(TEXT("fleet:%d:%d:port"), FleetInHand, Fleet->Home), Note))
	{
		CampaignSounds::Play(this, TEXT("ship_horn"), 0.45f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		Notify(FString::Printf(TEXT("%s returns to harbour at %s."), *Name, *Port), 5.0);
	}
	else
	{
		Notify(Note.IsEmpty() ? FString(TEXT("That order cannot be given now.")) : Note, 5.0);
	}
}

void ACampaignPlayerController::ClickOnTable(AArmyPiece* Piece, int32 RegionId)
{
	StopLooking();
	// A fleet or an air fleet: look at it, and at what it can be told to do.
	if (Piece && !Piece->IsArmy() && Piece->GetForceId() != INDEX_NONE)
	{
		if (!Piece->IsFleet() && Piece->GetBaseRegion() != INDEX_NONE)
		{
			// Aircraft belong to their airfield: a click on them opens it, with every group based there.
			PutDownArmy();
			SelectRegion(Piece->GetBaseRegion());
			OpenPage(TEXT("airfield"));
			CampaignSounds::Play(this, TEXT("fighter_pass"), 0.35f);
			return;
		}
		if (Piece->IsFleet())
		{
			// One of the player's fleets is picked up like an army; a click on the fleet in hand puts it down.
			if (FleetInHand == Piece->GetForceId())
			{
				PutDownFleet();
				return;
			}
			if (PickUpFleet(Piece->GetForceId()))
			{
				return;
			}
			PutDownFleet();
		}
		ShowForce(Piece->IsFleet(), Piece->GetForceId());
		CampaignSounds::Play(this, Piece->IsFleet() ? TEXT("ship_horn") : TEXT("fighter_pass"), 0.35f);
		return;
	}
	if (FleetInHand != INDEX_NONE && !Piece)
	{
		if (bPointerOnWater)
		{
			OrderFleetTo(SeaNear(PointerMap));
			return;
		}
		PutDownFleet(); // a click on the land lets the fleet go, and looks at the land
	}
	// A click on somebody's flag is a click on the ground it stands on.
	const int32 Target = Piece ? Piece->GetRegionId() : RegionId;

	// An airfield with nothing in hand: it opens, with the groups based there and what they can be sent to do.
	if (!Piece && ArmyRegion == INDEX_NONE && PointedField != INDEX_NONE && GetSession() && GetSession()->GetView().Airfields.Num() > 0)
	{
		SelectRegion(PointedField);
		OpenPage(TEXT("airfield"));
		CampaignSounds::Play(this, TEXT("click"), 0.5f);
		return;
	}

	if (ArmyRegion != INDEX_NONE && Target != INDEX_NONE && Target != ArmyRegion && FindReach(Target))
	{
		// With an army in hand, ground it can reach takes two clicks: the first looks at the place and offers it,
		// the second sends the army. A click meant for something else never moves anybody.
		if (PendingTarget == Target)
		{
			ConfirmPending();
		}
		else
		{
			SelectRegion(Target);
			SetPending(Target);
		}
		return;
	}
	if (ArmyRegion != INDEX_NONE && Target == ArmyRegion)
	{
		// A click on the ground the army in hand stands on puts it down.
		PutDownArmy();
		SelectRegion(Target);
		return;
	}
	// Anywhere else: look at that territory. If one of the player's armies is there it is picked up; if not,
	// whatever was in hand is put down.
	SelectRegion(Target);
	if (!PickUpArmyIn(Target))
	{
		PutDownArmy();
		if (Target != INDEX_NONE)
		{
			AdviserSaw(2); // a province looked at
			// Somebody's army stands there that cannot be picked up: its corps are dealt as cards all the same, as far
			// as they are known. The flag that was clicked, or else the first army on that ground.
			FString Whose = Piece && Piece->IsArmy() ? Piece->GetNationTag() : FString();
			if (Whose.IsEmpty() && GetSession())
			{
				for (const FViewCorps& Unit : GetSession()->GetView().Corps)
				{
					if (Unit.Region == Target)
					{
						Whose = Unit.Nation;
						break;
					}
				}
			}
			if (!Whose.IsEmpty())
			{
				LookAtArmy(Target, Whose);
			}
		}
	}
}

void ACampaignPlayerController::LookAtArmy(int32 RegionId, const FString& Nation)
{
	if (ArmyRegion != INDEX_NONE || FleetInHand != INDEX_NONE || RegionId == INDEX_NONE || Nation.IsEmpty())
	{
		return;
	}
	LookRegion = RegionId;
	LookNation = Nation;
	RefreshOrders();
}

void ACampaignPlayerController::StopLooking()
{
	if (LookRegion != INDEX_NONE)
	{
		LookRegion = INDEX_NONE;
		RefreshOrders();
	}
}

void ACampaignPlayerController::LayOutLook()
{
	FCampaignSession* Session = GetSession();
	const FCampaignView& View = Session->GetView();
	const FCampaignNation* Nation = Map->GetData().FindNation(LookNation);
	const FString Adjective = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : LookNation;
	int32 Known = 0, Unknown = 0, Divisions = 0, Reports = 0;
	FString General;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Region != LookRegion || Unit.Nation != LookNation)
		{
			continue;
		}
		FTrayCard Card;
		Card.CorpsId = Unit.Id;
		Card.bLook = true;
		if (Unit.bDim)
		{
			Card.bUnknown = true;
			Card.Name = FText::FromString(TEXT("Not identified"));
			Card.Kind = FText::FromString(TEXT("a corps"));
			++Unknown;
		}
		else
		{
			Card.Name = FText::FromString(Unit.Name);
			Card.Kind = FText::FromString(FCampaignMapData::CorpsKindLabel(Unit.Kind));
			Card.Divisions = Unit.Divisions;
			Card.Strength = Unit.Strength;
			Card.KindId = Unit.Elite == TEXT("heavy") ? FString(TEXT("heavy")) : Unit.Kind;
			Card.Ally = Unit.Ally;
			Card.Morale = Unit.Cohesion;
			Card.Experience = Unit.Experience;
			Card.Stale = Unit.Stale;
			Card.Nickname = FText::FromString(Unit.Nickname);
			Card.Grade = FText::FromString(Unit.Grade.IsEmpty() ? FString() : FString::Printf(TEXT("%s, %d"), *Unit.Grade, Unit.Experience));
			Card.Honours = Unit.Honours;
			Card.Parts = Unit.Parts;
			if (Unit.Morale >= 0.0f)
			{
				Card.bMood = true;
				Card.Morale = Unit.Morale;
				Card.Rest = Unit.Rest >= 0.0f ? Unit.Rest : Unit.Cohesion;
				Card.MoraleWord = Unit.MoraleWord;
				Card.RestWord = Unit.RestWord;
				Card.Trend = Unit.Trend;
				Card.bResting = Unit.bResting;
				Card.MoodTip = Unit.MoodLine + TEXT("\n") + Unit.RestLine;
			}
			++Known;
			Divisions += Unit.Divisions;
			Reports += Unit.Stale > 0 ? 1 : 0;
			if (General.IsEmpty() && !Unit.General.IsEmpty())
			{
				General = Unit.General;
			}
		}
		Tray.Cards.Add(Card);
	}
	if (Tray.Cards.Num() == 0)
	{
		LookRegion = INDEX_NONE; // nobody stands there any more
		return;
	}
	const FString Where = Map->GetData().Regions.IsValidIndex(LookRegion) ? Map->GetData().Regions[LookRegion].Name : FString();
	const FString Standing = View.RegionStanding.IsValidIndex(LookRegion) ? View.RegionStanding[LookRegion] : FString();
	const bool bMine = LookNation == View.Human;
	Tray.bVisible = true;
	Tray.bLook = true;
	Tray.Color = Nation ? FLinearColor(Nation->Color) : FLinearColor::Gray;
	Tray.Tag = LookNation;
	Tray.Title = FText::FromString(General.IsEmpty() ? FString::Printf(TEXT("%s army"), *Adjective) : General);
	const FString Size = Unknown == 0 ? FString::Printf(TEXT("%d corps, %d division%s%s"), Known, Divisions, Divisions == 1 ? TEXT("") : TEXT("s"), Reports > 0 ? TEXT(" (from reports gone stale)") : TEXT(""))
		: Known == 0 ? FString::Printf(TEXT("%d corps, none identified"), Unknown)
		: FString::Printf(TEXT("%d corps: %d identified, %d not"), Known + Unknown, Known, Unknown);
	Tray.Subtitle = FText::FromString(FString::Printf(TEXT("%s army at %s  \u00B7  %s"), *Adjective, *Where, *Size));
	Tray.PendingButton = FText::GetEmpty();
	Tray.JoinButton = FText::GetEmpty();
	Tray.Hint = FText::FromString(bMine ? TEXT("This army has used its month, or holds its ground: it can be given no orders now.")
		: Unknown > 0 ? TEXT("Not yours to command. A battle, an agent on watch or a reconnaissance flight would show what the unidentified corps are.")
		: TEXT("Not yours to command."));
	// The model of its first known corps stands beside the cards.
	for (const FTrayCard& Card : Tray.Cards)
	{
		if (!Card.bUnknown)
		{
			ShowCorps = INDEX_NONE;
			ShowCorpsModel(Card.CorpsId);
			break;
		}
	}
}

void ACampaignPlayerController::SetPending(int32 RegionId)
{
	PendingTarget = RegionId;
	Tray.PendingButton = FText::GetEmpty();
	Tray.Hint = FText::FromString(bBySea
		? TEXT("Click a port or a coast tinted blue, then click it again to send them by sea. Esc, or a click on their flag, puts them down.")
		: bByRail
		? TEXT("Click ground tinted blue, then click it again to send them by rail. Esc, or a click on their flag, puts them down.")
		: TEXT("Right-click where they should go, or drag the flag there. Esc puts them down."));
	const FReachRegion* Target = FindReach(RegionId);
	if (Target && Map.IsValid() && Map->GetData().Regions.IsValidIndex(RegionId))
	{
		const FString Name = Map->GetData().Regions[RegionId].Name;
		Tray.PendingButton = FText::FromString(FString::Printf(TEXT("%s %s"),
			bBySea ? (Target->bAttack ? TEXT("LAND AT") : TEXT("BY SEA TO")) : bByRail ? TEXT("BY RAIL TO") : Target->IsWar() ? TEXT("INVADE") : (Target->bAttack ? TEXT("ATTACK") : TEXT("MARCH TO")), *Name.ToUpper()));
		Tray.Hint = FText::FromString(FString::Printf(TEXT("Click %s again, or press the button above, to %s. Click anywhere else to think again."),
			*Name, Target->IsWar() ? TEXT("be asked whether you mean war") : Target->bAttack ? TEXT("draw up the attack") : TEXT("send them")));
	}
	else
	{
		PendingTarget = INDEX_NONE;
	}
	++Tray.Revision;
}

void ACampaignPlayerController::ConfirmPending()
{
	const int32 Target = PendingTarget;
	if (Target != INDEX_NONE && ArmyRegion != INDEX_NONE)
	{
		SetPending(INDEX_NONE);
		OrderTo(Target);
	}
}

void ACampaignPlayerController::DropArmy(int32 RegionId, bool bOnTable)
{
	AArmyPiece* Piece = HeldPiece.Get();
	if (!Piece)
	{
		return;
	}
	const FReachRegion* Target = (bOnTable && RegionId != INDEX_NONE && RegionId != ArmyRegion) ? FindReach(RegionId) : nullptr;
	if (Target && Target->IsWar())
	{
		// Set down across a frontier at peace: the flag goes back, and the game asks whether he means war.
		Piece->ReturnToRest();
		OfferWar(RegionId);
	}
	else if (Target && !Target->bAttack)
	{
		// Set down on ground it can reach: it goes there, settling from where the hand let go.
		const FVector From = Piece->GetActorLocation();
		OrderTo(RegionId, &From);
	}
	else if (Target)
	{
		// Set down on the enemy: the face-off decides whether the attack goes in. The flag goes back meanwhile.
		Piece->ReturnToRest();
		OrderTo(RegionId);
	}
	else if (RegionId != ArmyRegion && RegionId != INDEX_NONE)
	{
		// Set down on a country at peace with the player: does he mean war?
		Piece->ReturnToRest();
		if (!OfferWar(RegionId))
		{
			Notify(TEXT("They cannot get there this month. Lightened ground is where they can go."));
		}
	}
	// Whatever is left of the army where it started goes back to its place.
	if (HeldPiece.IsValid())
	{
		HeldPiece->ReturnToRest();
	}
}

void ACampaignPlayerController::SelectRegion(int32 RegionId)
{
	if (!Map.IsValid())
	{
		return;
	}
	SelectedRegion = RegionId;
	Map->SetSelectedRegion(RegionId);
	RefreshRegionPanel(RegionId);
}

void ACampaignPlayerController::OfferWarOnSelected()
{
	OfferWar(SelectedRegion, false); // asked from the panel: no army is on its way there
}

bool ACampaignPlayerController::PickUpArmyIn(int32 RegionId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || RegionId == INDEX_NONE)
	{
		return false;
	}
	const FCampaignView& View = Session->GetView();
	TArray<int32> Orderable;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Region == RegionId && Unit.Nation == View.Human && Unit.bCanOrder)
		{
			Orderable.Add(Unit.Id);
		}
	}
	if (Orderable.Num() == 0)
	{
		return false;
	}
	const bool bNewInHand = ArmyRegion != RegionId;
	ArmyRegion = RegionId;
	AdviserSaw(0);
	PickedCorps = MoveTemp(Orderable);
	RefreshOrders();
	if (bNewInHand)
	{
		// It answers, in its own language: a word on the tray for a moment (and one day a voice).
		struct FAnswers { const TCHAR* Tag; const TCHAR* Said[3]; };
		static const FAnswers Answers[] =
		{
			{ TEXT("GER"), { TEXT("Zu Befehl!"), TEXT("Jawohl!"), TEXT("Wird gemacht!") } },
			{ TEXT("UK"), { TEXT("Very good, sir."), TEXT("Right away, sir."), TEXT("Ready to move, sir.") } },
			{ TEXT("USA"), { TEXT("Yes, sir."), TEXT("On our way."), TEXT("Ready to roll.") } },
			{ TEXT("FRA"), { TEXT("\u00C0 vos ordres !"), TEXT("Bien re\u00E7u !"), TEXT("En avant !") } },
			{ TEXT("VIC"), { TEXT("\u00C0 vos ordres !"), TEXT("Bien re\u00E7u !"), TEXT("En avant !") } },
			{ TEXT("ITA"), { TEXT("Agli ordini!"), TEXT("Signors\u00EC!"), TEXT("Avanti!") } },
			{ TEXT("SOV"), { TEXT("\u0415\u0441\u0442\u044C!"), TEXT("\u0422\u0430\u043A \u0442\u043E\u0447\u043D\u043E!"), TEXT("\u0421\u043B\u0443\u0448\u0430\u044E\u0441\u044C!") } },
			{ TEXT("POL"), { TEXT("Rozkaz!"), TEXT("Tak jest!"), TEXT("Naprz\u00F3d!") } },
			{ TEXT("NED"), { TEXT("Tot uw orders!"), TEXT("Jawel!"), TEXT("Voorwaarts!") } },
			{ TEXT("BEL"), { TEXT("\u00C0 vos ordres !"), TEXT("Tot uw orders!"), TEXT("En avant !") } },
			{ TEXT("SWE"), { TEXT("Uppfattat!"), TEXT("Ska bli!"), TEXT("Fram\u00E5t!") } },
			{ TEXT("NOR"), { TEXT("Javel!"), TEXT("Skal bli!"), TEXT("Fremad!") } },
			{ TEXT("DEN"), { TEXT("Javel!"), TEXT("Forst\u00E5et!"), TEXT("Fremad!") } },
			{ TEXT("FIN"), { TEXT("K\u00E4skyst\u00E4!"), TEXT("Selv\u00E4!"), TEXT("Eteenp\u00E4in!") } },
			{ TEXT("HUN"), { TEXT("Igenis!"), TEXT("Parancs!"), TEXT("El\u0151re!") } },
			{ TEXT("ROM"), { TEXT("Am \u00EEn\u021Beles!"), TEXT("S\u0103 tr\u0103i\u021Bi!"), TEXT("\u00CEnainte!") } },
			{ TEXT("YUG"), { TEXT("Razumem!"), TEXT("Na zapovest!"), TEXT("Napred!") } },
			{ TEXT("BUL"), { TEXT("\u0421\u043B\u0443\u0448\u0430\u043C!"), TEXT("\u0422\u044A\u0439 \u0432\u044F\u0440\u043D\u043E!"), TEXT("\u041D\u0430\u043F\u0440\u0435\u0434!") } },
			{ TEXT("GRE"), { TEXT("\u039C\u03AC\u03BB\u03B9\u03C3\u03C4\u03B1!"), TEXT("\u0394\u03B9\u03B1\u03C4\u03AC\u03BE\u03C4\u03B5!"), TEXT("\u0395\u03BC\u03C0\u03C1\u03CC\u03C2!") } },
			{ TEXT("TUR"), { TEXT("Emredersiniz!"), TEXT("Ba\u015F\u00FCst\u00FCne!"), TEXT("\u0130leri!") } },
			{ TEXT("ESP"), { TEXT("\u00A1A la orden!"), TEXT("\u00A1S\u00ED, mi general!"), TEXT("\u00A1Adelante!") } },
			{ TEXT("POR"), { TEXT("\u00C0s ordens!"), TEXT("Sim, meu general!"), TEXT("Em frente!") } },
		};
		const FString Tag = View.Human;
		const FAnswers* Mine = nullptr;
		for (const FAnswers& Each : Answers)
		{
			if (Tag == Each.Tag)
			{
				Mine = &Each;
				break;
			}
		}
		static int32 Turn = 0;
		const FString Answer = Mine ? Mine->Said[Turn++ % 3] : TEXT("Ready.");
		ArmySaid = FText::FromString(FString::Printf(TEXT("\u201C%s\u201D"), *Answer));
		// And aloud, in a voice of its own country where the computer has one. Armies do not chatter: one answer,
		// then quiet for a few seconds; and never over a newsreel.
		if (!Newsreel.bOn && FPlatformTime::Seconds() - AnsweredAt > 3.0)
		{
			AnsweredAt = FPlatformTime::Seconds();
			Speak(Answer, VoiceOfNation(Tag), 190, 1);
		}
		ArmySaidUntil = FPlatformTime::Seconds() + (bScripted ? 30.0 : 2.6); // (held while pictures are taken, so that it is in them)
	}
	return true;
}

void ACampaignPlayerController::PutDownArmy()
{
	if (ArmyRegion == INDEX_NONE)
	{
		return;
	}
	ArmyRegion = INDEX_NONE;
	PickedCorps.Reset();
	RefreshOrders();
}

void ACampaignPlayerController::ToggleCorps(int32 CorpsId)
{
	if (ArmyRegion == INDEX_NONE)
	{
		return;
	}
	if (PickedCorps.Contains(CorpsId))
	{
		PickedCorps.Remove(CorpsId);
	}
	else
	{
		PickedCorps.Add(CorpsId);
	}
	RefreshOrders();
}

const FReachRegion* ACampaignPlayerController::FindReach(int32 RegionId) const
{
	return Reach.FindByPredicate([RegionId](const FReachRegion& R) { return R.Region == RegionId; });
}

void ACampaignPlayerController::RefreshOrders()
{
	FCampaignSession* Session = GetSession();
	++Tray.Revision;
	Tray.bVisible = false;
	if (Map.IsValid() && ArmyRegion == INDEX_NONE)
	{
		Map->SetShowpiece(FString()); // nothing in hand: the studio's camera rests
		ShowCorps = INDEX_NONE;
	}
	Tray.Cards.Reset();
	Tray.Ships.Reset();
	Tray.bFleet = false;
	Tray.bLook = false;
	Reach.Reset();
	RouteTarget = INDEX_NONE;
	PendingTarget = INDEX_NONE;
	Tray.PendingButton = FText::GetEmpty();

	if (RingedPiece.IsValid())
	{
		RingedPiece->SetSelected(false);
	}
	RingedPiece.Reset();

	if (!Session || !Map.IsValid())
	{
		return;
	}
	Map->SetRoute(FVector::ZeroVector, TArray<int32>());
	Map->ShowSeaReach(TArray<int32>(), INDEX_NONE, INDEX_NONE);

	if (ArmyRegion == INDEX_NONE)
	{
		bByRail = false;
		bBySea = false;
		Map->SetReach(Reach);
		if (FleetInHand != INDEX_NONE)
		{
			LookRegion = INDEX_NONE;
			LayOutFleet();
		}
		else if (LookRegion != INDEX_NONE)
		{
			LayOutLook();
		}
		return;
	}
	FleetInHand = INDEX_NONE; // an army in hand: the fleet is let go
	LookRegion = INDEX_NONE; // and nothing else is being looked at

	// The army in hand, as the rules see it now.
	const FCampaignView& View = Session->GetView();
	const FCampaignNation* Nation = Map->GetData().FindNation(View.Human);
	FString General;
	int32 Divisions = 0, CorpsCount = 0;
	TMap<FString, int32> Mix;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Region != ArmyRegion || Unit.Nation != View.Human)
		{
			continue;
		}
		FTrayCard Card;
		Card.CorpsId = Unit.Id;
		Card.Name = FText::FromString(Unit.Name);
		Card.Kind = FText::FromString(Unit.bInTown ? FCampaignMapData::CorpsKindLabel(Unit.Kind) + TEXT(", in the town") : FCampaignMapData::CorpsKindLabel(Unit.Kind));
		Card.Divisions = Unit.Divisions;
		Card.Strength = Unit.Strength;
		Card.KindId = Unit.Elite == TEXT("heavy") ? FString(TEXT("heavy")) : Unit.Kind;
		Card.Ally = Unit.Ally;
		Card.Morale = Unit.Cohesion;
		if (Unit.Morale >= 0.0f)
		{
			Card.bMood = true;
			Card.Morale = Unit.Morale;
			Card.Rest = Unit.Rest >= 0.0f ? Unit.Rest : Unit.Cohesion;
			Card.MoraleWord = Unit.MoraleWord;
			Card.RestWord = Unit.RestWord;
			Card.Trend = Unit.Trend;
			Card.bResting = Unit.bResting;
			Card.Service = Unit.Service;
			TArray<FString> Tip;
			Tip.Add(Unit.MoodLine);
			Tip.Add(Unit.RestLine);
			if (Unit.Why.Num() > 0)
			{
				Tip.Add(TEXT("What has moved its morale:"));
				Tip.Append(Unit.Why);
			}
			if (Unit.bResting)
			{
				Tip.Add(TEXT("If it neither marches nor fights this month it rests: it takes replacements faster, and its morale and rest rise."));
			}
			Card.MoodTip = FString::Join(Tip, TEXT("\n"));
		}
		Card.Parts = Unit.Parts;
		Card.Experience = Unit.Experience;
		Card.Moves = Unit.Moves;
		Card.MovesMax = Unit.MovesMax;
		Card.bCanOrder = Unit.bCanOrder;
		Card.bPicked = PickedCorps.Contains(Unit.Id);
		Card.Nickname = FText::FromString(Unit.Nickname);
		Card.Grade = FText::FromString(Unit.Grade.IsEmpty() ? FString() : FString::Printf(TEXT("%s, %d"), *Unit.Grade, Unit.Experience));
		Card.Honours = Unit.Honours;
		Tray.Cards.Add(Card);
		Divisions += Unit.Divisions;
		++CorpsCount;
		// What the army is made of, by its divisions: "4 armoured, 6 infantry".
		if (Unit.Parts.Num() > 0)
		{
			for (const FViewDivision& Part : Unit.Parts)
			{
				++Mix.FindOrAdd(Unit.Elite == TEXT("heavy") ? FString(TEXT("heavy tank")) : FCampaignMapData::CorpsKindLabel(Part.Kind));
			}
		}
		else
		{
			Mix.FindOrAdd(FCampaignMapData::CorpsKindLabel(Unit.Kind)) += Unit.Divisions;
		}
		if (General.IsEmpty() && !Unit.General.IsEmpty())
		{
			General = Unit.General;
		}
	}
	PickedCorps.RemoveAll([this](int32 Id) { return !Tray.Cards.ContainsByPredicate([Id](const FTrayCard& C) { return C.CorpsId == Id && C.bCanOrder; }); });
	if (Tray.Cards.Num() == 0)
	{
		ArmyRegion = INDEX_NONE;
		Map->SetReach(Reach);
		return;
	}

	const FString Adjective = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : View.Human;
	const FString Where = Map->GetData().Regions.IsValidIndex(ArmyRegion) ? Map->GetData().Regions[ArmyRegion].Name : FString();
	Tray.bVisible = true;
	Tray.Color = Nation ? FLinearColor(Nation->Color) : FLinearColor::Gray;
	Tray.Tag = View.Human;
	Tray.Title = FText::FromString(General.IsEmpty() ? FString::Printf(TEXT("%s army"), *Adjective) : General);
	Mix.ValueSort([](int32 A, int32 B) { return A > B; });
	TArray<FString> Made;
	for (const TPair<FString, int32>& Kind : Mix)
	{
		Made.Add(FString::Printf(TEXT("%d %s"), Kind.Value, *Kind.Key));
	}
	Tray.Subtitle = FText::FromString(FString::Printf(TEXT("%s army at %s  ·  %d corps  ·  %s division%s"),
		*Adjective, *Where, CorpsCount, Made.Num() > 0 ? *FString::Join(Made, TEXT(", ")) : *FString::FromInt(Divisions), Divisions == 1 ? TEXT("") : TEXT("s")));

	// The standing order they are under, if they all share one.
	Tray.Posture.Reset();
	bool bFirst = true;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (PickedCorps.Contains(Unit.Id))
		{
			Tray.Posture = bFirst ? Unit.Posture : (Tray.Posture == Unit.Posture ? Tray.Posture : FString());
			bFirst = false;
		}
	}
	Tray.bByRail = bByRail;
	Tray.bBySea = bBySea;
	// Whether the picked corps keep their veterans together and take no drafts (all of them, or it is not lit).
	Tray.bLean = PickedCorps.Num() > 0;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (PickedCorps.Contains(Unit.Id) && !Unit.bLean)
		{
			Tray.bLean = false;
		}
	}
	// Allied corps standing beside the army that could be asked to march with it.
	Tray.JoinButton = FText::GetEmpty();
	if (const FString* Could = View.Joins.Find(ArmyRegion))
	{
		Tray.JoinButton = FText::FromString(FString::Printf(TEXT("ASK %s CORPS TO JOIN"), *Could->ToUpper()));
	}
	// The model of one of its corps stands beside the cards: the one last pointed at, or the first.
	{
		const bool bStill = Tray.Cards.ContainsByPredicate([this](const FTrayCard& Card) { return Card.CorpsId == ShowCorps; });
		const int32 Wanted = bStill ? ShowCorps : (Tray.Cards.Num() > 0 ? Tray.Cards[0].CorpsId : INDEX_NONE);
		ShowCorps = INDEX_NONE; // so that it is set afresh
		ShowCorpsModel(Wanted);
	}

	if (bBySea)
	{
		TArray<int32> Ports, Beaches;
		Session->SeaReach(PickedCorps, Ports, Beaches);
		for (int32 RegionId : Ports)
		{
			FReachRegion Entry;
			Entry.Region = RegionId;
			Entry.Previous = ArmyRegion;
			Reach.Add(Entry);
		}
		for (int32 RegionId : Beaches)
		{
			FReachRegion Entry;
			Entry.Region = RegionId;
			Entry.Previous = ArmyRegion;
			Entry.bAttack = true; // a landing
			Reach.Add(Entry);
		}
	}
	else if (bByRail)
	{
		for (int32 RegionId : Session->RailReach(PickedCorps))
		{
			FReachRegion Entry;
			Entry.Region = RegionId;
			Entry.Previous = ArmyRegion;
			Reach.Add(Entry);
		}
	}
	else
	{
		Reach = Session->Reach(PickedCorps);
	}
	Map->SetReach(Reach, bByRail || bBySea);

	RingedPiece = Map->FindPiece(ArmyRegion, View.Human);
	if (RingedPiece.IsValid())
	{
		RingedPiece->SetSelected(true);
	}
	SetPending(INDEX_NONE); // sets the line that says what to do next
	--Tray.Revision; // already counted above
}

void ACampaignPlayerController::ShowRouteTo(int32 RegionId)
{
	if (RegionId == RouteTarget || !Map.IsValid())
	{
		return;
	}
	RouteTarget = RegionId;
	if (bByRail)
	{
		Map->SetRoute(FVector::ZeroVector, TArray<int32>());
		return;
	}
	TArray<int32> Road;
	// Follow the way back from the region pointed at to where the army stands.
	for (const FReachRegion* Step = FindReach(RegionId); Step && Road.Num() < 32; Step = FindReach(Step->Previous))
	{
		Road.Insert(Step->Region, 0);
		if (Step->Previous == ArmyRegion)
		{
			break;
		}
	}
	const FVector From = RingedPiece.IsValid() ? RingedPiece->GetRestLocation() : FVector::ZeroVector;
	const FReachRegion* Ends = FindReach(RegionId);
	Map->SetRoute(From, RingedPiece.IsValid() ? Road : TArray<int32>(), Ends && Ends->IsWar() ? 2 : Ends && Ends->bAttack ? 1 : 0);
}

void ACampaignPlayerController::OrderTo(int32 RegionId, const FVector* CarriedFrom)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || RegionId == ArmyRegion)
	{
		return;
	}
	if (PickedCorps.Num() == 0)
	{
		Notify(TEXT("Every corps of this army has been told to stay behind."));
		return;
	}
	const FReachRegion* Target = FindReach(RegionId);
	if (bBySea && Target)
	{
		if (Target->bAttack)
		{
			// A landing: say what it will meet, and ask.
			const FLandingPlan Plan = Session->PlanLanding(PickedCorps, RegionId);
			if (!Plan.bOk)
			{
				Notify(Plan.Why.IsEmpty() ? TEXT("They cannot land there.") : Plan.Why, 5.0);
				return;
			}
			WarPrompt = FWarPrompt();
			WarPrompt.bVisible = true;
			WarPrompt.bLanding = true;
			WarPrompt.Region = RegionId;
			WarPrompt.Tag = Session->GetView().Human;
			WarPrompt.Title = FText::FromString(FString::Printf(TEXT("Land at %s?"), *Plan.Name));
			WarPrompt.Body = FText::FromString(FString::Join(Plan.Lines, TEXT("\n")) + TEXT("\n\nOnce they sail it cannot be taken back."));
			WarPrompt.Confirm = FText::FromString(TEXT("SEND THEM IN"));
			return;
		}
		const TArray<FCorpsMove> Convoys = Session->Ship(PickedCorps, RegionId);
		if (Convoys.Num() == 0)
		{
			Notify(TEXT("No corps could sail: there is no room there, or no shipping left this month."));
			return;
		}
		ShowWar(Session->GetView(), Convoys, CarriedFrom);
		CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
		CampaignSounds::Play(this, TEXT("ship_horn"), 0.4f);
		PickedCorps.Reset();
		for (const FCorpsMove& Convoy : Convoys)
		{
			PickedCorps.Add(Convoy.CorpsId);
		}
		ArmyRegion = RegionId;
		bBySea = false;
		RefreshOrders();
		SelectRegion(RegionId);
		return;
	}
	if ((!Target || Target->IsWar()) && OfferWar(RegionId))
	{
		return;
	}
	if (Target && Target->IsWar())
	{
		Notify(TEXT("War cannot be declared on them this month."));
		return;
	}
	if (!Target)
	{
		Notify(bByRail ? TEXT("The railway does not reach there through friendly ground, or the corps have already used their month.") : TEXT("They cannot get there this month."));
		return;
	}
	if (bByRail)
	{
		const TArray<FCorpsMove> Trains = Session->Rail(PickedCorps, RegionId);
		if (Trains.Num() == 0)
		{
			Notify(TEXT("No corps could go: there is no room there, or they have already used their month."));
			return;
		}
		ShowWar(Session->GetView(), Trains, CarriedFrom);
		CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
		CampaignSounds::Play(this, TEXT("truck_engine"), 0.25f, 0.8f);
		PickedCorps.Reset();
		for (const FCorpsMove& Train : Trains)
		{
			PickedCorps.Add(Train.CorpsId);
		}
		ArmyRegion = RegionId;
		bByRail = false;
		RefreshOrders();
		SelectRegion(RegionId);
		return;
	}
	if (Target->bAttack)
	{
		AdviserSaw(1);
		OpenFaceOff(RegionId);
		return;
	}

	AdviserSaw(1);
	const TArray<FCorpsMove> Marches = Session->Move(PickedCorps, RegionId);
	if (Marches.Num() == 0)
	{
		Notify(TEXT("They cannot get there this month."));
		return;
	}
	ShowWar(Session->GetView(), Marches, CarriedFrom);
	CampaignSounds::Play(this, TEXT("stamp"), 0.7f);

	// The army in hand is now the corps that marched, where they have arrived.
	PickedCorps.Reset();
	for (const FCorpsMove& March : Marches)
	{
		PickedCorps.Add(March.CorpsId);
	}
	ArmyRegion = RegionId;
	RefreshOrders();
	SelectRegion(RegionId);
}

bool ACampaignPlayerController::OfferWar(int32 RegionId, bool bMarching)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return false;
	}
	const FCampaignView& View = Session->GetView();
	const FString Tag = View.RegionHolder.IsValidIndex(RegionId) ? View.RegionHolder[RegionId] : FString();
	// An army in hand that stands on the frontier goes on there once war is declared.
	const FReachRegion* Across = bMarching && ArmyRegion != INDEX_NONE ? FindReach(RegionId) : nullptr;
	return OfferWarOn(Tag, Across && Across->IsWar() ? RegionId : INDEX_NONE);
}

bool ACampaignPlayerController::OfferWarOn(const FString& Tag, int32 Region)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return false;
	}
	const FCampaignView& View = Session->GetView();
	if (Tag.IsEmpty() || Tag == View.Human || !View.Declarable.Contains(Tag))
	{
		return false;
	}
	const FCampaignNation* Nation = Map->GetData().FindNation(Tag);
	const FString Name = Nation ? Nation->Name : Tag;
	WarPrompt = FWarPrompt();
	WarPrompt.bVisible = true;
	WarPrompt.Tag = Tag;
	WarPrompt.Region = Region;
	WarPrompt.Title = FText::FromString(FString::Printf(TEXT("Declare war on %s?"), *Name));
	// What it would mean, as the rules foresee it: where the country stands, its friends, and what follows.
	const FWarPlan Plan = Session->PlanWar(Tag, WarPrompt.Region);
	if (Plan.bOk)
	{
		WarPrompt.Name = Plan.Name;
		WarPrompt.Title = FText::FromString(FString::Printf(TEXT("Declare war on %s?"), *Plan.Name));
		FString Body = Plan.Lead + TEXT("\n\n") + Plan.Friends + TEXT("\n\nWhat follows if you say yes:");
		for (const FString& Line : Plan.Lines)
		{
			Body += TEXT("\n\u2022  ") + Line;
		}
		Body += Region != INDEX_NONE ? TEXT("\n\nThis cannot be taken back. Say no, and your army stays where it is.") : TEXT("\n\nThis cannot be taken back.");
		WarPrompt.Body = FText::FromString(Body);
	}
	else
	{
		WarPrompt.Body = FText::FromString(FString::Printf(
			TEXT("%s is at peace with you. If you declare war, its army will fight you from this month, and it will throw in its lot with your enemies. ")
			TEXT("Your armies on its border can attack as soon as war is declared.\n\nThis cannot be taken back."), *Name));
	}
	if (WarPrompt.Region != INDEX_NONE)
	{
		WarPrompt.Confirm = FText::FromString(TEXT("DECLARE WAR AND MARCH"));
	}
	return true;
}

void ACampaignPlayerController::ConfirmWar()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !WarPrompt.bVisible)
	{
		return;
	}
	const FWarPrompt Asked = WarPrompt;
	WarPrompt = FWarPrompt();
	if (Asked.bLanding)
	{
		// The landing goes in.
		CampaignSounds::Play(this, TEXT("ship_guns"), 0.6f);
		CampaignSounds::PlayLater(this, TEXT("battle"), 0.8f, 0.7f);
		const FLandingResult Result = Session->Land(PickedCorps, Asked.Region);
		if (Map.IsValid() && Result.bOk)
		{
			Map->AddBattleSmoke(Asked.Region);
		}
		ShowWar(Session->GetView(), Result.Moved);
		if (Result.bOk)
		{
			CampaignSounds::PlayLater(this, Result.bTaken ? TEXT("victory") : TEXT("defeat"), 3.2f, 0.6f);
		}
		if (Result.Moved.Num() > 0)
		{
			PickedCorps.Reset();
			for (const FCorpsMove& Ashore : Result.Moved)
			{
				PickedCorps.Add(Ashore.CorpsId);
			}
			ArmyRegion = Asked.Region;
		}
		bBySea = false;
		RefreshOrders();
		SelectRegion(Result.bTaken ? Asked.Region : ArmyRegion);
		Notify(Result.Note, 9.0);
		return;
	}
	const FCampaignNation* Nation = Map.IsValid() ? Map->GetData().FindNation(Asked.Tag) : nullptr;
	const FString Name = !Asked.Name.IsEmpty() ? Asked.Name : Nation ? Nation->Name : Asked.Tag;
	if (!Session->Declare(Asked.Tag))
	{
		Notify(FString::Printf(TEXT("War could not be declared on %s this month."), *Name));
		return;
	}
	// The table as it stands now: their ground is enemy ground, and the army in hand may be able to reach it.
	ShowWar(Session->GetView(), TArray<FCorpsMove>());
	RefreshOrders();
	Notify(FString::Printf(TEXT("War is declared on %s."), *Name), 6.0);
	CampaignSounds::Play(this, TEXT("siren"), 0.5f);
	const FReachRegion* Onward = Asked.Region != INDEX_NONE && ArmyRegion != INDEX_NONE ? FindReach(Asked.Region) : nullptr;
	if (Onward && !Onward->IsWar())
	{
		// The army that was being sent there goes on: against a defended place that means the face-off.
		OrderTo(Asked.Region);
	}
	if (!FaceOff.bVisible)
	{
		RefreshRegionPanel(SelectedRegion);
	}
	if (Asked.bFromOffice && !FaceOff.bVisible)
	{
		OpenDiplomacy();
	}
}

void ACampaignPlayerController::CloseWarPrompt()
{
	const bool bBack = WarPrompt.bFromOffice;
	WarPrompt = FWarPrompt();
	if (bBack)
	{
		OpenDiplomacy();
	}
}

void ACampaignPlayerController::ShowWar(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom)
{
	if (Map.IsValid())
	{
		if (!Map->HasLand())
		{
			// The rivers and railways the rules know, laid on the table the first time the war is shown.
			if (FCampaignSession* Session = GetSession())
			{
				Map->SetLand(Session->Land());
			}
		}
		if (!Map->HasTowns())
		{
			if (FCampaignSession* Session = GetSession())
			{
				Map->SetTowns(Session->Towns());
			}
		}
		Map->SetTownPlots(View.PageIds.Contains(TEXT("towns"))); // (the cities keep ground open for their owner's buildings only where the rules have such)
		Map->SetHolders(View.RegionHolder, View.League);
		Map->SetSeason(View.Turn);
		Map->ShowArmies(View, Marches, CarriedFrom);
		Map->ShowWorks(View.BridgesDown, View.RailsCut);
		Map->ShowCivic(View.Civic);
		Map->ShowScars(View.Scars);
		Map->ShowSieges(View.Sieges);
		Map->SetWarAtNight(View.Blackout, View.Fronts);
		Map->ShowPrizes(View.Prizes);
		Map->ShowBuildings(View.RegionWorks);
		Map->ShowAirfieldSigns(View.Airfields);
		Map->ShowLanes(View.Lanes);
		Map->ShowFreeCapitals(View.FreeCapitals);
		Map->ShowShroud(View.Unseen);
		Map->ShowMud(View.bMud ? View.WeatherGround : TArray<int32>());
		if (Map->AreSidesShown())
		{
			Map->ShowSides(true, View.RegionStanding); // ground that has changed sides changes colour
		}
	}
}

namespace
{
	FString Thousands(int32 Number)
	{
		return FText::AsNumber(Number).ToString();
	}

	/** The corps of one side of a battle, a line each. After the battle the line says what it cost and what became of them. */
	FString ListUnits(const TArray<FBattleUnit>& Units, bool bAfter)
	{
		TArray<FString> Lines;
		for (const FBattleUnit& Unit : Units)
		{
			// A corps not identified has no arm and no numbers to give.
			FString Line = Unit.Kind.IsEmpty() ? Unit.Name : FString::Printf(TEXT("%s  \u00B7  %s, %d division%s"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind),
				Unit.Divisions, Unit.Divisions == 1 ? TEXT("") : TEXT("s"));
			if (bAfter)
			{
				Line += FString::Printf(TEXT("  \u00B7  lost %s"), *Thousands(Unit.Lost));
				if (!Unit.Fate.IsEmpty())
				{
					Line += FString::Printf(TEXT(", %s"), *Unit.Fate);
				}
			}
			else if (Unit.Men > 0)
			{
				Line += FString::Printf(TEXT("  \u00B7  %s men"), *Thousands(Unit.Men));
			}
			Lines.Add(Line);
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString SeniorGeneral(const TArray<FBattleUnit>& Units)
	{
		for (const FBattleUnit& Unit : Units)
		{
			if (!Unit.General.IsEmpty())
			{
				return Unit.General;
			}
		}
		return FString();
	}

	FString StrengthLine(const TArray<FBattleUnit>& Units)
	{
		int32 Divisions = 0, Men = 0, Unknown = 0;
		for (const FBattleUnit& Unit : Units)
		{
			Divisions += Unit.Divisions;
			Men += Unit.Men;
			Unknown += Unit.Kind.IsEmpty() || Unit.Men <= 0 ? 1 : 0;
		}
		if (Unknown > 0)
		{
			return FString::Printf(TEXT("%d corps  \u00B7  strength not known"), Units.Num());
		}
		return FString::Printf(TEXT("%d corps  \u00B7  %d division%s  \u00B7  %s men"), Units.Num(), Divisions, Divisions == 1 ? TEXT("") : TEXT("s"), *Thousands(Men));
	}
}

void ACampaignPlayerController::OpenFaceOff(int32 RegionId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const FAttackPreview Preview = Session->PreviewAttack(PickedCorps, RegionId);
	if (Preview.bValid && Preview.Attackers.Num() > 0)
	{
		CampaignSounds::Play(this, TEXT("drums"), 0.6f); // the two sides are drawn up
	}
	if (!Preview.Refused.IsEmpty())
	{
		Notify(Preview.Refused, 8.0); // they stand on a neutral's ground: war must be declared on it first
		return;
	}
	if (!Preview.bValid || Preview.Attackers.Num() == 0)
	{
		Notify(TEXT("None of the corps in hand stands next to that ground with a move left to attack it."));
		return;
	}
	const FCampaignMapData& Data = Map->GetData();
	const FCampaignView& View = Session->GetView();
	const FCampaignNation* Ours = Data.FindNation(View.Human);
	const FString TheirTag = Preview.Defenders.Num() > 0 ? Preview.Defenders[0].Nation : (View.RegionHolder.IsValidIndex(RegionId) ? View.RegionHolder[RegionId] : FString());
	const FCampaignNation* Theirs = Data.FindNation(TheirTag);
	const FString Place = Data.Regions.IsValidIndex(RegionId) ? Data.Regions[RegionId].Name : FString();

	FaceOff = FFaceOff();
	FaceOff.bVisible = true;
	FaceOff.Region = RegionId;
	FaceOff.Title = FText::FromString(FString::Printf(TEXT("The attack on %s"), *Place));
	{
		// An army with allied corps marching in it is named for all of them: "Germany and Slovakia".
		FString Named = Ours ? Ours->Name : View.Human;
		for (int32 Index = 0; Index < Preview.Allies.Num(); ++Index)
		{
			Named += (Index == Preview.Allies.Num() - 1 ? TEXT(" and ") : TEXT(", ")) + Preview.Allies[Index];
		}
		FaceOff.OurName = FText::FromString(Named);
	}
	FaceOff.TheirName = FText::FromString(Theirs ? Theirs->Name : TheirTag);
	FaceOff.OurColor = Ours ? FLinearColor(Ours->Color) : FLinearColor::Gray;
	FaceOff.TheirColor = Theirs ? FLinearColor(Theirs->Color) : FLinearColor::Gray;
	FaceOff.OurTag = View.Human;
	FaceOff.TheirTag = TheirTag;
	const FString OurGeneral = SeniorGeneral(Preview.Attackers), TheirGeneral = SeniorGeneral(Preview.Defenders);
	FaceOff.OurGeneral = FText::FromString(OurGeneral.IsEmpty() ? FString(TEXT("No general named")) : OurGeneral);
	FaceOff.TheirGeneral = FText::FromString(TheirGeneral.IsEmpty() ? FString(TEXT("No general named")) : TheirGeneral);
	FaceOff.OurStrength = FText::FromString(StrengthLine(Preview.Attackers));
	FaceOff.TheirStrength = FText::FromString(StrengthLine(Preview.Defenders));
	FaceOff.OurUnits = FText::FromString(ListUnits(Preview.Attackers, false));
	FaceOff.TheirUnits = FText::FromString(ListUnits(Preview.Defenders, false));
	FaceOff.Balance = Preview.Attack + Preview.Defence > 0.0f ? Preview.Attack / (Preview.Attack + Preview.Defence) : 0.5f;
	FaceOff.Prize = FText::FromString(Preview.Prize);
	if (Preview.bExact)
	{
		FString Odds = Preview.Odds >= 1.0f
			? FString::Printf(TEXT("Odds of %.1f to 1 in your favour"), Preview.Odds)
			: FString::Printf(TEXT("Odds of %.1f to 1 against you"), 1.0f / FMath::Max(Preview.Odds, 0.01f));
		// (the chance is printed as 100 per cent only when the attack cannot fail)
		if (Preview.ChancePct >= 0)
		{
			Odds += FString::Printf(TEXT("  \u00B7  about %d%% to take %s"), Preview.ChancePct, *Place);
		}
		FaceOff.OddsLine = FText::FromString(Odds);
	}
	else
	{
		// The defenders are not all identified: the staff give the odds as a band that holds the truth.
		auto Say = [](float Odds) { return Odds >= 1.0f ? FString::Printf(TEXT("%.1f to 1 for you"), Odds) : FString::Printf(TEXT("%.1f to 1 against"), 1.0f / FMath::Max(Odds, 0.01f)); };
		FString Odds = FString::Printf(TEXT("The staff's estimate: between %s and %s"), *Say(Preview.OddsLow), *Say(Preview.OddsHigh));
		if (Preview.ChancePctLow >= 0 && Preview.ChancePctHigh >= 0)
		{
			Odds += Preview.ChancePctLow == Preview.ChancePctHigh
				? FString::Printf(TEXT("  \u00B7  about %d%% to take %s"), Preview.ChancePctLow, *Place)
				: FString::Printf(TEXT("  \u00B7  %d to %d%% to take %s"), Preview.ChancePctLow, Preview.ChancePctHigh, *Place);
		}
		FaceOff.OddsLine = FText::FromString(Odds);
		if (!Preview.Note.IsEmpty())
		{
			FaceOff.Prize = FText::FromString(Preview.Note + (Preview.Prize.IsEmpty() ? FString() : TEXT("\n") + Preview.Prize));
		}
	}

	// The Battle Ticket: what the campaign map says about this fight, and what each thing does in it.
	TArray<FString> Lines;
	for (const FTicketLine& Line : Preview.Lines)
	{
		Lines.Add(Line.Effect.IsEmpty()
			? FString::Printf(TEXT("%s:  %s"), *Line.Label, *Line.Value)
			: FString::Printf(TEXT("%s:  %s  \u2014  %s"), *Line.Label, *Line.Value, *Line.Effect));
	}
	FaceOff.Ticket = FText::FromString(FString::Join(Lines, TEXT("\n")));
	// Where more corps stand in reach than were picked: what the attack could be, and a button to make it so.
	FaceOff.Mass = FText::FromString(Preview.Mass);
	FaceOff.MassIds = Preview.MassIds;
}

void ACampaignPlayerController::SendEveryone()
{
	if (!FaceOff.bVisible || FaceOff.bResolved || FaceOff.MassIds.Num() == 0)
	{
		return;
	}
	// The corps in hand are remembered, and taken up again if the attack is called off.
	if (PickedBeforeMass.Num() == 0)
	{
		PickedBeforeMass = PickedCorps;
	}
	const int32 RegionId = FaceOff.Region;
	PickedCorps = FaceOff.MassIds;
	OpenFaceOff(RegionId);
}

void ACampaignPlayerController::ConfirmAttack()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !FaceOff.bVisible || FaceOff.bResolved)
	{
		return;
	}
	const int32 RegionId = FaceOff.Region;
	const FAttackResult Result = Session->Attack(PickedCorps, RegionId);
	PickedBeforeMass.Reset();
	if (!Result.bFought)
	{
		FaceOff.bVisible = false;
		Notify(TEXT("The attack could not go in."));
		return;
	}

	// The fight is seen and heard, and then how it went.
	Map->AddBattleSmoke(RegionId);
	CampaignSounds::Play(this, TEXT("battle"), 0.8f);
	if (!Result.bYielded)
	{
		CampaignSounds::PlayLater(this, TEXT("field_gun"), 0.9f, 0.5f);
	}
	CampaignSounds::PlayLater(this, Result.bTaken || Result.bSiege ? TEXT("victory") : TEXT("defeat"), 2.8f, 0.6f);

	FaceOff.bResolved = true;
	FaceOff.ResultTitle = FText::FromString(Result.bYielded ? FString(TEXT("They fell back without a fight")) : Result.Level);
	const FString Outcome = Result.bSiege ? TEXT("The country is yours, but the town holds. It is now besieged: starve it out, or pick the province and storm it.")
		: Result.bTaken ? TEXT("The ground is yours.") : TEXT("The attack was held. The ground is still theirs.");
	FString Cost = FString::Printf(TEXT("You lost %s men; they lost %s"), *Thousands(Result.AttackerLost), *Thousands(Result.DefenderLost));
	if (Result.Prisoners > 0)
	{
		Cost += FString::Printf(TEXT(", %s of them taken prisoner"), *Thousands(Result.Prisoners));
	}
	FaceOff.ResultDetail = FText::FromString(Outcome + TEXT("  ") + Cost + TEXT("."));
	FaceOff.OurUnits = FText::FromString(ListUnits(Result.Attackers, true));
	FaceOff.TheirUnits = FText::FromString(ListUnits(Result.Defenders, true));

	// Show the result on the table: ground repainted if it changed hands, and the corps that followed up carried in.
	ShowWar(Session->GetView(), Result.Moved);
	if (Result.Moved.Num() > 0)
	{
		PickedCorps.Reset();
		for (const FCorpsMove& March : Result.Moved)
		{
			PickedCorps.Add(March.CorpsId);
		}
		ArmyRegion = RegionId;
	}
	RefreshOrders();
	SelectRegion(RegionId);
}

void ACampaignPlayerController::CloseFaceOff()
{
	if (FaceOff.bVisible && FaceOff.bResolved && Map.IsValid())
	{
		BattleMusicUntil = FPlatformTime::Seconds() + 6.0; // the march plays on a little after the result
		// The result has been read: the field is still smoking when the eye goes back to the table.
		Map->AddBattleSmoke(FaceOff.Region);
	}
	if (FaceOff.bVisible && !FaceOff.bResolved && PickedBeforeMass.Num() > 0)
	{
		PickedCorps = PickedBeforeMass; // called off after "send everyone": the corps that were in hand are in hand again
	}
	PickedBeforeMass.Reset();
	FaceOff.bVisible = false;
}

void ACampaignPlayerController::EndMonth()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || bWatching || FaceOff.bVisible || Opening.bVisible)
	{
		return;
	}
	// With a newsreel running or a great moment on the screen, the key that ends the month goes on to the next
	// thing instead: it must not end another month unseen.
	if (Newsreel.bOn || Announcements.Num() > 0)
	{
		if (!bScripted)
		{
			if (Newsreel.bOn)
			{
				EndNewsreel();
			}
			else
			{
				DismissAnnouncement();
			}
			return;
		}
		EndNewsreel();
		Announcements.Reset();
	}
	PutDownArmy();
	PutDownFleet();
	ClosePage();
	AdviserSaw(4);
	CampaignSounds::Play(this, TEXT("bell"), 0.5f);
	// Where every corps stands as the month ends, so that the others' marches can be seen from there.
	WatchStood.Reset();
	for (const FViewCorps& Unit : Session->GetView().Corps)
	{
		WatchStood.Add(Unit.Id, Unit.Region);
	}
	Month = Session->EndMonth();
	if (!bHelpSeen)
	{
		bHelpSeen = true; // a month has been played: the reminder of the keys folds away into its mark
		SaveOptions();
	}
	WatchIndex = 0;
	if (const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		WatchReturnFocus = Eye->GetFocus();
		WatchReturnZoom = Eye->GetZoom();
	}
	if (Month.Steps.Num() > 0 && (!bScripted || bAlwaysWatch))
	{
		// Other nations did things: go and look at each in turn, quickly.
		bWatching = true;
		WatchNext = 0.0;
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			Eye->SetHurry(true);
		}
	}
	else
	{
		FinishMonth();
	}
}

void ACampaignPlayerController::EndMonthAndWatch()
{
	bAlwaysWatch = true;
	EndMonth();
	bAlwaysWatch = false;
}

void ACampaignPlayerController::WatchNextStep()
{
	// Unless he has asked to watch everything (V), the player is shown only the moves that touch him; the rest
	// have happened all the same, and are in the month's news.
	while (!bWatchAll && Month.Steps.IsValidIndex(WatchIndex) && !Month.Steps[WatchIndex].bMine)
	{
		++WatchIndex;
	}
	if (!Month.Steps.IsValidIndex(WatchIndex))
	{
		FinishMonth();
		return;
	}
	const FMonthStep& Step = Month.Steps[WatchIndex++];
	// The armies are seen to march: every corps that stands somewhere new since the last step goes there on the table.
	TArray<FCorpsMove> Marches;
	for (const FViewCorps& Unit : Step.View.Corps)
	{
		const int32* Was = WatchStood.Find(Unit.Id);
		if (Was && *Was != Unit.Region)
		{
			FCorpsMove& March = Marches.AddDefaulted_GetRef();
			March.CorpsId = Unit.Id;
			March.Path.Add(Unit.Region);
		}
	}
	WatchStood.Reset();
	for (const FViewCorps& Unit : Step.View.Corps)
	{
		WatchStood.Add(Unit.Id, Unit.Region);
	}
	ShowWar(Step.View, Marches);
	// Where there was fighting, the field smokes.
	for (int32 RegionId : Step.Battles)
	{
		Map->AddBattleSmoke(RegionId);
	}
	if (Step.Battles.Num() > 0 && !Step.bMine)
	{
		CampaignSounds::Play(this, TEXT("field_gun"), 0.25f);
	}
	if (Step.Battles.Num() > 0 || Step.Raids.Num() > 0 || Step.SeaFights.Num() > 0)
	{
		BattleMusicUntil = FPlatformTime::Seconds() + (Step.bMine ? 9.0 : 5.0);
	}
	// A coast shelled from the sea smokes, and the guns are heard.
	for (int32 Coast : Step.Shelled)
	{
		Map->AddBattleSmoke(Coast);
	}
	if (Step.Shelled.Num() > 0)
	{
		CampaignSounds::Play(this, TEXT("ship_guns"), Step.bMine ? 0.5f : 0.3f);
		BattleMusicUntil = FPlatformTime::Seconds() + (Step.bMine ? 8.0 : 4.0);
	}
	// Where fleets fought, the guns are seen and heard over the water.
	for (int32 Zone : Step.SeaFights)
	{
		Map->AddSeaBattle(Zone);
	}
	if (Step.SeaFights.Num() > 0)
	{
		CampaignSounds::Play(this, TEXT("ship_guns"), Step.bMine ? 0.5f : 0.3f);
	}
	// The raids of the month are flown again, each group from its own airfield.
	for (const FSortie& Raid : Step.Raids)
	{
		Map->FlySortie(Raid);
	}
	if (Step.Raids.Num() > 0)
	{
		CampaignSounds::Play(this, Step.Raids[0].bBombers ? TEXT("bomber_drone") : TEXT("fighter_pass"), 0.35f);
	}

	// Travel to where it happened.
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		FVector2D Middle = FVector2D::ZeroVector;
		int32 Count = 0;
		for (int32 RegionId : Step.Focus)
		{
			if (Map->GetData().Regions.IsValidIndex(RegionId))
			{
				Middle += Map->GetData().Regions[RegionId].Center;
				++Count;
			}
		}
		if (Count == 0)
		{
			// Nothing on land: a fight at sea is looked at where it was.
			for (int32 Zone : Step.SeaFights)
			{
				if (Map->GetData().Seas.IsValidIndex(Zone))
				{
					Middle += Map->GetData().Seas[Zone].Center;
					++Count;
				}
			}
		}
		if (Count > 0)
		{
			Middle /= Count;
			Eye->GoTo(Map->MapToWorld(Middle.X, Middle.Y), FMath::Clamp(WatchReturnZoom, 0.42f, 0.6f));
		}
	}
	// Brian asked for this to go by very fast: about half a second a nation, a little longer when it is
	// the player's own ground or troops that were hit.
	if (Step.bMine)
	{
		CampaignSounds::Play(this, TEXT("explosion"), 0.3f);
	}
	// (a raid takes its time to fly: long enough to see it reach the target when it is the player's sky)
	const double Pause = Step.Raids.Num() > 0 ? (Step.bMine ? 6.0 : 2.2) : Step.SeaFights.Num() > 0 && Step.Battles.Num() == 0 ? (Step.bMine ? 2.4 : 1.4) : Step.bMine ? 1.3 : Step.bMarchOnly ? 0.55 : 0.8;
	Notify(Step.Note, Pause + 0.3);
	WatchNext = FPlatformTime::Seconds() + Pause;
}

void ACampaignPlayerController::SkipWatching()
{
	if (bWatching)
	{
		FinishMonth();
	}
}

void ACampaignPlayerController::FinishMonth()
{
	FCampaignSession* Session = GetSession();
	bWatching = false;
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->SetHurry(false);
	}
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const FCampaignView& View = Session->GetView();
	ShowWar(View, TArray<FCorpsMove>());
	if (Month.Steps.Num() > 0)
	{
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			Eye->GoTo(WatchReturnFocus, WatchReturnZoom);
		}
	}
	RefreshOrders();
	RefreshRegionPanel(Map->GetSelectedRegion());

	// The campaign is kept every month, so it can be continued another day.
	if (ACampaignGameMode* Mode = GetMode())
	{
		Mode->SaveCampaign();
	}

	// The great moments of the month are told one at a time, each on a screen of its own.
	Announcements = Month.Announcements;
	// (not while pictures are being taken: the newsreel has a picture of its own)
	const bool bReel = bNewsreels && !bScripted && Month.Reel.Num() > 0 && Month.Fallen.IsEmpty();
	if (Announcements.Num() > 0 && !bReel)
	{
		CampaignSounds::PlayLater(this, Announcements[0].bGood ? TEXT("victory") : TEXT("defeat"), 0.4f, 0.55f);
	}

	// A January opens with the year's title card; then the newsreel, if the month had stories worth one. The
	// great moments wait until both are over.
	LastYear = Month.Year.bValid ? Month.Year : LastYear;
	if (Month.Year.bValid && bBriefings && !bScripted && Month.Fallen.IsEmpty())
	{
		WaitingReel = bReel ? Month.Reel : TArray<FNewsCard>();
		WaitingReelDate = Month.ReelDate;
		PlayYearCard(Month.Year);
	}
	else if (bReel)
	{
		PlayNewsreel(Month.ReelDate, Month.Reel);
	}

	// The month's news stays up for a while.
	TArray<FString> Lines;
	for (int32 i = 0; i < Month.Headlines.Num() && i < 5; ++i)
	{
		// Each in a sentence or two: the rest is in the month's report (R).
		FString Line = Month.Headlines[i];
		if (Line.Len() > 150)
		{
			const int32 Stop = Line.Find(TEXT(". "), ESearchCase::CaseSensitive, ESearchDir::FromStart, 70);
			Line = Stop != INDEX_NONE && Stop < 170 ? Line.Left(Stop + 1) : Line.Left(150) + TEXT("...");
		}
		Lines.Add(Line);
	}
	if (Month.Headlines.Num() > 5)
	{
		Lines.Add(FString::Printf(TEXT("And %d more: press R for the month's report."), Month.Headlines.Num() - 5));
	}
	DispatchText = FString::Join(Lines, TEXT("\n\n"));
	DispatchUntil = FPlatformTime::Seconds() + 25.0;

	if (!Month.Fallen.IsEmpty())
	{
		const FCampaignNation* Nation = Map->GetData().FindNation(Month.Fallen);
		Notify(FString::Printf(TEXT("%s has fallen. The war goes on without you."), Nation ? *Nation->Name : *Month.Fallen), 12.0);
	}
	else
	{
		// A stock that has run dry, or will within six months, is said at once: it takes that long to do anything about it.
		FString Warning;
		if (View.bStores)
		{
			auto Warn = [&Warning](const FViewStock& Stock, const TCHAR* Word)
			{
				if (Stock.bShort || Stock.Months == 0)
				{
					Warning += FString::Printf(TEXT(" The %s depots are empty."), Word);
				}
				else if (Stock.Months > 0 && Stock.Months <= 6)
				{
					Warning += FString::Printf(TEXT(" %s runs out in %d month%s."), *FString(Word).Left(1).ToUpper().Append(FString(Word).Mid(1)), Stock.Months, Stock.Months == 1 ? TEXT("") : TEXT("s"));
				}
			};
			Warn(View.Oil, TEXT("oil"));
			Warn(View.Supplies, TEXT("supply"));
		}
		if (!Warning.IsEmpty())
		{
			CampaignSounds::Play(this, TEXT("siren"), 0.35f);
		}
		for (const FString& Alert : View.Alerts)
		{
			Warning += TEXT(" ") + Alert;
		}
		Notify(View.Human.IsEmpty() ? View.Date : FString::Printf(TEXT("%s. Your orders.%s"), *View.Date, *Warning), Warning.IsEmpty() ? 4.0 : 10.0);
	}
}

void ACampaignPlayerController::Notify(const FString& Text, double Seconds)
{
	NoticeText = Text;
	NoticeUntil = FPlatformTime::Seconds() + Seconds;
}

FText ACampaignPlayerController::GetDispatches() const
{
	return !bWatching && FPlatformTime::Seconds() < DispatchUntil ? FText::FromString(DispatchText) : FText::GetEmpty();
}

FText ACampaignPlayerController::GetNotice() const
{
	return FPlatformTime::Seconds() < NoticeUntil ? FText::FromString(NoticeText) : FText::GetEmpty();
}

FText ACampaignPlayerController::GetDateText() const
{
	const FCampaignSession* Session = GetSession();
	return FText::FromString(Session ? Session->GetView().Date : FString(TEXT("September 1939")));
}

FText ACampaignPlayerController::GetRulesNote() const
{
	const FCampaignSession* Session = GetSession();
	if (!Session)
	{
		const ACampaignGameMode* Mode = GetMode();
		const bool bReady = Mode && Mode->GetSession() && Mode->GetSession()->IsReady();
		return FText::FromString(bReady ? TEXT("Choose a campaign") : TEXT("The rules did not start"));
	}
	const FCampaignView& View = Session->GetView();
	if (View.Human.IsEmpty())
	{
		return FText::FromString(!View.Fallen.IsEmpty() ? TEXT("Your country has fallen. The war goes on without you.")
			: View.bOpen ? TEXT("Watching a war nobody has seen") : TEXT("Watching history unfold"));
	}
	const FCampaignNation* Nation = Map.IsValid() ? Map->GetData().FindNation(View.Human) : nullptr;
	FString Note = FString::Printf(TEXT("You lead %s"), Nation ? *Nation->Name : *View.Human);
	if (View.bOpen)
	{
		Note += View.Level.IsEmpty() ? FString(TEXT("  \u00B7  free play")) : TEXT("  \u00B7  ") + View.Level;
	}
	if (View.bEve)
	{
		Note += TEXT("  \u00B7  the eve of war: place your army, then end the month");
	}
	if (View.Rules == TEXT("stand-in"))
	{
		Note += TEXT("  \u00B7  stand-in rules: armies march, nothing fights yet");
	}
	return FText::FromString(Note);
}

bool ACampaignPlayerController::IsTownBarShown() const
{
	// (the bottom of the screen is the army's or the fleet's while one is in hand or being looked at)
	return TownBar.Plots.Num() > 0 && !Tray.bVisible && Map.IsValid() && Map->GetSelectedRegion() == TownBar.Region && !bWatching && !Newsreel.bOn && !Opening.bVisible;
}

void ACampaignPlayerController::PickTownPlot(int32 Index)
{
	TownBar.Picked = TownBar.Picked == Index || !TownBar.Plots.IsValidIndex(Index) ? INDEX_NONE : Index;
	++TownBar.Revision;
}

void ACampaignPlayerController::OrderTownBuilding(const FString& Line)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || TownBar.Region == INDEX_NONE)
	{
		return;
	}
	FString Note;
	const int32 Where = TownBar.Region;
	const bool bDone = Session->Act(FString::Printf(TEXT("civic:%d:%s"), Where, *Line), Where, Note);
	if (bDone)
	{
		// The builders are seen to start at once, at the town's edge, and heard.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
		CampaignSounds::PlayLater(this, TEXT("hammer"), 0.25f, 0.6f);
		TownBar.Picked = INDEX_NONE;
	}
	RefreshRegionPanel(Where);
	Notify(Note.IsEmpty() ? FString(TEXT("That cannot be built now.")) : Note, 5.0);
}

void ACampaignPlayerController::RefreshRegionPanel(int32 RegionId)
{
	RegionPanel = FRegionPanelInfo();
	RegionPanel.Revision = ++PanelRevision;
	{
		// The town's bar is written afresh with the panel; the plot picked on it is kept while the town is the same.
		const int32 Kept = TownBar.Region == RegionId ? TownBar.Picked : INDEX_NONE;
		const int32 Next = TownBar.Revision + 1;
		TownBar = FTownBar();
		TownBar.Revision = Next;
		TownBar.Picked = Kept;
	}
	if (!Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return;
	}
	const FCampaignMapData& Data = Map->GetData();
	const FCampaignRegion& Region = Data.Regions[RegionId];
	const FCampaignNation* Owner = Data.FindNation(Region.Owner);

	RegionPanel.bVisible = true;
	RegionPanel.Name = FText::FromString(Region.Name);
	RegionPanel.OwnerColor = Owner ? FLinearColor(Owner->Color) : FLinearColor::Gray;
	RegionPanel.OwnerTag = Region.Owner;

	FString OwnerLine = Owner ? Owner->Name : Region.Owner;
	if (const FCampaignSession* Session = GetSession())
	{
		const TArray<FString>& Standing = Session->GetView().RegionStanding;
		const FString Stand = Standing.IsValidIndex(RegionId) ? Standing[RegionId] : FString();
		OwnerLine += Stand == TEXT("own") ? TEXT("  \u00B7  Yours") : Stand == TEXT("ally") ? TEXT("  \u00B7  Ally")
			: Stand == TEXT("enemy") ? TEXT("  \u00B7  Enemy") : Stand == TEXT("neutral") ? TEXT("  \u00B7  Neutral") : TEXT("");
		// A country at peace with the player that he borders, or can reach by sea, is one he could go to war with.
		if (Session->GetView().Declarable.Contains(Region.Owner))
		{
			RegionPanel.DeclareTag = Region.Owner;
			RegionPanel.DiplomacyLine = FText::FromString(FString::Printf(
				TEXT("%s is at peace with you. Drag an army onto its ground, or press the button, and you will be asked whether to declare war."),
				Owner ? *Owner->Name : *Region.Owner));
		}
	}
	if (Region.bCapital)
	{
		OwnerLine += TEXT("  ·  Capital");
	}
	else if (Region.bColony)
	{
		OwnerLine += TEXT("  ·  Overseas territory");
	}
	RegionPanel.OwnerLine = FText::FromString(OwnerLine);

	TArray<FString> Lines;
	Lines.Add(FCampaignMapData::TerrainLabel(Region.Terrain));
	if (Region.Factories > 0)
	{
		Lines.Add(FString::Printf(TEXT("Industry %d"), Region.Factories));
	}
	if (Region.VictoryPoints > 0)
	{
		Lines.Add(FString::Printf(TEXT("Worth %d"), Region.VictoryPoints));
	}
	if (Region.bPort) Lines.Add(TEXT("Port"));
	if (Region.FortLevel > 0) Lines.Add(FString::Printf(TEXT("Fortified, level %d"), Region.FortLevel));
	if (Region.bOil) Lines.Add(TEXT("Oilfields"));
	if (Region.bOre) Lines.Add(TEXT("Iron ore"));
	if (Region.bDepot) Lines.Add(TEXT("Supply depot"));
	RegionPanel.Details = FText::FromString(FString::Join(Lines, TEXT("   ·   ")));

	// Who is standing here.
	TArray<FString> ForceLines;
	for (const AArmyPiece* Piece : Map->GetPieces())
	{
		if (!Piece || Piece->GetRegionId() != RegionId)
		{
			continue;
		}
		const FCampaignNation* Nation = Data.FindNation(Piece->GetNationTag());
		const FString Adjective = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : Piece->GetNationTag();
		FString Header = FString::Printf(TEXT("%s army, %d division%s"), *Adjective, Piece->GetDivisions(), Piece->GetDivisions() == 1 ? TEXT("") : TEXT("s"));
		if (!Piece->GetGeneral().IsEmpty())
		{
			Header += FString::Printf(TEXT(", under %s"), *Piece->GetGeneral());
		}
		ForceLines.Add(Header);
		// The player's own army is read from its cards when it is picked up; another's corps are listed here, a few of them.
		const FCampaignSession* Now = GetSession();
		if (Now && Piece->GetNationTag() == Now->GetView().Human)
		{
			continue;
		}
		int32 Listed = 0, Hidden = 0;
		for (const FViewCorps& Unit : Piece->GetCorps())
		{
			Hidden += Unit.bDim && Unit.bNear ? 1 : 0;
		}
		if (Hidden > 0)
		{
			// Corps in plain view that nobody has identified: their kind and strength are not known.
			ForceLines.Last() = FString::Printf(TEXT("%s army, %d corps: %s"), *Adjective, Piece->GetCorps().Num(),
				Hidden == Piece->GetCorps().Num() ? TEXT("none identified") : *FString::Printf(TEXT("%d not identified"), Hidden));
			ForceLines.Add(TEXT("      A battle, an agent or a reconnaissance flight would show what is there."));
		}
		for (const FViewCorps& Unit : Piece->GetCorps())
		{
			if (Unit.bDim)
			{
				continue;
			}
			if (++Listed > 4)
			{
				ForceLines.Add(FString::Printf(TEXT("      and %d more corps"), Piece->GetCorps().Num() - 4));
				break;
			}
			ForceLines.Add(Unit.Stale > 0
				? FString::Printf(TEXT("      %s  (%s, %d; a report %d month%s old)"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind), Unit.Divisions, Unit.Stale, Unit.Stale == 1 ? TEXT("") : TEXT("s"))
				: FString::Printf(TEXT("      %s  (%s, %d)"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind), Unit.Divisions));
		}
	}
	RegionPanel.Forces = FText::FromString(ForceLines.Num() > 0 ? FString::Join(ForceLines, TEXT("\n")) : FString(TEXT("No troops here.")));

	// On the player's own ground: what the province holds, and what can be raised or built there.
	if (FCampaignSession* Session = GetSession())
	{
		const FProvinceInfo Province = Session->Province(RegionId);
		if (Province.bValid)
		{
			// What the province holds and what it is worth: shown for any ground, friend's or enemy's.
			const bool bMine = Session->GetView().RegionHolder.IsValidIndex(RegionId) && Session->GetView().RegionHolder[RegionId] == Session->GetView().Human;
			FString Holdings = Province.Holdings;
			// (the prize line says what the place pays where the rules give one; the plain line is for where they do not)
			if (!Province.Yield.IsEmpty() && Province.PrizeLine.IsEmpty())
			{
				Holdings += bMine ? FString::Printf(TEXT("\nGives your treasury %s"), *Province.Yield) : FString::Printf(TEXT("\nWorth %s to whoever holds it"), *Province.Yield);
			}
			RegionPanel.Holdings = FText::FromString(Holdings);
			// Its chief town, and the prize: what taking the place is worth to the player, or what holding it pays.
			if (!Province.Town.IsEmpty() && Province.Town != Region.Name)
			{
				// (where the province is named for its city, its name has said so already)
				RegionPanel.Details = FText::FromString(FString::Printf(TEXT("Chief town %s  \u00B7  %s"), *Province.Town, *RegionPanel.Details.ToString()));
			}
			RegionPanel.Prize = FText::FromString(Province.PrizeLine);
			RegionPanel.PrizeTip = FText::FromString(FString::Join(Province.PrizeItems, TEXT("\n")));
			RegionPanel.bPrizeToTake = Province.PrizeKind == TEXT("taken") || Province.PrizeKind == TEXT("restored");
			if (Province.SiegeLines.Num() > 0)
			{
				RegionPanel.SiegeTitle = FText::FromString(Province.SiegeBy.IsEmpty() ? FString(TEXT("THE TOWN AS A FORTRESS")) : FString::Printf(TEXT("BESIEGED BY %s"), *Province.SiegeBy.ToUpper()));
				RegionPanel.SiegeText = FText::FromString(FString::Join(Province.SiegeLines, TEXT("\n")));
			}
			RegionPanel.DirectiveText = FText::FromString(Province.DirectiveLine);
			if (Province.PlacesLines.Num() > 0)
			{
				RegionPanel.PlacesTitle = FText::FromString(Province.PlacesHead.IsEmpty() ? FString(TEXT("PLACES OF THEIR OWN")) : Province.PlacesHead);
				RegionPanel.PlacesText = FText::FromString(FString::Join(Province.PlacesLines, TEXT("\n")));
			}
			// The town's plots: shown on the player's own ground, and elsewhere only where something stands.
			const bool bBuiltOn = Province.Plots.ContainsByPredicate([](const FString& Plot) { return Plot != TEXT("Free plot"); });
			RegionPanel.Plots = Province.bHasPlots && (Province.bPlotsMine || bBuiltOn) ? Province.Plots : TArray<FString>();
			RegionPanel.bPlotsMine = Province.bPlotsMine;
			if (RegionPanel.Plots.Num() > 0 && Province.TownPlots.Num() > 0)
			{
				const FString Holder = Session->GetView().RegionHolder.IsValidIndex(RegionId) ? Session->GetView().RegionHolder[RegionId] : Region.Owner;
				const int32 Count = Province.TownPlots.Num();
				TownBar.Region = RegionId;
				TownBar.Tag = Holder;
				TownBar.bMine = Province.bPlotsMine;
				TownBar.Plots = Province.TownPlots;
				TownBar.Offers = Province.TownOffers;
				TownBar.Title = FText::FromString(Province.Town.IsEmpty() ? Region.Name : Province.Town);
				TownBar.Sub = FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d building plot%s, %s"),
					Province.TownKind == TEXT("capital") ? TEXT("The capital") : Province.TownKind == TEXT("city") ? TEXT("A city") : Province.TownKind == TEXT("town") ? TEXT("A town") : TEXT("A village"),
					Count, Count == 1 ? TEXT("") : TEXT("s"), Province.PlotsFree > 0 ? *FString::Printf(TEXT("%d free"), Province.PlotsFree) : TEXT("all taken")));
				if (!TownBar.Plots.IsValidIndex(TownBar.Picked))
				{
					TownBar.Picked = INDEX_NONE;
				}
			}
			RegionPanel.PlotsFree = Province.PlotsFree;
			RegionPanel.PlotsCanBuild = Province.PlotsCanBuild;
		}
		// (ground that is not the player's can still have something to order: a reconnaissance flight over it)
		if (Province.bValid && (Province.bHome || Province.Options.Num() > 0))
		{
			RegionPanel.ProvinceNote = FText::FromString(Province.Note);
			RegionPanel.Options = Province.Options;
			if (!Province.bHome)
			{
				// On ground that is not the player's, only what can be ordered from afar is offered.
				RegionPanel.Options.RemoveAll([](const FProvinceOption& Option)
				{
					return !Option.Action.StartsWith(TEXT("work:recon")) && !Option.Action.StartsWith(TEXT("work:evac")) && !Option.Action.StartsWith(TEXT("work:storm"))
						&& !Option.Action.StartsWith(TEXT("work:sally")) && !Option.Action.StartsWith(TEXT("work:garrison")) && !Option.Action.StartsWith(TEXT("work:field"));
				});
			}
			RegionPanel.Forming = Province.Forming;
		}
	}
}
