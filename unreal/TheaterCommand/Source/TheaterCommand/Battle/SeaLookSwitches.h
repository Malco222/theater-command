#pragma once

// The release 2 sea's switches and the "Sea look:" line (the release 2 ocean plan, step 0: measure first; nothing on
// screen changes). Every new piece of the fleet's sea comes behind a switch of its own, with today's look as the default,
// tier 0 being release 1.5's sea exactly; the switches land a step at a time. At a battle's end ASeaBattle::SayFrameTimes
// says one line with the tier and every switch in force, so a bench run (Tools/sea_bench.sh) is labelled by what was
// drawn, not by what was asked: a switch asked for before its step lands is said to be asked for and not built.
// The table is the plan's section 3; Tools/sea_waves/switches_scan.py holds it too and checks the two agree, that no
// name collides with another (or with one the game already reads), and that a switch is marked built exactly when the
// game reads it. Engine-free: Tools/sea_waves/measure_test.cpp builds it in the cloud.

#include <string>
#include <vector>

namespace SeaLookSwitches
{
	struct Switch
	{
		/** On the command line, after its '-': "TheaterSeaSwell=" takes a value, "TheaterSeaProbe" is a flag. */
		const char* Name;
		/** Its value at tier 0 (release 1.5's sea). */
		const char* Today;
		/** What turns it on: a tier ("1", "2", "3"), "tier" (it is the tier), "own" (independent of the tier), "with
		 * Swell" or "with Day" (with that switch), "review" (review pictures only), "extra" (one at a time) or "none". */
		const char* OnAt;
		/** The plan's step that builds it. */
		const char* Step;
		/** Read by the scene in this tree. Until its step lands it is false: asked for, it changes nothing. */
		bool bBuilt;
	};

	inline constexpr Switch All[] = {
		{ "TheaterSeaLook=", "0", "tier", "15", false },
		{ "TheaterSeaMarksPool=", "0", "own", "1", true },
		{ "TheaterSeaFarEdge=", "15", "own", "1", false },
		{ "TheaterSeaSwell=", "0", "1", "8, 9", false },
		{ "TheaterSeaSwellScale=", "1", "with Swell", "9", false },
		{ "TheaterSeaRingCells=", "96", "with Swell", "2, 8", false },
		{ "TheaterSeaPatch=", "0", "1", "8, 9", false },
		{ "TheaterSeaPixelTrains=", "12", "1", "2, 7", false },
		{ "TheaterSeaFleetCaps=", "0", "1", "7", false },
		{ "TheaterSeaHullWaves=", "0", "1", "9a", false },
		{ "TheaterSeaClockCap=", "1.5", "with Swell", "9", false },
		{ "TheaterSeaWater=", "none", "1", "3, 8", false },
		{ "TheaterSeaProbe", "off", "review", "9", false },
		{ "TheaterSeaDay=", "seed", "2", "11", true },
		{ "TheaterSeaVisContrast=", "0.05", "with Day", "3", true },
		{ "TheaterSeaSkyCapture=", "0", "2", "11", false },
		{ "TheaterSeaStars=", "0", "with Day", "11", true },
		{ "TheaterSeaWakeLane=", "0", "2", "12", false },
		{ "TheaterSeaSpray=", "0", "2", "6, 12", true },
		{ "TheaterSeaIce=", "0", "2", "6", true },
		{ "TheaterSeaNightFight=", "0", "2", "11", true },
		{ "TheaterSeaClouds=", "0", "3", "13", true },
		{ "TheaterSeaCloudShade=", "0", "3", "13", true },
		{ "TheaterSeaRain=", "0", "3", "13", true },
		{ "TheaterSeaSkyStandIn=", "0", "3", "4, 13", true },
		{ "TheaterSeaWeather=", "none", "none", "13", true },
		{ "TheaterSeaCurve", "off", "extra", "14", false },
		{ "TheaterSeaWet", "off", "extra", "14", false },
		// (the gale, step 14(c): its numbers are made and tested, FleetWaves' gale (GaleCap, GaleFetchOf, StreakShareOf,
		// PaintLanesGale), FleetRideGale.h and SeaFace::MakeStreaks, but the game does not draw the fleet's sea yet, so
		// nothing reads the switch: it stays unbuilt until the scene asks FleetWaves::Make with GaleCap under it)
		{ "TheaterSeaGale", "off", "extra", "14", false },
		{ "TheaterSeaEyeRide", "off", "extra", "14", false },
		{ "TheaterSeaIntoSun", "off", "extra", "14", false },
		{ "TheaterSeaCoast", "off", "extra", "14", false },
		{ "TheaterSeaTurrets=", "0", "own", "16", false },
		{ "TheaterSeaStepDown=", "0", "1", "15", false },
	};
	inline constexpr int Count = static_cast<int>(sizeof(All) / sizeof(All[0]));

	/** A switch found on the command line: its place in All, and its value (empty for a flag). */
	struct Asked
	{
		int Index = -1;
		std::string Value;
	};

	inline bool TakesValue(const Switch& One)
	{
		const std::string Name = One.Name;
		return !Name.empty() && Name.back() == '=';
	}

	/** The sea's look as the scene applied it, read once as it opens (ASeaBattle::OpenOn) and said at its end by the "Sea
	 * look:" line from this, not from the command line again: the tier in force (0 until step 15 builds the tier, and
	 * after it what the scene took, from the command line, the saved choice or the frame guard's step down) and every
	 * switch of the table asked for, with its value as the scene took it. Each step that builds a switch writes its value
	 * in force here. */
	struct Look
	{
		int Tier = 0;
		std::vector<Asked> Given;
	};

	/** The switches of a table asked for, in the table's order: AskedFor(One, Value) says whether the switch One was asked
	 * for and, for one that takes a value, gives it (the scene asks the engine's command line; the cloud test a list). */
	template <typename AskFn>
	Look ReadOf(const Switch* Table, int Rows, AskFn&& AskedFor)
	{
		Look Out;
		for (int K = 0; K < Rows; ++K)
		{
			Asked One;
			if (AskedFor(Table[K], One.Value))
			{
				One.Index = K;
				One.Value = TakesValue(Table[K]) ? One.Value : std::string();
				Out.Given.push_back(One);
			}
		}
		return Out;
	}

	/** A built switch of 0 or 1 as the scene takes it (step 1(a)'s -TheaterSeaMarksPool=): on only when asked for as 1. Its
	 * value in force is written back into the look (1, or 0 for anything else asked), so the "Sea look:" line says what was
	 * applied, not what was typed. */
	inline bool TakeOnOff(Look& InForce, const char* Name)
	{
		for (Asked& One : InForce.Given)
		{
			if (One.Index >= 0 && One.Index < Count && std::string(All[One.Index].Name) == Name)
			{
				One.Value = One.Value == "1" ? "1" : "0";
				return One.Value == "1";
			}
		}
		return false;
	}

	/** The release 2 sea's switches asked for (ReadOf of All); the tier 0 until step 15 builds it. */
	template <typename AskFn>
	Look Read(AskFn&& AskedFor)
	{
		return ReadOf(All, Count, AskedFor);
	}

	/** A switch as it is written on the command line, with a value. */
	inline std::string Spelled(const Switch& One, const std::string& Value)
	{
		return TakesValue(One) ? "-" + std::string(One.Name) + Value : "-" + std::string(One.Name) + (Value.empty() ? "" : " " + Value);
	}

	/** The "Sea look:" line of a table of switches (All, or a test's own): the tier, every built switch with its value
	 * in force (asked or today's), and any asked for before its step has landed, which changes nothing. */
	inline std::string LineOf(const Switch* Table, int Rows, int Tier, const std::vector<Asked>& Given)
	{
		std::string Text = "Sea look: tier " + std::to_string(Tier) + (Tier == 0 ? " (release 1.5's sea, as made)" : "");
		int Built = 0;
		std::string InForce, NotBuilt;
		for (int K = 0; K < Rows; ++K)
		{
			const Switch& One = Table[K];
			const Asked* Mine = nullptr;
			for (const Asked& Each : Given)
			{
				Mine = Each.Index == K ? &Each : Mine;
			}
			if (One.bBuilt)
			{
				++Built;
				InForce += (InForce.empty() ? "" : ", ") + (Mine ? Spelled(One, Mine->Value) + " (asked)" : Spelled(One, TakesValue(One) ? One.Today : "off"));
			}
			else if (Mine)
			{
				NotBuilt += (NotBuilt.empty() ? "" : ", ") + Spelled(One, Mine->Value);
			}
		}
		if (Built == 0)
		{
			Text += "; none of the release 2 sea's " + std::to_string(Rows) + " switches is built yet, so every one is at today's value";
		}
		else
		{
			Text += "; " + std::to_string(Built) + " of the release 2 sea's " + std::to_string(Rows) + " switches built, in force: " + InForce;
		}
		if (!NotBuilt.empty())
		{
			Text += "; asked for but not built yet, so nothing changes: " + NotBuilt;
		}
		return Text + ".";
	}

	/** The "Sea look:" line of the release 2 sea's switches. Tier is the tier in force (0 until step 15 builds it). */
	inline std::string Line(int Tier, const std::vector<Asked>& Given)
	{
		return LineOf(All, Count, Tier, Given);
	}

	/** The "Sea look:" line of the look the scene applied. */
	inline std::string Line(const Look& InForce)
	{
		return Line(InForce.Tier, InForce.Given);
	}
}
