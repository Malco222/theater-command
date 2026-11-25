// Every aircraft flies as its type did (Brian, 24 November 2026: "i want the planes flying with realistic capabilities
// and tactics"): the test of the historian's table (Content/Data/Air/performance.json) and its reader (Battle/AirPerf),
// the limits the flight core would fly each type by.
//  - the table reads, and every figure in it carries a named source and a mark (measured, manufacturer, derived,
//    uncertain), its value inside its own range where it gives one;
//  - every model the game can draw (Content/Models' aircraft), every name the scene gives a model (AirShow's ShapeOf,
//    AirBattle's ModelFor, the ground and sea scenes), every model the recordings fly, and every model doctrine.ts and
//    ships.ts name (the plain "Fighter:<nation>" and its kind for every nation) finds a row;
//  - each type's limits lie inside physical bounds (a fighter's tightest turn 150 to 450 m and its roll 40 to 200 degrees
//    a second; a bomber's roll 15 to 70), checked on the figures as worked out before Limits holds them, so that the row
//    can fail (the reviewer of 25 November: on the held figures it could not); the two the hold catches are named;
//  - the stalls: VMin from the type's weight and lift, and every quoted stall's lift that of the field it sits in;
//  - the worked-out turn agrees with the turns the table gives;
//  - the orderings the record is sure of hold, each with its source beside it;
//  - a model's plain name flies the mark of the battle's date as the record dates it, a mark named outright as named;
//  - every tactic names its sources, and a fact one beside the rules' own doctrine.ts.
// Then a line a type: the limits, for the historian's page (historian/air-performance-1124.md).
// Usage: air_perf_test <performance.json> <Content/Models> [<rules' src/battle/air>] [recording.json ...]
#include "Battle/AirFeed.h"
#include "Battle/AirPerf.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr double Deg = 3.14159265358979323846 / 180.0;

	int Check(bool bOk, const char* What, const std::string& Got)
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Got.empty() ? "" : ": ", Got.c_str());
		return bOk ? 0 : 1;
	}

	std::string Slurp(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	}

	std::string Num(double V, int Places = 0)
	{
		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "%.*f", Places, V);
		return Buf;
	}

	/** The quoted model names in a stretch of the rules' TypeScript: 'Spitfire', 'B5N'; and a plain model's kind from
	 * its template (`Fighter:${nat}` gives "Fighter:"). Nations' tags and lower-case words are not models. A model's name
	 * is one word of letters, digits, '.', ':' and '-'; anything else between two quotes is not a name, and the second
	 * quote is then tried as an opening one, so an apostrophe in a comment ("the bomber's") cannot swallow a name. */
	void QuotedModels(const std::string& Text, std::set<std::string>& Out)
	{
		static const std::set<std::string> Nations = { "GER", "UK", "CAN", "AUS", "NZL", "SAF", "IND", "USA", "SOV", "FRA", "ITA", "JAP", "JPN" };
		for (size_t At = Text.find('\''); At != std::string::npos; At = Text.find('\'', At + 1))
		{
			const size_t End = Text.find('\'', At + 1);
			if (End == std::string::npos) break;
			const std::string Word = Text.substr(At + 1, End - At - 1);
			bool bWord = !Word.empty() && Word.size() <= 16;
			for (const char C : Word)
				bWord = bWord && (std::isalnum(static_cast<unsigned char>(C)) || C == '.' || C == ':' || C == '-');
			if (!bWord)
			{
				At = End - 1;
				continue;
			}
			At = End;
			if (!std::isupper(static_cast<unsigned char>(Word[0])) || Nations.count(Word)) continue;
			Out.insert(Word);
		}
		for (const char* Kind : { "Fighter", "Bomber", "Torpedo", "Dive" })
		{
			if (Text.find(std::string("`") + Kind + ":${") != std::string::npos) Out.insert(std::string(Kind) + ":");
		}
	}

	/** Every model name a group flies in a recording (its model and nation); false (and why) if it does not read. */
	bool RecordedModels(const std::string& Path, std::set<std::pair<std::string, std::string>>& Out, std::string& Why)
	{
		AirFeed::Recording Rec;
		std::string Fault;
		if (!AirFeed::ReadRecording(Slurp(Path), Rec, Fault))
		{
			Why += (Why.empty() ? "" : "; ") + Path + " does not read: " + Fault;
			return false;
		}
		for (const AirFeed::Frame& F : Rec.Views)
			for (const AirShow::Group& G : F.Groups)
				if (!G.Model.empty()) Out.insert({ G.Model, G.Nation });
		return true;
	}

	/** The lift a quoted stall implies at the row's weight: CL = 2 m g / (1.225 S V^2). */
	double StallLift(double MassKg, double WingM2, double StallKmh)
	{
		const double V = StallKmh / 3.6;
		return WingM2 > 0.0 && V > 0.0 ? 2.0 * MassKg * 9.81 / (1.225 * WingM2 * V * V) : 0.0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: air_perf_test <performance.json> <Content/Models> [<rules' src/battle/air>] [recording.json ...]\n");
		return 2;
	}
	int Bad = 0;
	const std::string Text = Slurp(Argv[1]);
	AirPerf::Table T;
	std::string Why;
	const bool bRead = AirPerf::Read(Text, T, Why);
	Bad += Check(bRead && T.Rows.size() >= 40, "the table reads, and has a row for every type", bRead ? std::to_string(T.Rows.size()) + " rows" : Why);
	if (!bRead) return 1;

	// Every figure: [value, source, mark] or [value, source, mark, low, high], its source one the file names, its mark one
	// of the four, its value inside its range.
	{
		AirFeed::Json J;
		std::string ParseWhy;
		AirFeed::Parse(Text, J, ParseWhy);
		const AirFeed::Json& Sources = J["sources"];
		static const std::set<std::string> Marks = { "measured", "manufacturer", "derived", "uncertain" };
		static const char* Figures[] = { "span_m", "wing_m2", "mass_kg", "power_hp", "power_m", "power_low_hp", "power_low_m", "thrust_kn", "top_kmh", "top_m", "cruise_kmh", "stall_kmh", "stall_flaps_kmh",
			"climb_ms", "to_height_min", "ceiling_m", "turn_m", "turn_s", "roll_dps", "roll_fast_dps", "g_limit", "dive_kmh", "bomb_kg", "range_km", "clmax" };
		int Count = 0, Wrong = 0;
		std::string First;
		std::map<std::string, int> ByMark;
		const AirFeed::Json& Rows = J["rows"];
		for (size_t I = 0; I < Rows.Count(); ++I)
		{
			for (const char* Name : Figures)
			{
				const AirFeed::Json& F = Rows.At(I)[Name];
				if (F.IsNull()) continue;
				++Count;
				const bool bShape = F.IsArray() && (F.Count() == 3 || F.Count() == 5) && F.At(0).IsNumber() && F.At(1).IsString() && F.At(2).IsString();
				const bool bSource = bShape && !Sources[F.At(1).Str()].IsNull();
				const bool bMark = bShape && Marks.count(F.At(2).Str());
				const bool bRange = !bShape || F.Count() == 3 || (F.At(3).Num() <= F.At(0).Num() + 1e-9 && F.At(0).Num() <= F.At(4).Num() + 1e-9);
				if (bMark) ++ByMark[F.At(2).Str()];
				if (!(bShape && bSource && bMark && bRange))
				{
					++Wrong;
					if (First.empty()) First = Rows.At(I)["key"].Str("") + "'s " + Name;
				}
			}
		}
		std::string Tally;
		for (const auto& [Mark, N] : ByMark) Tally += " " + Mark + " " + std::to_string(N);
		Bad += Check(Wrong == 0 && Count > 0, "every figure names its source and how sure it is, inside its own range",
			std::to_string(Count) + " figures:" + Tally + (First.empty() ? "" : "; first wrong: " + First));

		// The stalls (the reviewer of 25 November: the quoted stalls were on mixed bases, their lift 1.26 to 2.45). A
		// clean stall as the pilot's notes give it reads low near the stall (a lift of 1.2 to 1.8 at the row's weight); one
		// whose lift is more is a flaps-down or landing stall and sits in stall_flaps_kmh; neither is flown.
		std::string Stalls;
		int Clean = 0, Flaps = 0;
		for (size_t I = 0; I < Rows.Count(); ++I)
		{
			const AirFeed::Json& X = Rows.At(I);
			const double Mass = X["mass_kg"].At(0).Num(), Wing = X["wing_m2"].At(0).Num();
			if (!X["stall_kmh"].IsNull())
			{
				++Clean;
				const double Cl = StallLift(Mass, Wing, X["stall_kmh"].At(0).Num());
				if (Cl < 1.2 || Cl > 1.8) Stalls += " " + X["key"].Str("") + " clean " + Num(Cl, 2);
			}
			if (!X["stall_flaps_kmh"].IsNull())
			{
				++Flaps;
				const double Cl = StallLift(Mass, Wing, X["stall_flaps_kmh"].At(0).Num());
				if (Cl <= 1.8 || Cl > 3.5) Stalls += " " + X["key"].Str("") + " flaps " + Num(Cl, 2);
			}
		}
		Bad += Check(Stalls.empty() && Clean + Flaps >= 70, "every quoted stall's lift fits its field (clean 1.2 to 1.8; flaps down over 1.8)",
			std::to_string(Clean) + " clean, " + std::to_string(Flaps) + " flaps down" + (Stalls.empty() ? "" : "; off:" + Stalls));

		// The tactics: each names sources the file lists; a fact names one beside the rules' own doctrine.ts, which is
		// the game's reading, not the record (the reviewer of 25 November: doctrine.ts alone is circular).
		std::string Tactics;
		const AirFeed::Json& Tac = J["tactics"];
		for (size_t I = 0; I < Tac.Count(); ++I)
		{
			const AirFeed::Json& X = Tac.At(I);
			const std::string Kind = X["kind"].Str("");
			bool bKnown = X["by"].Count() > 0, bBeyond = false;
			for (size_t K = 0; K < X["by"].Count(); ++K)
			{
				const std::string By = X["by"].At(K).Str("");
				bKnown = bKnown && !Sources[By].IsNull();
				bBeyond = bBeyond || By != "doctrine";
			}
			if (!bKnown || (Kind != "fact" && Kind != "reading") || (Kind == "fact" && !bBeyond))
				Tactics += " " + X["topic"].Str("") + " " + X["who"].Str("") + " " + X["when"].Str("") + ";";
		}
		Bad += Check(Tactics.empty() && Tac.Count() >= 40, "every tactic names its sources, and a fact one beside doctrine.ts",
			std::to_string(Tac.Count()) + " tactics" + (Tactics.empty() ? "" : "; wrong:" + Tactics));
	}

	// Every name a model is drawn or named by finds a row.
	{
		std::set<std::string> Names;
		// (the aircraft the game can draw: Content/Models' files of kind "aircraft")
		int Files = 0;
		for (const auto& Entry : std::filesystem::directory_iterator(Argv[2]))
		{
			if (Entry.path().extension() != ".json") continue;
			const std::string Body = Slurp(Entry.path().string());
			if (Body.find("\"kind\": \"aircraft\"") == std::string::npos && Body.find("\"kind\":\"aircraft\"") == std::string::npos) continue;
			Names.insert(Entry.path().stem().string());
			++Files;
		}
		// (the scene's names: AirShow's ShapeOf, AirBattle's and SeaBattle's ModelFor, LandAir's machines, SeaShow's, the
		// engines' sounds; and the names the historian was asked to find: "Bf 109", "Bf109", "Me 109"; "B-17", "B17")
		for (const char* Name : { "B17", "Lancaster", "He111", "G4M", "Betty", "G3M", "Nell", "Wellington", "Pe2", "Pe-2", "LeO451", "LeO 451", "SM79", "SM.79",
			"B5N", "B6N", "D3A", "D4Y", "SBD", "SB2C", "Skua", "TBD", "TBF", "Swordfish", "Barracuda", "Stuka", "Zero", "Warhawk", "Spitfire", "Hurricane",
			"Mustang", "Wildcat", "Bf109", "D520", "D.520", "Kate", "Val", "Dauntless", "Devastator", "Avenger", "F4F", "A6M", "P40", "B-17", "P-40", "F6F",
			"Hellcat", "Me 262", "C.202", "MC.202", "Fw190", "Sturmovik", "Fighter", "Seafire", "Fulmar", "B24", "Yak", "Me262", "C202",
			"Bf 109", "Me 109", "Patrol:USA", "Dive:FRA", "Torpedo:ITA", "Bomber:UK", "Fighter:POL" })
			Names.insert(Name);
		// (the recordings' groups, by their models and nations)
		std::set<std::pair<std::string, std::string>> Flown;
		std::string RecWhy;
		int Recordings = 0, Unread = 0;
		std::string Rules;
		for (int A = 3; A < Argc; ++A)
		{
			const std::string Path = Argv[A];
			if (std::filesystem::is_directory(Path)) { Rules = Path; continue; }
			// (a recording that does not read is a failure of its own, not a recording with nothing in it)
			if (!RecordedModels(Path, Flown, RecWhy)) ++Unread;
			++Recordings;
		}
		Bad += Check(Unread == 0, "every recording given reads", std::to_string(Recordings) + " recordings" + (RecWhy.empty() ? "" : "; " + RecWhy));
		// (the rules': doctrine.ts's fighterOf, bomberOf and its single-engined set; ships.ts's deckModels)
		std::set<std::string> Ruled;
		if (!Rules.empty())
		{
			QuotedModels(Slurp(Rules + "/doctrine.ts"), Ruled);
			const std::string Ships = Slurp(Rules + "/ships.ts");
			const size_t From = Ships.find("export function deckModels");
			const size_t To = From == std::string::npos ? From : Ships.find("\n}", From);
			if (From != std::string::npos) QuotedModels(Ships.substr(From, To - From), Ruled);
			Bad += Check(Ruled.size() >= 30, "doctrine.ts and ships.ts read for the models they name", std::to_string(Ruled.size()) + " names");
		}
		std::string Missing;
		int Found = 0;
		for (const std::string& Name : Names)
		{
			if (T.Of(Name)) ++Found;
			else Missing += " " + Name;
		}
		for (const auto& [Model, Nation] : Flown)
		{
			if (T.Of(Model, Nation)) ++Found;
			else Missing += " " + Model + "(" + Nation + ")";
		}
		static const char* Everyone[] = { "GER", "UK", "CAN", "AUS", "NZL", "SAF", "IND", "USA", "SOV", "FRA", "ITA", "JAP", "JPN", "POL", "NED", "FIN", "ROM",
			"CHN", "HUN", "BEL", "NOR", "GRE", "YUG", "SWE", "ESP", "TUR", "THA" };
		for (const std::string& Name : Ruled)
		{
			if (Name.back() != ':')
			{
				if (T.Of(Name)) ++Found;
				else Missing += " " + Name;
				continue;
			}
			for (const char* Nat : Everyone)
				for (const double Year : { 1939.7, 1941.0, 1942.5, 1944.5 })
				{
					if (T.Of(Name + Nat, "", Year)) ++Found;
					else Missing += " " + Name + Nat;
				}
		}
		Bad += Check(Missing.empty() && Files >= 25, "every model the files, the scene, the recordings and the rules name finds a row",
			std::to_string(Files) + " model files, " + std::to_string(Recordings) + " recordings (" + std::to_string(Flown.size()) + " models flown), "
			+ std::to_string(Found) + " found" + (Missing.empty() ? std::string() : "; none for" + Missing));
	}

	// Each type's limits inside physical bounds; the worked-out turn against the turns the table gives.
	{
		std::string Out, Off, Held, Cache, Stall;
		int Turns = 0;
		// (the two the hold catches, and why: the Me 262A's jet thrust is weak at a fighting speed and its wing loading
		// high, about 495 m worked out, flown at 450 as the brief's fighter bound; the Swordfish I is a biplane, its two
		// wings' area on a short span, about 136 m worked out, flown at 150. Any other caught, or either of these let go,
		// fails the row)
		static const std::set<std::string> HeldOnes = { "Me 262A", "Swordfish I" };
		for (const AirPerf::Row& R : T.Rows)
		{
			const AirPerf::Flight F = AirPerf::Limits(R);
			const double Roll = F.RollMax / Deg;
			// (a hair's allowance, Eps, for a figure on the bound itself: 15 degrees a second comes back as 14.999999)
			constexpr double Eps = 1e-6;
			// (worked out before the hold: the turn, the roll's settling time)
			const double Worked = AirPerf::RadiusWorked(R), Settle = AirPerf::RollSettle(R, F.VMax);
			const double TurnLo = 150.0, TurnHi = R.IsFighter() ? 450.0 : 2500.0;
			bool bOk = F.VMin > 20.0 && F.VMin < F.VMax && F.VMax < F.VTop && F.VTop <= F.VDive && F.VTop < 260.0 && F.GMax >= 2.0 && F.GMax <= 6.5
				&& Settle >= 0.08 && Settle <= 1.2 && F.Ceiling >= 3000.0 && F.Ceiling <= 14000.0 && F.ClimbMost > 2.0 && F.ClimbMost < 25.0;
			if (R.IsFighter()) bOk = bOk && Roll >= 40.0 - Eps && Roll <= 200.0 + Eps;
			else bOk = bOk && Roll >= 15.0 - Eps && Roll <= 70.0 + Eps;
			const bool bInside = Worked >= TurnLo - Eps && Worked <= TurnHi + Eps;
			if (!bInside) Held += " " + R.Key + " " + Num(Worked) + " m";
			bOk = bOk && Worked > 0.0 && (bInside || HeldOnes.count(R.Key)) && (!bInside || !HeldOnes.count(R.Key));
			// (and what it flies is the worked-out turn held to the bounds, nothing else)
			bOk = bOk && std::fabs(F.RMin - std::clamp(Worked, TurnLo, TurnHi)) < 1e-9 && std::fabs(F.RollTime - std::clamp(Settle, 0.08, 1.2)) < 1e-12;
			if (!bOk) Out += " " + R.Key + " (" + Num(F.VMin) + "/" + Num(F.VMax) + "/" + Num(F.VTop) + "/" + Num(F.VDive) + " m/s, " + Num(Worked) + " m worked, " + Num(F.RMin) + " flown, " + Num(Roll) + " deg/s, "
					+ Num(Settle, 2) + " s)";
			// (VMin 1.15 times the stall from its weight and its lift, not the quoted stall)
			if (std::fabs(F.VMin - 1.15 * AirPerf::StallSpeed(R)) > 1e-9) Stall += " " + R.Key;
			// (the table's own limits, worked out once by Read, are Limits' own)
			const AirPerf::Flight Kept = T.LimitsOf(R);
			if (Kept.VMin != F.VMin || Kept.VMax != F.VMax || Kept.VTop != F.VTop || Kept.VDive != F.VDive || Kept.RMin != F.RMin || Kept.GMax != F.GMax || Kept.RollMax != F.RollMax
				|| Kept.RollTime != F.RollTime || Kept.ClimbMost != F.ClimbMost || Kept.Ceiling != F.Ceiling)
				Cache += " " + R.Key;
			// (a documented radius or turn time, within three tenths of the worked-out one at its height)
			if (R.TurnRadius > 0.0)
			{
				const double AtHeight = AirPerf::TurnRadius(R, R.TurnM > 0.0 ? R.TurnM : AirPerf::FightM);
				++Turns;
				if (std::fabs(AtHeight - R.TurnRadius) > 0.3 * R.TurnRadius) Off += " " + R.Key + " " + Num(AtHeight) + " m against " + Num(R.TurnRadius);
			}
			if (R.TurnTime > 0.0)
			{
				double Seconds = 0.0;
				AirPerf::TurnRadius(R, R.TurnM > 0.0 ? R.TurnM : AirPerf::FightM, &Seconds);
				++Turns;
				if (std::fabs(Seconds - R.TurnTime) > 0.3 * R.TurnTime) Off += " " + R.Key + " " + Num(Seconds, 1) + " s against " + Num(R.TurnTime, 1);
			}
		}
		Bad += Check(Out.empty(), "every type's limits inside physical bounds, as worked out before the hold (fighters: turn 150 to 450 m, roll 40 to 200 deg/s; bombers: roll 15 to 70)",
			std::to_string(T.Rows.size()) + " types; held:" + Held + (Out.empty() ? "" : "; out:" + Out));
		Bad += Check(Stall.empty(), "VMin is 1.15 times the stall worked out from the type's weight, wing and lift", Stall.empty() ? "" : "not:" + Stall);
		Bad += Check(Cache.empty() && T.Flights.size() == T.Rows.size(), "the limits Read keeps are Limits' own", Cache.empty() ? std::to_string(T.Flights.size()) + " kept" : "differ:" + Cache);
		Bad += Check(Off.empty() && Turns >= 10, "the worked-out turn within three tenths of every turn the table gives", std::to_string(Turns) + " given" + (Off.empty() ? "" : "; off:" + Off));
	}

	// The orderings the record is sure of.
	{
		auto L = [&T](const char* Key)
		{
			const AirPerf::Row* R = T.Named(Key);
			return R ? T.LimitsOf(*R) : AirPerf::Flight();
		};
		auto Has = [&T](std::initializer_list<const char*> Keys)
		{
			for (const char* Key : Keys)
				if (!T.Named(Key)) return false;
			return true;
		};
		// The A6M out-turns the F4F and the P-40: the US Navy's tests of the Akutan Zero, autumn 1942, whose advice was to
		// dive, push over and roll hard right at about 200 knots rather than turn with it (as Wikipedia's "Akutan Zero"
		// gives it, citing Rearden, Koga's Zero); Lundstrom, The First Team (the Wildcat pilots' rule not to dogfight the
		// Zero); Chennault's rule for the AVG's P-40s, hit and dive away, never turn.
		Bad += Check(Has({ "A6M2", "F4F-4", "P-40E" }) && L("A6M2").RMin < L("F4F-4").RMin && L("A6M2").RMin < L("P-40E").RMin,
			"the A6M out-turns the F4F and the P-40", Num(L("A6M2").RMin) + " m against " + Num(L("F4F-4").RMin) + " and " + Num(L("P-40E").RMin));
		// The Fw 190 out-rolls the Bf 109 and the Spitfire: AFDU's trials of the captured Fw 190A-3 (August 1942) found it
		// markedly better than the Spitfire V in the rate of roll (as a summary read on 25 November gives the report); the
		// wartime roll chart, as recalled (about 160 degrees a second against about 100; the table marks it uncertain).
		Bad += Check(Has({ "Fw 190A-4", "Bf 109E", "Bf 109F", "Spitfire I", "Spitfire V", "Spitfire IX" }) && L("Fw 190A-4").RollMax > L("Bf 109E").RollMax
			&& L("Fw 190A-4").RollMax > L("Bf 109F").RollMax && L("Fw 190A-4").RollMax > L("Spitfire I").RollMax && L("Fw 190A-4").RollMax > L("Spitfire V").RollMax
			&& L("Fw 190A-4").RollMax > L("Spitfire IX").RollMax,
			"the Fw 190 out-rolls the Bf 109 and the Spitfire", Num(L("Fw 190A-4").RollMax / Deg) + " deg/s against " + Num(L("Bf 109F").RollMax / Deg) + " and " + Num(L("Spitfire V").RollMax / Deg));
		// The Spitfire out-turns the Bf 109: the RAE's trials of the captured Bf 109E (Morgan and Morris, 1940: 885 ft
		// against the Spitfire's 696 ft at 12,000 ft, as commonly quoted; not read on the page). The Mk V against the F
		// and the IX against the G likewise: a reading, from their wing loadings (the 109's slats allowed for: the table's
		// clmax). (Ackroyd and Lamont's paper compares the four of 1940, but only its abstract was read, which gives no
		// radii and no ranking: it is not this ordering's source.)
		Bad += Check(L("Spitfire I").RMin < L("Bf 109E").RMin && L("Spitfire V").RMin < L("Bf 109F").RMin && L("Spitfire IX").RMin < L("Bf 109G").RMin,
			"the Spitfire out-turns the Bf 109", Num(L("Spitfire I").RMin) + " m against " + Num(L("Bf 109E").RMin) + "; V " + Num(L("Spitfire V").RMin) + " against F " + Num(L("Bf 109F").RMin));
		// The P-47 and the P-51 out-dive the Bf 109: their pilot's flight operating instructions' limits (500 and 505 mph
		// indicated, as recalled) against the 109's 750 km/h; the Eighth Air Force's fighters followed a diving 109 and
		// caught it (a reading).
		Bad += Check(L("P-47D").VDive > L("Bf 109G").VDive && L("P-51D").VDive > L("Bf 109G").VDive && L("P-51B").VDive > L("Bf 109F").VDive && L("P-47D").VDive > L("Bf 109E").VDive,
			"the P-47 and the P-51 out-dive the Bf 109", Num(L("P-47D").VDive * 3.6) + " and " + Num(L("P-51D").VDive * 3.6) + " km/h against " + Num(L("Bf 109G").VDive * 3.6));
		// The Bf 109 out-climbs the Hurricane: the makers' and the A&AEE's figures, as the rules' own data/aircraft.json
		// has them (15.5 against 12.9 m/s); the 109E's pilots climbed away from Hurricanes in 1940.
		Bad += Check(L("Bf 109E").ClimbMost > L("Hurricane I").ClimbMost, "the Bf 109 out-climbs the Hurricane",
			Num(L("Bf 109E").ClimbMost, 1) + " m/s against " + Num(L("Hurricane I").ClimbMost, 1));
		// The B-17 flies higher than the He 111: turbochargers (the B-17G's 35,600 ft service ceiling against the He 111H's
		// 7,800 m loaded); the makers' figures.
		Bad += Check(L("B-17G").Ceiling > L("He 111H").Ceiling, "the B-17 flies higher than the He 111", Num(L("B-17G").Ceiling) + " m against " + Num(L("He 111H").Ceiling));
		// The Me 262 is the fastest, and turns the widest of the fighters: its makers' 870 km/h; its pilots were told never
		// to turn with piston fighters (a jet's thrust is weak at low speed and its wing loading high). Both as it flies
		// (RMin, held to 450 m) and as worked out before the hold.
		{
			const AirPerf::Row* JetRow = T.Named("Me 262A");
			const AirPerf::Flight Jet = L("Me 262A");
			const double JetWorked = JetRow ? AirPerf::TurnRadius(*JetRow, AirPerf::FightM) : 0.0;
			std::string Faster, Tighter;
			for (const AirPerf::Row& R : T.Rows)
			{
				if (R.Key == "Me 262A") continue;
				const AirPerf::Flight F = T.LimitsOf(R);
				if (F.VTop >= Jet.VTop) Faster += " " + R.Key;
				if (R.IsFighter() && (F.RMin >= Jet.RMin || AirPerf::TurnRadius(R, AirPerf::FightM) >= JetWorked)) Tighter += " " + R.Key;
			}
			Bad += Check(JetRow && Jet.VTop > 0.0 && Faster.empty() && Tighter.empty(), "the Me 262 is the fastest and turns the worst",
				Num(Jet.VTop * 3.6) + " km/h, " + Num(Jet.RMin) + " m flown (" + Num(JetWorked) + " worked out)" + (Faster.empty() ? "" : "; as fast:" + Faster)
				+ (Tighter.empty() ? "" : "; as wide:" + Tighter));
		}
	}

	// A model's plain name flies the mark of its date, as the record dates the marks (guns.ts's where they agree: the
	// Spitfire V from 1941, the Fw 190A-8 from 1944, the P-51D from June 1944, the Zero's, the Yak's and the Betty's, the
	// B-17G from September 1943; the record's where guns.ts has no mark: the Bf 109G from mid-1942, the G-2 replacing the
	// F-4 from about June 1942; the Spitfire IX from 1943, the V being most of Fighter Command's Spitfires through 1942,
	// both readings of the date); a mark named outright flies as named (the reviewer of 25 November found "Bf 109E-4" of
	// 1943 flying as an F); a stand-in its nation's type.
	{
		struct Date
		{
			const char* Model;
			const char* Nation;
			double Year;
			const char* Wants;
		};
		static const Date Dates[] = {
			{ "Spitfire", "", 1940.6, "Spitfire I" }, { "Spitfire", "", 1941.5, "Spitfire V" }, { "Spitfire", "UK", 1943.0, "Spitfire IX" },
			{ "Spitfire", "", 0.0, "Spitfire I" }, { "Spitfire IX", "", 1940.0, "Spitfire IX" }, { "Bf109", "GER", 1940.6, "Bf 109E" },
			{ "Bf 109", "", 1942.0, "Bf 109F" }, { "Bf 109", "", 1942.6, "Bf 109G" }, { "Me 109", "", 1944.0, "Bf 109G" }, { "Fw190", "", 1943.0, "Fw 190A-4" }, { "Fw190", "", 1944.3, "Fw 190A-8" },
			{ "Spitfire", "", 1942.6, "Spitfire V" }, { "Bf 109E-4", "", 1943.0, "Bf 109E" }, { "Bf 109E-3", "GER", 1941.5, "Bf 109E" }, { "Emil", "", 1942.0, "Bf 109E" },
			{ "Fw 190A-3", "", 1944.5, "Fw 190A-4" }, { "Spitfire Mk I", "", 1943.0, "Spitfire I" }, { "Spitfire IA", "", 1942.0, "Spitfire I" }, { "A6M2 model 21", "", 1944.5, "A6M2" },
			{ "G4M1 model 11", "", 1944.0, "G4M1" }, { "Bf 109E", "", 1944.0, "Bf 109E" }, { "P-51D", "", 1944.0, "P-51D" }, { "B17", "USA", 1943.3, "B-17F" }, { "B17", "", 1944.0, "B-17G" },
			{ "B17", "", 0.0, "B-17G" }, { "B-17F", "", 1945.0, "B-17F" }, { "B-17E", "", 1942.5, "B-17F" },
			{ "Mustang", "", 1944.0, "P-51B" }, { "Mustang", "", 1944.6, "P-51D" }, { "Mustang", "", 0.0, "P-51D" }, { "Zero", "JAP", 1942.0, "A6M2" },
			{ "Zero", "", 1943.0, "A6M3" }, { "Zero", "", 1944.2, "A6M5" }, { "Yak", "SOV", 1942.2, "Yak-1" }, { "Yak", "", 1942.9, "Yak-1B" },
			{ "Yak", "", 1943.5, "Yak-9" }, { "Yak", "", 1944.8, "Yak-3" }, { "Betty", "", 1942.0, "G4M1" }, { "Betty", "", 1944.0, "G4M2" },
			{ "Fighter:SOV", "", 1941.0, "I-16 type 24" }, { "Fighter:ITA", "", 1941.0, "C.200" }, { "Fighter:ITA", "", 1942.0, "C.202" },
			{ "Fighter:FRA", "", 1940.3, "MS.406" }, { "Fighter:FRA", "", 1940.6, "D.520" }, { "Fighter:POL", "", 1939.7, "P.11c" },
			{ "Fighter", "FIN", 1942.0, "B-239" }, { "Fighter:HUN", "", 1940.0, "Fighter" }, { "Fighter:HUN", "", 1943.0, "Fighter 1942" },
			{ "Fighter", "", 0.0, "Fighter" }, { "Bomber:SOV", "", 1940.0, "SB-2" }, { "Bomber:FRA", "", 1939.8, "MB.210" }, { "Bomber:UK", "", 1940.0, "Blenheim IV" },
			{ "Bomber:HUN", "", 1941.0, "Bomber" }, { "Dive:ITA", "", 1941.0, "Ju 87B" }, { "Dive:FRA", "", 1940.0, "Dive" }, { "Torpedo:FRA", "", 1940.0, "Torpedo" },
			{ "Patrol:JAP", "", 1942.0, "PBY-5" }, { "Kate:JAP", "", 1941.9, "B5N2" }, { "Avenger:USA", "", 1943.0, "TBF-1" }, { "CarrierFighter", "ITA", 1942.0, "Fighter" },
		};
		std::string Wrong;
		for (const Date& D : Dates)
		{
			const AirPerf::Row* R = T.Of(D.Model, D.Nation, D.Year);
			if (!R || R->Key != D.Wants) Wrong += std::string(" ") + D.Model + " " + Num(D.Year, 1) + ": " + (R ? R->Key : std::string("none")) + " (wants " + D.Wants + ")";
		}
		Bad += Check(Wrong.empty(), "a model's plain name flies its date's mark, a mark named outright as named, a plain model its nation's type", std::to_string(std::size(Dates)) + " asked" + Wrong);
	}

	// A line a type: the limits as the flight core would fly them.
	std::printf("\n  The limits as the flight core would fly them (RMin as flown; the turn's worked-out radius and time at 3,000 m after).\n");
	std::printf("  %-14s %-15s %6s %6s %6s %6s %6s %5s %6s %6s %6s %7s\n", "type", "role", "VMin", "VMax", "VTop", "VDive", "RMin", "GMax", "Roll", "RollT", "Climb", "Ceiling");
	std::printf("  %-14s %-15s %6s %6s %6s %6s %6s %5s %6s %6s %6s %7s\n", "", "", "m/s", "m/s", "m/s", "m/s", "m", "g", "deg/s", "s", "m/s", "m");
	for (const AirPerf::Row& R : T.Rows)
	{
		const AirPerf::Flight F = T.LimitsOf(R);
		double Seconds = 0.0;
		const double Worked = AirPerf::TurnRadius(R, AirPerf::FightM, &Seconds);
		std::printf("  %-14s %-15s %6.1f %6.1f %6.1f %6.1f %6.0f %5.1f %6.0f %6.2f %6.1f %7.0f  (turn %.0f m, %.1f s)\n", R.Key.c_str(), R.Role.c_str(), F.VMin, F.VMax, F.VTop,
			F.VDive, F.RMin, F.GMax, F.RollMax / Deg, F.RollTime, F.ClimbMost, F.Ceiling, Worked, Seconds);
	}
	std::printf(Bad == 0 ? "\nperformance: all checks pass\n" : "\nperformance: %d checks FAILED\n", Bad);
	return Bad == 0 ? 0 : 1;
}
