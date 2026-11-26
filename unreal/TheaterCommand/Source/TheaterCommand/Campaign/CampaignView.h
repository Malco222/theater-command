#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"

/**
 * What the rules tell the game about the state of the campaign: a plain
 * picture of who holds what and who stands where. The 3D game draws this and
 * never works anything out for itself.
 */

/** An honour a unit carries, as it is shown: what is written on the colours, and the two colours of its ribbon. */
struct FHonourRibbon
{
	FString Title;
	FColor First = FColor(176, 141, 87);
	FColor Second = FColor(242, 234, 214);
};

/** (release 1) What an arm is, for the bigger unit card: all of it from the rules' own tables. */
struct FArmNote
{
	FString Kind; // inf, arm, art, mot, cav, mtn, eng, fort
	FString Label;
	int32 Attack = 0;
	int32 Defence = 0;
	FString Role; // what it is for, in a sentence
	FString Good; // the ground it is at its best on
	FString Poor; // and at its worst
};

/** (Brian's memorial card) A battleship or a carrier lost, by name: her crew and the men lost with her (the rules' 'hulls'). */
struct FLostHull
{
	FString Name; // empty where the nation's record of names has run out
	FString Class; // bb or cv
	FString Type; // (the historian, 19 November 2026) what she was where her class's word is not it: "battlecruiser" for Hood, Renown and Repulse; empty for the rest
	int32 Crew = 0;
	int32 Lost = 0;
	bool bPair = false; // two light carriers the counts treat as one: the name holds both, the crew is both crews
};

/** (release 1) Ships that went down in a fight at sea, as the month's replay shows them: where, whose, what class (bb, cv, ca, dd, ss), how many. */
struct FSinking
{
	int32 Zone = INDEX_NONE;
	int32 Region = INDEX_NONE; // (Brian's memorial card) a ship sunk at her moorings by a raid on a harbour: the province, with no sea (the bridge's zone is -1)
	FString Nation;
	FString Class;
	int32 Count = 1;
	TArray<FLostHull> Ships; // (Brian's memorial card) the battleships and carriers among them, by name
};

/** (release 1) A wreck on the chart: where a battleship, a carrier or a cruiser went down. */
struct FWreckMark
{
	int32 Zone = INDEX_NONE;
	FString Nation;
	FString Class;
	int32 Turn = 0;
	int32 Seed = 0; // (where in the sea it lies: the same every time)
	bool operator==(const FWreckMark& Other) const { return Zone == Other.Zone && Nation == Other.Nation && Class == Other.Class && Turn == Other.Turn && Seed == Other.Seed; }
};

/** (release 1) Enemy ships that were found at sea and are lost again: where they were last seen, and how long ago. */
struct FOldSighting
{
	int32 Zone = INDEX_NONE;
	FString Nation;
	int32 Months = 1;
	bool operator==(const FOldSighting& Other) const { return Zone == Other.Zone && Nation == Other.Nation && Months == Other.Months; }
};

/** One division of a corps, as its card shows it. */
struct FViewDivision
{
	FString Name; // "7th Panzer"
	FString Full; // "7th Panzer Division"
	FString Kind;
	FString Grade;
	float Strength = 1.0f;
	int32 Men = 0;
	int32 Stripes = 0; // 0 to 4: how seasoned it is
	bool bHardened = false;
};

struct FViewCorps
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	FString Kind; // inf, mot, arm, cav, mtn, fort
	FString Elite; // the special formation it is, if it is one: "heavy", "ski", "guards"
	bool bDim = false; // only glimpsed through the fog of war, or in view but not identified: a flag and no more
	bool bNear = false; // dim, but in plain view: a corps not identified
	bool bLean = false; // it takes no drafts: the veterans are kept together
	FString Ally; // an ally's corps that marches with the player's army: its own country
	FString Vol; // R2 bargains hook: a volunteer legion's home, whose plain flag it flies under its host's (empty in release 1)
	int32 Stale = 0; // known from a report this many months old (0: known as it is today)
	FString General; // empty when the corps has no named commander
	FString Posture; // stand, dig, back
	bool bDug = false; // it is entrenched: it spent last month where it stands, digging in
	int32 DugFor = 0; // months it has been dug in where it stands (the grand campaign): its works grow with them; 0 where months are not counted
	float Works = -1.0f; // how far its works have got, in months, where months are counted (attacks wear them down); -1 where they are not
	bool bWorksDone = false; // dug in, and its works finished (or dug in where months are not counted): wire and sandbags, not only an earth bank
	bool bSupplied = true; // supply reaches it
	bool bInTown = false; // it is the garrison of the province's town, not in the field
	int32 Divisions = 0;
	int32 Men = 0;
	float Strength = 1.0f; // 0..1: men and equipment left
	float Cohesion = 1.0f; // 0..1
	int32 Region = INDEX_NONE;
	int32 Moves = 0; // weeks of action left this month
	int32 MovesMax = 0;
	int32 MovesOut = INDEX_NONE; // (the 1.5.3 patch) a corps of his inside a town: the moves it has once an order brings it out (0 if it went in this month); INDEX_NONE where the rules do not say
	int32 MarchTo = INDEX_NONE; // (release 1) the province it is marching to on a standing order
	int32 MarchMonths = 0; // and about how many months it has still to go
	bool bMarchNext = false; // (one voice after a press) and that it has still to set off, as the next month opens
	bool bCanOrder = false; // the player may give this corps orders right now
	FString NoOrders; // (rules 84.1, ruling 176(c)) why a corps of his own never takes orders, in the rules' sentence: "Fortress troops man their works and do not move."
	FString Garrison; // (rules 84.2.4) a corps of his own inside a town: the rules' refusal naming the town ("First Army is the garrison of Pittsburgh. ...")
	int32 Experience = 0; // 0 to 100
	FString Grade; // what that experience is called: Green, Trained, Seasoned, Veteran, Elite
	FString Nickname; // the name the army gives it, once it has earned one
	FString Marked; // (release 2, the marked army; marked.ts) an enemy corps that took one of his towns: that town ("Paris"); empty otherwise
	bool bReady = false; // (release 2, new units ready; Brian 24 Nov 18:30Z) his corps handed over as this month opened: NEW on its pole
	TArray<FHonourRibbon> Honours;
	// The player's own corps, where the rules keep them (the grand campaign): morale and rest with their words, and its divisions.
	float Morale = -1.0f; // 0 to 1, or -1 where no morale is kept
	float Rest = -1.0f;
	FString MoraleWord, RestWord, Trend; // Trend is rising, falling or steady
	FString MoodLine, RestLine, Service;
	TArray<FString> Why; // what has moved its morale, a line each
	bool bResting = false; // it will count as rested if it neither marches nor fights this month
	TArray<FViewDivision> Parts;
	// (Rules version 84.) The weapons it carries, for the row of signs on its card and the weapons on its ring: its
	// anti-tank guns' level (0 to 3), then machine guns, mortars and snipers as 1 or 0. Empty for a corps the rule
	// counts none for (tanks, guns, engineers), for one not identified, and outside the grand campaign.
	TArray<int32> Carry;
	bool bDigging = false; // it has begun to dig where the first month's digging shows no works yet (the open desert)
	// ---- (Release 2: the battle schools.) Only rules that have them say 'school' at all; release 1's never do, and then
	// bSchoolKnown stays false and the corps card shows no school button (Campaign/BattleSchool).
	bool bSchoolKnown = false;
	bool bSchool = false; // at the battle schools this month: it takes no orders until it is back
	FString SchoolTo; // the grade a month there brings it to ("Veteran")
	FString SchoolWhy; // the rules' sentence why it cannot go; empty if it can (and for one already there)
	// ---- (end of the battle schools)
	// (Release 2, breakup.ts; Brian 24 November 2026 18:02Z) Where breaking corps up is on, for his own corps: whether
	// divisions can be split off it now (BreakWhy empty) and the corps it could rejoin here, if any.
	bool bBreakKnown = false;
	FString BreakWhy;
	int32 RejoinId = INDEX_NONE;
	FString RejoinName;
	// (Release 2, rearguard.ts; Brian 24 November 2026 18:24Z) Where rearguards are on, for his own corps: whether it
	// holds as one now, and why it cannot be ordered to (RearWhy empty where it can).
	bool bRearKnown = false;
	bool bRear = false;
	FString RearWhy;
};

/** Something the player has on order: a new corps forming up, or works being dug. */
struct FOrderLine
{
	int32 Id = INDEX_NONE;
	FString Name;
	FString Where;
	FString Due; // the month it arrives
	int32 Months = 0; // months still to wait
	int32 Cost = 0;
	bool bCanCancel = false; // only in the month it was ordered
	FString Sign; // the sign it takes on its city's badge ("inf", "arm", "bmr", "hull" and so on); empty for works
	FString Words; // what it is, in a few words
	bool bStar = false; // a special formation
	float Share = 0.0f; // how far along it is, 0 to 1
};

/** What one of the player's cities has on order, for its one badge on the map (rulings 46 and 57). */
struct FCityOrders
{
	int32 Region = INDEX_NONE;
	FString Sign; // of the unit ready soonest
	bool bStar = false;
	int32 Count = 1; // the orders there of exactly what the sign shows
	bool bPlus = false; // other kinds of unit are on order there too
	int32 Months = 0; // until the unit in the sign is ready
	int32 Total = 0;
	FString Head; // "3 on order: 2 infantry corps and 1 armoured corps. Next ready in 2 months."
	bool bSpied = false; // another country's place, known of only through an agent of the player's there
	TArray<FString> Lines; // one an order, soonest first
};

/**
 * (Release 2, Brian's city at a glance) One town's plate under its name on the map: who holds it, what is on order
 * there, what is going up and what is damaged. The player's own towns and his enemies' as far as his eyes reach
 * (an agent there, or a report of the corps inside); an ally's town has none. Display only: the rules decide nothing here.
 */
struct FCityGlance
{
	int32 Region = INDEX_NONE;
	bool bOwn = false;
	bool bSpied = false; // known only through intelligence: the plate wears the dashed frame and its age
	int32 Ago = -1; // months since the report, for a spied town
	int32 Garrison = 0; // divisions inside the walls, the guard counted, rounded
	bool bGuardOnly = false; // nobody but the town's own guard
	bool bQuiet = false; // (the art director, 22 November) the guard alone, out of the enemy's reach: no "guard" on the plate
	FString GarrisonFigure; // "4 div", "guard", or empty
	FString GarrisonWords; // "the Berlin garrison and the guard · 2 corps in the field outside"
	FString OrderFigure; // the months until the next unit here is ready ("2 mo", "now"), empty when nothing is on order
	FString OrderWords; // "2 infantry corps, then 1 more"
	int32 UpMonths = -1; // months until the next work here is finished, -1 when nothing is going up
	FString UpFigure, UpWords;
	int32 Damage = 0; // percent of the town (or its factories) wrecked
	FString DamageFigure, DamageWords;
};

/** (Release 2) One card on the city panel's GARRISON shelf: an army inside, the guard, an air group or a fleet. */
struct FGlanceCard
{
	FString Kind; // army, guard, air, fleet or field (his corps in the open country of the province)
	FString Sign; // the sign it wears (the order badge's signs, or "defence")
	FString Name, Line;
	float Share = 0.0f; // its strength or readiness, 0 to 1
	int32 Id = INDEX_NONE;
};

/** (Release 2) One card on the ON ORDER HERE shelf. */
struct FGlanceOrder
{
	FString Sign;
	bool bStar = false;
	FString Name, Line; // "Infantry corps", "3 divisions · forms here"
	float Share = 0.0f;
	int32 Months = 0;
	int32 Id = INDEX_NONE;
	bool bFresh = false; // ordered this month: it can still be taken back
};

/** (Release 2) A dated arrival history's book sends to this town (no order of his: nothing to take back). */
struct FGlanceArrival
{
	FString Name, Line, When;
};

/** (Release 2) One tile on the BUILDING AND REPAIRS shelf. */
struct FGlanceTile
{
	FString Kind; // up (going up), stands (finished), free (an empty plot) or damaged
	FString Sign, Name, Line;
	float Share = 0.0f;
	int32 Months = 0;
	int32 Cost = 0; // a repair's price, where he can order it
	FString Key; // what pressing it does: a works key for the rules ("work:repair"), "plots" for the town plan, or empty
	FString Cause; // why it is damaged, or why it cannot be ordered
	FString Does; // (release 2, Brian's 23 November ask) what a building gives: what stands does now, or what the level going up will do
};

/**
 * (Release 2; Brian, 23 November: "what anti-aircraft capabilities they have".) The town's AIR DEFENCE shelf: the level
 * of guns built round it, the guns as a count, the share of a bombing raid they stop, the fighters that would come up
 * over it, who holds the sky, and the next level of guns with its price.
 */
struct FGlanceAir
{
	bool bValid = false;
	int32 Level = 0;
	int32 Heavy = 0, Light = 0; // the guns as a count, as the raid card counts them
	int32 Stop = 0; // the share of a bombing raid they stop, in hundredths
	int32 Going = 0; // levels of guns on order here
	bool bWatch = false; // his side's town: the fighters over it are known
	int32 Fighters = 0; // the fighters that would come up over it against Foe
	FString Foe; // the enemy they would come up against, as an adjective ("Polish"), or empty at peace
	FString Sky; // "ours", "theirs" or empty: who holds the sky over it lately
	FString SkyBy; // whose sky it is, as an adjective
	bool bCanAdd = false; // the next level of guns is offered here
	FString AddKey, AddSize, AddWhy; // the arsenal's key ("work:aa"), "level 2", and why it cannot be ordered now (empty when it can)
	int32 AddCost = 0, AddMonths = 0;
	// (City defences, the switch on) The searchlights built here: their level, and the shelf's row as the rules word
	// it ("Searchlights level 2: every level of guns is lit by night, under a clear sky."). Level 0 and an empty
	// line with none built, and with the switch off, when the rules send no lights at all.
	int32 Lights = 0;
	FString LightsLine;
};

/** (Release 2; Brian, 23 November: "what units that city can build".) One order the arsenal would raise, base, lay down or build in this very town. */
struct FGlanceCan
{
	FString Key; // the arsenal's key: "corps:inf", "air:ftr", "ship:dd", "work:factory"
	FString Group; // army, air, navy or works
	FString Sign, Label, Size;
	bool bStar = false; // the nation's special formation
	int32 Cost = 0, Months = 0;
	FString Why; // why it cannot be ordered now (the treasury short), or empty when it can
};

/** (Release 2) A ship or air group mending at its base: refit in harbour, resting. */
struct FGlanceRow
{
	FString Name, Line, Sign;
	float Share = 0.0f;
};

/** (Release 2) The city panel's AT A GLANCE page: the same three shelves as the plate, in full. */
struct FCityShelves
{
	bool bValid = false;
	bool bOwn = false, bSpied = false;
	bool bAgent = false; // another country's town that an agent of his is at work in this month: he sees all of it
	int32 Ago = -1;
	FString GarrisonHead;
	TArray<FGlanceCard> Garrison;
	TArray<FGlanceOrder> Orders;
	TArray<FGlanceArrival> Arriving;
	FString BuildHead; // "2 going up · 1 damaged"
	TArray<FGlanceTile> Tiles;
	TArray<FGlanceRow> Rows;
	int32 Agents = 0; // his agents in the town
	bool bOnGuard = false; // one of them on guard against the enemy's
	FString Rail; // open, cut, none, or empty when unknown
	FGlanceAir Air; // (release 2) AIR DEFENCE
	TArray<FGlanceCan> Can; // (release 2) CAN BUILD HERE: his own towns only
};

/** The player's treasury. */
struct FTreasury
{
	bool bValid = false;
	int32 Production = 0; // production points in hand
	int32 Income = 0; // earned each month
	int32 Political = 0; // political capital
	TArray<FOrderLine> Orders;

	// Money as the player sees it (a production point is shown as a hundred).
	bool bBudget = false; // the grand campaign: forces cost money to keep, and the treasury can run into debt
	int32 Money = 0; // in the treasury now; below nothing when in debt
	int32 Gross = 0; // comes in each month
	int32 Upkeep = 0; // goes out each month to keep the forces in being
	int32 Net = 0; // what is left each month
	float Debt = 0.0f; // 0 in credit, rising to 1 when the nation owes all it is allowed to
};

/** One line of the ledger: a source of money or a cost, how much, and how many of the thing it counts. */
struct FLedgerLine
{
	FString Label;
	int32 Amount = 0;
	int32 Count = 0;
};

/** What one province gives each month. */
struct FLedgerProvince
{
	int32 Region = INDEX_NONE;
	FString Name;
	bool bHome = false;
	int32 Land = 0, Port = 0, Oil = 0, Factories = 0, Capital = 0, Total = 0;
};

/** The player's books: where the money comes from, what the forces cost, what is left, and what is on order. */
struct FLedger
{
	bool bValid = false;
	bool bBudget = false;
	FString Nation;
	FString Date;
	int32 Money = 0, Gross = 0, UpkeepTotal = 0, Net = 0, DebtFloor = 0, OnOrder = 0;
	float Debt = 0.0f;
	TArray<FLedgerLine> Income; // by source
	TArray<FLedgerLine> Changes; // nation-wide gains and losses on that income
	TArray<FLedgerLine> Upkeep; // by arm
	TArray<FLedgerProvince> Provinces; // richest first
	TArray<FLedgerLine> Orders; // what is on order: Label is what and where, Amount what was paid
	int32 Revision = 0;
};

/** A fleet: whose, where, what it is doing, and its ships by class. */
struct FViewFleet
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	int32 Zone = INDEX_NONE; // the sea it is in
	int32 Base = INDEX_NONE; // the region of its home port, or INDEX_NONE when that is off the map
	FString Port;
	FString Stance; // patrol, raid or port
	int32 Battleships = 0, Carriers = 0, Cruisers = 0, Destroyers = 0, Submarines = 0;
	int32 Home = INDEX_NONE; // the sea its harbour lies on
	// A fleet known only from a report (the fog at sea): a flag and a rough size, its ships not counted.
	bool bReport = false;
	FString SizeWord; // "A few ships", "A squadron", "A fleet", "Submarines"
	// The aircraft its carriers bring.
	int32 WingFighters = 0, WingStrike = 0;
	TArray<int32> Reach; // the seas it can sail to this month (the player's own fleets only)
	TArray<int32> Bases; // the friendly ports it could make its new home
	TMap<int32, int32> Barred; // (ruling 86) the seas it would reach but for a gate shut to it, each with that gate
	int32 Ships() const { return Battleships + Carriers + Cruisers + Destroyers + Submarines; }
};

/** A sea in a fleet's reach, with what it would meet there. */
struct FFleetSea
{
	int32 Zone = INDEX_NONE;
	int32 Surface = 0, Subs = 0; // enemy surface warships and submarines known there
	FString Meet; // clear, battle or shy: what a patrol there would come to
	float Hunt = 0.0f; // the share of those submarines it would sink in a month if it hunted there
	float Torpedo = 0.0f; // the chance it loses a heavy ship to a submarine there this month
	int32 Reported = 0; // enemy forces there known only from a report, their strength not counted
	FString Search; // how well the player's side has that sea searched, in a phrase
};

/** The ships of one class in a fleet, as its cards show them. */
struct FFleetShipClass
{
	FString Class; // bb, cv, ca, dd or ss
	FString Label, Role, Brief; // "Battleships", what they are for in a sentence, and in three words
	int32 Count = 0, Lost = 0;
};

/** The fleet in hand as the rules describe it, with what it could be told to do (the grand campaign). */
struct FFleetHand
{
	bool bValid = false;
	bool bGrand = false; // the navy as pieces: hunting, escorts, shelling, splitting and joining
	FString Doing, Admiral, Screen; // what it is doing; who commands; a warning if its heavy ships are thinly screened
	float Ready = 1.0f;
	int32 Guns = 0;
	bool bShelled = false; // its guns have spoken this month
	TArray<FFleetShipClass> Classes;
	TArray<FFleetSea> Seas;
	TArray<int32> Coasts; // enemy coasts it could shell from where it is
	// (Release 2, fleetattack.ts) Dropped on an enemy fleet, it attacks at once: the enemy fleets it can be sent at now,
	// whether that is switched on, and why it stays where it is this month once it has attacked.
	bool bFleetAttack = false;
	TArray<int32> Attack;
	FString Held;
	// (1.5.2) Cut off at sea beyond a shut strait: its card's line, and RETURN TO HARBOUR's tip where a harbour on its side
	// can take it (empty with none). Both empty when it is not cut off.
	FString Cut, HomeTip;
	// (Release 2; Brian, 26 November 2026 03:51Z) The fleets of countries at peace with him that he could declare war on
	// this month, each with the rules' reason it could not be sent at them even at war: empty where it could.
	TMap<int32, FString> Peace;
};

/** What else came of something pressed in a room, beyond its words. */
struct FActExtras
{
	int32 Shell = INDEX_NONE; // a coast just shelled
	int32 Made = INDEX_NONE; // a squadron just split off
	int32 Joined = INDEX_NONE; // the fleet another was just joined to
	int32 SeaFight = INDEX_NONE; // (release 2) the sea a fleet's attack was just fought in, or made in
	FString Outcome; // (release 2) what came of it: fought, missed (not found), declined (he made for port)
	TArray<FSinking> Sinkings; // (release 2) the ships that went down in it
};

/** Aircraft seen leaving for their target when a raid is ordered. */
struct FSortie
{
	bool bValid = false;
	int32 From = INDEX_NONE; // the province they fly from
	FVector2D To = FVector2D::ZeroVector; // where they are going, on the map
	int32 Region = INDEX_NONE; // the province bombed, if it is one
	FString Nation;
	bool bBombers = false;
	bool bEscort = false; // fighters go with the bombers
	int32 Flak = 0; // the level of the anti-aircraft guns round the target
	bool bLightFlak = false; // (release 2, the flak brief, 2c) no guns are built there, and its light flak is seen to fire: the bridge's "light"
	FString Meet; // the nation whose fighters rise to meet the raid; empty if none can
	int32 MeetFrom = INDEX_NONE; // the province whose airfield they rise from, if it is known
	// A carrier's aircraft fly from the ships, wherever they are on the map, not from an airfield.
	bool bFromSea = false;
	FVector2D FromPoint = FVector2D::ZeroVector;
	int32 FromFleet = INDEX_NONE; // the fleet itself, where that is known: the flight leaves from where its ships are drawn
	float Loss = 0.0f; // the share of the bombers likely to be lost
	bool bNight = false; // (ruling 107) flown by night: the place raided shows its searchlights, and the raid is watched in the dark
};

/** A button in one of the rooms the rules set out themselves. */
struct FPageButton
{
	FString Label;
	FString Act; // what to pass back when it is pressed
	FString Tip;
	bool bOk = false;
	bool bDanger = false; // something that cannot be undone
	bool bSafe = false; // (release 2, formations offered) the safe answer to a question asked in place on a page's head: lit, and Enter's and Esc's (ruling 134(f))
};

struct FPageRow
{
	FString Tag; // the nation whose flag stands beside it, if any
	FString Title;
	TArray<FString> Lines;
	TArray<FPageButton> Buttons;
	float Meter = -1.0f; // (rules version 71) a bar under the lines, 0 to 1: how far a test is met. None when less than nothing.
	FString Alarm; // (rules version 73) a line set out in red under the title: a warning that goes with the row
	FString Picture; // what to show a large picture of beside the row, if anything: "air:ftr:GER" is Germany's fighter, "air:bmr:UK" Britain's bomber
	TArray<FString> Tones; // (release 2, formations offered) how each of Lines is set where the row says it: "" plain, "muted" or "amber". With none, every line is set as it always was.
};

struct FPageSection
{
	FString Heading;
	FString Text;
	TArray<FPageRow> Rows;
};

/**
 * A room whose words and buttons the rules write and the game only draws: the secret service, the last act.
 * A new room in the rules needs nothing new here.
 */
/** (Ruling 54.) One air group on the raid card: what it is, where it flies from, how fit it is, and whether it goes. */
struct FRaidSeat
{
	int32 Id = INDEX_NONE;
	FString Name;
	FString Kind; // "ftr", "bmr" or "mixed"
	FString Nation;
	FString Base; // its airfield, or the fleet whose carriers it flies from
	FString Why; // why it cannot go; empty when it can
	FString Note; // what going means for it, in a few words
	FString Act; // what to pass back to add it or leave it out
	int32 Aircraft = 0;
	int32 Km = 0;
	FString Far; // (1.5.2) how far it has to fly, in the bridge's words: "1,240 km", or "Out of reach" for a carrier wing whose fleet's station is beyond the target
	bool bOutOfReach = false; // (1.5.2) the bridge sent no km: it cannot reach the target at all
	float Ready = 0.0f; // 0 to 1
	bool bPicked = false;
	bool bLocked = false; // it goes because it is ordered to stick with its bombers
	bool bLead = false;
	bool bAfloat = false;
	bool bStays = false; // (ruling 107(d)) an escort picked for a raid that is planned by night: it stays on the card and stays at home
	bool bDayOnly = false; // an escort that could join by day and cannot by night
};

/** A target on the raid card: the one being planned, or another in reach to step to. */
struct FRaidStop
{
	int32 Id = INDEX_NONE;
	FString Name;
	FString Sort; // factories, rail, harbour, airfield, works, troops, fleet, town
	FString Holder;
	FString Act;
	FString Picture; // what the studio is asked for: "Target:<sort>:<province or -1>:<holder>"
	FString Title; // its short title: its name, or for a fleet the sea it is in
	FString Who; // for a fleet, whose ships they are ("British ships"); empty otherwise
	int32 Km = 0;
	int32 Guns = -1; // the anti-aircraft guns round it, 0 to 3; -1 for a target that is not a place
	bool bNow = false;
};

/** An enemy fighter group that would rise to meet the raid. */
struct FRaidMeet
{
	FString Tag;
	FString Name;
	FString Base;
	int32 Fighters = 0;
	int32 Able = 0; // how many of them can reach the raid
};

/** (Ruling 107(d).) One way of flying a bombing raid, by day or by night: a whole small forecast in the rules' own words. */
struct FRaidWay
{
	FString Label; // BY DAY, BY NIGHT
	FString Why; // why it cannot be flown this way; empty when it can
	FString Verdict;
	FString Lost, FightersLost; // "2% · about 7"; the second empty where no fighters fly
	FString Foot;
	FString ToGuns, GunsWords; // the guns' part of the bombers lost: "about 23", "of them to flak and guns"
	FString StopNote; // "after the guns stop 30%"; empty with no guns
	FString Act; // what to pass back to choose it; empty for the one chosen
	FString Tip;
	float Success = 0.0f, LostShare = 0.0f, FighterShare = 0.0f;
	bool bNight = false;
	bool bChosen = false;
	TArray<TPair<FString, FString>> Hits; // a number, and a few words
	TArray<bool> HitDim; // a line printed only to stand beside the other column's
};

/** The raid card: the target as a picture, the forecast as numbers for bars, and a seat for every air group. */
struct FRaidCard
{
	bool bValid = false;
	FString Kind, KindLabel;
	FString Why; // why it cannot be flown as picked; empty when it can
	FRaidStop Target;
	FString What; // what is there, in a few words
	FString Effect; // what a hit would do, in one line
	FString Effects; // and in full, for the pointer
	TArray<TPair<FString, FString>> Hits; // up to three: a number, and a few words on what a hit does
	FString From; // the lead group's airfield, which the distance is counted from
	FString HolderName; // the country that holds the target
	FString AllAct, OtherAct; // the whole list of targets; another kind of raid
	int32 InReach = 0; // how many targets are in reach
	TArray<FRaidStop> Strip;
	TArray<FRaidSeat> Seats;
	TArray<FRaidMeet> Meets;
	float Success = 0.0f;
	FString Verdict;
	int32 Bombers = 0, BomberLoss = 0, Fighters = 0, FighterLoss = 0, FoeLoss = 0;
	float BomberShare = 0.0f;
	FString Massed, OffPatrol, Ticket; // for the pointer: what meeting up is worth, what it costs at home, the battle ticket
	FPageButton Fly;
	// (Release 2, atom.ts; the UI director under ruling 134(f).) A raid that carries the atomic bomb: BACK, the safe
	// answer, is the lit button and takes Enter and Esc; the raid takes a click only.
	bool bBomb = false;
	FPageButton Back;
	// (Ruling 107(d).) A bombing raid by day and by night, side by side: empty for any other kind of raid.
	TArray<FRaidWay> Ways;
	FString WaysLabel, WaysFoes, WaysFoesWords;
	int32 WaysBombers = 0;
	// (The defences brief.) The guns round the target: the level for the pips (0 to 3), "Level 2" and "AA guns".
	int32 WaysGuns = 0;
	FString WaysGunsFigure, WaysGunsNoun, WaysGunsTail, WaysGunsTip;
	FString GunsWords, ToGuns; // on the single forecast of any other raid: the one line under the bombers lost
	// (Release 2.) The fighters at the raid's height, which side's aircraft are stronger, the pilots' training (under Units
	// decide), and the words for the pointer; each empty where the rules give none.
	FString Types, TypesEdge, Pilots, TypesTip;
	// (Release 2, a raid on troops with the strafers' rule on; the UI director's words.) The fighter groups likely to go
	// down and strafe: the heading, the muted line once and a name a row, or the line for none in place of both (the
	// note then empty). bStrafe is false where the rules send none (the switch off, any other raid).
	bool bStrafe = false;
	FString StrafeHeading, StrafeNote, StrafeNone;
	TArray<FString> StrafeNames;
};

/** A figure on the month's card: "3,000" over "lost". */
struct FMonthFigure
{
	FString Value;
	FString Label;
};

/** A red band on the month's card: news that changes the war. */
struct FMonthBand
{
	FString Kind; // WAR, WARNING, FALLEN, FREED
	TArray<FString> Flags;
	FString Text, Tip;
	int32 Week = 0;
	FString When; // "week 1", or "July, week 1" for news dated after the month turned
	int32 Region = INDEX_NONE;
	// (Rules version 83.1.) A warning's own short title ("The country in danger"); a second line that is printed in
	// full under the headline; the sentence that says what to do, printed small where the date stood; a FREED by his
	// own side (a green edge); and a warning whose month of grace is spent (its label on cream, a cream edge).
	FString Title, Sub, Note;
	bool bGood = false, bSpent = false;
	// (Rules 84.2.6; ruling 211(c).) A warning that ends with the help of his home ground's first alarm: its SHOW ON MAP
	// is a press on the tray's red row, from the first alarm (it picks that army up and centres the town).
	bool bAlarm = false;
};

/** One box of the month's score. */
struct FMonthScore
{
	FString Label, Sign, Figure, Sub;
	int32 Tone = 0; // 1 the month went the player's way, -1 it did not, 0 neither
};

/** One of the player's fronts on the month's card. */
struct FMonthRow
{
	FString Picture, Sign, Place, Tag;
	int32 Tone = 0; // 1 good news, -1 bad, 2 something that blocks or warns
	FString A, B; // the two flags: his side's, then the other side's
	// (Rules 84.2.6.) The word that goes with the tag and the other side's name, so the row reads whole with no picture:
	// "WON from Mexico", "LOST to Mexico", "HELD against Canada", "BESIEGED by Canada". Empty where the rules give none.
	FString JoinWord, JoinName;
	FString Text;
	TArray<FMonthFigure> Figures;
	int32 Region = INDEX_NONE;
	int32 Sea = INDEX_NONE; // news from the sea: its place on the chart
};

/** The month's lead story. */
struct FMonthLead
{
	bool bValid = false;
	FString Tag, Stamp;
	int32 Tone = 0;
	FString Headline, A, AName, B, BName, Picture, Line;
	TArray<FMonthFigure> Figures;
	int32 Region = INDEX_NONE;
	int32 Report = INDEX_NONE; // the battle report to read, where it was a battle on land
};

struct FMonthHome
{
	FString Sign, Count, Text, Where;
	bool bGrey = false; // next month's, not this month's
	FString Tip; // the whole line of news, where the card gives it short
};

struct FMonthStaff
{
	FString Text, Act; // (the button is on a desk's first line only: no act, no button)
	bool bOk = true; // false once the desk has been taken back
	FString Tip;
};

struct FMonthElse
{
	FString Tag, Text;
	FString Tip;
};

/** (Rulings 64 and 65.) The month on one card: it takes the place of the typed strip and of the month's report. */
struct FMonthCard
{
	bool bValid = false;
	/** (Rules 84.3.3; ruling 123(b).) The card that opens a campaign, headed by the moment: "As September 1939 opens". */
	bool bOpening = false;
	FString Date;
	TArray<FMonthBand> Bands;
	TArray<FMonthScore> Score;
	FMonthLead Lead;
	TArray<FMonthRow> Rows;
	FString None, Foot, Count;
	FString Rest; // what is said at the card's foot of the news that found no line
	TArray<FMonthHome> Home;
	TArray<FMonthStaff> Staff;
	TArray<FMonthElse> Elsewhere;
	/** (Release 2, formations offered: the UI director's marks of 25 November 2026.) The one row for the formations offered to the
	 * player, first under the red bands: its sentence, whether one is in its last month (amber), and the act a press sends
	 * ("open:offers"). No row where the sentence is empty: nothing is offered, or the offers are switched off. */
	FString OffersText;
	bool bOffersAmber = false;
	FString OffersAct;
};

/** (Rules 84.3.3) One of FIRST COMMAND's staff notes, a tip or the closing note, in the rules' words for this game. */
struct FLessonNote
{
	/** "lesson", "tip" or "closing". */
	FString Kind;
	FString Id;
	/** Its place in the list, from 1, and how many there are (0 for a tip or the closing note). */
	int32 N = 0;
	int32 Of = 10;
	FString Head;
	FString Text;
	/** The deed line under the sentences ("Send one army by rail."); empty for lesson 1, a tip and the closing note. */
	FString Deed;
	/** The deed by its name in the list ('pick', 'march', 'rail', ...), and the moment it came due by ('army', 'train', ...). */
	FString Does;
	FString Due;
	/** Where the pointer stands on the map for a lesson that points at a flag: the army to pick up, one beside the enemy, the siege. */
	int32 Region = INDEX_NONE;
};

/** (Rules 84.3.3) What stands on the desk for FIRST COMMAND now, as the rules answer TC.lessonNow. */
struct FLessonDesk
{
	bool bValid = false;
	/** The notes are on, in a campaign with a player at the controls. */
	bool bOn = false;
	/** The campaign FIRST COMMAND opened. */
	bool bFirst = false;
	/** The note on the desk (a lesson, a tip or the closing note), if one is. */
	bool bNote = false;
	FLessonNote Note;
	/** Lesson 1, laid on the month card while it is open. */
	bool bLaid = false;
	FLessonNote Laid;
	/** Lessons stamped DONE since the game last asked: the note it shows takes the stamp. */
	TArray<FString> Stamped;
	/** Notes put away with LATER (the count on GUIDE), and lessons done. */
	int32 Waiting = 0;
	int32 Done = 0;
};

/**
 * (Rules 84.3.3; the art director's first-hour brief, section 3.) The one thing the staff note's pointer frames now:
 * nothing; a piece of the screen by its name (a button, or a line on a card); or a place on the map, as a box in
 * the viewport's pixels, which is drawn on the ground under every piece.
 */
struct FLessonAim
{
	enum EKind : uint8 { Nothing, Screen, Ground };
	EKind Kind = Nothing;
	/** On the screen: "giveorders", "line", "end", "send", "siege", "depth", "arsenal", "fly", "otherraid", "act:<the act>". */
	FName Name;
	/** On the ground: the frame, already 6 px clear of the thing it frames, in the viewport's pixels; and the ground under
	 * the thing in the world, whose height the frame is laid at. */
	FBox2D Box = FBox2D(ForceInit);
	FVector Foot = FVector::ZeroVector;
};

/** (Rules 84.3.3) A row of the Guide's list of the ten lessons. */
struct FLessonRow
{
	FString Id;
	int32 N = 0;
	FString Head;
	/** "done", "now" (waiting for him) or "later" (when its moment comes). */
	FString State;
	/** The first waiting row carries the kit's lit glow. */
	bool bLit = false;
};

/** (Rules 84.3.3) The Guide's first page: the ten lessons under their months, and the setting. */
struct FLessonGuide
{
	bool bValid = false;
	bool bFirst = false;
	FString Label, Head, State, Setting;
	bool bOn = true;
	int32 Waiting = 0;
	TArray<FString> GroupLabels;
	TArray<TArray<FLessonRow>> Groups;
};

/** (Rules 84.3.3; ruling 141) FIRST COMMAND's button on the door, and the campaign it opens. */
struct FLessonDoor
{
	bool bValid = false;
	FString Label;
	/** "New here? Learn as you play.", or "The ten lessons again." once all ten are done. */
	FString Line;
	bool bAllDone = false;
	FString Nation;
	int32 From = 1;
	FString Level, Depth;
};

// ---- The home front board (release 2; Brian, 25 November 2026) ----------------------------------------------
// (Brian: "I want the UI and user experience to feel seamless and easy to manage the cities, recruitment, ect".) One
// room for every town, raising forces and everything on order, as the rules set it out (campaign-bridge.ts, homeBoard).

/** A building that can be ordered in a town from the board: the staff's pick for the town, or one of the others. */
struct FHomeBuild
{
	FString Line, Name, Label, Effect;
	FString Why; // why BUILD is shut, where it is
	FString Act; // the town plan's own act, "civic:<region>:<line>"
	int32 Cost = 0, Months = 0;
	bool bOk = false;
};

/** A plot of a town, as a mark on its row. */
struct FHomePlot
{
	FString State; // "built", "going" or "free"
	FString Line, Name;
	int32 Level = 0;
	int32 Months = 0; // months left, where something is going up
};

/** (Release 2, a queue for a town: townqueue.ts) A building queued in a town, in its order, with the press that takes it off. */
struct FHomeQueued
{
	FString Line, Label, Act;
};

struct FHomeTown
{
	int32 Region = INDEX_NONE;
	FString Town, Province, Size;
	FString State; // "ready", "short" (only money is in the way), "going" or "done"
	FString Note; // why nothing can go up, where nothing can
	bool bOwn = true; // false on conquered ground
	int32 Pays = 0;
	TArray<FHomePlot> Plots;
	bool bBest = false;
	FHomeBuild Best;
	TArray<FHomeBuild> More;
	bool bGoing = false; // something is going up: its name, when it is due, and whether it can be taken back
	int32 GoingId = INDEX_NONE, GoingMonths = 0;
	FString GoingName, GoingDue, GoingAct;
	bool bGoingCancel = false;
	// (Release 2, a queue for a town; empty with that switch off) What is queued here, how the first stands
	// ("Waits for 40 more.", "Goes up after the market hall."), and what could be queued next, the staff's pick first.
	TArray<FHomeQueued> Queue;
	FString Wait;
	TArray<FHomeBuild> Then;
};

/** Something the arsenal offers that is raised and not built in a province: a corps, a wing, a ship. */
struct FHomeRaise
{
	FString Key, Group, Label, Where, Note, Why, Act;
	int32 Cost = 0, Months = 0;
	bool bOk = false;
};

struct FHomeOrder
{
	int32 Id = INDEX_NONE, Region = INDEX_NONE;
	FString What, Name, Where, Due, Act;
	int32 Months = 0, Cost = 0;
	bool bCancel = false;
	float Share = 0.0f; // how far along it is, 0 to 1
};

struct FHomeTile
{
	FString Kind, Label, Figure, Sub;
};

// (Release 2, the desk split) Recruitment or towns and trade on the board's staff strip: who runs it, and the press that changes hands.
struct FHomeDesk
{
	FString Name; // "RECRUITMENT", "TOWNS AND TRADE"
	bool bOn = false; // the staff runs it
	FString Act, Tip; // the press on the side that is not lit
};

struct FHomeBoard
{
	bool bValid = false;
	FString Nation, Date;
	bool bUp = false; // his month
	int32 Money = 0, Net = 0;
	TArray<FHomeTile> Tiles;
	int32 Ready = 0, Going = 0, All = 0;
	int32 Waits = 0; // (release 2, a queue for a town) towns whose first queued building waits for money
	TArray<FHomeTown> Towns; // richest first, as the staff ranks them
	TArray<FHomeRaise> Raise;
	// ORDER AGAIN: his own orders of the last month he ordered in, at today's prices.
	bool bAgain = false, bAgainOk = false;
	FString AgainWhen, AgainText, AgainLeft, AgainWhy, AgainAct;
	int32 AgainCost = 0;
	TArray<FHomeOrder> Orders; // soonest ready first
	// The desk that builds and raises: who runs it, the button that changes hands, and the share the staff may spend.
	bool bStaff = false, bStaffOn = false;
	FString StaffLine, StaffLabel, StaffAct, StaffTip;
	int32 StaffCap = 0;
	TArray<int32> StaffCaps;
	// (Release 2, the desk split) The strip's heading and its two halves; where the rules send none, the one button above.
	FString StaffHead;
	TArray<FHomeDesk> StaffDesks;
};

struct FOfficePage
{
	bool bValid = false;
	FRaidCard Card; // (ruling 54) set where the page is a raid being planned
	FMonthCard Month; // (ruling 64) set where the page is the month's report
	FHomeBoard Home; // (release 2) set where the page is the home front
	FString Id;
	FString Title;
	FString Headline;
	FString Note;
	TArray<FPageButton> Top;
	TArray<FPageSection> Sections;
	int32 Revision = 0;
};

/** One of the two stocks a great power keeps in the grand campaign: oil, or supplies. */
struct FViewStock
{
	float Stock = 0.0f;
	float Cap = 0.0f;
	float Net = 0.0f; // what the stock gains or loses a month
	int32 Months = -1; // months until the depots are empty at this rate; -1 when the stock is steady or growing
	bool bShort = false; // the need was not met this month
	FString Tip; // the books in words
	FString Word; // for the reserve of men: how it stands in a word ("Ample", "Thinning", "Short", "Spent"); empty for oil and supplies
};

/** A shipping lane drawn over the sea. */
struct FViewLane
{
	TArray<FVector2D> Path; // on the map
	FString State; // open, raided or cut
	float Flow = 1.0f; // the share of its cargoes getting through
};

/** (Release 2, columns.ts) Where a supply column was caught on the road: on the map, by whom, and how many lorries burned. */
struct FViewColumnHit
{
	FVector2D At = FVector2D::ZeroVector;
	bool bByAir = true; // enemy aircraft; otherwise partisans
	int32 Lost = 0;
};

/**
 * (Release 2, columns.ts) A month's supply column the player can see: the road from the railhead to the corps beyond
 * it, whose it is ("ours", "friend" or "theirs"), the lorries sent and lost, the share that got through, and where it
 * was caught. None while the columns are off.
 */
struct FViewColumn
{
	FString Nation;
	FString Side;
	TArray<FVector2D> Path; // on the map, railhead first
	int32 Lorries = 0;
	int32 Lost = 0;
	float Share = 1.0f;
	TArray<FViewColumnHit> Hits;
	// (Release 2, logistics.ts, each 0 or false where its switch is off) Loads carried by horse-drawn wagon; whether it
	// was stocking a forward dump; whether it set out from one.
	int32 Horse = 0;
	bool bFill = false;
	bool bFromDump = false;
};

/** (Release 2, logistics.ts) A forward dump the player can see: where, whose, the division-months in it, and whether it is still being brought up. */
struct FViewDump
{
	FString Nation;
	FString Side;
	FVector2D At = FVector2D::ZeroVector;
	int32 Region = INDEX_NONE;
	int32 Stock = 0;
	bool bFilling = false;
};

/** An air fleet on its airfields. */
/**
 * (Ruling 86.) A sea gate: the Panama Canal or one of the straits. Where its mark stands, who holds it, whether the
 * player's ships may pass, and the rules' own words for why: what comes before the marked words, the marked words
 * ("Open to your ships." or "Shut to your ships:"), and what comes after.
 */
struct FViewGate
{
	int32 Id = INDEX_NONE;
	FString Name;
	FVector2D At = FVector2D::ZeroVector; // on the map
	FString Holder;
	bool bOpen = true;
	FString Lead, Mark, Rest;
	FString Shut; // for a fleet's route that is refused: "The Panama Canal is shut to your ships: ..."
	// (Rules 84.1; ruling 176(e).) The order the player may give as its holder ("shut", "open", or nothing for anyone
	// else), the slip's last line for it, and where its holder has shut it, who did and to whom.
	FString Order, OrderLine, ShutLine;
};

/** (Rules 84.1; ruling 176(e).) The gate's question for its holder: what the order does, and whose trade it costs, as rows. */
/** (The design director, 17 November 2026.) One way up to an enemy army that is not next door, for the approach order. */
struct FApproachRow
{
	int32 Region = INDEX_NONE;
	FString Name;
	int32 Cost = 0, Left = 0;
	bool bAttack = false; // a move is left on arrival and nothing bars the way in: the attack card opens there
	FString Arrive; // "1 move left on arrival: you can attack this month"
	FString How; // its river and ground in a word
	FString Says; // enemy ground on the way: what the march does there
	TArray<int32> Road; // the provinces marched through, the approach itself last
	TArray<int32> Going; // (the code checker, 17 November) the corps of the hand that can reach it this month: they alone go
	FString River; // (sheet 34) the river between it and the enemy, by name, or empty: "across the Vistula" is said in the bright ink
	FString RiverWords; // "no river to cross", or "across the Vistula" (", bridges down")
	FString Ground; // the rules' key for its ground; the card prints the game's own word for it
};

/** The march up and the attack planned as one order: the game's pick first, then up to three more ways. */
struct FApproachPlan
{
	bool bOk = false;
	FString Name; // the enemy's province
	TArray<FApproachRow> Rows;
	FString None; // where nothing beside the enemy can be reached this month: the sentence that says so and names the nearest
	// (Sheet 34.) The card's two sides: where the road starts, the hand in numbers, and the enemy as far as the fog shows.
	FString From, Ours, Foe, FoeName, FoeLine;
};

struct FGateAsk
{
	bool bOk = false;
	FString Why; // why he cannot give the order, where he cannot
	FString Order, Label, Question, Lead, Heading, Total, None, Yes, No, Holder;
	TArray<FString> Rows; // "You: Your trade with X stops. -2.4 a month"
	// (Rules 84.2; ruling 177(a).) Who resents it, a row each ("Mexico: 1 point of opinion a month, 8 at most"), and what that comes to.
	FString ResentHeading, ResentNote;
	TArray<FString> Resent;
};

/**
 * (Rulings 110, 112 and 118; the art director's railway brief.) What the railway shows for the army in hand: the
 * second reach is light on the line itself.
 */
struct FRailLight
{
	int32 From = INDEX_NONE; // where the army stands
	TArray<FIntPoint> Tracks; // every working track a train could carry it along: the two provinces a line joins, the lower first
	TArray<int32> Dots; // provinces beyond its march that a train would take it to: a station dot each
	TArray<int32> Tint; // the provinces that take the faint cool tint
	TArray<FIntPoint> Bars; // a lit line that stops at a cut: the province it comes from, and the cut one
	TArray<int32> Ringed; // cut provinces that engineers in hand can be carried to: the lamp wears a white ring
};

/** What the trains will do for the army in hand this month: the rules' one line, and what the table lights. */
struct FRailShow
{
	bool bCan = false;
	FString Strong; // the opening that takes the railway's colour: "By rail this month to 3 more places:"
	FString Line; // the whole line
	FRailLight Light;
};

/** The pointer's words for a province beyond the march of the army in hand. */
struct FRailTip
{
	FString Ride; // "Right-click: by rail to Hanover, there this month. It takes the whole month."; empty where no train goes
	FString Relay; // "Engineers can relay the track here."
	FString No; // why no train will take it there
	FString Strong; // the opening of that reason that takes the emphasis, where it is a break he can act on
	bool bCutHere = false; // the reason is a cut at this very province
};

/** (Rulings 113 and 114.) A cut railway as the player knows it. */
struct FViewCut
{
	int32 Region = INDEX_NONE;
	FString How; // "now": seen as it is; "seen": as it was when last seen; "report": by his own doing, not seen since
	FString Head, Line, Short; // the rules' words: "Railway cut"; its sentence; "railway cut (bombed, September 1939)"
};

/** (Rulings 153 and 169.) One piece of work an agent can be given where he stands, with its odds in whole percents and the rules' reason where he cannot. */
struct FAgentWork
{
	FString Id, Label, Text, Why, Act;
	bool bOk = false, bNow = false;
	int32 Caught = 0, Success = -1; // (Success is under nothing for work that is not tried once: watching, reading, guarding)
};

/** An army one of his agents has in sight: where, whose, and how strong. */
struct FAgentSight
{
	int32 Region = INDEX_NONE;
	FString Name, Nation;
	int32 Corps = 0, Divisions = 0;
};

/**
 * (Rulings 153, 163 and 169.) One of the player's agents: a piece on the map that he alone sees, and everything its
 * card says. Every sentence is the rules' own.
 */
struct FViewAgent
{
	int32 Id = INDEX_NONE, Region = INDEX_NONE;
	FString Name, Rank, Toward; // "Halberd"; "Agent"; the rank he works toward, or empty at the top
	int32 Skill = 1, Ranks = 3, Xp = 0, Next = 0;
	FString Where, Host, HostName; // the province; the country that holds it, by its tag and by its name
	bool bAbroad = false, bAtWork = false;
	int32 Wait = 0, Caught = 0, Rest = 0; // months until he is at work; the chance he is caught this month, in percent; months he must lie low
	FString Mission, Doing; // what he is doing, by its id and in the rules' words
	FString Status; // what he is doing without the place, as the room's status line says it: "watching and reporting"
	FString Stuck; // the rules' reason why he cannot be sent anywhere now; empty when he can
	FString Home; // for an agent at home: the threat he guards against, in a sentence
	FString Will; // what the work he does (or will take up) is, in the rules' words
	TArray<FAgentWork> Work;
	TArray<FAgentSight> Sees;
};

/** (Ruling 159.) Where one of his agents was caught this month: a mark stands there until the month turns, with the rules' sentence on the pointer. */
struct FAgentMark
{
	int32 Region = INDEX_NONE;
	FString Text;
};

/**
 * (Release 2, the spies thread of 24 November 2026: Brian played and saw no spies.) An enemy agent his hunters have
 * uncovered on his ground and not yet arrested or turned: a piece on the map in its country's colours. A press on it
 * opens THE SECRET SERVICE, where it is arrested or turned; the rules' sentence is on the pointer over its province.
 */
struct FViewFoeAgent
{
	int32 Id = INDEX_NONE, Region = INDEX_NONE;
	FString Nat; // the service it works for
	bool bWatch = false; // a watcher (it maps his defences) rather than a saboteur
	FString Text;
};

/**
 * (Rulings 88, 101 and 102.) The round of armies as the rules count it, the same in both builds: Bright on NEXT
 * ARMY's disc (untouched armies at the front, and armies raised this month with no order), Quiet for the rear, Front
 * for how many of the bright stand at the front, All for every army Tab still goes to, and Idle for the question at
 * the month's end.
 */
struct FViewRound
{
	int32 Bright = 0, Quiet = 0, Front = 0, All = 0, Idle = 0;
	FString Words; // the bright count in the rules' words: "10 at the front and 1 newly raised"
	FString Question; // the headline of the question before the month ends: "11 armies still have moves left: ..."
};

/** An airfield as the player knows it (the grand campaign). */
struct FViewAirfield
{
	int32 Region = INDEX_NONE;
	FString Nation; // who holds it
	FString Side; // own, ally, foe or other
	FString Word; // "Landing ground", "Airfield", "Air base"...
	int32 Size = 1; // 1 to 4
	int32 Holds = 0; // how many aircraft it serves
	int32 Groups = 0, Aircraft = 0; // -1 where the fog of war hides what flies from it
	bool bCrowded = false;
	// (Release 2; the art director's airfield spec of 24 November, item 2.) Where what flies from it is known: its runway
	// shut, a repair in hand, and "strip" or "full" where airfields are targets (empty where the rules do not say).
	bool bShut = false, bMending = false;
	FString Tier;
	// (Brian, 24 November 2026: "when i have a unit on an allied airfield there is no way to know its there".) The player's
	// own groups among them, on a field another country holds: how many, their aircraft, and his nation (empty where none).
	int32 YourGroups = 0, YourAircraft = 0;
	FString YourNation;
};

struct FViewAirFleet
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	int32 Base = INDEX_NONE;
	int32 Fighters = 0, Bombers = 0;
	FString Mission;
	int32 Groups = 1; // how many air groups this piece stands for (one piece for each airfield in the grand campaign)
};

/** A province marked on the map for its prizes (Version 56): something there burns, or an army holds what lies outside a town it has not taken. */
struct FPrizeMark
{
	int32 Region = INDEX_NONE;
	FString By; // the tag of the army that holds prizes outside the town, or empty
	bool bBurning = false;
};
/** One of the six desks of the grand campaign (rules version 66): on the player's own desk, or run for him by his staff. */
struct FViewDesk
{
	FString Id; // "research"
	FString Name; // "Research"
	bool bStaff = false; // true while the staff runs it
	FString Line; // where the staff runs it: who does, and their news of the month
	FString Label; // the button that changes hands: "TAKE IT BACK", or "LET MY STAFF RUN RESEARCH"
	FString Act; // what the button tells the rules
	FString Tip;
};

/**
 * (Rules 84.2.6; ruling 211(c); the UI director's sheet 35.) The alarm that names the cure: an enemy army on his home
 * ground, or a home town of his fallen or besieged. The row's sentences are the rules' own, in order, the first bold.
 */
struct FHomeAlarm
{
	FString Case; // stands, taken, storm or stores; (84.3) empty: his ground still in enemy hands with nobody in it
	int32 Region = INDEX_NONE; // where the enemy stands: the town a press centres
	FString By; // the enemy nation's tag
	TArray<FString> Lines; // the enemy and where; what it can reach next month (left out where nothing is); the help
	int32 HelpRegion = INDEX_NONE; // where the nearest army of his that can come stands: a press puts it in hand
	TArray<int32> HelpCorps; // and the corps of it a press picks up (the rules' own; all of them there where none are given)
	int32 SendTo = INDEX_NONE; // the province the paper says to right-click
	FString Hand; // and the paper's line then: "The army round Vancouver is in hand. Right-click Pittsburgh to send it."
};

/** (Rules 84.2.6; ruling 211(a) as passed at 16:54 UTC.) His own guns left alone round an enemy town: his siege's own row. */
struct FGunsAlone
{
	int32 Region = INDEX_NONE; // the town they stand round: a press centres it
	FString What; // guns, engineers, or both
	bool bRed = false; // red while infantry or tanks of his can reach the town this month; plain where none can
	FString Line; // the rules' sentence: "Only guns are left round Vancouver: send infantry or tanks this month, or the siege is raised."
	// (Rules 84.3.) On a red row: the infantry or tanks of his that can reach them this month, the nearest army's, which
	// a press picks up, and where they stand.
	TArray<int32> HelpCorps;
	int32 From = INDEX_NONE;
};

struct FCampaignView
{
	FString Rules; // "stand-in" until the real rules are bundled in
	int32 Turn = 0;
	FString Date;
	FString Human; // the nation the player leads
	FString Current; // whose month it is
	bool bHumanUp = false;
	bool bMud = false, bWinter = false; // the weather in the east this month
	FString WeatherText; // said on the date plate; empty in clear weather
	TArray<int32> WeatherGround; // the provinces it lies on
	bool bEvePlan = false; // on the eve of war, the staff has a plan for placing the army
	bool bAtWar = false; // the player's country is at war with somebody
	bool bEve = false; // the player is placing an army on the eve of war: marches at home only, no month passes
	bool bOpen = false; // free play: no nation follows history's plan
	bool bAutoDig = false; // whoever stands a month digs in unasked (the grand campaign): the orders are then stand, or fall back
	int32 DugFull = 3; // months until a dug-in army's works are finished
	FString Fallen; // the player's nation, once it has been beaten
	TArray<FString> RegionHolder; // by region id: the nation that holds it now
	TArray<FString> RegionOwner; // by region id: its lawful owner where that is not its holder (occupied ground); empty where it is
	/** A siege as the land's paint shows it: the besieger's colour comes over the field from the sides his side holds. */
	struct FSiegePaint
	{
		int32 Region = INDEX_NONE;
		FString By; // who stands round the town
		float Progress = 0.0f; // 0 when the siege is laid, 1 when the town is about to fall
		bool bClosed = false; // the ring is closed: the colour comes in from every side
		TArray<int32> From; // the neighbouring provinces the besieger's side holds
	};
	TArray<FSiegePaint> SiegePaint;
	TArray<FString> RegionStanding; // by region id: where its holder stands with the player (own, ally, enemy, neutral)
	TMap<int32, FString> Joins; // provinces where allied corps could be asked to join the player's army: how many, and whose ("2 Slovak")
	TMap<int32, FString> Folds; // (Version 64) provinces where two of the player's corps are worn thin and could be folded into one: their names
	TArray<int32> WorksLeft; // (Version 64) fieldworks standing empty on the player's side's ground: three numbers each, the region, tenths of a month of work, room for how many corps
	TArray<int32> Unseen; // provinces the player has no eyes on, under the fog of war
	TArray<int32> Glimpsed; // (ruling 154) provinces he only glimpses: the haze stays, and an army there is a flag and no more
	TArray<int32> FreeCapitals; // capitals of fallen countries the player would set free by taking them
	TArray<FViewAirfield> Airfields; // every airfield the player knows of (the grand campaign; empty in a scenario)
	TArray<FCityOrders> CityOrders; // what each of the player's cities has on order
	TArray<FCityGlance> Glance; // (release 2) each town's plate under its name: garrison, on order, going up, damaged
	TArray<int32> Going; // works going up from the arsenal, where the player can know it: flat pairs, the region and what (bits: 1 factory, 2 shipyard, 4 guns, 8 airfield, 16 fortifications, 32 field strip)
	TArray<int32> Wrecked; // factories not working (bombed, sabotaged, burnt), where the player can know it: flat pairs, the region and how many
	TArray<FIntVector4> RegionWorks; // by region id: factories built since the war began, shipyard level, anti-aircraft level, airfield level
	TArray<FViewCorps> Corps;
	FTreasury Treasury;
	TArray<FString> Declarable; // the countries the player could declare war on this month
	TArray<FString> League; // the members of the League, the third alliance
	int32 IdleTeams = 0; // research teams of the player's with nothing to do
	// What is still worth doing before the month is ended, besides moving the armies: a line, the screen that
	// deals with it ("generals", "research", "arsenal") and a word of explanation.
	struct FToDo { FString Screen; FString Text; FString Tip; int32 Tone = 0; /* (release 2) the rules' own standing for the folded list: 2 this month, 1 next, 0 read from the words */ };
	TArray<FToDo> ToDo;
	TArray<FHomeAlarm> Alarms; // (rules 84.2.6) the alarm's rows, nearest his capital first; empty until the rules have it
	TArray<FGunsAlone> Alone; // (rules 84.2.6) his own guns left alone round an enemy town; empty until the rules have it
	// The towns' own buildings, four numbers each: region, line (0 market, 1 farm, 2 school, 3 camp, 4 hospital,
	// 5 hall), the level that stands, and 1 while the next is going up.
	TArray<int32> Civic;
	// Where battles were lately fought, three numbers each: region, months since, and how big (1 to 3).
	TArray<int32> Scars;
	// Towns under siege that the player knows of, three numbers each: region, 1 if the ring is closed, months of stores left.
	TArray<int32> Sieges;
	TArray<int32> Bombed; // the provinces bombed this month or last, where the player can know of it: each stands under black smoke
	// For the night: the nations at war (their towns are blacked out), and the fronts as pairs of regions.
	TArray<FString> Blackout;
	TArray<int32> Fronts;
	TArray<FString> Wars; // every pair of nations at war, two by two: the front line is drawn between their holdings
	TArray<int32> Cold; // provinces with factories and nothing on order, where the player can know it: their chimneys stand cold
	TArray<FArmNote> Arms; // (release 1) what each arm is, for the bigger unit card
	FString LiftLine; // (rules version 72) the sea lift in a few words, for the fleet card's heading: what the shipping carries a month and what is left
	int32 DialStop = 0; // (rules version 70) where the history dial stands, 1 to 5: 0 where there is none
	FString DialName, DialUnder;
	// (release 1) The admiral's chart: each sea by how well the player's side has it searched (0 not, 1 thinly, 2 well, 3 in full view), and old sightings.
	TArray<int32> SeaSearch;
	TArray<FOldSighting> OldSightings;
	TArray<FWreckMark> Wrecks; // (release 1) where the big ships went down
	const FArmNote* FindArm(const FString& Kind) const { return Arms.FindByPredicate([&Kind](const FArmNote& Each) { return Each.Kind == Kind; }); }
	TArray<FPrizeMark> Prizes; // provinces to mark for their prizes: a fire where one is wrecked, a pennant where a besieger holds one
	TArray<FString> Alerts; // things to tell the player as his month opens
	TArray<FString> PageIds; // the rooms the rules set out themselves that are open to him
	TArray<FString> PageLabels;
	bool bStores = false; // the player's nation keeps oil and supplies (a great power in the grand campaign)
	// (Version 59) The reserve of men, where the rules keep one: Stock is the share left of what the war opened with.
	bool bMen = false;
	FViewStock Men;
	// (Version 59) The war cabinet can be switched off, and whether it is.
	TArray<FString> Allied; // the countries whose armies fight beside the player's
	bool bCabinetCan = false, bCabinetOff = false;
	bool bOfficeSecure = false; // (version 62) the player's government cannot fall, whatever the cabinet's trust
	// (Version 59) The town a directive of the war cabinet names, and the months left: for a small seal on the map.
	int32 SealRegion = INDEX_NONE, SealLeft = 0;
	// (Version 60) The town the player has promised the cabinet, and the months left: a second seal, in wax red.
	int32 VowRegion = INDEX_NONE, VowLeft = 0;
	FViewStock Oil;
	FViewStock Supplies;
	TArray<FViewLane> Lanes;
	TArray<FViewColumn> Columns; // (release 2, columns.ts) the month's supply columns the player can see
	bool bNickFlags = false; // (release 2, nicknames.ts) the names corps earn are shown on their flags
	bool bAirNames = false; // (release 2, airnames.ts) new air groups are named with their own air force's numbers and words
	bool bMarkedOn = false; // (release 2, marked.ts) the armies that took his towns are marked in red
	bool bFleetAttack = false; // (release 2, fleetattack.ts) a fleet dropped on an enemy fleet attacks it at once
	bool bBreakUp = false; // (release 2, breakup.ts) divisions can be split off a corps in the tray, and put back
	bool bTaskForce = false; // (release 2, taskforce.ts) a fleet can be made a task force: a standing job, refit, and back
	bool bFleetWhy = false; // (release 2, fleetwhy.ts) the admirals give a reason for each fleet they order
	bool bRearguard = false; // (release 2, rearguard.ts) a small corps can be ordered to hold as a rearguard
	bool bSupportTeams = false; // (release 2, teams.ts) machine-gun and mortar brigades can be raised, by every power
	bool bFromWait = false; // (release 2, arrivalwaits.ts) a dated arrival formed from corps in the field waits while they are at the front
	bool bRearBridges = false; // (release 2, rearguard.ts) a rearguard lays charges on the bridges behind it (the switch as set)
	bool bShadowRule = false; // (release 2, shadow.ts) a fleet of cruisers can shadow an enemy fleet and report where it goes
	bool bVoyageLog = false; // (release 2, voyage.ts) each fleet keeps a voyage log on its page
	bool bSailors = false; // (release 2, the sea field's sailor rules: battle/sea/ticket.ts) men go into the water when a ship goes down
	bool bOwnWater = false; // (release 2, the sea field's own water: battle/sea/ticket.ts) captains mind their own side's torpedoes
	bool bSeaMines = false; // (release 2, mines.ts) fleets can lay minefields at sea
	bool bHeavySeas = false; // (release 2, heavyseas.ts) gales blow at sea: fleets slowed, convoys late, shadows lost
	bool bShadowRelay = false; // (release 2, shadow.ts) a fresh squadron takes over a shadow whose fuel has run out (the switch as set)
	bool bSeekOnce = false; // (release 2, navy.ts) a fleet seeks battle at most once a month
	bool bBeatReach = false; // (release 2, navy.ts) a fleet whose base moves patrols a sea it can reach from its new harbour
	bool bAtomBomb = false; // (release 2, atom.ts) the atomic project: research rows, plants, a bomb carried by a raid
	bool bCarrierStrike = false; // (release 2, harbourstrike.ts) a carrier fleet's page offers a strike on an enemy harbour (the switch as set)
	bool bWingByDate = false; // (release 2, carriers.ts) a navy's carrier wing changes by date (the switch as set)
	bool bSettledTowns = false; // (release 2, buildings.ts) a conquered town held three months can put up every building
	bool bTownQueue = false; // (release 2, townqueue.ts) the player can queue the next buildings for his towns
	bool bCityDefences = false; // (release 2, citydefences.ts) searchlights, the guns by night by what they see, a garrison's town works (the town's guns on tanks: sub-switch flakGround, off)
	// (Release 2) The options page's two switches: shown in a grand campaign only; the columns can be thrown in its
	// opening month only.
	bool bR2Switches = false, bColumnsOn = false, bColumnsCan = false;
	// (Release 2, logistics.ts) The forward dumps in sight, and the three supply orders' switches as thrown (each set in
	// the opening month only, as the columns are, and each working only with the columns on).
	TArray<FViewDump> Dumps;
	bool bLorryShiftSet = false, bWagonsSet = false, bDumpsSet = false;
	// (Release 2, the spies thread of 24 November 2026) The secret war's one switch on the options page: whether any of
	// release 2's spying is on in this campaign, and whether it can still be switched (its opening month only).
	bool bSecretWar = false, bSecretWarCan = false;
	TMap<FString, FIntVector> DesignYears; // by nation: the year its tank, fighter and bomber designs have reached
	FString Level; // the difficulty the grand campaign is on ("Colonel"); empty in a scenario
	// (rules version 66) How deep the player plays: the depth's name ("Theatre command", or "Your own mix"), the plain
	// word under it, how many of the six desks his staff runs, and the desks. Empty outside the grand campaign.
	FString CommandName, CommandUnder;
	int32 Staffed = 0;
	TArray<FViewDesk> Desks;
	TArray<int32> BridgesDown; // crossings (by their place in the land's list) whose bridges are blown
	TArray<int32> RailsCut; // regions whose railway is cut, as far as the player knows
	TArray<FViewCut> Cuts; // (rulings 113 and 114) the same, with how each is known and the rules' words for it
	TArray<FViewAgent> Agents; // (ruling 153) his agents: pieces on the map
	TArray<FAgentMark> AgentMarks; // (ruling 159) where one of his was caught this month
	TArray<FViewFoeAgent> FoeAgents; // (release 2) the enemy agents his hunters uncovered, still at large: pieces on the map
	int32 SpiesWaiting = 0; // (release 2) the number on the SPIES door: those enemy agents, and his agents newly at work this month
	TArray<FViewFleet> Fleets;
	TArray<FViewGate> Gates; // (ruling 86) the sea gates
	FViewRound Round; // (rulings 88, 101, 102) the round of armies
	TArray<FViewAirFleet> AirFleets;
	TArray<FViewAirFleet> BasedGroups; // (release 2) every air group as the rules list it, before the table's pieces are made one an airfield

	const FViewCorps* FindCorps(int32 Id) const
	{
		return Corps.FindByPredicate([Id](const FViewCorps& C) { return C.Id == Id; });
	}
};

/** One region an army could go to this month. */
struct FReachRegion
{
	int32 Region = INDEX_NONE;
	int32 Cost = 0;
	int32 Previous = INDEX_NONE; // the region it would be entered from
	bool bAttack = false; // enemy-held: going there means a battle
	bool bTake = false; // (rules 84.1) enemy ground with no army in its field: marching there is the order to take the place
	FString War; // the country at peace that holds it: going there means declaring war on it first
	bool IsWar() const { return !War.IsEmpty(); }
};

/** What declaring war on a country would mean, in the words the question is asked in. */
struct FWarPlan
{
	bool bOk = false;
	FString Name; // the country as it reads in a sentence: "the Netherlands"
	FString Lead; // where the country stands today
	FString Friends; // who its friends are
	TArray<FString> Lines; // what follows from saying yes
};

/** One line of a Battle Ticket: something the campaign map says, and what that does in the battle. */
struct FTicketLine
{
	FString Label;
	FString Value;
	FString Effect;
};

/** A corps as it is listed on one side of a battle. */
struct FBattleUnit
{
	FString Nation;
	FString Name;
	FString Kind;
	FString General;
	FString Fate; // after the battle: retreated, surrendered, destroyed, advanced, evacuated, yielded, or empty
	int32 Divisions = 0;
	int32 Men = 0;
	int32 Lost = 0;
};

/** The two armies drawn up against each other before an attack: who is there, the odds, and the Battle Ticket. */
/** (Rules version 84.) One line of WHAT YOU FACE: a sign (tank, anti-tank, machine-gun, mortar, sniper, gun, scouted) and a sentence. bUnknown marks the line for what is not scouted. */
struct FFaceLine
{
	FString Sign, Text;
	bool bUnknown = false;
};

struct FAttackPreview
{
	bool bValid = false;
	int32 Region = INDEX_NONE;
	float Attack = 0.0f;
	float Defence = 0.0f;
	float Odds = 0.0f;
	bool bExact = true; // false where the defenders are not all identified: the odds are then a band
	float OddsLow = 0.0f, OddsHigh = 0.0f;
	FString Note; // why the odds are a band, and how to narrow it
	TArray<FString> Allies; // the allied countries whose corps march in the attack
	FString Prize; // what taking the place is worth, in a sentence
	FString Refused; // why the troops there may not be attacked at all (they stand on a neutral's ground): then nothing else is filled in
	// (The UI director, 17 November.) Why the corps picked cannot go in by themselves (guns or engineers alone, ruling
	// 211(a)), in the rules' sentence: the card still opens on them, with no forecast and this sentence at its foot.
	FString Alone;
	// (Rules 84.1; rulings 175(c), 177, 178.) A march onto enemy ground with no army in its field: what it meets
	// ("open", "gates", "guard", "garrison", "unknown"), the rules' sentence for it, and the slip's and the card's
	// words. Where the town's guard will fight (bForecast) the fields above carry the assault as the army arrives,
	// and these the same assault made ready, a month later.
	FString Meets, MarchLine, Town, Forces, OrderWord, GuardName, GuardLine, GuardWho, Favours, NowSays, LaterSays, LaterChanceWords;
	FString NowLabel, LaterLabel, WaitSays; // (rulings 182 and 183) the two blocks' headings, and the rules' sentence on what waiting brings
	bool bForecast = false, bLater = false, bLaterExact = true;
	float LaterOdds = 0.0f, LaterOddsLow = 0.0f, LaterOddsHigh = 0.0f;
	float Even = 1.2f; // the odds at which it is an even chance: 1.2 to 1 in the open, more for an assault on a town
	int32 ChancePct = -1, ChancePctLow = -1, ChancePctHigh = -1; // the chance of taking the place, in whole per cent (100 only when it cannot fail); -1 where the rules give none
	FString Mass; // where more corps stand in reach than were picked: what sending everyone would come to, in a sentence
	TArray<int32> MassIds; // ...and the corps to pick to make it so
	// (Version 63) The chance in words (a best guess with its limits where the band is too wide to plan on), what the
	// fight is likely to cost each side in men either way, and how the card opened: the corps it counts, a sentence
	// where the staff brought in more than were picked, and the corps that were picked.
	FString VerdictTone, VerdictWord; // (rules 84.1) the staff's verdict in the rules' own word, and how it reads: "win", "" or "loss"
	FString ChanceWords;
	TArray<FString> Cost;
	TArray<int32> Ids;
	FString Opened;
	TArray<int32> PickedIds;
	// (Version 64) Soften the line first: what the defenders' fieldworks are, what a probe just found, the probe by
	// one corps (its button, what it would do, whether it can be ordered, and who goes), and the bombers on the line
	// (the same, and the page of the raid planner that opens on it).
	bool bSoften = false;
	FString SoftenHead, SoftenDone;
	FString ProbeLabel, ProbeText;
	bool bProbeOk = false;
	int32 ProbeCorps = INDEX_NONE;
	bool bBomb = false, bBombOk = false;
	FString BombText, BombOpen;
	TArray<FTicketLine> Lines;
	TArray<FBattleUnit> Attackers;
	TArray<FBattleUnit> Defenders;
	TArray<FFaceLine> Face; // (Version 84) what the defence will do to what he brings, as far as his reports know it: four lines at most, and one for what is not scouted
};

/** One corps' march: the regions it passed through, ending where it now stands. */
struct FCorpsMove
{
	int32 CorpsId = INDEX_NONE;
	TArray<int32> Path;
};

/** What a battle came to. */
struct FAttackResult
{
	bool bFought = false;
	FString Note; // the result in a sentence
	FString Level; // "Decisive victory" and the like, from the attacker's side
	bool bTaken = false;
	bool bYielded = false; // the defenders fell back without a fight
	bool bSiege = false; // the country round the town was won, but the town holds: it is now besieged
	bool bFell = false; // (rules 84.1, a march to take a place) the place is the player's side's: it fell with the move, or to the assault
	FString Stands; // ...and what still holds the town where it did not: "guard", "garrison" or nothing
	int32 AttackerLost = 0;
	int32 DefenderLost = 0;
	int32 Prisoners = 0;
	FString Price; // (Version 63) what it cost each side in men, and the share of those sent in, in a sentence
	FString Paid, PaidSign; // (Version 84) who paid most of it and to what, in a sentence or two, with its sign (or none)
	float Odds = 0.0f; // the odds as they stood on the day
	TArray<FTicketLine> Lines; // the Battle Ticket as it was on the day, read in the past
	TArray<FBattleUnit> Attackers;
	TArray<FBattleUnit> Defenders;
	TArray<FCorpsMove> Moved; // the corps that followed up into the ground they won
};

/**
 * (release 2, the enemy's month near you; Brian, 24 November 2026) One arrow the watch of the others' month draws on
 * the table near the player's ground (Rules/near-moves.ts).
 */
struct FStepArrow
{
	int32 From = INDEX_NONE; // the province it came from; none where that is not known (a landing)
	int32 To = INDEX_NONE;
	FString Nation;
	uint8 Kind = 0; // 0 a foreign army's move, 1 an attack on his troops, 2 his own corps falling back
	bool bWar = true; // at war with him (red); at peace (amber)
	bool bAway = false; // (a move) away from his ground
	bool bTaken = false; // (an attack) the place was lost
	bool bSiege = false; // (an attack) the field was lost, the town holds
	bool bYielded = false; // (an attack) his troops gave ground without a fight, as ordered
	bool bAlly = false; // (an attack) a battle of his allies near him, not his own (Brian, 24 Nov 18:01Z): shown with its switch on
	bool bOurs = false; // (an allies' battle) his side attacked
	int32 Corps = 1;
	int32 Divs = -1; // where seen whole; -1 where only glimpsed
};

/** One computer-run nation's month, when it did something worth seeing. */
struct FMonthStep
{
	FString Nation;
	FString Note;
	TArray<int32> Focus; // the regions it happened in
	TArray<int32> Battles; // the regions fought over
	bool bMarchOnly = false; // nothing was fought for or taken: armies only moved
	TArray<FSortie> Raids; // the raids flown in it, each group from its own airfield
	TArray<int32> SeaFights; // the seas where fleets fought in it
	TArray<FSinking> Sinkings; // (release 1) and the ships that went down in them, where the player's side fought
	TArray<int32> Shelled; // the coasts shelled from the sea in it
	bool bMine = false; // the player's own ground or troops were involved
	// ---- (release 2, the enemy's month near you)
	bool bNear = false; // it came near his ground, or touched him: the watch of the enemy's month stops here
	FString NearNote; // what it did near him, in his side's words
	TArray<FStepArrow> Arrows;
	bool bNearAllies = false; // his allies fought near him in it (OPTIONS "Allies' battles near you")
	FString AllyNote; // what those battles were
	// ---- (end release 2)
	FCampaignView View; // the war as it stood when that nation had finished
};

/** A great moment told on a screen of its own: a nation eliminated, a nation liberated. */
struct FAnnouncement
{
	FString Tag; // whose flag
	FString Title; // "FRANCE HAS FALLEN"
	FString Text;
	bool bGood = false; // good news for the player
	bool bEnding = false; // (ruling 204) the campaign's ending, won or lost: it alone may come before the month's card unasked
	FString Kicker; // "Eliminated · June 1940"
	FString More; // what it means from here on: how the country can come back
	TArray<FString> FactLabels, FactValues; // three figures for the foot
	int32 Region = INDEX_NONE; // its capital, to look at afterwards
};

/** One story of the month's newsreel: whose, its headline, the words read out, and the place shown meanwhile. */
struct FNewsCard
{
	FString Tag;
	FString Title;
	FString Text;
	int32 Region = INDEX_NONE;
	float Zoom = 0.36f; // how close the place is looked at while it is told
	float Turn = 0.0f; // which way the eye is turned, in degrees, for a stop of the opening flight
};

/** The title card that opens a year: the year, and how the player stands as it begins. */
struct FYearCard
{
	bool bValid = false;
	FString Year, Title, Kicker;
	TArray<FString> Lines;
};

/** Everything that happened between the player ending one month and beginning the next. */
/**
 * (Release 2; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered".) A unit the player raised or
 * bought, handed over as his month opened: what it is ("corps", "air", "ship"), the rules' line of news, where it stands,
 * and for a corps, which one.
 */
struct FReadyUnit
{
	FString What;
	FString Text;
	FString Name;
	int32 Region = INDEX_NONE;
	int32 Corps = INDEX_NONE;
};

/**
 * (Release 2, the scramble offer; Brian, 24 November 2026 22:21Z: "i want every air battle playable".) The month stopped
 * where an enemy raid meets his fighters: the rules' "scramble" object (the contract, air-battles/every-kind/CONTRACT.md
 * section 7), carried by TC.endMonth's answer in place of the month's card. Who is coming ("GER", "German") and where
 * ("Kent", its province), the rules' plain words for the card, and the figures when the answer gives them (-1 when it does
 * not). bDemo: offered by the bridge's stand-in (Rules/scramble-demo.ts), which books nothing.
 */
struct FScrambleOffer
{
	bool bValid = false;
	FString Id;
	FString Kind;
	int32 Target = INDEX_NONE;
	FString Name;
	FString Nation;
	FString Adjective;
	FString Words;
	int32 Bombers = -1;
	int32 Escorts = -1;
	int32 Fighters = -1;
	TArray<FString> Groups;
	bool bDemo = false;
};

struct FMonthResult
{
	TArray<FMonthStep> Steps;
	TArray<FString> Headlines; // the month's news, most important first
	FString Fallen; // the player's nation, if it has just been beaten
	TArray<FAnnouncement> Announcements;
	FString ReelDate; // the month the newsreel is of
	FYearCard Year; // set when the month that opens is a January
	TArray<FNewsCard> Reel; // the month's stories; empty in a quiet month
	TArray<FReadyUnit> Ready; // (release 2) the units handed over to him as his month opened
	FScrambleOffer Scramble; // (release 2) set when the month stopped on an enemy raid he can meet: the rest waits on it
};

/** One way into a campaign: a nation to lead (or nobody, to watch), with a line about what lies ahead. */
struct FStartSide
{
	FString Nation; // empty to watch
	FString Name;
	FString Blurb;
	TArray<FString> Aims; // the short and the long campaign, for the starts that have them
};

/** A point in the war a campaign can begin at, and the sides that can be taken there. */
struct FStartGroup
{
	FString Title;
	int32 From = 1; // the month the player takes over; 1 is September 1939
	FString ViewMode; // "open" for free play
	TArray<FStartSide> Sides;
};

/** A day the grand campaign can be taken over on. */
/** How hard a grand campaign can be made. */
struct FGrandLevel
{
	FString Id; // "colonel"
	FString Name; // "Colonel"
	FString Blurb;
	TArray<FString> Lines; // what it changes
	FString Under; // (a depth only) the plain word under its name: "Simple", "Standard", "Full"
	int32 Stop = 0; // (a stop of the history dial only) its number, 1 to 5
};


struct FGrandYear
{
	FString Year; // "1939"
	FString Title; // "September 1939"
	FString Note; // how the war stands that day
	int32 From = 1;
};

/** A country that can be led in the grand campaign from a given day. */
struct FGrandNation
{
	FString Nation;
	FString Name;
	FString Blurb; // a word about a great power; empty for the rest
	FString Standing; // "At peace", or who it is at war with
	bool bMajor = false;
	bool bSmall = false; // a small country whose path is its own to choose
	TArray<FString> Abilities; // what the country does in its own way, and its special formation, a line each
	int32 Corps = 0;
	int32 Regions = 0;
};

/** A heading and what stands under it, for a page of text shown to the player. */
struct FDocumentSection
{
	FString Heading;
	FString Body;
};

/** A page shown over the table: a unit's service record, or the roll of honour. */
struct FDocumentPage
{
	bool bValid = false;
	FString Title;
	FString Subtitle;
	TArray<FHonourRibbon> Ribbons;
	TArray<FDocumentSection> Sections;
	FString Door, DoorLabel; // a room that leads on from this page, and what its button says
};

/** Something the player can order in a territory: a kind of corps to raise, or works to build. */
struct FProvinceOption
{
	FString Action; // inf, mtn, mot, arm, or fort
	FString Label;
	FString Detail; // what it costs and how long it takes
	FString Why; // why it cannot be ordered now, if it cannot
	bool bEnabled = false;
	// For a corps to raise: what it costs, how long it takes, how many divisions, and what it costs to keep.
	int32 Cost = 0, Months = 0, Divisions = 0, Keep = 0;
};

/** One thing the arsenal offers: a corps, a wing of aircraft, ships, or a work for a province. */
struct FArsenalOffer
{
	FString Key; // "corps:arm", "air:ftr", "ship:dd", "work:aa": what to pass back to order it
	FString Group; // army, air, navy or works
	FString Kind; // (Version 64) for a cadre: the kind of corps it is, for its picture
	FString Label;
	FString Blurb;
	FString Size; // "2 divisions", "100 fighters", "level 2"
	FString Note; // where it goes and what it joins
	FString Why; // why it cannot be ordered now; empty when it can
	int32 Cost = 0;
	int32 Upkeep = 0; // each month once delivered
	int32 Months = 0;
	bool bPoor = false; // money is the only thing in the way
};

/** Everything the player's nation can order, with one province in mind. */
struct FArsenal
{
	bool bValid = false;
	bool bBudget = false;
	int32 Money = 0;
	FString Region; // the province in mind; empty for none
	TArray<FArsenalOffer> Offers;
	TArray<FLedgerLine> Orders; // what is already on order
	int32 Revision = 0;
};

/** One of the powers at war, as the foreign office lists it. */
struct FDiplomacyPower
{
	// (Version 65) Its general staff as the player knows it: its character in a word, what it is doing in a sentence,
	// how it fights (for a tooltip), and whether a main effort of its is aimed at the player or his allies, and where.
	FString StaffLabel, StaffLine, StaffSays;
	bool bThreat = false;
	int32 ThreatRegion = INDEX_NONE;
	FString ThreatName;
	FString Tag;
	FString Name;
	FString Side; // own, ally, enemy or other: how it stands to the player
	FString Note; // who it is at war with, or when it fell
	int32 Divisions = 0;
	float Will = 0.0f; // how much fight it has left, 0 to 1
	bool bAlive = true;
	bool bWar = false; // the player could declare war on it this month
};

/** A country at peace, and where it stands between the two blocs. */
struct FDiplomacyRow
{
	FString Tag;
	FString Name;
	FString Label; // "Leaning Axis"
	FString Word; // how it would take to being courted
	FString Why; // why it cannot be courted now, when it could otherwise
	FString Friends;
	int32 Opinion = 0; // -100 the Allies ... +100 the Axis
	int32 Divisions = 0;
	bool bCourt = false; // it is open to courting at all
	bool bCanCourt = false; // and the player can afford to this month
	bool bTrades = false; // it trades with the player today
	bool bWar = false; // the player could declare war on it this month
};

/** One of a general's four skills. */
struct FGeneralSkill
{
	FString Id; // offense, defense, logistics or leadership
	FString Label;
	FString Blurb;
	int32 Value = 0;
	int32 Max = 0;
	bool bMore = false; // he has a point to spend and room for it here
};

/** One of the player's generals: in command, waiting for one, or recovering from wounds. */
struct FGeneralEntry
{
	int32 Id = INDEX_NONE;
	FString Name;
	FString Rank;
	FString Where; // "Commands XIX Corps (armor, 2 divisions) at Farther Pomerania", or "Waiting for a command"
	FString Record;
	int32 Level = 0;
	int32 Points = 0; // skill points not yet spent
	float Progress = 0.0f; // toward the next level, 0 to 1
	bool bCommands = false;
	bool bWaiting = false;
	bool bRelieve = false; // he can be taken off his corps this month
	bool bSack = false; // he can be dismissed for good
	bool bPopular = false; // and the army would take it badly
	TArray<FGeneralSkill> Skills;
	TArray<FString> Traits; // "Panzer leader: ..."
	TArray<bool> TraitGood;
};

/** A corps with no general, and what can be done about it. */
struct FVacantCorps
{
	int32 Corps = INDEX_NONE;
	FString Name;
	FString Line; // "Infantry, 4 divisions, at Saar"
	int32 BestId = INDEX_NONE; // the waiting general who suits it best, if one may take it this month
	FString BestName;
	bool bPromote = false; // an officer can be promoted from its ranks now
	FString WhyPromote;
};

/** The generals of the player's nation. */
struct FGeneralStaff
{
	bool bValid = false;
	bool bUp = false;
	FString Nation;
	FString Date;
	FString Note;
	int32 Capital = 0;
	int32 PromoteCost = 0;
	int32 Commanding = 0;
	int32 Waiting = 0;
	TArray<FGeneralEntry> Generals;
	TArray<FVacantCorps> Vacant;
	int32 Revision = 0;
};

/** A design's date in history, as a mark along a branch's strip of the war. */
struct FResearchMark
{
	float At = 0.0f; // 0 at September 1939, 1 at the end of the strip
	bool bGot = false; // the designers have it
	FString Name; // "Tiger: August 1942 in history"
};

/** One branch of the armed forces in the design offices. */
struct FResearchField
{
	FString Id; // armour, infantry, doctrine, fighters, bombers, navy, radar, industry
	FString Label;
	FString Pace; // "3 months ahead"
	FString Now; // the design in service
	FString Next; // what comes next, and when
	FString Effect; // what being ahead or behind is worth in battle; empty when level with history
	FString Work; // what this many teams means
	int32 Ahead = 0; // months ahead of history, negative when behind
	int32 Teams = 0;
	int32 Max = 0;
	bool bGood = false;
	bool bMore = false; // another team can be put on it
	bool bFewer = false; // a team can be taken off it
	float Fill = 0.0f; // how far the designers have come along the strip
	float Today = 0.0f; // where history's own clock stands
	TArray<FResearchMark> Marks;
};

/** One technology of the grand campaign's tree. */
struct FTechNode
{
	FString Id;
	FString Branch; // land, air, naval, industry or doctrine
	int32 Tier = 1; // 1 to 4
	FString Name;
	FString Text; // what it is
	FString Rule; // what it does
	FString State; // done, work, ready or locked
	FString Needs; // the technologies it still waits on, in a line
	FString Why; // why a free team cannot begin it now
	int32 Months = 0; // a team's work if begun now
	int32 Left = 0; // for one in work: months still to go
	int32 Of = 0; // and the months it was begun with
	bool bCan = false;
	bool bStop = false;
	// (The research tree's chart.) Its year in history; a team's months in its own time (fewer than Months when it
	// is begun ahead of its time); every technology it needs, by id and by name, and whether he has it; and its tag
	// ("NEW UNIT", "+1 TEAM") until it is researched.
	int32 Year = 0;
	int32 Base = 0;
	TArray<FString> NeedIds, NeedNames;
	TArray<bool> NeedDone;
	FString Tag;
};

struct FTechBranch
{
	FString Id;
	FString Label;
	FString Text;
	int32 Done = 0;
	int32 Total = 0;
};

/** Another power's designs in service. */
struct FResearchPower
{
	FString Tag;
	FString Name;
	FString Doctrine;
	TArray<FString> Labels;
	TArray<FString> Designs;
};

/** The design offices of the player's nation, and what the other powers have in service. */
struct FResearch
{
	bool bValid = false;
	bool bUp = false;
	bool bOwn = false; // the nation does its own research
	FString Nation;
	FString Date;
	FString Doctrine;
	FString Note;
	int32 Teams = 0;
	int32 Free = 0;
	int32 Money = 0;
	bool bBureau = false; // another design team can still be founded
	bool bBureauCan = false; // and the treasury can pay for it now
	int32 BureauCost = 0;
	FString BureauWhy;
	TArray<FResearchField> Fields;
	TArray<FResearchPower> Others;
	// The grand campaign: the technology tree takes the place of the branches above.
	bool bTree = false;
	int32 Busy = 0;
	int32 Done = 0;
	int32 Total = 0;
	TArray<FString> Eras; // the name of each step of the tree, by tier
	TArray<FTechBranch> Branches;
	TArray<FTechNode> Nodes;
	int32 Revision = 0;
};

/** One of the paths open to a small country in the player's hands. */
struct FPathOption
{
	FString Id; // axis, allies, alone, found or join
	FString Title;
	FString Text;
	FString Why; // why it cannot be taken, or where the country stands on it
	bool bOk = false;
	bool bNow = false; // the path the country is on today
	bool bFinal = false; // an alliance, once joined, is not left
};

/** A country asked into the League: to found it with the player, or to join it. */
struct FLeagueAnswer
{
	FString Tag;
	FString Name;
	FString Why;
	bool bOk = false; // it would say yes
	bool bCan = false; // and the player can ask it today
};

/** The foreign office: political capital, the powers at war, and every country at peace on the track. */
struct FDiplomacy
{
	bool bValid = false;
	bool bUp = false; // it is the player's month
	FString Nation;
	FString Date;
	FString Bloc; // "the Axis", "the Allies", or empty
	FString Wind; // which way the neutrals are bending
	/** (rules version 70) One claim on named ground: who claims it, from whom and why, and how far it has gone (0 not pressed, 1 pressing, 2 at war for it). */
	struct FClaim
	{
		FString Title, From, Tag;
		int32 State = 0, Region = INDEX_NONE;
		bool bOnYou = false;
	};
	TArray<FClaim> Claims; // those on the player's own ground first
	FString ClaimsLead; // what the Powers view says above them: empty where there is nothing to say
	FString Help; // what courting does
	int32 Capital = 0;
	int32 Income = 0;
	int32 Cap = 0;
	int32 Partners = 0;
	int32 Cost = 0;
	TArray<FDiplomacyPower> Powers;
	TArray<FDiplomacyRow> Rows;
	// The League, once it exists.
	bool bLeague = false;
	FString LeagueName;
	FString LeagueSince;
	TArray<FDiplomacyPower> Members; // Side is "own" for the player
	// A small country's own path: empty for a great power.
	FString PathLead;
	TArray<FPathOption> Paths;
	TArray<FLeagueAnswer> Founders; // who would found the League with the player
	TArray<FLeagueAnswer> Invites; // who the player, a member, could invite
	int32 InviteCost = 0;
	int32 Revision = 0;
};

/** (Ruling 87.) The rules' plan for a march kept on a standing order, before it is ordered. */
struct FMarchPlan
{
	bool bOk = false; // it can be ordered
	bool bSpent = false; // the corps has no moves left: it sets off as the next month opens
	bool bNow = false; // an ordinary move reaches the place this month: nothing would be kept
	int32 Months = 0;
	TArray<int32> Road; // the place itself last
	FString Line; // what an order here would do, or why it cannot be given: the rules' own words
};

/** What a landing on an enemy coast would meet, and afterwards how it went. */
struct FLandingPlan
{
	bool bOk = false;
	FString Why; // why it cannot be made, if it cannot
	FString Name; // the coast
	TArray<FString> Lines; // the crossing, the sky, the defenders
	FString Warn; // (rules version 73) set out in red: the enemy could close the sea behind the force once it is ashore
	// (rules version 74) The headline, which speaks for the whole venture: "Ashore, then cut off" (red), "A costly
	// crossing" (amber), or how the fight for the beach stands; its tone ("red", "amber", "good" or none); and a line under it.
	FString Verdict, Tone, Under, UnderTone;
	// (rules version 83) The test of strength, the card's last note: its heading ("Three months on"), its line, and
	// how it is to be read ("holds", "even" or "lost").
	FString AshoreHead, AshoreLine, AshoreTone;
};

struct FLandingResult
{
	bool bOk = false;
	bool bTaken = false;
	FString Note;
	TArray<FCorpsMove> Moved;
};

/** A fleet or an air fleet, for the panel that opens when it is clicked. */
struct FForcePanel
{
	bool bValid = false;
	FString Name;
	FString Nation;
	bool bMine = false;
	TArray<FString> Lines; // what it has, where it is, what it is doing
	TArray<FProvinceOption> Options; // the orders it can be given
};

/** A river: its name and its course, in map units, from source to mouth. */
struct FLandRiver
{
	FString Name;
	TArray<FVector2D> Course;
	FString Into; // what it runs into, as the wider chart's file names it: another river, a lake or a sea. Empty where the rules do not say (Europe's)
};

/** Where a road and railway cross a river between two regions. */
struct FLandCrossing
{
	int32 A = INDEX_NONE, B = INDEX_NONE, River = INDEX_NONE;
	FVector2D At = FVector2D::ZeroVector;
};

/** A railway between two regions, and the point where it crosses their border. */
struct FLandRail
{
	int32 A = INDEX_NONE, B = INDEX_NONE;
	FVector2D At = FVector2D::ZeroVector;
};

/** The geography the rules know and the map file does not: rivers, crossings, railways, steep country. */
struct FLandFeatures
{
	TArray<FLandRiver> Rivers;
	TArray<FLandCrossing> Crossings;
	TArray<FLandRail> Rails;
	TArray<int32> Steep;
};

/** A province's chief town as the rules know it: its name, what the province pays a month, and how that ranks. */
struct FTownInfo
{
	int32 Region = INDEX_NONE;
	FString Name; // "Essen"
	FString Province; // "Ruhr"
	FString Size; // capital, city, town or village
	int32 Worth = 0; // money a month to its own country
	int32 Rank = 0; // 0 to 5
};

/** One building plot of a town: what stands on it, or nothing. */
struct FTownPlot
{
	FString Line; // market, farm, school, camp, hospital, hall; empty for a free plot
	int32 No = INDEX_NONE; // the same as a number, 0 to 5
	FString Name, Label; // "Commerce", "Market hall"
	int32 Level = 0;
	bool bBuilding = false; // the next level (or the first) is going up
	FString Due; // the month it is due
	FString Does; // what the levels that stand do, in a sentence
};

/** A line of building as it could be ordered in a town: the next level, what it costs and does, or why not. */
struct FTownOffer
{
	FString Line;
	int32 No = INDEX_NONE;
	FString Name, Label, Effect, Blurb, Why;
	int32 Level = 0, Has = 0, Cost = 0, Months = 0;
	// (Release 2, a queue for a town) Where BUILD cannot be pressed for the builders or the money: the press that queues
	// the line's next level, and that level's name. Empty with the queue switched off, or where nothing can be queued.
	FString QueueAct, QueueLabel;
	// The level that press queues (it can be a level above Level, behind what is going up or queued in the line), its
	// price and months, and the name of the level it goes up after ('' for a first level). 0 and '' where nothing can be queued.
	int32 QueueLevel = 0, QueueCost = 0, QueueMonths = 0;
	FString QueueAfter;
};

struct FProvinceInfo
{
	bool bValid = false;
	int32 Region = INDEX_NONE;
	bool bHome = false;
	FString Holdings; // industry, works, harbour, in a line
	FString Yield; // what the province gives each month, in a line
	FString Town, TownSize; // its chief town, and what kind of place that is
	int32 TownWorth = 0;
	FString PrizeKind; // yours, ally, taken or restored
	FString PrizeLine; // what taking it is worth (or what holding it pays), in a sentence
	TArray<FString> PrizeItems; // and in detail, a line each
	FString Note;
	TArray<FProvinceOption> Options;
	TArray<FOrderLine> Forming;
	FString FormingHead; // the total with its breakdown, where units are on order here
	FCityShelves Glance; // (release 2) the AT A GLANCE page: garrison, on order here, building and repairs
	// The town's building plots (the grand campaign): what stands on each or "Free plot", whether the town is the
	// player's to build in, and how many kinds of building could be ordered there now.
	bool bHasPlots = false, bPlotsMine = false;
	TArray<FString> Plots;
	int32 PlotsFree = 0, PlotsCanBuild = 0;
	// The same as things to draw.
	TArray<FTownPlot> TownPlots;
	TArray<FTownOffer> TownOffers;
	// (Release 2, build from the map) The line the staff would put up here next, and what is queued here, in a line.
	FString TownBest, TownQueued;
	// The town as a fortress, and how its siege stands (the grand campaign): who stands round it, and plain lines.
	FString SiegeBy;
	bool bSiegeOwn = false; // the town is the player's own (a town of his besieged: its head is the one in red)
	TArray<FString> SiegeLines;
	// (Version 64) Fieldworks standing empty here: a heading, and who can use them.
	FString LeftWorksTitle, LeftWorksText;
	// (Version 63) A province cut off from the rest of its country: a heading, what its troops live on and for how long.
	FString PocketTitle, PocketText;
	bool bPocketSpent = false;
	// The places of the province that are prizes of their own (Version 56), in plain lines.
	FString DirectiveLine; // a directive of the war cabinet that names this province, in a sentence
	FString PlacesHead;
	TArray<FString> PlacesLines;
	FString TownKind; // capital, city, town or village
	int32 YieldTotal = -1; // (release 2, the town card's tiles) what the province pays a month in all; -1 where the rules give none
};

/**
 * (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) A war the player kept
 * with SAVE on the menu, beside the campaign the game keeps by itself: as many as he likes, each a copy of the war as
 * it stood when he pressed it. They are kept in Saved/Campaign/Saves, a .json for the war and a .txt for its line.
 */
struct FSavedWar
{
	FString Id; // its files' name, without the ending: war-20261124-184012
	FString Line; // what it is, as CONTINUE says it: "Germany, July 1940, free play"
	FDateTime When; // when it was saved, by the computer's own clock
};
