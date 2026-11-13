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
	FText ProvinceNote;
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
	float Morale = 1.0f; // 0..1: how well it holds together
	int32 Experience = 0; // 0 to 100
	TArray<FHonourRibbon> Honours;
};

/** One ship of the fleet in hand, as a card. */
struct FShipCard
{
	FString Model; // "Battleship:UK"
	FText Name; // "Battleship"
	FText Role; // what it is for, in a line
	int32 Count = 1; // how many ships this card stands for, when the fleet has too many to show each
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
	TArray<FShipCard> Ships;
	FString FleetStance; // patrol, raid or port: what the fleet is doing now
	FString FleetNext; // patrol or raid: what it will do in the sea it is sent to next
	FString Posture; // the standing order the picked corps are under: stand, dig or back; empty if they differ
	bool bByRail = false; // the next order sends them by rail
	bool bBySea = false; // or by sea: to a friendly port, or ashore on an enemy coast
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
	int32 Tab = 0; // 0: the grand campaign, 1: the historical scenarios
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
	/** A line about the province under the pointer, for a tip that follows the mouse; empty when a screen is open. */
	FText GetHoverTip() const { return HoverTip; }
	/** Where the pointer is, in the window's own pixels. */
	FVector2D GetPointer() const { return LastMouse; }
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
	/** The small map in the corner: its picture, the marks on it, and a jump to a place on it. */
	UTexture2D* GetMinimapTexture() const;
	/** Armies as (x, y, whose) with x and y from 0 to 1 and whose 0 for the player's, 1 for a friend's, 2 for an enemy's, 3 for others; and the part of the map on screen. */
	void GetMinimapMarks(TArray<FVector>& OutDots, FBox2D& OutLooking) const;
	void JumpToMapShare(const FVector2D& Share);
	bool IsMapShown() const;
	/** A fleet in hand: picked up like an army, sent to a sea with a right-click, its ships shown as cards. */
	bool PickUpFleet(int32 FleetId);
	void PutDownFleet();
	bool HasFleetInHand() const { return FleetInHand != INDEX_NONE; }
	/** What the fleet in hand does where it is sent: "patrol" or "raid". Changes what it is doing now if it is at sea. */
	void SetFleetStance(const FString& Stance);
	void FleetToHarbour();
	/** Sends the fleet in hand to a sea, to do there what it has been told (patrol or raid). */
	void OrderFleetTo(int32 Zone);
	void ShowShipModel(const FString& Model, const FText& Name);
	/** Picks up the next of the player's armies that can still move this month, and goes to it. */
	void NextArmy();
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
	int32 ProvinceTab = 0;
	int32 FleetInHand = INDEX_NONE; // the fleet picked up, by its id
	FString FleetNext = TEXT("patrol");
	int32 FleetHoverSea = INDEX_NONE; // the sea the pointer is nearest, while a fleet is in hand
	FVector2D PointerMap = FVector2D::ZeroVector; // where the pointer is on the map
	bool bPointerOnWater = false;
	void LayOutFleet();
	int32 SeaNear(const FVector2D& MapPoint) const;
	int32 PointedField = INDEX_NONE; // the province whose airfield the pointer is over
	int32 HoverTipField = INDEX_NONE;
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
	int32 SelectedRegion = INDEX_NONE;
	int32 SizedForWidth = 0;
	int32 SizedForHeight = 0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D RightPressPosition = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
