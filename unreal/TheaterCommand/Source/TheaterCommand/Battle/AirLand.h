#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * The ground under a raid over land with no stage (Bremen's first), painted as pictures, in plain C++ so that the
 * cloud test paints exactly what the game lays (Brian, 20 November 2026, on the Bremen film: "how do we make the ground
 * below look better?"; the art director on build 1778: the Bremen ground was one flat khaki and wants the map's paint,
 * the river and the town). It is the campaign's ground for every European raid, not one film's set: it reads only the
 * board (the aim, true north, the land and the great rivers, the month, the seed, the target's name) and, where the
 * book of town plans has the target (Content/Data/AirGround/towns.json), that town's plan; every other town is made up
 * from the same parts.
 *
 * Seen from 7 to 8 km up, a pixel of the screen is some 4 to 6 m of ground. So the ground is three pictures round the
 * aim (Frames): about 8 m a texel along the raid's way in, 24 m round that, 96 m out to the haze. What is laid:
 * - farmland: blocks of fields about a kilometre across, each cut into strips its own way (the regions share a
 *   general lie), in the month's colours (ploughed earth, winter green, stubble, pasture), with hedgerows or ditches on
 *   the blocks' sides and faint balks between the strips; farmsteads in their groves; meadows by the rivers;
 * - woods: whole blocks of plantation with straight edges, and larger forests from two-scale noise with rides cut
 *   through them, conifer and bare broadleaf mixed;
 * - villages on the roads, every one with its church, joined by country roads (some lined with trees) and the main
 *   roads out of the town;
 * - the river: its plan's course with meanders, embanked quays in the town, groynes outside it, bridges where the roads
 *   and railways cross, the harbour basins with their quays, sheds and ships;
 * - the town: the old town's crooked blocks and its spires (inside its ramparts' star-shaped moat where it had one),
 *   then the courtyard blocks of the newer town, rows of terraced houses, villas in gardens, allotments, the works with
 *   their halls, chimneys and gasometers, the railway yards' many tracks, parks and cemeteries, the airfield;
 * - the sea where the board has it, with a pale strip of shore.
 * Every roof, wood, hedge, spire and chimney has a height, and in the near picture each casts its shadow on what lies
 * on its far side from the sun (ShadeRows). In the coarser pictures what is smaller than a texel is laid as its
 * average, shadows included, so the far ground is the near ground seen from farther, not another ground.
 *
 * Places are board metres (x east on the board, y north on the board: the raid comes in from its south edge). Town
 * plans are in metres east and north of the aim by the compass (true north on the turned board is Place::North).
 */
namespace AirLand
{
	using Pt = std::pair<double, double>;

	/** What the battle gives. */
	struct Place
	{
		double AimX = 50000.0, AimY = 70000.0;
		/** True north on the turned board, a unit vector (the ticket's ground.north; none given: the board's own). */
		double NorthX = 0.0, NorthY = 1.0;
		/** The target province's country (plains, forest, hills, mountain, marsh, urban): it sets the woods and fields. */
		std::string Terrain = "plains";
		/** The target's name: its town plan, if the book has one. */
		std::string Name;
		int Month = 6;
		uint64_t Seed = 1;
		/** Toward the sun, board metres (x, y) and up (z); need not be a unit vector. */
		double SunX = 0.433, SunY = -0.75, SunZ = 0.5;
		/** The board's land (place.ground.land), rings; empty: land everywhere. */
		std::vector<std::vector<Pt>> Land;
		/** The board's great rivers (place.ground.rivers), lines. */
		std::vector<std::vector<Pt>> Rivers;
	};

	/** One picture: W by H texels Step metres apart over x from X0 and y from Y0. Row 0 is the north edge (y = Y0 + H * Step). */
	struct Frame
	{
		double X0 = 0.0, Y0 = 0.0, Step = 8.0;
		int W = 0, H = 0;
		double X1() const { return X0 + W * Step; }
		double Y1() const { return Y0 + H * Step; }
		bool Holds(double X, double Y) const { return X >= X0 && X <= X1() && Y >= Y0 && Y <= Y1(); }
	};

	/** The three pictures round the aim, finest first: each one's ground is drawn from it where no finer one holds it. */
	std::vector<Frame> Frames(const Place& P);

	/**
	 * Toward the sun over a place at a time, a unit vector (board x, y and up z): Latitude in degrees north, Month 1..12
	 * (its middle day), Hour the local sun time (12 is noon), true north on the board (as Place's). For the scene's light
	 * over a raid with no stage: Bremen in December at half past twelve is some 13 degrees up, a little west of south.
	 */
	void SunAt(double Latitude, int Month, double Hour, double NorthX, double NorthY, double& X, double& Y, double& Z);

	/** What a painter laid, for the log and the test. */
	struct Tally
	{
		std::string Plan;      // the town plan's name, or "made" for a town made up
		int Villages = 0;
		int Links = 0;         // country roads between villages
		int Bridges = 0;       // roads and railways over a river wider than a ditch
		int Spires = 0;
		int Segments = 0;      // pieces of the lines (rivers, roads, railways)
		bool bCoast = false;   // the board has sea
	};

	class Painter
	{
	public:
		/** PlansJson: the book of town plans' text (empty: every town made up). Builds what every texel asks of. */
		Painter(const Place& P, const std::string& PlansJson);
		~Painter();
		Painter(const Painter&) = delete;
		Painter& operator=(const Painter&) = delete;

		/**
		 * Paints rows [Row0, Row1) of a picture: RGBA bytes (sRGB, alpha 255), and each texel's height over the ground
		 * in metres (Heights may be null; ShadeRows needs it). Rgba and Heights are the whole picture's (W * H). Any
		 * thread, any rows: nothing the painter holds is changed.
		 */
		void PaintRows(const Frame& F, int Row0, int Row1, uint8_t* Rgba, float* Heights) const;

		/** Darkens rows [Row0, Row1) where something taller toward the sun shades them; every row's heights must be painted. */
		void ShadeRows(const Frame& F, int Row0, int Row1, uint8_t* Rgba, const float* Heights) const;

		/** Whether a picture of this texel size gets shadows (the near and middle ones; the far one lays them in its averages). */
		static bool Shades(const Frame& F) { return F.Step <= 30.0; }

		/** A whole picture on this thread (the tests). */
		void Paint(const Frame& F, std::vector<uint8_t>& Rgba) const;

		/**
		 * A bomb's mark that stays (the scene's bomb burst on land): the texels of a picture round (X, Y) darkened to a
		 * crater and the fresh earth thrown round it. Pixels is the whole picture's, four bytes a texel, red first or,
		 * bBgra, blue first (Unreal's FColor). Returns the box of texels changed (X0, Y0, X1, Y1 inclusive; X0 > X1
		 * when nothing was).
		 */
		void Scar(const Frame& F, double X, double Y, double Kilograms, uint8_t* Pixels, bool bBgra, int Box[4]) const;

		const Tally& Laid() const;

		/** What is at a board point, for the picture tests' measures: 0 sea, 1 fields, 2 woods, 3 river, 4 town, 5 old town, 6 village. */
		int What(double X, double Y) const;

	private:
		struct Impl;
		std::unique_ptr<Impl> Inner;
	};
}
