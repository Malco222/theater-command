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
	/** (rules version 70) The history dial's five stops for the new-campaign card (none where the rules have no dial), and the one it opens on. */
	const TArray<FGrandLevel>& HistoryStops() const { return Histories; }
	int32 FirstHistory() const { return DefaultHistory; }

	/** Who can be led in the grand campaign from a month. History is played up to it the first time this is asked. */
	TArray<FGrandNation> GrandNations(int32 From, int32 Seed);

	/** Starts a new campaign. Human is the nation the player leads, or empty to watch. From is the month to take over in. */
	bool Begin(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, FString& OutError, const FString& Level = FString(), const FString& Depth = FString(), int32 History = 0);

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
	/**
	 * Where a picked-up army could go. If asked for (release 1): OutDanger, the regions the enemy the player can see
	 * could strike next month; OutFar, friendly ground beyond this month's reach with about how many months the march
	 * would take; OutStanding, the road still ahead of a corps already on a standing march.
	 */
	TArray<FReachRegion> Reach(const TArray<int32>& CorpsIds, TArray<int32>* OutDanger = nullptr, TMap<int32, int32>* OutFar = nullptr, TArray<int32>* OutStanding = nullptr, bool* OutSpent = nullptr);
	/** (Ruling 87.) The rules' plan for a march of these corps to a place on a standing order: nothing is ordered by asking. */
	FMarchPlan PlanMarch(const TArray<int32>& CorpsIds, int32 RegionId);
	/**
	 * (Ruling 102.) The province of the army the round goes to next, by the rules' own order: those not yet visited
	 * this round first, the nearest to From (on the map's own scale) among equals. InOutSeen is the round so far and
	 * comes back with this one added. INDEX_NONE when every army has its orders.
	 */
	int32 NextArmy(TArray<int32>& InOutSeen, const FVector2D& From);
	/** STAND THIS MONTH for the army in a province, or taking it back. Returns how the army there now stands: "all", "some", "none", or empty. */
	FString Stand(int32 RegionId, bool bOn);
	/** How the army in a province stands this month, without changing it. */
	FString Stands(int32 RegionId);
	/** (For the photographer only.) Gives a province anti-aircraft guns of a level, for a picture of the raid card's defences. */
	void StageGuns(int32 RegionId, int32 Level);
	/** (For the photographer only.) Cuts a province's railway, with a cause; "report" or "seen" only writes down that the player knows of a cut there. */
	void StageCut(int32 RegionId, const FString& Cause, const FString& How);
	/** (For the photographer only.) Flies a reconnaissance flight over a province; what the flight reported, or empty. */
	FString StageRecon(int32 RegionId);
	/** (For the photographer only.) Sets an agent of the player's at work in a province at once ("road": still on his way there; or a mission's id). His id. */
	int32 StageAgent(int32 RegionId, const FString& Mission);
	/** (For the photographer only.) Writes down that an agent of his was caught in a province this month. */
	void StageCaught(int32 RegionId);
	/** (Ruling 163.) Why the player's agent cannot be sent to this province, in the rules' words; empty where he can go. */
	FString WhyNotSendAgent(int32 AgentId, int32 RegionId);

	/** Orders the given corps to march to a region. Returns the marches that actually happened. */
	/** OutNote: what the rules say of the order where it needs saying (Version 65: a full province next door, and the swap offered). */
	TArray<FCorpsMove> Move(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutNote = nullptr);

	/** The odds for the given corps attacking a region, with both armies and the Battle Ticket. */
	/** bOpening: the card is being opened, so the staff may bring in everyone the roads will carry where that beats the pick. */
	FAttackPreview PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening = false);

	/** Sends the given corps in. The battle is settled at once by the rules. */
	FAttackResult Attack(const TArray<int32>& CorpsIds, int32 RegionId);

	/** (The playtester's click-through.) The digest a step is logged with, as the rules write it, with its stamp and the holder of the province named. */
	FString StepDigest(int32 RegionId, const TArray<int32>& CorpsIds, int32 FleetId, int64& OutStamp, FString& OutHolder);
	/** (The same.) A unit or a place by its name: a province, a sea, a corps and where it stands, a fleet, an air group. INDEX_NONE for what no such name is. */
	bool StepFind(const FString& Name, int32& OutRegion, int32& OutSea, int32& OutCorps, int32& OutCorpsRegion, int32& OutFleet, int32& OutGroup);

	/**
	 * (Rules 84.1.) What marching the given corps onto enemy ground with no army in its field will meet. Not valid
	 * where the march is a plain move or a field battle. The rules read the forecast off a copy of the campaign: it
	 * is for the slip of a province the pointer rests on and for the card, never for every frame.
	 */
	FAttackPreview PreviewTake(const TArray<int32>& CorpsIds, int32 RegionId);

	/** (Rules 84.1.) The order to take a place: the corps march in, and with bAssault go at a town held by its guard alone as they arrive. */
	FAttackResult Take(const TArray<int32>& CorpsIds, int32 RegionId, bool bAssault);

	/** (Rules 84.1; ruling 176(e).) The gate's question for its holder, and his order to shut it or open it again. */
	FGateAsk AskGate(int32 GateId);
	/** The march up to an enemy army that is not next door, and the attack, planned as one order (the rules' own reckoning). */
	FApproachPlan PlanApproach(const TArray<int32>& CorpsIds, int32 RegionId);
	/** (Rules 84.2.4.) Why the player's corps inside a province's town take no marching orders, in the rules' own sentence; empty where he has none there. */
	FString GarrisonWhy(int32 RegionId);
	bool OrderGate(int32 GateId, bool bShut, FString& OutNote);
	/** (Rules 84.2.) The warning where the corps in hand are the last to hold a gate's province: "If they go, the Panama Canal is left to its town guard: about 4,800 men, and no army." */
	FString GateLeft(const TArray<int32>& CorpsIds);

	/**
	 * (Ruling 182.) The assault on a town the player's army already stands round, for its card. Not valid where
	 * there is nothing to storm; Refused carries the rules' sentence where they will not have a storm this month.
	 */
	FAttackPreview PreviewStorm(int32 RegionId);

	/** (Ruling 182.) The storm, ordered from its card: every corps of the player's round the town goes in. */
	FAttackResult Storm(int32 RegionId);

	/** The player's standing order for these corps on the day the enemy comes: "stand", "dig" or "back". */
	void SetStance(const TArray<int32>& CorpsIds, const FString& Posture);

	/** Where the given corps could go by rail this month. */
	TArray<int32> RailReach(const TArray<int32>& CorpsIds);

	/** Sends the given corps by rail. Returns the ones that went. */
	TArray<FCorpsMove> Rail(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutSaid = nullptr);
	/** (Rulings 110, 112, 118.) What the trains will do for these corps this month; March is where they can walk to. */
	FRailShow RailShow(const TArray<int32>& CorpsIds, const TArray<int32>& March);
	/** The pointer's words for a province beyond their march. */
	FRailTip RailTip(const TArray<int32>& CorpsIds, int32 RegionId);

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
	/** An order that may end in a battle (attack, take): what the rules answer, read as a battle's result. */
	FAttackResult Fight(const FString& Function, const FString& ArgumentsJson);
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
	TArray<FGrandLevel> Histories;
	int32 DefaultHistory = 3;
};
