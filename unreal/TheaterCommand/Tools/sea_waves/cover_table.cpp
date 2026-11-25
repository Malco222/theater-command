// Theater Command: how much of the sky a cumulus day paints, seen from the sea (the release 2 sea weather).
//
// SeaSky::CumulusPainted is this tool's table: for cumulus laid at 0, 0.05, ... AirSky::CumulusMost, the share of the
// whole dome the painter covers (the mean alpha, each pixel the same piece of sky, as a sky's eighths are counted),
// from a ring of eyes 28 m over the sea, 5 to 8.6 km round the board's middle, averaged over several seeds. The sea
// lays a cumulus day at the cover that paints what the day says (SeaSky::CumulusLaidFor). Seeds change the clouds a
// cover lays by a few hundredths; eyes change what one picture sees, so both are averaged.
//
// Not part of a suite (a few minutes on 4 cores). Build and run from the project (TheaterCommand), on one line:
//   S=Source/TheaterCommand; g++ -std=c++20 -O2 -I$S -pthread -o /tmp/cover_table Tools/sea_waves/cover_table.cpp
//     $S/Battle/SeaSky.cpp $S/Battle/AirSky.cpp $S/Battle/AirFeed.cpp $S/Battle/AirShow.cpp $S/Battle/AirDesk.cpp
//     $S/Battle/AirGround.cpp $S/Battle/ShipGuns.cpp $S/Effects/TheaterFxCore.cpp
//   /tmp/cover_table [eyes] [seed ...]      (default: 20 eyes; seeds 1942 241 77 5 11 19 23 31 47)
// It prints each cover's mean, the least and most seed, the overhead caps beside it, and the table as SeaSky.h holds it.
// A painter or a field change that moves the table re-measures it here, and SeaSky.h takes the new line.

#include "Battle/AirSky.h"
#include "Battle/SeaSky.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace
{
	using AirSky::Vec;
	constexpr double Pi = 3.14159265358979323846;
	constexpr double MiddleX = 10000.0, MiddleY = 10000.0;
	constexpr int Steps = 12; // 0, 0.05, ... 0.55

	struct Seen
	{
		double Whole = 0.0, Caps = 0.0;
	};

	Seen Measure(double Cover, uint64_t Seed, int Eyes, const std::vector<uint8_t>& Sheet)
	{
		const Vec Sun = Vec(-0.35, -0.45, 0.82).Unit();
		const Vec Wind = Vec(13.5 * std::sin(420.0 * Pi / 180.0), 13.5 * std::cos(420.0 * Pi / 180.0), 0.0);
		AirSky::Day Day;
		Day.bGiven = true;
		Day.bDeck = false;
		Day.Base = 600.0;
		Day.Top = 2600.0;
		Day.Cover = Cover;
		// (laid as AirSky lays it for the sea, the cover as it stands: the table is of the cover laid, not of a day's)
		const AirSky::Field F = AirSky::Lay(Day, SeaSky::PlacingFor(Seed, MiddleX, MiddleY, Sun, Wind));
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
			SeaSky::DomeHow How;
			How.W = SeaSky::DomeW / 4;
			How.H = SeaSky::DomeH / 4;
			How.Sheet = &Sheet;
			SeaSky::Dome D;
			SeaSky::PaintDome(F, Vec(MiddleX + Away * std::cos(Round), MiddleY + Away * std::sin(Round), 28.0), Vec(), Sun,
				AirSky::Rgb(1.0f, 1.0f, 1.0f), Paint, D, How);
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
	const int Eyes = argc > 1 ? std::max(1, std::atoi(argv[1])) : 20;
	std::vector<uint64_t> Seeds;
	for (int A = 2; A < argc; ++A)
	{
		Seeds.push_back(static_cast<uint64_t>(std::strtoull(argv[A], nullptr, 10)));
	}
	if (Seeds.empty())
	{
		Seeds = { 1942, 241, 77, 5, 11, 19, 23, 31, 47 };
	}
	std::vector<uint8_t> Sheet;
	AirSky::MakeFragments(Sheet);
	std::vector<std::vector<Seen>> Got(Seeds.size(), std::vector<Seen>(Steps));
	{
		std::vector<std::thread> Each;
		for (size_t I = 0; I < Seeds.size(); ++I)
		{
			Each.emplace_back([&, I]() {
				for (int Step = 0; Step < Steps; ++Step)
				{
					Got[I][Step] = Measure(Step * 0.05, Seeds[I], Eyes, Sheet);
				}
			});
		}
		for (std::thread& T : Each)
		{
			T.join();
		}
	}
	std::printf("laid   whole  (least - most seed)  caps over 60 deg\n");
	std::vector<double> Mean(Steps, 0.0);
	for (int Step = 0; Step < Steps; ++Step)
	{
		double Sum = 0.0, Caps = 0.0, Least = 1.0, Most = 0.0;
		for (size_t I = 0; I < Seeds.size(); ++I)
		{
			Sum += Got[I][Step].Whole;
			Caps += Got[I][Step].Caps;
			Least = std::min(Least, Got[I][Step].Whole);
			Most = std::max(Most, Got[I][Step].Whole);
		}
		Mean[Step] = Sum / Seeds.size();
		std::printf("%.2f   %.3f  (%.3f - %.3f)        %.3f\n", Step * 0.05, Mean[Step], Least, Most, Caps / Seeds.size());
	}
	std::printf("constexpr double CumulusPainted[%d] = { ", Steps);
	for (int Step = 0; Step < Steps; ++Step)
	{
		std::printf("%.3f%s", Mean[Step], Step + 1 < Steps ? ", " : " };\n");
	}
	return 0;
}
