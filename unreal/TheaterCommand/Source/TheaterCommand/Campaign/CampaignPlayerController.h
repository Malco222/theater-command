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
	// What the player can do here, on home ground: raise corps, build works, and what is already forming.
	FText Holdings;
	FText ProvinceNote;
	TArray<FProvinceOption> Options;
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
	TArray<FHonourRibbon> Honours;
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
	FString Posture; // the standing order the picked corps are under: stand, dig or back; empty if they differ
	bool bByRail = false; // the next order sends them by rail
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
	bool bWorking = false; // history is being played up to the day chosen; the list of countries follows

	// The scenarios: single campaigns in which the other nations keep to history's plan.
	TArray<FOpeningGroup> Groups;
	FText SavedLine; // empty when there is nothing to continue
	int32 Revision = 0;
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

	/** Takes back an order placed this month. */
	void CancelOrder(int32 OrderId);

	/** The treasury in a line, for the date plate. */
	FText GetTreasuryText() const;

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
	bool bScripted = false;
	bool bAlwaysWatch = false;
	bool bByRail = false;
	int32 PanelRevision = 0;
	int32 SizedForWidth = 0;
	int32 SizedForHeight = 0;
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D RightPressPosition = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
