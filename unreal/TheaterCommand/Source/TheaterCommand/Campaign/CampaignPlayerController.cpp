#include "Campaign/CampaignPlayerController.h"
#include "Campaign/TheaterSpeed.h"
#include "Campaign/CampaignText.h"
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
#include "Battle/AirBattle.h"
#include "Battle/LandBattle.h"
#include "Battle/LandField.h"
#include "Battle/LandScape.h"
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
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/Parse.h"

namespace
{
	/**
	 * (Brian, 24 November 2026: "when i have a unit on an allied airfield there is no way to know its there".) On a field
	 * another country holds, how many of the groups based there are the player's: ", 1 of them yours", ", yours" or "".
	 */
	FString YoursAmong(const FViewAirfield& Field)
	{
		if (Field.YourGroups <= 0 || Field.Groups <= 0)
		{
			return FString();
		}
		return Field.YourGroups >= Field.Groups ? FString(Field.Groups == 1 ? TEXT(", yours") : TEXT(", all yours"))
			: FString::Printf(TEXT(", %d of them yours"), Field.YourGroups);
	}

	/** (Speed, b414 mark 1: see ACampaignPlayerController::Afterwards.) */
	TAutoConsoleVariable<int32> CVarPictureFirst(
		TEXT("tc.Speed.PictureFirst"), 1,
		TEXT("1 (default): an order's answer is laid on the table a step a frame: the ground and the armies, then the rest of the table, then the army in hand's orders and the province's card. 0: all in one frame, as before."),
		ECVF_Default);

	/**
	 * (Speed, b414 mark 1.) The army in hand after an order that carries the corps that moved into a province, as the
	 * rules' thread is to ask its questions ahead: see FCampaignSession::MoveAside. Selected: the province whose card is
	 * shown after (INDEX_NONE: none).
	 */
	FCampaignSession::FHandNext HandOf(const TArray<FCorpsMove>& Moved, int32 Region, int32 Selected, bool bPlain, const FString& Adjective)
	{
		FCampaignSession::FHandNext Next;
		Next.Region = Region;
		Next.Selected = Selected;
		Next.bPlain = bPlain;
		Next.Adjective = Adjective;
		for (const FCorpsMove& Each : Moved)
		{
			Next.Ids.Add(Each.CorpsId);
		}
		return Next;
	}

	/**
	 * (The 1.5.1 pre-check, 24 November 20:24Z.) A fleet the player's fleet in hand may be set on: one of a nation at war with
	 * his, as an army is set only on an army he is at war with (WhyNoAttackOn). An ally's or a neutral's fleet under the
	 * pointer is only sea: a drop or a right-click there sails his fleet to it, as before the patch.
	 */
	bool IsEnemyFleet(const AArmyPiece* Piece, const FCampaignView& View)
	{
		if (!Piece || !Piece->IsFleet() || Piece->GetForceId() == INDEX_NONE || Piece->GetNationTag() == View.Human)
		{
			return false;
		}
		const FString& Tag = Piece->GetNationTag();
		for (int32 Index = 0; Index + 1 < View.Wars.Num(); Index += 2)
		{
			if ((View.Wars[Index] == View.Human && View.Wars[Index + 1] == Tag) || (View.Wars[Index + 1] == View.Human && View.Wars[Index] == Tag))
			{
				return true;
			}
		}
		return false;
	}
}

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
	// (rules version 66) How deep it is played; the depth the rules offer first unless another was picked before.
	Opening.Depths = Rules->GrandDepths();
	Opening.Histories = Rules->HistoryStops();
	if (!Opening.Histories.IsValidIndex(Opening.HistoryIndex))
	{
		Opening.HistoryIndex = Opening.Histories.IndexOfByPredicate([Rules](const FGrandLevel& Stop) { return Stop.Stop == Rules->FirstHistory(); });
	}
	if (!Opening.Depths.IsValidIndex(Opening.DepthIndex))
	{
		Opening.DepthIndex = Opening.Depths.IndexOfByPredicate([Rules](const FGrandLevel& Depth) { return Depth.Id == Rules->FirstDepth(); });
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
	ReadSavedWars(); // (release 1.5.1)
	Opening.Door = Rules->LessonDoor(); // (rules 84.3.3: FIRST COMMAND's words, lit until the ten are done)
	Opening.bCanReturn = Rules->IsRunning();
	if (bShow && !Opening.bVisible)
	{
		DoorSince = FPlatformTime::Seconds();
	}
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
		// (the UI director, 10:57 on build 1638: provinces, as the brief of round 1 asked, not territories)
		Out.Strength = FText::FromString(FString::Printf(TEXT("%d corps, %d %s"), Nation.Corps, Nation.Regions, Nation.Regions == 1 ? TEXT("province") : TEXT("provinces")));
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
	Opening.Tab = Tab == 2 || Tab == 3 ? Tab : Opening.Years.Num() == 0 ? 1 : FMath::Clamp(Tab, 0, 1);
	Opening.AskDelete.Empty(); // (a DELETE left unanswered is not carried to another page)
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
			Add(TEXT("cabinet"), TEXT("The war cabinet"), TEXT("Ministers, decrees, the cabinet's trust and its directives. Off: none of it, and nothing it moved is moved; what stands open is settled first, and you are shown the bill. It can be switched in any month, and this campaign remembers it."), bSits ? TEXT("ON") : TEXT("OFF"), bSits);
			// (Version 62) Whether the cabinet can bring the player down: a setting of its own, under the cabinet's switch.
			if (bSits)
			{
				const bool bSecure = Session->GetView().bOfficeSecure;
				Add(TEXT("office"), TEXT("Your office"), TEXT("Secure: the cabinet still doubts and withholds, but it never falls and you are never turned out. Otherwise a cabinet left with no trust in you for months running falls."), bSecure ? TEXT("SECURE") : TEXT("CAN BE BROUGHT DOWN"), bSecure);
			}
		}
	}
	// (rules version 66) How deep the campaign is played: the door to the staff's room, where a depth or any one desk is changed.
	if (const FCampaignSession* Session = GetSession())
	{
		if (Session->IsRunning() && !Session->GetView().CommandName.IsEmpty() && !Session->GetView().Human.IsEmpty())
		{
			Add(TEXT("depth"), TEXT("How deep you play"), TEXT("Which parts of the war are on your own desk and which your staff runs for you: research, building and trade, diplomacy, the secret service, the air war and the navy. The armies are always yours. Press to open YOUR STAFF and change the depth, or any one desk, in any month."), *Session->GetView().CommandName.ToUpper(), true);
		}
	}
	// (The 1.5 patch, breakup.ts; Brian 24 November 2026 18:02Z) Divisions split off a corps, and put back: on in a grand
	// campaign unless switched off here, in any month.
	if (const FCampaignSession* Session = GetSession())
	{
		const FCampaignView& Now = Session->GetView();
		if (Session->IsRunning() && Now.bBreakUpCan)
		{
			Add(TEXT("breakuprule"), TEXT("Break up corps"), TEXT("Pick up an army and every division of each corps is a tile on its card. Click the tiles of the divisions you want apart and press SPLIT OFF: they become a corps of their own where the army stands, and stay behind when the army moves. REJOIN puts a part back into the corps it came from. Nothing is lost or gained, and no corps grows bigger than it was. It can be switched in any month."),
				Now.bBreakUp ? TEXT("ON") : TEXT("OFF"), Now.bBreakUp);
		}
		// (The 1.5.1 patch, fleetattack.ts; Brian 23 November 2026 22:23Z and 24 November 19:47Z) Fleets dropped on an enemy
		// fleet attack it at once: on in a grand campaign unless switched off here, in any month.
		if (Session->IsRunning() && Now.bFleetAttackCan)
		{
			Add(TEXT("fleetattackrule"), TEXT("Fleets attack fleets"), TEXT("Drop one of your fleets on an enemy fleet in its reach, at sea or in his harbour, and the battle is fought at once, at any odds: the card gives the odds and the likely losses first. A fleet attacks once a month, and stays where the battle leaves it. Off, fleets meet only at the month's end, where an admiral thinks the odds good enough. It can be switched in any month."),
				Now.bFleetAttack ? TEXT("ON") : TEXT("OFF"), Now.bFleetAttack);
		}
	}
	Add(TEXT("music"), TEXT("Music"), TEXT("The game's own score, which follows the war. Key M."), bMusic ? TEXT("ON") : TEXT("OFF"), bMusic);
	Add(TEXT("loud"), TEXT("How loud the music plays"), TEXT("From soft, under the sounds of the table, to full."), Loud == 0 ? TEXT("SOFT") : Loud == 1 ? TEXT("MIDDLE") : TEXT("FULL"), bMusic);
	Add(TEXT("sounds"), TEXT("Sounds"), TEXT("Boots, engines, guns, aircraft and the click of every button. Key N."), CampaignSounds::IsOn() ? TEXT("ON") : TEXT("OFF"), CampaignSounds::IsOn());
	Add(TEXT("newsreels"), TEXT("Newsreels"), TEXT("A month's great stories shown as film when it ends. Key K; Space skips one."), bNewsreels ? TEXT("ON") : TEXT("OFF"), bNewsreels);
	Add(TEXT("narrator"), TEXT("The narrator"), TEXT("The game's voice: it reads the newsreels and the opening flight aloud, speaks for the adviser, and lets your armies answer."), bNarrator ? TEXT("ON") : TEXT("OFF"), bNarrator);
	Add(TEXT("showsfirst"), TEXT("Play the month before its card"), TEXT("Off: the month's card comes first when a month ends, and the tour of the map and the newsreel are a press away on it. On: the other countries' moves, the newsreel and the great moments play before the card."), bShowsFirst ? TEXT("ON") : TEXT("OFF"), bShowsFirst);
	Add(TEXT("briefing"), TEXT("The opening flight"), TEXT("A flight over the situation when a campaign begins, and a title card as each year opens."), bBriefings ? TEXT("ON") : TEXT("OFF"), bBriefings);
	Add(TEXT("moments"), TEXT("Moments at the front"), TEXT("When you take a capital or win a great battle, the eye drops to the place for about three seconds and is handed back. Any key or click ends it sooner."), bMoments ? TEXT("ON") : TEXT("OFF"), bMoments);
	// (The 1.5 patch; Brian 24 November 2026 18:13Z: "when i move a navy I want to see the boats sail to it as well".)
	// (Release 1.5.1; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered")
	Add(TEXT("readynotice"), TEXT("New units ready"), TEXT("On: when a corps, aircraft or ships you raised or bought are handed over, a READY FOR ORDERS plate names them as your month opens, once the month's card is closed. SHOW ME (or press G) takes the eye to each in turn and puts a new army in your hand. Off: they are told only in the month's news."), bReadyNotice ? TEXT("ON") : TEXT("OFF"), bReadyNotice);
	Add(TEXT("sail"), TEXT("Ships sail on the map"), TEXT("A fleet sent to another sea, into harbour or out of it sails there by water, round the land, its bows the way it goes, in two or three seconds. Off, it is simply found where it now is. Fleets only reported, not seen, are never shown sailing."), bShipsSail ? TEXT("ON") : TEXT("OFF"), bShipsSail);
	Add(TEXT("askend"), TEXT("Ask before a month ends with idle armies"), TEXT("When armies at the front have not moved and have no orders, END THE MONTH asks once before it ends the month. Armies in the rear are never counted."), bAskAtMonthsEnd ? TEXT("ON") : TEXT("OFF"), bAskAtMonthsEnd);
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
	else if (Id == TEXT("showsfirst"))
	{
		bShowsFirst = !bShowsFirst;
	}
	else if (Id == TEXT("briefing"))
	{
		bBriefings = !bBriefings;
	}
	else if (Id == TEXT("moments"))
	{
		bMoments = !bMoments;
	}
	else if (Id == TEXT("readynotice")) // (release 1.5.1, new units ready)
	{
		bReadyNotice = !bReadyNotice;
	}
	else if (Id == TEXT("sail")) // (release 2: ships that sail)
	{
		bShipsSail = !bShipsSail;
		if (Map.IsValid())
		{
			Map->SetShipsSail(bShipsSail);
		}
	}
	else if (Id == TEXT("askend"))
	{
		bAskAtMonthsEnd = !bAskAtMonthsEnd;
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
	else if (Id == TEXT("depth"))
	{
		CloseMenu();
		OpenStaff();
		return; // (kept by the campaign, not with the options)
	}
	else if (Id == TEXT("office"))
	{
		if (FCampaignSession* Session = GetSession())
		{
			FString Note;
			Session->Act(Session->GetView().bOfficeSecure ? TEXT("cabinet:office:fall") : TEXT("cabinet:office:secure"), INDEX_NONE, Note);
			if (!Note.IsEmpty())
			{
				Notify(Note, 6.0);
			}
		}
		++Opening.Revision;
		return; // (kept by the campaign, not with the options)
	}
	else if (Id == TEXT("fleetattackrule"))
	{
		// (The 1.5.1 patch, fleetattack.ts) Fleets sent straight at the enemy, switched in any month; kept by the campaign.
		if (FCampaignSession* Session = GetSession())
		{
			FString Note;
			if (Session->Act(Session->GetView().bFleetAttack ? TEXT("fleetattackrule:off") : TEXT("fleetattackrule:on"), INDEX_NONE, Note))
			{
				ShowWar(Session->GetView(), TArray<FCorpsMove>());
				// (1.5.1, Brian's hand-check list: a fleet still in hand when the row is flipped is laid out afresh, so that its
				// pointer, its "Drop ..." line and its first drop follow the new setting; its copy was refreshed only when it
				// was picked up, and switched back on, its first drop sailed it to the sea instead of opening the ATTACK card)
				if (FleetInHand != INDEX_NONE)
				{
					RefreshOrders();
				}
			}
			if (!Note.IsEmpty())
			{
				Notify(Note, 6.0);
			}
		}
		++Opening.Revision;
		return; // (kept by the campaign, not with the options)
	}
	else if (Id == TEXT("breakuprule"))
	{
		// (The 1.5 patch, breakup.ts) Breaking corps up, switched in any month; kept by the campaign.
		if (FCampaignSession* Session = GetSession())
		{
			FString Note;
			if (Session->Act(Session->GetView().bBreakUp ? TEXT("breakuprule:off") : TEXT("breakuprule:on"), INDEX_NONE, Note))
			{
				PickedDivisions.Reset();
				ShowWar(Session->GetView(), TArray<FCorpsMove>());
				// (1.5.1, Brian's hand-check list: an army still in hand when the row is flipped has its card laid out afresh,
				// its division tiles and SPLIT OFF with it, as the fleet in hand is for "Fleets attack fleets"; before, the card
				// kept the old setting until the army was put down and picked up again)
				if (ArmyRegion != INDEX_NONE)
				{
					RefreshOrders();
				}
			}
			if (!Note.IsEmpty())
			{
				Notify(Note, 6.0);
			}
		}
		++Opening.Revision;
		return; // (kept by the campaign, not with the options)
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
		TEXT("{\"music\":%s,\"loud\":%d,\"sounds\":%s,\"newsreels\":%s,\"narrator\":%s,\"adviser\":%s,\"watchAll\":%s,\"day\":%d,\"edge\":%s,\"briefing\":%s,\"colour\":%d,\"lamp\":%s,\"helped\":%s,\"moments\":%s,\"askEnd\":%s,\"marchSlip\":%s,\"showsFirst\":%s}\n"),
		Word(!Mode || Mode->IsMusicOn()), Mode ? Mode->GetMusicLevel() : 1, Word(CampaignSounds::IsOn()), Word(bNewsreels), Word(bNarrator), Word(bAdviserWanted), Word(bWatchAll), Keeping, Word(bEdgeScroll), Word(bBriefings), Map.IsValid() ? Map->GetColourStrength() : 1, Word(bLampSaved), Word(bHelpSeen), Word(bMoments), Word(bAskAtMonthsEnd), Word(bMarchSlipDone), Word(bShowsFirst));
	// (The 1.5 patch) "Ships sail on the map", kept with the rest.
	// (Release 1.5.1) "New units ready", kept with the rest.
	FFileHelper::SaveStringToFile(Text.LeftChop(2) + FString::Printf(TEXT(",\"sail\":%s,\"readyNotice\":%s}\n"), Word(bShipsSail), Word(bReadyNotice)), *OptionsFile());
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
	bMoments = Yes(TEXT("moments"), true);
	bAskAtMonthsEnd = Yes(TEXT("askEnd"), true);
	bMarchSlipDone = Yes(TEXT("marchSlip"), false);
	bShowsFirst = Yes(TEXT("showsFirst"), false);
	bShipsSail = Yes(TEXT("sail"), true); // (the 1.5 patch: ships that sail, on unless switched off)
	bReadyNotice = Yes(TEXT("readyNotice"), true); // (release 1.5.1: new units ready, on unless switched off)
	if (Map.IsValid())
	{
		Map->SetShipsSail(bShipsSail);
	}
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

void ACampaignPlayerController::PickHistory(int32 Index)
{
	if (Opening.Histories.IsValidIndex(Index) && Index != Opening.HistoryIndex)
	{
		Opening.HistoryIndex = Index;
		++Opening.Revision;
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
}

void ACampaignPlayerController::PickDepth(int32 Index)
{
	if (Opening.Depths.IsValidIndex(Index) && Index != Opening.DepthIndex)
	{
		Opening.DepthIndex = Index;
		++Opening.Revision;
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
}

FText ACampaignPlayerController::GetDepthLine() const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || Session->GetView().CommandName.IsEmpty() || Session->GetView().Human.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const FCampaignView& View = Session->GetView();
	return FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s"), *View.CommandName,
		View.Staffed == 0 ? TEXT("every desk is yours") : View.Staffed == 1 ? TEXT("your staff runs 1 desk") : *FString::Printf(TEXT("your staff runs %d desks"), View.Staffed)));
}

void ACampaignPlayerController::OpenStaff()
{
	OpenPage(TEXT("staff"));
}

const FViewDesk* ACampaignPlayerController::FindDesk(const FString& Id) const
{
	const FCampaignSession* Session = GetSession();
	return Session && !Session->GetView().Human.IsEmpty() ? Session->GetView().Desks.FindByPredicate([&Id](const FViewDesk& Desk) { return Desk.Id == Id; }) : nullptr;
}

void ACampaignPlayerController::PressDesk(const FString& Id)
{
	FCampaignSession* Session = GetSession();
	const FViewDesk* Desk = FindDesk(Id);
	if (!Session || !Desk)
	{
		return;
	}
	const FString Action = Desk->Act; // (kept: the desks are read again when the rules answer)
	FString Note;
	const bool bOk = Session->Act(Action, INDEX_NONE, Note);
	CampaignSounds::Play(this, bOk ? TEXT("stamp") : TEXT("click"), 0.6f);
	if (bOk)
	{
		// (a staff handed a desk in the player's own month goes to work at once: the table and the open screen are read again)
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		if (bResearchOpen)
		{
			const int32 Revision = Research.Revision + 1;
			Research = Session->Research();
			Research.Revision = Revision;
			bResearchOpen = Research.bValid;
		}
		if (bArsenalOpen)
		{
			const FCampaignView& View = Session->GetView();
			const bool bMine = View.RegionHolder.IsValidIndex(SelectedRegion) && View.RegionHolder[SelectedRegion] == View.Human;
			const int32 Revision = Arsenal.Revision + 1;
			Arsenal = Session->Arsenal(bMine ? SelectedRegion : INDEX_NONE);
			Arsenal.Revision = Revision;
			bArsenalOpen = Arsenal.bValid;
		}
		if (bDiplomacyOpen)
		{
			const int32 Revision = Diplomacy.Revision + 1;
			Diplomacy = Session->Diplomacy();
			Diplomacy.Revision = Revision;
			bDiplomacyOpen = Diplomacy.bValid;
		}
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 8.0);
	}
}

void ACampaignPlayerController::BeginGrandCampaign(bool bWatch, const FString& DepthWord)
{
	ACampaignGameMode* Mode = GetMode();
	if (!Mode || Opening.bWorking || !Opening.Years.IsValidIndex(Opening.YearIndex) || (!bWatch && !Opening.Nations.IsValidIndex(Opening.NationIndex)))
	{
		return;
	}
	const FString Human = bWatch ? FString() : Opening.Nations[Opening.NationIndex].Tag;
	const FString Level = Opening.Levels.IsValidIndex(Opening.LevelIndex) ? Opening.Levels[Opening.LevelIndex].Id : FString();
	FString Depth = !bWatch && Opening.Depths.IsValidIndex(Opening.DepthIndex) ? Opening.Depths[Opening.DepthIndex].Id : FString();
	if (!bWatch && !DepthWord.IsEmpty())
	{
		// (1.5.3, dev only: a step file's start line that names a depth is played at it, in a run that takes pictures too.
		// Before, "start UK 1939-09 seed 4 theatre" played the full game unless the launch said -TheaterDepth=theatre.)
		Depth = DepthWord;
	}
	else if (FCString::Strifind(FCommandLine::Get(), TEXT("TheaterShots=")) != nullptr)
	{
		// (a run that takes pictures plays the full game, with every desk in the player's hands, as it always has:
		// "-TheaterDepth=theatre" asks for another depth)
		Depth.Reset();
		FParse::Value(FCommandLine::Get(), TEXT("TheaterDepth="), Depth);
	}
	// (rules version 70) The history dial: the stop picked, for a watched war too. ("-TheaterHistory=5" asks for one in a picture run.)
	int32 History = Opening.Histories.IsValidIndex(Opening.HistoryIndex) ? Opening.Histories[Opening.HistoryIndex].Stop : 0;
	FParse::Value(FCommandLine::Get(), TEXT("TheaterHistory="), History);
	if (Mode->StartCampaign(Human, Opening.Years[Opening.YearIndex].From, TEXT("grand"), GrandSeed, Level, Depth, History))
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

void ACampaignPlayerController::AskToBegin(int32 Kind, int32 GroupIndex, int32 SideIndex)
{
	// (Ruling 130.) One campaign is kept on this computer. A new war writes over it at its first month's end, when
	// the menu is opened, or when the game is left: so where one is saved (and a campaign under way is saved as the
	// menu opens) the new war waits for the player's answer to a question that names it.
	Opening.AskKind = Kind;
	Opening.AskGroup = GroupIndex;
	Opening.AskSide = SideIndex;
	if (Opening.SavedLine.IsEmpty())
	{
		AnswerBegin(true);
		return;
	}
	++Opening.Revision;
}

void ACampaignPlayerController::AnswerBegin(bool bBegin)
{
	const int32 Kind = Opening.AskKind, GroupIndex = Opening.AskGroup, SideIndex = Opening.AskSide;
	Opening.AskKind = 0;
	++Opening.Revision;
	if (!bBegin)
	{
		return;
	}
	if (Kind == 3)
	{
		BeginCampaign(GroupIndex, SideIndex);
	}
	else if (Kind == 1 || Kind == 2)
	{
		BeginGrandCampaign(Kind == 2);
	}
	else if (Kind == 4)
	{
		BeginFirstCommand();
	}
	else if (Kind == 5)
	{
		LoadWar(Opening.AskLoad);
	}
}

void ACampaignPlayerController::ReadSavedWars()
{
	ACampaignGameMode* Mode = GetMode();
	Opening.SavedWars = Mode ? Mode->SavedWars() : TArray<FSavedWar>();
}

void ACampaignPlayerController::ShowSavedWars()
{
	ReadSavedWars();
	Opening.SavesNote = FText::GetEmpty();
	ShowOpeningTab(3);
}

void ACampaignPlayerController::SaveWar()
{
	// (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) A copy of the war
	// as it stands, kept beside the campaign the game keeps by itself; the page of saved wars opens with it at the top.
	ACampaignGameMode* Mode = GetMode();
	FCampaignSession* Rules = GetSession();
	if (!Mode || !Rules || !Rules->IsRunning() || !Opening.bCanReturn)
	{
		return;
	}
	FSavedWar War;
	if (Mode->SaveWarAs(War))
	{
		Opening.JustSaved = War.Id;
		Opening.SavesNote = FText::FromString(FString::Printf(TEXT("Saved: %s."), *War.Line));
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
	}
	else
	{
		Opening.SavesNote = FText::FromString(TEXT("The war could not be saved."));
	}
	ReadSavedWars();
	Opening.AskDelete.Empty();
	Opening.Tab = 3;
	++Opening.Revision;
}

void ACampaignPlayerController::AskToLoadWar(const FString& Id)
{
	ACampaignGameMode* Mode = GetMode();
	if (!Mode || Opening.bWorking)
	{
		return;
	}
	Opening.AskDelete.Empty();
	Opening.AskLoad = Id;
	// (Ruling 130: nothing replaces the campaign kept by itself without asking. Loading lays this war where that one
	// was, and the game keeps this one in its place from then on; where that one is kept as a saved war too, word for
	// word, nothing is lost and nothing is asked.)
	if (Opening.SavedLine.IsEmpty() || Mode->IsAutosaveKept())
	{
		LoadWar(Id);
		return;
	}
	Opening.AskKind = 5;
	++Opening.Revision;
}

void ACampaignPlayerController::AnswerLoadKeeping()
{
	// (the question's third answer: the campaign kept by itself is kept as a saved war, and then the other is loaded)
	ACampaignGameMode* Mode = GetMode();
	if (Opening.AskKind != 5 || !Mode)
	{
		return;
	}
	Opening.AskKind = 0;
	FSavedWar Kept;
	if (!Mode->KeepAutosaveAsWar(Kept))
	{
		Opening.SavesNote = FText::FromString(TEXT("Your campaign could not be kept as a saved war, so the other was not loaded."));
		ReadSavedWars();
		++Opening.Revision;
		return;
	}
	LoadWar(Opening.AskLoad);
}

void ACampaignPlayerController::LoadWar(const FString& Id)
{
	ACampaignGameMode* Mode = GetMode();
	FString Why;
	if (Mode && Mode->LoadSavedWar(Id, Why))
	{
		Opening.SavesNote = FText::GetEmpty();
		Opening.JustSaved.Empty();
		Opening.AskLoad.Empty();
		EnterCampaign();
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
		return;
	}
	Opening.SavesNote = FText::FromString(Why.IsEmpty() ? FString(TEXT("That saved war could not be read.")) : Why);
	ReadSavedWars();
	++Opening.Revision;
}

void ACampaignPlayerController::AskToDeleteWar(const FString& Id)
{
	Opening.AskDelete = Id;
	++Opening.Revision;
}

void ACampaignPlayerController::AnswerDeleteWar(bool bDelete)
{
	ACampaignGameMode* Mode = GetMode();
	if (bDelete && Mode && !Opening.AskDelete.IsEmpty())
	{
		Mode->DeleteSavedWar(Opening.AskDelete);
		if (Opening.JustSaved == Opening.AskDelete)
		{
			Opening.JustSaved.Empty();
			Opening.SavesNote = FText::GetEmpty();
		}
	}
	Opening.AskDelete.Empty();
	ReadSavedWars();
	++Opening.Revision;
}

void ACampaignPlayerController::BeginFirstCommand()
{
	// (Ruling 141; rules 84.3.3.) FIRST COMMAND's campaign is the rules' own (lessons.json: Germany, from its first
	// month, at Cadet and Field command), begun as the door's other wars are, and marked as FIRST COMMAND's: the
	// notes are on, and the ten are armed again if all were done.
	ACampaignGameMode* Mode = GetMode();
	FCampaignSession* Rules = GetSession();
	if (!Mode || !Rules || Opening.bWorking)
	{
		return;
	}
	const FLessonDoor Door = Rules->LessonDoor();
	if (!Door.bValid || Door.Nation.IsEmpty())
	{
		return;
	}
	if (Mode->StartCampaign(Door.Nation, Door.From, TEXT("grand"), NewSeed(), Door.Level, Door.Depth, 0, true))
	{
		bFreshCampaign = true;
		EnterCampaign();
		CampaignSounds::Play(this, TEXT("bugle"), 0.6f);
	}
}

void ACampaignPlayerController::OpenMenu()
{
	Opening.AskKind = 0; // (a question left unanswered is not carried to the next time the screen opens)
	Opening.AskDelete.Empty();
	Opening.JustSaved.Empty();
	Opening.SavesNote = FText::GetEmpty();
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
	// (the table has just been laid out for this campaign as it stands: from here on, ground that changes hands from
	// one month to the next is inked over)
	TurnLaid = Session->GetView().Turn;
	HumanLaid = Session->GetView().Human;
	// A clean table: nothing in hand, nothing picked, no old news.
	Afterwards.Reset(); // (what was left of an order's answer belonged to the war before; the session dropped its own)
	ReadyUnits.Reset(); // (release 1.5.1, new units ready: nor a READY plate of the campaign left)
	bReadyDue = false;
	ReadySaid = -1;
	RaidCardDue.Reset(); // (nor the report of a raid flown in the campaign left)
	PendingEnemyReach.Reset();
	Opening.bVisible = false;
	FaceOff = FFaceOff();
	CloseDocument();
	bWatching = false;
	ArmyRegion = INDEX_NONE;
	PickedCorps.Reset();
	// (The code checker, 17 November.) Nor is anything of the campaign just left carried into this one: a fleet or an
	// agent in hand, a question half asked, the round of armies already seen.
	PickedBeforeMass.Reset();
	PutDownFleet();
	PutDownAgent();
	PickedDivisions.Reset(); // (nor divisions picked on a card of the war before)
	WarPrompt = FWarPrompt();
	EndAsk.bVisible = false;
	RoundSeen.Reset();
	// (The code checker, 17 November.) The last month's newsreel and the first night's word belong to the campaign just
	// left too: a loaded game has no reel to play again until its own month ends, and tells its own first night.
	LastReel.Reset();
	LastReelDate.Reset();
	bNightTold = false;
	AlarmShown = 0;
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
	// (Rules 84.3.3) FIRST COMMAND: the staff's notes start afresh with the campaign. A war played from its first
	// month has no month card until its second (trap 1), so where lesson 1 is due the card it lies on is owed, and
	// is opened once the opening flight is over; until then no note stands on the desk.
	ResetLessons();
	bool bNotesOn = false;
	{
		FLessonDesk Probe;
		if (Nation && Session->LessonNow(true, TArray<int32>(), Probe))
		{
			bNotesOn = Probe.bOn;
			bOpeningCardOwed = Probe.bLaid && View.Turn == 1;
		}
	}
	// The adviser stands by for the first months of a campaign with somebody at the table: while the staff's notes
	// are on, they teach in his place, at his place under the date card.
	{
		const int32 Next = Adviser.Revision + 1; // (so that the screen writes his list afresh)
		Adviser = FAdviser();
		bAdviserSpoke = false;
		Adviser.Revision = Next;
		Adviser.bOn = Nation != nullptr && View.Turn <= 2 && !bScripted && bAdviserWanted && !bNotesOn;
	}
	Notify(Nation
		? FString::Printf(TEXT("%s. You lead %s.%s"), *View.Date, *WithThe(Nation->Name), View.bEve ? TEXT(" Place your army, then end the month.") : TEXT(""))
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

	// (Brian, 17 November 2026: "how do we make the game run more smooth". The art director's speed brief, group A,
	// all at once: a map seen from above needs one sun, one soft fill, the relief shade already painted into the
	// ground and the table lamp. Bounced light worked out each frame, mirror reflections, fine contact shade and the
	// lens's blurs are given up; shadows are softer; the map is drawn at two thirds of the window each way and
	// sharpened up, with the frame, the cards and the plates drawn after at full size. Timed inside one running game
	// on his Mac, over New York and Louisiana at playing height: the graphics chip's share of a frame falls from
	// 22.8 ms to 15.9, and the picture's light stays within one part in a hundred. "-TheaterOldLight" keeps the
	// old settings, for a picture to set beside the new.)
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLight")))
	{
		static const TPair<const TCHAR*, const TCHAR*> Trims[] = {
			{ TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("0") },
			{ TEXT("r.ReflectionMethod"), TEXT("0") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("0") },
			{ TEXT("r.ContactShadows"), TEXT("0") },
			{ TEXT("sg.ShadowQuality"), TEXT("2") },
			{ TEXT("r.DepthOfFieldQuality"), TEXT("0") },
			{ TEXT("r.LensFlareQuality"), TEXT("0") },
			{ TEXT("r.LightShaftQuality"), TEXT("0") },
		};
		for (const TPair<const TCHAR*, const TCHAR*>& Trim : Trims)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Trim.Key))
			{
				Variable->Set(Trim.Value, ECVF_SetByCode);
			}
		}
	}

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
			if (Map->GetData().bWorld)
			{
				// (the whole-world map: the eye may go wherever the world goes, from its north-west corner to its south-east)
				const FCampaignMapData& World = Map->GetData();
				const FVector NorthWest = Map->MapToWorld(World.WorldMinX, World.WorldMinY), SouthEast = Map->MapToWorld(World.WorldMinX + World.WorldWidth, World.WorldMinY + World.WorldHeight);
				Eye->SetBoundsBox(FVector2D(SouthEast.X, NorthWest.Y), FVector2D(NorthWest.X, SouthEast.Y));
				// (and from west to east it runs right round: the eye's place is kept within half a world of the map's own middle)
				const double HalfWorld = World.WorldWidth * 0.5 * Map->UnitScale;
				Eye->SetWrap(-HalfWorld, HalfWorld);
				// (and at the chart's north and south edges the table beyond it fills no more than about a fifth of the screen)
				Eye->SetEdgeHold(0.2f, 0.2f);
			}
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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACampaignPlayerController::EscapeKey);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ACampaignPlayerController::EnterKey);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACampaignPlayerController::SpaceKey);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACampaignPlayerController::ToggleMusic);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ACampaignPlayerController::OpenGuide);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ACampaignPlayerController::ToggleEffects);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACampaignPlayerController::TabKey); // (rules 84.3.2: through the key guard)
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ACampaignPlayerController::OpenForces);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACampaignPlayerController::OpenReports);
	InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ACampaignPlayerController::OpenGoals);
	InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ACampaignPlayerController::ToggleWealth);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ACampaignPlayerController::ToggleDayNight);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACampaignPlayerController::ToggleFrame);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ACampaignPlayerController::ToggleNewsreels);
	InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ACampaignPlayerController::GoToCapital);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ACampaignPlayerController::GoToCapital);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ACampaignPlayerController::ToggleWatchAll);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACampaignPlayerController::OpenScreen1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACampaignPlayerController::OpenScreen2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACampaignPlayerController::OpenScreen3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACampaignPlayerController::OpenScreen4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACampaignPlayerController::OpenScreen5);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ACampaignPlayerController::ShowReady); // (release 1.5.1: new units ready, SHOW ME)
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
	if (Action == TEXT("work:storm"))
	{
		// (Ruling 182: no storm is fought without its card.)
		OpenStormCard(Map->GetSelectedRegion(), true);
		return;
	}
	if (Action == TEXT("work:sally"))
	{
		// (The design director, 17 November: no press fights a battle whose odds have not been shown. The sortie has
		// the attack card, under its own title, with SALLY OUT and a way to call it off.)
		OpenSortieCard(Map->GetSelectedRegion());
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
	LessonPageChanged();
}

void ACampaignPlayerController::ClosePage()
{
	bPageOpen = false;
	LessonPageChanged();
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
		// (Rules 84.3.3) A line of the month card followed by its pin: lesson 6's deed, which only the screen sees.
		if (Page.Month.bValid)
		{
			LessonDeedsOwed.Emplace(TEXT("line"), true);
		}
		// A way from the list of forces to one of them on the table: the view goes there, and it is picked up or opened.
		TArray<FString> Parts;
		Action.ParseIntoArray(Parts, TEXT(":"));
		const int32 Number = Parts.Num() >= 3 ? FCString::Atoi(*Parts[2]) : INDEX_NONE;
		ClosePage();
		if (Parts.Num() >= 3 && Parts[1] == TEXT("alarm"))
		{
			// (Rules 84.2.6; the words of ruling 211, section 2.) SHOW ON MAP on the month card's warning does what a press
			// on the alarm's row does, from its first alarm (the one the warning's help names): the nearest help in hand and
			// the town centred. Without an alarm it shows the place.
			AlarmShown = 0;
			if (GetAlarm())
			{
				PressAlarm();
				return;
			}
			Parts[1] = TEXT("region");
		}
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
		else if (Parts.Num() >= 3 && Parts[1] == TEXT("sea") && Map.IsValid())
		{
			// (the month's card: news from the sea has its place on the chart)
			if (Eye && Map->GetData().Seas.IsValidIndex(Number))
			{
				const FVector2D At = Map->GetData().Seas[Number].Center;
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
		LessonPageChanged();
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
	if (bOk && Extras.SeaFight != INDEX_NONE && Map.IsValid() && Map->GetData().Seas.IsValidIndex(Extras.SeaFight))
	{
		// (The 1.5.1 patch, fleetattack.ts) A fleet sent straight at the enemy: the room is put away, and where it was fought the
		// guns are seen and heard over the water, with the ships that went down, as in the month's own fights at sea.
		if (Extras.Outcome == TEXT("fought"))
		{
			Map->AddSeaBattle(Extras.SeaFight);
			static const TCHAR* Order[] = { TEXT("bb"), TEXT("cv"), TEXT("ca"), TEXT("dd"), TEXT("ss") };
			int32 Shown = 0;
			for (const TCHAR* Class : Order)
			{
				for (const FSinking& Sinking : Extras.Sinkings)
				{
					for (int32 Each = 0; Sinking.Zone >= 0 && Sinking.Class == Class && Each < FMath::Min(Sinking.Count, 2) && Shown < 6; ++Each)
					{
						Map->AddSinking(Sinking.Zone, Sinking.Nation, Sinking.Class, Shown++);
					}
				}
			}
			Remember(Extras.Sinkings); // (Brian's memorial card: the battleships and carriers among them, by name)
			CampaignSounds::Play(this, TEXT("ship_guns"), 0.6f);
			CampaignSounds::PlayLater(this, TEXT("explosion"), 0.9f, 0.4f);
			BattleMusicUntil = FPlatformTime::Seconds() + 10.0;
		}
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			const FVector2D At = Map->GetData().Seas[Extras.SeaFight].Center;
			Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Clamp(Eye->GetZoom(), 0.42f, 0.6f));
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
		// (Brian, 24 November 2026: a raid "didnt't tell me the results of the battle".) The rules name the raid's report
		// as the page that comes next. It opens once the aircraft have been seen to fly the raid (PlayerTick), half a
		// second after the last of them is home: the flight over the map lasts eleven seconds (CampaignMapActor,
		// FlightLasts). A run of pictures or of steps has it only when it asks for it by name ("-TheaterRaidCard"):
		// the runs there are were timed without it, and a page opening by itself would lie over their pictures.
		FString Asked;
		const bool bRun = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Asked, false) || FParse::Value(FCommandLine::Get(), TEXT("TheaterSteps="), Asked, false);
		if (Next.StartsWith(TEXT("airreport:")) && (!bRun || FParse::Param(FCommandLine::Get(), TEXT("TheaterRaidCard"))))
		{
			RaidCardDue = Next;
			RaidCardAt = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 11.5;
			RaidCardTurn = Session->GetView().Turn;
		}
		ClosePage();
		return;
	}
	if (bOk && Next == TEXT("close"))
	{
		// (Version 64) Two corps folded into one: the card is put away, and the army is in hand again as it now is.
		ClosePage();
		if (ArmyRegion != INDEX_NONE)
		{
			PickUpArmyIn(ArmyRegion);
		}
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
	LessonPageChanged();
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

float ACampaignPlayerController::GetPaper() const
{
	return Map.IsValid() ? Map->GetPaper() : 0.0f;
}

float ACampaignPlayerController::GetMoonlight() const
{
	return Map.IsValid() ? Map->GetMoonlight() : 0.0f;
}

void ACampaignPlayerController::GetMapLetters(TArray<FMapLetter>& Out) const
{
	THEATER_SPEED_SCOPE(GetMapLetters);
	Out.Reset();
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Map.IsValid() || !GetSession() || !Eye || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	// Countries' names are for the view from high up, and fade out on the way down. The seas' are there, faint, at
	// working height and above; just below it they begin to go, and by the close view they are gone (the art
	// director's ruling: close to, one letter of a sea's name was a pale patch on the water beside a ship).
	const float Distance = Eye->GetDistance();
	const float Countries = FMath::GetMappedRangeValueClamped(FVector2f(10500.0f, 12500.0f), FVector2f(0.0f, 0.92f), Distance);
	const float Seas = FMath::GetMappedRangeValueClamped(FVector2f(2100.0f, 4400.0f), FVector2f(0.0f, 0.6f), Distance);
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const TheaterSpeed::FEye OneView(this); // (the view worked out once for every letter: see TheaterSpeed)
	auto OnScreen = [this, &OneView](const FVector2D& MapPoint, double Lift, FVector2D& OutScreen)
	{
		return OneView.Project(Map->SurfacePoint(MapPoint.X, MapPoint.Y, Lift), OutScreen);
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
			Name.bShadowed = Word.bOverRoofs;
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
			if (Word.bSea && Map->ChartLevelOf(Word.Sea) < 2)
			{
				Letter.bInk = true;
				Letter.Alpha = FMath::Min(Alpha * 1.5f, 0.92f);
			}
		}
	}
}

void ACampaignPlayerController::GetColoursOnScreen(TArray<FBox2D>& Out) const
{
	THEATER_SPEED_SCOPE(GetColoursOnScreen);
	Out.Reset();
	if (!Map.IsValid() || !PlayerCameraManager || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	const TheaterSpeed::FEye OneView(this); // (the view worked out once for every point: see TheaterSpeed)
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const FVector Up = PlayerCameraManager->GetCameraRotation().Quaternion().GetUpVector();
	for (const TObjectPtr<AArmyPiece>& Piece : Map->GetPieces())
	{
		FVector Middle;
		double Radius = 0.0;
		FVector2D At, Top;
		if (Piece && Piece->GetColours(Middle, Radius) && OneView.Project(Middle, At) && OneView.Project(Middle + Up * Radius, Top)
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
	THEATER_SPEED_SCOPE(GetTownPlates);
	Out.Reset();
	const FCampaignSession* Session = GetSession();
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Map.IsValid() || !Session || !Eye || !PlayerCameraManager || Opening.bVisible || Newsreel.bOn)
	{
		return;
	}
	const TheaterSpeed::FEye OneView(this); // (the view worked out once for every point: see TheaterSpeed)
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
		if (!OneView.Project(Spot.World + Toward * Spot.Reach, Screen)
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
		Plate.Region = Spot.Region;
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
		if (OneView.Project(Map->MapToWorld(Place.X, Place.Y, 0.0) + FVector(0.0, 0.0, 3.0 * Map->UnitScale), Screen)
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
	// (Brian's ask of 15 November 2026; rulings 46 and 57.) A city of the player's with a unit on order wears one
	// badge, directly above its name plate: the sign of the unit ready soonest, how many of it are on order there,
	// a plus where other kinds are too, and the months to go in a brass disc. Pulling back it loses the count, then
	// all but the sign, and from far off it is put away, as the harbour badge is.
	if (State >= 1)
	{
		const int32 Stage = Distance <= 5500.0f ? 0 : Distance <= 8500.0f ? 1 : 2;
		for (const FCityOrders& City : View.CityOrders)
		{
			if (!Data.Regions.IsValidIndex(City.Region) || City.Sign.IsEmpty())
			{
				continue;
			}
			FTownPlate* Wears = Out.FindByPredicate([&City](const FTownPlate& Each) { return Each.Region == City.Region && !Each.Name.IsEmpty(); });
			if (!Wears)
			{
				// (The art director's round 35: a badge hung where its city's name plate is not shown floated alone, and
				// Bielefeld's fighter read as an airfield. A badge goes away with its plate.)
				continue;
			}
			Wears->bOrders = true;
			Wears->Sign = City.Sign;
			Wears->bStar = City.bStar;
			Wears->bSpied = City.bSpied;
			Wears->OrderCount = City.Count;
			Wears->bPlus = City.bPlus;
			Wears->Months = City.Months;
			Wears->Stage = Stage;
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
			if (Field.Groups <= 0 || !Map->GetAirfieldWorld(Field.Region, World) || !OneView.Project(World, Screen)
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
		// Ships lying in harbour wear a small badge on their port in the same way: an anchor and how many ships
		// are in, all the fleets of one nation in that port together. (The ships themselves lie small at anchor
		// beside it, and a click on them takes that fleet in hand.)
		TMap<FString, FTownPlate> Harbours;
		for (const TPair<int32, TObjectPtr<AArmyPiece>>& Pair : Map->GetFleetPieces())
		{
			const AArmyPiece* Fleet = Pair.Value;
			FVector2D Screen;
			if (!Fleet || !Fleet->IsAtAnchor() || Fleet->IsHidden() || !OneView.Project(Fleet->GetRestLocation(), Screen)
				|| Screen.X < -40.0 || Screen.X > Width + 40.0 || Screen.Y < -20.0 || Screen.Y > Height + 20.0)
			{
				continue;
			}
			const FString Key = FString::Printf(TEXT("%d|%s"), Fleet->GetFleetBase(), *Fleet->GetNationTag());
			if (FTownPlate* Known = Harbours.Find(Key))
			{
				Known->Rank += Fleet->GetFleetShips(); // (kept here until the badges are handed over)
				Known->At.Y = FMath::Min(Known->At.Y, Screen.Y - 30.0);
				continue;
			}
			FTownPlate& Badge = Harbours.Add(Key);
			Badge.At = Screen - FVector2D(0.0, 30.0);
			Badge.bHarbour = true;
			Badge.Rank = Fleet->GetFleetShips();
			if (const FCampaignNation* Nation = Data.FindNation(Fleet->GetNationTag()))
			{
				Badge.Colour = FLinearColor(Nation->Color);
			}
		}
		for (TPair<FString, FTownPlate>& Pair : Harbours)
		{
			Pair.Value.Name = FString::FromInt(Pair.Value.Rank);
			Pair.Value.Rank = -1;
			Out.Add(Pair.Value);
		}
	}
	// (Ruling 86; the art director's brief.) The sea gates: the Panama Canal and the three straits. One mark each,
	// on the water at its narrowest: the holder's flag and a lamp, green where the player's ships may pass and red
	// where they may not. Close up, where names show, it has its name; from far up it is the lamp alone. They are
	// handed over last, so that they step clear of everything else.
	{
		const int32 GateStage = Distance <= 5500.0f ? 0 : Distance <= 12500.0f ? 1 : 2;
		for (const FViewGate& Gate : View.Gates)
		{
			FVector2D Screen;
			if (!OneView.Project(Map->MapToWorld(Gate.At.X, Gate.At.Y, 0.0), Screen)
				|| Screen.X < -40.0 || Screen.X > Width + 40.0 || Screen.Y < -20.0 || Screen.Y > Height + 20.0)
			{
				continue;
			}
			FTownPlate& Mark = Out.AddDefaulted_GetRef();
			Mark.At = Screen;
			Mark.Name = Gate.Name;
			Mark.bGate = true;
			Mark.bGateOpen = Gate.bOpen;
			Mark.Gate = Gate.Id;
			Mark.Flag = Gate.Holder;
			Mark.Stage = GateStage;
			Mark.Rank = -2;
		}
	}
	// A crowded province shows one flag: beside it, a small count of the armies put away behind it. (Point at the
	// province and they fan out.) At any height, since the armies it stands for are hidden at any height.
	{
		TArray<TPair<AArmyPiece*, int32>> Crowds;
		Map->GetCrowds(Crowds);
		const FVector Right = PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
		for (const TPair<AArmyPiece*, int32>& Crowd : Crowds)
		{
			FVector Middle;
			double Radius = 0.0;
			FVector2D Screen;
			if (Crowd.Key->GetColours(Middle, Radius) && OneView.Project(Middle + Right * Radius * 0.95, Screen)
				&& Screen.X > -40.0 && Screen.X < Width + 40.0 && Screen.Y > -20.0 && Screen.Y < Height + 20.0)
			{
				FTownPlate& Count = Out.AddDefaulted_GetRef();
				Count.At = Screen + FVector2D(16.0, -9.0);
				Count.Name = FString::Printf(TEXT("+%d"), Crowd.Value);
				Count.bCrowd = true;
				Count.Rank = 900;
			}
		}
	}
}

int32 ACampaignPlayerController::GateUnder(const FVector2D& Pointer) const
{
	for (const TPair<int32, FBox2D>& Mark : GateMarks)
	{
		if (Mark.Value.ExpandBy(3.0).IsInside(Pointer))
		{
			return Mark.Key;
		}
	}
	return INDEX_NONE;
}

void ACampaignPlayerController::StageSea(const FString& NameBegins)
{
	StagedSea = INDEX_NONE;
	if (Map.IsValid() && !NameBegins.IsEmpty())
	{
		const TArray<FCampaignSeaZone>& Seas = Map->GetData().Seas;
		for (int32 Zone = 0; Zone < Seas.Num(); ++Zone)
		{
			if (Seas[Zone].Name.StartsWith(NameBegins))
			{
				StagedSea = Zone;
				break;
			}
		}
	}
}

void ACampaignPlayerController::StageGate(const FString& NameBegins)
{
	StagedGate = INDEX_NONE;
	if (const FCampaignSession* Session = GetSession())
	{
		for (const FViewGate& Gate : Session->GetView().Gates)
		{
			if (!NameBegins.IsEmpty() && Gate.Name.StartsWith(NameBegins))
			{
				StagedGate = Gate.Id;
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
	else if (Screen.StartsWith(TEXT("siege:")) && Map.IsValid())
	{
		// (Brian, 18 November 2026.) A town he besieges: the view goes there and the army round it is taken in hand,
		// so that its tray, with STORM lit on it, is what he sees. With no army to take up, the province's orders.
		const int32 Where = FCString::Atoi(*Screen.Mid(6));
		if (Map->GetData().Regions.IsValidIndex(Where))
		{
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
			{
				const FVector2D At = Map->GetData().Regions[Where].Center;
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), 0.3f);
			}
			PutDownFleet();
			SelectRegion(Where);
			if (PickUpArmyIn(Where))
			{
				CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
			}
			else
			{
				SetProvinceTab(2);
			}
		}
	}
	else if (Screen.StartsWith(TEXT("field:")) && Map.IsValid())
	{
		// (Version 86: a crowded airfield's row. The view goes to the field and its page is opened, as OPEN THE AIRFIELD does.)
		const int32 Where = FCString::Atoi(*Screen.Mid(6));
		if (Map->GetData().Regions.IsValidIndex(Where))
		{
			if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
			{
				const FVector2D At = Map->GetData().Regions[Where].Center;
				Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
			}
			PutDownFleet();
			SelectRegion(Where);
			OpenPage(TEXT("airfield"));
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

const FHomeAlarm* ACampaignPlayerController::GetAlarm() const
{
	const FCampaignSession* Session = GetSession();
	const bool bShown = Session && Session->GetView().bHumanUp && !bWatching && !Opening.bVisible && Session->GetView().Alarms.Num() > 0;
	return bShown ? &Session->GetView().Alarms[AlarmShown % Session->GetView().Alarms.Num()] : nullptr;
}

int32 ACampaignPlayerController::GetAlarmCount() const
{
	return GetAlarm() ? GetSession()->GetView().Alarms.Num() : 0;
}

int32 ACampaignPlayerController::GetAlarmPlace() const
{
	const int32 Count = GetAlarmCount();
	return Count > 0 ? AlarmShown % Count + 1 : 0;
}

void ACampaignPlayerController::PressAlarm()
{
	FCampaignSession* Session = GetSession();
	const FHomeAlarm* Shown = GetAlarm();
	if (!Session || !Map.IsValid() || !Shown || FaceOff.bVisible || WarPrompt.bVisible)
	{
		return;
	}
	// (a copy: putting an army in hand reads the view again)
	const FHomeAlarm Alarm = *Shown;
	const int32 Count = Session->GetView().Alarms.Num();
	AlarmShown = Count > 0 ? (AlarmShown % Count + 1) % Count : 0; // (several at once: each press goes on to the next)
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	// (The words of ruling 211, section 1.) A press picks up that army, and the paper says the cure in one line, in the
	// rules' words. Where no army can come a press centres the town, and nothing more. No new mark is drawn on the map.
	if (Alarm.HelpRegion != INDEX_NONE && PickUpArmyIn(Alarm.HelpRegion))
	{
		// (the rules name the corps a press picks up: the hand is those of them that can take orders now)
		if (Alarm.HelpCorps.Num() > 0 && PickedCorps.ContainsByPredicate([&Alarm](int32 Id) { return Alarm.HelpCorps.Contains(Id); }))
		{
			PickedCorps.RemoveAll([&Alarm](int32 Id) { return !Alarm.HelpCorps.Contains(Id); });
			RefreshOrders();
		}
		CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
		if (!Alarm.Hand.IsEmpty())
		{
			Notify(Alarm.Hand, 9.0);
		}
	}
	if (Map->GetData().Regions.IsValidIndex(Alarm.Region))
	{
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			// (Rules 84.3; the second Playtester's 84.2.6 find.) The place and where the paper says to right-click are
			// both in view: the camera stands between them.
			const FVector2D Place = Map->GetData().Regions[Alarm.Region].Center;
			const FVector2D Click = Alarm.HelpRegion != INDEX_NONE && Map->GetData().Regions.IsValidIndex(Alarm.SendTo) ? Map->GetData().Regions[Alarm.SendTo].Center : Place;
			const FVector2D At = (Place + Click) * 0.5;
			Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
		}
	}
}

const FGunsAlone* ACampaignPlayerController::GetGunsAlone(int32 Index) const
{
	const FCampaignSession* Session = GetSession();
	const bool bShown = Session && Session->GetView().bHumanUp && !bWatching && !Opening.bVisible && Session->GetView().Alone.IsValidIndex(Index);
	return bShown ? &Session->GetView().Alone[Index] : nullptr;
}

void ACampaignPlayerController::PressGunsAlone(int32 Index)
{
	const FGunsAlone* Shown = GetGunsAlone(Index);
	FCampaignSession* Session = GetSession();
	if (!Session || !Shown || !Map.IsValid() || !Map->GetData().Regions.IsValidIndex(Shown->Region) || FaceOff.bVisible || WarPrompt.bVisible)
	{
		return;
	}
	const FGunsAlone Row = *Shown; // (a copy: putting an army in hand reads the view again)
	const FCampaignMapData& Data = Map->GetData();
	// (Ruling 211(a) as passed at 16:54 UTC: a press centres the town.)
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	// (Rules 84.3; the Playtester's 84.2.6 find.) On a red row a press picks up the infantry or tanks of his that can
	// reach the guns this month, not the guns, and the paper says the cure in one line, as the red home alarm does.
	int32 Other = Row.Region;
	if (Row.bRed && Data.Regions.IsValidIndex(Row.From) && PickUpArmyIn(Row.From))
	{
		Other = Row.From;
		if (Row.HelpCorps.Num() > 0 && PickedCorps.ContainsByPredicate([&Row](int32 Id) { return Row.HelpCorps.Contains(Id); }))
		{
			PickedCorps.RemoveAll([&Row](int32 Id) { return !Row.HelpCorps.Contains(Id); });
			RefreshOrders();
		}
		CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
		// (the browser's words: a lone corps by its name, more than one as the army where they stand)
		FString Who = FString::Printf(TEXT("The army at %s"), *Data.Regions[Row.From].Name);
		if (PickedCorps.Num() == 1)
		{
			for (const FViewCorps& Unit : Session->GetView().Corps)
			{
				if (Unit.Id == PickedCorps[0])
				{
					Who = Unit.Name;
					break;
				}
			}
		}
		Notify(FString::Printf(TEXT("%s is in hand. Right-click %s to send it."), *Who, *Data.Regions[Row.Region].Name), 9.0);
	}
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		// (Both the town and the army in hand are in view: the second Playtester's 84.2.6 find.)
		const FVector2D At = (Data.Regions[Row.Region].Center + Data.Regions[Other].Center) * 0.5;
		Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
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
	// The same key puts away whichever of the Guide's pages is open.
	if (bLessonGuideOpen)
	{
		CloseLessonGuide();
		return;
	}
	if (Document.bValid && Document.Title == TEXT("How to play"))
	{
		CloseDocument();
		return;
	}
	if (IsAnswering())
	{
		return; // (Speed, b414 mark 1, as Enter: opening either page asks the rules, and an order is still being answered)
	}
	// (Rules 84.3.3; ruling 147.) In a war he leads, the Guide opens on its list of the ten lessons: the typed page of
	// rules is its second, behind "How a month works".
	if (!OpenLessonGuide())
	{
		OpenHowToPlay();
	}
}

bool ACampaignPlayerController::OpenLessonGuide()
{
	FCampaignSession* Session = GetSession();
	if (!Session || Session->GetView().Human.IsEmpty())
	{
		return false;
	}
	FLessonGuide Guide = Session->LessonGuide(LessonCardOpen(), PickedCorps);
	if (!Guide.bValid)
	{
		return false;
	}
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	CloseArsenal();
	CloseLedger();
	CloseDiplomacy();
	CloseResearch();
	CloseGenerals();
	CloseDocument();
	LessonGuidePage = MoveTemp(Guide);
	bLessonGuideOpen = true;
	++LessonGuideRevision;
	GuideOverPage = bPageOpen ? Page.Id : FString(); // (a card open under it stays; another opened after takes its place)
	return true;
}

void ACampaignPlayerController::RefreshLessonGuide()
{
	FCampaignSession* Session = GetSession();
	if (!bLessonGuideOpen || !Session)
	{
		return;
	}
	FLessonGuide Guide = Session->LessonGuide(LessonCardOpen(), PickedCorps);
	if (Guide.bValid)
	{
		LessonGuidePage = MoveTemp(Guide);
		++LessonGuideRevision;
	}
}

void ACampaignPlayerController::PressGuide(const FString& Action)
{
	if (!bLessonGuideOpen)
	{
		return;
	}
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	if (Action.StartsWith(TEXT("back:")))
	{
		// (a row waiting for him: its note comes back, and the Guide closes on it, as the browser's does)
		bLessonGuideOpen = false;
		++LessonGuideRevision;
	}
	// (Asked as the note's own presses are: at once where no order is being answered, else once it is. The list is
	// read again with the answer.)
	LessonActsOwed.Add(Action);
	bLessonsDue = true;
	if (Afterwards.Num() == 0 && !IsAnswering())
	{
		AskLessons();
	}
}

void ACampaignPlayerController::CloseLessonGuide()
{
	if (!bLessonGuideOpen)
	{
		return;
	}
	bLessonGuideOpen = false;
	++LessonGuideRevision;
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	bLessonsDue = true; // (the notes stood aside while it was open: the desk is asked again)
}

void ACampaignPlayerController::OpenHowToPlay()
{
	EndNewsreel();
	if (bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible)
	{
		return;
	}
	if (bLessonGuideOpen)
	{
		bLessonGuideOpen = false; // (from the list's "How a month works": the typed page takes its place)
		++LessonGuideRevision;
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
		TEXT("Every flag is an army, a fleet or an air fleet, and the number under it is its strength. Click any province to see who holds it and what it has."));
	Add(TEXT("A MONTH"),
		TEXT("Each turn is one month. Give your orders, then press END THE MONTH, or Enter. The other nations move, and the month's news is shown at the top right. ")
		TEXT("Every corps has a few moves each month: marching through friendly ground costs one, and entering enemy ground costs more."));
	Add(TEXT("MOVING AND ATTACKING"),
		TEXT("Click one of your flags to pick the army up. Ground it can march to lightens, ground it can attack turns red, and a neutral country's ground beside it turns amber. A dashed red line marks how deep the enemy you can see could strike next month. ")
		TEXT("Click where it should go and click again to send it, or drag the flag there. An attack first shows both armies and the odds: SEND THEM IN fights it, CALL IT OFF does not. Esc puts the army down.\n")
		TEXT("NEXT ARMY (Tab), under END THE MONTH, goes round every army that still waits for an order, so that none is missed; STAND THIS MONTH on an army's card tells it to wait where it is. ")
		TEXT("Right-click friendly ground too far for this month and the army marches there by itself over the months, halting if the enemy comes near. An army with no moves left can be sent on the same way: it sets off as the next month opens.\n")
		TEXT("In the army's tray you can leave some corps behind, say what it does if attacked (stand, dig in or fall back), and send it BY SEA from a port. Beyond its march, a right-click on a place with a station dot sends it by rail along the lit lines, which takes its whole month.\n")
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
		TEXT("The ARSENAL sells corps, aircraft, ships and works such as factories, shipyards, airfields and anti-aircraft guns. Click one of your own provinces first, so the game knows where to build. Works you build stand on the map."));
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
		TEXT("ORDERS opens everything else: every sea in reach with the enemy known there, the shipping lanes it could escort and what that would save, and the way to split ships off as a fleet of their own or join another fleet. ")
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

void ACampaignPlayerController::EnterKey()
{
	if (IsAnswering())
	{
		return; // (Speed, b414 mark 1: an order is still being answered, and a hurried second press is not another order)
	}
	// (Rules 84.3.1; the second playtester's check of 84.3.) The map's own Enter never takes the press that put a card
	// down, nor one hurried after it: Enter on CALL IT OFF or Esc, and a second Enter at once, opened the question
	// before the month's end. A card put down by the mouse leaves it free.
	const bool bCardUp = IsCardUp();
	if (!bCardUp && !bScripted && FPlatformTime::Seconds() - KeyShutAt < 0.5)
	{
		return;
	}
	bEnterByKey = true;
	EndMonth();
	bEnterByKey = false;
	if (bCardUp)
	{
		KeyShutAt = FPlatformTime::Seconds();
	}
}

void ACampaignPlayerController::EscapeKey()
{
	if (IsAnswering())
	{
		return; // (as Enter)
	}
	const bool bCardUp = IsCardUp();
	Escape();
	if (bCardUp)
	{
		KeyShutAt = FPlatformTime::Seconds(); // (the map's Enter waits the key guard after it)
	}
}

void ACampaignPlayerController::SpaceKey()
{
	if (IsAnswering())
	{
		return; // (as Enter)
	}
	const bool bCardUp = IsCardUp();
	SpaceBar();
	if (bCardUp)
	{
		KeyShutAt = FPlatformTime::Seconds(); // (the map's Enter waits the key guard after it)
	}
}

void ACampaignPlayerController::TabKey()
{
	if (IsAnswering())
	{
		return; // (as Enter)
	}
	// (Rules 84.3.2; the second playtester's check of 84.3.1.) Tab on the question before the month's end is its SHOW ME
	// and puts the question down by a key: the map's own Enter then waits the guard's half second, so Tab and a hurried
	// Enter never end the month the question was asked about.
	const bool bCardUp = IsCardUp();
	NextArmy();
	if (bCardUp && !IsCardUp())
	{
		KeyShutAt = FPlatformTime::Seconds();
	}
}

void ACampaignPlayerController::SpaceBar()
{
	if (FaceOff.bVisible && FaceOff.bResolved && !bScripted)
	{
		if (FPlatformTime::Seconds() - FaceOffSince > 0.5)
		{
			CloseFaceOff(); // CONTINUE
		}
		return;
	}
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

const FArmNote* ACampaignPlayerController::FindArm(const FString& Kind) const
{
	const FCampaignSession* Session = GetSession();
	return Session ? Session->GetView().FindArm(Kind) : nullptr;
}

void ACampaignPlayerController::ReadKeptViews()
{
	if (bKeptViewsRead)
	{
		return;
	}
	bKeptViewsRead = true;
	// (kept in the game's own settings file, so that they are there again the next time it is opened)
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		FString Line;
		TArray<FString> Parts;
		if (GConfig && GConfig->GetString(TEXT("TheaterCommand.Views"), *FString::Printf(TEXT("View%d"), Slot + 6), Line, GGameUserSettingsIni) && Line.ParseIntoArray(Parts, TEXT(",")) == 5)
		{
			FKeptView& View = KeptViews[Slot];
			View.bKept = true;
			View.Focus = FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), 0.0);
			View.Zoom = FCString::Atof(*Parts[2]);
			View.Yaw = FCString::Atof(*Parts[3]);
			View.Tilt = FCString::Atof(*Parts[4]);
		}
	}
}

void ACampaignPlayerController::KeepView(int32 Slot)
{
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || Slot < 0 || Slot >= 4)
	{
		return;
	}
	ReadKeptViews();
	FKeptView& View = KeptViews[Slot];
	View.bKept = true;
	View.Focus = Eye->GetFocus();
	View.Zoom = Eye->GetZoom();
	View.Yaw = Eye->GetYaw();
	View.Tilt = Eye->GetTilt();
	if (GConfig)
	{
		GConfig->SetString(TEXT("TheaterCommand.Views"), *FString::Printf(TEXT("View%d"), Slot + 6),
			*FString::Printf(TEXT("%.1f,%.1f,%.4f,%.2f,%.2f"), View.Focus.X, View.Focus.Y, View.Zoom, View.Yaw, View.Tilt), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	CampaignSounds::Play(this, TEXT("stamp"), 0.4f);
	Notify(FString::Printf(TEXT("This view is kept under %d. Press %d to come back to it."), Slot + 6, Slot + 6), 4.0);
}

void ACampaignPlayerController::GoToKeptView(int32 Slot)
{
	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || Slot < 0 || Slot >= 4)
	{
		return;
	}
	ReadKeptViews();
	const FKeptView& View = KeptViews[Slot];
	if (!View.bKept)
	{
		Notify(FString::Printf(TEXT("No view is kept under %d. Hold Shift and press %d to keep this one."), Slot + 6, Slot + 6), 5.0);
		return;
	}
	Eye->SetHeading(View.Yaw, View.Tilt);
	Eye->GoTo(View.Focus, View.Zoom);
}

int32 ACampaignPlayerController::ArmiesYetToMove() const
{
	// (Ruling 102(b).) Every number of the round is the rules' own, so that both builds count alike.
	const FCampaignSession* Session = GetSession();
	return Session && !bWatching && !Opening.bVisible ? Session->GetView().Round.All : 0;
}

int32 ACampaignPlayerController::ArmiesIdleAtFront() const
{
	const FCampaignSession* Session = GetSession();
	return Session && !bWatching && !Opening.bVisible ? Session->GetView().Round.Bright : 0;
}

int32 ACampaignPlayerController::ArmiesInRear() const
{
	const FCampaignSession* Session = GetSession();
	return Session && !bWatching && !Opening.bVisible ? Session->GetView().Round.Quiet : 0;
}

bool ACampaignPlayerController::IsRoundNewlyRaised() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().Round.Bright > 0 && Session->GetView().Round.Front <= 0;
}

FString ACampaignPlayerController::GetRoundWords() const
{
	const FCampaignSession* Session = GetSession();
	return Session ? Session->GetView().Round.Words : FString();
}

FString ACampaignPlayerController::GetRoundQuestion() const
{
	const FCampaignSession* Session = GetSession();
	return Session ? Session->GetView().Round.Question : FString();
}

bool ACampaignPlayerController::IsRoundMixed() const
{
	const FCampaignSession* Session = GetSession();
	return Session && Session->GetView().Round.Front > 0 && Session->GetView().Round.Bright > Session->GetView().Round.Front;
}

void ACampaignPlayerController::StageGuns(int32 RegionId, int32 Level)
{
	if (FCampaignSession* Session = GetSession())
	{
		Session->StageGuns(RegionId, Level);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
}

FString ACampaignPlayerController::StageRecon(int32 RegionId)
{
	FString Said;
	if (FCampaignSession* Session = GetSession())
	{
		Said = Session->StageRecon(RegionId);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
	return Said;
}

void ACampaignPlayerController::StageCut(int32 RegionId, const FString& Cause, const FString& How)
{
	if (FCampaignSession* Session = GetSession())
	{
		Session->StageCut(RegionId, Cause, How);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
}

void ACampaignPlayerController::StageAgent(int32 RegionId, const FString& Mission)
{
	if (FCampaignSession* Session = GetSession())
	{
		Session->StageAgent(RegionId, Mission);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
}

void ACampaignPlayerController::StageCaught(int32 RegionId)
{
	if (FCampaignSession* Session = GetSession())
	{
		Session->StageCaught(RegionId);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
}

const FViewAgent* ACampaignPlayerController::GetAgentInHand() const
{
	const FCampaignSession* Session = GetSession();
	return Session && AgentInHand != INDEX_NONE ? Session->GetView().Agents.FindByPredicate([this](const FViewAgent& Each) { return Each.Id == AgentInHand; }) : nullptr;
}

FString ACampaignPlayerController::GetAgentHint() const
{
	const FViewAgent* Agent = GetAgentInHand();
	if (!Agent)
	{
		return FString();
	}
	// (Ruling 169(a).) How he is sent, in the army card's own words with his time added; while he cannot be sent, the
	// rules' own reason stands there instead.
	return !Agent->Stuck.IsEmpty() ? Agent->Stuck : FString(TEXT("Right-click where the agent should go, or drag him there: a month on the road. Esc puts him down."));
}

bool ACampaignPlayerController::GetAgentInHandBox(FBox2D& OutBox) const
{
	FVector Foot = FVector::ZeroVector;
	double High = 0.0;
	FVector2D Low, Crown;
	if (AgentInHand == INDEX_NONE || !Map.IsValid() || !Map->GetAgentInHandStand(Foot, High)
		|| !ProjectWorldLocationToScreen(Foot, Low) || !ProjectWorldLocationToScreen(Foot + FVector(0.0, 0.0, High), Crown))
	{
		return false;
	}
	// (his base is about half as wide on the screen as he is tall, and the near half of it lies under his feet)
	const double Tall = FMath::Abs(Low.Y - Crown.Y);
	OutBox = FBox2D(FVector2D(Low.X - 0.5 * Tall - 6.0, FMath::Min(Low.Y, Crown.Y) - 6.0), FVector2D(Low.X + 0.5 * Tall + 6.0, FMath::Max(Low.Y, Crown.Y) + 0.3 * Tall + 6.0));
	return true;
}

bool ACampaignPlayerController::PickUpAgent(int32 AgentId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !Session->GetView().Agents.ContainsByPredicate([AgentId](const FViewAgent& Each) { return Each.Id == AgentId; }))
	{
		return false;
	}
	// Whatever else was in hand is put down, and no territory is being looked at: the card is his.
	PutDownArmy();
	PutDownFleet();
	StopLooking();
	SelectRegion(INDEX_NONE);
	RegionPanel.bVisible = false;
	RegionPanel.Revision = ++PanelRevision;
	AgentInHand = AgentId;
	Map->SetAgentInHand(AgentId);
	return true;
}

bool ACampaignPlayerController::PickUpAgentIn(int32 RegionId)
{
	const FCampaignSession* Session = GetSession();
	const FViewAgent* There = Session ? Session->GetView().Agents.FindByPredicate([RegionId](const FViewAgent& Each) { return Each.Region == RegionId; }) : nullptr;
	return There && PickUpAgent(There->Id);
}

void ACampaignPlayerController::PutDownAgent()
{
	if (AgentInHand == INDEX_NONE)
	{
		return;
	}
	AgentInHand = INDEX_NONE;
	bAgentHeld = false;
	if (Map.IsValid())
	{
		Map->SetAgentInHand(INDEX_NONE);
		Map->SetWatchEdge(INDEX_NONE);
	}
}

void ACampaignPlayerController::SendAgentTo(int32 RegionId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || AgentInHand == INDEX_NONE || !Map->GetData().Regions.IsValidIndex(RegionId))
	{
		return;
	}
	Map->ReturnAgent();
	// (Ruling 163: an agent may go anywhere but his own province and a dormant country's ground. Where the rules
	// refuse, the reason is theirs, and he stays in hand.)
	const FString Refused = Session->WhyNotSendAgent(AgentInHand, RegionId);
	if (!Refused.IsEmpty())
	{
		Notify(Refused, 6.0);
		return;
	}
	FString Note;
	if (Session->Act(FString::Printf(TEXT("spy:send:%d"), AgentInHand), RegionId, Note))
	{
		PutDownAgent();
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 6.0);
	}
}

void ACampaignPlayerController::SetAgentWork(const FString& Act)
{
	FCampaignSession* Session = GetSession();
	if (!Session || AgentInHand == INDEX_NONE || Act.IsEmpty())
	{
		return;
	}
	FString Note;
	if (Session->Act(Act, INDEX_NONE, Note))
	{
		ShowWar(Session->GetView(), TArray<FCorpsMove>()); // (his work changes what he sees, and so what the map shows)
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 6.0);
	}
}

void ACampaignPlayerController::StandThisMonth()
{
	FCampaignSession* Session = GetSession();
	if (!Session || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0)
	{
		return;
	}
	// (The order is the rules' own, kept in the game and so in its saves: a save moves between the two builds.)
	const bool bWas = bHandStands;
	bHandStands = Session->Stand(ArmyRegion, !bWas) == TEXT("all");
	++Tray.Revision;
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
	Notify(bWas ? TEXT("The order to stand is taken back: this army waits for orders again.") : TEXT("This army stands where it is this month. NEXT ARMY passes it by until the next."), 5.0);
}

bool ACampaignPlayerController::IsStandingThisMonth() const
{
	return bHandStands && ArmyRegion != INDEX_NONE;
}

void ACampaignPlayerController::AnswerEndAsk(bool bShowMe)
{
	if (!EndAsk.bVisible)
	{
		return;
	}
	EndAsk.bVisible = false;
	if (bShowMe)
	{
		NextArmy();
	}
	else
	{
		EndMonth();
	}
}

void ACampaignPlayerController::ToggleEndAskWanted()
{
	bAskAtMonthsEnd = !bAskAtMonthsEnd;
	SaveOptions();
}

void ACampaignPlayerController::ShowEndAsk()
{
	EndAsk.bVisible = true;
	EndAsk.Count = FMath::Max(GetSession() ? GetSession()->GetView().Round.Bright : 0, 1);
	EndAsk.Raised = GetSession() ? FMath::Max(GetSession()->GetView().Round.Bright - GetSession()->GetView().Round.Front, 0) : 0;
	EndAsk.Since = FPlatformTime::Seconds();
}

bool ACampaignPlayerController::IsMarchSlipShown() const
{
	// (One voice after a press.) The first lesson for an army picked up with no moves left: never the result of an
	// order, and any order to that army puts it away.
	// (not for corps inside a town: they march nowhere, this month or next, until they take the field)
	return bHandSpent && !bHandOrdered && !bMarchSlipDone && ArmyRegion != INDEX_NONE && !bBySea && !bByRail && Tray.bVisible && !Tray.bFleet && !Tray.bLook && Tray.FieldButton.IsEmpty();
}

void ACampaignPlayerController::DismissMarchSlip()
{
	if (!bMarchSlipDone)
	{
		bMarchSlipDone = true;
		SaveOptions();
		CampaignSounds::Play(this, TEXT("click"), 0.5f);
	}
}

const FMarchPlan& ACampaignPlayerController::PlanFor(int32 RegionId)
{
	if (RegionId != KeptPlanRegion)
	{
		KeptPlanRegion = RegionId;
		FCampaignSession* Session = GetSession();
		KeptPlan = (Session && RegionId != INDEX_NONE && ArmyRegion != INDEX_NONE && RegionId != ArmyRegion) ? Session->PlanMarch(PickedCorps, RegionId, HandArmy, HandArmyIds) : FMarchPlan();
	}
	return KeptPlan;
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

void ACampaignPlayerController::SpeakLesson(const FLessonNote& Note)
{
	// (Rules 84.3.3; the design director's brief: "the same notes in the 3D game from the same list, read aloud by the
	// built-in voice when the voice is on". The deed line is read last.) A tip is its one line; the closing note its
	// heading and sentences. Each part ends as a sentence does, so that the voice stops between them.
	FString Words;
	auto Add = [&Words](const FString& Part)
	{
		const FString Said = Part.TrimStartAndEnd();
		if (Said.IsEmpty())
		{
			return;
		}
		Words += (Words.IsEmpty() ? TEXT("") : TEXT(" ")) + Said;
		if (!Said.EndsWith(TEXT(".")) && !Said.EndsWith(TEXT("?")) && !Said.EndsWith(TEXT("!")) && !Said.EndsWith(TEXT(":")))
		{
			Words += TEXT(".");
		}
	};
	if (Note.Kind != TEXT("tip"))
	{
		Add(Note.Head);
	}
	Add(Note.Text);
	if (Note.Kind == TEXT("lesson"))
	{
		Add(Note.Deed);
	}
	// (A note is said at once, cutting short whatever was being said; the lines waiting keep their turn after it.)
	Speak(Words, TEXT("Daniel"), 176, 1);
}

bool ACampaignPlayerController::IsFrameHidden() const
{
	// ("-TheaterNoFrame" takes a run's pictures with the frame put away)
	static const bool bAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterNoFrame"));
	return bFrameHidden || bAsked;
}

void ACampaignPlayerController::ToggleFrame()
{
	// (only over the table itself: a page, the face-off or a newsreel is not put away)
	if (!bFrameHidden && (!IsMapShown() || FaceOff.bVisible || bWatching))
	{
		return;
	}
	bFrameHidden = !bFrameHidden;
	FrameHintUntil = bFrameHidden ? FPlatformTime::Seconds() + 3.0 : 0.0;
}

void ACampaignPlayerController::StartMoment(int32 RegionId)
{
	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye || !Map.IsValid() || !Map->GetData().Regions.IsValidIndex(RegionId) || bMomentOn)
	{
		return;
	}
	bMomentOn = true;
	MomentHome = Eye->GetFocus();
	MomentHomeZoom = Eye->GetZoom();
	MomentUntil = FPlatformTime::Seconds() + 3.2;
	const FVector2D Place = Map->GetData().Regions[RegionId].Center;
	// Down to the front: low over the place, where the smoke of the battle is still rising. It goes down in about a
	// second, not at the snap of an ordinary move.
	Eye->SetGlide(2.6f);
	MomentGlideUntil = 0.0;
	Eye->GoTo(Map->MapToWorld(Place.X, Place.Y, 0.0), FMath::Min(Eye->GetZoom(), 0.2f));
}

void ACampaignPlayerController::EndMoment()
{
	if (!bMomentOn)
	{
		return;
	}
	bMomentOn = false;
	// The eye is handed back where it was, and as high, at the same unhurried pace; then it is quick again.
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->GoTo(MomentHome, MomentHomeZoom);
		MomentGlideUntil = FPlatformTime::Seconds() + 1.5;
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
	// One key steps through the views of the map: as it is, the countries in flat colour, by wealth, by friend and
	// foe, and (release 1) the admiral's chart.
	const int32 Now = bChartView ? 4 : Map->IsWealthShown() ? 2 : Map->AreSidesShown() ? 3 : Map->AreColoursFlat() ? 1 : 0;
	const int32 Next = (Now + 1) % 5;
	Map->SetFlatColours(Next == 1);
	Map->ShowWealth(Next == 2);
	Map->ShowSides(Next == 3, Session->GetView().RegionStanding);
	bChartView = Next == 4;
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	// (where this game keeps no fog at sea, every sea is in full view and the chart has nothing to veil)
	const bool bFogAtSea = Session->GetView().SeaSearch.ContainsByPredicate([](int32 Level) { return Level < 3; });
	Notify(Next == 1 ? TEXT("Flat colours: every country painted whole in its own colour. Press Y for the wealth view.")
		: Next == 2 ? TEXT("The wealth view: the deeper the gold, the more a province pays. Press Y for friend and foe.")
		: Next == 3 ? TEXT("Friend and foe: your own ground green, your allies' blue, your enemies' red. Press Y for the admiral's chart.")
		: Next == 4 ? (bFogAtSea ? TEXT("The admiral's chart: a sea you have not searched lies under a veil, a lighter one where the search is thin, and a red ring marks where enemy ships were last seen. Press Y to put it away.")
			: TEXT("The admiral's chart: every sea is in full view in this campaign, so there is nothing to veil. Press Y to put it away."))
		: TEXT("The map as it is: each country's colour a stain over its own ground."), 6.0);
}

// ---- (Rules 84.3.3) FIRST COMMAND: the staff's notes ---------------------------------------------------------------
// The rules say which note is on the desk, and stamp the lessons his deeds earn (campaign-bridge.ts, lessonNow): the
// game keeps only what is on the screen. A note comes, slides in, and stays where it came until it goes; when its
// deed is done the DONE stamp lands on it and it stays two seconds at most (a click sends it at once); then the next
// comes. A deed done while another note is up earns its stamp quietly, as in the browser.

void ACampaignPlayerController::ResetLessons()
{
	Lessons = FLessonDesk();
	LessonNote = LessonGone = LessonLaid = FLessonNote();
	bLessonNote = bLessonLaid = bLessonsDue = bOpeningCardOwed = bLessonStampOwed = bLessonGoneStamped = bLessonNoteHidden = bArsenalStrip = bArsenalStripPutAway = false;
	bLessonLaidSpoken = false;
	if (bLessonGuideOpen)
	{
		bLessonGuideOpen = false;
		++LessonGuideRevision;
	}
	++LessonArrival;
	LessonCameAt = 0.0;
	LessonGoneAt = LessonStampAt = -1.0;
	LessonStampUntil = 0.0;
	LessonMomentKey = 0;
	LessonDeedsOwed.Reset();
	LessonActsOwed.Reset();
	LessonAim = FLessonAim();
	if (!GroundAimLaid.IsEmpty() && Map.IsValid())
	{
		Map->ShowPointer(TArray<FVector>(), TArray<FColor>());
	}
	GroundAimLaid.Reset();
}

bool ACampaignPlayerController::IsLessonQuiet() const
{
	// (the browser's quiet(): nothing while the other nations move, a film runs, the map is not his to look at, the
	// GUIDE is open, or the card lesson 1 lies on is still to open)
	return !IsMapShown() || bWatching || Announcements.Num() > 0 || WarPrompt.bVisible || bMomentOn || bOpeningCardOwed || BriefingAt > 0.0
		|| bLessonGuideOpen || (Document.bValid && Document.Title == TEXT("How to play"));
}

bool ACampaignPlayerController::HasLessonNote() const
{
	return bLessonNote && !IsLessonQuiet() && FPlatformTime::Seconds() >= LessonCameAt;
}

double ACampaignPlayerController::GetLessonAge() const
{
	return FPlatformTime::Seconds() - LessonCameAt;
}

double ACampaignPlayerController::GetLessonStampAge() const
{
	return LessonStampAt < 0.0 ? -1.0 : FPlatformTime::Seconds() - LessonStampAt;
}

bool ACampaignPlayerController::GetLessonLeaving(FLessonNote& Out, float& Faded) const
{
	const double Since = FPlatformTime::Seconds() - LessonGoneAt;
	if (LessonGoneAt < 0.0 || Since >= 0.11 || IsLessonQuiet())
	{
		return false;
	}
	Out = LessonGone;
	Faded = static_cast<float>(FMath::Clamp(Since / 0.11, 0.0, 1.0));
	return true;
}

void ACampaignPlayerController::LessonPageChanged()
{
	// (Lesson 1's deed is the card it lay on put down: by GIVE ORDERS, Enter, TOUR THE MAP or a line's pin, trap 7.)
	if (bLessonLaid && !LessonCardOpen())
	{
		bLessonLaid = false;
		LessonDeedsOwed.Emplace(TEXT("card"), true);
	}
	bLessonsDue = true;
	// (Asked at once where no order is being answered, so that lesson 1 is on the month card as it first opens.)
	if (Afterwards.Num() == 0 && !IsAnswering() && !Opening.bVisible)
	{
		AskLessons();
	}
}

void ACampaignPlayerController::AskLessons()
{
	bLessonsDue = false;
	FCampaignSession* Session = GetSession();
	if (!Session || Session->GetView().Human.IsEmpty())
	{
		LessonDeedsOwed.Reset();
		LessonActsOwed.Reset();
		if (bLessonNote)
		{
			LessonGone = LessonNote;
			bLessonGoneStamped = false;
			LessonGoneAt = FPlatformTime::Seconds();
			bLessonNote = false;
		}
		return;
	}
	// What is owed first, in the order it was done; then the desk as the screen stands now. Every lesson stamped on
	// the way is gathered, for the note on the screen to take its stamp. (Lesson 10's note says its words for the
	// ARSENAL while the ARSENAL is open: the UI director, build 1640.)
	Session->SetLessonPageOpen(bArsenalOpen);
	TArray<FString> Stamped;
	FLessonDesk Desk;
	for (const TPair<FString, bool>& Deed : LessonDeedsOwed)
	{
		if (Session->LessonDeed(Deed.Key, Deed.Value, Desk))
		{
			Stamped.Append(Desk.Stamped);
		}
	}
	LessonDeedsOwed.Reset();
	for (const FString& Act : LessonActsOwed)
	{
		if (Session->LessonAct(Act, LessonCardOpen(), PickedCorps, Desk))
		{
			Stamped.Append(Desk.Stamped);
		}
	}
	LessonActsOwed.Reset();
	if (!Session->LessonNow(LessonCardOpen(), PickedCorps, Desk))
	{
		return;
	}
	Stamped.Append(Desk.Stamped);
	Desk.Stamped = MoveTemp(Stamped);
	TakeLessons(Desk);
	RefreshLessonGuide(); // (the Guide's list, if it is open, as the notebook now stands)
}

void ACampaignPlayerController::TakeLessons(const FLessonDesk& Desk)
{
	const double Now = FPlatformTime::Seconds();
	Lessons = Desk;
	// Lesson 1 lies on the month card while it is open, and nowhere else.
	bLessonLaid = Desk.bLaid && LessonCardOpen();
	if (bLessonLaid)
	{
		LessonLaid = Desk.Laid;
	}
	// The stamp lands on the note he is reading, if its deed is the one done. One earned while the screen is not his
	// (END THE MONTH's, read once the others have moved: trap 2) waits with its note until his month is back.
	if (bLessonNote && LessonStampUntil <= 0.0 && LessonNote.Kind == TEXT("lesson") && Desk.Stamped.Contains(LessonNote.Id))
	{
		bLessonStampOwed = true;
	}
	if (bLessonStampOwed)
	{
		if (IsLessonQuiet() || bLessonNoteHidden)
		{
			return;
		}
		bLessonStampOwed = false;
		LessonStampAt = Now;
		LessonStampUntil = Now + 2.0;
		CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
		return;
	}
	if (LessonStampUntil > 0.0)
	{
		return; // (the stamped note stays until its two seconds are up or it is clicked away; the next waits)
	}
	ShowDeskNote(Now);
}

void ACampaignPlayerController::ShowDeskNote(double Now, bool bWasStamped)
{
	const bool bSame = bLessonNote && Lessons.bNote && Lessons.Note.Kind == LessonNote.Kind && Lessons.Note.Id == LessonNote.Id;
	if (bSame)
	{
		LessonNote = Lessons.Note; // (its words may name another place now; it stays where it stands)
		return;
	}
	if (bLessonNote)
	{
		// The note on the desk fades out over 110 ms, and the next slides in after it: a stamped one with its stamp.
		LessonGone = LessonNote;
		bLessonGoneStamped = bWasStamped && LessonNote.Kind == TEXT("lesson");
		LessonGoneAt = Now;
		bLessonNote = false;
	}
	if (Lessons.bNote)
	{
		LessonNote = Lessons.Note;
		bLessonNote = true;
		++LessonArrival;
		LessonCameAt = LessonGoneAt >= 0.0 && Now - LessonGoneAt < 0.11 ? LessonGoneAt + 0.11 : Now;
	}
}

void ACampaignPlayerController::PressLesson(const FString& Action)
{
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	if (Action == TEXT("send"))
	{
		// (Ruling 147: a click on a stamped note sends it at once.)
		if (LessonStampUntil > 0.0)
		{
			LessonStampUntil = FPlatformTime::Seconds();
		}
		return;
	}
	LessonActsOwed.Add(Action);
	bLessonsDue = true;
	if (IsArsenalStrip() && (Action == TEXT("later") || Action == TEXT("close") || Action == TEXT("tip")))
	{
		// (he asked: the arsenal closes up)
		bArsenalStrip = false;
		bArsenalStripPutAway = true;
	}
	if (Afterwards.Num() == 0 && !IsAnswering())
	{
		AskLessons();
	}
}

void ACampaignPlayerController::GetLessonKeepClear(TArray<FBox2D>& Out) const
{
	Out.Reset();
	const FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	// A flag of his on the screen, cloth and plate, as GetColoursOnScreen boxes every flag; where none stands, the
	// middle of its province.
	auto FlagIn = [this, Session, &Out](int32 Region)
	{
		if (!Map->GetData().Regions.IsValidIndex(Region))
		{
			return;
		}
		FVector Middle;
		double Radius = 0.0;
		FVector2D At, Top;
		const AArmyPiece* Piece = Map->FindPiece(Region, Session->GetView().Human);
		if (Piece && Piece->GetColours(Middle, Radius) && PlayerCameraManager && ProjectWorldLocationToScreen(Middle, At)
			&& ProjectWorldLocationToScreen(Middle + PlayerCameraManager->GetCameraRotation().Quaternion().GetUpVector() * Radius, Top))
		{
			const double Half = FMath::Clamp(FVector2D::Distance(At, Top) * 0.8, 6.0, 90.0) + 6.0;
			Out.Add(FBox2D(At - FVector2D(Half, Half * 0.75 + 12.0), At + FVector2D(Half, Half * 1.35)));
			return;
		}
		const FVector2D Center = Map->GetData().Regions[Region].Center;
		if (ProjectWorldLocationToScreen(Map->MapToWorld(Center.X, Center.Y), At))
		{
			Out.Add(FBox2D(At - FVector2D(30.0, 40.0), At + FVector2D(30.0, 30.0)));
		}
	};
	if (ArmyRegion != INDEX_NONE)
	{
		FlagIn(ArmyRegion);
	}
	if (bLessonNote && LessonNote.Region != INDEX_NONE)
	{
		FlagIn(LessonNote.Region);
	}
	// (and what the pointer frames on the ground, which the note must never cover either)
	if (LessonAim.Kind == FLessonAim::Ground)
	{
		Out.Add(LessonAim.Box);
	}
}

// ---- (Rules 84.3.3; the art director's first-hour brief, section 3) The pointer -----------------------------------
// The one thing the note points at, for the lesson on the desk and where he stands now (firsthour.ts, target()): on a
// card only the card's own way to the deed, never a thing the card covers (trap 9); otherwise a button by its name,
// a flag of his, or the lit ground nearest the army in hand. On the map it is laid on the ground (ShowPointer).

namespace
{
	/** (lessons.ts, BOMBING) The raids that earn lesson 9: "Fight for the sky" is not one of them (trap 8). */
	bool IsBombingRaid(const FString& Kind)
	{
		static const TCHAR* const Bombing[] = { TEXT("bomb"), TEXT("night"), TEXT("ground"), TEXT("line"), TEXT("town"), TEXT("harbour"), TEXT("strike"), TEXT("fleet") };
		for (const TCHAR* Each : Bombing)
		{
			if (Kind == Each)
			{
				return true;
			}
		}
		return false;
	}
}

bool ACampaignPlayerController::PieceBoxOnScreen(const AArmyPiece* Piece, FBox2D& Out) const
{
	Out = FBox2D(ForceInit);
	if (!Piece || Piece->IsHidden() || !PlayerCameraManager)
	{
		return false;
	}
	// The whole piece, from the pole's tip to the base (the brief): the base's round on the ground, and the flag over
	// it where it flies one.
	const FQuat Eye = PlayerCameraManager->GetCameraRotation().Quaternion();
	const FVector Right = Eye.GetRightVector(), Up = Eye.GetUpVector();
	const FVector Along = FVector::CrossProduct(Right, FVector::UpVector).GetSafeNormal();
	const FVector Base = Piece->GetActorLocation();
	const double Round = FMath::Max(Piece->GetBaseRadius() * Piece->GetActorScale3D().X, 1.0);
	TArray<FVector, TInlineAllocator<8>> Points;
	Points.Add(Base + Right * Round);
	Points.Add(Base - Right * Round);
	Points.Add(Base + Along * Round);
	Points.Add(Base - Along * Round);
	FVector Middle;
	double Cloth = 0.0;
	if (Piece->GetColours(Middle, Cloth))
	{
		Points.Add(Middle + Up * (Cloth * 0.8) + Right * (Cloth * 0.7));
		Points.Add(Middle + Up * (Cloth * 0.8) - Right * (Cloth * 0.7));
	}
	else
	{
		Points.Add(Base + FVector::UpVector * (Round * 2.0));
	}
	for (const FVector& Point : Points)
	{
		FVector2D At;
		if (!ProjectWorldLocationToScreen(Point, At))
		{
			Out = FBox2D(ForceInit);
			return false;
		}
		Out += At;
	}
	return Out.bIsValid;
}

bool ACampaignPlayerController::GroundSpotOnScreen(int32 Region, FVector2D& Out, FVector& Foot) const
{
	if (!Map.IsValid() || !Map->GetData().Regions.IsValidIndex(Region))
	{
		return false;
	}
	const FVector2D Center = Map->GetData().Regions[Region].Center;
	Foot = Map->SurfacePointOver(Center.X, Center.Y, 0.0);
	if (!ProjectWorldLocationToScreen(Foot, Out))
	{
		return false;
	}
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const double Half = 22.0 * Height / 1000.0;
	const FBox2D Frame(Out - FVector2D(Half, Half), Out + FVector2D(Half, Half));
	if (Frame.Min.X < 0.0 || Frame.Min.Y < 0.0 || Frame.Max.X > Width || Frame.Max.Y > Height)
	{
		return false;
	}
	// (A spot he can see and click: not under a panel of the frame, nor under the note.)
	for (const FBox2D& Panel : FrameOnScreen)
	{
		if (Panel.Intersect(Frame))
		{
			return false;
		}
	}
	return !(LessonNoteOnScreen.bIsValid && LessonNoteOnScreen.Intersect(Frame));
}

void ACampaignPlayerController::AimLesson()
{
	THEATER_SPEED_SCOPE(AimLesson);
	FLessonAim Aim;
	const FCampaignSession* Session = GetSession();
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const double S = Height / 1000.0; // (the brief's pixels, of the 1600 by 1000 frame, in the viewport's)
	auto OnScreen = [&Aim](const FName& Name)
	{
		Aim.Kind = FLessonAim::Screen;
		Aim.Name = Name;
	};
	// A piece of his framed whole, 6 px clear of it.
	auto FramePiece = [&](const AArmyPiece* Piece) -> bool
	{
		FBox2D Box;
		if (!PieceBoxOnScreen(Piece, Box))
		{
			return false;
		}
		Box = Box.ExpandBy(6.0 * S);
		if (Box.Max.X < 0.0 || Box.Max.Y < 0.0 || Box.Min.X > Width || Box.Min.Y > Height)
		{
			return false;
		}
		Aim.Kind = FLessonAim::Ground;
		Aim.Box = Box;
		Aim.Foot = Piece->GetActorLocation();
		return true;
	};
	auto FrameFlag = [&](int32 Region) -> bool
	{
		return Map.IsValid() && Session && FramePiece(Map->FindPiece(Region, Session->GetView().Human));
	};
	// Of the provinces lit on the ground, the one nearest the army in hand that he can see: a frame 44 square at its middle.
	auto FrameGround = [&](const TArray<int32>& Lit) -> bool
	{
		FVector2D From(Width * 0.5, Height * 0.5);
		FBox2D Hand;
		if (RingedPiece.IsValid() && PieceBoxOnScreen(RingedPiece.Get(), Hand))
		{
			From = Hand.GetCenter();
		}
		double Best = TNumericLimits<double>::Max();
		for (const int32 Region : Lit)
		{
			FVector2D At;
			FVector Foot;
			if (Region == ArmyRegion || !GroundSpotOnScreen(Region, At, Foot))
			{
				continue;
			}
			const double Far = FVector2D::DistSquared(At, From);
			if (Far < Best)
			{
				Best = Far;
				Aim.Kind = FLessonAim::Ground;
				Aim.Box = FBox2D(At - FVector2D(22.0 * S, 22.0 * S), At + FVector2D(22.0 * S, 22.0 * S));
				Aim.Foot = Foot;
			}
		}
		return Aim.Kind == FLessonAim::Ground;
	};
	if (Session && Map.IsValid() && !IsLessonQuiet() && !Session->GetView().Human.IsEmpty())
	{
		const FString& Human = Session->GetView().Human;
		if (IsLessonLaid())
		{
			OnScreen(TEXT("giveorders")); // (lesson 1: the card put down is its deed; its pointer only while its card is up, trap 7)
		}
		else if (bLessonNote && !IsLessonStamped() && LessonNote.Kind == TEXT("lesson"))
		{
			const FString& Id = LessonNote.Id;
			const FViewDesk* Air = FindDesk(TEXT("air"));
			const FViewDesk* Build = FindDesk(TEXT("build"));
			const bool bInHand = PickedCorps.Num() > 0 && ArmyRegion != INDEX_NONE;
			if (IsLessonBesideCard())
			{
				const bool bFace = FaceOff.bVisible && !FaceOff.bResolved;
				if (Id == TEXT("card") && LessonCardOpen())
				{
					OnScreen(TEXT("line"));
				}
				else if (Id == TEXT("attack") && bFace && !FaceOff.bMarch && !FaceOff.bStorm)
				{
					OnScreen(TEXT("send"));
				}
				else if (Id == TEXT("city") && bFace && (FaceOff.bMarch || FaceOff.bStorm))
				{
					OnScreen(TEXT("siege"));
				}
				else if (Id == TEXT("raid") && bPageOpen && Page.Card.bValid)
				{
					OnScreen(IsBombingRaid(Page.Card.Kind) ? TEXT("fly") : TEXT("otherraid")); // (trap 8: back to a bombing raid)
				}
				else if ((Id == TEXT("raid") || Id == TEXT("build")) && bPageOpen && Page.Id == TEXT("staff"))
				{
					const FViewDesk* Desk = Id == TEXT("raid") ? Air : Build;
					if (Desk && Desk->bStaff && !Desk->Act.IsEmpty())
					{
						OnScreen(FName(*(TEXT("act:") + Desk->Act)));
					}
				}
				else if (Id == TEXT("build") && bArsenalOpen)
				{
					// (Through the arsenal's door the clamps leave its button (the art director's brief, section 10): on
					// LET MY STAFF's row while the staff has the desk, else on the first thing he can order, as the browser.)
					OnScreen(Build && Build->bStaff ? TEXT("deskbar:build") : TEXT("order"));
				}
			}
			else if (Id == TEXT("pickup") || Id == TEXT("city"))
			{
				FrameFlag(LessonNote.Region);
			}
			else if (Id == TEXT("march") && bInHand)
			{
				TArray<int32> Lit;
				for (const FReachRegion& Step : Reach)
				{
					if (!Step.bAttack && !Step.IsWar())
					{
						Lit.Add(Step.Region);
					}
				}
				FrameGround(Lit);
			}
			else if (Id == TEXT("attack"))
			{
				TArray<int32> Lit;
				for (const FReachRegion& Step : Reach)
				{
					if (Step.bAttack && !Step.IsWar() && bInHand)
					{
						Lit.Add(Step.Region);
					}
				}
				if (!FrameGround(Lit))
				{
					FrameFlag(LessonNote.Region);
				}
			}
			else if (Id == TEXT("rail") && bInHand)
			{
				FrameGround(RailBeyond.Array());
			}
			else if (Id == TEXT("end"))
			{
				OnScreen(TEXT("end"));
			}
			else if (Id == TEXT("raid"))
			{
				if (Air && Air->bStaff)
				{
					OnScreen(TEXT("depth")); // (ruling b425: YOUR STAFF opens from the red line under the date)
				}
				else
				{
					// "Click your aircraft on the map": the first group of his that he can see.
					for (const FViewAirFleet& Wing : Session->GetView().AirFleets)
					{
						if (Wing.Nation == Human && FramePiece(Map->FindAirPiece(Wing.Id)))
						{
							break;
						}
					}
				}
			}
			else if (Id == TEXT("build"))
			{
				OnScreen(Build && Build->bStaff ? TEXT("depth") : TEXT("arsenal"));
			}
		}
	}
	LessonAim = Aim;
	if (Aim.Kind == FLessonAim::Ground)
	{
		LayGroundPointer(Aim);
	}
	else if (!GroundAimLaid.IsEmpty())
	{
		GroundAimLaid.Reset();
		if (Map.IsValid())
		{
			Map->ShowPointer(TArray<FVector>(), TArray<FColor>());
		}
	}
}

void ACampaignPlayerController::LayGroundPointer(const FLessonAim& Aim)
{
	if (!Map.IsValid() || !PlayerCameraManager)
	{
		return;
	}
	// Laid again only when the frame or the eye has moved.
	const FVector Eye = PlayerCameraManager->GetCameraLocation();
	const FRotator Facing = PlayerCameraManager->GetCameraRotation();
	const FString Key = FString::Printf(TEXT("%.0f %.0f %.0f %.0f|%.1f %.1f %.1f|%.2f %.2f %.2f"), Aim.Box.Min.X, Aim.Box.Min.Y, Aim.Box.Max.X, Aim.Box.Max.Y,
		Eye.X, Eye.Y, Eye.Z, Facing.Pitch, Facing.Yaw, Facing.Roll);
	if (Key == GroundAimLaid)
	{
		return;
	}
	GroundAimLaid = Key;
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const double S = Height / 1000.0;
	const FVector2D A = Aim.Box.Min, B = Aim.Box.Max;
	const double Arm = ((B.X - A.X) < 60.0 * S ? 12.0 : 16.0) * S;
	TArray<FVector> Corners;
	TArray<FColor> Inks;
	// A rectangle of the screen laid on the ground: each corner where the eye's ray through it meets the ground at the
	// height of the thing framed, so that from the eye it stands exactly where the screen has it, and a piece in front
	// of it hides it.
	auto Lay = [&](double X0, double Y0, double X1, double Y1, double Lift, const FColor& Ink)
	{
		const FVector2D Points[4] = { FVector2D(FMath::Min(X0, X1), FMath::Min(Y0, Y1)), FVector2D(FMath::Max(X0, X1), FMath::Min(Y0, Y1)),
			FVector2D(FMath::Max(X0, X1), FMath::Max(Y0, Y1)), FVector2D(FMath::Min(X0, X1), FMath::Max(Y0, Y1)) };
		FVector Laid[4];
		for (int32 Each = 0; Each < 4; ++Each)
		{
			FVector From, Way;
			if (!DeprojectScreenPositionToWorld(static_cast<float>(Points[Each].X), static_cast<float>(Points[Each].Y), From, Way) || Way.Z > -0.02)
			{
				return;
			}
			const double Along = (Aim.Foot.Z - From.Z) / Way.Z;
			if (Along <= 0.0)
			{
				return;
			}
			const FVector2D On = Map->WorldToMap(From + Way * Along);
			Laid[Each] = Map->SurfacePointOver(On.X, On.Y, Lift);
		}
		for (const FVector& Corner : Laid)
		{
			Corners.Add(Corner);
		}
		Inks.Add(Ink);
	};
	// The four clamps: a path through each corner of the frame, Thick across with square ends, the arm along the edge
	// taking the corner's square and the arm down the side the rest (the browser's #fhground, fh-case under fh-arm).
	auto Clamps = [&](double Thick, double Lift, const FColor& Ink)
	{
		const double Half = Thick * 0.5;
		const FVector2D Ends[4] = { A, FVector2D(B.X, A.Y), FVector2D(A.X, B.Y), B };
		const FVector2D Ways[4] = { FVector2D(1.0, 1.0), FVector2D(-1.0, 1.0), FVector2D(1.0, -1.0), FVector2D(-1.0, -1.0) };
		for (int32 Each = 0; Each < 4; ++Each)
		{
			const FVector2D C = Ends[Each], W = Ways[Each];
			Lay(C.X - W.X * Half, C.Y - W.Y * Half, C.X + W.X * (Arm + Half), C.Y + W.Y * Half, Lift, Ink);
			Lay(C.X - W.X * Half, C.Y + W.Y * Half, C.X + W.X * Half, C.Y + W.Y * (Arm + Half), Lift, Ink);
		}
	};
	Clamps(6.0 * S, 0.34, FLinearColor(FColor::FromHex(TEXT("0E0B08"))).CopyWithNewOpacity(0.9f).ToFColor(false)); // (desk-950 at 90%)
	Clamps(3.0 * S, 0.38, FLinearColor(FColor::FromHex(TEXT("E0B872"))).ToFColor(false)); // (brass-300)
	Map->ShowPointer(Corners, Inks);
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
	// (The UI director, check 19.) The arm stands on its own line under the name, with no dot: a dot at a fold
	// began the line ("· Infantry").
	ShowTitle = FText::FromString(FString::Printf(TEXT("%s\n%s"), *Card->Name.ToString(), *Title));
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

void ACampaignPlayerController::FrameReachSoon()
{
	if (bScripted || bWatching || ArmyRegion == INDEX_NONE)
	{
		FrameReachFor = INDEX_NONE;
		return;
	}
	FrameReachFor = ArmyRegion;
	FrameReachFrom = FPlatformTime::Seconds() + 0.15; // (the tray is laid out a frame after the army is in hand)
	FrameReachUntil = FPlatformTime::Seconds() + 3.0;
}

void ACampaignPlayerController::FrameReach()
{
	THEATER_SPEED_SCOPE(FrameReach); // (the code checker's pre-check: the search below grows with the panels, four loops deep)
	ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	if (!Eye || !Map.IsValid() || !Map->GetData().Regions.IsValidIndex(ArmyRegion) || Width <= 0 || Height <= 0 || FrameOnScreen.Num() == 0)
	{
		return;
	}
	const FCampaignMapData& Data = Map->GetData();
	const FVector2D Screen(Width, Height);
	TArray<FBox2D> Panels;
	for (const FBox2D& Each : FrameOnScreen)
	{
		Panels.Add(FBox2D(Each.Min - FVector2D(8.0, 8.0), Each.Max + FVector2D(8.0, 8.0)));
	}

	// The ground in question, on the screen: the army's own province and every one it can reach, each with room round it.
	TArray<int32> Places;
	Places.Add(ArmyRegion);
	for (const FReachRegion& Each : Reach)
	{
		Places.AddUnique(Each.Region);
	}
	const double Room = 56.0, Touch = 20.0;
	FBox2D Ground(ForceInit);
	bool bHidden = false;
	for (const int32 Place : Places)
	{
		if (!Data.Regions.IsValidIndex(Place))
		{
			continue;
		}
		const FVector2D At = Data.Regions[Place].Center;
		FVector2D Spot;
		if (!ProjectWorldLocationToScreen(Map->MapToWorld(At.X, At.Y), Spot) || Spot.X < -Screen.X || Spot.X > 2.0 * Screen.X || Spot.Y < -Screen.Y || Spot.Y > 2.0 * Screen.Y)
		{
			continue; // (behind the eye, or round the world's seam: not framed)
		}
		Ground += FBox2D(Spot - FVector2D(Room, Room), Spot + FVector2D(Room, Room));
		const FBox2D Near(Spot - FVector2D(Touch, Touch), Spot + FVector2D(Touch, Touch));
		for (const FBox2D& Panel : Panels)
		{
			if (Panel.Intersect(Near))
			{
				bHidden = true; // (under a panel)
			}
		}
	}
	if (!Ground.bIsValid || !bHidden)
	{
		return; // (none of it is under a panel: the map stays where the player put it)
	}

	// The open map: of the stretches of the screen no panel covers, the one the ground fits best, and of those that
	// fit, the one it need travel least to reach.
	TArray<double> Xs = { 0.0, Screen.X }, Ys = { 0.0, Screen.Y };
	for (const FBox2D& Panel : Panels)
	{
		Xs.AddUnique(FMath::Clamp(Panel.Min.X, 0.0, Screen.X));
		Xs.AddUnique(FMath::Clamp(Panel.Max.X, 0.0, Screen.X));
		Ys.AddUnique(FMath::Clamp(Panel.Min.Y, 0.0, Screen.Y));
		Ys.AddUnique(FMath::Clamp(Panel.Max.Y, 0.0, Screen.Y));
	}
	Xs.Sort();
	Ys.Sort();
	const FVector2D Need = Ground.GetSize(), Middle = Ground.GetCenter();
	// (how far the ground's middle must move to lie in an open stretch: nowhere, if it is inside already)
	auto ShiftInto = [&Need, &Middle](const FBox2D& Open, bool bFits) -> FVector2D
	{
		if (!bFits)
		{
			return Open.GetCenter() - Middle;
		}
		const FVector2D Low = Open.Min + Need * 0.5, High = Open.Max - Need * 0.5;
		return FVector2D(FMath::Clamp(Middle.X, Low.X, High.X), FMath::Clamp(Middle.Y, Low.Y, High.Y)) - Middle;
	};
	FBox2D Best(ForceInit);
	double BestFit = 0.0, BestTravel = 0.0;
	for (int32 Left = 0; Left < Xs.Num(); ++Left)
	{
		for (int32 Right = Left + 1; Right < Xs.Num(); ++Right)
		{
			for (int32 Top = 0; Top < Ys.Num(); ++Top)
			{
				for (int32 Bottom = Top + 1; Bottom < Ys.Num(); ++Bottom)
				{
					const FBox2D Open(FVector2D(Xs[Left], Ys[Top]), FVector2D(Xs[Right], Ys[Bottom]));
					const FVector2D Size = Open.GetSize();
					if (Size.X < 160.0 || Size.Y < 160.0)
					{
						continue;
					}
					bool bClear = true;
					for (const FBox2D& Panel : Panels)
					{
						if (Panel.Min.X < Open.Max.X - 0.5 && Panel.Max.X > Open.Min.X + 0.5 && Panel.Min.Y < Open.Max.Y - 0.5 && Panel.Max.Y > Open.Min.Y + 0.5)
						{
							bClear = false;
							break;
						}
					}
					if (!bClear)
					{
						continue;
					}
					const double Fit = FMath::Min(1.0, FMath::Min(Size.X / FMath::Max(Need.X, 1.0), Size.Y / FMath::Max(Need.Y, 1.0)));
					const double Travel = ShiftInto(Open, Fit >= 0.999).Size();
					if (Fit > BestFit + 0.01 || (Fit > BestFit - 0.01 && Travel < BestTravel))
					{
						Best = Open;
						BestFit = Fit;
						BestTravel = Travel;
					}
				}
			}
		}
	}
	if (!Best.bIsValid || BestFit <= 0.0)
	{
		return;
	}

	// Where the ground would rest: inside the open stretch, moved no further than it must be. Where it does not fit,
	// the eye goes back a little (never past the height a campaign opens at), and the ground is centred there.
	const double Ratio = Eye->MaxDistance / FMath::Max(Eye->MinDistance, 1.0f);
	const float Zoom = Eye->GetZoom();
	float NewZoom = Zoom;
	if (BestFit < 0.999)
	{
		NewZoom = FMath::Min(Zoom + static_cast<float>(FMath::Loge(FMath::Min(1.0 / BestFit, 1.6)) / FMath::Loge(Ratio)), FMath::Max(Zoom, 0.62f));
	}
	const double Scale = FMath::Pow(Ratio, static_cast<double>(NewZoom - Zoom)); // (how much further every pixel reaches on the table)
	const FVector2D Shift = ShiftInto(Best, BestFit >= 0.999); // (where the ground's middle should come to, from where it is, on the screen)
	auto OnTable = [this](const FVector2D& At, FVector& Out) -> bool
	{
		FVector Origin, Direction;
		return DeprojectScreenPositionToWorld(static_cast<float>(At.X), static_cast<float>(At.Y), Origin, Direction) && RayOnTable(Origin, Direction, Out);
	};
	FVector GroundAt, GoalAt;
	if (!OnTable(Middle, GroundAt) || !OnTable(Middle + Shift, GoalAt))
	{
		return;
	}
	// (The table under the screen's middle is the eye's focus. A point the ground should come to lies, after the move,
	// at the focus plus its old offset from the focus, stretched by however far the eye went back.)
	const FVector Focus = Eye->GetFocus();
	const FVector Goal = GroundAt - (GoalAt - Focus) * Scale;
	if (FVector::Dist2D(Goal, Focus) < 1.0 && NewZoom == Zoom)
	{
		return;
	}
	Eye->GoTo(FVector(Goal.X, Goal.Y, 0.0), NewZoom, false);
	// (item 4: the view moved under a pointer that did not)
	float PointerX = 0.0f, PointerY = 0.0f;
	TipHushedAt = GetMousePosition(PointerX, PointerY) ? FVector2D(PointerX, PointerY) : LastMouse;
	bTipHushed = true;
}

void ACampaignPlayerController::NextArmy()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || bWatching || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible)
	{
		return;
	}
	// (the question at the month's end: this key is its SHOW ME, once it has been up for half a second)
	if (EndAsk.bVisible)
	{
		if (FPlatformTime::Seconds() - EndAsk.Since < 0.5)
		{
			return;
		}
		EndAsk.bVisible = false;
	}
	// (Rulings 88, 101 and 102.) The round is the rules' own: the front first, then the rear; in each, those not yet
	// touched before the others, then the nearest; each is shown once before any is shown again.
	if (Session->GetView().Round.All <= 0)
	{
		Notify(TEXT("Every army has its orders. Press Enter to end the month."), 4.0);
		return;
	}
	// (where the round is measured from: the army in hand, or else what the eye is on)
	FVector2D From = FVector2D::ZeroVector;
	if (Map->GetData().Regions.IsValidIndex(ArmyRegion))
	{
		From = Map->GetData().Regions[ArmyRegion].Center;
		RoundSeen.AddUnique(ArmyRegion); // (the army in hand has been seen: Tab goes on from it)
	}
	else if (const ACampaignCameraPawn* Looking = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		From = Map->WorldToMap(Looking->GetFocus());
	}
	const int32 Next = Session->NextArmy(RoundSeen, From);
	if (!Map->GetData().Regions.IsValidIndex(Next))
	{
		Notify(TEXT("Every army has its orders. Press Enter to end the month."), 4.0);
		return;
	}
	SelectRegion(Next);
	PickUpArmyIn(Next);
	CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		const FVector2D At = Map->GetData().Regions[Next].Center;
		Eye->GoTo(Map->MapToWorld(At.X, At.Y), FMath::Min(Eye->GetZoom(), 0.4f), false);
		// (The UI director's 84.3 check, item 4.) The view moves under a pointer that has not: the hover box goes until
		// the pointer moves again, so it never names a place the pointer was not put on.
		float PointerX = 0.0f, PointerY = 0.0f;
		TipHushedAt = GetMousePosition(PointerX, PointerY) ? FVector2D(PointerX, PointerY) : LastMouse;
		bTipHushed = true;
	}
	FrameReachSoon(); // (item 3: once the view has come to rest)
}

void ACampaignPlayerController::Escape()
{
	// Esc backs out one step: the watching, the face-off, the army in hand, then the territory.
	if (bFrameHidden)
	{
		ToggleFrame(); // (and first of all it brings back a frame that was put away)
		return;
	}
	if (Newsreel.bOn)
	{
		EndNewsreel();
	}
	else if (Announcements.Num() > 0)
	{
		DismissAnnouncement();
	}
	else if (bLessonGuideOpen)
	{
		CloseLessonGuide();
	}
	else if (Document.bValid)
	{
		CloseDocument();
	}
	else if (Opening.bVisible && Opening.AskKind != 0)
	{
		AnswerBegin(false); // (ruling 134(f): the safe answer is the default. Esc keeps the saved campaign.)
	}
	else if (Opening.bVisible && !Opening.AskDelete.IsEmpty())
	{
		AnswerDeleteWar(false); // (release 1.5.1, ruling 134(f): Esc keeps the saved war)
	}
	else if (Opening.bVisible)
	{
		CloseMenu();
	}
	else if (EndAsk.bVisible)
	{
		EndAsk.bVisible = false; // (the question at the month's end is put away: nothing is ended)
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
	else if (AgentInHand != INDEX_NONE)
	{
		PutDownAgent(); // (ruling 169: Esc puts him down)
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
	return Map.IsValid() && DeprojectMousePositionToWorld(Origin, Direction) && RayOnTable(Origin, Direction, OutWorld);
}

int32 ACampaignPlayerController::AimedOf(const AArmyPiece* Piece, int32 Ground) const
{
	// (Brian, 17 November 2026: "if an enemy army is out of a city i couldnt attack them".) With an army in hand, an
	// army's figure, flag and counters stand for the ground that army stands on, for the right button as they always
	// have for the left: the order, the lit province, the road and the tip all go to that army's province, never to
	// whatever ground shows behind the figure.
	return (ArmyRegion != INDEX_NONE && Piece && Piece->IsArmy() && Piece->GetRegionId() != INDEX_NONE) ? Piece->GetRegionId() : Ground;
}

bool ACampaignPlayerController::RightClickOnFigure(int32 CorpsId, int32& OutAimed, int32& OutGround)
{
	OutAimed = OutGround = INDEX_NONE;
	const AArmyPiece* Figure = Map.IsValid() ? Map->FindPieceOfCorps(CorpsId) : nullptr;
	FVector Colours;
	double Radius = 0.0;
	if (!Figure || !PlayerCameraManager || !Figure->GetColours(Colours, Radius))
	{
		return false;
	}
	// The pointer on the middle of its colours: the line from the eye through them, as the mouse would give it.
	const FVector Origin = PlayerCameraManager->GetCameraLocation();
	const FVector Direction = (Colours - Origin).GetSafeNormal();
	FVector OnGround;
	const bool bOnTable = RayOnTable(Origin, Direction, OnGround);
	OutGround = bOnTable ? Map->RegionAtWorld(OnGround) : INDEX_NONE;
	OutAimed = AimedOf(Map->PieceAtRay(Origin, Direction), OutGround);
	if (!IsTableBusy())
	{
		RightClickOn(OutAimed, INDEX_NONE, bOnTable);
	}
	return true;
}

bool ACampaignPlayerController::RayOnTable(const FVector& Origin, const FVector& Direction, FVector& OutWorld) const
{
	if (!Map.IsValid() || Direction.Z > -0.01)
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
	TheaterSpeed::NoteFrame(DeltaTime, GetWorld()); // ("-TheaterSpeedLog": what the frame costs)
	THEATER_SPEED_SCOPE(PlayerTick);
	Super::PlayerTick(DeltaTime);
	PumpVoice();
	// (Speed, b414 mark 1.) An order the rules answered on their own thread: its answer is taken once it is in, and what
	// follows it is laid on the table a step a frame.
	if (Afterwards.Num() > 0)
	{
		THEATER_SPEED_SCOPE(AnswerStep);
		const TFunction<void()> Step = MoveTemp(Afterwards[0]);
		Afterwards.RemoveAt(0);
		Step();
	}
	else if (FCampaignSession* Answering = GetSession())
	{
		Answering->Deliver();
	}
	// (Rules 84.3.3, trap 1.) The card lesson 1 lies on, once the opening flight is over and nothing else has the screen.
	if (bOpeningCardOwed && BriefingAt <= 0.0 && !bWatching && !Newsreel.bOn && Announcements.Num() == 0 && !FaceOff.bVisible && !WarPrompt.bVisible && !Opening.bVisible && !IsAnswering())
	{
		bOpeningCardOwed = false;
		if (FCampaignSession* Owed = GetSession())
		{
			bMonthCardDue = Owed->OpenOpeningCard();
		}
	}
	// (Ruling 64.) The month's card, once nothing else has the screen.
	if (bMonthCardDue && !bWatching && !Newsreel.bOn && Announcements.Num() == 0 && !FaceOff.bVisible && !WarPrompt.bVisible && !Opening.bVisible)
	{
		bMonthCardDue = false;
		OpenPage(TEXT("reports"));
	}
	// (Brian, 24 November 2026: a raid "didnt't tell me the results of the battle".) The report of a raid his crews
	// flew, once its aircraft have been seen to fly it, nothing else has the screen and no button of the mouse is held
	// (a drag of the map, or of a fleet, is let finish). It is dropped when the month moves on, or when something else
	// has held the screen for twenty seconds: it is still to be read from the airfield, in THE MONTH'S RAIDS.
	if (!RaidCardDue.IsEmpty())
	{
		const double RaidNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		const FCampaignSession* Told = GetSession();
		if (!Told || bWatching || bMonthCardDue || Told->GetView().Turn != RaidCardTurn || RaidNow >= RaidCardAt + 20.0)
		{
			RaidCardDue.Reset();
		}
		else if (RaidNow >= RaidCardAt && !IsCardUp() && !IsAnswering() && !bLeftHeld && !bRightHeld)
		{
			const FString Due = RaidCardDue;
			RaidCardDue.Reset();
			OpenPage(Due);
		}
	}
	// (Release 1.5.1, new units ready) The READY plate, once nothing else holds the screen (the month's card closed) and
	// the month's own notice ("June 1941. Your orders.") has gone.
	// (The 1.5 pre-check, 24 November 19:18Z: not under a battle or a raid being fought, where it would run out unseen.)
	if (bReadyDue && !bMonthCardDue && !bWatching && !Newsreel.bOn && Announcements.Num() == 0 && !FaceOff.bVisible && !WarPrompt.bVisible && !Opening.bVisible && !bPageOpen && !IsFightingLive() && FPlatformTime::Seconds() >= NoticeUntil)
	{
		SayReady();
	}
	// (Rules 84.3.3) FIRST COMMAND. The DONE stamp stands two seconds at most, then the next note comes. The notes are
	// asked again whenever what only the screen sees has changed (the card open, the ARSENAL open, the army in hand, his
	// month or the others'), or an order of his has been answered; and only once no order is being answered.
	{
		const double Now = FPlatformTime::Seconds();
		if (LessonStampUntil > 0.0 && Now >= LessonStampUntil)
		{
			LessonStampUntil = 0.0;
			LessonStampAt = -1.0;
			ShowDeskNote(Now, true);
			bLessonsDue = true;
		}
		// (A stamp earned while the screen was not his lands once it is again, whatever ended last: an announcement,
		// a question of war, a moment at the front, the briefing or the newsreel; or a room with no margin for the note.)
		if (bLessonStampOwed && !IsLessonQuiet() && !bLessonNoteHidden)
		{
			bLessonsDue = true;
		}
		// (The art director's brief, section 10.) Lesson 10's deed is done in the arsenal: while the arsenal is open its
		// note is laid across it, and the strip keeps its room until the room closes, for what comes next (the closing
		// note) to take; unless he puts the note away himself.
		if (!bArsenalOpen)
		{
			bArsenalStrip = bArsenalStripPutAway = false;
		}
		else if (!bArsenalStrip && !bArsenalStripPutAway && HasLessonNote() && LessonNote.Kind == TEXT("lesson") && LessonNote.Id == TEXT("build"))
		{
			bArsenalStrip = true;
		}
		if (const FCampaignSession* Seen = GetSession())
		{
			uint32 Key = (LessonCardOpen() ? 1u : 0u) | (bWatching ? 2u : 0u) | (Seen->GetView().bHumanUp ? 4u : 0u) | (bArsenalOpen ? 8u : 0u);
			Key = HashCombine(Key, GetTypeHash(Seen->GetView().Turn));
			for (const int32 Id : PickedCorps)
			{
				Key = HashCombine(Key, GetTypeHash(Id));
			}
			if (Key != LessonMomentKey)
			{
				LessonMomentKey = Key;
				bLessonsDue = true;
			}
		}
		if (bLessonsDue && Afterwards.Num() == 0 && !IsAnswering() && !Opening.bVisible)
		{
			AskLessons();
		}
		// What the note points at this frame (the screen draws a button's; the ground's is laid here).
		AimLesson();
		// (The Guide's list is a card over the war: whatever else is opened, the others' turn, or a reel (the briefing's
		// flight plays as one), takes its place.)
		if (bLessonGuideOpen && (Opening.bVisible || bWatching || Newsreel.bOn || FaceOff.bVisible || WarPrompt.bVisible || Document.bValid || bArsenalOpen || bLedgerOpen
			|| bDiplomacyOpen || bResearchOpen || bGeneralsOpen || (bPageOpen ? Page.Id : FString()) != GuideOverPage))
		{
			bLessonGuideOpen = false;
			++LessonGuideRevision;
			bLessonsDue = true;
		}
		// (The brief.) The note is read aloud as it comes, when the voice is on: its heading, its sentences, and its
		// deed line last; lesson 1 as it is laid on the card. Never while the screen hides it: the screen places a note
		// after this tick, so a new one is read from the next tick on, once the screen has said whether it shows.
		if (HasLessonNote() && LessonSpokenArrival != LessonArrival)
		{
			if (LessonPlacedArrival != LessonArrival)
			{
				LessonPlacedArrival = LessonArrival;
			}
			else if (!bLessonNoteHidden)
			{
				LessonSpokenArrival = LessonArrival;
				SpeakLesson(LessonNote);
			}
		}
		if (IsLessonLaid() && !IsLessonQuiet())
		{
			if (!bLessonLaidSpoken)
			{
				bLessonLaidSpoken = true;
				SpeakLesson(LessonLaid);
			}
		}
		else
		{
			bLessonLaidSpoken = false;
		}
	}
	if (MomentGlideUntil > 0.0 && FPlatformTime::Seconds() >= MomentGlideUntil)
	{
		MomentGlideUntil = 0.0;
		if (ACampaignCameraPawn* Back = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			Back->SetGlide(0.0f);
		}
	}
	// A moment at the front is over after about three seconds, or as soon as the player touches anything.
	if (bMomentOn)
	{
		bool bTouched = FPlatformTime::Seconds() >= MomentUntil;
		if (!bTouched)
		{
			// (any key at all, any button of the mouse, or its wheel)
			static TArray<FKey> Every;
			if (Every.Num() == 0)
			{
				EKeys::GetAllKeys(Every);
				Every.RemoveAll([](const FKey& Key) { return Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() || Key.IsGamepadKey() || Key.IsTouch() || Key == EKeys::AnyKey; });
			}
			for (const FKey& Key : Every)
			{
				if (WasInputKeyJustPressed(Key))
				{
					bTouched = true;
					break;
				}
			}
		}
		if (bTouched)
		{
			EndMoment();
		}
	}
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
		// (The UI director's 84.3 check, item 3.) A right-click that lands on a panel never reaches the map, so it is
		// noted here, as the screen saw it, and answered with a line below.
		const bool bRightDown = Slate.GetPressedMouseButtons().Contains(EKeys::RightMouseButton);
		if (bRightDown && !bSlateRightDown)
		{
			bRightPressOnPanel = bPointerOverPanel;
			RightPanelPress = Slate.GetCursorPos();
		}
		else if (!bRightDown && bSlateRightDown && bRightPressOnPanel)
		{
			bRightPressOnPanel = false;
			bPanelRightClick = bPointerOverPanel && FVector2D::Distance(Slate.GetCursorPos(), RightPanelPress) <= 6.0;
		}
		bSlateRightDown = bRightDown;
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
		// (Brian's score, 18 November 2026: the theme is the opening screen's; a country at war has its own two pieces.)
		const bool bInPlay = Now && !Opening.bVisible && !Now->GetView().Human.IsEmpty();
		Mode->SetMusicMood(bFighting ? TEXT("battle") : !bInPlay ? TEXT("theme") : Now->GetView().bAtWar ? TEXT("war") : TEXT("quiet"));
		Mode->DuckMusic(Newsreel.bOn);
		// One of his wars has ended: The Long Walk Home. (Counted from one month to the next of the same chair; a game
		// loaded or begun anew starts the count again.)
		if (bInPlay)
		{
			const FCampaignView& Seen = Now->GetView();
			int32 Mine = 0;
			for (int32 K = 0; K + 1 < Seen.Wars.Num(); K += 2)
			{
				Mine += Seen.Wars[K] == Seen.Human || Seen.Wars[K + 1] == Seen.Human ? 1 : 0;
			}
			if (MusicWarsOf == Seen.Human && Seen.Turn >= MusicWarsTurn && Seen.Turn <= MusicWarsTurn + 1 && Mine < MusicWars)
			{
				Mode->PlayHomecoming();
			}
			MusicWarsOf = Seen.Human;
			MusicWars = Mine;
			MusicWarsTurn = Seen.Turn;
		}
		else
		{
			MusicWarsOf.Reset();
		}
	}

	// The first time evening comes on in the room, it is said what is happening and how to hold the day.
	if (!bToldOfEvening && !bBusyWithScreens() && GetSession())
	{
		for (TActorIterator<ACampaignEnvironment> It(GetWorld()); It; ++It)
		{
			if (It->GetNight() > 0.25f)
			{
				bToldOfEvening = true;
				Notify(It->IsMonthsEnd() ? TEXT("The month is over: evening in the war room while the rest of the world moves. Morning comes with your next month. Press T to hold it at day.")
					: TEXT("Evening in the war room: the lamps are lit, and day will come round again. Press T to hold it at day."), 9.0);
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
			// ("-TheaterMapSize=67": the map drawn at that many hundredths of a 1600 by 1000 window each way, for timing a trim before it is decided.)
			// (The art director's group A, item 5: the map at two thirds of a 1600 by 1000 window each way. It was at
			// about four fifths. "-TheaterOldLight" keeps the old size with the old light.)
			static const double Budget = []
			{
				int32 Hundredths = 0;
				return FParse::Value(FCommandLine::Get(), TEXT("TheaterMapSize="), Hundredths) && Hundredths >= 40 && Hundredths <= 100
					? 1600.0 * 1000.0 * (Hundredths / 100.0) * (Hundredths / 100.0)
					: FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLight")) ? 1250.0 * 780.0 : 1072.0 * 670.0;
			}();
			// (Sharp lettering, 18 November 2026; the UI director's ruling (c). The game is drawn at the panel's own fine
			// pixels, two to a point on Brian's Mac (bAllowHighDPIInGameMode), so that every card and line of type is
			// crisp where it was one soft pixel to a point. The map keeps the size it was drawn at: it is made at half
			// the window's pixels each way, as it was when macOS did the doubling, and only the lettering and the
			// cards are laid over it at all of them. Measured dragged over New York with the frame limit lifted: 94
			// frames a second so, 96 before; with the map upscaled to every fine pixel it was 57. A window that is not
			// that large (a picture run's 1600 by 1000) is drawn as it always was. "-nohighdpi" is Unreal's own switch
			// for one pixel to a point.)
			float Fine = 1.0f;
			if (GEngine && GEngine->GameViewport)
			{
				const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
				Fine = Window.IsValid() ? Window->GetDPIScaleFactor() : 1.0f;
			}
			const bool bFine = Fine >= 1.5f && static_cast<double>(ViewWidth) * ViewHeight > 2400000.0;
			const double Each = bFine ? Fine : 1.0;
			if (IConsoleVariable* Second = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport")))
			{
				Second->Set(bFine ? FMath::RoundToInt32(100.0f / Fine) : 100, ECVF_SetByCode);
			}
			const int32 Percent = FMath::Clamp(FMath::RoundToInt32(100.0 * FMath::Sqrt(Budget / ((ViewWidth / Each) * (ViewHeight / Each)))), 50, 100);
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
			{
				Variable->Set(Percent, ECVF_SetByCode);
			}
			UE_LOG(LogTheater, Display, TEXT("Window: %d by %d pixels, %.1f to a point; the map is drawn at %d by %d and %s"), ViewWidth, ViewHeight, Fine,
				FMath::RoundToInt32(ViewWidth / Each * Percent / 100.0), FMath::RoundToInt32(ViewHeight / Each * Percent / 100.0),
				bFine ? TEXT("the lettering at every pixel") : TEXT("filled out to the window by Unreal's upscaler"));
		}
	}

	// What the pointer is over: a piece if it is on one (its counters, pole or colours), and the ground behind it.
	FVector PointerWorld;
	const bool bOnTable = bHaveMouse && PointerOnTable(PointerWorld);
	int32 PointedRegion = bOnTable ? Map->RegionAtWorld(PointerWorld) : INDEX_NONE;
	PointerMap = bOnTable ? Map->WorldToMap(PointerWorld) : FVector2D::ZeroVector;
	bPointerOnWater = bOnTable && PointedRegion == INDEX_NONE;
	// (1.5.1; Brian, 24 November 2026: "its hard to click on an airfield"; the art director's spec, item 1.) With nothing
	// in hand an airfield is pointed at from anywhere in a ring 28 of the screen's units across round the middle of its
	// runway, whatever the height of the eye (FieldInRing). With an army, a fleet or an agent in hand only on the field
	// itself, as before, so that no order is turned into a look at an airfield.
	// (Only where the rules keep airfields, the grand campaign's air war: elsewhere the field spots are works on the
	// ground, and the ring would only steal the province's clicks.) The overlap is judged afresh each frame, and a click's
	// swap lasts until the pointer moves 6 units off, whatever it moves over (the code review of 18:30Z).
	const bool bHandsFree = ArmyRegion == INDEX_NONE && FleetInHand == INDEX_NONE && AgentInHand == INDEX_NONE;
	const bool bFieldsKept = GetSession() && GetSession()->GetView().Airfields.Num() > 0;
	bPointerInOverlap = false;
	if (!bHaveMouse || FVector2D::DistSquared(Mouse, OverlapClickAt) > FMath::Square(6.0 * GateMarkScale))
	{
		bOverlapSwapped = false;
	}
	PointedField = !bOnTable ? INDEX_NONE : bHandsFree && bFieldsKept && !bScripted ? FieldInRing(Mouse, PointedRegion) : Map->AirfieldNear(PointerMap);
	AArmyPiece* PointedPiece = nullptr;
	AArmyPiece* AimedPiece = nullptr; // (what an order would go to: never the flag that is being carried)
	int32 PointedAgent = INDEX_NONE;
	{
		FVector RayOrigin, RayDirection;
		if (bHaveMouse && !bScripted && DeprojectMousePositionToWorld(RayOrigin, RayDirection))
		{
			PointedPiece = Map->PieceAtRay(RayOrigin, RayDirection);
			AimedPiece = (PointedPiece && (PointedPiece == HeldPiece.Get() || PointedPiece == HeldFleetPiece.Get())) ? Map->PieceAtRay(RayOrigin, RayDirection, nullptr, PointedPiece) : PointedPiece;
			// (Brian, 24 November 2026 21:53Z: "i cant click on the london airfield".) Where another country's corps stands on
			// or beside a field, as an ally's do at London, its pole and counters took the pointer from the field. With nothing
			// in hand, inside an airfield's ring (on the town's turn of an overlap too, so that the next click there is the
			// field's), another country's army keeps the pointer only on its colours; its pole, counters and guns leave it to
			// the field. (A click on another country's flag only looks at the ground it stands on.) The player's own pieces
			// still win, one that stands behind the army giving way too. Fleets are left alone: a fleet at anchor shows no
			// colours. With no nation of his own (his country fallen, a war watched) nothing changes: the airfield's page
			// would not open. And it is judged before the agent is, so that an agent beside such an army is not passed over.
			if (PointedPiece && (PointedField != INDEX_NONE || bPointerInOverlap) && bHandsFree && bFieldsKept && GetSession()
				&& !GetSession()->GetView().Human.IsEmpty())
			{
				const FString& Leads = GetSession()->GetView().Human;
				auto GivesWay = [&Leads, &RayOrigin, &RayDirection](const AArmyPiece* Stander)
				{
					return Stander->IsArmy() && Stander->GetNationTag() != Leads && !Stander->ColoursHitByRay(RayOrigin, RayDirection);
				};
				if (GivesWay(PointedPiece))
				{
					AArmyPiece* Behind = Map->PieceAtRay(RayOrigin, RayDirection, nullptr, PointedPiece);
					PointedPiece = (Behind && !GivesWay(Behind)) ? Behind : nullptr;
					AimedPiece = (PointedPiece == HeldPiece.Get() || PointedPiece == HeldFleetPiece.Get()) ? nullptr : PointedPiece; // (never the flag that is being carried)
				}
			}
			// (Ruling 153; the art director's ruling of 17 November.) An agent's piece: a small thing among large ones.
			// He is taken hold of from a little way round him; but over an army's flag or base the army wins, unless
			// the pointer is on his own body.
			bool bOnHisBody = false;
			PointedAgent = Map->AgentAtRay(RayOrigin, RayDirection, &bOnHisBody);
			if (PointedAgent != INDEX_NONE && PointedPiece && !bOnHisBody)
			{
				PointedAgent = INDEX_NONE;
			}
			else if (PointedAgent != INDEX_NONE)
			{
				PointedPiece = nullptr;
			}
		}
	}

	// (What an order would go to: the army whose figure is under the pointer, or else the ground. See AimedOf.)
	// (The same spec.) Inside its ring the airfield wins over the province and the town's plate: the pointer points at the
	// field's own province, for the lit ground, the tip and the click. A piece or an agent under the pointer still wins
	// (another country's piece only on its colours, above).
	if (PointedPiece || PointedAgent != INDEX_NONE)
	{
		PointedField = INDEX_NONE; // (a piece or an agent under the pointer takes the click, so the tip is not the airfield's)
	}
	if (PointedField != INDEX_NONE && bHandsFree && bFieldsKept)
	{
		PointedRegion = PointedField;
		bPointerOnWater = false;
	}
	const int32 AimedRegion = AimedOf(PointedAgent == INDEX_NONE ? AimedPiece : nullptr, PointedRegion);

	// The other nations' month plays itself out, a nation at a time.
	if (bWatching && FPlatformTime::Seconds() >= WatchNext)
	{
		WatchNextStep();
	}
	const bool bBusy = IsTableBusy();
	// (Speed, b414 mark 1.) An order still being answered: the map takes no other order, and the pointer asks the rules
	// nothing, until it is on the table. (The ground under the pointer stays lit: that asks nothing.) It is asked afresh at
	// each gate, never kept from the top of the tick: a march given by this very tick's release must already hush this
	// tick's road and tip, or their questions would wait out the whole order on the frame of the press.

	// (The UI director's 84.3 check, item 3.) An army picked up by a hand: once the view is still and its tray is laid out,
	// the map is framed so the ground it can reach lies clear of the tray, the chips and every other panel. A press that
	// becomes a carry, a turn of the view or an order given first leaves the map where it is.
	{
		const FVector CameraSpot = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		const bool bViewStill = FVector::DistSquared(CameraSpot, LastCameraSpot) < 1.0;
		LastCameraSpot = CameraSpot;
		if (FrameReachFor != INDEX_NONE)
		{
			const double Now = FPlatformTime::Seconds();
			if (FrameReachFor != ArmyRegion || bHandOrdered || bScripted || bWatching || bDragging || bTurning || Now > FrameReachUntil)
			{
				FrameReachFor = INDEX_NONE;
			}
			else if (!bBusy && !bLeftHeld && !bRightHeld && Now >= FrameReachFrom && bViewStill)
			{
				FrameReachFor = INDEX_NONE;
				FrameReach();
			}
		}
	}

	// (Ruling 205(c).) The first night of a campaign says once, on the paper line, that it is night and which key holds
	// the hour: Brian took the night for the fog of war (17 November 2026).
	if (!bNightTold && !bScripted && !bBusy)
	{
		TActorIterator<ACampaignEnvironment> Room(GetWorld());
		if (Room && Room->GetTimeKeeping() == 0 && Room->GetNight() > 0.55f)
		{
			bNightTold = true;
			Notify(TEXT("Night has fallen and passes in about a minute. Press T to hold the room at day."), 10.0); // (the UI director's words, 17 November)
		}
	}
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
		bRightGameOnPanel = bPointerOverPanel; // (item 3: an order never goes through a panel to ground the player cannot see)
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
	// (The UI director's 84.3 check, item 3.) A right-click on a panel, with something in hand to send: the line names the
	// place beneath, and how to reach it.
	if (bPanelRightClick)
	{
		bPanelRightClick = false;
		if (!bBusy && !bScripted && (ArmyRegion != INDEX_NONE || FleetInHand != INDEX_NONE || AgentInHand != INDEX_NONE))
		{
			const FCampaignMapData& Data = Map->GetData();
			const int32 Sea = (FleetInHand != INDEX_NONE && bOnTable && bPointerOnWater) ? SeaNear(PointerMap) : INDEX_NONE;
			const FString Place = Data.Seas.IsValidIndex(Sea) ? Data.Seas[Sea].Name : Data.Regions.IsValidIndex(AimedRegion) ? Data.Regions[AimedRegion].Name : FString();
			if (!Place.IsEmpty())
			{
				Notify(FString::Printf(TEXT("%s lies under this panel: move the map, then right-click it."), *Place), 6.0);
			}
		}
	}
	if (bRightHeld && !IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (!bTurning && !bBusy && !IsAnswering() && !bRightGameOnPanel)
		{
			// (The 1.5.1 patch, fleetattack.ts) A right-click on an enemy fleet with a fleet in hand is the order to attack it, as on
			// an enemy army with an army in hand: the card, odds first. Where fleet attacks are off it is a sea order, as before.
			const FCampaignSession* Now = GetSession();
			const bool bOnEnemyFleet = FleetInHand != INDEX_NONE && Now && IsEnemyFleet(AimedPiece, Now->GetView());
			if (!(bOnEnemyFleet && AttackFleetWith(AimedPiece->GetForceId())))
			{
				// (the order itself is RightClickOn, so that the playtester's hook gives exactly the order the mouse gives)
				RightClickOn(AimedRegion, (FleetInHand != INDEX_NONE && bOnTable && bPointerOnWater) ? SeaNear(PointerMap) : INDEX_NONE, bOnTable);
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
		PressGate = HoverGate; // (latched now: while the button is held the hover says no padlock)
		HeldPiece.Reset();
		HeldFleetPiece.Reset();
		bHeldWasInHand = false;
		bFleetWasInHand = false;
		bAgentHeld = false;
		if (!bBusy && !IsAnswering() && PointedAgent != INDEX_NONE)
		{
			// (Ruling 153.) A press on an agent's piece picks him up, as a press on a flag picks up an army.
			bAgentWasInHand = AgentInHand == PointedAgent;
			bAgentHeld = bAgentWasInHand || PickUpAgent(PointedAgent);
		}
		else if (!bBusy && !IsAnswering() && PointedPiece && PointedPiece->IsFleet() && PointedPiece->GetForceId() != INDEX_NONE
			&& GetSession() && GetSession()->GetView().bFleetAttack && PointedPiece->GetNationTag() == GetSession()->GetView().Human)
		{
			// (Behind the "Fleets attack fleets" row: switched off, a press on a fleet drags the table as it always did, and a
			// plain click picks the fleet up.)
			// (The 1.5.1 patch; Brian 23 November 2026 22:23Z: "the same way like I do when I'm moving my ground units".) A press on
			// one of the player's fleets takes hold of it, as a press on a flag takes hold of an army: dragged, it is carried;
			// let go over a sea it sails there, and over an enemy fleet it attacks. A plain click picks it up, as before.
			bFleetWasInHand = FleetInHand == PointedPiece->GetForceId();
			StopLooking();
			if (bFleetWasInHand || PickUpFleet(PointedPiece->GetForceId()))
			{
				HeldFleetPiece = PointedPiece;
			}
		}
		else if (!bBusy && !IsAnswering())
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
		else if (bDragging && HeldFleetPiece.IsValid())
		{
			// (The 1.5.1 patch) The fleet is in the hand: it follows the pointer over the table, and the table slides along at the
			// edge of the view, as an army's flag does.
			if (bOnTable)
			{
				HeldFleetPiece->Carry(PointerWorld);
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
		else if (bDragging && bAgentHeld && AgentInHand != INDEX_NONE)
		{
			// (Ruling 169: he can be dragged like a flag.) The agent hangs over the pointer, and the table slides along
			// when the hand comes to the edge of the view.
			if (bOnTable)
			{
				Map->CarryAgent(PointerWorld);
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
		if (bAgentHeld)
		{
			if (bDragging && AgentInHand != INDEX_NONE)
			{
				// Let go over a province: he is sent there. Let go over the sea, or off the table: he goes back where he stood.
				Map->ReturnAgent();
				if (bOnTable && PointedRegion != INDEX_NONE)
				{
					SendAgentTo(PointedRegion);
				}
			}
			else if (bAgentWasInHand)
			{
				PutDownAgent(); // (a plain click on the agent already in hand puts him down again)
			}
			bAgentHeld = false;
		}
		else if (HeldFleetPiece.IsValid())
		{
			if (bDragging)
			{
				// (The 1.5.1 patch) Let go over an enemy fleet: the attack card, odds first. Over the sea: it sails there. Anywhere
				// else: it goes back where it lay, still in hand. (The carried piece goes back either way: the map moves it
				// once the order is given.)
				HeldFleetPiece->ReturnToRest();
				const FCampaignSession* Now = GetSession();
				const bool bOnEnemyFleet = Now && IsEnemyFleet(AimedPiece, Now->GetView());
				if (!(bOnEnemyFleet && AttackFleetWith(AimedPiece->GetForceId())) && bOnTable && bPointerOnWater)
				{
					OrderFleetTo(SeaNear(PointerMap));
				}
			}
			else if (bFleetWasInHand)
			{
				PutDownFleet(); // (a plain click on the fleet already in hand puts it down again)
			}
		}
		else if (HeldPiece.IsValid())
		{
			if (bDragging)
			{
				DropArmy(AimedRegion, bOnTable); // (set down on an army's figure: set down on that army's ground)
			}
			else if (bHeldWasInHand)
			{
				// A plain click on the flag of the army already in hand puts it down again.
				PutDownArmy();
			}
			// Otherwise a plain click on the flag has already picked the army up.
		}
		else if (!bDragging && !bBusy && !IsAnswering())
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
			else if (PressGate != INDEX_NONE && (GateUnder(Mouse) == PressGate || StagedGate == PressGate) && ArmyRegion == INDEX_NONE && FleetInHand == INDEX_NONE && AgentInHand == INDEX_NONE)
			{
				// (Ruling 178(d).) The padlock is a door, for the holder alone. (The code checker, 17 November: judged on the
				// padlock pressed, still under the pointer as the button is let go, since the hover is blank while it is held.)
				OpenGateBox(PressGate);
			}
			else
			{
				LastClickRegion = Ground;
				LastClickAt = ClickedAt;
				ClickOnTable(PointedPiece, PointedRegion);
				// (The art director's airfield spec, item 1.) Where a town's ring and an airfield's overlap, the next click on the
				// same spot takes the other one, and the one after that this one again.
				if (bPointerInOverlap && !PointedPiece)
				{
					bOverlapSwapped = !bOverlapSwapped;
					OverlapClickAt = Mouse;
				}
			}
		}
		bLeftHeld = false;
		bDragging = false;
		PressGate = INDEX_NONE;
		HeldPiece.Reset();
		HeldFleetPiece.Reset();
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
	// (Ruling 153.) An agent the pointer rests on shows that he can be picked up; one being carried is in a closed hand.
	Map->SetAgentHovered(bBusy ? INDEX_NONE : PointedAgent);
	if (bAgentHeld && bDragging)
	{
		Pointer = EMouseCursor::GrabHandClosed;
	}
	else if (!bBusy && PointedAgent != INDEX_NONE)
	{
		Pointer = EMouseCursor::GrabHand;
	}
	// (Ruling 163.) With an agent in hand no reach is lit: the province under the pointer and its one ring are edged as
	// the ground he would watch from there.
	Map->SetWatchEdge(AgentInHand == INDEX_NONE || bBusy ? INDEX_NONE : StagedHover != INDEX_NONE ? StagedHover : (bScripted || bPointerOverPanel || !bOnTable) ? INDEX_NONE : PointedRegion);
	if (Pointer == EMouseCursor::Default && !bBusy && !bScripted && !bTurning && !bPointerOverPanel && bOnTable)
	{
		if (ArmyRegion != INDEX_NONE && AimedRegion != INDEX_NONE && AimedRegion != ArmyRegion)
		{
			const FReachRegion* There = FindReach(AimedRegion);
			Pointer = !There ? EMouseCursor::SlashedCircle : (There->bAttack || There->IsWar()) ? EMouseCursor::Crosshairs : EMouseCursor::Default;
		}
		else if (FleetInHand != INDEX_NONE)
		{
			const FCampaignSession* Now = GetSession();
			const FViewFleet* Held = Now ? Now->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
			if (Held && FleetHand.bFleetAttack && IsEnemyFleet(AimedPiece, Now->GetView()))
			{
				// (The 1.5.1 patch) Over a fleet of a nation at war with his that it can be sent at: the sights. Over one out of reach: the bar.
				Pointer = FleetHand.Attack.Contains(AimedPiece->GetForceId()) ? EMouseCursor::Crosshairs : EMouseCursor::SlashedCircle;
			}
			else if (Held && bPointerOnWater)
			{
				Pointer = Held->Reach.Contains(SeaNear(PointerMap)) ? EMouseCursor::Default : EMouseCursor::SlashedCircle;
			}
			else if (Held && PointedRegion != INDEX_NONE)
			{
				const bool bShell = FleetHand.bGrand && !FleetHand.bShelled && FleetHand.Coasts.Contains(PointedRegion);
				// (1.5.2) its own harbour is refused while it is held, or cut off with no harbour on its side
				const bool bHomeShut = !FleetHand.Held.IsEmpty() || (!FleetHand.Cut.IsEmpty() && FleetHand.HomeTip.IsEmpty());
				Pointer = bShell ? EMouseCursor::Crosshairs : ((PointedRegion == Held->Base && !bHomeShut) || Held->Bases.Contains(PointedRegion)) ? EMouseCursor::Default : EMouseCursor::SlashedCircle;
			}
		}
	}
	CurrentMouseCursor = Pointer;

	// Whatever the pointer rests on is lit faintly, and with an army in hand the road to it is drawn.
	if (!bScripted && !bTurning && (!bDragging || HeldPiece.IsValid()))
	{
		Map->SetHoveredRegion(bBusy || bPointerOverPanel ? INDEX_NONE : AimedRegion); // (nor is the ground behind a panel lit)
		if (ArmyRegion != INDEX_NONE && !bBusy && !IsAnswering())
		{
			ShowRouteTo(AimedRegion);
		}
	}

	Map->SetShowProvinces(IsInputKeyDown(EKeys::P)); // (held: every province line at full strength)
	// (release 1) Held: every piece is lifted off the table, to read the bare ground under it. ("-TheaterBareGround" holds it for a picture.)
	{
		static const bool bHeldForPicture = FParse::Param(FCommandLine::Get(), TEXT("TheaterBareGround"));
		Map->SetPiecesShown(!(IsInputKeyDown(EKeys::B) || bHeldForPicture));
	}
	// (release 1) The admiral's chart: asked for as a view of the map, or laid out whenever a fleet is in hand. ("-TheaterChart" holds it for a picture.)
	if (const FCampaignSession* Now = GetSession())
	{
		static const bool bChartForPicture = FParse::Param(FCommandLine::Get(), TEXT("TheaterChart"));
		Map->ShowChart(bChartView || FleetInHand != INDEX_NONE || bChartForPicture, Now->GetView().SeaSearch, Now->GetView().OldSightings);
	}
	{
		// ("-TheaterSinking=6:GER:bb,6:UK:ca" sends ships down in a sea for a picture, once the table is laid)
		static bool bSunkForPicture = false;
		FString Asked;
		if (!bSunkForPicture && !Opening.bVisible && GetWorld() && GetWorld()->GetTimeSeconds() > 3.0 && FParse::Value(FCommandLine::Get(), TEXT("TheaterSinking="), Asked, false))
		{
			bSunkForPicture = true;
			TArray<FString> Each;
			Asked.ParseIntoArray(Each, TEXT(","));
			// (and "6:UK:bb:Hood:1418:1415" names her for the memorial plate, with her crew and the men lost; a seventh
			// part "pair" is two light carriers counted as one, and "-" for the name is a ship the record has no name for)
			TArray<FSinking> NamedForPicture;
			for (int32 Nth = 0; Nth < Each.Num(); ++Nth)
			{
				TArray<FString> Parts;
				const int32 Given = Each[Nth].ParseIntoArray(Parts, TEXT(":"));
				if ((Given == 3 || Given >= 6) && !Parts[0].StartsWith(TEXT("r")))
				{
					Map->AddSinking(FCString::Atoi(*Parts[0]), Parts[1], Parts[2], Nth);
				}
				if (Given >= 6)
				{
					FSinking& Sinking = NamedForPicture.AddDefaulted_GetRef();
					Sinking.Zone = Parts[0].StartsWith(TEXT("r")) ? INDEX_NONE : FCString::Atoi(*Parts[0]); // ("r412" for the sea is a harbour: province 412)
					Sinking.Region = Parts[0].StartsWith(TEXT("r")) ? FCString::Atoi(*Parts[0].Mid(1)) : INDEX_NONE;
					Sinking.Nation = Parts[1];
					Sinking.Class = Parts[2];
					FLostHull& Hull = Sinking.Ships.AddDefaulted_GetRef();
					Hull.Name = Parts[3] == TEXT("-") ? FString() : Parts[3];
					Hull.Class = Parts[2];
					Hull.Crew = FCString::Atoi(*Parts[4]);
					Hull.Lost = FCString::Atoi(*Parts[5]);
					Hull.bPair = Parts.IsValidIndex(6) && Parts[6] == TEXT("pair");
					Hull.Type = Parts.IsValidIndex(6) && Parts[6] != TEXT("pair") ? Parts[6] : FString(); // (or her type, as "battlecruiser")
				}
			}
			Remember(NamedForPicture, true);
		}
	}
	// (release 1) The lamp over the army or the fleet in hand.
	Map->SetHandLamp(RingedPiece.IsValid() && ArmyRegion != INDEX_NONE ? RingedPiece.Get() : FleetInHand != INDEX_NONE ? Map->FindFleetPiece(FleetInHand) : nullptr);
	// (release 1) Views kept under 6, 7, 8 and 9: Shift with the key keeps the view, the key alone comes back to it.
	if (!Opening.bVisible && !Newsreel.bOn && !bWatching)
	{
		static const FKey Keys[4] = { EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			if (WasInputKeyJustPressed(Keys[Slot]))
			{
				if (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift))
				{
					KeepView(Slot);
				}
				else
				{
					GoToKeptView(Slot);
				}
			}
		}
	}
	Map->UpdateForCamera(Eye->GetDistance(), Eye->GetYaw(), Eye->GetFocus());

	// A tip that follows the pointer: what the province under it is, whose, what it is worth and who stands there.
	// (no tip for the ground behind a panel the pointer is on)
	const bool bStagedTip = StagedHover != INDEX_NONE && Map->GetData().Regions.IsValidIndex(StagedHover);
	if (bStagedTip)
	{
		const FCampaignRegion& Staged = Map->GetData().Regions[StagedHover];
		ProjectWorldLocationToScreen(Map->MapToWorld(Staged.Centroid.X, Staged.Centroid.Y), StagedPointer);
	}
	// (The UI director's 84.3 check, item 4: after Tab has moved the view, no box until the pointer itself moves.)
	if (bTipHushed && bHaveMouse && FVector2D::DistSquared(Mouse, TipHushedAt) > 9.0)
	{
		bTipHushed = false;
	}
	// (while pictures are being taken the real pointer, wherever it happens to lie, says nothing)
	const int32 TipRegion = bStagedTip ? StagedHover : (bScripted || bBusy || IsAnswering() || !bHaveMouse || bLeftHeld || bRightHeld || bPointerOverPanel || bTipHushed) ? INDEX_NONE : AimedRegion;
	const int32 TipField = TipRegion == INDEX_NONE ? INDEX_NONE : bStagedTip ? (bStagedField ? StagedHover : INDEX_NONE) : PointedField;
	const int32 TipHand = ArmyRegion * 4 + (bByRail ? 1 : 0) + (bBySea ? 2 : 0) + (AgentInHand + 1) * 100003;
	if (TipRegion != HoverTipRegion || TipField != HoverTipField || TipHand != HoverTipHand)
	{
		THEATER_SPEED_SCOPE(HoverTip);
		HoverTipRegion = TipRegion;
		HoverTipField = TipField;
		HoverTipHand = TipHand;
		HoverTip = FText::GetEmpty();
		HoverTipMarked = FText::GetEmpty();
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
			// (Rulings 112 to 114: a railway known to be cut says so, with its cause and month, in the rules' words.)
			FString CutLine;
			if (const FViewCut* Cut = View.Cuts.FindByPredicate([TipRegion](const FViewCut& Each) { return Each.Region == TipRegion; }))
			{
				if (!Cut->Short.IsEmpty())
				{
					CutLine = Cut->Short.Left(1).ToUpper() + Cut->Short.Mid(1) + (Cut->How == TEXT("now") ? TEXT(": no train runs through") : TEXT(""));
					Line += TEXT("\n") + CutLine;
				}
			}
			// (Ruling 46: pointing at a city lists everything it has on order, the total first.)
			if (const FCityOrders* City = View.CityOrders.FindByPredicate([TipRegion](const FCityOrders& Each) { return Each.Region == TipRegion; }))
			{
				Line += TEXT("\n") + City->Head;
				for (int32 K = 0; K < FMath::Min(City->Lines.Num(), 6); ++K)
				{
					Line += TEXT("\n  ") + City->Lines[K];
				}
				if (City->Lines.Num() > 6)
				{
					Line += FString::Printf(TEXT("\n  and %d more"), City->Lines.Num() - 6);
				}
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
				// (Brian, 17 November 2026: "it says i have planes at an airfield but none are there". Ruling 175: what
				// stands there is said first and plainly, and the field's room is never worded as aircraft.)
				const FString Stands = Field->Groups < 0 ? FString(TEXT("What flies from it is not known."))
					: Field->Groups == 0 ? FString(TEXT("No aircraft are based here."))
					: FString::Printf(TEXT("%d air group%s based here%s: %s aircraft%s."), Field->Groups, Field->Groups == 1 ? TEXT("") : TEXT("s"), *YoursAmong(*Field), *FText::AsNumber(Field->Aircraft).ToString(), Field->bCrowded ? TEXT(" (crowded)") : TEXT(""));
				Line = FString::Printf(TEXT("%s  \u00B7  %s\n%s It has room for %s.\nClick to open the airfield."), *Field->Word.ToUpper(), *Region.Name, *Stands, *FText::AsNumber(Field->Holds).ToString());
			}
			// With an army in hand: what a right-click here would do, and what it would cost.
			FString TipMarked;
			const FString GarrisonTip = (ArmyRegion != INDEX_NONE && TipRegion != ArmyRegion) ? GarrisonSays() : FString();
			// (release 2 and the 1.5.1 patch: a garrison that could come out and get here is told what a right-click would
			// do as an army in the field is, the rules looking at it as if out; it comes out only as the order goes)
			const bool bComesOut = !GarrisonTip.IsEmpty() && ComesOutTo(TipRegion);
			if (!GarrisonTip.IsEmpty() && !bComesOut)
			{
				Line += TEXT("\n") + GarrisonTip + GarrisonWhere(); // (a garrison in hand is promised no march)
			}
			else if (ArmyRegion != INDEX_NONE && TipRegion != ArmyRegion)
			{
				if (bComesOut)
				{
					Line += TEXT("\nThey are inside the town, and come out of it as the order goes.");
				}
				// (Rules 84.2.5; ruling 211(a); the code checker, 17 November.) Guns and engineers sent forward alone: the
				// refusal is asked as OrderTo asks it, after the question of war and before anything else is promised.
				const FString Alone = (!bByRail && !bBySea) ? Session->WhyNotLead(PickedCorps, TipRegion) : FString();
				if (const FReachRegion* There = FindReach(TipRegion))
				{
					if (There->IsWar())
					{
						const FCampaignNation* Theirs = Map->GetData().FindNation(There->War);
						Line += FString::Printf(TEXT("\nRight-click: march in, which means war with %s. You are asked first."), Theirs ? *WithThe(Theirs->Name) : *There->War);
					}
					else if (!Alone.IsEmpty())
					{
						Line += TEXT("\n") + Alone;
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
							const FAttackPreview Glance = Session->PreviewAttack(PickedCorps, TipRegion, false, /*bGlance*/ true); // (the odds alone: see PreviewAttack)
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
								Line += TEXT("\nRight-click: attack. The staff's estimate: ") + CampaignText::OddsBetween(Glance.OddsLow, Glance.OddsHigh) + TEXT(".");
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
					else if (There->bTake)
					{
						// (Rules 84.1; ruling 175(c); the UI director's sheet 21.) Enemy ground with no army in its field:
						// what stands there, what the march comes to, and the rules' sentence for what it meets.
						const FAttackPreview Meets = Session->PreviewTake(PickedCorps, TipRegion);
						const FString Weeks = FString::Printf(TEXT("%d week%s of the month"), FMath::Max(There->Cost, 1), There->Cost == 1 ? TEXT("") : TEXT("s"));
						if (!Meets.bValid)
						{
							Line += FString::Printf(TEXT("\nRight-click: march here, %s."), *Weeks);
						}
						else
						{
							if (!Meets.Forces.IsEmpty())
							{
								Line += TEXT("\n") + Meets.Forces + TEXT(".");
							}
							if (Meets.Meets == TEXT("guard") && Meets.bForecast)
							{
								auto Say = [](float Odds) { return Odds >= 1.0f ? FString::Printf(TEXT("%.1f to 1 for you"), Odds) : FString::Printf(TEXT("%.1f to 1 against"), 1.0f / FMath::Max(Odds, 0.01f)); };
								Line += FString::Printf(TEXT("\nRight-click: %s. Odds as you arrive: %s. You are shown the card first."), *Meets.OrderWord.ToLower(), *Say(Meets.Odds));
								if (Meets.bLater)
								{
									Line += FString::Printf(TEXT("\nIts guard will fight. Made ready, a month later: %s."), *Say(Meets.LaterOdds));
								}
							}
							else
							{
								Line += FString::Printf(TEXT("\nRight-click: %s, %s."), *(Meets.OrderWord.IsEmpty() ? FString(TEXT("march in")) : Meets.OrderWord.ToLower()), *Weeks);
								if (!Meets.MarchLine.IsEmpty())
								{
									Line += TEXT("\n") + Meets.MarchLine;
								}
							}
						}
						if (EnemyReach.Contains(TipRegion))
						{
							Line += TEXT("\nThe enemy you can see could strike here next month.");
						}
					}
					else
					{
						Line += FString::Printf(TEXT("\nRight-click: march here, %d week%s of the month."), FMath::Max(There->Cost, 1), There->Cost == 1 ? TEXT("") : TEXT("s"));
						if (EnemyReach.Contains(TipRegion))
						{
							Line += TEXT("\nThe enemy you can see could strike here next month.");
						}
					}
				}
				else if (!Alone.IsEmpty())
				{
					// (Rules 84.2.5; ruling 211(a).) Guns sent forward alone: the refusal on the pointer's paper slip, in ink.
					Line += TEXT("\n") + Alone;
				}
				else if (const FApproachPlan Road = (!bByRail && !bBySea && !bHandSpent) ? Session->PlanApproach(PickedCorps, TipRegion) : FApproachPlan(); Road.bOk)
				{
					// (The code checker, 17 November.) One test for the tip and the order: the march-up card is promised only
					// where a right-click opens it (OrderTo asks the same plan, in the same place in its order). (Rules 84.5;
					// sheet 34: the slip says the order, and under it the staff's pick and the moves left on arrival.)
					const FApproachRow& Pick = Road.Rows[0];
					const FString Left = Pick.Left <= 0 ? FString(TEXT("no move left")) : FString::Printf(TEXT("%d %s left"), Pick.Left, Pick.Left == 1 ? TEXT("move") : TEXT("moves"));
					Line += FString::Printf(TEXT("\nRight-click \u00B7 march up and attack\nBy %s, %s on arrival"), *Pick.Name, *Left);
				}
				else if (!bByRail && !bBySea && RailBeyond.Contains(TipRegion))
				{
					// (The railway brief, section 8.) A province it can ride to: the rules' one line, in the lit railway's
					// own blue-white; and what engineers can do at a cut they are carried to.
					const FRailTip Train = Session->RailTip(PickedCorps, TipRegion);
					if (!Train.Ride.IsEmpty())
					{
						TipMarked = TEXT("<Rail>") + PlainInMarked(Train.Ride) + TEXT("</>") + (Train.Relay.IsEmpty() ? FString() : TEXT("\n") + PlainInMarked(Train.Relay));
					}
				}
				else if (bHandSpent && !bByRail && !bBySea)
				{
					// (Ruling 87.) An army with no moves left. On friendly ground a right-click is kept, and it sets off
					// as the next month opens; anywhere else the rules say why not. The words are the rules' own.
					const FMarchPlan& KeptWay = PlanFor(TipRegion);
					if (KeptWay.bOk)
					{
						TipMarked = TEXT("<Brass>No moves left this month.</>\nRight-click: ") + PlainInMarked(KeptWay.Line);
					}
					else
					{
						TipMarked = FString::Printf(TEXT("<Red>%s</>"), *PlainInMarked(KeptWay.Line.IsEmpty() ? FString(TEXT("No moves left this month.")) : KeptWay.Line));
					}
				}
				else if (const int32* Months = (!bByRail && !bBySea) ? FarMonths.Find(TipRegion) : nullptr)
				{
					const FString Kept = FString::Printf(TEXT("Out of reach this month. Right-click: march here over about %s. The army goes on by itself as each month opens, and halts if the enemy comes near."),
						*Months == 1 ? TEXT("a month") : *FString::Printf(TEXT("%d months"), *Months));
					// (The railway brief, section 8.) Why no train will take it there. A break he can act on (a cut, a
					// siege, bridges down) comes first, its opening with the box's emphasis, and the kept march's line
					// after it, dim; any other reason is an aside after the march's line. A cut is said once.
					const FRailTip Train = Session->RailTip(PickedCorps, TipRegion);
					if (!Train.Strong.IsEmpty() && Train.No.StartsWith(Train.Strong))
					{
						TipMarked = TEXT("<Strong>") + PlainInMarked(Train.Strong) + TEXT("</>") + PlainInMarked(Train.No.Mid(Train.Strong.Len())) + TEXT("\n<Dim>") + PlainInMarked(Kept) + TEXT("</>");
						if (Train.bCutHere && !CutLine.IsEmpty())
						{
							Line.ReplaceInline(*(TEXT("\n") + CutLine), TEXT(""));
						}
					}
					else
					{
						Line += TEXT("\n") + Kept;
						if (!Train.No.IsEmpty())
						{
							Line += TEXT("\n") + Train.No;
						}
					}
				}
				else
				{
					// (The code checker, 17 November.) What a right-click here would say, as OrderTo says it: the rules'
					// sentence naming the nearest ground beside the enemy, or why no attack goes in, or that it is out of reach.
					const FString Why = (!bByRail && !bBySea) ? (Road.None.IsEmpty() ? WhyNoAttackOn(TipRegion) : Road.None) : FString();
					Line += TEXT("\n") + (Why.IsEmpty() ? FString(TEXT("Out of this army's reach this month.")) : Why);
				}
			}
			if (FleetInHand != INDEX_NONE)
			{
				const FViewFleet* Held = View.Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
				if (Held && Held->Base == TipRegion)
				{
					// (1.5.2, the release review) Held where this month's battle left it, or cut off beyond a shut strait, its own
					// harbour says what a right-click there does (or why it does nothing), as the tray's RETURN TO HARBOUR does.
					Line += !FleetHand.Held.IsEmpty() ? FString::Printf(TEXT("\n%s"), *FleetHand.Held)
						: !FleetHand.HomeTip.IsEmpty() ? FString::Printf(TEXT("\nRight-click: %s sails for harbour. %s"), *Held->Name, *FleetHand.HomeTip)
						: !FleetHand.Cut.IsEmpty() ? FString::Printf(TEXT("\n%s"), *FleetHand.Cut)
						: FString::Printf(TEXT("\nRight-click: %s returns to harbour here."), *Held->Name);
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
			// (Ruling 159.) Where one of his agents was caught this month, the mark left there says so, in the rules' sentence.
			for (const FAgentMark& Mark : View.AgentMarks)
			{
				if (Mark.Region == TipRegion)
				{
					Line += TEXT("\n") + Mark.Text;
				}
			}
			// (Rulings 163 and 169.) With an agent in hand: what a right-click does here; or, where the rules refuse the
			// journey, their own reason.
			if (AgentInHand != INDEX_NONE)
			{
				const FString Refused = Session->WhyNotSendAgent(AgentInHand, TipRegion);
				Line += TEXT("\n") + (Refused.IsEmpty() ? FString(TEXT("Right-click: the agent goes here, a month on the road.")) : Refused);
			}
			HoverTip = FText::FromString(Line);
			HoverTipMarked = FText::FromString(TipMarked);
		}
	}
	// With a fleet in hand, the sea under the pointer is marked, and what a right-click would do is said.
	{
		// (for the photographer, who has no mouse: the sea the pointer is staged on)
		const bool bStagedSea = StagedSea != INDEX_NONE && FleetInHand != INDEX_NONE && Map->GetData().Seas.IsValidIndex(StagedSea);
		if (bStagedSea)
		{
			const FVector2D Middle = Map->GetData().Seas[StagedSea].Center;
			ProjectWorldLocationToScreen(Map->MapToWorld(Middle.X, Middle.Y), StagedPointer);
		}
		const int32 Sea = bStagedSea ? StagedSea
			: (FleetInHand != INDEX_NONE && !bBusy && bHaveMouse && bPointerOnWater && !bLeftHeld && !bRightHeld && !bPointerOverPanel && !bTipHushed) ? SeaNear(PointerMap) : INDEX_NONE;
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
					// (release 1) Ships that were found here and are lost again.
					for (const FOldSighting& Seen : Session->GetView().OldSightings)
					{
						if (Seen.Zone == Sea)
						{
							const FCampaignNation* Whose = Map->GetData().FindNation(Seen.Nation);
							Known += FString::Printf(TEXT(" %s ships were last seen here %s ago."), Whose ? *Whose->Adjective : *Seen.Nation, Seen.Months == 1 ? TEXT("a month") : *FString::Printf(TEXT("%d months"), Seen.Months));
						}
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
				// (Ruling 86.) A sea it would reach but for a gate shut to it: the gate is named, with its reason.
				const int32* BarredBy = Held->Reach.Contains(Sea) ? nullptr : Held->Barred.Find(Sea);
				const FViewGate* Bar = BarredBy ? Session->GetView().Gates.FindByPredicate([BarredBy](const FViewGate& Each) { return Each.Id == *BarredBy; }) : nullptr;
				HoverTipMarked = Bar ? FText::FromString(FString::Printf(TEXT("<Red>%s</>"), *PlainInMarked(Bar->Shut))) : FText::GetEmpty();
				// (The 1.5.1 patch, fleetattack.ts) An enemy fleet here it can be sent at: dropped on it, the fleet attacks at once.
				// And a fleet that attacked this month stays where the battle left it.
				FString Attack;
				if (FleetHand.bFleetAttack)
				{
					for (const FViewFleet& Other : Session->GetView().Fleets)
					{
						if (Other.Zone == Sea && FleetHand.Attack.Contains(Other.Id))
						{
							Attack = FString::Printf(TEXT("\nDrop %s on %s to attack it now, odds first."), *Held->Name, *Other.Name);
							break;
						}
					}
				}
				// (1.5.3, the 1.5.2 review) A fleet cut off beyond a shut strait is not told a sea is too far: over a sea no gate
				// bars, its line says why, as a right-click there answers it. Over a barred sea the gate's red line says why, as the
				// browser's tip does, and a right-click there gives its cut-off line, as the browser's click does.
				HoverTip = FText::FromString(!FleetHand.Held.IsEmpty() ? FString::Printf(TEXT("%s%s\n%s"), *Name, *Known, *FleetHand.Held)
					: Held->Reach.Contains(Sea)
					? FString::Printf(TEXT("%s%s\nRight-click: %s sails here to %s.%s"), *Name, *Known, *Held->Name, FleetNext == TEXT("raid") ? TEXT("raid merchant shipping") : FleetNext == TEXT("hunt") ? TEXT("hunt submarines") : TEXT("patrol"), *Attack)
					: Bar ? Name
					: !FleetHand.Cut.IsEmpty() ? FString::Printf(TEXT("%s\n%s"), *Name, *FleetHand.Cut)
					: FString::Printf(TEXT("%s\nToo far for %s this month."), *Name, *Held->Name));
			}
			else if (TipRegion == INDEX_NONE)
			{
				HoverTip = FText::GetEmpty();
			}
		}
	}

	// (Ruling 86.) The pointer on a gate's mark: who holds it, and whether the player's ships may pass, in the
	// words of that gate's own rule. Its box takes the place of whatever lies under the mark.
	{
		const int32 OnGate = StagedGate != INDEX_NONE ? StagedGate : (bScripted || bBusy || !bHaveMouse || bLeftHeld || bRightHeld || bPointerOverPanel || bTipHushed) ? INDEX_NONE : GateUnder(Mouse);
		const FCampaignSession* Told = GetSession();
		const FViewGate* Shown = (Told && OnGate != INDEX_NONE) ? Told->GetView().Gates.FindByPredicate([OnGate](const FViewGate& Each) { return Each.Id == OnGate; }) : nullptr;
		if (Shown)
		{
			if (HoverGate != OnGate)
			{
				GateTip = FText::FromString(Shown->Name);
				// (Rulings 176(e) and 178(d): nothing on the slip is red. For its holder the last line is the order a click
				// gives; for anyone else, that only the holder can give it.)
				GateTipMarked = FText::FromString(PlainInMarked(Shown->Lead)
					+ (Shown->Mark.IsEmpty() ? FString() : FString::Printf(TEXT("<%s>%s</>"), Shown->bOpen ? TEXT("Brass") : TEXT("Strong"), *PlainInMarked(Shown->Mark)))
					+ PlainInMarked(Shown->Rest)
					+ (Shown->OrderLine.IsEmpty() ? FString() : Shown->Order.IsEmpty() ? TEXT("\n<Dim>") + PlainInMarked(Shown->OrderLine) + TEXT("</>")
						: TEXT("\n<Brass>Click:</> ") + PlainInMarked(Shown->OrderLine)));
			}
			HoverGate = OnGate;
			HoverTip = GateTip;
			HoverTipMarked = GateTipMarked;
			if (StagedGate != INDEX_NONE)
			{
				for (const TPair<int32, FBox2D>& Mark : GateMarks)
				{
					if (Mark.Key == StagedGate)
					{
						StagedPointer = Mark.Value.GetCenter();
					}
				}
			}
		}
		else if (HoverGate != INDEX_NONE)
		{
			HoverGate = INDEX_NONE;
			HoverTip = FText::GetEmpty();
			HoverTipMarked = FText::GetEmpty();
			HoverTipRegion = -2; // (and whatever lies under the pointer says its own piece again)
			FleetHoverSea = -2;
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
	THEATER_SPEED_SCOPE(HoldArmy);
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
		FrameReachSoon(); // (the UI director's 84.3 check, item 3: a press that becomes a carry never moves the map; see PlayerTick)
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
	// (the small map's picture is the map's own box, or the whole world where the world is attached)
	const bool bWhole = Data.bWorld && Data.WorldWidth > 0.0;
	const double FrameX = bWhole ? Data.WorldMinX : 0.0, FrameY = bWhole ? Data.WorldMinY : 0.0;
	const double FrameWide = bWhole ? Data.WorldWidth : Data.Width, FrameHigh = bWhole ? Data.WorldHeight : Data.Height;
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
			OutDots.Add(FVector((At.X - FrameX) / FrameWide, (At.Y - FrameY) / FrameHigh, Whose));
		}
	}
	if (const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		// About what the screen takes in: as wide as the view at the point looked at, and three fifths as deep.
		const FVector2D Middle = Map->WorldToMap(Eye->GetFocus());
		const double PerUnit = FVector::Dist(Map->MapToWorld(0.0, 0.0), Map->MapToWorld(1.0, 0.0));
		const double Wide = PerUnit > 0.0 ? 2.0 * Eye->GetDistance() * FMath::Tan(FMath::DegreesToRadians(Eye->GetHorizontalFov() * 0.5)) / PerUnit : 100.0;
		const FVector2D Half(Wide * 0.5 / FrameWide, Wide * 0.3 / FrameHigh);
		const FVector2D Centre((Middle.X - FrameX) / FrameWide, (Middle.Y - FrameY) / FrameHigh);
		OutLooking = FBox2D(Centre - Half, Centre + Half);
	}
}

bool ACampaignPlayerController::IsMinimapWorld() const
{
	return Map.IsValid() && Map->GetData().bWorld && Map->GetData().WorldWidth > 0.0;
}

double ACampaignPlayerController::GetMinimapCentre() const
{
	if (!IsMinimapWorld())
	{
		return 0.5;
	}
	const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn());
	if (!Eye)
	{
		return 0.5;
	}
	const FCampaignMapData& Data = Map->GetData();
	const double Share = (Map->WorldToMap(Eye->GetFocus()).X - Data.WorldMinX) / Data.WorldWidth;
	return Share - FMath::FloorToDouble(Share);
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
		if (IsMinimapWorld())
		{
			// (round the world a share may run past either end of the picture: the eye takes the short way to it)
			Eye->GoTo(Map->MapToWorld(Data.WorldMinX + Share.X * Data.WorldWidth, Data.WorldMinY + FMath::Clamp(Share.Y, 0.0, 1.0) * Data.WorldHeight));
			return;
		}
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
	PutDownAgent(); // (a fleet in hand: the agent is let go)
	FleetInHand = FleetId;
	AdviserSaw(3);
	FleetNext = Fleet->Stance == TEXT("raid") ? TEXT("raid") : Fleet->Stance == TEXT("hunt") ? TEXT("hunt") : TEXT("patrol");
	FleetHoverSea = INDEX_NONE;
	CampaignSounds::Play(this, TEXT("ship_horn"), 0.35f);
	RefreshOrders();
	return true;
}

bool ACampaignPlayerController::AttackFleetWith(int32 Target)
{
	if (FleetInHand == INDEX_NONE || Target == INDEX_NONE || !FleetHand.bValid || !FleetHand.bFleetAttack)
	{
		return false;
	}
	// (The 1.5.1 patch, fleetattack.ts; Brian 23 November 2026 22:23Z) Set down on an enemy fleet: the card asks first, with the
	// odds and what each side is likely to lose, as an army's face-off does. Where it cannot be sent (out of reach, at
	// peace, already sent this month) the card says why, in a sentence, and SEND THEM IN is shut.
	CampaignSounds::Play(this, TEXT("ship_horn"), 0.4f);
	OpenPage(FString::Printf(TEXT("fleetattack:%d:%d"), FleetInHand, Target));
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
	// (1.5.3, the 1.5.2 review) Cut off beyond a shut strait it reaches no sea: its heading says so, not "can reach 0 seas".
	const FString ReachWords = !FleetHand.Cut.IsEmpty() ? FString(TEXT("cut off beyond a shut strait"))
		: FString::Printf(TEXT("can reach %d sea%s"), Fleet->Reach.Num(), Fleet->Reach.Num() == 1 ? TEXT("") : TEXT("s"));
	Tray.Subtitle = FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d ship%s%s  \u00B7  %s"), *Doing, Fleet->Ships(), Fleet->Ships() == 1 ? TEXT("") : TEXT("s"),
		FleetHand.Admiral.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  \u00B7  %s"), *FleetHand.Admiral), *ReachWords));
	// (Rules version 72: the sea lift is on the fleet's card, since it is the fleet that covers a crossing. It is in
	// the card's plain heading and not on its warning line: it is a fact, not an alarm.)
	if (!View.LiftLine.IsEmpty())
	{
		Tray.Subtitle = FText::FromString(Tray.Subtitle.ToString() + TEXT("  \u00B7  ") + View.LiftLine);
	}
	Tray.FleetWarning = FText::FromString(FleetHand.bShelled ? FString(TEXT("Its guns have spoken this month.")) : FleetHand.Screen);
	Tray.FleetStance = Fleet->Stance;
	if (FleetNext == TEXT("hunt") && !Tray.bFleetGrand)
	{
		FleetNext = TEXT("patrol");
	}
	Tray.FleetNext = FleetNext;
	// (1.5.2) Held where this month's battle left it, all four of its orders are refused with that reason. Cut off beyond
	// a shut strait, its three at sea are refused with its card's line, and its way home only with no harbour on its side.
	Tray.FleetSeaWhy = !FleetHand.Held.IsEmpty() ? FleetHand.Held : FleetHand.Cut;
	Tray.FleetHomeWhy = !FleetHand.Held.IsEmpty() ? FleetHand.Held : FleetHand.HomeTip.IsEmpty() ? FleetHand.Cut : FString();
	Tray.FleetHomeTip = FleetHand.HomeTip;
	Tray.PendingButton = FText::GetEmpty();
	// (1.5.2) A fleet held or cut off is told so, and not told to right-click seas it cannot go to. Its guns can still
	// shell a coast in red, if one is in reach.
	const TCHAR* StillShells = FleetHand.Coasts.Num() > 0 ? TEXT(" A coast in red can still be shelled.") : TEXT("");
	Tray.Hint = FText::FromString(!View.bHumanUp ? TEXT("It is not your month.")
		: !FleetHand.Held.IsEmpty() ? FString::Printf(TEXT("%s%s Esc lets it go."), *FleetHand.Held, StillShells)
		: !FleetHand.Cut.IsEmpty() ? FString::Printf(TEXT("%s%s%s Esc lets it go."), *FleetHand.Cut, FleetHand.HomeTip.IsEmpty() ? TEXT("") : TEXT(" RETURN TO HARBOUR takes it to a harbour on its side."), StillShells)
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
	ShowTitle = FText::FromString(FString::Printf(TEXT("%s\n%s"), *Tray.Title.ToString(), *Name.ToString())); // (check 19: the ship's own line, no dot)
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
		// (1.5.3, the 1.5.2 review) Held where this month's battle left it, or cut off beyond a shut strait (no sea is ringed
		// for it), it is refused with that reason, held first, as the tray's buttons and the rules give it.
		const FString HeldOrCut = !FleetHand.Held.IsEmpty() ? FleetHand.Held : FleetHand.Cut;
		if (!HeldOrCut.IsEmpty())
		{
			Notify(HeldOrCut, 8.0);
			return;
		}
		// (Ruling 86.) A route refused because a gate is shut says so.
		const int32* BarredBy = Fleet->Barred.Find(Zone);
		const FViewGate* Bar = BarredBy ? Session->GetView().Gates.FindByPredicate([BarredBy](const FViewGate& Each) { return Each.Id == *BarredBy; }) : nullptr;
		Notify(Bar && !Bar->Shut.IsEmpty() ? Bar->Shut : FString::Printf(TEXT("%s cannot reach the %s this month."), *Name, *Sea), Bar ? 8.0 : 5.0);
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
		const FString Name = Fleet->Name, Was = Fleet->Stance;
		if (Session->OrderForce(FString::Printf(TEXT("fleet:%d:%d:%s"), FleetInHand, Fleet->Zone, *FleetNext), Note))
		{
			ShowWar(Session->GetView(), TArray<FCorpsMove>());
			Notify(FString::Printf(TEXT("%s: %s."), *Name, FleetNext == TEXT("raid") ? TEXT("raiding merchant shipping, and keeping clear of the enemy fleet")
				: FleetNext == TEXT("hunt") ? TEXT("hunting submarines with its destroyers") : TEXT("on patrol, seeking the enemy's warships and guarding the convoys")), 5.0);
		}
		else
		{
			// (1.5.2) Refused (held where this month's battle left it, or cut off beyond a shut strait): it says why, and
			// the button stays on what the fleet is doing, not on the order it was refused.
			FleetNext = Was == TEXT("raid") || Was == TEXT("hunt") ? Was : FString(TEXT("patrol"));
			Notify(Note.IsEmpty() ? FString(TEXT("That order cannot be given now.")) : Note, 5.0);
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
		// (1.5.2) Cut off beyond a shut strait it makes for a harbour on its side instead, which becomes its base: the
		// harbour named is the one it is in now.
		const FViewFleet* Now = Session->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; });
		const FString Into = Now ? Now->Port : Port;
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		RefreshOrders();
		Notify(Into == Port ? FString::Printf(TEXT("%s returns to harbour at %s."), *Name, *Into)
			: FString::Printf(TEXT("%s cannot reach %s: it makes for %s instead, its new base."), *Name, *Port, *Into), 5.0);
	}
	else
	{
		Notify(Note.IsEmpty() ? FString(TEXT("That order cannot be given now.")) : Note, 5.0);
	}
}

int32 ACampaignPlayerController::FieldInRing(const FVector2D& Mouse, int32 Ground)
{
	THEATER_SPEED_SCOPE(FieldInRing);
	bPointerInOverlap = false;
	// (on the field itself, close to, it is that field, as it always was)
	const int32 OnField = Map->AirfieldNear(PointerMap);
	if (OnField != INDEX_NONE)
	{
		return OnField;
	}
	// (The art director, 24 November 18:29Z: the trim.) The ring only from working height down, where the airfields' badges
	// are drawn (GetTownPlates). From the theatre view a click picks the province, as in 1.5 (only the field itself, above,
	// is an airfield there), and the province panel's airfield row is the way in.
	if (const ACampaignCameraPawn* Height = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		if (Height->GetDistance() > 12500.0f)
		{
			return INDEX_NONE;
		}
	}
	// The ring: 14 of the screen's units round the middle of the runway, in the window's own pixels (GateMarkScale is the
	// screen's pixels to its unit, as the HUD last painted).
	const double Ring = 14.0 * GateMarkScale;
	const TheaterSpeed::FEye OneView(this);
	FVector2D Here, East, North;
	if (!OneView.Project(Map->SurfacePointOver(PointerMap.X, PointerMap.Y, 0.0), Here)
		|| !OneView.Project(Map->SurfacePointOver(PointerMap.X + 1.0, PointerMap.Y, 0.0), East)
		|| !OneView.Project(Map->SurfacePointOver(PointerMap.X, PointerMap.Y + 1.0, 0.0), North))
	{
		return INDEX_NONE;
	}
	// How many pixels a step of the map is here, the shorter way: every field that might be inside the ring is taken, and
	// each is then measured on the screen.
	// (the smaller stretch of the map onto the screen here, so that a turned and tilted view misses no field)
	const FVector2D AlongX = East - Here, AlongY = North - Here;
	const double XX = AlongX.SizeSquared(), YY = AlongY.SizeSquared(), XY = FVector2D::DotProduct(AlongX, AlongY);
	const double Step = FMath::Max(FMath::Sqrt(FMath::Max(0.5 * (XX + YY - FMath::Sqrt(FMath::Square(XX - YY) + 4.0 * XY * XY)), 0.0)), 0.05);
	TArray<TPair<int32, FVector2D>> Near;
	Map->AirfieldsWithin(PointerMap, FMath::Min(Ring / Step * 1.25 + 1.0, 400.0), Near);
	int32 Best = INDEX_NONE;
	double BestAway = Ring;
	for (const TPair<int32, FVector2D>& Candidate : Near)
	{
		FVector Runway;
		FVector2D Seen;
		if (Map->GetAirfieldWorld(Candidate.Key, Runway) && OneView.Project(Runway, Seen))
		{
			const double Away = FVector2D::Distance(Seen, Mouse);
			if (Away <= BestAway)
			{
				BestAway = Away;
				Best = Candidate.Key;
			}
		}
	}
	if (Best == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	// Where the town of the ground under the pointer is in the same ring round its own middle, the rings overlap: the
	// nearer middle wins, and a click there turns it to the other until the pointer moves off (see the click).
	const FCampaignMapData& Data = Map->GetData();
	if (Data.Regions.IsValidIndex(Ground))
	{
		const FVector2D Middle = Data.Regions[Ground].Center;
		FVector2D Town;
		if (OneView.Project(Map->SurfacePointOver(Middle.X, Middle.Y, 0.0), Town) && FVector2D::Distance(Town, Mouse) <= Ring)
		{
			bPointerInOverlap = true;
			const bool bFieldNearer = BestAway <= FVector2D::Distance(Town, Mouse);
			return bFieldNearer != bOverlapSwapped ? Best : INDEX_NONE;
		}
	}
	return Best;
}

void ACampaignPlayerController::ClickOnTable(AArmyPiece* Piece, int32 RegionId)
{
	THEATER_SPEED_SCOPE(ClickOnTable);
	StopLooking();
	PutDownAgent(); // (a click anywhere else lets the agent in hand go, and looks at what was clicked)
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
			// (The 1.5.1 patch) With a fleet in hand, a click on an enemy fleet is the order to attack it: the card, odds first.
			if (FleetInHand != INDEX_NONE && FleetInHand != Piece->GetForceId() && GetSession() && IsEnemyFleet(Piece, GetSession()->GetView())
				&& AttackFleetWith(Piece->GetForceId()))
			{
				return;
			}
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
	// (Rules 84.2.3, "no press on an enemy army goes unanswered"; the playtester's cases 4 to 6.) With an army in hand
	// that cannot attack the enemy army clicked: the click still looks at the place, and the paper line says why no
	// attack was planned, in the words the right button gives.
	FString WhyNot;
	if (ArmyRegion != INDEX_NONE && Target != INDEX_NONE && Target != ArmyRegion && !bByRail && !bBySea && !WhyNoAttackOn(Target).IsEmpty())
	{
		WhyNot = ComesOut() ? FString() : GarrisonSays(); // (a garrison that could come out is refused for the place's own reason)
		if (!WhyNot.IsEmpty())
		{
			WhyNot += GarrisonWhere();
		}
		if (WhyNot.IsEmpty() && bHandSpent)
		{
			const FMarchPlan& KeptWay = PlanFor(Target);
			WhyNot = KeptWay.Line.IsEmpty() ? FString(TEXT("No moves left this month.")) : KeptWay.Line;
		}
		if (WhyNot.IsEmpty())
		{
			WhyNot = WhyNoAttackOn(Target);
		}
	}
	// Anywhere else: look at that territory. If one of the player's armies is there it is picked up; if not,
	// whatever was in hand is put down.
	SelectRegion(Target);
	if (!WhyNot.IsEmpty())
	{
		Notify(WhyNot, 9.0);
	}
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
			Card.Men = Unit.Men;
			Card.KindId = Unit.Elite == TEXT("heavy") ? FString(TEXT("heavy")) : Unit.Kind;
			Card.Ally = Unit.Ally;
			Card.Morale = Unit.Cohesion;
			Card.Experience = Unit.Experience;
			Card.Stale = Unit.Stale;
			Card.Carry = Unit.Carry; // (Version 84: an identified corps' weapons are known with it)
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
	Tray.Subtitle = FText::FromString(CampaignText::Phrases(FString::Printf(TEXT("%s army at %s  \u00B7  %s"), *Adjective, *Where, *Size)));
	Tray.PendingButton = FText::GetEmpty();
	Tray.JoinButton = FText::GetEmpty();
	// (Ruling 176: a unit that takes no orders says why. Fortress troops never march, and the rules' sentence says so.)
	const bool bAllFortress = bMine && !Tray.Cards.ContainsByPredicate([](const FTrayCard& Each) { return Each.KindId != TEXT("fort"); });
	FString RulesSay;
	for (const FTrayCard& Each : Tray.Cards)
	{
		if (bMine && RulesSay.IsEmpty() && !Each.NoOrders.IsEmpty())
		{
			RulesSay = Each.NoOrders;
		}
	}
	Tray.Hint = FText::FromString(!RulesSay.IsEmpty() ? *RulesSay
		: bAllFortress ? TEXT("Fortress troops man their works and do not move.")
		: bMine ? TEXT("This army has used its month, or holds its ground: it can be given no orders now.")
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

void ACampaignPlayerController::TrayTownOrder(int32 Which)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0 || IsAnswering())
	{
		return;
	}
	if (Which == 0)
	{
		// (Ruling 182: no storm is fought without its card. The card says why where the storm cannot be made.)
		CampaignSounds::Play(this, TEXT("click"), 0.6f);
		OpenStormCard(ArmyRegion, true);
		return;
	}
	// (copies: the tray is set out again as each order is carried out)
	const TArray<FString> Actions = Which == 1 ? Tray.GarrisonActions : Tray.FieldActions;
	const int32 Here = ArmyRegion;
	FString Said;
	bool bAny = false;
	for (const FString& Action : Actions)
	{
		FString Note;
		if (Session->OrderInProvince(Action, Here, Note))
		{
			bAny = true;
			Said = Note;
		}
		else if (Said.IsEmpty())
		{
			Said = Note;
		}
	}
	if (bAny)
	{
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
		CampaignSounds::Play(this, TEXT("marching"), 0.5f);
	}
	RefreshOrders();
	RefreshRegionPanel(Here);
	if (!Said.IsEmpty())
	{
		Notify(Said, 6.0);
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
	if (!bBySea && !bByRail && !Tray.StormButton.IsEmpty())
	{
		// (an army that stands round an enemy town is told how the town is attacked)
		// (The UI director's words, 18 November 2026: a line that carries a name keeps the name's own capitals.)
		Tray.Hint = FText::FromString(Tray.StormTown + TEXT(": press its button above, or right-click the town. A right-click elsewhere marches them away. Esc puts them down."));
	}
	{
		const FString Garrison = GarrisonSays();
		if (!Garrison.IsEmpty())
		{
			// (The UI director, 18 November 2026: the card's line above already says whose garrison they are. Where the
			// card carries TAKE THE FIELD, the foot says only what to do.)
			Tray.Hint = FText::FromString(ComesOut()
				? FString(TEXT("Right-click where they should go, or drag the flag there: they come out of the town on the way. Esc puts them down."))
				: !Tray.FieldButton.IsEmpty()
				? FString(TEXT("To march, TAKE THE FIELD first. Right-click the town for its orders. Esc puts them down."))
				: Garrison + GarrisonWhere() + TEXT(" Esc puts them down."));
		}
	}
	if (FCampaignSession* Rules = GetSession())
	{
		const FString Left = Rules->GateLeft(PickedCorps);
		if (!Left.IsEmpty())
		{
			Tray.Hint = FText::FromString(Left + TEXT(" ") + Tray.Hint.ToString());
		}
	}
	Tray.HintLead = FText::GetEmpty();
	if (bHandSpent && !bHandOrdered && !bBySea && !bByRail)
	{
		// (Ruling 87.) An army with no moves left: its order is kept, and it sets off as the next month opens. (One voice
		// after a press: only before any order is given to it, the one time the foot speaks of next month.)
		Tray.HintLead = FText::FromString(TEXT("No moves left this month."));
		Tray.Hint = FText::FromString(TEXT("Right-click a friendly place and they march there from next month."));
		// (The playtester's run of 17 November: a garrison always reads "no moves", and was promised a march here.)
		const FString Garrison = GarrisonSays();
		if (!Garrison.IsEmpty())
		{
			Tray.HintLead = FText::GetEmpty();
			// (The UI director, 18 November 2026: the card's line above already says whose garrison they are. Where the
			// card carries TAKE THE FIELD, the foot says only what to do.)
			Tray.Hint = FText::FromString(ComesOut()
				? FString(TEXT("Right-click where they should go, or drag the flag there: they come out of the town on the way. Esc puts them down."))
				: !Tray.FieldButton.IsEmpty()
				? FString(TEXT("To march, TAKE THE FIELD first. Right-click the town for its orders. Esc puts them down."))
				: Garrison + GarrisonWhere() + TEXT(" Esc puts them down."));
		}
	}
	if (StandingRoad.Num() > 0 && !bBySea && !bByRail)
	{
		// (One voice after a press.) The order is given and the paper line has said so: the foot says only what a press
		// would do now, with no brass lead.
		Tray.HintLead = FText::GetEmpty();
		Tray.Hint = FText::FromString(TEXT("Right-click another place to change the order."));
	}
	const FReachRegion* Target = FindReach(RegionId);
	if (Target && Map.IsValid() && Map->GetData().Regions.IsValidIndex(RegionId))
	{
		const FString Name = Map->GetData().Regions[RegionId].Name;
		Tray.PendingButton = FText::FromString(FString::Printf(TEXT("%s %s"),
			bBySea ? (Target->bAttack ? TEXT("LAND AT") : TEXT("BY SEA TO")) : bByRail ? TEXT("BY RAIL TO") : Target->IsWar() ? TEXT("INVADE") : (Target->bAttack ? TEXT("ATTACK") : TEXT("MARCH TO")), *Name.ToUpper()));
		Tray.HintLead = FText::GetEmpty();
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
	{
		const FString Garrison = GarrisonSays();
		// (a garrison's flag set down where it could go this month once out: the order goes on, as OrderTo has it)
		if (!Garrison.IsEmpty() && !(bOnTable && ComesOutTo(RegionId)))
		{
			// (a garrison's flag set down somewhere: it goes back, and is told how it can be moved)
			Piece->ReturnToRest();
			if (bOnTable && RegionId != INDEX_NONE && RegionId != ArmyRegion)
			{
				Notify(Garrison + GarrisonWhere(), 10.0);
			}
			return;
		}
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
		// (The code checker, 17 November.) Set down anywhere out of this month's reach, the flag goes back and the drop
		// is answered as a right-click there is: a kept march (ruling 87), the train, the march up to a far enemy and
		// its attack, the question of war on a country at peace, or the rules' reason why not. One test for both.
		Piece->ReturnToRest();
		OrderTo(RegionId);
	}
	// Whatever is left of the army where it started goes back to its place.
	if (HeldPiece.IsValid())
	{
		HeldPiece->ReturnToRest();
	}
}

void ACampaignPlayerController::SelectRegion(int32 RegionId)
{
	THEATER_SPEED_SCOPE(SelectRegion);
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
	TArray<int32> Orderable, Within;
	for (const FViewCorps& Unit : View.Corps)
	{
		if (Unit.Region == RegionId && Unit.Nation == View.Human && Unit.bCanOrder)
		{
			(Unit.bInTown ? Within : Orderable).Add(Unit.Id);
		}
	}
	// (The code checker, 17 November.) The army picked up is the army in the field. The town's garrison stays in its
	// town: it was being handed marches it cannot make, and the order vanished without a word. Where nobody stands in
	// the field the garrison is what is in hand, so that its cards and its orders can still be read.
	if (Orderable.Num() == 0)
	{
		Orderable = MoveTemp(Within);
	}
	if (Orderable.Num() == 0)
	{
		return false;
	}
	const bool bNewInHand = ArmyRegion != RegionId;
	PutDownAgent(); // (an army in hand: the agent is let go)
	if (bNewInHand)
	{
		bHandOrdered = false; // (one voice after a press: a new hand, and no order given it yet)
	}
	ArmyRegion = RegionId;
	AdviserSaw(0);
	PickedCorps = MoveTemp(Orderable);
	RefreshOrders();
	if (bNewInHand)
	{
		// (Rules 84.2.4; the playtester's case 2.) A garrison in hand says at once how it comes out of its town.
		const FString Garrison = GarrisonSays();
		if (!Garrison.IsEmpty() && ComesOut())
		{
			// (release 2, Brian, 24 November 2026: no press first; an order to march or attack brings them out)
			Notify(TEXT("They are inside the town. Send them anywhere they can go this month: they come out of it as the order goes."), 8.0);
		}
		else if (!Garrison.IsEmpty())
		{
			Notify(Garrison + GarrisonWhere(), 10.0);
		}
	}
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
	PickedDivisions.Reset();
	bHandOrdered = false;
	RefreshOrders();
}

void ACampaignPlayerController::ToggleDivision(int32 CorpsId, int32 Slot)
{
	const FTrayCard* Card = Tray.Cards.FindByPredicate([CorpsId](const FTrayCard& C) { return C.CorpsId == CorpsId; });
	if (ArmyRegion == INDEX_NONE || !Card || Slot < 0)
	{
		return;
	}
	if (!Card->bCanBreak)
	{
		if (!Card->BreakWhy.IsEmpty())
		{
			Notify(Card->BreakWhy, 6.0);
		}
		return;
	}
	TSet<int32>& Set = PickedDivisions.FindOrAdd(CorpsId);
	if (Set.Contains(Slot))
	{
		Set.Remove(Slot);
	}
	else
	{
		Set.Add(Slot);
	}
	if (Set.Num() == 0)
	{
		PickedDivisions.Remove(CorpsId);
	}
	CampaignSounds::Play(this, TEXT("click"), 0.4f);
	RefreshOrders();
}

void ACampaignPlayerController::SplitOffDivisions(int32 CorpsId)
{
	FCampaignSession* Session = GetSession();
	const TSet<int32>* Set = PickedDivisions.Find(CorpsId);
	if (!Session || !Set || Set->Num() == 0)
	{
		Notify(TEXT("Click the tiles of the divisions to split off first."), 5.0);
		return;
	}
	TArray<int32> Slots = Set->Array();
	Slots.Sort();
	TArray<FString> Parts;
	for (const int32 Slot : Slots)
	{
		Parts.Add(FString::FromInt(Slot));
	}
	FString Note, Next;
	TArray<FSortie> Sorties;
	FActExtras Extras;
	if (Session->Act(FString::Printf(TEXT("breakup:%d:%s"), CorpsId, *FString::Join(Parts, TEXT(","))), INDEX_NONE, Note, &Sorties, &Next, &Extras))
	{
		// The picks are spent; the new corps stays behind when the army moves (it is not among the corps picked to go).
		PickedDivisions.Remove(CorpsId);
		if (Extras.Made != INDEX_NONE)
		{
			PickedCorps.Remove(Extras.Made);
		}
		CampaignSounds::Play(this, TEXT("stamp"), 0.5f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
	// (a refused split keeps the picks, so they can be changed and tried again)
	if (!Note.IsEmpty())
	{
		Notify(Note, 6.0);
	}
	RefreshOrders();
}

void ACampaignPlayerController::RejoinCorps(int32 CorpsId, int32 IntoId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || CorpsId == INDEX_NONE || IntoId == INDEX_NONE)
	{
		return;
	}
	FString Note;
	if (Session->Act(FString::Printf(TEXT("rejoin:%d:%d"), CorpsId, IntoId), INDEX_NONE, Note))
	{
		PickedCorps.Remove(CorpsId);
		PickedDivisions.Remove(CorpsId);
		PickedDivisions.Remove(IntoId); // (its tiles are laid out afresh: the old picks no longer point at the same divisions)
		CampaignSounds::Play(this, TEXT("stamp"), 0.5f);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
	}
	if (!Note.IsEmpty())
	{
		Notify(Note, 6.0);
	}
	RefreshOrders();
}

void ACampaignPlayerController::ToggleCorps(int32 CorpsId)
{
	if (ArmyRegion == INDEX_NONE)
	{
		return;
	}
	if (const FTrayCard* Card = Tray.Cards.FindByPredicate([CorpsId](const FTrayCard& C) { return C.CorpsId == CorpsId; }))
	{
		if (!Card->Apart.IsEmpty())
		{
			Notify(Card->Apart, 7.0); // (the garrison is not slipped into the field army's hand: the code checker, 17 November)
			return;
		}
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
	THEATER_SPEED_SCOPE(RefreshOrders);
	FCampaignSession* Session = GetSession();
	bLessonsDue = true; // (Rules 84.3.3: an order of his may have done a lesson's deed; the notes are asked once it is all in)
	if (PendingEnemyReach.IsSet())
	{
		// (an enemy's reach put off by LaterOrders and not yet shown: it is shown first, as it was before the steps)
		if (Map.IsValid())
		{
			Map->ShowEnemyReach(PendingEnemyReach.GetValue());
		}
		PendingEnemyReach.Reset();
	}
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
		Card.Men = Unit.Men;
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
		// (Release 2, breakup.ts) Its division tiles can be picked and split off, where that is on and it can be done now.
		Card.bCanBreak = Unit.bBreakKnown && Unit.BreakWhy.IsEmpty() && Unit.Divisions >= 2;
		Card.BreakWhy = Unit.BreakWhy;
		Card.RejoinId = Unit.RejoinId;
		Card.RejoinName = Unit.RejoinName;
		Card.Carry = Unit.Carry; // (Version 84)
		Card.bDigging = Unit.bDigging;
		Card.Experience = Unit.Experience;
		Card.Moves = Unit.Moves;
		Card.MovesMax = Unit.MovesMax;
		if (Unit.MarchTo != INDEX_NONE && Map.IsValid() && Map->GetData().Regions.IsValidIndex(Unit.MarchTo))
		{
			// (One voice after a press.) The lasting fact is each corps' own line: on its way, or set off from next month.
			Card.Marching = Unit.bMarchNext ? FString::Printf(TEXT("Marches to %s from next month"), *Map->GetData().Regions[Unit.MarchTo].Name)
				: FString::Printf(TEXT("Marching to %s, about %s"), *Map->GetData().Regions[Unit.MarchTo].Name,
				Unit.MarchMonths <= 1 ? TEXT("a month") : *FString::Printf(TEXT("%d months"), Unit.MarchMonths));
		}
		Card.bCanOrder = Unit.bCanOrder;
		Card.NoOrders = Unit.NoOrders;
		Card.bInTown = Unit.bInTown;
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
	// (The code checker, 17 November.) With the army in the field in hand, the town's garrison cannot be clicked in among
	// them: it marches nowhere until it takes the field, and would be left behind without a word. Its card says why, in
	// the rules' refusal of 84.2.4 that names the town (the UI director, 17 November: no sentence of the 3D game's own).
	if (Tray.Cards.ContainsByPredicate([](const FTrayCard& C) { return C.bPicked && !C.bInTown; }))
	{
		for (FTrayCard& Card : Tray.Cards)
		{
			if (Card.bInTown && !Card.bPicked && Card.bCanOrder)
			{
				const FViewCorps* Inside = View.Corps.FindByPredicate([&Card](const FViewCorps& Each) { return Each.Id == Card.CorpsId; });
				Card.Apart = Inside && !Inside->Garrison.IsEmpty() ? Inside->Garrison : Session->GarrisonWhy(ArmyRegion);
			}
		}
	}

	const FString Adjective = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : View.Human;
	const FString Where = Map->GetData().Regions.IsValidIndex(ArmyRegion) ? Map->GetData().Regions[ArmyRegion].Name : FString();
	Tray.bVisible = true;
	Tray.Color = Nation ? FLinearColor(Nation->Color) : FLinearColor::Gray;
	Tray.Tag = View.Human;
	Tray.Title = FText::FromString(General.IsEmpty() ? FString::Printf(TEXT("%s army"), *Adjective) : General);
	// (One voice after a press.) The army as the tray's head names it, for the rules' sentences, and the corps it has in
	// the field (the town's garrison where nobody stands outside), as an army is picked up.
	HandArmy = General.IsEmpty() ? FString::Printf(TEXT("the %s army"), *Adjective) : FString::Printf(TEXT("%s's army"), *General);
	HandArmyIds.Reset();
	{
		TArray<int32> InTown;
		for (const FViewCorps& Unit : View.Corps)
		{
			if (Unit.Region == ArmyRegion && Unit.Nation == View.Human && Unit.bCanOrder)
			{
				(Unit.bInTown ? InTown : HandArmyIds).Add(Unit.Id);
			}
		}
		if (HandArmyIds.Num() == 0)
		{
			HandArmyIds = MoveTemp(InTown);
		}
	}
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
	// The town's orders for the army in hand, on its own tray (Brian, 18 November 2026): storm the town it stands
	// round, go into the town of the province it stands in, or come out of it. They are the province's own orders,
	// offered by the rules; a storm still opens its card first (ruling 182).
	Tray.StormButton = Tray.StormTip = Tray.GarrisonButton = Tray.GarrisonTip = Tray.FieldButton = FText::GetEmpty();
	Tray.StormTown.Reset();
	Tray.bStormFavoured = false;
	Tray.GarrisonActions.Reset();
	Tray.FieldActions.Reset();
	if (ArmyRegion != INDEX_NONE && PickedCorps.Num() > 0 && View.bHumanUp)
	{
		FString Town;
		for (const FProvinceOption& Option : Session->Province(ArmyRegion).Options)
		{
			if (!Option.bEnabled)
			{
				continue;
			}
			if (Option.Action == TEXT("work:storm"))
			{
				Tray.StormButton = FText::FromString(Option.Label.ToUpper()); // ("Storm Panama City")
				Tray.StormTown = Option.Label;
				Tray.StormTip = FText::FromString(Option.Detail + TEXT(" Its card shows the odds first, and KEEP UP THE SIEGE leaves it be."));
			}
			else if (Option.Action.StartsWith(TEXT("work:garrison:")) && PickedCorps.Contains(FCString::Atoi(*Option.Action.Mid(14))))
			{
				Tray.GarrisonActions.Add(Option.Action);
				FString Who;
				Option.Label.Split(TEXT(": into "), &Who, &Town); // ("First Army: into New York")
			}
			else if (Option.Action.StartsWith(TEXT("work:field:")) && PickedCorps.Contains(FCString::Atoi(*Option.Action.Mid(11))))
			{
				Tray.FieldActions.Add(Option.Action);
			}
		}
		if (!Tray.StormButton.IsEmpty())
		{
			// (The UI director, 18 November 2026: one lit button on a card, and the light is the staff's pointer. STORM
			// is lit only while the staff's forecast favours the assault. The forecast is read off a copy of the
			// campaign, so it is asked once for a hand, a place and a month.)
			FString Key = FString::Printf(TEXT("%d:%d:"), View.Turn, ArmyRegion);
			for (const int32 Id : PickedCorps)
			{
				Key += FString::Printf(TEXT("%d,"), Id);
			}
			if (Key != StormFavourKey)
			{
				StormFavourKey = Key;
				const FAttackPreview Storm = Session->PreviewStorm(ArmyRegion);
				bStormFavourKept = Storm.bValid && Storm.Refused.IsEmpty() && Storm.Favours == TEXT("storm");
			}
			Tray.bStormFavoured = bStormFavourKept;
		}
		if (Tray.GarrisonActions.Num() > 0)
		{
			Tray.GarrisonButton = FText::FromString(Town.IsEmpty() ? FString(TEXT("GARRISON THE TOWN")) : TEXT("GARRISON ") + Town.ToUpper());
			Tray.GarrisonTip = FText::FromString(TEXT("The corps in hand go into the town and hold its walls. A town with troops inside does not fall to an army that only marches in: it must be besieged or stormed. Inside, they neither march nor attack until they TAKE THE FIELD again."));
		}
		if (Tray.FieldActions.Num() > 0)
		{
			Tray.FieldButton = FText::FromString(TEXT("TAKE THE FIELD"));
		}
	}
	// (Version 64) Two corps of the army worn thin, which could be folded into one.
	Tray.FoldButton = FText::GetEmpty();
	if (const FString* Worn = View.Folds.Find(ArmyRegion))
	{
		Tray.FoldButton = FText::FromString(TEXT("FOLD WORN CORPS"));
		Tray.FoldTip = FText::FromString(FString::Printf(TEXT("%s are worn thin. The men of both can fill one corps, under the better of the two generals; the corps that is emptied goes home as a cadre, to be raised again from the arsenal at half the price of a new corps. Nothing is done until you have seen what it makes of them."), **Worn));
	}
	// The model of one of its corps stands beside the cards: the one last pointed at, or the first.
	{
		const bool bStill = Tray.Cards.ContainsByPredicate([this](const FTrayCard& Card) { return Card.CorpsId == ShowCorps; });
		const int32 Wanted = bStill ? ShowCorps : (Tray.Cards.Num() > 0 ? Tray.Cards[0].CorpsId : INDEX_NONE);
		ShowCorps = INDEX_NONE; // so that it is set afresh
		ShowCorpsModel(Wanted);
	}

	TArray<int32> Danger, Road;
	TMap<int32, int32> Far;
	bHandSpent = false;
	bHandStands = ArmyRegion != INDEX_NONE && Session && Session->Stands(ArmyRegion) == TEXT("all");
	KeptPlanRegion = -2; // (whatever was planned was planned for another hand)
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
		Reach = Session->Reach(PickedCorps, &Danger, &Far, &Road, &bHandSpent);
	}
	EnemyReach = TSet<int32>(Danger);
	FarMonths = MoveTemp(Far);
	StandingRoad = MoveTemp(Road);
	Map->SetReach(Reach, bByRail || bBySea, bOrdersInSteps); // (in steps, the enemy's old reach stands until its own step below)
	// (Rulings 110, 112, 118; the art director's railway brief.) The second reach, with no switch to press: where a
	// train would take the army this month beyond its march, shown as light on the railway itself.
	RailBeyond.Reset();
	Tray.bRailCan = false;
	Tray.RailStrong.Reset();
	Tray.RailLine.Reset();
	if (!bBySea && !bByRail && Session && PickedCorps.Num() > 0 && ArmyRegion != INDEX_NONE)
	{
		TArray<int32> Walked;
		for (const FReachRegion& Step : Reach)
		{
			if (!Step.bAttack && !Step.IsWar())
			{
				Walked.Add(Step.Region);
			}
		}
		FRailShow Trains = Session->RailShow(PickedCorps, Walked, HandArmy, HandArmyIds);
		Trains.Light.From = ArmyRegion;
		RailBeyond = TSet<int32>(Trains.Light.Tint);
		Tray.bRailCan = Trains.bCan;
		Tray.RailStrong = Trains.Strong;
		Tray.RailLine = Trains.Line;
		Map->SetRailLight(Trains.Light);
	}
	// (how far the enemy could strike next month: only for an army that marches)
	if (bOrdersInSteps)
	{
		// (Speed, b414 mark 2: after an order's answer, a frame of its own. See LaterOrders.)
		PendingEnemyReach = Danger;
		Later([this]()
		{
			if (PendingEnemyReach.IsSet() && Map.IsValid())
			{
				Map->ShowEnemyReach(PendingEnemyReach.GetValue());
			}
			PendingEnemyReach.Reset();
		});
	}
	else
	{
		Map->ShowEnemyReach(Danger);
	}
	RingedPiece = Map->FindPiece(ArmyRegion, View.Human);
	if (RingedPiece.IsValid())
	{
		RingedPiece->SetSelected(true);
	}
	// (an army on a standing march shows the road still ahead of it while it is in hand: drawn from its flag, so
	// once the flag is known)
	RouteTarget = -2;
	ShowRouteTo(INDEX_NONE);
	SetPending(INDEX_NONE); // sets the line that says what to do next
	--Tray.Revision; // already counted above
}

void ACampaignPlayerController::ShowRouteTo(int32 RegionId)
{
	THEATER_SPEED_SCOPE(ShowRouteTo);
	if (RegionId == RouteTarget || !Map.IsValid())
	{
		return;
	}
	RouteTarget = RegionId;
	// (the railway brief, section 8: the route a train would take to the province under the pointer turns white)
	Map->SetRailRoute(RailBeyond.Contains(RegionId) ? RegionId : INDEX_NONE);
	// (Ruling 132(a): one route under the pointer, the one the right-click will take. Where a train reaches the
	// place this month that is the lit rail route alone; the kept march's dashed road is for ground no train
	// reaches, and for an army that cannot ride this month.)
	if (bByRail || RailBeyond.Contains(RegionId))
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
	bool bKeptRoad = false;
	if (Road.Num() == 0 && !bBySea)
	{
		// (Ruling 87.) Friendly ground out of this month's reach, and any friendly ground for an army with no moves
		// left: the road the kept march would take, from the flag to the place.
		if (RegionId != INDEX_NONE && RegionId != ArmyRegion && FarMonths.Contains(RegionId))
		{
			const FMarchPlan& KeptWay = PlanFor(RegionId);
			if (KeptWay.bOk && !KeptWay.bNow)
			{
				Road = KeptWay.Road;
			}
		}
		if (Road.Num() == 0)
		{
			Road = StandingRoad; // (nothing in reach is pointed at: the road of its standing march, if it has one)
		}
		bKeptRoad = Road.Num() > 0;
	}
	const FVector From = RingedPiece.IsValid() ? RingedPiece->GetRestLocation() : FVector::ZeroVector;
	const FReachRegion* Ends = FindReach(RegionId);
	Map->SetRoute(From, RingedPiece.IsValid() ? Road : TArray<int32>(), bKeptRoad ? 3 : Ends && Ends->IsWar() ? 2 : Ends && Ends->bAttack ? 1 : 0);
}

bool ACampaignPlayerController::OpenApproach(int32 RegionId, bool bSayNone)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0)
	{
		return false;
	}
	const FApproachPlan Plan = Session->PlanApproach(PickedCorps, RegionId);
	if (!Plan.bOk)
	{
		if (bSayNone && !Plan.None.IsEmpty())
		{
			Notify(Plan.None, 11.0); // (nothing beside the enemy can be reached this month: the rules say so and name the nearest)
			return true;
		}
		return false;
	}
	const FCampaignView& View = Session->GetView();
	WarPrompt = FWarPrompt();
	WarPrompt.bVisible = true;
	WarPrompt.bApproach = true;
	WarPrompt.ApproachTo = RegionId;
	WarPrompt.Region = RegionId;
	WarPrompt.Ways = Plan.Rows;
	WarPrompt.Way = 0;
	// (whose army is marched on: its flag heads the card)
	if (const FViewCorps* Foe = View.Corps.FindByPredicate([&](const FViewCorps& Each) { return Each.Region == RegionId && Each.Nation != View.Human && !Each.bInTown; }))
	{
		WarPrompt.Tag = Foe->Nation;
	}
	WarPrompt.Name = Plan.Name;
	if (!Plan.Foe.IsEmpty())
	{
		WarPrompt.Tag = Plan.Foe;
	}
	// (Rules 84.5; the UI director's sheet 34.) The card in the landing card's parts: the kicker and the date, the title,
	// the two sides with THE ROAD between them, and the fixed foot, CALL IT OFF and MARCH UP. Its words are the sheet's.
	WarPrompt.Title = FText::FromString(FString::Printf(TEXT("Marching on %s"), *Plan.Name));
	WarPrompt.Kicker = FString::Printf(TEXT("MARCH UP AND ATTACK  \u00B7  %s"), *View.Date.ToUpper());
	WarPrompt.OursTag = View.Human;
	// (One voice after a press: who is named.) The whole army in hand as the tray's head names it, part of it as "2 corps
	// of the American army", and one corps alone by its name without the bracket.
	if (PickedCorps.Num() == 1)
	{
		const FViewCorps* Alone = View.Corps.FindByPredicate([this](const FViewCorps& Each) { return Each.Id == PickedCorps[0]; });
		FString Called = Alone ? Alone->Name : HandArmy;
		int32 Open = INDEX_NONE;
		if (Called.EndsWith(TEXT(")")) && Called.FindLastChar(TEXT('('), Open) && Called.Mid(Open).Contains(TEXT("Division")))
		{
			Called = Called.Left(Open).TrimEnd();
		}
		WarPrompt.OursName = Called;
	}
	else if (PickedCorps.Num() == HandArmyIds.Num() && !PickedCorps.ContainsByPredicate([this](int32 Id) { return !HandArmyIds.Contains(Id); }))
	{
		WarPrompt.OursName = HandArmy;
	}
	else
	{
		WarPrompt.OursName = FString::Printf(TEXT("%d corps of %s"), PickedCorps.Num(), *HandArmy);
	}
	if (!WarPrompt.OursName.IsEmpty())
	{
		WarPrompt.OursName[0] = FChar::ToUpper(WarPrompt.OursName[0]);
	}
	WarPrompt.OursLine = Plan.Ours;
	WarPrompt.FoeName = Plan.FoeName;
	WarPrompt.FoeLine = Plan.FoeLine;
	WarPrompt.From = Plan.From;
	WarPrompt.WaysHead = FText::FromString(TEXT("THE APPROACH"));
	WarPrompt.WaysNote = FText::FromString(Plan.Rows.Num() > 1 ? TEXT("press a row to go another way") : TEXT("the only one in reach this month"));
	WarPrompt.Confirm = FText::FromString(TEXT("MARCH UP"));
	WarPrompt.Cancel = FText::FromString(TEXT("CALL IT OFF"));
	WarPromptSince = FPlatformTime::Seconds();
	DressApproach();
	AdviserSaw(1);
	CampaignSounds::Play(this, TEXT("click"), 0.5f);
	return true;
}

void ACampaignPlayerController::DressApproach()
{
	if (!WarPrompt.bApproach || !WarPrompt.Ways.IsValidIndex(WarPrompt.Way) || !Map.IsValid())
	{
		return;
	}
	const FApproachRow& Way = WarPrompt.Ways[WarPrompt.Way];
	// (Rules 84.5; sheet 34, the words passed at 16:23 UTC.) The headline is what he gets on arrival, bright where he can
	// attack this month and amber where the attack waits; under it the staff's pick and what Enter does.
	const FString Army = WarPrompt.OursName.IsEmpty() ? FString(TEXT("The army")) : WarPrompt.OursName;
	const FString ArmyMid = Army.StartsWith(TEXT("The "), ESearchCase::CaseSensitive) ? TEXT("the") + Army.Mid(3) : Army; // (inside a sentence)
	WarPrompt.Road = FString::Printf(TEXT("from %s to %s, beside the enemy"), *WarPrompt.From, *Way.Name);
	WarPrompt.Headline = FText::FromString(Way.Arrive);
	WarPrompt.bHeadAmber = !Way.bAttack;
	FString Under;
	if (Way.bAttack)
	{
		Under = WarPrompt.Way == 0 ? FString::Printf(TEXT("The staff takes you by %s, the ground beside the enemy that is reached soonest. "), *Way.Name) : FString();
		Under += FString::Printf(TEXT("When %s arrives the attack card opens, with the odds in front of you."), *ArmyMid);
	}
	else
	{
		Under = FString::Printf(TEXT("The march alone is carried out. %s ends the month at %s, beside the enemy."), *Army, *Way.Name);
	}
	WarPrompt.Under = FText::FromString(Under);
	WarPrompt.Body = FText::GetEmpty();
	WarPrompt.Last = FText::GetEmpty();
	// The road is drawn: to the approach, and on to the enemy.
	TArray<int32> Road = Way.Road;
	Road.Add(WarPrompt.ApproachTo);
	const FVector From = RingedPiece.IsValid() ? RingedPiece->GetRestLocation() : FVector::ZeroVector;
	Map->SetRailRoute(INDEX_NONE);
	Map->SetRoute(From, RingedPiece.IsValid() ? Road : TArray<int32>(), 1);
	RouteTarget = -3; // (whatever the pointer rests on next is drawn afresh)
	Map->SetHoveredRegion(Way.Region);
}

void ACampaignPlayerController::PickApproach(int32 Index)
{
	if (WarPrompt.bVisible && WarPrompt.bApproach && WarPrompt.Ways.IsValidIndex(Index) && Index != WarPrompt.Way)
	{
		WarPrompt.Way = Index;
		DressApproach();
		CampaignSounds::Play(this, TEXT("click"), 0.5f);
	}
}

void ACampaignPlayerController::CarryOutApproach(const FWarPrompt& Asked)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !Asked.Ways.IsValidIndex(Asked.Way) || ArmyRegion == INDEX_NONE)
	{
		return;
	}
	const FApproachRow Way = Asked.Ways[Asked.Way];
	const int32 Enemy = Asked.ApproachTo;
	RouteTarget = -3;
	// (The code checker, 17 November.) Only the corps that can reach that province this month are sent by its row: the
	// rest of the hand stays where it is, and is given no march to a later month.
	if (Way.Going.Num() > 0)
	{
		PickedCorps.RemoveAll([&Way](int32 Id) { return !Way.Going.Contains(Id); });
		if (PickedCorps.Num() == 0)
		{
			PickedCorps = Way.Going;
		}
	}
	TMap<int32, int32> StoodAt;
	for (const FViewCorps& Unit : Session->GetView().Corps)
	{
		if (PickedCorps.Contains(Unit.Id))
		{
			StoodAt.Add(Unit.Id, Unit.Region);
		}
	}
	// (the army as the tray named it when the order was given: the paper names it the same way if it stops short)
	const FString ArmyWas = HandArmy;
	const TArray<int32> ArmyIdsWas = HandArmyIds;
	// The march, as a right-click on the approach would give it (the same order, the same words where it says any).
	RefreshOrders();
	OrderTo(Way.Region);
	if (FaceOff.bVisible || WarPrompt.bVisible)
	{
		return; // (it has a question of its own to ask first)
	}
	{
		// Where the corps set off and none of them got there, the paper says where the army stops, naming it as the rest
		// of "One voice after a press" does (the UI director, 17 November). Where none set off the order has already said
		// why, and where the march goes on by itself next month the kept march's own sentence has said so.
		FString StoppedAt;
		bool bArrived = false, bKept = false;
		TArray<const FViewCorps*> Went;
		for (const FViewCorps& Unit : Session->GetView().Corps)
		{
			const int32* From = StoodAt.Find(Unit.Id);
			if (!From || Unit.Region == *From)
			{
				continue;
			}
			Went.Add(&Unit);
			bArrived = bArrived || Unit.Region == Way.Region;
			bKept = bKept || Unit.MarchTo != INDEX_NONE;
			if (StoppedAt.IsEmpty() && Map->GetData().Regions.IsValidIndex(Unit.Region))
			{
				StoppedAt = Map->GetData().Regions[Unit.Region].Name;
			}
		}
		if (!bArrived)
		{
			if (Went.Num() > 0 && !bKept && !StoppedAt.IsEmpty())
			{
				// Whole army "The American army"; part "2 corps of the American army"; one corps by its name, without the bracket.
				FString Who;
				if (Went.Num() == 1)
				{
					Who = Went[0]->Name;
					int32 Open = INDEX_NONE;
					if (Who.EndsWith(TEXT(")")) && Who.FindLastChar(TEXT('('), Open) && Who.Mid(Open).Contains(TEXT("Division")))
					{
						Who = Who.Left(Open).TrimEnd();
					}
				}
				else if (Went.Num() == ArmyIdsWas.Num() && !Went.ContainsByPredicate([&ArmyIdsWas](const FViewCorps* Each) { return !ArmyIdsWas.Contains(Each->Id); }))
				{
					Who = ArmyWas;
				}
				else
				{
					Who = FString::Printf(TEXT("%d corps of %s"), Went.Num(), *ArmyWas);
				}
				if (!Who.IsEmpty())
				{
					Who[0] = FChar::ToUpper(Who[0]);
					Notify(FString::Printf(TEXT("%s stops at %s this month, short of %s."), *Who, *StoppedAt, *Way.Name), 9.0);
				}
			}
			return;
		}
	}
	if (ArmyRegion != Way.Region)
	{
		return;
	}
	const FReachRegion* There = FindReach(Enemy);
	if (There && There->bAttack)
	{
		OpenFaceOff(Enemy, true); // (the plan he was shown: with a move left the attack card opens on arrival)
		if (FaceOff.bVisible)
		{
			return;
		}
	}
	// (Rules 84.5; sheet 34.) The march alone was carried out: what the paper says after Enter on the amber card.
	Notify(FString::Printf(TEXT("%s has marched to %s. No move is left this month: right-click the army round %s next month to plan the attack."),
		Asked.OursName.IsEmpty() ? TEXT("The army") : *Asked.OursName, *Way.Name, *Asked.Name), 9.0);
}

bool ACampaignPlayerController::ComesOut() const
{
	return ArmyRegion != INDEX_NONE && PickedCorps.Num() > 0 && !bBySea && !bByRail && Reach.Num() > 0
		&& Tray.FieldActions.Num() >= PickedCorps.Num() && !GarrisonSays().IsEmpty();
}

bool ACampaignPlayerController::ComesOutTo(int32 RegionId) const
{
	return RegionId != INDEX_NONE && RegionId != ArmyRegion && FindReach(RegionId) != nullptr && ComesOut();
}

FString ACampaignPlayerController::GarrisonWhere() const
{
	// (The design director, 17 November: no hint sends him to a press that does nothing. The sortie and the order to
	// take the field stand under ORDERS on the province's card, and a right-click on the garrison's own town, or on
	// the army round it, opens that card there.)
	return TEXT(" Right-click the town to open its orders.");
}

FString ACampaignPlayerController::GarrisonSays() const
{
	// (The rules builder, 17 November: corps in a town neither march nor attack until they take the field; a garrison
	// shut in attacks by sortie. The code checker: a garrison in hand was being promised marches it would never make.
	// The sentence is the rules' own (whyNotOrderGarrison, rules 84.2.4); the same words stand here for a save or a
	// bundle that has none.)
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || PickedCorps.Num() == 0)
	{
		return FString();
	}
	const FCampaignView& View = Session->GetView();
	FString First;
	for (const int32 Id : PickedCorps)
	{
		const FViewCorps* Unit = View.Corps.FindByPredicate([Id](const FViewCorps& Each) { return Each.Id == Id; });
		if (!Unit || !Unit->bInTown)
		{
			return FString();
		}
		if (First.IsEmpty())
		{
			First = Unit->Name;
		}
	}
	{
		const FString Rules = Session->GarrisonWhy(ArmyRegion);
		if (!Rules.IsEmpty())
		{
			return Rules;
		}
	}
	auto AtWarWith = [&View](const FString& Tag)
	{
		for (int32 Index = 0; Index + 1 < View.Wars.Num(); Index += 2)
		{
			if ((View.Wars[Index] == View.Human && View.Wars[Index + 1] == Tag) || (View.Wars[Index + 1] == View.Human && View.Wars[Index] == Tag))
			{
				return true;
			}
		}
		return false;
	};
	const int32 Here = ArmyRegion;
	const bool bShutIn = View.Corps.ContainsByPredicate([&](const FViewCorps& Each) { return Each.Region == Here && !Each.bInTown && Each.Nation != View.Human && AtWarWith(Each.Nation); });
	const FTownInfo* Town = Map->TownOf(ArmyRegion);
	const FString Place = Town && !Town->Name.IsEmpty() ? Town->Name : (Map->GetData().Regions.IsValidIndex(ArmyRegion) ? Map->GetData().Regions[ArmyRegion].Name : FString(TEXT("the town")));
	const bool bOne = PickedCorps.Num() == 1 && !First.IsEmpty();
	const FString Who = bOne ? First + TEXT(" is") : FString(TEXT("They are"));
	return bShutIn
		? FString::Printf(TEXT("%s the garrison of %s, shut in by the army round it. Order a sortie against that army."), *Who, *Place)
		: FString::Printf(TEXT("%s the garrison of %s. Order %s to take the field first."), *Who, *Place, bOne ? TEXT("it") : TEXT("them"));
}

FString ACampaignPlayerController::WhyNoAttackOn(int32 RegionId) const
{
	const FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || !Map->GetData().Regions.IsValidIndex(RegionId) || !Map->GetData().Regions.IsValidIndex(ArmyRegion))
	{
		return FString();
	}
	const FCampaignView& View = Session->GetView();
	auto AtWarWith = [&View](const FString& Tag)
	{
		for (int32 Index = 0; Index + 1 < View.Wars.Num(); Index += 2)
		{
			if ((View.Wars[Index] == View.Human && View.Wars[Index + 1] == Tag) || (View.Wars[Index + 1] == View.Human && View.Wars[Index] == Tag))
			{
				return true;
			}
		}
		return false;
	};
	// An enemy army in the field there, as far as he can see.
	if (!View.Corps.ContainsByPredicate([&](const FViewCorps& Each) { return Each.Region == RegionId && Each.Nation != View.Human && AtWarWith(Each.Nation); }))
	{
		return FString();
	}
	const FString& Name = Map->GetData().Regions[RegionId].Name;
	if (View.bEve)
	{
		// (Rules 84.2.3: on the eve of war no attack goes in. The browser game's own sentence.)
		return TEXT("On the eve of war every army stays where it is placed: no attack goes in until the war has opened.");
	}
	if (Map->GetData().Regions[ArmyRegion].Neighbors.Contains(RegionId))
	{
		// Next door, and still no attack: the ground between bars the way (the rules' own bar: rivers.ts).
		return FString::Printf(TEXT("This army cannot attack the enemy at %s from here: the ground between bars its way. A river with its bridges down stops wheels and tracks, and tanks cannot climb the steepest ground."), *Name);
	}
	// The nearest ground beside the enemy that this army can march to this month.
	const FReachRegion* Beside = nullptr;
	for (const FReachRegion& Each : Reach)
	{
		if (!Each.bAttack && !Each.IsWar() && Map->GetData().Regions.IsValidIndex(Each.Region) && Map->GetData().Regions[Each.Region].Neighbors.Contains(RegionId) && (!Beside || Each.Cost < Beside->Cost))
		{
			Beside = &Each;
		}
	}
	if (Beside)
	{
		return FString::Printf(TEXT("An army attacks from the province next door. March to %s first, beside the enemy at %s, then right-click the enemy again."), *Map->GetData().Regions[Beside->Region].Name, *Name);
	}
	// (a train runs only through friendly ground: it is offered where the enemy stands on his own or an ally's ground)
	const FString Standing = View.RegionStanding.IsValidIndex(RegionId) ? View.RegionStanding[RegionId] : FString();
	return FString::Printf(TEXT("An army attacks from the province next door, and %s is too far to come up beside this month. %s"), *Name,
		(Standing == TEXT("own") || Standing == TEXT("ally")) ? TEXT("March toward it, or send the army by train to the province beside it.") : TEXT("March toward it: the army can attack once it stands beside it."));
}

void ACampaignPlayerController::OrderTo(int32 RegionId, const FVector* CarriedFrom)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || ArmyRegion == INDEX_NONE || RegionId == ArmyRegion)
	{
		if (Session && Map.IsValid() && ArmyRegion != INDEX_NONE && RegionId == ArmyRegion)
		{
			const FString Garrison = GarrisonSays();
			if (!Garrison.IsEmpty())
			{
				// (The rules builder's cases, 17 November; the playtester's case 2.) The garrison in hand and its own
				// town right-clicked: the rules' sentence says how it comes out (the sortie, or taking the field), and
				// the province's card opens on its orders, where that order stands with its odds.
				SelectRegion(RegionId);
				SetProvinceTab(2);
				// (A garrison shut in: the sortie's card, with its odds and a way to call it off. Where the rules will
				// not have a sortie now, as after one this month, their reason is said by the card's own asking.)
				bool bShutIn = false;
				for (const FProvinceOption& Option : Session->Province(RegionId).Options)
				{
					bShutIn = bShutIn || Option.Action.StartsWith(TEXT("work:sally"));
				}
				if (bShutIn)
				{
					OpenSortieCard(RegionId);
					return;
				}
				FString Refused;
				for (const FProvinceOption& Option : Session->Province(RegionId).Options)
				{
					if (Option.Action.StartsWith(TEXT("work:sally")) && !Option.bEnabled && !Option.Why.IsEmpty())
					{
						Refused = Option.Why;
					}
				}
				Notify(Refused.IsEmpty() ? Garrison + TEXT(" The order is under ORDERS on the province's card.") : Refused, 10.0);
				return;
			}
			// (Ruling 182.) A right-click on the town with the army that stands round it in hand: the assault's card.
			OpenStormCard(RegionId, false);
		}
		return;
	}
	if (PickedCorps.Num() == 0)
	{
		Notify(TEXT("Every corps of this army has been told to stay behind."));
		return;
	}
	{
		const FString Garrison = GarrisonSays();
		// (Release 2 and the 1.5.1 patch, Brian, 24 November 2026: a garrison sent where it could go this month once out
		// is ordered as an army in the field is. The rules bring it out of its town inside the order's own call, and
		// only if the order goes: a card's No, the war question's No or a refusal leaves it inside, nothing spent.)
		if (!Garrison.IsEmpty() && !ComesOutTo(RegionId))
		{
			Notify(Garrison + GarrisonWhere(), 10.0); // (a garrison in hand: it is told how it can be moved, and promised no march)
			return;
		}
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
			WarPrompt.Body = FText::FromString(FString::Join(Plan.Lines, TEXT("\n")));
			if (!Plan.AshoreLine.IsEmpty())
			{
				// (set out as the card's other notes are: a heading, and its line marked)
				WarPrompt.Ashore = FText::FromString(Plan.AshoreHead.ToUpper() + TEXT("\n\u00B7 ") + Plan.AshoreLine);
				WarPrompt.AshoreTone = Plan.AshoreTone == TEXT("lost") ? 3 : Plan.AshoreTone == TEXT("even") ? 2 : 0;
			}
			WarPrompt.Last = FText::FromString(TEXT("Once they sail it cannot be taken back."));
			WarPrompt.Warning = FText::FromString(Plan.Warn);
			WarPrompt.Verdict = FText::FromString(Plan.Verdict);
			WarPrompt.VerdictUnder = FText::FromString(Plan.Under);
			WarPrompt.VerdictTone = Plan.Tone == TEXT("red") ? 3 : Plan.Tone == TEXT("amber") ? 2 : Plan.Tone == TEXT("good") ? 1 : 0;
			WarPrompt.UnderTone = Plan.UnderTone == TEXT("amber") ? 2 : 3;
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
	if (!bByRail && !bBySea)
	{
		// (Rules 84.2.5; ruling 211(a).) Guns and engineers never go forward alone: no attack, no storm, no march onto
		// enemy-held ground without a fighting corps. The rules' sentence answers the press, and nothing is ordered.
		const FString Alone = Session->WhyNotLead(PickedCorps, RegionId);
		if (!Alone.IsEmpty())
		{
			Notify(Alone, 7.0);
			return;
		}
	}
	if (!Target && !bByRail && !bBySea && !bHandSpent && OpenApproach(RegionId, false))
	{
		// (The design director, 17 November.) An enemy army that is not next door, with ground beside it in this
		// month's reach: the march up and the attack are planned as one order, before any train or kept march is thought of.
		return;
	}
	if (!Target && !bByRail && !bBySea && RailBeyond.Contains(RegionId))
	{
		// (Ruling 118.) Beyond its march a right-click takes the train: there this month, the whole month spent.
		FString Said;
		const TArray<FCorpsMove> Trains = Session->Rail(PickedCorps, RegionId, &Said);
		if (Trains.Num() == 0)
		{
			Notify(TEXT("No corps could go: there is no room there, or they have already used their month."));
			return;
		}
		bHandOrdered = true;
		ShowWar(Session->GetView(), Trains, CarriedFrom);
		CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
		CampaignSounds::Play(this, TEXT("truck_engine"), 0.25f, 0.8f);
		PickedCorps.Reset();
		for (const FCorpsMove& Train : Trains)
		{
			PickedCorps.Add(Train.CorpsId);
		}
		ArmyRegion = RegionId;
		if (!Said.IsEmpty())
		{
			Notify(Said, 8.0);
		}
		RefreshOrders();
		SelectRegion(RegionId);
		return;
	}
	if (!Target && !bByRail && FarMonths.Contains(RegionId))
	{
		// (release 1) Friendly ground too far for this month: a march that carries on. The army sets off at once, as
		// far as it can go, and the rules send it on as each month opens.
		AdviserSaw(1);
		const bool bWasSpent = bHandSpent;
		// (One voice after a press.) The rules' one sentence answers the press, naming the army as the tray's head does.
		// (Speed, b414 mark 1.) Answered on the rules' own thread: see AfterFarMarch.
		const TOptional<FVector> Carried = CarriedFrom ? TOptional<FVector>(*CarriedFrom) : TOptional<FVector>();
		const TWeakObjectPtr<ACampaignPlayerController> Self(this);
		Session->MoveAside(PickedCorps, RegionId, HandArmy, HandArmyIds, !bScripted,
			[Here = ArmyRegion, Hand = PickedCorps, Adjective = HandAdjective()](const FCampaignSession::FMoveAnswer& Answer)
			{
				// (the army goes as far as it can this month, and is in hand where the first march ends)
				const int32 Ends = Answer.Moved.Num() > 0 && Answer.Moved[0].Path.Num() > 0 ? Answer.Moved[0].Path.Last() : Here;
				FCampaignSession::FHandNext Next = HandOf(Answer.Moved, Ends, Ends, true, Adjective);
				if (Answer.Moved.Num() == 0)
				{
					Next.Ids = Hand;
				}
				return Next;
			},
			[Self, bWasSpent, Carried](const FCampaignSession::FMoveAnswer& Answer)
			{
				if (ACampaignPlayerController* Me = Self.Get())
				{
					Me->AfterFarMarch(Answer.Moved, Answer.Note, bWasSpent, Carried);
				}
			});
		return;
	}
	if (!Target)
	{
		if (!bByRail && bHandSpent)
		{
			const FMarchPlan& KeptWay = PlanFor(RegionId);
			Notify(KeptWay.Line.IsEmpty() ? FString(TEXT("No moves left this month.")) : KeptWay.Line, 6.0);
			return;
		}
		if (!bByRail && !bBySea)
		{
			// (Brian, 17 November 2026: "if an enemy army is out of a city i couldnt attack them".) An enemy army he can
			// see and cannot reach: the game says how an attack is made and where to march first, never only "cannot".
			// (The design director, 17 November.) The march up and the attack are planned as one order and shown first.
			if (OpenApproach(RegionId, true))
			{
				return;
			}
			const FString Why = WhyNoAttackOn(RegionId);
			if (!Why.IsEmpty())
			{
				Notify(Why, 10.0);
				return;
			}
		}
		if (bBySea)
		{
			// (The UI director, 18 November 2026; ruling 204.) A refusal by sea says why: the first test the place
			// fails, in the rules' own words, as the browser shows it. ("Leeds has no coast."; "All the shipping is in
			// use this month."; a sea the enemy has closed.)
			const FString Why = Session->WhyNotBySea(PickedCorps, RegionId);
			if (!Why.IsEmpty())
			{
				Notify(Why, 8.0);
				return;
			}
		}
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
		bHandOrdered = true;
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
		OpenFaceOff(RegionId, true); // (Version 63) the card opens on the best attack to be had
		return;
	}
	// (Rules 84.1; rulings 175(c) and 178. Brian, 17 November 2026: "You should be able to take over a territory if no
	// opposing force is there".) Enemy ground with no army in its field: marching in is the order to take it. Where
	// the town's guard will fight, the attack card comes first, with the guard as the defender. Where nothing is
	// risked the army simply goes, and the rules' sentence says what the march came to.
	FString MarchSays, MarchMeets;
	if (Target->bTake && OpenMarchCard(RegionId, MarchSays, &MarchMeets))
	{
		AdviserSaw(1);
		return;
	}

	AdviserSaw(1);
	// (Speed, the design director's mark 1 for build 414: a press's own work under a tenth of a second.) The march is
	// answered by the rules on their own thread, with what the army in hand will ask next; the answer is laid on the
	// table when it comes, a frame or a few later (AfterMarch).
	const TOptional<FVector> Carried = CarriedFrom ? TOptional<FVector>(*CarriedFrom) : TOptional<FVector>();
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Session->MoveAside(PickedCorps, RegionId, HandArmy, HandArmyIds, !bScripted,
		[RegionId, Adjective = HandAdjective()](const FCampaignSession::FMoveAnswer& Answer)
		{
			return Answer.Moved.Num() > 0 ? HandOf(Answer.Moved, RegionId, RegionId, true, Adjective) : FCampaignSession::FHandNext();
		},
		[Self, RegionId, MarchSays, MarchMeets, Carried](const FCampaignSession::FMoveAnswer& Answer)
		{
			if (ACampaignPlayerController* Me = Self.Get())
			{
				Me->AfterMarch(RegionId, Answer.Moved, Answer.Note, MarchSays, MarchMeets, Carried);
			}
		});
}

void ACampaignPlayerController::AfterMarch(int32 RegionId, const TArray<FCorpsMove>& Marches, FString Said, const FString& MarchSays, const FString& MarchMeets, const TOptional<FVector>& Carried)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	if (Said.IsEmpty() && Marches.Num() > 0 && Marches.ContainsByPredicate([RegionId](const FCorpsMove& Each) { return Each.Path.Num() > 0 && Each.Path.Last() == RegionId; }))
	{
		Said = MarchSays; // (said once the army is there)
	}
	if (Marches.Num() == 0)
	{
		// (Version 65: where the place is a full province next door, the rules offer to change places with a corps there)
		Notify(Said.IsEmpty() ? FString(TEXT("They cannot get there this month.")) : Said, Said.IsEmpty() ? 5.0 : 9.0);
		return;
	}
	bHandOrdered = true;
	if (!Said.IsEmpty())
	{
		Notify(Said, 7.0);
	}
	ShowWar(Session->GetView(), Marches, Carried.IsSet() ? &Carried.GetValue() : nullptr, true);
	CampaignSounds::Play(this, TEXT("stamp"), 0.7f);

	// The army in hand is now the corps that marched, where they have arrived.
	PickedCorps.Reset();
	for (const FCorpsMove& March : Marches)
	{
		PickedCorps.Add(March.CorpsId);
	}
	ArmyRegion = RegionId;
	const bool bArrived = Marches.ContainsByPredicate([RegionId](const FCorpsMove& Each) { return Each.Path.Num() > 0 && Each.Path.Last() == RegionId; });
	LaterOrders([this, RegionId, bUnseenTown = MarchMeets == TEXT("unknown") && bArrived]()
	{
		FCampaignSession* Now = GetSession();
		SelectRegion(RegionId);
		if (Now && bUnseenTown)
		{
			// (Rulings 207 and 182 as changed, 17 November.) The army has come up to a town its reports could not see into
			// and stands round it. No card opens by itself in the middle of his orders: where only the town guard holds
			// it, the paper line says so, and the storm's card opens when he presses for it.
			const FAttackPreview Found = Now->PreviewStorm(RegionId);
			if (Found.bValid && Found.bForecast && Found.Meets == TEXT("guard"))
			{
				Notify(FString::Printf(TEXT("Only its town guard holds %s. Right-click the town to plan the storm."), *Found.Town), 10.0);
			}
		}
	});
}

void ACampaignPlayerController::AfterFarMarch(const TArray<FCorpsMove>& Marches, const FString& Said, bool bWasSpent, const TOptional<FVector>& Carried)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	if (Said.IsEmpty() && Marches.Num() == 0)
	{
		Notify(TEXT("They cannot march there."), 5.0);
		return;
	}
	bHandOrdered = true; // (the slip is put away, and the foot says only how to change the order)
	if (bWasSpent && !bMarchSlipDone)
	{
		bMarchSlipDone = true; // (ruling 87: one march kept for a spent army, and the slip has done its work)
		SaveOptions();
	}
	if (!Said.IsEmpty())
	{
		Notify(Said, 10.0);
	}
	CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
	if (Marches.Num() > 0)
	{
		ShowWar(Session->GetView(), Marches, Carried.IsSet() ? &Carried.GetValue() : nullptr, true);
		PickedCorps.Reset();
		for (const FCorpsMove& March : Marches)
		{
			PickedCorps.Add(March.CorpsId);
		}
		if (Marches[0].Path.Num() > 0)
		{
			ArmyRegion = Marches[0].Path.Last();
		}
	}
	LaterOrders([this]()
	{
		SelectRegion(ArmyRegion);
	});
}

bool ACampaignPlayerController::IsTableBusy() const
{
	// (Rules 84.3.3: the Guide's list of the ten lessons lies over the table as a paper does. The key guard, the hover
	// and the right button read it here.)
	return bWatching || Newsreel.bOn || FaceOff.bVisible || WarPrompt.bVisible || Opening.bVisible || Document.bValid || bLedgerOpen || bArsenalOpen || bDiplomacyOpen || bResearchOpen || bGeneralsOpen || bPageOpen || bLessonGuideOpen || Announcements.Num() > 0
		|| (Map.IsValid() && Map->IsPutAway()); // (air battle: the table stands idle under a battle's scene)
}

bool ACampaignPlayerController::IsAnswering() const
{
	if (LiveBattle.IsValid() || LiveRaid.IsValid())
	{
		return true; // (his own battle is open: the map takes no order until it is handed back)
	}
	if (Afterwards.Num() > 0)
	{
		return true;
	}
	const ACampaignGameMode* Mode = GetMode();
	const FCampaignSession* Session = Mode ? Mode->GetSession() : nullptr;
	return Session && Session->IsAnswering();
}

void ACampaignPlayerController::Later(TFunction<void()>&& Step)
{
	if (bScripted || CVarPictureFirst.GetValueOnGameThread() == 0)
	{
		Step();
		return;
	}
	Afterwards.Add(MoveTemp(Step));
}

void ACampaignPlayerController::LaterOrders(TFunction<void()>&& Then)
{
	Later([this]()
	{
		bOrdersInSteps = true;
		RefreshOrders();
		bOrdersInSteps = false;
	});
	Later(MoveTemp(Then));
}

void ACampaignPlayerController::FinishAnswer()
{
	const ACampaignGameMode* Mode = GetMode();
	if (FCampaignSession* Session = Mode ? Mode->GetSession() : nullptr)
	{
		Session->Deliver(true);
	}
	while (Afterwards.Num() > 0)
	{
		const TFunction<void()> Step = MoveTemp(Afterwards[0]);
		Afterwards.RemoveAt(0);
		Step();
	}
}

FString ACampaignPlayerController::HandAdjective() const
{
	const FCampaignSession* Session = GetSession();
	const FString Human = Session ? Session->GetView().Human : FString();
	const FCampaignNation* Nation = Map.IsValid() ? Map->GetData().FindNation(Human) : nullptr;
	return Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : Human;
}

void ACampaignPlayerController::RightClickOn(int32 RegionId, int32 Sea, bool bOnTheTable)
{
	THEATER_SPEED_SCOPE(RightClickOn);
	if (IsAnswering())
	{
		return; // (Speed, b414 mark 1: the last order is still being answered; the playtester's hook comes here too)
	}
	if (AgentInHand != INDEX_NONE)
	{
		// (Ruling 169.) The agent in hand is sent with a right-click, as an army is. An agent may go anywhere:
		// where the rules refuse, their reason is said.
		if (RegionId != INDEX_NONE)
		{
			SendAgentTo(RegionId);
		}
	}
	else if (ArmyRegion != INDEX_NONE && RegionId != INDEX_NONE)
	{
		OrderTo(RegionId);
	}
	else if (ArmyRegion == INDEX_NONE && FleetInHand == INDEX_NONE && LookRegion != INDEX_NONE && RegionId != INDEX_NONE && RegionId != LookRegion
		&& GetSession() && LookNation == GetSession()->GetView().Human && !Tray.Hint.IsEmpty())
	{
		// (Ruling 176.) His own army that takes no orders, and he has tried to send it: the reason is said.
		Notify(Tray.Hint.ToString(), 6.0);
	}
	else if (FleetInHand != INDEX_NONE && bOnTheTable)
	{
		// A fleet in hand is sent with a right-click, as an army is: to the sea under the pointer, or home
		// if it is its own harbour that is pointed at.
		const FCampaignSession* Now = GetSession();
		const FViewFleet* Held = Now ? Now->GetView().Fleets.FindByPredicate([this](const FViewFleet& Each) { return Each.Id == FleetInHand; }) : nullptr;
		if ((Sea != INDEX_NONE))
		{
			OrderFleetTo(Sea);
		}
		else if (Held && RegionId == Held->Base)
		{
			FleetToHarbour();
		}
		else if (Held && FleetHand.bGrand && !FleetHand.bShelled && FleetHand.Coasts.Contains(RegionId))
		{
			// An enemy coast its guns can reach: the question is put first, with what the shelling would do.
			OpenPage(FString::Printf(TEXT("shell:%d:%d"), FleetInHand, RegionId));
		}
		else if (Held && Held->Bases.Contains(RegionId))
		{
			FleetToNewBase(RegionId);
		}
		else
		{
			// (1.5.3, the 1.5.2 review) Held where this month's battle left it, or cut off beyond a shut strait (no sea is ringed
			// for it), it is not told to right-click a ringed sea: its line says why, held first, as the tray's hint and the
			// browser say it, with the way home where a harbour on its side can take it, and the coasts its guns can still shell.
			const TCHAR* ShellsStill = FleetHand.Coasts.Num() > 0 ? TEXT(" A coast in red can still be shelled.") : TEXT("");
			const FString HeldOrCutLand = !FleetHand.Held.IsEmpty() ? FString::Printf(TEXT("%s%s"), *FleetHand.Held, ShellsStill)
				: !FleetHand.Cut.IsEmpty() ? FString::Printf(TEXT("%s%s%s"), *FleetHand.Cut, FleetHand.HomeTip.IsEmpty() ? TEXT("") : TEXT(" RETURN TO HARBOUR takes it to a harbour on its side."), ShellsStill)
				: FString();
			Notify(!HeldOrCutLand.IsEmpty() ? HeldOrCutLand
				: FString(TEXT("A fleet keeps to the sea: right-click one of the ringed seas, a friendly port tinted blue to make it the fleet's new home, or its own harbour to bring it in.")), 6.0);
		}
	}
}

bool ACampaignPlayerController::IsBusyForSteps() const
{
	// (an order still being answered, or its picture still being laid, counts as the table moving: see MoveAside)
	// (a raid's report still to open is the table moving too: the step that flew the raid ends with its report up.
	// Only a run that asked for the report by name, "-TheaterRaidCard", is ever owed one: see PressPage.)
	return bWatching || Newsreel.bOn || Announcements.Num() > 0 || EndAsk.bVisible || IsAnswering() || !RaidCardDue.IsEmpty();
}

void ACampaignPlayerController::StepThroughMonth()
{
	if (EndAsk.bVisible)
	{
		AnswerEndAsk(false);
	}
	else if (Newsreel.bOn)
	{
		EndNewsreel();
	}
	else if (Announcements.Num() > 0)
	{
		DismissAnnouncement();
	}
	else if (bWatching)
	{
		SkipWatching();
	}
}

bool ACampaignPlayerController::RestoreCampaign(const FString& SaveText)
{
	ACampaignGameMode* Mode = GetMode();
	if (!Mode)
	{
		return false;
	}
	FinishAnswer(); // (an order still being answered is laid on the table it was given on, before that table is put away)
	// (whatever is open or in hand belongs to the game being left)
	if (FaceOff.bVisible)
	{
		FaceOff.bResolved = false;
		CloseFaceOff();
	}
	CloseWarPrompt();
	ClosePage();
	CloseArsenal();
	CloseResearch();
	CloseGenerals();
	CloseDiplomacy();
	CloseLedger();
	CloseDocument();
	PutDownArmy();
	PutDownFleet();
	PutDownAgent();
	StopLooking();
	if (!Mode->RestoreCampaign(SaveText))
	{
		return false;
	}
	EnterCampaign();
	return true;
}

bool ACampaignPlayerController::BeginGrandFor(const FString& Tag, int32 FromTurn, int32 Seed, const FString& LevelWord, const FString& DepthWord)
{
	// (1.5.3) The depth asked for, as the rules name it; a depth they do not offer begins nothing, rather than a war at
	// another depth than the step file says.
	const int32 DepthAt = DepthWord.IsEmpty() ? INDEX_NONE
		: Opening.Depths.IndexOfByPredicate([&DepthWord](const FGrandLevel& Each) { return Each.Id.Equals(DepthWord, ESearchCase::IgnoreCase); });
	if (!DepthWord.IsEmpty() && DepthAt == INDEX_NONE)
	{
		return false;
	}
	const FString DepthId = DepthAt != INDEX_NONE ? Opening.Depths[DepthAt].Id : FString();
	int32 Year = INDEX_NONE;
	for (int32 Index = 0; Index < Opening.Years.Num(); ++Index)
	{
		if (Opening.Years[Index].From == FromTurn)
		{
			Year = Index;
		}
	}
	if (Year == INDEX_NONE)
	{
		return false;
	}
	PickGrandYear(Year, true);
	if (Seed > 0)
	{
		GrandSeed = Seed;
		LoadGrandNations();
	}
	PickGrandNation(Tag);
	if (!Opening.Nations.IsValidIndex(Opening.NationIndex) || Opening.Nations[Opening.NationIndex].Tag != Tag)
	{
		return false;
	}
	for (int32 Index = 0; Index < Opening.Levels.Num(); ++Index)
	{
		if (!LevelWord.IsEmpty() && Opening.Levels[Index].Id.Equals(LevelWord, ESearchCase::IgnoreCase))
		{
			PickLevel(Index);
		}
	}
	if (DepthAt != INDEX_NONE)
	{
		PickDepth(DepthAt);
	}
	Opening.AskKind = 0;
	BeginGrandCampaign(false, DepthId);
	return !Opening.bVisible;
}

FString ACampaignPlayerController::StepState() const
{
	return EndAsk.bVisible ? FString(TEXT("the question at the month's end")) : bWatching ? FString(TEXT("the other countries' month"))
		: Newsreel.bOn ? FString(TEXT("a newsreel")) : Announcements.Num() > 0 ? FString(TEXT("an announcement")) : bPageOpen ? Page.Title : FString();
}

bool ACampaignPlayerController::ClickGate(const FString& NameBegins)
{
	const FCampaignSession* Now = GetSession();
	if (!Now)
	{
		return false;
	}
	for (const FViewGate& Each : Now->GetView().Gates)
	{
		if (Each.Name.StartsWith(NameBegins, ESearchCase::IgnoreCase))
		{
			if (ArmyRegion == INDEX_NONE && FleetInHand == INDEX_NONE && AgentInHand == INDEX_NONE)
			{
				OpenGateBox(Each.Id);
			}
			return true;
		}
	}
	return false;
}

void ACampaignPlayerController::DescribeScreen(FString& OutOpen, FString& OutWords, FString& OutHand) const
{
	THEATER_SPEED_SCOPE(HarnessDescribeScreen);
	// The card or page on top, by its title.
	if (FaceOff.bVisible) { OutOpen = FaceOff.Title.ToString() + (FaceOff.bResolved ? TEXT(" (the result)") : TEXT("")); }
	else if (WarPrompt.bVisible) { OutOpen = WarPrompt.Title.ToString(); }
	else if (EndAsk.bVisible) { OutOpen = TEXT("The question at the month's end"); }
	else if (Document.bValid) { OutOpen = Document.Title; }
	else if (bLessonGuideOpen) { OutOpen = LessonGuidePage.Head.IsEmpty() ? FString(TEXT("THE GUIDE")) : TEXT("THE GUIDE: ") + LessonGuidePage.Head; } // (84.3.3: the ten lessons)
	else if (bPageOpen) { OutOpen = Page.Title; }
	else if (bArsenalOpen) { OutOpen = TEXT("THE ARSENAL"); }
	else if (bResearchOpen) { OutOpen = TEXT("RESEARCH"); }
	else if (bGeneralsOpen) { OutOpen = TEXT("THE GENERALS"); }
	else if (bDiplomacyOpen) { OutOpen = TEXT("THE FOREIGN OFFICE"); }
	else if (bLedgerOpen) { OutOpen = TEXT("THE LEDGER"); }
	else if (Newsreel.bOn) { OutOpen = TEXT("A newsreel"); }
	else if (Opening.bVisible) { OutOpen = TEXT("The opening screen"); }
	else if (RegionPanel.bVisible) { OutOpen = RegionPanel.Name.ToString(); }
	else { OutOpen.Reset(); }
	// The words he is shown: the line under the map, the tray's hint, a card's verdict.
	TArray<FString> Said;
	const FString Notice = GetNotice().ToString();
	if (!Notice.IsEmpty()) { Said.Add(Notice); }
	if (FaceOff.bVisible)
	{
		if (FaceOff.bResolved) { Said.Add(FaceOff.ResultTitle.ToString() + TEXT(": ") + FaceOff.ResultDetail.ToString()); }
		else { Said.Add(FaceOff.OddsLine.ToString()); }
	}
	else if (WarPrompt.bVisible) { Said.Add(WarPrompt.Body.ToString()); }
	else if (bLessonGuideOpen)
	{
		if (!LessonGuidePage.State.IsEmpty()) { Said.Add(LessonGuidePage.State); }
		if (LessonGuidePage.Waiting > 0) { Said.Add(FString::Printf(TEXT("%d waiting"), LessonGuidePage.Waiting)); }
	}
	else if (bPageOpen)
	{
		if (!Page.Headline.IsEmpty()) { Said.Add(Page.Headline); }
		// (1.5.1, the Mac's p7, 25 November 2026: the step player's reading only. A fleet's note runs past 400 letters
		// with its admirals' line last, and a section's own line, "it stays where the battle left it", was not read.)
		if (!Page.Note.IsEmpty()) { Said.Add(Page.Note.Left(1200)); }
		for (const FPageSection& Section : Page.Sections)
		{
			if (!Section.Text.IsEmpty()) { Said.Add(Section.Text.Left(400)); }
		}
	}
	else if (Tray.bVisible)
	{
		Said.Add(Tray.Title.ToString() + TEXT(": ") + Tray.Subtitle.ToString());
		if (!Tray.Hint.IsEmpty()) { Said.Add(Tray.Hint.ToString()); }
	}
	OutWords = FString::Join(Said, TEXT(" | "));
	// What is in hand.
	const ACampaignMapActor* Table = Map.Get();
	auto Place = [Table](int32 RegionId) { return Table && Table->GetData().Regions.IsValidIndex(RegionId) ? Table->GetData().Regions[RegionId].Name : FString(TEXT("?")); };
	if (ArmyRegion != INDEX_NONE)
	{
		TArray<FString> Ids;
		for (const int32 Id : PickedCorps) { Ids.Add(FString::FromInt(Id)); }
		// (1.5.1, the Mac's p7: a division's tile picked or let go on a card is a change in hand the step player can see.)
		// (1.5.3: only the picks the tray shows lit are counted, as MakeCard in CampaignHUD.cpp lights a tile: on a card of
		// the tray in view that can be broken up, among the tiles it lays out, twelve at most. A pick kept from a card that
		// is no longer shown, such as another army's before this one was picked up, is not in hand.)
		int32 Splitting = 0;
		if (Tray.bVisible)
		{
			for (const FTrayCard& Card : Tray.Cards)
			{
				const TSet<int32>* Set = !Card.bLook && Card.bCanBreak ? PickedDivisions.Find(Card.CorpsId) : nullptr;
				const int32 Tiles = Card.Parts.Num() > 0 ? FMath::Min(Card.Parts.Num(), 12) : FMath::Clamp(Card.Divisions, 1, 12);
				for (int32 Slot = 0; Set && Slot < Tiles; ++Slot)
				{
					Splitting += Set->Contains(Slot) ? 1 : 0;
				}
			}
		}
		const FString Split = Splitting > 0 ? FString::Printf(TEXT(", %d division%s picked to split off"), Splitting, Splitting == 1 ? TEXT("") : TEXT("s")) : FString();
		OutHand = FString::Printf(TEXT("army at %s: corps %s%s%s"), *Place(ArmyRegion), *FString::Join(Ids, TEXT(",")), bByRail ? TEXT(" (by rail)") : bBySea ? TEXT(" (by sea)") : TEXT(""), *Split);
	}
	else if (FleetInHand != INDEX_NONE) { OutHand = FString::Printf(TEXT("fleet %d"), FleetInHand); }
	else if (AgentInHand != INDEX_NONE) { OutHand = FString::Printf(TEXT("agent %d"), AgentInHand); }
	else if (LookRegion != INDEX_NONE) { OutHand = FString::Printf(TEXT("looking at the %s army at %s"), *LookNation, *Place(LookRegion)); }
	else { OutHand.Reset(); }
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
	const FString Name = Nation ? WithThe(Nation->Name) : Tag; // (84.2.6: "Declare war on the United States?")
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
			TEXT("Your armies on its border can attack as soon as war is declared.\n\nThis cannot be taken back."), *HeadThe(Name)));
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
	if (Asked.bApproach)
	{
		CarryOutApproach(Asked);
		return;
	}

	if (Asked.bGate)
	{
		// The holder's order: the gate is shut to all ships but his own and his allies', or opened again.
		FString Said;
		const bool bGiven = Session->OrderGate(Asked.Gate, Asked.bGateShut, Said);
		ShowWar(Session->GetView(), TArray<FCorpsMove>());
		HoverGate = INDEX_NONE; // (its slip is read again)
		CampaignSounds::Play(this, bGiven ? TEXT("stamp") : TEXT("click"), 0.7f);
		if (!Said.IsEmpty())
		{
			Notify(Said, 8.0);
		}
		return;
	}
	if (Asked.bLanding)
	{
		// The landing goes in. (The card is down and the guns are heard at once; the rules answer on their own thread,
		// with what the army ashore will ask next: speed, b414 mark 1. See AfterLanding.)
		CampaignSounds::Play(this, TEXT("ship_guns"), 0.6f);
		CampaignSounds::PlayLater(this, TEXT("battle"), 0.8f, 0.7f);
		const int32 Beach = Asked.Region;
		const TWeakObjectPtr<ACampaignPlayerController> Self(this);
		Session->LandAside(PickedCorps, Beach, !bScripted,
			[Beach, Here = ArmyRegion, Hand = PickedCorps, bPlain = !bByRail, Adjective = HandAdjective()](const FLandingResult& Result)
			{
				const bool bAshore = Result.Moved.Num() > 0;
				FCampaignSession::FHandNext Next = HandOf(Result.Moved, bAshore ? Beach : Here, INDEX_NONE, bPlain, Adjective);
				if (!bAshore)
				{
					Next.Ids = Hand;
				}
				Next.Selected = Result.bTaken ? Beach : Next.Region;
				return Next;
			},
			[Self, Beach](const FLandingResult& Result)
			{
				if (ACampaignPlayerController* Me = Self.Get())
				{
					Me->AfterLanding(Beach, Result);
				}
			});
		return;
	}
	const FCampaignNation* Nation = Map.IsValid() ? Map->GetData().FindNation(Asked.Tag) : nullptr;
	const FString Name = !Asked.Name.IsEmpty() ? Asked.Name : Nation ? WithThe(Nation->Name) : Asked.Tag;
	FString Answered;
	if (!Session->Declare(Asked.Tag, &Answered))
	{
		Notify(FString::Printf(TEXT("War could not be declared on %s this month."), *Name));
		return;
	}
	// The table as it stands now: their ground is enemy ground, and the army in hand may be able to reach it.
	ShowWar(Session->GetView(), TArray<FCorpsMove>());
	RefreshOrders();
	// (Rules 84.2.5; the UI director, 17 November: one voice after a press.) A war that brings another at once says so on
	// the same paper, in the rules' words: "Britain stands by Canada. Britain declares war on the United States: ..."
	Notify(Answered.IsEmpty() ? FString::Printf(TEXT("War is declared on %s."), *Name) : FString::Printf(TEXT("War is declared on %s. %s"), *Name, *Answered), Answered.IsEmpty() ? 6.0 : 11.0);
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

void ACampaignPlayerController::AfterLanding(int32 RegionId, const FLandingResult& Result)
{
	FCampaignSession* Session = GetSession();
	if (!Session)
	{
		return;
	}
	if (Map.IsValid() && Result.bOk)
	{
		Map->AddBattleSmoke(RegionId);
	}
	ShowWar(Session->GetView(), Result.Moved, nullptr, true);
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
		ArmyRegion = RegionId;
	}
	bBySea = false;
	LaterOrders([this, RegionId, bTaken = Result.bTaken, Note = Result.Note]()
	{
		SelectRegion(bTaken ? RegionId : ArmyRegion);
		Notify(Note, 9.0);
	});
}

void ACampaignPlayerController::CloseWarPrompt()
{
	const bool bBack = WarPrompt.bFromOffice;
	if (WarPrompt.bApproach && Map.IsValid())
	{
		// (the planned road goes with the plan)
		Map->SetRoute(FVector::ZeroVector, TArray<int32>());
		RouteTarget = -3;
	}
	WarPrompt = FWarPrompt();
	if (bBack)
	{
		OpenDiplomacy();
	}
}

void ACampaignPlayerController::ShowWar(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom, bool bPictureFirst)
{
	StormFavourKey.Reset(); // (something was ordered: the staff's forecast of a storm is asked afresh for the army in hand)
	THEATER_SPEED_SCOPE(ShowWar);
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
		// (Ink that spreads is for ground that changes hands as the months go by. A campaign begun, or a saved one
		// taken up, lays the table out as it stands: with ink, every province that had changed hands since 1939
		// seeped in at once when a campaign of 1941 was opened.)
		Map->SetInkAllowed(TurnLaid >= 0 && HumanLaid == View.Human && (View.Turn == TurnLaid || View.Turn == TurnLaid + 1));
		TurnLaid = View.Turn;
		{
			// (A guard, 18 November 2026. Rules Version 85 read 339 provinces more than the table drew, and an army
			// that marched into one of them was nowhere to be seen; nothing said so. The rules' picture holds one
			// holder for every province they know: if that is more than the table has, it is said once, as an error,
			// so that the picture check stops on it. (Fewer is as it should be: a history scenario's rules know
			// Europe's 375 alone, and the world beyond them is the table's backdrop.)
			static int32 SaidFor = -1;
			const int32 Known = View.RegionHolder.Num(), Drawn = Map->GetData().Regions.Num();
			if (Known > Drawn && SaidFor != Known)
			{
				SaidFor = Known;
				UE_LOG(LogTheater, Error, TEXT("The rules know %d provinces and the table draws %d: an army in a province the table does not have cannot be seen. The map files the table reads are not the ones the rules were built from."), Known, Drawn);
			}
		}
		HumanLaid = View.Human;
		Map->SetHolders(View.RegionHolder, View.League, View.RegionOwner, View.SiegePaint);
		Map->SetSeason(View.Turn);
		Map->ShowArmies(View, Marches, CarriedFrom);
		Map->ShowAgents(View); // (ruling 153: after the armies, whose bases an agent stands clear of)
		if (AgentInHand != INDEX_NONE && !GetAgentInHand())
		{
			PutDownAgent(); // (caught, or retired, while in hand)
		}
	}
	// (Speed, b414 mark 1; the speed detective's "change the picture first".) After an order, the ground's new holders
	// and the armies where they now stand are the press's own picture; the works, the scars, the front line, the haze
	// and the rest follow a frame later.
	if (bPictureFirst)
	{
		Later([this]()
		{
			if (const FCampaignSession* Now = GetSession())
			{
				ShowWarRest(Now->GetView());
			}
		});
	}
	else
	{
		ShowWarRest(View);
	}
}

void ACampaignPlayerController::ShowWarRest(const FCampaignView& View)
{
	THEATER_SPEED_SCOPE(ShowWarRest);
	if (Map.IsValid())
	{
		Map->ShowWorks(View.BridgesDown, View.RailsCut);
		Map->ShowCuts(View.Cuts);
		Map->ShowCivic(View.Civic);
		Map->ShowScars(View.Scars);
		Map->ShowSieges(View.Sieges);
		Map->ShowBombed(View.Bombed);
		Map->ShowWorksLeft(View.WorksLeft);
		Map->ShowWrecked(View.Wrecked);
		Map->SetWarAtNight(View.Blackout, View.Fronts);
		Map->ShowFrontLine(View.Wars, View.Human);
		Map->ShowWrecks(View.Wrecks, View.Turn);
		Map->ShowIdle(View.Cold);
		Map->ShowPrizes(View.Prizes);
		Map->ShowWorksGoing(View.Going);
		Map->ShowBuildings(View.RegionWorks);
		Map->ShowAirfieldSigns(View.Airfields);
		Map->ShowLanes(View.Lanes);
		Map->ShowFreeCapitals(View.FreeCapitals);
		Map->ShowShroud(View.Unseen, View.Glimpsed);
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

	/**
	 * The corps of one side of a battle: each one's name, and its figures, which never fold. After the battle the
	 * figures say what it cost and what became of them. (The UI director, check 19: the card sets the two on one line
	 * where they fit, and the name over its figures where they do not; the dot stands only between facts on one line.)
	 */
	TArray<FRosterLine> RosterOf(const TArray<FBattleUnit>& Units, bool bAfter)
	{
		TArray<FRosterLine> Roster;
		for (const FBattleUnit& Unit : Units)
		{
			FRosterLine& Row = Roster.AddDefaulted_GetRef();
			Row.Name = Unit.Name;
			// A corps not identified has no arm and no numbers to give.
			TArray<FString> Facts;
			if (!Unit.Kind.IsEmpty())
			{
				Facts.Add(FString::Printf(TEXT("%s, %d division%s"), *FCampaignMapData::CorpsKindLabel(Unit.Kind), Unit.Divisions, Unit.Divisions == 1 ? TEXT("") : TEXT("s")));
			}
			if (bAfter)
			{
				Facts.Add(Unit.Fate.IsEmpty() ? FString::Printf(TEXT("lost %s"), *Thousands(Unit.Lost)) : FString::Printf(TEXT("lost %s, %s"), *Thousands(Unit.Lost), *Unit.Fate));
			}
			else if (Unit.Men > 0)
			{
				Facts.Add(FString::Printf(TEXT("%s men"), *Thousands(Unit.Men)));
			}
			Row.Figures = FString::Join(Facts, TEXT(" \u00B7 "));
		}
		return Roster;
	}

	/** The same corps as plain text, a line each, for anything that reads the card as words. */
	FString ListUnits(const TArray<FRosterLine>& Roster)
	{
		TArray<FString> Lines;
		for (const FRosterLine& Row : Roster)
		{
			Lines.Add(Row.Figures.IsEmpty() ? Row.Name : Row.Name + TEXT(" \u00B7 ") + Row.Figures);
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

	/**
	 * (The UI director, 17 November, on Version 84.3.) The chance of taking a place, on the card's odds line: at the
	 * ends of the scale in words, never "about 100%" or "about 0%". The bridge says the row of everyone in reach the
	 * same way (chanceTo), and both follow the browser's endSaid (Rules 84.3.1): bNamed where the line has already
	 * named the staff ("The staff's estimate: ...").
	 */
	FString ChanceTo(const FString& Words, const FString& Place, bool bNamed = false)
	{
		if (Words == TEXT("about 100%"))
		{
			return bNamed ? TEXT("they are sure of it") : TEXT("your staff is sure of it");
		}
		if (Words == TEXT("about 0%"))
		{
			return bNamed ? TEXT("they give it no chance") : TEXT("your staff gives it no chance");
		}
		return FString::Printf(TEXT("%s to take %s"), *Words, *Place);
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
			return CampaignText::Phrases(FString::Printf(TEXT("%d corps  \u00B7  strength not known"), Units.Num()));
		}
		return CampaignText::Phrases(FString::Printf(TEXT("%d corps  \u00B7  %d division%s  \u00B7  %s men"), Units.Num(), Divisions, Divisions == 1 ? TEXT("") : TEXT("s"), *Thousands(Men)));
	}
}

namespace
{
	/**
	 * The Battle Ticket as text: every line of it, and the lines that matter most, which a long ticket opens folded to.
	 * (The UI director's review of build 1628: no dashes in the game's words; a line's parts join with the game's own dot.)
	 */
	void WriteTicket(const TArray<FTicketLine>& Ticket, FFaceOff& Out)
	{
		static const TCHAR* const Key[] = { TEXT("Forces"), TEXT("Troops"), TEXT("Dug in"), TEXT("Supply"), TEXT("Earlier attacks"), TEXT("Weight of attack") };
		TArray<FString> Lines, Main;
		for (const FTicketLine& Line : Ticket)
		{
			const FString Text = Line.Effect.IsEmpty()
				? FString::Printf(TEXT("%s:  %s"), *Line.Label, *Line.Value)
				: FString::Printf(TEXT("%s:  %s  \u00B7  %s"), *Line.Label, *Line.Value, *Line.Effect);
			Lines.Add(Text);
			for (const TCHAR* Label : Key)
			{
				if (Line.Label == Label)
				{
					Main.Add(Text);
					break;
				}
			}
		}
		Out.Ticket = FText::FromString(FString::Join(Lines, TEXT("\n")));
		Out.TicketKey = FText::FromString(FString::Join(Main, TEXT("\n")));
		Out.TicketLines = Lines.Num();
		// (a ticket of a few lines is short enough as it is)
		Out.TicketHidden = Lines.Num() > 8 && Main.Num() > 0 ? Lines.Num() - Main.Num() : 0;
	}
}

void ACampaignPlayerController::OpenFaceOff(int32 RegionId, bool bOpening, const FAttackPreview* Given)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	// (Given: the forecast is already in hand: the assault a march on a guarded town ends in.)
	const FAttackPreview Preview = Given ? *Given : Session->PreviewAttack(PickedCorps, RegionId, bOpening);
	if (Preview.bValid && Preview.Attackers.Num() > 0)
	{
		// the two sides are drawn up (and where the pick cannot go, the card only turns over)
		CampaignSounds::Play(this, Preview.Alone.IsEmpty() ? TEXT("drums") : TEXT("click"), 0.6f);
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

	if (bOpening && Preview.PickedIds.Num() > 0 && Preview.Ids.Num() > 0)
	{
		// (Version 63) Everyone the roads will carry makes a better attack of it than the corps in hand: the staff has
		// brought them in. The corps that were in hand are remembered, for the button that goes back to them and for
		// the attack being called off.
		if (PickedBeforeMass.Num() == 0)
		{
			PickedBeforeMass = PickedCorps;
		}
		PickedCorps = Preview.Ids;
	}
	FaceOff = FFaceOff();
	FaceOff.bVisible = true;
	FaceOffSince = FPlatformTime::Seconds();
	FaceOff.Region = RegionId;
	FaceOff.bWidened = PickedBeforeMass.Num() > 0;
	FaceOff.Opened = FText::FromString(!Preview.Opened.IsEmpty() ? Preview.Opened
		: FaceOff.bWidened ? FString(TEXT("Every corps the roads will carry is going in.")) : FString());
	FaceOff.Cost = FText::FromString(FString::Join(Preview.Cost, TEXT("\n")));
	FaceOff.bSoften = Preview.bSoften;
	FaceOff.SoftenHead = FText::FromString(Preview.SoftenHead);
	FaceOff.SoftenDone = FText::FromString(Preview.SoftenDone);
	FaceOff.ProbeLabel = FText::FromString(Preview.ProbeLabel);
	FaceOff.ProbeText = FText::FromString(Preview.ProbeText);
	FaceOff.bProbeOk = Preview.bProbeOk;
	FaceOff.ProbeCorps = Preview.ProbeCorps;
	FaceOff.bBomb = Preview.bBomb;
	FaceOff.bBombOk = Preview.bBombOk;
	FaceOff.BombText = FText::FromString(Preview.BombText);
	FaceOff.BombOpen = Preview.BombOpen;
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
	// (Ruled by the design director at 16:52 UTC on 17 November.) Where no general is named, the large line carries the
	// force's own name, as the map's flag and the province panel give it ("American army", "Mexican army"). A named
	// general takes the line back.
	auto ForceName = [](const FCampaignNation* Nation, const FString& Tag)
	{
		return FString::Printf(TEXT("%s army"), Nation && !Nation->Adjective.IsEmpty() ? *Nation->Adjective : *Tag);
	};
	FaceOff.OurGeneral = FText::FromString(OurGeneral.IsEmpty() ? ForceName(Ours, View.Human) : OurGeneral);
	FaceOff.TheirGeneral = FText::FromString(TheirGeneral.IsEmpty() ? ForceName(Theirs, TheirTag) : TheirGeneral);
	FaceOff.OurStrength = FText::FromString(StrengthLine(Preview.Attackers));
	FaceOff.TheirStrength = FText::FromString(StrengthLine(Preview.Defenders));
	FaceOff.Alone = FText::FromString(Preview.Alone);
	if (!Preview.Alone.IsEmpty())
	{
		// (The UI director, 17 November.) The corps picked, as they were picked, and the men among them who could take
		// the ground: none.
		int32 Divisions = 0;
		for (const FBattleUnit& Unit : Preview.Attackers)
		{
			Divisions += Unit.Divisions;
		}
		FaceOff.OurStrength = FText::FromString(CampaignText::Phrases(FString::Printf(TEXT("%d corps  \u00B7  %d division%s  \u00B7  0 men taking ground"),
			Preview.Attackers.Num(), Divisions, Divisions == 1 ? TEXT("") : TEXT("s"))));
	}
	FaceOff.OurRoster = RosterOf(Preview.Attackers, false);
	FaceOff.TheirRoster = RosterOf(Preview.Defenders, false);
	FaceOff.OurUnits = FText::FromString(ListUnits(FaceOff.OurRoster));
	FaceOff.TheirUnits = FText::FromString(ListUnits(FaceOff.TheirRoster));
	FaceOff.Balance = Preview.Attack + Preview.Defence > 0.0f ? Preview.Attack / (Preview.Attack + Preview.Defence) : 0.5f;
	FaceOff.Prize = FText::FromString(Preview.Prize);
	if (!Preview.Alone.IsEmpty())
	{
		// (the pick cannot go: there is no forecast, and the foot says why)
	}
	else if (Preview.bExact)
	{
		FString Odds = Preview.Odds >= 1.0f
			? FString::Printf(TEXT("Odds of %.1f to 1 in your favour"), Preview.Odds)
			: FString::Printf(TEXT("Odds of %.1f to 1 against you"), 1.0f / FMath::Max(Preview.Odds, 0.01f));
		// (the chance is "sure" only when the attack cannot fail, and said in words at the ends of the scale)
		if (!Preview.ChanceWords.IsEmpty())
		{
			Odds += TEXT("  \u00B7  ") + ChanceTo(Preview.ChanceWords, Place);
		}
		else if (Preview.ChancePct >= 0)
		{
			Odds += TEXT("  \u00B7  ") + ChanceTo(FString::Printf(TEXT("about %d%%"), Preview.ChancePct), Place);
		}
		FaceOff.OddsLine = FText::FromString(CampaignText::Phrases(Odds));
	}
	else
	{
		// The defenders are not all identified: the staff give the odds as a band that holds the truth.
		FString Odds = TEXT("The staff's estimate: ") + CampaignText::OddsBetween(Preview.OddsLow, Preview.OddsHigh);
		if (!Preview.ChanceWords.IsEmpty())
		{
			// (Version 63: a band too wide to plan on is given as the staff's best guess with its limits)
			Odds += TEXT("  \u00B7  ") + ChanceTo(Preview.ChanceWords, Place, /*bNamed*/ true);
		}
		else if (Preview.ChancePctLow >= 0 && Preview.ChancePctHigh >= 0)
		{
			Odds += TEXT("  \u00B7  ") + (Preview.ChancePctLow == Preview.ChancePctHigh
				? ChanceTo(FString::Printf(TEXT("about %d%%"), Preview.ChancePctLow), Place, /*bNamed*/ true)
				: FString::Printf(TEXT("%d to %d%% to take %s"), Preview.ChancePctLow, Preview.ChancePctHigh, *Place));
		}
		FaceOff.OddsLine = FText::FromString(CampaignText::Phrases(Odds));
		// (Version 84: where WHAT YOU FACE says what is not scouted, it says it once. The staff's longer note stands
		// only on a card with no such block, as in a history campaign.)
		if (!Preview.Note.IsEmpty() && Preview.Face.Num() == 0)
		{
			FaceOff.Prize = FText::FromString(Preview.Note + (Preview.Prize.IsEmpty() ? FString() : TEXT("\n") + Preview.Prize));
		}
	}

	// The Battle Ticket: what the campaign map says about this fight, and what each thing does in it.
	WriteTicket(Preview.Lines, FaceOff);
	// Where more corps stand in reach than were picked: what the attack could be, and a button to make it so.
	FaceOff.Mass = FText::FromString(Preview.Mass);
	FaceOff.Face = Preview.Face; // (Version 84; ruling 174)
	FaceOff.bLongShot = Preview.VerdictTone == TEXT("loss");
	FaceOff.MassIds = Preview.MassIds;
}

void ACampaignPlayerController::OpenGateBox(int32 GateId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || GateId == INDEX_NONE)
	{
		return;
	}
	const FGateAsk Ask = Session->AskGate(GateId);
	if (!Ask.bOk)
	{
		if (!Ask.Why.IsEmpty())
		{
			Notify(Ask.Why, 6.0); // (only its holder can shut it; or it is not his month)
		}
		return;
	}
	WarPrompt = FWarPrompt();
	WarPrompt.bVisible = true;
	WarPrompt.bGate = true;
	WarPrompt.Gate = GateId;
	WarPrompt.bGateShut = Ask.Order == TEXT("shut");
	WarPrompt.Tag = Ask.Holder;
	WarPrompt.Title = FText::FromString(Ask.Question);
	// The price stands above the button: whose trade stops or goes the long way round, his own first, a row each.
	FString Body = Ask.Lead + TEXT("\n\n") + Ask.Heading.ToUpper();
	if (Ask.Rows.Num() == 0)
	{
		Body += TEXT("\n") + Ask.None;
	}
	for (int32 Index = 0; Index < Ask.Rows.Num(); ++Index)
	{
		// (past six rows, the first five and a count)
		if (Ask.Rows.Num() > 6 && Index >= 5)
		{
			Body += FString::Printf(TEXT("\n\u00B7 and %d more"), Ask.Rows.Num() - 5);
			break;
		}
		Body += TEXT("\n\u00B7 ") + Ask.Rows[Index];
	}
	if (!Ask.Total.IsEmpty())
	{
		Body += TEXT("\n") + Ask.Total;
	}
	if (!Ask.ResentHeading.IsEmpty() && Ask.Resent.Num() > 0)
	{
		// (Ruling 177(a).) The second tally: the countries at peace whose trade it turns back, and what each will think of it.
		Body += TEXT("\n\n") + Ask.ResentHeading.ToUpper();
		for (int32 Index = 0; Index < Ask.Resent.Num(); ++Index)
		{
			if (Ask.Resent.Num() > 6 && Index >= 5)
			{
				Body += FString::Printf(TEXT("\n\u00B7 and %d more"), Ask.Resent.Num() - 5);
				break;
			}
			Body += TEXT("\n\u00B7 ") + Ask.Resent[Index];
		}
		if (!Ask.ResentNote.IsEmpty())
		{
			Body += TEXT("\n") + Ask.ResentNote;
		}
	}
	WarPrompt.Body = FText::FromString(Body);
	WarPrompt.Confirm = FText::FromString(Ask.Yes.ToUpper());
	WarPrompt.Cancel = FText::FromString(Ask.No.ToUpper());
	CampaignSounds::Play(this, TEXT("click"), 0.6f);
}

bool ACampaignPlayerController::OpenMarchCard(int32 RegionId, FString& OutSays, FString* OutMeets)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return false;
	}
	const FAttackPreview Preview = Session->PreviewTake(PickedCorps, RegionId);
	if (!Preview.bValid)
	{
		return false; // a plain march
	}
	OutSays = Preview.MarchLine;
	if (OutMeets)
	{
		*OutMeets = Preview.Meets;
	}
	if (Preview.Meets != TEXT("guard") || !Preview.bForecast || Preview.Attackers.Num() == 0)
	{
		return false; // (ruling 178(a): a march that risks nothing gets no card and no yes)
	}
	OpenFaceOff(RegionId, false, &Preview);
	if (!FaceOff.bVisible)
	{
		return false;
	}
	DressSiegeCard(Preview, RegionId);
	FaceOff.bMarch = true;
	return true;
}

bool ACampaignPlayerController::OpenSortieCard(int32 RegionId)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || RegionId == INDEX_NONE)
	{
		return false;
	}
	const FAttackPreview Preview = Session->PreviewSally(RegionId);
	if (!Preview.Refused.IsEmpty())
	{
		Notify(Preview.Refused, 8.0); // (one sortie a town a month, nobody fit to go: the rules' own sentence)
		return false;
	}
	if (!Preview.bValid || Preview.Attackers.Num() == 0)
	{
		Notify(TEXT("No garrison of yours is shut in here by an army round its town."), 6.0);
		return false;
	}
	PickedBeforeMass.Reset();
	OpenFaceOff(RegionId, false, &Preview);
	if (!FaceOff.bVisible)
	{
		return false;
	}
	const FString Town = Preview.Town.IsEmpty() ? (Map->GetData().Regions.IsValidIndex(RegionId) ? Map->GetData().Regions[RegionId].Name : FString()) : Preview.Town;
	FaceOff.bSally = true;
	FaceOff.bSoften = false; // (a sortie is not softened first: no probe, no bombers' row)
	FaceOff.bBomb = false;
	FaceOff.Title = FText::FromString(FString::Printf(TEXT("The sortie from %s"), *Town));
	// (the forecast's line speaks of the siege, not of taking the town it already holds)
	{
		// (the line is set in phrases that do not break: the words are looked for as they were set)
		const FString Place = Map->GetData().Regions.IsValidIndex(RegionId) ? Map->GetData().Regions[RegionId].Name : Town;
		const FString Taken = FString::Printf(TEXT("to take %s"), *Place), Raised = TEXT("to raise the siege");
		FaceOff.OddsLine = FText::FromString(FaceOff.OddsLine.ToString().Replace(*CampaignText::Phrases(Taken), *CampaignText::Phrases(Raised)).Replace(*Taken, *Raised));
	}
	return true;
}

bool ACampaignPlayerController::OpenStormCard(int32 RegionId, bool bSayWhy, bool bOnArrival)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return false;
	}
	const FAttackPreview Preview = Session->PreviewStorm(RegionId);
	if (!Preview.Refused.IsEmpty())
	{
		if (!bOnArrival)
		{
			Notify(Preview.Refused, 7.0); // (the rules will not have a storm this month, and say why)
		}
		return false;
	}
	if (!Preview.bValid || !Preview.bForecast || Preview.Attackers.Num() == 0 || (bOnArrival && Preview.Meets != TEXT("guard")))
	{
		if (bSayWhy)
		{
			Notify(TEXT("No army of yours stands round a town here that it could storm this month."), 6.0);
		}
		return false;
	}
	OpenFaceOff(RegionId, false, &Preview);
	if (!FaceOff.bVisible)
	{
		return false;
	}
	DressSiegeCard(Preview, RegionId);
	FaceOff.bStorm = true;
	FaceOff.bArrival = bOnArrival;
	return true;
}

void ACampaignPlayerController::DressSiegeCard(const FAttackPreview& Preview, int32 RegionId)
{
	const FString Town = Preview.Town.IsEmpty() ? (Map.IsValid() && Map->GetData().Regions.IsValidIndex(RegionId) ? Map->GetData().Regions[RegionId].Name : FString()) : Preview.Town;
	auto Odds = [&Town](float At, float Low, float High, bool bExact, const FString& ChanceWords)
	{
		FString Line = bExact ? (At >= 1.0f ? FString::Printf(TEXT("Odds of %.1f to 1 in your favour"), At) : FString::Printf(TEXT("Odds of %.1f to 1 against you"), 1.0f / FMath::Max(At, 0.01f)))
			: TEXT("The staff's estimate: ") + CampaignText::OddsBetween(Low, High);
		if (!ChanceWords.IsEmpty())
		{
			Line += TEXT("  \u00B7  ") + ChanceTo(ChanceWords, Town, /*bNamed*/ !bExact); // (the estimate's line names the staff)
		}
		return FText::FromString(CampaignText::Phrases(Line));
	};
	// (Rulings 177(b), 178(c) and 180: the order its own forecast favours is the lit one; the rules say which.)
	FaceOff.bFavourSiege = Preview.Favours != TEXT("storm");
	FaceOff.Title = FText::FromString(FString::Printf(TEXT("The assault on %s"), *Town));
	// The guard (or the garrison) stands where the enemy general's name stands, under the game's own name for it;
	// its strength under that as an army's is; and who they are.
	FaceOff.TheirGeneral = FText::FromString(Preview.GuardName.IsEmpty() ? FString::Printf(TEXT("Town guard of %s"), *Town) : Preview.GuardName);
	FaceOff.TheirStrength = FText::FromString(Preview.GuardLine);
	FaceOff.TheirUnits = FText::FromString(Preview.GuardWho);
	FaceOff.TheirRoster.Reset(); // (the guard is said in a sentence, not listed corps by corps)
	// Block 1 is SEND THEM IN's forecast (the card's own bar and odds), with its heading and its sentence.
	FaceOff.NowLabel = FText::FromString(Preview.NowLabel.IsEmpty() ? FString(TEXT("SEND THEM IN")) : Preview.NowLabel);
	FaceOff.NowSays = FText::FromString(Preview.NowSays);
	// Block 2 is headed by the no-risk button's words: a second forecast (the same assault a month later, made
	// ready), or the rules' one sentence on what waiting brings, or nothing.
	FaceOff.LaterLabel = FText::FromString(Preview.LaterLabel);
	FaceOff.LaterOdds = Preview.bLater ? Odds(Preview.LaterOdds, Preview.LaterOddsLow, Preview.LaterOddsHigh, Preview.bLaterExact, Preview.LaterChanceWords) : FText::GetEmpty();
	FaceOff.LaterSays = FText::FromString(Preview.bLater ? Preview.LaterSays : Preview.WaitSays);
	FaceOff.LaterBalance = Preview.bLater ? Preview.LaterOdds / (Preview.LaterOdds + 1.0f) : 0.5f;
	FaceOff.Prize = FText::FromString(Preview.Prize);
}

void ACampaignPlayerController::ConfirmMarch(bool bAssault)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !FaceOff.bVisible || FaceOff.bResolved || IsAnswering() || !(FaceOff.bMarch || FaceOff.bStorm))
	{
		return;
	}
	if (bAssault)
	{
		ResolveAttack(); // the army marches in and goes at the town as it arrives, or the army round it storms
		return;
	}
	if (FaceOff.bStorm)
	{
		CloseFaceOff(); // KEEP UP THE SIEGE: nothing changes (ruling 182)
		return;
	}
	// LAY SIEGE FIRST: the army marches in and closes round the town. Nothing is risked this month. (The card is put
	// away at once; the march is answered on the rules' own thread: speed, b414 mark 1. See AfterSiege.)
	const int32 RegionId = FaceOff.Region;
	PickedBeforeMass.Reset();
	FaceOff.bVisible = false;
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Session->FightAside(TEXT("take"), PickedCorps, RegionId, false, !bScripted,
		[RegionId, bPlain = !bByRail && !bBySea, Adjective = HandAdjective()](const FAttackResult& Result)
		{
			return Result.Moved.Num() > 0 ? HandOf(Result.Moved, RegionId, RegionId, bPlain, Adjective) : FCampaignSession::FHandNext();
		},
		[Self, RegionId](const FAttackResult& Result)
		{
			if (ACampaignPlayerController* Me = Self.Get())
			{
				Me->AfterSiege(RegionId, Result);
			}
		});
}

void ACampaignPlayerController::AfterSiege(int32 RegionId, const FAttackResult& Result)
{
	FCampaignSession* Session = GetSession();
	if (!Session)
	{
		return;
	}
	if (Result.Moved.Num() == 0)
	{
		Notify(Result.Note.IsEmpty() ? FString(TEXT("They cannot get there this month.")) : Result.Note, 6.0);
		return;
	}
	ShowWar(Session->GetView(), Result.Moved, nullptr, true);
	CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
	PickedCorps.Reset();
	for (const FCorpsMove& March : Result.Moved)
	{
		PickedCorps.Add(March.CorpsId);
	}
	ArmyRegion = RegionId;
	if (!Result.Note.IsEmpty())
	{
		Notify(Result.Note, 9.0);
	}
	LaterOrders([this, RegionId]()
	{
		SelectRegion(RegionId);
	});
}

void ACampaignPlayerController::SendEveryone()
{
	if (!FaceOff.bVisible || FaceOff.bResolved || IsAnswering() || FaceOff.MassIds.Num() == 0)
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

void ACampaignPlayerController::ProbeLine()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !FaceOff.bVisible || FaceOff.bResolved || IsAnswering() || !FaceOff.bProbeOk || FaceOff.ProbeCorps == INDEX_NONE)
	{
		return;
	}
	const int32 RegionId = FaceOff.Region;
	FString Note;
	// (the corps in hand go with the order, so that the rules can say what the card read before the probe and after)
	FString Force;
	for (const int32 Id : PickedCorps)
	{
		Force += (Force.IsEmpty() ? TEXT("") : TEXT(".")) + FString::FromInt(Id);
	}
	const bool bProbed = Session->Act(FString::Printf(TEXT("probe:%d:%d:%s"), FaceOff.ProbeCorps, RegionId, *Force), RegionId, Note);
	CampaignSounds::Play(this, bProbed ? TEXT("field_gun") : TEXT("click"), 0.5f);
	if (!Note.IsEmpty())
	{
		Notify(Note, 9.0);
	}
	if (!bProbed)
	{
		return;
	}
	// It cost men and moves, and the corps it found are named now: the table and the card are read again.
	ShowWar(Session->GetView(), TArray<FCorpsMove>());
	// (The code checker, 17 November.) Where the staff has widened the attack the corps going in stand in several
	// provinces, and reading the tray again would cut the hand back to one of them while the card still said everyone goes.
	const TArray<int32> Going = PickedCorps;
	RefreshOrders();
	if (PickedBeforeMass.Num() > 0)
	{
		PickedCorps = Going;
	}
	FaceOff.bVisible = false;
	OpenFaceOff(RegionId);
	if (!FaceOff.bVisible)
	{
		// (nobody in hand can attack any longer: the corps that were in hand before the staff widened the attack are in hand again)
		if (PickedBeforeMass.Num() > 0)
		{
			PickedCorps = PickedBeforeMass;
		}
		PickedBeforeMass.Reset();
	}
}

void ACampaignPlayerController::BombLine()
{
	if (!FaceOff.bVisible || FaceOff.bResolved || IsAnswering() || !FaceOff.bBombOk || FaceOff.BombOpen.IsEmpty())
	{
		return;
	}
	const FString Door = FaceOff.BombOpen;
	const int32 RegionId = FaceOff.Region;
	CloseFaceOff(); // the attack is put by: it is ordered again when the bombers are back
	SelectRegion(RegionId);
	OpenPage(Door);
}

void ACampaignPlayerController::OpenFold()
{
	if (ArmyRegion != INDEX_NONE)
	{
		OpenPage(FString::Printf(TEXT("fold:%d"), ArmyRegion));
	}
}

void ACampaignPlayerController::SendOnlyPicked()
{
	if (!FaceOff.bVisible || FaceOff.bResolved || IsAnswering() || PickedBeforeMass.Num() == 0)
	{
		return;
	}
	const int32 RegionId = FaceOff.Region;
	const TArray<int32> Everyone = PickedCorps, PickedFirst = PickedBeforeMass;
	const double CardSince = FaceOffSince;
	PickedCorps = PickedBeforeMass;
	PickedBeforeMass.Reset();
	// (The UI director, 17 November.) The card turns to exactly the corps picked. Where they cannot go by themselves
	// (guns alone, ruling 211(a)) it still does: it shows them with no men to take the ground, says why at its foot,
	// and offers everyone in reach again.
	OpenFaceOff(RegionId);
	if (FaceOffSince == CardSince)
	{
		// (The rules refused the attack outright and said why on the paper: the card still shows everyone going in, and
		// the hand goes back to match it.)
		PickedCorps = Everyone;
		PickedBeforeMass = PickedFirst;
	}
}

void ACampaignPlayerController::ConfirmAttack()
{
	if (LiveBattle.IsValid())
	{
		return; // (his own battle is open: the card behind it waits for its result)
	}
	// (Rulings 134(f), 138(a) and 177(b).) On the card before a march the key, like the lit button, gives the order
	// the forecast favours.
	if (FaceOff.bVisible && !FaceOff.bResolved && (FaceOff.bMarch || FaceOff.bStorm) && FaceOff.bFavourSiege)
	{
		ConfirmMarch(false);
		return;
	}
	ResolveAttack();
}

void ACampaignPlayerController::ResolveAttack()
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || !FaceOff.bVisible || FaceOff.bResolved || IsAnswering())
	{
		return;
	}
	if (!FaceOff.Alone.IsEmpty())
	{
		return; // (the UI director, 17 November: SEND THEM IN takes no press and no key while the pick cannot go)
	}
	const int32 RegionId = FaceOff.Region;
	const bool bMarch = FaceOff.bMarch; // (rules 84.1: the army marches in and goes at the town as it arrives)
	const bool bStorm = FaceOff.bStorm; // (ruling 182: the army already round the town storms it)
	const bool bSally = FaceOff.bSally; // (the garrison's sortie against the army round its town)
	// (Speed, the design director's mark 1 for build 414.) The battle is fought by the rules on their own thread, with what
	// the army in hand will ask after it; the card turns to the result when the answer comes, a frame or a few later.
	const TCHAR* const Function = bSally ? TEXT("sally") : bStorm ? TEXT("storm") : bMarch ? TEXT("take") : TEXT("attack");
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Session->FightAside(Function, PickedCorps, RegionId, true, !bScripted,
		[RegionId, bMarch, Here = ArmyRegion, Hand = PickedCorps, HandBefore = PickedBeforeMass, bPlain = !bByRail && !bBySea, Adjective = HandAdjective()](const FAttackResult& Result)
		{
			// (as AfterFight leaves the army in hand, and the card it shows)
			FCampaignSession::FHandNext Next = HandOf(Result.Moved, RegionId, RegionId, bPlain, Adjective);
			if (!Result.bFought)
			{
				if (bMarch && Result.Moved.Num() > 0)
				{
					return Next;
				}
				Next = HandOf(TArray<FCorpsMove>(), HandBefore.Num() > 0 && Result.Moved.Num() == 0 ? Here : INDEX_NONE, INDEX_NONE, bPlain, Adjective);
				Next.Ids = HandBefore;
				return Next;
			}
			if (Result.Moved.Num() == 0)
			{
				Next.Region = Here;
				Next.Ids = Hand;
			}
			return Next;
		},
		[Self, RegionId, bMarch, bStorm, bSally](const FAttackResult& Result)
		{
			if (ACampaignPlayerController* Me = Self.Get())
			{
				Me->AfterFight(RegionId, bMarch, bStorm, bSally, Result);
			}
		});
}

bool ACampaignPlayerController::CanFightLive() const
{
	// (a plain attack on the card, ready to go in: not a march, a storm, or a sortie, and a pick that can go)
	const bool bPlain = ALandBattle::CampaignLiveOn() && !bScripted && FaceOff.bVisible && !FaceOff.bResolved && !FaceOff.bMarch && !FaceOff.bStorm
		&& !FaceOff.bSally && FaceOff.Alone.IsEmpty() && PickedCorps.Num() > 0 && !LiveBattle.IsValid();
	if (!bPlain)
	{
		return false;
	}
	// (release 1.5, Brian 23 November 05:18Z: only on a battlefield that has passed, and only where the field can fight it;
	// worked out once a card, the first time it is asked)
	if (FaceOff.FieldOffer < 0 || FaceOff.FieldPicks != PickedCorps)
	{
		const_cast<ACampaignPlayerController*>(this)->WeighFieldOffer();
	}
	return FaceOff.FieldOffer > 0;
}

FString ACampaignPlayerController::FieldLookAt(const FCampaignMapData& Data, int32 RegionId, int32 Turn)
{
	if (!Data.Regions.IsValidIndex(RegionId))
	{
		return FString();
	}
	const FCampaignRegion& R = Data.Regions[RegionId];
	LandField::Field F;
	F.Lat = Data.LatitudeAt(R.Center.Y);
	double Lon = Data.LongitudeAt(R.Center.X);
	while (Lon > 180.0) Lon -= 360.0;
	while (Lon < -180.0) Lon += 360.0;
	F.Lon = Lon;
	F.Terrain = TCHAR_TO_UTF8(*R.Terrain.ToLower());
	F.Month = (8 + FMath::Max(1, Turn) - 1) % 12 + 1; // (turn 1 is September 1939)
	const LandScape::Look L = LandScape::Pick(F);
	FString Word = UTF8_TO_TCHAR(LandScape::NameOf(L));
	if (LandScape::SnowOf(F, L) >= 0.25)
	{
		Word += TEXT("+snow");
	}
	return Word.ToLower();
}

bool ACampaignPlayerController::FieldLookPassed(const FString& Look)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TheaterFightEveryLook")))
	{
		return true;
	}
	TArray<FString> Lines;
	const FString File = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"), TEXT("fight-yourself-looks.txt"));
	if (!FFileHelper::LoadFileToStringArray(Lines, *File))
	{
		return Look == TEXT("bocage"); // (the list as Brian left it on 23 November: the bocage alone)
	}
	for (FString Line : Lines)
	{
		int32 Hash = INDEX_NONE;
		if (Line.FindChar(TEXT('#'), Hash))
		{
			Line.LeftInline(Hash);
		}
		Line.TrimStartAndEndInline();
		if (!Line.IsEmpty() && (Line == TEXT("*") || Line.Equals(Look, ESearchCase::IgnoreCase)))
		{
			return true;
		}
	}
	return false;
}

void ACampaignPlayerController::WeighFieldOffer()
{
	FaceOff.FieldOffer = 0;
	FaceOff.FieldPicks = PickedCorps;
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid() || PickedCorps.Num() == 0)
	{
		return;
	}
	if (FaceOff.FieldLook.IsEmpty())
	{
		FaceOff.FieldLook = FieldLookAt(Map->GetData(), FaceOff.Region, Session->GetView().Turn);
	}
	if (!FieldLookPassed(FaceOff.FieldLook))
	{
		UE_LOG(LogTheater, Display, TEXT("Campaign: FIGHT IT YOURSELF not offered at %d: its battlefield (%s) has not passed yet; SEND THEM IN only"), FaceOff.Region, *FaceOff.FieldLook);
		return;
	}
	// (the field must be able to fight this attack: a ticket, and not a kind of place the staff still settles)
	const FString IdsText = FString::JoinBy(PickedCorps, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
	FString LookText, LookError;
	TSharedPtr<FJsonObject> LookAnswer;
	const TSharedPtr<FJsonObject>* LookTicket = nullptr;
	if (!Session->CallBattle(TEXT("battleLook"), FString::Printf(TEXT("[[%s],%d]"), *IdsText, FaceOff.Region), LookText, LookError)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LookText), LookAnswer) || !LookAnswer.IsValid()
		|| !LookAnswer->TryGetObjectField(TEXT("ticket"), LookTicket) || !LookTicket || !LookTicket->IsValid())
	{
		UE_LOG(LogTheater, Display, TEXT("Campaign: FIGHT IT YOURSELF not offered at %d (%s): no field battle to fight here %s"), FaceOff.Region, *FaceOff.FieldLook, *LookError);
		return;
	}
	FaceOff.FieldOffer = 1;
	UE_LOG(LogTheater, Display, TEXT("Campaign: FIGHT IT YOURSELF offered at %d (%s)"), FaceOff.Region, *FaceOff.FieldLook);
}

void ACampaignPlayerController::FightLive()
{
	FCampaignSession* Session = GetSession();
	UWorld* Here = GetWorld();
	if (!Session || !Map.IsValid() || !Here || !CanFightLive() || IsAnswering() || TActorIterator<ALandBattle>(Here))
	{
		return;
	}
	const int32 RegionId = FaceOff.Region;
	const TArray<int32> CorpsIds = PickedCorps;
	const FString IdsText = FString::JoinBy(CorpsIds, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
	FString LookText, LookError;
	if (!Session->CallBattle(TEXT("battleLook"), FString::Printf(TEXT("[[%s],%d]"), *IdsText, RegionId), LookText, LookError))
	{
		UE_LOG(LogTheater, Warning, TEXT("Campaign: TC.battleLook failed: %s"), *LookError);
		Notify(TEXT("This battle cannot be fought in the field: SEND THEM IN lets your generals fight it."), 6.0);
		return;
	}
	TSharedPtr<FJsonObject> LookAnswer;
	const TSharedRef<TJsonReader<>> LookReader = TJsonReaderFactory<>::Create(LookText);
	const bool bLookRead = FJsonSerializer::Deserialize(LookReader, LookAnswer) && LookAnswer.IsValid();
	const TSharedPtr<FJsonObject>* LookTicket = nullptr;
	if (!bLookRead || !LookAnswer->TryGetObjectField(TEXT("ticket"), LookTicket) || !LookTicket || !LookTicket->IsValid())
	{
		FString Why;
		if (bLookRead && LookAnswer->TryGetStringField(TEXT("why"), Why) && !Why.IsEmpty())
		{
			// (a kind of place the staff still settles: the card stays, and SEND THEM IN with it)
			Notify(Why, 7.0);
			return;
		}
		// (no field battle here at all: the generals' answer is the only one there is)
		Notify(TEXT("There is no field battle to fight here: your generals send them in."), 5.0);
		ResolveAttack();
		return;
	}
	double TicketSeed = 0.0;
	(*LookTicket)->TryGetNumberField(TEXT("seed"), TicketSeed);
	// (release 1.5) The place on the earth, for the land rules' sky: the battle is laid on the look the card weighed
	// (LandScape::Pick from its latitude and longitude) under that latitude's sun, not Normandy's.
	if (Map->GetData().Regions.IsValidIndex(RegionId))
	{
		const FCampaignMapData& Data = Map->GetData();
		const FVector2D At = Data.Regions[RegionId].Center;
		double Lon = Data.LongitudeAt(At.X);
		while (Lon > 180.0) Lon -= 360.0;
		while (Lon < -180.0) Lon += 360.0;
		const TSharedPtr<FJsonObject>* Live = nullptr;
		TSharedPtr<FJsonObject> LiveObject = LookAnswer->TryGetObjectField(TEXT("live"), Live) && Live && Live->IsValid() ? *Live : MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> LatLon;
		LatLon.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Data.LatitudeAt(At.Y) * 100.0) / 100.0));
		LatLon.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Lon * 100.0) / 100.0));
		LiveObject->SetArrayField(TEXT("latLon"), LatLon);
		LookAnswer->SetObjectField(TEXT("live"), LiveObject);
		FString Placed;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Placed);
		if (FJsonSerializer::Serialize(LookAnswer.ToSharedRef(), Writer))
		{
			LookText = Placed;
		}
	}
	ALandBattle* Field = Here->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Field)
	{
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Campaign: FIGHT IT YOURSELF, the attack on %d by [%s] opened live (seed %.0f)"), RegionId, *IdsText, TicketSeed);
	LiveBattle = Field;
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Field->HandBack = [Self, RegionId, CorpsIds, TicketSeed](const FString& ResultJson)
	{
		if (ACampaignPlayerController* Me = Self.Get())
		{
			Me->LiveHandBack(RegionId, CorpsIds, TicketSeed, ResultJson);
		}
	};
	Field->LiveTicketWhenReady(LookText, TEXT("att"), true);
}

void ACampaignPlayerController::LiveHandBack(int32 RegionId, const TArray<int32>& CorpsIds, double TicketSeed, const FString& ResultJson)
{
	LiveBattle.Reset();
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	if (ResultJson.IsEmpty())
	{
		// (the field closed with no result: the attack card is as he left it, SEND THEM IN and CALL IT OFF still on it)
		UE_LOG(LogTheater, Warning, TEXT("Campaign: the battle for %d closed with no result; nothing booked"), RegionId);
		Notify(TEXT("The battle could not be fought to its end, so nothing was booked: SEND THEM IN lets your generals fight it."), 7.0);
		return;
	}
	const FAttackResult Result = Session->SettleLandLive(CorpsIds, RegionId, ResultJson, TicketSeed);
	UE_LOG(LogTheater, Display, TEXT("Campaign: the battle for %d booked (%s)"), RegionId, Result.bFought ? TEXT("fought") : TEXT("not booked"));
	FaceOffSince = FPlatformTime::Seconds(); // (the result card's half second before Enter, as after SEND THEM IN)
	AfterFight(RegionId, false, false, false, Result);
}

namespace
{
	/** (Release 1.6) The raid card's fly act, "raid:<kind>:<target>:<id>.<id>..." (the rules' raid card), read: false where it is not one. */
	bool ReadFlyAct(const FString& Act, FString& OutKind, int32& OutTarget, TArray<int32>& OutIds)
	{
		TArray<FString> Parts;
		Act.ParseIntoArray(Parts, TEXT(":"), false);
		if (Parts.Num() != 4 || Parts[0] != TEXT("raid") || Parts[1].IsEmpty() || !Parts[2].IsNumeric())
		{
			return false;
		}
		OutKind = Parts[1];
		OutTarget = FCString::Atoi(*Parts[2]);
		OutIds.Reset();
		TArray<FString> Picked;
		Parts[3].ParseIntoArray(Picked, TEXT("."), true);
		for (const FString& Id : Picked)
		{
			if (!Id.IsNumeric())
			{
				return false;
			}
			OutIds.Add(FCString::Atoi(*Id));
		}
		return OutIds.Num() > 0;
	}

	/** (Release 1.6) The raids he may fly himself: a bombing raid by day and a fight for the sky (the rules' FOUGHT_KINDS). */
	bool IsFlownKind(const FString& Kind)
	{
		return Kind == TEXT("bomb") || Kind == TEXT("superiority");
	}
}

bool ACampaignPlayerController::CanFlyLive() const
{
	// (the raid card up, a raid by day or for the sky that can go as picked, and no battle of his already open)
	if (!AAirBattle::CampaignLiveOn() || bScripted || !bPageOpen || !Page.Card.bValid || !Page.Card.Fly.bOk || LiveRaid.IsValid() || LiveBattle.IsValid())
	{
		return false;
	}
	FString Kind;
	int32 Target = INDEX_NONE;
	TArray<int32> Ids;
	if (!ReadFlyAct(Page.Card.Fly.Act, Kind, Target, Ids) || !IsFlownKind(Kind))
	{
		return false;
	}
	if (FlyOfferAct != Page.Card.Fly.Act)
	{
		// (weighed once a pick, the first time it is asked: the rules must have a battle in the air for this raid)
		FlyOfferAct = Page.Card.Fly.Act;
		FlyOffer = 0;
		FCampaignSession* Session = GetSession();
		FString LookText, LookError;
		TSharedPtr<FJsonObject> LookAnswer;
		const TSharedPtr<FJsonObject>* LookTicket = nullptr;
		const FString IdsText = FString::JoinBy(Ids, TEXT(","), [](int32 Id) { return FString::FromInt(Id); });
		if (!Session || !Session->CallBattle(TEXT("airLook"), FString::Printf(TEXT("[\"%s\",%d,[%s]]"), *Kind, Target, *IdsText), LookText, LookError)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LookText), LookAnswer) || !LookAnswer.IsValid()
			|| !LookAnswer->TryGetObjectField(TEXT("ticket"), LookTicket) || !LookTicket || !LookTicket->IsValid())
		{
			FString Why = LookError;
			if (LookAnswer.IsValid())
			{
				LookAnswer->TryGetStringField(TEXT("why"), Why);
			}
			UE_LOG(LogTheater, Display, TEXT("Campaign: FLY IT YOURSELF not offered for %s: no battle in the air for it (%s); FLY THE RAID only"), *FlyOfferAct, *Why);
			return false;
		}
		FlyOffer = 1;
		UE_LOG(LogTheater, Display, TEXT("Campaign: FLY IT YOURSELF offered for %s"), *FlyOfferAct);
	}
	return FlyOffer > 0;
}

void ACampaignPlayerController::FlyLive()
{
	FCampaignSession* Session = GetSession();
	UWorld* Here = GetWorld();
	if (!Session || !Map.IsValid() || !Here || !CanFlyLive() || IsAnswering() || TActorIterator<AAirBattle>(Here) || TActorIterator<ALandBattle>(Here))
	{
		return;
	}
	FString Kind;
	int32 Target = INDEX_NONE;
	TArray<int32> Ids;
	if (!ReadFlyAct(Page.Card.Fly.Act, Kind, Target, Ids))
	{
		return;
	}
	CampaignSounds::Play(this, TEXT("stamp"), 0.6f);
	AAirBattle* Sky = Here->SpawnActor<AAirBattle>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Sky)
	{
		return;
	}
	const TWeakObjectPtr<ACampaignPlayerController> Self(this);
	Sky->HandBack = [Self, Kind, Target](const FString& EndJson)
	{
		if (ACampaignPlayerController* Me = Self.Get())
		{
			Me->AirHandBack(Kind, Target, EndJson);
		}
	};
	if (!Sky->BeginLive(Session, Kind, Target, Ids))
	{
		// (the rules would not open it after all: the card stays, FLY THE RAID with it, and the offer is not made again for this pick)
		Sky->HandBack = nullptr;
		Sky->Destroy();
		FlyOffer = 0;
		Notify(TEXT("This raid cannot be flown in the air battle: FLY THE RAID sends your crews."), 6.0);
		return;
	}
	LiveRaid = Sky;
	UE_LOG(LogTheater, Display, TEXT("Campaign: FLY IT YOURSELF, %s on %d by [%s] opened live"), *Kind, Target, *FString::JoinBy(Ids, TEXT(","), [](int32 Id) { return FString::FromInt(Id); }));
	FlyLivePage = Page.Id;
	FlyLiveAct = Page.Card.Fly.Act;
	ClosePage();
}

void ACampaignPlayerController::AirHandBack(const FString& Kind, int32 Target, const FString& EndJson)
{
	LiveRaid.Reset();
	FlyOfferAct.Reset();
	FlyOffer = -1;
	const FString FlownPage = FlyLivePage, FlownAct = FlyLiveAct;
	FlyLivePage.Reset();
	FlyLiveAct.Reset();
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	FString Note;
	TArray<FSortie> Sorties;
	const bool bBooked = !EndJson.IsEmpty() && Session->SettleAirLive(EndJson, Note, &Sorties);
	// (whatever came of it, the table is brought up to the rules' picture of the war: the groups away, the target hit)
	ShowWar(Session->GetView(), TArray<FCorpsMove>());
	RefreshOrders();
	if (EndJson.IsEmpty())
	{
		UE_LOG(LogTheater, Warning, TEXT("Campaign: the raid (%s on %d) closed with no result; nothing booked"), *Kind, Target);
		// (release 1.5: nothing booked, so the raid card comes back as he left it, FLY THE RAID on it; FLY IT YOURSELF is not
		// offered again for that pick, whose rules have just failed, as when BeginLive cannot open it)
		if (!FlownPage.IsEmpty())
		{
			OpenPage(FlownPage);
			if (bPageOpen)
			{
				FlyOfferAct = FlownAct;
				FlyOffer = 0;
			}
		}
		Notify(TEXT("The raid could not be flown to its end, so nothing was booked: FLY THE RAID on its card still sends your crews."), 7.0);
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Campaign: the raid (%s on %d) flown live, booked: %s"), *Kind, Target, bBooked ? TEXT("yes") : TEXT("no"));
	if (!Note.IsEmpty())
	{
		Notify(Note, 12.0);
	}
	// The eye goes to the target, where the map now shows what came of it. (Not in a run that takes pictures.)
	FString Shots;
	const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
	if (Sorties.Num() > 0 && !bPictures)
	{
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			Eye->GoTo(Map->MapToWorld(Sorties[0].To.X, Sorties[0].To.Y), FMath::Clamp(Eye->GetZoom(), 0.3f, 0.45f));
		}
	}
}

void ACampaignPlayerController::AfterFight(int32 RegionId, bool bMarch, bool bStorm, bool bSally, const FAttackResult& Result)
{
	FCampaignSession* Session = GetSession();
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const TArray<int32> HandBefore = PickedBeforeMass;
	PickedBeforeMass.Reset();
	if (!Result.bFought)
	{
		FaceOff.bVisible = false;
		if (HandBefore.Num() > 0 && Result.Moved.Num() == 0)
		{
			// (The code checker, 17 November.) Refused after the staff had widened the attack: the corps that were in
			// hand are in hand again, and the next order moves nobody else's.
			PickedCorps = HandBefore;
			RefreshOrders();
		}
		if (bStorm || bSally)
		{
			Notify(Result.Note.IsEmpty() ? FString(bSally ? TEXT("The sortie could not go out.") : TEXT("The assault could not go in.")) : Result.Note, 7.0);
			return;
		}
		if (bMarch && Result.Moved.Num() > 0)
		{
			// (the march met no fight after all: the place fell as the army came, or the army stands round the town)
			ShowWar(Session->GetView(), Result.Moved, nullptr, true);
			CampaignSounds::Play(this, TEXT("stamp"), 0.7f);
			PickedCorps.Reset();
			for (const FCorpsMove& March : Result.Moved)
			{
				PickedCorps.Add(March.CorpsId);
			}
			ArmyRegion = RegionId;
			if (!Result.Note.IsEmpty())
			{
				Notify(Result.Note, 9.0);
			}
			LaterOrders([this, RegionId]()
			{
				SelectRegion(RegionId);
			});
			return;
		}
		Notify(bMarch ? TEXT("They cannot get there this month.") : TEXT("The attack could not go in."));
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
	FaceOffSince = FPlatformTime::Seconds(); // (the result takes no key for its first half second either)
	{
		// A capital taken, or a great battle won outright (a hundred thousand men and more lost between the two
		// sides): when its result has been read, the eye drops to the place for a moment. Not in a run that takes pictures.
		FString Shots;
		const bool bPictures = FParse::Value(FCommandLine::Get(), TEXT("TheaterShots="), Shots, false);
		const bool bCapital = Map->GetData().Regions.IsValidIndex(RegionId) && Map->GetData().Regions[RegionId].bCapital;
		const bool bGreat = !Result.bYielded && Result.Level.Contains(TEXT("Decisive victory")) && Result.AttackerLost + Result.DefenderLost >= 100000;
		bMomentDue = bMoments && !bPictures && !bScripted && ((Result.bTaken && bCapital) || bGreat);
	}
	FaceOff.ResultTitle = FText::FromString(Result.bYielded ? FString(TEXT("They fell back without a fight")) : Result.Level);
	const FString Outcome = bSally ? FString(Result.bTaken ? TEXT("The ring is broken: the siege is raised.") : TEXT("The sortie was driven back in. The siege goes on."))
		: (bMarch || bStorm) ? (Result.bTaken || Result.bFell ? TEXT("The town is stormed. The ground is yours.")
			: TEXT("The assault was thrown back. Your army stands round the town: the siege goes on, and the assault can go in again next month, made ready."))
		: Result.bSiege ? TEXT("The country is yours, but the town holds. It is now besieged: starve it out, or pick the province and storm it.")
		: Result.bTaken ? TEXT("The ground is yours.") : TEXT("The attack was held. The ground is still theirs.");
	FString Cost = FString::Printf(TEXT("You lost %s men; they lost %s"), *Thousands(Result.AttackerLost), *Thousands(Result.DefenderLost));
	if (Result.Prisoners > 0)
	{
		Cost += FString::Printf(TEXT(", %s of them taken prisoner"), *Thousands(Result.Prisoners));
	}
	Cost += TEXT(".");
	if (!Result.Price.IsEmpty())
	{
		// (Version 63) The price as the rules state it: men lost on each side, and the share of those sent in.
		Cost = Result.Price;
		if (Result.Prisoners > 0)
		{
			Cost += FString::Printf(TEXT(" %s of theirs were taken prisoner."), *Thousands(Result.Prisoners));
		}
	}
	FaceOff.ResultDetail = FText::FromString(Outcome + TEXT("  ") + Cost);
	// (Version 84) Who paid, and to what: the rules' sentence, under the price.
	FaceOff.Paid = Result.bYielded ? FString() : Result.Paid;
	FaceOff.PaidSign = Result.PaidSign;
	if (Result.Lines.Num() > 0)
	{
		WriteTicket(Result.Lines, FaceOff); // the ticket as it was on the day, read in the past
	}
	if (Result.Odds > 0.0f && !Result.bYielded)
	{
		// (and the odds as they stood on the day, in place of the forecast)
		FaceOff.OddsLine = FText::FromString(Result.Odds >= 1.0f
			? FString::Printf(TEXT("The odds on the day were %.1f to 1 in your favour"), Result.Odds)
			: FString::Printf(TEXT("The odds on the day were %.1f to 1 against you"), 1.0f / FMath::Max(Result.Odds, 0.01f)));
	}
	FaceOff.OurRoster = RosterOf(Result.Attackers, true);
	FaceOff.TheirRoster = RosterOf(Result.Defenders, true);
	FaceOff.OurUnits = FText::FromString(ListUnits(FaceOff.OurRoster));
	FaceOff.TheirUnits = FText::FromString(ListUnits(FaceOff.TheirRoster));

	// Show the result on the table: ground repainted if it changed hands, and the corps that followed up carried in.
	ShowWar(Session->GetView(), Result.Moved, nullptr, true);
	if (Result.Moved.Num() > 0)
	{
		PickedCorps.Reset();
		for (const FCorpsMove& March : Result.Moved)
		{
			PickedCorps.Add(March.CorpsId);
		}
		ArmyRegion = RegionId;
	}
	LaterOrders([this, RegionId]()
	{
		SelectRegion(RegionId);
	});
}

void ACampaignPlayerController::CloseFaceOff()
{
	if (LiveBattle.IsValid())
	{
		return; // (his own battle is open: the card behind it waits for its result)
	}
	if (FaceOff.bVisible && !FaceOff.bResolved && IsAnswering())
	{
		return; // (the attack has been sent in and its answer is on its way: the card is not called off under it)
	}
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
	const bool bMoment = FaceOff.bVisible && FaceOff.bResolved && bMomentDue;
	bMomentDue = false;
	FaceOff.bVisible = false;
	if (bMoment)
	{
		StartMoment(FaceOff.Region);
	}
}

void ACampaignPlayerController::EndMonth()
{
	if (LiveBattle.IsValid() || LiveRaid.IsValid())
	{
		return; // (his own battle is open: no key reaches the map behind it)
	}
	// (Speed, b414 mark 1.) An order still being answered is finished, and its picture laid, before anything else: the
	// END THE MONTH button is not gated as the keys are, and the answer's follow-up must not land on the next month's table.
	FinishAnswer();
	FCampaignSession* Session = GetSession();
	// (Rulings 134(f) and 138(1): before a saved campaign is replaced, Enter keeps the campaign. And with a card up
	// the key never ends the month behind it: it presses the card's lit button, below.)
	if (Opening.bVisible && Opening.AskKind != 0)
	{
		AnswerBegin(false);
		return;
	}
	if (Opening.bVisible && !Opening.AskDelete.IsEmpty())
	{
		AnswerDeleteWar(false); // (release 1.5.1: Enter keeps the saved war, as KEEP IT)
		return;
	}
	// (The UI director's 84.3 check, item 6.) On the door Enter is BEGIN AS ..., its lit button, once the door has been
	// up for half a second. Where a campaign is saved, that asks first, and the question's lit answer keeps it.
	if (Opening.bVisible && bEnterByKey && !bScripted)
	{
		if (CanBeginByKey() && FPlatformTime::Seconds() - DoorSince > 0.5)
		{
			AskToBegin(1);
		}
		return;
	}
	// (A paper laid over the map has no lit button: Enter never ends the month behind it. Esc puts it away. The Guide's
	// list of the ten lessons is such a paper.)
	if ((Document.bValid || bLessonGuideOpen) && bEnterByKey && !bScripted)
	{
		return;
	}
	if (WarPrompt.bVisible && !bScripted)
	{
		if (!WarPrompt.IsWarQuestion())
		{
			// (The design director, 17 November: Enter marches. He asked for the card himself; not in its first half second.)
			// (The UI director, 17 November, in place of ruling 138(1)'s words: as in the browser, Enter gives the lit
			// order on the landing and the gate cards too, and Esc puts them away. Those orders can be changed again before
			// the month ends. Before a declaration of war, which cannot be taken back, Enter is NOT NOW, below.)
			if (FPlatformTime::Seconds() - WarPromptSince > 0.5)
			{
				ConfirmWar();
			}
			return;
		}
		CloseWarPrompt();
		return;
	}
	if (FaceOff.bVisible && !bScripted)
	{
		// (Ruling 203; "Questions and keys".) On the attack card Enter is the lit button: he opened the card himself.
		// Not in the card's first half second, nor in the result's; and never on a card that opened by itself (ruling 183).
		if (FPlatformTime::Seconds() - FaceOffSince > 0.5)
		{
			if (FaceOff.bResolved)
			{
				CloseFaceOff(); // (the UI director, 17 November: CONTINUE takes Enter and Space)
			}
			else if (FaceOff.CallOffLit() && !FaceOff.bMarch && !FaceOff.bStorm)
			{
				// (the verdict is a long shot or worse, or the pick cannot go: Enter is CALL IT OFF, and the paper line says so)
				const FString Title = FaceOff.Title.ToString();
				CloseFaceOff();
				Notify(FString::Printf(TEXT("%s is called off. Nothing was spent."), *Title), 5.0);
			}
			else if (!FaceOff.bArrival)
			{
				ConfirmAttack();
			}
		}
		return;
	}
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
	// (Ruling 64.) With the month's card up, the key that ends the month is GIVE ORDERS: it puts the card away, and
	// must not end another month unseen.
	if (!bScripted && bPageOpen && Page.Month.bValid)
	{
		ClosePage();
		return;
	}
	// (Rulings 88 and 90.) Armies at the front that have not been touched and have no orders: the month is never
	// barred, but he is asked once, and may tick not to be asked. With the question up, this key is its END THE
	// MONTH: but the question takes no key for its first half second, so that a held or doubled Enter cannot end a
	// month before he has seen it.
	if (EndAsk.bVisible)
	{
		if (FPlatformTime::Seconds() - EndAsk.Since < 0.5)
		{
			return;
		}
		if (bEnterByKey)
		{
			// (Rules 84.3.1; the UI director's ruling of 17 November, one rule for every card: Enter presses the lit
			// button, and the lit button is the safe one.) On the question Enter is SHOW ME, as Tab is: a double Enter
			// shows the armies and never ends the month. END THE MONTH takes a click.
			AnswerEndAsk(true);
			return;
		}
		EndAsk.bVisible = false;
	}
	else if (!bScripted && bAskAtMonthsEnd && EndAskedTurn != Session->GetView().Turn)
	{
		// (Ruling 111(4): the question's number is the button's number.)
		const int32 Idle = Session->GetView().Round.Bright;
		EndAsk.Raised = FMath::Max(Session->GetView().Round.Bright - Session->GetView().Round.Front, 0);
		if (Idle > 0)
		{
			EndAskedTurn = Session->GetView().Turn;
			EndAsk.bVisible = true;
			EndAsk.Count = Idle;
			EndAsk.Since = FPlatformTime::Seconds();
			CampaignSounds::Play(this, TEXT("click"), 0.6f);
			return;
		}
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
	// (release 1.5.1, new units ready: last month's plate goes as this month ends)
	ReadyUnits.Reset();
	bReadyDue = false;
	ReadySaid = -1;
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
	// (Ruling 203.) The month's card is the first thing he sees: the other countries' moves play first only where he
	// has asked for that (Options, or END THE MONTH AND WATCH), and are TOUR THE MAP on the card otherwise.
	if (Month.Steps.Num() > 0 && (bAlwaysWatch || (bShowsFirst && !bScripted)))
	{
		// Other nations did things: go and look at each in turn, quickly.
		bWatching = true;
		WatchNext = 0.0;
		// (Dusk when the month ends: evening comes over the table while the rest of the world moves.)
		for (TActorIterator<ACampaignEnvironment> Room(GetWorld()); Room; ++Room)
		{
			Room->SetMonthsEnd(true);
		}
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

void ACampaignPlayerController::PlayLastReel()
{
	if (LastReel.Num() == 0 || Newsreel.bOn)
	{
		return;
	}
	ClosePage();
	bMonthCardDue = true; // (the month's card comes back when the film is over)
	PlayNewsreel(LastReelDate, LastReel);
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
	// (release 1) And the ships that went down in them are seen to go: the biggest first, no more than six of a month's fight.
	{
		static const TCHAR* Order[] = { TEXT("bb"), TEXT("cv"), TEXT("ca"), TEXT("dd"), TEXT("ss") };
		int32 Shown = 0;
		for (const TCHAR* Class : Order)
		{
			for (const FSinking& Sinking : Step.Sinkings)
			{
				// (a ship sunk at her moorings by a raid on a harbour has no sea, zone -1: she is not counted among the six
				// the chart shows going down. The pre-check, 19:36Z on 19 November 2026.)
				for (int32 Each = 0; Sinking.Zone >= 0 && Sinking.Class == Class && Each < FMath::Min(Sinking.Count, 2) && Shown < 6; ++Each)
				{
					Map->AddSinking(Sinking.Zone, Sinking.Nation, Sinking.Class, Shown++);
				}
			}
		}
	}
	Remember(Step.Sinkings); // (Brian's memorial card: the battleships and carriers among them, by name)
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
	const double Pause = Step.Raids.Num() > 0 ? (Step.bMine ? 6.0 : 2.2) : Step.Sinkings.Num() > 0 ? 6.4 : Step.SeaFights.Num() > 0 && Step.Battles.Num() == 0 ? (Step.bMine ? 2.4 : 1.4) : Step.bMine ? 1.3 : Step.bMarchOnly ? 0.55 : 0.8;
	Notify(Step.Note, Pause + 0.3);
	WatchNext = FPlatformTime::Seconds() + Pause;
}

void ACampaignPlayerController::WatchAgain()
{
	if (bWatching || Month.Steps.Num() == 0 || !Map.IsValid())
	{
		return;
	}
	ClosePage();
	bRewatching = true;
	WatchStood.Reset();
	for (const FViewCorps& Unit : Month.Steps[0].View.Corps)
	{
		WatchStood.Add(Unit.Id, Unit.Region);
	}
	WatchIndex = 0;
	if (const ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		WatchReturnFocus = Eye->GetFocus();
		WatchReturnZoom = Eye->GetZoom();
	}
	bWatching = true;
	WatchNext = 0.0;
	for (TActorIterator<ACampaignEnvironment> Room(GetWorld()); Room; ++Room)
	{
		Room->SetMonthsEnd(true);
	}
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->SetHurry(true);
	}
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
	// (and with the new month it is morning again)
	for (TActorIterator<ACampaignEnvironment> Room(GetWorld()); Room; ++Room)
	{
		Room->SetMonthsEnd(false);
	}
	if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
	{
		Eye->SetHurry(false);
	}
	if (!Session || !Map.IsValid())
	{
		return;
	}
	const FCampaignView& View = Session->GetView();
	RoundSeen.Reset(); // (the code checker, 17 November: a new month, and Tab's round of the armies begins again)
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
	if (bRewatching)
	{
		// (watched again from the month's card: nothing is kept or told twice, and the card comes back)
		bRewatching = false;
		bMonthCardDue = true;
		return;
	}
	// (Release 1.5.1, new units ready; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered".)
	// The units handed over to him as his month opened are named on a plate once nothing else holds the screen, the
	// month's card closed (PlayerTick); while pictures are taken, at once (below).
	ReadyUnits = Month.Fallen.IsEmpty() && !View.Human.IsEmpty() ? Month.Ready : TArray<FReadyUnit>();
	ReadyNext = 0;
	ReadySaid = -1;
	ReadyNation = View.Human;
	bReadyDue = bReadyNotice && ReadyUnits.Num() > 0;

	// The campaign is kept every month, so it can be continued another day.
	if (ACampaignGameMode* Mode = GetMode())
	{
		Mode->SaveCampaign();
	}

	// (Ruling 203.) The month's card comes first: the great moments are its bands, and the newsreel is a press away on
	// it. They play before it only where he has asked for that in Options, and where there is no card to come (his
	// country has fallen, or nobody is playing).
	const bool bShowsNow = (bShowsFirst && !bScripted) || !Month.Fallen.IsEmpty() || View.Human.IsEmpty();
	LastReel = Month.Reel;
	LastReelDate = Month.ReelDate;
	// The great moments of the month are told one at a time, each on a screen of its own. (Ruling 204(3): unasked,
	// only the campaign's ending may come before the card.)
	Announcements.Reset();
	for (const FAnnouncement& Told : Month.Announcements)
	{
		if (bShowsNow || Told.bEnding)
		{
			Announcements.Add(Told);
		}
	}
	// (not while pictures are being taken: the newsreel has a picture of its own)
	const bool bReel = bNewsreels && bShowsNow && !bScripted && Month.Reel.Num() > 0 && Month.Fallen.IsEmpty();
	if (Announcements.Num() > 0 && !bReel)
	{
		CampaignSounds::PlayLater(this, Announcements[0].bGood ? TEXT("victory") : TEXT("defeat"), 0.4f, 0.55f);
	}

	// A January opens with the year's title card; then the newsreel, if the month had stories worth one. The
	// great moments wait until both are over.
	LastYear = Month.Year.bValid ? Month.Year : LastYear;
	if (Month.Year.bValid && bBriefings && bShowsNow && !bScripted && Month.Fallen.IsEmpty())
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
	// (Ruling 64; Brian, 16 November 2026: "i dont love the turn update UI telling you what happend. its very plain
	// and hard to read".) The month's card takes the typed strip's place: it opens once the newsreel and the great
	// moments are over (see PlayerTick). The strip is kept only where there is no card, which is when the
	// player's country has fallen. While pictures are being taken the card is opened by asking for it.
	const bool bCard = Month.Fallen.IsEmpty() && !View.Human.IsEmpty();
	DispatchUntil = bCard ? 0.0 : FPlatformTime::Seconds() + 25.0;
	bMonthCardDue = bCard && !bScripted;

	if (!Month.Fallen.IsEmpty())
	{
		const FCampaignNation* Nation = Map->GetData().FindNation(Month.Fallen);
		Notify(FString::Printf(TEXT("%s has fallen. The war goes on without you."), Nation ? *HeadThe(Nation->Name) : *Month.Fallen), 12.0);
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
	if (bReadyDue && bScripted)
	{
		SayReady(); // (release 1.5.1, new units ready: a picture has the plate at once)
	}
}

// (Release 1.5.1, new units ready; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered")
void ACampaignPlayerController::SayReady()
{
	bReadyDue = false;
	if (!bReadyNotice || ReadyUnits.Num() == 0)
	{
		return;
	}
	// Each unit in the rules' own line ("XXXI Corps has finished forming at Paris and is ready for orders."), three at most.
	TArray<FString> Lines;
	for (int32 K = 0; K < ReadyUnits.Num() && K < 3; ++K)
	{
		Lines.Add(ReadyUnits[K].Text);
	}
	if (ReadyUnits.Num() > 3)
	{
		Lines.Add(FString::Printf(TEXT("And %d more: press R for the month's report."), ReadyUnits.Num() - 3));
	}
	ReadyHeading = ReadyUnits.Num() == 1 ? FString(TEXT("READY FOR ORDERS")) : FString::Printf(TEXT("READY FOR ORDERS  \u00B7  %d NEW"), ReadyUnits.Num());
	Notify(FString::Join(Lines, TEXT("\n")), 8.0 + 3.0 * FMath::Min(ReadyUnits.Num(), 3));
	ReadySaid = NoticesSaid;
	ReadyNext = 0;
}

void ACampaignPlayerController::ShowReady()
{
	if (!CanShowReady() || !Map.IsValid())
	{
		return;
	}
	const TArray<FCampaignRegion>& Regions = Map->GetData().Regions;
	const int32 Count = ReadyUnits.Num();
	const int32 Index = ReadyNext % Count;
	ReadyNext = Index + 1;
	const FReadyUnit& Unit = ReadyUnits[Index];
	// (a corps is looked for where it stands now: it may have been moved since)
	int32 Place = Unit.Region;
	if (const FCampaignSession* ReadySession = GetSession(); ReadySession && Unit.Corps != INDEX_NONE)
	{
		for (const FViewCorps& Corps : ReadySession->GetView().Corps)
		{
			if (Corps.Id == Unit.Corps)
			{
				Place = Corps.Region;
				break;
			}
		}
	}
	if (Regions.IsValidIndex(Place))
	{
		if (ACampaignCameraPawn* Eye = Cast<ACampaignCameraPawn>(GetPawn()))
		{
			Eye->GoTo(Map->MapToWorld(Regions[Place].Center.X, Regions[Place].Center.Y), 0.42f);
		}
		// (the place picked, and a new army there put in his hand)
		SelectRegion(Place);
		if (Unit.Corps != INDEX_NONE && PickUpArmyIn(Place))
		{
			CampaignSounds::Play(this, TEXT("pickup"), 0.5f);
		}
	}
	// The plate says the one looked at, and stands a while longer.
	ReadyHeading = Count == 1 ? FString(TEXT("READY FOR ORDERS")) : FString::Printf(TEXT("READY FOR ORDERS  \u00B7  %d OF %d"), Index + 1, Count);
	Notify(Unit.Text, 10.0);
	ReadySaid = NoticesSaid;
}

void ACampaignPlayerController::Notify(const FString& Text, double Seconds)
{
	++NoticesSaid; // (the playtester's hook: a sentence said again is still an answer)
	NoticeText = Text;
	NoticeUntil = FPlatformTime::Seconds() + Seconds;
}

// (Brian's memorial card, 19 November 2026.) A battleship or a carrier that goes down in a fight the player's side
// was in is named on a small plate low on the screen: where, who she was, and the men lost with her. The names,
// crews and losses are the rules' own record ('hulls'); nothing is said of a ship the rules do not name a class
// for. The plate takes no click and stops no play: it fades in, stands a few seconds and fades out.
void ACampaignPlayerController::Remember(const TArray<FSinking>& Sinkings, bool bHold)
{
	if (!Map.IsValid())
	{
		return;
	}
	// (The names were the rules builder's draft until the historian had checked the roster; the plate was silent in
	// play until then. The historian's corrections were cleared at 20:02Z on 19 November 2026 and the coordinator
	// gave the word: the plate shows. "-TheaterNoMemorial" keeps it away, for a picture that wants the sea clear.)
	static const bool bKeptAway = FParse::Param(FCommandLine::Get(), TEXT("TheaterNoMemorial"));
	if (!bHold && bKeptAway)
	{
		return;
	}
	FMemorialShow Card;
	int32 Zone = INDEX_NONE, Harbour = INDEX_NONE, Extra = 0;
	bool bOneSea = true, bAny = false;
	for (const FSinking& Sinking : Sinkings)
	{
		const FCampaignNation* Nation = Map->GetData().FindNation(Sinking.Nation);
		const FString Whose = Nation && !Nation->Adjective.IsEmpty() ? Nation->Adjective : Sinking.Nation;
		for (const FLostHull& Hull : Sinking.Ships)
		{
			if (Hull.Class != TEXT("bb") && Hull.Class != TEXT("cv"))
			{
				continue;
			}
			bOneSea = bOneSea && (!bAny || (Zone == Sinking.Zone && Harbour == Sinking.Region));
			bAny = true;
			Zone = Sinking.Zone;
			Harbour = Sinking.Region;
			if (Card.Rows.Num() >= 3)
			{
				++Extra;
				continue;
			}
			// (the historian's word for her where the rules give one: Hood was a battlecruiser, not a battleship)
			const FString KindWord = Hull.bPair ? FString(TEXT("light carriers")) : !Hull.Type.IsEmpty() ? Hull.Type : Hull.Class == TEXT("cv") ? FString(TEXT("aircraft carrier")) : FString(TEXT("battleship"));
			const TCHAR* Kind = *KindWord;
			const TCHAR* Her = Hull.bPair ? TEXT("their") : TEXT("her");
			const FString Men = FText::AsNumber(FMath::Max(Hull.Crew, 0)).ToString();
			// (The two directors' single ruling, 19 November 2026: one wording for both sides, and the line ends at "lost".)
			FString Loss;
			if (Hull.Crew > 0 && Hull.Lost >= Hull.Crew)
			{
				Loss = FString::Printf(TEXT("All %s of %s men were lost."), *Men, Her);
			}
			else if (Hull.Crew > 0 && Hull.Lost <= 0)
			{
				Loss = FString::Printf(TEXT("All %s of %s men were saved."), *Men, Her);
			}
			else if (Hull.Crew > 0 && Hull.Lost == 1)
			{
				Loss = FString::Printf(TEXT("One of %s %s men was lost."), Her, *Men);
			}
			else if (Hull.Crew > 0)
			{
				Loss = FString::Printf(TEXT("%s of %s %s men were lost."), *FText::AsNumber(Hull.Lost).ToString(), Her, *Men);
			}
			FMemorialShow::FRow& Row = Card.Rows.AddDefaulted_GetRef();
			if (!Hull.Name.IsEmpty())
			{
				Row.Name = Hull.Name;
				Row.Line = FString::Printf(TEXT("%s %s. %s"), *Whose, Kind, *Loss).TrimEnd();
			}
			else
			{
				// (the record of names has run out: she is still marked, as what she was)
				const bool bVowel = Whose.Len() > 0 && FString(TEXT("AEIOU")).Contains(Whose.Left(1).ToUpper());
				Row.Name = FString::Printf(TEXT("%s %s %s"), bVowel ? TEXT("An") : TEXT("A"), *Whose, Kind);
				Row.Line = Loss;
			}
		}
	}
	if (Card.Rows.Num() == 0)
	{
		return;
	}
	const FString Sea = bOneSea && Map->GetData().Seas.IsValidIndex(Zone) ? Map->GetData().Seas[Zone].Name : FString();
	// (A raid on a harbour sinks ships at their moorings: the bridge gives the province and no sea. They were not
	// lost at sea, and the plate says where they lay.)
	const bool bInHarbour = bOneSea && !Map->GetData().Seas.IsValidIndex(Zone) && Map->GetData().Regions.IsValidIndex(Harbour);
	Card.Heading = bInHarbour ? FString::Printf(TEXT("LOST IN HARBOUR  \u00B7  %s"), *Map->GetData().Regions[Harbour].Name.ToUpper())
		: Sea.IsEmpty() ? FString(TEXT("LOST AT SEA")) : FString::Printf(TEXT("LOST AT SEA  \u00B7  %s"), *Sea.ToUpper());
	if (Extra > 0)
	{
		Card.More = Extra == 1 ? FString(TEXT("and one more capital ship")) : FString::Printf(TEXT("and %d more capital ships"), Extra);
	}
	Card.From = FPlatformTime::Seconds();
	// (The art director and the UI director, 19 November 2026: 6.5 seconds for one ship and two more for each further name.)
	Card.Until = Card.From + (bHold ? 3600.0 : 6.5 + 2.0 * (Card.Rows.Num() - 1));
	Memorial = MoveTemp(Card);
	UE_LOG(LogTheater, Display, TEXT("Memorial: %s; %d named, %d more; first: %s (%s)"), *Memorial.Heading, Memorial.Rows.Num(), Extra, *Memorial.Rows[0].Name, *Memorial.Rows[0].Line);
}

float ACampaignPlayerController::GetMemorialShown() const
{
	const double Clock = FPlatformTime::Seconds();
	if (Memorial.Rows.Num() == 0 || Clock >= Memorial.Until)
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::Min(FMath::Clamp((Clock - Memorial.From) / 0.6, 0.0, 1.0), FMath::Clamp((Memorial.Until - Clock) / 1.2, 0.0, 1.0)));
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
	FString Note = FString::Printf(TEXT("You lead %s"), Nation ? *WithThe(Nation->Name) : *View.Human);
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
	THEATER_SPEED_SCOPE(RefreshRegionPanel);
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
	RegionPanel.Details = FText::FromString(CampaignText::Phrases(FString::Join(Lines, TEXT("   \u00B7   "))));

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
				RegionPanel.Details = FText::FromString(CampaignText::Phrases(FString::Printf(TEXT("Chief town %s  \u00B7  %s"), *Province.Town, *RegionPanel.Details.ToString())));
			}
			RegionPanel.Prize = FText::FromString(Province.PrizeLine);
			RegionPanel.PrizeTip = FText::FromString(FString::Join(Province.PrizeItems, TEXT("\n")));
			RegionPanel.bPrizeToTake = Province.PrizeKind == TEXT("taken") || Province.PrizeKind == TEXT("restored");
			RegionPanel.LeftWorksTitle = FText::FromString(Province.LeftWorksTitle);
			RegionPanel.LeftWorksText = FText::FromString(Province.LeftWorksText);
			RegionPanel.PocketTitle = FText::FromString(Province.PocketTitle);
			RegionPanel.PocketText = FText::FromString(Province.PocketText);
			RegionPanel.bPocketSpent = Province.bPocketSpent;
			if (Province.SiegeLines.Num() > 0)
			{
				RegionPanel.SiegeTitle = FText::FromString(Province.SiegeBy.IsEmpty() ? FString(TEXT("THE TOWN AS A FORTRESS")) : FString::Printf(TEXT("BESIEGED BY %s"), *Province.SiegeBy.ToUpper()));
				RegionPanel.bSiegeOwn = !Province.SiegeBy.IsEmpty() && Province.bSiegeOwn;
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
				// (The code checker, 17 November.) Ground that is not his home ground. Where he holds it (conquered
				// ground), every order the rules offer is shown, as at home: engineers' jobs, bridges, the railway, what
				// can be wrecked or burnt. Anywhere else, what can be ordered from afar is always shown, and of the rest
				// only what the rules will take now: an enemy's province is not hung with orders nobody can give.
				// (The cabinet's promises have their own page.)
				const FCampaignView& Now = Session->GetView();
				const bool bHeld = Now.RegionHolder.IsValidIndex(RegionId) && Now.RegionHolder[RegionId] == Now.Human;
				RegionPanel.Options.RemoveAll([bHeld](const FProvinceOption& Option)
				{
					if (Option.Action.StartsWith(TEXT("work:promise")))
					{
						return true;
					}
					const bool bFromAfar = Option.Action.StartsWith(TEXT("work:recon")) || Option.Action.StartsWith(TEXT("work:evac")) || Option.Action.StartsWith(TEXT("work:storm"))
						|| Option.Action.StartsWith(TEXT("work:sally")) || Option.Action.StartsWith(TEXT("work:garrison")) || Option.Action.StartsWith(TEXT("work:field"));
					return !bFromAfar && !bHeld && !Option.bEnabled;
				});
			}
			RegionPanel.Forming = Province.Forming;
			RegionPanel.FormingHead = Province.FormingHead;
		}
	}
}
