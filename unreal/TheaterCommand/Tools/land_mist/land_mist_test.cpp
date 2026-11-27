// The blood mist's cloud test (-TheaterLandHitMist; Effects R2): builds Battle/LandMen and Effects/TheaterFxCore (the
// game's own code, no engine) and checks:
//   event    a little battle of its own: men killed by a rifle, an mg, a sub-machine gun and a pistol from known places,
//            a rifle with no firer known, a mortar and a tank: one strike for each bullet's fall and none for the
//            others; its time the round's arrival (Hits::bArrive: d + range / 760 m/s of life, at most 0.5 s of life,
//            at Life 2), else the fall's (d); his fall starting then; the place within his body's box; the way from
//            the firer through him; a seek back and forward again gives the same list.
//   battles  each recording given, every hit part on and bArrive: one strike for each bullet death and none for any
//            other, its time his fall's and d plus the round's flight, his place, the way from the firer; the falls'
//            rest places with bArrive and without; the kit fed the battle's strikes at their times (life clock):
//            strikes, mists drawn, the most pieces alive at once, the densest frame's Draw with them and without.
//   kit      Battle::Struck: no row (this tree's recipe less men.struck.on, and 4060f0923's recipe): nothing laid or
//            drawn; with it, ten pieces at the most for every weapon and part, the share passing through by the odds,
//            no lamp, everything gone a second on; the same dice twice the same bytes; a shell's and a burst's frames
//            the same to the byte whether strikes were laid among them or not, once the mist has gone; seen end-on, the
//            mist's pieces spread across the path with side_m, r1_share_k and first_alpha_k (rms across, at least 0.04 m
//            and 2.5 times as without them), and those rows left out or written 0, 0, 1 the same bytes.
// Usage: land_mist_test <soldiers folder> <armies.json> <recipe.txt> <old recipe.txt> [battle.json ...]
// (recipe.txt: tools/battle-fx/preview/flatten.py's lines, path TAB value)
#include "Battle/LandMen.h"
#include "Effects/TheaterFxCore.h"
#include "land_json.h"

#include <algorithm>
#include <chrono>
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
	int Failed = 0;
	void Check(bool bOk, const std::string& What)
	{
		std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str());
		Failed += bOk ? 0 : 1;
	}
	std::string Fmt(const char* F, double A, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Buf[320];
		std::snprintf(Buf, sizeof(Buf), F, A, B, C, D);
		return Buf;
	}
	std::string ReadFile(const std::string& P)
	{
		std::ifstream F(P, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	// ---- the recipe, as the preview reads it
	class FlatRecipe : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;
		bool Find(const char* Path, double& Out) const override
		{
			const auto It = Numbers.find(Path);
			if (It == Numbers.end()) return false;
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			const auto It = Words.find(Path);
			if (It == Words.end()) return false;
			Out = It->second;
			return true;
		}
	};
	FlatRecipe ReadRecipe(const std::string& Path)
	{
		FlatRecipe R;
		std::ifstream F(Path);
		std::string Line;
		while (std::getline(F, Line))
		{
			const size_t Tab = Line.find('\t');
			if (Tab == std::string::npos) continue;
			const std::string Key = Line.substr(0, Tab), Val = Line.substr(Tab + 1);
			if (!Val.empty() && Val[0] == '=') R.Words[Key] = Val.substr(1);
			else R.Numbers[Key] = std::stod(Val);
		}
		return R;
	}
	double FlatGround(void*, double, double) { return 0.0; }

	// ---- the soldier files
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
			if (!S->Load(ReadFile(Folder + "/" + N), Err)) continue;
			Fs.ByArmy[N.substr(8, N.size() - 13)] = S;
		}
		Fs.Armies = ReadFile(ArmiesPath);
		return Fs;
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

	bool SameStrike(const Crowd::Strike& A, const Crowd::Strike& B, double Tol)
	{
		auto Near = [Tol](const Vec3& P, const Vec3& Q) { return std::fabs(P.X - Q.X) <= Tol && std::fabs(P.Y - Q.Y) <= Tol && std::fabs(P.Z - Q.Z) <= Tol; };
		return A.Man == B.Man && A.T == B.T && A.Gun == B.Gun && A.bKilled == B.bKilled && A.Part == B.Part && Near(A.At, B.At) && Near(A.Dir, B.Dir);
	}

	// ---- a little battle: GER men (block 2) killed, USA firers (block 1) where the case says
	struct Case
	{
		const char* Name;
		double SX, SY;      // the firer's place against the man's (none: no firer)
		bool bFirer;
		std::string W;
		bool bBullet;
		bool bWound = false; // (struck and not killed: the rules' wd, no d and no round)
	};
	const double Hit = 5.0;

	std::string Little(const std::vector<Case>& Cs)
	{
		std::ostringstream R, Sh, Rounds;
		R.precision(10);
		Sh.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": 41, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}], \"men\": [";
		Rounds << "\"rounds\": [";
		for (size_t I = 0; I < Cs.size(); ++I)
		{
			const Case& C = Cs[I];
			const int K = static_cast<int>(I) + 1;
			const double X = 60.0 * static_cast<double>(I), Y = 0.0;
			R << (I ? "," : "") << "{\"b\": 2, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[0, " << X << ", " << Y << ", 0], [40, " << X << ", " << Y
			  << ", 0]], \"ch\": [[0, \"fire_stand\", \"none\", 0, 0, 0, 0]], " << (C.bWound ? "\"wd\": " : "\"d\": ") << Hit << "}";
			if (C.bFirer)
			{
				const double Face = std::atan2(-C.SX, -C.SY);
				Sh << ", {\"b\": 1, \"k\": " << K << ", \"r\": \"rifle\", \"at\": [[0, " << X + C.SX << ", " << Y + C.SY << ", " << Face << "], [40, " << X + C.SX << ", "
				   << Y + C.SY << ", " << Face << "]], \"ch\": [[0, \"fire_prone\", \"none\", 0, 0, 0, 0]]}";
			}
			if (C.bWound) continue;
			Rounds << (Rounds.str().size() > 11 ? "," : "") << "{\"t\": " << Hit << ", \"l\": " << Hit << ", \"w\": \"" << C.W << "\", ";
			if (C.bFirer) Rounds << "\"by\": 1, \"k\": " << K << ", ";
			Rounds << "\"on\": 2, \"ok\": " << K << ", \"x\": " << X << ", \"y\": " << Y << ", \"r\": \"kill\"}";
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

	/** Played 30 frames a second from From to To, the strikes asked each frame as the scene asks them. */
	void Collect(Crowd& C, double From, double To, std::vector<Crowd::Strike>& Out, double& Asked)
	{
		for (int F = static_cast<int>(std::ceil(From * 30.0 - 1e-9)); F / 30.0 <= To + 1e-9; ++F)
		{
			const double T = F / 30.0;
			C.Update(T);
			if (Asked < 0.0 || T < Asked) Asked = T;
			C.StrikesIn(Asked, T, Out);
			Asked = T;
		}
	}

	Crowd::Hits AllOn(bool bArrive)
	{
		Crowd::Hits H;
		H.bJolt = H.bVariety = H.bWounded = H.bFlinch = true;
		H.bArrive = bArrive;
		return H;
	}

	void Events(const Files& Fs)
	{
		std::printf("== event\n");
		const std::vector<Case> Cs = {
			{ "rifle, 15 m ahead", 0.0, 15.0, true, "rifle", true },
			{ "rifle, 300 m on his left", -300.0, 0.0, true, "rifle", true },
			{ "mg, 120 m ahead and right", 70.0, 97.5, true, "mg", true },
			{ "rifle, 600 m behind (capped)", 0.0, -600.0, true, "rifle", true },
			{ "smg, 20 m", 12.0, 16.0, true, "smg", true },
			{ "pistol, 8 m", -8.0, 0.0, true, "pistol", true },
			{ "rifle, no firer known", 0.0, 0.0, false, "rifle", true },
			{ "mortar", 0.0, 200.0, true, "mortar", false },
			{ "tank", 0.0, 300.0, true, "tank", false },
			{ "wounded", 0.0, 0.0, false, "", false, true },
		};
		const std::string Rec = Little(Cs);
		for (int Mode = 0; Mode < 3; ++Mode)
		{
			// (0: every hit part on with bArrive, as -TheaterLandHits now; 1: the same without bArrive, as 4060f0923's; 2: every part off)
			const Crowd::Hits H = Mode == 0 ? AllOn(true) : Mode == 1 ? AllOn(false) : Crowd::Hits();
			const char* ModeName = Mode == 0 ? "hits on" : Mode == 1 ? "hits on less bArrive" : "hits off";
			Crowd C;
			MakeCrowd(C, Fs, Rec, H);
			std::vector<Crowd::Strike> Got;
			double Asked = -1.0;
			Collect(C, 0.0, 10.0, Got, Asked);
			std::map<int, int> Count;
			for (const Crowd::Strike& S : Got) ++Count[S.Man];
			for (size_t I = 0; I < Cs.size(); ++I)
			{
				const Case& Cc = Cs[I];
				const int Man = C.Find(2, static_cast<int>(I) + 1, 0);
				const ManInfo& In = C.Info(Man);
				const int N = Count[Man];
				const Crowd::Strike* S = nullptr;
				for (const Crowd::Strike& Sk : Got) if (Sk.Man == Man) S = &Sk;
				if (Cc.bWound)
				{
					// (a wound: one strike as he goes down, where hits show wounds (bWounded), "other", not killed; none off)
					const bool bWant = Mode != 2;
					const bool bOk = bWant ? N == 1 && S && !S->bKilled && S->Gun == "other" && S->T == In.WoundStarted && S->T == Hit : N == 0;
					Check(bOk, std::string("event (") + ModeName + ") " + Cc.Name + Fmt(": %.0f strike (%.0f), as he goes down at %.3f, not killed, no round known", bWant ? 1.0 : 0.0, N, In.WoundStarted));
					continue;
				}
				if (!Cc.bBullet)
				{
					Check(N == 0, std::string("event (") + ModeName + ") " + Cc.Name + Fmt(": no strike (%.0f), fell at %.3f", N, In.FallStarted));
					continue;
				}
				const double Range = Cc.bFirer ? std::hypot(Cc.SX, Cc.SY) : 0.0;
				const double Want = Mode == 0 && Cc.bFirer ? Hit + std::min(std::min(Range / 760.0, 0.5) / 2.0, 0.5) : Hit;
				if (!S || N != 1) { Check(false, std::string("event (") + ModeName + ") " + Cc.Name + Fmt(": one strike (%.0f)", N)); continue; }
				double X = 0.0, Y = 0.0, Z = 0.0;
				float Fa = 0.f;
				C.PlaceOf(Man, X, Y, Z, Fa);
				const double Flat = std::hypot(S->At.X - X, S->At.Y - Y), Up = S->At.Z - Z;
				// (the way: from the firer, a metre over the ground there, through where it struck)
				double Away = 1.0;
				if (Cc.bFirer)
				{
					const double DX = S->At.X - (X + Cc.SX), DY = S->At.Y - (Y + Cc.SY), DZ = S->At.Z - 1.0, DL = std::sqrt(DX * DX + DY * DY + DZ * DZ);
					Away = (S->Dir.X * DX + S->Dir.Y * DY + S->Dir.Z * DZ) / DL;
				}
				else Away = -S->Dir.Y; // (no firer: from his front, he faces +y)
				const double DirLen = std::sqrt(S->Dir.X * S->Dir.X + S->Dir.Y * S->Dir.Y + S->Dir.Z * S->Dir.Z);
				std::printf("  event (%s) %s: at %.4f (fell %.4f, d %.1f) %s %s, %.2f m up, %.2f m off, way %.3f,%.3f,%.3f, range %.1f\n", ModeName, Cc.Name, S->T, In.FallStarted, In.Dead,
					S->Gun.c_str(), S->Part.c_str(), Up, Flat, static_cast<double>(S->Dir.X), static_cast<double>(S->Dir.Y), static_cast<double>(S->Dir.Z), static_cast<double>(S->Range));
				Check(std::fabs(S->T - Want) < 1e-9 && S->T == In.FallStarted && S->Gun == Cc.W && S->bKilled,
					std::string("event (") + ModeName + ") " + Cc.Name + Fmt(": struck at %.4f, the round's arrival %.4f, his fall's start %.4f", S->T, Want, In.FallStarted));
				Check(Flat < 0.6 && Up > 0.0 && Up < 2.0 && std::fabs(DirLen - 1.0) < 1e-4 && Away > 0.999,
					std::string("event (") + ModeName + ") " + Cc.Name + Fmt(": in his body's box (%.2f m off, %.2f m up), the way from the firer through it (%.4f)", Flat, Up, Away));
			}
		}
		// A seek: played to 7 s, back to 2 s, on to 10 s: the strikes after the seek are those of a straight play.
		{
			Crowd A, B;
			MakeCrowd(A, Fs, Rec, AllOn(true));
			MakeCrowd(B, Fs, Rec, AllOn(true));
			std::vector<Crowd::Strike> Straight, Before, After;
			double Asked = -1.0, AskedB = -1.0;
			Collect(A, 0.0, 10.0, Straight, Asked);
			Collect(B, 0.0, 7.0, Before, AskedB);
			Collect(B, 2.0, 10.0, After, AskedB);
			bool bSame = Straight.size() == After.size() && Straight.size() == Before.size();
			for (size_t K = 0; bSame && K < Straight.size(); ++K) bSame = SameStrike(Straight[K], After[K], 0.02) && SameStrike(Straight[K], Before[K], 0.0);
			Check(bSame, Fmt("event: a seek back from 7 s to 2 s and on again gives the same %.0f strikes (%.0f after it)", static_cast<double>(Straight.size()), static_cast<double>(After.size())));
			// (and a jump forward past them all, asked at the far end at once: the pure function still lists them)
			std::vector<Crowd::Strike> Whole;
			A.StrikesIn(0.0, 10.0, Whole);
			// (listed by man, where the frames list them by time)
			std::vector<Crowd::Strike> ByMan = Straight;
			std::stable_sort(ByMan.begin(), ByMan.end(), [](const Crowd::Strike& P, const Crowd::Strike& Q) { return P.Man < Q.Man; });
			bool bWhole = Whole.size() == ByMan.size();
			for (size_t K = 0; bWhole && K < Whole.size(); ++K) bWhole = Whole[K].Man == ByMan[K].Man && Whole[K].T == ByMan[K].T && Whole[K].Gun == ByMan[K].Gun;
			Check(bWhole, "event: asked over the whole battle at its end, the same men at the same times");
			std::vector<Crowd::Strike> None;
			A.StrikesIn(10.0, 2.0, None);
			A.StrikesIn(10.0, 10.0, None);
			Check(None.empty(), "event: nothing asked backward or over no time");
		}
	}

	// ---- the battles
	struct Rows
	{
		int Men = 0, Bullet = 0, Other = 0, Strikes = 0, Twice = 0, Missing = 0, Wrong = 0, BadT = 0, BadBox = 0, BadWay = 0, NoFirer = 0;
		double MostFlight = 0.0, MostDev = 0.0;
		std::vector<Crowd::Strike> All;
		std::vector<double> PX, PY, Started, Rested;
		std::vector<std::string> Clip;
		std::vector<double> KX, KY;
	};

	Rows PlayBattle(const std::string& RecText, const Files& Fs, const Crowd::Hits& H)
	{
		Rows Out;
		LandJson::Value Rec;
		if (!LandJson::Parse(RecText, Rec)) { std::printf("FAIL: cannot read the recording\n"); std::exit(1); }
		Crowd C;
		C.SetLife(2.0);
		for (const auto& KV : Fs.ByArmy) C.AddSoldier(KV.first, KV.second);
		std::string Err;
		if (!C.ReadArmies(Fs.Armies, Err)) { std::printf("FAIL: armies: %s\n", Err.c_str()); std::exit(1); }
		C.SetHits(H);
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
		// (each man's kill as the land_hits test reads it: the round nearest his d, and its firer's man)
		struct K { double T; std::string W; int Firer; double X = 0.0, Y = 0.0; };
		std::vector<std::vector<K>> Kills(static_cast<size_t>(N));
		const auto& Rs = Rec["rounds"];
		for (size_t I = 0; I < Rs.Count(); ++I)
		{
			const auto& R = Rs.At(I);
			if (R["r"].Str("") != "kill" || !R.Has("ok")) continue;
			const int M = C.Find(R["on"].Int(-1), R["ok"].Int(-1), std::max(0, R["oj"].Int(0)));
			const int Fm = R.Has("by") && R.Has("k") ? C.Find(R["by"].Int(-1), R["k"].Int(-1), std::max(0, R["j"].Int(0))) : -1;
			if (M >= 0) Kills[static_cast<size_t>(M)].push_back({ R["t"].Num(), R["w"].Str(""), Fm, R["x"].Num(0.0), R["y"].Num(0.0) });
		}
		std::vector<const K*> Best(static_cast<size_t>(N), nullptr);
		double Last = 0.0;
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(I);
			Last = std::max(Last, In.Dead);
			for (const K& Kk : Kills[static_cast<size_t>(I)])
			{
				if (!Best[static_cast<size_t>(I)] || std::fabs(Kk.T - In.Dead) < std::fabs(Best[static_cast<size_t>(I)]->T - In.Dead)) Best[static_cast<size_t>(I)] = &Kk;
			}
		}
		const double End = Last + 4.0;
		// (each firer's place at the frame his round was fired, for the way)
		std::vector<double> FirerX(static_cast<size_t>(N), 0.0), FirerY(static_cast<size_t>(N), 0.0);
		std::vector<char> FirerKnown(static_cast<size_t>(N), 0);
		std::vector<int> Struck(static_cast<size_t>(N), 0);
		double Asked = -1.0;
		std::vector<Crowd::Strike> Now;
		for (int F = 0; F / 30.0 <= End; ++F)
		{
			const double T = F / 30.0;
			C.Update(T);
			for (int I = 0; I < N; ++I)
			{
				const K* Kk = Best[static_cast<size_t>(I)];
				if (Kk && Kk->Firer >= 0 && !FirerKnown[static_cast<size_t>(I)] && T + 1e-9 >= Kk->T)
				{
					double Z = 0.0;
					float Fa = 0.f;
					C.PlaceOf(Kk->Firer, FirerX[static_cast<size_t>(I)], FirerY[static_cast<size_t>(I)], Z, Fa);
					FirerKnown[static_cast<size_t>(I)] = 1;
				}
			}
			Now.clear();
			if (Asked < 0.0) Asked = T;
			C.StrikesIn(Asked, T, Now);
			Asked = T;
			for (const Crowd::Strike& S : Now)
			{
				const size_t I = static_cast<size_t>(S.Man);
				++Out.Strikes;
				if (++Struck[I] > 1) ++Out.Twice;
				const ManInfo& In = C.Info(S.Man);
				const K* Kk = Best[I];
				if (!Kk || (Kk->W != "rifle" && Kk->W != "mg" && Kk->W != "smg" && Kk->W != "pistol")) { ++Out.Wrong; continue; }
				const double Flight = S.T - In.Dead;
				Out.MostFlight = std::max(Out.MostFlight, Flight);
				const double Want = S.Range > 0.f ? std::min(std::min(static_cast<double>(S.Range) / 760.0, 0.5) / 2.0, 0.5) : 0.0;
				if (H.bArrive) Out.MostDev = std::max(Out.MostDev, std::fabs(Flight - Want));
				if (S.T != In.FallStarted || Flight < 0.0 || Flight > 0.25 + 1e-9 || (!H.bArrive && Flight != 0.0) || (H.bArrive && std::fabs(Flight - Want) > 0.005)) ++Out.BadT;
				double X = 0.0, Y = 0.0, Z = 0.0;
				float Fa = 0.f;
				C.PlaceOf(S.Man, X, Y, Z, Fa);
				const double Flat = std::hypot(S.At.X - X, S.At.Y - Y), Up = S.At.Z - Z;
				if (!(Flat < 0.8 && Up > -0.05 && Up < 2.0)) ++Out.BadBox;
				if (FirerKnown[I])
				{
					const double DX = S.At.X - FirerX[I], DY = S.At.Y - FirerY[I], DL = std::hypot(DX, DY), HL = std::hypot(S.Dir.X, S.Dir.Y);
					if (DL > 5.0 && HL > 1e-6 && (S.Dir.X * DX + S.Dir.Y * DY) / (DL * HL) < 0.9) ++Out.BadWay;
				}
				else ++Out.NoFirer;
				Out.All.push_back(S);
			}
		}
		Out.PX.assign(static_cast<size_t>(N), 0.0);
		Out.PY = Out.PX;
		for (int I = 0; I < N; ++I)
		{
			const ManInfo& In = C.Info(I);
			Out.Started.push_back(In.FallStarted);
			Out.Rested.push_back(In.Rested);
			Out.Clip.push_back(In.FallClip + "/" + In.FallStance);
			Out.KX.push_back(Best[static_cast<size_t>(I)] ? Best[static_cast<size_t>(I)]->X : 0.0);
			Out.KY.push_back(Best[static_cast<size_t>(I)] ? Best[static_cast<size_t>(I)]->Y : 0.0);
			double Z = 0.0;
			float Fa = 0.f;
			C.PlaceOf(I, Out.PX[static_cast<size_t>(I)], Out.PY[static_cast<size_t>(I)], Z, Fa);
			if (In.FallStarted < 0.0) continue;
			const K* Kk = Best[static_cast<size_t>(I)];
			const bool bBullet = Kk && (Kk->W == "rifle" || Kk->W == "mg" || Kk->W == "smg" || Kk->W == "pistol");
			if (bBullet) { ++Out.Bullet; if (Struck[static_cast<size_t>(I)] == 0) ++Out.Missing; }
			else ++Out.Other;
		}
		return Out;
	}

	TheaterFx::Battle::View EyeAt(const TheaterFx::Vec& Eye, const TheaterFx::Vec& Look, double Ppr)
	{
		TheaterFx::Battle::View V;
		V.Eye = Eye;
		V.Forward = (Look - Eye).Unit();
		V.Right = V.Forward.Cross(TheaterFx::Vec(0.0, 0.0, 1.0)).Unit();
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = Ppr;
		return V;
	}

	void Battles(const std::vector<std::string>& Paths, const Files& Fs, const FlatRecipe& Book)
	{
		std::printf("== battles\n");
		for (const std::string& Path : Paths)
		{
			std::string Name = Path.substr(Path.find_last_of('/') + 1);
			Name = Name.substr(0, Name.size() - 5);
			const std::string Text = ReadFile(Path);
			const Rows On = PlayBattle(Text, Fs, AllOn(true));
			const Rows Was = PlayBattle(Text, Fs, AllOn(false));
			const Rows Off = PlayBattle(Text, Fs, Crowd::Hits());
			for (int Pass = 0; Pass < 3; ++Pass)
			{
				const Rows& R = Pass == 0 ? On : Pass == 1 ? Was : Off;
				const char* P = Pass == 0 ? "hits on" : Pass == 1 ? "hits on less bArrive" : "hits off";
				std::printf("  %s (%s): %d men, %d bullet deaths, %d others; %d strikes (%d with no firer known), the round's flight at most %.3f s, off its range's by %.4f s at most\n",
					Name.c_str(), P, R.Men, R.Bullet, R.Other, R.Strikes, R.NoFirer, R.MostFlight, R.MostDev);
				Check(R.Strikes == R.Bullet && R.Twice == 0 && R.Missing == 0 && R.Wrong == 0,
					Name + " (" + P + Fmt("): one strike for each bullet death (%.0f of %.0f), none twice (%.0f), none for another death (%.0f)", R.Strikes, R.Bullet, R.Twice, R.Wrong));
				Check(R.BadT == 0, Name + " (" + P + Fmt("): each strike at his fall's start and the round's arrival (%.0f not)", R.BadT));
				Check(R.BadBox == 0 && R.BadWay == 0, Name + " (" + P + Fmt("): each in his body's box (%.0f not) and going from its firer through him (%.0f not)", R.BadBox, R.BadWay));
			}
			// The falls with bArrive against without: they start later by the flight alone and rest where they did.
			// (his place is where his track has him as he falls: he lags the recording by his own Delay, so a fall begun
			// later finds him nearer the place the rules killed him at, never farther; and a stance or a neighbour's fall
			// read later can give another clip)
			double MostStart = 0.0, MostRest = 0.0, MostPlace = 0.0, MostFarther = 0.0;
			int Moved = 0, OtherClip = 0, Nearer = 0;
			for (size_t I = 0; I < On.Started.size(); ++I)
			{
				if (Was.Started[I] < 0.0) continue;
				const double Dt = On.Started[I] - Was.Started[I];
				MostStart = std::max(MostStart, std::fabs(Dt));
				Moved += Dt != 0.0 ? 1 : 0;
				if (On.Rested[I] >= 0.0 && Was.Rested[I] >= 0.0) MostRest = std::max(MostRest, std::fabs(On.Rested[I] - Was.Rested[I]));
				const double Gap = std::hypot(On.PX[I] - Was.PX[I], On.PY[I] - Was.PY[I]);
				const double FromWas = std::hypot(Was.PX[I] - Was.KX[I], Was.PY[I] - Was.KY[I]), FromOn = std::hypot(On.PX[I] - On.KX[I], On.PY[I] - On.KY[I]);
				MostFarther = std::max(MostFarther, FromOn - FromWas);
				Nearer += FromOn < FromWas - 0.01 ? 1 : 0;
				OtherClip += On.Clip[I] != Was.Clip[I] ? 1 : 0;
				if (Gap > 0.01 && std::getenv("LAND_MIST_WHY")) std::fprintf(stderr, "  moved: man %zu %.3f m, start %.3f -> %.3f, clip %s -> %s, from the kill's place %.3f -> %.3f m\n", I, Gap, Was.Started[I], On.Started[I], Was.Clip[I].c_str(), On.Clip[I].c_str(), std::hypot(Was.PX[I] - Was.KX[I], Was.PY[I] - Was.KY[I]), std::hypot(On.PX[I] - On.KX[I], On.PY[I] - On.KY[I]));
				MostPlace = std::max(MostPlace, Gap);
			}
			std::printf("  %s: with bArrive %d falls start later, by %.3f s at most; they come to rest %.3f s later at most; %d in another clip; his place %.3f m off at most, %d nearer the rules' place\n",
				Name.c_str(), Moved, MostStart, MostRest, OtherClip, MostPlace, Nearer);
			Check(MostStart <= 0.25 + 1e-9 && MostFarther < 0.01, Name + Fmt(": bArrive moves a fall's start by %.3f s at most (0.25 at Life 2), and no body farther from the place the rules killed him at (%.3f m at most)", MostStart, MostFarther));
			// The kit fed the battle's strikes (hits on) at their times on its life clock (Life 2), drawn 30 frames a
			// second of life from a battle camera's eye over each strike: the most pieces alive at once, the cost.
			TheaterFx::Battle Kit, Empty;
			Kit.Setup(&Book, FlatGround, nullptr, 7);
			Empty.Setup(&Book, FlatGround, nullptr, 7);
			size_t Next = 0;
			int Most = 0, Mists = 0, Pieces = 0, MostPieces = 0;
			double MostAt = -1.0;
			const double LifeEnd = (On.All.empty() ? 0.0 : On.All.back().T * 2.0) + 2.0;
			TheaterFx::Vec Mid;
			for (const Crowd::Strike& S : On.All) Mid = Mid + TheaterFx::Vec(S.At.X, S.At.Y, 0.0) * (1.0 / static_cast<double>(std::max<size_t>(1, On.All.size())));
			const TheaterFx::Battle::View V = EyeAt(Mid + TheaterFx::Vec(-150.0, -150.0, 120.0), Mid, 1000.0);
			for (int F = 0; F / 30.0 <= LifeEnd; ++F)
			{
				const double T = F / 30.0;
				while (Next < On.All.size() && On.All[Next].T * 2.0 <= T)
				{
					const Crowd::Strike& S = On.All[Next++];
					Kit.Struck(S.T * 2.0, TheaterFx::Vec(S.At.X, S.At.Y, S.At.Z), TheaterFx::Vec(S.Dir.X, S.Dir.Y, S.Dir.Z), S.Gun.c_str(), S.Part.c_str());
					Mists += Kit.StruckThrough() ? 1 : 0;
					Pieces += Kit.StruckPieces();
					MostPieces = std::max(MostPieces, Kit.StruckPieces());
				}
				const int Drawn = Kit.Draw(T, V);
				if (Drawn > Most) { Most = Drawn; MostAt = T; }
			}
			// (the densest frame drawn again, a fresh kit fed the same strikes up to it: with them, and without)
			double MsOn = 0.0, MsOff = 0.0;
			if (MostAt >= 0.0)
			{
				TheaterFx::Battle K2;
				K2.Setup(&Book, FlatGround, nullptr, 7);
				for (const Crowd::Strike& S : On.All)
				{
					if (S.T * 2.0 <= MostAt) K2.Struck(S.T * 2.0, TheaterFx::Vec(S.At.X, S.At.Y, S.At.Z), TheaterFx::Vec(S.Dir.X, S.Dir.Y, S.Dir.Z), S.Gun.c_str(), S.Part.c_str());
				}
				std::vector<double> A, B;
				for (int Rep = 0; Rep < 7; ++Rep)
				{
					const auto T0 = std::chrono::steady_clock::now();
					K2.Draw(MostAt, V);
					const auto T1 = std::chrono::steady_clock::now();
					Empty.Draw(MostAt, V);
					const auto T2 = std::chrono::steady_clock::now();
					A.push_back(std::chrono::duration<double, std::milli>(T1 - T0).count());
					B.push_back(std::chrono::duration<double, std::milli>(T2 - T1).count());
				}
				std::sort(A.begin(), A.end());
				std::sort(B.begin(), B.end());
				MsOn = A[3];
				MsOff = B[3];
			}
			// (timings vary with the machine's load: printed to stderr, so the two compilers' outputs compare)
			std::fprintf(stderr, "  %s: the densest frame's Draw %.4f ms with its mist, %.4f ms without (median of 7)\n", Name.c_str(), MsOn, MsOff);
			std::printf("  %s: the kit fed %zu strikes: %d mists drawn, %d pieces laid (%d for one strike at most), %d pieces alive at once at the most\n",
				Name.c_str(), On.All.size(), Mists, Pieces, MostPieces, Most);
			Check(MostPieces <= 10 && Kit.Draw(LifeEnd + 1.0, V) == 0, Name + Fmt(": ten pieces a strike at the most (%.0f), none left a second after the last", MostPieces));
		}
	}

	// ---- the kit alone
	void KitAlone(const FlatRecipe& Book, const FlatRecipe& Old)
	{
		std::printf("== kit\n");
		using TheaterFx::Vec;
		const TheaterFx::Battle::View V = EyeAt(Vec(15.0, 0.0, 1.4), Vec(0.0, 0.0, 1.3), 1000.0);
		// (no row: this tree's recipe less men.struck.on, and 4060f0923's)
		FlatRecipe Less = Book;
		Less.Numbers.erase("men.struck.on");
		for (int Which = 0; Which < 2; ++Which)
		{
			TheaterFx::Battle K;
			K.Setup(Which ? &Old : &Less, FlatGround, nullptr, 7);
			K.Draw(0.0, V);
			int Laid = 0;
			for (int I = 0; I < 20; ++I)
			{
				K.Struck(0.1 * I, Vec(0.0, 0.0, 1.3), Vec(1.0, 0.0, 0.0), "rifle", "chest");
				Laid += K.StruckPieces();
			}
			Check(Laid == 0 && K.Draw(1.0, V) == 0 && K.PuffsAsked() == 0, std::string("kit: ") + (Which ? "4060f0923's recipe" : "the row's on left out") + Fmt(": nothing laid (%.0f) or drawn", Laid));
		}
		// (every weapon and part, a thousand strikes each: pieces, the share passing through, no lamp, all gone)
		const char* Weapons[] = { "rifle", "mg", "smg", "pistol", "other" };
		const char* Parts[] = { "chest", "head", "thigh_l" };
		int Most = 0;
		bool bGone = true, bLamps = true;
		for (const char* W : Weapons)
		{
			for (const char* P : Parts)
			{
				TheaterFx::Battle K;
				K.Setup(&Book, FlatGround, nullptr, 7);
				K.Draw(0.0, V);
				int Through = 0;
				for (int I = 0; I < 1000; ++I)
				{
					const double T = 0.01 * I;
					K.Struck(T, Vec(0.37 * (I % 13) + 0.013 * I, 0.21 * (I % 7) - 0.007 * I, 1.3 + 0.0011 * (I % 300)), Vec(1.0, 0.1, -0.02), W, P);
					Through += K.StruckThrough() ? 1 : 0;
					Most = std::max(Most, K.StruckPieces());
					if (I % 10 == 9) K.Draw(T, V);
				}
				TheaterFx::Battle::Lamp L[8];
				bLamps = bLamps && K.Lamps(5.0, L, 8, 1.0) == 0;
				bGone = bGone && K.Draw(10.0 + 1.0, V) == 0;
				const double Odds = std::min(1.0, (std::string(W) == "rifle" || std::string(W) == "mg" ? 0.8 : std::string(W) == "smg" ? 0.3 : std::string(W) == "pistol" ? 0.25 : 0.4)
					* (std::string(P) == "thigh_l" ? 1.15 : 1.0));
				std::printf("  kit: %s into the %s: %.1f%% passed through (odds %.1f%%)\n", W, P, Through / 10.0, Odds * 100.0);
				Check(std::fabs(Through / 1000.0 - Odds) < 0.05, std::string("kit: ") + W + " into the " + P + Fmt(": the share through by its odds (%.3f against %.3f)", Through / 1000.0, Odds));
			}
		}
		Check(Most <= 10, Fmt("kit: ten pieces a strike at the most (%.0f)", Most));
		Check(bLamps, "kit: no lamp");
		Check(bGone, "kit: everything gone a second after the last strike");
		// (one strike drawn every frame: alive under a second, every piece by 0.6 s gone; a through and a lodged one)
		for (int Through = 0; Through < 2; ++Through)
		{
			TheaterFx::Battle K;
			K.Setup(&Book, FlatGround, nullptr, 7);
			K.Draw(0.0, V);
			K.Struck(0.1, Vec(0.0, 0.0, 1.3), Vec(0.0, 1.0, 0.0), "rifle", "chest", Through);
			double LastSeen = 0.0;
			int First = 0;
			for (int F = 4; F <= 60; ++F)
			{
				const int N = K.Draw(F / 30.0, V);
				if (F == 4) First = N;
				if (N > 0) LastSeen = F / 30.0 - 0.1;
			}
			Check(First == (Through ? K.StruckPieces() : 1) && LastSeen <= 0.6, std::string("kit: a strike ") + (Through ? "passing through" : "lodged")
				+ Fmt(": %.0f pieces drawn at once, the last seen %.2f s after it", First, LastSeen));
		}
		// The same dice twice: the same bytes.
		{
			TheaterFx::Battle A, B;
			A.Setup(&Book, FlatGround, nullptr, 7);
			B.Setup(&Book, FlatGround, nullptr, 7);
			for (TheaterFx::Battle* K : { &A, &B })
			{
				K->Draw(0.0, V);
				K->Struck(0.1, Vec(0.0, 0.0, 1.3), Vec(0.0, 1.0, 0.0), "mg", "chest");
				K->Struck(0.12, Vec(0.0, 1.0, 1.6), Vec(0.0, 1.0, 0.0), "rifle", "head");
				K->Draw(0.2, V);
			}
			Check(A.Points().size() == B.Points().size() && std::memcmp(A.Points().data(), B.Points().data(), A.Points().size() * sizeof(Vec)) == 0 && A.Colours() == B.Colours(), "kit: the same strikes twice, the same bytes");
		}
		// Seen end-on (the round coming toward the eye): the mist's pieces set off across the path (side_m, r1_share_k,
		// first_alpha_k) spread on the screen, not stacked on one spot; and those rows left out, or written at their
		// no-change values, the same bytes.
		{
			FlatRecipe Bare = Book;
			FlatRecipe Same = Book;
			for (const char* R : { "men.struck.mist.side_m", "men.struck.mist.r1_share_k", "men.struck.mist.first_alpha_k" })
			{
				Bare.Numbers.erase(R);
			}
			Same.Numbers["men.struck.mist.side_m"] = 0.0;
			Same.Numbers["men.struck.mist.r1_share_k"] = 0.0;
			Same.Numbers["men.struck.mist.first_alpha_k"] = 1.0;
			// (a puff's place: the mean of its points; the mist's pieces: past the exit, wider than a drop)
			auto Across = [&](const FlatRecipe& Rc, double& Rms, std::vector<Vec>& Pts, std::vector<uint8_t>& Cols)
			{
				double Sum = 0.0;
				int Count = 0;
				for (int I = 0; I < 200; ++I)
				{
					TheaterFx::Battle K;
					K.Setup(&Rc, FlatGround, nullptr, 7 + I);
					K.Draw(0.0, V);
					K.Struck(0.1, Vec(0.0, 0.0, 1.3), Vec(1.0, 0.0, 0.0), "rifle", "chest", 1);
					const int N = K.Draw(0.1 + 0.02, V);
					const size_t Per = static_cast<size_t>(K.PerPuff());
					std::vector<Vec> Mid;
					for (int J = 0; J < N; ++J)
					{
						Vec C(0.0, 0.0, 0.0);
						double Far = 0.0;
						for (size_t Pt = 0; Pt < Per; ++Pt) C = C + K.Points()[static_cast<size_t>(J) * Per + Pt];
						C = C * (1.0 / static_cast<double>(Per));
						for (size_t Pt = 0; Pt < Per; ++Pt)
						{
							const Vec Off = K.Points()[static_cast<size_t>(J) * Per + Pt] - C;
							Far = std::max(Far, Off.Dot(Off));
						}
						if (C.X > 0.05 && std::sqrt(Far) > 0.03) Mid.push_back(C);
					}
					if (I == 0)
					{
						const size_t Pn = static_cast<size_t>(N) * Per;
						Pts.assign(K.Points().begin(), K.Points().begin() + static_cast<std::ptrdiff_t>(Pn));
						Cols.assign(K.Colours().begin(), K.Colours().begin() + static_cast<std::ptrdiff_t>(Pn * 4));
					}
					if (Mid.size() < 2) continue;
					Vec M(0.0, 0.0, 0.0);
					for (const Vec& C : Mid) M = M + C;
					M = M * (1.0 / static_cast<double>(Mid.size()));
					for (const Vec& C : Mid)
					{
						Sum += (C.Y - M.Y) * (C.Y - M.Y) + (C.Z - M.Z) * (C.Z - M.Z);
						++Count;
					}
				}
				Rms = Count > 0 ? std::sqrt(Sum / Count) : 0.0;
			};
			double RmsRow = 0.0, RmsBare = 0.0, RmsSame = 0.0;
			std::vector<Vec> PRow, PBare, PSame;
			std::vector<uint8_t> CRow, CBare, CSame;
			Across(Book, RmsRow, PRow, CRow);
			Across(Bare, RmsBare, PBare, CBare);
			Across(Same, RmsSame, PSame, CSame);
			const bool bRows = Book.Numbers.count("men.struck.mist.side_m") > 0;
			std::printf("  kit: end-on at +0.02 s, the mist's pieces across the path %.3f m (rms) with the rows, %.3f m without\n", RmsRow, RmsBare);
			Check(!bRows || (RmsRow >= 0.04 && RmsRow >= 2.5 * RmsBare), Fmt("kit: end-on, the pieces spread across the path, %.3f m with the rows against %.3f m without (at least 0.04 m and 2.5 times)", RmsRow, RmsBare));
			Check(RmsSame == RmsBare && PSame.size() == PBare.size() && !PBare.empty() && std::memcmp(PSame.data(), PBare.data(), PBare.size() * sizeof(Vec)) == 0 && CSame == CBare,
				"kit: side_m, r1_share_k and first_alpha_k left out or written 0, 0, 1: the same bytes");
		}
		// A shell, a burst and a burning wreck, with strikes laid among them and without: once the mist has gone, the
		// frames the same to the byte (own dice: no draw of theirs shifted).
		{
			TheaterFx::Battle A, B;
			A.Setup(&Book, FlatGround, nullptr, 7);
			B.Setup(&Book, FlatGround, nullptr, 7);
			const TheaterFx::Battle::View W = EyeAt(Vec(-40.0, -30.0, 6.0), Vec(0.0, 0.0, 2.0), 1000.0);
			bool bSame = true;
			int Drawn[2] = { 0, 0 };
			for (int F = 0; F <= 150; ++F)
			{
				const double T = F / 30.0;
				for (TheaterFx::Battle* K : { &A, &B })
				{
					if (F == 3) K->Shell(T, Vec(4.0, 6.0, 0.0), "how105", "earth");
					if (F == 5) K->Shots(T, 3, Vec(-20.0, 10.0, 0.6), Vec(2.0, 0.0, 0.0), "mg", 8, 10.0, "earth");
					if (F == 6) K->Burn(T, Vec(8.0, -4.0, 0.0), 2.4, 60.0);
					if (K == &B && F >= 8 && F <= 14) K->Struck(T, Vec(1.0 * F, 0.0, 1.3), Vec(0.0, 1.0, 0.0), F % 2 ? "rifle" : "mg", F % 3 ? "chest" : "head");
					Drawn[K == &B ? 1 : 0] = K->Draw(T, W);
				}
				if (F >= 14 + 30)
				{
					// (the puffs drawn this frame: the room folded away past them is as long as the most a frame has drawn)
					const size_t Pts = static_cast<size_t>(Drawn[0]) * static_cast<size_t>(A.PerPuff());
					bSame = bSame && Drawn[0] == Drawn[1] && Drawn[0] > 0 && std::memcmp(A.Points().data(), B.Points().data(), Pts * sizeof(Vec)) == 0
						&& std::memcmp(A.Colours().data(), B.Colours().data(), Pts * 4) == 0;
				}
			}
			Check(bSame, "kit: a shell, an mg's burst and a fire draw the same bytes with strikes laid among them, once the mist has gone");
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::printf("usage: land_mist_test <soldiers folder> <armies.json> <recipe.txt> <old recipe.txt> [battle.json ...]\n");
		return 2;
	}
	const Files Fs = ReadFiles(Argv[1], Argv[2]);
	const FlatRecipe Book = ReadRecipe(Argv[3]);
	const FlatRecipe Old = ReadRecipe(Argv[4]);
	std::vector<std::string> Battles_;
	for (int I = 5; I < Argc; ++I) Battles_.push_back(Argv[I]);
	Events(Fs);
	KitAlone(Book, Old);
	Battles(Battles_, Fs, Book);
	std::printf("%s: %d failed\n", Failed ? "FAIL" : "ok", Failed);
	return Failed ? 1 : 0;
}
