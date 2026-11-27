// Churned ground between dug-in lines on the campaign map: the cloud's checks on Campaign/FrontChurn (see run.sh).
// With "--map FILE OUTDIR" it also paints drawings of it over the game's own ground (see draw_churn.py), with the
// fronts chained into lines by the game's own Campaign/FrontLine, as the map chains them.
#include "Campaign/FrontChurn.h"
#include "Campaign/FrontLine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace TheaterChurn;

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

	double ToLight(double Eye)
	{
		const double C = Eye / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}
	double ToEye(double Light)
	{
		const double C = std::max(0.0, Light);
		return 255.0 * (C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055);
	}
	double Lstar(const uint8_t C[3])
	{
		const double Y = 0.2126 * ToLight(C[0]) + 0.7152 * ToLight(C[1]) + 0.0722 * ToLight(C[2]);
		return Y > 0.008856 ? 116.0 * std::cbrt(Y) - 16.0 : 903.3 * Y;
	}

	/** A straight line along X at Y = 0, from X0 to X1, the armies facing at its middle, its state the same along it. */
	FLine Straight(double X0, double X1, const FState& State, double FacingAt)
	{
		FLine Line;
		for (double X = X0; X <= X1 + 1e-9; X += 0.5)
		{
			Line.X.push_back(X);
			Line.Y.push_back(0.0);
			Line.Facing.push_back(static_cast<float>(std::fabs(X - FacingAt)));
			Line.State.push_back(State);
		}
		return Line;
	}

	FBand BandAt(double X, double D, double Facing, const FState& State)
	{
		FBandIn In;
		In.X = X;
		In.Y = D;
		In.D = std::fabs(D);
		In.Side = D;
		In.bSided = true;
		In.bFoot = true;
		In.FootX = X;
		In.FootY = 0.0;
		In.Facing = Facing;
		In.State = State;
		return Band(In);
	}

	// ---- the months ----
	void CheckMonths()
	{
		const double Want[] = { 0.0, 0.0, 0.3, 0.5, 0.7, 0.85, 1.0, 1.0 };
		bool bTable = true;
		for (int M = 0; M <= 7; ++M)
		{
			bTable &= std::fabs(Grown(M) - Want[M]) < 1e-9;
		}
		Check(bTable, "grows by the art director's table: nothing in month 1, 0.3, 0.5, 0.7, 0.85, whole at 6");
		Check(Grown(1.5) > 0.0 && Grown(1.5) < 0.3, "and evenly between the months");
		Check(std::fabs(Left(0.0) - 1.0) < 1e-9 && std::fabs(Left(4.0) - 0.5) < 1e-9 && std::fabs(Left(8.0) - 0.25) < 1e-9 && Left(12.0) == 0.0 && Left(20.0) == 0.0,
			"heals: half at 4 months, three quarters at 8, gone at 12");
		Check(CratersLeft(4.0) < Left(4.0) && CratersLeft(6.0) == 0.0, "the craters green over first (" + Num(CratersLeft(4.0)) + " left at 4 months)");
		const FState Whole = Growing(6.0), Healed = Healing(Whole, 8.0);
		Check(Whole.Size == 1.0f && Whole.Pale == 0.0f, "whole at six months, not pale");
		Check(Healed.Size < 0.3f && Healed.Pits == 0.0f && Healed.Pale > 0.7f, "healing: weaker, no craters, a paler strip");
		Check(IsNothing(Growing(1.0)) && IsNothing(Healing(Whole, 12.0)), "nothing in the first month, nothing after a year");
		const FState Half = Mix(Growing(2.0), Growing(4.0), 0.5);
		Check(std::fabs(Half.Size - 0.5f) < 1e-6f, "the month's cross-fade runs evenly between the two");
	}

	// ---- the band's width ----
	void CheckWidth()
	{
		double Widest = 0.0, SumCore = 0.0, SumFirst = 0.0;
		double LeastHalf = 1e9, MostHalf = 0.0;
		int Count = 0;
		const FState Whole = Growing(6.0), First = Growing(2.0);
		for (int K = 0; K < 4000; ++K)
		{
			const double X = K * 0.05;
			const FBand B = BandAt(X, 0.0, 3.0, Whole);
			// where does the body end, either side?
			double Lo = 0.0, Hi = 0.0;
			for (double D = -3.5; D < 3.5; D += 0.01)
			{
				if (BandAt(X, D, 3.0, Whole).Body > 0.0f)
				{
					Lo = std::min(Lo, D);
					Hi = std::max(Hi, D);
				}
			}
			Widest = std::max(Widest, Hi - Lo);
			SumCore += 2.0 * B.HalfCore;
			SumFirst += 2.0 * BandAt(X, 0.0, 3.0, First).HalfCore;
			LeastHalf = std::min<double>(LeastHalf, B.HalfCore);
			MostHalf = std::max<double>(MostHalf, B.HalfCore);
			++Count;
		}
		Check(Widest <= 4.0 + 1e-6, "never over 4 units in all (widest " + Num(Widest) + ")");
		Check(std::fabs(SumCore / Count - 2.4) < 0.3, "whole: the core about 2.4 wide where the armies stand (" + Num(SumCore / Count) + ")");
		Check(std::fabs(SumFirst / Count - 1.0) < 0.2, "first show: the core about 1.0 wide (" + Num(SumFirst / Count) + ")");
		const double Swing = (MostHalf - LeastHalf) / (0.5 * (MostHalf + LeastHalf));
		Check(Swing > 0.25 && Swing < 0.7, "its width wanders by about two fifths (" + Num(Swing) + ")");
		// Strongest where the armies stand; nothing far along the frontier from them.
		const double Far = BandAt(5.0, 0.0, 30.0, Whole).Strength;
		Check(Far > 0.45 && Far < 0.55, "far along a dug-in pair's own border it thins to about half (" + Num(Far) + "), as the fires thin to embers");
		Check(BandAt(5.0, 0.0, 12.0, Whole).Strength < BandAt(5.0, 0.0, 2.0, Whole).Strength, "tapering away from where they face each other");
		// Weak is broken, never thin: wherever it shows, its body is at least about 1.4 wide.
		double Thinnest = 1e9;
		for (const double Size : { 0.05, 0.15, 0.3, 0.6, 1.0 })
		{
			FState Weak = Whole;
			Weak.Size = Weak.Dark = Weak.Pits = static_cast<float>(Size);
			for (int K = 0; K < 1500; ++K)
			{
				const double X = K * 0.13;
				if (BandAt(X, 0.0, 3.0, Weak).Whole < 0.5f)
				{
					continue;
				}
				double Lo = 0.0, Hi = 0.0;
				for (double D = -3.0; D <= 3.0; D += 0.01)
				{
					FBandIn In;
					In.X = X;
					In.Y = D;
					In.D = std::fabs(D);
					In.Side = D;
					In.bSided = true;
					In.bFoot = true;
					In.FootX = In.X;
					In.FootY = 0.0;
					In.Facing = 3.0;
					In.State = Weak;
					const FBand B = Band(In);
					if (B.Body >= 0.5f * B.Whole)
					{
						Lo = std::min(Lo, D);
						Hi = std::max(Hi, D);
					}
				}
				Thinnest = std::min(Thinnest, Hi - Lo);
			}
		}
		Check(Thinnest >= Narrowest - 0.05, "weak is broken, never thin: wherever it shows it is at least about 1.4 wide (thinnest " + Num(Thinnest) + ")");
		// Weaker shows as patches two to five units long with gaps between; whole, it is unbroken.
		auto Patches = [&](const FState& State, double Facing, double& Covered, double& Typical)
		{
			std::vector<double> Runs;
			double Run = 0.0, On = 0.0;
			const double Step = 0.05;
			int Samples = 0;
			for (int K = 0; K < 8000; ++K)
			{
				const bool bOn = BandAt(K * Step, 0.0, Facing, State).Whole >= 0.5f;
				On += bOn ? 1.0 : 0.0;
				++Samples;
				if (bOn)
				{
					Run += Step;
				}
				else if (Run > 0.0)
				{
					Runs.push_back(Run);
					Run = 0.0;
				}
			}
			Covered = On / Samples;
			std::sort(Runs.begin(), Runs.end());
			Typical = Runs.empty() ? 0.0 : Runs[Runs.size() / 2];
		};
		double Covered = 0.0, Typical = 0.0;
		Patches(Whole, 3.0, Covered, Typical);
		Check(Covered > 0.995, "whole where the armies stand it is unbroken (" + Num(Covered) + ")");
		Patches(First, 3.0, Covered, Typical);
		Check(Covered > 0.3 && Covered < 0.8 && Typical >= 1.5 && Typical <= 6.0, "at first show it is patches (" + Num(Covered) + " laid, patches about " + Num(Typical) + " long)");
		Patches(Healing(Whole, 8.0), 3.0, Covered, Typical);
		Check(Covered > 0.15 && Covered < 0.6, "eight months healed it is islands (" + Num(Covered) + " left)");
		Patches(Healing(Whole, 3.0), 3.0, Covered, Typical);
		Check(Covered > 0.85, "three months healed it is still whole (" + Num(Covered) + ")");
		// Its middle strays off the frontier by up to half a unit.
		double Stray0 = 0.0;
		for (int K = 0; K < 4000; ++K)
		{
			Stray0 = std::max(Stray0, std::fabs(static_cast<double>(BandAt(K * 0.1, 0.0, 3.0, Whole).Middle)));
		}
		Check(Stray0 > 0.3 && Stray0 <= Stray + 1e-6, "its middle strays off the frontier by up to half a unit (most " + Num(Stray0) + ")");
		// And it shows only where the whole band is four pixels wide or more on the screen.
		Check(Shown(2.4) == 0.0 && Shown(14.0) == 1.0 && Shown(4.0) > 0.2 && Shown(4.0) < 0.8, "from theatre height (the band about 2.4 pixels) it fades out; from working height (about 14) it shows");
		Check(CraterShown(2.0) == 0.0 && CraterShown(6.5) == 1.0, "a crater draws only from three pixels across, whole from six");
	}

	// ---- the ragged edge ----
	void CheckEdge()
	{
		// Along a straight frontier, the fine edge (where the ragged body crosses a half), on the left side.
		const FState Whole = Growing(6.0);
		std::vector<double> EdgeAt;
		const double Step = 0.05;
		auto RaggedAt = [&](double X, double D)
		{
			FBandIn In;
			In.X = X;
			In.Y = D;
			In.D = std::fabs(D);
			In.Side = D;
			In.bSided = true;
			In.bFoot = true;
			In.FootX = In.X;
			In.FootY = 0.0;
			In.Along = X;
			In.Facing = 3.0;
			In.State = Whole;
			return static_cast<double>(Band(In).Ragged);
		};
		double Softest = 0.0;
		for (int K = 0; K < 3000; ++K)
		{
			const double X = K * Step;
			double Last = 0.0, From = -1.0, To = -1.0;
			for (double D = 0.0; D < 3.2; D += 0.005)
			{
				const double R = RaggedAt(X, D);
				if (R >= 0.5)
				{
					Last = D;
				}
				if (R >= 0.9)
				{
					From = D;
				}
				if (R >= 0.1)
				{
					To = D;
				}
			}
			EdgeAt.push_back(Last);
			Softest = std::max(Softest, To - From);
		}
		// a straight stretch: the edge within 0.06 units of the straight line between its two ends
		double LongestStraight = 0.0;
		for (size_t A = 0; A < EdgeAt.size(); A += 5)
		{
			for (size_t B = A + 20; B < EdgeAt.size(); B += 5)
			{
				bool bStraight = true;
				for (size_t K = A; K <= B && bStraight; ++K)
				{
					const double T = static_cast<double>(K - A) / static_cast<double>(B - A);
					bStraight = std::fabs(EdgeAt[K] - (EdgeAt[A] + (EdgeAt[B] - EdgeAt[A]) * T)) < 0.06;
				}
				if (!bStraight)
				{
					break;
				}
				LongestStraight = std::max(LongestStraight, static_cast<double>(B - A) * Step);
			}
		}
		Check(LongestStraight <= 3.0, "no stretch of the edge runs straight for more than about 3 units (longest " + Num(LongestStraight) + ")");
		Check(Softest < 0.3, "the veil's edge is crisp (it falls from nine tenths to one tenth within " + Num(Softest) + " of a unit)");
		// Tongues: reaching out from the edge as it lies over three units, 0.3 to 0.8 of a unit, a few of them in every ten units.
		double Longest = 0.0;
		int Tongues = 0;
		bool bIn = false;
		for (size_t K = 30; K + 30 < EdgeAt.size(); ++K)
		{
			std::vector<double> Round(EdgeAt.begin() + static_cast<long>(K) - 30, EdgeAt.begin() + static_cast<long>(K) + 31);
			std::nth_element(Round.begin(), Round.begin() + 30, Round.end());
			const double Out = EdgeAt[K] - Round[30];
			Longest = std::max(Longest, Out);
			if (Out >= 0.3 && !bIn)
			{
				++Tongues;
			}
			bIn = Out >= 0.2;
		}
		const double Per10 = Tongues * 10.0 / (static_cast<double>(EdgeAt.size()) * Step);
		Check(Longest >= 0.3 && Longest <= TongueLongest + 0.1, "the veil's tongues reach out up to about 0.8 of a unit (longest " + Num(Longest) + ")");
		Check(Per10 >= 1.0 && Per10 <= 12.0, "uneven, a few in every ten units (" + Num(Per10) + ")");
	}

	// ---- the ground picture ----
	void CheckPaint()
	{
		const FState Whole = Growing(6.0);
		const uint8_t Grounds[][3] = { { 0xA8, 0xA7, 0x77 }, { 0x6C, 0x7F, 0x55 }, { 0xA3, 0x96, 0x70 }, { 0x7C, 0x8F, 0x78 }, { 160, 160, 120 }, { 90, 110, 70 } };
		double WorstShare = 0.0, LeastBrown = 1e9;
		bool bKept = true;
		for (const auto& G : Grounds)
		{
			for (int K = 0; K < 200; ++K)
			{
				const double X = K * 0.37;
				const FBand B = BandAt(X, 0.0, 3.0, Whole);
				uint8_t Out[3];
				PaintCell(G, B, Whole, EGround::Common, ESeason::Dry, X, 0.0, Out);
				// (and the fine veil over it: the two together)
				const double V = Veil(B, Whole, X, 0.0);
				uint8_t Seen[3];
				for (int C = 0; C < 3; ++C)
				{
					Seen[C] = static_cast<uint8_t>(std::lround(ToEye(ToLight(Out[C]) * (1.0 - V))));
				}
				const double Share = 1.0 - Lstar(Seen) / Lstar(G);
				WorstShare = std::max(WorstShare, Share);
				auto Brown = [](const uint8_t C[3]) { return (static_cast<double>(C[0]) - C[2]) / (static_cast<double>(C[0]) + C[1] + C[2]); };
				LeastBrown = std::min(LeastBrown, Brown(Out) - Brown(G));
			}
		}
		Check(WorstShare <= MostDarker + 0.006, "the core, picture and veil together, is at most a quarter darker than its ground (worst " + Num(WorstShare) + ")");
		Check(LeastBrown > 0.0, "and browner than it (red over blue, as a share of the whole, up by at least " + Num(LeastBrown) + ")");
		// Healing: the summed darkness against the bare ground about half at four months and a quarter at eight (the art director's read of the second drawings: 8 months was a tenth).
		{
			const uint8_t G[3] = { 0xA8, 0xA7, 0x77 };
			auto Summed = [&](const FState& State)
			{
				double Sum = 0.0;
				for (int K = 0; K < 1600; ++K)
				{
					for (double D = -3.0; D <= 3.0; D += 0.1)
					{
						const double X = K * 0.25;
						const FBand B = BandAt(X, D, 3.0, State);
						uint8_t Out[3];
						PaintCell(G, B, State, EGround::Common, ESeason::Dry, X, D, Out);
						const double V = Veil(B, State, X, D);
						uint8_t Seen[3];
						for (int C = 0; C < 3; ++C)
						{
							Seen[C] = static_cast<uint8_t>(std::lround(ToEye(ToLight(Out[C]) * (1.0 - V))));
						}
						Sum += 1.0 - Lstar(Seen) / Lstar(G);
					}
				}
				return Sum;
			};
			const double Was = Summed(Whole), At4 = Summed(Healing(Whole, 4.0)) / Was, At8 = Summed(Healing(Whole, 8.0)) / Was;
			Check(At4 > 0.35 && At4 < 0.8 && At8 > 0.15 && At8 < 0.4, "healing: about half the darkness left at four months (" + Num(At4) + ") and a quarter at eight (" + Num(At8) + ")");
		}
		// Plots still show through as darker versions of themselves: two grounds a field apart stay apart.
		{
			const FBand B = BandAt(1.0, 0.0, 3.0, Whole);
			const uint8_t Light[3] = { 168, 167, 119 }, Darker[3] = { 120, 128, 85 };
			uint8_t A[3], C[3];
			PaintCell(Light, B, Whole, EGround::Common, ESeason::Dry, 1.0, 0.0, A);
			PaintCell(Darker, B, Whole, EGround::Common, ESeason::Dry, 1.0, 0.0, C);
			const double Was = Lstar(Light) - Lstar(Darker), Now = Lstar(A) - Lstar(C);
			bKept = Now > 0.12 * Was;
			Check(bKept, "two fields under the core still differ (" + Num(Now) + " of " + Num(Was) + " in lightness)");
		}
		// Mottled: never one smooth brown.
		{
			double Sum = 0.0, Sum2 = 0.0;
			int Count = 0;
			const uint8_t G[3] = { 0xA8, 0xA7, 0x77 };
			for (int K = 0; K < 400; ++K)
			{
				const double X = K * 0.29, Y = 0.1 * std::sin(K * 0.7);
				const FBand B = BandAt(X, Y, 3.0, Whole);
				if (B.Core < 0.99f)
				{
					continue;
				}
				uint8_t Out[3];
				PaintCell(G, B, Whole, EGround::Common, ESeason::Dry, X, Y, Out);
				const double L = Lstar(Out);
				Sum += L;
				Sum2 += L * L;
				++Count;
			}
			const double Spread = Count > 1 ? std::sqrt(std::max(0.0, Sum2 / Count - (Sum / Count) * (Sum / Count))) : 0.0;
			Check(Count > 50 && Spread > 1.2, "the core is mottled, never one smooth brown (spread " + Num(Spread) + " in lightness)");
		}
		// Nothing changes off the band, or at month 1.
		{
			const uint8_t G[3] = { 0xA8, 0xA7, 0x77 };
			uint8_t Out[3];
			PaintCell(G, BandAt(1.0, 3.5, 3.0, Whole), Whole, EGround::Common, ESeason::Dry, 1.0, 3.5, Out);
			bool bSame = Out[0] == G[0] && Out[1] == G[1] && Out[2] == G[2];
			PaintCell(G, BandAt(1.0, 0.0, 3.0, Growing(1.0)), Growing(1.0), EGround::Common, ESeason::Dry, 1.0, 0.0, Out);
			bSame &= Out[0] == G[0] && Out[1] == G[1] && Out[2] == G[2];
			Check(bSame, "off the band, and in the first month, the ground is untouched");
		}
		// Woods thin in the core and keep in the edge; snow breaks in the core.
		{
			const FBand Core = BandAt(1.0, 0.0, 3.0, Whole);
			FBand Edge = Core;
			for (double D = 0.0; D < 2.0; D += 0.01)
			{
				const FBand B = BandAt(1.0, D, 3.0, Whole);
				if (B.Core < 0.02f && B.Body > 0.2f)
				{
					Edge = B;
					break;
				}
			}
			Check(Wooded(255, Core, Whole) < 90 && Wooded(255, Edge, Whole) > 245, "the woods thin out in the core and keep in the soft edge");
			Check(Snow(255, Core, Whole) < 120 && Snow(255, Edge, Whole) > 245, "the snow breaks in the core, and closes in clean at the edge");
		}
		// Mud and the desert take their own tones.
		{
			const FBand B = BandAt(1.0, 0.0, 3.0, Whole);
			const uint8_t G[3] = { 90, 74, 56 }, Sand[3] = { 0xD3, 0xBC, 0x86 }; // (in the mud months the band lies on the mud's own colour)
			uint8_t Mud[3], Dry[3], Desert[3];
			PaintCell(G, B, Whole, EGround::Common, ESeason::Mud, 1.0, 0.0, Mud);
			PaintCell(G, B, Whole, EGround::Common, ESeason::Dry, 1.0, 0.0, Dry);
			PaintCell(Sand, B, Whole, EGround::Desert, ESeason::Dry, 1.0, 0.0, Desert);
			Check(Lstar(Mud) < Lstar(Dry), "in the mud months the band is the mud's wetter, darker earth");
			Check(Lstar(Desert) > Lstar(Dry) && Lstar(Desert) >= 0.75 * Lstar(Sand) - 0.6, "a desert front is churned sand, paler than its ground is dark");
		}
	}

	// ---- the fine veil and the craters ----
	void CheckVeilAndPits()
	{
		const FState Whole = Growing(6.0);
		float Most = 0.0f;
		bool bOff = true;
		for (int K = 0; K < 2000; ++K)
		{
			const double X = K * 0.11;
			Most = std::max(Most, Veil(BandAt(X, 0.0, 3.0, Whole), Whole, X, 0.0));
			bOff &= Veil(BandAt(X, 3.0, 3.0, Whole), Whole, X, 3.0) == 0.0f;
		}
		Check(Most > 0.1f && Most <= static_cast<float>(VeilMost), "the fine veil darkens a little and no more (most " + Num(Most) + ")");
		Check(bOff, "and nothing off the band");

		const FLine Line = Straight(-40.0, 40.0, Whole, 0.0);
		std::vector<FPit> All, Holes, Flecks;
		Pits(Line, All);
		std::vector<FPit> Again;
		Pits(Line, Again);
		bool bSame = All.size() == Again.size();
		for (size_t K = 0; bSame && K < All.size(); ++K)
		{
			bSame = All[K].X == Again[K].X && All[K].Y == Again[K].Y && All[K].Radius == Again[K].Radius;
		}
		for (const FPit& Hole : All)
		{
			(Hole.bSpoil ? Flecks : Holes).push_back(Hole);
		}
		Check(bSame && !Holes.empty(), "the craters are the same each time (" + std::to_string(Holes.size()) + " along 80 units)");
		double Small = 1e9, Big = 0.0;
		bool bInCore = true, bFlecksOut = true;
		for (const FPit& Hole : Holes)
		{
			Small = std::min(Small, 2.0 * Hole.Radius);
			Big = std::max(Big, 2.0 * Hole.Radius);
			const FBand B = BandAt(Hole.X, 0.0, std::fabs(Hole.X), Whole);
			bInCore &= std::fabs(Hole.Y - B.Middle) + Hole.Radius <= B.HalfCore + 0.12;
		}
		for (const FPit& Fleck : Flecks)
		{
			const FBand B = BandAt(Fleck.X, 0.0, std::fabs(Fleck.X), Whole);
			bFlecksOut &= std::fabs(Fleck.Y - B.Middle) >= B.HalfCore - 0.05;
		}
		Check(Small >= 0.15 - 1e-9 && Big <= 0.5 + 1e-9 && Big / Small > 2.5, "craters 0.15 to 0.5 across, about three to one (" + Num(Small) + " to " + Num(Big) + ")");
		Check(bInCore, "none in the soft edge");
		Check(!Flecks.empty() && Flecks.size() * 3 < Holes.size() && bFlecksOut, "a few flecks of dry spoil toward the edges (" + std::to_string(Flecks.size()) + ")");
		// Where whole and the armies stand (within 8 units of them): the pit floors about 7% of the band at most, in clusters.
		double Floor = 0.0, Laid = 0.0;
		int Windows = 0, Empty = 0, Dense = 0;
		for (double X = -8.0; X < 8.0; X += 0.05)
		{
			for (double Y = -2.2; Y <= 2.2; Y += 0.05)
			{
				FBandIn In;
				In.X = X;
				In.Y = Y;
				In.D = std::fabs(Y);
				In.Side = Y;
				In.bSided = true;
				In.bFoot = true;
				In.FootX = In.X;
				In.FootY = 0.0;
				In.Facing = std::fabs(X);
				In.State = Whole;
				if (Band(In).Body < 0.5f)
				{
					continue;
				}
				Laid += 1.0;
				for (const FPit& Hole : Holes)
				{
					const double Floor2 = 0.62 * Hole.Radius * 0.62 * Hole.Radius;
					if ((Hole.X - X) * (Hole.X - X) + (Hole.Y - Y) * (Hole.Y - Y) <= Floor2)
					{
						Floor += 1.0;
						break;
					}
				}
			}
		}
		for (double X = -30.0; X < 30.0; X += 1.0)
		{
			int In = 0;
			for (const FPit& Hole : Holes)
			{
				In += Hole.X >= X && Hole.X < X + 1.0 ? 1 : 0;
			}
			++Windows;
			Empty += In == 0 ? 1 : 0;
			Dense += In >= 10 ? 1 : 0;
		}
		Check(Floor / Laid > 0.03 && Floor / Laid <= 0.08, "the pit floors are about 7% of the band at most where it is whole (" + Num(Floor / Laid) + ")");
		Check(Empty * 5 >= Windows && Dense > 0, "in clusters with bare stretches between, never an even pepper (" + std::to_string(Empty) + " of " + std::to_string(Windows) + " units bare)");
		// Healing and weak ground: the craters go first, and a weak band has fewer.
		std::vector<FPit> Later, Weak;
		Pits(Straight(-40.0, 40.0, Healing(Whole, 4.0), 0.0), Later);
		Pits(Straight(-40.0, 40.0, Growing(2.0), 0.0), Weak);
		auto CratersIn = [](const std::vector<FPit>& Set) { return static_cast<size_t>(std::count_if(Set.begin(), Set.end(), [](const FPit& P) { return !P.bSpoil; })); };
		Check(CratersIn(Later) * 4 < Holes.size(), "four months after the front moved on, few craters are left (" + std::to_string(CratersIn(Later)) + ")");
		Check(CratersIn(Weak) * 4 < Holes.size(), "at first show, few craters (" + std::to_string(CratersIn(Weak)) + ")");
		// A crater: its sunny rim 7D6748, its pit 4A3B2B, the shaded third of a large one 2B231B, never black; in the mud months a few pools, only in large pits.
		uint8_t Rim[3], Bottom[3], Shade[3];
		FPit SmallPit = Holes.front(), LargePit = Holes.front();
		for (const FPit& Hole : Holes)
		{
			SmallPit = Hole.Radius < SmallPit.Radius ? Hole : SmallPit;
			LargePit = Hole.Radius > LargePit.Radius ? Hole : LargePit;
		}
		const uint8_t WantRim[3] = { 125, 103, 72 }, WantPit[3] = { 74, 59, 43 }, WantShade[3] = { 43, 35, 27 };
		PitColours(EGround::Common, ESeason::Dry, SmallPit, Rim, Bottom, Shade);
		bool bColours = std::fabs(Lstar(Rim) - Lstar(WantRim)) < 2.5 && std::fabs(Lstar(Bottom) - Lstar(WantPit)) < 2.5 && Shade[0] == Bottom[0];
		PitColours(EGround::Common, ESeason::Dry, LargePit, Rim, Bottom, Shade);
		bColours &= std::fabs(Lstar(Shade) - Lstar(WantShade)) < 2.5 && Lstar(Shade) > 10.0;
		Check(bColours, "a crater: its sunny rim 7D6748, its pit 4A3B2B, the shaded third of a large one 2B231B, never black");
		int Pools = 0, Large = 0, SmallPools = 0;
		for (const FPit& Hole : Holes)
		{
			PitColours(EGround::Common, ESeason::Mud, Hole, Rim, Bottom, Shade);
			const bool bPool = Bottom[2] > Bottom[0];
			if (Hole.Radius >= 0.15)
			{
				++Large;
				Pools += bPool ? 1 : 0;
			}
			else
			{
				SmallPools += bPool ? 1 : 0;
			}
		}
		Check(SmallPools == 0 && Large > 0 && Pools * 12 > Large && Pools * 3 < Large, "in the mud months about one large pit in six holds a dull sky pool, and no small one (" + std::to_string(Pools) + " of " + std::to_string(Large) + ")");
	}

	// ---- measuring ----
	void CheckMeasure()
	{
		std::vector<FLine> Lines = { Straight(0.0, 10.0, Growing(6.0), 5.0) };
		FNearest Near;
		Measure(Lines, -2.0, -3.0, 0.25, 57, 25, Near);
		bool bRight = true;
		for (int J = 0; J < 25; ++J)
		{
			for (int I = 0; I < 57; ++I)
			{
				const double X = -2.0 + I * 0.25, Y = -3.0 + J * 0.25;
				const double Want = X < 0.0 ? std::hypot(X, Y) : X > 10.0 ? std::hypot(X - 10.0, Y) : std::fabs(Y);
				const size_t Idx = static_cast<size_t>(J * 57 + I);
				FBandIn In;
				const bool bNear = At(Lines, Near, static_cast<int>(Idx), X, Y, In);
				if (Want <= Touch - 1e-6)
				{
					bRight &= bNear && std::fabs(In.D - Want) < 1e-4;
				}
				else if (Want > Touch + 1e-6)
				{
					bRight &= !bNear;
				}
			}
		}
		Check(bRight, "distances to the line are right, and nothing beyond its reach");
		FLine Open = Straight(0.0, 20.0, Growing(6.0), 10.0);
		EaseEnds(Open);
		Check(Open.State.front().Size == 0.0f && Open.State.back().Size == 0.0f && Open.State[20].Size == 1.0f, "an open line's band comes to nothing at its ends");
		// Two pairs on one line, the one whole where its armies face and the other at first show: they join at the stronger, with no notch.
		FLine Two = Straight(0.0, 40.0, Growing(6.0), 10.0);
		for (size_t K = 0; K < Two.X.size(); ++K)
		{
			if (Two.X[K] > 20.0)
			{
				Two.State[K] = Growing(2.0);
				Two.Facing[K] = static_cast<float>(std::fabs(Two.X[K] - 30.0));
			}
		}
		Strengthen(Two);
		double Dip = 1e9;
		bool bFalls = true;
		for (size_t K = 0; K < Two.X.size(); ++K)
		{
			if (Two.X[K] >= 17.0 && Two.X[K] <= 25.0)
			{
				Dip = std::min<double>(Dip, Two.Strength[K]);
				bFalls &= Two.X[K] <= 17.0 || Two.Strength[K] <= Two.Strength[K - 1] + 1e-6f;
			}
		}
		const double Weaker = 0.3;
		Check(Dip >= Weaker - 0.01 && bFalls && Two.Strength[40] > 0.85f && Two.Strength[36] > 0.99f, "two pairs join at the stronger, blended over about 3 units, with no notch (least " + Num(Dip) + ")");
		// The end of a band: weaker over its last 3 to 4 units, so that it breaks up and dies, never a stump.
		FLine End = Straight(0.0, 30.0, Growing(6.0), 15.0);
		Strengthen(End);
		EaseEnds(End);
		bool bBreaks = false;
		for (double X = 0.0; X < 3.5; X += 0.05)
		{
			FBandIn In;
			In.X = X;
			In.Y = 0.0;
			In.D = 0.0;
			In.Facing = 15.0 - X;
			In.Strength = End.Strength[static_cast<size_t>(X / 0.5)];
			In.State = End.State[static_cast<size_t>(X / 0.5)];
			bBreaks |= Band(In).Whole < 0.5f;
		}
		Check(End.Strength.front() == 0.0f && End.Strength[8] > 0.9f && bBreaks, "a band's end breaks up and dies over its last 3 to 4 units");
	}

	// ---- drawings over the game's own ground ----

	struct FBoard
	{
		int NX = 0, NY = 0;
		double Cell = 1.5, X0 = 0.0, Y0 = 0.0;
		std::vector<uint8_t> Rgb, Wooded, Kind;
		std::vector<TheaterFront::FFrontPiece> Pieces;
		struct FPair
		{
			int Low = -1, High = -1;
			double FX = 0.0, FY = 0.0;
		};
		std::vector<FPair> Pairs;
		struct FView
		{
			double X0, Y0, X1, Y1, Px;
			char Name[32];
		};
		std::vector<FView> Views;
	};

	bool ReadBoard(const char* Path, FBoard& Board)
	{
		FILE* File = std::fopen(Path, "rb");
		if (!File)
		{
			return false;
		}
		bool bOk = std::fread(&Board.NX, 4, 1, File) == 1 && std::fread(&Board.NY, 4, 1, File) == 1 && std::fread(&Board.Cell, 8, 1, File) == 1
			&& std::fread(&Board.X0, 8, 1, File) == 1 && std::fread(&Board.Y0, 8, 1, File) == 1;
		const size_t Count = bOk ? static_cast<size_t>(Board.NX) * static_cast<size_t>(Board.NY) : 0;
		Board.Rgb.resize(Count * 3);
		Board.Wooded.resize(Count);
		Board.Kind.resize(Count);
		bOk = bOk && std::fread(Board.Rgb.data(), 1, Count * 3, File) == Count * 3 && std::fread(Board.Wooded.data(), 1, Count, File) == Count && std::fread(Board.Kind.data(), 1, Count, File) == Count;
		int Pieces = 0, Pairs = 0, Views = 0;
		bOk = bOk && std::fread(&Pieces, 4, 1, File) == 1;
		for (int K = 0; bOk && K < Pieces; ++K)
		{
			double V[4];
			int S[2];
			bOk = std::fread(V, 8, 4, File) == 4 && std::fread(S, 4, 2, File) == 2;
			TheaterFront::FFrontPiece Piece;
			Piece.A = { V[0], V[1] };
			Piece.B = { V[2], V[3] };
			Piece.Low = std::min(S[0], S[1]);
			Piece.High = std::max(S[0], S[1]);
			Board.Pieces.push_back(Piece);
		}
		bOk = bOk && std::fread(&Pairs, 4, 1, File) == 1;
		for (int K = 0; bOk && K < Pairs; ++K)
		{
			int S[2];
			double F[2];
			bOk = std::fread(S, 4, 2, File) == 2 && std::fread(F, 8, 2, File) == 2;
			Board.Pairs.push_back({ std::min(S[0], S[1]), std::max(S[0], S[1]), F[0], F[1] });
		}
		bOk = bOk && std::fread(&Views, 4, 1, File) == 1;
		for (int K = 0; bOk && K < Views; ++K)
		{
			FBoard::FView View;
			double V[5];
			bOk = std::fread(V, 8, 5, File) == 5 && std::fread(View.Name, 1, 32, File) == 32;
			View.X0 = V[0];
			View.Y0 = V[1];
			View.X1 = V[2];
			View.Y1 = V[3];
			View.Px = V[4];
			View.Name[31] = 0;
			Board.Views.push_back(View);
		}
		std::fclose(File);
		return bOk;
	}

	/** The front's lines, as the map lays them (chained, despiked, thinned, rounded, divided), each point with its pair's facing distance. */
	void LayLines(const FBoard& Board, std::vector<FLine>& Lines, std::vector<int>& PairOf)
	{
		TheaterFront::FRules Rules;
		Rules.Step = 0.5;
		TheaterFront::FScratch Work;
		std::vector<TheaterFront::FChain> Chains;
		TheaterFront::ChainFront(Board.Pieces, Rules, Work, Chains);
		for (TheaterFront::FChain& Chain : Chains)
		{
			TheaterFront::Despike(Chain);
			TheaterFront::Thin(Chain, Rules.Thin, Work);
			TheaterFront::Round(Chain, Rules.Passes, Rules.Cut, Work);
		}
		TheaterFront::MeetLines(Chains);
		for (TheaterFront::FChain& Chain : Chains)
		{
			TheaterFront::Divide(Chain, Rules.Step, Work);
		}
		// Where the two armies face each other: the point of their shared border nearest the middle between them.
		std::vector<TheaterFront::FPoint> FaceAt(Board.Pairs.size());
		for (size_t K = 0; K < Board.Pairs.size(); ++K)
		{
			double Best = 1e18;
			for (const TheaterFront::FFrontPiece& Piece : Board.Pieces)
			{
				if (Piece.Low != Board.Pairs[K].Low || Piece.High != Board.Pairs[K].High)
				{
					continue;
				}
				const double DX = Piece.B.X - Piece.A.X, DY = Piece.B.Y - Piece.A.Y, L2 = DX * DX + DY * DY;
				const double T = L2 > 1e-12 ? std::clamp(((Board.Pairs[K].FX - Piece.A.X) * DX + (Board.Pairs[K].FY - Piece.A.Y) * DY) / L2, 0.0, 1.0) : 0.0;
				const TheaterFront::FPoint On{ Piece.A.X + DX * T, Piece.A.Y + DY * T };
				const double D = std::hypot(On.X - Board.Pairs[K].FX, On.Y - Board.Pairs[K].FY);
				if (D < Best)
				{
					Best = D;
					FaceAt[K] = On;
				}
			}
		}
		Lines.clear();
		PairOf.clear();
		for (const TheaterFront::FChain& Chain : Chains)
		{
			FLine& Line = Lines.emplace_back();
			Line.bClosed = Chain.bClosed;
			for (const TheaterFront::FPoint& P : Chain.Points)
			{
				// (its pair: the nearest piece's)
				double Best = 1e18;
				int Pair = -1;
				for (const TheaterFront::FFrontPiece& Piece : Board.Pieces)
				{
					const double DX = Piece.B.X - Piece.A.X, DY = Piece.B.Y - Piece.A.Y, L2 = DX * DX + DY * DY;
					const double T = L2 > 1e-12 ? std::clamp(((P.X - Piece.A.X) * DX + (P.Y - Piece.A.Y) * DY) / L2, 0.0, 1.0) : 0.0;
					const double D = std::hypot(P.X - Piece.A.X - DX * T, P.Y - Piece.A.Y - DY * T);
					if (D < Best)
					{
						Best = D;
						for (size_t K = 0; K < Board.Pairs.size(); ++K)
						{
							if (Board.Pairs[K].Low == Piece.Low && Board.Pairs[K].High == Piece.High)
							{
								Pair = static_cast<int>(K);
							}
						}
					}
				}
				Line.X.push_back(P.X);
				Line.Y.push_back(P.Y);
				Line.Facing.push_back(Pair >= 0 ? static_cast<float>(std::hypot(P.X - FaceAt[static_cast<size_t>(Pair)].X, P.Y - FaceAt[static_cast<size_t>(Pair)].Y)) : 1e9f);
				Line.State.push_back(FState());
				PairOf.push_back(Pair);
			}
		}
	}

	struct FMoment
	{
		const char* Name;
		double Months; // dug in and facing
		double Since; // months since it moved on (0 while it stands)
		ESeason Season;
	};

	void WritePpm(const std::string& Path, int W, int H, const std::vector<uint8_t>& Rgb)
	{
		FILE* File = std::fopen(Path.c_str(), "wb");
		if (!File)
		{
			return;
		}
		std::fprintf(File, "P6\n%d %d\n255\n", W, H);
		std::fwrite(Rgb.data(), 1, Rgb.size(), File);
		std::fclose(File);
	}

	int Draw(const char* BoardPath, const char* OutDir)
	{
		FBoard Board;
		if (!ReadBoard(BoardPath, Board))
		{
			std::printf("could not read %s\n", BoardPath);
			return 2;
		}
		std::vector<FLine> Lines;
		std::vector<int> PairOf;
		LayLines(Board, Lines, PairOf);
		std::printf("front: %zu pieces, %zu pairs, %zu lines\n", Board.Pieces.size(), Board.Pairs.size(), Lines.size());
		// (and three more looks the art director asked for: one edge at twice the close look's scale, one band's end and
		// its sharpest corner, each at the close look's scale)
		for (const FBoard::FView& View : std::vector<FBoard::FView>(Board.Views))
		{
			if (std::strcmp(View.Name, "close") == 0)
			{
				const double CX = 0.5 * (View.X0 + View.X1), CY = 0.5 * (View.Y0 + View.Y1);
				FBoard::FView Edge{ CX - 7.0, CY - 4.5, CX + 7.0, CY + 4.5, View.Px * 0.5, "edge" };
				Board.Views.push_back(Edge);
			}
		}
		for (const FLine& Line : Lines)
		{
			const size_t N = Line.X.size();
			if (!Line.bClosed && N > 10)
			{
				Board.Views.push_back({ Line.X[0] - 14.0, Line.Y[0] - 9.0, Line.X[0] + 14.0, Line.Y[0] + 9.0, 0.07, "end" });
			}
			double Sharpest = 0.0;
			size_t At = 0;
			for (size_t K = 6; K + 6 < N; ++K)
			{
				const double A = std::atan2(Line.Y[K] - Line.Y[K - 6], Line.X[K] - Line.X[K - 6]), B = std::atan2(Line.Y[K + 6] - Line.Y[K], Line.X[K + 6] - Line.X[K]);
				double Turn = std::fabs(B - A);
				Turn = std::min(Turn, 2.0 * 3.14159265358979 - Turn);
				if (Turn > Sharpest)
				{
					Sharpest = Turn;
					At = K;
				}
			}
			if (Sharpest > 0.5)
			{
				Board.Views.push_back({ Line.X[At] - 14.0, Line.Y[At] - 9.0, Line.X[At] + 14.0, Line.Y[At] + 9.0, 0.07, "corner" });
			}
			break;
		}
		const FMoment Moments[] = {
			{ "m0", 0.0, 0.0, ESeason::Dry }, { "m2", 2.0, 0.0, ESeason::Dry }, { "m4", 4.0, 0.0, ESeason::Dry }, { "m6", 6.0, 0.0, ESeason::Dry },
			{ "heal3", 6.0, 3.0, ESeason::Dry }, { "heal8", 6.0, 8.0, ESeason::Dry }, { "heal12", 6.0, 12.0, ESeason::Dry },
			{ "m6-mud", 6.0, 0.0, ESeason::Mud }, { "m6-snow", 6.0, 0.0, ESeason::Snow } };
		const int NX = Board.NX, NY = Board.NY;
		FNearest Coarse;
		Measure(Lines, Board.X0, Board.Y0, Board.Cell, NX, NY, Coarse);
		for (const FMoment& Moment : Moments)
		{
			const FState Now = Moment.Since > 0.0 ? Healing(Growing(Moment.Months), Moment.Since) : Growing(Moment.Months);
			std::vector<FLine> Here = Lines;
			for (FLine& Line : Here)
			{
				std::fill(Line.State.begin(), Line.State.end(), Now);
				Strengthen(Line);
				EaseEnds(Line);
			}
			// The ground picture: the band painted in, the woods thinned, the snow broken; then the woods' cloth and the snow over it, as the material lays them.
			std::vector<double> Picture(static_cast<size_t>(NX) * static_cast<size_t>(NY) * 3), Plain(Picture.size());
			std::vector<uint8_t> Painted(static_cast<size_t>(NX) * static_cast<size_t>(NY) * 3);
			for (int J = 0; J < NY; ++J)
			{
				for (int I = 0; I < NX; ++I)
				{
					const size_t Idx = static_cast<size_t>(J) * static_cast<size_t>(NX) + static_cast<size_t>(I);
					const double X = Board.X0 + I * Board.Cell, Y = Board.Y0 + J * Board.Cell;
					uint8_t Out[3] = { Board.Rgb[Idx * 3], Board.Rgb[Idx * 3 + 1], Board.Rgb[Idx * 3 + 2] };
					uint8_t Woods = Board.Wooded[Idx], SnowShare = Moment.Season == ESeason::Snow ? 225 : 0;
					const uint8_t Bare[3] = { Out[0], Out[1], Out[2] }, BareWoods = Woods, BareSnow = SnowShare;
					FBandIn In;
					if (Board.Kind[Idx] < 3 && At(Here, Coarse, static_cast<int>(Idx), X, Y, In))
					{
						const FBand B = Band(In);
						const uint8_t Was[3] = { Out[0], Out[1], Out[2] };
						PaintCell(Was, B, In.State, static_cast<EGround>(Board.Kind[Idx]), Moment.Season, X, Y, Out);
						Woods = Wooded(Woods, B, In.State);
						SnowShare = Snow(SnowShare, B, In.State);
					}
					Painted[Idx * 3] = Out[0];
					Painted[Idx * 3 + 1] = Out[1];
					Painted[Idx * 3 + 2] = Out[2];
					const double WoodsColour[3] = { 58.0, 74.0, 46.0 }, SnowColour[3] = { 236.0, 238.0, 240.0 };
					auto Lay = [&](const uint8_t Ground[3], uint8_t WoodsShare, uint8_t SnowOn, std::vector<double>& Into)
					{
						const double W = 0.55 * WoodsShare / 255.0, S = SnowOn / 255.0;
						for (int C = 0; C < 3; ++C)
						{
							double L = ToLight(Ground[C]) * (1.0 - W) + ToLight(WoodsColour[C]) * W;
							L = L * (1.0 - S) + ToLight(SnowColour[C]) * S;
							Into[Idx * 3 + static_cast<size_t>(C)] = Board.Kind[Idx] == 3 ? ToLight(Board.Rgb[Idx * 3 + static_cast<size_t>(C)]) : L;
						}
					};
					Lay(Out, Woods, SnowShare, Picture);
					Lay(Bare, BareWoods, BareSnow, Plain);
				}
			}
			for (const FBoard::FView& View : Board.Views)
			{
				const int W = static_cast<int>((View.X1 - View.X0) / View.Px), H = static_cast<int>((View.Y1 - View.Y0) / View.Px);
				// (where the whole band, about 3.6 units, would be under about four pixels wide it fades out: the picture's
				// share, the veil and the craters alike; and the veil is drawn only where the eye is lower than the picture's points)
				const double Show = Shown(3.6 / View.Px);
				const bool bFine = View.Px < Board.Cell * 0.75 && Show > 0.0;
				std::vector<double> Light(static_cast<size_t>(W) * static_cast<size_t>(H) * 3);
				for (int J = 0; J < H; ++J)
				{
					for (int I = 0; I < W; ++I)
					{
						// (the picture as the material reads it: smoothed between its points)
						const double GX = (View.X0 + (I + 0.5) * View.Px - Board.X0) / Board.Cell, GY = (View.Y0 + (J + 0.5) * View.Px - Board.Y0) / Board.Cell;
						const int I0 = std::clamp(static_cast<int>(std::floor(GX)), 0, NX - 2), J0 = std::clamp(static_cast<int>(std::floor(GY)), 0, NY - 2);
						const double U = std::clamp(GX - I0, 0.0, 1.0), V = std::clamp(GY - J0, 0.0, 1.0);
						for (int C = 0; C < 3; ++C)
						{
							auto P = [&](int A, int B) { return Picture[(static_cast<size_t>(B) * static_cast<size_t>(NX) + static_cast<size_t>(A)) * 3 + static_cast<size_t>(C)]; };
							auto Q = [&](int A, int B) { return Plain[(static_cast<size_t>(B) * static_cast<size_t>(NX) + static_cast<size_t>(A)) * 3 + static_cast<size_t>(C)]; };
							const double Churned = (P(I0, J0) * (1 - U) + P(I0 + 1, J0) * U) * (1 - V) + (P(I0, J0 + 1) * (1 - U) + P(I0 + 1, J0 + 1) * U) * V;
							const double Bare = (Q(I0, J0) * (1 - U) + Q(I0 + 1, J0) * U) * (1 - V) + (Q(I0, J0 + 1) * (1 - U) + Q(I0 + 1, J0 + 1) * U) * V;
							Light[(static_cast<size_t>(J) * static_cast<size_t>(W) + static_cast<size_t>(I)) * 3 + static_cast<size_t>(C)] = Bare + (Churned - Bare) * Show;
						}
					}
				}
				if (bFine)
				{
					FNearest Fine;
					Measure(Here, View.X0 + 0.5 * View.Px, View.Y0 + 0.5 * View.Px, View.Px, W, H, Fine);
					for (int J = 0; J < H; ++J)
					{
						for (int I = 0; I < W; ++I)
						{
							const size_t Idx = static_cast<size_t>(J) * static_cast<size_t>(W) + static_cast<size_t>(I);
							const double X = View.X0 + (I + 0.5) * View.Px, Y = View.Y0 + (J + 0.5) * View.Px;
							FBandIn In;
							if (At(Here, Fine, static_cast<int>(Idx), X, Y, In))
							{
								const double V = Veil(Band(In), In.State, X, Y) * Show;
								for (int C = 0; C < 3; ++C)
								{
									Light[Idx * 3 + static_cast<size_t>(C)] *= 1.0 - V;
								}
							}
						}
					}
					for (const FLine& Line : Here)
					{
						std::vector<FPit> Holes;
						Pits(Line, Holes);
						for (const FPit& Hole : Holes)
						{
							const int GI = std::clamp(static_cast<int>((Hole.X - Board.X0) / Board.Cell + 0.5), 0, NX - 1), GJ = std::clamp(static_cast<int>((Hole.Y - Board.Y0) / Board.Cell + 0.5), 0, NY - 1);
							const size_t GIdx = static_cast<size_t>(GJ) * static_cast<size_t>(NX) + static_cast<size_t>(GI);
							if (Board.Kind[GIdx] >= 3)
							{
								continue;
							}
							uint8_t Rim[3], Bottom[3], Shade[3];
							PitColours(static_cast<EGround>(Board.Kind[GIdx]), Moment.Season, Hole, Rim, Bottom, Shade);
							// (drawn only from three pixels across, whole from six: smaller, its cluster shows as the veil's mottle)
							const double Seen = CraterShown(2.0 * Hole.Radius / View.Px) * Show * Hole.Dark;
							if (Seen <= 0.0)
							{
								continue;
							}
							const int PI0 = static_cast<int>((Hole.X - Hole.Radius - View.X0) / View.Px) - 1, PI1 = static_cast<int>((Hole.X + Hole.Radius - View.X0) / View.Px) + 1;
							const int PJ0 = static_cast<int>((Hole.Y - Hole.Radius - View.Y0) / View.Px) - 1, PJ1 = static_cast<int>((Hole.Y + Hole.Radius - View.Y0) / View.Px) + 1;
							for (int PJ = std::max(0, PJ0); PJ <= std::min(H - 1, PJ1); ++PJ)
							{
								for (int PI = std::max(0, PI0); PI <= std::min(W - 1, PI1); ++PI)
								{
									// (a low ring of rim round a pit, lit from the north-west: the rim shows its own colour on the sunny
									// side only and darkens the ground a little on the far side; the pit's floor is shaded in the third
									// nearest the sun, under the near wall; a fleck of spoil is its colour all over; four samples a pixel)
									const size_t Idx = static_cast<size_t>(PJ) * static_cast<size_t>(W) + static_cast<size_t>(PI);
									for (int S = 0; S < 4; ++S)
									{
										const double SX = View.X0 + (PI + 0.25 + 0.5 * (S % 2)) * View.Px, SY = View.Y0 + (PJ + 0.25 + 0.5 * (S / 2)) * View.Px;
										const double DX = (SX - Hole.X) / Hole.Radius, DY = (SY - Hole.Y) / Hole.Radius, R = std::hypot(DX, DY);
										if (R >= 1.0)
										{
											continue;
										}
										const double Sunward = R > 1e-6 ? (-DX * 0.7071 - DY * 0.7071) / R : 0.0;
										for (int C = 0; C < 3; ++C)
										{
											double& L = Light[Idx * 3 + static_cast<size_t>(C)];
											double Want = L;
											if (Hole.bSpoil)
											{
												Want = ToLight(Rim[C]);
											}
											else if (R < 0.62)
											{
												Want = ToLight(Sunward * R > 0.62 / 3.0 ? Shade[C] : Bottom[C]);
											}
											else if (Sunward > 0.0)
											{
												Want = L + (ToLight(Rim[C]) - L) * std::min(1.0, 1.6 * Sunward);
											}
											else
											{
												Want = L * (1.0 + 0.14 * Sunward);
											}
											L += 0.25 * Seen * (Want - L);
										}
									}
								}
							}
						}
					}
				}
				std::vector<uint8_t> Eye(Light.size());
				for (size_t K = 0; K < Light.size(); ++K)
				{
					Eye[K] = static_cast<uint8_t>(std::lround(std::clamp(ToEye(Light[K]), 0.0, 255.0)));
				}
				WritePpm(std::string(OutDir) + "/churn-" + View.Name + "-" + Moment.Name + ".ppm", W, H, Eye);
			}
		}
		return 0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc >= 4 && std::strcmp(Argv[1], "--map") == 0)
	{
		return Draw(Argv[2], Argv[3]);
	}
	CheckMonths();
	CheckWidth();
	CheckEdge();
	CheckPaint();
	CheckVeilAndPits();
	CheckMeasure();
	std::printf("front churn: %d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
