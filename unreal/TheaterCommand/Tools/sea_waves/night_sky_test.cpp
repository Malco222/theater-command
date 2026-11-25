// The night sky over the sea (Battle/SeaNightSky.{h,cpp}; look bible S7; the release 2 ocean plan, step 11(f)): the real
// stars for the battle's place and hour, and the moon with her phase. Engine-free. Built and run by
// Tools/sea_waves/run_night_sky.sh with g++ and clang, -Wall -Wextra -Wshadow -Werror.
//
// Checks: the catalogue as written (Sirius first, 2,887 stars of V 5.5 and brighter); the calendar and the sidereal
// clock against Meeus's worked examples; precession against Polaris's 1950 place; the sun at the equinox and the Arctic
// circle's midsummer midnight; the hour of night found from the sun's depth; real stars where they stand (Sirius at a
// January midnight, the Southern Cross over the Solomons and never over the Channel, Orion's belt on a winter evening,
// Polaris at the latitude's height); how many are laid on a dark night, a moonlit one, in twilight and by day; nothing
// laid in the sea's haze or behind the moon or the cloud; the moon's lit share and the side it faces; the mesh; the
// same sky every time; and its cost, once a cut.

#include "Battle/SeaNightSky.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failed = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
			std::printf("FAIL %s\n", What.c_str());
		}
	}

	std::string F(double V, int Places = 3)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.*f", Places, V);
		return Buf;
	}

	/** The catalogue's row nearest a J2000 place (degrees apart on the sky), and how far it is. */
	int FindStar(double RaHours, double DecDeg, double& OutApart)
	{
		int Best = -1;
		OutApart = 1.0e9;
		for (int I = 0; I < SeaNightSky::StarCount(); ++I)
		{
			const SeaNightSky::Star& S = SeaNightSky::StarAt(I);
			const double Dra = (S.RaHours - RaHours) * 15.0 * std::cos(DecDeg * 3.14159265358979 / 180.0);
			const double Apart = std::sqrt(Dra * Dra + (S.DecDeg - DecDeg) * (S.DecDeg - DecDeg));
			if (Apart < OutApart)
			{
				OutApart = Apart;
				Best = I;
			}
		}
		return Best;
	}

	/** A star's height and bearing at a place and hour (precessed to the year). */
	void StarUp(int Index, const SeaNightSky::When& W, double Hour, double& Up, double& From)
	{
		const SeaNightSky::Star& S = SeaNightSky::StarAt(Index);
		double Ra = 0.0, Dec = 0.0;
		SeaNightSky::Precess(S.RaHours, S.DecDeg, W.Year + (W.Month - 0.5) / 12.0, Ra, Dec);
		SeaNightSky::ToAltAz(Ra, Dec, W.Lat, SeaNightSky::SiderealDegrees(W, Hour), Up, From);
	}

	bool IsLaid(const SeaNightSky::Sky& S, int Index)
	{
		return std::any_of(S.Stars.begin(), S.Stars.end(), [Index](const SeaNightSky::Laid& L) { return L.Index == Index; });
	}

	double DotOf(const SeaNightSky::Dir& A, const SeaNightSky::Dir& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
}

int main()
{
	using namespace SeaNightSky;

	// ---- The catalogue as written
	Check(StarCount() == 2887, "the catalogue holds the 2,887 stars of V 5.5 and brighter (" + std::to_string(StarCount()) + ")");
	{
		const Star& Sirius = StarAt(0);
		Check(std::abs(Sirius.RaHours - 6.7525) < 0.001 && std::abs(Sirius.DecDeg + 16.716) < 0.01 && std::abs(Sirius.V + 1.46) < 0.01,
			"Sirius is the brightest, first in the table");
		bool bSorted = true, bInRange = true;
		for (int I = 1; I < StarCount(); ++I)
		{
			bSorted = bSorted && StarAt(I).V >= StarAt(I - 1).V;
			const Star& S = StarAt(I);
			bInRange = bInRange && S.RaHours >= 0.0f && S.RaHours < 24.0f && S.DecDeg >= -90.0f && S.DecDeg <= 90.0f && S.V <= 5.5f;
		}
		Check(bSorted, "the table runs brightest first");
		Check(bInRange, "every row's place and magnitude in range");
	}

	// ---- The calendar and the clock (Meeus, Astronomical Algorithms, examples 7.a and 12.a)
	Check(std::abs(JulianDate(2000, 1, 1.5) - 2451545.0) < 1.0e-9, "J2000 is Julian day 2451545.0");
	Check(std::abs(JulianDate(1957, 10, 4.81) - 2436116.31) < 1.0e-6, "Sputnik's launch, 1957 October 4.81, is Julian day 2436116.31 (Meeus 7.a)");
	{
		When W;
		W.Lat = 0.0;
		W.Lon = 0.0;
		W.Year = 1987;
		W.Month = 4;
		W.Day = 10;
		const double Gmst = SiderealDegrees(W, 0.0);
		Check(std::abs(Gmst - 197.693195) < 0.0005, "Greenwich sidereal time at 1987 April 10 0h is 13h 10m 46.37s (Meeus 12.a): " + F(Gmst, 5));
		W.Lon = -75.0; // (an hour of longitude: the same moment five hours of local time earlier gives the same sidereal time less 75)
		const double Local = SiderealDegrees(W, -5.0);
		Check(std::abs(std::fmod(Gmst - 75.0 + 360.0, 360.0) - Local) < 0.0005, "local sidereal time is Greenwich's plus the longitude");
	}

	// ---- Precession: Polaris back to 1950 (its 1950.0 place: 1h 48m 49s, +89 01' 43")
	{
		double Apart = 0.0;
		const int Polaris = FindStar(2.5302, 89.2642, Apart);
		Check(Polaris >= 0 && Apart < 0.01, "Polaris found in the catalogue");
		double Ra = 0.0, Dec = 0.0;
		Precess(StarAt(Polaris).RaHours, StarAt(Polaris).DecDeg, 1950.0, Ra, Dec);
		Check(std::abs(Dec - 89.0286) < 0.02 && std::abs(Ra - 1.8136) < 0.03, "Polaris precessed to 1950 stands at its 1950.0 place: " + F(Ra, 4) + " h, " + F(Dec, 4) + " deg");
		// (and at any hour, from any northern latitude, at the latitude's height within its distance from the pole)
		When W;
		W.Lat = 55.0;
		W.Lon = -25.0;
		W.Year = 1942;
		W.Month = 1;
		bool bAll = true;
		for (double H = 0.0; H < 24.0; H += 1.0)
		{
			double Up = 0.0, From = 0.0;
			StarUp(Polaris, W, H, Up, From);
			bAll = bAll && std::abs(Up - 55.0) < 1.1 && (From < 2.0 || From > 358.0);
		}
		Check(bAll, "Polaris stands at the latitude's height, due north, through the night");
	}

	// ---- The sun
	{
		When W;
		W.Lat = 0.0;
		W.Lon = 0.0;
		W.Year = 1942;
		W.Month = 3;
		W.Day = 21;
		double Up = 0.0, From = 0.0;
		SunAt(W, 12.0, Up, From);
		Check(Up > 88.0, "the equinox's noon sun stands overhead at the equator: " + F(Up, 2));
		W.Lat = 66.56;
		W.Month = 6;
		W.Day = 21;
		SunAt(W, 24.0, Up, From);
		Check(std::abs(Up) < 0.8 && (From < 5.0 || From > 355.0), "the midsummer midnight sun on the Arctic circle sits on the north horizon: " + F(Up, 2) + " deg, from " + F(From, 1));
	}

	// ---- The hour of night from the sun's depth
	{
		When W;
		W.Lat = 57.0;
		W.Lon = -25.0;
		W.Year = 1943;
		W.Month = 1;
		W.SunUp = -18.0;
		const double H = HourOfNight(W);
		double Up = 0.0, From = 0.0;
		SunAt(W, H, Up, From);
		Check(H > 16.5 && H < 20.0 && std::abs(Up + 18.0) < 0.02, "a January convoy south of Iceland: full night (sun -18) falls at " + F(H, 2) + " h local, the sun then at " + F(Up, 3));
		W.Lat = 60.0;
		W.Month = 6;
		W.Day = 21;
		const double Summer = HourOfNight(W);
		Check(std::abs(std::fmod(Summer, 24.0)) < 0.5 || std::abs(std::fmod(Summer, 24.0) - 24.0) < 0.5,
			"where the sun never sinks to -18 (60 N in June), local midnight, her lowest: " + F(Summer, 2));
	}

	// ---- Real stars where they stand
	{
		double Apart = 0.0;
		const int Sirius = FindStar(6.7525, -16.716, Apart);
		When W;
		W.Lat = 45.0;
		W.Lon = -30.0;
		W.Year = 1943;
		W.Month = 1;
		W.Day = 15;
		double Up = 0.0, From = 0.0;
		StarUp(Sirius, W, 24.0, Up, From);
		Check(Up > 23.0 && Up < 30.0 && From > 165.0 && From < 205.0, "Sirius at a January midnight from 45 N stands low in the south: " + F(Up, 1) + " deg, from " + F(From, 0));

		const int Acrux = FindStar(12.4433, -63.0992, Apart);
		W.Lat = 50.0;
		W.Lon = 0.0;
		W.Month = 5;
		bool bNeverUp = true;
		for (double H = 0.0; H < 24.0; H += 0.5)
		{
			StarUp(Acrux, W, H, Up, From);
			bNeverUp = bNeverUp && Up < 0.0;
		}
		Check(bNeverUp, "the Southern Cross never rises over the Channel");
		W.Lat = -9.4;
		W.Lon = 160.0;
		W.Year = 1942;
		W.Month = 8;
		double Most = -90.0;
		for (double H = 0.0; H < 24.0; H += 0.5)
		{
			StarUp(Acrux, W, H, Up, From);
			Most = std::max(Most, Up);
		}
		Check(Most > 35.0, "the Southern Cross stands high over the Solomons: up to " + F(Most, 1) + " deg");

		// (Orion's belt on a winter's evening, laid on the dome)
		W.Lat = 50.0;
		W.Lon = -20.0;
		W.Year = 1942;
		W.Month = 12;
		W.Day = 20;
		W.SolarHour = 23.0;
		W.SunUp = -50.0;
		const Sky S = Lay(W, Lens{}, Seeing{});
		const int Alnilam = FindStar(5.6036, -1.2019, Apart), Alnitak = FindStar(5.6793, -1.9428, Apart), Mintaka = FindStar(5.5334, -0.2992, Apart);
		Check(IsLaid(S, Alnilam) && IsLaid(S, Alnitak) && IsLaid(S, Mintaka), "Orion's belt is on the dome on a December night");
	}

	// ---- How many, and where none are laid
	auto Night = [](double SunUp, bool bMoon, double MoonUp, double MoonLit)
	{
		When W;
		W.Lat = 50.0;
		W.Lon = -20.0;
		W.Year = 1942;
		W.Month = 1;
		W.Day = 15;
		W.SunUp = SunUp;
		W.bMoon = bMoon;
		W.MoonUp = MoonUp;
		W.MoonFrom = 160.0;
		W.MoonLit = MoonLit;
		return W;
	};
	{
		const Sky Dark = Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Seeing{});
		const Sky Moonlit = Lay(Night(-24.0, true, 50.0, 1.0), Lens{}, Seeing{});
		const Sky Nautical = Lay(Night(-8.0, false, -90.0, 0.0), Lens{}, Seeing{});
		const Sky Civil = Lay(Night(-3.0, false, -90.0, 0.0), Lens{}, Seeing{});
		const Sky Daylight = Lay(Night(10.0, false, -90.0, 0.0), Lens{}, Seeing{});
		const int N = static_cast<int>(Dark.Stars.size());
		std::printf("stars laid: dark %d, full moon high %d, nautical twilight %d, civil twilight %d, day %d\n", N, static_cast<int>(Moonlit.Stars.size()),
			static_cast<int>(Nautical.Stars.size()), static_cast<int>(Civil.Stars.size()), static_cast<int>(Daylight.Stars.size()));
		Check(N > 700 && N < 1500, "a dark night at sea lays most of the thousand-odd stars above the haze (" + std::to_string(N) + ")");
		Check(Moonlit.Stars.size() < 0.5 * Dark.Stars.size(), "a full moon high washes out more than half of them");
		Check(Nautical.Stars.size() < 120 && !Nautical.Stars.empty(), "in nautical twilight only the bright ones");
		Check(Civil.Stars.size() < 12, "at the end of civil twilight a handful at most");
		Check(Daylight.Stars.empty(), "none by day");
		bool bAboveHaze = true, bLeast = true, bFinite = true;
		const double LeastHalf = 0.5 * 1.5 * (50.0 * 3.14159265358979 / 180.0) / 1600.0;
		for (const SeaNightSky::Laid& L : Dark.Stars)
		{
			bAboveHaze = bAboveHaze && std::asin(L.Way.Z) * 180.0 / 3.14159265358979 >= 3.0 - 1.0e-9;
			bLeast = bLeast && L.HalfRad >= LeastHalf * 0.999;
			bFinite = bFinite && std::isfinite(L.R) && std::isfinite(L.G) && std::isfinite(L.B) && L.R >= 0.0f && L.G >= 0.0f && L.B >= 0.0f && L.R <= 1.6f;
		}
		Check(bAboveHaze, "none laid in the sea's haze, within 3 degrees of the horizon");
		Check(bLeast, "every star at least 1.5 pixels across");
		Check(bFinite, "every star's light finite, never negative, never past 1.5");

		Seeing Half;
		Half.Cover = 0.5;
		const Sky Clouded = Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Half);
		const double Share = static_cast<double>(Clouded.HiddenByCloud) / std::max(1.0, static_cast<double>(Clouded.HiddenByCloud + Clouded.Stars.size()));
		Check(Share > 0.42 && Share < 0.58, "half the sky under cloud hides about half the stars: " + F(Share, 3));
		Seeing Whole;
		Whole.Cover = 1.0;
		Check(Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Whole).Stars.empty(), "an overcast hides them all");
		bool bSubset = true;
		for (const SeaNightSky::Laid& L : Clouded.Stars)
		{
			bSubset = bSubset && IsLaid(Dark, L.Index);
		}
		Check(bSubset, "the cloud only takes stars away (the same ones each time)");

		// (the moon hides what is behind her: put her on a bright star)
		const SeaNightSky::Laid& Bright = Dark.Stars.front();
		When W = Night(-24.0, true, std::asin(Bright.Way.Z) * 180.0 / 3.14159265358979, 0.3);
		W.MoonFrom = std::fmod(std::atan2(Bright.Way.X, Bright.Way.Y) * 180.0 / 3.14159265358979 + 360.0, 360.0);
		const Sky Behind = Lay(W, Lens{}, Seeing{});
		Check(!IsLaid(Behind, Bright.Index) && Behind.HiddenByMoon >= 1, "a star behind the moon is left out");

		// (the same sky every time)
		const Sky Again = Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Seeing{});
		bool bSame = Again.Stars.size() == Dark.Stars.size();
		for (size_t I = 0; bSame && I < Dark.Stars.size(); ++I)
		{
			bSame = Dark.Stars[I].Index == Again.Stars[I].Index && Dark.Stars[I].R == Again.Stars[I].R && Dark.Stars[I].Way.X == Again.Stars[I].Way.X;
		}
		Check(bSame, "the same inputs lay the same sky");
	}

	// ---- The Milky Way
	{
		// (the galaxy's centre, Sagittarius A*: 17h 45m 40s, -29 00' 28"; and Polaris)
		double L = 0.0, B = 0.0;
		ToGalactic(17.0 + 45.0 / 60.0 + 40.0 / 3600.0, -(29.0 + 28.0 / 3600.0), L, B);
		Check(std::abs(L) < 0.2 || std::abs(L - 360.0) < 0.2, "the galaxy's centre stands at galactic longitude 0 (" + F(L, 3) + ")");
		Check(std::abs(B) < 0.2, "and at latitude 0 (" + F(B, 3) + ")");
		ToGalactic(2.530, 89.264, L, B);
		Check(std::abs(L - 123.28) < 0.5 && std::abs(B - 26.46) < 0.5, "Polaris stands at l 123.3, b 26.5 (" + F(L, 2) + ", " + F(B, 2) + ")");
		// (there and back again)
		bool bBack = true;
		for (double Ll = 5.0; Ll < 360.0; Ll += 37.0)
		{
			for (double Bb = -70.0; Bb <= 70.0; Bb += 35.0)
			{
				double Ra = 0.0, Dec = 0.0, L2 = 0.0, B2 = 0.0;
				FromGalactic(Ll, Bb, Ra, Dec);
				ToGalactic(Ra, Dec, L2, B2);
				bBack = bBack && std::abs(B2 - Bb) < 1.0e-6 && std::abs(std::fmod(L2 - Ll + 540.0, 360.0) - 180.0) < 1.0e-6;
			}
		}
		Check(bBack, "every place goes to the galaxy's frame and back");
		Check(MilkyAt(0.0, 0.0) > MilkyAt(180.0, 0.0) * 3.0, "the band is far brighter toward the centre than toward the anticentre");
		Check(MilkyAt(5.0, 0.0) > 0.5 && MilkyAt(0.0, 45.0) < 0.01, "bright in Sagittarius, nothing at the galaxy's pole");
		Check(MilkyAt(40.0, 0.5) < 0.75 * MilkyAt(40.0, 5.0) && MilkyAt(40.0, 0.5) < 0.75 * MilkyAt(40.0, -4.0),
			"the Great Rift is a dark lane splitting the band in Aquila (" + F(MilkyAt(40.0, 0.5), 3) + " against " + F(MilkyAt(40.0, 5.0), 3) + ")");

		const Sky Dark2 = Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Seeing{});
		const Sky Moon2 = Lay(Night(-24.0, true, 50.0, 1.0), Lens{}, Seeing{});
		const Sky Twi2 = Lay(Night(-8.0, false, -90.0, 0.0), Lens{}, Seeing{});
		std::printf("the Milky Way: dark night %d specks at %.0f%%, full moon %d, nautical twilight %d\n",
			static_cast<int>(Dark2.Glow.size()), 100.0 * Dark2.MilkyShare, static_cast<int>(Moon2.Glow.size()), static_cast<int>(Twi2.Glow.size()));
		Check(Dark2.Glow.size() > 600 && Dark2.MilkyShare > 0.9, "a dark night shows the band (" + std::to_string(Dark2.Glow.size()) + " specks)");
		Check(Moon2.Glow.empty() && Twi2.Glow.empty(), "a full moon or twilight washes it away");
		bool bBand = true, bFaint = true, bHigh = true;
		double MostB = 0.0;
		for (const SeaNightSky::Laid& G : Dark2.Glow)
		{
			bFaint = bFaint && G.R > 0.0f && G.R < 0.01f;
			bHigh = bHigh && std::asin(G.Way.Z) * 180.0 / 3.14159265358979 >= 3.0 - 1.0e-9;
		}
		// (every speck within 30 degrees of the galaxy's plane: it is a band, not a scatter)
		for (const SeaNightSky::Laid& G : Dark2.Glow)
		{
			double Up = std::asin(G.Way.Z) * 180.0 / 3.14159265358979;
			double From = std::fmod(std::atan2(G.Way.X, G.Way.Y) * 180.0 / 3.14159265358979 + 360.0, 360.0);
			double Ra = 0.0, Dec = 0.0, Gl = 0.0, Gb = 0.0;
			// (back from the sky to the frame: the hour angle at the place and sidereal time)
			const double Lat = 50.0 * 3.14159265358979 / 180.0, U = Up * 3.14159265358979 / 180.0, A = From * 3.14159265358979 / 180.0;
			const double SinDec = std::sin(U) * std::sin(Lat) + std::cos(U) * std::cos(Lat) * std::cos(A);
			Dec = std::asin(SinDec) * 180.0 / 3.14159265358979;
			const double HourAngle = std::atan2(-std::cos(U) * std::sin(A), std::sin(U) * std::cos(Lat) - std::cos(U) * std::sin(Lat) * std::cos(A)) * 180.0 / 3.14159265358979;
			Ra = std::fmod(Dark2.SiderealDeg - HourAngle + 720.0, 360.0) / 15.0;
			ToGalactic(Ra, Dec, Gl, Gb);
			MostB = std::max(MostB, std::abs(Gb));
			bBand = bBand && std::abs(Gb) < 31.0;
		}
		Check(bBand, "every speck lies along the galaxy's plane, within 30 degrees (most " + F(MostB, 1) + ")");
		Check(bFaint, "every speck is far fainter than a star");
		Check(bHigh, "none in the sea's haze");
		const Sky Again2 = Lay(Night(-24.0, false, -90.0, 0.0), Lens{}, Seeing{});
		bool bSame2 = Again2.Glow.size() == Dark2.Glow.size();
		for (size_t I = 0; bSame2 && I < Dark2.Glow.size(); ++I)
		{
			bSame2 = Dark2.Glow[I].Way.X == Again2.Glow[I].Way.X && Dark2.Glow[I].R == Again2.Glow[I].R;
		}
		Check(bSame2, "the band stands still: the same specks every time");
		const Mesh Band = BuildMesh(Dark2, 4.0e7);
		Check(Band.GlowQuads == static_cast<int>(Dark2.Glow.size()) && Band.StarQuads == static_cast<int>(Dark2.Stars.size()),
			"the mesh carries a square for every star and every speck");
	}

	// ---- The moon's face
	{
		for (double Lit : { 0.08, 0.25, 0.5, 0.75, 0.95 })
		{
			When W;
			W.Lat = 50.0;
			W.Lon = -20.0;
			W.Year = 1942;
			W.Month = 1;
			W.SunUp = -24.0;
			W.bMoon = true;
			W.MoonUp = 30.0;
			W.MoonFrom = 200.0;
			W.MoonLit = Lit;
			const Sky S = Lay(W, Lens{}, Seeing{});
			const Face Fc = MoonFace(S);
			int In = 0, LitN = 0;
			double Cx = 0.0, Cy = 0.0;
			for (int I = -200; I <= 200; ++I)
			{
				for (int J = -200; J <= 200; ++J)
				{
					const double X = I / 200.0, Y = J / 200.0;
					if (X * X + Y * Y > 1.0)
					{
						continue;
					}
					++In;
					if (MoonLightAt(S, Fc.Right, Fc.Up, X, Y) > 0.3)
					{
						++LitN;
						Cx += X;
						Cy += Y;
					}
				}
			}
			const double Share = static_cast<double>(LitN) / In;
			Check(std::abs(Share - Lit) < 0.03, "the moon " + F(Lit, 2) + " lit shows " + F(Share, 3) + " of her face lit");
			// (the lit side faces the sun as she stands in the sky)
			const double Sx = DotOf(S.SunWay, Fc.Right), Sy = DotOf(S.SunWay, Fc.Up);
			Check(Lit > 0.9 || Cx * Sx + Cy * Sy > 0.0, "her lit side faces the sun (" + F(Lit, 2) + ")");
			const Mesh M = BuildMesh(S, 4.0e7);
			Check(M.MoonTriangles == 64 + 2 * 64 * 7, "her disc is 64 round and 8 rings in");
		}
	}

	// ---- The mesh, and its cost once a cut
	{
		When W;
		W.Lat = 57.0;
		W.Lon = -25.0;
		W.Year = 1943;
		W.Month = 1;
		W.SunUp = -18.0;
		W.bMoon = true;
		W.MoonUp = 25.0;
		W.MoonFrom = 228.0;
		W.MoonLit = 0.34;
		const double Radius = 4.0e7; // (400 km at 100 units a metre)
		std::vector<double> Times;
		Mesh M;
		Sky S;
		for (int K = 0; K < 15; ++K)
		{
			const auto T0 = std::chrono::steady_clock::now();
			S = Lay(W, Lens{}, Seeing{});
			M = BuildMesh(S, Radius);
			Times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count());
		}
		std::sort(Times.begin(), Times.end());
		const int Points = static_cast<int>(M.V.size() / 3);
		bool bIndices = M.Triangles.size() % 3 == 0, bOnDome = true;
		for (int T : M.Triangles)
		{
			bIndices = bIndices && T >= 0 && T < Points;
		}
		for (int P = 0; P < Points; ++P)
		{
			const double D = std::sqrt(M.V[P * 3] * M.V[P * 3] + M.V[P * 3 + 1] * M.V[P * 3 + 1] + M.V[P * 3 + 2] * M.V[P * 3 + 2]);
			bOnDome = bOnDome && std::abs(D - Radius) < Radius * 0.0001;
		}
		Check(M.StarQuads == static_cast<int>(S.Stars.size()) && M.C.size() == static_cast<size_t>(Points) * 4, "a square for every star, a colour for every point");
		Check(bIndices, "every triangle's points exist");
		Check(bOnDome, "every point on the dome");
		std::printf("the convoy night (57 N, January, moon 0.34 lit at 25 deg): %d stars, hour %.2f, limit %.2f; %d triangles; lay and mesh %.3f ms (median of 15)\n",
			static_cast<int>(S.Stars.size()), S.SolarHour, S.LimitMag, static_cast<int>(M.Triangles.size() / 3), Times[Times.size() / 2]);
		Check(Times[Times.size() / 2] < 4.0, "the sky is laid and meshed in under 4 ms, once a cut");
		Check(M.Triangles.size() / 3 < 8000, "under 8,000 triangles on the dome (the stars, the Milky Way's specks and the moon)");
		// (in a twilight sky her dark side is left out, so the sky shows through it; on a dark night her whole disc is
		// laid, earthshine on her dark side; the stars always all drawn)
		const Mesh Twilight = BuildMesh(S, Radius, 64, 8, 0.08);
		int MoonIn = 0, MoonOut = 0, StarsOut = 0;
		for (size_t P = 0; P < Twilight.C.size() / 4; ++P)
		{
			const bool bStar = P < static_cast<size_t>(Twilight.StarQuads) * 4;
			const float A = Twilight.C[P * 4 + 3];
			if (bStar)
			{
				StarsOut += A < 0.5f ? 1 : 0;
			}
			else
			{
				(A > 0.5f ? MoonIn : MoonOut) += 1;
			}
		}
		Check(StarsOut == 0, "the cut leaves every star drawn");
		Check(MoonIn > 0 && MoonOut > 0, "a 0.34-lit moon in twilight: her lit side drawn (" + std::to_string(MoonIn) + " points), her dark side left out (" + std::to_string(MoonOut) + ")");
		int WholeOut = 0;
		for (size_t P = 0; P < M.C.size() / 4; ++P)
		{
			WholeOut += M.C[P * 4 + 3] < 0.5f ? 1 : 0;
		}
		Check(WholeOut == 0, "on a dark night her whole disc is laid");
	}

	std::printf("night_sky_test: %d checks, %d failed\n", Checks, Failed);
	return Failed == 0 ? 0 : 1;
}
