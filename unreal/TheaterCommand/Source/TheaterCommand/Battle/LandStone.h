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
// A gun and the men whose hands are on her are one thing (the art director, 27 November 2026, 20:54Z and 21:10Z): the
// piece, her gunner and any crew man within a pace and a half of him take one step, chosen from their footprints
// together, on the gunner's side of a wall. Where no one step of a pace and a half clears them all, bodies before
// metal: the step that clears the men, a foot of her mount left in the stone; and where none clears the men, none.
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

	/** One of several that stand as one thing (ClearAll): where the rules have him, the way he faces (a unit vector on
	 * the ground) and his reach. bMetal: a weapon's own mount, which gives way to the men's bodies. */
	struct Member
	{
		double X = 0.0, Y = 0.0, Fx = 0.0, Fy = 1.0;
		Body Reach;
		bool bMetal = false;
	};

	/** The walls' lines. Add them all, then ask: Clear only reads, so it may be asked from several threads at once. */
	class Walls
	{
	public:
		/** The most a body is taken to reach from its place (metres): a lying man's muzzle. */
		static constexpr double MostBody = 1.3;
		/** The furthest a man is stepped through a wall's line to have it at his back (metres): past that he stays on
		 * his own side. */
		static constexpr double MostAcross = 1.0;
		/** The furthest a gun and the men at her are stepped as one (metres): past that, the men alone; past that for
		 * them, no step. */
		static constexpr double MostAsOne = 1.5;
		/** How far clear of the stone several that stand as one are kept, every one of them (metres). They are asked
		 * for as one thing, by their leader's facing; each man at her then asks for himself by his own, and one whose
		 * facing is a hair off his gunner's must still find himself clear at her step, or he would stand a hair from
		 * it on a step of his own and be one with her no longer. */
		static constexpr double AsOneClear = 0.05;

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

		/** How ClearAll answered: they are clear as the rules have them; one step clears them all; one step clears the
		 * men and leaves metal in the stone; no step of MostAsOne clears the men, and none is taken. */
		enum class AsOne { Clear, All, MenOnly, None };

		/**
		 * One step for several that stand as one thing: Them[0] leads (the gunner, a man: the side of a wall is his,
		 * chosen as Clear chooses it for a man alone), the rest follow him to it. Where his side of the wall they are
		 * deepest in gives no step of MostAsOne that clears them, its other side is tried before her mount is left in
		 * the stone. bStood: they have their answer already and stand at, or are stepping to, the step StoodDx,
		 * StoodDy: while all are clear there they stay, whether or not the rules' places have become clear too, and
		 * while the men are clear there with her mount in the stone they stay unless a step on from there, the
		 * leader keeping his sides, clears her too. Clear, for them, is AsOneClear clear of the stone, as they stand
		 * and after a step. OutDx, OutDy the one step from the rules' places. True where there is a step to take.
		 */
		bool ClearAll(const Member* Them, int Count, bool bStood, double StoodDx, double StoodDy, double& OutDx, double& OutDy, AsOne& OutHow) const
		{
			OutDx = 0.0;
			OutDy = 0.0;
			OutHow = AsOne::Clear;
			if (Lengths.empty() || !Them || Count <= 0)
			{
				return false;
			}
			std::vector<int32_t> Near;
			bool bAnyMetal = false;
			for (int M = 0; M < Count; ++M)
			{
				if (!std::isfinite(Them[M].X) || !std::isfinite(Them[M].Y))
				{
					return false;
				}
				bAnyMetal = bAnyMetal || Them[M].bMetal;
				const auto Found = Cells.find(KeyOf(CellOf(Them[M].X), CellOf(Them[M].Y)));
				if (Found == Cells.end())
				{
					continue;
				}
				for (const int32_t K : Found->second)
				{
					bool bHave = false;
					for (const int32_t Had : Near)
					{
						bHave = bHave || Had == K;
					}
					if (!bHave)
					{
						Near.push_back(K);
					}
				}
			}
			if (Near.empty())
			{
				return false;
			}
			// (where they stand already: all of them, then the men alone)
			if (bStood && std::isfinite(StoodDx) && std::isfinite(StoodDy) && std::hypot(StoodDx, StoodDy) <= MostAsOne + 1.0e-9)
			{
				for (int Pass = 0; Pass < (bAnyMetal ? 2 : 1); ++Pass)
				{
					const bool bMetal = Pass == 0;
					double Sx = StoodDx, Sy = StoodDy;
					bool bDone = false;
					int Wall = -1, Side = 0;
					SteppedAll(Near, Them, Count, bMetal, true, StoodDx, StoodDy, -1, 0, Sx, Sy, bDone, Wall, Side);
					if (bDone && std::hypot(Sx, Sy) <= MostAsOne)
					{
						OutDx = Sx;
						OutDy = Sy;
						OutHow = bMetal ? (Sx != 0.0 || Sy != 0.0 ? AsOne::All : AsOne::Clear) : AsOne::MenOnly;
						return Sx != 0.0 || Sy != 0.0;
					}
				}
			}
			// (afresh from the rules' places: all of them, then the men alone)
			for (int Pass = 0; Pass < (bAnyMetal ? 2 : 1); ++Pass)
			{
				const bool bMetal = Pass == 0;
				double Dx = 0.0, Dy = 0.0;
				bool bDone = false;
				int FirstWall = -1, FirstSide = 0;
				const bool bMoved = SteppedAll(Near, Them, Count, bMetal, false, 0.0, 0.0, -1, 0, Dx, Dy, bDone, FirstWall, FirstSide);
				if (!bMoved)
				{
					// (clear as the rules have them; with her mount in the stone, if this is the men's pass)
					OutHow = bMetal ? AsOne::Clear : AsOne::MenOnly;
					return false;
				}
				if (bDone && std::hypot(Dx, Dy) <= MostAsOne)
				{
					OutDx = Dx;
					OutDy = Dy;
					OutHow = bMetal ? AsOne::All : AsOne::MenOnly;
					return true;
				}
				// (the other side of the wall they are deepest in, where the leader stands beside it)
				if (FirstWall >= 0 && FirstSide != 0)
				{
					double Ox = 0.0, Oy = 0.0;
					bool bOtherDone = false;
					int OtherWall = -1, OtherSide = 0;
					SteppedAll(Near, Them, Count, bMetal, false, 0.0, 0.0, FirstWall, -FirstSide, Ox, Oy, bOtherDone, OtherWall, OtherSide);
					if (bOtherDone && std::hypot(Ox, Oy) <= MostAsOne)
					{
						OutDx = Ox;
						OutDy = Oy;
						OutHow = bMetal ? AsOne::All : AsOne::MenOnly;
						return true;
					}
				}
			}
			OutHow = AsOne::None;
			return false;
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
		/** A man at Px, Py by one wall's length: the way out of its stone for him (Nx, Ny, a unit vector), how far out
		 * along it he stands (Out), and the side of its line he is to stand on (OutSide: +1 the left of A to B, -1 the
		 * right; 0 past an end, where the way out is round the end). Held: the side to hold him to where he is beside
		 * the wall (another's, whom he follows), or 0 to choose it: the side of Kx, Ky where bKeep (the place he
		 * keeps), else by where he stands and the way he faces. */
		static void ByWall(const Length& L, double Px, double Py, double Fx, double Fy, double Fore, double Aft, double Wide,
			bool bKeep, double Kx, double Ky, int Held, double& Nx, double& Ny, double& Out, int& OutSide)
		{
			const double Lx = L.Bx - L.Ax, Ly = L.By - L.Ay, L2 = Lx * Lx + Ly * Ly;
			const double Len = std::sqrt(L2);
			const double Raw = L2 > 0.0 ? ((Px - L.Ax) * Lx + (Py - L.Ay) * Ly) / L2 : 0.0;
			const double U = Raw < 0.0 ? 0.0 : Raw > 1.0 ? 1.0 : Raw;
			const double Qx = L.Ax + Lx * U, Qy = L.Ay + Ly * U;
			const double Far = std::hypot(Px - Qx, Py - Qy);
			Nx = 0.0;
			Ny = 0.0;
			Out = 0.0;
			OutSide = 0;
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
				return;
			}
			// (beside the wall: which side of its line he is to stand on)
			const double Leftx = -Ly / Len, Lefty = Lx / Len;
			const double Here = (Px - Qx) * Leftx + (Py - Qy) * Lefty;
			double Side = Here >= 0.0 ? 1.0 : -1.0;
			const double Was = bKeep ? (Kx - Qx) * Leftx + (Ky - Qy) * Lefty : 0.0;
			if (Held != 0)
			{
				Side = Held > 0 ? 1.0 : -1.0;
			}
			else if (bKeep && std::fabs(Was) > 0.01)
			{
				Side = Was > 0.0 ? 1.0 : -1.0;
			}
			else
			{
				// (the side he faces, where he faces across the wall: the wall at his back, if a step takes him there)
				const double Faces = Fx * Leftx + Fy * Lefty;
				if (std::fabs(Faces) > 0.5)
				{
					const double Want = Faces > 0.0 ? 1.0 : -1.0;
					if (Want != Side)
					{
						// (through the line to the far face, or back from the near one: across where a step does it,
						// and where it is the nearer way)
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
			OutSide = Side > 0.0 ? 1 : -1;
		}
		/** Several as one (ClearAll), from the step X, Y to one at which every one of them is clear of every wall near
		 * (OutX, OutY), a wall and a member at a time, the deepest first; false where they are clear as they stand
		 * there. Them[0]'s side of each wall he is beside is held for the rest. bWithMetal: her mount too, else the
		 * men alone. bKeeps: the leader keeps the side of each wall he is on at X, Y. HeldWall, HeldSide: one wall on
		 * which the leader is held to a side (-1: none). bOutDone: clear at OutX, OutY (the turns were enough).
		 * OutFirstWall, OutFirstSide: the wall they were deepest in as they stood at X, Y, and the leader's side of it
		 * then (0 where he is past its end); -1 where they were clear. */
		bool SteppedAll(const std::vector<int32_t>& Near, const Member* Them, int Count, bool bWithMetal, bool bKeeps, double X, double Y,
			int HeldWall, int HeldSide, double& OutX, double& OutY, bool& bOutDone, int& OutFirstWall, int& OutFirstSide) const
		{
			double Dx = X, Dy = Y;
			bool bMoved = false, bKeep = bKeeps;
			double Kx = Them[0].X + X, Ky = Them[0].Y + Y;
			bOutDone = false;
			OutFirstWall = -1;
			OutFirstSide = 0;
			for (int Turn = 0; Turn <= Turns; ++Turn)
			{
				double Deepest = 0.0, StepX = 0.0, StepY = 0.0;
				int DeepWall = -1, DeepSide = 0;
				for (const int32_t K : Near)
				{
					const Length& L = Lengths[static_cast<size_t>(K)];
					int Lead = 0;
					for (int M = 0; M < Count; ++M)
					{
						const Member& B = Them[M];
						if (B.bMetal && !bWithMetal)
						{
							continue;
						}
						const double Fore = Capped(B.Reach.Fore), Aft = Capped(B.Reach.Aft), Wide = Capped(B.Reach.Aside);
						double Nx = 0.0, Ny = 0.0, Out = 0.0;
						int Side = 0;
						if (M == 0)
						{
							ByWall(L, B.X + Dx, B.Y + Dy, B.Fx, B.Fy, Fore, Aft, Wide, bKeep, Kx, Ky, K == HeldWall ? HeldSide : 0, Nx, Ny, Out, Side);
							Lead = Side;
						}
						else
						{
							// (he follows the leader to his side; where the leader is past the wall's end, he keeps his own)
							ByWall(L, B.X + Dx, B.Y + Dy, B.Fx, B.Fy, Fore, Aft, Wide, true, B.X + Dx, B.Y + Dy, Lead, Nx, Ny, Out, Side);
						}
						// (short of AsOneClear clear of it by Deep)
						const double Deep = L.Half + AsOneClear + ToWall(Nx, Ny, B.Fx, B.Fy, Fore, Aft, Wide) - Out;
						if (Deep > Deepest)
						{
							Deepest = Deep;
							StepX = Nx * Deep;
							StepY = Ny * Deep;
							DeepWall = K;
							DeepSide = Lead;
						}
					}
				}
				if (Turn == 0)
				{
					OutFirstWall = Deepest > 1.0e-4 ? DeepWall : -1;
					OutFirstSide = Deepest > 1.0e-4 ? DeepSide : 0;
				}
				if (Deepest <= 1.0e-4)
				{
					bOutDone = true;
					break;
				}
				if (Turn == Turns)
				{
					// (the last look: not clear after every turn; within two millimetres counts)
					bOutDone = Deepest <= 2.0e-3;
					break;
				}
				Dx += StepX;
				Dy += StepY;
				bMoved = true;
				// (stepped clear of one wall, the leader keeps the side he is now on of every other)
				bKeep = true;
				Kx = Them[0].X + Dx;
				Ky = Them[0].Y + Dy;
			}
			OutX = Dx;
			OutY = Dy;
			return bMoved;
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
					double Nx = 0.0, Ny = 0.0, Out = 0.0;
					int Side = 0;
					ByWall(L, Px, Py, Fx, Fy, Fore, Aft, Wide, bKeep, Kx, Ky, 0, Nx, Ny, Out, Side);
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

	/** ClearAll for several given as the men's crowd gives them (LandMen::StoneMember, or any with its fields: X, Y,
	 * Facing in radians clockwise from north, Lie, bPiece and her Fore, Aft, Aside): the one adapter, for the battle
	 * and for the cloud test. OutHow: the AsOne as a number. At most 8 of them. */
	template <typename Given>
	bool ClearAllOf(const Walls& Stone, const Given* Them, int Count, bool bStood, double StoodDx, double StoodDy, double& OutDx, double& OutDy, int& OutHow)
	{
		Member As[8];
		const int N = Count < 8 ? Count : 8;
		for (int K = 0; K < N; ++K)
		{
			const double Heading = static_cast<double>(Them[K].Facing);
			As[K].X = Them[K].X;
			As[K].Y = Them[K].Y;
			As[K].Fx = std::sin(Heading);
			As[K].Fy = std::cos(Heading);
			As[K].bMetal = Them[K].bPiece;
			if (Them[K].bPiece)
			{
				As[K].Reach.Fore = Them[K].Fore;
				As[K].Reach.Aft = Them[K].Aft;
				As[K].Reach.Aside = Them[K].Aside;
			}
			else
			{
				As[K].Reach = BodyOf(static_cast<double>(Them[K].Lie));
			}
		}
		Walls::AsOne How = Walls::AsOne::Clear;
		const bool bStep = Stone.ClearAll(As, N, bStood, StoodDx, StoodDy, OutDx, OutDy, How);
		OutHow = static_cast<int>(How);
		return bStep;
	}
}
