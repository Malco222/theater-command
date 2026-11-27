// The season turns on the campaign map: the cloud's checks on Campaign/SeasonTurn (see run.sh).
// With "--map FILE OUTDIR" it also paints drawings of the turn over the game's own east (see draw_east.py).
#include "Campaign/SeasonTurn.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace TheaterSeasonTurn;

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
		std::snprintf(Text, sizeof(Text), "%.4f", V);
		return Text;
	}

	double Luma(const uint8_t C[3]) { return 0.2126 * C[0] + 0.7152 * C[1] + 0.0722 * C[2]; }

	// ---- the clock ----
	void CheckClock()
	{
		Check(Front(-1.0) == 0.0 && Front(0.0) == 0.0, "nothing has moved before the spread begins");
		double Was = 0.0;
		bool bMonotone = true;
		for (int K = 1; K <= 400; ++K)
		{
			const double Now = Front(K * (Spreads + Settles + 0.5) / 400.0);
			bMonotone &= Now >= Was - 1e-12;
			Was = Now;
		}
		Check(bMonotone, "the front never goes back");
		Check(std::fabs(Front(Spreads + Settles) - Settled) < 1e-9 && IsSettled(Front(Spreads + Settles)), "settled after " + Num(Spreads + Settles) + " s");
		Check(!IsSettled(Front(Spreads + Settles * 0.9)), "not settled before its time");
		Check(std::fabs(Spreads + Settles - 3.5) < 1e-9, "about three and a half seconds, as the sheet asks");
		// Every point, from the first to the last to change, is untouched at the start and wholly changed, band gone, when settled.
		bool bStartClean = true, bEndWhole = true;
		for (int K = 0; K <= 100; ++K)
		{
			const float A = static_cast<float>(K / 100.0);
			const FShown S0 = At(A, Front(0.0)), S1 = At(A, Settled);
			bStartClean &= S0.Share == 0.0f && S0.Band == 0.0f;
			bEndWhole &= S1.Share == 1.0f && S1.Band == 0.0f;
		}
		Check(bStartClean, "at the start no point shows any change");
		Check(bEndWhole, "settled, every point shows its whole change and no wet band");
		// The front is soft: for land changing evenly, the share that changes in any tenth of a second stays small (no snap, no jump).
		double Worst = 0.0;
		for (int Step = 0; Step < 400; ++Step)
		{
			const double T0 = Step * (Spreads + Settles) / 400.0, T1 = T0 + 0.1;
			double Moved = 0.0;
			for (int K = 0; K < 1000; ++K)
			{
				const float A = static_cast<float>((K + 0.5) / 1000.0);
				Moved += At(A, Front(T1)).Share - At(A, Front(T0)).Share;
			}
			Worst = std::max(Worst, Moved / 1000.0);
		}
		Check(Worst < 0.075, "no tenth of a second changes more than 7.5% of the land (worst " + Num(Worst) + ")");
		Check(Worst > 0.02, "and it does move, " + Num(Worst) + " in the busiest tenth");
		// The wet band lies just behind the front and nowhere else.
		bool bBandBehind = true;
		float MostBand = 0.0f;
		for (int K = 0; K <= 200; ++K)
		{
			const double F = K / 200.0 * Settled;
			for (int J = 0; J <= 100; ++J)
			{
				const float A = static_cast<float>(J / 100.0);
				const FShown S = At(A, F);
				MostBand = std::max(MostBand, S.Band);
				if (S.Band > 0.0f && (F - A < 0.0 || F - A > BandWidth))
				{
					bBandBehind = false;
				}
			}
		}
		Check(bBandBehind, "the wet band lies only in the strip just behind the front");
		Check(MostBand > 0.95f && MostBand <= 1.0f, "the band reaches its full weight and no more (" + Num(MostBand) + ")");
	}

	// ---- noise and distance ----
	void CheckNoise()
	{
		bool bRange = true, bSame = true;
		double Sum = 0.0;
		int Differ = 0;
		for (int K = 0; K < 20000; ++K)
		{
			const double X = (K % 200) * 0.137 - 3.0, Y = (K / 200) * 0.191 + 5.0;
			const double N = Noise(X, Y, 7u);
			bRange &= N >= 0.0 && N <= 1.0;
			bSame &= N == Noise(X, Y, 7u);
			Differ += std::fabs(N - Noise(X, Y, 8u)) > 0.05 ? 1 : 0;
			Sum += N;
		}
		Check(bRange, "noise stays within 0 and 1");
		Check(bSame, "noise is the same for the same place and seed");
		Check(Differ > 12000, "another seed is another noise");
		Check(std::fabs(Sum / 20000.0 - 0.5) < 0.06, "noise is about a half on the whole (" + Num(Sum / 20000.0) + ")");
		// smooth: a step of a twentieth of a unit moves it little
		double Steepest = 0.0;
		for (int K = 0; K < 5000; ++K)
		{
			const double X = K * 0.0371, Y = K * 0.0213;
			Steepest = std::max(Steepest, std::fabs(Noise(X + 0.05, Y, 3u) - Noise(X, Y, 3u)));
		}
		Check(Steepest < 0.12, "noise is smooth: a twentieth of a unit moves it at most " + Num(Steepest));
	}

	void CheckDistance()
	{
		const int NX = 61, NY = 47;
		std::vector<float> Field(static_cast<size_t>(NX * NY), 1e9f);
		const int Seeds[][2] = { { 5, 5 }, { 50, 9 }, { 30, 40 }, { 12, 33 } };
		for (const auto& S : Seeds)
		{
			Field[static_cast<size_t>(S[1] * NX + S[0])] = 0.0f;
		}
		Spread(Field, NX, NY, 1e9f);
		double WorstShare = 0.0;
		for (int J = 0; J < NY; ++J)
		{
			for (int I = 0; I < NX; ++I)
			{
				double True = 1e9;
				for (const auto& S : Seeds)
				{
					True = std::min(True, std::hypot(I - S[0], J - S[1]));
				}
				const double Got = Field[static_cast<size_t>(J * NX + I)];
				if (True > 0.0)
				{
					WorstShare = std::max(WorstShare, std::fabs(Got - True) / True);
				}
				else if (Got != 0.0)
				{
					WorstShare = 1.0;
				}
			}
		}
		Check(WorstShare < 0.085, "distance on the grid within 8.5% of the straight line (worst " + Num(WorstShare) + ")");
		std::vector<float> Capped(static_cast<size_t>(NX * NY), 1e9f);
		Capped[0] = 0.0f;
		Spread(Capped, NX, NY, 20.0f);
		Check(Capped.back() == 20.0f && Capped[1] == 1.0f, "distance is capped where asked");
	}

	void CheckEvenAndBlur()
	{
		// Arrivals crowded early (as the soak's are, the water being everywhere) come out spread evenly, in the same order.
		std::vector<float> Arrive;
		uint32_t Seed = 99u;
		for (int K = 0; K < 20000; ++K)
		{
			Seed = Seed * 1664525u + 1013904223u;
			const double U = static_cast<double>(Seed >> 8) / 16777216.0;
			Arrive.push_back(static_cast<float>(U * U * 0.7));
		}
		std::vector<float> Spread01 = Arrive;
		Even(Spread01);
		bool bOrder = true;
		for (size_t K = 1; K < Arrive.size(); ++K)
		{
			bOrder &= (Arrive[K] < Arrive[K - 1]) == (Spread01[K] < Spread01[K - 1]) || Arrive[K] == Arrive[K - 1] || std::fabs(Spread01[K] - Spread01[K - 1]) < 2e-4f;
		}
		double Worst = 0.0;
		for (int Q = 1; Q < 10; ++Q)
		{
			const double Below = static_cast<double>(std::count_if(Spread01.begin(), Spread01.end(), [Q](float A) { return A < Q / 10.0; })) / static_cast<double>(Spread01.size());
			Worst = std::max(Worst, std::fabs(Below - Q / 10.0));
		}
		Check(bOrder, "spreading the arrivals evenly keeps which point comes first");
		Check(Worst < 0.01, "and the front crosses as much land in each tenth of the turn (worst " + Num(Worst) + " off)");
		// Smoothing keeps an even field even and takes the step out of a province's edge.
		const int NX = 60, NY = 40;
		std::vector<float> Flat(NX * NY, 1.3f), Step(NX * NY);
		for (int J = 0; J < NY; ++J)
		{
			for (int I = 0; I < NX; ++I)
			{
				Step[static_cast<size_t>(J * NX + I)] = I < NX / 2 ? 0.3f : 2.1f;
			}
		}
		Blur(Flat, NX, NY, 4);
		Blur(Step, NX, NY, 4);
		double Off = 0.0, Steepest = 0.0;
		for (int J = 0; J < NY; ++J)
		{
			for (int I = 0; I < NX; ++I)
			{
				Off = std::max(Off, std::fabs(Flat[static_cast<size_t>(J * NX + I)] - 1.3));
				if (I > 0)
				{
					Steepest = std::max(Steepest, std::fabs(static_cast<double>(Step[static_cast<size_t>(J * NX + I)] - Step[static_cast<size_t>(J * NX + I - 1)])));
				}
			}
		}
		Check(Off < 1e-5, "smoothing keeps an even field even");
		Check(Steepest < 0.25, "and a province's step of 1.8 comes out a slope (steepest " + Num(Steepest) + " a point)");
	}

	// ---- the mud ----
	FMudIn Point(double X, double Y, double Water, double Edge, double Height = 1.1, double Rough = 0.2, double Wooded = 0.0)
	{
		FMudIn In;
		In.X = X;
		In.Y = Y;
		In.Water = Water;
		In.Edge = Edge;
		In.Height = Height;
		In.Rough = Rough;
		In.Wooded = Wooded;
		return In;
	}

	void CheckMud()
	{
		// It stops inside the rules' ground: none at the edge or within EdgeStop of it.
		bool bStops = true;
		for (int K = 0; K < 4000; ++K)
		{
			const FMud M = Mud(Point(K * 0.73, K * 0.41, (K % 50) * 1.0, (K % 16) * EdgeStop / 15.0));
			bStops &= M.Wet == 0.0f && M.Pool == 0.0f;
		}
		Check(bStops, "no mud at the edge of the rules' ground or within " + Num(EdgeStop) + " of it");
		// Near the edge it breaks into patches, the fewer the nearer; well inside it lies everywhere.
		auto Covered = [](double E0, double E1)
		{
			int Wet = 0, All = 0;
			for (int K = 0; K < 6000; ++K)
			{
				const double E = E0 + (E1 - E0) * ((K * 37) % 6000) / 6000.0;
				const FMud M = Mud(Point(K * 0.917, K * 0.533 + 40.0, 30.0, E));
				Wet += M.Wet > 0.05f ? 1 : 0;
				++All;
			}
			return static_cast<double>(Wet) / All;
		};
		const double Near = Covered(EdgeStop, EdgeStop + 2.5), Mid = Covered(EdgeStop + 4.0, EdgeStop + 7.0), Deep = Covered(EdgeStop + EdgeReach + 1.0, 80.0);
		Check(Near < 0.35, "within 2.5 of its stop the mud lies in patches only (" + Num(Near) + " covered)");
		Check(Mid > Near + 0.15 && Mid < 0.95, "further in, more of it (" + Num(Mid) + ")");
		Check(Deep > 0.995, "well inside it lies everywhere (" + Num(Deep) + ")");
		// Wettest by the water; low flat ground wetter than hills; the water's side soaks first.
		double WetNear = 0.0, WetFar = 0.0, AriNear = 0.0, AriFar = 0.0, WetLow = 0.0, WetHigh = 0.0, AriLow = 0.0, AriHigh = 0.0;
		float LeastDeep = 1.0f;
		const int N = 3000;
		for (int K = 0; K < N; ++K)
		{
			const double X = K * 1.37, Y = K * 0.71;
			const FMud A = Mud(Point(X, Y, 1.0, 60.0)), B = Mud(Point(X, Y, 70.0, 60.0));
			WetNear += A.Wet; WetFar += B.Wet; AriNear += A.Arrive; AriFar += B.Arrive;
			const FMud L = Mud(Point(X, Y, 30.0, 60.0, 0.9, 0.1)), H = Mud(Point(X, Y, 30.0, 60.0, 2.2, 1.8));
			WetLow += L.Wet; WetHigh += H.Wet; AriLow += L.Arrive; AriHigh += H.Arrive;
			LeastDeep = std::min({ LeastDeep, A.Wet, B.Wet, L.Wet, H.Wet });
		}
		WetNear /= N; WetFar /= N; AriNear /= N; AriFar /= N; WetLow /= N; WetHigh /= N; AriLow /= N; AriHigh /= N;
		Check(WetNear > WetFar + 0.2, "wettest by the water (" + Num(WetNear) + " against " + Num(WetFar) + " far off)");
		Check(WetLow > WetHigh + 0.08, "the low flat ground wetter than the hills (" + Num(WetLow) + " against " + Num(WetHigh) + ")");
		Check(AriNear + 0.35 < AriFar, "the water's side soaks first (" + Num(AriNear) + " against " + Num(AriFar) + ")");
		Check(AriLow + 0.12 < AriHigh, "the low ground soaks before the high (" + Num(AriLow) + " against " + Num(AriHigh) + ")");
		Check(LeastDeep > 0.2f, "deep in the mud ground nowhere is dry: its driest rises still take a thin mud (least " + Num(LeastDeep) + ")");
		// Woods hardly change.
		double WetWood = 0.0, WetOpen = 0.0;
		for (int K = 0; K < N; ++K)
		{
			WetWood += Mud(Point(K * 1.1, K * 0.3, 20.0, 50.0, 1.1, 0.2, 1.0)).Wet;
			WetOpen += Mud(Point(K * 1.1, K * 0.3, 20.0, 50.0, 1.1, 0.2, 0.0)).Wet;
		}
		Check(WetWood < 0.35 * WetOpen, "woods hardly change (" + Num(WetWood / N) + " under woods, " + Num(WetOpen / N) + " in the open)");
	}

	/** A made-up stretch of the east on the game's own grid (cells of 1.5 map units): a winding river, a lake, a rise, the mud ground's edge along one side. */
	struct FBoard
	{
		int NX = 260, NY = 200;
		double Cell = 1.5;
		std::vector<FMud> Mud;
	};

	FBoard MakeBoard()
	{
		FBoard B;
		const size_t Count = static_cast<size_t>(B.NX * B.NY);
		std::vector<float> Water(Count, 1e9f), Edge(Count, 1e9f);
		for (int J = 0; J < B.NY; ++J)
		{
			for (int I = 0; I < B.NX; ++I)
			{
				const size_t Idx = static_cast<size_t>(J * B.NX + I);
				const double RiverJ = 100.0 + 30.0 * std::sin(I * 0.045) + 8.0 * std::sin(I * 0.17);
				if (std::fabs(J - RiverJ) < 0.75 || std::hypot(I - 200.0, J - 40.0) < 9.0)
				{
					Water[Idx] = 0.0f;
				}
				if (I < 6) // (the ground west of here is not the rules' mud ground)
				{
					Edge[Idx] = 0.0f;
				}
			}
		}
		Spread(Water, B.NX, B.NY, 1e9f);
		Spread(Edge, B.NX, B.NY, 1e9f);
		B.Mud.resize(Count);
		for (int J = 0; J < B.NY; ++J)
		{
			for (int I = 0; I < B.NX; ++I)
			{
				const size_t Idx = static_cast<size_t>(J * B.NX + I);
				const double X = I * B.Cell, Y = J * B.Cell;
				const double Height = 1.0 + 0.9 * std::exp(-(std::pow(I - 60.0, 2) + std::pow(J - 160.0, 2)) / 900.0);
				B.Mud[Idx] = TheaterSeasonTurn::Mud(Point(X, Y, Water[Idx] * B.Cell, Edge[Idx] * B.Cell, Height, 0.3));
			}
		}
		return B;
	}

	void CheckBoard(const FBoard& B)
	{
		// "Never show the cells": from one cell to the next the mud changes smoothly, well inside its ground.
		double StepWet = 0.0, StepArrive = 0.0;
		int Pools = 0, Muddy = 0;
		for (int J = 1; J < B.NY; ++J)
		{
			for (int I = 1; I < B.NX; ++I)
			{
				const FMud& M = B.Mud[static_cast<size_t>(J * B.NX + I)];
				const FMud& L = B.Mud[static_cast<size_t>(J * B.NX + I - 1)];
				const FMud& U = B.Mud[static_cast<size_t>((J - 1) * B.NX + I)];
				if (I > 30)
				{
					StepWet = std::max({ StepWet, static_cast<double>(std::fabs(M.Wet - L.Wet)), static_cast<double>(std::fabs(M.Wet - U.Wet)) });
					StepArrive = std::max({ StepArrive, static_cast<double>(std::fabs(M.Arrive - L.Arrive)), static_cast<double>(std::fabs(M.Arrive - U.Arrive)) });
				}
				Muddy += M.Wet > 0.05f ? 1 : 0;
				Pools += M.Pool > 0.3f ? 1 : 0;
			}
		}
		Check(StepWet < 0.12, "no cell edge in the mud: neighbours differ by at most " + Num(StepWet));
		Check(StepArrive < 0.1, "no cell edge in the soak's front: neighbours arrive at most " + Num(StepArrive) + " of the turn apart (" + Num(StepArrive * Spreads) + " s)");
		const double PoolShare = Muddy ? static_cast<double>(Pools) / Muddy : 0.0;
		Check(PoolShare > 0.0005 && PoolShare < 0.03, "a few pools, and few of them (" + Num(PoolShare * 100.0) + "% of the mud)");
	}

	void CheckPaint()
	{
		const uint8_t Steppe[3] = { 150, 138, 100 }, Meadow[3] = { 96, 112, 70 }, Dark[3] = { 40, 44, 36 };
		FMud None;
		uint8_t Out[3];
		const FShown Whole{ 1.0f, 0.0f }, Nothing{ 0.0f, 0.0f };
		PaintMud(Steppe, None, Whole, Out);
		Check(Out[0] == 150 && Out[1] == 138 && Out[2] == 100, "no mud, the ground as it was");
		FMud Some;
		Some.Wet = 0.8f;
		Some.Tone = 1.0f;
		PaintMud(Steppe, Some, Nothing, Out);
		Check(Out[0] == 150 && Out[1] == 138 && Out[2] == 100, "mud not yet reached, the ground as it was");
		// Settled wet earth on the steppe and on a meadow: about the art director's (78, 62, 46), a little lifted by the ground under it.
		double Sum[3] = { 0.0, 0.0, 0.0 };
		uint8_t Least = 255, Most = 0;
		const int N = 2000;
		for (int K = 0; K < N; ++K)
		{
			const FMud M = Mud(Point(K * 2.3 + 500.0, K * 1.1, 25.0, 60.0));
			PaintMud(K % 2 ? Steppe : Meadow, M, Whole, Out);
			for (int C = 0; C < 3; ++C)
			{
				Sum[C] += Out[C];
				Least = std::min(Least, Out[C]);
				Most = std::max(Most, Out[C]);
			}
		}
		const double R = Sum[0] / N, G = Sum[1] / N, B = Sum[2] / N;
		Check(std::fabs(R - 78.0) < 16.0 && std::fabs(G - 62.0) < 16.0 && std::fabs(B - 46.0) < 16.0,
			"settled mud about (78, 62, 46): (" + Num(R) + ", " + Num(G) + ", " + Num(B) + ")");
		Check(R > G && G > B, "it is a brown, warm to cool as earth is");
		Check(Least >= 30, "never black (darkest channel " + std::to_string(Least) + ")");
		Check(Most - Least > 25, "never one smooth brown: tones from " + std::to_string(Least) + " to " + std::to_string(Most));
		// The wettest by the water is darker than the drying edge, and the drying edge paler than wet earth.
		FMud Wettest, Dry;
		Wettest.Wet = 1.0f;
		Dry.Wet = 0.3f;
		uint8_t A[3], C[3];
		PaintMud(Steppe, Wettest, Whole, A);
		PaintMud(Steppe, Dry, Whole, C);
		Check(Luma(A) + 15.0 < Luma(C), "the wettest is darker than the drying edge (" + Num(Luma(A)) + " against " + Num(Luma(C)) + ")");
		// The wet band at the front: darker, never black, and gone at settled.
		uint8_t Banded[3];
		PaintMud(Steppe, Some, FShown{ 1.0f, 1.0f }, Banded);
		PaintMud(Steppe, Some, Whole, Out);
		Check(Luma(Banded) + 5.0 < Luma(Out) && Banded[2] >= 28, "the band is darker and wetter, never black (" + Num(Luma(Banded)) + " against " + Num(Luma(Out)) + ")");
		// A pool: a dull grey-blue, never a white speck.
		FMud Pool;
		Pool.Wet = 0.9f;
		Pool.Pool = 1.0f;
		PaintMud(Steppe, Pool, Whole, Out);
		Check(Out[2] >= Out[0] && Out[0] < 150 && Out[2] < 160, "a pool is a dull sky, never white (" + std::to_string(Out[0]) + ", " + std::to_string(Out[1]) + ", " + std::to_string(Out[2]) + ")");
		// Dark ground stays dark ground, not lifted to mud's brightness by much.
		PaintMud(Dark, Some, Whole, Out);
		Check(Luma(Out) < 80.0, "dark ground stays dark");
		// Half way through the turn, half way to the mud (in light).
		uint8_t Half[3];
		PaintMud(Steppe, Some, FShown{ 0.5f, 0.0f }, Half);
		Check(Luma(Half) < 138.0 && Luma(Half) > Luma(Out), "half shown lies between the ground and the mud");
	}

	/** The quick mix the map paints every frame of a turn with, against the paint itself. */
	void CheckQuickMix()
	{
		uint32_t Seed = 12345u;
		auto Next = [&Seed]() -> double
		{
			Seed = Seed * 1664525u + 1013904223u;
			return static_cast<double>(Seed >> 8) / 16777216.0;
		};
		int WorstEnds = 0, Worst = 0, WorstPool = 0;
		for (int Trial = 0; Trial < 4000; ++Trial)
		{
			// (the table's own ground: greens, browns and the steppe's tan)
			const uint8_t Dry[3] = { static_cast<uint8_t>(70.0 + 110.0 * Next()), static_cast<uint8_t>(80.0 + 90.0 * Next()), static_cast<uint8_t>(45.0 + 85.0 * Next()) };
			FMud M;
			M.Wet = static_cast<float>(0.05 + 0.95 * Next());
			M.Pool = Next() < 0.2 ? static_cast<float>(Next()) : 0.0f;
			M.Tone = static_cast<float>(0.9 + 0.2 * Next());
			uint8_t Whole[3], Banded[3], Exact[3], Quick[3];
			PaintMud(Dry, M, FShown{ 1.0f, 0.0f }, Whole);
			PaintMud(Dry, M, FShown{ 1.0f, 1.0f }, Banded);
			const float Shares[] = { 0.0f, 0.15f, 0.35f, 0.5f, 0.65f, 0.85f, 1.0f };
			const float Bands[] = { 0.0f, 0.3f, 0.7f, 1.0f };
			for (float Share : Shares)
			{
				for (float Band : Bands)
				{
					PaintMud(Dry, M, FShown{ Share, Band }, Exact);
					MixMud(Dry, Whole, Banded, FShown{ Share, Band }, Quick);
					const bool bEnd = Share == 0.0f || (Share == 1.0f && (Band == 0.0f || Band == 1.0f));
					for (int K = 0; K < 3; ++K)
					{
						const int Off = std::abs(static_cast<int>(Exact[K]) - static_cast<int>(Quick[K]));
						int& Into = bEnd ? WorstEnds : M.Pool > 0.0f ? WorstPool : Worst;
						Into = std::max(Into, Off);
					}
				}
			}
		}
		Check(WorstEnds <= 1, "the quick mix is the paint itself at the ends of a turn (off by at most " + std::to_string(WorstEnds) + " of 255)");
		// (On the way it runs a little light: the band's own cover is mixed in with its colour. A cell is in the front
		// for about a quarter of a second.)
		Check(Worst <= 14, "and near it on the way, in the front's quarter second (off by at most " + std::to_string(Worst) + " of 255)");
		Check(WorstPool <= 20, "a pool comes in a moment behind the mud round it (off by at most " + std::to_string(WorstPool) + " of 255)");
	}

	void CheckSnow()
	{
		double NorthComing = 0.0, SouthComing = 0.0, HighComing = 0.0, LowComing = 0.0;
		double NorthGoing = 0.0, SouthGoing = 0.0, HighGoing = 0.0, LowGoing = 0.0;
		bool bRange = true;
		for (int K = 0; K < 1000; ++K)
		{
			const double G = Noise(K * 0.31, K * 0.17, 5u);
			NorthComing += SnowArrive(0.05, 0.3, G, true);
			SouthComing += SnowArrive(0.95, 0.3, G, true);
			HighComing += SnowArrive(0.5, 0.95, G, true);
			LowComing += SnowArrive(0.5, 0.05, G, true);
			NorthGoing += SnowArrive(0.05, 0.3, G, false);
			SouthGoing += SnowArrive(0.95, 0.3, G, false);
			HighGoing += SnowArrive(0.5, 0.95, G, false);
			LowGoing += SnowArrive(0.5, 0.05, G, false);
			for (double S = -0.5; S <= 1.5; S += 0.25)
			{
				const float A = SnowArrive(S, 1.0 - S, G, K % 2 == 0);
				bRange &= A >= 0.0f && A <= 1.0f;
			}
		}
		Check(NorthComing + 400.0 < SouthComing, "coming snow comes from the north first");
		Check(HighComing + 150.0 < LowComing, "...and from high ground first");
		Check(SouthGoing + 400.0 < NorthGoing, "going snow goes from the south first");
		Check(LowGoing + 150.0 < HighGoing, "...and from the low ground first");
		Check(bRange, "an arrival is always between 0 and 1");
		Check(SnowNow(20, 200, FShown{ 0.0f, 0.0f }) == 20 && SnowNow(20, 200, FShown{ 1.0f, 0.0f }) == 200 && SnowNow(200, 20, FShown{ 0.5f, 0.0f }) == 110, "snow shows between what lay and what will");
	}

	// ---- drawings over the game's own east (--map): the mud ground, its rivers and heights, as draw_east.py lays them ----
	int Draw(const char* MapPath, const char* OutDir)
	{
		FILE* In = std::fopen(MapPath, "rb");
		if (!In)
		{
			std::printf("No board at %s\n", MapPath);
			return 1;
		}
		int Size[2] = { 0, 0 };
		double Cell = 1.5;
		if (std::fread(Size, sizeof(int), 2, In) != 2 || std::fread(&Cell, sizeof(double), 1, In) != 1)
		{
			std::fclose(In);
			return 1;
		}
		const int NX = Size[0], NY = Size[1];
		const size_t Count = static_cast<size_t>(NX) * static_cast<size_t>(NY);
		// per cell: ground RGB (3 bytes), mud ground (1 byte: 1 mud, 0 land without mud, 2 water), height (float), rough (float), wooded (byte)
		std::vector<uint8_t> Ground(Count * 3), Kind(Count), Wooded(Count);
		std::vector<float> Height(Count), Rough(Count);
		bool bRead = std::fread(Ground.data(), 1, Count * 3, In) == Count * 3 && std::fread(Kind.data(), 1, Count, In) == Count
			&& std::fread(Height.data(), sizeof(float), Count, In) == Count && std::fread(Rough.data(), sizeof(float), Count, In) == Count
			&& std::fread(Wooded.data(), 1, Count, In) == Count;
		std::fclose(In);
		if (!bRead)
		{
			std::printf("The board at %s is short\n", MapPath);
			return 1;
		}
		std::vector<float> Water(Count, 1e9f), Edge(Count, 1e9f);
		for (size_t K = 0; K < Count; ++K)
		{
			Water[K] = Kind[K] == 2 ? 0.0f : 1e9f;
			Edge[K] = Kind[K] == 0 ? 0.0f : 1e9f;
		}
		Spread(Water, NX, NY, 1e9f);
		Spread(Edge, NX, NY, 1e9f);
		// (as the table has them: its heights smoothed over a few units by the turn, its roughness over ten by the terrain)
		Blur(Height, NX, NY, 3);
		Blur(Rough, NX, NY, static_cast<int>(std::lround(10.0 / Cell)));
		std::vector<FMud> Muds(Count);
		for (int J = 0; J < NY; ++J)
		{
			for (int I = 0; I < NX; ++I)
			{
				const size_t K = static_cast<size_t>(J) * static_cast<size_t>(NX) + static_cast<size_t>(I);
				if (Kind[K] != 1)
				{
					continue;
				}
				FMudIn P = Point(I * Cell, J * Cell, Water[K] * Cell, Edge[K] * Cell, Height[K], Rough[K], Wooded[K] / 255.0);
				Muds[K] = Mud(P);
			}
		}
		// The arrivals spread evenly over the turn, as the table spreads them.
		{
			std::vector<float> Arrive;
			for (size_t K = 0; K < Count; ++K)
			{
				if (Kind[K] == 1)
				{
					Arrive.push_back(Muds[K].Arrive);
				}
			}
			Even(Arrive);
			size_t Next = 0;
			for (size_t K = 0; K < Count; ++K)
			{
				if (Kind[K] == 1)
				{
					Muds[K].Arrive = Arrive[Next++];
				}
			}
		}
		const double Times[] = { 0.0, 1.0, 2.0, Spreads + Settles };
		const char* Names[] = { "0-before", "1-one-second", "2-two-seconds", "3-settled" };
		for (int T = 0; T < 4; ++T)
		{
			const double F = Front(Times[T]);
			std::string Path = std::string(OutDir) + "/mud-" + Names[T] + ".ppm";
			FILE* Out = std::fopen(Path.c_str(), "wb");
			if (!Out)
			{
				return 1;
			}
			std::fprintf(Out, "P6\n%d %d\n255\n", NX, NY);
			std::vector<uint8_t> Row(static_cast<size_t>(NX) * 3);
			for (int J = 0; J < NY; ++J)
			{
				for (int I = 0; I < NX; ++I)
				{
					const size_t K = static_cast<size_t>(J) * static_cast<size_t>(NX) + static_cast<size_t>(I);
					uint8_t* C = &Row[static_cast<size_t>(I) * 3];
					if (Kind[K] == 1)
					{
						PaintMud(&Ground[K * 3], Muds[K], At(Muds[K].Arrive, F), C);
					}
					else
					{
						std::memcpy(C, &Ground[K * 3], 3);
					}
				}
				std::fwrite(Row.data(), 1, Row.size(), Out);
			}
			std::fclose(Out);
			std::printf("drew %s\n", Path.c_str());
		}
		return 0;
	}
}

int main(int Count, char** Words)
{
	if (Count == 4 && std::strcmp(Words[1], "--map") == 0)
	{
		return Draw(Words[2], Words[3]);
	}
	CheckClock();
	CheckNoise();
	CheckDistance();
	CheckEvenAndBlur();
	CheckMud();
	CheckBoard(MakeBoard());
	CheckPaint();
	CheckQuickMix();
	CheckSnow();
	std::printf("season turn: %d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
