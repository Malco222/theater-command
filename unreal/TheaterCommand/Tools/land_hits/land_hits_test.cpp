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
		double Speed = 0.0;   // his speed over the third of a second before he fell (metres a second of life, Life 2)
		std::string StartFrom;  // his fall clip's "start"[0]
		bool bAimed = false;
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
		std::vector<double> X(static_cast<size_t>(N), 0.0), Y(static_cast<size_t>(N), 0.0), Speed(static_cast<size_t>(N), 0.0);
		std::vector<char> Got(static_cast<size_t>(N), 0);
		// (each man's place over the last ten frames, for his speed as he fell)
		std::vector<std::pair<double, double>> Ring(static_cast<size_t>(N) * 10);
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
					const auto& Then = Ring[static_cast<size_t>(I) * 10 + static_cast<size_t>((F + 1) % 10)];
					if (F >= 10) Speed[static_cast<size_t>(I)] = std::hypot(X[static_cast<size_t>(I)] - Then.first, Y[static_cast<size_t>(I)] - Then.second) / (9.0 / 30.0) / 2.0;
				}
				else if (!Got[static_cast<size_t>(I)])
				{
					double PX = 0.0, PY = 0.0, PZ = 0.0;
					float Fa = 0.f;
					C.PlaceOf(I, PX, PY, PZ, Fa);
					Ring[static_cast<size_t>(I) * 10 + static_cast<size_t>(F % 10)] = { PX, PY };
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
			R.Speed = Speed[static_cast<size_t>(I)];
			R.X = X[static_cast<size_t>(I)];
			R.Y = Y[static_cast<size_t>(I)];
#ifndef LAND_HITS_BASE
			R.Rate = In.FallRate;
			R.Skip = In.FallSkip;
#endif
			const Soldier* S = C.SoldierOf(I);
			R.bDeaths = S && !S->Deaths.empty();
#ifndef LAND_HITS_BASE
			R.bAimed = In.bFallAimed;
			const int Ci = S ? S->ClipIndex(R.Clip) : -1;
			if (Ci >= 0) R.StartFrom = S->Clips[static_cast<size_t>(Ci)].StartFrom;
#endif
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
		std::string PoseAtWd;   // (the rules' own way: from wd the drill's stance, 'wounded', as the bearers' option writes it)
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
			  << M.Y + std::cos(M.F) * M.Go * (T1 - Hit) << ", " << M.F << "]], \"ch\": [[0, \"" << M.Pose << "\", \"" << M.Cover << "\", 0, 0, 0, 0]";
			if (!M.PoseAtWd.empty() && M.Wd >= 0.0) R << ", [" << M.Wd << ", \"" << M.PoseAtWd << "\", \"none\", 0, 0, 0, 0]";
			R << "]";
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

	/** The angle between two frames' turns (radians). */
	double AngleBetween(const Xform& A, const Xform& B)
	{
		double Tr = 0.0;
		for (int I = 0; I < 3; ++I)
		{
			for (int K = 0; K < 3; ++K) Tr += static_cast<double>(A.R[I * 3 + K]) * B.R[I * 3 + K];
		}
		return std::acos(std::clamp((Tr - 1.0) / 2.0, -1.0, 1.0));
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
		// (d) the modeller's bones: the chest turned more than 4 degrees and the rifle (bone 'weapon', on the root) moved
		// with it, so the turn is free to be that big
		{
			const int Chi = S->BoneIndex("chest");
			const double Turn = AngleBetween(A2.FramesOf(Ix)[Chi], B2.FramesOf(Ix)[Chi]);
			const bool bGunMoved = !SameBits(PA, PB, Gun);
			if (!bLegFall)
			{
				Check(Turn > 4.0 * 0.0174533 && bGunMoved, std::string("jolt ") + Name + Fmt(": (d) the chest turned %.1f degrees, the weapon bone moved with it", Turn / 0.0174533));
			}
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

	/** The head 40 ms behind the chest: at death + 0.02 s and + 0.033 s the head's turn (against the unjolted) is less than
	 * the chest's, and it has caught up by 0.1 s. */
	void HeadLag(const Files& Fs)
	{
		Man1 M;
		M.D = Hit;
		M.bShooter = true;
		M.SX = -150.0;
		const std::string Rec = Little({ M });
		Crowd::Hits On;
		On.bJolt = true;
		const double Ats[] = { 0.02, 0.033, 0.05, 0.1 };
		double Head[4] = {}, Chest[4] = {};
		for (int K = 0; K < 4; ++K)
		{
			Crowd A, B;
			MakeCrowd(A, Fs, Rec, Crowd::Hits());
			MakeCrowd(B, Fs, Rec, On);
			double Clock = 0.0;
			A.Update(0.0);
			B.Update(0.0);
			PlayTo(A, B, Clock, Hit + Ats[K]);
			const int Ix = A.Find(2, 1, 0);
			const Soldier* S = A.SoldierOf(Ix);
			Head[K] = AngleBetween(A.FramesOf(Ix)[S->BoneIndex("head")], B.FramesOf(Ix)[S->BoneIndex("head")]);
			Chest[K] = AngleBetween(A.FramesOf(Ix)[S->BoneIndex("chest")], B.FramesOf(Ix)[S->BoneIndex("chest")]);
			std::printf("  jolt head lag: at death + %.3f s chest %.1f, head %.1f degrees off the unjolted\n", Ats[K], Chest[K] / 0.0174533, Head[K] / 0.0174533);
		}
		Check(Head[0] < Chest[0] && Head[1] < Chest[1], "jolt: (d) the head lags the chest at + 0.02 s and + 0.033 s (turned less)");
	}

	void Jolts(const Files& Fs)
	{
		std::printf("== jolt\n");
		HeadLag(Fs);
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

	/** One man's fall (variety on), his dice another's by the men standing far off before him (Fillers), his lead's kind
	 * where given, with others about him (Near) and the scene's room (Room). */
	std::string FallOf(const Files& Fs, const Man1& M, int Fillers, const std::string& Kind, const std::vector<Man1>& Near, const RoomFn& Room = RoomFn())
	{
		std::vector<Man1> All;
		for (int K = 0; K < Fillers; ++K)
		{
			Man1 F;
			F.X = 500.0 + 10.0 * K;
			All.push_back(F);
		}
		All.push_back(M);
		for (const Man1& N : Near) All.push_back(N);
		Crowd C;
		Crowd::Hits On;
		On.bVariety = true;
		On.bJolt = true;
		if (Room) C.SetRoom(Room);
		MakeCrowd(C, Fs, Little(All), On);
		const int Ix = C.Find(2, Fillers + 1, 0);
		if (!Kind.empty()) C.SetFallKind(Ix, Kind);
		for (int F = 0; F <= static_cast<int>((Hit + 0.1) * 30.0); ++F) C.Update(F / 30.0);
		return C.Info(Ix).FallClip;
	}

	void Rules(const Files& Fs)
	{
		std::printf("== the soldier modeller's rules (bVariety)\n");
		const Soldier& S = *Fs.ByArmy.at("GER");
		auto StartOf = [&S](const std::string& Clip) { const int I = S.ClipIndex(Clip); return I >= 0 ? S.Clips[static_cast<size_t>(I)].StartFrom : std::string("?"); };
		// (a) a man walking (2.4 battle m/s, 1.2 of life) is not aiming: never a fall opening at the shoulder; a man at
		// his aim may take one
		int Walked = 0, WalkShoulder = 0, WalkIdle = 0, AimShoulder = 0;
		std::map<std::string, int> Seen;
		for (int V = 0; V < 14; ++V)
		{
			Man1 W;
			W.Pose = "advance";
			W.Go = 2.4;
			W.D = Hit;
			W.bShooter = true;
			W.SY = 150.0;
			const std::string F = FallOf(Fs, W, V, "", {});
			++Walked;
			++Seen[F];
			WalkShoulder += StartOf(F) == "fire_stand" ? 1 : 0;
			WalkIdle += StartOf(F) == "idle" ? 1 : 0;
			Man1 A = W;
			A.Pose = "fire_stand";
			A.Go = 0.0;
			AimShoulder += StartOf(FallOf(Fs, A, V, "", {})) == "fire_stand" ? 1 : 0;
		}
		std::printf("  (a) a walking man's falls over 14 dice:");
		for (const auto& KV : Seen) std::printf(" %s(%s) %d", KV.first.c_str(), StartOf(KV.first).c_str(), KV.second);
		std::printf("\n");
		Check(WalkShoulder == 0 && WalkIdle * 3 >= Walked, Fmt("rules (a): a walking man never opens at the shoulder (%.0f), a third or more open in idle (%.0f of 14)", WalkShoulder, WalkIdle));
		Check(AimShoulder > 0, Fmt("rules (a): a man at his aim may (%.0f of 14 open at the shoulder)", AimShoulder));
		// (b) never a thrown fall for a bullet, even where his lead asks the kind
		int Thrown = 0;
		for (int V = 0; V < 10; ++V)
		{
			Man1 T;
			T.D = Hit;
			T.bShooter = true;
			T.SY = 150.0;
			const std::string F = FallOf(Fs, T, V, "thrown", {});
			Thrown += F.rfind("die_thrown_", 0) == 0 ? 1 : 0;
			T.W = "mg";
			Thrown += FallOf(Fs, T, V, "thrown", {}).rfind("die_thrown_", 0) == 0 ? 1 : 0;
		}
		Check(Thrown == 0, Fmt("rules (b): no die_thrown_* for a rifle's or an mg's round, the kind asked 20 times (%.0f)", Thrown));
		// (c) room: die_side_b carries him 1.07 m to his left; with a man standing a metre to his left, never; with none
		// there, sometimes. A scene with no room anywhere: no fall that carries him more than half a metre.
		int SideB = 0, SideBFree = 0, Far = 0;
		for (int V = 0; V < 16; ++V)
		{
			Man1 A;
			A.D = Hit;
			A.bShooter = true;
			A.SX = 150.0;    // (from his right: the side falls)
			A.SY = 0.0;
			Man1 Left;
			Left.X = -1.0;
			Left.Pose = "fire_stand";
			SideB += FallOf(Fs, A, V, "side", { Left }) == "die_side_b" ? 1 : 0;
			SideBFree += FallOf(Fs, A, V, "side", {}) == "die_side_b" ? 1 : 0;
			const std::string F = FallOf(Fs, A, V, "", {}, [](double, double, double, double) { return false; });
			const int Ci = S.ClipIndex(F);
			if (Ci >= 0 && S.Clips[static_cast<size_t>(Ci)].bEndOffset && std::hypot(S.Clips[static_cast<size_t>(Ci)].EndOffset.X, S.Clips[static_cast<size_t>(Ci)].EndOffset.Y) > 0.5f) ++Far;
		}
		Check(SideB == 0 && SideBFree > 0, Fmt("rules (c): die_side_b with a man a metre to his left %.0f times, with none %.0f of 16", SideB, SideBFree));
		Check(Far == 0, Fmt("rules (c): the scene with no room: falls carrying him over half a metre %.0f of 16", Far));
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
		// six men wounded together, the first three within 5 m (all get up at back); a carried man (the bearers' way, left
		// as before); a man who never gets up; a man wounded and later killed where he lies; a wounded man who had a round to
		// fire after he went down
		for (int I = 0; I < 6; ++I)
		{
			Man1 M;
			M.X = 2.0 * I;
			M.Y = I < 3 ? 1.0 * I : 3.0 * I;
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
		Man1 Killed;
		Killed.X = 140.0;
		Killed.Wd = 7.0;
		Killed.D = 12.0;
		Men.push_back(Killed);
		// the rules' wounded option's men (wd and the stance 'wounded' from wd): one whose track walks on after wd, one
		// killed (d) while he lies, one wounded after he was killed (wd after d: never down)
		Man1 Rules;
		Rules.X = 180.0;
		Rules.Go = 1.5;
		Rules.Pose = "fire_stand";
		Rules.Wd = 8.0;
		Rules.PoseAtWd = "wounded";
		Men.push_back(Rules);
		Man1 RulesD = Rules;
		RulesD.X = 220.0;
		RulesD.Go = 0.0;
		RulesD.Pose = "fire_stand";
		RulesD.D = 15.0;
		Men.push_back(RulesD);
		Man1 RulesLate = RulesD;
		RulesLate.X = 260.0;
		RulesLate.D = 5.0;
		Men.push_back(RulesLate);
		const int N = static_cast<int>(Men.size());
		const int RulesIx = N - 3, RulesDIx = N - 2, RulesLateIx = N - 1;
		const int KilledIx = N - 4, StaysIx = N - 5, CarriedIx = N - 6;
		std::string Rec = Little(Men);
		// (the first man has rounds booked after his wound: he fires none while down)
		{
			const size_t At = Rec.find("\"rounds\": [");
			const std::string Shots = "{\"t\": 9.0, \"l\": 9.0, \"w\": \"rifle\", \"by\": 2, \"k\": 1, \"on\": 1, \"ok\": 1, \"x\": 0, \"y\": 100, \"r\": \"miss\"},"
				"{\"t\": 15.0, \"l\": 15.0, \"w\": \"rifle\", \"by\": 2, \"k\": 1, \"on\": 1, \"ok\": 1, \"x\": 0, \"y\": 100, \"r\": \"miss\"},";
			Rec.insert(At + 11, Shots);
		}
		Crowd::Hits On;
		On.bJolt = On.bVariety = On.bWounded = true;
		Crowd C, Off;
		MakeCrowd(C, Fs, Rec, On);
		MakeCrowd(Off, Fs, Rec, Crowd::Hits());
		std::vector<int> Ix;
		for (int I = 0; I < N; ++I) Ix.push_back(C.Find(2, I + 1, 0));
		const Soldier* S = C.SoldierOf(Ix[0]);
		const int Ch = S->BoneIndex("chest"), Pe = S->BoneIndex("pelvis"), Hd = S->BoneIndex("head");
		const size_t NB = S->Bones.size();
		std::vector<int> WasDown(static_cast<size_t>(N), 0), Flipped(static_cast<size_t>(N), 0), Rose(static_cast<size_t>(N), 0), UpFlip(static_cast<size_t>(N), 0);
		std::vector<double> MostDown(static_cast<size_t>(N), 0.0), MostUp(static_cast<size_t>(N), 0.0), Stirred(static_cast<size_t>(N), 0.0), Moved(static_cast<size_t>(N), 0.0);
		std::vector<Vec3> Was(static_cast<size_t>(N)), Start(static_cast<size_t>(N));
		std::vector<std::vector<Vec3>> Still(static_cast<size_t>(N));
		std::vector<double> UpChest(static_cast<size_t>(N), 0.0);
		bool bFinite = true;
		int ShotsDown = 0;
		// (one authority: a fall's start once a man at most; down wounded and not getting up, never on his feet; a man
		// down wounded never carried along his track)
		std::vector<double> FirstFall(static_cast<size_t>(N), -1.0), FirstWound(static_cast<size_t>(N), -1.0);
		std::vector<int> FellTwice(static_cast<size_t>(N), 0), StoodDown(static_cast<size_t>(N), 0);
		std::vector<double> Slid(static_cast<size_t>(N), 0.0);
		std::vector<Vec3> DownFrom(static_cast<size_t>(N));
		std::vector<int> bDownFrom(static_cast<size_t>(N), 0);
		double KillJump = 0.0, KillHead = 0.0, KillHeadWas = -1.0;
		auto Points = [&](int M) {
			std::vector<Vec3> P;
			for (size_t K = 0; K < NB; ++K) P.push_back(C.FramesOf(M)[K].Point(S->Bones[K].Head));
			return P;
		};
		for (int F = 0; F <= 40 * 30; ++F)
		{
			const double T = F / 30.0;
			C.Update(T);
			Off.Update(T);
			for (const Event& E : C.Events())
			{
				if (E.Man == Ix[0] && E.Fx == "muzzle_flash" && E.T >= 6.0 && E.T < 30.0) ++ShotsDown;
			}
			for (int I = 0; I < N; ++I)
			{
				const int M = Ix[static_cast<size_t>(I)];
				bFinite = bFinite && FramesFinite(C, M);
				const ManInfo& In = C.Info(M);
				const Vec3 Pel = C.FramesOf(M)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head);
				const double Step = F > 0 ? Len(Minus(Pel, Was[static_cast<size_t>(I)])) : 0.0;
				Was[static_cast<size_t>(I)] = Pel;
				if (I == KilledIx && T > 11.5 && T < 13.5)
				{
					KillJump = std::max(KillJump, Step);
					const double Hz = C.FramesOf(M)[Hd].Point(S->Bones[static_cast<size_t>(Hd)].Head).Z;
					// (his jolt, a third of a second from his death, is no pop: measured outside it)
					if (KillHeadWas >= 0.0 && (T < 12.0 || T > 12.0 + 0.34 / 2.0 + 1.0 / 30.0)) KillHead = std::max(KillHead, std::fabs(Hz - KillHeadWas));
					KillHeadWas = Hz;
				}
				{
					const size_t K = static_cast<size_t>(I);
					if (In.FallStarted >= 0.0)
					{
						if (FirstFall[K] < 0.0) FirstFall[K] = In.FallStarted;
						else if (In.FallStarted != FirstFall[K]) FellTwice[K] = 1;
					}
					if (In.WoundStarted >= 0.0)
					{
						if (FirstWound[K] < 0.0) FirstWound[K] = In.WoundStarted;
						else if (In.WoundStarted != FirstWound[K]) FellTwice[K] = 1;
					}
					const int Wk = S->ClipIndex(In.WoundClip);
					const double WEnd = In.WoundStarted + (Wk >= 0 ? static_cast<double>(S->Clips[static_cast<size_t>(Wk)].Long) / 2.0 : 0.0);
					if (In.WoundStarted >= 0.0 && T > WEnd + 0.5 && (In.Back < 0.0 || T < In.Back))
					{
						if (Pel.Z > 0.45f) StoodDown[K] = 1;
						if (!bDownFrom[K]) { DownFrom[K] = Pel; bDownFrom[K] = 1; }
						Slid[K] = std::max(Slid[K], std::hypot(static_cast<double>(Pel.X - DownFrom[K].X), static_cast<double>(Pel.Y - DownFrom[K].Y)) - static_cast<double>(In.Dragged));
					}
				}
				if (In.WoundStarted < 0.0) continue;
				const int Wc = S->ClipIndex(In.WoundClip);
				const double Ends = In.WoundStarted + (Wc >= 0 ? static_cast<double>(S->Clips[static_cast<size_t>(Wc)].Long) / 2.0 : 0.0);
				const float Up = C.FramesOf(M)[Ch].R[6];
				// down: from his fall's end till back (the hand-over into what he does down there counted)
				if (T >= Ends && (In.Back < 0.0 || T < In.Back) && (I != KilledIx || T < 12.0))
				{
					if (Up < -0.5f) WasDown[static_cast<size_t>(I)] = 1;
					if (Up > 0.5f && WasDown[static_cast<size_t>(I)]) Flipped[static_cast<size_t>(I)] = 1;
					if (T > Ends + 1.0 / 30.0) MostDown[static_cast<size_t>(I)] = std::max(MostDown[static_cast<size_t>(I)], Step);
					if (T > Ends + 0.5 && Pel.Z > 0.45f) Rose[static_cast<size_t>(I)] = 1;
					// (alive: how far any bone moves from where it lay two seconds after his fall's end, over the next 15 s,
					// his pelvis's own move taken out)
					const std::vector<Vec3> P = Points(M);
					if (T >= Ends + 2.0 && Still[static_cast<size_t>(I)].empty()) { Still[static_cast<size_t>(I)] = P; Start[static_cast<size_t>(I)] = Pel; }
					else if (!Still[static_cast<size_t>(I)].empty() && T < Ends + 17.0)
					{
						const Vec3 Drift = Minus(Pel, Start[static_cast<size_t>(I)]);
						Moved[static_cast<size_t>(I)] = std::max(Moved[static_cast<size_t>(I)], Len(Drift));
						for (size_t K = 0; K < NB; ++K)
						{
							if (S->OnWeapon(static_cast<int>(K))) continue;
							Stirred[static_cast<size_t>(I)] = std::max(Stirred[static_cast<size_t>(I)], Len(Minus(Minus(P[K], Still[static_cast<size_t>(I)][K]), Drift)));
						}
					}
				}
				// getting up: from a second before back to five after
				if (In.Back >= 0.0 && T >= In.Back - 1.0 && T < In.Back + 5.0)
				{
					MostUp[static_cast<size_t>(I)] = std::max(MostUp[static_cast<size_t>(I)], Step);
					if (T < In.Back) UpChest[static_cast<size_t>(I)] = Up;
					else if ((UpChest[static_cast<size_t>(I)] < -0.5f && Up > 0.5f) || (UpChest[static_cast<size_t>(I)] > 0.5f && Up < -0.5f)) UpFlip[static_cast<size_t>(I)] = 1;
				}
			}
		}
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(Ix[static_cast<size_t>(I)]);
			std::printf("  wounded %d: down at %.2f in %s, then %s (phase %.3f, rate %.3f, dragged %.1f m); back %.1f; pelvis at most %.1f cm a frame down, %.1f cm getting up; stirring %.0f cm\n",
				I + 1, In.WoundStarted, In.WoundClip.c_str(), In.WoundLoop.c_str(), static_cast<double>(In.WoundPhase), static_cast<double>(In.WoundRate), static_cast<double>(In.Dragged), In.Back,
				100.0 * MostDown[static_cast<size_t>(I)], 100.0 * MostUp[static_cast<size_t>(I)], 100.0 * Stirred[static_cast<size_t>(I)]);
		}
		std::set<std::string> Ways;
		bool bApart = true;
		for (int A = 0; A < 6; ++A)
		{
			const ManInfo& Ia = C.Info(Ix[static_cast<size_t>(A)]);
			Ways.insert(Ia.WoundLoop);
			for (int B = A + 1; B < 3 && A < 3; ++B)
			{
				const ManInfo& Ib = C.Info(Ix[static_cast<size_t>(B)]);
				if (Ia.WoundClip == Ib.WoundClip && Ia.WoundLoop == Ib.WoundLoop && std::fabs(Ia.WoundPhase - Ib.WoundPhase) < 0.05f) bApart = false;
			}
		}
		bool bAllDown = true, bNoFlip = true, bStayed = true, bSmooth = true, bUpSmooth = true, bAlive = true, bNoUpFlip = true, bDragged = false, bFaceDown = true;
		for (int I = 0; I < 6; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			const ManInfo& In = C.Info(Ix[K]);
			bAllDown = bAllDown && In.WoundStarted >= 0.0;
			bNoFlip = bNoFlip && !Flipped[K];
			bStayed = bStayed && !Rose[K];
			bSmooth = bSmooth && MostDown[K] < 0.08;
			bUpSmooth = bUpSmooth && MostUp[K] < 0.15;
			bNoUpFlip = bNoUpFlip && !UpFlip[K];
			bAlive = bAlive && Stirred[K] > 0.15;
			bDragged = bDragged || (In.Dragged > 1.0f && Moved[K] > 1.0);
			bFaceDown = bFaceDown && WasDown[K];
		}
		Check(bAllDown, "wounded: the six wd men went down at wd");
		Check(bApart, "wounded: three men within 5 m, three different falls, loops or phases");
		Check(Ways.size() >= 2, Fmt("wounded: each his own choice down there (%.0f ways among six)", static_cast<double>(Ways.size())));
		Check(bDragged, "wounded: one at least drags himself more than a metre on (and his body goes with him)");
		Check(bAlive && Stirred[static_cast<size_t>(StaysIx)] > 0.15, "wounded: every man down reads as alive (some bone of his moves 15 cm or more besides his drag, over 15 s)");
		Check(bFaceDown, "wounded: a man who gets up at back lies face down (his file has no get_up_back)");
		Check(bNoFlip && !Flipped[static_cast<size_t>(StaysIx)], "wounded: never face down onto his back after his fall");
		Check(bStayed && !Rose[static_cast<size_t>(StaysIx)], "wounded: down from his fall's end till back (pelvis under 0.45 m)");
		Check(bSmooth, "wounded: down there his pelvis never moves 8 cm in a frame (the hand-over into his loop or his drag about his pelvis)");
		Check(bUpSmooth, "wounded: getting up at back his pelvis never moves 15 cm in a frame (no jump)");
		Check(bNoUpFlip, "wounded: getting up, never rolled from face down to his back or back");
		bool bUp = true;
		for (int I = 0; I < 6; ++I)
		{
			const int M = Ix[static_cast<size_t>(I)];
			const double Pz = C.FramesOf(M)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head).Z;
			double X = 0.0, Y = 0.0, Z = 0.0;
			float Fa = 0.f;
			C.PlaceOf(M, X, Y, Z, Fa);
			const Vec3 Pel = C.FramesOf(M)[Pe].Point(S->Bones[static_cast<size_t>(Pe)].Head);
			bUp = bUp && C.LieOf(M) < 0.1f && Pz > 0.8 && std::hypot(Pel.X - X, Pel.Y - Y) < 0.4;
		}
		Check(bUp, "wounded: up on his feet after back, back on his own place");
		Check(C.Info(Ix[static_cast<size_t>(CarriedIx)]).WoundStarted < 0.0, "wounded: a carried man (cy) is left to the bearers' way");
		Check(C.Info(Ix[static_cast<size_t>(StaysIx)]).WoundStarted >= 0.0 && C.LieOf(Ix[static_cast<size_t>(StaysIx)]) > 0.9f, "wounded: a man with no back still down at the end");
		const ManInfo& Ki = C.Info(Ix[static_cast<size_t>(KilledIx)]);
		std::printf("  wounded then killed: %s, pelvis at most %.1f cm a frame, head %.1f cm a frame about his death\n", Ki.FallClip.c_str(), 100.0 * KillJump, 100.0 * KillHead);
		Check(Ki.FallClip == "still" && KillJump < 0.05 && KillHead < 0.05, "wounded: killed where he lies, he goes still there (no new fall, no jump)");
		Check(ShotsDown == 0, Fmt("wounded: no shot from a man down wounded (%.0f)", ShotsDown));
		bool bOffNone = true;
		for (int I = 0; I < N; ++I) bOffNone = bOffNone && Off.Info(Ix[static_cast<size_t>(I)]).WoundStarted < 0.0;
		Check(bOffNone, "wounded: off, no man goes down (as before)");
		Check(bFinite, "wounded: no NaN");
		// The rules' wounded option with these on: the build decides his fall (StartWound) and what he does down there;
		// the rules' stance 'wounded' waits (as any stance while he is down); off, the rules' stance plays as before.
		const ManInfo& Ru = C.Info(Ix[static_cast<size_t>(RulesIx)]);
		const ManInfo& Rd = C.Info(Ix[static_cast<size_t>(RulesDIx)]);
		const ManInfo& Rl = C.Info(Ix[static_cast<size_t>(RulesLateIx)]);
		bool bOnce = true, bNeverStood = true, bNoSlide = true;
		for (int I = 0; I < N; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			bOnce = bOnce && !FellTwice[K] && !(FirstFall[K] >= 0.0 && FirstWound[K] >= 0.0 && Men[K].D < Men[K].Wd);
			bNeverStood = bNeverStood && !StoodDown[K];
			bNoSlide = bNoSlide && Slid[K] < 0.5;
		}
		std::printf("  the rules' wounded: walking on %s at %.2f (%s), killed down there %s / %s, wounded after death %s; slid at most %.2f m past his drag\n",
			Ru.WoundClip.c_str(), Ru.WoundStarted, Ru.WoundLoop.c_str(), Rd.WoundClip.c_str(), Rd.FallClip.c_str(), Rl.WoundStarted < 0.0 ? "never down" : "DOWN",
			*std::max_element(Slid.begin(), Slid.end()));
		Check(Ru.WoundStarted >= 8.0 && Ru.WoundStarted < 8.0 + 1.0 / 15.0 && C.LieOf(Ix[static_cast<size_t>(RulesIx)]) > 0.9f, "wounded, the rules' option: wd and the stance 'wounded' together, he goes down at wd once, by the hits' fall, and stays down");
		Check(Slid[static_cast<size_t>(RulesIx)] < 0.5, Fmt("wounded, the rules' option: his track walks on after wd, he does not (%.2f m)", Slid[static_cast<size_t>(RulesIx)]));
		Check(Rd.WoundStarted >= 0.0 && Rd.FallClip == "still", "wounded, the rules' option: d while he lies, he stays down and goes still (no second fall)");
		Check(Rl.WoundStarted < 0.0, "wounded, the rules' option: wd after his death, never a wound's fall");
		Check(bOnce, "wounded: no man falls twice (one fall's start, or one wound's, each man)");
		Check(bNeverStood, "wounded: no man down wounded stands (pelvis under 0.45 m from his fall's end till back)");
		Check(bNoSlide, "wounded: no man down wounded slides with his track (pelvis within half a metre of where he lay, past his own drag)");
		const Playing OffR = Off.PlayingOf(Ix[static_cast<size_t>(RulesIx)]);
		Check(Off.Info(Ix[static_cast<size_t>(RulesIx)]).WoundStarted < 0.0 && (OffR.Clip == "wounded_waiting" || OffR.Clip == "carried_wounded" || OffR.Clip == "fire_prone"),
			"wounded, the rules' option, hits off: the rules' stance 'wounded' plays as before (" + OffR.Clip + ")");
	}

	/** A live battle: a wound sent in a later delta (wd, back) as the drill will send it, read by Extend. */
	void LiveWound(const Files& Fs)
	{
		Man1 M;
		const std::string Whole = Little({ M });
		// (the header: the whole battle without its men; the man in a first delta, his wound in a second)
		std::string Header = Whole;
		const size_t A = Header.find("\"men\": [");
		const size_t B = Header.find("], \"rounds\"");
		const std::string Row = Header.substr(A + 8, B - A - 8);
		Header.erase(A + 8, B - A - 8);
		Crowd C;
		Crowd::Hits On;
		On.bWounded = true;
		MakeCrowd(C, Fs, Header, On);
		std::string Err;
		std::string Mine = Row;
		Mine.insert(1, "\"n\": 0, ");
		const bool bOk1 = C.Extend("{\"t\": 1.0, \"men\": [" + Mine + "]}", Err);
		for (int F = 0; F <= 90; ++F) C.Update(F / 30.0);
		const bool bOk2 = C.Extend("{\"t\": 3.0, \"men\": [{\"n\": 0, \"wd\": 4.0, \"back\": 20.0}]}", Err);
		for (int F = 91; F <= 10 * 30; ++F) C.Update(F / 30.0);
		const int Ix = C.Find(2, 1, 0);
		const bool bDown = Ix >= 0 && C.Info(Ix).WoundStarted >= 0.0 && C.LieOf(Ix) > 0.9f;
		std::printf("  live: deltas %s, %s (%s); the man %s\n", bOk1 ? "read" : "refused", bOk2 ? "read" : "refused", Err.c_str(), bDown ? "down wounded" : "not down");
		Check(bOk1 && bOk2 && bDown, "wounded: a live battle's wd in a later delta (Extend) puts him down");
	}

	/** A flinch clip by name (hit_flinch_f, the soldiers thread's, here a stand-in made of fire_stand with its additive base):
	 * laid over his pose instead of the code's jolt, pelvis to the bit where the clip leaves it, no NaN. */
	void FlinchByName(const Files& Fs, const std::string& Folder)
	{
		std::string Text = ReadFile(Folder + "/soldier_GER.json");
		const size_t Key = Text.find("\"fire_stand\":{");
		const size_t Open = Key == std::string::npos ? Key : Text.find('{', Key);
		size_t Close = Open;
		for (int Depth = 0; Close != std::string::npos && Close < Text.size(); ++Close)
		{
			if (Text[Close] == '{') ++Depth;
			else if (Text[Close] == '}' && --Depth == 0) break;
		}
		const size_t Clips = Text.find("\"clips\":{");
		if (Open == std::string::npos || Clips == std::string::npos || Close >= Text.size()) { Check(false, "flinch: the test's stand-in clip made"); return; }
		std::string Body = Text.substr(Open, Close - Open + 1);
		Body.insert(1, "\"additive_base\": [\"fire_stand\", 0.0], ");
		Text.insert(Clips + 9, "\"hit_flinch_f\": " + Body + ", ");
		auto Own = std::make_shared<Soldier>();
		std::string Err;
		if (!Own->Load(Text, Err)) { Check(false, "flinch: the stand-in file loads: " + Err); return; }
		Files F2 = Fs;
		F2.ByArmy["GER"] = Own;
		Man1 M;
		M.D = Hit;
		M.bShooter = true;
		M.SY = 150.0;
		const std::string Rec = Little({ M });
		Crowd A, B;
		Crowd::Hits On;
		On.bJolt = true;
		MakeCrowd(A, Fs, Rec, On);
		MakeCrowd(B, F2, Rec, On);
		double Clock = 0.0;
		A.Update(0.0);
		B.Update(0.0);
		PlayTo(A, B, Clock, Hit + 0.3);
		const int Ix = A.Find(2, 1, 0);
		Pose PA, PB;
		A.PoseOf(Ix, PA);
		B.PoseOf(Ix, PB);
		bool bDiffer = false;
		for (int K = 0; K < MaxBones; ++K) bDiffer = bDiffer || !SameBits(PA, PB, K);
		Check(bDiffer && FramesFinite(B, Ix), "flinch: a file with hit_flinch_f plays it over his pose in place of the code's jolt (no NaN)");
	}

	void Variety(const std::string& Battle, const Played& P, std::map<std::string, int>& StandOnly)
	{
		static const std::set<std::string> Stands = { "die_back_a", "die_back_b", "die_forward_b", "die_head_a", "die_knees_a", "die_knees_b", "die_side_a", "die_side_b",
			"die_side_c", "die_side_d" };
		int Same = 0, Limp = 0, Old = 0, Bullets = 0, Thrown = 0, Shoulder = 0, Stand = 0, Idle = 0, Run = 0;
		std::map<std::string, int> Kinds, Clips;
		for (const Row& A : P.Rows)
		{
			if (A.bDeaths && (A.Clip == "settle" || A.Clip == "-")) ++Limp;
			if (A.bDeaths && (A.Clip == "die_back" || A.Clip == "die_forward" || A.Clip == "die_kneel")) ++Old;
			if (A.W != "rifle" && A.W != "mg") continue;
			++Bullets;
			++Kinds[A.Kind];
			Thrown += A.Clip.rfind("die_thrown_", 0) == 0 ? 1 : 0;
			Run += A.Stance == "run" ? 1 : 0;
			if (A.Stance == "stand" && !A.bAimed)
			{
				++Stand;
				Idle += A.StartFrom == "idle" ? 1 : 0;
				Shoulder += A.StartFrom == "fire_stand" ? 1 : 0;
			}
			++Clips[A.Clip];
			if (Stands.count(A.Clip)) ++StandOnly[A.Clip];
			for (const Row& O : P.Rows)
			{
				if (&O == &A || O.Clip != A.Clip || O.Started < 0.0) continue;
				if (std::fabs(O.Started - A.Started) > 8.0 || std::hypot(O.X - A.X, O.Y - A.Y) > 15.0) continue;
				if (O.Rate == A.Rate && O.Skip == A.Skip) ++Same;
			}
		}
		std::printf("  %s: %d bullet deaths, %zu clips; by kind:", Battle.c_str(), Bullets, Clips.size());
		for (const auto& KV : Kinds) std::printf(" %s %.0f%%", KV.first.c_str(), 100.0 * KV.second / std::max(1, Bullets));
		std::printf("\n");
		std::printf("  %s: run falls %.0f%% of bullet deaths; standing falls of men not aiming %d, %d of them opening in idle\n", Battle.c_str(), 100.0 * Run / std::max(1, Bullets), Stand, Idle);
		Check(Same == 0, "variety " + Battle + Fmt(": same clip within 15 m and 8 s at the same rate and start: %.0f", Same));
		Check(Thrown == 0, "variety " + Battle + Fmt(": (b) a thrown fall (die_thrown_*) for a bullet: %.0f", Thrown));
		Check(Shoulder == 0, "variety " + Battle + Fmt(": (a) a standing fall opening at the shoulder (fire_stand) for a man not aiming: %.0f", Shoulder));
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
#ifndef LAND_HITS_BASE
		// (how fast the men killed by a bullet in a gait were going, metres a second of life: the walking line, WalkBelow)
		int Gait = 0, Bands[6] = {};
		for (const Row& R : P.Rows)
		{
			if (R.Stance != "run" || (R.W != "rifle" && R.W != "mg")) continue;
			++Gait;
			++Bands[R.Speed < 1.0 ? 0 : R.Speed < 1.6 ? 1 : R.Speed < 2.2 ? 2 : R.Speed < 2.8 ? 3 : R.Speed < 3.5 ? 4 : 5];
		}
		std::printf("  gait speeds of %d bullet deaths in a gait (life m/s): <1 %d, 1-1.6 %d, 1.6-2.2 %d, 2.2-2.8 %d, 2.8-3.5 %d, >3.5 %d\n", Gait, Bands[0], Bands[1], Bands[2], Bands[3], Bands[4], Bands[5]);
#endif
	}
#ifdef LAND_HITS_BASE
	return 0;
#else
	Jolts(Fs);
	FlinchByName(Fs, Argv[1]);
	Rules(Fs);
	Struck(Fs);
	Wounded(Fs);
	LiveWound(Fs);
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
		Check(StandOnly.size() >= 3, Fmt("variety: the standing falls bullets never reached are reached (%.0f of them, %.0f falls)", static_cast<double>(StandOnly.size()), Reached));
	}
	std::printf("%s: %d failed\n", Failed ? "FAIL" : "ok", Failed);
	return Failed ? 1 : 0;
#endif
}
