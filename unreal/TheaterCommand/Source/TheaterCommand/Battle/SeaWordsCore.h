#pragma once

#include "Battle/SeaShow.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * The torpedo words of the 3D sea scene, in plain C++ so the cloud test reads exactly what the player will (as AirDesk is
 * the air scene's words). Brian, 25 November 12:52Z: friendly ships passing in front of torpedoes. The naval rules answer
 * it behind their switch "Friendly torpedoes": a piece holds its salvo while one of its own side's pieces is in its line
 * of fire, and its own side's pieces turn clear of a salvo of their own running. The words are the UI director's, final
 * at 13:27Z (relayed by the naval thread): a tag over each of the player's own pieces while it holds or turns clear, and
 * one line at the foot of the screen while the camera's subject is that piece. ASeaBattle places them and SSeaBattleScreen
 * draws them; neither words them. A piece routing that turns clear has no orders to take up again: its line breaks off
 * (the UI director, 25 November, as the rules made clearUntil the second the heads pass her), by the state its view
 * gives it (the show's piece has it already: no field of the rules').
 *
 * The fields are the piece's extra in each view, live or recorded (SeaFeed reads them into SeaShow::Piece, as each view
 * says): heldFor and heldForName, clearFrom, clearUntil and clearFromName, ordered. Without the switch there are none and
 * nothing shows. Only the player's own pieces have words: an enemy piece holds or turns clear with none. The words name a
 * piece as the rules name it (lower case, one to ten ships in words, figures from 11, where the log keeps figures: "four
 * destroyers of the Home Fleet"), never a ship, and never speak of dice or odds.
 */
namespace SeaWords
{
	/** What a piece is doing, as its words say. */
	enum class Kind { None, Held, Clearing };

	/** The tags (the UI director's, exact). */
	inline constexpr const char* HeldTag = "Torpedoes held";
	inline constexpr const char* ClearingTag = "Turning clear of torpedoes";

	/** A piece's fields as its view gives them (SeaShow::Piece keeps them, FieldsOf reads them back). */
	struct Fields
	{
		/** heldFor: the own piece in its line of fire (-1 none; the rules keep it a second past their last check), and that
		 * piece as the rules name it, lower case. */
		int HeldFor = -1;
		std::string HeldForName;
		/** clearFrom and clearUntil: the own piece whose running salvo it turns clear of (-1 none) and the battle second the
		 * rules let her go, the heads past her where she lies (never later than the salvo lands); and the launcher as the
		 * rules name it. A clearFrom without its clearUntil is not read (SeaFeed). */
		int ClearFrom = -1;
		double ClearUntil = -1.0;
		std::string ClearFromName;
		/** ordered: the piece is under an order of the player's own (with either). */
		bool bOrdered = false;
		/** Routing: the piece breaking off, as its view's state says ("routing"; not one of the rules' torpedo fields, the
		 * show's piece has it: FieldsOf reads it). */
		bool bRouting = false;

		bool Held() const { return HeldFor >= 0; }
		bool Clearing() const { return ClearFrom >= 0; }
		bool Any() const { return Held() || Clearing(); }
	};
	Fields FieldsOf(const SeaShow::Piece& P);

	/** The word for one ship or the word for more (is or are, has or have, she or they, her or their), by the ships of the
	 * piece meant, as the rules' many() picks: one ship (or none) the first, two or more the second. */
	const char* Agree(int Ships, const char* One, const char* Many);
	/** The whole seconds from Now to Until, rounded up (the count never says the torpedoes have passed before they have),
	 * never below 0. */
	int SecondsLeft(double Until, double Now);
	/** "1 second", "12 seconds", "0 seconds". */
	std::string SecondsWords(int Seconds);
	/** The ships a piece's name itself gives, as the rules or the log name it: 1 where it opens "a ", "an " or "one ", the
	 * count where it opens with a count ("two " to "twenty ", or in figures: "12 "), 0 where it gives no number. */
	int ShipsByName(const std::string& Name);

	/** A piece's words: its tag and its line (empty with none). */
	struct Said
	{
		Kind What = Kind::None;
		std::string Tag, Line;
	};
	/**
	 * Fields of a piece of side Side (0 the attacker, 1 the defender) with Ships ships afloat, the player's side Mine (-1
	 * none), at battle time Now. NamedShips: the ships afloat now of the piece heldFor (below 1 not known). The verb after
	 * heldForName agrees with the phrase itself where it gives a number ("a cruiser ... is", "two destroyers ... are", even
	 * in the second the rules keep the name after a ship of it has gone down), else with NamedShips, else "are". Held wins
	 * over turning clear. A piece routing turns clear with no count and no orders to take up: "Breaking off, and turning
	 * clear of torpedoes fired by ...", ordered or not, one ship or more (its tag the same; held, its words the same). A
	 * piece with no ship afloat, or not the player's, says nothing; a line needs the name of the piece it speaks of (the
	 * tag stands without it).
	 */
	Said Of(const Fields& F, int Side, int Mine, int Ships, int NamedShips, double Now);
	/** The same for a piece of the show, the piece it names looked up among Pieces (the show's pieces now). */
	Said Of(const SeaShow::Piece& P, const std::map<int, SeaShow::Piece>& Pieces, int Mine, double Now);

	/** One tag, over a piece's leading ship. */
	struct Tag
	{
		int Piece = -1;
		Kind What = Kind::None;
		std::string Text;
	};
	/** Everything the screen writes: a tag for each of the player's own pieces holding or turning clear (by its id), and
	 * the line of the camera's subject piece (LinePiece; the line empty while that piece has none to say: never another
	 * piece's in its place). */
	struct Words
	{
		std::vector<Tag> Tags;
		std::string Line;
		int LinePiece = -1;
		Kind LineWhat = Kind::None;
	};
	/** The words for the show's pieces now (Show::Pieces()), the player's side Mine, the camera's subject piece Subject (-1
	 * none), at battle time Now (Show::Shown()). */
	Words Write(const std::map<int, SeaShow::Piece>& Pieces, int Mine, int Subject, double Now);

	// ---- Where the tags stand on the screen (SSeaBattleScreen draws them where Lay puts them) ---------------------------

	/** A rectangle on the screen, in the screen's own units, down from its top: left, top, right, bottom. */
	struct Box
	{
		double L = 0.0, T = 0.0, R = 0.0, B = 0.0;
		/** Whether it and Other overlap, or come within Room of each other (Room 0: overlap only). */
		bool Touches(const Box& Other, double Room) const;
	};
	/** A tag to stand: its foot (over the top of its piece's leading ship, on the screen), its plate's width, how far in
	 * front of the eye that ship lies, its piece, and whether that piece is the camera's subject. */
	struct Spot
	{
		double X = 0.0, Y = 0.0, Wide = 0.0, Deep = 0.0;
		int Piece = -1;
		bool bSubject = false;
	};
	/** A tag stood: which spot it is (its place among them), its plate, its foot, how often it was lifted over a tag
	 * standing already (0: on its own ship), and its stalk, from the plate down to the foot, as the pieces to draw (top,
	 * bottom) round any other tag's plate it passes behind. */
	struct Laid
	{
		size_t Of = 0;
		Box Plate;
		double FootX = 0.0, FootY = 0.0;
		int Lifts = 0;
		std::vector<std::pair<double, double>> Stalk;
	};
	/** How far a lifted tag stands over the one it clears (between the one's plate and the other's stalk), how near two
	 * tags may come, and how often one is lifted before it is left off. */
	inline constexpr double TagGap = 4.0, TagRoom = 2.0;
	inline constexpr int MostLifts = 3;
	/**
	 * The tags on the screen, never on one another (the UI director's rule for the air scene's tags, b384(d), kept here):
	 * the camera's subject's first, so it stays on its ship, then the nearest to the eye, so the ship in front keeps hers.
	 * Each stands on its foot, its plate High tall and its stalk Stalk under it; one that would touch a tag standing
	 * already (its plate and the first Stalk of its stalk) is lifted just over that one, its stalk run down to its ship
	 * behind the plates it passes, at most MostLifts times, else left off. None covers a place kept (Kept: the bar, the
	 * line, the month scene's caption and its way back to the map), plate or stalk; none lifted off the screen's top.
	 */
	std::vector<Laid> Lay(const std::vector<Spot>& Spots, const std::vector<Box>& Kept, double High, double Stalk);
}
