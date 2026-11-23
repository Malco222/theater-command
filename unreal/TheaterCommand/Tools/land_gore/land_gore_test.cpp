// The gore falls' cloud test (Effects, release 2, behind -TheaterLandGore; Battle/LandMen's Crowd::AddGore and SetGore).
// Plays a small battle of its own through the crowd: 160 men, an American and a German line, killed at 3 s by a field
// gun's shell, a mortar's bomb or a rifle, each burst 0 to 13 m from the man it kills. Checks:
//   off   twins added but the switch off: every man falls exactly as with no twins (his fall, his skin, byte for byte)
//   on    only shell kills near their burst are gored, the share falling away with the distance; each gored man takes
//         one of his twin's gore falls, keeps his clips, a limb leaves its joint, his limb_off events come; no man's
//         skin holds a NaN
//   same  a second crowd gores the same men with the same falls
//   back  a replay from the start gives every man his own file again; played on, the same men are gored again
// With an out folder it also paints gore_falls.ppm: six gored men from the side, each from the hit to rest in six steps.
// Usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include <chrono>
#include <cstring>
#include <set>

namespace
{
	const char* const Cuts[] = { "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "thigh_l", "thigh_r", "calf_l", "calf_r" };
	constexpr int MenEach = 80;
	constexpr double Hit = 3.0;

	struct Who
	{
		int B = 1, K = 1;
		std::string W;
		double Burst = 0.0;
	};

	std::string Recording(std::vector<Who>& Men)
	{
		std::ostringstream R, Rounds;
		R << "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}],"
		  << "\"men\": [";
		Rounds << "\"rounds\": [";
		for (int I = 0; I < 2 * MenEach; ++I)
		{
			Who Wh;
			Wh.B = I % 2 ? 2 : 1;
			Wh.K = I / 2 + 1;
			const int Kind = (I / 2) % 8;
			Wh.W = Kind < 6 ? "gun" : Kind == 6 ? "mortar" : "rifle";
			Wh.Burst = Wh.W == "rifle" ? 0.0 : static_cast<double>((I / 16) % 14);
			const double X = 30.0 * (I / 2), Y = Wh.B == 1 ? 0.0 : 60.0, F = Wh.B == 1 ? 0.0 : 3.14159265358979;
			const char* Stance = (I / 2) % 5 == 4 ? "wait" : "fire_stand";
			if (I) { R << ","; Rounds << ","; }
			R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X << ", " << Y << ", " << F << "], [10, " << X << ", " << Y << ", " << F
			  << "]], \"ch\": [[0, \"" << Stance << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
			// (the burst that far from him, off to his side: x, y the round's, as the drill writes a shell's)
			const double A = 0.7 * I;
			Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", \"on\": " << Wh.B << ", \"ok\": " << Wh.K << ", \"x\": " << X + Wh.Burst * std::cos(A)
			       << ", \"y\": " << Y + Wh.Burst * std::sin(A) << ", \"r\": \"kill\"}";
			Men.push_back(Wh);
		}
		R << "], " << Rounds.str() << "]}";
		return R.str();
	}

	uint64_t HashOf(const std::vector<float>& V, uint64_t H)
	{
		for (const float F : V)
		{
			uint32_t U = 0;
			std::memcpy(&U, &F, sizeof(U));
			H = (H ^ U) * 1099511628211ull;
		}
		return H;
	}

	/** The widest any limb stands off its joint (metres): the cut bone's head against where its parent holds it. */
	double LimbGap(const Crowd& C, int Man)
	{
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		double Most = 0.0;
		for (const char* Name : Cuts)
		{
			const int B = S.BoneIndex(Name);
			if (B < 0) continue;
			const int P = S.Bones[static_cast<size_t>(B)].Parent;
			if (P < 0) continue;
			const Vec3 Head = S.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[P].Point(Head);
			Most = std::max(Most, std::sqrt(static_cast<double>((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z))));
		}
		return Most;
	}

	struct Result
	{
		std::vector<std::string> Clip;
		std::vector<int> Gored;
		std::vector<double> Gap;
		uint64_t Skin = 1469598103934665603ull;
		int LimbOff = 0, NaN = 0;
		double Ms = 0.0;
	};

	Result Play(Crowd& C, const std::vector<Who>& Men, double From, double To)
	{
		Result Out;
		const auto T0 = std::chrono::steady_clock::now();
		for (double T = From; T <= To; T += 1.0 / 60.0)
		{
			C.Update(T);
			for (const Event& E : C.Events()) Out.LimbOff += E.Fx == "limb_off" ? 1 : 0;
		}
		Out.Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const int Man = C.Find(Men[I].B, Men[I].K);
			Out.Clip.push_back(C.Info(Man).FallClip);
			std::vector<float> At, Fa;
			Skinned(C, Man, At, Fa);
			for (const float F : At) Out.NaN += std::isfinite(F) ? 0 : 1;
			Out.Skin = HashOf(At, Out.Skin);
			Out.Gap.push_back(LimbGap(C, Man));
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]\n");
		return 2;
	}
	std::string Err;
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	std::map<std::string, Soldier> Twins;
	for (const char* Army : { "GER", "POL", "USA" })
	{
		auto S = std::make_shared<Soldier>();
		if (!S->Load(ReadFile(std::string(Argv[1]) + "/soldier_" + Army + ".json"), Err)) { std::printf("soldier %s: %s\n", Army, Err.c_str()); return 1; }
		Files[Army] = S;
		const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + "_gore.json");
		if (Text.empty()) continue;
		Soldier Tw;
		if (!Tw.Load(Text, Err)) { std::printf("twin %s: %s\n", Army, Err.c_str()); return 1; }
		Twins[Army] = std::move(Tw);
	}
	if (Twins.find("USA") == Twins.end() || Twins.find("GER") == Twins.end()) { std::printf("FAIL: no USA or GER twin in %s\n", Argv[2]); return 1; }
	const std::string Armies = ReadFile(Argv[3]);
	std::vector<Who> Men;
	const std::string Rec = Recording(Men);
	const GroundFn Flat = [](double, double) { return Ground(); };
	int Failed = 0;
	auto Check = [&Failed](bool bOk, const std::string& What) { std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str()); Failed += bOk ? 0 : 1; };
	auto Make = [&](Crowd& C, bool bTwins, bool bOn) {
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		if (bTwins)
		{
			for (const auto& KV : Twins)
			{
				std::string Why;
				if (!C.AddGore(KV.first, KV.second, Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
			}
		}
		C.SetGore(bOn);
		if (!C.ReadArmies(Armies, Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); std::exit(1); }
		C.SetEye(-1000, -1000, 2);
	};
	const double Until = Hit + 5.0;

	// ---- Off: the twins added, the switch off, against no twins at all.
	{
		Crowd A, B;
		Make(A, false, false);
		Make(B, true, false);
		const Result Ra = Play(A, Men, 0.0, Until), Rb = Play(B, Men, 0.0, Until);
		Check(Ra.Clip == Rb.Clip && Ra.Skin == Rb.Skin && B.Gored() == 0 && Rb.LimbOff == 0, "off: every fall and every skin as with no twins, none gored");
		std::printf("     (%.0f ms against %.0f ms for %d frames)\n", Rb.Ms, Ra.Ms, static_cast<int>(Until * 60.0));
	}

	// ---- On.
	Crowd C;
	Make(C, true, true);
	const Result Rc = Play(C, Men, 0.0, Until);
	int Gored = 0, RifleGored = 0, FarGored = 0, BadClip = 0, NoLimb = 0, LimbOnWhole = 0, LostClips = 0;
	int ByD[14] = {}, ShellD[14] = {};
	std::vector<int> GoredMen;
	for (size_t I = 0; I < Men.size(); ++I)
	{
		const int Man = C.Find(Men[I].B, Men[I].K);
		const Soldier* Own = Files[C.Info(Man).FileArmy].get();
		const bool bGored = C.SoldierOf(Man) != Own;
		if (Men[I].W != "rifle") { ++ShellD[static_cast<int>(Men[I].Burst)]; if (bGored) ++ByD[static_cast<int>(Men[I].Burst)]; }
		if (!bGored)
		{
			if (Rc.Gap[I] > 0.05) ++LimbOnWhole;
			continue;
		}
		++Gored;
		GoredMen.push_back(static_cast<int>(I));
		const Soldier& S = *C.SoldierOf(Man);
		if (Men[I].W == "rifle") ++RifleGored;
		const double Reach = Men[I].W == "gun" ? 12.0 : 8.0;
		if (Men[I].Burst >= Reach) ++FarGored;
		bool bIn = false;
		for (const auto& St : S.DeathsGore) for (const auto& Kd : St.second) for (const std::string& N : Kd.second) bIn = bIn || N == Rc.Clip[I];
		if (!bIn) ++BadClip;
		if (Rc.Gap[I] < 0.25) ++NoLimb;
		for (const Clip& Cl : Own->Clips) LostClips += S.ClipIndex(Cl.Name) == Own->ClipIndex(Cl.Name) ? 0 : 1;
	}
	std::printf("     gored %d of %d men (%d twins read); by the burst's distance (shell kills):", Gored, static_cast<int>(Men.size()), static_cast<int>(Twins.size()));
	for (int D = 0; D < 14; ++D) std::printf(" %dm %d/%d", D, ByD[D], ShellD[D]);
	std::printf("\n");
	Check(Gored > 0 && Gored == C.Gored(), "on: men gored, and the crowd's count says so");
	Check(RifleGored == 0, "on: no man killed by a rifle gored");
	Check(FarGored == 0, "on: no man past the burst's reach gored");
	Check(ByD[0] + ByD[1] + ByD[2] + ByD[3] > ByD[8] + ByD[9] + ByD[10] + ByD[11], "on: more gored near the burst than far from it");
	Check(BadClip == 0, "on: each gored man in one of his twin's gore falls");
	Check(LostClips == 0, "on: each gored man keeps every clip number of his own file");
	Check(NoLimb == 0, "on: each gored man has a limb off its joint (more than 0.25 m) at rest");
	Check(LimbOnWhole == 0, "on: no other man's limb leaves its joint");
	Check(Rc.LimbOff >= Gored, "on: the limb_off events come (" + std::to_string(Rc.LimbOff) + ")");
	Check(Rc.NaN == 0, "on: no NaN in any skin");
	std::printf("     (%.0f ms for %d frames)\n", Rc.Ms, static_cast<int>(Until * 60.0));

	// ---- Same: another crowd, built alike.
	{
		Crowd D;
		Make(D, true, true);
		const Result Rd = Play(D, Men, 0.0, Until);
		Check(Rd.Clip == Rc.Clip && Rd.Skin == Rc.Skin, "same: a second crowd gores the same men with the same falls");
	}

	// ---- Back: a replay from the start, then on again.
	{
		std::vector<uint32_t> No;
		for (const int I : GoredMen) No.push_back(C.MeshNo(C.Find(Men[static_cast<size_t>(I)].B, Men[static_cast<size_t>(I)].K)));
		C.Update(0.5);
		bool bOwn = C.Gored() == 0;
		for (size_t K = 0; K < GoredMen.size(); ++K)
		{
			const Who& Wh = Men[static_cast<size_t>(GoredMen[K])];
			const int Man = C.Find(Wh.B, Wh.K);
			bOwn = bOwn && C.SoldierOf(Man) == Files[C.Info(Man).FileArmy].get() && C.MeshNo(Man) != No[K] && C.PointsOf(Man, Near) > 0;
		}
		Check(bOwn, "back: a replay from the start gives every gored man his own file and mesh again");
		const Result Re = Play(C, Men, 0.5, Until);
		Check(Re.Clip == Rc.Clip && Re.Skin == Rc.Skin, "back: played on, the same men are gored with the same falls");
	}

	// ---- The picture: six gored men, each in his own fall, from the side.
	if (Argc > 4 && !GoredMen.empty())
	{
		std::vector<int> Pick;
		std::set<std::string> Seen;
		for (const int I : GoredMen) if (Pick.size() < 6 && Seen.insert(Rc.Clip[static_cast<size_t>(I)]).second) Pick.push_back(I);
		for (const int I : GoredMen) if (Pick.size() < 6 && std::find(Pick.begin(), Pick.end(), I) == Pick.end()) Pick.push_back(I);
		Crowd P;
		Make(P, true, true);
		const int Cols = 6, Rows = static_cast<int>(Pick.size()), SheetW = 1600, CellH = 210;
		struct Step { std::vector<float> At, Fa; };
		std::vector<std::vector<Step>> Steps(static_cast<size_t>(Rows), std::vector<Step>(Cols));
		std::vector<std::pair<double, std::pair<int, int>>> Due;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const Soldier& Tw = *C.SoldierOf(C.Find(Wh.B, Wh.K));
			const int Ix = Tw.ClipIndex(Rc.Clip[static_cast<size_t>(Pick[static_cast<size_t>(Row)])]);
			const double Long = Ix >= 0 ? Tw.Clips[static_cast<size_t>(Ix)].Long : 2.0;
			for (int Col = 0; Col < Cols; ++Col) Due.push_back({ Hit + 0.02 + (Col < Cols - 1 ? Long * Col / (Cols - 1) : Long + 0.3), { Row, Col } });
		}
		std::stable_sort(Due.begin(), Due.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		double Now = 0.0;
		for (const auto& Dn : Due)
		{
			while (Now + 1.0 / 60.0 < Dn.first) { Now += 1.0 / 60.0; P.Update(Now); }
			Now = Dn.first;
			P.Update(Now);
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Dn.second.first)])];
			Step& St = Steps[static_cast<size_t>(Dn.second.first)][static_cast<size_t>(Dn.second.second)];
			Skinned(P, P.Find(Wh.B, Wh.K), St.At, St.Fa);
		}
		std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * Rows) * 3, 236);
		Light Lt = Daylight(215.0, 13.0);
		Lt.EarthA = Lin(118, 98, 76);
		Lt.EarthB = Lin(96, 80, 62);
		Lt.HazeK = 0.0;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const int Man = P.Find(Wh.B, Wh.K);
			std::printf("     picture row %d: %s, %s %s, burst %.0f m off, %s (%s)\n", Row, P.Info(Man).FileArmy.c_str(), Wh.W.c_str(), "kill", Wh.Burst,
				P.Info(Man).FallClip.c_str(), P.Info(Man).FallKind.c_str());
			double XLo = 1e9, XHi = -1e9, YLo = 1e9, YHi = -1e9;
			for (const Step& St : Steps[static_cast<size_t>(Row)])
			{
				for (size_t K = 0; K < St.At.size(); K += 3)
				{
					XLo = std::min(XLo, static_cast<double>(St.At[K])); XHi = std::max(XHi, static_cast<double>(St.At[K]));
					YLo = std::min(YLo, static_cast<double>(St.At[K + 1])); YHi = std::max(YHi, static_cast<double>(St.At[K + 1]));
				}
			}
			const double XMid = 0.5 * (XLo + XHi), YMid = 0.5 * (YLo + YHi);
			const double Back = std::max(5.0, (YHi - YLo) * 1.4 + 3.0);
			for (int Col = 0; Col < Cols; ++Col)
			{
				const int OX = (SheetW * Col) / Cols, OX1 = (SheetW * (Col + 1)) / Cols;
				const int CW = OX1 - OX - 4, CH = CellH - 4;
				Scene Sc;
				const Step& St = Steps[static_cast<size_t>(Row)][static_cast<size_t>(Col)];
				Sc.AddMan(P.LookOf(Man, Near), St.At, St.Fa);
				Sc.AddGround(XMid - 10.0, YMid - 10.0, XMid + 10.0, YMid + 12.0, 0.5, Flat);
				const double Span = std::max(XHi - XLo + 0.8, 2.4 * CW / CH);
				const double Fov = 2.0 * std::atan(Span * 0.5 / Back) * 180.0 / 3.14159265358979;
				const Camera Cam({ XMid, YMid - Back, 1.45 }, { XMid, YMid, 0.6 }, Fov, CW * 2, CH * 2);
				ShadowMap Sh;
				Sh.Build(Sc, Lt.ToSun, { XMid - 4.0, YMid - 4.0, -0.1 }, { XMid + 4.0, YMid + 4.0, 2.2 }, 1536);
				const Picture Pic = Paint(Sc, Cam, Lt, Sh);
				Put(Pic, 1.0, Sheet, SheetW, OX + 2, Row * CellH + 2);
			}
		}
		const std::string OutPath = std::string(Argv[4]) + "/gore_falls.ppm";
		if (!WritePpm(OutPath, Sheet, SheetW, CellH * Rows)) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
		std::printf("wrote %s (%dx%d)\n", OutPath.c_str(), SheetW, CellH * Rows);
	}
	std::printf(Failed ? "land_gore_test: %d FAILED\n" : "land_gore_test: all passed\n", Failed);
	return Failed ? 1 : 0;
}
