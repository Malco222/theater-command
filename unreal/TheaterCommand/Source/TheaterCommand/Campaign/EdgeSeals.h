#pragma once

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
		War, // war declared: two strokes crossed
		Fallen, // a country fallen: a chevron pointing down
		Freed, // a country freed: a chevron pointing up
		News, // news with a title of its own, and a line of home or of the staff: three short lines
		Globe, // the rest of the war: a ring, its meridian and its equator
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
	 * hanging at the cut. UTF-8 in and out: only spaces and the ASCII stops are read, so a name is never split.
	 */
	std::string EightWords(const std::string& Text, int Most = 8);

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
	 * logged where it meets one. Meet is FSlateRect::DoRectanglesIntersect's test, as the screen makes it: boxes that
	 * only touch meet too (a seal laid right keeps Keep clear of every piece, so it never even touches one). UnderPiece is
	 * the first of Pieces a seal's box meets, -1 for none.
	 */
	bool Meet(const FBox& A, const FBox& B);
	int UnderPiece(const FBox& Seal, const std::vector<FBox>& Pieces);

	/** When the watch looks: once for each laying (the board's Revision) and slot, the first time its seal is live and has arrived. */
	class FUnderWatch
	{
	public:
		static constexpr int Slots = 16;
		bool Due(int Slot, int Revision, bool bLive, bool bArrived);

	private:
		bool bSeen[Slots] = {};
		int SeenRevision[Slots] = {};
	};

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

		/** One stroke of a drawn mark, from the disc's middle: a bar with round ends (Long by Thick, turned Degrees clockwise), a dot (Long across) or a ring (Long across, its line Thick). */
		enum class EStroke : int
		{
			Bar = 0,
			Dot,
			Ring,
		};
		struct FStroke
		{
			EStroke Kind = EStroke::Bar;
			double X = 0.0;
			double Y = 0.0;
			double Long = 0.0;
			double Thick = 0.0;
			double Degrees = 0.0;
		};

		/**
		 * The strokes of a drawn mark (Warning, War, Fallen, Freed, News, Globe), about SignW across and inside the
		 * hollow, none thinner than MinStrokePx on a screen of PxPerUnit: how many (at most 4). 0 for the order sign and
		 * the "+N", which are drawn otherwise.
		 */
		int MarkOf(EGlyph Glyph, double PxPerUnit, FStroke (&Out)[4]);

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
	}
}
