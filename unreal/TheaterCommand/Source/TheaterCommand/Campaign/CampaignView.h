#pragma once

#include "CoreMinimal.h"

/**
 * What the rules tell the game about the state of the campaign: a plain
 * picture of who holds what and who stands where. The 3D game draws this and
 * never works anything out for itself.
 */

struct FViewCorps
{
	int32 Id = INDEX_NONE;
	FString Nation;
	FString Name;
	FString Kind; // inf, mot, arm, cav, mtn, fort
	FString General; // empty when the corps has no named commander
	FString Posture; // stand, dig, back
	int32 Divisions = 0;
	float Strength = 1.0f; // 0..1: men and equipment left
	float Cohesion = 1.0f; // 0..1
	int32 Region = INDEX_NONE;
	int32 Moves = 0; // weeks of action left this month
	int32 MovesMax = 0;
	bool bCanOrder = false; // the player may give this corps orders right now
};

struct FCampaignView
{
	FString Rules; // "stand-in" until the real rules are bundled in
	int32 Turn = 0;
	FString Date;
	FString Human; // the nation the player leads
	FString Current; // whose month it is
	bool bHumanUp = false;
	TArray<FString> RegionHolder; // by region id: the nation that holds it now
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

/** One corps' march: the regions it passed through, ending where it now stands. */
struct FCorpsMove
{
	int32 CorpsId = INDEX_NONE;
	TArray<int32> Path;
};
