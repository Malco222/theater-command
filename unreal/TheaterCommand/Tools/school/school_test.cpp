// The battle schools' button on a corps' card (release 2), checked in the cloud: Campaign/BattleSchool, the game's own
// code, built without the engine. With no argument it runs its own cases; given the lines probe.mjs wrote from a rules
// bundle run as the game runs it, it also checks the button every one of those corps cards would show, and what each
// press of BRING BACK there does to the army in hand.
// Usage: school_test [cards.tsv]
#include "Campaign/BattleSchool.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0;
	int Checks = 0;

	void Expect(bool bHolds, const std::string& What)
	{
		++Checks;
		if (!bHolds)
		{
			++Failures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	BattleSchool::FCorpsFacts Fit()
	{
		BattleSchool::FCorpsFacts Facts;
		Facts.bKnown = true;
		Facts.bOwn = true;
		Facts.bGrand = true;
		Facts.bHisMonth = true;
		Facts.To = "Veteran";
		return Facts;
	}

	void OwnCases()
	{
		using BattleSchool::EFace;
		// Release 1's rules: no 'school' in the view, so nothing shows, whatever else is true.
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bKnown = false;
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "release 1 (no 'school' in the view) shows no button");
			Facts.bAtSchool = true;
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "release 1 shows no button even for a corps said to be at school");
			Expect(BattleSchool::ButtonFor(Facts).Label.empty(), "no button, no label");
		}
		// Outside the grand campaign, on an ally's corps, on fortress troops, outside his month: nothing.
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bGrand = false;
			Facts.Why = "The battle schools are open only in the grand campaign.";
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "a history scenario shows no button");
		}
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bOwn = false;
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "an ally's corps shows no button");
		}
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bFort = true;
			Facts.Why = "A fortress stays where it is.";
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "fortress troops show no button");
		}
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bHisMonth = false;
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "outside the player's month no button shows");
			Facts.bAtSchool = true;
			Expect(BattleSchool::ButtonFor(Facts).Face == EFace::None, "outside the player's month no BRING BACK shows");
		}
		// Fit to go: SEND TO SCHOOL, lit, and its tip names the grade it comes back.
		{
			const BattleSchool::FButton Button = BattleSchool::ButtonFor(Fit());
			Expect(Button.Face == EFace::Send && Button.bLit, "a corps fit to go has SEND TO SCHOOL lit");
			Expect(Button.Label == "SEND TO SCHOOL", "its label is SEND TO SCHOOL");
			Expect(Button.Tip.find("comes back veteran when your next month opens") != std::string::npos, "its tip names the grade, as a word in a sentence: " + Button.Tip);
			Expect(Button.Tip.find("no orders") != std::string::npos, "its tip says it takes no orders there");
		}
		// Not fit: SEND TO SCHOOL unlit, and its tip is the rules' reason, word for word.
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.Why = "It has marched this month already. A term at the schools is a whole quiet month.";
			const BattleSchool::FButton Button = BattleSchool::ButtonFor(Facts);
			Expect(Button.Face == EFace::Send && !Button.bLit, "a corps that cannot go has SEND TO SCHOOL unlit");
			Expect(Button.Tip == Facts.Why, "its tip is the rules' reason, word for word");
		}
		// At school: BRING BACK, lit (the rules let it be called back), and it says what is given up.
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.bAtSchool = true;
			Facts.To = "Seasoned";
			const BattleSchool::FButton Button = BattleSchool::ButtonFor(Facts);
			Expect(Button.Face == EFace::Back && Button.bLit, "a corps at school has BRING BACK lit");
			Expect(Button.Label == "BRING BACK", "its label is BRING BACK");
			Expect(Button.Tip.find("comes back seasoned") != std::string::npos && Button.Tip.find("no term") != std::string::npos, "its tip says what it would come back as, and what calling it back costs: " + Button.Tip);
		}
		// No grade given: the tips still read as sentences.
		{
			BattleSchool::FCorpsFacts Facts = Fit();
			Facts.To.clear();
			Expect(BattleSchool::ButtonFor(Facts).Tip.find("a grade better") != std::string::npos, "with no grade the send tip says a grade better");
			Facts.bAtSchool = true;
			Expect(BattleSchool::ButtonFor(Facts).Tip.find("a grade better") != std::string::npos, "with no grade the back tip says a grade better");
			Expect(BattleSchool::ButtonFor(Facts).Tip.find("  ") == std::string::npos, "no doubled spaces");
		}
		// BRING BACK and the army in hand: it stays on one side of a town's walls (the code checker's rule of 17 November).
		{
			using BattleSchool::EBackInHand;
			BattleSchool::FHandFacts Hand;
			Hand.bNoHand = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::PickUp, "nothing in hand (a whole army at the schools, only looked at): the army is picked up");
			Hand.bBackInTown = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::PickUp, "nothing in hand, a garrison called back: the army is picked up (PickUpArmyIn keeps the sides apart)");
			Hand = BattleSchool::FHandFacts();
			Hand.bHandInField = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::Untouched, "the army in hand stands elsewhere: it is left as it is");
			Hand.bHandHere = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::Join, "a field corps called back to a field hand comes along");
			Hand.bHandInField = false;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::Join, "a field corps called back to an empty hand comes along alone");
			Hand.bHandInTown = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::PickUp, "a field corps called back while only the garrison is in hand: the field army is picked up instead (the reviewer's case)");
			Hand.bHandInField = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::PickUp, "a hand somehow mixed is put right by a field corps called back");
			Hand.bBackInTown = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::KeepOut, "a garrison called back to a mixed hand stays out");
			Hand.bHandInTown = false;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::KeepOut, "a garrison called back while the field army is in hand stays out");
			Hand.bHandInField = false;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::Join, "a garrison called back to an empty hand comes along");
			Hand.bHandInTown = true;
			Expect(BattleSchool::HandOnBack(Hand) == EBackInHand::Join, "a garrison called back to the garrison in hand comes along");
		}
		Expect(BattleSchool::GradeInSentence("Veteran") == "veteran", "Veteran in a sentence");
		Expect(BattleSchool::GradeInSentence("") == "", "an empty grade stays empty");
		Expect(BattleSchool::GradeInSentence("\xC3\x89lite") == "\xC3\x89lite", "letters outside plain ASCII are left alone");
	}

	/** Splits at each Mark, keeping empty fields (a corps with no reason has an empty last field). */
	std::vector<std::string> SplitAt(const std::string& Text, char Mark)
	{
		std::vector<std::string> Parts;
		std::string Part;
		for (const char Letter : Text)
		{
			if (Letter == Mark)
			{
				Parts.push_back(Part);
				Part.clear();
			}
			else
			{
				Part += Letter;
			}
		}
		Parts.push_back(Part);
		return Parts;
	}

	/** A list of ids "68,178": none when it is empty. */
	std::vector<std::string> ListOf(const std::string& Text)
	{
		return Text.empty() ? std::vector<std::string>() : SplitAt(Text, ',');
	}

	/**
	 * A 'hand' line from probe.mjs: hand, rules, scenario, no hand, hand here, back in town, hand in town, hand in field,
	 * the corps called back, the ids in hand, and every corps of his in the province after the press as id:in town:takes
	 * orders. HandOnBack is run on it, and the hand the controller is left with is worked out as SchoolCorps does it
	 * (PickUpArmyIn: the corps taking orders in the field, or the town's if there are none; RefreshOrders: only corps
	 * taking orders stay picked), then checked to stand on one side of the walls.
	 */
	void HandLine(const std::vector<std::string>& Part, int& Hands)
	{
		using BattleSchool::EBackInHand;
		Expect(Part.size() == 11, "a hand line has eleven fields");
		if (Part.size() != 11)
		{
			return;
		}
		++Hands;
		const std::string& Scenario = Part[2];
		BattleSchool::FHandFacts Facts;
		Facts.bNoHand = Part[3] == "1";
		Facts.bHandHere = Part[4] == "1";
		Facts.bBackInTown = Part[5] == "1";
		Facts.bHandInTown = Part[6] == "1";
		Facts.bHandInField = Part[7] == "1";
		const std::string& Back = Part[8];
		const std::vector<std::string> Before = ListOf(Part[9]);
		struct FHere { std::string Id; bool bTown = false; bool bOrders = false; };
		std::vector<FHere> After;
		for (const std::string& Each : ListOf(Part[10]))
		{
			const std::vector<std::string> Bits = SplitAt(Each, ':');
			Expect(Bits.size() == 3, Scenario + ": a corps in the province reads id:town:orders: " + Each);
			if (Bits.size() == 3)
			{
				After.push_back({ Bits[0], Bits[1] == "1", Bits[2] == "1" });
			}
		}
		const auto Find = [&After](const std::string& Id) -> const FHere*
		{
			for (const FHere& One : After)
			{
				if (One.Id == Id)
				{
					return &One;
				}
			}
			return nullptr;
		};
		const auto OneSide = [&Find](const std::vector<std::string>& Ids)
		{
			bool bTown = false, bField = false;
			for (const std::string& Id : Ids)
			{
				const FHere* Corps = Find(Id);
				(Corps && Corps->bTown ? bTown : bField) = true;
			}
			return !(bTown && bField);
		};
		const EBackInHand Step = BattleSchool::HandOnBack(Facts);
		const EBackInHand Wanted = Scenario == "garrison-back" ? EBackInHand::KeepOut : Scenario == "field-back" ? EBackInHand::Join : EBackInHand::PickUp;
		Expect(Step == Wanted, Scenario + ": BRING BACK does what the scenario wants of the hand");
		std::vector<std::string> Hand;
		if (Step == EBackInHand::PickUp)
		{
			for (const bool bTown : { false, true })
			{
				for (const FHere& One : After)
				{
					if (One.bOrders && One.bTown == bTown)
					{
						Hand.push_back(One.Id);
					}
				}
				if (!Hand.empty())
				{
					break;
				}
			}
		}
		else
		{
			Hand = Before;
			if (Step == EBackInHand::Join)
			{
				Hand.push_back(Back);
			}
			std::erase_if(Hand, [&Find](const std::string& Id) { const FHere* Corps = Find(Id); return !Corps || !Corps->bOrders; });
		}
		const FHere* Returned = Find(Back);
		Expect(Returned != nullptr && Returned->bOrders && Returned->bTown == Facts.bBackInTown, Scenario + ": the corps called back takes orders, on its own side of the walls");
		Expect(!Hand.empty(), Scenario + ": an army is in hand afterwards");
		Expect(OneSide(Hand), Scenario + ": the hand afterwards stands on one side of the town's walls");
		const bool bInHand = std::find(Hand.begin(), Hand.end(), Back) != Hand.end();
		if (!Facts.bBackInTown)
		{
			Expect(bInHand, Scenario + ": a corps back in the field is in hand");
		}
		if (Step == EBackInHand::KeepOut)
		{
			// (RefreshOrders then gives its card the rules' reason: a garrison taking orders, not picked, a field corps picked)
			Expect(!bInHand && Returned && Returned->bTown && Returned->bOrders && !OneSide([&] { std::vector<std::string> With = Hand; With.push_back(Back); return With; }()),
				Scenario + ": the garrison stays out of the field army's hand, and its card is greyed with the reason");
		}
		if (Scenario == "garrison-in-hand" || Scenario == "garrison-back")
		{
			// (before the fix the corps called back was simply added to the hand)
			std::vector<std::string> Added = Before;
			Added.push_back(Back);
			Expect(!OneSide(Added), Scenario + ": the corps simply added to the hand would have mixed garrison and field army");
		}
		std::string Shown;
		for (const std::string& Id : Hand)
		{
			Shown += (Shown.empty() ? "" : ",") + Id;
		}
		std::printf("  hand %s: in hand %s, corps %s called back: %s; in hand afterwards %s\n", Scenario.c_str(), Part[9].empty() ? "nobody" : Part[9].c_str(), Back.c_str(),
			Step == EBackInHand::Join ? "it comes along" : Step == EBackInHand::PickUp ? "the army is picked up afresh" : Step == EBackInHand::KeepOut ? "it stays out" : "the hand is left", Shown.c_str());
	}

	/** The lines probe.mjs wrote: phase, rules, known, own, grand, fort, his month, at school, can order, id, to, why. */
	void BundleCases(const char* Path)
	{
		using BattleSchool::EFace;
		std::ifstream In(Path);
		Expect(In.good(), std::string("the cards file opens: ") + Path);
		std::string Line;
		int Rows = 0, Sends = 0, Unlit = 0, Backs = 0, Hidden = 0, Hands = 0;
		bool bRelease2 = false;
		while (std::getline(In, Line))
		{
			if (Line.empty())
			{
				continue;
			}
			const std::vector<std::string> Part = SplitAt(Line, '\t');
			if (Part[0] == "hand")
			{
				HandLine(Part, Hands);
				continue;
			}
			Expect(Part.size() == 12, "a line has twelve fields: " + Line);
			if (Part.size() != 12)
			{
				continue;
			}
			++Rows;
			const std::string& Phase = Part[0];
			const bool bR2 = Part[1] == "r2";
			bRelease2 = bRelease2 || bR2;
			BattleSchool::FCorpsFacts Facts;
			Facts.bKnown = Part[2] == "1";
			Facts.bOwn = Part[3] == "1";
			Facts.bGrand = Part[4] == "1";
			Facts.bFort = Part[5] == "1";
			Facts.bHisMonth = Part[6] == "1";
			Facts.bAtSchool = Part[7] == "1";
			const bool bCanOrder = Part[8] == "1";
			Facts.To = Part[10];
			Facts.Why = Part[11];
			const std::string Who = Phase + " corps " + Part[9];
			// (release 1's view never speaks of the schools; release 2's does for every corps of the player's own, and for
			// every corps that is seen plainly: one only glimpsed through the fog is a flag and no more)
			Expect(bR2 ? (Facts.bKnown || !Facts.bOwn) : !Facts.bKnown, Who + ": the view speaks of the schools exactly when the rules have TC.school");
			const BattleSchool::FButton Button = BattleSchool::ButtonFor(Facts);
			const bool bShows = Facts.bKnown && Facts.bOwn && Facts.bGrand && !Facts.bFort && Facts.bHisMonth;
			if (!bShows)
			{
				++Hidden;
				Expect(Button.Face == EFace::None && Button.Label.empty(), Who + ": no button");
				continue;
			}
			if (Facts.bAtSchool)
			{
				++Backs;
				Expect(Button.Face == EFace::Back && Button.bLit, Who + ": at school, BRING BACK lit");
				// (the rules give a corps at school no orders: its card is kept pressable for BRING BACK alone)
				Expect(!bCanOrder, Who + ": a corps at school takes no orders");
			}
			else
			{
				++Sends;
				Expect(Button.Face == EFace::Send, Who + ": SEND TO SCHOOL");
				Expect(Button.bLit == Facts.Why.empty(), Who + ": lit exactly when the rules give no reason");
				if (!Button.bLit)
				{
					++Unlit;
					Expect(Button.Tip == Facts.Why, Who + ": unlit, its tip is the rules' reason word for word");
				}
				else
				{
					Expect(!Facts.To.empty() && Button.Tip.find(BattleSchool::GradeInSentence(Facts.To)) != std::string::npos, Who + ": lit, its tip names the grade: " + Button.Tip);
				}
			}
			if (Phase == "sent")
			{
				Expect(Button.Face == EFace::Back, Who + ": once sent, the card offers BRING BACK");
			}
			else if (Phase == "back")
			{
				Expect(Button.Face == EFace::Send && Button.bLit && bCanOrder, Who + ": called back, it takes orders and could be sent again");
			}
			else if (Phase == "refused")
			{
				Expect(Button.Face == EFace::Send && !Button.bLit, Who + ": refused, its button stays unlit");
			}
			else if (Phase == "history")
			{
				Expect(false, Who + ": a history scenario's card shows a button");
			}
		}
		Expect(Rows > 0, std::string("the cards file has lines: ") + Path);
		if (bRelease2)
		{
			Expect(Sends > 0 && Unlit > 0 && Backs > 0 && Hidden > 0, "the release 2 bundle showed every face: a lit send, an unlit send, a bring back, and cards with none");
			Expect(Hands == 4, "the release 2 bundle called a corps back to the army in hand four ways");
		}
		else
		{
			Expect(Sends == 0 && Backs == 0 && Hands == 0, "the release 1 bundle shows no button on any card");
		}
		std::printf("%s: %d cards (%d SEND TO SCHOOL, %d of them unlit; %d BRING BACK; %d with none), %d hands, %s rules\n", Path, Rows, Sends, Unlit, Backs, Hidden, Hands, bRelease2 ? "release 2" : "release 1");
	}
}

int main(int Count, char** Arguments)
{
	OwnCases();
	for (int Index = 1; Index < Count; ++Index)
	{
		BundleCases(Arguments[Index]);
	}
	std::printf("%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
