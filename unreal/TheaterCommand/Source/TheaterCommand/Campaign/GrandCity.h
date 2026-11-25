#pragma once

#include <cmath>
#include <cstdint>

/**
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 3, grander great cities. Brian,
 * 25 November 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks".)
 * A city or a capital on the campaign map is a grid of blocks round its square: a few closed blocks at its heart,
 * terraces round them, houses standing apart at its edge, and there it stops. Grander, it is the same town with every
 * house where it stood, and:
 *
 *  - its heart keeps its cornice out to the heart's edge, under steeper roofs, and the terraces beside the heart
 *    stand taller, so that the middle of the town is one full mass that steps down to its edge;
 *  - some of the heart's blocks carry a corner turret or a small dome, lower and smaller than a capital's own;
 *  - the houses at its edge stand in gardens, and beyond them, between the roads that leave the town, lies one more
 *    ring of villas in their own gardens: a garden suburb, inside the town's own measure, so that a great city has
 *    no hard edge.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/grand_city/run.sh). The town's builder
 * (FMiniatureMesh::Township) asks here how tall, which block and where; it builds the pieces. Nothing here is thrown
 * on the builder's dice: every answer comes from the block's own place in the grid (I, J) and the town's own number
 * (Seed), so the town of today is not moved by it, and the town seen from high up is laid out as the near one is.
 * Decoration only: no rule of the game reads any of it.
 *
 * Measures are in house-units (the builder's U: a house is under one of them long) unless they say otherwise.
 */
namespace TheaterGrandCity
{
	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }

	/** A smooth step from nothing at Low to one at High. */
	inline double Smooth(double Low, double High, double V)
	{
		const double T = Clamp((V - Low) / (High - Low), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/**
	 * A number from nothing up to (never reaching) one that depends only on the block (I, J), the town (Seed) and which
	 * question is asked of it (Salt): the same every time it is asked, and never the dice's.
	 */
	inline double Chance(int I, int J, int Seed, int Salt)
	{
		std::uint32_t H = static_cast<std::uint32_t>(I) * 73856093u;
		H ^= static_cast<std::uint32_t>(J) * 19349663u;
		H ^= static_cast<std::uint32_t>(Seed) * 83492791u;
		H ^= static_cast<std::uint32_t>(Salt) * 2654435761u;
		H ^= H >> 16;
		H *= 0x7feb352du;
		H ^= H >> 15;
		H *= 0x846ca68bu;
		H ^= H >> 16;
		return static_cast<double>(H >> 8) / 16777216.0;
	}

	/** The same, laid between Low and High. */
	inline double Between(int I, int J, int Seed, int Salt, double Low, double High) { return Low + (High - Low) * Chance(I, J, Seed, Salt); }

	/** Whether Bearing lies within Within degrees of Other (both in degrees, any turn of the compass). */
	inline bool NearBearing(double Bearing, double Other, double Within) { return std::fabs(std::remainder(Bearing - Other, 360.0)) < Within; }

	// ---- The heart.

	/** A closed block's walls at the square, and at the heart's edge (today's fall to 0.88 there). */
	inline constexpr double CorniceAtSquare = 1.25, CorniceAtEdge = 1.1;
	/** The height of a closed block's walls, RingOverHeart being how far out it stands (nothing at the middle, one at the heart's edge). */
	inline double HeartTall(double RingOverHeart) { return CorniceAtSquare + (CorniceAtEdge - CorniceAtSquare) * Clamp(RingOverHeart, 0.0, 1.0); }
	/** Its roof, steeper than today's 0.14. */
	inline constexpr double HeartRoof = 0.22;
	/** The terraces just outside the heart stand this many times their height of today, */
	inline constexpr double TerraceMost = 1.4;
	/** falling to today's by the middle ring. Ring is how far out the block stands as a share of the town's measure. */
	inline double Storeys(double Ring, double Heart, double Middle) { return TerraceMost - (TerraceMost - 1.0) * Smooth(Heart, Middle, Ring); }

	/** What a closed block of the heart carries over its roofs. */
	enum class EAccent { None, Turret, Dome };
	/** Of the heart's blocks, the share with a corner turret, and with a dome. */
	inline constexpr double TurretShare = 0.4, DomeShare = 0.15;
	inline EAccent Accent(int I, int J, int Seed)
	{
		const double Drawn = Chance(I, J, Seed, 3);
		return Drawn < TurretShare ? EAccent::Turret : Drawn < TurretShare + DomeShare ? EAccent::Dome : EAccent::None;
	}
	/** A wing of a closed block is built to its block's height times at most this (the builder's own dice). */
	inline constexpr double WingMost = 1.1;
	/** The turret: a square shaft on the block's corner nearest the square, this wide, standing this far over the tallest a wing may be, under a pointed cap. */
	inline constexpr double TurretWide = 0.46, TurretOver = 0.12, TurretCapWide = 0.3, TurretCap = 0.26;
	inline double TurretTall(double Tall) { return Tall * WingMost + TurretOver; }
	/** The dome: a round drum in the block's court, this far across, its top this far over the tallest a wing may be, and the dome on it. */
	inline constexpr double DrumWide = 0.6, DrumOver = 0.05, DomeWide = 0.66, DomeRise = 0.2, DomeSpike = 0.16;
	inline double DrumTall(double Tall) { return Tall * WingMost + DrumOver; }
	/** The highest point of either, over the ground. (A capital's own dome stands 1.7 high and a full unit across.) */
	inline double AccentTop(EAccent Kind, double Tall)
	{
		return Kind == EAccent::Turret ? TurretTall(Tall) + TurretCap : Kind == EAccent::Dome ? DrumTall(Tall) + DomeRise + DomeSpike : 0.0;
	}

	// ---- The far side's own (the town kits 7 to 10): the same grander form, in each kit's own manner of building.
	//
	// The walled town (7) and the timber town (8) build in tiers under eaves, as their own great buildings do: there
	// the turret is a corner tower with an eave, a small upper storey and a pointed roof, and the dome is a hall roof,
	// a low square storey under one wide tent of tile. The Indian town (10) is flat-roofed: every one of its accents is
	// a dome on a drum, in its own whitewash. Thatch and tin (9) stays as low as it is, with no accents: its grandeur
	// is at its edge, where the suburb's villas are white bungalows under tin, each with a veranda.

	/** True for the four kits of the far side. */
	inline bool FarKit(int Look) { return Look >= 7 && Look <= 10; }
	/** True for the kits that build in tiers under eaves (the walled town and the timber town). */
	inline bool EavesKit(int Look) { return Look == 7 || Look == 8; }
	/** True for the kit that keeps the heart it has (thatch and tin): no fuller middle and no accents, the suburb only. */
	inline bool LowKit(int Look) { return Look == 9; }
	/**
	 * The corner tower under eaves: on the turret's shaft an eave this far out all round and this deep, a step up to an
	 * upper storey this wide and tall, and its pointed roof this wide and high. It tops out where a block's dome does.
	 */
	inline constexpr double EaveOut = 0.12, EaveDeep = 0.07, EaveStep = 0.05, UpperWide = 0.3, UpperTall = 0.12, UpperCapWide = 0.26, UpperCap = 0.12;
	inline double EavesTowerTop(double Tall) { return TurretTall(Tall) + EaveStep + UpperTall + UpperCap; }
	/** The hall roof: a square storey in the block's court, as wide as the drum, under one tent of tile this far out all round and this high. */
	inline constexpr double HallEave = 0.14, HallRoof = 0.24;
	inline double HallTop(double Tall) { return DrumTall(Tall) + HallRoof; }
	/** The highest point of a block's accent in a kit of the far side. */
	inline double KitAccentTop(int Look, EAccent Kind, double Tall)
	{
		if (Kind == EAccent::None || LowKit(Look))
		{
			return 0.0;
		}
		if (EavesKit(Look))
		{
			return Kind == EAccent::Turret ? EavesTowerTop(Tall) : HallTop(Tall);
		}
		return AccentTop(Look == 10 ? EAccent::Dome : Kind, Tall); // (the flat-roofed Indian town: every one a dome)
	}
	/**
	 * The walled town's wall stands as a square, this share of the town's measure from its middle on each side (the
	 * builder's own WallRing). A lawn is laid wholly inside it or wholly outside it, this far (in house-units) clear
	 * of it, never under it.
	 */
	inline constexpr double WalledRing = 0.9, WallClear = 0.2;
	/** True where a lawn from (LowA, LowB) to (HighA, HighB), in the plan's own measure, lies clear of a square wall Ring from the middle. */
	inline bool ClearOfWall(double LowA, double HighA, double LowB, double HighB, double Ring, double Margin)
	{
		const double MostA = std::fmax(std::fabs(LowA), std::fabs(HighA)), MostB = std::fmax(std::fabs(LowB), std::fabs(HighB));
		if (MostA < Ring - Margin && MostB < Ring - Margin)
		{
			return true; // (wholly inside the walls)
		}
		return LowA > Ring + Margin || HighA < -Ring - Margin || LowB > Ring + Margin || HighB < -Ring - Margin; // (or wholly beyond one side)
	}
	/** The bungalow's veranda: a roof of tin this far out all round the house, this share of the way up its wall, and this thick. */
	inline constexpr double VerandaOut = 0.22, VerandaAt = 0.8, VerandaThick = 0.025;

	// ---- The garden suburb.

	/** A garden: a lawn about the middle of its block (A along the town's streets and B across them, from the block's middle), and trees at its corners. */
	struct FTreeSpot
	{
		double A = 0.0, B = 0.0; // from the block's middle
		double High = 0.0, Round = 0.0; // its height, and its crown's radius
		bool bDark = false; // the darker of the town's two leaves
	};
	struct FGarden
	{
		bool bLawn = false;
		double A = 0.0, B = 0.0; // the lawn's middle, from the block's
		double Long = 0.0, Wide = 0.0; // along and across
		int Trees = 0;
		FTreeSpot Tree[4];
	};
	/** A lawn is never longer or wider than this, which is less than a block is: two gardens never meet. */
	inline constexpr double LawnLongMost = 1.5, LawnWideMost = 1.1;
	/** Of the blocks at the town's edge (its houses standing apart), the share whose houses stand in a garden. */
	inline constexpr double EdgeGardens = 0.65;

	/** A villa of the suburb. */
	struct FVilla
	{
		double A = 0.0, B = 0.0; // from the block's middle
		double Long = 0.0, Wide = 0.0, High = 0.0, Roof = 0.0;
		double Turn = 0.0; // degrees from the town's long streets
		int Wall = 0, Tile = 0; // which of the place's five wall paints and five roof paints
		bool bLit = false; // a lamp in its window by night
	};
	struct FPlot
	{
		FGarden Garden;
		int Villas = 0;
		FVilla Villa[2];
	};

	/** True where (A, B) lies within Margin of the villa's walls. */
	inline bool Beside(const FVilla& Villa, double A, double B, double Margin)
	{
		const double Turned = Villa.Turn * 3.14159265358979323846 / 180.0;
		const double DA = A - Villa.A, DB = B - Villa.B;
		const double Along = DA * std::cos(Turned) + DB * std::sin(Turned), Across = -DA * std::sin(Turned) + DB * std::cos(Turned);
		return std::fabs(Along) < 0.5 * Villa.Long + Margin && std::fabs(Across) < 0.5 * Villa.Wide + Margin;
	}

	/** How far a villa's walls reach from her middle along the streets, and across them, turned as she is. */
	inline double ReachAlong(const FVilla& Villa)
	{
		const double Turned = Villa.Turn * 3.14159265358979323846 / 180.0;
		return 0.5 * Villa.Long * std::fabs(std::cos(Turned)) + 0.5 * Villa.Wide * std::fabs(std::sin(Turned));
	}
	inline double ReachAcross(const FVilla& Villa)
	{
		const double Turned = Villa.Turn * 3.14159265358979323846 / 180.0;
		return 0.5 * Villa.Long * std::fabs(std::sin(Turned)) + 0.5 * Villa.Wide * std::fabs(std::cos(Turned));
	}

	/**
	 * The lawn of block (I, J), no shorter than LongLeast and no narrower than WideLeast, and trees just inside Trees
	 * of its corners. Salt keeps an edge block's garden and a suburb's apart.
	 */
	inline FGarden Lawn(int I, int J, int Seed, int Salt, int Trees, double LongLeast, double WideLeast)
	{
		FGarden Garden;
		Garden.bLawn = true;
		Garden.Long = Between(I, J, Seed, Salt, LongLeast, LawnLongMost);
		Garden.Wide = Between(I, J, Seed, Salt + 1, WideLeast, LawnWideMost);
		Garden.A = Between(I, J, Seed, Salt + 2, -0.08, 0.08);
		Garden.B = Between(I, J, Seed, Salt + 3, -0.05, 0.05);
		// (which corner the first tree stands at, and so round)
		const int First = static_cast<int>(Chance(I, J, Seed, Salt + 4) * 4.0);
		for (int K = 0; K < Trees && K < 4; ++K)
		{
			const int Corner = (First + K) % 4;
			FTreeSpot& Spot = Garden.Tree[Garden.Trees++];
			Spot.A = Garden.A + (Corner == 0 || Corner == 3 ? -1.0 : 1.0) * (0.5 * Garden.Long - 0.07) + Between(I, J, Seed, Salt + 10 + 5 * K, -0.03, 0.03);
			Spot.B = Garden.B + (Corner < 2 ? -1.0 : 1.0) * (0.5 * Garden.Wide - 0.07) + Between(I, J, Seed, Salt + 11 + 5 * K, -0.03, 0.03);
			Spot.High = Between(I, J, Seed, Salt + 12 + 5 * K, 0.45, 0.7);
			Spot.Round = Between(I, J, Seed, Salt + 13 + 5 * K, 0.16, 0.24);
			Spot.bDark = Chance(I, J, Seed, Salt + 14 + 5 * K) < 0.5;
		}
		return Garden;
	}

	/** The garden of a block at the town's edge, where its houses stand apart: a lawn and two trees for most, nothing for the rest. */
	inline FGarden EdgeGarden(int I, int J, int Seed)
	{
		return Chance(I, J, Seed, 40) < EdgeGardens ? Lawn(I, J, Seed, 41, 2, 1.15, 0.85) : FGarden();
	}

	/** The suburb reaches no further from the town's middle than this share of the town's own measure (a block's middle), */
	inline constexpr double FringeOut = 0.96;
	/** and no villa's middle further out than this. */
	inline constexpr double VillaOut = 0.97;
	/** A ring of suburb thinner than this has no villas. */
	inline constexpr double FringeLeast = 0.6;
	/**
	 * Where the suburb begins on a bearing: Limit is how far the town runs on it, and the builder lays no block whose
	 * middle is at or beyond this (it lays one where Far <= Limit * (0.96 + 0.08 * a throw) - 0.35 Um). In map units,
	 * as Limit and Um (a house-unit in map units) are.
	 */
	inline double FringeFrom(double Limit, double Um) { return Limit * 1.04 - 0.35 * Um; }
	/**
	 * The chance that a block whose middle is Far from the town's middle is one of the suburb's: nothing inside the
	 * town or beyond its measure (Reach), and falling from nine in ten at the town's last houses to four in ten at
	 * the suburb's rim. All four in map units.
	 */
	inline double FringeShare(double Far, double Limit, double Reach, double Um)
	{
		const double From = FringeFrom(Limit, Um), To = FringeOut * Reach;
		if (To - From < FringeLeast * Um || Far < From || Far > To)
		{
			return 0.0;
		}
		return 0.9 - 0.5 * (Far - From) / (To - From);
	}
	/** Whether block (I, J) is one of the suburb's, given that chance. */
	inline bool InFringe(int I, int J, int Seed, double Share) { return Share > 0.0 && Chance(I, J, Seed, 11) < Share; }

	/** A tree's trunk stands no nearer a villa's wall than this. */
	inline constexpr double TrunkClear = 0.08;
	/**
	 * A plot of the suburb: a lawn, on it one villa along the street or two smaller ones side by side across it, and
	 * trees just inside its corners. LitShare is the share of the town's houses that keep a lamp in the window. Every
	 * corner of a villa stands on the lawn; a tree that would stand against a villa's wall is left out.
	 */
	inline FPlot Plot(int I, int J, int Seed, double LitShare)
	{
		FPlot Made;
		const FGarden All = Lawn(I, J, Seed, 70, 2 + static_cast<int>(Chance(I, J, Seed, 69) * 3.0), 1.3, 0.95);
		Made.Villas = Chance(I, J, Seed, 20) < 0.4 ? 2 : 1;
		for (int K = 0; K < Made.Villas; ++K)
		{
			FVilla& Villa = Made.Villa[K];
			const int Salt = 21 + 10 * K;
			const bool bPair = Made.Villas == 2;
			Villa.Long = bPair ? Between(I, J, Seed, Salt, 0.5, 0.62) : Between(I, J, Seed, Salt, 0.62, 0.8);
			Villa.Wide = bPair ? Between(I, J, Seed, Salt + 1, 0.36, 0.44) : Between(I, J, Seed, Salt + 1, 0.42, 0.54);
			Villa.High = bPair ? Between(I, J, Seed, Salt + 2, 0.3, 0.42) : Between(I, J, Seed, Salt + 2, 0.32, 0.46);
			Villa.Roof = Between(I, J, Seed, Salt + 3, 0.22, 0.3);
			Villa.Turn = bPair ? 90.0 + Between(I, J, Seed, Salt + 6, -8.0, 8.0) : Between(I, J, Seed, Salt + 6, -10.0, 10.0);
			// (how much room the lawn leaves her, along and across)
			const double RoomA = 0.5 * All.Long - ReachAlong(Villa) - 0.02, RoomB = 0.5 * All.Wide - ReachAcross(Villa) - 0.02;
			if (bPair)
			{
				// (one toward each end of the lawn, never nearer its middle than leaves a walk between the two)
				const double Out = Between(I, J, Seed, Salt + 4, std::fmin(0.33, RoomA), RoomA);
				Villa.A = All.A + (K == 0 ? -Out : Out);
			}
			else
			{
				Villa.A = All.A + Between(I, J, Seed, Salt + 4, -RoomA, RoomA);
			}
			Villa.B = All.B + Between(I, J, Seed, Salt + 5, -RoomB, RoomB);
			Villa.Wall = static_cast<int>(Chance(I, J, Seed, Salt + 8) * 5.0);
			Villa.Tile = static_cast<int>(Chance(I, J, Seed, Salt + 9) * 5.0);
			Villa.bLit = Chance(I, J, Seed, 60 + K) < LitShare;
		}
		Made.Garden = All;
		Made.Garden.Trees = 0;
		for (int K = 0; K < All.Trees; ++K)
		{
			bool bClear = true;
			for (int V = 0; V < Made.Villas; ++V)
			{
				bClear = bClear && !Beside(Made.Villa[V], All.Tree[K].A, All.Tree[K].B, TrunkClear);
			}
			if (bClear)
			{
				Made.Garden.Tree[Made.Garden.Trees++] = All.Tree[K];
			}
		}
		return Made;
	}
}
