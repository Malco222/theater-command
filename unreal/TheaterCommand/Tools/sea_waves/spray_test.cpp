// The release 2 ocean plan, steps 6(b) and 12(b): bow spray (Battle/FleetSpray; the look bible's S6, "Spray sheets over the
// bow from force 6"). No engine: the core itself, held to the plan and to the figures its comments cite:
//   - none at Beaufort 3, whatever the ship and the sea;
//   - the thresholds by class: 4 for destroyers and boats, 5 for cruisers and merchants, 6 for battleships and carriers;
//   - green water and spindrift only from Beaufort 7, and only under the gale (-TheaterSeaGale), the wind otherwise held to
//     SeaShow's 13.8 m/s, and winds to the gale's 24.4 m/s taken;
//   - at most 80 puffs alive of the kit's 3,000, and no sheet thrown by halves;
//   - only the 4 ships nearest the eye, nearest first;
//   - head and bow seas throw the most, following seas little;
//   - a sheet stands to a destroyer's bridge in a Beaufort 6 head sea, fans 30 to 60 degrees each side, is blown aft and
//     down the wind, and falls within 1 to 3 s;
//   - the 1.5 s rest a ship, one sheet a plunge;
//   - the same spray every play, whatever is asked first, and nothing while a picture run skips ahead;
//   - the cost of a frame.
// Then every recorded action, stand-in and real night played through the show itself (the game's own SeaShow, as the sea
// show's cloud test builds it), with the show's own table ride at each stem over a wind sweep: the caps hold, the nearest
// four are the nearest four, and a frame's cost is measured on the battles' own ships.
// Usage: spray_test [recording.json ...]   (built and run by Tools/sea_waves/run_storm.sh)

#include "Battle/FleetSpray.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Got)
	{
		++Checks;
		Failed += bOk ? 0 : 1;
		std::printf("  %s  %s: %s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.c_str());
	}

	void Note(const std::string& What, const std::string& Got) { std::printf("  note  %s: %s\n", What.c_str(), Got.c_str()); }

	std::string Num(double X, int Places = 2)
	{
		char Text[80];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	constexpr double Pi = FleetSpray::Pi;
	/** The wind of a Beaufort number (WMO's scale, its middle: U = 0.836 B^(3/2)). */
	double WindOf(double Force) { return 0.836 * std::pow(Force, 1.5); }

	/**
	 * A ship steaming in a seaway, her stem driven as the scene drives it: the show's own table ride (her pitch period and
	 * her row's angle at the Beaufort, SeaShow's RideOf and AtForce) with the drawn water at her stem where a swell is
	 * drawn. Off is the sea's bearing off her bow; the wind blows from her heading plus it.
	 */
	struct Ride
	{
		FleetSpray::Hull H;
		double PitchT = 5.5, PitchDeg = 2.5, HeaveM = 0.3, Phase = 0.0, WaveA = 0.0, WaveT = 7.0;
	};

	Ride RideOf(const std::string& Class, double Long, double Beam, double Force, double Off, double Speed = 7.5, int Key = 1)
	{
		Ride R;
		R.H.Key = Key;
		R.H.Class = Class;
		R.H.Long = Long;
		R.H.Beam = Beam;
		R.H.Speed = Speed;
		R.H.Heading = 0.0;
		R.H.OffBow = Off;
		// (the show's table, SeaShow's RideOf: a destroyer's pitch period 5.5 s and 4.0 degrees at Beaufort 6, a cruiser's
		// 7.5 s and 2.0, a battleship's 10 s and 1.25; her heave a share of the waves' height, 0.021 U^2)
		const double K = std::clamp((Long - 110.0) / 140.0, 0.0, 1.0);
		R.PitchT = 5.5 + (10.0 - 5.5) * K;
		const double AtSix = 4.0 + (1.25 - 4.0) * K;
		R.PitchDeg = AtSix * std::clamp((Force - 2.0) / 4.0, 0.0, 1.0);
		const double U = WindOf(Force);
		const double High = 0.021 * std::min(U, 13.8) * std::min(U, 13.8);
		R.HeaveM = (0.2 + (0.075 - 0.2) * K) * High;
		return R;
	}

	/** Her stem's ride and the water there at T: the table ride quickened by the encounter, as the scene shapes the input
	 * (the table's period has no heading in it). */
	void At(Ride& R, double T, double U)
	{
		const double E = FleetSpray::Encounter(U, R.H.Speed, R.H.OffBow);
		const double W = 2.0 * Pi / R.PitchT * E;
		R.H.Pitch = R.PitchDeg * std::sin(W * T + R.Phase);
		R.H.Heave = R.HeaveM * std::sin(W * T + R.Phase - 0.6);
		R.H.WaterAtStem = R.WaveA > 0.0 ? R.WaveA * std::sin(2.0 * Pi / R.WaveT * T * E + 1.1) : 0.0;
	}

	struct Played
	{
		int Sheets = 0, Green = 0, Spindrift = 0, Calls = 0, MostLive = 0, Dropped = 0, Partial = 0;
		double MostTop = 0.0, LeastFan = 1.0e9, MostFan = 0.0, LeastLife = 1.0e9, MostLife = 0.0, LeastGap = 1.0e9;
		double Aft = 1.0, Downwind = -1.0; // the worst of each: the sheet's drift against her way, and against the wind
	};

	/** One ship in a seaway for Seconds at 30 frames a second, the eye half a kilometre off her. */
	Played Sweep(const std::string& Class, double Long, double Beam, double Force, double Off, bool bGale, double Seconds = 120.0, double Speed = 7.5, double WaveA = 0.0,
		bool bRow = false)
	{
		FleetSpray::Settings Set;
		Set.bGale = bGale;
		Set.bSprayRow = bRow;
		Set.Seed = 239;
		FleetSpray::Spray Spray(Set);
		Ride R = RideOf(Class, Long, Beam, Force, Off, Speed);
		R.WaveA = WaveA;
		const double U = WindOf(Force);
		FleetSpray::Wind W;
		W.U = U;
		W.From = R.H.Heading + Off;
		W.bGale = bGale;
		Played Out;
		std::vector<FleetSpray::Hull> Ships;
		std::vector<FleetSpray::Call> Calls;
		double Last = -1.0e9;
		const double Ax = std::sin(R.H.Heading), Ay = std::cos(R.H.Heading);
		const double Tx = -std::sin(W.From), Ty = -std::cos(W.From);
		for (int Fr = 0; Fr * (1.0 / 30.0) <= Seconds; ++Fr)
		{
			const double T = Fr / 30.0;
			At(R, T, FleetSpray::WindFor(U, bGale));
			Ships.assign(1, R.H);
			Spray.Step(T, Ships, R.H.X, R.H.Y - 500.0, W, false, Calls);
			bool bSheet = false;
			for (const FleetSpray::Call& C : Calls)
			{
				++Out.Calls;
				Out.MostTop = std::max(Out.MostTop, C.Top);
				Out.LeastFan = std::min(Out.LeastFan, C.Fan);
				Out.MostFan = std::max(Out.MostFan, C.Fan);
				Out.LeastLife = std::min(Out.LeastLife, C.Life);
				Out.MostLife = std::max(Out.MostLife, C.Life);
				Out.Green += C.Is == FleetSpray::What::Green ? 1 : 0;
				Out.Spindrift += C.Is == FleetSpray::What::Spindrift ? 1 : 0;
				bSheet = bSheet || C.Is == FleetSpray::What::Sheet;
				const double Len = std::hypot(C.DriftX, C.DriftY);
				if (Len > 1e-6)
				{
					// (blown aft: the drift against her way; and down the wind: against where the wind blows toward)
					Out.Aft = std::min(Out.Aft, (C.DriftX * Ax + C.DriftY * Ay) / Len);
					Out.Downwind = std::max(Out.Downwind, (C.DriftX * Tx + C.DriftY * Ty) / Len);
				}
			}
			if (!Calls.empty())
			{
				Out.LeastGap = std::min(Out.LeastGap, T - Last);
				Last = T;
				++Out.Sheets;
				Out.Partial += bSheet ? 0 : 1;
			}
			Out.MostLive = std::max(Out.MostLive, Spray.Live(T));
		}
		Out.Dropped = Spray.Counted().Dropped;
		return Out;
	}

	/** A line of ships abreast at set distances from the eye, all alike in a Beaufort 6 head sea. */
	struct Line
	{
		std::vector<FleetSpray::Hull> Ships;
		std::vector<Ride> Rides;
		FleetSpray::Wind W;
	};

	Line LineOf(int N, double Force, bool bGale)
	{
		Line L;
		const double U = WindOf(Force);
		L.W.U = U;
		L.W.From = 0.0;
		L.W.bGale = bGale;
		for (int I = 0; I < N; ++I)
		{
			Ride R = RideOf("dd", 110.0, 11.0, Force, 0.0, 7.5, 10 + I);
			R.Phase = 0.37 * I; // (each on her own part of the swell, as the show gives each her own Roll0)
			R.H.X = 0.0;
			R.H.Y = 300.0 * (I + 1); // (the eye at the origin: she is the (I+1)th nearest)
			L.Rides.push_back(R);
		}
		L.Ships.resize(static_cast<size_t>(N));
		return L;
	}
}

int main(int ArgC, char** ArgV)
{
	std::printf("Bow spray (Battle/FleetSpray; the release 2 ocean plan, steps 6(b) and 12(b))\n");

	// ----------------------------------------------------------------------------------------------------------------
	// The wind, the Beaufort scale and the gale's cap.
	{
		Check("the wind held to SeaShow's 13.8 m/s without the gale, and to the gale's 24.4 m/s with it (-TheaterSeaGale, step 14(c))",
			FleetSpray::WindFor(30.0, false) == 13.8 && FleetSpray::WindFor(30.0, true) == 24.4 && FleetSpray::WindFor(9.0, false) == 9.0,
			Num(FleetSpray::WindFor(30.0, false)) + " / " + Num(FleetSpray::WindFor(30.0, true)) + " m/s");
		const bool bEdges = FleetSpray::BeaufortOf(5.4, true) == 3 && FleetSpray::BeaufortOf(5.6, true) == 4 && FleetSpray::BeaufortOf(11.0, true) == 6
			&& FleetSpray::BeaufortOf(14.0, true) == 7 && FleetSpray::BeaufortOf(21.0, true) == 9;
		Check("the Beaufort number of a wind, WMO's scale (force 4 from 5.5 m/s, 6 from 10.8, 7 from 13.9, 9 from 20.8)", bEdges,
			std::to_string(FleetSpray::BeaufortOf(5.6, true)) + ", " + std::to_string(FleetSpray::BeaufortOf(14.0, true)) + ", " + std::to_string(FleetSpray::BeaufortOf(21.0, true)));
		Check("without the gale no wind reads over Beaufort 6, whatever is given", FleetSpray::BeaufortOf(24.4, false) == 6 && FleetSpray::BeaufortOf(18.0, false) == 6,
			std::to_string(FleetSpray::BeaufortOf(24.4, false)));
		// [P] Ochi 1964: 12 ft/s (3.66 m/s) for a 520 ft (158 m) ship, so 0.093 sqrt(g L)
		const double Ochi = FleetSpray::OchiSlam * std::sqrt(FleetSpray::Gee * 158.5);
		Check("Ochi's slamming threshold holds his own figure (12 ft/s for a 520 ft ship)", std::fabs(Ochi - 3.66) < 0.1, Num(Ochi) + " m/s");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Nothing at Beaufort 3, and the thresholds by class.
	{
		bool bQuiet = true;
		std::string Said;
		for (const char* Class : { "dd", "ss", "ca", "mv", "bb", "cv" })
		{
			for (const double Force : { 1.0, 2.0, 3.0 })
			{
				const Played P = Sweep(Class, std::string(Class) == "ss" ? 67.0 : 110.0, 11.0, Force, 0.0, false);
				bQuiet = bQuiet && P.Sheets == 0;
				Said += P.Sheets ? std::string(Class) + " at B" + Num(Force, 0) + ": " + std::to_string(P.Sheets) + " " : "";
			}
		}
		Check("none at Beaufort 3 and under, for every class, head seas and all", bQuiet, bQuiet ? "no sheets, 18 sweeps" : Said);

		struct Want
		{
			const char* Class;
			double Long, Beam;
			int From;
		};
		const Want Wants[] = { { "dd", 110.0, 11.0, 4 }, { "ss", 67.0, 6.2, 4 }, { "ca", 180.0, 18.0, 5 }, { "mv", 135.0, 17.5, 5 },
			{ "bb", 250.0, 32.0, 6 }, { "cv", 250.0, 30.0, 6 } };
		bool bByClass = true;
		std::string Rows;
		for (const Want& W : Wants)
		{
			int First = 0;
			for (int Force = 4; Force <= 6 && First == 0; ++Force)
			{
				First = Sweep(W.Class, W.Long, W.Beam, Force, 0.0, false).Sheets > 0 ? Force : 0;
			}
			bByClass = bByClass && First == W.From;
			Rows += std::string(W.Class) + " from B" + std::to_string(First) + " (wanted " + std::to_string(W.From) + "); ";
		}
		Check("the plan's thresholds by class: Beaufort 4 for destroyers and boats, 5 for cruisers and merchants, 6 for battleships and carriers", bByClass, Rows);
		// (and a hull of no known class by her length: a destroyer's row up to 125 m, a cruiser's to 210 m, a battleship's past it)
		const FleetSpray::Bow B1 = FleetSpray::BowOf("xx", 100.0), B2 = FleetSpray::BowOf("xx", 180.0), B3 = FleetSpray::BowOf("xx", 250.0);
		Check("a class the battle does not name takes her row by her length", B1.SheetFrom == 4 && B2.SheetFrom == 5 && B3.SheetFrom == 6,
			std::to_string(B1.SheetFrom) + ", " + std::to_string(B2.SheetFrom) + ", " + std::to_string(B3.SheetFrom));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Green water and spindrift: only from Beaufort 7, and only under the gale.
	{
		bool bNone = true;
		std::string Said;
		for (int Force = 4; Force <= 9; ++Force)
		{
			const Played Held = Sweep("dd", 110.0, 11.0, Force, 0.0, false);
			bNone = bNone && Held.Green == 0 && Held.Spindrift == 0;
			Said += Held.Green || Held.Spindrift ? "B" + std::to_string(Force) + " " : "";
		}
		Check("no green water and no spindrift without the gale, at any wind asked (the wind is held to Beaufort 6 there)", bNone,
			bNone ? "none from Beaufort 4 to 9 asked" : Said);
		const Played Six = Sweep("dd", 110.0, 11.0, 6, 0.0, true);
		const Played Seven = Sweep("dd", 110.0, 11.0, 7, 0.0, true);
		const Played Nine = Sweep("dd", 110.0, 11.0, 9, 0.0, true);
		Check("under the gale: none at Beaufort 6, green seas and spindrift from 7, and more of both at 9",
			Six.Green == 0 && Six.Spindrift == 0 && Seven.Green > 0 && Seven.Spindrift > 0 && Nine.Spindrift >= Seven.Spindrift && Nine.Sheets > 0,
			"B6 " + std::to_string(Six.Green) + "/" + std::to_string(Six.Spindrift) + ", B7 " + std::to_string(Seven.Green) + "/" + std::to_string(Seven.Spindrift)
				+ ", B9 " + std::to_string(Nine.Green) + "/" + std::to_string(Nine.Spindrift) + " (green/spindrift)");
		Check("the gale's own winds are taken to 24.4 m/s, and a sheet is still thrown there", Nine.Sheets > 0 && Nine.MostTop > 0.0,
			std::to_string(Nine.Sheets) + " sheets, highest " + Num(Nine.MostTop) + " m");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The sheet itself: her bridge in a Beaufort 6 head sea, its fan, its fall, blown aft and down the wind.
	{
		const Played P = Sweep("dd", 110.0, 11.0, 6, 0.0, false);
		// (the destroyer's bridge is 11 m over her waterline: a sheet reaching it, and never far over it)
		Check("a sheet stands to a destroyer's bridge in a Beaufort 6 head sea (8 to 14 m: her bridge 11 m, the cap 14 m)",
			P.MostTop >= 8.0 && P.MostTop <= 14.0, Num(P.MostTop) + " m");
		Check("it fans 30 to 60 degrees each side of her stem", P.LeastFan >= 30.0 && P.MostFan <= 60.0, Num(P.LeastFan) + " to " + Num(P.MostFan) + " degrees");
		Check("it rises and falls within 1 to 3 s", P.LeastLife >= 1.0 && P.MostLife <= 3.0, Num(P.LeastLife) + " to " + Num(P.MostLife) + " s");
		Check("it is blown aft and down the wind (the wind across her deck, her own way taken out)", P.Aft < -0.5 && P.Downwind > 0.9,
			"aft " + Num(P.Aft) + ", downwind " + Num(P.Downwind));
		Check("one sheet a plunge, and 1.5 s of rest between sheets", P.LeastGap >= 1.5 - 1e-9 && P.Sheets > 3, Num(P.LeastGap) + " s least gap, " + std::to_string(P.Sheets) + " sheets");
		Check("no sheet thrown by halves (every batch carries its sheet; the green sea and the spindrift go with it)", P.Partial == 0, std::to_string(P.Partial) + " partial");
		// (and the same sheet through the naval spray row the effects thread may add: one call, the sheet whole)
		const Played Row = Sweep("dd", 110.0, 11.0, 6, 0.0, false, 120.0, 7.5, 0.0, true);
		Check("through the naval spray row (the requires-probe): the same sheets, one call each, the same height and fan",
			Row.Sheets == P.Sheets && Row.Calls == Row.Sheets && std::fabs(Row.MostTop - P.MostTop) < 1e-9,
			std::to_string(Row.Sheets) + " sheets, " + std::to_string(Row.Calls) + " calls");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Head and bow seas throw the most, following seas little.
	{
		const Played Head = Sweep("dd", 110.0, 11.0, 6, 0.0, false);
		const Played Bow = Sweep("dd", 110.0, 11.0, 6, 45.0 * Pi / 180.0, false);
		const Played Beam = Sweep("dd", 110.0, 11.0, 6, 90.0 * Pi / 180.0, false);
		const Played Follow = Sweep("dd", 110.0, 11.0, 6, Pi, false);
		const bool bOrder = Head.Sheets >= Bow.Sheets && Bow.Sheets >= Beam.Sheets && Beam.Sheets >= Follow.Sheets && Head.Sheets > 2 * std::max(1, Follow.Sheets);
		Check("head seas throw the most, then bow, then beam, and a following sea little (fewer than half the head sea's)", bOrder,
			"head " + std::to_string(Head.Sheets) + ", bow " + std::to_string(Bow.Sheets) + ", beam " + std::to_string(Beam.Sheets) + ", following " + std::to_string(Follow.Sheets));
		Check("and the head sea's sheets stand higher than the following sea's", Head.MostTop > Follow.MostTop,
			Num(Head.MostTop) + " m against " + Num(Follow.MostTop) + " m");
		// [P] the encounter: 1 + w V cos(off bow) / g, quicker head on, slower before the sea
		const double Ahead = FleetSpray::Encounter(12.0, 8.0, 0.0), Astern = FleetSpray::Encounter(12.0, 8.0, Pi);
		Check("the encounter quickens the sea head on and slows it before it (Principles of Naval Architecture III)", Ahead > 1.3 && Astern < 0.7 && Astern > 0.0,
			Num(Ahead) + " head, " + Num(Astern) + " following");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Only the 4 ships nearest the eye, and at most 80 puffs alive.
	{
		FleetSpray::Settings Set;
		Set.Seed = 239;
		FleetSpray::Spray Spray(Set);
		Line L = LineOf(9, 6, false);
		std::vector<FleetSpray::Call> Calls;
		std::set<int> Threw;
		int MostLive = 0, Partial = 0, Batches = 0;
		bool bNearest = true;
		for (int Fr = 0; Fr < 30 * 120; ++Fr)
		{
			const double T = Fr / 30.0;
			for (size_t I = 0; I < L.Rides.size(); ++I)
			{
				At(L.Rides[I], T, FleetSpray::WindFor(L.W.U, false));
				L.Ships[I] = L.Rides[I].H;
			}
			Spray.Step(T, L.Ships, 0.0, 0.0, L.W, false, Calls);
			const std::vector<int>& Near = Spray.Throwing();
			bNearest = bNearest && Near.size() == 4 && Near[0] == 10 && Near[1] == 11 && Near[2] == 12 && Near[3] == 13;
			bool bSheet = Calls.empty();
			for (const FleetSpray::Call& C : Calls)
			{
				Threw.insert(C.Key);
				bSheet = bSheet || C.Is == FleetSpray::What::Sheet;
			}
			Batches += Calls.empty() ? 0 : 1;
			Partial += bSheet ? 0 : 1;
			MostLive = std::max(MostLive, Spray.Live(T));
		}
		Check("of 9 ships, only those among the 4 nearest the eye throw, and the nearest four are named nearest first", bNearest && Threw.size() <= 4 && *Threw.rbegin() <= 13,
			std::to_string(Threw.size()) + " threw (the room allows no more), the farthest key " + std::to_string(*Threw.rbegin()) + " of 10 to 18");
		Check("at most 80 puffs alive of the effects kit's 3,000", MostLive <= FleetSpray::MostLive, std::to_string(MostLive) + " at the most");
		Check("sheets left unthrown for want of room, never thrown by halves", Spray.Counted().Dropped > 0 && Partial == 0,
			std::to_string(Spray.Counted().Dropped) + " left, " + std::to_string(Batches) + " thrown, " + std::to_string(Partial) + " partial");
		Note("the battle's line", Spray.Line());
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The same spray every play, whatever is asked first; nothing while a picture run skips ahead.
	{
		auto Play = [](bool bBackwards, bool bSkip)
		{
			FleetSpray::Settings Set;
			Set.Seed = 1944;
			FleetSpray::Spray Spray(Set);
			Line L = LineOf(6, 6, false);
			std::vector<FleetSpray::Call> Calls;
			std::string Text;
			for (int Fr = 0; Fr < 30 * 60; ++Fr)
			{
				const double T = Fr / 30.0;
				for (size_t I = 0; I < L.Rides.size(); ++I)
				{
					At(L.Rides[I], T, FleetSpray::WindFor(L.W.U, false));
					L.Ships[I] = L.Rides[I].H;
				}
				if (bBackwards)
				{
					std::reverse(L.Ships.begin(), L.Ships.end());
				}
				Spray.Step(T, L.Ships, 0.0, 0.0, L.W, bSkip, Calls);
				for (const FleetSpray::Call& C : Calls)
				{
					char Buf[160];
					std::snprintf(Buf, sizeof Buf, "%.3f %d %d %.4f %.4f %.4f %.3f %.3f %.3f|", C.T, C.Key, static_cast<int>(C.Is), C.X, C.Y, C.Z, C.Top, C.Fan, C.Life);
					Text += Buf;
				}
			}
			return Text;
		};
		const std::string One = Play(false, false), Two = Play(false, false), Back = Play(true, false), Skipped = Play(false, true);
		Check("the same spray every play", One == Two && !One.empty(), std::to_string(One.size()) + " characters of calls");
		Check("and whatever order the ships are asked in", Back == One, Back == One ? "the same" : "differs");
		Check("nothing thrown while a picture run skips ahead", Skipped.empty(), Skipped.empty() ? "no calls" : "calls made");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// A frame's cost.
	{
		FleetSpray::Settings Set;
		Set.Seed = 239;
		FleetSpray::Spray Spray(Set);
		Line L = LineOf(40, 6, false);
		std::vector<FleetSpray::Call> Calls;
		const int Frames = 30 * 300;
		for (int Fr = 0; Fr < 300; ++Fr)
		{
			const double T = Fr / 30.0;
			for (size_t I = 0; I < L.Rides.size(); ++I)
			{
				At(L.Rides[I], T, FleetSpray::WindFor(L.W.U, false));
				L.Ships[I] = L.Rides[I].H;
			}
			Spray.Step(T, L.Ships, 0.0, 0.0, L.W, false, Calls);
		}
		const auto Began = std::chrono::steady_clock::now();
		for (int Fr = 0; Fr < Frames; ++Fr)
		{
			const double T = 10.0 + Fr / 30.0;
			for (size_t I = 0; I < L.Rides.size(); ++I)
			{
				At(L.Rides[I], T, FleetSpray::WindFor(L.W.U, false));
				L.Ships[I] = L.Rides[I].H;
			}
			Spray.Step(T, L.Ships, 0.0, 0.0, L.W, false, Calls);
		}
		const double Each = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count() / Frames;
		Check("a frame of spray for 40 ships costs little (0.02 ms or less on the cloud Xeon)", Each <= 0.02, Num(Each, 4) + " ms a frame");
	}

	// ----------------------------------------------------------------------------------------------------------------
	// The battles themselves: every recording, stand-in and real night through the show, over a wind sweep.
	for (int K = 1; K < ArgC; ++K)
	{
		const std::string Path = ArgV[K];
		SeaFeed::Recording Rec;
		std::string Error;
		const bool bRead = SeaFeed::ReadRecording(ReadAll(Path), Rec, Error) && !Rec.Views.empty();
		std::printf("%s\n", Path.substr(Path.find_last_of('/') + 1).c_str());
		Check("the recording read", bRead, bRead ? std::to_string(Rec.Views.size()) + " views" : Error);
		if (!bRead)
		{
			continue;
		}
		// (its own wind, and a gale over the same action: the sweep the plan asks for)
		struct Run
		{
			double Wind;
			bool bGale;
			const char* Word;
		};
		const Run Runs[] = { { Rec.Board.WindSpeed > 0.0 ? Rec.Board.WindSpeed : 7.0, false, "her own wind" }, { 12.3, false, "Beaufort 6" }, { 17.2, true, "Beaufort 8, the gale" } };
		for (const Run& R : Runs)
		{
			SeaShow::Field Board = Rec.Board;
			Board.WindSpeed = R.Wind;
			SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239);
			Show.SetField(Board);
			Show.SetMine(SeaFeed::SideOf(Rec.Views.front().Head.Me));
			FleetSpray::Settings Set;
			Set.bGale = R.bGale;
			Set.Seed = Rec.Seed ? Rec.Seed : 239;
			FleetSpray::Spray Spray(Set);
			std::vector<FleetSpray::Hull> Hulls;
			std::vector<FleetSpray::Call> Calls;
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& F = Rec.Views[Taken++];
				Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
			};
			TakeNext();
			const double BeginT = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
			// (a window of the action, so a night of hours is played in a minute of the cloud's time: 300 s at 30 frames a second)
			const double Span = std::min(300.0, EndT - BeginT);
			const int Frames = static_cast<int>(Span * 30.0) + 1;
			int MostLive = 0, Partial = 0, Throwing = 0;
			bool bNearest = true;
			double Took = 0.0;
			for (int Fr = 0; Fr < Frames; ++Fr)
			{
				const double T = BeginT + Fr / 30.0;
				while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9)
				{
					TakeNext();
				}
				Show.Advance(T);
				// (the eye where the scene puts it for the fleet shots: two kilometres short of the ships' middle)
				double Mx = 0.0, My = 0.0;
				int N = 0;
				Hulls.clear();
				for (const SeaShow::ShipOut& S : Show.Ships())
				{
					FleetSpray::Hull& H = Hulls.emplace_back();
					H.Key = S.Key;
					H.Class = S.Class;
					H.Long = S.Long;
					H.Beam = S.Beam;
					H.X = S.At.X;
					H.Y = S.At.Y;
					H.Heading = S.Heading * Pi / 180.0;
					H.Speed = S.Speed;
					H.Heave = S.Heave - S.Dived;
					H.Pitch = S.Pitch;
					H.bThrows = S.bShown && (S.Doing == SeaShow::Fate::Line || S.Doing == SeaShow::Fate::Limping) && (S.Class != "ss" || S.Dive.empty() || S.Dive == "surfaced");
					Mx += S.At.X;
					My += S.At.Y;
					++N;
				}
				Mx = N ? Mx / N : 0.0;
				My = N ? My / N : 0.0;
				FleetSpray::Wind W;
				W.U = Show.WindSpeed();
				W.From = Show.WindFrom();
				W.bGale = R.bGale;
				const auto Stepping = std::chrono::steady_clock::now();
				Spray.Step(T, Hulls, Mx, My - 2000.0, W, false, Calls);
				Took += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Stepping).count();
				// (the nearest four, worked out again here from the ships themselves)
				std::vector<std::pair<double, int>> By;
				for (const FleetSpray::Hull& H : Hulls)
				{
					if (H.bThrows)
					{
						By.emplace_back(std::hypot(H.X - Mx, H.Y - (My - 2000.0)), H.Key);
					}
				}
				std::sort(By.begin(), By.end());
				const std::vector<int>& Near = Spray.Throwing();
				for (size_t I = 0; I < Near.size(); ++I)
				{
					bNearest = bNearest && I < By.size() && Near[I] == By[I].second;
				}
				bNearest = bNearest && Near.size() == std::min<size_t>(FleetSpray::Nearest, By.size());
				Throwing = std::max(Throwing, static_cast<int>(Near.size()));
				bool bSheet = Calls.empty();
				std::set<int> Keys;
				for (const FleetSpray::Call& C : Calls)
				{
					bSheet = bSheet || C.Is == FleetSpray::What::Sheet;
					Keys.insert(C.Key);
					bNearest = bNearest && std::find(Near.begin(), Near.end(), C.Key) != Near.end();
				}
				Partial += bSheet ? 0 : 1;
				MostLive = std::max(MostLive, Spray.Live(T));
			}
			const FleetSpray::Count& C = Spray.Counted();
			Check(std::string("at ") + R.Word + ": only the nearest four throw, at most 80 puffs alive, no sheet by halves",
				bNearest && MostLive <= FleetSpray::MostLive && Partial == 0,
				std::to_string(C.Sheets) + " sheets (" + std::to_string(C.Green) + " green, " + std::to_string(C.Spindrift) + " spindrift, " + std::to_string(C.Dropped)
					+ " left), " + std::to_string(MostLive) + " puffs at the most, " + std::to_string(Throwing) + " ships throwing, "
					+ Num(Took / Frames, 4) + " ms a frame");
			if (!R.bGale)
			{
				Check(std::string("at ") + R.Word + ": no green water and no spindrift without the gale", C.Green == 0 && C.Spindrift == 0,
					std::to_string(C.Green) + " / " + std::to_string(C.Spindrift));
			}
		}
	}

	std::printf("%d checks, %d failed\n%s\n", Checks, Failed, Failed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return Failed ? 1 : 0;
}
