// The torpedo words' cloud test (Battle/SeaWordsCore; the naval rules behind their switch "Friendly torpedoes", Brian
// 25 November 12:52Z, and the UI director's words, final at 13:27Z): every word of the tags and the line, string for
// string (her and their, is and are, has and have, your orders and hers or theirs, 1 second and N seconds, the count
// down to 0; is and are as the phrase counts the ships where it does; routing, breaking off, no count and no orders,
// his order or not, one ship or more); an enemy piece never has words; no fields, nothing; held wins over turning
// clear; the line is the camera's subject's own or none. Where the tags stand (SeaWords::Lay): never on one another,
// the subject's first and on her ship, then the nearest; none over the bar, the line, the month scene's caption or its
// way back; a lifted tag's stalk down to its ship behind the plates it passes. The shots' eyes, their axes the scene's
// (the up square to the look). The fields as SeaFeed reads them and the show carries them, through a few views written
// here (the player's order, one ship, the count's last second and past it, a turn clear without its name: what the
// computer's sides never record), at their pinned moments and then played frame by frame as a recording, as either
// side, through every check below. Then each recording played through the very code the game runs (SeaFeed, SeaShow,
// SeaWordsCore), at 32 frames a second: SeaFeed reads every field the views set (read here apart, from the JSON
// itself), and each turning piece's state; the tags stand over the player's own pieces exactly while the views set
// their fields, each of his pieces given one tagged; a piece turning clear while routing breaks off (no count, no
// orders), one not routing never does; a recording without the fields shows no words at all; with them the show is the
// same ship for ship as without; where the views set fields on the other side's pieces, a second play takes the player
// as that side; the naval rules' own-water recordings (recordings/own-water-r2, the "Friendly torpedoes" switch on)
// show the very lines pinned for them, string for string, their names counting one to ten in words, each turn clear not
// routing ending at its count's 0, and samar nothing at any frame (one in an own-water-r2 folder that its pins do not
// know fails); and the tags, through the eyes the scene's shots put the camera at (a Fleets shot, the Line shot, the
// overview), stand laid apart on the screen.
//
//   Tools/sea_show/run.sh   (builds and runs it over every recording)
//
// Plain C++20, no engine.

#include "Battle/AirFeed.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Battle/SeaWordsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0, Passes = 0;

	void Expect(bool bOk, const std::string& Name, const std::string& Got, const std::string& Want)
	{
		if (bOk) ++Passes;
		else ++Failures;
		std::printf("  %s %-70s %s (%s)\n", bOk ? "ok  " : "FAIL", Name.c_str(), Got.c_str(), Want.c_str());
	}

	void Same(const std::string& Name, const std::string& Got, const std::string& Want)
	{
		Expect(Got == Want, Name, "\"" + Got + "\"", Got == Want ? "exact" : "\"" + Want + "\"");
	}

	/** Words that must never be on the screen (the words never speak of dice or odds). */
	bool Clean(const std::string& Line)
	{
		std::string Low = Line;
		for (char& C : Low) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		for (const char* Bad : { "dice", "odds", "chance", "%", "roll" })
		{
			if (Low.find(Bad) != std::string::npos) return false;
		}
		return true;
	}

	SeaWords::Fields HeldFor(int Id, const std::string& Name, bool bOrdered)
	{
		SeaWords::Fields F;
		F.HeldFor = Id;
		F.HeldForName = Name;
		F.bOrdered = bOrdered;
		return F;
	}

	SeaWords::Fields ClearOf(int Id, double Until, const std::string& Name, bool bOrdered)
	{
		SeaWords::Fields F;
		F.ClearFrom = Id;
		F.ClearUntil = Until;
		F.ClearFromName = Name;
		F.bOrdered = bOrdered;
		return F;
	}

	/** The tags of the moment, "piece tag", in order. */
	std::string TagsOf(const SeaWords::Words& W);

	// ---- The words, string for string ------------------------------------------------------------------------------

	void Words()
	{
		std::printf("the torpedo words, string for string\n");
		using SeaWords::Of;
		const std::string Cruiser = "a cruiser of the Home Fleet", Four = "four destroyers of the Home Fleet";
		const std::string Held1 = "Holding her torpedoes while ", Held4 = "Holding their torpedoes while ";

		// The tags.
		Same("the held tag", Of(HeldFor(5, Cruiser, true), 0, 0, 4, 1, 100.0).Tag, "Torpedoes held");
		Same("the turning clear tag", Of(ClearOf(1, 112.0, Four, true), 0, 0, 4, 4, 100.0).Tag, "Turning clear of torpedoes");

		// Held: ordered and not, one ship and a division (her and their, she fires and they fire), and the verb by the ships
		// of the piece in the way (is for one, are for more).
		Same("held, ordered, a division, the one in the way one ship", Of(HeldFor(5, Cruiser, true), 0, 0, 4, 1, 100.0).Line,
		     "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear.");
		Same("held, ordered, one ship", Of(HeldFor(5, Cruiser, true), 0, 0, 1, 1, 100.0).Line,
		     "Holding her torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: she fires the moment the water is clear.");
		Same("held, unordered, one ship", Of(HeldFor(5, Cruiser, false), 0, 0, 1, 1, 100.0).Line,
		     "Holding her torpedoes while a cruiser of the Home Fleet is in the line of fire. She fires the moment the water is clear.");
		Same("held, unordered, a division", Of(HeldFor(5, Cruiser, false), 0, 0, 3, 1, 100.0).Line,
		     "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. They fire the moment the water is clear.");
		Same("held, the one in the way a division (are)", Of(HeldFor(1, Four, false), 0, 0, 1, 4, 100.0).Line,
		     "Holding her torpedoes while four destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
		Same("held, ordered, both divisions", Of(HeldFor(1, Four, true), 0, 0, 2, 4, 100.0).Line,
		     "Holding their torpedoes while four destroyers of the Home Fleet are in the line of fire. Your order stands: they fire the moment the water is clear.");
		// (the verb agrees with the phrase where the phrase counts the ships, never with the piece holding: the rules keep the
		// name a second past their last check, and a ship of it may go down in that second, the views then one afloat)
		Same("held, the phrase counts two, one afloat: the phrase's \"are\"", Of(HeldFor(1, "two destroyers of the Home Fleet", false), 0, 0, 1, 1, 100.0).Line,
		     Held1 + "two destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
		Same("held, the phrase counts one (\"a ...\"), three afloat: \"is\"", Of(HeldFor(5, Cruiser, false), 0, 0, 1, 3, 100.0).Line,
		     Held1 + "a cruiser of the Home Fleet is in the line of fire. She fires the moment the water is clear.");
		Same("held, the phrase counts one (\"one ...\"), two afloat: \"is\"", Of(HeldFor(5, "one destroyer of the Home Fleet", true), 0, 0, 2, 2, 100.0).Line,
		     Held4 + "one destroyer of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear.");
		Same("held, the phrase counts in figures (12), one afloat: \"are\"", Of(HeldFor(1, "12 destroyers of the Home Fleet", false), 0, 0, 1, 1, 100.0).Line,
		     Held1 + "12 destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
		// (a phrase with no count: the ships the views have afloat, else more)
		Same("held, the phrase without a count, one afloat: \"is\"", Of(HeldFor(1, "the destroyers of the Home Fleet", false), 0, 0, 1, 1, 100.0).Line,
		     Held1 + "the destroyers of the Home Fleet is in the line of fire. She fires the moment the water is clear.");
		Same("held, the phrase without a count, three afloat: \"are\"", Of(HeldFor(1, "the destroyers of the Home Fleet", false), 0, 0, 1, 3, 100.0).Line,
		     Held1 + "the destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
		Same("held, the phrase without a count, not in the views: \"are\"", Of(HeldFor(9, "the destroyers of the Home Fleet", false), 0, 0, 1, -1, 100.0).Line,
		     Held1 + "the destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
		{
			// (the count a name gives, as the log writes it)
			std::string Got;
			for (const char* Name : { "a cruiser of the Home Fleet", "an aircraft carrier of the Home Fleet", "one destroyer of the Home Fleet",
			                          "two destroyers of the Home Fleet", "four destroyers of the Home Fleet", "twelve destroyers of the Home Fleet",
			                          "twenty destroyers of the Home Fleet", "12 destroyers of the Home Fleet", "the destroyers of the Home Fleet",
			                          "another cruiser of the Home Fleet", "anchored", "" })
			{
				Got += (Got.empty() ? "" : " ") + std::to_string(SeaWords::ShipsByName(Name));
			}
			Same("the ships a name counts: a, an, one, two, four, twelve, twenty, 12, the, another, one word, none", Got, "1 1 1 2 4 12 20 12 0 0 0 0");
		}
		// (the piece the name means no longer in the views: its name says)
		Same("held, the one in the way not in the views: \"a ...\" is", Of(HeldFor(9, Cruiser, false), 0, 0, 1, -1, 100.0).Line,
		     Held1 + "a cruiser of the Home Fleet is in the line of fire. She fires the moment the water is clear.");
		Same("held, the one in the way not in the views: \"an ...\" is", Of(HeldFor(9, "an aircraft carrier of the Home Fleet", false), 0, 0, 4, 0, 100.0).Line,
		     Held4 + "an aircraft carrier of the Home Fleet is in the line of fire. They fire the moment the water is clear.");
		Same("held, the one in the way not in the views: \"four ...\" are", Of(HeldFor(9, Four, false), 0, 0, 4, -1, 100.0).Line,
		     Held4 + "four destroyers of the Home Fleet are in the line of fire. They fire the moment the water is clear.");

		// Turning clear: your orders under his order, else hers or theirs; she takes and they take.
		Same("turning clear, ordered, a division", Of(ClearOf(1, 112.0, Four, true), 0, 0, 4, -1, 100.0).Line,
		     "Turning clear of torpedoes fired by four destroyers of the Home Fleet. They take up your orders again in 12 seconds, once the torpedoes have passed.");
		Same("turning clear, ordered, one ship", Of(ClearOf(1, 112.0, Four, true), 0, 0, 1, -1, 100.0).Line,
		     "Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up your orders again in 12 seconds, once the torpedoes have passed.");
		Same("turning clear, unordered, one ship", Of(ClearOf(1, 112.0, Four, false), 0, 0, 1, -1, 100.0).Line,
		     "Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up her orders again in 12 seconds, once the torpedoes have passed.");
		Same("turning clear, unordered, a division", Of(ClearOf(1, 112.0, Four, false), 0, 0, 2, -1, 100.0).Line,
		     "Turning clear of torpedoes fired by four destroyers of the Home Fleet. They take up their orders again in 12 seconds, once the torpedoes have passed.");
		// (the torpedoes have passed, whatever fired them: one ship's salvo too)
		Same("turning clear of one ship's salvo: the torpedoes still have passed", Of(ClearOf(3, 112.0, Cruiser, false), 0, 0, 1, -1, 100.0).Line,
		     "Turning clear of torpedoes fired by a cruiser of the Home Fleet. She takes up her orders again in 12 seconds, once the torpedoes have passed.");

		// Routing: breaking off, she has no orders to take up again: the one line, no count and no orders, a ship alone or a
		// division, his order or not, the count's 0 passed or not; the tag the same; no name, the tag alone; held, the held
		// words unchanged; the enemy's, none.
		{
			auto Routing = [](SeaWords::Fields F)
			{
				F.bRouting = true;
				return F;
			};
			const std::string Breaking = "Breaking off, and turning clear of torpedoes fired by four destroyers of the Home Fleet.";
			for (int Ships : { 1, 4 })
				for (bool bOrdered : { false, true })
				{
					const SeaWords::Said Said = Of(Routing(ClearOf(1, 112.0, Four, bOrdered)), 0, 0, Ships, -1, 100.0);
					const std::string Who = std::string(Ships == 1 ? "a ship alone" : "a division") + (bOrdered ? ", ordered" : ", unordered");
					Same("turning clear, routing, " + Who + ": the line", Said.Line, Breaking);
					Same("turning clear, routing, " + Who + ": the tag", Said.Tag, "Turning clear of torpedoes");
					Expect(Said.What == SeaWords::Kind::Clearing, "turning clear, routing, " + Who + ": turning clear", Said.What == SeaWords::Kind::Clearing ? "turning clear" : "not",
					       "turning clear");
				}
			Same("turning clear, routing, its count's 0 passed: no count still", Of(Routing(ClearOf(1, 100.0, Four, false)), 0, 0, 4, -1, 105.0).Line, Breaking);
			Same("turning clear, routing, of one ship's salvo", Of(Routing(ClearOf(3, 112.0, Cruiser, true)), 0, 0, 1, -1, 100.0).Line,
			     "Breaking off, and turning clear of torpedoes fired by a cruiser of the Home Fleet.");
			const SeaWords::Said Unnamed = Of(Routing(ClearOf(1, 112.0, "", true)), 0, 0, 1, -1, 100.0);
			Expect(Unnamed.What == SeaWords::Kind::Clearing && Unnamed.Tag == "Turning clear of torpedoes" && Unnamed.Line.empty(),
			       "turning clear, routing, no name: the tag, no line", Unnamed.Tag + " / " + Unnamed.Line, "Turning clear of torpedoes / ");
			SeaWords::Fields Both = Routing(HeldFor(5, Cruiser, true));
			Both.ClearFrom = 1;
			Both.ClearUntil = 112.0;
			Both.ClearFromName = Four;
			const SeaWords::Said HeldToo = Of(Both, 0, 0, 4, 1, 100.0);
			Same("routing, held and turning clear: the held tag", HeldToo.Tag, "Torpedoes held");
			Same("routing, held and turning clear: the held line unchanged", HeldToo.Line,
			     "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear.");
			Same("routing, held: the held line unchanged", Of(Routing(HeldFor(1, Four, false)), 0, 0, 1, 4, 100.0).Line,
			     "Holding her torpedoes while four destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear.");
			const SeaWords::Said Enemy = Of(Routing(ClearOf(12, 112.0, "four destroyers of the Battle Fleet", false)), 0, 1, 4, -1, 100.0);
			Expect(Enemy.What == SeaWords::Kind::None && Enemy.Tag.empty() && Enemy.Line.empty(), "an enemy piece routing, turning clear: no words", Enemy.Tag + Enemy.Line, "none");
			bool bClean = true;
			for (int Ships : { 1, 4 })
				for (bool bOrdered : { false, true })
					bClean = bClean && Clean(Of(Routing(ClearOf(1, 112.0, Four, bOrdered)), 0, 0, Ships, -1, 100.0).Line);
			Expect(bClean, "routing: the words never speak of dice or odds", bClean ? "never" : "they do", "never");
			// (the state as the show's piece has it, the scene's way in: FieldsOf reads "routing", and only that, as breaking off)
			std::string States;
			for (const char* State : { "routing", "steady", "wavering", "withdrawn", "gone", "" })
			{
				SeaShow::Piece P;
				P.State = State;
				States += (States.empty() ? "" : " ") + std::string(SeaWords::FieldsOf(P).bRouting ? "1" : "0");
			}
			Same("FieldsOf: routing as the piece's state, routing, steady, wavering, withdrawn, gone, none", States, "1 0 0 0 0 0");
			std::map<int, SeaShow::Piece> Pieces;
			auto Add = [&Pieces](int Id, int Ships, const char* State, bool bOrdered) -> SeaShow::Piece&
			{
				SeaShow::Piece& P = Pieces[Id];
				P.Id = Id;
				P.Side = 0;
				P.Ships = Ships;
				P.State = State;
				P.ClearFrom = 9;
				P.ClearUntil = 112.0;
				P.ClearFromName = "four destroyers of the Home Fleet";
				P.bOrdered = bOrdered;
				return P;
			};
			Add(1, 4, "routing", false);
			Add(2, 1, "routing", true);
			Add(3, 4, "steady", false);
			Add(4, 1, "wavering", true);
			Add(9, 4, "steady", false).ClearFrom = -1;
			std::string Said;
			for (int Subject : { 1, 2, 3, 4 }) Said += (Said.empty() ? "" : " | ") + SeaWords::Write(Pieces, 0, Subject, 100.0).Line;
			Same("the show's pieces: a division routing, a ship alone routing ordered, a division steady, a ship alone wavering ordered", Said,
			     Breaking + " | " + Breaking +
			         " | Turning clear of torpedoes fired by four destroyers of the Home Fleet. They take up their orders again in 12 seconds, once the torpedoes have passed."
			         " | Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up your orders again in 12 seconds, once the torpedoes have passed.");
			Same("the show's pieces, routing or not: the tags", TagsOf(SeaWords::Write(Pieces, 0, -1, 100.0)),
			     "1 Turning clear of torpedoes, 2 Turning clear of torpedoes, 3 Turning clear of torpedoes, 4 Turning clear of torpedoes");
		}

		// The count: whole seconds left, rounded up, 1 second alone, never below 0.
		auto Count = [](double Until, double Now)
		{
			const std::string Line = SeaWords::Of(ClearOf(1, Until, "four destroyers of the Home Fleet", true), 0, 0, 4, -1, Now).Line;
			const size_t From = Line.find(" again in "), To = Line.find(", once");
			return From == std::string::npos || To == std::string::npos ? std::string("(none)") : Line.substr(From + 10, To - From - 10);
		};
		Same("the count, 12 s left", Count(112.0, 100.0), "12 seconds");
		Same("the count, 12.4 s left (rounded up)", Count(112.4, 100.0), "13 seconds");
		Same("the count, 12 s left by the clock's sums (12.0000001)", Count(112.0000001, 100.0), "12 seconds");
		Same("the count, 2 s left", Count(102.0, 100.0), "2 seconds");
		Same("the count, 1.5 s left", Count(101.5, 100.0), "2 seconds");
		Same("the count, 1 s left (one second)", Count(101.0, 100.0), "1 second");
		Same("the count, 0.4 s left (one second)", Count(100.4, 100.0), "1 second");
		Same("the count, none left", Count(100.0, 100.0), "0 seconds");
		Same("the count, past the landing (never below 0)", Count(100.0, 105.0), "0 seconds");
		Same("the count, the salvo long landed", Count(100.0, 1000.0), "0 seconds");
		{
			// (down to 0 a second at a time, as the battle's clock runs)
			std::string Went;
			int Last = SeaWords::SecondsLeft(100.0, 90.0);
			bool bDown = true;
			for (double Now = 90.0; Now <= 104.0 + 1e-9; Now += 0.05)
			{
				const int Left = SeaWords::SecondsLeft(100.0, Now);
				bDown = bDown && Left <= Last && Left >= 0 && Last - Left <= 1;
				Last = Left;
			}
			Went = std::to_string(SeaWords::SecondsLeft(100.0, 90.0)) + " down to " + std::to_string(Last);
			Expect(bDown && Last == 0 && SeaWords::SecondsLeft(100.0, 90.0) == 10, "the count runs down to 0 a second at a time, never below", Went, "10 down to 0");
		}
		Same("1 second", SeaWords::SecondsWords(1), "1 second");
		Same("0 seconds", SeaWords::SecondsWords(0), "0 seconds");
		Same("12 seconds", SeaWords::SecondsWords(12), "12 seconds");

		// is and are, has and have, her and their: by the ships of the piece meant (one, or none, the first).
		Same("is, one ship", SeaWords::Agree(1, "is", "are"), "is");
		Same("are, four ships", SeaWords::Agree(4, "is", "are"), "are");
		Same("are, two ships", SeaWords::Agree(2, "is", "are"), "are");
		Same("has, one ship", SeaWords::Agree(1, "has", "have"), "has");
		Same("have, three ships", SeaWords::Agree(3, "has", "have"), "have");
		Same("her, one ship", SeaWords::Agree(1, "her", "their"), "her");
		Same("their, a division", SeaWords::Agree(4, "her", "their"), "their");

		// Held wins over turning clear, both on one piece.
		{
			SeaWords::Fields Both = HeldFor(5, Cruiser, true);
			Both.ClearFrom = 1;
			Both.ClearUntil = 112.0;
			Both.ClearFromName = Four;
			const SeaWords::Said Said = Of(Both, 0, 0, 4, 1, 100.0);
			Same("held wins: the tag", Said.Tag, "Torpedoes held");
			Same("held wins: the line", Said.Line,
			     "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear.");
		}

		// Never the enemy's, never with no fields, never a piece with none afloat; the tag without a name, no line.
		{
			const SeaWords::Said Enemy = Of(HeldFor(12, "a cruiser of the Battle Fleet", true), 1, 0, 1, 1, 100.0);
			Expect(Enemy.What == SeaWords::Kind::None && Enemy.Tag.empty() && Enemy.Line.empty(), "an enemy piece holding: no words", Enemy.Tag + Enemy.Line, "none");
			const SeaWords::Said EnemyClear = Of(ClearOf(12, 112.0, "four destroyers of the Battle Fleet", false), 0, 1, 4, -1, 100.0);
			Expect(EnemyClear.What == SeaWords::Kind::None && EnemyClear.Tag.empty() && EnemyClear.Line.empty(), "an enemy piece turning clear (the player the defender): no words",
			       EnemyClear.Tag + EnemyClear.Line, "none");
			const SeaWords::Said Nobody = Of(HeldFor(5, Cruiser, true), 0, -1, 1, 1, 100.0);
			Expect(Nobody.What == SeaWords::Kind::None && Nobody.Tag.empty(), "no player's side: no words", Nobody.Tag, "none");
			const SeaWords::Said Off = Of(SeaWords::Fields(), 0, 0, 4, -1, 100.0);
			Expect(Off.What == SeaWords::Kind::None && Off.Tag.empty() && Off.Line.empty(), "no fields (the switch off): no words", Off.Tag + Off.Line, "none");
			const SeaWords::Said Sunk = Of(HeldFor(5, Cruiser, true), 0, 0, 0, 1, 100.0);
			Expect(Sunk.What == SeaWords::Kind::None && Sunk.Tag.empty(), "a piece with none afloat: no words", Sunk.Tag, "none");
			const SeaWords::Said Unnamed = Of(HeldFor(5, "", true), 0, 0, 1, 1, 100.0);
			Expect(Unnamed.Tag == "Torpedoes held" && Unnamed.Line.empty(), "held with no name: the tag, no line", Unnamed.Tag + " / " + Unnamed.Line, "Torpedoes held / ");
			// (the same for a turn clear without its clearFromName, a division and a ship alone under his order)
			const SeaWords::Said UnnamedClear = Of(ClearOf(1, 112.0, "", false), 0, 0, 4, -1, 100.0);
			Expect(UnnamedClear.What == SeaWords::Kind::Clearing && UnnamedClear.Tag == "Turning clear of torpedoes" && UnnamedClear.Line.empty(),
			       "turning clear with no name: the tag, no line", UnnamedClear.Tag + " / " + UnnamedClear.Line, "Turning clear of torpedoes / ");
			const SeaWords::Said UnnamedClearOrdered = Of(ClearOf(1, 112.0, "", true), 0, 0, 1, -1, 100.0);
			Expect(UnnamedClearOrdered.What == SeaWords::Kind::Clearing && UnnamedClearOrdered.Tag == "Turning clear of torpedoes" && UnnamedClearOrdered.Line.empty(),
			       "turning clear with no name, one ship, ordered: the tag, no line", UnnamedClearOrdered.Tag + " / " + UnnamedClearOrdered.Line, "Turning clear of torpedoes / ");
		}

		// The screen's words from the show's pieces: the tags his own only, the line the subject's own or none.
		{
			std::map<int, SeaShow::Piece> Pieces;
			auto Add = [&Pieces](int Id, int Side, int Ships) -> SeaShow::Piece&
			{
				SeaShow::Piece& P = Pieces[Id];
				P.Id = Id;
				P.Side = Side;
				P.Ships = Ships;
				return P;
			};
			SeaShow::Piece& Mine1 = Add(1, 0, 4);
			Mine1.ClearFrom = 2;
			Mine1.ClearUntil = 130.0;
			Mine1.ClearFromName = "two destroyers of the Home Fleet";
			SeaShow::Piece& Mine2 = Add(2, 0, 2);
			Mine2.HeldFor = 7;
			Mine2.HeldForName = "a battleship of the Home Fleet";
			Mine2.bOrdered = true;
			Add(3, 0, 1);
			Add(7, 0, 1);
			SeaShow::Piece& Theirs = Add(11, 1, 4);
			Theirs.HeldFor = 10;
			Theirs.HeldForName = "four destroyers of the Battle Fleet";
			SeaShow::Piece& TheirsClear = Add(14, 1, 1);
			TheirsClear.ClearFrom = 10;
			TheirsClear.ClearUntil = 140.0;
			TheirsClear.ClearFromName = "four destroyers of the Battle Fleet";
			Add(10, 1, 4);

			const SeaWords::Words None = SeaWords::Write(Pieces, 0, -1, 100.0);
			std::string Tagged;
			for (const SeaWords::Tag& T : None.Tags) Tagged += (Tagged.empty() ? "" : ", ") + std::to_string(T.Piece) + " " + T.Text;
			Same("the tags: his own holding or turning clear, none of the enemy's", Tagged, "1 Turning clear of torpedoes, 2 Torpedoes held");
			Expect(None.Line.empty() && None.LinePiece < 0, "no subject: no line", None.Line, "none");
			const SeaWords::Words On2 = SeaWords::Write(Pieces, 0, 2, 100.0);
			Same("the subject held: its line", On2.Line,
			     "Holding their torpedoes while a battleship of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear.");
			const SeaWords::Words On1 = SeaWords::Write(Pieces, 0, 1, 100.0);
			Same("the subject turning clear: its line", On1.Line,
			     "Turning clear of torpedoes fired by two destroyers of the Home Fleet. They take up their orders again in 30 seconds, once the torpedoes have passed.");
			const SeaWords::Words On3 = SeaWords::Write(Pieces, 0, 3, 100.0);
			Expect(On3.Line.empty() && On3.Tags.size() == 2, "the subject his own with nothing to say: no line (never another piece's)", On3.Line, "none, the two tags");
			const SeaWords::Words On11 = SeaWords::Write(Pieces, 0, 11, 100.0);
			Expect(On11.Line.empty(), "the subject the enemy's, holding: no line", On11.Line, "none");
			const SeaWords::Words On14 = SeaWords::Write(Pieces, 0, 14, 100.0);
			Expect(On14.Line.empty(), "the subject the enemy's, turning clear: no line", On14.Line, "none");
			// (the player the defender: the Battle Fleet's are his, the Home Fleet's the enemy's)
			const SeaWords::Words Def = SeaWords::Write(Pieces, 1, 14, 100.0);
			std::string DefTagged;
			for (const SeaWords::Tag& T : Def.Tags) DefTagged += (DefTagged.empty() ? "" : ", ") + std::to_string(T.Piece) + " " + T.Text;
			Same("the player the defender: his own tags only", DefTagged, "11 Torpedoes held, 14 Turning clear of torpedoes");
			Same("the player the defender: his subject's line", Def.Line,
			     "Turning clear of torpedoes fired by four destroyers of the Battle Fleet. She takes up her orders again in 40 seconds, once the torpedoes have passed.");
			// (the piece in the way looked up among the pieces: Destroyers 10's four ships)
			const SeaWords::Words Def11 = SeaWords::Write(Pieces, 1, 11, 100.0);
			Same("the player the defender: the verb by the ships of the one in the way", Def11.Line,
			     "Holding their torpedoes while four destroyers of the Battle Fleet are in the line of fire. They fire the moment the water is clear.");
			// (no fields anywhere: nothing)
			std::map<int, SeaShow::Piece> Bare = Pieces;
			for (auto& Pair : Bare)
			{
				SeaShow::Piece Plain = SeaShow::Piece();
				Plain.Id = Pair.second.Id;
				Plain.Side = Pair.second.Side;
				Plain.Ships = Pair.second.Ships;
				Pair.second = Plain;
			}
			bool bQuiet = true;
			for (int Subject = -1; Subject <= 14; ++Subject)
			{
				const SeaWords::Words W = SeaWords::Write(Bare, 0, Subject, 100.0);
				bQuiet = bQuiet && W.Tags.empty() && W.Line.empty();
			}
			Expect(bQuiet, "no fields on any piece: no tags, no line, whatever the subject", bQuiet ? "none" : "some", "none");
		}

		// The words never speak of dice or odds.
		{
			bool bClean = true;
			for (int Ships : { 1, 4 })
				for (bool bOrdered : { false, true })
				{
					bClean = bClean && Clean(Of(HeldFor(5, Cruiser, bOrdered), 0, 0, Ships, 1, 100.0).Line) && Clean(Of(ClearOf(1, 112.0, Four, bOrdered), 0, 0, Ships, -1, 100.0).Line);
				}
			Expect(bClean, "the words never speak of dice or odds", bClean ? "never" : "they do", "never");
		}
	}

	// ---- Where the tags stand: never on one another ------------------------------------------------------------------

	constexpr double TagHigh = 18.0, TagStalk = 8.0; // (SSeaBattleScreen's)
	/** About the width of a tag's plate at 10 points (the screen measures it: some 6 a letter and 12 of margin). */
	double PlateWide(const std::string& Text) { return 5.9 * static_cast<double>(Text.size()) + 12.0; }

	SeaWords::Spot SpotAt(double X, double Y, double Wide, double Deep, int Piece, bool bSubject = false)
	{
		SeaWords::Spot S;
		S.X = X;
		S.Y = Y;
		S.Wide = Wide;
		S.Deep = Deep;
		S.Piece = Piece;
		S.bSubject = bSubject;
		return S;
	}

	SeaWords::Box ZoneOf(const SeaWords::Laid& One) { return SeaWords::Box{ One.Plate.L, One.Plate.T, One.Plate.R, One.Plate.B + TagStalk }; }

	/** What is wrong with tags as laid, if anything (empty: nothing): two on one another (plate and the first of its
	 * stalk), one on a place kept, a stalk piece behind no plate missing or one across another's plate, a tag lifted off
	 * the screen's top or more than MostLifts, a tag the wrong size or not over its foot. */
	std::string Wrong(const std::vector<SeaWords::Spot>& Spots, const std::vector<SeaWords::Box>& Kept, const std::vector<SeaWords::Laid>& Laid)
	{
		char Buf[200];
		for (size_t I = 0; I < Laid.size(); ++I)
		{
			const SeaWords::Laid& A = Laid[I];
			const SeaWords::Spot& S = Spots[A.Of];
			if (std::fabs(A.Plate.B - A.Plate.T - TagHigh) > 1e-9 || std::fabs(A.Plate.R - A.Plate.L - S.Wide) > 1e-9 || std::fabs((A.Plate.L + A.Plate.R) * 0.5 - S.X) > 1e-9 ||
			    A.FootX != S.X || A.FootY != S.Y)
			{
				std::snprintf(Buf, sizeof Buf, "piece %d: not its size or not over its foot", S.Piece);
				return Buf;
			}
			const double Lift = (S.Y - TagStalk - TagHigh) - A.Plate.T;
			if (A.Lifts < 0 || A.Lifts > SeaWords::MostLifts || (A.Lifts == 0) != (std::fabs(Lift) < 1e-9) || Lift < -1e-9 || (A.Lifts > 0 && A.Plate.T < 0.0))
			{
				std::snprintf(Buf, sizeof Buf, "piece %d: lifted %d times, %.2f up, top %.2f", S.Piece, A.Lifts, Lift, A.Plate.T);
				return Buf;
			}
			const SeaWords::Box Whole{ A.Plate.L, A.Plate.T, A.Plate.R, S.Y };
			for (const SeaWords::Box& Keep : Kept)
			{
				if (Whole.Touches(Keep, 0.0))
				{
					std::snprintf(Buf, sizeof Buf, "piece %d over a place kept (%.0f %.0f %.0f %.0f)", S.Piece, Keep.L, Keep.T, Keep.R, Keep.B);
					return Buf;
				}
			}
			for (size_t J = 0; J < Laid.size(); ++J)
			{
				if (J == I) continue;
				const SeaWords::Laid& Z = Laid[J];
				if (J > I && ZoneOf(A).Touches(ZoneOf(Z), 0.0))
				{
					std::snprintf(Buf, sizeof Buf, "pieces %d and %d on one another", S.Piece, Spots[Z.Of].Piece);
					return Buf;
				}
				// (a stalk never across another's plate: it passes behind it, in pieces)
				for (const std::pair<double, double>& Bit : A.Stalk)
				{
					if (A.FootX >= Z.Plate.L && A.FootX <= Z.Plate.R && Bit.first < Z.Plate.B && Z.Plate.T < Bit.second)
					{
						std::snprintf(Buf, sizeof Buf, "piece %d: its stalk across piece %d's plate", S.Piece, Spots[Z.Of].Piece);
						return Buf;
					}
				}
			}
			// (the stalk runs from the plate to the foot: its pieces in order, within it, and every gap behind a plate)
			double Down = A.Plate.B;
			for (const std::pair<double, double>& Bit : A.Stalk)
			{
				const double Mid = (Down + Bit.first) * 0.5;
				bool bBehind = Bit.first - Down < 1e-9;
				for (const SeaWords::Laid& Z : Laid)
				{
					bBehind = bBehind || (&Z != &A && A.FootX >= Z.Plate.L - 1.0 && A.FootX <= Z.Plate.R + 1.0 && Mid >= Z.Plate.T - 1.0 && Mid <= Z.Plate.B + 1.0);
				}
				if (Bit.first < Down - 1e-9 || Bit.second <= Bit.first || !bBehind)
				{
					std::snprintf(Buf, sizeof Buf, "piece %d: its stalk broken (%.2f to %.2f after %.2f)", S.Piece, Bit.first, Bit.second, Down);
					return Buf;
				}
				Down = Bit.second;
			}
			// (down to the foot, but where the foot itself is behind another's plate: the ship's top hidden by that tag)
			bool bFootBehind = false;
			for (const SeaWords::Laid& Z : Laid)
			{
				bFootBehind = bFootBehind || (&Z != &A && A.FootX >= Z.Plate.L - 1.0 && A.FootX <= Z.Plate.R + 1.0 && S.Y >= Z.Plate.T - 1.0 && S.Y <= Z.Plate.B + 1.5 &&
				                              std::fabs(Down - (Z.Plate.T - 1.0)) < 1e-9);
			}
			if (A.Stalk.empty() || (std::fabs(Down - S.Y) > 1e-9 && !bFootBehind))
			{
				std::snprintf(Buf, sizeof Buf, "piece %d: its stalk ends at %.2f, its foot %.2f", S.Piece, Down, S.Y);
				return Buf;
			}
		}
		return "";
	}

	std::string Stood(const std::vector<SeaWords::Spot>& Spots, const std::vector<SeaWords::Laid>& Laid)
	{
		std::string Out;
		for (const SeaWords::Laid& One : Laid) Out += (Out.empty() ? "" : ", ") + std::to_string(Spots[One.Of].Piece) + (One.Lifts ? "^" + std::to_string(One.Lifts) : "");
		return Out;
	}

	void Placing()
	{
		std::printf("where the tags stand: never on one another\n");
		using SeaWords::Lay;
		const double Clear = PlateWide(SeaWords::ClearingTag), Held = PlateWide(SeaWords::HeldTag);
		const std::vector<SeaWords::Box> None;
		// (the bar's plate on a 1600 by 900 screen, and a line's)
		const std::vector<SeaWords::Box> BarAndLine = { SeaWords::Box{ 549.0, -1.0e9, 1051.0, 46.0 }, SeaWords::Box{ 300.0, 810.0, 1300.0, 836.0 } };

		{
			// One alone: on its ship, as ever.
			const std::vector<SeaWords::Spot> Spots = { SpotAt(800.0, 400.0, Clear, 1000.0, 6) };
			const std::vector<SeaWords::Laid> L = Lay(Spots, None, TagHigh, TagStalk);
			const bool bOk = L.size() == 1 && L[0].Lifts == 0 && L[0].Plate.T == 400.0 - TagStalk - TagHigh && L[0].Plate.B == 400.0 - TagStalk && L[0].Stalk.size() == 1 &&
			                 L[0].Stalk[0].first == 400.0 - TagStalk && L[0].Stalk[0].second == 400.0;
			Expect(bOk && Wrong(Spots, None, L).empty(), "one tag alone: on its ship, its stalk whole", Stood(Spots, L), "6");
		}
		{
			// Two in one column, nearly on one another (two ships up one column, as a Fleets shot's eye sees them): the nearer
			// on her ship, the farther lifted just over it, its stalk behind it.
			const std::vector<SeaWords::Spot> Spots = { SpotAt(806.0, 396.0, Clear, 1585.0, 7), SpotAt(800.0, 400.0, Clear, 1000.0, 6) };
			const std::vector<SeaWords::Laid> L = Lay(Spots, None, TagHigh, TagStalk);
			const bool bOk = L.size() == 2 && L[0].Of == 1 && L[0].Lifts == 0 && L[1].Of == 0 && L[1].Lifts == 1 &&
			                 std::fabs(L[1].Plate.T - (L[0].Plate.T - SeaWords::TagGap - TagStalk - TagHigh)) < 1e-9 && L[1].Stalk.size() == 2 &&
			                 std::fabs(L[1].Stalk[0].second - (L[0].Plate.T - 1.0)) < 1e-9 && std::fabs(L[1].Stalk[1].first - (L[0].Plate.B + 1.0)) < 1e-9 && L[1].Stalk[1].second == 396.0;
			const std::string Bad = Wrong(Spots, None, L);
			Expect(bOk && Bad.empty(), "two up one column: the nearer on her ship, the farther lifted over it, its stalk behind", Stood(Spots, L) + (Bad.empty() ? "" : " " + Bad), "6, 7^1");
		}
		{
			// The camera's subject first, though farther: her tag stays on her ship.
			const std::vector<SeaWords::Spot> Spots = { SpotAt(806.0, 396.0, Clear, 1585.0, 7, true), SpotAt(800.0, 400.0, Clear, 1000.0, 6) };
			const std::vector<SeaWords::Laid> L = Lay(Spots, None, TagHigh, TagStalk);
			const bool bOk = L.size() == 2 && L[0].Of == 0 && L[0].Lifts == 0 && L[1].Of == 1 && L[1].Lifts == 1;
			Expect(bOk && Wrong(Spots, None, L).empty(), "the subject's tag first, on her ship, though farther", Stood(Spots, L), "7, 6^1");
		}
		{
			// Four together (four pieces turning clear at once): all four stand, none on another.
			const std::vector<SeaWords::Spot> Spots = { SpotAt(700.0, 420.0, Clear, 1400.0, 1), SpotAt(760.0, 430.0, Clear, 900.0, 3), SpotAt(706.0, 414.0, Clear, 1800.0, 6),
			                                            SpotAt(650.0, 425.0, Clear, 1100.0, 7) };
			const std::vector<SeaWords::Laid> L = Lay(Spots, BarAndLine, TagHigh, TagStalk);
			const std::string Bad = Wrong(Spots, BarAndLine, L);
			Expect(L.size() == 4 && Bad.empty() && Spots[L[0].Of].Piece == 3 && L[0].Lifts == 0, "four turning clear together: all stand, none on another, the nearest on her ship",
			       Stood(Spots, L) + (Bad.empty() ? "" : " " + Bad), "3, then the rest lifted clear");
		}
		{
			// Five on one spot: lifted three times at most, the fifth left off.
			std::vector<SeaWords::Spot> Spots;
			for (int K = 0; K < 5; ++K) Spots.push_back(SpotAt(800.0, 500.0, Held, 1000.0 + 100.0 * K, K + 1));
			const std::vector<SeaWords::Laid> L = Lay(Spots, None, TagHigh, TagStalk);
			Expect(L.size() == 4 && Wrong(Spots, None, L).empty() && L.back().Lifts == SeaWords::MostLifts, "five on one spot: three lifted, the fifth left off", Stood(Spots, L),
			       "1, 2^1, 3^2, 4^3");
		}
		{
			// Side by side, apart: each on its own ship.
			const std::vector<SeaWords::Spot> Spots = { SpotAt(400.0, 400.0, Clear, 1000.0, 1), SpotAt(400.0 + Clear + 4.0, 400.0, Clear, 900.0, 2) };
			const std::vector<SeaWords::Laid> L = Lay(Spots, None, TagHigh, TagStalk);
			Expect(L.size() == 2 && L[0].Lifts == 0 && L[1].Lifts == 0 && Wrong(Spots, None, L).empty(), "two apart: each on her own ship", Stood(Spots, L), "2, 1");
		}
		{
			// The places kept: none over the bar or the line (as before), nor lifted into the bar; none over the month scene's
			// caption or its way back to the map.
			const SeaWords::Box Caption{ 528.0, 668.0, 1072.0, 740.0 }, Back{ 1340.0, 862.0, 1580.0, 884.0 };
			const std::vector<SeaWords::Box> Month = { Caption, Back };
			auto Stands = [](const std::vector<SeaWords::Spot>& Spots, const std::vector<SeaWords::Box>& Kept) { return Stood(Spots, Lay(Spots, Kept, TagHigh, TagStalk)); };
			Same("a tag over the bar: left off", Stands({ SpotAt(800.0, 60.0, Clear, 1000.0, 1) }, BarAndLine), "");
			Same("a tag just under the bar: stands", Stands({ SpotAt(800.0, 46.0 + TagStalk + TagHigh + 0.5, Clear, 1000.0, 1) }, BarAndLine), "1");
			Same("a tag lifted into the bar: left off", Stands({ SpotAt(800.0, 90.0, Clear, 1000.0, 1), SpotAt(805.0, 92.0, Clear, 1200.0, 2) }, BarAndLine), "1");
			Same("a tag whose stalk reaches the line: left off", Stands({ SpotAt(800.0, 815.0, Clear, 1000.0, 1) }, BarAndLine), "");
			Same("a tag under the line: stands", Stands({ SpotAt(800.0, 836.0 + TagStalk + TagHigh + 0.5, Clear, 1000.0, 1) }, BarAndLine), "1");
			Same("a tag lifted up across the line: left off", Stands({ SpotAt(800.0, 870.0, Clear, 1000.0, 1), SpotAt(805.0, 872.0, Clear, 1200.0, 2) }, BarAndLine), "1");
			Same("a tag on the month's caption: left off", Stands({ SpotAt(800.0, 700.0, Clear, 1000.0, 1) }, Month), "");
			Same("a tag whose stalk reaches the caption: left off", Stands({ SpotAt(600.0, 672.0, Held, 1000.0, 1) }, Month), "");
			Same("a tag clear over the caption: stands", Stands({ SpotAt(600.0, 668.0 - 0.5, Held, 1000.0, 1) }, Month), "1");
			Same("a tag lifted onto the caption's... no: one under it lifted onto it, left off", Stands({ SpotAt(600.0, 770.0, Held, 1000.0, 1), SpotAt(602.0, 771.0, Held, 1100.0, 2) }, Month), "1");
			Same("a tag on the way back to the map: left off", Stands({ SpotAt(1450.0, 890.0, Held, 1000.0, 1) }, Month), "");
			Same("a tag beside the way back: stands", Stands({ SpotAt(1200.0, 890.0, Held, 1000.0, 1) }, Month), "1");
			Same("a tag lifted off the screen's top: left off", Stands({ SpotAt(200.0, 30.0, Held, 1000.0, 1), SpotAt(202.0, 31.0, Held, 1100.0, 2) }, None), "1");
		}
		{
			// Any tags at all (a fixed scatter, a clump, a row): never on one another, never on a place kept, the subject's
			// on her ship wherever she can stand; the same however they come.
			const std::vector<SeaWords::Box> Kept = { BarAndLine[0], BarAndLine[1], SeaWords::Box{ 528.0, 668.0, 1072.0, 740.0 }, SeaWords::Box{ 1340.0, 862.0, 1580.0, 884.0 } };
			unsigned Seed = 239u;
			auto Next = [&Seed]() { Seed = Seed * 1664525u + 1013904223u; return static_cast<double>(Seed >> 8) / 16777216.0; };
			long Cases = 0, Bad = 0, Lifted = 0, SubjectOff = 0, Unsteady = 0;
			std::string First;
			for (int Case = 0; Case < 3000; ++Case)
			{
				const int Count = 1 + Case % 8;
				const double Cx = 100.0 + 1400.0 * Next(), Cy = 30.0 + 840.0 * Next(), Spread = Case % 3 == 0 ? 400.0 : Case % 3 == 1 ? 60.0 : 12.0;
				std::vector<SeaWords::Spot> Spots;
				for (int K = 0; K < Count; ++K)
				{
					Spots.push_back(SpotAt(Cx + Spread * (Next() - 0.5) * 2.0, Cy + Spread * (Next() - 0.5), Next() < 0.5 ? Clear : Held, 500.0 + 3000.0 * Next(), K + 1,
					                       K == Count - 1 && Case % 2 == 0));
				}
				const std::vector<SeaWords::Laid> L = Lay(Spots, Kept, TagHigh, TagStalk);
				++Cases;
				const std::string Why = Wrong(Spots, Kept, L);
				if (!Why.empty())
				{
					++Bad;
					if (First.empty()) First = "case " + std::to_string(Case) + ": " + Why;
				}
				for (const SeaWords::Laid& One : L) Lifted += One.Lifts > 0 ? 1 : 0;
				// (the subject's, where her own ship's place is clear of the places kept, stands first on it)
				for (size_t K = 0; K < Spots.size(); ++K)
				{
					if (!Spots[K].bSubject) continue;
					const SeaWords::Box Whole{ Spots[K].X - Spots[K].Wide * 0.5, Spots[K].Y - TagStalk - TagHigh, Spots[K].X + Spots[K].Wide * 0.5, Spots[K].Y };
					const bool bFree = std::none_of(Kept.begin(), Kept.end(), [&Whole](const SeaWords::Box& Keep) { return Whole.Touches(Keep, 0.0); });
					if (bFree && (L.empty() || L[0].Of != K || L[0].Lifts != 0)) ++SubjectOff;
				}
				// (the same, the spots the other way round)
				std::vector<SeaWords::Spot> Back(Spots.rbegin(), Spots.rend());
				const std::vector<SeaWords::Laid> LB = Lay(Back, Kept, TagHigh, TagStalk);
				std::string Was, Now;
				char Buf[96];
				for (const SeaWords::Laid& One : L)
				{
					std::snprintf(Buf, sizeof Buf, "%d %.6f %.6f;", Spots[One.Of].Piece, One.Plate.L, One.Plate.T);
					Was += Buf;
				}
				for (const SeaWords::Laid& One : LB)
				{
					std::snprintf(Buf, sizeof Buf, "%d %.6f %.6f;", Back[One.Of].Piece, One.Plate.L, One.Plate.T);
					Now += Buf;
				}
				if (Was != Now) ++Unsteady;
			}
			Expect(Bad == 0 && Lifted > 0, "3000 scatters, clumps and rows: never on one another nor on a place kept, every stalk whole",
			       std::to_string(Bad) + " wrong of " + std::to_string(Cases) + ", " + std::to_string(Lifted) + " lifted" + (First.empty() ? "" : " (" + First + ")"), "0 wrong, some lifted");
			Expect(SubjectOff == 0, "the subject's tag first and on her ship wherever her ship's place is clear", std::to_string(SubjectOff) + " off", "0");
			Expect(Unsteady == 0, "the same tags stand the same however they come", std::to_string(Unsteady) + " otherwise", "0");
		}
	}

	// ---- The recordings --------------------------------------------------------------------------------------------

	constexpr double ScreenW = 1600.0, ScreenH = 900.0;
	double Radians(double Degrees) { return Degrees * 3.14159265358979323846 / 180.0; }

	/** An eye as ShotEye puts the camera (board metres: X east, Y north, Z up), its lens across the screen's width, and the
	 * piece of its subject (-1 none). */
	struct Look
	{
		const char* Name;
		double From[3], To[3];
		double Lens;
		int Subject;
	};

	/** An eye's axes in board metres as the scene has them (EyeFacing, the look's rotator with no roll: its quaternion's
	 * forward, right and up vectors, through World's X and Y swapped): the look F; the right Rt, level; the up Up, the
	 * right across the look, square to both, tilted forward as the eye looks down. */
	struct Axes
	{
		double F[3], Rt[3], Up[3];
	};
	Axes AxesOf(const Look& Eye)
	{
		Axes A;
		for (int C = 0; C < 3; ++C) A.F[C] = Eye.To[C] - Eye.From[C];
		const double FL = std::sqrt(A.F[0] * A.F[0] + A.F[1] * A.F[1] + A.F[2] * A.F[2]);
		for (double& C : A.F) C /= FL;
		const double RL = std::hypot(A.F[0], A.F[1]);
		A.Rt[0] = A.F[1] / RL;
		A.Rt[1] = -A.F[0] / RL;
		A.Rt[2] = 0.0;
		// (Rt x F: (-Fz Fx, -Fz Fy) / |Fxy| across, |Fxy| up)
		A.Up[0] = A.Rt[1] * A.F[2] - A.Rt[2] * A.F[1];
		A.Up[1] = A.Rt[2] * A.F[0] - A.Rt[0] * A.F[2];
		A.Up[2] = A.Rt[0] * A.F[1] - A.Rt[1] * A.F[0];
		return A;
	}

	/** Where a point (board metres) stands through an eye, as ASeaBattle::PlaceTorpedoWords puts it: across and down the
	 * screen, 0 to 1 on it, and its depth along the look. */
	void Through(const Look& Eye, const Axes& A, const double P[3], double& Across, double& Down, double& Deep)
	{
		const double O[3] = { P[0] - Eye.From[0], P[1] - Eye.From[1], P[2] - Eye.From[2] };
		const double HalfWide = std::tan(Radians(Eye.Lens * 0.5));
		Deep = O[0] * A.F[0] + O[1] * A.F[1] + O[2] * A.F[2];
		Across = 0.5 + 0.5 * (O[0] * A.Rt[0] + O[1] * A.Rt[1] + O[2] * A.Rt[2]) / (Deep * HalfWide);
		Down = 0.5 - 0.5 * (ScreenW / ScreenH) * (O[0] * A.Up[0] + O[1] * A.Up[1] + O[2] * A.Up[2]) / (Deep * HalfWide);
	}

	/** The tags' feet through an eye, as ASeaBattle::PlaceTorpedoWords finds them: over the top of each tagged piece's
	 * leading ship, drawn, in front of the eye and on a 1600 by 900 screen. */
	std::vector<SeaWords::Spot> SpotsThrough(const SeaShow::Show& Show, const SeaWords::Words& W, const Look& Eye)
	{
		const Axes A = AxesOf(Eye);
		std::vector<SeaWords::Spot> Spots;
		for (const SeaWords::Tag& Tg : W.Tags)
		{
			const SeaShow::ShipOut* Lead = Show.Ship(Show.LeaderOf(Tg.Piece));
			if (!Lead || !Lead->bShown) continue;
			const double Top[3] = { Lead->At.X, Lead->At.Y, std::max(Lead->At.Z, 0.0) + Lead->Heave + 6.0 + 0.12 * Lead->Long };
			double Across = 0.0, Down = 0.0, Deep = 0.0;
			Through(Eye, A, Top, Across, Down, Deep);
			if (Deep < 1.0) continue;
			if (Across < 0.0 || Across > 1.0 || Down < 0.0 || Down > 1.0) continue;
			Spots.push_back(SpotAt(Across * ScreenW, Down * ScreenH, PlateWide(Tg.Text), Deep, Tg.Piece, Tg.Piece == Eye.Subject));
		}
		return Spots;
	}

	/** The eyes the tags are put through, as the scene's: the axes FRotationMatrix gives the look's rotator (pitch P, yaw Y,
	 * no roll; forward (cP cY, cP sY, sP), right (-sY, cY, 0), up (-sP cY, -sP sY, cP) in the engine's X north, Y east),
	 * square to one another; the point looked at at the screen's very centre, a point over it above the centre and one to
	 * the right of it right of the centre. */
	void Eyes()
	{
		std::printf("the shots' eyes, the axes as the scene's\n");
		const Look All[] = {
			// (the overview on light-ships, high and looking down across; a Fleets shot, 55 m up; the Line shot, 20 m up; a level
			// eye; one looking steeply down; one looking down the board's north)
			{ "the overview", { 12295.0, 7810.0, 1300.0 }, { 9324.0, 10573.0, 0.0 }, 55.0, -1 },
			{ "a Fleets shot", { 8406.0, 9228.0, 55.0 }, { 8700.0, 9850.0, 0.0 }, 45.0, -1 },
			{ "the Line shot", { 6000.0, 6000.0, 20.0 }, { 5500.0, 6300.0, 8.0 }, 40.0, -1 },
			{ "a level eye", { 0.0, 0.0, 10.0 }, { 100.0, 50.0, 10.0 }, 55.0, -1 },
			{ "a steep eye", { 1000.0, 1000.0, 2000.0 }, { 1100.0, 1200.0, 0.0 }, 55.0, -1 },
			{ "an eye looking north, down", { 500.0, 0.0, 300.0 }, { 500.0, 1000.0, 0.0 }, 55.0, -1 },
		};
		for (const Look& Eye : All)
		{
			const Axes A = AxesOf(Eye);
			// (the scene's: the look in the engine's axes, World's X and Y swapped; its rotator's axes; back to the board's)
			const double EX = A.F[1], EY = A.F[0], EZ = A.F[2];
			const double Yaw = std::atan2(EY, EX), Pitch = std::atan2(EZ, std::hypot(EX, EY));
			const double SP = std::sin(Pitch), CP = std::cos(Pitch), SY = std::sin(Yaw), CY = std::cos(Yaw);
			const double SceneF[3] = { CP * SY, CP * CY, SP }, SceneRt[3] = { CY, -SY, 0.0 }, SceneUp[3] = { -SP * SY, -SP * CY, CP };
			double Most = 0.0, Square = 0.0;
			for (int C = 0; C < 3; ++C)
			{
				Most = std::max({ Most, std::fabs(A.F[C] - SceneF[C]), std::fabs(A.Rt[C] - SceneRt[C]), std::fabs(A.Up[C] - SceneUp[C]) });
			}
			Square = std::max({ std::fabs(A.Up[0] * A.F[0] + A.Up[1] * A.F[1] + A.Up[2] * A.F[2]), std::fabs(A.Up[0] * A.Rt[0] + A.Up[1] * A.Rt[1] + A.Up[2] * A.Rt[2]),
			                    std::fabs(A.Up[0] * A.Up[0] + A.Up[1] * A.Up[1] + A.Up[2] * A.Up[2] - 1.0) });
			char Got[160];
			std::snprintf(Got, sizeof Got, "up %.4f %.4f %.4f, up.look %.2e, off the scene's by %.2e", A.Up[0], A.Up[1], A.Up[2],
			              A.Up[0] * A.F[0] + A.Up[1] * A.F[1] + A.Up[2] * A.F[2], Most);
			Expect(Most < 1e-9 && Square < 1e-9, std::string(Eye.Name) + ": the up square to the look, the axes the scene's", Got, "square, the scene's");
			double Across = 0.0, Down = 0.0, Deep = 0.0, OverAcross = 0.0, OverDown = 0.0, RightAcross = 0.0, RightDown = 0.0;
			Through(Eye, A, Eye.To, Across, Down, Deep);
			const double Over[3] = { Eye.To[0], Eye.To[1], Eye.To[2] + 10.0 };
			Through(Eye, A, Over, OverAcross, OverDown, Deep);
			const double Right[3] = { Eye.To[0] + 10.0 * A.Rt[0], Eye.To[1] + 10.0 * A.Rt[1], Eye.To[2] };
			Through(Eye, A, Right, RightAcross, RightDown, Deep);
			std::snprintf(Got, sizeof Got, "looked at %.6f %.6f; over it %.4f %.4f; right of it %.4f %.4f", Across, Down, OverAcross, OverDown, RightAcross, RightDown);
			Expect(std::fabs(Across - 0.5) < 1e-9 && std::fabs(Down - 0.5) < 1e-9 && std::fabs(OverAcross - 0.5) < 1e-9 && OverDown < 0.5 && RightAcross > 0.5 &&
			           std::fabs(RightDown - 0.5) < 1e-9,
			       std::string(Eye.Name) + ": the point looked at at the centre, over it above, right of it right", Got, "0.5 0.5; above; right");
		}
	}

	/** A piece as the views' JSON says it, read here apart from SeaFeed: its side, ships and whether it is routing (its
	 * state) as last told, and whether its view sets the words' fields (heldFor; clearFrom with its clearUntil), with their
	 * names, ordered, and the second the heads pass her (clearUntil). */
	struct RawPiece
	{
		int Side = 0;
		int Ships = 1;
		bool bHeld = false, bClear = false;
		bool bHeldNamed = false, bClearNamed = false, bOrdered = false, bRouting = false;
		double ClearUntil = -1.0;
		std::string ClearName;
	};
	struct RawView
	{
		double T = 0.0;
		std::map<int, RawPiece> Pieces;
	};
	/** What the views set, all told, read apart from SeaFeed to hold its reading to: the piece-views holding and turning
	 * clear, and the views with any; those without a name; ordered; turning clear past their clearUntil; turning clear
	 * while routing; each side's pieces given a field in some view (0 the attacker, 1 the defender); and the turns clear the
	 * views end before their clearUntil (the second the rules let her go, the heads past her), with the most seconds the
	 * count had left then. And, for the frame-by-frame checks to be seen to meet them: the piece-views with a field and one
	 * ship afloat, and those with a field under an order, each side's. */
	struct RawCount
	{
		long Held = 0, Clear = 0, HeldViews = 0, ClearViews = 0, HeldUnnamed = 0, ClearUnnamed = 0, Ordered = 0, PastUntil = 0, EndedEarly = 0, Alone = 0,
		     ClearRouting = 0;
		long OrderedOn[2] = { 0, 0 };
		double MostLeft = 0.0;
		std::set<int> Fielded[2];
	};

	/** The recording as the views' JSON says it, apart from SeaFeed. */
	bool ReadRaw(const std::string& Text, std::vector<RawView>& Out, std::string& Me, bool& bAnyFields, RawCount& Count)
	{
		AirFeed::Json J;
		std::string Error;
		if (!AirFeed::Parse(Text, J, Error)) return false;
		const AirFeed::Json& Views = J["views"];
		Me = Views.At(0)["me"].Str("att");
		bAnyFields = false;
		Count = RawCount();
		std::map<int, RawPiece> Kept;
		for (size_t K = 0; K < Views.Count(); ++K)
		{
			const AirFeed::Json& V = Views.At(K);
			RawView R;
			R.T = V["t"].Num();
			bool bHeldHere = false, bClearHere = false;
			const AirFeed::Json& Units = V["units"];
			for (size_t U = 0; U < Units.Count(); ++U)
			{
				const AirFeed::Json& Unit = Units.At(U);
				const AirFeed::Json& X = Unit["extra"];
				const int Id = Unit["id"].Int();
				RawPiece P = Kept.count(Id) ? Kept[Id] : RawPiece();
				if (Unit["side"].IsString()) P.Side = Unit["side"].Str() == "def" ? 1 : 0;
				if (Unit["state"].IsString()) P.bRouting = Unit["state"].Str() == "routing";
				P.Ships = std::clamp(X["ships"].IsNumber() ? X["ships"].Int() : Unit["figures"].Int(P.Ships), 0, 64);
				P.bHeld = X["heldFor"].IsNumber() && X["heldFor"].Int() >= 0;
				P.bClear = X["clearFrom"].IsNumber() && X["clearFrom"].Int() >= 0 && X["clearUntil"].IsNumber();
				P.bHeldNamed = P.bHeld && X["heldForName"].IsString() && !X["heldForName"].Str().empty();
				P.bClearNamed = P.bClear && X["clearFromName"].IsString() && !X["clearFromName"].Str().empty();
				P.bOrdered = X["ordered"].Truth(false);
				P.ClearUntil = P.bClear ? X["clearUntil"].Num() : -1.0;
				P.ClearName = P.bClearNamed ? X["clearFromName"].Str() : std::string();
				bAnyFields = bAnyFields || !X["heldFor"].IsNull() || !X["clearFrom"].IsNull() || !X["clearUntil"].IsNull() || !X["heldForName"].IsNull() ||
				             !X["clearFromName"].IsNull() || !X["ordered"].IsNull();
				if (P.bHeld)
				{
					++Count.Held;
					Count.HeldUnnamed += P.bHeldNamed ? 0 : 1;
					bHeldHere = true;
				}
				if (P.bClear)
				{
					++Count.Clear;
					Count.ClearUnnamed += P.bClearNamed ? 0 : 1;
					Count.PastUntil += R.T > P.ClearUntil + 1e-9 ? 1 : 0;
					Count.ClearRouting += P.bRouting ? 1 : 0;
					bClearHere = true;
				}
				Count.Ordered += P.bOrdered ? 1 : 0;
				if (P.bHeld || P.bClear)
				{
					Count.Fielded[P.Side].insert(Id);
					Count.Alone += P.Ships == 1 ? 1 : 0;
					Count.OrderedOn[P.Side] += P.bOrdered ? 1 : 0;
				}
				Kept[Id] = P;
				R.Pieces[Id] = P;
			}
			Count.HeldViews += bHeldHere ? 1 : 0;
			Count.ClearViews += bClearHere ? 1 : 0;
			Out.push_back(R);
		}
		// (a turn clear the next view ends, or a piece it leaves out, before its clearUntil)
		for (size_t K = 0; K + 1 < Out.size(); ++K)
		{
			for (const auto& Pair : Out[K].Pieces)
			{
				if (!Pair.second.bClear) continue;
				const auto Next = Out[K + 1].Pieces.find(Pair.first);
				if (Next != Out[K + 1].Pieces.end() && Next->second.bClear) continue;
				const double Left = Pair.second.ClearUntil - Out[K + 1].T;
				if (Left > 1e-9)
				{
					++Count.EndedEarly;
					Count.MostLeft = std::max(Count.MostLeft, Left);
				}
			}
		}
		return !Out.empty();
	}

	/** A line a recording must show: at battle time T with piece Piece the camera's subject (a second into a view's two, so
	 * the view reached is never in doubt), its tag and its line (both empty: none). Piece -1: the tags then, each "piece
	 * tag", in the Tag. */
	struct Shown
	{
		double T;
		int Piece;
		const char* Tag;
		const char* Line;
	};

	/** What one of the naval rules' own-water recordings must show (recordings/own-water-r2: the "Friendly torpedoes" switch
	 * on, the computer on both sides, so never "ordered"), known by its key, seed and views, the player Mine (the scene's
	 * own, the views' me, att; or the other side, where its pieces are given fields): its lines string for string;
	 * Quiet, nothing at any frame whatever the subject (its views set no field); Meet, its tags meet through the shots'
	 * eyes, unlaid, and are lifted apart; EnemyOnly, the views give fields to the enemy's pieces only, and none shows.
	 * And, where set (below 0 not), his own turns clear as the views end them: LetGo, those not routing as their field
	 * ends, each ending at its count's 0; LetGoRouting, those routing as it ends (breaking off: no count); RoutingViews,
	 * his piece-views turning clear while routing (read apart from SeaFeed). */
	struct Pinned
	{
		const char* Key;
		uint64_t Seed;
		size_t Views;
		int Mine;
		bool bQuiet, bMeet, bEnemyOnly;
		std::vector<Shown> Lines;
		int LetGo = -1, LetGoRouting = -1, RoutingViews = -1;
	};

	const std::vector<Pinned>& Pins()
	{
		static const char* const Held = "Torpedoes held";
		static const char* const Clear = "Turning clear of torpedoes";
		// (Force H's destroyers, the player's, divisions of four and of two: "their", "they"; the piece in the way as the
		// rules name it, its count in words (one to ten; figures from 11, the log's figures kept), so "are")
		static const char* const For4 = "Holding their torpedoes while four destroyers of Force H are in the line of fire. They fire the moment the water is clear.";
		static const char* const For2 = "Holding their torpedoes while two destroyers of Force H are in the line of fire. They fire the moment the water is clear.";
		// (a division routing has no orders to take up again: it breaks off, no count)
		static const char* const Breaking = "Breaking off, and turning clear of torpedoes fired by four destroyers of Force H.";
		// (two battle lines: the Battle Fleet's Destroyers 3, three afloat, holding for its Destroyers 2)
		static const char* const ForBattle = "Holding their torpedoes while four destroyers of the Battle Fleet are in the line of fire. They fire the moment the water is clear.";
		static const std::vector<Shown> TwoLinesAtt = { { 377.0, 12, "", "" }, { 377.0, -1, "", "" } };
		static const std::vector<Shown> TwoLinesDef = { { 371.0, 12, "", "" }, { 377.0, 12, Held, ForBattle }, { 377.0, -1, "12 Torpedoes held", "" }, { 377.0, 1, "", "" },
		                                                 { 383.0, 12, "", "" } };
		static const std::vector<Pinned> All = {
			// Light ships against battleships: Force H's destroyers hold and turn clear (35 piece-views held, 40 turning clear);
			// the 1st Fleet's none. Destroyers 2 and 3 each hold for the other 382 to 388 s; Destroyers 4 holds and turns
			// clear at once 410 to 416 s (held wins). clearUntil is the second the heads pass her, so each turn clear ends
			// within a view of its count's 0: Destroyers 3 at 408 s (its 0 at 406.1 s, the count read 0 seconds), Destroyers 5
			// at 430 s (430.8 s) and Destroyers 4 at 444 s (443.2 s), both routing from 426 s, so breaking off, no count.
			{ "light-ships", 63591, 289, 0, false, true, false,
			  { { 352.5, 6, "", "" },
			    { 355.0, 6, Held, For4 },
			    { 361.0, 6, "", "" },
			    { 379.0, 2, Held, For4 },
			    { 385.0, 2, Held, For4 },
			    { 385.0, 3, Held, For4 },
			    { 393.0, 4, Held, For2 },
			    { 393.0, 5, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 36 seconds, once the torpedoes have passed." },
			    { 393.0, -1, "3 Torpedoes held, 4 Torpedoes held, 5 Turning clear of torpedoes", "" },
			    { 393.0, 1, "", "" },
			    { 393.0, 16, "", "" },
			    { 401.0, 3, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 2 seconds, once the torpedoes have passed." },
			    { 405.0, 3, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 1 second, once the torpedoes have passed." },
			    { 407.0, 3, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 0 seconds, once the torpedoes have passed." },
			    { 409.0, 3, "", "" },
			    { 411.0, 4, Held, For2 },
			    { 415.0, 4, Held, For4 },
			    { 419.0, 4, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 21 seconds, once the torpedoes have passed." },
			    { 425.0, 4, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 18 seconds, once the torpedoes have passed." },
			    { 425.0, 5, Clear, "Turning clear of torpedoes fired by four destroyers of Force H. They take up their orders again in 7 seconds, once the torpedoes have passed." },
			    { 427.0, -1, "4 Turning clear of torpedoes, 5 Turning clear of torpedoes", "" },
			    { 427.0, 4, Clear, Breaking },
			    { 427.0, 5, Clear, Breaking },
			    { 429.0, 5, Clear, Breaking },
			    { 431.0, 5, "", "" },
			    { 441.0, 4, Clear, Breaking },
			    { 443.0, 4, Clear, Breaking },
			    { 443.0, -1, "4 Turning clear of torpedoes", "" },
			    { 445.0, 4, "", "" },
			    { 445.0, -1, "", "" } },
			  1, 2, 11 },
			// Two battle lines, and the same with the sailors' rules: only the Battle Fleet's Destroyers 3 holds (372 to 380 s,
			// for its Destroyers 2), the enemy's as the scene takes the player (att); his own as the defender.
			{ "two-lines", 23996, 305, 0, false, false, true, TwoLinesAtt },
			{ "two-lines", 23996, 305, 1, false, false, false, TwoLinesDef },
			{ "two-lines-sailors", 71510, 315, 0, false, false, true, TwoLinesAtt },
			{ "two-lines-sailors", 71510, 315, 1, false, false, false, TwoLinesDef },
			// The action off Samar, and the same with the sailors' rules: recorded with the switch on, no piece holds or turns
			// clear, and nothing shows.
			{ "samar", 16077, 220, 0, true, false, false, {} },
			{ "samar-sailors", 16077, 220, 0, true, false, false, {} },
		};
		return All;
	}

	/** The pins for a recording with the player Mine (null: none). */
	const Pinned* PinFor(const SeaFeed::Recording& Rec, int Mine)
	{
		for (const Pinned& P : Pins())
		{
			if (Rec.Key == P.Key && Rec.Seed == P.Seed && Rec.Views.size() == P.Views && P.Mine == Mine) return &P;
		}
		return nullptr;
	}

	/** The tags of the moment, "piece tag", in order. */
	std::string TagsOf(const SeaWords::Words& W)
	{
		std::string Out;
		for (const SeaWords::Tag& T : W.Tags) Out += (Out.empty() ? "" : ", ") + std::to_string(T.Piece) + " " + T.Text;
		return Out;
	}

	/** A line pinned for the moment shown: its tag and its line with its piece the subject (or, Piece -1, the tags). */
	void CheckShown(const std::map<int, SeaShow::Piece>& Pieces, int Mine, double Now, double T, const Shown& C, const std::string& Who)
	{
		char Name[128];
		std::snprintf(Name, sizeof Name, "%sat %.1f s, %s", Who.c_str(), C.T, C.Piece >= 0 ? ("piece " + std::to_string(C.Piece) + " the subject:").c_str() : "the tags:");
		Expect(std::fabs(T - C.T) < 1e-9, std::string(Name) + " on a frame", std::to_string(T), "on it");
		const SeaWords::Words On = SeaWords::Write(Pieces, Mine, C.Piece, Now);
		if (C.Piece < 0)
		{
			Same(std::string(Name) + " each", TagsOf(On), C.Tag);
			return;
		}
		std::string Tag;
		for (const SeaWords::Tag& Tg : On.Tags)
			if (Tg.Piece == C.Piece) Tag = Tg.Text;
		Same(std::string(Name) + " the tag", Tag, C.Tag);
		Same(std::string(Name) + " the line", On.Line, C.Line);
	}

	// ---- The fields as SeaFeed reads them and the show carries them: views written here ------------------------------

	/** Six views written here (100 to 110 s), for what the computer's sides never record (the own-water recordings are
	 * fought by the computer on both sides): the player's order, a ship alone holding and turning clear, a division
	 * turning clear under his order, held winning over turning clear under his order, the count's last second, a turn
	 * clear the views keep past its salvo's landing and a turn clear without its clearFromName; what SeaFeed leaves
	 * unread (a turn clear without its clearUntil, an id below 0); a view without the fields clearing them, ordered too;
	 * and the enemy's pieces holding and turning clear under an order. Each piece's fields as the rules write them into
	 * its extra. */
	std::string WrittenViews()
	{
		struct Is
		{
			int Id;
			const char* Side;
			const char* Kind;
			const char* Name;
			int Ships;
			double X, Y;
		};
		const Is Pieces[] = { { 1, "att", "dd", "Destroyers 1", 4, 9000.0, 9000.0 },   { 2, "att", "ca", "Cruiser 2", 1, 9000.0, 9600.0 },
		                      { 3, "att", "dd", "Destroyers 3", 2, 9400.0, 9000.0 },   { 4, "att", "dd", "Destroyers 4", 2, 9800.0, 9000.0 },
		                      { 5, "att", "ca", "Cruiser 3", 1, 9400.0, 9600.0 },      { 11, "def", "dd", "Destroyers 2", 4, 11000.0, 9000.0 },
		                      { 12, "def", "ca", "Cruiser 1", 1, 11000.0, 9600.0 } };
		auto Fields = [](double T, int Id) -> std::string
		{
			switch (Id)
			{
				// (Destroyers 1, a division: holds for Cruiser 3 under his order, 100 and 102 s; nothing after)
				case 1: return T <= 102.0 ? ",\"heldFor\":5,\"heldForName\":\"a cruiser of the Home Fleet\",\"ordered\":true" : "";
				// (Cruiser 2, a ship alone: turns clear of Destroyers 1's salvo, landing at 105.4 s, under his order to 102 s;
				// the views keep the turn past the landing)
				case 2: return std::string(",\"clearFrom\":1,\"clearUntil\":105.4,\"clearFromName\":\"4 destroyers of the Home Fleet\"") + (T <= 102.0 ? ",\"ordered\":true" : "");
				// (Destroyers 3, a division of two: a turn clear without its clearUntil at 100 s, held for an id below 0 at 102 s,
				// turning clear under his order at 104 s, its salvo landing at 115.4 s, and let go at 106 s)
				case 3:
					return T <= 100.0   ? ",\"clearFrom\":1,\"clearFromName\":\"4 destroyers of the Home Fleet\""
					       : T <= 102.0 ? ",\"heldFor\":-3,\"heldForName\":\"a cruiser of the Home Fleet\""
					       : T <= 104.0 ? ",\"clearFrom\":1,\"clearUntil\":115.4,\"clearFromName\":\"4 destroyers of the Home Fleet\",\"ordered\":true"
					                    : "";
				// (Destroyers 4, a division of two: turns clear of Destroyers 1's salvo without its clearFromName at 102 s, landing
				// at 112.4 s, so the tag and no line; nothing before or after)
				case 4: return T == 102.0 ? ",\"clearFrom\":1,\"clearUntil\":112.4" : "";
				// (Cruiser 3, a ship alone: holds for Destroyers 3 at 104 and 106 s; at 108 and 110 s holds for Destroyers 1 and
				// turns clear of its salvo at once, under his order)
				case 5:
					return T <= 102.0   ? ""
					       : T <= 106.0 ? ",\"heldFor\":3,\"heldForName\":\"2 destroyers of the Home Fleet\""
					                    : ",\"heldFor\":1,\"heldForName\":\"4 destroyers of the Home Fleet\",\"clearFrom\":1,\"clearUntil\":109.4,"
					                      "\"clearFromName\":\"4 destroyers of the Home Fleet\",\"ordered\":true";
				// (the enemy's: Destroyers 2 holds for Cruiser 1, Cruiser 1 turns clear of Destroyers 2's salvo, both ordered)
				case 11: return ",\"heldFor\":12,\"heldForName\":\"a cruiser of the Battle Fleet\",\"ordered\":true";
				case 12: return ",\"clearFrom\":11,\"clearUntil\":130.4,\"clearFromName\":\"4 destroyers of the Battle Fleet\",\"ordered\":true";
				default: return "";
			}
		};
		std::string Out = "{\"what\":\"views written by sea_words_test\",\"recording\":true,\"key\":\"written-views\",\"seed\":239,\"step\":2,"
		                  "\"field\":{\"size\":20000,\"edge\":5000,\"zoneName\":\"North Sea\",\"title\":\"Views written here\",\"date\":\"Practice\","
		                  "\"att\":{\"nat\":\"UK\",\"adj\":\"British\"},\"def\":{\"nat\":\"GER\",\"adj\":\"German\"},"
		                  "\"classes\":{\"ca\":{\"len\":180,\"beam\":20,\"speed\":13,\"turn\":0.08,\"reach\":5000},\"dd\":{\"len\":110,\"beam\":11,\"speed\":14,\"turn\":0.12,\"reach\":5000}},"
		                  "\"torp\":{\"travel\":75,\"radius\":800}},\"views\":[";
		char Buf[640];
		for (int K = 0; K < 6; ++K)
		{
			const double T = 100.0 + 2.0 * K;
			std::snprintf(Buf, sizeof Buf, "%s{\"t\":%.0f,\"limit\":1500,\"speed\":1,\"me\":\"att\",\"units\":[", K ? "," : "", T);
			Out += Buf;
			for (size_t P = 0; P < sizeof Pieces / sizeof Pieces[0]; ++P)
			{
				const Is& I = Pieces[P];
				std::snprintf(Buf, sizeof Buf,
				              "%s{\"id\":%d,\"side\":\"%s\",\"kind\":\"%s\",\"name\":\"%s\",\"nat\":\"%s\",\"x\":%.1f,\"y\":%.1f,\"face\":0,\"str\":1,\"mor\":1,"
				              "\"state\":\"steady\",\"figures0\":%d,\"figures\":%d,\"act\":\"fight\",\"speed\":5,\"engaged\":-1,\"extra\":{\"cls\":\"%s\",\"ships0\":%d,\"ships\":%d",
				              P ? "," : "", I.Id, I.Side, I.Kind, I.Name, I.Side[0] == 'a' ? "UK" : "GER", I.X, I.Y + 10.0 * K, I.Ships, I.Ships, I.Kind, I.Ships, I.Ships);
				Out += Buf;
				Out += Fields(T, I.Id) + "}}";
			}
			Out += "]}";
		}
		return Out + "]}";
	}

	std::string Told(const SeaShow::Piece* P)
	{
		if (!P) return "(not in the view)";
		char Buf[256];
		std::snprintf(Buf, sizeof Buf, "held for %d \"%s\", clear of %d until %.1f \"%s\", %s", P->HeldFor, P->HeldForName.c_str(), P->ClearFrom, P->ClearUntil,
		              P->ClearFromName.c_str(), P->bOrdered ? "ordered" : "not ordered");
		return Buf;
	}

	void Feed()
	{
		std::printf("the fields as SeaFeed reads them and the show carries them (views written here: his order, a ship alone, the last second, past the landing, unnamed)\n");
		SeaFeed::Recording Rec;
		std::string Error;
		if (!SeaFeed::ReadRecording(WrittenViews(), Rec, Error) || Rec.Views.size() != 6)
		{
			Expect(false, "the views written here read", Error.empty() ? std::to_string(Rec.Views.size()) + " views" : Error, "6 views");
			return;
		}
		auto PieceIn = [&Rec](size_t View, int Id) -> const SeaShow::Piece*
		{
			for (const SeaShow::Piece& P : Rec.Views[View].Pieces)
				if (P.Id == Id) return &P;
			return nullptr;
		};
		{
			const SeaShow::Piece* P = PieceIn(0, 1);
			Expect(P && P->HeldFor == 5 && P->HeldForName == "a cruiser of the Home Fleet" && P->bOrdered && P->ClearFrom < 0, "held, under his order: read", Told(P),
			       "held for 5, its name, ordered");
			P = PieceIn(0, 2);
			Expect(P && P->ClearFrom == 1 && std::fabs(P->ClearUntil - 105.4) < 1e-9 && P->ClearFromName == "4 destroyers of the Home Fleet" && P->bOrdered && P->HeldFor < 0,
			       "turning clear, under his order: read", Told(P), "clear of 1 until 105.4, its name, ordered");
			P = PieceIn(0, 3);
			Expect(P && P->ClearFrom == -1 && P->ClearFromName.empty() && P->ClearUntil < 0.0, "turning clear without its clearUntil: not read", Told(P), "none");
			P = PieceIn(1, 3);
			Expect(P && P->HeldFor == -1 && P->HeldForName.empty(), "held for an id below 0: none", Told(P), "none");
			P = PieceIn(1, 4);
			Expect(P && P->ClearFrom == 1 && std::fabs(P->ClearUntil - 112.4) < 1e-9 && P->ClearFromName.empty() && !P->bOrdered && P->HeldFor < 0,
			       "turning clear without its clearFromName: read, the name empty", Told(P), "clear of 1 until 112.4, no name, not ordered");
			P = PieceIn(2, 1);
			Expect(P && P->HeldFor == -1 && P->HeldForName.empty() && !P->bOrdered, "a view without the fields clears them, ordered too", Told(P), "none, not ordered");
			P = PieceIn(2, 2);
			Expect(P && P->ClearFrom == 1 && P->ClearFromName == "4 destroyers of the Home Fleet" && !P->bOrdered, "his order off, the turn clear standing: read", Told(P),
			       "clear of 1, not ordered");
			P = PieceIn(4, 2);
			Expect(P && P->ClearFrom == 1 && std::fabs(P->ClearUntil - 105.4) < 1e-9, "a turn clear the views keep past its salvo's landing: read", Told(P), "clear of 1 until 105.4");
			P = PieceIn(4, 5);
			Expect(P && P->HeldFor == 1 && P->ClearFrom == 1 && std::fabs(P->ClearUntil - 109.4) < 1e-9 && P->bOrdered, "held and turning clear at once, under his order: both read",
			       Told(P), "held for 1, clear of 1 until 109.4, ordered");
			P = PieceIn(0, 11);
			Expect(P && P->HeldFor == 12 && P->bOrdered, "the enemy's held, ordered: read (the words leave it)", Told(P), "held for 12, ordered");
		}
		// Through the show, as the recordings are played (32 frames a second), the words of the moment shown.
		static const char* const Held = "Torpedoes held";
		static const char* const Clear = "Turning clear of torpedoes";
		const std::vector<Shown> Att = {
			{ 101.0, -1, "1 Torpedoes held, 2 Turning clear of torpedoes", "" },
			{ 101.0, 1, Held, "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear." },
			{ 101.0, 2, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Home Fleet. She takes up your orders again in 5 seconds, once the torpedoes have passed." },
			{ 101.0, 3, "", "" },
			{ 101.0, 11, "", "" },
			{ 101.0, 12, "", "" },
			{ 103.0, -1, "1 Torpedoes held, 2 Turning clear of torpedoes, 4 Turning clear of torpedoes", "" },
			{ 103.0, 2, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Home Fleet. She takes up your orders again in 3 seconds, once the torpedoes have passed." },
			{ 103.0, 3, "", "" },
			{ 103.0, 4, Clear, "" },
			{ 104.5, -1, "2 Turning clear of torpedoes, 3 Turning clear of torpedoes, 5 Torpedoes held", "" },
			{ 104.5, 1, "", "" },
			{ 104.5, 4, "", "" },
			{ 104.5, 2, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Home Fleet. She takes up her orders again in 1 second, once the torpedoes have passed." },
			{ 104.5, 3, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Home Fleet. They take up your orders again in 11 seconds, once the torpedoes have passed." },
			{ 104.5, 5, Held, "Holding her torpedoes while 2 destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear." },
			{ 106.5, -1, "2 Turning clear of torpedoes, 5 Torpedoes held", "" },
			{ 106.5, 2, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Home Fleet. She takes up her orders again in 0 seconds, once the torpedoes have passed." },
			{ 106.5, 3, "", "" },
			{ 108.5, -1, "2 Turning clear of torpedoes, 5 Torpedoes held", "" },
			{ 108.5, 5, Held, "Holding her torpedoes while 4 destroyers of the Home Fleet are in the line of fire. Your order stands: she fires the moment the water is clear." },
		};
		const std::vector<Shown> Def = {
			{ 101.0, -1, "11 Torpedoes held, 12 Turning clear of torpedoes", "" },
			{ 101.0, 11, Held, "Holding their torpedoes while a cruiser of the Battle Fleet is in the line of fire. Your order stands: they fire the moment the water is clear." },
			{ 101.0, 12, Clear, "Turning clear of torpedoes fired by 4 destroyers of the Battle Fleet. She takes up your orders again in 30 seconds, once the torpedoes have passed." },
			{ 101.0, 1, "", "" },
			{ 101.0, 2, "", "" },
			{ 103.0, 4, "", "" },
		};
		for (const int Mine : { 0, 1 })
		{
			SeaShow::Show Show(Rec.Seed);
			Show.SetField(Rec.Board);
			Show.SetMine(Mine);
			Show.Eye = SeaShow::Vec(10000.0, 7000.0, 900.0);
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& F = Rec.Views[Taken++];
				Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
			};
			TakeNext();
			TakeNext();
			const std::vector<Shown>& Want = Mine == 0 ? Att : Def;
			size_t Next = 0;
			for (long K = 0; Next < Want.size(); ++K)
			{
				const double T = Rec.Views.front().Head.T + static_cast<double>(K) / 32.0;
				if (T > Rec.Views.back().Head.T + 1e-9) break;
				while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
				Show.Advance(T);
				while (Next < Want.size() && Want[Next].T <= T + 1e-9) CheckShown(Show.Pieces(), Mine, Show.Shown(), T, Want[Next++], Mine == 0 ? "written, " : "written, the player def, ");
			}
			Expect(Next == Want.size(), std::string("written, the player ") + (Mine == 0 ? "att" : "def") + ": every line checked",
			       std::to_string(Next) + " of " + std::to_string(Want.size()), "all");
		}
	}

	// ---- The recordings, played ----------------------------------------------------------------------------------

	/** The ships as the show draws them, for two shows to be held the same. */
	std::string ShipsOf(const SeaShow::Show& Show)
	{
		std::string Out;
		char Buf[160];
		for (const SeaShow::ShipOut& S : Show.Ships())
		{
			std::snprintf(Buf, sizeof Buf, "%d %.9f %.9f %.9f %.9f %d;", S.Key, S.At.X, S.At.Y, S.At.Z, S.Heading, static_cast<int>(S.Doing));
			Out += Buf;
		}
		return Out;
	}

	/** One play of a recording through the show, the player Mine (the scene's own, the first view's me; or the other side,
	 * where the views give fields to its pieces), and what is pinned for it then (null: nothing). */
	void PlayAs(const SeaFeed::Recording& Rec, const std::vector<RawView>& Raw, bool bFields, int Mine, const Pinned* Pin)
	{
		// The same recording with every field taken off: the show must be the same, ship for ship (the fields change
		// nothing but the words).
		SeaFeed::Recording Bare = Rec;
		for (SeaFeed::Frame& F : Bare.Views)
			for (SeaShow::Piece& P : F.Pieces)
			{
				P.HeldFor = P.ClearFrom = -1;
				P.HeldForName.clear();
				P.ClearFromName.clear();
				P.ClearUntil = -1.0;
				P.bOrdered = false;
			}

		SeaShow::Show Show(Rec.Seed ? Rec.Seed : 239), BareShow(Rec.Seed ? Rec.Seed : 239);
		for (SeaShow::Show* S : { &Show, &BareShow })
		{
			S->SetField(Rec.Board);
			S->SetMine(Mine);
			const double Mid = Rec.Board.Size * 0.5;
			S->Eye = SeaShow::Vec(Mid, Mid - 3000.0, 900.0);
		}
		size_t Taken = 0;
		auto TakeNext = [&]()
		{
			const SeaFeed::Frame& F = Rec.Views[Taken];
			const SeaFeed::Frame& B = Bare.Views[Taken];
			Show.Take(F.Head.T, F.Pieces, F.Events, F.bZones ? &F.Zones : nullptr, F.Head.Contact);
			if (bFields) BareShow.Take(B.Head.T, B.Pieces, B.Events, B.bZones ? &B.Zones : nullptr, B.Head.Contact);
			++Taken;
		};
		TakeNext();
		if (Taken < Rec.Views.size()) TakeNext();

		std::set<std::string> Names;
		for (const SeaFeed::Frame& F : Rec.Views)
			for (const SeaShow::Piece& P : F.Pieces)
				if (!P.Name.empty()) Names.insert(P.Name);
		// (each of his own pieces some view gives a field, with a ship afloat then: each must be tagged)
		std::set<int> OwnFielded;
		for (const RawView& V : Raw)
			for (const auto& Pair : V.Pieces)
				if ((Pair.second.bHeld || Pair.second.bClear) && Pair.second.Side == Mine && Pair.second.Ships >= 1) OwnFielded.insert(Pair.first);

		// The show's own fixed step: within it of a view's second, the view the picture has reached is in doubt (the show
		// takes it at its own step, not the frame's), so those frames are not held to the views read apart.
		constexpr double Doubt = 0.06;
		const double T0 = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		long Frames = 0, Judged = 0, TagFrames = 0, Tags = 0, Lines = 0, Subjects = 0, TagsOff = 0, TextOff = 0, LinesOff = 0, EnemySet = 0, EnemySaid = 0, Unled = 0, Dirty = 0,
		     Named = 0, Apart = 0;
		std::map<std::string, long> Looks, RawOn, LiftedLooks, SpotsOn, TwoOn;
		long LaidLifted = 0, LaidWrong = 0, LookSubjectOff = 0;
		std::string FirstLaidWrong;
		std::string FirstOff;
		std::map<int, long> TaggedFrames;
		// His own turns clear, each a spell of the frames the show carries its field, one after another: the last count its
		// line showed (and any below 0), where the field ended against the count's 0 (clearUntil), and whether she was
		// routing as it ended.
		struct Spell
		{
			int Piece = -1;
			double From = 0.0, Last = 0.0, End = -1.0, Until = -1.0;
			long Count = -1, Counts = 0, Below = 0;
			bool bRouting = false;
		};
		std::map<int, Spell> Turning;
		std::vector<Spell> Turned;
		// Each of his pieces turning clear (not held) with its name, as the views read apart have it: routing, exactly the
		// breaking-off line; not routing, the line with its count and orders, never that one. A turning-clear line where
		// the views read apart set no turn clear (after the field ends). And the name each line gives, by its count: one to
		// ten in words, figures only from 11 (figures to 10, or a count past ten in words, are off).
		long RoutingFrames = 0, RoutingOff = 0, SteadyFrames = 0, SteadyOff = 0, ClearAfter = 0, NamesInWords = 0, NamesFigures = 0, NamesOff = 0, NamesUncounted = 0;
		std::set<std::pair<size_t, int>> RoutingViews;
		std::string FirstTurnOff, FirstNameOff;
		size_t NextCheck = 0;
		std::vector<Shown> Checks = Pin ? Pin->Lines : std::vector<Shown>();
		std::stable_sort(Checks.begin(), Checks.end(), [](const Shown& L, const Shown& R) { return L.T < R.T; });
		const std::string Who = Mine != SeaFeed::SideOf(Rec.Views.front().Head.Me) ? std::string("the player ") + (Mine == 1 ? "def" : "att") + ", " : std::string();
		size_t RawAt = 0;
		for (long K = 0;; ++K)
		{
			// (32 frames a second: a frame's second is exact, so the pinned lines fall on frames)
			const double T = T0 + static_cast<double>(K) / 32.0;
			if (T > EndT + 1e-9) break;
			while (Taken < Rec.Views.size() && Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
			Show.Advance(T);
			if (bFields) BareShow.Advance(T);
			++Frames;
			const double Now = Show.Shown();
			const std::map<int, SeaShow::Piece>& Pieces = Show.Pieces();
			const SeaWords::Words W = SeaWords::Write(Pieces, Mine, -1, Now);
			if (!W.Line.empty()) ++LinesOff;
			if (!W.Tags.empty()) ++TagFrames;
			Tags += static_cast<long>(W.Tags.size());
			// The tags against the views read apart: his own pieces with a field set in the view reached, and nothing else.
			while (RawAt + 1 < Raw.size() && Raw[RawAt + 1].T <= T + 1e-9) ++RawAt;
			const bool bDoubt = T - Raw[RawAt].T < Doubt;
			std::map<int, std::string> Want;
			for (const auto& Pair : Raw[RawAt].Pieces)
			{
				const RawPiece& P = Pair.second;
				if (!(P.bHeld || P.bClear)) continue;
				if (P.Side != Mine) { ++EnemySet; continue; }
				if (P.Ships >= 1) Want[Pair.first] = P.bHeld ? SeaWords::HeldTag : SeaWords::ClearingTag;
			}
			std::map<int, std::string> Got;
			for (const SeaWords::Tag& Tg : W.Tags)
			{
				Got[Tg.Piece] = Tg.Text;
				++TaggedFrames[Tg.Piece];
				const auto Was = Pieces.find(Tg.Piece);
				if (Was != Pieces.end() && Was->second.Side != Mine) ++EnemySaid;
				// (each tag stands over its piece's leading ship, afloat and drawn)
				const int Lead = Show.LeaderOf(Tg.Piece);
				const SeaShow::ShipOut* Ship = Lead >= 0 ? Show.Ship(Lead) : nullptr;
				if (!Ship || !Ship->bShown) ++Unled;
			}
			if (!bDoubt)
			{
				++Judged;
				bool bOff = false;
				for (const auto& Pair : Want)
				{
					const auto Has = Got.find(Pair.first);
					if (Has == Got.end()) { bOff = true; continue; }
					if (Has->second != Pair.second) ++TextOff;
				}
				for (const auto& Pair : Got)
					if (!Want.count(Pair.first)) bOff = true;
				if (bOff)
				{
					++TagsOff;
					if (FirstOff.empty())
					{
						char Buf[96];
						std::snprintf(Buf, sizeof Buf, "%.3f s: %zu tags for %zu set", T, Got.size(), Want.size());
						FirstOff = Buf;
					}
				}
			}
			// The line with each piece the camera's subject: its own words or none (never another's), clean, naming no ship.
			for (const auto& Pair : Pieces)
			{
				++Subjects;
				const SeaWords::Words On = SeaWords::Write(Pieces, Mine, Pair.first, Now);
				const SeaWords::Said Own = SeaWords::Of(Pair.second, Pieces, Mine, Now);
				if (On.Line != Own.Line || (!On.Line.empty() && On.LinePiece != Pair.first)) ++LinesOff;
				// (a turn clear's line against the views read apart: routing, the breaking-off line exactly; else never it)
				const bool bTurnLine = On.Line.rfind("Turning clear of torpedoes fired by ", 0) == 0 || On.Line.rfind("Breaking off", 0) == 0;
				if (!bDoubt && Pair.second.Side == Mine)
				{
					const auto Read = Raw[RawAt].Pieces.find(Pair.first);
					const RawPiece* R = Read != Raw[RawAt].Pieces.end() ? &Read->second : nullptr;
					bool bOff = false;
					if (R && R->bClear && !R->bHeld && R->bClearNamed && R->Ships >= 1)
					{
						if (R->bRouting)
						{
							++RoutingFrames;
							RoutingViews.insert({ RawAt, Pair.first });
							bOff = On.Line != "Breaking off, and turning clear of torpedoes fired by " + R->ClearName + ".";
							RoutingOff += bOff ? 1 : 0;
						}
						else
						{
							++SteadyFrames;
							bOff = On.Line.rfind("Turning clear of torpedoes fired by " + R->ClearName + ". ", 0) != 0 || On.Line.find(" orders again in ") == std::string::npos ||
							       On.Line.find("Breaking off") != std::string::npos;
							SteadyOff += bOff ? 1 : 0;
						}
					}
					else if (bTurnLine && !(R && R->bClear && !R->bHeld))
					{
						++ClearAfter;
						bOff = true;
					}
					if (bOff && FirstTurnOff.empty())
					{
						char Buf[96];
						std::snprintf(Buf, sizeof Buf, "%.3f s piece %d: ", T, Pair.first);
						FirstTurnOff = Buf + On.Line;
					}
				}
				if (On.Line.empty()) continue;
				++Lines;
				if (Pair.second.Side != Mine) ++EnemySaid;
				if (!Clean(On.Line)) ++Dirty;
				for (const std::string& Name : Names)
					if (On.Line.find(Name) != std::string::npos) ++Named;
				// (the name the line gives, by the first word after "while" or "fired by": its count)
				std::string First;
				for (const char* Lead : { " while ", " fired by " })
				{
					const std::string Before = Lead;
					const size_t At = On.Line.find(Before);
					if (At == std::string::npos) continue;
					const size_t Start = At + Before.size();
					First = On.Line.substr(Start, On.Line.find(' ', Start) - Start);
					break;
				}
				static const std::set<std::string> ToTen = { "a", "an", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };
				static const std::set<std::string> PastTen = { "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty" };
				const bool bFigures = !First.empty() && std::all_of(First.begin(), First.end(), [](char C) { return std::isdigit(static_cast<unsigned char>(C)) != 0; });
				bool bNameOff = false;
				if (bFigures)
				{
					const bool bPastTen = First.size() > 2 || std::atoi(First.c_str()) >= 11;
					NamesFigures += bPastTen ? 1 : 0;
					bNameOff = !bPastTen;
				}
				else if (ToTen.count(First)) ++NamesInWords;
				else if (PastTen.count(First)) bNameOff = true;
				else ++NamesUncounted;
				if (bNameOff)
				{
					++NamesOff;
					if (FirstNameOff.empty()) FirstNameOff = On.Line;
				}
			}
			// (his own turns clear, spell by spell: one the show no longer carries has ended, at this frame)
			for (auto It = Turning.begin(); It != Turning.end();)
			{
				const auto Has = Pieces.find(It->first);
				if (Has != Pieces.end() && Has->second.ClearFrom >= 0 && Has->second.Ships >= 1)
				{
					++It;
					continue;
				}
				It->second.End = T;
				Turned.push_back(It->second);
				It = Turning.erase(It);
			}
			for (const auto& Pair : Pieces)
			{
				const SeaShow::Piece& P = Pair.second;
				if (P.Side != Mine || P.ClearFrom < 0 || P.Ships < 1) continue;
				Spell& Sp = Turning[Pair.first];
				if (Sp.Piece < 0)
				{
					Sp.Piece = Pair.first;
					Sp.From = T;
				}
				Sp.Last = T;
				Sp.Until = P.ClearUntil;
				Sp.bRouting = P.State == "routing";
				const std::string Line = SeaWords::Of(P, Pieces, Mine, Now).Line;
				const size_t In = Line.find(" orders again in ");
				if (In != std::string::npos)
				{
					Sp.Count = std::strtol(Line.c_str() + In + 17, nullptr, 10);
					Sp.Below += Sp.Count < 0 ? 1 : 0;
					++Sp.Counts;
				}
			}
			// The lines pinned for this recording, string for string.
			while (NextCheck < Checks.size() && Checks[NextCheck].T <= T + 1e-9) CheckShown(Pieces, Mine, Now, T, Checks[NextCheck++], Who);
			if (bFields && ShipsOf(Show) != ShipsOf(BareShow)) ++Apart;
			// The tags on the screen through the eyes ShotEye puts the camera at (a Fleets shot on each tagged piece, the Line
			// shot from ahead of his line's guide, the overview), each over the top of its leading ship
			// (ASeaBattle::PlaceTorpedoWords), stood by SeaWords::Lay as the screen stands them, the bar and the line kept clear.
			if (W.Tags.size() >= 2)
			{
				std::vector<Look> Eyes;
				for (const SeaWords::Tag& Eyed : W.Tags)
				{
					const SeaShow::ShipOut* On = Show.Ship(Show.LeaderOf(Eyed.Piece));
					if (!On || !On->bShown) continue;
					// (Fleets: 700 m astern of her and 420 m on her starboard quarter, 55 m up, looking 250 m ahead of her; 45 degrees)
					const double AX = std::sin(Radians(On->Heading)), AY = std::cos(Radians(On->Heading)), SX = std::cos(Radians(On->Heading)), SY = -std::sin(Radians(On->Heading));
					Eyes.push_back(Look{ "a Fleets shot", { On->At.X - AX * 700.0 + SX * 420.0, On->At.Y - AY * 700.0 + SY * 420.0, 55.0 }, { On->At.X + AX * 250.0, On->At.Y + AY * 250.0, 0.0 },
					                     45.0, Eyed.Piece });
				}
				{
					// (Line: ahead of his line's guide along the line's axis, a little to its engaged side, 20 m up, looking a third of
					// the way down the line; 40 degrees; the guide's piece the subject)
					const SeaShow::ShipOut* Guide = nullptr;
					double SumX = 0.0, SumY = 0.0, FoeX = 0.0, FoeY = 0.0;
					int Count = 0, Foes = 0;
					for (const SeaShow::ShipOut& Ship : Show.Ships())
					{
						if (Ship.Side == Mine && Ship.LineAt >= 0 && Ship.bShown)
						{
							SumX += Ship.At.X;
							SumY += Ship.At.Y;
							++Count;
							if (Ship.LineAt == 0 && !Guide) Guide = &Ship;
						}
						if (Ship.Side != Mine && Ship.Doing == SeaShow::Fate::Line && Ship.bShown)
						{
							FoeX += Ship.At.X;
							FoeY += Ship.At.Y;
							++Foes;
						}
					}
					if (Guide && Count > 0)
					{
						const double CX = std::sin(Radians(Guide->Heading)), CY = std::cos(Radians(Guide->Heading));
						const double MX = SumX / Count, MY = SumY / Count;
						const double Spread = std::hypot(Guide->At.X - MX, Guide->At.Y - MY);
						const double XX = Spread > 2.0 * Guide->Long ? (Guide->At.X - MX) / Spread : CX, XY = Spread > 2.0 * Guide->Long ? (Guide->At.Y - MY) / Spread : CY;
						double OX = XY, OY = -XX;
						if (Foes > 0 && (FoeX / Foes - Guide->At.X) * OX + (FoeY / Foes - Guide->At.Y) * OY < 0.0)
						{
							OX = -OX;
							OY = -OY;
						}
						const double Far = std::max(Spread, 600.0) * 0.35;
						Eyes.push_back(Look{ "the Line shot", { Guide->At.X + XX * Guide->Long * 5.0 + OX * Guide->Long * 2.0, Guide->At.Y + XY * Guide->Long * 5.0 + OY * Guide->Long * 2.0, 20.0 },
						                     { Guide->At.X - XX * Far, Guide->At.Y - XY * Far, 8.0 }, 40.0, Guide->Piece });
					}
				}
				{
					// (the overview: high behind the attacker's ships and off to one side; 55 degrees; no subject)
					double Ax = 0.0, Ay = 0.0, Bx = 0.0, By = 0.0;
					int Na = 0, Nb = 0;
					for (const SeaShow::ShipOut& Ship : Show.Ships())
					{
						if (Ship.Doing != SeaShow::Fate::Line || !Ship.bShown) continue;
						if (Ship.Side == 0)
						{
							Ax += Ship.At.X;
							Ay += Ship.At.Y;
							++Na;
						}
						else if (Ship.Side == 1)
						{
							Bx += Ship.At.X;
							By += Ship.At.Y;
							++Nb;
						}
					}
					if (Na > 0 && Nb > 0)
					{
						Ax /= Na;
						Ay /= Na;
						Bx /= Nb;
						By /= Nb;
						const double Len = std::max(std::hypot(Bx - Ax, By - Ay), 1e-6);
						const double WX = (Bx - Ax) / Len, WY = (By - Ay) / Len;
						Eyes.push_back(Look{ "the overview", { Ax - WX * 2200.0 + WY * 1500.0, Ay - WY * 2200.0 - WX * 1500.0, 1300.0 }, { (Ax + Bx) * 0.5, (Ay + By) * 0.5, 0.0 }, 55.0, -1 });
					}
				}
				for (const Look& Eye : Eyes)
				{
					const std::vector<SeaWords::Spot> Spots = SpotsThrough(Show, W, Eye);
					const std::string Said = Eye.Subject >= 0 ? SeaWords::Write(Pieces, Mine, Eye.Subject, Now).Line : std::string();
					std::vector<SeaWords::Box> Kept = { SeaWords::Box{ 549.0, -1.0e9, 1051.0, 46.0 } };
					if (!Said.empty())
					{
						const double LineWide = std::min(ScreenW - 64.0, 7.0 * static_cast<double>(Said.size()));
						Kept.push_back(SeaWords::Box{ (ScreenW - LineWide) * 0.5 - 14.0, ScreenH - 64.0 - 26.0, (ScreenW + LineWide) * 0.5 + 14.0, ScreenH - 64.0 });
					}
					++Looks[Eye.Name];
					// (the tags this eye puts on the screen, and whether two or more, so a shot is seen to check something)
					SpotsOn[Eye.Name] += static_cast<long>(Spots.size());
					TwoOn[Eye.Name] += Spots.size() >= 2 ? 1 : 0;
					// (as they would stand on their own ships, unlaid)
					bool bRawOn = false;
					for (size_t I = 0; I < Spots.size() && !bRawOn; ++I)
						for (size_t J = I + 1; J < Spots.size() && !bRawOn; ++J)
						{
							const SeaWords::Box A{ Spots[I].X - Spots[I].Wide * 0.5, Spots[I].Y - TagStalk - TagHigh, Spots[I].X + Spots[I].Wide * 0.5, Spots[I].Y };
							const SeaWords::Box Z{ Spots[J].X - Spots[J].Wide * 0.5, Spots[J].Y - TagStalk - TagHigh, Spots[J].X + Spots[J].Wide * 0.5, Spots[J].Y };
							bRawOn = A.Touches(Z, 0.0);
						}
					if (bRawOn) ++RawOn[Eye.Name];
					const std::vector<SeaWords::Laid> L = SeaWords::Lay(Spots, Kept, TagHigh, TagStalk);
					long LiftedHere = 0;
					for (const SeaWords::Laid& One : L) LiftedHere += One.Lifts > 0 ? 1 : 0;
					LaidLifted += LiftedHere;
					if (LiftedHere > 0) ++LiftedLooks[Eye.Name];
					const std::string Why = Wrong(Spots, Kept, L);
					if (!Why.empty())
					{
						++LaidWrong;
						if (FirstLaidWrong.empty())
						{
							char Buf[96];
							std::snprintf(Buf, sizeof Buf, "%.3f s, %s on piece %d: ", T, Eye.Name, Eye.Subject);
							FirstLaidWrong = Buf + Why;
						}
					}
					for (size_t Sk = 0; Sk < Spots.size(); ++Sk)
					{
						if (!Spots[Sk].bSubject) continue;
						const SeaWords::Box Whole{ Spots[Sk].X - Spots[Sk].Wide * 0.5, Spots[Sk].Y - TagStalk - TagHigh, Spots[Sk].X + Spots[Sk].Wide * 0.5, Spots[Sk].Y };
						const bool bFree = std::none_of(Kept.begin(), Kept.end(), [&Whole](const SeaWords::Box& Keep) { return Whole.Touches(Keep, 0.0); });
						if (bFree && (L.empty() || L[0].Of != Sk || L[0].Lifts != 0)) ++LookSubjectOff;
					}
				}
			}
		}
		const bool bEnded = !bFields || Show.Describe() == BareShow.Describe();

		std::printf("  %ld frames (%ld held to the views), %ld with tags (%ld tags), %ld lines of %ld subjects tried; enemy piece-views with a field set %ld\n", Frames, Judged,
		            TagFrames, Tags, Lines, Subjects, EnemySet);
		Expect(TagsOff == 0 && TextOff == 0, "the tags stand over his own pieces exactly while a field is set",
		       std::to_string(TagsOff) + " frames off, " + std::to_string(TextOff) + " worded off" + (FirstOff.empty() ? "" : " (" + FirstOff + ")"), "0");
		Expect(EnemySaid == 0, "an enemy piece never has words (tag or line)", std::to_string(EnemySaid), "0");
		Expect(LinesOff == 0, "the line only the subject's own (none without a subject)", std::to_string(LinesOff) + " off", "0");
		Expect(Unled == 0, "every tag over a leading ship afloat and drawn", std::to_string(Unled) + " without", "0");
		Expect(Dirty == 0 && Named == 0, "the words never speak of dice or odds, nor name a piece by its own name",
		       std::to_string(Dirty) + " dirty, " + std::to_string(Named) + " named", "0");
		if (!bFields)
		{
			Expect(Tags == 0 && Lines == 0, "no fields in the views: no words at all", std::to_string(Tags) + " tags, " + std::to_string(Lines) + " lines", "none");
		}
		else
		{
			Expect(Apart == 0 && bEnded, "the fields change nothing in the show (ship for ship as without them)", std::to_string(Apart) + " frames apart" + (bEnded ? "" : ", ending apart"), "0");
		}
		if (RoutingFrames + SteadyFrames > 0)
		{
			// (a piece breaking off has no orders to take up again: no count, no orders, whether ordered or not)
			Expect(RoutingOff == 0 && SteadyOff == 0, "turning clear: routing, exactly the breaking-off line (no count, no orders); not routing, never it",
			       std::to_string(RoutingFrames) + " routing piece-frames (" + std::to_string(RoutingViews.size()) + " piece-views) " + std::to_string(RoutingOff) + " off, " +
			           std::to_string(SteadyFrames) + " not routing " + std::to_string(SteadyOff) + " off" + (FirstTurnOff.empty() ? "" : " (" + FirstTurnOff + ")"),
			       "0 off");
		}
		{
			std::string Each;
			for (const Spell& Sp : Turned)
			{
				char Buf[200];
				const double Past = Sp.End - Sp.Until;
				std::snprintf(Buf, sizeof Buf, "piece %d %.3f to %.3f s, %s, last count %ld s, its 0 at %.2f s, %.3f s %s", Sp.Piece, Sp.From, Sp.End,
				              Sp.bRouting ? "routing" : "not routing", Sp.Count, Sp.Until, std::fabs(Past), Past >= 0.0 ? "before the field ended" : "after it ended");
				Each += (Each.empty() ? "" : "; ") + std::string(Buf);
			}
			if (!Turned.empty() || !Turning.empty())
			{
				std::printf("  his own turns clear as the show carries them: %s; %zu still turning at the end; the names in the lines: %ld counted one to ten in words, "
				            "%ld in figures from 11, %ld uncounted, %ld in figures to 10 or in words past 10\n",
				            Each.empty() ? "none ended" : Each.c_str(), Turning.size(), NamesInWords, NamesFigures, NamesUncounted, NamesOff);
			}
		}
		if (!OwnFielded.empty())
		{
			// (each of his own pieces the views give a field, with a ship afloat then, is tagged)
			std::string Each;
			bool bAll = true;
			for (const int Id : OwnFielded)
			{
				Each += (Each.empty() ? "" : ", ") + std::to_string(Id) + ": " + std::to_string(TaggedFrames[Id]);
				bAll = bAll && TaggedFrames[Id] > 0;
			}
			Expect(bAll, "each of his own pieces the views give a field is tagged (frames)", Each, "each some");
		}
		long AllLooks = 0, AllOn = 0;
		if (!Looks.empty())
		{
			std::string Each;
			for (const auto& Pair : Looks)
			{
				AllLooks += Pair.second;
				AllOn += RawOn[Pair.first];
				Each += (Each.empty() ? "" : ", ") + Pair.first + " " + std::to_string(Pair.second) + " (" + std::to_string(SpotsOn[Pair.first]) + " tags on the screen, " +
				        std::to_string(TwoOn[Pair.first]) + " with two or more there, " + std::to_string(RawOn[Pair.first]) + " met, " + std::to_string(LiftedLooks[Pair.first]) +
				        " with a tag lifted)";
			}
			std::printf("  looks with two tags or more set, the tags each put on the screen, the looks with two or more there, those whose tags would meet unlaid and "
			            "those laid with a tag lifted: %s; %ld tags lifted\n",
			            Each.c_str(), LaidLifted);
			Expect(LaidWrong == 0, "the tags laid through the shots' eyes: never on one another nor on the bar or the line",
			       std::to_string(LaidWrong) + " looks wrong of " + std::to_string(AllLooks) + (FirstLaidWrong.empty() ? "" : " (" + FirstLaidWrong + ")"), "0");
			Expect(LookSubjectOff == 0, "the subject's tag on her ship in every look", std::to_string(LookSubjectOff) + " off", "0");
		}
		if (!Pin) return;
		// What is pinned for this recording.
		const std::string Of = std::string(Pin->Key) + (Who.empty() ? "" : " (" + Who.substr(0, Who.size() - 2) + ")");
		Expect(NextCheck == Checks.size(), Of + ": every pinned line checked", std::to_string(NextCheck) + " of " + std::to_string(Checks.size()), "all");
		if (Pin->bMeet)
		{
			// (its divisions hold and turn clear side by side: their tags meet on the screen, unlaid, and are lifted apart)
			Expect(AllOn > 0 && LaidLifted > 0 && LaidWrong == 0, Of + ": its tags meet on the screen, and are lifted apart",
			       std::to_string(AllOn) + " looks met of " + std::to_string(AllLooks) + ", " + std::to_string(LaidLifted) + " lifted", "some, some");
			// (and each of the three shots puts two tags or more on the screen in some look, so none passes having laid nothing)
			std::string Each;
			bool bEach = true;
			for (const char* Shot : { "a Fleets shot", "the Line shot", "the overview" })
			{
				Each += (Each.empty() ? "" : ", ") + std::string(Shot) + " " + std::to_string(SpotsOn[Shot]) + " on the screen, " + std::to_string(TwoOn[Shot]) + " looks with two";
				bEach = bEach && SpotsOn[Shot] > 0 && TwoOn[Shot] > 0;
			}
			Expect(bEach, Of + ": each shot (Fleets, Line, overview) lays two tags or more on the screen", Each, "each some");
		}
		if (Pin->LetGo >= 0)
		{
			// (the rules let her go as the heads pass her: each turn clear not routing as its field ends shows its count
			// down to within 2 s of 0, never below, the field ending in the 2 s after the count's 0; one routing breaks off)
			long LetGo = 0, LetGoRouting = 0, LetGoOff = 0;
			std::string Each;
			for (const Spell& Sp : Turned)
			{
				if (Sp.bRouting)
				{
					++LetGoRouting;
					continue;
				}
				++LetGo;
				const double Past = Sp.End - Sp.Until;
				const bool bOk = Sp.Counts > 0 && Sp.Count >= 0 && Sp.Count <= 2 && Sp.Below == 0 && Past >= -1e-6 && Past <= 2.0 + 1e-6;
				LetGoOff += bOk ? 0 : 1;
				char Buf[160];
				std::snprintf(Buf, sizeof Buf, "piece %d: last count %ld s, %ld below 0, the field ended %.3f s %s its 0", Sp.Piece, Sp.Count, Sp.Below, std::fabs(Past),
				              Past >= 0.0 ? "after" : "before");
				Each += (Each.empty() ? "" : "; ") + std::string(Buf);
			}
			Expect(LetGo == Pin->LetGo && LetGoRouting == Pin->LetGoRouting && LetGoOff == 0 && ClearAfter == 0 && Turning.empty(),
			       Of + ": each turn clear not routing as it ends ends at its count's 0 (within 2 s, never below 0), none after the field ends",
			       std::to_string(LetGo) + " not routing (" + Each + "), " + std::to_string(LetGoRouting) + " routing, " + std::to_string(ClearAfter) + " lines after, " +
			           std::to_string(Turning.size()) + " unended",
			       std::to_string(Pin->LetGo) + " at their 0, " + std::to_string(Pin->LetGoRouting) + " routing, none after");
		}
		if (Pin->RoutingViews >= 0)
		{
			Expect(static_cast<long>(RoutingViews.size()) == Pin->RoutingViews && RoutingOff == 0 && SteadyFrames > 0 && SteadyOff == 0,
			       Of + ": its turns clear while routing break off, exactly, each piece-view; the rest count down",
			       std::to_string(RoutingViews.size()) + " routing piece-views (" + std::to_string(RoutingFrames) + " frames, " + std::to_string(RoutingOff) + " off), " +
			           std::to_string(SteadyFrames) + " frames not routing (" + std::to_string(SteadyOff) + " off)",
			       std::to_string(Pin->RoutingViews) + ", 0 off, some, 0 off");
		}
		if (Lines > 0)
		{
			// (the rules name a piece with its count in words from one to ten, figures from 11; the log keeps figures)
			Expect(NamesOff == 0 && NamesInWords > 0, Of + ": the names in its lines count one to ten in words, figures only from 11",
			       std::to_string(Lines) + " lines: " + std::to_string(NamesInWords) + " in words, " + std::to_string(NamesFigures) + " in figures from 11, " +
			           std::to_string(NamesUncounted) + " uncounted, " + std::to_string(NamesOff) + " off" + (FirstNameOff.empty() ? "" : " (" + FirstNameOff + ")"),
			       "some in words, 0 off");
		}
		if (Pin->bEnemyOnly)
		{
			Expect(bFields && EnemySet > 0 && Tags == 0 && Lines == 0 && EnemySaid == 0, Of + ": the enemy's fields set in the views, and no words at all",
			       std::to_string(EnemySet) + " enemy piece-views set (frames), " + std::to_string(Tags) + " tags, " + std::to_string(Lines) + " lines", "some set, none");
		}
		if (Pin->bQuiet)
		{
			Expect(!bFields && Tags == 0 && Lines == 0 && Subjects > 0, Of + ": recorded with the switch on, no field: nothing at any frame, whatever the subject",
			       std::to_string(Frames) + " frames, " + std::to_string(Subjects) + " subjects tried, " + std::to_string(Tags) + " tags, " + std::to_string(Lines) + " lines",
			       "nothing");
		}
	}

	/** Whether a path lies within a folder of this name, anywhere along it. */
	bool InFolder(const std::string& Path, const std::string& Folder)
	{
		size_t From = 0;
		for (size_t Slash = Path.find_first_of("/\\"); Slash != std::string::npos; From = Slash + 1, Slash = Path.find_first_of("/\\", From))
		{
			if (Path.compare(From, Slash - From, Folder) == 0) return true;
		}
		return false;
	}

	/** One recording (Path: its file, or what it is), read and played. */
	bool PlayText(const std::string& Path, const std::string& Text)
	{
		SeaFeed::Recording Rec;
		std::string Error;
		std::vector<RawView> Raw;
		std::string Me;
		bool bFields = false;
		RawCount Count;
		if (!SeaFeed::ReadRecording(Text, Rec, Error) || !ReadRaw(Text, Raw, Me, bFields, Count))
		{
			std::printf("%s: %s\n", Path.c_str(), Error.empty() ? "not a recording" : Error.c_str());
			++Failures;
			return false;
		}
		// (the player's side as the scene takes it, ASeaBattle::PlaceTorpedoWords: the first view's "me")
		const int Mine = SeaFeed::SideOf(Rec.Views.front().Head.Me);
		std::printf("%s: %s, the player %s; %s\n", Rec.Key.c_str(), Rec.Board.Title.c_str(), Me.c_str(), bFields ? "the views carry the torpedo words' fields" : "no torpedo words' fields");

		// SeaFeed reads every field the views set, as the JSON read apart has them.
		long Held = 0, Clear = 0, HeldUnnamed = 0, ClearUnnamed = 0, Ordered = 0, ClearRouting = 0;
		for (const SeaFeed::Frame& F : Rec.Views)
			for (const SeaShow::Piece& P : F.Pieces)
			{
				Held += P.HeldFor >= 0 ? 1 : 0;
				HeldUnnamed += P.HeldFor >= 0 && P.HeldForName.empty() ? 1 : 0;
				Clear += P.ClearFrom >= 0 ? 1 : 0;
				ClearUnnamed += P.ClearFrom >= 0 && P.ClearFromName.empty() ? 1 : 0;
				Ordered += P.bOrdered ? 1 : 0;
				ClearRouting += P.ClearFrom >= 0 && P.State == "routing" ? 1 : 0;
			}
		char Buf[640];
		std::snprintf(Buf, sizeof Buf,
		              "  the views set %ld piece-views held (in %ld views), %ld turning clear (in %ld views; %ld of them routing); held without a name %ld, turning clear "
		              "without one %ld, ordered %ld (with a field: att %ld, def %ld); a ship alone with a field %ld; turning clear past its clearUntil %ld; turns ended "
		              "before their clearUntil %ld (the count at most %.1f s from its 0)\n",
		              Count.Held, Count.HeldViews, Count.Clear, Count.ClearViews, Count.ClearRouting, Count.HeldUnnamed, Count.ClearUnnamed, Count.Ordered, Count.OrderedOn[0],
		              Count.OrderedOn[1], Count.Alone, Count.PastUntil, Count.EndedEarly, Count.MostLeft);
		std::printf("%s", Buf);
		Expect(Held == Count.Held && Clear == Count.Clear && HeldUnnamed == Count.HeldUnnamed && ClearUnnamed == Count.ClearUnnamed && Ordered == Count.Ordered,
		       "SeaFeed reads every field the views set (held, turning clear, their names, ordered)",
		       std::to_string(Held) + " held, " + std::to_string(Clear) + " turning clear, " + std::to_string(HeldUnnamed + ClearUnnamed) + " unnamed, " + std::to_string(Ordered) +
		           " ordered",
		       std::to_string(Count.Held) + ", " + std::to_string(Count.Clear) + ", " + std::to_string(Count.HeldUnnamed + Count.ClearUnnamed) + ", " + std::to_string(Count.Ordered));
		// (and the state of each piece turning clear, routing or not, as the words read it: the show's piece's own, no field
		// of the rules')
		if (Count.Clear > 0)
		{
			Expect(ClearRouting == Count.ClearRouting, "SeaFeed reads the state of each piece turning clear (routing: breaking off)",
			       std::to_string(ClearRouting) + " routing of " + std::to_string(Clear), std::to_string(Count.ClearRouting) + " of " + std::to_string(Count.Clear));
		}

		// As the scene takes the player; and, where the views give fields to the other side's pieces, as that side too (the
		// scene never does for this recording, its player the first view's me: this play reads those fields as his own, so
		// the real fields of either side reach the words).
		size_t Played = 0, PinPlays = 0;
		for (const Pinned& P : Pins()) PinPlays += Rec.Key == P.Key && Rec.Seed == P.Seed && Rec.Views.size() == P.Views ? 1 : 0;
		const Pinned* Pin = PinFor(Rec, Mine);
		PlayAs(Rec, Raw, bFields, Mine, Pin);
		Played += Pin ? 1 : 0;
		if (Mine == 0 || Mine == 1)
		{
			const int Other = 1 - Mine;
			if (!Count.Fielded[Other].empty())
			{
				std::printf("%s as if the player were the %s (his own: the views' fields on that side's pieces)\n", Rec.Key.c_str(), Other == 1 ? "defender" : "attacker");
				const Pinned* OtherPin = PinFor(Rec, Other);
				PlayAs(Rec, Raw, bFields, Other, OtherPin);
				Played += OtherPin ? 1 : 0;
			}
		}
		if (PinPlays > 0) Expect(Played == PinPlays, Rec.Key + ": every pinned play played", std::to_string(Played) + " of " + std::to_string(PinPlays), "all");
		// (a naval own-water recording, in an own-water-r2 folder, is found by its pins: re-recorded with another seed or
		// another length, it would match none and play unpinned, so it fails here until its pins are made anew; the shipped
		// Content recordings of the same keys are never in such a folder)
		if (InFolder(Path, "own-water-r2"))
		{
			std::string Known;
			for (const Pinned& P : Pins())
			{
				const std::string One = "seed " + std::to_string(P.Seed) + ", " + std::to_string(P.Views) + " views";
				if (Rec.Key == P.Key && Known.find(One) == std::string::npos) Known += (Known.empty() ? "" : "; ") + One;
			}
			Expect(PinPlays > 0, Rec.Key + ": an own-water recording, found by its pins (key, seed, views)",
			       "seed " + std::to_string(Rec.Seed) + ", " + std::to_string(Rec.Views.size()) + " views", Known.empty() ? "no pins for its key" : Known);
		}
		return true;
	}

	bool PlayRecording(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			std::printf("%s: cannot read\n", Path.c_str());
			++Failures;
			return false;
		}
		std::stringstream Buffer;
		Buffer << In.rdbuf();
		return PlayText(Path, Buffer.str());
	}

	/** The views written here played as a recording, frame by frame, as the scene's player (att) and as the defender: the
	 * frame-by-frame checks (the tags held to the views, the enemy never, the line only the subject's, clean and naming no
	 * piece, the fields changing nothing ship for ship, the tags laid through the shots' eyes) on what the computer's sides
	 * never record: his order and the enemy's, a ship alone, a turn clear past its landing, a turn clear without its name. */
	void FeedPlayed()
	{
		std::printf("the views written here, played frame by frame as a recording, as either side\n");
		const std::string Text = WrittenViews();
		std::vector<RawView> Raw;
		std::string Me;
		bool bFields = false;
		RawCount Count;
		const bool bRead = ReadRaw(Text, Raw, Me, bFields, Count);
		char Got[200];
		std::snprintf(Got, sizeof Got, "ordered with a field: att %ld, def %ld; a ship alone %ld; past the landing %ld; turning clear unnamed %ld; fielded att %zu, def %zu",
		              Count.OrderedOn[0], Count.OrderedOn[1], Count.Alone, Count.PastUntil, Count.ClearUnnamed, Count.Fielded[0].size(), Count.Fielded[1].size());
		Expect(bRead && bFields && Count.OrderedOn[0] > 0 && Count.OrderedOn[1] > 0 && Count.Alone > 0 && Count.PastUntil > 0 && Count.ClearUnnamed > 0 &&
		           !Count.Fielded[0].empty() && !Count.Fielded[1].empty(),
		       "the views written here give the frame-by-frame play his order, the enemy's, a ship alone, past the landing, an unnamed turn clear", Got, "each some");
		PlayText("(the views written here)", Text);
	}
}

int main(int Argc, char** Argv)
{
	Words();
	Placing();
	Eyes();
	Feed();
	FeedPlayed();
	for (int K = 1; K < Argc; ++K) PlayRecording(Argv[K]);
	std::printf("%d checks: %d passed, %d failed\n", Passes + Failures, Passes, Failures);
	std::printf(Failures ? "SOME CHECKS FAILED\n" : "ALL CHECKS PASSED\n");
	return Failures ? 1 : 0;
}
