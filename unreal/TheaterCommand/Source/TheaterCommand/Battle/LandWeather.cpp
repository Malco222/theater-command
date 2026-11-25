#include "Battle/LandWeather.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace LandWeather
{
	namespace
	{
		double Num(const Book& Numbers, const char* Path, double Default)
		{
			double V = Default;
			if (Numbers && Numbers(Path, V) && std::isfinite(V)) return V;
			return Default;
		}

		double Ease(double A, double B, double X)
		{
			if (B == A) return X >= B ? 1.0 : 0.0;
			const double T = std::clamp((X - A) / (B - A), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		std::string Lower(const std::string& Text)
		{
			std::string Out;
			for (const char Ch : Text)
			{
				if (!std::isspace(static_cast<unsigned char>(Ch))) Out += static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
			}
			return Out;
		}

		std::string Fmt(const char* Pattern, double A, double B = 0.0)
		{
			char Buf[160];
			std::snprintf(Buf, sizeof(Buf), Pattern, A, B);
			return Buf;
		}
	}

	void ReadForced(const std::string& Text, std::string& Kind, double& Strength)
	{
		Kind.clear();
		Strength = -1.0;
		const std::string T = Lower(Text);
		const size_t Comma = T.find(',');
		Kind = T.substr(0, Comma);
		if (Comma != std::string::npos)
		{
			const std::string Rest = T.substr(Comma + 1);
			if (!Rest.empty()) Strength = std::clamp(std::atof(Rest.c_str()), 0.0, 1.0);
		}
		// ("-TheaterLandFall=0.6": rain, or snow where it lies)
		if (!Kind.empty() && (std::isdigit(static_cast<unsigned char>(Kind[0])) || Kind[0] == '.'))
		{
			Strength = std::clamp(std::atof(Kind.c_str()), 0.0, 1.0);
			Kind = "any";
		}
	}

	Weather Pick(const Asked& A, const Book& Numbers)
	{
		Weather W;
		const std::string Word = Lower(A.Word);
		const double Cover = std::clamp(A.Cover, 0.0, 1.0);
		const double SnowOver = Num(Numbers, "pick.snow_lying_from", 0.3);
		const bool bSnowy = A.SnowLying >= SnowOver;
		const std::string Forced = Lower(A.ForcedKind);
		if (!Forced.empty())
		{
			// The Mac's stand-in: -TheaterLandFall=rain,0.8 (or snow, or a strength alone: snow where it lies, else rain).
			if (Forced == "none" || Forced == "off" || Forced == "dry" || A.ForcedStrength == 0.0)
			{
				W.Why = "none falls (-TheaterLandFall)";
				return W;
			}
			W.Kind = Forced == "snow" ? WeatherFall::Kind::Snow : Forced == "rain" ? WeatherFall::Kind::Rain : bSnowy ? WeatherFall::Kind::Snow : WeatherFall::Kind::Rain;
			W.Strength = A.ForcedStrength >= 0.0 ? A.ForcedStrength : 0.7;
			W.Why = std::string(WeatherFall::NameOf(W.Kind)) + Fmt(" at %.2f, as -TheaterLandFall asks", W.Strength);
		}
		else if (Word == "rain")
		{
			// A closed sky over the place this month (the campaign's "rain"): it is raining, harder the heavier the deck
			// (the ticket's sky, 0.75 to 0.95 for rain); over snow lying, it snows.
			const double From = Num(Numbers, "pick.rain.from_cover", 0.7), Full = Num(Numbers, "pick.rain.full_cover", 0.95);
			W.Kind = bSnowy ? WeatherFall::Kind::Snow : WeatherFall::Kind::Rain;
			W.Strength = Num(Numbers, "pick.rain.least", 0.45) + (1.0 - Num(Numbers, "pick.rain.least", 0.45)) * Ease(From, Full, Cover);
			W.Why = std::string("the campaign's closed sky (rain), cover ") + Fmt("%.2f", Cover) + (bSnowy ? ", snow lying: it snows" : "");
		}
		else if (Word == "snow")
		{
			// The snow months: snow lies; it falls only under a heavy deck (the ticket's 0.8 to 1: about half its battles).
			const double From = Num(Numbers, "pick.snow.from_cover", 0.88), Full = Num(Numbers, "pick.snow.full_cover", 1.0);
			if (A.bDeck && Cover >= From)
			{
				W.Kind = WeatherFall::Kind::Snow;
				W.Strength = Num(Numbers, "pick.snow.least", 0.35) + (1.0 - Num(Numbers, "pick.snow.least", 0.35)) * Ease(From, Full, Cover);
				W.Why = Fmt("the snow months under a deck of %.2f: snow falls", Cover);
			}
			else
			{
				W.Why = Fmt("the snow months, the sky %.2f: the snow lies, none falls", Cover);
			}
		}
		else if (Word == "mud")
		{
			// The thaw and the autumn rains: under the heaviest of its decks (0.6 to 0.8), a drizzle.
			const double From = Num(Numbers, "pick.mud.from_cover", 0.76);
			if (Cover >= From)
			{
				W.Kind = bSnowy ? WeatherFall::Kind::Snow : WeatherFall::Kind::Rain;
				W.Strength = Num(Numbers, "pick.mud.least", 0.25) + Num(Numbers, "pick.mud.more", 0.15) * Ease(From, From + 0.05, Cover);
				W.Why = Fmt("the thaw's mud under a deck of %.2f: a drizzle", Cover);
			}
			else
			{
				W.Why = Fmt("the thaw's mud, the sky %.2f: none falls", Cover);
			}
		}
		else
		{
			W.Why = Word.empty() ? "no campaign weather: none falls" : "the campaign's sky is " + Word + ": none falls";
		}
		if (W.Kind == WeatherFall::Kind::None)
		{
			return W;
		}
		// (a tropical downpour is heavier; rain over the desert lighter)
		if (Forced.empty() && W.Kind == WeatherFall::Kind::Rain)
		{
			if (A.Look == "jungle" || A.Look == "island" || A.Look == "rice") W.Strength += Num(Numbers, "pick.rain.tropic_more", 0.15);
			if (A.Look == "desert" || A.Look == "drysteppe") W.Strength *= Num(Numbers, "pick.rain.dry_times", 0.7);
		}
		W.Strength = std::clamp(W.Strength, 0.0, 1.0);
		const double S = W.Strength;
		if (W.Kind == WeatherFall::Kind::Rain)
		{
			W.Wet = std::clamp(Num(Numbers, "day.rain.wet_least", 0.55) + (1.0 - Num(Numbers, "day.rain.wet_least", 0.55)) * S, 0.0, 1.0);
			W.HazeTimes = 1.0 + Num(Numbers, "day.rain.haze_more", 1.3) * S;
			W.HazeToward = Num(Numbers, "day.rain.haze_toward", 0.5) * S;
			W.HazeInk[0] = Num(Numbers, "day.rain.haze_r", 0.44);
			W.HazeInk[1] = Num(Numbers, "day.rain.haze_g", 0.46);
			W.HazeInk[2] = Num(Numbers, "day.rain.haze_b", 0.49);
			W.LightTimes = 1.0 - Num(Numbers, "day.rain.dimmer", 0.18) * S;
			// (wet things shine: the ground's sheen from the sky at a low angle, the hedges' and the walls' a little, the
			// steel most; the rougher the wetter, from the dry numbers each ink was made with)
			W.GroundRough = 0.95 - Num(Numbers, "day.rain.ground_rough_less", 0.32) * W.Wet;
			W.GroundSpecular = 0.15 + Num(Numbers, "day.rain.ground_specular_more", 0.22) * W.Wet;
			W.ThingRough = 0.9 - Num(Numbers, "day.rain.thing_rough_less", 0.22) * W.Wet;
			W.ThingSpecular = 0.2 + Num(Numbers, "day.rain.thing_specular_more", 0.15) * W.Wet;
			W.SteelRough = Num(Numbers, "day.rain.steel_rough", 0.45);
			W.SteelSpecular = Num(Numbers, "day.rain.steel_specular", 0.45);
			W.InkOverHaze = Num(Numbers, "day.rain.ink_over_haze", 1.15);
			W.Sound = S;
		}
		else
		{
			W.HazeTimes = 1.0 + Num(Numbers, "day.snow.haze_more", 2.0) * S;
			W.HazeToward = Num(Numbers, "day.snow.haze_toward", 0.6) * S;
			W.HazeInk[0] = Num(Numbers, "day.snow.haze_r", 0.70);
			W.HazeInk[1] = Num(Numbers, "day.snow.haze_g", 0.73);
			W.HazeInk[2] = Num(Numbers, "day.snow.haze_b", 0.78);
			W.LightTimes = 1.0 - Num(Numbers, "day.snow.dimmer", 0.1) * S;
			W.InkOverHaze = Num(Numbers, "day.snow.ink_over_haze", 1.45);
		}
		W.HazeTimes = std::max(1.0, W.HazeTimes);
		W.HazeToward = std::clamp(W.HazeToward, 0.0, 1.0);
		W.LightTimes = std::clamp(W.LightTimes, 0.3, 1.0);
		return W;
	}

	double RainOnField(const Asked& A, const Book& Numbers)
	{
		Asked Dry = A;
		Dry.SnowLying = 0.0;
		const Weather W = Pick(Dry, Numbers);
		return W.Kind == WeatherFall::Kind::Rain ? W.Wet : 0.0;
	}
}
