#pragma once

#include <cmath>

#include "ShipWay.h"

/**
 * (Release 2, the campaign map's upgrade 3; Brian, 25 November 2026: "I want this to visually wow people on how
 * smooth and realistic and detailed grand it looks".) Funnel smoke for a fleet on the campaign map: every ship with a
 * funnel has steam up. Lying still at sea she shows a pale wisp that stands over her and leans down the wind. Under
 * way her smoke streams astern, along the water she has crossed (so it bends where she turned), lower and longer the
 * faster she goes; it is thick and black while she works up to speed, with her fires forced, and thins to grey as she
 * holds her speed or eases. A submarine shows none, and nor does a fleet at anchor by her port.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/ship_smoke/run.sh). The piece
 * (AArmyPiece) says where each funnel's top is and how she steams; the map (ACampaignMapActor::UpdateSmoke) draws the
 * puffs, through the same mesh as the rest of the table's smoke. Nothing is kept from frame to frame: a puff is where
 * the clock says it is, so the plume is the same at any frame rate and costs nothing when it is not drawn.
 *
 * A plume's measures are in the lengths of the ship that makes it; time is in seconds. A course is as
 * Campaign/ShipWay.h keeps one: her place at each tick, the way her bows point there, and how far she has run.
 */
namespace TheaterShipSmoke
{
	using TheaterShipWay::FSpot;

	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }
	inline double Smooth(double From, double To, double V)
	{
		const double X = Clamp((V - From) / (To - From), 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	/** Seconds a puff takes from the funnel to the end of the plume. */
	inline constexpr double Life = 2.2;
	/** The most puffs all the fleets together may take of one frame (the table's smoke has room for 2,400 of every kind). */
	inline constexpr int MostPuffs = 420;
	/** How fast she gathers way is read over this long each side of the moment, in seconds: see Steams. */
	inline constexpr double GatherOver = 0.12;
	/** Gathering way this fast (shares of her full speed a second) her fires are forced all they can be; losing it this fast they are eased all they can be. A fleet works up to her best in a second and a half or so of a voyage. */
	inline constexpr double Hard = 0.6;

	/**
	 * How much smoke a class of ship makes for her length: 0 a battleship, 1 a carrier, 2 a cruiser, 3 a destroyer
	 * (anything else smokes as a cruiser). A destroyer is all boilers, and makes more for her size than a battleship;
	 * a carrier's one funnel on her island makes a little less. A submarine (4) runs on her diesels and makes none.
	 */
	inline double Girth(int Class)
	{
		switch (Class)
		{
		case 0: return 1.0;
		case 1: return 0.9;
		case 3: return 1.3;
		case 4: return 0.0;
		default: return 1.05;
		}
	}

	/** How she steams: what leaves her funnels, and how the plume lies. */
	struct FFire
	{
		double Thick = 0.0; // how thick the smoke is as it leaves her (the most a puff covers, 0 to 1)
		double Dark = 0.0;  // 0 the pale haze of fires banked, 1 the black of fires forced
		double Reach = 0.0; // how far astern the plume streams, in her lengths
		double High = 0.0;  // how high it stands over the funnel by its end, in her lengths
		double Lee = 0.0;   // how far down the wind it has gone by its end, in her lengths
		double End = 0.0;   // how big a puff is by its end (its radius, in her lengths, before her class's girth)
	};

	/**
	 * Pace is her speed as a share of her full speed (0 lying still, 1 at her best). Gather is how fast that grows, in
	 * shares a second: over nought as she works up to speed, under it as she eases. Much (0 to 2) is the player's
	 * measure of how thick, Long (0 to 2) of how far astern.
	 *
	 * Lying still: a thin pale wisp that rises a little over a quarter of her length and leans down the wind (it is the
	 * common sight on the table, and is kept slight). At her best: a pale haze still, twice as thick, a length and a
	 * half astern and blown low. Working up: thick and black, for as long as she gathers way. Easing: a little thinner
	 * and paler than her speed alone would make it.
	 *
	 * (Why a steady fire is pale. The table's sea at working height is a dark blue, about a tenth to a sixth of white
	 * on the Mac's stills. A pale smoke reads on it strongly and a middle grey weakly; black reads least of all, on
	 * the water, and is seen where it lies over her wake and herself: so it is made the thickest.)
	 */
	inline FFire Fire(double Pace, double Gather, double Much = 1.0, double Long = 1.0)
	{
		const double P = Clamp(Pace, 0.0, 1.0);
		const double Forced = Clamp(Gather / Hard, 0.0, 1.0), Eased = Clamp(-Gather / Hard, 0.0, 1.0);
		FFire Out;
		Out.Dark = Clamp(0.08 + 0.12 * P + 0.8 * Forced - 0.06 * Eased, 0.0, 1.0);
		Out.Thick = Clamp(Clamp(Much, 0.0, 2.0) * Clamp(0.26 + 0.4 * P + 0.3 * Forced - 0.08 * Eased, 0.15, 0.95), 0.0, 1.0);
		Out.Reach = 1.55 * P * Clamp(Long, 0.0, 2.0);
		Out.High = 0.28 - 0.14 * P;
		Out.Lee = 0.36 - 0.16 * P;
		Out.End = 0.11 + 0.07 * P;
		return Out;
	}

	/** For this long before her course ends (seconds) a puff lets go of the fire that made it: see Carries and Keeps. */
	inline constexpr double LetGo = 0.8;

	/**
	 * The fire a puff carries with it. The plume lies as her fire is Now (how far astern, how high, how far down the
	 * wind, how big), but each puff is as thick and as dark as the fire that made it (Then, its age ago): the black of
	 * working up goes down the plume with the puffs that were made then and thins out with them, and the plume never
	 * changes colour all at once, as it would if every puff took the fire of the moment (on a course with bends she
	 * eases and works up again several times in a few seconds). Keep, 1 to 0, is how much of the fire that made it a
	 * puff still has: all of it out at sea, none as her course ends, so that she comes to rest under the smoke of a
	 * ship lying still.
	 */
	inline FFire Carries(const FFire& Now, const FFire& Then, double Keep)
	{
		const double K = Clamp(Keep, 0.0, 1.0);
		FFire Out = Now;
		Out.Thick = Now.Thick + (Then.Thick - Now.Thick) * K;
		Out.Dark = Now.Dark + (Then.Dark - Now.Dark) * K;
		return Out;
	}

	/** How much of the fire that made it a puff keeps, with Left seconds of her course still to run. */
	inline double Keeps(double Left)
	{
		return Smooth(0.0, LetGo, Left);
	}

	/** One puff of a plume: where it is from the funnel's top (in her lengths), how big, and how thick. */
	struct FPuff
	{
		double Astern = 0.0;  // back along the water she has crossed
		double Rise = 0.0;    // above the funnel's top
		double Lee = 0.0;     // down the wind
		double Size = 0.0;    // its radius
		double Stretch = 1.0; // how much longer than wide it is drawn, along the plume (1: round)
		double Thick = 0.0;   // how much it covers at its middle, 0 to 1
		// The way the plume runs where the puff is, for the drawing out: so much astern, so much up, so much down the
		// wind (the three together a unit; all nought for a puff that is round).
		double RunAstern = 0.0, RunRise = 0.0, RunLee = 0.0;
	};

	/**
	 * The puff a share Age of its life gone (0 as it leaves the funnel, 1 at the plume's end), of a plume of Count
	 * puffs from a ship of that Girth. It leaves the funnel small and thick and near it; it grows, thins and is left
	 * astern faster the older it is, so that the young puffs crowd the funnel where they are small and the old ones
	 * are spread where they are large. Where the puffs would stand apart (a fast ship's, or the few that are drawn
	 * from far off), each is drawn out along the plume until it reaches its neighbour's middle (Stretch, to four times
	 * its width at most). The table's soft puff is thick at its middle and nothing at its rim, evenly between: two
	 * that reach each other's middles are together as thick between them as either is at its own, so the plume is one
	 * even ribbon, as thick along its middle as the puffs are, and never a row of them.
	 */
	inline FPuff Puff(const FFire& Fire, double Age, int Count, double GirthOf)
	{
		const double A = Clamp(Age, 0.0, 1.0);
		const int Many = Count < 1 ? 1 : Count;
		FPuff Out;
		Out.Astern = Fire.Reach * std::pow(A, 1.5);
		Out.Rise = Fire.High * std::pow(A, 0.75);
		Out.Lee = Fire.Lee * std::pow(A, 1.6);
		Out.Size = GirthOf * (0.03 + (Fire.End - 0.03) * std::pow(A, 0.8));
		// (How far apart two neighbours lie along the plume: the rate it runs, over how many there are. As it leaves
		// the funnel a puff is taken for a fiftieth of its life old: it has no rate at nought.)
		const double Young = A < 0.02 ? 0.02 : A;
		const double GoesAstern = Fire.Reach * 1.5 * std::sqrt(Young), GoesUp = Fire.High * 0.75 * std::pow(Young, -0.25), GoesLee = Fire.Lee * 1.6 * std::pow(Young, 0.6);
		const double Goes = std::sqrt(GoesAstern * GoesAstern + GoesUp * GoesUp + GoesLee * GoesLee);
		const double Gap = Goes / static_cast<double>(Many);
		Out.Stretch = Out.Size > 1.0e-9 ? Clamp(Gap / Out.Size, 1.0, 4.0) : 1.0;
		if (Out.Stretch > 1.0 && Goes > 1.0e-9)
		{
			Out.RunAstern = GoesAstern / Goes;
			Out.RunRise = GoesUp / Goes;
			Out.RunLee = GoesLee / Goes;
		}
		Out.Thick = Fire.Thick * Smooth(0.0, 0.07, A) * std::pow(1.0 - A, 0.9);
		return Out;
	}

	/** A colour of smoke: red, green and blue, 0 to 1. */
	struct FInk
	{
		double R = 0.0, G = 0.0, B = 0.0;
	};

	/**
	 * The colour of a puff: as it leaves her, from the pale grey of an easy fire to the brown-black of oil burnt in a
	 * hurry (Dark); and paler and greyer the older it is, as it thins into the air. The pale is as light as the smoke
	 * of the towns' chimneys (0.6), well over the tone of the table's sea. Never bright enough or warm enough to be
	 * taken for a light by night (the map's night rule tells a light by its colour, from 0.75: Tools/ship_smoke checks it).
	 */
	inline FInk Ink(double Dark, double Age)
	{
		const double D = Clamp(Dark, 0.0, 1.0), Thinned = 0.55 * Clamp(Age, 0.0, 1.0);
		const FInk Pale{ 0.6, 0.59, 0.575 }, Oily{ 0.07, 0.066, 0.063 }, Haze{ 0.64, 0.63, 0.62 };
		const FInk Head{ Pale.R + (Oily.R - Pale.R) * D, Pale.G + (Oily.G - Pale.G) * D, Pale.B + (Oily.B - Pale.B) * D };
		return FInk{ Head.R + (Haze.R - Head.R) * Thinned, Head.G + (Haze.G - Head.G) * Thinned, Head.B + (Haze.B - Head.B) * Thinned };
	}

	/** How closely the smoke is drawn, by how far the eye is from what it looks at: 0 close to, 1 from working height, 2 from high up. */
	inline int Level(double EyeDistance)
	{
		return EyeDistance < 1500.0 ? 0 : EyeDistance < 3600.0 ? 1 : 2;
	}

	/** Close to, every funnel has a plume of its own; from further off a ship has one, from the middle of her funnels. */
	inline bool EachFunnel(int LevelOf)
	{
		return LevelOf <= 0;
	}

	/** How many puffs make a plume: more under way than lying still, and fewer the further the eye. From high up a ship lying still shows none. */
	inline int Puffs(int LevelOf, bool bUnderWay)
	{
		const int L = LevelOf < 0 ? 0 : LevelOf > 2 ? 2 : LevelOf;
		static constexpr int Way[3] = { 6, 5, 4 }, Still[3] = { 4, 3, 0 };
		return bUnderWay ? Way[L] : Still[L];
	}

	/**
	 * How far through its life the puff Which of Count is, at the clock's Now: each takes Life seconds from the funnel
	 * to the plume's end and then leaves the funnel again, the Count of them evenly apart. Seed tells one funnel from
	 * the next, so that no two puff together.
	 */
	inline double AgeOf(double Now, int Which, int Count, double Seed)
	{
		const double Turn = Now / Life + static_cast<double>(Which) / static_cast<double>(Count < 1 ? 1 : Count) + Seed;
		return Turn - std::floor(Turn);
	}

	/**
	 * How she steams on a course of her own, Clock seconds into it: her places at each tick (Each seconds apart) and
	 * Full, the furthest she goes in one tick. Her pace is the mean of the two ticks about the moment; how fast she
	 * gathers way is read from a little before to a little after (GatherOver each side), so that it does not flicker
	 * from tick to tick. Before her course begins and after it ends she lies still.
	 */
	template <typename TPoint>
	inline void Steams(const TPoint* Points, int Count, double Each, double Full, double Clock, double& OutPace, double& OutGather)
	{
		OutPace = 0.0;
		OutGather = 0.0;
		if (Count < 2 || Each <= 0.0 || Full <= 1.0e-9)
		{
			return;
		}
		auto Leg = [&](int K)
		{
			const double X = static_cast<double>(Points[K + 1].X) - static_cast<double>(Points[K].X);
			const double Y = static_cast<double>(Points[K + 1].Y) - static_cast<double>(Points[K].Y);
			return Clamp(std::sqrt(X * X + Y * Y) / Full, 0.0, 1.0);
		};
		auto PaceAt = [&](double Time)
		{
			const double Beat = Time / Each;
			if (Beat <= 0.0 || Beat >= static_cast<double>(Count - 1))
			{
				return 0.0;
			}
			// (leg K is run between tick K and tick K + 1: its pace is hers at its middle)
			const double From = Beat - 0.5;
			const int Low = From < 0.0 ? 0 : (static_cast<int>(From) > Count - 2 ? Count - 2 : static_cast<int>(From));
			const int Next = Low + 1 > Count - 2 ? Count - 2 : Low + 1;
			const double Share = Clamp(From - static_cast<double>(Low), 0.0, 1.0);
			return Leg(Low) + (Leg(Next) - Leg(Low)) * Share;
		};
		OutPace = PaceAt(Clock);
		OutGather = (PaceAt(Clock + GatherOver) - PaceAt(Clock - GatherOver)) / (2.0 * GatherOver);
	}

	/**
	 * And under the old sail, corner to corner on one ease: a share Progress of the way through a time of Seconds. She
	 * is quickest in the middle of it, gathers way to there and loses it after.
	 */
	inline void SteamsEased(double Progress, double Seconds, double& OutPace, double& OutGather)
	{
		const double P = Clamp(Progress, 0.0, 1.0);
		OutPace = 4.0 * P * (1.0 - P);
		OutGather = Seconds > 1.0e-6 && P > 0.0 && P < 1.0 ? 4.0 * (1.0 - 2.0 * P) / Seconds : 0.0;
	}

	/**
	 * Where a point of her was when she was Back further astern along her course: what must be added to where it is
	 * now. Ran is how far she has run. Off is where the point lies from her middle now (flat on the table, in the
	 * course's units): it is carried round with her head, so that on a turn the smoke of a funnel far from her middle
	 * lies along the water that funnel crossed. Before her course began she is taken to have come straight on to its
	 * start. With nothing gone astern it is nothing, whatever her head, so that a plume always begins at its funnel.
	 */
	template <typename TPoint, typename THead>
	inline FSpot Was(const TPoint* Points, const THead* Heads, const double* Marks, int Count, double Ran, double Back, FSpot Off)
	{
		if (Count < 2 || Back <= 0.0)
		{
			return FSpot{};
		}
		FSpot Here, HereHead, There, ThereHead;
		TheaterShipWay::Along(Points, Heads, Marks, Count, Ran, Here, HereHead);
		TheaterShipWay::Along(Points, Heads, Marks, Count, Ran - Back, There, ThereHead);
		HereHead = TheaterShipWay::Unit(HereHead, FSpot{ 0.0, 1.0 });
		ThereHead = TheaterShipWay::Unit(ThereHead, HereHead);
		// (her head then, from her head now: the turn she has made since)
		const double Cos = TheaterShipWay::Dot(HereHead, ThereHead), Sin = TheaterShipWay::Cross(HereHead, ThereHead);
		const FSpot OffThen{ Off.X * Cos - Off.Y * Sin, Off.X * Sin + Off.Y * Cos };
		return (There - Here) + OffThen - Off;
	}

	/** And with no course of her own: straight astern of the way her bows point now. */
	inline FSpot WasStraight(FSpot HeadNow, double Back)
	{
		return Back <= 0.0 ? FSpot{} : TheaterShipWay::Unit(HeadNow, FSpot{ 0.0, 1.0 }) * -Back;
	}
}
