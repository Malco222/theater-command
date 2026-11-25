#include "Battle/MonthSeaScene.h"

#include "Battle/SeaBattle.h"
#include "Campaign/CampaignGameMode.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignView.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"

AMonthSeaScene::AMonthSeaScene()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// (after the controller's tick, which takes the rules' answers and opens and shuts the month's card)
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

bool AMonthSeaScene::IsWanted()
{
	FString Asked;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSubScene="), Asked))
	{
		return true;
	}
	// (a run that tests a scene, takes pictures or plays the playtester's steps is left as it was)
	static const TCHAR* const Runs[] = { TEXT("TheaterShots="), TEXT("TheaterSteps="), TEXT("TheaterSeaBattle="), TEXT("TheaterAirBattle="), TEXT("TheaterStage=") };
	for (const TCHAR* Run : Runs)
	{
		if (FCString::Strifind(FCommandLine::Get(), Run) != nullptr)
		{
			return false;
		}
	}
	return true;
}

void AMonthSeaScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Here = GetWorld();
	ACampaignGameMode* Campaign = Here ? Cast<ACampaignGameMode>(Here->GetAuthGameMode()) : nullptr;
	FCampaignSession* Session = Campaign ? Campaign->GetSession() : nullptr;
	ACampaignPlayerController* Player = Here ? Cast<ACampaignPlayerController>(Here->GetFirstPlayerController()) : nullptr;
	if (!Session || !Player || !Session->IsRunning() || Playing.IsValid())
	{
		FreeSince = -1.0;
		return;
	}
	const FCampaignView& View = Session->GetView();
	// A war begun or read in: nothing kept of the last holds (an answer still coming for it is dropped as it lands).
	if (Session->GetWarsBegun() != War)
	{
		War = Session->GetWarsBegun();
		bAsking = false; // (an answer still coming is dropped as it lands: it was for the war before)
		AskedTurn = INDEX_NONE;
		Mark = -1;
		Waiting.Reset();
		WaitingTurn = INDEX_NONE;
		Player->OfferMonthSea(false); // (release 2, tc.UI.SeaWaits)
	}
	// (a month gone by before its scene could play: only the month in hand is shown)
	if (!Waiting.IsEmpty() && WaitingTurn != View.Turn)
	{
		Waiting.Reset();
		WaitingTurn = INDEX_NONE;
	}
	if (Waiting.IsEmpty())
	{
		Player->OfferMonthSea(false); // (release 2, tc.UI.SeaWaits: nothing held, nothing offered)
	}
	// The table free: no card, page, newsreel or question over it, no month being played through, no order being answered,
	// and no button held (a flag or an agent being carried: the scene's taking the keys would let go of it as an order).
	const bool bHeld = Player->IsInputKeyDown(EKeys::LeftMouseButton) || Player->IsInputKeyDown(EKeys::RightMouseButton) || Player->IsInputKeyDown(EKeys::MiddleMouseButton)
		|| (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Num() > 0);
	const bool bFree = !Player->IsTableBusy() && !Player->IsBusyForSteps() && !Session->IsAnswering() && !bHeld;
	const double Now = FPlatformTime::Seconds();
	FreeSince = bFree ? (FreeSince < 0.0 ? Now : FreeSince) : -1.0;
	const bool bFreeAWhile = bFree && Now - FreeSince >= 1.0;

	// The month's question, asked once a month: while its card lies open (the rules work as he reads), or once the
	// table has stood free a second where no card opens. Only for a player who leads a nation, and only in his month.
	const bool bCardOpen = Player->IsPageOpen() && Player->GetPage().Month.bValid;
	if (!bAsking && AskedTurn != View.Turn && !View.Human.IsEmpty() && View.bHumanUp && !Session->IsAnswering() && (bCardOpen || bFreeAWhile))
	{
		// (the news counted from the last month's mark where that month was asked; else from this month's own)
		const int32 From = AskedTurn != INDEX_NONE && AskedTurn == View.Turn - 1 ? Mark : -1;
		AskedTurn = View.Turn;
		bAsking = true;
		const int32 AskedFor = View.Turn, InWar = War;
		TWeakObjectPtr<AMonthSeaScene> Self(this);
		Session->SubSceneAside(From, [Self, AskedFor, InWar](int32 NextMark, const FString& Recording)
			{
				AMonthSeaScene* Hook = Self.Get();
				if (!Hook)
				{
					return;
				}
				Hook->bAsking = false;
				if (Hook->War != InWar || Hook->AskedTurn != AskedFor)
				{
					return; // (another war, or another month, since it was asked)
				}
				Hook->Mark = NextMark;
				if (!Recording.IsEmpty())
				{
					Hook->Waiting = Recording;
					Hook->WaitingTurn = AskedFor;
				}
			});
		return;
	}

	// The scene, once the table has stood free a second. (Release 2, tc.UI.SeaWaits; Brian's card, 25 November 2026:
	// "waits behind a button".) In a month that opened with the enemy's moves near him, it does not follow them by
	// itself: it waits behind THE MONTH AT SEA until he presses it, or the month ends.
	const bool bHeldBack = !Waiting.IsEmpty() && Player->HoldsMonthSea();
	if (bHeldBack)
	{
		Player->OfferMonthSea(true);
	}
	const bool bAskedFor = bHeldBack && bFree && Player->TakeMonthSeaAsk();
	if (!Waiting.IsEmpty() && ((!bHeldBack && bFreeAWhile) || bAskedFor))
	{
		Player->OfferMonthSea(false);
		const FString Text = MoveTemp(Waiting);
		Waiting.Reset();
		const FString Name = FString::Printf(TEXT("%s, %s"), *View.Human, *View.Date);
		WaitingTurn = INDEX_NONE;
		FreeSince = -1.0;
		ASeaBattle* Scene = Here->SpawnActor<ASeaBattle>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Scene && Scene->BeginMonthScene(Text, Name))
		{
			Playing = Scene;
		}
		else if (Scene)
		{
			UE_LOG(LogTheater, Warning, TEXT("Sea scene: the month's scene (%s) could not be played; the month goes on without it."), *Name);
			Scene->Destroy();
		}
	}
}
