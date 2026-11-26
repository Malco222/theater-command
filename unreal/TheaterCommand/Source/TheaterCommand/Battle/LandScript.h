#pragma once

// The general's walk pressed in code (release 2), for the Mac's builder, who has no hand on the mouse
// (-TheaterLandScript=<folder>): which of his units it carries into the woods before the battle, which goes forward,
// which attacks whom, where one fires into cover and whose plate it holds up. Plain C++ over his clipboard (LandCommand)
// and the rules' ground (LandFeed), so the cloud test (land_command_test.cpp) makes the same choices the game will;
// ALandBattle (LandBattleCommand.cpp) presses them through his own pointer, as his hand would.

#include "Battle/LandCommand.h"
#include "Battle/LandFeed.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace LandScript
{
	using Point = std::pair<double, double>;

	/** Whether the rules' cell under a place is woods (a block standing there at the start is hidden). */
	inline bool Woods(const LandFeed::Field& F, double X, double Y)
	{
		if (F.Cols <= 0 || F.Rows <= 0 || F.Cell <= 0.0 || F.Ground.size() < static_cast<size_t>(F.Cols) * static_cast<size_t>(F.Rows))
		{
			return false;
		}
		if (X < 0.0 || Y < 0.0 || X >= F.Cols * F.Cell || Y >= F.Rows * F.Cell)
		{
			return false;
		}
		const int Col = std::clamp(static_cast<int>(std::floor(X / F.Cell)), 0, F.Cols - 1);
		const int Row = std::clamp(static_cast<int>(std::floor(Y / F.Cell)), 0, F.Rows - 1);
		const size_t Kind = F.Ground[static_cast<size_t>(Row) * static_cast<size_t>(F.Cols) + static_cast<size_t>(Col)];
		return Kind < F.Grounds.size() && F.Grounds[Kind] == "woods";
	}

	/** Still on the field and fighting (not gone, routed or withdrawn). */
	inline bool Standing(const LandCommand::Tag& T)
	{
		return T.State != "gone" && T.State != "routing" && T.State != "withdrawn";
	}

	inline bool Among(const std::vector<int>& Blocks, int Block)
	{
		return std::find(Blocks.begin(), Blocks.end(), Block) != Blocks.end();
	}

	/** The middle of the enemy's blocks still on the field (all of them, seen or not: only to know which way is
	 * forward); false with none. */
	inline bool FoeMiddle(const LandCommand::General& G, double& OutX, double& OutY)
	{
		double Sx = 0.0, Sy = 0.0;
		int N = 0;
		for (const LandCommand::Tag& T : G.Tags())
		{
			if (T.bMine || !Standing(T)) continue;
			Sx += T.X;
			Sy += T.Y;
			++N;
		}
		if (N == 0) return false;
		OutX = Sx / N;
		OutY = Sy / N;
		return true;
	}

	/** His block that takes orders, of a kind ("inf", "arm", ...; "" any) and not among Not, the one nearest the enemy;
	 * -1 none. */
	inline int Mine(const LandCommand::General& G, const std::string& Kind, const std::vector<int>& Not)
	{
		double Fx = 0.0, Fy = 0.0;
		const bool bFoe = FoeMiddle(G, Fx, Fy);
		int Best = -1;
		double Nearest = 1e300;
		for (const LandCommand::Tag& T : G.Tags())
		{
			if (!T.bMine || !T.bOrderable || Among(Not, T.Block) || (!Kind.empty() && T.Kind != Kind)) continue;
			const double Far = bFoe ? std::hypot(T.X - Fx, T.Y - Fy) : 0.0;
			if (Far < Nearest)
			{
				Nearest = Far;
				Best = T.Block;
			}
		}
		return Best;
	}

	/** The rules' clearance between two blocks set down in the setup step, metres (LAND.placeClear), and a little more. */
	constexpr double PlaceClear = 12.0 + 2.0;

	/**
	 * In the setup step: the woods on his own ground nearest where the block stands now, where it may stand (as far as
	 * the screen can tell) with woods a little round it as well as under it (so it is not on the edge), clear of every
	 * other block by the rules' own margin (their boxes, as the rules measure them) and of the places already Taken (for
	 * blocks of its own size). False when his ground has no woods for it.
	 */
	inline bool WoodsFor(const LandCommand::General& G, const LandFeed::Field& F, int Block, const std::vector<Point>& Taken, double& OutX, double& OutY)
	{
		const LandCommand::Tag* T = G.TagOf(Block);
		const double* Room = G.Room();
		if (!T || !Room || Room[2] <= Room[0] || Room[3] <= Room[1]) return false;
		const double Step = std::max(10.0, F.Cell * 0.5);
		const double Round = 10.0;
		auto Clear = [&](double X, double Y)
		{
			for (const LandCommand::Tag& O : G.Tags())
			{
				if (O.Block == Block || O.State == "gone") continue;
				if (std::abs(O.X - X) < (O.W + T->W) * 0.5 + PlaceClear && std::abs(O.Y - Y) < (O.D + T->D) * 0.5 + PlaceClear) return false;
			}
			return std::none_of(Taken.begin(), Taken.end(), [&](const Point& P) { return std::abs(P.first - X) < T->W + PlaceClear && std::abs(P.second - Y) < T->D + PlaceClear; });
		};
		double Nearest = 1e300;
		bool bFound = false;
		for (int Pass = 0; Pass < 2 && !bFound; ++Pass)
		{
			for (double X = Room[0] + Step * 0.5; X < Room[2]; X += Step)
			{
				for (double Y = Room[1] + Step * 0.5; Y < Room[3]; Y += Step)
				{
					if (!Woods(F, X, Y)) continue;
					// (the first pass wants woods all round it, the second only under it)
					if (Pass == 0 && !(Woods(F, X - Round, Y) && Woods(F, X + Round, Y) && Woods(F, X, Y - Round) && Woods(F, X, Y + Round))) continue;
					if (!Clear(X, Y) || !G.MayStand(Block, X, Y)) continue;
					const double Far = std::hypot(X - T->X, Y - T->Y);
					if (Far < Nearest)
					{
						Nearest = Far;
						OutX = X;
						OutY = Y;
						bFound = true;
					}
				}
			}
		}
		return bFound;
	}

	/** A point Metres from the block toward the enemy's middle, kept 20 m inside the field; false with no enemy. */
	inline bool Forward(const LandCommand::General& G, const LandFeed::Field& F, int Block, double Metres, double& OutX, double& OutY)
	{
		const LandCommand::Tag* T = G.TagOf(Block);
		double Fx = 0.0, Fy = 0.0;
		if (!T || !FoeMiddle(G, Fx, Fy)) return false;
		const double Dx = Fx - T->X, Dy = Fy - T->Y, L = std::hypot(Dx, Dy);
		if (L < 1.0) return false;
		const double Go = std::min(Metres, L * 0.5);
		OutX = std::clamp(T->X + Dx / L * Go, 20.0, std::max(20.0, F.W - 20.0));
		OutY = std::clamp(T->Y + Dy / L * Go, 20.0, std::max(20.0, F.H - 20.0));
		return true;
	}

	/** The enemy block nearest a point that he can see and point at (met, not hidden, still fighting); -1 none. */
	inline int Foe(const LandCommand::General& G, double X, double Y)
	{
		int Best = -1;
		double Nearest = 1e300;
		for (const LandCommand::Tag& T : G.Tags())
		{
			if (T.bMine || !T.bKnown || T.bHidden || T.bVague || !Standing(T)) continue;
			const double Far = std::hypot(T.X - X, T.Y - Y);
			if (Far < Nearest)
			{
				Nearest = Far;
				Best = T.Block;
			}
		}
		return Best;
	}

	/** What a fire into cover aims at. */
	enum class Cover
	{
		None,
		Hedge,
		Woods,
	};

	/**
	 * Cover ahead of the block to fire into: the nearest point of a hedge (Hedges, field metres) between 60 and 450 m
	 * from it on the enemy's side of it, else the nearest woods there; Cover::None with neither.
	 */
	inline Cover CoverFor(const LandCommand::General& G, const LandFeed::Field& F, const std::vector<std::vector<Point>>& Hedges, int Block, double& OutX, double& OutY)
	{
		const LandCommand::Tag* T = G.TagOf(Block);
		double Fx = 0.0, Fy = 0.0;
		if (!T || !FoeMiddle(G, Fx, Fy)) return Cover::None;
		const double Dx = Fx - T->X, Dy = Fy - T->Y, L = std::hypot(Dx, Dy);
		if (L < 1.0) return Cover::None;
		auto Ahead = [&](double X, double Y, double& Far)
		{
			Far = std::hypot(X - T->X, Y - T->Y);
			return Far >= 60.0 && Far <= 450.0 && ((X - T->X) * Dx + (Y - T->Y) * Dy) / (Far * L) > 0.5;
		};
		double Nearest = 1e300;
		for (const std::vector<Point>& Line : Hedges)
		{
			for (size_t K = 0; K + 1 < Line.size(); ++K)
			{
				const double Ax = Line[K].first, Ay = Line[K].second, Bx = Line[K + 1].first, By = Line[K + 1].second;
				const int Bits = std::max(1, static_cast<int>(std::hypot(Bx - Ax, By - Ay) / 5.0));
				for (int B = 0; B <= Bits; ++B)
				{
					const double X = Ax + (Bx - Ax) * B / Bits, Y = Ay + (By - Ay) * B / Bits;
					double Far = 0.0;
					if (Ahead(X, Y, Far) && Far < Nearest)
					{
						Nearest = Far;
						OutX = X;
						OutY = Y;
					}
				}
			}
		}
		if (Nearest < 1e300) return Cover::Hedge;
		const double Step = std::max(10.0, F.Cell * 0.5);
		for (double X = std::max(0.0, T->X - 450.0); X <= std::min(F.W, T->X + 450.0); X += Step)
		{
			for (double Y = std::max(0.0, T->Y - 450.0); Y <= std::min(F.H, T->Y + 450.0); Y += Step)
			{
				double Far = 0.0;
				if (Woods(F, X, Y) && Ahead(X, Y, Far) && Far < Nearest)
				{
					Nearest = Far;
					OutX = X;
					OutY = Y;
				}
			}
		}
		return Nearest < 1e300 ? Cover::Woods : Cover::None;
	}
}
