// Men out of their machines without their packs, for the art director (Effects, R2: Crowd::PilotDress and CrewDress,
// AddRider's bNoPack; the soldiers thread's pack_tris): a downed pilot and a tank's crewman bailing out of armies with no
// tankers' file (the Russian and the British), as the crowd draws them, by land_men_picture's painter (its light and
// ground are stand-ins). Rows: each army's pilot (bareheaded, rifle and pack off), then its crewman (helmet on, rifle
// and pack off). Columns: near from the front, standing as he does once up (stand_up's end), then the same as before
// (his pack on), to compare what goes from his front (his webbing's straps and pouches, and anything that should not:
// the review of 24 November found the collar tabs in fc61734e's run); near from behind, standing; mid from behind,
// running (run); near from behind as before (his pack on), to compare. pack_off.ppm, and pack_off.txt (rows, columns,
// what each man is drawn with).
// Usage: Tools/land_show/pack_picture.sh [soldiers folder with pack_tris] [out folder]
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include "pack_cut.h"

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: pack_picture <soldiers folder> <armies.json> <out folder>\n");
		return 2;
	}
	std::string Err;
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	for (const char* Name : { "SOV", "UK" })
	{
		auto S = std::make_shared<Soldier>();
		if (!S->Load(ReadFile(std::string(Argv[1]) + "/soldier_" + Name + ".json"), Err)) { std::printf("soldier_%s: %s\n", Name, Err.c_str()); return 1; }
		Files[Name] = S;
	}
	Crowd C;
	for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
	const std::string Rec = "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, \"blocks\": [], \"men\": [], \"rounds\": []}";
	const GroundFn Flat = [](double, double) { return Ground(); };
	if (!C.ReadArmies(ReadFile(Argv[2]), Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); return 1; }
	C.SetEye(6.0, 0.0, 1.6);

	struct Row
	{
		const char* Nat;
		bool bPilot;
	};
	const Row Rows[] = { { "SOV", true }, { "SOV", false }, { "UK", true }, { "UK", false } };
	// (columns: level, from behind, his pack as before, clip, time into it)
	struct Col
	{
		Lod L;
		bool bBack, bBefore;
		const char* Clip;
		float Time;
	};
	const Col Cols[] = { { Near, false, false, "stand_up", 0.6f }, { Near, false, true, "stand_up", 0.6f }, { Near, true, false, "stand_up", 0.6f },
		{ Mid, true, false, "run", 0.25f }, { Near, true, true, "stand_up", 0.6f } };
	const int NC = static_cast<int>(sizeof(Cols) / sizeof(Cols[0])), NR = 4, CellW = 330, CellH = 540, SheetW = CellW * NC;
	std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * NR) * 3, 236);
	std::ofstream Notes(std::string(Argv[3]) + "/pack_off.txt");
	Notes << "columns: near from the front (stand_up's end) | near from the front as before, his pack on | near from behind (stand_up's end) | mid from behind (run) | "
			 "near from behind as before, his pack on\n";
	double Now = 1.0;
	for (int R = 0; R < NR; ++R)
	{
		const Row& Rw = Rows[R];
		const Crowd::Dress D = Rw.bPilot ? C.PilotDress(Rw.Nat) : C.CrewDress(Rw.Nat);
		const Soldier* S = C.SoldierFile(D.File);
		if (!S) { std::printf("FAIL: no file for the %s %s\n", Rw.Nat, Rw.bPilot ? "pilot" : "crewman"); return 1; }
		const uint64_t Seed = 0xC47E2ull ^ static_cast<uint64_t>(R + 1) * 0x9E3779B97F4A7C15ull;
		const int Man = C.AddRider(D.File, Rw.Nat, Seed, D.Row, D.bBareHead, D.bNoPack);
		const int Before = C.AddRider(D.File, Rw.Nat, Seed, D.Row, D.bBareHead, false);
		if (Man < 0 || Before < 0) { std::printf("FAIL: no rider\n"); return 1; }
		Notes << "row " << R << ": the " << Rw.Nat << " " << (Rw.bPilot ? "pilot down" : "crewman out of her") << ": soldier_" << D.File << (D.bBareHead ? ", bareheaded" : ", his helmet on")
			  << (!D.bNoPack ? "" : S->PackCount[Near] > 0 ? ", no pack" : ", no pack asked (his file marks none)") << ", no rifle; near " << C.LookOf(Man, Near).Triangles.size() / 3 << " triangles (as before " << C.LookOf(Before, Near).Triangles.size() / 3
			  << "), mid " << C.LookOf(Man, Mid).Triangles.size() / 3 << " (" << C.LookOf(Before, Mid).Triangles.size() / 3 << ")\n";
		for (int K = 0; K < NC; ++K)
		{
			const Col& Cl = Cols[K];
			const int Who = Cl.bBefore ? Before : Man;
			const int Clip = S->ClipIndex(Cl.Clip);
			if (Clip < 0) { std::printf("FAIL: soldier_%s has no %s\n", D.File.c_str(), Cl.Clip); return 1; }
			Crowd::Ride Rd = PackCut::Standing(Clip, Cl.Time);
			Rd.Seat.T[0] = 0.f;
			Rd.Seat.T[1] = 0.f;
			C.SetRide(Who, Rd);
			Now += 0.1;
			C.Update(Now);
			std::vector<float> At(static_cast<size_t>(C.PointsOf(Who, Cl.L)) * 3), Fa(At.size());
			C.Skin(Who, Cl.L, At.data(), Fa.data());
			Scene Sc;
			Sc.AddMan(C.LookOf(Who, Cl.L), At, Fa);
			Sc.AddGround(-4.0, -4.0, 4.0, 4.0, 0.1, Flat);
			// (he faces east: from the front the eye is east of him, from behind west; the sun over the eye's shoulder)
			const double Side = Cl.bBack ? -1.0 : 1.0;
			Light Lt = Daylight(Cl.bBack ? 255.0 : 75.0, 34.0);
			Lt.EarthA = Lin(118, 108, 82);
			Lt.EarthB = Lin(98, 90, 70);
			Lt.HazeK = 0.0;
			const Camera Cam({ 3.3 * Side, 0.85 * Side, 1.25 }, { 0.0, 0.0, 0.88 }, 25.0, (CellW - 4) * 2, (CellH - 4) * 2);
			ShadowMap Sh;
			Sh.Build(Sc, Lt.ToSun, { -2.0, -2.0, -0.1 }, { 2.0, 2.0, 2.0 }, 1536);
			const Picture Pic = Paint(Sc, Cam, Lt, Sh);
			Put(Pic, 1.0, Sheet, SheetW, K * CellW + 2, R * CellH + 2);
			Crowd::Ride Off;
			C.SetRide(Who, Off);
		}
	}
	const std::string OutPath = std::string(Argv[3]) + "/pack_off.ppm";
	if (!WritePpm(OutPath, Sheet, SheetW, CellH * NR)) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
	std::printf("wrote %s\n", OutPath.c_str());
	return 0;
}
