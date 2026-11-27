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
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
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
}

int main(int Argc, char** Argv)
{
	WallsOwn();
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
	std::printf("%s\n", Bad ? "FAILED" : "passed");
	return Bad ? 1 : 0;
}
