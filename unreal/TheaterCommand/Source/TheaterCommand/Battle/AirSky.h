#pragma once

#include "Effects/TheaterFxCore.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * The sky of an air battle, in plain C++: the day's clouds laid round the target and turned to the eye, so that the
 * cloud test lays and checks exactly what the game draws (air 3d-handoff.md section 10, "The sky at bomber height";
 * the art director's ruling of 19 November 04:40Z).
 *
 * The battle (src/battle/air/sky.ts) gives the day: a cloud deck under the bombers, or scattered cumulus, with one
 * cloud base and one top in metres above the sea (sky.place.cloud). Nothing here counts: it is only what is drawn,
 * and where falling aircraft and their pieces pass out of sight (into a deck's top).
 *
 * What is decided here, and why:
 * - A deck sits 1.5 to 2.5 km below the raid's bombers (the art director): where the drawn top comes nearer than
 *   1.5 km it is lowered, its thickness kept; for the lowest band (1,800 m) it lies above the bombers instead. Falls
 *   end in its top as drawn.
 * - Cloud never sits lower than 300 m over the ground under it (the cloud table's note for the Andes, Yunnan,
 *   Ethiopia and Anatolia), so a cloud over a mountain is lifted with it.
 * - Cumulus are scattered (about three eighths of the sky), each a flat grey base and a lit, lumpy dome of overlapping
 *   puffs, most shallow and a few up to the day's tops; fewer over the target, so the ships or the town stay in sight
 *   (Pearl Harbor's own morning: cloud banked on the mountains, the harbour open). A deck covers most of the sky with
 *   a few holes, and a strike on ships has its hole over the ships.
 * - The field thins out toward its edge, so no line shows where it ends; past it the haze takes the sky.
 * - It drifts with the battle's wind.
 *
 * Places are in board metres (X east, Y north, Z up, the sea at 0), as the flight core's.
 */
namespace AirSky
{
	using Vec = TheaterFx::Vec;
	using Rgb = TheaterFx::Rgb;

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
		/** The height the raid's bombers fly at (metres), for the deck's place. */
		double Band = 4500.0;
		/** A raid on a harbour or a strike on a fleet: a deck keeps a hole over the ships. */
		bool bShips = false;
		/** The way toward the sun (board axes), for which side of each cloud is lit. */
		Vec Sun = Vec(-0.35, -0.45, 0.82);
		/** Half the side of the square laid round the target (metres). */
		double Half = 12000.0;
		/** The ground's height under a board point (metres); unset: the sea. */
		std::function<double(double, double)> Ground;
		/** The most puffs (the painter's work each time the eye moves). */
		int MostPuffs = 3200;
	};

	/** One puff of cloud: a soft disc turned to the eye. */
	struct Puff
	{
		Vec At;
		double R = 200.0;     // metres
		double Flat = 1.0;    // its upright squashed to this share (a cloud's flat base)
		double Lit = 1.0;     // 0 to 1: how much of the sun it has (its cloud's top and sunward side)
		double Alpha = 0.7;   // at its heart
		int Cloud = 0;
	};

	/** One cloud (a cumulus, or a tile of the deck): its foot's middle, its width and its height. */
	struct Cloud
	{
		Vec At;
		double W = 1000.0, H = 500.0;
		int First = 0, Count = 0;
	};

	struct Field
	{
		Day Sky;
		/** The day's base and top as laid (a deck moved for the band; over the sea; a cloud over high ground is lifted
		 * on its own). */
		double Base = 0.0, Top = 0.0;
		/** Where falls and their pieces pass out of sight: a deck's top as laid; 0 on a cumulus day. */
		double DeckTop = 0.0;
		/** How far the deck was moved for the band (metres, up or down), and why ("", "below the band", "above the band"). */
		double Moved = 0.0;
		std::string MovedWhy;
		std::vector<Cloud> Clouds;
		std::vector<Puff> Puffs;
		/** The share of the square's middle (within 0.7 Half of the target, past its clearing of 4.5 km) under cloud. */
		double Cover = 0.0;
		/** Clouds left out because the puffs ran out (none, in a battle the size of these). */
		int Short = 0;
	};

	/** The day's clouds round the target, the same every time for the same battle. */
	Field Lay(const Day& D, const Placing& P);

	/** The deck's place for the raid's band (the art director's rule): Base and Top in, moved; returns the move. */
	double PlaceDeck(double& Base, double& Top, double Band, std::string& OutWhy);

	/**
	 * The puffs turned to an eye and coloured, far to near: each a middle and two rings of Sides points (PerPuff()
	 * points a puff, both faces of every triangle in Triangles()), as the effects kit lays its own, so the same
	 * see-through, unlit material draws them. Colours are linear red, green, blue and alpha, four bytes a point.
	 */
	class Painter
	{
	public:
		explicit Painter(int InSides = 12);
		int Sides() const { return SideCount; }
		int PerPuff() const { return 1 + 2 * SideCount; }
		/** The triangles for Count puffs (fixed: a puff's slot keeps its triangles, only its points change). */
		std::vector<int> Triangles(int Count) const;
		/** Lays the field for an eye. The points are in the field's own frame: Drift is the wind's carry so far, which
		 * the caller adds to the whole (the scene moves the mesh by it), and the eye is turned back by it here. Sun is
		 * the way toward the sun; Light the hour's light on cloud (white by day). Returns the nearest puff's distance. */
		double Paint(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, std::vector<Vec>& OutPoints, std::vector<uint8_t>& OutRgba);
		/** Whether the eye has moved enough since the last Paint (in the field's own frame) for the turn of the
		 * puffs to show: about a quarter of a degree at the nearest puff. */
		bool Stale(const Vec& Eye, const Vec& Drift) const;

	private:
		int SideCount = 12;
		std::vector<double> Cos, Sin;
		struct Order
		{
			int Index = 0;
			double Deep = 0.0;
		};
		std::vector<Order> Sorted;
		bool bPainted = false;
		Vec LastEye;      // in the field's frame (eye less drift)
		double LastNear = 1000.0;
	};

	/** The cloud's colours: its sunlit top, its shaded side, its grey base. */
	extern const Rgb SunSide, ShadeSide, BaseSide;
}
