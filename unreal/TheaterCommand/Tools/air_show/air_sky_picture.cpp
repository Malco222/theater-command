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
// the battle opens, far from the target (the clouds lie wherever the fight is); then a deck day from above. A few
// dark aircraft are drawn at the raid's height for the contrast the art director asked for (a bright cloud floor
// under dark aircraft).

#include "Battle/AirFeed.h"
#include "Battle/AirSky.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

	// The haze, roughly as the scene sets it: none within 2 km, at most 0.8, thicker low down.
	const double HazeCol[3] = { 0.55, 0.66, 0.78 };
	double HazeOf(const Vec& Eye, const Vec& P)
	{
		const double D = (P - Eye).Length();
		const double Low = std::exp(-std::min(Eye.Z, P.Z) / 3000.0) * 0.6 + 0.4;
		return 0.8 * (1.0 - std::exp(-std::max(0.0, D - 2000.0) / 30000.0 * Low));
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

	void Backdrop(Canvas& C, const Camera& Cam)
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
					for (int K = 0; K < 3; ++K) Out[K] = (Sea[K] * (1.0 + Ripple)) * (1.0 - F) + HazeCol[K] * F;
				}
				else
				{
					const double Up = std::min(1.0, D.Z * 3.0);
					for (int K = 0; K < 3; ++K) Out[K] = HazeCol[K] + (Top[K] - HazeCol[K]) * Up;
				}
			}
	}

	struct Corner
	{
		double X, Y;
		double R, G, B, A;
	};

	void Triangle(Canvas& C, const Corner& A, const Corner& B, const Corner& D)
	{
		const double Area = (B.X - A.X) * (D.Y - A.Y) - (B.Y - A.Y) * (D.X - A.X);
		if (std::fabs(Area) < 1e-9) return;
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
				const double Al = A.A * W0 + B.A * W1 + D.A * W2;
				if (Al <= 0.0) continue;
				double* Out = &C.Px[(static_cast<size_t>(Y) * C.W + X) * 3];
				Out[0] = Out[0] * (1.0 - Al) + (A.R * W0 + B.R * W1 + D.R * W2) * Al;
				Out[1] = Out[1] * (1.0 - Al) + (A.G * W0 + B.G * W1 + D.G * W2) * Al;
				Out[2] = Out[2] * (1.0 - Al) + (A.B * W0 + B.B * W1 + D.B * W2) * Al;
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
	const AirSky::Placing At = AirFeed::PlacingOf(Rec.Board, Rec.Views.front().Groups, Rec.Seed);
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
	AirSky::Painter Paint;
	int N = 0;
	for (int Pass = 0; Pass < 5; ++Pass)
	{
		const Shot S = Pass < 4 ? Shots[Pass] : Shot{ "a deck day, from above", Vec(TX, TY - 7000.0, 5500.0), Vec(TX, TY + 3000.0, 2600.0), 55.0 };
		const AirSky::Field& Sky = Pass < 4 ? Day : Deck;
		Camera Cam;
		Cam.Look(S.From, S.To, S.Fov);
		Canvas C;
		C.Init(Cam.W, Cam.H);
		Backdrop(C, Cam);
		std::vector<Vec> Points;
		std::vector<uint8_t> Rgba;
		Paint.Paint(Sky, Cam.Eye, Vec(), At.Sun, Rgb(1.0f, 1.0f, 1.0f), Points, Rgba);
		const std::vector<int> Tris = Paint.Triangles(Sky.MostPuffs);
		// (one face of each pair: the painter lays both, the software painter needs one)
		for (size_t K = 0; K + 2 < Tris.size(); K += 6)
		{
			Corner V[3];
			bool bIn = true;
			for (int J = 0; J < 3; ++J)
			{
				const size_t I = static_cast<size_t>(Tris[K + static_cast<size_t>(J)]);
				double Z = 0.0;
				if (!Cam.Project(Points[I], V[J].X, V[J].Y, Z)) { bIn = false; break; }
				const double F = HazeOf(Cam.Eye, Points[I]);
				V[J].R = (Rgba[I * 4] / 255.0) * (1.0 - F) + HazeCol[0] * F;
				V[J].G = (Rgba[I * 4 + 1] / 255.0) * (1.0 - F) + HazeCol[1] * F;
				V[J].B = (Rgba[I * 4 + 2] / 255.0) * (1.0 - F) + HazeCol[2] * F;
				V[J].A = Rgba[I * 4 + 3] / 255.0;
			}
			if (bIn) Triangle(C, V[0], V[1], V[2]);
		}
		for (const Vec& P : Raid) Dot(C, Cam, Pass < 4 ? P : Vec(P.X, P.Y, 4500.0), 14.0);
		const std::string Path = std::string(Argv[2]) + "/" + Rec.Key + "-sky-" + std::to_string(++N) + ".ppm";
		C.Save(Path);
		std::printf("%s: %s (%s, from %.0f to %.0f m, %zu clouds, %d puffs drawn) -> %s\n", Rec.Key.c_str(), S.Name, Sky.Sky.bDeck ? "deck" : "cumulus", Sky.Base, Sky.Top,
		            Sky.Clouds.size(), Paint.Drawn(), Path.c_str());
	}
	return 0;
}
