// The blood mist's pictures (-TheaterLandHitMist; Effects R2): the men from Battle/LandMen's own skinning (the crowd
// as the scene plays it, every hit part on with bArrive) and the mist from Effects/TheaterFxCore (Battle::Struck, fed
// each strike as LandMen::Crowd::StrikesIn gives it), drawn together by the kit's previewer's own painter
// (tools/battle-fx/preview/fxpreview.cpp: its ground, its depth test, its puffs) through a 42 degree lens at 1920 x
// 1080. Each frame is written as linear light (<name>.f32, three floats a pixel) with the mist and without it, for
// land_mist_pictures.py to grade through the land battle's grade (land_grade.py) and lay out. Not the game's renderer:
// flat-shaded men, one sun.
//   rifle_<d>_<k>     one German rifleman standing, killed by a rifle round from 150 m ahead, seen from his side at
//                     d = 15, 40, 100 and 300 m, at 0, 0.05, 0.15, 0.3 and 0.6 s of life after it arrives
//   mg_<k>            an mg's burst across three men a metre and a half apart, 20 m off, at 0.05, 0.15, 0.3, 0.6 s
//                     after the first strike
//   head_<k>          a real head strike: the same man, his fall by the crowd's own rules a die_head one, so
//                     StruckPlace gives the head bone's place; 15 m off, at the same times as the rifle's
//   facing_15_<k>, sky_20_<k>, woods_40_<k>   the rifle's strike from the far side (the mist toward the eye), from a
//                     low eye against the sky, and at 40 m against dark woods (FX_BACK=80,95,59 given by the caller)
// With men.txt: each strike's time, place, part, whether it passed through and its pieces.
// Usage: [LAND_MIST_SHOTS=rifle_15,mg] land_mist_picture <soldiers folder> <armies.json> <recipe.txt> <out folder>
#define main FxPreviewMain
#include "fxpreview.cpp"
#undef main

#include "Battle/LandMen.h"

#include <dirent.h>

namespace
{
	struct MenFiles
	{
		std::map<std::string, std::shared_ptr<LandMen::Soldier>> ByArmy;
		std::string Armies;
	};

	std::string Slurp(const std::string& P)
	{
		std::ifstream F(P, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	MenFiles ReadMen(const std::string& Folder, const std::string& ArmiesPath)
	{
		MenFiles Fs;
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
			auto S = std::make_shared<LandMen::Soldier>();
			std::string Err;
			if (!S->Load(Slurp(Folder + "/" + N), Err)) continue;
			Fs.ByArmy[N.substr(8, N.size() - 13)] = S;
		}
		Fs.Armies = Slurp(ArmiesPath);
		return Fs;
	}

	struct Victim
	{
		double X = 0.0, Y = 0.0, D = 5.0;
		std::string Ch = "idle";   // (what he is doing as the round comes: "fire_stand" for a man aiming)
	};

	/** GER men (block 2) standing facing +y, each killed at D by a round of W from his firer (block 1) at SX, SY. */
	std::string Battle_(const std::vector<Victim>& Vs, double SX, double SY, const std::string& W, int Seed = 42)
	{
		std::ostringstream R, Sh, Rounds;
		R.precision(10);
		Sh.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": " << Seed << ", \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}], \"men\": [";
		Rounds << "\"rounds\": [";
		for (size_t I = 0; I < Vs.size(); ++I)
		{
			const Victim& V = Vs[I];
			R << (I ? "," : "") << "{\"b\": 2, \"k\": " << I + 1 << ", \"r\": \"rifle\", \"at\": [[0, " << V.X << ", " << V.Y << ", 0], [40, " << V.X << ", " << V.Y
			  << ", 0]], \"ch\": [[0, \"" << V.Ch << "\", \"none\", 0, 0, 0, 0]], \"d\": " << V.D << "}";
			Rounds << (I ? "," : "") << "{\"t\": " << V.D << ", \"l\": " << V.D << ", \"w\": \"" << W << "\", \"by\": 1, \"k\": 1, \"on\": 2, \"ok\": " << I + 1
				   << ", \"x\": " << V.X << ", \"y\": " << V.Y << ", \"r\": \"kill\"}";
		}
		const double Face = std::atan2(-SX, -SY);
		Sh << ", {\"b\": 1, \"k\": 1, \"r\": \"rifle\", \"at\": [[0, " << SX << ", " << SY << ", " << Face << "], [40, " << SX << ", " << SY << ", " << Face
		   << "]], \"ch\": [[0, \"fire_prone\", \"none\", 0, 0, 0, 0]]}";
		R << Sh.str() << "], " << Rounds.str() << "]}";
		return R.str();
	}

	void Men(Canvas& C, const Camera& Cam, LandMen::Crowd& Cr)
	{
		const Vec Sun = Vec(-0.45, -0.35, 0.82).Unit();
		for (int Man = 0; Man < Cr.Count(); ++Man)
		{
			if (!Cr.Visible(Man) || Cr.IsRider(Man)) continue;
			const LandMen::Look& Lk = Cr.LookOf(Man, LandMen::Near);
			const int N = Cr.PointsOf(Man, LandMen::Near);
			std::vector<float> At(static_cast<size_t>(N) * 3), Fa(static_cast<size_t>(N) * 3);
			Cr.Skin(Man, LandMen::Near, At.data(), Fa.data());
			auto P = [&](uint32_t K) { return Vec(At[K * 3], At[K * 3 + 1], At[K * 3 + 2]); };
			for (size_t T = 0; T + 2 < Lk.Triangles.size(); T += 3)
			{
				const uint32_t A = Lk.Triangles[T], B = Lk.Triangles[T + 1], Cc = Lk.Triangles[T + 2];
				const Vec Pts[3] = { P(A), P(B), P(Cc) };
				const Vec Nn = (Pts[1] - Pts[0]).Cross(Pts[2] - Pts[0]).Unit();
				const double Lit = 0.32 + 0.68 * std::fabs(Nn.Dot(Sun)); // (two-sided: his own axes are the mirror's)
				double Col[3];
				for (int K = 0; K < 3; ++K)
				{
					const double Byte = (Lk.Rgb[A * 3 + K] + Lk.Rgb[B * 3 + K] + Lk.Rgb[Cc * 3 + K]) / 3.0;
					Col[K] = ToLinear(Byte) * Lit;
				}
				Solid(C, Cam, Pts, Col);
			}
		}
	}

	struct Shot
	{
		std::string Name;
		std::vector<Victim> Vs;
		double SX = 0.0, SY = 150.0;
		std::string W = "rifle";
		Vec Eye, Look;
		std::vector<double> After;   // seconds of life after the first strike
		bool bHead = false;
		int Seed = 42;               // (the recording's seed: the head shot's is one whose fall the crowd's own rules make a die_head one)
		int Through = 1;             // (the kit's pass-through roll forced: 1 through, 0 lodged, -1 its own dice)
	};

	void Take(const Shot& S, const MenFiles& Fs, const FlatRecipe& Book, const std::string& Out, FILE* Log)
	{
		LandMen::Crowd Cr;
		Cr.SetLife(2.0);
		for (const auto& KV : Fs.ByArmy) Cr.AddSoldier(KV.first, KV.second);
		std::string Err;
		LandMen::Crowd::Hits H;
		H.bJolt = H.bVariety = H.bWounded = H.bFlinch = H.bArrive = true;
		Cr.SetHits(H);
		if (!Cr.ReadArmies(Fs.Armies, Err) || !Cr.Build(Battle_(S.Vs, S.SX, S.SY, S.W, S.Seed), [](double X, double Y) { LandMen::Ground G; G.Height = static_cast<float>(Swell(nullptr, X, Y)); return G; }, Err))
		{
			std::printf("cannot build: %s\n", Err.c_str());
			std::exit(1);
		}
		if (S.bHead) Cr.SetFallKind(Cr.Find(2, 1, 0), "head");
		Cr.SetEye(S.Eye.X, S.Eye.Y, S.Eye.Z);
		Battle Fx;
		Fx.Setup(&Book, &Swell, nullptr, 3u);
		Fx.SetSrgbBytes(true);
		Fx.SetLandGrade(true);
		Fx.SetSoftContact(0.1);
		Fx.SetWind(Vec(1.6, 0.4, 0.0));
		Camera Cam;
		Cam.W = 1920;
		Cam.H = 1080;
		Cam.Eye = S.Eye;
		Cam.Forward = (S.Look - S.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Cam.Ppr = Cam.W / (2.0 * std::tan(21.0 * 3.14159265358979 / 180.0)); // (the game's 42 degree lens)
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr;
		View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
		View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		// (the battle's clock 240 steps a battle second, the kit's twice it: Life 2; the strikes handed over as they come)
		double First = -1.0, Asked = -1.0;
		std::vector<LandMen::Crowd::Strike> Got;
		size_t Panel = 0;
		Fx.Draw(0.0, View);
		for (int F = 0; F <= 240 * 9 && Panel < S.After.size(); ++F)
		{
			const double T = F / 240.0;
			Cr.Update(T);
			Got.clear();
			if (Asked < 0.0) Asked = T;
			Cr.StrikesIn(Asked, T, Got);
			Asked = T;
			for (LandMen::Crowd::Strike& K : Got)
			{
				// (the head shot: a real one, the place StruckPlace gives for his die_head fall, on the head bone; no stand-in)
				Fx.Struck(K.T * 2.0, Vec(K.At.X, K.At.Y, K.At.Z), Vec(K.Dir.X, K.Dir.Y, K.Dir.Z), K.Gun.c_str(), K.Part.c_str(), S.Through);
				if (First < 0.0) First = K.T;
				std::fprintf(Log, "%s: man %d struck at %.4f s (battle; %.4f of life) in the %s, at %.2f,%.2f,%.2f going %.3f,%.3f,%.3f from %.0f m: %s, %d pieces; his fall %s\n", S.Name.c_str(), K.Man, K.T, K.T * 2.0,
					K.Part.c_str(), static_cast<double>(K.At.X), static_cast<double>(K.At.Y), static_cast<double>(K.At.Z), static_cast<double>(K.Dir.X), static_cast<double>(K.Dir.Y), static_cast<double>(K.Dir.Z),
					static_cast<double>(K.Range), Fx.StruckThrough() ? "through" : "lodged", Fx.StruckPieces(), Cr.Info(K.Man).FallClip.c_str());
			}
			if (F % 4 == 0) Fx.Draw(T * 2.0, View); // (the kit drawn 60 times a second of life, as a frame rate)
			while (First >= 0.0 && Panel < S.After.size() && T * 2.0 + 1e-9 >= First * 2.0 + S.After[Panel])
			{
				const int N = Fx.Draw(T * 2.0, View);
				for (int Mist = 0; Mist < 2; ++Mist)
				{
					Canvas C;
					C.Init(Cam.W, Cam.H);
					const std::vector<Vec> NoMen;
					Paint(C, Cam, nullptr, 0, NoMen, false);
					Men(C, Cam, Cr);
					if (Mist) Puffs(C, Cam, Fx, N);
					char Name[512];
					std::snprintf(Name, sizeof(Name), "%s/%s_%zu_%s.ppm", Out.c_str(), S.Name.c_str(), Panel, Mist ? "on" : "off");
					C.Save(Name);
				}
				std::fprintf(Log, "%s panel %zu: %.3f s after the first strike, %d pieces drawn\n", S.Name.c_str(), Panel, T * 2.0 - First * 2.0, N);
				++Panel;
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::printf("usage: land_mist_picture <soldiers folder> <armies.json> <recipe.txt> <out folder>\n");
		return 2;
	}
	const MenFiles Fs = ReadMen(Argv[1], Argv[2]);
	// (LAND_MIST_PROBE=1: the fall the crowd's own rules give the head shot's man, by the recording's seed and what he
	// was doing, and the part StruckPlace names for it; no pictures)
	if (std::getenv("LAND_MIST_PROBE"))
	{
		for (const char* Ch : { "idle", "fire_stand" })
		{
			for (int Seed = 1; Seed <= 40; ++Seed)
			{
				LandMen::Crowd Cr;
				Cr.SetLife(2.0);
				for (const auto& KV : Fs.ByArmy) Cr.AddSoldier(KV.first, KV.second);
				std::string Err;
				LandMen::Crowd::Hits H;
				H.bJolt = H.bVariety = H.bWounded = H.bFlinch = H.bArrive = true;
				Cr.SetHits(H);
				Victim V;
				V.Ch = Ch;
				if (!Cr.ReadArmies(Fs.Armies, Err) || !Cr.Build(Battle_({ V }, 0.0, 150.0, "rifle", Seed), [](double X, double Y) { LandMen::Ground G; G.Height = static_cast<float>(Swell(nullptr, X, Y)); return G; }, Err)) return 1;
				const int Man = Cr.Find(2, 1, 0);
				Cr.SetFallKind(Man, "head");
				Cr.Update(5.3);
				LandMen::Vec3 At;
				std::string Part;
				const bool bAt = Cr.StruckPlace(Man, At, Part);
				std::printf("probe %s seed %d man %d: fall %s, struck %s at %.2f,%.2f,%.2f\n", Ch, Seed, Man, Cr.Info(Man).FallClip.c_str(), bAt ? Part.c_str() : "-",
					static_cast<double>(At.X), static_cast<double>(At.Y), static_cast<double>(At.Z));
			}
		}
		return 0;
	}
	FlatRecipe Book;
	{
		std::ifstream F(Argv[3]);
		std::string Line;
		while (std::getline(F, Line))
		{
			const size_t Tab = Line.find('\t');
			if (Tab == std::string::npos) continue;
			const std::string Key = Line.substr(0, Tab), Val = Line.substr(Tab + 1);
			if (!Val.empty() && Val[0] == '=') Book.Words[Key] = Val.substr(1);
			else Book.Numbers[Key] = std::stod(Val);
		}
	}
	const std::string Out = Argv[4];
	setenv("FX_FLOAT", "1", 1);
	// (the field green written to land on about 98,104,62 under the land battle's grade: land_grade.py write 98 104 62)
	if (!std::getenv("FX_GROUND_RAW")) setenv("FX_GROUND_RAW", "134,140,70", 1);
	FILE* Log = std::fopen((Out + "/men.txt").c_str(), "a");
	if (!Log) return 1;
	const std::vector<double> Times = { 0.0, 0.05, 0.15, 0.3, 0.6 };
	std::vector<Shot> Shots;
	for (double D : { 15.0, 40.0, 100.0, 300.0 })
	{
		Shot S;
		char Name[32];
		std::snprintf(Name, sizeof(Name), "rifle_%.0f", D);
		S.Name = Name;
		S.Vs = { Victim() };
		S.Eye = Vec(D, -0.6, 1.55 + Swell(nullptr, D, -0.6));
		S.Look = Vec(0.0, -0.6, 1.1);
		S.After = Times;
		Shots.push_back(S);
	}
	{
		Shot S;
		S.Name = "mg";
		S.W = "mg";
		S.Through = -1;
		S.SX = 60.0;
		S.SY = 110.0;
		for (int K = 0; K < 3; ++K)
		{
			Victim V;
			V.X = -1.5 + 1.5 * K;
			V.Y = 0.3 * K;
			V.D = 5.0 + 0.05 * K;   // (a tenth of a second of life apart: the burst's rounds)
			S.Vs.push_back(V);
		}
		S.Eye = Vec(15.0, -12.0, 1.7);
		S.Look = Vec(0.0, 0.0, 1.0);
		S.After = { 0.05, 0.15, 0.3, 0.6 };
		Shots.push_back(S);
	}
	{
		Shot S;
		S.Name = "head";
		S.Vs = { Victim() };
		S.bHead = true;
		S.Seed = 3;   // (a recording whose dice give him die_head_b, as most seeds do: LAND_MIST_PROBE=1 lists them)
		S.Eye = Vec(15.0, -0.6, 1.6 + Swell(nullptr, 15.0, -0.6));
		S.Look = Vec(0.0, -0.6, 1.2);
		S.After = Times;
		Shots.push_back(S);
	}
	// (the same rifle round seen from the far side, the mist coming toward the eye; low at 20 m against the sky; at 40 m
	// against dark woods, FX_BACK given by the caller)
	{
		Shot S;
		S.Name = "facing_15";
		S.Vs = { Victim() };
		S.Eye = Vec(1.2, -15.0, 1.6 + Swell(nullptr, 1.2, -15.0));
		S.Look = Vec(0.0, 0.0, 1.1);
		S.After = Times;
		Shots.push_back(S);
		S.Name = "sky_20";
		S.Eye = Vec(20.0, -0.6, 0.35 + Swell(nullptr, 20.0, -0.6));
		S.Look = Vec(0.0, -0.6, 1.45);
		Shots.push_back(S);
		S.Name = "woods_40";
		S.Eye = Vec(40.0, -0.6, 1.55 + Swell(nullptr, 40.0, -0.6));
		S.Look = Vec(0.0, -0.6, 1.1);
		Shots.push_back(S);
	}
	// (LAND_MIST_SHOTS=rifle_15,mg: those shots alone, so a small disk can take them a few at a time; men.txt added to)
	const char* Only = std::getenv("LAND_MIST_SHOTS");
	for (const Shot& S : Shots)
	{
		if (Only && (std::string(",") + Only + ",").find("," + S.Name + ",") == std::string::npos) continue;
		Take(S, Fs, Book, Out, Log);
	}
	std::fclose(Log);
	return 0;
}
