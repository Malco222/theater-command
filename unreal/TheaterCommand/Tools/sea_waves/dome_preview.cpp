// Theater Command: preview pictures of the sea's sky dome for the art director (the release 2 sea weather).
//
// The painter's own dome (SeaSky::PaintDome) of a day asked as -TheaterSeaWeather= asks it, from an eye over the board's
// middle, looked at through a 60 degree lens: toward the horizon (pitched up 10 degrees) and up into the sky (60
// degrees), each over a plain blue sky and a dark sea. Not the game's frame (no M_SkyDome, no far sheet, no haze, no
// sea): what the clouds paint and where the gaps are, for a first look before the Mac's stills.
//
// Not part of a suite. Build from the project (TheaterCommand), on one line:
//   S=Source/TheaterCommand; g++ -std=c++20 -O2 -I$S -o /tmp/dome_preview Tools/sea_waves/dome_preview.cpp
//     $S/Battle/SeaSky.cpp $S/Battle/AirSky.cpp $S/Battle/AirFeed.cpp $S/Battle/AirShow.cpp $S/Battle/AirDesk.cpp
//     $S/Battle/AirGround.cpp $S/Battle/ShipGuns.cpp $S/Effects/TheaterFxCore.cpp
//   /tmp/dome_preview <cumulus|deck> <cover> <eye height, m, or "overview"> <out.ppm> [seed]
// It writes one picture, the two views side by side, and prints the day as the sea chose it.

#include "Battle/AirSky.h"
#include "Battle/SeaSky.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	using AirSky::Vec;
	constexpr double Pi = 3.14159265358979323846;
	constexpr double MiddleX = 10000.0, MiddleY = 10000.0;
	constexpr int ViewW = 960, ViewH = 600;

	Vec Cross(const Vec& A, const Vec& B)
	{
		return Vec(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}

	double Encode(double Linear)
	{
		const double C = std::clamp(Linear, 0.0, 1.0);
		return C <= 0.0031308 ? 12.92 * C : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055;
	}

	/** The dome's colour and alpha at a way from the eye, between its pixels (the columns coming round). */
	void Sample(const SeaSky::Dome& D, const Vec& Way, double Out[4])
	{
		double Column = 0.0, Row = 0.0;
		SeaSky::PlaceOf(Way, D.W, D.H, Column, Row);
		const double U = Column - 0.5, V = std::clamp(Row - 0.5, 0.0, static_cast<double>(D.H - 1));
		const int X0 = static_cast<int>(std::floor(U)), Y0 = static_cast<int>(std::floor(V));
		const double FX = U - X0, FY = V - Y0;
		for (int K = 0; K < 4; ++K)
		{
			Out[K] = 0.0;
		}
		for (int J = 0; J < 2; ++J)
		{
			for (int I = 0; I < 2; ++I)
			{
				const int X = ((X0 + I) % D.W + D.W) % D.W, Y = std::min(D.H - 1, Y0 + J);
				const double Weight = (I ? FX : 1.0 - FX) * (J ? FY : 1.0 - FY);
				for (int K = 0; K < 4; ++K)
				{
					Out[K] += Weight * D.Rgba[(static_cast<size_t>(Y) * D.W + X) * 4 + K] / 255.0;
				}
			}
		}
	}

	/** One view through the lens into the picture at its left edge Left. */
	void View(const SeaSky::Dome& D, double Bearing, double Pitch, std::vector<uint8_t>& Picture, int PictureW, int Left)
	{
		const double B = Bearing * Pi / 180.0, P = Pitch * Pi / 180.0;
		const Vec Ahead(std::sin(B) * std::cos(P), std::cos(B) * std::cos(P), std::sin(P));
		const Vec Right(std::cos(B), -std::sin(B), 0.0);
		const Vec Up = Cross(Right, Ahead);
		const double HalfW = std::tan(30.0 * Pi / 180.0), HalfH = HalfW * ViewH / ViewW;
		for (int J = 0; J < ViewH; ++J)
		{
			for (int I = 0; I < ViewW; ++I)
			{
				const double X = (2.0 * (I + 0.5) / ViewW - 1.0) * HalfW, Y = (1.0 - 2.0 * (J + 0.5) / ViewH) * HalfH;
				const Vec Way = (Ahead + Right * X + Up * Y).Unit();
				double R = 0.0, G = 0.0, Bl = 0.0;
				if (Way.Z <= 0.0)
				{
					// (the sea: dark, a little lighter toward the horizon)
					const double Toward = std::exp(-40.0 * -Way.Z);
					R = 0.03 + 0.05 * Toward;
					G = 0.05 + 0.07 * Toward;
					Bl = 0.07 + 0.09 * Toward;
				}
				else
				{
					// (a plain clear sky, deep overhead and pale at the horizon, linear)
					const double High = std::sqrt(Way.Z);
					R = 0.55 - 0.45 * High;
					G = 0.68 - 0.40 * High;
					Bl = 0.85 - 0.20 * High;
					double C[4];
					Sample(D, Way, C);
					// (the painted light: 128 the light painted, the scene's ink lifting the whole as M_SkyDome does)
					const double Lift = 2.0 * 1.3 * 0.72;
					R = R * (1.0 - C[3]) + C[3] * C[0] * Lift;
					G = G * (1.0 - C[3]) + C[3] * C[1] * Lift;
					Bl = Bl * (1.0 - C[3]) + C[3] * C[2] * Lift;
				}
				const size_t At = (static_cast<size_t>(J) * PictureW + Left + I) * 3;
				Picture[At + 0] = static_cast<uint8_t>(std::lround(255.0 * Encode(R)));
				Picture[At + 1] = static_cast<uint8_t>(std::lround(255.0 * Encode(G)));
				Picture[At + 2] = static_cast<uint8_t>(std::lround(255.0 * Encode(Bl)));
			}
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		std::fprintf(stderr, "usage: dome_preview <cumulus|deck> <cover> <eye height|overview> <out.ppm> [seed]\n");
		return 2;
	}
	SeaSky::Asked Switches;
	Switches.Draw = SeaSky::Clouds::Dome;
	Switches.bWeather = true;
	Switches.bWeatherDeck = std::strcmp(argv[1], "deck") == 0;
	Switches.WeatherCover = std::atof(argv[2]);
	const uint64_t Seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 1942;
	const SeaSky::Sky Chosen = SeaSky::Choose(SeaSky::Given(), Switches);
	const Vec Sun = Vec(-0.35, -0.45, 0.82).Unit();
	const Vec Wind = Vec(13.5 * std::sin(420.0 * Pi / 180.0), 13.5 * std::cos(420.0 * Pi / 180.0), 0.0);
	const AirSky::Field F = SeaSky::Lay(Chosen, SeaSky::PlacingFor(Seed, MiddleX, MiddleY, Sun, Wind));
	const bool bOverview = std::strcmp(argv[3], "overview") == 0;
	const double EyeZ = bOverview ? SeaSky::OverviewHeight(F.Sky.bDeck, F.Base) : std::atof(argv[3]);
	std::vector<uint8_t> Sheet;
	AirSky::MakeFragments(Sheet);
	AirSky::Painter Paint;
	SeaSky::DomeHow How;
	How.Sheet = &Sheet;
	SeaSky::Dome D;
	SeaSky::PaintDome(F, Vec(MiddleX, MiddleY, EyeZ), Vec(), Sun, AirSky::Rgb(1.0f, 1.0f, 1.0f), Paint, D, How);
	std::printf("%s\n  eye %.0f m; %zu clouds, base %.0f m, top %.0f m; the dome paints %.3f of the sky (%d puffs, %d left out)\n",
		Chosen.Why.c_str(), EyeZ, F.Clouds.size(), F.Base, F.Top, D.Cover(), D.Puffs, D.Dropped);
	const int PictureW = 2 * ViewW + 8;
	std::vector<uint8_t> Picture(static_cast<size_t>(PictureW) * ViewH * 3, 255);
	// (down-sun, where the clouds' lit sides face the eye)
	View(D, 60.0, 10.0, Picture, PictureW, 0);
	View(D, 60.0, 60.0, Picture, PictureW, ViewW + 8);
	std::ofstream Out(argv[4], std::ios::binary);
	Out << "P6\n" << PictureW << " " << ViewH << "\n255\n";
	Out.write(reinterpret_cast<const char*>(Picture.data()), static_cast<std::streamsize>(Picture.size()));
	return Out ? 0 : 1;
}
