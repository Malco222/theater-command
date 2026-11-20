#pragma once

#include "CoreMinimal.h"
#include "Battle/Fates.h"

class FBattleEffects;

/** Where a ship lies when a step of her look plays (the scene's own frame: she may have moved since the hit). */
struct FShipLie
{
	FTransform World = FTransform::Identity; // the scene's frame to the world's
	FVector Base = FVector::ZeroVector;      // in the scene's frame: her waterline amidships, where she lies now
	double SeaZ = 0.0;                       // in the scene's frame: the sea's face (her waterline before she settled)
	double Yaw = 0.0;                        // degrees: her model's bow (-X) turned along her heading
	double Long = 100.0, Beam = 12.0, High = 20.0; // metres
	double UnitsPerMetre = 100.0;
};

/**
 * How the hits on a scene's ships look (Battle/Fates: Brian, 20 November, "i want this same thing for naval and ground
 * battles"): the set-piece air battle's stage now (AAirStage), the fleet action's scene when it comes.
 *
 * A hit she survives no longer looks the same each time: a fire her crew put out in a minute or two, a fire that
 * spreads, a torpedo's flooding that lists her toward the hit until her crew counter-flood, steam from an engine room,
 * a bomb bursting harmlessly on a battleship's armour, a carrier's deck park going up plane by plane. Her end stays
 * exactly the one the numbers name (the dispatch says it); only its details vary: which side she rolls to (the side she
 * was flooding on), bow or stern first, how fast, which magazine, where she breaks.
 *
 * Looks only, and its own dice: nothing the battle counts, records or says changes. The steps are played on the
 * effects kit; the kit's newer calls (Steam, CookOff, Burn with an id and BurnOut, Glance) are made only where the kit
 * has them, so this builds on the kit as it is today and after. Until then a step that has no call is left out, never
 * drawn with a stand-in (the effects artist, 20 November 14:39Z), apart from a deck park's pops, drawn as small bursts.
 * At most MostFires fires burn on one ship and MostFiresAll on the scene (the Pearl Harbor stage's room); at most
 * MostLeaks steam leaks and MostCookOffs cook-offs run at once on the scene (the pre-check's probe of the kit's room,
 * 20 November 17:44Z: all the new calls at once on six burning ships reached the room's edge).
 */
class FShipLooks
{
public:
	/** Reads Content/Data/Battle/fates.json; false, with a warning, if it cannot (the scene keeps its plain looks). */
	bool Begin(uint64 Seed);
	bool IsReady() const { return Picker.IsValid(); }
	void End();

	/** A hit she survives (air:ship-hit). Strike: the air battle's "torpedo", "dive", "level" or "bomb". Hurt: 0..1. */
	void Hit(int32 Id, const FString& Class, const FString& Strike, bool bHarbour, double Hurt, double Now);
	/** Her end as the numbers name it (How). The look's dials, for the scene's own act of that end: side (-1 or 1),
	 * pace (her end's times multiplied by it), end ("bow", "stern", "even", "fore", "aft"), roll, cut. Null if none;
	 * read it at once (the next Sink may move it). */
	const Fates::Fate* Sink(int32 Id, const FString& Class, const FString& How, bool bHarbour, double Now);

	/** The test reel (-TheaterStageReel): every look a ship of Class that survives her hit can show, as "key|strike". */
	TArray<FString> ReelOf(const FString& Class, bool bHarbour) const;
	/** Plays one look of the reel on ship Id, alone (her pose and what was still to come cleared); its key, or empty. */
	FString Reel(int32 Id, const FString& Class, const FString& KeyAndStrike, bool bHarbour, double Now);
	/** The effects kit was cleared (FBattleEffects::Clear): what the looks had running on it is forgotten, and their
	 * steps still to come dropped. The ships' list, trim and settle stay. */
	void Cleared();
	/** For the looks reel's camera: where her last reel look acts, as a share of her length from amidships (minus toward
	 * the bow) and metres above her waterline, and the side it acts on (1 starboard, -1 port). */
	bool ReelAim(int32 Id, double High, double& OutAlong, double& OutUp, double& OutSide) const;

	/** Plays the steps now due. LieOf: where ship Id lies now (false: she is gone, her steps are dropped). */
	void Advance(double Now, FBattleEffects* Effects, TFunctionRef<bool(int32, FShipLie&)> LieOf);

	/** Her list (degrees, plus toward her +Y side), trim (degrees, plus down by the bow) and settle (metres) now. */
	bool Pose(int32 Id, double Now, double& OutList, double& OutTrim, double& OutSettle) const;
	/** The side her looks have her flooding on (-1 or 1), or 0 if none yet. */
	double SideOf(int32 Id) const;
	/** Fires burning on her now, her looks' and the scene's own; AddFire counts one of the scene's own. */
	int32 FiresOn(int32 Id, double Now) const;
	void AddFire(int32 Id, double Until);

	static constexpr int32 MostFires = 5;
	static constexpr int32 MostFiresAll = 20;
	static constexpr int32 MostLeaks = 6;
	static constexpr int32 MostCookOffs = 3;
	static constexpr int32 MostGlancesFrame = 2; // glancing shots begun in one frame; the rest wait a frame

private:
	struct FDue
	{
		double At = 0.0;
		int32 Ship = 0;
		Fates::Beat Step;
		uint32 Mix = 0; // (a number of the step's own, for where on the deck "deck" falls)
	};
	TUniquePtr<Fates::Table> Table;
	TUniquePtr<Fates::Director> Picker;
	TArray<FDue> Due;
	TMap<int32, Fates::Hull> Hulls;
	TMap<int32, double> Sides;
	TMap<int32, TArray<double>> Fires; // by ship: when each fire she has stops
	TMap<int32, Fates::Fate> Ends;
	TMap<int32, FString> ReelAt; // by ship: the place her last reel look acts at (Reel)
	TMap<int32, TArray<int32>> Burns; // by ship: the ids of the fires her looks lit, where the kit takes ids
	TMap<int32, TArray<int32>> Leaks; // by ship: the ids of her steam leaks, where the kit has steam
	TArray<double> LeakStops, CookStops; // on the scene: when each running leak and cook-off ends
	FBattleEffects* Kit = nullptr;    // the kit the last Advance played on (to stop her steam between plays)
	int32 NextBurn = 0;

	void Queue(int32 Id, const Fates::Fate& Look, double Now, bool bBurstsOnly = false);
	static bool IsBurst(const Fates::Beat& Step);
	void GiveWay(int32 Id, bool bUnder, double Now);
	void StopLeaks(int32 Id);
	void Play(const FDue& Step, const FShipLie& Lie, FBattleEffects* Effects, double Now);
	int32 FiresAll(double Now) const;
};
