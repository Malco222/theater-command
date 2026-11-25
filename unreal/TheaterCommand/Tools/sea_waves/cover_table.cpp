// Theater Command: how much of the sky a sea battle's clouds paint, seen from the sea (the release 2 sea weather).
//
// Two tables, each the share of the whole dome the painter covers (the mean alpha, each pixel the same piece of sky, as
// a sky's eighths are counted), from a ring of eyes 28 m over the sea, 5 to 8.6 km round the board's middle, averaged
// over several seeds. Seeds change the clouds a cover lays by a few hundredths; eyes change what one picture sees, so
// both are averaged.
// - cumulus (SeaSky::CumulusPainted): for cumulus laid at 0, 0.05, ... AirSky::CumulusMost. The sea lays a cumulus day
//   at the cover that paints what the day says (SeaSky::CumulusLaidFor).
// - strato (SeaSky::StratoPainted): for the broken deck of stratocumulus a cumulus day of SeaSky::StratoFrom or more is
//   drawn as (laid at AirSky::DeckLeast, its base 800 m, its top SeaSky::StratoDepth over it), its gaps opened at
//   each of SeaSky::StratoHoles (AirSky::Placing::DeckHoles: the noise under which a tile is left out). The sea opens
//   the gaps that paint what the day says (SeaSky::StratoHolesFor). "-base=600" measures it on another base.
//
// Not part of a suite (a few minutes on 4 cores). Build and run from the project (TheaterCommand), on one line:
//   S=Source/TheaterCommand; g++ -std=c++20 -O2 -I$S -pthread -o /tmp/cover_table Tools/sea_waves/cover_table.cpp
//     $S/Battle/SeaSky.cpp $S/Battle/AirSky.cpp $S/Battle/AirFeed.cpp $S/Battle/AirShow.cpp $S/Battle/AirDesk.cpp
//     $S/Battle/AirGround.cpp $S/Battle/ShipGuns.cpp $S/Effects/TheaterFxCore.cpp
//   /tmp/cover_table [cumulus|strato] [-base=800] [eyes] [seed ...]   (default: cumulus, 20 eyes; seeds 1942 241 77 5
//     11 19 23 31 47)
// It prints each step's mean, the least and most seed, the overhead caps beside it, and the table as SeaSky.h holds it.
// A painter or a field change that moves a table re-measures it here, and SeaSky.h takes the new line.

#include "Battle/AirSky.h"
#include "Battle/SeaSky.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <thread>
#include <vector>

namespace
{
	using AirSky::Vec;
	constexpr double Pi = 3.14159265358979323846;
	constexpr double MiddleX = 10000.0, MiddleY = 10000.0;
	constexpr int CumulusSteps = 12; // 0, 0.05, ... 0.55
	constexpr int StratoSteps = static_cast<int>(std::size(SeaSky::StratoHoles));

	struct Seen
	{
		double Whole = 0.0, Caps = 0.0;
	};

	struct Way
	{
		bool bStrato = false;
		double Base = 800.0;
	};

	/** The step's own number: a cumulus cover laid, or a strato deck's gap threshold. */
	double StepOf(const Way& How, int Step)
	{
		return How.bStrato ? SeaSky::StratoHoles[Step] : Step * 0.05;
	}

	Seen Measure(const Way& How, int Step, uint64_t Seed, int Eyes, const std::vector<uint8_t>& Sheet)
	{
		const Vec Sun = Vec(-0.35, -0.45, 0.82).Unit();
		const Vec Wind = Vec(13.5 * std::sin(420.0 * Pi / 180.0), 13.5 * std::cos(420.0 * Pi / 180.0), 0.0);
		AirSky::Day Day;
		Day.bGiven = true;
		AirSky::Placing P = SeaSky::PlacingFor(Seed, MiddleX, MiddleY, Sun, Wind);
		if (How.bStrato)
		{
			// (the deck as the sea lays a cumulus day of StratoFrom or more: SeaSky::Choose)
			Day.bDeck = true;
			Day.Base = How.Base;
			Day.Top = How.Base + SeaSky::StratoDepth;
			Day.Cover = AirSky::DeckLeast;
			P.DeckHoles = StepOf(How, Step);
		}
		else
		{
			// (laid as AirSky lays it for the sea, the cover as it stands: the table is of the cover laid, not of a day's)
			Day.bDeck = false;
			Day.Base = 600.0;
			Day.Top = 2600.0;
			Day.Cover = StepOf(How, Step);
		}
		const AirSky::Field F = AirSky::Lay(Day, P);
		Seen Out;
		if (F.Clouds.empty())
		{
			return Out;
		}
		for (int K = 0; K < Eyes; ++K)
		{
			const double Round = 2.0 * Pi * (K % 10) / 10.0 + 0.31 * (K / 10);
			const double Away = 5000.0 + 3600.0 * (K / 10) / std::max(1, (Eyes - 1) / 10);
			AirSky::Painter Paint;
			SeaSky::DomeHow Dome;
			Dome.W = SeaSky::DomeW / 4;
			Dome.H = SeaSky::DomeH / 4;
			Dome.Sheet = &Sheet;
			SeaSky::Dome D;
			SeaSky::PaintDome(F, Vec(MiddleX + Away * std::cos(Round), MiddleY + Away * std::sin(Round), 28.0), Vec(), Sun,
				AirSky::Rgb(1.0f, 1.0f, 1.0f), Paint, D, Dome);
			Out.Whole += D.Cover();
			// (the caps over 60 degrees, as sea_sky_test's sweep measures them)
			const int Rows = std::max(1, static_cast<int>(std::ceil((1.0 - std::sin(60.0 * Pi / 180.0)) * D.H)));
			double Alpha = 0.0;
			for (int Y = 0; Y < Rows; ++Y)
			{
				for (int X = 0; X < D.W; ++X)
				{
					Alpha += D.Rgba[(static_cast<size_t>(Y) * D.W + X) * 4 + 3] / 255.0;
				}
			}
			Out.Caps += Alpha / (static_cast<double>(Rows) * D.W);
		}
		Out.Whole /= Eyes;
		Out.Caps /= Eyes;
		return Out;
	}
}

int main(int argc, char** argv)
{
	Way How;
	int A = 1;
	if (A < argc && (std::strcmp(argv[A], "strato") == 0 || std::strcmp(argv[A], "cumulus") == 0))
	{
		How.bStrato = std::strcmp(argv[A], "strato") == 0;
		++A;
	}
	if (A < argc && std::strncmp(argv[A], "-base=", 6) == 0)
	{
		How.Base = std::atof(argv[A] + 6);
		++A;
	}
	const int Eyes = A < argc ? std::max(1, std::atoi(argv[A++])) : 20;
	std::vector<uint64_t> Seeds;
	for (; A < argc; ++A)
	{
		Seeds.push_back(static_cast<uint64_t>(std::strtoull(argv[A], nullptr, 10)));
	}
	if (Seeds.empty())
	{
		Seeds = { 1942, 241, 77, 5, 11, 19, 23, 31, 47 };
	}
	const int Steps = How.bStrato ? StratoSteps : CumulusSteps;
	std::vector<uint8_t> Sheet;
	AirSky::MakeFragments(Sheet);
	std::vector<std::vector<Seen>> Got(Seeds.size(), std::vector<Seen>(static_cast<size_t>(Steps)));
	{
		std::vector<std::thread> Each;
		for (size_t I = 0; I < Seeds.size(); ++I)
		{
			Each.emplace_back([&, I]() {
				for (int Step = 0; Step < Steps; ++Step)
				{
					Got[I][static_cast<size_t>(Step)] = Measure(How, Step, Seeds[I], Eyes, Sheet);
				}
			});
		}
		for (std::thread& T : Each)
		{
			T.join();
		}
	}
	if (How.bStrato)
	{
		std::printf("broken deck of stratocumulus, base %.0f m, top %.0f m\n", How.Base, How.Base + SeaSky::StratoDepth);
	}
	std::printf("%s  whole  (least - most seed)  caps over 60 deg\n", How.bStrato ? "holes" : "laid ");
	std::vector<double> Mean(static_cast<size_t>(Steps), 0.0);
	for (int Step = 0; Step < Steps; ++Step)
	{
		double Sum = 0.0, Caps = 0.0, Least = 1.0, Most = 0.0;
		for (size_t I = 0; I < Seeds.size(); ++I)
		{
			const Seen& One = Got[I][static_cast<size_t>(Step)];
			Sum += One.Whole;
			Caps += One.Caps;
			Least = std::min(Least, One.Whole);
			Most = std::max(Most, One.Whole);
		}
		Mean[static_cast<size_t>(Step)] = Sum / Seeds.size();
		std::printf("%.2f   %.3f  (%.3f - %.3f)        %.3f\n", StepOf(How, Step), Mean[static_cast<size_t>(Step)], Least, Most, Caps / Seeds.size());
	}
	std::printf("constexpr double %s[%d] = { ", How.bStrato ? "StratoPainted" : "CumulusPainted", Steps);
	for (int Step = 0; Step < Steps; ++Step)
	{
		std::printf("%.3f%s", Mean[static_cast<size_t>(Step)], Step + 1 < Steps ? ", " : " };\n");
	}
	return 0;
}
