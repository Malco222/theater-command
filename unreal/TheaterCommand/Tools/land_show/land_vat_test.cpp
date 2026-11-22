// The pose bank in vertex-animation form (the true-scale plan's SO1 as the Mac study wants it): mid and far men drawn as
// instances, one component per body set and level, each instance's pose read by its material from one buffer of every
// bank pose (BankBody) by two pose numbers and a share (BankShow), helmets as instances of their own (HelmetFrame).
// Plays a recorded ground battle at 60 frames a second twice, with the eye where the men's cloud test puts it: once with
// the bank and SetPoseBankMid, once live (as before), and checks
//   - the bake: each army's cost (its soldier files' bakes at both levels and the buffers and looks made from them),
//     at most 200 ms an army;
//   - that the buffer IS the bake: BankBody's pose k is BankMesh's body point for point, every set and pose, and the
//     mid one the banked man's own mid skin; the body look the man's own triangles;
//   - what the material would draw (BankBody's two poses lerped by the share, placed by WorldFrame) against the live
//     man at the same moment, at mid and at far, in metres and in pixels (a 1920-wide picture, 90 degrees across);
//   - the pop at 25 m: the frame a man goes from near (skinned live) to mid (an instance), the instance against his
//     live mid skin that frame, in pixels;
//   - the helmet: HelmetFrame on the finish's points against his skin's finish;
//   - BankShowBefore is the last frame's BankShow; BankPose the nearer of BankShow's two;
//   - that nothing of the play changes: the same events at the same times.
// Usage: land_vat_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>
// LAND_SHOW_LIFE=x (2 by default), LAND_BANK_SECONDS=s plays only the first s seconds.
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
	double Worst(const float* A, const float* B, int Points)
	{
		double W = 0.0;
		for (int P = 0; P < Points; ++P)
		{
			const size_t I = static_cast<size_t>(P) * 3;
			const double D = std::sqrt(static_cast<double>((A[I] - B[I]) * (A[I] - B[I]) + (A[I + 1] - B[I + 1]) * (A[I + 1] - B[I + 1]) + (A[I + 2] - B[I + 2]) * (A[I + 2] - B[I + 2])));
			W = std::max(W, D);
		}
		return W;
	}

	/** What the material draws: the buffer's two poses lerped by the share, placed by his frame. */
	void Instance(const std::vector<float>& Body, int Points, const Crowd::Show& Sh, const Xform& W, std::vector<float>& Out)
	{
		Out.resize(static_cast<size_t>(Points) * 3);
		const float* A = Body.data() + static_cast<size_t>(Sh.A) * static_cast<size_t>(Points) * 3;
		const float* B = Body.data() + static_cast<size_t>(Sh.B) * static_cast<size_t>(Points) * 3;
		for (int P = 0; P < Points; ++P)
		{
			const size_t I = static_cast<size_t>(P) * 3;
			const float X = A[I] + (B[I] - A[I]) * Sh.W, Y = A[I + 1] + (B[I + 1] - A[I + 1]) * Sh.W, Z = A[I + 2] + (B[I + 2] - A[I + 2]) * Sh.W;
			Out[I] = W.R[0] * X + W.R[1] * Y + W.R[2] * Z + W.T[0];
			Out[I + 1] = W.R[3] * X + W.R[4] * Y + W.R[5] * Z + W.T[1];
			Out[I + 2] = W.R[6] * X + W.R[7] * Y + W.R[8] * Z + W.T[2];
		}
	}

	/** Pixels a metre at distance D on a 1920-wide picture 90 degrees across (Unreal's default view). */
	double PxPerM(double D) { return 960.0 / std::max(1.0, D); }

	std::string ArmyOfFile(const std::string& Name)
	{
		const size_t U = Name.find('_');
		return U == std::string::npos ? Name : Name.substr(0, U);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::fprintf(stderr, "usage: land_vat_test <recording.json> <soldiers folder> <armies.json> <pose_bank.json>\n");
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

	// V: the bank, mid men banked too (the vertex-animation form); L: live, as before.
	Crowd V, L;
	for (Crowd* C : { &V, &L })
	{
		C->SetLife(Life);
		for (const auto& KV : Files) C->AddSoldier(KV.first, KV.second);
		if (!C->ReadArmies(ReadFile(Argv[3]), Err)) { std::printf("cannot read %s: %s\n", Argv[3], Err.c_str()); return 1; }
	}
	if (!V.ReadPoseBank(BankText, Err)) { std::printf("cannot read the pose bank %s: %s\n", Argv[4], Err.c_str()); return 1; }
	V.SetPoseBankMid(true);
	if (!V.PoseBankMid()) Fail("mid banking did not turn on");
	const auto B0 = std::chrono::steady_clock::now();
	if (!V.Build(RecText, GroundAt, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
	const double BuildV = Ms(B0, std::chrono::steady_clock::now());
	if (!L.Build(RecText, GroundAt, Err)) { std::printf("cannot build the crowd: %s\n", Err.c_str()); return 1; }
	const int N = V.Count();
	const int NP = V.BankPoses();
	std::printf("%s: %d men; built in %.1f ms with the bank at mid and far; %d poses\n", Argv[1], N, BuildV, NP);

	// The bake, each army: its files' bakes (both levels), and the buffers and looks the draw side makes from them.
	std::map<std::string, double> ArmyMs;
	std::map<std::string, std::string> ArmyOfName;   // a file's own name -> its army (the file's name to its first _)
	for (const auto& KV : Files) ArmyOfName[KV.second->Name] = ArmyOfFile(KV.first);
	for (const auto& KV : V.PoseBankMsByFile()) ArmyMs[ArmyOfName.count(KV.first) ? ArmyOfName[KV.first] : KV.first] += KV.second;
	std::map<int, int> ManOfKey;   // a man of each body set
	for (int I = 0; I < N; ++I)
	{
		if (V.BodyKey(I) >= 0 && !ManOfKey.count(V.BodyKey(I))) ManOfKey[V.BodyKey(I)] = I;
	}
	if (static_cast<int>(ManOfKey.size()) != V.BodyKeys()) Fail("%d body sets but %zu met on men", V.BodyKeys(), ManOfKey.size());
	std::map<std::pair<int, int>, std::vector<float>> Bodies;   // (set, level) -> the buffer, as the draw side holds it
	double BufferBytes = 0.0;
	for (const auto& KV : ManOfKey)
	{
		const std::string Army = ArmyOfName[V.SoldierOf(KV.second)->Name];
		const auto T0 = std::chrono::steady_clock::now();
		for (const Lod Lv : { Mid, Far })
		{
			std::vector<float> At, Fa;
			if (!V.BankBody(KV.first, Lv, At, Fa)) { Fail("set %d has no %s buffer", KV.first, Lv == Mid ? "mid" : "far"); continue; }
			const Look& Lk = V.BankBodyLook(KV.first, Lv);
			if (Lk.Points != V.BankBodyPoints(KV.first, Lv)) Fail("set %d's look has %d points, its body %d", KV.first, Lk.Points, V.BankBodyPoints(KV.first, Lv));
			if (At.size() != static_cast<size_t>(NP) * static_cast<size_t>(Lk.Points) * 3 || Fa.size() != At.size()) Fail("set %d's buffer is %zu floats", KV.first, At.size());
			BufferBytes += static_cast<double>(At.size() + Fa.size()) * 2.0;   // (as half floats on the card)
			Bodies[{ KV.first, static_cast<int>(Lv) }] = std::move(At);
		}
		ArmyMs[Army] += Ms(T0, std::chrono::steady_clock::now());
	}
	std::printf("bake by army (both levels, the buffers and looks made):");
	for (const auto& KV : ArmyMs)
	{
		std::printf(" %s %.1f ms;", KV.first.c_str(), KV.second);
		if (KV.second > 200.0) Fail("the %s bake took %.1f ms (over 200)", KV.first.c_str(), KV.second);
	}
	std::printf("\nbody sets: %d (bank sets with the finish: %d); the buffers %.1f MB as half floats (points and facings)\n", V.BodyKeys(), V.BankKeys(), BufferBytes / 1048576.0);

	// The buffer IS the bake: BankBody's pose k against BankMesh's body, every pose of every set (far); the look's
	// triangles the man's own body's.
	{
		double Off = 0.0;
		long Poses = 0;
		std::vector<float> Mesh, MeshFa;
		for (const auto& KV : ManOfKey)
		{
			const int I = KV.second;
			const std::vector<float>& Body = Bodies[{ KV.first, static_cast<int>(Far) }];
			const int Pts = V.BankBodyPoints(KV.first, Far);
			for (int K = 0; K < NP; ++K)
			{
				if (!V.BankHas(KV.first, K)) continue;
				if (!V.BankMesh(V.BankKey(I), K, Mesh, MeshFa)) { Fail("set %d pose %d: no BankMesh", KV.first, K); continue; }
				for (size_t Q = 0; Q < static_cast<size_t>(Pts) * 3; ++Q) Off = std::max(Off, static_cast<double>(std::fabs(Mesh[Q] - Body[static_cast<size_t>(K) * static_cast<size_t>(Pts) * 3 + Q])));
				++Poses;
			}
			for (const Lod Lv : { Mid, Far })
			{
				const Look& Own = V.LookOf(I, Lv);
				const Look& Bl = V.BankBodyLook(KV.first, Lv);
				if (Bl.Triangles.size() > Own.Triangles.size() || !std::equal(Bl.Triangles.begin(), Bl.Triangles.end(), Own.Triangles.begin()))
					Fail("set %d's %s triangles are not man %d's body's", KV.first, Lv == Mid ? "mid" : "far", I);
			}
			const int Fi = V.HelmetOf(I);
			if (Fi >= 0)
			{
				std::vector<float> Hp, Hf;
				if (!V.HelmetMesh(KV.first, Fi, Far, Hp, Hf) || static_cast<int>(Hp.size()) != V.HelmetLook(KV.first, Fi, Far).Points * 3) Fail("set %d finish %d: no helmet mesh", KV.first, Fi);
			}
		}
		std::printf("buffer against BankMesh: %ld poses, worst %.3g m\n", Poses, Off);
		if (Off > 0.0) Fail("the buffer is %.3g m off BankMesh", Off);
	}

	// Play.
	const double Dt = 1.0 / 60.0;
	double LastDeath = 0.0;
	for (int I = 0; I < N; ++I) LastDeath = std::max(LastDeath, V.Info(I).Dead);
	double End = std::max(Rec["end"].Num(270.0), LastDeath + 12.0);
	if (const char* Secs = std::getenv("LAND_BANK_SECONDS")) End = std::min(End, std::atof(Secs));
	const int Frames = static_cast<int>(std::ceil(End / Dt)) + 1;
	const int Every = 30;
	std::vector<double> UpdV, UpdL;
	std::vector<double> DrawMid, DrawFar, DrawMidPx, DrawFarPx, NearestMid, NearestMidPx;  // the instance against the live man
	std::vector<double> Pop, PopPx, PopNearestPx;                                          // the frame he goes near to mid
	std::vector<double> SelfMid, HelmetOff;
	std::map<std::string, std::vector<double>> MidWhy, MidClip;   // the instance's px at mid by what he does, by clip
	long BankedFrames = 0, MidFrames = 0, ManFrames = 0, ShowOff = 0, BeforeOff = 0, Shows = 0, Blended = 0;
	long EventsV = 0, EventsL = 0, EventsOff = 0, Crossings = 0, CrossedSkinned = 0;
	std::vector<int> LodWas(static_cast<size_t>(N), -1), NearAt(static_cast<size_t>(N), -1000);
	std::vector<char> BankedWas(static_cast<size_t>(N), 0);
	std::vector<double> Comes, ComesPx;   // a man coming into the bank at mid otherwise (his clip into the bank's)
	std::vector<Crowd::Show> ShowWas(static_cast<size_t>(N));
	std::vector<float> Inst, Live, Skinned, Fa, HelmetAt, HelmetFa;
	double EyeX = 0, EyeY = 0;
	for (int Fr = 0; Fr < Frames; ++Fr)
	{
		const double T = Fr * Dt;
		if (Fr % 60 == 0)
		{
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
		V.SetEye(EyeX, EyeY, EyeZ);
		L.SetEye(EyeX, EyeY, EyeZ);
		auto T0 = std::chrono::steady_clock::now();
		V.Update(T);
		auto T1 = std::chrono::steady_clock::now();
		L.Update(T);
		auto T2 = std::chrono::steady_clock::now();
		UpdV.push_back(Ms(T0, T1));
		UpdL.push_back(Ms(T1, T2));
		{
			const std::vector<Event>& Ev = V.Events();
			const std::vector<Event>& El = L.Events();
			EventsV += static_cast<long>(Ev.size());
			EventsL += static_cast<long>(El.size());
			if (Ev.size() != El.size()) ++EventsOff;
			else
			{
				for (size_t K = 0; K < Ev.size(); ++K)
				{
					if (Ev[K].Man != El[K].Man || Ev[K].Fx != El[K].Fx || std::fabs(Ev[K].T - El[K].T) > 1e-9) { ++EventsOff; break; }
				}
			}
		}

		for (int I = 0; I < N; ++I)
		{
			const size_t Iz = static_cast<size_t>(I);
			if (!V.Visible(I)) { LodWas[Iz] = -1; continue; }
			++ManFrames;
			const Lod Lv = V.LodOf(I);
			MidFrames += Lv == Mid;
			const int K = V.BankPose(I);
			const Crowd::Show Sh = V.BankShow(I);
			// (BankShowBefore is last frame's BankShow, whatever his level then)
			const Crowd::Show Bf = V.BankShowBefore(I);
			if (Fr > 0 && LodWas[Iz] >= 0 && ShowWas[Iz].A >= 0 && (Bf.A != ShowWas[Iz].A || Bf.B != ShowWas[Iz].B || Bf.W != ShowWas[Iz].W)) ++BeforeOff;
			// (a man who comes nearer than 25 m is posed live; the frame after he goes out past it again he is an
			// instance: the pop at 25 m is that frame's instance against his live mid skin)
			if (Lv == Near) NearAt[Iz] = Fr;
			const bool bCameIn = K >= 0 && Lv == Mid && !BankedWas[Iz];
			const bool bCrossed = bCameIn && Fr - NearAt[Iz] <= 2;
			if (LodWas[Iz] == static_cast<int>(Near) && Lv == Mid && K < 0) ++CrossedSkinned;
			LodWas[Iz] = static_cast<int>(Lv);
			BankedWas[Iz] = K >= 0;
			// (the raw show, kept whatever the level, for next frame's BankShowBefore)
			ShowWas[Iz] = Sh;
			if (K < 0) continue;
			++BankedFrames;
			++Shows;
			Blended += Sh.W > 0.f;
			if ((Sh.W < 0.5f ? Sh.A : Sh.B) != K) ++ShowOff;
			if (V.BankLevel(I) != static_cast<int>(Lv)) Fail("man %d banked at level %d, his level %d", I, V.BankLevel(I), static_cast<int>(Lv));
			const bool bSample = bCameIn || Fr % Every == 0;
			if (!bSample) continue;
			const int Key = V.BodyKey(I);
			const int Pts = V.BankBodyPoints(Key, Lv);
			Instance(Bodies[{ Key, static_cast<int>(Lv) }], Pts, Sh, V.WorldFrame(I), Inst);
			Live.resize(static_cast<size_t>(L.PointsOf(I, Lv)) * 3);
			Fa.resize(Live.size());
			L.Skin(I, Lv, Live.data(), Fa.data());
			const double D = Worst(Inst.data(), Live.data(), Pts);
			const double Px = D * PxPerM(V.SeenDistance(I));
			// (the bank's nearest pose alone, as before: what the blend gains)
			Crowd::Show One;
			One.A = One.B = K;
			Instance(Bodies[{ Key, static_cast<int>(Lv) }], Pts, One, V.WorldFrame(I), Skinned);
			const double Dn = Worst(Skinned.data(), Live.data(), Pts);
			if (bCrossed)
			{
				++Crossings;
				Pop.push_back(D);
				PopPx.push_back(Px);
				PopNearestPx.push_back(Dn * PxPerM(V.SeenDistance(I)));
			}
			else if (bCameIn)
			{
				Comes.push_back(D);
				ComesPx.push_back(Px);
			}
			if (Fr % Every != 0) continue;
			if (Lv == Mid)
			{
				DrawMid.push_back(D);
				DrawMidPx.push_back(Px);
				const Playing Pl = L.PlayingOf(I);
				MidWhy[Pl.bShooting ? "shot shown" : Pl.bBlending ? "two clips blending" : Sh.W > 0.f ? "between two poses" : "one pose"].push_back(Px);
				MidClip[Pl.Clip].push_back(Px);
				NearestMid.push_back(Dn);
				NearestMidPx.push_back(Dn * PxPerM(V.SeenDistance(I)));
				// (his own mid skin, banked: the nearest pose's buffer placed by his frame, but his face's few points)
				Skinned.resize(static_cast<size_t>(V.PointsOf(I, Mid)) * 3);
				Fa.resize(Skinned.size());
				V.Skin(I, Mid, Skinned.data(), Fa.data());
				Instance(Bodies[{ Key, static_cast<int>(Mid) }], Pts, One, V.WorldFrame(I), Inst);
				std::vector<double> Ds;
				for (int P = 0; P < Pts; ++P)
				{
					const size_t Q = static_cast<size_t>(P) * 3;
					Ds.push_back(std::sqrt(static_cast<double>((Inst[Q] - Skinned[Q]) * (Inst[Q] - Skinned[Q]) + (Inst[Q + 1] - Skinned[Q + 1]) * (Inst[Q + 1] - Skinned[Q + 1]) + (Inst[Q + 2] - Skinned[Q + 2]) * (Inst[Q + 2] - Skinned[Q + 2]))));
				}
				SelfMid.push_back(Quantile(Ds, 0.9));
			}
			else
			{
				DrawFar.push_back(D);
				DrawFarPx.push_back(Px);
			}
			// The helmet: HelmetFrame on the finish's points against his skin's finish (one pose shown: exact).
			const int Fi = V.HelmetOf(I);
			if (Fi >= 0 && Sh.W == 0.f)
			{
				Xform Hf;
				if (!V.HelmetFrame(I, Hf) || !V.HelmetMesh(Key, Fi, Lv, HelmetAt, HelmetFa)) { Fail("man %d: no helmet frame or mesh", I); continue; }
				const int Hp = static_cast<int>(HelmetAt.size() / 3);
				const int Total = V.PointsOf(I, Lv);
				Skinned.resize(static_cast<size_t>(Total) * 3);
				Fa.resize(Skinned.size());
				V.Skin(I, Lv, Skinned.data(), Fa.data());
				if (Total - Pts != Hp) { Fail("man %d: %d finish points skinned, the helmet mesh %d", I, Total - Pts, Hp); continue; }
				double Hw = 0.0;
				for (int P = 0; P < Hp; ++P)
				{
					const Vec3 At = Hf.Point({ HelmetAt[static_cast<size_t>(P) * 3], HelmetAt[static_cast<size_t>(P) * 3 + 1], HelmetAt[static_cast<size_t>(P) * 3 + 2] });
					const float* S = &Skinned[static_cast<size_t>(Pts + P) * 3];
					Hw = std::max(Hw, std::sqrt(static_cast<double>((At.X - S[0]) * (At.X - S[0]) + (At.Y - S[1]) * (At.Y - S[1]) + (At.Z - S[2]) * (At.Z - S[2]))));
				}
				HelmetOff.push_back(Hw);
			}
		}
	}

	std::printf("played %.0f s at 60 frames a second: %.1f%% of the men's frames at mid, %.1f%% banked (mid and far); %.1f%% of banked frames between two poses\n", End,
		100.0 * static_cast<double>(MidFrames) / static_cast<double>(std::max(1L, ManFrames)), 100.0 * static_cast<double>(BankedFrames) / static_cast<double>(std::max(1L, ManFrames)),
		100.0 * static_cast<double>(Blended) / static_cast<double>(std::max(1L, Shows)));
	std::printf("BankPose the nearer of BankShow's two: off in %ld of %ld; BankShowBefore not last frame's: %ld\n", ShowOff, Shows, BeforeOff);
	if (ShowOff > Shows / 1000) Fail("BankPose is not BankShow's nearer pose in %ld frames", ShowOff);
	if (BeforeOff) Fail("BankShowBefore was not the last frame's BankShow %ld times", BeforeOff);
	std::printf("the instance (two poses lerped) vs the live man, his worst point: mid median %.3f m / p99 %.3f / worst %.3f (%.2f / %.2f / %.2f px), %zu samples; far median %.3f / p99 %.3f (%.2f / %.2f px)\n",
		Quantile(DrawMid, 0.5), Quantile(DrawMid, 0.99), Quantile(DrawMid, 1.0), Quantile(DrawMidPx, 0.5), Quantile(DrawMidPx, 0.99), Quantile(DrawMidPx, 1.0), DrawMid.size(),
		Quantile(DrawFar, 0.5), Quantile(DrawFar, 0.99), Quantile(DrawFarPx, 0.5), Quantile(DrawFarPx, 0.99));
	std::printf("  at mid by what he does (median / p99 px, share):");
	for (const auto& KV : MidWhy) std::printf(" %s %.2f / %.2f (%.1f%%);", KV.first.c_str(), Quantile(KV.second, 0.5), Quantile(KV.second, 0.99), 100.0 * static_cast<double>(KV.second.size()) / static_cast<double>(std::max<size_t>(1, DrawMid.size())));
	std::printf("\n  at mid by clip (median / p99 px, samples):");
	{
		std::vector<std::pair<double, std::string>> By;
		for (const auto& KV : MidClip) By.push_back({ Quantile(KV.second, 0.99), KV.first });
		std::sort(By.rbegin(), By.rend());
		for (const auto& KV : By) std::printf(" %s %.2f / %.2f (%zu);", KV.second.c_str(), Quantile(MidClip[KV.second], 0.5), KV.first, MidClip[KV.second].size());
	}
	std::printf("\n");
	std::printf("  the nearest pose alone at mid: median %.3f m / p99 %.3f (%.2f / %.2f px)\n", Quantile(NearestMid, 0.5), Quantile(NearestMid, 0.99), Quantile(NearestMidPx, 0.5), Quantile(NearestMidPx, 0.99));
	std::printf("the pop at 25 m (near to mid, %ld crossings; the crossing frame itself skinned live %ld times, as it should): the instance off the live mid skin median %.3f m / p90 %.3f / p99 %.3f; %.2f / %.2f / %.2f px (the nearest pose alone %.2f / %.2f / %.2f px)\n",
		Crossings, CrossedSkinned, Quantile(Pop, 0.5), Quantile(Pop, 0.9), Quantile(Pop, 0.99), Quantile(PopPx, 0.5), Quantile(PopPx, 0.9), Quantile(PopPx, 0.99),
		Quantile(PopNearestPx, 0.5), Quantile(PopNearestPx, 0.9), Quantile(PopNearestPx, 0.99));
	std::printf("  a mid man's clip coming into the bank (%zu): median %.3f m / p99 %.3f; %.2f / %.2f px\n", Comes.size(), Quantile(Comes, 0.5), Quantile(Comes, 0.99), Quantile(ComesPx, 0.5), Quantile(ComesPx, 0.99));
	if (Quantile(PopPx, 0.5) > 1.0) Fail("the pop at 25 m: half the crossings move a point %.2f px or more", Quantile(PopPx, 0.5));
	std::printf("his banked mid skin vs the buffer's pose placed by his frame (p90 of his points, worst man): %.4f m\n", Quantile(SelfMid, 1.0));
	if (Quantile(SelfMid, 1.0) > 0.001) Fail("a banked mid skin is %.4f m off the buffer", Quantile(SelfMid, 1.0));
	std::printf("helmet by HelmetFrame vs his skin's finish (%zu samples): worst %.4f m\n", HelmetOff.size(), Quantile(HelmetOff, 1.0));
	if (Quantile(HelmetOff, 1.0) > 0.001) Fail("a helmet by HelmetFrame is %.4f m off his skin's", Quantile(HelmetOff, 1.0));
	std::printf("the play: events %ld with the bank, %ld without; %ld frames differ\n", EventsV, EventsL, EventsOff);
	if (EventsOff || EventsV != EventsL) Fail("the bank changed the play's events (%ld frames)", EventsOff);
	std::printf("Update, ms (mean / p95): %.2f / %.2f with mid and far banked, %.2f / %.2f live\n", Mean(UpdV), Quantile(UpdV, 0.95), Mean(UpdL), Quantile(UpdL, 0.95));
	if (!BankedFrames || DrawMid.empty()) Fail("no mid man was banked");
	// The bank read again after Build (its bakes made anew; the sets made again as the men are attached): the same sets,
	// the same buffers.
	{
		const int Sets = V.BodyKeys();
		if (!V.ReadPoseBank(BankText, Err)) Fail("the bank could not be read again: %s", Err.c_str());
		if (V.BodyKeys() != Sets) Fail("%d body sets after reading the bank again, %d before", V.BodyKeys(), Sets);
		long Off = 0;
		for (const auto& KV : Bodies)
		{
			std::vector<float> At, Fa2;
			if (!V.BankBody(KV.first.first, static_cast<Lod>(KV.first.second), At, Fa2) || At != KV.second) ++Off;
		}
		std::printf("the bank read again after Build: %d body sets, %ld buffers differ\n", V.BodyKeys(), Off);
		if (Off) Fail("%ld buffers differ after reading the bank again", Off);
	}
	std::printf(Fails ? "%d FAILED\n" : "all passed\n", Fails);
	return Fails ? 1 : 0;
}
