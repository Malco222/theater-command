// Street cars on the campaign map, their cloud test (Tools/street_life/run.sh): the game's own Campaign/StreetLife.h,
// no engine.

#include "Campaign/StreetLife.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace TheaterStreetLife;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	double Apart(FSpot A, FSpot B) { return std::hypot(A.X - B.X, A.Y - B.Y); }

	/** Dice for the avenues made by chance: the same every run. */
	struct FDice
	{
		std::uint64_t State = 0x9E3779B97F4A7C15ull;
		double Next()
		{
			State ^= State << 13;
			State ^= State >> 7;
			State ^= State << 17;
			return static_cast<double>(State >> 11) / 9007199254740992.0;
		}
		double Between(double Low, double High) { return Low + (High - Low) * Next(); }
	};
}

int main()
{
	// ---- A line: from rest to rest, never faster than her best, never backward.
	const double Longs[6] = { 3.5, 4.2, 6.0, 9.0, 14.0, 22.0 };
	for (const double Long : Longs)
	{
		const std::string Name = "a line " + std::to_string(Long).substr(0, 4) + " long";
		const FLine Line = Lay(FSpot{ 10.0, 20.0 }, FSpot{ 10.0 + Long * 0.6, 20.0 - Long * 0.8 }, 7);
		Check(Line.bOpen, Name + " is worked");
		Check(std::fabs(Line.Long - Long) < 1.0e-9, Name + ": its length is told rightly");
		Check(Line.Peak <= Cruise + 1.0e-12 && Line.Peak > 0.3, Name + ": her best on it is no more than a street car's best, and no crawl");
		Check(Line.Level >= -1.0e-12, Name + ": she holds her best for no less than nothing");
		Check((Long < Cruise * Ramp) == (Line.Peak < Cruise - 1.0e-9), Name + ": only on a short line does she not reach her best");
		Check(std::fabs(Gone(Line, 0.0)) < 1.0e-12 && std::fabs(Gone(Line, Line.Run) - Line.Long) < 1.0e-9, Name + ": she leaves from one stop and comes to the other");
		Check(Speed(Line, 0.0) == 0.0 && Speed(Line, Line.Run) == 0.0, Name + ": at rest at both");
		bool bForward = true, bInBest = true, bGentle = true, bSpeedTrue = true;
		const double Step = 0.01;
		double Before = 0.0, SpeedBefore = 0.0;
		for (double T = Step; T <= Line.Run + 1.0e-9; T += Step)
		{
			const double Now = Gone(Line, T), Made = (Now - Before) / Step, Told = Speed(Line, T - 0.5 * Step);
			bForward = bForward && Now >= Before - 1.0e-12;
			bInBest = bInBest && Made <= Line.Peak * 1.0001;
			bSpeedTrue = bSpeedTrue && std::fabs(Made - Told) < 0.004;
			bGentle = bGentle && std::fabs(Made - SpeedBefore) / Step < 1.55 * Line.Peak / Ramp;
			Before = Now;
			SpeedBefore = Made;
		}
		Check(bForward, Name + ": she never goes backward between stops");
		Check(bInBest, Name + ": never faster than her best on it");
		Check(bSpeedTrue, Name + ": the speed she is said to make is the speed she makes");
		Check(bGentle, Name + ": she gathers and loses way no faster than a street car may");
		// By the clock: no jump over two rounds, standing as long as she should, back in her place a round later.
		const double Lap = Round(Line);
		Check(std::fabs(Lap - (Line.WaitIn + Line.WaitOut + 2.0 * Line.Run)) < 1.0e-12, Name + ": a round is two runs and two stands");
		bool bNoJump = true, bOnLine = true, bWayTold = true;
		double StoodIn = 0.0, StoodOut = 0.0;
		FCar Was = Where(Line, -3.0);
		const double Tick = 0.02;
		for (double Clock = -3.0 + Tick; Clock < 2.0 * Lap; Clock += Tick)
		{
			const FCar Car = Where(Line, Clock);
			bNoJump = bNoJump && Apart(Car.At, Was.At) <= Cruise * Tick * 1.01;
			bOnLine = bOnLine && Car.Out >= -1.0e-9 && Car.Out <= Line.Long + 1.0e-9 && Apart(Car.At, FSpot{ Line.From.X + Car.Along.X * Car.Out, Line.From.Y + Car.Along.Y * Car.Out }) < 1.0e-9;
			bWayTold = bWayTold && (Car.bStanding || (Car.Out >= Was.Out - 1.0e-9) == Car.bOutward || Was.bStanding);
			if (Clock >= 0.0 && Clock < Lap && Car.bStanding)
			{
				(Car.Out < 0.5 * Line.Long ? StoodIn : StoodOut) += Tick;
			}
			Was = Car;
		}
		Check(bNoJump, Name + ": by the clock she never jumps");
		Check(bOnLine, Name + ": she is always on her line");
		Check(bWayTold, Name + ": the way she is said to go is the way she goes");
		Check(std::fabs(StoodIn - Line.WaitIn) < 0.1 && std::fabs(StoodOut - Line.WaitOut) < 0.1, Name + ": she stands at each end as long as she should");
		const FCar One = Where(Line, 12.3), Again = Where(Line, 12.3 + Lap), Third = Where(Line, 12.3 - 5.0 * Lap);
		Check(Apart(One.At, Again.At) < 1.0e-9 && Apart(One.At, Third.At) < 1.0e-9, Name + ": a round later, and five before, she is in the same place");
		Check(std::fabs(std::hypot(One.Along.X, One.Along.Y) - 1.0) < 1.0e-9, Name + ": the way her line runs is a unit");
	}
	// ---- Too short a line has no car; no two lines keep step.
	{
		const FLine Short = Lay(FSpot{ 0.0, 0.0 }, FSpot{ LeastRun - 0.01, 0.0 }, 3);
		Check(!Short.bOpen, "a line under the least run has no car");
		const FCar Idle = Where(Short, 50.0);
		Check(Idle.bStanding && Apart(Idle.At, Short.From) < 1.0e-12, "and asked for one, she stands at its stop");
		int Different = 0;
		for (int Seed = 0; Seed < 40; ++Seed)
		{
			const FLine A = Lay(FSpot{ 0.0, 0.0 }, FSpot{ 12.0, 0.0 }, Seed), B = Lay(FSpot{ 0.0, 0.0 }, FSpot{ 12.0, 0.0 }, Seed + 1);
			Different += std::fabs(Where(A, 100.0).Out - Where(B, 100.0).Out) > 0.2 || Where(A, 100.0).bOutward != Where(B, 100.0).bOutward ? 1 : 0;
		}
		Check(Different >= 30, "lines of the same length with neighbouring seeds do not keep step");
		// (the four arms of one city: the seeds the map gives them)
		const FLine North = Lay(FSpot{ 0.0, -5.0 }, FSpot{ 0.0, -17.0 }, 410 * 4), East = Lay(FSpot{ 4.0, 0.0 }, FSpot{ 16.0, 0.0 }, 410 * 4 + 1);
		Check(std::fabs(Round(North) - Round(East)) > 0.5 || std::fabs(North.Start - East.Start) > 2.0, "two arms of one city do not keep step");
	}
	// ---- An arm of an avenue: worked only where its cobble is laid unbroken from the square.
	{
		const double Square = 4.1, LengthOf = 3.36;
		double From = 0.0, To = 0.0;
		const bool bWhole = Arm(Square, 16.0, 14.0, LengthOf, [](double) { return true; }, From, To);
		Check(bWhole && std::fabs(From - (Square + StopClear)) < 1.0e-12 && std::fabs(To - (16.0 - EndClear)) < 1.0e-9, "a whole avenue: her stop is just outside the square and her terminus just short of the cobble's end");
		Check(From - 0.5 * CarLong > Square, "her whole length is clear of the square at her stop");
		// (the cobble broken 12 units out: by a river, say)
		const bool bBroken = Arm(Square, 16.0, 14.0, LengthOf, [](double Out) { return Out < 12.0; }, From, To);
		Check(bBroken && To < 12.0 + 0.5 * LengthOf && To > From + LeastRun - 1.0e-9, "a broken avenue: she turns back before the break");
		Check(!Arm(Square, 16.0, 14.0, LengthOf, [](double Out) { return Out < 9.0; }, From, To), "broken too near the square: no car");
		Check(!Arm(Square, 16.0, 14.0, LengthOf, [](double Out) { return Out > 8.0 || Out < 0.0; }, From, To), "no cobble by the square: no car");
		Check(!Arm(Square, 7.0, 14.0, LengthOf, [](double) { return true; }, From, To), "too short an arm: no car");
		Check(!Arm(Square, 0.0, 14.0, LengthOf, [](double) { return true; }, From, To) && !Arm(Square, 16.0, 0.0, LengthOf, [](double) { return true; }, From, To), "an avenue with no length one way: no car");
		// Over many avenues made by chance: wherever an arm is worked, every length of cobble under her run is laid.
		FDice Dice;
		int Worked = 0, Bare = 0, Unsound = 0;
		for (int Trial = 0; Trial < 2000; ++Trial)
		{
			const double Reach = Dice.Between(3.0, 30.0), Other = Dice.Between(3.0, 30.0), Edge = Dice.Next() < 0.5 ? 4.1 : 6.3;
			const double Gap = Dice.Next() < 0.5 ? 1.0e9 : Dice.Between(-Other, Reach), Wide = Dice.Between(0.5, 6.0);
			auto Laid = [Gap, Wide](double Out) { return std::fabs(Out - Gap) > 0.5 * Wide; };
			if (!Arm(Edge, Reach, Other, LengthOf, Laid, From, To))
			{
				++Bare;
				continue;
			}
			++Worked;
			const double Whole = Reach + Other;
			const int Lengths = static_cast<int>(std::fmax(1.0, std::ceil(Whole / LengthOf)));
			const double Each = Whole / Lengths;
			bool bSound = From - 0.5 * CarLong > Edge && To + 0.5 * CarLong < Reach + 1.0e-9 && To - From >= LeastRun - 1.0e-9;
			for (double Out = From - 0.5 * CarLong; Out <= To + 0.5 * CarLong; Out += 0.05)
			{
				const int K = static_cast<int>(std::floor((Out + Other) / Each));
				bSound = bSound && K >= 0 && K < Lengths && Laid(-Other + (K + 0.5) * Each);
			}
			Unsound += bSound ? 0 : 1;
		}
		Check(Worked > 600 && Bare > 200, "avenues made by chance: many are worked and many are not");
		Check(Unsound == 0, "and wherever one is worked, there is laid cobble under every part of her run, clear of the square and short of the end");
		std::printf("Avenues: 2000 arms made by chance; %d worked, %d not\n", Worked, Bare);
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
