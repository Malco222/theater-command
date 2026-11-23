// The bail clips' picture (Effects, release 2, -TheaterLandBailOut): a tank's crewman bailing out in the soldiers'
// clips as the scene plays them (LandBail::PoseAt laid on a rider's seat, as ALandBattle::BailOut does), and a man on
// fire running and rolling. Her hull and turret are drawn as raised ground (the picture draws men and ground only), the
// camera side-on from the side he drops from. For the art director's look before the Mac's.
// Usage: Tools/land_bail/sheet.sh <soldiers folder with the bail clips> <armies.json> <out folder>
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include "Battle/LandBail.h"

namespace
{
	// (her hull 3.0 m wide, 5.9 m long, 1.9 m high, facing north; her turret's roof 2.85 m up, 2.0 m wide and 2.3 m
	// long, a little back of her middle)
	double HullAt(double X, double Y)
	{
		if (std::abs(X) <= 1.0 && Y >= -1.45 && Y <= 0.85) return 2.85;
		if (std::abs(X) <= 1.5 && std::abs(Y) <= 2.95) return 1.9;
		return 0.0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: bail_sheet <soldiers folder> <armies.json> <out folder>\n");
		return 2;
	}
	std::string Err;
	auto Inf = std::make_shared<Soldier>(), Tk = std::make_shared<Soldier>();
	if (!Inf->Load(ReadFile(std::string(Argv[1]) + "/soldier_USA.json"), Err)) { std::printf("soldier_USA: %s\n", Err.c_str()); return 1; }
	if (!Tk->Load(ReadFile(std::string(Argv[1]) + "/soldier_USA_tanker.json"), Err)) { std::printf("soldier_USA_tanker: %s\n", Err.c_str()); return 1; }
	for (const char* Name : { "bail_climb_out", "bail_step_down", "bail_jump_down", "burning_run", "burning_fall_roll" })
	{
		if (Tk->ClipIndex(Name) < 0) { std::printf("FAIL: soldier_USA_tanker has no %s\n", Name); return 1; }
	}
	Crowd C;
	C.AddSoldier("USA", Inf);
	C.AddSoldier("USA_tanker", Tk);
	const std::string Rec = "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		"\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\"}],"
		"\"men\": [{\"b\": 1, \"k\": 1, \"r\": \"rifle\", \"at\": [[0, 500, 500, 0], [10, 500, 500, 0]], \"ch\": [[0, \"wait\", \"none\", 0, 0, 0, 0]]}],"
		"\"rounds\": []}";
	const GroundFn Flat = [](double, double) { return Ground(); };
	if (!C.ReadArmies(ReadFile(Argv[2]), Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); return 1; }
	const int Man = C.AddRider("USA_tanker", "USA", 0xBA11ull);
	if (Man < 0) { std::printf("FAIL: no rider\n"); return 1; }
	C.SetEye(14.0, 0.0, 2.0);

	// Her crew, with the clips: the commander first out of her turret, dropping off her right side.
	LandBail::Wreck W;
	W.Middle = LandBail::V3{ 0.0, 0.0, 0.0 };
	W.Heading = 0.0;
	W.Wide = 3.0;
	W.Long = 5.9;
	W.Exits.push_back(LandBail::V3{ 0.5, -0.95, 2.85 }); // (his cupola near her turret's back)
	W.Away = 3.14159265358979;
	W.bClips = true;
	W.bStep = true;
	W.TurretHalf = 1.0;
	W.bUnderFire = false;
	W.Ground = [](double, double) { return 0.0; };
	LandBail::Man M;
	M.Side = 1;
	M.Out = 0.0;
	M.Hatch = W.Exits[0];
	LandBail::LayWayOff(W, M); // (over her turret's roof, down onto her deck, off her hull, as the scene lays him)
	std::printf("  his way: walks %.2f s over her roof to (%.2f, %.2f), steps down to (%.2f, %.2f, %.2f), walks %.2f s, jumps from (%.2f, %.2f, %.2f) to (%.2f, %.2f); %.2f s to the ground\n",
		M.RoofS, M.Brink.X, M.Brink.Y, M.Step.X, M.Step.Y, M.Step.Z, M.DeckS, M.Deck.X, M.Deck.Y, M.Deck.Z, M.Down.X, M.Down.Y, LandBail::OnGroundOf(M));
	if (!M.bStep) { std::printf("FAIL: he does not step down off her turret\n"); return 1; }
	// (and off her back, his side blocked by a vehicle close beside her: over her turret's back onto her engine deck)
	LandBail::Man B;
	B.Side = 1;
	B.OffEnd = -1;
	B.Out = 0.0;
	B.Hatch = W.Exits[0];
	LandBail::LayWayOff(W, B);
	std::printf("  off her back: %.2f s over her roof to (%.2f, %.2f), steps down to (%.2f, %.2f, %.2f), %.2f s on, jumps from (%.2f, %.2f) to (%.2f, %.2f)\n",
		B.RoofS, B.Brink.X, B.Brink.Y, B.Step.X, B.Step.Y, B.Step.Z, B.DeckS, B.Deck.X, B.Deck.Y, B.Down.X, B.Down.Y);
	if (!B.bStep) { std::printf("FAIL: he does not step down off her turret's back\n"); return 1; }
	B.Lee = LandBail::V3{ B.Down.X, B.Down.Y - 6.0, 0.0 };
	B.Goal = LandBail::V3{ B.Down.X + 2.0, B.Down.Y - 20.0, 0.0 };
	B.Speed = 3.8;
	const double BRoof = 1.5 + B.RoofS, BJump = BRoof + 0.7333 + B.DeckS, BGround = LandBail::OnGroundOf(B);
	M.Lee = LandBail::V3{ 3.6, -9.0, 0.0 };
	M.Goal = LandBail::V3{ 6.0, -22.0, 0.0 };
	M.Speed = 3.8;
	const double OnGround = LandBail::OnGroundOf(M);
	const double OnRoof = 1.5 + M.RoofS, Jump = OnRoof + 0.7333 + M.DeckS;
	// (a man on fire: out the same way, running 2.5 s, then the roll)
	LandBail::Man F = M;
	F.How = LandBail::End::Burning;
	F.FallAt = OnGround + 2.2;
	F.FireOut = F.FallAt + 3.0;
	F.bLives = true;
	const std::vector<std::pair<const LandBail::Man*, std::vector<double>>> Rows = {
		{ &M, { 0.2, 0.7, 1.1, 1.5 + 0.5 * M.RoofS, OnRoof + 0.1, OnRoof + 0.35, OnRoof + 0.6, Jump + 0.3, Jump + 0.75, Jump + 1.1, OnGround + 0.4 } },
		{ &M, { 0.2, 0.7, 1.1, 1.5 + 0.5 * M.RoofS, OnRoof + 0.1, OnRoof + 0.35, OnRoof + 0.6, Jump + 0.3, Jump + 0.75, Jump + 1.1, OnGround + 0.4 } },
		{ &B, { 0.7, 1.1, 1.5 + 0.5 * B.RoofS, BRoof + 0.1, BRoof + 0.35, BRoof + 0.6, BJump - 0.5 * B.DeckS, BJump + 0.3, BJump + 0.75, BJump + 1.1, BGround + 0.4 } },
		{ &F, { OnGround + 0.6, OnGround + 1.0, OnGround + 1.4, OnGround + 1.8, F.FallAt + 0.1, F.FallAt + 0.6, F.FallAt + 1.1, F.FallAt + 1.8, F.FallAt + 2.6, F.FallAt + 3.2, F.FallAt + 3.6 } },
	};
	const int Cols = 11, CellH = 240, SheetW = 2640;
	std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * static_cast<int>(Rows.size())) * 3, 236);
	Light Lt = Daylight(215.0, 24.0);
	Lt.EarthA = Lin(118, 98, 76);
	Lt.EarthB = Lin(96, 80, 62);
	Lt.HazeK = 0.0;
	const GroundFn Hull = [](double X, double Y) { Ground G; G.Height = static_cast<float>(HullAt(X, Y)); return G; };
	int Bad = 0;
	double Now = 1.0;
	for (size_t Row = 0; Row < Rows.size(); ++Row)
	{
		for (int Col = 0; Col < Cols; ++Col)
		{
			const double T = Rows[Row].second[static_cast<size_t>(Col)];
			const LandBail::Pose P = LandBail::PoseAt(*Rows[Row].first, T, W.Ground);
			Crowd::Ride R;
			R.bShown = P.bShown;
			R.Clip = Tk->ClipIndex(P.Clip);
			R.Time = static_cast<float>(P.Time);
			R.bUnarmed = true;
			R.bOwnRoot = true;
			const double Fx = std::sin(P.Facing), Fy = std::cos(P.Facing);
			float* Rm = R.Seat.R;
			Rm[0] = static_cast<float>(Fx); Rm[1] = static_cast<float>(Fy); Rm[2] = 0.f;
			Rm[3] = static_cast<float>(Fy); Rm[4] = static_cast<float>(-Fx); Rm[5] = 0.f;
			Rm[6] = 0.f; Rm[7] = 0.f; Rm[8] = 1.f;
			R.Seat.T[0] = static_cast<float>(P.At.X);
			R.Seat.T[1] = static_cast<float>(P.At.Y);
			R.Seat.T[2] = static_cast<float>(P.At.Z);
			if (R.Clip < 0) { std::printf("FAIL: no clip %s\n", P.Clip.c_str()); ++Bad; continue; }
			C.SetRide(Man, R);
			Now += 0.1;
			C.Update(Now);
			std::vector<float> At, Fa;
			Skinned(C, Man, At, Fa);
			// (his feet: the balls of his feet against what is under them)
			double Feet = 1e9;
			for (const char* Ball : { "ball_l", "ball_r" })
			{
				const int Bi = Tk->BoneIndex(Ball);
				if (Bi < 0) continue;
				const Vec3 Pt = C.FramesOf(Man)[Bi].Point(Tk->Bones[static_cast<size_t>(Bi)].Head);
				Feet = std::min(Feet, static_cast<double>(Pt.Z) - HullAt(static_cast<double>(Pt.X), static_cast<double>(Pt.Y)));
			}
			std::printf("  row %zu, %.2f s: %s at %.2f s, seat (%.2f, %.2f, %.2f), body (%.2f, %.2f, %.2f), feet %.2f m up\n", Row, T,
				P.Clip.c_str(), P.Time, P.At.X, P.At.Y, P.At.Z, P.Root.X, P.Root.Y, P.Root.Z, Feet);
			Scene Sc;
			Sc.AddMan(C.LookOf(Man, Near), At, Fa);
			Sc.AddGround(-5.0, -10.0, 9.0, 5.0, 0.1, Hull);
			const int OX = (SheetW * Col) / Cols, OX1 = (SheetW * (Col + 1)) / Cols;
			const int CW = OX1 - OX - 4, CH = CellH - 4;
			// (from the side he drops from, three-quarters on; then from her front, her turret, deck and hull's edge
			// as steps; off her back seen side-on; the man on fire followed off along his way, her hull behind him)
			const Camera Cam = Row == 0 ? Camera({ 9.5, 7.5, 3.6 }, { 1.6, -0.3, 1.7 }, 26.0, CW * 2, CH * 2)
				: Row == 1 ? Camera({ 1.9, 11.0, 2.2 }, { 1.9, -0.3, 1.7 }, 24.0, CW * 2, CH * 2)
				: Row == 2 ? Camera({ 12.5, -3.4, 2.4 }, { 0.5, -3.4, 1.7 }, 26.0, CW * 2, CH * 2)
				: Camera({ 13.0, -5.0, 2.2 }, { 2.6, -6.5, 0.7 }, 34.0, CW * 2, CH * 2);
			ShadowMap Sh;
			Sh.Build(Sc, Lt.ToSun, { -4.0, -9.0, -0.1 }, { 8.0, 4.0, 4.0 }, 1536);
			const Picture Pic = Paint(Sc, Cam, Lt, Sh);
			Put(Pic, 1.0, Sheet, SheetW, OX + 2, static_cast<int>(Row) * CellH + 2);
		}
	}
	const std::string OutPath = std::string(Argv[3]) + "/bail_clips.ppm";
	if (!WritePpm(OutPath, Sheet, SheetW, CellH * static_cast<int>(Rows.size()))) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
	std::printf("wrote %s\n", OutPath.c_str());
	return Bad ? 1 : 0;
}
