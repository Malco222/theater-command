// The pose bank's test (the true-scale plan's SO1): far men shown as one of about 70 baked poses an army instead of
// posed and skinned each frame. Plays a recorded ground battle twice at 60 frames a second, with the eye where the
// men's cloud test puts it (among the thickest knot of men): once with the bank read (Crowd::ReadPoseBank), once
// without, and checks
//   - the bake: its cost at Build for each soldier file (under 200 ms each);
//   - that a banked man IS the bank's pose: his bones are the clip's at the pose's phase (as Sample and BoneFrames give
//     them) and his far skin is the live skin of those bones, point for point (floats on the field: under 1 mm);
//   - what the bank costs the eye: how far each banked man's far skin is from the live skin of the same man at the
//     same moment without the bank (the poses between the bank's: his worst point, over the men and the frames);
//   - that nothing of the play changes: the same events (shots, footsteps, falls) at the same times, the same places;
//   - the frame's cost both ways: Update, and the far skins the scene would still make (the banked men none: they
//     are instances of the (set, pose) meshes, BankMesh), and how many such meshes the battle shows.
// Usage: land_bank_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>
// LAND_SHOW_LIFE=x: life seconds a battle second (Crowd::SetLife), 2 by default as the scene plays the fight;
// LAND_BANK_SECONDS=s plays only the first s seconds.
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

	/** The recording's ground (as the men's cloud test reads it). */
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

	double Quantile(std::vector<double> V, double Q)
	{
		if (V.empty()) return 0.0;
		std::sort(V.begin(), V.end());
		const size_t K = std::min(V.size() - 1, static_cast<size_t>(Q * static_cast<double>(V.size() - 1) + 0.5));
		return V[K];
	}
	double Mean(const std::vector<double>& V)
	{
		double S = 0.0;
		for (const double X : V) S += X;
		return V.empty() ? 0.0 : S / static_cast<double>(V.size());
	}
	/** The worst of three-float points apart, metres. */
	double Worst(const float* A, const float* B, int Points, int* OutAt = nullptr)
	{
		double W = 0.0;
		for (int P = 0; P < Points; ++P)
		{
			const size_t I = static_cast<size_t>(P) * 3;
			const double D = std::sqrt(static_cast<double>((A[I] - B[I]) * (A[I] - B[I]) + (A[I + 1] - B[I + 1]) * (A[I + 1] - B[I + 1]) + (A[I + 2] - B[I + 2]) * (A[I + 2] - B[I + 2])));
			if (D > W) { W = D; if (OutAt) *OutAt = P; }
		}
		return W;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::fprintf(stderr, "usage: land_bank_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>\n");
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
	const std::string BankText = ReadFile(Argv[4]);
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
	const char* LifeEnv = std::getenv("LAND_SHOW_LIFE");
	const double Life = LifeEnv ? std::atof(LifeEnv) : 2.0;

	// Two crowds of the same battle: B with the bank, L live (as before).
	Crowd B, L;
	for (Crowd* C : { &B, &L })
	{
		C->SetLife(Life);
		for (const auto& KV : Files) C->AddSoldier(KV.first, KV.second);
		if (!C->ReadArmies(ReadFile(Argv[3]), Err)) { std::printf("cannot read %s: %s\n", Argv[3], Err.c_str()); return 1; }
	}
	if (!B.ReadPoseBank(BankText, Err)) { std::printf("cannot read the pose bank %s: %s\n", Argv[4], Err.c_str()); return 1; }
	{
		Crowd Bad;
		std::string E2;
		if (Bad.ReadPoseBank("{\"poses\": []}", E2) || Bad.PoseBankOn()) Fail("an empty bank was read");
	}
	double BuildMs[2] = {};
	for (int K = 0; K < 2; ++K)
	{
		Crowd& C = K == 0 ? B : L;
		const auto A = std::chrono::steady_clock::now();
		if (!C.Build(RecText, GroundAt, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
		BuildMs[K] = Ms(A, std::chrono::steady_clock::now());
	}
	const int N = B.Count();
	std::printf("%s: %d men; built in %.1f ms with the bank, %.1f ms without\n", Argv[1], N, BuildMs[0], BuildMs[1]);
	std::printf("bank: %d poses; bake %.1f ms in all:", B.BankPoses(), B.PoseBankMs());
	for (const auto& KV : B.PoseBankMsByFile())
	{
		std::printf(" %s %.1f ms;", KV.first.c_str(), KV.second);
		if (KV.second > 200.0) Fail("the bake of %s took %.1f ms (over 200)", KV.first.c_str(), KV.second);
	}
	std::printf("\nbank sets (men alike out there): %d\n", B.BankKeys());
	if (L.PoseBankOn() || L.BankKeys() != 0) Fail("the crowd without a bank has one");
	{
		int NoKey = 0;
		for (int I = 0; I < N; ++I) NoKey += B.BankKey(I) < 0;
		if (NoKey) Fail("%d men have no bank set", NoKey);
	}

	// Play.
	const double Dt = 1.0 / 60.0;
	double LastDeath = 0.0;
	for (int I = 0; I < N; ++I) LastDeath = std::max(LastDeath, B.Info(I).Dead);
	double End = std::max(Rec["end"].Num(270.0), LastDeath + 12.0);
	if (const char* Secs = std::getenv("LAND_BANK_SECONDS")) End = std::min(End, std::atof(Secs));
	const int Frames = static_cast<int>(std::ceil(End / Dt)) + 1;
	const int Every = 30;  // the checks every half second
	std::vector<double> UpdB, UpdL, SkinB, SkinL;
	std::vector<double> Quant;                     // each banked man's worst point from the live man, metres, sampled
	std::map<std::string, double> QuantWorstBy;    // by the clip the live man shows
	std::map<std::string, std::vector<double>> QuantBy;
	std::map<std::string, std::vector<double>> QuantWhy;  // by what he is doing: one clip, blending two, a shot shown
	double SelfWorst = 0.0, BoneWorst = 0.0, MeshWorst = 0.0;
	long MeshOffPoints = 0, MeshPoints = 0;
	long EventsB = 0, EventsL = 0, EventsOff = 0, PlaceOff = 0;
	long BankedFrames = 0, FarFrames = 0, ManFrames = 0, Switches = 0;
	std::set<std::pair<int, int>> Shown;           // (set, pose) shown
	std::vector<int> LastPose(static_cast<size_t>(N), -1);
	std::vector<uint32_t> StampB(static_cast<size_t>(N), 0xFFFFFFFFu), StampL(static_cast<size_t>(N), 0xFFFFFFFFu);
	std::vector<int> LodB(static_cast<size_t>(N), -1), LodL(static_cast<size_t>(N), -1);
	std::vector<float> SA, SB2, SL, Mesh, MeshFa, Fa;
	double EyeX = 0, EyeY = 0;
	for (int Fr = 0; Fr < Frames; ++Fr)
	{
		const double T = Fr * Dt;
		if (Fr % 60 == 0)
		{
			// (the eye among the men with the most others within 25 m, 6 m behind, as the cloud test's)
			int Best = -1, BestN = -1;
			std::vector<double> X(static_cast<size_t>(N)), Y(static_cast<size_t>(N));
			for (int I = 0; I < N; ++I)
			{
				double Z;
				float F;
				L.PlaceOf(I, X[static_cast<size_t>(I)], Y[static_cast<size_t>(I)], Z, F);
			}
			for (int I = 0; I < N; ++I)
			{
				if (!L.Visible(I)) continue;
				int Near25 = 0;
				for (int K = 0; K < N; ++K)
				{
					const double DX = X[static_cast<size_t>(K)] - X[static_cast<size_t>(I)], DY = Y[static_cast<size_t>(K)] - Y[static_cast<size_t>(I)];
					Near25 += DX * DX + DY * DY < 625.0;
				}
				if (Near25 > BestN) { BestN = Near25; Best = I; }
			}
			if (Best >= 0)
			{
				double X0, Y0, Z0;
				float F;
				L.PlaceOf(Best, X0, Y0, Z0, F);
				EyeX = X0 - 6.0 * std::sin(F);
				EyeY = Y0 - 6.0 * std::cos(F);
			}
		}
		const double EyeZ = Fd(EyeX, EyeY).Height + 1.7;
		B.SetEye(EyeX, EyeY, EyeZ);
		L.SetEye(EyeX, EyeY, EyeZ);
		auto T0 = std::chrono::steady_clock::now();
		B.Update(T);
		auto T1 = std::chrono::steady_clock::now();
		L.Update(T);
		auto T2 = std::chrono::steady_clock::now();
		UpdB.push_back(Ms(T0, T1));
		UpdL.push_back(Ms(T1, T2));

		// The same play: the same events at the same times.
		{
			const std::vector<Event>& Eb = B.Events();
			const std::vector<Event>& El = L.Events();
			EventsB += static_cast<long>(Eb.size());
			EventsL += static_cast<long>(El.size());
			if (Eb.size() != El.size()) ++EventsOff;
			else
			{
				for (size_t K = 0; K < Eb.size(); ++K)
				{
					if (Eb[K].Man != El[K].Man || Eb[K].Fx != El[K].Fx || std::fabs(Eb[K].T - El[K].T) > 1e-9) { ++EventsOff; break; }
				}
			}
		}

		// The far skins the scene would make: each man when his Stamp or level moved; banked men none (instances).
		T0 = std::chrono::steady_clock::now();
		for (int I = 0; I < N; ++I)
		{
			if (!B.Visible(I)) continue;
			const Lod Lv = B.LodOf(I);
			const size_t Iz = static_cast<size_t>(I);
			if (B.BankPose(I) >= 0)
			{
				LodB[Iz] = -1;
				continue;
			}
			if (B.Stamp(I) == StampB[Iz] && static_cast<int>(Lv) == LodB[Iz]) continue;
			StampB[Iz] = B.Stamp(I);
			LodB[Iz] = static_cast<int>(Lv);
			SA.resize(static_cast<size_t>(B.PointsOf(I, Lv)) * 3);
			Fa.resize(SA.size());
			B.Skin(I, Lv, SA.data(), Fa.data());
		}
		T1 = std::chrono::steady_clock::now();
		for (int I = 0; I < N; ++I)
		{
			if (!L.Visible(I)) continue;
			const Lod Lv = L.LodOf(I);
			const size_t Iz = static_cast<size_t>(I);
			if (L.Stamp(I) == StampL[Iz] && static_cast<int>(Lv) == LodL[Iz]) continue;
			StampL[Iz] = L.Stamp(I);
			LodL[Iz] = static_cast<int>(Lv);
			SL.resize(static_cast<size_t>(L.PointsOf(I, Lv)) * 3);
			Fa.resize(SL.size());
			L.Skin(I, Lv, SL.data(), Fa.data());
		}
		T2 = std::chrono::steady_clock::now();
		SkinB.push_back(Ms(T0, T1));
		SkinL.push_back(Ms(T1, T2));

		// Who is banked, the meshes shown, the pose changes; and the Stamp held while the pose is.
		for (int I = 0; I < N; ++I)
		{
			if (!B.Visible(I)) continue;
			++ManFrames;
			FarFrames += B.LodOf(I) == Far;
			const int K = B.BankPose(I);
			const size_t Iz = static_cast<size_t>(I);
			if (K >= 0)
			{
				++BankedFrames;
				Shown.insert({ B.BankKey(I), K });
				if (LastPose[Iz] >= 0 && LastPose[Iz] != K) ++Switches;
			}
			LastPose[Iz] = K;
		}

		if (Fr % Every != 0) continue;
		for (int I = 0; I < N; ++I)
		{
			const int K = B.BankPose(I);
			if (K < 0 || !B.Visible(I)) continue;
			const Soldier& S = *B.SoldierOf(I);
			const int Pts = B.PointsOf(I, Far);
			SA.resize(static_cast<size_t>(Pts) * 3);
			SB2.resize(SA.size());
			SL.resize(SA.size());
			Fa.resize(SA.size());
			// (1) He is the bank's pose: his bones the clip's at the pose's phase...
			{
				const int Ci = S.ClipIndex(B.BankClip(K));
				Pose P;
				Sample(S.Clips[static_cast<size_t>(Ci)], B.BankPhase(K) * S.Clips[static_cast<size_t>(Ci)].Long, P);
				std::vector<Xform> Model(S.Bones.size());
				BoneFrames(S, P, Model.data());
				const Xform* Fr2 = B.FramesOf(I);
				for (size_t Bo = 0; Bo < S.Bones.size(); ++Bo)
				{
					// (a rifle-down copy: the rifle's bones are laid down, the rest as the clip's)
					if (B.BankRifleDown(K) && S.OnWeapon(static_cast<int>(Bo))) continue;
					const Xform W = Compose(B.WorldFrame(I), Model[Bo]);
					const double D = std::sqrt(static_cast<double>((W.T[0] - Fr2[Bo].T[0]) * (W.T[0] - Fr2[Bo].T[0]) + (W.T[1] - Fr2[Bo].T[1]) * (W.T[1] - Fr2[Bo].T[1]) + (W.T[2] - Fr2[Bo].T[2]) * (W.T[2] - Fr2[Bo].T[2])));
					BoneWorst = std::max(BoneWorst, D);
					for (int A = 0; A < 9; ++A) BoneWorst = std::max(BoneWorst, static_cast<double>(std::fabs(W.R[A] - Fr2[Bo].R[A])));
				}
			}
			// ...and his far skin the live skin of those bones (the bank turned off a moment: the same frames, skinned).
			B.Skin(I, Far, SA.data(), Fa.data());
			B.SetPoseBank(false);
			B.Skin(I, Far, SB2.data(), Fa.data());
			B.SetPoseBank(true);
			SelfWorst = std::max(SelfWorst, Worst(SA.data(), SB2.data(), Pts));
			// (the set's mesh, placed by his frame: the same but his face's few moved points)
			if (B.BankMesh(B.BankKey(I), K, Mesh, MeshFa) && static_cast<int>(Mesh.size()) == Pts * 3)
			{
				const Xform& W = B.WorldFrame(I);
				for (int P = 0; P < Pts; ++P)
				{
					const size_t Q = static_cast<size_t>(P) * 3;
					const float X = Mesh[Q], Y = Mesh[Q + 1], Z = Mesh[Q + 2];
					const float Wx = W.R[0] * X + W.R[1] * Y + W.R[2] * Z + W.T[0];
					const float Wy = W.R[3] * X + W.R[4] * Y + W.R[5] * Z + W.T[1];
					const float Wz = W.R[6] * X + W.R[7] * Y + W.R[8] * Z + W.T[2];
					const double D = std::sqrt(static_cast<double>((Wx - SA[Q]) * (Wx - SA[Q]) + (Wy - SA[Q + 1]) * (Wy - SA[Q + 1]) + (Wz - SA[Q + 2]) * (Wz - SA[Q + 2])));
					MeshWorst = std::max(MeshWorst, D);
					MeshOffPoints += D > 0.001;
					++MeshPoints;
				}
				if (static_cast<int>(B.BankLook(B.BankKey(I)).Triangles.size()) != static_cast<int>(B.LookOf(I, Far).Triangles.size())) Fail("set %d's triangles are not man %d's", B.BankKey(I), I);
			}
			else Fail("set %d has no mesh in pose %d (or not man %d's points)", B.BankKey(I), K, I);
			// (2) What the bank costs: the live man at this very moment, his far skin.
			{
				double X1, Y1, Z1, X2, Y2, Z2;
				float F1, F2;
				B.PlaceOf(I, X1, Y1, Z1, F1);
				L.PlaceOf(I, X2, Y2, Z2, F2);
				if (std::fabs(X1 - X2) > 1e-6 || std::fabs(Y1 - Y2) > 1e-6 || std::fabs(F1 - F2) > 1e-6) ++PlaceOff;
			}
			L.Skin(I, Far, SL.data(), Fa.data());
			int WorstAt = 0;
			const double Q = Worst(SA.data(), SL.data(), Pts, &WorstAt);
			if (std::getenv("LAND_BANK_WORST") && Q > 0.6)
			{
				// (which bone's the worst point is nearest, on the live man)
				const Xform* Fl = L.FramesOf(I);
				int Bn = 0;
				double Bd = 1e9;
				for (size_t Bo = 0; Bo < S.Bones.size(); ++Bo)
				{
					const double D = std::hypot(std::hypot(Fl[Bo].T[0] - SL[static_cast<size_t>(WorstAt) * 3], Fl[Bo].T[1] - SL[static_cast<size_t>(WorstAt) * 3 + 1]), Fl[Bo].T[2] - SL[static_cast<size_t>(WorstAt) * 3 + 2]);
					if (D < Bd) { Bd = D; Bn = static_cast<int>(Bo); }
				}
				const Playing Pw = L.PlayingOf(I);
				std::printf("  worst %.2f m at t %.1f man %d: live %s t %.3f rate %.2f blend %d shot %d lie %.2f; bank %d %s %.3f; near bone %s\n", Q, T, I,
					Pw.Clip.c_str(), Pw.Time, Pw.Rate, Pw.bBlending ? 1 : 0, Pw.bShooting ? 1 : 0, Pw.Lie, K, B.BankClip(K).c_str(), B.BankPhase(K), S.Bones[static_cast<size_t>(Bn)].Name.c_str());
			}
			Quant.push_back(Q);
			const Playing Pl = L.PlayingOf(I);
			const std::string Clip = Pl.Clip;
			QuantBy[Clip].push_back(Q);
			const std::string Why = Pl.bShooting ? "shot shown" : Pl.bBlending ? "two clips blending" : B.BankClip(K) == Clip ? "the clip's own poses" : "an instead clip's poses";
			QuantWhy[Why].push_back(Q);
			QuantWorstBy[Clip] = std::max(QuantWorstBy[Clip], Q);
		}
	}

	std::printf("played %.0f s at 60 frames a second; %.1f%% of the men's frames at the far level, %.1f%% banked\n", End,
		100.0 * static_cast<double>(FarFrames) / static_cast<double>(std::max(1L, ManFrames)), 100.0 * static_cast<double>(BankedFrames) / static_cast<double>(std::max(1L, ManFrames)));
	{
		long Down = 0;
		for (const auto& Kp : Shown) Down += B.BankRifleDown(Kp.second);
		std::printf("rifle-down poses shown: %ld\n", Down);
	}
	std::printf("meshes the scene builds: %zu (set, pose) shown of %d sets x %d poses; a banked man changes pose %.2f times a second\n",
		Shown.size(), B.BankKeys(), B.BankPoses(), static_cast<double>(Switches) / std::max(1.0, static_cast<double>(BankedFrames) * Dt));
	std::printf("banked is the bank's pose: bones off by %.2g (metres and matrix), far skin off its live skin by %.2g m\n", BoneWorst, SelfWorst);
	if (BoneWorst > 1e-4) Fail("a banked man's bones are %.3g off the bank's pose", BoneWorst);
	// (both are floats on the field, some hundreds of metres from its corner: a float there is good to about 0.1 mm)
	if (SelfWorst > 1e-3) Fail("a banked man's far skin is %.3g m off the live skin of his bones", SelfWorst);
	std::printf("set mesh placed by WorldFrame: worst %.4f m; %.2f%% of points over 1 mm (his face's moved points)\n", MeshWorst,
		100.0 * static_cast<double>(MeshOffPoints) / static_cast<double>(std::max(1L, MeshPoints)));
	if (MeshWorst > 0.05) Fail("a set's mesh placed by WorldFrame is %.3f m off his banked skin", MeshWorst);
	std::printf("banked vs the live man at the same moment (far skin, his worst point; %zu samples): median %.3f m, p90 %.3f, p99 %.3f, worst %.3f\n",
		Quant.size(), Quantile(Quant, 0.5), Quantile(Quant, 0.9), Quantile(Quant, 0.99), Quantile(Quant, 1.0));
	{
		std::vector<std::pair<double, std::string>> ByWorst;
		for (const auto& KV : QuantBy) ByWorst.push_back({ Quantile(KV.second, 0.5), KV.first });
		std::sort(ByWorst.rbegin(), ByWorst.rend());
		std::printf("  by clip (median / worst, samples):");
		for (const auto& KV : ByWorst) std::printf(" %s %.3f/%.3f (%zu);", KV.second.c_str(), KV.first, QuantWorstBy[KV.second], QuantBy[KV.second].size());
		std::printf("\n");
	}
	std::printf("  by what he is doing (median / p99 / worst, share of samples):");
	for (const auto& KV : QuantWhy)
	{
		std::printf(" %s %.3f/%.3f/%.3f (%.1f%%);", KV.first.c_str(), Quantile(KV.second, 0.5), Quantile(KV.second, 0.99), Quantile(KV.second, 1.0),
			100.0 * static_cast<double>(KV.second.size()) / static_cast<double>(std::max<size_t>(1, Quant.size())));
	}
	std::printf("\n");
	std::printf("the play: events %ld with the bank, %ld without; %ld frames differ; %ld sampled places differ\n", EventsB, EventsL, EventsOff, PlaceOff);
	if (EventsOff || EventsB != EventsL) Fail("the bank changed the play's events (%ld frames)", EventsOff);
	if (PlaceOff) Fail("the bank moved %ld men", PlaceOff);
	std::printf("frame cost, ms (mean / p95): Update %.2f / %.2f with the bank, %.2f / %.2f without; skins %.2f / %.2f with, %.2f / %.2f without\n",
		Mean(UpdB), Quantile(UpdB, 0.95), Mean(UpdL), Quantile(UpdL, 0.95), Mean(SkinB), Quantile(SkinB, 0.95), Mean(SkinL), Quantile(SkinL, 0.95));
	if (!BankedFrames) Fail("no man was banked");
	std::printf(Fails ? "%d FAILED\n" : "all passed\n", Fails);
	return Fails ? 1 : 0;
}
