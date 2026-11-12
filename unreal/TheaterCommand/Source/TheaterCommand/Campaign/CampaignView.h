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

struct FViewCorps
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	FString Kind; // inf, mot, arm, cav, mtn, fort
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
	int32 Ships() const { return Battleships + Carriers + Cruisers + Destroyers + Submarines; }
};

/** An air fleet on its airfields. */
struct FViewAirFleet
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	int32 Base = INDEX_NONE;
	int32 Fighters = 0, Bombers = 0;
	FString Mission;
};

struct FCampaignView
{
	FString Rules; // "stand-in" until the real rules are bundled in
	int32 Turn = 0;
	FString Date;
	FString Human; // the nation the player leads
	FString Current; // whose month it is
	bool bHumanUp = false;
	bool bEve = false; // the player is placing an army on the eve of war: marches at home only, no month passes
	bool bOpen = false; // free play: no nation follows history's plan
	FString Fallen; // the player's nation, once it has been beaten
	TArray<FString> RegionHolder; // by region id: the nation that holds it now
	TArray<FString> RegionStanding; // by region id: where its holder stands with the player (own, ally, enemy, neutral)
	TArray<FViewCorps> Corps;
	FTreasury Treasury;
	TArray<FString> Declarable; // the countries the player could declare war on this month
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
	bool bMine = false; // the player's own ground or troops were involved
	FCampaignView View; // the war as it stood when that nation had finished
};

/** Everything that happened between the player ending one month and beginning the next. */
struct FMonthResult
{
	TArray<FMonthStep> Steps;
	TArray<FString> Headlines; // the month's news, most important first
	FString Fallen; // the player's nation, if it has just been beaten
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

/** What a territory holds, and what the player can do there. */
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

struct FProvinceInfo
{
	bool bValid = false;
	int32 Region = INDEX_NONE;
	bool bHome = false;
	FString Holdings; // industry, works, harbour, in a line
	FString Yield; // what the province gives each month, in a line
	FString Note;
	TArray<FProvinceOption> Options;
	TArray<FOrderLine> Forming;
};
