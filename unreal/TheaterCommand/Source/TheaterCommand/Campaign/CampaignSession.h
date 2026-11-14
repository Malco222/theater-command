#pragma once

#include "CoreMinimal.h"
#include "Campaign/CampaignView.h"

class FRulesHost;
class FJsonObject;

/**
 * One campaign in progress. It owns the rules engine, asks it questions, gives
 * it the player's orders, and keeps the latest picture of the war for
 * everything else to draw.
 */
class FCampaignSession
{
public:
	FCampaignSession();
	~FCampaignSession();

	/** Starts the rules engine and loads the rules. No campaign is under way yet. */
	bool Prepare(FString& OutError);
	bool IsReady() const { return bReady; }

	/** The points in the war a campaign can begin at. */
	TArray<FStartGroup> Starts();

	/** The days the grand campaign can be taken over on. Empty with rules that have no grand campaign. */
	const TArray<FGrandYear>& GrandYears() const { return Years; }

	/** The levels of difficulty a grand campaign can be played at, easiest first, and the one it starts on unless another is chosen. */
	const TArray<FGrandLevel>& GrandLevels() const { return Levels; }
	const FString& FairLevel() const { return DefaultLevel; }
	/** (rules version 66) How deep a grand campaign can be played, lightest first, and the depth offered first. */
	const TArray<FGrandLevel>& GrandDepths() const { return Depths; }
	const FString& FirstDepth() const { return DefaultDepth; }

	/** Who can be led in the grand campaign from a month. History is played up to it the first time this is asked. */
	TArray<FGrandNation> GrandNations(int32 From, int32 Seed);

	/** Starts a new campaign. Human is the nation the player leads, or empty to watch. From is the month to take over in. */
	bool Begin(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, FString& OutError, const FString& Level = FString(), const FString& Depth = FString());

	/** The whole campaign as text, to be written to a file. Empty if there is no campaign. */
	FString Save();

	/** One line about a saved campaign ("Germany, October 1939"), or empty if the text is not a campaign that can be continued. */
	FString DescribeSave(const FString& SaveText);

	/** Picks a saved campaign up where it was left. */
	bool Load(const FString& SaveText);

	bool IsRunning() const { return bRunning; }
	const FCampaignView& GetView() const { return View; }

	/** Where the given corps could go this month. */
	/** What declaring war on a country would mean, for the question asked first. RegionId is where the army in hand was going, or INDEX_NONE. */
	FWarPlan PlanWar(const FString& Nation, int32 RegionId);
	TArray<FReachRegion> Reach(const TArray<int32>& CorpsIds);

	/** Orders the given corps to march to a region. Returns the marches that actually happened. */
	/** OutNote: what the rules say of the order where it needs saying (Version 65: a full province next door, and the swap offered). */
	TArray<FCorpsMove> Move(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutNote = nullptr);

	/** The odds for the given corps attacking a region, with both armies and the Battle Ticket. */
	/** bOpening: the card is being opened, so the staff may bring in everyone the roads will carry where that beats the pick. */
	FAttackPreview PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening = false);

	/** Sends the given corps in. The battle is settled at once by the rules. */
	FAttackResult Attack(const TArray<int32>& CorpsIds, int32 RegionId);

	/** The player's standing order for these corps on the day the enemy comes: "stand", "dig" or "back". */
	void SetStance(const TArray<int32>& CorpsIds, const FString& Posture);

	/** Where the given corps could go by rail this month. */
	TArray<int32> RailReach(const TArray<int32>& CorpsIds);

	/** Sends the given corps by rail. Returns the ones that went. */
	TArray<FCorpsMove> Rail(const TArray<int32>& CorpsIds, int32 RegionId);

	/** Where these corps could go by sea this month: friendly ports to be shipped to, and enemy coasts to land on. */
	void SeaReach(const TArray<int32>& CorpsIds, TArray<int32>& OutPorts, TArray<int32>& OutBeaches);
	TArray<FCorpsMove> Ship(const TArray<int32>& CorpsIds, int32 RegionId);
	FLandingPlan PlanLanding(const TArray<int32>& CorpsIds, int32 RegionId);
	FLandingResult Land(const TArray<int32>& CorpsIds, int32 RegionId);

	/** What a territory holds, and what the player can raise or build there. */
	FProvinceInfo Province(int32 RegionId);

	/** Orders something in a territory: a kind of corps ("inf", "mtn", "mot", "arm") or works ("fort"). OutNote says what was ordered. */
	bool OrderInProvince(const FString& Action, int32 RegionId, FString& OutNote, TArray<FSortie>* OutSorties = nullptr);

	/** Takes back an order placed this month. */
	bool CancelOrder(int32 OrderId);

	/** Everything the player can order, with a province in mind (INDEX_NONE for none). */
	FArsenal Arsenal(int32 RegionId);

	/** A fleet ("fleet") or an air fleet ("air") and the orders it can be given; and giving it one of them. */
	FForcePanel Force(const FString& Kind, int32 Id);
	bool OrderForce(const FString& Key, FString& OutNote);

	/** Rivers, crossings, railways and steep country, as the rules have them. Empty with the stand-in rules. */
	FLandFeatures Land();
	/** Dismisses a general for good; sets the picked corps' standing order about drafts; places the army by the staff's plan on the eve of war. */
	bool SackGeneral(int32 GeneralId, FString& OutNote);
	bool SetDrafts(const TArray<int32>& CorpsIds, bool bTake, FString& OutNote);
	bool PlaceByPlan(FString& OutNote);
	/** Asks the allied corps in a province to march with the player's army there; sends one of them home again. */
	bool JoinArmy(int32 RegionId, FString& OutNote);
	bool SendBack(int32 CorpsId, FString& OutNote);
	/** The sea under a point of the map, or INDEX_NONE for land. */
	int32 SeaAt(const FVector2D& MapPoint);
	/** Every province's chief town, what the province pays and how that ranks. */
	TArray<FTownInfo> Towns();

	/** Places an order from the arsenal. OutName is what was ordered. */
	bool OrderArsenal(const FString& Key, int32 RegionId, FString& OutName);

	/** The player's books for this month. */
	FLedger Ledger();

	/** The foreign office: the powers at war and every country at peace, as the player sees them. */
	FDiplomacy Diplomacy();

	/** Spends political capital to court a neutral. OutNote says what came of it. */
	bool Court(const FString& Nation, FString& OutNote);

	/** The abilities of the player's nation, a line each. bOutActive is false in a historical scenario, where they do not count. */
	TArray<FString> Abilities(bool& bOutActive);

	/** One of the rooms the rules set out themselves ("spies", "lastact"), with the province selected on the map in mind. */
	FOfficePage Page(const FString& Id, int32 RegionId);

	/** Something pressed in such a room. OutNote says what came of it. */
	bool Act(const FString& Action, int32 RegionId, FString& OutNote, TArray<FSortie>* OutSorties = nullptr, FString* OutOpen = nullptr, FActExtras* OutExtras = nullptr);
	/** One of the player's fleets as the rules describe it, with the seas and coasts in its reach. */
	FFleetHand FleetInHand(int32 FleetId);
	/** The situation as the campaign opens, as stops of a flight over the map. Empty if there is nothing to tell. */
	TArray<FNewsCard> Briefing(FString& OutNation, FString& OutDate);

	/** The generals of the player's nation, and the corps that have none. */
	FGeneralStaff Generals();
	bool AssignGeneral(int32 GeneralId, int32 CorpsId);
	bool PromoteOfficer(int32 CorpsId, FString& OutName);
	bool RelieveGeneral(int32 GeneralId);
	bool SpendSkill(int32 GeneralId, const FString& Skill);

	/** The design offices: every branch, the teams on it, and the other powers' designs. */
	FResearch Research();

	/** The grand campaign's tree: puts a free team to work on a technology (OutWhy says why not), or takes it off. */
	bool StartTech(const FString& Id, FString& OutWhy);
	bool StopTech(const FString& Id);

	/** Puts this many design teams on a branch. */
	bool SetTeams(const FString& Field, int32 Teams);

	/** Founds one more design team, paid for from the treasury. */
	bool FoundBureau();

	/** The player's small country takes a path: axis, allies, alone, found (with a partner) or join. OutNote says what came of it. */
	bool TakePath(const FString& Id, const FString& Partner, FString& OutNote);

	/** The player, a member of the League, invites a willing country into it. */
	bool InviteToLeague(const FString& Nation, FString& OutNote);

	/** The player declares war on a country he is at peace with. */
	bool Declare(const FString& Nation);

	/** One corps' service record: what it is, its honours with their citations, and its diary. */
	FDocumentPage Record(int32 CorpsId);

	/** The roll of honour and the memorial wall. */
	FDocumentPage Roll();

	/** Ends the player's month and plays every other nation's. Returns what happened, nation by nation. */
	FMonthResult EndMonth();

	/** Fired whenever the picture of the war has changed. */
	DECLARE_MULTICAST_DELEGATE(FOnViewChanged);
	FOnViewChanged OnViewChanged;

private:
	bool CallRules(const FString& Function, const FString& ArgumentsJson, TSharedPtr<FJsonObject>& OutObject);
	static FCampaignView ParseView(const TSharedPtr<FJsonObject>& Object);
	void TakeView(const TSharedPtr<FJsonObject>& Holder);

	TUniquePtr<FRulesHost> Host;
	FCampaignView View;
	bool bReady = false;
	bool bRunning = false;
	TArray<FGrandYear> Years;
	TArray<FGrandLevel> Levels;
	FString DefaultLevel;
	TArray<FGrandLevel> Depths;
	FString DefaultDepth;
};
