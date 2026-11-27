// The silver Mustang (the Battle rules card's "Silver Mustang", row flyMustangMetal; Battle/AirSheen's ReadOwn and
// BareTune). Usage: air_mustang_metal_test <MustangBare.json> <Mustang.json> <Source dir>.
//
// Brian, 27 November 2026: "shouldnt the p-51 mustangs be silver with black paint?", "it should be shiny silver",
// "metalic". Her file is the air rules thread's issue-13 (the art director's pick, E0E3E5 at roughness 0.28), kept as
// Content/Models/MustangBare.json beside the old Mustang.json, which the row off still draws. Checked here: both files'
// bytes; her finish read as the game reads it, paint by paint against the art director's rulings; M_AirPaint's law on
// her numbers (her bare skin polished metal under its own floor, no glare guard; her paint and marks satin, no metal);
// refusals; and the game's side as text (the row off unless asked, the name, the ink, the read).
#include "Battle/AirSheen.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failed = 0;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s  %s\n", bOk ? "ok   " : "FAIL ", What.c_str());
		Failed += bOk ? 0 : 1;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Out;
		Out << In.rdbuf();
		return Out.str();
	}

	int Count(const std::string& Text, const std::string& Find)
	{
		int N = 0;
		for (size_t At = Text.find(Find); At != std::string::npos; At = Text.find(Find, At + Find.size()))
		{
			++N;
		}
		return N;
	}

	uint64_t Fnv(const std::string& Bytes)
	{
		uint64_t H = 0xcbf29ce484222325ull;
		for (const unsigned char C : Bytes)
		{
			H ^= C;
			H *= 0x100000001b3ull;
		}
		return H;
	}

	/** Her near paints, as numbers, from the file's own "paints" list (the reader does not keep them). */
	std::vector<int> NearPaints(const std::string& Text)
	{
		std::vector<int> Out;
		const size_t Near = Text.find("\"near\"");
		const size_t Key = Near == std::string::npos ? std::string::npos : Text.find("\"paints\"", Near);
		const size_t Open = Key == std::string::npos ? std::string::npos : Text.find('[', Key);
		const size_t Close = Open == std::string::npos ? std::string::npos : Text.find(']', Open);
		if (Close == std::string::npos) return Out;
		const char* P = Text.c_str() + Open + 1;
		const char* End = Text.c_str() + Close;
		while (P < End)
		{
			char* Next = nullptr;
			const long V = std::strtol(P, &Next, 10);
			if (Next == P) { ++P; continue; }
			Out.push_back(static_cast<int>(V));
			P = Next;
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::printf("usage: air_mustang_metal_test <MustangBare.json> <Mustang.json> <Source dir>\n");
		return 2;
	}
	std::printf("silver Mustang: her bare metal file, its finish and the game's row\n");
	const std::string Bare = ReadAll(Argv[1]);
	const std::string Old = ReadAll(Argv[2]);
	const std::string Src = Argv[3];

	// The files' bytes: hers is issue-13's (sha256 4b7fe030...), the old one untouched (the row off draws it)
	Check(Bare.size() == 3338874 && Fnv(Bare) == 0x7fe81244a95260aaull, "MustangBare.json is issue-13/Mustang.json byte for byte (3,338,874 bytes, FNV-1a 7fe81244a95260aa)");
	Check(Old.size() == 2840697 && Fnv(Old) == 0x5eb305dffb669325ull, "Mustang.json is untouched (the row off draws every Mustang from it, as before)");
	Check(Bare.find("\"schemes\"") == std::string::npos && Bare.find("mark_months") == std::string::npos,
		"her file carries no paint by date (Brian's standing rule: markings and paint never by year)");

	// Her finish as the game reads it
	AirSheen::OwnFinish F;
	std::string Why;
	const bool bRead = AirSheen::ReadOwn(Bare, F, Why);
	Check(bRead && F.Near.Count() == 40535 && F.Far.Count() == 4756 && F.NearShade.size() == 40535 && F.FarShade.size() == 4756,
		"read: near 40,535 and far 4,756 points, gloss, metal and shade each one a point" + (bRead ? std::string() : " (" + Why + ")"));
	const std::vector<int> Paints = NearPaints(Bare);
	Check(Paints.size() == F.Near.Count(), "her near paints, one a point");
	// (paint numbers, as her file's paint_names: 0 upper, 2 under, 3 nose, 4 boss, 5 glass, 6 frame, 16 glare, 40 band, 41 band_under)
	std::map<int, std::set<int>> Glosses, Metals;
	for (size_t K = 0; bRead && K < Paints.size() && K < F.Near.Count(); ++K)
	{
		Glosses[Paints[K]].insert(F.Near.Gloss[K]);
		Metals[Paints[K]].insert(F.Near.Metal[K]);
	}
	auto All = [&](int Paint, int Least, int Most, int Metal)
	{
		if (Glosses[Paint].empty()) return false;
		for (const int G : Glosses[Paint]) if (G < Least || G > Most) return false;
		return Metals[Paint].size() == 1 && *Metals[Paint].begin() == Metal;
	};
	Check(All(0, 25, 35, 100) && All(2, 25, 35, 100) && All(6, 25, 32, 100),
		"bare skin (upper, under, canopy frame): metal 100, gloss 25 to 35 (the art director's pick, 04:58Z)");
	Check(All(3, 50, 100, 0) && All(4, 50, 100, 0) && All(16, 50, 100, 0) && All(40, 50, 100, 0) && All(41, 50, 100, 0),
		"her black paint (nose, spinner, glare panel, bands): metal 0, gloss 50 or more (paint stays paint)");
	Check(All(5, 7, 7, 0), "her glass: gloss 7, no metal");
	Check(F.MarkGloss == 50 && F.MarkMetal == 0, "her marks (star and bar, number), laid after her file's points: satin, no metal");

	// M_AirPaint's law on her numbers (her own tune: a floor of 0.25, no glare guard)
	const AirSheen::Tune T = AirSheen::BareTune();
	Check(T.Floor == 0.25 && T.Glare == 0.0 && T.Gain == 1.0 && T.MetalFrom == 0.5 && T.Graze == 0.0, "her tune: floor 0.25, glare guard 0, the rest the B-17's");
	{
		const AirSheen::Carried C = AirSheen::Carry(28, 100, 1.0);
		const AirSheen::Finish Skin = AirSheen::Law(C.U, C.V, AirSheen::Untouched, 0.15, T, 0.0);
		Check(std::fabs(Skin.Rough - 0.28) < 0.002 && Skin.Metal > 0.999, "bare skin at her pick: roughness 0.28, metal 1 (polished metal, not pale paint)");
		const AirSheen::Finish Under = AirSheen::Law(C.U, C.V, AirSheen::Untouched, 0.15, AirSheen::Tune(), 0.0);
		Check(Under.Rough > 0.449, "the B-17's floor (0.45) would have held her at satin: hence her own");
		// (no glare guard: at the raid's low sun, looking straight down her mirror, the guard is 0, so her finish stands)
		const AirSheen::Vec3 N{ 0.0, 0.0, 1.0 }, Sun{ 0.0, 0.97, 0.24 }, Eye{ 0.0, -0.97, 0.24 };
		const double Guard = AirSheen::GuardOf(N, Eye, Sun, T);
		const double BGuard = AirSheen::GuardOf(N, Eye, Sun, AirSheen::Tune());
		Check(Guard == 0.0 && BGuard > 0.9, "no glare guard on her: 0 in the sun's mirror at 14 degrees (the B-17's would be 1 there)");
		const AirSheen::Carried P = AirSheen::Carry(60, 0, 1.0);
		const AirSheen::Finish Band = AirSheen::Law(P.U, P.V, AirSheen::Untouched, 0.15, T, 0.0);
		Check(std::fabs(Band.Rough - 0.60) < 0.002 && Band.Metal == 0.0, "her black bands: roughness 0.60, no metal");
		const AirSheen::Carried M = AirSheen::Carry(F.MarkGloss, F.MarkMetal, 1.0);
		const AirSheen::Finish Mark = AirSheen::Law(M.U, M.V, AirSheen::Untouched, 0.15, T, 0.0);
		Check(std::fabs(Mark.Rough - 0.50) < 0.002 && Mark.Metal == 0.0, "her star and bar: roughness 0.50, no metal");
		const AirSheen::Finish Nothing = AirSheen::Law(0.0, 0.0, 0.9, 0.15, T, 0.0);
		Check(Nothing.Rough == 0.9 && Nothing.Spec == 0.15 && Nothing.Metal == 0.0, "nothing carried: the flat ink's numbers exactly");
		// (every near point's finish lies in range: no point is a mirror, and only metal points carry metal)
		bool bRange = bRead;
		for (size_t K = 0; bRange && K < F.Near.Count(); ++K)
		{
			const AirSheen::Carried E = AirSheen::Carry(F.Near.Gloss[K], F.Near.Metal[K], F.NearShade[K]);
			const AirSheen::Finish Each = AirSheen::Law(E.U, E.V, AirSheen::Untouched, 0.15, T, 0.0);
			bRange = Each.Rough >= 0.069 && Each.Rough <= 0.9 && Each.Metal >= 0.0 && Each.Metal <= 1.0 && (F.Near.Metal[K] >= 50 || Each.Metal == 0.0)
				&& std::isfinite(Each.Rough);
		}
		Check(bRange, "every near point: roughness 0.07 (her glass) to 0.9, metal only where her file has 50 or more");
	}

	// Refusals
	{
		AirSheen::OwnFinish R;
		std::string W;
		Check(!AirSheen::ReadOwn("{}", R, W) && !AirSheen::ReadOwn("not json", R, W), "no near, or not JSON: refused");
		Check(!AirSheen::ReadOwn("{\"near\":{\"paints\":[0,1],\"gloss\":[10,20],\"metal\":[0,0]},\"far\":{\"paints\":[0],\"gloss\":[10],\"metal\":[0],\"shade\":[1]}}", R, W),
			"no shade: refused");
		Check(!AirSheen::ReadOwn("{\"near\":{\"paints\":[0],\"gloss\":[101],\"metal\":[0],\"shade\":[1]},\"far\":{\"paints\":[0],\"gloss\":[10],\"metal\":[0],\"shade\":[1]}}", R, W),
			"a gloss over 100: refused");
		Check(!AirSheen::ReadOwn("{\"near\":{\"paints\":[0],\"gloss\":[10],\"metal\":[0],\"shade\":[1.5]},\"far\":{\"paints\":[0],\"gloss\":[10],\"metal\":[0],\"shade\":[1]}}", R, W),
			"a shade over 1: refused");
		Check(AirSheen::ReadOwn("{\"near\":{\"paints\":[0],\"gloss\":[10],\"metal\":[0],\"shade\":[0.5]},\"far\":{\"paints\":[0],\"gloss\":[10],\"metal\":[100],\"shade\":[1]}}", R, W)
				&& R.Far.Metal[0] == 100 && R.NearShade[0] == 0.5f,
			"a small good file: read as written");
	}

	// The game's side, as text
	{
		const std::string Live = ReadAll(Src + "/Battle/LandLive.cpp");
		const std::string Battle = ReadAll(Src + "/Battle/AirBattle.cpp");
		const std::string State = ReadAll(Src + "/Battle/AirBattleState.h");
		Check(Count(Live, "{ TEXT(\"flyMustangMetal\"), TEXT(\"Silver Mustang\"),") == 1
				&& Count(Live, "TEXT(\"TheaterAirNoMustangMetal\"), false, TEXT(\"TheaterAirMustangMetal\") },") == 1,
			"the row: Silver Mustang, off unless asked (-TheaterAirMustangMetal)");
		Check(Count(State, "int MustangMetalOn = -1;") == 1 && Count(Battle, "State->MustangMetalOn = 0;\n\tif (!FLandLive::RuleNow(TEXT(\"flyMustangMetal\")))\n\t{\n\t\treturn;\n\t}") == 1,
			"the row off: nothing read, nothing said");
		Check(Count(Battle, "const FString Name = bBare && Plain == TEXT(\"Mustang\") ? FString(TEXT(\"MustangBare\")) : Plain;") == 1
				&& Count(Battle, "const bool bBare = State && State->MustangMetalOn == 1 && BareNow;") == 1
				&& Count(Battle, "(bBare ? TEXT(\"|bare\") : TEXT(\"\"))") == 1,
			"every Mustang drawn from MustangBare.json only with the row on (ModelFor), kept apart in its answers");
		Check(Count(Battle, "AirParts::OwnOnto(Near, BareNow->Near, BareNow->NearShade, BareMarks);") == 1
				&& Count(Battle, "AirParts::OwnOnto(Away, BareNow->Far, BareNow->FarShade, BareMarks);") == 1,
			"her near and far shapes carry her finish (so her far copies glint too)");
		Check(Count(Battle, "if (BareInk && Shape.StartsWith(TEXT(\"MustangBare:\")))") == 1 && Count(Battle, "BareInk = BareMade;") == 1
				&& Count(Battle, "TEXT(\"Models/MustangBare.json\")") == 1 && Count(Battle, "MustangMetalRead();") == 2,
			"her ink on M_AirPaint, her file read once a run, at both battle starts");
		Check(Count(Battle, "TEXT(\"TheaterAirMustangMetalInk=\")") == 1, "her numbers for stills: -TheaterAirMustangMetalInk=gain,floor,spec,metal,glare,graze,metalfrom");
	}

	if (Failed == 0)
	{
		std::printf("silver Mustang: all checks pass\n");
	}
	else
	{
		std::printf("silver Mustang: %d FAILED\n", Failed);
	}
	return Failed == 0 ? 0 : 1;
}
