#pragma once

#include "CoreMinimal.h"

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
	int32 Stale = 0; // known from a report this many months old (0: known as it is today)
	FString General; // empty when the corps has no named commander
	FString Posture; // stand, dig, back
	int32 Divisions = 0;
	int32 Men = 0;
	float Strength = 1.0f; // 0..1: men and equipment left
	float Cohesion = 1.0f; // 0..1
	int32 Region = INDEX_NONE;
	int32 Moves = 0; // weeks of action left this month
	int32 MovesMax = 0;
	bool bCanOrder = false; // the player may give this corps orders right now
	int32 Experience = 0; // 0 to 100
	FString Grade; // what that experience is called: Green, Trained, Seasoned, Veteran, Elite
	FString Nickname; // the name the army gives it, once it has earned one
	TArray<FHonourRibbon> Honours;
	// The player's own corps, where the rules keep them (the grand campaign): morale and rest with their words, and its divisions.
	float Morale = -1.0f; // 0 to 1, or -1 where no morale is kept
	float Rest = -1.0f;
	FString MoraleWord, RestWord, Trend; // Trend is rising, falling or steady
	FString MoodLine, RestLine, Service;
	TArray<FString> Why; // what has moved its morale, a line each
	bool bResting = false; // it will count as rested if it neither marches nor fights this month
	TArray<FViewDivision> Parts;
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
	TArray<int32> Reach; // the seas it can sail to this month (the player's own fleets only)
	TArray<int32> Bases; // the friendly ports it could make its new home
	int32 Ships() const { return Battleships + Carriers + Cruisers + Destroyers + Submarines; }
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
	FString Meet; // the nation whose fighters rise to meet the raid; empty if none can
	int32 MeetFrom = INDEX_NONE; // the province whose airfield they rise from, if it is known
	float Loss = 0.0f; // the share of the bombers likely to be lost
};

/** A button in one of the rooms the rules set out themselves. */
struct FPageButton
{
	FString Label;
	FString Act; // what to pass back when it is pressed
	FString Tip;
	bool bOk = false;
	bool bDanger = false; // something that cannot be undone
};

struct FPageRow
{
	FString Tag; // the nation whose flag stands beside it, if any
	FString Title;
	TArray<FString> Lines;
	TArray<FPageButton> Buttons;
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
struct FOfficePage
{
	bool bValid = false;
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
};

/** A shipping lane drawn over the sea. */
struct FViewLane
{
	TArray<FVector2D> Path; // on the map
	FString State; // open, raided or cut
	float Flow = 1.0f; // the share of its cargoes getting through
};

/** An air fleet on its airfields. */
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
	FString Fallen; // the player's nation, once it has been beaten
	TArray<FString> RegionHolder; // by region id: the nation that holds it now
	TArray<FString> RegionStanding; // by region id: where its holder stands with the player (own, ally, enemy, neutral)
	TMap<int32, FString> Joins; // provinces where allied corps could be asked to join the player's army: how many, and whose ("2 Slovak")
	TArray<int32> Unseen; // provinces the player has no eyes on, under the fog of war
	TArray<int32> FreeCapitals; // capitals of fallen countries the player would set free by taking them
	TArray<FViewAirfield> Airfields; // every airfield the player knows of (the grand campaign; empty in a scenario)
	TArray<FIntVector4> RegionWorks; // by region id: factories built since the war began, shipyard level, anti-aircraft level, airfield level
	TArray<FViewCorps> Corps;
	FTreasury Treasury;
	TArray<FString> Declarable; // the countries the player could declare war on this month
	TArray<FString> League; // the members of the League, the third alliance
	int32 IdleTeams = 0; // research teams of the player's with nothing to do
	TArray<FString> Alerts; // things to tell the player as his month opens
	TArray<FString> PageIds; // the rooms the rules set out themselves that are open to him
	TArray<FString> PageLabels;
	bool bStores = false; // the player's nation keeps oil and supplies (a great power in the grand campaign)
	FViewStock Oil;
	FViewStock Supplies;
	TArray<FViewLane> Lanes;
	TMap<FString, FIntVector> DesignYears; // by nation: the year its tank, fighter and bomber designs have reached
	FString Level; // the difficulty the grand campaign is on ("Colonel"); empty in a scenario
	TArray<int32> BridgesDown; // crossings (by their place in the land's list) whose bridges are blown
	TArray<int32> RailsCut; // regions whose railway is cut
	TArray<FViewFleet> Fleets;
	TArray<FViewAirFleet> AirFleets;

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
	TArray<FTicketLine> Lines;
	TArray<FBattleUnit> Attackers;
	TArray<FBattleUnit> Defenders;
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
	int32 AttackerLost = 0;
	int32 DefenderLost = 0;
	int32 Prisoners = 0;
	TArray<FBattleUnit> Attackers;
	TArray<FBattleUnit> Defenders;
	TArray<FCorpsMove> Moved; // the corps that followed up into the ground they won
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
	bool bMine = false; // the player's own ground or troops were involved
	FCampaignView View; // the war as it stood when that nation had finished
};

/** A great moment told on a screen of its own: a nation eliminated, a nation liberated. */
struct FAnnouncement
{
	FString Tag; // whose flag
	FString Title; // "FRANCE HAS FALLEN"
	FString Text;
	bool bGood = false; // good news for the player
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
};

/** Everything that happened between the player ending one month and beginning the next. */
struct FMonthResult
{
	TArray<FMonthStep> Steps;
	TArray<FString> Headlines; // the month's news, most important first
	FString Fallen; // the player's nation, if it has just been beaten
	TArray<FAnnouncement> Announcements;
	FString ReelDate; // the month the newsreel is of
	TArray<FNewsCard> Reel; // the month's stories; empty in a quiet month
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
};

/** One thing the arsenal offers: a corps, a wing of aircraft, ships, or a work for a province. */
struct FArsenalOffer
{
	FString Key; // "corps:arm", "air:ftr", "ship:dd", "work:aa": what to pass back to order it
	FString Group; // army, air, navy or works
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

/** What a landing on an enemy coast would meet, and afterwards how it went. */
struct FLandingPlan
{
	bool bOk = false;
	FString Why; // why it cannot be made, if it cannot
	FString Name; // the coast
	TArray<FString> Lines; // the crossing, the sky, the defenders
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
};
