// Bullet hits' cloud test (Crowd::SetHits, -TheaterLandHits): builds Battle/LandMen (the game's own code, no engine)
// and checks, each part alone where it can:
//   off       every part off: each man's pose bits (every second, every man, thrift on, Life 2) and every fall (clip,
//             kind, stance, side, start) over the battles given, hashed; run.sh compares the hash with the same tool
//             built against the base (LAND_HITS_BASE) and between g++ and clang++.
//   jolt      one German rifleman killed by a rifle (and an mg) from his front, his left, behind, and running: at his
//             death + 0.05 s his chest turned away from the shooter, toward the struck shoulder; pelvis and clavicles
//             untouched to the bit; his fists on the rifle (within 1 cm of the unjolted pose in the rifle's frame);
//             the pose his own to the bit at + 0.34 s; no pop over the fall's blend; an mg's two kicks; a leg fall's knee.
//   variety   over the battles: no clip a body within 15 m showed in the last 5 s unless his rate or start differ, no
//             man limp, none on the old three-clip table, the standing falls reached; the share by kind printed.
//   struck    StruckPlace: within 0.5 m of his place, at his stance's height; a head fall standing above 1.2 m.
//   wounded   hand-written rows (wd, back, cy as the feed reads them): three men within 5 m go down three ways or at
//             three phases, never face down onto his back, stay down, are up at back; a carried man left as before.
// No NaN anywhere. Usage: land_hits_test <soldiers folder> <armies.json> [battle.json ...]
// Built with -DLAND_HITS_BASE it prints only the off lines, against a LandMen without SetHits (the base).
#include "Battle/LandMen.h"
#include "land_json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
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
	std::string ReadFile(const std::string& P)
	{
		std::ifstream F(P, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
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
			return H.empty() ? 0.f : H[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)];
		}
		Ground operator()(double X, double Y) const
		{
			const double FX = X / Cell - 0.5, FY = Y / Cell - 0.5;
			const int C0 = static_cast<int>(std::floor(FX)), R0 = static_cast<int>(std::floor(FY));
			const double U = FX - C0, V = FY - R0;
			const double H00 = At(C0, R0), H10 = At(C0 + 1, R0), H01 = At(C0, R0 + 1), H11 = At(C0 + 1, R0 + 1);
			Ground G;
			G.Height = static_cast<float>(H00 * (1 - U) * (1 - V) + H10 * U * (1 - V) + H01 * (1 - U) * V + H11 * U * V);
			return G;
		}
	};

	struct Files
	{
		std::map<std::string, std::shared_ptr<Soldier>> ByArmy;
		std::string Armies;
	};

	Files ReadFiles(const std::string& Folder, const std::string& ArmiesPath)
	{
		Files Fs;
		DIR* Dir = opendir(Folder.c_str());
		std::vector<std::string> Names;
		if (Dir)
		{
			while (dirent* E = readdir(Dir))
			{
				const std::string N = E->d_name;
				if (N.rfind("soldier_", 0) == 0 && N.size() > 13 && N.substr(N.size() - 5) == ".json" && N.find("_gore") == std::string::npos) Names.push_back(N);
			}
			closedir(Dir);
		}
		std::sort(Names.begin(), Names.end());
		for (const std::string& N : Names)
		{
			auto S = std::make_shared<Soldier>();
			std::string Err;
			if (!S->Load(ReadFile(Folder + "/" + N), Err)) { std::printf("skip %s: %s\n", N.c_str(), Err.c_str()); continue; }
			Fs.ByArmy[N.substr(8, N.size() - 13)] = S;
		}
		Fs.Armies = ReadFile(ArmiesPath);
		return Fs;
	}

	uint64_t Fnv(uint64_t H, const void* P, size_t N)
	{
		const unsigned char* B = static_cast<const unsigned char*>(P);
		for (size_t I = 0; I < N; ++I) { H ^= B[I]; H *= 1099511628211ull; }
		return H;
	}
	uint64_t FnvText(uint64_t H, const std::string& S) { return Fnv(H, S.data(), S.size() + 1); }

	struct Row
	{
		int I = 0;
		std::string File, Clip, Kind, Stance, From, W;
		double Dead = -1.0, Started = -1.0, X = 0.0, Y = 0.0;
		float Rate = 1.f, Skip = 0.f;
		bool bDeaths = false;
	};

	struct Played
	{
		uint64_t Hash = 1469598103934665603ull;
		std::vector<Row> Rows;
		bool bNaN = false;
		int Men = 0;
	};

	/** One battle through the crowd as falls_measure plays it (30 fps, Life 2, thrift on), every man's pose bits hashed
	 * once a battle second, and each dead man's fall read back. */
	Played PlayBattle(const std::string& RecText, const Files& Fs, const void* HitsPtr)
	{
		Played Out;
		LandJson::Value Rec;
		if (!LandJson::Parse(RecText, Rec)) { std::printf("FAIL: cannot read the recording\n"); std::exit(1); }
		Crowd C;
		C.SetLife(2.0);
		for (const auto& KV : Fs.ByArmy) C.AddSoldier(KV.first, KV.second);
		std::string Err;
		if (!C.ReadArmies(Fs.Armies, Err)) { std::printf("FAIL: armies: %s\n", Err.c_str()); std::exit(1); }
#ifndef LAND_HITS_BASE
		if (HitsPtr) C.SetHits(*static_cast<const Crowd::Hits*>(HitsPtr));
#else
		(void)HitsPtr;
#endif
		Field Fd;
		{
			const auto& F = Rec["field"];
			Fd.Cols = F["cols"].Int(120);
			Fd.Rows = F["rows"].Int(80);
			Fd.Cell = F["cell"].Num(25.0);
			for (size_t I = 0; I < F["height"].Count(); ++I) Fd.H.push_back(static_cast<float>(F["height"].At(I).Num()));
		}
		const GroundFn GroundAt = [&Fd](double X, double Y) { return Fd(X, Y); };
		if (!C.Build(RecText, GroundAt, Err)) { std::printf("FAIL: build: %s\n", Err.c_str()); std::exit(1); }
		C.SetThrift(true);
		const int N = C.Count();
		Out.Men = N;
		struct K { double T; std::string W; };
		std::vector<std::vector<K>> Kills(static_cast<size_t>(N));
		const auto& Rs = Rec["rounds"];
		for (size_t I = 0; I < Rs.Count(); ++I)
		{
			const auto& R = Rs.At(I);
			if (R["r"].Str("") != "kill" || !R.Has("ok")) continue;
			const int M = C.Find(R["on"].Int(-1), R["ok"].Int(-1), std::max(0, R["oj"].Int(0)));
			if (M >= 0) Kills[static_cast<size_t>(M)].push_back({ R["t"].Num(), R["w"].Str("") });
		}
		double Last = 0.0;
		for (int I = 0; I < N; ++I) Last = std::max(Last, C.Info(I).Dead);
		const double End = Last + 4.0;
		std::vector<double> X(static_cast<size_t>(N), 0.0), Y(static_cast<size_t>(N), 0.0);
		std::vector<char> Got(static_cast<size_t>(N), 0);
		uint64_t H = Out.Hash;
		for (int F = 0; F * (1.0 / 30.0) <= End; ++F)
		{
			C.Update(F / 30.0);
			for (int I = 0; I < N; ++I)
			{
				if (C.Info(I).FallStarted >= 0.0 && !Got[static_cast<size_t>(I)])
				{
					double Z = 0.0;
					float Fa = 0.f;
					C.PlaceOf(I, X[static_cast<size_t>(I)], Y[static_cast<size_t>(I)], Z, Fa);
					Got[static_cast<size_t>(I)] = 1;
				}
			}
			if (F % 30 != 0) continue;
			for (int I = 0; I < N; ++I)
			{
				if (!C.Visible(I)) continue;
				const Xform* Fr = C.FramesOf(I);
				const Soldier* S = C.SoldierOf(I);
				if (!Fr || !S) continue;
				const size_t NB = std::min(S->Bones.size(), static_cast<size_t>(MaxBones));
				H = Fnv(H, Fr, NB * sizeof(Xform));
				for (size_t B = 0; B < NB; ++B)
				{
					for (float V : Fr[B].R) Out.bNaN = Out.bNaN || !std::isfinite(V);
					for (float V : Fr[B].T) Out.bNaN = Out.bNaN || !std::isfinite(V);
				}
			}
		}
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(I);
			H = FnvText(H, In.FallClip);
			H = FnvText(H, In.FallKind);
			H = FnvText(H, In.FallStance);
			H = FnvText(H, In.FallFrom);
			H = Fnv(H, &In.FallStarted, sizeof(double));
			if (In.Dead < 0.0) continue;
			Row R;
			R.I = I;
			R.File = In.FileArmy;
			R.Clip = In.FallClip.empty() ? "-" : In.FallClip;
			R.Kind = In.FallKind;
			R.Stance = In.FallStance;
			R.From = In.FallFrom;
			R.Dead = In.Dead;
			R.Started = In.FallStarted;
			R.X = X[static_cast<size_t>(I)];
			R.Y = Y[static_cast<size_t>(I)];
#ifndef LAND_HITS_BASE
			R.Rate = In.FallRate;
			R.Skip = In.FallSkip;
#endif
			const Soldier* S = C.SoldierOf(I);
			R.bDeaths = S && !S->Deaths.empty();
			const K* Best = nullptr;
			for (const K& Kk : Kills[static_cast<size_t>(I)])
			{
				if (!Best || std::fabs(Kk.T - In.Dead) < std::fabs(Best->T - In.Dead)) Best = &Kk;
			}
			R.W = Best ? Best->W : "none";
			Out.Rows.push_back(R);
		}
		Out.Hash = H;
		return Out;
	}

	std::string Hex(uint64_t H)
	{
		char B[24];
		std::snprintf(B, sizeof(B), "%016llx", static_cast<unsigned long long>(H));
		return B;
	}

	std::string NameOf(const std::string& Path)
	{
		const size_t S = Path.find_last_of('/');
		std::string N = S == std::string::npos ? Path : Path.substr(S + 1);
		if (N.size() > 5 && N.substr(N.size() - 5) == ".json") N.resize(N.size() - 5);
		return N;
	}
}

#ifndef LAND_HITS_BASE
namespace
{
	int Failed = 0;
	void Check(bool bOk, const std::string& What)
	{
		std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str());
		Failed += bOk ? 0 : 1;
	}
	std::string Fmt(const char* F, double A, double B = 0.0, double C = 0.0)
	{
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), F, A, B, C);
		return Buf;
	}

	const double Hit = 5.0;

	/** A little battle of its own: GER men (block 2) killed or wounded, USA shooters (block 1) where the case says. */
	struct Man1
	{
		double X = 0.0, Y = 0.0, F = 0.0, Go = 0.0;
		std::string Pose = "fire_stand", Cover = "none";
		double D = -1.0, Wd = -1.0, Back = -1.0;
		bool bCarried = false;
		bool bShooter = false;
		double SX = 0.0, SY = 0.0;
		std::string W = "rifle";
	};

	std::string Little(const std::vector<Man1>& Men)
	{
		std::ostringstream R, Sh, Rounds;
		R.precision(10);
		Sh.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": 41, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}], \"men\": [";
		Rounds << "\"rounds\": [";
		bool bRound = false;
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const Man1& M = Men[I];
			const int K = static_cast<int>(I) + 1;
			const double T0 = 0.0, T1 = 40.0;
			R << (I ? "," : "") << "{\"b\": 2, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[" << T0 << ", " << M.X - std::sin(M.F) * M.Go * Hit << ", "
			  << M.Y - std::cos(M.F) * M.Go * Hit << ", " << M.F << "], [" << T1 << ", " << M.X + std::sin(M.F) * M.Go * (T1 - Hit) << ", "
			  << M.Y + std::cos(M.F) * M.Go * (T1 - Hit) << ", " << M.F << "]], \"ch\": [[0, \"" << M.Pose << "\", \"" << M.Cover << "\", 0, 0, 0, 0]]";
			if (M.D >= 0.0) R << ", \"d\": " << M.D;
			if (M.Wd >= 0.0) R << ", \"wd\": " << M.Wd;
			if (M.Back >= 0.0) R << ", \"back\": " << M.Back;
			if (M.bCarried) R << ", \"cy\": [[" << M.Wd + 5.0 << ", " << M.Wd + 30.0 << ", [1, 90, 0], [1, 91, 0]]]";
			R << "}";
			if (M.bShooter)
			{
				const double Face = std::atan2(M.X - M.SX, M.Y - M.SY);
				Sh << ", {\"b\": 1, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[0, " << M.SX << ", " << M.SY << ", " << Face << "], [40, " << M.SX << ", "
				   << M.SY << ", " << Face << "]], \"ch\": [[0, \"fire_prone\", \"none\", 0, 0, 0, 0]]}";
			}
			if (M.D >= 0.0)
			{
				Rounds << (bRound ? "," : "") << "{\"t\": " << M.D << ", \"l\": " << M.D << ", \"w\": \"" << M.W << "\", ";
				if (M.bShooter) Rounds << "\"by\": 1, \"k\": " << K << ", ";
				Rounds << "\"on\": 2, \"ok\": " << K << ", \"x\": " << M.X << ", \"y\": " << M.Y << ", \"r\": \"kill\"}";
				bRound = true;
			}
		}
		R << Sh.str() << "], " << Rounds.str() << "]}";
		return R.str();
	}

	void MakeCrowd(Crowd& C, const Files& Fs, const std::string& Rec, const Crowd::Hits& H)
	{
		C.SetLife(2.0);
		for (const auto& KV : Fs.ByArmy) C.AddSoldier(KV.first, KV.second);
		std::string Err;
		C.SetHits(H);
		if (!C.ReadArmies(Fs.Armies, Err) || !C.Build(Rec, [](double, double) { return Ground(); }, Err)) { std::printf("FAIL: build: %s\n", Err.c_str()); std::exit(1); }
		C.SetEye(-1000, -1000, 2);
	}

	Vec3 BonePoint(const Crowd& C, int Man, const char* Name)
	{
		const Soldier* S = C.SoldierOf(Man);
		const int B = S ? S->BoneIndex(Name) : -1;
		if (B < 0) return {};
		return C.FramesOf(Man)[B].Point(S->Bones[static_cast<size_t>(B)].Head);
	}
	Vec3 Minus(const Vec3& A, const Vec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
	double Len(const Vec3& A) { return std::sqrt(static_cast<double>(A.X) * A.X + static_cast<double>(A.Y) * A.Y + static_cast<double>(A.Z) * A.Z); }

	/** A point in a bone's own frame (the frame's inverse). */
	Vec3 InFrame(const Xform& F, const Vec3& P)
	{
		const Vec3 D { P.X - F.T[0], P.Y - F.T[1], P.Z - F.T[2] };
		return { F.R[0] * D.X + F.R[3] * D.Y + F.R[6] * D.Z, F.R[1] * D.X + F.R[4] * D.Y + F.R[7] * D.Z, F.R[2] * D.X + F.R[5] * D.Y + F.R[8] * D.Z };
	}

	bool SameBits(const Pose& A, const Pose& B, int Bone)
	{
		return std::memcmp(&A.Rot[Bone], &B.Rot[Bone], sizeof(Quat)) == 0 && std::memcmp(&A.Move[Bone], &B.Move[Bone], sizeof(Vec3)) == 0;
	}

	bool FramesFinite(const Crowd& C, int Man)
	{
		const Soldier* S = C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		for (size_t B = 0; S && B < S->Bones.size(); ++B)
		{
			for (float V : Fr[B].R) if (!std::isfinite(V)) return false;
			for (float V : Fr[B].T) if (!std::isfinite(V)) return false;
		}
		return true;
	}

	/** Both crowds played over the same frames (30 a second) to just short of T, then to T. */
	void PlayTo(Crowd& A, Crowd& B, double& Clock, double T)
	{
		while (Clock + 1.0 / 30.0 < T)
		{
			Clock += 1.0 / 30.0;
			A.Update(Clock);
			B.Update(Clock);
		}
		Clock = T;
		A.Update(T);
		B.Update(T);
	}

	void JoltCase(const Files& Fs, const char* Name, double SX, double SY, double Go, const std::string& W, bool bLegFall, int Fillers = 0)
	{
		Man1 M;
		M.Go = Go;
		M.Pose = Go > 0.0 ? "advance" : "fire_stand";
		M.D = Hit;
		M.bShooter = true;
		M.SX = SX;
		M.SY = SY;
		M.W = W;
		// (men before him standing far off, unhurt, so his own dice are another man's: a leg fall's case asks for a man whose
		// dice give one of the kind his lead names, SetFallKind)
		std::vector<Man1> All;
		for (int K = 0; K < Fillers; ++K)
		{
			Man1 F;
			F.X = 500.0 + 10.0 * K;
			All.push_back(F);
		}
		All.push_back(M);
		const std::string Rec = Little(All);
		Crowd A, B;
		Crowd::Hits On;
		On.bJolt = true;
		MakeCrowd(A, Fs, Rec, Crowd::Hits());
		MakeCrowd(B, Fs, Rec, On);
		const int Ix = A.Find(2, Fillers + 1, 0);
		if (bLegFall) { A.SetFallKind(Ix, "leg"); B.SetFallKind(Ix, "leg"); }
		double Clock = 0.0;
		A.Update(0.0);
		B.Update(0.0);
		PlayTo(A, B, Clock, Hit - 1.0 / 30.0);
		// (the fall's opening at 120 frames a second: the largest move of any bone a frame, unjolted and jolted)
		double MostA = 0.0, MostB = 0.0, Peak = 0.0;
		std::vector<double> Away;
		std::vector<Vec3> WasA, WasB;
		const Soldier* S = A.SoldierOf(Ix);
		const size_t NB = S->Bones.size();
		auto Points = [&](const Crowd& C) {
			std::vector<Vec3> P;
			for (size_t K = 0; K < NB; ++K) P.push_back(C.FramesOf(Ix)[K].Point(S->Bones[K].Head));
			return P;
		};
		const double Fx = std::sin(0.0), Fy = std::cos(0.0);
		double DX = 0.0 - SX, DY = 0.0 - SY;
		const double DL = std::hypot(DX, DY);
		DX /= DL;
		DY /= DL;
		bool bFinite = true;
		for (int Step = 0; Step <= 60; ++Step)
		{
			const double T = Hit + Step / 120.0;
			A.Update(T);
			B.Update(T);
			bFinite = bFinite && FramesFinite(B, Ix);
			const std::vector<Vec3> PA = Points(A), PB = Points(B);
			if (!WasA.empty())
			{
				for (size_t K = 0; K < NB; ++K)
				{
					if (S->OnWeapon(static_cast<int>(K))) continue;
					MostA = std::max(MostA, Len(Minus(PA[K], WasA[K])));
					MostB = std::max(MostB, Len(Minus(PB[K], WasB[K])));
				}
			}
			WasA = PA;
			WasB = PB;
			const Vec3 D = Minus(BonePoint(B, Ix, "neck"), BonePoint(A, Ix, "neck"));
			Away.push_back(D.X * DX + D.Y * DY);
			Peak = std::max(Peak, Len(D));
		}
		Clock = Hit + 0.5;
		// The checks at death + 0.05 s (a new pair of crowds, played to it).
		Crowd A2, B2;
		MakeCrowd(A2, Fs, Rec, Crowd::Hits());
		MakeCrowd(B2, Fs, Rec, On);
		if (bLegFall) { A2.SetFallKind(Ix, "leg"); B2.SetFallKind(Ix, "leg"); }
		double C2 = 0.0;
		A2.Update(0.0);
		B2.Update(0.0);
		PlayTo(A2, B2, C2, Hit + 0.05);
		Pose PA, PB;
		A2.PoseOf(Ix, PA);
		B2.PoseOf(Ix, PB);
		const int Pelvis = S->BoneIndex("pelvis"), ClL = S->BoneIndex("clavicle_l"), ClR = S->BoneIndex("clavicle_r");
		const std::string Fall = B2.Info(Ix).FallClip;
		std::printf("  jolt %s (%s, %s): fall %s, the neck moved at most %.1f cm\n", Name, W.c_str(), Go > 0.0 ? "running" : "standing", Fall.c_str(), 100.0 * Peak);
		Check(A2.Info(Ix).FallClip == Fall, std::string("jolt ") + Name + ": the same fall jolted or not (variety off)");
		const Vec3 Neck = Minus(BonePoint(B2, Ix, "neck"), BonePoint(A2, Ix, "neck"));
		const double Off = Neck.X * DX + Neck.Y * DY;
		if (!bLegFall) Check(Off > 0.01, std::string("jolt ") + Name + Fmt(": at death + 0.05 s the chest is turned away from the shooter (%.1f cm away)", 100.0 * Off));
		else Check(Off > 0.002, std::string("jolt ") + Name + Fmt(": at death + 0.05 s the chest leans away from the shooter (%.1f cm away)", 100.0 * Off));
		Check(SameBits(PA, PB, Pelvis) && SameBits(PA, PB, ClL) && SameBits(PA, PB, ClR) && SameBits(PA, PB, 0),
			std::string("jolt ") + Name + ": root, pelvis and both clavicles untouched, to the bit");
		bool bLegs = true;
		for (const char* Leg : { "thigh_l", "thigh_r", "foot_l", "foot_r", "pouch_l", "pouch_r" })
		{
			const int Bi = S->BoneIndex(Leg);
			if (Bi >= 0 && !SameBits(PA, PB, Bi)) bLegs = false;
		}
		if (!bLegFall)
		{
			bool bCalves = true;
			for (const char* Leg : { "calf_l", "calf_r" }) bCalves = bCalves && SameBits(PA, PB, S->BoneIndex(Leg));
			Check(bLegs && bCalves, std::string("jolt ") + Name + ": thighs, calves, feet and pouches untouched, to the bit");
		}
		else
		{
			const bool bKnee = !SameBits(PA, PB, S->BoneIndex("calf_l")) || !SameBits(PA, PB, S->BoneIndex("calf_r"));
			Check(bLegs && bKnee && Fall.rfind("die_leg_", 0) == 0, std::string("jolt ") + Name + ": a leg fall buckles a knee (a calf turned), thighs and feet untouched");
		}
		// His fists on the rifle: each fist in the rifle's own frame, jolted against not.
		const int Gun = S->WeaponBone();
		double Grip = 0.0;
		for (const char* Hand : { "hand_l", "hand_r" })
		{
			const Vec3 Ia = InFrame(A2.FramesOf(Ix)[Gun], BonePoint(A2, Ix, Hand)), Ib = InFrame(B2.FramesOf(Ix)[Gun], BonePoint(B2, Ix, Hand));
			Grip = std::max(Grip, Len(Minus(Ia, Ib)));
		}
		Check(Grip < 0.01, std::string("jolt ") + Name + Fmt(": the fists keep the rifle (%.2f mm off the unjolted grip at most)", 1000.0 * Grip));
		// The struck shoulder goes back (the twist), shot from his left: the left shoulder.
		if (SX < -1.0 && !bLegFall)
		{
			const Vec3 L = Minus(BonePoint(B2, Ix, "upperarm_l"), BonePoint(A2, Ix, "upperarm_l"));
			const Vec3 R = Minus(BonePoint(B2, Ix, "upperarm_r"), BonePoint(A2, Ix, "upperarm_r"));
			const double Lb = L.X * Fx + L.Y * Fy, Rb = R.X * Fx + R.Y * Fy;
			Check(Lb < Rb, std::string("jolt ") + Name + Fmt(": the left shoulder (struck) is turned back against the right (%.1f against %.1f cm)", 100.0 * Lb, 100.0 * Rb));
		}
		PlayTo(A2, B2, C2, Hit + 0.34);
		A2.PoseOf(Ix, PA);
		B2.PoseOf(Ix, PB);
		bool bAll = true;
		for (int K = 0; K < MaxBones; ++K) bAll = bAll && SameBits(PA, PB, K);
		Check(bAll, std::string("jolt ") + Name + ": at death + 0.34 s the pose is the unjolted one, to the bit");
		Check(MostB <= MostA + 0.03, std::string("jolt ") + Name + Fmt(": no pop (largest move of a bone in 1/120 s %.1f cm jolted, %.1f cm not)", 100.0 * MostB, 100.0 * MostA));
		Check(bFinite, std::string("jolt ") + Name + ": no NaN");
		if (W == "mg")
		{
			// two kicks: the neck's push away rises twice
			int Peaks = 0;
			for (size_t K = 1; K + 1 < Away.size(); ++K)
			{
				if (Away[K] > Away[K - 1] && Away[K] >= Away[K + 1] && Away[K] > 0.003) ++Peaks;
			}
			Check(Peaks >= 2, std::string("jolt ") + Name + Fmt(": an mg's two kicks (%.0f rises of the push away)", Peaks));
		}
	}

	void Jolts(const Files& Fs)
	{
		std::printf("== jolt\n");
		JoltCase(Fs, "from the front", 0.0, 150.0, 0.0, "rifle", false);
		JoltCase(Fs, "from his left", -150.0, 0.0, 0.0, "rifle", false);
		JoltCase(Fs, "from behind", 0.0, -150.0, 0.0, "rifle", false);
		JoltCase(Fs, "running, from the front", 10.0, 150.0, 2.5, "rifle", false);
		JoltCase(Fs, "mg from his right", 150.0, 20.0, 0.0, "mg", false);
		// (the first man, by how many stand before him, whose dice give him a leg fall where his lead asks one)
		int Fillers = 0;
		for (; Fillers < 16; ++Fillers)
		{
			Man1 M;
			M.D = Hit;
			M.bShooter = true;
			M.SY = 150.0;
			std::vector<Man1> All(static_cast<size_t>(Fillers));
			for (int K = 0; K < Fillers; ++K) All[static_cast<size_t>(K)].X = 500.0 + 10.0 * K;
			All.push_back(M);
			Crowd C;
			MakeCrowd(C, Fs, Little(All), Crowd::Hits());
			C.SetFallKind(C.Find(2, Fillers + 1, 0), "leg");
			for (int F = 0; F <= static_cast<int>(Hit * 30.0) + 2; ++F) C.Update(F / 30.0);
			if (C.Info(C.Find(2, Fillers + 1, 0)).FallClip.rfind("die_leg_", 0) == 0) break;
		}
		JoltCase(Fs, "leg fall from the front", 0.0, 150.0, 0.0, "rifle", true, Fillers);
	}

	void Struck(const Files& Fs)
	{
		std::printf("== struck place\n");
		std::vector<Man1> Men;
		const char* Poses[] = { "fire_stand", "advance", "fire_kneel", "fire_prone", "fire_stand", "fire_stand" };
		const char* Covers[] = { "none", "none", "none", "none", "woods", "none" };
		const char* Bands[] = { "stand", "run", "kneel", "prone", "cover", "head" };
		for (int I = 0; I < 6; ++I)
		{
			Man1 M;
			M.X = 40.0 * I;
			M.Pose = Poses[I];
			M.Cover = Covers[I];
			M.Go = std::strcmp(Poses[I], "advance") == 0 ? 2.5 : 0.0;
			M.D = Hit;
			M.bShooter = true;
			M.SX = M.X;
			M.SY = 150.0;
			Men.push_back(M);
		}
		// one alive, unknown: StruckPlace by his stance alone
		Man1 Alive;
		Alive.X = 300.0;
		Men.push_back(Alive);
		const std::string Rec = Little(Men);
		Crowd::Hits On;
		On.bJolt = true;
		On.bVariety = true;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			Crowd C;
			MakeCrowd(C, Fs, Rec, Pass ? On : Crowd::Hits());
			C.SetFallKind(C.Find(2, 6, 0), "head");
			for (int F = 0; F <= static_cast<int>((Hit + 1.0 / 30.0) * 30.0); ++F) C.Update(F / 30.0);
			for (int I = 0; I < 7; ++I)
			{
				const int Ix = C.Find(2, I + 1, 0);
				Vec3 At;
				std::string Part;
				const bool bGot = C.StruckPlace(Ix, At, Part);
				double X = 0.0, Y = 0.0, Z = 0.0;
				float Fa = 0.f;
				C.PlaceOf(Ix, X, Y, Z, Fa);
				const double Flat = std::hypot(At.X - X, At.Y - Y), H = At.Z - Z;
				const char* Band = I < 6 ? Bands[I] : "alive";
				double Lo = 1.0, Hi = 1.7;
				if (!std::strcmp(Band, "run")) Lo = 0.9;
				if (!std::strcmp(Band, "kneel") || !std::strcmp(Band, "cover")) Lo = 0.5, Hi = 1.2;
				if (!std::strcmp(Band, "prone")) Lo = 0.0, Hi = 0.5;
				if (!std::strcmp(Band, "head")) Lo = 1.2, Hi = 1.9;
				if (!std::strcmp(Band, "alive")) Lo = 1.34, Hi = 1.36;
				// (a gut or a leg fall is struck lower: spine_01, a thigh's middle)
				const bool bLow = !std::strcmp(Band, "kneel") || !std::strcmp(Band, "cover");
				if (Part == "spine_01" && std::strcmp(Band, "prone")) Lo = bLow ? 0.4 : 0.8, Hi = bLow ? 1.0 : 1.35;
				if (Part.rfind("thigh", 0) == 0 && std::strcmp(Band, "prone")) Lo = 0.05, Hi = 1.0;
				std::printf("  struck (%s) %s: %s at %.2f m up, %.2f m off his place, fall %s\n", Pass ? "hits on" : "hits off", Band, Part.c_str(), H, Flat, C.Info(Ix).FallClip.c_str());
				Check(bGot && Flat < 0.5 && H >= Lo && H <= Hi && std::isfinite(H), std::string("struck ") + Band + (Pass ? " (hits on)" : " (hits off)") + Fmt(": within 0.5 m of his place, %.2f m up in [%.2f, %.2f]", H, Lo, Hi));
				if (!std::strcmp(Band, "head")) Check(C.Info(Ix).FallClip.rfind("die_head_", 0) == 0 && Part == "head", "struck head: a head fall struck in the head");
			}
		}
	}

	void Wounded(const Files& Fs)
	{
		std::printf("== wounded\n");
		std::vector<Man1> Men;
		// three men within 5 m, wounded together; a fourth carried (the bearers' way, left as before); a fifth never back
		const double Xs[] = { 0.0, 2.0, 4.0 };
		for (double X : Xs)
		{
			Man1 M;
			M.X = X;
			M.Y = 1.0 * X;
			M.Wd = 6.0;
			M.Back = 30.0;
			Men.push_back(M);
		}
		Man1 Carried;
		Carried.X = 60.0;
		Carried.Wd = 6.0;
		Carried.bCarried = true;
		Men.push_back(Carried);
		Man1 Stays;
		Stays.X = 100.0;
		Stays.Pose = "fire_kneel";
		Stays.Wd = 7.0;
		Men.push_back(Stays);
		const std::string Rec = Little(Men);
		Crowd::Hits On;
		On.bJolt = On.bVariety = On.bWounded = true;
		Crowd C, Off;
		MakeCrowd(C, Fs, Rec, On);
		MakeCrowd(Off, Fs, Rec, Crowd::Hits());
		const int N = 5;
		std::vector<int> Ix;
		for (int I = 0; I < N; ++I) Ix.push_back(C.Find(2, I + 1, 0));
		const Soldier* S = C.SoldierOf(Ix[0]);
		const int Ch = S->BoneIndex("chest"), Pe = S->BoneIndex("pelvis");
		std::vector<int> WasDown(N, 0), Flipped(N, 0), Rose(N, 0);
		std::vector<double> LowestUp(N, 9.0);
		bool bFinite = true;
		for (int F = 0; F <= 36 * 30; ++F)
		{
			const double T = F / 30.0;
			C.Update(T);
			Off.Update(T);
			for (int I = 0; I < N; ++I)
			{
				const int M = Ix[static_cast<size_t>(I)];
				bFinite = bFinite && FramesFinite(C, M);
				const ManInfo& In = C.Info(M);
				if (In.WoundStarted < 0.0 || (In.Back >= 0.0 && T >= In.Back)) continue;
				// (his chest's front, its own x, on the field: face down below -0.5, on his back above 0.5; from his fall's
				// last stillness on, the clip's own roll before it not counted: the hand-over and what he does down there)
				const int Wc = S->ClipIndex(In.WoundClip);
				const double Still = In.WoundStarted + (Wc >= 0 ? 0.9 * static_cast<double>(S->Clips[static_cast<size_t>(Wc)].Long) / 2.0 : 0.0);
				const float Up = C.FramesOf(M)[Ch].R[6];
				if (T >= Still && T < Still + 1.0 / 30.0) std::printf("  wounded %d: chest front %.2f up as his fall ends\n", I + 1, static_cast<double>(Up));
				if (T < Still) continue;
				if (Up < -0.5f) WasDown[static_cast<size_t>(I)] = 1;
				if (Up > 0.5f && WasDown[static_cast<size_t>(I)]) Flipped[static_cast<size_t>(I)] = 1;
				const double Pz = C.FramesOf(M)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head).Z;
				if (T > In.WoundStarted + 2.5) Rose[static_cast<size_t>(I)] = std::max(Rose[static_cast<size_t>(I)], Pz > 0.45 ? 1 : 0);
			}
		}
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(Ix[static_cast<size_t>(I)]);
			std::printf("  wounded %d: down at %.2f in %s, then %s (phase %.3f, rate %.3f); back %.1f\n", I + 1, In.WoundStarted, In.WoundClip.c_str(), In.WoundLoop.c_str(),
				static_cast<double>(In.WoundPhase), static_cast<double>(In.WoundRate), In.Back);
		}
		bool bApart = true;
		for (int A = 0; A < 3; ++A)
		{
			for (int B = A + 1; B < 3; ++B)
			{
				const ManInfo& Ia = C.Info(Ix[static_cast<size_t>(A)]);
				const ManInfo& Ib = C.Info(Ix[static_cast<size_t>(B)]);
				if (Ia.WoundClip == Ib.WoundClip && Ia.WoundLoop == Ib.WoundLoop && std::fabs(Ia.WoundPhase - Ib.WoundPhase) < 0.05f) bApart = false;
			}
		}
		bool bAllDown = true, bNoFlip = true, bStayed = true;
		for (int I = 0; I < 3; ++I)
		{
			bAllDown = bAllDown && C.Info(Ix[static_cast<size_t>(I)]).WoundStarted >= 0.0;
			bNoFlip = bNoFlip && !Flipped[static_cast<size_t>(I)];
			bStayed = bStayed && !Rose[static_cast<size_t>(I)];
		}
		Check(bAllDown, "wounded: the three wd men went down at wd");
		Check(bApart, "wounded: three men within 5 m, three different falls, loops or phases");
		for (int I = 0; I < N; ++I) if (Flipped[static_cast<size_t>(I)]) std::printf("  wounded %d: face down, then on his back\n", I + 1);
		Check(bNoFlip && !Flipped[4], "wounded: never face down onto his back after his fall");
		Check(bStayed && !Rose[4], "wounded: down from his fall's end till back (pelvis under 0.45 m)");
		// up at back: his stance's again, on his feet
		bool bUp = true;
		for (int I = 0; I < 3; ++I)
		{
			const int M = Ix[static_cast<size_t>(I)];
			const double Pz = C.FramesOf(M)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head).Z;
			bUp = bUp && C.LieOf(M) < 0.1f && Pz > 0.8;
		}
		Check(bUp, "wounded: up on his feet after back");
		Check(C.Info(Ix[3]).WoundStarted < 0.0, "wounded: a carried man (cy) is left to the bearers' way");
		Check(C.Info(Ix[4]).WoundStarted >= 0.0 && C.LieOf(Ix[4]) > 0.9f, "wounded: a man with no back still down at the end");
		bool bOffNone = true;
		for (int I = 0; I < N; ++I) bOffNone = bOffNone && Off.Info(Ix[static_cast<size_t>(I)]).WoundStarted < 0.0;
		Check(bOffNone, "wounded: off, no man goes down (as before)");
		Check(bFinite, "wounded: no NaN");
	}

	void Variety(const std::string& Battle, const Played& P, std::map<std::string, int>& StandOnly)
	{
		static const std::set<std::string> Stands = { "die_back_a", "die_back_b", "die_forward_b", "die_head_a", "die_knees_a", "die_knees_b", "die_side_a", "die_side_b",
			"die_side_c", "die_side_d" };
		int Same = 0, Limp = 0, Old = 0, Bullets = 0;
		std::map<std::string, int> Kinds, Clips;
		for (const Row& A : P.Rows)
		{
			if (A.bDeaths && (A.Clip == "settle" || A.Clip == "-")) ++Limp;
			if (A.bDeaths && (A.Clip == "die_back" || A.Clip == "die_forward" || A.Clip == "die_kneel")) ++Old;
			if (A.W != "rifle" && A.W != "mg") continue;
			++Bullets;
			++Kinds[A.Kind];
			++Clips[A.Clip];
			if (Stands.count(A.Clip)) ++StandOnly[A.Clip];
			for (const Row& O : P.Rows)
			{
				if (&O == &A || O.Clip != A.Clip || O.Started < 0.0) continue;
				if (std::fabs(O.Started - A.Started) > 5.0 || std::hypot(O.X - A.X, O.Y - A.Y) > 15.0) continue;
				if (O.Rate == A.Rate && O.Skip == A.Skip) ++Same;
			}
		}
		std::printf("  %s: %d bullet deaths, %zu clips; by kind:", Battle.c_str(), Bullets, Clips.size());
		for (const auto& KV : Kinds) std::printf(" %s %.0f%%", KV.first.c_str(), 100.0 * KV.second / std::max(1, Bullets));
		std::printf("\n");
		Check(Same == 0, "variety " + Battle + Fmt(": same clip within 15 m and 5 s at the same rate and start: %.0f", Same));
		Check(Limp == 0, "variety " + Battle + Fmt(": limp (no clip, a file with falls): %.0f", Limp));
		Check(Old == 0, "variety " + Battle + Fmt(": on the old three-clip table (a file with falls): %.0f", Old));
		Check(!P.bNaN, "variety " + Battle + ": no NaN");
	}
}
#endif

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: land_hits_test <soldiers folder> <armies.json> [battle.json ...]\n");
		return 2;
	}
	const Files Fs = ReadFiles(Argv[1], Argv[2]);
	std::vector<std::string> Battles;
	for (int I = 3; I < Argc; ++I) Battles.push_back(Argv[I]);
	// ---- Off: every part off, the hash of every pose and fall.
	std::vector<std::string> Texts;
	for (const std::string& B : Battles) Texts.push_back(ReadFile(B));
	for (size_t K = 0; K < Battles.size(); ++K)
	{
#ifndef LAND_HITS_BASE
		const Crowd::Hits None;
		const Played P = PlayBattle(Texts[K], Fs, &None);
#else
		const Played P = PlayBattle(Texts[K], Fs, nullptr);
#endif
		int Falls = 0;
		for (const Row& R : P.Rows) Falls += R.Started >= 0.0 ? 1 : 0;
		std::printf("off %s: %d men, %d falls, hash %s%s\n", NameOf(Battles[K]).c_str(), P.Men, Falls, Hex(P.Hash).c_str(), P.bNaN ? " NaN" : "");
	}
#ifdef LAND_HITS_BASE
	return 0;
#else
	Jolts(Fs);
	Struck(Fs);
	Wounded(Fs);
	std::printf("== variety (bVariety alone, then every part on)\n");
	std::map<std::string, int> StandOnly, StandAll;
	for (size_t K = 0; K < Battles.size(); ++K)
	{
		Crowd::Hits V;
		V.bVariety = true;
		Variety(NameOf(Battles[K]), PlayBattle(Texts[K], Fs, &V), StandOnly);
		Crowd::Hits All;
		All.bJolt = All.bVariety = All.bWounded = All.bFlinch = true;
		Variety(NameOf(Battles[K]) + " (all on)", PlayBattle(Texts[K], Fs, &All), StandAll);
	}
	if (!Battles.empty())
	{
		int Reached = 0;
		std::printf("  standing falls reached by bullets:");
		for (const auto& KV : StandOnly) { std::printf(" %s %d", KV.first.c_str(), KV.second); Reached += KV.second; }
		std::printf("\n");
		Check(StandOnly.size() >= 5, Fmt("variety: the standing falls bullets never reached are reached (%.0f of them, %.0f falls)", static_cast<double>(StandOnly.size()), Reached));
	}
	std::printf("%s: %d failed\n", Failed ? "FAIL" : "ok", Failed);
	return Failed ? 1 : 0;
#endif
}
