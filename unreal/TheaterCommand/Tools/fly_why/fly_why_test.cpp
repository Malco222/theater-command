// The raid card says why FLY IT YOURSELF is not offered: the cloud test of a mend in his game (the design director's
// known-faults line of 27 November 2026 23:13Z, corrected 23:16Z). Run by Tools/fly_why/run.sh.
//
// The fault: CanFlyLive asked the rules (TC.airLook) whether the raid as picked has a battle in the air and, where they
// said no, only wrote their why to the log; the card dropped FLY IT YOURSELF and said nothing. The mend keeps that why
// with the refusal (FlyOfferWhy) and the card draws it above its buttons (FlyNotOfferedWhy). The controller and the
// card need the engine, so this test reads their source, as the pre-check and the Mac build it, and holds:
//  1. The card's words (the UI director's yes of 23:28Z): the general sentence is FlyLive's own notice, one string read
//     by both; the rules' why follows the first sentence and is never copied into the 3D game.
//  2. Every refusal keeps a line, and every offer or reset clears it: each FlyOffer = 0 is followed by the line, each
//     FlyOffer = -1 (and the weighing's start) by its reset, and the offered path is as before.
//  3. CanFlyLive's log lines are as before, letter for letter and no more of them, and the rules are asked no more often
//     (one TC.airLook call, in the weighing, with its arguments as before; FlyNotOfferedWhy, asked twice a frame by the
//     card, is held whole and asks the rules nothing).
//  4. The card draws the line only where FlyNotOfferedWhy has one, collapsed elsewhere; FLY IT YOURSELF, FLY THE RAID
//     and ANOTHER KIND OF RAID are drawn as before.
//  5. The rules' refusals (src/battle/air/ticket.ts airTicket) are each a whole sentence, and the test prints what the
//     card says for each it can show (two never reach the card: a night raid is never weighed by airLook, and FLY THE
//     RAID is shut where nobody is at the controls).

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0;
	int Checks = 0;

	void Check(bool bOk, const std::string& What, int Line)
	{
		++Checks;
		if (!bOk)
		{
			++Failures;
			std::printf("FAILED (line %d): %s\n", Line, What.c_str());
		}
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Text;
		Text << In.rdbuf();
		return Text.str();
	}

	size_t Count(const std::string& Text, const std::string& Piece)
	{
		size_t N = 0;
		for (size_t At = Text.find(Piece); At != std::string::npos; At = Text.find(Piece, At + Piece.size()))
		{
			++N;
		}
		return N;
	}

	/** The text from the first Start to the first End after it (empty where either is missing). */
	std::string Between(const std::string& Text, const std::string& Start, const std::string& End)
	{
		const size_t From = Text.find(Start);
		if (From == std::string::npos)
		{
			return std::string();
		}
		const size_t To = Text.find(End, From + Start.size());
		return To == std::string::npos ? std::string() : Text.substr(From, To - From);
	}

	/** The first non-blank line after each place Piece is found. */
	std::vector<std::string> LineAfterEach(const std::string& Text, const std::string& Piece)
	{
		std::vector<std::string> Out;
		for (size_t At = Text.find(Piece); At != std::string::npos; At = Text.find(Piece, At + Piece.size()))
		{
			size_t Next = Text.find('\n', At);
			std::string Line;
			while (Next != std::string::npos && Line.find_first_not_of(" \t\r") == std::string::npos)
			{
				const size_t End = Text.find('\n', Next + 1);
				Line = Text.substr(Next + 1, End == std::string::npos ? std::string::npos : End - Next - 1);
				Next = End;
			}
			const size_t Lead = Line.find_first_not_of(" \t");
			Out.push_back(Lead == std::string::npos ? std::string() : Line.substr(Lead));
		}
		return Out;
	}
}

#define CHECK(Condition) Check((Condition), #Condition, __LINE__)
#define CHECK_SAID(Condition, What) Check((Condition), (What), __LINE__)

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: fly_why_test <Source/TheaterCommand> <rules src/battle/air/ticket.ts>\n");
		return 2;
	}
	const std::string Src = Argv[1];
	const std::string Pc = ReadAll(Src + "/Campaign/CampaignPlayerController.cpp");
	const std::string Ph = ReadAll(Src + "/Campaign/CampaignPlayerController.h");
	const std::string Hud = ReadAll(Src + "/Campaign/CampaignHUD.cpp");
	const std::string Ticket = ReadAll(Argv[2]);
	CHECK(!Pc.empty() && !Ph.empty() && !Hud.empty() && !Ticket.empty());

	// 1. The card's words.
	const std::string General = "This raid cannot be flown in the air battle: FLY THE RAID sends your crews.";
	const std::string Lead = "This raid cannot be flown in the air battle. ";
	const std::string Line = Between(Pc, "FString ACampaignPlayerController::FlyNotOfferedLine(const FString& RulesWhy)", "\n}\n");
	CHECK(!Line.empty());
	CHECK(Count(Line, "const FString Why = RulesWhy.TrimStartAndEnd();") == 1);
	CHECK(Count(Line, "? FString(TEXT(\"" + General + "\"))") == 1);
	CHECK(Count(Line, ": FString(TEXT(\"" + Lead + "\")) + Why;") == 1);
	// (the UI director, 23:28Z: the general sentence is FlyLive's own notice, read from one place: the line's one string,
	// which both of FlyLive's notices and the card read; no second copy in the controller or on the card)
	CHECK(Count(Pc, General) == 1);
	CHECK(Count(Hud, General) == 0);
	CHECK(Count(Pc, "Notify(FlyNotOfferedLine(FString()), 6.0);") == 2);
	CHECK(Count(Hud, Lead) == 0);
	CHECK(Count(Ph, "FString FlyNotOfferedWhy() const;") == 1);
	CHECK(Count(Ph, "static FString FlyNotOfferedLine(const FString& RulesWhy);") == 1);
	CHECK(Count(Ph, "mutable FString FlyOfferWhy;") == 1);

	// 2. Every refusal keeps a line; every offer and reset clears it.
	for (const std::string& After : LineAfterEach(Pc, "FlyOffer = 0;"))
	{
		CHECK_SAID(After == "FlyOfferWhy.Reset();" || After == "FlyOfferWhy = FlyNotOfferedLine(FString());", "after FlyOffer = 0: " + After);
	}
	CHECK(Count(Pc, "FlyOffer = 0;") == 3);
	for (const std::string& After : LineAfterEach(Pc, "FlyOffer = -1;"))
	{
		CHECK_SAID(After == "FlyOfferWhy.Reset();", "after FlyOffer = -1: " + After);
	}
	CHECK(Count(Pc, "FlyOffer = -1;") == 1);
	const std::string Can = Between(Pc, "bool ACampaignPlayerController::CanFlyLive() const", "\n}\n");
	CHECK(!Can.empty());
	// (the weighing starts afresh for each pick, and each of its two refusals keeps its line)
	CHECK(Count(Can, "FlyOffer = 0;\n\t\tFlyOfferWhy.Reset();") == 1);
	CHECK(Count(Can, "(TC.airKinds); FLY THE RAID only\"), *FlyOfferAct);\n\t\t\tFlyOfferWhy = FlyNotOfferedLine(FString());\n\t\t\treturn false;") == 1);
	// (only the rules' own why reaches the card, never an error of the call: RulesWhy starts empty and is set in one place)
	CHECK(Count(Can, "\t\t\tFString RulesWhy;\n\t\t\tif (LookAnswer.IsValid() && LookAnswer->TryGetStringField(TEXT(\"why\"), Why))\n\t\t\t{\n\t\t\t\tRulesWhy = Why;\n\t\t\t}") == 1);
	CHECK(Count(Pc, "RulesWhy =") == 1);
	CHECK(Count(Pc, "FString RulesWhy") == 1);
	CHECK(Count(Can, "FlyOfferWhy = FlyNotOfferedLine(RulesWhy);") == 1);
	// (the offered path as before: FlyOffer 1, its log line, the answer)
	CHECK(Count(Can, "\t\tFlyOffer = 1;\n\t\tUE_LOG(LogTheater, Display, TEXT(\"Campaign: FLY IT YOURSELF offered for %s\"), *FlyOfferAct);\n\t}\n\treturn FlyOffer > 0;") == 1);
	CHECK(Count(Pc, "FlyOfferWhy =") == 4);
	CHECK(Count(Pc, "FlyOfferWhy.Reset();") == 2);

	// 3. The log as before, letter for letter and no new line, and no more calls to the rules.
	CHECK(Count(Can, "UE_LOG(LogTheater, Display, TEXT(\"Campaign: FLY IT YOURSELF not offered for %s: the rules could not be told that a raid of this kind is flown (TC.airKinds); FLY THE RAID only\"), *FlyOfferAct);") == 1);
	CHECK(Count(Can, "UE_LOG(LogTheater, Display, TEXT(\"Campaign: FLY IT YOURSELF not offered for %s: no battle in the air for it (%s); FLY THE RAID only\"), *FlyOfferAct, *Why);") == 1);
	CHECK(Count(Can, "UE_LOG(LogTheater, Display, TEXT(\"Campaign: FLY IT YOURSELF offered for %s\"), *FlyOfferAct);") == 1);
	CHECK(Count(Can, "UE_LOG(") == 3);
	CHECK(Count(Pc, "the raid card says why") == 0);
	CHECK(Count(Pc, "CallBattle(TEXT(\"airLook\")") == 1);
	CHECK(Count(Can, "Session->CallBattle(TEXT(\"airLook\"), FString::Printf(TEXT(\"[\\\"%s\\\",%d,[%s]]\"), *Kind, Target, *IdsText), LookText, LookError)") == 1);
	// (FlyNotOfferedWhy whole, as the card asks it twice a frame: CanFlyLive's weighing first, then CanFlyLive's own gates
	// letter for letter, so the card never explains a raid he does not fly himself, and no call of its own to the rules)
	const std::string Gate = "if ((!AAirBattle::CampaignLiveOn() && !AAirBattle::CampaignLiveOn(TEXT(\"ground\"))) || bScripted || !bPageOpen || !Page.Card.bValid || !Page.Card.Fly.bOk || LiveRaid.IsValid() || LiveBattle.IsValid())";
	const std::string Read = "if (!ReadFlyAct(Page.Card.Fly.Act, Kind, Target, Ids) || !IsFlownKind(Kind))";
	CHECK(Count(Can, Gate) == 1);
	CHECK(Count(Can, Read) == 1);
	const std::string Why = Between(Pc, "FString ACampaignPlayerController::FlyNotOfferedWhy() const", "\n}\n");
	const std::string WhyWhole = "FString ACampaignPlayerController::FlyNotOfferedWhy() const\n{\n"
		"\t// (weighed as the button is, once a pick: nothing to say where it is offered, or before CanFlyLive has weighed this pick)\n"
		"\tif (CanFlyLive() || FlyOffer != 0 || FlyOfferAct != Page.Card.Fly.Act || FlyOfferWhy.IsEmpty())\n\t{\n\t\treturn FString();\n\t}\n"
		"\t// (the gates CanFlyLive passes before it weighs: with the switch off, a scripted run, no card, a raid FLY THE RAID cannot\n"
		"\t// send, a battle already open, or a raid he does not fly himself, the card says nothing new)\n"
		"\t" + Gate + "\n\t{\n\t\treturn FString();\n\t}\n"
		"\tFString Kind;\n\tint32 Target = INDEX_NONE;\n\tTArray<int32> Ids;\n"
		"\t" + Read + "\n\t{\n\t\treturn FString();\n\t}\n"
		"\treturn FlyOfferWhy;";
	CHECK(Why == WhyWhole);

	// 4. The card.
	const std::string Foot = Between(Hud, "Deal(TEXT(\"COULD JOIN\"), Could, TEXT(\"No other group can reach it this month.\"));", "/** (Release 2, THE HOME FRONT) Whether the room open is the home front");
	CHECK(!Foot.empty());
	const size_t WhyAt = Foot.find("Controller->FlyNotOfferedWhy()");
	const size_t ButtonsAt = Foot.find("Aimed(TEXT(\"otherraid\")");
	CHECK(WhyAt != std::string::npos && ButtonsAt != std::string::npos && WhyAt < ButtonsAt);
	CHECK(Count(Hud, "FlyNotOfferedWhy()") == 2);
	// (the line's slot whole: in the foot by the buttons, Regular 11, Muted, wrapping, right-aligned, and nothing else)
	CHECK(Count(Foot, "\t\t\t(PageFoot.IsValid() ? PageFoot : PageBody)->AddSlot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 8.0f, 12.0f, 0.0f)\n"
		"\t\t\t[\n\t\t\t\tSNew(STextBlock)\n"
		"\t\t\t\t.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->FlyNotOfferedWhy().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })\n"
		"\t\t\t\t.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() ? Controller->FlyNotOfferedWhy() : FString()); })\n"
		"\t\t\t\t.Font(Font(TEXT(\"Regular\"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).Justification(ETextJustify::Right)\n"
		"\t\t\t];\n") == 1);
	CHECK(Count(Foot, ".Visibility_Lambda([this]() { return Controller.IsValid() && Controller->CanFlyLive() ? EVisibility::Visible : EVisibility::Collapsed; })") == 1);
	CHECK(Count(Foot, "SNew(STextBlock).Text(FText::FromString(TEXT(\"FLY IT YOURSELF\"))).Font(Font(TEXT(\"Bold\"), 11, 140)).ColorAndOpacity(BrightBrass)") == 1);
	CHECK(Count(Foot, ".IsEnabled(Card.Fly.bOk)") == 1);
	// (FLY THE RAID whole: its own check, its click and its words as before)
	CHECK(Count(Foot, "Aimed(TEXT(\"fly\"), SNew(SButton).ButtonStyle(Card.bBomb ? &PlainButton : &BrassStyle).ContentPadding(FMargin(Card.bBomb ? 20.0f : 38.0f, Card.bBomb ? 10.0f : 11.0f)).IsFocusable(false).IsEnabled(Card.Fly.bOk)\n"
		"\t\t\t\t\t.ToolTipText(FText::FromString(Card.Fly.Tip))\n"
		"\t\t\t\t\t.OnClicked_Lambda(Go(Card.Fly.Act))\n"
		"\t\t\t\t\t[\n"
		"\t\t\t\t\t\tSNew(STextBlock).Text(FText::FromString(Card.Fly.Label.IsEmpty() ? FString(TEXT(\"FLY THE RAID\")) : Card.Fly.Label)).Font(Font(TEXT(\"Bold\"), Card.bBomb ? 11 : 12, Card.bBomb ? 200 : 240)).ColorAndOpacity(!Card.Fly.bOk ? Faint : Card.bBomb ? Cream : Ink)\n"
		"\t\t\t\t\t])") == 1);
	CHECK(Count(Foot, "TEXT(\"ANOTHER KIND OF RAID\")") == 1);

	// 5. The rules' refusals: whole sentences, and what the card says for each it can show.
	const std::string Air = Between(Ticket, "export function airTicket(", "\n}\n");
	CHECK(!Air.empty());
	const std::regex Said("why: '([^']*)'");
	int Whys = 0;
	for (std::sregex_iterator It(Air.begin(), Air.end(), Said), End; It != End; ++It)
	{
		const std::string W = (*It)[1].str();
		++Whys;
		CHECK_SAID(!W.empty() && W.back() == '.' && W.front() >= 'A' && W.front() <= 'Z', "a whole sentence: " + W);
		// (the UI director: airLook's own sentence, never a copy: no rules' why is written into the controller or the card)
		CHECK_SAID(Count(Pc, W) == 0 && Count(Hud, W) == 0, "no copy of the rules' why in the 3D game: " + W);
		// (never on the card: IsFlownKind never sends a night raid to airLook, and FLY THE RAID is shut where nobody is at
		// the controls, the UI director's condition 2)
		const bool bNever = W == "Nobody is at the controls." || W.rfind("Only day raids", 0) == 0;
		if (bNever)
		{
			std::printf("  never on the card: \"%s\"\n", W.c_str());
		}
		else
		{
			std::printf("  the card: \"%s%s\"\n", Lead.c_str(), W.c_str());
		}
	}
	CHECK(Whys >= 3);
	std::printf("  the card, with no why from the rules: \"%s\"\n", General.c_str());

	std::printf("%s: %d checks, %d failed\n", Failures == 0 ? "PASS" : "FAIL", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
