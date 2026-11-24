// The riders without their packs (Effects, R2: Crowd::AddRider's bNoPack beside bBareHead; the soldiers thread's
// pack_tris, claude/project-thread-labtrr fc61734e: "Leave that run out and draw nothing in its place, because the cloth
// under it is whole"). Reads every soldier file in a folder (the men, their gore twins, the tankers) and checks:
//   read    each level's pack_tris as the file writes it (Soldier::PackFirst, PackCount), lying within its triangles,
//           never reaching into his helmet's run (failed: the read leaves such a run alone) and (reported) ending where
//           it begins; made-up runs not whole numbers, out of the level's triangles or reaching into the helmet's are
//           left alone (0), a whole one read; (reported, not failed) the helmet's paint at his collar and shoulders in
//           the run, badges and the like the soldiers' note keeps drawn (CollarInRun)
//   cut     in every file that marks its pack, at every level, four riders of one seed and ride: whole (as before),
//           bareheaded (as before), no pack, bareheaded and no pack. Each one's look is the file's triangles without
//           the runs (none; the helmet's; the pack's; both at once), in the file's order, to the last index: the pack's
//           run not drawn and nothing in its place; only the points no kept triangle uses dropped; every kept point
//           skinned and coloured as the whole man's own, to the bit; the whole man the file's own triangles
//   twins   a gore twin with pack_tris: its caps whole without his pack (no cap point dropped), its joined skin's points
//           that go with the pack counted (the crowd skips them, as it does the helmet's)
//   plain   a folder whose files have no pack_tris (today's Content): no pack read, and a rider with no pack is the same
//           man as before to the byte, helmeted or bareheaded (his points, triangles, colours and skin at every level)
//   dress   Crowd::PilotDress and CrewDress for every army: the tankers' file where it is here, else the infantry file
//           with the pack off (and, for a pilot, the helmet)
// Usage: land_pack_test <soldiers folder> <armies.json> [<a folder without pack_tris>]
#include "Battle/LandMen.h"
#include "land_json.h"
#include "pack_cut.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace LandMen;

namespace
{
	int Fails = 0;
	void Fail(const std::string& What)
	{
		++Fails;
		if (Fails <= 60) std::printf("  FAIL: %s\n", What.c_str());
	}

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	/** soldier_*.json in a folder, by the file's name without soldier_ and .json ("SOV", "SOV_gore", "USA_tanker"). */
	std::map<std::string, std::string> SoldierFiles(const std::string& Dir)
	{
		std::map<std::string, std::string> Out;
		std::error_code Why;
		for (const std::filesystem::directory_entry& E : std::filesystem::directory_iterator(Dir, Why))
		{
			const std::string Name = E.path().filename().string();
			if (Name.rfind("soldier_", 0) != 0 || Name.size() < 14 || Name.substr(Name.size() - 5) != ".json") continue;
			Out[Name.substr(8, Name.size() - 13)] = E.path().string();
		}
		return Out;
	}

	/** The file's pack_tris object as it writes it (its own small read, apart from the game's), or Null. */
	LandJson::Value RawPack(const std::string& Text)
	{
		LandJson::Value V;
		const size_t Key = Text.find("\"pack_tris\"");
		if (Key == std::string::npos) return V;
		const size_t Open = Text.find('{', Key), Close = Text.find('}', Key);
		if (Open == std::string::npos || Close == std::string::npos || Close < Open) return V;
		LandJson::Parse(Text.substr(Open, Close - Open + 1), V);
		return V;
	}

	/** The army a file's men are painted by: its name up to the first underscore ("UK_gore": UK). */
	std::string ArmyOf(const std::string& Name) { return Name.substr(0, Name.find('_')); }

	const char* const Levels[LodCount] = { "near", "mid", "far" };

	/** A crowd of no men (its riders only), with these files, the armies' colours and a July 1944 day in Normandy. */
	bool Ready(Crowd& C, const std::map<std::string, std::shared_ptr<Soldier>>& Files, const std::string& Armies, std::string& Err)
	{
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		const std::string Rec = "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, \"blocks\": [], \"men\": [], \"rounds\": []}";
		const GroundFn Flat = [](double, double) { return Ground(); };
		return C.ReadArmies(Armies, Err) && C.Build(Rec, Flat, Err);
	}

	/** The clip a rider stands in for the checks: idle where the file has it, else its first. */
	int StandClip(const Soldier& S)
	{
		const int Idle = S.ClipIndex("idle");
		return Idle >= 0 ? Idle : 0;
	}

	/** Whether a run [First, First + Count) reaches into his helmet's at that level (helmet_tris). */
	bool IntoHelmet(const Soldier& S, int L, int First, int Count)
	{
		return Count > 0 && S.HelmetCount[L] > 0 && First < S.HelmetFirst[L] + S.HelmetCount[L] && S.HelmetFirst[L] < First + Count;
	}

	/** The near pack run's triangles in the helmet's paint (5) at his collar and shoulders: on the neck, the head, a
	 * collarbone (the gore twins') or an upper arm, or on the chest's front (x > 0 at rest). In fc61734e these are the
	 * collar tabs (UK, POL, FRA, GER, CHN, JAP), the Italian's collar patches and the snow suit's hood's front hem
	 * (SOV_snow), which pack_range.py took for kit though the soldiers' note keeps badges drawn; the helmet's paint lower
	 * down (the German's gas-mask can on his hip, the pieces on the American's back) is kit and not counted. Reported,
	 * not failed: the files' to mend. */
	int CollarInRun(const Soldier& S)
	{
		const Mesh& M = S.Meshes[Near];
		int Out = 0;
		for (int T = S.PackFirst[Near]; T < S.PackFirst[Near] + S.PackCount[Near]; ++T)
		{
			const uint32_t P = M.Triangles[static_cast<size_t>(T) * 3];
			if (M.Paint[P] != 5 || M.Part[P] >= S.Bones.size()) continue;
			const std::string& Bone = S.Bones[M.Part[P]].Name;
			const bool bFront = M.At[static_cast<size_t>(P) * 3] > 0.f;
			const bool bShoulder = Bone == "clavicle_l" || Bone == "clavicle_r" || Bone == "upperarm_l" || Bone == "upperarm_r";
			Out += Bone == "neck" || Bone == "head" || bShoulder || (Bone == "chest" && bFront) ? 1 : 0;
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: land_pack_test <soldiers folder> <armies.json> [<a folder without pack_tris>]\n");
		return 2;
	}
	const std::string Armies = ReadFile(Argv[2]);
	std::string Err;
	std::printf("RIDERS WITHOUT THEIR PACKS (Effects, R2: AddRider's bNoPack; pack_tris) in %s\n", Argv[1]);

	// ---- Read, and cut: every file of the folder, one at a time (some 7 MB each).
	const std::map<std::string, std::string> Paths = SoldierFiles(Argv[1]);
	int Read = 0, Packed = 0, Twins = 0, EndsAtHelmet = 0, Levelled = 0, ReachesHelmet = 0;
	int Riders = 0, LeastPackTris = 1 << 30, MostPackTris = 0;
	std::vector<std::string> Unpacked;
	std::string Collars;
	for (const auto& KV : Paths)
	{
		const std::string Text = ReadFile(KV.second);
		auto S = std::make_shared<Soldier>();
		if (Text.empty() || !S->Load(Text, Err)) { Fail("soldier_" + KV.first + ".json: " + Err); continue; }
		++Read;
		const LandJson::Value Raw = RawPack(Text);
		bool bPack = false;
		for (int L = 0; L < LodCount; ++L)
		{
			const Lod Lv = static_cast<Lod>(L);
			const LandJson::Value& R = Raw[Levels[L]];
			const int RawFirst = R.At(0).Int(0), RawCount = R.At(1).Int(0);
			const int Tris = static_cast<int>(S->Meshes[L].Triangles.size() / 3);
			// (a run reaching into his helmet's would take part of a crewman's helmet with his pack: the file's fault, left
			// alone by the read, so he is drawn with his pack and all his helmet)
			if (IntoHelmet(*S, L, RawFirst, RawCount))
			{
				++ReachesHelmet;
				Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": its pack run " + std::to_string(RawFirst) + ", " + std::to_string(RawCount) + " reaches into his helmet's "
					+ std::to_string(S->HelmetFirst[L]) + ", " + std::to_string(S->HelmetCount[L]));
				if (S->PackCount[L] != 0) Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": a pack run reaching into his helmet's read all the same");
				continue;
			}
			if (S->PackFirst[L] != RawFirst || S->PackCount[L] != RawCount)
				Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": pack_tris read " + std::to_string(S->PackFirst[L]) + ", " + std::to_string(S->PackCount[L])
					+ ", the file " + std::to_string(RawFirst) + ", " + std::to_string(RawCount));
			if (S->PackCount[L] <= 0) continue;
			bPack = true;
			++Levelled;
			if (S->PackFirst[L] < 0 || S->PackFirst[L] + S->PackCount[L] > Tris) Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": its pack run out of its triangles");
			EndsAtHelmet += S->PackFirst[L] + S->PackCount[L] == S->HelmetFirst[Lv] ? 1 : 0;
			if (L == Near)
			{
				LeastPackTris = std::min(LeastPackTris, S->PackCount[L]);
				MostPackTris = std::max(MostPackTris, S->PackCount[L]);
			}
		}
		if (!bPack) { Unpacked.push_back(KV.first); continue; }
		++Packed;
		const int Collar = CollarInRun(*S);
		if (Collar > 0) Collars += (Collars.empty() ? "" : ", ") + KV.first + " " + std::to_string(Collar);

		// Four riders of one seed and ride: whole, bareheaded, no pack, both.
		Crowd C;
		std::map<std::string, std::shared_ptr<Soldier>> One;
		One[KV.first] = S;
		if (!Ready(C, One, Armies, Err)) { Fail("soldier_" + KV.first + ".json: the crowd: " + Err); continue; }
		const std::string Army = ArmyOf(KV.first);
		const uint64_t Seed = 0xC47E2ull;
		const int Whole = C.AddRider(KV.first, Army, Seed), Bare = C.AddRider(KV.first, Army, Seed, std::string(), true);
		const int NoPack = C.AddRider(KV.first, Army, Seed, std::string(), false, true), Both = C.AddRider(KV.first, Army, Seed, std::string(), true, true);
		if (Whole < 0 || Bare < 0 || NoPack < 0 || Both < 0) { Fail("soldier_" + KV.first + ".json: no rider made"); continue; }
		const Crowd::Ride R = PackCut::Standing(StandClip(*S), 0.3f);
		for (const int Man : { Whole, Bare, NoPack, Both }) C.SetRide(Man, R);
		C.Update(1.0);
		Riders += 4;
		std::string Line;
		for (int L = 0; L < LodCount; ++L)
		{
			const Lod Lv = static_cast<Lod>(L);
			const Mesh& Full = S->Meshes[L];
			const PackCut::Run Helmet = PackCut::HelmetRun(*S, Lv), Pack = PackCut::PackRun(*S, Lv);
			// (the whole man is the file's own mesh, untouched; the bareheaded one its helmet's cut alone, as before)
			const PackCut::Cut AsIs = PackCut::CutOf(Full, {}), HelmetOff = PackCut::CutOf(Full, { Helmet });
			const PackCut::Cut PackOff = PackCut::CutOf(Full, { Pack }), BothOff = PackCut::CutOf(Full, { Helmet, Pack });
			if (AsIs.Triangles != Full.Triangles || AsIs.Points != Full.Points) Fail("soldier_" + KV.first + ".json: the uncut reference is not the file's mesh");
			const struct { int Man; const PackCut::Cut* Want; const char* What; } Rows[] = {
				{ Whole, &AsIs, "whole" }, { Bare, &HelmetOff, "bareheaded" }, { NoPack, &PackOff, "no pack" }, { Both, &BothOff, "bareheaded and no pack" } };
			for (const auto& Row : Rows)
			{
				const std::string Why = PackCut::Compare(C, Row.Man, Whole, Lv, *Row.Want);
				if (!Why.empty()) Fail("soldier_" + KV.first + ".json at " + Levels[L] + ", " + Row.What + ": " + Why);
				if (Row.Want->Orphans > 0) Fail("soldier_" + KV.first + ".json at " + Levels[L] + ", " + Row.What + ": a kept triangle's point dropped");
			}
			// (nothing in its place: exactly the run's triangles fewer; the helmet and pack cut is the two runs' at once)
			if (static_cast<int>(PackOff.Triangles.size() / 3) != static_cast<int>(Full.Triangles.size() / 3) - S->PackCount[L]
				|| static_cast<int>(BothOff.Triangles.size() / 3) != static_cast<int>(Full.Triangles.size() / 3) - S->PackCount[L] - std::max(0, S->HelmetCount[L]))
				Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": a cut left more or fewer triangles than its runs");
			char Buf[160];
			std::snprintf(Buf, sizeof(Buf), "%s%s %d/%d tris, %d pts dropped (both %d/%d, %d)", L ? "; " : "", Levels[L], PackOff.LeftOut,
				static_cast<int>(Full.Triangles.size() / 3), PackOff.DroppedPoints, BothOff.LeftOut, static_cast<int>(Full.Triangles.size() / 3), BothOff.DroppedPoints);
			Line += Buf;
			// A gore twin's caps whole without his pack; its joined points that go with the pack (skipped, as the helmet's).
			if (!S->Cuts.empty())
			{
				int CapGone = 0, CapPoints = 0;
				for (const Soldier::Cut& Ct : S->Cuts)
				{
					for (const int* Cap : { Ct.Stump[L], Ct.Limb[L] })
					{
						for (int P = Cap[0]; P < Cap[0] + Cap[1]; ++P)
						{
							++CapPoints;
							CapGone += P < 0 || P >= Full.Points || PackOff.ToCompact[static_cast<size_t>(P)] < 0 ? 1 : 0;
						}
					}
				}
				int JoinedGone = 0;
				for (const uint32_t P : S->Joins[L].Point) JoinedGone += PackOff.ToCompact[P] < 0 ? 1 : 0;
				if (CapGone > 0 || CapPoints == 0) Fail("soldier_" + KV.first + ".json at " + Levels[L] + ": " + std::to_string(CapGone) + " of its " + std::to_string(CapPoints) + " cap points dropped with his pack");
				std::snprintf(Buf, sizeof(Buf), " [caps %d pts kept, joined %zu, %d with the pack]", CapPoints, S->Joins[L].Point.size(), JoinedGone);
				Line += Buf;
			}
		}
		if (!S->Cuts.empty()) ++Twins;
		std::printf("  soldier_%s.json: no pack: %s\n", KV.first.c_str(), Line.c_str());
	}
	std::printf("  %d files read, %d mark their pack (%d gore twins), at %d levels, %d of them ending where the helmet begins, %d reaching into it; near runs %d to %d "
		"triangles\n", Read, Packed, Twins, Levelled, EndsAtHelmet, ReachesHelmet, MostPackTris > 0 ? LeastPackTris : 0, MostPackTris);
	if (Packed > 0)
		std::printf("  helmet paint at his collar and shoulders in the near pack run (collar tabs, collar patches, a hood's hem: not kit; reported, not failed, the "
			"files' to mend): %s\n", Collars.empty() ? "none" : Collars.c_str());
	if (!Unpacked.empty())
	{
		std::string Names;
		for (const std::string& N : Unpacked) Names += (Names.empty() ? "" : ", ") + N;
		std::printf("  no pack_tris: %s\n", Names.c_str());
	}
	if (Packed > 0)
		std::printf("  %d riders checked: no pack, and bareheaded and no pack, their pack's run left out at every level with nothing in its place, only its own points dropped, "
			"every kept point as the whole man's; whole and bareheaded as before\n", Riders);

	// ---- A run not whole, out of the level's triangles or reaching into his helmet's, is left alone; a whole one read
	// (made up on a copy).
	if (!Paths.empty())
	{
		const std::string Base = ReadFile(Paths.begin()->second);
		std::string Stripped = Base;
		const size_t Key = Stripped.find("\"pack_tris\"");
		if (Key != std::string::npos) Stripped.replace(Key, 11, "\"pack_tris_was\""); // (the file's own run put by)
		struct Case
		{
			std::string Run;
			int First, Count;
		};
		std::vector<Case> Cases = { { "[12, 30]", 12, 30 }, { "[-1, 5]", 0, 0 }, { "[12, 99999999]", 0, 0 }, { "[12.5, 3]", 0, 0 }, { "[12, 0]", 0, 0 },
			{ "[12]", 0, 0 }, { "[12, 3, 4]", 0, 0 }, { "\"x\"", 0, 0 }, { "[12, -3]", 0, 0 } };
		// (against his helmet's run: ending where it begins, read; one triangle into it, inside it, or its last, none;
		// just after it, read)
		Soldier Plain;
		int Helmet = 0;
		if (Plain.Load(Stripped, Err) && Plain.HelmetCount[Near] > 30 && Plain.HelmetFirst[Near] > 30)
		{
			const int H = Plain.HelmetFirst[Near], N = Plain.HelmetCount[Near], T = static_cast<int>(Plain.Meshes[Near].Triangles.size() / 3);
			const auto Run = [](int F, int C) { return "[" + std::to_string(F) + ", " + std::to_string(C) + "]"; };
			Cases.push_back({ Run(H - 30, 30), H - 30, 30 });
			Cases.push_back({ Run(H - 30, 31), 0, 0 });
			Cases.push_back({ Run(H + 2, 5), 0, 0 });
			Cases.push_back({ Run(H + N - 1, 1), 0, 0 });
			Helmet = 4;
			if (H + N < T)
			{
				Cases.push_back({ Run(H + N, 1), H + N, 1 });
				++Helmet;
			}
		}
		else
			Fail("the made-up pack_tris' base file " + Paths.begin()->first + ": no helmet run at near to set runs against");
		int Right = 0;
		for (const Case& Cs : Cases)
		{
			std::string Made = Stripped;
			Made.insert(Made.find('{') + 1, "\"pack_tris\": {\"near\": " + Cs.Run + "}, ");
			Soldier S;
			if (!S.Load(Made, Err)) { Fail("a made-up pack_tris " + Cs.Run + ": " + Err); continue; }
			const bool bOk = S.PackFirst[Near] == Cs.First && S.PackCount[Near] == Cs.Count && S.PackCount[Mid] == 0 && S.PackCount[Far] == 0;
			if (!bOk) Fail("a made-up pack_tris " + Cs.Run + ": read " + std::to_string(S.PackFirst[Near]) + ", " + std::to_string(S.PackCount[Near]));
			Right += bOk ? 1 : 0;
		}
		std::printf("  %d of %zu made-up pack_tris read as they should be (a whole run within the triangles read; not whole, short, long, negative or past the end: none; "
			"%d set against his helmet's run in soldier_%s: ending where it begins or just after it, read; reaching into it, none)\n",
			Right, Cases.size(), Helmet, Paths.begin()->first.c_str());
	}

	// ---- Plain: a folder without pack_tris (today's Content) draws a rider with no pack as before, to the byte.
	if (Argc > 3)
	{
		const std::map<std::string, std::string> Plain = SoldierFiles(Argv[3]);
		int Same = 0, Marked = 0;
		for (const auto& KV : Plain)
		{
			auto S = std::make_shared<Soldier>();
			if (!S->Load(ReadFile(KV.second), Err)) { Fail("plain soldier_" + KV.first + ".json: " + Err); continue; }
			bool bAny = false;
			for (int L = 0; L < LodCount; ++L) bAny = bAny || S->PackCount[L] != 0 || S->PackFirst[L] != 0;
			if (bAny) { ++Marked; continue; }
			Crowd C;
			std::map<std::string, std::shared_ptr<Soldier>> One;
			One[KV.first] = S;
			if (!Ready(C, One, Armies, Err)) { Fail("plain soldier_" + KV.first + ".json: the crowd: " + Err); continue; }
			const std::string Army = ArmyOf(KV.first);
			const int Whole = C.AddRider(KV.first, Army, 0x5EEDull), NoPack = C.AddRider(KV.first, Army, 0x5EEDull, std::string(), false, true);
			const int Bare = C.AddRider(KV.first, Army, 0x5EEDull, std::string(), true), Both = C.AddRider(KV.first, Army, 0x5EEDull, std::string(), true, true);
			const Crowd::Ride R = PackCut::Standing(StandClip(*S), 0.3f);
			for (const int Man : { Whole, NoPack, Bare, Both }) C.SetRide(Man, R);
			C.Update(1.0);
			bool bSame = true;
			for (int L = 0; L < LodCount && bSame; ++L)
			{
				const Lod Lv = static_cast<Lod>(L);
				for (const auto& Pair : { std::make_pair(Whole, NoPack), std::make_pair(Bare, Both) })
				{
					const int P = C.PointsOf(Pair.first, Lv);
					if (P != C.PointsOf(Pair.second, Lv)) { bSame = false; break; }
					const Look& A = C.LookOf(Pair.first, Lv);
					const Look& B = C.LookOf(Pair.second, Lv);
					std::vector<float> Aa(static_cast<size_t>(P) * 3), Af(Aa.size()), Ba(Aa.size()), Bf(Aa.size());
					C.Skin(Pair.first, Lv, Aa.data(), Af.data());
					C.Skin(Pair.second, Lv, Ba.data(), Bf.data());
					bSame = bSame && A.Points == B.Points && A.Triangles == B.Triangles && A.Paint == B.Paint && A.Rgb == B.Rgb && Aa == Ba && Af == Bf;
				}
			}
			if (!bSame) Fail("plain soldier_" + KV.first + ".json: a rider with no pack drawn otherwise than before, without pack_tris");
			Same += bSame ? 1 : 0;
		}
		std::printf("  plain (%s): %zu files, %d marking a pack; %d with none: a rider with no pack the same man as before to the byte (helmeted and bareheaded, every level)\n",
			Argv[3], Plain.size(), Marked, Same);
	}

	// ---- Dress: how a man out of his machine is drawn (PilotDress, CrewDress), each army's, with the folder's files.
	{
		std::map<std::string, std::shared_ptr<Soldier>> Files;
		for (const char* Name : { "USA", "USA_tanker", "GER", "GER_panzer", "SOV", "UK", "POL", "ITA", "JAP", "FRA", "CHN" })
		{
			const auto Found = Paths.find(Name);
			if (Found == Paths.end()) continue;
			auto S = std::make_shared<Soldier>();
			if (S->Load(ReadFile(Found->second), Err)) Files[Name] = S;
		}
		Crowd C;
		if (!Ready(C, Files, Armies, Err)) Fail("dress: the crowd: " + Err);
		for (const char* Nation : { "USA", "GER", "SOV", "UK", "ITA", "JAP", "FRA", "POL", "CHN", "FIN" })
		{
			const Crowd::Dress P = C.PilotDress(Nation), W = C.CrewDress(Nation);
			const bool bCrew = (std::string(Nation) == "USA" && Files.count("USA_tanker")) || (std::string(Nation) == "GER" && Files.count("GER_panzer"));
			std::string Row;
			const std::string Infantry = C.RiderFileOf(Nation, Row);
			const bool bPilotOk = bCrew ? P.bTankers && !P.bBareHead && !P.bNoPack && P.File != Nation
				: !P.bTankers && P.bBareHead && P.bNoPack && P.File == Infantry && P.Row == Row;
			const bool bCrewOk = bCrew ? W.bTankers && !W.bBareHead && !W.bNoPack && W.File == P.File
				: !W.bTankers && !W.bBareHead && W.bNoPack && W.File == (Files.count(Nation) ? std::string(Nation) : std::string()) && W.Row.empty();
			if (!bPilotOk) Fail(std::string("PilotDress(") + Nation + "): " + P.File + (P.bBareHead ? ", bareheaded" : "") + (P.bNoPack ? ", no pack" : ""));
			if (!bCrewOk) Fail(std::string("CrewDress(") + Nation + "): " + W.File + (W.bNoPack ? ", no pack" : ""));
			const Soldier* Pf = C.SoldierFile(P.File);
			const Soldier* Wf = C.SoldierFile(W.File);
			// (how he is dressed said only of a man drawn: no file here, no helmet or pack to speak of)
			std::printf("  the %s pilot down: %s%s%s%s; a crewman out of her: %s%s%s\n", Nation, P.File.empty() ? "no file (none drawn)" : ("soldier_" + P.File).c_str(),
				Pf && P.bBareHead ? ", bareheaded" : "", Pf && P.bNoPack ? (Pf->PackCount[Near] > 0 ? ", no pack" : ", no pack asked (his file marks none)") : "",
				P.bTankers ? " (tankers')" : "", W.File.empty() ? "no file (none drawn)" : ("soldier_" + W.File).c_str(),
				W.bNoPack && Wf ? (Wf->PackCount[Near] > 0 ? ", no pack" : ", no pack asked (his file marks none)") : "", W.bTankers ? " (tankers')" : "");
		}
	}

	std::printf("%s\n", Fails ? "land_pack_test: FAILED" : "land_pack_test: all passed");
	return Fails ? 1 : 0;
}
