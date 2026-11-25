// Busy ports on the campaign map, their cloud test (Tools/port_life/run.sh): the game's own Campaign/PortLife.h, no
// engine. Given a file name it also writes one port's scene (the land, the quay, the round, and where its two ships
// are every three seconds of a lap) as JSON for Tools/port_life/picture.py.

#include "Campaign/PortLife.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace TheaterPortLife;

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

	double Far(FSpot A, FSpot B) { return std::hypot(A.X - B.X, A.Y - B.Y); }

	/** Degrees between two unit vectors. */
	double Swing(FSpot A, FSpot B)
	{
		const double Dot = Clamp(A.X * B.X + A.Y * B.Y, -1.0, 1.0);
		return std::acos(Dot) * 180.0 / Pi;
	}

	/** A coast, for a test: land everywhere south of a line, and round islands and headlands. */
	struct FCoast
	{
		struct FIsle { FSpot Mid; double Radius; };
		std::vector<FIsle> Isles;
		double ShoreY = 0.0; // land where Y is over this (south of it)
		bool Wet(double X, double Y) const
		{
			if (Y > ShoreY)
			{
				return false;
			}
			for (const FIsle& Isle : Isles)
			{
				if (std::hypot(X - Isle.Mid.X, Y - Isle.Mid.Y) < Isle.Radius)
				{
					return false;
				}
			}
			return true;
		}
	};

	/** Chance, the same every run. */
	struct FDice
	{
		unsigned State = 20261125u;
		double Roll()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>((State >> 8) & 0xFFFFFFu) / static_cast<double>(0x1000000u);
		}
		double Between(double Low, double High) { return Low + (High - Low) * Roll(); }
	};
}

int main(int ArgC, char** ArgV)
{
	// ---- 1. The round: closed, no corner, where it should be.
	for (const double Reach : Reaches)
	{
		for (const double Bearing : { 0.0, 37.0, 90.0, 200.0, 315.0 })
		{
			const FRoads Roads = Lay(FSpot{ 100.0, 80.0 }, Bearing, Reach, 7);
			const std::string Name = "round " + std::to_string(static_cast<int>(Reach)) + " on " + std::to_string(static_cast<int>(Bearing));
			Check(Roads.bOpen, Name + ": open");
			Check(std::fabs(Roads.Length - (2.0 * (Reach - NearOff) + 2.0 * Pi * Half)) < 1.0e-9, Name + ": its length is two straights and two half circles");
			FSpot First, FirstAhead, Last, LastAhead;
			On(Roads, 0.0, First, FirstAhead);
			On(Roads, Roads.Length - 1.0e-7, Last, LastAhead);
			Check(Far(First, Last) < 1.0e-5 && Swing(FirstAhead, LastAhead) < 0.01, Name + ": it closes on her berth, heading as she lay");
			const double B = Bearing * Pi / 180.0;
			Check(std::fabs(FirstAhead.X - std::sin(B)) < 1.0e-9 && std::fabs(FirstAhead.Y + std::cos(B)) < 1.0e-9, Name + ": she lies with her head to sea");
			const int Steps = 4000;
			double WorstStep = 0.0, WorstSwing = 0.0, Nearest = 1.0e9, Furthest = 0.0, Longest = 0.0;
			FSpot Before = First, BeforeAhead = FirstAhead;
			for (int K = 1; K <= Steps; ++K)
			{
				FSpot At, Ahead;
				On(Roads, Roads.Length * K / Steps, At, Ahead);
				WorstStep = std::fmax(WorstStep, std::fabs(Far(At, Before) - Roads.Length / Steps));
				WorstSwing = std::fmax(WorstSwing, Swing(Ahead, BeforeAhead));
				Longest = std::fmax(Longest, std::fabs(std::hypot(Ahead.X, Ahead.Y) - 1.0));
				Nearest = std::fmin(Nearest, Far(At, Roads.Quay));
				Furthest = std::fmax(Furthest, Far(At, Roads.Quay));
				Before = At;
				BeforeAhead = Ahead;
			}
			Check(WorstStep < 1.0e-4, Name + ": no gap and no doubling back anywhere on it");
			// (a step of the round on a turn swings her by the step over the radius, and no more)
			Check(WorstSwing < (Roads.Length / Steps) / Half * 180.0 / Pi * 1.01 + 1.0e-6, Name + ": her heading never jumps");
			Check(Longest < 1.0e-9, Name + ": her heading is a unit everywhere");
			Check(Nearest > NearOff - Half - 1.0e-6, Name + ": never nearer the quay than the ships alongside allow");
			Check(Furthest < Reach + Half + 1.0e-6, Name + ": never further out than her far turn");
		}
	}

	// ---- 2. Her times.
	for (const double Reach : Reaches)
	{
		const FRoads Roads = Lay(FSpot{ 0.0, 0.0 }, 0.0, Reach, 3);
		const std::string Name = "times, reach " + std::to_string(static_cast<int>(Reach));
		bool bForward = true;
		for (int K = 0; K < Stations; ++K)
		{
			bForward = bForward && Roads.Times[K + 1] > Roads.Times[K];
		}
		Check(bForward, Name + ": her clock runs forward at every station");
		Check(Roads.Sail > Roads.Length / Cruise && Roads.Sail < Roads.Length / (Cruise * 0.3), Name + ": once round takes longer than at her best and under three times as long");
		Check(Roads.Lie >= 18.0 && Roads.Lie <= 34.0 && Roads.Lie < 0.5 * (Roads.Sail + Roads.Lie), Name + ": she lies 18 to 34 seconds, under half her lap");
		Check(std::fabs(Run(Roads, 0.0)) < 1.0e-12 && std::fabs(Run(Roads, Roads.Sail) - Roads.Length) < 1.0e-9, Name + ": she starts at her berth and ends there");
		const double Tick = 1.0 / 60.0;
		double Fastest = 0.0, Slowest = 1.0e9, QuickestHead = 0.0, HarbourHead = 0.0, UpHarbour = 0.0;
		const double In = 2.0 * Straight(Roads) + Pi * Half;
		for (double Since = Tick; Since < Roads.Sail; Since += Tick)
		{
			const double Was = Run(Roads, Since - Tick), Is = Run(Roads, Since);
			const double Speed = (Is - Was) / Tick;
			Fastest = std::fmax(Fastest, Speed);
			Slowest = std::fmin(Slowest, Speed);
			FSpot A, AheadA, B, AheadB;
			On(Roads, Was, A, AheadA);
			On(Roads, Is, B, AheadB);
			const double Head = Swing(AheadA, AheadB) / Tick;
			QuickestHead = std::fmax(QuickestHead, Head);
			if (Is > In)
			{
				HarbourHead = std::fmax(HarbourHead, Head);
				UpHarbour = std::fmax(UpHarbour, Speed);
			}
		}
		Check(Fastest < Cruise * 1.02, Name + ": never faster than her best");
		Check(Slowest > Cruise * LeastPace * 0.9, Name + ": never stopped while under way");
		Check(QuickestHead < 17.0, Name + ": her head never swings faster than 17 degrees a second");
		Check(HarbourHead < 9.0, Name + ": rounding up off the quay it swings under 9 degrees a second");
		Check(UpHarbour < Cruise * HarbourPace * 1.05, Name + ": in the harbour she keeps her harbour speed or less");
		// (her pace is the same thing as her speed, so that the wake that is shown by it is honest)
		double WorstPace = 0.0;
		for (double Since = 1.0; Since < Roads.Sail - 1.0; Since += 0.5)
		{
			const double Speed = (Run(Roads, Since + Tick) - Run(Roads, Since - Tick)) / (2.0 * Tick);
			WorstPace = std::fmax(WorstPace, std::fabs(Speed / Cruise - Pace(Roads, Run(Roads, Since))));
		}
		Check(WorstPace < 0.06, Name + ": the pace she is said to make is the speed she makes");
	}

	// ---- 3. By the clock: no jump, lying or sailing; her share of the lap lying; two ships of a port never meet.
	{
		const FRoads Roads = Lay(FSpot{ 40.0, 40.0 }, 60.0, 24.0, 11);
		const double Lap = Roads.Sail + Roads.Lie, Tick = 1.0 / 60.0;
		double WorstJump = 0.0, WorstTurn = 0.0, Lying = 0.0, Closest = 1.0e9;
		FAfloat Before = Where(Roads, 0.0, 0, 2);
		int Ticks = 0;
		for (double Clock = Tick; Clock < 2.0 * Lap; Clock += Tick, ++Ticks)
		{
			const FAfloat Her = Where(Roads, Clock, 0, 2), Other = Where(Roads, Clock, 1, 2);
			WorstJump = std::fmax(WorstJump, Far(Her.At, Before.At));
			WorstTurn = std::fmax(WorstTurn, Swing(Her.Ahead, Before.Ahead));
			Lying += Her.bLying ? 1.0 : 0.0;
			Closest = std::fmin(Closest, Far(Her.At, Other.At));
			Check(!Her.bLying || (Her.Pace == 0.0 && Her.Along == 0.0), "by the clock: lying, she makes no way");
			Before = Her;
		}
		Checks -= Ticks - 1; // (that one counted once, not seven thousand times)
		Check(WorstJump < Cruise * Tick * 1.05, "by the clock: she never moves further in a frame than her best speed allows, through two laps");
		Check(WorstTurn < 17.0 * Tick * 1.05, "by the clock: her head never jumps, through two laps");
		Check(std::fabs(Lying / Ticks - Roads.Lie / Lap) < 0.01, "by the clock: she lies for her share of the lap");
		Check(Closest > 2.0 * Half * 0.95, "by the clock: two ships of one port are never nearer than the fairway is broad");
		const FAfloat At0 = Where(Roads, 5.0, 0, 1), AtLap = Where(Roads, 5.0 + Lap, 0, 1);
		Check(Far(At0.At, AtLap.At) < 1.0e-6, "by the clock: a lap later she is where she was");
		const FRoads Elsewhere = Lay(FSpot{ 40.0, 40.0 }, 60.0, 24.0, 12);
		Check(std::fabs(Elsewhere.Start - Roads.Start) > 1.0 || std::fabs(Elsewhere.Lie - Roads.Lie) > 1.0, "by the clock: two ports do not keep step");
	}

	// ---- 4. Finding water, on coasts made by chance.
	{
		FDice Dice;
		int Found = 0, Longest = 0, Swung = 0, Shut = 0;
		bool bAllWet = true, bLongest = true, bShutRight = true;
		for (int Coast = 0; Coast < 2000; ++Coast)
		{
			FCoast Sea;
			Sea.ShoreY = 0.0;
			const int Isles = static_cast<int>(Dice.Between(0.0, 5.0));
			for (int K = 0; K < Isles; ++K)
			{
				Sea.Isles.push_back({ FSpot{ Dice.Between(-30.0, 30.0), Dice.Between(-40.0, 2.0) }, Dice.Between(1.5, 9.0) });
			}
			const FSpot Quay{ 0.0, -0.3 };
			auto Wet = [&Sea](double X, double Y) { return Sea.Wet(X, Y); };
			const FRoads Roads = Find(Quay, 0.0, Coast, Wet);
			if (Roads.bOpen)
			{
				++Found;
				Longest += Roads.Far == Reaches[0] ? 1 : 0;
				Swung += Roads.Bearing != 0.0 ? 1 : 0;
				// (every point of it, looked for more finely than Find looks, has water; at Clear less a little on either side too)
				for (int K = 0; K < 2000 && bAllWet; ++K)
				{
					FSpot At, Ahead;
					On(Roads, Roads.Length * K / 2000, At, Ahead);
					const FSpot Beam{ -Ahead.Y, Ahead.X };
					bAllWet = Sea.Wet(At.X, At.Y) && Sea.Wet(At.X + Beam.X * Clear * 0.7, At.Y + Beam.Y * Clear * 0.7) && Sea.Wet(At.X - Beam.X * Clear * 0.7, At.Y - Beam.Y * Clear * 0.7);
				}
				// (and no longer one would have floated, on any swing)
				for (const double Reach : Reaches)
				{
					for (const double Swing_ : Swings)
					{
						if (Reach > Roads.Far && Afloat(Lay(Quay, Swing_, Reach, Coast), Wet))
						{
							bLongest = false;
						}
					}
				}
			}
			else
			{
				++Shut;
				for (const double Reach : Reaches)
				{
					for (const double Swing_ : Swings)
					{
						bShutRight = bShutRight && !Afloat(Lay(Quay, Swing_, Reach, Coast), Wet);
					}
				}
			}
		}
		std::printf("Coasts: 2000 made by chance; %d ports have their roads (%d the longest, %d swung off the straight way out), %d have no water for one\n", Found, Longest, Swung, Shut);
		Check(bAllWet, "finding water: every point of every fairway found is afloat with room on either side");
		Check(bLongest, "finding water: the fairway found is the longest that floats");
		Check(bShutRight, "finding water: a port is left without only where none floats");
		Check(Found > 800 && Shut > 100, "finding water: the coasts made by chance try both cases well");
		// (an open coast gives the longest, straight out)
		FCoast Open;
		const FRoads Plain = Find(FSpot{ 0.0, -0.3 }, 0.0, 1, [&Open](double X, double Y) { return Open.Wet(X, Y); });
		Check(Plain.bOpen && Plain.Far == Reaches[0] && Plain.Bearing == 0.0, "finding water: an open coast gives the longest fairway, straight out");
		// (a quay at the head of a creek two units wide has none)
		FCoast Creek;
		Creek.Isles.push_back({ FSpot{ -7.0, -6.0 }, 6.0 });
		Creek.Isles.push_back({ FSpot{ 7.0, -6.0 }, 6.0 });
		const FRoads None = Find(FSpot{ 0.0, -0.3 }, 0.0, 1, [&Creek](double X, double Y) { return Creek.Wet(X, Y); });
		Check(!None.bOpen, "finding water: a quay at the head of a narrow creek has no roads");
		Check(!Lay(FSpot{ 0.0, 0.0 }, 0.0, NearOff + 1.0, 1).bOpen, "a fairway too short to turn in is not open");
	}

	// ---- 5. Her wake.
	{
		const FRoads Roads = Lay(FSpot{ 0.0, 0.0 }, 120.0, 30.0, 5);
		FAfloat Her;
		Her.bLying = false;
		Her.Along = 0.6 * Roads.Length;
		Her.Pace = Pace(Roads, Her.Along);
		On(Roads, Her.Along, Her.At, Her.Ahead);
		bool bOnCourse = true;
		for (double Back = -1.5; Back < 8.0; Back += 0.25)
		{
			FSpot At, Ahead, Want, WantAhead;
			const double Made = Astern(Roads, Her, Back, At, Ahead);
			On(Roads, Her.Along - Back, Want, WantAhead);
			bOnCourse = bOnCourse && Far(At, Want) < 1.0e-9 && Made == 1.0;
		}
		Check(bOnCourse, "her wake: every point of it lies on the water she has crossed");
		// (just off her berth: wake only as far astern as she has come)
		Her.Along = 2.0;
		On(Roads, Her.Along, Her.At, Her.Ahead);
		FSpot At, Ahead;
		Check(Astern(Roads, Her, 0.5, At, Ahead) == 1.0 && Astern(Roads, Her, 1.5, At, Ahead) == 0.5 && Astern(Roads, Her, 2.0, At, Ahead) == 0.0 && Astern(Roads, Her, 5.0, At, Ahead) == 0.0,
			"her wake: off her berth it reaches only as far astern as she has come");
		FSpot Berth, BerthAhead;
		On(Roads, 0.0, Berth, BerthAhead);
		Astern(Roads, Her, 5.0, At, Ahead);
		Check(std::fabs(Far(At, Berth) - 3.0) < 1.0e-9 && Swing(Ahead, BerthAhead) < 1.0e-6, "her wake: astern of her berth its points lie straight back, with nothing on them");
	}

	// ---- 6. Two ports' rounds.
	{
		const FRoads One = Lay(FSpot{ 0.0, 0.0 }, 0.0, 24.0, 1), Beside = Lay(FSpot{ 4.0, 0.0 }, 0.0, 24.0, 2), Along = Lay(FSpot{ 16.0, 0.0 }, 0.0, 24.0, 3), FarOff = Lay(FSpot{ 200.0, 0.0 }, 0.0, 24.0, 4);
		Check(Foul(One, Beside, 5.0) && Foul(Beside, One, 5.0), "two ports four units apart foul one another");
		Check(!Foul(One, Along, 5.0), "two ports sixteen units apart on one shore do not");
		Check(!Foul(One, FarOff, 5.0), "two ports far apart do not");
		const FRoads Across = Lay(FSpot{ 0.0, -40.0 }, 180.0, 24.0, 5); // (a port on the far shore of a strait, its ships coming this way)
		Check(Foul(One, Across, 5.0), "two ports facing each other over a narrow strait foul one another");
		Check(Off(One, FSpot{ 0.0, -12.0 }) < Half + 0.6 && Off(One, FSpot{ 40.0, -12.0 }) > 30.0, "how far a point lies off a round is told rightly");
	}

	// ---- The scene, for the picture.
	if (ArgC > 1)
	{
		FCoast Sea;
		Sea.ShoreY = 0.0;
		Sea.Isles.push_back({ FSpot{ -13.0, -1.0 }, 6.0 });
		Sea.Isles.push_back({ FSpot{ 17.0, -15.0 }, 4.5 });
		Sea.Isles.push_back({ FSpot{ 4.0, 6.0 }, 9.0 });
		const FSpot Quay{ 4.0, -3.2 };
		const FRoads Roads = Find(Quay, 0.0, 4, [&Sea](double X, double Y) { return Sea.Wet(X, Y); });
		std::ofstream Out(ArgV[1]);
		Out << "{\n \"shoreY\": " << Sea.ShoreY << ",\n \"isles\": [";
		for (size_t K = 0; K < Sea.Isles.size(); ++K)
		{
			Out << (K ? ", " : "") << "[" << Sea.Isles[K].Mid.X << ", " << Sea.Isles[K].Mid.Y << ", " << Sea.Isles[K].Radius << "]";
		}
		Out << "],\n \"quay\": [" << Quay.X << ", " << Quay.Y << "],\n \"open\": " << (Roads.bOpen ? "true" : "false") << ", \"bearing\": " << Roads.Bearing << ", \"far\": " << Roads.Far
			<< ", \"length\": " << Roads.Length << ", \"sail\": " << Roads.Sail << ", \"lie\": " << Roads.Lie << ", \"half\": " << Half << ", \"nearOff\": " << NearOff << ", \"cruise\": " << Cruise << ",\n \"round\": [";
		for (int K = 0; K <= 400; ++K)
		{
			FSpot At, Ahead;
			On(Roads, Roads.Length * K / 400, At, Ahead);
			Out << (K ? ", " : "") << "[" << At.X << ", " << At.Y << ", " << Pace(Roads, Roads.Length * K / 400) << "]";
		}
		Out << "],\n \"ships\": [";
		const double Lap = Roads.Sail + Roads.Lie;
		bool bFirst = true;
		for (double Clock = -Roads.Start; Clock < Lap - Roads.Start; Clock += 3.0)
		{
			for (int Ship = 0; Ship < 2; ++Ship)
			{
				const FAfloat Her = Where(Roads, Clock, Ship, 2);
				Out << (bFirst ? "" : ", ") << "[" << Clock + Roads.Start << ", " << Ship << ", " << Her.At.X << ", " << Her.At.Y << ", " << Her.Ahead.X << ", " << Her.Ahead.Y << ", " << Her.Pace << ", " << (Her.bLying ? 1 : 0) << "]";
				bFirst = false;
			}
		}
		Out << "]\n}\n";
		std::printf("Scene written to %s\n", ArgV[1]);
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
