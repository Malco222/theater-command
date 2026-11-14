#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Campaign/CampaignView.h"
#include "CampaignPlayerController.generated.h"

class ACampaignMapActor;
class ACampaignCameraPawn;
class AArmyPiece;
class FCampaignSession;

/** What the panel shows about the territory the player has clicked. Worked out once, when the selection changes. */
struct FRegionPanelInfo
{
	bool bVisible = false;
	FText Name;
	FText OwnerLine;
	FLinearColor OwnerColor = FLinearColor::Gray;
	FString OwnerTag; // whose flag to show
	FText Details;
	FText Forces;
	// A country at peace with the player that he could go to war with: its tag, and a word about it.
	FString DeclareTag;
	FText DiplomacyLine;
	// What the player can do here, on home ground: raise corps, build works, and what is already forming.
	FText Holdings;
	FText Prize; // what taking the place is worth, or what holding it pays, in a sentence
	FText PrizeTip; // and in detail
	bool bPrizeToTake = false; // it is not the player's yet
	// The town's building plots (the grand campaign), each as what stands on it or "Free plot".
	TArray<FString> Plots;
	bool bPlotsMine = false;
	int32 PlotsFree = 0, PlotsCanBuild = 0;
	FText ProvinceNote;
	FText SiegeTitle, SiegeText; // the town as a fortress, and how its siege stands
	FText PlacesTitle, PlacesText; // the places of the province that are prizes of their own, and scorched earth
	FText DirectiveText; // a directive of the war cabinet that names this province
	TArray<FProvinceOption> Options;
	/** The heading over those: "PROVINCE", or "ORDERS" for a fleet or an air fleet. */
	FText OptionsTitle = FText::FromString(TEXT("PROVINCE"));
	TArray<FOrderLine> Forming;
	int32 Revision = 0;
};

/** One corps of a picked-up army, as a card in the tray. */
struct FTrayCard
{
	int32 CorpsId = INDEX_NONE;
	FText Name;
	FText Kind;
	int32 Divisions = 0;
	float Strength = 1.0f;
	int32 Moves = 0;
	int32 MovesMax = 0;
	bool bPicked = false; // going with the army, or left behind
	bool bCanOrder = false;
	FText Nickname; // empty until the corps has earned one
	FText Grade; // "Veteran, 65"
	FString KindId; // inf, arm, art, mot, cav, mtn, eng, fort: for the picture on each division's card
	FString Ally; // an ally's corps marching with the army: its own country's tag
	bool bLook = false; // a card of an army only being looked at: it cannot be given orders from here
	bool bUnknown = false; // a corps in view that nobody has identified
	int32 Stale = 0; // known from a report this many months old
	float Morale = 1.0f; // 0..1: how well it holds together
	// Where the rules keep morale and rest (the grand campaign): both, with their words and why; and each division.
	bool bMood = false;
	float Rest = 1.0f;
	FString MoraleWord, RestWord, Trend, MoodTip, Service;
	bool bResting = false;
	TArray<FViewDivision> Parts;
	int32 Experience = 0; // 0 to 100
	TArray<FHonourRibbon> Honours;
};

/** One ship of the fleet in hand, as a card. */
struct FShipCard
{
	FString Model; // "Battleship:UK"
	FText Name; // "Battleship"
	FText Role; // what it is for, in a line
	FText Brief; // and in three words, for the face of the card
	int32 Count = 1; // how many ships this card stands for, when the fleet has too many to show each
};

/** The adviser's first steps: shown at the start of a campaign, each ticked as the player does it. */
struct FAdviser
{
	bool bOn = false;
	bool bDone[5] = { false, false, false, false, false }; // an army picked up, sent, a province looked at, an airfield or fleet opened, the month ended
	int32 Revision = 0;
};

/** The newsreel being shown: the month it is of, its stories, and which is on the screen (-1 for the title). */
struct FNewsreelShow
{
	bool bOn = false;
	int32 Kind = 0; // 0 the month's newsreel, in black and white; 1 the opening flight, the situation told in colour; 2 a year's title card
	FYearCard Year; // for a title card
	FString Date;
	TArray<FNewsCard> Cards;
	int32 Index = -1;
	double Until = 0.0; // when the story on the screen gives way to the next
};

/** The army the player has picked up: its general, and its corps as cards. */
struct FArmyTray
{
	bool bVisible = false;
	FText Title;
	FText Subtitle;
	FLinearColor Color = FLinearColor::Gray;
	FString Tag; // whose flag to show
	TArray<FTrayCard> Cards;
	bool bFleet = false; // it is a fleet in hand, not an army: its ships are the cards
	bool bLook = false; // somebody's army being looked at, not one in hand: its corps as cards, and no orders
	TArray<FShipCard> Ships;
	FString FleetStance; // patrol, raid or port: what the fleet is doing now
	FString FleetNext; // patrol, raid or hunt: what it will do in the sea it is sent to next
	bool bFleetGrand = false; // the navy as pieces: it can hunt, escort, shell, split and join
	int32 FleetAircraft = 0; // the aircraft its carriers bring, if it has carriers
	FText FleetWarning; // its heavy ships are thinly screened, or its guns have spoken
	FString Posture; // the standing order the picked corps are under: stand, dig or back; empty if they differ
	bool bByRail = false; // the next order sends them by rail
	bool bBySea = false; // or by sea: to a friendly port, or ashore on an enemy coast
	bool bLean = false; // the picked corps take no drafts
	FText JoinButton; // "ASK 1 SLOVAK CORPS TO JOIN": allied corps beside the army that could march with it. Empty when there are none
	FText PendingButton; // "MARCH TO LODZ": the place clicked once, waiting for the word. Empty when there is none
	FText Hint; // what to do next, in a line
	int32 Revision = 0; // goes up whenever the cards change, so the screen knows to redraw them
};

/** One choice on the opening screen. */
struct FOpeningSide
{
	FText Name;
	FText Blurb;
	FText Aims;
	FLinearColor Color = FLinearColor::Gray;
	FString Tag; // whose flag to show; empty to watch
	bool bWatch = false;
};

/** A day the grand campaign can be taken over on. */
struct FOpeningYear
{
	FText Year;
	FText Title;
	FText Note;
	int32 From = 1;
};

/** A country that can be led in the grand campaign. */
struct FOpeningNation
{
	FString Tag;
	FText Name;
	FText Blurb;
	FText Standing; // "At peace", or who it is at war with
	FText Strength; // "28 corps, 36 territories"
	bool bMajor = false;
	bool bSmall = false; // a small country whose path is its own to choose
	TArray<FString> Abilities; // what the country does in its own way, a line each
};

struct FOpeningGroup
{
	FText Title;
	TArray<FOpeningSide> Sides;
};

/** The opening screen: where a campaign can begin and as whom, and the campaign saved on this computer if there is one. */
struct FOpening
{
	bool bVisible = false;
	bool bCanReturn = false; // a campaign is under way behind the screen, so it can simply be closed

	// The grand campaign: any country, taken over on one of a few days, and free to make its own history after.
	int32 Tab = 0; // 0: the grand campaign, 1: the historical scenarios, 2: the options
	TArray<FOpeningYear> Years;
	int32 YearIndex = 0;
	TArray<FOpeningNation> Nations;
	int32 NationIndex = INDEX_NONE; // the country chosen, if one is
	FText GrandNote; // a word about who is missing from the list, if anyone is
	// How hard the war is to be: the levels, easiest first, and the one chosen.
	TArray<FGrandLevel> Levels;
	int32 LevelIndex = INDEX_NONE;
	bool bWorking = false; // history is being played up to the day chosen; the list of countries follows

	// The scenarios: single campaigns in which the other nations keep to history's plan.
	TArray<FOpeningGroup> Groups;
	FText SavedLine; // empty when there is nothing to continue
	int32 Revision = 0;
};

/** One letter of a name lettered across the map, as the screen should draw it this frame. */
struct FMapLetter
{
	TCHAR Char = TEXT(' ');
	FVector2D At = FVector2D::ZeroVector; // the middle of the letter on the screen
	FVector2D Along = FVector2D::ZeroVector; // on the screen: the way the word runs, as long as a capital is high
	FVector2D Up = FVector2D::ZeroVector; // and the way the letter's top points, the same length on the map
	float Alpha = 1.0f;
	bool bSea = false;
	FString Whole; // a province's name, written whole here instead of a single letter
};

/** A town's name plate as the screen should draw it this frame. */
struct FTownPlate
{
	FVector2D At = FVector2D::ZeroVector; // on the screen, in its own points: the middle of the plate's top edge
	FString Name;
	FString Line; // empty from far off
	FLinearColor Colour = FLinearColor::Gray; // of whoever holds the place
	bool bCapital = false;
	bool bFar = false; // seen from far off: a small plate
	bool bAirfield = false;
	bool bHarbour = false; // ships in harbour: an anchor and how many // not a town at all: the badge of an airfield with air groups based on it (Name is how many)
	bool bSeal = false; // not a town either: the seal on the town a directive of the war cabinet names (Name is the months left)
	bool bVow = false; // with bSeal: it is the player's own promise to the cabinet, in wax red, not the cabinet's directive in brass
	int32 Rank = 0;
};

/** The bar of a town's building plots, shown when one of the player's provinces is picked and nothing is in hand. */
struct FTownBar
{
	int32 Region = INDEX_NONE;
	FString Tag; // whose town it is
	bool bMine = false;
	FText Title, Sub;
	TArray<FTownPlot> Plots;
	TArray<FTownOffer> Offers;
	int32 Picked = INDEX_NONE; // the plot whose choices are laid out above the bar
	int32 Revision = 0;
};

/** One of the game's standing choices, as the options page shows it. They are remembered from one day to the next. */
struct FGameOption
{
	FString Id;
	FText Name;
	FText Blurb;
	FText State; // what its button reads
	bool bOn = false; // lit
};

/** The question put to the player before he goes to war with a country he is at peace with. */
struct FWarPrompt
{
	bool bVisible = false;
	FString Tag; // the country war would be declared on
	FString Name; // its name as it reads in a sentence
	int32 Region = INDEX_NONE; // where the army in hand was being sent, to carry on there once war is declared
	FText Title;
	FText Body;
	bool bFromOffice = false; // asked from the foreign office: go back there afterwards
	// The same question is put before a landing from the sea: then this is true, and the button says so.
	bool bLanding = false;
	FText Confirm = FText::FromString(TEXT("DECLARE WAR"));
};

/** The two armies drawn up against each other before an attack and, once it is fought, what the battle came to. */
struct FFaceOff
{
	bool bVisible = false;
	bool bResolved = false;
	int32 Region = INDEX_NONE;
	FText Title;
	FText OurName;
	FText TheirName;
	FLinearColor OurColor = FLinearColor::Gray;
	FLinearColor TheirColor = FLinearColor::Gray;
	FString OurTag; // whose flags to show
	FString TheirTag;
	FText OurGeneral;
	FText TheirGeneral;
	FText OurStrength;
	FText TheirStrength;
	FText OurUnits;
	FText TheirUnits;
	float Balance = 0.5f; // the attackers' share of the strength on the field
	FText OddsLine;
	FText Prize; // what taking the place is worth
	FText Mass; // where more corps stand in reach than were picked: what sending everyone would come to
	TArray<int32> MassIds; // ...and the corps that would go
	FText Ticket;
	FText ResultTitle;
	FText ResultDetail;
};

/**
 * The player's hands on the campaign table: moving the view, pointing at
 * territories, picking up armies and giving them their orders.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACampaignPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	const FRegionPanelInfo& GetRegionPanel() const { return RegionPanel; }
	const FArmyTray& GetArmyTray() const { return Tray; }
	const FFaceOff& GetFaceOff() const { return FaceOff; }
	const FOpening& GetOpening() const { return Opening; }
	const FWarPrompt& GetWarPrompt() const { return WarPrompt; }

	/**
	 * Asks the player whether to declare war on the country that holds a territory. Returns false, asking
	 * nothing, if that country is not one he is at peace with and could go to war with this month.
	 */
	bool OfferWar(int32 RegionId, bool bMarching = true);
	/** The same question, about a country by its tag. Region is where the army in hand goes on to afterwards, or INDEX_NONE. */
	bool OfferWarOn(const FString& Tag, int32 Region);

	/** The same question about the country whose territory is selected, asked from the territory panel. */
	void OfferWarOnSelected();

	/** Declares the war that was asked about, and sends the army in hand on if it was on its way there. */
	void ConfirmWar();
	void CloseWarPrompt();

	/** A page shown over the table: a unit's service record or the roll of honour. Revision goes up when it changes. */
	const FDocumentPage& GetDocument() const { return Document; }
	int32 GetDocumentRevision() const { return DocumentRevision; }
	void OpenRecord(int32 CorpsId);
	void OpenRoll();
	void CloseDocument();

	/** Starts the scenario chosen on the opening screen. */
	void BeginCampaign(int32 GroupIndex, int32 SideIndex);

	/** The opening screen's two pages: 0 is the grand campaign, 1 the historical scenarios. */
	void ShowOpeningTab(int32 Tab);
	/** The options page: the game's standing choices, and a press on one of them. */
	TArray<FGameOption> GetOptions() const;
	void PressOption(const FString& Id);

	/**
	 * Chooses the day the grand campaign is taken over on, and works out who can be led from it. For a later
	 * year that means playing history up to it, which takes a few seconds: the screen says so first, unless
	 * bAtOnce asks for the answer before returning (scripts do).
	 */
	void PickGrandYear(int32 Index, bool bAtOnce = false);

	/** Chooses a country for the grand campaign, by its place in the list or by its tag. */
	void PickGrandNation(int32 Index);
	void PickGrandNation(const FString& Tag);

	/** Starts the grand campaign as the country chosen, or with nobody leading. */
	void BeginGrandCampaign(bool bWatch);

	/** Picks up the campaign saved on this computer. */
	void ContinueCampaign();

	/** Saves the campaign and shows the opening screen over it. */
	void OpenMenu();

	/** Goes back from the opening screen to the campaign under way. */
	void CloseMenu();

	/** The month's news, shown for a while after the month turns. */
	FText GetDispatches() const;

	/** True while the other nations' months are being played out in front of the player. */
	bool IsWatching() const { return bWatching; }
	ACampaignMapActor* GetMap() const { return Map.Get(); }
	FCampaignSession* GetSession() const;

	FText GetDateText() const;
	FText GetRulesNote() const;
	/** A line of news for the player, shown for a few seconds. Empty when there is none. */
	FText GetNotice() const;

	/** Selects a territory, or none with INDEX_NONE. */
	void SelectRegion(int32 RegionId);

	/** Picks up the player's army in a region, if there is one that can be given orders. */
	bool PickUpArmyIn(int32 RegionId);
	void PutDownArmy();

	/** Leaves a corps of the picked-up army behind, or takes it along again. */
	void ToggleCorps(int32 CorpsId);

	/** The standing order for the army in hand on the day the enemy comes: "stand", "dig" or "back". */
	void SetStance(const FString& Posture);

	/** Switches between marching and going by rail for the army in hand. */
	void ToggleRail();

	/** Orders something in the selected territory: a kind of corps to raise, or "fort". */
	void ProvinceAction(const FString& Action);

	/** Switches the army in hand between marching and going by sea. */
	void ToggleSea();

	/** On the opening screen: chooses how hard the grand campaign is to be. */
	void PickLevel(int32 Index);

	/** Shows a fleet or an air fleet in the panel, with the orders it can be given. */
	void ShowForce(bool bFleet, int32 Id);

	/** Takes back an order placed this month. */
	void CancelOrder(int32 OrderId);

	/** The treasury in a line, for the date plate. */
	FText GetTreasuryText() const;

	/** What comes in, goes out and is left each month, in a line under it; empty in the scenarios, where nothing goes out. */
	FText GetBudgetText() const;

	/** True when the treasury is in debt, so the plate can show it in red. */
	bool IsInDebt() const;

	/** The ledger: the player's books, shown over the table. Revision goes up when they change. */
	const FLedger& GetLedger() const { return Ledger; }
	bool IsLedgerOpen() const { return bLedgerOpen; }
	FString GetLedgerNationTag() const;
	void OpenLedger();
	void CloseLedger();

	/** How to play, as a page over the table. */
	void OpenGuide();

	/** A room the rules set out themselves: the secret service, the last act. */
	const FOfficePage& GetPage() const { return Page; }
	bool IsPageOpen() const { return bPageOpen; }
	void OpenPage(const FString& Id);
	void ClosePage();
	void PressPage(const FString& Action);
	/** The rooms open to the player now, for the buttons that lead to them: ids and labels, in step. */
	void GetPageLinks(TArray<FString>& OutIds, TArray<FString>& OutLabels) const;

	/** The generals: who commands what, who is waiting, and the corps that have nobody. */
	const FGeneralStaff& GetGenerals() const { return Staff; }
	bool IsGeneralsOpen() const { return bGeneralsOpen; }
	void OpenGenerals();
	void CloseGenerals();
	void AssignGeneral(int32 GeneralId, int32 CorpsId);
	void PromoteOfficer(int32 CorpsId);
	void RelieveGeneral(int32 GeneralId);
	/** Dismisses a general for good: asked twice, since it cannot be undone. Says which general is being asked about. */
	void SackGeneral(int32 GeneralId);
	int32 GetSackAsked() const { return SackAsked; }
	/** The army in hand takes drafts, or keeps its veterans together and takes none. */
	void ToggleDrafts();
	/** On the eve of war: the army is placed as the staff advises. */
	void PlaceByPlan();
	bool HasEvePlan() const;
	/** The honours won, and what can be made of each at home. */
	void OpenHeroes();
	void SpendSkill(int32 GeneralId, const FString& Skill);

	/** The design offices: the teams at work on each branch of the armed forces. */
	const FResearch& GetResearch() const { return Research; }
	bool IsResearchOpen() const { return bResearchOpen; }
	void OpenResearch();
	void CloseResearch();
	void SetResearchTeams(const FString& Field, int32 Teams);
	void StartTech(const FString& Id);
	void StopTech(const FString& Id);
	/** Research teams of the player's with nothing to do, for the badge on the button. */
	int32 GetIdleTeams() const;
	/** What is still worth doing before the month is ended: the line at this place in the list, or empty. And a press on it. */
	FString GetToDoText(int32 Index) const;
	FString GetToDoTip(int32 Index) const;
	void PressToDo(int32 Index);
	/** A line about the province under the pointer, for a tip that follows the mouse; empty when a screen is open. */
	FText GetHoverTip() const { return HoverTip; }
	/** Where the pointer is, in the window's own pixels. */
	FVector2D GetPointer() const { return StagedHover != INDEX_NONE ? StagedPointer : LastMouse; }
	// For the photographer, who has no mouse: the tip as it would read with the pointer resting on this region.
	void StageHover(int32 RegionId) { StagedHover = RegionId; }
	/** Shows the model of one of the corps in hand large and turning beside the cards. */
	void ShowCorpsModel(int32 CorpsId);
	FText GetShowTitle() const { return ShowTitle; }
	/** The great moment being told now, or null: a nation eliminated or liberated, on a screen of its own. */
	const FAnnouncement* GetAnnouncement() const { return Announcements.Num() > 0 ? &Announcements[0] : nullptr; }
	void DismissAnnouncement();
	/** Tells a great moment on a screen of its own, after any already waiting. */
	void Announce(const FAnnouncement& Told) { Announcements.Add(Told); }
	/** Which part of the province panel is open: 0 the province, 1 what can be raised there, 2 the works. */
	int32 GetProvinceTab() const { return ProvinceTab; }
	void SetProvinceTab(int32 Tab) { ProvinceTab = Tab; RegionPanel.Revision = ++PanelRevision; }
	/** Opens the town plan of the province picked on the map: its plots and what could be built on them. */
	void OpenTownPlan();
	/** The town's plots as a bar of pictures; whether it is to be shown; a click on a plot; and an order from its choices. */
	const FTownBar& GetTownBar() const { return TownBar; }
	bool IsTownBarShown() const;
	/** Where the armies' colours and their strength plates are on the screen, in its own pixels: the towns' name plates step aside from them. */
	void GetColoursOnScreen(TArray<FBox2D>& Out) const;
	/** Whether the reminder of the keys stands open: through a new player's first month, while the ? key is held, or kept open by a click on its mark. */
	bool IsHelpShown() const;
	/** A click on the mark: keep the reminder open, or fold it away (and with it the first month's showing). */
	void ToggleHelp();
	void PickTownPlot(int32 Index);
	void OrderTownBuilding(const FString& Line);
	/** The small map in the corner: its picture, the marks on it, and a jump to a place on it. */
	UTexture2D* GetMinimapTexture() const;
	/** Armies as (x, y, whose) with x and y from 0 to 1 and whose 0 for the player's, 1 for a friend's, 2 for an enemy's, 3 for others; and the part of the map on screen. */
	void GetMinimapMarks(TArray<FVector>& OutDots, FBox2D& OutLooking) const;
	/** The name plates of the towns in view, the ones that matter most first. */
	void GetTownPlates(TArray<FTownPlate>& Out) const;
	/** The names of the countries and the seas, letter by letter, lying on the map. */
	void GetMapLetters(TArray<FMapLetter>& Out) const;
	/** How far into a moonlit night the table is, 0 to 1: the lettering on the map dims with it. */
	float GetMoonlight() const;
	/** How far the table has gone over to the staff's paper map (the top of the zoom): 0 to 1. Names are lettered on it in ink. */
	float GetPaper() const;
	/** True where whoever stands a month digs in unasked (the grand campaign): the tray then offers stand, or fall back. */
	bool IsAutoDig() const;
	/** The reserve of men, where the rules keep one (the grand campaign); null otherwise. */
	const FViewStock* GetMen() const;
	void JumpToMapShare(const FVector2D& Share);
	bool IsMapShown() const;
	/** A fleet in hand: picked up like an army, sent to a sea with a right-click, its ships shown as cards. */
	bool PickUpFleet(int32 FleetId);
	void PutDownFleet();
	bool HasFleetInHand() const { return FleetInHand != INDEX_NONE; }
	/** What the fleet in hand does where it is sent: "patrol" or "raid". Changes what it is doing now if it is at sea. */
	void SetFleetStance(const FString& Stance);
	void FleetToHarbour();
	const FFleetHand& GetFleetHand() const { return FleetHand; }
	/** Opens the room with everything the fleet in hand could be told to do: seas, lanes to escort, coasts to shell, splitting and joining. */
	void OpenFleetOrders();
	/** The room of the aircraft the fleet in hand carries: search a sea, strike a fleet, raid a harbour. */
	void OpenFleetWings();
	/** Moves the fleet in hand to a new home port. */
	void FleetToNewBase(int32 RegionId);
	/** Sends the fleet in hand to a sea, to do there what it has been told (patrol or raid). */
	void OrderFleetTo(int32 Zone);
	void ShowShipModel(const FString& Model, const FText& Name);
	/** Picks up the next of the player's armies that can still move this month, and goes to it. */
	void NextArmy();
	/** How many of the player's armies can still move this month: the reminder beside the button that ends it. */
	int32 ArmiesYetToMove() const;
	/** The list of everything the player commands, each with a way to go to it (L). */
	void OpenForces();
	/** The reserve of men as a sum, and the player's say over the call-up (version 62). */
	void OpenMen() { OpenPage(TEXT("men")); }
	/** Looks at an army that is not in hand (an enemy's, an ally's, or one of the player's that cannot move): its corps are dealt as cards, as far as they are known. */
	void LookAtArmy(int32 RegionId, const FString& Nation);
	void StopLooking();
	/** Joint armies: asks the allied corps beside the army in hand to march with it; sends one of them home. */
	void AskAlliesToJoin();
	void SendBackCorps(int32 CorpsId);
	/** The nation the player leads; empty when nobody is led. */
	FString GetPlayerTag() const;
	/** Says in the notice line why something pressed cannot be done. */
	void SayWhy(const FString& Why);
	/** The weather in the east this month, for the date plate; empty in clear weather. */
	FText GetWeatherText() const;
	/** The adviser's first steps, and the way to send him away. */
	const FAdviser& GetAdviser() const { return Adviser; }
	void DismissAdviser() { Adviser.bOn = false; ++Adviser.Revision; }
	void ShowAdviser() { Adviser.bOn = true; ++Adviser.Revision; }
	/** The newsreel: the month's great stories flown over in black and white, each read out. Space skips it; K turns newsreels off or on. */
	const FNewsreelShow& GetNewsreel() const { return Newsreel; }
	void PlayNewsreel(const FString& Date, const TArray<FNewsCard>& Cards, int32 Kind = 0);
	/** The opening flight: the situation as the campaign begins, told stop by stop over the map. Space skips it. */
	void PlayBriefing();
	/** The title card that opens a year. A click or Space goes on. */
	void PlayYearCard(const FYearCard& Year);
	const FYearCard& GetLastYear() const { return LastYear; }
	FText GetArmySaid() const { return FPlatformTime::Seconds() < ArmySaidUntil ? ArmySaid : FText::GetEmpty(); }
	void NextNewsCard();
	void EndNewsreel();
	void ToggleNewsreels();
	/** Day and night in the room: running, held at day, or held at night (T). */
	void ToggleDayNight();
	/** Begins a moment over a province: the eye goes down to it, stays about three seconds and is handed back. Any key or click ends it sooner. */
	void StartMoment(int32 RegionId);
	void EndMoment();
	/** The F key: puts the frame away for a clean picture of the table, or brings it back. */
	void ToggleFrame();
	bool IsFrameHidden() const;
	bool IsFrameHintShown() const { return FPlatformTime::Seconds() < FrameHintUntil; }
	/** The wealth view of the map: provinces tinted by what they pay (Y). */
	void ToggleWealth();
	bool IsWealthShown() const;
	/** What winning means: the campaign's objectives, each thing they ask for ticked or not (O). */
	void OpenGoals();
	/** What every nation did last month, each with a way to go and look (R). */
	void OpenReports();
	/** Looks at the player's capital (Home, or C). */
	void GoToCapital();
	/** While the month is replayed: the nations whose moves are shown, in order, and which is on the table now. */
	void GetWatchOrder(TArray<FString>& OutTags, int32& OutNow) const;
	/** Whether every nation's move is watched at the end of a month, or only what touches the player. */
	void ToggleWatchAll();
	/** The player's oil or supplies, or null where his nation keeps no stocks (a small country, a scenario, nobody led). */
	const FViewStock* GetStock(bool bOil) const;
	void FoundResearchBureau();

	/** The foreign office: the powers at war, the countries at peace, courting them and declaring war. */
	const FDiplomacy& GetDiplomacy() const { return Diplomacy; }
	bool IsDiplomacyOpen() const { return bDiplomacyOpen; }
	void OpenDiplomacy();
	void CloseDiplomacy();
	void CourtNation(const FString& Tag);
	/** A small country's path. The first press asks; the second, on the same path, takes it. Founding the League names the partner. */
	void PressPath(const FString& Id, const FString& Partner = FString());
	const FString& GetPathAsked() const { return PathAsked; }
	void InviteToLeague(const FString& Tag);
	/** Asked from the foreign office: the question about declaring war on a country, with no army on its way. */
	void AskWarOn(const FString& Tag);

	/** The arsenal: everything that can be ordered, with the territory that is selected in mind. */
	const FArsenal& GetArsenal() const { return Arsenal; }
	bool IsArsenalOpen() const { return bArsenalOpen; }
	void OpenArsenal();
	void CloseArsenal();
	void OrderFromArsenal(const FString& Key);

	/** Closes the ledger and looks at one of its provinces on the table. */
	void ShowProvince(int32 RegionId);

	/** Shows the road the picked-up army would take to a region. */
	void ShowRouteTo(int32 RegionId);

	/**
	 * Orders the picked-up army to a region. CarriedFrom is where the player's hand set the piece down, when it
	 * was dragged there: the piece then settles straight onto its place.
	 */
	void OrderTo(int32 RegionId, const FVector* CarriedFrom = nullptr);

	/** A click on a territory, as the mouse makes it: for scripts. */
	void ClickRegion(int32 RegionId) { ClickOnTable(nullptr, RegionId); }

	/** Sends the army in hand to the place that was clicked once and is waiting for the word. */
	void ConfirmPending();

	/** Sends the attack shown on the face-off in. */
	void ConfirmAttack();
	/** On the attack card: pick every corps that can reach the place, and show the attack again as it would then be. */
	void SendEveryone();

	/** Calls the attack off, or closes the result of one that has been fought. */
	void CloseFaceOff();

	void EndMonth();

	/** Ends the month and plays the other nations' months out one at a time, even with a script driving. */
	void EndMonthAndWatch();

	/** Cuts short the watching of the other nations' month. */
	void SkipWatching();

	/**
	 * The hand on a flag, as three steps a script can take too: take hold of the player's army in a region,
	 * hang it over another, and let go of it there.
	 */
	bool HoldArmyIn(int32 RegionId);
	void CarryHeldOver(int32 RegionId);
	void DropHeldOver(int32 RegionId);

	/** While a script is driving (the development photographer), the pointer does not light or route anything. */
	void SetScripted(bool bInScripted) { bScripted = bInScripted; }

private:
	void WheelUp();
	void WheelDown();
	void Escape();

	/** Where on the table the pointer is, allowing for the height of the ground. */
	bool PointerOnTable(FVector& OutWorld) const;

	void RefreshRegionPanel(int32 RegionId);
	void RefreshOrders();
	void Notify(const FString& Text, double Seconds = 5.0);
	void OpenFaceOff(int32 RegionId);
	void ShowWar(const FCampaignView& View, const TArray<FCorpsMove>& Marches, const FVector* CarriedFrom = nullptr);
	bool CanPickUp(const AArmyPiece* Piece) const;
	void ClickOnTable(AArmyPiece* Piece, int32 RegionId);
	bool Hold(AArmyPiece* Piece);
	void SetPending(int32 RegionId);
	void DropArmy(int32 RegionId, bool bOnTable);
	void WatchNextStep();
	void FinishMonth();
	void SpaceBar();
	void ToggleMusic();
	void ToggleEffects();
	int32 ShownForce = INDEX_NONE;
	bool bShownFleet = false;

	void RefreshOpening(bool bShow);
	void EnterCampaign();
	class ACampaignGameMode* GetMode() const;
	const FReachRegion* FindReach(int32 RegionId) const;

	TWeakObjectPtr<ACampaignMapActor> Map;
	FRegionPanelInfo RegionPanel;
	FArmyTray Tray;

	/** The army in hand: the region it stands in and the corps that will go when it is ordered. */
	int32 ArmyRegion = INDEX_NONE;
	TArray<int32> PickedCorps;
	TArray<int32> PickedBeforeMass; // the corps that were in hand before "send everyone" was pressed on the attack card
	TArray<FReachRegion> Reach;
	int32 RouteTarget = INDEX_NONE;
	int32 PendingTarget = INDEX_NONE; // ground clicked once with an army in hand: a second click, or the button, sends it there
	bool bHeldWasInHand = false; // the flag pressed was already the army in hand, so a plain click on it puts it down
	TWeakObjectPtr<AArmyPiece> RingedPiece;
	TWeakObjectPtr<AArmyPiece> HoverPiece; // the piece the pointer rests on, if it can be picked up
	TWeakObjectPtr<AArmyPiece> HeldPiece; // the piece the left button went down on

	FFaceOff FaceOff;
	FWarPrompt WarPrompt;
	FLedger Ledger;
	bool bLedgerOpen = false;
	FDiplomacy Diplomacy;
	bool bDiplomacyOpen = false;
	FResearch Research;
	bool bResearchOpen = false;
	FGeneralStaff Staff;
	bool bGeneralsOpen = false;
	FOfficePage Page;
	bool bPageOpen = false;
	void RefreshGenerals(bool bArmiesChanged);
	FString PathAsked; // the path waiting for the player's yes, with its partner: "found:DEN"
	FArsenal Arsenal;
	bool bArsenalOpen = false;
	FOpening Opening;
	FDocumentPage Document;
	int32 DocumentRevision = 0;
	TArray<FStartGroup> StartGroups;
	TArray<FGrandNation> GrandNations;
	int32 GrandSeed = 1939; // the dice for the campaign about to be chosen
	int32 WorkingFrames = 0;
	void LoadGrandNations();
	static int32 NewSeed();

	FString NoticeText;
	double NoticeUntil = 0.0;
	FString DispatchText;
	double DispatchUntil = 0.0;

	// The other nations' month, being played out one nation at a time.
	bool bWatching = false;
	FMonthResult Month;
	int32 WatchIndex = 0;
	double WatchNext = 0.0;
	FVector WatchReturnFocus = FVector::ZeroVector;
	float WatchReturnZoom = 0.5f;

	bool bLeftHeld = false;
	bool bDragging = false;
	bool bRightHeld = false;
	bool bTurning = false;
	bool bHaveLastMouse = false;
	FText HoverTip;
	TArray<FAnnouncement> Announcements;
	FText ShowTitle;
	int32 ShowCorps = INDEX_NONE;
	bool bToldOfEvening = false; // the first dusk has been explained
	int32 LastClickRegion = INDEX_NONE; // for a double click: where the last plain click fell, and when
	double LastClickAt = 0.0;
	bool bPointerOverPanel = false; // the pointer is on one of the screen's panels, not on the map
	bool bButtonDown = false; // the mouse was down at the last look
	const void* HoveredButton = nullptr; // the button the pointer is on, to tick once as it crosses onto one
	double BattleMusicUntil = 0.0; // the march plays until then
	int32 SackAsked = INDEX_NONE; // the general whose dismissal has been asked once
	FAdviser Adviser;
	void AdviserSaw(int32 Step);
	bool bBusyWithScreens() const { return bWatching || Newsreel.bOn || Opening.bVisible || Announcements.Num() > 0 || bScripted; }
	FNewsreelShow Newsreel;
	bool bNewsreels = true; // K turns them off
	// The standing choices, kept in a file of their own so they outlast the campaign.
	void LoadOptions();
	void SaveOptions() const;
	static FString OptionsFile();
	bool bOptionsRead = false;
	bool bNarrator = true; // the voice that reads the newsreels
	bool bBriefings = true; // the opening flight when a campaign begins, and the title card of each year
	bool bFrameHidden = false; // the frame is put away for a clean picture (the F key)
	double FrameHintUntil = 0.0;
	bool bMoments = true; // the eye drops to the front for a few seconds when a capital falls or a great battle is decided
	bool bMomentDue = false; // such a thing has just happened: the moment begins when its result has been read
	bool bMomentOn = false;
	double MomentUntil = 0.0;
	double MomentGlideUntil = 0.0; // the eye keeps its slow travel until it is back where it was
	FVector MomentHome = FVector::ZeroVector; // where the eye was looking, and from how high, before the moment
	float MomentHomeZoom = 0.5f;
	bool bHelpSeen = false; // the player has ended a month, or folded the keys away: they are not shown unasked again
	bool bHelpPinned = false; // the reminder of the keys is kept open by a click on its mark
	bool bFreshCampaign = false; // a campaign has just been begun, not continued
	FYearCard LastYear; // the last year's card, to show again
	// What an army answers as it is picked up, in its own language, for a moment.
	FText ArmySaid;
	double ArmySaidUntil = 0.0;
	double BriefingAt = 0.0; // when the opening flight is to begin, a moment after the table is laid
	// A newsreel that waits for a year's title card to go by.
	FString WaitingReelDate;
	TArray<FNewsCard> WaitingReel;
	bool bAdviserWanted = true; // the staff officer of the opening months
	bool bEdgeScroll = true; // the pointer resting at the window's edge slides the table
	float EdgeRest = 0.0f; // how long it has rested there
	TArray<FAnnouncement> HeldAnnouncements; // waiting for the newsreel to end
	FVector ReelReturnFocus = FVector::ZeroVector;
	float ReelReturnZoom = 0.5f, ReelReturnYaw = 0.0f;
	FProcHandle Narrator; // the voice reading the story
	void Narrate(const FString& Words);
	void HushNarrator();
	// The game's voice (the computer's own, for a start): the newsreels and the opening flight read aloud, the
	// title card of a year, the adviser's steps, and an army's answer in its own language when it is picked up.
	// One line is spoken at a time; the others wait their turn.
	struct FVoiceLine
	{
		FString Words, Voice;
		int32 Rate = 180;
	};
	TArray<FVoiceLine> VoiceQueue;
	int32 VoiceCount = 0;
	double AnsweredAt = 0.0; // when an army last answered aloud: they do not chatter
	bool bAdviserSpoke = false; // the adviser has said his first word in this campaign
	/** Cut: 0 waits its turn; 1 is said now and the rest wait behind it; 2 is said now and the rest are forgotten. */
	void Speak(const FString& Words, const FString& Voice, int32 Rate, int32 Cut);
	void PumpVoice();
	void StopSpeaking();
	bool IsSpeaking();
	void SpeakAdviser();
	int32 ProvinceTab = 0;
	int32 FleetInHand = INDEX_NONE; // the fleet picked up, by its id
	FString FleetNext = TEXT("patrol");
	mutable FVector2D SeaAskedAt = FVector2D(-1.0e6, -1.0e6); // where the rules were last asked which sea lies under the pointer
	mutable int32 SeaAsked = INDEX_NONE;
	int32 LookRegion = INDEX_NONE; // where the army being looked at stands, and whose it is
	FString LookNation;
	void LayOutLook();
	FFleetHand FleetHand; // the fleet in hand as the rules describe it
	int32 FleetHoverSea = INDEX_NONE; // the sea the pointer is nearest, while a fleet is in hand
	FVector2D PointerMap = FVector2D::ZeroVector; // where the pointer is on the map
	bool bPointerOnWater = false;
	void LayOutFleet();
	int32 SeaNear(const FVector2D& MapPoint) const;
	int32 PointedField = INDEX_NONE; // the province whose airfield the pointer is over
	int32 HoverTipField = INDEX_NONE;
	int32 StagedHover = INDEX_NONE;
	FVector2D StagedPointer = FVector2D::ZeroVector;
	int32 HoverTipHand = INDEX_NONE; // what was in hand, and how it was to travel, when the tip was written
	int32 HoverTipRegion = INDEX_NONE;
	bool bWatchAll = true; // watch every nation's move at the month's end; V switches to only what touches the player
	TMap<int32, int32> WatchStood; // where each corps stood at the last step watched, to see who has marched
	void OpenScreen1();
	void OpenScreen2();
	void OpenScreen3();
	void OpenScreen4();
	void OpenScreen5();
	bool bScripted = false;
	bool bAlwaysWatch = false;
	bool bByRail = false;
	bool bBySea = false;
	int32 PanelRevision = 0;
	FTownBar TownBar;
	int32 SelectedRegion = INDEX_NONE;
	int32 SizedForWidth = 0;
	int32 SizedForHeight = 0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D RightPressPosition = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
