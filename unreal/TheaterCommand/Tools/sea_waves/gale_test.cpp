// THE GALE'S CLOUD TEST (the release 2 ocean plan, step 14(c): "-TheaterSeaGale", an extra behind its own switch, off by
// default). It holds the gale to the plan and to the sea itself:
//  - the spectrum to 24.4 m/s with J above 0.15 (the plan's named test for this step), over winds from the cap to the
//    gale's and over seeds, with and without a swell;
//  - the sea's height and periods against the World Meteorological Organization's Beaufort 7, 8 and 9 (WMO-No. 558),
//    fetch-limited (FleetWaves::GaleFetchOf, CERC's duration-limited fetch) and fully raised (Pierson-Moskowitz);
//  - the whitecaps' cover against Monahan's at Beaufort 8 and 9 (the fold table, extended to gale winds in its own rows
//    without moving the bytes of the rows below);
//  - SeaFace::MakeStreaks: the streaks along the wind, no seams, the share by wind (WMO's "in streaks", "well-marked",
//    "dense"), and deterministic;
//  - FleetRideGale's limits (data the step 5 ride will read, never ride code): the art director's own table read again
//    from SeaShow.cpp, the limits equal to it at every wind to the 13.8 m/s cap, the rule that makes the gale's steps
//    above it, and no heave limit;
//  - the freeboard tests at Beaufort 7, 8 and 9 and below them: the sea the scene draws against the deck heights of
//    destroyers, corvettes, frigates and surfaced submarines (the ship models' own decks, Content/Models/Ships, held by
//    gale_decks.py; a boat at the trim the convoy looks draw her at), at four headings to the sea and two speeds, with the
//    ride as step 5 lays it, stating how often green water reaches each deck (each instant once), pinned as measured;
//  - the wake lane's blue as each hull's footprint (FleetWaves::PaintLanesGale), red and green byte for byte PaintLanes';
//  - a source scan: FleetRideGale.h holds data alone, the gale's own functions are new, and nothing the show decides
//    reads any of it.
// Below 13.8 m/s nothing is changed: Tools/sea_waves/gale_pin.cpp holds the core's outputs to 942d9295's, byte for byte.
// Plain C++20, no engine. Usage: gale_test <Source/TheaterCommand> <out dir>
#include "Battle/ConvoyLooksCore.h"
#include "Battle/FleetRideGale.h"
#include "Battle/FleetWaves.h"
#include "Battle/SeaFace.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
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

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	using Clock = std::chrono::steady_clock;
	double Since(Clock::time_point T0) { return std::chrono::duration<double>(Clock::now() - T0).count(); }

	/** A sea raised by U under the gale's cap, with the fetch the gale gives it (0: fully raised, Pierson-Moskowitz). */
	Sea GaleSea(double U, uint64_t Seed, bool bFetch, double SwellH = 0.0, double SwellT = 0.0, double Down = 0.6)
	{
		Asked A;
		A.WindU = U;
		A.DownwindR = Down;
		A.Seed = Seed;
		A.Cap = GaleCap;
		A.SwellH = SwellH;
		A.SwellT = SwellT;
		A.SwellFromR = Down + Pi + 0.4;
		A.Fetch = bFetch ? GaleFetchOf(U) : 0.0;
		return Make(A);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The spectrum to the gale's cap.

	void Spectrum24()
	{
		double LeastJ = 9.0, WorstLean = 0.0;
		std::string Each;
		int MostD = 0, Seas = 0, Gridded = 0;
		double ShortestDisplaced = 1e9;
		// J over a 1 km square at 2 m, every wind from the cap to the gale's, three seeds, with a swell and without
		for (double U = 14.0; U <= GaleCap + 1e-9; U = std::min(U + 1.0, U < GaleCap - 1e-9 ? GaleCap : GaleCap + 1.0))
		{
			double LeastHere = 9.0;
			for (uint64_t Seed : { 7ULL, 239ULL, 4242ULL })
			{
				for (int Swell = 0; Swell < 2; ++Swell)
				{
					for (int Fetch = 0; Fetch < 2; ++Fetch)
					{
						const Sea S = GaleSea(U, Seed, Fetch == 1, Swell ? 4.0 : 0.0, Swell ? 14.0 : 0.0, 0.2 * static_cast<double>(Seed % 11));
						++Seas;
						MostD = std::max(MostD, S.ND);
						double Lean = 0.0;
						for (int J = 0; J < S.ND; ++J)
						{
							Lean += S.D[J].Q * S.D[J].K * S.D[J].A;
							if (S.D[J].A > 0.0) ShortestDisplaced = std::min(ShortestDisplaced, S.D[J].L);
						}
						for (int J = 0; J < S.NP; ++J) Lean += S.P[J].Q * S.P[J].K * S.P[J].A;
						WorstLean = std::max(WorstLean, Lean - S.Qb);
						// (every sea's J over the square, one instant each: the seeds take turns at three)
						const double T = Seed == 7 ? 61.3 : (Seed == 239 ? 517.7 : 1234.5);
						for (int Jy = 0; Jy <= 500; ++Jy)
						{
							for (int I = 0; I <= 500; ++I)
							{
								const double X = -500.0 + I * 2.0 + 0.37, Y = -500.0 + Jy * 2.0 + 0.71;
								LeastHere = std::min(LeastHere, SlopesAt(S, X, Y, T).J);
							}
						}
						++Gridded;
					}
				}
			}
			LeastJ = std::min(LeastJ, LeastHere);
			Each += Num(U, 1) + " m/s " + Num(LeastHere, 3) + "; ";
			if (U >= GaleCap) break;
		}
		Check("the gale's spectrum to 24.4 m/s: the least J over a 1 km square at 2 m is above 0.15 (the plan's named test for step 14(c); every wind from 14 m/s "
			  "to the cap, three seeds, fetch-limited and fully raised, with a 4 m 14 s swell and without)",
			LeastJ > 0.15 && Gridded == Seas, "least " + Num(LeastJ, 4) + " over " + std::to_string(Gridded) + " gale seas of " + std::to_string(Seas) + ": " + Each);
		Check("the gale's spectrum: sum(Q k a) over every train is still at most Qb, and no displaced train is shorter than 8 m, at gale winds",
			WorstLean <= 1e-12 && MostD <= MostDrawn && ShortestDisplaced >= ShortestDrawn,
			"worst over " + Num(WorstLean * 1e15, 3) + "e-15, " + std::to_string(MostD) + " trains at most, shortest displaced " + Num(ShortestDisplaced, 2) + " m");
	}

	void WmoHeights()
	{
		// WMO's rows: the gale's own sea (the fetch a gale of GaleHours raises over) carries each row's probable height at
		// the row's foot and rises toward the row's probable most by its head, never past it; the sea the wind has fully
		// raised carries the probable most at the row's middle instead.
		double WorstFoot = 0.0, WorstMost = 0.0, OverMost = 0.0;
		std::string Rows;
		bool bInRange = true;
		for (const BeaufortSea& R : FleetWaves::WmoGale)
		{
			const Sea Foot = GaleSea(R.From, 4242, true), Head = GaleSea(R.To, 4242, true);
			const Sea Mid = GaleSea(0.5 * (R.From + R.To), 4242, false);
			WorstFoot = std::max(WorstFoot, std::fabs(Foot.Hs / R.Probable - 1.0));
			WorstMost = std::max(WorstMost, std::fabs(Mid.Hs / R.Most - 1.0));
			OverMost = std::max(OverMost, Head.Hs / R.Most);
			// (every wind of the row: the gale's sea between the row's probable (less 5%) and its probable most)
			for (double U = R.From; U <= R.To + 1e-9; U += 0.1)
			{
				const Sea S = GaleSea(U, 4242, true);
				bInRange = bInRange && S.Hs >= 0.95 * R.Probable && S.Hs <= R.Most + 1e-9 && S.Hs >= Foot.Hs - 1e-9;
			}
			Rows += "B" + std::to_string(R.Force) + " (" + Num(R.From, 1) + "-" + Num(R.To, 1) + " m/s; WMO probable " + Num(R.Probable, 1) + " m, most " +
				Num(R.Most, 1) + " m): the gale's sea " + Num(Foot.Hs, 2) + " m Tp " + Num(Foot.Tp, 1) + " s at its foot (fetch " +
				Num(GaleFetchOf(R.From) / 1000.0, 0) + " km) to " + Num(Head.Hs, 2) + " m Tp " + Num(Head.Tp, 1) + " s at its head; fully raised at its middle " +
				Num(Mid.Hs, 2) + " m Tp " + Num(Mid.Tp, 1) + " s.  ";
		}
		Check("WMO's Beaufort 7, 8 and 9 (WMO-No. 558): the gale's own sea (duration-limited, a gale of " + Num(GaleHours, 1) +
				  " hours, GaleFetchOf) carries each row's probable height at the row's foot, within 5%",
			WorstFoot <= 0.05, "worst " + Num(100 * WorstFoot, 1) + "%");
		Check("WMO's Beaufort 7, 8 and 9: across each row the gale's sea rises from the row's probable height toward its probable most and never passes it", bInRange,
			"the head of each row reaches " + Num(100 * OverMost, 0) + "% of its most at most");
		Check("WMO's Beaufort 7, 8 and 9: the sea the wind has fully raised (no fetch, Pierson-Moskowitz, the show's 0.021 U^2) carries the row's probable most at "
			  "the row's middle, within 10% (a gale of two days over 1,000 km of sea)",
			WorstMost <= 0.10, "worst " + Num(100 * WorstMost, 1) + "%");
		Note("the sea by Beaufort: " + Rows);
		// The height and the peak period grow with the wind across the cap, and the gale's sea is never lower than the cap's own
		double Last = 0.0, LastTp = 0.0, StepTp = 0.0;
		bool bUp = true, bFloor = true;
		const Sea Cap = GaleSea(Raised, 4242, true);
		const double CapHs = Cap.Hs;
		std::string Worst;
		for (double U = 13.0; U <= GaleCap + 1e-9; U += 0.1)
		{
			const Sea S = GaleSea(U, 4242, true);
			// (the cap itself is the one step: there the show's own fully raised sea gives way to the gale's younger,
			// duration-limited one, which is steeper, so its peak period is a little shorter; its height is the same)
			const bool bAtCap = U > Raised && U <= Raised + 0.15;
			if (bAtCap) StepTp = LastTp > 0.0 ? 1.0 - S.Tp / LastTp : 0.0;
			if (S.Hs < Last - 1e-9 || (S.Tp < LastTp - 1e-9 && !bAtCap)) Worst += Num(U, 1) + " m/s (" + Num(S.Hs, 3) + " m, Tp " + Num(S.Tp, 2) + "); ";
			bUp = bUp && S.Hs >= Last - 1e-9 && (S.Tp >= LastTp - 1e-9 || bAtCap);
			bFloor = bFloor && (U <= Raised || S.Hs >= CapHs - 1e-9);
			Last = S.Hs;
			LastTp = S.Tp;
		}
		Check("the gale's sea grows with the wind across the 13.8 m/s cap and is never lower than the cap's own: its height rises everywhere, and its peak period "
			  "rises above the cap, stepping down by under 2% at the cap itself, where the show's fully raised sea gives way to the gale's younger, steeper one "
			  "(the duration-limited sea's height grows as U^1.25, its peak period as U^0.5)",
			bUp && bFloor && StepTp < 0.02, Worst.empty() ? "13.8 m/s: " + Num(CapHs, 2) + " m, Tp " + Num(Cap.Tp, 2) + " s; the step at the cap " +
					Num(100 * StepTp, 2) + "% of the period; 24.4 m/s: " + Num(Last, 2) + " m, Tp " + Num(LastTp, 1) + " s"
															: "falls at " + Worst);
		// (GaleFetchOf gives no fetch at the cap itself, so the cap's sea is the show's by construction; what holds the two seas
		// together is GaleHours: the duration-limited law's own sea at 13.8 m/s, and the gale's just over it)
		double CapLaw = 0.0, CapW = 0.0;
		FetchLaw(Raised, FetchOfHours(Raised, GaleHours), CapLaw, CapW);
		const Sea JustOver = GaleSea(Raised + 1e-6, 4242, true);
		const double Show = 0.021 * Raised * Raised;
		Check("the gale's sea meets the show's at the cap (0.021 x 13.8^2 = 4.00 m), so nothing jumps when the switch raises it: the duration-limited sea of a gale of " +
				  Num(GaleHours, 1) + " hours at 13.8 m/s, and the gale's sea a micrometre a second over it, within 0.5% of the show's",
			std::fabs(CapLaw / Show - 1.0) <= 0.005 && std::fabs(JustOver.Hs / Show - 1.0) <= 0.005,
			"the law's " + Num(CapLaw, 4) + " m (fetch " + Num(FetchOfHours(Raised, GaleHours) / 1000.0, 1) + " km), just over the cap " + Num(JustOver.Hs, 4) + " m, the show's " +
				Num(Show, 4) + " m");
		// The periods: the fetch-limited sea's peak against JONSWAP's own law
		double WorstTp = 0.0;
		for (double U = 14.0; U <= GaleCap + 1e-9; U += 0.5)
		{
			double Hj = 0.0, Wj = 0.0;
			FetchLaw(U, GaleFetchOf(U), Hj, Wj);
			const Sea S = GaleSea(U, 4242, true);
			WorstTp = std::max(WorstTp, std::fabs(S.Tp / (TwoPi / Wj) - 1.0));
		}
		Check("the gale's periods: the fetch-limited sea's peak period is JONSWAP's fetch law's within 5% (Make takes the higher of its peak frequency and the fully "
			  "raised sea's, so a younger sea's peak is never longer than a fully raised one's)",
			WorstTp <= 0.05, "worst " + Num(100 * WorstTp, 2) + "%");
	}

	void Caps()
	{
		// The whitecaps' cover at gale winds, as step 7 draws it (the gate times the whitecaps' picture, and the lagged
		// tails), against Monahan's: the fold table's own gale rows
		const int CapsSize = 512;
		const std::vector<uint8_t> Picture = SeaFace::MakeFoam(CapsSize, 160.0);
		double Worst = 0.0;
		std::string Covers;
		for (int B = 8; B <= 9; ++B)
		{
			const double U = 0.836 * std::pow(B, 1.5);
			const Sea S = GaleSea(U, 239, false, 0.0, 0.0, 0.9);
			double Cover = 0.0;
			long long N = 0;
			for (int Jy = 0; Jy <= 700; ++Jy)
			{
				for (int I = 0; I <= 700; ++I)
				{
					const double X = -700.0 + I * 2.0 + 0.37, Y = -700.0 + Jy * 2.0 + 0.71;
					const Foam F = FoamAt(S, X, Y, 123.4);
					Cover += std::max(F.Head > 0.0 ? F.Head * HeadShapeAt(S, Picture, CapsSize, X, Y, 123.4) : 0.0, F.Tail);
					++N;
				}
			}
			Cover /= static_cast<double>(N);
			const double Want = MonahanOf(S.WindU);
			Worst = std::max(Worst, std::fabs(Cover / Want - 1.0));
			Covers += "B" + std::to_string(B) + " (" + Num(U, 1) + " m/s) " + Num(100 * Cover, 2) + "% against Monahan's " + Num(100 * Want, 2) + "%; ";
		}
		Check("whitecaps: the drawn cover at Beaufort 8 and 9 is within 25% of Monahan's 3.84e-6 U^3.41 (the fold table's gale rows; the rows below 13.8 m/s are "
			  "byte for byte as they were)",
			Worst <= 0.25, "worst " + Num(100 * Worst, 1) + "%: " + Covers);
		// Fold and FoldSoft rise smoothly over the gale's winds, and FarHead saturates at 1
		bool bSmooth = true;
		double LastFold = 0.0;
		for (double U = 13.8; U <= GaleCap + 1e-9; U += 0.1)
		{
			double Fold = 0.0, Soft = 0.0, Far = 0.0;
			FoldOf(U, Fold, Soft, Far);
			bSmooth = bSmooth && Fold >= LastFold - 1e-9 && Soft > 0.0 && Far >= 0.0 && Far <= 1.0;
			LastFold = Fold;
		}
		Check("whitecaps: the gate rises with the wind to the gale's cap, its softness stays positive and FarHead keeps to 0 to 1", bSmooth,
			"Fold " + Num(LastFold, 4) + " at 24.4 m/s");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The wind's streaks.

	void Streaks(const std::string& Out)
	{
		const int Size = 512;
		const double Tile = 256.0;
		const auto T0 = Clock::now();
		const std::vector<uint8_t> A = SeaFace::MakeStreaks(Size, Tile);
		const double Ms = std::chrono::duration<double, std::milli>(Clock::now() - T0).count();
		const std::vector<uint8_t> B = SeaFace::MakeStreaks(Size, Tile);
		Check("streaks: SeaFace::MakeStreaks is the same picture every time (the same bytes, whatever is made between)", A == B,
			std::to_string(A.size()) + " bytes, made in " + Num(Ms, 0) + " ms");
		double Cover = 0.0;
		for (size_t P = 0; P < A.size(); P += 4) Cover += A[P] / 255.0;
		Cover /= A.size() / 4;
		Check("streaks: the foam covers 2 to 12% of the picture (every streak at once, before the wind's share picks them)", Cover > 0.02 && Cover < 0.12,
			Num(100 * Cover, 2) + "%");
		// No seams: wherever a read between two neighbours takes foam, both hold one streak's rank (as the whitecaps' picture)
		int Seams = 0;
		for (int J = 0; J < Size; ++J)
		{
			for (int I = 0; I < Size; ++I)
			{
				const size_t P = (static_cast<size_t>(J) * Size + I) * 4;
				for (const size_t Q : { (static_cast<size_t>(J) * Size + (I + 1) % Size) * 4, (static_cast<size_t>((J + 1) % Size) * Size + I) * 4 })
				{
					if ((A[P] > 0 || A[Q] > 0) && A[P + 1] != A[Q + 1]) ++Seams;
				}
			}
		}
		Check("streaks: no seam where a read mixes two streaks' ranks (each streak's rank carried out round it, as the whitecaps' picture's time is)", Seams == 0,
			std::to_string(Seams));
		// Along the wind: a streak is far longer along u than across v. The runs of foam along each row, against across.
		double AlongMean = 0.0, AcrossMean = 0.0;
		int AlongRuns = 0, AcrossRuns = 0;
		for (int Way = 0; Way < 2; ++Way)
		{
			for (int Line = 0; Line < Size; ++Line)
			{
				int Run = 0;
				for (int K = 0; K <= Size; ++K)
				{
					const int I = Way == 0 ? K % Size : Line, J = Way == 0 ? Line : K % Size;
					const bool bFoam = K < Size && A[(static_cast<size_t>(J) * Size + I) * 4] > 40;
					if (bFoam)
					{
						++Run;
					}
					else if (Run > 0)
					{
						if (Way == 0)
						{
							AlongMean += Run;
							++AlongRuns;
						}
						else
						{
							AcrossMean += Run;
							++AcrossRuns;
						}
						Run = 0;
					}
				}
			}
		}
		AlongMean = AlongRuns ? AlongMean / AlongRuns * Tile / Size : 0.0;
		AcrossMean = AcrossRuns ? AcrossMean / AcrossRuns * Tile / Size : 0.0;
		Check("streaks: the foam runs along the wind, not across it (a run of foam along u against one across v, metres)", AlongMean > 3.0 * AcrossMean,
			"along " + Num(AlongMean, 2) + " m, across " + Num(AcrossMean, 2) + " m");
		// The share by wind: none under Beaufort 6's foot, WMO's "in streaks" at 7, "well-marked" at 8, "dense" at 9
		const double S6 = StreakShareOf(10.0), S65 = StreakShareOf(12.0), S7 = StreakShareOf(13.9), S8 = StreakShareOf(17.2), S9 = StreakShareOf(20.8);
		Check("streaks: the wind's share is nothing below Beaufort 6, faint in its upper half, and rises through WMO's \"in streaks\" (7), \"well-marked\" (8) "
			  "and \"dense\" (9) to 1 at the cap",
			S6 == 0.0 && S65 > 0.0 && S65 < S7 && S7 > 0.1 && S7 < S8 && S8 > 0.3 && S8 < S9 && S9 > 0.6 && StreakShareOf(GaleCap) == 1.0 && StreakShareOf(30.0) == 1.0,
			"10 m/s " + Num(S6, 2) + ", 12 " + Num(S65, 2) + ", 13.9 " + Num(S7, 2) + ", 17.2 " + Num(S8, 2) + ", 20.8 " + Num(S9, 2));
		// The sea drawn at a wind: its cover rises with the share, and a stronger wind keeps a weaker one's streaks
		double Drawn[5] = {};
		const double Winds[5] = { 12.0, 13.9, 17.2, 20.8, GaleCap };
		bool bKeeps = true;
		for (int K = 0; K < 5; ++K)
		{
			const double Share = StreakShareOf(Winds[K]);
			for (size_t P = 0; P < A.size(); P += 4)
			{
				if (A[P + 1] / 255.0 <= Share) Drawn[K] += A[P] / 255.0;
			}
			Drawn[K] /= A.size() / 4;
			bKeeps = bKeeps && (K == 0 || Drawn[K] >= Drawn[K - 1]);
		}
		// (the picture itself, for the art director and the Mac look check, as the air show writes the whitecaps' own)
		{
			std::ofstream File(Out + "/streaks.rgba", std::ios::binary);
			File << "streaks " << Size << " " << Num(Tile, 0) << "\n";
			File.write(reinterpret_cast<const char*>(A.data()), static_cast<std::streamsize>(A.size()));
			Note("the wind's streaks are in " + Out + "/streaks.rgba (" + std::to_string(Size) + " a side over " + Num(Tile, 0) +
				" m, RGBA: foam, rank, how far along, mottle)");
		}
		Check("streaks: the sea's drawn streaks grow with the wind and a stronger wind keeps a weaker one's (the rank under the share)", bKeeps && Drawn[4] > Drawn[0],
			"cover drawn at 12, 13.9, 17.2, 20.8 and 24.4 m/s: " + Num(100 * Drawn[0], 2) + ", " + Num(100 * Drawn[1], 2) + ", " + Num(100 * Drawn[2], 2) + ", " +
				Num(100 * Drawn[3], 2) + ", " + Num(100 * Drawn[4], 2) + "%");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// FleetRideGale: the limits as data.

	/** A row of the art director's table as SeaShow.cpp's own text holds it (RideOf's Small, Middle and Big): roll and pitch
	 * at Beaufort 3, 4, 5 and 6, and the roll period. */
	struct ShowRow
	{
		double Roll[4] = {}, Pitch[4] = {}, RollT = 0.0;
	};
	bool ShowTable(const std::string& Text, ShowRow (&Out)[3])
	{
		static const std::regex Row("static const Ride (Small|Middle|Big) = \\{ \\{ ([^}]*)\\}, \\{ ([^}]*)\\}, ([0-9.]+), ([0-9.]+), ([0-9.]+) \\};");
		int Seen = 0;
		for (std::sregex_iterator It(Text.begin(), Text.end(), Row), End; It != End; ++It)
		{
			const std::string Which = (*It)[1];
			ShowRow& R = Out[Which == "Small" ? 0 : Which == "Middle" ? 1 : 2];
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				std::istringstream In((*It)[2 + Pass].str());
				std::string Each;
				int N = 0;
				while (std::getline(In, Each, ',') && N < 4)
				{
					(Pass == 0 ? R.Roll : R.Pitch)[N++] = std::atof(Each.c_str());
				}
				if (N != 4) return false;
			}
			R.RollT = std::atof((*It)[4].str().c_str());
			++Seen;
		}
		return Seen == 3;
	}
	/** The show's RideOf and AtForce, written here again from SeaShow.cpp's text (not from FleetRideGale.h), over the table
	 * as read: her row by her length, then the row at a Beaufort number. */
	ShowRow ShowRideOf(const ShowRow (&Table)[3], double Long)
	{
		const double K = std::clamp((Long - 110.0) / 140.0, 0.0, 1.0) * 2.0;
		const ShowRow& A = K < 1.0 ? Table[0] : Table[1];
		const ShowRow& B = K < 1.0 ? Table[1] : Table[2];
		const double T = K < 1.0 ? K : K - 1.0;
		ShowRow R;
		for (int I = 0; I < 4; ++I)
		{
			R.Roll[I] = A.Roll[I] + (B.Roll[I] - A.Roll[I]) * T;
			R.Pitch[I] = A.Pitch[I] + (B.Pitch[I] - A.Pitch[I]) * T;
		}
		R.RollT = A.RollT + (B.RollT - A.RollT) * T;
		return R;
	}
	double ShowAtForce(const double (&Row)[4], double Force)
	{
		if (Force <= 3.0) return Row[0] * std::clamp(Force / 3.0, 0.0, 1.0);
		const double K = std::min(Force, 6.0) - 3.0;
		const int I = std::min(static_cast<int>(K), 2);
		return Row[I] + (Row[I + 1] - Row[I]) * (K - I);
	}
	/** The show's Beaufort number of a wind (SeaShow.cpp's Force, unrounded). */
	double ShowForce(double U) { return std::pow(U / 0.836, 2.0 / 3.0); }

	void Limits(const std::string& Src)
	{
		using namespace FleetRideGale;
		const std::string Show = ReadAll(Src + "/Battle/SeaShow.cpp");
		ShowRow Table[3];
		const bool bRead = ShowTable(Show, Table);
		bool bSame = bRead;
		for (int K = 0; K < 3; ++K)
		{
			for (int I = 0; I < 4; ++I) bSame = bSame && Table[K].Roll[I] == TableRoll[K][I] && Table[K].Pitch[I] == TablePitch[K][I];
			bSame = bSame && Table[K].RollT == TableRollT[K];
		}
		Check("the ride's limits: the art director's own table (roll and pitch at Beaufort 3 to 6, and the roll periods), read again from SeaShow.cpp's RideOf rows, "
			  "is the one FleetRideGale.h mirrors (it changes neither the table nor the show's Raised cap of 13.8 m/s)",
			bSame, bRead ? "a destroyer's roll " + Num(Table[0].Roll[0], 2) + ", " + Num(Table[0].Roll[1], 2) + ", " + Num(Table[0].Roll[2], 2) + ", " + Num(Table[0].Roll[3], 2) +
						 "; roll periods " + Num(Table[0].RollT, 1) + ", " + Num(Table[1].RollT, 1) + ", " + Num(Table[2].RollT, 1) + " s"
						   : "SeaShow.cpp's Ride table not read");
		// The show's own formulas, which the mirror and this test's ShowRideOf and ShowAtForce follow, word for word
		const char* Lines[] = { "const double Raised = std::min(WindU, 13.8);", "const double Force = std::pow(WindU / 0.836, 2.0 / 3.0)",
			"const double K = Clamp((Long - 110.0) / 140.0, 0.0, 1.0) * 2.0;", "R.Roll[I] = A.Roll[I] + (B.Roll[I] - A.Roll[I]) * T;",
			"R.RollT = A.RollT + (B.RollT - A.RollT) * T;", "if (Force <= 3.0) return Row[0] * Clamp(Force / 3.0, 0.0, 1.0);",
			"const double K = std::min(Force, 6.0) - 3.0;", "const int I = std::min(static_cast<int>(K), 2);", "return Row[I] + (Row[I + 1] - Row[I]) * (K - I);",
			"const double Swell = AtForce(R.Roll, Force)" };
		std::string Missing;
		for (const char* Line : Lines)
		{
			if (Show.find(Line) == std::string::npos) Missing += std::string(Line) + " ";
		}
		Check("the ride's limits: SeaShow's cap stays 13.8 m/s, its Beaufort number (U / 0.836)^(2/3), RideOf's rows between lengths and AtForce between columns, "
			  "flat over 6, as the mirror has them (the show does not move)",
			Missing.empty(), Missing.empty() ? std::to_string(std::size(Lines)) + " lines of SeaShow.cpp as they were" : "not found: " + Missing);
		// Up to the show's cap the gale's limits are the art director's own, exactly, at every wind and length
		const double CapForce = ShowForce(Raised);
		const double Lengths[] = { 40.0, 62.5, 67.0, 80.0, 91.9, 95.0, 110.0, 114.0, 118.5, 123.0, 145.0, 180.0, 215.0, 250.0, 300.0 };
		double WorstBelow = 0.0, WorstT = 0.0;
		long long Cases = 0;
		for (double Long : Lengths)
		{
			const ShowRow R = ShowRideOf(Table, Long);
			WorstT = std::max(WorstT, std::fabs(RollTAt(Long) - R.RollT));
			for (int K = 0; K <= 1380; ++K)
			{
				const double F = ShowForce(0.01 * K);
				WorstBelow = std::max({ WorstBelow, std::fabs(LimitAt(Long, F, true) - ShowAtForce(R.Roll, F)), std::fabs(LimitAt(Long, F, false) - ShowAtForce(R.Pitch, F)) });
				Cases += 2;
			}
		}
		Check("the ride's limits are the art director's own (AtForce over RideOf's row) at every wind to the 13.8 m/s cap, every hundredth of a metre a second, for "
			  "hulls of 40 to 300 m, and her roll period RideOf's: the gale moves nothing the show's table gives",
			WorstBelow == 0.0 && WorstT == 0.0 && std::fabs(ForceCap - CapForce) <= 1e-12,
			std::to_string(Cases) + " cases, worst " + Num(WorstBelow, 12) + " deg; the roll period within " + Num(WorstT, 12) + " s; the cap at Beaufort " +
				Num(ForceCap, 6) + " against the show's " + Num(CapForce, 6) + " (a destroyer's roll at 10 m/s " + Num(LimitAt(114.0, ShowForce(10.0), true), 2) +
				", at 13.0 and 13.8 m/s " + Num(LimitAt(114.0, ShowForce(13.0), true), 2) + " and " + Num(LimitAt(114.0, CapForce, true), 2) + " deg)");
		// Above it they climb, with no jump at the cap, to the gale's rows at 7, 8 and 9, and hold 9's over it
		double Jump = 0.0, WorstRow = 0.0;
		bool bRises = true, bStops = true, bSmallMore = true;
		for (double Long : Lengths)
		{
			int A = 0;
			double T = 0.0;
			RowsOf(Long, A, T);
			for (int bRoll = 0; bRoll < 2; ++bRoll)
			{
				const double AtCap = LimitAt(Long, CapForce, bRoll == 1);
				Jump = std::max(Jump, std::fabs(LimitAt(Long, CapForce + 1e-9, bRoll == 1) - AtCap));
				double Last = AtCap;
				for (double F = CapForce; F <= 10.0; F += 0.01)
				{
					const double Now = LimitAt(Long, F, bRoll == 1);
					bRises = bRises && Now >= Last - 1e-12;
					Last = Now;
				}
				bRises = bRises && LimitAt(Long, 7.0, bRoll == 1) > AtCap;
				bStops = bStops && LimitAt(Long, 9.0, bRoll == 1) == LimitAt(Long, 12.0, bRoll == 1);
				for (int F = 0; F < Forces; ++F)
				{
					const double Lo = bRoll ? Gale[A].Roll[F] : Gale[A].Pitch[F], Hi = bRoll ? Gale[A + 1].Roll[F] : Gale[A + 1].Pitch[F];
					WorstRow = std::max(WorstRow, std::fabs(LimitAt(Long, 7.0 + F, bRoll == 1) - (Lo + (Hi - Lo) * T)));
				}
			}
			for (double F : { 7.0, 8.0, 9.0 }) bSmallMore = bSmallMore && LimitAt(95.0, F, true) >= LimitAt(Long, F, true) - 1e-12;
		}
		Check("the ride's limits climb only above the cap: no jump there, rising to the gale's rows at Beaufort 7, 8 and 9, held over 9, and a small hull held to more "
			  "than a big one",
			Jump <= 1e-6 && bRises && bStops && bSmallMore && WorstRow <= 1e-12,
			"over a billionth of a Beaufort at the cap " + Num(Jump, 9) + " deg; a destroyer (114 m) at 15.5, 18.9 and 22.6 m/s: roll " +
				Num(LimitAt(114.0, ShowForce(15.5), true), 2) + ", " + Num(LimitAt(114.0, ShowForce(18.9), true), 2) + ", " + Num(LimitAt(114.0, ShowForce(22.6), true), 2) +
				" deg, pitch " + Num(LimitAt(114.0, ShowForce(15.5), false), 2) + ", " + Num(LimitAt(114.0, ShowForce(18.9), false), 2) + ", " +
				Num(LimitAt(114.0, ShowForce(22.6), false), 2) + "; a battleship's roll at 9 " + Num(LimitAt(250.0, 9.0, true), 2));
		// The rule: the step from 6 to 7 is the art director's own step from 5 to 6, and each step after it three quarters of the one before
		double WorstRule = 0.0;
		for (int K = 0; K < 3; ++K)
		{
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				const double* Of = Pass == 0 ? Gale[K].Roll : Gale[K].Pitch;
				const double Six = Pass == 0 ? TableRoll[K][3] : TablePitch[K][3], Five = Pass == 0 ? TableRoll[K][2] : TablePitch[K][2];
				double Step = Six - Five, Want = Six;
				for (int F = 0; F < Forces; ++F)
				{
					Want += Step;
					Step *= 0.75;
					WorstRule = std::max(WorstRule, std::fabs(Of[F] - Want));
				}
			}
		}
		Check("the ride's limits: Beaufort 7, 8 and 9 follow the rule (the art director's step from 5 to 6 carried on, each step three quarters of the one before)",
			WorstRule <= 1e-9, "worst " + Num(WorstRule * 1e9, 3) + "e-9 deg");
		// Data alone: no ride code in the header (step 5 builds the ride; nothing is built twice), and no heave limit
		const std::string Header = ReadAll(Src + "/Battle/FleetRideGale.h");
		const std::pair<const char*, const char*> Banned[] = { { "FleetWaves::", "the wave core" }, { "#include", "an include" }, { "SeaShow::", "the show" },
			{ "RideAt", "the ride's own work" }, { "HullRide", "the ride's own work" }, { "virtual", "a rider" }, { "FMath", "the engine" }, { "Dice", "a draw" },
			{ "std::", "the library" }, { "HeaveMost", "a heave limit (step 5: heave is not limited)" }, { "tanh", "a soft limit's own work" } };
		std::string Found;
		for (const auto& [Word, Why] : Banned)
		{
			if (Header.find(Word) != std::string::npos) Found += std::string(Word) + " (" + Why + "); ";
		}
		Check("the ride's limits are data (Battle/FleetRideGale.h: numbers and the readers of them alone, no ride, no include, nothing of FleetWaves or SeaShow, "
			  "and no heave limit), so step 5's ride reads them and nothing is built twice",
			Found.empty() && Header.size() > 2000, Found.empty() ? std::to_string(Header.size()) + " bytes" : Found);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The freeboard tests.

	/** One run: a hull of a row's class, at a heading to the sea and a speed, in a sea; the share of the time the water stood
	 * over each deck (each instant once: a deck is wet when the water stands over it on either side), how deep, and what her
	 * ride did; and the rms of her heave against the water's at her middle. */
	struct Run
	{
		double Bow = 0.0, Waist = 0.0, Stern = 0.0;
		double MostRoll = 0.0, MostPitch = 0.0, MostHeave = 0.0, OverBow = 0.0, OverWaist = 0.0, OverStern = 0.0;
		double HeaveRms = 0.0, WaterRms = 0.0;
		// (her heave's rms over an hour as she goes, and what her transfer function gives at her encounter frequencies,
		// sqrt(sum |C|^2 / 2): over 400 s two trains met at nearly one frequency need not have beaten through, over an hour
		// they have)
		double HeaveHour = 0.0, HeaveTf = 0.0;
		double LongestMet = 0.0; // (the longest period she meets any of her trains at, seconds)
	};

	/** [EST] The hull reference: a hull's own heave and pitch at a natural period of 5.75 s, damping 0.25 (a destroyer's,
	 * 5.5-6 s and 0.2-0.3 in the sea scene's review), in place of the look's light low-pass; her roll the look's. Not what
	 * is drawn: what the sea does to a hull, to show what the look's filter leaves out at her bow. (No bow swell-up either,
	 * so its bow is still a floor.) */
	constexpr double HullPeriod = 5.75, HullDamping = 0.25;

	/** Her ride as the look draws it, at her middle and the sea clock: RideAt's sum, roll and pitch soft-limited as step 5
	 * lays them (RollMax tanh(R / RollMax)), heave not ("water on her side is exactly what the tests catch"). */
	void LookRide(const Sea& S, const HullRide& H, double X, double Y, double T, double RollMax, double PitchMax, double& Heave, double& Pitch, double& Roll)
	{
		RideAt(S, H, X, Y, T, nullptr, Heave, Pitch, Roll);
		Roll = RollMax * std::tanh(Roll / RollMax);
		Pitch = PitchMax * std::tanh(Pitch / PitchMax);
	}

	Run Freeboard(const FleetRideGale::Deck& D, const Sea& S, double Force, double OffWind, double Speed, double Trim, bool bHull)
	{
		using namespace FleetRideGale;
		Run Out;
		const double Hdg = S.Downwind + Pi + OffWind; // (off head to the sea: 0 head, pi/2 beam, as she meets it)
		HullRide H;
		if (!bHull)
		{
			// (the ride as step 5 lays it: RideTrains' filters, her roll period by RideOf)
			RideTrains(S, D.Long, D.Beam, Hdg, Speed, RollTAt(D.Long), H);
		}
		else
		{
			RideFilter F = FilterFor(S, RollTAt(D.Long));
			F.HeaveW = TwoPi / HullPeriod;
			F.HeaveZeta = HullDamping;
			H.N = 0;
			for (int J = 0; J < S.ND; ++J)
			{
				if (S.D[J].L < RideShortest * D.Long) continue;
				H.Of[H.N] = J;
				H.C[H.N] = RideCoef(S.D[J], D.Long, D.Beam, Hdg, Speed, F);
				++H.N;
			}
		}
		const double RollMax = SoftOver * LimitAt(D.Long, Force, true), PitchMax = SoftOver * LimitAt(D.Long, Force, false);
		for (int I = 0; I < H.N; ++I)
		{
			Out.HeaveTf += 0.5 * std::norm(H.C[I].Heave);
			const Train& Tr = S.D[H.Of[I]];
			const double We = std::fabs(Tr.W - (Tr.Kx * std::sin(Hdg) + Tr.Ky * std::cos(Hdg)) * Speed);
			Out.LongestMet = std::max(Out.LongestMet, We > 1e-9 ? TwoPi / We : 1e30);
		}
		Out.HeaveTf = std::sqrt(Out.HeaveTf);
		const double Sx = std::sin(Hdg), Cy = std::cos(Hdg);
		const double Step = 0.25, Span = 400.0;
		const int Steps = static_cast<int>(Span / Step);
		long long N = 0;
		for (int K = 0; K < Steps; ++K)
		{
			const double T = 31.0 + K * Step;
			const double X = 400.0 + Speed * (T - 31.0) * Sx, Y = -250.0 + Speed * (T - 31.0) * Cy;
			double Heave = 0.0, Pitch = 0.0, Roll = 0.0;
			LookRide(S, H, X, Y, T, RollMax, PitchMax, Heave, Pitch, Roll);
			Out.MostRoll = std::max(Out.MostRoll, std::fabs(Roll));
			Out.MostPitch = std::max(Out.MostPitch, std::fabs(Pitch));
			Out.MostHeave = std::max(Out.MostHeave, std::fabs(Heave));
			const double Middle = HeightAt(S, X, Y, T);
			Out.HeaveRms += Heave * Heave;
			Out.WaterRms += Middle * Middle;
			const double Sp = std::sin(Pitch * Pi / 180.0), Cp = std::cos(Pitch * Pi / 180.0);
			const double Sr = std::sin(Roll * Pi / 180.0), Cr = std::cos(Roll * Pi / 180.0);
			struct Station
			{
				double Along, Across, Deck;
				double* Share;
				double* Over;
			};
			const Station Three[3] = { { 0.45 * D.Long, 0.0, D.Bow, &Out.Bow, &Out.OverBow }, { -0.1 * D.Long, D.WaistOut, D.Waist, &Out.Waist, &Out.OverWaist },
				{ -0.4 * D.Long, D.SternOut, D.Stern, &Out.Stern, &Out.OverStern } };
			for (const Station& St : Three)
			{
				// (either side, the instant counted once: the weather side is where the sea heels her down, and a quartering
				// sea takes each in turn. Her model turned about her waterline amidships, then lowered by her trim, as the
				// convoy looks draw a boat)
				bool bWet = false;
				for (const double Side : { St.Across, -St.Across })
				{
					const double Px = X + St.Along * Sx + Side * Cy, Py = Y + St.Along * Cy - Side * Sx;
					const double Deck = Heave - Trim + St.Along * Sp + St.Deck * Cp * Cr - Side * Sr;
					const double Water = HeightAt(S, Px, Py, T);
					if (Water > Deck)
					{
						bWet = true;
						*St.Over = std::max(*St.Over, Water - Deck);
					}
					if (St.Across == 0.0) break; // (the bow's station is on her middle line: once)
				}
				*St.Share += bWet ? 1.0 : 0.0;
			}
			++N;
		}
		const double Of = static_cast<double>(N);
		Out.Bow /= Of;
		Out.Waist /= Of;
		Out.Stern /= Of;
		Out.HeaveRms = std::sqrt(Out.HeaveRms / Of);
		Out.WaterRms = std::sqrt(Out.WaterRms / Of);
		for (int K = 0; K < 7200; ++K)
		{
			const double T = 31.0 + K * 0.5;
			double Heave = 0.0, Pitch = 0.0, Roll = 0.0;
			LookRide(S, H, 400.0 + Speed * (T - 31.0) * Sx, -250.0 + Speed * (T - 31.0) * Cy, T, RollMax, PitchMax, Heave, Pitch, Roll);
			Out.HeaveHour += Heave * Heave;
		}
		Out.HeaveHour = std::sqrt(Out.HeaveHour / 7200.0);
		return Out;
	}

	/** The trim the convoy looks draw a surfaced boat at, from the game's own StepBoat: her casing's drawn height over the
	 * still sea blown (her model's waterline) less awash (as she runs surfaced). 0 for a model it has no row for. */
	double DrawnTrim(const std::string& Model)
	{
		const ConvoyLook::BoatModel M = ConvoyLook::SculptedOf(Model);
		if (M.Name.empty()) return 0.0;
		const ConvoyLook::Drawn Up{}; // (the stage draws the modeller's boat at its own size: its stretch 1)
		ConvoyLook::BoatIn In;
		In.T = 10.0;
		In.Dive = "surfaced";
		In.Keel = 4.7;
		In.Dived = 0.0;
		In.Speed = 7.0;
		ConvoyLook::BoatState Awash, Blown;
		ConvoyLook::Blow(Blown, true);
		const ConvoyLook::BoatOut A = ConvoyLook::StepBoat(Awash, In, M, Up), B = ConvoyLook::StepBoat(Blown, In, M, Up);
		return B.DeckZ - A.DeckZ;
	}

	void Freeboards(const std::string& Out)
	{
		using namespace FleetRideGale;
		std::string Report = "# The gale's freeboard tests (the release 2 ocean plan, step 14(c); risk 14: \"at Beaufort 7 and up, crests can rise through a\n"
							 "# destroyer's low decks\"). How often the sea the scene draws stood over each deck: the share of the time over 400 s at 4 Hz, each\n"
							 "# instant counted once (a deck is wet when the water stands over it on either side), the most of four headings to the sea (head,\n"
							 "# bow, beam and quartering) and both speeds (her class's and hove to, 3 m/s). The sea: at Beaufort 7, 8 and 9 the gale's own, at\n"
							 "# each row's middle wind with GaleFetchOf's fetch (never a fully raised sea the scene does not ask for); at the show's cap, 13.8 m/s,\n"
							 "# and at 12.3 m/s the show's own sea (fully raised), which is the one drawn there. The ride: as step 5 lays it (its light low-pass\n"
							 "# at 0.4 Tp for heave and pitch, her roll oscillator at RideOf's period, full weights), roll and pitch soft-limited at 1.1 x\n"
							 "# FleetRideGale's LimitAt, heave not limited. A boat at the trim the game draws her at (ConvoyLooksCore: trim awash, her casing\n"
							 "# 0.5 m over the still sea, unless her tanks are blown). The decks are the ship models' own (Content/Models/Ships; a corvette's\n"
							 "# and a frigate's are estimates until the modeller makes them). These are the look's; for a destroyer the hull reference (a hull's\n"
							 "# own heave and pitch, 5.75 s and damping 0.25 [EST], in place of the look's low-pass) is given beside them.\n";
		// The boats' trim, as the game draws them
		std::string Trims;
		double WorstTrim = 0.0;
		for (const Deck& D : Decks)
		{
			const double Want = D.bBoat ? DrawnTrim(D.Model) : 0.0;
			WorstTrim = std::max(WorstTrim, std::fabs(Want - D.Trim));
			if (D.bBoat) Trims += std::string(D.Model) + " " + Num(Want, 3) + " m; ";
		}
		Check("freeboard: a surfaced boat is held at the trim the game draws her at (ConvoyLooksCore's StepBoat: awash, her casing 0.5 m over the still sea, her "
			  "model lowered by FleetRideGale's Trim; blown, at her model's waterline)",
			WorstTrim <= 0.005, Trims + "worst " + Num(100 * WorstTrim, 2) + " cm from the rows");

		const double Headings[4] = { 0.0, Pi / 4.0, Pi / 2.0, 3.0 * Pi / 4.0 };
		double LeastRatio = 9.0, MostRatio = 0.0, WorstTf = 0.0;
		int TfRuns = 0, TfSlow = 0, TfOver5 = 0;
		double SumHour = 0.0, SumTf = 0.0;
		std::string RatioLeast, RatioMost, TfWhere;
		// The worst of four headings and two speeds: each deck's most, and the run that was wettest over all three
		auto Worst = [&](const Deck& D, const Sea& S, double Force, double Trim, bool bHull, std::string& Where)
		{
			Run Most;
			double Wettest = -1.0;
			for (double Off : Headings)
			{
				for (double Speed : { D.Speed, HoveTo })
				{
					const Run One = Freeboard(D, S, Force, Off, Speed, Trim, bHull);
					const double Sum = One.Bow + One.Waist + One.Stern;
					if (Sum > Wettest)
					{
						Wettest = Sum;
						Where = Num(Off * 180.0 / Pi, 0) + " deg off head, " + Num(Speed, 1) + " m/s";
					}
					Most.Bow = std::max(Most.Bow, One.Bow);
					Most.Waist = std::max(Most.Waist, One.Waist);
					Most.Stern = std::max(Most.Stern, One.Stern);
					Most.MostRoll = std::max(Most.MostRoll, One.MostRoll);
					Most.MostPitch = std::max(Most.MostPitch, One.MostPitch);
					Most.MostHeave = std::max(Most.MostHeave, One.MostHeave);
					Most.OverBow = std::max(Most.OverBow, One.OverBow);
					Most.OverWaist = std::max(Most.OverWaist, One.OverWaist);
					Most.OverStern = std::max(Most.OverStern, One.OverStern);
					// (only where she meets every train of hers at least ten times in the hour: running before a quartering sea she
					// keeps pace with some, and an hour does not average a train met once)
					if (!bHull && One.LongestMet > 360.0) ++TfSlow;
					if (!bHull && One.LongestMet <= 360.0)
					{
						++TfRuns;
						SumHour += One.HeaveHour * One.HeaveHour;
						SumTf += One.HeaveTf * One.HeaveTf;
						const double Off1 = std::fabs(One.HeaveHour / std::max(One.HeaveTf, 1e-9) - 1.0);
						TfOver5 += Off1 > 0.05 ? 1 : 0;
						if (Off1 > WorstTf)
						{
							WorstTf = Off1;
							TfWhere = std::string(D.Name) + " at Beaufort " + Num(Force, 1) + ", " + Num(Off * 180.0 / Pi, 0) + " deg off head, " + Num(Speed, 1) + " m/s";
						}
					}
					if (!bHull && Force > ForceCap)
					{
						const double Ratio = One.HeaveRms / std::max(One.WaterRms, 1e-9);
						if (Ratio < LeastRatio)
						{
							LeastRatio = Ratio;
							RatioLeast = std::string(D.Name) + " at Beaufort " + Num(Force, 0) + ", " + Num(Off * 180.0 / Pi, 0) + " deg off head, " + Num(Speed, 1) + " m/s";
						}
						if (Ratio > MostRatio)
						{
							MostRatio = Ratio;
							RatioMost = std::string(D.Name) + " at Beaufort " + Num(Force, 0) + ", " + Num(Off * 180.0 / Pi, 0) + " deg off head, " + Num(Speed, 1) + " m/s";
						}
					}
				}
			}
			if (Wettest <= 0.0) Where = "dry at every heading and speed";
			return Most;
		};
		auto Shares = [](const Run& R) { return "bow " + Num(100 * R.Bow, 2) + "% (deepest " + Num(R.OverBow, 2) + " m), waist " + Num(100 * R.Waist, 2) + "% (" + Num(R.OverWaist, 2) +
										" m), stern " + Num(100 * R.Stern, 2) + "% (" + Num(R.OverStern, 2) + " m)"; };
		// (pinned as measured: within 0.003 and a twentieth of the pin, either way)
		int Pins = 0;
		std::string Moved;
		auto Pin = [&](const std::string& What, double Got, double Want)
		{
			++Pins;
			if (std::fabs(Got - Want) > 0.003 + 0.05 * Want) Moved += What + " " + Num(100 * Got, 2) + "% against its pin " + Num(100 * Want, 2) + "%; ";
		};
		bool bGrows = true, bDryBelow = true, bHullBow = true, bHullWetter = true;
		std::string Drier, WetBelow, HullBows;
		double SmallShare[Forces] = {}, BigShare[Forces] = {};
		std::string MadeMost, MadeBelow;
		const char* Names[3] = { "bow", "waist", "stern" };
		for (int Which = 0; Which < DeckCount; ++Which)
		{
			const Deck& D = Decks[Which];
			const bool bDestroyer = Which < 4;
			Report += "\n" + std::string(D.Name) + " (" + Num(D.Long, 1) + " m x " + Num(D.Beam, 1) + " m" + (D.Model[0] ? std::string(", ") + D.Model + ".json" : std::string()) +
				"; decks over her model's waterline: bow " + Num(D.Bow, 2) + " m, waist " + Num(D.Waist, 2) + " m at " + Num(D.WaistOut, 1) + " m out, stern " + Num(D.Stern, 2) +
				" m at " + Num(D.SternOut, 1) + " m out" +
				(D.bBoat ? "; drawn at trim awash " + Num(D.Trim, 2) + " m lower: bow " + Num(D.Bow - D.Trim, 2) + ", waist " + Num(D.Waist - D.Trim, 2) + ", stern " +
							   Num(D.Stern - D.Trim, 2) + " m over the still sea"
						 : std::string()) +
				")\n";
			double Last[3] = { -1.0, -1.0, -1.0 }, LastSum = -1.0, Measured[3][Forces] = {};
			for (int F = 0; F < Forces; ++F)
			{
				const BeaufortSea& R = FleetWaves::WmoGale[F];
				const double U = 0.5 * (R.From + R.To), Force = ShowForce(U);
				const Sea S = GaleSea(U, 4242, true);
				std::string Where;
				const Run Most = Worst(D, S, Force, D.Trim, false, Where);
				Report += "  B" + std::to_string(R.Force) + " (" + Num(U, 1) + " m/s, Hs " + Num(S.Hs, 2) + " m, Tp " + Num(S.Tp, 1) + " s): " + Shares(Most) +
					"; her ride at most " + Num(Most.MostRoll, 1) + " deg of roll, " + Num(Most.MostPitch, 1) + " of pitch, " + Num(Most.MostHeave, 2) +
					" m of heave; wettest at " + Where + "\n";
				const double Share[3] = { Most.Bow, Most.Waist, Most.Stern };
				const double Pinned[3] = { GreenMost[Which].Bow[F], GreenMost[Which].Waist[F], GreenMost[Which].Stern[F] };
				for (int K = 0; K < 3; ++K)
				{
					Pin(std::string(D.Name) + " " + Names[K] + " at B" + std::to_string(R.Force), Share[K], Pinned[K]);
					if (!D.bBoat && Share[K] < Last[K] - 0.005)
					{
						bGrows = false;
						Drier += std::string(D.Name) + " " + Names[K] + " at B" + std::to_string(R.Force) + "; ";
					}
					Last[K] = Share[K];
					Measured[K][F] = Share[K];
				}
				if (D.bBoat)
				{
					const double Sum = Share[0] + Share[1] + Share[2];
					if (Sum < LastSum - 0.005)
					{
						bGrows = false;
						Drier += std::string(D.Name) + " at B" + std::to_string(R.Force) + "; ";
					}
					LastSum = Sum;
				}
				const double Mine = Most.Bow + Most.Waist + Most.Stern;
				if (Which == 0) SmallShare[F] = Mine;
				if (std::string(D.Name) == "destroyer (USA)") BigShare[F] = Mine;
				if (D.bBoat)
				{
					std::string Blown;
					const Run Up = Worst(D, S, Force, 0.0, false, Blown);
					Report += "      her tanks blown (forced up, or striking: at her model's waterline): " + Shares(Up) + "\n";
				}
				if (bDestroyer)
				{
					std::string Ref;
					const Run Hull = Worst(D, S, Force, D.Trim, true, Ref);
					Report += "      the hull reference (her own heave and pitch at 5.75 s, damping 0.25 [EST]; not drawn): " + Shares(Hull) + "\n";
					if (F == 2)
					{
						bHullBow = bHullBow && Hull.Bow > 0.0;
						bHullWetter = bHullWetter && Hull.Bow > Most.Bow;
						HullBows += std::string(D.Name) + " " + Num(100 * Hull.Bow, 2) + "% against the look's " + Num(100 * Most.Bow, 2) + "%; ";
					}
				}
			}
			{
				char Row[240];
				std::snprintf(Row, sizeof(Row), "\t\t{ { %.4f, %.4f, %.4f }, { %.4f, %.4f, %.4f }, { %.4f, %.4f, %.4f } }, // %s\n", Measured[0][0], Measured[0][1],
					Measured[0][2], Measured[1][0], Measured[1][1], Measured[1][2], Measured[2][0], Measured[2][1], Measured[2][2], D.Name);
				MadeMost += Row;
			}
			// Below Beaufort 7: at the show's cap, 13.8 m/s, and at 12.3 m/s (Beaufort 6), the show's own sea, the one drawn there
			for (const double U : { Raised, 12.3 })
			{
				const Sea S = GaleSea(U, 4242, true); // (GaleFetchOf gives no fetch at or under the cap: the show's sea, fully raised)
				std::string Where;
				const Run Most = Worst(D, S, ShowForce(U), D.Trim, false, Where);
				Report += "  " + std::string(U == Raised ? "at the cap" : "B6") + " (" + Num(U, 1) + " m/s, Hs " + Num(S.Hs, 2) + " m, Tp " + Num(S.Tp, 1) + " s): " +
					Shares(Most) + "; wettest at " + Where + "\n";
				if (bDestroyer && (Most.Bow > 0.0 || Most.Waist > 0.0 || Most.Stern > 0.0))
				{
					bDryBelow = false;
					WetBelow += std::string(D.Name) + " at " + Num(U, 1) + " m/s; ";
				}
				if (U == Raised)
				{
					const GreenCap& Want = GreenBelow[Which];
					Pin(std::string(D.Name) + " bow at the cap", Most.Bow, Want.Bow);
					Pin(std::string(D.Name) + " waist at the cap", Most.Waist, Want.Waist);
					Pin(std::string(D.Name) + " stern at the cap", Most.Stern, Want.Stern);
					char Row[160];
					std::snprintf(Row, sizeof(Row), "\t\t{ %.4f, %.4f, %.4f }, // %s\n", Most.Bow, Most.Waist, Most.Stern, D.Name);
					MadeBelow += Row;
				}
			}
		}
		Check("freeboard: every deck's green water is FleetRideGale's pin, as measured (GreenMost at Beaufort 7, 8 and 9, and GreenBelow at the 13.8 m/s cap: " +
				  std::to_string(Pins) + " shares, each within 0.003 and a twentieth of its pin, either way)",
			Moved.empty(), Moved.empty() ? "written to " + Out + "/gale_freeboard.txt" : Moved);
		Check("freeboard: more green water as the gale rises (at Beaufort 8 and 9 a ship's every deck, and a boat's three summed, at least the force before's, "
			  "less 0.005; a boat's casing awash is under water near half the time from Beaufort 6, so deck by deck it no longer grows)",
			bGrows,
			bGrows ? "every class and deck" : "drier at " + Drier);
		Check("freeboard: below Beaufort 7 (at the show's 13.8 m/s cap and at 12.3 m/s, at every heading and speed) every destroyer's forecastle, weather deck and "
			  "quarterdeck stay dry (step 5's own check, as amended for destroyers and bigger); a boat's casing and bow and a corvette's waist wash there, and are "
			  "pinned as measured",
			bDryBelow, bDryBelow ? "the four destroyers" : "wet: " + WetBelow);
		Check("freeboard: heave is not limited (step 5): her heave's rms over an hour is what her transfer function gives at her encounter frequencies (step 5's own "
			  "check, as amended), within 2% over every run together and 15% in each, no more than one run in twenty over 5% (two trains met at nearly one "
			  "frequency need not beat through in an hour), where she meets each of her trains at least ten times in the hour",
			std::fabs(std::sqrt(SumHour / std::max(SumTf, 1e-12)) - 1.0) <= 0.02 && WorstTf <= 0.15 && TfOver5 * 20 <= TfRuns && TfRuns > 10 * TfSlow,
			std::to_string(TfRuns) + " runs (" + std::to_string(TfSlow) + " left out, keeping pace with a train), all together " +
				Num(100 * (std::sqrt(SumHour / std::max(SumTf, 1e-12)) - 1.0), 2) + "%, " + std::to_string(TfOver5) + " runs over 5%, worst " + Num(100 * WorstTf, 1) + "% (" + TfWhere + "); against the water's rms at her middle in the gale, " + Num(LeastRatio, 2) + " (" + RatioLeast + ") to " +
				Num(MostRatio, 2) + " (" + RatioMost + "): a hull heaves less than the water where she meets the waves fast or they are short against her");
		Check("freeboard: a low-sided destroyer ships more green water than a drier one of her length (the 3.42 m weather deck against the USA model's 4.02 m) at "
			  "Beaufort 8 and 9",
			SmallShare[1] > BigShare[1] && SmallShare[2] > BigShare[2],
			"destroyer " + Num(100 * SmallShare[1], 2) + "% and " + Num(100 * SmallShare[2], 2) + "% against " + Num(100 * BigShare[1], 2) + "% and " +
				Num(100 * BigShare[2], 2) + "% (the three decks summed)");
		Check("freeboard: the look's dry forecastle is its filter's, not the sea's: with a hull's own heave and pitch (5.75 s, damping 0.25 [EST]) in place of the "
			  "look's low-pass, every destroyer's forecastle ships green water at Beaufort 9, as real destroyers did, and more than the look's",
			bHullBow && bHullWetter, HullBows);
		std::ofstream File(Out + "/gale_freeboard.txt");
		File << Report;
		// (the rows as FleetRideGale::GreenMost and GreenBelow hold them, as measured: GALE_ROWS=1 writes them, so a change in
		// the sea or the ride is re-pinned by hand, in the same commit as the change)
		if (const char* bRows = std::getenv("GALE_ROWS"); bRows && *bRows == '1')
		{
			std::ofstream Rows(Out + "/gale_rows.inc");
			Rows << "GreenMost:\n" << MadeMost << "GreenBelow:\n" << MadeBelow;
		}
		Note("the freeboard report is in " + Out + "/gale_freeboard.txt");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The wake lane's blue: each hull's footprint.

	void LaneFoot()
	{
		std::vector<LanePoint> Wake;
		for (int Ship = 0; Ship < 3; ++Ship)
		{
			for (int K = 0; K < 24; ++K)
			{
				LanePoint P;
				P.X = 60.0 + 180.0 * Ship + 11.0 * K;
				P.Y = -300.0 + 30.0 * K;
				P.Age = 5.0 * K;
				P.Beam = 11.0 + 4.0 * Ship;
				P.Ship = Ship;
				Wake.push_back(P);
			}
		}
		const std::vector<OilDisc> Oil = { { 120.0, 180.0, 40.0 } };
		std::vector<LaneHull> Hulls;
		for (int Ship = 0; Ship < 3; ++Ship)
		{
			LaneHull H;
			H.X = 60.0 + 180.0 * Ship + 11.0 * 23.0 + 57.0;
			H.Y = -300.0 + 30.0 * 23.0 + 15.0;
			H.Hdg = 0.4 + 0.7 * Ship;
			H.Long = 114.0 + 60.0 * Ship;
			H.Beam = 11.0 + 4.0 * Ship;
			Hulls.push_back(H);
		}
		const LaneWindow W = LaneWindowFor(500.0, 300.0);
		std::vector<uint8_t> RG, A, B;
		PaintLanes(Wake, Oil, W, 20.0, RG);
		PaintLanesGale(Wake, Oil, Hulls, W, 20.0, A);
		PaintLanesGale(Wake, Oil, Hulls, W, 20.0, B);
		bool bSame = A.size() == RG.size() * 2 && A == B;
		int Blue = 0, Alpha = 0;
		for (size_t K = 0; K * 4 + 3 < A.size(); ++K)
		{
			bSame = bSame && A[4 * K] == RG[2 * K] && A[4 * K + 1] == RG[2 * K + 1];
			Blue += A[4 * K + 2] > 128 ? 1 : 0;
			Alpha += A[4 * K + 3] != 0 ? 1 : 0;
		}
		Check("the lane under the gale: RGBA8, its red and green byte for byte PaintLanes' (the step 12 lane as made), its blue each hull's footprint, and it is a "
			  "pure function of its points",
			bSame && Alpha == 0 && Blue > 0, std::to_string(Blue) + " texels inside a hull, alpha spare");
		// The footprint itself: her signed distance in from her waterline, against the true distance to that curve, and read
		// back from the blue byte
		double WorstBack = 0.0, WorstDist = 0.0;
		int In = 0, Outside = 0, Flat = 0, Signs = 0;
		const LaneHull& H = Hulls[0];
		const double Sx = std::sin(H.Hdg), Cy = std::cos(H.Hdg);
		// (her waterline as a closed curve in her own frame: 8,000 points down one side and up the other, so the nearest of
		// them is within 7 mm of the curve itself)
		std::vector<double> CurveA, CurveC;
		for (int K = 0; K <= 4000; ++K)
		{
			const double S = -1.0 + 2.0 * K / 4000.0;
			CurveA.push_back(S * 0.5 * H.Long);
			CurveC.push_back(0.5 * H.Beam * HalfBreadthShare(S));
		}
		for (int K = 4000; K >= 0; --K)
		{
			CurveA.push_back(CurveA[K]);
			CurveC.push_back(-CurveC[K]);
		}
		for (int K = 0; K < 20000; ++K)
		{
			const double Along = (2.0 * Mix(31000 + 2 * K) - 1.0) * 0.75 * H.Long, Across = (2.0 * Mix(31000 + 2 * K + 1) - 1.0) * 1.6 * H.Beam;
			const double X = H.X + Along * Sx + Across * Cy, Y = H.Y + Along * Cy - Across * Sx;
			const double Inside = HullInside(H, X, Y);
			// (the true distance to the curve, and the side: inside where her half-breadth there is wider than the offset)
			double True = 1e30;
			for (size_t Q = 0; Q < CurveA.size(); ++Q)
			{
				True = std::min(True, std::hypot(Along - CurveA[Q], Across - CurveC[Q]));
			}
			const bool bIn = std::fabs(Along) < 0.5 * H.Long && std::fabs(Across) < 0.5 * H.Beam * HalfBreadthShare(Along / (0.5 * H.Long));
			Signs += (Inside > 0.0) == bIn ? 0 : 1;
			In += bIn ? 1 : 0;
			Outside += bIn ? 0 : 1;
			Flat += FootFlatten(Inside) >= 1.0 ? 1 : 0;
			// (within a tenth of the distance and 2 cm: the first order of the distance, exact along her straight sides, and
			// her waterline's own curvature where it bends)
			if (True < 8.0) WorstDist = std::max(WorstDist, std::fabs(std::fabs(Inside) - True) - 0.1 * True - 0.02);
			const double Blue8 = std::round(255.0 * std::clamp(0.5 + std::clamp(Inside, -FootReach, FootReach) / (2.0 * FootReach), 0.0, 1.0)) / 255.0;
			if (std::fabs(Inside) < FootReach - 0.2) WorstBack = std::max(WorstBack, std::fabs(FootInside(Blue8) - Inside));
		}
		Check("the footprint: her signed distance in from her waterline, plus inside and nought on it (the waterplane's coefficient 0.71, a destroyer's), is the true "
			  "distance to that curve within a tenth of it and 2 cm out to 8 m, on the right side of it everywhere, and reads back from the blue byte within 7 cm (0.125 m a step over "
			  "16 m either way)",
			In > 1000 && Outside > 1000 && Signs == 0 && WorstDist <= 0.0 && WorstBack <= 0.07,
			std::to_string(In) + " points inside, " + std::to_string(Flat) + " where the sea is wholly flat; " + std::to_string(Signs) + " on the wrong side; worst "
				+ Num(100 * WorstBack, 2) + " cm read back, and " + Num(100 * WorstDist, 4) + " cm over the distance's own margin");
		Check("the footprint: the drawn sea is flat inside her and untouched outside (0 at her waterline, 1 from 1.5 m in)",
			FootFlatten(-0.1) == 0.0 && FootFlatten(0.0) == 0.0 && FootFlatten(0.75) > 0.4 && FootFlatten(0.75) < 0.6 && FootFlatten(FootRamp) == 1.0 &&
				FootFlatten(10.0) == 1.0);
		// Her footprint stays inside her: no texel beyond her bounds reads as inside
		bool bClean = true;
		const int N = W.Texels;
		const double Texel = W.Size / N;
		for (int J = 0; J < N; ++J)
		{
			for (int I = 0; I < N; ++I)
			{
				const double X = W.X0 + (I + 0.5) * Texel, Y = W.Y0 + (J + 0.5) * Texel;
				if (A[4 * (static_cast<size_t>(J) * N + I) + 2] <= 128) continue;
				bool bAny = false;
				for (const LaneHull& Her : Hulls) bAny = bAny || HullInside(Her, X, Y) > 0.0;
				bClean = bClean && bAny;
			}
		}
		Check("the footprint: a texel reads as inside a hull only where its own middle is inside one (the lane's texel is 4.9 m, so the flat is a distance, never a "
			  "cover spread onto the water beside her)",
			bClean);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The source scan.

	void Scan(const std::string& Src)
	{
		// The gale's own functions and data are new lines: nothing above them changed, and nothing the show decides reads
		// them (the show's own files never name them).
		const std::string Waves = ReadAll(Src + "/Battle/FleetWaves.h"), Face = ReadAll(Src + "/Battle/SeaFace.h");
		bool bHas = Waves.find("GaleFetchOf") != std::string::npos && Waves.find("StreakShareOf") != std::string::npos &&
			Waves.find("PaintLanesGale") != std::string::npos && Waves.find("WmoGale") != std::string::npos && Face.find("MakeStreaks") != std::string::npos;
		Check("the gale's own: GaleFetchOf, StreakShareOf, PaintLanesGale and WMO's rows in FleetWaves.h, MakeStreaks in SeaFace.h", bHas);
		// MakeFoam and MakeShore are not touched (pearl_pin holds their bodies; here: the gale adds its own function only)
		const std::string FaceCpp = ReadAll(Src + "/Battle/SeaFace.cpp");
		Check("SeaFace: MakeStreaks is a new function beside MakeFoam and MakeShore, which it does not call",
			FaceCpp.find("SeaFace::MakeStreaks") != std::string::npos && FaceCpp.find("MakeFoam(") == FaceCpp.rfind("MakeFoam(") &&
				FaceCpp.find("std::vector<uint8_t> SeaFace::MakeFoam") < FaceCpp.find("std::vector<uint8_t> SeaFace::MakeStreaks"));
		std::string Readers;
		for (const char* F : { "Battle/SeaShow.cpp", "Battle/SeaShow.h", "Battle/SeaFeed.cpp", "Battle/AirStage.cpp", "Battle/AirShow.cpp", "Battle/ConvoyLooksCore.cpp" })
		{
			const std::string Text = ReadAll(Src + "/" + F);
			for (const char* Name : { "FleetRideGale", "MakeStreaks", "GaleFetchOf", "StreakShareOf", "PaintLanesGale", "GaleCap" })
			{
				if (Text.find(Name) != std::string::npos) Readers += std::string(F) + ":" + Name + " ";
			}
		}
		Check("look only: nothing the show decides reads the gale (SeaShow, SeaFeed, AirStage, AirShow and the convoy looks' core name none of it)", Readers.empty(),
			Readers.empty() ? "none" : Readers);
		const std::string Switches = ReadAll(Src + "/Battle/SeaLookSwitches.h");
		Check("the switch: -TheaterSeaGale is in the table and still off and not built (the game does not draw the fleet's sea yet, so nothing reads it)",
			Switches.find("{ \"TheaterSeaGale\", \"off\", \"extra\", \"14\", false },") != std::string::npos);
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: gale_test <Source/TheaterCommand> <out dir>\n");
		return 2;
	}
	const std::string Src = argv[1], Out = argv[2];
	const auto T0 = Clock::now();
	struct Part
	{
		const char* Name;
		std::function<void()> Run;
	};
	const Part Parts[] = {
		{ "spectrum to 24.4", [] { Spectrum24(); } },
		{ "WMO's heights and periods", [] { WmoHeights(); } },
		{ "whitecaps at gale winds", [] { Caps(); } },
		{ "the wind's streaks", [&] { Streaks(Out); } },
		{ "the ride's limits", [&] { Limits(Src); } },
		{ "freeboard", [&] { Freeboards(Out); } },
		{ "the lane's footprint", [] { LaneFoot(); } },
		{ "the source scan", [&] { Scan(Src); } },
	};
	const char* Only = std::getenv("GALE_ONLY");
	for (const Part& P : Parts)
	{
		if (Only && *Only && std::string(",").append(Only).append(",").find(std::string(",") + P.Name + ",") == std::string::npos) continue;
		const auto P0 = Clock::now();
		std::printf("-- %s\n", P.Name);
		P.Run();
		std::printf("   (%.1f s)\n", Since(P0));
		std::fflush(stdout);
	}
	std::printf("gale_test: %d checks, %d failed (%.0f s)\n", Checks, Failed, Since(T0));
	return Failed == 0 ? 0 : 1;
}
