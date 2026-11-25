#include "Battle/FleetWaves.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace FleetWaves
{
	namespace
	{
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
		double Sat(double V) { return Clamp(V, 0.0, 1.0); }
		double Sq(double V) { return V * V; }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Cheb(double Ax, double Ay, double Bx, double By) { return std::max(std::fabs(Ax - Bx), std::fabs(Ay - By)); }
		/** [P] The share of a Pierson-Moskowitz sea's energy below frequency W: exp(-1.25 (Wp/W)^4). */
		double ShareBelow(double W, double Wp)
		{
			if (W <= 0.0) return 0.0;
			const double R = Wp / W;
			return std::exp(-1.25 * Sq(Sq(R)));
		}
		/** The art director's knots by wind (M_StageSea's, AAirStage::MovingSea): at the middle of Beaufort 3, 4, 5 and 6. */
		const double Forces[4] = { 4.4, 6.7, 9.4, 12.3 };
		const double CapSizes[4] = { 0.8, 1.2, 2.0, 3.0 }, FoamLives[4] = { 0.3, 1.0, 1.6, 2.0 };
		double Knots(double U, const double (&At)[4], const double (&Is)[4])
		{
			if (U <= At[0]) return Is[0];
			for (int K = 1; K < 4; ++K)
			{
				if (U <= At[K]) return Is[K - 1] + (Is[K] - Is[K - 1]) * (U - At[K - 1]) / (At[K] - At[K - 1]);
			}
			return Is[3];
		}
		/** A read of the ripples picture Length metres to it, sliding at the speed of waves a seventh of it: lengths a second. */
		double ReadRate(double Length) { return std::sqrt(Gee * (Length / 7.0) / TwoPi) / Length; }
		/** The whitecaps' gate by wind (Tools/sea_waves/fold_table.cpp writes it). */
		const double FoldTable[][4] = {
#include "Battle/FleetWavesFold.inl"
		};
		constexpr int FoldRows = static_cast<int>(sizeof(FoldTable) / sizeof(FoldTable[0]));
		/** A train's phase at a point and clock, taken through the wraps first (radians, in [-pi, pi)). */
		double PhaseOf(const Train& T, double X, double Y, double Time) { return TwoPi * Turns((T.Kx * X + T.Ky * Y - T.W * Time + T.Phase0) / TwoPi); }
		// (the stream's numbers, each use its own place in it)
		enum : uint64_t { AtSplit = 0, AtHeading = 10, AtFrequency = 20, AtPhase = 30, AtSwell = 40, AtPixel = 50 };

		/** Where the barycentric weights of (Px, Py) in a triangle are; false for a triangle with no area. */
		bool Bary(double Px, double Py, const double (&X)[3], const double (&Y)[3], double (&L)[3])
		{
			const double Det = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2]);
			const double Scale = std::max({ std::fabs(X[0] - X[2]), std::fabs(Y[0] - Y[2]), std::fabs(X[1] - X[2]), std::fabs(Y[1] - Y[2]), 1e-30 });
			if (std::fabs(Det) < 1e-12 * Scale * Scale) return false;
			L[0] = ((Y[1] - Y[2]) * (Px - X[2]) + (X[2] - X[1]) * (Py - Y[2])) / Det;
			L[1] = ((Y[2] - Y[0]) * (Px - X[2]) + (X[0] - X[2]) * (Py - Y[2])) / Det;
			L[2] = 1.0 - L[0] - L[1];
			return true;
		}

		/** The skirt's s-th near point, in the stage's cells from the last ring's middle: round its edge, s from 0 to 4N. */
		void SkirtCell(int N, int Si, int& X, int& Y)
		{
			const int H = N / 2;
			Si = ((Si % (4 * N)) + 4 * N) % (4 * N);
			if (Si < N) { X = -H + Si; Y = -H; }
			else if (Si < 2 * N) { X = H; Y = -H + (Si - N); }
			else if (Si < 3 * N) { X = H - (Si - 2 * N); Y = H; }
			else { X = -H; Y = H - (Si - 3 * N); }
		}

		/** The drawn sea as the chip lays it, in board metres and double: one frame's (or, bBefore, its eye and clock
		 * before on this frame's snaps). */
		struct Lay
		{
			/** One of the patch's two notches (Q 0 at its corner (-H, +H), 1 at (+H, -H)): the main triangle (A, B, the
			 * corner) of ring K, A and B the ends of its long side (B - A = (C, C)). */
			struct NotchOf
			{
				bool bOn = false;
				int K = 0;
				double Ax = 0.0, Ay = 0.0, Bx = 0.0, By = 0.0, Cx = 0.0, Cy = 0.0, Cc = 1.0;
			};
			const Sea* S;
			const Drawn& D;
			double Ex, Ey, T;
			int Half;
			double Cells[MostRings + 2] = {};
			NotchOf Notches[2];
			Lay(const Sea* InS, const Drawn& InD, bool bBefore)
				: S(InS), D(InD), Ex(bBefore ? InD.EyeBeforeX : InD.EyeX), Ey(bBefore ? InD.EyeBeforeY : InD.EyeY), T(bBefore ? InD.SeaTBefore : InD.SeaT),
				  Half(std::max(InD.N, 8) / 2)
			{
				Cells[0] = D.C0;
				for (int K = 1; K < MostRings + 2; ++K) Cells[K] = 2.0 * Cells[K - 1];
				for (int Q = 0; Q < 2; ++Q) FindNotch(Q);
			}
			int Rings() const { return std::clamp(D.Rings, 1, MostRings); }
			double C(int K) const { return K >= 0 && K < MostRings + 2 ? Cells[K] : D.C0 * std::ldexp(1.0, K); }
			/** CDLOD's morph: 0 inside the ring, rising over an eighth of its half side to 1 by Half - 3 cells, so that with the eye
			 * anywhere in its snap cell (up to two cells from the middle) the outer two cells, whose odd points the edge's fold
			 * reaches, have eased whole. */
			double Morph(int K, double Dist) const
			{
				const double Cc = C(K), B = (Half - 3.0) * Cc, A = B - Half * Cc * 0.125;
				return Sat((Dist - A) / std::max(B - A, 1e-9));
			}
			int RingOf(double X, double Y) const
			{
				for (int K = 0; K < Rings(); ++K)
				{
					if (Cheb(X, Y, D.RingX[K], D.RingY[K]) <= Half * C(K)) return K;
				}
				return -1;
			}
			double RingCell(int K, double X, double Y) const
			{
				const double Cc = C(K);
				return Cc * (1.0 + Morph(K, Cheb(X, Y, Ex, Ey) + 2.0 * Cc));
			}
			double MainCell(double X, double Y) const
			{
				const int K = RingOf(X, Y);
				return K < 0 ? 1e30 : RingCell(K, X, Y);
			}
			bool PatchOn() const { return D.Patch.bOn && D.Patch.Cell > 0.0; }
			bool InSquare(double X, double Y) const { return PatchOn() && Cheb(X, Y, D.Patch.X, D.Patch.Y) <= D.Patch.Half; }
			/** How far a point of the patch has eased from its own sea to the main mesh's: 0 out to Full from the subject
			 * (unsnapped), 1 from EaseEnd (Chebyshev), and as far again as the patch is not present (1 - Presence (1 - that)):
			 * at Presence 0 it is 1 everywhere. Over it both its weights' cell and its place go to the main mesh's. */
			double PatchEase(double X, double Y) const
			{
				const PatchAt& P = D.Patch;
				const double E = Sat((Cheb(X, Y, P.SubjectX, P.SubjectY) - P.Full) / std::max(P.EaseEnd - P.Full, 1e-9));
				return 1.0 - Sat(P.Presence) * (1.0 - E);
			}
			/** The patch's own cell out to Full from the subject (unsnapped), eased to the main mesh's by EaseEnd. */
			double PatchCell(double X, double Y) const { return Lerp(D.Patch.Cell, std::max(D.Patch.Cell, MainCell(X, Y)), PatchEase(X, Y)); }
			/** Which side of notch Q's long side a point lies: above 0 inside the notch, 0 on the long side. */
			static double NotchSide(const NotchOf& N, int Q, double X, double Y) { return (Q == 0 ? 1.0 : -1.0) * ((Y - X) - (N.Ay - N.Ax)); }
			/** The notch holding a point inside the square (strictly over its long side), or -1. */
			int NotchAt(double X, double Y) const
			{
				for (int Q = 0; Q < 2; ++Q)
				{
					if (Notches[Q].bOn && NotchSide(Notches[Q], Q, X, Y) > 0.0) return Q;
				}
				return -1;
			}
			double CellAt(double X, double Y) const
			{
				if (InSquare(X, Y) && NotchAt(X, Y) < 0) return PatchCell(X, Y);
				return MainCell(X, Y);
			}
			/** A point's ease onto the main mesh where the patch draws it (in its square, out of its notches), else 0. */
			double BandAt(double X, double Y) const { return InSquare(X, Y) && NotchAt(X, Y) < 0 ? PatchEase(X, Y) : 0.0; }
			/** Train J's drawn weight at a point: at its cell, and over the patch's ease its weight at the main mesh's cell, as
			 * far as the point has eased onto the main mesh's surface. */
			double WeightAt(int J, double X, double Y) const
			{
				const double L = S->D[J].L, E = BandAt(X, Y), W = WeightOf(L, CellAt(X, Y));
				return E > 0.0 ? Lerp(W, WeightOf(L, MainCell(X, Y)), E) : W;
			}
			static double OddOf(double G) { return std::fabs(G - 2.0 * std::floor(G * 0.5)); }
			/** A main point's rest: its ring's snap and cells; an odd point on the ring's outer edge goes to its even neighbour
			 * toward the ring's corners at (-, +) and (+, -), so the edge has the next ring's points and no T-junction (the ring's
			 * outer two cells have eased whole, so the triangles that fold away and those that grow lie on the same flat pieces:
			 * the surface is the same); and beside the patch's corners of that kind the fold puts the point on the corner. */
			void Moved(int K, double Gx, double Gy, double& X, double& Y) const
			{
				const double Cc = C(K);
				const double Px = D.RingX[K] + Gx * Cc, Py = D.RingY[K] + Gy * Cc;
				const bool bEdgeX = std::fabs(std::fabs(Gx) - Half) < 0.25, bEdgeY = std::fabs(std::fabs(Gy) - Half) < 0.25;
				X = Px - (bEdgeY ? OddOf(Gx) * Cc * (Gy > 0.0 ? 1.0 : -1.0) : 0.0);
				Y = Py - (bEdgeX ? OddOf(Gy) * Cc * (Gx > 0.0 ? 1.0 : -1.0) : 0.0);
			}
			/** Notch Q: the main cell just inside the square at that corner, on the ring that holds it; none where the ring's own
			 * fold has moved A or B (onto the corner: the triangle has no area). */
			void FindNotch(int Q)
			{
				NotchOf& N = Notches[Q];
				N = NotchOf();
				if (!PatchOn()) return;
				const double H = D.Patch.Half, In = 0.25 * D.Patch.Cell;
				N.Cx = D.Patch.X + (Q == 0 ? -H : H);
				N.Cy = D.Patch.Y + (Q == 0 ? H : -H);
				N.K = RingOf(N.Cx + (Q == 0 ? In : -In), N.Cy + (Q == 0 ? -In : In));
				if (N.K < 0) return;
				N.Cc = C(N.K);
				N.Ax = Q == 0 ? N.Cx : N.Cx - N.Cc;
				N.Ay = Q == 0 ? N.Cy - N.Cc : N.Cy;
				N.Bx = N.Ax + N.Cc;
				N.By = N.Ay + N.Cc;
				double Mx = 0.0, My = 0.0;
				Moved(N.K, (N.Ax - D.RingX[N.K]) / N.Cc, (N.Ay - D.RingY[N.K]) / N.Cc, Mx, My);
				if (Mx != N.Ax || My != N.Ay) return;
				Moved(N.K, (N.Bx - D.RingX[N.K]) / N.Cc, (N.By - D.RingY[N.K]) / N.Cc, Mx, My);
				if (Mx != N.Bx || My != N.By) return;
				N.bOn = true;
			}
			Disp At(double X, double Y, double Cell, double* Top) const
			{
				double W[MostDrawn] = {};
				double Sum = 0.0;
				for (int J = 0; J < S->ND; ++J)
				{
					W[J] = WeightOf(S->D[J].L, Cell);
					Sum += W[J] * S->D[J].A;
				}
				if (Top) *Top = Sum;
				return Displace(*S, X, Y, T, W);
			}
			/** How far a main point has eased toward the next ring's grid: over the ring's outer band (CDLOD's morph) an odd point's
			 * shift and height go to the middle of its two even neighbours' along its odd axes (Sx, Sy), so the rest grid stays the
			 * ring's own. 0 for an even point and a point that moves (Moved's). */
			double Ease(int K, double Gx, double Gy, int& Sx, int& Sy) const
			{
				Sx = OddOf(Gx) > 0.5 ? 1 : 0;
				Sy = OddOf(Gy) > 0.5 ? 1 : 0;
				if (Sx == 0 && Sy == 0) return 0.0;
				const double Cc = C(K), Px = D.RingX[K] + Gx * Cc, Py = D.RingY[K] + Gy * Cc;
				double Mx = 0.0, My = 0.0;
				Moved(K, Gx, Gy, Mx, My);
				if (Mx != Px || My != Py) return 0.0;
				return Morph(K, Cheb(Px, Py, Ex, Ey));
			}
			/** A main point's shift and height, eased (Top: its crest top, eased the same; Eased: how far). */
			Disp Eased(int K, double Gx, double Gy, double* Top, double* EaseOut) const
			{
				double X = 0.0, Y = 0.0, T0 = 0.0;
				Moved(K, Gx, Gy, X, Y);
				Disp Dd = At(X, Y, RingCell(K, X, Y), &T0);
				int Sx = 0, Sy = 0;
				const double M = Ease(K, Gx, Gy, Sx, Sy);
				if (M > 0.0)
				{
					const double Cc = C(K), Ax = X - Sx * Cc, Ay = Y - Sy * Cc, Bx = X + Sx * Cc, By = Y + Sy * Cc;
					double Ta = 0.0, Tb = 0.0;
					const Disp Da = At(Ax, Ay, RingCell(K, Ax, Ay), &Ta), Db = At(Bx, By, RingCell(K, Bx, By), &Tb);
					Dd.X = Lerp(Dd.X, 0.5 * (Da.X + Db.X), M);
					Dd.Y = Lerp(Dd.Y, 0.5 * (Da.Y + Db.Y), M);
					Dd.Z = Lerp(Dd.Z, 0.5 * (Da.Z + Db.Z), M);
					T0 = Lerp(T0, 0.5 * (Ta + Tb), M);
				}
				if (Top) *Top = T0;
				if (EaseOut) *EaseOut = M;
				return Dd;
			}
			/** A main point's shift and height where it would lie without the edge's fold (an odd point eased by the morph to the
			 * middle of its even neighbours): on the ring's outer two cells, where the fold moves points, the morph is whole, so
			 * the folded triangles and these lie on the same flat pieces. */
			Disp Unfolded(int K, double Gx, double Gy) const
			{
				const double Cc = C(K), X = D.RingX[K] + Gx * Cc, Y = D.RingY[K] + Gy * Cc;
				Disp Dd = At(X, Y, RingCell(K, X, Y), nullptr);
				const int Sx = OddOf(Gx) > 0.5 ? 1 : 0, Sy = OddOf(Gy) > 0.5 ? 1 : 0;
				const double M = Sx + Sy > 0 ? Morph(K, Cheb(X, Y, Ex, Ey)) : 0.0;
				if (M > 0.0)
				{
					const double Ax = X - Sx * Cc, Ay = Y - Sy * Cc, Bx = X + Sx * Cc, By = Y + Sy * Cc;
					const Disp Da = At(Ax, Ay, RingCell(K, Ax, Ay), nullptr), Db = At(Bx, By, RingCell(K, Bx, By), nullptr);
					Dd.X = Lerp(Dd.X, 0.5 * (Da.X + Db.X), M);
					Dd.Y = Lerp(Dd.Y, 0.5 * (Da.Y + Db.Y), M);
					Dd.Z = Lerp(Dd.Z, 0.5 * (Da.Z + Db.Z), M);
				}
				return Dd;
			}
			/** The main mesh's shift and height over a rest point (before the patch's drop): the cell of the first ring that
			 * holds it, split along its 00-11 diagonal, its corners unfolded (the trims and the next ring's edge lie on the
			 * same flat pieces). Nought beyond the rings. */
			Disp MainAt(double X, double Y) const
			{
				const int K = RingOf(X, Y);
				if (K < 0) return Disp();
				const double Cc = C(K), Fx = (X - D.RingX[K]) / Cc, Fy = (Y - D.RingY[K]) / Cc;
				const double Gx = Clamp(std::floor(Fx), -Half, Half - 1.0), Gy = Clamp(std::floor(Fy), -Half, Half - 1.0);
				const double U = Fx - Gx, V = Fy - Gy;
				const bool bLow = U >= V; // (below the diagonal: 00, 10, 11; above it 00, 01, 11)
				const Disp A = Unfolded(K, Gx, Gy), B = Unfolded(K, Gx + (bLow ? 1.0 : 0.0), Gy + (bLow ? 0.0 : 1.0)), Cc11 = Unfolded(K, Gx + 1.0, Gy + 1.0);
				const double Hi = std::max(U, V), Lo = std::min(U, V);
				Disp Out;
				Out.X = A.X + Hi * (B.X - A.X) + Lo * (Cc11.X - B.X);
				Out.Y = A.Y + Hi * (B.Y - A.Y) + Lo * (Cc11.Y - B.Y);
				Out.Z = A.Z + Hi * (B.Z - A.Z) + Lo * (Cc11.Z - B.Z);
				return Out;
			}
			/** A point of the main mesh, ring K, cells (Gx, Gy) from its middle (board), as the chip puts it (bDrop: under the
			 * patch it drops out of sight). */
			Placed Main(int K, double Gx, double Gy, bool bDrop = true) const
			{
				Placed P;
				Moved(K, Gx, Gy, P.RestX, P.RestY);
				P.Cell = RingCell(K, P.RestX, P.RestY);
				double Top = 0.0;
				const Disp Dd = Eased(K, Gx, Gy, &Top, &P.Ease);
				P.X = P.RestX + Dd.X;
				P.Y = P.RestY + Dd.Y;
				P.Z = Dd.Z;
				if (bDrop && PatchOn() && Cheb(P.RestX, P.RestY, D.Patch.X, D.Patch.Y) < D.Patch.Half - 0.001)
				{
					P.Z -= Top + PatchDrop;
					P.bDropped = true;
				}
				return P;
			}
			/** A patch point in or on notch Q: on the notch's long side, where a line across the patch's diagonals through it
			 * meets it, laid as the main mesh lays that side (its two main points, and linearly between). */
			Placed NotchPoint(int Q, double X, double Y) const
			{
				const NotchOf& N = Notches[Q];
				const double Tt = Sat(((X - N.Ax) + (Y - N.Ay)) / (2.0 * N.Cc));
				const Placed A = Main(N.K, (N.Ax - D.RingX[N.K]) / N.Cc, (N.Ay - D.RingY[N.K]) / N.Cc, false);
				const Placed B = Main(N.K, (N.Bx - D.RingX[N.K]) / N.Cc, (N.By - D.RingY[N.K]) / N.Cc, false);
				Placed P;
				P.RestX = Lerp(N.Ax, N.Bx, Tt);
				P.RestY = Lerp(N.Ay, N.By, Tt);
				P.Cell = MainCell(P.RestX, P.RestY);
				P.X = Lerp(A.X, B.X, Tt);
				P.Y = Lerp(A.Y, B.Y, Tt);
				P.Z = Lerp(A.Z, B.Z, Tt);
				P.bNotch = true;
				return P;
			}
			/** A point of the patch, cells (Gx, Gy) from its middle (board); its edge is the main mesh's own, and its points in
			 * or on a notch lie on the notch's long side. */
			Placed PatchPoint(double Gx, double Gy, bool bEdge) const
			{
				const double Rx = D.Patch.X + Gx * D.Patch.Cell, Ry = D.Patch.Y + Gy * D.Patch.Cell;
				for (int Q = 0; Q < 2; ++Q)
				{
					if (Notches[Q].bOn && NotchSide(Notches[Q], Q, Rx, Ry) >= 0.0) return NotchPoint(Q, Rx, Ry);
				}
				Placed P;
				P.RestX = Rx;
				P.RestY = Ry;
				Disp Dd;
				if (!bEdge)
				{
					// (its own sea at its eased cell, and over the ease the main mesh's surface as far as it has eased: from
					// EaseEnd it is the main mesh's own, so a snap of the square changes nothing there)
					P.Cell = PatchCell(P.RestX, P.RestY);
					Dd = At(P.RestX, P.RestY, P.Cell, nullptr);
					const double E = PatchEase(P.RestX, P.RestY);
					if (E > 0.0)
					{
						const Disp Dm = MainAt(P.RestX, P.RestY);
						Dd.X = Lerp(Dd.X, Dm.X, E);
						Dd.Y = Lerp(Dd.Y, Dm.Y, E);
						Dd.Z = Lerp(Dd.Z, Dm.Z, E);
					}
				}
				else
				{
					P.Cell = MainCell(P.RestX, P.RestY);
					const int Ke = std::max(RingOf(P.RestX, P.RestY), 0);
					const double Ce = C(Ke), Sx = D.RingX[Ke], Sy = D.RingY[Ke];
					const bool bAlongY = std::fabs(std::fabs(Gx) - PatchCells / 2) < 0.25;
					const double From = bAlongY ? P.RestY : P.RestX;
					const double AtI = std::floor((bAlongY ? P.RestY - Sy : P.RestX - Sx) / Ce);
					const double Across = std::floor((bAlongY ? P.RestX - Sx : P.RestY - Sy) / Ce + 0.5);
					double Ia = AtI, Ib = AtI, T0 = 0.0, Lx = 0.0, Ly = 0.0;
					bool bHave = false;
					for (int N = -1; N <= 2; ++N)
					{
						const double I = AtI + N;
						double Qx = 0.0, Qy = 0.0;
						Moved(Ke, bAlongY ? Across : I, bAlongY ? I : Across, Qx, Qy);
						if (N > -1 && !bHave)
						{
							const double L0 = bAlongY ? Ly : Lx, L1 = bAlongY ? Qy : Qx;
							if (L0 <= From && From <= L1 && L1 > L0)
							{
								Ia = I - 1.0;
								Ib = I;
								T0 = (From - L0) / std::max(L1 - L0, 1e-9);
								bHave = true;
							}
						}
						Lx = Qx;
						Ly = Qy;
					}
					const Disp Da = Eased(Ke, bAlongY ? Across : Ia, bAlongY ? Ia : Across, nullptr, nullptr);
					const Disp Db = Eased(Ke, bAlongY ? Across : Ib, bAlongY ? Ib : Across, nullptr, nullptr);
					Dd.X = Lerp(Da.X, Db.X, T0);
					Dd.Y = Lerp(Da.Y, Db.Y, T0);
					Dd.Z = Lerp(Da.Z, Db.Z, T0);
				}
				P.X = P.RestX + Dd.X;
				P.Y = P.RestY + Dd.Y;
				P.Z = Dd.Z;
				return P;
			}
			/** The drawn mesh over a rest point (bPatch: with the patch where it lies; its notches the main mesh's): its triangle's
			 * corners' shifts (X, Y: where each is drawn less its rest) and heights (Z), laid linearly. */
			Disp MeshAt(double X, double Y, bool bPatch) const
			{
				auto Of = [](const Placed& P)
				{
					Disp O;
					O.X = P.X - P.RestX;
					O.Y = P.Y - P.RestY;
					O.Z = P.Z;
					return O;
				};
				auto Mix3 = [](const Disp& A, double La, const Disp& B, double Lb, const Disp& C, double Lc)
				{
					Disp O;
					O.X = La * A.X + Lb * B.X + Lc * C.X;
					O.Y = La * A.Y + Lb * B.Y + Lc * C.Y;
					O.Z = La * A.Z + Lb * B.Z + Lc * C.Z;
					return O;
				};
				if (bPatch && InSquare(X, Y) && NotchAt(X, Y) < 0)
				{
					const int Pn = PatchCells, Ph = PatchCells / 2;
					const double Fx = (X - (D.Patch.X - D.Patch.Half)) / D.Patch.Cell, Fy = (Y - (D.Patch.Y - D.Patch.Half)) / D.Patch.Cell;
					const int I = std::clamp(static_cast<int>(std::floor(Fx)), 0, Pn - 1), J = std::clamp(static_cast<int>(std::floor(Fy)), 0, Pn - 1);
					const double U = Fx - I, V = Fy - J;
					auto Z = [&](int Ii, int Jj)
					{
						const bool bEdge = Ii == 0 || Jj == 0 || Ii == Pn || Jj == Pn;
						return Of(PatchPoint(Ii - Ph, Jj - Ph, bEdge));
					};
					// (below the diagonal Z00 + U (Z10 - Z00) + V (Z11 - Z10), above it Z00 + V (Z01 - Z00) + U (Z11 - Z01))
					auto Side = [](const Disp& A, double Ua, const Disp& B, double Ub, const Disp& C)
					{
						Disp O;
						O.X = A.X + Ua * (B.X - A.X) + Ub * (C.X - B.X);
						O.Y = A.Y + Ua * (B.Y - A.Y) + Ub * (C.Y - B.Y);
						O.Z = A.Z + Ua * (B.Z - A.Z) + Ub * (C.Z - B.Z);
						return O;
					};
					const Disp Z00 = Z(I, J), Z11 = Z(I + 1, J + 1);
					if (U >= V) return Side(Z00, U, Z(I + 1, J), V, Z11);
					return Side(Z00, V, Z(I, J + 1), U, Z11);
				}
				const int K = RingOf(X, Y);
				if (K >= 0)
				{
					const double Cc = C(K), Sx = D.RingX[K], Sy = D.RingY[K];
					const int Fi = static_cast<int>(std::floor((X - Sx) / Cc)), Fj = static_cast<int>(std::floor((Y - Sy) / Cc));
					int Lo[2] = { 0, 0 }, Hi[2] = { -1, -1 };
					if (K >= 1)
					{
						const double Ci = C(K);
						const int Dx = static_cast<int>(std::lround((D.RingX[K - 1] - Sx) / Ci)), Dy = static_cast<int>(std::lround((D.RingY[K - 1] - Sy) / Ci));
						Lo[0] = Dx - Half / 2;
						Hi[0] = Dx + Half / 2;
						Lo[1] = Dy - Half / 2;
						Hi[1] = Dy + Half / 2;
					}
					double Best = -1e30;
					Disp BestZ;
					bool bFound = false;
					for (int Dj = -1; Dj <= 1; ++Dj)
					{
						for (int Di = -1; Di <= 1; ++Di)
						{
							const int I = Fi + Di, J = Fj + Dj;
							if (I < -Half || I >= Half || J < -Half || J >= Half) continue;
							if (K >= 1 && I >= Lo[0] && I + 1 <= Hi[0] && J >= Lo[1] && J + 1 <= Hi[1]) continue;
							const int Ci[4][2] = { { I, J }, { I + 1, J }, { I + 1, J + 1 }, { I, J + 1 } };
							double Cx[4], Cy[4];
							for (int Q = 0; Q < 4; ++Q) Moved(K, Ci[Q][0], Ci[Q][1], Cx[Q], Cy[Q]);
							static const int Tris[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
							for (const auto& Tr : Tris)
							{
								const double Tx[3] = { Cx[Tr[0]], Cx[Tr[1]], Cx[Tr[2]] }, Ty[3] = { Cy[Tr[0]], Cy[Tr[1]], Cy[Tr[2]] };
								double L[3];
								if (!Bary(X, Y, Tx, Ty, L)) continue;
								const double Least = std::min({ L[0], L[1], L[2] });
								if (Least < -1e-7 || Least <= Best) continue;
								Disp Zs[3];
								for (int Q = 0; Q < 3; ++Q)
								{
									const int Corner = Tr[Q];
									Zs[Q] = Of(Main(K, Ci[Corner][0], Ci[Corner][1], bPatch));
								}
								Best = Least;
								BestZ = Mix3(Zs[0], L[0], Zs[1], L[1], Zs[2], L[2]);
								bFound = true;
							}
						}
					}
					if (bFound) return BestZ;
				}
				// (beyond the rings: the skirt, from the last ring's edge to the flat far square)
				const int Last = Rings() - 1;
				const double Sx = D.RingX[Last], Sy = D.RingY[Last];
				const double Dx = X - Sx, Dy = Y - Sy, Far = std::max(std::fabs(Dx), std::fabs(Dy));
				if (Far <= 0.0) return Disp();
				const int N = 2 * Half;
				// (where along the edge the point looks out from the middle; the stage's cells are the board's swapped)
				const double Ux = Dy / Far * Half, Uy = Dx / Far * Half; // stage cells on the edge
				double S0 = 0.0;
				if (Uy <= -Half + 1e-9) S0 = Ux + Half;
				else if (Ux >= Half - 1e-9) S0 = N + (Uy + Half);
				else if (Uy >= Half - 1e-9) S0 = 2 * N + (Half - Ux);
				else S0 = 3 * N + (Half - Uy);
				const int Si0 = static_cast<int>(std::floor(S0));
				for (int Ds = -2; Ds <= 2; ++Ds)
				{
					int Ax = 0, Ay = 0, Bx = 0, By = 0;
					SkirtCell(N, Si0 + Ds, Ax, Ay);
					SkirtCell(N, Si0 + Ds + 1, Bx, By);
					// (board cells: the stage's swapped)
					const Placed Pa = Main(Last, Ay, Ax), Pb = Main(Last, By, Bx);
					const double Fax = Sx + static_cast<double>(Ay) / Half * SkirtReach, Fay = Sy + static_cast<double>(Ax) / Half * SkirtReach;
					const double Fbx = Sx + static_cast<double>(By) / Half * SkirtReach, Fby = Sy + static_cast<double>(Bx) / Half * SkirtReach;
					const double Tx[2][3] = { { Pa.RestX, Pb.RestX, Fbx }, { Pa.RestX, Fbx, Fax } };
					const double Ty[2][3] = { { Pa.RestY, Pb.RestY, Fby }, { Pa.RestY, Fby, Fay } };
					const Disp Da = Of(Pa), Db = Of(Pb), Flat;
					const Disp* Tz[2][3] = { { &Da, &Db, &Flat }, { &Da, &Flat, &Flat } };
					for (int Q = 0; Q < 2; ++Q)
					{
						double L[3];
						if (!Bary(X, Y, Tx[Q], Ty[Q], L)) continue;
						if (std::min({ L[0], L[1], L[2] }) < -1e-9) continue;
						return Mix3(*Tz[Q][0], L[0], *Tz[Q][1], L[1], *Tz[Q][2], L[2]);
					}
				}
				return Disp();
			}
			double MeshZ(double X, double Y, bool bPatch) const { return MeshAt(X, Y, bPatch).Z; }
		};
	}

	double Mix(uint64_t N)
	{
		uint64_t Z = N + 0x9E3779B97F4A7C15ULL;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) / 9007199254740992.0;
	}

	double Turns(double T) { return T - std::floor(T + 0.5); }

	double Spectrum(double W, double Wp, double Hs)
	{
		if (W <= 0.0 || Wp <= 0.0) return 0.0;
		const double R = Wp / W;
		return Sq(Hs / 4.0) * 5.0 * Sq(Sq(Wp)) / (Sq(Sq(W)) * W) * std::exp(-1.25 * Sq(Sq(R)));
	}

	double SpreadAt(double W, double Wp)
	{
		const double R = W / std::max(Wp, 1e-9);
		const double Sv = R > 1.0 ? 11.5 * std::pow(R, -2.5) : 11.5 * std::pow(R, 5.0);
		return std::sqrt(2.0 / (Sv + 1.0));
	}

	double CoxMunkOf(double U) { return 0.003 + 0.00512 * std::max(U, 0.0); }
	double MonahanOf(double U) { return 3.84e-6 * std::pow(std::max(U, 0.0), 3.41); }
	double ForceOf(double U) { return std::pow(std::max(U, 0.0) / 0.836, 2.0 / 3.0); }

	void FoldOf(double U, double& Fold, double& FoldSoft, double& FarHead)
	{
		int R = 1;
		while (R < FoldRows - 1 && U > FoldTable[R][0]) ++R;
		const double T = Sat((U - FoldTable[R - 1][0]) / (FoldTable[R][0] - FoldTable[R - 1][0]));
		Fold = Lerp(FoldTable[R - 1][1], FoldTable[R][1], T);
		FoldSoft = Lerp(FoldTable[R - 1][2], FoldTable[R][2], T);
		FarHead = Lerp(FoldTable[R - 1][3], FoldTable[R][3], T);
	}

	void FetchLaw(double U, double Fetch, double& Hs, double& Wp)
	{
		const double Uu = std::max(U, 0.5);
		const double X = Gee * std::max(Fetch, 1.0) / Sq(Uu);
		Hs = 1.6e-3 * std::sqrt(X) * Sq(Uu) / Gee;
		Wp = TwoPi * 3.5 * (Gee / Uu) * std::pow(X, -0.33);
	}

	Sea Make(const Asked& A)
	{
		Sea S;
		S.Seed = A.Seed;
		auto R = [&A](uint64_t I) { return Mix(A.Seed * 31 + 17 + I); };
		const double Cap = A.Cap > 0.0 ? A.Cap : Raised;
		const double Uw = Clamp(A.WindU, 0.0, Cap);
		const double Ue = std::max(Uw, 0.5); // (the least wind the core raises a sea by)
		S.WindU = Uw;
		S.Downwind = A.DownwindR;
		S.Hs = 0.021 * Sq(Ue); // [SHOW] the show's High
		S.Wp = 0.877 * Gee / Ue;
		if (A.Fetch > 0.0)
		{
			double Hf = 0.0, Wf = 0.0;
			FetchLaw(Ue, A.Fetch, Hf, Wf);
			if (Hf < S.Hs)
			{
				S.Hs = Hf;
				S.Wp = std::max(Wf, S.Wp);
			}
		}
		S.Tp = TwoPi / S.Wp;
		S.Lp = TwoPi * Gee / Sq(S.Wp);
		S.Force = ForceOf(Uw);
		S.Qb = Clamp(0.25 + 0.1 * (S.Force - 2.0), 0.25, 0.8);
		S.CoxMunk = CoxMunkOf(Uw);
		FoldOf(Uw, S.Fold, S.FoldSoft, S.FarHead);
		const double Wd = Clamp(Uw, 2.0, 14.0); // (the wind M_StageSea's reads are laid by)
		S.TailTau = Knots(Wd, Forces, FoamLives);
		const double M0 = Sq(S.Hs / 4.0);

		// The wind sea: five bands of two trains (four with a swell), each band's pair at plus and minus its spread.
		S.bSwell = A.SwellH > 0.0 && A.SwellT > 0.0 && Gee * Sq(A.SwellT) / TwoPi >= ShortestDrawn;
		const int WindBands = S.bSwell ? Bands - 1 : Bands;
		Train Made[MostDrawn];
		int N = 0, Top = 0;
		for (int B = 0; B < WindBands; ++B)
		{
			const double Wb = S.Wp * BandAt[B];
			// (both trains of the band at least ShortestDrawn long, whatever their jitter; the bands run shorter outward)
			if (TwoPi * Gee / Sq(Wb * (1.0 + FrequencyJitter)) < ShortestDrawn) break;
			Top = B + 1;
			const double Eb = M0 * (ShareBelow(S.Wp * BandEdge[B + 1], S.Wp) - ShareBelow(S.Wp * BandEdge[B], S.Wp));
			const double Spread = SpreadAt(Wb, S.Wp);
			const double Split = 0.4 + 0.2 * R(AtSplit + B);
			for (int P = 0; P < 2; ++P)
			{
				const uint64_t I = 2 * B + P;
				Train& T = Made[N++];
				T.Band = B;
				T.Spread = Spread;
				T.W = Wb * (1.0 + FrequencyJitter * (2.0 * R(AtFrequency + I) - 1.0));
				T.K = Sq(T.W) / Gee;
				T.L = TwoPi / T.K;
				const double H = A.DownwindR + (P == 0 ? 1.0 : -1.0) * Spread * (1.0 + HeadingJitter * (2.0 * R(AtHeading + I) - 1.0));
				T.Kx = T.K * std::sin(H);
				T.Ky = T.K * std::cos(H);
				T.A = std::sqrt(2.0 * Eb * (P == 0 ? Split : 1.0 - Split));
				T.Phase0 = TwoPi * R(AtPhase + I);
			}
			S.InBands += Eb / std::max(M0, 1e-30);
		}
		if (S.bSwell)
		{
			const double Es = Sq(A.SwellH / 4.0), Split = 0.4 + 0.2 * R(AtSwell);
			for (int P = 0; P < 2; ++P)
			{
				Train& T = Made[N++];
				T.Band = -1;
				T.W = TwoPi / A.SwellT * (1.0 + FrequencyJitter * (2.0 * R(AtSwell + 1 + P) - 1.0));
				T.K = Sq(T.W) / Gee;
				T.L = TwoPi / T.K;
				const double H = A.SwellFromR + Pi + (P == 0 ? 0.5 : -0.5) * SwellApart;
				T.Kx = T.K * std::sin(H);
				T.Ky = T.K * std::cos(H);
				T.A = std::sqrt(2.0 * Es * (P == 0 ? Split : 1.0 - Split));
				T.Phase0 = TwoPi * R(AtSwell + 3 + P);
			}
		}
		// (largest first: the whitecaps' tails take the first three)
		std::stable_sort(Made, Made + N, [](const Train& X, const Train& Y) { return X.A > Y.A; });
		for (int J = 0; J < N; ++J) S.D[J] = Made[J];
		S.ND = N;

		// The pixel trains: 2 to 30 m, slices of the spectrum the displaced bands leave, the cos-2s spread.
		S.NP = std::clamp(A.PixelTrains, 0, MostPixel);
		const double CoverLo = Top > 0 ? S.Wp * BandEdge[0] : 0.0, CoverHi = Top > 0 ? S.Wp * BandEdge[Top] : 0.0;
		auto Left = [&](double W0, double W1)
		{
			double E = ShareBelow(W1, S.Wp) - ShareBelow(W0, S.Wp);
			const double Lo = std::max(W0, CoverLo), Hi = std::min(W1, CoverHi);
			if (Hi > Lo) E -= ShareBelow(Hi, S.Wp) - ShareBelow(Lo, S.Wp);
			return M0 * std::max(E, 0.0);
		};
		for (int J = 0; J < S.NP; ++J)
		{
			const double L0 = PixelLongest * std::pow(PixelShortest / PixelLongest, static_cast<double>(J) / S.NP);
			const double L1 = PixelLongest * std::pow(PixelShortest / PixelLongest, static_cast<double>(J + 1) / S.NP);
			const double W0 = std::sqrt(TwoPi * Gee / L0), W1 = std::sqrt(TwoPi * Gee / L1);
			Train& T = S.P[J];
			T.Band = -2;
			T.W = std::sqrt(W0 * W1) * (1.0 + FrequencyJitter * (2.0 * R(AtPixel + 3 * J) - 1.0));
			T.K = Sq(T.W) / Gee;
			T.L = TwoPi / T.K;
			T.Spread = SpreadAt(T.W, S.Wp);
			const double H = A.DownwindR + (J % 2 == 0 ? 1.0 : -1.0) * T.Spread * (1.0 + HeadingJitter * (2.0 * R(AtPixel + 3 * J + 1) - 1.0));
			T.Kx = T.K * std::sin(H);
			T.Ky = T.K * std::cos(H);
			T.A = std::sqrt(2.0 * Left(W0, W1));
			T.Phase0 = TwoPi * R(AtPixel + 3 * J + 2);
		}

		// The clocked pixel reads (M_StageSea's, laid by this wind): their rates, and the detail they draw.
		const double ChopScale = Clamp(Sq(Wd / 7.5), 0.35, 2.4), Steep = Clamp(0.8 + 0.4 * (Wd - 4.0) / 7.0, 0.75, 1.25);
		const double CapScale = Knots(Wd, Forces, CapSizes) / 1.9;
		S.CapTile = 160.0 * CapScale;
		S.Read[ChopA] = ReadRate(64.0 * ChopScale);
		S.Read[ChopB] = ReadRate(80.0 * ChopScale);
		S.Read[RippleA] = ReadRate(13.0);
		S.Read[RippleB] = ReadRate(17.0);
		S.Read[Gusts] = Wd / (2400.0 * 4.0);
		S.Read[GustFingers] = Wd / 600.0;
		S.Read[FoamA] = 0.8 * Wd / 7.5 / (160.0 * CapScale);
		S.Read[FoamB] = 1.0 * Wd / 7.5 / (190.0 * CapScale);
		S.Read[FoamShift] = 0.3 / 640.0;
		S.Read[CrestFrame] = Gee / S.Wp / (160.0 * CapScale); // [EST] the peak's crests running through the whitecaps' picture
		const double Detail = PictureMss * (Sq(0.11 * Steep) + Sq(0.09 * Steep) + Sq(0.08) + Sq(0.07));

		// The slopes: displaced, pixel and detail never past Cox-Munk (the roughness makes up the rest, step 7).
		double DispMss = 0.0, PixMss = 0.0;
		for (int J = 0; J < S.ND; ++J) DispMss += Sq(S.D[J].K * S.D[J].A) * 0.5;
		for (int J = 0; J < S.NP; ++J) PixMss += Sq(S.P[J].K * S.P[J].A) * 0.5;
		const double Room = std::max(S.CoxMunk - DispMss, 0.0);
		double Share = 1.0;
		if (PixMss + Detail > Room && PixMss + Detail > 0.0) Share = Room / (PixMss + Detail);
		for (int J = 0; J < S.NP; ++J) S.P[J].A *= std::sqrt(Share);
		S.DetailMss = Detail * Share;

		// The leans: sum(Q k a) over every train at most Qb, so J stays above 1 - Qb and nothing folds.
		const int All = std::max(S.ND + S.NP, 1);
		for (int J = 0; J < S.ND; ++J)
		{
			Train& T = S.D[J];
			T.Q = T.K * T.A > 0.0 ? std::min(S.Qb / (All * T.K * T.A), 1.0) : 0.0;
		}
		for (int J = 0; J < S.NP; ++J)
		{
			Train& T = S.P[J];
			T.Q = T.K * T.A > 0.0 ? std::min(S.Qb / (All * T.K * T.A), 1.0) : 0.0;
		}
		return S;
	}

	Disp Displace(const Sea& S, double X, double Y, double T, const double* W)
	{
		Disp D;
		for (int J = 0; J < S.ND; ++J)
		{
			const Train& Tr = S.D[J];
			const double Wj = W ? W[J] : 1.0;
			if (Wj <= 0.0) continue;
			const double Th = PhaseOf(Tr, X, Y, T);
			const double Sh = -Wj * Tr.Q * Tr.A * std::sin(Th) / std::max(Tr.K, 1e-12);
			D.X += Sh * Tr.Kx;
			D.Y += Sh * Tr.Ky;
			D.Z += Wj * Tr.A * std::cos(Th);
		}
		return D;
	}

	Slopes SlopesAt(const Sea& S, double X, double Y, double T, const double* Fade)
	{
		Slopes O;
		double Xx = 0.0, Yy = 0.0, Xy = 0.0;
		for (int J = 0; J < MostDrawn + MostPixel; ++J)
		{
			const bool bDrawn = J < MostDrawn;
			const int I = bDrawn ? J : J - MostDrawn;
			if (I >= (bDrawn ? S.ND : S.NP)) continue;
			const Train& Tr = bDrawn ? S.D[I] : S.P[I];
			const double F = Fade ? Fade[J] : 1.0;
			const double Th = PhaseOf(Tr, X, Y, T), C = std::cos(Th), Sn = std::sin(Th);
			const double Nx = Tr.Kx / std::max(Tr.K, 1e-12), Ny = Tr.Ky / std::max(Tr.K, 1e-12);
			const double Q = F * Tr.Q * Tr.K * Tr.A;
			O.Sx -= F * Tr.A * Tr.Kx * Sn;
			O.Sy -= F * Tr.A * Tr.Ky * Sn;
			Xx += Q * Nx * Nx * C;
			Yy += Q * Ny * Ny * C;
			Xy += Q * Nx * Ny * C;
			O.Mss += F * Sq(Tr.K * Tr.A) * 0.5;
		}
		O.Squeeze = -(Xx + Yy);
		O.J = (1.0 - Xx) * (1.0 - Yy) - Xy * Xy;
		return O;
	}

	double CoverOf(double J, const double (&Lag)[4], double Fold, double FoldSoft, double TailTau, double Shape)
	{
		const double Soft = std::max(FoldSoft, 1e-4);
		const double Head = Sat((Fold - J) / Soft) * Sat(Shape);
		double Tail = 0.0;
		for (int L = 0; L < 4; ++L) Tail += Sat((Fold - Lag[L]) / Soft) * std::exp(-TailLags[L] / std::max(TailTau, 1e-3));
		return std::max(Head, Sat(Tail) * TailShare);
	}

	double HeadShapeAt(const Sea& S, const std::vector<uint8_t>& Picture, int Size, double X, double Y, double T)
	{
		if (Size <= 0 || Picture.size() < static_cast<size_t>(Size) * Size * 4) return 1.0;
		const double Tile = std::max(S.CapTile, 1e-3), Dx = std::sin(S.Downwind), Dy = std::cos(S.Downwind);
		// (u downwind, sliding downwind with the peak's crests; v across, to the wind's right; the picture's pixels' middles
		// at (i + 1/2) / Size, as a GPU's bilinear read takes them)
		const double U = (X * Dx + Y * Dy) / Tile - Turns(S.Read[CrestFrame] * T), V = (X * Dy - Y * Dx) / Tile;
		const double Fu = (U - std::floor(U)) * Size - 0.5, Fv = (V - std::floor(V)) * Size - 0.5;
		const int I0 = static_cast<int>(std::floor(Fu)), J0 = static_cast<int>(std::floor(Fv));
		const double Au = Fu - I0, Av = Fv - J0;
		auto Red = [&](int I, int J)
		{
			I = ((I % Size) + Size) % Size;
			J = ((J % Size) + Size) % Size;
			return Picture[(static_cast<size_t>(J) * Size + I) * 4] / 255.0;
		};
		return Lerp(Lerp(Red(I0, J0), Red(I0 + 1, J0), Au), Lerp(Red(I0, J0 + 1), Red(I0 + 1, J0 + 1), Au), Av);
	}

	Foam FoamAt(const Sea& S, double X, double Y, double T, double Fold, double FoldSoft)
	{
		Foam F;
		if (Fold < 0.0 && FoldSoft < 0.0)
		{
			Fold = S.Fold;
			FoldSoft = S.FoldSoft;
		}
		double Xx = 0.0, Yy = 0.0, Xy = 0.0;
		double Lx[3] = {}, Ly[3] = {}, Lxy[3] = {}, C0[3] = {};
		for (int J = 0; J < MostDrawn + MostPixel; ++J)
		{
			const bool bDrawn = J < MostDrawn;
			const int I = bDrawn ? J : J - MostDrawn;
			if (I >= (bDrawn ? S.ND : S.NP)) continue;
			const Train& Tr = bDrawn ? S.D[I] : S.P[I];
			const double C = std::cos(PhaseOf(Tr, X, Y, T));
			const double Nx = Tr.Kx / std::max(Tr.K, 1e-12), Ny = Tr.Ky / std::max(Tr.K, 1e-12);
			const double Q = Tr.Q * Tr.K * Tr.A;
			Xx += Q * Nx * Nx * C;
			Yy += Q * Ny * Ny * C;
			Xy += Q * Nx * Ny * C;
			if (bDrawn && I < 3)
			{
				Lx[I] = Q * Nx * Nx;
				Ly[I] = Q * Ny * Ny;
				Lxy[I] = Q * Nx * Ny;
				C0[I] = C;
			}
		}
		F.J = (1.0 - Xx) * (1.0 - Yy) - Xy * Xy;
		const double Soft = std::max(FoldSoft, 1e-4);
		F.Head = Sat((Fold - F.J) / Soft);
		double Tail = 0.0;
		for (int L = 0; L < 4; ++L)
		{
			double Ax = Xx, Ay = Yy, Axy = Xy;
			for (int I = 0; I < std::min(S.ND, 3); ++I)
			{
				const Train& Tr = S.D[I];
				const double Dc = std::cos(PhaseOf(Tr, X, Y, T - TailLags[L])) - C0[I];
				Ax += Lx[I] * Dc;
				Ay += Ly[I] * Dc;
				Axy += Lxy[I] * Dc;
			}
			F.Lag[L] = (1.0 - Ax) * (1.0 - Ay) - Axy * Axy;
			Tail += Sat((Fold - F.Lag[L]) / Soft) * std::exp(-TailLags[L] / std::max(S.TailTau, 1e-3));
		}
		F.Tail = Sat(Tail) * TailShare;
		return F;
	}

	double FoamFade(const Sea& S, double Depth, double PxPerRad)
	{
		double Least = 1.0;
		for (int J = 0; J < S.ND; ++J)
		{
			if (S.D[J].A > 0.0) Least = std::min(Least, PixelFade(S.D[J].L, Depth, PxPerRad));
		}
		for (int J = 0; J < S.NP; ++J)
		{
			if (S.P[J].A > 0.0) Least = std::min(Least, PixelFade(S.P[J].L, Depth, PxPerRad));
		}
		return Least;
	}

	Foam FoamDrawn(const Sea& S, double X, double Y, double T, double Depth, double PxPerRad)
	{
		Foam F = FoamAt(S, X, Y, T);
		const double Fade = FoamFade(S, Depth, PxPerRad);
		F.Head = Lerp(S.FarHead, F.Head, Fade);
		F.Tail = Lerp(0.0, F.Tail, Fade);
		return F;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// What is drawn.

	int RingsFor(const Sea& S, int N)
	{
		double Longest = 0.0;
		for (int J = 0; J < S.ND; ++J) Longest = std::max(Longest, S.D[J].L);
		// (the longest train's: its weight gone by the last ring's edge at the least C0, and one more)
		const int Trains = Longest > 0.0 ? std::max(1, static_cast<int>(std::ceil(std::log2(Longest / (8.0 * LeastC0)) - 1e-9))) + 1 : 1;
		// (the farthest subject's: her widest patch (8 m cells) inside the last ring from the 14 m lookout, C0 1 m, ring K's
		// square holding Half - 2 of its cells from the eye wherever it has snapped)
		const int Half = std::max((N / 4) * 4, 8) / 2;
		const double Reach = PatchFarthest + 2.0 * PatchHalf + 128.0;
		const int Subject = static_cast<int>(std::ceil(std::log2(Reach / (Half - 2.0)) - 1e-9)) + 1;
		return std::clamp(std::max(Trains, Subject), 1, 2 * Half > 96 ? GridRings128 : GridRings);
	}

	namespace
	{
		/** The subject's patch at a cell (2, 4 or 8 m), for a frame whose rings are laid: its square's middle snapped so its edges
		 * lie on the even lines of every ring it touches (its half side a whole number of a cell Cm no finer than the coarsest
		 * it touches and no coarser than Coarsest, its middle that half side plus a whole number of twice Cm), none finer than
		 * its own cell; false where it cannot (bOut: its square reaches past the rings). Cm is the coarsest touched in any
		 * pass, held at Coarsest where the square first reaches a coarser ring: a snap by it may move the square off that ring
		 * (else no patch), and one on a coarser cell's lines lies on a finer one's, so the passes settle. Her place is within
		 * Cm, so within Coarsest, of the middle, as EaseEndOf asks. */
		bool PlacePatch(const Drawn& D, const double* Subject, double Cell, double Coarsest, double& Mx, double& My, bool& bOut)
		{
			const Lay L(nullptr, D, false);
			const double Half = 0.5 * PatchCells * Cell;
			Mx = Subject[0];
			My = Subject[1];
			bOut = false;
			double Cm = 0.0;
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				int Most = -1, Least = MostRings;
				for (int Q = 0; Q < 9; ++Q)
				{
					const int K = L.RingOf(Mx + ((Q % 3) - 1) * Half, My + ((Q / 3) - 1) * Half);
					if (K < 0)
					{
						bOut = true;
						return false;
					}
					Most = std::max(Most, K);
					Least = std::min(Least, K);
				}
				const int Nearest = L.RingOf(Clamp(D.EyeX, Mx - Half, Mx + Half), Clamp(D.EyeY, My - Half, My + Half));
				if (Nearest >= 0) Least = std::min(Least, Nearest);
				const double Here = L.C(Most);
				if (L.C(Least) < Cell || (Here > Coarsest && Pass > 0)) return false;
				Cm = std::max(Cm, std::min(Here, Coarsest));
				if (std::fmod(Half, Cm) != 0.0) return false;
				const double Step = 2.0 * Cm;
				const double Nx = Half + std::round((Subject[0] - Half) / Step) * Step, Ny = Half + std::round((Subject[1] - Half) / Step) * Step;
				if (Nx == Mx && Ny == My && Here <= Cm) return true;
				Mx = Nx;
				My = Ny;
			}
			return false;
		}

		/** Where the patch's ease onto the main mesh is done (from her unsnapped place), its square Half to a side and its
		 * middle snapped by twice Coarsest at most (so within Coarsest of her): every point that one of two neighbouring
		 * squares holds and the other does not lies Half - Coarsest or more from her, every point of a notch (a main cell
		 * of at most Coarsest, cut along its diagonal from the square's corner) Half - 1.5 Coarsest or more, and the patch's
		 * points beside a notch a patch cell less. From here the patch is the main mesh's surface, so its snaps move nothing. */
		double EaseEndOf(double Half, double Coarsest, double Cell) { return Half - 1.5 * Coarsest - Cell; }

		/** The rings laid round the frame's eye (its C0 and rings), and the stage's Origin with them. */
		void SnapRings(Drawn& D)
		{
			for (int K = 0; K < D.Rings; ++K)
			{
				const double Step = 2.0 * D.C0 * std::ldexp(1.0, K);
				D.RingX[K] = std::floor(D.EyeX / Step) * Step;
				D.RingY[K] = std::floor(D.EyeY / Step) * Step;
			}
			D.OriginX = D.RingX[D.Rings - 1];
			D.OriginY = D.RingY[D.Rings - 1];
		}

		/** One way to lay her patch: its cell, the coarsest cell its square may touch, how far its own sea reaches from her and
		 * where its ease onto the main mesh is done; whole where its cell holds her trains of half her length out to half her
		 * length. */
		struct PatchPlan
		{
			double Cell = 0.0, Coarsest = 0.0, Full = 0.0, EaseEnd = 0.0;
			bool bWhole = false;
		};

		/** A cell's plans, best first. Pass 0: the whole one (its cell no coarser than Need, its own sea out to Want, and the
		 * coarsest cell its square may touch the most that leaves PatchEaseLeast of ease past Want). Pass 1: the rest, each a
		 * coarser cell its square may touch (up to a third of its half side), its own sea as far as the ease then allows, so a
		 * square can sit where a whole one cannot. */
		int PlansFor(double Cell, double Want, double Need, int Pass, PatchPlan (&Out)[8])
		{
			const double Half = 0.5 * PatchCells * Cell, Top = Half / 3.0;
			double Whole = Top;
			while (Whole >= Cell && EaseEndOf(Half, Whole, Cell) - Want < PatchEaseLeast) Whole *= 0.5;
			const bool bFits = Whole >= Cell;
			int N = 0;
			if (Pass == 0)
			{
				if (bFits && Cell <= Need) Out[N++] = { Cell, Whole, Want, EaseEndOf(Half, Whole, Cell), true };
				return N;
			}
			for (double C = bFits ? (Cell <= Need ? 2.0 * Whole : Whole) : Cell; C <= Top && N < 8; C *= 2.0)
			{
				const double End = EaseEndOf(Half, C, Cell), Full = std::min(Want, End - PatchEaseLeast);
				if (Full > 0.0) Out[N++] = { Cell, C, Full, End, false };
			}
			return N;
		}

		/** How far she and the eye go while the patch fades (PatchFade at the frame's speeds). */
		struct PatchLead
		{
			double Sx = 0.0, Sy = 0.0, Ex = 0.0, Ey = 0.0;
			bool Any() const { return Sx != 0.0 || Sy != 0.0 || Ex != 0.0 || Ey != 0.0; }
		};

		/** Whether a plan's square can still be laid for her as far as the lead goes (an eighth of it at a time, the eye moved on
		 * as far and the rings snapped round it; at the end her place a snap more along her way past the eye), so a fade out
		 * begun now ends before it could not. */
		bool Lasts(const Drawn& D, const double* Subject, const PatchPlan& P, const PatchLead& A)
		{
			if (!A.Any()) return true;
			Drawn F = D;
			F.Patch = PatchAt();
			const double Rx = A.Sx - A.Ex, Ry = A.Sy - A.Ey, R = std::hypot(Rx, Ry);
			for (int Q = 1; Q <= 8; ++Q)
			{
				const double T = 0.125 * Q;
				F.EyeX = D.EyeX + T * A.Ex;
				F.EyeY = D.EyeY + T * A.Ey;
				SnapRings(F);
				double At[2] = { Subject[0] + T * A.Sx, Subject[1] + T * A.Sy };
				if (Q == 8 && R > 0.0)
				{
					At[0] += 2.0 * P.Coarsest * Rx / R;
					At[1] += 2.0 * P.Coarsest * Ry / R;
				}
				double Mx = 0.0, My = 0.0;
				bool bOut = false;
				if (!PlacePatch(F, At, P.Cell, P.Coarsest, Mx, My, bOut)) return false;
			}
			return true;
		}

		/** The patch from a plan, its square laid at (Mx, My) for her at Subject (shown whole: the caller sets its presence). */
		void SetPatch(PatchAt& P, const PatchPlan& Pl, const double* Subject, double Mx, double My)
		{
			P.X = Mx;
			P.Y = My;
			P.Half = 0.5 * PatchCells * Pl.Cell;
			P.Cell = Pl.Cell;
			P.SubjectX = Subject[0];
			P.SubjectY = Subject[1];
			P.Full = Pl.Full;
			P.EaseEnd = Pl.EaseEnd;
			P.Snap = 2.0 * Pl.Coarsest;
			P.bWhole = Pl.bWhole;
			P.bOn = true;
		}

		/** A fresh patch for her: the first plan (whole ones first, then the rest; cells PatchCell, twice it, then PatchFinest)
		 * whose square can be laid now and lasts the lead. Passes: 0 whole ones only, 1 the rest only, 2 both. bOut: some
		 * square reached past the rings. */
		bool ChoosePatch(const Drawn& D, const double* Subject, double Want, double Need, const PatchLead& A, int Passes, PatchAt& Out, bool& bOut)
		{
			static constexpr double Cells[3] = { PatchCell, 2.0 * PatchCell, PatchFinest };
			bOut = false;
			for (int Pass = Passes == 1 ? 1 : 0; Pass <= (Passes == 0 ? 0 : 1); ++Pass)
			{
				for (const double Cell : Cells)
				{
					PatchPlan Plans[8];
					const int N = PlansFor(Cell, Want, Need, Pass, Plans);
					for (int I = 0; I < N; ++I)
					{
						double Mx = 0.0, My = 0.0;
						bool bOutHere = false;
						if (!PlacePatch(D, Subject, Plans[I].Cell, Plans[I].Coarsest, Mx, My, bOutHere))
						{
							bOut = bOut || bOutHere;
							continue;
						}
						if (!Lasts(D, Subject, Plans[I], A)) continue;
						SetPatch(Out, Plans[I], Subject, Mx, My);
						return true;
					}
				}
			}
			return false;
		}
	}

	double NeedCell(const Sea& S, double Long)
	{
		double Shortest = 0.0;
		for (int J = 0; J < S.ND; ++J)
		{
			if (S.D[J].L >= 0.5 * Long && (Shortest == 0.0 || S.D[J].L < Shortest)) Shortest = S.D[J].L;
		}
		return Shortest > 0.0 ? Shortest / 12.0 : 1e30;
	}

	double MainUnder(const Drawn& D, const double* Subject, double Long)
	{
		if (!Subject) return 0.0;
		const Lay L(nullptr, D, false);
		const double H = 0.5 * std::max(Long, 1.0);
		double Most = 0.0;
		for (int Q = 0; Q < 9; ++Q) Most = std::max(Most, L.MainCell(Subject[0] + ((Q % 3) - 1) * H, Subject[1] + ((Q / 3) - 1) * H));
		return Most;
	}

	Drawn DrawnFor(const Sea& S, const double (&Eye)[3], const double (&Look)[3], double LensDeg, const double* Subject, double SeaT, const Drawn* Last,
		bool bCut, int Rings, int N, double ScreenPx, double SubjectLong, double FrameSeconds)
	{
		(void)Look;
		Drawn D;
		D.EyeX = Eye[0];
		D.EyeY = Eye[1];
		D.Rings = std::clamp(Rings, 1, MostRings);
		D.N = std::max((N / 4) * 4, 8);
		D.SeaT = SeaT;
		const bool bHeld = Last && !bCut;
		D.EyeBeforeX = bHeld ? Last->EyeX : D.EyeX;
		D.EyeBeforeY = bHeld ? Last->EyeY : D.EyeY;
		D.SeaTBefore = bHeld ? Last->SeaT : SeaT;
		D.PxPerRad = ScreenPx / (2.0 * std::tan(Clamp(LensDeg, 1.0, 170.0) * Pi / 360.0));
		if (bHeld)
		{
			D.C0 = Last->C0;
		}
		else
		{
			// (the power of two nearest the eye's height over 14, 0.5 to 8 m)
			const double Want = std::max(Eye[2], 0.1) / 14.0;
			D.C0 = Clamp(std::ldexp(1.0, static_cast<int>(std::lround(std::log2(Want)))), LeastC0, MostC0);
		}
		// (the last ring's edge has every displaced train's weight gone)
		double Longest = 0.0;
		for (int J = 0; J < S.ND; ++J) Longest = std::max(Longest, S.D[J].L);
		while (D.C0 * std::ldexp(1.0, D.Rings) < Longest / 8.0 && D.C0 < 1024.0) D.C0 *= 2.0;
		SnapRings(D);
		D.Patch = PatchAt();
		D.Patch.Presence = 0.0;
		const PatchAt* Was = bHeld ? &Last->Patch : nullptr;
		const bool bWasOn = Was && Was->bOn && Was->Cell > 0.0;
		// (her place: the shot's subject; with none, while the last frame's patch is still shown, where it left her, so it
		// fades out there)
		double Kept[2] = { 0.0, 0.0 };
		const double* Her = Subject;
		if (!Her && bWasOn && Was->Presence > 0.0)
		{
			Kept[0] = Was->SubjectX;
			Kept[1] = Was->SubjectY;
			Her = Kept;
		}
		const double Long = std::max(SubjectLong, 1.0), Want = 0.5 * Long, Need = NeedCell(S, Long), Wants = std::min(Need, PatchCell);
		bool bForced = false;
		if (Her && !bHeld)
		{
			// At a cut, shown whole at once where she wants it: C0 as the eye has it, then doubled, the first that lays a whole
			// patch; else the first that lays any; while her square reaches past the rings, doubled again, up to MostC0.
			if (MainUnder(D, Her, Long) > Wants)
			{
				const double Base = D.C0;
				double Got = 0.0;
				for (int Passes = 0; Passes < 2 && Got == 0.0; ++Passes)
				{
					for (double C = Base;; C *= 2.0)
					{
						D.C0 = C;
						SnapRings(D);
						bool bOut = false;
						PatchAt P;
						if (ChoosePatch(D, Her, Want, Need, PatchLead(), Passes, P, bOut))
						{
							D.Patch = P;
							Got = C;
							break;
						}
						if (C >= MostC0 || (C > Base && !bOut)) break;
					}
				}
				D.C0 = Got > 0.0 ? Got : Base;
				SnapRings(D);
				D.Patch.Presence = 1.0;
				D.Patch.WantCell = Wants;
			}
		}
		else if (Her)
		{
			// A held frame: the patch fades in and out over PatchFade seconds of the frame's own time (Presence), and is put on,
			// taken off or changed only at nought, where it is the main mesh's own surface; it begins to fade out while its
			// square will still last the fade (her place and the eye's PatchFade ahead at their speeds, and a snap more).
			const double Dt = std::max(FrameSeconds >= 0.0 ? FrameSeconds : SeaT - Last->SeaT, 0.0), Step = Dt / PatchFade;
			// (a new subject far from the last: the last frame's patch fades out where it was, and hers comes in after it)
			const bool bJump = Subject && Was->bSubject &&
				std::hypot(Subject[0] - Was->SubjectX, Subject[1] - Was->SubjectY) > std::max(30.0, 60.0 * Dt);
			PatchLead A;
			if (Dt > 0.0)
			{
				const double Go = PatchFade / Dt;
				if (Subject && Was->bSubject && !bJump)
				{
					A.Sx = (Subject[0] - Was->SubjectX) * Go;
					A.Sy = (Subject[1] - Was->SubjectY) * Go;
				}
				A.Ex = (D.EyeX - Last->EyeX) * Go;
				A.Ey = (D.EyeY - Last->EyeY) * Go;
			}
			if (bWasOn)
			{
				const double Where[2] = { bJump || !Subject ? Was->SubjectX : Her[0], bJump || !Subject ? Was->SubjectY : Her[1] };
				const PatchPlan Pl = { Was->Cell, 0.5 * Was->Snap, Was->Full, Was->EaseEnd, Was->bWhole };
				double Mx = 0.0, My = 0.0;
				bool bOut = false;
				if (PlacePatch(D, Where, Pl.Cell, Pl.Coarsest, Mx, My, bOut))
				{
					// (it stays while she still wants it and its square lasts the lead; a part patch makes way for a whole one that
					// can be laid and lasts)
					bool bStay = Subject && !bJump && MainUnder(D, Subject, Long) > PatchStay * Was->WantCell && Lasts(D, Where, Pl, A);
					if (bStay && !Pl.bWhole)
					{
						PatchAt Better;
						bool bOutWhole = false;
						bStay = !ChoosePatch(D, Subject, Want, Need, A, 0, Better, bOutWhole);
					}
					const double P = bStay ? std::min(Was->Presence + Step, 1.0) : std::max(Was->Presence - Step, 0.0);
					if (bStay || P > 0.0)
					{
						SetPatch(D.Patch, Pl, Where, Mx, My);
						D.Patch.Presence = P;
						D.Patch.WantCell = Was->WantCell;
					}
				}
				else
				{
					bForced = Was->Presence > 0.0;
				}
			}
			if (!D.Patch.bOn && Subject && MainUnder(D, Subject, Long) > Wants)
			{
				// (a fresh one: at nought where the last frame's has just gone, else a step of its fade in)
				PatchAt P;
				bool bOut = false;
				if (ChoosePatch(D, Subject, Want, Need, A, 2, P, bOut))
				{
					D.Patch = P;
					D.Patch.Presence = bWasOn ? 0.0 : std::min(Step, 1.0);
					D.Patch.WantCell = Wants;
				}
			}
		}
		if (Subject && !D.Patch.bOn)
		{
			D.Patch.SubjectX = Subject[0];
			D.Patch.SubjectY = Subject[1];
		}
		D.Patch.bSubject = Subject != nullptr;
		D.Patch.bForced = bForced;
		if (!D.Patch.bOn)
		{
			D.Patch.X = D.OriginX;
			D.Patch.Y = D.OriginY;
			D.Patch.Presence = 0.0;
		}
		return D;
	}

	double CellOf(const Drawn& D, int Ring) { return D.C0 * std::ldexp(1.0, Ring); }

	int RingOf(const Drawn& D, double X, double Y) { return Lay(nullptr, D, false).RingOf(X, Y); }

	double MorphOf(const Drawn& D, int K, double Dist) { return Lay(nullptr, D, false).Morph(K, Dist); }

	double CellAt(const Drawn& D, double X, double Y) { return Lay(nullptr, D, false).CellAt(X, Y); }

	double MainCellAt(const Drawn& D, double X, double Y) { return Lay(nullptr, D, false).MainCell(X, Y); }

	double WeightOf(double L, double Cell)
	{
		const double Lg = std::max(L, 1e-3), E = Lg * 0.125;
		return Sat((E - Cell) / std::max(E - Lg / 12.0, 1e-9));
	}

	double WeightAt(const Sea& S, const Drawn& D, double X, double Y, int J)
	{
		if (J < 0 || J >= S.ND) return 0.0;
		return Lay(&S, D, false).WeightAt(J, X, Y);
	}

	void WeightsAt(const Sea& S, const Drawn& D, double X, double Y, double* W)
	{
		const Lay L(&S, D, false);
		for (int J = 0; J < MostDrawn; ++J) W[J] = J < S.ND ? L.WeightAt(J, X, Y) : 0.0;
	}

	double HeightDrawn(const Sea& S, const Drawn& D, double X, double Y)
	{
		double W[MostDrawn];
		WeightsAt(S, D, X, Y, W);
		return Displace(S, X, Y, D.SeaT, W).Z;
	}

	double HeightAt(const Sea& S, double X, double Y, double T)
	{
		// (three Newton steps on rest + shift(rest) = (X, Y), from the point itself)
		double Rx = X, Ry = Y;
		for (int It = 0; It < 3; ++It)
		{
			double Fx = Rx - X, Fy = Ry - Y, Jxx = 1.0, Jyy = 1.0, Jxy = 0.0;
			for (int J = 0; J < S.ND; ++J)
			{
				const Train& Tr = S.D[J];
				const double Th = PhaseOf(Tr, Rx, Ry, T), Sn = std::sin(Th), C = std::cos(Th);
				const double Nx = Tr.Kx / std::max(Tr.K, 1e-12), Ny = Tr.Ky / std::max(Tr.K, 1e-12);
				Fx -= Tr.Q * Tr.A * Nx * Sn;
				Fy -= Tr.Q * Tr.A * Ny * Sn;
				const double Q = Tr.Q * Tr.A * C;
				Jxx -= Q * Nx * Tr.Kx;
				Jyy -= Q * Ny * Tr.Ky;
				Jxy -= Q * Nx * Tr.Ky;
			}
			const double Det = Jxx * Jyy - Jxy * Jxy;
			if (std::fabs(Det) < 1e-9) break;
			Rx -= (Jyy * Fx - Jxy * Fy) / Det;
			Ry -= (Jxx * Fy - Jxy * Fx) / Det;
		}
		return Displace(S, Rx, Ry, T).Z;
	}

	double ChordBound(const Sea& S, const Drawn& D, double X, double Y)
	{
		// (the cells of the triangle about the point lie between the least and the most over the square one diagonal
		// round it: the cell grows with the distance from the eye, and from the subject inside her patch, so the square's
		// corners and its points nearest the eye and the subject bound it)
		// (over the patch's ease the drawn surface goes to the main mesh's, whose triangles are its cell's: the main mesh's
		// cell joins the most there, and the square round the point reaches as far)
		const Lay L(&S, D, false);
		const double C0 = L.CellAt(X, Y);
		const double Diag = std::sqrt(2.0) * 2.0 * std::min(L.BandAt(X, Y) > 0.0 ? std::max(C0, L.MainCell(X, Y)) : C0, 1e6);
		double Least = C0, Most = C0;
		auto Take = [&](double Px, double Py)
		{
			const double C = L.CellAt(Px, Py);
			Least = std::min(Least, C);
			Most = std::max(Most, L.BandAt(Px, Py) > 0.0 ? std::max(C, L.MainCell(Px, Py)) : C);
		};
		for (int Q = 0; Q < 9; ++Q) Take(X + ((Q % 3) - 1) * Diag, Y + ((Q / 3) - 1) * Diag);
		Take(Clamp(D.EyeX, X - Diag, X + Diag), Clamp(D.EyeY, Y - Diag, Y + Diag));
		if (D.Patch.bOn) Take(Clamp(D.Patch.SubjectX, X - Diag, X + Diag), Clamp(D.Patch.SubjectY, Y - Diag, Y + Diag));
		const double Side = std::sqrt(2.0) * std::min(Most, 1e6);
		double B = 0.0;
		for (int J = 0; J < S.ND; ++J)
		{
			// (each train: its sag over the triangle's longest side at the most weight it has there, and the most its
			// weight changes across the triangle, which the mesh lays straight)
			const double High = WeightOf(S.D[J].L, Least), Low = WeightOf(S.D[J].L, Most);
			B += S.D[J].A * (High * (1.0 - std::cos(Pi * std::min(Side / std::max(S.D[J].L, 1e-3), 1.0))) + (High - Low));
		}
		return B;
	}

	double CrestTop(const Sea& S, const Drawn& D, double X, double Y)
	{
		const Lay L(&S, D, false);
		double B = 0.0;
		for (int J = 0; J < S.ND; ++J) B += L.WeightAt(J, X, Y) * S.D[J].A;
		return B;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The meshes.

	namespace
	{
		struct Builder
		{
			Mesh& M;
			int Add(double X, double Y, int Ring, int Kind, bool bAxes)
			{
				GridPoint P;
				P.X = static_cast<float>(X);
				P.Y = static_cast<float>(Y);
				P.Ring = Ring;
				P.Kind = Kind;
				if (bAxes)
				{
					P.AxisX = static_cast<float>(std::fabs(X - 2.0 * std::floor(X * 0.5)));
					P.AxisY = static_cast<float>(std::fabs(Y - 2.0 * std::floor(Y * 0.5)));
				}
				M.Points.push_back(P);
				return static_cast<int>(M.Points.size()) - 1;
			}
			/** A triangle, the way round that faces up as AAirStage::BuildSea lays them: (P1 - P2) x (P0 - P2) up. */
			void Tri(int A, int B, int C, double Ax, double Ay, double Bx, double By, double Cx, double Cy)
			{
				const double Z = (Bx - Cx) * (Ay - Cy) - (By - Cy) * (Ax - Cx);
				M.Triangles.push_back(A);
				if (Z >= 0.0)
				{
					M.Triangles.push_back(B);
					M.Triangles.push_back(C);
				}
				else
				{
					M.Triangles.push_back(C);
					M.Triangles.push_back(B);
				}
			}
			/** A cell's two triangles, split along its 00-11 diagonal (so a fully morphed ring is the next one's grid). */
			void Cell(int I00, int I10, int I11, int I01, double X, double Y)
			{
				Tri(I00, I10, I11, X, Y, X + 1, Y, X + 1, Y + 1);
				Tri(I00, I11, I01, X, Y, X + 1, Y + 1, X, Y + 1);
			}
		};
	}

	Mesh Grid(int N, int Rings)
	{
		Mesh M;
		Builder B{ M };
		N = std::max((N / 4) * 4, 8);
		Rings = std::clamp(Rings, 1, MostRings);
		const int H = N / 2, Q = H / 2, Side = N + 1;
		std::vector<int> Edge;
		for (int Ring = 0; Ring < Rings; ++Ring)
		{
			std::vector<int> Idx(Side * Side, -1);
			auto In = [&](int X, int Y) { return X > -Q && X < Q + 1 && Y > -Q && Y < Q + 1; };
			for (int Y = -H; Y <= H; ++Y)
			{
				for (int X = -H; X <= H; ++X)
				{
					if (Ring == 0 || !In(X, Y)) Idx[(Y + H) * Side + (X + H)] = B.Add(X, Y, Ring, 0, true);
				}
			}
			for (int J = -H; J < H; ++J)
			{
				for (int I = -H; I < H; ++I)
				{
					if (Ring > 0 && I >= -Q && I <= Q && J >= -Q && J <= Q) continue;
					auto At = [&](int X, int Y) { return Idx[(Y + H) * Side + (X + H)]; };
					B.Cell(At(I, J), At(I + 1, J), At(I + 1, J + 1), At(I, J + 1), I, J);
				}
			}
			if (Ring >= 1)
			{
				// (the L trim between the inner ring and this one's frame, in each of its four ways)
				for (int Kind = 1; Kind <= 4; ++Kind)
				{
					const int Dx = (Kind - 1) & 1, Dy = (Kind - 1) >> 1;
					const int Ic = Dx == 0 ? Q : -Q, Jr = Dy == 0 ? Q : -Q;
					std::vector<int> Own(Side * Side, -1);
					auto At = [&](int X, int Y)
					{
						int& I = Own[(Y + H) * Side + (X + H)];
						if (I < 0) I = B.Add(X, Y, Ring, Kind, true);
						return I;
					};
					for (int J = -Q; J <= Q; ++J) B.Cell(At(Ic, J), At(Ic + 1, J), At(Ic + 1, J + 1), At(Ic, J + 1), Ic, J);
					for (int I = -Q; I <= Q; ++I)
					{
						if (I == Ic) continue;
						B.Cell(At(I, Jr), At(I + 1, Jr), At(I + 1, Jr + 1), At(I, Jr + 1), I, Jr);
					}
				}
			}
			if (Ring == Rings - 1)
			{
				Edge.resize(4 * N);
				for (int S = 0; S < 4 * N; ++S)
				{
					int X = 0, Y = 0;
					SkirtCell(N, S, X, Y);
					Edge[S] = Idx[(Y + H) * Side + (X + H)];
				}
			}
		}
		// The flat skirt: from the last ring's edge out to SkirtReach.
		std::vector<int> Far(4 * N);
		for (int S = 0; S < 4 * N; ++S)
		{
			int X = 0, Y = 0;
			SkirtCell(N, S, X, Y);
			Far[S] = B.Add(static_cast<double>(X) / H, static_cast<double>(Y) / H, Rings - 1, 6, false);
		}
		const double Out = 1e6;
		for (int S = 0; S < 4 * N; ++S)
		{
			const int T = (S + 1) % (4 * N);
			int Ax = 0, Ay = 0, Bx = 0, By = 0;
			SkirtCell(N, S, Ax, Ay);
			SkirtCell(N, T, Bx, By);
			B.Tri(Edge[S], Edge[T], Far[T], Ax, Ay, Bx, By, Bx * Out / H, By * Out / H);
			B.Tri(Edge[S], Far[T], Far[S], Ax, Ay, Bx * Out / H, By * Out / H, Ax * Out / H, Ay * Out / H);
		}
		return M;
	}

	Mesh Patch()
	{
		Mesh M;
		Builder B{ M };
		const int H = PatchCells / 2, Side = PatchCells + 1;
		for (int Y = -H; Y <= H; ++Y)
		{
			for (int X = -H; X <= H; ++X) B.Add(X, Y, -1, (std::abs(X) == H || std::abs(Y) == H) ? 7 : 0, false);
		}
		for (int J = -H; J < H; ++J)
		{
			for (int I = -H; I < H; ++I)
			{
				auto At = [&](int X, int Y) { return (Y + H) * Side + (X + H); };
				B.Cell(At(I, J), At(I + 1, J), At(I + 1, J + 1), At(I, J + 1), I, J);
			}
		}
		return M;
	}

	Placed PlaceOf(const Sea& S, const Drawn& D, const GridPoint& P, bool bBefore)
	{
		const Lay L(&S, D, bBefore);
		// (the stage's cells are the board's swapped)
		const double Gx = P.Y, Gy = P.X;
		Placed Out;
		if (P.Ring >= 0)
		{
			const int K = std::min(P.Ring, L.Rings() - 1);
			const double Cc = L.C(K), Sx = D.RingX[K], Sy = D.RingY[K];
			bool bGone = false;
			if (P.Kind >= 1 && P.Kind <= 4)
			{
				if (K < 1)
				{
					bGone = true;
				}
				else
				{
					const int Bx = static_cast<int>(std::floor((D.RingX[K - 1] - Sx) / Cc + 0.5)), By = static_cast<int>(std::floor((D.RingY[K - 1] - Sy) / Cc + 0.5));
					bGone = P.Kind != 1 + By + 2 * Bx; // (the stage's dx is the board's dy)
				}
			}
			if (bGone)
			{
				Out.RestX = Out.X = Sx;
				Out.RestY = Out.Y = Sy;
				Out.Cell = 1e30;
				Out.bCollapsed = true;
				return Out;
			}
			if (P.Kind == 6)
			{
				Out.RestX = Out.X = Sx + Gx * SkirtReach;
				Out.RestY = Out.Y = Sy + Gy * SkirtReach;
				Out.Cell = 1e30;
				return Out;
			}
			return L.Main(K, Gx, Gy);
		}
		if (L.PatchOn()) return L.PatchPoint(Gx, Gy, P.Kind == 7);
		Out.RestX = Out.X = D.Patch.X;
		Out.RestY = Out.Y = D.Patch.Y;
		Out.Z = -PatchDrop;
		Out.Cell = 1e30;
		Out.bCollapsed = true;
		return Out;
	}

	double DrawnMeshZ(const Sea& S, const Drawn& D, double X, double Y) { return Lay(&S, D, false).MeshZ(X, Y, true); }

	double MainMeshZ(const Sea& S, const Drawn& D, double X, double Y) { return Lay(&S, D, false).MeshZ(X, Y, false); }

	double DrawnSurfaceZ(const Sea& S, const Drawn& D, double X, double Y, double* RestOut)
	{
		// (the rest point whose drawn place is (X, Y): the fixed point of rest = (X, Y) less the mesh's shift there, which the
		// shift's lean (its slopes at most Qb, a quarter) draws in four times a step, to a tenth of a micrometre)
		const Lay L(&S, D, false);
		double Rx = X, Ry = Y;
		Disp M = L.MeshAt(Rx, Ry, true);
		for (int It = 0; It < 60; ++It)
		{
			const double Nx = X - M.X, Ny = Y - M.Y;
			const bool bDone = std::fabs(Nx - Rx) < 1e-7 && std::fabs(Ny - Ry) < 1e-7;
			Rx = Nx;
			Ry = Ny;
			M = L.MeshAt(Rx, Ry, true);
			if (bDone) break;
		}
		if (RestOut)
		{
			RestOut[0] = Rx;
			RestOut[1] = Ry;
		}
		return M.Z;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The ride.

	RideFilter FilterFor(const Sea& S, double RollT)
	{
		RideFilter F;
		F.HeaveW = TwoPi / (HeaveNatural * std::max(S.Tp, 0.1));
		F.HeaveZeta = HeaveDamping;
		F.RollW = TwoPi / std::max(RollT, 0.5);
		F.RollZeta = RollDamping;
		return F;
	}

	RideC RideCoef(const Train& T, double Long, double Beam, double Hdg, double Speed, const RideFilter& F)
	{
		using Cx = std::complex<double>;
		// The plane by least squares through her stations: along u = -L/2, -L/4, 0, L/4, L/2 (ends 0.5), across
		// v = -B/2, 0, B/2 (ends 0.5), each the train's e^(i K.r) times its height. The stations' sums part into along
		// times across, and the symmetry leaves each a cosine or a sine: two sines a train.
		const double Ka = T.Kx * std::sin(Hdg) + T.Ky * std::cos(Hdg); // (K along her heading)
		const double Kr = T.Kx * std::cos(Hdg) - T.Ky * std::sin(Hdg); // (K toward her starboard side)
		const double Qa = Ka * Long * 0.25, Hb = Kr * Beam * 0.5;
		const double C1 = std::cos(Qa), S1 = std::sin(Qa), C2 = C1 * C1 - S1 * S1, S2 = 2.0 * S1 * C1;
		const double Cb = std::cos(Hb), Sb = std::sin(Hb);
		const double Along = 1.0 + 2.0 * C1 + C2;                  // sum w e^(i u Ka)
		const Cx AlongU(0.0, 0.5 * Long * (S1 + S2));               // sum w u e^(i u Ka)
		const double Across = 1.0 + Cb;                             // sum w e^(i v Kr)
		const Cx AcrossV(0.0, 0.5 * Beam * Sb);                     // sum w v e^(i v Kr)
		const double Sw = 8.0, Su = 0.75 * Long * Long, Sv = Beam * Beam;
		const Cx H = T.A * Along * Across / Sw;
		const Cx P = T.A * AlongU * Across / std::max(Su, 1e-12);
		const Cx R = T.A * Along * AcrossV / std::max(Sv, 1e-12);
		// (through her filters at the frequency she meets the train at, W - K.V)
		const double We = T.W - Ka * Speed;
		auto G = [We](double Wn, double Zeta)
		{
			const double Rr = We / std::max(Wn, 1e-9);
			return 1.0 / Cx(1.0 - Rr * Rr, -2.0 * Zeta * Rr);
		};
		const Cx Gh = G(F.HeaveW, F.HeaveZeta), Gr = G(F.RollW, F.RollZeta);
		RideC C;
		C.Heave = Gh * H;
		C.Pitch = Gh * P * (180.0 / Pi);
		C.Roll = -(Gr * R) * (180.0 / Pi);
		return C;
	}

	void RideTrains(const Sea& S, double Long, double Beam, double Hdg, double Speed, double RollT, HullRide& Out)
	{
		const RideFilter F = FilterFor(S, RollT);
		Out.N = 0;
		for (int J = 0; J < S.ND; ++J)
		{
			if (S.D[J].L < RideShortest * Long) continue;
			Out.Of[Out.N] = J;
			Out.C[Out.N] = RideCoef(S.D[J], Long, Beam, Hdg, Speed, F);
			++Out.N;
		}
	}

	void RideAt(const Sea& S, const HullRide& H, double X, double Y, double SeaT, const double* W, double& Heave, double& Pitch, double& Roll)
	{
		Heave = Pitch = Roll = 0.0;
		for (int I = 0; I < H.N; ++I)
		{
			const int J = H.Of[I];
			const double Wj = W ? W[J] : 1.0;
			const double Th = PhaseOf(S.D[J], X, Y, SeaT), C = std::cos(Th) * Wj, Sn = std::sin(Th) * Wj;
			Heave += H.C[I].Heave.real() * C - H.C[I].Heave.imag() * Sn;
			Pitch += H.C[I].Pitch.real() * C - H.C[I].Pitch.imag() * Sn;
			Roll += H.C[I].Roll.real() * C - H.C[I].Roll.imag() * Sn;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// SeaNear.

	namespace
	{
		/** The main mesh's cells, quickly: ring K's at a Chebyshev distance from the eye (its weights' morph two cells ahead),
		 * and the cell from the distance alone (a ring's snap only moves its edge where its morph is already whole, and there
		 * the next ring's cell is the same). */
		struct NearCells
		{
			int Rings = 1;
			double Thr[MostRings] = {}, Ck[MostRings] = {}, A2[MostRings] = {}, Inv[MostRings] = {};
			explicit NearCells(const Lay& L)
			{
				Rings = L.Rings();
				for (int K = 0; K < Rings; ++K)
				{
					const double Cc = L.C(K), B = (L.Half - 3.0) * Cc, A = B - L.Half * Cc * 0.125;
					Ck[K] = Cc;
					A2[K] = A - 2.0 * Cc;
					Inv[K] = 1.0 / std::max(B - A, 1e-9);
					Thr[K] = (L.Half - 4) * Cc;
				}
			}
			double Ring(int K, double Dist) const { return Ck[K] * (1.0 + Sat((Dist - A2[K]) * Inv[K])); }
			double ByDist(double Dist) const
			{
				int K = 0;
				while (K < Rings - 1 && Dist > Thr[K]) ++K;
				return Ring(K, Dist);
			}
		};

		/** Sum over the first NJ trains of Xc[U][I] A[U] - Xs[U][I] B[U] (the tables a train a Stride apart) for I in [Lo, Hi),
		 * into Row: sixteen or eight nodes at a time, the trains all at once into each, so the compilers keep the sums in
		 * vector registers (the last sixteen or eight overlap those before, which are laid again the same). */
		void SumRun(const float* Xc, const float* Xs, size_t Stride, const float* A, const float* B, int NJ, int Lo, int Hi, float* Row)
		{
			int I = Lo;
			if (Hi - Lo >= 16)
			{
				for (; I < Hi; I += 16)
				{
					// (a last few, eight or under, go to the eight-wide pass below)
					if (Hi - I <= 8 && I > Lo) break;
					if (I + 16 > Hi) I = Hi - 16;
					float A0 = 0.0f, A1 = 0.0f, A2 = 0.0f, A3 = 0.0f, A4 = 0.0f, A5 = 0.0f, A6 = 0.0f, A7 = 0.0f;
					float A8 = 0.0f, A9 = 0.0f, A10 = 0.0f, A11 = 0.0f, A12 = 0.0f, A13 = 0.0f, A14 = 0.0f, A15 = 0.0f;
					const float* __restrict C = Xc + I;
					const float* __restrict Sn = Xs + I;
					for (int U = 0; U < NJ; ++U, C += Stride, Sn += Stride)
					{
						const float Ca = A[U], Sb = B[U];
						A0 += C[0] * Ca - Sn[0] * Sb;
						A1 += C[1] * Ca - Sn[1] * Sb;
						A2 += C[2] * Ca - Sn[2] * Sb;
						A3 += C[3] * Ca - Sn[3] * Sb;
						A4 += C[4] * Ca - Sn[4] * Sb;
						A5 += C[5] * Ca - Sn[5] * Sb;
						A6 += C[6] * Ca - Sn[6] * Sb;
						A7 += C[7] * Ca - Sn[7] * Sb;
						A8 += C[8] * Ca - Sn[8] * Sb;
						A9 += C[9] * Ca - Sn[9] * Sb;
						A10 += C[10] * Ca - Sn[10] * Sb;
						A11 += C[11] * Ca - Sn[11] * Sb;
						A12 += C[12] * Ca - Sn[12] * Sb;
						A13 += C[13] * Ca - Sn[13] * Sb;
						A14 += C[14] * Ca - Sn[14] * Sb;
						A15 += C[15] * Ca - Sn[15] * Sb;
					}
					float* __restrict R = Row + I;
					R[0] = A0;
					R[1] = A1;
					R[2] = A2;
					R[3] = A3;
					R[4] = A4;
					R[5] = A5;
					R[6] = A6;
					R[7] = A7;
					R[8] = A8;
					R[9] = A9;
					R[10] = A10;
					R[11] = A11;
					R[12] = A12;
					R[13] = A13;
					R[14] = A14;
					R[15] = A15;
				}
				if (I >= Hi) return;
				I = Hi - 8;
			}
			if (Hi - I >= 8 || I > Lo)
			{
				for (; I < Hi; I += 8)
				{
					if (I + 8 > Hi) I = Hi - 8;
					float A0 = 0.0f, A1 = 0.0f, A2 = 0.0f, A3 = 0.0f, A4 = 0.0f, A5 = 0.0f, A6 = 0.0f, A7 = 0.0f;
					const float* __restrict C = Xc + I;
					const float* __restrict Sn = Xs + I;
					for (int U = 0; U < NJ; ++U, C += Stride, Sn += Stride)
					{
						const float Ca = A[U], Sb = B[U];
						A0 += C[0] * Ca - Sn[0] * Sb;
						A1 += C[1] * Ca - Sn[1] * Sb;
						A2 += C[2] * Ca - Sn[2] * Sb;
						A3 += C[3] * Ca - Sn[3] * Sb;
						A4 += C[4] * Ca - Sn[4] * Sb;
						A5 += C[5] * Ca - Sn[5] * Sb;
						A6 += C[6] * Ca - Sn[6] * Sb;
						A7 += C[7] * Ca - Sn[7] * Sb;
					}
					float* __restrict R = Row + I;
					R[0] = A0;
					R[1] = A1;
					R[2] = A2;
					R[3] = A3;
					R[4] = A4;
					R[5] = A5;
					R[6] = A6;
					R[7] = A7;
				}
				return;
			}
			for (; I < Hi; ++I)
			{
				float Acc = 0.0f;
				for (int U = 0; U < NJ; ++U) Acc += Xc[U * Stride + I] * A[U] - Xs[U * Stride + I] * B[U];
				Row[I] = Acc;
			}
		}

		/** SumRun's, each node's weights its own cell's (Own[I]): sum over the trains of Sat((Ev - Own) Iv) (Xc A - Xs B), the
		 * Sat as (|x| - |x - 1| + 1) / 2, so the compilers keep it in vector registers too (A and B come halved). */
		void SumOwn(const float* Xc, const float* Xs, size_t Stride, const float* A, const float* B, const float* EvA, const float* IvA, const float* Own, int NJ,
			int Lo, int Hi, float* Row)
		{
			int I = Lo;
			if (Hi - Lo >= 8)
			{
				for (; I < Hi; I += 8)
				{
					if (I + 8 > Hi) I = Hi - 8;
					float A0 = 0.0f, A1 = 0.0f, A2 = 0.0f, A3 = 0.0f, A4 = 0.0f, A5 = 0.0f, A6 = 0.0f, A7 = 0.0f;
					const float* __restrict C = Xc + I;
					const float* __restrict Sn = Xs + I;
					const float* __restrict O = Own + I;
					for (int U = 0; U < NJ; ++U, C += Stride, Sn += Stride)
					{
						const float Ca = A[U], Sb = B[U], Ev = EvA[U], Iw = IvA[U];
						A0 += (std::fabs((Ev - O[0]) * Iw) - std::fabs((Ev - O[0]) * Iw - 1.0f) + 1.0f) * (C[0] * Ca - Sn[0] * Sb);
						A1 += (std::fabs((Ev - O[1]) * Iw) - std::fabs((Ev - O[1]) * Iw - 1.0f) + 1.0f) * (C[1] * Ca - Sn[1] * Sb);
						A2 += (std::fabs((Ev - O[2]) * Iw) - std::fabs((Ev - O[2]) * Iw - 1.0f) + 1.0f) * (C[2] * Ca - Sn[2] * Sb);
						A3 += (std::fabs((Ev - O[3]) * Iw) - std::fabs((Ev - O[3]) * Iw - 1.0f) + 1.0f) * (C[3] * Ca - Sn[3] * Sb);
						A4 += (std::fabs((Ev - O[4]) * Iw) - std::fabs((Ev - O[4]) * Iw - 1.0f) + 1.0f) * (C[4] * Ca - Sn[4] * Sb);
						A5 += (std::fabs((Ev - O[5]) * Iw) - std::fabs((Ev - O[5]) * Iw - 1.0f) + 1.0f) * (C[5] * Ca - Sn[5] * Sb);
						A6 += (std::fabs((Ev - O[6]) * Iw) - std::fabs((Ev - O[6]) * Iw - 1.0f) + 1.0f) * (C[6] * Ca - Sn[6] * Sb);
						A7 += (std::fabs((Ev - O[7]) * Iw) - std::fabs((Ev - O[7]) * Iw - 1.0f) + 1.0f) * (C[7] * Ca - Sn[7] * Sb);
					}
					float* __restrict R = Row + I;
					R[0] = A0;
					R[1] = A1;
					R[2] = A2;
					R[3] = A3;
					R[4] = A4;
					R[5] = A5;
					R[6] = A6;
					R[7] = A7;
				}
				return;
			}
			for (; I < Hi; ++I)
			{
				float Acc = 0.0f;
				for (int U = 0; U < NJ; ++U)
				{
					const float X = (EvA[U] - Own[I]) * IvA[U];
					Acc += (std::fabs(X) - std::fabs(X - 1.0f) + 1.0f) * (Xc[U * Stride + I] * A[U] - Xs[U * Stride + I] * B[U]);
				}
				Row[I] = Acc;
			}
		}

		/** One level laid out: each node's cell is its column's (ColCell) or its row's (RowCell), the larger; the nodes
		 * [HoleLo, HoleHi) of a row are not laid (the finer level's hole: nothing samples them). The columns' cells fall to
		 * Imin's and rise past it. For the subject's block (bBlock) a node's cell is Fc + (max(MainX, MainY) - Fc) max(EaseX,
		 * EaseY), the main mesh's from the eye and the ease from her each the larger of its column's and its row's: its
		 * column's (ColCell = Fc + (MainX - Fc) EaseX) where both are the column's, its row's where both are the row's, and its
		 * own between; MainX falls to IminM and rises past it, EaseX to IminE. */
		struct NearPlan
		{
			int Nn = 0, Imin = 0, IminM = 0, IminE = 0;
			bool bBlock = false;
			float Fc = 0.0f;
			std::vector<float> ColCell, RowCell, MainX, MainY, EaseX, EaseY;
			std::vector<int> HoleLo, HoleHi;
			void Reset(int N, bool bIsBlock)
			{
				Nn = N;
				Imin = IminM = IminE = 0;
				bBlock = bIsBlock;
				ColCell.assign(static_cast<size_t>(N), 0.0f);
				RowCell.assign(static_cast<size_t>(N), 0.0f);
				HoleLo.assign(static_cast<size_t>(N), 0);
				HoleHi.assign(static_cast<size_t>(N), 0);
				if (bBlock)
				{
					MainX.assign(static_cast<size_t>(N), 0.0f);
					MainY.assign(static_cast<size_t>(N), 0.0f);
					EaseX.assign(static_cast<size_t>(N), 0.0f);
					EaseY.assign(static_cast<size_t>(N), 0.0f);
				}
			}
		};

		/** In a V (falling to At, rising past it), the run of I whose V[I] is at most (bStrict: under) Most: [Lo, Hi). */
		void RunUnder(const float* V, int N, int At, float Most, bool bStrict, int& Lo, int& Hi)
		{
			auto Over = [Most, bStrict](float X) { return bStrict ? X >= Most : X > Most; };
			if (Over(V[At]))
			{
				Lo = Hi = At;
				return;
			}
			Lo = static_cast<int>(std::partition_point(V, V + At + 1, Over) - V);
			Hi = static_cast<int>(std::partition_point(V + At, V + N, [&Over](float X) { return !Over(X); }) - V);
		}

		/** One level's heights by the separable sum: a train's two tables along x (cos and sin of kx x, and again times its
		 * weight at each column's cell), and its two numbers along y a row (by rotations: two sines a train a level). Each run
		 * of nodes whose cells are all their column's takes the column-weighted tables, each run whose cells are their row's
		 * the row's weights, and the few whose cells are their own (in the subject's block, where the cell from the eye and
		 * the ease from her each come from a different side) are weighted node by node. Trains longest first, so those with
		 * weight at a cell are the first few (L > 8 cell), and a run takes only them. */
		void Fill(const Sea& S, double T, NearLevel& Lv, const NearPlan& Pl, double LeastCell)
		{
			const int Nn = Lv.Nodes;
			int Use[MostDrawn] = {}, NJ = 0;
			for (int J = 0; J < S.ND; ++J)
			{
				if (WeightOf(S.D[J].L, LeastCell) <= 0.0) continue;
				int At = NJ++;
				while (At > 0 && S.D[Use[At - 1]].L < S.D[J].L)
				{
					Use[At] = Use[At - 1];
					--At;
				}
				Use[At] = J;
			}
			if (NJ == 0)
			{
				Lv.H.assign(static_cast<size_t>(Nn) * Nn, 0.0f);
				return;
			}
			// (every node but the hole's is laid below, and nothing reads the hole: no need to clear it)
			Lv.H.resize(static_cast<size_t>(Nn) * Nn);
			thread_local std::vector<float> Tables;
			const size_t Stride = static_cast<size_t>(Nn);
			Tables.resize(4 * static_cast<size_t>(MostDrawn) * Stride);
			float* Cx = Tables.data();
			float* Sx = Cx + MostDrawn * Stride;
			float* Cw = Sx + MostDrawn * Stride;
			float* Sw = Cw + MostDrawn * Stride;
			double Cy[MostDrawn] = {}, Sy[MostDrawn] = {}, CsY[MostDrawn] = {}, SsY[MostDrawn] = {};
			// (a train's weight at a cell, Sat((E - cell) Iv) in float, and the cell from which it has none, 8 cell >= L)
			float Ev[MostDrawn] = {}, Iv[MostDrawn] = {}, Lv8[MostDrawn] = {};
			for (int U = 0; U < NJ; ++U)
			{
				const Train& Tr = S.D[Use[U]];
				const double Lg = std::max(Tr.L, 1e-3);
				Ev[U] = static_cast<float>(Lg * 0.125);
				Iv[U] = static_cast<float>(1.0 / std::max(Lg * (0.125 - 1.0 / 12.0), 1e-9));
				Lv8[U] = static_cast<float>(Lg * 0.125);
				const double A0 = TwoPi * Turns(Tr.Kx * Lv.X0 / TwoPi), Step = Tr.Kx * Lv.Cell;
				const double Cs = std::cos(Step), Ss = std::sin(Step);
				double C = std::cos(A0), Sn = std::sin(A0);
				float* Xc = Cx + U * Stride;
				float* Xs = Sx + U * Stride;
				float* Wc = Cw + U * Stride;
				float* Ws = Sw + U * Stride;
				for (int I = 0; I < Nn; ++I)
				{
					const float W = std::min(std::max((Ev[U] - Pl.ColCell[static_cast<size_t>(I)]) * Iv[U], 0.0f), 1.0f);
					Xc[I] = static_cast<float>(C);
					Xs[I] = static_cast<float>(Sn);
					Wc[I] = W * Xc[I];
					Ws[I] = W * Xs[I];
					const double Cn = C * Cs - Sn * Ss;
					Sn = Sn * Cs + C * Ss;
					C = Cn;
				}
				const double B0 = TwoPi * Turns((Tr.Ky * Lv.Y0 - Tr.W * T + Tr.Phase0) / TwoPi), StepY = Tr.Ky * Lv.Cell;
				CsY[U] = std::cos(StepY);
				SsY[U] = std::sin(StepY);
				Cy[U] = std::cos(B0) * Tr.A;
				Sy[U] = std::sin(B0) * Tr.A;
			}
			auto Count = [&](float Cell)
			{
				if (Lv8[NJ - 1] > Cell) return NJ;
				int N = 0;
				while (N < NJ && Lv8[N] > Cell) ++N;
				return N;
			};
			for (int Jy = 0; Jy < Nn; ++Jy)
			{
				float Ay[MostDrawn] = {}, By[MostDrawn] = {}, Wa[MostDrawn] = {}, Wb[MostDrawn] = {}, Ah[MostDrawn] = {}, Bh[MostDrawn] = {};
				const float Rc = Pl.RowCell[static_cast<size_t>(Jy)];
				bool bRowWhole = true;
				for (int U = 0; U < NJ; ++U)
				{
					Ay[U] = static_cast<float>(Cy[U]);
					By[U] = static_cast<float>(Sy[U]);
					const float W = std::min(std::max((Ev[U] - Rc) * Iv[U], 0.0f), 1.0f);
					bRowWhole = bRowWhole && W == 1.0f;
					Wa[U] = W * Ay[U];
					Wb[U] = W * By[U];
					Ah[U] = 0.5f * Ay[U];
					Bh[U] = 0.5f * By[U];
					const double Cn = Cy[U] * CsY[U] - Sy[U] * SsY[U];
					Sy[U] = Sy[U] * CsY[U] + Cy[U] * SsY[U];
					Cy[U] = Cn;
				}
				float* Row = Lv.H.data() + static_cast<size_t>(Jy) * Nn;
				const int RowN = Count(Rc);
				const int Hl = Pl.HoleLo[static_cast<size_t>(Jy)], Hh = Pl.HoleHi[static_cast<size_t>(Jy)];
				// (a run of one kind, [Lo, Hi), laid but for the hole)
				auto Lay = [&](int Kind, int Lo, int Hi)
				{
					auto Part = [&](int A, int B)
					{
						if (A >= B) return;
						if (Kind == 1)
						{
							SumRun(Cx, Sx, Stride, Wa, Wb, RowN, A, B, Row);
						}
						else if (Kind == 0)
						{
							// (a ring's columns' cells fall toward Imin and rise past it: the run's least is at an end, or Imin)
							float Least = std::min(Pl.ColCell[static_cast<size_t>(A)], Pl.ColCell[static_cast<size_t>(B - 1)]);
							if (A <= Pl.Imin && Pl.Imin < B) Least = std::min(Least, Pl.ColCell[static_cast<size_t>(Pl.Imin)]);
							if (Pl.bBlock)
							{
								for (int Q = A; Q < B; ++Q) Least = std::min(Least, Pl.ColCell[static_cast<size_t>(Q)]);
							}
							SumRun(Cw, Sw, Stride, Ay, By, Count(Least), A, B, Row);
						}
						else
						{
							// (each node's own cell, in place of the run's; where eight or more nodes running have one cell, mostly
							// (the main mesh's cell is its ring's alone but in its morph band, the ease 0 or 1 but in its band), they
							// are a run of one weight a train: the row's numbers weighted at that cell)
							thread_local std::vector<float> Own;
							Own.resize(static_cast<size_t>(Nn));
							const float My = Pl.MainY[static_cast<size_t>(Jy)], Ey = Pl.EaseY[static_cast<size_t>(Jy)];
							for (int Q = A; Q < B; ++Q)
							{
								Own[static_cast<size_t>(Q)] = Pl.Fc + (std::max(Pl.MainX[static_cast<size_t>(Q)], My) - Pl.Fc) * std::max(Pl.EaseX[static_cast<size_t>(Q)], Ey);
							}
							int From = A;
							auto Mixed = [&](int M0, int M1)
							{
								if (M0 >= M1) return;
								float Least = 1e30f;
								for (int Q = M0; Q < M1; ++Q) Least = std::min(Least, Own[static_cast<size_t>(Q)]);
								SumOwn(Cx, Sx, Stride, Ah, Bh, Ev, Iv, Own.data(), Count(Least), M0, M1, Row);
							};
							for (int Q = A; Q < B;)
							{
								const float Cell = Own[static_cast<size_t>(Q)];
								int End = Q + 1;
								while (End < B && Own[static_cast<size_t>(End)] == Cell) ++End;
								if (End - Q >= 8)
								{
									Mixed(From, Q);
									float Wc[MostDrawn] = {}, Ws[MostDrawn] = {};
									const int Nw = Count(Cell);
									for (int U = 0; U < Nw; ++U)
									{
										const float W = std::min(std::max((Ev[U] - Cell) * Iv[U], 0.0f), 1.0f);
										Wc[U] = W * Ay[U];
										Ws[U] = W * By[U];
									}
									SumRun(Cx, Sx, Stride, Wc, Ws, Nw, Q, End, Row);
									From = End;
								}
								Q = End;
							}
							Mixed(From, B);
						}
					};
					if (Hh <= Hl)
					{
						Part(Lo, Hi);
						return;
					}
					Part(Lo, std::min(Hi, Hl));
					Part(std::max(Lo, Hh), Hi);
				};
				if (!Pl.bBlock)
				{
					// (a ring's level: where every train has its whole weight at the row's cell, a node's weights are its column's
					// (a weight never rises with the cell, so a node's, at the larger of the two cells, is the less of the two), and
					// the row is one run)
					if (bRowWhole)
					{
						Lay(0, 0, Nn);
						continue;
					}
					// (else a node's cell is its row's where its column's is no more, a run round Imin; its column's either side)
					int A = 0, B = 0;
					RunUnder(Pl.ColCell.data(), Nn, Pl.Imin, Rc, false, A, B);
					Lay(0, 0, A);
					Lay(1, A, B);
					Lay(0, B, Nn);
					continue;
				}
				// (the subject's block: its row's cell where the ease is the row's (EaseX <= Ey, the run Be) and the main mesh's
				// is the row's too (MainX <= My, the run Am) or the ease nought; its column's where both are the column's
				// (EaseX > Ey and MainX >= My: outside Be and outside the run Am2 of MainX < My); its own between. The runs' ends
				// part the row into pieces, each of one kind.)
				const float My = Pl.MainY[static_cast<size_t>(Jy)], Ey = Pl.EaseY[static_cast<size_t>(Jy)];
				int Am = 0, Am1 = 0, Am2 = 0, Am3 = 0, Be = 0, Be1 = 0;
				RunUnder(Pl.MainX.data(), Nn, Pl.IminM, My, false, Am, Am1);
				RunUnder(Pl.MainX.data(), Nn, Pl.IminM, My, true, Am2, Am3);
				RunUnder(Pl.EaseX.data(), Nn, Pl.IminE, Ey, false, Be, Be1);
				int Cuts[8] = { 0, Am, Am1, Am2, Am3, Be, Be1, Nn };
				std::sort(Cuts, Cuts + 8);
				int From = 0, Kind = -1;
				for (int C = 1; C < 8; ++C)
				{
					const int Lo = Cuts[C - 1], Hi = Cuts[C];
					if (Lo >= Hi) continue;
					const bool bBe = Lo >= Be && Lo < Be1, bAm = Lo >= Am && Lo < Am1, bAmStrict = Lo >= Am2 && Lo < Am3;
					const int Here = bBe ? ((bAm || Ey == 0.0f) ? 1 : 2) : (bAmStrict ? 2 : 0);
					if (Here != Kind)
					{
						if (Kind >= 0) Lay(Kind, From, Lo);
						From = Lo;
						Kind = Here;
					}
				}
				if (Kind >= 0) Lay(Kind, From, Nn);
			}
		}
	}

	namespace
	{
		/** The eye's levels lay their shifts at every other node (the even ones, which never ease), as the subject's block does:
		 * at every fourth, a shift's shortest trains (8 cells long) are read from two or three nodes a length, and the height
		 * it finds is off by up to 5.5 cm at 13.8 m/s (2.9 cm at every other). */
		constexpr int RingShiftEvery = 2;

		/** A sea whose heights are another's shift along X (Axis 0) or Y (1): each displaced train Q a k^ long and a quarter
		 * turn on (-Q a kx / k sin th is Q a kx / k cos(th + pi / 2)), so Fill lays a level's shifts as it lays its heights,
		 * each train weighted at the node's cell as the mesh weights it. */
		Sea ShiftSea(const Sea& S, int Axis)
		{
			Sea O = S;
			for (int J = 0; J < O.ND; ++J)
			{
				const Train& T = S.D[J];
				O.D[J].A = T.Q * T.A * (Axis == 0 ? T.Kx : T.Ky) / std::max(T.K, 1e-12);
				O.D[J].Phase0 = T.Phase0 + 0.5 * Pi;
			}
			return O;
		}

		/** A level's shifts at every Every-th node (Ps.Nn a side from its first node), laid by Fill from the two shift seas with
		 * Ps (those nodes' cells), into Lv.S, X and Y in turn. */
		void FillShifts(const Sea& Sx, const Sea& Sy, double T, NearLevel& Lv, const NearPlan& Ps, double LeastCell, int Every)
		{
			thread_local NearLevel Ax, Ay;
			Ax.X0 = Ay.X0 = Lv.X0;
			Ax.Y0 = Ay.Y0 = Lv.Y0;
			Ax.Cell = Ay.Cell = Every * Lv.Cell;
			Ax.Nodes = Ay.Nodes = Ps.Nn;
			Fill(Sx, T, Ax, Ps, LeastCell);
			Fill(Sy, T, Ay, Ps, LeastCell);
			const size_t Count = static_cast<size_t>(Ps.Nn) * Ps.Nn;
			Lv.SNodes = Ps.Nn;
			Lv.SInv = Lv.Inv / Every;
			Lv.S.resize(2 * Count);
			float* Into = Lv.S.data();
			const float* X = Ax.H.data();
			const float* Y = Ay.H.data();
			for (size_t I = 0; I < Count; ++I)
			{
				Into[2 * I] = X[I];
				Into[2 * I + 1] = Y[I];
			}
		}

		/** The eye's level for a point (NearLevelAt's pick, without the subject's block). */
		const NearLevel& EyeLevel(const Near& N, double X, double Y)
		{
			const double Q = std::max(std::fabs(X - N.Ex), std::fabs(Y - N.Ey)) * N.Pick;
			uint64_t Bits = 0;
			std::memcpy(&Bits, &Q, sizeof(Bits));
			int K = static_cast<int>(Bits >> 52) - 1023 + static_cast<int>((Bits & 0xFFFFFFFFFFFFFull) != 0);
			K = K < 0 ? 0 : (K > MostRings - 1 ? MostRings - 1 : K);
			return N.L[K];
		}
	}

	void BuildNear(const Sea& S, const Drawn& D, Near& Out, int MostLevels, bool bShifts)
	{
		const Lay L(&S, D, false);
		Out.Ex = L.Ex;
		Out.Ey = L.Ey;
		Out.NL = 0;
		const int H = L.Half, Nn = 2 * H + 1;
		Out.Pick = 1.0 / ((H - 2) * D.C0);
		const NearCells Cs(L);
		thread_local NearPlan Pl, Ps;
		const Sea ShiftX = bShifts ? ShiftSea(S, 0) : Sea(), ShiftY = bShifts ? ShiftSea(S, 1) : Sea();
		for (int K = 0; K < std::min(L.Rings(), std::clamp(MostLevels, 0, MostRings)); ++K)
		{
			NearLevel& Lv = Out.L[K];
			const double Cc = L.C(K);
			bool bAny = false;
			for (int J = 0; J < S.ND; ++J) bAny = bAny || WeightOf(S.D[J].L, Cc) > 0.0;
			if (!bAny) break;
			Lv.Cell = Cc;
			Lv.Inv = 1.0 / Cc;
			Lv.Nodes = Nn;
			Lv.X0 = D.RingX[K] - H * Cc;
			Lv.Y0 = D.RingY[K] - H * Cc;
			Lv.Mx = D.RingX[K];
			Lv.My = D.RingY[K];
			Lv.Reach = H * Cc;
			Pl.Reset(Nn, false);
			// (a finer level holds the inside of the ring before, all but its outer two cells and a half: SampleNear reads this
			// level from H / 2 - 1 cells out (23 at RingCells 96), its nodes from H / 2 - 2, and the hole's last node is H / 2 -
			// 3 from the ring's middle, under a cell from the eye)
			if (K >= 1)
			{
				const double Inner = H * L.C(K - 1) - 2.5 * Cc;
				const double Hx0 = (D.RingX[K - 1] - Inner - Lv.X0) / Cc, Hx1 = (D.RingX[K - 1] + Inner - Lv.X0) / Cc;
				const double Hy0 = (D.RingY[K - 1] - Inner - Lv.Y0) / Cc, Hy1 = (D.RingY[K - 1] + Inner - Lv.Y0) / Cc;
				for (int Jy = 0; Jy < Nn; ++Jy)
				{
					if (!(Jy > Hy0 && Jy < Hy1)) continue;
					const int Lo = std::clamp(static_cast<int>(std::floor(Hx0)) + 1, 0, Nn), Hi = std::clamp(static_cast<int>(std::ceil(Hx1)), Lo, Nn);
					Pl.HoleLo[static_cast<size_t>(Jy)] = Lo;
					Pl.HoleHi[static_cast<size_t>(Jy)] = Hi;
				}
			}
			// (the cell is Ring's of the larger of a node's two distances from the eye, and Ring's never falls with the distance:
			// so it is the larger of its column's and its row's)
			for (int I = 0; I < Nn; ++I)
			{
				Pl.ColCell[static_cast<size_t>(I)] = static_cast<float>(Cs.Ring(K, std::fabs(Lv.X0 + I * Cc - L.Ex)));
				Pl.RowCell[static_cast<size_t>(I)] = static_cast<float>(Cs.Ring(K, std::fabs(Lv.Y0 + I * Cc - L.Ey)));
			}
			Pl.Imin = std::clamp(static_cast<int>(std::lround((L.Ex - Lv.X0) / Cc)), 0, Nn - 1);
			Fill(S, D.SeaT, Lv, Pl, Cc);
			// (its shifts, at every RingShiftEvery-th node (even: they never ease), their cells the full plan's; the hole shrunk
			// by RingShiftEvery and three cells, as a shift is read from the nodes round a point, up to RingShiftEvery cells
			// nearer the eye than it)
			if (bShifts)
			{
				const int Sn = Nn / RingShiftEvery + 1;
				Ps.Reset(Sn, false);
				for (int I = 0; I < Sn; ++I)
				{
					Ps.ColCell[static_cast<size_t>(I)] = Pl.ColCell[static_cast<size_t>(std::min(RingShiftEvery * I, Nn - 1))];
					Ps.RowCell[static_cast<size_t>(I)] = Pl.RowCell[static_cast<size_t>(std::min(RingShiftEvery * I, Nn - 1))];
				}
				Ps.Imin = std::clamp(static_cast<int>(std::lround((L.Ex - Lv.X0) / (RingShiftEvery * Cc))), 0, Sn - 1);
				if (K >= 1)
				{
					const double Inner = H * L.C(K - 1) - (RingShiftEvery + 3.0) * Cc, Sc = RingShiftEvery * Cc;
					const double Hx0 = (D.RingX[K - 1] - Inner - Lv.X0) / Sc, Hx1 = (D.RingX[K - 1] + Inner - Lv.X0) / Sc;
					const double Hy0 = (D.RingY[K - 1] - Inner - Lv.Y0) / Sc, Hy1 = (D.RingY[K - 1] + Inner - Lv.Y0) / Sc;
					for (int Jy = 0; Jy < Sn; ++Jy)
					{
						if (!(Jy > Hy0 && Jy < Hy1)) continue;
						const int Lo = std::clamp(static_cast<int>(std::floor(Hx0)) + 1, 0, Sn), Hi = std::clamp(static_cast<int>(std::ceil(Hx1)), Lo, Sn);
						Ps.HoleLo[static_cast<size_t>(Jy)] = Lo;
						Ps.HoleHi[static_cast<size_t>(Jy)] = Hi;
					}
				}
				FillShifts(ShiftX, ShiftY, D.SeaT, Lv, Ps, Cc, RingShiftEvery);
			}
			else
			{
				Lv.SNodes = 2;
				Lv.SInv = 0.0;
				if (Lv.S.size() < 8) Lv.S.resize(8);
				std::fill(Lv.S.begin(), Lv.S.begin() + 8, 0.0f);
			}
			// (the morph: over the band an odd point's height eases to the middle of its two even neighbours' along its odd axes,
			// in place: an odd node's two neighbours along its odd axes are even nodes, which never ease)
			const double Geo = (H - 3.0) * Cc - H * Cc * 0.125, GeoInv = 1.0 / std::max(H * Cc * 0.125, 1e-9);
			float* Hs = Lv.H.data();
			// (the nodes past Geo, row by row: a whole row where the row is, else its two ends; odd rows take every node,
			// even rows the odd ones)
			const float Mx0 = static_cast<float>((Lv.X0 - L.Ex) * GeoInv), Mstep = static_cast<float>(Cc * GeoInv), Mgeo = static_cast<float>(Geo * GeoInv);
			thread_local std::vector<float> Mc;
			Mc.resize(static_cast<size_t>(Nn));
			for (int I = 0; I < Nn; ++I) Mc[static_cast<size_t>(I)] = std::fabs(Mx0 + static_cast<float>(I) * Mstep) - Mgeo;
			const float* McI = Mc.data();
			for (int Jy = 0; Jy < Nn; ++Jy)
			{
				const int Oy = (Jy - H) & 1;
				if (Oy && (Jy == 0 || Jy == Nn - 1)) continue;
				const double Dy = std::fabs(Lv.Y0 + Jy * Cc - L.Ey);
				const size_t Row = static_cast<size_t>(Jy) * Nn, Step = Oy ? static_cast<size_t>(Nn) : 0;
				const float My = static_cast<float>(Dy * GeoInv) - Mgeo;
				auto Run = [&](int From, int To)
				{
					// (From to To - 1: on an odd row the nodes odd along x go to the middle of their diagonal neighbours and the
					// even ones of those above and below; on an even row the odd ones only, along x. A node odd along x needs
					// both neighbours along it, and the two end nodes have the parity of H)
					if (!Oy || (H & 1))
					{
						From = std::max(From, 1);
						To = std::min(To, Nn - 1);
					}
					for (int Ox = 0; Ox < 2; ++Ox)
					{
						if (!Oy && !Ox) continue;
						const std::ptrdiff_t S2 = static_cast<std::ptrdiff_t>(Step) + Ox;
						float* Hr = Hs + Row;
						for (int I = From + (((From - H) & 1) != Ox ? 1 : 0); I < To; I += 2)
						{
							const float M = std::min(std::max(McI[I], My), 1.0f);
							if (M > 0.0f) Hr[I] += M * (0.5f * (Hr[I - S2] + Hr[I + S2]) - Hr[I]);
						}
					}
				};
				if (Dy > Geo) Run(0, Nn);
				else
				{
					// (|X - Ex| > Geo: I below Left, or above Right)
					Run(0, std::clamp(static_cast<int>(std::ceil((L.Ex - Geo - Lv.X0) / Cc)), 0, Nn));
					Run(std::clamp(static_cast<int>(std::floor((L.Ex + Geo - Lv.X0) / Cc)) + 1, 0, Nn), Nn);
				}
			}
			Out.NL = K + 1;
		}
		// (the rest: empty, reading 0 and answering for nothing)
		auto Empty = [](NearLevel& Lv)
		{
			Lv.X0 = Lv.Y0 = Lv.Mx = Lv.My = 0.0;
			Lv.Cell = 1.0;
			Lv.Inv = 0.0;
			Lv.Reach = -1.0;
			Lv.Nodes = 2;
			Lv.SNodes = 2;
			Lv.SInv = 0.0;
			// (its first four heights and shifts, nought; vectors kept at their size, so the level is laid again without clearing
			// them)
			if (Lv.H.size() < 4) Lv.H.resize(4);
			std::fill(Lv.H.begin(), Lv.H.begin() + 4, 0.0f);
			if (Lv.S.size() < 8) Lv.S.resize(8);
			std::fill(Lv.S.begin(), Lv.S.begin() + 8, 0.0f);
		};
		for (int K = Out.NL; K < MostRings; ++K) Empty(Out.L[K]);
		// The subject's block: the patch's own grid over the square where it has not yet eased whole onto the main mesh
		// (EaseEnd from her unsnapped place, and a patch cell more, where a cell has a corner short of it; past that, its edge
		// and its notches included, the patch is the main mesh's surface, which the eye's levels hold), and the nodes a patch
		// cell round it, which a sample inside reads.
		NearLevel& Lv = Out.L[NearBlock];
		Empty(Lv);
		// (at presence nought the patch is the main mesh's own surface everywhere, which the eye's levels hold)
		if (L.PatchOn() && D.Patch.Presence > 0.0)
		{
			const PatchAt& Pt = D.Patch;
			const double Pc = Pt.Cell, Px0 = Pt.X - Pt.Half, Py0 = Pt.Y - Pt.Half;
			// (the nodes of every cell a point within EaseEnd and a cell of her reads, a square of the patch's grid: inside it,
			// but where its edge rows are already the main mesh's own)
			const int Most = PatchCells;
			const int I0 = std::clamp(static_cast<int>(std::floor((Pt.SubjectX - Pt.EaseEnd - Px0) / Pc)) - 1, 0, Most);
			const int J0 = std::clamp(static_cast<int>(std::floor((Pt.SubjectY - Pt.EaseEnd - Py0) / Pc)) - 1, 0, Most);
			// (and with its shifts two more nodes up each way: a shift is read from its even nodes, up to two cells past the point)
			const int More = bShifts ? 4 : 2;
			const int I1 = std::clamp(static_cast<int>(std::floor((Pt.SubjectX + Pt.EaseEnd - Px0) / Pc)) + More, 0, Most);
			const int J1 = std::clamp(static_cast<int>(std::floor((Pt.SubjectY + Pt.EaseEnd - Py0) / Pc)) + More, 0, Most);
			const int Pn = std::max(I1 - I0, J1 - J0) + 1;
			const int Ib = std::clamp(I0, 0, Most + 1 - Pn), Jb = std::clamp(J0, 0, Most + 1 - Pn);
			Lv.Cell = Pc;
			Lv.Inv = 1.0 / Pc;
			Lv.Nodes = Pn;
			Lv.X0 = Px0 + Ib * Pc;
			Lv.Y0 = Py0 + Jb * Pc;
			Lv.Mx = Pt.SubjectX;
			Lv.My = Pt.SubjectY;
			Lv.Reach = Pt.EaseEnd + Pc;
			Pl.Reset(Pn, true);
			// (its cell is its own out to Full from her, then eases to the main mesh's by EaseEnd: Lerp(Pc, max(Pc, main),
			// ease); the main mesh's cell is ByDist's of the larger distance from the eye, and the ease Sat's of the larger
			// distance from her, each never falling with its distance: so each is the larger of a column's and a row's, and
			// the node's cell is its column's where both are the column's, its row's where both are the row's)
			// (and the ease as far again as the patch is not shown: 1 - Presence (1 - ease), which is the larger of a column's and
			// a row's as the ease is)
			const double Ease = 1.0 / std::max(Pt.EaseEnd - Pt.Full, 1e-9), Shown = Sat(Pt.Presence);
			const float Fc = static_cast<float>(Pc);
			// (the block's plan at nodes a Step of cells apart from its first, Count a side: its heights' (1) and its shifts' (2))
			auto Plan = [&](NearPlan& P, int Count, int Step)
			{
				P.Reset(Count, true);
				P.Fc = Fc;
				for (int I = 0; I < Count; ++I)
				{
					const size_t Q = static_cast<size_t>(I);
					const double X = Lv.X0 + I * Step * Pc, Y = Lv.Y0 + I * Step * Pc;
					P.MainX[Q] = static_cast<float>(std::max(Pc, Cs.ByDist(std::fabs(X - L.Ex))));
					P.EaseX[Q] = static_cast<float>(1.0 - Shown * (1.0 - Sat((std::fabs(X - Pt.SubjectX) - Pt.Full) * Ease)));
					P.MainY[Q] = static_cast<float>(std::max(Pc, Cs.ByDist(std::fabs(Y - L.Ey))));
					P.EaseY[Q] = static_cast<float>(1.0 - Shown * (1.0 - Sat((std::fabs(Y - Pt.SubjectY) - Pt.Full) * Ease)));
					P.ColCell[Q] = Fc + (P.MainX[Q] - Fc) * P.EaseX[Q];
					P.RowCell[Q] = Fc + (P.MainY[Q] - Fc) * P.EaseY[Q];
				}
				P.IminM = std::clamp(static_cast<int>(std::lround((L.Ex - Lv.X0) / (Step * Pc))), 0, Count - 1);
				P.IminE = std::clamp(static_cast<int>(std::lround((Pt.SubjectX - Lv.X0) / (Step * Pc))), 0, Count - 1);
			};
			Plan(Pl, Pn, 1);
			Fill(S, D.SeaT, Lv, Pl, Pc);
			// (its shifts at its even nodes, the same sum, each eased onto the eye's level's shift as far as the node has eased)
			const int Sn = bShifts ? Pn / 2 + 1 : 0;
			if (bShifts)
			{
				Plan(Ps, Sn, 2);
				FillShifts(ShiftX, ShiftY, D.SeaT, Lv, Ps, Pc, 2);
			}
			for (int J = 0; J < Sn; ++J)
			{
				const float Ey = Ps.EaseY[static_cast<size_t>(J)];
				for (int I = 0; I < Sn; ++I)
				{
					const float E = std::max(Ps.EaseX[static_cast<size_t>(I)], Ey);
					if (E <= 0.0f) continue;
					const double X = Lv.X0 + 2.0 * I * Pc, Y = Lv.Y0 + 2.0 * J * Pc;
					double Mx = 0.0, My = 0.0;
					ShiftLevel(EyeLevel(Out, X, Y), X, Y, Mx, My);
					float* Sp = Lv.S.data() + 2 * (static_cast<size_t>(J) * Sn + I);
					Sp[0] += E * (static_cast<float>(Mx) - Sp[0]);
					Sp[1] += E * (static_cast<float>(My) - Sp[1]);
				}
			}
			// (over the ease each node goes to the main mesh's surface, as far as it has eased (the larger of its column's
			// and its row's): the eye's level for its distance, which is the main mesh itself, read as SampleLevel reads it.
			// A row eases whole where its own ease is over nought, else at its two ends, past the run where the column's is
			// nought; along it the level changes seldom, and its two rows are found once for each)
			const double Pick = Out.Pick;
			for (int J = 0; J < Pn; ++J)
			{
				const float Ey = Pl.EaseY[static_cast<size_t>(J)];
				const double Y = Lv.Y0 + J * Pc, Dy = std::fabs(Y - L.Ey);
				float* Row = Lv.H.data() + static_cast<size_t>(J) * Pn;
				int A = Pn, B = Pn;
				if (Ey <= 0.0f) RunUnder(Pl.EaseX.data(), Pn, Pl.IminE, 0.0f, false, A, B);
				int Kc = -1, Last = 0;
				const float *H0 = nullptr, *H1 = nullptr;
				double V = 0.0, Lx0 = 0.0, Inv = 0.0;
				auto Blend = [&](int From, int To)
				{
					for (int I = From; I < To; ++I)
					{
						const double X = Lv.X0 + I * Pc;
						const double Qd = std::max(std::fabs(X - L.Ex), Dy) * Pick;
						uint64_t Bits = 0;
						std::memcpy(&Bits, &Qd, sizeof(Bits));
						int K = static_cast<int>(Bits >> 52) - 1023 + static_cast<int>((Bits & 0xFFFFFFFFFFFFFull) != 0);
						K = K < 0 ? 0 : (K > MostRings - 1 ? MostRings - 1 : K);
						if (K != Kc)
						{
							const NearLevel& Lk = Out.L[K];
							Kc = K;
							Last = Lk.Nodes - 2;
							const double Fy = (Y - Lk.Y0) * Lk.Inv;
							int Jl = static_cast<int>(Fy);
							Jl = Jl < 0 ? 0 : (Jl > Last ? Last : Jl);
							V = Fy - Jl;
							H0 = Lk.H.data() + static_cast<size_t>(Jl) * Lk.Nodes;
							H1 = H0 + Lk.Nodes;
							Lx0 = Lk.X0;
							Inv = Lk.Inv;
						}
						const double Fx = (X - Lx0) * Inv;
						int Il = static_cast<int>(Fx);
						Il = Il < 0 ? 0 : (Il > Last ? Last : Il);
						const double U = Fx - Il;
						const double Z00 = H0[Il], Z10 = H0[Il + 1], Z01 = H1[Il], Z11 = H1[Il + 1];
						const double Least = 0.5 * (U + V - std::fabs(U - V));
						const float Main = static_cast<float>(Z00 + U * (Z10 - Z00) + V * (Z01 - Z00) + Least * ((Z11 - Z10) - (Z01 - Z00)));
						const float E = std::max(Pl.EaseX[static_cast<size_t>(I)], Ey);
						Row[I] += E * (Main - Row[I]);
					}
				};
				Blend(0, A);
				Blend(B, Pn);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Light on the water.

	double PixelFade(double L, double Depth, double PxPerRad) { return Sat((PxPerRad / std::max(Depth, 1e-3) * L - 4.0) * 0.25); }

	double FarLean(double Depth) { return Lerp(1.0, 0.35, Sat(Depth / 6000.0)); }

	double DrawnMss(const Sea& S, const Drawn& D, double Depth)
	{
		double M = 0.0;
		for (int J = 0; J < S.ND; ++J) M += PixelFade(S.D[J].L, Depth, D.PxPerRad) * Sq(S.D[J].K * S.D[J].A) * 0.5;
		for (int J = 0; J < S.NP; ++J) M += PixelFade(S.P[J].L, Depth, D.PxPerRad) * Sq(S.P[J].K * S.P[J].A) * 0.5;
		return M + S.DetailMss * Sq(FarLean(Depth));
	}

	double RoughnessFor(double R0, double Dmss) { return std::pow(std::pow(std::max(R0, 0.0), 4.0) + std::max(Dmss, 0.0), 0.25); }

	// ----------------------------------------------------------------------------------------------------------------
	// The stage's numbers.

	ForStage EmptyStage()
	{
		ForStage F;
		F.Shape = { 1.0f, 1.0f, 1.0f, 8.0f };
		F.Fold = { -1.0f, 1.0f, 1.0f, 0.0f };
		F.Lean = { 0.0f, 0.0f, 0.0f, 1.0f };
		F.PatchEase = { 0.0f, 0.0f, 0.0f, 1.0f };
		F.Fade = { 0.0f, 0.0f, 0.0f, 0.0f };
		return F;
	}

	ForStage StageParams(const Sea& S, const Drawn& D, double UnitsPerMetre)
	{
		const double U = UnitsPerMetre;
		ForStage F = EmptyStage();
		auto Phase = [&](const Train& T, double Time) { return static_cast<float>(Turns((T.Kx * D.OriginX + T.Ky * D.OriginY + T.Phase0 - T.W * Time) / TwoPi)); };
		auto Wave = [&](const Train& T, double Time) { return F4{ static_cast<float>(T.Ky / (TwoPi * U)), static_cast<float>(T.Kx / (TwoPi * U)), static_cast<float>(T.A * U), Phase(T, Time) }; };
		auto Stage = [&](double Bx, double By, float& Sx, float& Sy)
		{
			Sx = static_cast<float>((By - D.OriginY) * U);
			Sy = static_cast<float>((Bx - D.OriginX) * U);
		};
		for (int J = 0; J < S.ND; ++J)
		{
			const Train& T = S.D[J];
			F.Train[J] = Wave(T, D.SeaT);
			F.TrainBefore[J] = Wave(T, D.SeaTBefore);
			F.Steep[J] = { static_cast<float>(T.Q * T.A * U), static_cast<float>(T.L * U), static_cast<float>(T.W), static_cast<float>(T.K * T.A) };
		}
		for (int J = 0; J < S.NP; ++J) F.Pixel[J] = Wave(S.P[J], D.SeaT);
		for (int K = 0; K < std::clamp(D.Rings, 1, MostRings); ++K)
		{
			float Sx = 0.0f, Sy = 0.0f;
			Stage(D.RingX[K], D.RingY[K], Sx, Sy);
			F4& R = F.Rings[K / 2];
			if (K % 2 == 0)
			{
				R.R = Sx;
				R.G = Sy;
			}
			else
			{
				R.B = Sx;
				R.A = Sy;
			}
		}
		Stage(D.EyeX, D.EyeY, F.Eye.R, F.Eye.G);
		Stage(D.EyeBeforeX, D.EyeBeforeY, F.Eye.B, F.Eye.A);
		F.Shape = { static_cast<float>(D.C0 * U), static_cast<float>(std::clamp(D.Rings, 1, MostRings)), static_cast<float>(D.PxPerRad), static_cast<float>(D.N) };
		if (D.Patch.bOn && D.Patch.Cell > 0.0)
		{
			Stage(D.Patch.X, D.Patch.Y, F.Patch.R, F.Patch.G);
			F.Patch.B = static_cast<float>(D.Patch.Half * U);
			F.Patch.A = static_cast<float>(D.Patch.Cell * U);
		}
		else
		{
			Stage(D.Patch.X, D.Patch.Y, F.Patch.R, F.Patch.G);
		}
		if (D.Patch.bOn && D.Patch.Cell > 0.0)
		{
			// (her offset from the patch's middle, which stays small, so the chip's distance to her keeps a float's places)
			F.PatchEase.R = static_cast<float>((D.Patch.SubjectY - D.Patch.Y) * U);
			F.PatchEase.G = static_cast<float>((D.Patch.SubjectX - D.Patch.X) * U);
			F.PatchEase.B = static_cast<float>(D.Patch.Full * U);
			F.PatchEase.A = static_cast<float>(D.Patch.EaseEnd * U);
		}
		F.Fade = { static_cast<float>(D.Patch.bOn && D.Patch.Cell > 0.0 ? Sat(D.Patch.Presence) : 0.0), static_cast<float>(S.FarHead), 0.0f, 0.0f };
		F.Fold = { static_cast<float>(S.Fold), static_cast<float>(S.FoldSoft), static_cast<float>(S.TailTau), static_cast<float>(TailShare) };
		F.Lean = { static_cast<float>(S.Qb / std::max(S.ND + S.NP, 1)), static_cast<float>(S.Qb), static_cast<float>(S.DetailMss), static_cast<float>(U) };
		F.Rough = { static_cast<float>(S.CoxMunk), static_cast<float>(S.WindU), 0.0f, 0.0f };
		for (int R = 0; R < ReadCount; ++R)
		{
			const float V = static_cast<float>(Turns(S.Read[R] * D.SeaT));
			F4& Into = F.ReadPhase[R / 4];
			(R % 4 == 0 ? Into.R : R % 4 == 1 ? Into.G : R % 4 == 2 ? Into.B : Into.A) = V;
		}
		F.Origin = { static_cast<float>(D.OriginY * U), static_cast<float>(D.OriginX * U), 0.0f, 0.0f };
		return F;
	}

	std::vector<std::pair<std::string, F4>> StageList(const ForStage& F)
	{
		std::vector<std::pair<std::string, F4>> L;
		for (int J = 0; J < MostDrawn; ++J) L.emplace_back("Train" + std::to_string(J), F.Train[J]);
		for (int J = 0; J < MostDrawn; ++J) L.emplace_back("TrainBefore" + std::to_string(J), F.TrainBefore[J]);
		for (int J = 0; J < MostDrawn; ++J) L.emplace_back("Steep" + std::to_string(J), F.Steep[J]);
		for (int J = 0; J < MostPixel; ++J) L.emplace_back("Pixel" + std::to_string(J), F.Pixel[J]);
		for (int J = 0; J < 4; ++J) L.emplace_back("Rings" + std::to_string(J), F.Rings[J]);
		L.emplace_back("Eye", F.Eye);
		L.emplace_back("Shape", F.Shape);
		L.emplace_back("Patch", F.Patch);
		L.emplace_back("PatchEase", F.PatchEase);
		L.emplace_back("Fold", F.Fold);
		L.emplace_back("Lean", F.Lean);
		L.emplace_back("Rough", F.Rough);
		for (int J = 0; J < 3; ++J) L.emplace_back("ReadPhase" + std::to_string(J), F.ReadPhase[J]);
		L.emplace_back("Origin", F.Origin);
		L.emplace_back("Fade", F.Fade);
		return L;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The wake lane.

	LaneWindow LaneWindowFor(double EyeX, double EyeY)
	{
		LaneWindow W;
		const double Texel = W.Size / W.Texels;
		W.X0 = std::floor((EyeX - 0.5 * W.Size) / Texel) * Texel;
		W.Y0 = std::floor((EyeY - 0.5 * W.Size) / Texel) * Texel;
		return W;
	}

	void PaintLanes(const std::vector<LanePoint>& Wake, const std::vector<OilDisc>& Oil, const LaneWindow& W, double WindU, std::vector<uint8_t>& RG)
	{
		const int N = std::max(W.Texels, 1);
		const double Texel = W.Size / N;
		std::vector<float> Red(static_cast<size_t>(N) * N, 0.0f), Green(static_cast<size_t>(N) * N, 0.0f);
		// [EST] a lane spreads and fades faster in a stronger wind; its fresh foam lasts under a minute
		const double Spread = 0.02 + 0.004 * std::max(WindU, 0.0), Life = 900.0 / (1.0 + 0.2 * std::max(WindU, 0.0));
		const double FoamLife = 40.0 / (1.0 + 0.05 * std::max(WindU, 0.0));
		auto Stamp = [&](double X0, double Y0, double Reach, auto&& Value)
		{
			const int I0 = std::max(0, static_cast<int>(std::floor((X0 - Reach - W.X0) / Texel))), I1 = std::min(N - 1, static_cast<int>(std::floor((X0 + Reach - W.X0) / Texel)));
			const int J0 = std::max(0, static_cast<int>(std::floor((Y0 - Reach - W.Y0) / Texel))), J1 = std::min(N - 1, static_cast<int>(std::floor((Y0 + Reach - W.Y0) / Texel)));
			for (int J = J0; J <= J1; ++J)
			{
				for (int I = I0; I <= I1; ++I) Value(W.X0 + (I + 0.5) * Texel, W.Y0 + (J + 0.5) * Texel, static_cast<size_t>(J) * N + I);
			}
		};
		for (size_t K = 0; K + 1 < Wake.size(); ++K)
		{
			const LanePoint& A = Wake[K];
			const LanePoint& B = Wake[K + 1];
			if (A.Ship != B.Ship) continue;
			const double Dx = B.X - A.X, Dy = B.Y - A.Y, Len2 = Dx * Dx + Dy * Dy;
			const double Widest = 0.5 * std::max(A.Beam, B.Beam) * (1.0 + Spread * std::max(A.Age, B.Age));
			const double Mx = 0.5 * (A.X + B.X), My = 0.5 * (A.Y + B.Y), Reach = 0.5 * std::sqrt(Len2) + Widest + Texel;
			Stamp(Mx, My, Reach, [&](double X, double Y, size_t At)
			{
				const double T = Len2 > 0.0 ? Sat(((X - A.X) * Dx + (Y - A.Y) * Dy) / Len2) : 0.0;
				const double Px = A.X + T * Dx, Py = A.Y + T * Dy, Dist = std::hypot(X - Px, Y - Py);
				const double Age = Lerp(A.Age, B.Age, T), Beam = Lerp(A.Beam, B.Beam, T);
				const double Half = 0.5 * Beam * (1.0 + Spread * Age), FoamHalf = 0.4 * Beam * (1.0 + 0.01 * Age);
				if (Dist < Half) Red[At] = std::max(Red[At], static_cast<float>(std::exp(-Age / Life) * (1.0 - Sq(Dist / Half))));
				if (Dist < FoamHalf) Green[At] = std::max(Green[At], static_cast<float>(std::exp(-Age / FoamLife) * (1.0 - Sq(Dist / FoamHalf))));
			});
		}
		for (const OilDisc& O : Oil)
		{
			if (O.Radius <= 0.0) continue;
			Stamp(O.X, O.Y, O.Radius + Texel, [&](double X, double Y, size_t At)
			{
				const double Dist = std::hypot(X - O.X, Y - O.Y);
				if (Dist < O.Radius) Red[At] = std::max(Red[At], static_cast<float>(Sat((O.Radius - Dist) / std::max(2.0 * Texel, 1e-6))));
			});
		}
		RG.assign(static_cast<size_t>(N) * N * 2, 0);
		for (size_t K = 0; K < Red.size(); ++K)
		{
			RG[2 * K] = static_cast<uint8_t>(std::lround(255.0 * Sat(Red[K])));
			RG[2 * K + 1] = static_cast<uint8_t>(std::lround(255.0 * Sat(Green[K])));
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The gale (step 14(c)): new functions only; nothing above reads them.

	double FetchOfHours(double U, double Hours)
	{
		const double Uu = std::max(U, 0.5), T = std::max(Hours, 0.0) * 3600.0;
		return Sq(Uu) / Gee * std::pow(Gee * T / (68.8 * Uu), 1.5);
	}

	double GaleFetchOf(double U, double FieldFetch, double Hours)
	{
		if (U <= Raised) return FieldFetch;
		const double Fetch = FetchOfHours(std::min(U, GaleCap), Hours);
		return FieldFetch > 0.0 ? std::min(Fetch, FieldFetch) : Fetch;
	}

	double StreakShareOf(double U)
	{
		static const double At[] = { 10.8, 13.9, 17.2, 20.8, GaleCap }, Is[] = { 0.0, 0.15, 0.45, 0.8, 1.0 };
		if (U <= At[0]) return 0.0;
		for (int K = 1; K < 5; ++K)
		{
			if (U <= At[K]) return Is[K - 1] + (Is[K] - Is[K - 1]) * (U - At[K - 1]) / (At[K] - At[K - 1]);
		}
		return 1.0;
	}

	double HalfBreadthShare(double S)
	{
		const double A = std::min(std::fabs(S), 1.0);
		return S >= 0.0 ? 1.0 - A * A : 1.0 - A * A * A;
	}

	namespace
	{
		/** A hull as the footprint reads her: her middle, her heading's sine and cosine, her half length and half beam
		 * (taken once a hull, not once a texel). */
		struct HullFrame
		{
			double X = 0.0, Y = 0.0, Sx = 0.0, Cy = 1.0, Half = 0.5, Bh = 0.0;
		};
		HullFrame FrameOfHull(const LaneHull& H)
		{
			HullFrame F;
			F.X = H.X;
			F.Y = H.Y;
			F.Half = 0.5 * std::max(H.Long, 1e-3);
			F.Bh = 0.5 * std::max(H.Beam, 0.0);
			F.Sx = std::sin(H.Hdg);
			F.Cy = std::cos(H.Hdg);
			return F;
		}
		double InsideOf(const HullFrame& F, double X, double Y)
		{
			const double A = (X - F.X) * F.Sx + (Y - F.Y) * F.Cy; // (along her, forward plus)
			const double C = std::fabs((X - F.X) * F.Cy - (Y - F.Y) * F.Sx); // (across her, either side)
			if (std::fabs(A) >= F.Half) return -std::hypot(std::fabs(A) - F.Half, C); // (past her stem or her stern)
			const double S = A / F.Half;
			const double Slope = (S >= 0.0 ? -2.0 * S : 3.0 * S * S) * F.Bh / F.Half; // (the half-breadth's slope along her)
			return (F.Bh * HalfBreadthShare(S) - C) / std::sqrt(1.0 + Slope * Slope);
		}
		/** The texels a hull's footprint reaches (her square, with FootReach round her: beyond it the distance is clamped to
		 * -FootReach, which is blue 0). */
		struct TexelBox
		{
			int I0 = 0, I1 = -1, J0 = 0, J1 = -1;
		};
		TexelBox BoxOf(const LaneHull& H, const LaneWindow& W, int N, double Texel)
		{
			const double Reach = 0.5 * std::hypot(H.Long, H.Beam) + FootReach + Texel;
			TexelBox B;
			B.I0 = std::max(0, static_cast<int>(std::floor((H.X - Reach - W.X0) / Texel)));
			B.I1 = std::min(N - 1, static_cast<int>(std::floor((H.X + Reach - W.X0) / Texel)));
			B.J0 = std::max(0, static_cast<int>(std::floor((H.Y - Reach - W.Y0) / Texel)));
			B.J1 = std::min(N - 1, static_cast<int>(std::floor((H.Y + Reach - W.Y0) / Texel)));
			return B;
		}
	}

	double HullInside(const LaneHull& H, double X, double Y) { return InsideOf(FrameOfHull(H), X, Y); }

	void PaintLanesGale(const std::vector<LanePoint>& Wake, const std::vector<OilDisc>& Oil, const std::vector<LaneHull>& Hulls, const LaneWindow& W, double WindU,
		std::vector<uint8_t>& RGBA)
	{
		// (the step 12 lane's red and green, into a buffer the thread keeps, so a paint allocates nothing once warm)
		thread_local std::vector<uint8_t> RG;
		thread_local std::vector<float> In;
		PaintLanes(Wake, Oil, W, WindU, RG);
		const int N = std::max(W.Texels, 1);
		const double Texel = W.Size / N;
		const size_t Count = static_cast<size_t>(N) * N;
		RGBA.resize(Count * 4);
		for (size_t K = 0; K < Count; ++K)
		{
			RGBA[4 * K] = RG[2 * K];
			RGBA[4 * K + 1] = RG[2 * K + 1];
			RGBA[4 * K + 2] = 0; // (-FootReach and beyond: blue 0, as lround(255 x 0) is)
			RGBA[4 * K + 3] = 0;
		}
		if (In.size() != Count) In.assign(Count, static_cast<float>(-FootReach));
		// Each hull's box: cleared, then the most inside of every hull that reaches each texel, then its blue byte; the boxes
		// alone are touched, and left cleared for the next paint
		for (const LaneHull& H : Hulls)
		{
			const TexelBox B = BoxOf(H, W, N, Texel);
			for (int J = B.J0; J <= B.J1; ++J)
			{
				for (int I = B.I0; I <= B.I1; ++I) In[static_cast<size_t>(J) * N + I] = static_cast<float>(-FootReach);
			}
		}
		for (const LaneHull& H : Hulls)
		{
			const TexelBox B = BoxOf(H, W, N, Texel);
			const HullFrame F = FrameOfHull(H);
			for (int J = B.J0; J <= B.J1; ++J)
			{
				for (int I = B.I0; I <= B.I1; ++I)
				{
					const size_t At = static_cast<size_t>(J) * N + I;
					In[At] = std::max(In[At], static_cast<float>(std::clamp(InsideOf(F, W.X0 + (I + 0.5) * Texel, W.Y0 + (J + 0.5) * Texel), -FootReach, FootReach)));
				}
			}
		}
		for (const LaneHull& H : Hulls)
		{
			const TexelBox B = BoxOf(H, W, N, Texel);
			for (int J = B.J0; J <= B.J1; ++J)
			{
				for (int I = B.I0; I <= B.I1; ++I)
				{
					const size_t At = static_cast<size_t>(J) * N + I;
					RGBA[4 * At + 2] = static_cast<uint8_t>(std::lround(255.0 * Sat(0.5 + In[At] / (2.0 * FootReach))));
				}
			}
		}
		for (const LaneHull& H : Hulls)
		{
			const TexelBox B = BoxOf(H, W, N, Texel);
			for (int J = B.J0; J <= B.J1; ++J)
			{
				for (int I = B.I0; I <= B.I1; ++I) In[static_cast<size_t>(J) * N + I] = static_cast<float>(-FootReach);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The chip's text in float: FleetWavesGpu.inl through stand-ins for the shader's types.

	namespace Gpu
	{
		namespace
		{
			bool KeepNaNs = false;
		}
		float2 operator+(float2 A, float2 B) { return float2(A.x + B.x, A.y + B.y); }
		float2 operator-(float2 A, float2 B) { return float2(A.x - B.x, A.y - B.y); }
		float2 operator*(float2 A, float B) { return float2(A.x * B, A.y * B); }
		float4 operator+(float4 A, float4 B) { return float4(A.x + B.x, A.y + B.y, A.z + B.z, A.w + B.w); }
		float4 operator-(float4 A, float4 B) { return float4(A.x - B.x, A.y - B.y, A.z - B.z, A.w - B.w); }
		float4 operator*(float4 A, float B) { return float4(A.x * B, A.y * B, A.z * B, A.w * B); }
		float floor(float X) { return std::floor(X); }
		float frac(float X) { return X - std::floor(X); }
		float abs(float X) { return std::fabs(X); }
		float sqrt(float X) { return std::sqrt(X); }
		float sin(float X) { return std::sin(X); }
		float cos(float X) { return std::cos(X); }
		float exp(float X) { return std::exp(X); }
		float exp2(float X) { return std::exp2(X); }
		float min(float A, float B)
		{
			if (KeepNaNs && (std::isnan(A) || std::isnan(B))) return std::nanf("");
			return A < B ? A : B;
		}
		float max(float A, float B)
		{
			if (KeepNaNs && (std::isnan(A) || std::isnan(B))) return std::nanf("");
			return A > B ? A : B;
		}
		int min(int A, int B) { return A < B ? A : B; }
		int max(int A, int B) { return A > B ? A : B; }
		float clamp(float X, float Lo, float Hi) { return min(max(X, Lo), Hi); }
		float saturate(float X)
		{
			if (KeepNaNs && std::isnan(X)) return X;
			return X < 0.0f ? 0.0f : (X > 1.0f ? 1.0f : X);
		}
		float lerp(float A, float B, float T) { return A + (B - A) * T; }
		float4 lerp(float4 A, float4 B, float T) { return A + (B - A) * T; }

		float4 Run(const Inputs& In, bool bNaNs)
		{
			KeepNaNs = bNaNs;
			const float4 Train0 = In.Train[0], Train1 = In.Train[1], Train2 = In.Train[2], Train3 = In.Train[3], Train4 = In.Train[4];
			const float4 Train5 = In.Train[5], Train6 = In.Train[6], Train7 = In.Train[7], Train8 = In.Train[8], Train9 = In.Train[9];
			const float4 Steep0 = In.Steep[0], Steep1 = In.Steep[1], Steep2 = In.Steep[2], Steep3 = In.Steep[3], Steep4 = In.Steep[4];
			const float4 Steep5 = In.Steep[5], Steep6 = In.Steep[6], Steep7 = In.Steep[7], Steep8 = In.Steep[8], Steep9 = In.Steep[9];
			const float4 Pixel0 = In.Pixel[0], Pixel1 = In.Pixel[1], Pixel2 = In.Pixel[2], Pixel3 = In.Pixel[3], Pixel4 = In.Pixel[4], Pixel5 = In.Pixel[5];
			const float4 Pixel6 = In.Pixel[6], Pixel7 = In.Pixel[7], Pixel8 = In.Pixel[8], Pixel9 = In.Pixel[9], Pixel10 = In.Pixel[10];
			const float4 Pixel11 = In.Pixel[11], Pixel12 = In.Pixel[12], Pixel13 = In.Pixel[13], Pixel14 = In.Pixel[14], Pixel15 = In.Pixel[15];
			const float4 Rings0 = In.Rings[0], Rings1 = In.Rings[1], Rings2 = In.Rings[2], Rings3 = In.Rings[3];
			const float4 Eye = In.Eye, Shape = In.Shape, Patch = In.Patch, PatchEase = In.PatchEase, Fold = In.Fold, Lean = In.Lean, Fade = In.Fade;
			const float2 Local = In.Local, Vtx = In.Vtx, Axis = In.Axis;
			const float Cell = In.Cell, Depth = In.Depth, Mode = In.Mode;
			(void)Axis;
			(void)Cell;
#include "Battle/FleetWavesGpu.inl"
		}

		Inputs From(const ForStage& F, bool bBefore)
		{
			auto V = [](const F4& C) { return float4(C.R, C.G, C.B, C.A); };
			Inputs I;
			for (int J = 0; J < MostDrawn; ++J)
			{
				I.Train[J] = V(bBefore ? F.TrainBefore[J] : F.Train[J]);
				I.Steep[J] = V(F.Steep[J]);
			}
			for (int J = 0; J < MostPixel; ++J) I.Pixel[J] = V(F.Pixel[J]);
			for (int J = 0; J < 4; ++J) I.Rings[J] = V(F.Rings[J]);
			I.Eye = bBefore ? float4(F.Eye.B, F.Eye.A, F.Eye.R, F.Eye.G) : V(F.Eye);
			I.Shape = V(F.Shape);
			I.Patch = V(F.Patch);
			I.PatchEase = V(F.PatchEase);
			I.Fold = V(F.Fold);
			I.Lean = V(F.Lean);
			I.Fade = V(F.Fade);
			return I;
		}
	}
}
