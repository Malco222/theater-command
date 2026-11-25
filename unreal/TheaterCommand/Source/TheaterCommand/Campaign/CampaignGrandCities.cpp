#include "Campaign/CampaignMapActor.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/*
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 3, grander great cities. Brian,
 * 25 November 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks". The
 * reckoning is Campaign/GrandCity.h, with its cloud test in Tools/grand_city; the building is in the town's builder,
 * FMiniatureMesh::Township, where FTownSite::bGrand is set.)
 *
 * A city or a capital on the table is a grid of blocks round its square, and at its edge it stops. With
 * "tc.Map.GrandCities 1" it is the same town, every house where it stood, and:
 *   - its heart keeps its cornice out to the heart's edge under steeper roofs, and the terraces beside the heart
 *     stand taller: the middle of the town is one full mass that steps down to its edge;
 *   - some of the heart's blocks carry a corner turret or a small dome, lower and smaller than a capital's own, and
 *     none of them the blocks that stand in the square before a capital's great building;
 *   - most of the houses at its edge stand in a garden, a lawn with two trees; and beyond them, between the roads out
 *     of the town, lies one more ring of villas on their lawns, inside the town's own measure.
 *
 * Where it is and is not: cities and capitals only; never one packed into a small province (it is built tall
 * already); Europe's kits, the Americas' and the railway town's at first (the walled town, the timber town, thatch
 * and tin and the Indian town are as they were). No dice are thrown for it, on or off: the town of today is not moved,
 * and the town seen from high up is laid out as the near one is.
 *
 * The towns are built again when the switch is turned (ACampaignMapActor's tick, beside "tc.Speed.TownSquares"): that
 * is one long frame, as when the cities leave room for a quarter. Decoration only: no rule reads it and nothing of it
 * is saved. Off as it comes, until Brian has seen the Mac's pictures of it; "-TheaterGrandCities" turns it on for them.
 */

namespace
{
	TAutoConsoleVariable<int32> CVarGrandCities(
		TEXT("tc.Map.GrandCities"), 0,
		TEXT("1: the cities and capitals of the campaign map stand in their grander form (a fuller heart under steeper roofs, turrets and small domes over it, gardens and villas at the edge; every house of the town as it was stays where it stood); 0: the towns as they were. The towns are built again when it is turned."));
}

bool ACampaignMapActor::GrandCitiesWanted()
{
	static const bool bGrandCitiesAsked = FParse::Param(FCommandLine::Get(), TEXT("TheaterGrandCities"));
	return bGrandCitiesAsked || CVarGrandCities.GetValueOnGameThread() != 0;
}
