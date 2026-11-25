// The torpedo words' cloud test (Battle/SeaWordsCore; the naval rules behind their switch "Friendly torpedoes", Brian 25
// November 12:52Z, and the UI director's words, final at 13:27Z): every word of the tags and the line, string for string
// (her and their, is and are, has and have, your orders and hers or theirs, 1 second and N seconds, the count down to 0;
// is and are as the phrase counts the ships where it does); an enemy piece never has words; no fields, nothing; held wins
// over turning clear; the line is the camera's subject's own or none. Where the tags stand (SeaWords::Lay): never on one
// another, the subject's first and on her ship, then the nearest; none over the bar, the line, the month scene's caption
// or its way back; a lifted tag's stalk down to its ship behind the plates it passes. Then each recording played through the very code the game runs (SeaFeed, SeaShow, SeaWordsCore), at 32
// frames a second: the tags stand over the player's own pieces exactly while the views (read here apart, from the JSON
// itself) set their fields; a recording without the fields (every one the battle has made so far) shows no words at
// all; with them the show is the same ship for ship as without; and the stand-in (stand-ins/two-lines-torpedoes.json,
// made by stand-ins/torpedo_words.py) shows the very lines it was made to; and the tags, through the eyes the scene's
// shots put the camera at (a Fleets shot, the Line shot, the overview), stand laid apart on the screen.
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
			// Two in one column, nearly on one another (the 430.5 s of the stand-in: Cruiser 1 and Cruiser 4 up the column
			// from a Fleets shot's eye): the nearer on her ship, the farther lifted just over it, its stalk behind it.
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
			// Four together (pieces 1, 3, 6 and 7 at 430.5 s): all four stand, none on another.
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

	/** The tags' feet through an eye, as ASeaBattle::PlaceTorpedoWords finds them: over the top of each tagged piece's
	 * leading ship, drawn, in front of the eye and on a 1600 by 900 screen. */
	std::vector<SeaWords::Spot> SpotsThrough(const SeaShow::Show& Show, const SeaWords::Words& W, const Look& Eye)
	{
		double F[3] = { Eye.To[0] - Eye.From[0], Eye.To[1] - Eye.From[1], Eye.To[2] - Eye.From[2] };
		const double FL = std::sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]);
		for (double& C : F) C /= FL;
		// (right: the look across the up, level; up: the right across the look)
		double Rt[2] = { F[1], -F[0] };
		const double RL = std::hypot(Rt[0], Rt[1]);
		Rt[0] /= RL;
		Rt[1] /= RL;
		const double Up[3] = { -F[2] * Rt[1], F[2] * Rt[0], Rt[0] * F[1] - Rt[1] * F[0] };
		const double HalfWide = std::tan(Radians(Eye.Lens * 0.5));
		std::vector<SeaWords::Spot> Spots;
		for (const SeaWords::Tag& Tg : W.Tags)
		{
			const SeaShow::ShipOut* Lead = Show.Ship(Show.LeaderOf(Tg.Piece));
			if (!Lead || !Lead->bShown) continue;
			const double Top = std::max(Lead->At.Z, 0.0) + Lead->Heave + 6.0 + 0.12 * Lead->Long;
			const double O[3] = { Lead->At.X - Eye.From[0], Lead->At.Y - Eye.From[1], Top - Eye.From[2] };
			const double Deep = O[0] * F[0] + O[1] * F[1] + O[2] * F[2];
			if (Deep < 1.0) continue;
			const double Across = 0.5 + 0.5 * (O[0] * Rt[0] + O[1] * Rt[1]) / (Deep * HalfWide);
			const double Down = 0.5 - 0.5 * (ScreenW / ScreenH) * (O[0] * Up[0] + O[1] * Up[1] + O[2] * Up[2]) / (Deep * HalfWide);
			if (Across < 0.0 || Across > 1.0 || Down < 0.0 || Down > 1.0) continue;
			Spots.push_back(SpotAt(Across * ScreenW, Down * ScreenH, PlateWide(Tg.Text), Deep, Tg.Piece, Tg.Piece == Eye.Subject));
		}
		return Spots;
	}

	/** A piece as the views' JSON says it, read here apart from SeaFeed: its side and ships as last told, and whether its
	 * view sets the words' fields (heldFor; clearFrom with its clearUntil). */
	struct RawPiece
	{
		int Side = 0;
		int Ships = 1;
		bool bHeld = false, bClear = false;
	};
	struct RawView
	{
		double T = 0.0;
		std::map<int, RawPiece> Pieces;
	};

	/** A line the stand-in must show: at battle time T with piece Piece the camera's subject (off the views' own seconds,
	 * so the view reached is never in doubt), its tag and its line (both empty: none). */
	struct Shown
	{
		double T;
		int Piece;
		const char* Tag;
		const char* Line;
	};

	const std::vector<Shown>& StandInLines()
	{
		static const std::vector<Shown> Lines = {
			{ 300.5, 1, "", "" },
			{ 366.5, 2, "Torpedoes held", "Holding their torpedoes while a battleship of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear." },
			{ 390.5, 4, "Torpedoes held", "Holding her torpedoes while four destroyers of the Home Fleet are in the line of fire. She fires the moment the water is clear." },
			{ 390.5, 1, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by two destroyers of the Home Fleet. They take up their orders again in 61 seconds, once the torpedoes have passed." },
			{ 410.5, 1, "Torpedoes held", "Holding their torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: they fire the moment the water is clear." },
			{ 430.5, 1, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by two destroyers of the Home Fleet. They take up your orders again in 21 seconds, once the torpedoes have passed." },
			{ 430.5, 7, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by two destroyers of the Home Fleet. She takes up her orders again in 21 seconds, once the torpedoes have passed." },
			{ 430.5, 3, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up your orders again in 68 seconds, once the torpedoes have passed." },
			{ 430.5, 6, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up her orders again in 68 seconds, once the torpedoes have passed." },
			{ 444.5, 6, "Torpedoes held", "Holding her torpedoes while a cruiser of the Home Fleet is in the line of fire. Your order stands: she fires the moment the water is clear." },
			{ 450.5, 1, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by two destroyers of the Home Fleet. They take up your orders again in 1 second, once the torpedoes have passed." },
			{ 452.5, 1, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by two destroyers of the Home Fleet. They take up your orders again in 0 seconds, once the torpedoes have passed." },
			{ 454.5, 1, "", "" },
			{ 499.5, 3, "Turning clear of torpedoes",
			  "Turning clear of torpedoes fired by four destroyers of the Home Fleet. She takes up your orders again in 0 seconds, once the torpedoes have passed." },
			{ 362.5, 11, "", "" },
			{ 385.5, 19, "", "" },
			{ 420.5, 14, "", "" },
		};
		return Lines;
	}

	/** The recording as the views' JSON says it, apart from SeaFeed. */
	bool ReadRaw(const std::string& Text, std::vector<RawView>& Out, std::string& Me, bool& bAnyFields)
	{
		AirFeed::Json J;
		std::string Error;
		if (!AirFeed::Parse(Text, J, Error)) return false;
		const AirFeed::Json& Views = J["views"];
		Me = Views.At(0)["me"].Str("att");
		bAnyFields = false;
		std::map<int, RawPiece> Kept;
		for (size_t K = 0; K < Views.Count(); ++K)
		{
			const AirFeed::Json& V = Views.At(K);
			RawView R;
			R.T = V["t"].Num();
			const AirFeed::Json& Units = V["units"];
			for (size_t U = 0; U < Units.Count(); ++U)
			{
				const AirFeed::Json& Unit = Units.At(U);
				const AirFeed::Json& X = Unit["extra"];
				const int Id = Unit["id"].Int();
				RawPiece P = Kept.count(Id) ? Kept[Id] : RawPiece();
				if (Unit["side"].IsString()) P.Side = Unit["side"].Str() == "def" ? 1 : 0;
				P.Ships = std::clamp(X["ships"].IsNumber() ? X["ships"].Int() : Unit["figures"].Int(P.Ships), 0, 64);
				P.bHeld = X["heldFor"].IsNumber() && X["heldFor"].Int() >= 0;
				P.bClear = X["clearFrom"].IsNumber() && X["clearFrom"].Int() >= 0 && X["clearUntil"].IsNumber();
				bAnyFields = bAnyFields || !X["heldFor"].IsNull() || !X["clearFrom"].IsNull() || !X["clearUntil"].IsNull() || !X["heldForName"].IsNull() ||
				             !X["clearFromName"].IsNull() || !X["ordered"].IsNull();
				Kept[Id] = P;
				R.Pieces[Id] = P;
			}
			Out.push_back(R);
		}
		return !Out.empty();
	}

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
		SeaFeed::Recording Rec;
		std::string Error;
		std::vector<RawView> Raw;
		std::string Me;
		bool bFields = false;
		if (!SeaFeed::ReadRecording(Buffer.str(), Rec, Error) || !ReadRaw(Buffer.str(), Raw, Me, bFields))
		{
			std::printf("%s: %s\n", Path.c_str(), Error.empty() ? "not a recording" : Error.c_str());
			++Failures;
			return false;
		}
		// (the player's side as the scene takes it, ASeaBattle::PlaceTorpedoWords: the first view's "me")
		const int Mine = SeaFeed::SideOf(Rec.Views.front().Head.Me);
		const bool bStandIn = Rec.Key == "two-lines-torpedoes";
		std::printf("%s: %s, the player %s; %s\n", Rec.Key.c_str(), Rec.Board.Title.c_str(), Me.c_str(), bFields ? "the views carry the torpedo words' fields" : "no torpedo words' fields");

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

		// The show's own fixed step: within it of a view's second, the view the picture has reached is in doubt (the show
		// takes it at its own step, not the frame's), so those frames are not held to the views read apart.
		constexpr double Doubt = 0.06;
		const double T0 = Rec.Views.front().Head.T, EndT = Rec.Views.back().Head.T;
		long Frames = 0, Judged = 0, TagFrames = 0, Tags = 0, Lines = 0, TagsOff = 0, TextOff = 0, LinesOff = 0, EnemySet = 0, EnemySaid = 0, Unled = 0, Dirty = 0, Named = 0, Apart = 0;
		std::map<std::string, long> Looks, RawOn;
		long LaidLifted = 0, LaidWrong = 0, LookSubjectOff = 0;
		std::string FirstLaidWrong;
		std::string FirstOff;
		std::map<int, long> TaggedFrames;
		size_t NextCheck = 0;
		std::vector<Shown> Checks = bStandIn ? StandInLines() : std::vector<Shown>();
		std::sort(Checks.begin(), Checks.end(), [](const Shown& L, const Shown& R) { return L.T < R.T; });
		size_t RawAt = 0;
		for (long K = 0;; ++K)
		{
			// (32 frames a second: a frame's second is exact, so the stand-in's checks fall on frames)
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
				const SeaWords::Words On = SeaWords::Write(Pieces, Mine, Pair.first, Now);
				const SeaWords::Said Own = SeaWords::Of(Pair.second, Pieces, Mine, Now);
				if (On.Line != Own.Line || (!On.Line.empty() && On.LinePiece != Pair.first)) ++LinesOff;
				if (On.Line.empty()) continue;
				++Lines;
				if (Pair.second.Side != Mine) ++EnemySaid;
				if (!Clean(On.Line)) ++Dirty;
				for (const std::string& Name : Names)
					if (On.Line.find(Name) != std::string::npos) ++Named;
			}
			// The stand-in's own lines, string for string.
			while (NextCheck < Checks.size() && Checks[NextCheck].T <= T + 1e-9)
			{
				const Shown& C = Checks[NextCheck++];
				const SeaWords::Words On = SeaWords::Write(Pieces, Mine, C.Piece, Now);
				std::string Tag;
				for (const SeaWords::Tag& Tg : On.Tags)
					if (Tg.Piece == C.Piece) Tag = Tg.Text;
				char Name[96];
				std::snprintf(Name, sizeof Name, "at %.1f s, piece %d the subject:", C.T, C.Piece);
				Expect(std::fabs(T - C.T) < 1e-9, std::string(Name) + " on a frame", std::to_string(T), "on it");
				Same(std::string(Name) + " the tag", Tag, C.Tag);
				Same(std::string(Name) + " the line", On.Line, C.Line);
			}
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
					for (const SeaWords::Laid& One : L) LaidLifted += One.Lifts > 0 ? 1 : 0;
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

		std::printf("  %ld frames (%ld held to the views), %ld with tags (%ld tags), %ld lines; enemy piece-views with a field set %ld\n", Frames, Judged, TagFrames, Tags, Lines, EnemySet);
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
		if (!Looks.empty())
		{
			long AllLooks = 0, AllOn = 0;
			std::string Each;
			for (const auto& Pair : Looks)
			{
				AllLooks += Pair.second;
				AllOn += RawOn[Pair.first];
				Each += (Each.empty() ? "" : ", ") + Pair.first + " " + std::to_string(Pair.second) + " (" + std::to_string(RawOn[Pair.first]) + " met)";
			}
			std::printf("  looks with two tags or more, and those whose tags would meet unlaid: %s; %ld tags lifted\n", Each.c_str(), LaidLifted);
			Expect(LaidWrong == 0, "the tags laid through the shots' eyes: never on one another nor on the bar or the line",
			       std::to_string(LaidWrong) + " looks wrong of " + std::to_string(AllLooks) + (FirstLaidWrong.empty() ? "" : " (" + FirstLaidWrong + ")"), "0");
			Expect(LookSubjectOff == 0, "the subject's tag on her ship in every look", std::to_string(LookSubjectOff) + " off", "0");
			if (bStandIn)
			{
				// (the stand-in does put tags on one another, unlaid: its divisions turn clear together)
				Expect(AllOn > 0 && LaidLifted > 0, "the stand-in's tags meet on the screen, and are lifted apart",
				       std::to_string(AllOn) + " looks met, " + std::to_string(LaidLifted) + " lifted", "some, some");
			}
		}
		if (bStandIn)
		{
			// (not empty: each own piece given spans was tagged, and the enemy had spans of its own to stay quiet through)
			std::string Each;
			bool bAll = true;
			for (const int Id : { 1, 2, 3, 4, 6, 7 })
			{
				Each += (Each.empty() ? "" : ", ") + std::to_string(Id) + ": " + std::to_string(TaggedFrames[Id]);
				bAll = bAll && TaggedFrames[Id] > 0;
			}
			Expect(bAll && EnemySet > 0 && NextCheck == Checks.size(), "the stand-in's spans each tagged, the enemy's untagged, every line checked",
			       Each + "; enemy " + std::to_string(EnemySet) + "; " + std::to_string(NextCheck) + " of " + std::to_string(Checks.size()) + " lines", "every own span, enemy spans, all lines");
		}
		return true;
	}
}

int main(int Argc, char** Argv)
{
	Words();
	Placing();
	for (int K = 1; K < Argc; ++K) PlayRecording(Argv[K]);
	std::printf("%d checks: %d passed, %d failed\n", Passes + Failures, Passes, Failures);
	std::printf(Failures ? "SOME CHECKS FAILED\n" : "ALL CHECKS PASSED\n");
	return Failures ? 1 : 0;
}
