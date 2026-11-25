// The sky's picture (air 3d-handoff.md section 10, "The sky at bomber height"): the clouds of a recorded air battle as
// the scene lays them (Source/TheaterCommand/Battle/AirSky), turned to an eye by the very painter the scene uses, and
// drawn by a small software painter over a plain sea and sky, so that the art director can judge the clouds before
// a Mac build. Not the game's picture: Unreal's lighting, tone curve and haze are not copied, only roughly stood in for
// (the haze from 2 km, thicker low down, toward the battle's inscattering colour).
//
//   Tools/air_show/air_sky_picture <recording.json> <out folder>     (writes <key>-sky-<n>.ppm)
//   AIR_SKY_PICTURES=1 Tools/air_show/run.sh                        (builds it and draws every recording's)
//
// Four eyes: behind the raid as the strategy camera opens (6 km back, 5 km up), low over the sea under the cloud
// base as a torpedo bomber sees it, high over the field looking down on it, and behind the raid's first group where
// the battle opens, far from the target (the clouds lie wherever the fight is); then a deck day from above, and last
// the day's own clouds looked steeply down on from 5 km, 60 degrees down (the sea dark under them). A few dark
// aircraft are drawn at the raid's height for the contrast the art director asked for (a bright cloud floor under
// dark aircraft).

#include "Battle/AirFeed.h"
#include "Battle/AirLand.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using AirSky::Rgb;
	using AirSky::Vec;

	double ToPicture(double L)
	{
		L = std::max(0.0, std::min(1.0, L));
		return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055);
	}

	double Linear(double C255)
	{
		const double C = C255 / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}

	struct Camera
	{
		Vec Eye, Right, Up, Forward;
		double Ppr = 1000.0;
		int W = 1280, H = 720;
		void Look(const Vec& From, const Vec& To, double FovDegrees)
		{
			Eye = From;
			Forward = (To - From).Unit();
			Right = Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit(Vec(1.0, 0.0, 0.0));
			Up = Right.Cross(Forward);
			Ppr = W / (2.0 * std::tan(FovDegrees * 3.14159265358979 / 360.0));
		}
		bool Project(const Vec& P, double& Sx, double& Sy, double& Z) const
		{
			const Vec D = P - Eye;
			Z = D.Dot(Forward);
			if (Z < 1.0) return false;
			Sx = W * 0.5 + D.Dot(Right) / Z * Ppr;
			Sy = H * 0.5 - D.Dot(Up) / Z * Ppr;
			return true;
		}
		Vec Ray(double Sx, double Sy) const
		{
			return (Forward + Right * ((Sx - W * 0.5) / Ppr) + Up * ((H * 0.5 - Sy) / Ppr)).Unit();
		}
	};

	// The haze as the scene sets Unreal's height fog (AirBattle.cpp, LaySky: density 0.00045, height falloff 0.006, from
	// 2 km, at most 0.8), by the engine's own line integral, the fog's height at the sea: thick low down and thin high
	// up, so that looking down from bomber height the sea stays dark (the art director, 19 November 12:59Z) while the
	// horizon and the low air far off are soft. (The engine's scale: its density and falloff a thousandth a unit, 100
	// units a metre.)
	const double HazeCol[3] = { 0.55, 0.66, 0.78 };
	constexpr double HazeDensity = 0.00045 / 1000.0 * 100.0, HazeFalloff = 0.006 / 1000.0 * 100.0, HazeStart = 2000.0, HazeMost = 0.8;
	double HazeOf(const Vec& Eye, const Vec& P)
	{
		const double D = (P - Eye).Length();
		if (D <= HazeStart) return 0.0;
		const Vec Way = (P - Eye) * (1.0 / D);
		const double Z0 = Eye.Z + Way.Z * HazeStart, Rise = P.Z - Z0, Long = D - HazeStart;
		const double X = HazeFalloff * Rise;
		const double Line = std::fabs(X) < 1e-6 ? 1.0 : (1.0 - std::exp2(-X)) / X;
		const double Depth = HazeDensity * std::exp2(-HazeFalloff * Z0) * Long * Line;
		return std::min(HazeMost, 1.0 - std::exp2(-Depth));
	}

	struct Canvas
	{
		int W = 1280, H = 720;
		std::vector<double> Px; // red, green, blue
		void Init(int InW, int InH)
		{
			W = InW;
			H = InH;
			Px.assign(static_cast<size_t>(W) * H * 3, 0.0);
		}
		void Save(const std::string& Path) const
		{
			std::ofstream F(Path, std::ios::binary);
			F << "P6\n" << W << " " << H << "\n255\n";
			for (double V : Px)
			{
				const unsigned char B = static_cast<unsigned char>(ToPicture(V) + 0.5);
				F.write(reinterpret_cast<const char*>(&B), 1);
			}
		}
	};

	// (the sky brighter toward the sun, as the air's haze throws its light forward: the engine's sky does this of itself,
	// the art director on round 18c, eye 8, "the sky brighter toward the sun")
	// (and under a deck the sea in its shade, AirSky::ShadeOf as the key light's light function reads it: the sea's paint is
	// its look in the whole sun, about half of that the sky's light, the rest the sun's)
	void Backdrop(Canvas& C, const Camera& Cam, const Vec& Sun, const AirSky::DeckShade* Shade = nullptr)
	{
		const double Sea[3] = { Linear(38), Linear(62), Linear(78) };
		const double Top[3] = { Linear(120), Linear(150), Linear(190) };
		for (int Y = 0; Y < C.H; ++Y)
			for (int X = 0; X < C.W; ++X)
			{
				const Vec D = Cam.Ray(X + 0.5, Y + 0.5);
				double* Out = &C.Px[(static_cast<size_t>(Y) * C.W + X) * 3];
				if (D.Z < -1e-5)
				{
					const double T = -Cam.Eye.Z / D.Z;
					const Vec P = Cam.Eye + D * T;
					const double F = HazeOf(Cam.Eye, P);
					// (a little swell of light on the water)
					const double Ripple = 0.04 * std::sin(P.X * 0.013 + std::sin(P.Y * 0.007) * 3.0) * std::sin(P.Y * 0.011);
					const double Lit = Shade ? 0.45 + 0.55 * Shade->At(P, Sun) : 1.0;
					for (int K = 0; K < 3; ++K) Out[K] = (Sea[K] * Lit * (1.0 + Ripple)) * (1.0 - F) + HazeCol[K] * F;
				}
				else
				{
					const double Up = std::min(1.0, D.Z * 3.0);
					const double Toward = std::max(0.0, (D.X * Sun.X + D.Y * Sun.Y + D.Z * Sun.Z) / std::max(1e-9, D.Length()));
					const double Glow = 0.18 * std::pow(Toward, 3.0) + 0.3 * std::pow(Toward, 24.0);
					const double Warm[3] = { 1.0, 0.96, 0.86 };
					for (int K = 0; K < 3; ++K) Out[K] = HazeCol[K] + (Top[K] - HazeCol[K]) * Up + Glow * Warm[K];
				}
			}
	}

	struct Corner
	{
		double X, Y;
		double R, G, B, A;
		double F = 0.0;                     // the haze over it
		double U = 0.0, V = 0.0, Behind = 0.0, Ahead = 0.0; // its place on the sheet of shapes, and the sun's way
	};

	// The sheet of cloud shapes as the scene hands it to M_Cloud (AirSky::MakeFragments): its levels, each half the last,
	// read between pixels (the engine's mip levels, read at the level the triangle's size on the picture asks).
	struct Sheet
	{
		std::vector<std::vector<double>> Levels; // four numbers a pixel, 0 to 1
		std::vector<int> Sides;
		bool bOn = false;
		void Make()
		{
			std::vector<uint8_t> Rgba;
			AirSky::MakeFragments(Rgba);
			int Side = AirSky::FragmentCells * AirSky::FragmentSize;
			Levels.emplace_back(Rgba.size());
			for (size_t K = 0; K < Rgba.size(); ++K) Levels[0][K] = Rgba[K] / 255.0;
			Sides.push_back(Side);
			while (Side > 1)
			{
				const int Half = Side / 2;
				const std::vector<double>& From = Levels.back();
				std::vector<double> To(static_cast<size_t>(Half) * Half * 4);
				for (int Y = 0; Y < Half; ++Y)
					for (int X = 0; X < Half; ++X)
						for (int K = 0; K < 4; ++K)
						{
							auto P = [&](int AX, int AY) { return From[(static_cast<size_t>(AY) * Side + AX) * 4 + K]; };
							To[(static_cast<size_t>(Y) * Half + X) * 4 + K] = 0.25 * (P(2 * X, 2 * Y) + P(2 * X + 1, 2 * Y) + P(2 * X, 2 * Y + 1) + P(2 * X + 1, 2 * Y + 1));
						}
				Levels.push_back(std::move(To));
				Sides.push_back(Half);
				Side = Half;
			}
			bOn = true;
		}
		void Read(double U, double V, int Level, double Out[4]) const
		{
			Level = std::max(0, std::min(static_cast<int>(Levels.size()) - 1, Level));
			const int Side = Sides[static_cast<size_t>(Level)];
			const std::vector<double>& P = Levels[static_cast<size_t>(Level)];
			const double X = U * Side - 0.5, Y = V * Side - 0.5;
			const int X0 = static_cast<int>(std::floor(X)), Y0 = static_cast<int>(std::floor(Y));
			const double FX = X - X0, FY = Y - Y0;
			auto At = [&](int AX, int AY, int K) { AX = std::max(0, std::min(Side - 1, AX)); AY = std::max(0, std::min(Side - 1, AY)); return P[(static_cast<size_t>(AY) * Side + AX) * 4 + K]; };
			for (int K = 0; K < 4; ++K)
				Out[K] = (At(X0, Y0, K) * (1.0 - FX) + At(X0 + 1, Y0, K) * FX) * (1.0 - FY) + (At(X0, Y0 + 1, K) * (1.0 - FX) + At(X0 + 1, Y0 + 1, K) * FX) * FY;
		}
	};

	void Triangle(Canvas& C, const Corner& A, const Corner& B, const Corner& D, const Sheet* Shapes = nullptr)
	{
		const double Area = (B.X - A.X) * (D.Y - A.Y) - (B.Y - A.Y) * (D.X - A.X);
		if (std::fabs(Area) < 1e-9) return;
		// (the sheet's level: as many of its pixels to one of the picture's as the triangle covers)
		int Level = 0;
		if (Shapes)
		{
			const double Side = Shapes->Sides[0];
			const double UvArea = std::fabs((B.U - A.U) * (D.V - A.V) - (B.V - A.V) * (D.U - A.U)) * Side * Side;
			Level = UvArea > std::fabs(Area) ? static_cast<int>(std::floor(0.5 * std::log2(UvArea / std::fabs(Area)) + 0.5)) : 0;
		}
		const int X0 = std::max(0, static_cast<int>(std::floor(std::min({ A.X, B.X, D.X }))));
		const int X1 = std::min(C.W - 1, static_cast<int>(std::ceil(std::max({ A.X, B.X, D.X }))));
		const int Y0 = std::max(0, static_cast<int>(std::floor(std::min({ A.Y, B.Y, D.Y }))));
		const int Y1 = std::min(C.H - 1, static_cast<int>(std::ceil(std::max({ A.Y, B.Y, D.Y }))));
		for (int Y = Y0; Y <= Y1; ++Y)
			for (int X = X0; X <= X1; ++X)
			{
				const double PX = X + 0.5, PY = Y + 0.5;
				const double W0 = ((B.X - PX) * (D.Y - PY) - (B.Y - PY) * (D.X - PX)) / Area;
				const double W1 = ((D.X - PX) * (A.Y - PY) - (D.Y - PY) * (A.X - PX)) / Area;
				const double W2 = 1.0 - W0 - W1;
				if (W0 < 0.0 || W1 < 0.0 || W2 < 0.0) continue;
				double Al = A.A * W0 + B.A * W1 + D.A * W2;
				if (Al <= 0.0) continue;
				double R = A.R * W0 + B.R * W1 + D.R * W2, G = A.G * W0 + B.G * W1 + D.G * W2, Bl = A.B * W0 + B.B * W1 + D.B * W2;
				if (Shapes)
				{
					// (M_Cloud: the shape's light, from the side, from behind the eye, from ahead, by the mesh's second
					// texture coordinate, twice the sheet's number; its alpha times the mesh's)
					double S[4];
					Shapes->Read(A.U * W0 + B.U * W1 + D.U * W2, A.V * W0 + B.V * W1 + D.V * W2, Level, S);
					const double Behind = A.Behind * W0 + B.Behind * W1 + D.Behind * W2, Ahead = A.Ahead * W0 + B.Ahead * W1 + D.Ahead * W2;
					const double Light = 2.0 * (S[0] + (S[1] - S[0]) * Behind + (S[2] - S[0]) * Ahead);
					R *= Light;
					G *= Light;
					Bl *= Light;
					Al = std::min(1.0, Al * S[3]);
					if (Al <= 0.0) continue;
				}
				const double F = A.F * W0 + B.F * W1 + D.F * W2;
				R = R * (1.0 - F) + HazeCol[0] * F;
				G = G * (1.0 - F) + HazeCol[1] * F;
				Bl = Bl * (1.0 - F) + HazeCol[2] * F;
				double* Out = &C.Px[(static_cast<size_t>(Y) * C.W + X) * 3];
				Out[0] = Out[0] * (1.0 - Al) + R * Al;
				Out[1] = Out[1] * (1.0 - Al) + G * Al;
				Out[2] = Out[2] * (1.0 - Al) + Bl * Al;
			}
	}

	void Dot(Canvas& C, const Camera& Cam, const Vec& P, double Span)
	{
		double SX, SY, Z;
		if (!Cam.Project(P, SX, SY, Z)) return;
		const double R = std::max(1.0, Span * 0.5 / Z * Cam.Ppr);
		const double F = HazeOf(Cam.Eye, P);
		const double Dark[3] = { Linear(40) * (1.0 - F) + HazeCol[0] * F, Linear(42) * (1.0 - F) + HazeCol[1] * F, Linear(38) * (1.0 - F) + HazeCol[2] * F };
		for (int Y = static_cast<int>(SY - R * 0.25); Y <= static_cast<int>(SY + R * 0.25); ++Y)
			for (int X = static_cast<int>(SX - R); X <= static_cast<int>(SX + R); ++X)
				if (X >= 0 && Y >= 0 && X < C.W && Y < C.H)
					for (int K = 0; K < 3; ++K) C.Px[(static_cast<size_t>(Y) * C.W + X) * 3 + K] = Dark[K];
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: air_sky_picture <recording.json> <out folder>\n");
		return 2;
	}
	std::ifstream In(Argv[1], std::ios::binary);
	std::stringstream Text;
	Text << In.rdbuf();
	AirFeed::Recording Rec;
	std::string Error;
	if (!AirFeed::ReadRecording(Text.str(), Rec, Error))
	{
		std::fprintf(stderr, "%s: %s\n", Argv[1], Error.c_str());
		return 2;
	}
	AirSky::Placing At = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
	// (the sun as the scene has it where AIR_SKY_SUN gives the stage's "sun" as "125,18", its bearing from true north,
	// clockwise, and its height, both in degrees; turned as the stage is turned on the board (AirStage::TowardSun).
	// Without it, the scene's own sun for a battle with no stage: its place's at half past noon where the recording gives
	// its latitude (AAirBattle::TowardSun, AirLand::SunAt), else 30 degrees up on the board's bearing 150)
	{
		double Bearing = 150.0, Up = 30.0, Turn = 0.0;
		const char* Given = std::getenv("AIR_SKY_SUN");
		if (Given)
		{
			std::sscanf(Given, "%lf,%lf", &Bearing, &Up);
			Turn = Rec.Board.StageTurn;
		}
		const double B = Bearing * 3.14159265358979 / 180.0 - Turn, E = Up * 3.14159265358979 / 180.0;
		At.Sun = Vec(std::cos(E) * std::sin(B), std::cos(E) * std::cos(B), std::sin(E));
		if (!Given && Rec.Board.StageKey.empty() && std::isfinite(Rec.Board.Latitude))
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			AirLand::SunAt(Rec.Board.Latitude, Rec.Board.Month + 1, 12.5, Rec.Board.NorthX, Rec.Board.NorthY, X, Y, Z);
			At.Sun = Vec(X, Y, std::max(Z, 0.07)).Unit(At.Sun);
		}
	}
	const AirSky::Field Day = AirSky::Lay(Rec.Board.Cloud, At);
	const double TX = Rec.Board.TargetX, TY = Rec.Board.TargetY;
	// The raid's aircraft from the first view that has them near the target, for scale and contrast.
	// (each drawn at its own height; on the deck day at the band's)
	std::vector<Vec> Raid;
	for (const AirFeed::Frame& F : Rec.Views)
	{
		for (const AirShow::Group& G : F.Groups)
			if (G.Side == 0 && std::hypot(G.X - TX, G.Y - TY) < 9000.0)
				for (int K = 0; K < std::min(G.Figures, 9); ++K)
					Raid.push_back(Vec(G.X + (K % 3 - 1) * 60.0, G.Y + (K / 3 - 1) * 50.0, G.Alt > 100.0 ? G.Alt : 3000.0));
		if (!Raid.empty()) break;
	}
	struct Shot
	{
		const char* Name;
		Vec From, To;
		double Fov;
	};
	// (the raid's first group as the battle opens: where the fight begins, its way toward the target)
	Vec Opens(TX, TY - 20000.0, 4000.0);
	for (const AirShow::Group& G : Rec.Views.front().Groups)
		if (G.Side == 0 && !G.bFighter)
		{
			Opens = Vec(G.X, G.Y, G.Alt > 100.0 ? G.Alt : 3000.0);
			break;
		}
	const Vec Toward = Vec(TX - Opens.X, TY - Opens.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
	const Shot Shots[] = {
		{ "behind the raid", Vec(TX, TY - 6000.0, 5000.0), Vec(TX, TY + 2000.0, 600.0), 55.0 },
		{ "low over the sea", Vec(TX - 6000.0, TY - 5000.0, 120.0), Vec(TX + 4000.0, TY + 3000.0, 700.0), 60.0 },
		{ "high over the field", Vec(TX - 2000.0, TY - 9000.0, 7500.0), Vec(TX + 1000.0, TY + 3000.0, 0.0), 55.0 },
		{ "where the battle opens", Opens - Toward * 3000.0 + Vec(0.0, 0.0, 600.0), Opens + Toward * 6000.0 - Vec(0.0, 0.0, 1500.0), 55.0 },
	};
	// A deck day too, for its look (this battle's own day may be cumulus): base 2,000 m, tops 3,000 m, under a 4,500 m
	// band, seen from 5.5 km behind and above the raid.
	AirSky::Day DeckDay;
	DeckDay.bGiven = true;
	DeckDay.bDeck = true;
	DeckDay.Base = 2000.0;
	DeckDay.Top = 3000.0;
	AirSky::Placing DeckAt = At;
	DeckAt.Band = 4500.0;
	const AirSky::Field Deck = AirSky::Lay(DeckDay, DeckAt);
	const AirSky::DeckShade DayShade = AirSky::ShadeOf(Day), DeckDayShade = AirSky::ShadeOf(Deck);
	AirSky::Painter Paint;
	// (the clouds as drawn from the sheet of shapes, as the scene draws them where M_Cloud is built; AIR_SKY_DISCS=1 the
	// soft discs, as where it is not. The sheet itself is drawn too, over a mid grey, lit from the side)
	Sheet Shapes;
	const char* Discs = std::getenv("AIR_SKY_DISCS");
	if (!(Discs && std::string(Discs) == "1"))
	{
		Shapes.Make();
		Paint.SetSprites(true);
		const int Side = Shapes.Sides[0];
		Canvas Board;
		Board.Init(Side, Side);
		for (size_t K = 0; K < static_cast<size_t>(Side) * Side; ++K)
		{
			const double* P = &Shapes.Levels[0][K * 4];
			for (int J = 0; J < 3; ++J) Board.Px[K * 3 + J] = Linear(96) * (1.0 - P[3]) + std::min(1.0, 2.0 * P[J == 0 ? 0 : J == 1 ? 1 : 2] * 0.85) * P[3];
		}
		Board.Save(std::string(Argv[2]) + "/cloud-sheet.ppm");
	}
	int N = 0;
	// (and the low eye again with the sun straight behind it and straight ahead of it: the art director's eye 2, round
	// 18b, sunlit faces with the sun behind, dark bodies with silver rims with it ahead)
	Vec SunFlat = Vec(At.Sun.X, At.Sun.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
	const Vec Low(TX, TY, 120.0), High(0.0, 0.0, 580.0);
	const Shot SunShots[] = {
		{ "low, the sun behind the eye", Low + SunFlat * 5000.0, Low + SunFlat * -5000.0 + High, 60.0 },
		{ "low, the sun ahead", Low - SunFlat * 5000.0, Low + SunFlat * 5000.0 + High, 60.0 },
	};
	for (int Pass = 0; Pass < 8; ++Pass)
	{
		const Shot S = Pass < 4 ? Shots[Pass] : Pass >= 6 ? SunShots[Pass - 6] : Pass == 4 ? Shot{ "a deck day, from above", Vec(TX, TY - 7000.0, 5500.0), Vec(TX, TY + 3000.0, 2600.0), 55.0 }
		                                              : Shot{ "steeply down from 5 km", Vec(TX - 9000.0, TY - 9000.0, 5000.0), Vec(TX - 9000.0, TY - 9000.0 + 5000.0 / std::tan(60.0 * 3.14159265358979 / 180.0), 0.0), 55.0 };
		const AirSky::Field& Sky = Pass == 4 ? Deck : Day;
		Camera Cam;
		Cam.Look(S.From, S.To, S.Fov);
		Canvas C;
		C.Init(Cam.W, Cam.H);
		const AirSky::DeckShade& Shade = Pass == 4 ? DeckDayShade : DayShade;
		Backdrop(C, Cam, At.Sun, Shade.Side > 0 ? &Shade : nullptr);
		std::vector<Vec> Points, Shadows;
		std::vector<uint8_t> Rgba, ShadowRgba;
		std::vector<float> Uvs;
		// (as the scene lays them: the shadows apart, drawn first, under everything)
		Paint.Paint(Sky, Cam.Eye, Vec(), At.Sun, Rgb(1.0f, 1.0f, 1.0f), Points, Rgba, &Shadows, &ShadowRgba, &Uvs);
		auto DrawAll = [&C, &Cam](const std::vector<Vec>& Pts, const std::vector<uint8_t>& Cols, const std::vector<int>& Tris, const std::vector<float>* Places, const Sheet* From)
		{
			// (one face of each pair: the painter lays both, the software painter needs one)
			for (size_t K = 0; K + 2 < Tris.size(); K += 6)
			{
				Corner V[3];
				bool bIn = true;
				for (int J = 0; J < 3; ++J)
				{
					const size_t I = static_cast<size_t>(Tris[K + static_cast<size_t>(J)]);
					double Z = 0.0;
					if (!Cam.Project(Pts[I], V[J].X, V[J].Y, Z)) { bIn = false; break; }
					V[J].F = HazeOf(Cam.Eye, Pts[I]);
					V[J].R = Cols[I * 4] / 255.0;
					V[J].G = Cols[I * 4 + 1] / 255.0;
					V[J].B = Cols[I * 4 + 2] / 255.0;
					V[J].A = Cols[I * 4 + 3] / 255.0;
					if (Places && Places->size() >= (I + 1) * 4)
					{
						V[J].U = (*Places)[I * 4];
						V[J].V = (*Places)[I * 4 + 1];
						V[J].Behind = (*Places)[I * 4 + 2];
						V[J].Ahead = (*Places)[I * 4 + 3];
					}
				}
				if (bIn) Triangle(C, V[0], V[1], V[2], From);
			}
		};
		DrawAll(Shadows, ShadowRgba, Paint.Triangles(AirSky::Painter::MostShadows), nullptr, nullptr);
		DrawAll(Points, Rgba, Paint.Triangles(Sky.MostPuffs), Shapes.bOn ? &Uvs : nullptr, Shapes.bOn ? &Shapes : nullptr);
		for (const Vec& P : Raid) Dot(C, Cam, Pass != 4 ? P : Vec(P.X, P.Y, 4500.0), 14.0);
		const std::string Path = std::string(Argv[2]) + "/" + Rec.Key + "-sky-" + std::to_string(++N) + ".ppm";
		C.Save(Path);
		std::printf("%s: %s (%s, from %.0f to %.0f m, %zu clouds, %d puffs and %d shadows drawn) -> %s\n", Rec.Key.c_str(), S.Name, Sky.Sky.bDeck ? "deck" : "cumulus", Sky.Base, Sky.Top,
		            Sky.Clouds.size(), Paint.Drawn(), Paint.ShadowsDrawn(), Path.c_str());
	}
	return 0;
}
