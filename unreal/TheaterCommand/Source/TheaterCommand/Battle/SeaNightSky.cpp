#include "SeaNightSky.h"

#include <algorithm>
#include <cmath>

namespace SeaNightSky
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;

		const Star Catalogue[] = {
#include "SeaNightStars.inl"
		};
		constexpr int CatalogueCount = static_cast<int>(sizeof(Catalogue) / sizeof(Catalogue[0]));

		/** [EST] The light lost through the air at sea, magnitudes an air mass: a clear night on the open sea, with its
		 * salt haze, a little more than a dry mountain's 0.2 to 0.25 (V band). */
		constexpr double Extinction = 0.35;
		/** [EST] Nothing within this many degrees of the sea's horizon: its haze swallows even the bright ones. */
		constexpr double HazeDeg = 3.0;
		/** The faintest the catalogue was cut at (make_star_table.py). */
		constexpr double FaintestKept = 5.5;
		/** [EST] A full moon high in the sky takes this many magnitudes off what can be seen (a dark sky's 6 to the 4 or
		 * so of a moonlit one). */
		constexpr double MoonWash = 1.8;
		/** The moon's mean half width across the sky: 0.518 degrees across. */
		constexpr double MoonHalfDeg = 0.2591;
		/** [EST] Earthshine: the dark of her face lit by the Earth, as a share of her lit face's light, most at a thin
		 * crescent (Leonardo's "ashen light"). Enough to make her round, never enough to read as lit. */
		constexpr double EarthShine = 0.02;
		/** The terminator's softness across her face (her own hills and the half degree of the sun), in her radius. */
		constexpr double TerminatorSoft = 0.04;
		/** A star at least this many of the screen's pixels across, so the anti-aliasing never loses it (the plan's 1.5). */
		constexpr double LeastPx = 1.5;

		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp((X - E0) / (E1 - E0), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		double Wrap360(double D)
		{
			D = std::fmod(D, 360.0);
			return D < 0.0 ? D + 360.0 : D;
		}
		Dir Norm(Dir A)
		{
			const double L = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
			if (L <= 0.0)
			{
				return Dir{ 0.0, 0.0, 1.0 };
			}
			return Dir{ A.X / L, A.Y / L, A.Z / L };
		}
		double DotOf(const Dir& A, const Dir& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		Dir CrossOf(const Dir& A, const Dir& B) { return Dir{ A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		/** SplitMix64: a star's own number for the cloud's cover, never the show's. */
		double HashOf(uint64_t N)
		{
			uint64_t Z = N + 0x9E3779B97F4A7C15ull;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
		}

		/** The moon's seas as they lie on her face (selenographic latitude and longitude, east toward Mare Crisium; the
		 * radius each covers, degrees of her surface). Her near side's dark plains, the "man in the moon". */
		struct Mare
		{
			double Lat, Lon, Radius;
		};
		const Mare Maria[] = {
			{ 18.0, -57.0, 20.0 },  // Oceanus Procellarum (its northern half)
			{ -5.0, -45.0, 15.0 },  // Oceanus Procellarum (its southern half)
			{ 33.0, -16.0, 13.0 },  // Mare Imbrium
			{ 28.0, 17.5, 9.0 },    // Mare Serenitatis
			{ 8.5, 31.0, 11.0 },    // Mare Tranquillitatis
			{ 17.0, 59.0, 7.0 },    // Mare Crisium
			{ -7.8, 51.3, 8.5 },    // Mare Fecunditatis
			{ -21.0, -17.0, 9.5 },  // Mare Nubium
			{ -24.0, -39.0, 5.0 },  // Mare Humorum
			{ -15.0, 35.0, 4.5 },   // Mare Nectaris
			{ 13.0, 1.0, 4.5 },     // Sinus Medii and Mare Vaporum
			{ 56.0, 0.0, 6.0 },     // Mare Frigoris (a band; its middle)
		};
		/** [EST] How much darker her seas are than her highlands, as the eye sees her (albedo about 0.07 against 0.12). */
		constexpr double MareDark = 0.38;
	}

	int StarCount()
	{
		return CatalogueCount;
	}

	const Star& StarAt(int Index)
	{
		return Catalogue[std::clamp(Index, 0, CatalogueCount - 1)];
	}

	double JulianDate(int Year, int Month, double DayAndFraction)
	{
		// (Meeus, Astronomical Algorithms, chapter 7: the Gregorian calendar)
		int Y = Year, M = Month;
		if (M <= 2)
		{
			Y -= 1;
			M += 12;
		}
		const int A = Y / 100;
		const int B = 2 - A + A / 4;
		return std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + DayAndFraction + B - 1524.5;
	}

	namespace
	{
		double JulianOf(const When& W, double SolarHour)
		{
			// (local mean solar time is the universal time moved by the longitude: an hour for every 15 degrees east)
			const double Universal = SolarHour - W.Lon / 15.0;
			return JulianDate(W.Year, std::clamp(W.Month, 1, 12), std::clamp(W.Day, 1, 31) + Universal / 24.0);
		}
	}

	double SiderealDegrees(const When& W, double SolarHour)
	{
		// (the IAU 1982 Greenwich mean sidereal time, then the longitude)
		const double JD = JulianOf(W, SolarHour);
		const double T = (JD - 2451545.0) / 36525.0;
		const double Greenwich = 280.46061837 + 360.98564736629 * (JD - 2451545.0) + 0.000387933 * T * T - T * T * T / 38710000.0;
		return Wrap360(Greenwich + W.Lon);
	}

	void ToAltAz(double RaHours, double DecDeg, double LatDeg, double SiderealDeg, double& OutUp, double& OutFrom)
	{
		const double H = (SiderealDeg - RaHours * 15.0) * Rad, D = DecDeg * Rad, P = LatDeg * Rad;
		const double SinUp = std::sin(P) * std::sin(D) + std::cos(P) * std::cos(D) * std::cos(H);
		OutUp = std::asin(Clamp(SinUp, -1.0, 1.0)) / Rad;
		// (bearing clockwise from north: a star in the east as it rises, hour angle -90, bears 090)
		OutFrom = Wrap360(std::atan2(-std::cos(D) * std::sin(H), std::sin(D) * std::cos(P) - std::cos(D) * std::sin(P) * std::cos(H)) / Rad);
	}

	void SunAt(const When& W, double SolarHour, double& OutUp, double& OutFrom)
	{
		// (the Astronomical Almanac's low-precision sun: mean longitude, mean anomaly, the equation of centre)
		const double N = JulianOf(W, SolarHour) - 2451545.0;
		const double L = 280.460 + 0.9856474 * N, G = (357.528 + 0.9856003 * N) * Rad;
		const double Lambda = (L + 1.915 * std::sin(G) + 0.020 * std::sin(2.0 * G)) * Rad;
		const double Eps = (23.439 - 0.0000004 * N) * Rad;
		const double Ra = Wrap360(std::atan2(std::cos(Eps) * std::sin(Lambda), std::cos(Lambda)) / Rad) / 15.0;
		const double Dec = std::asin(std::sin(Eps) * std::sin(Lambda)) / Rad;
		ToAltAz(Ra, Dec, W.Lat, SiderealDegrees(W, SolarHour), OutUp, OutFrom);
	}

	double HourOfNight(const When& W)
	{
		// (from noon on through the night, every quarter hour, the first time she sinks to the rules' depth; then halved
		// down to a few seconds)
		double Up = 0.0, From = 0.0, Lowest = 1.0e9, LowestAt = 24.0;
		double Before = 0.0;
		SunAt(W, 12.0, Before, From);
		for (double H = 12.25; H <= 36.0 + 1.0e-9; H += 0.25)
		{
			SunAt(W, H, Up, From);
			if (Up < Lowest)
			{
				Lowest = Up;
				LowestAt = H;
			}
			if (Before > W.SunUp && Up <= W.SunUp)
			{
				double A = H - 0.25, B = H;
				for (int K = 0; K < 30; ++K)
				{
					const double M = 0.5 * (A + B);
					double MUp = 0.0;
					SunAt(W, M, MUp, From);
					(MUp > W.SunUp ? A : B) = M;
				}
				return 0.5 * (A + B);
			}
			Before = Up;
		}
		return LowestAt;
	}

	void Precess(double RaHours, double DecDeg, double Year, double& OutRaHours, double& OutDecDeg)
	{
		// (IAU 1976 precession from J2000 to the middle of the battle's year: Meeus 21.2-21.4; about 0.8 degrees of
		// longitude back to 1942)
		const double T = (JulianDate(static_cast<int>(std::floor(Year)), 7, 2.0) - 2451545.0) / 36525.0;
		const double Arc = Rad / 3600.0;
		const double Zeta = (2306.2181 * T + 0.30188 * T * T + 0.017998 * T * T * T) * Arc;
		const double Z = (2306.2181 * T + 1.09468 * T * T + 0.018203 * T * T * T) * Arc;
		const double Theta = (2004.3109 * T - 0.42665 * T * T - 0.041833 * T * T * T) * Arc;
		const double Ra0 = RaHours * 15.0 * Rad, Dec0 = DecDeg * Rad;
		const double A = std::cos(Dec0) * std::sin(Ra0 + Zeta);
		const double B = std::cos(Theta) * std::cos(Dec0) * std::cos(Ra0 + Zeta) - std::sin(Theta) * std::sin(Dec0);
		const double C = std::sin(Theta) * std::cos(Dec0) * std::cos(Ra0 + Zeta) + std::cos(Theta) * std::sin(Dec0);
		OutRaHours = Wrap360((std::atan2(A, B) + Z) / Rad) / 15.0;
		OutDecDeg = std::asin(Clamp(C, -1.0, 1.0)) / Rad;
	}

	Dir AltAz(double UpDeg, double FromDeg)
	{
		const double U = UpDeg * Rad, F = FromDeg * Rad;
		return Dir{ std::cos(U) * std::sin(F), std::cos(U) * std::cos(F), std::sin(U) };
	}

	double AirMass(double UpDeg)
	{
		// (Kasten and Young 1989)
		const double H = std::max(UpDeg, 0.0);
		return 1.0 / (std::sin(H * Rad) + 0.50572 * std::pow(H + 6.07995, -1.6364));
	}

	double LimitOf(const When& W, const Seeing& See)
	{
		double Limit = FaintestKept;
		if (See.StarShare >= 0.0)
		{
			// (the light core's own twilight: none of them by day, all of them in full night)
			Limit = -1.5 + (FaintestKept + 1.5) * Clamp(See.StarShare, 0.0, 1.0);
		}
		else
		{
			// (the sky darkening through the twilights: the first bright stars as civil twilight ends, about the second
			// magnitude by nautical, the rest by astronomical; Schaefer's limits for a clear sky, rounded)
			const double S = W.SunUp;
			if (S >= -2.0)
			{
				Limit = -1.5;
			}
			else if (S >= -6.0)
			{
				Limit = -1.5 + 3.0 * (-2.0 - S) / 4.0;
			}
			else if (S >= -12.0)
			{
				Limit = 1.5 + 3.0 * (-6.0 - S) / 6.0;
			}
			else if (S >= -18.0)
			{
				Limit = 4.5 + 1.0 * (-12.0 - S) / 6.0;
			}
		}
		if (W.bMoon && W.MoonUp > 0.0)
		{
			Limit -= MoonWash * Clamp(W.MoonLit, 0.0, 1.0) * Smooth(0.0, 30.0, W.MoonUp);
		}
		return std::min(Limit, FaintestKept);
	}

	void ColourOf(double Kelvin, float& R, float& G, float& B)
	{
		if (Kelvin <= 0.0)
		{
			R = G = B = 1.0f;
			return;
		}
		// (a black body's colour, Tanner Helland's fit to Charity's table, in 0 to 255 and taken to linear; then half way
		// back to white, as the eye sees stars: only the reddest and bluest show their colour)
		const double T = Clamp(Kelvin, 1500.0, 40000.0) / 100.0;
		double Rs = 255.0, Gs = 0.0, Bs = 255.0;
		if (T <= 66.0)
		{
			Gs = 99.4708025861 * std::log(T) - 161.1195681661;
			Bs = T <= 19.0 ? 0.0 : 138.5177312231 * std::log(T - 10.0) - 305.0447927307;
		}
		else
		{
			Rs = 329.698727446 * std::pow(T - 60.0, -0.1332047592);
			Gs = 288.1221695283 * std::pow(T - 60.0, -0.0755148492);
		}
		auto Linear = [](double S) { const double C = Clamp(S, 0.0, 255.0) / 255.0; return std::pow(C, 2.2); };
		double Lr = Linear(Rs), Lg = Linear(Gs), Lb = Linear(Bs);
		const double Most = std::max({ Lr, Lg, Lb, 1.0e-6 });
		Lr = 0.5 + 0.5 * Lr / Most;
		Lg = 0.5 + 0.5 * Lg / Most;
		Lb = 0.5 + 0.5 * Lb / Most;
		R = static_cast<float>(Lr);
		G = static_cast<float>(Lg);
		B = static_cast<float>(Lb);
	}

	Sky Lay(const When& W, const Lens& L, const Seeing& See)
	{
		Sky Out;
		const double Hour = W.SolarHour >= 0.0 ? W.SolarHour : HourOfNight(W);
		Out.SolarHour = std::fmod(Hour, 24.0);
		Out.SiderealDeg = SiderealDegrees(W, Hour);
		double SunUp = 0.0, SunFrom = 0.0;
		SunAt(W, Hour, SunUp, SunFrom);
		Out.SunWay = AltAz(SunUp, SunFrom);
		Out.NorthPole = AltAz(W.Lat, 0.0);
		Out.LimitMag = LimitOf(W, See);
		Out.bMoonUp = W.bMoon && W.MoonUp > -MoonHalfDeg;
		Out.MoonWay = AltAz(W.MoonUp, W.MoonFrom);
		Out.MoonHalfRad = MoonHalfDeg * Rad;
		Out.MoonLit = Clamp(W.MoonLit, 0.0, 1.0);
		const double PxRad = std::max(L.FovDeg, 1.0) * Rad / std::max(L.ViewPx, 64.0);
		const double LeastHalf = 0.5 * LeastPx * PxRad;
		const double Cover = Clamp(See.Cover, 0.0, 1.0);
		const double MoonGuard = std::cos(Out.MoonHalfRad * 1.1);
		for (int I = 0; I < CatalogueCount; ++I)
		{
			const Star& S = Catalogue[I];
			if (S.V > Out.LimitMag + 0.0001)
			{
				++Out.TooFaint;
				continue;
			}
			double Ra = 0.0, Dec = 0.0, Up = 0.0, From = 0.0;
			Precess(S.RaHours, S.DecDeg, W.Year + (W.Month - 0.5) / 12.0, Ra, Dec);
			ToAltAz(Ra, Dec, W.Lat, Out.SiderealDeg, Up, From);
			if (Up < HazeDeg)
			{
				++Out.BelowHaze;
				continue;
			}
			// (dimmed through the air low down, against the limit as seen overhead)
			const double Seen = S.V + Extinction * (AirMass(Up) - 1.0);
			if (Seen > Out.LimitMag)
			{
				++Out.TooFaint;
				continue;
			}
			if (Cover > 0.0 && HashOf(static_cast<uint64_t>(I) * 2654435761ull + 77ull) < Cover)
			{
				++Out.HiddenByCloud;
				continue;
			}
			const Dir Way = AltAz(Up, From);
			if (Out.bMoonUp && DotOf(Way, Out.MoonWay) > MoonGuard)
			{
				++Out.HiddenByMoon;
				continue;
			}
			// (its light: a magnitude-one star 1, each magnitude about 1.58 times dimmer, the half power of the true 2.51
			// as the eye judges stars against each other on a screen; faded in over the last 0.7 of a magnitude above
			// the limit, so as the twilight deepens they come out and none pops)
			double Light = See.Strength * std::pow(10.0, -0.2 * (Seen - 1.0)) * Smooth(Out.LimitMag, Out.LimitMag - 0.7, Seen);
			double Half = LeastHalf;
			if (Light > 1.5)
			{
				// (the brightest grow a little rather than burn white: the same light over a wider square)
				Half = LeastHalf * std::sqrt(Light / 1.5);
				Light = 1.5;
			}
			Laid One;
			One.Way = Way;
			One.HalfRad = Half;
			One.Index = I;
			float R = 1.0f, G = 1.0f, B = 1.0f;
			ColourOf(S.Kelvin, R, G, B);
			One.R = static_cast<float>(R * Light);
			One.G = static_cast<float>(G * Light);
			One.B = static_cast<float>(B * Light);
			Out.Stars.push_back(One);
		}
		return Out;
	}

	double MoonLightAt(const Sky& S, const Dir& Right, const Dir& Up, double X, double Y)
	{
		const double R2 = X * X + Y * Y;
		if (R2 > 1.0)
		{
			return 0.0;
		}
		// (her phase: the side toward the sun lit; the terminator a half ellipse whose width across her is how lit she is)
		const double Sx = DotOf(S.SunWay, Right), Sy = DotOf(S.SunWay, Up);
		const double A = (std::abs(Sx) + std::abs(Sy) > 1.0e-9) ? std::atan2(Sy, Sx) : 0.0;
		const double Xs = X * std::cos(A) + Y * std::sin(A), Ys = -X * std::sin(A) + Y * std::cos(A);
		const double Edge = (1.0 - 2.0 * S.MoonLit) * std::sqrt(std::max(0.0, 1.0 - Ys * Ys));
		const double Lit = Smooth(-TerminatorSoft, TerminatorSoft, Xs - Edge);
		// (her seas: where this point of her face lies, in her own latitude and longitude, east toward the right)
		const double Lat = std::asin(Clamp(Y, -1.0, 1.0));
		const double CosLat = std::cos(Lat);
		const double Lon = CosLat > 1.0e-6 ? std::asin(Clamp(X / CosLat, -1.0, 1.0)) : 0.0;
		const Dir P{ std::cos(Lat) * std::sin(Lon), std::sin(Lat), std::cos(Lat) * std::cos(Lon) };
		double Sea = 0.0;
		for (const Mare& M : Maria)
		{
			const Dir C{ std::cos(M.Lat * Rad) * std::sin(M.Lon * Rad), std::sin(M.Lat * Rad), std::cos(M.Lat * Rad) * std::cos(M.Lon * Rad) };
			const double Apart = std::acos(Clamp(DotOf(P, C), -1.0, 1.0)) / Rad;
			Sea = std::max(Sea, 1.0 - Smooth(M.Radius, M.Radius * 1.3, Apart));
		}
		const double Albedo = 1.0 - MareDark * Sea;
		const double Shine = EarthShine * (1.0 - S.MoonLit);
		return Albedo * (Shine + (1.0 - Shine) * Lit);
	}

	Face MoonFace(const Sky& S, int Segments, int Rings)
	{
		Face Out;
		Segments = std::max(Segments, 8);
		Rings = std::max(Rings, 1);
		// (her up toward the celestial pole as she stands in the sky, her right across: an eye facing her with the pole
		// above has her east, Mare Crisium, on its right)
		const Dir M = S.MoonWay;
		Dir Up{ S.NorthPole.X - DotOf(S.NorthPole, M) * M.X, S.NorthPole.Y - DotOf(S.NorthPole, M) * M.Y, S.NorthPole.Z - DotOf(S.NorthPole, M) * M.Z };
		if (std::sqrt(DotOf(Up, Up)) < 1.0e-6)
		{
			Up = Dir{ 0.0, 0.0, 1.0 };
		}
		Up = Norm(Up);
		const Dir Right = Norm(CrossOf(M, Up));
		Out.Right = Right;
		Out.Up = Up;
		Out.X.push_back(0.0);
		Out.Y.push_back(0.0);
		for (int R = 1; R <= Rings; ++R)
		{
			const double Rr = static_cast<double>(R) / Rings;
			for (int K = 0; K < Segments; ++K)
			{
				const double Ang = 2.0 * Pi * K / Segments;
				Out.X.push_back(Rr * std::cos(Ang));
				Out.Y.push_back(Rr * std::sin(Ang));
			}
		}
		for (size_t P = 0; P < Out.X.size(); ++P)
		{
			Out.Light.push_back(static_cast<float>(MoonLightAt(S, Right, Up, Out.X[P], Out.Y[P])));
		}
		// (wound so the face looks back at the eye: counter-clockwise as the eye sees her, x right and y up)
		auto At = [Segments](int Ring, int K) { return 1 + (Ring - 1) * Segments + ((K % Segments) + Segments) % Segments; };
		for (int K = 0; K < Segments; ++K)
		{
			Out.Triangles.insert(Out.Triangles.end(), { 0, At(1, K), At(1, K + 1) });
		}
		for (int R = 1; R < Rings; ++R)
		{
			for (int K = 0; K < Segments; ++K)
			{
				const int A = At(R, K), B = At(R, K + 1), C = At(R + 1, K), D = At(R + 1, K + 1);
				Out.Triangles.insert(Out.Triangles.end(), { A, C, D, A, D, B });
			}
		}
		return Out;
	}

	Mesh BuildMesh(const Sky& S, double Radius, int MoonSegments, int MoonRings)
	{
		Mesh Out;
		Out.V.reserve((S.Stars.size() * 4 + 600) * 3);
		Out.C.reserve((S.Stars.size() * 4 + 600) * 4);
		Out.Triangles.reserve(S.Stars.size() * 6 + 3000);
		for (const Laid& One : S.Stars)
		{
			// (a square facing the eye: two directions across the line to it)
			Dir U = CrossOf(Dir{ 0.0, 0.0, 1.0 }, One.Way);
			if (DotOf(U, U) < 1.0e-12)
			{
				U = Dir{ 1.0, 0.0, 0.0 };
			}
			U = Norm(U);
			const Dir Vv = Norm(CrossOf(One.Way, U));
			const double H = One.HalfRad * Radius;
			const int Base = static_cast<int>(Out.V.size() / 3);
			const double Corners[4][2] = { { -1.0, -1.0 }, { 1.0, -1.0 }, { 1.0, 1.0 }, { -1.0, 1.0 } };
			for (const auto& Cn : Corners)
			{
				Out.V.push_back(One.Way.X * Radius + (U.X * Cn[0] + Vv.X * Cn[1]) * H);
				Out.V.push_back(One.Way.Y * Radius + (U.Y * Cn[0] + Vv.Y * Cn[1]) * H);
				Out.V.push_back(One.Way.Z * Radius + (U.Z * Cn[0] + Vv.Z * Cn[1]) * H);
				Out.C.insert(Out.C.end(), { One.R, One.G, One.B, 1.0f });
			}
			Out.Triangles.insert(Out.Triangles.end(), { Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
			++Out.StarQuads;
		}
		if (S.bMoonUp)
		{
			const Face F = MoonFace(S, MoonSegments, MoonRings);
			const double HalfWide = std::tan(S.MoonHalfRad) * Radius;
			// (her light low down: reddened and dimmed through the air, as she rises orange out of the haze)
			const double Up = std::asin(Clamp(S.MoonWay.Z, -1.0, 1.0)) / Rad;
			const double Low = 1.0 - Smooth(0.0, 15.0, Up);
			const double Through = std::pow(10.0, -0.4 * Extinction * (AirMass(Up) - 1.0));
			// (high: a faintly warm white, 1, 0.97, 0.92; on the horizon 1, 0.74, 0.48, the orange of a rising moon)
			const float Tr = static_cast<float>(Through), Tg = static_cast<float>(Through * (0.97 - 0.23 * Low)), Tb = static_cast<float>(Through * (0.92 - 0.44 * Low));
			const int Base = static_cast<int>(Out.V.size() / 3);
			for (size_t P = 0; P < F.X.size(); ++P)
			{
				const double Ox = (F.Right.X * F.X[P] + F.Up.X * F.Y[P]) * HalfWide, Oy = (F.Right.Y * F.X[P] + F.Up.Y * F.Y[P]) * HalfWide,
							 Oz = (F.Right.Z * F.X[P] + F.Up.Z * F.Y[P]) * HalfWide;
				Out.V.push_back(S.MoonWay.X * Radius + Ox);
				Out.V.push_back(S.MoonWay.Y * Radius + Oy);
				Out.V.push_back(S.MoonWay.Z * Radius + Oz);
				const float L = F.Light[P];
				Out.C.insert(Out.C.end(), { L * Tr, L * Tg, L * Tb, 1.0f });
			}
			for (int T : F.Triangles)
			{
				Out.Triangles.push_back(Base + T);
			}
			Out.MoonTriangles = static_cast<int>(F.Triangles.size() / 3);
		}
		return Out;
	}
}
