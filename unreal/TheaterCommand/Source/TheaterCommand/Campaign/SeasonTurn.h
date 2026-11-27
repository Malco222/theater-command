#pragma once

#include <cstdint>
#include <vector>

/**
 * (Release 2, the campaign map look builder; Brian's pick of 18 November 2026, "a season turns moment on the campaign
 * map, where snow or mud visibly spreads across the land at the start of winter or spring", and his bar of
 * 25 November, "I want this to visually wow people on how smooth and realistic and detailed grand it looks".)
 *
 * The season turns on the table. Two things, both looks only (no die is thrown, nothing the rules read):
 *
 *  1. MUD YOU CAN SEE. In the months the rules lay mud on the east, the ground there was washed one flat brown, whole
 *     provinces at a time, edged by their borders. Here the mud is painted into the ground picture itself: it soaks
 *     out from the rivers, the lakes and the low, flat ground, wettest by the water and drying paler toward its edge,
 *     and it thins to patches and stops a little inside the ground the rules give it, never at a province's line.
 *     The owners' stain, the fields, the woods, the borders and everything stood on the table are drawn over it as
 *     before, so a nation's mud is still its own and the plots read as darker versions of themselves.
 *
 *  2. THE TURN ITSELF. In the month the mud first comes, or the snow first lies (or goes), the change is not simply
 *     there: over about three and a half seconds it spreads across the land, as the art director's sheet asks
 *     (art-director/b493/season-turns-look.md). Snow comes from the north and from high ground first, as a soft
 *     ragged front; at the thaw it goes from the south and the low ground first. Mud soaks outward from the rivers
 *     and the low ground with a darker, wetter band at its front. Any key or click settles it at once.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/season_turn/run.sh). The map
 * (ACampaignMapActor::ShowMud, SetSeason, UpdateSeasonTurn) measures each point of its ground, asks these for what
 * to paint and when, and sends the pictures. Measures are in map units (a house on the table is about 2.1); time is
 * in seconds; colours are 0 to 255 as the eye sees them.
 */
namespace TheaterSeasonTurn
{
	// ---- the clock of a turn --------------------------------------------------------------------------------------

	/** Seconds after the change is known before it starts to spread: the month card comes first, and the frame of the month's turn is a heavy one. */
	inline constexpr double Delay = 0.75;
	/** Seconds the front takes to cross the land. */
	inline constexpr double Spreads = 3.0;
	/** Seconds it then takes to settle: the wet band at the mud's front dries back into the rest. */
	inline constexpr double Settles = 0.5;
	/** Seconds a key or click takes to bring it to its settled state. */
	inline constexpr double SkipTakes = 0.2;
	/** How wide the soft front is, as a share of the whole crossing. */
	inline constexpr double FrontWidth = 0.08;
	/** How wide the mud's wet band is behind its front, as a share of the whole crossing. */
	inline constexpr double BandWidth = 0.14;

	/**
	 * Where the front stands, Seconds after the spread began: 0 before anything has moved; past every point's arrival,
	 * band and all, once it has settled. A point arriving at A (0 the first to change, 1 the last) is changed by
	 * the time the front reaches A + FrontWidth.
	 */
	double Front(double Seconds);
	/** The front's place when it has wholly settled: nothing changes after it. */
	inline constexpr double Settled = 1.0 + FrontWidth + BandWidth;
	/** True once the spread has run its course (or been skipped to its end). */
	inline bool IsSettled(double FrontNow) { return FrontNow >= Settled; }

	/** How much of a point's change shows when the front stands at FrontNow (Share, 0 to 1), and how much of the mud's wet band lies on it (Band, 0 to 1). */
	struct FShown
	{
		float Share = 0.0f;
		float Band = 0.0f;
	};
	FShown At(float Arrive, double FrontNow);

	// ---- noise and distance -----------------------------------------------------------------------------------------

	/** A smooth noise from 0 to 1 (about a half on the whole), changing over about one unit of X and Y; the same for the same Seed always. */
	double Noise(double X, double Y, uint32_t Seed);

	/**
	 * Distance on a grid: Field holds 0 where a point is one of those measured from and anything at or above Far
	 * elsewhere, and comes back holding each point's distance from the nearest of them, in grid steps (to within about
	 * eight parts in a hundred of the straight line), capped at Far. NX by NY, row by row.
	 */
	void Spread(std::vector<float>& Field, int NX, int NY, float Far);

	/** Field smoothed over Radius points each way (an even box, twice over: near a bell), in place. NX by NY, row by row. */
	void Blur(std::vector<float>& Field, int NX, int NY, int Radius);

	/**
	 * A turn's arrivals spread evenly over it, in place: each becomes the share of the turn's points that arrive before
	 * it, so that the front crosses about as much of the land in each moment of the turn, and none of the three seconds
	 * is spent on a few late corners. Which point comes before which is kept.
	 */
	void Even(std::vector<float>& Arrive);

	// ---- the mud ----------------------------------------------------------------------------------------------------

	/** A river's own flood ground: the water's wet is half as heavy this far from it (and a tenth three times as far). */
	inline constexpr double SoakNear = 6.0;
	/** The front of the soak reaches this far from the water by the time it is half way across the land. */
	inline constexpr double SoakReach = 45.0;
	/** The mud stops this far inside the ground the rules give it. */
	inline constexpr double EdgeStop = 1.5;
	/** ...and thins to patches over this much more. */
	inline constexpr double EdgeReach = 10.0;

	/** What the map knows of one point of its ground. */
	struct FMudIn
	{
		double X = 0.0, Y = 0.0; // where it is, in map units
		double Water = 1.0e9; // map units to the nearest river, lake or shore
		double Edge = 1.0e9; // map units to the nearest land the mud does not lie on (the edge of the rules' mud ground)
		double Height = 1.0; // the table's height there, smoothed over a few units (map units; the plains of the east stand about 1 to 1.6)
		double Rough = 0.0; // how broken the country is (the province's relief amplitude, map units, smoothed across its borders as the table smooths it)
		double Wooded = 0.0; // 0 to 1, how far it is under woods (the woods' cloth is drawn over the ground)
	};

	/** The mud a point takes when it has settled, and when the soak reaches it. */
	struct FMud
	{
		float Wet = 0.0f; // 0 none, 1 the wettest
		float Arrive = 1.0f; // 0 the first to soak, 1 the last
		float Pool = 0.0f; // 0 to 1: a small standing pool that catches the sky
		float Tone = 1.0f; // the mud's own light and dark, so that it is never one smooth brown
	};
	FMud Mud(const FMudIn& In);

	/** The colours of the mud, as the art director gave them. */
	inline constexpr double WetEarth[3] = { 78.0, 62.0, 46.0 };
	inline constexpr double Wettest[3] = { 62.0, 50.0, 38.0 };
	inline constexpr double Drying[3] = { 120.0, 102.0, 78.0 }; // (toward the road's dust where it dries at its edges)
	inline constexpr double SkyPool[3] = { 128.0, 138.0, 148.0 };
	/** How far toward the mud's colour the ground is taken where the mud lies whole: the ground's own grain is kept under it. */
	inline constexpr double Cover = 0.92;

	/** The ground's colour (RGB, 0 to 255 as the eye sees it) with the mud on it as it shows now. */
	void PaintMud(const uint8_t In[3], const FMud& Mud, const FShown& Shown, uint8_t Out[3]);

	/**
	 * The same, quick enough for every frame of a turn over a whole front: the ground's own colour (Dry), the ground
	 * with the mud on it whole (Whole: PaintMud at a Share of 1 and no band) and with the wet band on it too (Banded:
	 * a Share of 1 and a Band of 1), mixed as light by what shows now. Within a few steps of 255 of PaintMud itself.
	 */
	void MixMud(const uint8_t Dry[3], const uint8_t Whole[3], const uint8_t Banded[3], const FShown& Shown, uint8_t Out[3]);

	// ---- the snow ---------------------------------------------------------------------------------------------------

	/**
	 * When a point's snow changes in the spread. South is 0 at the north edge of the ground whose snow is changing
	 * and 1 at its south edge; High 0 on the low ground and 1 on the highest; Grain a noise from 0 to 1. Coming snow
	 * comes from the north and from high ground first; going snow goes from the south and from the low ground first.
	 */
	float SnowArrive(double South, double High, double Grain, bool bComing);
	/** A point's snow as it shows now, between what lay before (Was) and what lies when it has settled (Will), 0 to 255. */
	uint8_t SnowNow(uint8_t Was, uint8_t Will, const FShown& Shown);
}
