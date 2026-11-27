#pragma once

// The stone a man is never drawn inside: the field's walls, as lines on the ground.
//
// A mend of a fault in the game (27 November 2026). The rules and the drill place the men knowing nothing of the walls
// the field lays, and a man whose place fell in a wall's thickness was drawn in the stone: the wall's faces hid all of
// him but what stood out of them, a helmet, two arms and a rifle over a toe (the Mac's film of a German machine-gun
// crew man kneeling 0.10 m from the middle of a village wall for 36 s). A man at rest is drawn clear of the stone, his
// body and his weapon; the rules' place is not touched.
//
// Where he goes (the art director, 27 November 2026, 20:03Z): the nearest clear place within about a metre, on the side
// he fires from. A wall stands at his back or beside him, not before his muzzle, where a step takes him there: a man
// who faces through the wall from inside it steps forward out of its far face. One who would have further to go than
// that stays on his own side, drawn back until his muzzle is clear.
//
// Plain C++, no engine: the men's cloud test builds it (Tools/land_show/land_stone_test.cpp).

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace LandStone
{
	/** How far a man's body and weapon reach from his place on the ground (metres): before him (his muzzle), behind him
	 * (his heels, a lying man's boots) and to either side (his shoulders). Each at most Walls::MostBody. */
	struct Body
	{
		double Fore = 0.95, Aft = 0.40, Aside = 0.35;
	};

	/** A man's reach as he stands or kneels (Lie 0) and as he lies (Lie 1), and between as he goes down. */
	inline Body BodyOf(double Lie)
	{
		const double K = Lie < 0.0 ? 0.0 : Lie > 1.0 ? 1.0 : Lie;
		Body B;
		B.Fore = 0.95 + (1.25 - 0.95) * K;
		B.Aft = 0.40 + (1.00 - 0.40) * K;
		B.Aside = 0.35;
		return B;
	}

	/** The walls' lines. Add them all, then ask: Clear only reads, so it may be asked from several threads at once. */
	class Walls
	{
	public:
		/** The most a body is taken to reach from its place (metres): a lying man's muzzle. */
		static constexpr double MostBody = 1.3;
		/** The furthest a man is stepped through a wall's line to have it at his back (metres): past that he stays on
		 * his own side. */
		static constexpr double MostAcross = 1.0;

		/** A wall's length from A to B (field metres), its stone HalfThick either side of the line. */
		void Add(double Ax, double Ay, double Bx, double By, double HalfThick)
		{
			if (!std::isfinite(Ax) || !std::isfinite(Ay) || !std::isfinite(Bx) || !std::isfinite(By) || !(HalfThick > 0.0))
			{
				return;
			}
			const int32_t K = static_cast<int32_t>(Lengths.size());
			Lengths.push_back({ Ax, Ay, Bx, By, HalfThick });
			// (a man is asked for by his rules' place's cell, and a step may carry him a body and a wall's width on)
			const double Reach = 2.0 * (HalfThick + MostBody);
			const int64_t I0 = CellOf(std::fmin(Ax, Bx) - Reach), I1 = CellOf(std::fmax(Ax, Bx) + Reach);
			const int64_t J0 = CellOf(std::fmin(Ay, By) - Reach), J1 = CellOf(std::fmax(Ay, By) + Reach);
			for (int64_t J = J0; J <= J1; ++J)
			{
				for (int64_t I = I0; I <= I1; ++I)
				{
					Cells[KeyOf(I, J)].push_back(K);
				}
			}
		}

		int Count() const { return static_cast<int>(Lengths.size()); }

		/**
		 * How far from his place a man is drawn so that he stands clear of the stone. X, Y his place (the rules');
		 * Fx, Fy the way he faces (a unit vector on the ground); Reach his body's and weapon's. bStood: he has his
		 * answer already and stands at, or is stepping to, StoodX, StoodY (where the last answer put him): while that
		 * place is clear he stays there, whether or not the rules' place has become clear too (he does not slide back
		 * as he holds his rest), and when it no longer is (he has turned his muzzle to the wall, or lain down) he
		 * steps on from there, on the side of each wall he stands on, so a man who turns where he is never changes
		 * sides. OutDx, OutDy the step from the rules' place. True where there is a step to take; false, and zeros,
		 * where he is clear as the rules have him and stands nowhere else.
		 */
		bool Clear(double X, double Y, double Fx, double Fy, const Body& Reach, bool bStood, double StoodX, double StoodY, double& OutDx, double& OutDy) const
		{
			OutDx = 0.0;
			OutDy = 0.0;
			if (Lengths.empty() || !std::isfinite(X) || !std::isfinite(Y))
			{
				return false;
			}
			const auto Found = Cells.find(KeyOf(CellOf(X), CellOf(Y)));
			if (Found == Cells.end())
			{
				return false;
			}
			double Px = X, Py = Y;
			if (bStood && std::isfinite(StoodX) && std::isfinite(StoodY) && std::hypot(StoodX - X, StoodY - Y) <= 2.0 * (MostBody + 1.0))
			{
				Stepped(Found->second, StoodX, StoodY, Fx, Fy, Reach, true, Px, Py);
			}
			else if (!Stepped(Found->second, X, Y, Fx, Fy, Reach, false, Px, Py))
			{
				return false;
			}
			OutDx = Px - X;
			OutDy = Py - Y;
			return OutDx != 0.0 || OutDy != 0.0;
		}

	private:
		struct Length
		{
			double Ax, Ay, Bx, By, Half;
		};
		static constexpr double Cell = 8.0;
		static int64_t CellOf(double V) { return static_cast<int64_t>(std::floor(V / Cell)); }
		static int64_t KeyOf(int64_t I, int64_t J) { return I * 1000003 + J; }
		static double Capped(double V) { return V < 0.0 ? 0.0 : V > MostBody ? MostBody : V; }
		/** How far a man reaches toward a wall that lies the other way from N (N points from the wall to him): his
		 * heels' where he has his back to it, his muzzle's where he faces it, his shoulders' where he stands along it. */
		static double ToWall(double Nx, double Ny, double Fx, double Fy, double Fore, double Aft, double Wide)
		{
			const double Away = Nx * Fx + Ny * Fy;
			const double Square = std::fmin(1.0, std::fabs(Away));
			return Square * (Away > 0.0 ? Aft : Fore) + (1.0 - Square) * Wide;
		}
		/** From X, Y to a place clear of every wall near (OutX, OutY), a wall at a time, the deepest first; false where
		 * he is clear as he stands there. bKeeps: he stands at X, Y and keeps the side of each wall he is on; else the
		 * side is chosen by the way he faces. */
		bool Stepped(const std::vector<int32_t>& Near, double X, double Y, double Fx, double Fy, const Body& Reach, bool bKeeps, double& OutX, double& OutY) const
		{
			const double Fore = Capped(Reach.Fore), Aft = Capped(Reach.Aft), Wide = Capped(Reach.Aside);
			double Px = X, Py = Y;
			bool bMoved = false, bKeep = bKeeps;
			double Kx = X, Ky = Y;
			for (int Turn = 0; Turn < Turns; ++Turn)
			{
				double Deepest = 0.0, StepX = 0.0, StepY = 0.0;
				for (const int32_t K : Near)
				{
					const Length& L = Lengths[static_cast<size_t>(K)];
					const double Lx = L.Bx - L.Ax, Ly = L.By - L.Ay, L2 = Lx * Lx + Ly * Ly;
					const double Len = std::sqrt(L2);
					const double Raw = L2 > 0.0 ? ((Px - L.Ax) * Lx + (Py - L.Ay) * Ly) / L2 : 0.0;
					const double U = Raw < 0.0 ? 0.0 : Raw > 1.0 ? 1.0 : Raw;
					const double Qx = L.Ax + Lx * U, Qy = L.Ay + Ly * U;
					const double Far = std::hypot(Px - Qx, Py - Qy);
					double Nx = 0.0, Ny = 0.0, Out = 0.0;
					if (Len <= 0.0 || Raw < 0.0 || Raw > 1.0)
					{
						// (past the wall's end, or a length of no length: round its end, the side he is on)
						if (Far > 1.0e-6)
						{
							Nx = (Px - Qx) / Far;
							Ny = (Py - Qy) / Far;
						}
						else
						{
							Ny = 1.0;
						}
						Out = Far;
					}
					else
					{
						// (beside the wall: which side of its line he is to stand on)
						const double Leftx = -Ly / Len, Lefty = Lx / Len;
						const double Here = (Px - Qx) * Leftx + (Py - Qy) * Lefty;
						double Side = Here >= 0.0 ? 1.0 : -1.0;
						const double Was = bKeep ? (Kx - Qx) * Leftx + (Ky - Qy) * Lefty : 0.0;
						if (bKeep && std::fabs(Was) > 0.01)
						{
							Side = Was > 0.0 ? 1.0 : -1.0;
						}
						else
						{
							// (the side he faces, where he faces across the wall: the wall at his back, if a step takes
							// him there)
							const double Faces = Fx * Leftx + Fy * Lefty;
							if (std::fabs(Faces) > 0.5)
							{
								const double Want = Faces > 0.0 ? 1.0 : -1.0;
								if (Want != Side)
								{
									// (through the line to the far face, or back from the near one: across where
									// a step does it, and where it is the nearer way)
									const double Across = std::fabs(Here) + L.Half + ToWall(Want * Leftx, Want * Lefty, Fx, Fy, Fore, Aft, Wide);
									const double Back = L.Half + ToWall(Side * Leftx, Side * Lefty, Fx, Fy, Fore, Aft, Wide) - std::fabs(Here);
									if (Across <= MostAcross || Across < Back)
									{
										Side = Want;
									}
								}
							}
						}
						Nx = Leftx * Side;
						Ny = Lefty * Side;
						Out = Here * Side;
					}
					const double Deep = L.Half + ToWall(Nx, Ny, Fx, Fy, Fore, Aft, Wide) - Out;
					if (Deep > Deepest)
					{
						Deepest = Deep;
						StepX = Nx * Deep;
						StepY = Ny * Deep;
					}
				}
				if (Deepest <= 1.0e-4)
				{
					break;
				}
				Px += StepX;
				Py += StepY;
				bMoved = true;
				// (stepped clear of one wall, he keeps the side he is now on of every other)
				bKeep = true;
				Kx = Px;
				Ky = Py;
			}
			OutX = Px;
			OutY = Py;
			return bMoved;
		}
		/** The most walls a man is stepped clear of in turn: where two meet, each step off one may bring him a little
		 * nearer the other, and a lying man in a yard's corner takes a dozen turns to the millimetre. */
		static constexpr int Turns = 16;
		std::vector<Length> Lengths;
		std::unordered_map<int64_t, std::vector<int32_t>> Cells;
	};
}
