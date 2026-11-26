// The finer B-17 behind its switch (the Battle rules card's "Finer B-17", row b17Fine, release 2; Brian, 25 November 2026:
// "why don't we have the new b-17 models in the game?"). Two things are proved here, on the game's own standard-C++ cores:
//
// - With the row off, the game reads what it read before the row was there: Content/Models/B17.json and
//   Content/Data/NoseArt/panels.json the same bytes as on the base (30899121), the B-17 asked for by that name, the flight
//   core's shape for her (span, length, engines, gunners' stations) the same numbers, her panel the same panel, and her
//   earned colours laid the same way (no rivet or seam line of any of today's models taking the coat's colour).
// - With the row on, every one of them is the newer B-17's: the file B17Fine.json (its sha256 5e1f3900...d0d79b, as
//   the modeller handed it over), her engines where her file's sockets put them, her own panel from panels-fine.json,
//   her rivet and seam lines taking the coat's colour, and each of her four airscrews turned by an engine of its own.
//
// The Unreal side (AAirBattle::ModelFor, its readers and caches, the nose-art panels' reader, the close-up copies' bound)
// cannot be built here; what can be checked of it without the engine is checked from its source: every reader of a model
// file takes the file's name from the shape's name, which ModelFor gives, and nothing in the air battle names B17.json.
//
//   air_b17_fine_test <Content/Models> <Content/Data/NoseArt> <Source/TheaterCommand>

#include "Battle/AirFeed.h"
#include "Battle/AirNoseArt.h"
#include "Battle/AirScrew.h"
#include "Battle/AirShow.h"
#include "Battle/AirUnitColours.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;

	void Check(bool bOk, const char* What, const std::string& Detail = std::string())
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Detail.empty() ? "" : ": ", Detail.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		Out.assign(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
		return true;
	}

	/** FNV-1a, 64 bits: the files' fingerprints as they stand on the base. */
	uint64_t Print(const std::string& Bytes)
	{
		uint64_t H = 0xcbf29ce484222325ull;
		for (const char C : Bytes)
		{
			H ^= static_cast<unsigned char>(C);
			H *= 0x100000001b3ull;
		}
		return H;
	}

	std::string Hex(uint64_t V)
	{
		char Out[32];
		std::snprintf(Out, sizeof Out, "%016llx", static_cast<unsigned long long>(V));
		return Out;
	}

	bool Near(double A, double B, double Within) { return std::fabs(A - B) <= Within; }

	/** The flight core's shape for the B-17 before the row was there (AirShow.cpp at 30899121, its "B17" line). */
	AirShow::Shape Before()
	{
		AirShow::Shape S;
		S.Span = 31.6;
		S.Long = 22.7;
		for (const double Y : { -9.2, -4.9, 4.9, 9.2 }) S.Engines.push_back(AirShow::Vec(1.6, Y, 0.2));
		const double L = 22.7 * 0.5, High = 2.2;
		S.Stations["nose"] = AirShow::Vec(L * 0.95, 0.0, 0.1 * High);
		S.Stations["chin"] = AirShow::Vec(L * 0.85, 0.0, -0.35 * High);
		S.Stations["top"] = AirShow::Vec(L * 0.45, 0.0, 0.75 * High);
		S.Stations["radio"] = AirShow::Vec(-L * 0.05, 0.0, 0.55 * High);
		S.Stations["ball"] = AirShow::Vec(-L * 0.15, 0.0, -0.55 * High);
		S.Stations["waistL"] = AirShow::Vec(-L * 0.4, -0.45 * High, 0.15 * High);
		S.Stations["waistR"] = AirShow::Vec(-L * 0.4, 0.45 * High, 0.15 * High);
		S.Stations["tail"] = AirShow::Vec(-L * 0.97, 0.0, 0.15 * High);
		S.Stations["cockpit"] = AirShow::Vec(22.7 * 0.35, 0.0, 1.0);
		S.Body = High;
		return S;
	}

	bool SameVec(const AirShow::Vec& A, const AirShow::Vec& B) { return A.X == B.X && A.Y == B.Y && A.Z == B.Z; }

	/** Every number of two shapes the same, bit for bit (bEngines false: all but the engines). */
	bool SameShape(const AirShow::Shape& A, const AirShow::Shape& B, bool bEngines, std::string& Why)
	{
		if (A.Span != B.Span || A.Long != B.Long || A.Body != B.Body || A.bDive != B.bDive) { Why = "span, length, body or dive"; return false; }
		if (A.Stations.size() != B.Stations.size()) { Why = "stations"; return false; }
		for (const auto& [Name, At] : A.Stations)
		{
			const auto It = B.Stations.find(Name);
			if (It == B.Stations.end() || !SameVec(At, It->second)) { Why = "station " + Name; return false; }
		}
		if (bEngines)
		{
			if (A.Engines.size() != B.Engines.size()) { Why = "engines"; return false; }
			for (size_t K = 0; K < A.Engines.size(); ++K)
				if (!SameVec(A.Engines[K], B.Engines[K])) { Why = "engine " + std::to_string(K + 1); return false; }
		}
		return true;
	}

	/** A model file's near shape: points, paints and triangles, and its box's middle. */
	struct NearShape
	{
		std::vector<AirScrew::Vec3> Points;
		std::vector<int> Paints, Triangles;
		double Middle[3] = { 0.0, 0.0, 0.0 };
	};

	bool ReadNear(const AirFeed::Json& J, NearShape& Out)
	{
		const AirFeed::Json& N = J["near"];
		const std::vector<AirFeed::Json>& P = N["points"].Items;
		if (P.empty() || P.size() % 3 != 0) return false;
		double Lo[3] = { 1e9, 1e9, 1e9 }, Hi[3] = { -1e9, -1e9, -1e9 };
		for (size_t K = 0; K + 2 < P.size(); K += 3)
		{
			const AirScrew::Vec3 V{ P[K].Num(), P[K + 1].Num(), P[K + 2].Num() };
			Out.Points.push_back(V);
			const double C[3] = { V.X, V.Y, V.Z };
			for (int I = 0; I < 3; ++I)
			{
				Lo[I] = std::min(Lo[I], C[I]);
				Hi[I] = std::max(Hi[I], C[I]);
			}
		}
		for (int I = 0; I < 3; ++I) Out.Middle[I] = 0.5 * (Lo[I] + Hi[I]);
		for (const AirFeed::Json& V : N["paints"].Items) Out.Paints.push_back(V.Int(0));
		for (const AirFeed::Json& V : N["triangles"].Items) Out.Triangles.push_back(V.Int(0));
		return Out.Paints.size() == Out.Points.size();
	}

	/** Each airscrew's engine as AAirBattle::TakeDiscEngines numbers it: the engine nearest its hub across the span. */
	std::vector<int> EnginesOfScrews(const AirScrew::Found& F, const AirShow::Shape& S)
	{
		std::vector<int> Out;
		for (const AirScrew::Screw& Sc : F.Screws)
		{
			int Best = 1;
			double Off = 1e9;
			for (size_t K = 0; K < S.Engines.size(); ++K)
			{
				const double D = std::fabs(S.Engines[K].Y - Sc.Hub.Y);
				if (D < Off)
				{
					Off = D;
					Best = static_cast<int>(K) + 1;
				}
			}
			Out.push_back(Best);
		}
		return Out;
	}

	size_t Count(const std::string& Text, const std::string& Word)
	{
		size_t N = 0;
		for (size_t At = Text.find(Word); At != std::string::npos; At = Text.find(Word, At + Word.size())) ++N;
		return N;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_b17_fine_test <Content/Models> <Content/Data/NoseArt> <Source/TheaterCommand>\n");
		return 2;
	}
	const std::string Models = Argv[1], NoseArt = Argv[2], Source = Argv[3];
	std::string OldText, FineText, Panels, FinePanels, Why;
	const bool bOld = ReadText(Models + "/B17.json", OldText);
	const bool bFineFile = ReadText(Models + "/B17Fine.json", FineText);
	const bool bPanels = ReadText(NoseArt + "/panels.json", Panels);
	const bool bFinePanels = ReadText(NoseArt + "/panels-fine.json", FinePanels);

	// ---- The files.
	std::printf("The files\n");
	// (pins: B17.json and panels.json as they stood at 30899121, when the row was added. A change to either made on purpose
	// (panels.json baked again, say) fails these two, and is no fault of the row: then set the new size and hash here.)
	Check(bOld && OldText.size() == 1561602u && Print(OldText) == 0xa225e548b55800afull, "B17.json the base's bytes, as pinned here (the row off reads them; changed on purpose? update the pin)",
		std::to_string(OldText.size()) + " bytes, " + Hex(Print(OldText)));
	Check(bFineFile && FineText.size() == 10295280u && Print(FineText) == 0xa7a7f4b8f73a6e13ull, "B17Fine.json the modeller's file as handed over (sha256 5e1f3900...d0d79b)",
		std::to_string(FineText.size()) + " bytes, " + Hex(Print(FineText)));
	Check(bPanels && Panels.size() == 186570u && Print(Panels) == 0x24d2774256b94f3full, "panels.json the base's bytes, as pinned here (the row off reads it alone; changed on purpose? update the pin)",
		std::to_string(Panels.size()) + " bytes, " + Hex(Print(Panels)));
	Check(bFinePanels, "panels-fine.json there (the finer B-17's own panel)");
	Check(!std::filesystem::exists(Models + "/Fine") && !std::filesystem::exists(Models + "/Old/B17.json"), "no other B-17 file the readers could take instead (Models/Fine, Models/Old/B17.json)");

	// ---- Which file a model is read from.
	std::printf("Which file each model is read from (AirShow::FileOf, which AAirBattle::ModelFor asks)\n");
	int Files = 0, Moved = 0;
	std::string MovedList;
	for (const auto& Entry : std::filesystem::directory_iterator(Models))
	{
		if (Entry.path().extension() != ".json") continue;
		const std::string Kind = Entry.path().stem().string();
		++Files;
		for (const bool bFine : { false, true })
		{
			const std::string To = AirShow::FileOf(Kind, bFine);
			if (To != Kind)
			{
				++Moved;
				MovedList += " " + Kind + (bFine ? "(on)->" : "(off)->") + To;
			}
		}
	}
	Check(AirShow::FileOf("B17", false) == "B17" && AirShow::FileOf("B17", true) == "B17Fine", "the B-17: B17 off, B17Fine on");
	Check(Moved == 1 && AirShow::FileOf("B17Fine", true) == "B17Fine", "every other model the same file either way", std::to_string(Files) + " files; moved:" + MovedList);
	Check(AirShow::FileOf("B-17", true) == "B-17" && AirShow::FileOf("Bomber", true) == "Bomber" && AirShow::FileOf("B24", true) == "B24",
		"only the file's own name moves (ModelFor gives it after its table: B-17 is B17 by then)");

	// ---- The flight core's shape.
	std::printf("The flight core's shape for the B-17 (AirShow::ShapeOf)\n");
	AirShow::SetFineB17(false);
	const AirShow::Shape Off = AirShow::ShapeOf("B17", false);
	Check(SameShape(Off, Before(), true, Why), "row off: span, length, engines and gunners' stations the base's, bit for bit", Why);
	AirShow::SetFineB17(true);
	const AirShow::Shape On = AirShow::ShapeOf("B17", false);
	const AirShow::Shape OtherOn = AirShow::ShapeOf("Lancaster", false);
	AirShow::SetFineB17(false);
	const AirShow::Shape OtherOff = AirShow::ShapeOf("Lancaster", false);
	const AirShow::Shape OffAgain = AirShow::ShapeOf("B17", false);
	Check(SameShape(On, Before(), false, Why), "row on: span, length and gunners' stations the same (her formation's slots and guns unchanged)", Why);
	Check(SameShape(OtherOn, OtherOff, true, Why) && SameShape(OffAgain, Before(), true, Why), "no other model changed, and the row off again gives the base's", Why);
	AirFeed::Json FineJ;
	NearShape FineNear;
	const bool bFineRead = bFineFile && AirFeed::Parse(FineText, FineJ, Why) && ReadNear(FineJ, FineNear);
	Check(bFineRead, "B17Fine.json read", Why);
	if (bFineRead)
	{
		// Her engines where her file's sockets put them, about the middle of her near shape's box, as the scene draws her
		// (and as the Betty's, the Nell's and the Wellington's lines were made): engine.1 her port outer, as the crews counted.
		std::string Line;
		bool bAll = On.Engines.size() == 4;
		for (int E = 1; E <= 4 && bAll; ++E)
		{
			bool bFound = false;
			for (const AirFeed::Json& S : FineJ["sockets"].Items)
			{
				if (S["name"].Str("") != "engine." + std::to_string(E)) continue;
				bFound = true;
				const AirShow::Vec Want(S["at"].At(0).Num() - FineNear.Middle[0], S["at"].At(1).Num() - FineNear.Middle[1], S["at"].At(2).Num() - FineNear.Middle[2]);
				const AirShow::Vec& Got = On.Engines[static_cast<size_t>(E - 1)];
				char Each[160];
				std::snprintf(Each, sizeof Each, " %d: %.2f %.2f %.2f (socket less middle %.3f %.3f %.3f);", E, Got.X, Got.Y, Got.Z, Want.X, Want.Y, Want.Z);
				Line += Each;
				bAll = bAll && Near(Got.X, Want.X, 0.01) && Near(Got.Y, Want.Y, 0.01) && Near(Got.Z, Want.Z, 0.01);
			}
			bAll = bAll && bFound;
		}
		Check(bAll, "row on: each engine at her file's socket less her box's middle, within a centimetre", Line);
		char Mid[96];
		std::snprintf(Mid, sizeof Mid, "%.3f %.3f %.3f", FineNear.Middle[0], FineNear.Middle[1], FineNear.Middle[2]);
		Check(Near(FineNear.Middle[0], -3.368, 0.001) && Near(FineNear.Middle[2], 1.6, 0.001), "her box's middle as measured for the engines' line", Mid);
		// Each airscrew turned by its own engine (TakeDiscEngines: nearest across the span): on her own engines four engines
		// for four screws; on the older line's two of them would share an engine.
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(FineText);
		std::vector<bool> Blade(FineNear.Points.size(), false);
		for (size_t K = 0; K < Blade.size(); ++K) Blade[K] = Are.IsBlade(FineNear.Paints[K]);
		const AirScrew::Found Screws = AirScrew::Find(FineNear.Points, Blade, FineNear.Triangles);
		const std::vector<int> Mine = EnginesOfScrews(Screws, On), Older = EnginesOfScrews(Screws, Off);
		std::string Seen;
		for (size_t K = 0; K < Mine.size(); ++K) Seen += " " + std::to_string(Mine[K]) + "/" + std::to_string(Older[K]);
		Check(Screws.Screws.size() == 4 && std::set<int>(Mine.begin(), Mine.end()).size() == 4, "row on: her four airscrews, each on an engine of its own (on her line / on the older line)", Seen);
	}

	// ---- The nose-art panels.
	std::printf("The nose-art panels (AirNoseArt::ReadPanel, as the battle reads them)\n");
	AirNoseArt::Panel P, Q;
	Check(AirNoseArt::ReadPanel(Panels, "B17", P, Why) && !AirNoseArt::ReadPanel(Panels, "B17Fine", Q, Why), "row off: panels.json the older B-17's panel and no other");
	Check(AirNoseArt::ReadPanel(FinePanels, "B17Fine", Q, Why) && !AirNoseArt::ReadPanel(FinePanels, "B17", P, Why), "row on: panels-fine.json the finer B-17's and no other", Why);

	// ---- Earned colours: rivet and seam lines.
	std::printf("Earned colours on rivet and seam lines (AirUnitColours)\n");
	{
		int Others = 0, Following = 0;
		std::string Which;
		for (const auto& Entry : std::filesystem::directory_iterator(Models))
		{
			if (Entry.path().extension() != ".json") continue;
			const std::string Kind = Entry.path().stem().string();
			if (AirUnitColours::LinesTakeCoat(Kind))
			{
				++Following;
				Which += " " + Kind;
			}
			else
			{
				++Others;
			}
		}
		Check(Following == 1 && AirUnitColours::LinesTakeCoat("B17Fine") && !AirUnitColours::LinesTakeCoat("B17"), "only the finer B-17's lines take the coat's colour; today's models' never",
			std::to_string(Others) + " others;" + Which);
	}

	// ---- The Unreal side, from its source (it cannot be built here: the pre-check builds it with the Mac's flags). These
	// count lines of text: they show the lines are there, not that the code behaves, and a harmless reformat breaks them
	// (then mend the text here). The proof for the Unreal side is the Mac's run with -TheaterAirNoB17Fine against the base.
	std::printf("The air battle's readers, from their source (%s)\n", Source.c_str());
	std::string Battle, Colours, Art, Rules;
	const bool bSource = ReadText(Source + "/Battle/AirBattle.cpp", Battle) && ReadText(Source + "/Battle/AirBattleUnitColours.cpp", Colours)
		&& ReadText(Source + "/Battle/AirBattleNoseArt.cpp", Art) && ReadText(Source + "/Battle/LandLive.cpp", Rules);
	Check(bSource, "the source read");
	if (bSource)
	{
		const std::string Reader = "TEXT(\"Models\") / (Kind + TEXT(\".json\"))";
		Check(Count(Battle, Reader) == 4 && Count(Colours, Reader) == 1, "five readers of a model's file by its shape's name: the airscrews (near, and far with the row flyPropSpin), the tally, the bay, the earned colours",
			std::to_string(Count(Battle, Reader)) + " in AirBattle.cpp, " + std::to_string(Count(Colours, Reader)) + " in AirBattleUnitColours.cpp");
		Check(Count(Battle, "AirShow::FileOf(AirParts::Narrow(Named ? *Named : Model), true)") == 1 && Count(Battle, "State->B17FineOn == 1") >= 2,
			"ModelFor names the finer file while the battle's row is on, and only then");
		Check(Count(Battle, "B17FineRead();") == 2, "the row read as each battle begins (a recording's and a live one's), before any shape is made");
		Check(Count(Battle, "B17.json") == 0 && Count(Colours, "B17.json") == 0 && Count(Art, "B17.json") == 0, "nothing in the air battle names B17.json");
		Check(Count(Battle, "if (!Made.bFine || Made.bNearMade)") == 1 && Count(Battle, "&& (bRidden || B17FineNearRoom(*Lit));") == 1,
			"the close-up copies' bound passes every aircraft but a finer B-17 at once (row off: none is one), and never the ridden one");
		Check(Count(Battle, "TEXT(\"TheaterAirTimeEye=\")") == 1 && Count(Battle, "if (Asked == 0 || !Eye || !bBegun || !State.IsValid())") == 1,
			"the timing eye does nothing without its word (-TheaterAirTimeEye=box, =far or =both)");
		Check(Count(Battle, "|| (TimeEyeKey >= 0 && P.Key == TimeEyeKey);") == 1 && Count(Battle, "TimeEyeKey = TimeEyeGroup = -1;") == 2,
			"the timing eye's box rider ridden as the close view's (her own close-up shape, counted past the bound), and no rider unless it rides");
		Check(Count(Art, "panels-fine.json") >= 1 && Count(Art, "if (S.B17FineOn == 1)") == 1, "panels-fine.json read only while the row is on");
		Check(Count(Rules, "TEXT(\"b17Fine\"), TEXT(\"Finer B-17\")") == 1 && Count(Rules, "RuleKey.Equals(TEXT(\"b17Fine\")") == 1
				&& Count(Rules, "TEXT(\"TheaterAirNoB17Fine\"), false, TEXT(\"TheaterAirB17Fine\")") == 1,
			"the row on the card, off unless asked (-TheaterAirB17Fine), and never sent to the land rules");
	}

	std::printf(Bad ? "b17 fine: %d failed\n" : "b17 fine: all checks pass\n", Bad);
	return Bad ? 1 : 0;
}
