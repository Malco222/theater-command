#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
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
	/**
	 * (Release 2, tc.Speed.NationsAside; the UI director's bar: every press answers within a tenth of a second.)
	 * GrandNations worked out on the rules' own thread as an order is: a later year is history played up to it, 0.7 to
	 * 1 s for January 1941 in a cloud run, and the opening screen asks it on every year picked and every time it opens.
	 * Then is given the list on the game thread, from Deliver, a frame or a few later (empty where the rules failed).
	 * bAside false, or the rules not ready or not given aside, asks it at once and gives it to Then before returning.
	 */
	void GrandNationsAside(int32 From, int32 Seed, bool bAside, TFunction<void(TArray<FGrandNation>)>&& Then);

	/** Starts a new campaign. Human is the nation the player leads, or empty to watch. From is the month to take over in. */
	/** bFirstCommand (rules 84.3.3): the campaign FIRST COMMAND opened, marked as its, with the ten notes armed again once all are done. */
	bool Begin(const FString& Human, int32 From, const FString& ViewMode, int32 Seed, FString& OutError, const FString& Level = FString(), const FString& Depth = FString(), int32 History = 0, bool bFirstCommand = false);

	/** The whole campaign as text, to be written to a file. Empty if there is no campaign. */
	FString Save();
	/**
	 * (Release 2, tc.Speed.SaveAside) Save, worked out on the rules' own thread as an order is (the war written out
	 * changes nothing in it, so nothing kept is forgotten for it): Then is given the text on the game thread, from
	 * Deliver, a frame or a few later, or empty where there is no campaign or the rules failed. bAside false (or the
	 * rules not given aside) asks it at once, as Save does, and gives it to Then before returning.
	 */
	void SaveAside(bool bAside, TFunction<void(FString)>&& Then);

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
	/** Army, ArmyIds: (one voice after a press) the army in hand's name as its tray names it, and its corps in the field, so that the line names whom it is true of. */
	FMarchPlan PlanMarch(const TArray<int32>& CorpsIds, int32 RegionId, const FString& Army = FString(), const TArray<int32>& ArmyIds = TArray<int32>());
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
	/** Army, ArmyIds: as for PlanMarch. A march kept is answered by one sentence naming the army, or so many corps of it, or the one corps. */
	TArray<FCorpsMove> Move(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutNote = nullptr, const FString& Army = FString(), const TArray<int32>& ArmyIds = TArray<int32>());

	/** The odds for the given corps attacking a region, with both armies and the Battle Ticket. */
	/** bOpening: the card is being opened, so the staff may bring in everyone the roads will carry where that beats the pick. */
	/** bGlance: for the pointer's tip, which reads only the odds; the rules leave out the card's soften row, the dearest part. */
	FAttackPreview PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening = false, bool bGlance = false);

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
	/** (Rules 84.2.5; ruling 211(a).) Why these corps cannot go forward onto that ground by themselves ("Guns cannot take ground alone. ..."), in the rules' own sentence; empty where they can, and before that version. */
	FString WhyNotLead(const TArray<int32>& CorpsIds, int32 RegionId);
	/** Why the chosen corps cannot go by sea to a place the by-sea offer leaves out: the rules' own sentence for the first test it fails, or nothing. */
	FString WhyNotBySea(const TArray<int32>& CorpsIds, int32 RegionId);
	bool OrderGate(int32 GateId, bool bShut, FString& OutNote);
	/** (Rules 84.2.) The warning where the corps in hand are the last to hold a gate's province: "If they go, the Panama Canal is left to its town guard: about 4,800 men, and no army." */
	FString GateLeft(const TArray<int32>& CorpsIds);

	/**
	 * (Ruling 182.) The assault on a town the player's army already stands round, for its card. Not valid where
	 * there is nothing to storm; Refused carries the rules' sentence where they will not have a storm this month.
	 */
	FAttackPreview PreviewStorm(int32 RegionId);
	/** The garrison's sortie against the army round its town, as a forecast for the attack card; and the sortie ordered from that card. */
	FAttackPreview PreviewSally(int32 RegionId);
	FAttackResult Sally(int32 RegionId);

	/** (Ruling 182.) The storm, ordered from its card: every corps of the player's round the town goes in. */
	FAttackResult Storm(int32 RegionId);

	/** The player's standing order for these corps on the day the enemy comes: "stand", "dig" or "back". */
	void SetStance(const TArray<int32>& CorpsIds, const FString& Posture);

	/** Where the given corps could go by rail this month. */
	TArray<int32> RailReach(const TArray<int32>& CorpsIds);

	/** Sends the given corps by rail. Returns the ones that went. */
	TArray<FCorpsMove> Rail(const TArray<int32>& CorpsIds, int32 RegionId, FString* OutSaid = nullptr);
	/** (Rulings 110, 112, 118.) What the trains will do for these corps this month; March is where they can walk to. */
	/** Army, ArmyIds: as for PlanMarch, so that "No train for the American army this month" is said of the army where it is true of all of it. */
	FRailShow RailShow(const TArray<int32>& CorpsIds, const TArray<int32>& March, const FString& Army = FString(), const TArray<int32>& ArmyIds = TArray<int32>());
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
	/** (Release 2: the battle schools.) Sends the corps to the battle schools for the month (bGo), or calls them back; OutNote is the rules' word on it. */
	bool SendToSchool(const TArray<int32>& CorpsIds, bool bGo, FString& OutNote);
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
	bool Declare(const FString& Nation, FString* Said = nullptr);

	/** One corps' service record: what it is, its honours with their citations, and its diary. */
	FDocumentPage Record(int32 CorpsId);

	/** The roll of honour and the memorial wall. */
	FDocumentPage Roll();

	/** Ends the player's month and plays every other nation's. Returns what happened, nation by nation. */
	FMonthResult EndMonth();
	/**
	 * (Release 2, tc.Speed.MonthAside; the map-speed lane, the whole-world map's month end.) EndMonth worked out on the
	 * rules' own thread as an order is: the other nations' months (1.0 to 1.7 s a month on the whole-world map in a cloud
	 * run, after the far map's speedups) and the reading of their answer (some 1.9 MB of text there, five steps each with
	 * a whole picture of the war) are done there, so the frames go on meanwhile. Then is given what happened on the game
	 * thread, from Deliver, a frame or a few later, after the new picture of the war is taken; empty where the rules
	 * failed, as EndMonth's. IsAnswering is true until then, so the table takes no order. bAside false, the switch off, or
	 * "tc.Speed.RulesAside 0" ends the month at once, as EndMonth does, and gives it to Then before returning.
	 */
	void EndMonthAside(bool bAside, TFunction<void(FMonthResult&&)>&& Then);

	// ---- (Rules 84.3.3) FIRST COMMAND: the first hour's ten staff notes ----
	// The lessons, their words and the tests of their deeds are the rules'; the deeds of the player's own orders are
	// read by the rules as each order is carried out. The notebook (what he has learnt) is the player's, kept in its
	// own small file beside the options and never in a save. None of these calls changes the war, so none forgets
	// the questions kept (see CallRules).

	/**
	 * Whether the ARSENAL is open, the page lesson 10 sends the player to: while it is, that lesson's note says its words
	 * for being there (it asks for the artillery corps, since a note never asks for what is already done; the UI director,
	 * build 1640). The screen sets it before it asks the desk, and it goes with every lesson call's moment.
	 */
	void SetLessonPageOpen(bool bOpen) { bLessonPageOpen = bOpen; }
	/** The note on the desk for this moment: bCard, the month card is open; InHand, the corps of the army in hand. */
	bool LessonNow(bool bCard, const TArray<int32>& InHand, FLessonDesk& Out);
	/** A deed only the screen sees: "card" (the month card lesson 1 lay on, put down) or "line" (one of its lines followed). */
	bool LessonDeed(const FString& Deed, bool bCard, FLessonDesk& Out);
	/** "later", "back:<id>", "again:<id>", "all", "on", "off", "close" or "tip": see the bridge's lessonAct. */
	bool LessonAct(const FString& Action, bool bCard, const TArray<int32>& InHand, FLessonDesk& Out);
	/** The Guide's list of the ten lessons, as the screen stands (the lesson due now is "waiting for you"). */
	FLessonGuide LessonGuide(bool bCard = false, const TArray<int32>& InHand = TArray<int32>());
	/** FIRST COMMAND's button on the door, and the campaign it starts (asked before any campaign is under way). */
	FLessonDoor LessonDoor();
	/** (Trap 1) Lays out the card that opens a war played from its first month as the month's card, for the reports page. */
	bool OpenOpeningCard();

	/** Fired whenever the picture of the war has changed. */
	DECLARE_MULTICAST_DELEGATE(FOnViewChanged);
	FOnViewChanged OnViewChanged;

	/**
	 * (Speed: the design director's mark 1 for build 414, a press's own work under a tenth of a second.) The orders that
	 * cost most on the Mac (a march about 47 ms in the rules, a landing 85, a sortie 67) are answered on a thread of
	 * their own, so that the frame of the press waits for nothing. The picture of the war is read from the answer on
	 * that thread too, and the questions the army in hand asks straight after (how it stands, where it can reach, what
	 * the trains can do, the province's card) are asked there in the same trip and kept, so that the frames that show
	 * the answer ask the rules nothing. Each was checked against the rules: asked twice it answers the same, and it
	 * leaves the saved game as it found it. The game's own picture changes only on the game thread, when the answer is
	 * taken (Deliver). Every other call into the rules waits for an answer still being worked out, and takes it first.
	 * With bAside false (the photographer), or "tc.Speed.RulesAside 0", the order is answered at once, as before.
	 */
	struct FHandNext
	{
		/** The province the army in hand will be in, and its corps there before the tray leaves out those that cannot be ordered. */
		int32 Region = INDEX_NONE;
		TArray<int32> Ids;
		/** The province whose card is shown after the order. */
		int32 Selected = INDEX_NONE;
		/** A hand that marches (not one at sea or on the railway): it asks where it can reach, and what the trains can do. */
		bool bPlain = true;
		/** The nation's adjective as the tray's head names its army ("the German army"), where no general leads it. */
		FString Adjective;
	};
	/** The marches an order made, and the rules' sentence on it. */
	struct FMoveAnswer
	{
		TArray<FCorpsMove> Moved;
		FString Note;
	};
	/** Each Guess is run on the rules' thread, on the answer as read there: it may use only what it was given. Then is run on the game thread. */
	void MoveAside(const TArray<int32>& CorpsIds, int32 RegionId, const FString& Army, const TArray<int32>& ArmyIds, bool bAside,
		TFunction<FHandNext(const FMoveAnswer&)>&& Guess, TFunction<void(const FMoveAnswer&)>&& Then);
	void LandAside(const TArray<int32>& CorpsIds, int32 RegionId, bool bAside, TFunction<FHandNext(const FLandingResult&)>&& Guess, TFunction<void(const FLandingResult&)>&& Then);
	/**
	 * (Speed, the UI director's bar of 22 November 2026: every press answers within a tenth of a second.) The attack
	 * card's forecast, PreviewAttack's, worked out on the rules' own thread as an order is (it changes nothing in the
	 * war, so nothing kept is forgotten for it); Then is given it on the game thread, from Deliver, a frame or a few
	 * later, or an empty forecast where the rules failed or found nobody there. bAside false asks it at once, as before.
	 */
	void PreviewAside(const TArray<int32>& CorpsIds, int32 RegionId, bool bOpening, bool bAside, TFunction<void(const FAttackPreview&)>&& Then);
	/** Function: "attack", "take" (bAssault as Take's), "storm" or "sally" (no corps: the province says who). */
	void FightAside(const FString& Function, const TArray<int32>& CorpsIds, int32 RegionId, bool bAssault, bool bAside,
		TFunction<FHandNext(const FAttackResult&)>&& Guess, TFunction<void(const FAttackResult&)>&& Then);
	/**
	 * (release 2) A land battle the player fought himself, as the general, handed back: booked as the attack order is,
	 * through TC.battleSettleLand with the ticket's seed (so a battle other than the one the campaign would make now is
	 * never booked), and its picture of the war taken. ResultJson is the field's TC.landResult answer ({ "over": .. }).
	 */
	FAttackResult SettleLandLive(const TArray<int32>& CorpsIds, int32 RegionId, const FString& ResultJson, double TicketSeed);
	/**
	 * (Release 1.6) A raid the player flew himself, handed back: TC.battleEnd's answer for the air battle (the rules booked
	 * the raid there, as TC.act books FLY THE RAID, with the battle's losses), read as Act reads its answer: whether it went,
	 * the rules' note, the groups seen to fly it, and its picture of the war taken. False where it could not be read or did not go.
	 */
	bool SettleAirLive(const FString& EndJson, FString& OutNote, TArray<FSortie>* OutSorties = nullptr);

	// ---- (Release 2, the scramble offer; Brian, 24 November 2026 22:21Z: "i want every air battle playable".) The month
	// may stop where an enemy raid meets his fighters (EndMonth's FMonthResult::Scramble); he flies the defence himself
	// (AAirBattle::BeginLiveCall with TC.scrambleStart and ScrambleArgs) or leaves it to his staff. The calls are the
	// contract's (air-battles/every-kind/CONTRACT.md section 7): until the warplanes thread's month stop lands, the
	// bridge's stand-in (Rules/scramble-demo.ts) answers them, and books nothing.
	/** TC.scrambleMode(Mode) before TC.endMonth: "every", "big" or "never". Rules without it are not asked again (logged once): harmless.
	 * "never" over "never" is not sent: the rules start there at every load, so with the switch off nothing new is asked. */
	bool TellScrambleMode(const FString& Mode);
	/** (-TheaterAirScrambleDemo) TC.scrambleDemo(): the stand-in's offer of an enemy raid on him now, or false where there is none. Changes nothing in the war. */
	bool ScrambleDemo(FScrambleOffer& Out);
	/** TC.scrambleLeave(Id, { rest }): his staff meets the raid; bRest (the card's tick) settles the rest of the month's raids too. OutJson: its answer as given. */
	bool ScrambleLeave(const FString& Id, bool bRest, FString& OutJson);
	/** TC.scrambleStart's arguments, ["Id"], for AAirBattle::BeginLiveCall. */
	static FString ScrambleArgs(const FString& Id);
	/**
	 * The month's next answer after a scramble (TC.battleEnd's at CONTINUE, or TC.scrambleLeave's), read as EndMonth reads
	 * TC.endMonth's: true where it is a month's answer (it has the month's steps, or the next scramble with its id), its
	 * picture of the war taken. False where it is not (the stand-in's { ok, note }, a scramble with no id, or nothing): Out
	 * and the war's picture are left as they were. OutNote: the answer's note, where it has one, either way.
	 */
	bool ReadMonthAnswer(const FString& Json, FMonthResult& Out, FString& OutNote);
	// ---- (end release 2, the scramble offer)
	/** True from an order given aside until its Then has been run. */
	bool IsAnswering() const { return Aside.IsValid() || static_cast<bool>(AsideThen); }
	/** Once a frame, from the player's controller: where the answer is in, it is taken and its Then is run. bWait: waited for, where it is still being worked out. */
	void Deliver(bool bWait = false);

	/**
	 * (D1, ruling 241) The month's submarine scene: the viewer's setting, "all" (every month that holds a scene; the
	 * default), "mine" (his own losses only), "never" (none), or "off" (none, and the rules keep no record of the month
	 * at sea). It is the viewer's, kept outside the save (Saved/Campaign/subscene.json, a run's own subscene-runs.json,
	 * where a run is "off" unless asked), and "-TheaterSubScene=" sets it for a run. The rules are told it before every
	 * start and after every load (TC.subSceneOn); SetSubScene tells them at once and writes it.
	 */
	FString GetSubScene() const;
	void SetSubScene(const FString& Setting);
	/**
	 * (Release 2, teamfire.ts) The Battle rules card's rows the campaign's battle card must follow. Every infantry force's
	 * own teams are on the field only with the rows `teams` and `facing` both on (FLandLive::RuleNow), and the card prices
	 * them only then (TC.teamFire): told to the rules after every start and every load, and here when a row is clicked
	 * with a war running. Off, the rules delete their key, so the saved war is the one it always was.
	 */
	void SyncBattleRules();
	/** TC.subScene(mark, view) asked on the rules' thread beside the war (it reads the war and changes nothing in it): not
	 * an order, so IsAnswering stays false and the table takes its keys and orders meanwhile; anything else asked of the
	 * rules waits for it first (Settle). Then, run on the game thread by Deliver, is given the mark to count from next
	 * month and the staged recording's text, empty where this month holds no scene for him (or the rules could not
	 * say). Mark -1: from the war's first headline. Dropped, its Then unrun, where the war is begun or read in. */
	void SubSceneAside(int32 Mark, TFunction<void(int32 NextMark, const FString& Recording)>&& Then);
	/** How many wars have been begun or read in since the game opened: a change says nothing kept of the last holds. */
	int32 GetWarsBegun() const { return WarsBegun; }

	/**
	 * (Ruling 238, the air battle scene.) A battle's own call on the rules (land's battle-bridge.ts: TC.airStart,
	 * battleTick, battleSpeed, battleOrder, battleView, battleEnd), its answer handed back as JSON text for the scene's
	 * own reader. Every such call may change the war, so the kept answers to questions are forgotten.
	 */
	bool CallBattle(const FString& Function, const FString& ArgumentsJson, FString& OutJson, FString& OutError);

	// R2 bargains hook: a neutral's bargains and the volunteer legions (Campaign/BargainCalls.cpp; switched off in release 1)
	/** TC.bargainsLook()'s answer as JSON text, for Campaign/BargainFeed: {"on":false} while both are off. Changes nothing, and is kept as the questions are. */
	bool BargainsLook(FString& OutJson);
	/** The player bids Pp a month (one of the round's steps) for the bargain Id. */
	bool BargainBid(const FString& Id, double Pp, FString& OutNote);
	/** The player, as the seller, sells the bargain Id to Buyer ("" for nobody). */
	bool BargainSell(const FString& Id, const FString& Buyer, FString& OutNote);

private:
	bool CallRules(const FString& Function, const FString& ArgumentsJson, TSharedPtr<FJsonObject>& OutObject);
	struct FAside;
	/** The order on the rules' thread: Guess (there) says what the hand will ask next; Then (here) is given the answer, or nothing where the rules failed. */
	void Order(const FString& Function, const FString& ArgumentsJson, bool bAside, TFunction<FHandNext(const TSharedPtr<FJsonObject>&)>&& Guess, TFunction<void(const TSharedPtr<FJsonObject>&)>&& Then);
	/** Waits for an order still being answered, where there is one, and takes its picture of the war and the answers kept with it. Its Then is left for Deliver. */
	void Settle();
	/** A new war, or one read in: the order still being answered is taken, and its Then, which belongs to the old war, is dropped. */
	void DropAside();
	/** An order that may end in a battle (attack, take): what the rules answer, read as a battle's result. */
	FAttackResult Fight(const FString& Function, const FString& ArgumentsJson);
	static FCampaignView ParseView(const TSharedPtr<FJsonObject>& Object);
	/** ParseView without the speed log's timing, which is kept on the game thread only: for the rules' thread. */
	static FCampaignView ReadView(const TSharedPtr<FJsonObject>& Object);
	/** The endMonth answer read into what happened (its picture of the war is left for TakeView). bGameThread: the steps' pictures read with ParseView's timing, as EndMonth always read them; false on the rules' thread. */
	static FMonthResult ReadMonth(const TSharedPtr<FJsonObject>& Object, bool bGameThread);
	void TakeView(const TSharedPtr<FJsonObject>& Holder);
	/** (Rules 84.3.3) The player's notebook of FIRST COMMAND: its file, read into the rules once they are loaded, and written whenever an answer carries it. */
	static FString NotebookFile();
	void ReadNotebook();
	void KeepNotebook(const TSharedPtr<FJsonObject>& Answer);
	/** A lesson call's answer read into the desk, the notebook written where it changed. */
	bool ReadLessonDesk(const TSharedPtr<FJsonObject>& Object, FLessonDesk& Out);
	/** The ARSENAL is open, as the screen last said (SetLessonPageOpen). */
	bool bLessonPageOpen = false;
	/** (D1) TC.subSceneOn as the viewer's setting says (GetSubScene), on the rules as they stand, war or none. */
	void TellSubScene();
	/** (Release 2) TC.teamFire as the Battle rules card stands, on the war the rules hold: see SyncBattleRules. */
	void TellBattleRules();
	/** The setting's file, and the setting as read (empty until GetSubScene first reads it). */
	static FString SubSceneFile();
	mutable FString SubScene;
	/** The rules have no month's scene (the stand-in rules, or rules before D1): nothing is asked of them. */
	bool bNoSubScene = false;
	/** (Release 2, the scramble offer) The rules have no TC.scrambleMode (the stand-in rules, or rules before it): it is not asked again. */
	bool bNoScrambleMode = false;
	/** (Release 2, the scramble offer; the reviewer, 25 November) What TC.scrambleMode was last told by this load of the
	 * rules ("never" until told: the rules start there). "never" is not sent again over "never", so that with "Scramble
	 * your fighters" off no new call is made. */
	FString ScrambleModeSent = TEXT("never");
	int32 WarsBegun = 0;

	TUniquePtr<FRulesHost> Host;
	/** The order being answered on the rules' thread, and that thread's work; the Then of an answer taken and not yet run. */
	TSharedPtr<FAside, ESPMode::ThreadSafe> Aside;
	TFuture<void> AsideRun;
	/** (D1) The month's scene being asked for on the rules' thread (SubSceneAside), and that thread's work. */
	struct FSceneAsk;
	TSharedPtr<FSceneAsk, ESPMode::ThreadSafe> SceneAsk;
	TFuture<void> SceneAskRun;
	/** Waits for the month's scene being asked for, where it is (the rules answer one thing at a time). */
	void WaitSceneAsk();
	TFunction<void()> AsideThen;
	/** The rules' last answers to the questions that change nothing, by question and what was asked: see CallRules. */
	TMap<FString, FString> RulesMemo;
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
