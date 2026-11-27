// The worn B-17 paint's test (the Battle rules card's "Worn B-17 paint", row flyB17Skin; Brian, 26 November 2026:
// "i want the new ones in the game", the look he approved on 25 November). Her shape stays Content/Models/B17Fine.json;
// the model thread's paint for that shape is laid over it from Content/Data/AirSkin/B17Fine.json (Battle/AirSkin, made
// by Tools/make_air_skin.py from the model thread's file). Usage:
//   air_b17_skin_test <Content/Models/B17Fine.json> <Content/Data/AirSkin/B17Fine.json> [<Source/TheaterCommand>]
//   S1 the overlay reads with her counts (143,899 near points, 15,534 far, 46 paints) and is made for these very bytes
//      (the base's size and FNV-1a 64 as the tool wrote them: a re-export of B17Fine.json without the tool fails here);
//      each fault refused: a key naming a month, a year, a scheme, a mark or a "from"; a blend of 101; a soot of -1;
//      45 colours; one point short; a repaint past the points; a fractional number; a base size or a check point
//      negative, fractional or past 2^53 (refused before any cast);
//   S2 the law: Battle/AirSkin's copy of the game's aircraft law (CampaignMiniatures.cpp's aircraft branch, quoted in S5)
//      gives every check point the tool wrote within one level a channel, unshaded and shaded;
//   S3 the earned colours' model (Battle/AirUnitColours) under the skin: its colours, blend and soot are the overlay's,
//      its far airscrew domes repainted, sizes kept; a model of other counts is left as it was; her four engines and
//      their spinners still found, near and far;
//   S4 the nose-art panel's under-colour: the overlay's panel_blend is the median blend of her near paint-0 points;
//   S5 (given the source folder) the Unreal side, read from its source (it cannot be built here): the row off unless
//      asked; the overlay read in one place, as the battle begins, only with the row on; the law's lines unchanged.
#include "Battle/AirSkin.h"
#include "Battle/AirUnitColours.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;

	void Check(bool bOk, const std::string& What, const std::string& Got = "")
	{
		std::printf("  %s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.empty() ? "" : ": ", Got.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream Buf;
		Buf << In.rdbuf();
		Out = Buf.str();
		return true;
	}

	int Count(const std::string& Text, const std::string& What)
	{
		int N = 0;
		for (size_t At = Text.find(What); At != std::string::npos; At = Text.find(What, At + 1)) ++N;
		return N;
	}

	std::string Hex(uint32_t V)
	{
		char B[8];
		std::snprintf(B, sizeof B, "%06X", static_cast<unsigned>(V & 0xFFFFFFu));
		return B;
	}

	int Off(uint32_t A, uint32_t B)
	{
		int Most = 0;
		for (int S = 0; S <= 16; S += 8)
		{
			Most = std::max(Most, std::abs(static_cast<int>((A >> S) & 0xFF) - static_cast<int>((B >> S) & 0xFF)));
		}
		return Most;
	}

	/** The overlay's text with its first From changed to To (an empty string where From is not in it). */
	std::string Swapped(const std::string& Text, const std::string& From, const std::string& To)
	{
		const size_t At = Text.find(From);
		if (At == std::string::npos) return std::string();
		return Text.substr(0, At) + To + Text.substr(At + From.size());
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: air_b17_skin_test <B17Fine.json> <AirSkin/B17Fine.json> [<Source/TheaterCommand>]\n");
		return 2;
	}
	std::string BaseText, SkinText;
	if (!ReadText(Argv[1], BaseText) || !ReadText(Argv[2], SkinText))
	{
		std::fprintf(stderr, "cannot read the files\n");
		return 2;
	}
	AirUnitColours::Model Base;
	std::string Why;
	if (!AirUnitColours::ReadModel(BaseText, "B17Fine", Base, Why))
	{
		std::fprintf(stderr, "B17Fine.json does not read: %s\n", Why.c_str());
		return 2;
	}
	const size_t NearN = Base.Near.Count(), FarN = Base.Far.Count(), PaintN = Base.Colours.size();
	std::printf("S1 the overlay reads, made for these bytes\n");
	Check(NearN == 143899 && FarN == 15534 && PaintN == 46, "her counts", std::to_string(NearN) + " / " + std::to_string(FarN) + " / " + std::to_string(PaintN));
	AirSkin::Overlay O;
	const bool bRead = AirSkin::Read(SkinText, NearN, FarN, PaintN, O, Why);
	Check(bRead, "the overlay reads", Why);
	if (!bRead) return 1;
	Check(O.Of == "B17Fine", "laid on B17Fine", O.Of);
	Check(O.BaseBytes == BaseText.size() && O.BaseFnv64 == AirSkin::Fnv64(BaseText), "made for B17Fine.json as it is (size and FNV-1a 64)");
	Check(O.BaseSha256.rfind("5e1f39002423", 0) == 0, "the base's sha256 is the game's", O.BaseSha256.substr(0, 16));
	Check(AirSkin::Fnv64("") == 0xCBF29CE484222325ull && AirSkin::Fnv64("a") == 0xAF63DC4C8601EC8Cull, "FNV-1a 64 of known strings");
	Check(Count(SkinText, "mark") == 0 && Count(SkinText, "month") == 0 && Count(SkinText, "year") == 0 && Count(SkinText, "USA") == 0, "no marks, months or years in it");
	{
		AirSkin::Overlay Again;
		std::string W;
		Check(AirSkin::Read(SkinText, NearN, FarN, PaintN, Again, W) && Again.Colours == O.Colours && Again.Near.Soot == O.Near.Soot && Again.Far.Repaint == O.Far.Repaint,
			"read twice, the same");
	}
	struct Fault
	{
		const char* What;
		std::string Text;
		size_t Near, Far, Paints;
	};
	const size_t Colours = SkinText.find("\"colours\":[");
	const std::vector<Fault> Faults = {
		{ "a month key", Swapped(SkinText, "\"skin_of\"", "\"month\":4,\"skin_of\""), NearN, FarN, PaintN },
		{ "a year key", Swapped(SkinText, "\"skin_of\"", "\"year\":1943,\"skin_of\""), NearN, FarN, PaintN },
		{ "a schemes key", Swapped(SkinText, "\"skin_of\"", "\"schemes\":[],\"skin_of\""), NearN, FarN, PaintN },
		{ "a marks key", Swapped(SkinText, "\"skin_of\"", "\"marks\":[],\"skin_of\""), NearN, FarN, PaintN },
		{ "a from key in a level", Swapped(SkinText, "\"blend\":[", "\"from\":\"1944-04\",\"blend\":["), NearN, FarN, PaintN },
		{ "a blend of 101", Swapped(SkinText, "\"blend\":[", "\"blend\":[101,") , NearN, FarN, PaintN },
		{ "a soot of -1", Swapped(SkinText, "\"soot\":[", "\"soot\":[-1,"), NearN, FarN, PaintN },
		{ "a fractional blend", Swapped(SkinText, "\"blend\":[", "\"blend\":[0.5,"), NearN, FarN, PaintN },
		{ "45 colours", Swapped(SkinText, "\"colours\":[\"" + SkinText.substr(Colours + 12, 6) + "\",", "\"colours\":["), NearN, FarN, PaintN },
		{ "near one point short (the counts asked one more)", SkinText, NearN + 1, FarN, PaintN },
		{ "far one point more (the counts asked one fewer)", SkinText, NearN, FarN - 1, PaintN },
		{ "a repaint past the points", Swapped(SkinText, "\"repaint\":[]", "\"repaint\":[99999999,1]"), NearN, FarN, PaintN },
		{ "a repaint past the paints", Swapped(SkinText, "\"repaint\":[]", "\"repaint\":[0,46]"), NearN, FarN, PaintN },
		{ "no shade", Swapped(SkinText, "\"shade\":[", "\"shadow\":["), NearN, FarN, PaintN },
		{ "no source sha", Swapped(SkinText, "\"source_sha256\"", "\"source\""), NearN, FarN, PaintN },
		{ "a base_bytes of -1", Swapped(SkinText, "\"base_bytes\":", "\"base_bytes\":-1,\"was\":"), NearN, FarN, PaintN },
		{ "a base_bytes of 1e300", Swapped(SkinText, "\"base_bytes\":", "\"base_bytes\":1e300,\"was\":"), NearN, FarN, PaintN },
		{ "a fractional base_bytes", Swapped(SkinText, "\"base_bytes\":", "\"base_bytes\":0.5,\"was\":"), NearN, FarN, PaintN },
		{ "a check point of -1", Swapped(SkinText, "\"point\":", "\"point\":-1,\"was\":"), NearN, FarN, PaintN },
		{ "a check point of 1e300", Swapped(SkinText, "\"point\":", "\"point\":1e300,\"was\":"), NearN, FarN, PaintN },
		{ "a fractional check point", Swapped(SkinText, "\"point\":", "\"point\":2.5,\"was\":"), NearN, FarN, PaintN },
	};
	for (const Fault& F : Faults)
	{
		AirSkin::Overlay No;
		std::string W;
		const bool bMade = !F.Text.empty();
		Check(bMade && !AirSkin::Read(F.Text, F.Near, F.Far, F.Paints, No, W), std::string("refused: ") + F.What, W);
	}

	std::printf("S2 the law gives the tool's check colours\n");
	Check(O.Checks.size() >= 20, "check points", std::to_string(O.Checks.size()));
	int Worst = 0, WorstShaded = 0;
	bool bPaints = true;
	for (const AirSkin::Check& C : O.Checks)
	{
		const AirSkin::Level& L = O.LevelOf(C.bFar);
		const AirUnitColours::Mesh& Me = C.bFar ? Base.Far : Base.Near;
		const int Paint = L.PaintAt(C.Point, Me.Paints[C.Point]);
		bPaints = bPaints && Paint == C.Paint;
		const uint32_t Plain = AirSkin::Law(O.Colours, Paint, L.Blend[C.Point], L.Soot[C.Point]);
		const uint32_t Shaded = AirSkin::Law(O.Colours, Paint, L.Blend[C.Point], L.Soot[C.Point], AirSkin::ShadeFactor(L.Shade[C.Point], 1.0));
		Worst = std::max(Worst, Off(Plain, C.Rgb));
		WorstShaded = std::max(WorstShaded, Off(Shaded, C.Shaded));
	}
	Check(bPaints, "each check point's paint is the base's or its repaint");
	Check(Worst <= 1, "unshaded within one level", std::to_string(Worst));
	Check(WorstShaded <= 1, "shaded within one level", std::to_string(WorstShaded));
	Check(AirSkin::Law(O.Colours, 0, 0, 0) == O.Colours[0] && AirSkin::Law(O.Colours, 0, 100, 0) == O.Colours[1] && AirSkin::Law(O.Colours, 5, 0, 0) == O.Colours[5],
		"the law at its ends is the paints' own colours");
	Check(AirSkin::ShadeFactor(100, 1.0) == 1.0 && AirSkin::ShadeFactor(12, 0.0) == 1.0 && std::fabs(AirSkin::ShadeFactor(12, 1.0) - 0.12) < 1e-12
			&& std::fabs(AirSkin::ShadeFactor(50, 0.5) - 0.75) < 1e-12 && AirSkin::ShadeFactor(40, 7.0) == AirSkin::ShadeFactor(40, 1.0) && AirSkin::ShadeFactor(40, -1.0) == 1.0,
		"the contact shade's factor: none at 100 or at strength 0, the whole shade at 1, halfway at 0.5, the strength clamped");
	{
		int Darker = 0;
		for (size_t K = 0; K < O.Near.Count(); ++K) Darker += O.Near.Shade[K] < 100 ? 1 : 0;
		const auto Least = *std::min_element(O.Near.Shade.begin(), O.Near.Shade.end());
		Check(Least >= 12 && Darker > 0, "her near shade 0.12 to 1 (the model thread's)", std::to_string(Darker) + " points shaded, the least " + std::to_string(Least));
	}

	std::printf("S3 the earned colours' model under the skin\n");
	{
		AirUnitColours::Model M = Base;
		Check(AirSkin::OntoModel(O, M), "laid on");
		bool bColours = M.Colours.size() == O.Colours.size();
		for (size_t K = 0; bColours && K < M.Colours.size(); ++K)
		{
			const AirUnitColours::Rgb& C = M.Colours[K];
			bColours = ((static_cast<uint32_t>(C.R) << 16) | (static_cast<uint32_t>(C.G) << 8) | C.B) == O.Colours[K];
		}
		Check(bColours, "its colours the overlay's");
		bool bNumbers = true;
		for (const bool bFar : { false, true })
		{
			const AirSkin::Level& L = O.LevelOf(bFar);
			const AirUnitColours::Mesh& Me = bFar ? M.Far : M.Near;
			const AirUnitColours::Mesh& Was = bFar ? Base.Far : Base.Near;
			bNumbers = bNumbers && Me.Count() == Was.Count() && Me.Blend.size() == L.Count() && Me.Soot.size() == L.Count();
			for (size_t K = 0; bNumbers && K < L.Count(); ++K)
			{
				bNumbers = Me.Blend[K] == L.Blend[K] && Me.Soot[K] == L.Soot[K] && Me.Paints[K] == L.PaintAt(K, Was.Paints[K]);
			}
		}
		Check(bNumbers, "its blend, soot and paints the overlay's, sizes kept");
		Check(O.Far.Repaint.size() == 160 && O.Near.Repaint.empty(), "160 far points repainted, none near (the model file of 26 November)",
			std::to_string(O.Far.Repaint.size()) + " / " + std::to_string(O.Near.Repaint.size()));
		const AirUnitColours::Found F = AirUnitColours::Find(M);
		int Spinners = 0;
		for (const AirUnitColours::Engine& E : F.Engines) Spinners += E.bSpinner ? 1 : 0;
		Check(F.Engines.size() == 4 && Spinners == 4, "her four engines and their spinners found under the skin", std::to_string(F.Engines.size()) + " engines, " + std::to_string(Spinners) + " spinners");
		std::vector<AirUnitColours::Where> NearAt, FarAt;
		AirUnitColours::Places(M, F, false, NearAt);
		AirUnitColours::Places(M, AirUnitColours::OnShape(F, true), true, FarAt);
		const long NearSpin = std::count(NearAt.begin(), NearAt.end(), AirUnitColours::Where::Spinner), FarSpin = std::count(FarAt.begin(), FarAt.end(), AirUnitColours::Where::Spinner);
		Check(NearSpin > 0 && FarSpin > 0, "spinner points near and far", std::to_string(NearSpin) + " / " + std::to_string(FarSpin));
		AirUnitColours::Model Other = Base;
		Other.Far.Paints.pop_back();
		Other.Far.Points.resize(Other.Far.Points.size() - 3);
		Other.Far.Facings.resize(Other.Far.Facings.size() - 3);
		const AirUnitColours::Model Kept = Other;
		Check(!AirSkin::OntoModel(O, Other) && Other.Colours == Kept.Colours && Other.Near.Soot == Kept.Near.Soot && Other.Far.Blend == Kept.Far.Blend, "a model of other counts left as it was");
	}

	std::printf("S4 the nose-art panel's under-colour\n");
	{
		std::vector<int> Zero;
		for (size_t K = 0; K < NearN; ++K)
		{
			if (O.Near.PaintAt(K, Base.Near.Paints[K]) == 0) Zero.push_back(O.Near.Blend[K]);
		}
		std::sort(Zero.begin(), Zero.end());
		const int Median = Zero.empty() ? 0 : Zero[Zero.size() / 2];
		Check(O.PanelBlend == Median, "panel_blend the median of her near paint-0 blend", std::to_string(O.PanelBlend) + " / " + std::to_string(Median));
		Check(AirSkin::PanelUnder(O) == AirSkin::Law(O.Colours, 0, Median, 0), "the panel fades into her coat there", Hex(AirSkin::PanelUnder(O)));
	}

	if (Argc >= 4)
	{
		std::printf("S5 the Unreal side, from its source\n");
		const std::string Src = Argv[3];
		std::string Miniatures;
		Check(ReadText(Src + "/Campaign/CampaignMiniatures.cpp", Miniatures), "CampaignMiniatures.cpp read");
		// (the law's lines, as they stand on 3d6cd8fb2: the skin changes its inputs, never its arithmetic)
		Check(Count(Miniatures, "Shade = FMath::Lerp(PlanePaints[0], PlanePaints[1], FMath::Min(Raw->Blend[K], 100) / 100.0f);") == 1
			&& Count(Miniatures, "Shade = FMath::Lerp(Shade, PlanePaints[11], 0.72f * FMath::Min(Raw->Soot[K], 100) / 100.0f);") == 1,
			"the law's blend and soot lines unchanged");
		std::string Battle, BattleH, Rules, Colours2, Nose;
		Check(ReadText(Src + "/Battle/AirBattle.cpp", Battle) && ReadText(Src + "/Battle/AirBattle.h", BattleH) && ReadText(Src + "/Battle/LandLive.cpp", Rules)
			&& ReadText(Src + "/Battle/AirBattleUnitColours.cpp", Colours2) && ReadText(Src + "/Battle/AirBattleNoseArt.cpp", Nose), "the air battle's source read");
		// (the row: once, off unless asked, its launch words, passed over by the land rules as every "fly" row is)
		Check(Count(Rules, "{ TEXT(\"flyB17Skin\"), TEXT(\"Worn B-17 paint\"), ") == 1 && Count(Rules, "TEXT(\"TheaterAirNoB17Skin\"), false, TEXT(\"TheaterAirB17Skin\") }") == 1
				&& Count(Rules, "RuleKey.StartsWith(TEXT(\"fly\")") == 1,
			"the row: off unless asked, -TheaterAirB17Skin / -TheaterAirNoB17Skin, never sent to the land rules");
		// (read in one place, as the battle begins, after the finer B-17's own row; nothing read or set with it off)
		Check(Count(Battle, "\tB17SkinRead();") == 2 && Count(Battle, "\tB17FineRead();") == 2 && Count(BattleH, "void B17SkinRead();") == 1, "B17SkinRead called where B17FineRead is, both battle starts");
		size_t AfterFine = 0;
		bool bOrder = true;
		for (size_t At = Battle.find("\tB17FineRead();"); At != std::string::npos; At = Battle.find("\tB17FineRead();", At + 1))
		{
			const size_t Skin = Battle.find("\tB17SkinRead();", At);
			bOrder = bOrder && Skin != std::string::npos && Skin > At && Skin - At < 200 && Skin > AfterFine;
			AfterFine = At;
		}
		Check(bOrder, "each B17SkinRead right after its B17FineRead");
		const size_t Body = Battle.find("void AAirBattle::B17SkinRead()");
		const size_t BodyEnd = Body == std::string::npos ? std::string::npos : Battle.find("\n}\n", Body);
		const std::string Read = Body == std::string::npos || BodyEnd == std::string::npos ? std::string() : Battle.substr(Body, BodyEnd - Body);
		const size_t Unset = Read.find("AirSkin::Set(nullptr);"), Rule = Read.find("FLandLive::RuleNow(TEXT(\"flyB17Skin\"))"), File = Read.find("FFileHelper::LoadFileToArray("),
					 Said = Read.find("UE_LOG");
		Check(!Read.empty() && Unset != std::string::npos && Rule != std::string::npos && File != std::string::npos && Unset < Rule && Rule < File && Rule < Said,
			"off: unset first, then the row asked before any file is read or any line said");
		Check(Count(Battle, "FLandLive::RuleNow(TEXT(\"flyB17Skin\"))") == 1 && Count(Battle + Colours2 + Nose + Miniatures, "AirSkin::Read(") == 1
				&& Count(Battle, "AirSkin::Set(") == 2 && Count(Colours2 + Nose + Miniatures, "AirSkin::Set(") == 0,
			"the overlay read and set in B17SkinRead alone");
		// (every other reader asks AirSkin only once it holds an overlay; with the row off every key is what it was)
		const std::string All = Battle + Colours2 + Nose + Miniatures;
		int Asked = 0, Guarded = 0;
		for (size_t At = All.find("AirSkin::For("); At != std::string::npos; At = All.find("AirSkin::For(", At + 1))
		{
			++Asked;
			const size_t LineAt = All.rfind('\n', At);
			const std::string Line = All.substr(LineAt + 1, At - LineAt - 1);
			Guarded += Line.find("AirSkin::Active() ?") != std::string::npos ? 1 : 0;
		}
		// (five since the paint sheen, row flyB17Sheen: B17SheenRead asks for her overlay too, behind AirSkin::Active() as
		// every other)
		Check(Asked == 5 && Guarded == 4 && Count(Battle, "\tif (AirSkin::Active())\n\t{\n\t\tFString SkinKind = Name, SkinWhose;") == 1,
			"AirSkin::For asked five times, each behind AirSkin::Active()", std::to_string(Asked) + " asked, " + std::to_string(Guarded + 1) + " guarded");
		Check(Count(Battle, "\tif (bSkin)\n\t{\n\t\tShapeKey += TEXT(\"|skin\");\n\t}") == 1 && Count(Miniatures, "+ (WornSkin ? TEXT(\"|skin\") : TEXT(\"\"))") == 1,
			"her shape and her read kept apart under the skin ('|skin')");
		Check(Count(Miniatures, "const int32 Livery = Raw->bAircraft && !Raw->bSkinned && !UnitTable::OneLook() ? SchemeStanding(Raw->SchemeFrom) : 0;") == 1
				&& Count(Miniatures, "Skinned->bSkinned = true;") == 1,
			"never by date under the skin (her schemes stand aside)");
		// (the release lead's model warm, 27 November: a model's file may be parsed on a worker (BuildRaw). Her skin is laid
		// on the game thread alone, in Sculpted, over a copy of her plain read kept beside it; the worker never asks AirSkin.)
		const size_t SculptedAt = Miniatures.find("bool FMiniatureMesh::Sculpted(");
		const size_t BuildAt = Miniatures.find("TSharedPtr<FRaw> BuildRaw(");
		Check(SculptedAt != std::string::npos && BuildAt != std::string::npos && BuildAt < SculptedAt && Miniatures.find("AirSkin::") > SculptedAt
				&& Count(Miniatures, "const TSharedPtr<FRaw> Skinned = MakeShared<FRaw>(*Raw);") == 1 && Count(Miniatures, "Read.Add(SkinWhich, Skinned);") == 1,
			"her skin laid on the game thread (Sculpted) over a copy of her plain read; the warm worker's BuildRaw never asks AirSkin");
		const size_t Strength = Read.find("TEXT(\"TheaterAirB17SkinShade=\")");
		Check(Strength != std::string::npos && Strength > Rule && Count(Battle, "TEXT(\"TheaterAirB17SkinShade=\")") == 1
				&& Count(Miniatures, "if (Raw->bSkinned && Raw->Worn.IsValidIndex(K) && Raw->Worn[K] < 1.0f)") == 1
				&& Count(Miniatures, "AirSkin::ShadeFactor(WornLevel->Shade[static_cast<size_t>(SkinK)], WornSkin->ShadeStrength)") == 1,
			"the contact shade: multiplied in under the skin alone, its strength asked only with the row on");
		Check(Count(Colours2, "AirSkin::OntoModel(*WornSkin, Known.Model)") == 1 && Count(Nose, "ColourOf(WornSkin ? AirSkin::PanelUnder(*WornSkin) : Panel->Skin)") == 1,
			"the earned colours and the nose-art panel take the overlay");
	}
	std::printf("air_b17_skin_test: %s\n", Bad ? "FAILED" : "ok");
	return Bad ? 1 : 0;
}
