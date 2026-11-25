// THE WHITECAPS' GATE BY WIND (the release 2 ocean plan, step 2): writes Battle/FleetWavesFold.inl. For each wind from
// 0.5 to 24.4 m/s (0.5 m/s steps, and the gale's cap), the reference sea (seed 239, raised to the gale's cap) is sampled
// over a 2 km square at 32 instants, a deterministic Monte Carlo on the waves' own stream; FoldSoft is a quarter of J's
// spread there, and Fold is solved (bisection) so the drawn cover, the heads and the lagged tails as step 7 draws them
// (FleetWaves::CoverOf: each head the gate times the whitecaps' picture under it, SeaFace::MakeFoam(512, 160) read in the
// crests' moving frame by FleetWaves::HeadShapeAt), is Monahan's 3.84e-6 U^3.41. Below 3 m/s (Beaufort 2 and under)
// nothing breaks: Fold -1. FarHead: the head the chip draws where the trains have faded under the pixel (their gate would
// alias there), Monahan's cover over the picture's mean under the same samples, so the cover holds at every depth (at most 1).
// Plain C++20, no engine. Usage: fold_table [out]   (no out: standard output)
#include "Battle/FleetWaves.h"
#include "Battle/SeaFace.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
	using namespace FleetWaves;
	constexpr uint64_t RefSeed = 239;
	constexpr int Points = 16384, Instants = 32;
	constexpr double Square = 2000.0;
	std::string Text;
	Text += "// THE WHITECAPS' GATE BY WIND (the release 2 ocean plan, step 2): written by Tools/sea_waves/fold_table.cpp; do not edit.\n";
	Text += "// Each row: the wind (m/s), Fold and FoldSoft (on J), solved so the drawn cover (heads, the gate times the whitecaps' picture, and lagged\n";
	Text += "// tails) is Monahan's; and FarHead, the head drawn where the trains have faded under the pixel (Monahan's over the picture's mean).\n";
	std::vector<double> Winds;
	for (int I = 1; I <= 48; ++I) Winds.push_back(0.5 * I);
	Winds.push_back(GaleCap);
	std::vector<float> Js, Lags, Shapes;
	const int CapsSize = 512;
	const std::vector<uint8_t> Caps = SeaFace::MakeFoam(CapsSize, 160.0);
	for (const double U : Winds)
	{
		double Fold = -1.0, Soft = 1.0, FarHead = 0.0;
		if (U >= 3.0)
		{
			Asked A;
			A.WindU = U;
			A.DownwindR = 0.7;
			A.Seed = RefSeed;
			A.Cap = GaleCap;
			const Sea S = Make(A);
			Js.clear();
			Lags.clear();
			Shapes.clear();
			double Sum = 0.0, Sum2 = 0.0;
			for (int M = 0; M < Instants; ++M)
			{
				const double T = 11.0 + 37.25 * M;
				for (int P = 0; P < Points; ++P)
				{
					const uint64_t Key = 0x5EA0000ULL + static_cast<uint64_t>(M) * Points + P;
					const double X = (Mix(Key * 2) - 0.5) * Square, Y = (Mix(Key * 2 + 1) - 0.5) * Square;
					const Foam F = FoamAt(S, X, Y, T, 2.0, 1.0);
					Js.push_back(static_cast<float>(F.J));
					Shapes.push_back(static_cast<float>(HeadShapeAt(S, Caps, CapsSize, X, Y, T)));
					for (double L : F.Lag) Lags.push_back(static_cast<float>(L));
					Sum += F.J;
					Sum2 += F.J * F.J;
				}
			}
			const double Count = static_cast<double>(Js.size());
			const double Spread = std::sqrt(std::max(Sum2 / Count - (Sum / Count) * (Sum / Count), 0.0));
			Soft = std::max(0.25 * Spread, 0.005);
			const double Want = MonahanOf(S.WindU);
			auto Cover = [&](double F)
			{
				double C = 0.0;
				for (size_t K = 0; K < Js.size(); ++K)
				{
					const double Lag[4] = { Lags[4 * K], Lags[4 * K + 1], Lags[4 * K + 2], Lags[4 * K + 3] };
					C += CoverOf(Js[K], Lag, F, Soft, S.TailTau, Shapes[K]);
				}
				return C / Count;
			};
			double Lo = -1.0, Hi = 2.0;
			for (int It = 0; It < 60; ++It)
			{
				const double Mid = 0.5 * (Lo + Hi);
				(Cover(Mid) < Want ? Lo : Hi) = Mid;
			}
			Fold = 0.5 * (Lo + Hi);
			double Mean = 0.0;
			for (const float Sh : Shapes) Mean += Sh;
			Mean /= Count;
			FarHead = std::min(Want / std::max(Mean, 1e-6), 1.0);
		}
		char Row[96];
		std::snprintf(Row, sizeof(Row), "{ %.1f, %.5f, %.5f, %.6f },\n", U, Fold, Soft, FarHead);
		Text += Row;
	}
	if (argc > 1)
	{
		FILE* F = std::fopen(argv[1], "wb");
		if (!F)
		{
			std::fprintf(stderr, "fold_table: cannot write %s\n", argv[1]);
			return 1;
		}
		std::fwrite(Text.data(), 1, Text.size(), F);
		std::fclose(F);
	}
	else
	{
		std::fwrite(Text.data(), 1, Text.size(), stdout);
	}
	return 0;
}
