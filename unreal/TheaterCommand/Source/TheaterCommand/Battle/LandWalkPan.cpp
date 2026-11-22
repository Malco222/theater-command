#include "Battle/LandWalkPan.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <map>
#include <utility>

namespace LandWalkPan
{
	namespace
	{
		double GroundOf(const LandEye::World& Here, double X, double Y)
		{
			return Here.Height ? Here.Height(X, Y) : 0.0;
		}

		bool MiddleOf(const LandFeed::Recording& Rec, const std::vector<int>& Men, double T, double& OutX, double& OutY)
		{
			double X = 0.0, Y = 0.0;
			int N = 0;
			for (const int I : Men)
			{
				const LandFeed::Track& Tr = Rec.Tracks[static_cast<size_t>(I)];
				if ((Tr.Fell >= 0.0 && Tr.Fell <= T) || (Tr.Out >= 0.0 && Tr.Out <= T)) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
				X += S.X;
				Y += S.Y;
				++N;
			}
			if (N == 0) return false;
			OutX = X / N;
			OutY = Y / N;
			return true;
		}

		/** The eye off the flank of the middle of Best's walk or drive over the pan: each height of Ups, each distance of
		 * Offs off the middle, either side, the first whose sight lines to the start, middle and end are open (else the
		 * first free place, bOpen false); with its distances to the start, middle and end. */
		Plan EyeOff(const LandFeed::Recording& Rec, const LandEye::World& Here, double T0, double For, Plan Best, std::initializer_list<double> Ups,
			std::initializer_list<double> Offs)
		{
			const double T1 = T0 + For;
			double X0 = 0.0, Y0 = 0.0, Xm = 0.0, Ym = 0.0, X1 = 0.0, Y1 = 0.0;
			MiddleOf(Rec, Best.Men, T0, X0, Y0);
			MiddleOf(Rec, Best.Men, T0 + 0.5 * For, Xm, Ym);
			MiddleOf(Rec, Best.Men, T1, X1, Y1);
			const double Way = std::max(1e-6, std::hypot(X1 - X0, Y1 - Y0));
			const double Nx = -(Y1 - Y0) / Way, Ny = (X1 - X0) / Way;
			const double W = Rec.Ground.W > 0.0 ? Rec.Ground.W : 1e9, H = Rec.Ground.H > 0.0 ? Rec.Ground.H : 1e9;
			const LandPlay::Vec A(X0, Y0, GroundOf(Here, X0, Y0) + 1.2), M(Xm, Ym, GroundOf(Here, Xm, Ym) + 1.2), B(X1, Y1, GroundOf(Here, X1, Y1) + 1.2);
			bool bFound = false;
			LandPlay::Vec Fallback;
			bool bFallback = false;
			for (const double Up : Ups)
				for (const double Off : Offs)
					for (const double Side : { 1.0, -1.0 })
					{
						if (bFound) break;
						const double Ex = Xm + Nx * Side * Off, Ey = Ym + Ny * Side * Off;
						if (Ex < 10.0 || Ey < 10.0 || Ex > W - 10.0 || Ey > H - 10.0) continue;
						const LandPlay::Vec E(Ex, Ey, GroundOf(Here, Ex, Ey) + Up);
						if (Here.Free && !Here.Free(E)) continue;
						if (!bFallback)
						{
							Fallback = E;
							bFallback = true;
						}
						const bool bOpen = !Here.Open || (Here.Open(E, A) && Here.Open(E, M) && Here.Open(E, B));
						if (!bOpen) continue;
						Best.Eye = E;
						Best.bOpen = true;
						bFound = true;
					}
			if (!bFound)
			{
				if (!bFallback) return Plan();
				Best.Eye = Fallback;
				Best.bOpen = false;
			}
			Best.FromStart = std::hypot(X0 - Best.Eye.X, Y0 - Best.Eye.Y);
			Best.FromMiddle = std::hypot(Xm - Best.Eye.X, Ym - Best.Eye.Y);
			Best.FromEnd = std::hypot(X1 - Best.Eye.X, Y1 - Best.Eye.Y);
			return Best;
		}
	}

	Plan Choose(const LandFeed::Recording& Rec, const LandEye::World& Here, double T0, double For)
	{
		Plan Best;
		const double T1 = T0 + For;
		std::map<int, std::vector<int>> ByBlock;
		for (size_t I = 0; I < Rec.Tracks.size(); ++I)
		{
			const LandFeed::Track& Tr = Rec.Tracks[I];
			if (!Tr.IsMan() || Tr.bTwin || Tr.At.empty()) continue;
			if ((Tr.Fell >= 0.0 && Tr.Fell < T1) || (Tr.Out >= 0.0 && Tr.Out < T1)) continue;
			ByBlock[Tr.Block].push_back(static_cast<int>(I));
		}
		double BestScore = 0.0;
		for (const auto& [Block, Men] : ByBlock)
		{
			if (Men.size() < 6) continue;
			// (steady: its middle's way over each quarter of the pan near its way over the whole, and walking pace)
			double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
			if (!MiddleOf(Rec, Men, T0, X0, Y0) || !MiddleOf(Rec, Men, T1, X1, Y1)) continue;
			const double Speed = std::hypot(X1 - X0, Y1 - Y0) / std::max(1.0, For);
			if (Speed < 0.5 || Speed > 2.6) continue;
			double Wander = 0.0, Px = X0, Py = Y0;
			for (int Q = 1; Q <= 4; ++Q)
			{
				double Qx = 0.0, Qy = 0.0;
				if (!MiddleOf(Rec, Men, T0 + For * Q / 4.0, Qx, Qy)) continue;
				const double Want = Speed * For / 4.0, Got = std::hypot(Qx - Px, Qy - Py);
				Wander += std::fabs(Got - Want) / std::max(0.5, Want);
				Px = Qx;
				Py = Qy;
			}
			const double Score = static_cast<double>(Men.size()) * std::exp(-std::pow((Speed - 1.3) / 0.7, 2.0)) / (1.0 + Wander);
			if (Score <= BestScore) continue;
			BestScore = Score;
			Best = Plan();
			Best.Block = Block;
			Best.Men = Men;
			Best.Speed = Speed;
		}
		if (Best.Block < 0) return Best;
		// The eye off the flank of the walk's middle: both sides, near to far, a man's height and then a bank's.
		return EyeOff(Rec, Here, T0, For, Best, { 1.7, 3.5 }, { 70.0, 55.0, 85.0, 45.0, 100.0 });
	}

	Plan ChooseTank(const LandFeed::Recording& Rec, const LandEye::World& Here, double T0, double For)
	{
		const double T1 = T0 + For;
		// (every steady drive, best first: the lens tries each in turn until one is seen in the open, so a town's or a
		// wood's tank gives way to one on open ground)
		std::vector<std::pair<double, Plan>> Drives;
		for (size_t I = 0; I < Rec.Tracks.size(); ++I)
		{
			const LandFeed::Track& Tr = Rec.Tracks[I];
			if (Tr.Role != "tank" || Tr.bTwin || Tr.At.empty() || Tr.bPinned) continue;
			if (Tr.Start() > T0 || Tr.End() < T1) continue;
			if ((Tr.Fell >= 0.0 && Tr.Fell < T1) || (Tr.Out >= 0.0 && Tr.Out < T1)) continue;
			const std::vector<int> Her{ static_cast<int>(I) };
			double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
			if (!MiddleOf(Rec, Her, T0, X0, Y0) || !MiddleOf(Rec, Her, T1, X1, Y1)) continue;
			const double Speed = std::hypot(X1 - X0, Y1 - Y0) / std::max(1.0, For);
			if (Speed < 2.0 || Speed > 9.0) continue;
			// (steady: her way over each quarter near her way over the whole, so the lens follows a drive, not a halt)
			double Wander = 0.0, Px = X0, Py = Y0;
			bool bAll = true;
			for (int Q = 1; Q <= 4; ++Q)
			{
				double Qx = 0.0, Qy = 0.0;
				if (!MiddleOf(Rec, Her, T0 + For * Q / 4.0, Qx, Qy))
				{
					bAll = false;
					break;
				}
				const double Want = Speed * For / 4.0, Got = std::hypot(Qx - Px, Qy - Py);
				Wander += std::fabs(Got - Want) / std::max(0.5, Want);
				Px = Qx;
				Py = Qy;
			}
			if (!bAll) continue;
			const double Score = std::exp(-std::pow((Speed - 5.0) / 2.5, 2.0)) / (1.0 + Wander);
			Plan Her1;
			Her1.Block = Tr.Block;
			Her1.Men = Her;
			Her1.Speed = Speed;
			Drives.push_back({ Score, std::move(Her1) });
		}
		std::stable_sort(Drives.begin(), Drives.end(), [](const auto& A, const auto& B) { return A.first > B.first; });
		// The eye off the flank of her drive's middle, past the hand-over: both sides, near to far, a man on a bank's
		// height, then a roof's, then a church tower's (Normandy's hedges). The first drive seen in the open, of the
		// best eight; else the best's first free place, not open.
		Plan First;
		for (size_t K = 0; K < Drives.size() && K < 8; ++K)
		{
			Plan P = EyeOff(Rec, Here, T0, For, Drives[K].second, { 2.5, 6.0, 10.0 }, { 190.0, 170.0, 215.0, 240.0 });
			if (P.Block >= 0 && P.bOpen) return P;
			if (First.Block < 0 && P.Block >= 0) First = std::move(P);
		}
		return First;
	}


	bool Middle(const LandFeed::Recording& Rec, const LandEye::World& Here, const Plan& P, double T, LandPlay::Vec& Out)
	{
		double X = 0.0, Y = 0.0;
		if (P.Block < 0 || !MiddleOf(Rec, P.Men, T, X, Y)) return false;
		Out = LandPlay::Vec(X, Y, GroundOf(Here, X, Y) + 1.0);
		return true;
	}
}
