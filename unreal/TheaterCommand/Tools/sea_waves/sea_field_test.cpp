// The sea's look from the rules' field (Battle/SeaLookField; the release 2 ocean plan, step 4), read from every recording
// and stand-in the game and its tests play, and from hand-written fields for every shape the rules may send.
// No engine. Usage: sea_field_test RECORDING.json...
#include "Battle/SeaFeed.h"
#include "Battle/SeaLookField.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace SeaLookField;

namespace
{
	int Checks = 0, Failed = 0;
	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
		}
		std::printf("%s %s\n", bOk ? "  ok  " : "  FAIL", What.c_str());
	}
	bool Near(double A, double B, double Within = 1.0e-9) { return std::abs(A - B) <= Within; }
	std::string Base(const std::string& Path)
	{
		const size_t Slash = Path.find_last_of('/');
		std::string Name = Slash == std::string::npos ? Path : Path.substr(Slash + 1);
		const size_t Dot = Name.rfind(".json");
		return Dot == std::string::npos ? Name : Name.substr(0, Dot);
	}
	Field FromText(const std::string& FieldText)
	{
		AirFeed::Json J;
		std::string Why;
		if (!AirFeed::Parse(FieldText, J, Why))
		{
			std::printf("  (not JSON: %s)\n", Why.c_str());
		}
		return Read(J);
	}
}

int main(int Argc, char** Argv)
{
	// ---- Every recording and stand-in, as the game reads them
	int Fleet = 0, Convoy = 0, Looks = 0;
	for (int A = 1; A < Argc; ++A)
	{
		const std::string Path = Argv[A], Name = Base(Path);
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Text;
		Text << In.rdbuf();
		Field F;
		std::string Why;
		const bool bRead = ReadRecording(Text.str(), F, &Why);
		Check(bRead, Name + ": read (" + (bRead ? std::string("its field") : Why) + ")");
		if (!bRead)
		{
			continue;
		}
		std::printf("        %s: %s\n", Name.c_str(), Said(F).c_str());
		// (the show's own reader, on the same text: the light the battle uses and the look's must agree)
		SeaFeed::Recording R;
		std::string WhyFeed;
		if (SeaFeed::ReadRecording(Text.str(), R, WhyFeed))
		{
			const SeaShow::Field& B = R.Board;
			bool bSame = B.bLight == F.L.bGiven && Near(B.Lat, F.Lat) && Near(B.Lon, F.Lon) && B.Month == F.Month && B.Year == F.Year;
			if (F.L.bGiven)
			{
				bSame = bSame && Near(B.SunUp, F.L.SunUp) && Near(B.MoonLit, F.L.MoonLit) && Near(B.Seeing, F.L.Vis);
				if (F.L.bMoonPlace)
				{
					bSame = bSame && Near(B.MoonUp, F.L.MoonUp) && Near(B.MoonFrom, F.L.MoonFrom);
				}
			}
			Check(bSame, Name + ": its place, date and light as the show reads them");
		}
		else
		{
			Check(false, Name + ": the show reads it too (" + WhyFeed + ")");
		}
		const bool bFleet = Name == "two-lines" || Name == "light-ships" || Name == "samar";
		if (bFleet)
		{
			++Fleet;
			Check(!F.bAny && F.Hour < 0.0 && !F.L.bGiven && !F.S.bGiven && !F.W.bSwell && !F.N.bGiven,
				Name + ": a real fleet action gives nothing for the look yet (the dated ask is open), so it is drawn as today");
		}
		if (Name.rfind("convoy-", 0) == 0 && Name.find("night") == std::string::npos)
		{
			++Convoy;
			Check(F.L.bGiven && F.L.SunUp < -6.0 && F.N.bGiven && F.N.bWindow && F.N.Day >= 1 && F.N.To > F.N.From && F.N.Length > F.N.To,
				Name + ": a real wolf-pack night gives its light, its night (day " + std::to_string(F.N.Day) + ") and its window");
		}
		if (F.S.bGiven || F.W.bSwell || F.Hour >= 0.0)
		{
			++Looks;
		}
	}
	Check(Fleet == 3, "the three real fleet actions were read (" + std::to_string(Fleet) + ")");
	Check(Convoy == 3, "the three real wolf-pack nights were read (" + std::to_string(Convoy) + ")");
	Check(Looks >= 3, "the look stand-ins carry a sky, a swell or an hour (" + std::to_string(Looks) + ")");

	// ---- The stand-ins say what they were made to say (Tools/sea_waves/stand-ins/make_stand_ins.py)
	for (int A = 1; A < Argc; ++A)
	{
		const std::string Path = Argv[A], Name = Base(Path);
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Text;
		Text << In.rdbuf();
		Field F;
		if (!ReadRecording(Text.str(), F))
		{
			continue;
		}
		if (Name == "samar-dawn")
		{
			Check(Near(F.Hour, 6.8) && F.L.bSunFrom && Near(F.L.SunFrom, 95.0) && Near(F.L.SunUp, 6.0) && Near(F.L.Vis, 20000.0) && F.S.Kind == "cumulus" &&
				Near(F.S.Cover, 0.50) && Near(F.S.Top, 3500.0) && Near(F.S.Rain, 0.7) && F.W.bSwell && Near(F.W.SwellHeight, 1.5) && Near(F.W.SwellPeriod, 11.0) && Near(F.W.SwellFrom, 80.0),
				"samar-dawn: 6.8 h, the sun 6 deg up from 095, seen 20 km, cumulus 0.50 to 3,500 m with rain squalls at 0.7, a 1.5 m swell at 11 s from 080");
		}
		if (Name == "north-atlantic-gale")
		{
			Check(Near(F.WindSpeed, 19.0) && F.S.Kind == "deck" && Near(F.S.Cover, 0.9) && Near(F.S.Rain, 0.5) && Near(F.L.Vis, 3000.0) &&
				Near(F.W.SwellHeight, 4.0) && Near(F.W.SwellPeriod, 13.0),
				"north-atlantic-gale: wind 19 m/s, a deck at 0.9, rain 0.5, vis 3,000 m, a 4 m swell at 13 s");
		}
		if (Name == "fleet-swell")
		{
			Check(F.W.bSwell && Near(F.W.SwellHeight, 1.2) && Near(F.W.SwellPeriod, 9.0) && !F.S.bGiven && !F.L.bGiven && F.Hour < 0.0,
				"fleet-swell: only a 1.2 m swell at 9 s added to a real fleet action");
		}
	}

	// ---- Every shape the rules may send
	{
		const Field None = FromText("{}");
		Check(!None.bAny && None.Hour < 0.0 && !None.L.bGiven && !None.S.bGiven && !None.W.bSwell && !None.N.bGiven && None.WindSpeed < 0.0,
			"an empty field gives nothing: every part not given");
		const Field NotObject = FromText("[1, 2]");
		Check(!NotObject.bAny, "a field that is not an object gives nothing");

		const Field Bare = FromText(R"({"light": {"sun": -12, "moon": 0.6, "vis": 2000}})");
		Check(Bare.L.bGiven && Near(Bare.L.SunUp, -12.0) && !Bare.L.bSunFrom && Bare.L.bMoon && Near(Bare.L.MoonLit, 0.6) && !Bare.L.bMoonPlace && Near(Bare.L.Vis, 2000.0),
			"light as bare numbers: the sun's height, the moon's lit share (her place not given), the vis");
		const Field Whole = FromText(R"({"light": {"sun": {"up": -3.5, "from": 271}, "moon": {"lit": 0.9, "up": 40, "from": 130}}, "when": {"year": 1942, "month": 10, "hour": 25.5}})");
		Check(Whole.L.bSunFrom && Near(Whole.L.SunFrom, 271.0) && Near(Whole.L.SunUp, -3.5) && Whole.L.bMoonPlace && Near(Whole.L.MoonUp, 40.0) && Near(Whole.L.MoonFrom, 130.0),
			"light as objects: the sun's bearing read (SeaFeed reads only her height), the moon's place");
		Check(Near(Whole.Hour, 1.5) && Whole.bWhen && Whole.Year == 1942 && Whole.Month == 10, "when.hour read, taken round the clock (25.5 is 1.5)");
		const Field Odd = FromText(R"({"sky": {"cover": 1.7, "kind": "fog", "base": -50, "top": 20, "rain": -0.4, "word": "thick"}, "light": {"sun": 120, "moon": {"lit": 3}}})");
		Check(Odd.S.bGiven && Near(Odd.S.Cover, 1.0) && Odd.S.Kind.empty() && Near(Odd.S.Base, 0.0) && Near(Odd.S.Top, 20.0) && Near(Odd.S.Rain, 0.0) && Odd.S.Word == "thick",
			"a sky out of range is held in range, a kind not known is left not given");
		Check(Near(Odd.L.SunUp, 90.0) && Near(Odd.L.MoonLit, 1.0), "a sun over 90 deg and a moon over full are held in range");
		const Field Swell = FromText(R"({"sea": {"swell": {"height": 2.5, "period": 30, "from": -90}, "fetch": 300}})");
		Check(Swell.bAny && Swell.W.bSwell && Near(Swell.W.SwellPeriod, 25.0) && Near(Swell.W.SwellFrom, 270.0) && Near(Swell.W.Fetch, 300.0),
			"a swell read, its period held to 25 s, its bearing taken round to 270");
		const Field HalfSwell = FromText(R"({"sea": {"swell": {"height": 2.5}}})");
		Check(!HalfSwell.W.bSwell, "a swell with no period is not given");
		const Field Night = FromText(R"({"night": {"hours": 13.39, "day": 7}, "window": {"from": 1848, "to": 2568, "night": 48204}})");
		Check(Night.N.bGiven && Night.N.Day == 7 && Night.DayOfMonth() == 7 && Night.N.bWindow && Near(Night.WindowMiddleHours(), (1848.0 + 2568.0) / 7200.0),
			"a convoy night's day of the month and its window's middle");
		Check(FromText("{}").DayOfMonth() == 15, "no day given: the 15th");
		// (the night sky's own "when": a real wolf-pack night's header, and a field with no light)
		const Field Pack = FromText(R"({"when": {"year": 1940, "month": 9}, "lat": 58, "lon": -22, "light": {"sun": -18, "moon": {"lit": 0.34, "up": 25, "from": 228}, "vis": 1432}, "night": {"hours": 13.39, "day": 7}})");
		const SeaNightSky::When W = NightWhen(Pack);
		Check(Near(W.Lat, 58.0) && Near(W.Lon, -22.0) && W.Year == 1940 && W.Month == 10 && W.Day == 7 && W.SolarHour < 0.0 && Near(W.SunUp, -18.0) &&
				W.bMoon && Near(W.MoonUp, 25.0) && Near(W.MoonFrom, 228.0) && Near(W.MoonLit, 0.34),
			"the night sky's when from a wolf-pack night: 7 October 1940 (its month counted from 1), the sun 18 down, the moon placed");
		const SeaNightSky::When Day = NightWhen(FromText(R"({"lat": 11.7, "lon": 126.2})"));
		Check(Day.SunUp > 0.0 && !Day.bMoon, "no light given: the sun high, no moon, so the dome stays off");
		const SeaNightSky::When Bare2 = NightWhen(FromText(R"({"light": {"sun": -20, "moon": 0.5}})"));
		Check(!Bare2.bMoon, "a moon given only as her lit share is placed nowhere: no disc");
		const Field Said1 = FromText("{}");
		Check(Said(Said1).find("hour not given") != std::string::npos && Said(Said1).find("light not given") != std::string::npos &&
				Said(Said1).find("sky not given") != std::string::npos && Said(Said1).find("swell not given") != std::string::npos,
			"the log line says each part not given");
	}

	std::printf("sea_field_test: %d checks, %d failed\n", Checks, Failed);
	return Failed == 0 ? 0 : 1;
}
