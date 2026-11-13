#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSounds.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignSession.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
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
	Opening.Tab = Opening.Years.Num() == 0 ? 1 : FMath::Clamp(Tab, 0, 1);
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
	Notify(Nation
		? FString::Printf(TEXT("%s. You lead %s.%s"), *View.Date, *Nation->Name, View.bEve ? TEXT(" Place your army, then end the month.") : TEXT(""))
		: FString::Printf(TEXT("%s. End the month to watch the war."), *View.Date), 7.0);
}

void ACampaignPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
		: Posture == TEXT("back") ? TEXT("They will fall back whole if they are attacked at odds they cannot hold.")
		: TEXT("They will stand and fight where they are."), 4.0);
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
	const bool bDone = Session->OrderInProvince(Action, Map->GetSelectedRegion(), Note);
	if (bDone && Action.StartsWith(TEXT("work:")))
	{
		// The table shows it at once: the bridge down, the line cut; and it is heard.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		CampaignSounds::Play(this, Action.StartsWith(TEXT("work:blow")) ? TEXT("explosion") : TEXT("hammer"), 0.6f);
	}
	RefreshRegionPanel(Map->GetSelectedRegion());
	Notify(Note, 5.0);
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
	FString Note;
	FSortie Sortie;
	const bool bOk = Session->Act(Action, SelectedRegion, Note, &Sortie);
	CampaignSounds::Play(this, bOk ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (bOk && Sortie.bValid && Map.IsValid())
	{
		// The aircraft are seen to take off for their target and come home again.
		Map->FlySortie(Sortie);
		CampaignSounds::Play(this, Sortie.bBombers ? TEXT("bomber_drone") : TEXT("fighter_pass"), 0.45f);
	}
	if (bOk)
	{
		// Whatever it was may have changed the table: the fog lifted, the world at war.
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 8.0);
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
	if (bWatching || FaceOff.bVisible || Opening.bVisible)
	{
		return;
	}
	if (FCampaignSession* Session = GetSession())
	{
		Document = Session->Roll();
		++DocumentRevision;
	}
}

void ACampaignPlayerController::OpenGuide()
{
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
		TEXT("In the army's tray you can leave some corps behind, say what it does if attacked (stand, dig in or fall back), and send it BY RAIL along a working railway or BY SEA from a port."));
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
	Add(TEXT("FLEETS AND AIR FLEETS"),
		TEXT("Click a fleet or an air fleet to give it orders: where to patrol or raid, and what to bomb. ")
		TEXT("A corps in a port can go BY SEA to a friendly port or land on an enemy coast. Before a landing the game tells you what it will meet, and asks."));
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
		}
	}
}

void ACampaignPlayerController::ToggleEffects()
{
	Notify(CampaignSounds::Toggle() ? TEXT("Sound effects on.") : TEXT("Sound effects off. Press N to bring them back."), 3.0);
}

void ACampaignPlayerController::SpaceBar()
{
	SkipWatching();
}

// The number keys open the screens of the top bar, and the same key puts one away again.
void ACampaignPlayerController::OpenScreen1() { if (bArsenalOpen) { CloseArsenal(); } else { OpenArsenal(); } }
void ACampaignPlayerController::OpenScreen2() { if (bGeneralsOpen) { CloseGenerals(); } else { OpenGenerals(); } }
void ACampaignPlayerController::OpenScreen3() { if (bResearchOpen) { CloseResearch(); } else { OpenResearch(); } }
void ACampaignPlayerController::OpenScreen4() { if (bDiplomacyOpen) { CloseDiplomacy(); } else { OpenDiplomacy(); } }
void ACampaignPlayerController::OpenScreen5() { if (bLedgerOpen) { CloseLedger(); } else { OpenLedger(); } }

void ACampaignPlayerController::ToggleWatchAll()
{
	bWatchAll = !bWatchAll;
	Notify(bWatchAll ? TEXT("At the end of a month you will watch every nation's moves and battles.") : TEXT("Quick months: you will be shown only what touches you. The rest is in the news."), 5.0);
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
	if (Document.bValid)
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

	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || !Map.IsValid())
	{
		return;
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
	const bool bBusy = bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Document.bValid || bLedgerOpen || bArsenalOpen || bDiplomacyOpen || bResearchOpen || bGeneralsOpen || bPageOpen;

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
			ClickOnTable(PointedPiece, PointedRegion);
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
	CurrentMouseCursor = HeldPiece.IsValid() && bDragging ? EMouseCursor::GrabHandClosed : (Offered ? EMouseCursor::GrabHand : EMouseCursor::Default);

	// Whatever the pointer rests on is lit faintly, and with an army in hand the road to it is drawn.
	if (!bScripted && !bTurning && (!bDragging || HeldPiece.IsValid()))
	{
		Map->SetHoveredRegion(bBusy ? INDEX_NONE : PointedRegion);
		if (ArmyRegion != INDEX_NONE && !bBusy)
		{
			ShowRouteTo(PointedRegion);
		}
	}

	Map->UpdateForCamera(Eye->GetDistance(), Eye->GetYaw(), Eye->GetFocus());

	// A tip that follows the pointer: what the province under it is, whose, what it is worth and who stands there.
	const int32 TipRegion = (bBusy || !bHaveMouse || bLeftHeld || bRightHeld) ? INDEX_NONE : PointedRegion;
	if (TipRegion != HoverTipRegion)
	{
		HoverTipRegion = TipRegion;
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
			if (Region.VictoryPoints > 0) { Worth.Add(FString::Printf(TEXT("worth %d"), Region.VictoryPoints)); }
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
			}
			HoverTip = FText::FromString(Line);
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

void ACampaignPlayerController::ClickOnTable(AArmyPiece* Piece, int32 RegionId)
{
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
		ShowForce(Piece->IsFleet(), Piece->GetForceId());
		CampaignSounds::Play(this, Piece->IsFleet() ? TEXT("ship_horn") : TEXT("fighter_pass"), 0.35f);
		return;
	}
	// A click on somebody's flag is a click on the ground it stands on.
	const int32 Target = Piece ? Piece->GetRegionId() : RegionId;

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
	ArmyRegion = RegionId;
	PickedCorps = MoveTemp(Orderable);
	RefreshOrders();
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
	Tray.Cards.Reset();
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

	if (ArmyRegion == INDEX_NONE)
	{
		bByRail = false;
		bBySea = false;
		Map->SetReach(Reach);
		return;
	}

	// The army in hand, as the rules see it now.
	const FCampaignView& View = Session->GetView();
	const FCampaignNation* Nation = Map->GetData().FindNation(View.Human);
	FString General;
	int32 Divisions = 0, CorpsCount = 0;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Region != ArmyRegion || Unit.Nation != View.Human)
		{
			continue;
		}
		FTrayCard Card;
		Card.CorpsId = Unit.Id;
		Card.Name = FText::FromString(Unit.Name);
		Card.Kind = FText::FromString(FCampaignMapData::CorpsKindLabel(Unit.Kind));
		Card.Divisions = Unit.Divisions;
		Card.Strength = Unit.Strength;
		Card.KindId = Unit.Elite == TEXT("heavy") ? FString(TEXT("arm")) : Unit.Kind;
		Card.Morale = Unit.Cohesion;
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
	Tray.Subtitle = FText::FromString(FString::Printf(TEXT("%s army at %s  ·  %d corps, %d division%s"),
		*Adjective, *Where, CorpsCount, Divisions, Divisions == 1 ? TEXT("") : TEXT("s")));

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
	Map->SetRoute(From, RingedPiece.IsValid() ? Road : TArray<int32>());
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
		OpenFaceOff(RegionId);
		return;
	}

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
		Map->SetHolders(View.RegionHolder, View.League);
		Map->ShowArmies(View, Marches, CarriedFrom);
		Map->ShowWorks(View.BridgesDown, View.RailsCut);
		Map->ShowBuildings(View.RegionWorks);
		Map->ShowLanes(View.Lanes);
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
			FString Line = FString::Printf(TEXT("%s  \u00B7  %s, %d division%s"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind),
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
		int32 Divisions = 0, Men = 0;
		for (const FBattleUnit& Unit : Units)
		{
			Divisions += Unit.Divisions;
			Men += Unit.Men;
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
	FaceOff.OurName = FText::FromString(Ours ? Ours->Name : View.Human);
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
	FaceOff.OddsLine = FText::FromString(Preview.Odds >= 1.0f
		? FString::Printf(TEXT("Odds of %.1f to 1 in your favour"), Preview.Odds)
		: FString::Printf(TEXT("Odds of %.1f to 1 against you"), 1.0f / FMath::Max(Preview.Odds, 0.01f)));

	// The Battle Ticket: what the campaign map says about this fight, and what each thing does in it.
	TArray<FString> Lines;
	for (const FTicketLine& Line : Preview.Lines)
	{
		Lines.Add(Line.Effect.IsEmpty()
			? FString::Printf(TEXT("%s:  %s"), *Line.Label, *Line.Value)
			: FString::Printf(TEXT("%s:  %s  \u2014  %s"), *Line.Label, *Line.Value, *Line.Effect));
	}
	FaceOff.Ticket = FText::FromString(FString::Join(Lines, TEXT("\n")));
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
	CampaignSounds::PlayLater(this, Result.bTaken ? TEXT("victory") : TEXT("defeat"), 2.8f, 0.6f);

	FaceOff.bResolved = true;
	FaceOff.ResultTitle = FText::FromString(Result.bYielded ? FString(TEXT("They fell back without a fight")) : Result.Level);
	const FString Outcome = Result.bTaken ? TEXT("The ground is yours.") : TEXT("The attack was held. The ground is still theirs.");
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
		// The result has been read: the field is still smoking when the eye goes back to the table.
		Map->AddBattleSmoke(FaceOff.Region);
	}
	FaceOff.bVisible = false;
}

void ACampaignPlayerController::EndMonth()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || bWatching || FaceOff.bVisible || Opening.bVisible)
	{
		return;
	}
	PutDownArmy();
	CampaignSounds::Play(this, TEXT("bell"), 0.5f);
	// Where every corps stands as the month ends, so that the others' marches can be seen from there.
	WatchStood.Reset();
	for (const FViewCorps& Unit : Session->GetView().Corps)
	{
		WatchStood.Add(Unit.Id, Unit.Region);
	}
	Month = Session->EndMonth();
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
	const double Pause = Step.bMine ? 1.3 : Step.bMarchOnly ? 0.55 : 0.8;
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

	// The month's news stays up for a while.
	TArray<FString> Lines;
	for (int32 i = 0; i < Month.Headlines.Num() && i < 7; ++i)
	{
		Lines.Add(Month.Headlines[i]);
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

void ACampaignPlayerController::RefreshRegionPanel(int32 RegionId)
{
	RegionPanel = FRegionPanelInfo();
	RegionPanel.Revision = ++PanelRevision;
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
		for (const FViewCorps& Unit : Piece->GetCorps())
		{
			ForceLines.Add(FString::Printf(TEXT("      %s  (%s, %d)"), *Unit.Name, *FCampaignMapData::CorpsKindLabel(Unit.Kind), Unit.Divisions));
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
			if (!Province.Yield.IsEmpty())
			{
				Holdings += bMine ? FString::Printf(TEXT("\nGives your treasury %s"), *Province.Yield) : FString::Printf(TEXT("\nWorth %s to whoever holds it"), *Province.Yield);
			}
			RegionPanel.Holdings = FText::FromString(Holdings);
		}
		if (Province.bValid && Province.bHome)
		{
			RegionPanel.ProvinceNote = FText::FromString(Province.Note);
			RegionPanel.Options = Province.Options;
			RegionPanel.Forming = Province.Forming;
		}
	}
}
