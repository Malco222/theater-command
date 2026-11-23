// The men's cloud test: plays a recorded ground battle's men through Battle/LandMen (the game's own code, no engine) at
// 60 frames a second, as the scene would, and checks what Brian will see: nothing broken (no NaN), every man the
// battle kills starts his fall on the frame he is hit, ends lying on the ground (not standing, not sunk in it) and
// never moves again, no body is ever taken away, feet do not skate where the play rate can follow the ground, every
// round the rules fired comes out as one shot at its own time from the man's muzzle, and the frame's cost.
// Usage: land_show_test <recording.json> <soldiers folder> <armies.json>
// LAND_SHOW_THRIFT=1 plays the crowd thrifty (Crowd::SetThrift, as the scene does: far men posed every few frames);
// LAND_SHOW_THREADS=n (4 by default) times the scene's own skin of the men in the picture over n threads too.
// LAND_SHOW_LIFE=x: life seconds a battle second (Crowd::SetLife), 2 by default as the scene plays the fight (the rules'
// clock about twice life's); 1 plays the clips on the battle's clock, as before.
// LAND_SHOW_LEVELS=1 sets the scene's GS3 levels (Crowd::SetLevels: Near under 8.6 m, the 150 lines of a 1.8 m man at
// the 50 deg lens and 665 lines; Mid to 80 m, where his shadow ends) and plays the scene's cap of 50 meshes made again
// a frame: how many men change level a frame, how long the cap keeps one at his old level, and that no man flickers
// between two levels (back again within half a second).
#include "land_json.h"
#include "Battle/LandMen.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
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

	/** The recording's ground: heights at the cells' centres, row 0 the south edge, read between them bilinearly. */
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

	bool Finite(const Vec3& V) { return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z); }

	/** The scene's ParallelFor, for the timing: a few threads kept waiting, each taking the next man until none is left
	 * (the caller's own thread works too). */
	class Pool
	{
	public:
		explicit Pool(int Threads)
		{
			for (int K = 1; K < Threads; ++K) Workers.emplace_back([this] { Work(); });
		}
		~Pool()
		{
			{
				std::lock_guard<std::mutex> Lock(Mx);
				bStop = true;
				++Round;
			}
			Wake.notify_all();
			for (std::thread& W : Workers) W.join();
		}
		void For(int Count, const std::function<void(int)>& Body)
		{
			{
				std::lock_guard<std::mutex> Lock(Mx);
				Job = &Body;
				Total = Count;
				Next = 0;
				Busy = static_cast<int>(Workers.size());
				++Round;
			}
			Wake.notify_all();
			Take();
			std::unique_lock<std::mutex> Lock(Mx);
			Done.wait(Lock, [this] { return Busy == 0; });
		}
	private:
		void Take()
		{
			for (int I = Next.fetch_add(1); I < Total; I = Next.fetch_add(1)) (*Job)(I);
		}
		void Work()
		{
			uint64_t Seen = 0;
			for (;;)
			{
				{
					std::unique_lock<std::mutex> Lock(Mx);
					Wake.wait(Lock, [&] { return Round != Seen; });
					Seen = Round;
					if (bStop) return;
				}
				Take();
				{
					std::lock_guard<std::mutex> Lock(Mx);
					--Busy;
				}
				Done.notify_one();
			}
		}
		std::vector<std::thread> Workers;
		std::mutex Mx;
		std::condition_variable Wake, Done;
		const std::function<void(int)>* Job = nullptr;
		std::atomic<int> Next { 0 };
		int Total = 0, Busy = 0;
		uint64_t Round = 0;
		bool bStop = false;
	};

	uint64_t Hash(const float* P, size_t N)
	{
		uint64_t H = 1469598103934665603ull;
		const unsigned char* B = reinterpret_cast<const unsigned char*>(P);
		for (size_t I = 0; I < N * sizeof(float); ++I) { H ^= B[I]; H *= 1099511628211ull; }
		return H;
	}

	double Percentile(std::vector<double> V, double P)
	{
		if (V.empty()) return 0.0;
		std::sort(V.begin(), V.end());
		const size_t I = std::min(V.size() - 1, static_cast<size_t>(P * static_cast<double>(V.size() - 1) + 0.5));
		return V[I];
	}

	/** A gait's own foot slide, as the clip is made: played in place at its own rate with the ground going back under
	 * him at its stride speed, the planted ball of each foot (within 2 cm of its lowest) moves this fast. The clips are
	 * not perfect; the crowd is checked against this, scaled by the rate it plays at. */
	struct GaitFeet
	{
		float Low[2] = { 0.f, 0.f };   // each ball's lowest height over the loop
		double Slide = 0.0;             // m/s at the clip's own rate
		double Ahead = 0.0;             // of which along his forward (the part the play rate governs)
	};

	GaitFeet FeetOf(const Soldier& S, const Clip& C, int BallL, int BallR)
	{
		GaitFeet G;
		const int N = 120;
		std::vector<Vec3> P[2];
		Pose Ps;
		Xform Fr[MaxBones];
		const int Ball[2] = { BallL, BallR };
		const float Spd = C.Speed();
		for (int I = 0; I <= N; ++I)
		{
			const float T = C.Long * static_cast<float>(I) / N;
			Sample(C, T, Ps);
			BoneFrames(S, Ps, Fr);
			for (int F = 0; F < 2; ++F)
			{
				Vec3 At = Fr[Ball[F]].Point(S.Bones[static_cast<size_t>(Ball[F])].Head);
				At.X += Spd * T;
				P[F].push_back(At);
			}
		}
		for (int F = 0; F < 2; ++F)
		{
			G.Low[F] = 1e9f;
			for (const Vec3& V : P[F]) G.Low[F] = std::min(G.Low[F], V.Z);
		}
		double Sum = 0.0, SumAhead = 0.0;
		int Count = 0;
		const double Dt = C.Long / N;
		for (int F = 0; F < 2; ++F)
		{
			for (int I = 1; I <= N; ++I)
			{
				const Vec3& A = P[F][static_cast<size_t>(I - 1)];
				const Vec3& B = P[F][static_cast<size_t>(I)];
				if (A.Z < G.Low[F] + 0.02f && B.Z < G.Low[F] + 0.02f)
				{
					Sum += std::hypot(B.X - A.X, B.Y - A.Y) / Dt;
					SumAhead += std::fabs(B.X - A.X) / Dt;
					++Count;
				}
			}
		}
		G.Slide = Count ? Sum / Count : 0.0;
		G.Ahead = Count ? SumAhead / Count : 0.0;
		return G;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: land_show_test <recording.json> <soldiers folder> <armies.json>\n");
		return 2;
	}
	int Fails = 0;
	auto Fail = [&](const char* Fmt, auto... Args) {
		++Fails;
		if (Fails <= 40)
		{
			std::printf("  FAIL: ");
			if constexpr (sizeof...(Args) == 0) std::fputs(Fmt, stdout);
			else std::printf(Fmt, Args...);
			std::printf("\n");
		}
	};

	// ---- The files.
	const std::string RecText = ReadFile(Argv[1]);
	LandJson::Value Rec;
	std::string Err;
	if (RecText.empty() || !LandJson::Parse(RecText, Rec)) { std::printf("cannot read %s\n", Argv[1]); return 1; }
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	for (const char* Army : { "GER", "POL", "USA", "USA_bar", "USA_nco", "USA_bazooka", "USA_loader", "USA_radio", "SOV" })
	{
		const std::string Path = std::string(Argv[2]) + "/soldier_" + Army + ".json";
		const std::string Text = ReadFile(Path);
		if (Text.empty() && (std::string(Army).rfind("USA", 0) == 0 || std::string(Army) == "SOV")) continue; // (the GI's and the Red Army man's files, when they are here)
		auto S = std::make_shared<Soldier>();
		const auto A = std::chrono::steady_clock::now();
		if (Text.empty() || !S->Load(Text, Err)) { std::printf("cannot read %s: %s\n", Path.c_str(), Err.c_str()); return 1; }
		const auto B = std::chrono::steady_clock::now();
		std::printf("%s: %zu bytes, %zu bones, %zu clips, near/mid/far %d/%d/%d points, %zu faces, %zu helmet finishes, deaths map %s; read in %.1f ms\n",
			Path.c_str(), Text.size(), S->Bones.size(), S->Clips.size(), S->Meshes[Near].Points, S->Meshes[Mid].Points, S->Meshes[Far].Points,
			S->Faces.size(), S->Finishes.size(), S->Deaths.empty() ? "no (the README's three falls)" : "yes", Ms(A, B));
		// (release 2's field clips' prop keys, as the scene draws the lineman's reel and the spliced wire's ends by them:
		// the reel's root before him, his fists on its handles 0.23 m apart and 0.56 m over its root; two fists at a splice)
		for (const char* Name : { "reel_walk", "splice_kneel", "stretcher_front" })
		{
			const int Ix = S->ClipIndex(Name);
			if (Ix < 0) continue;
			const Clip& Cl = S->Clips[static_cast<size_t>(Ix)];
			const std::string Track = std::string(Name) == "reel_walk" ? "reel" : std::string(Name) == "splice_kneel" ? "hands" : "grips";
			const Clip::PropKeys* Pk = Cl.PropOf(Track);
			if (!Pk) { Fail("%s: %s has no %s keys", Path.c_str(), Name, Track.c_str()); continue; }
			std::vector<float> V(Pk->Fields.size());
			int Off = 0;
			for (int K = 0; K <= 20; ++K)
			{
				Pk->Sample(Cl.Long * K / 20.f, Cl.bLoop, Cl.Long, V.data());
				if (Track == "reel" && V.size() >= 9)
				{
					const float Apart = std::hypot(V[6] - V[3], V[7] - V[4]), Over = 0.5f * (V[5] + V[8]) - V[2];
					Off += V[0] < 0.2f || V[0] > 0.8f || std::fabs(Apart - 0.23f) > 0.02f || std::fabs(Over - 0.56f) > 0.03f ? 1 : 0;
				}
				else if (Track == "hands" && V.size() >= 6)
				{
					Off += V[2] < 0.2f || V[2] > 1.2f || V[5] < 0.2f || V[5] > 1.2f || std::hypot(V[3] - V[0], V[4] - V[1]) > 0.5f ? 1 : 0;
				}
			}
			if (Off > 0) Fail("%s: %s's %s keys off the prop's handles at %d of 21 times", Path.c_str(), Name, Track.c_str(), Off);
			else if (Track != "grips") std::printf("  %s: %s keys, %zu fields, %zu keys over %.2f s\n", Name, Track.c_str(), Pk->Fields.size(), Pk->T.size(), Cl.Long);
		}
		Files[Army] = S;
	}
	Field Fd;
	{
		const LandJson::Value& F = Rec["field"];
		Fd.Cols = F["cols"].Int(120);
		Fd.Rows = F["rows"].Int(80);
		Fd.Cell = F["cell"].Num(25.0);
		for (size_t I = 0; I < F["height"].Count(); ++I) Fd.H.push_back(static_cast<float>(F["height"].At(I).Num()));
		if (Fd.H.size() != static_cast<size_t>(Fd.Cols) * static_cast<size_t>(Fd.Rows)) { std::printf("field heights do not fit its cells\n"); return 1; }
	}
	// (and a shell hole's rim torn under bodies already lying, for the Unsettle check after the run: none until then)
	double RimX = 0.0, RimY = 0.0, RimH = 0.0;
	const GroundFn GroundAt = [&Fd, &RimX, &RimY, &RimH](double X, double Y)
	{
		Ground G = Fd(X, Y);
		if (RimH > 0.0)
		{
			const double D = (std::sqrt((X - RimX) * (X - RimX) + (Y - RimY) * (Y - RimY)) - 1.2) / 0.45;
			G.Height += static_cast<float>(RimH * std::exp(-D * D));
		}
		return G;
	};

	const char* LifeEnv = std::getenv("LAND_SHOW_LIFE");
	const double Life = LifeEnv ? std::atof(LifeEnv) : 2.0;
	Crowd C;
	C.SetLife(Life);
	std::printf("life: %.2f life seconds a battle second (the men's clips, bursts and falls timed in life's)\n", Life);
	for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
	if (!C.ReadArmies(ReadFile(Argv[3]), Err)) { std::printf("cannot read %s: %s\n", Argv[3], Err.c_str()); return 1; }
	{
		const auto A = std::chrono::steady_clock::now();
		if (!C.Build(RecText, GroundAt, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
		const auto B = std::chrono::steady_clock::now();
		std::printf("%s: %d men (%s, %d-%02d, %s, front %s); built in %.1f ms\n", Argv[1], C.Count(), Rec["title"].Str("").c_str(),
			C.Month() / 12, C.Month() % 12 + 1, C.Season().c_str(), C.FrontName().c_str(), Ms(A, B));
	}
	const int N = C.Count();
	const char* ThriftEnv = std::getenv("LAND_SHOW_THRIFT");
	const bool bThrift = ThriftEnv && std::string(ThriftEnv) == "1";
	C.SetThrift(bThrift);
	// LAND_SHOW_RUN_STYLES=1: each man his own runner (Crowd::SetRunStyles, the scene's "runs" trial)
	const char* StylesEnv = std::getenv("LAND_SHOW_RUN_STYLES");
	const bool bRunStyles = StylesEnv && std::string(StylesEnv) == "1";
	C.SetRunStyles(bRunStyles);
	if (bRunStyles) std::printf("run styles: each man his own runner (Crowd::SetRunStyles)\n");
	const char* ThreadsEnv = std::getenv("LAND_SHOW_THREADS");
	const int Threads = std::max(1, ThreadsEnv ? std::atoi(ThreadsEnv) : 4);
	if (bThrift) std::printf("thrifty: far men posed every 2nd to 4th frame by their distance (Crowd::SetThrift)\n");
	const char* LevelsEnv = std::getenv("LAND_SHOW_LEVELS");
	const bool bLevels = LevelsEnv && std::string(LevelsEnv) == "1";
	const double LevelLines = 1.8 * 665.0 / (2.0 * std::tan(25.0 * 3.14159265358979 / 180.0));
	const double CullFrom = LevelLines / 2.0;
	const int RebuildCap = 50;
	if (bLevels)
	{
		C.SetLevels(LevelLines / 150.0, 80.0);
		std::printf("levels (GS3): Near within %.2f m, Mid to 80 m, Far beyond, none past %.0f m but in the mass shot; at most %d meshes made again a frame\n",
			LevelLines / 150.0, CullFrom, RebuildCap);
	}

	// Who is who: armies, faces and helmets.
	{
		std::map<std::string, int> ByArmy, ByFinish;
		std::set<int> Faces[2];
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(I);
			ByArmy[In.Army + " (" + In.FileArmy + " file) " + In.Role]++;
			const Soldier* S = C.SoldierOf(I);
			ByFinish[In.Army + ": " + (In.Finish >= 0 ? S->Finishes[static_cast<size_t>(In.Finish)].Name : std::string("own helmet"))]++;
			Faces[In.Army == "GER" ? 0 : 1].insert(In.Face);
		}
		std::printf("men:");
		for (const auto& KV : ByArmy) std::printf(" %s %d;", KV.first.c_str(), KV.second);
		std::printf("\nhelmets:");
		for (const auto& KV : ByFinish) std::printf(" %s %d;", KV.first.c_str(), KV.second);
		std::printf("\nfaces: GER %zu of 8 used, USA %zu of 8 used\n", Faces[0].size(), Faces[1].size());
	}

	// The rounds the rules fired, by the man who fired them, and the dead.
	struct Booked { double T; int Man; std::string W; };
	std::vector<Booked> Rounds;
	{
		const LandJson::Value& Rs = Rec["rounds"];
		for (size_t K = 0; K < Rs.Count(); ++K)
		{
			const LandJson::Value& R = Rs.At(K);
			const int Man = C.Find(R["by"].Int(-1), R["k"].Int(-1), std::max(0, R["j"].Int(0)));
			if (Man < 0) continue;
			const double T = R["t"].Num();
			const double D = C.Info(Man).Dead;
			if (D >= 0.0 && T > D) continue;
			Rounds.push_back({ T, Man, R["w"].Str("") });
		}
	}
	std::vector<int> Dead;
	for (int I = 0; I < N; ++I) if (C.Info(I).Dead >= 0.0) Dead.push_back(I);
	double LastDeath = 0.0;
	for (const int I : Dead) LastDeath = std::max(LastDeath, C.Info(I).Dead);
	const double End = std::max(Rec["end"].Num(270.0), LastDeath + 12.0);

	// The gaits' own foot slide (the clip as made).
	const Soldier& Ger = *Files["GER"];
	const int BallL = Ger.BoneIndex("ball_l"), BallR = Ger.BoneIndex("ball_r");
	std::map<std::pair<const Soldier*, std::string>, GaitFeet> Feet;
	for (const auto& KV : Files)
	{
		for (const Clip& Cl : KV.second->Clips)
		{
			if (Cl.bLoop && Cl.Stride > 0.f && Cl.Name != "crawl") Feet[{ KV.second.get(), Cl.Name }] = FeetOf(*KV.second, Cl, BallL, BallR);
		}
	}

	// ---- Play, 60 frames a second, as the scene would: the eye among the thickest knot of men, each man skinned at
	// the level of detail the crowd gives him and only when his Stamp says he moved.
	const double Dt = 1.0 / 60.0;
	const int Frames = static_cast<int>(std::ceil(End / Dt)) + 1;
	std::vector<std::vector<float>> At(static_cast<size_t>(N)), Facing(static_cast<size_t>(N));
	for (int I = 0; I < N; ++I)
	{
		At[static_cast<size_t>(I)].resize(static_cast<size_t>(C.PointsOf(I, Near)) * 3);
		Facing[static_cast<size_t>(I)].resize(At[static_cast<size_t>(I)].size());
	}
	std::vector<uint32_t> LastStamp(static_cast<size_t>(N), 0xFFFFFFFFu);
	std::vector<int> LastLod(static_cast<size_t>(N), -1);
	std::vector<double> UpdateMs, SkinMs, FrameMs;
	// The scene's own way (ALandBattle::DrawMen): only the men in the picture (a 16:9 lens of 45 degrees, its edge 15%
	// out, and any man within 6 m of the eye), each when his Stamp or his level changed since he was last skinned there;
	// timed on one core, and spread over the threads.
	std::vector<std::vector<float>> SceneAt(static_cast<size_t>(N)), SceneFacing(static_cast<size_t>(N));
	std::vector<uint32_t> SceneStamp(static_cast<size_t>(N), 0xFFFFFFFFu);
	std::vector<int> SceneLod(static_cast<size_t>(N), -1);
	std::vector<double> SceneMs, SceneWallMs;
	std::vector<int> Due;
	long SceneSkins = 0, InPicture = 0;
	Pool Workers(Threads);
	const double HalfWide = std::atan(std::tan(22.5 * 3.14159265358979 / 180.0) * 16.0 / 9.0 * 1.15);
	double LookF = 0.0;
	std::vector<int> NearCounts;
	int MostNear = 0, MostMid = 0;
	double SkinnedPoints = 0.0;
	long NanFrames = 0;
	// (GS3: each man's level as last updated and as the scene last drew it under the cap, when it last changed and
	// from what; the changes a frame, the men held back and the longest wait, the flickers, the shadow switches)
	std::vector<int> LevelWas(static_cast<size_t>(N), -1), LevelBefore(static_cast<size_t>(N), -1), DrawnLevel(static_cast<size_t>(N), -1);
	std::vector<double> LevelAt(static_cast<size_t>(N), -1e9);
	std::vector<int> HeldFrames(static_cast<size_t>(N), 0);
	long LevelChanges = 0, Flickers = 0, ShadowSwitches = 0, HeldTotal = 0, LevelCapFrames = 0, CulledTotal = 0;
	int MostChanges = 0, LongestHeld = 0, MostHeld = 0;
	double MostChangesAt = 0.0;

	// Deaths.
	struct Body
	{
		int FirstFallFrame = -1;
		bool bFrozen = false;
		uint32_t Stamp = 0;
		uint64_t Hash = 0;
		double X = 0, Y = 0, Z = 0;
		float F = 0;
		int Changed = 0, Removed = 0;
		double Rested = -1.0;
		double RawLow = 0.0, Low = 0.0, Head = 0.0, Pelvis = 0.0, High = 0.0;
		int Below = 0;
	};
	std::map<int, Body> Bodies;
	for (const int I : Dead) Bodies[I] = Body();

	// Shots and events.
	std::map<std::string, long> ByFx;
	struct Seen { double T; int Man; Weapon W; int Bullets; bool bShooting; };
	std::vector<Seen> Shots;
	long ShotBad = 0, ShotFar = 0;
	std::map<int, size_t> OpenBurst;  // man -> index into Shots of his burst in progress

	// Feet.
	struct FootState { bool bValid = false; Vec3 P[2]; std::string Clip; double X = 0, Y = 0; float F = 0; };
	std::vector<FootState> FootPrev(static_cast<size_t>(N));
	struct Slide { double World = 0.0, Expect = 0.0, Fwd = 0.0, FwdExpect = 0.0, Side = 0.0, Turn = 0.0, Ahead = 0.0; long Samples = 0; long Frames = 0;
		double SFwd = 0.0, SFwdExpect = 0.0; long SSamples = 0; };
	std::map<std::string, Slide> Uncapped, Capped;
	long GaitFrames = 0, CappedFrames = 0;
	std::map<std::string, long> ClipFrames;
	long ManFrames = 0;
	// Elbow room (the cut 2 film: knots of men passing through each other, pairs shoulder to shoulder in one pose, a man
	// stepping through a body): every tenth of a second, pairs of living men closer than 0.6 m and 0.3 m (a team's two at
	// their work left out: the same block and key man, one of them a loader, crewman, mortar, hmg, at, mg or fo), and
	// men on the move with their feet within 0.35 m of a body.
	long RoomSamples = 0, Close06 = 0, Close03 = 0, OnBody = 0, MovingSamples = 0;
	std::map<std::pair<int, int>, long> ClosePairs;
	struct BodyLine { double AX, AY, BX, BY; };
	std::map<int, BodyLine> BodyLines;
	auto TeamRole = [](const std::string& R) { return R == "loader" || R == "crew" || R == "mortar" || R == "hmg" || R == "at" || R == "mg" || R == "fo"; };

	const int Pelvis = Ger.BoneIndex("pelvis"), HeadB = Ger.BoneIndex("head");
	std::vector<float> Raw;
	double EyeX = 0, EyeY = 0;
	for (int Fr = 0; Fr < Frames; ++Fr)
	{
		const double T = Fr * Dt;
		// The eye: every half second, among the men with the most others within 25 m (the worst case for the near level).
		if (Fr % 30 == 0)
		{
			int Best = -1, BestN = -1;
			for (int I = 0; I < N; ++I)
			{
				if (!C.Visible(I)) continue;
				double X, Y, Z;
				float F;
				C.PlaceOf(I, X, Y, Z, F);
				int Near25 = 0;
				for (int K = 0; K < N; ++K)
				{
					double X2, Y2, Z2;
					float F2;
					C.PlaceOf(K, X2, Y2, Z2, F2);
					if ((X2 - X) * (X2 - X) + (Y2 - Y) * (Y2 - Y) < 625.0) ++Near25;
				}
				if (Near25 > BestN) { BestN = Near25; Best = I; }
			}
			if (Best >= 0)
			{
				double X, Y, Z;
				float F;
				C.PlaceOf(Best, X, Y, Z, F);
				EyeX = X - 6.0 * std::sin(F);
				EyeY = Y - 6.0 * std::cos(F);
				LookF = F;
			}
		}
		C.SetEye(EyeX, EyeY, Fd(EyeX, EyeY).Height + 1.7);

		const auto T0 = std::chrono::steady_clock::now();
		C.Update(T);
		const auto T1 = std::chrono::steady_clock::now();
		int NearN = 0, MidN = 0;
		for (int I = 0; I < N; ++I)
		{
			if (!C.Visible(I)) continue;
			const Lod L = C.LodOf(I);
			NearN += L == Near;
			MidN += L == Mid;
			const uint32_t St = C.Stamp(I);
			if (St == LastStamp[static_cast<size_t>(I)] && L == LastLod[static_cast<size_t>(I)]) continue;
			LastStamp[static_cast<size_t>(I)] = St;
			LastLod[static_cast<size_t>(I)] = L;
			SkinnedPoints += C.Skin(I, L, At[static_cast<size_t>(I)].data(), Facing[static_cast<size_t>(I)].data());
		}
		const auto T2 = std::chrono::steady_clock::now();
		if (bLevels)
		{
			int Changes = 0, Rebuilds = RebuildCap, Held = 0;
			for (int I = 0; I < N; ++I)
			{
				const size_t K = static_cast<size_t>(I);
				if (!C.Visible(I)) continue;
				if (C.SeenDistance(I) > CullFrom) { ++CulledTotal; continue; }
				const int L = static_cast<int>(C.LodOf(I));
				if (LevelWas[K] >= 0 && L != LevelWas[K])
				{
					++Changes;
					// (the test's eye jumps every half second, a cut: a change it made is no flicker, nor its undoing)
					const bool bCut = Fr % 30 == 0;
					if (!bCut && L == LevelBefore[K] && T - LevelAt[K] < 0.5)
					{
						++Flickers;
						if (Flickers <= 3)
						{
							std::printf("  flicker: man %d back to level %d at %.2f s, %.2f s after leaving it (seen %.2f m)\n", I, L, T, T - LevelAt[K], C.SeenDistance(I));
						}
					}
					LevelBefore[K] = LevelWas[K];
					LevelAt[K] = bCut ? -1e9 : T;
				}
				LevelWas[K] = L;
				// (the scene's DrawMen: a man whose drawn level differs is made again while the frame's cap lasts; past it,
				// kept at his old level, and a new man not drawn yet)
				if (DrawnLevel[K] != L)
				{
					if (Rebuilds > 0)
					{
						--Rebuilds;
						if (DrawnLevel[K] >= 0 && (DrawnLevel[K] == static_cast<int>(Far)) != (L == static_cast<int>(Far))) ++ShadowSwitches;
						DrawnLevel[K] = L;
						HeldFrames[K] = 0;
					}
					else
					{
						++Held;
						LongestHeld = std::max(LongestHeld, ++HeldFrames[K]);
					}
				}
			}
			LevelChanges += Changes;
			HeldTotal += Held;
			LevelCapFrames += Held > 0;
			MostHeld = std::max(MostHeld, Held);
			if (Changes > MostChanges) { MostChanges = Changes; MostChangesAt = T; }
		}
		UpdateMs.push_back(Ms(T0, T1));
		SkinMs.push_back(Ms(T1, T2));
		FrameMs.push_back(Ms(T0, T2));
		{
			// The scene's men this frame: in the picture and changed since last drawn.
			Due.clear();
			for (int I = 0; I < N; ++I)
			{
				if (!C.Visible(I) || C.Fade(I) < 0.5f) continue;
				double X, Y, Z;
				float F;
				C.PlaceOf(I, X, Y, Z, F);
				const double DX = X - EyeX, DY = Y - EyeY;
				const double Ahead = DX * std::sin(LookF) + DY * std::cos(LookF);
				const double Side = DX * std::cos(LookF) - DY * std::sin(LookF);
				const bool bPicture = DX * DX + DY * DY < 36.0 || (Ahead > 0.5 && std::fabs(std::atan2(Side, Ahead)) < HalfWide);
				if (!bPicture) continue;
				++InPicture;
				const Lod L = C.LodOf(I);
				if (C.Stamp(I) == SceneStamp[static_cast<size_t>(I)] && static_cast<int>(L) == SceneLod[static_cast<size_t>(I)]) continue;
				SceneStamp[static_cast<size_t>(I)] = C.Stamp(I);
				SceneLod[static_cast<size_t>(I)] = static_cast<int>(L);
				if (SceneAt[static_cast<size_t>(I)].empty())
				{
					SceneAt[static_cast<size_t>(I)].resize(static_cast<size_t>(C.PointsOf(I, Near)) * 3);
					SceneFacing[static_cast<size_t>(I)].resize(SceneAt[static_cast<size_t>(I)].size());
				}
				Due.push_back(I);
			}
			SceneSkins += static_cast<long>(Due.size());
			// (a body's first skin at a level reads the ground: done here first, on this thread, as the scene does)
			for (const int I : Due)
			{
				const Lod L = C.LodOf(I);
				if (C.SkinReadsGround(I, L)) C.Skin(I, L, SceneAt[static_cast<size_t>(I)].data(), SceneFacing[static_cast<size_t>(I)].data());
			}
			auto One = [&](int K) {
				const int I = Due[static_cast<size_t>(K)];
				C.Skin(I, C.LodOf(I), SceneAt[static_cast<size_t>(I)].data(), SceneFacing[static_cast<size_t>(I)].data());
			};
			const std::function<void(int)> Body = One;
			// (one core and the threads in turn first, so neither always finds the other's work warm in the caches)
			double OneMs = 0.0, WallMs = 0.0;
			for (int Pass = 0; Pass < 2; ++Pass)
			{
				const bool bThreads = (Pass == 0) == (Fr % 2 == 1);
				const auto A = std::chrono::steady_clock::now();
				if (bThreads) Workers.For(static_cast<int>(Due.size()), Body);
				else for (int K = 0; K < static_cast<int>(Due.size()); ++K) One(K);
				const auto B = std::chrono::steady_clock::now();
				(bThreads ? WallMs : OneMs) = Ms(A, B);
			}
			SceneMs.push_back(Ms(T0, T1) + OneMs);
			SceneWallMs.push_back(Ms(T0, T1) + WallMs);
		}
		NearCounts.push_back(NearN);
		MostNear = std::max(MostNear, NearN);
		MostMid = std::max(MostMid, MidN);

		// (Below here: the checks, out of the timing.)
		bool bNan = false;
		for (int I = 0; I < N; ++I)
		{
			if (!C.Visible(I) || LastLod[static_cast<size_t>(I)] < 0) continue;
			const int P = C.PointsOf(I, static_cast<Lod>(LastLod[static_cast<size_t>(I)])) * 3;
			if (Fr % 10 == 0 || LastStamp[static_cast<size_t>(I)] == C.Stamp(I))
			{
				for (int K = 0; K < P; ++K)
				{
					if (!std::isfinite(At[static_cast<size_t>(I)][static_cast<size_t>(K)]) || !std::isfinite(Facing[static_cast<size_t>(I)][static_cast<size_t>(K)])) { bNan = true; break; }
				}
			}
		}
		if (bNan) { ++NanFrames; if (NanFrames < 4) Fail("NaN in a skinned man at %.2f s", T); }

		// Events.
		for (const Event& Ev : C.Events())
		{
			ByFx[Ev.Fx]++;
			if (!Finite(Ev.At) || !Finite(Ev.Dir)) { Fail("NaN in event %s at %.2f s", Ev.Fx.c_str(), Ev.T); continue; }
			const bool bShot = Ev.Fx == "muzzle_flash" || Ev.Fx == "mortar" || Ev.Fx == "grenade_release";
			if (!bShot) continue;
			double X, Y, Z;
			float F;
			C.PlaceOf(Ev.Man, X, Y, Z, F);
			if (std::hypot(Ev.At.X - X, Ev.At.Y - Y) > 2.5 || std::fabs(Ev.At.Z - Z) > 2.5) ++ShotFar;
			const float DL = std::sqrt(Ev.Dir.X * Ev.Dir.X + Ev.Dir.Y * Ev.Dir.Y + Ev.Dir.Z * Ev.Dir.Z);
			if (std::fabs(DL - 1.f) > 0.01f) ++ShotBad;
			if (Ev.Round == 0)
			{
				OpenBurst[Ev.Man] = Shots.size();
				Shots.push_back({ Ev.T, Ev.Man, Ev.Gun, 1, C.PlayingOf(Ev.Man).bShooting });
				if (Ev.Cue.empty()) Fail("a round's first shot has no cue (man %d at %.2f s)", Ev.Man, Ev.T);
			}
			else
			{
				auto Open = OpenBurst.find(Ev.Man);
				if (Open == OpenBurst.end()) { Fail("a burst's later bullet without its first (man %d at %.2f s)", Ev.Man, Ev.T); continue; }
				Seen& S = Shots[Open->second];
				if (std::fabs(Ev.T - (S.T + 0.1 / Life * Ev.Round)) > 1e-6) Fail("a burst's bullet off its time (man %d at %.2f s)", Ev.Man, Ev.T);
				if (!Ev.Cue.empty()) Fail("a burst's later bullet has a cue (man %d at %.2f s)", Ev.Man, Ev.T);
				S.Bullets = std::max(S.Bullets, Ev.Round + 1);
			}
		}

		// Deaths: the fall starts on the first frame at or after his time, he lies, freezes and stays.
		for (auto& KV : Bodies)
		{
			const int I = KV.first;
			Body& B = KV.second;
			const ManInfo& In = C.Info(I);
			if (!C.Visible(I) || C.Fade(I) < 1.f) ++B.Removed;
			if (B.FirstFallFrame < 0 && In.FallStarted >= 0.0) B.FirstFallFrame = Fr;
			if (!B.bFrozen && C.Frozen(I))
			{
				B.bFrozen = true;
				B.Rested = In.Rested;
				B.Stamp = C.Stamp(I);
				std::vector<float> P(static_cast<size_t>(C.PointsOf(I, Near)) * 3), Fa(P.size());
				const int Pts = C.Skin(I, Near, P.data(), Fa.data());
				B.Hash = Hash(P.data(), static_cast<size_t>(Pts) * 3);
				C.PlaceOf(I, B.X, B.Y, B.Z, B.F);
				B.Low = 1e9;
				B.High = -1e9;
				for (int K = 0; K < Pts; ++K)
				{
					const float* Q = &P[static_cast<size_t>(K) * 3];
					const double Over = Q[2] - Fd(Q[0], Q[1]).Height;
					B.Low = std::min(B.Low, Over);
					B.High = std::max(B.High, Over);
					if (Over < -0.02) ++B.Below;
				}
				// As the fall leaves him, before he is laid on the ground: the whole near mesh by his frames.
				const Soldier& S = *C.SoldierOf(I);
				Raw.resize(static_cast<size_t>(S.Meshes[Near].Points) * 3);
				SkinMesh(S.Meshes[Near], C.FramesOf(I), Raw.data(), nullptr);
				B.RawLow = 1e9;
				for (int K = 0; K < S.Meshes[Near].Points; ++K)
				{
					const float* Q = &Raw[static_cast<size_t>(K) * 3];
					B.RawLow = std::min(B.RawLow, static_cast<double>(Q[2] - Fd(Q[0], Q[1]).Height));
				}
				const Xform* Fm = C.FramesOf(I);
				const Vec3 H = Fm[HeadB].Point(S.Bones[static_cast<size_t>(HeadB)].Head);
				const Vec3 Pv = Fm[Pelvis].Point(S.Bones[static_cast<size_t>(Pelvis)].Head);
				B.Head = H.Z - Fd(H.X, H.Y).Height;
				B.Pelvis = Pv.Z - Fd(Pv.X, Pv.Y).Height;
				// The other levels are laid on the ground too.
				for (const Lod L : { Mid, Far })
				{
					std::vector<float> Q(static_cast<size_t>(C.PointsOf(I, L)) * 3);
					const int Pl = C.Skin(I, L, Q.data(), nullptr);
					for (int K = 0; K < Pl; ++K)
					{
						if (Q[static_cast<size_t>(K) * 3 + 2] - Fd(Q[static_cast<size_t>(K) * 3], Q[static_cast<size_t>(K) * 3 + 1]).Height < -0.02) ++B.Below;
					}
				}
			}
			else if (B.bFrozen)
			{
				double X, Y, Z;
				float F;
				C.PlaceOf(I, X, Y, Z, F);
				bool bChanged = C.Stamp(I) != B.Stamp || !C.Frozen(I) || X != B.X || Y != B.Y || F != B.F;
				if (Fr % 60 == 0)
				{
					std::vector<float> P(static_cast<size_t>(C.PointsOf(I, Near)) * 3);
					const int Pts = C.Skin(I, Near, P.data(), nullptr);
					bChanged = bChanged || Hash(P.data(), static_cast<size_t>(Pts) * 3) != B.Hash;
				}
				if (bChanged) ++B.Changed;
			}
		}

		if (Fr % 6 == 0)
		{
			struct At2 { double X, Y; int I; };
			std::map<int64_t, std::vector<At2>> Grid;
			auto Cell = [](double X, double Y) { return (static_cast<int64_t>(std::floor(X)) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(static_cast<int32_t>(std::floor(Y)))); };
			std::vector<At2> Living;
			for (int I = 0; I < N; ++I)
			{
				if (!C.Visible(I)) continue;
				if (C.Frozen(I))
				{
					if (!BodyLines.count(I))
					{
						// (a body as a line from his head through his pelvis to his feet)
						const Soldier& Sd = *C.SoldierOf(I);
						const Xform* Fm = C.FramesOf(I);
						const Vec3 H = Fm[HeadB].Point(Sd.Bones[static_cast<size_t>(HeadB)].Head);
						const Vec3 Pv = Fm[Pelvis].Point(Sd.Bones[static_cast<size_t>(Pelvis)].Head);
						BodyLines[I] = { H.X, H.Y, Pv.X - 0.95 * (H.X - Pv.X), Pv.Y - 0.95 * (H.Y - Pv.Y) };
					}
					continue;
				}
				if (C.IsFalling(I)) continue;
				double X, Y, Z;
				float F;
				C.PlaceOf(I, X, Y, Z, F);
				Living.push_back({ X, Y, I });
				Grid[Cell(X, Y)].push_back({ X, Y, I });
			}
			for (const At2& A : Living)
			{
				++RoomSamples;
				for (int Dy = -1; Dy <= 1; ++Dy)
					for (int Dx = -1; Dx <= 1; ++Dx)
					{
						const auto Found = Grid.find(Cell(A.X + Dx, A.Y + Dy));
						if (Found == Grid.end()) continue;
						for (const At2& B : Found->second)
						{
							if (B.I <= A.I) continue;
							const double D = std::hypot(A.X - B.X, A.Y - B.Y);
							if (D >= 0.6) continue;
							const ManInfo& Ia = C.Info(A.I);
							const ManInfo& Ib = C.Info(B.I);
							if (Ia.Block == Ib.Block && Ia.K == Ib.K && (TeamRole(Ia.Role) || TeamRole(Ib.Role))) continue;
							++Close06;
							if (D < 0.3) ++Close03;
							++ClosePairs[{ A.I, B.I }];
						}
					}
				const Playing Pl = C.PlayingOf(A.I);
				if (!Pl.bGait || Pl.Speed < 0.5f) continue;
				++MovingSamples;
				for (const auto& KV : BodyLines)
				{
					const BodyLine& L = KV.second;
					const double Vx = L.BX - L.AX, Vy = L.BY - L.AY;
					const double Len2 = std::max(1e-9, Vx * Vx + Vy * Vy);
					const double U = std::clamp(((A.X - L.AX) * Vx + (A.Y - L.AY) * Vy) / Len2, 0.0, 1.0);
					if (std::hypot(A.X - (L.AX + U * Vx), A.Y - (L.AY + U * Vy)) < 0.35) { ++OnBody; break; }
				}
			}
		}

		// Clips shown, and the feet on the ground.
		for (int I = 0; I < N; ++I)
		{
			if (!C.Visible(I)) continue;
			const Playing Pl = C.PlayingOf(I);
			++ManFrames;
			ClipFrames[Pl.bShooting ? std::string("(shot) ") + Pl.Clip : Pl.Clip]++;
			FootState& FS = FootPrev[static_cast<size_t>(I)];
			const bool bAlive = C.Info(I).FallStarted < 0.0;
			if (!bAlive || !Pl.bGait || Pl.bBlending || Pl.bShooting || Pl.Speed < 0.3f || Pl.Clip == "crawl") { FS.bValid = false; continue; }
			++GaitFrames;
			if (Pl.bCapped) ++CappedFrames;
			const Soldier& S = *C.SoldierOf(I);
			const Xform* Fm = C.FramesOf(I);
			Vec3 P[2] = { Fm[BallL].Point(S.Bones[static_cast<size_t>(BallL)].Head), Fm[BallR].Point(S.Bones[static_cast<size_t>(BallR)].Head) };
			const auto Found = Feet.find({ &S, Pl.Clip });
			if (FS.bValid && FS.Clip == Pl.Clip && Found != Feet.end())
			{
				Slide& Sl = (Pl.bCapped ? Capped : Uncapped)[Pl.Clip];
				++Sl.Frames;
				// (How the root itself went: sideways to his facing, turning, and ahead against his speed.)
				double X, Y, Z;
				float Fc;
				C.PlaceOf(I, X, Y, Z, Fc);
				const double Fx = std::sin(FS.F), Fy = std::cos(FS.F);
				const double VX = (X - FS.X) / Dt, VY = (Y - FS.Y) / Dt;
				const double Side = std::fabs(VX * Fy - VY * Fx);
				const double Turning = std::fabs(std::remainder(static_cast<double>(Fc - FS.F), 2.0 * 3.141592653589793)) / Dt;
				Sl.Side += Side;
				Sl.Ahead += (VX * Fx + VY * Fy) - Pl.Speed;
				Sl.Turn += Turning;
				// Going straight: hardly turning, hardly sideways (where the play rate alone decides the feet).
				const bool bStraight = Turning < 0.25 && Side < 0.15;
				for (int F = 0; F < 2; ++F)
				{
					const double H0 = FS.P[F].Z - Fd(FS.P[F].X, FS.P[F].Y).Height;
					const double H1 = P[F].Z - Fd(P[F].X, P[F].Y).Height;
					const double Lo = Found->second.Low[F] + 0.02;
					if (H0 < Lo && H1 < Lo)
					{
						Sl.World += std::hypot(P[F].X - FS.P[F].X, P[F].Y - FS.P[F].Y) / Dt;
						Sl.Expect += Found->second.Slide * Pl.Rate;
						Sl.Fwd += std::fabs((P[F].X - FS.P[F].X) * Fx + (P[F].Y - FS.P[F].Y) * Fy) / Dt;
						Sl.FwdExpect += Found->second.Ahead * Pl.Rate;
						++Sl.Samples;
						if (bStraight)
						{
							Sl.SFwd += std::fabs((P[F].X - FS.P[F].X) * Fx + (P[F].Y - FS.P[F].Y) * Fy) / Dt;
							Sl.SFwdExpect += Found->second.Ahead * Pl.Rate;
							++Sl.SSamples;
						}
					}
				}
			}
			FS.bValid = true;
			FS.Clip = Pl.Clip;
			FS.P[0] = P[0];
			FS.P[1] = P[1];
			double Z;
			C.PlaceOf(I, FS.X, FS.Y, Z, FS.F);
		}
	}

	// ---- The report.
	std::printf("\nplayed %.1f s at 60 frames a second (%d frames), %d men\n", End, Frames, N);
	{
		long Long = 0;
		for (const auto& KV : ClosePairs) Long += KV.second >= 10; // (a pair closer than 0.6 m for a second or more)
		std::printf("elbow room: of %ld looks at a living man, %.2f%% had another within 0.6 m and %.2f%% within 0.3 m (%zu pairs, %ld of them for a second or more); %.2f%% of %ld looks at a man on the move had his feet on a body\n",
			RoomSamples, 100.0 * Close06 / std::max(1L, RoomSamples), 100.0 * Close03 / std::max(1L, RoomSamples), ClosePairs.size(), Long,
			100.0 * OnBody / std::max(1L, MovingSamples), MovingSamples);
		// (on the drill's spacing, 3f1c2c29: 0.09% and 0.03%; the second cut's recording had knots of five and six)
		if (Close06 > 0.005 * RoomSamples) Fail("living men stand closer than 0.6 m on %.2f%% of looks (more than 0.5%%)", 100.0 * Close06 / std::max(1L, RoomSamples));
		if (OnBody > 0.002 * MovingSamples) Fail("men on the move have their feet on a body on %.2f%% of looks (more than 0.2%%)", 100.0 * OnBody / std::max(1L, MovingSamples));
	}

	// Deaths.
	std::printf("\ndeaths (%zu): man, army, when, fall (stance, kind, from), fall start frame vs d, rest after, lying (head, pelvis, highest point over the ground), clip's own lowest point, laid lowest\n", Dead.size());
	int DeathFails = 0;
	for (const auto& KV : Bodies)
	{
		const int I = KV.first;
		const Body& B = KV.second;
		const ManInfo& In = C.Info(I);
		const int Want = static_cast<int>(std::ceil(In.Dead / Dt - 1e-9));
		std::printf("  %3d %s b%d k%-2d d %6.2f  %-11s (%s, %s, %s)  frame %d vs %d  rest +%.2f s  head %.2f pelvis %.2f top %.2f m  raw %+.3f  laid %+.3f\n",
			I, In.Army.c_str(), In.Block, In.K, In.Dead, In.FallClip.c_str(), In.FallStance.c_str(), In.FallKind.c_str(), In.FallFrom.c_str(),
			B.FirstFallFrame, Want, B.Rested - In.Dead, B.Head, B.Pelvis, B.High, B.RawLow, B.Low);
		const int Before = DeathFails;
		if (B.FirstFallFrame < 0 || std::abs(B.FirstFallFrame - Want) > 1) { Fail("man %d's fall started on frame %d, his d is frame %d", I, B.FirstFallFrame, Want); ++DeathFails; }
		if (!B.bFrozen) { Fail("man %d never came to rest", I); ++DeathFails; continue; }
		if (B.Low > 0.02 || B.Low < -0.02) { Fail("man %d's lowest point lies %.3f m off the ground", I, B.Low); ++DeathFails; }
		if (B.Below > 0) { Fail("man %d has %d points more than 2 cm under the ground", I, B.Below); ++DeathFails; }
		if (B.Head > 0.5 || B.Pelvis > 0.4) { Fail("man %d is not lying (head %.2f m, pelvis %.2f m up)", I, B.Head, B.Pelvis); ++DeathFails; }
		if (B.Changed > 0) { Fail("man %d moved or changed %d times after he froze", I, B.Changed); ++DeathFails; }
		if (B.Removed > 0) { Fail("man %d was hidden or faded on %d frames", I, B.Removed); ++DeathFails; }
		(void)Before;
	}
	std::printf("  %s\n", DeathFails ? "DEATHS FAIL" : "deaths ok: every fall starts on its frame, every body lies on the ground and never moves, none removed");
	// A shell hole torn under a body already lying (the game's MakeCraters calls Unsettle with the hole's reach): he is
	// laid again on its rim, nothing of him under it, and the bodies beyond its reach are left as they were.
	{
		int Body = -1;
		for (const auto& KV : Bodies) if (C.Frozen(KV.first)) { Body = KV.first; break; }
		if (Body < 0) Fail("no body at rest to tear a hole under");
		else
		{
			double X, Y, Z;
			float Fc;
			C.PlaceOf(Body, X, Y, Z, Fc);
			{
				// (from where his body lies as skinned, not his place: a fall that carries him a metre or two, a blast's or
				// a run's, left the rim's ring wholly off him, so the check had nothing to look at)
				std::vector<float> Was_(static_cast<size_t>(C.PointsOf(Body, Near)) * 3);
				const int N_ = C.Skin(Body, Near, Was_.data(), nullptr);
				if (N_ > 0)
				{
					double Sx = 0.0, Sy = 0.0;
					for (int K = 0; K < N_; ++K) { Sx += Was_[static_cast<size_t>(K) * 3]; Sy += Was_[static_cast<size_t>(K) * 3 + 1]; }
					X = Sx / N_;
					Y = Sy / N_;
				}
			}
			RimX = X + 0.9;
			RimY = Y;
			RimH = 0.25;
			const double Reach = 2.2;
			std::map<int, uint32_t> Beyond;
			for (const auto& KV : Bodies)
			{
				double Bx, By, Bz;
				float Bf;
				C.PlaceOf(KV.first, Bx, By, Bz, Bf);
				if ((Bx - RimX) * (Bx - RimX) + (By - RimY) * (By - RimY) > (Reach + 8.0) * (Reach + 8.0)) Beyond[KV.first] = C.Stamp(KV.first); // (a thrown body lies up to 7 m off his root)
			}
			const uint32_t Was = C.Stamp(Body);
			const int Laid = C.Unsettle(RimX, RimY, Reach);
			const bool bReads = C.SkinReadsGround(Body, Near);
			std::vector<float> P(static_cast<size_t>(C.PointsOf(Body, Near)) * 3);
			const int Pts = C.Skin(Body, Near, P.data(), nullptr);
			int Below = 0, OnRim = 0;
			double Low = 1e9;
			for (int K = 0; K < Pts; ++K)
			{
				const float* Q = &P[static_cast<size_t>(K) * 3];
				const double Ground_ = GroundAt(Q[0], Q[1]).Height;
				const double Over = Q[2] - Ground_;
				Low = std::min(Low, Over);
				if (Over < -0.02) ++Below;
				if (Ground_ - Fd(Q[0], Q[1]).Height > 0.05 && Over < 0.02) ++OnRim;
			}
			int Moved = 0;
			for (const auto& KV : Beyond) Moved += C.Stamp(KV.first) != KV.second;
			RimH = 0.0;
			std::printf("  a hole torn under body %d: %d laid again; %d of his points on its rim, %d more than 2 cm under it, the lowest %+.3f m; %d of %zu bodies beyond its reach moved\n",
				Body, Laid, OnRim, Below, Low, Moved, Beyond.size());
			if (Laid < 1 || C.Stamp(Body) == Was || !bReads) Fail("a body under a new shell hole was not laid again");
			if (Below > 0 || OnRim == 0) Fail("a body under a new shell hole is not laid on its rim (%d points under, %d on it)", Below, OnRim);
			if (Moved > 0) Fail("%d bodies beyond a new shell hole's reach were laid again", Moved);
		}
	}

	// Shots.
	std::map<std::string, long> Want, Got;
	for (const Booked& R : Rounds) Want[R.W]++;
	for (const Seen& S : Shots) Got[WeaponName(S.W)]++;
	std::printf("\nshots: the recording's rounds by the men %zu, shots out %zu (", Rounds.size(), Shots.size());
	for (const auto& KV : Want) std::printf(" %s %ld/%ld", KV.first.c_str(), Got[KV.first], KV.second);
	std::printf(" )\n");
	if (Shots.size() != Rounds.size()) Fail("%zu rounds booked, %zu shots came out", Rounds.size(), Shots.size());
	{
		// Each round's shot at its own time from its own man.
		std::multiset<std::pair<int, long long>> Expect;
		for (const Booked& R : Rounds) Expect.insert({ R.Man, std::llround(R.T * 1e6) });
		long Off = 0;
		for (const Seen& S : Shots)
		{
			auto It = Expect.find({ S.Man, std::llround(S.T * 1e6) });
			if (It == Expect.end()) ++Off;
			else Expect.erase(It);
		}
		if (Off) Fail("%ld shots not on a round's time", Off);
		std::map<int, long> Bursts;
		long Aimed = 0, Posed = 0;
		for (const Seen& S : Shots)
		{
			if (S.W == Weapon::Mg) Bursts[S.Bullets]++;
		}
		for (const Seen& S : Shots)
		{
			if (S.W == Weapon::Mortar) continue;
			++Posed;
			if (S.bShooting) ++Aimed;
		}
		std::printf("  every shot on its round's time from its man: %s; mg bursts by bullets:", Off ? "NO" : "yes");
		for (const auto& KV : Bursts) std::printf(" %d:%ld", KV.first, KV.second);
		std::printf("\n  rifle and mg shots played in a firing clip (the rest are fired on the move): %ld of %ld (%.0f%%)\n", Aimed, Posed, Posed ? 100.0 * Aimed / Posed : 0.0);
		std::printf("  shot events with a bad direction %ld, more than 2.5 m from the man %ld\n", ShotBad, ShotFar);
		if (ShotBad) Fail("%ld shot directions are not unit", ShotBad);
		if (ShotFar) Fail("%ld shots far from their man", ShotFar);
	}
	std::printf("  events:");
	for (const auto& KV : ByFx) std::printf(" %s %ld;", KV.first.c_str(), KV.second);
	std::printf("\n");

	// The clip's own shots, for comparison (bClipShots): the first minute.
	{
		Crowd C2;
		C2.SetLife(Life);
		for (const auto& KV : Files) C2.AddSoldier(KV.first, KV.second);
		C2.ReadArmies(ReadFile(Argv[3]), Err);
		C2.bClipShots = true;
		C2.Build(RecText, GroundAt, Err);
		long Flash = 0, Plain = 0;
		for (int Fr = 0; Fr * Dt <= 60.0; ++Fr)
		{
			C2.Update(Fr * Dt);
			for (const Event& Ev : C2.Events()) Flash += Ev.Fx == "muzzle_flash";
		}
		for (const Seen& S : Shots) Plain += S.T <= 60.0 && S.W != Weapon::Mortar;
		std::printf("  bClipShots (the clips' own flashes back on), first minute: %ld muzzle flashes, against %ld rounds' shots without it\n", Flash, Plain);
	}

	// ---- What the Normandy recording does not have: a small battle of its own on flat ground. Men who leave with
	// their block fade out (a dead man never does), a grenade's release on its round's time, a round fired from wait,
	// an mg burst booked through Fire, and the deaths map (a copy of the German file given one) with the Fates rule.
	{
		std::ostringstream R;
		R << "{\"seed\": 5, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.1, -1.1]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}], \"men\": [";
		// Block 1: k0 leaves at 4 s; k1 dies at 1.5 s and his block leaves at 4 s; k2 throws a grenade at 6 s; k3 fires
		// from wait at 5 s; k4 is given an mg burst at 7 s by Fire. (k1 dies before block 2's first, at 2 s: the Fates rule
		// runs over the whole crowd's falls in turn, so a fall of his between two of theirs would let theirs repeat.)
		const char* Poses[5] = { "advance", "fire_stand", "fire_stand", "wait", "hide" };
		for (int K = 0; K < 5; ++K)
		{
			R << (K ? "," : "") << "{\"b\": 1, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[0, " << 10 * K << ", 0, 0], [20, " << 10 * K
			  << (K == 0 ? ", 60" : ", 0") << ", 0]], \"ch\": [[0, \"" << Poses[K] << "\", \"none\", 0, 0, 0, 0]]";
			if (K == 0 || K == 1) R << ", \"out\": 4";
			if (K == 1) R << ", \"d\": 1.5";
			R << "}";
		}
		// Block 2: twelve Germans standing at their rifles, killed one by one (the deaths map and the Fates rule).
		for (int K = 0; K < 12; ++K)
		{
			R << ",{\"b\": 2, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[0, " << 10 * K << ", 100, 3.1416], [30, " << 10 * K
			  << ", 100, 3.1416]], \"ch\": [[0, \"" << (K % 3 == 2 ? "wait" : "fire_stand") << "\", \"none\", 0, 0, 0, 0]], \"d\": " << 2.0 + K << "}";
		}
		R << "], \"rounds\": [{\"t\": 6, \"by\": 1, \"k\": 2, \"w\": \"grenade\", \"on\": 2, \"r\": \"miss\"}, {\"t\": 5, \"by\": 1, \"k\": 3, \"w\": \"rifle\", \"on\": 2, \"r\": \"miss\"}]}";

		auto Mapped = std::make_shared<Soldier>(*Files["GER"]);
		Mapped->Deaths["stand"] = { { "back", { "die_back" } }, { "forward", { "die_forward" } }, { "knees", { "die_kneel" } } };
		Mapped->Deaths["kneel"] = { { "knees", { "die_kneel" } }, { "forward", { "die_forward" } } };
		Crowd S;
		S.AddSoldier("GER", Mapped);
		S.AddSoldier("POL", Files["POL"]);
		S.ReadArmies(ReadFile(Argv[3]), Err);
		const GroundFn Flat = [](double, double) { return Ground(); };
		if (!S.Build(R.str(), Flat, Err)) { Fail("the small battle: %s", Err.c_str()); }
		else
		{
			const int Leaver = S.Find(1, 0), Body = S.Find(1, 1), Thrower = S.Find(1, 2), Waiter = S.Find(1, 3), Gunner = S.Find(1, 4);
			S.SetFallKind(S.Find(2, 7), "knees");
			S.Fire(Gunner, 7.0, Weapon::Mg);
			double HalfFade = -1.0, Gone = -1.0, Release = -1.0, Flash = -1.0;
			bool bThrowing = false, bBodyHidden = false;
			int Bullets = 0;
			std::string FlashCue, BurstCue;
			for (int Fr = 0; Fr * Dt <= 16.0; ++Fr)
			{
				const double T = Fr * Dt;
				S.Update(T);
				if (HalfFade < 0.0 && S.Fade(Leaver) <= 0.5f) HalfFade = T - (4.0 + S.Info(Leaver).Delay);
				if (Gone < 0.0 && !S.Visible(Leaver)) Gone = T - (4.0 + S.Info(Leaver).Delay);
				if (!S.Visible(Body) || S.Fade(Body) < 1.f) bBodyHidden = true;
				for (const Event& Ev : S.Events())
				{
					if (Ev.Man == Thrower && Ev.Fx == "grenade_release") { Release = Ev.T; bThrowing = S.PlayingOf(Thrower).bShooting; }
					if (Ev.Man == Waiter && Ev.Fx == "muzzle_flash") { Flash = Ev.T; FlashCue = Ev.Cue; }
					if (Ev.Man == Gunner && Ev.Fx == "muzzle_flash") { ++Bullets; if (Ev.Round == 0) BurstCue = Ev.Cue; }
				}
			}
			std::printf("\nsmall battle: a man who left faded to half %.2f s after leaving and was gone at %.2f s; the body of a man whose block left %s\n",
				HalfFade, Gone, bBodyHidden ? "WAS HIDDEN" : "stayed");
			std::printf("  grenade released at %.3f s (round 6.000, throwing: %s); a round from wait flashed at %.3f s (%s); a booked mg burst of %d bullets (%s)\n",
				Release, bThrowing ? "yes" : "no", Flash, FlashCue.c_str(), Bullets, BurstCue.c_str());
			if (HalfFade < 0.4 || HalfFade > 0.6 || Gone < 0.9 || Gone > 1.1) Fail("the man who left did not fade over a second (half %.2f, gone %.2f)", HalfFade, Gone);
			if (bBodyHidden) Fail("a dead man was hidden when his block left");
			if (std::fabs(Release - 6.0) > 1e-6 || !bThrowing) Fail("the grenade was not released on its round (%.3f s)", Release);
			if (std::fabs(Flash - 5.0) > 1e-6 || FlashCue != "rifle_shot") Fail("the round from wait did not flash on its time (%.3f s)", Flash);
			if (Bullets < 6 || Bullets > 10 || BurstCue != "lmg_burst") Fail("the booked burst had %d bullets", Bullets);
			std::printf("  the deaths map's falls in order (kind):");
			std::string Last;
			int Repeats = 0;
			for (int K = 0; K < 12; ++K)
			{
				const ManInfo& In = S.Info(S.Find(2, K));
				std::printf(" %s (%s)", In.FallClip.c_str(), In.FallKind.c_str());
				if (In.FallClip == Last) ++Repeats;
				Last = In.FallClip;
			}
			std::printf("\n");
			if (Repeats) Fail("the same fall twice running %d times (the Fates rule)", Repeats);
			if (S.Info(S.Find(2, 7)).FallKind != "knees") Fail("the fall kind the lead asked for was not taken");
		}
	}

	// Feet.
	std::printf("\nfeet: %ld gait man-frames, %ld (%.0f%%) at the 1.6 cap (the recording's men move faster than the clips can step, so their feet slide by design)\n",
		GaitFrames, CappedFrames, GaitFrames ? 100.0 * CappedFrames / GaitFrames : 0.0);
	std::printf("  a planted foot's slide, m/s (the clip's own as made, scaled by the rate it plays at, in brackets):\n");
	std::printf("  gait      uncapped: going straight, along his forward (what the rate governs) | all, along | all, in all | root sideways, turning    capped: in all\n");
	for (const auto& KV : Feet)
	{
		if (KV.first.first != &Ger) continue;
		const std::string& Name = KV.first.second;
		const Slide& U = Uncapped[Name];
		const Slide& Cp = Capped[Name];
		if (!U.Samples && !Cp.Samples) continue;
		const double Ns = std::max(1L, U.Samples), Nf = std::max(1L, U.Frames), Nst = std::max(1L, U.SSamples);
		std::printf("  %-8s  %.3f (%.3f) [%ld] | %.3f (%.3f) | %.3f (%.3f) [%ld] | %.2f m/s, %.2f rad/s    %.2f  [%ld]\n", Name.c_str(), U.SFwd / Nst,
			U.SFwdExpect / Nst, U.SSamples, U.Fwd / Ns, U.FwdExpect / Ns, U.World / Ns, U.Expect / Ns, U.Samples, U.Side / Nf, U.Turn / Nf,
			Cp.Samples ? Cp.World / Cp.Samples : 0.0, Cp.Samples);
		// Skating: the planted foot sliding along his way faster than the clip itself does at that rate, by more than
		// 10 cm a second. (Sideways and turning slide come from the recording's track, which zig-zags a little between
		// its kept points; the rate cannot answer for them.)
		if (U.SSamples > 50 && U.SFwd / Nst > U.SFwdExpect / Nst + 0.10)
			Fail("%s skates: %.3f m/s planted along his way against %.3f m/s the clip's own", Name.c_str(), U.SFwd / Nst, U.SFwdExpect / Nst);
	}

	// Clips shown.
	{
		std::vector<std::pair<long, std::string>> V;
		for (const auto& KV : ClipFrames) V.push_back({ KV.second, KV.first });
		std::sort(V.rbegin(), V.rend());
		std::printf("\nclips shown (share of man-frames):");
		for (size_t K = 0; K < V.size() && K < 16; ++K) std::printf(" %s %.1f%%;", V[K].second.c_str(), 100.0 * V[K].first / std::max(1L, ManFrames));
		std::printf("\n");
		// Run styles: the men on the run show the four runners, none of them the most by far; off, only the file's own.
		long Runs[4] = { 0, 0, 0, 0 };
		for (const auto& KV : ClipFrames)
		{
			const std::string& Nm = KV.first;
			for (const char* Base : { "run", "advance" })
			{
				const std::string B(Base);
				if (Nm == B) Runs[0] += KV.second;
				else if (Nm == B + "_b") Runs[1] += KV.second;
				else if (Nm == B + "_c") Runs[2] += KV.second;
				else if (Nm == B + "_d") Runs[3] += KV.second;
			}
		}
		const long AllRuns = Runs[0] + Runs[1] + Runs[2] + Runs[3];
		std::printf("run styles: of %ld man-frames running or advancing, own %.0f%%, b %.0f%%, c %.0f%%, d %.0f%%\n", AllRuns,
			100.0 * Runs[0] / std::max(1L, AllRuns), 100.0 * Runs[1] / std::max(1L, AllRuns), 100.0 * Runs[2] / std::max(1L, AllRuns),
			100.0 * Runs[3] / std::max(1L, AllRuns));
		if (!bRunStyles && Runs[0] != AllRuns) Fail("run styles off, yet %ld man-frames ran as another runner", AllRuns - Runs[0]);
		if (bRunStyles && AllRuns > 2000)
		{
			for (int K = 0; K < 4; ++K)
			{
				if (Runs[K] < AllRuns / 10) Fail("run styles on, yet runner %d shows on only %.1f%% of the running", K, 100.0 * Runs[K] / AllRuns);
			}
		}
	}

	// The tank commanders (Crowd::AddRider): each crew file's tank clips on the seats their places name, as the scene
	// seats him on a tank's commander socket; his feet on the seat, up out of the hatch, his gun put away, nothing NaN,
	// shown only when the scene says, and the battle's men unchanged around him.
	{
		std::printf("\nTHE TANK COMMANDERS (riders)\n");
		std::map<std::string, std::shared_ptr<Soldier>> Crews;
		for (const char* Army : { "USA_tanker", "GER_panzer" })
		{
			const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + ".json");
			auto S = std::make_shared<Soldier>();
			if (Text.empty()) { std::printf("  soldier_%s.json not here: passed over\n", Army); continue; }
			if (!S->Load(Text, Err)) { Fail("soldier_%s.json: %s", Army, Err.c_str()); continue; }
			Crews[Army] = S;
		}
		Crowd C3;
		C3.SetLife(Life);
		for (const auto& KV : Files) C3.AddSoldier(KV.first, KV.second);
		for (const auto& KV : Crews) C3.AddSoldier(KV.first, KV.second);
		C3.ReadArmies(ReadFile(Argv[3]), Err);
		C3.Build(RecText, GroundAt, Err);
		const int Before = C3.Count();
		int Checked = 0;
		double TopLo = 1e9, TopHi = -1e9;
		for (const auto& KV : Crews)
		{
			const Soldier& S = *KV.second;
			const std::string Army = KV.first.substr(0, 3);
			const int Man = C3.AddRider(KV.first, Army, 0x5EEDull + static_cast<uint64_t>(Checked));
			if (Man < 0 || !C3.IsRider(Man) || C3.SoldierFile(KV.first) != &S) { Fail("%s: no rider made", KV.first.c_str()); continue; }
			const int Head = S.BoneIndex("head"), Neck = S.BoneIndex("chest");
			const int FootL = S.BoneIndex("ball_l"), FootR = S.BoneIndex("ball_r");
			const int Gun = S.WeaponBone();
			for (const char* Name : { "commander_hatch", "commander_roof_mg", "commander_roof_mg_burst", "die_hatch_slump", "die_hatch_drop" })
			{
				const int Ci = S.ClipIndex(Name);
				if (Ci < 0) { Fail("%s: no clip %s", KV.first.c_str(), Name); continue; }
				const Clip& Cl = S.Clips[static_cast<size_t>(Ci)];
				if (Cl.Seats.empty()) Fail("%s %s: no places", KV.first.c_str(), Name);
				for (const Clip::Seat& St : Cl.Seats)
				{
					// (the socket on a turret facing east, 2.4 m up; his root the socket moved by the offset)
					const double SX = 300.0, SY = 200.0, SZ = 2.4;
					Crowd::Ride R;
					R.bShown = true;
					R.Clip = Ci;
					float* M = R.Seat.R;
					M[0] = 1.f; M[1] = 0.f; M[2] = 0.f;    // forward east, right south, up
					M[3] = 0.f; M[4] = -1.f; M[5] = 0.f;
					M[6] = 0.f; M[7] = 0.f; M[8] = 1.f;
					R.Seat.T[0] = static_cast<float>(SX + St.Offset.X);
					R.Seat.T[1] = static_cast<float>(SY - St.Offset.Y);
					R.Seat.T[2] = static_cast<float>(SZ + St.Offset.Z);
					const float Times[3] = { 0.1f, Cl.Long * 0.5f, Cl.Long - 0.05f };
					for (int Ti = 0; Ti < 3; ++Ti)
					{
						R.Time = Times[Ti];
						C3.SetRide(Man, R);
						C3.Update(1.0 + 0.1 * Ti);
						++Checked;
						if (!C3.Visible(Man)) { Fail("%s %s on %s: not shown", KV.first.c_str(), Name, St.Piece.c_str()); continue; }
						const Xform* F = C3.FramesOf(Man);
						const Vec3 HeadAt = F[Head].Point(S.Bones[static_cast<size_t>(Head)].Head);
						const Vec3 ChestAt = F[Neck].Point(S.Bones[static_cast<size_t>(Neck)].Head);
						const Vec3 LA = F[FootL].Point(S.Bones[static_cast<size_t>(FootL)].Head), RA = F[FootR].Point(S.Bones[static_cast<size_t>(FootR)].Head);
						const double Rim = SZ + 0.25;
						// (the head's top some 0.25 m on from the neck, the way the neck runs)
						const double NX = HeadAt.X - ChestAt.X, NY = HeadAt.Y - ChestAt.Y, NZ = HeadAt.Z - ChestAt.Z;
						const double Top = HeadAt.Z + 0.25 * NZ / std::max(0.05, std::sqrt(NX * NX + NY * NY + NZ * NZ));
						const bool bDrop = std::string(Name) == "die_hatch_drop", bSlump = std::string(Name) == "die_hatch_slump";
						if (!bDrop && !bSlump) { TopLo = std::min(TopLo, Top - Rim); TopHi = std::max(TopHi, Top - Rim); }
						if (!bDrop && !bSlump && (Top < Rim + 0.3 || Top > Rim + 1.2))
							Fail("%s %s on %s at %.2f s: his head's top %.2f m over the rim (up out of the hatch: 0.3 to 1.2)", KV.first.c_str(), Name, St.Piece.c_str(), Times[Ti], Top - Rim);
						if (bDrop && Ti == 2 && Top > Rim)
							Fail("%s %s on %s at its end: his head's top %.2f m over the rim (down inside)", KV.first.c_str(), Name, St.Piece.c_str(), Top - Rim);
						if (!bDrop && std::min(LA.Z, RA.Z) < R.Seat.T[2] - 0.15f)
							Fail("%s %s on %s at %.2f s: a foot %.2f m under the floor he stands on", KV.first.c_str(), Name, St.Piece.c_str(), Times[Ti], R.Seat.T[2] - std::min(LA.Z, RA.Z));
						if (std::fabs(HeadAt.X - static_cast<float>(SX)) > 1.2 || std::fabs(HeadAt.Y - static_cast<float>(SY)) > 1.2)
							Fail("%s %s on %s: his head %.2f m east, %.2f m north of the hatch", KV.first.c_str(), Name, St.Piece.c_str(), HeadAt.X - SX, HeadAt.Y - SY);
						if (Gun >= 0 && (F[Gun].R[0] != 0.f || F[Gun].R[4] != 0.f || F[Gun].R[8] != 0.f))
							Fail("%s %s: his gun is drawn (its bones not put away)", KV.first.c_str(), Name);
						double RX = 0.0, RY = 0.0, RZ = 0.0;
						float RF = 0.f;
						C3.PlaceOf(Man, RX, RY, RZ, RF);
						if (std::fabs(RZ - R.Seat.T[2]) > 1e-4 || std::fabs(std::remainder(RF - 1.5707963, 6.2831853)) > 1e-3)
							Fail("%s %s: his place %.3f m up, facing %.1f deg (the seat's %.3f, 90)", KV.first.c_str(), Name, RZ, RF * 57.29578f, R.Seat.T[2]);
						for (int L = 0; L < LodCount; ++L)
						{
							const int P = C3.PointsOf(Man, static_cast<Lod>(L));
							std::vector<float> SkAt(static_cast<size_t>(P) * 3), SkFa(static_cast<size_t>(P) * 3);
							C3.Skin(Man, static_cast<Lod>(L), SkAt.data(), SkFa.data());
							for (const float V : SkAt) if (!std::isfinite(V)) { Fail("%s %s: NaN skinned", KV.first.c_str(), Name); break; }
						}
					}
					// (blended from the hatch's clip into this one, halfway)
					R.FromClip = S.ClipIndex("commander_hatch");
					R.FromTime = 3.f;
					R.W = 0.5f;
					C3.SetRide(Man, R);
					C3.Update(2.0);
					const Vec3 Mid = C3.FramesOf(Man)[Head].Point(S.Bones[static_cast<size_t>(Head)].Head);
					if (!Finite(Mid)) Fail("%s %s: NaN blended", KV.first.c_str(), Name);
				}
			}
			// (hidden when the scene says: his hatch shut, his tank hidden)
			Crowd::Ride Off;
			C3.SetRide(Man, Off);
			C3.Update(3.0);
			if (C3.Visible(Man)) Fail("%s: shown when the scene hides him", KV.first.c_str());
		}
		// (the battle's own men unchanged by the riders: each still found as he was, none of them a rider)
		bool bSame = true;
		for (int Man = 0; Man < Before && bSame; ++Man) bSame = !C3.IsRider(Man);
		if (!bSame || C3.Count() != Before + static_cast<int>(Crews.size())) Fail("the riders changed the battle's men (%d men, %d before)", C3.Count(), Before);
		// (a scrub back: the crowd starts over; a rider is posed again as the scene says)
		C3.Update(0.5);
		std::printf("  %d poses of %zu commanders' tank clips on their seats checked (feet on the floor, head up out of the hatch, gun put away, shown as told); "
			"up in the hatch or at the roof gun, his head's top %.2f to %.2f m over the rim\n", Checked, Crews.size(), TopLo, TopHi);
	}

	// Frame time.
	{
		double Sum = 0.0;
		for (const double F : FrameMs) Sum += F;
		double USum = 0.0, SSum = 0.0;
		for (const double F : UpdateMs) USum += F;
		for (const double F : SkinMs) SSum += F;
		double NearMean = 0.0;
		for (const int K : NearCounts) NearMean += K;
		NearMean /= std::max<size_t>(1, NearCounts.size());
		const double Mean = Sum / std::max<size_t>(1, FrameMs.size());
		const double P99 = Percentile(FrameMs, 0.99);
		const size_t Worst = static_cast<size_t>(std::max_element(FrameMs.begin(), FrameMs.end()) - FrameMs.begin());
		std::printf("\nframe time, one core (Update + skinning the men who moved): mean %.3f ms, p95 %.3f, p99 %.3f, max %.3f (at %.2f s: Update %.3f, skinning %.3f); Update mean %.3f ms (%.1f us a man), skinning mean %.3f ms\n",
			Mean, Percentile(FrameMs, 0.95), P99, FrameMs[Worst], Worst * Dt, UpdateMs[Worst], SkinMs[Worst], USum / FrameMs.size(), 1000.0 * USum / FrameMs.size() / N,
			SSum / FrameMs.size());
		std::printf("  the eye among the thickest knot: near (under 25 m) mean %.1f men, most %d; mid most %d; %.0f points skinned a frame on average\n",
			NearMean, MostNear, MostMid, SkinnedPoints / FrameMs.size());
		std::printf("  at 110 men (scaled by the men): about %.2f ms mean, %.2f ms p99 (target under 3 ms)\n", Mean * 110.0 / N, P99 * 110.0 / N);
		if (Mean * 110.0 / N > 3.0) Fail("mean frame %.3f ms scaled to 110 men is over 3 ms", Mean * 110.0 / N);
		double OneSum = 0.0, WallSum = 0.0;
		for (const double F : SceneMs) OneSum += F;
		for (const double F : SceneWallMs) WallSum += F;
		const double OneMean = OneSum / std::max<size_t>(1, SceneMs.size()), WallMean = WallSum / std::max<size_t>(1, SceneWallMs.size());
		std::printf("  the scene's frame (Update + skinning the men in the picture who changed, %.0f of %.0f in the picture a frame, %s): one core mean %.3f ms, p95 %.3f, p99 %.3f; "
			"skins over %d threads, wall mean %.3f ms, p95 %.3f, p99 %.3f (targets at some 850 men: 3 ms one core %s, 1.5 ms wall %s)\n",
			static_cast<double>(SceneSkins) / std::max<size_t>(1, SceneMs.size()), static_cast<double>(InPicture) / std::max<size_t>(1, SceneMs.size()),
			bThrift ? "thrifty" : "every man posed every frame", OneMean, Percentile(SceneMs, 0.95), Percentile(SceneMs, 0.99), Threads, WallMean,
			Percentile(SceneWallMs, 0.95), Percentile(SceneWallMs, 0.99), OneMean < 3.0 ? "met" : "NOT met", WallMean < 1.5 ? "met" : "NOT met");
	}
	if (NanFrames) Fail("%ld frames with NaN", NanFrames);
	if (bLevels)
	{
		const double Fs = static_cast<double>(std::max<size_t>(1, FrameMs.size()));
		std::printf("\nlevels (GS3): %.2f men change level a frame on average, the most %d (at %.2f s); %ld shadow switches (%.2f a frame); "
			"the cap of %d held men at their old level in %ld frames (the most %d in a frame, %.2f a frame), the longest wait %d frames (%.0f ms); "
			"%ld flickers (a man back to his last level within half a second); %.1f men a frame too small to draw (past %.0f m)\n",
			LevelChanges / Fs, MostChanges, MostChangesAt, ShadowSwitches, ShadowSwitches / Fs, RebuildCap, LevelCapFrames, MostHeld, HeldTotal / Fs, LongestHeld,
			LongestHeld * Dt * 1000.0, Flickers, CulledTotal / Fs, CullFrom);
		if (LongestHeld > 30) Fail("a man waited %d frames (over half a second) for his new level under the cap", LongestHeld);
		if (Flickers > LevelChanges / 50 + 2) Fail("%ld of %ld level changes flicker back within half a second", Flickers, LevelChanges);
	}

	// Over the cores (SetRun, the frame-rate lane of 22 November): the same men to the bit as one after another, and the
	// same events in the same order, frame by frame, thrifty, the eye among them; and what it costs each way.
	{
		Crowd One, Many;
		Pool Cores(4);
		for (Crowd* Cr : { &One, &Many })
		{
			Cr->SetLife(Life);
			for (const auto& KV : Files) Cr->AddSoldier(KV.first, KV.second);
			Cr->ReadArmies(ReadFile(Argv[3]), Err);
			Cr->Build(RecText, GroundAt, Err);
			Cr->SetThrift(true);
		}
		Many.SetRun([&Cores](int Count, const std::function<void(int)>& Job) { Cores.For(Count, Job); });
		double LookX = 0.0, LookY = 0.0, LookZ = 0.0;
		float LookFacing = 0.f;
		One.PlaceOf(One.Count() / 2, LookX, LookY, LookZ, LookFacing);
		long Stepped = 0, ManDiffs = 0, EventDiffs = 0, Posed = 0, Falls = 0;
		double OneMs = 0.0, ManyMs = 0.0, PosedOff = 0.0;
		const double Step = 1.0 / 30.0, Until = std::min(End, 240.0);
		for (double T = 0.0; T <= Until; T += Step, ++Stepped)
		{
			One.SetEye(LookX, LookY - 40.0, LookZ + 6.0);
			Many.SetEye(LookX, LookY - 40.0, LookZ + 6.0);
			const auto A = std::chrono::steady_clock::now();
			One.Update(T);
			const auto B = std::chrono::steady_clock::now();
			Many.Update(T);
			const auto Cc = std::chrono::steady_clock::now();
			OneMs += Ms(A, B);
			ManyMs += Ms(B, Cc);
			const std::vector<Event>& E1 = One.Events();
			const std::vector<Event>& E2 = Many.Events();
			if (E1.size() != E2.size()) ++EventDiffs;
			else
				for (size_t K = 0; K < E1.size(); ++K)
					if (E1[K].T != E2[K].T || E1[K].Man != E2[K].Man || E1[K].Fx != E2[K].Fx || E1[K].Cue != E2[K].Cue || E1[K].Round != E2[K].Round) { ++EventDiffs; break; }
			for (int M = 0; M < One.Count(); ++M)
			{
				if (One.IsFalling(M)) ++Falls;
				const Soldier* S1 = One.SoldierOf(M);
				const Xform* F1 = One.FramesOf(M);
				const Xform* F2 = Many.FramesOf(M);
				if (One.Visible(M) != Many.Visible(M) || (S1 && F1 && F2 && std::memcmp(F1, F2, sizeof(Xform) * S1->Bones.size()) != 0)) { ++ManDiffs; continue; }
				double X1, Y1, Z1, X2, Y2, Z2;
				float Fa, Fb;
				if (One.Visible(M) && One.PlaceOf(M, X1, Y1, Z1, Fa) && One.PlaceAsPosed(M, X2, Y2, Z2, Fb))
				{
					++Posed;
					PosedOff = std::max(PosedOff, std::fabs(Z1 - Z2));
				}
			}
		}
		std::printf("\nover the cores: %ld frames to %.0f s, %d men; men unlike one after another %ld, frames with unlike events %ld (%ld falling a frame summed); "
			"Update one core %.3f ms, over 4 %.3f ms a frame; PlaceAsPosed off PlaceOf by %.3f m at most (%ld asked)\n",
			Stepped, Until, One.Count(), ManDiffs, EventDiffs, Falls, OneMs / std::max(1L, Stepped), ManyMs / std::max(1L, Stepped), PosedOff, Posed);
		if (ManDiffs > 0 || EventDiffs > 0) Fail("the men over the cores are not the men one after another (%ld men, %ld frames of events)", ManDiffs, EventDiffs);
		if (PosedOff > 0.25) Fail("PlaceAsPosed is %.2f m off PlaceOf", PosedOff);
	}

	std::printf("\n%s (%d failures)\n", Fails ? "LAND SHOW FAILED" : "land show ok", Fails);
	return Fails ? 1 : 0;
}
