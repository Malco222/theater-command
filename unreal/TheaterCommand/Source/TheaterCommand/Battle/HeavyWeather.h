#pragma once

#include "CoreMinimal.h"
#include "Battle/FleetSpray.h"
#include "Templates/UniquePtr.h"

#include <vector>

class AAirStage;
class FBattleEffects;
namespace SeaShow
{
	struct Field;
	struct ShipOut;
	class Show;
}

/**
 * HEAVY WEATHER in the sea battle's scene (the look bible's S6; the release 2 ocean plan, steps 6(b) and 12(b)): what
 * ASeaBattle's marked hooks hand it ("// S6 hook"), drawn. Looks only, and nothing at all unless asked for:
 *  - bow spray (-TheaterSeaSpray=1): the sheets FleetSpray throws over the bows of the 4 ships nearest the eye, sent to the
 *    effects kit through its naval spray row where the kit has one (FleetSpray::HasSprayRow, the ask to the effects
 *    thread), else through its existing Bow row, the low spurt on the water (Skip, "water"): on the sea beside her stem
 *    (the scene's bOnWater held) or on her forecastle (the kit's ground there is her deck); at most 80 puffs alive;
 *  - ice (-TheaterSeaIce=1): the frost of the battle's place, month and wind (Battle/ShipIce) set on the stage, which
 *    glazes each hull's colours as it lays her (AAirStage::LayShip); said once on the log.
 * The spray's work is plain C++ (Battle/FleetSpray, Tools/sea_waves/spray_test.cpp); here only the kit's calls and the
 * lines it says. Nothing is done in its destructor.
 */
class FHeavyWeather
{
public:
	FHeavyWeather() = default;
	FHeavyWeather(const FHeavyWeather&) = delete;
	FHeavyWeather& operator=(const FHeavyWeather&) = delete;

	/** OpenOn's hook: the spray made under -TheaterSeaSpray=1 (bSpray), for the battle's seed; without it Held is left empty
	 * and nothing is drawn. bGale: Beaufort 7 and over, its green seas and spindrift, which are the sea gale's own (step
	 * 14(c), another thread's, not landed: the scene passes false until it does, and the core holds the wind to SeaShow's
	 * 13.8 m/s there). */
	static void Open(TUniquePtr<FHeavyWeather>& Held, bool bSpray, bool bGale, uint64 Seed);

	/** LayAndSteer's hook, for every ship of every step: on the first ship of a step, the step itself (every ship of the
	 * show, the eye, the wind; the sheets thrown, sent to the kit). Nothing without the spray made, or without a kit. */
	static void Ship(TUniquePtr<FHeavyWeather>& Held, FBattleEffects* Kit, const SeaShow::Show& Show, const SeaShow::ShipOut& Ship, double UnitsPerMetre, double Now,
	                 bool bSkip, bool& bOnWater);

	/** OpenOn's hook for ice: the frost of the battle's field (its latitude, longitude and month), its wind and where it
	 * blows from (radians), from the battle's opening (BeginT) set on the stage, and said. */
	static void Ice(AAirStage* Stage, const SeaShow::Field& Board, double WindU, double WindFrom, double BeginT);

	/** The spray's line once a battle (SayThreadsAndLook): sheets, green seas, spindrift and the most puffs alive. */
	static void Say(const TUniquePtr<FHeavyWeather>& Held);

private:
	FleetSpray::Spray Core;
	std::vector<FleetSpray::Hull> Hulls;
	std::vector<FleetSpray::Call> Calls;
	double SteppedAt = -1.0e9;
	void Step(FBattleEffects* Kit, const SeaShow::Show& Show, double UnitsPerMetre, double Now, bool bSkip, bool& bOnWater);
};
