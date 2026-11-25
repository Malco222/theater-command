// Armies with weight: the cloud's checks on Campaign/ArmyCarry.h (see run.sh).
#include "Campaign/ArmyCarry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace TheaterArmyCarry;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bHolds, const std::string& What)
	{
		++Checks;
		if (!bHolds)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	std::string Num(double V)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.4g", V);
		return Text;
	}

	// The table's measures as the piece has them: a map unit at the size pieces are drawn, and its base.
	const double Unit = 14.0;
	const double Base = 4.3 * Unit;
	const double Reach = HangsOn * Unit;

	/** The same numbers every run. */
	struct FDice
	{
		unsigned long long Seed;
		double Roll()
		{
			Seed = Seed * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<double>((Seed >> 11) & ((1ULL << 53) - 1)) / static_cast<double>(1ULL << 53);
		}
	};

	double Flat(FSpot A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }

	double FromLeg(FSpot P, FSpot A, FSpot B)
	{
		const FSpot Leg = B - A;
		const double Long = Dot(Leg, Leg);
		const double T = Long > 1.0e-12 ? Clamp(Dot(P - A, Leg) / Long, 0.0, 1.0) : 0.0;
		return Size(P - (A + Leg * T));
	}

	double FromRoad(FSpot P, const std::vector<FSpot>& Road)
	{
		double Least = 1.0e30;
		for (std::size_t I = 1; I < Road.size(); ++I)
		{
			Least = std::min(Least, FromLeg(P, Road[I - 1], Road[I]));
		}
		return Least;
	}

	double LengthOf(const std::vector<FSpot>& Road)
	{
		double Long = 0.0;
		for (std::size_t I = 1; I < Road.size(); ++I)
		{
			Long += Size(Road[I] - Road[I - 1]);
		}
		return Long;
	}

	/** Where a share of the way along a road is, as the piece finds it: by the distances at its points. */
	struct FWay
	{
		std::vector<FSpot> Points;
		std::vector<double> Marks;
		explicit FWay(const std::vector<FSpot>& Road) : Points(Road)
		{
			Marks.push_back(0.0);
			for (std::size_t I = 1; I < Points.size(); ++I)
			{
				Marks.push_back(Marks.back() + Size(Points[I] - Points[I - 1]));
			}
		}
		FSpot At(double Along) const
		{
			const double Target = Clamp(Along, 0.0, 1.0) * Marks.back();
			std::size_t Leg = 1;
			while (Leg + 1 < Marks.size() && Marks[Leg] < Target)
			{
				++Leg;
			}
			const double Span = std::max(Marks[Leg] - Marks[Leg - 1], 1.0e-3);
			return Points[Leg - 1] + (Points[Leg] - Points[Leg - 1]) * ((Target - Marks[Leg - 1]) / Span);
		}
	};

	/** The march's time as the piece reckons it for a lifted march of a length. */
	double MarchTime(double Long) { return Clamp(0.55 + Long / 5200.0, 0.75, 2.4); }

	void TheRoad()
	{
		std::vector<FSpot> Out;
		const std::vector<FSpot> Two{ { 0.0, 0.0, 5.0 }, { 400.0, 100.0, 9.0 } };
		Round(Two, 0.8, 9.0 * Unit, Out);
		Check(Out.size() == 2 && Size(Out[0] - Two[0]) == 0.0 && Size(Out[1] - Two[1]) == 0.0, "a road of two points comes back as given");
		const std::vector<FSpot> Line{ { 0.0, 0.0, 0.0 }, { 300.0, 0.0, 0.0 }, { 700.0, 0.0, 0.0 } };
		Round(Line, 0.8, 9.0 * Unit, Out);
		Check(Out.size() == 3, "a straight road is not rounded");
		const std::vector<FSpot> Twice{ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 300.0, 0.0, 0.0 }, { 300.0, 0.0, 0.0 }, { 300.0, 400.0, 0.0 } };
		Round(Twice, 0.8, 9.0 * Unit, Out);
		bool bNoneRepeats = true;
		for (std::size_t I = 1; I < Out.size(); ++I)
		{
			bNoneRepeats = bNoneRepeats && Size(Out[I] - Out[I - 1]) > 1.0e-6;
		}
		Check(bNoneRepeats && Out.size() > 3, "points given twice are dropped, and the corner between is rounded");
		const std::vector<FSpot> Bend{ { 0.0, 0.0, 0.0 }, { 500.0, 0.0, 0.0 }, { 500.0, 500.0, 0.0 } };
		Round(Bend, 0.0, 9.0 * Unit, Out);
		Check(Out.size() == 3, "with nothing asked the road is as given");
		std::vector<FSpot> Wide;
		Round(Bend, 1.0, 1.0e9, Out);
		Round(Bend, 7.0, 1.0e9, Wide);
		Check(Out.size() == Wide.size() && std::fabs(LengthOf(Out) - LengthOf(Wide)) < 1.0e-9, "more than all of a corner is all of it");

		FDice Dice{ 20261125ULL };
		double WorstOff = 0.0, WorstTurn = 0.0, LeastKept = 1.0, WorstCorner = 0.0;
		bool bEnds = true, bShorter = true, bHeights = true, bForward = true;
		for (int Trial = 0; Trial < 400; ++Trial)
		{
			std::vector<FSpot> Road;
			FSpot At{ Dice.Roll() * 4000.0, Dice.Roll() * 4000.0, Dice.Roll() * 30.0 };
			double Heading = Dice.Roll() * 6.283185307179586;
			const int Legs = 2 + static_cast<int>(Dice.Roll() * 5.0);
			Road.push_back(At);
			for (int Leg = 0; Leg < Legs; ++Leg)
			{
				Heading += (Dice.Roll() * 2.0 - 1.0) * 120.0 * Degree; // (a road between neighbours seldom doubles back: to 120 degrees a corner)
				const double Long = 150.0 + Dice.Roll() * 900.0;
				At = At + FSpot{ std::cos(Heading) * Long, std::sin(Heading) * Long, (Dice.Roll() - 0.5) * 20.0 };
				Road.push_back(At);
			}
			const double Much = 0.4 + Dice.Roll() * 0.6, Most = (4.0 + Dice.Roll() * 10.0) * Unit;
			Round(Road, Much, Most, Out);
			bEnds = bEnds && Size(Out.front() - Road.front()) == 0.0 && Size(Out.back() - Road.back()) == 0.0;
			bShorter = bShorter && LengthOf(Out) <= LengthOf(Road) + 1.0e-6;
			LeastKept = std::min(LeastKept, LengthOf(Out) / LengthOf(Road));
			double Low = 1.0e30, High = -1.0e30;
			for (const FSpot& Point : Road)
			{
				Low = std::min(Low, Point.Z);
				High = std::max(High, Point.Z);
			}
			for (std::size_t I = 0; I < Out.size(); ++I)
			{
				WorstOff = std::max(WorstOff, FromRoad(Out[I], Road) / Most);
				bHeights = bHeights && Out[I].Z >= Low - 1.0e-9 && Out[I].Z <= High + 1.0e-9;
				if (I >= 2)
				{
					const FSpot A = Out[I - 1] - Out[I - 2], B = Out[I] - Out[I - 1];
					const double Turn = std::acos(Clamp(Dot(A, B) / (Size(A) * Size(B)), -1.0, 1.0));
					WorstTurn = std::max(WorstTurn, Turn);
				}
				bForward = bForward && (I == 0 || Size(Out[I] - Out[I - 1]) > 1.0e-6);
			}
			for (std::size_t Corner = 1; Corner + 1 < Road.size(); ++Corner)
			{
				WorstCorner = std::max(WorstCorner, FromRoad(Road[Corner], Out) / Most);
			}
		}
		Check(bEnds, "the rounded road begins and ends exactly where the road does");
		Check(bShorter, "the rounded road is never longer than the road");
		Check(LeastKept > 0.8, "and never much shorter: the least kept was " + Num(LeastKept));
		Check(bHeights, "its heights lie between the road's own");
		Check(bForward, "no two of its points are the same");
		Check(WorstOff <= 0.5 + 1.0e-6, "it is never further from the road than half the most a corner is cut by: " + Num(WorstOff));
		Check(WorstCorner <= 0.5 + 1.0e-6, "and it passes every corner within that: " + Num(WorstCorner));
		Check(WorstTurn < 30.0 * Degree, "no point of the rounded road turns by 30 degrees: the most was " + Num(WorstTurn / Degree));
	}

	void TheCarry()
	{
		for (double Seconds : { 0.28, 0.5, 0.75, 0.9, 1.2, 1.7, 2.4, 3.2 })
		{
			const std::string For = " (a march of " + Num(Seconds) + " s)";
			const double Up = UpShare(Seconds), Down = DownShare(Seconds);
			Check(Up > 0.0 && Down > 0.0 && Up + Down < 0.5, "it is carried level for at least half its time" + For);
			if (Seconds >= 0.75)
			{
				Check(Up * Seconds > 0.14 && Up * Seconds < 0.33, "it is taken up in about a quarter of a second: " + Num(Up * Seconds) + For);
				Check(Down * Seconds > 0.17 && Down * Seconds < 0.37, "and set down in about a quarter: " + Num(Down * Seconds) + For);
			}
			const FHeld Start = Lifted(0.0, Seconds), End = Lifted(1.0, Seconds), Middle = Lifted(0.5, Seconds);
			Check(Start.Along == 0.0 && Start.High == 0.0, "it starts on the table where it stood" + For);
			Check(End.Along == 1.0 && std::fabs(End.High) < 1.0e-12, "it ends on the table at its place" + For);
			Check(Middle.High == 1.0 && std::fabs(Middle.Along - 0.5) < 0.1, "half way it is at its full height and about half way along" + For);
			bool bOnward = true, bWithin = true;
			double Jump = 0.0, HighAsItGoes = 1.0, HighOverPlace = 0.0;
			FHeld Was = Start;
			const int Parts = 4000;
			for (int Part = 1; Part <= Parts; ++Part)
			{
				const FHeld Now = Lifted(static_cast<double>(Part) / Parts, Seconds);
				bOnward = bOnward && Now.Along >= Was.Along;
				bWithin = bWithin && Now.High >= 0.0 && Now.High <= 1.0 && Now.Along >= 0.0 && Now.Along <= 1.0;
				Jump = std::max(Jump, std::max(std::fabs(Now.Along - Was.Along), std::fabs(Now.High - Was.High)));
				if (Was.Along < 0.01 && Now.Along >= 0.01)
				{
					HighAsItGoes = Now.High;
				}
				if (Was.Along < 0.999 && Now.Along >= 0.999)
				{
					HighOverPlace = Now.High;
				}
				Was = Now;
			}
			Check(bOnward, "it never goes back along its road" + For);
			Check(bWithin, "its height and its way stay between nothing and all" + For);
			Check(Jump < 0.006, "it never jumps: " + Num(Jump) + For);
			Check(HighAsItGoes >= 0.5, "it is clear of the table before it goes: " + Num(HighAsItGoes) + For);
			Check(HighOverPlace >= 0.7, "it is over its place before it comes down the last of the way: " + Num(HighOverPlace) + For);
			const double LandsAt = (Lifted(1.0 - 1.0e-4, Seconds).High - End.High) / (1.0e-4 * Seconds);
			Check(LandsAt > 5.0, "it lands moving, at over five lifts a second: " + Num(LandsAt) + For);
			const double LeavesAt = Lifted(1.0e-4, Seconds).High / (1.0e-4 * Seconds);
			Check(LeavesAt < 0.2 * LandsAt, "and leaves the table far more gently than it lands" + For);
		}
		const FHeld Start = FromHand(0.0), End = FromHand(1.0);
		Check(Start.Along == 0.0 && Start.High == 1.0, "from the hand it starts where it was held");
		Check(End.Along == 1.0 && std::fabs(End.High) < 1.0e-12, "and ends on its place");
		bool bOnward = true, bDown = true;
		double OverAt = 0.0;
		FHeld Was = Start;
		for (int Part = 1; Part <= 4000; ++Part)
		{
			const FHeld Now = FromHand(Part / 4000.0);
			bOnward = bOnward && Now.Along >= Was.Along;
			bDown = bDown && Now.High <= Was.High && Now.High >= 0.0;
			if (Was.Along < 0.999 && Now.Along >= 0.999)
			{
				OverAt = Now.High;
			}
			Was = Now;
		}
		Check(bOnward && bDown, "from the hand it goes on and down, never back or up");
		Check(OverAt >= 0.7, "from the hand it is over its place while still high: " + Num(OverAt));
		Check((FromHand(1.0 - 1.0e-4).High - End.High) / 1.0e-4 > 2.0, "and it too lands moving");
	}

	/** A lifted march of a length along +X at a frame rate: the most it leans each way, its lean every tenth of a second, as it lands, and its rock after. */
	struct FRun
	{
		double Fore = 0.0, Back = 0.0, AtLanding = 0.0, RockFore = 0.0, RockBack = 0.0, StillAfter = -1.0, LaterSwing = 0.0, MostRise = 0.0;
		std::vector<double> Tenths;
	};
	FRun March(double Long, int Frames, double Scale = 1.0, int HitchAt = -1)
	{
		FRun Run;
		const double Seconds = MarchTime(Long);
		FHang Hang;
		auto Where = [&](double At) { return FSpot{ Lifted(At / Seconds, Seconds).Along * Long * Scale, 0.0, 0.0 }; };
		for (int Frame = 0; static_cast<double>(Frame) / Frames < Seconds - 1.0e-9; ++Frame)
		{
			const int Next = Frame == HitchAt ? Frame + Frames : Frame + 1; // (a hitch: one frame a second long)
			const double To = std::min(static_cast<double>(Next) / Frames, Seconds);
			Carried(Hang, Where, static_cast<double>(Frame) / Frames, To, Seconds, Reach * Scale);
			const FLean Leans = Lean(Hang, Reach * Scale, MostLean);
			Run.Fore = std::max(Run.Fore, Leans.X);
			Run.Back = std::min(Run.Back, Leans.X);
			if (Next % (Frames / 10) == 0)
			{
				Run.Tenths.push_back(Leans.X);
			}
			Frame = Next - 1;
		}
		Run.AtLanding = Lean(Hang, Reach * Scale, MostLean).X;
		Kick(Hang, 1.0, 0.0, Landing, Reach * Scale);
		auto Stands = [&](double) { return FSpot{ Long * Scale, 0.0, 0.0 }; };
		int Turns = 0;
		double Was = Run.AtLanding;
		Run.RockFore = Run.RockBack = Run.AtLanding;
		for (int Frame = 0; Frame < 3 * Frames; ++Frame)
		{
			Swing(Hang, Stands, static_cast<double>(Frame) / Frames, static_cast<double>(Frame + 1) / Frames, OnTheTable, Reach * Scale);
			const FLean Leans = Lean(Hang, Reach * Scale, MostLean);
			Run.RockFore = std::max(Run.RockFore, Leans.X);
			Run.RockBack = std::min(Run.RockBack, Leans.X);
			Run.MostRise = std::max(Run.MostRise, Set(Leans, Base).Rise);
			if ((Was > 0.0) != (Leans.X > 0.0))
			{
				++Turns;
			}
			if (Turns >= 2)
			{
				Run.LaterSwing = std::max(Run.LaterSwing, std::fabs(Leans.X));
			}
			Was = Leans.X;
			if ((Frame + 1) % (Frames / 10) == 0)
			{
				Run.Tenths.push_back(Leans.X);
			}
			if (Still(Hang, Reach * Scale))
			{
				Run.StillAfter = static_cast<double>(Frame + 1) / Frames;
				break;
			}
		}
		return Run;
	}

	void TheHang()
	{
		{
			FHang Hang;
			auto Rests = [](double) { return FSpot{ 120.0, -40.0, 0.0 }; };
			for (int Frame = 0; Frame < 120; ++Frame)
			{
				Swing(Hang, Rests, Frame / 60.0, (Frame + 1) / 60.0, InTheAir, Reach);
			}
			Check(Hang.X == 0.0 && Hang.Y == 0.0 && Still(Hang, Reach), "a piece held still hangs straight");
			FHang Going;
			Going.HandRateX = 900.0;
			Going.HandRateY = -300.0;
			auto Steady = [](double At) { return FSpot{ 900.0 * At, -300.0 * At, 0.0 }; };
			double Worst = 0.0;
			for (int Frame = 0; Frame < 120; ++Frame)
			{
				Swing(Going, Steady, Frame / 60.0, (Frame + 1) / 60.0, InTheAir, Reach);
				Worst = std::max(Worst, std::max(std::fabs(Going.X), std::fabs(Going.Y)));
			}
			Check(Worst < 1.0e-6, "a piece carried at a steady speed hangs straight: " + Num(Worst));
		}
		for (double Long : { 150.0, 250.0, 600.0, 1500.0, 3000.0, 6000.0, 12000.0 })
		{
			const std::string For = " (a march of " + Num(Long) + ")";
			const FRun Run = March(Long, 60);
			Check(Run.Fore > 1.0 * Degree, "as the hand gathers speed its top leans the way it goes: " + Num(Run.Fore / Degree) + For);
			Check(Run.Back < -0.5 * Degree, "as the hand slows its foot swings on under it: " + Num(Run.Back / Degree) + For);
			Check(Run.Fore <= MostLean && -Run.Back <= MostLean, "it never leans past its most" + For);
			Check(std::fabs(Run.AtLanding) < 1.8 * Degree, "it lands all but upright: " + Num(Run.AtLanding / Degree) + For);
			Check(Run.RockFore > 1.4 * Degree && Run.RockFore < 2.6 * Degree, "it rocks on about two degrees as it lands: " + Num(Run.RockFore / Degree) + For);
			Check(Run.RockBack < -0.3 * Degree && Run.RockBack > -1.7 * Degree, "and once back the other way: " + Num(Run.RockBack / Degree) + For);
			Check(Run.LaterSwing < 0.7 * Degree, "after that there is little left to see: " + Num(Run.LaterSwing / Degree) + For);
			Check(Run.StillAfter > 0.2 && Run.StillAfter < 0.7, "it is still within seven tenths of a second: " + Num(Run.StillAfter) + For);
			Check(Run.MostRise < Base * std::sin(2.6 * Degree), "its base lifts no more than a rock of two and a half degrees needs" + For);
			for (int Frames : { 20, 30, 120 })
			{
				const FRun Other = March(Long, Frames);
				double Apart = 0.0;
				for (std::size_t Tenth = 0; Tenth < std::min(Run.Tenths.size(), Other.Tenths.size()); ++Tenth)
				{
					Apart = std::max(Apart, std::fabs(Run.Tenths[Tenth] - Other.Tenths[Tenth]));
				}
				Check(Apart < 0.25 * Degree, "the same lean every tenth of a second at " + Num(Frames) + " frames a second as at 60: apart by " + Num(Apart / Degree) + For);
				Check(std::fabs(Other.AtLanding - Run.AtLanding) < 0.2 * Degree, "and the same lean as it lands: " + Num(Other.AtLanding / Degree) + For);
			}
			const FRun Scaled = March(Long, 60, 10.0);
			Check(std::fabs(Scaled.Fore - Run.Fore) < 1.0e-6 && std::fabs(Scaled.RockFore - Run.RockFore) < 1.0e-6, "a table ten times the size shows the same angles" + For);
			const FRun Hitched = March(Long, 60, 1.0, static_cast<int>(18.0 * MarchTime(Long)));
			Check(Hitched.Fore <= MostLean && -Hitched.Back <= MostLean && Hitched.StillAfter > 0.0 && Hitched.StillAfter < 0.9,
				"a frame a second long does it no harm: still after " + Num(Hitched.StillAfter) + For);
		}
		// Round a bend: a quarter turn to the left of its way at a steady speed. Its foot swings wide, so its top leans into the turn.
		{
			FHang Hang;
			const double Speed = 1500.0, Radius = 260.0;
			Hang.HandRateX = Speed;
			auto Bend = [&](double At)
			{
				const double Ran = Speed * At, Straight = 300.0, Arc = Radius * 1.5707963267948966;
				if (Ran < Straight)
				{
					return FSpot{ Ran, 0.0, 0.0 };
				}
				if (Ran < Straight + Arc)
				{
					const double Angle = (Ran - Straight) / Radius;
					return FSpot{ Straight + Radius * std::sin(Angle), Radius * (1.0 - std::cos(Angle)), 0.0 };
				}
				return FSpot{ Straight + Radius, Radius + (Ran - Straight - Arc), 0.0 };
			};
			double MostIn = 0.0, MostOut = 0.0;
			for (int Frame = 0; Frame < 60; ++Frame)
			{
				Swing(Hang, Bend, Frame / 60.0, (Frame + 1) / 60.0, InTheAir, Reach);
				const double Ran = Speed * (Frame + 1) / 60.0;
				if (Ran > 300.0 && Ran < 300.0 + Radius * 1.5707963267948966)
				{
					const double Angle = (Ran - 300.0) / Radius;
					const FLean Leans = Lean(Hang, Reach, MostLean);
					const double Inward = -std::sin(Angle) * Leans.X + std::cos(Angle) * Leans.Y; // toward the middle of the turn
					MostIn = std::max(MostIn, Inward);
					MostOut = std::min(MostOut, Inward);
				}
			}
			Check(MostIn > 2.0 * Degree, "round a bend its top leans into the turn (its foot swings wide): " + Num(MostIn / Degree));
			Check(MostOut > -0.2 * Degree, "and never out of it: " + Num(MostOut / Degree));
		}
		// The same march on the other three ways: the lean turns with the road.
		{
			const double Long = 900.0, Seconds = MarchTime(Long);
			double Peak[4][2] = {};
			for (int Way = 0; Way < 4; ++Way)
			{
				const double Angle = Way * 1.5707963267948966;
				FHang Hang;
				auto Where = [&](double At) { const double Ran = Lifted(At / Seconds, Seconds).Along * Long; return FSpot{ std::cos(Angle) * Ran, std::sin(Angle) * Ran, 0.0 }; };
				for (double At = 0.0; At < 0.4 * Seconds; At += 1.0 / 60.0)
				{
					Swing(Hang, Where, At, At + 1.0 / 60.0, InTheAir, Reach);
					const FLean Leans = Lean(Hang, Reach, MostLean);
					const double On = std::cos(Angle) * Leans.X + std::sin(Angle) * Leans.Y, Off = -std::sin(Angle) * Leans.X + std::cos(Angle) * Leans.Y;
					Peak[Way][0] = std::max(Peak[Way][0], On);
					Peak[Way][1] = std::max(Peak[Way][1], std::fabs(Off));
				}
			}
			for (int Way = 1; Way < 4; ++Way)
			{
				Check(std::fabs(Peak[Way][0] - Peak[0][0]) < 1.0e-9 && Peak[Way][1] < 1.0e-9, "the lean is the same whichever way the road runs (way " + Num(Way) + ")");
			}
		}
		// A hand that throws the piece about: never past its most, and a still hand lets it come to rest.
		{
			FDice Dice{ 77ULL };
			FHang Hang;
			FSpot Hand{}, Next{};
			double Worst = 0.0;
			for (int Frame = 0; Frame < 600; ++Frame)
			{
				const FSpot From = Hand;
				Next = Frame < 400 ? FSpot{ Hand.X + (Dice.Roll() - 0.5) * 900.0, Hand.Y + (Dice.Roll() - 0.5) * 900.0, 0.0 } : Hand;
				const FSpot To = Next;
				const double Start = Frame / 60.0;
				auto Moved = [&](double At) { return From + (To - From) * Clamp((At - Start) * 60.0, 0.0, 1.0); };
				Swing(Hang, Moved, Start, Start + 1.0 / 60.0, InTheAir, Reach);
				const FLean Leans = Lean(Hang, Reach, MostLean);
				Worst = std::max(Worst, std::sqrt(Leans.X * Leans.X + Leans.Y * Leans.Y));
				Hand = Next;
			}
			Check(Worst <= MostLean, "thrown about in the hand it never leans past its most: " + Num(Worst / Degree));
			Check(Still(Hang, Reach), "and held still again it comes to rest");
		}
	}

	void TheSet()
	{
		const FSet Upright = Set(FLean{}, Base);
		Check(Upright.UpX == 0.0 && Upright.UpY == 0.0 && Upright.UpZ == 1.0 && Upright.Rise == 0.0, "no lean: its pole points straight up and it stands on the table");
		FDice Dice{ 5ULL };
		bool bUnit = true, bWay = true, bOnTable = true;
		for (int Trial = 0; Trial < 300; ++Trial)
		{
			const double Angle = Dice.Roll() * MostLean, Way = Dice.Roll() * 6.283185307179586;
			const FLean Leans{ std::cos(Way) * Angle, std::sin(Way) * Angle };
			const FSet Out = Set(Leans, Base);
			bUnit = bUnit && std::fabs(Out.UpX * Out.UpX + Out.UpY * Out.UpY + Out.UpZ * Out.UpZ - 1.0) < 1.0e-12;
			bWay = bWay && Out.UpX * std::cos(Way) + Out.UpY * std::sin(Way) >= 0.0 && std::fabs(-Out.UpX * std::sin(Way) + Out.UpY * std::cos(Way)) < 1.0e-12 && Out.UpZ > 0.98;
			// The rim of its base, turned as the piece is (the shortest turn that takes straight up to its pole): how high each point of it then lies.
			double Lowest = 1.0e30;
			for (int Point = 0; Point < 72; ++Point)
			{
				const double Round = Point * 6.283185307179586 / 72.0;
				Lowest = std::min(Lowest, Out.Rise - Base * (std::cos(Round) * Out.UpX + std::sin(Round) * Out.UpY));
			}
			bOnTable = bOnTable && Lowest > -1.0e-9 && Lowest < 0.02 * Base;
		}
		Check(bUnit, "the way its pole points is a unit");
		Check(bWay, "and it points the way it leans");
		Check(bOnTable, "the low edge of its base stays on the table: never under it, never off it");
	}

	void TheDust()
	{
		Check(std::fabs(Heft(3) - 1.0) < 1.0e-12 && Heft(1) < Heft(3) && Heft(6) > Heft(3) && Heft(40) == Heft(8) && Heft(0) >= 0.8, "more of an army lands harder, within bounds");
		for (double Much : { Heft(1), Heft(3), Heft(8) })
		{
			const std::string For = " (heft " + Num(Much) + ")";
			Check(Skirt(0.0, Much).Thick == 0.0 && Skirt(SkirtLasts, Much).Thick == 0.0 && Skirt(-1.0, Much).Thick == 0.0, "the low ring begins and ends as nothing" + For);
			bool bOut = true, bThin = true;
			double Was = 0.0, Jump = 0.0, WasThick = 0.0, Thickest = 0.0, RunsOutBy = 0.0;
			for (int Part = 1; Part < 600; ++Part)
			{
				const FDust Dust = Skirt(SkirtLasts * Part / 600.0, Much);
				bOut = bOut && Dust.Out >= Was && Dust.Size > 0.0 && Dust.High > 0.0 && Dust.High < 0.5;
				bThin = bThin && Dust.Thick >= 0.0 && Dust.Thick <= 0.6;
				Jump = std::max(Jump, std::fabs(Dust.Thick - WasThick));
				Thickest = std::max(Thickest, Dust.Thick);
				if (Part == 200)
				{
					RunsOutBy = (Dust.Out - Skirt(1.0e-6, Much).Out) / (Skirt(SkirtLasts - 1.0e-6, Much).Out - Skirt(1.0e-6, Much).Out);
				}
				Was = Dust.Out;
				WasThick = Dust.Thick;
			}
			Check(bOut, "the low ring runs outward and stays low" + For);
			Check(bThin && Thickest > 0.3, "it is seen, and never thick: " + Num(Thickest) + For);
			Check(Jump < 0.02, "it does not pop: " + Num(Jump) + For);
			Check(RunsOutBy > 0.6, "it runs out quickest at first: " + Num(RunsOutBy) + " of the way in a third of its time" + For);
			for (int Ground = 0; Ground < 3; ++Ground)
			{
				const double Lasts = BillowLasts(Ground);
				Check(Billow(0.0, Much, Ground).Thick == 0.0 && Billow(Lasts, Much, Ground).Thick == 0.0, "the cloud begins and ends as nothing" + For);
				bool bRises = true;
				double High = 0.0, MostThick = 0.0, BillowJump = 0.0, BillowWas = 0.0;
				for (int Part = 1; Part < 600; ++Part)
				{
					const FDust Dust = Billow(Lasts * Part / 600.0, Much, Ground);
					bRises = bRises && Dust.High >= High && Dust.Thick >= 0.0 && Dust.Thick <= 0.46;
					BillowJump = std::max(BillowJump, std::fabs(Dust.Thick - BillowWas));
					High = Dust.High;
					BillowWas = Dust.Thick;
					MostThick = std::max(MostThick, Dust.Thick);
				}
				Check(bRises && MostThick > 0.2 && BillowJump < 0.02, "the cloud rises, is seen, never thick, and does not pop" + For);
			}
			Check(Billow(1.0, Much, 1).High < Billow(1.0, Much, 0).High && BillowLasts(1) < BillowLasts(0) && BillowLasts(2) > BillowLasts(0), "snow falls back sooner than dust, and sand hangs longer" + For);
			Check(Skirt(0.3, Much).Out > Billow(0.3, Much, 0).Out, "the low ring runs out ahead of the cloud" + For);
		}
		Check(Skirt(0.3, Heft(8)).Out > Skirt(0.3, Heft(1)).Out && Billow(0.5, Heft(8), 0).Size > Billow(0.5, Heft(1), 0).Size, "a bigger army raises more of both");
	}

	/** Whole marches as the piece makes them, frame by frame at 60 a second, on rounded roads. */
	void WholeMarches()
	{
		FDice Dice{ 99ULL };
		double WorstStep = 0.0, WorstLean = 0.0, WorstLanding = 0.0, WorstTilt = 0.0;
		bool bArrives = true;
		for (int Trial = 0; Trial < 200; ++Trial)
		{
			std::vector<FSpot> Road, Rounded;
			FSpot At{ Dice.Roll() * 3000.0, Dice.Roll() * 3000.0, 0.0 };
			double Heading = Dice.Roll() * 6.283185307179586;
			Road.push_back(At);
			const int Legs = 1 + static_cast<int>(Dice.Roll() * 5.0);
			for (int Leg = 0; Leg < Legs; ++Leg)
			{
				Heading += (Dice.Roll() * 2.0 - 1.0) * 110.0 * Degree;
				const double Long = 250.0 + Dice.Roll() * 700.0;
				At = At + FSpot{ std::cos(Heading) * Long, std::sin(Heading) * Long, 0.0 };
				Road.push_back(At);
			}
			Round(Road, 0.8, 9.0 * Unit, Rounded);
			const FWay Way(Rounded);
			const double Seconds = MarchTime(Way.Marks.back());
			FHang Hang;
			auto Where = [&](double Time) { return Way.At(Lifted(Time / Seconds, Seconds).Along); };
			FSpot Was = Where(0.0);
			FLean WasLean{};
			const double Fastest = 1.875 * Way.Marks.back() / (0.6 * Seconds); // (the quickest this ease ever goes, with room)
			for (double Time = 0.0; Time < Seconds - 1.0e-9; Time += 1.0 / 60.0)
			{
				const double To = std::min(Time + 1.0 / 60.0, Seconds);
				Carried(Hang, Where, Time, To, Seconds, Reach);
				const FSpot Now = Where(To);
				WorstStep = std::max(WorstStep, Flat(Now - Was) / (Fastest / 60.0));
				const FLean Leans = Lean(Hang, Reach, MostLean);
				WorstLean = std::max(WorstLean, std::sqrt(Leans.X * Leans.X + Leans.Y * Leans.Y));
				WorstTilt = std::max(WorstTilt, std::sqrt((Leans.X - WasLean.X) * (Leans.X - WasLean.X) + (Leans.Y - WasLean.Y) * (Leans.Y - WasLean.Y)));
				Was = Now;
				WasLean = Leans;
			}
			bArrives = bArrives && Size(Was - Road.back()) < 1.0e-6;
			const FLean Landing = Lean(Hang, Reach, MostLean);
			WorstLanding = std::max(WorstLanding, std::sqrt(Landing.X * Landing.X + Landing.Y * Landing.Y));
		}
		if (std::getenv("ARMYCARRY_SAY"))
		{
			std::printf("whole marches: quickest frame %.3f of its ease, most lean %.2f, most change in a frame %.2f, most lean at landing %.2f (degrees)\n",
				WorstStep, WorstLean / Degree, WorstTilt / Degree, WorstLanding / Degree);
		}
		Check(bArrives, "every march ends exactly on its place");
		Check(WorstStep <= 1.0, "no frame carries it further than its ease allows: " + Num(WorstStep));
		Check(WorstLean <= MostLean, "on a rounded road it never leans past its most: " + Num(WorstLean / Degree));
		Check(WorstTilt < 2.6 * Degree, "it is never whipped: its lean changes by under two and a half degrees in a frame: " + Num(WorstTilt / Degree));
		Check(WorstLanding < 2.2 * Degree, "it lands near enough upright off any road: " + Num(WorstLanding / Degree));
	}
}

int main()
{
	TheRoad();
	TheCarry();
	TheHang();
	TheSet();
	TheDust();
	WholeMarches();
	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
