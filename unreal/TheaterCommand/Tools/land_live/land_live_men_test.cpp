// The live men's cloud test (release 2): Battle/LandMen's crowd grown from a live battle's header and deltas
// (Crowd::Extend, as the 3D game's scene will feed it) against the crowd built from the same battle read whole
// (Crowd::Build), both played at 60 frames a second with the screen some battle seconds behind the latest delta (2 by
// default, as the scene runs; the deltas come in while it plays, and once over it plays on to the end and 8 s more).
// Every frame, every man: the same count, his place within 0.25 m (a provisional point is within 0.1 m each way), his
// fall time known (and right) by the time the screen comes to it, falling and at rest on the same frames, the same clip
// for 99% of man-frames at least, and the same shots (man, round time, bullet, effect) and falls each frame. Once over,
// every body at rest in the same place, with the same fall.
// Usage: land_live_men_test <folder with header.json, deltas.jsonl, whole.json> <soldiers folder> <armies.json> [lag]
#include "land_json.h"
#include "Battle/LandMen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace LandMen;

namespace
{
	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	double Ms(std::chrono::steady_clock::time_point A, std::chrono::steady_clock::time_point B)
	{
		return std::chrono::duration<double, std::milli>(B - A).count();
	}

	/** The battle's ground (the header's field, as land_show_test reads it): heights at the cells' centres, bilinear. */
	struct Field
	{
		int Cols = 1, Rows = 1;
		double Cell = 25.0;
		std::vector<float> H;
		float At(int C, int R) const
		{
			C = std::max(0, std::min(Cols - 1, C));
			R = std::max(0, std::min(Rows - 1, R));
			return H[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)];
		}
		Ground operator()(double X, double Y) const
		{
			const double FX = X / Cell - 0.5, FY = Y / Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			const double U = FX - C0, V = FY - R0;
			const double H00 = At(C0, R0), H10 = At(C0 + 1, R0), H01 = At(C0, R0 + 1), H11 = At(C0 + 1, R0 + 1);
			Ground G;
			G.Height = static_cast<float>(H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V);
			const double DX = ((H10 - H00) * (1 - V) + (H11 - H01) * V) / Cell;
			const double DY = ((H01 - H00) * (1 - U) + (H11 - H10) * U) / Cell;
			const double L = std::sqrt(DX * DX + DY * DY + 1.0);
			G.Normal = { static_cast<float>(-DX / L), static_cast<float>(-DY / L), static_cast<float>(1.0 / L) };
			return G;
		}
	};

	/** A shot or a fall as the frame's events give it: man, effect, round time, bullet (falls: the slump). */
	using Fired = std::tuple<int, std::string, double, int, int>;

	std::vector<Fired> ShotsAndFalls(const Crowd& C)
	{
		std::vector<Fired> Out;
		for (const Event& E : C.Events())
		{
			if (E.Fx == "muzzle_flash" || E.Fx == "grenade_release" || E.Fx == "mortar" || E.Fx == "slump")
			{
				// (a clip's own muzzle flash is off, as the scene plays: every flash is a round's)
				Out.emplace_back(E.Man, E.Fx, E.T, E.Round, static_cast<int>(E.Gun));
			}
		}
		std::sort(Out.begin(), Out.end());
		return Out;
	}

	/** Every event of the frame, the clips' own too (footsteps, knees, bolts), for the share that match. */
	std::vector<std::tuple<int, std::string, long long>> AllEvents(const Crowd& C)
	{
		std::vector<std::tuple<int, std::string, long long>> Out;
		for (const Event& E : C.Events()) Out.emplace_back(E.Man, E.Fx, std::llround(E.T * 1e6));
		std::sort(Out.begin(), Out.end());
		return Out;
	}

	Vec3 PelvisOf(const Crowd& C, int Man)
	{
		const Soldier* S = C.SoldierOf(Man);
		const int B = S->BoneIndex("pelvis");
		if (B < 0) return {};
		return C.FramesOf(Man)[B].Point(S->Bones[static_cast<size_t>(B)].Head);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: land_live_men_test <folder> <soldiers folder> <armies.json> [lag seconds]\n");
		return 2;
	}
	const std::string Folder = Argv[1];
	const double Lag = Argc > 4 ? std::atof(Argv[4]) : 2.0;
	int Failures = 0;
	auto Fail = [&](const char* Fmt, auto... Args) {
		++Failures;
		if (Failures <= 30)
		{
			std::printf("  FAIL: ");
			if constexpr (sizeof...(Args) == 0) std::fputs(Fmt, stdout);
			else std::printf(Fmt, Args...);
			std::printf("\n");
		}
	};

	// ---- The files: the soldiers, the header (its field for the ground), the deltas a line each, the whole battle.
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	std::string Err;
	for (const char* Army : { "GER", "POL", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader", "USA_radio", "SOV" })
	{
		const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + ".json");
		if (Text.empty() && (std::string(Army).rfind("USA", 0) == 0 || std::string(Army) == "SOV")) continue;
		auto S = std::make_shared<Soldier>();
		if (Text.empty() || !S->Load(Text, Err)) { std::printf("cannot read soldier_%s.json: %s\n", Army, Err.c_str()); return 1; }
		Files[Army] = S;
	}
	const std::string ArmiesText = ReadFile(Argv[3]);
	const std::string HeaderText = ReadFile(Folder + "/header.json");
	const std::string WholeText = ReadFile(Folder + "/whole.json");
	LandJson::Value Header;
	if (HeaderText.empty() || !LandJson::Parse(HeaderText, Header)) { std::printf("cannot read %s/header.json\n", Folder.c_str()); return 1; }
	Field Fd;
	{
		const LandJson::Value& F = Header["field"];
		Fd.Cols = F["cols"].Int(120);
		Fd.Rows = F["rows"].Int(80);
		Fd.Cell = F["cell"].Num(25.0);
		for (size_t I = 0; I < F["height"].Count(); ++I) Fd.H.push_back(static_cast<float>(F["height"].At(I).Num()));
		if (Fd.H.size() != static_cast<size_t>(Fd.Cols) * static_cast<size_t>(Fd.Rows)) { std::printf("field heights do not fit its cells\n"); return 1; }
	}
	const GroundFn GroundAt = [&Fd](double X, double Y) { return Fd(X, Y); };
	struct Delta { double T = 0.0; std::string Text; };
	std::vector<Delta> Deltas;
	{
		std::ifstream In(Folder + "/deltas.jsonl", std::ios::binary);
		std::string Line;
		while (std::getline(In, Line))
		{
			if (Line.empty()) continue;
			const size_t At = Line.find("\"t\":");
			if (At == std::string::npos) { std::printf("a delta without t\n"); return 1; }
			Deltas.push_back({ std::strtod(Line.c_str() + At + 4, nullptr), std::move(Line) });
		}
	}
	if (Deltas.empty() || WholeText.empty()) { std::printf("no deltas or no whole.json in %s\n", Folder.c_str()); return 1; }
	const double End = Deltas.back().T;

	// ---- The two crowds, alike but for what they are built from.
	Crowd W, L;
	for (Crowd* C : { &W, &L })
	{
		C->SetLife(2.0);
		for (const auto& KV : Files) C->AddSoldier(KV.first, KV.second);
		if (!C->ReadArmies(ArmiesText, Err)) { std::printf("cannot read armies.json: %s\n", Err.c_str()); return 1; }
		C->SetThrift(false);
	}
	if (!W.Build(WholeText, GroundAt, Err)) { std::printf("cannot build the whole battle: %s\n", Err.c_str()); return 1; }
	if (!L.Build(HeaderText, GroundAt, Err)) { std::printf("cannot build from the header: %s\n", Err.c_str()); return 1; }
	if (L.Count() != 0) Fail("the header made %d men (none expected)", L.Count());
	// (the eye where the land show puts it does not matter here: both crowds pose every man every frame)
	W.SetEye(0.0, 0.0, 0.0);
	L.SetEye(0.0, 0.0, 0.0);

	// ---- Play.
	const double Dt = 1.0 / 60.0;
	size_t Next = 0;
	double Latest = 0.0;
	double ExtendMs = 0.0, ExtendMost = 0.0, ExtendMostAfter = 0.0, FirstMs = 0.0;
	std::vector<std::pair<size_t, double>> SlowDeltas;   // (the scene's game thread takes a delta between frames)
	long long ManFrames = 0, ClipSame = 0, Over01 = 0, Over025 = 0, CountDiff = 0, DeadDiff = 0, FrozenDiff = 0, FallingDiff = 0;
	long long ShotFrames = 0, ShotFramesDiff = 0, ShotsW = 0, ShotsL = 0, FallsW = 0, FallsL = 0, FallsDiff = 0;
	long long AllW = 0, AllSame = 0;
	double MostOff = 0.0, SumOff = 0.0;
	double MostOffT = 0.0;
	int MostOffMan = -1;
	bool bLooksChecked = false;
	int ClipShown = 0, ShotShown = 0, StateShown = 0;
	std::map<std::pair<std::string, std::string>, long long> ClipPairs;
	std::vector<double> FellW, FellL;
	std::vector<double> KnownAt;    // the frame each live man's fall was first known
	double SoonestFall = 1e9;       // the least time between knowing a fall and the screen reaching it
	int Frames = 0;
	for (int F = 0;; ++F)
	{
		const double T = F * Dt;
		if (T > End + 8.0) break;
		// The deltas the rules have handed over by now: the screen stays Lag behind the latest.
		while (Next < Deltas.size() && Latest < T + Lag - 1e-9)
		{
			const auto A = std::chrono::steady_clock::now();
			if (!L.Extend(Deltas[Next].Text, Err)) { std::printf("delta %zu (t %.1f) refused: %s\n", Next, Deltas[Next].T, Err.c_str()); return 1; }
			const double Took = Ms(A, std::chrono::steady_clock::now());
			ExtendMs += Took;
			ExtendMost = std::max(ExtendMost, Took);
			if (Next == 0) FirstMs = Took;
			else ExtendMostAfter = std::max(ExtendMostAfter, Took);
			if (Took > 8.0) SlowDeltas.push_back({ Next, Took });
			Latest = Deltas[Next].T;
			++Next;
		}
		W.Update(T);
		L.Update(T);
		++Frames;
		if (!bLooksChecked && L.Count() > 0)
		{
			// Who is who, as Build settles it: the same men in the same order, with the same looks.
			bLooksChecked = true;
			int Diff = 0;
			for (int I = 0; I < std::min(W.Count(), L.Count()); ++I)
			{
				const ManInfo& A = W.Info(I);
				const ManInfo& B = L.Info(I);
				if (A.Block != B.Block || A.K != B.K || A.J != B.J || A.Role != B.Role || A.Army != B.Army || A.FileArmy != B.FileArmy
					|| A.Face != B.Face || A.Finish != B.Finish || A.Delay != B.Delay || A.Jitter != B.Jitter || L.Find(B.Block, B.K, B.J) != I)
					++Diff;
			}
			if (Diff) Fail("%d men differ in who they are or their looks", Diff);
		}
		if (W.Count() != L.Count())
		{
			++CountDiff;
			if (CountDiff == 1) Fail("at %.3f s the whole battle has %d men, the live one %d", T, W.Count(), L.Count());
		}
		const int N = std::min(W.Count(), L.Count());
		for (int I = 0; I < N; ++I)
		{
			++ManFrames;
			double XA = 0.0, YA = 0.0, ZA = 0.0, XB = 0.0, YB = 0.0, ZB = 0.0;
			float FA = 0.f, FB = 0.f;
			W.PlaceOf(I, XA, YA, ZA, FA);
			L.PlaceOf(I, XB, YB, ZB, FB);
			const double Off = std::hypot(XA - XB, YA - YB);
			SumOff += Off;
			if (Off > MostOff) { MostOff = Off; MostOffT = T; MostOffMan = I; }
			if (Off > 0.1) ++Over01;
			if (Off > 0.25) ++Over025;
			// (the whole battle knows every fall from the start, the live one once a delta tells it: never another time,
			// and by the time the screen comes to it)
			const double DA = W.Info(I).Dead, DB = L.Info(I).Dead;
			if ((DB >= 0.0 && DB != DA) || (DA >= 0.0 && DA <= T && DB != DA)) ++DeadDiff;
			if (DB >= 0.0 && (KnownAt.size() <= static_cast<size_t>(I) || KnownAt[static_cast<size_t>(I)] < 0.0))
			{
				KnownAt.resize(std::max(KnownAt.size(), static_cast<size_t>(I) + 1), -1.0);
				KnownAt[static_cast<size_t>(I)] = T;
				SoonestFall = std::min(SoonestFall, DB - T);
			}
			const bool bFrozenDiff = W.Frozen(I) != L.Frozen(I), bFallingDiff = W.IsFalling(I) != L.IsFalling(I);
			FrozenDiff += bFrozenDiff;
			FallingDiff += bFallingDiff;
			if ((bFrozenDiff || bFallingDiff) && StateShown++ < 5)
				std::printf("  state differs: man %d at %.3f s: whole %s%s, live %s%s (d %.2f / %.2f)\n", I, T, W.Frozen(I) ? "frozen" : "",
					W.IsFalling(I) ? "falling" : "", L.Frozen(I) ? "frozen" : "", L.IsFalling(I) ? "falling" : "", W.Info(I).Dead, L.Info(I).Dead);
			const Playing PA = W.PlayingOf(I), PB = L.PlayingOf(I);
			if (PA.Clip == PB.Clip) ++ClipSame;
			else
			{
				++ClipPairs[{ PA.Clip, PB.Clip }];
				if (ClipShown++ < 8)
					std::printf("  clip differs: man %d (%s) at %.3f s: whole %s, live %s\n", I, W.Info(I).Role.c_str(), T, PA.Clip.c_str(), PB.Clip.c_str());
			}
		}
		// The frame's shots and falls, one for one.
		const std::vector<Fired> SA = ShotsAndFalls(W), SB = ShotsAndFalls(L);
		for (const Fired& E : SA) (std::get<1>(E) == "slump" ? FallsW : ShotsW) += 1;
		for (const Fired& E : SB) (std::get<1>(E) == "slump" ? FallsL : ShotsL) += 1;
		if (!SA.empty() || !SB.empty())
		{
			++ShotFrames;
			if (SA != SB)
			{
				++ShotFramesDiff;
				if (ShotShown++ < 5) std::printf("  shots differ at %.3f s: whole %zu, live %zu\n", T, SA.size(), SB.size());
			}
		}
		// Falls begun this frame (the fall's clip, or a slump): the same men, the same clips.
		for (int I = 0; I < N; ++I)
		{
			const bool bA = W.Info(I).FallStarted >= 0.0 && (FellW.size() <= static_cast<size_t>(I) || FellW[static_cast<size_t>(I)] < 0.0);
			const bool bB = L.Info(I).FallStarted >= 0.0 && (FellL.size() <= static_cast<size_t>(I) || FellL[static_cast<size_t>(I)] < 0.0);
			if (bA != bB || (bA && W.Info(I).FallClip != L.Info(I).FallClip))
			{
				++FallsDiff;
				Fail("man %d at %.3f s: whole fall %s, live fall %s", I, T, bA ? W.Info(I).FallClip.c_str() : "-", bB ? L.Info(I).FallClip.c_str() : "-");
			}
		}
		FellW.resize(static_cast<size_t>(W.Count()), -1.0);
		FellL.resize(static_cast<size_t>(L.Count()), -1.0);
		for (int I = 0; I < W.Count(); ++I) FellW[static_cast<size_t>(I)] = W.Info(I).FallStarted;
		for (int I = 0; I < L.Count(); ++I) FellL[static_cast<size_t>(I)] = L.Info(I).FallStarted;
		const auto EA = AllEvents(W), EB = AllEvents(L);
		AllW += static_cast<long long>(EA.size());
		{
			size_t A = 0, B = 0;
			while (A < EA.size() && B < EB.size())
			{
				if (EA[A] == EB[B]) { ++AllSame; ++A; ++B; }
				else if (EA[A] < EB[B]) ++A;
				else ++B;
			}
		}
	}

	// ---- Once over: every body at rest where the whole battle laid it.
	int Bodies = 0, BodiesOff = 0, RestDiff = 0;
	double BodyMost = 0.0;
	for (int I = 0; I < std::min(W.Count(), L.Count()); ++I)
	{
		if (W.Frozen(I) != L.Frozen(I)) { ++RestDiff; continue; }
		if (!W.Frozen(I)) continue;
		++Bodies;
		double XA = 0.0, YA = 0.0, ZA = 0.0, XB = 0.0, YB = 0.0, ZB = 0.0;
		float FA = 0.f, FB = 0.f;
		W.PlaceOf(I, XA, YA, ZA, FA);
		L.PlaceOf(I, XB, YB, ZB, FB);
		const Vec3 PA = PelvisOf(W, I), PB = PelvisOf(L, I);
		const double Off = std::max(std::hypot(XA - XB, YA - YB), static_cast<double>(std::sqrt((PA.X - PB.X) * (PA.X - PB.X) + (PA.Y - PB.Y) * (PA.Y - PB.Y) + (PA.Z - PB.Z) * (PA.Z - PB.Z))));
		BodyMost = std::max(BodyMost, Off);
		if (Off > 0.25 || W.Info(I).FallClip != L.Info(I).FallClip || W.Info(I).Rested != L.Info(I).Rested) ++BodiesOff;
	}

	const double ClipShare = ManFrames ? 100.0 * static_cast<double>(ClipSame) / static_cast<double>(ManFrames) : 100.0;
	std::printf("%s: %d men, %d frames at 60 a second to %.1f s, the screen %.1f s behind the latest of %zu deltas\n", Folder.c_str(), W.Count(),
		Frames, End + 8.0, Lag, Deltas.size());
	std::printf("  places: most %.3f m off (man %d at %.2f s), mean %.4f m; man-frames over 0.1 m %lld (%.3f%%), over 0.25 m %lld\n", MostOff,
		MostOffMan, MostOffT, ManFrames ? SumOff / static_cast<double>(ManFrames) : 0.0, Over01,
		ManFrames ? 100.0 * static_cast<double>(Over01) / static_cast<double>(ManFrames) : 0.0, Over025);
	std::printf("  count differs on %lld frames; man-frames with a fall time wrong or not yet known when shown %lld (each known at least %.2f s before\n"
		"  the screen reached it), at rest %lld, falling %lld\n", CountDiff, DeadDiff, SoonestFall, FrozenDiff, FallingDiff);
	std::printf("  clips: the same in %.3f%% of %lld man-frames (%lld differ)", ClipShare, ManFrames, ManFrames - ClipSame);
	{
		std::vector<std::pair<long long, std::pair<std::string, std::string>>> Top;
		for (const auto& KV : ClipPairs) Top.push_back({ KV.second, KV.first });
		std::sort(Top.rbegin(), Top.rend());
		for (size_t K = 0; K < Top.size() && K < 4; ++K) std::printf("%s %s/%s %lld", K ? ";" : ":", Top[K].second.first.c_str(), Top[K].second.second.c_str(), Top[K].first);
	}
	std::printf("\n  shots %lld whole, %lld live, falls (slumps) %lld / %lld; frames with shots or falls %lld, differing %lld; falls begun differently %lld\n",
		ShotsW, ShotsL, FallsW, FallsL, ShotFrames, ShotFramesDiff, FallsDiff);
	std::printf("  every event the same frame (the clips' own too: footsteps, knees, bolts): %.2f%% of %lld\n",
		AllW ? 100.0 * static_cast<double>(AllSame) / static_cast<double>(AllW) : 100.0, AllW);
	std::printf("  once over: %d bodies at rest, the most %.3f m from the whole battle's, %d off (place, fall or rest time); %d differ in being at rest\n",
		Bodies, BodyMost, BodiesOff, RestDiff);
	std::printf("  Extend: %.1f ms in all, %.2f ms a delta, the most %.1f ms; the first (%zu bytes, every man new) %.1f ms, the most after it %.1f ms;"
		" over 8 ms:", ExtendMs, ExtendMs / static_cast<double>(Deltas.size()), ExtendMost, Deltas.front().Text.size(), FirstMs, ExtendMostAfter);
	if (SlowDeltas.empty()) std::printf(" none");
	for (const auto& Slow : SlowDeltas) std::printf(" delta %zu (t %.1f, %zu bytes) %.1f ms;", Slow.first, Deltas[Slow.first].T, Deltas[Slow.first].Text.size(), Slow.second);
	std::printf("\n");

	if (Next != Deltas.size()) Fail("only %zu of %zu deltas taken", Next, Deltas.size());
	if (CountDiff) Fail("the men's count differs on %lld frames", CountDiff);
	if (Over025) Fail("%lld man-frames more than 0.25 m from the whole battle's place", Over025);
	if (DeadDiff) Fail("%lld man-frames with a fall time wrong or not known when shown", DeadDiff);
	if (FrozenDiff || FallingDiff) Fail("%lld man-frames at rest and %lld falling differently", FrozenDiff, FallingDiff);
	if (ClipShare < 99.0) Fail("the clips are the same in only %.2f%% of man-frames", ClipShare);
	if (ShotFramesDiff) Fail("%lld frames with different shots or falls", ShotFramesDiff);
	if (BodiesOff || RestDiff) Fail("%d bodies lie elsewhere or fell otherwise, %d differ in being at rest", BodiesOff, RestDiff);
	std::printf("%s %s (%d failures)\n", Failures ? "FAIL" : "PASS", Folder.c_str(), Failures);
	return Failures ? 1 : 0;
}
