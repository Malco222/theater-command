#pragma once

#include <cmath>

#include "ShipWay.h"

/**
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 1, busy ports. Brian, 25 November
 * 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks".) A port on the
 * campaign map has its quay, its cranes and two ships lying alongside, and none of it moves. Here a small merchant ship
 * or two works each port: she lies off the quay with her head to sea, weighs, gathers way down the right-hand side of
 * the fairway, eases round a wide turn in the offing, comes back up the other side, slows as she nears the harbour,
 * rounds up off the quay and lies there again. Two ships of one port pass each other in the fairway. Her wake lies
 * along the water she has crossed, and none when she lies still.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/port_life/run.sh). The map
 * (ACampaignMapActor, Campaign/CampaignPortLife.cpp) finds each port's water, builds the little ships and moves them.
 * Nothing is kept from frame to frame: a ship is where the clock says she is, so she runs the same at any frame rate
 * and costs nothing while she is not drawn. Decoration only: no rule of the game reads any of it.
 *
 * Measures are in the map's units and seconds. A bearing is in degrees east of north; on the map east is +X and
 * south is +Y.
 */
namespace TheaterPortLife
{
	using TheaterShipWay::FSpot;

	inline constexpr double Pi = 3.14159265358979323846;

	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }
	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	inline double Smooth(double From, double To, double V)
	{
		const double X = Clamp((V - From) / (To - From), 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	/** Half the breadth of the fairway, which is also the radius of both her turns. A little more than her own length. */
	inline constexpr double Half = 3.0;
	/** How far off the quay's edge the middle of her turn by the harbour lies: she never comes nearer the quay than this less Half, clear of the ships alongside it. */
	inline constexpr double NearOff = 6.6;
	/** Her best speed, in map units a second: under a lorry convoy's two, well under a train's. */
	inline constexpr double Cruise = 1.3;
	/** From her berth she gathers way over this far. */
	inline constexpr double Gather = 6.0;
	/** Coming in, she comes down to her harbour speed over this much of the fairway. */
	inline constexpr double EaseIn = 9.0;
	/** Shares of her best speed: round the turn in the offing, in the harbour, and the least she ever makes under way. */
	inline constexpr double TurnPace = 0.62, HarbourPace = 0.34, LeastPace = 0.1;
	/** The water she wants on either side of her course. */
	inline constexpr double Clear = 1.3;
	/** How many stations her times are reckoned at, once round. */
	inline constexpr int Stations = 160;
	/** How far off the quay the fairway may end, longest first; and how far it may be swung off the straight way out to sea, least first (degrees). */
	inline constexpr double Reaches[4] = { 30.0, 24.0, 18.0, 13.0 };
	inline constexpr double Swings[7] = { 0.0, -15.0, 15.0, -30.0, 30.0, -45.0, 45.0 };

	/** A port's roads: the fairway its ships work, and the times of once round it. */
	struct FRoads
	{
		bool bOpen = false; // there is water for it
		FSpot Quay; // the quay's edge, on the map
		double Bearing = 0.0; // which way the fairway runs out from there
		double Far = 0.0; // how far off the quay the middle of the turn in the offing lies
		double Length = 0.0; // once round
		double Sail = 0.0; // seconds once round, under way
		double Lie = 0.0; // seconds she lies off the quay between two rounds
		double Start = 0.0; // seconds its clock is ahead of the world's, so that no two ports keep step
		double Times[Stations + 1] = {}; // seconds since she weighed, at each station of the round
	};

	/** A ship of a port, at a moment. */
	struct FAfloat
	{
		FSpot At; // her middle, on the map
		FSpot Ahead; // the way her bows point, a unit
		double Along = 0.0; // how far round she is
		double Pace = 0.0; // her speed as a share of her best; nothing while she lies
		bool bLying = true;
	};

	/** The length of each straight of the fairway. */
	inline double Straight(const FRoads& Roads) { return Roads.Far - NearOff; }

	/**
	 * The round, by how far along it: from her berth out along the right-hand side of the fairway (as one looking to
	 * sea has it), round the turn in the offing, back along the other side, and round up to her berth again.
	 */
	inline void On(const FRoads& Roads, double Along, FSpot& OutAt, FSpot& OutAhead)
	{
		const double Leg = Straight(Roads), Turn = Pi * Half;
		double D = std::fmod(Along, Roads.Length);
		if (D < 0.0)
		{
			D += Roads.Length;
		}
		// (out to sea and across: across is to the right of one looking to sea)
		double Out, Across, AheadOut, AheadAcross;
		if (D < Leg)
		{
			Out = NearOff + D; Across = Half; AheadOut = 1.0; AheadAcross = 0.0;
		}
		else if (D < Leg + Turn)
		{
			const double T = (D - Leg) / Half;
			Out = Roads.Far + Half * std::sin(T); Across = Half * std::cos(T); AheadOut = std::cos(T); AheadAcross = -std::sin(T);
		}
		else if (D < 2.0 * Leg + Turn)
		{
			Out = Roads.Far - (D - Leg - Turn); Across = -Half; AheadOut = -1.0; AheadAcross = 0.0;
		}
		else
		{
			const double T = (D - 2.0 * Leg - Turn) / Half;
			Out = NearOff - Half * std::sin(T); Across = -Half * std::cos(T); AheadOut = -std::cos(T); AheadAcross = std::sin(T);
		}
		const double B = Roads.Bearing * Pi / 180.0;
		const FSpot Sea{ std::sin(B), -std::cos(B) }, Right{ std::cos(B), std::sin(B) };
		OutAt = Roads.Quay + Sea * Out + Right * Across;
		OutAhead = Sea * AheadOut + Right * AheadAcross;
	}

	/** Her speed there, as a share of her best: slow off her berth, eased round the far turn, slow up the harbour, and the way off her as she rounds up. */
	inline double Pace(const FRoads& Roads, double Along)
	{
		const double Leg = Straight(Roads), Turn = Pi * Half, In = 2.0 * Leg + Turn;
		double V = 1.0 - (1.0 - TurnPace) * (Smooth(Leg - 4.0, Leg, Along) - Smooth(Leg + Turn, Leg + Turn + 4.0, Along));
		V = Lerp(V, HarbourPace, Smooth(In - EaseIn, In, Along));
		V = Lerp(V, LeastPace, Smooth(In + 0.4 * Turn, Roads.Length, Along));
		const double Gathered = Lerp(LeastPace, 1.0, Smooth(0.0, Gather, Along));
		return V < Gathered ? V : Gathered;
	}

	/** A fairway laid out from a quay on a bearing, with its times reckoned. Seed sets how long its ships lie and where its clock stands. */
	inline FRoads Lay(FSpot Quay, double Bearing, double Far, int Seed)
	{
		FRoads Roads;
		Roads.bOpen = Far > NearOff + 2.0;
		Roads.Quay = Quay;
		Roads.Bearing = Bearing;
		Roads.Far = Far;
		Roads.Length = 2.0 * (Far - NearOff) + 2.0 * Pi * Half;
		const double Step = Roads.Length / Stations;
		Roads.Times[0] = 0.0;
		for (int K = 0; K < Stations; ++K)
		{
			Roads.Times[K + 1] = Roads.Times[K] + Step / (Cruise * Pace(Roads, (K + 0.5) * Step));
		}
		Roads.Sail = Roads.Times[Stations];
		const int Odd = ((Seed % 1000) + 1000) % 1000;
		Roads.Lie = 18.0 + 4.0 * (Odd % 5);
		Roads.Start = (Roads.Sail + Roads.Lie) * ((Odd * 37) % 100) / 100.0;
		return Roads;
	}

	/** True where the whole round has water under it and Clear of it on either side. Wet(X, Y) says where the sea is. */
	template <typename FWet>
	inline bool Afloat(const FRoads& Roads, FWet&& Wet)
	{
		if (!Roads.bOpen)
		{
			return false;
		}
		const int Looks = static_cast<int>(std::ceil(Roads.Length / 0.75));
		for (int K = 0; K < Looks; ++K)
		{
			FSpot At, Ahead;
			On(Roads, Roads.Length * K / Looks, At, Ahead);
			const FSpot Beam{ -Ahead.Y, Ahead.X };
			for (const double Side : { -1.0, 0.0, 1.0 })
			{
				const FSpot Look = At + Beam * (Side * Clear);
				if (!Wet(Look.X, Look.Y))
				{
					return false;
				}
			}
		}
		return true;
	}

	/**
	 * A port's roads, found: the longest fairway that has water, and of those the one that runs straightest out to
	 * sea. Bearing is the way the open water lies from the quay. Not open where there is no water for the shortest.
	 */
	template <typename FWet>
	inline FRoads Find(FSpot Quay, double Bearing, int Seed, FWet&& Wet)
	{
		for (const double Far : Reaches)
		{
			for (const double Swung : Swings)
			{
				const FRoads Roads = Lay(Quay, Bearing + Swung, Far, Seed);
				if (Afloat(Roads, Wet))
				{
					return Roads;
				}
			}
		}
		FRoads None;
		None.Quay = Quay;
		None.Bearing = Bearing;
		return None;
	}

	/** How far round she is, Since seconds after she weighed. */
	inline double Run(const FRoads& Roads, double Since)
	{
		if (Since <= 0.0)
		{
			return 0.0;
		}
		if (Since >= Roads.Sail)
		{
			return Roads.Length;
		}
		int Low = 0, High = Stations;
		while (High - Low > 1)
		{
			const int Mid = (Low + High) / 2;
			if (Roads.Times[Mid] <= Since)
			{
				Low = Mid;
			}
			else
			{
				High = Mid;
			}
		}
		const double Span = Roads.Times[High] - Roads.Times[Low];
		return Roads.Length * (Low + (Span > 1.0e-9 ? (Since - Roads.Times[Low]) / Span : 0.0)) / Stations;
	}

	/** Where the port's ship is by the clock: the Ship-th of Ships that work it, spaced evenly round its clock. */
	inline FAfloat Where(const FRoads& Roads, double Clock, int Ship, int Ships)
	{
		FAfloat Her;
		const double Lap = Roads.Sail + Roads.Lie;
		if (!Roads.bOpen || Lap <= 0.0)
		{
			Her.At = Roads.Quay;
			Her.Ahead = FSpot{ 0.0, -1.0 };
			return Her;
		}
		double Phase = std::fmod(Clock + Roads.Start + Lap * Ship / (Ships > 0 ? Ships : 1), Lap);
		if (Phase < 0.0)
		{
			Phase += Lap;
		}
		Her.bLying = Phase < Roads.Lie;
		if (!Her.bLying)
		{
			Her.Along = Run(Roads, Phase - Roads.Lie);
			Her.Pace = Pace(Roads, Her.Along);
		}
		On(Roads, Her.Along, Her.At, Her.Ahead);
		return Her;
	}

	/**
	 * The water Back astern of her middle, along the course she has sailed (ahead of her where Back is less than
	 * nothing), and how much of a wake has been made there: none on the water she has not yet crossed since she weighed.
	 */
	inline double Astern(const FRoads& Roads, const FAfloat& Her, double Back, FSpot& OutAt, FSpot& OutAhead)
	{
		const double There = Her.Along - Back;
		On(Roads, There > 0.0 ? There : 0.0, OutAt, OutAhead);
		if (There < 0.0)
		{
			// (astern of her berth nothing was crossed: the point is carried straight back from it, with no wake on it)
			OutAt = OutAt + OutAhead * There;
		}
		return Clamp(There, 0.0, 1.0);
	}

	/** The least distance from a point to the round, looked for at every map unit of it. */
	inline double Off(const FRoads& Roads, FSpot Point)
	{
		double Least = 1.0e9;
		const int Looks = static_cast<int>(std::ceil(Roads.Length));
		for (int K = 0; K < Looks; ++K)
		{
			FSpot At, Ahead;
			On(Roads, Roads.Length * K / Looks, At, Ahead);
			const double Far = std::hypot(At.X - Point.X, At.Y - Point.Y);
			Least = Far < Least ? Far : Least;
		}
		return Least;
	}

	/** True where two ports' rounds come within Room of one another: only one of them is worked. */
	inline bool Foul(const FRoads& One, const FRoads& Other, double Room)
	{
		// (quickly, where they are far apart: neither reaches further from its quay than its far turn and half a fairway)
		const double Apart = std::hypot(One.Quay.X - Other.Quay.X, One.Quay.Y - Other.Quay.Y);
		if (Apart > One.Far + Other.Far + 2.0 * Half + Room)
		{
			return false;
		}
		const int Looks = static_cast<int>(std::ceil(One.Length / 1.5));
		for (int K = 0; K < Looks; ++K)
		{
			FSpot At, Ahead;
			On(One, One.Length * K / Looks, At, Ahead);
			if (Off(Other, At) < Room)
			{
				return true;
			}
		}
		return false;
	}
}
