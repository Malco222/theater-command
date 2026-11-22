// The news seals' cloud test (release 2, campaign map item 5): Campaign/EdgeSeals, the game's own code, with no engine.
// Run by Tools/edge_seals/run.sh.
// (0010) Also the under-frame watch: a seal's box held against the frame's pieces, both in the window's measure, meets
// none when the keep-off boxes are read in that measure, and meets the top bar, as Mac run 3's "+N" did, when they are
// read in the desktop's.

#include "Campaign/EdgeSeals.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <limits>
#include <new>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
	int Failures = 0;
	int Checks = 0;
	// (counts every allocation, to show that laying the seals again allocates nothing)
	long Allocations = 0;

	void Check(bool bOk, const char* What, int Line)
	{
		++Checks;
		if (!bOk)
		{
			++Failures;
			std::printf("FAILED (line %d): %s\n", Line, What);
		}
	}
}

#define CHECK(Condition) Check((Condition), #Condition, __LINE__)

// (not inlined: where g++ inlines the pair it sees std::free take what operator new gave, and warns of a mismatch)
[[gnu::noinline]] void* operator new(std::size_t Size)
{
	++Allocations;
	if (void* Block = std::malloc(Size == 0 ? 1 : Size))
	{
		return Block;
	}
	throw std::bad_alloc();
}

[[gnu::noinline]] void operator delete(void* Block) noexcept
{
	std::free(Block);
}

[[gnu::noinline]] void operator delete(void* Block, std::size_t) noexcept
{
	std::free(Block);
}

using namespace EdgeSeals;

namespace
{
	FBox SealBox(const FParams& Params, const FSpot& Spot)
	{
		return FBox{ Spot.Centre.X - Params.SealW * 0.5, Spot.Centre.Y - Params.SealH * 0.5, Spot.Centre.X + Params.SealW * 0.5, Spot.Centre.Y + Params.SealH * 0.5 };
	}

	/** Every seal shown keeps inside the screen's margin, clear of every piece of the frame and of every other seal. */
	bool LaidClear(const FParams& Params, const std::vector<FBox>& Reserved, const std::vector<FSpot>& Spots, std::string& Why)
	{
		const double Eps = 1e-6;
		for (size_t Index = 0; Index < Spots.size(); ++Index)
		{
			const FSpot& Spot = Spots[Index];
			if (!Spot.bShown)
			{
				continue;
			}
			const FBox Box = SealBox(Params, Spot);
			if (Box.MinX < Params.Margin - Eps || Box.MinY < Params.Margin - Eps || Box.MaxX > Params.ScreenW - Params.Margin + Eps || Box.MaxY > Params.ScreenH - Params.Margin + Eps)
			{
				Why = "a seal off the screen's margin";
				return false;
			}
			for (const FBox& Piece : Reserved)
			{
				if (!Piece.IsEmpty() && Box.Overlaps(Piece.Grown(Params.Keep - Eps)))
				{
					Why = "a seal on a piece of the frame";
					return false;
				}
			}
			for (size_t Other = Index + 1; Other < Spots.size(); ++Other)
			{
				if (Spots[Other].bShown && Box.Overlaps(SealBox(Params, Spots[Other]).Grown(Params.Gap - Eps)))
				{
					Why = "two seals on each other";
					return false;
				}
			}
			// (it slides in from beyond its own edge)
			const bool bOff = Spot.From.X < 0.0 || Spot.From.Y < 0.0 || Spot.From.X > Params.ScreenW || Spot.From.Y > Params.ScreenH;
			if (!bOff)
			{
				Why = "a seal that slides in from on the screen";
				return false;
			}
		}
		return true;
	}

	/** The frame of the folded screen, as a 1600 by 900 frame lays it out: the date strip, the right column, the minimap, the tray. */
	std::vector<FBox> FoldedFrame(bool bTray)
	{
		std::vector<FBox> Frame = {
			FBox{ 24.0, 24.0, 330.0, 62.0 }, // the date strip
			FBox{ 1600.0 - 24.0 - 420.0, 24.0, 1576.0, 76.0 }, // the doors
			FBox{ 1600.0 - 24.0 - 320.0, 86.0, 1576.0, 520.0 }, // END THE MONTH, the red and amber rows, THIS MONTH
			FBox{ 1600.0 - 24.0 - 300.0, 900.0 - 24.0 - 240.0, 1576.0, 876.0 }, // the minimap
		};
		if (bTray)
		{
			Frame.push_back(FBox{ 24.0, 900.0 - 24.0 - 260.0, 900.0, 876.0 });
		}
		return Frame;
	}

	FParams Folded()
	{
		FParams Params;
		Params.ScreenW = 1600.0;
		Params.ScreenH = 900.0;
		return Params;
	}
}

static void TestItems()
{
	std::vector<FItem> Items;
	ListItems(FCardCounts{ 2, true, 3, 2, 1, 2 }, Items);
	const ESection Want[] = { ESection::Band, ESection::Band, ESection::Lead, ESection::Row, ESection::Row, ESection::Row, ESection::Home, ESection::Home, ESection::Staff, ESection::Elsewhere, ESection::Elsewhere };
	CHECK(Items.size() == sizeof(Want) / sizeof(Want[0]));
	bool bOrder = Items.size() == sizeof(Want) / sizeof(Want[0]);
	for (size_t Index = 0; bOrder && Index < Items.size(); ++Index)
	{
		bOrder = Items[Index].Section == Want[Index];
	}
	CHECK(bOrder);
	CHECK(Items[1].Index == 1 && Items[2].Index == 0 && Items[5].Index == 2 && Items[10].Index == 1);
	ListItems(FCardCounts{ 0, false, 0, 0, 0, 0 }, Items);
	CHECK(Items.empty());
	ListItems(FCardCounts{ 0, false, 2, 0, 0, 0 }, Items);
	CHECK(Items.size() == 2 && Items[0].Section == ESection::Row);

	CHECK(Fold(5, 9).Shown == 5 && Fold(5, 9).More == 0);
	CHECK(Fold(9, 9).Shown == 9 && Fold(9, 9).More == 0);
	CHECK(Fold(10, 9).Shown == 8 && Fold(10, 9).More == 2);
	CHECK(Fold(23, 9).Shown == 8 && Fold(23, 9).More == 15);
	CHECK(Fold(0, 9).Shown == 0 && Fold(0, 9).More == 0);
	CHECK(Fold(3, 1).Shown == 0 && Fold(3, 1).More == 3);
	CHECK(Fold(3, 0).Shown == 0 && Fold(3, 0).More == 3);
}

static void TestWords()
{
	CHECK(ClassOf(ESection::Band, "WARNING") == 1);
	CHECK(ClassOf(ESection::Band, "WAR") == 2 && ClassOf(ESection::Band, "FREED") == 2 && ClassOf(ESection::Band, "NEWS") == 2);
	CHECK(ClassOf(ESection::Lead, "LAST MONTH") == 2);
	CHECK(ClassOf(ESection::Lead, "WON") == 3);
	CHECK(ClassOf(ESection::Row, "WARNING") == 1);
	CHECK(ClassOf(ESection::Row, "FALLEN") == 2);
	CHECK(ClassOf(ESection::Row, "LOST") == 3 && ClassOf(ESection::Row, "") == 3);
	CHECK(ClassOf(ESection::Home, "") == 6 && ClassOf(ESection::Staff, "") == 6);
	CHECK(ClassOf(ESection::Elsewhere, "") == 7 && ClassOf(ESection::More, "") == 8);

	CHECK(GlyphOf(ESection::Band, "WARNING", false) == EGlyph::Warning);
	CHECK(GlyphOf(ESection::Band, "WAR", false) == EGlyph::War);
	CHECK(GlyphOf(ESection::Band, "FALLEN", false) == EGlyph::Fallen);
	CHECK(GlyphOf(ESection::Row, "FREED", false) == EGlyph::Freed);
	CHECK(GlyphOf(ESection::Band, "NEWS", false) == EGlyph::News);
	CHECK(GlyphOf(ESection::Row, "WON", true) == EGlyph::Sign);
	CHECK(GlyphOf(ESection::Lead, "LAST MONTH", false) == EGlyph::Warning);
	CHECK(GlyphOf(ESection::Elsewhere, "", false) == EGlyph::Globe);
	CHECK(GlyphOf(ESection::Staff, "", false) == EGlyph::News);
	CHECK(GlyphOf(ESection::More, "", true) == EGlyph::More);

	CHECK(LeadSign("troops") == "inf" && LeadSign("town") == "inf" && LeadSign("rail-yard") == "inf");
	CHECK(LeadSign("fleet") == "hull" && LeadSign("harbour") == "hull" && LeadSign("airfield") == "ftr");
	CHECK(LeadSign("factories").empty() && LeadSign("").empty());

	CHECK(RowLine("Monterrey", "WON", "from", "Mexico", "The city is taken.") == "Monterrey WON from Mexico");
	CHECK(RowLine("Seattle", "HELD", "", "", "") == "Seattle HELD");
	CHECK(RowLine("Seattle", "HELD", "", "", "The siege of Seattle is raised.") == "Seattle HELD: The siege of Seattle is raised.");
	CHECK(RowLine("", "SUNK", "", "", "x") == "SUNK: x");
	CHECK(RowLine("North Atlantic", "", "", "", "Three convoys came through.") == "Three convoys came through.");
	CHECK(RowLine("North Atlantic", "", "", "", "") == "North Atlantic");
	// (a band's item that found no band is a row with its band's word and no other side: its news is its sentence)
	CHECK(RowLine("Germany", "WAR", "", "", "Germany declares war on Poland.") == "Germany WAR: Germany declares war on Poland.");
	CHECK(EightWords(RowLine("Germany", "WAR", "", "", "Germany declares war on Poland.")) == "Germany WAR: Germany declares war on Poland.");
	CHECK(EightWords(RowLine("Coast watch", "WARNING", "", "", "Italy masses troops on the Yugoslav border.")) == "Coast watch WARNING: Italy masses troops");
	// (a sea or air row never names the other side with a join word: SUNK, HIT, BLOCKED, MISSED)
	CHECK(EightWords(RowLine("North Sea", "SUNK", "", "", "A U-boat sinks two transports.")) == "North Sea SUNK: A U-boat sinks two transports.");
	CHECK(EightWords(RowLine("Ruhr", "HIT", "", "", "Our bombers hit the Ruhr's works, and three are lost.")) == "Ruhr HIT: Our bombers hit the Ruhr's works");
	// (a join word with no name is no join)
	CHECK(RowLine("Brest", "LOST", "to", "", "The port falls.") == "Brest LOST: The port falls.");

	CHECK(EightWords("Germany declares war on Poland") == "Germany declares war on Poland");
	CHECK(EightWords("  Germany   declares war  ") == "Germany   declares war");
	CHECK(EightWords("") .empty() && EightWords("   ").empty());
	CHECK(EightWords("one two three four five six seven eight") == "one two three four five six seven eight");
	CHECK(EightWords("one two three four five six seven eight nine ten") == "one two three four five six seven eight");
	CHECK(EightWords("Paris falls, and the French government leaves for Bordeaux tonight") == "Paris falls, and the French government leaves");
	CHECK(EightWords("The siege of Leningrad tightens, and the last road across the lake is shelled") == "The siege of Leningrad tightens");
	CHECK(EightWords("one two three four five six seven, eight nine") == "one two three four five six seven");
	CHECK(EightWords("one, two three four five six seven eight nine") == "one, two three four five six seven eight");
	CHECK(EightWords("one two three four five six seven \xE2\x80\x94 eight nine") == "one two three four five six seven");
	CHECK(EightWords("Wroc\xC5\x82" "aw and \xC5\x81\xC3\xB3" "d\xC5\xBA hold against the attack from the west today") == "Wroc\xC5\x82" "aw and \xC5\x81\xC3\xB3" "d\xC5\xBA hold against the attack");
	CHECK(EightWords("a b c", 2) == "a b" && EightWords("a b c", 0) == "a");
}

static void TestLanes()
{
	const FParams Params = Folded();
	const FLane Top = LaneOf(Params, EEdge::Top), Right = LaneOf(Params, EEdge::Right), Bottom = LaneOf(Params, EEdge::Bottom), Left = LaneOf(Params, EEdge::Left);
	CHECK(Top.bAlongX && Bottom.bAlongX && !Left.bAlongX && !Right.bAlongX);
	CHECK(std::fabs(Top.Fixed - (14.0 + 22.0)) < 1e-9 && std::fabs(Bottom.Fixed - (900.0 - 14.0 - 22.0)) < 1e-9);
	CHECK(std::fabs(Left.Fixed - (14.0 + 17.0)) < 1e-9 && std::fabs(Right.Fixed - (1600.0 - 14.0 - 17.0)) < 1e-9);
	double Along = 0.0;
	CHECK(DistanceToLane(Top, FPoint{ 800.0, 36.0 }, Along) < 1e-9 && std::fabs(Along - 800.0) < 1e-9);
	CHECK(std::fabs(DistanceToLane(Top, FPoint{ -100.0, 36.0 }, Along) - (100.0 + 31.0)) < 1e-9 && std::fabs(Along - 31.0) < 1e-9);
	const FPoint From = FromOffScreen(Params, EEdge::Right, FPoint{ Right.Fixed, 400.0 });
	CHECK(From.X > Params.ScreenW && std::fabs(From.Y - 400.0) < 1e-9);
	CHECK(FromOffScreen(Params, EEdge::Top, FPoint{ 500.0, Top.Fixed }).Y < 0.0);
	CHECK(FromOffScreen(Params, EEdge::Bottom, FPoint{ 500.0, Bottom.Fixed }).Y > Params.ScreenH);
	CHECK(FromOffScreen(Params, EEdge::Left, FPoint{ Left.Fixed, 300.0 }).X < 0.0);
}

static void TestLayout()
{
	FLayout Layout;
	std::vector<FSpot> Spots;
	std::string Why;
	const FParams Params = Folded();

	// 1. Three seals with no place, nothing on the screen: a neat stack in the middle of the top edge, side by side.
	{
		const std::vector<FWant> Wants(3);
		const std::vector<FBox> None;
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots.size() == 3 && Spots[0].bShown && Spots[1].bShown && Spots[2].bShown);
		CHECK(Spots[0].Edge == EEdge::Top && Spots[2].Edge == EEdge::Top);
		CHECK(std::fabs(Spots[1].Centre.X - 800.0) < 1e-6);
		CHECK(std::fabs((Spots[1].Centre.X - Spots[0].Centre.X) - (Params.SealW + Params.Gap)) < 1e-6);
		CHECK(std::fabs((Spots[2].Centre.X - Spots[1].Centre.X) - (Params.SealW + Params.Gap)) < 1e-6);
		CHECK(LaidClear(Params, None, Spots, Why));
	}

	// 2. A place off the screen to the right, half way down: the right edge, level with it.
	{
		std::vector<FWant> Wants(1);
		Wants[0] = FWant{ true, FPoint{ 2400.0, 450.0 } };
		const std::vector<FBox> None;
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].bShown && Spots[0].Edge == EEdge::Right && std::fabs(Spots[0].Centre.Y - 450.0) < 1e-6);
		// A place on the screen near its foot: the bottom edge, under it.
		Wants[0] = FWant{ true, FPoint{ 700.0, 820.0 } };
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Bottom && std::fabs(Spots[0].Centre.X - 700.0) < 1e-6);
		// Off the screen above and to the left, further above than to the left: the top edge, at its left end.
		Wants[0] = FWant{ true, FPoint{ -50.0, -400.0 } };
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top && std::fabs(Spots[0].Centre.X - (Params.Margin + Params.SealW * 0.5)) < 1e-6);
		CHECK(LaidClear(Params, None, Spots, Why));
	}

	// 3. The folded frame: a place to the right half way down cannot have the right edge under the column; it goes as
	// near as it can, and nothing lies on the frame.
	{
		const std::vector<FBox> Frame = FoldedFrame(false);
		std::vector<FWant> Wants(1);
		Wants[0] = FWant{ true, FPoint{ 1700.0, 300.0 } };
		Layout.Lay(Params, Wants, Frame, Spots);
		CHECK(Spots[0].bShown);
		CHECK(LaidClear(Params, Frame, Spots, Why));
		// (the right edge between the column's foot and the minimap's head has room: it is the nearest free point)
		CHECK(Spots[0].Edge == EEdge::Right && Spots[0].Centre.Y > 520.0 && Spots[0].Centre.Y < 636.0);
		// A place at the top right: under the doors, so the top edge to the left of them, or the right edge below.
		Wants[0] = FWant{ true, FPoint{ 1500.0, 10.0 } };
		Layout.Lay(Params, Wants, Frame, Spots);
		CHECK(Spots[0].bShown && LaidClear(Params, Frame, Spots, Why));
		CHECK(Spots[0].Edge == EEdge::Top && Spots[0].Centre.X < 1600.0 - 24.0 - 420.0);
		// With the tray up a place at the foot on the left goes to the bottom edge clear of the tray, or the left edge above it.
		const std::vector<FBox> WithTray = FoldedFrame(true);
		Wants[0] = FWant{ true, FPoint{ 200.0, 950.0 } };
		Layout.Lay(Params, Wants, WithTray, Spots);
		CHECK(Spots[0].bShown && LaidClear(Params, WithTray, Spots, Why));
		CHECK((Spots[0].Edge == EEdge::Bottom && Spots[0].Centre.X > 900.0) || (Spots[0].Edge == EEdge::Left && Spots[0].Centre.Y < 616.0));
	}

	// 4. Two places at the same point: side by side on the same edge, never on each other; and card order wins the spot.
	{
		std::vector<FWant> Wants(2);
		Wants[0] = FWant{ true, FPoint{ 600.0, 1200.0 } };
		Wants[1] = FWant{ true, FPoint{ 600.0, 1200.0 } };
		const std::vector<FBox> None;
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Bottom && Spots[1].Edge == EEdge::Bottom);
		CHECK(std::fabs(Spots[0].Centre.X - 600.0) < 1e-6);
		CHECK(std::fabs(std::fabs(Spots[1].Centre.X - 600.0) - (Params.SealW + Params.Gap)) < 1e-6);
		CHECK(LaidClear(Params, None, Spots, Why));
	}

	// 5. The corner: a seal on the top edge at its left end and one on the left edge at its top never meet.
	{
		std::vector<FWant> Wants(2);
		Wants[0] = FWant{ true, FPoint{ 40.0, -300.0 } };
		Wants[1] = FWant{ true, FPoint{ -300.0, 40.0 } };
		const std::vector<FBox> None;
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top && Spots[1].Edge == EEdge::Left);
		CHECK(std::fabs(Spots[0].Centre.X - 40.0) < 1e-6 && Spots[1].Centre.Y >= Params.Margin + Params.SealH + Params.Gap + Params.SealH * 0.5 - 1e-6);
		CHECK(LaidClear(Params, None, Spots, Why));
		// (beyond a corner, the edge the place lies more squarely in front of: far to the right and a little above, the right)
		Wants[0] = FWant{ true, FPoint{ 3000.0, -50.0 } };
		Layout.Lay(Params, std::vector<FWant>(1, Wants[0]), None, Spots);
		CHECK(Spots[0].Edge == EEdge::Right && std::fabs(Spots[0].Centre.Y - (Params.Margin + Params.SealH * 0.5)) < 1e-6);
		Wants[0] = FWant{ true, FPoint{ -10.0, -300.0 } };
		Layout.Lay(Params, std::vector<FWant>(1, Wants[0]), None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top);
		// (and exactly on the diagonal, the top)
		Wants[0] = FWant{ true, FPoint{ 31.0 - 100.0, 36.0 - 100.0 } };
		Layout.Lay(Params, std::vector<FWant>(1, Wants[0]), None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top);
	}

	// 5b. A seal keeps the edge it lies on while no other is nearer by more than Stick: no flicker at a corner.
	{
		std::vector<FWant> Wants(1);
		const std::vector<FBox> None;
		Wants[0] = FWant{ true, FPoint{ 1700.0, -80.0 }, -1 }; // (beyond the top right corner, a little nearer the right)
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Right);
		Wants[0] = FWant{ true, FPoint{ 1690.0, -110.0 }, static_cast<int>(EEdge::Right) }; // (now a little nearer the top)
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Right);
		Wants[0].Kept = -1;
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top);
		Wants[0] = FWant{ true, FPoint{ 1000.0, -400.0 }, static_cast<int>(EEdge::Right) }; // (far nearer the top: it moves)
		Layout.Lay(Params, Wants, None, Spots);
		CHECK(Spots[0].Edge == EEdge::Top);
	}

	// 6. Nine seals with no place under the folded frame: one stack, clear of the date strip and the doors.
	{
		const std::vector<FBox> Frame = FoldedFrame(false);
		const std::vector<FWant> Wants(9);
		Layout.Lay(Params, Wants, Frame, Spots);
		bool bAll = true;
		for (size_t Index = 0; Index < Spots.size(); ++Index)
		{
			bAll = bAll && Spots[Index].bShown && Spots[Index].Edge == EEdge::Top;
			if (Index > 0)
			{
				bAll = bAll && std::fabs(Spots[Index].Centre.X - Spots[Index - 1].Centre.X - (Params.SealW + Params.Gap)) < 1e-6;
			}
		}
		CHECK(bAll);
		CHECK(LaidClear(Params, Frame, Spots, Why));
		// A screen too narrow for one stack: one by one, still clear.
		FParams Narrow = Params;
		Narrow.ScreenW = 420.0;
		const std::vector<FBox> Strip = { FBox{ 24.0, 24.0, 200.0, 62.0 } };
		Layout.Lay(Narrow, Wants, Strip, Spots);
		CHECK(LaidClear(Narrow, Strip, Spots, Why));
		int Shown = 0;
		for (const FSpot& Spot : Spots)
		{
			Shown += Spot.bShown ? 1 : 0;
		}
		CHECK(Shown == 9);
	}

	// 7. No room at all: nothing shown, nothing broken.
	{
		FParams Tiny = Params;
		Tiny.ScreenW = 40.0;
		Tiny.ScreenH = 40.0;
		const std::vector<FWant> Wants(2);
		const std::vector<FBox> None;
		Layout.Lay(Tiny, Wants, None, Spots);
		CHECK(Spots.size() == 2 && !Spots[0].bShown && !Spots[1].bShown);
	}

	// 8. Random frames and places, many times over: every seal shown is clear, and nearly all are shown.
	{
		std::mt19937 Dice(1939);
		std::uniform_real_distribution<double> Unit(0.0, 1.0);
		int Laid = 0, Missing = 0;
		bool bClear = true;
		for (int Round = 0; Round < 4000 && bClear; ++Round)
		{
			FParams Random = Params;
			Random.ScreenW = 1000.0 + 1400.0 * Unit(Dice);
			Random.ScreenH = 700.0 + 700.0 * Unit(Dice);
			std::vector<FBox> Frame;
			const int Pieces = static_cast<int>(Unit(Dice) * 7.0);
			for (int Piece = 0; Piece < Pieces; ++Piece)
			{
				// (pieces of the frame stand against an edge or a corner, as the frame's do)
				const double W = 60.0 + 380.0 * Unit(Dice), H = 30.0 + 360.0 * Unit(Dice);
				const bool bRight = Unit(Dice) < 0.5, bLow = Unit(Dice) < 0.5;
				const double X = bRight ? Random.ScreenW - 24.0 - W : 24.0 + (Unit(Dice) < 0.3 ? Random.ScreenW * 0.4 * Unit(Dice) : 0.0);
				const double Y = bLow ? Random.ScreenH - 24.0 - H : 24.0;
				Frame.push_back(FBox{ X, Y, X + W, Y + H });
			}
			std::vector<FWant> Wants(1 + static_cast<size_t>(Unit(Dice) * 9.0));
			for (FWant& Want : Wants)
			{
				Want.bPlace = Unit(Dice) < 0.7;
				Want.Kept = Unit(Dice) < 0.5 ? static_cast<int>(Unit(Dice) * 4.0) : -1;
				Want.At = FPoint{ (Unit(Dice) * 3.0 - 1.0) * Random.ScreenW, (Unit(Dice) * 3.0 - 1.0) * Random.ScreenH };
			}
			Layout.Lay(Random, Wants, Frame, Spots);
			bClear = LaidClear(Random, Frame, Spots, Why);
			for (const FSpot& Spot : Spots)
			{
				(Spot.bShown ? Laid : Missing) += 1;
			}
		}
		if (!bClear)
		{
			std::printf("  (random frames: %s)\n", Why.c_str());
		}
		CHECK(bClear);
		CHECK(Missing * 100 < Laid); // (fewer than one in a hundred finds no room, and then only on a crowded small screen)
	}

	// 9. Laid again with the same sizes, as every frame lays them: the same spots, and not one allocation.
	{
		const std::vector<FBox> Frame = FoldedFrame(true);
		std::vector<FWant> Wants(9);
		for (size_t Index = 0; Index < Wants.size(); ++Index)
		{
			Wants[Index] = FWant{ Index % 3 != 0, FPoint{ 200.0 * static_cast<double>(Index) - 300.0, Index % 2 ? -200.0 : 1100.0 } };
		}
		Layout.Lay(Params, Wants, Frame, Spots);
		const std::vector<FSpot> First = Spots;
		const long Before = Allocations;
		for (int Frame60 = 0; Frame60 < 60; ++Frame60)
		{
			Wants[4].At.X += 3.0; // (the camera moves a little each frame)
			Layout.Lay(Params, Wants, Frame, Spots);
		}
		CHECK(Allocations == Before);
		Wants[4].At.X -= 180.0;
		Layout.Lay(Params, Wants, Frame, Spots);
		bool bSame = Spots.size() == First.size();
		for (size_t Index = 0; bSame && Index < Spots.size(); ++Index)
		{
			bSame = Spots[Index].bShown == First[Index].bShown && Spots[Index].Edge == First[Index].Edge && std::fabs(Spots[Index].Centre.X - First[Index].Centre.X) < 1e-9 && std::fabs(Spots[Index].Centre.Y - First[Index].Centre.Y) < 1e-9;
		}
		CHECK(bSame);
		CHECK(LaidClear(Params, Frame, Spots, Why));
	}
}

/** The frame of the folded screen in a smaller window, its pieces where the 1600 by 900 frame puts them from each edge. */
static std::vector<FBox> SmallFrame(double W, double H)
{
	return {
		FBox{ 24.0, 24.0, 330.0, 62.0 },
		FBox{ W - 24.0 - 420.0, 24.0, W - 24.0, 76.0 },
		FBox{ W - 24.0 - 320.0, 86.0, W - 24.0, 520.0 },
		FBox{ W - 24.0 - 300.0, H - 24.0 - 240.0, W - 24.0, H - 24.0 },
		FBox{ 24.0, H - 24.0 - 260.0, 900.0, H - 24.0 }, // (the tray)
	};
}

static void TestRanked()
{
	FLayout Layout;
	std::vector<FSpot> Spots;
	std::string Why;
	const FParams Params = Folded();
	const std::vector<FBox> None;

	// 1. Room for all: every item stands, in card order, and there is no "+N".
	{
		std::vector<FWant> Wants(5);
		for (size_t Index = 0; Index < Wants.size(); ++Index)
		{
			Wants[Index] = FWant{ Index < 3, FPoint{ 300.0 + 200.0 * static_cast<double>(Index), 1200.0 } };
		}
		const FFold Fold = Layout.LayRanked(Params, Wants, 5, 9, None, Spots);
		CHECK(Fold.Shown == 5 && Fold.More == 0 && Spots.size() == 5);
		CHECK(Spots[0].bShown && Spots[4].bShown && Spots[0].Edge == EEdge::Bottom && Spots[4].Edge == EEdge::Top);
		CHECK(LaidClear(Params, None, Spots, Why));
	}

	// 2. More items than may stand: eight, and "+N" for the rest, in the stack at the top.
	{
		const std::vector<FWant> Wants(9);
		const FFold Fold = Layout.LayRanked(Params, Wants, 14, 9, None, Spots);
		CHECK(Fold.Shown == 8 && Fold.More == 6 && Spots.size() == 9);
		CHECK(Spots[8].bShown && Spots[8].Edge == EEdge::Top && Spots[8].Centre.X > Spots[7].Centre.X);
		// (fewer wants than items: the items not given fold too)
		const FFold Short = Layout.LayRanked(Params, std::vector<FWant>(3), 7, 9, None, Spots);
		CHECK(Short.Shown == 3 && Short.More == 4 && Spots.size() == 4 && Spots[3].bShown);
		const FFold Empty = Layout.LayRanked(Params, std::vector<FWant>(), 0, 9, None, Spots);
		CHECK(Empty.Shown == 0 && Empty.More == 0 && Spots.empty());
	}

	// 3. No room at all: nothing stands, and every item is counted in a "+N" that has no room either.
	{
		FParams Tiny = Params;
		Tiny.ScreenW = 40.0;
		Tiny.ScreenH = 40.0;
		const FFold Fold = Layout.LayRanked(Tiny, std::vector<FWant>(4), 4, 9, None, Spots);
		CHECK(Fold.Shown == 0 && Fold.More == 4 && Spots.size() == 1 && !Spots[0].bShown);
		// Room for one seal only: the "+N" stands for them all, so nothing is lost without trace.
		FParams One = Params;
		One.ScreenW = 2.0 * Params.Margin + Params.SealW + 4.0;
		One.ScreenH = 2.0 * Params.Margin + Params.SealH + 4.0;
		std::vector<FWant> Wants(3);
		Wants[0] = FWant{ true, FPoint{ 10.0, 10.0 } };
		const FFold Single = Layout.LayRanked(One, Wants, 3, 9, None, Spots);
		CHECK(Single.Shown == 0 && Single.More == 3 && Spots.size() == 1 && Spots[0].bShown);
	}

	// 4. The reviewer's case: a 1024 by 768 window (frame units 0.9 of a pixel) with the tray up, six items with a place
	// (the bands, the lead and the rows: classes 1 to 3) before three with none (home, the staff, elsewhere: 6 and 7).
	// Where the edge is full the last items fold into "+N": an item with a place never gives way to one ranked below
	// it, every item that does not stand is counted, and what stands is clear of the frame and of each other.
	{
		const double Sizes[3][2] = { { 1024.0 * 0.9, 768.0 * 0.9 }, { 1280.0 * 0.9, 720.0 * 0.9 }, { 1600.0, 900.0 } };
		std::mt19937 Dice(5);
		std::uniform_real_distribution<double> Unit(0.0, 1.0);
		int Folded[3] = { 0, 0, 0 }, Inverted = 0, Lost = 0;
		bool bClear = true;
		for (int Sized = 0; Sized < 3; ++Sized)
		{
			const double* Size = Sizes[Sized];
			FParams Window = Params;
			Window.ScreenW = Size[0];
			Window.ScreenH = Size[1];
			const std::vector<FBox> Frame = SmallFrame(Window.ScreenW, Window.ScreenH);
			for (int Round = 0; Round < 20000; ++Round)
			{
				std::vector<FWant> Wants(9);
				for (size_t Index = 0; Index < 6; ++Index)
				{
					Wants[Index] = FWant{ true, FPoint{ (Unit(Dice) * 1.2 - 0.1) * Window.ScreenW, (Unit(Dice) * 1.2 - 0.1) * Window.ScreenH }, -1 };
				}
				const FFold Fold = Layout.LayRanked(Window, Wants, 9, 9, Frame, Spots);
				Folded[Sized] += Fold.More > 0 ? 1 : 0;
				// Which items stand: the first Shown, each at its spot; the "+N" after them.
				bool bStands[9] = {};
				int Standing = 0;
				for (int Index = 0; Index < Fold.Shown && static_cast<size_t>(Index) < Spots.size(); ++Index)
				{
					bStands[Index] = Spots[static_cast<size_t>(Index)].bShown;
					Standing += bStands[Index] ? 1 : 0;
				}
				const bool bRestStands = Fold.More > 0 && Spots.size() == static_cast<size_t>(Fold.Shown + 1) && Spots.back().bShown;
				// (nothing without trace: every item that does not stand is counted in a "+N" that stands)
				Lost += Standing + Fold.More == 9 && (Fold.More == 0 || bRestStands) ? 0 : 1;
				// (no item with a place, the bands, the lead and the rows, gives way while home, staff or elsewhere stands)
				for (int Placed = 0; Placed < 6; ++Placed)
				{
					for (int Below = 6; Below < 9; ++Below)
					{
						Inverted += !bStands[Placed] && bStands[Below] ? 1 : 0;
					}
				}
				if (bClear && !LaidClear(Window, Frame, Spots, Why))
				{
					bClear = false;
					std::printf("  (ranked, window %.0f by %.0f: %s)\n", Window.ScreenW, Window.ScreenH, Why.c_str());
				}
			}
		}
		CHECK(bClear);
		CHECK(Lost == 0);
		CHECK(Inverted == 0);
		// (the small window, its bottom edge all tray and its right all column, has room for about eight: there the case
		// is exercised, about one card in four; the full screen never folds)
		CHECK(Folded[0] > 1000 && Folded[2] == 0);
	}

	// 5. Random frames, places and counts: what stands is always the card's first items, then "+N" for all the rest.
	{
		std::mt19937 Dice(1940);
		std::uniform_real_distribution<double> Unit(0.0, 1.0);
		bool bRanked = true, bClear = true;
		for (int Round = 0; Round < 4000 && bRanked && bClear; ++Round)
		{
			FParams Random = Params;
			Random.ScreenW = 500.0 + 1900.0 * Unit(Dice);
			Random.ScreenH = 400.0 + 1000.0 * Unit(Dice);
			std::vector<FBox> Frame;
			const int Pieces = static_cast<int>(Unit(Dice) * 8.0);
			for (int Piece = 0; Piece < Pieces; ++Piece)
			{
				const double W = 60.0 + 420.0 * Unit(Dice), H = 30.0 + 400.0 * Unit(Dice);
				const double X = Unit(Dice) < 0.5 ? Random.ScreenW - 24.0 - W : 24.0 + Random.ScreenW * 0.3 * Unit(Dice);
				const double Y = Unit(Dice) < 0.5 ? Random.ScreenH - 24.0 - H : 24.0;
				Frame.push_back(FBox{ X, Y, X + W, Y + H });
			}
			const int Total = static_cast<int>(Unit(Dice) * 16.0);
			std::vector<FWant> Wants(static_cast<size_t>(std::min(Total, 9)));
			for (FWant& Want : Wants)
			{
				Want.bPlace = Unit(Dice) < 0.7;
				Want.At = FPoint{ (Unit(Dice) * 3.0 - 1.0) * Random.ScreenW, (Unit(Dice) * 3.0 - 1.0) * Random.ScreenH };
			}
			const FFold Fold = Layout.LayRanked(Random, Wants, Total, 9, Frame, Spots);
			const bool bRest = Fold.More > 0;
			bRanked = Fold.Shown >= 0 && Fold.Shown + Fold.More == Total && Fold.Shown + (bRest ? 1 : 0) <= 9 && (Total > 9 ? bRest : true);
			bRanked = bRanked && Spots.size() == static_cast<size_t>(Fold.Shown + (bRest ? 1 : 0));
			for (size_t Index = 0; bRanked && Index < Spots.size(); ++Index)
			{
				// (every seal laid stands, save a "+N" that has no room even alone)
				bRanked = Spots[Index].bShown || (Fold.Shown == 0 && bRest);
			}
			bClear = LaidClear(Random, Frame, Spots, Why);
		}
		if (!bClear)
		{
			std::printf("  (ranked random frames: %s)\n", Why.c_str());
		}
		CHECK(bRanked);
		CHECK(bClear);
	}

	// 6. Laid again every frame in a window where they fold: the same answer, and not one allocation.
	{
		FParams Window = Params;
		Window.ScreenW = 1024.0 * 0.9;
		Window.ScreenH = 768.0 * 0.9;
		const std::vector<FBox> Frame = SmallFrame(Window.ScreenW, Window.ScreenH);
		std::vector<FWant> Wants(9);
		for (size_t Index = 0; Index < 6; ++Index)
		{
			// (all six places off the screen's foot, under the tray: the bottom and left edges crowd)
			Wants[Index] = FWant{ true, FPoint{ 40.0 + 30.0 * static_cast<double>(Index), Window.ScreenH + 40.0 } };
		}
		const FFold First = Layout.LayRanked(Window, Wants, 9, 9, Frame, Spots);
		const long Before = Allocations;
		FFold Again = First;
		for (int Frame60 = 0; Frame60 < 60; ++Frame60)
		{
			Again = Layout.LayRanked(Window, Wants, 9, 9, Frame, Spots);
		}
		CHECK(Allocations == Before);
		CHECK(Again.Shown == First.Shown && Again.More == First.More && LaidClear(Window, Frame, Spots, Why));
	}
}

static void TestMotion()
{
	CHECK(SlideShare(0.0, 0) == 0.0);
	CHECK(SlideShare(0.38, 0) == 1.0 && SlideShare(5.0, 8) == 1.0);
	CHECK(SlideShare(0.23, 2) == 0.0 && SlideShare(0.25, 2) > 0.0);
	double Last = -1.0;
	bool bRising = true;
	for (int Step = 0; Step <= 100; ++Step)
	{
		const double Share = SlideShare(Step * 0.01, 1);
		bRising = bRising && Share >= Last && Share >= 0.0 && Share <= 1.0;
		Last = Share;
	}
	CHECK(bRising);
	// (it slows as it arrives: more than half the way in the first third of the time)
	CHECK(SlideShare(0.38 / 3.0, 0) > 0.5);
	CHECK(std::fabs(Pulse(0.0)) < 1e-9 && std::fabs(Pulse(1.2) - 1.0) < 1e-9 && std::fabs(Pulse(2.4)) < 1e-9);
	bool bInside = true;
	for (int Step = 0; Step < 500; ++Step)
	{
		const double Value = Pulse(Step * 0.037);
		bInside = bInside && Value >= 0.0 && Value <= 1.0;
	}
	CHECK(bInside);
}

namespace
{
	/** A box in the layer's units as painted on a screen of PxPerUnit, the layer at the window's corner: the window's pixels. */
	FBox Painted(const FBox& Units, double PxPerUnit)
	{
		return FBox{ Units.MinX * PxPerUnit, Units.MinY * PxPerUnit, Units.MaxX * PxPerUnit, Units.MaxY * PxPerUnit };
	}

	/**
	 * The frame's pieces (in the window's pixels, as FrameRects keeps them) brought into the layer's units: by the
	 * layer's last paint (the window's measure, as 0010 reads them), or by its Tick geometry (the desktop's measure, as
	 * Mac run 3 read them), which takes the window's place on the desktop off every box.
	 */
	std::vector<FBox> KeptOff(const std::vector<FBox>& PiecesPx, double PxPerUnit, double WindowX, double WindowY, bool bDesktop)
	{
		std::vector<FBox> Out;
		const double OffX = bDesktop ? WindowX : 0.0, OffY = bDesktop ? WindowY : 0.0;
		for (const FBox& Piece : PiecesPx)
		{
			Out.push_back(FBox{ (Piece.MinX - OffX) / PxPerUnit, (Piece.MinY - OffY) / PxPerUnit, (Piece.MaxX - OffX) / PxPerUnit, (Piece.MaxY - OffY) / PxPerUnit });
		}
		return Out;
	}

	/** How many of the seals laid would the watch find painted under a piece of the frame. */
	int UnderCount(const FParams& Params, const std::vector<FSpot>& Spots, const std::vector<FBox>& PiecesPx, double PxPerUnit)
	{
		int Under = 0;
		for (const FSpot& Spot : Spots)
		{
			if (Spot.bShown && UnderPiece(Painted(SealBox(Params, Spot), PxPerUnit), PiecesPx) >= 0)
			{
				++Under;
			}
		}
		return Under;
	}
}

/**
 * (0010) The under-frame watch, and the fault it is there to catch: Mac run 3 laid the "+N" seal under the top bar,
 * the keep-off boxes having been read in the desktop's measure (the layer's Tick geometry) where they are kept in the
 * window's (its paint geometry).
 */
static void TestUnderFrame()
{
	// Meet is FSlateRect::DoRectanglesIntersect's test: touching meets, a hair apart does not.
	const FBox Ten{ 0.0, 0.0, 10.0, 10.0 };
	CHECK(Meet(Ten, FBox{ 5.0, 5.0, 15.0, 15.0 }) && Meet(Ten, FBox{ 10.0, 0.0, 20.0, 10.0 }) && Meet(Ten, FBox{ 0.0, 10.0, 10.0, 20.0 }));
	CHECK(!Meet(Ten, FBox{ 10.001, 0.0, 20.0, 10.0 }) && !Meet(Ten, FBox{ 0.0, -20.0, 10.0, -0.001 }));
	CHECK(Meet(Ten, FBox{ 2.0, 2.0, 3.0, 3.0 }) && Meet(FBox{ 2.0, 2.0, 3.0, 3.0 }, Ten));
	CHECK(UnderPiece(Ten, {}) == -1 && UnderPiece(Ten, { FBox{ 20.0, 20.0, 30.0, 30.0 }, FBox{ 9.0, 9.0, 12.0, 12.0 } }) == 1);

	// The watch looks once per laying and slot, and only at a seal on the screen that has slid all the way in.
	{
		FUnderWatch Watch;
		CHECK(!Watch.Due(0, 1, false, false) && !Watch.Due(0, 1, true, false) && !Watch.Due(0, 1, false, true));
		CHECK(Watch.Due(0, 1, true, true) && !Watch.Due(0, 1, true, true));
		CHECK(Watch.Due(1, 1, true, true) && Watch.Due(0, 2, true, true) && !Watch.Due(0, 2, true, true) && !Watch.Due(1, 1, true, true));
		CHECK(Watch.Due(0, 0, true, true) && Watch.Due(8, 0, true, true));
		CHECK(!Watch.Due(-1, 1, true, true) && !Watch.Due(FUnderWatch::Slots, 1, true, true));
		// (and again where, in the same laying, the slot comes to hold the "+N" in place of its item, or the item back: a
		// "+N" laid after the first look, as when the tray comes up, is looked at too, once)
		FUnderWatch Later;
		CHECK(Later.Due(3, 7, true, true, 0) && !Later.Due(3, 7, true, true, 0));
		CHECK(Later.Due(3, 7, true, true, 1) && !Later.Due(3, 7, true, true, 1) && Later.Due(3, 7, true, true, 0));
		CHECK(!Later.Due(3, 7, true, false, 1) && !Later.Due(4, 7, false, true, 1));
	}

	// Run 3's fault, as a model (not its frame, which is not known here): Brian's window, 1.84 px a unit, 1566 by 971
	// units, at 29 px in and 130 px down the desktop (centred, and under the Mac's menu bar), with a bar across the top
	// edge from 24 to 72 units high. Read right, the bar takes the whole top lane and the stack goes to the foot; read
	// in the desktop's measure, the bar rises 71 units and lets the lane free, the stack stands at the top's middle, and
	// is painted under the bar. The watch sees it; laid right, it sees nothing.
	{
		const double Px = 1.84;
		FParams Mac;
		Mac.ScreenW = 1566.0;
		Mac.ScreenH = 971.0;
		const std::vector<FBox> PiecesPx = { Painted(FBox{ 24.0, 24.0, 1542.0, 72.0 }, Px), Painted(FBox{ 1566.0 - 24.0 - 320.0, 86.0, 1542.0, 520.0 }, Px) };
		std::vector<FWant> Wants(5); // (no place: home, the staff, elsewhere, and the "+N")
		FLayout Layout;
		std::vector<FSpot> Right, Wrong;
		Layout.LayRanked(Mac, Wants, 12, 9, KeptOff(PiecesPx, Px, 29.0, 130.0, false), Right);
		Layout.LayRanked(Mac, Wants, 12, 9, KeptOff(PiecesPx, Px, 29.0, 130.0, true), Wrong);
		bool bRightAway = !Right.empty(), bWrongTop = !Wrong.empty();
		for (const FSpot& Spot : Right)
		{
			bRightAway = bRightAway && Spot.bShown && Spot.Edge != EEdge::Top;
		}
		for (const FSpot& Spot : Wrong)
		{
			bWrongTop = bWrongTop && Spot.bShown && Spot.Edge == EEdge::Top;
		}
		CHECK(bRightAway);
		CHECK(bWrongTop);
		CHECK(UnderCount(Mac, Right, PiecesPx, Px) == 0);
		CHECK(UnderCount(Mac, Wrong, PiecesPx, Px) == static_cast<int>(Wrong.size()));
		// (a window at the desktop's corner reads the same either way: the fault showed only in a window placed off it)
		std::vector<FSpot> Corner;
		Layout.LayRanked(Mac, Wants, 12, 9, KeptOff(PiecesPx, Px, 0.0, 0.0, true), Corner);
		bool bSame = Corner.size() == Right.size();
		for (size_t Index = 0; bSame && Index < Corner.size(); ++Index)
		{
			bSame = Corner[Index].Edge == Right[Index].Edge && Corner[Index].Centre.X == Right[Index].Centre.X && Corner[Index].Centre.Y == Right[Index].Centre.Y;
		}
		CHECK(bSame);
	}

	// Random frames, windows anywhere on the desktop and every scale from 0.7 to 2.2 px a unit: laid in the right
	// measure no seal is ever painted under a piece (they keep Keep clear, so they never even touch); in the desktop's
	// measure the watch finds them under it in many.
	{
		std::mt19937 Dice(2026);
		std::uniform_real_distribution<double> Unit(0.0, 1.0);
		FLayout Layout;
		std::vector<FSpot> Spots;
		int RightUnder = 0, WrongRounds = 0, Rounds = 0;
		for (int Round = 0; Round < 3000; ++Round)
		{
			const double Px = 0.7 + 1.5 * Unit(Dice);
			FParams Window;
			Window.ScreenW = 900.0 + 1000.0 * Unit(Dice);
			Window.ScreenH = 650.0 + 500.0 * Unit(Dice);
			std::vector<FBox> PiecesPx;
			const int Pieces = 1 + static_cast<int>(Unit(Dice) * 6.0);
			for (int Piece = 0; Piece < Pieces; ++Piece)
			{
				const double W = 60.0 + 600.0 * Unit(Dice), H = 30.0 + 300.0 * Unit(Dice);
				const double X = 24.0 + (Window.ScreenW - 48.0 - W) * Unit(Dice);
				const double Y = Unit(Dice) < 0.5 ? 24.0 : Window.ScreenH - 24.0 - H;
				PiecesPx.push_back(Painted(FBox{ X, Y, X + W, Y + H }, Px));
			}
			std::vector<FWant> Wants(1 + static_cast<size_t>(Unit(Dice) * 9.0));
			for (FWant& Want : Wants)
			{
				Want.bPlace = Unit(Dice) < 0.6;
				Want.At = FPoint{ (Unit(Dice) * 1.4 - 0.2) * Window.ScreenW, (Unit(Dice) * 1.4 - 0.2) * Window.ScreenH };
			}
			const int Total = static_cast<int>(Wants.size()) + static_cast<int>(Unit(Dice) * 5.0);
			const double WindowX = 300.0 * Unit(Dice), WindowY = 25.0 + 200.0 * Unit(Dice);
			Layout.LayRanked(Window, Wants, Total, 9, KeptOff(PiecesPx, Px, WindowX, WindowY, false), Spots);
			RightUnder += UnderCount(Window, Spots, PiecesPx, Px);
			Layout.LayRanked(Window, Wants, Total, 9, KeptOff(PiecesPx, Px, WindowX, WindowY, true), Spots);
			WrongRounds += UnderCount(Window, Spots, PiecesPx, Px) > 0 ? 1 : 0;
			++Rounds;
		}
		std::printf("(under the frame, %d random windows: laid right %d seals under a piece; read in the desktop's measure, %d windows with one or more)\n", Rounds, RightUnder, WrongRounds);
		CHECK(RightUnder == 0);
		CHECK(WrongRounds > Rounds / 10);
	}
}

/** (0010) The photographer's asks of the seals, "-TheaterSeal=open:N,hover:N,slide", as the layer reads them once. */
static void TestSealAsk()
{
	const FSealAsk None = ReadSealAsk("");
	CHECK(None.Open == 0 && None.Hover == 0 && !None.bSlide && None.Unknown == 0);
	const FSealAsk Open = ReadSealAsk("open:3");
	CHECK(Open.Open == 3 && Open.Hover == 0 && !Open.bSlide && Open.Unknown == 0);
	const FSealAsk All = ReadSealAsk("hover:2,slide,open:1");
	CHECK(All.Open == 1 && All.Hover == 2 && All.bSlide && All.Unknown == 0);
	// (letters in either case, spaces about the parts, an empty part, and the last of two asks of a kind stand)
	const FSealAsk Loose = ReadSealAsk(" Hover:4 ,, SLIDE,open:2,open:7");
	CHECK(Loose.Open == 7 && Loose.Hover == 4 && Loose.bSlide && Loose.Unknown == 0);
	// (a seal past the most that may stand, nine, names none: it is not known, and the ask before it stands; with room
	// for twelve, it is a seal)
	const FSealAsk Past = ReadSealAsk("open:2,open:10,hover:99", 9);
	CHECK(Past.Open == 2 && Past.Hover == 0 && Past.Unknown == 2);
	CHECK(ReadSealAsk("open:9,hover:9").Open == 9 && ReadSealAsk("open:9,hover:9").Hover == 9 && ReadSealAsk("open:12", 12).Open == 12);
	// (a long number is no seal, and is never read as one: no count past what an int holds, whatever is typed)
	const FSealAsk Long = ReadSealAsk("open:4294967299,hover:99999999999999999999,open:3");
	CHECK(Long.Open == 3 && Long.Hover == 0 && Long.Unknown == 2);
	// (a seal is named from 1, in at most two figures: anything else is not known, and left out)
	const FSealAsk Wrong = ReadSealAsk("open:0,hover:-1,open:x,hover:,open,slid,hover:123,ping:2");
	CHECK(Wrong.Open == 0 && Wrong.Hover == 0 && !Wrong.bSlide && Wrong.Unknown == 8);
	const FSealAsk Some = ReadSealAsk("open:2,glow");
	CHECK(Some.Open == 2 && Some.Unknown == 1);
}

/** The art director's ruling of 22 November, section 1: a seal wears the colour of the row it opens, and which pulse. */
static void TestColours()
{
	auto Is = [](const FLook& Worn, EWax Wax, bool bPulse) { return Worn.Wax == Wax && Worn.bPulse == bPulse; };
	// A dated warning: red, and it pulses, as a band (a band's tone is -1 where the card does not edge it green), as a
	// row that found no band, and as the lead of the month a stake falls due.
	CHECK(Is(LookOf(ESection::Band, "WARNING", -1), EWax::Red, true));
	CHECK(Is(LookOf(ESection::Row, "WARNING", -1), EWax::Red, true));
	CHECK(Is(LookOf(ESection::Lead, "LAST MONTH", -1), EWax::Red, true));
	// WAR, FALLEN, FREED and NEWS: the news band's brass, and they pulse, as a band and as a row that found no band.
	bool bBand = true;
	for (const char* Word : { "WAR", "FALLEN", "FREED", "NEWS" })
	{
		bBand = bBand && Is(LookOf(ESection::Band, Word, -1), EWax::Band, true) && Is(LookOf(ESection::Row, Word, -1), EWax::Band, true);
		bBand = bBand && Is(LookOf(ESection::Row, Word, 0), EWax::Band, true); // (a NEWS row's tone is plain)
	}
	CHECK(bBand);
	CHECK(Is(LookOf(ESection::Band, "WAR", -1), EWax::Band, true));
	CHECK(Is(LookOf(ESection::Band, "FALLEN", -1), EWax::Band, true));
	CHECK(Is(LookOf(ESection::Band, "FREED", -1), EWax::Band, true));
	CHECK(Is(LookOf(ESection::Band, "NEWS", -1), EWax::Band, true));
	// A FREED his own side won keeps the card's green, and pulses: the band's "good" (Tone 1, as EdgeNews passes
	// Band.bGood), and the row's tone 1 for the same freeing when it found no band.
	CHECK(Is(LookOf(ESection::Band, "FREED", 1), EWax::Green, true));
	CHECK(Is(LookOf(ESection::Row, "FREED", 1), EWax::Green, true));
	// Amber, green, plain brass and grey seals do not pulse.
	CHECK(Is(LookOf(ESection::Row, "BESIEGED", 2), EWax::Amber, false));
	CHECK(Is(LookOf(ESection::Row, "BLOCKED", 2), EWax::Amber, false));
	CHECK(Is(LookOf(ESection::Row, "WON", 1), EWax::Green, false));
	CHECK(Is(LookOf(ESection::Lead, "WON", 1), EWax::Green, false));
	// (a front's row of plain news, tagless: the card edges it in its muted grey, and its seal is grey; home's lines of
	// this month, which the card draws in brass-300, stay plain brass, and so does a lead that is neither good nor bad)
	CHECK(Is(LookOf(ESection::Row, "", 0), EWax::Grey, false));
	CHECK(Is(LookOf(ESection::Lead, "", 0), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::Home, "", 0), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::Staff, "", 0), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::More, "", 0), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::Home, "", 3), EWax::Grey, false));
	CHECK(Is(LookOf(ESection::Elsewhere, "", 0), EWax::Grey, false));
	// Bad news is no warning: nothing that arrives wears red.
	CHECK(Is(LookOf(ESection::Row, "LOST", -1), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::Lead, "LOST", -1), EWax::Brass, false));
	CHECK(Is(LookOf(ESection::Row, "SUNK", -1), EWax::Brass, false));
	// Over every part, word and tone: red is the dated warning's alone; the news band's brass is class 2's alone; only
	// red, band brass and class 2's green pulse; amber, green, plain brass and grey otherwise never do.
	const ESection Sections[] = { ESection::Band, ESection::Lead, ESection::Row, ESection::Home, ESection::Staff, ESection::Elsewhere, ESection::More };
	const char* Words[] = { "", "WARNING", "WAR", "FALLEN", "FREED", "NEWS", "LAST MONTH", "WON", "LOST", "HELD", "BESIEGED", "SUNK", "HIT", "BLOCKED", "MISSED" };
	bool bRedOnly = true, bBandOnly = true, bPulseOnly = true, bStill = true;
	for (const ESection Section : Sections)
	{
		for (const char* Word : Words)
		{
			for (int Tone = -1; Tone <= 3; ++Tone)
			{
				const FLook Worn = LookOf(Section, Word, Tone);
				const int Class = ClassOf(Section, Word);
				const bool bDated = Class == 1 || (Section == ESection::Lead && std::string(Word) == "LAST MONTH");
				bRedOnly = bRedOnly && (Worn.Wax == EWax::Red) == bDated;
				bBandOnly = bBandOnly && (Worn.Wax == EWax::Band) == (Class == 2 && !bDated && Tone != 1);
				bPulseOnly = bPulseOnly && Worn.bPulse == (bDated || Class == 2);
				const bool bQuiet = Worn.Wax == EWax::Amber || Worn.Wax == EWax::Brass || Worn.Wax == EWax::Grey || (Worn.Wax == EWax::Green && Class != 2);
				bStill = bStill && (!bQuiet || !Worn.bPulse);
			}
		}
	}
	CHECK(bRedOnly);
	CHECK(bBandOnly);
	CHECK(bPulseOnly);
	CHECK(bStill);
	// A front's row and its seal match on the Unreal card: the row's edge as CampaignHUD's MonthRowOf draws it (with
	// this patch's hook, ruling 160: a band's word that found no band on the news band's brass-300 unless his own side
	// freed it), in the seals' colours. The one difference is ruled: a row of bad news is edged red, its seal plain brass.
	auto CardEdge = [](const std::string& Tag, int Tone)
	{
		const bool bNews = Tag == "WAR" || Tag == "FALLEN" || Tag == "FREED" || Tag == "NEWS";
		if (bNews && Tone != 1)
		{
			return EWax::Band;
		}
		return Tone == 1 ? EWax::Green : Tone == -1 ? EWax::Red : Tone == 2 ? EWax::Amber : EWax::Grey;
	};
	bool bMatch = true;
	for (const char* Word : Words)
	{
		for (int Tone = -1; Tone <= 3; ++Tone)
		{
			if (std::string(Word) == "WARNING" && Tone != -1)
			{
				continue; // (a warning's row is always edged red: the card gives it the tone "bad")
			}
			const EWax Seal = LookOf(ESection::Row, Word, Tone).Wax, Edge = CardEdge(Word, Tone);
			bMatch = bMatch && (Seal == Edge || (Edge == EWax::Red && std::string(Word) != "WARNING" && Seal == EWax::Brass));
		}
	}
	CHECK(bMatch);
	CHECK(CardEdge("NEWS", 0) == EWax::Band && LookOf(ESection::Row, "NEWS", 0).Wax == EWax::Band);
	CHECK(CardEdge("WAR", -1) == EWax::Band && LookOf(ESection::Row, "WAR", -1).Wax == EWax::Band);
	CHECK(CardEdge("", 0) == EWax::Grey && LookOf(ESection::Row, "", 0).Wax == EWax::Grey);
	// Our own order signs are pressed in; no star (the ruling's section 2), nor the night raid's moon and star, nor the
	// day raid's sun (the art director's call of 22 November, 08:18Z: at the seal's weight it reads as a cog). (Those two
	// are the raid card's signs: no row of the card is signed "day" or "night" today, as a raid's row is signed bmr.)
	CHECK(MayPress("inf") && MayPress("hull") && MayPress("ftr") && MayPress("art") && MayPress("aa"));
	CHECK(!MayPress("star") && !MayPress("night") && !MayPress("day") && !MayPress(""));
	CHECK(GlyphOf(ESection::Row, "", MayPress("star")) == EGlyph::News);
	CHECK(GlyphOf(ESection::Home, "", MayPress("star")) == EGlyph::News);
	CHECK(GlyphOf(ESection::Row, "", MayPress("day")) == EGlyph::News && GlyphOf(ESection::Row, "", MayPress("night")) == EGlyph::News);
	CHECK(GlyphOf(ESection::Row, "WON", MayPress("inf")) == EGlyph::Sign);
	// (a raid's row as the card signs it, bmr by day or by night, HIT, MISSED or HELD: its seal presses the bomber)
	CHECK(MayPress("bmr") && GlyphOf(ESection::Row, "HIT", MayPress("bmr")) == EGlyph::Sign && GlyphOf(ESection::Row, "MISSED", MayPress("bmr")) == EGlyph::Sign
		&& GlyphOf(ESection::Row, "HELD", MayPress("bmr")) == EGlyph::Sign);
}

namespace
{
	/** A drawn mark's box about the disc's middle, and how far its farthest point lies from it. */
	struct FMarkBox
	{
		double MinX = 1e9, MinY = 1e9, MaxX = -1e9, MaxY = -1e9, Far = 0.0;
	};

	FMarkBox MarkBox(const Look::FStroke* Strokes, int Count)
	{
		FMarkBox Box;
		for (int Index = 0; Index < Count; ++Index)
		{
			const Look::FStroke& Cut = Strokes[Index];
			double HalfX = Cut.Long * 0.5, HalfY = Cut.Long * 0.5, Far = std::hypot(Cut.X, Cut.Y) + Cut.Long * 0.5;
			if (Cut.Kind == Look::EStroke::Bar)
			{
				// (a bar with round ends: its two ends' middles, and the round end's half width round them)
				const double Turn = Cut.Degrees * 3.141592653589793 / 180.0, Reach = (Cut.Long - Cut.Thick) * 0.5;
				HalfX = Reach * std::fabs(std::cos(Turn)) + Cut.Thick * 0.5;
				HalfY = Reach * std::fabs(std::sin(Turn)) + Cut.Thick * 0.5;
				Far = std::max(std::hypot(Cut.X + Reach * std::cos(Turn), Cut.Y + Reach * std::sin(Turn)), std::hypot(Cut.X - Reach * std::cos(Turn), Cut.Y - Reach * std::sin(Turn))) + Cut.Thick * 0.5;
			}
			Box.MinX = std::min(Box.MinX, Cut.X - HalfX);
			Box.MaxX = std::max(Box.MaxX, Cut.X + HalfX);
			Box.MinY = std::min(Box.MinY, Cut.Y - HalfY);
			Box.MaxY = std::max(Box.MaxY, Cut.Y + HalfY);
			Box.Far = std::max(Box.Far, Far);
		}
		return Box;
	}
}

namespace
{
	/** A stroke as a segment and a half width (a bar's round ends, a dot): the ink is every point within Half of it. */
	struct FCapsule
	{
		double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0, Half = 0.0;
	};

	FCapsule CapsuleOf(const Look::FStroke& Cut)
	{
		if (Cut.Kind == Look::EStroke::Dot)
		{
			return FCapsule{ Cut.X, Cut.Y, Cut.X, Cut.Y, Cut.Long * 0.5 };
		}
		const double Turn = Cut.Degrees * 3.141592653589793 / 180.0, Reach = (Cut.Long - Cut.Thick) * 0.5;
		return FCapsule{ Cut.X - Reach * std::cos(Turn), Cut.Y - Reach * std::sin(Turn), Cut.X + Reach * std::cos(Turn), Cut.Y + Reach * std::sin(Turn), Cut.Thick * 0.5 };
	}

	double PointToSegment(double PX, double PY, const FCapsule& S)
	{
		const double DX = S.BX - S.AX, DY = S.BY - S.AY, Length2 = DX * DX + DY * DY;
		const double T = Length2 > 0.0 ? std::clamp(((PX - S.AX) * DX + (PY - S.AY) * DY) / Length2, 0.0, 1.0) : 0.0;
		return std::hypot(PX - (S.AX + T * DX), PY - (S.AY + T * DY));
	}

	/** The clear ground between two strokes' ink (negative where they overlap): segments that do not cross. */
	double Clear(const FCapsule& A, const FCapsule& B)
	{
		const double Least = std::min({ PointToSegment(A.AX, A.AY, B), PointToSegment(A.BX, A.BY, B), PointToSegment(B.AX, B.AY, A), PointToSegment(B.BX, B.BY, A) });
		return Least - A.Half - B.Half;
	}
}

/**
 * (0010) The war mark as crossed swords (the art director, 22 November, 08:36Z: the plain X read "cancel"): long blades
 * narrowing to their points above the crossing, a guard across each near its foot and a short grip below, and a small
 * pommel where it holds 1.5 px on the screen; as heavy and about as large as the X was, and the same on both sides. The
 * review of 0010: the outline must say "sword" without the pommel (it read as the X with a notch across each lower
 * arm at 1441 by 878 and 1024 by 768), each part that must hold 1.5 px is measured on each side, as the eye sees it
 * (the guard past the blade at its full width, the pommel past its grip), and nothing swells on a screen of no size.
 */
static void TestSwords()
{
	auto High = [](double Height) { return (1000.0 / 900.0) * (Height / 1080.0); };
	const double Full = High(1080.0), Mac = High(1786.0), Reference = High(1000.0), Small = High(878.0), Least = High(768.0);
	// (Brian's window, 2881 by 1786 on the Mac's fine pixels at 2.0: 1.84 px a unit, the disc about 27 of the Mac's
	// points across, 55 of its pixels)
	CHECK(std::fabs(Mac - 1.8374) < 0.001 && std::fabs(Full - 1.1111) < 0.001);
	const double Screens[] = { Least, Small, High(971.0), Reference, Full, High(1440.0), Mac, 2.0, 2.0 * Mac, 3.0 };

	// The parts hold 1.5 px on every screen from 1024 by 768 up, each measured on each side: the guard's width, how far
	// it stands past the blade at its full width (its round end left out), the grip, the point; the blade is the X's
	// weight, 2.4 units, and the point narrower than the blade wherever the screen lets it be.
	{
		bool bHolds = true, bNarrows = true;
		for (const double Scale : Screens)
		{
			const Look::FSword Sword = Look::SwordOf(Scale);
			bHolds = bHolds && Sword.GuardThick * Scale >= 1.5 - 1e-9 && (Sword.Overhang - Sword.GuardThick * 0.5) * Scale >= 1.5 - 1e-9 && Sword.GripThick * Scale >= 1.5 - 1e-9
				&& Sword.PointThick * Scale >= 1.5 - 1e-9 && Sword.BladeThick == Look::Stroke(2.4, Scale);
			bNarrows = bNarrows && Sword.PointThick < Sword.BladeThick;
		}
		CHECK(bHolds);
		CHECK(bNarrows);
		const Look::FSword AtFull = Look::SwordOf(Full), AtSmall = Look::SwordOf(Small), AtLeast = Look::SwordOf(Least);
		std::printf("(the swords: the guard past the blade at its full width %.2f px at 1920 by 1080, %.2f at 1441 by 878, %.2f at 1024 by 768; the point %.2f px against the blade's %.2f at 1441 by 878)\n",
			(AtFull.Overhang - AtFull.GuardThick * 0.5) * Full, (AtSmall.Overhang - AtSmall.GuardThick * 0.5) * Small, (AtLeast.Overhang - AtLeast.GuardThick * 0.5) * Least,
			AtSmall.PointThick * Small, AtSmall.BladeThick * Small);
	}
	// The pommel stands only where its neck and how far it stands past its grip on each side both hold 1.5 px: drawn
	// 3.2 across on a grip of 1.6, from 1.875 px a unit (a window 1823 high). Below it, at 1920 by 1080 and in Brian's
	// window too, the grip ends round, and the mark is eight bars.
	{
		bool bDropped = true, bShown = true;
		for (const double Scale : Screens)
		{
			const Look::FSword Sword = Look::SwordOf(Scale);
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(EGlyph::War, Scale, Strokes);
			const bool bHolds = Sword.Neck * Scale >= 1.5 - 1e-9 && (Sword.PommelD - Sword.GripThick) * 0.5 * Scale >= 1.5 - 1e-9;
			if (Scale < 1.875)
			{
				bDropped = bDropped && !Sword.bPommel && !bHolds && Count == 8 && Sword.GripEnd > Sword.Pommel;
				for (int Each = 0; Each < Count; ++Each)
				{
					bDropped = bDropped && Strokes[Each].Kind == Look::EStroke::Bar;
				}
			}
			else
			{
				bShown = bShown && Sword.bPommel && bHolds && Count == 10 && Sword.GripEnd == Sword.Pommel && Strokes[4].Kind == Look::EStroke::Dot && Strokes[9].Kind == Look::EStroke::Dot;
			}
		}
		CHECK(bDropped);
		CHECK(bShown);
		CHECK(Look::SwordOf(1.875).bPommel && !Look::SwordOf(1.875 - 1e-6).bPommel && !Look::SwordOf(Full).bPommel && !Look::SwordOf(Mac).bPommel);
		// (a screen of no measure is taken at a pixel a unit; and however small a screen is measured, the guard never
		// stands out more than 4 units, nor goes past the hollow's size: the review's denormal scale made it endless)
		CHECK(!Look::SwordOf(0.0).bPommel && Look::SwordOf(0.0).Overhang == Look::SwordOf(1.0).Overhang);
		bool bBounded = true;
		for (const double Tiny : { std::numeric_limits<double>::denorm_min(), 1e-300, 0.01, 0.1, 0.25 })
		{
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(EGlyph::War, Tiny, Strokes);
			const Look::FSword Sword = Look::SwordOf(Tiny);
			bBounded = bBounded && Sword.Overhang <= 4.0 && std::isfinite(Sword.Lift) && Count == 8;
			for (int Each = 0; Each < Count; ++Each)
			{
				bBounded = bBounded && std::isfinite(Strokes[Each].Long) && std::isfinite(Strokes[Each].X) && std::isfinite(Strokes[Each].Y) && Strokes[Each].Long <= 2.0 * Look::HollowR;
			}
		}
		CHECK(bBounded);
	}
	// The mark is the same on both sides: the second sword is the first in a mirror, stroke for stroke.
	{
		bool bMirror = true;
		for (const double Scale : Screens)
		{
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(EGlyph::War, Scale, Strokes);
			const int Half = Count / 2;
			bMirror = bMirror && Count % 2 == 0;
			for (int Each = 0; Each < Half; ++Each)
			{
				const Look::FStroke& A = Strokes[Each];
				const Look::FStroke& B = Strokes[Each + Half];
				bMirror = bMirror && A.Kind == B.Kind && std::fabs(A.X + B.X) < 1e-9 && std::fabs(A.Y - B.Y) < 1e-9 && A.Long == B.Long && A.Thick == B.Thick
					&& std::fabs(A.Degrees + B.Degrees) < 1e-9;
			}
		}
		CHECK(bMirror);
	}
	// About as large as the X was (two strokes 20 long and 2.4 thick, crossed 36 degrees off the level), within a unit
	// and a half each way, and centred on the disc's middle on every screen; the points up and out, the guards and hilts
	// down and out.
	{
		const double Reach = (20.0 - 2.4) * 0.5, OldW = 2.0 * (Reach * std::cos(36.0 * 3.141592653589793 / 180.0) + 1.2), OldH = 2.0 * (Reach * std::sin(36.0 * 3.141592653589793 / 180.0) + 1.2);
		bool bSize = true, bCentred = true, bWays = true;
		for (const double Scale : { Small, Reference, Full, Mac })
		{
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(EGlyph::War, Scale, Strokes);
			const FMarkBox Box = MarkBox(Strokes, Count);
			bSize = bSize && std::fabs((Box.MaxX - Box.MinX) - OldW) <= 1.5 && std::fabs((Box.MaxY - Box.MinY) - OldH) <= 1.5;
			bCentred = bCentred && std::fabs(Box.MaxX + Box.MinX) < 1e-9 && std::fabs(Box.MaxY + Box.MinY) < 1e-9;
			const Look::FSword Sword = Look::SwordOf(Scale);
			const FCapsule Point = CapsuleOf(Strokes[1]), Guard = CapsuleOf(Strokes[2]), Grip = CapsuleOf(Strokes[3]);
			bWays = bWays && std::min(Point.AY, Point.BY) < Sword.Lift - 4.0 && Guard.AY > Sword.Lift && Guard.BY > Sword.Lift && std::max(Grip.AY, Grip.BY) > Sword.Lift + 3.0;
		}
		CHECK(bSize);
		CHECK(bCentred);
		CHECK(bWays);
		Look::FStroke Strokes[Look::MarkMost];
		const FMarkBox Box = MarkBox(Strokes, Look::MarkOf(EGlyph::War, Full, Strokes));
		std::printf("(the swords' box %.2f by %.2f units at 1920 by 1080, the X's %.2f by %.2f)\n", Box.MaxX - Box.MinX, Box.MaxY - Box.MinY, OldW, OldH);
	}
	// The outline says "sword" without the pommel: from where the two cross, each blade runs to its point a third again
	// as far as its hilt runs to the grip's end (the X's four arms were equal), and narrows to that point.
	{
		bool bLong = true;
		for (const double Scale : Screens)
		{
			const Look::FSword Sword = Look::SwordOf(Scale);
			const double Hilt = Sword.bPommel ? -Sword.Pommel + Sword.PommelD * 0.5 : -Sword.GripEnd + Sword.GripThick * 0.5;
			bLong = bLong && (Sword.bPommel || Sword.Tip >= 1.2 * Hilt) && Sword.Point >= 2.0;
		}
		CHECK(bLong);
	}
	// No part runs into another it should stand apart from, by 1.5 px, on every screen from 1024 by 768 up: a guard and
	// the other sword's blade and point, the two guards, a pommel and its own guard, the two hilts.
	{
		bool bApart = true;
		double Tightest = 1e9;
		for (const double Scale : Screens)
		{
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(EGlyph::War, Scale, Strokes);
			const int Half = Count / 2; // (each sword: 0 blade, 1 point, 2 guard, 3 grip, 4 pommel where it stands)
			auto Gap = [&](int A, int B) { return Clear(CapsuleOf(Strokes[A]), CapsuleOf(Strokes[B])) * Scale; };
			const double Least2 = std::min({ Gap(2, Half + 0), Gap(2, Half + 1), Gap(Half + 2, 0), Gap(Half + 2, 1), Gap(2, Half + 2), Gap(3, Half + 3) });
			Tightest = std::min(Tightest, Least2);
			bApart = bApart && Least2 >= 1.5 - 1e-9;
			if (Half == 5)
			{
				bApart = bApart && Gap(4, 2) >= 1.5 - 1e-9 && Gap(4, Half + 4) >= 1.5 && Gap(4, Half + 3) >= 1.5;
			}
		}
		CHECK(bApart);
		std::printf("(the swords' parts that stand apart: %.2f px at the least)\n", Tightest);
	}
}

namespace
{
	/** The real months' cards (real_months.inc, from the rules bundle): one line a month, then one an item, tab-separated. */
	const char* const RealMonths =
#include "real_months.inc"
		;

	/** One month of the card as the edge's seals see it: each item's look and what the card says of it, in card order. */
	struct FMonth
	{
		std::string Seat;
		std::string Date;
		std::vector<FAlike> Looks;
		std::vector<FSaid> Said;
	};

	std::vector<std::string> Fields(const std::string& Line)
	{
		std::vector<std::string> Out;
		size_t At = 0;
		for (;;)
		{
			const size_t Tab = Line.find('\t', At);
			Out.push_back(Line.substr(At, Tab == std::string::npos ? std::string::npos : Tab - At));
			if (Tab == std::string::npos)
			{
				break;
			}
			At = Tab + 1;
		}
		Out.resize(std::max<size_t>(Out.size(), 8));
		return Out;
	}

	/** An item as EdgeNews.cpp's EdgeSealOf makes it: its look (AlikeOf, the lead's sign from its picture) and its words. */
	void AddItem(FMonth& Month, ESection Section, const std::string& Word, int Tone, const std::string& Sign, const std::string& A, const std::string& B, const std::string& C, const std::string& Text)
	{
		Month.Looks.push_back(AlikeOf(Section, Word, Tone, Section == ESection::Lead ? LeadSign(Sign) : Sign));
		if (Section == ESection::Row)
		{
			Month.Said.push_back(RowSaid(A, Word, B, C, Text));
		}
		else if (Section == ESection::Home)
		{
			std::string Said = A.empty() ? Text : A + " " + Text;
			Said += B.empty() ? "" : ", " + B;
			Month.Said.push_back(PlainSaid(Section, Said));
		}
		else
		{
			Month.Said.push_back(PlainSaid(Section, Text));
		}
	}

	std::vector<FMonth> ReadMonths()
	{
		std::vector<FMonth> Months;
		const std::string All(RealMonths);
		size_t At = 0;
		while (At < All.size())
		{
			const size_t End = std::min(All.find('\n', At), All.size());
			const std::string Line = All.substr(At, End - At);
			At = End + 1;
			if (Line.empty())
			{
				continue;
			}
			const std::vector<std::string> F = Fields(Line);
			if (F[0] == "M")
			{
				Months.push_back(FMonth{ F[1], F[2], {}, {} });
			}
			else if (!Months.empty())
			{
				AddItem(Months.back(), static_cast<ESection>(std::atoi(F[0].c_str())), F[1], std::atoi(F[2].c_str()), F[3], F[4], F[5], F[6], F[7]);
			}
		}
		return Months;
	}

	/** Mac run 3's month (USA, grand, from January 1942, seed 1939): the card of January 1942 as the rules gave it. */
	FMonth RunThree()
	{
		FMonth Month{ "USA", "January 1942", {}, {} };
		AddItem(Month, ESection::Band, "WAR", -1, "", "", "", "", "Cuba declares war on Germany");
		AddItem(Month, ESection::Band, "WAR", -1, "", "", "", "", "Panama declares war on Germany");
		for (const char* Place : { "Costa Rica", "Nicaragua", "Honduras", "El Salvador", "Guatemala", "Haiti", "Dominican Republic" })
		{
			const std::string Named = std::string(Place) == "Dominican Republic" ? "The Dominican Republic" : Place;
			AddItem(Month, ESection::Row, "WAR", -1, "", Place, "", "", Named + " declares war on Germany.");
		}
		AddItem(Month, ESection::Row, "NEWS", 0, "", "Soviet Union", "", "", "British and Soviet troops have entered Iran.");
		AddItem(Month, ESection::Home, "", 0, "", "", "", "", "German envoys are courting Buenos Aires.");
		AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", "Soviet troops close round Helsinki: the capital of Finland is under siege, with one corps inside.");
		AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", "Soviet troops blow the bridges over the Oka at Kaluga as German forces reach the river.");
		AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", "Scorched earth: German troops tore up the railway at Kaluga as they were driven off.");
		AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", "No. 4 Group bombs Berlin by night.");
		AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", "4\xC2\xAA Divisione Aerea strikes at shipping in the Western Mediterranean.");
		return Month;
	}

	/** The runs cover every item once, in card order: each starts where the last ended, and their counts add up. */
	bool RunsCover(const std::vector<FAlikeRun>& Runs, int Items)
	{
		int Next = 0;
		for (const FAlikeRun& Run : Runs)
		{
			if (Run.First != Next || Run.Count < 1)
			{
				return false;
			}
			Next += Run.Count;
		}
		return Next == Items;
	}

	/** Each run is alike, holds no warning where it folds, and is whole: a fold is never 2, and no item next to it looks the same. */
	bool RunsRight(const std::vector<FAlike>& Looks, const std::vector<FAlikeRun>& Runs, std::string& Why)
	{
		for (size_t Index = 0; Index < Runs.size(); ++Index)
		{
			const FAlikeRun& Run = Runs[Index];
			const FAlike& First = Looks[static_cast<size_t>(Run.First)];
			if (Run.Count == 2)
			{
				Why = "a fold of two";
				return false;
			}
			for (int Each = Run.First; Each < Run.First + Run.Count; ++Each)
			{
				if (!(Looks[static_cast<size_t>(Each)] == First) || (Run.Count > 1 && Looks[static_cast<size_t>(Each)].IsWarning()))
				{
					Why = "a fold of items that are not alike, or of a warning";
					return false;
				}
			}
			const int Before = Run.First - 1, After = Run.First + Run.Count;
			if (Run.Count > 1 && ((Before >= 0 && Looks[static_cast<size_t>(Before)] == First) || (After < static_cast<int>(Looks.size()) && Looks[static_cast<size_t>(After)] == First)))
			{
				Why = "a fold that stops short of an alike item";
				return false;
			}
			// (three alike in a row that are not a warning always fold)
			if (Run.Count == 1 && !First.IsWarning() && Index + 2 < Runs.size() && Runs[Index + 1].Count == 1 && Runs[Index + 2].Count == 1
				&& Looks[static_cast<size_t>(Runs[Index + 1].First)] == First && Looks[static_cast<size_t>(Runs[Index + 2].First)] == First)
			{
				Why = "three alike left unfolded";
				return false;
			}
		}
		return true;
	}

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	bool StartsWith(const std::string& Text, const std::string& Part)
	{
		return Text.compare(0, Part.size(), Part) == 0;
	}

	/** A sentence with its "its" and "his" read as "their", as a folded seal's words say them of more than one. */
	std::string Theirs(const std::string& Sentence)
	{
		std::string Out = " " + Sentence + " ";
		for (const char* Own : { " its ", " his " })
		{
			for (size_t At = Out.find(Own); At != std::string::npos; At = Out.find(Own, At + 1))
			{
				Out.replace(At, 5, " their ");
			}
		}
		return Out.substr(1, Out.size() - 2);
	}

	/**
	 * The test's own reading of a folded seal's first words where they cannot name the run: the line whole where it is
	 * one clause (no , ; : and no sentence's end before its last word) of at most 16 words, else its eight words; no
	 * stop at the end.
	 */
	std::string FirstSaid(const std::string& Line)
	{
		std::vector<std::string> Words;
		size_t At = 0;
		while (At < Line.size())
		{
			while (At < Line.size() && Line[At] == ' ')
			{
				++At;
			}
			const size_t Start = At;
			while (At < Line.size() && Line[At] != ' ')
			{
				++At;
			}
			if (At > Start)
			{
				Words.push_back(Line.substr(Start, At - Start));
			}
		}
		bool bClause = !Words.empty() && Words.size() <= 16;
		for (size_t Index = 0; bClause && Index < Words.size(); ++Index)
		{
			const char Last = Words[Index].back();
			bClause = Last != ',' && Last != ';' && Last != ':' && Last != '!' && Last != '?' && (Last != '.' || Index + 1 == Words.size());
		}
		std::string Out = bClause ? Line : EightWords(Line);
		while (!Out.empty() && (Out.back() == '.' || Out.back() == ',' || Out.back() == ':' || Out.back() == ';' || Out.back() == ' '))
		{
			Out.pop_back();
		}
		return Out;
	}

	/**
	 * Nothing the card does not say: a folded seal's words begin with its count, name its first items in card order
	 * (each from its own sentence or its row's place), and say how many more; or they are its first item's own words,
	 * then how many more reports (never "and N more" after the first's own words, which reads as more of the first),
	 * and the parts of the card those are in.
	 */
	bool SaysOnlyTheCard(const std::vector<FSaid>& Said, const FAlikeRun& Run, const std::string& Words, std::string& Why)
	{
		const std::string Count = std::to_string(Run.Count);
		const FSaid& First = Said[static_cast<size_t>(Run.First)];
		const size_t Colon = Words.find(": ");
		if (StartsWith(Words, Count + " ") && Colon != std::string::npos)
		{
			const std::string Says = Words.substr(Count.size() + 1, Colon - Count.size() - 1);
			const std::string Names = Words.substr(Colon + 2);
			// (the words after the verb are on every item's sentence, or the head on every row)
			const size_t Space = Says.find(' ');
			const std::string Tail = Space == std::string::npos ? std::string() : Says.substr(Space);
			const int Named = Run.Count > 4 ? 3 : Run.Count;
			size_t From = 0;
			for (int Index = 0; Index < Run.Count; ++Index)
			{
				const FSaid& Item = Said[static_cast<size_t>(Run.First + Index)];
				const bool bHead = Item.Head == Says;
				if (!bHead && !Contains(Theirs(Item.Sentence), Tail))
				{
					Why = "a folded seal's words say what an item's sentence does not";
					return false;
				}
				if (Index < Named)
				{
					// (its name, where the list has it, in card order: its row's place, or the words of its own sentence before
					// its verb, the word before the rest)
					std::string Sentence = Theirs(Item.Sentence);
					while (!Sentence.empty() && Sentence.back() == '.')
					{
						Sentence.pop_back();
					}
					const size_t Rest = Tail.empty() ? Sentence.size() : Sentence.find(Tail);
					const size_t Verb = Rest == std::string::npos || Rest == 0 ? std::string::npos : Sentence.rfind(' ', Rest - 1);
					std::string Name = bHead ? Item.Name : Verb == std::string::npos ? std::string() : Sentence.substr(0, Verb);
					if (StartsWith(Name, "The "))
					{
						Name[0] = 't';
					}
					const size_t Found = Names.find(Name, From);
					if (Found == std::string::npos || Name.empty())
					{
						Why = "a name not in card order, or not the card's";
						return false;
					}
					From = Found + Name.size();
				}
			}
			const std::string Rest = Run.Count > 4 ? " and " + std::to_string(Run.Count - 3) + " more" : std::string();
			if (Names.size() < Rest.size() || Names.compare(Names.size() - Rest.size(), Rest.size(), Rest) != 0)
			{
				Why = "a folded seal's words that do not say how many more";
				return false;
			}
			return true;
		}
		const std::string Own = FirstSaid(First.Line);
		const int More = Run.Count - 1;
		std::string Where;
		for (int Index = Run.First + 1; Index < Run.First + Run.Count; ++Index)
		{
			const ESection Part = Said[static_cast<size_t>(Index)].Section;
			const std::string Named = Part == ESection::Row ? "on your fronts" : Part == ESection::Home ? "at home" : Part == ESection::Staff ? "from your staff" : Part == ESection::Elsewhere ? "elsewhere in the war" : "?";
			if (!Contains(Where, Named))
			{
				Where += (Where.empty() ? " " : " and ") + Named;
			}
		}
		Where = Contains(Where, "?") ? std::string() : Where;
		const std::string Rest = "; " + std::to_string(More) + (More == 1 ? " more report" : " more reports");
		// (three parts are listed "a, b and c": the test takes them as "a and b and c")
		std::string Given = Words;
		const size_t Comma = Given.find(", ", Own.size() + Rest.size());
		Given = Comma == std::string::npos ? Given : Given.replace(Comma, 2, " and ");
		if (!StartsWith(Words, Own + Rest) || Given != Own + Rest + Where)
		{
			Why = "a folded seal's words that are neither its names nor its first item's words, how many more reports and where";
			return false;
		}
		return true;
	}
}

/**
 * Ruling 64(g) as amended 08:55Z: three or more alike in a row stand as one seal with their count, at the first item's
 * place; a press opens the card at the run's first row. Mac run 3's month, the warnings, the order and the counts over
 * every real month, the card left as it was, the words under the pointer, the "+N" counting items, and the tab.
 */
static void TestFold()
{
	// 1. Mac run 3's month (January 1942, USA): sixteen items, so eight seals and a "+8" before the fold. Folded: four,
	// the nine declarations of war as one, the news from Iran, home's line, and elsewhere's five as one; no "+N".
	const FMonth Three = RunThree();
	std::vector<FAlikeRun> Runs;
	FoldAlike(Three.Looks, Runs);
	CHECK(Three.Looks.size() == 16 && Runs.size() == 4);
	CHECK(Runs.size() == 4 && Runs[0].First == 0 && Runs[0].Count == 9 && Runs[1].Count == 1 && Runs[2].Count == 1 && Runs[3].First == 11 && Runs[3].Count == 5);
	CHECK(Three.Looks[0].Wax == EWax::Band && Three.Looks[0].Glyph == EGlyph::War && Three.Looks[0].bPulse && Three.Looks[9].Glyph == EGlyph::News && Three.Looks[9].bPulse);
	CHECK(Three.Looks[10].Wax == EWax::Brass && !Three.Looks[10].bPulse && Three.Looks[11].Wax == EWax::Grey && Three.Looks[11].Glyph == EGlyph::Globe);
	// (elsewhere's five were five grey seals of their own before the fold: no two items ever shared a seal)
	CHECK(Fold(16, 9).Shown == 8 && Fold(16, 9).More == 8);
	if (Runs.size() == 4)
	{
		const std::string War = FoldWords(Three.Said, Runs[0]), Iran = FoldWords(Three.Said, Runs[1]), Home = FoldWords(Three.Said, Runs[2]), Elsewhere = FoldWords(Three.Said, Runs[3]);
		CHECK(War == "9 declare war on Germany: Cuba, Panama, Costa Rica and 6 more");
		CHECK(Iran == EightWords(Three.Said[9].Line) && Iran == "Soviet Union NEWS: British and Soviet troops");
		CHECK(Home == "German envoys are courting Buenos Aires.");
		CHECK(Elsewhere == "Soviet troops close round Helsinki; 4 more reports elsewhere in the war");
		std::printf("(run 3's month folded: \"%s\"; \"%s\")\n", War.c_str(), Elsewhere.c_str());
		// Laid at 1441 by 878 and at 1024 by 768 (the layer is 972 units high: 1595 by 972, 1296 by 972), with the frame:
		// all four stand, and no "+N".
		FLayout Layout;
		std::vector<FSpot> Spots;
		const std::vector<int> Carries = { 9, 1, 1, 5 };
		for (const double Wide : { 1441.0 / 878.0 * 972.0, 1024.0 / 768.0 * 972.0 })
		{
			FParams Window;
			Window.ScreenW = std::floor(Wide);
			Window.ScreenH = 972.0;
			std::vector<FWant> Wants(4);
			Wants[0] = FWant{ true, FPoint{ Window.ScreenW * 0.3, Window.ScreenH * 0.6 }, -1 }; // (Cuba, south-west on the map)
			Wants[1] = FWant{ true, FPoint{ Window.ScreenW * 0.9, Window.ScreenH * 0.4 }, -1 }; // (Iran, off to the east)
			const FFold Laid = Layout.LayRanked(Window, Wants, Carries, 16, 9, SmallFrame(Window.ScreenW, Window.ScreenH), Spots);
			std::string Why;
			CHECK(Laid.Shown == 4 && Laid.More == 0 && Spots.size() == 4 && Spots[0].bShown && Spots[3].bShown);
			CHECK(LaidClear(Window, SmallFrame(Window.ScreenW, Window.ScreenH), Spots, Why));
		}
	}

	// 2. A warning never folds, and no run crosses one: three warnings in a row stand as three; alike seals either side
	// of one are two runs, each folded only where it is three or more on its own.
	{
		FMonth Warned;
		for (int Each = 0; Each < 3; ++Each)
		{
			AddItem(Warned, ESection::Band, "WARNING", -1, "", "", "", "", "Act by March.");
		}
		AddItem(Warned, ESection::Lead, "LAST MONTH", -1, "troops", "", "", "", "The last month to hold the line.");
		for (int Each = 0; Each < 2; ++Each)
		{
			AddItem(Warned, ESection::Row, "WAR", -1, "", "A", "", "", "A declares war on B.");
		}
		AddItem(Warned, ESection::Row, "WARNING", -1, "", "C", "", "", "Act by April.");
		for (int Each = 0; Each < 3; ++Each)
		{
			AddItem(Warned, ESection::Row, "WAR", -1, "", "D", "", "", "D declares war on B.");
		}
		FoldAlike(Warned.Looks, Runs);
		CHECK(Warned.Looks[0].IsWarning() && Warned.Looks[3].IsWarning() && Warned.Looks[6].IsWarning() && !Warned.Looks[4].IsWarning());
		CHECK(Runs.size() == 8 && Runs[0].Count == 1 && Runs[1].Count == 1 && Runs[2].Count == 1 && Runs[3].Count == 1);
		CHECK(Runs.size() == 8 && Runs[4].Count == 1 && Runs[5].Count == 1 && Runs[6].Count == 1 && Runs[7].First == 7 && Runs[7].Count == 3);
		// (with a fold of two allowed, the warnings still stand alone)
		FoldAlike(Warned.Looks, Runs, 2);
		bool bAlone = true;
		for (const FAlikeRun& Run : Runs)
		{
			bAlone = bAlone && (Run.Count == 1 || !Warned.Looks[static_cast<size_t>(Run.First)].IsWarning());
		}
		CHECK(bAlone && Runs.size() == 7 && Runs[4].Count == 2);
		// (a WARNING is the warning's mark too, on whatever wax: three such in a row stand as three; and a run either side
		// of a warning in its middle is cut there, each side folded only where it is three or more on its own)
		const FAlike Marked = AlikeOf(ESection::Lead, "WARNING", -1, "");
		CHECK(Marked.Glyph == EGlyph::Warning && Marked.IsWarning());
		FoldAlike({ Marked, Marked, Marked }, Runs);
		CHECK(Runs.size() == 3);
		const FAlike Declared = AlikeOf(ESection::Band, "WAR", -1, ""), Warning = AlikeOf(ESection::Band, "WARNING", -1, "");
		FoldAlike({ Declared, Declared, Warning, Declared, Declared, Declared }, Runs);
		CHECK(Runs.size() == 4 && Runs[0].Count == 1 && Runs[1].Count == 1 && Runs[2].Count == 1 && Runs[3].First == 3 && Runs[3].Count == 3);
	}

	// 3. Order kept and counts adding up, over random cards (warnings among them) and over every real month of every
	// seat in real_months.inc; and the "+N" counts items: what stands carries, and the "+N" counts, every item.
	{
		const FAlike Palette[] = { AlikeOf(ESection::Band, "WAR", -1, ""), AlikeOf(ESection::Band, "WARNING", -1, ""), AlikeOf(ESection::Row, "", 0, ""),
			AlikeOf(ESection::Row, "LOST", -1, "inf"), AlikeOf(ESection::Row, "LOST", -1, "arm"), AlikeOf(ESection::Elsewhere, "", 0, "") };
		std::mt19937 Dice(64);
		std::uniform_int_distribution<int> Pick(0, 5), Length(0, 30), Stay(0, 2);
		bool bCover = true, bRight = true;
		std::string Why;
		for (int Round = 0; Round < 20000; ++Round)
		{
			std::vector<FAlike> Looks;
			const int Items = Length(Dice);
			int Last = Pick(Dice);
			for (int Each = 0; Each < Items; ++Each)
			{
				Last = Stay(Dice) > 0 ? Last : Pick(Dice);
				Looks.push_back(Palette[Last]);
			}
			FoldAlike(Looks, Runs);
			bCover = bCover && RunsCover(Runs, Items);
			bRight = bRight && RunsRight(Looks, Runs, Why);
		}
		CHECK(bCover);
		CHECK(bRight);

		const std::vector<FMonth> Months = ReadMonths();
		int Seats = 0, Folded = 0, Before = 0, After = 0, Lost = 0, Wrong = 0;
		std::string LastSeat;
		FLayout Layout;
		std::vector<FSpot> Spots;
		std::vector<FWant> Wants;
		std::vector<int> Carries;
		for (const FMonth& Month : Months)
		{
			Seats += Month.Seat != LastSeat ? 1 : 0;
			LastSeat = Month.Seat;
			FoldAlike(Month.Looks, Runs);
			bCover = bCover && RunsCover(Runs, static_cast<int>(Month.Looks.size()));
			bRight = bRight && RunsRight(Month.Looks, Runs, Why);
			Before += Fold(static_cast<int>(Month.Looks.size()), 9).Shown + (Fold(static_cast<int>(Month.Looks.size()), 9).More > 0 ? 1 : 0);
			// (the board keeps the first nine runs; the screen stands what it has room for, the rest in a "+N" of items)
			const size_t Kept = std::min<size_t>(Runs.size(), 9);
			Wants.assign(Kept, FWant());
			Carries.clear();
			for (size_t Index = 0; Index < Kept; ++Index)
			{
				Carries.push_back(Runs[Index].Count);
				Folded += Runs[Index].Count > 1 ? 1 : 0;
			}
			for (const double Wide : { 1595.0, 1296.0 })
			{
				FParams Window;
				Window.ScreenW = Wide;
				Window.ScreenH = 972.0;
				const FFold Laid = Layout.LayRanked(Window, Wants, Carries, static_cast<int>(Month.Looks.size()), 9, SmallFrame(Wide, 972.0), Spots);
				int Carried = 0;
				for (int Index = 0; Index < Laid.Shown; ++Index)
				{
					Carried += Carries[static_cast<size_t>(Index)];
				}
				Lost += Carried + Laid.More != static_cast<int>(Month.Looks.size()) ? 1 : 0;
				Wrong += Laid.Shown + (Laid.More > 0 ? 1 : 0) > 9 ? 1 : 0;
				After += Wide == 1595.0 ? Laid.Shown + (Laid.More > 0 ? 1 : 0) : 0;
			}
		}
		CHECK(Months.size() >= 40 && Seats >= 10 && Folded >= 40);
		CHECK(bCover && bRight);
		CHECK(Lost == 0 && Wrong == 0);
		std::printf("(the fold over %zu real months of %d seats: %d folded seals; %d seals stood before it, %d after, at 1441 by 878)\n", Months.size(), Seats, Folded, Before, After);

		// The "+N" counts items, not seals: run 3's month with room for three seals only stands the nine and the news
		// from Iran, and its "+N" counts the six items of the two seals that do not stand.
		FParams Window;
		Window.ScreenW = 1595.0;
		Window.ScreenH = 972.0;
		const FFold Three3 = Layout.LayRanked(Window, std::vector<FWant>(4), { 9, 1, 1, 5 }, 16, 3, {}, Spots);
		CHECK(Three3.Shown == 2 && Three3.More == 6 && Spots.size() == 3);
		// (each item a seal of its own, as before the fold: the same as LayRanked without carries, every time)
		std::mt19937 Again(9);
		std::uniform_int_distribution<int> Count(0, 14);
		bool bSame = true;
		std::vector<FSpot> Others;
		for (int Round = 0; Round < 500; ++Round)
		{
			const int Given = Count(Again), Total = Given + Count(Again) / 3;
			const std::vector<FWant> Some(static_cast<size_t>(Given));
			const FFold A = Layout.LayRanked(Window, Some, Total, 9, SmallFrame(1595.0, 972.0), Spots);
			const FFold B = Layout.LayRanked(Window, Some, std::vector<int>(static_cast<size_t>(Given), 1), Total, 9, SmallFrame(1595.0, 972.0), Others);
			bSame = bSame && A.Shown == B.Shown && A.More == B.More && Spots.size() == Others.size();
		}
		CHECK(bSame);
		// (and laying the carried seals again allocates nothing, once warm)
		const std::vector<FBox> Frame = SmallFrame(1595.0, 972.0);
		const std::vector<FWant> Four(4);
		const std::vector<int> Carry = { 9, 1, 1, 5 };
		Layout.LayRanked(Window, Four, Carry, 16, 9, Frame, Spots);
		const long Was = Allocations;
		Layout.LayRanked(Window, Four, Carry, 16, 9, Frame, Spots);
		CHECK(Allocations == Was);
	}

	// 4. The card is unchanged: the fold reads the items and changes none of them, nor their order; the runs give back
	// every item in card order, and a folded seal is its first item's (so a press opens the card at the run's first row).
	{
		const FMonth Three2 = RunThree();
		const FMonth Kept = Three2;
		FoldAlike(Three2.Looks, Runs);
		for (const FAlikeRun& Run : Runs)
		{
			FoldWords(Three2.Said, Run);
		}
		bool bSame = Kept.Looks == Three2.Looks && Kept.Said.size() == Three2.Said.size();
		for (size_t Index = 0; bSame && Index < Kept.Said.size(); ++Index)
		{
			const FSaid& A = Kept.Said[Index];
			const FSaid& B = Three2.Said[Index];
			bSame = A.Section == B.Section && A.Line == B.Line && A.Sentence == B.Sentence && A.Name == B.Name && A.Head == B.Head;
		}
		CHECK(bSame);
		std::vector<FItem> Items;
		ListItems(FCardCounts{ 2, false, 8, 1, 0, 5 }, Items);
		std::vector<int> Back;
		for (const FAlikeRun& Run : Runs)
		{
			for (int Each = Run.First; Each < Run.First + Run.Count; ++Each)
			{
				Back.push_back(Each);
			}
		}
		bool bInOrder = Back.size() == Items.size();
		for (size_t Index = 0; bInOrder && Index < Back.size(); ++Index)
		{
			bInOrder = Back[Index] == static_cast<int>(Index);
		}
		CHECK(bInOrder && Items[static_cast<size_t>(Runs[0].First)].Section == ESection::Band && Items[static_cast<size_t>(Runs[3].First)].Section == ESection::Elsewhere && Items[static_cast<size_t>(Runs[3].First)].Index == 0);
	}

	// 5. The words under the pointer: the names in card order, how many more, nothing the card does not say.
	{
		auto Words = [](const std::vector<std::pair<std::string, std::string>>& Items)
		{
			FMonth Month;
			for (const auto& Item : Items)
			{
				if (Item.first.empty())
				{
					AddItem(Month, ESection::Elsewhere, "", 0, "", "", "", "", Item.second);
				}
				else
				{
					AddItem(Month, ESection::Row, "HIT", -1, "bmr", Item.first, "", "", Item.second);
				}
			}
			return FoldWords(Month.Said, FAlikeRun{ 0, static_cast<int>(Items.size()) });
		};
		CHECK(Words({ { "", "Paris falls." }, { "", "Rome falls." }, { "", "Oslo falls." } }) == "3 fall: Paris, Rome and Oslo");
		CHECK(Words({ { "", "Paris has fallen." }, { "", "Rome has fallen." }, { "", "Oslo has fallen." }, { "", "Riga has fallen." } }) == "4 have fallen: Paris, Rome, Oslo and Riga");
		CHECK(Words({ { "", "Bernard Montgomery reports for duty and is waiting for a command." }, { "", "Harold Alexander reports for duty and is waiting for a command." }, { "", "Alan Brooke reports for duty and is waiting for a command." } })
			== "3 report for duty and are waiting for a command: Bernard Montgomery, Harold Alexander and Alan Brooke");
		CHECK(Words({ { "", "The Dominican Republic declares war on Germany." }, { "", "Haiti declares war on Germany." }, { "", "Cuba declares war on Germany." } }) == "3 declare war on Germany: the Dominican Republic, Haiti and Cuba");
		// (the rows' head and their places, where their sentences name no one: the card's "Ruhr HIT" three times)
		CHECK(Words({ { "Ruhr", "Their bombers hit its factories by night." }, { "Hamburg", "Their bombers hit its factories by night." }, { "Bohemia", "Their bombers hit its factories by night." } }) == "3 HIT: Ruhr, Hamburg and Bohemia");
		// (else the first item's words: a name twice, a stop in the name, a capital after it, or different words after it)
		CHECK(Words({ { "", "Soviet troops close round Riga." }, { "", "Soviet troops close round Pskov." }, { "", "Soviet troops close round Narva." } }) == "Soviet troops close round Riga; 2 more reports elsewhere in the war");
		CHECK(Words({ { "", "Scorched earth: German troops tore up the railway." }, { "", "Scorched earth: Soviet troops tore up the railway." }, { "", "Scorched earth: Finnish troops tore up the railway." } })
			== "Scorched earth: German troops tore up the railway; 2 more reports elsewhere in the war");
		CHECK(Words({ { "", "Paris falls." }, { "", "Paris falls." }, { "", "Rome falls." } }) == "Paris falls; 2 more reports elsewhere in the war");
		CHECK(Words({ { "", "No. 3 Group bombs Paris by night." }, { "", "No. 4 Group bombs Rome by night." }, { "", "No. 5 Group bombs Turin by night." } }) == "No. 3 Group bombs Paris by night; 2 more reports elsewhere in the war");
		// (a list's "and" is never the verb, nor a name with one in it listed; words that run on into a second sentence are
		// not made plural; and an item with no words of its own gives the count and its part of the card)
		CHECK(Words({ { "", "Britain and France declare war on Germany." }, { "", "Canada and France declare war on Germany." }, { "", "India and France declare war on Germany." } })
			== "Britain and France declare war on Germany; 2 more reports elsewhere in the war");
		CHECK(Words({ { "", "Montgomery reports for duty. He was in Egypt." }, { "", "Alexander reports for duty. He was in Egypt." }, { "", "Brooke reports for duty. He was in Egypt." } })
			== "Montgomery reports for duty. He was in Egypt; 2 more reports elsewhere in the war");
		CHECK(Words({ { "", "U-47 torpedoes a battleship." }, { "", "U-99 torpedoes a battleship." }, { "", "U-100 torpedoes a battleship." } }) == "3 torpedo a battleship: U-47, U-99 and U-100");
		CHECK(Words({ { "", "" }, { "", "" }, { "", "" } }) == "3 elsewhere in the war");
		// (the review's cards, on the rules' own sentences: a thing named after a nation's adjective is the name's, never
		// the verb, navy.ts's "${adj} troops land at X from the sea" and diplomacy.ts's envoys; the predicate is said up to
		// a colon in it, a stop on the verb kept off its plural, and "its" made "their"; a second verb after "and", or a
		// "her", is not split; and a front's rows list "The sky over" as "the sky over", as the names do)
		auto Each = [&Words](std::initializer_list<const char*> Names, const std::string& Says)
		{
			std::vector<std::pair<std::string, std::string>> Items;
			for (const char* Name : Names)
			{
				Items.emplace_back("", std::string(Name) + " " + Says);
			}
			return Words(Items);
		};
		CHECK(Each({ "German troops", "British troops", "French troops" }, "land at Narvik from the sea.") == "3 land at Narvik from the sea: German troops, British troops and French troops");
		CHECK(Each({ "German airborne troops", "Italian airborne troops", "Japanese airborne troops" }, "take Crete from the air.")
			== "3 take Crete from the air: German airborne troops, Italian airborne troops and Japanese airborne troops");
		CHECK(Each({ "German envoys", "Italian envoys", "Japanese envoys" }, "are courting Buenos Aires.") == "3 are courting Buenos Aires: German envoys, Italian envoys and Japanese envoys");
		CHECK(Each({ "Germany", "Italy", "Japan" }, "rearms: more of its industry goes over to war work.") == "3 rearm: Germany, Italy and Japan");
		CHECK(Each({ "Argentina", "Chile", "Spain" }, "leans toward the Axis: trade and treaties, but not at war.") == "3 lean toward the Axis: Argentina, Chile and Spain");
		CHECK(Each({ "Hungary", "Romania", "Bulgaria" }, "lets its claim to Transylvania rest for now.") == "3 let their claim to Transylvania rest for now: Hungary, Romania and Bulgaria");
		CHECK(Each({ "Hungary", "Romania", "Finland" }, "joins the Axis and declares war on the Soviet Union.") == "Hungary joins the Axis and declares war on the Soviet Union; 2 more reports elsewhere in the war");
		CHECK(Each({ "Hood", "Repulse", "Barham" }, "loses her captain.") == "Hood loses her captain; 2 more reports elsewhere in the war");
		// (a name's own small word is the name's; but the verb is the first other word in small letters, so a run that does
		// not split there splits nowhere: never "3 her captain: Hood loses, Repulse sinks and Barham rams")
		CHECK(Each({ "Charles de Gaulle", "Bernard Montgomery", "Alan Brooke" }, "reports for duty.") == "3 report for duty: Charles de Gaulle, Bernard Montgomery and Alan Brooke");
		CHECK(Words({ { "", "Hood loses her captain." }, { "", "Repulse sinks her captain." }, { "", "Barham rams her captain." } }) == "Hood loses her captain; 2 more reports elsewhere in the war");
		CHECK(PluralVerb("rearms:") == "rearm:" && PluralVerb("declares,") == "declare," && PluralVerb("is;") == "are;");
		{
			FMonth Sky;
			for (const char* Place : { "The sky over Lodz", "The sky over Rzhev", "The sky over Novgorod" })
			{
				AddItem(Sky, ESection::Row, "LOST", -1, "ftr", Place, "", "", "Our fighters were driven from it.");
			}
			std::string Why;
			const std::string Said = FoldWords(Sky.Said, FAlikeRun{ 0, 3 });
			CHECK(Said == "3 LOST: the sky over Lodz, the sky over Rzhev and the sky over Novgorod" && SaysOnlyTheCard(Sky.Said, FAlikeRun{ 0, 3 }, Said, Why));
			// (a run that is not wholly on the card says nothing, and its end is weighed without overflowing)
			CHECK(FoldWords(Sky.Said, FAlikeRun{ INT_MAX, 2 }).empty() && FoldWords(Sky.Said, FAlikeRun{ 2, INT_MAX }).empty() && FoldWords(Sky.Said, FAlikeRun{ 1, 3 }).empty());
		}
		CHECK(PluralVerb("focuses") == "focus" && PluralVerb("echoes") == "echo" && PluralVerb("vetoes") == "veto" && PluralVerb("uses") == "use" && PluralVerb("causes") == "cause");
		CHECK(PluralVerb("declares") == "declare" && PluralVerb("carries") == "carry" && PluralVerb("dies") == "die" && PluralVerb("presses") == "press" && PluralVerb("reaches") == "reach");
		CHECK(PluralVerb("closes") == "close" && PluralVerb("seizes") == "seize" && PluralVerb("goes") == "go" && PluralVerb("tore") == "tore" && PluralVerb("WAR") == "WAR" && PluralVerb("pass") == "pass");
		// Over every folded seal of the real months: nothing the card does not say.
		const std::vector<FMonth> Months = ReadMonths();
		int Told = 0, Named = 0, Bad = 0;
		std::string Why, First;
		for (const FMonth& Month : Months)
		{
			FoldAlike(Month.Looks, Runs);
			for (const FAlikeRun& Run : Runs)
			{
				const std::string Said = FoldWords(Month.Said, Run);
				if (Run.Count == 1)
				{
					Bad += Said == EightWords(Month.Said[static_cast<size_t>(Run.First)].Line) ? 0 : 1;
					continue;
				}
				++Told;
				Named += StartsWith(Said, std::to_string(Run.Count) + " ") ? 1 : 0;
				if (!SaysOnlyTheCard(Month.Said, Run, Said, Why))
				{
					++Bad;
					First = First.empty() ? Said + " (" + Why + ")" : First;
				}
			}
		}
		CHECK(Bad == 0 && Told >= 40 && Named >= 10);
		if (Bad > 0)
		{
			std::printf("  first: %s\n", First.c_str());
		}
		std::printf("(the real months' %d folded seals: %d name their items, the rest give the first item's words)\n", Told, Named);
	}

	// 6. The tab: the date strip's paper between the tails, under the disc's foot (never on the wax, nor its figures),
	// inside the seal's box (a folded seal is the size of any other), its figures 9 px tall or more at 1441 by 878.
	{
		bool bUnder = true;
		for (int Figures = 1; Figures <= Look::CountMostFigures; ++Figures)
		{
			const FBox Tab = Look::TabOf(Figures), Ink = Look::FiguresOf(Figures);
			bUnder = bUnder && Tab.MinX >= 0.0 && Tab.MaxX <= Look::BoxW && Tab.MinY >= 0.0 && Tab.MaxY <= Look::BoxH;
			bUnder = bUnder && Tab.MinY > Look::MidY + Look::DiscR && Ink.MinY > Look::MidY + Look::DiscR;
			bUnder = bUnder && Ink.MinX >= Tab.MinX && Ink.MaxX <= Tab.MaxX && Ink.MinY >= Tab.MinY && Ink.MaxY <= Tab.MaxY;
			bUnder = bUnder && std::fabs((Tab.MinX + Tab.MaxX) * 0.5 - Look::MidX) < 1e-9;
			for (const Look::FRun& Run : Look::Runs)
			{
				const double Turn = Run.Degrees * 3.141592653589793 / 180.0;
				bUnder = bUnder && Look::MidY + Run.Out * std::sin(Turn) + Run.Radius < Tab.MinY;
			}
		}
		CHECK(bUnder);
		CHECK(Look::FiguresIn(9) == 1 && Look::FiguresIn(10) == 2 && Look::FiguresIn(99) == 2 && Look::FiguresIn(100) == 3);
		const double Small = 878.0 / 972.0, Full = 1080.0 / 972.0;
		CHECK(Look::FigurePx(Small) >= 9.0 && Look::FigurePx(Full) > Look::FigurePx(Small));
		// (the figures stand in the middle of the tab: the text is placed by its line's middle, FigureLift above theirs)
		CHECK(Look::FigureLift() > 0.0 && Look::FigureLift() < 0.1 * Look::CountEm());
		std::printf("(the count's figures %.2f px tall at 1441 by 878, %.2f at 1920 by 1080; the tab %.1f by %.1f units for one figure)\n", Look::FigurePx(Small), Look::FigurePx(Full),
			Look::TabOf(1).MaxX - Look::TabOf(1).MinX, Look::TabOf(1).MaxY - Look::TabOf(1).MinY);
	}
}

namespace
{
	/**
	 * The test's own list of the joining and small words a cut never ends on, kept apart from EdgeSeals' own: the rules'
	 * LITTLE (src/campaign/monthcard.ts, the card's own cut), and "her". A word of stops only, or a dash, counts too.
	 */
	bool IsSmall(std::string Word)
	{
		while (!Word.empty() && (Word.back() == ',' || Word.back() == ';' || Word.back() == ':' || Word.back() == '.'))
		{
			Word.pop_back();
		}
		if (Word.empty() || Word == "-" || Word == "--" || Word == "\xE2\x80\x93" || Word == "\xE2\x80\x94")
		{
			return true;
		}
		for (char& Letter : Word)
		{
			Letter = Letter >= 'A' && Letter <= 'Z' ? static_cast<char>(Letter - 'A' + 'a') : Letter;
		}
		for (const char* Small : { "a", "an", "the", "of", "at", "in", "on", "to", "with", "for", "from", "by", "and", "or", "but", "its", "their", "his", "is", "are", "was",
				 "were", "be", "been", "has", "have", "had", "can", "could", "will", "would", "may", "must", "not", "no", "it", "they", "he", "we", "that", "which",
				 "as", "if", "so", "than", "then", "when", "while", "before", "after", "until", "because", "about", "out", "up", "off", "into", "over", "under",
				 "near", "round", "against", "between", "who", "whose", "her" })
		{
			if (Word == Small)
			{
				return true;
			}
		}
		return false;
	}

	/** A line's words, split at spaces. */
	std::vector<std::string> WordsOf(const std::string& Line)
	{
		std::vector<std::string> Out;
		size_t At = 0;
		while (At < Line.size())
		{
			while (At < Line.size() && Line[At] == ' ')
			{
				++At;
			}
			const size_t Start = At;
			while (At < Line.size() && Line[At] != ' ')
			{
				++At;
			}
			if (At > Start)
			{
				Out.push_back(Line.substr(Start, At - Start));
			}
		}
		return Out;
	}
}

/**
 * The eight-word cut never ends on a joining or small word (the rules' LITTLE: at, in, on, of, to, by, for, from, with,
 * and, or, the, a, an, its, his, their, is, has, have, been, under, round...; and her), on a dash or on stops standing
 * apart, nor leaves a bracket open: it steps back to the last word that says something. A seal's own words and a
 * folded seal's first item's words alike ("The Baltic Fleet shells the Finnish troops at, and 2 more" was 0010's),
 * over the 48 real months, every cut's last word shown.
 */
static void TestCuts()
{
	// Each small word, in any case and with a stop after it, as the eighth word: the cut steps back to the seventh.
	bool bBack = true;
	for (const char* Small : { "at", "in", "on", "of", "to", "by", "for", "from", "with", "and", "or", "the", "a", "an", "its", "his", "her", "their", "is", "are", "was",
			 "were", "be", "been", "has", "have", "had", "as", "but", "into", "over", "under", "round", "against", "between", "because", "whose" })
	{
		const std::string Lower(Small);
		std::string Upper = Lower;
		Upper[0] = static_cast<char>(Upper[0] - 'a' + 'A');
		bBack = bBack && EightWords("one two three four five six seven " + Lower + " nine ten") == "one two three four five six seven";
		bBack = bBack && EightWords("one two three four five six seven " + Upper + " nine ten") == "one two three four five six seven";
		bBack = bBack && EightWords("one two three four five six seven " + Lower + ", nine ten") == "one two three four five six seven";
	}
	CHECK(bBack);
	// (a word that only starts or ends like one is a word that says something)
	CHECK(EightWords("one two three four five six seven another nine") == "one two three four five six seven another");
	CHECK(EightWords("one two three four five six seven theirs nine") == "one two three four five six seven theirs");
	CHECK(EightWords("one two three four five six seven Inn nine") == "one two three four five six seven Inn");
	CHECK(EightWords("one two three four five six seven forth nine") == "one two three four five six seven forth");
	// (several in a row, and a dash among them, all go; a stop the cut then ends on is dropped, as at any cut)
	CHECK(EightWords("one two three four five six of the nine ten") == "one two three four five six");
	CHECK(EightWords("one two three four five six \xE2\x80\x94 of nine ten") == "one two three four five six");
	CHECK(EightWords("one two three four five six, and the nine ten") == "one two three four five six");
	// (a line that is not cut keeps its own last word, whatever it is; nor does a line of nothing else lose its first)
	CHECK(EightWords("The siege is lifted at last and") == "The siege is lifted at last and");
	CHECK(EightWords("of the a an in on at to by", 8) == "of" && EightWords("a b c", 2) == "a b" && EightWords("a b c", 0) == "a");
	// (the review's: stops standing apart, a dash with a stop on it, and the space before them, are all stepped back
	// past; a bracket the cut would leave open goes back to before the word that opened it; one the line closes stays)
	CHECK(EightWords("The fleet moves on Malta and the ; convoy waits off Alexandria") == "The fleet moves on Malta");
	CHECK(EightWords("German troops close round Kiev \xE2\x80\x94, the city holds out against them") == "German troops close round Kiev");
	CHECK(EightWords("one two three four five six seven -; nine") == "one two three four five six seven");
	CHECK(EightWords(", Paris falls today", 1) == "Paris" && EightWords(" ; Paris falls") == "Paris falls" && EightWords(" ; , ").empty());
	CHECK(EightWords("Fifth Army: VI Corps (36th and 34th Infantry Divisions) loses 1,073 men to enemy ships") == "Fifth Army: VI Corps");
	CHECK(EightWords("VI Corps (two divisions) loses 1,073 men to enemy ships and aircraft") == "VI Corps (two divisions) loses 1,073 men");
	CHECK(EightWords("(one two three four five six seven eight nine") == "(one two three four five six seven eight");
	CHECK(EightWords("Charles de Gaulle reports for duty and is waiting for a command.") == "Charles de Gaulle reports for duty");
	CHECK(EightWords("Soviet Union NEWS: British and Soviet troops have entered Iran.") == "Soviet Union NEWS: British and Soviet troops");
	// 0010's own: elsewhere's first item, and a run of three of them. (The cuts TestWords holds that ended on a small
	// word, "troops on the", "leaves for", "the attack from", now end a word or two sooner.)
	const std::string Baltic = "The Baltic Fleet shells the Finnish troops at Karelia from the Gulf of Finland.";
	CHECK(EightWords(Baltic) == "The Baltic Fleet shells the Finnish troops");
	{
		// (a folded seal's first item of one clause and no more than 16 words is said whole, so that no verb loses its
		// object, "the Western Approaches Fleet sink"; a longer one, or one of more clauses, is cut as a seal's own words)
		std::vector<FSaid> Said;
		Said.push_back(PlainSaid(ESection::Elsewhere, Baltic));
		Said.push_back(PlainSaid(ESection::Elsewhere, "Hungary joins the Axis."));
		Said.push_back(PlainSaid(ESection::Elsewhere, "Romania joins the Axis."));
		CHECK(FoldWords(Said, FAlikeRun{ 0, 3 }) == "The Baltic Fleet shells the Finnish troops at Karelia from the Gulf of Finland; 2 more reports elsewhere in the war");
		Said[0] = PlainSaid(ESection::Elsewhere, "The Baltic Fleet shells the Finnish troops at Karelia from the Gulf of Finland and the Aaland Islands.");
		CHECK(FoldWords(Said, FAlikeRun{ 0, 3 }) == "The Baltic Fleet shells the Finnish troops; 2 more reports elsewhere in the war");
		Said[0] = PlainSaid(ESection::Elsewhere, "The escorts of the Western Approaches Fleet sink 1 German submarine in the Western Approaches.");
		CHECK(FoldWords(Said, FAlikeRun{ 0, 3 }) == "The escorts of the Western Approaches Fleet sink 1 German submarine in the Western Approaches; 2 more reports elsewhere in the war");
		// (where the others are in more than one part, each is named once, in card order; one more is one report)
		Said[1] = PlainSaid(ESection::Home, "Research complete: Radar.");
		Said[2] = PlainSaid(ESection::Staff, "Your staff on the fighting at Sedan.");
		CHECK(FoldWords(Said, FAlikeRun{ 0, 3 }) == "The escorts of the Western Approaches Fleet sink 1 German submarine in the Western Approaches; 2 more reports at home and from your staff");
		CHECK(FoldWords(Said, FAlikeRun{ 1, 2 }) == "Research complete: Radar; 1 more report from your staff");
	}

	// Over the 48 real months: every item's own words, and every folded seal's first item's words, cut or not; every
	// cut's last word is one that says something, and the cut is the line's own first words.
	const std::vector<FMonth> Months = ReadMonths();
	std::vector<FAlikeRun> Runs;
	std::vector<std::pair<std::string, int>> Last;
	int Items = 0, Cuts = 0, Bad = 0, Short = 0, FoldCuts = 0, FoldBad = 0;
	std::string FirstBad, FoldLast;
	for (const FMonth& Month : Months)
	{
		for (const FSaid& Said : Month.Said)
		{
			++Items;
			const std::string Cut = EightWords(Said.Line);
			const std::vector<std::string> Whole = WordsOf(Said.Line), Kept = WordsOf(Cut);
			if (Kept.size() >= Whole.size())
			{
				continue;
			}
			++Cuts;
			std::string End = Kept.empty() ? std::string() : Kept.back();
			while (!End.empty() && (End.back() == ',' || End.back() == ';' || End.back() == ':'))
			{
				End.pop_back();
			}
			const bool bOwn = Said.Line.compare(0, Cut.size(), Cut) == 0;
			if (Kept.empty() || IsSmall(End) || !bOwn)
			{
				++Bad;
				FirstBad = FirstBad.empty() ? Cut : FirstBad;
			}
			Short += Kept.size() < 4 ? 1 : 0;
			auto Found = std::find_if(Last.begin(), Last.end(), [&End](const std::pair<std::string, int>& Each) { return Each.first == End; });
			if (Found == Last.end())
			{
				Last.emplace_back(End, 1);
			}
			else
			{
				++Found->second;
			}
		}
		// (a folded seal that gives its first item's words: what stands before "; N more reports")
		FoldAlike(Month.Looks, Runs);
		for (const FAlikeRun& Run : Runs)
		{
			const std::string Words = FoldWords(Month.Said, Run);
			const size_t And = Words.rfind("; "); // (the last: the first item's own words may hold one)
			const FSaid& First = Month.Said[static_cast<size_t>(Run.First)];
			if (Run.Count < 2 || And == std::string::npos || WordsOf(EightWords(First.Line)).size() >= WordsOf(First.Line).size())
			{
				continue;
			}
			++FoldCuts;
			const std::vector<std::string> Kept = WordsOf(Words.substr(0, And));
			const std::string End = Kept.empty() ? std::string() : Kept.back();
			FoldBad += Kept.empty() || IsSmall(End) ? 1 : 0;
			FoldLast += (FoldLast.empty() ? "" : "; ") + Words.substr(0, And) + " [" + End + "]";
		}
	}
	CHECK(Months.size() == 48 && Cuts >= 40 && Bad == 0 && Short == 0);
	CHECK(FoldCuts >= 1 && FoldBad == 0);
	if (Bad > 0)
	{
		std::printf("  first: %s\n", FirstBad.c_str());
	}
	std::sort(Last.begin(), Last.end(), [](const std::pair<std::string, int>& A, const std::pair<std::string, int>& B) { return A.second != B.second ? A.second > B.second : A.first < B.first; });
	std::string Shown;
	for (const std::pair<std::string, int>& Each : Last)
	{
		Shown += (Shown.empty() ? "" : ", ") + Each.first + (Each.second > 1 ? " x" + std::to_string(Each.second) : std::string());
	}
	std::printf("(the real months' eight words: %d of %d items cut, %d ending on a small word; every cut's last word: %s)\n", Cuts, Items, Bad, Shown.c_str());
	std::printf("(the real months' folded seals that give their first item's words cut: %d, %d ending on a small word: %s)\n", FoldCuts, FoldBad, FoldLast.c_str());
}

/** The art director's look limits of 22 November, section 2, as EdgeSeals::Look measures the seal. */
static void TestSealLook()
{
	const FParams Params;
	CHECK(Look::BoxW == Params.SealW && Look::BoxH == Params.SealH);
	CHECK(Look::MidY + Look::DiscR <= Look::BoxH && Look::MidX + Look::DiscR <= Look::BoxW);
	// On the screen a frame's unit is FrameScale (1000 / 900) times Unreal's own scale for the window (its height over
	// 1080): 1.029 px at 1600 by 1000, where the seals are judged; 0.74 at 1280 by 720; twice that on the fine pixels.
	const double Reference = (1000.0 / 900.0) * (1000.0 / 1080.0);
	const double Scales[] = { (1000.0 / 900.0) * (600.0 / 1080.0), (1000.0 / 900.0) * (720.0 / 1080.0), (1000.0 / 900.0) * (878.0 / 1080.0), Reference, (1000.0 / 900.0) * (1440.0 / 1080.0), 2.0 * Reference, 3.0 };

	// The pressed sign: about 18 px across at the reference screen, an order sign fitted as it was drawn, inside the hollow.
	const FPoint Wide = Look::FitSign(192.0, 128.0);
	CHECK(std::fabs(Wide.X - Look::SignW) < 1e-9 && Wide.Y <= Look::SignH + 1e-9);
	CHECK(Wide.X * Reference > 17.0 && Wide.X * Reference < 19.0);
	const FPoint Square = Look::FitSign(64.0, 64.0);
	CHECK(Square.X <= Look::SignW + 1e-9 && Square.Y <= Look::SignH + 1e-9 && std::fabs(Square.X - Square.Y) < 1e-9);
	const FPoint Undrawn = Look::FitSign(0.0, 0.0);
	CHECK(std::fabs(Undrawn.X - Look::SignW) < 1e-9 && Undrawn.Y <= Look::SignH + 1e-9);
	// (corner to corner, with its wall, inside the hollow)
	CHECK(std::hypot(Look::SignW, Look::SignH) * 0.5 + Look::WallOf(Reference) <= Look::HollowR);

	// The drawn marks: about 18 px across at the reference screen (16 to 19), inside the hollow with their wall at
	// every scale, and no stroke under 1.5 px on the screen from a window 600 high to the fine pixels.
	const EGlyph Drawn[] = { EGlyph::Warning, EGlyph::War, EGlyph::Fallen, EGlyph::Freed, EGlyph::News, EGlyph::Globe };
	bool bAcross = true, bInside = true, bThick = true, bSome = true;
	for (const EGlyph Glyph : Drawn)
	{
		for (const double Scale : Scales)
		{
			Look::FStroke Strokes[Look::MarkMost];
			const int Count = Look::MarkOf(Glyph, Scale, Strokes);
			bSome = bSome && Count > 0 && Count <= Look::MarkMost;
			const FMarkBox Box = MarkBox(Strokes, Count);
			bInside = bInside && Box.Far + Look::WallOf(Scale) * std::sqrt(2.0) <= Look::HollowR + 1e-9;
			for (int Index = 0; Index < Count; ++Index)
			{
				const double Line = Strokes[Index].Kind == Look::EStroke::Dot ? Strokes[Index].Long : Strokes[Index].Thick;
				bThick = bThick && Line * Scale >= Look::MinStrokePx - 1e-9;
			}
			if (Scale == Reference)
			{
				const double Across = std::max(Box.MaxX - Box.MinX, Box.MaxY - Box.MinY) * Reference;
				bAcross = bAcross && Across >= 16.0 && Across <= 19.0;
			}
		}
	}
	CHECK(bSome);
	CHECK(bAcross);
	CHECK(bInside);
	CHECK(bThick);
	{
		Look::FStroke Strokes[4];
		CHECK(Look::MarkOf(EGlyph::Sign, Reference, Strokes) == 0 && Look::MarkOf(EGlyph::More, Reference, Strokes) == 0);
	}
	// The seal signs (the art director's ruling of 22 November, 06:45Z): one set, drawn for 1920 by 1080 and used on
	// every screen, a sign pressed only where its finest part (its thinnest stroke or its narrowest gap or counter: the
	// set draws every cap, gap and fillet 16 of 192) holds 1.5 px, else the seal takes its drawn mark (never a smudge,
	// never a gap filled in), and the whole set at once: no screen shows some seals' signs and others' three lines.
	// Brian's window on the Mac's fine pixels is about 1786 high: 1.84 px a unit; a 1080 window 1.11; 1600 by 1000, the
	// reference, 1.03; 1280 by 720, 0.74. (TestSealSigns measures the pictures themselves.)
	{
		const double Mac = (1000.0 / 900.0) * (1786.0 / 1080.0), Full = 1000.0 / 900.0, Small = (1000.0 / 900.0) * (720.0 / 1080.0);
		auto High = [](double Height) { return (1000.0 / 900.0) * (Height / 1080.0); };
		const double Finer[] = { Full, High(1200.0), High(1440.0), Mac, 2.0 * Mac };
		// (under the edge: a window 971 high; a 1600 by 1000 desktop window, 950 high inside its title bar; a 1440 by 900
		// one, about 855; 1280 by 720)
		const double Under[] = { High(971.0), High(950.0), High(855.0), Small };
		const char* const Pressed[] = { "aa", "arm", "art", "bmr", "cav", "cv", "eng", "ftr", "hull", "inf", "mot", "mtn", "para", "ss", "tp" };
		auto Holds = [](const char* Sign, double Scale) { return Look::SignHolds(Look::SignFinest(Sign), 192.0, 128.0, Scale); };
		bool bMeasured = true, bFinest = true, bFiner = true, bJudged = true, bTogether = true, bBelow = true;
		for (const char* Sign : Pressed)
		{
			// (both measures in the table, each at least 16 of 192 less half a pixel for the antialiasing)
			bMeasured = bMeasured && MayPress(Sign) && Look::SignThinnest(Sign) >= Look::SealFloor - 0.5 && Look::SignNarrowestGap(Sign) >= Look::SealFloor - 0.5;
			// (the finest part is the set's 16 for every sign, never the measure, whose corner allowance reads a cap or a
			// fillet 16 across as up to 23.5; and the pictures bear it out, to half a pixel)
			bFinest = bFinest && Look::SignFinest(Sign) == Look::SealFloor && Look::SealFloor <= std::min(Look::SignThinnest(Sign), Look::SignNarrowestGap(Sign)) + 0.5;
			// (every seal sign is pressed at 1920 by 1080 and on every finer screen, Brian's among them: no three lines there)
			for (const double Scale : Finer)
			{
				bFiner = bFiner && Holds(Sign, Scale);
			}
			// (and at 1600 by 1000, where the seals are judged, and from a window 973 high)
			bJudged = bJudged && Holds(Sign, Reference) && Holds(Sign, High(973.0));
			// (every sign at the same edge: 16 of 192 fitted 18 wide is 1.5 units, so a pixel a unit, a window 972 high)
			bTogether = bTogether && Holds(Sign, 1.0) && !Holds(Sign, 1.0 - 1e-9);
			// (below it the three lines show on every seal)
			for (const double Scale : Under)
			{
				bBelow = bBelow && !Holds(Sign, Scale);
			}
		}
		CHECK(Look::SealFloor == 16.0 && Look::SealFloor / Look::SignPictureW * Look::FitSign(192.0, 128.0).X == 1.5);
		CHECK(bMeasured);
		CHECK(bFinest);
		CHECK(bFiner);
		CHECK(bJudged);
		CHECK(bTogether);
		CHECK(bBelow);
		CHECK(std::fabs(High(972.0) - 1.0) < 1e-12);
		// (the review's cases: a 1440 by 900 window pressed art, eng, mot and ss on their corner measures, art's 16 px
		// notch at 1.32 px; a 1600 by 1000 window pressed bmr and ftr at 1.5004 px with their caps at 1.466. Neither now)
		CHECK(!Holds("art", High(855.0)) && !Holds("eng", High(855.0)) && !Holds("mot", High(855.0)) && !Holds("ss", High(855.0)));
		CHECK(!Holds("bmr", High(950.0)) && !Holds("ftr", High(950.0)));
		// (the same sign drawn twice as large keeps the same measure: it is a share of the picture's width)
		CHECK(Look::SignHolds(Look::SignFinest("inf"), 384.0, 256.0, Mac) == Holds("inf", Mac));
		CHECK(Look::SignHolds(Look::SignFinest("inf"), 384.0, 256.0, 1.0) && !Look::SignHolds(Look::SignFinest("inf"), 384.0, 256.0, 1.0 - 1e-9));
		// (no seal picture, no measure, no press: the star and the night and day raids' signs, which MayPress refuses,
		// and a sign unknown)
		CHECK(Look::SignFinest("star") == 0.0 && Look::SignFinest("night") == 0.0 && Look::SignFinest("day") == 0.0 && Look::SignFinest("") == 0.0);
		CHECK(Look::SignThinnest("star") == 0.0 && Look::SignNarrowestGap("night") == 0.0 && Look::SignThinnest("day") == 0.0 && Look::SignNarrowestGap("day") == 0.0 && Look::SignFinest("zz") == 0.0);
		CHECK(!Holds("star", 3.0) && !Holds("night", 3.0) && !Holds("day", 3.0) && !Holds("", 3.0) && !Look::SignHolds(0.0, 192.0, 128.0, 100.0));
		// (a seal signed "day" shows the drawn three lines on every screen, from 720 high to twice Brian's window, as one
		// signed "night" does: its word's mark, never the sun, and never a sign that the layer would then find too fine
		// (EdgeNewsLayer's PaintMark takes the three lines for a sign with no picture or one that does not hold). No row or
		// line of the card is signed "day" today: a raid's row is signed bmr, by day or by night, so a raid's seal presses
		// the bomber from 972 high, as every pressable sign is checked above. The refusal keeps the sun out of the wax if a
		// row is ever signed "day".)
		{
			const double Every[] = { 0.0, Small, High(855.0), High(950.0), High(971.0), 1.0, High(973.0), Reference, Full, High(1200.0), High(1440.0), Mac, 2.0 * Mac, 3.0 };
			bool bDayLines = true;
			for (const double Scale : Every)
			{
				for (const ESection Section : { ESection::Row, ESection::Home })
				{
					for (const char* Word : { "", "HIT" })
					{
						const EGlyph Glyph = GlyphOf(Section, Word, MayPress("day"));
						Look::FStroke Lines[4];
						const int Count = Look::MarkOf(Glyph, Scale, Lines);
						bDayLines = bDayLines && Glyph == EGlyph::News && GlyphOf(Section, Word, MayPress("night")) == Glyph && !Holds("day", Scale) && Count == 3;
						for (int Each = 0; Each < Count; ++Each)
						{
							bDayLines = bDayLines && Lines[Each].Kind == Look::EStroke::Bar && Lines[Each].Degrees == 0.0;
						}
					}
				}
			}
			CHECK(bDayLines);
		}
		// (a screen of no measure is taken at a pixel a unit)
		CHECK(Holds("art", 0.0) == Holds("art", 1.0) && Holds("inf", std::nan("")) == Holds("inf", 1.0));
	}

	// A stroke is kept as drawn where it is thick enough, made thicker where it is not, and never swells past 6 units.
	CHECK(Look::Stroke(2.0, Reference) == 2.0);
	CHECK(std::fabs(Look::Stroke(1.2, Reference) * Reference - 1.5) < 1e-9);
	CHECK(std::fabs(Look::Stroke(2.0, 0.5) - 3.0) < 1e-9);
	CHECK(Look::Stroke(1.0, 0.0) == 1.5 && Look::Stroke(1.0, std::nan("")) == 1.5 && Look::Stroke(1.0, 0.01) == 6.0);
	CHECK(Look::WallOf(Reference) <= 1.0 && Look::WallOf(0.5) == 1.0 && Look::WallOf(3.0) == 0.75 && Look::WallOf(0.0) == 1.0);

	// The hollow lit from the upper left: the ring that catches the light lies, inside the hollow, along its lower-right
	// wall; the ring in shadow along its upper-left wall (the patch lit it from the lower right).
	CHECK(Look::LightX < 0.0 && Look::LightY < 0.0);
	const Look::FLips Lips = Look::HollowLips();
	CHECK(Lips.LitX < 0.0 && Lips.LitY < 0.0 && Lips.ShadeX > 0.0 && Lips.ShadeY > 0.0);
	const double Diagonal = std::sqrt(0.5), Line = Lips.R - 0.5;
	CHECK(std::hypot(Lips.LitX + Line * Diagonal, Lips.LitY + Line * Diagonal) < Look::HollowR);
	CHECK(std::hypot(Lips.LitX - Line * Diagonal, Lips.LitY - Line * Diagonal) > Look::HollowR);
	CHECK(std::hypot(Lips.ShadeX - Line * Diagonal, Lips.ShadeY - Line * Diagonal) < Look::HollowR);
	CHECK(std::hypot(Lips.ShadeX + Line * Diagonal, Lips.ShadeY + Line * Diagonal) > Look::HollowR);
	CHECK(Look::HollowR < Look::DiscR - 2.0 * Look::RimW);

	// Four small, uneven runs of wax: none more than a quarter of the disc's radius, each out past the rim by a little
	// (0.5 to 1.5), and neither evenly spaced (a gear) nor alike (a badge's edge).
	CHECK(Look::RunCount == 4);
	bool bSmall = true;
	double LeastGap = 360.0, MostGap = 0.0, LeastR = 1e9, MostR = 0.0, LeastOut = 1e9, MostOut = 0.0;
	for (int Index = 0; Index < Look::RunCount; ++Index)
	{
		const Look::FRun& Run = Look::Runs[Index];
		const double Past = Run.Out + Run.Radius - Look::DiscR;
		bSmall = bSmall && Run.Radius <= 0.25 * Look::DiscR && Past >= 0.5 && Past <= 1.5 && Run.Out + Run.Radius <= Look::MidX;
		const double Next = Index + 1 < Look::RunCount ? Look::Runs[Index + 1].Degrees : Look::Runs[0].Degrees + 360.0;
		LeastGap = std::min(LeastGap, Next - Run.Degrees);
		MostGap = std::max(MostGap, Next - Run.Degrees);
		LeastR = std::min(LeastR, Run.Radius);
		MostR = std::max(MostR, Run.Radius);
		LeastOut = std::min(LeastOut, Past);
		MostOut = std::max(MostOut, Past);
	}
	CHECK(bSmall);
	CHECK(LeastGap > 0.0 && MostGap - LeastGap >= 30.0);
	CHECK(MostR - LeastR >= 0.8 && MostOut - LeastOut >= 0.2);

	// The ribbon's tails: short, neither longer than the disc is tall, and inside the seal's box.
	bool bTails = true;
	for (const Look::FTail& Tail : Look::Tails)
	{
		const double Turn = Tail.Degrees * 3.141592653589793 / 180.0, Reach = (Tail.Long - Tail.Thick) * 0.5;
		const double HalfX = Reach * std::fabs(std::cos(Turn)) + Tail.Thick * 0.5, HalfY = Reach * std::fabs(std::sin(Turn)) + Tail.Thick * 0.5;
		bTails = bTails && Tail.Long <= 2.0 * Look::DiscR && Tail.Y + HalfY <= Look::BoxH && Tail.X - HalfX >= 0.0 && Tail.X + HalfX <= Look::BoxW;
		// (and each comes out from under the disc)
		bTails = bTails && Tail.Y - HalfY < Look::MidY + Look::DiscR;
	}
	CHECK(bTails);

	// The call at 2.4 s: calm (a breath of a few hundredths), the ring no wider than twice the disc even under the
	// pointer's grow, never as opaque as the seal (it and the glow are drawn in the seal's own wax: never brighter),
	// fading to nothing at its widest.
	bool bNarrow = true, bDim = true, bCalm = true;
	for (int Step = 0; Step <= 2000; ++Step)
	{
		const Look::FCall Call = Look::CallOf(Step * 0.005);
		const double Grown = std::max(Call.Grow, Look::HoverGrow);
		bNarrow = bNarrow && (Call.RingR + Call.RingW * 0.5) * Grown <= 2.0 * Look::DiscR && Call.GlowR * Grown <= 2.0 * Look::DiscR && Call.RingR > Look::DiscR;
		bDim = bDim && Call.Ring >= 0.0 && Call.Ring <= 0.6 && Call.Glow >= 0.0 && Call.Glow <= 0.25 && Call.Ring + Call.Glow < 1.0;
		bCalm = bCalm && Call.Grow >= 1.0 && Call.Grow <= 1.0 + Look::Breath + 1e-12;
	}
	CHECK(bNarrow);
	CHECK(bDim);
	CHECK(bCalm && Look::Breath <= 0.05);
	CHECK(Look::CallOf(2.4 - 1e-6).Ring < 0.01 && std::fabs(Look::CallOf(1.2).Grow - (1.0 + Look::Breath)) < 1e-9 && Look::CallOf(0.0).Grow == 1.0);
	// (the ring rises from nothing at the rim, so that it never pops in, and never jumps, across the cycle's turn too)
	bool bSmooth = true;
	double Most = 0.0;
	for (int Step = 0; Step <= 2000; ++Step)
	{
		const double Now = Look::CallOf(Step * 0.005).Ring, Next = Look::CallOf((Step + 1) * 0.005).Ring;
		bSmooth = bSmooth && std::fabs(Next - Now) <= 0.02;
		Most = std::max(Most, Now);
	}
	CHECK(Look::CallOf(0.0).Ring == 0.0 && Look::CallOf(2.4).Ring < 1e-9 && Look::CallOf(4.8 + 1e-9).Ring < 1e-6);
	CHECK(bSmooth);
	CHECK(Most > 0.45 && Most <= 0.55);
}

namespace
{
	/**
	 * A small inflate (RFC 1951, after zlib's puff): stored, fixed and dynamic blocks, enough to read the seal signs'
	 * PNG data with no library. False for data it cannot read.
	 */
	class FInflate
	{
	public:
		FInflate(const std::vector<unsigned char>& InData, size_t InAt)
			: Data(InData), At(InAt)
		{
		}

		bool Run(std::vector<unsigned char>& Out)
		{
			bool bLast = false;
			while (!bLast && !bFailed)
			{
				bLast = Bits(1) == 1;
				const int Type = Bits(2);
				if (Type == 0)
				{
					Stored(Out);
				}
				else if (Type == 1)
				{
					FHuffman Lengths, Distances;
					short Each[288];
					for (int Symbol = 0; Symbol < 288; ++Symbol)
					{
						Each[Symbol] = static_cast<short>(Symbol < 144 ? 8 : Symbol < 256 ? 9 : Symbol < 280 ? 7 : 8);
					}
					Build(Lengths, Each, 288);
					for (int Symbol = 0; Symbol < 30; ++Symbol)
					{
						Each[Symbol] = 5;
					}
					Build(Distances, Each, 30);
					Codes(Out, Lengths, Distances);
				}
				else if (Type == 2)
				{
					Dynamic(Out);
				}
				else
				{
					bFailed = true;
				}
			}
			return !bFailed;
		}

	private:
		struct FHuffman
		{
			short Count[16] = {};
			short Symbol[288] = {};
		};

		int Bits(int Need)
		{
			long Value = BitBuf;
			while (BitCount < Need)
			{
				if (At >= Data.size())
				{
					bFailed = true;
					return 0;
				}
				Value |= static_cast<long>(Data[At++]) << BitCount;
				BitCount += 8;
			}
			BitBuf = Value >> Need;
			BitCount -= Need;
			return static_cast<int>(Value & ((1L << Need) - 1));
		}

		void Stored(std::vector<unsigned char>& Out)
		{
			BitBuf = 0;
			BitCount = 0;
			if (At + 4 > Data.size())
			{
				bFailed = true;
				return;
			}
			const unsigned Length = static_cast<unsigned>(Data[At]) | (static_cast<unsigned>(Data[At + 1]) << 8);
			const unsigned Check = static_cast<unsigned>(Data[At + 2]) | (static_cast<unsigned>(Data[At + 3]) << 8);
			At += 4;
			if (Length != (~Check & 0xFFFFu) || At + Length > Data.size())
			{
				bFailed = true;
				return;
			}
			Out.insert(Out.end(), Data.begin() + static_cast<long>(At), Data.begin() + static_cast<long>(At + Length));
			At += Length;
		}

		/** The canonical code's next symbol; -1 where the bits make none. */
		int Decode(const FHuffman& Table)
		{
			int Code = 0, First = 0, Index = 0;
			for (int Length = 1; Length < 16; ++Length)
			{
				Code |= Bits(1);
				const int Count = Table.Count[Length];
				if (Code - Count < First)
				{
					return Table.Symbol[Index + (Code - First)];
				}
				Index += Count;
				First = (First + Count) << 1;
				Code <<= 1;
			}
			return -1;
		}

		/** Builds a canonical code from its lengths: 0 for a complete code, more for an incomplete one, less for one over-full. */
		static int Build(FHuffman& Table, const short* Lengths, int Number)
		{
			for (short& Count : Table.Count)
			{
				Count = 0;
			}
			for (int Symbol = 0; Symbol < Number; ++Symbol)
			{
				++Table.Count[Lengths[Symbol]];
			}
			if (Table.Count[0] == Number)
			{
				return 0;
			}
			int Left = 1;
			for (int Length = 1; Length < 16; ++Length)
			{
				Left = (Left << 1) - Table.Count[Length];
				if (Left < 0)
				{
					return Left;
				}
			}
			short Offsets[16] = {};
			for (int Length = 1; Length < 15; ++Length)
			{
				Offsets[Length + 1] = static_cast<short>(Offsets[Length] + Table.Count[Length]);
			}
			for (int Symbol = 0; Symbol < Number; ++Symbol)
			{
				if (Lengths[Symbol] != 0)
				{
					Table.Symbol[Offsets[Lengths[Symbol]]++] = static_cast<short>(Symbol);
				}
			}
			return Left;
		}

		void Codes(std::vector<unsigned char>& Out, const FHuffman& Lengths, const FHuffman& Distances)
		{
			static constexpr short LengthBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
			static constexpr short LengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
			static constexpr short DistanceBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
			static constexpr short DistanceExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
			while (!bFailed)
			{
				int Symbol = Decode(Lengths);
				if (Symbol < 0)
				{
					bFailed = true;
				}
				else if (Symbol < 256)
				{
					Out.push_back(static_cast<unsigned char>(Symbol));
				}
				else if (Symbol == 256)
				{
					return;
				}
				else
				{
					Symbol -= 257;
					if (Symbol >= 29)
					{
						bFailed = true;
						return;
					}
					const int Length = LengthBase[Symbol] + Bits(LengthExtra[Symbol]);
					const int Far = Decode(Distances);
					if (Far < 0 || Far >= 30)
					{
						bFailed = true;
						return;
					}
					const size_t Back = static_cast<size_t>(DistanceBase[Far] + Bits(DistanceExtra[Far]));
					if (Back > Out.size())
					{
						bFailed = true;
						return;
					}
					for (int Each = 0; Each < Length; ++Each)
					{
						Out.push_back(Out[Out.size() - Back]);
					}
				}
			}
		}

		void Dynamic(std::vector<unsigned char>& Out)
		{
			static constexpr short Order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
			const int LengthCount = Bits(5) + 257, DistanceCount = Bits(5) + 1, CodeCount = Bits(4) + 4;
			if (LengthCount > 286 || DistanceCount > 30)
			{
				bFailed = true;
				return;
			}
			short Lengths[320] = {};
			for (int Index = 0; Index < CodeCount; ++Index)
			{
				Lengths[Order[Index]] = static_cast<short>(Bits(3));
			}
			FHuffman LengthCode, DistanceCode;
			if (Build(LengthCode, Lengths, 19) != 0)
			{
				bFailed = true;
				return;
			}
			int Index = 0;
			while (Index < LengthCount + DistanceCount && !bFailed)
			{
				int Symbol = Decode(LengthCode);
				if (Symbol < 0)
				{
					bFailed = true;
					return;
				}
				if (Symbol < 16)
				{
					Lengths[Index++] = static_cast<short>(Symbol);
					continue;
				}
				short Repeat = 0;
				if (Symbol == 16)
				{
					if (Index == 0)
					{
						bFailed = true;
						return;
					}
					Repeat = Lengths[Index - 1];
					Symbol = 3 + Bits(2);
				}
				else
				{
					Symbol = Symbol == 17 ? 3 + Bits(3) : 11 + Bits(7);
				}
				if (Index + Symbol > LengthCount + DistanceCount)
				{
					bFailed = true;
					return;
				}
				while (Symbol-- > 0)
				{
					Lengths[Index++] = Repeat;
				}
			}
			if (bFailed || Lengths[256] == 0)
			{
				bFailed = true;
				return;
			}
			const int LengthLeft = Build(LengthCode, Lengths, LengthCount);
			const int DistanceLeft = Build(DistanceCode, Lengths + LengthCount, DistanceCount);
			if (LengthLeft < 0 || (LengthLeft > 0 && LengthCount - LengthCode.Count[0] != 1) || DistanceLeft < 0
				|| (DistanceLeft > 0 && DistanceCount - DistanceCode.Count[0] != 1))
			{
				bFailed = true;
				return;
			}
			Codes(Out, LengthCode, DistanceCode);
		}

		const std::vector<unsigned char>& Data;
		size_t At = 0;
		long BitBuf = 0;
		int BitCount = 0;
		bool bFailed = false;
	};

	/** A picture as the PNG holds it: 8-bit RGBA, row by row. */
	struct FPicture
	{
		int W = 0;
		int H = 0;
		std::vector<unsigned char> Rgba;
	};

	unsigned ReadBig(const std::vector<unsigned char>& Bytes, size_t At)
	{
		return (static_cast<unsigned>(Bytes[At]) << 24) | (static_cast<unsigned>(Bytes[At + 1]) << 16) | (static_cast<unsigned>(Bytes[At + 2]) << 8) | Bytes[At + 3];
	}

	/** Reads an 8-bit RGBA PNG, not interlaced (as the seal signs are written); false for anything else. */
	bool ReadPng(const std::filesystem::path& Path, FPicture& Out)
	{
		std::FILE* File = std::fopen(Path.string().c_str(), "rb");
		if (!File)
		{
			return false;
		}
		std::vector<unsigned char> Bytes;
		unsigned char Block[4096];
		for (size_t Got; (Got = std::fread(Block, 1, sizeof(Block), File)) > 0;)
		{
			Bytes.insert(Bytes.end(), Block, Block + Got);
		}
		std::fclose(File);
		static constexpr unsigned char Signature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
		if (Bytes.size() < 8 || !std::equal(Signature, Signature + 8, Bytes.begin()))
		{
			return false;
		}
		std::vector<unsigned char> Packed;
		bool bHead = false;
		for (size_t At = 8; At + 12 <= Bytes.size();)
		{
			const size_t Length = ReadBig(Bytes, At);
			const std::string Type(Bytes.begin() + static_cast<long>(At + 4), Bytes.begin() + static_cast<long>(At + 8));
			if (At + 12 + Length > Bytes.size())
			{
				return false;
			}
			const size_t Body = At + 8;
			if (Type == "IHDR")
			{
				Out.W = static_cast<int>(ReadBig(Bytes, Body));
				Out.H = static_cast<int>(ReadBig(Bytes, Body + 4));
				// (8 bits, RGBA, the one compression and filter method, not interlaced)
				bHead = Length == 13 && Bytes[Body + 8] == 8 && Bytes[Body + 9] == 6 && Bytes[Body + 10] == 0 && Bytes[Body + 11] == 0 && Bytes[Body + 12] == 0
					&& Out.W > 0 && Out.H > 0 && Out.W <= 4096 && Out.H <= 4096;
			}
			else if (Type == "IDAT")
			{
				Packed.insert(Packed.end(), Bytes.begin() + static_cast<long>(Body), Bytes.begin() + static_cast<long>(Body + Length));
			}
			else if (Type == "IEND")
			{
				break;
			}
			At += 12 + Length;
		}
		// (the zlib wrapper: deflate, no preset dictionary, its check word right)
		if (!bHead || Packed.size() < 2 || (Packed[0] & 0x0F) != 8 || (Packed[1] & 0x20) != 0 || ((Packed[0] << 8) | Packed[1]) % 31 != 0)
		{
			return false;
		}
		std::vector<unsigned char> Raw;
		if (!FInflate(Packed, 2).Run(Raw))
		{
			return false;
		}
		const size_t Stride = static_cast<size_t>(Out.W) * 4;
		if (Raw.size() < (Stride + 1) * static_cast<size_t>(Out.H))
		{
			return false;
		}
		Out.Rgba.assign(Stride * static_cast<size_t>(Out.H), 0);
		for (int Row = 0; Row < Out.H; ++Row)
		{
			const unsigned char Filter = Raw[(Stride + 1) * static_cast<size_t>(Row)];
			const unsigned char* In = &Raw[(Stride + 1) * static_cast<size_t>(Row) + 1];
			unsigned char* Line = &Out.Rgba[Stride * static_cast<size_t>(Row)];
			const unsigned char* Above = Row > 0 ? Line - Stride : nullptr;
			for (size_t Each = 0; Each < Stride; ++Each)
			{
				const int Left = Each >= 4 ? Line[Each - 4] : 0, Up = Above ? Above[Each] : 0, Corner = Above && Each >= 4 ? Above[Each - 4] : 0;
				int Guess = 0;
				switch (Filter)
				{
				case 0:
					break;
				case 1:
					Guess = Left;
					break;
				case 2:
					Guess = Up;
					break;
				case 3:
					Guess = (Left + Up) / 2;
					break;
				case 4:
				{
					const int Base = Left + Up - Corner, ToLeft = std::abs(Base - Left), ToUp = std::abs(Base - Up), ToCorner = std::abs(Base - Corner);
					Guess = ToLeft <= ToUp && ToLeft <= ToCorner ? Left : ToUp <= ToCorner ? Up : Corner;
					break;
				}
				default:
					return false;
				}
				Line[Each] = static_cast<unsigned char>((In[Each] + Guess) & 0xFF);
			}
		}
		return true;
	}

	// The measure of a seal sign's picture: the same steps as Tools/edge_seals/seal_signs.py's measure(), all in whole
	// numbers, so that both find the same samples and the same widths.
	constexpr int MeasureSamples = 4; // samples a pixel, each way
	constexpr int MeasurePad = 32; // px of clear ground round the picture
	constexpr int MeasureMost = 512; // the widest measure, in eighths of a px (64 px)
	constexpr long long MeasureDepth2 = 16; // a sliver deeper than a pixel (4 samples) is a stroke or a gap, not the antialiasing
	constexpr long long MeasureFar = 1000000000000000LL;

	long long FloorDiv(long long Value, long long By)
	{
		return Value >= 0 ? Value / By : -((-Value + By - 1) / By);
	}

	/** The picture's ink at four samples a pixel each way, on its ground: bilinear between pixel middles, ink where half or more. */
	std::vector<unsigned char> InkSamples(const FPicture& Picture, int& OutW, int& OutH)
	{
		OutW = (Picture.W + 2 * MeasurePad) * MeasureSamples;
		OutH = (Picture.H + 2 * MeasurePad) * MeasureSamples;
		std::vector<unsigned char> Ink(static_cast<size_t>(OutW) * static_cast<size_t>(OutH), 0);
		auto Alpha = [&Picture](long long X, long long Y)
		{
			const long long CX = std::min<long long>(std::max<long long>(X, 0), Picture.W - 1), CY = std::min<long long>(std::max<long long>(Y, 0), Picture.H - 1);
			return static_cast<long long>(Picture.Rgba[(static_cast<size_t>(CY) * static_cast<size_t>(Picture.W) + static_cast<size_t>(CX)) * 4 + 3]);
		};
		for (int J = 0; J < Picture.H * MeasureSamples; ++J)
		{
			const long long V8 = 2LL * J - 3, Y0 = FloorDiv(V8, 8), WY1 = V8 - 8 * Y0, WY0 = 8 - WY1;
			for (int K = 0; K < Picture.W * MeasureSamples; ++K)
			{
				const long long U8 = 2LL * K - 3, X0 = FloorDiv(U8, 8), WX1 = U8 - 8 * X0, WX0 = 8 - WX1;
				const long long Sum = (Alpha(X0, Y0) * WX0 + Alpha(X0 + 1, Y0) * WX1) * WY0 + (Alpha(X0, Y0 + 1) * WX0 + Alpha(X0 + 1, Y0 + 1) * WX1) * WY1;
				Ink[static_cast<size_t>(J + MeasurePad * MeasureSamples) * static_cast<size_t>(OutW) + static_cast<size_t>(K + MeasurePad * MeasureSamples)] = 2 * Sum >= 16320 ? 1 : 0;
			}
		}
		return Ink;
	}

	/** Each sample's squared distance to the nearest sample of Feature, in samples (exact: Felzenszwalb and Huttenlocher); MeasureFar where there is none. */
	void SquaredDistance(const std::vector<unsigned char>& Feature, int GW, int GH, std::vector<long long>& Out)
	{
		Out.assign(Feature.size(), MeasureFar);
		std::vector<long long> Across(Feature.size(), MeasureFar);
		for (int Y = 0; Y < GH; ++Y)
		{
			const size_t Row = static_cast<size_t>(Y) * static_cast<size_t>(GW);
			long long Last = -MeasureFar;
			for (int X = 0; X < GW; ++X)
			{
				Last = Feature[Row + static_cast<size_t>(X)] ? X : Last;
				Across[Row + static_cast<size_t>(X)] = Last > -MeasureFar ? X - Last : MeasureFar;
			}
			Last = MeasureFar;
			for (int X = GW - 1; X >= 0; --X)
			{
				Last = Feature[Row + static_cast<size_t>(X)] ? X : Last;
				Across[Row + static_cast<size_t>(X)] = std::min(Across[Row + static_cast<size_t>(X)], Last < MeasureFar ? Last - X : MeasureFar);
			}
		}
		std::vector<long long> Squared(static_cast<size_t>(GH)), Where(static_cast<size_t>(GH));
		std::vector<double> Bound(static_cast<size_t>(GH) + 1);
		for (int X = 0; X < GW; ++X)
		{
			int Top = -1;
			for (int Y = 0; Y < GH; ++Y)
			{
				const long long G = Across[static_cast<size_t>(Y) * static_cast<size_t>(GW) + static_cast<size_t>(X)];
				if (G >= MeasureFar)
				{
					continue;
				}
				const long long F = G * G;
				double Meet = 0.0;
				while (Top >= 0)
				{
					const long long P = Where[static_cast<size_t>(Top)];
					Meet = static_cast<double>((F + 1LL * Y * Y) - (Squared[static_cast<size_t>(Top)] + P * P)) / (2.0 * static_cast<double>(Y - P));
					if (Meet > Bound[static_cast<size_t>(Top)])
					{
						break;
					}
					--Top;
				}
				++Top;
				Where[static_cast<size_t>(Top)] = Y;
				Squared[static_cast<size_t>(Top)] = F;
				Bound[static_cast<size_t>(Top)] = Top == 0 ? -1e300 : Meet;
			}
			if (Top < 0)
			{
				continue;
			}
			int Near = 0;
			for (int Y = 0; Y < GH; ++Y)
			{
				while (Near < Top && Bound[static_cast<size_t>(Near) + 1] < Y)
				{
					++Near;
				}
				const long long Off = Y - Where[static_cast<size_t>(Near)];
				Out[static_cast<size_t>(Y) * static_cast<size_t>(GW) + static_cast<size_t>(X)] = Off * Off + Squared[static_cast<size_t>(Near)];
			}
		}
	}

	/**
	 * A seal sign's thinnest stroke (bGap false) or narrowest gap or counter (bGap true), in px of its picture: the widest
	 * T, in eighths of a px up to 64, for which opening the ink (or the clear ground) with a disc T across takes nothing
	 * deeper than a pixel. A stroke narrower goes whole; a round cap or a fillet T across, or an antialiased edge, loses
	 * a sliver at most.
	 */
	double MeasureSign(const FPicture& Picture, bool bGap)
	{
		int GW = 0, GH = 0;
		const std::vector<unsigned char> Ink = InkSamples(Picture, GW, GH);
		std::vector<unsigned char> Part(Ink.size()), Other(Ink.size()), Core(Ink.size());
		for (size_t Each = 0; Each < Ink.size(); ++Each)
		{
			Part[Each] = static_cast<unsigned char>(bGap ? !Ink[Each] : Ink[Each]);
			Other[Each] = static_cast<unsigned char>(!Part[Each]);
		}
		std::vector<long long> Depth2, Reach2;
		SquaredDistance(Other, GW, GH, Depth2);
		auto Holds = [&](long long N)
		{
			if (N == 0)
			{
				return true;
			}
			for (size_t Each = 0; Each < Part.size(); ++Each)
			{
				Core[Each] = static_cast<unsigned char>(Part[Each] && 16 * Depth2[Each] >= N * N);
			}
			SquaredDistance(Core, GW, GH, Reach2);
			for (size_t Each = 0; Each < Part.size(); ++Each)
			{
				if (Part[Each] && !(16 * Reach2[Each] <= N * N) && Depth2[Each] > MeasureDepth2)
				{
					return false;
				}
			}
			return true;
		};
		if (Holds(MeasureMost))
		{
			return MeasureMost / 8.0;
		}
		long long Low = 0, High = MeasureMost;
		while (High - Low > 1)
		{
			const long long Mid = (Low + High) / 2;
			(Holds(Mid) ? Low : High) = Mid;
		}
		return static_cast<double>(Low) / 8.0;
	}

	/** The picture's alpha, row by row, as CRC-32 (zlib's, the PNG one): seal_signs.py's alpha_sum(). */
	unsigned AlphaSum(const FPicture& Picture)
	{
		unsigned Crc = 0xFFFFFFFFu;
		for (size_t Each = 3; Each < Picture.Rgba.size(); Each += 4)
		{
			Crc ^= static_cast<unsigned>(Picture.Rgba[Each]);
			for (int Bit = 0; Bit < 8; ++Bit)
			{
				Crc = (Crc >> 1) ^ (0xEDB88320u & (0u - (Crc & 1u)));
			}
		}
		return ~Crc;
	}

	struct FSealSum
	{
		const char* Sign;
		unsigned Sum;
	};

	// BEGIN seal_signs.py sums (written by Tools/edge_seals/seal_signs.py --write-table: each seal picture's alpha, row
	// by row, as CRC-32; TestSealSigns and --check hold every picture to its sum, so a picture changes only with its drawing)
	constexpr FSealSum SealSums[] = {
		{ "aa", 0xab49af22u },
		{ "arm", 0xc5652dbfu },
		{ "art", 0x73d7537du },
		{ "bmr", 0x6066b31eu },
		{ "cav", 0xa4a923e0u },
		{ "cv", 0x47cb26efu },
		{ "eng", 0xcb08da77u },
		{ "ftr", 0x1d65e56au },
		{ "hull", 0x7d9629dfu },
		{ "inf", 0x9ea351b6u },
		{ "mot", 0xb7bdc88bu },
		{ "mtn", 0xffff5b6du },
		{ "para", 0x9fc48530u },
		{ "ss", 0xc7e37753u },
		{ "tp", 0x242e8c04u },
	};
	// END seal_signs.py sums

	/** Whether a pixel of the picture is ink: its alpha half or more. */
	bool Inked(const FPicture& Picture, int X, int Y)
	{
		return Picture.Rgba[(static_cast<size_t>(Y) * static_cast<size_t>(Picture.W) + static_cast<size_t>(X)) * 4 + 3] >= 128;
	}

	/** The runs of ink along a row of the picture (bColumn false) or down a column (true), each its first and last pixel. */
	std::vector<std::pair<int, int>> InkRuns(const FPicture& Picture, int At, bool bColumn)
	{
		std::vector<std::pair<int, int>> Runs;
		const int Length = bColumn ? Picture.H : Picture.W;
		auto Ink = [&](int Along) { return bColumn ? Inked(Picture, At, Along) : Inked(Picture, Along, At); };
		for (int Along = 0; Along < Length;)
		{
			if (!Ink(Along))
			{
				++Along;
				continue;
			}
			const int First = Along;
			while (Along < Length && Ink(Along))
			{
				++Along;
			}
			Runs.emplace_back(First, Along - 1);
		}
		return Runs;
	}

	// The shapes the art director's calls of 22 November, 08:18Z chose, read from the 192 by 128 pictures, so that a
	// picture of 0008's drawing fails even where it measures as the new one does (0008's aa and tp measure within the
	// table's 0.25 of the new ones).

	/**
	 * aa, "mount dropped": a gun on a stand. The stand joins the barrel to the base (ink down the column at x 86 from y 56
	 * to 96), and nothing stands right of it where 0008's merged mount, the wedge that read as a ramp, reached (x 112 to
	 * 123, y 60 to 88: 330 pixels of ink in 0008's picture, none in this one).
	 */
	bool AaOnStand(const FPicture& Picture)
	{
		bool bStand = true, bClear = true;
		for (int Y = 56; Y <= 96; ++Y)
		{
			bStand = bStand && Inked(Picture, 86, Y);
		}
		for (int Y = 60; Y <= 88; ++Y)
		{
			for (int X = 112; X <= 123; ++X)
			{
				bClear = bClear && !Inked(Picture, X, Y);
			}
		}
		return bStand && bClear;
	}

	/**
	 * arm, "gun lifted": the barrel stands clear over a notch. At least 16 columns (16 of 192) in which a clear run 16 px
	 * tall or more has ink above it (the barrel) and below it (the hull): 35 in this picture, x 141 to 175; 3 in 0008's,
	 * whose gun was a slab over a dent.
	 */
	bool ArmNotch(const FPicture& Picture)
	{
		int Columns = 0;
		for (int X = 0; X < Picture.W; ++X)
		{
			const std::vector<std::pair<int, int>> Runs = InkRuns(Picture, X, true);
			bool bClear = false;
			for (size_t Each = 1; Each < Runs.size(); ++Each)
			{
				bClear = bClear || Runs[Each].first - Runs[Each - 1].second - 1 >= 16;
			}
			Columns += bClear ? 1 : 0;
		}
		return Columns >= 16;
	}

	/**
	 * tp, "funnels apart": two funnels. Every row through them (y 28 to 44, above the deckhouse) crosses two runs of ink,
	 * each 16 px wide or more, with 16 px or more of clear between them (x 44 to 76 and 119 to 150 in this picture; 0008's
	 * had the one funnel).
	 */
	bool TpFunnels(const FPicture& Picture)
	{
		bool bTwo = true;
		for (int Y = 28; Y <= 44; ++Y)
		{
			const std::vector<std::pair<int, int>> Runs = InkRuns(Picture, Y, false);
			bTwo = bTwo && Runs.size() == 2 && Runs[0].second - Runs[0].first + 1 >= 16 && Runs[1].second - Runs[1].first + 1 >= 16
				&& Runs[1].first - Runs[0].second - 1 >= 16;
		}
		return bTwo;
	}
}

/**
 * The seal set (the art director's ruling of 22 November, 06:45Z), measured from its pictures: every order sign in
 * Content/CityOrders that may be pressed has its seal picture in Content/CityOrders/Seal (192 by 128, white on clear,
 * so the layer's tint still works), its thinnest stroke and narrowest gap or counter measured here no thinner than
 * 16 of its 192 pixels (15.5 allowed: the measure reads the edge at half its alpha, so the antialiasing can take a
 * quarter to half a pixel), and as EdgeSeals' table has them; the star and the night and day raids' signs have none.
 * Each picture is held to its sum (SealSums, its alpha as CRC-32, written by seal_signs.py with the table), so a
 * picture can change only with the drawing that made it. And the shapes of the art director's calls of 22 November,
 * 08:18Z are read from the pictures: aa a gun on a stand, arm's barrel clear over a notch, tp's two funnels apart.
 */
static void TestSealSigns(const char* Folder)
{
	if (!Folder)
	{
		std::printf("(the order signs' folder was not given: the seal pictures were not measured)\n");
		return;
	}
	const std::filesystem::path Orders(Folder), Seals = Orders / "Seal";
	int Found = 0, Pictures = 0, Summed = 0;
	bool bEach = true, bRead = true, bWhite = true, bFloor = true, bTable = true, bRefused = true, bStray = true, bSums = true;
	bool bAaStand = false, bArmNotch = false, bTpFunnels = false;
	std::error_code Failed;
	std::vector<std::string> Names;
	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Orders, Failed))
	{
		Names.push_back(Entry.path().filename().string());
	}
	std::sort(Names.begin(), Names.end());
	for (const std::string& Name : Names)
	{
		if (Name.size() <= 10 || Name.compare(0, 6, "order-") != 0 || Name.compare(Name.size() - 4, 4, ".png") != 0)
		{
			continue;
		}
		const std::string Sign = Name.substr(6, Name.size() - 10);
		const std::filesystem::path Seal = Seals / ("seal-" + Sign + ".png");
		++Found;
		if (!MayPress(Sign))
		{
			// (no emblem in the wax: no picture, no measure)
			bRefused = bRefused && !std::filesystem::exists(Seal) && Look::SignFinest(Sign) == 0.0;
			continue;
		}
		FPicture Picture;
		if (!std::filesystem::exists(Seal))
		{
			bEach = false;
			std::printf("no seal picture: %s\n", Sign.c_str());
			continue;
		}
		if (!ReadPng(Seal, Picture) || Picture.W != 192 || Picture.H != 128)
		{
			bRead = false;
			std::printf("not read as 192 by 128 RGBA: %s\n", Seal.string().c_str());
			continue;
		}
		++Pictures;
		// (the picture is the one its drawing made: its alpha's sum as seal_signs.py wrote it)
		const unsigned Sum = AlphaSum(Picture);
		const FSealSum* Held = nullptr;
		for (const FSealSum& Each : SealSums)
		{
			Held = Sign == Each.Sign ? &Each : Held;
		}
		if (Held && Held->Sum == Sum)
		{
			++Summed;
		}
		else
		{
			bSums = false;
			std::printf("the picture is not the one drawn: %s sums %08x, SealSums has %08x\n", Sign.c_str(), Sum, Held ? Held->Sum : 0u);
		}
		// (the calls' shapes)
		bAaStand = Sign == "aa" ? AaOnStand(Picture) : bAaStand;
		bArmNotch = Sign == "arm" ? ArmNotch(Picture) : bArmNotch;
		bTpFunnels = Sign == "tp" ? TpFunnels(Picture) : bTpFunnels;
		for (size_t Each = 0; Each < Picture.Rgba.size(); Each += 4)
		{
			bWhite = bWhite && Picture.Rgba[Each] == 255 && Picture.Rgba[Each + 1] == 255 && Picture.Rgba[Each + 2] == 255;
		}
		const double Floor = Look::SealFloor / Look::SignPictureW * Picture.W - 0.5;
		const double Stroke = MeasureSign(Picture, false), Gap = MeasureSign(Picture, true);
		const double TableStroke = Look::SignThinnest(Sign), TableGap = Look::SignNarrowestGap(Sign);
		std::printf("seal %-4s stroke %6.3f (table %6.3f)  gap %6.3f (table %6.3f)\n", Sign.c_str(), Stroke, TableStroke, Gap, TableGap);
		if (Stroke < Floor || Gap < Floor)
		{
			bFloor = false;
			std::printf("under 16 of 192: %s\n", Sign.c_str());
		}
		if (std::fabs(Stroke - TableStroke) > 0.25 || std::fabs(Gap - TableGap) > 0.25)
		{
			bTable = false;
			std::printf("the table does not match the picture: %s\n", Sign.c_str());
		}
	}
	// (and no picture in the seal set that no sign presses)
	std::error_code SealsFailed;
	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Seals, SealsFailed))
	{
		const std::string Name = Entry.path().filename().string();
		const bool bSeal = Name.size() > 9 && Name.compare(0, 5, "seal-") == 0 && Name.compare(Name.size() - 4, 4, ".png") == 0;
		const std::string Sign = bSeal ? Name.substr(5, Name.size() - 9) : std::string();
		bStray = bStray && bSeal && MayPress(Sign) && std::filesystem::exists(Orders / ("order-" + Sign + ".png"));
	}
	std::printf("(the set is pressed on its finest part, %g of 192: %.2f px at 1920 by 1080, %.2f in Brian's window; from a window 972 high)\n", Look::SealFloor,
		Look::SealFloor * Look::FitSign(192.0, 128.0).X / Look::SignPictureW * (1000.0 / 900.0), Look::SealFloor * Look::FitSign(192.0, 128.0).X / Look::SignPictureW * (1000.0 / 900.0) * (1786.0 / 1080.0));
	CHECK(!Failed && !SealsFailed && Found >= 18 && Pictures == 15);
	CHECK(bEach);
	CHECK(bRead);
	CHECK(bWhite);
	CHECK(bFloor);
	CHECK(bTable);
	CHECK(bSums && Summed == 15 && std::size(SealSums) == 15u);
	// (aa a gun on a stand, arm's barrel clear over a notch, tp's two funnels apart)
	CHECK(bAaStand);
	CHECK(bArmNotch);
	CHECK(bTpFunnels);
	CHECK(bRefused && !std::filesystem::exists(Seals / "seal-star.png") && !std::filesystem::exists(Seals / "seal-night.png") && !std::filesystem::exists(Seals / "seal-day.png"));
	CHECK(bStray);
}

int main(int ArgCount, char** Args)
{
	TestItems();
	TestWords();
	TestLanes();
	TestLayout();
	TestRanked();
	TestMotion();
	TestColours();
	TestSealLook();
	TestSealSigns(ArgCount > 1 ? Args[1] : nullptr);
	TestUnderFrame();
	TestSealAsk();
	TestSwords();
	TestFold();
	TestCuts();
	std::printf("edge seals: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
