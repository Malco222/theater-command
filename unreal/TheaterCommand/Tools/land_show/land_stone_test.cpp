// No man is drawn in a wall's stone (Battle/LandStone.h and LandMen's SetStone; a mend of a fault in the game, 27
// November 2026: a man whose place the rules set in a village wall's thickness showed as a helmet, two arms and a rifle
// standing out of its face).
//
// The walls' own answers: a man in the stone with the wall at his back steps out on his own side; one who faces
// through it from inside steps forward out of its far face; one further off who faces it stays on his side, drawn back
// until his muzzle is clear; one who stands along it clears his shoulders; a man who turns where he stands never
// changes sides; where two walls meet he is clear of both; and over 200,000 places by walls laid as the field lays
// them, every man asked for ends clear of every wall, by a step of no more than a lying man's length and a wall's.
//
// The men's crowd on a recorded battle: for men who rest some seconds, a wall laid through where each rests (as the
// rules might have had it), the battle played at 60 frames a second with the walls and, beside it, without. With them
// a man at rest stands clear of the stone (his middle a shoulder's breadth from its face at least); he takes the step
// no faster than a walk, never a jump, save at the battle's first frame where he starts clear; he takes it as he comes
// in to his rest (the art director, 27 November 2026, 20:54Z: he arrives at the clear place), so that as he stops he
// stands where the walls put him as he came, short only by what a walk could not cover in a bound of a few tenths of a
// second; while he holds a rest pose, a quarter of a second and more without turning or going down or getting up, his
// step does not move at all; on the move, once he has had the time a walk takes to give his step back and is not
// about to rest by a wall, he is where the rules have him; a man who falls lies where he was drawn; the step the
// crowd reports is the whole difference from the crowd without walls; and every man nowhere near a wall is the same
// in both to the bit. The side: once a man stands at his resting place, he is drawn on the side of his wall the first
// answer of his stay put him, no later answer of the stay puts him on the other, and each asking names, as the place
// he stands at, where the answer before it put him. A man who pivots where he stands is asked for as he will face
// (his reach may sweep the stone on the way round, counted apart); in the second after a turn or a going down his
// reach is no more than a hand's breadth in the stone.
//
// A gun's team (the art director, 27 November 2026, 21:10Z: the piece, her gunner and any crew man within 1.5 m of
// him are one thing and take one step; bodies before metal; where no step of 1.5 m clears the men the team stays).
// The walls' own answers for several as one: fixed cases (the wall at their backs, through it, astride it, a lane
// too narrow for her mount, a slot too narrow for a man, a wall too thick to clear her) and 100,000 teams by walls
// laid as the field lays them; a team is kept 0.05 m clear of the stone, so that a man at her whose facing is a hair
// off his gunner's is clear at her step by his own. In the crowd, on each recording: the recording's guns each given
// a made-up number two at the gunner's hand (who comes with him, after him, or before him), a third man 2 m off and
// for some a man at his left hand who was there 25 s before; a wall through each gunner's rest, or beyond the third
// man, or a lane, or a slot. Her gunner and her mount are never in the stone where the answer clears them; every man
// at her stands at her gunner's step, whoever came first (but one who stood there longer before the gun came than
// the crowd looks ahead, who keeps his own); the others step on their own, and never on to her ground or before her
// muzzle; nobody holding a rest pose is moved, and a team that stands with her mount in the stone is not moved to
// clear her; what the crowd tells of is the same with the guns as without; and the crowd stepped over the cores is
// the same to the bit as one man after another.
// Usage: land_stone_test <recording.json> <soldiers folder> <armies.json>
#include "land_json.h"
#include "Battle/LandMen.h"
#include "Battle/LandStone.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace LandMen;

namespace
{
	int Bad = 0;
	template <typename... A> void Fail(const char* Fmt, A... Args)
	{
		++Bad;
		if (Bad <= 40)
		{
			std::printf("FAIL: ");
			if constexpr (sizeof...(Args) == 0) std::fputs(Fmt, stdout);
			else std::printf(Fmt, Args...);
			std::printf("\n");
		}
	}

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	struct Seg
	{
		double Ax, Ay, Bx, By, Half;
	};

	/** The test's own measure, by every wall: how far inside a man's need the nearest stone lies (over 0: not clear). */
	double DeepIn(const std::vector<Seg>& Segs, double X, double Y, double Fx, double Fy, const LandStone::Body& B)
	{
		double Worst = -1e9;
		for (const Seg& S : Segs)
		{
			const double Lx = S.Bx - S.Ax, Ly = S.By - S.Ay, L2 = Lx * Lx + Ly * Ly;
			double U = L2 > 0.0 ? ((X - S.Ax) * Lx + (Y - S.Ay) * Ly) / L2 : 0.0;
			U = std::clamp(U, 0.0, 1.0);
			const double Qx = S.Ax + Lx * U, Qy = S.Ay + Ly * U;
			const double Far = std::hypot(X - Qx, Y - Qy);
			if (Far < 1e-9) return 1e9;
			const double Nx = (X - Qx) / Far, Ny = (Y - Qy) / Far;
			const double Away = Nx * Fx + Ny * Fy, Square = std::min(1.0, std::fabs(Away));
			const double Need = S.Half + Square * (Away > 0.0 ? B.Aft : B.Fore) + (1.0 - Square) * B.Aside;
			Worst = std::max(Worst, Need - Far);
		}
		return Worst;
	}

	uint64_t Seed = 0x5713E5ull;
	double Rnd()
	{
		Seed = Seed * 6364136223846793005ull + 1442695040888963407ull;
		return static_cast<double>((Seed >> 11) & 0xFFFFFFFFFFFFFull) / static_cast<double>(0x10000000000000ull);
	}

	void WallsOwn()
	{
		const double Half = 0.30;
		const LandStone::Body Up = LandStone::BodyOf(0.0), Down = LandStone::BodyOf(1.0);
		// (a man's reach, pinned: on his feet or his knee his muzzle 0.95 m, his heels 0.40, his shoulders 0.35; lying
		// 1.25 and 1.00; a step across a wall 1 m at most where the way back is no shorter)
		if (Up.Fore != 0.95 || Up.Aft != 0.40 || Up.Aside != 0.35 || Down.Fore != 1.25 || Down.Aft != 1.00 || Down.Aside != 0.35) Fail("a man's reach is no longer the pinned one");
		if (LandStone::Walls::MostAcross != 1.0 || LandStone::Walls::MostBody != 1.3) Fail("the walls' most step across or most body is no longer the pinned one");
		if (Crowd::StoneSpeed != 1.4 || Crowd::StoneStill != 0.3f || Crowd::StoneDwell != 0.1) Fail("the crowd's walk, stillness or dwell is no longer the pinned one");
		// One wall along the north-south line x 100, from y 0 to y 20: its left (Add's A to B) is west.
		LandStone::Walls W;
		W.Add(100.0, 0.0, 100.0, 20.0, Half);
		double Dx = 0.0, Dy = 0.0;
		// (in the stone, 0.10 m west of the line, facing west: the wall at his back; out to the west)
		if (!W.Clear(99.9, 10.0, -1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man in the stone was left there");
		if (std::fabs((99.9 + Dx) - (100.0 - Half - Up.Aft)) > 1e-3 || std::fabs(Dy) > 1e-9) Fail("the wall at his back: drawn at x %.3f, wanted %.3f", 99.9 + Dx, 100.0 - Half - Up.Aft);
		// (the same place, facing east through the wall: a step forward out of the far face, the wall at his back)
		if (!W.Clear(99.9, 10.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man facing through the stone was left there");
		if (std::fabs((99.9 + Dx) - (100.0 + Half + Up.Aft)) > 1e-3) Fail("facing through the wall from inside it: drawn at x %.3f, wanted %.3f", 99.9 + Dx, 100.0 + Half + Up.Aft);
		if (std::fabs(Dx) > LandStone::Walls::MostAcross + 1e-9) Fail("the step across was %.3f m, over %.2f", std::fabs(Dx), LandStone::Walls::MostAcross);
		// (0.60 m west of the line, outside the stone, facing east at it: further than a step across; back until his
		// muzzle is clear)
		if (!W.Clear(99.4, 10.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man with his muzzle in the wall was left there");
		if (std::fabs((99.4 + Dx) - (100.0 - Half - Up.Fore)) > 1e-3) Fail("facing the wall from outside it: drawn at x %.3f, wanted %.3f", 99.4 + Dx, 100.0 - Half - Up.Fore);
		// (along the wall, 0.2 m east of the line: his shoulders clear, on his own side)
		if (!W.Clear(100.2, 10.0, 0.0, 1.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man along the wall in its stone was left there");
		if (std::fabs((100.2 + Dx) - (100.0 + Half + Up.Aside)) > 1e-3) Fail("along the wall: drawn at x %.3f, wanted %.3f", 100.2 + Dx, 100.0 + Half + Up.Aside);
		// (on the line itself, along it: to the line's left, a whole step)
		if (!W.Clear(100.0, 10.0, 0.0, 1.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man on the line was left there");
		if (std::fabs((100.0 + Dx) - (100.0 - Half - Up.Aside)) > 1e-3) Fail("on the line: drawn at x %.3f, wanted %.3f", 100.0 + Dx, 100.0 - Half - Up.Aside);
		// (stood west of it already, he turns to face east through it: he keeps his side and backs off his muzzle)
		if (!W.Clear(99.9, 10.0, 1.0, 0.0, Up, true, 99.3, 10.0, Dx, Dy)) Fail("a man who turned was left in the stone");
		if (99.9 + Dx > 100.0 - Half) Fail("a man who turned where he stood changed sides (x %.3f)", 99.9 + Dx);
		if (std::fabs((99.9 + Dx) - (100.0 - Half - Up.Fore)) > 1e-3) Fail("turned to face the wall: drawn at x %.3f, wanted %.3f", 99.9 + Dx, 100.0 - Half - Up.Fore);
		// (drawn back from the wall with his muzzle to it, he turns his back to it: where the rules have him is clear
		// again, and he stays where he stands. He does not slide back as he holds his rest.)
		if (!W.Clear(99.2, 10.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man with his muzzle in the wall, 0.8 m from its line, was left there");
		{
			const double StoodX = 99.2 + Dx;
			double Ex = 0.0, Ey = 0.0;
			if (W.Clear(99.2, 10.0, -1.0, 0.0, Up, false, 0.0, 0.0, Ex, Ey)) Fail("a man with his back to the wall, 0.8 m from its line, was moved");
			if (!W.Clear(99.2, 10.0, -1.0, 0.0, Up, true, StoodX, 10.0, Ex, Ey) || std::fabs((99.2 + Ex) - StoodX) > 1e-9 || std::fabs(Ey) > 1e-9)
			{
				Fail("a man who turned his back to the wall did not stay where he stood: drawn at x %.3f, stood at %.3f", 99.2 + Ex, StoodX);
			}
		}
		// (lying in the stone, facing through it: the nearer way is forward, though it is more than a step)
		if (!W.Clear(99.95, 10.0, 1.0, 0.0, Down, false, 0.0, 0.0, Dx, Dy)) Fail("a lying man in the stone was left there");
		if (std::fabs((99.95 + Dx) - (100.0 + Half + Down.Aft)) > 1e-3) Fail("lying in the stone facing through it: drawn at x %.3f, wanted %.3f", 99.95 + Dx, 100.0 + Half + Down.Aft);
		// (past the wall's end: round it, 0.1 m north of its north end, facing south at it)
		if (!W.Clear(100.0, 20.1, 0.0, -1.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man at the wall's end was left there");
		if (std::fabs((20.1 + Dy) - (20.0 + Half + Up.Fore)) > 1e-3 || std::fabs(Dx) > 1e-9) Fail("at the wall's end: drawn at y %.3f, wanted %.3f", 20.1 + Dy, 20.0 + Half + Up.Fore);
		// (clear already; far off; no number)
		if (W.Clear(98.0, 10.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy) || Dx != 0.0 || Dy != 0.0) Fail("a man 2 m from the wall was moved");
		if (W.Clear(500.0, 500.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy) || Dx != 0.0 || Dy != 0.0) Fail("a man far from any wall was moved");
		if (W.Clear(std::nan(""), 10.0, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy) || Dx != 0.0 || Dy != 0.0) Fail("a place that is no number was moved");
		{
			LandStone::Walls None;
			None.Add(1.0, 1.0, 2.0, 2.0, 0.0);
			None.Add(std::nan(""), 1.0, 2.0, 2.0, 0.3);
			if (None.Count() != 0 || None.Clear(1.5, 1.5, 1.0, 0.0, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a wall of no thickness or no number was kept");
		}
		// A corner: a second wall east from the first's north end.
		W.Add(100.0, 20.0, 110.0, 20.0, Half);
		const std::vector<Seg> Two = { { 100.0, 0.0, 100.0, 20.0, Half }, { 100.0, 20.0, 110.0, 20.0, Half } };
		if (!W.Clear(100.2, 19.9, 0.7071, -0.7071, Up, false, 0.0, 0.0, Dx, Dy)) Fail("a man in the corner was left there");
		if (DeepIn(Two, 100.2 + Dx, 19.9 + Dy, 0.7071, -0.7071, Up) > 2e-3) Fail("a man in the corner is not clear of both walls (%.3f m in)", DeepIn(Two, 100.2 + Dx, 19.9 + Dy, 0.7071, -0.7071, Up));

		// Walls laid as the field lays them (lines of points 2 m apart that bend a little, and a square yard), and
		// 200,000 men at random within 1.5 m of them.
		LandStone::Walls Many;
		std::vector<Seg> Segs;
		auto Lay = [&](double Ax, double Ay, double Bx, double By) { Many.Add(Ax, Ay, Bx, By, Half); Segs.push_back({ Ax, Ay, Bx, By, Half }); };
		for (int Line = 0; Line < 40; ++Line)
		{
			// (each line in its own 80 m square, so no two lines cross: the field's walls meet at corners, as the yard's)
			double X = 200.0 + 80.0 * (Line % 8) + 10.0 * Rnd(), Y = 200.0 + 80.0 * (Line / 8) + 10.0 * Rnd(), Way = 6.2832 * Rnd();
			for (int K = 0; K < 12; ++K)
			{
				const double Nx = X + 2.0 * std::cos(Way), Ny = Y + 2.0 * std::sin(Way);
				Lay(X, Y, Nx, Ny);
				X = Nx;
				Y = Ny;
				Way += (K == 5 ? 1.5708 : 0.5) * (Rnd() - 0.5) * (K == 5 ? 2.0 : 1.0);
			}
		}
		Lay(50.0, 50.0, 70.0, 50.0);
		Lay(70.0, 50.0, 70.0, 70.0);
		Lay(70.0, 70.0, 50.0, 70.0);
		Lay(50.0, 70.0, 50.0, 50.0);
		int Asked = 0, Stepped = 0, NotClear = 0, Changed = 0, TurnedAbout = 0, SlidBack = 0;
		double Longest = 0.0, WorstLeft = 0.0;
		for (int I = 0; I < 200000; ++I)
		{
			const Seg& S = Segs[static_cast<size_t>(Rnd() * static_cast<double>(Segs.size())) % Segs.size()];
			const double U = Rnd(), Off = 3.0 * (Rnd() - 0.5), Len = std::hypot(S.Bx - S.Ax, S.By - S.Ay);
			const double X = S.Ax + (S.Bx - S.Ax) * U - (S.By - S.Ay) / Len * Off, Y = S.Ay + (S.By - S.Ay) * U + (S.Bx - S.Ax) / Len * Off;
			const double Face = 6.2832 * Rnd(), Fx = std::sin(Face), Fy = std::cos(Face);
			const LandStone::Body B = LandStone::BodyOf(Rnd() < 0.3 ? 1.0 : Rnd() < 0.2 ? Rnd() : 0.0);
			++Asked;
			const bool bWas = DeepIn(Segs, X, Y, Fx, Fy, B) > 1e-4;
			const bool bStep = Many.Clear(X, Y, Fx, Fy, B, false, 0.0, 0.0, Dx, Dy);
			if (bStep) ++Stepped;
			if (!bWas && bStep) Fail("a man clear of every wall was moved %.3f m (at %.2f %.2f)", std::hypot(Dx, Dy), X, Y);
			if (bWas && !bStep) Fail("a man %.3f m inside his need was left there (at %.2f %.2f)", DeepIn(Segs, X, Y, Fx, Fy, B), X, Y);
			Longest = std::max(Longest, std::hypot(Dx, Dy));
			if (DeepIn(Segs, X + Dx, Y + Dy, Fx, Fy, B) > 2e-3)
			{
				++NotClear;
				WorstLeft = std::max(WorstLeft, DeepIn(Segs, X + Dx, Y + Dy, Fx, Fy, B));
			}
			// (asked again where he was put, as a man who stands there: he stays)
			if (bStep)
			{
				double Ex = 0.0, Ey = 0.0;
				Many.Clear(X, Y, Fx, Fy, B, true, X + Dx, Y + Dy, Ex, Ey);
				if (std::hypot(Ex - Dx, Ey - Dy) > 2e-3) ++Changed;
				// (and turned about there, where that leaves both his place and the rules' clear: he stays, he does
				// not slide back to the rules' place)
				if (DeepIn(Segs, X, Y, -Fx, -Fy, B) < -0.01 && DeepIn(Segs, X + Dx, Y + Dy, -Fx, -Fy, B) < -0.01)
				{
					++TurnedAbout;
					Many.Clear(X, Y, -Fx, -Fy, B, true, X + Dx, Y + Dy, Ex, Ey);
					if (std::hypot(Ex - Dx, Ey - Dy) > 2e-3) ++SlidBack;
				}
			}
		}
		std::printf("the walls' own: %d men asked by %zu lengths of wall, %d stepped clear, the longest step %.2f m; %d not clear after it (the worst %.4f m in); %d moved when asked again; %d turned about where both places were then clear, %d of them moved\n",
			Asked, Segs.size(), Stepped, Longest, NotClear, WorstLeft, Changed, TurnedAbout, SlidBack);
		// (a lying man's muzzle and a wall's half, twice where two walls meet)
		const double MostStep = 2.0 * (Half + LandStone::Walls::MostBody);
		if (Longest > MostStep) Fail("the longest step was %.2f m, over %.2f", Longest, MostStep);
		if (NotClear > 0) Fail("%d of %d men were not clear of every wall after the step", NotClear, Asked);
		if (Changed > 0) Fail("%d men moved again when asked where they stood", Changed);
		if (TurnedAbout < 1000) Fail("only %d men turned about with both places clear: staying was not tried", TurnedAbout);
		if (SlidBack > 0) Fail("%d men who turned about where they stood did not stay there", SlidBack);
		if (Stepped < Asked / 5) Fail("only %d of %d men were stepped: the walls were not tried", Stepped, Asked);
	}

	/** A gun's team as the crowd gives it: her gunner at X, Y facing Face (radians clockwise from north) and lying by
	 * Lie, she 0.6 m before him (her reach a machine gun's on her tripod), and his number two 1.1 m from him: Two +1 to
	 * his right, -1 to his left, 2 behind him, 0 none. */
	int TeamAt(LandStone::Member* Them, double X, double Y, double Face, double Lie, int Two)
	{
		const double Fx = std::sin(Face), Fy = std::cos(Face);
		int N = 0;
		Them[N].X = X;
		Them[N].Y = Y;
		Them[N].Fx = Fx;
		Them[N].Fy = Fy;
		Them[N].Reach = LandStone::BodyOf(Lie);
		Them[N].bMetal = false;
		++N;
		Them[N].X = X + 0.6 * Fx;
		Them[N].Y = Y + 0.6 * Fy;
		Them[N].Fx = Fx;
		Them[N].Fy = Fy;
		Them[N].Reach.Fore = 0.80;
		Them[N].Reach.Aft = 0.92;
		Them[N].Reach.Aside = 0.65;
		Them[N].bMetal = true;
		++N;
		if (Two != 0)
		{
			const double Ox = Two == 2 ? -Fx : Fy * static_cast<double>(Two), Oy = Two == 2 ? -Fy : -Fx * static_cast<double>(Two);
			Them[N].X = X + 1.1 * Ox;
			Them[N].Y = Y + 1.1 * Oy;
			Them[N].Fx = Fx;
			Them[N].Fy = Fy;
			Them[N].Reach = LandStone::BodyOf(Lie);
			Them[N].bMetal = false;
			++N;
		}
		return N;
	}

	/** The test's own measure for several at one step: how far inside the men's need, and her mount's, the nearest
	 * stone lies (over 0: not clear). */
	void DeepOfAll(const std::vector<Seg>& Segs, const LandStone::Member* Them, int Count, double Dx, double Dy, double& OutMen, double& OutMetal)
	{
		OutMen = -1e9;
		OutMetal = -1e9;
		for (int K = 0; K < Count; ++K)
		{
			const double In = DeepIn(Segs, Them[K].X + Dx, Them[K].Y + Dy, Them[K].Fx, Them[K].Fy, Them[K].Reach);
			if (Them[K].bMetal) OutMetal = std::max(OutMetal, In);
			else OutMen = std::max(OutMen, In);
		}
	}

	/** A gun and the men whose hands are on her take one step (the art director, 27 November 2026): the walls' own
	 * answers for several as one. */
	void TeamsOwn()
	{
		using LandStone::Walls;
		const double Half = 0.30, East = 1.5707963267948966, West = -East, North = 0.0;
		if (Walls::MostAsOne != 1.5) Fail("the furthest a gun and her men step as one is no longer the pinned 1.5 m");
		if (Walls::AsOneClear != 0.05) Fail("how far clear of the stone a team's step leaves it is no longer the pinned 0.05 m");
		const double C = Walls::AsOneClear;
		LandStone::Member T[8];
		double Dx = 0.0, Dy = 0.0, Men = 0.0, Metal = 0.0;
		Walls::AsOne How = Walls::AsOne::None;
		// One wall along the north-south line x 100, from y 0 to y 20: its left is west.
		Walls W;
		W.Add(100.0, 0.0, 100.0, 20.0, Half);
		const std::vector<Seg> One = { { 100.0, 0.0, 100.0, 20.0, Half } };
		// (clear of it: no step)
		int N = TeamAt(T, 95.0, 10.0, West, 1.0, 1);
		if (W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::Clear || Dx != 0.0 || Dy != 0.0) Fail("a gun's team clear of the wall was moved");
		// (her gunner lies in the stone, 0.10 m west of the line, facing west, his number two beside him: the wall at
		// their backs, one step west for all three, his boots clear of it by the team's clearance)
		N = TeamAt(T, 99.9, 10.0, West, 1.0, 1);
		if (!W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::All) Fail("a gun's team in the stone was not stepped clear as one");
		if (std::fabs(Dx - (-1.2 - C)) > 1e-3 || std::fabs(Dy) > 1e-6) Fail("the wall at the team's back: stepped %.3f %.3f, wanted %.3f 0.000", Dx, Dy, -1.2 - C);
		DeepOfAll(One, T, N, Dx, Dy, Men, Metal);
		if (Men > 2e-3 - C || Metal > 2e-3 - C) Fail("the wall at the team's back: after the step the men are %.3f m in, her mount %.3f (wanted %.2f m clear)", Men, Metal, C);
		// (what the clearance is for: his number two, whose facing is a hair off his gunner's either way (0.02
		// radians), asks for himself at her step and is clear there: he stands at it, one with her. And a team that
		// stands 0.02 m short of the clearance steps on by what it lacks and no more)
		for (int Way = -1; Way <= 1; Way += 2)
		{
			const double Off = West + 0.02 * static_cast<double>(Way);
			double Ax = 0.0, Ay = 0.0;
			W.Clear(T[2].X, T[2].Y, std::sin(Off), std::cos(Off), T[2].Reach, true, T[2].X + Dx, T[2].Y + Dy, Ax, Ay);
			if (std::hypot(Ax - Dx, Ay - Dy) > 1e-9) Fail("a number two a hair off his gunner's facing is not clear at her step: his own is %.4f %.4f, hers %.4f %.4f", Ax, Ay, Dx, Dy);
		}
		{
			double Ex = 0.0, Ey = 0.0;
			Walls::AsOne Again = Walls::AsOne::None;
			if (!W.ClearAll(T, N, true, Dx + 0.02, 0.0, Ex, Ey, Again) || Again != Walls::AsOne::All || std::fabs(Ex - Dx) > 1e-3 || std::fabs(Ey) > 1e-9)
				Fail("a team that stood 0.02 m short of its clearance: stepped to %.4f %.4f, wanted %.4f 0", Ex, Ey, Dx);
		}
		// (the same place, facing east through the wall: all three out of its far face, the wall at their backs)
		N = TeamAt(T, 99.9, 10.0, East, 1.0, 1);
		if (!W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::All) Fail("a gun's team facing through the stone was not stepped clear as one");
		if (std::fabs(Dx - (1.4 + C)) > 1e-3 || std::fabs(Dy) > 1e-6) Fail("a team facing through the wall: stepped %.3f %.3f, wanted %.3f 0.000", Dx, Dy, 1.4 + C);
		DeepOfAll(One, T, N, Dx, Dy, Men, Metal);
		if (Men > 2e-3 - C || Metal > 2e-3 - C) Fail("a team facing through the wall: after the step the men are %.3f m in, her mount %.3f", Men, Metal);
		// (a kneeling gunner and his gun, through the wall to its east face; asked again where they stand, they stay;
		// turned north there, they keep the east side and step on a little for her shoulders; asked afresh facing
		// north, they would have stepped west)
		N = TeamAt(T, 99.9, 10.0, East, 0.0, 0);
		if (!W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::All || std::fabs(Dx - (0.8 + C)) > 1e-3) Fail("a kneeling gunner and his gun facing through the wall: stepped %.3f, wanted %.3f", Dx, 0.8 + C);
		if (!W.ClearAll(T, N, true, 0.8 + C, 0.0, Dx, Dy, How) || How != Walls::AsOne::All || std::fabs(Dx - (0.8 + C)) > 1e-6 || std::fabs(Dy) > 1e-6) Fail("a team asked again where it stands moved (to %.3f %.3f)", Dx, Dy);
		N = TeamAt(T, 99.9, 10.0, North, 0.0, 0);
		if (!W.ClearAll(T, N, true, 0.8 + C, 0.0, Dx, Dy, How) || How != Walls::AsOne::All || std::fabs(Dx - (1.05 + C)) > 1e-3) Fail("a team that turned where it stood: stepped %.3f, wanted %.3f (the side it stood on)", Dx, 1.05 + C);
		if (!W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::All || std::fabs(Dx - (-0.85 - C)) > 1e-3) Fail("a team along the wall asked afresh: stepped %.3f, wanted %.3f", Dx, -0.85 - C);
		// (her gunner west of the line and his number two, at his right hand as he faces north, east of it: his own
		// side would take the number two 1.65 m through the wall; the other side clears all three in 1.05 m)
		N = TeamAt(T, 99.9, 10.0, North, 0.0, 1);
		if (!W.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::All || std::fabs(Dx - (1.05 + C)) > 1e-3) Fail("a team astride the wall: stepped %.3f (how %d), wanted %.3f to the number two's side", Dx, static_cast<int>(How), 1.05 + C);
		DeepOfAll(One, T, N, Dx, Dy, Men, Metal);
		if (Men > 2e-3 - C || Metal > 2e-3 - C) Fail("a team astride the wall: after the step the men are %.3f m in, her mount %.3f", Men, Metal);
		// A lane 1.1 m wide between two walls' faces: room for men's shoulders, none for her tripod's. In its middle the
		// men are clear as they are and she is left with her feet in the stone; 0.25 m off the middle, his shoulder
		// 0.05 m in the stone, the men step that far and the team's clearance on, and she stays at her gunner. (Out of
		// the lane through a wall is more than 1.5 m for her.)
		{
			Walls Lane;
			Lane.Add(100.0, 0.0, 100.0, 20.0, Half);
			Lane.Add(101.7, 0.0, 101.7, 20.0, Half);
			const std::vector<Seg> Two = { { 100.0, 0.0, 100.0, 20.0, Half }, { 101.7, 0.0, 101.7, 20.0, Half } };
			N = TeamAt(T, 100.85, 10.0, North, 0.0, 2);
			if (Lane.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::MenOnly || Dx != 0.0 || Dy != 0.0) Fail("men clear in a lane too narrow for their gun were moved (how %d, %.3f %.3f)", static_cast<int>(How), Dx, Dy);
			N = TeamAt(T, 100.6, 10.0, North, 0.0, 2);
			if (!Lane.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::MenOnly) Fail("in a lane too narrow for the gun the men were not stepped clear (how %d)", static_cast<int>(How));
			if (std::fabs(Dx - (0.05 + C)) > 1e-3 || std::fabs(Dy) > 1e-6) Fail("in a lane too narrow for the gun: stepped %.3f %.3f, wanted %.3f 0.000", Dx, Dy, 0.05 + C);
			DeepOfAll(Two, T, N, Dx, Dy, Men, Metal);
			if (Men > 2e-3 - C) Fail("bodies before metal: the men are %.3f m in the stone after the men's step (wanted %.2f m clear)", Men, C);
			if (Metal < 0.05) Fail("the lane was to be too narrow for her mount, and she is clear (%.3f)", Metal);
			// (a man alone has the same answer as the men's step there, but for the team's clearance)
			double Ax = 0.0, Ay = 0.0;
			Lane.Clear(100.6, 10.0, 0.0, 1.0, LandStone::BodyOf(0.0), false, 0.0, 0.0, Ax, Ay);
			if (std::fabs(Ax + C - Dx) > 1e-6 || std::fabs(Ay - Dy) > 1e-6) Fail("the men's step is not a man's own and the clearance there (%.3f against %.3f)", Dx, Ax);
		}
		// A lane 0.6 m wide between two thick walls: no room for a man's shoulders and no way out within 1.5 m: no step.
		{
			Walls Slot;
			Slot.Add(100.0, 0.0, 100.0, 20.0, 0.6);
			Slot.Add(101.8, 0.0, 101.8, 20.0, 0.6);
			N = TeamAt(T, 100.9, 10.0, North, 0.0, 2);
			if (Slot.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::None || Dx != 0.0 || Dy != 0.0) Fail("where no step of 1.5 m clears the men, one was taken (how %d, %.3f %.3f)", static_cast<int>(How), Dx, Dy);
		}
		// A wall 1.4 m thick: her gunner 0.5 m west of its line facing east, she past the line. Clearing her is 1.6 m
		// either way, over the most; the men's step back, 1.15 m and the clearance, is taken and she stays at her gunner.
		{
			Walls Thick;
			Thick.Add(100.0, 0.0, 100.0, 20.0, 0.7);
			const std::vector<Seg> Th = { { 100.0, 0.0, 100.0, 20.0, 0.7 } };
			N = TeamAt(T, 99.5, 10.0, East, 0.0, 0);
			if (!Thick.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How) || How != Walls::AsOne::MenOnly || std::fabs(Dx - (-1.15 - C)) > 1e-3) Fail("a step for her over 1.5 m: stepped %.3f (how %d), wanted the men's %.3f", Dx, static_cast<int>(How), -1.15 - C);
			DeepOfAll(Th, T, N, Dx, Dy, Men, Metal);
			if (Men > 2e-3 - C) Fail("a step for her over 1.5 m: the men are %.3f m in the stone after theirs", Men);
		}

		// Walls laid as the field lays them, and 100,000 teams at random within 1.5 m of them: her gunner lying or on
		// his knee, she before him, and for three in five a number two at his right or left hand.
		Walls Many;
		std::vector<Seg> Segs;
		auto Lay = [&](double Ax, double Ay, double Bx, double By) { Many.Add(Ax, Ay, Bx, By, Half); Segs.push_back({ Ax, Ay, Bx, By, Half }); };
		for (int Line = 0; Line < 40; ++Line)
		{
			double X = 200.0 + 80.0 * (Line % 8) + 10.0 * Rnd(), Y = 200.0 + 80.0 * (Line / 8) + 10.0 * Rnd(), Way = 6.2832 * Rnd();
			for (int K = 0; K < 12; ++K)
			{
				const double Nx = X + 2.0 * std::cos(Way), Ny = Y + 2.0 * std::sin(Way);
				Lay(X, Y, Nx, Ny);
				X = Nx;
				Y = Ny;
				Way += (K == 5 ? 1.5708 : 0.5) * (Rnd() - 0.5) * (K == 5 ? 2.0 : 1.0);
			}
		}
		Lay(50.0, 50.0, 70.0, 50.0);
		Lay(70.0, 50.0, 70.0, 70.0);
		Lay(70.0, 70.0, 50.0, 70.0);
		Lay(50.0, 70.0, 50.0, 50.0);
		int Asked = 0, WasClear = 0, AsAll = 0, AsMen = 0, AsMenStep = 0, AsNone = 0, MovedClear = 0, MenLeft = 0, MetalLeft = 0, Over = 0, NoneClear = 0, Restless = 0;
		double Longest = 0.0;
		for (int I = 0; I < 100000; ++I)
		{
			const Seg& S = Segs[static_cast<size_t>(Rnd() * static_cast<double>(Segs.size())) % Segs.size()];
			const double U = Rnd(), Off = 3.0 * (Rnd() - 0.5), Len = std::hypot(S.Bx - S.Ax, S.By - S.Ay);
			const double X = S.Ax + (S.Bx - S.Ax) * U - (S.By - S.Ay) / Len * Off, Y = S.Ay + (S.By - S.Ay) * U + (S.Bx - S.Ax) / Len * Off;
			const double Face = 6.2832 * Rnd();
			const double Lie = Rnd() < 0.6 ? 1.0 : 0.0;
			const double Pick = Rnd();
			N = TeamAt(T, X, Y, Face, Lie, Pick < 0.3 ? 1 : Pick < 0.6 ? -1 : 0);
			++Asked;
			// (as they stood: clear only where they were the team's clearance clear)
			double MenWas = 0.0, MetalWas = 0.0;
			DeepOfAll(Segs, T, N, 0.0, 0.0, MenWas, MetalWas);
			MenWas += C;
			MetalWas += C;
			const bool bStep = Many.ClearAll(T, N, false, 0.0, 0.0, Dx, Dy, How);
			const double Step = std::hypot(Dx, Dy);
			Longest = std::max(Longest, Step);
			if (Step > Walls::MostAsOne + 1e-9) ++Over;
			if (bStep != (Dx != 0.0 || Dy != 0.0)) Fail("a team's answer and its step do not agree (at %.2f %.2f)", X, Y);
			DeepOfAll(Segs, T, N, Dx, Dy, Men, Metal);
			if (MenWas <= 1e-4 && MetalWas <= 1e-4)
			{
				++WasClear;
				if (bStep || How != Walls::AsOne::Clear) ++MovedClear;
				continue;
			}
			if (How == Walls::AsOne::Clear) Fail("a team %.3f m (her mount %.3f) inside its need was called clear (at %.2f %.2f)", MenWas, MetalWas, X, Y);
			else if (How == Walls::AsOne::All)
			{
				++AsAll;
				if (Men > 2e-3 - C) ++MenLeft;
				if (Metal > 2e-3 - C) ++MetalLeft;
				// (asked again where they were put, as a team that stands there: they stay)
				double Ex = 0.0, Ey = 0.0;
				Walls::AsOne Again = Walls::AsOne::None;
				Many.ClearAll(T, N, true, Dx, Dy, Ex, Ey, Again);
				if (Again != Walls::AsOne::All || std::hypot(Ex - Dx, Ey - Dy) > 2e-3) ++Restless;
			}
			else if (How == Walls::AsOne::MenOnly)
			{
				++AsMen;
				AsMenStep += bStep;
				if (Men > 2e-3 - (bStep ? C : 0.0)) ++MenLeft;
			}
			else
			{
				++AsNone;
				// (no step: only where the men were not clear as the rules had them)
				if (MenWas <= 1e-4) ++NoneClear;
			}
		}
		std::printf("a gun's team as one: %d teams asked, %d clear as they stood; one step cleared all of %d (the longest %.2f m), the men alone of %d (%d by a step), none of %d\n",
			Asked, WasClear, AsAll, Longest, AsMen, AsMenStep, AsNone);
		std::printf("a gun's team as one: %d moved though %.2f m clear; after the step %d with a man less than that clear, %d of the all-clear with her mount less; %d steps over %.1f m; %d left as they were with the men clear; %d moved when asked again\n",
			MovedClear, C, MenLeft, MetalLeft, Over, Walls::MostAsOne, NoneClear, Restless);
		if (MovedClear > 0) Fail("%d teams clear of every wall were moved", MovedClear);
		if (MenLeft > 0) Fail("%d teams were stepped and a man of theirs left in the stone", MenLeft);
		if (MetalLeft > 0) Fail("%d teams were called all clear with her mount in the stone", MetalLeft);
		if (Over > 0) Fail("%d teams were stepped further than %.1f m", Over, Walls::MostAsOne);
		if (NoneClear > 0) Fail("%d teams were given no step and called so though their men were clear", NoneClear);
		if (Restless > 0) Fail("%d teams moved again when asked where they stood", Restless);
		if (AsAll < Asked / 5) Fail("only %d of %d teams were stepped as one: the walls were not tried", AsAll, Asked);
	}

	/** One value of a recording as JSON text again (a made-up man's stance changes are his gunner's own). */
	void WriteJson(const LandJson::Value& V, std::string& Out)
	{
		char Buf[40];
		switch (V.K)
		{
		case LandJson::Value::Null:
			Out += "null";
			break;
		case LandJson::Value::Bool:
			Out += V.N != 0.0 ? "true" : "false";
			break;
		case LandJson::Value::Number:
			std::snprintf(Buf, sizeof(Buf), "%.10g", V.N);
			Out += Buf;
			break;
		case LandJson::Value::String:
			Out += '"';
			Out += V.S;
			Out += '"';
			break;
		case LandJson::Value::Array:
			Out += '[';
			for (size_t I = 0; I < V.Items.size(); ++I)
			{
				if (I > 0) Out += ',';
				WriteJson(V.Items[I], Out);
			}
			Out += ']';
			break;
		case LandJson::Value::Object:
			Out += '{';
			for (size_t I = 0; I < V.Items.size(); ++I)
			{
				if (I > 0) Out += ',';
				Out += '"';
				Out += V.Keys[I];
				Out += "\":";
				WriteJson(V.Items[I], Out);
			}
			Out += '}';
			break;
		}
	}

	/** A gun and the men whose hands are on her as one thing, in the men's crowd on a recorded battle (the art
	 * director, 27 November 2026, 20:54Z and 21:10Z). The recording's own gun teams (a machine gun's or a mortar's key
	 * man and his crew, who in these recordings stand 4 m and more from him), and with each gunner two made-up men of
	 * his team: a number two 1.1 m at his right hand, who goes where he goes (with him, or a second and a half after
	 * him, or three seconds before him, by the gun's number), and a third man 2 m at his left; for one gun in three a
	 * man at his left hand too, who is there 25 s before him: longer than the crowd looks ahead for a gun, so he
	 * stands on a step of his own when she comes, and keeps it unless it clears them all. For most guns a wall
	 * through where the gunner rests, as the test above lays them; for one in four a wall a hand's breadth beyond the
	 * third man, whose own step clear of it would bring him on to her ground; for one a lane between two walls with
	 * room for men's shoulders and none for her mount (her number two behind her gunner: the men alone are stepped);
	 * for one a slot between two thick walls with no room for a man and no way out within the furthest a team steps
	 * (no step is taken). The battle played with the walls and the guns told of and, beside it, plain, and a third
	 * time over the cores. Her gunner at rest stands clear, and she with him wherever the walls'
	 * answer was a step for all; where it was a step for the men alone, he does; where it was none, he stands where
	 * the rules have him. His number two stands at the gunner's own step, clear, or on a step of his own, clear; the
	 * third man and the crew stand clear on their own steps, never on to her ground, and one held off it stands where
	 * the rules have him; so does a man who walks up to a gun that stands, at no frame of his way drawn on her
	 * ground. No man's step moves while he holds a rest pose (but with his gunner's, as the gunner turns
	 * or goes down), none faster than a walk; the step is the whole difference from the plain crowd; and the crowd
	 * tells of the same things at the same times with the guns as without. */
	void TeamsInBattle(const std::string& RecText, const LandJson::Value& Rec, const std::map<std::string, std::shared_ptr<Soldier>>& Files, const std::string& Armies, const char* Name)
	{
		const double Half = 0.30, Life = 2.0;
		const LandStone::Body Mount{ 0.80, 0.92, 0.65 };
		if (Crowd::StoneTeamReach != 1.5 || Crowd::StoneTeamAhead != 20.0 || Crowd::StoneMuzzleFar != 10.0 || Crowd::StoneMuzzleCos != 0.866)
			Fail("a gun's team in the crowd: its pinned figures have changed (her reach 1.5 m, the look ahead 20 s, her muzzle 10 m within cos 0.866)");
		struct Gun
		{
			int Block = 0, K = 0, Kind = 0, When = 0, Man = -1, Two = -1, Third = -1, Left = -1, Late = -1;
			size_t LeftHand = 0;
			double X = 0.0, Y = 0.0, From = 0.0, To = 0.0, Faced = 0.0;
			std::vector<Seg> Walls;
		};
		std::vector<Gun> Guns;
		std::string Added;
		// (when a made-up man comes to where his gunner rests from T0: with him, a second and a half after him, or 3 s
		// or 25 s before him. His way there is the gunner's, later or earlier; once both stand there he turns and
		// goes down as his gunner does, at the same moments)
		const auto Shifted = [](double T, int When, double T0)
		{
			const double Early = When == 2 ? 3.0 : When == 3 ? 25.0 : 0.0;
			if (T0 <= 0.0) return T;
			if (When == 1) return T < T0 ? (T <= 0.0 ? 0.0 : T + 1.5) : std::max(T, T0 + 1.5);
			return T < T0 ? std::max(0.0, T - Early) : T;
		};
		const auto AddMan = [&Added, &Shifted](const LandJson::Value& M, int J, double Ox, double Oy, int When, double T0)
		{
			char Buf[200];
			std::snprintf(Buf, sizeof(Buf), "{\"b\":%d,\"k\":%d,\"j\":%d,\"r\":\"loader\",\"at\":[", M["b"].Int(0), M["k"].Int(0), J);
			Added += Buf;
			const LandJson::Value& At = M["at"];
			bool bAny = false;
			for (size_t P = 0; P < At.Count(); ++P)
			{
				const double Was = At.At(P).At(0).Num(), T = Shifted(Was, When, T0);
				// (points wound together: the last of them is where he is then)
				if (P + 1 < At.Count() && Shifted(At.At(P + 1).At(0).Num(), When, T0) <= T) continue;
				// (a man who is there before his gunner: there from that much before, as his gunner first faces there)
				const double Early = When == 2 ? 3.0 : When == 3 ? 25.0 : 0.0;
				if (Early > 0.0 && T0 > Early && Was == T0)
				{
					std::snprintf(Buf, sizeof(Buf), "%s[%.10g,%.10g,%.10g,%.10g]", bAny ? "," : "", T0 - Early, At.At(P).At(1).Num() + Ox, At.At(P).At(2).Num() + Oy, At.At(P).At(3).Num());
					Added += Buf;
					bAny = true;
				}
				std::snprintf(Buf, sizeof(Buf), "%s[%.10g,%.10g,%.10g,%.10g]", bAny ? "," : "", T, At.At(P).At(1).Num() + Ox, At.At(P).At(2).Num() + Oy, At.At(P).At(3).Num());
				Added += Buf;
				bAny = true;
			}
			Added += "],\"ch\":[";
			const LandJson::Value& Ch = M["ch"];
			for (size_t C = 0; C < Ch.Count(); ++C)
			{
				std::snprintf(Buf, sizeof(Buf), "%s[%.10g", C > 0 ? "," : "", Shifted(Ch.At(C).At(0).Num(), When, T0));
				Added += Buf;
				for (size_t V = 1; V < Ch.At(C).Count(); ++V)
				{
					Added += ',';
					WriteJson(Ch.At(C).At(V), Added);
				}
				Added += ']';
			}
			Added += "],\"s\":[]},";
		};
		// (a man who comes to a gun that stands there from the start: he walks 8 m up along her gunner's facing to a
		// place by her and is there Arrive seconds in; from then he turns and goes down as her gunner does)
		const auto AddLate = [&Added](const LandJson::Value& M, int J, double Ox, double Oy, double WayX, double WayY, double Faced, double Arrive)
		{
			char Buf[320];
			const LandJson::Value& At = M["at"];
			const double ToX = At.At(0).At(1).Num() + Ox, ToY = At.At(0).At(2).Num() + Oy;
			std::snprintf(Buf, sizeof(Buf), "{\"b\":%d,\"k\":%d,\"j\":%d,\"r\":\"loader\",\"at\":[[0,%.10g,%.10g,%.10g],[%.10g,%.10g,%.10g,%.10g]",
				M["b"].Int(0), M["k"].Int(0), J, ToX - 8.0 * WayX, ToY - 8.0 * WayY, Faced, Arrive, ToX, ToY, Faced);
			Added += Buf;
			for (size_t P = 0; P < At.Count(); ++P)
			{
				if (At.At(P).At(0).Num() <= Arrive) continue;
				std::snprintf(Buf, sizeof(Buf), ",[%.10g,%.10g,%.10g,%.10g]", At.At(P).At(0).Num(), At.At(P).At(1).Num() + Ox, At.At(P).At(2).Num() + Oy, At.At(P).At(3).Num());
				Added += Buf;
			}
			std::snprintf(Buf, sizeof(Buf), "],\"ch\":[[0,\"walk\",\"none\",null,null,0,0],[%.10g,\"deployed\",\"none\",null,null,0,0]", Arrive);
			Added += Buf;
			const LandJson::Value& Ch = M["ch"];
			for (size_t C = 0; C < Ch.Count(); ++C)
			{
				if (Ch.At(C).At(0).Num() <= Arrive) continue;
				std::snprintf(Buf, sizeof(Buf), ",[%.10g", Ch.At(C).At(0).Num());
				Added += Buf;
				for (size_t V = 1; V < Ch.At(C).Count(); ++V)
				{
					Added += ',';
					WriteJson(Ch.At(C).At(V), Added);
				}
				Added += ']';
			}
			Added += "],\"s\":[]},";
		};
		const LandJson::Value& Men = Rec["men"];
		for (size_t I = 0; I < Men.Count(); ++I)
		{
			const LandJson::Value& M = Men.At(I);
			const std::string Role = M["r"].Str("");
			if ((Role != "hmg" && Role != "mortar") || M["j"].Int(0) != 0) continue;
			const LandJson::Value& At = M["at"];
			for (size_t A = 0; A + 1 < At.Count();)
			{
				size_t B = A;
				while (B + 1 < At.Count() && std::hypot(At.At(B + 1).At(1).Num() - At.At(A).At(1).Num(), At.At(B + 1).At(2).Num() - At.At(A).At(2).Num()) < 1e-9) ++B;
				const double T0 = At.At(A).At(0).Num(), T1 = At.At(B).At(0).Num();
				if (T1 - T0 >= 6.0 && T0 < 140.0)
				{
					Gun G;
					G.Block = M["b"].Int(0);
					G.K = M["k"].Int(0);
					// (0 a wall through where he rests, 1 one beyond the third man, 2 the lane, 3 the slot)
					G.Kind = Guns.size() % 4 == 3 ? 1 : Guns.size() == 5 ? 2 : Guns.size() == 8 ? 3 : 0;
					G.When = static_cast<int>(Guns.size() % 3);
					G.X = At.At(A).At(1).Num();
					G.Y = At.At(A).At(2).Num();
					G.From = T0;
					G.To = std::min(T1, 147.0);
					// (the way he faces once he has turned to his front there: StoneSettle into his stay)
					size_t Fc = A;
					while (Fc < B && At.At(Fc + 1).At(0).Num() <= T0 + Crowd::StoneSettle) ++Fc;
					G.Faced = At.At(Fc).At(3).Num();
					if (Fc < B)
					{
						double Turn = At.At(Fc + 1).At(3).Num() - G.Faced;
						while (Turn > 3.14159265358979) Turn -= 6.28318530717959;
						while (Turn < -3.14159265358979) Turn += 6.28318530717959;
						const double Between = At.At(Fc + 1).At(0).Num() - At.At(Fc).At(0).Num();
						G.Faced += Turn * (Between > 1e-9 ? std::clamp((T0 + Crowd::StoneSettle - At.At(Fc).At(0).Num()) / Between, 0.0, 1.0) : 0.0);
					}
					const double Ax = std::sin(G.Faced), Ay = std::cos(G.Faced), Rx = Ay, Ry = -Ax;
					// (a wall along his facing, its line Off metres to his right)
					const auto Along = [&G, Ax, Ay, Rx, Ry](double Off, double Thick)
					{
						const double Ox = G.X + Off * Rx, Oy = G.Y + Off * Ry;
						G.Walls.push_back({ Ox - 3.0 * Ax, Oy - 3.0 * Ay, Ox + 3.0 * Ax, Oy + 3.0 * Ay, Thick });
					};
					if (G.Kind == 1)
					{
						// (a hand's breadth beyond the third man at his left)
						Along(-2.10, Half);
					}
					else if (G.Kind == 2)
					{
						// (the lane: its faces 0.30 m at his left and 0.80 m at his right; a man's shoulders want 0.35)
						Along(-0.60, Half);
						Along(1.10, Half);
					}
					else if (G.Kind == 3)
					{
						// (the slot: its faces 0.30 m either side of him, the stone 1.2 m thick)
						Along(-0.90, 0.6);
						Along(0.90, 0.6);
					}
					else
					{
						// (through where he rests: across the way he faces, its middle before him, or at a heading by his number)
						const double Way = Guns.size() % 2 == 0 ? G.Faced - 1.5707963 : 0.7854 * static_cast<double>((Guns.size() / 2) % 4);
						const double Wx = std::sin(Way), Wy = std::cos(Way);
						const double Ox = G.X + 0.10 * Wy, Oy = G.Y - 0.10 * Wx;
						G.Walls.push_back({ Ox - 3.0 * Wx, Oy - 3.0 * Wy, Ox + 3.0 * Wx, Oy + 3.0 * Wy, Half });
					}
					// (his number two at his right hand; in the lane and the slot behind him)
					if (G.Kind >= 2) AddMan(M, 90, -1.1 * Ax, -1.1 * Ay, G.When, T0);
					else AddMan(M, 90, 1.1 * Rx, 1.1 * Ry, G.When, T0);
					AddMan(M, 91, -2.0 * Rx, -2.0 * Ry, 0, T0);
					// (at a gun that stands from the start with the wall beyond her third man: a man who walks up
					// inside that wall to a place a quarter of a metre on from the third man's, 8 s in. His own step
					// out of the stone would bring him within her reach of her gunner)
					if (G.Kind == 1 && T0 <= 0.0) AddLate(M, 93, -2.0 * Rx + 0.25 * Ax, -2.0 * Ry + 0.25 * Ay, Ax, Ay, G.Faced, 8.0);
					if (Guns.size() % 3 == 1 && G.Kind < 2) AddMan(M, 92, -1.1 * Rx, -1.1 * Ry, 3, T0);
					Guns.push_back(G);
					break;
				}
				A = B + 1;
			}
		}
		if (Guns.size() < 6)
		{
			Fail("only %d gun teams rest 6 s in %s: the gun's one step was not tried", static_cast<int>(Guns.size()), Name);
			return;
		}
		const size_t MenAt = RecText.find("\"men\":[");
		if (MenAt == std::string::npos)
		{
			Fail("no men in %s", Name);
			return;
		}
		const std::string Mod = RecText.substr(0, MenAt + 7) + Added + RecText.substr(MenAt + 7);
		const GroundFn Flat = [](double, double) { return Ground(); };
		Crowd Cw, Pl, Cm;
		std::string Err;
		for (Crowd* C : { &Cw, &Pl, &Cm })
		{
			C->SetLife(Life);
			for (const auto& KV : Files) C->AddSoldier(KV.first, KV.second);
			if (!C->ReadArmies(Armies, Err))
			{
				Fail("cannot read the armies: %s", Err.c_str());
				return;
			}
		}
		auto Stone = std::make_shared<LandStone::Walls>();
		std::vector<Seg> Segs;
		for (const Gun& G : Guns)
		{
			for (const Seg& Wl : G.Walls)
			{
				Stone->Add(Wl.Ax, Wl.Ay, Wl.Bx, Wl.By, Wl.Half);
				Segs.push_back(Wl);
			}
		}
		for (Crowd* C : { &Cw, &Cm })
		{
			C->SetStone([Stone](double X, double Y, float Facing, float Lie, bool bStood, double StoodX, double StoodY, double& OutDx, double& OutDy)
			{
				const double Heading = static_cast<double>(Facing);
				return Stone->Clear(X, Y, std::sin(Heading), std::cos(Heading), LandStone::BodyOf(static_cast<double>(Lie)), bStood, StoodX, StoodY, OutDx, OutDy);
			});
			C->SetStoneTeam([Stone](const StoneMember* Them, int Count, bool bStood, double StoodDx, double StoodDy, double& OutDx, double& OutDy, int& OutHow)
			{
				return LandStone::ClearAllOf(*Stone, Them, Count, bStood, StoodDx, StoodDy, OutDx, OutDy, OutHow);
			});
		}
		// (the third crowd over the cores, as the battle runs it: a gun's men read her gunner's step, which he has
		// before any of them is stepped; the same to the bit as one after another)
		Cm.SetRun([](int Count, const std::function<void(int)>& Job)
		{
			std::vector<std::thread> Threads;
			for (int K = 1; K < Count; ++K) Threads.emplace_back([&Job, K] { Job(K); });
			if (Count > 0) Job(0);
			for (std::thread& Th : Threads) Th.join();
		});
		if (!Pl.Build(Mod, Flat, Err) || !Cw.Build(Mod, Flat, Err) || !Cm.Build(Mod, Flat, Err))
		{
			Fail("cannot build the crowd with the made-up men: %s", Err.c_str());
			return;
		}
		// The men watched: each gun's gunner, his number two and the third man (in that order, three to a gun), then
		// the recording's own crews.
		struct Hand
		{
			int Man = -1, Gun = -1, What = 0;   // What: 0 her gunner, 1 his number two, 2 the third man, 3 one of the crew, 4 the man at his left hand
		};
		std::vector<Hand> Hands;
		for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
		{
			Gun& G = Guns[Gi];
			G.Man = Cw.Find(G.Block, G.K, 0);
			G.Two = Cw.Find(G.Block, G.K, 90);
			G.Third = Cw.Find(G.Block, G.K, 91);
			if (G.Man < 0 || G.Two < 0 || G.Third < 0 || Cw.IsRider(G.Man))
			{
				Fail("a gun's gunner or a made-up man of his is not in the crowd (block %d, k %d)", G.Block, G.K);
				return;
			}
			Cw.AddStoneTeam(G.Man, 0.6, 0.0, Mount.Fore, Mount.Aft, Mount.Aside);
			Cm.AddStoneTeam(G.Man, 0.6, 0.0, Mount.Fore, Mount.Aft, Mount.Aside);
			Hands.push_back({ G.Man, static_cast<int>(Gi), 0 });
			Hands.push_back({ G.Two, static_cast<int>(Gi), 1 });
			Hands.push_back({ G.Third, static_cast<int>(Gi), 2 });
		}
		for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
		{
			Gun& G = Guns[Gi];
			G.Left = Cw.Find(G.Block, G.K, 92);
			if (G.Left >= 0)
			{
				G.LeftHand = Hands.size();
				Hands.push_back({ G.Left, static_cast<int>(Gi), 4 });
			}
		}
		for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
		{
			Gun& G = Guns[Gi];
			G.Late = Cw.Find(G.Block, G.K, 93);
			// (5: the man who walks up to a gun that stands)
			if (G.Late >= 0) Hands.push_back({ G.Late, static_cast<int>(Gi), 5 });
		}
		if (Cw.StoneTeamCount() != static_cast<int>(Guns.size())) Fail("the crowd counts %d guns of the %d told of", Cw.StoneTeamCount(), static_cast<int>(Guns.size()));
		for (size_t I = 0; I < Men.Count(); ++I)
		{
			const LandJson::Value& M = Men.At(I);
			const int J = M["j"].Int(0);
			if (J < 1) continue;
			for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
			{
				if (Guns[Gi].Block != M["b"].Int(0) || Guns[Gi].K != M["k"].Int(0)) continue;
				const int Man = Cw.Find(Guns[Gi].Block, Guns[Gi].K, J);
				if (Man >= 0 && !Cw.IsRider(Man)) Hands.push_back({ Man, static_cast<int>(Gi), 3 });
			}
		}
		struct State
		{
			double Sx = 0.0, Sy = 0.0, X = 0.0, Y = 0.0, Qx = 0.0, Qy = 0.0, Moved = 0.0, Speed = 0.0;
			float F = 0.f, Qf = 0.f;
			bool bAlive = false, bWasStill = true;
			double LastSx = 0.0, LastSy = 0.0, PlainX = 0.0, PlainY = 0.0, PoseHeldFor = 0.0, SetOffAt = -1e9, StepGone = 0.0;
			double TurnedAt = -1e9, StepWas = 0.0;
			float PoseF = 0.f, PoseLie = 0.f, FacedWas = 0.f;
			double Wait[2] = { 0.0, 0.0 };
			int WaitKind[2] = { 0, 0 };
			double Skated = 0.0;
			bool bKept = false, bFarAsOne = false;
		};
		std::vector<State> St(Hands.size());
		double End = 0.0;
		for (const Gun& G : Guns) End = std::max(End, G.To + 3.0);
		End = std::min(End, 150.0);
		const double Dt = 1.0 / 60.0, Pace = Crowd::StoneSpeed * Dt;
		const int Frames = static_cast<int>(std::ceil(End / Dt)) + 1;
		int Jumps = 0, NotRules = 0, EventsOff = 0, EventsSeen = 0, CoresOff = 0;
		int SkateChecks = 0, SkateFrames = 0, BoundFrames = 0, WithFrames = 0;
		int GunChecks = 0, HowFrames[4] = { 0, 0, 0, 0 }, HowOdd = 0, GunnerIn = 0, MountIn = 0, NoneMoved = 0;
		int AsOneChecks = 0, AsOneApart = 0, OwnChecks = 0, CrewIn = 0, FarChecks = 0, FarIn = 0, KeptChecks = 0, KeptMoved = 0, OnHers = 0;
		int LateMen = 0, LateWay = 0, LateKept = 0, LateOnHers = 0;
		int LeftAsOne = 0, LeftOwn = 0, LeftCame = 0, LeftCameOwn = 0;
		double WorstGunner = -1e9, WorstMount = -1e9, WorstCrew = -1e9, WorstFar = -1e9, Longest = 0.0;
		std::vector<char> bAsOneStepped(Guns.size(), 0), bOwnStep(Guns.size(), 0);
		std::vector<int> HowFirst(Guns.size(), -1);
		for (int Fr = 0; Fr < Frames; ++Fr)
		{
			const double T = static_cast<double>(Fr) * Dt;
			Cw.Update(T);
			Pl.Update(T);
			Cm.Update(T);
			{
				const std::vector<Event>& A = Cw.Events();
				const std::vector<Event>& B = Pl.Events();
				const std::vector<Event>& C = Cm.Events();
				EventsSeen += static_cast<int>(B.size());
				bool bOff = A.size() != B.size(), bCoresOff = A.size() != C.size();
				for (size_t E = 0; E < A.size() && !bOff; ++E)
				{
					bOff = A[E].T != B[E].T || A[E].Man != B[E].Man || A[E].Fx != B[E].Fx;
				}
				for (size_t E = 0; E < A.size() && !bCoresOff; ++E)
				{
					bCoresOff = A[E].T != C[E].T || A[E].Man != C[E].Man || A[E].Fx != C[E].Fx;
				}
				EventsOff += bOff;
				for (size_t H = 0; H < Hands.size() && !bCoresOff; ++H)
				{
					double Ax = 0.0, Ay = 0.0, Az = 0.0, Bx = 0.0, By = 0.0, Bz = 0.0, Asx = 0.0, Asy = 0.0, Bsx = 0.0, Bsy = 0.0;
					float Af = 0.f, Bf = 0.f;
					Cw.PlaceOf(Hands[H].Man, Ax, Ay, Az, Af);
					Cm.PlaceOf(Hands[H].Man, Bx, By, Bz, Bf);
					Cw.StoneStep(Hands[H].Man, Asx, Asy);
					Cm.StoneStep(Hands[H].Man, Bsx, Bsy);
					bCoresOff = Ax != Bx || Ay != By || Az != Bz || Af != Bf || Asx != Bsx || Asy != Bsy
						|| Cw.StoneAsOne(Hands[H].Man) != Cm.StoneAsOne(Hands[H].Man) || Cw.StoneKeptOff(Hands[H].Man) != Cm.StoneKeptOff(Hands[H].Man);
				}
				CoresOff += bCoresOff;
			}
			for (size_t H = 0; H < Hands.size(); ++H)
			{
				State& S = St[H];
				const int Man = Hands[H].Man;
				double Z = 0.0, Qz = 0.0;
				Cw.StoneStep(Man, S.Sx, S.Sy);
				Cw.PlaceOf(Man, S.X, S.Y, Z, S.F);
				Pl.PlaceOf(Man, S.Qx, S.Qy, Qz, S.Qf);
				const double DeadAt = Pl.Info(Man).Dead;
				S.bAlive = (DeadAt < 0.0 || T < DeadAt) && Cw.Visible(Man) && Pl.Visible(Man);
				S.Moved = Fr > 0 ? std::hypot(S.Sx - S.LastSx, S.Sy - S.LastSy) : 0.0;
				if (S.Moved > Pace + 1e-9) ++Jumps;
				S.LastSx = S.Sx;
				S.LastSy = S.Sy;
				Longest = std::max(Longest, std::hypot(S.Sx, S.Sy));
				if (S.bAlive && std::hypot(S.X - S.Qx - S.Sx, S.Y - S.Qy - S.Sy) > 1e-6) ++NotRules;
				S.Speed = Fr > 0 ? std::hypot(S.Qx - S.PlainX, S.Qy - S.PlainY) / Dt : 0.0;
				S.PlainX = S.Qx;
				S.PlainY = S.Qy;
				const float LieNow = Cw.LieOf(Man);
				const bool bPose = Fr > 0 && S.Speed < 1e-6 && std::fabs(S.Qf - S.PoseF) < 1e-4f && std::fabs(LieNow - S.PoseLie) < 1e-4f && S.bAlive;
				S.PoseHeldFor = bPose ? S.PoseHeldFor + Dt : 0.0;
				S.PoseF = S.Qf;
				S.PoseLie = LieNow;
				const bool bStill = S.Speed < static_cast<double>(Crowd::StoneStill);
				if (!bStill && S.bWasStill)
				{
					S.SetOffAt = T - Dt;
					S.StepGone = 0.0;
				}
				S.bWasStill = bStill;
				S.StepGone += S.Moved;
				const double StepNow = std::hypot(S.Sx, S.Sy);
				float Turned = S.F - S.FacedWas;
				while (Turned > 3.14159265f) Turned -= 6.2831853f;
				while (Turned < -3.14159265f) Turned += 6.2831853f;
				if (Fr > 0 && (std::fabs(Turned) > 0.002f || std::fabs(StepNow - S.StepWas) > 1e-6)) S.TurnedAt = T;
				S.FacedWas = S.F;
				S.StepWas = StepNow;
			}
			// (no skating, as above; a man who stands at his gun's step moves it with her gunner's, as the gunner turns
			// his muzzle to a wall or goes down: counted apart)
			for (size_t H = 0; H < Hands.size(); ++H)
			{
				State& S = St[H];
				const Hand& Hd = Hands[H];
				const State& Sg = St[3 * static_cast<size_t>(Hd.Gun)];
				if (S.PoseHeldFor <= 0.0) S.Wait[0] = S.Wait[1] = 0.0;
				if (S.PoseHeldFor >= 0.25 && (S.Sx != 0.0 || S.Sy != 0.0)) ++SkateChecks;
				if (S.Wait[0] > 0.0)
				{
					if (S.WaitKind[0] == 2) ++WithFrames;
					else if (S.WaitKind[0] == 1) ++BoundFrames;
					else
					{
						++SkateFrames;
						S.Skated += S.Wait[0];
					}
				}
				S.Wait[0] = S.Wait[1];
				S.WaitKind[0] = S.WaitKind[1];
				const bool bBound = S.StepGone >= Crowd::StoneSpeed * (T - S.SetOffAt - Crowd::StoneDwell - 0.05);
				const bool bWith = Hd.What != 0 && Cw.StoneAsOne(Hd.Man) && Sg.Moved > 5e-4 && Sg.PoseHeldFor < 0.25;
				S.Wait[1] = S.PoseHeldFor >= 0.25 && S.Moved > 5e-4 ? S.Moved : 0.0;
				S.WaitKind[1] = bWith ? 2 : bBound ? 1 : 0;
			}
			// (each gun at rest: from three and a half seconds after her gunner stops, time for a late number two and
			// for the step at a walk, to his rest's end; none of them within half a second of a turn or a step)
			for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
			{
				const Gun& G = Guns[Gi];
				const State& Sg = St[3 * Gi];
				const State& S2 = St[3 * Gi + 1];
				if (T < G.From + 3.5 || T > G.To - 0.05 || !Sg.bAlive || T - Sg.TurnedAt <= 0.5) continue;
				const int How = Cw.StoneTeamHow(static_cast<int>(Gi));
				++GunChecks;
				if (How >= 0 && How <= 3) ++HowFrames[How];
				else ++HowOdd;
				if (HowFirst[Gi] < 0) HowFirst[Gi] = How;
				const double Fx = std::sin(static_cast<double>(Sg.F)), Fy = std::cos(static_cast<double>(Sg.F));
				const double In = DeepIn(Segs, Sg.X, Sg.Y, Fx, Fy, LandStone::BodyOf(0.0));
				const double InMount = DeepIn(Segs, Sg.X + 0.6 * Fx, Sg.Y + 0.6 * Fy, Fx, Fy, Mount);
				if (How == 3)
				{
					if (std::hypot(Sg.Sx, Sg.Sy) > 1e-9) ++NoneMoved;
				}
				else
				{
					WorstGunner = std::max(WorstGunner, In);
					if (In > 2e-3) ++GunnerIn;
					if (How != 2)
					{
						WorstMount = std::max(WorstMount, InMount);
						if (InMount > 2e-3) ++MountIn;
					}
				}
				for (int Side = 0; Side < (G.Left >= 0 ? 2 : 1); ++Side)
				{
					const State& Sc = Side == 0 ? S2 : St[G.LeftHand];
					const int Man = Side == 0 ? G.Two : G.Left;
					if (!Sc.bAlive || Sc.Speed >= 1e-6 || T - Sc.TurnedAt <= 0.5 || std::hypot(Sc.Qx - Sg.Qx, Sc.Qy - Sg.Qy) > Crowd::StoneTeamReach) continue;
					const double InC = DeepIn(Segs, Sc.X, Sc.Y, std::sin(static_cast<double>(Sc.F)), std::cos(static_cast<double>(Sc.F)), LandStone::BodyOf(0.0));
					const bool bAsOne = Cw.StoneAsOne(Man);
					if (bAsOne && std::hypot(Sc.Sx - Sg.Sx, Sc.Sy - Sg.Sy) > 1e-4) ++AsOneApart;
					if (!bAsOne || How != 3)
					{
						WorstCrew = std::max(WorstCrew, InC);
						if (InC > 2e-3) ++CrewIn;
					}
					if (Side == 0)
					{
						if (bAsOne)
						{
							++AsOneChecks;
							if (std::hypot(Sc.Sx, Sc.Sy) > 0.05) bAsOneStepped[Gi] = 1;
						}
						else
						{
							++OwnChecks;
							bOwnStep[Gi] = 1;
						}
					}
					else
					{
						// (the man at his left hand: where the gun came to him in the battle, there 25 s before her)
						LeftAsOne += bAsOne;
						LeftOwn += !bAsOne;
						if (G.From > 0.0)
						{
							++LeftCame;
							LeftCameOwn += !bAsOne;
						}
					}
				}
			}
			// (the third man and the crews, at rest half a second and more: each clear on his own step, but one held
			// off her ground, who stands where the rules have him; and none stepped on to her ground or before her
			// muzzle from a place off them)
			for (size_t H = 0; H < Hands.size(); ++H)
			{
				const Hand& Hd = Hands[H];
				State& S = St[H];
				// (the man who walks up to a gun that stands, every frame, on his way too: never drawn on her ground
				// or before her muzzle from a place off them)
				if (Hd.What == 5 && S.bAlive)
				{
					const State& Sh = St[3 * static_cast<size_t>(Hd.Gun)];
					LateWay += S.Speed > 0.1;
					if (Sh.bAlive && Sh.Speed < 1e-6)
					{
						const double Hx = std::sin(static_cast<double>(Sh.F)), Hy = std::cos(static_cast<double>(Sh.F));
						const double Px = Sh.X + 0.6 * Hx, Py = Sh.Y + 0.6 * Hy;
						const auto Her = [&](double X, double Y)
						{
							if (std::hypot(X - Sh.X, Y - Sh.Y) < Crowd::StoneTeamReach - 1e-6 || std::hypot(X - Px, Y - Py) < Crowd::StoneTeamReach - 1e-6) return true;
							const double Dx = X - Px, Dy = Y - Py, Far = std::hypot(Dx, Dy);
							return Far <= Crowd::StoneMuzzleFar && Far > 1e-6 && (Dx * Hx + Dy * Hy) / Far > Crowd::StoneMuzzleCos + 1e-6;
						};
						if (!Her(S.Qx, S.Qy) && Her(S.X, S.Y)) ++LateOnHers;
					}
				}
				if (Hd.What < 2 || Hd.What == 4 || !S.bAlive || S.PoseHeldFor < 0.5 || T - S.TurnedAt <= 0.5 || T < 3.5) continue;
				if (Cw.StoneKeptOff(Hd.Man))
				{
					LateKept += Hd.What == 5;
					++KeptChecks;
					S.bKept = true;
					if (std::hypot(S.Sx, S.Sy) > 1e-9) ++KeptMoved;
					continue;
				}
				if (Cw.StoneAsOne(Hd.Man)) S.bFarAsOne = true;
				++FarChecks;
				const double In = DeepIn(Segs, S.X, S.Y, std::sin(static_cast<double>(S.F)), std::cos(static_cast<double>(S.F)), LandStone::BodyOf(0.0));
				WorstFar = std::max(WorstFar, In);
				if (In > 2e-3) ++FarIn;
				const State& Sg = St[3 * static_cast<size_t>(Hd.Gun)];
				if (Sg.bAlive && Sg.Speed < 1e-6 && Sg.PoseHeldFor >= 0.5)
				{
					const double Fx = std::sin(static_cast<double>(Sg.F)), Fy = std::cos(static_cast<double>(Sg.F));
					const double Mx = Sg.X + 0.6 * Fx, My = Sg.Y + 0.6 * Fy;
					const auto Hers = [&](double X, double Y)
					{
						if (std::hypot(X - Sg.X, Y - Sg.Y) < Crowd::StoneTeamReach - 1e-6 || std::hypot(X - Mx, Y - My) < Crowd::StoneTeamReach - 1e-6) return true;
						const double Dx = X - Mx, Dy = Y - My, Far = std::hypot(Dx, Dy);
						return Far <= Crowd::StoneMuzzleFar && Far > 1e-6 && (Dx * Fx + Dy * Fy) / Far > Crowd::StoneMuzzleCos + 1e-6;
					};
					if (!Hers(S.Qx, S.Qy) && Hers(S.X, S.Y)) ++OnHers;
				}
			}
		}
		int Skaters = 0, KeptMen = 0, FarAsOne = 0, AsOneGuns = 0, OwnGuns = 0, WallGuns = 0, BeyondGuns = 0, LaneGuns = 0, SlotGuns = 0, FirstHow[4] = { 0, 0, 0, 0 };
		double LongestSkate = 0.0;
		for (const State& S : St)
		{
			Skaters += S.Skated > 0.0;
			LongestSkate = std::max(LongestSkate, S.Skated);
			KeptMen += S.bKept;
			FarAsOne += S.bFarAsOne;
		}
		for (size_t Gi = 0; Gi < Guns.size(); ++Gi)
		{
			AsOneGuns += bAsOneStepped[Gi];
			OwnGuns += bOwnStep[Gi];
			WallGuns += Guns[Gi].Kind == 0;
			BeyondGuns += Guns[Gi].Kind == 1;
			LaneGuns += Guns[Gi].Kind == 2;
			SlotGuns += Guns[Gi].Kind == 3;
			if (HowFirst[Gi] >= 0 && HowFirst[Gi] <= 3) ++FirstHow[HowFirst[Gi]];
		}
		std::printf("a gun's team in the crowd: %d guns, a wall through the gunner's rest for %d, beyond the third man for %d, a lane for %d, a slot for %d; %d men watched, %d frames to %.0f s\n",
			static_cast<int>(Guns.size()), WallGuns, BeyondGuns, LaneGuns, SlotGuns, static_cast<int>(Hands.size()), Frames, End);
		std::printf("her gunner at rest: %d checks; the walls' answer clear as they stood %d, one step for all %d, the men alone %d, none %d (by gun, as each first stood: %d, %d, %d, %d); %d in the stone (the nearest %.3f m clear), her mount %d times (%.3f m clear), %d moved where the answer was none\n",
			GunChecks, HowFrames[0], HowFrames[1], HowFrames[2], HowFrames[3], FirstHow[0], FirstHow[1], FirstHow[2], FirstHow[3], GunnerIn, -WorstGunner, MountIn, -WorstMount, NoneMoved);
		std::printf("his number two at her: %d checks at her gunner's step (%d guns where it was a step of 0.05 m or more), %d apart from it; %d checks on a step of his own (%d guns); %d in the stone (the nearest %.3f m clear)\n",
			AsOneChecks, AsOneGuns, AsOneApart, OwnChecks, OwnGuns, CrewIn, -WorstCrew);
		std::printf("the man at his left hand, at her: %d checks at her gunner's step, %d on a step of his own; of them where the gun came to him 25 s after he stood there: %d checks, %d on his own step\n",
			LeftAsOne, LeftOwn, LeftCame, LeftCameOwn);
		std::printf("the third man and the crews: %d checks on their own steps, %d in the stone (the nearest %.3f m clear); %d men held off her ground, %d checks, %d moved; %d times one stood on her ground from a place off it; %d of them at some time at her gunner's step\n",
			FarChecks, FarIn, -WorstFar, KeptMen, KeptChecks, KeptMoved, OnHers, FarAsOne);
		for (const Hand& Hd : Hands) LateMen += Hd.What == 5;
		std::printf("a man who walks up to a gun that stands, to a wall by her: %d men, %d frames on their way; held off her ground at rest in %d checks; %d frames drawn on her ground from a place off it, coming or standing\n",
			LateMen, LateWay, LateKept, LateOnHers);
		std::printf("no skating by a gun: %d frames of these men holding a rest pose a step off the rules' place; in %d of them the step moved, %d men, the longest %.3f m in all; %d more as a bound's step was finished; %d more with her gunner's step as he turned or went down\n",
			SkateChecks, SkateFrames, Skaters, LongestSkate, BoundFrames, WithFrames);
		std::printf("by a gun: the longest step %.2f m; %d jumps; %d frames where the step was not the whole difference from the plain crowd; the crowd told of %d things, %d frames not the same with the guns as without; over the cores %d frames not the same to the bit as one after another\n",
			Longest, Jumps, NotRules, EventsSeen, EventsOff, CoresOff);
		if (GunChecks < 2000) Fail("only %d checks of a gunner at rest: the gun's one step was not tried", GunChecks);
		if (HowOdd > 0) Fail("%d times a gunner at rest had no answer from the walls", HowOdd);
		if (HowFrames[1] < 500) Fail("only %d checks where one step cleared a gun's team: it was not tried", HowFrames[1]);
		if (HowFrames[2] < 500) Fail("only %d checks where a step cleared the men of a gun's team and not her mount: bodies before metal was not tried", HowFrames[2]);
		if (HowFrames[3] < 500) Fail("only %d checks where no step cleared the men of a gun's team: it was not tried", HowFrames[3]);
		if (LeftAsOne + LeftOwn < 2000 || LeftCame < 500) Fail("only %d checks of a man at his gunner's left hand, %d where the gun came to him: the man there first was not tried", LeftAsOne + LeftOwn, LeftCame);
		if (OwnChecks > 0) Fail("%d times a number two at her stood on a step of his own, at %d guns: here each gun's team has one step, whoever comes first", OwnChecks, OwnGuns);
		if (LeftOwn > LeftCameOwn) Fail("%d times a man at his gunner's left hand from the battle's start stood on a step of his own", LeftOwn - LeftCameOwn);
		if (GunnerIn > 0) Fail("%d times a gunner at rest was drawn in the stone (the worst %.3f m inside his need)", GunnerIn, WorstGunner);
		if (MountIn > 0) Fail("%d times her mount stood in the stone where the walls' answer cleared her (the worst %.3f m)", MountIn, WorstMount);
		if (NoneMoved > 0) Fail("%d times a gunner was stepped where no step cleared his team's men", NoneMoved);
		if (AsOneChecks < 2000 || AsOneGuns < 3) Fail("only %d checks of a number two at his gunner's step, at %d guns stepped: one thing was not tried", AsOneChecks, AsOneGuns);
		if (AsOneApart > 0) Fail("%d times a number two said to stand at his gunner's step stood apart from it", AsOneApart);
		if (CrewIn > 0) Fail("%d times a number two at her was drawn in the stone (the worst %.3f m inside his need)", CrewIn, WorstCrew);
		if (FarIn > 0) Fail("%d times a man of a gun's crew on his own step was drawn in the stone (the worst %.3f m)", FarIn, WorstFar);
		if (KeptMen < 1) Fail("no man was held off her ground: the wall beyond the third man was not tried");
		if (KeptMoved > 0) Fail("%d times a man held off her ground did not stand where the rules have him", KeptMoved);
		if (LateMen < 1 || LateWay < 60 || LateKept < 1) Fail("no man walked up to a gun that stands and was held off her ground (%d men, %d frames on their way, %d checks held off): the coming in to her was not tried", LateMen, LateWay, LateKept);
		if (LateOnHers > 0) Fail("%d frames a man who walked up to a gun that stands was drawn on her ground from a place off it", LateOnHers);
		if (OnHers > 0) Fail("%d times a man of her crew was stepped on to her ground or before her muzzle", OnHers);
		if (SkateFrames > 0) Fail("%d times the step of a gun's man holding a rest pose moved: %d men slid where they stood, the longest %.3f m", SkateFrames, Skaters, LongestSkate);
		if (Jumps > 0) Fail("%d times a gun's man's step moved faster than a walk", Jumps);
		if (NotRules > 0) Fail("%d times a gun's man's step was not the whole difference from the plain crowd", NotRules);
		if (EventsOff > 0) Fail("%d frames where the crowd told of other things with the guns than without", EventsOff);
		if (CoresOff > 0) Fail("%d frames where the guns' men over the cores were not the men one after another", CoresOff);
	}
}

int main(int Argc, char** Argv)
{
	WallsOwn();
	TeamsOwn();
	if (Argc < 4)
	{
		std::printf("usage: land_stone_test <recording.json> <soldiers folder> <armies.json>\n");
		return 1;
	}
	const std::string RecText = ReadFile(Argv[1]);
	LandJson::Value Rec;
	std::string Err;
	if (RecText.empty() || !LandJson::Parse(RecText, Rec)) { std::printf("cannot read %s\n", Argv[1]); return 1; }
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	for (const char* Army : { "GER", "POL", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader", "USA_radio", "SOV" })
	{
		const std::string Path = std::string(Argv[2]) + "/soldier_" + Army + ".json";
		const std::string Text = ReadFile(Path);
		if (Text.empty() && (std::string(Army).rfind("USA", 0) == 0 || std::string(Army) == "SOV")) continue;
		auto S = std::make_shared<Soldier>();
		if (Text.empty() || !S->Load(Text, Err)) { std::printf("cannot read %s: %s\n", Path.c_str(), Err.c_str()); return 1; }
		Files[Army] = S;
	}
	const GroundFn Flat = [](double, double) { return Ground(); };
	const double Life = 2.0, Half = 0.30;

	// The plain crowd first: who dies, and when.
	Crowd W, P;
	for (Crowd* C : { &W, &P })
	{
		C->SetLife(Life);
		for (const auto& KV : Files) C->AddSoldier(KV.first, KV.second);
		if (!C->ReadArmies(ReadFile(Argv[3]), Err)) { std::printf("cannot read %s: %s\n", Argv[3], Err.c_str()); return 1; }
	}
	if (!P.Build(RecText, Flat, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
	const int N = P.Count();

	// The men who rest: from the recording's own tracks, each man's first stay of 6 s or more at one place in the
	// battle's first 140 s (his kept points there may be many: he turns where he stands), and whether he starts the
	// battle there or dies there. A wall through where each of up to 60 of them rests, 0.10 m off his place (he is in
	// its stone), 6 m long; no two walls within 12 m. Every other man's wall lies across the way the recording has
	// him face as he comes to rest, its middle before him (he faces through it from inside it: the far face is tried
	// in every battle); the others' at one of four headings by his number. Those who start the battle at rest and
	// those who die at rest are taken first, up to 12 of each.
	struct Rest
	{
		int Block = 0, K = 0, J = 0, Man = -1;
		double X = 0.0, Y = 0.0, From = 0.0, To = 0.0, Dead = -1.0;
		bool bFirst = false;      // he starts the battle there
		bool bDies = false;       // he dies there
		Seg Wall{};
	};
	std::vector<Rest> All, Rests;
	const LandJson::Value& Men = Rec["men"];
	for (size_t I = 0; I < Men.Count(); ++I)
	{
		const LandJson::Value& M = Men.At(I);
		const LandJson::Value& At = M["at"];
		const int Man = P.Find(M["b"].Int(0), M["k"].Int(0), M["j"].Int(0));
		if (Man < 0 || P.IsRider(Man)) continue;
		for (size_t A = 0; A + 1 < At.Count();)
		{
			size_t B = A;
			while (B + 1 < At.Count() && std::hypot(At.At(B + 1).At(1).Num() - At.At(A).At(1).Num(), At.At(B + 1).At(2).Num() - At.At(A).At(2).Num()) < 1e-9) ++B;
			const double T0 = At.At(A).At(0).Num(), T1 = At.At(B).At(0).Num();
			if (T1 - T0 >= 6.0 && T0 < 140.0)
			{
				Rest R;
				R.Block = M["b"].Int(0);
				R.K = M["k"].Int(0);
				R.J = M["j"].Int(0);
				R.Man = Man;
				R.X = At.At(A).At(1).Num();
				R.Y = At.At(A).At(2).Num();
				R.From = T0;
				R.To = std::min(T1, 147.0);
				R.bFirst = A == 0 && T0 <= 0.0;
				R.Dead = P.Info(Man).Dead;
				R.bDies = R.Dead >= R.From + 1.5 && R.Dead <= R.To;
				const bool bBefore = I % 2 == 0 && At.At(A).Count() > 3;
				// (the way he faces once he has turned to his front there: StoneSettle into his stay, as the crowd reads it)
				size_t Fc = A;
				while (Fc < B && At.At(Fc + 1).At(0).Num() <= T0 + Crowd::StoneSettle) ++Fc;
				double Faced = At.At(Fc).Count() > 3 ? At.At(Fc).At(3).Num() : 0.0;
				if (Fc < B && At.At(Fc + 1).Count() > 3)
				{
					double Turn = At.At(Fc + 1).At(3).Num() - Faced;
					while (Turn > 3.14159265358979) Turn -= 6.28318530717959;
					while (Turn < -3.14159265358979) Turn += 6.28318530717959;
					const double Between = At.At(Fc + 1).At(0).Num() - At.At(Fc).At(0).Num();
					Faced += Turn * (Between > 1e-9 ? std::clamp((T0 + Crowd::StoneSettle - At.At(Fc).At(0).Num()) / Between, 0.0, 1.0) : 0.0);
				}
				const double Way = bBefore ? Faced - 1.5707963 : 0.7854 * static_cast<double>((I / 2) % 4);
				const double Wx = std::sin(Way), Wy = std::cos(Way);
				const double Ox = R.X + 0.10 * Wy, Oy = R.Y - 0.10 * Wx;
				R.Wall = { Ox - 3.0 * Wx, Oy - 3.0 * Wy, Ox + 3.0 * Wx, Oy + 3.0 * Wy, Half };
				All.push_back(R);
				break;
			}
			A = B + 1;
		}
	}
	for (int Pass = 0; Pass < 3; ++Pass)
	{
		int Taken = 0;
		for (const Rest& R : All)
		{
			if (Rests.size() >= 60 || (Pass < 2 && Taken >= 12)) break;
			if (Pass == 0 ? !R.bFirst : Pass == 1 ? !R.bDies : false) continue;
			bool bNear = false;
			for (const Rest& O : Rests) bNear = bNear || std::hypot(O.X - R.X, O.Y - R.Y) < 12.0;
			if (bNear) continue;
			Rests.push_back(R);
			++Taken;
		}
	}
	if (Rests.size() < 20) { std::printf("only %zu men rest 6 s in %s: nothing to try\n", Rests.size(), Argv[1]); return 1; }
	auto Stone = std::make_shared<LandStone::Walls>();
	std::vector<Seg> Segs;
	for (const Rest& R : Rests)
	{
		Stone->Add(R.Wall.Ax, R.Wall.Ay, R.Wall.Bx, R.Wall.By, R.Wall.Half);
		Segs.push_back(R.Wall);
	}
	// What the crowd asks of the walls for each of these men's resting places, as he comes in to it and as he stands
	// there. Once he stands there: the side of his wall the first answer of his stay put him on (there he is to be
	// drawn while he stays), how often a later answer of the stay put him on the other, and whether the place stood at
	// was where the answer before it had put him.
	struct Asked
	{
		double X = 0.0, Y = 0.0, Ax = 0.0, Ay = 0.0, Lx = 0.0, Ly = 0.0;
		int StaySide = 0, SideFlips = 0, Fresh = 0, HintOff = 0, Frame = -1000;
		bool bHas = false, bThere = false;
		double AnsX = 0.0, AnsY = 0.0;
	};
	auto Asks = std::make_shared<std::vector<Asked>>(Rests.size());
	auto AsksLock = std::make_shared<std::mutex>();
	// (the frame being played: a place stood at is held to the last answer only where that was asked the frame before)
	auto FrameNow = std::make_shared<std::atomic<int>>(0);
	for (size_t K = 0; K < Rests.size(); ++K)
	{
		const Seg& Wl = Rests[K].Wall;
		const double Len = std::hypot(Wl.Bx - Wl.Ax, Wl.By - Wl.Ay);
		Asked& A = (*Asks)[K];
		A.X = Rests[K].X;
		A.Y = Rests[K].Y;
		A.Ax = Wl.Ax;
		A.Ay = Wl.Ay;
		A.Lx = -(Wl.By - Wl.Ay) / Len;
		A.Ly = (Wl.Bx - Wl.Ax) / Len;
	}
	W.SetStone([Stone, Asks, AsksLock, FrameNow](double X, double Y, float Facing, float Lie, bool bStood, double StoodX, double StoodY, double& OutDx, double& OutDy)
	{
		const double Heading = static_cast<double>(Facing);
		const bool bStep = Stone->Clear(X, Y, std::sin(Heading), std::cos(Heading), LandStone::BodyOf(static_cast<double>(Lie)), bStood, StoodX, StoodY, OutDx, OutDy);
		for (Asked& A : *Asks)
		{
			if (std::fabs(X - A.X) > 1e-4 || std::fabs(Y - A.Y) > 1e-4) continue;
			const std::lock_guard<std::mutex> Hold(*AsksLock);
			const int Frame = FrameNow->load();
			const int SideNow = !bStep ? 0 : (X + OutDx - A.Ax) * A.Lx + (Y + OutDy - A.Ay) * A.Ly > 0.0 ? 1 : -1;
			if (!A.bThere) A.StaySide = 0;
			else if (A.StaySide == 0) A.StaySide = SideNow;
			else if (SideNow != 0 && SideNow != A.StaySide) ++A.SideFlips;
			if (bStood && A.bThere && A.bHas && A.Frame == Frame - 1 && std::hypot(StoodX - A.AnsX, StoodY - A.AnsY) > 1e-3) ++A.HintOff;
			A.Frame = Frame;
			if (!bStood) ++A.Fresh;
			A.bHas = bStep;
			A.AnsX = X + OutDx;
			A.AnsY = Y + OutDy;
		}
		return bStep;
	});
	if (!W.Build(RecText, Flat, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
	if (W.Count() != N) { std::printf("the two crowds are not the same men\n"); return 1; }
	double End = 0.0;
	int Found = 0, Starts = 0, Dying = 0;
	for (const Rest& R : Rests)
	{
		++Found;
		Starts += R.bFirst;
		Dying += R.bDies;
		End = std::max(End, R.To + 3.0);
	}
	End = std::min(End, 150.0);

	// Which men are nowhere near a wall all battle long (by the plain crowd): they must be the same in both.
	std::vector<char> NearWall(static_cast<size_t>(N), 0);
	const double Dt = 1.0 / 60.0;
	const int Frames = static_cast<int>(std::ceil(End / Dt)) + 1;
	std::vector<double> LastDx(static_cast<size_t>(N), 0.0), LastDy(static_cast<size_t>(N), 0.0);
	// (on the move, by the plain crowd: a man who has gone a metre a second or more for a second and a half is where
	// the rules have him, whatever wall he passes: at a walk he has given back two metres of step by then)
	std::vector<double> PlainX(static_cast<size_t>(N), 0.0), PlainY(static_cast<size_t>(N), 0.0), MovingFor(static_cast<size_t>(N), 0.0);
	// (a man on the move is stepped only as he comes in to a rest his track shows ahead: no longer at a stretch than
	// the crowd looks ahead)
	std::vector<double> SteppedSince(static_cast<size_t>(N), -1.0);
	int ComingIn = 0;
	int MovingChecks = 0, MovingByWall = 0, MovingStepped = 0;
	int AtRestChecks = 0, InStone = 0, Jumps = 0, NotRules = 0, OnTheMove = 0, MovedOff = 0, FellChecks = 0, FellMoved = 0, FirstClear = 0, FirstAsked = 0;
	std::vector<char> bCrossed(Rests.size(), 0), bFacedIt(Rests.size(), 0);
	int SideChecks = 0, WrongSide = 0;
	// (a man who turns, or goes down, takes his new step no faster than he walks: for half a second after either his
	// reach may be a little in the stone, counted apart and held to a hand's breadth)
	std::vector<double> TurnedAt(Rests.size(), -1e9), StepWas(Rests.size(), 0.0);
	std::vector<float> FacedWas(Rests.size(), 0.f);
	int TurningChecks = 0, TurningIn = 0, PivotChecks = 0;
	double WorstTurning = -1e9, WorstPivot = -1e9;
	// (as he pivots where he stands he is asked for as he will face when the pivot ends, within StoneSettle: his
	// muzzle may sweep the stone on the way round, counted apart)
	std::vector<char> bPivots(Rests.size(), 0);
	double WorstIn = -1e9, LongestStep = 0.0, FastestStep = 0.0;
	std::vector<double> FellX(Rests.size(), 0.0), FellY(Rests.size(), 0.0), ClearFor(Rests.size(), 0.0), ClearX(Rests.size(), 0.0), ClearY(Rests.size(), 0.0);
	// (the arrival: a man who comes in to his rest stands, as he stops, at the place the walls gave for it as he came:
	// how far short of it he is drawn at that frame. One who had less time on his way than a walk takes to make the
	// step, a bound of a few tenths of a second from where he lay, may be short by what a walk could not cover.)
	std::vector<double> ArrivalGap(Rests.size(), -1.0), ArrivalStep(Rests.size(), 0.0), ArrivalNotice(Rests.size(), 0.0);
	std::vector<double> RestPlainX(Rests.size(), 0.0), RestPlainY(Rests.size(), 0.0), StillSince(Rests.size(), 0.0), LeftAt(Rests.size(), 0.0);
	std::vector<char> bStopped(Rests.size(), 0), bWasThere(Rests.size(), 0);
	int Arrivals = 0, ArrivedShort = 0, Bounds = 0;
	double WorstArrival = 0.0, WorstBound = 0.0;
	// (no skating: while a man holds a rest pose, a quarter of a second and more at his place without turning or
	// going down or getting up, his step does not move)
	// (and goes on holding it the two frames after: the walls are asked for a man told to go down or to turn in the
	// frame he is told, the frame before his pose first shows it)
	std::vector<double> PoseHeldFor(static_cast<size_t>(N), 0.0), Skated(static_cast<size_t>(N), 0.0);
	std::vector<double> SkateWaits(static_cast<size_t>(N) * 2, 0.0);
	// (the one step that may still be moving then: a bound's. A man who dashes a few tenths of a second from one place
	// by a wall to another has not the time on his way for a walk to make the change, and finishes it where he stops.
	// Told from a slide by this: his step has moved at a walk the whole time since he set off, less the moment the
	// crowd waits to know he has stopped. Counted apart.)
	std::vector<double> SetOffAt(static_cast<size_t>(N), -1e9), StepGone(static_cast<size_t>(N), 0.0), BoundSkated(static_cast<size_t>(N), 0.0);
	std::vector<char> bWasStill(static_cast<size_t>(N), 1);
	int BoundFrames = 0;
	std::vector<float> PoseF(static_cast<size_t>(N), 0.f), PoseLie(static_cast<size_t>(N), 0.f);
	int SkateFrames = 0, SkateChecks = 0;
	// (drawn off a place the rules have had clear a second: only as he comes in to a rest by a wall, which he reaches
	// within the time the crowd looks ahead; held here until that is seen, or not)
	std::vector<std::vector<double>> OffClear(Rests.size());
	// (and only once he has been on his way as long as a walk takes to give back the step he set off with)
	std::vector<double> WayFor(Rests.size(), 0.0), LeftWith(Rests.size(), 0.0);
	std::vector<char> bFellKept(Rests.size(), 0);
	struct Snap { int Man; int Frame; double X, Y, Z; float F; };
	std::vector<Snap> SnapW, SnapP;
	const double Pace = Crowd::StoneSpeed * Dt;
	for (int Fr = 0; Fr < Frames; ++Fr)
	{
		const double T = static_cast<double>(Fr) * Dt;
		FrameNow->store(Fr);
		W.Update(T);
		P.Update(T);
		for (int Man = 0; Man < N; ++Man)
		{
			double Sx = 0.0, Sy = 0.0;
			W.StoneStep(Man, Sx, Sy);
			const double Moved = std::hypot(Sx - LastDx[static_cast<size_t>(Man)], Sy - LastDy[static_cast<size_t>(Man)]);
			if (Fr > 0)
			{
				FastestStep = std::max(FastestStep, Moved);
				if (Moved > Pace + 1e-9) ++Jumps;
			}
			LastDx[static_cast<size_t>(Man)] = Sx;
			LastDy[static_cast<size_t>(Man)] = Sy;
			LongestStep = std::max(LongestStep, std::hypot(Sx, Sy));
			if (Sx != 0.0 || Sy != 0.0) NearWall[static_cast<size_t>(Man)] = 1;
			const double MovedNow = Fr > 0 ? Moved : 0.0;
			// (the step is the whole difference from the plain crowd, while both are alive and on the field)
			double Wx = 0.0, Wy = 0.0, Wz = 0.0, Px = 0.0, Py = 0.0, Pz = 0.0;
			float Wf = 0.f, Pf = 0.f;
			W.PlaceOf(Man, Wx, Wy, Wz, Wf);
			P.PlaceOf(Man, Px, Py, Pz, Pf);
			const double DeadAt = P.Info(Man).Dead;
			if ((DeadAt < 0.0 || T < DeadAt) && W.Visible(Man) && P.Visible(Man) && std::hypot(Wx - Px - Sx, Wy - Py - Sy) > 1e-6) ++NotRules;
			{
				const size_t M = static_cast<size_t>(Man);
				const double Speed = Fr > 0 ? std::hypot(Px - PlainX[M], Py - PlainY[M]) / Dt : 0.0;
				const float LieNow = W.LieOf(Man);
				const bool bPose = Fr > 0 && Speed < 1e-6 && std::fabs(Pf - PoseF[M]) < 1e-4f && std::fabs(LieNow - PoseLie[M]) < 1e-4f
					&& (DeadAt < 0.0 || T < DeadAt) && W.Visible(Man);
				PoseHeldFor[M] = bPose ? PoseHeldFor[M] + Dt : 0.0;
				PoseF[M] = Pf;
				PoseLie[M] = LieNow;
				{
					const bool bStill = Speed < static_cast<double>(Crowd::StoneStill);
					if (!bStill && bWasStill[M])
					{
						SetOffAt[M] = T - Dt;
						StepGone[M] = 0.0;
					}
					bWasStill[M] = bStill;
					StepGone[M] += MovedNow;
				}
				if (!bPose) SkateWaits[2 * M] = SkateWaits[2 * M + 1] = 0.0;
				if (PoseHeldFor[M] >= 0.25 && (Sx != 0.0 || Sy != 0.0)) ++SkateChecks;
				if (SkateWaits[2 * M] > 0.0)
				{
					++SkateFrames;
					Skated[M] += SkateWaits[2 * M];
				}
				else if (SkateWaits[2 * M] < 0.0)
				{
					++BoundFrames;
					BoundSkated[M] -= SkateWaits[2 * M];
				}
				SkateWaits[2 * M] = SkateWaits[2 * M + 1];
				const bool bBound = StepGone[M] >= Crowd::StoneSpeed * (T - SetOffAt[M] - Crowd::StoneDwell - 0.05);
				SkateWaits[2 * M + 1] = PoseHeldFor[M] >= 0.25 && MovedNow > 5e-4 ? (bBound ? -MovedNow : MovedNow) : 0.0;
				MovingFor[M] = Speed >= 1.0 && (DeadAt < 0.0 || T < DeadAt) ? MovingFor[M] + Dt : 0.0;
				PlainX[M] = Px;
				PlainY[M] = Py;
				if (MovingFor[M] >= 1.5)
				{
					++MovingChecks;
					if (DeepIn(Segs, Px, Py, std::sin(static_cast<double>(Pf)), std::cos(static_cast<double>(Pf)), LandStone::BodyOf(0.0)) > 0.0) ++MovingByWall;
					if (Sx != 0.0 || Sy != 0.0)
					{
						++ComingIn;
						if (SteppedSince[M] < 0.0) SteppedSince[M] = T;
						if (T - SteppedSince[M] > Crowd::StoneAhead + 0.1) ++MovingStepped;
					}
					else SteppedSince[M] = -1.0;
				}
				else SteppedSince[M] = -1.0;
			}
			if (Fr % 30 == 0 && !NearWall[static_cast<size_t>(Man)])
			{
				SnapW.push_back({ Man, Fr, Wx, Wy, Wz, Wf });
				SnapP.push_back({ Man, Fr, Px, Py, Pz, Pf });
			}
		}
		for (size_t K = 0; K < Rests.size(); ++K)
		{
			const Rest& R = Rests[K];
			if (R.Man < 0) continue;
			double X = 0.0, Y = 0.0, Z = 0.0, Sx = 0.0, Sy = 0.0;
			float F = 0.f;
			W.PlaceOf(R.Man, X, Y, Z, F);
			W.StoneStep(R.Man, Sx, Sy);
			const bool bAlive = R.Dead < 0.0 || T < R.Dead;
			{
				const double StepNow = std::hypot(Sx, Sy);
				float Turned = F - FacedWas[K];
				while (Turned > 3.14159265f) Turned -= 6.2831853f;
				while (Turned < -3.14159265f) Turned += 6.2831853f;
				if (Fr > 0 && (std::fabs(Turned) > 0.002f || std::fabs(StepNow - StepWas[K]) > 1e-6)) TurnedAt[K] = T;
				bPivots[K] = Fr > 0 && std::fabs(Turned) > 0.002f;
				FacedWas[K] = F;
				StepWas[K] = StepNow;
			}
			// (the arrival: the frame he reaches his resting place, against the walls' answer for it as he came in; and
			// how long he had been on his way since he last stayed anywhere as long as a rest)
			{
				double Px = 0.0, Py = 0.0, Pz = 0.0;
				float Pf = 0.f;
				P.PlaceOf(R.Man, Px, Py, Pz, Pf);
				const double Speed = Fr > 0 ? std::hypot(Px - RestPlainX[K], Py - RestPlainY[K]) / Dt : 0.0;
				RestPlainX[K] = Px;
				RestPlainY[K] = Py;
				// (told to the walls' asking: he stood at his resting place this frame and the one before)
				{
					const bool bAtIt = std::hypot(Px - R.X, Py - R.Y) < 1e-6;
					const std::lock_guard<std::mutex> Hold(*AsksLock);
					(*Asks)[K].bThere = bAtIt && bWasThere[K];
					bWasThere[K] = bAtIt;
				}
				if (Speed >= static_cast<double>(Crowd::StoneStill))
				{
					if (T - StillSince[K] >= Crowd::StoneStay + Dt) LeftAt[K] = T - Dt;
					StillSince[K] = T;
				}
				if (!bStopped[K] && T >= R.From - 0.01 && std::hypot(Px - R.X, Py - R.Y) < 1e-6)
				{
					bStopped[K] = 1;
					const Asked& A = (*Asks)[K];
					if (bAlive && !R.bFirst && Speed > 0.0 && A.bHas && A.Frame >= Fr - 2)
					{
						ArrivalStep[K] = std::hypot(A.AnsX - R.X, A.AnsY - R.Y);
						ArrivalGap[K] = std::hypot(R.X + Sx - A.AnsX, R.Y + Sy - A.AnsY);
						ArrivalNotice[K] = T - LeftAt[K];
					}
				}
			}
			// (the battle's first frame: a man who starts in a wall starts clear of it)
			if (Fr == 0 && R.bFirst)
			{
				++FirstAsked;
				if (DeepIn(Segs, X, Y, std::sin(static_cast<double>(F)), std::cos(static_cast<double>(F)), LandStone::BodyOf(0.0)) <= 2e-3) ++FirstClear;
			}
			// (at rest: from 1.5 s after he stops, time for the step at a walk, to the rest's end)
			if (bAlive && T >= R.From + 1.5 && T <= R.To - 0.05)
			{
				// (clear by a standing man's reach at least, his muzzle, his heels and his shoulders: a lying man's is more)
				const double In = DeepIn(Segs, X, Y, std::sin(static_cast<double>(F)), std::cos(static_cast<double>(F)), LandStone::BodyOf(0.0));
				const Seg& Wl = R.Wall;
				const double Len = std::hypot(Wl.Bx - Wl.Ax, Wl.By - Wl.Ay), Lx = -(Wl.By - Wl.Ay) / Len, Ly = (Wl.Bx - Wl.Ax) / Len;
				const double Drawn = (X - Wl.Ax) * Lx + (Y - Wl.Ay) * Ly, Ruled = (R.X - Wl.Ax) * Lx + (R.Y - Wl.Ay) * Ly;
				if (T - TurnedAt[K] > 0.5 / Life * 2.0)
				{
					++AtRestChecks;
					WorstIn = std::max(WorstIn, In);
					if (In > 2e-3) ++InStone;
					// (the side the walls' first answer of this stay put him on: there he is drawn)
					const int Side = (*Asks)[K].StaySide;
					if (Side != 0)
					{
						++SideChecks;
						if ((Drawn > 0.0 ? 1 : -1) != Side) ++WrongSide;
					}
				}
				else if (bPivots[K])
				{
					++PivotChecks;
					WorstPivot = std::max(WorstPivot, In);
				}
				else
				{
					++TurningChecks;
					WorstTurning = std::max(WorstTurning, In);
					if (In > 0.10) ++TurningIn;
				}
				// (which side of his wall he stands on, against the rules' place's; and whether the wall is before
				// his muzzle: squarely faced from within two metres)
				if ((Drawn > 0.0) != (Ruled > 0.0)) bCrossed[K] = 1;
				const double Toward = -(std::sin(static_cast<double>(F)) * Lx + std::cos(static_cast<double>(F)) * Ly) * (Drawn > 0.0 ? 1.0 : -1.0);
				if (Toward > 0.9 && std::fabs(Drawn) < 2.0) bFacedIt[K] = 1;
			}
			// (wherever the rules' place has been clear of every wall a second, by a lying man's reach and a hand's
			// breadth, he is drawn where the rules have him: on his way again after his rest, and before it)
			{
				double Px = 0.0, Py = 0.0, Pz = 0.0;
				float Pf = 0.f;
				P.PlaceOf(R.Man, Px, Py, Pz, Pf);
				const bool bClear = DeepIn(Segs, Px, Py, std::sin(static_cast<double>(Pf)), std::cos(static_cast<double>(Pf)), LandStone::BodyOf(1.0)) <= -0.05;
				ClearFor[K] = bClear ? ClearFor[K] + Dt : 0.0;
				// (on his way: a man at rest stays where he stands while that is clear, and gives the step back at a walk
				// as he goes)
				const double PlainSpeed = Fr > 0 ? std::hypot(Px - ClearX[K], Py - ClearY[K]) / Dt : 0.0;
				ClearX[K] = Px;
				ClearY[K] = Py;
				if (PlainSpeed >= static_cast<double>(Crowd::StoneStill)) WayFor[K] += Dt;
				else
				{
					WayFor[K] = 0.0;
					LeftWith[K] = std::hypot(Sx, Sy);
				}
				if (bAlive && ClearFor[K] >= 1.0 && WayFor[K] >= 0.1 + LeftWith[K] / Crowd::StoneSpeed)
				{
					++OnTheMove;
					if (Sx != 0.0 || Sy != 0.0) OffClear[K].push_back(T);
				}
				// (he rests by a wall now: what he was stepped as he came in, the crowd's look ahead back, was his coming in)
				if (!bAlive) OffClear[K].clear();
				else if (!bClear && PlainSpeed < static_cast<double>(Crowd::StoneStill))
				{
					while (!OffClear[K].empty() && OffClear[K].back() >= T - Crowd::StoneAhead - 0.2) OffClear[K].pop_back();
				}
				while (!OffClear[K].empty() && OffClear[K].front() < T - Crowd::StoneAhead - 0.3)
				{
					++MovedOff;
					OffClear[K].erase(OffClear[K].begin());
				}
			}
			// (a man who falls at his rest lies where he was drawn: his frame on the field, the ground flat here, and
			// the place the crowd gives for him)
			if (R.Dead >= R.From + 1.5 && R.Dead <= R.To)
			{
				const Xform& Fw = W.WorldFrame(R.Man);
				if (T < R.Dead) { FellX[K] = X; FellY[K] = Y; bFellKept[K] = 1; }
				else if (bFellKept[K] && T >= R.Dead + 1.0)
				{
					++FellChecks;
					if (std::hypot(X - FellX[K], Y - FellY[K]) > 0.02 || std::hypot(static_cast<double>(Fw.T[0]) - FellX[K], static_cast<double>(Fw.T[1]) - FellY[K]) > 0.02) ++FellMoved;
				}
			}
		}
	}
	int Same = 0, Differ = 0;
	for (size_t K = 0; K < SnapW.size(); ++K)
	{
		if (NearWall[static_cast<size_t>(SnapW[K].Man)]) continue;
		const bool bSame = SnapW[K].X == SnapP[K].X && SnapW[K].Y == SnapP[K].Y && SnapW[K].Z == SnapP[K].Z && SnapW[K].F == SnapP[K].F;
		if (bSame) ++Same; else ++Differ;
	}
	int Never = 0;
	for (int Man = 0; Man < N; ++Man) Never += !NearWall[static_cast<size_t>(Man)];
	for (size_t K = 0; K < Rests.size(); ++K)
	{
		if (ArrivalGap[K] < 0.0) continue;
		++Arrivals;
		// (what a walk could not cover in the time he was on his way)
		const double Bound = std::max(0.0, ArrivalStep[K] - Crowd::StoneSpeed * std::max(0.0, ArrivalNotice[K] - 0.05));
		if (Bound > 0.0)
		{
			++Bounds;
			WorstBound = std::max(WorstBound, ArrivalGap[K]);
		}
		else WorstArrival = std::max(WorstArrival, ArrivalGap[K]);
		if (ArrivalGap[K] > 0.03 + Bound) ++ArrivedShort;
	}
	int Skaters = 0, Bounders = 0;
	double LongestSkate = 0.0, LongestBound = 0.0;
	for (int Man = 0; Man < N; ++Man)
	{
		Skaters += Skated[static_cast<size_t>(Man)] > 0.0;
		LongestSkate = std::max(LongestSkate, Skated[static_cast<size_t>(Man)]);
		Bounders += BoundSkated[static_cast<size_t>(Man)] > 0.0;
		LongestBound = std::max(LongestBound, BoundSkated[static_cast<size_t>(Man)]);
	}
	int Crossed = 0, CrossedLater = 0, FacedIt = 0, AskedAfresh = 0, HintOff = 0, SideFlips = 0;
	for (size_t K = 0; K < Rests.size(); ++K)
	{
		Crossed += bCrossed[K];
		CrossedLater += bCrossed[K] && !Rests[K].bFirst;
		FacedIt += bFacedIt[K];
		AskedAfresh += (*Asks)[K].Fresh > 0;
		HintOff += (*Asks)[K].HintOff;
		SideFlips += (*Asks)[K].SideFlips;
	}
	std::printf("%s: %d men, %d frames to %.0f s; %d walls laid through resting men's places (%d of them start the battle there, %d die there)\n", Argv[1], N, Frames, End, Found, Starts, Dying);
	std::printf("sides: %d of the %d stepped through their wall to have it at their back (%d of them as they came to rest in the battle); %d at some time faced their wall squarely from their own side\n", Crossed, Found, CrossedLater, FacedIt);
	std::printf("the side kept: %d of the %d asked for afresh at their resting place; %d checks of the side they are drawn on, %d on the other; %d answers on the other side from the first of their stay; %d times the place stood at was not where the last answer put him\n",
		AskedAfresh, Found, SideChecks, WrongSide, SideFlips, HintOff);
	std::printf("at rest: %d checks, %d in the stone, the nearest %.3f m clear of his need; the longest step %.2f m; the fastest %.4f m a frame (a walk is %.4f); %d jumps\n",
		AtRestChecks, InStone, -WorstIn, LongestStep, FastestStep, Pace, Jumps);
	std::printf("turning or going down at rest: %d checks in the second after, the furthest in the stone %.3f m, %d over a hand's breadth; %d as he pivots, his reach as it sweeps round the furthest in the stone %.3f m\n",
		TurningChecks, std::max(0.0, WorstTurning), TurningIn, PivotChecks, std::max(0.0, WorstPivot));
	std::printf("on the move a second and a half or more: %d checks, %d of them within a wall's reach, %d as he came in to a rest, %d stepped off the rules' place for longer than that\n", MovingChecks, MovingByWall, ComingIn, MovingStepped);
	std::printf("the arrival: %d men came in to a rest in a wall; as they stopped the furthest from the place the walls gave as they came %.3f m; %d more made a bound to it too short for a walk to take the step (the furthest short %.3f m); %d not there by more than that\n",
		Arrivals - Bounds, WorstArrival, Bounds, WorstBound, ArrivedShort);
	std::printf("no skating: %d frames of a man holding a rest pose a step off the rules' place; in %d of them his step moved, %d men, the longest %.3f m in all; %d more frames as a man finished the step of a bound too short for a walk to make it, %d men, the longest %.3f m in all\n",
		SkateChecks, SkateFrames, Skaters, LongestSkate, BoundFrames, Bounders, LongestBound);
	std::printf("first frame: %d of %d men who start in a wall start clear; the rules' place clear of every wall a second: %d checks, %d still stepped; fallen at rest: %d checks, %d moved\n",
		FirstClear, FirstAsked, OnTheMove, MovedOff, FellChecks, FellMoved);
	std::printf("the step is the whole difference from the plain crowd: %d frames of a man where it was not; men never near a wall: %d, their %d readings the same to the bit, %d not\n",
		NotRules, Never, Same, Differ);
	if (Crossed < 3) Fail("only %d men stepped through their wall: the far side was not tried", Crossed);
	if (CrossedLater < 1) Fail("no man who came to rest in a wall in the battle stepped through it: only those who start there did");
	if (SideChecks < 2000) Fail("only %d checks of the side a man is drawn on: the side was not tried", SideChecks);
	if (WrongSide > 0) Fail("%d times a man at rest was drawn on the other side of his wall from where the walls' answer put him", WrongSide);
	if (SideFlips > 0) Fail("%d times the walls' answer for a man's resting place changed sides while he stood there", SideFlips);
	if (HintOff > 0) Fail("%d times the place a man was said to stand at was not where the last answer put him", HintOff);
	if (Starts < 3) Fail("only %d men start the battle in a wall: the first frame was not tried", Starts);
	if (AtRestChecks < 2000) Fail("only %d checks of a man at rest in a wall: the walls were not tried", AtRestChecks);
	if (InStone > 0) Fail("%d times a man at rest was drawn in the stone (the worst %.3f m inside his need)", InStone, WorstIn);
	if (TurningIn > 0) Fail("%d times a man turning at rest reached more than a hand's breadth into the stone (the worst %.3f m)", TurningIn, WorstTurning);
	if (MovingStepped > 0) Fail("%d times a man on the move was drawn off the rules' place for longer than a coming in to rest", MovingStepped);
	if (Arrivals - Bounds < 5) Fail("only %d men came in to a rest in a wall: the arrival was not tried", Arrivals - Bounds);
	if (ArrivedShort > 0) Fail("%d men who came in to their rest were not at the clear place as they stopped (the furthest %.3f m off)", ArrivedShort, std::max(WorstArrival, WorstBound));
	if (SkateChecks < 2000) Fail("only %d frames of a man holding a rest pose a step off the rules' place: the skating was not tried", SkateChecks);
	if (SkateFrames > 0) Fail("%d times the step of a man holding a rest pose moved: %d men slid where they stood, the longest %.3f m", SkateFrames, Skaters, LongestSkate);
	if (MovingByWall < 20) Fail("only %d checks of a man on the move by a wall: the walk past was not tried", MovingByWall);
	if (Jumps > 0) Fail("%d times a man's step moved faster than a walk", Jumps);
	if (FirstAsked > 0 && FirstClear != FirstAsked) Fail("%d of %d men who start the battle in a wall did not start clear of it", FirstAsked - FirstClear, FirstAsked);
	if (MovedOff > 0) Fail("%d times a man whose place the rules had clear of every wall a second was still drawn off it", MovedOff);
	if (OnTheMove < 2000) Fail("only %d checks of these men clear of their walls: the way on was not tried", OnTheMove);
	if (FellMoved > 0) Fail("%d times a fallen man did not lie where he was drawn", FellMoved);
	if (NotRules > 0) Fail("%d times the step was not the whole difference from the plain crowd", NotRules);
	if (Differ > 0 || Same < 1000) Fail("men nowhere near a wall: %d readings differ from the plain crowd (%d the same)", Differ, Same);
	if (LongestStep > 2.0 * (Half + LandStone::Walls::MostBody)) Fail("the longest step was %.2f m", LongestStep);
	TeamsInBattle(RecText, Rec, Files, ReadFile(Argv[3]), Argv[1]);
	std::printf("%s\n", Bad ? "FAILED" : "passed");
	return Bad ? 1 : 0;
}
