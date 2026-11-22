#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/**
 * (Release 2, campaign map plan item 5, "News at the map's edge". Brian's card of 16 November 2026, ruling 131:
 * "Reports arrive as seals on the edge of the map, so only your own battles stop the month.")
 *
 * When a month resolves the month's card no longer opens by itself: its items are laid as small wax seals along the
 * edge of the map's clear band, and a press on one opens the card at its row. Ruling 64(g): the seals are the SAME
 * items as the card, in the SAME order (the card's ranking: warnings that need a decision, war and peace that involve
 * him, ground, sea and air, goals and honours, home, elsewhere); 64(d): a seal is put towards the map only where its
 * row has a place.
 *
 * This file is the seals' pure logic, with no engine in it: which items the card has and in what order, how many
 * seals stand and how many fold into "+N", each item's class, colour, pulse, sign and eight words, the seal's look
 * measures (Look: the art director's limits of 22 November), and where on the screen each seal lies: on the edge
 * nearest its row's place as the camera sees the map, never on a piece of the frame (the right column, the date strip,
 * the tray, the minimap) nor on another seal, the seals with no place in a neat stack at the top edge.
 * Tools/edge_seals/run.sh builds it with g++ and checks it. Nothing here is saved: the seals are laid again from the
 * month's card.
 */
namespace EdgeSeals
{
	/** The parts of the month's card, in the card's own order. More is the "+N" seal that stands for the rest. */
	enum class ESection : int
	{
		Band = 0,
		Lead = 1,
		Row = 2,
		Home = 3,
		Staff = 4,
		Elsewhere = 5,
		More = 6,
	};

	/** The four edges of the screen. */
	enum class EEdge : int
	{
		Top = 0,
		Right = 1,
		Bottom = 2,
		Left = 3,
	};

	/** What is pressed into a seal: the row's own order sign, or a mark drawn in line where the row has none. */
	enum class EGlyph : int
	{
		Sign = 0, // the order sign named by the item, its seal picture (Content/CityOrders/Seal), where it may be pressed (MayPress)
		Warning, // a warning that needs his decision: an upright stroke and a dot
		War, // war declared: two swords crossed, each with a guard near its foot (and a pommel where it holds: Look::SwordOf)
		Fallen, // a country fallen: a chevron pointing down
		Freed, // a country freed: a chevron pointing up
		News, // news with a title of its own, and a line of home or of the staff: three short lines
		Globe, // the rest of the war: a ring, its equator and one curved meridian
		More, // the "+N" seal: its count
	};

	struct FPoint
	{
		double X = 0.0;
		double Y = 0.0;
	};

	/** A box on the screen, in the units the seals are laid in. */
	struct FBox
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;

		bool Overlaps(const FBox& Other) const;
		FBox Grown(double By) const;
		bool IsEmpty() const { return MaxX <= MinX || MaxY <= MinY; }
	};

	/** How many of each part the month's card has, as the card is drawn. */
	struct FCardCounts
	{
		int Bands = 0;
		bool bLead = false;
		int Rows = 0;
		int Home = 0;
		int Staff = 0;
		int Elsewhere = 0;
	};

	/** One item of the card: its part, and its place in that part. */
	struct FItem
	{
		ESection Section = ESection::Band;
		int Index = 0;
	};

	/** The card's items, in the card's order: the bands, the lead story, the rows of the fronts, home, the staff, elsewhere. */
	void ListItems(const FCardCounts& Counts, std::vector<FItem>& Out);

	/** How many seals stand for Count items when at most Most may: every item, or Most - 1 of them and a "+N" seal. */
	struct FFold
	{
		int Shown = 0; // the items that have a seal of their own (the first ones, in card order)
		int More = 0; // how many fold into the "+N" seal; 0 where there is none
	};
	FFold Fold(int Count, int Most);

	/**
	 * Ruling 64's class of an item, from its part and the word it carries: a band's or a row's tag (WARNING, WAR,
	 * FALLEN, FREED, NEWS; WON, LOST, ...), the lead story's stamp (LAST MONTH on the month a stake falls due). 1 a
	 * warning that needs his decision; 2 war and peace (a band, or a band's item that found no band); 3 the rest of the
	 * fighting (ground, sea and air, goals and honours alike: the card does not say which); 6 home and the staff; 7
	 * elsewhere; 8 the "+N" seal.
	 */
	int ClassOf(ESection Section, const std::string& Word);

	/**
	 * The colours a seal's wax comes in: the colours of the rows of the month's card (the art director's ruling of 22
	 * November, section 1; rulings 149(g) and 160). EdgeNewsLayer maps each to its inks.
	 */
	enum class EWax : int
	{
		Red = 0, // a dated warning: the only red seal
		Band, // war and peace and the news (class 2): the kit's tc-band--news, its wax (108,80,36) to (85,63,29), rim and sign brass-300
		Green, // good news, and a FREED his own side won
		Amber, // something that blocks or warns
		Brass, // plain brass: home's and the staff's lines, a lead story that is neither good nor bad, the "+N", and bad news (nothing that arrives wears red)
		Grey, // a row of plain news (the card edges it in its muted grey), elsewhere in the war, and next month's at home
	};

	/** A seal's colour, and whether it pulses until it is opened. */
	struct FLook
	{
		EWax Wax = EWax::Brass;
		bool bPulse = false;
	};

	/**
	 * The art director's ruling of 22 November, section 1: a seal wears the colour of the row it opens on the month's
	 * card. Tone is that row's tone as the card colours it: 1 good (for a band, its "good": a FREED his own side won,
	 * which the card edges green), -1 bad, 2 something that blocks or warns, 0 neither, 3 next month's (grey).
	 * - A dated warning is red, and pulses: a WARNING band or row (class 1), and the lead story of the month a stake
	 *   falls due (LAST MONTH), which is that warning (ruling 118(f)) and which the card frames in the warning's red.
	 *   Nothing else is red: a row of bad news wears plain brass.
	 * - WAR, FALLEN, FREED and NEWS (class 2) wear the news band's brass and pulse; one his own side won (Tone 1)
	 *   keeps the card's green, and pulses.
	 * - The rest wear their row's tone, green, amber, plain brass or grey (elsewhere is grey; the "+N" plain brass),
	 *   and do not pulse. A front's row of plain news (Tone 0: a ship sunk or a raid with no side named, no tag) is
	 *   grey, as the card edges it (MonthRowOf's muted A89F8D); home's lines of this month stay plain brass, as the
	 *   card draws them (BrightBrass), and so do the staff's lines and a lead story that is neither good nor bad.
	 */
	FLook LookOf(ESection Section, const std::string& Word, int Tone);

	/**
	 * Whether an order sign may be pressed into the wax. Our own order signs make a seal ours; no crest, eagle, star or
	 * emblem goes in it (the ruling of 22 November, section 2). So the star (goals, honours, a works' order) and the
	 * night raid's sign (a moon and a star) are refused, and so is the day raid's sun (the art director's call of 22
	 * November, 08:18Z: at the seal's weight it reads as a cog, which on a map of factories says industry): the seal
	 * takes its drawn mark instead. (The raids' two signs are the raid card's: no row or line of the month card is
	 * signed "day" or "night" today, as a raid's row is signed bmr by day or by night and its seal presses the bomber.
	 * The refusal keeps the sun and the moon out of the wax if one ever is.)
	 */
	bool MayPress(const std::string& Sign);

	/** What is pressed into the seal of an item that carries Word, where it has an order sign of its own (bSign) or not. */
	EGlyph GlyphOf(ESection Section, const std::string& Word, bool bSign);

	/** The order sign for the lead story, from its picture's name ("troops" an infantry sign, "fleet" a hull...); "" for none. */
	std::string LeadSign(const std::string& Picture);

	/**
	 * A front's row as one line. A land row that names the other side is its head as the card prints it: "Monterrey WON
	 * from Mexico". A row with a tag and no other side named (a band's item that found no band, a ship sunk, a raid)
	 * is its head and the sentence the card prints under it: "Germany WAR: Germany declares war on Poland." A row with
	 * no tag is its sentence.
	 */
	std::string RowLine(const std::string& Place, const std::string& Tag, const std::string& Join, const std::string& JoinName, const std::string& Text);

	/**
	 * The row's eight words, for the pointer on its seal: the line whole where it has no more than Most words, else cut
	 * at a whole word, at the last comma, colon or semicolon that leaves at least half of them where there is one. No
	 * dots are added (the UI director's review of build 1628: nothing is cut with three dots), and no stop is left
	 * hanging at the cut. Nor does a cut end on a lone dash (a stop after it or not), on stops standing apart, or on a
	 * joining or small word (the rules' own list for their cut, monthcard.ts LITTLE: at, in, on, of, to, by, for, from,
	 * with, and, or, but, the, a, an, its, his, their, is, are, was, were, been, has, have, had, as, into, over, under,
	 * round, against, between and the like; and her): it steps back to the last word that says something, so that
	 * neither a seal's words nor a folded seal's first words stop short ("the Finnish troops at", "reports for duty and
	 * is"). Nor does it leave a bracket open: it goes back to before the word that opened it ("VI Corps (36th and 34th
	 * Infantry" is "VI Corps"). A line that is not cut keeps its own last word. UTF-8 in and out: only spaces, the ASCII
	 * stops and brackets are read, so a name is never split.
	 */
	std::string EightWords(const std::string& Text, int Most = 8);

	/**
	 * The design director's ruling of 20:27Z on a folded seal's listed lines: "One rule for the fallback cut, so it never
	 * reads broken either: it ends at a whole word, never on a verb that lacks its object and never on a small word; where
	 * the eighth word is such a word, it steps back to the last noun." ListedCut is EightWords' cut with that rule: where
	 * the last word it keeps is a verb of the news lines that the cut leaves without its object (VerbWithoutObject: "The
	 * escorts of the Western Approaches Fleet sink"), it steps back past it, and past any small word before it, to the last
	 * word that is neither ("The escorts of the Western Approaches Fleet"; "No. 11 Group has driven" is "No. 11 Group").
	 * A verb with a stop after it ends its clause and stays. A seal's own words are EightWords' still (ruling 5).
	 */
	std::string ListedCut(const std::string& Text, int Most = 8);

	/**
	 * Whether a cut that ends on Word, the word before it Before, ends on a verb that lacks its object: one of the news
	 * lines' own verbs that take one (sink, shell, bomb, blow, seize, close, cut, hit, take, lose, declare...: the table
	 * in EdgeSeals.cpp, which the test holds to the verbs it derives from the 48 real months' lines), in a form that wants
	 * what follows it. After "has", "have" or "had" its done form ("has driven"); after "is", "are", "was", "were", "be",
	 * "been" or "being" its -ing form only ("is manning"; "is bombed" is done to it, and whole); else its plain, third person's,
	 * past or -ing form, never a done form that is not also a past ("shipping sunk this month" is whole). A word in
	 * capitals, a row's tag ("HIT"), is a head, never a verb; nor is a word after "the", "a", "its" and the like. Any case
	 * ("Take Lodz"); a stop after the word is not read.
	 */
	bool VerbWithoutObject(const std::string& Before, const std::string& Word);

	/** The most words a folded seal's listed line gives as its whole clause (the design director, 20:27Z). */
	constexpr int ClauseMost = 16;

	/**
	 * A line's whole clause, for a folded seal's listed line (the design director, 20:27Z: "A listed line is its whole
	 * clause, up to 16 words, where the line still holds it"): the line from its start to the end of the clause its eight
	 * words end in (EightWords), that is, to the first word from their last on that ends its clause outside a bracket (a
	 * ",", ";" or ":" after it, or a sentence's end; "No." and "1." end nothing), else to the line's end; no stop or full
	 * stop at its end. Where the eight words end at a stop, it is they ("German troops close round Warsaw"); where they are
	 * the whole line, the line. "" where it would be more than Most words, leave a bracket open, or end on an article or a
	 * joining word (the, a, an, and, or, but), on stops standing alone or on a dash: the list gives the eight-word cut
	 * there (ListedCut). A clause the line ends on a small word of its own is whole ("as they were driven off").
	 */
	std::string WholeClause(const std::string& Text, int Most = ClauseMost);

	/**
	 * Ruling 64(g) as amended 08:55Z, the fold of alike runs: "Three or more in a row that would look the same (wax,
	 * mark, sign, pulse) stand as one seal with their count at the first item's place, and a press opens the card at
	 * that run's first row." FAlike is how an item's seal looks on the edge: its wax, what is pressed in it (its drawn
	 * mark, or the order sign it names) and whether it pulses; the fold compares the looks as the screen draws them
	 * (AsDrawn: below 972 high no order sign is pressed). A WARNING (the dated warning's red wax, or the warning's mark
	 * on any wax) never folds, so no run crosses one either.
	 */
	struct FAlike
	{
		EWax Wax = EWax::Brass;
		EGlyph Glyph = EGlyph::News;
		std::string Sign; // the order sign pressed in it, where Glyph is Sign; empty otherwise
		bool bPulse = false;

		/** A WARNING: the dated warning's red wax, or anything pressed with the warning's mark (GlyphOf: "WARNING", "LAST MONTH"). */
		bool IsWarning() const { return Wax == EWax::Red || Glyph == EGlyph::Warning; }
		bool operator==(const FAlike& Other) const = default;
	};
	/** An item's look, from its part, its word, its tone and the order sign its row names: LookOf's wax and pulse, GlyphOf's mark, the sign where MayPress. */
	FAlike AlikeOf(ESection Section, const std::string& Word, int Tone, const std::string& Sign);
	/**
	 * An item's look as a screen draws it, which is what the fold compares ("would look the same"): where the seal set
	 * is not pressed (bPressed false, Look::SetPressed: under a window 972 high), and on every screen for an order sign
	 * with no seal picture (Look::SignFinest 0), the seal shows the three lines in place of its sign, so its look there
	 * is the News mark, alike with the three lines' seals of its wax and pulse. Any other look is as it is. EdgeNews
	 * folds the month both ways and the screen stands the fold it draws.
	 */
	FAlike AsDrawn(const FAlike& Look, bool bPressed);

	/** A run of the card's items that stands as one seal: the first item's place in card order, and how many (1: a seal of its own). */
	struct FAlikeRun
	{
		int First = 0;
		int Count = 1;
	};
	/**
	 * The card's items as the seals that stand for them, in card order, into Out: each run of Least or more alike items
	 * in a row as one, every other item as its own. Display only: the card and its items are not changed, every item
	 * is in one run and one only, in order, and the runs' counts add up to the items.
	 */
	void FoldAlike(const std::vector<FAlike>& Looks, std::vector<FAlikeRun>& Out, int Least = 3);
	/**
	 * Whether two runs share an item. The month is folded once for each kind of screen (AsDrawn), and a seal opened in
	 * one fold marks opened every seal of the other that shares an item with it, so a window moved across 972 high
	 * keeps what has been read.
	 */
	bool RunsMeet(const FAlikeRun& A, const FAlikeRun& B);

	/**
	 * What the card says of one item, for the words of a folded seal: its part; the line its seal's own eight words
	 * are cut from (Line); the card's sentence for it (Sentence: a band's or a row's text, the lead's headline, the line
	 * of home as said, the staff's, elsewhere's); and for a front's row its place (Name) and the rest of its head as the
	 * card prints it (Head: "LOST against Germany", "HIT").
	 */
	struct FSaid
	{
		ESection Section = ESection::Band;
		std::string Line;
		std::string Sentence;
		std::string Name;
		std::string Head;
	};
	/** A front's row as FSaid: its line (RowLine), its sentence (its text, else its line), its place, and its head where it has a tag. */
	FSaid RowSaid(const std::string& Place, const std::string& Tag, const std::string& Join, const std::string& JoinName, const std::string& Text);
	/** Any other item as FSaid: its line and its sentence are the same words. */
	FSaid PlainSaid(ESection Section, const std::string& Text);

	/**
	 * The hover: the engine's own tip, which the layer gives a seal's words (SWidget::SetToolTipText, EdgeNewsLayer) and
	 * the frame dresses as it dresses every tip (CampaignHUD, "ToolTip.Font": the typed face at TipPt). The layer gives
	 * it no size of its own: the engine sizes it to its words and wraps them at Slate.ToolTipWrapWidth, TipWrap units
	 * (the engine's own 1000). The face is Courier Prime, every letter of it TipAdvance of an em wide (1228 of 2048, from
	 * the font file), so TipPt's em is TipPt * 4/3 units and a letter 7.6 of them. But Slate sets each letter a whole
	 * pixel's advance at the scale it draws the tip (the shaped glyph's advance is a whole number of pixels, the face
	 * hinted), so on a screen of Scale pixels a unit a letter is round(7.6 * Scale) / Scale units, and one line holds
	 * TipLettersAt(Scale) letters: 125 at a pixel a unit (1920 by 1080), 133 at two, 120 in a window 783 high, 118 in
	 * one 640 high; 131 only were the advance not a whole pixel. The tip is drawn at the game layer's scale (the window's
	 * height over 1080, Unreal's own curve) or, where the engine gives it a window of its own, at the desktop's (one to
	 * three pixels a point); which is not read here, as Unreal is not installed. So the hover's own size is the least
	 * line of either on every screen the seals are held to: TipLetters, 118 letters. A hover of more wraps to a second
	 * line on some screen. Today four of the real months' hovers are longer than that, the longest a folded seal's of 132
	 * letters, which wraps at 1920 by 1080.
	 */
	constexpr double TipPt = 9.5;
	constexpr double TipWrap = 1000.0;
	constexpr double TipAdvance = 1228.0 / 2048.0;
	constexpr double TipLeastHigh = 600.0; // the least window the seals are held to, as their strokes' 1.5 px floor is (Look::Stroke)
	constexpr double TipMostScale = 3.0; // the most pixels a unit (and a desktop's pixels a point) they are held to
	/** The letters one line of the hover holds on a screen of Scale pixels a unit: as many as are no wider, together, than TipWrap, each at its whole-pixel advance. */
	int TipLettersAt(double Scale);
	/**
	 * The letters one line of the hover holds on every screen: the least TipLettersAt of the game layer's scale for
	 * every window from TipLeastHigh to TipMostScale * 1080 high, and of the desktop's from 1 to TipMostScale in quarters.
	 */
	int TipLetters();
	/** How many letters some words are (UTF-8 in: each letter one advance of the typed face, "4ª" two). */
	int LettersOf(const std::string& Words);

	/**
	 * An item's own short line, for a folded seal that lists its items: a front's row's title as the card prints it, its
	 * place and its head ("Polish Silesia LOST against Germany", "Ruhr HIT"); any other item's first words, cut at a
	 * whole word by ListedCut, EightWords' own rules and never on a verb without its object ("Soviet troops blow the
	 * bridges over the Oka", "The escorts of the Western Approaches Fleet"). No stop or full stop at its end. The shortest
	 * line the list gives the item; where the hover's line holds it, the list gives its whole clause (FoldWords).
	 */
	std::string ShortLine(const FSaid& Said);

	/**
	 * The words under the pointer for a folded seal: the run's items named in card order, and how many more, in nothing
	 * the card does not say, on one line of the hover at most (Room letters: TipLetters).
	 * - Where each sentence is a name (one to four words, none named twice; a thing named after a nation's adjective,
	 *   "German troops", is the name's) and then the same words, those words up to any colon, their verb made plural
	 *   and "its" made "their", and the names: "9 declare war on Germany: Cuba, Panama, Costa Rica and 6 more".
	 * - Else, where each is a front's row with the same head, that head and the places: "3 HIT: Ruhr, Hamburg and Bohemia".
	 * - Else (the design director, 64(g)(4), 13:00Z) each item's own line, in card order, one after another, as many as
	 *   the hover's one line holds, then "and N more" for the rest: "Soviet troops close round Helsinki; Soviet troops
	 *   blow the bridges over the Oka; and 3 more". The lines stand apart by "; ", so the "and N more" is one more of the
	 *   list, never more of the line before it. Each listed line (the design director, 20:27Z) is the item's whole clause
	 *   (WholeClause, up to ClauseMost words) where the hover's line still holds it with how many more after it, else its
	 *   short line (ShortLine: a row's title, which comes first where the row has one, else its eight words cut by
	 *   ListedCut, never on a small word or a verb without its object); the list ends at the first item that fits neither
	 *   way (unless every item left, each short, fits with no "and N more" after them), and then each line still short is
	 *   given whole where the line, as it then stands, holds it: "The escorts of the Western Approaches Fleet sink 1 German
	 *   submarine in the Western Approaches; and 2 more". The hover is never longer for it. A row whose title the
	 *   list gives already gives its own first words, its title first ("Stalino HIT: The Black Sea Fleet shelled the
	 *   troops ashore", or cut, "Stalino HIT: The Black Sea Fleet"). A first line too long for the hover with its "and N
	 *   more" even cut is cut a word shorter until it fits (ListedCut), never into a row's title ("Ruhr HIT", never
	 *   "Ruhr"); where it cannot, or no item has words of its own, the count and where on the card they are ("3 elsewhere
	 *   in the war").
	 * Names are listed as the rules list them (listNames): up to four whole, more as the first three "and N more"; a
	 * name's "The" is "the" in the list.
	 */
	std::string FoldWords(const std::vector<FSaid>& Said, const FAlikeRun& Run, int Room = TipLetters());
	/** A verb as it stands after more than one name: "declares" "declare", "has" "have", "is" "are", "carries" "carry". A word with no third person's -s (a past, a word in capitals) is left as it is. */
	std::string PluralVerb(const std::string& Word);

	/** The sizes the seals are laid out with, in the screen's units (the frame's own, before its scale). */
	struct FParams
	{
		double ScreenW = 1920.0;
		double ScreenH = 1080.0;
		double SealW = 34.0; // a seal's box: the disc and the ribbon's tail under it
		double SealH = 44.0;
		double Margin = 14.0; // from the screen's edge to a seal's box
		double Gap = 6.0; // between two seals
		double Keep = 8.0; // between a seal and a piece of the frame
		double Stick = 48.0; // how much nearer another edge must be before a seal leaves the edge it lies on
	};

	/**
	 * What a seal asks for: a place on the screen, where its row has a place the camera can put there; and the edge it
	 * lies on already (-1 for none), which it keeps while no other edge is nearer by more than Stick, so that a place
	 * near a corner does not send its seal from one edge to the other and back as the camera moves.
	 */
	struct FWant
	{
		bool bPlace = false;
		FPoint At;
		int Kept = -1;
	};

	/** Where a seal lies: its edge, the middle of its box, and the point off the screen it slides in from. */
	struct FSpot
	{
		bool bShown = false; // false only where every edge is full
		EEdge Edge = EEdge::Top;
		FPoint Centre;
		FPoint From;
	};

	/**
	 * The seals laid along the edge. Keeps its working lists between calls, so that laying the seals again every frame
	 * as the camera moves allocates nothing once the lists have grown to the month's size.
	 */
	class FLayout
	{
	public:
		/**
		 * Lays one seal for each want, in card order, into Out (resized to match). The seals with no place go first, in a
		 * neat stack in the middle of the top edge (the one stretch of it nearest the middle that takes them all, else one
		 * by one); then each seal with a place, on the edge nearest that place and as near it along the edge as the frame
		 * and the seals before it allow, else on the next nearest edge. Reserved are the frame's pieces: no seal's box
		 * comes within Keep of one, nor within Gap of another seal's.
		 */
		void Lay(const FParams& Params, const std::vector<FWant>& Wants, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out);

		/**
		 * The month's seals as the screen shows them, in the card's ranking (ruling 64(g): the same items as the card, and
		 * nothing lost without trace). Items holds what the card's first items want, in card order; Total is how many
		 * items the card has in all. At most Most seals stand: every item where Total <= Most, else the first Most - 1
		 * and a "+N" seal for the rest. Where the edge has no room for them all, the last item standing folds into the
		 * "+N" seal, then the one before it, until what is left is laid whole: so no item stands while one ranked above it
		 * does not, and every item that does not stand is counted in the "+N". Out holds the spots of the items that
		 * stand, in card order, and after them the "+N" seal's where More > 0. Returns how many items stand (Shown) and
		 * how many fold into the "+N" (More). Where not even the "+N" seal alone has room, Shown is 0 and its spot is
		 * not shown.
		 */
		FFold LayRanked(const FParams& Params, const std::vector<FWant>& Items, int Total, int Most, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out);

		/**
		 * The same, for seals that may each stand for a run of alike items (FoldAlike): Seals holds what each seal wants,
		 * in card order, and Carries how many items each stands for (one where Carries has no entry for it). Every seal
		 * given stands, with no "+N", only where they are no more than Most and carry all Total items; else at most Most - 1
		 * stand and the "+N" seal counts the items the rest carry, and the items past them. Returns how many seals stand
		 * (Shown) and how many items the "+N" counts (More): the "+N" counts items, never seals.
		 */
		FFold LayRanked(const FParams& Params, const std::vector<FWant>& Seals, const std::vector<int>& Carries, int Total, int Most, const std::vector<FBox>& Reserved, std::vector<FSpot>& Out);

	private:
		/** Of the points of an edge where a box of the given half sizes stands clear of every obstacle, the one nearest Wanted. */
		bool FreeNearest(const FParams& Params, EEdge Edge, double Wanted, double HalfAlong, double& OutAt);
		/** Puts one seal on the first of its edges (nearest first) that has room; false where none has. */
		bool PlaceOne(const FParams& Params, const EEdge* Edges, int EdgeCount, double Along[4], FSpot& Out);
		void Claim(const FParams& Params, const FSpot& Spot);

		std::vector<FBox> Obstacles;
		std::vector<std::pair<double, double>> Blocked;
		std::vector<FWant> Trial;
	};

	/**
	 * The under-frame watch (Mac run 3: the "+N" seal was laid and painted under the top bar, its keep-off boxes read in
	 * the wrong measure). Not in shipping builds: each seal, once per laying, once it is on the screen and has slid all
	 * the way in, is held against every box it keeps off, both as last painted (the window's measure), and a warning is
	 * logged where it meets one. Meet is FSlateRect::DoRectanglesIntersect's test: boxes that only touch meet too (a seal
	 * laid right keeps Keep clear of every piece, so it never even touches one). UnderPiece is the first of Pieces a
	 * seal's box meets, -1 for none: the layer's watch calls it with the boxes as painted, so the test holds what runs.
	 */
	bool Meet(const FBox& A, const FBox& B);
	int UnderPiece(const FBox& Seal, const std::vector<FBox>& Pieces);

	/**
	 * When the watch looks: once for each laying (the board's Revision) and slot, the first time its seal is live and has
	 * arrived; and once more each time the slot comes to hold another seal in the same laying (Holding: 1 for the "+N",
	 * 0 for the board's own), as when the tray comes up and the last items fold into a "+N" that takes the slot of one
	 * that stood. Mac run 3's fault was a "+N" under the top bar: a "+N" laid after the first look is looked at too.
	 */
	class FUnderWatch
	{
	public:
		static constexpr int Slots = 16;
		bool Due(int Slot, int Revision, bool bLive, bool bArrived, int Holding = 0);

	private:
		bool bSeen[Slots] = {};
		int SeenRevision[Slots] = {};
		int SeenHolding[Slots] = {};
	};

	/**
	 * The photographer's asks of the seals: "-TheaterSeal=" on the command line, its parts joined by commas (a
	 * development tool, not part of the game; EdgeNewsLayer reads the line once, as the layer is made). A seal is
	 * named by its place among the seals standing on the edge, in card order, from 1 (the "+N" counted where it stands,
	 * as the under-frame watch names them).
	 * - "open:N": the Nth seal shows as opened, in its dull grey (on the screen only: no card opens).
	 * - "hover:N": the Nth seal as if the pointer rested on it: it grows, the brass ring goes round it, and the ping
	 *   spreads from its place on the map. Its words go to the log, since the engine's tip is not in a picture.
	 * - "slide": each laying waits beyond the edge until a picture is taken, and sets off as that picture is taken, so
	 *   that the photographer's clip after its first picture ("-TheaterClip=30,0.05") holds the slide-in. With it the
	 *   layer times the slide-in, the call and the ping on the world's clock, which the clip steps by its own gap, not
	 *   on the computer's, which a tick that takes a picture runs on by a quarter of a second and more.
	 * A part it does not know, or a number under 1 or past Most (the most seals that may stand, FEdgeNewsBoard::MostSeals),
	 * is counted in Unknown and left out. A seal named that does not stand on this month's edge is said so in the log
	 * (EdgeNewsLayer), and nothing is shown for it.
	 */
	struct FSealAsk
	{
		int Open = 0;
		int Hover = 0;
		bool bSlide = false;
		int Unknown = 0;
	};
	FSealAsk ReadSealAsk(const std::string& Asked, int Most = 9);

	/** An edge's lane: the line the middles of its seals lie on (Fixed), and the stretch of it they may take (Low to High). */
	struct FLane
	{
		bool bAlongX = true;
		double Fixed = 0.0;
		double Low = 0.0;
		double High = 0.0;
	};
	FLane LaneOf(const FParams& Params, EEdge Edge);

	/** How far a point is from an edge's lane, and where along the lane it falls (clamped to the lane). */
	double DistanceToLane(const FLane& Lane, const FPoint& At, double& OutAlong);

	/** The point off the screen beyond an edge that a seal standing at Centre slides in from. */
	FPoint FromOffScreen(const FParams& Params, EEdge Edge, const FPoint& Centre);

	/**
	 * How far in a seal has slid, 0 to 1, Since seconds after the seals were laid: the Order-th in card order sets off
	 * Order * Stagger seconds after the first, and takes Slide seconds, slowing as it arrives.
	 */
	double SlideShare(double Since, int Order, double Stagger = 0.12, double Slide = 0.38);

	/** The slow pulse of a seal that calls (FLook::bPulse) and is not yet opened, 0 to 1 and back over Period seconds. */
	double Pulse(double Now, double Period = 2.4);

	/**
	 * The seal as EdgeNewsLayer draws it, in the frame's units (its box is FParams' 34 by 44, the disc at its head), to
	 * the art director's look limits of 22 November, section 2, which the test holds it to. On the screen a unit is
	 * the frame's FrameScale times Unreal's own scale for the window: about a pixel at 1600 by 1000, 0.74 of one in a
	 * window 720 high, about 1.84 in Brian's window on the Mac's fine pixels (1786 high). What must hold in pixels is
	 * measured at that scale (PxPerUnit: EdgeNewsLayer measures it, the viewport's pixels over the layer's units).
	 */
	namespace Look
	{
		constexpr double BoxW = 34.0;
		constexpr double BoxH = 44.0;
		constexpr double MidX = 17.0; // the disc's middle in the box
		constexpr double MidY = 17.0;
		constexpr double DiscR = 15.0; // the disc: 30 across, 30 tall
		constexpr double HollowR = 12.0; // the hollow the die pressed
		constexpr double RimW = 1.3; // the rim's line
		constexpr double SignW = 18.0; // the pressed sign: about 18 across (a pixel a unit at 1600 by 1000), at most 12.5 tall
		constexpr double SignH = 12.5;
		constexpr double MinStrokePx = 1.5; // no stroke of a pressed mark under 1.5 pixels on the screen
		constexpr double LightX = -0.6; // the light, as the offset towards it: from the upper left, like the kit's other raised pieces
		constexpr double LightY = -0.7;
		constexpr double Breath = 0.035; // a seal that calls grows by at most this share as it breathes
		constexpr double HoverGrow = 1.08; // and by this under the pointer

		/** The four small runs of wax at the rim: where (degrees clockwise from the right), how big, and how far out their middles lie. */
		struct FRun
		{
			double Degrees = 0.0;
			double Radius = 0.0;
			double Out = 0.0;
		};
		constexpr int RunCount = 4;
		// (uneven in place and in size, and soft: wax, never a gear's teeth or a badge's scalloped edge)
		inline constexpr FRun Runs[RunCount] = { { 31.0, 2.9, 13.4 }, { 142.0, 2.2, 13.9 }, { 203.0, 3.4, 13.0 }, { 296.0, 2.6, 13.6 } };

		/** The ribbon's two tails from under the disc, in the date strip's paper: their middles, length, width and turn (degrees clockwise). */
		struct FTail
		{
			double X = 0.0;
			double Y = 0.0;
			double Long = 0.0;
			double Thick = 0.0;
			double Degrees = 0.0;
		};
		// (short: neither is longer than the disc is tall)
		inline constexpr FTail Tails[2] = { { 10.2, 33.4, 16.0, 5.6, 108.0 }, { 23.8, 33.4, 16.0, 5.6, 72.0 } };

		/** A stroke Units thick, made no thinner than MinStrokePx on a screen of PxPerUnit pixels to a unit. */
		double Stroke(double Units, double PxPerUnit);

		/** An order sign's picture, DrawnW by DrawnH, fitted into SignW by SignH as it was drawn: its size (X wide, Y high). */
		FPoint FitSign(double DrawnW, double DrawnH);

		/**
		 * The width the seal signs' pictures are drawn at (Content/CityOrders/Seal/seal-<key>.png, 192 by 128, as the
		 * badges' order-<key>.png are): their measures' unit.
		 */
		constexpr double SignPictureW = 192.0;

		/**
		 * The seal set (the art director's ruling of 22 November, 06:45Z): each order sign the seals press has a picture
		 * of its own, drawn for 1920 by 1080 and used on every screen, its strokes thickened by offsetting them (round
		 * caps kept) so that the thinnest is about 16 of the picture's 192 pixels, and every gap and counter as wide,
		 * a detail that would close dropped or merged (Tools/edge_seals/seal_signs.py, which draws them and writes these
		 * measures). The badges elsewhere keep their own pictures.
		 *
		 * SignThinnest: a seal sign's thinnest stroke, SignNarrowestGap its narrowest gap or counter, in pixels of its
		 * picture as drawn SignPictureW wide, as the test measures the pictures (the widest disc that opens the ink, or
		 * the ground, taking nothing deeper than a pixel). At a corner that measure lets a sliver up to a pixel deep pass,
		 * so it reads a round cap or a fillet 16 across as 16 to about 23, by the corner's angle: the two say that no part
		 * is under the floor (none under SealFloor less half a pixel), not how wide the finest part is.
		 * SignFinest: the finest part the screen must hold at 1.5 px, SealFloor for every sign with a seal picture (the
		 * set's caps, gaps and fillets are all drawn 16 across), so the whole set is pressed or not together. 0 for a sign
		 * with no seal picture: it is not pressed.
		 */
		double SignThinnest(const std::string& Sign);
		double SignNarrowestGap(const std::string& Sign);
		double SignFinest(const std::string& Sign);

		/**
		 * The seal set's finest part as drawn, in pixels of its pictures (SignPictureW wide): the offset of 8 makes every
		 * stroke's round cap 16 across, and the closing of 8 every gap, counter and fillet, so 16 is the thinnest stroke
		 * and the narrowest gap of every seal sign alike (Tools/edge_seals/seal_signs.py, FLOOR).
		 */
		constexpr double SealFloor = 16.0;

		/**
		 * Whether a seal sign whose finest part is Finest (SignFinest), its picture DrawnW by DrawnH, keeps that part at
		 * MinStrokePx or more once fitted into SignW by SignH (FitSign) on a screen of PxPerUnit pixels to a unit (none,
		 * or no measure, is taken at a pixel a unit). Where it does not, the seal takes its drawn mark: under 1.5 px a
		 * stroke turns to a smudge and a gap fills in (the art director's ruling of 22 November, section 2). The seal set,
		 * 16 of 192 fitted 18 wide, is 1.5 units: it holds from a pixel a unit, a window 972 high (1920 by 1080 and up),
		 * every sign at once; below it, every seal the three lines.
		 */
		bool SignHolds(double Finest, double DrawnW, double DrawnH, double PxPerUnit);

		/** The height the seal signs' pictures are drawn at, with SignPictureW (the test reads every picture at 192 by 128). */
		constexpr double SignPictureH = 128.0;
		/**
		 * Whether the seal set is pressed on a screen of PxPerUnit pixels to a unit: SignHolds for its finest part
		 * (SealFloor) in a picture SignPictureW by SignPictureH, from a window 972 high up. Below it every seal sign is
		 * drawn as the three lines, and the screen stands the month folded as so drawn (AsDrawn).
		 */
		bool SetPressed(double PxPerUnit);

		/**
		 * One stroke of a drawn mark, from the disc's middle: a bar with round ends (Long by Thick, turned Degrees
		 * clockwise), a dot (Long across), a ring (Long across, its line Thick) or an upright ellipse (Long high and Wide
		 * across, its line Thick; drawn as EllipseSteps short pieces, EllipsePoint).
		 */
		enum class EStroke : int
		{
			Bar = 0,
			Dot,
			Ring,
			Ellipse,
		};
		struct FStroke
		{
			EStroke Kind = EStroke::Bar;
			double X = 0.0;
			double Y = 0.0;
			double Long = 0.0;
			double Thick = 0.0;
			double Degrees = 0.0;
			double Wide = 0.0;
		};

		/**
		 * The globe's meridian (the art director, 64(g)(4)): an upright ellipse GlobeMeridian of the ring's width across,
		 * meeting the ring at its poles, so that no straight upright stroke stands in the globe (the old straight
		 * meridian made it read as a cross in a circle). Under a window 593 high, where its line is thickened to hold
		 * 1.5 px, it is widened to keep 1.5 px open inside it (up to the ring's width), so it never fills in to a bar.
		 */
		constexpr double GlobeMeridian = 0.45;
		/** How many short straight pieces an ellipse stroke's line is drawn in: each shorter than its line is thick, so none reads as straight. */
		constexpr int EllipseSteps = 48;
		/** The point Step of EllipseSteps on an ellipse stroke's middle line, from the disc's middle: 0 at its top, then round clockwise; EllipseSteps is the top again. */
		FPoint EllipsePoint(const FStroke& Cut, int Step);

		/** The most strokes a drawn mark has (the crossed swords' ten: each a blade, its point, a guard, a grip and a pommel). */
		constexpr int MarkMost = 10;

		/**
		 * The strokes of a drawn mark (Warning, War, Fallen, Freed, News, Globe), about SignW across and inside the
		 * hollow, none thinner than MinStrokePx on a screen of PxPerUnit, into Out, which has Room for so many: how many
		 * it wrote (at most MarkMost; a mark with more strokes than Room is cut short, so give it MarkMost). 0 for the
		 * order sign and the "+N", which are drawn otherwise.
		 */
		int MarkOf(EGlyph Glyph, double PxPerUnit, FStroke* Out, int Room);
		template <std::size_t Room>
		int MarkOf(EGlyph Glyph, double PxPerUnit, FStroke (&Out)[Room])
		{
			return MarkOf(Glyph, PxPerUnit, Out, static_cast<int>(Room));
		}

		/**
		 * The war mark's two swords (the art director, 22 November, 08:36Z: the plain X read "cancel"). They cross flat,
		 * as the X did and as sabres are, Turn degrees off the level, the points up and out and the hilts down and out;
		 * the second is the first seen in a mirror. Each is a blade from its guard to its point, long above the crossing
		 * and narrowing over its last Point units to the tip (PointThick); a short guard across the blade near its foot;
		 * a short grip under the guard; and, where it holds 1.5 px, a small round pommel at the grip's end. So the
		 * outline is two swords, long blades rising and short hilts below, not the X's four equal arms. Along a sword,
		 * from where the two cross (Lift below the disc's middle, so that the mark's box is centred on it): the tip at
		 * Tip, the guard's middle at Guard, the grip's end at GripEnd, the pommel's middle at Pommel (all but the tip
		 * below, so negative). The blade is as heavy as the X's strokes were, and the whole about as large: about 18
		 * across and 13 high, inside the hollow.
		 *
		 * What must hold 1.5 px on the screen, as every stroke of a mark does, each measured as the eye sees it, on each
		 * side: the guard's width (GuardThick), and how far it stands out past the blade at its full width (Overhang less
		 * the guard's round end, GuardThick / 2: made longer on a small screen so that it holds, to at most 4 units);
		 * and for the pommel, the grip left bare between the guard and the pommel (Neck) and how far the pommel stands
		 * past its grip on each side (half of PommelD less GripThick), without which it would read as the grip's own end.
		 * Where either is under 1.5 px, the pommel is left off (bPommel false) and the grip ends round: as drawn, under
		 * 1.875 px a unit, a window 1823 high (the art director's call: the floor per side, or across the pommel).
		 */
		struct FSword
		{
			double Turn = 36.0;
			double Lift = 0.0;
			double Tip = 0.0;
			double BladeThick = 0.0;
			double Point = 0.0;
			double PointThick = 0.0;
			double Guard = 0.0;
			double GuardLong = 0.0;
			double GuardThick = 0.0;
			double Overhang = 0.0;
			double GripThick = 0.0;
			double GripEnd = 0.0;
			double Pommel = 0.0;
			double PommelD = 0.0;
			double Neck = 0.0;
			bool bPommel = false;
		};
		FSword SwordOf(double PxPerUnit);

		/**
		 * How far the pressed sign's other wall is drawn from it, along the light's diagonal (both X and Y): about a
		 * pixel at PxPerUnit, 0.75 to 1 unit, so that the sign with its wall stays inside the hollow.
		 */
		double WallOf(double PxPerUnit);

		/**
		 * The hollow's two lips, lit from the upper left: the ring that catches the light is moved towards the light, so
		 * that the part of it inside the hollow lies along the lower-right inner wall; the ring in shadow is moved away,
		 * so that inside the hollow it lies along the upper-left wall. Offsets from the disc's middle, and their radius.
		 */
		struct FLips
		{
			double LitX = 0.0;
			double LitY = 0.0;
			double ShadeX = 0.0;
			double ShadeY = 0.0;
			double R = 0.0;
		};
		FLips HollowLips();

		/**
		 * The call of a seal that pulses, Since seconds after the seals were laid: it breathes (Grow), a glow comes and
		 * goes under it, and a ring spreads from its rim, rising over the first tenth of its spread (it never pops in)
		 * and fading to nothing at its widest. The glow and the ring are drawn in the seal's own colours (the glow in its
		 * wax, the ring in its wax or, for the news band's seal, its brass-300 rim) and never more opaque than 55%, so
		 * neither is ever brighter than the seal; the ring at its widest, under the pointer's grow too, is no wider than
		 * twice the disc.
		 */
		struct FCall
		{
			double Grow = 1.0;
			double GlowR = 0.0;
			double Glow = 0.0;
			double RingR = 0.0;
			double RingW = 0.0;
			double Ring = 0.0;
		};
		FCall CallOf(double Since, double Period = 2.4);

		/**
		 * A folded seal's count (ruling 64(g) as amended 08:55Z): a small tab of the date strip's paper laid over the
		 * ribbon's tails under the disc, its figures in dark ink in the kit's typed face (Courier Prime Bold,
		 * CampaignHUD's Typed) at CountPt. The tab lies wholly under the disc's foot and its rim (TabTop), so the figures
		 * are never on the wax, and inside the seal's box, which stays 34 by 44: a folded seal is the size of any other,
		 * and breathes and grows under the pointer as one.
		 *
		 * The paper runs the tab's full height, from TabTop to the box's foot, so the figures stand on paper and on
		 * nothing else: the tab is edged in the ribbon's darker fold at its two sides only (TabEdge, a line held to
		 * 1.5 px), which parts it from the tails, and between the edge and the figures' ink lies TabPaper of paper at the
		 * least. (Edged all round, the edge drawn inside the tab lay under the figures' tops and feet: 13 to 35% of their
		 * ink was on it, by the screen.) So the tab is as wide as its figures, two edges and their paper, by the screen.
		 * One figure stands over the tails' roots; two cover most of the tails, which is the art director's to rule on (the
		 * two figures need about 19 units of ink, and the tails part by 7 to 15 below the disc). Three, for a run of a
		 * hundred (which no month's card has), are drawn in a tab as wide as the box, their outer figures over its edges.
		 *
		 * The face's measures, in ems (from the font file, CourierPrime-Bold.ttf, 2048 to the em): the figures' ink runs
		 * from FigureBottom (the round ones' overshoot) to FigureTop (the 1) about the baseline, FigureH in all, and is at
		 * most FigureInk wide (the 4) inside an advance of FigureAdvance; the line Slate measures runs from Ascent above
		 * the baseline to Descent below it (the face has no line gap). A Slate point is 4/3 of a unit (96 a inch), so
		 * CountPt's em is CountPt * 4/3 units, and its figures stand 9.8 px tall at 1441 by 878 and 12.1 at 1920 by 1080.
		 * The art director's floor of 9 px (FigureFloorPx) holds at CountPt from a window 806 high up (FigureFloorHigh).
		 * Under it the figures are drawn larger, never CountPt raised (the art director, 64(g)(4), ruling 3: the numerals
		 * at the larger of CountPt's size and 9 px): FigureGrow, so 9 px at 1024 by 768, where the tab widens with its
		 * figures, about 3 to 4%; every window from 806 high up keeps its tab and its figures exactly as they were. The
		 * figures are never taller than the tab, which holds 9 px from a window 754 high. FigurePx is how tall they are
		 * drawn. CampaignHUD's mount line asks Typed for CountPt (13.5): the layer takes the face as it is given and draws
		 * it FigureGrow larger (the test reads the mount line and holds the two to each other).
		 */
		constexpr double TabTop = 32.4;
		constexpr double TabBottom = BoxH;
		constexpr double TabPaper = 1.0; // the paper between a side's edge and the figures' ink, at the least
		constexpr double TabRound = 1.6;
		constexpr double CountPt = 13.5;
		constexpr double FigureTop = 0.592;
		constexpr double FigureBottom = -0.011;
		constexpr double FigureH = FigureTop - FigureBottom;
		constexpr double FigureInk = 0.48;
		constexpr double FigureAdvance = 0.6;
		constexpr double Ascent = 0.78125;
		constexpr double Descent = 0.341796875;
		constexpr int CountMostFigures = 3; // a run is never so long; a longer count is drawn in the tab of three
		constexpr double FigureFloorPx = 9.0; // the art director's floor for the count's figures

		/** The count's em at CountPt, in units. */
		constexpr double CountEm() { return CountPt * 4.0 / 3.0; }
		/**
		 * How much larger than CountPt's the figures are drawn on a screen of PxPerUnit: enough for them to stand
		 * FigureFloorPx tall, and no taller than the tab; exactly 1 where CountPt's stand so tall already, from a window
		 * FigureFloorHigh high up.
		 */
		double FigureGrow(double PxPerUnit);
		/** The em the figures are drawn at on a screen of PxPerUnit, in units: CountEm grown by FigureGrow. */
		double CountEmAt(double PxPerUnit);
		/** How far above the middle of its line, as Slate measures it, the figures' ink has its middle on a screen of PxPerUnit, in units: the line is set this much below the tab's middle. */
		double FigureLift(double PxPerUnit);
		/** The fold-coloured edge at each side of the tab, on a screen of PxPerUnit: a unit, held to 1.5 px. */
		double TabEdge(double PxPerUnit);
		/**
		 * The tab for a count of Figures figures (1 to CountMostFigures) on a screen of PxPerUnit, centred under the
		 * disc: the figures' ink as the face sets them, TabPaper of paper and TabEdge of edge either side, as wide as the
		 * seal's box at the most. The figures stand FigureTrack closer than the face sets them, so the paper at each side
		 * of two figures is half a pixel more than TabPaper.
		 */
		FBox TabOf(int Figures, double PxPerUnit);
		/**
		 * Where the figures' ink lies on it: centred in the tab, as wide as Figures figures set FigureTrack closer than the
		 * face sets them, FigureH tall at the em they are drawn at.
		 */
		FBox FiguresOf(int Figures, double PxPerUnit);
		/**
		 * How much closer than the face's advance two neighbouring figures stand (the art director, 64(g)(4), ruling 4):
		 * 1 px, in units, so that two figures leave half a pixel more paper at each side of a tab no wider than before,
		 * and never touch its edges, as "21" did. Never more than three quarters of the room between two of the widest
		 * figures' ink, so they never touch each other (which binds only under a window 562 high). The figures are
		 * never drawn smaller to save width.
		 */
		double FigureTrack(double PxPerUnit);
		/** Where the middle of figure Index (from 0, of Figures) stands across the seal's box, in units: one advance less FigureTrack from the next, the figures centred on the tab. */
		double FigureAt(int Index, int Figures, double PxPerUnit);
		/** How tall the figures' ink is drawn on a screen of PxPerUnit pixels to a unit (grown by FigureGrow). */
		double FigurePx(double PxPerUnit);
		/** The lowest window, in pixels high, from which CountPt's own figures stand FigureFloorPx tall or more (a unit is the window's height over 972): under it they are grown. */
		double FigureFloorHigh();
		/** How many figures a count has. */
		int FiguresIn(int Count);
	}
}
