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
