#include "Battle/SeaDayCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace SeaDay
{
	namespace
	{
		constexpr double Ln2 = 0.69314718055994530942;
		constexpr double Deg = Pi / 180.0;
		/** [EST] The sky's tints at a low sun, as multiples of the day's colours: the horizon toward her (gold), away from
		 * her (a dim rose-grey), the sky higher up (a deeper blue); and the night's blue over all of it. */
		constexpr Rgb WarmToward{ 1.38, 0.97, 0.62 }, AwayTint{ 0.72, 0.66, 0.74 }, HighTint{ 0.78, 0.90, 1.12 }, NightTint{ 0.70, 0.82, 1.20 };
		constexpr Rgb White{ 1.0, 1.0, 1.0 };

		double Clamp(double X, double Lo, double Hi) { return X < Lo ? Lo : X > Hi ? Hi : X; }
		/** From A at T 0 to B at T 1, exactly A and B at the ends (and held there). */
		double Lerp(double A, double B, double T) { return T <= 0.0 ? A : T >= 1.0 ? B : A + (B - A) * T; }
		/** From A at T 0 to B at T 1 by equal ratios (both above 0), exactly A and B at the ends. */
		double LogLerp(double A, double B, double T) { return T <= 0.0 ? A : T >= 1.0 ? B : std::exp(Lerp(std::log(A), std::log(B), T)); }
		/** 0 at Low, 1 at High, straight between (either way round). */
		double Ramp(double Low, double High, double X) { return Clamp((X - Low) / (High - Low), 0.0, 1.0); }
		Rgb Scale(const Rgb& C, double S) { return { C.R * S, C.G * S, C.B * S }; }
		Rgb Times(const Rgb& A, const Rgb& B) { return { A.R * B.R, A.G * B.G, A.B * B.B }; }
		Rgb Mix(const Rgb& A, const Rgb& B, double T) { return { Lerp(A.R, B.R, T), Lerp(A.G, B.G, T), Lerp(A.B, B.B, T) }; }
		Rgb Grey(double V) { return { V, V, V }; }
		double Luminance(const Rgb& C) { return 0.2126 * C.R + 0.7152 * C.G + 0.0722 * C.B; }
		bool IsWhite(const Rgb& C) { return C.R == 1.0 && C.G == 1.0 && C.B == 1.0; }

		uint64_t SplitMix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}

		// [TODAY] The scene's colours, each channel the float the scene holds.
		Rgb TodayScreen()
		{
			return { static_cast<float>(SkyOnScreenPixel[0]) / 255.0f, static_cast<float>(SkyOnScreenPixel[1]) / 255.0f, static_cast<float>(SkyOnScreenPixel[2]) / 255.0f };
		}
		Rgb TodaySeaSky()
		{
			const Rgb Still{ static_cast<float>(SeaSkyPixel[0]) / 255.0f, static_cast<float>(SeaSkyPixel[1]) / 255.0f, static_cast<float>(SeaSkyPixel[2]) / 255.0f };
			return LightOfStill(Still, FitRed, FitBlue);
		}
		Rgb TodayHigh()
		{
			// (AAirStage::MovingSea: the sky SetSeaWind was given, a fifth of it and bluer higher up, in float)
			const Rgb Sky = TodaySeaSky();
			return { static_cast<float>(Sky.R) * SkyHighShare[0] * 1.0f, static_cast<float>(Sky.G) * SkyHighShare[1] * 1.0f, static_cast<float>(Sky.B) * SkyHighShare[2] * 1.0f };
		}
		Rgb TodayInk() { return { HazeInkToday[0], HazeInkToday[1], HazeInkToday[2] }; }
		Rgb AirSeaInk() { return { static_cast<float>(SeaHazeInkToday[0]), static_cast<float>(SeaHazeInkToday[1]), static_cast<float>(SeaHazeInkToday[2]) }; }

		/** [TODAY] The haze TakeTheRoom leaves: the room's fog at 0.00045 and 0.006, then BattleLook::AirHaze from 15 km at
		 * twice that, wholly opaque at its most, in HazeInk; the sea material's own haze as MovingSea set it. */
		Haze TodayHaze(double UnitsPerMetre)
		{
			Haze H;
			H.Kind = HazeKind::Today;
			H.Start = HazeStart;
			H.Density = static_cast<float>(HazeThicker * static_cast<double>(RoomDensity));
			H.Falloff = static_cast<float>(static_cast<double>(RoomFalloff));
			H.MostOpacity = static_cast<float>(1.0);
			H.Colour = TodayInk();
			H.Beta = BetaOfDensity(H.Density, UnitsPerMetre);
			return H;
		}

		const std::vector<double>& EyesOf(const std::vector<double>& Asked)
		{
			static const std::vector<double> Planned(std::begin(PlannedEyes), std::end(PlannedEyes));
			return Asked.empty() ? Planned : Asked;
		}

		const char* KindName(BodyKind K) { return K == BodyKind::Sun ? "the sun" : K == BodyKind::Moon ? "the moon" : "the night's glow overhead"; }

		/** The scene's own numbers: as given, else the built-in ones. */
		TodayScene SceneOf(const TodayScene& Given, double UnitsPerMetre) { return Given.bGiven ? Given : BuiltInScene(UnitsPerMetre); }
	}

	double Ease(double From, double To, double X)
	{
		if (From == To) return X >= To ? 1.0 : 0.0;
		const double S = Clamp((X - From) / (To - From), 0.0, 1.0);
		return S * S * (3.0 - 2.0 * S);
	}

	double SeedHour(uint64_t Seed, float AskedHour)
	{
		// (OpenOn's own: 8 in the morning to 3 in the afternoon by the seed; "-TheaterSeaHour=" from 5 to 19)
		double Hour = SeedHourFrom + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;
		if (AskedHour >= static_cast<float>(AskedHourLeast) && AskedHour <= static_cast<float>(AskedHourMost)) Hour = AskedHour;
		return Hour;
	}

	void SunAt(double Latitude, int Month0, double Hour, double& OutFrom, double& OutUp)
	{
		// (SeaParts::SunAt, step for step: degrees to radians as times pi / 180; the scene's own arc tangent is the engine's,
		// so seed mode takes the stage's sun as the scene worked it out)
		const double ToRad = Pi / 180.0, ToDeg = 180.0 / Pi;
		const double Day = 30.4 * std::clamp(Month0, 0, 11) + 15.0;
		const double Decl = 23.44 * ToRad * std::sin(2.0 * Pi * (284.0 + Day) / 365.0);
		const double Phi = Clamp(Latitude, -80.0, 80.0) * ToRad;
		const double H = (15.0 * (Hour - 12.0)) * ToRad;
		const double SinUp = std::sin(Phi) * std::sin(Decl) + std::cos(Phi) * std::cos(Decl) * std::cos(H);
		OutUp = std::asin(Clamp(SinUp, -1.0, 1.0)) * ToDeg;
		const double Bearing = std::atan2(-std::sin(H) * std::cos(Decl), std::sin(Decl) * std::cos(Phi) - std::cos(Decl) * std::sin(Phi) * std::cos(H));
		OutFrom = std::fmod(Bearing * ToDeg + 360.0, 360.0);
	}

	void SeedSun(uint64_t Seed, double Lat, int Month0, float AskedHour, double& OutFrom, double& OutUp)
	{
		SunAt(Lat, Month0, SeedHour(Seed, AskedHour), OutFrom, OutUp);
		OutUp = OutUp < ClampLow ? ClampLow : OutUp < ClampHigh ? OutUp : ClampHigh;
	}

	double EveningFrom(double Lat, int Month0, double Up)
	{
		double From = 0.0, Noon = 0.0, Midnight = 0.0;
		SunAt(Lat, Month0, 12.0, From, Noon);
		SunAt(Lat, Month0, 24.0, From, Midnight);
		double Hour = 24.0;
		if (Up >= Noon)
		{
			Hour = 12.0;
		}
		else if (Up > Midnight)
		{
			// (from noon to midnight she only sinks)
			double Lo = 12.0, Hi = 24.0;
			for (int K = 0; K < 60; ++K)
			{
				const double Mid = 0.5 * (Lo + Hi);
				double MidFrom = 0.0, MidUp = 0.0;
				SunAt(Lat, Month0, Mid, MidFrom, MidUp);
				(MidUp > Up ? Lo : Hi) = Mid;
			}
			Hour = 0.5 * (Lo + Hi);
		}
		double At = 0.0;
		SunAt(Lat, Month0, Hour, From, At);
		return From;
	}

	double DaylightOf(double SunUp) { return Ease(-6.0, 10.0, SunUp); }

	Rgb LightOfStill(const Rgb& Still, double GradeRed, double GradeBlue)
	{
		// (SeaParts::LightOfStill, step for step: the still's pixel linearised, the fitted ACES curve solved for its one
		// root above 0, the 1.58 of the light and the grade's gain undone; each channel the float the scene makes)
		auto One = [](double V, double Gain)
		{
			V = Clamp(V, 0.0, 0.999);
			const double Shown = V <= 0.04045 ? V / 12.92 : std::pow((V + 0.055) / 1.055, 2.4);
			const double A = 2.43 * Shown - 2.51, B = 0.59 * Shown - 0.03, C = 0.14 * Shown;
			const double X = (-B - std::sqrt(std::max(B * B - 4.0 * A * C, 0.0))) / (2.0 * A);
			return static_cast<double>(static_cast<float>(X / 1.58 / Gain));
		};
		const double Red = 1.01 * GradeRed / FitRed, Blue = 0.87 * GradeBlue / FitBlue;
		return { One(static_cast<float>(Still.R), std::max(Red, 0.1)), One(static_cast<float>(Still.G), 0.97), One(static_cast<float>(Still.B), std::max(Blue, 0.1)) };
	}

	Rgb StillOf(const Rgb& Light, double GradeRed, double GradeBlue)
	{
		auto One = [](double L, double Gain)
		{
			const double X = std::max(L, 0.0) * 1.58 * Gain;
			const double Shown = X * (2.51 * X + 0.03) / (X * (2.43 * X + 0.59) + 0.14);
			const double V = Shown <= 0.0031308 ? 12.92 * Shown : 1.055 * std::pow(Shown, 1.0 / 2.4) - 0.055;
			return Clamp(V, 0.0, 1.0);
		};
		const double Red = std::max(1.01 * GradeRed / FitRed, 0.1), Blue = std::max(0.87 * GradeBlue / FitBlue, 0.1);
		return { One(Light.R, Red), One(Light.G, 0.97), One(Light.B, Blue) };
	}

	Rgb KelvinColour(double Kelvin)
	{
		// (the engine's FLinearColor::MakeFromColorTemperature: the Planckian locus in CIE 1960 UCS, Krystek's fit, to
		// XYZ at luminance 1, then BT.709)
		const double T = Clamp(Kelvin, 1000.0, 15000.0);
		const double U = (0.860117757 + 1.54118254e-4 * T + 1.28641212e-7 * T * T) / (1.0 + 8.42420235e-4 * T + 7.08145163e-7 * T * T);
		const double V = (0.317398726 + 4.22806245e-5 * T + 4.20481691e-8 * T * T) / (1.0 - 2.89741816e-5 * T + 1.61456053e-7 * T * T);
		const double X = 3.0 * U / (2.0 * U - 8.0 * V + 4.0);
		const double Y = 2.0 * V / (2.0 * U - 8.0 * V + 4.0);
		const double Z = 1.0 - X - Y;
		const double Lx = X / Y, Ly = 1.0, Lz = Z / Y;
		return { 3.2404542 * Lx - 1.5371385 * Ly - 0.4985314 * Lz, -0.9692660 * Lx + 1.8760108 * Ly + 0.0415560 * Lz, 0.0556434 * Lx - 0.2040259 * Ly + 1.0572252 * Lz };
	}

	Way TowardOf(double Up, double From)
	{
		const double U = Up * Deg, F = From * Deg;
		return { std::sin(F) * std::cos(U), std::cos(F) * std::cos(U), std::sin(U) };
	}

	double FogSeen(const Fog& F, double EyeZ, double TargetZ, double Across)
	{
		// [ENGINE] HeightFogCommon.ush (GetExponentialHeightFog, CalculateLineIntegralShared) with FogRendering.cpp's
		// parameters: the component's FogDensity and FogHeightFalloff a thousandth each, the density at the eye
		// Density x 2^-(Falloff (EyeZ - HeightZ)) (its power held to -125..126), or at the start's crossing; the line
		// integral (1 - 2^-f)/f over the ray's rise f = Falloff x dZ, its Taylor ln 2 - (ln 2)^2 f / 2 within 0.01 of 0
		// (FLT_EPSILON2); what is let through 2^-(that x the ray's length), never under 1 - FogMaxOpacity.
		const double Dz = TargetZ - EyeZ;
		const double Length = std::sqrt(Across * Across + Dz * Dz);
		if (Length <= 0.0) return 1.0;
		const double DensityU = F.Density / 1000.0, FalloffU = F.Falloff / 1000.0;
		double RayLength = Length, RayZ = Dz, Origin = 0.0;
		if (F.Start > 0.0)
		{
			const double Cut = F.Start / Length;
			if (Cut >= 1.0) return 1.0;
			const double CutZ = EyeZ + Cut * Dz;
			RayLength = (1.0 - Cut) * Length;
			RayZ = TargetZ - CutZ;
			Origin = DensityU * std::exp2(-std::max(-127.0, FalloffU * (CutZ - F.HeightZ)));
		}
		else
		{
			Origin = DensityU * std::exp2(Clamp(-FalloffU * (EyeZ - F.HeightZ), -125.0, 126.0));
		}
		const double Rise = std::max(-127.0, FalloffU * RayZ);
		const double Line = std::fabs(Rise) > 0.01 ? (1.0 - std::exp2(-Rise)) / Rise : Ln2 - 0.5 * Ln2 * Ln2 * Rise;
		const double Seen = Clamp(std::exp2(-(Origin * Line * RayLength)), 0.0, 1.0);
		return std::max(Seen, 1.0 - F.MostOpacity);
	}

	double DensityOfBeta(double Beta, double UnitsPerMetre)
	{
		// (a level ray: 2^-(Density/1000 x ln 2 x its length in units), so e^-(Density/1000 x (ln 2)^2 x units a metre))
		return 1000.0 * Beta / (Ln2 * Ln2 * UnitsPerMetre);
	}

	double BetaOfDensity(double Density, double UnitsPerMetre) { return Density / 1000.0 * Ln2 * Ln2 * UnitsPerMetre; }

	FogFit FogFor(double Beta, const std::vector<double>& Eyes, double Range, double FogZ, double UnitsPerMetre, double Cmin)
	{
		const double Unit = UnitsPerMetre, Height = FogZ * Unit;
		// (the density at the sea held at Beta's, whatever the falloff: the component's own is at its height)
		const double AtSea = DensityOfBeta(Beta, Unit);
		auto FogAt = [&](double Falloff)
		{
			Fog F;
			F.Falloff = Falloff;
			F.HeightZ = Height;
			F.Density = AtSea * std::exp2(-Falloff / 1000.0 * Height);
			return F;
		};
		auto Worst = [&](const Fog& F)
		{
			double W = 1.0;
			for (const double Eye : Eyes) W = std::min(W, FogSeen(F, Eye * Unit, 0.0, Range * Unit));
			return W;
		};
		FogFit Out;
		Fog F = FogAt(0.0);
		if (Worst(F) < Cmin)
		{
			// (the least falloff that keeps every planned ray at Cmin: the high eyes' rays climb into thinner air)
			double Lo = 0.0, Hi = 1.0e-4;
			while (Worst(FogAt(Hi)) < Cmin && Hi < 1.0) Hi *= 2.0;
			for (int K = 0; K < 60; ++K)
			{
				const double Mid = 0.5 * (Lo + Hi);
				(Worst(FogAt(Mid)) < Cmin ? Lo : Hi) = Mid;
			}
			F = FogAt(Hi);
			Out.bBand = true;
			// (past any band a level sea would keep: thin it to the worst ray)
			const double W = Worst(F);
			if (W < Cmin && W > 0.0) F.Density *= std::log(Cmin) / std::log(W);
		}
		Out.Density = F.Density;
		Out.Falloff = F.Falloff;
		Out.WorstSeen = Worst(F);
		return Out;
	}

	namespace
	{
		/** The air's own extinction (natural, a metre): clear (a 25 km meteorological range, the 2% contrast range),
		 * thickened by rain, by a low deck or squalls, and by spray in a strong wind. */
		double OwnAirBeta(const HazeAsk& A)
		{
			const bool bLow = A.Kind == "deck" || A.Kind == "squalls";
			const double Rain = Clamp(A.Rain, 0.0, 1.0), Cover = Clamp(A.Cover, 0.0, 1.0);
			const double Range = ClearAirRange / ((1.0 + 15.0 * std::pow(Rain, 1.3)) * (1.0 + (bLow ? 0.5 * Cover : 0.0)) * (1.0 + std::max(0.0, A.Wind - 10.0) / 10.0));
			return -std::log(0.02) / Range;
		}

		/** The least a hull's silhouette (HullOverSky of the horizon, the sea SeaOverSky of it) may be let through the air
		 * and keep Cmin against the sea and the sky. */
		double LeastFor(const HazeAsk& A, double Cmin, double HullOverSky)
		{
			const double Gap = std::fabs(A.SeaOverSky - HullOverSky);
			return std::max(Cmin / std::max(Gap + Cmin * (1.0 - A.SeaOverSky), 1e-9), Cmin / std::max(std::fabs(1.0 - HullOverSky), 1e-9));
		}

		/** What the worst planned eye's ray lets through to a ship at Sees (and a little beyond), by the engine's line
		 * integral. */
		double WorstAt(const Fog& G, const HazeAsk& A, double Sees)
		{
			double W = 1.0;
			for (const double Eye : EyesOf(A.Eyes)) W = std::min(W, FogSeen(G, Eye * A.UnitsPerMetre, 0.0, Sees * 1.02 * A.UnitsPerMetre));
			return W;
		}

		/** Thinned only where it would hide what the rules say is seen: a hull's silhouette at Sees keeps Cmin against the
		 * sea and the sky from every planned eye. True if it had to be. */
		bool ThinFor(Fog& F, const HazeAsk& A, double Cmin, double HullOverSky, double Sees)
		{
			const double Unit = A.UnitsPerMetre, Height = A.FogZ * Unit;
			const double Least = LeastFor(A, Cmin, HullOverSky);
			double W = WorstAt(F, A, Sees);
			if (W >= Least) return false;
			if (Least >= 1.0)
			{
				// (no air would do: the clearest the engine keeps, 1e-7 a metre)
				F.Density = DensityOfBeta(1.0e-7, Unit) * std::exp2(-F.Falloff / 1000.0 * Height);
			}
			else
			{
				// (the optical depth is the density's times the ray's own: thinned so the worst ray keeps Least)
				while (W <= 1e-300)
				{
					F.Density *= 1e-3;
					W = WorstAt(F, A, Sees);
				}
				if (W < Least) F.Density *= std::log(Least) / std::log(W);
			}
			return true;
		}

		/** The night's own air (the sun far under): OwnAirBeta's; then thinned only where it would hide what the rules say
		 * is seen: a hull's silhouette at vis, or at the show's own clear night's 4 km where they give none, keeps Cmin
		 * against the sea and the sky from every planned eye (NightSight checks it with the night's own light). The rules'
		 * vis bounds the night's air; it never makes it. */
		void NightAir(const HazeAsk& A, double Cmin, Haze& H)
		{
			const double Unit = A.UnitsPerMetre, Height = A.FogZ * Unit;
			const double Own = OwnAirBeta(A);
			Fog F;
			F.Falloff = RoomFalloff;
			F.HeightZ = Height;
			F.Density = DensityOfBeta(Own, Unit) * std::exp2(-F.Falloff / 1000.0 * Height);
			H.NightOwnBeta = Own;
			H.bNightThinned = ThinFor(F, A, Cmin, A.HullOverSky, A.Vis > 0.0 ? A.Vis : ClearNightSees);
			H.NightDensity = F.Density;
			H.NightFalloff = F.Falloff;
			H.NightBeta = BetaOfDensity(F.Density * std::exp2(F.Falloff / 1000.0 * Height), Unit);
		}

		/** By day with no vis: the air scene's sea haze (AAirBattle: from 30 km, 1.2 times the room's), and the weather's
		 * own air added to it, what its rain, low deck and spray add to clear air's extinction (nothing under a clear or a
		 * fair sky). The density is the two's at the sea; the start the air scene's 30 km times the square of its share of
		 * the air, so the weather's air is from near the eye where it is most of the air (a few hundred metres out in a
		 * squall). Thinned as the night's air is, only where a hull lit by day at Show::Hunt's 4 km would lose her
		 * silhouette from a planned eye, by the weather's part alone (the air scene's never). Its density at the fog's
		 * height (the engine's) and its start (metres). */
		Fog DayAir(const HazeAsk& A, double Cmin, Haze& H)
		{
			const double Unit = A.UnitsPerMetre, Height = A.FogZ * Unit;
			const double AirSea = static_cast<float>(AirSeaThicker * static_cast<double>(RoomDensity));
			auto DayFog = [&](double Weather)
			{
				Fog F;
				F.Falloff = RoomFalloff;
				F.HeightZ = Height;
				F.Density = Weather > 0.0 ? AirSea + Weather : AirSea;
				const double Share = AirSea / F.Density;
				F.Start = Weather > 0.0 ? AirSeaStart * (Share * Share) : AirSeaStart;
				return F;
			};
			HazeAsk Clear = A;
			Clear.Rain = 0.0;
			Clear.Cover = 0.0;
			Clear.Kind.clear();
			Clear.Wind = 0.0;
			const double Own = OwnAirBeta(A) - OwnAirBeta(Clear);
			H.WeatherOwnBeta = Own;
			if (Own <= 0.0) return DayFog(0.0);
			double Weather = DensityOfBeta(Own, Unit) * std::exp2(-RoomFalloff / 1000.0 * Height);
			const double Least = LeastFor(A, Cmin, HullOverSkyDay);
			auto Worst = [&](double W)
			{
				Fog G = DayFog(W);
				G.Start *= Unit;
				return WorstAt(G, A, ClearNightSees);
			};
			if (Worst(Weather) < Least)
			{
				// (the light let through only grows as the weather's part thins: the least thinning that keeps Least, by halves)
				H.bWeatherThinned = true;
				double Lo = 0.0, Hi = Weather;
				for (int K = 0; K < 60; ++K)
				{
					const double Mid = 0.5 * (Lo + Hi);
					(Worst(Mid) >= Least ? Lo : Hi) = Mid;
				}
				Weather = Lo;
			}
			H.WeatherBeta = BetaOfDensity(Weather * std::exp2(RoomFalloff / 1000.0 * Height), Unit);
			return DayFog(Weather);
		}
	}

	Haze HazeFor(const HazeAsk& A)
	{
		Haze H;
		const double Unit = A.UnitsPerMetre, Height = A.FogZ * Unit, Cmin = Clamp(A.Cmin, 1e-4, 0.5);
		NightAir(A, Cmin, H);
		const double NightDensity = H.NightDensity, NightFalloff = H.NightFalloff;
		H.MostOpacity = 1.0;
		if (A.Vis > 0.0)
		{
			// From the horizon up vis is the fog: a ship at the rules' vis keeps Cmin, from every planned eye. It comes in
			// over the twilight as the sun climbs from -6 degrees to the horizon, its density by equal ratios and its
			// falloff straight (the plan's ruling: the rules' vis by day comes before the tenth a minute there).
			const double DayBeta = -std::log(Cmin) / A.Vis;
			const FogFit Fit = FogFor(DayBeta, EyesOf(A.Eyes), A.Vis, A.FogZ, Unit, Cmin);
			const double W = Ramp(VisFogLowest, VisFogFull, A.SunUp);
			H.Kind = W > 0.0 ? HazeKind::Vis : HazeKind::Night;
			H.Vis = A.Vis;
			H.VisShare = W;
			H.Start = 0.0;
			H.Density = LogLerp(NightDensity, Fit.Density, W);
			H.Falloff = Lerp(NightFalloff, Fit.Falloff, W);
			H.BandFalloff = Fit.Falloff;
			H.WorstSeen = Fit.WorstSeen;
			H.bSun = true;
			// (one haze for the sea and the ships: the fog's, from the eye; the material's own made nothing)
			H.bMaterialToday = false;
			H.MaterialTop = 0.0;
			H.MaterialReach = MaterialReachNever / Unit;
		}
		else
		{
			// No vis: by day the air scene's sea haze with the weather's own air added where the field's sky thickens it
			// (DayAir: a squall the same air by day as by night, but for the clear air under it); the night's own air at
			// night. Nothing comes before the tenth here, so the two meet over the arc it needs, from -18 degrees to +2: the
			// density by equal ratios, the start out from the eye as the cube of the way, going out as the air thins.
			const double W = Ramp(AirSeaFrom, AirSeaFull, A.SunUp);
			const Fog Day = DayAir(A, Cmin, H);
			H.Kind = W >= 1.0 ? HazeKind::AirSea : HazeKind::Night;
			H.Start = Day.Start * (W * W * W);
			H.Density = LogLerp(NightDensity, Day.Density, W);
			H.Falloff = Lerp(NightFalloff, static_cast<double>(RoomFalloff), W);
			H.bSun = false;
			// (the material's own haze the scene's by day, made nothing at -18 degrees and below, where the night's air
			// begins at the eye; between, its reach eased by equal ratios from MaterialReachFar, where it is under a
			// hundredth anywhere to the sea's edge)
			H.bMaterialToday = W >= 1.0;
			H.MaterialTop = W > 0.0 ? A.MaterialTop : 0.0;
			H.MaterialReach = W > 0.0 ? LogLerp(MaterialReachFar, A.MaterialReach, W) : MaterialReachNever / Unit;
		}
		H.Beta = BetaOfDensity(H.Density * std::exp2(H.Falloff / 1000.0 * Height), Unit);
		return H;
	}

	double MoonKeyOf(double Up, double Lit)
	{
		if (Up <= 0.0 || Lit <= 0.0) return 0.0;
		return KeyDay * MoonFull * Clamp(Lit, 0.0, 1.0) * std::sqrt(std::sin(Up * Deg));
	}

	namespace
	{
		/** The night's own lamps, the sun far under: the moon if she is up and lit, else the glow overhead. */
		struct NightOwn
		{
			BodyKind Kind = BodyKind::Glow;
			double Up = 90.0;       // the key's height
			double Lamp = 0.0;      // the key lamp's intensity, under the cover
			double OnWater = 0.0;   // its light on the level sea
			double MoonShare = 0.0; // her own light: her key's share of the day's (the sky's colours add it to the glow's)
			double Lift = 0.0;      // what the sky light and the fills add to the glow's share: the larger of her own and
			                        // the glow's key she replaces (the night's floor, the lamps' only)
			double SkyShare = 0.0;  // the share of the day's the sky light and the fills keep (before the cover's lift)
			double ColourShare = 0.0; // the share of the day's the sky's colours keep: the glow's and her own
			double OnSea = 0.0;     // all of it on the level sea (OnSea's measure)
		};

		NightOwn NightOf(bool bMoon, double MoonUp, double MoonLit, double KeyCover, double SkyLift)
		{
			const double Glow = KeyDay * GlowShare, Ambient = SkyDay + FillsOnSea * FillDay;
			NightOwn N;
			if (bMoon)
			{
				// (her key; the sky light and the fills by her own light and the glow's, which does not go out when she rises
				// (so a hull's lit side stays a silhouette against the sea her sky lights); and never less light on the level sea
				// than the moonless night's, however low or thin she is: the glow's key she replaces goes into them. The sky's
				// colours take her own light only.)
				const double Key = MoonKeyOf(MoonUp, MoonLit), Sine = std::sin(MoonUp * Deg);
				N.Kind = BodyKind::Moon;
				N.Up = MoonUp;
				N.Lamp = Key * KeyCover;
				N.OnWater = N.Lamp * Sine;
				N.MoonShare = Key / KeyDay;
				N.Lift = std::max(N.MoonShare, (Glow * KeyCover - N.OnWater) / (SkyLift * Ambient));
			}
			else
			{
				N.Lamp = Glow * KeyCover;
				N.OnWater = N.Lamp;
			}
			N.SkyShare = GlowShare + N.Lift;
			N.ColourShare = GlowShare + N.MoonShare;
			N.OnSea = N.OnWater + N.SkyShare * SkyLift * Ambient;
			return N;
		}
	}

	Sight NightSight(double Vis, const NightLamps& N, const Fog& Air, const std::vector<double>& Eyes, double Cmin, double UnitsPerMetre, double HorizonLight, double LeastBias)
	{
		Sight S;
		// (no vis said: the show's own clear night, Show::Hunt's lookout)
		S.Vis = Vis > 0.0 ? Vis : ClearNightSees;
		// The exposure: by the night's light on the level sea, against a clear moonless night's and a clear half moon's 30
		// degrees up, by equal ratios (wide enough that a brighter night is never shown darker); never under LeastBias.
		const double Dark = NightOf(false, 0.0, 0.0, 1.0, 1.0).OnSea, Moonlit = NightOf(true, 30.0, 0.5, 1.0, 1.0).OnSea;
		const double MoonShare = Clamp(std::log(std::max(N.OnSea, 1e-12) / Dark) / std::log(Moonlit / Dark), 0.0, 1.0);
		S.Bias = std::max(DarkBias + (MoonlitBias - DarkBias) * MoonShare, std::min(LeastBias, MostBias));
		// Her silhouette: her lit side by the night's lamps against the horizon's light, through the night's air from every
		// planned eye (the engine's line integral), against the sky and against the sea that mirrors it.
		S.HullShare = HullOverSkyDay * ((N.Key + N.Sky) / (KeyDay + SkyDay)) / std::max(N.Horizon, 1e-12);
		S.Seen = 1.0;
		for (const double Eye : Eyes) S.Seen = std::min(S.Seen, FogSeen(Air, Eye * UnitsPerMetre, 0.0, S.Vis * UnitsPerMetre));
		const double HullFar = S.HullShare * S.Seen + (1.0 - S.Seen), SeaFar = SeaOverSky * S.Seen + (1.0 - S.Seen);
		S.ContrastSky = std::fabs(1.0 - HullFar);
		S.ContrastSea = std::fabs(SeaFar - HullFar) / std::max(SeaFar, 1e-12);
		// On the screen, at the exposure: opened while she would sit within LeastLevels of the sky or the sea.
		auto Levels = [&](double Bias)
		{
			const double Lift = std::exp2(Bias) * HorizonLight * N.Horizon;
			auto Pixel = [&](double Share) { return 255.0 * StillOf(Grey(Share * Lift), FitRed, FitBlue).G; };
			S.LevelsSky = Pixel(1.0) - Pixel(HullFar);
			S.LevelsSea = std::fabs(Pixel(SeaFar) - Pixel(HullFar));
		};
		Levels(S.Bias);
		while ((S.LevelsSky < LeastLevels || S.LevelsSea < LeastLevels) && S.Bias < MostBias - 1e-9)
		{
			S.Bias = std::min(S.Bias + 0.1, MostBias);
			Levels(S.Bias);
		}
		S.bHolds = S.ContrastSky >= Cmin && S.ContrastSea >= Cmin && S.LevelsSky >= LeastLevels && S.LevelsSea >= LeastLevels;
		return S;
	}

	Water WaterNamed(WaterKind Kind)
	{
		// [EST] The sea's paint as a tint of today's, its brightness kept (so the light the art director passed holds),
		// and the light through a crest.
		Water W;
		W.Kind = Kind;
		switch (Kind)
		{
		case WaterKind::Tropic: // deep ultramarine, turquoise in the crests
			W.Name = "tropic";
			W.Body = { 0.74, 0.97, 1.42 };
			W.Crest = { 0.22, 0.86, 0.78 };
			break;
		case WaterKind::Temperate: // the North Sea's green-grey
			W.Name = "temperate";
			W.Body = { 1.00, 1.04, 0.82 };
			W.Crest = { 0.42, 0.74, 0.56 };
			break;
		case WaterKind::North: // the North Atlantic's and the Arctic's slate
			W.Name = "north";
			W.Body = { 0.92, 1.00, 1.06 };
			W.Crest = { 0.38, 0.60, 0.62 };
			break;
		case WaterKind::Med: // the Mediterranean's clear deep blue
			W.Name = "med";
			W.Body = { 0.76, 0.98, 1.36 };
			W.Crest = { 0.24, 0.72, 0.86 };
			break;
		}
		W.Body = Scale(W.Body, 1.0 / Luminance(W.Body));
		return W;
	}

	Water WaterOf(double Lat, double Lon, int Month0)
	{
		// (the Mediterranean and the Black Sea: west of 0 longitude the Mediterranean lies wholly south of 40N, so not the
		// Bay of Biscay; the Black Sea's north-west and the Sea of Azov to 47.5N)
		if (Lat >= 30.0 && Lat <= (Lon >= 27.0 ? 47.5 : 46.0) && Lon >= -6.0 && Lon <= 42.0 && !(Lon < 0.0 && Lat > 40.0)) return WaterNamed(WaterKind::Med);
		// (the shelf seas' green-grey: the North Sea, and the Yellow Sea with the Bohai)
		if (Lat >= 51.0 && Lat <= 61.0 && Lon >= -4.0 && Lon <= 10.0) return WaterNamed(WaterKind::Temperate);
		if (Lat >= 34.0 && Lat <= 41.0 && Lon >= 117.0 && Lon <= 127.0) return WaterNamed(WaterKind::Temperate);
		// (the tropics, and the subtropical open ocean to 40 degrees, the clearest deep blue there is: the tropics' paint)
		if (std::fabs(Lat) < 40.0) return WaterNamed(WaterKind::Tropic);
		// (the north, by the winter half year's line)
		const int M = std::clamp(Month0, 0, 11);
		const bool bWinter = Lat >= 0.0 ? (M >= 10 || M <= 3) : (M >= 4 && M <= 9);
		return WaterNamed(std::fabs(Lat) >= (bWinter ? 45.0 : 50.0) ? WaterKind::North : WaterKind::Temperate);
	}

	bool WaterKindOf(const std::string& Name, WaterKind& Out)
	{
		const std::pair<const char*, WaterKind> Names[] = { { "tropic", WaterKind::Tropic }, { "temperate", WaterKind::Temperate }, { "north", WaterKind::North }, { "med", WaterKind::Med } };
		for (const auto& N : Names)
		{
			if (Name == N.first)
			{
				Out = N.second;
				return true;
			}
		}
		return false;
	}

	float MaterialHazed(float Depth, float Reach)
	{
		const float Over = Depth / Reach;
		const float Square = Over * Over;
		return Square / (Square + 1.0f);
	}

	TodayScene BuiltInScene(double UnitsPerMetre)
	{
		TodayScene S;
		S.bGiven = true;
		S.SkyOnScreen = TodayScreen();
		S.SeaSkyLight = TodaySeaSky();
		S.SkyHigh = TodayHigh();
		S.MaterialTop = MaterialTopToday;
		S.MaterialReach = MaterialReachToday;
		const Haze H = TodayHaze(UnitsPerMetre);
		S.FogStart = H.Start;
		S.FogDensity = H.Density;
		S.FogFalloff = H.Falloff;
		S.FogMostOpacity = H.MostOpacity;
		S.FogColour = H.Colour;
		return S;
	}

	namespace
	{
		/** The light on the level sea: the key's lamp times the sine of its height (nothing from under the horizon), the sky
		 * light and the fills. */
		double OnSeaOf(double KeyLight, const Way& Toward, double SkyLight, double FillLight) { return KeyLight * std::max(0.0, Toward.Z) + SkyLight + FillsOnSea * FillLight; }

		/** NightSight on the night's own lamps and air (the same for a night and for the moonless night it is held to). */
		struct NightSeen
		{
			NightLamps Lamps;
			Haze Air;
			Fog NightFog;
		};
		NightSeen NightSeenOf(const NightOwn& N, double SkyLift, const Rgb& SeaSky, HazeAsk HA)
		{
			NightSeen Out;
			Out.Lamps.Key = N.Lamp;
			Out.Lamps.Sky = SkyDay * N.SkyShare * SkyLift;
			Out.Lamps.OnSea = N.OnSea;
			// (the horizon the look shows at night, its SeaSkyLight: the scene's in the night's blue at the sky's colours'
			// share, the glow's and the moon's own; the cover lifts the sky light and the fills, its bounce, not the horizon's
			// colour, and the night's floor lifts the lamps only)
			Out.Lamps.Horizon = N.ColourShare * Luminance(Times(SeaSky, NightTint)) / std::max(Luminance(SeaSky), 1e-12);
			HA.HullOverSky = HullOverSkyDay * ((Out.Lamps.Key + Out.Lamps.Sky) / (KeyDay + SkyDay)) / std::max(Out.Lamps.Horizon, 1e-12);
			Out.Air = HazeFor(HA);
			Out.NightFog.Density = Out.Air.NightDensity;
			Out.NightFog.Falloff = Out.Air.NightFalloff;
			Out.NightFog.HeightZ = HA.FogZ * HA.UnitsPerMetre;
			return Out;
		}
	}

	Look LookOf(const Field& F, const StageSun& Stage, const Asked& A, const TodayScene& Given)
	{
		Look L;
		L.Day = A.Day;
		const double Unit = A.UnitsPerMetre;
		const TodayScene Scene = SceneOf(Given, Unit);
		if (A.Water >= 0 && A.Water <= static_cast<int>(WaterKind::Med))
		{
			L.Sea = WaterNamed(static_cast<WaterKind>(A.Water));
			L.Sea.bAsked = true;
		}
		else
		{
			L.Sea = WaterOf(F.Lat, F.Lon, F.Month);
		}
		if (A.Day == Mode::Seed)
		{
			// TODAY: the stage's sun as the scene worked it out, and its sky, haze and sea material as it holds them.
			L.Source = SunSource::Stage;
			L.Hour = SeedHour(F.Seed, A.AskedHour);
			L.SunUp = Stage.Up;
			L.SunFrom = Stage.From;
			L.Key = { BodyKind::Sun, Stage.Up, Stage.From, TowardOf(Stage.Up, Stage.From) };
			L.Daylight = Clamp(DaylightOf(Stage.Up) * Clamp(L.KeyLight / KeyDay, 0.2, 1.0), 0.0, 1.0);
			L.SkyOnScreen = Scene.SkyOnScreen;
			L.SeaSkyLight = Scene.SeaSkyLight;
			L.SkyHigh = Scene.SkyHigh;
			L.SkyToward = L.SeaSkyLight;
			L.SkyAway = L.SeaSkyLight;
			L.Air.Kind = HazeKind::Today;
			L.Air.Start = Scene.FogStart;
			L.Air.Density = Scene.FogDensity;
			L.Air.Falloff = Scene.FogFalloff;
			L.Air.MostOpacity = Scene.FogMostOpacity;
			L.Air.Colour = Scene.FogColour;
			L.Air.Beta = BetaOfDensity(L.Air.Density, Unit);
			L.Air.bMaterialToday = true;
			L.Air.MaterialTop = Scene.MaterialTop;
			L.Air.MaterialReach = Scene.MaterialReach;
			return L;
		}

		// FIELD. The sun: the field's light, else its hour (or the asked one, or the seed's) through SunAt, unclamped.
		double Up = 0.0, From = 0.0;
		const bool bAskedHour = A.AskedHour >= static_cast<float>(AskedHourLeast) && A.AskedHour <= static_cast<float>(AskedHourMost);
		if (F.bLight)
		{
			L.Source = SunSource::FieldLight;
			L.Hour = F.bHour ? F.Hour : -1.0;
			Up = F.SunUp;
			double HourUp = 0.0; // (the hour's own height: the field's light gives hers)
			if (F.bSunFrom) From = F.SunFrom;
			else if (F.bHour) SunAt(F.Lat, F.Month, F.Hour, From, HourUp);
			else From = EveningFrom(F.Lat, F.Month, Up);
		}
		else
		{
			L.Source = bAskedHour || !F.bHour ? SunSource::SeedHour : SunSource::FieldHour;
			L.Hour = bAskedHour ? static_cast<double>(A.AskedHour) : F.bHour ? F.Hour : SeedHour(F.Seed);
			SunAt(F.Lat, F.Month, L.Hour, From, Up);
		}
		(void)Stage; // (field mode aims away from the clamped sun)
		L.SunUp = Up;
		L.SunFrom = From;
		L.MoonUp = F.bLight ? F.MoonUp : -90.0;
		L.MoonFrom = F.bLight ? F.MoonFrom : 0.0;
		L.MoonLit = F.bLight ? Clamp(F.MoonLit, 0.0, 1.0) : 0.0;
		// (no moon without light: a light{} that names none, read as an unlit moon, is keyed by the glow)
		const bool bMoon = F.bLight && F.MoonUp > 0.0 && L.MoonLit > 0.0;

		// The key: the sun's lamp while she is above -4 degrees, the glow's at -10 to the day's at 4 by equal ratios (under
		// the horizon she lights nothing on the water); below -4 the night's own body, faded in from nothing at -4 to its
		// own strength at -10, so the switch falls where the key puts nothing on the water and the dusk is the sky light's.
		const double Glow = KeyDay * GlowShare;
		const double Cover = F.Sky.bGiven ? Clamp(F.Sky.Cover, 0.0, 1.0) : 0.0, Rain = F.Sky.bGiven ? Clamp(F.Sky.Rain, 0.0, 1.0) : 0.0;
		const double KeyCover = (1.0 - (1.0 - OvercastKey) * Cover) * (1.0 - RainKey * Rain);
		const double SkyLift = (1.0 + OvercastSky * Cover) * (1.0 - RainSky * Rain);
		const NightOwn Night = NightOf(bMoon, F.MoonUp, L.MoonLit, KeyCover, SkyLift);
		// (the night's own body: nothing at -4 degrees and above, wholly in by -10)
		const double Fade = 1.0 - Ramp(KeyRampLow, SunKeyLowest, Up);
		if (Up > SunKeyLowest)
		{
			L.KeyLight = LogLerp(Glow, KeyDay, Ramp(KeyRampLow, KeyRampHigh, Up)) * KeyCover;
			L.Key = { BodyKind::Sun, Up, From, TowardOf(Up, From) };
		}
		else
		{
			L.KeyLight = Night.Lamp * Fade;
			if (Night.Kind == BodyKind::Moon) L.Key = { BodyKind::Moon, F.MoonUp, F.MoonFrom, TowardOf(F.MoonUp, F.MoonFrom) };
			else L.Key = { BodyKind::Glow, 90.0, 0.0, Way{ 0.0, 0.0, 1.0 } };
		}
		L.KeyShare = L.KeyLight / KeyDay;
		// (warmth: exactly today's above 12 degrees, easing to 6,500 K by 6, where the atmosphere's own transmittance, on
		// per pixel, reddens her; the night's 8,000 K as the dusk deepens)
		if (L.Key.Kind == BodyKind::Sun) L.Kelvin = Up >= KelvinEaseHigh ? KelvinDay : Lerp(TwilightKelvin, KelvinDay, Ease(KelvinEaseLow, KelvinEaseHigh, Up));
		else L.Kelvin = Lerp(TwilightKelvin, NightKelvin, Ease(SunKeyLowest, KeyRampLow, Up));
		L.bPerPixelTransmittance = true;

		// The sky: the dusk's share of the day's, the glow's at -16 degrees and below to the day's at 6 by equal ratios,
		// whatever the moon; a moon adds her own light to it, faded in under -4 as her key is, so she moves nothing from -4
		// up and the night's sky by her own light alone. The sky light and the fills (the room's fills stand for the sky's
		// bounce) add the night's floor instead, the larger of her own light and the glow's key she replaces, so the light
		// on the level sea is never less than the moonless night's at the same sun; overcast lifts them. The floor is the
		// lamps' only, never the sky's colours.
		const double Dusk = LogLerp(GlowShare, 1.0, Ramp(SkyRampLow, SkyRampHigh, Up));
		const double SkyLevel = Dusk + Fade * Night.MoonShare;
		const double LampLevel = Dusk + Fade * Night.Lift;
		L.SkyShare = LampLevel * SkyLift;
		L.SkyLight = SkyDay * L.SkyShare;
		L.FillShare = L.SkyShare;
		L.FillLight = FillDay * L.FillShare;
		// (what lands on the level sea, and the unlit pieces by it against a clear day's at the same sun, the day's at 6
		// degrees for a lower one: exactly 1 by a clear day; the self-lit ones never scale)
		L.OnSea = OnSeaOf(L.KeyLight, L.Key.Toward, L.SkyLight, L.FillLight);
		const double DayOnSea = KeyDay * std::sin(std::max(Up, UnlitDayLowest) * Deg) + SkyDay + FillsOnSea * FillDay;
		L.UnlitShare = std::min(1.0, L.OnSea / DayOnSea);
		L.SelfLitShare = 1.0;

		// The sky's colours: the scene's by day, gold toward a low sun and rose-grey away from her, the night's blue after.
		const double Gold = Ease(SkyGoldFrom, 1.0, Up) * Ease(-11.0, -2.0, Up), Blue = Ease(-3.0, -13.0, Up);
		const Rgb NightBlue = Mix(White, NightTint, Blue);
		auto Tint = [&](const Rgb& Low) { return Scale(Times(Mix(White, Low, Gold), NightBlue), SkyLevel); };
		const Rgb MeanTint = Tint(Mix(WarmToward, AwayTint, 0.5));
		const Rgb SeaSky = Scene.SeaSkyLight;
		L.SeaSkyLight = Times(SeaSky, MeanTint);
		L.SkyToward = Times(SeaSky, Tint(WarmToward));
		L.SkyAway = Times(SeaSky, Tint(AwayTint));
		L.SkyHigh = Times(Scene.SkyHigh, Tint(HighTint));
		if (Cover > 0.0) L.SkyHigh = Mix(L.SkyHigh, Times(L.SeaSkyLight, Rgb{ 0.95, 0.97, 1.0 }), Cover);
		L.SkyOnScreen = IsWhite(MeanTint) ? Scene.SkyOnScreen : StillOf(Times(LightOfStill(Scene.SkyOnScreen, FitRed, FitBlue), MeanTint), FitRed, FitBlue);

		// The haze, and the night's sight (the night's own lamps and horizon, whatever the hour: the exposure's anchor).
		HazeAsk HA;
		HA.Vis = F.bLight ? F.Seeing : 0.0;
		HA.SunUp = Up;
		HA.Cover = Cover;
		HA.Rain = Rain;
		HA.Wind = F.Wind;
		HA.Kind = F.Sky.bGiven ? F.Sky.Kind : std::string();
		HA.Cmin = A.VisContrast;
		HA.UnitsPerMetre = Unit;
		HA.FogZ = A.FogZ;
		HA.Eyes = EyesOf(A.Eyes);
		HA.MaterialTop = Scene.MaterialTop;
		HA.MaterialReach = Scene.MaterialReach;
		const NightSeen Seen = NightSeenOf(Night, SkyLift, SeaSky, HA);
		L.Air = Seen.Air;
		L.Air.Colour = Times(HA.Vis > 0.0 ? TodayInk() : AirSeaInk(), MeanTint);
		// (the sun-side haze as strong as the lesser of the vis fog's presence and the sun's key: either, not both at once,
		// moves it in a minute)
		if (L.Air.bSun) L.Air.SunColour = Scale(KelvinColour(L.Kelvin), InscatterShare * std::min(L.Air.VisShare, L.KeyShare));
		// (a moon never shows the night darker than her moonless night: her exposure never less than its, less the stops
		// her light adds on the level sea; the night's own, and under -4 degrees the moonless look's at the same sun)
		double LeastBias = 0.0, HereBias = 0.0;
		const bool bFloored = bMoon && Up < SunKeyLowest;
		if (bMoon)
		{
			const NightOwn Dark = NightOf(false, 0.0, 0.0, KeyCover, SkyLift);
			const NightSeen DarkSeen = NightSeenOf(Dark, SkyLift, SeaSky, HA);
			const Sight DarkSight = NightSight(HA.Vis, DarkSeen.Lamps, DarkSeen.NightFog, HA.Eyes, HA.Cmin, Unit, Luminance(SeaSky));
			LeastBias = DarkSight.Bias - std::log2(Night.OnSea / Dark.OnSea);
			if (bFloored)
			{
				// (the moonless look at this sun, as LookOf makes it: the glow's key faded in overhead, the dusk's lamps, and
				// its exposure on the way to its night's)
				const double DarkOnSea = OnSeaOf(Dark.Lamp * Fade, Way{ 0.0, 0.0, 1.0 }, SkyDay * (Dusk * SkyLift), FillDay * (Dusk * SkyLift));
				const double MoonlessBias = Lerp(TwilightBias, DarkSight.Bias, Ramp(SunKeyLowest, KeyRampLow, Up));
				HereBias = MoonlessBias - std::log2(L.OnSea / DarkOnSea);
				if (Up <= KeyRampLow) LeastBias = std::max(LeastBias, HereBias);
			}
		}
		L.Night = NightSight(HA.Vis, Seen.Lamps, Seen.NightFog, HA.Eyes, HA.Cmin, Unit, Luminance(SeaSky), LeastBias);

		// The exposure: 0 by day, 1.2 stops as the sun goes under -4 degrees, the night's by -10 (a moon's never under the
		// moonless look's at the same sun less the stops she adds).
		if (Up >= KeyRampHigh) L.Bias = 0.0;
		else if (Up >= SunKeyLowest) L.Bias = TwilightBias * (KeyRampHigh - Up) / (KeyRampHigh - SunKeyLowest);
		else L.Bias = Lerp(TwilightBias, L.Night.Bias, Ramp(SunKeyLowest, KeyRampLow, Up));
		if (bFloored) L.Bias = std::max(L.Bias, std::min(HereBias, MostBias));
		// (the night grade, through SetSeaWind's grade argument; smoke by the campaign map's night rule)
		const double Graded = Ease(2.0, -10.0, Up);
		L.Saturation = Lerp(1.0, NightSaturation, Graded);
		L.BlueGain = Lerp(1.0, NightBlueGain, Graded);
		L.HourLight = Mix(White, Rgb{ SmokeNight[0], SmokeNight[1], SmokeNight[2] }, 1.0 - DaylightOf(Up));
		L.StarShare = Ease(-4.0, -12.0, Up) * (1.0 - 0.5 * (bMoon ? L.MoonLit : 0.0));
		L.Daylight = Clamp(DaylightOf(Up) * Clamp(L.KeyLight / KeyDay, 0.2, 1.0), 0.0, 1.0);

		// The unfogged dome's moon and stars (step 11f): no disc for a moon with no light.
		L.Moon.bUp = F.bLight && F.MoonUp > -L.Moon.Radius && L.MoonLit > 0.0;
		L.Moon.Up = L.MoonUp;
		L.Moon.From = L.MoonFrom;
		L.Moon.Lit = L.MoonLit;
		L.Moon.Phase = std::acos(Clamp(2.0 * L.MoonLit - 1.0, -1.0, 1.0));
		L.Moon.Toward = TowardOf(L.MoonUp, L.MoonFrom);
		{
			const Way S = TowardOf(Up, From), M = L.Moon.Toward;
			const double Along = S.X * M.X + S.Y * M.Y + S.Z * M.Z;
			Way Across{ S.X - Along * M.X, S.Y - Along * M.Y, S.Z - Along * M.Z };
			const double Size = std::sqrt(Across.X * Across.X + Across.Y * Across.Y + Across.Z * Across.Z);
			L.Moon.BrightToward = Size > 1e-9 ? Way{ Across.X / Size, Across.Y / Size, Across.Z / Size } : Way{ 0.0, 0.0, 1.0 };
		}
		L.Moon.Bright = Scale(LightOfStill(Grey(0.92), FitRed, FitBlue), 1.0 / std::exp2(L.Bias));
		L.Stars.Share = L.StarShare;
		L.Stars.Visible = L.StarShare * (1.0 - Cover);
		L.Stars.Seed = SplitMix(F.Seed * 31ull + 101ull);
		L.Stars.Bright = Scale(LightOfStill(Grey(0.8), FitRed, FitBlue), 1.0 / std::exp2(L.Bias));
		return L;
	}

	std::string Say(const Look& L)
	{
		char Line[1600];
		const char* Source = L.Source == SunSource::Stage ? "the stage's" : L.Source == SunSource::FieldLight ? "the field's light" : L.Source == SunSource::FieldHour ? "the field's hour" : "the seed's hour";
		const char* HazeName = L.Air.Kind == HazeKind::Today ? "today's" : L.Air.Kind == HazeKind::AirSea ? "the air scene's over the sea" : L.Air.Kind == HazeKind::Vis ? "the rules' vis" : "the night's air";
		char Hour[32] = "";
		if (L.Hour >= 0.0) std::snprintf(Hour, sizeof(Hour), ", hour %.2f", L.Hour);
		int N = std::snprintf(Line, sizeof(Line), "Sea day: %s: the sun %.1f deg up from %.0f (%s%s); the key %s at %.3f (%.3f of the day's), %.0f K; fills %.3f, sky light %.3f",
			L.Day == Mode::Seed ? "seed (today's)" : "field", L.SunUp, L.SunFrom, Source, Hour, KindName(L.Key.Kind), L.KeyLight, L.KeyShare, L.Kelvin, L.FillLight, L.SkyLight);
		if (L.Moon.bUp && N > 0 && N < static_cast<int>(sizeof(Line)))
		{
			N += std::snprintf(Line + N, sizeof(Line) - N, "; the moon %.0f deg up from %.0f, %.2f lit", L.MoonUp, L.MoonFrom, L.MoonLit);
		}
		if (N > 0 && N < static_cast<int>(sizeof(Line)))
		{
			N += std::snprintf(Line + N, sizeof(Line) - N, "; exposure %+.2f stops, saturation %.2f, blue %.2f; unlit pieces %.3f; the haze %s from %.0f m, extinction %.2e a metre",
				L.Bias, L.Saturation, L.BlueGain, L.UnlitShare, HazeName, L.Air.Start, L.Air.Beta);
		}
		if (L.Air.Vis > 0.0 && N > 0 && N < static_cast<int>(sizeof(Line)))
		{
			N += std::snprintf(Line + N, sizeof(Line) - N, " (vis %.0f m, %.2f in; at night met by light: a silhouette keeps %.2f against the sky, %.2f against the sea)",
				L.Air.Vis, L.Air.VisShare, L.Night.ContrastSky, L.Night.ContrastSea);
		}
		if (N > 0 && N < static_cast<int>(sizeof(Line)))
		{
			std::snprintf(Line + N, sizeof(Line) - N, "; the water %s%s", L.Sea.Name, L.Sea.bAsked ? " (asked)" : "");
		}
		return Line;
	}
}
