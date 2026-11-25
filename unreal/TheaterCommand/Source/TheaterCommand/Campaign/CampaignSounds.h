#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * The game's sound effects: an army setting off, a gun, a ship's siren, the stamp of an order given.
 *
 * The sounds are made by Tools/make_sounds.mjs and brought in by Tools/build_sounds.sh; each is known here by
 * its file's name ("tank_engine", "rifles"). If the sounds have not been made, or one is missing, nothing
 * plays and nothing fails.
 */
namespace CampaignSounds
{
	/** Plays an effect now. Volume and Pitch are multiples of the sound as it was made. */
	void Play(const UObject* World, const TCHAR* Name, float Volume = 1.0f, float Pitch = 1.0f);

	/** Plays an effect after a pause, in seconds. */
	void PlayLater(const UObject* World, const TCHAR* Name, float Seconds, float Volume = 1.0f);

	/** Turns the effects on or off (the N key). Returns whether they are now on. */
	bool Toggle();
	bool IsOn();
	void SetOn(bool bOn);

	/** What an army of these kinds of corps sounds like setting off: tracks, lorries, hooves or boots. */
	const TCHAR* OfArmy(const TArray<FString>& Kinds, const FString& Nation);

	/** What a thing ordered from the arsenal sounds like ("corps:arm", "air:bmr", "ship:bb", "work:factory"). */
	const TCHAR* OfOrder(const FString& Key);
}
