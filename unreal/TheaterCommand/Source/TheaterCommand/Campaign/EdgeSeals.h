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
 * seals stand and how many fold into "+N", each item's class, sign and eight words, and where on the screen each
 * seal lies: on the edge nearest its row's place as the camera sees the map, never on a piece of the frame (the right
 * column, the date strip, the tray, the minimap) nor on another seal, the seals with no place in a neat stack at the
 * top edge. Tools/edge_seals/run.sh builds it with g++ and checks it. Nothing here is saved: the seals are laid
 * again from the month's card.
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
		Sign = 0, // the order sign named by the item (Content/CityOrders)
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

	/** Classes 1 and 2 are red, and pulse until they are opened. */
	inline bool IsRed(int Class) { return Class >= 1 && Class <= 2; }

	/** What is pressed into the seal of an item that carries Word, where it has an order sign of its own (bSign) or not. */
	EGlyph GlyphOf(ESection Section, const std::string& Word, bool bSign);

	/** The order sign for the lead story, from its picture's name ("troops" an infantry sign, "fleet" a hull...); "" for none. */
	std::string LeadSign(const std::string& Picture);

	/** A front's row as one line: "Monterrey WON from Mexico"; a row with no tag is its sentence. */
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

	private:
		/** Of the points of an edge where a box of the given half sizes stands clear of every obstacle, the one nearest Wanted. */
		bool FreeNearest(const FParams& Params, EEdge Edge, double Wanted, double HalfAlong, double& OutAt);
		/** Puts one seal on the first of its edges (nearest first) that has room; false where none has. */
		bool PlaceOne(const FParams& Params, const EEdge* Edges, int EdgeCount, double Along[4], FSpot& Out);
		void Claim(const FParams& Params, const FSpot& Spot);

		std::vector<FBox> Obstacles;
		std::vector<std::pair<double, double>> Blocked;
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

	/** The slow pulse of a red seal not yet opened, 0 to 1 and back over Period seconds. */
	double Pulse(double Now, double Period = 2.4);
}
