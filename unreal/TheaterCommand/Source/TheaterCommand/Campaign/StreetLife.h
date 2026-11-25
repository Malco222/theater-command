#pragma once

#include <cmath>

#include "ShipWay.h"

/**
 * (Release 2, the campaign map's upgrade 4, second half: bigger, busier cities. Step 2, street cars. Brian, 25 November
 * 2026: "I want this to visually wow people on how smooth and realistic and detailed grand it looks".) A city on the
 * campaign map has its square and two avenues crossing in it, and nothing moved in them. Here a street car works each
 * arm of an avenue: she stands at her stop by the square, pulls away, runs out to the end of the arm, slows, stands at
 * her terminus and comes back the way she went. She is the same at both ends, as a street car is, and never turns.
 *
 * An avenue is one car wide, and a city's great church or a capital's great building stands on its line in the
 * square: so no car crosses the square, and each arm is a line of its own with one car on it. Nothing can meet.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/street_life/run.sh). The town's builder
 * (FMiniatureMesh::Township) says where each arm's cobble is laid; the map (ACampaignMapActor,
 * Campaign/CampaignStreetLife.cpp) builds the cars and moves them. Nothing is kept from frame to frame: a car is where
 * the clock says she is, so she runs the same at any frame rate and costs nothing while she is not drawn. Decoration
 * only: no rule of the game reads any of it.
 *
 * Measures are in the map's units and seconds.
 */
namespace TheaterStreetLife
{
	using TheaterShipWay::FSpot;

	inline double Clamp(double V, double Low, double High) { return V < Low ? Low : V > High ? High : V; }

	/** The car: her length, breadth and height to the roof. A house on the map is one to two units long, and an avenue 0.6 wide. */
	inline constexpr double CarLong = 1.6, CarWide = 0.28, CarHigh = 0.36;
	/** Her best speed, in map units a second: well under a small ship's 1.3 and a lorry convoy's two. */
	inline constexpr double Cruise = 0.9;
	/** Seconds she takes to gather her way from a stop, and to lose it coming to one. */
	inline constexpr double Ramp = 3.0;
	/** Seconds she stands at her stop by the square, and at her terminus (each line a little more, its own). */
	inline constexpr double StandIn = 7.0, StandOut = 10.0;
	/** Her stop lies this far outside the square's edge, and her terminus this far short of the cobble's end (her middle). */
	inline constexpr double StopClear = 0.5 * CarLong + 0.15, EndClear = 0.5 * CarLong + 0.3;
	/** An arm with less run than this between its two stops has no car. */
	inline constexpr double LeastRun = 3.5;

	/** One arm of an avenue as a car's line: from her stop by the square to her terminus. */
	struct FLine
	{
		bool bOpen = false; // it is long enough to be worked
		FSpot From; // her stop by the square, on the map
		FSpot To; // her terminus
		double Long = 0.0; // from one to the other
		double Peak = 0.0; // the best speed she reaches on it (her Cruise, or less on a short line)
		double Level = 0.0; // seconds she holds it
		double Run = 0.0; // seconds from stop to stop
		double WaitIn = 0.0, WaitOut = 0.0; // seconds she stands at each
		double Start = 0.0; // seconds its clock is ahead of the world's, so that no two lines keep step
	};

	/** A car at a moment. */
	struct FCar
	{
		FSpot At; // her middle, on the map
		FSpot Along; // the way her line runs, from its stop by the square to its terminus (a unit; she lies along it whichever way she goes)
		double Out = 0.0; // how far she is from her stop by the square
		double Speed = 0.0; // map units a second, never less than nothing
		bool bOutward = true; // she is going out, or standing at the square about to
		bool bStanding = true;
	};

	/**
	 * Where a car may run on one arm of an avenue. The avenue's cobble is laid in lengths from Other behind the
	 * square's middle to Reach ahead of it (the town's builder lays each length only where its middle may be built on:
	 * Laid says so, given how far ahead of the middle that is, behind being less than nothing). Square is how far the
	 * square's edge lies from its middle on this arm, and LengthOf the most one length of cobble may be. Her stop is
	 * just outside the square, her terminus just short of where the unbroken cobble ends. False where the cobble by
	 * the square is not laid, or the run is too short.
	 */
	template <typename FLaid>
	inline bool Arm(double Square, double Reach, double Other, double LengthOf, FLaid&& Laid, double& OutFrom, double& OutTo)
	{
		const double Whole = Reach + Other;
		if (Reach <= 0.0 || Other <= 0.0 || LengthOf <= 0.0)
		{
			return false;
		}
		const int Lengths = static_cast<int>(std::fmax(1.0, std::ceil(Whole / LengthOf)));
		const double Each = Whole / Lengths;
		const double Stop = Square + StopClear;
		// (the length her stop lies on, and her whole car with it)
		const int First = static_cast<int>(std::floor((Stop - 0.5 * CarLong + Other) / Each));
		if (Stop + 0.5 * CarLong > Reach || First < 0 || First >= Lengths)
		{
			return false;
		}
		int Last = First - 1;
		for (int K = First; K < Lengths; ++K)
		{
			if (!Laid(-Other + (K + 0.5) * Each))
			{
				break;
			}
			Last = K;
		}
		if (Last < First)
		{
			return false;
		}
		const double End = -Other + (Last + 1) * Each - EndClear;
		if (End - Stop < LeastRun)
		{
			return false;
		}
		OutFrom = Stop;
		OutTo = End;
		return true;
	}

	/** A line from her stop by the square to her terminus. Seed (the province and the arm) sets how long she stands and where its clock begins. */
	inline FLine Lay(FSpot From, FSpot To, int Seed)
	{
		FLine Line;
		Line.From = From;
		Line.To = To;
		Line.Long = std::hypot(To.X - From.X, To.Y - From.Y);
		Line.bOpen = Line.Long >= LeastRun;
		if (!Line.bOpen)
		{
			return Line;
		}
		// (a short line: she never reaches her best, and is slowing as soon as she has gathered way)
		Line.Peak = std::fmin(Cruise, Line.Long / Ramp);
		Line.Level = (Line.Long - Line.Peak * Ramp) / Line.Peak;
		Line.Run = 2.0 * Ramp + Line.Level;
		const int Odd = Seed < 0 ? -Seed : Seed;
		Line.WaitIn = StandIn + (Odd % 4);
		Line.WaitOut = StandOut + ((Odd / 4) % 5);
		const double Round = Line.WaitIn + Line.WaitOut + 2.0 * Line.Run;
		Line.Start = Round * ((Odd * 37) % 100) / 100.0;
		return Line;
	}

	/** How far she is from the stop she left, Since seconds after she pulled away from it. */
	inline double Gone(const FLine& Line, double Since)
	{
		const double T = Clamp(Since, 0.0, Line.Run);
		// (her speed rises as a smooth step over Ramp seconds, holds, and falls the same way: what she has run while it rises is its integral)
		auto Risen = [&Line](double Seconds)
		{
			const double U = Clamp(Seconds / Ramp, 0.0, 1.0);
			return Line.Peak * Ramp * (U * U * U - 0.5 * U * U * U * U);
		};
		if (T <= Ramp)
		{
			return Risen(T);
		}
		if (T <= Ramp + Line.Level)
		{
			return Line.Peak * (0.5 * Ramp + (T - Ramp));
		}
		return Line.Long - Risen(Line.Run - T);
	}

	/** The speed she makes then. */
	inline double Speed(const FLine& Line, double Since)
	{
		if (Since <= 0.0 || Since >= Line.Run)
		{
			return 0.0;
		}
		const double Edge = std::fmin(Since, Line.Run - Since);
		const double U = Clamp(Edge / Ramp, 0.0, 1.0);
		return Line.Peak * U * U * (3.0 - 2.0 * U);
	}

	/** Seconds from one leaving of the square's stop to the next. */
	inline double Round(const FLine& Line) { return Line.WaitIn + Line.WaitOut + 2.0 * Line.Run; }

	/** Where the line's car is at Clock (seconds, the world's). */
	inline FCar Where(const FLine& Line, double Clock)
	{
		FCar Car;
		Car.At = Line.From;
		if (!Line.bOpen)
		{
			return Car;
		}
		Car.Along = FSpot{ (Line.To.X - Line.From.X) / Line.Long, (Line.To.Y - Line.From.Y) / Line.Long };
		const double Lap = Round(Line);
		double Phase = std::fmod(Clock + Line.Start, Lap);
		if (Phase < 0.0)
		{
			Phase += Lap;
		}
		if (Phase < Line.WaitIn)
		{
			Car.Out = 0.0;
		}
		else if (Phase < Line.WaitIn + Line.Run)
		{
			Car.Out = Gone(Line, Phase - Line.WaitIn);
			Car.Speed = Speed(Line, Phase - Line.WaitIn);
			Car.bStanding = false;
		}
		else if (Phase < Line.WaitIn + Line.Run + Line.WaitOut)
		{
			Car.Out = Line.Long;
			Car.bOutward = false;
		}
		else
		{
			const double Back = Phase - Line.WaitIn - Line.Run - Line.WaitOut;
			Car.Out = Line.Long - Gone(Line, Back);
			Car.Speed = Speed(Line, Back);
			Car.bOutward = false;
			Car.bStanding = false;
		}
		Car.At = FSpot{ Line.From.X + Car.Along.X * Car.Out, Line.From.Y + Car.Along.Y * Car.Out };
		return Car;
	}
}
