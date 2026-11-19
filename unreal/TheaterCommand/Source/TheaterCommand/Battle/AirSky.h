#pragma once

#include "Effects/TheaterFxCore.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * The sky of an air battle, in plain C++: the day's clouds, laid once and drawn round the eye wherever it is, turned to
 * it, so that the cloud test lays and checks exactly what the game draws (air 3d-handoff.md section 10, "The sky at
 * bomber height"; the art director's ruling of 19 November 04:40Z).
 *
 * The battle (src/battle/air/sky.ts) gives the day: a cloud deck under the bombers, or scattered cumulus, with one
 * cloud base and one top in metres above the sea (sky.place.cloud). Nothing here counts: it is only what is drawn,
 * and where falling aircraft and their pieces pass out of sight (into the deck, where a tile of it is drawn).
 *
 * What is decided here, and why:
 * - The clouds lie wherever the fight is (at Pearl Harbor most of it, and most falls, are 20 to 32 km short of the
 *   harbour): they are laid on a square of Period metres that repeats without a seam, and each cloud is drawn where
 *   it lies nearest the eye, out to Reach, thinning into the haze past two thirds of that, so that the clouds run
 *   on toward the horizon, small, as the fair-weather sky does from bomber height.
 * - A deck sits 1.5 to 2.5 km below the raid's lowest bombers above 2,000 m (the art director): moved up or down to
 *   that, its thickness kept, thinned only where keeping it would bring its base within 300 m of the sea. Over the
 *   lowest band (1,800 m) it lies above those bombers instead, its base 600 m over them (thinned to fit under any
 *   higher band). Falls begun above it end in it where a tile of it is drawn under them; through a hole they go on
 *   down.
 * - Cloud never sits lower than 300 m over the ground under it (the cloud table's note for the Andes, Yunnan,
 *   Ethiopia and Anatolia), so a cloud over a mountain is lifted with it, where it is drawn. A deck tile lifted so
 *   high that it would come within 1.5 km of the bombers is thinned, or not drawn where the mountain stands through.
 * - Cumulus are scattered (about three eighths of the sky), each a flat grey base and a lit, lumpy dome of overlapping
 *   puffs, most shallow and a few up to the day's tops; fewer over the target, so the ships or the town stay in sight
 *   (Pearl Harbor's own morning: cloud banked on the mountains, the harbour open). A deck covers most of the sky with
 *   a few holes, and a strike on ships has its hole over the ships.
 * - Cumulus drift with the battle's wind, and the clearing stays over the target as they go: a cloud carried into it
 *   thins away, one carried out of it fills in. A deck stays where it is laid (from bomber height its drift could not
 *   be seen), so its hole stays over the ships.
 * - Far clouds are drawn with fewer puffs (each layer's first two, its turrets, its veil), the farthest with three
 *   (its base, its top, its veil); the puffs in between fade.
 *
 * Places are in board metres (X east, Y north, Z up, the sea at 0), as the flight core's.
 */
namespace AirSky
{
	using Vec = TheaterFx::Vec;
	using Rgb = TheaterFx::Rgb;

	/** The battle's wind (board axes, metres a second), as the effects kit is given it: 3 toward the east, 2 toward
	 * the south. */
	extern const Vec Wind;
	/** The side of the square the clouds are laid on, which repeats round the eye (metres): over twice the reach, so
	 * no cloud is ever wanted at two places at once. */
	constexpr double Period = 44000.0;
	/** How far from the eye clouds are drawn (metres); they thin into the haze past two thirds of it. */
	constexpr double Reach = 20000.0;
	/** Within this distance of the eye a cloud is drawn with all its puffs; past it with its far ones (the rest fading
	 * out over the next kilometre). */
	constexpr double NearAll = 5000.0;
	/** Past this distance a cloud is drawn with its farthest-seen puffs only (its base, its top and its veil; the rest
	 * fading out over the next 1.5 km). */
	constexpr double FarAll = 10000.0;

	/** The day's sky as the battle gives it (sky.place.cloud). Absent (a rules bundle from before it): cumulus. */
	struct Day
	{
		bool bGiven = false;
		bool bDeck = false;            // kind "deck", else "cumulus"
		double Base = 950.0, Top = 2500.0; // metres above the sea (the cloud table's fallback middles when absent)
		double Chance = 0.0;
		std::string Zone;
	};

	/** Where and for what battle the sky is laid. */
	struct Placing
	{
		uint64_t Seed = 238;
		double TargetX = 50000.0, TargetY = 70000.0;
		/** The height of the raid's lowest bombers above 2,000 m (metres): a deck's top is kept 1.5 to 2.5 km under
		 * it. 0 when none fly that high. */
		double Band = 4500.0;
		/** The height of the raid's highest bombers at 2,000 m or lower (the 1,800 m band; metres): a deck's base is
		 * kept 600 m over them. 0 when there are none. */
		double LowBand = 0.0;
		/** A raid on a harbour or a strike on a fleet: a deck keeps a hole over the ships. */
		bool bShips = false;
		/** The way toward the sun (board axes), for which side of each cloud is lit. */
		Vec Sun = Vec(-0.35, -0.45, 0.82);
		/** The ground's height under a board point (metres); unset: the sea. */
		std::function<double(double, double)> Ground;
		/** The most puffs drawn at once (the painter's room: its mesh's size, and its work each time the eye moves). */
		int MostPuffs = 4000;
	};

	/** One puff of cloud: a soft disc turned to the eye. Laid at the sea's level (its cloud's foot on the day's base). */
	struct Puff
	{
		Vec At;
		double R = 200.0;     // metres
		double Flat = 1.0;    // its upright squashed to this share (a cloud's flat base)
		double Lit = 1.0;     // 0 to 1: how much of the sun it has (its cloud's top and sunward side)
		double Alpha = 0.7;   // at its heart
		int Cloud = 0;
		int Lod = 2;          // drawn: 0 within NearAll of the eye, 1 within FarAll, 2 out to the reach
	};

	/** One cloud (a cumulus, or a tile of the deck): its foot's middle on the laid square, its width and height. */
	struct Cloud
	{
		Vec At;
		double W = 1000.0, H = 500.0;
		int First = 0, Count = 0;
		/** 0.1 to 0.9: how soon the target's clearing (or the ships' hole) takes it. */
		double Fate = 0.5;
	};

	/** Where one cloud is drawn: the copy of the square nearest the eye (Shift), lifted over the ground under it,
	 * thinned for the bombers' band (Squash: its height's share; 0 not drawn), and how much the clearing leaves. */
	struct Placed
	{
		Vec Shift;
		double Lift = 0.0, Squash = 1.0, Show = 1.0;
	};

	struct Field
	{
		Day Sky;
		/** The day's base and top as laid (a deck moved for the band) over the sea. */
		double Base = 0.0, Top = 0.0;
		/** A deck's top as laid over the sea, the height above which a fall begins above it; 0 on a cumulus day. */
		double DeckTop = 0.0;
		/** How far the deck's top was moved for the band (metres, up or down), and why ("", "below the band", "up to
		 * the band", "above the band"). */
		double Moved = 0.0;
		std::string MovedWhy;
		/** The clouds and their puffs on the laid square (its corner Origin, Period metres a side). */
		std::vector<Cloud> Clouds;
		std::vector<Puff> Puffs;
		Vec Origin;
		/** The share of the sky under cloud round the target (4.5 to 9 km from it) as first laid. */
		double Cover = 0.0;
		double TargetX = 0.0, TargetY = 0.0, Band = 0.0;
		bool bShips = false;
		int MostPuffs = 4000;
		std::function<double(double, double)> Ground;
		/** Which laying this is (each Lay its own number), so a painter knows a field it has not painted before. A field
		 * changed after it is laid is laid again. */
		uint64_t Serial = 0;

		/** The wind's carry after so many air seconds: cumulus drift; a deck stays. */
		Vec DriftAt(double AirSeconds) const;
		/** The copy of the square that puts a cloud nearest a point of the laid frame (multiples of Period). */
		Vec ShiftNear(int CloudIndex, double X, double Y) const;
		/** How far a cloud is lifted with its middle at a board point: to 300 m over the highest ground under it. */
		double LiftAt(int CloudIndex, double X, double Y) const;
		/** A deck tile's height share once lifted (the bombers' band: 1 as laid, less where thinned, 0 where the
		 * mountain stands through); 1 for cumulus. */
		double SquashFor(int CloudIndex, double Lift) const;
		/** How much of a cloud the clearing leaves, its middle at a board point: cumulus reaching within 4 km of the
		 * target thin out, the higher a cloud's Fate the sooner; a deck on a strike at ships keeps its hole within
		 * 4.5 km. */
		double ShownAt(int CloudIndex, double X, double Y) const;
		/** A cloud as drawn for a point of the laid frame (an eye less the drift), the field carried by Drift. */
		Placed Place(int CloudIndex, double X, double Y, const Vec& Drift) const;
		/** The share of board points under a cloud more than half shown, the field carried by Drift: the points on a
		 * grid within Outside of the target (a square), past Inside (a circle). */
		double CoverOf(const Vec& Drift, double Inside, double Outside) const;
		/** The deck's drawn top over a board point (metres): 0 where no tile of it is drawn (a hole, the ships'
		 * clearing, high ground through it) and on a cumulus day. */
		double DeckAt(double X, double Y) const;

		// (the deck's tiles by cell of the laid square, DeckCell metres a cell: the tile whose middle is nearest among
		// those drawn over it, -1 none)
		std::vector<int> DeckTiles;
		double DeckCell = 240.0;
		int DeckN = 0;
	};

	/** The day's clouds, the same every time for the same battle. */
	Field Lay(const Day& D, const Placing& P);

	/** The deck's place for the raid's bands (the art director's rule): Base and Top in, moved; returns how far the
	 * top moved. Band and LowBand as in Placing. */
	double PlaceDeck(double& Base, double& Top, double Band, double LowBand, std::string& OutWhy);

	/**
	 * The clouds round an eye turned to it and coloured, far to near: up to the field's MostPuffs puffs, each a middle
	 * and two rings of Sides points (PerPuff() points a puff, both faces of every triangle in Triangles()), as the
	 * effects kit lays its own, so the same see-through, unlit material draws them. Colours are linear red, green, blue
	 * and alpha, four bytes a point. The room not needed is drawn as points (no area, nothing to fill), first.
	 */
	class Painter
	{
	public:
		explicit Painter(int InSides = 12);
		int Sides() const { return SideCount; }
		int PerPuff() const { return 1 + 2 * SideCount; }
		/** The triangles for Count puffs (fixed: a puff's slot keeps its triangles, only its points change). */
		std::vector<int> Triangles(int Count) const;
		/** Lays the field for an eye. The points are in the field's own frame: Drift is the wind's carry so far
		 * (Field::DriftAt), which the caller adds to the whole (the scene moves the mesh by it), and the eye is turned
		 * back by it here. Sun is the way toward the sun; Light the hour's light on cloud (white by day). Returns the
		 * nearest puff's distance. */
		double Paint(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, std::vector<Vec>& OutPoints, std::vector<uint8_t>& OutRgba);
		/** Whether the eye has moved enough since the last Paint (in the field's own frame) for the turn of the
		 * puffs to show: about a quarter of a degree at the nearest puff. */
		bool Stale(const Vec& Eye, const Vec& Drift) const;
		/** The puffs drawn by the last Paint, and those it had no room for (the farthest). */
		int Drawn() const { return LastDrawn; }
		int Dropped() const { return LastDropped; }

	private:
		int SideCount = 12;
		std::vector<double> Cos, Sin;
		struct Order
		{
			int Index = 0;
			int Cloud = 0;
			double Deep = 0.0;
			double Fade = 1.0;
		};
		std::vector<Order> Sorted;
		std::vector<Placed> Where;
		// (each cloud's lift, kept until its middle has moved 150 m: the ground is looked up again only then)
		struct Kept
		{
			double X = 1e30, Y = 1e30, Lift = 0.0;
		};
		std::vector<Kept> Lifts;
		bool bPainted = false;
		uint64_t LastSerial = 0;
		Vec LastEye;      // in the field's frame (eye less drift)
		double LastNear = 1000.0;
		int LastDrawn = 0, LastDropped = 0;
	};

	/** The cloud's colours: its sunlit top, its shaded side, its grey base. */
	extern const Rgb SunSide, ShadeSide, BaseSide;
}
