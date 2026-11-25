// The men's worn look before and after, for the art director (release 2, behind tc.Look.Worn: LandMen::Crowd::SetWearMix):
// the American rifleman as the crowd draws him, by land_men_picture's painter (its light and ground are stand-ins; it
// draws no roughness, so the last column paints it as grey). Columns: OFF (as today); ON on Normandy's own soil; ON on
// the desert's sand (221, 210, 180, the art director's sample); ON in rain on Normandy's soil; a fresh replacement on
// Normandy's soil (W 0.1); the roughness the ON look writes into his points' alpha, as grey (pale rough, dark sheen).
// The ON columns are a man six weeks or more in the line (W 1). Rows: near from the front, near from behind, his legs
// close from the front. worn.ppm, and worn.txt (the soils and what each column is).
//
// Normandy's soil is the game's own reading of the field (WholeLook::FieldDust of the field's mean paint), made here
// without the engine: the field laid from the recording as the game lays it (LandSight::FieldOf, LandField::Ground,
// without a region's kit models or the trial's words), its mean paint on the same 24 by 24 grid with water not
// counted (WholeLook::GroundMean), and the hue held in the same earth box (WholeLook::FieldDust; a copy of its few lines
// for this picture alone, as WholeLook needs the engine). LAND_WORN_SOIL="r g b" (sRGB bytes) paints with another, such
// as the value the game logs ("Whole look: worn ON for the men: boots and knees take ...").
// Usage: Tools/land_show/worn_picture.sh [out folder]
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include "pack_cut.h"

#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandSight.h"

namespace
{
	double PictureOf(double Light) { Light = std::max(Light, 0.0); return Light <= 0.0031308 ? 12.92 * Light : 1.055 * std::pow(Light, 1.0 / 2.4) - 0.055; }
	double LightOf(double Pic) { Pic = std::max(Pic, 0.0); return Pic <= 0.04045 ? Pic / 12.92 : std::pow((Pic + 0.055) / 1.055, 2.4); }

	/** Normandy's soil as the game reads it (see the top): sRGB 0 to 1 into Out; false, and why, where it cannot be read
	 * (then the files' own dust stands in). */
	bool FieldSoil(const std::string& RecPath, double* Out, std::string& Said)
	{
		LandFeed::Recording Rec;
		std::string Err;
		if (!LandFeed::ReadRecording(ReadFile(RecPath), Rec, Err)) { Said = "the recording: " + Err; return false; }
		const LandField::Field F = LandSight::FieldOf(Rec);
		const LandField::Ground G(F);
		// (WholeLook::GroundMean on the painted field's own size, as LandBattle's FieldMeanPaint asks it)
		const int Across = 24;
		const double Wide = Rec.Ground.Cols * Rec.Ground.Cell, Deep = Rec.Ground.Rows * Rec.Ground.Cell;
		double Sum[3] = { 0, 0, 0 };
		int Count = 0;
		for (int Row = 0; Row < Across; ++Row)
		{
			for (int Col = 0; Col < Across; ++Col)
			{
				const double X = (Col + 0.5) / Across * Wide, Y = (Row + 0.5) / Across * Deep;
				LandField::Drawn What = LandField::Drawn::Pasture;
				G.HeightAt(X, Y, &What);
				if (What == LandField::Drawn::Water) continue;
				const LandField::Rgb8 C = G.ColourAt(X, Y);
				Sum[0] += LightOf(C.R / 255.0);
				Sum[1] += LightOf(C.G / 255.0);
				Sum[2] += LightOf(C.B / 255.0);
				++Count;
			}
		}
		if (Count == 0) { Said = "every point of the field is water"; return false; }
		const double Mean[3] = { Sum[0] / Count, Sum[1] / Count, Sum[2] / Count };
		// (WholeLook::FieldDust: Boxed with red/green 1.00 to 1.25, blue/green 0.70 to 0.90, read as a picture keeps it,
		// the value kept)
		const double R = PictureOf(Mean[0]), Gr = PictureOf(Mean[1]), B = PictureOf(Mean[2]);
		double Dust[3] = { LightOf(Gr * std::clamp(R / Gr, 1.00, 1.25)), LightOf(Gr), LightOf(Gr * std::clamp(B / Gr, 0.70, 0.90)) };
		const auto Value = [](const double* L) { return 0.2126 * L[0] + 0.7152 * L[1] + 0.0722 * L[2]; };
		const double Was = Value(Mean), Now = Value(Dust);
		for (double& D : Dust) D *= Now > 1e-6 ? Was / Now : 1.0;
		const double Snow = G.SnowLying();
		char Line[256];
		std::snprintf(Line, sizeof(Line), "%s: the field's mean paint %.0f, %.0f, %.0f (sRGB), %d of %d points not water, snow lying %.2f", Rec.Key.c_str(), 255.0 * R, 255.0 * Gr, 255.0 * B, Count,
			Across * Across, Snow);
		Said = Line;
		if (Snow >= 0.25) { Said += " (snow: the files' own dust)"; return false; }
		for (int K = 0; K < 3; ++K) Out[K] = std::round(255.0 * PictureOf(Dust[K])) / 255.0; // (as the game's ToFColor(true))
		return true;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 5)
	{
		std::fprintf(stderr, "usage: worn_picture <soldiers folder> <armies.json> <normandy recording> <out folder>\n");
		return 2;
	}
	const std::string OutDir = Argv[4];
	std::string Err;
	// (read with its wear for the ON columns, and as the game reads it with the switch off for the OFF one)
	auto Worn = std::make_shared<Soldier>(), Plain = std::make_shared<Soldier>();
	const std::string Text = ReadFile(std::string(Argv[1]) + "/soldier_USA.json");
	if (!Worn->Load(Text, Err, true) || !Plain->Load(Text, Err)) { std::printf("soldier_USA: %s\n", Err.c_str()); return 1; }
	const std::string Armies = ReadFile(Argv[2]);

	double Normandy[3] = { 0.60, 0.54, 0.43 };
	std::string Said;
	const bool bField = FieldSoil(Argv[3], Normandy, Said);
	if (const char* V = std::getenv("LAND_WORN_SOIL"))
	{
		int R = 0, G = 0, B = 0;
		if (std::sscanf(V, "%d %d %d", &R, &G, &B) == 3)
		{
			Normandy[0] = R / 255.0;
			Normandy[1] = G / 255.0;
			Normandy[2] = B / 255.0;
			Said += " (LAND_WORN_SOIL stands in)";
		}
	}
	const double Sand[3] = { 221.0 / 255.0, 210.0 / 255.0, 180.0 / 255.0 };
	std::printf("%s; Normandy's soil %.0f, %.0f, %.0f%s\n", Said.c_str(), 255.0 * Normandy[0], 255.0 * Normandy[1], 255.0 * Normandy[2], bField ? "" : " (the files' own dust)");

	struct Col
	{
		const char* Name;
		bool bOn;
		const double* Soil;
		double W, Rain;
		bool bRough;
	};
	const Col Cols[] = { { "OFF (as today)", false, Normandy, 0.6, 0.0, false }, { "ON, Normandy's soil, W 1", true, Normandy, 1.0, 0.0, false },
		{ "ON, desert sand, W 1", true, Sand, 1.0, 0.0, false }, { "ON, rain, Normandy, W 1", true, Normandy, 1.0, 1.0, false },
		{ "fresh (W 0.1), Normandy", true, Normandy, 0.1, 0.0, false }, { "roughness (ON, Normandy, W 1)", true, Normandy, 1.0, 0.0, true } };
	struct Row
	{
		const char* Name;
		D3 Eye, At;
		double Fov, SunAz;
	};
	// (he faces east, x: from the front the eye is east of him; the sun over the eye's shoulder)
	const Row Rows[] = { { "near, front", { 3.3, 0.85, 1.25 }, { 0.0, 0.0, 0.88 }, 25.0, 75.0 }, { "near, behind", { -3.3, -0.85, 1.25 }, { 0.0, 0.0, 0.88 }, 25.0, 255.0 },
		{ "legs, close", { 1.55, 0.45, 0.62 }, { 0.0, 0.0, 0.45 }, 25.0, 75.0 } };
	const int NC = static_cast<int>(sizeof(Cols) / sizeof(Cols[0])), NR = static_cast<int>(sizeof(Rows) / sizeof(Rows[0])), CellW = 330, CellH = 540, SheetW = CellW * NC;
	std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * NR) * 3, 236);
	std::ofstream Notes(OutDir + "/worn.txt");
	Notes << "soil: " << Said << "; Normandy's " << std::lround(255.0 * Normandy[0]) << ", " << std::lround(255.0 * Normandy[1]) << ", " << std::lround(255.0 * Normandy[2])
		  << (bField ? "" : " (the files' own dust)") << "; the desert's 221, 210, 180\n";
	const std::string Rec = "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, \"blocks\": [], \"men\": [], \"rounds\": []}";
	const GroundFn Flat = [](double, double) { return Ground(); };
	for (int K = 0; K < NC; ++K)
	{
		const Col& Cl = Cols[K];
		Crowd C;
		if (Cl.bOn)
		{
			Crowd::WearMix Mix;
			Mix.bOn = true;
			Mix.bRough = true;
			for (int A = 0; A < 3; ++A) Mix.Soil[A] = static_cast<float>(Cl.Soil[A]);
			Mix.Rain = static_cast<float>(Cl.Rain);
			Mix.W = static_cast<float>(Cl.W);
			C.SetWearMix(Mix);
		}
		C.AddSoldier("USA", Cl.bOn ? Worn : Plain);
		if (!C.ReadArmies(Armies, Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); return 1; }
		C.SetEye(6.0, 0.0, 1.6);
		const int Man = C.AddRider("USA", "USA", 0xA11CE);
		const Soldier& S = Cl.bOn ? *Worn : *Plain;
		const int Clip = S.ClipIndex("stand_up");
		if (Man < 0 || Clip < 0) { std::printf("FAIL: no rider, or no stand_up\n"); return 1; }
		Crowd::Ride Rd = PackCut::Standing(Clip, 0.6f);
		Rd.bUnarmed = false;
		Rd.Seat.T[0] = 0.f;
		Rd.Seat.T[1] = 0.f;
		C.SetRide(Man, Rd);
		C.Update(1.0);
		std::vector<float> At(static_cast<size_t>(C.PointsOf(Man, Near)) * 3), Fa(At.size());
		C.Skin(Man, Near, At.data(), Fa.data());
		Look Lk = C.LookOf(Man, Near);
		if (Cl.bRough)
		{
			// (the alpha the mesh builders write, byte over 255, as grey: 0.3 (glass) to 1 spread over 20 to 211 of 255,
			// so the cloth's 0.9 is pale and the skin's 0.55, the leather's 0.45 and the steel's 0.42 darker)
			for (size_t P = 0; P < static_cast<size_t>(Lk.Points); ++P)
			{
				const double Rough = Lk.Alpha(P) / 255.0;
				const uint8_t Grey = static_cast<uint8_t>(std::lround(20.0 + 191.0 * std::clamp((Rough - 0.3) / 0.7, 0.0, 1.0)));
				for (size_t A = 0; A < 3; ++A) Lk.Rgb[P * 3 + A] = Grey;
			}
		}
		Notes << "column " << K << ": " << Cl.Name << "; " << (Cl.bOn ? "the mix set" : "the mix never set, his file read without its wear") << "; ground painted " << std::lround(255.0 * Cl.Soil[0]) << ", "
			  << std::lround(255.0 * Cl.Soil[1]) << ", " << std::lround(255.0 * Cl.Soil[2]) << "\n";
		for (int R = 0; R < NR; ++R)
		{
			const Row& Rw = Rows[R];
			Scene Sc;
			Sc.AddMan(Lk, At, Fa);
			Sc.AddGround(-4.0, -4.0, 4.0, 4.0, 0.1, Flat);
			Light Lt = Daylight(Rw.SunAz, 34.0);
			Lt.EarthA = Lin(static_cast<int>(std::lround(255.0 * Cl.Soil[0])), static_cast<int>(std::lround(255.0 * Cl.Soil[1])), static_cast<int>(std::lround(255.0 * Cl.Soil[2])));
			Lt.EarthB = Lt.EarthA * 0.82;
			Lt.HazeK = 0.0;
			const Camera Cam(Rw.Eye, Rw.At, Rw.Fov, (CellW - 4) * 2, (CellH - 4) * 2);
			ShadowMap Sh;
			Sh.Build(Sc, Lt.ToSun, { -2.0, -2.0, -0.1 }, { 2.0, 2.0, 2.0 }, 1536);
			const Picture Pic = Paint(Sc, Cam, Lt, Sh);
			Put(Pic, 1.0, Sheet, SheetW, K * CellW + 2, R * CellH + 2);
		}
	}
	for (int R = 0; R < NR; ++R) Notes << "row " << R << ": " << Rows[R].Name << "\n";
	const std::string OutPath = OutDir + "/worn.ppm";
	if (!WritePpm(OutPath, Sheet, SheetW, CellH * NR)) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
	std::printf("wrote %s\n", OutPath.c_str());
	return 0;
}
