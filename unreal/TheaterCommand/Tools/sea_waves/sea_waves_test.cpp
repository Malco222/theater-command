// THE WAVE CORE'S CLOUD TEST (the release 2 ocean plan, step 2; section 6's sea_waves_test). Battle/FleetWaves.{h,cpp}
// and the chip's text, Battle/FleetWavesGpu.inl, compiled in float through the core's own stand-ins, held to the plan:
// the spectrum and its pairs, the seeds, no folding, the whitecaps against Monahan, the slopes against Cox-Munk and the
// road's roughness against a dense BRDF average, no pops across any wrap or snap, the weights, the clipmap watertight,
// the patch (its snaps, and far subjects and long hulls whole under it), the chord bound, the references (HeightAt,
// RideCoef), SeaNear (out to the farthest weight, and its cost at the shots' eyes), the chip against the core, and the
// ride's cost on the recorded actions (played through the show itself). Nothing here is wired into the game: step 2 is cloud
// only. Plain C++20, no engine. Usage: sea_waves_test <Source/TheaterCommand> <out dir> [recording.json ...]
#include "Battle/FleetWaves.h"
#include "Battle/SeaFace.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <time.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	using namespace FleetWaves;
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Detail = "")
	{
		++Checks;
		if (!bOk) ++Failed;
		std::printf("%s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Detail.empty() ? "" : ": ", Detail.c_str());
	}

	void Note(const std::string& What) { std::printf("     %s\n", What.c_str()); }

	std::string Num(double X, int Places = 3)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string Sci(double X)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.3g", X);
		return Text;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	double Sq(double X) { return X * X; }

	using Clock = std::chrono::steady_clock;
	double Since(Clock::time_point T0) { return std::chrono::duration<double>(Clock::now() - T0).count(); }
	/** This thread's own time (s): what a cost is, without the turns other work on the shared cores takes from it. */
	double CpuNow()
	{
		timespec T{};
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &T);
		return static_cast<double>(T.tv_sec) + 1e-9 * static_cast<double>(T.tv_nsec);
	}

	/** A cost: the least of Rounds rounds (each Round() gives its own cost), and while that is over Most, of more rounds for
	 * up to Seconds more on the wall clock. The cloud's four cores are shared, and a neighbour's busy core slows every round
	 * of a stretch at once (by up to 1.7 times, measured), which the thread's own time does not take out. */
	template <class F>
	double LeastOf(int Rounds, double Most, double Seconds, F&& Round)
	{
		double Best = 1e30;
		for (int R = 0; R < Rounds; ++R) Best = std::min(Best, Round());
		const Clock::time_point T0 = Clock::now();
		while (Best > Most && Since(T0) < Seconds) Best = std::min(Best, Round());
		return Best;
	}

	Sea SeaOf(double U, uint64_t Seed, bool bGale = false, double SwellH = 0.0, double SwellT = 0.0, double Down = 0.6)
	{
		Asked A;
		A.WindU = U;
		A.DownwindR = Down;
		A.Seed = Seed;
		A.Cap = bGale ? GaleCap : Raised;
		A.SwellH = SwellH;
		A.SwellT = SwellT;
		A.SwellFromR = Down + Pi + 0.5;
		return Make(A);
	}

	Drawn FrameOf(const Sea& S, double Ex, double Ey, double Ez, double T, const double* Subject = nullptr, int N = 96, const Drawn* Last = nullptr,
		double Lens = 60.0)
	{
		const double Eye[3] = { Ex, Ey, Ez }, Look[3] = { Ex + 100.0, Ey + 100.0, 0.0 };
		return DrawnFor(S, Eye, Look, Lens, Subject, T, Last, Last == nullptr, RingsFor(S), N);
	}

	/** The test's own Mitsuyasu, for the pairs. */
	double MitsuyasuSpread(double W, double Wp)
	{
		const double R = W / Wp;
		const double Sv = R > 1.0 ? 11.5 * std::pow(R, -2.5) : 11.5 * std::pow(R, 5.0);
		return std::sqrt(2.0 / (Sv + 1.0));
	}

	double Wrap(double A) { return A - TwoPi * std::floor((A + Pi) / TwoPi); }

	// ---------------------------------------------------------------------------------------------------------------
	// The spectrum, the pairs, the seeds.

	void Spectra()
	{
		double WorstHs = 0.0, WorstLaid = 0.0, WorstTp = 0.0, WorstShare = 0.0, WorstDisp = 0.0, WorstBand = 0.0;
		int BandsSeen = 0;
		for (int G = 0; G < 2; ++G)
		{
			const double Cap = G ? GaleCap : Raised;
			for (double U = 0.5; U <= Cap + 1e-9; U = std::min(U + 0.5, U < Cap - 1e-9 ? Cap : Cap + 1.0))
			{
				const Sea S = SeaOf(U, 11 + static_cast<uint64_t>(U * 10), G == 1);
				const double Uw = std::min(U, Cap);
				WorstHs = std::max(WorstHs, std::fabs(S.Hs / (0.021 * Uw * Uw) - 1.0));
				// (the laid spectrum, integrated: its height and its peak)
				double M0 = 0.0, Peak = 0.0, PeakW = 0.0;
				const int Steps = 20000;
				const double W0 = 0.05 * S.Wp, W1 = 30.0 * S.Wp;
				for (int I = 0; I < Steps; ++I)
				{
					const double Wa = W0 * std::pow(W1 / W0, static_cast<double>(I) / Steps), Wb = W0 * std::pow(W1 / W0, static_cast<double>(I + 1) / Steps);
					const double Wm = std::sqrt(Wa * Wb), Sv = Spectrum(Wm, S.Wp, S.Hs);
					M0 += Sv * (Wb - Wa);
					if (Sv > Peak)
					{
						Peak = Sv;
						PeakW = Wm;
					}
				}
				WorstLaid = std::max(WorstLaid, std::fabs(4.0 * std::sqrt(M0) / (0.021 * Uw * Uw) - 1.0));
				WorstTp = std::max(WorstTp, std::fabs(TwoPi / PeakW / (0.73 * std::max(Uw, 0.5)) - 1.0));
				// (each band's pair against Spectrum() integrated over the band's edges, and the bands' sum against it over
				// [BandEdge[0], BandEdge[Top]]: the trains tied to the laid spectrum, not to the share Make keeps)
				auto Integral = [&](double Wa0, double Wb0)
				{
					double I = 0.0;
					const int Parts = 4000;
					for (int Q = 0; Q < Parts; ++Q)
					{
						const double Wa = Wa0 * std::pow(Wb0 / Wa0, static_cast<double>(Q) / Parts), Wb = Wa0 * std::pow(Wb0 / Wa0, static_cast<double>(Q + 1) / Parts);
						I += Spectrum(std::sqrt(Wa * Wb), S.Wp, S.Hs) * (Wb - Wa);
					}
					return I;
				};
				double Held = 0.0;
				int Top = 0;
				for (int B = 0; B < Bands; ++B)
				{
					double Pair = 0.0;
					int Trains = 0;
					for (int J = 0; J < S.ND; ++J)
					{
						if (S.D[J].Band != B) continue;
						Pair += Sq(S.D[J].A) * 0.5;
						++Trains;
					}
					if (Trains == 0) continue;
					Top = B + 1;
					Held += Pair;
					WorstBand = std::max(WorstBand, std::fabs(Pair / Integral(S.Wp * BandEdge[B], S.Wp * BandEdge[B + 1]) - 1.0));
					++BandsSeen;
				}
				if (Top > 0) WorstShare = std::max(WorstShare, std::fabs(Held / Integral(S.Wp * BandEdge[0], S.Wp * BandEdge[Top]) - 1.0));
				for (int J = 0; J < S.ND + S.NP; ++J)
				{
					const Train& T = J < S.ND ? S.D[J] : S.P[J - S.ND];
					WorstDisp = std::max({ WorstDisp, std::fabs(T.W * T.W - Gee * T.K) / (Gee * T.K), std::fabs(std::hypot(T.Kx, T.Ky) / T.K - 1.0),
						std::fabs(T.L * T.K / TwoPi - 1.0) });
				}
				if (U >= Cap) break;
			}
		}
		Check("spectrum: Hs = 0.021 Uw^2 within 5%, 0.5 to 13.8 m/s and to 24.4 m/s under the gale's cap", WorstHs <= 0.05, "worst " + Num(100 * WorstHs, 2) + "%");
		Check("spectrum: the laid spectrum's own Hs (4 sqrt m0, integrated) within 5%", WorstLaid <= 0.05, "worst " + Num(100 * WorstLaid, 2) + "%");
		Check("spectrum: Tp (the laid spectrum's peak) = 0.73 U within 5%", WorstTp <= 0.05, "worst " + Num(100 * WorstTp, 2) + "%");
		Check("spectrum: each band's pair holds a1^2/2 + a2^2/2 = Spectrum() integrated between Wp BandEdge[B] and Wp BandEdge[B + 1], within 1%",
			BandsSeen > 50 && WorstBand <= 0.01, std::to_string(BandsSeen) + " bands, worst " + Num(100 * WorstBand, 3) + "%");
		Check("spectrum: the displaced wind trains hold Spectrum() integrated over [Wp BandEdge[0], Wp BandEdge[Top]], within 1%", WorstShare <= 0.01,
			"worst " + Num(100 * WorstShare, 3) + "%");
		Check("spectrum: w^2 = g k exactly (and K = |(Kx, Ky)|, L = 2 pi / K)", WorstDisp <= 1e-12, Sci(WorstDisp));
		// JONSWAP's fetch law, where it holds the sea lower than the wind's whole sea
		double WorstFetch = 0.0, WorstFetchTp = 0.0;
		int Cases = 0;
		for (double U : { 6.0, 10.0, 13.8 })
		{
			for (double Fetch : { 3000.0, 10000.0, 30000.0, 80000.0 })
			{
				Asked A;
				A.WindU = U;
				A.Fetch = Fetch;
				A.Seed = 5;
				const Sea S = Make(A);
				const double X = Gee * Fetch / (U * U), Hj = 1.6e-3 * std::sqrt(X) * U * U / Gee, Tj = 1.0 / (3.5 * Gee / U * std::pow(X, -0.33));
				if (Hj >= 0.021 * U * U) continue;
				++Cases;
				WorstFetch = std::max(WorstFetch, std::fabs(S.Hs / Hj - 1.0));
				WorstFetchTp = std::max(WorstFetchTp, std::fabs(S.Tp / Tj - 1.0));
			}
		}
		Check("spectrum: JONSWAP's fetch law within 10% where a fetch holds the sea (Hs and Tp)", Cases >= 6 && WorstFetch <= 0.1 && WorstFetchTp <= 0.1,
			std::to_string(Cases) + " cases, worst Hs " + Num(100 * WorstFetch, 2) + "%, Tp " + Num(100 * WorstFetchTp, 2) + "%");
	}

	void Pairs()
	{
		double WorstRatioLo = 9.0, WorstRatioHi = 0.0, WorstSplitLo = 1.0, WorstSplitHi = 0.0, WorstLean = 0.0, ShortestDisplaced = 1e9;
		int MostD = 0, MostP = 0, Opposite = 0, PairsSeen = 0, SwellOk = 0, SwellSeas = 0, Unpaired = 0;
		for (double U : { 3.0, 5.0, 7.0, 9.0, 11.0, 13.8, 18.0, 24.4 })
		{
			for (uint64_t Seed = 1; Seed <= 24; ++Seed)
			{
				for (int Swell = 0; Swell < 2; ++Swell)
				{
					const Sea S = SeaOf(U, Seed, U > Raised, Swell ? 1.5 : 0.0, Swell ? 11.0 : 0.0, 0.3 * static_cast<double>(Seed));
					MostD = std::max(MostD, S.ND);
					MostP = std::max(MostP, S.NP);
					int Bands5 = 0;
					for (int B = 0; B < Bands; ++B)
					{
						const Train* Two[2] = { nullptr, nullptr };
						int Seen = 0;
						for (int J = 0; J < S.ND; ++J)
						{
							if (S.D[J].Band == B && Seen < 2) Two[Seen++] = &S.D[J];
						}
						if (Seen == 0) continue;
						++Bands5;
						if (Seen != 2)
						{
							++Unpaired;
							continue;
						}
						++PairsSeen;
						const double Theta = MitsuyasuSpread(S.Wp * BandAt[B], S.Wp);
						double Off[2];
						for (int P = 0; P < 2; ++P)
						{
							Off[P] = Wrap(std::atan2(Two[P]->Kx, Two[P]->Ky) - S.Downwind);
							const double Ratio = std::fabs(Off[P]) / Theta;
							WorstRatioLo = std::min(WorstRatioLo, Ratio);
							WorstRatioHi = std::max(WorstRatioHi, Ratio);
						}
						if (Off[0] * Off[1] < 0.0) ++Opposite;
						const double Split = Sq(Two[0]->A) / (Sq(Two[0]->A) + Sq(Two[1]->A));
						WorstSplitLo = std::min(WorstSplitLo, Split);
						WorstSplitHi = std::max(WorstSplitHi, Split);
					}
					if (Swell)
					{
						++SwellSeas;
						int N = 0;
						double H[2] = { 0.0, 0.0 };
						for (int J = 0; J < S.ND; ++J)
						{
							if (S.D[J].Band == -1 && N < 2) H[N++] = std::atan2(S.D[J].Kx, S.D[J].Ky);
						}
						if (N == 2 && std::fabs(std::fabs(Wrap(H[0] - H[1])) - SwellApart) < 1e-9 && Bands5 <= Bands - 1) ++SwellOk;
					}
					double Lean = 0.0;
					for (int J = 0; J < S.ND; ++J)
					{
						ShortestDisplaced = std::min(ShortestDisplaced, S.D[J].L);
						Lean += S.D[J].Q * S.D[J].K * S.D[J].A;
					}
					for (int J = 0; J < S.NP; ++J) Lean += S.P[J].Q * S.P[J].K * S.P[J].A;
					WorstLean = std::max(WorstLean, Lean - S.Qb);
					if (S.Qb > 0.85) WorstLean = std::max(WorstLean, 1.0);
				}
			}
		}
		Check("pairs: each band's two trains at plus and minus theta_b of downwind (Mitsuyasu's s), within the seeded 20% jitter",
			PairsSeen > 500 && Opposite == PairsSeen && Unpaired == 0 && WorstRatioLo >= 0.8 - 1e-9 && WorstRatioHi <= 1.2 + 1e-9,
			std::to_string(PairsSeen) + " pairs, |off| / theta_b " + Num(WorstRatioLo, 3) + " to " + Num(WorstRatioHi, 3));
		Check("pairs: the energy split 0.4 to 0.6", WorstSplitLo >= 0.4 - 1e-12 && WorstSplitHi <= 0.6 + 1e-12, Num(WorstSplitLo, 3) + " to " + Num(WorstSplitHi, 3));
		Check("pairs: at most 10 displaced trains and 16 pixel trains, none displaced shorter than 8 m", MostD <= MostDrawn && MostP <= MostPixel && ShortestDisplaced >= ShortestDrawn,
			"most " + std::to_string(MostD) + " and " + std::to_string(MostP) + ", the shortest displaced " + Num(ShortestDisplaced, 2) + " m");
		Check("pairs: a swell is two trains 8 deg apart, and the wind sea drops its top band for it", SwellOk == SwellSeas, std::to_string(SwellOk) + " of " + std::to_string(SwellSeas));
		Check("pairs: sum(Q k a) over every train at most Qb (and Qb at most 0.85), so J stays above 0.15", WorstLean <= 1e-12, "worst over " + Sci(WorstLean));
	}

	bool SameSea(const Sea& A, const Sea& B)
	{
		auto SameTrain = [](const Train& X, const Train& Y)
		{
			return X.Kx == Y.Kx && X.Ky == Y.Ky && X.K == Y.K && X.W == Y.W && X.A == Y.A && X.Q == Y.Q && X.Phase0 == Y.Phase0 && X.L == Y.L && X.Band == Y.Band;
		};
		if (A.ND != B.ND || A.NP != B.NP || A.Hs != B.Hs || A.Fold != B.Fold || A.DetailMss != B.DetailMss) return false;
		for (int J = 0; J < A.ND; ++J)
		{
			if (!SameTrain(A.D[J], B.D[J])) return false;
		}
		for (int J = 0; J < A.NP; ++J)
		{
			if (!SameTrain(A.P[J], B.P[J])) return false;
		}
		for (int R = 0; R < ReadCount; ++R)
		{
			if (A.Read[R] != B.Read[R]) return false;
		}
		return true;
	}

	void Seeds(const std::string& Src)
	{
		const Sea A1 = SeaOf(8.0, 77), A2 = SeaOf(8.0, 77);
		const Sea Other = SeaOf(12.0, 3, false, 2.0, 10.0);
		const Sea A3 = SeaOf(8.0, 77);
		const Sea B = SeaOf(8.0, 78);
		Check("seeds: one seed and wind give the same trains every time, whatever is made between", SameSea(A1, A2) && SameSea(A1, A3) && !SameSea(A1, Other));
		Check("seeds: another seed gives other trains", !SameSea(A1, B));
		// (the source: no engine, no FMath, no rand, never the show's Dice or Roll)
		const char* Files[] = { "Battle/FleetWaves.h", "Battle/FleetWaves.cpp", "Battle/FleetWavesGpu.inl", "Battle/FleetWavesFold.inl" };
		const std::regex Banned[] = { std::regex("FMath"), std::regex("#include\\s*[\"<](CoreMinimal|Engine|UObject|Kismet|Math/|HAL/|Misc/|Components/|GameFramework/)"),
			std::regex("\\brand\\s*\\("), std::regex("\\bsrand\\b"), std::regex("<random>"), std::regex("\\bDice\\s*\\("), std::regex("\\bRoll\\s*\\("),
			std::regex("\\bUE_LOG\\b"), std::regex("\\bTEXT\\s*\\(") };
		std::string Found;
		size_t Bytes = 0;
		for (const char* F : Files)
		{
			const std::string Text = ReadAll(Src + "/" + F);
			Bytes += Text.size();
			for (const std::regex& R : Banned)
			{
				if (std::regex_search(Text, R)) Found += std::string(F) + " ";
			}
		}
		Check("seeds: a source scan finds no FMath, engine include, rand, Dice or Roll in the core and the chip's text", Found.empty() && Bytes > 20000,
			Found.empty() ? std::to_string(Bytes) + " bytes read" : Found);
		// (HeightAt and HeightDrawn are the tests' reference: nothing in the game calls them)
		std::string Callers;
		int Scanned = 0;
		for (const auto& E : std::filesystem::recursive_directory_iterator(Src))
		{
			if (!E.is_regular_file()) continue;
			const std::string Path = E.path().string(), Ext = E.path().extension().string();
			if (Ext != ".h" && Ext != ".cpp" && Ext != ".inl" && Ext != ".cs") continue;
			if (Path.find("Battle/FleetWaves.") != std::string::npos) continue;
			const std::string Text = ReadAll(Path);
			++Scanned;
			// (the campaign's terrain has a HeightAt of its own: only the wave core's is looked for)
			static const std::regex Ours("FleetWaves::Height(At|Drawn)\\s*\\("), Any("\\bHeight(At|Drawn)\\s*\\(");
			const bool bUses = Text.find("FleetWaves") != std::string::npos;
			if (std::regex_search(Text, Ours) || (bUses && std::regex_search(Text, Any))) Callers += Path + " ";
		}
		Check("seeds: HeightAt and HeightDrawn are called from the tests alone (a source scan of the game)", Scanned > 50 && Callers.empty(),
			std::to_string(Scanned) + " files" + (Callers.empty() ? "" : ": " + Callers));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// No folding, the whitecaps, the slopes, the road.

	void Folding()
	{
		// (the whitecaps' picture step 7 draws the heads with, as fold_table solves Fold with it)
		const int CapsSize = 512;
		const std::vector<uint8_t> Caps = SeaFace::MakeFoam(CapsSize, 160.0);
		double LeastJ = 9.0;
		std::string Each;
		double WorstCover = 0.0;
		std::string Covers;
		for (int B = 2; B <= 9; ++B)
		{
			const double U = 0.836 * std::pow(B, 1.5);
			for (uint64_t Seed : { 4242ULL, 239ULL })
			{
				if (Seed == 239 && (B < 3 || B > 7)) continue;
				const Sea S = SeaOf(U, Seed, true, 0.0, 0.0, 0.9);
				double Least = 9.0, Cover = 0.0;
				long long N = 0;
				const double Step = 2.0;
				for (double T : { 123.4, 731.9 })
				{
					for (int Jy = 0; Jy <= 1000; ++Jy)
					{
						for (int I = 0; I <= 1000; ++I)
						{
							const double X = -1000.0 + I * Step + 0.37, Y = -1000.0 + Jy * Step + 0.71;
							const Foam F = FoamAt(S, X, Y, T);
							Least = std::min(Least, F.J);
							// (the head as step 7 draws it: the gate times the whitecaps' picture under it, read in the crests' frame)
							Cover += std::max(F.Head > 0.0 ? F.Head * HeadShapeAt(S, Caps, CapsSize, X, Y, T) : 0.0, F.Tail);
							++N;
						}
					}
					if (Seed == 239) break;
				}
				Cover /= static_cast<double>(N);
				if (Seed == 4242)
				{
					LeastJ = std::min(LeastJ, Least);
					Each += "B" + std::to_string(B) + " " + Num(Least, 3) + " ";
				}
				if (B >= 3 && B <= 7)
				{
					const double Want = MonahanOf(S.WindU);
					WorstCover = std::max(WorstCover, std::fabs(Cover / Want - 1.0));
					Covers += "B" + std::to_string(B) + (Seed == 239 ? " (table's seed) " : " ") + Num(100 * Cover, 3) + "% against " + Num(100 * Want, 3) + "%; ";
				}
			}
		}
		Check("no folding: the least J over a 2 km square at 2 m, Beaufort 2 to 9, is above 0.15", LeastJ > 0.15, Each);
		Check("whitecaps: the drawn cover (fold-gated heads times the whitecaps' picture, SeaFace::MakeFoam(512, 160), and the lagged tails) within 25% of Monahan, "
			  "Beaufort 3 to 7", WorstCover <= 0.25,
			"worst " + Num(100 * WorstCover, 1) + "%: " + Covers);
	}

	void SlopeBudget()
	{
		// (the remainder as the material forms it: the MPC's Rough.R (Cox-Munk) less the .inl's own Mode-2 w, the slopes the
		// pixel draws; drawn + remainder is then Cox-Munk wherever the drawn slopes are under it, and Rough carries the core's
		// Cox-Munk and capped wind)
		double WorstTotal = 0.0, WorstDrawn = 0.0, WorstRough = 0.0, WorstMpc = 0.0, WorstDrawnChip = 0.0;
		long long Under = 0, Pixels = 0;
		const double Units = 100.0;
		for (double U : { 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 13.8, 18.0, 24.4 })
		{
			const Sea S = SeaOf(U, 9, U > Raised);
			WorstMpc = std::max(WorstMpc, std::fabs(StageParams(S, FrameOf(S, 0.0, 0.0, 25.0, 100.0), Units).Rough.R / S.CoxMunk - 1.0));
			WorstMpc = std::max(WorstMpc, std::fabs(StageParams(S, FrameOf(S, 0.0, 0.0, 25.0, 100.0), Units).Rough.G - std::min(U, U > Raised ? GaleCap : Raised)));
			for (double Lens : { 15.0, 30.0, 60.0, 90.0 })
			{
				const Drawn D = FrameOf(S, 0.0, 0.0, 25.0, 100.0, nullptr, 96, nullptr, Lens);
				const ForStage Fs = StageParams(S, D, Units);
				Gpu::Inputs In = Gpu::From(Fs, false);
				for (double Depth : { 2.0, 5.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0, 6000.0, 10000.0, 20000.0 })
				{
					const double Drawn_ = DrawnMss(S, D, Depth);
					WorstDrawn = std::max(WorstDrawn, Drawn_ / S.CoxMunk);
					for (int K = 0; K < 4; ++K)
					{
						In.Local = Gpu::float2(static_cast<float>(37.0 * K * Units), static_cast<float>(-53.0 * K * Units));
						In.Depth = static_cast<float>(Depth * Units);
						In.Mode = 2.0f;
						const Gpu::float4 O = Gpu::Run(In);
						const float Rem = std::max(Fs.Rough.R - O.w, 0.0f);
						++Pixels;
						WorstDrawnChip = std::max(WorstDrawnChip, static_cast<double>(O.w) / S.CoxMunk);
						if (O.w >= Fs.Rough.R) continue;
						++Under;
						WorstTotal = std::max(WorstTotal, std::fabs((O.w + Rem) / S.CoxMunk - 1.0));
						for (double R0 : { 0.02, 0.1, 0.3 })
							WorstRough = std::max(WorstRough, std::fabs(std::pow(RoughnessFor(R0, Rem), 4.0) - std::pow(R0, 4.0) - Rem) / std::max(static_cast<double>(Rem), 1e-9));
					}
				}
			}
		}
		Check("slope variance: the MPC's Rough carries the core's Cox-Munk and the capped wind (material_check finds it by name)", WorstMpc <= 1e-5, Sci(WorstMpc));
		Check("slope variance: the .inl's drawn slopes (Mode 2's w) + the remainder max(Rough.R - w, 0) are Cox-Munk wherever w is under it, every distance and "
			  "lens; M3's roughness (r0^4 + dmss)^(1/4) carries the remainder",
			Under > 500 && WorstTotal <= 1e-5 && WorstRough < 1e-4,
			std::to_string(Under) + " of " + std::to_string(Pixels) + " pixels under, worst " + Sci(WorstTotal) + "; roughness " + Sci(WorstRough));
		Check("slope variance: what is drawn never exceeds Cox-Munk by more than 25% (the core's DrawnMss and the .inl's w)", WorstDrawn <= 1.25 && WorstDrawnChip <= 1.25,
			"most " + Num(WorstDrawn, 3) + " and " + Num(WorstDrawnChip, 3) + " of Cox-Munk");
	}

	// The road: Unreal's GGX against a dense average of the facets the roughness stands for.
	struct V3
	{
		double X, Y, Z;
	};
	V3 Unit(V3 A)
	{
		const double N = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
		return { A.X / N, A.Y / N, A.Z / N };
	}
	double Dot(V3 A, V3 B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	double Spec(V3 N, V3 L, V3 V, double R)
	{
		const double A = R * R, A2 = A * A, Nl = Dot(N, L), Nv = Dot(N, V);
		if (Nl <= 0.0 || Nv <= 0.0) return 0.0;
		const V3 H = Unit({ L.X + V.X, L.Y + V.Y, L.Z + V.Z });
		const double Nh = Dot(N, H), Dd = Nh * Nh * (A2 - 1.0) + 1.0, Dn = A2 / (Pi * Dd * Dd);
		const double Vis = 0.5 / (Nl * (Nv * (1.0 - A) + A) + Nv * (Nl * (1.0 - A) + A));
		const double F = 0.02 + 0.98 * std::pow(1.0 - Dot(V, H), 5.0);
		return Dn * Vis * F;
	}
	double Truth(V3 L, V3 V, double R0, double Mss)
	{
		const V3 H = Unit({ L.X + V.X, L.Y + V.Y, L.Z + V.Z });
		const double Shx = -H.X / H.Z, Shy = -H.Y / H.Z, S2 = Mss / 2.0, A0 = R0 * R0;
		double Sum = 0.0, Prev = 0.0;
		const int NR = 60, NA = 24;
		const double Rmax = 7.0 * std::sqrt(S2) + std::hypot(Shx, Shy) + 10.0 * A0, Rmin = A0 * 0.02;
		for (int I = 0; I < NR; ++I)
		{
			const double R = Rmin * std::pow(Rmax / Rmin, (I + 0.5) / NR), R1 = Rmin * std::pow(Rmax / Rmin, (I + 1.0) / NR), Dr = R1 - Prev;
			Prev = R1;
			for (int K = 0; K < NA; ++K)
			{
				const double Psi = TwoPi * (K + 0.5) / NA, Sx = Shx + R * std::cos(Psi), Sy = Shy + R * std::sin(Psi);
				const double P = std::exp(-(Sx * Sx + Sy * Sy) / (2.0 * S2)) / (TwoPi * S2);
				const V3 N = Unit({ -Sx, -Sy, 1.0 });
				const double Nl = Dot(N, L), Nv = Dot(N, V);
				if (Nl <= 0.0 || Nv <= 0.0) continue;
				Sum += P * Spec(N, L, V, R0) * Nl * Nv / N.Z * R * Dr * (TwoPi / NA);
			}
		}
		// (the slopes left to the roughness mask and shadow each other: Smith, height-correlated, Gaussian slopes)
		auto Lam = [&](double Cz)
		{
			const double T = std::sqrt(std::max(1.0 - Cz * Cz, 0.0)) / Cz;
			if (T < 1e-9) return 0.0;
			const double A = 1.0 / (std::sqrt(Mss) * T);
			return (std::erf(A) - 1.0) / 2.0 + std::exp(-A * A) / (2.0 * A * std::sqrt(Pi));
		};
		return Sum / V.Z / (1.0 + Lam(V.Z) + Lam(L.Z));
	}

	void Road()
	{
		double LoAcross = 9.0, HiAcross = 0.0, LoAlong = 9.0, HiAlong = 0.0, V1Worst = 0.0;
		int V1Fails = 0, Cases = 0;
		bool bWidens = true;
		for (double R0 : { 0.05, 0.3 })
		{
			for (double Sun : { 20.0, 40.0 })
			{
				double LastAcross = 0.0;
				for (double U : { 3.0, 8.0, 14.0 })
				{
					const double Mss = CoxMunkOf(U), E = Sun * Pi / 180.0;
					const V3 L = { std::cos(E), 0.0, std::sin(E) };
					const double Reff = RoughnessFor(R0, Mss), Rv1 = std::sqrt(R0 * R0 + Mss);
					const int NP = 61, ND = 31;
					const double PM = 45.0, DMax = std::min(89.0, 2.0 * Sun + 35.0);
					std::vector<double> G[3];
					for (auto& Gm : G) Gm.assign(NP * ND, 0.0);
					for (int I = 0; I < NP; ++I)
					{
						for (int J = 0; J < ND; ++J)
						{
							const double Phi = (-PM + 2.0 * PM * I / (NP - 1)) * Pi / 180.0, Dl = (0.5 + (DMax - 0.5) * J / (ND - 1)) * Pi / 180.0;
							const V3 V = { -std::cos(Dl) * std::cos(Phi), -std::cos(Dl) * std::sin(Phi), std::sin(Dl) };
							G[0][I * ND + J] = Truth(L, V, R0, Mss);
							G[1][I * ND + J] = Spec({ 0.0, 0.0, 1.0 }, L, V, Reff) * L.Z;
							G[2][I * ND + J] = Spec({ 0.0, 0.0, 1.0 }, L, V, Rv1) * L.Z;
						}
					}
					// (the road's width across, at the mirror's row, and its length along, at the mirror's column: each the
					// equivalent width, the sum over the peak)
					int Jm = 0;
					double Bd = 1e9;
					for (int J = 0; J < ND; ++J)
					{
						const double Dd = std::fabs(0.5 + (DMax - 0.5) * J / (ND - 1) - Sun);
						if (Dd < Bd)
						{
							Bd = Dd;
							Jm = J;
						}
					}
					const int Im = (NP - 1) / 2;
					double Across[3], Along[3];
					for (int M = 0; M < 3; ++M)
					{
						const double Pk = G[M][Im * ND + Jm];
						double A = 0.0, B = 0.0;
						for (int I = 0; I < NP; ++I) A += G[M][I * ND + Jm];
						for (int J = 0; J < ND; ++J) B += G[M][Im * ND + J];
						Across[M] = A * (2.0 * PM / (NP - 1)) / Pk;
						Along[M] = B * ((DMax - 0.5) / (ND - 1)) / Pk;
					}
					++Cases;
					const double Ra = Across[1] / Across[0], Rl = Along[1] / Along[0], R1 = Across[2] / Across[0];
					LoAcross = std::min(LoAcross, Ra);
					HiAcross = std::max(HiAcross, Ra);
					LoAlong = std::min(LoAlong, Rl);
					HiAlong = std::max(HiAlong, Rl);
					if (R1 < 0.8 || R1 > 1.2) ++V1Fails;
					V1Worst = std::max(V1Worst, std::fabs(R1 - 1.0));
					if (Across[1] < LastAcross) bWidens = false;
					LastAcross = Across[1];
				}
			}
		}
		Check("road: M3's roughness (r0^4 + dmss)^(1/4) gives the dense BRDF average's width across within 20% and along within 40%",
			LoAcross >= 0.8 && HiAcross <= 1.2 && LoAlong >= 0.6 && HiAlong <= 1.4,
			std::to_string(Cases) + " cases (3, 8, 14 m/s; r0 0.05, 0.3; sun 20, 40 deg): across " + Num(LoAcross, 2) + " to " + Num(HiAcross, 2) + ", along " + Num(LoAlong, 2) + " to " +
				Num(HiAlong, 2));
		Check("road: v1's sqrt(r0^2 + dmss) misses it (the test can tell), and the road widens with the wind", V1Fails == Cases && bWidens,
			std::to_string(V1Fails) + " of " + std::to_string(Cases) + " outside 20%, worst " + Num(100 * V1Worst, 0) + "% off");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// No wrap pops.

	void Wraps()
	{
		const Sea S = SeaOf(11.0, 21, false, 1.8, 10.5);
		const double U = 100.0;
		double WorstStep = 0.0, WorstBefore = 0.0, WorstMaterial = 0.0;
		long long Frames = 0;
		auto StepOf = [&](const ForStage& A, const ForStage& B, double Dt, bool bBefore)
		{
			double W = 0.0;
			for (int J = 0; J < S.ND; ++J)
			{
				W = std::max(W, std::fabs(Turns(static_cast<double>(B.Train[J].A) - A.Train[J].A + S.D[J].W * Dt / TwoPi)));
				if (bBefore) W = std::max(W, std::fabs(Turns(static_cast<double>(B.TrainBefore[J].A) - A.TrainBefore[J].A + S.D[J].W * Dt / TwoPi)));
			}
			for (int J = 0; J < S.NP; ++J) W = std::max(W, std::fabs(Turns(static_cast<double>(B.Pixel[J].A) - A.Pixel[J].A + S.P[J].W * Dt / TwoPi)));
			for (int R = 0; R < ReadCount; ++R)
			{
				auto Get = [R](const ForStage& F)
				{
					const F4& V = F.ReadPhase[R / 4];
					return static_cast<double>(R % 4 == 0 ? V.R : R % 4 == 1 ? V.G : R % 4 == 2 ? V.B : V.A);
				};
				W = std::max(W, std::fabs(Turns(Get(B) - Get(A) - S.Read[R] * Dt)));
			}
			return W;
		};
		const Drawn Base = FrameOf(S, 1234.0, -5678.0, 30.0, 0.0);
		// (the whole material term at a few of the drawn points, frame by frame through the wraps)
		const Mesh G = Grid(96, Base.Rings);
		std::vector<int> Points;
		for (int K = 0; K < 40; ++K) Points.push_back(static_cast<int>(Mix(900 + K) * G.Points.size()));
		double Rate = 0.0;
		for (int J = 0; J < S.ND; ++J) Rate += S.D[J].A * S.D[J].W * (1.0 + S.D[J].Q * 2.0);
		for (double Mid : { 600.0, 3600.0 })
		{
			for (double Dt : { 0.001, 1.0 / 60.0 })
			{
				Drawn D = Base;
				D.SeaT = D.SeaTBefore = Mid - 0.01 - 2 * Dt;
				ForStage Last = StageParams(S, D, U);
				std::vector<Gpu::float4> LastOut(Points.size());
				for (int N = 0; Mid - 0.01 - 2 * Dt + N * Dt <= Mid + 0.01 + 2 * Dt; ++N)
				{
					D.SeaTBefore = D.SeaT;
					D.SeaT = Mid - 0.01 - 2 * Dt + N * Dt;
					const ForStage F = StageParams(S, D, U);
					if (N > 0)
					{
						WorstStep = std::max(WorstStep, StepOf(Last, F, D.SeaT - D.SeaTBefore, N >= 2));
						for (int J = 0; J < S.ND; ++J) WorstBefore = std::max(WorstBefore, std::fabs(static_cast<double>(F.TrainBefore[J].A) - Last.Train[J].A));
					}
					Gpu::Inputs In = Gpu::From(F, false);
					for (size_t P = 0; P < Points.size(); ++P)
					{
						const GridPoint& Gp = G.Points[Points[P]];
						In.Local = Gpu::float2(Gp.X, Gp.Y);
						In.Vtx = Gpu::float2(static_cast<float>(Gp.Ring), static_cast<float>(Gp.Kind));
						In.Mode = 0.0f;
						const Gpu::float4 O = Gpu::Run(In);
						if (N > 0)
						{
							const double Move = std::sqrt(Sq(O.x - LastOut[P].x) + Sq(O.y - LastOut[P].y) + Sq(O.z - LastOut[P].z)) / U;
							WorstMaterial = std::max(WorstMaterial, Move / (Rate * (D.SeaT - D.SeaTBefore) + 1e-3));
						}
						LastOut[P] = O;
					}
					Last = F;
					++Frames;
				}
			}
		}
		Check("no wrap pops: every phase the MPC carries (Train, TrainBefore, Pixel, every ReadPhase) steps by rate x dt within 1e-6 turns across 599.99-600.01 and 3599.99-3600.01 s",
			WorstStep <= 1e-6, "worst " + Sci(WorstStep) + " turns, " + std::to_string(Frames) + " frames");
		Check("no wrap pops: TrainBefore is the frame before's Train on this frame's snaps", WorstBefore == 0.0, Sci(WorstBefore));
		Check("no wrap pops: the whole material term (the .inl's points) moves no faster than the waves across the same instants", WorstMaterial <= 1.0,
			"most " + Num(WorstMaterial, 3) + " of the waves' own speed (1 mm allowed)");
		// four hours at 60 frames a second
		Drawn D = Base;
		D.SeaT = D.SeaTBefore = 0.0;
		ForStage Last = StageParams(S, D, U);
		double Worst4h = 0.0;
		const long long N4 = 4LL * 3600 * 60;
		for (long long N = 1; N <= N4; ++N)
		{
			D.SeaTBefore = D.SeaT;
			D.SeaT = static_cast<double>(N) / 60.0;
			const ForStage F = StageParams(S, D, U);
			Worst4h = std::max(Worst4h, StepOf(Last, F, D.SeaT - D.SeaTBefore, N >= 2));
			Last = F;
		}
		Check("no wrap pops: over 4 hours at 60 fps each frame's step is rate x dt within 1e-6 turns", Worst4h <= 1e-6, "worst " + Sci(Worst4h) + " turns over " + std::to_string(N4) + " frames");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The weights, the grid, the snaps, the patch, the chord bound.

	struct Laid
	{
		std::vector<Placed> P;
	};
	Laid LayAll(const Sea& S, const Drawn& D, const Mesh& M, bool bBefore = false)
	{
		Laid L;
		L.P.reserve(M.Points.size());
		for (const GridPoint& G : M.Points) L.P.push_back(PlaceOf(S, D, G, bBefore));
		return L;
	}

	void Weights()
	{
		bool bEdges = true;
		for (double L : { 8.0, 20.0, 57.3, 150.0, 400.0 })
		{
			bEdges = bEdges && WeightOf(L, L / 8.0 * (1.0 + 1e-9)) == 0.0 && WeightOf(L, L / 8.0 * 1.01) == 0.0 && WeightOf(L, L / 12.0) == 1.0 && WeightOf(L, L / 12.0 * 0.99) == 1.0;
			double Prev = 1.0;
			for (int K = 0; K <= 100; ++K)
			{
				const double W = WeightOf(L, L / 14.0 + (L / 7.0 - L / 14.0) * K / 100.0);
				bEdges = bEdges && W <= Prev + 1e-15;
				Prev = W;
			}
		}
		Check("weights: w is 0 wherever the cell is over L/8 and 1 wherever it is L/12 or less (and falls between)", bEdges);
		// every train with weight has 8 or more points a wavelength in the drawn triangulation
		long long Tris = 0, Bad = 0;
		double Worst = 0.0;
		const double Subj[2] = { 900.0, 350.0 };
		struct Case
		{
			double U, SwellH, Ex, Ey, Ez;
			bool bSubject;
		};
		const Case Cases[] = { { 6.0, 0.0, 10.0, 20.0, 12.0, false }, { 13.8, 2.0, -3000.0, 7000.0, 60.0, true }, { 9.0, 1.0, 22000.0, -23000.0, 30.0, true },
			{ 24.4, 3.0, 50.0, 50.0, 400.0, false } };
		for (const Case& C : Cases)
		{
			const Sea S = SeaOf(C.U, 31, C.U > Raised, C.SwellH, 12.0);
			const double Sub[2] = { C.Ex + Subj[0], C.Ey + Subj[1] };
			const Drawn D = FrameOf(S, C.Ex, C.Ey, C.Ez, 77.0, C.bSubject ? Sub : nullptr);
			for (int Which = 0; Which < 2; ++Which)
			{
				if (Which == 1 && !D.Patch.bOn) continue;
				const Mesh M = Which == 0 ? Grid(D.N, D.Rings) : Patch();
				const Laid L = LayAll(S, D, M);
				for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
				{
					const Placed* V[3] = { &L.P[M.Triangles[T]], &L.P[M.Triangles[T + 1]], &L.P[M.Triangles[T + 2]] };
					// (a triangle with a point dropped under the patch is out of sight)
					if (V[0]->bDropped || V[1]->bDropped || V[2]->bDropped) continue;
					const double X0 = std::min({ V[0]->RestX, V[1]->RestX, V[2]->RestX }), X1 = std::max({ V[0]->RestX, V[1]->RestX, V[2]->RestX });
					const double Y0 = std::min({ V[0]->RestY, V[1]->RestY, V[2]->RestY }), Y1 = std::max({ V[0]->RestY, V[1]->RestY, V[2]->RestY });
					// (an eased point's height has gone toward the middle of its even neighbours, a cell of its ring either side:
					// so far as it has, the sea is sampled there at up to twice its ring's cell, and the triangle counts that)
					double Extent = std::max(X1 - X0, Y1 - Y0);
					if (Extent <= 0.0) continue;
					for (int Q = 0; Q < 3; ++Q)
					{
						if (V[Q]->Ease <= 0.0) continue;
						const double Cv = D.C0 * std::ldexp(1.0, M.Points[M.Triangles[T + Q]].Ring);
						Extent = std::max(Extent, Cv * (1.0 + V[Q]->Ease));
					}
					++Tris;
					for (int J = 0; J < S.ND; ++J)
					{
						bool bAny = false;
						for (const Placed* P : V) bAny = bAny || WeightOf(S.D[J].L, P->Cell) > 0.0;
						if (!bAny) continue;
						const double Over = Extent / (S.D[J].L / 8.0);
						Worst = std::max(Worst, Over);
						if (Over > 1.0 + 1e-9) ++Bad;
					}
				}
			}
		}
		Check("weights: every train with weight has at least 8 points a wavelength in the drawn triangulation (rings, trims, morph, skirt, patch; an eased point's spacing counted as its ring's cell times one and its ease)", Bad == 0 && Tris > 100000,
			std::to_string(Tris) + " triangles, the longest side against L/8 at most " + Num(Worst, 3));
	}

	struct Key2
	{
		int64_t X, Y;
		bool operator==(const Key2& O) const { return X == O.X && Y == O.Y; }
	};
	struct KeyHash
	{
		size_t operator()(const Key2& K) const { return std::hash<int64_t>()(K.X * 1000003 + K.Y); }
	};
	struct EdgeKey
	{
		Key2 A, B;
		bool operator==(const EdgeKey& O) const { return A == O.A && B == O.B; }
	};
	struct EdgeHash
	{
		size_t operator()(const EdgeKey& K) const { return KeyHash()(K.A) * 31 + KeyHash()(K.B); }
	};
	Key2 KeyOf(double X, double Y) { return { static_cast<int64_t>(std::llround(X * 1e6)), static_cast<int64_t>(std::llround(Y * 1e6)) }; }

	void GridChecks()
	{
		const Mesh G96 = Grid(96, GridRings), G128 = Grid(128, GridRings128), P = Patch();
		Check("grid: points as the ledger caps them (RingCells 96 at its most rings, 8: 72k; 128 at 7: 104k; the patch: 9.5k)",
			G96.Points.size() <= 72000 && G128.Points.size() <= 104000 && P.Points.size() <= 9500,
			std::to_string(G96.Points.size()) + ", " + std::to_string(G128.Points.size()) + " and " + std::to_string(P.Points.size()) + " points (" +
				std::to_string(G96.Triangles.size() / 3) + " triangles at 96)");
		// (the grid a battle builds, Grid(N, RingsFor(S)), for every wind Make allows, with and without a swell)
		{
			int Seas = 0, MostRingsSeen = 0;
			size_t Most96 = 0, Most128 = 0;
			bool bOk = true;
			std::map<int, std::pair<size_t, size_t>> ByRings;
			for (int G = 0; G < 2; ++G)
			{
				const double Cap = G ? GaleCap : Raised;
				for (double U = 0.5; U <= Cap + 1e-9; U = std::min(U + 0.5, U < Cap - 1e-9 ? Cap : Cap + 1.0))
				{
					for (int Sw = 0; Sw < 3; ++Sw)
					{
						const double SwellH = Sw == 0 ? 0.0 : (Sw == 1 ? 1.8 : 3.0), SwellT = Sw == 0 ? 0.0 : (Sw == 1 ? 12.0 : 16.0);
						const Sea S = SeaOf(U, 31 + static_cast<uint64_t>(U * 10), G == 1, SwellH, SwellT);
						const int R = RingsFor(S), R128 = RingsFor(S, 128);
						++Seas;
						MostRingsSeen = std::max(MostRingsSeen, R);
						if (!ByRings.count(R)) ByRings[R] = { Grid(96, R).Points.size(), 0 };
						if (!ByRings.count(-R128)) ByRings[-R128] = { 0, Grid(128, R128).Points.size() };
						Most96 = std::max(Most96, ByRings[R].first);
						Most128 = std::max(Most128, ByRings[-R128].second);
						bOk = bOk && R <= GridRings && R128 <= GridRings128 && ByRings[R].first <= 72000 && ByRings[-R128].second <= 104000;
					}
					if (U >= Cap) break;
				}
			}
			Check("grid: Grid(N, RingsFor(S, N)) within the ledger's caps (72k at 96, 104k at 128) and at most GridRings rings (8 at 96, 7 at 128) for every wind 0.5 to 13.8 m/s and the gale's "
				  "24.4, with no swell, a 1.8 m 12 s swell and a 3 m 16 s one",
				bOk && Seas > 80, std::to_string(Seas) + " seas, at most " + std::to_string(MostRingsSeen) + " rings: " + std::to_string(Most96) + " points at 96, " +
					std::to_string(Most128) + " at 128");
		}
		long long Frames = 0, Open = 0, Flipped = 0, Edges = 0, Mixed = 0;
		int KindsSeen[5] = {};
		double WorstZ = 0.0;
		for (int F = 0; F < 24; ++F)
		{
			const double U = F % 3 == 0 ? 13.8 : (F % 3 == 1 ? 7.0 : 10.0);
			const Sea S = SeaOf(U, 50 + F, false, F % 2 ? 2.0 : 0.0, 13.0);
			const double Ex = (Mix(7000 + F) - 0.5) * 48000.0, Ey = (Mix(8000 + F) - 0.5) * 48000.0, Ez = 3.0 + Mix(9000 + F) * 120.0;
			const int N = F % 4 == 3 ? 128 : 96;
			const Drawn D = FrameOf(S, Ex, Ey, Ez, 10.0 * F, nullptr, N);
			const Mesh M = Grid(D.N, D.Rings);
			const Laid L = LayAll(S, D, M);
			++Frames;
			// (which trim each ring uses)
			for (const GridPoint& Gp : M.Points)
			{
				if (Gp.Kind >= 1 && Gp.Kind <= 4)
				{
					const Placed& Pl = L.P[&Gp - M.Points.data()];
					if (!Pl.bCollapsed) KindsSeen[Gp.Kind] = 1;
				}
			}
			std::unordered_map<EdgeKey, int, EdgeHash> Directed;
			std::unordered_map<Key2, double, KeyHash> Zs;
			for (size_t K = 0; K < L.P.size(); ++K)
			{
				const Placed& Pl = L.P[K];
				if (Pl.bCollapsed || M.Points[K].Kind == 6) continue;
				const Key2 Kk = KeyOf(Pl.RestX, Pl.RestY);
				const auto It = Zs.find(Kk);
				if (It == Zs.end()) Zs[Kk] = Pl.Z;
				else WorstZ = std::max(WorstZ, std::fabs(It->second - Pl.Z));
			}
			for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
			{
				const int I[3] = { M.Triangles[T], M.Triangles[T + 1], M.Triangles[T + 2] };
				// (the stage's frame: X is the board's Y)
				double Sx[3], Sy[3];
				for (int Q = 0; Q < 3; ++Q)
				{
					Sx[Q] = L.P[I[Q]].RestY;
					Sy[Q] = L.P[I[Q]].RestX;
				}
				const double Cross = (Sx[1] - Sx[2]) * (Sy[0] - Sy[2]) - (Sy[1] - Sy[2]) * (Sx[0] - Sx[2]);
				if (Cross < -1e-9) ++Flipped;
				// (every triangle, a morph's or a trim's collapsed ones too: a collapsed one with three points on a line
				// stands upright over its edge, and joins the two sides as a wall of no width)
				for (int Q = 0; Q < 3; ++Q)
				{
					const int A = I[Q], B = I[(Q + 1) % 3];
					if (M.Points[A].Kind == 6 && M.Points[B].Kind == 6) continue; // (the skirt's far edge is the world's)
					const Key2 Ka = KeyOf(L.P[A].RestX, L.P[A].RestY), Kb = KeyOf(L.P[B].RestX, L.P[B].RestY);
					if (Ka == Kb) continue;
					Directed[{ Ka, Kb }] += 1;
				}
			}
			for (const auto& E : Directed)
			{
				++Edges;
				const auto Back = Directed.find({ E.first.B, E.first.A });
				if (Back == Directed.end()) ++Open;
				else if (Back->second != E.second) ++Mixed;
			}
		}
		Check("grid: watertight for every combination of snaps: each inside edge laid as often one way as the other, no T-junction after the trims collapse and the morph",
			Open == 0 && Mixed == 0, std::to_string(Frames) + " frames, " + std::to_string(Edges) + " edges by place, " + std::to_string(Open) + " open, " + std::to_string(Mixed) + " unbalanced");
		Check("grid: every trim's four ways used (the three not in use collapse to a point)", KindsSeen[1] && KindsSeen[2] && KindsSeen[3] && KindsSeen[4]);
		Check("grid: points that meet have one height (ring to trim to ring)", WorstZ <= 1e-9, Sci(WorstZ) + " m");
		Check("grid: every triangle faces up as AAirStage::BuildSea lays them", Flipped == 0, std::to_string(Flipped) + " flipped");
	}

	void Snaps()
	{
		const Sea S = SeaOf(12.0, 61, false, 1.5, 11.0);
		const Drawn Base = FrameOf(S, 333.3, -444.4, 20.0, 50.0);
		const double Reach = (Base.N / 2) * CellOf(Base, Base.Rings - 1) * 0.98;
		std::vector<std::pair<double, double>> Fixed;
		for (int K = 0; K < 10000; ++K)
		{
			// (most near the eye, where the rings are fine)
			const double F = std::pow(Mix(100 + K), 3.0);
			Fixed.emplace_back(Base.EyeX + (2.0 * Mix(200000 + K) - 1.0) * Reach * F, Base.EyeY + (2.0 * Mix(300000 + K) - 1.0) * Reach * F);
		}
		double WorstSnap = 0.0;
		int Snapped = 0;
		for (int K = 0; K < Base.Rings; ++K)
		{
			for (int Axis = 0; Axis < 2; ++Axis)
			{
				const double Step = 2.0 * CellOf(Base, K);
				const double At = (std::floor((Axis == 0 ? Base.EyeX : Base.EyeY) / Step) + 1.0) * Step;
				double Ex = Base.EyeX, Ey = Base.EyeY;
				(Axis == 0 ? Ex : Ey) = At - 1e-7;
				const Drawn A = FrameOf(S, Ex, Ey, 20.0, 50.0, nullptr, 96, &Base);
				(Axis == 0 ? Ex : Ey) = At + 1e-7;
				const Drawn B = FrameOf(S, Ex, Ey, 20.0, 50.0, nullptr, 96, &A);
				if ((Axis == 0 ? A.RingX[K] : A.RingY[K]) != (Axis == 0 ? B.RingX[K] : B.RingY[K])) ++Snapped;
				for (const auto& P : Fixed) WorstSnap = std::max(WorstSnap, std::fabs(DrawnMeshZ(S, A, P.first, P.second) - DrawnMeshZ(S, B, P.first, P.second)));
			}
		}
		Check("snaps: DrawnMeshZ at 10k fixed points changes by under 1 cm across a snap of any ring", Snapped == 2 * Base.Rings && WorstSnap < 0.01,
			std::to_string(Snapped) + " snaps, worst " + Num(1000 * WorstSnap, 3) + " mm");
		// across the rings' edges
		double WorstEdge = 0.0;
		int Crossings = 0;
		for (int K = 0; K + 1 < Base.Rings; ++K)
		{
			const double H = (Base.N / 2) * CellOf(Base, K);
			for (int Q = 0; Q < 400; ++Q)
			{
				const double T = (2.0 * Mix(400000 + 1000 * K + Q) - 1.0) * H * 0.999;
				const int Side = Q % 4;
				double X = Base.RingX[K], Y = Base.RingY[K];
				double Nx = 0.0, Ny = 0.0;
				if (Side == 0) { X += H; Y += T; Nx = 1.0; }
				else if (Side == 1) { X -= H; Y += T; Nx = -1.0; }
				else if (Side == 2) { Y += H; X += T; Ny = 1.0; }
				else { Y -= H; X += T; Ny = -1.0; }
				const double Zin = DrawnMeshZ(S, Base, X - 1e-6 * Nx, Y - 1e-6 * Ny), Zout = DrawnMeshZ(S, Base, X + 1e-6 * Nx, Y + 1e-6 * Ny);
				WorstEdge = std::max(WorstEdge, std::fabs(Zin - Zout));
				++Crossings;
			}
		}
		Check("snaps: the drawn mesh is continuous across the rings' edges (a jump under 1 cm)", WorstEdge < 0.01, std::to_string(Crossings) + " crossings, worst " + Num(1000 * WorstEdge, 4) + " mm");
		// the eye moving: a pop is a jump that does not shrink with the step. The same path is walked at 0.34 m steps and at
		// steps ten times finer: the morph and the weights slide with the eye, so the surface moves smoothly, but a snap or a
		// morph that jumped would show the same jump at both.
		auto Walk = [&](int Steps, double Scale)
		{
			double Worst = 0.0;
			Drawn Was = Base;
			std::vector<double> Before(2000);
			for (int K = 0; K < 2000; ++K) Before[K] = DrawnMeshZ(S, Was, Fixed[K].first, Fixed[K].second);
			for (int Step = 1; Step <= Steps; ++Step)
			{
				const double Ex = Base.EyeX + 0.3 * Scale * Step, Ey = Base.EyeY + 0.17 * Scale * Step;
				const Drawn Now = FrameOf(S, Ex, Ey, 20.0, 50.0, nullptr, 96, &Was);
				for (int K = 0; K < 2000; ++K)
				{
					const double Z = DrawnMeshZ(S, Now, Fixed[K].first, Fixed[K].second);
					Worst = std::max(Worst, std::fabs(Z - Before[K]));
					Before[K] = Z;
				}
				Was = Now;
			}
			return Worst;
		};
		const double StepM = std::hypot(0.3, 0.17);
		const double WorstMove = Walk(300, 1.0), WorstFine = Walk(3000, 0.1);
		Check("snaps: moving the eye never pops (300 steps of 0.34 m, every snap and morph on the way, walked again at 0.034 m: the worst change shrinks with the step, and is under 2 mm at the fine steps)",
			WorstFine <= 0.2 * WorstMove && WorstFine < 0.002,
			"worst " + Num(1000 * WorstMove, 3) + " mm a 0.34 m step, " + Num(1000 * WorstFine, 3) + " mm a 0.034 m step (" + Num(1000 * WorstMove / StepM, 1) +
				" mm a metre of eye travel, the morph and the weights sliding)");
	}

	void PatchChecks()
	{
		int On = 0, Lines = 0, LinesOk = 0;
		long long EdgePoints = 0, Dropped = 0, Hidden = 0, Eased = 0, Partly = 0;
		double WorstGap = 0.0, WorstEased = 0.0, WorstPop = 0.0, WorstPopPx = 0.0, Shallowest = 1e9;
		long long PopPoints = 0, PopOver1cm = 0;
		int PatchSnaps = 0;
		for (int F = 0; F < 12; ++F)
		{
			const Sea S = SeaOf(F % 2 ? 13.8 : 8.0, 70 + F, false, F % 3 ? 1.8 : 0.0, 12.0);
			const double Ex = (Mix(11000 + F) - 0.5) * 40000.0, Ey = (Mix(12000 + F) - 0.5) * 40000.0;
			const double Ez = 8.0 + 40.0 * Mix(13000 + F);
			const double Far = 300.0 + 900.0 * Mix(14000 + F), Bear = TwoPi * Mix(15000 + F);
			const double Sub[2] = { Ex + Far * std::sin(Bear), Ey + Far * std::cos(Bear) };
			const Drawn D = FrameOf(S, Ex, Ey, Ez, 31.0 * F, Sub);
			if (!D.Patch.bOn) continue;
			++On;
			// (its edges on every touched ring's even lines)
			for (int K = 0; K < D.Rings; ++K)
			{
				const double C = CellOf(D, K);
				bool bTouched = false;
				for (int Q = 0; Q < 9; ++Q) bTouched = bTouched || RingOf(D, D.Patch.X + ((Q % 3) - 1) * D.Patch.Half, D.Patch.Y + ((Q / 3) - 1) * D.Patch.Half) == K;
				if (!bTouched) continue;
				for (double E : { D.Patch.X - D.Patch.Half - D.RingX[K], D.Patch.X + D.Patch.Half - D.RingX[K], D.Patch.Y - D.Patch.Half - D.RingY[K],
						 D.Patch.Y + D.Patch.Half - D.RingY[K] })
				{
					++Lines;
					if (std::fmod(std::fabs(E), 2.0 * C) == 0.0) ++LinesOk;
				}
			}
			const Mesh Pm = Patch();
			for (const GridPoint& G : Pm.Points)
			{
				const Placed P = PlaceOf(S, D, G);
				const double Gap = std::fabs(P.Z - MainMeshZ(S, D, P.RestX, P.RestY));
				if (G.Kind == 7)
				{
					WorstGap = std::max(WorstGap, Gap);
					++EdgePoints;
					continue;
				}
				// (from EaseEnd round her, beside its notches and in them, the patch is the main mesh's own surface)
				const double Dp = std::max(std::fabs(P.RestX - D.Patch.SubjectX), std::fabs(P.RestY - D.Patch.SubjectY));
				if (Dp >= D.Patch.EaseEnd)
				{
					WorstEased = std::max(WorstEased, Gap);
					++Eased;
				}
				else if (Dp > D.Patch.Full) ++Partly;
			}
			const Mesh M = Grid(D.N, D.Rings);
			for (const GridPoint& G : M.Points)
			{
				if (std::fabs(G.Y * CellOf(D, std::max(G.Ring, 0)) + D.RingX[std::max(G.Ring, 0)] - D.Patch.X) > D.Patch.Half + 64.0) continue;
				const Placed P = PlaceOf(S, D, G);
				if (!P.bDropped) continue;
				++Dropped;
				const double Over = DrawnMeshZ(S, D, P.RestX, P.RestY) - P.Z;
				Shallowest = std::min(Shallowest, Over);
				if (Over >= 1.0) ++Hidden;
			}
			// (the subject moving across the patch's snaps, 1 m a step: at each snap the surface against the same frame with the
			// square left where it was, so only the snap is measured. The ease onto the main mesh goes with her unsnapped place
			// and is whole wherever the square's edge or its notches can be, so nothing may move; each point's move is also
			// taken in pixels from the eye, through the frame's lens)
			Drawn Was = D;
			std::vector<std::pair<double, double>> Near;
			for (int K = 0; K < 300; ++K)
				Near.emplace_back(D.Patch.X + (2.0 * Mix(16000 + K) - 1.0) * D.Patch.Half * 0.95, D.Patch.Y + (2.0 * Mix(17000 + K) - 1.0) * D.Patch.Half * 0.95);
			for (int K = 1; K <= 80; ++K)
			{
				const double Sub2[2] = { Sub[0] + K * 1.0, Sub[1] - K * 0.37 };
				const double Eye[3] = { Ex, Ey, Ez }, Look[3] = { Sub2[0], Sub2[1], 0.0 };
				const Drawn Now = DrawnFor(S, Eye, Look, 60.0, Sub2, Was.SeaT, &Was, false, Was.Rings, Was.N);
				if (!Now.Patch.bOn) break;
				if ((Now.Patch.X != Was.Patch.X || Now.Patch.Y != Was.Patch.Y) && Now.Patch.Cell == Was.Patch.Cell)
				{
					++PatchSnaps;
					Drawn Alt = Now;
					Alt.Patch.X = Was.Patch.X;
					Alt.Patch.Y = Was.Patch.Y;
					for (const auto& Q : Near)
					{
						const double Jump = std::fabs(DrawnMeshZ(S, Now, Q.first, Q.second) - DrawnMeshZ(S, Alt, Q.first, Q.second));
						const double Dist = std::sqrt(Sq(Q.first - Ex) + Sq(Q.second - Ey) + Ez * Ez);
						WorstPop = std::max(WorstPop, Jump);
						WorstPopPx = std::max(WorstPopPx, Jump / Dist * Now.PxPerRad);
						if (Jump > 0.01) ++PopOver1cm;
						++PopPoints;
					}
				}
				Was = Now;
			}
		}
		Check("patch: its edges lie on the main mesh's lines (twice the coarsest cell it touches) in every frame it is on", On >= 4 && Lines > 0 && LinesOk == Lines,
			std::to_string(On) + " frames with a patch, " + std::to_string(LinesOk) + " of " + std::to_string(Lines) + " edge lines");
		Check("patch: its edge is the main mesh's own (a gap under 1 mm)", EdgePoints > 0 && WorstGap < 0.001, std::to_string(EdgePoints) + " edge points, worst " + Num(1000 * WorstGap, 4) + " mm");
		Check("patch: from EaseEnd round her unsnapped place (beside its notches and in them) its points are the main mesh's own surface (a gap under 1 mm)",
			Eased > 10000 && Partly > 1000 && WorstEased < 0.001,
			std::to_string(Eased) + " points, worst " + Num(1000 * WorstEased, 4) + " mm; " + std::to_string(Partly) + " points easing onto it");
		Check("patch: the main mesh under it drops out of sight (each of its points 1 m or more under the drawn surface)", Dropped > 0 && Hidden == Dropped,
			std::to_string(Dropped) + " points dropped, the shallowest " + Num(Shallowest, 2) + " m under");
		Check("patch: as the subject moves across a snap of the patch the drawn surface moves by under 1 cm anywhere, its edge and its notches included (the "
			  "ease onto the main mesh goes with her unsnapped place, as the rings' morph goes with the eye's)",
			PatchSnaps >= 6 && PopPoints > 5000 && WorstPop < 0.01,
			std::to_string(PatchSnaps) + " snaps, " + std::to_string(PopPoints) + " points: worst " + Num(1000 * WorstPop, 3) + " mm (" + Num(WorstPopPx, 5) +
				" px at the frame's lens), " + std::to_string(PopOver1cm) + " over 1 cm");
	}

	void Chords()
	{
		double Worst = 0.0;
		long long N = 0;
		for (int F = 0; F < 6; ++F)
		{
			const Sea S = SeaOf(F % 2 ? 13.8 : 7.0, 90 + F, false, F % 3 ? 2.0 : 0.0, 12.0);
			const double Ex = 1000.0 * F, Ey = -700.0 * F, Ez = 10.0 + 25.0 * F;
			const double Sub[2] = { Ex + 500.0, Ey + 420.0 };
			const Drawn D = FrameOf(S, Ex, Ey, Ez, 40.0 * F, F % 2 ? Sub : nullptr);
			const double Reach = (D.N / 2) * CellOf(D, D.Rings - 1);
			for (int K = 0; K < 4000; ++K)
			{
				const double R = std::pow(Mix(20000 + 5000 * F + K), 3.0) * Reach;
				const double X = D.EyeX + (2.0 * Mix(21000 + 5000 * F + K) - 1.0) * R, Y = D.EyeY + (2.0 * Mix(22000 + 5000 * F + K) - 1.0) * R;
				const double Mesh_ = DrawnMeshZ(S, D, X, Y), True = HeightDrawn(S, D, X, Y), Bound = ChordBound(S, D, X, Y);
				Worst = std::max(Worst, std::fabs(Mesh_ - True) - Bound);
				++N;
			}
		}
		Check("chord bound: DrawnMeshZ within ChordBound of the weighted true height everywhere sampled (so a mark lifted by it never dips under)", Worst <= 1e-4,
			std::to_string(N) + " points, worst over the bound " + Num(1000 * Worst, 3) + " mm");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The references: HeightAt, RideCoef.

	void References()
	{
		double Worst = 0.0;
		for (int F = 0; F < 4; ++F)
		{
			const double U = F == 3 ? 24.4 : 4.0 + 4.9 * F;
			const Sea S = SeaOf(U, 100 + F, U > Raised, F >= 2 ? 2.5 : 0.0, 12.0);
			for (int K = 0; K < 500; ++K)
			{
				const double X = (Mix(30000 + 1000 * F + K) - 0.5) * 4000.0, Y = (Mix(31000 + 1000 * F + K) - 0.5) * 4000.0, T = 1000.0 * Mix(32000 + K);
				double Rx = X, Ry = Y;
				for (int It = 0; It < 50; ++It)
				{
					const Disp D = Displace(S, Rx, Ry, T);
					Rx = X - D.X;
					Ry = Y - D.Y;
				}
				Worst = std::max(Worst, std::fabs(HeightAt(S, X, Y, T) - Displace(S, Rx, Ry, T).Z));
			}
		}
		Check("references: HeightAt's 3 inversions within 2 cm of a 50-step root", Worst <= 0.02, "worst " + Num(1000 * Worst, 3) + " mm");
		// RideCoef against a time-stepped simulation of the same plane fit and filters
		double WorstRide = 0.0;
		int Cases = 0;
		const Sea S = SeaOf(11.0, 7, false, 2.0, 11.0);
		struct HullOf
		{
			double Long, Beam, RollT;
		};
		const HullOf Hulls[] = { { 100.0, 11.0, 9.0 }, { 180.0, 20.0, 12.5 }, { 250.0, 33.0, 16.0 } };
		for (const HullOf& H : Hulls)
		{
			const RideFilter Fl = FilterFor(S, H.RollT);
			for (int J : { 0, 3, S.ND - 1 })
			{
				const Train& T = S.D[J];
				for (double Off : { 0.0, 0.8, 1.6, 2.6 })
				{
					for (double Speed : { 0.0, 7.0, 14.0 })
					{
						const double Hdg = std::atan2(T.Kx, T.Ky) + Off;
						const RideC C = RideCoef(T, H.Long, H.Beam, Hdg, Speed, Fl);
						const double Ax = std::sin(Hdg), Ay = std::cos(Hdg), Rx = std::cos(Hdg), Ry = -std::sin(Hdg);
						// (the plane through her 15 stations each moment, then the filters, by RK4)
						auto Raw = [&](double Time, double& H0, double& P0, double& R0)
						{
							const double Mx = Ax * Speed * Time, My = Ay * Speed * Time;
							double Sw = 0.0, Su = 0.0, Sv = 0.0;
							H0 = P0 = R0 = 0.0;
							for (int A = 0; A < 5; ++A)
							{
								const double Uu = H.Long * (A / 4.0 - 0.5), Wa = (A == 0 || A == 4) ? 0.5 : 1.0;
								for (int B = 0; B < 3; ++B)
								{
									const double Vv = H.Beam * (B / 2.0 - 0.5), W = Wa * (B == 1 ? 1.0 : 0.5);
									const double X = Mx + Uu * Ax + Vv * Rx, Y = My + Uu * Ay + Vv * Ry;
									const double Z = T.A * std::cos(T.Kx * X + T.Ky * Y - T.W * Time + T.Phase0);
									H0 += W * Z;
									P0 += W * Uu * Z;
									R0 += W * Vv * Z;
									Sw += W;
									Su += W * Uu * Uu;
									Sv += W * Vv * Vv;
								}
							}
							H0 /= Sw;
							P0 /= Su;
							R0 /= Sv;
						};
						double Y[6] = {}; // heave, its rate; pitch, its rate; roll, its rate (the raw slope's response)
						auto Deriv = [&](double Time, const double (&In)[6], double (&Out)[6])
						{
							double H0, P0, R0;
							Raw(Time, H0, P0, R0);
							const double Wh = Fl.HeaveW, Wr = Fl.RollW;
							Out[0] = In[1];
							Out[1] = Wh * Wh * (H0 - In[0]) - 2.0 * Fl.HeaveZeta * Wh * In[1];
							Out[2] = In[3];
							Out[3] = Wh * Wh * (P0 - In[2]) - 2.0 * Fl.HeaveZeta * Wh * In[3];
							Out[4] = In[5];
							Out[5] = Wr * Wr * (R0 - In[4]) - 2.0 * Fl.RollZeta * Wr * In[5];
						};
						const double Dt = 0.01, Total = 330.0, From = 250.0;
						double Err[3] = {}, Amp[3] = { std::abs(C.Heave), std::abs(C.Pitch), std::abs(C.Roll) };
						for (double Time = 0.0; Time < Total; Time += Dt)
						{
							double K1[6], K2[6], K3[6], K4[6], Tmp[6];
							Deriv(Time, Y, K1);
							for (int Q = 0; Q < 6; ++Q) Tmp[Q] = Y[Q] + 0.5 * Dt * K1[Q];
							Deriv(Time + 0.5 * Dt, Tmp, K2);
							for (int Q = 0; Q < 6; ++Q) Tmp[Q] = Y[Q] + 0.5 * Dt * K2[Q];
							Deriv(Time + 0.5 * Dt, Tmp, K3);
							for (int Q = 0; Q < 6; ++Q) Tmp[Q] = Y[Q] + Dt * K3[Q];
							Deriv(Time + Dt, Tmp, K4);
							for (int Q = 0; Q < 6; ++Q) Y[Q] += Dt / 6.0 * (K1[Q] + 2.0 * K2[Q] + 2.0 * K3[Q] + K4[Q]);
							const double Now = Time + Dt;
							if (Now < From) continue;
							const double Th = T.Kx * Ax * Speed * Now + T.Ky * Ay * Speed * Now - T.W * Now + T.Phase0;
							const std::complex<double> E = std::polar(1.0, Th);
							Err[0] = std::max(Err[0], std::fabs((C.Heave * E).real() - Y[0]));
							Err[1] = std::max(Err[1], std::fabs((C.Pitch * E).real() - Y[2] * 180.0 / Pi));
							Err[2] = std::max(Err[2], std::fabs((C.Roll * E).real() + Y[4] * 180.0 / Pi));
						}
						for (int Q = 0; Q < 3; ++Q)
						{
							if (Amp[Q] > 1e-6) WorstRide = std::max(WorstRide, Err[Q] / Amp[Q]);
						}
						++Cases;
					}
				}
			}
		}
		Check("references: RideCoef's closed form within 1% of a time-stepped simulation of the same plane fit and filters, after transients", WorstRide <= 0.01,
			std::to_string(Cases) + " hulls, trains, headings and speeds; worst " + Num(100 * WorstRide, 3) + "%");
	}

	/** Every displaced train at least Long / 2 long at full weight at her stem, middle and stern (heading Hdg, from north
	 * clockwise); the least such weight, and how many such trains. */
	double LeastUnder(const Sea& S, const Drawn& D, const double* Sub, double Long, double Hdg, int& Held)
	{
		double Least = 1.0;
		Held = 0;
		for (int J = 0; J < S.ND; ++J)
		{
			if (S.D[J].L < 0.5 * Long) continue;
			++Held;
			for (double At : { -0.5, 0.0, 0.5 })
				Least = std::min(Least, WeightAt(S, D, Sub[0] + At * Long * std::sin(Hdg), Sub[1] + At * Long * std::cos(Hdg), J));
		}
		return Least;
	}

	void PatchReach()
	{
		// (far subjects: 1.5, 2 and 3 km from 14, 28 and 60 m eyes, at 8 and 13.8 m/s with and without a swell, eight bearings
		// and eight headings each, hulls of 100, 180 and 250 m (the patch's sizing): a patch, and every train of half her
		// length or more whole from stem to stern)
		int Cases = 0, Patched = 0, Whole = 0, Trains = 0;
		std::string Short, Worst;
		std::map<int, std::pair<int, int>> Others;
		for (double U : { 8.0, 13.8 })
		{
			for (double SwellH : { 0.0, U < 10.0 ? 1.8 : 2.5 })
			{
				const Sea S = SeaOf(U, 301 + static_cast<uint64_t>(U), false, SwellH, 12.0);
				for (double Ez : { 14.0, 28.0, 60.0 })
				{
					for (double Far : { 1500.0, 2000.0, 3000.0 })
					{
						for (int B = 0; B < 8; ++B)
						{
							const double Ex = 1234.5 + 97.0 * B, Ey = -876.25 + 41.0 * B, Bear = TwoPi * (B + 0.37) / 8.0;
							const double Sub[2] = { Ex + Far * std::sin(Bear), Ey + Far * std::cos(Bear) };
							const double Eye[3] = { Ex, Ey, Ez }, Look[3] = { Sub[0], Sub[1], 0.0 };
							for (double Long : { 250.0, 180.0, 100.0 })
							{
								const Drawn D = DrawnFor(S, Eye, Look, 60.0, Sub, 400.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
								bool bAll = D.Patch.bOn;
								int Held = 0;
								for (int H = 0; H < 8 && bAll; ++H) bAll = LeastUnder(S, D, Sub, Long, TwoPi * H / 8.0, Held) >= 1.0;
								Others[static_cast<int>(Long)].first += 1;
								Others[static_cast<int>(Long)].second += bAll ? 1 : 0;
								++Cases;
								Patched += D.Patch.bOn ? 1 : 0;
								Whole += bAll ? 1 : 0;
								Trains += Held;
								if (!bAll && Short.size() < 300)
									Short += Num(Long, 0) + " m hull, " + Num(U, 1) + " m/s eye " + Num(Ez, 0) + " m at " + Num(Far, 0) + " m (" +
										(D.Patch.bOn ? "cell " + Num(D.Patch.Cell, 0) : std::string("no patch")) + "); ";
							}
						}
					}
				}
			}
		}
		std::string Rest;
		for (const auto& O : Others) Rest += std::to_string(O.first) + " m hulls whole in " + std::to_string(O.second.second) + " of " + std::to_string(O.second.first) + "; ";
		Check("patch: a far subject (1.5, 2 and 3 km from 14, 28 and 60 m eyes, 8 and 13.8 m/s, with and without a swell; hulls of 100, 180 and 250 m) has a "
			  "patch in whatever ring holds her",
			Cases >= 800 && Patched == Cases, std::to_string(Patched) + " of " + std::to_string(Cases));
		Check("patch: there every displaced train of half her length or more is at full weight at her stem, middle and stern, every heading (a destroyer's 100 m, "
			  "a cruiser's 180 m, a battleship's 250 m)",
			Cases >= 800 && Whole == Cases,
			Rest + std::to_string(Trains) + " trains" + (Short.empty() ? "" : ": " + Short));

		// (the full-weight zone round her unsnapped middle: hulls of 100, 180 and 250 m at every heading (15 deg) and every
		// offset from the square's snapped middle (8 m steps over a whole snap both ways), 600 to 900 m from a 28 m eye)
		int Placed = 0, Held = 0, NoPatch = 0;
		double LeastW = 1.0;
		std::string Miss;
		for (double U : { 8.0, 13.8 })
		{
			const Sea S = SeaOf(U, 411 + static_cast<uint64_t>(U), false, U < 10.0 ? 1.8 : 2.5, 12.0);
			for (double Long : { 100.0, 180.0, 250.0 })
			{
				const double Ex = 503.0, Ey = -211.0, Ez = 28.0;
				const double Base[2] = { Ex + 650.0, Ey + 380.0 };
				const double Eye[3] = { Ex, Ey, Ez };
				const Drawn D0 = DrawnFor(S, Eye, Eye, 60.0, Base, 50.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
				if (!D0.Patch.bOn)
				{
					++NoPatch;
					continue;
				}
				const double Snap = D0.Patch.Snap;
				for (double Ox = 0.0; Ox < Snap; Ox += 8.0)
				{
					for (double Oy = 0.0; Oy < Snap; Oy += 8.0)
					{
						const double Sub[2] = { Base[0] + Ox, Base[1] + Oy };
						const Drawn D = DrawnFor(S, Eye, Eye, 60.0, Sub, 50.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
						if (!D.Patch.bOn)
						{
							++NoPatch;
							continue;
						}
						for (int H = 0; H < 24; ++H)
						{
							int Trains2 = 0;
							const double W = LeastUnder(S, D, Sub, Long, TwoPi * H / 24.0, Trains2);
							++Placed;
							LeastW = std::min(LeastW, W);
							if (W >= 1.0) ++Held;
							else if (Miss.size() < 200) Miss += Num(Long, 0) + " m at " + Num(U, 1) + " m/s offset " + Num(Ox, 0) + "," + Num(Oy, 0) + " heading " + std::to_string(15 * H) + "; ";
						}
					}
				}
			}
		}
		Check("patch: every displaced train of half her length or more has weight 1 at her stem, middle and stern, for hulls of 100, 180 and 250 m at every "
			  "heading and every offset from the square's snapped middle (its own cell out to half her length from her unsnapped middle)",
			Placed > 1000 && Held == Placed && NoPatch == 0,
			std::to_string(Held) + " of " + std::to_string(Placed) + ", the least weight " + Num(LeastW, 3) + (NoPatch ? ", " + std::to_string(NoPatch) + " without a patch" : "") +
				(Miss.empty() ? "" : ": " + Miss));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// SeaNear.

	// (plan v2's B4 lines for the world reads, step 2's third review: BuildNear with its shifts at 96 and 128, SampleNear at a
	// world point, and the frame's sum with them; the rest surface's own lines, 0.2, 0.35 ms, 12 ns and 0.3 ms, stand)
	constexpr double NearBuildWorld96 = 0.4e-3, NearBuildWorld128 = 0.5e-3, NearSampleWorld = 35e-9, NearFrameWorld = 0.6e-3;

	/** One frame's SeaNear cost at a shot height: BuildNear (least of twenty builds, and more while over) and SampleNear a
	 * sample over 40 ships' 250 points each, ships out to 3 km, each ship's points along and across her 250 m and taken ship
	 * by ship as the kit takes them (least of fifteen rounds, and more while over); in the thread's own time. bWorld: the
	 * world reads (BuildNear with its shifts, SampleNear); else the rest surface alone (BuildNear's heights,
	 * SampleNearRest), the ledger's first line. */
	void NearCost(const Sea& S, const Drawn& D, double Most, double& Build, double& Sample, int& Levels, bool bWorld = false)
	{
		Near Nr;
		BuildNear(S, D, Nr, MostRings, bWorld);
		Levels = Nr.NL;
		std::vector<double> Xs, Ys;
		for (int Ship = 0; Ship < 40; ++Ship)
		{
			const double R = 3000.0 * std::sqrt(Mix(61000 + Ship)), A = TwoPi * Mix(62000 + Ship), H = TwoPi * Mix(63000 + Ship);
			const double Cx = D.EyeX + R * std::sin(A), Cy = D.EyeY + R * std::cos(A);
			for (int K = 0; K < 250; ++K)
			{
				const double Along = (2.0 * Mix(64000 + 250 * Ship + K) - 1.0) * 125.0, Across = (2.0 * Mix(65000 + 250 * Ship + K) - 1.0) * 15.0;
				Xs.push_back(Cx + Along * std::sin(H) + Across * std::cos(H));
				Ys.push_back(Cy + Along * std::cos(H) - Across * std::sin(H));
			}
		}
		double Sink = 0.0;
		Sample = LeastOf(15, 9e-9, 3.0, [&]
		{
			const double T0 = CpuNow();
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				if (bWorld)
					for (size_t K = 0; K < Xs.size(); ++K) Sink += SampleNear(Nr, Xs[K], Ys[K]);
				else
					for (size_t K = 0; K < Xs.size(); ++K) Sink += SampleNearRest(Nr, Xs[K], Ys[K]);
			}
			return (CpuNow() - T0) / (4.0 * Xs.size());
		});
		if (Sink == 12345.678) std::printf("%g\n", Sink);
		// (the build last, and more of it while the frame is over: its share is what the samples and the ride leave)
		Build = LeastOf(20, Most - 1e4 * Sample - 0.02e-3, 3.0, [&]
		{
			const double T0 = CpuNow();
			BuildNear(S, D, Nr, MostRings, bWorld);
			return CpuNow() - T0;
		});
	}

	void SeaNear()
	{
		double Worst = 0.0, Mean = 0.0, Farthest = 0.0, WorstFar = 0.0;
		long long N = 0, Beyond = 0;
		double BuildWorst = 0.0, BuildWorldWorst = 0.0;
		int MostLevels = 0;
		long long Nodes = 0;
		// (the subject 600 m out, where her patch has an 8 m cell, or 450 m out (Off 0.75), where it has 4 m)
		struct Case
		{
			double U, SwellH, Ez;
			bool bSubject;
			double Off = 1.0;
		};
		const Case Cases[] = { { 6.0, 0.0, 6.0, false }, { 13.8, 2.5, 6.0, true }, { 13.8, 2.5, 40.0, true }, { 10.0, 1.5, 150.0, false }, { 13.8, 0.0, 20.0, true },
			{ 8.0, 1.8, 14.0, true }, { 8.0, 1.8, 28.0, true }, { 13.8, 0.0, 14.0, true }, { 13.8, 2.5, 14.0, true, 0.75 }, { 8.0, 1.8, 28.0, true, 0.75 },
			{ 13.8, 2.5, 6.0, true, 0.75 } };
		int Cells4 = 0, Cells8 = 0;
		for (const Case& C : Cases)
		{
			const Sea S = SeaOf(C.U, 140, false, C.SwellH, 12.0);
			const double Sub[2] = { 3210.0 + 600.0 * C.Off, -987.0 + 300.0 * C.Off };
			const Drawn D = FrameOf(S, 3210.0, -987.0, C.Ez, 222.0, C.bSubject ? Sub : nullptr);
			Cells4 += D.Patch.bOn && D.Patch.Cell == PatchCell ? 1 : 0;
			Cells8 += D.Patch.bOn && D.Patch.Cell == 2.0 * PatchCell ? 1 : 0;
			Near Nr;
			BuildNear(S, D, Nr);
			// (the cost: the worst sea, least of twenty in the thread's own time, and more while over, the cloud's cores being shared)
			const double Best = LeastOf(20, 0.2e-3, 3.0, [&]
			{
				const double T0 = CpuNow();
				BuildNear(S, D, Nr, MostRings, false);
				return CpuNow() - T0;
			});
			BuildWorst = std::max(BuildWorst, Best);
			BuildWorldWorst = std::max(BuildWorldWorst, LeastOf(20, NearBuildWorld96, 3.0, [&]
			{
				const double T0 = CpuNow();
				BuildNear(S, D, Nr);
				return CpuNow() - T0;
			}));
			MostLevels = std::max(MostLevels, Nr.NL);
			long long Here = Nr.L[NearBlock].Nodes * Nr.L[NearBlock].Nodes;
			for (int K = 0; K < Nr.NL; ++K) Here += static_cast<long long>(Nr.L[K].Nodes) * Nr.L[K].Nodes;
			Nodes = std::max(Nodes, Here);
			// (everywhere: out to the farthest point where any train has weight (the last level's square: beyond it every
			// weight is nought) and on to 3 km, more often near the eye; and the subject's block)
			const NearLevel& Coarse = Nr.L[std::max(Nr.NL - 1, 0)];
			const double Reach = std::max(std::fabs(Coarse.Mx - D.EyeX), std::fabs(Coarse.My - D.EyeY)) + Coarse.Reach;
			Farthest = std::max(Farthest, Reach);
			const double Out = std::max(Reach * 1.2, 3000.0);
			for (int K = 0; K < 8000; ++K)
			{
				const double R = std::pow(Mix(50000 + K), 2.0) * Out;
				double X = D.EyeX + (2.0 * Mix(51000 + K) - 1.0) * R, Y = D.EyeY + (2.0 * Mix(52000 + K) - 1.0) * R;
				if (C.bSubject && K % 4 == 0)
				{
					X = D.Patch.X + (2.0 * Mix(53000 + K) - 1.0) * D.Patch.Half;
					Y = D.Patch.Y + (2.0 * Mix(54000 + K) - 1.0) * D.Patch.Half;
				}
				const double E = std::fabs(SampleNearRest(Nr, X, Y) - DrawnMeshZ(S, D, X, Y));
				Worst = std::max(Worst, E);
				Mean += E;
				++N;
				if (std::max(std::fabs(X - D.EyeX), std::fabs(Y - D.EyeY)) > Reach)
				{
					++Beyond;
					WorstFar = std::max(WorstFar, E);
				}
			}
		}
		Check("SeaNear: within 5 cm of DrawnMeshZ everywhere, out to the farthest point any train has weight (8 m/s with a swell, 13.8 m/s with and without) "
			  "and on to 3 km, and over the subject's patch (at 4 m and at 8 m)",
			Worst <= 0.05 && Beyond > 1000 && Cells4 >= 2 && Cells8 >= 2,
			std::to_string(N) + " points, worst " + Num(1000 * Worst, 3) + " mm, mean " + Num(1000 * Mean / N, 3) + " mm; weight reaches " + Num(Farthest, 0) +
				" m at most, " + std::to_string(Beyond) + " points beyond it within " + Num(1000 * WorstFar, 3) + " mm; patches at 4 m in " +
				std::to_string(Cells4) + " frames, at 8 m in " + std::to_string(Cells8));
		Check("SeaNear: BuildNear costs 0.2 ms or less on the cloud's cores at RingCells 96, any eye (the busiest of eleven frames, least of twenty builds and of up "
			  "to 3 s more while over, the thread's own time)",
			BuildWorst * 1000.0 <= 0.2,
			Num(BuildWorst * 1000.0, 3) + " ms; at most " + std::to_string(MostLevels) + " levels and the subject's block, " + std::to_string(Nodes) + " nodes");
		Check("SeaNear: BuildNear with the shifts the world reads take (plan v2's B4 line for them) costs " + Num(1000.0 * NearBuildWorld96, 2) +
				  " ms or less at RingCells 96, any eye (as above)",
			BuildWorldWorst <= NearBuildWorld96, Num(BuildWorldWorst * 1000.0, 3) + " ms, the heights alone " + Num(BuildWorst * 1000.0, 3) + " ms");
		// (RingCells 128: every eye level 129 x 129, its own ledger line)
		{
			double Worst128 = 0.0, World128 = 0.0;
			for (const Case& C : Cases)
			{
				const Sea S = SeaOf(C.U, 140, false, C.SwellH, 12.0);
				const double Sub[2] = { 3210.0 + 600.0 * C.Off, -987.0 + 300.0 * C.Off };
				const Drawn D = FrameOf(S, 3210.0, -987.0, C.Ez, 222.0, C.bSubject ? Sub : nullptr, 128);
				Near Nr;
				BuildNear(S, D, Nr);
				Worst128 = std::max(Worst128, LeastOf(20, 0.35e-3, 3.0, [&]
				{
					const double T0 = CpuNow();
					BuildNear(S, D, Nr, MostRings, false);
					return CpuNow() - T0;
				}));
				World128 = std::max(World128, LeastOf(20, NearBuildWorld128, 3.0, [&]
				{
					const double T0 = CpuNow();
					BuildNear(S, D, Nr);
					return CpuNow() - T0;
				}));
			}
			Check("SeaNear: BuildNear costs 0.35 ms or less at RingCells 128 (the switch's own ledger line: its levels are 129 x 129)", Worst128 * 1000.0 <= 0.35,
				Num(Worst128 * 1000.0, 3) + " ms, the busiest of eleven frames");
			Check("SeaNear: BuildNear with the world reads' shifts costs " + Num(1000.0 * NearBuildWorld128, 2) + " ms or less at RingCells 128", World128 <= NearBuildWorld128,
				Num(World128 * 1000.0, 3) + " ms, the busiest of eleven frames");
		}
		// SampleNear's cost: points all over what it covers (a third in the subject's block), in no order; and the bilinear
		// sample alone, at the coarsest level, which is what the plan measured at 6 ns
		const Sea S = SeaOf(13.8, 141, false, 2.5, 12.0);
		const double Sub[2] = { 3210.0 + 600.0, -987.0 + 300.0 };
		const Drawn D = FrameOf(S, 3210.0, -987.0, 6.0, 222.0, Sub);
		Near Nr;
		BuildNear(S, D, Nr);
		double Reach = (D.N / 2) * D.C0;
		for (int K = 0; K + 1 < Nr.NL; ++K) Reach *= 2.0;
		std::vector<double> Xs(1 << 16), Ys(1 << 16), Xl(1 << 16), Yl(1 << 16);
		const NearLevel& Coarse = Nr.L[std::max(Nr.NL - 1, 0)];
		for (size_t K = 0; K < Xs.size(); ++K)
		{
			Xs[K] = D.EyeX + (2.0 * Mix(60000 + K) - 1.0) * Reach;
			Ys[K] = D.EyeY + (2.0 * Mix(70000 + K) - 1.0) * Reach;
			if (Nr.L[NearBlock].Reach > 0.0 && K % 3 == 0)
			{
				Xs[K] = Nr.L[NearBlock].Mx + (2.0 * Mix(80000 + K) - 1.0) * Nr.L[NearBlock].Reach;
				Ys[K] = Nr.L[NearBlock].My + (2.0 * Mix(90000 + K) - 1.0) * Nr.L[NearBlock].Reach;
			}
			Xl[K] = Coarse.Mx + (2.0 * Mix(60000 + K) - 1.0) * Coarse.Reach;
			Yl[K] = Coarse.My + (2.0 * Mix(70000 + K) - 1.0) * Coarse.Reach;
		}
		// (least of fifteen rounds of four passes, and more while over, in the thread's own time)
		double Sink = 0.0;
		const double Best = LeastOf(15, 12e-9, 3.0, [&]
		{
			const double T0 = CpuNow();
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				for (size_t K = 0; K < Xs.size(); ++K) Sink += SampleNearRest(Nr, Xs[K], Ys[K]);
			}
			return (CpuNow() - T0) / (4.0 * Xs.size());
		});
		const double BestWorld = LeastOf(15, NearSampleWorld, 3.0, [&]
		{
			const double T0 = CpuNow();
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				for (size_t K = 0; K < Xs.size(); ++K) Sink += SampleNear(Nr, Xs[K], Ys[K]);
			}
			return (CpuNow() - T0) / (4.0 * Xs.size());
		});
		const double BestLevel = LeastOf(15, 6e-9, 3.0, [&]
		{
			const double T0 = CpuNow();
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				for (size_t K = 0; K < Xl.size(); ++K) Sink += SampleLevel(Coarse, Xl[K], Yl[K]);
			}
			return (CpuNow() - T0) / (4.0 * Xl.size());
		});
		Check("SeaNear: a bilinear sample (SampleLevel, the plan's measured 6 ns) costs 6 ns or less (least of fifteen rounds and of up to 3 s more "
			  "while over)", BestLevel * 1e9 <= 6.0,
			Num(BestLevel * 1e9, 2) + " ns, " + std::to_string(Coarse.Nodes) + "^2 nodes, points in no order");
		Check("SeaNear: SampleNearRest, the level found from the eye's distance and sampled, costs 12 ns or less (plan v2's ledger counts it at 12: the bilinear "
			  "sample's 6 and the level's pick, without a search or a branch)",
			Best * 1e9 <= 12.0, Num(Best * 1e9, 2) + " ns over " + std::to_string(Nr.NL) + " levels" + (Nr.L[NearBlock].Reach > 0.0 ? " and the subject's block" : "") +
				", points in no order (" + Sci(Sink) + ")");
		Check("SeaNear: SampleNear at a world point (the shift's read, the step back to the rest point, and its height's read) costs " + Num(1e9 * NearSampleWorld, 0) +
				  " ns or less (plan v2's B4 line for it)",
			BestWorld <= NearSampleWorld, Num(BestWorld * 1e9, 2) + " ns, points in no order");
		// The game thread's SeaNear at the shots' heights: BuildNear + 10k SampleNear + the ride, 0.3 ms or less, which leaves
		// the ledger's 0.1 ms for DrawnFor, the ribbons and the ceiling inside its 0.4 ms
		{
			const Sea Sw = SeaOf(13.8, 141, false, 2.5, 12.0);
			double WorstSum = 0.0, WorstWorld = 0.0;
			std::string Each, EachWorld;
			for (double Ez : { 14.0, 18.0, 28.0, 55.0 })
			{
				const double Far[2] = { 3210.0 + 1500.0, -987.0 + 1000.0 };
				const Drawn Df = FrameOf(Sw, 3210.0, -987.0, Ez, 222.0, Far);
				double Build = 0.0, Sample = 0.0;
				int Levels = 0;
				NearCost(Sw, Df, 0.3e-3, Build, Sample, Levels);
				const double Sum = Build + 1e4 * Sample + 0.02e-3;
				WorstSum = std::max(WorstSum, Sum);
				Each += Num(Ez, 0) + " m: " + Num(1000 * Build, 3) + " + " + Num(1e4 * 1000 * Sample, 3) + " + 0.020 = " + Num(1000 * Sum, 3) + " ms (" + std::to_string(Levels) +
					" levels, " + Num(1e9 * Sample, 2) + " ns); ";
				NearCost(Sw, Df, NearFrameWorld, Build, Sample, Levels, true);
				const double World = Build + 1e4 * Sample + 0.02e-3;
				WorstWorld = std::max(WorstWorld, World);
				EachWorld += Num(Ez, 0) + " m: " + Num(1000 * Build, 3) + " + " + Num(1e4 * 1000 * Sample, 3) + " + 0.020 = " + Num(1000 * World, 3) + " ms (" +
					Num(1e9 * Sample, 2) + " ns); ";
			}
			Check("SeaNear: at the shots' eyes (14, 18, 28 and 55 m; 13.8 m/s and a 2.5 m swell; the subject 1.8 km out) BuildNear + 10k SampleNear (40 ships' "
				  "250 points, ships to 3 km) + the ride's 0.02 ms come to 0.3 ms or less (the rest surface: BuildNear's heights, SampleNearRest)",
				WorstSum * 1000.0 <= 0.3, Each);
			Check("SeaNear: the same at world points (BuildNear with its shifts, 10k SampleNear) comes to " + Num(1000.0 * NearFrameWorld, 2) +
					  " ms or less (plan v2's B4 line for the world reads)",
				WorstWorld <= NearFrameWorld, EachWorld);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The chip against the core.

	void Chip()
	{
		const double U = 100.0;
		double WorstPos = 0.0, WorstBefore = 0.0, WorstFar = 0.0, WorstAngle = 0.0, WorstJ = 0.0, WorstMss = 0.0, WorstFoam = 0.0, WorstCell = 0.0;
		long long Points = 0, Pixels = 0, Frames = 0;
		const double Wraps[] = { 599.99, 600.0, 600.01, 3599.99, 3600.01, 14400.0 };
		const Mesh Pm = Patch();
		for (int F = 0; F < 100; ++F)
		{
			const double Wind = 3.0 + 21.4 * Mix(80000 + F);
			const Sea S = SeaOf(Wind, 200 + F, true, F % 3 ? 3.0 * Mix(81000 + F) : 0.0, 8.0 + 8.0 * Mix(82000 + F));
			// (the board's corners 24 km out, a snap now and then)
			double Ex = (2.0 * Mix(83000 + F) - 1.0) * 24000.0, Ey = (2.0 * Mix(84000 + F) - 1.0) * 24000.0;
			const double Ez = 2.0 + 600.0 * std::pow(Mix(85000 + F), 2.0);
			if (F % 5 == 0) Ex = std::round(Ex / 64.0) * 64.0 + (F % 2 ? 1e-6 : -1e-6);
			const double T = F < 6 ? Wraps[F] : (F < 12 ? Wraps[F - 6] + 1e-3 : 20000.0 * Mix(86000 + F));
			const double Sub[2] = { Ex + 900.0 * std::sin(F), Ey + 900.0 * std::cos(F) };
			const Drawn D0 = FrameOf(S, Ex - 3.0, Ey + 1.0, Ez, T - 1.0 / 60.0, F % 2 ? Sub : nullptr, F % 4 == 3 ? 128 : 96);
			const Drawn D = FrameOf(S, Ex, Ey, Ez, T, F % 2 ? Sub : nullptr, D0.N, &D0);
			const ForStage Fs = StageParams(S, D, U);
			const Mesh Mm = Grid(D.N, D.Rings);
			++Frames;
			auto ToBoard = [&](const Gpu::float4& O, double& X, double& Y, double& Z)
			{
				X = D.OriginX + O.y / U;
				Y = D.OriginY + O.x / U;
				Z = O.z / U;
			};
			for (int Before = 0; Before < 2; ++Before)
			{
				Gpu::Inputs In = Gpu::From(Fs, Before == 1);
				for (int K = 0; K < 600; ++K)
				{
					const bool bPatch = D.Patch.bOn && K % 4 == 0;
					const Mesh& From = bPatch ? Pm : Mm;
					const GridPoint& Gp = From.Points[static_cast<size_t>(Mix(87000 + 1000 * F + K + 500 * Before) * From.Points.size())];
					const Placed P = PlaceOf(S, D, Gp, Before == 1);
					In.Local = Gpu::float2(Gp.X, Gp.Y);
					In.Vtx = Gpu::float2(static_cast<float>(Gp.Ring), static_cast<float>(Gp.Kind));
					In.Mode = 0.0f;
					const Gpu::float4 O = Gpu::Run(In);
					double X, Y, Z;
					ToBoard(O, X, Y, Z);
					const double E = std::sqrt(Sq(X - P.X) + Sq(Y - P.Y) + Sq(Z - P.Z));
					if (Gp.Kind == 6) WorstFar = std::max(WorstFar, E / SkirtReach);
					else (Before ? WorstBefore : WorstPos) = std::max(Before ? WorstBefore : WorstPos, E);
					if (P.Cell < 1e20 && O.w < 1e20f) WorstCell = std::max(WorstCell, std::fabs(O.w / U - P.Cell) / P.Cell);
					++Points;
				}
			}
			// the pixel
			Gpu::Inputs In = Gpu::From(Fs, false);
			for (int K = 0; K < 400; ++K)
			{
				const double Rr = 5000.0 * std::pow(Mix(88000 + 1000 * F + K), 2.0), Bb = TwoPi * Mix(89000 + 1000 * F + K);
				const double X = D.EyeX + Rr * std::sin(Bb), Y = D.EyeY + Rr * std::cos(Bb);
				const double Depth = std::max(Rr, 3.0) * (0.8 + 0.4 * Mix(90000 + K));
				double Fade[MostDrawn + MostPixel] = {};
				for (int J = 0; J < S.ND; ++J) Fade[J] = PixelFade(S.D[J].L, Depth, D.PxPerRad);
				for (int J = 0; J < S.NP; ++J) Fade[MostDrawn + J] = PixelFade(S.P[J].L, Depth, D.PxPerRad);
				const Slopes Sl = SlopesAt(S, X, Y, D.SeaT, Fade);
				In.Local = Gpu::float2(static_cast<float>((Y - D.OriginY) * U), static_cast<float>((X - D.OriginX) * U));
				In.Depth = static_cast<float>(Depth * U);
				In.Mode = 2.0f;
				const Gpu::float4 O = Gpu::Run(In);
				// (the stage's slopes: x is the board's y)
				const double Nc[3] = { -Sl.Sx, -Sl.Sy, 1.0 }, Ng[3] = { -O.y, -O.x, 1.0 };
				const double Dp = (Nc[0] * Ng[0] + Nc[1] * Ng[1] + 1.0) / (std::sqrt(Sq(Nc[0]) + Sq(Nc[1]) + 1.0) * std::sqrt(Sq(Ng[0]) + Sq(Ng[1]) + 1.0));
				WorstAngle = std::max(WorstAngle, std::acos(std::min(Dp, 1.0)) * 180.0 / Pi);
				WorstJ = std::max(WorstJ, std::fabs(O.z - Sl.J));
				const double Mss = Sl.Mss + S.DetailMss * Sq(FarLean(Depth));
				WorstMss = std::max(WorstMss, std::fabs(O.w - Mss) / std::max(Mss, 1e-6));
				// (the foam, where every train is at full weight)
				if (Depth * 4.0 * 2.0 <= D.PxPerRad * PixelShortest)
				{
					In.Mode = 3.0f;
					const Gpu::float4 Of = Gpu::Run(In);
					const Foam Fm = FoamAt(S, X, Y, D.SeaT);
					WorstFoam = std::max({ WorstFoam, std::fabs(Of.x - Fm.Head), std::fabs(Of.y - Fm.Tail) });
				}
				++Pixels;
			}
		}
		Check("chip: the .inl in float, fed StageParams (the swap, the units, turns, origins to 24 km), puts every point within 3 mm of the double core",
			WorstPos <= 0.003 && WorstCell <= 1e-5, std::to_string(Points) + " points in " + std::to_string(Frames) + " frames (wraps and snaps among them), worst " +
				Num(1000 * WorstPos, 3) + " mm; the weights' cell within " + Sci(WorstCell) + "; the skirt's far points within " + Sci(WorstFar) + " of its reach");
		Check("chip: the TrainBefore branch equals the core at the clock and eye before, on this frame's snaps (3 mm)", WorstBefore <= 0.003, Num(1000 * WorstBefore, 3) + " mm");
		Check("chip: the pixel's normal within 0.2 deg of the core's (J and the slopes drawn with it)", WorstAngle <= 0.2 && WorstJ <= 1e-4 && WorstMss <= 1e-4,
			std::to_string(Pixels) + " pixels, worst " + Num(WorstAngle, 5) + " deg, J " + Sci(WorstJ) + ", slopes drawn " + Sci(WorstMss));
		Check("chip: the whitecaps' heads and tails as the core's (0.02)", WorstFoam <= 0.02, Sci(WorstFoam));
		// an empty MPC: nothing moves, under a NaN-keeping shim
		const ForStage E = EmptyStage();
		Gpu::Inputs In = Gpu::From(E, false);
		bool bZero = true, bFinite = true;
		const Mesh G = Grid(96, 7);
		for (int K = 0; K < 2000; ++K)
		{
			const GridPoint& Gp = K % 5 == 0 ? Pm.Points[K % Pm.Points.size()] : G.Points[static_cast<size_t>(Mix(91000 + K) * G.Points.size())];
			In.Local = Gpu::float2(Gp.X, Gp.Y);
			In.Vtx = Gpu::float2(static_cast<float>(Gp.Ring), static_cast<float>(Gp.Kind));
			In.Mode = 0.0f;
			const Gpu::float4 A = Gpu::Run(In, true);
			In.Mode = 1.0f;
			const Gpu::float4 B = Gpu::Run(In, true);
			for (float V : { A.x, A.y, A.z, B.x, B.y }) bFinite = bFinite && std::isfinite(V);
			// (the waves' part: the place less the rest, and the height; the patch's points gather out of sight)
			bZero = bZero && A.x == B.x && A.y == B.y && (Gp.Ring < 0 ? A.z <= 0.0f : A.z == 0.0f);
			for (float Mode : { 2.0f, 3.0f })
			{
				In.Mode = Mode;
				In.Depth = 1000.0f * (K % 7);
				const Gpu::float4 P = Gpu::Run(In, true);
				for (float V : { P.x, P.y, P.z, P.w }) bFinite = bFinite && std::isfinite(V);
				bZero = bZero && (Mode == 2.0f ? (P.x == 0.0f && P.y == 0.0f && P.z == 1.0f && P.w == 0.0f) : (P.x == 0.0f && P.y == 0.0f));
			}
		}
		Check("chip: an empty MPC gives no displacement, flat slopes and no foam, and no NaN under a NaN-keeping shim", bZero && bFinite);
	}

	void TurnsScan(const std::string& Src)
	{
		const std::string Text = ReadAll(Src + "/Battle/FleetWavesGpu.inl");
		// each sin( and cos( takes a phase made as 6.28318531f * Turn(...) (Fw.Turn in the body), and nothing else
		const std::regex Call("\\b(sin|cos)\\(([A-Za-z0-9_]+)\\)");
		int Calls = 0, Good = 0;
		std::string Bad;
		for (auto It = std::sregex_iterator(Text.begin(), Text.end(), Call); It != std::sregex_iterator(); ++It)
		{
			++Calls;
			const std::string Var = (*It)[2];
			const std::regex Made("float\\s+" + Var + "\\s*=\\s*6\\.28318531f\\s*\\*\\s*(Fw\\.)?Turn\\(");
			if (std::regex_search(Text, Made)) ++Good;
			else Bad += Var + " ";
		}
		int Raw = 0;
		const std::regex Other("\\b(sin|cos)\\((?![A-Za-z0-9_]+\\))");
		for (auto It = std::sregex_iterator(Text.begin(), Text.end(), Other); It != std::sregex_iterator(); ++It) ++Raw;
		Check("chip: each train's turns are taken into [-0.5, 0.5) before the multiply by 2 pi (a source scan of FleetWavesGpu.inl)", Calls >= 6 && Good == Calls && Raw == 0,
			std::to_string(Good) + " of " + std::to_string(Calls) + " sines and cosines, each of a phase so taken" + (Bad.empty() ? "" : ": " + Bad) + "; " + std::to_string(Raw) + " of anything else");
		// (HLSL before 2021 lays both arms of a ?:, so an array index in either arm must be in bounds whichever arm is taken:
		// every index in an arm that is not a constant is clamped, min( or max( at its head; the C++ twin cannot show this)
		{
			std::string Body;
			// (the text without its comments)
			Body = std::regex_replace(Text, std::regex("//[^\n]*"), "");
			int Arms = 0, Indexed = 0, Clamped = 0;
			std::string Loose;
			const std::regex Ternary("\\?([^:;]*):([^;]*);");
			const std::regex Index("[A-Za-z_][A-Za-z0-9_]*\\[([^\\]]*)\\]");
			for (auto It = std::sregex_iterator(Body.begin(), Body.end(), Ternary); It != std::sregex_iterator(); ++It)
			{
				for (int Arm = 1; Arm <= 2; ++Arm)
				{
					const std::string Text2 = (*It)[Arm];
					++Arms;
					for (auto Ix = std::sregex_iterator(Text2.begin(), Text2.end(), Index); Ix != std::sregex_iterator(); ++Ix)
					{
						std::string E = (*Ix)[1];
						E.erase(0, E.find_first_not_of(" \t"));
						if (std::regex_match(E, std::regex("[0-9]+u?"))) continue;
						++Indexed;
						if (E.rfind("min(", 0) == 0 || E.rfind("max(", 0) == 0) ++Clamped;
						else Loose += "[" + E + "] ";
					}
				}
			}
			Check("chip: every array index in an arm of a ?: that is not a constant is clamped (min( or max( at its head), since HLSL lays both arms (a source scan "
				  "of FleetWavesGpu.inl)",
				Arms > 0 && Indexed >= 2 && Clamped == Indexed, std::to_string(Clamped) + " of " + std::to_string(Indexed) + " indices in " + std::to_string(Arms) + " arms" +
					(Loose.empty() ? "" : ": " + Loose));
		}
	}

	void Params(const std::string& Out)
	{
		const Sea S = SeaOf(8.0, 239, false, 1.2, 10.0);
		const double Sub[2] = { 700.0, 250.0 };
		const Drawn D = FrameOf(S, 0.0, 0.0, 20.0, 612.5, Sub);
		const ForStage F = StageParams(S, D, 100.0);
		const auto L = StageList(F);
		std::string J = "{\n  \"plan\": \"release 2 ocean plan, step 2\",\n  \"units_per_metre\": 100,\n  \"vectors\": {\n";
		for (size_t K = 0; K < L.size(); ++K)
		{
			char Row[256];
			std::snprintf(Row, sizeof(Row), "    \"%s\": [%.9g, %.9g, %.9g, %.9g]%s\n", L[K].first.c_str(), L[K].second.R, L[K].second.G, L[K].second.B, L[K].second.A,
				K + 1 < L.size() ? "," : "");
			J += Row;
		}
		J += "  }\n}\n";
		const std::string Path = Out + "/sea_waves_params.json";
		std::ofstream File(Path, std::ios::binary);
		File << J;
		File.close();
		const std::string Back = ReadAll(Path);
		Check("params: sea_waves_params.json written for material_check (every MPC vector by name)", Back == J && L.size() == 62, std::to_string(L.size()) + " vectors, " + Path);
		// the empty MPC's own defaults: no effect anywhere
		const ForStage E = EmptyStage();
		Check("params: the empty MPC is no sea (Fold under every J, no patch, the shape's divisors 1 or more)",
			E.Fold.R < 0.0f && E.Patch.A == 0.0f && E.Shape.R >= 1.0f && E.Shape.G >= 1.0f && E.Lean.A >= 1.0f);
	}

	void Lanes()
	{
		std::vector<LanePoint> Wake;
		for (int K = 0; K < 40; ++K)
		{
			LanePoint P;
			P.X = 100.0 - 8.0 * K;
			P.Y = 50.0 + 0.5 * K;
			P.Age = 4.0 * K;
			P.Beam = 20.0;
			P.Ship = 1;
			Wake.push_back(P);
		}
		std::vector<OilDisc> Oil = { { 300.0, -200.0, 40.0 } };
		const LaneWindow W = LaneWindowFor(37.3, -11.9);
		std::vector<uint8_t> A, B, C;
		PaintLanes(Wake, Oil, W, 8.0, A);
		PaintLanes(Wake, Oil, W, 8.0, B);
		PaintLanes(Wake, Oil, W, 14.0, C);
		const double Texel = W.Size / W.Texels;
		auto At = [&](const std::vector<uint8_t>& RG, double X, double Y, int Ch)
		{
			const int I = static_cast<int>((X - W.X0) / Texel), J = static_cast<int>((Y - W.Y0) / Texel);
			return RG[2 * (static_cast<size_t>(J) * W.Texels + I) + Ch];
		};
		const bool bSnapped = std::fabs(W.X0 / Texel - std::round(W.X0 / Texel)) < 1e-9 && std::fabs(W.Y0 / Texel - std::round(W.Y0 / Texel)) < 1e-9;
		const bool bFresh = At(A, 92.0, 50.5, 0) > At(A, -150.0, 65.0, 0) && At(A, 92.0, 50.5, 1) > 200 && At(A, 300.0, -200.0, 0) > 200;
		Check("lanes: PaintLanes is a pure function of its points, on a window snapped to whole texels; the lane is fresh at the stern and fades, the oil calm",
			A == B && A.size() == 2u * 512 * 512 && bSnapped && bFresh && A != C);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The ride's cost on the recorded actions, played through the show itself.

	struct HullNow
	{
		int Key;
		float X, Y, Hdg, Spd, Long, Beam, RollT;
	};

	double RollTOf(double Long)
	{
		// (SeaShow's RideOf: 9 s at 110 m and under, 12.5 at 180, 16 at 250 and over)
		const double K = std::clamp((Long - 110.0) / 140.0, 0.0, 1.0) * 2.0;
		return K < 1.0 ? 9.0 + 3.5 * K : 12.5 + 3.5 * (K - 1.0);
	}

	void Rides(const std::vector<std::string>& Recordings)
	{
		if (Recordings.empty())
		{
			Note("ride: no recordings given");
			return;
		}
		double WorstMean = 0.0;
		std::string Each;
		int Played = 0;
		for (const std::string& Path : Recordings)
		{
			SeaFeed::Recording Rec;
			std::string Error;
			if (!SeaFeed::ReadRecording(ReadAll(Path), Rec, Error) || Rec.Views.empty())
			{
				Check("ride: " + Path + " read", false, Error);
				continue;
			}
			SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239);
			Show.SetField(Rec.Board);
			Show.SetMine(SeaFeed::SideOf(Rec.Views.front().Head.Me));
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& F = Rec.Views[Taken++];
				Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
			};
			TakeNext();
			const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
			const int Frames = static_cast<int>(std::floor((EndT + 12.0 - BeginT) * 30.0)) + 1;
			std::vector<std::vector<HullNow>> All(Frames);
			size_t Most = 0, HullFrames = 0;
			for (int Fr = 0; Fr < Frames; ++Fr)
			{
				const double T = BeginT + Fr / 30.0;
				while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
				Show.Eye = SeaShow::Vec(0.0, -3000.0, 900.0);
				Show.Advance(T);
				for (const SeaShow::ShipOut& Sh : Show.Ships())
				{
					if (!Sh.bShown || Sh.Doing == SeaShow::Fate::Gone) continue;
					All[Fr].push_back({ Sh.Key, static_cast<float>(Sh.At.X), static_cast<float>(Sh.At.Y), static_cast<float>(Sh.Heading * Pi / 180.0), static_cast<float>(Sh.Speed),
						static_cast<float>(Sh.Long), static_cast<float>(Sh.Beam), static_cast<float>(RollTOf(Sh.Long)) });
				}
				Most = std::max(Most, All[Fr].size());
				HullFrames += All[Fr].size();
			}
			// (the show's own wind, and a swell, so every one of the ten trains is displaced: the most the ride can cost)
			const Sea S = SeaOf(std::max(Show.WindSpeed(), 4.0), Rec.Seed + 1, false, 1.8, 11.0, Show.WindFrom() + Pi);
			const Drawn D = FrameOf(S, 0.0, -3000.0, 900.0, BeginT);
			// (least of three plays, and more while over, in the thread's own time)
			double BestP999 = 1e9, Sink = 0.0;
			const double Best = LeastOf(3, 0.02e-3, 3.0, [&]
			{
				std::unordered_map<int, HullRide> Rides_;
				std::vector<double> Took(Frames);
				const double Start = CpuNow();
				for (int Fr = 0; Fr < Frames; ++Fr)
				{
					const double F0 = CpuNow();
					const double T = BeginT + Fr / 30.0;
					int I = 0;
					for (const HullNow& H : All[Fr])
					{
						auto It = Rides_.find(H.Key);
						// (a sixth of the hulls refreshed each frame, staggered; a new one at once)
						if (It == Rides_.end() || (Fr + I) % 6 == 0)
						{
							HullRide& R = Rides_[H.Key];
							RideTrains(S, H.Long, H.Beam, H.Hdg, H.Spd, H.RollT, R);
							It = Rides_.find(H.Key);
						}
						double W[MostDrawn];
						WeightsAt(S, D, H.X, H.Y, W);
						double Hv, Pt, Rl;
						RideAt(S, It->second, H.X, H.Y, T, W, Hv, Pt, Rl);
						Sink += Hv + Pt + Rl;
						++I;
					}
					Took[Fr] = CpuNow() - F0;
				}
				const double Whole = CpuNow() - Start;
				std::sort(Took.begin(), Took.end());
				BestP999 = std::min(BestP999, Took[static_cast<size_t>(0.999 * (Frames - 1))]);
				return Whole / std::max<size_t>(HullFrames, 1) * 40.0;
			});
			++Played;
			WorstMean = std::max(WorstMean, Best);
			const std::string Name = Path.substr(Path.find_last_of('/') + 1);
			Each += Name + ": " + std::to_string(Most) + " hulls at most, " + Num(1000.0 * Best, 4) + " ms a frame for 40 (one in a thousand frames " + Num(1000.0 * BestP999, 4) +
				" ms with " + std::to_string(Most) + "); ";
			(void)Sink;
		}
		Check("ride: 40 hulls' evaluation plus the staggered refresh 0.02 ms a frame or less on the recorded actions (the show's own hulls, frame by frame, "
			  "least of three plays and of up to 3 s more while over, the thread's own time)",
			Played == static_cast<int>(Recordings.size()) && WorstMean * 1000.0 <= 0.02, Each);
	}


	// ---------------------------------------------------------------------------------------------------------------
	// Step 2's third review: the patch over a whole sweep and on held walks, SeaNear at world points, the whitecaps at depth.

	/** The first cell the eye's height asks for (DrawnFor's, before any doubling). */
	double EyeC0(double Ez) { return std::clamp(std::ldexp(1.0, static_cast<int>(std::lround(std::log2(std::max(Ez, 0.1) / 14.0)))), LeastC0, MostC0); }

	/** The places plan v2's step 2 states where a cut's patch does not hold her trains of half her length whole (else null):
	 * "near", within 300 m of an eye whose first cell is 2 m or less, a hull over 144 m (more than a 2 m patch holds), or any
	 * from 280 m (the 4 m square cannot lie clear of the 2 m ring there, nor the 2 m square's ease clear of the 8 m ring's
	 * morph, at C0 or twice it); "mid", a hull of 240 m or more 550 to 610 m out under an eye whose first
	 * cell is 4 m or less (the 4 m square's edge reaches the 32 m ring, the 8 m square's the 4 m ring); "far", where some of
	 * her trains of half her length are under 96 m (an 8 m cell holds them partly), past the reach of the 4 m square that
	 * would hold them (the edge of the ring of its coarsest cell C, less its half side and a snap of 2 C). */
	const char* CutGap(const Sea& S, double Long, double Ez, double Cheb)
	{
		const double C0 = EyeC0(Ez);
		if (C0 <= 2.0 && Cheb <= 300.0 && (Long > 144.0 || Cheb >= 280.0)) return "near";
		if (Long >= 240.0 && C0 <= 4.0 && Cheb >= 550.0 && Cheb <= 610.0) return "mid";
		double C = PatchHalf / 3.0;
		while (C >= PatchCell && PatchHalf - 1.5 * C - PatchCell - 0.5 * Long < PatchEaseLeast) C *= 0.5;
		if (NeedCell(S, Long) < 2.0 * PatchCell && Cheb > 48.0 * C - PatchHalf - 2.0 * C) return "far";
		return nullptr;
	}

	void PatchSweep()
	{
		// (cuts: hulls of 100, 180 and 250 m from 50 m to 3 km in 5 m Chebyshev steps, at bearings of 0, 22.5, 45 and 90 deg,
		// from 14, 28 and 60 m eyes, at 8 and 13.8 m/s with and without a swell: every train of half her length or more at
		// weight 1 at her stem, middle and stern, eight headings, wherever the plan does not state otherwise)
		long long Cases = 0, Whole = 0, Unstated = 0, Patched = 0, Wanted = 0;
		std::map<std::string, long long> ByGap;
		std::string Miss;
		for (double U : { 8.0, 13.8 })
		{
			for (int Sw = 0; Sw < 2; ++Sw)
			{
				const Sea S = SeaOf(U, 521 + static_cast<uint64_t>(U) + 7 * Sw, false, Sw ? (U < 10.0 ? 1.8 : 2.5) : 0.0, 12.0);
				for (double Ez : { 14.0, 28.0, 60.0 })
				{
					for (double BearDeg : { 0.0, 22.5, 45.0, 90.0 })
					{
						const double B = BearDeg * Pi / 180.0, Bx = std::sin(B), By = std::cos(B), M = std::max(std::fabs(Bx), std::fabs(By));
						const double Ex = 1234.5 + 7.0 * BearDeg, Ey = -876.25 + 3.0 * Ez;
						const double Eye[3] = { Ex, Ey, Ez };
						for (double Far = 50.0; Far <= 3000.0 + 1e-9; Far += 5.0)
						{
							const double Sub[2] = { Ex + Far * Bx / M, Ey + Far * By / M };
							for (double Long : { 100.0, 180.0, 250.0 })
							{
								const Drawn D = DrawnFor(S, Eye, Eye, 60.0, Sub, 400.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
								bool bAll = true;
								int Held = 0;
								for (int H = 0; H < 8 && bAll; ++H) bAll = LeastUnder(S, D, Sub, Long, TwoPi * H / 8.0, Held) >= 1.0 - 1e-9;
								++Cases;
								Patched += D.Patch.bOn ? 1 : 0;
								Wanted += D.Patch.bOn || MainUnder(D, Sub, Long) > std::min(NeedCell(S, Long), PatchCell) ? 1 : 0;
								if (bAll)
								{
									++Whole;
									continue;
								}
								const char* Gap = CutGap(S, Long, Ez, Far);
								if (Gap)
								{
									++ByGap[Gap];
									continue;
								}
								++Unstated;
								if (Miss.size() < 400)
									Miss += Num(Long, 0) + " m hull " + Num(Far, 0) + " m out at " + Num(BearDeg, 1) + " deg, eye " + Num(Ez, 0) + " m, " + Num(U, 1) + " m/s" +
										(Sw ? " and swell" : "") + " (" + (D.Patch.bOn ? "cell " + Num(D.Patch.Cell, 0) + ", C0 " + Num(D.C0, 1) : std::string("no patch")) + "); ";
							}
						}
					}
				}
			}
		}
		std::string Gaps;
		for (const auto& G : ByGap) Gaps += std::to_string(G.second) + " " + G.first + "; ";
		Check("patch sweep: at a cut, hulls of 100, 180 and 250 m from 50 m to 3 km in 5 m Chebyshev steps (bearings 0, 22.5, 45 and 90 deg; eyes 14, 28 and "
			  "60 m; 8 and 13.8 m/s with and without a swell) have every train of half their length or more at weight 1 at stem, middle and stern, every heading, "
			  "but in the places plan v2's step 2 states (near, mid, far)",
			Cases > 50000 && Unstated == 0,
			std::to_string(Whole) + " of " + std::to_string(Cases) + " whole, " + std::to_string(Patched) + " with a patch (" + std::to_string(Wanted) +
				" wanting one); stated: " + Gaps + std::to_string(Unstated) + " not stated" + (Miss.empty() ? "" : ": " + Miss));

		// (a true Chebyshev 3 km, due north, north-east and east, ten seeds a wind)
		long long Far3 = 0, Patched3 = 0, Whole3 = 0, Stated3 = 0, Unstated3 = 0;
		std::string Miss3;
		for (double U : { 8.0, 13.8 })
		{
			for (uint64_t Seed = 0; Seed < 10; ++Seed)
			{
				for (int Sw = 0; Sw < 2; ++Sw)
				{
					const Sea S = SeaOf(U, 601 + 37 * Seed + static_cast<uint64_t>(U), false, Sw ? (U < 10.0 ? 1.8 : 2.5) : 0.0, 12.0);
					for (double Ez : { 14.0, 28.0, 60.0 })
					{
						for (double BearDeg : { 0.0, 45.0, 90.0 })
						{
							const double B = BearDeg * Pi / 180.0, Bx = std::sin(B), By = std::cos(B), M = std::max(std::fabs(Bx), std::fabs(By));
							const double Ex = -3456.5 + 11.0 * Seed, Ey = 2222.25 - 5.0 * Seed;
							const double Eye[3] = { Ex, Ey, Ez }, Sub[2] = { Ex + 3000.0 * Bx / M, Ey + 3000.0 * By / M };
							for (double Long : { 100.0, 180.0, 250.0 })
							{
								const Drawn D = DrawnFor(S, Eye, Eye, 60.0, Sub, 400.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
								bool bAll = D.Patch.bOn;
								int Held = 0;
								for (int H = 0; H < 8 && bAll; ++H) bAll = LeastUnder(S, D, Sub, Long, TwoPi * H / 8.0, Held) >= 1.0 - 1e-9;
								++Far3;
								Patched3 += D.Patch.bOn ? 1 : 0;
								if (bAll) ++Whole3;
								else if (D.Patch.bOn && CutGap(S, Long, Ez, 3000.0)) ++Stated3;
								else
								{
									++Unstated3;
									if (Miss3.size() < 300) Miss3 += Num(Long, 0) + " m at " + Num(BearDeg, 0) + " deg, " + Num(U, 1) + " m/s seed " + std::to_string(Seed) + "; ";
								}
							}
						}
					}
				}
			}
		}
		Check("patch: at a true Chebyshev 3 km (due north, north-east and east; ten seeds a wind, with and without a swell; eyes 14, 28 and 60 m) every subject "
			  "has a patch, and every train of half her length or more is whole but where plan v2 states it is not (some trains under 96 m past the 4 m square's reach)",
			Far3 >= 1000 && Patched3 == Far3 && Unstated3 == 0,
			std::to_string(Patched3) + " of " + std::to_string(Far3) + " with a patch; " + std::to_string(Whole3) + " whole, " + std::to_string(Stated3) +
				" in the stated far gap, " + std::to_string(Unstated3) + " not stated" + (Miss3.empty() ? "" : ": " + Miss3));
	}

	/** One held walk: her (or the eye) from 50 m to 3 km apart (or back), Step metres a frame at 10 m/s (FrameSeconds Step /
	 * 10, the sea clock held, so only the patch and the rings change), the drawn mesh at fixed points round her (her middle,
	 * stem and stern, and five within 80 m) compared frame to frame. */
	struct WalkOut
	{
		double Worst = 0.0, WorstAtSwitch = 0.0, SwitchSurface = 0.0, WorstFade = 0.0, WorstMoving = 0.0;
		long long Frames = 0, Shown = 0;
		int Switches = 0, Forced = 0, Late = 0, Fades = 0;
		std::string LateAt;
	};
	WalkOut HeldWalk(const Sea& S, double Long, double Ez, bool bEye, bool bOut, double Step)
	{
		WalkOut W;
		const double V = 10.0, Dt = Step / V, Bear = 0.52, Bx = std::sin(Bear), By = std::cos(Bear);
		const double Ex0 = 1234.5, Ey0 = -876.25, Last = 3000.0, First = 50.0;
		const int N = static_cast<int>(std::lround((Last - First) / Step));
		auto Place = [&](int K, double (&Eye)[3], double (&Sub)[2])
		{
			const double Sep = bOut ? First + K * Step : Last - K * Step;
			if (bEye)
			{
				Sub[0] = Ex0 + Last * Bx;
				Sub[1] = Ey0 + Last * By;
				Eye[0] = Sub[0] - Sep * Bx;
				Eye[1] = Sub[1] - Sep * By;
			}
			else
			{
				Eye[0] = Ex0;
				Eye[1] = Ey0;
				Sub[0] = Ex0 + Sep * Bx;
				Sub[1] = Ey0 + Sep * By;
			}
			Eye[2] = Ez;
		};
		double Eye[3], Sub[2];
		Place(0, Eye, Sub);
		Drawn Was = DrawnFor(S, Eye, Eye, 60.0, Sub, 300.0, nullptr, true, RingsFor(S), 96, 1077.0, Long);
		double Offs[8][2];
		for (int Q = 0; Q < 8; ++Q)
		{
			const double R = Q < 3 ? 0.5 * Long * (Q - 1) : 80.0 * std::sqrt(Mix(97000 + Q)), A = Q < 3 ? Bear : TwoPi * Mix(98000 + Q);
			Offs[Q][0] = R * std::sin(A);
			Offs[Q][1] = R * std::cos(A);
		}
		for (int K = 1; K <= N; ++K)
		{
			Place(K, Eye, Sub);
			const Drawn Now = DrawnFor(S, Eye, Eye, 60.0, Sub, Was.SeaT, &Was, false, Was.Rings, Was.N, 1077.0, Long, Dt);
			double Change = 0.0, Moving = 0.0;
			// (and the part of it that is the patch and the rings moving alone: this frame's patch at the last frame's presence)
			Drawn Held = Now;
			if (Held.Patch.bOn && Was.Patch.bOn && Held.Patch.Cell == Was.Patch.Cell && Held.Patch.Full == Was.Patch.Full) Held.Patch.Presence = Was.Patch.Presence;
			for (const auto& O : Offs)
			{
				const double Before = DrawnMeshZ(S, Was, Sub[0] + O[0], Sub[1] + O[1]);
				Change = std::max(Change, std::fabs(DrawnMeshZ(S, Now, Sub[0] + O[0], Sub[1] + O[1]) - Before));
				Moving = std::max(Moving, std::fabs(DrawnMeshZ(S, Held, Sub[0] + O[0], Sub[1] + O[1]) - Before));
			}
			W.Worst = std::max(W.Worst, Change);
			W.WorstMoving = std::max(W.WorstMoving, Moving);
			++W.Frames;
			W.Shown += Now.Patch.bOn && Now.Patch.Presence > 0.0 ? 1 : 0;
			W.Forced += Now.Patch.bForced ? 1 : 0;
			// (a switch: on, off, or another cell or reach; a snap of its middle is no switch: its points stay on its cell's grid)
			const bool bSwitch = Now.Patch.bOn != Was.Patch.bOn || (Now.Patch.bOn && (Now.Patch.Cell != Was.Patch.Cell || Now.Patch.Full != Was.Patch.Full));
			const bool bFade = Now.Patch.bOn && Was.Patch.bOn && Now.Patch.Presence != Was.Patch.Presence;
			if (bFade)
			{
				++W.Fades;
				W.WorstFade = std::max(W.WorstFade, Change);
			}
			if (bSwitch)
			{
				++W.Switches;
				W.WorstAtSwitch = std::max(W.WorstAtSwitch, Change);
				// (made where it is not shown: the last frame's patch off, or reaching nought on this frame (a fade step from it at
				// most), the new one at nought or a fade step in; and at nought each is the main mesh's own surface)
				const bool bWasHid = !Was.Patch.bOn || Was.Patch.Presence <= Dt / PatchFade + 1e-12;
				const bool bNowHid = !Now.Patch.bOn || Now.Patch.Presence <= Dt / PatchFade + 1e-12;
				if (!bWasHid || !bNowHid)
				{
					++W.Late;
					if (W.LateAt.size() < 300)
						W.LateAt += "k " + std::to_string(K) + ": " + (Was.Patch.bOn ? Num(Was.Patch.Cell, 0) + " m/" + Num(Was.Patch.Full, 1) + " at " + Num(Was.Patch.Presence, 3) : std::string("off")) +
							" to " + (Now.Patch.bOn ? Num(Now.Patch.Cell, 0) + " m/" + Num(Now.Patch.Full, 1) + " at " + Num(Now.Patch.Presence, 3) : std::string("off")) + "; ";
				}
				for (const Drawn* F : { static_cast<const Drawn*>(&Was), &Now })
				{
					if (!F->Patch.bOn) continue;
					Drawn Hid = *F;
					Hid.Patch.Presence = 0.0;
					for (const auto& O : Offs)
						W.SwitchSurface = std::max(W.SwitchSurface, std::fabs(DrawnMeshZ(S, Hid, Sub[0] + O[0], Sub[1] + O[1]) - MainMeshZ(S, Hid, Sub[0] + O[0], Sub[1] + O[1])));
				}
			}
			Was = Now;
		}
		return W;
	}

	void PatchWalks()
	{
		// (hulls of 100, 180 and 250 m walked out from 50 m to 3 km and back in, and the eye walked away from each, on held
		// frames at 13.8 m/s (its trains 66 to 280 m) from a 14 m eye; at 1 m steps, at 0.5 m and at 0.1 m (the time each in
		// step, at 10 m/s))
		const Sea S = SeaOf(13.8, 301, false, 0.0, 12.0);
		int Walks = 0, Switches = 0, Forced = 0, Late = 0, Fades = 0;
		double WorstSurface = 0.0, WorstRatio = 0.0, WorstFine = 0.0, WorstCoarse = 0.0, WorstSwitchFine = 0.0, WorstFadeFine = 0.0, MovingFine = 0.0, MovingCoarse = 0.0;
		long long Frames = 0, Shown = 0;
		std::string Each, LateAt;
		for (double Long : { 100.0, 180.0, 250.0 })
		{
			for (int Way = 0; Way < 3; ++Way)
			{
				const bool bEye = Way == 2, bOut = Way != 1;
				const WalkOut Coarse = HeldWalk(S, Long, 14.0, bEye, bOut, 1.0), Half = HeldWalk(S, Long, 14.0, bEye, bOut, 0.5),
							  Fine = HeldWalk(S, Long, 14.0, bEye, bOut, 0.1);
				++Walks;
				for (const WalkOut* W : { &Coarse, &Half, &Fine })
				{
					Switches += W->Switches;
					Forced += W->Forced;
					Late += W->Late;
					if (!W->LateAt.empty() && LateAt.size() < 600) LateAt += Num(Long, 0) + " m " + (bEye ? "eye away" : (bOut ? "out" : "in")) + ": " + W->LateAt;
					Fades += W->Fades;
					WorstSurface = std::max(WorstSurface, W->SwitchSurface);
				}
				Frames += Coarse.Frames;
				Shown += Coarse.Shown;
				const double Ratio = Fine.Worst / std::max(Coarse.Worst, 1e-9);
				WorstRatio = std::max(WorstRatio, Ratio);
				WorstFine = std::max(WorstFine, Fine.Worst);
				WorstCoarse = std::max(WorstCoarse, Coarse.Worst);
				WorstSwitchFine = std::max(WorstSwitchFine, Fine.WorstAtSwitch);
				WorstFadeFine = std::max(WorstFadeFine, Fine.WorstFade);
				MovingFine = std::max(MovingFine, Fine.WorstMoving);
				MovingCoarse = std::max(MovingCoarse, Coarse.WorstMoving);
				Each += Num(Long, 0) + " m " + (bEye ? "eye away" : (bOut ? "out" : "in")) + ": " + std::to_string(Coarse.Switches) + "/" + std::to_string(Half.Switches) + "/" +
					std::to_string(Fine.Switches) + " switches, " + Num(1000 * Coarse.Worst, 1) + " / " + Num(1000 * Half.Worst, 1) + " / " + Num(1000 * Fine.Worst, 1) + " mm; ";
			}
		}
		Check("patch walks: on held frames (1, 0.5 and 0.1 m a frame, the sea clock held) no switch is forced (the patch's square always lasts its fade out: her "
			  "place and the eye's PatchFade ahead and a snap more)", Forced == 0 && Switches > 0, std::to_string(Forced) + " forced of " + std::to_string(Switches) + " switches over " + std::to_string(Walks) + " walks");
		Check("patch walks: every on, off and cell change is made at presence 0, where the patch is not shown (the last frame's off or fading to nought on this "
			  "frame, a fade step from it at most; the new one at nought or a fade step in), and there the patch is the main mesh's own surface (within 1 mm)",
			Late == 0 && WorstSurface < 0.001, std::to_string(Late) + " late of " + std::to_string(Switches) + "; the patch at nought within " + Num(1000 * WorstSurface, 4) +
				" mm of the main mesh" + (LateAt.empty() ? "" : "; " + LateAt));
		Check("patch walks: moving her or the eye never pops (each walk again at 0.1 m steps, a tenth of the time: the worst change at fixed points round her, "
			  "switches and fades included, at most a fifth of the 1 m steps')",
			WorstRatio <= 0.2, "fine over coarse at most " + Num(WorstRatio, 3) + "; worst " + Num(1000 * WorstCoarse, 1) + " mm a 1 m step, " + Num(1000 * WorstFine, 1) +
				" mm a 0.1 m step (at a switch " + Num(1000 * WorstSwitchFine, 2) + " mm, fading " + Num(1000 * WorstFadeFine, 2) + " mm; the patch and rings moving alone, at a held "
				"presence, " + Num(1000 * MovingCoarse, 1) + " mm a 1 m step, " + Num(1000 * MovingFine, 1) + " mm a 0.1 m step); " + std::to_string(Fades) +
				" fade steps; the patch shown in " + std::to_string(Shown) + " of " + std::to_string(Frames) + " 1 m frames; " + Each);
	}

	void NearWorld()
	{
		// a Near made as it stands reads 0 (no build yet: the sea is made lazily, and the frame guard may cut BuildNear)
		{
			const Near Fresh{};
			bool bZero = true;
			for (int K = 0; K < 64; ++K)
			{
				const double X = (Mix(99000 + K) - 0.5) * 1e5, Y = (Mix(99100 + K) - 0.5) * 1e5;
				double Sx = 1.0, Sy = 1.0;
				NearShift(Fresh, X, Y, Sx, Sy);
				bZero = bZero && SampleNear(Fresh, X, Y) == 0.0 && SampleNearRest(Fresh, X, Y) == 0.0 && Sx == 0.0 && Sy == 0.0;
			}
			Check("SeaNear: a Near not yet built reads 0 everywhere, its shift nought (its levels made as they stand are BuildNear's empty one)", bZero);
		}
		// DrawnSurfaceZ: every drawn point lies on it, found from its place back to its rest
		{
			const Sea S = SeaOf(13.8, 142, false, 2.5, 12.0);
			const double Sub[2] = { 3210.0 + 450.0, -987.0 + 225.0 };
			const Drawn D = FrameOf(S, 3210.0, -987.0, 14.0, 222.0, Sub);
			const Mesh G = Grid(D.N, D.Rings), Pm = Patch();
			double WorstZ = 0.0, WorstRest = 0.0;
			int Points = 0;
			for (int K = 0; K < 1500; ++K)
			{
				const bool bPatch = D.Patch.bOn && K % 3 == 0;
				const Mesh& From = bPatch ? Pm : G;
				const GridPoint& Gp = From.Points[static_cast<size_t>(Mix(99200 + K) * From.Points.size())];
				if (Gp.Kind == 6) continue;
				const Placed P = PlaceOf(S, D, Gp);
				if (P.bCollapsed || P.bDropped || P.bNotch) continue;
				double Rest[2] = { 0.0, 0.0 };
				const double Z = DrawnSurfaceZ(S, D, P.X, P.Y, Rest);
				WorstZ = std::max(WorstZ, std::fabs(Z - P.Z));
				WorstRest = std::max(WorstRest, std::hypot(Rest[0] - P.RestX, Rest[1] - P.RestY));
				++Points;
			}
			Check("DrawnSurfaceZ: every drawn point (rings, trims, the patch and its edge) lies on it, its rest found again from its place (13.8 m/s, a 2.5 m swell)",
				Points > 800 && WorstZ < 1e-5 && WorstRest < 1e-4,
				std::to_string(Points) + " points, height within " + Sci(WorstZ) + " m, rest within " + Sci(WorstRest) + " m");
		}
		// SampleNear at world points against the drawn surface there
		{
			double Worst = 0.0, Sum = 0.0, WorstRestRead = 0.0;
			long long N = 0;
			std::string Each;
			struct Case
			{
				double U, SwellH, Ez, Off;
			};
			const Case Cases[] = { { 8.0, 0.0, 14.0, 0.75 }, { 8.0, 1.8, 28.0, 1.5 }, { 13.8, 0.0, 14.0, 0.75 }, { 13.8, 2.5, 6.0, 0.75 }, { 13.8, 2.5, 28.0, 1.5 },
				{ 13.8, 2.5, 55.0, 0.0 } };
			for (const Case& C : Cases)
			{
				const Sea S = SeaOf(C.U, 143, false, C.SwellH, 12.0);
				const double Sub[2] = { 3210.0 + 600.0 * C.Off, -987.0 + 300.0 * C.Off };
				const Drawn D = FrameOf(S, 3210.0, -987.0, C.Ez, 222.0, C.Off > 0.0 ? Sub : nullptr);
				Near Nr;
				BuildNear(S, D, Nr);
				double WorstHere = 0.0;
				for (int K = 0; K < 1500; ++K)
				{
					const double R = std::pow(Mix(99300 + K), 2.0) * 3000.0;
					double X = D.EyeX + (2.0 * Mix(99400 + K) - 1.0) * R, Y = D.EyeY + (2.0 * Mix(99500 + K) - 1.0) * R;
					if (D.Patch.bOn && K % 4 == 0)
					{
						X = D.Patch.X + (2.0 * Mix(99600 + K) - 1.0) * D.Patch.Half;
						Y = D.Patch.Y + (2.0 * Mix(99700 + K) - 1.0) * D.Patch.Half;
					}
					const double Truth = DrawnSurfaceZ(S, D, X, Y);
					const double E = std::fabs(SampleNear(Nr, X, Y) - Truth);
					WorstHere = std::max(WorstHere, E);
					Sum += E * E;
					++N;
					WorstRestRead = std::max(WorstRestRead, std::fabs(SampleNearRest(Nr, X, Y) - Truth));
				}
				Worst = std::max(Worst, WorstHere);
				Each += Num(C.U, 1) + " m/s" + (C.SwellH > 0.0 ? " and swell" : "") + ", eye " + Num(C.Ez, 0) + " m" + (D.Patch.bOn ? ", " + Num(D.Patch.Cell, 0) + " m patch" : "") +
					": " + Num(1000 * WorstHere, 1) + " mm; ";
			}
			Check("SeaNear: SampleNear at a world point within 5 cm of DrawnSurfaceZ, the drawn surface over it (8 and 13.8 m/s, with and without a swell, eyes 6 "
				  "to 55 m, over the subject's patch and out to 3 km)",
				Worst <= 0.05, std::to_string(N) + " points, worst " + Num(1000 * Worst, 1) + " mm, rms " + Num(1000 * std::sqrt(Sum / std::max<long long>(N, 1)), 2) +
					" mm (the rest point's height there is off by up to " + Num(1000 * WorstRestRead, 0) + " mm); " + Each);
		}
	}

	void FoamDepths()
	{
		const double PxPerRad = 1077.0 / (2.0 * std::tan(30.0 * Pi / 180.0)), U = 100.0;
		// (the chip's whitecaps against FoamDrawn at every depth, 3 m to 8 km)
		double WorstChip = 0.0;
		long long Pixels = 0;
		for (int F = 0; F < 24; ++F)
		{
			const Sea S = SeaOf(3.0 + 21.4 * Mix(99800 + F), 300 + F, true, F % 3 ? 2.0 : 0.0, 12.0);
			const Drawn D = FrameOf(S, 1000.0 * F, -500.0, 20.0, 50.0 + 7.0 * F);
			const ForStage Fs = StageParams(S, D, U);
			Gpu::Inputs In = Gpu::From(Fs, false);
			for (int K = 0; K < 300; ++K)
			{
				const double Depth = 3.0 * std::pow(8000.0 / 3.0, Mix(99900 + 300 * F + K));
				const double X = D.EyeX + (Mix(99950 + 300 * F + K) - 0.5) * 4000.0, Y = D.EyeY + (Mix(99970 + 300 * F + K) - 0.5) * 4000.0;
				In.Local = Gpu::float2(static_cast<float>((Y - D.OriginY) * U), static_cast<float>((X - D.OriginX) * U));
				In.Depth = static_cast<float>(Depth * U);
				In.Mode = 3.0f;
				const Gpu::float4 O = Gpu::Run(In);
				const Foam Fm = FoamDrawn(S, X, Y, D.SeaT, Depth, D.PxPerRad);
				WorstChip = std::max({ WorstChip, std::fabs(O.x - Fm.Head), std::fabs(O.y - Fm.Tail) });
				++Pixels;
			}
		}
		Check("chip: the whitecaps' heads and tails as FoamDrawn's at every depth, 3 m to 8 km (the gate from every train at full weight, going to its mean as "
			  "the shortest train fades under the pixel) (0.02)",
			WorstChip <= 0.02, std::to_string(Pixels) + " pixels, worst " + Sci(WorstChip));
		// (the drawn cover at depth: heads times the whitecaps' picture, and tails, as FoamDrawn draws them, against Monahan)
		const int CapsSize = 512;
		const std::vector<uint8_t> Caps = SeaFace::MakeFoam(CapsSize, 160.0);
		const double Depths[] = { 50.0, 100.0, 200.0, 300.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 };
		constexpr int ND = sizeof(Depths) / sizeof(Depths[0]);
		double WorstCover = 0.0;
		std::string Covers;
		for (int B = 3; B <= 7; ++B)
		{
			const double Wind = 0.836 * std::pow(B, 1.5);
			const Sea S = SeaOf(Wind, 4242, true, 0.0, 0.0, 0.9);
			double Fades[ND], Cover[ND] = {};
			for (int Q = 0; Q < ND; ++Q) Fades[Q] = FoamFade(S, Depths[Q], PxPerRad);
			long long N = 0;
			for (int Jy = 0; Jy <= 700; ++Jy)
			{
				for (int I = 0; I <= 700; ++I)
				{
					const double X = -700.0 + I * 2.0 + 0.37, Y = -700.0 + Jy * 2.0 + 0.71;
					const Foam F = FoamAt(S, X, Y, 123.4);
					const double Shape = HeadShapeAt(S, Caps, CapsSize, X, Y, 123.4);
					for (int Q = 0; Q < ND; ++Q)
					{
						const double Head = S.FarHead + (F.Head - S.FarHead) * Fades[Q];
						Cover[Q] += std::max(Head * Shape, F.Tail * Fades[Q]);
					}
					++N;
				}
			}
			const double Want = MonahanOf(S.WindU);
			Covers += "B" + std::to_string(B) + " (" + Num(100 * Want, 3) + "%):";
			for (int Q = 0; Q < ND; ++Q)
			{
				const double C = Cover[Q] / static_cast<double>(N);
				WorstCover = std::max(WorstCover, std::fabs(C / Want - 1.0));
				Covers += " " + Num(100 * C, 3);
			}
			Covers += "; ";
		}
		Check("whitecaps: the drawn cover at depth (heads, the gate going to FarHead as the shortest train fades under the pixel, times the whitecaps' picture; "
			  "and tails) within 25% of Monahan from 50 m to 8 km, Beaufort 3 to 7 (60 deg lens, 1077 px)",
			WorstCover <= 0.25, "worst " + Num(100 * WorstCover, 1) + "%; % at 50, 100, 200, 300, 500 m, 1, 2, 4 and 8 km: " + Covers);
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: sea_waves_test <Source/TheaterCommand> <out dir> [recording.json ...]\n");
		return 2;
	}
	const std::string Src = argv[1], Out = argv[2];
	std::vector<std::string> Recordings;
	for (int K = 3; K < argc; ++K) Recordings.push_back(argv[K]);
	const auto T0 = Clock::now();
	struct Part
	{
		const char* Name;
		std::function<void()> Run;
	};
	const Part Parts[] = {
		{ "spectrum", [] { Spectra(); } },
		{ "pairs", [] { Pairs(); } },
		{ "seeds", [&] { Seeds(Src); } },
		{ "folding and whitecaps", [] { Folding(); } },
		{ "slopes", [] { SlopeBudget(); } },
		{ "road", [] { Road(); } },
		{ "wraps", [] { Wraps(); } },
		{ "weights", [] { Weights(); } },
		{ "grid", [] { GridChecks(); } },
		{ "snaps", [] { Snaps(); } },
		{ "patch", [] { PatchChecks(); } },
		{ "patch reach", [] { PatchReach(); } },
		{ "patch sweep", [] { PatchSweep(); } },
		{ "patch walks", [] { PatchWalks(); } },
		{ "chord", [] { Chords(); } },
		{ "references", [] { References(); } },
		{ "SeaNear", [] { SeaNear(); } },
		{ "SeaNear world", [] { NearWorld(); } },
		{ "chip", [] { Chip(); } },
		{ "chip scan", [&] { TurnsScan(Src); } },
		{ "foam depths", [] { FoamDepths(); } },
		{ "params", [&] { Params(Out); } },
		{ "lanes", [] { Lanes(); } },
		{ "ride", [&] { Rides(Recordings); } },
	};
	const char* Only = std::getenv("SEA_WAVES_ONLY"); // (a list of parts, to run alone while working on one)
	for (const Part& P : Parts)
	{
		if (Only && *Only && std::string(",") .append(Only).append(",").find(std::string(",") + P.Name + ",") == std::string::npos) continue;
		const auto P0 = Clock::now();
		std::printf("-- %s\n", P.Name);
		P.Run();
		std::printf("   (%.1f s)\n", Since(P0));
		std::fflush(stdout);
	}
	std::printf("sea_waves_test: %d checks, %d failed (%.0f s)\n", Checks, Failed, Since(T0));
	return Failed == 0 ? 0 : 1;
}
