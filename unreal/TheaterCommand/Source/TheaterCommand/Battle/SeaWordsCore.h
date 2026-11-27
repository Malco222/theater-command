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
 * The words follow the picture (the UI director, 26 November 05:17Z, sea-torpedo-scene-hold-UI-MARKS.md): the rules judge
 * a piece's line of fire from one point while the scene's columns trail behind it, so after the rules let a salvo go the
 * scene may still hold some of its tubes (SeaShow's own holds). So:
 *  - Held: the launcher keeps "Torpedoes held" while any of her tubes is held, by the rules or by the scene, and it goes
 *    off with her last tube's launch. Her line is the built one and names the piece whose ship stands in the water: the
 *    rules' heldForName while the rules hold her, the scene's own blocker while the scene does (the piece across the most
 *    of her held tubes, another piece's before her own, as the show last looked: SeaShow::TubesOut::Across), the name
 *    changing as that piece does. Once any torpedo of the salvo the scene holds is in the water: "Holding the rest of
 *    their torpedoes while ...".
 *  - Turning clear: "Turning clear of torpedoes about to be fired by {phrase}." until the first torpedo of the launcher's
 *    latest salvo is in the water, then "Turning clear of torpedoes fired by {phrase}."; each with the count to clearUntil
 *    (when she turns back on screen; rounded up, never below 0), and no ", once the torpedoes have passed". Routing, the
 *    same "about to be fired by" and "fired by", no count. The tag "Turning clear of torpedoes" in both.
 *  - Both at once: a piece turning clear while the scene still holds some of her own tubes shows the turning-clear tag
 *    and line (they explain the turn the player sees); when the turn ends with tubes still held, the held ones come back.
 *    The rules' own hold still wins over a turn clear, as built, and so does the view's that holds her until the view
 *    that ends it, her salvo told since, while the scene holds some of it (light ships: Destroyers 4 held, "the rest of",
 *    416.3 to 418 s, turning clear from 418 s, as the UI director checked it).
 *  - Never silence: no own piece holds tubes or turns clear without its tag.
 *
 * The fields are the piece's extra in each view, live or recorded (SeaFeed reads them into SeaShow::Piece, as each view
 * says; the show takes a piece's held ones off as its salvo is told after its view, the rules letting it fire, so its tag
 * goes off as its first track leaves unless the scene holds some of it): heldFor and heldForName, clearFrom, clearUntil and
 * clearFromName, ordered; and the scene's picture of the torpedoes (Picture: SeaShow::Show::TubesNow, read only). All of it
 * behind the switch (SeaFeed::FriendlyTorpedoes; the scene's holds are there without it too): off, nothing shows. Only the
 * player's own pieces have words: an enemy piece holds or turns clear with none. The words name a piece as the rules or
 * the log name it (lower case, one to ten ships in words, figures from 11, where the log keeps figures: "four destroyers
 * of the Home Fleet"; a piece the scene's blocker that the rules never named, by its latest salvo's line in the log, "Four
 * destroyers of the Home Fleet fire torpedoes at ...", where its count is still its ships afloat; else no line, the tag
 * alone), never a ship, and never speak of dice or odds.
 */
namespace SeaWords
{
	/** What a piece is doing, as its words say. */
	enum class Kind { None, Held, Clearing };

	/** The tags (the UI director's, exact). */
	inline constexpr const char* HeldTag = "Torpedoes held";
	inline constexpr const char* ClearingTag = "Turning clear of torpedoes";

	/** A piece's fields as its view gives them (SeaShow::Piece keeps them, FieldsOf reads them back), and what the scene
	 * shows of the torpedoes that bear on it (from the Picture). */
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
		/** The scene's picture (none without it: FieldsOf(Piece)). SceneHeld: her tubes the scene still holds, after the rules
		 * let her salvo go; bHeldAway: some torpedo of the salvo it holds them of is in the water; Across: the own piece whose
		 * ship stands across their way (-1 none found), AcrossName its phrase (empty: none known, so no line).
		 * bLauncherAway: some torpedo of the latest salvo of the piece she turns clear of (clearFrom) is in the water. */
		int SceneHeld = 0;
		bool bHeldAway = false;
		int Across = -1;
		std::string AcrossName;
		bool bLauncherAway = false;
		/** The view reached holds her, but her salvo has been told since (the rules have let her fire: HeldFor is off;
		 * SeaShow::Piece::bHeldLetGo). */
		bool bHeldLetGo = false;

		/** Held by the rules (their heldFor), by the scene (her tubes it still holds), either. */
		bool RulesHeld() const { return HeldFor >= 0; }
		bool SceneHolds() const { return SceneHeld > 0; }
		bool Held() const { return RulesHeld() || SceneHolds(); }
		bool Clearing() const { return ClearFrom >= 0; }
		bool Any() const { return Held() || Clearing(); }
		/** What her words show: held while the rules hold her (their hold wins over a turn clear), or the scene does and she
		 * is not turning clear, or is, but the view reached still holds her (her salvo told since: its hold wins over its own
		 * turn clear until the view that ends it); else turning clear, while she is. */
		bool ShowsHeld() const { return RulesHeld() || (SceneHolds() && (!Clearing() || bHeldLetGo)); }
	};
	/** The rules' fields alone, as the show's piece has them (no picture). */
	Fields FieldsOf(const SeaShow::Piece& P);

	/**
	 * What the scene shows of the torpedoes now, for the words (read from the show: PictureOf): by piece, its tubes and
	 * salvos (SeaShow::TubesOut), and the phrase for it where one is known: as the rules last named it in a view the
	 * picture has reached, or its latest salvo's line in the log ("Four destroyers of Force H fire torpedoes at ..."), the
	 * later of the two; either only where its count (if it gives one) is the piece's ships afloat now.
	 */
	struct Picture
	{
		std::map<int, SeaShow::TubesOut> Tubes;
		std::map<int, std::string> Phrases;
	};
	Picture PictureOf(const SeaShow::Show& Show);
	/** The phrase a salvo's line in the log gives its piece ("Four destroyers of Force H fire torpedoes at a battleship of
	 * the 1st Fleet." gives "four destroyers of Force H"; empty where the line is not so made). */
	std::string PhraseOfSalvoLine(const std::string& Line);
	/** A piece's fields as its view gives them, with what the picture shows of the torpedoes that bear on it. */
	Fields FieldsOf(const SeaShow::Piece& P, const Picture& Pic);

	/** The word for one ship or the word for more (is or are, has or have, she or they, her or their), by the ships of the
	 * piece meant, as the rules' many() picks: one ship (or none) the first, two or more the second. */
	const char* Agree(int Ships, const char* One, const char* Many);
	/** The whole seconds from Now to Until, rounded up (the count never says she takes up her orders before she does),
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
	 * none), at battle time Now. NamedShips: the ships afloat now of the piece the held line names (heldFor while the rules
	 * hold her, else the scene's blocker; below 1 not known). The verb after the name agrees with the phrase itself where it
	 * gives a number ("a cruiser ... is", "two destroyers ... are", even in the second the rules keep the name after a ship
	 * of it has gone down), else with NamedShips, else "are". The rules' hold wins over turning clear; turning clear wins
	 * over the scene's hold, but for the view's hold let go (Fields::ShowsHeld). A piece routing turns clear with no count
	 * and no orders to take up: "Breaking off, and turning clear of torpedoes (about to be) fired by ...", ordered or not,
	 * one ship or more (its tag the same; held, its words the same). A piece with no ship afloat, or not the player's, says
	 * nothing; a line needs the name of the piece it speaks of (the tag stands without it).
	 */
	Said Of(const Fields& F, int Side, int Mine, int Ships, int NamedShips, double Now);
	/** The same for a piece of the show, with the picture, the piece it names looked up among Pieces (the show's pieces now). */
	Said Of(const SeaShow::Piece& P, const std::map<int, SeaShow::Piece>& Pieces, const Picture& Pic, int Mine, double Now);

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
	/** The words for pieces as the show has them now (Show::Pieces()) with the picture, the player's side Mine, the camera's
	 * subject piece Subject (-1 none), at battle time Now (Show::Shown()). */
	Words Write(const std::map<int, SeaShow::Piece>& Pieces, const Picture& Pic, int Mine, int Subject, double Now);
	/** The scene's: the show's pieces and picture now, at the moment shown, behind the "Friendly torpedoes" switch (bOn:
	 * SeaFeed::FriendlyTorpedoes; off, nothing at all). */
	Words Write(const SeaShow::Show& Show, int Mine, int Subject, bool bOn);

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
