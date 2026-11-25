// THE FLEET SKY'S CLOUD TEST (the release 2 ocean plan, step 13: clouds and weather). Battle/SeaSky.{h,cpp} held to the
// plan: the five switches as the scene takes them, the day from the field's sky (or the stand-in, or a review day), the
// laying through AirSky with the field's own wind, the dome's mapping and its painting from the shot's own eye, the
// cloud shade the key lamp's light function reads, the far rain and its squall, and the deck's far sheet. Nothing here
// is the engine's: the engine's side is Battle/SeaWeather, which lays exactly what this paints.
//
// The checks the plan asks for:
//   - the painted cover as the plan's line 940 now has it (the sea scene, 25 November; the old 5 per cent cannot hold):
//     the measured figures pinned, none at a clear sky, nine tenths and more at a full overcast, never less as the
//     dealt cover rises from none to full in tenths, and the worst gap held as a ceiling (SeaSky::WorstCoverGap);
//   - a cumulus day laid at the cover that paints its own, seen from the sea (SeaSky::CumulusLaidFor, the sea scene's
//     follow-up): the whole sky within five hundredths of the day's cover, as far as cumulus can paint (CumulusReach),
//     the day's rain, light and word its own, and a deck and air's stand-in laid as given; from SeaSky::StratoFrom a
//     broken deck of stratocumulus (the sea scene's ruling), its gaps opened (AirSky::Placing::DeckHoles, by the
//     measured SeaSky::StratoPainted) to read as the day's cover, its base held to 600 to 1,000 m, its light under it
//     the laid deck's taken as far as its cover reads, laid byte for byte as AirSky lays that deck, and air's own deck
//     unchanged with DeckHoles left at -1;
//   - the overview's eye under a deck the dome draws (the plan's line 941, SeaSky::OverviewHeight);
//   - the dome picture is the same bytes twice over, and agrees with AirSky::ShadeOf's holes;
//   - a cloud's bearing and height above the horizon from every planned eye is the line from that eye to its puff,
//     within a texel, and its shadow lies where the sun's line through it crosses the sea;
//   - with no sky from the rules and the stand-in switch off, no clouds;
//   - the stand-in switch lays exactly air's own stand-in cumulus.
//
// Plain C++20, no engine. Usage: sea_sky_test [out dir]
#include "Battle/AirSky.h"
#include "Battle/SeaLookSwitches.h"
#include "Battle/SeaSky.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace
{
	using Vec = AirSky::Vec;
	using Rgb = AirSky::Rgb;
	constexpr double Pi = 3.14159265358979323846;

	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Detail = "")
	{
		++Checks;
		if (!bOk)
		{
			++Failed;
		}
		std::printf("%s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Detail.empty() ? "" : ": ", Detail.c_str());
	}

	void Note(const std::string& What) { std::printf("     %s\n", What.c_str()); }

	std::string Num(double X, int Places = 3)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	using Clock = std::chrono::steady_clock;
	double Since(Clock::time_point T0) { return std::chrono::duration<double>(Clock::now() - T0).count(); }

	/** The board's middle for the test, and the sun and wind the scene would hand the core. */
	constexpr double MiddleX = 10000.0, MiddleY = 10000.0;
	const Vec TestSun = Vec(-0.35, -0.45, 0.82).Unit();
	/** A winter North Atlantic gale's wind: 13.5 m/s from 240 degrees (the gale stand-in), the way it blows toward. */
	const Vec GaleWind = Vec(13.5 * std::sin((240.0 + 180.0) * Pi / 180.0), 13.5 * std::cos((240.0 + 180.0) * Pi / 180.0), 0.0);

	/** The shots' eyes the sea scene plans (Battle/SeaBattle's PlanShots is the engine's: these are the heights and the
	 * stands it uses, the board's middle and two kilometres either side, and the overview). */
	struct Eye
	{
		const char* Name;
		Vec At;
	};
	std::vector<Eye> PlannedEyes()
	{
		return {
			{ "a destroyer's bridge at the middle (14 m)", Vec(MiddleX, MiddleY, 14.0) },
			{ "a cruiser's bridge at the middle (28 m)", Vec(MiddleX, MiddleY, 28.0) },
			{ "a battleship's top at the middle (60 m)", Vec(MiddleX, MiddleY, 60.0) },
			{ "28 m, two kilometres east", Vec(MiddleX + 2000.0, MiddleY, 28.0) },
			{ "28 m, two kilometres west", Vec(MiddleX - 2000.0, MiddleY, 28.0) },
			{ "28 m, two kilometres north", Vec(MiddleX, MiddleY + 2000.0, 28.0) },
			{ "28 m, two kilometres south", Vec(MiddleX, MiddleY - 2000.0, 28.0) },
			{ "the overview (1,300 m)", Vec(MiddleX, MiddleY, 1300.0) },
		};
	}

	/** A look with the switches of a list asked for (the scene asks the engine's command line). */
	SeaLookSwitches::Look Asked(const std::map<std::string, std::string>& Given)
	{
		return SeaLookSwitches::Read([&Given](const SeaLookSwitches::Switch& One, std::string& Value)
		{
			const auto Found = Given.find(One.Name);
			if (Found == Given.end())
			{
				return false;
			}
			Value = Found->second;
			return true;
		});
	}

	/** A switch's value in force, as the "Sea look:" line would say it. */
	std::string InForce(const SeaLookSwitches::Look& Look, const char* Name)
	{
		for (const SeaLookSwitches::Asked& One : Look.Given)
		{
			if (One.Index >= 0 && One.Index < SeaLookSwitches::Count && std::string(SeaLookSwitches::All[One.Index].Name) == Name)
			{
				return One.Value;
			}
		}
		return "(not asked)";
	}

	/** A recording's text with a field sky of its own. */
	std::string RecordingWith(const std::string& SkyText)
	{
		return "{\"what\":\"sea\",\"key\":\"test\",\"seed\":241,\"field\":{\"size\":20000,\"wind\":{\"from\":240,\"speed\":13.5}"
			+ (SkyText.empty() ? std::string() : ",\"sky\":" + SkyText) + "},\"views\":[]}";
	}

	/** Two lists of places, the same or not (Vec is plain numbers, with no comparison of its own). */
	bool Same(const std::vector<Vec>& A, const std::vector<Vec>& B)
	{
		if (A.size() != B.size())
		{
			return false;
		}
		for (size_t K = 0; K < A.size(); ++K)
		{
			if (A[K].X != B[K].X || A[K].Y != B[K].Y || A[K].Z != B[K].Z)
			{
				return false;
			}
		}
		return true;
	}

	/** A hash of a field's clouds and puffs: two layings are the same laying when their hashes are. */
	uint64_t HashOf(const AirSky::Field& F)
	{
		uint64_t H = 1469598103934665603ull;
		auto Eat = [&H](double V)
		{
			const int64_t Fixed = static_cast<int64_t>(std::llround(V * 1000.0));
			for (int K = 0; K < 8; ++K)
			{
				H ^= static_cast<uint64_t>((Fixed >> (8 * K)) & 0xFF);
				H *= 1099511628211ull;
			}
		};
		Eat(F.Base);
		Eat(F.Top);
		Eat(F.Cover);
		Eat(static_cast<double>(F.Clouds.size()));
		Eat(static_cast<double>(F.Puffs.size()));
		for (const AirSky::Cloud& C : F.Clouds)
		{
			Eat(C.At.X);
			Eat(C.At.Y);
			Eat(C.At.Z);
			Eat(C.W);
			Eat(C.H);
		}
		for (const AirSky::Puff& P : F.Puffs)
		{
			Eat(P.At.X);
			Eat(P.At.Y);
			Eat(P.At.Z);
			Eat(P.R);
			Eat(P.Lit);
			Eat(P.Alpha);
		}
		return H;
	}

	/** The day the gale stand-in's field gives: a closed deck with rain. */
	SeaSky::Sky GaleSky()
	{
		const SeaSky::Given Field = SeaSky::ReadRecording(RecordingWith("{\"kind\":\"deck\",\"cover\":0.95,\"base\":450,\"top\":2800,\"rain\":0.6,\"word\":\"overcast\"}"));
		return SeaSky::Choose(Field, SeaSky::Asked());
	}
	/** A scattered cumulus day, as the rules pass one (the art director's 0.39). */
	SeaSky::Sky FairSky(double Cover = 0.39)
	{
		SeaSky::Given Field;
		Field.bGiven = true;
		Field.Day.bGiven = true;
		Field.Day.bDeck = false;
		Field.Day.Base = 700.0;
		Field.Day.Top = 2600.0;
		Field.Day.Cover = Cover;
		Field.Day.Word = "scattered";
		return SeaSky::Choose(Field, SeaSky::Asked());
	}

	// ---- The switches ---------------------------------------------------------------------------------------------

	void Switches()
	{
		{
			SeaLookSwitches::Look Look = Asked({});
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("nothing asked: nothing drawn", Take.Draw == SeaSky::Clouds::Off && !Take.bShade && !Take.bRain && !Take.bStandIn && !Take.bWeather && !Take.Any());
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaClouds=", "dome" }, { "TheaterSeaCloudShade=", "1" }, { "TheaterSeaRain=", "1" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("-TheaterSeaClouds=dome, the shade and the rain on", Take.Draw == SeaSky::Clouds::Dome && Take.bShade && Take.bRain && Take.Any()
				&& InForce(Look, "TheaterSeaClouds=") == "dome" && InForce(Look, "TheaterSeaRain=") == "1", InForce(Look, "TheaterSeaClouds="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaClouds=", "puffs" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("-TheaterSeaClouds=puffs is drawn as the dome until step 13(e) lands, and the line says dome",
				Take.Draw == SeaSky::Clouds::Dome && InForce(Look, "TheaterSeaClouds=") == "dome", InForce(Look, "TheaterSeaClouds="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaClouds=", "2" }, { "TheaterSeaCloudShade=", "yes" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("anything else asked is off, said as 0", Take.Draw == SeaSky::Clouds::Off && !Take.bShade
				&& InForce(Look, "TheaterSeaClouds=") == "0" && InForce(Look, "TheaterSeaCloudShade=") == "0",
				InForce(Look, "TheaterSeaClouds=") + ", " + InForce(Look, "TheaterSeaCloudShade="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaWeather=", "deck,0.8" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("-TheaterSeaWeather=deck,0.8 asks for a deck of eight tenths, its rain as the cover gives",
				Take.bWeather && Take.bWeatherDeck && std::fabs(Take.WeatherCover - 0.8) < 1e-9 && Take.WeatherRain < 0.0
					&& InForce(Look, "TheaterSeaWeather=") == "deck,0.80", InForce(Look, "TheaterSeaWeather="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaWeather=", "0.3" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("-TheaterSeaWeather=0.3 is a cumulus day of three tenths", Take.bWeather && !Take.bWeatherDeck && std::fabs(Take.WeatherCover - 0.3) < 1e-9
				&& InForce(Look, "TheaterSeaWeather=") == "cumulus,0.30", InForce(Look, "TheaterSeaWeather="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaWeather=", "deck,0.95,0.6" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("-TheaterSeaWeather=deck,0.95,0.6 asks for its rain too", Take.bWeather && Take.bWeatherDeck && std::fabs(Take.WeatherRain - 0.6) < 1e-9
				&& InForce(Look, "TheaterSeaWeather=") == "deck,0.95,0.60", InForce(Look, "TheaterSeaWeather="));
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaWeather=", "squally" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			Check("a review day that cannot be read is none", !Take.bWeather && InForce(Look, "TheaterSeaWeather=") == "none", InForce(Look, "TheaterSeaWeather="));
		}
		{
			// (the five of them are the plan's step 13; the line says every one in force)
			int Built = 0;
			for (int K = 0; K < SeaLookSwitches::Count; ++K)
			{
				const std::string Name = SeaLookSwitches::All[K].Name;
				const bool bMine = Name == "TheaterSeaClouds=" || Name == "TheaterSeaCloudShade=" || Name == "TheaterSeaRain="
					|| Name == "TheaterSeaSkyStandIn=" || Name == "TheaterSeaWeather=";
				Built += bMine && SeaLookSwitches::All[K].bBuilt ? 1 : 0;
			}
			Check("the five weather switches are marked built in the table", Built == 5, std::to_string(Built) + " of 5");
		}
	}

	// ---- The day --------------------------------------------------------------------------------------------------

	void TheDay()
	{
		{
			const SeaSky::Given None = SeaSky::ReadRecording(RecordingWith(""));
			Check("a field with no sky gives none", !None.bGiven);
			const SeaSky::Sky Chosen = SeaSky::Choose(None, SeaSky::Asked());
			Check("with no sky from the rules and the stand-in off, nothing is laid", !Chosen.bLaid && !Chosen.bStandIn);
			const AirSky::Field F = SeaSky::Lay(Chosen, SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
			Check("and no clouds are laid", F.Clouds.empty() && F.Puffs.empty(), std::to_string(F.Clouds.size()) + " clouds");
		}
		{
			const SeaSky::Given Field = SeaSky::ReadRecording(RecordingWith("{\"kind\":\"deck\",\"cover\":0.95,\"base\":450,\"top\":2800,\"rain\":0.6,\"word\":\"overcast\"}"));
			Check("the gale's field sky is read whole", Field.bGiven && Field.Day.bDeck && std::fabs(Field.Day.Cover - 0.95) < 1e-9
				&& std::fabs(Field.Day.Base - 450.0) < 1e-9 && std::fabs(Field.Day.Top - 2800.0) < 1e-9 && std::fabs(Field.Rain - 0.6) < 1e-9
				&& Field.Day.Word == "overcast");
			const SeaSky::Sky Chosen = SeaSky::Choose(Field, SeaSky::Asked());
			Check("the rules' sky is laid, and the log says so", Chosen.bLaid && !Chosen.bStandIn && !Chosen.bAsked
				&& Chosen.Why.find("the rules' sky") == 0 && std::fabs(Chosen.Rain - 0.6) < 1e-9, Chosen.Why);
		}
		{
			std::string Error = "none";
			const SeaSky::Given Broken = SeaSky::ReadRecording("{not json", &Error);
			Check("a recording that cannot be read gives none, and why", !Broken.bGiven && Error != "none", Error);
		}
		{
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaSkyStandIn=", "1" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			const SeaSky::Sky Chosen = SeaSky::Choose(SeaSky::Given(), Take);
			Check("the stand-in switch lays air's own stand-in cumulus, and the log says the rules sent none",
				Chosen.bLaid && Chosen.bStandIn && Chosen.Why == "stand-in sky: the rules sent none", Chosen.Why);
			const AirSky::Day Air;
			Check("the stand-in day is AirSky::Day as made (the art director's passed cumulus)",
				!Chosen.Day.bDeck && std::fabs(Chosen.Day.Base - Air.Base) < 1e-9 && std::fabs(Chosen.Day.Top - Air.Top) < 1e-9
					&& std::fabs(Chosen.Day.CoverLaid() - AirSky::DefaultCumulus) < 1e-9, Num(Chosen.Day.CoverLaid()));
			const AirSky::Placing P = SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind);
			const AirSky::Field Mine = SeaSky::Lay(Chosen, P);
			const AirSky::Field Air2 = AirSky::Lay(AirSky::Day(), P);
			Check("and it lays exactly what AirSky lays for that day", HashOf(Mine) == HashOf(Air2) && !Mine.Clouds.empty(),
				std::to_string(Mine.Clouds.size()) + " clouds, " + std::to_string(Mine.Puffs.size()) + " puffs");
		}
		{
			// (a review day: its kind and cover over the rules' base and top)
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaWeather=", "deck,0.80" } });
			const SeaSky::Asked Take = SeaSky::Take(Look);
			const SeaSky::Given Field = SeaSky::ReadRecording(RecordingWith("{\"kind\":\"cumulus\",\"cover\":0.2,\"base\":700,\"top\":2200}"));
			const SeaSky::Sky Chosen = SeaSky::Choose(Field, Take);
			Check("a review day overrides the rules' kind and cover, keeping their base and top",
				Chosen.bLaid && Chosen.bAsked && Chosen.Day.bDeck && std::fabs(Chosen.Day.CoverLaid() - 0.8) < 1e-9
					&& std::fabs(Chosen.Day.Base - 700.0) < 1e-9 && std::fabs(Chosen.Day.Top - 2200.0) < 1e-9 && Chosen.Day.Word == "broken",
				Chosen.Why);
		}
		{
			// (how heavy the rain is: as air has its showers, and a closed deck's rain)
			AirSky::Day Cumulus;
			Cumulus.bGiven = true;
			Cumulus.Cover = 0.45;
			const double None = SeaSky::RainOf(Cumulus, -1.0);
			Cumulus.Cover = 0.55;
			const double Half = SeaSky::RainOf(Cumulus, -1.0);
			AirSky::Day Deck;
			Deck.bGiven = true;
			Deck.bDeck = true;
			Deck.Cover = 0.85;
			const double Dry = SeaSky::RainOf(Deck, -1.0);
			Deck.Cover = 0.97;
			const double Wet = SeaSky::RainOf(Deck, -1.0);
			Check("no rain at 0.45 of cumulus, three of the six curtains from 0.55", None <= 0.0 && std::fabs(Half - 0.5) < 1e-9, Num(None) + " to " + Num(Half));
			Check("no rain under a deck at 0.85, three curtains at 0.97", Dry <= 0.0 && std::fabs(Wet - 0.6) < 1e-9, Num(Dry) + " to " + Num(Wet));
			Check("rain given by the rules stands", std::fabs(SeaSky::RainOf(Deck, 0.25) - 0.25) < 1e-9);
		}
		{
			// (the sea's cumulus laid at the cover that paints the day's: the sea scene's follow-up, 25 November)
			bool bRises = true, bInside = true;
			double Was = -1.0;
			for (int Hundredth = 0; Hundredth <= 100; ++Hundredth)
			{
				const double Laid = SeaSky::CumulusLaidFor(Hundredth / 100.0);
				bRises = bRises && Laid >= Was;
				bInside = bInside && Laid >= 0.0 && Laid <= AirSky::CumulusMost;
				Was = Laid;
			}
			bool bTable = true;
			for (size_t Step = 1; Step < std::size(SeaSky::CumulusPainted); ++Step)
			{
				bTable = bTable && SeaSky::CumulusPainted[Step] > SeaSky::CumulusPainted[Step - 1];
			}
			Check("the table of what cumulus paints rises at every step, from none at none",
				bTable && SeaSky::CumulusPainted[0] == 0.0 && std::size(SeaSky::CumulusPainted) == 12, Num(SeaSky::CumulusReach) + " at the most");
			Check("the cover laid for a day's is none at none, never less as the day's rises, and never over CumulusMost",
				SeaSky::CumulusLaidFor(0.0) == 0.0 && bRises && bInside);
			Check("it reads the table backward: each step's painted share is laid at that step",
				std::fabs(SeaSky::CumulusLaidFor(SeaSky::CumulusPainted[4]) - 0.20) < 1e-9 && std::fabs(SeaSky::CumulusLaidFor(SeaSky::CumulusPainted[8]) - 0.40) < 1e-9,
				Num(SeaSky::CumulusLaidFor(SeaSky::CumulusPainted[4])) + ", " + Num(SeaSky::CumulusLaidFor(SeaSky::CumulusPainted[8])));
			Check("from the most cumulus paints up, it is laid at CumulusMost",
				SeaSky::CumulusLaidFor(SeaSky::CumulusReach) == AirSky::CumulusMost && SeaSky::CumulusLaidFor(0.5) == AirSky::CumulusMost
					&& SeaSky::CumulusLaidFor(1.0) == AirSky::CumulusMost);
			const SeaSky::Sky Fair = FairSky(0.2);
			Check("a cumulus day of the rules is laid at more, reads as its own cover, and the log says both",
				Fair.bLaid && std::fabs(Fair.Cover - 0.2) < 1e-9 && std::fabs(Fair.Day.Cover - SeaSky::CumulusLaidFor(0.2)) < 1e-9 && Fair.Day.Cover > 0.25
					&& Fair.Why.find("cumulus of 0.20 (scattered), laid at " + Num(Fair.Day.Cover, 2)) != std::string::npos, Fair.Why);
			AirSky::Day AsGiven = Fair.Day;
			AsGiven.Cover = 0.2;
			Check("its rain and its light are the cover it reads as",
				Fair.Rain == SeaSky::RainOf(AsGiven, -1.0) && std::fabs(SeaSky::ReadAs(Fair).Cover - 0.2) < 1e-9
					&& AirSky::LightOf(SeaSky::ReadAs(Fair), 0.0).Sky == AirSky::LightOf(AsGiven, 0.0).Sky);
			const SeaSky::Sky Gale = GaleSky();
			Check("a deck is laid as given", Gale.Day.bDeck && Gale.Day.Cover == 0.95 && Gale.Cover == 0.95
				&& Gale.Why.find("laid at") == std::string::npos, Gale.Why);
			SeaLookSwitches::Look Look = Asked({ { "TheaterSeaSkyStandIn=", "1" } });
			const SeaSky::Sky StandIn = SeaSky::Choose(SeaSky::Given(), SeaSky::Take(Look));
			Check("and air's stand-in (no cover given: the art director's passed cumulus) as it is",
				StandIn.Day.Cover < 0.0 && std::fabs(StandIn.Cover - AirSky::DefaultCumulus) < 1e-9 && StandIn.Why.find("laid at") == std::string::npos, StandIn.Why);
			SeaLookSwitches::Look Review = Asked({ { "TheaterSeaWeather=", "0.3" } });
			const SeaSky::Sky Pictures = SeaSky::Choose(SeaSky::Given(), SeaSky::Take(Review));
			Check("a cumulus day asked for review pictures is laid as the rules' would be",
				!Pictures.Day.bDeck && std::fabs(Pictures.Cover - 0.3) < 1e-9 && std::fabs(Pictures.Day.Cover - SeaSky::CumulusLaidFor(0.3)) < 1e-9, Pictures.Why);
			const SeaSky::Sky Most = FairSky(0.38);
			Check("a cumulus day between what cumulus can paint and StratoFrom is cumulus laid at the most, and reads that most",
				!Most.bStrato && !Most.Day.bDeck && Most.Day.Cover == AirSky::CumulusMost && Most.Cover == SeaSky::CumulusReach && Most.DeckHoles < 0.0,
				Most.Why);
		}
		{
			// (a half-cloudy day drawn as a broken deck of stratocumulus: the sea scene's ruling, 25 November)
			bool bHoles = true, bFalls = true;
			for (size_t Step = 1; Step < std::size(SeaSky::StratoPainted); ++Step)
			{
				bHoles = bHoles && SeaSky::StratoHoles[Step] > SeaSky::StratoHoles[Step - 1];
				bFalls = bFalls && SeaSky::StratoPainted[Step] < SeaSky::StratoPainted[Step - 1];
			}
			constexpr size_t Last = std::size(SeaSky::StratoPainted) - 1;
			Check("the broken deck's table: the more of it left out, the less sky it paints, and it spans every cover from StratoFrom to a deck's",
				bHoles && bFalls && std::size(SeaSky::StratoHoles) == std::size(SeaSky::StratoPainted) && SeaSky::StratoPainted[Last] < SeaSky::StratoFrom
					&& SeaSky::StratoReach >= AirSky::DeckLeast,
				Num(SeaSky::StratoPainted[Last]) + " at " + Num(SeaSky::StratoHoles[Last], 2) + " to " + Num(SeaSky::StratoReach) + " at " + Num(SeaSky::StratoHoles[0], 2));
			bool bOpens = true, bInside = true;
			double Was = 1.0;
			for (int Hundredth = 30; Hundredth <= 100; ++Hundredth)
			{
				const double Holes = SeaSky::StratoHolesFor(Hundredth / 100.0);
				bOpens = bOpens && Holes <= Was;
				bInside = bInside && Holes >= SeaSky::StratoHoles[0] && Holes <= SeaSky::StratoHoles[Last];
				Was = Holes;
			}
			Check("its gaps open as the cover falls, never past the table's ends, and it reads the table backward",
				bOpens && bInside && std::fabs(SeaSky::StratoHolesFor(SeaSky::StratoPainted[3]) - SeaSky::StratoHoles[3]) < 1e-9
					&& SeaSky::StratoHolesFor(1.0) == SeaSky::StratoHoles[0] && SeaSky::StratoHolesFor(0.0) == SeaSky::StratoHoles[Last],
				Num(SeaSky::StratoHolesFor(SeaSky::StratoPainted[3]), 3));
			const SeaSky::Sky Half = FairSky(0.5);
			AirSky::Day AsGiven;
			AsGiven.bGiven = true;
			AsGiven.Cover = 0.5;
			Check("a cumulus day of StratoFrom or more is drawn as a broken deck laid at DeckLeast, its gaps opened to read as the day's cover",
				Half.bLaid && Half.bStrato && Half.Day.bDeck && Half.Day.Cover == AirSky::DeckLeast && std::fabs(Half.Cover - 0.5) < 1e-9
					&& std::fabs(Half.DeckHoles - SeaSky::StratoHolesFor(0.5)) < 1e-9 && Half.DeckHoles > AirSky::DeckHoleBelow(AirSky::DeckLeast)
					&& Half.Why.find("cumulus of 0.50 (scattered), drawn as a broken deck of stratocumulus, 700 to 1400 m, its gaps opened at "
						+ Num(Half.DeckHoles, 2) + " so that it reads 0.50") != std::string::npos,
				Half.Why);
			Check("its word and its rain stay the day's own", Half.Day.Word == "scattered" && Half.Rain == SeaSky::RainOf(AsGiven, -1.0) && Half.Rain > 0.0,
				Half.Day.Word + ", rain " + Num(Half.Rain));
			SeaSky::Given Low, High;
			Low.bGiven = High.bGiven = true;
			Low.Day.bGiven = High.Day.bGiven = true;
			Low.Day.Cover = High.Day.Cover = 0.45;
			Low.Day.Base = 300.0;
			Low.Day.Top = 2600.0;
			High.Day.Base = 1800.0;
			High.Day.Top = 4000.0;
			const SeaSky::Sky Under = SeaSky::Choose(Low, SeaSky::Asked()), Over = SeaSky::Choose(High, SeaSky::Asked());
			Check("its base is the day's held to 600 to 1,000 m, its top 700 m over it",
				Under.Day.Base == SeaSky::StratoBaseLeast && Under.Day.Top == SeaSky::StratoBaseLeast + SeaSky::StratoDepth
					&& Over.Day.Base == SeaSky::StratoBaseMost && Over.Day.Top == SeaSky::StratoBaseMost + SeaSky::StratoDepth,
				Num(Under.Day.Base, 0) + " to " + Num(Under.Day.Top, 0) + " m, " + Num(Over.Day.Base, 0) + " to " + Num(Over.Day.Top, 0) + " m");
			Check("and the overview's eye follows its base, 150 m under it",
				SeaSky::OverviewHeight(Half.Day.bDeck, Half.Day.Base) == Half.Day.Base - SeaSky::OverviewUnderBase,
				Num(SeaSky::OverviewHeight(Half.Day.bDeck, Half.Day.Base), 0) + " m");
			const SeaSky::Sky Thick = FairSky(0.72);
			Check("a cumulus day of more than it can paint reads as the most it paints, at its fewest gaps",
				Thick.bStrato && Thick.Cover == SeaSky::StratoReach && Thick.DeckHoles == SeaSky::StratoHoles[0], Thick.Why);
			const AirSky::Lighting HalfLight = SeaSky::LightOf(Half, 1.0), DeckLight = AirSky::LightOf(SeaSky::ReadAs(Half), 1.0);
			const AirSky::Lighting ThickLight = SeaSky::LightOf(Thick, 1.0), OverTop = SeaSky::LightOf(Half, 0.0);
			Check("under it the light is the laid deck's taken as far as its cover reads of the most it paints: brighter than the deck's, darker than a clear day's",
				SeaSky::ReadAs(Half).bDeck && SeaSky::ReadAs(Half).Cover == AirSky::DeckLeast && HalfLight.Sun > DeckLight.Sun && HalfLight.Sun < 1.0
					&& std::fabs(1.0 - HalfLight.Sun - 0.5 / SeaSky::StratoReach * (1.0 - DeckLight.Sun)) < 1e-9 && HalfLight.Sky > 1.0
					&& HalfLight.Sky < DeckLight.Sky,
				"sun " + Num(HalfLight.Sun) + " against the deck's " + Num(DeckLight.Sun) + ", sky " + Num(HalfLight.Sky) + " against " + Num(DeckLight.Sky));
			Check("at its fewest gaps the deck's own light, and over its top a clear day's",
				std::fabs(ThickLight.Sun - AirSky::LightOf(SeaSky::ReadAs(Thick), 1.0).Sun) < 1e-9 && OverTop.Sun == 1.0 && OverTop.Sky == 1.0);
			Check("cumulus and the rules' decks keep air's light for the day as it reads",
				SeaSky::LightOf(FairSky(0.2), 0.0).Sky == AirSky::LightOf(SeaSky::ReadAs(FairSky(0.2)), 0.0).Sky
					&& SeaSky::LightOf(GaleSky(), 0.7).Sun == AirSky::LightOf(GaleSky().Day, 0.7).Sun);
			SeaLookSwitches::Look Review = Asked({ { "TheaterSeaWeather=", "0.5" } });
			const SeaSky::Sky Pictures = SeaSky::Choose(SeaSky::Given(), SeaSky::Take(Review));
			Check("a half-cloudy day asked for review pictures is drawn so too", Pictures.bStrato && Pictures.Day.bDeck && std::fabs(Pictures.Cover - 0.5) < 1e-9,
				Pictures.Why);
			const SeaSky::Sky Gale = GaleSky();
			Check("a deck the rules send is no stratocumulus, its gaps air's own", !Gale.bStrato && Gale.DeckHoles < 0.0);
			const AirSky::Placing P = SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind);
			AirSky::Placing Opened = P;
			Opened.DeckHoles = Half.DeckHoles;
			const AirSky::Field Mine = SeaSky::Lay(Half, P);
			Check("the sea lays it as AirSky lays that deck with those gaps, byte for byte", HashOf(Mine) == HashOf(AirSky::Lay(Half.Day, Opened)) && !Mine.Clouds.empty(),
				std::to_string(Mine.Clouds.size()) + " tiles");
			Check("and has fewer tiles than air's own deck at DeckLeast", Mine.Clouds.size() < AirSky::Lay(Half.Day, P).Clouds.size(),
				std::to_string(Mine.Clouds.size()) + " against " + std::to_string(AirSky::Lay(Half.Day, P).Clouds.size()));
			AirSky::Placing Said = P;
			Said.DeckHoles = AirSky::DeckHoleBelow(GaleSky().Day.CoverLaid());
			Check("AirSky::Placing::DeckHoles left at -1 is air's own deck, the same as its own threshold written out",
				P.DeckHoles < 0.0 && HashOf(AirSky::Lay(GaleSky().Day, P)) == HashOf(AirSky::Lay(GaleSky().Day, Said)));
		}
		{
			const AirSky::Placing P = SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind);
			Check("the placing: the board's middle the target, no ships' hole, no bombers' band, room for 1,500 puffs",
				std::fabs(P.TargetX - MiddleX) < 1e-9 && std::fabs(P.TargetY - MiddleY) < 1e-9 && !P.bShips && P.Band == 0.0 && P.LowBand == 0.0
					&& P.MostPuffs == SeaSky::MostPuffs && !P.Ground, std::to_string(P.MostPuffs));
			Check("and the field's own wind", std::fabs(P.Wind.X - GaleWind.X) < 1e-9 && std::fabs(P.Wind.Y - GaleWind.Y) < 1e-9);
			const AirSky::Placing Plain;
			Check("AirSky::Placing's wind, where a battle gives none, is the air battle's own",
				std::fabs(Plain.Wind.X - AirSky::Wind.X) < 1e-9 && std::fabs(Plain.Wind.Y - AirSky::Wind.Y) < 1e-9 && std::fabs(Plain.Wind.Z - AirSky::Wind.Z) < 1e-9);
			int LatX = 0, LatY = 0;
			AirSky::WindLattice(AirSky::Wind, LatX, LatY);
			Check("and its rows and rolls repeat on the air battle's own (2, 3) step", LatX == 2 && LatY == 3,
				std::to_string(LatX) + ", " + std::to_string(LatY));
			int GaleX = 0, GaleY = 0;
			AirSky::WindLattice(GaleWind, GaleX, GaleY);
			const double Along = GaleX * GaleWind.X + GaleY * GaleWind.Y;
			Check("a wind of its own turns them across it, whole numbers of waves to the square",
				(GaleX != 0 || GaleY != 0) && std::fabs(Along) < 0.25 * std::hypot(static_cast<double>(GaleX), static_cast<double>(GaleY)) * GaleWind.Length(),
				std::to_string(GaleX) + ", " + std::to_string(GaleY));
			const AirSky::Field Turned = SeaSky::Lay(FairSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
			const AirSky::Field Air = SeaSky::Lay(FairSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind));
			Check("the same day on two winds is two skies: the rows run along each", HashOf(Turned) != HashOf(Air));
			Check("and the wind is kept with the laying", std::fabs(Turned.Wind.X - GaleWind.X) < 1e-9 && Turned.AcrossX == GaleX && Turned.AcrossY == GaleY);
		}
	}

	// ---- The dome's mapping ---------------------------------------------------------------------------------------

	void Mapping()
	{
		double Worst = 0.0, WorstUv = 0.0;
		for (int Round = 0; Round < 360; Round += 7)
		{
			for (int Up = 0; Up <= 90; Up += 3)
			{
				const double Bearing = Round * Pi / 180.0, High = Up * Pi / 180.0;
				const Vec Way(std::cos(High) * std::sin(Bearing), std::cos(High) * std::cos(Bearing), std::sin(High));
				double Column = 0.0, Row = 0.0;
				SeaSky::PlaceOf(Way, SeaSky::DomeW, SeaSky::DomeH, Column, Row);
				const Vec Back = SeaSky::WayOf(Column, Row, SeaSky::DomeW, SeaSky::DomeH);
				Worst = std::max(Worst, (Back - Way).Length());
				// (the material's own numbers, the world's axes: its x is the board's north, its y the board's east)
				double U = 0.0, V = 0.0;
				SeaSky::DomeUv(Way.Y, Way.X, Way.Z, U, V);
				WorstUv = std::max(WorstUv, std::max(std::fabs(U - Column / SeaSky::DomeW), std::fabs(V - Row / SeaSky::DomeH)));
			}
		}
		Check("a way to the picture and back is the same way", Worst < 1e-9, Num(Worst, 12));
		Check("M_SkyDome's own texture coordinates are the picture's places", WorstUv < 1e-12, Num(WorstUv, 12));
		// (the rows even in the sine of the height: the sky near the horizon has the most of them)
		const double AtHorizon = (SeaSky::WayOf(0.0, SeaSky::DomeH - 1.5, SeaSky::DomeW, SeaSky::DomeH).Z
			- SeaSky::WayOf(0.0, SeaSky::DomeH - 0.5, SeaSky::DomeW, SeaSky::DomeH).Z);
		const double Rows = 1.0 / (std::asin(1.0 / SeaSky::DomeH) * 180.0 / Pi);
		Check("the rows are even in the sine of the height: about nine to a degree at the horizon", AtHorizon > 0.0 && Rows > 8.0 && Rows < 10.0, Num(Rows, 1));
		const double Across = SeaSky::DomeW / 360.0;
		Check("and the columns about eleven to a degree across", Across > 11.0 && Across < 11.7, Num(Across, 1));
	}

	// ---- The dome -------------------------------------------------------------------------------------------------

	AirSky::Field OnePuffField(double X, double Y, double Z, double R);

	/** The caps of the picture round the zenith: the rows from the zenith down to Degrees over the horizon. */
	int CapRows(double Degrees, int H)
	{
		return std::max(1, static_cast<int>(std::ceil((1.0 - std::sin(Degrees * Pi / 180.0)) * H)));
	}

	void PaintOne(const AirSky::Field& F, const Vec& At, SeaSky::Dome& Out, int Rows = -1, int W = SeaSky::DomeW, int H = SeaSky::DomeH)
	{
		AirSky::Painter Paint;
		SeaSky::DomeHow How;
		How.W = W;
		How.H = H;
		How.Rows = Rows;
		SeaSky::PaintDome(F, At, Vec(), TestSun, Rgb(1.0f, 1.0f, 1.0f), Paint, Out, How);
	}

	/** An exhaustive painter, the box left out: every pixel of the picture against every triangle the painter lays. What
	 * PaintDome must paint, byte for byte, however it narrows its work. */
	void Exhaustive(const AirSky::Field& F, const Vec& At, int W, int H, std::vector<uint8_t>& Out)
	{
		AirSky::Painter Paint;
		Paint.SetInk(1.3);
		std::vector<Vec> Points, Shadows;
		std::vector<uint8_t> Colours, ShadowColours;
		std::vector<float> Places;
		Paint.Paint(F, At, Vec(), TestSun, Rgb(1.0f, 1.0f, 1.0f), Points, Colours, &Shadows, &ShadowColours, &Places);
		const std::vector<int> Triangles = Paint.Triangles(Paint.RoomFor(F));
		std::vector<double> Laid(static_cast<size_t>(W) * H * 4, 0.0);
		for (size_t K = 0; K + 5 < Triangles.size(); K += 6)
		{
			const Vec A = Points[static_cast<size_t>(Triangles[K])] - At;
			const Vec B = Points[static_cast<size_t>(Triangles[K + 1])] - At;
			const Vec C = Points[static_cast<size_t>(Triangles[K + 2])] - At;
			const Vec BxC = B.Cross(C), CxA = C.Cross(A), AxB = A.Cross(B);
			const double Det = A.Dot(BxC);
			if (std::fabs(Det) < 1e-9)
			{
				continue;
			}
			double Alphas[3], Reds[3], Greens[3], Blues[3];
			bool bAny = false;
			for (int J = 0; J < 3; ++J)
			{
				const size_t I = static_cast<size_t>(Triangles[K + static_cast<size_t>(J)]);
				Reds[J] = Colours[I * 4] / 255.0;
				Greens[J] = Colours[I * 4 + 1] / 255.0;
				Blues[J] = Colours[I * 4 + 2] / 255.0;
				Alphas[J] = Colours[I * 4 + 3] / 255.0;
				bAny = bAny || Alphas[J] > 0.0;
			}
			if (!bAny)
			{
				continue;
			}
			for (int Y = 0; Y < H; ++Y)
			{
				for (int X = 0; X < W; ++X)
				{
					const Vec Way = SeaSky::WayOf(X + 0.5, Y + 0.5, W, H);
					const double WA = Way.Dot(BxC), WB = Way.Dot(CxA), WC = Way.Dot(AxB);
					const bool bIn = Det > 0.0 ? (WA >= 0.0 && WB >= 0.0 && WC >= 0.0) : (WA <= 0.0 && WB <= 0.0 && WC <= 0.0);
					if (!bIn)
					{
						continue;
					}
					const double Sum = WA + WB + WC;
					if (std::fabs(Sum) < 1e-12)
					{
						continue;
					}
					const double LA = WA / Sum, LB = WB / Sum, LC = WC / Sum;
					const double Alpha = Alphas[0] * LA + Alphas[1] * LB + Alphas[2] * LC;
					if (Alpha <= 0.0)
					{
						continue;
					}
					double* Into = &Laid[(static_cast<size_t>(Y) * W + X) * 4];
					const double Keep = 1.0 - Alpha;
					Into[0] = Into[0] * Keep + (Reds[0] * LA + Reds[1] * LB + Reds[2] * LC) * Alpha;
					Into[1] = Into[1] * Keep + (Greens[0] * LA + Greens[1] * LB + Greens[2] * LC) * Alpha;
					Into[2] = Into[2] * Keep + (Blues[0] * LA + Blues[1] * LB + Blues[2] * LC) * Alpha;
					Into[3] = Into[3] * Keep + Alpha;
				}
			}
		}
		Out.assign(static_cast<size_t>(W) * H * 4, 0);
		for (size_t K = 0; K < static_cast<size_t>(W) * H; ++K)
		{
			const double Alpha = std::min(1.0, std::max(0.0, Laid[K * 4 + 3]));
			if (Alpha > 1e-6)
			{
				for (int J = 0; J < 3; ++J)
				{
					Out[K * 4 + static_cast<size_t>(J)] = static_cast<uint8_t>(std::min(255.0, std::max(0.0, Laid[K * 4 + static_cast<size_t>(J)] / Alpha * 127.5 + 0.5)));
				}
			}
			Out[K * 4 + 3] = static_cast<uint8_t>(std::min(255.0, std::max(0.0, Alpha * 255.0 + 0.5)));
		}
	}

	/** The share of the caps of a picture under cloud, from a ring of eyes round the board's middle: the mean of the
	 * alpha painted above Degrees over the horizon, which is the share of that sky the clouds cover. */
	double CapCover(const AirSky::Field& F, double Degrees, int Eyes, double EyeZ, const std::vector<uint8_t>* Sheet = nullptr,
		int W = SeaSky::DomeW, int H = SeaSky::DomeH, double From = 5000.0, double To = 8600.0)
	{
		const int Rows = CapRows(Degrees, H);
		double Sum = 0.0;
		for (int K = 0; K < Eyes; ++K)
		{
			// (round the ring the field's own cover is measured over: a cap from one eye sees only the sky a few hundred
			// metres overhead, so it takes many of them to measure a cover)
			const double Round = 2.0 * Pi * (K % 10) / 10.0 + 0.31 * (K / 10);
			const double Away = Eyes > 10 ? From + (To - From) * (K / 10) / std::max(1, (Eyes - 1) / 10) : 0.5 * (From + To);
			AirSky::Painter Paint;
			SeaSky::DomeHow How;
			How.W = W;
			How.H = H;
			How.Rows = Rows;
			How.Sheet = Sheet;
			SeaSky::Dome Cap;
			SeaSky::PaintDome(F, Vec(MiddleX + Away * std::cos(Round), MiddleY + Away * std::sin(Round), EyeZ), Vec(), TestSun,
				Rgb(1.0f, 1.0f, 1.0f), Paint, Cap, How);
			double Alpha = 0.0;
			for (int Y = 0; Y < Rows; ++Y)
			{
				for (int X = 0; X < Cap.W; ++X)
				{
					Alpha += Cap.Rgba[(static_cast<size_t>(Y) * Cap.W + X) * 4 + 3] / 255.0;
				}
			}
			Sum += Alpha / (static_cast<double>(Rows) * Cap.W);
		}
		return Sum / std::max(1, Eyes);
	}

	/** The share of the whole sky under cloud seen from the sea (SeaSky::Dome::Cover: each pixel the same piece of sky,
	 * as a sky's eighths are counted), from a ring of eyes 5 to 8.6 km round the board's middle at a quarter of the
	 * picture, as Tools/sea_waves/cover_table measures SeaSky::CumulusPainted. */
	double WholeCover(const AirSky::Field& F, int Eyes, const std::vector<uint8_t>* Sheet)
	{
		double Sum = 0.0;
		for (int K = 0; K < Eyes; ++K)
		{
			const double Round = 2.0 * Pi * (K % 10) / 10.0 + 0.31 * (K / 10);
			const double Away = 5000.0 + 3600.0 * (K / 10) / std::max(1, (Eyes - 1) / 10);
			AirSky::Painter Paint;
			SeaSky::DomeHow How;
			How.W = SeaSky::DomeW / 4;
			How.H = SeaSky::DomeH / 4;
			How.Sheet = Sheet;
			SeaSky::Dome D;
			SeaSky::PaintDome(F, Vec(MiddleX + Away * std::cos(Round), MiddleY + Away * std::sin(Round), 28.0), Vec(), TestSun,
				Rgb(1.0f, 1.0f, 1.0f), Paint, D, How);
			Sum += D.Cover();
		}
		return Sum / std::max(1, Eyes);
	}

	void DomeChecks()
	{
		const AirSky::Placing P = SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind);
		// (the painter's own checks on the art director's 0.39 laid as it is, not as the sea lays it to read: CumulusLaidFor)
		AirSky::Day AsPassed = FairSky().Day;
		AsPassed.Cover = AirSky::DefaultCumulus;
		const AirSky::Field F = AirSky::Lay(AsPassed, P);
		Check("a scattered cumulus day lays its clouds", F.Clouds.size() > 50 && !F.Puffs.empty(),
			std::to_string(F.Clouds.size()) + " clouds, " + std::to_string(F.Puffs.size()) + " puffs, cover " + Num(F.Cover));
		SeaSky::Dome One, Two;
		const auto T0 = Clock::now();
		PaintOne(F, Vec(MiddleX, MiddleY, 28.0), One);
		Note("a whole dome of " + std::to_string(One.W) + " by " + std::to_string(One.H) + ": " + std::to_string(One.Puffs) + " puffs, "
			+ std::to_string(One.Triangles) + " triangles, " + std::to_string(One.Tested) + " pixels tested, " + Num(Since(T0), 2) + " s");
		PaintOne(F, Vec(MiddleX, MiddleY, 28.0), Two);
		Check("the same field, eye, drift, sun and light give the same picture, byte for byte", One.Rgba == Two.Rgba);
		// (the room of the plan's ledger, 1,500 puffs: a cloudy day has more than that within the reach, and the farthest
		// are left out, which the far deck sheet and the haze band stand in for at the horizon)
		Check("the painter fills its room of 1,500 puffs, the farthest left out", One.Puffs == SeaSky::MostPuffs && One.Dropped > 0,
			std::to_string(One.Puffs) + " drawn, " + std::to_string(One.Dropped) + " left out");
		SeaSky::Dome Moved;
		PaintOne(F, Vec(MiddleX + 3000.0, MiddleY, 28.0), Moved);
		Check("a dome from another eye is another picture", Moved.Rgba != One.Rgba);
		// The painting itself: against a painter that tests every pixel of the picture against every triangle, with no
		// box at all. The box, the turn round the picture's seam and the zenith must lose nothing.
		for (const Vec& At : { Vec(MiddleX, MiddleY, 28.0), Vec(MiddleX + 3000.0, MiddleY - 1000.0, 60.0) })
		{
			std::vector<uint8_t> Every;
			Exhaustive(F, At, 128, 16, Every);
			SeaSky::Dome Boxed;
			PaintOne(F, At, Boxed, -1, 128, 16);
			// (the dilation fills the empty pixels' colour, which the exhaustive painter leaves black: the cloud itself,
			// where there is any, must be the same)
			long Off = 0, Worst = 0, Colour = 0;
			for (size_t K = 0; K < 128 * 16; ++K)
			{
				const long Away = static_cast<long>(Boxed.Rgba[K * 4 + 3]) - Every[K * 4 + 3];
				Off += Away != 0 ? 1 : 0;
				Worst = std::max(Worst, std::labs(Away));
				for (int J = 0; Every[K * 4 + 3] > 0 && J < 3; ++J)
				{
					Colour = std::max(Colour, std::labs(static_cast<long>(Boxed.Rgba[K * 4 + static_cast<size_t>(J)]) - Every[K * 4 + static_cast<size_t>(J)]));
				}
			}
			Check("the picture is what a painter that tests every pixel against every triangle paints", Off == 0 && Colour == 0,
				"eye " + Num(At.X, 0) + ", " + Num(At.Y, 0) + ", " + Num(At.Z, 0) + ": " + std::to_string(Off) + " pixels off, worst alpha "
					+ std::to_string(Worst) + ", worst colour " + std::to_string(Colour));
		}
		// The share of the sky the clouds cover, measured on the caps round the zenith (60 degrees up and over) from a
		// ring of eyes 5 to 8.6 km from the middle, the ring the field's own cover is measured over.
		const double Painted = CapCover(F, 60.0, 40, 28.0);
		const double Coarse = CapCover(F, 60.0, 40, 28.0, nullptr, SeaSky::DomeW / 4, SeaSky::DomeH / 4);
		Check("the painted cover does not depend on how big the picture is", std::fabs(Painted - Coarse) < 0.02,
			Num(Painted) + " at " + std::to_string(SeaSky::DomeW) + " by " + std::to_string(SeaSky::DomeH) + ", " + Num(Coarse) + " at a quarter of that");
		// The plan (step 13) asks for the painted cover within 5 per cent of the field's. Measured, it is not: the field's
		// Cover is a footprint (AirSky::Field::CoverOf counts a board point under a cloud when it lies within 0.45 of the
		// cloud's width of its middle and the cloud is more than half shown), while the sky the clouds paint is their own
		// drawn bodies, whose puffs fade at their rims and, drawn from the sheet of torn fragments, are ragged and full of
		// gaps. The two differ by a steady factor, which is what is held here: the air battle's cumulus paints about half
		// its footprint cover, and a deck's lids about six sevenths of theirs.
		std::vector<uint8_t> Sheet;
		AirSky::MakeFragments(Sheet);
		const double Shapes = CapCover(F, 60.0, 40, 28.0, &Sheet);
		Note("the plan's 5 per cent of the field's cover does not hold and is not held here: cumulus of " + Num(F.Cover)
			+ " paints " + Num(Painted) + " as soft discs and " + Num(Shapes) + " from the sheet of shapes");
		Check("a cumulus day paints about half the sky its footprint cover claims, the shapes a little less than the discs",
			Painted / F.Cover > 0.40 && Painted / F.Cover < 0.65 && Shapes < Painted,
			Num(Painted / F.Cover) + " of the field's as discs, " + Num(Shapes / F.Cover) + " from the sheet");
		double Was = 0.0;
		bool bRises = true, bNear = true;
		std::string Said;
		for (const double Cover : { 0.65, 0.80, 0.95, 1.00 })
		{
			SeaSky::Given Field;
			Field.bGiven = true;
			Field.Day.bGiven = true;
			Field.Day.bDeck = true;
			Field.Day.Base = 600.0;
			Field.Day.Top = 2200.0;
			Field.Day.Cover = Cover;
			const AirSky::Field Deck = SeaSky::Lay(SeaSky::Choose(Field, SeaSky::Asked()), SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind));
			const double Lid = CapCover(Deck, 60.0, 16, 20.0, &Sheet, SeaSky::DomeW / 2, SeaSky::DomeH / 2, 3000.0, 4600.0);
			bRises = bRises && Lid > Was;
			// (under seven eighths its torn lids alone, about six sevenths of the cover; from there to a full overcast the
			// dome shuts the gaps behind them, SeaSky::DeckClosing)
			bNear = bNear && (Cover < SeaSky::DeckOpenUnder ? Lid / Cover > 0.78 && Lid / Cover < 0.92 : Lid / Cover > 0.92 && Lid <= 1.0);
			Was = Lid;
			Said += (Said.empty() ? "" : ", ") + Num(Cover, 2) + " paints " + Num(Lid);
		}
		Check("a deck paints about six sevenths of its day's cover under seven eighths, its gaps shut over that, and more of it the more cloud the day has",
			bRises && bNear, Said);
		// The sea scene's rule for the plan's cover line (25 November): the sky as the rules deal it, swept from a clear day
		// to a full overcast in tenths (cumulus under DeckLeast, a deck from it): nothing painted at none, at least nine
		// tenths at a full overcast, never less as the cover rises, and the worst gap between the field's cover and the
		// painted one held as a ceiling.
		{
			double Before = -1.0, Gap = 0.0, Full = 0.0, None = 1.0;
			bool bNeverLess = true;
			std::string Swept;
			for (int Tenth = 0; Tenth <= 10; ++Tenth)
			{
				const double Cover = Tenth / 10.0;
				SeaSky::Given Field;
				Field.bGiven = true;
				Field.Day.bGiven = true;
				Field.Day.bDeck = Cover >= AirSky::DeckLeast;
				Field.Day.Base = 600.0;
				Field.Day.Top = Field.Day.bDeck ? 2200.0 : 2600.0;
				Field.Day.Cover = Cover;
				const SeaSky::Sky Chosen = SeaSky::Choose(Field, SeaSky::Asked());
				const AirSky::Field Dealt = SeaSky::Lay(Chosen, SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind));
				// (each kind as it is laid measured as above: cumulus over the field's own ring, a deck over its nearer one;
				// cumulus from 100 eyes, since 40 scatter by about a hundredth, as much as air's cumulus grows from 0.3 to 0.4
				// of cover; the broken stratocumulus as cumulus is, over three seeds, since its gaps are kilometres wide and
				// one seed's ring of eyes may stand under one)
				double Seen = 0.0;
				if (Chosen.bStrato)
				{
					for (const uint64_t Seed : { 1942ull, 241ull, 77ull })
					{
						Seen += CapCover(SeaSky::Lay(Chosen, SeaSky::PlacingFor(Seed, MiddleX, MiddleY, TestSun, GaleWind)), 60.0, 100, 28.0, &Sheet,
							SeaSky::DomeW / 4, SeaSky::DomeH / 4) / 3.0;
					}
				}
				else if (!Dealt.Clouds.empty())
				{
					Seen = Dealt.Sky.bDeck ? CapCover(Dealt, 60.0, 10, 20.0, &Sheet, SeaSky::DomeW / 4, SeaSky::DomeH / 4, 3000.0, 4600.0)
										   : CapCover(Dealt, 60.0, 100, 28.0, &Sheet, SeaSky::DomeW / 4, SeaSky::DomeH / 4);
				}
				bNeverLess = bNeverLess && Seen >= Before;
				Before = Seen;
				Gap = std::max(Gap, Cover - Seen);
				None = Tenth == 0 ? Seen : None;
				Full = Tenth == 10 ? Seen : Full;
				Swept += (Swept.empty() ? "" : ", ") + Num(Cover, 1) + " " + Num(Seen, 4);
			}
			Check("a clear day paints no cloud at all", None <= 0.0, Num(None, 4));
			Check("a full overcast paints at least nine tenths of the sky", Full >= 0.9, Num(Full));
			Check("the painted cover is never less as the dealt cover rises, from none to a full overcast", bNeverLess, Swept);
			// (the worst gap, measured 25 November, kept as a ceiling: it may shrink, never grow)
			Check("the worst gap between the field's cover and the painted one is no wider than measured (" + Num(SeaSky::WorstCoverGap, 2) + ")",
				Gap <= SeaSky::WorstCoverGap, Num(Gap, 3));
		}
		// The sea's cumulus as it reads (the sea scene's follow-up and ruling, 25 November): a day of the rules laid at the
		// cover that paints its own, by the whole sky seen from the sea, the mean over three seeds: as cumulus up to what
		// it can paint (SeaSky::CumulusReach), and from SeaSky::StratoFrom as a broken deck of stratocumulus, within five
		// hundredths of the day's (Sky::Cover).
		{
			std::string Reads;
			bool bReads = true;
			for (const double Cover : { 0.1, 0.2, 0.3, 0.4, 0.5 })
			{
				SeaSky::Given Field;
				Field.bGiven = true;
				Field.Day.bGiven = true;
				Field.Day.Base = 600.0;
				Field.Day.Top = 2600.0;
				Field.Day.Cover = Cover;
				const SeaSky::Sky Chosen = SeaSky::Choose(Field, SeaSky::Asked());
				double Seen = 0.0;
				for (const uint64_t Seed : { 1942ull, 241ull, 77ull })
				{
					Seen += WholeCover(SeaSky::Lay(Chosen, SeaSky::PlacingFor(Seed, MiddleX, MiddleY, TestSun, GaleWind)), 10, &Sheet) / 3.0;
				}
				const double Want = Chosen.bStrato ? Cover : std::min(Cover, SeaSky::CumulusReach);
				bReads = bReads && std::fabs(Chosen.Cover - Want) < 1e-9 && std::fabs(Seen - Want) <= 0.05;
				Reads += (Reads.empty() ? "" : ", ") + Num(Cover, 1)
					+ (Chosen.bStrato ? " a broken deck with gaps at " + Num(Chosen.DeckHoles, 2) : " laid at " + Num(Chosen.Day.Cover, 2)) + " paints " + Num(Seen);
			}
			Check("a cumulus day paints the sky its cover says, as cumulus to " + Num(SeaSky::CumulusReach) + " and from " + Num(SeaSky::StratoFrom, 2)
					+ " as broken stratocumulus, within five hundredths",
				bReads, Reads);
		}
		// The overview's eye under a deck the dome draws (the plan's line 941).
		Check("the overview stays at 1,300 m with no deck drawn, whatever the base", SeaSky::OverviewHeight(false, 450.0) == 1300.0
			&& SeaSky::OverviewHeight(false, 0.0) == 1300.0, Num(SeaSky::OverviewHeight(false, 450.0), 0));
		Check("under a deck it is 150 m under the base, never over 1,300 m nor under 300 m",
			SeaSky::OverviewHeight(true, 1000.0) == 850.0 && SeaSky::OverviewHeight(true, 2500.0) == 1300.0
				&& SeaSky::OverviewHeight(true, 450.0) == 300.0 && SeaSky::OverviewHeight(true, 1450.0) == 1300.0,
			Num(SeaSky::OverviewHeight(true, 1000.0), 0) + " under a 1,000 m base, " + Num(SeaSky::OverviewHeight(true, 450.0), 0) + " under 450 m");
		// Nothing is lost where the picture's columns come round again (the seam at due north).
		const double Span = 4000.0;
		double Seam = 0.0, Side = 0.0;
		for (int K = 0; K < 2; ++K)
		{
			const double AX = K == 0 ? MiddleX : MiddleX + Span, AY = K == 0 ? MiddleY + Span : MiddleY;
			const AirSky::Field Alone = OnePuffField(AX, AY, 900.0, 400.0);
			SeaSky::Dome Shown;
			PaintOne(Alone, Vec(MiddleX, MiddleY, 28.0), Shown);
			double Sum = 0.0;
			for (size_t J = 0; J < Shown.Rgba.size(); J += 4)
			{
				Sum += Shown.Rgba[J + 3] / 255.0;
			}
			(K == 0 ? Seam : Side) = Sum / (static_cast<double>(Shown.W) * Shown.H);
		}
		Check("a cloud over the picture's seam, due north, is painted whole", Seam > 0.0 && std::fabs(Seam - Side) / Side < 0.03,
			"due north " + Num(Seam, 5) + ", due east " + Num(Side, 5));
		// (an overcast deck: the sky nearly all cloud overhead)
		const AirSky::Field Gale = SeaSky::Lay(GaleSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
		const double Shut = CapCover(Gale, 60.0, 10, 28.0, &Sheet, SeaSky::DomeW / 2, SeaSky::DomeH / 2, 0.0, 0.0);
		Check("an overcast deck of 0.95 is painted shut overhead", Shut > 0.75, Num(Shut) + " of the caps under cloud");
	}

	// ---- The bearings and the shadows -----------------------------------------------------------------------------

	/** One cloud of one puff, laid by hand on the laid square: for the bearing and the shadow, measured against the
	 * line from the eye. */
	AirSky::Field OnePuffField(double X, double Y, double Z, double R)
	{
		AirSky::Field F;
		F.Sky.bGiven = true;
		F.Sky.Cover = 0.2;
		F.Base = Z - R;
		F.Top = Z + R;
		F.Origin = Vec(MiddleX - 0.5 * AirSky::Period, MiddleY - 0.5 * AirSky::Period, 0.0);
		F.TargetX = MiddleX;
		F.TargetY = MiddleY;
		F.MostPuffs = 16;
		F.Serial = 0x5EA5EA;
		AirSky::Cloud& C = F.Clouds.emplace_back();
		C.At = Vec(X, Y, Z - R);
		C.W = 2.0 * R;
		C.H = 2.0 * R;
		C.First = 0;
		C.Count = 1;
		C.HeartReach = R;
		C.RimReach = R;
		AirSky::Puff& Each = F.Puffs.emplace_back();
		Each.At = Vec(X, Y, Z);
		Each.R = R;
		Each.Flat = 1.0;
		Each.Lit = 1.0;
		Each.Alpha = 1.0;
		Each.Cloud = 0;
		Each.Lod = 0;
		return F;
	}

	void Bearings()
	{
		// A single puff 4 km north-east of the middle: its bearing and height above the horizon from every planned eye,
		// against the line from that eye to it, within a texel of the picture.
		const double PX = MiddleX + 2800.0, PY = MiddleY + 2800.0, PZ = 900.0, PR = 300.0;
		const AirSky::Field F = OnePuffField(PX, PY, PZ, PR);
		double WorstColumn = 0.0, WorstRow = 0.0;
		std::string Where;
		for (const Eye& E : PlannedEyes())
		{
			// (the dome is the sky over the horizon: a cloud under the eye is not in it, so the overview, which flies at
			// 1,300 m, is given a cloud over its own head. The scene must keep its eye under the day's base, or hold the
			// clouds it flies among in the puffs mesh instead: the plan's step 13(e))
			const double Deep = std::max(PZ, E.At.Z + 600.0);
			const AirSky::Field Over = OnePuffField(PX, PY, Deep, PR);
			SeaSky::Dome Shown;
			PaintOne(Over, E.At, Shown);
			// (the middle of what was painted: every pixel's way weighted by its alpha, in the picture's own places)
			double Weight = 0.0, Column = 0.0, Row = 0.0;
			double Least = 1e30, Most = -1e30;
			for (int Y = 0; Y < Shown.H; ++Y)
			{
				for (int X = 0; X < Shown.W; ++X)
				{
					const double Alpha = Shown.Rgba[(static_cast<size_t>(Y) * Shown.W + X) * 4 + 3] / 255.0;
					if (Alpha <= 0.0)
					{
						continue;
					}
					Least = std::min(Least, static_cast<double>(X));
					Most = std::max(Most, static_cast<double>(X));
					Weight += Alpha;
					Column += Alpha * (X + 0.5);
					Row += Alpha * (Y + 0.5);
				}
			}
			if (Weight <= 0.0 || Most - Least > 0.5 * Shown.W)
			{
				Check(std::string("the puff is painted from ") + E.Name, false, "nothing painted");
				continue;
			}
			Column /= Weight;
			Row /= Weight;
			double WantColumn = 0.0, WantRow = 0.0;
			SeaSky::PlaceOf(Vec(PX, PY, Deep) - E.At, Shown.W, Shown.H, WantColumn, WantRow);
			const double OffColumn = std::fabs(Column - WantColumn), OffRow = std::fabs(Row - WantRow);
			if (OffColumn + OffRow > WorstColumn + WorstRow)
			{
				WorstColumn = OffColumn;
				WorstRow = OffRow;
				Where = E.Name;
			}
		}
		Check("a cloud's bearing and height from every planned eye is the line from that eye to its puff, within a texel",
			WorstColumn <= 1.0 && WorstRow <= 1.0, "worst " + Num(WorstColumn, 2) + " columns and " + Num(WorstRow, 2) + " rows, at " + Where);
		{
			// (and what the dome cannot show: a cloud under the eye, which the overview at 1,300 m would fly over)
			SeaSky::Dome Under;
			PaintOne(F, Vec(MiddleX, MiddleY, 1300.0), Under);
			Check("a cloud under the eye is not in the dome, which is the sky over the horizon", Under.Cover() <= 0.0, Num(Under.Cover(), 5));
		}
		// Its shadow: where the sun's line through the cloud's middle crosses the sea, and the light function reads it dark.
		const AirSky::DeckShade Shade = SeaSky::ShadeOf(F, TestSun, Vec(), 256);
		Check("a cumulus day's clouds lay a shade picture of their own", Shade.Side == 256 && !Shade.Light.empty(), std::to_string(Shade.Side));
		const double High = F.Clouds[0].At.Z + 0.45 * F.Clouds[0].H;
		const Vec Sea(PX - TestSun.X * High / TestSun.Z, PY - TestSun.Y * High / TestSun.Z, 0.0);
		const double Dark = Shade.At(Sea, TestSun), Open = Shade.At(Vec(PX + 6000.0, PY + 6000.0, 0.0), TestSun);
		Check("the sea down-sun of a cloud lies in its shade, and the sea away from it in the sun",
			Dark < 0.45 && Open > 0.95, "under it " + Num(Dark) + ", away " + Num(Open));
		Check("and the heart of the shade is the cumulus's own transmission", std::fabs(Dark - SeaSky::CumulusThrough) < 0.02, Num(Dark));
		// (the picture's own middle of the shadow, against where the sun's line through the cloud crosses the picture)
		const Vec Want = SeaSky::ShadowPlace(F, 0, TestSun, Shade.Z);
		const double Step = Shade.Period / Shade.Side;
		double Weight = 0.0, MX = 0.0, MY = 0.0;
		for (int Y = 0; Y < Shade.Side; ++Y)
		{
			for (int X = 0; X < Shade.Side; ++X)
			{
				const double Deep = 1.0 - Shade.Light[static_cast<size_t>(Y) * Shade.Side + X] / 255.0;
				if (Deep <= 0.0)
				{
					continue;
				}
				// (the nearest copy of the square to where it is wanted: the picture repeats)
				const double AX = Shade.Origin.X + (X + 0.5) * Step, AY = Shade.Origin.Y + (Y + 0.5) * Step;
				const double FoldX = AX - Shade.Period * std::round((AX - Want.X) / Shade.Period);
				const double FoldY = AY - Shade.Period * std::round((AY - Want.Y) / Shade.Period);
				Weight += Deep;
				MX += Deep * FoldX;
				MY += Deep * FoldY;
			}
		}
		const double OffX = Weight > 0.0 ? std::fabs(MX / Weight - Want.X) : 1e9, OffY = Weight > 0.0 ? std::fabs(MY / Weight - Want.Y) : 1e9;
		Check("its shadow lies along the sun's line through it, within a texel of the shade picture",
			OffX <= Step && OffY <= Step, "off " + Num(OffX, 1) + " m east and " + Num(OffY, 1) + " m north, a texel being " + Num(Step, 1) + " m");
		// Every puff the painter drew stands in the picture where its middle points.
		{
			AirSky::Painter Paint;
			const AirSky::Field Fair = SeaSky::Lay(FairSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind));
			std::vector<Vec> Points, Shadows;
			std::vector<uint8_t> Colours, ShadowColours;
			std::vector<float> Places;
			Paint.SetInk(1.3);
			Paint.Paint(Fair, Vec(MiddleX, MiddleY, 28.0), Vec(), TestSun, Rgb(1.0f, 1.0f, 1.0f), Points, Colours, &Shadows, &ShadowColours, &Places);
			SeaSky::Dome Shown;
			PaintOne(Fair, Vec(MiddleX, MiddleY, 28.0), Shown);
			const int Per = Paint.PerPuff();
			int Looked = 0, Found = 0;
			for (size_t K = 0; K + static_cast<size_t>(Per) <= Points.size(); K += static_cast<size_t>(Per))
			{
				const double Alpha = Colours[K * 4 + 3] / 255.0;
				if (Alpha < 0.35)
				{
					continue;
				}
				const Vec Way = Points[K] - Vec(MiddleX, MiddleY, 28.0);
				if (Way.Z < 0.0)
				{
					continue;
				}
				++Looked;
				Found += Shown.AlphaAt(Way) >= 0.35 * Alpha ? 1 : 0;
			}
			Check("every puff the painter drew is cloud in the picture at its own middle's way", Looked > 100 && Found >= Looked - Looked / 50,
				std::to_string(Found) + " of " + std::to_string(Looked));
		}
	}

	// ---- The shade and the dome's holes ---------------------------------------------------------------------------

	void ShadeChecks()
	{
		const AirSky::Field Gale = SeaSky::Lay(GaleSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
		const AirSky::DeckShade Mine = SeaSky::ShadeOf(Gale, TestSun, Vec(), 256);
		const AirSky::DeckShade Air = AirSky::ShadeOf(Gale, 256);
		Check("a deck's shade is air's own, the same picture byte for byte", Mine.Light == Air.Light && Mine.Side == Air.Side
			&& std::fabs(Mine.Z - Air.Z) < 1e-9 && std::fabs(Mine.Through - Air.Through) < 1e-9, std::to_string(Mine.Side) + " a side");
		// A broken deck, where its holes can be told apart: the picture's cloud straight overhead against the shade's
		// own hole over the same board point.
		const AirSky::Field Broken = SeaSky::Lay(FairSky(0.72), SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind));
		Check("a heavy cumulus day is laid as a broken deck (the broken stratocumulus)", !Broken.Clouds.empty() && Broken.Sky.bDeck);
		const AirSky::Field Opened = SeaSky::Lay(FairSky(0.45), SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind));
		const AirSky::DeckShade Strato = SeaSky::ShadeOf(Opened, TestSun, Vec(), 256);
		const double AirOwn = AirSky::ShadeOf(AirSky::Lay(Opened.Sky, SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind)), 256).Mean();
		Check("a lighter one lays a deck's shade, more of the square in sun through its wider gaps than under air's own deck",
			Opened.Sky.bDeck && Strato.Side == 256 && Strato.Mean() > AirOwn + 0.05,
			Num(Strato.Mean()) + " of the sun on the whole at 0.45, air's own deck at 0.6 " + Num(AirOwn));
		SeaSky::Given Field;
		Field.bGiven = true;
		Field.Day.bGiven = true;
		Field.Day.bDeck = true;
		Field.Day.Base = 600.0;
		Field.Day.Top = 2200.0;
		Field.Day.Cover = 0.72;
		const AirSky::Field Deck = SeaSky::Lay(SeaSky::Choose(Field, SeaSky::Asked()), SeaSky::PlacingFor(1942, MiddleX, MiddleY, TestSun, GaleWind));
		const AirSky::DeckShade Holes = SeaSky::ShadeOf(Deck, TestSun, Vec(), 256);
		const int Rows = std::max(2, CapRows(85.0, 256));
		int Looked = 0, Agreed = 0;
		for (int K = 0; K < 24; ++K)
		{
			const double Round = 2.0 * Pi * K / 24.0, Away = 2000.0 + 500.0 * (K % 7);
			const double EX = MiddleX + Away * std::cos(Round), EY = MiddleY + Away * std::sin(Round);
			SeaSky::Dome Cap;
			PaintOne(Deck, Vec(EX, EY, 20.0), Cap, Rows, 1024, 256);
			double Alpha = 0.0;
			for (int Y = 0; Y < Rows; ++Y)
			{
				for (int X = 0; X < Cap.W; ++X)
				{
					Alpha += Cap.Rgba[(static_cast<size_t>(Y) * Cap.W + X) * 4 + 3] / 255.0;
				}
			}
			Alpha /= static_cast<double>(Rows) * Cap.W;
			// (the shade's own picture straight over that point: shut where the deck is drawn, open in a hole)
			const double Step = Holes.Period / Holes.Side;
			const int SX = ((static_cast<int>(std::floor((EX - Holes.Origin.X) / Step)) % Holes.Side) + Holes.Side) % Holes.Side;
			const int SY = ((static_cast<int>(std::floor((EY - Holes.Origin.Y) / Step)) % Holes.Side) + Holes.Side) % Holes.Side;
			const double Light = Holes.Light[static_cast<size_t>(SY) * Holes.Side + SX] / 255.0;
			const bool bOverhead = Alpha > 0.5, bShaded = Light < 0.5 * (1.0 + Holes.Through);
			++Looked;
			Agreed += bOverhead == bShaded ? 1 : 0;
		}
		Check("the dome's cloud overhead agrees with the shade picture's holes", Looked > 0 && Agreed * 100 >= Looked * 85,
			std::to_string(Agreed) + " of " + std::to_string(Looked) + " eyes");
		// A clear day casts no shade.
		SeaSky::Given Clear;
		Clear.bGiven = true;
		Clear.Day.bGiven = true;
		Clear.Day.Cover = 0.0;
		const AirSky::Field Nothing = SeaSky::Lay(SeaSky::Choose(Clear, SeaSky::Asked()), SeaSky::PlacingFor(7, MiddleX, MiddleY, TestSun, GaleWind));
		Check("a clear day lays no clouds and no shade", SeaSky::ShadeOf(Nothing, TestSun).Side == 0, std::to_string(Nothing.Clouds.size()) + " clouds");
		// The shade is the same picture twice over, and the drift moves it.
		const AirSky::Field Fair = SeaSky::Lay(FairSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind));
		const AirSky::DeckShade A = SeaSky::ShadeOf(Fair, TestSun, Vec(), 128);
		const AirSky::DeckShade B = SeaSky::ShadeOf(Fair, TestSun, Vec(), 128);
		const AirSky::DeckShade C = SeaSky::ShadeOf(Fair, TestSun, Fair.DriftAt(300.0), 128);
		Check("a cumulus shade picture is the same twice over, and the wind's carry moves it", A.Light == B.Light && A.Light != C.Light,
			Num(A.Mean()) + " of the sun over the square");
		Check("and it stands between the day's base and top", A.Z > Fair.Base && A.Z < Fair.Top && std::fabs(A.Top - Fair.Top) < 1e-9, Num(A.Z, 0) + " m");
	}

	// ---- Rain ---------------------------------------------------------------------------------------------------

	void RainChecks()
	{
		const SeaSky::Sky Chosen = GaleSky();
		const AirSky::Field Gale = SeaSky::Lay(Chosen, SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
		const SeaSky::Rain Wet = SeaSky::RainUnder(Gale, Chosen.Rain, 241);
		const SeaSky::Rain Again = SeaSky::RainUnder(Gale, Chosen.Rain, 241);
		Check("the gale's rain lays curtains, as many as six times how heavy it is",
			static_cast<int>(Wet.Curtains.size()) == static_cast<int>(std::lround(SeaSky::MostCurtains * Chosen.Rain)),
			std::to_string(Wet.Curtains.size()) + " curtains for rain " + Num(Chosen.Rain));
		Check("the same field and seed lay the same rain", Same(Wet.Points, Again.Points) && Wet.Alpha == Again.Alpha);
		bool bFar = true, bApart = true, bWide = true, bTall = true;
		for (size_t K = 0; K < Wet.Curtains.size(); ++K)
		{
			const SeaSky::Curtain& One = Wet.Curtains[K];
			const double Away = std::hypot(One.Mid.X - MiddleX, One.Mid.Y - MiddleY);
			bFar = bFar && Away >= SeaSky::CurtainsFrom && Away <= SeaSky::CurtainsTo;
			bWide = bWide && One.Radius >= 1500.0 && One.Radius <= 3000.0;
			bTall = bTall && One.Top > Gale.Base && One.Foot <= 0.0;
			for (size_t J = 0; J < K; ++J)
			{
				bApart = bApart && std::hypot(One.Mid.X - Wet.Curtains[J].Mid.X, One.Mid.Y - Wet.Curtains[J].Mid.Y) >= 5000.0;
			}
		}
		Check("each curtain stands 6 to 18 km from the middle, at least 5 km from the next", bFar && bApart);
		Check("a deck's rain is a band one and a half tiles across, from inside the base down to the sea", bWide && bTall);
		Check("the curtains' mesh is four crossed sheets of 5 by 5 points, both faces of every triangle",
			Wet.Points.size() == Wet.Curtains.size() * 4 * 25 && Wet.Alpha.size() == Wet.Points.size()
				&& Wet.Triangles.size() == Wet.Curtains.size() * 4 * 16 * 12,
			std::to_string(Wet.Points.size()) + " points, " + std::to_string(Wet.Triangles.size() / 3) + " triangles");
		int Most = 0;
		for (const uint8_t A : Wet.Alpha)
		{
			Most = std::max(Most, static_cast<int>(A));
		}
		const double Peak = 0.1 + 0.2 * Chosen.Rain;
		Check("each sheet's alpha at its thickest is 0.1 and 0.2 of how heavy the rain is", std::fabs(Most / 255.0 - Peak) < 0.01,
			Num(Most / 255.0) + " against " + Num(Peak));
		bool bLag = true;
		for (const SeaSky::Curtain& One : Wet.Curtains)
		{
			// (the foot lags upwind of the cloud: against the way the wind blows)
			bLag = bLag && One.Lag.X * GaleWind.X + One.Lag.Y * GaleWind.Y < 0.0;
		}
		Check("the rain lags upwind of its cloud as it falls into the slower air under it", bLag);
		const SeaSky::Squall One = SeaSky::SquallOf(Wet);
		bool bUnder = false;
		for (const SeaSky::Curtain& Each : Wet.Curtains)
		{
			bUnder = bUnder || (std::fabs(One.X - (Each.Mid.X + Each.Lag.X)) < 1.0 && std::fabs(One.Y - (Each.Mid.Y + Each.Lag.Y)) < 1.0);
		}
		Check("the squall's footprint is the heaviest curtain's foot on the sea", One.bOn && bUnder && One.R > 0.0 && std::fabs(One.Heavy - Chosen.Rain) < 1e-9,
			Num(One.R, 0) + " m across, " + Num(One.Heavy) + " heavy");
		// A fair day: no rain at all.
		const AirSky::Field Fair = SeaSky::Lay(FairSky(0.3), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind));
		const SeaSky::Rain Dry = SeaSky::RainUnder(Fair, SeaSky::RainOf(Fair.Sky, -1.0), 241);
		Check("a fair day has no rain and no squall", Dry.Curtains.empty() && Dry.Points.empty() && !SeaSky::SquallOf(Dry).bOn);
		// A cumulus day the rules send rain on: showers under the biggest clouds, each about a quarter of its cloud across.
		SeaSky::Given Showery;
		Showery.bGiven = true;
		Showery.Day.bGiven = true;
		Showery.Day.Base = 700.0;
		Showery.Day.Top = 2600.0;
		Showery.Day.Cover = 0.35;
		Showery.Rain = 0.6;
		const SeaSky::Sky Heavy = SeaSky::Choose(Showery, SeaSky::Asked());
		const AirSky::Field Showers = SeaSky::Lay(Heavy, SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind));
		const SeaSky::Rain Some = SeaSky::RainUnder(Showers, Heavy.Rain, 241);
		bool bShower = !Some.Curtains.empty() && !Showers.Sky.bDeck;
		for (const SeaSky::Curtain& Each : Some.Curtains)
		{
			bShower = bShower && Each.Radius >= 500.0 && Each.Radius <= 2200.0 && Each.Cloud >= 0;
		}
		Check("a wet cumulus day has showers, each about a quarter of its cloud across", bShower,
			std::to_string(Some.Curtains.size()) + " showers for rain " + Num(Heavy.Rain));
		// A heavy day of cumulus (0.55, its rain the day's own), drawn as the broken stratocumulus: its rain in bands.
		const SeaSky::Sky Broken = FairSky(0.55);
		const SeaSky::Rain Bands = SeaSky::RainUnder(SeaSky::Lay(Broken, SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, AirSky::Wind)), Broken.Rain, 241);
		bool bBands = Broken.bStrato && !Bands.Curtains.empty();
		for (const SeaSky::Curtain& Each : Bands.Curtains)
		{
			bBands = bBands && Each.Radius >= 1500.0 && Each.Radius <= 3000.0 && Each.Cloud >= 0;
		}
		Check("a heavy cumulus day drawn as broken stratocumulus rains as a deck does, in bands one and a half tiles across", bBands,
			std::to_string(Bands.Curtains.size()) + " bands for rain " + Num(Broken.Rain));
	}

	// ---- The deck's far sheet -------------------------------------------------------------------------------------

	void FarSheet()
	{
		const AirSky::Field Gale = SeaSky::Lay(GaleSky(), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
		SeaSky::Given Field;
		Field.bGiven = true;
		Field.Day.bGiven = true;
		Field.Day.bDeck = true;
		Field.Day.Cover = 0.62;
		const AirSky::Field Thin = SeaSky::Lay(SeaSky::Choose(Field, SeaSky::Asked()), SeaSky::PlacingFor(241, MiddleX, MiddleY, TestSun, GaleWind));
		const double Shut = SeaSky::FarSheetSolid(Gale), Open = SeaSky::FarSheetSolid(Thin);
		Check("the far sheet is whole under an overcast and two thirds or so under a broken deck",
			Shut > 0.9 && Open > 0.39 && Open < 0.55 && Shut > Open, Num(Open) + " at 0.62 of cover to " + Num(Shut) + " at 0.95");
		const Rgb Under = SeaSky::FarSheetColour(Gale, 10.0), Over = SeaSky::FarSheetColour(Gale, Gale.Top + 500.0);
		Check("seen from under its base it is the base's grey, and from over its top the pale sunlit lid",
			Under.R < Over.R && Under.G < Over.G && Under.B < Over.B,
			"under " + Num(Under.R) + "," + Num(Under.G) + "," + Num(Under.B) + " over " + Num(Over.R) + "," + Num(Over.G) + "," + Num(Over.B));
	}

	// ---- The two stand-in weather days on disk ---------------------------------------------------------------------

	std::string FileText(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			return std::string();
		}
		return std::string(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
	}

	void StandInDays()
	{
		// (Tools/sea_waves/stand-ins/weather_days.py writes the overcast, make_stand_ins.py the squall dawn; run from the project, or say where with
		// SEA_SKY_STANDINS. The sea scene plays one with -TheaterSeaBattle=<path>.)
		const char* Where = std::getenv("SEA_SKY_STANDINS");
		const std::string Dir = Where && *Where ? std::string(Where) : std::string("Tools/sea_waves/stand-ins");
		{
			const std::string Text = FileText(Dir + "/north-atlantic-deck.json");
			Check("the overcast stand-in is on disk", !Text.empty(), std::to_string(Text.size()) + " bytes");
			if (!Text.empty())
			{
				std::string Error;
				const SeaSky::Given Field = SeaSky::ReadRecording(Text, &Error);
				Check("its field sends the closed deck the plan asks for",
					Field.bGiven && Field.Day.bDeck && std::fabs(Field.Day.Cover - 0.95) < 1e-9
						&& std::fabs(Field.Day.Base - 450.0) < 1e-9 && std::fabs(Field.Day.Top - 2800.0) < 1e-9,
					Error.empty() ? Num(Field.Day.Cover) + " of cover, " + Num(Field.Day.Base, 0) + " to " + Num(Field.Day.Top, 0) + " m" : Error);
				const SeaSky::Sky Chosen = SeaSky::Choose(Field, SeaSky::Asked());
				Check("the day it lays rains 0.6, which the rain at the eye must be given too",
					Chosen.bLaid && std::fabs(Chosen.Rain - 0.6) < 1e-9, Num(Chosen.Rain));
				AirFeed::Json Whole;
				std::string Why;
				Check("the recording reads", AirFeed::Parse(Text, Whole, Why), Why);
				const AirFeed::Json& Wind = Whole["field"]["wind"];
				Check("its wind is the task's 13.5 m/s, force 6", std::fabs(Wind["speed"].Num(0.0) - 13.5) < 1e-9,
					Num(Wind["speed"].Num(0.0), 1) + " m/s from " + Num(Wind["from"].Num(0.0), 2) + " rad");
				Check("it is a daylight action: the sun is up, and no star shell is left in it",
					Whole["field"]["light"]["sun"].Num(-20.0) > 0.0 && Text.find("sea:starshell") == std::string::npos,
					Num(Whole["field"]["light"]["sun"].Num(-20.0), 1) + " degrees up");
			}
		}
		{
			const std::string Text = FileText(Dir + "/samar-dawn.json");
			Check("the squall dawn stand-in is on disk", !Text.empty(), std::to_string(Text.size()) + " bytes");
			if (!Text.empty())
			{
				std::string Error;
				const SeaSky::Given Field = SeaSky::ReadRecording(Text, &Error);
				Check("its field sends broken cumulus with its own rain",
					Field.bGiven && !Field.Day.bDeck && std::fabs(Field.Day.Cover - 0.50) < 1e-9
						&& std::fabs(Field.Day.Base - 600.0) < 1e-9 && std::fabs(Field.Day.Top - 3500.0) < 1e-9
						&& std::fabs(Field.Rain - 0.7) < 1e-9,
					Error.empty() ? Num(Field.Day.Cover) + " of cover, rain " + Num(Field.Rain) : Error);
				const SeaSky::Sky Chosen = SeaSky::Choose(Field, SeaSky::Asked());
				// (the squalls the carriers hid in are the day's point: heavier than the cover alone would give)
				const double Alone = SeaSky::RainOf(Chosen.Day, -1.0);
				Check("the rain the field names wins over the one its cover alone would give",
					Chosen.bLaid && std::fabs(Chosen.Rain - 0.7) < 1e-9 && Alone < Chosen.Rain,
					Num(Chosen.Rain) + " given against " + Num(Alone) + " from the cover");
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	const std::string Out = Argc > 1 ? Argv[1] : std::string();
	(void)Out;
	const auto T0 = Clock::now();
	struct Part
	{
		const char* Name;
		std::function<void()> Run;
	};
	const Part Parts[] = {
		{ "switches", [] { Switches(); } },
		{ "the day", [] { TheDay(); } },
		{ "the mapping", [] { Mapping(); } },
		{ "the dome", [] { DomeChecks(); } },
		{ "bearings and shadows", [] { Bearings(); } },
		{ "the shade", [] { ShadeChecks(); } },
		{ "rain", [] { RainChecks(); } },
		{ "the far sheet", [] { FarSheet(); } },
		{ "the stand-in days", [] { StandInDays(); } },
	};
	const char* Only = std::getenv("SEA_SKY_ONLY"); // (a list of parts, to run one alone while working on it)
	for (const Part& P : Parts)
	{
		if (Only && *Only && std::string(",").append(Only).append(",").find(std::string(",") + P.Name + ",") == std::string::npos)
		{
			continue;
		}
		const auto P0 = Clock::now();
		std::printf("-- %s\n", P.Name);
		P.Run();
		std::printf("   (%.1f s)\n", Since(P0));
		std::fflush(stdout);
	}
	std::printf("sea_sky_test: %d checks, %d failed (%.0f s)\n", Checks, Failed, Since(T0));
	return Failed == 0 ? 0 : 1;
}
