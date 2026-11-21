// The men's cloud test: plays a recorded ground battle's men through Battle/LandMen (the game's own code, no engine) at
// 60 frames a second, as the scene would, and checks what Brian will see: nothing broken (no NaN), every man the
// battle kills starts his fall on the frame he is hit, ends lying on the ground (not standing, not sunk in it) and
// never moves again, no body is ever taken away, feet do not skate where the play rate can follow the ground, every
// round the rules fired comes out as one shot at its own time from the man's muzzle, and the frame's cost.
// Usage: land_show_test <recording.json> <soldiers folder> <armies.json>
#include "land_json.h"
#include "Battle/LandMen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
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
	for (const char* Army : { "GER", "POL" })
	{
		const std::string Path = std::string(Argv[2]) + "/soldier_" + Army + ".json";
		const std::string Text = ReadFile(Path);
		auto S = std::make_shared<Soldier>();
		const auto A = std::chrono::steady_clock::now();
		if (Text.empty() || !S->Load(Text, Err)) { std::printf("cannot read %s: %s\n", Path.c_str(), Err.c_str()); return 1; }
		const auto B = std::chrono::steady_clock::now();
		std::printf("%s: %zu bytes, %zu bones, %zu clips, near/mid/far %d/%d/%d points, %zu faces, %zu helmet finishes, deaths map %s; read in %.1f ms\n",
			Path.c_str(), Text.size(), S->Bones.size(), S->Clips.size(), S->Meshes[Near].Points, S->Meshes[Mid].Points, S->Meshes[Far].Points,
			S->Faces.size(), S->Finishes.size(), S->Deaths.empty() ? "no (the README's three falls)" : "yes", Ms(A, B));
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
	const GroundFn GroundAt = [&Fd](double X, double Y) { return Fd(X, Y); };

	Crowd C;
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
			const int Man = C.Find(R["by"].Int(-1), R["k"].Int(-1));
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
	const double End = std::max(Rec["end"].Num(270.0), LastDeath + 6.0);

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
	std::vector<int> NearCounts;
	int MostNear = 0, MostMid = 0;
	double SkinnedPoints = 0.0;
	long NanFrames = 0;

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
		UpdateMs.push_back(Ms(T0, T1));
		SkinMs.push_back(Ms(T1, T2));
		FrameMs.push_back(Ms(T0, T2));
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
				if (std::fabs(Ev.T - (S.T + 0.1 * Ev.Round)) > 1e-6) Fail("a burst's bullet off its time (man %d at %.2f s)", Ev.Man, Ev.T);
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
	}
	if (NanFrames) Fail("%ld frames with NaN", NanFrames);

	std::printf("\n%s (%d failures)\n", Fails ? "LAND SHOW FAILED" : "land show ok", Fails);
	return Fails ? 1 : 0;
}
