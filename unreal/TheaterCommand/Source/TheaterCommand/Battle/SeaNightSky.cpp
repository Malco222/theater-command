#include "SeaNightSky.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

		/** The galaxy's north pole and the galactic longitude of the celestial pole (IAU 1958 in J2000). */
		constexpr double PoleRaDeg = 192.85948, PoleDecDeg = 27.12825, PoleL = 122.93192;
		/** [EST] The Milky Way's light where it is brightest (the Sagittarius star cloud), against a first-magnitude
		 * star's: it is a glow, never a point, so each speck of it carries a thousandth or so of one. */
		constexpr double MilkyLight = 0.0016;
		/** [EST] How many specks the band is drawn with over the whole sphere: enough that the eye reads a band and not
		 * dots (about a third of them stand above the horizon on any night), few enough to cost nothing (each is one
		 * square, as a star is, and they are laid once at a cut). */
		constexpr int MilkyMost = 5200;
		/** [EST] A speck is this many times a star's least width: a little wider, so they melt together into a glow. */
		constexpr double MilkyWide = 2.2;

		/** One bright cloud of the band: where it lies (galactic longitude and latitude), how far it reaches (degrees
		 * along the band and across it) and how much it lifts the band's light there. */
		struct Cloud
		{
			double L, B, Along, Across, Lift;
		};
		const Cloud Clouds[] = {
			{ 4.0, -2.0, 14.0, 7.0, 1.25 },    // the great Sagittarius cloud, toward the galaxy's centre
			{ 27.0, -1.0, 10.0, 5.0, 0.95 },   // the Scutum cloud
			{ 50.0, 0.0, 9.0, 4.0, 0.45 },     // Aquila and Vulpecula
			{ 78.0, 2.5, 12.0, 6.0, 0.85 },    // the Cygnus cloud, overhead on a northern summer night
			{ 287.0, -1.0, 13.0, 6.0, 0.90 },  // Carina, the southern sky's brightest reach
			{ 310.0, 0.0, 10.0, 5.0, 0.55 },   // Crux and Centaurus
			{ 333.0, -1.5, 10.0, 5.0, 0.60 },  // Norma
			{ 206.0, -2.0, 14.0, 5.0, 0.10 },  // the anticentre, faintest of all
		};
		/** The Great Rift's dark lane, where the dust in front of the band splits it: from Cygnus down through Aquila
		 * and Ophiuchus to Sagittarius. Where it lies along the band, how wide across, and how much it takes away. */
		struct Lane
		{
			double FromL, ToL, B, Across, Takes;
		};
		const Lane Lanes[] = {
			{ 10.0, 80.0, 0.5, 2.4, 0.62 },     // the Great Rift
			{ 280.0, 320.0, -0.5, 2.0, 0.45 },  // the Coalsack and the southern dust, by Crux
		};

		/** Where two galactic longitudes stand apart, in degrees (the short way round). */
		double ApartL(double A, double B)
		{
			const double D = std::abs(Wrap360(A) - Wrap360(B));
			return D > 180.0 ? 360.0 - D : D;
		}
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

	void ToGalactic(double RaHours, double DecDeg, double& OutL, double& OutB)
	{
		const double Ra = RaHours * 15.0 * Rad, Dec = DecDeg * Rad;
		const double Pr = PoleRaDeg * Rad, Pd = PoleDecDeg * Rad;
		const double SinB = std::sin(Dec) * std::sin(Pd) + std::cos(Dec) * std::cos(Pd) * std::cos(Ra - Pr);
		OutB = std::asin(Clamp(SinB, -1.0, 1.0)) / Rad;
		const double Y = std::cos(Dec) * std::sin(Ra - Pr);
		const double X = std::sin(Dec) * std::cos(Pd) - std::cos(Dec) * std::sin(Pd) * std::cos(Ra - Pr);
		OutL = Wrap360(PoleL - std::atan2(Y, X) / Rad);
	}

	void FromGalactic(double L, double B, double& OutRaHours, double& OutDecDeg)
	{
		const double Lr = L * Rad, Br = B * Rad;
		const double Pd = PoleDecDeg * Rad, Pl = PoleL * Rad;
		const double SinDec = std::sin(Br) * std::sin(Pd) + std::cos(Br) * std::cos(Pd) * std::cos(Pl - Lr);
		OutDecDeg = std::asin(Clamp(SinDec, -1.0, 1.0)) / Rad;
		const double Y = std::cos(Br) * std::sin(Pl - Lr);
		const double X = std::sin(Br) * std::cos(Pd) - std::cos(Br) * std::sin(Pd) * std::cos(Pl - Lr);
		OutRaHours = Wrap360(PoleRaDeg + std::atan2(Y, X) / Rad) / 15.0;
	}

	double MilkyAt(double L, double B)
	{
		// (a painter's map of the band, from the photographs' broad shape: a band thickest and brightest toward the
		// galaxy's centre in Sagittarius, thinning to almost nothing at the anticentre; its star clouds; its dark
		// lanes in front. Nothing is copied: these are figures for the shape, not a survey's data.)
		const double Middle = ApartL(L, 0.0);                       // 0 at the centre, 180 at the anticentre
		const double Toward = 1.0 - Middle / 180.0;                  // 1 at the centre, 0 at the anticentre
		const double Thick = 3.2 + 4.3 * Toward * Toward;            // degrees: the band's half width across
		double Light = 0.0;
		if (std::abs(B) < 4.0 * Thick)
		{
			Light = std::exp(-0.5 * (B / Thick) * (B / Thick)) * (0.14 + 0.52 * Toward * Toward * Toward);
		}
		for (const Cloud& C : Clouds)
		{
			// (far from this cloud along the band or across it: nothing of it here, and no call made)
			if (ApartL(L, C.L) > 3.0 * C.Along || std::abs(B - C.B) > 3.0 * C.Across)
			{
				continue;
			}
			const double Dl = ApartL(L, C.L) / C.Along, Db = (B - C.B) / C.Across;
			Light += C.Lift * std::exp(-0.5 * (Dl * Dl + Db * Db));
		}
		if (Light <= 0.0)
		{
			return 0.0;
		}
		for (const Lane& Ln : Lanes)
		{
			const double Mid = Wrap360(0.5 * (Ln.FromL + Ln.ToL)), Half = 0.5 * std::abs(Ln.ToL - Ln.FromL);
			if (ApartL(L, Mid) > Half + 12.0 || std::abs(B - Ln.B) > 3.0 * Ln.Across)
			{
				continue;
			}
			const double Reach = 1.0 - Smooth(Half, Half + 12.0, ApartL(L, Mid));
			const double Deep = (B - Ln.B) / Ln.Across;
			Light *= 1.0 - Ln.Takes * Reach * std::exp(-0.5 * Deep * Deep);
		}
		return Clamp(Light, 0.0, 1.0);
	}

	namespace
	{
		/** One speck of the band, drawn once in the galaxy's frame and kept: where it stands at J2000, how bright the
		 * band is there, and its own number (for the cloud's cover). */
		struct Speck
		{
			double RaHours, DecDeg, Bright;
			uint64_t Own;
		};
		/** The band's specks, laid out once by a golden-ratio walk along it and a hash across it, kept for every sky
		 * after: the same band every time, and nothing to work out again at a cut. */
		const std::vector<Speck>& BandSpecks()
		{
			static const std::vector<Speck> Band = []
			{
				std::vector<Speck> Out;
				Out.reserve(MilkyMost);
				const double Golden = 0.6180339887498949;
				for (int K = 0; K < MilkyMost * 3 && static_cast<int>(Out.size()) < MilkyMost; ++K)
				{
					const double L = 360.0 * std::fmod(K * Golden, 1.0);
					// (across the band: more toward its middle, out to 25 degrees either side)
					const double U = 2.0 * HashOf(static_cast<uint64_t>(K) * 6364136223846793005ull + 11ull) - 1.0;
					const double B = 25.0 * U * std::abs(U);
					const double Bright = MilkyAt(L, B);
					if (Bright <= 0.02 || HashOf(static_cast<uint64_t>(K) * 2862933555777941757ull + 29ull) > Bright)
					{
						continue;
					}
					Speck One{ 0.0, 0.0, Bright, static_cast<uint64_t>(K) };
					FromGalactic(L, B, One.RaHours, One.DecDeg);
					Out.push_back(One);
				}
				return Out;
			}();
			return Band;
		}
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

		// The Milky Way, on a dark night only: the unresolved stars of the galaxy's own plane. Each speck is a square
		// like a star's, a little wider, so together they read as a glow and not as dots. The specks are drawn once,
		// in the galaxy's own frame (Specks), and only turned to the sky here, so the band never crawls between cuts
		// and costs nothing to work out again.
		Out.MilkyShare = Smooth(4.4, 5.4, Out.LimitMag);
		if (Out.MilkyShare > 0.0)
		{
			for (const Speck& Sp : BandSpecks())
			{
				const double Bright = Sp.Bright;
				double Up = 0.0, From = 0.0, Carried = 0.0, CarriedDec = 0.0;
				Precess(Sp.RaHours, Sp.DecDeg, W.Year + (W.Month - 0.5) / 12.0, Carried, CarriedDec);
				ToAltAz(Carried, CarriedDec, W.Lat, Out.SiderealDeg, Up, From);
				if (Up < HazeDeg)
				{
					continue;
				}
				if (Cover > 0.0 && HashOf(Sp.Own * 2654435761ull + 131ull) < Cover)
				{
					continue;
				}
				const Dir Way = AltAz(Up, From);
				if (Out.bMoonUp && DotOf(Way, Out.MoonWay) > MoonGuard)
				{
					continue;
				}
				const double Through = std::pow(10.0, -0.4 * Extinction * (AirMass(Up) - 1.0));
				const double Light = See.Strength * MilkyLight * Bright * Out.MilkyShare * Through;
				Laid One;
				One.Way = Way;
				One.HalfRad = LeastHalf * MilkyWide;
				// (its light a touch warm, as the band's own unresolved stars are)
				One.R = static_cast<float>(Light * 1.00);
				One.G = static_cast<float>(Light * 0.98);
				One.B = static_cast<float>(Light * 0.92);
				Out.Glow.push_back(One);
			}
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

	Mesh BuildMesh(const Sky& S, double Radius, int MoonSegments, int MoonRings, double MoonDarkCut)
	{
		Mesh Out;
		const size_t Squares = S.Stars.size() + S.Glow.size();
		Out.V.reserve((Squares * 4 + 600) * 3);
		Out.C.reserve((Squares * 4 + 600) * 4);
		Out.Triangles.reserve(Squares * 6 + 3000);
		auto Square = [&Out, Radius](const Laid& One)
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
		};
		for (const Laid& One : S.Stars)
		{
			Square(One);
			++Out.StarQuads;
		}
		for (const Laid& One : S.Glow)
		{
			Square(One);
			++Out.GlowQuads;
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
				Out.C.insert(Out.C.end(), { L * Tr, L * Tg, L * Tb, MoonDarkCut <= 0.0 || L >= MoonDarkCut ? 1.0f : 0.0f });
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
