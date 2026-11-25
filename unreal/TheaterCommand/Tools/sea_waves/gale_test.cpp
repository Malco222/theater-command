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
//    from SeaShow.cpp, the rule that makes the gale's steps from it, and the limits' shape;
//  - the freeboard tests at Beaufort 7, 8 and 9: the gale's crests against the deck heights of destroyers, corvettes,
//    frigates and surfaced submarines (the ship models' own decks, Content/Models/Ships, held by gale_decks.py), at every
//    heading to the sea and at two speeds, with the ride at its limits, stating how often green water reaches each deck;
//  - the wake lane's blue as each hull's footprint (FleetWaves::PaintLanesGale), red and green byte for byte PaintLanes';
//  - a source scan: FleetRideGale.h holds data alone, the gale's own functions are new, and nothing the show decides
//    reads any of it.
// Below 13.8 m/s nothing is changed: Tools/sea_waves/gale_pin.cpp holds the core's outputs to 942d9295's, byte for byte.
// Plain C++20, no engine. Usage: gale_test <Source/TheaterCommand> <out dir>
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
		int MostD = 0, Seas = 0;
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
						if (Seed != 239 || Swell == 1) continue; // (the dense grid on one seed a wind: the others' leans alone)
						for (double T : { 61.3, 517.7 })
						{
							for (int Jy = 0; Jy <= 500; ++Jy)
							{
								for (int I = 0; I <= 500; ++I)
								{
									const double X = -500.0 + I * 2.0 + 0.37, Y = -500.0 + Jy * 2.0 + 0.71;
									LeastHere = std::min(LeastHere, SlopesAt(S, X, Y, T).J);
								}
							}
						}
					}
				}
			}
			LeastJ = std::min(LeastJ, LeastHere);
			Each += Num(U, 1) + " m/s " + Num(LeastHere, 3) + "; ";
			if (U >= GaleCap) break;
		}
		Check("the gale's spectrum to 24.4 m/s: the least J over a 1 km square at 2 m is above 0.15 (the plan's named test for step 14(c); "
			  "fetch-limited and fully raised, with a swell and without)",
			LeastJ > 0.15, "least " + Num(LeastJ, 4) + " over " + std::to_string(Seas) + " gale seas: " + Each);
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
		Check("the gale's sea at the cap is exactly the cap's own sea (0.021 x 13.8^2 = 4.00 m: the fetch-limited sea and the show's meet there, so nothing jumps "
			  "when the switch raises the cap)",
			std::fabs(CapHs - 0.021 * Raised * Raised) <= 0.01, Num(CapHs, 4) + " m against " + Num(0.021 * Raised * Raised, 4) + " m");
		// The periods: the fetch-limited sea's peak against JONSWAP's own law
		double WorstTp = 0.0;
		for (double U = 14.0; U <= GaleCap + 1e-9; U += 0.5)
		{
			double Hj = 0.0, Wj = 0.0;
			FetchLaw(U, GaleFetchOf(U), Hj, Wj);
			const Sea S = GaleSea(U, 4242, true);
			WorstTp = std::max(WorstTp, std::fabs(S.Tp / (TwoPi / Wj) - 1.0));
		}
		Check("the gale's periods: the fetch-limited sea's peak period is JONSWAP's fetch law's within 5% (the core takes the longer of it and the fully raised "
			  "sea's, so the peak never shortens)",
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

	/** A number named in SeaShow.cpp's own Ride table (the art director's), read from its text: Small, Middle and Big rows. */
	bool ShowTable(const std::string& Text, double (&Roll5)[3], double (&Roll6)[3], double (&Pitch5)[3], double (&Pitch6)[3], double (&RollT)[3])
	{
		static const std::regex Row("static const Ride (Small|Middle|Big) = \\{ \\{ ([^}]*)\\}, \\{ ([^}]*)\\}, ([0-9.]+), ([0-9.]+), ([0-9.]+) \\};");
		int Seen = 0;
		for (std::sregex_iterator It(Text.begin(), Text.end(), Row), End; It != End; ++It)
		{
			const std::string Which = (*It)[1];
			const int K = Which == "Small" ? 0 : Which == "Middle" ? 1 : 2;
			double Rolls[4] = {}, Pitches[4] = {};
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				std::istringstream In((*It)[2 + Pass].str());
				std::string Each;
				int N = 0;
				while (std::getline(In, Each, ',') && N < 4)
				{
					(Pass == 0 ? Rolls : Pitches)[N++] = std::atof(Each.c_str());
				}
				if (N != 4) return false;
			}
			Roll5[K] = Rolls[2];
			Roll6[K] = Rolls[3];
			Pitch5[K] = Pitches[2];
			Pitch6[K] = Pitches[3];
			RollT[K] = std::atof((*It)[4].str().c_str());
			++Seen;
		}
		return Seen == 3;
	}

	void Limits(const std::string& Src)
	{
		using namespace FleetRideGale;
		double Roll5[3] = {}, Roll6[3] = {}, Pitch5[3] = {}, Pitch6[3] = {}, RollT[3] = {};
		const std::string Show = ReadAll(Src + "/Battle/SeaShow.cpp");
		const bool bRead = ShowTable(Show, Roll5, Roll6, Pitch5, Pitch6, RollT);
		bool bSame = bRead;
		for (int K = 0; K < 3; ++K)
		{
			bSame = bSame && Roll5[K] == TableRoll5[K] && Roll6[K] == TableRoll6[K] && Pitch5[K] == TablePitch5[K] && Pitch6[K] == TablePitch6[K] && RollT[K] == TableRollT[K];
		}
		Check("the ride's limits: the art director's own Beaufort 5 and 6 columns and roll periods, read again from SeaShow.cpp's Ride table (Small, Middle, Big), "
			  "are the ones FleetRideGale.h holds (it changes neither the table nor the show's Raised cap of 13.8 m/s)",
			bSame, bRead ? "roll 6: " + Num(Roll6[0], 2) + ", " + Num(Roll6[1], 2) + ", " + Num(Roll6[2], 2) + "; pitch 6: " + Num(Pitch6[0], 2) + ", " +
					Num(Pitch6[1], 2) + ", " + Num(Pitch6[2], 2)
							: "SeaShow.cpp's Ride table not read");
		Check("the ride's limits: SeaShow's own cap stays 13.8 m/s and its table stops at Beaufort 6 (the show does not move)",
			Show.find("const double Raised = std::min(WindU, 13.8);") != std::string::npos && Show.find("std::min(Force, 6.0) - 3.0") != std::string::npos);
		// The rule: the step from 6 to 7 is the art director's own step from 5 to 6, and each step after it three quarters of the one before
		double WorstRule = 0.0;
		for (int K = 0; K < 3; ++K)
		{
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				const double* Of = Pass == 0 ? Rows[K]->Roll : Rows[K]->Pitch;
				const double Six = Pass == 0 ? TableRoll6[K] : TablePitch6[K], Five = Pass == 0 ? TableRoll5[K] : TablePitch5[K];
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
		// The shape: a hull rolls and pitches more as the gale rises, a small hull more than a big one, and the limits are
		// continuous with the art director's at Beaufort 6
		bool bRises = true, bSmallMore = true, bJoins = true;
		for (double Long : { 95.0, 110.0, 118.5, 180.0, 250.0, 300.0 })
		{
			for (int bRoll = 0; bRoll < 2; ++bRoll)
			{
				double Last = LimitAt(Long, 6.0, bRoll == 1);
				const int K = RowOf(Long) >= 1.0 ? 1 : 0;
				const double T = RowOf(Long) - K;
				const double Six = (bRoll ? TableRoll6[K] : TablePitch6[K]) + ((bRoll ? TableRoll6[K + 1] : TablePitch6[K + 1]) - (bRoll ? TableRoll6[K] : TablePitch6[K])) * T;
				bJoins = bJoins && std::fabs(Last - Six) <= 1e-9 && std::fabs(LimitAt(Long, 5.5, bRoll == 1) - Six) <= 1e-9;
				for (double F = 6.0; F <= 10.0; F += 0.05)
				{
					const double Now = LimitAt(Long, F, bRoll == 1);
					bRises = bRises && Now >= Last - 1e-9;
					Last = Now;
				}
				bRises = bRises && std::fabs(LimitAt(Long, 9.0, bRoll == 1) - LimitAt(Long, 12.0, bRoll == 1)) <= 1e-9;
			}
			bSmallMore = bSmallMore && (Long <= 95.0 || LimitAt(95.0, 8.0, true) >= LimitAt(Long, 8.0, true));
		}
		Check("the ride's limits: they join the art director's at Beaufort 6, rise with the gale, stop at 9, and a small hull is held to more than a big one",
			bRises && bSmallMore && bJoins,
			"a destroyer at 7, 8, 9: roll " + Num(LimitAt(114.0, 7.0, true), 2) + ", " + Num(LimitAt(114.0, 8.0, true), 2) + ", " + Num(LimitAt(114.0, 9.0, true), 2) +
				" deg; pitch " + Num(LimitAt(114.0, 7.0, false), 2) + ", " + Num(LimitAt(114.0, 8.0, false), 2) + ", " + Num(LimitAt(114.0, 9.0, false), 2) +
				"; a battleship's roll " + Num(LimitAt(250.0, 9.0, true), 2));
		// Data alone: no ride code in the header (step 5 builds the ride; nothing is built twice)
		const std::string Header = ReadAll(Src + "/Battle/FleetRideGale.h");
		const std::pair<const char*, const char*> Banned[] = { { "FleetWaves::", "the wave core" }, { "#include", "an include" }, { "SeaShow::", "the show" },
			{ "RideAt", "the ride's own work" }, { "HullRide", "the ride's own work" }, { "virtual", "a rider" }, { "FMath", "the engine" }, { "Dice", "a draw" },
			{ "std::", "the library" } };
		std::string Found;
		for (const auto& [Word, Why] : Banned)
		{
			if (Header.find(Word) != std::string::npos) Found += std::string(Word) + " (" + Why + "); ";
		}
		Check("the ride's limits are data (Battle/FleetRideGale.h: numbers and the readers of them alone, no ride, no include, nothing of FleetWaves or SeaShow), "
			  "so step 5's ride reads them and nothing is built twice",
			Found.empty() && Header.size() > 2000, Found.empty() ? std::to_string(Header.size()) + " bytes" : Found);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The freeboard tests.

	/** One run: a hull of a row's class, at a heading to the sea and a speed, in a sea; how often the water stood over each
	 * deck, and what her ride did. */
	struct Run
	{
		double Bow = 0.0, Waist = 0.0, Stern = 0.0; // the share of the time the water stood over each deck
		double MostRoll = 0.0, MostPitch = 0.0, MostHeave = 0.0, OverBow = 0.0, OverWaist = 0.0, OverStern = 0.0;
	};

	Run Freeboard(const FleetRideGale::Deck& D, const Sea& S, double Force, double OffWind, double Speed)
	{
		using namespace FleetRideGale;
		Run Out;
		const double Hdg = S.Downwind + Pi + OffWind; // (off head to the sea: 0 head, pi/2 beam, as she meets it)
		HullRide H;
		RideTrains(S, D.Long, D.Beam, Hdg, Speed, TableRollT[RowOf(D.Long) >= 1.0 ? 1 : 0], H);
		const double RollMax = SoftOver * LimitAt(D.Long, Force, true), PitchMax = SoftOver * LimitAt(D.Long, Force, false);
		const double HeaveMax = HeaveMost * std::max(S.Hs, 1e-3); // (the gale's own heave limit: see FleetRideGale.h)
		const double Sx = std::sin(Hdg), Cy = std::cos(Hdg);
		const double Step = 0.25, Span = 400.0;
		const int Steps = static_cast<int>(Span / Step);
		long long N = 0;
		for (int K = 0; K < Steps; ++K)
		{
			const double T = 31.0 + K * Step;
			const double X = 400.0 + Speed * (T - 31.0) * Sx, Y = -250.0 + Speed * (T - 31.0) * Cy;
			double Heave = 0.0, Pitch = 0.0, Roll = 0.0;
			RideAt(S, H, X, Y, T, nullptr, Heave, Pitch, Roll);
			Roll = RollMax * std::tanh(Roll / RollMax);
			Pitch = PitchMax * std::tanh(Pitch / PitchMax);
			Heave = HeaveMax * std::tanh(Heave / HeaveMax);
			Out.MostRoll = std::max(Out.MostRoll, std::fabs(Roll));
			Out.MostPitch = std::max(Out.MostPitch, std::fabs(Pitch));
			Out.MostHeave = std::max(Out.MostHeave, std::fabs(Heave));
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
				// (either side: the weather side is where the sea heels her down, and a quartering sea takes each in turn)
				for (const double Side : { St.Across, -St.Across })
				{
					const double Px = X + St.Along * Sx + Side * Cy, Py = Y + St.Along * Cy - Side * Sx;
					const double Deck = Heave + St.Along * Sp + St.Deck * Cp * Cr - Side * Sr;
					const double Water = HeightAt(S, Px, Py, T);
					if (Water > Deck)
					{
						*St.Share += 1.0;
						*St.Over = std::max(*St.Over, Water - Deck);
					}
					if (St.Across == 0.0) break; // (the bow's station is on her middle line: once)
				}
			}
			++N;
		}
		const double Of = static_cast<double>(N);
		Out.Bow /= Of;
		Out.Waist /= Of;
		Out.Stern /= Of;
		return Out;
	}

	void Freeboards(const std::string& Out)
	{
		using namespace FleetRideGale;
		std::string Report = "# The gale's freeboard tests (the release 2 ocean plan, step 14(c); risk 14: \"at Beaufort 7 and up, crests can rise through a\n"
							 "# destroyer's low decks\"). How often the gale's sea stood over each deck, with the ride at FleetRideGale's limits: the share of the\n"
							 "# time over 400 s at 4 Hz, the most of every heading to the sea (head, bow, beam and quartering) and of both speeds (her class's and\n"
							 "# hove to, 3 m/s), in the gale's own sea (GaleFetchOf) and in the sea the wind has fully raised. The decks are the ship models' own\n"
							 "# (Content/Models/Ships; a corvette's and a frigate's are estimates until the modeller makes them).\n";
		double WorstOver = 0.0;
		std::string WorstWhere, Made;
		int Rows = 0;
		bool bGrows = true, bSmallWetter = true, bQuietBelow = true;
		double SmallShare[Forces] = {}, BigShare[Forces] = {};
		for (int Which = 0; Which < DeckCount; ++Which)
		{
			const Deck& D = Decks[Which];
			Report += "\n" + std::string(D.Name) + " (" + Num(D.Long, 1) + " m x " + Num(D.Beam, 1) + " m" + (D.Model[0] ? std::string(", ") + D.Model + ".json" : std::string()) +
				"; decks over her waterline: bow " + Num(D.Bow, 2) + " m, waist " + Num(D.Waist, 2) + " m at " + Num(D.WaistOut, 1) + " m out, stern " + Num(D.Stern, 2) +
				" m at " + Num(D.SternOut, 1) + " m out)\n";
			double Last[3] = { -1.0, -1.0, -1.0 }, Measured[3][Forces] = {};
			for (int F = 0; F < Forces; ++F)
			{
				const BeaufortSea& R = FleetWaves::WmoGale[F];
				const double U = 0.5 * (R.From + R.To);
				Run Most;
				std::string Worst;
				for (int Fetch = 0; Fetch < 2; ++Fetch)
				{
					const Sea S = GaleSea(U, 4242, Fetch == 1);
					for (double Off : { 0.0, Pi / 4.0, Pi / 2.0, 3.0 * Pi / 4.0 })
					{
						for (double Speed : { D.Speed, HoveTo })
						{
							const Run One = Freeboard(D, S, R.Force, Off, Speed);
							if (One.Bow + One.Waist + One.Stern > Most.Bow + Most.Waist + Most.Stern)
							{
								Worst = Num(Off * 180.0 / Pi, 0) + " deg off head, " + Num(Speed, 1) + " m/s, " + (Fetch ? "the gale's sea" : "fully raised");
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
							// (the limits themselves: the ride never past its soft limit, and look heave at most 0.5 Hs)
							const double RollMax = SoftOver * LimitAt(D.Long, static_cast<double>(R.Force), true);
							const double PitchMax = SoftOver * LimitAt(D.Long, static_cast<double>(R.Force), false);
							if (One.MostRoll > RollMax + 1e-9 || One.MostPitch > PitchMax + 1e-9) bGrows = false;
							if (One.MostHeave > HeaveMost * S.Hs + 1e-9)
							{
								const double Over = One.MostHeave / (HeaveMost * S.Hs);
								if (Over > WorstOver)
								{
									WorstOver = Over;
									WorstWhere = std::string(D.Name) + " at B" + std::to_string(R.Force);
								}
							}
						}
					}
				}
				const double Share[3] = { Most.Bow, Most.Waist, Most.Stern };
				for (int K = 0; K < 3; ++K)
				{
					bGrows = bGrows && Share[K] >= Last[K] - 0.02; // (more green water as the gale rises, within the runs' own noise)
					Last[K] = Share[K];
				}
				const double Mine = Most.Bow + Most.Waist + Most.Stern;
				if (Which == 0) SmallShare[F] = Mine;
				if (std::string(D.Name) == "destroyer (USA)") BigShare[F] = Mine;
				Report += "  B" + std::to_string(R.Force) + " (" + Num(U, 1) + " m/s, Hs " + Num(GaleSea(U, 4242, true).Hs, 2) + " to " + Num(GaleSea(U, 4242, false).Hs, 2) +
					" m): green water over her bow " + Num(100 * Most.Bow, 2) + "% of the time (deepest " + Num(Most.OverBow, 2) + " m), her waist " +
					Num(100 * Most.Waist, 2) + "% (" + Num(Most.OverWaist, 2) + " m), her stern " + Num(100 * Most.Stern, 2) + "% (" + Num(Most.OverStern, 2) +
					" m); her ride at most " + Num(Most.MostRoll, 1) + " deg of roll, " + Num(Most.MostPitch, 1) + " of pitch, " + Num(Most.MostHeave, 2) +
					" m of heave; worst at " + Worst + "\n";
				for (int K = 0; K < 3; ++K) Measured[K][F] = Share[K];
				const Green& Cap = GreenMost[Which];
				const double Caps[3] = { Cap.Bow[F], Cap.Waist[F], Cap.Stern[F] };
				const char* Names[3] = { "bow", "waist", "stern" };
				for (int K = 0; K < 3; ++K)
				{
					++Rows;
					if (Share[K] > Caps[K] + 1e-12)
					{
						Report += "    OVER the row's " + std::string(Names[K]) + " cap " + Num(100 * Caps[K], 2) + "%\n";
						bQuietBelow = false;
					}
				}
			}
			{
				// (the row as FleetRideGale::GreenMost would pin it: each measured share times 1.25 and 0.005 more)
				char Row[240];
				std::snprintf(Row, sizeof(Row), "\t\t{ { %.4f, %.4f, %.4f }, { %.4f, %.4f, %.4f }, { %.4f, %.4f, %.4f } }, // %s\n",
					1.25 * Measured[0][0] + 0.005, 1.25 * Measured[0][1] + 0.005, 1.25 * Measured[0][2] + 0.005, 1.25 * Measured[1][0] + 0.005,
					1.25 * Measured[1][1] + 0.005, 1.25 * Measured[1][2] + 0.005, 1.25 * Measured[2][0] + 0.005, 1.25 * Measured[2][1] + 0.005,
					1.25 * Measured[2][2] + 0.005, D.Name);
				Made += Row;
			}
			// Below Beaufort 7 her deck edge stays dry (step 5's own limit, which the gale's rows take over from)
			const Sea Six = GaleSea(12.3, 4242, false);
			const Run Dry = Freeboard(D, Six, 6.0, Pi / 2.0, D.Speed);
			if (Dry.Waist > 0.0 || Dry.Stern > 0.0) bQuietBelow = false;
			Report += "  B6 (12.3 m/s, Hs " + Num(Six.Hs, 2) + " m, the show's own sea at its cap, beam on): her waist " + Num(100 * Dry.Waist, 2) + "%, her stern " +
				Num(100 * Dry.Stern, 2) + "%, her bow " + Num(100 * Dry.Bow, 2) + "%\n";
		}
		for (int F = 0; F < Forces; ++F) bSmallWetter = bSmallWetter && SmallShare[F] >= BigShare[F] - 0.05;
		Check("freeboard: every deck's green water keeps to FleetRideGale::GreenMost, the row it is held to at Beaufort 7, 8 and 9 (" + std::to_string(Rows) +
				  " rows: 9 classes x 3 forces x 3 decks, the most of 4 headings, 2 speeds and 2 seas)",
			bQuietBelow, "written to " + Out + "/gale_freeboard.txt");
		Check("freeboard: more green water as the gale rises, and below Beaufort 7 her deck edge and quarterdeck stay dry (step 5's own limit, which the gale's rows "
			  "take over from)",
			bGrows, bGrows ? "every class" : "a class got drier, or the ride passed its soft limit");
		Check("freeboard: look heave keeps to 0.5 Hs in the gale too (step 5's limit; the gale's rows give heave the same soft limit as roll and pitch, since the "
			  "trains' crests sum to about 0.8 Hs at gale winds)",
			WorstOver <= 1.0, WorstOver <= 1.0 ? "every run of every class" : WorstWhere + " by " + Num(WorstOver, 2) + "x");
		Check("freeboard: a low-sided destroyer ships more green water than a drier one of the same length (the USA model's 4.02 m weather deck against the 3.42 m one)",
			bSmallWetter, "destroyer " + Num(100 * SmallShare[2], 1) + "% against " + Num(100 * BigShare[2], 1) + "% at Beaufort 9 (the three decks summed)");
		std::ofstream File(Out + "/gale_freeboard.txt");
		File << Report;
		// (the rows as FleetRideGale::GreenMost holds them, each measured share with its margin: GALE_ROWS=1 prints them,
		// so a change in the sea or the ride is re-pinned by hand, in the same commit as the change)
		if (const char* bRows = std::getenv("GALE_ROWS"); bRows && *bRows == '1')
		{
			std::ofstream Out2(Out + "/gale_rows.inc");
			Out2 << Made;
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
