#include "Battle/SeaLookField.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace SeaLookField
{
	namespace
	{
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
		double Wrap360(double D)
		{
			D = std::fmod(D, 360.0);
			return D < 0.0 ? D + 360.0 : D;
		}
		std::string Fixed(double V, int Places)
		{
			char Buf[48];
			std::snprintf(Buf, sizeof(Buf), "%.*f", Places, V);
			return Buf;
		}
	}

	double Field::WindowMiddleHours() const
	{
		if (!N.bWindow)
		{
			return -1.0;
		}
		return 0.5 * (N.From + N.To) / 3600.0;
	}

	Field Read(const AirFeed::Json& J)
	{
		using AirFeed::Json;
		Field Out;
		if (!J.IsObject())
		{
			return Out;
		}
		Out.Title = J["title"].Str("");
		Out.ZoneName = J["zoneName"].Str("");
		Out.Lat = Clamp(J["lat"].Num(Out.Lat), -90.0, 90.0);
		Out.Lon = J["lon"].Num(Out.Lon);
		const Json& When = J["when"];
		if (When.IsObject())
		{
			Out.bWhen = When["year"].IsNumber() && When["month"].IsNumber();
			Out.Year = When["year"].Int(Out.Year);
			Out.Month = std::clamp(When["month"].Int(Out.Month), 0, 11);
			if (When["hour"].IsNumber())
			{
				Out.Hour = Wrap360(When["hour"].Num() * 15.0) / 15.0;
				Out.bAny = true;
			}
		}
		const Json& Wind = J["wind"];
		if (Wind.IsObject())
		{
			Out.WindFrom = Wind["from"].Num(0.0);
			Out.WindSpeed = Wind["speed"].IsNumber() ? std::max(0.0, Wind["speed"].Num()) : -1.0;
		}
		else if (Wind.IsNumber())
		{
			Out.WindFrom = Wind.Num();
		}

		// light{sun, moon, vis}: a number or an object each (sun: up, or {up, from}; moon: lit, or {lit, up, from})
		const Json& Light = J["light"];
		if (Light.IsObject())
		{
			Out.bAny = true;
			Out.L.bGiven = true;
			const Json& Sun = Light["sun"];
			Out.L.SunUp = Clamp(Sun.IsObject() ? Sun["up"].Num(-20.0) : Sun.Num(-20.0), -90.0, 90.0);
			if (Sun.IsObject() && Sun["from"].IsNumber())
			{
				Out.L.bSunFrom = true;
				Out.L.SunFrom = Wrap360(Sun["from"].Num());
			}
			const Json& Moon = Light["moon"];
			if (Moon.IsObject() || Moon.IsNumber())
			{
				Out.L.bMoon = true;
				Out.L.MoonLit = Clamp(Moon.IsObject() ? Moon["lit"].Num(0.0) : Moon.Num(0.0), 0.0, 1.0);
				if (Moon.IsObject() && Moon["up"].IsNumber() && Moon["from"].IsNumber())
				{
					Out.L.bMoonPlace = true;
					Out.L.MoonUp = Clamp(Moon["up"].Num(), -90.0, 90.0);
					Out.L.MoonFrom = Wrap360(Moon["from"].Num());
				}
			}
			Out.L.Vis = std::max(0.0, Light["vis"].Num(0.0));
		}

		// sky{cover, kind, base, top, rain, word}
		const Json& Sky = J["sky"];
		if (Sky.IsObject())
		{
			Out.bAny = true;
			Out.S.bGiven = true;
			if (Sky["cover"].IsNumber())
			{
				Out.S.Cover = Clamp(Sky["cover"].Num(), 0.0, 1.0);
			}
			const std::string Kind = Sky["kind"].Str("");
			if (Kind == "cumulus" || Kind == "deck" || Kind == "squalls")
			{
				Out.S.Kind = Kind;
			}
			if (Sky["base"].IsNumber())
			{
				Out.S.Base = std::max(0.0, Sky["base"].Num());
			}
			if (Sky["top"].IsNumber())
			{
				Out.S.Top = std::max(Out.S.Base, Sky["top"].Num());
			}
			if (Sky["rain"].IsNumber())
			{
				Out.S.Rain = Clamp(Sky["rain"].Num(), 0.0, 1.0);
			}
			Out.S.Word = Sky["word"].Str("");
		}

		// sea{swell{height, period, from}, fetch}
		const Json& Sea = J["sea"];
		if (Sea.IsObject())
		{
			const Json& Swell = Sea["swell"];
			if (Swell.IsObject() && Swell["height"].IsNumber() && Swell["period"].IsNumber())
			{
				Out.bAny = true;
				Out.W.bSwell = true;
				Out.W.SwellHeight = Clamp(Swell["height"].Num(), 0.0, 20.0);
				Out.W.SwellPeriod = Clamp(Swell["period"].Num(), 2.0, 25.0);
				Out.W.SwellFrom = Wrap360(Swell["from"].Num(0.0));
			}
			if (Sea["fetch"].IsNumber())
			{
				Out.bAny = true;
				Out.W.Fetch = std::max(0.0, Sea["fetch"].Num());
			}
		}

		// A convoy night's own: night{hours, day} and window{from, to, night}
		const Json& Night = J["night"];
		if (Night.IsObject())
		{
			Out.bAny = true;
			Out.N.bGiven = true;
			Out.N.Hours = Clamp(Night["hours"].Num(0.0), 0.0, 24.0);
			const int Day = Night["day"].Int(0);
			Out.N.Day = Day >= 1 && Day <= 31 ? Day : 0;
		}
		const Json& Window = J["window"];
		if (Window.IsObject() && Window["from"].IsNumber() && Window["to"].IsNumber())
		{
			Out.N.bWindow = true;
			Out.N.From = std::max(0.0, Window["from"].Num());
			Out.N.To = std::max(Out.N.From, Window["to"].Num());
			Out.N.Length = std::max(0.0, Window["night"].Num(Out.N.Hours * 3600.0));
		}
		return Out;
	}

	bool ReadRecording(const std::string& Text, Field& Out, std::string* OutError)
	{
		AirFeed::Json J;
		std::string Why;
		if (!AirFeed::Parse(Text, J, Why))
		{
			if (OutError)
			{
				*OutError = "not JSON: " + Why;
			}
			return false;
		}
		if (!J["field"].IsObject())
		{
			if (OutError)
			{
				*OutError = "no field";
			}
			return false;
		}
		Out = Read(J["field"]);
		return true;
	}

	SeaNightSky::When NightWhen(const Field& F)
	{
		SeaNightSky::When W;
		W.Lat = F.Lat;
		W.Lon = F.Lon;
		W.Year = F.Year;
		W.Month = F.Month + 1;
		W.Day = F.DayOfMonth();
		W.SolarHour = F.Hour;
		W.SunUp = F.L.bGiven ? F.L.SunUp : 90.0;
		W.bMoon = F.L.bGiven && F.L.bMoon && F.L.bMoonPlace;
		W.MoonUp = W.bMoon ? F.L.MoonUp : -90.0;
		W.MoonFrom = W.bMoon ? F.L.MoonFrom : 0.0;
		W.MoonLit = W.bMoon ? F.L.MoonLit : 0.0;
		return W;
	}

	std::string Said(const Field& F)
	{
		std::string Line = "the field for the look: ";
		Line += F.Hour >= 0.0 ? "hour " + Fixed(F.Hour, 2) : std::string("hour not given");
		if (F.L.bGiven)
		{
			Line += "; light: sun " + Fixed(F.L.SunUp, 1) + " deg" + (F.L.bSunFrom ? " from " + Fixed(F.L.SunFrom, 0) : std::string());
			if (F.L.bMoon)
			{
				Line += ", moon " + Fixed(100.0 * F.L.MoonLit, 0) + "% lit" +
					(F.L.bMoonPlace ? " " + Fixed(F.L.MoonUp, 0) + " deg up from " + Fixed(F.L.MoonFrom, 0) : std::string());
			}
			Line += F.L.Vis > 0.0 ? ", vis " + Fixed(F.L.Vis, 0) + " m" : std::string(", vis not said");
		}
		else
		{
			Line += "; light not given";
		}
		if (F.S.bGiven)
		{
			Line += "; sky: " + (F.S.Kind.empty() ? std::string("kind not said") : F.S.Kind) +
				(F.S.Cover >= 0.0 ? " " + Fixed(F.S.Cover, 2) : std::string()) + (F.S.Rain >= 0.0 ? ", rain " + Fixed(F.S.Rain, 2) : std::string());
		}
		else
		{
			Line += "; sky not given";
		}
		Line += F.W.bSwell ? "; swell " + Fixed(F.W.SwellHeight, 1) + " m at " + Fixed(F.W.SwellPeriod, 0) + " s from " + Fixed(F.W.SwellFrom, 0)
						   : std::string("; swell not given");
		if (F.N.bGiven)
		{
			Line += "; night of " + Fixed(F.N.Hours, 1) + " h" + (F.N.Day > 0 ? " on day " + std::to_string(F.N.Day) : std::string()) +
				(F.N.bWindow ? ", window " + Fixed(F.N.From / 3600.0, 2) + " to " + Fixed(F.N.To / 3600.0, 2) + " h in" : std::string());
		}
		return Line;
	}
}
