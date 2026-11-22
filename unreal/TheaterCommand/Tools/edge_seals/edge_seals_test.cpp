// The news seals' cloud test (release 2, campaign map item 5): Campaign/EdgeSeals, the game's own code, with no engine.
// Run by Tools/edge_seals/run.sh.

#include "Campaign/EdgeSeals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <random>
#include <string>
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

void* operator new(std::size_t Size)
{
	++Allocations;
	if (void* Block = std::malloc(Size == 0 ? 1 : Size))
	{
		return Block;
	}
	throw std::bad_alloc();
}

void operator delete(void* Block) noexcept
{
	std::free(Block);
}

void operator delete(void* Block, std::size_t) noexcept
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
	CHECK(EightWords(RowLine("Coast watch", "WARNING", "", "", "Italy masses troops on the Yugoslav border.")) == "Coast watch WARNING: Italy masses troops on the");
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
	CHECK(EightWords("Paris falls, and the French government leaves for Bordeaux tonight") == "Paris falls, and the French government leaves for");
	CHECK(EightWords("The siege of Leningrad tightens, and the last road across the lake is shelled") == "The siege of Leningrad tightens");
	CHECK(EightWords("one two three four five six seven, eight nine") == "one two three four five six seven");
	CHECK(EightWords("one, two three four five six seven eight nine") == "one, two three four five six seven eight");
	CHECK(EightWords("one two three four five six seven \xE2\x80\x94 eight nine") == "one two three four five six seven");
	CHECK(EightWords("Wroc\xC5\x82" "aw and \xC5\x81\xC3\xB3" "d\xC5\xBA hold against the attack from the west today") == "Wroc\xC5\x82" "aw and \xC5\x81\xC3\xB3" "d\xC5\xBA hold against the attack from");
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
	// Our own order signs are pressed in; no star (the ruling's section 2), nor the night raid's moon and star.
	CHECK(MayPress("inf") && MayPress("hull") && MayPress("ftr") && MayPress("art") && MayPress("aa"));
	CHECK(!MayPress("star") && !MayPress("night") && !MayPress(""));
	CHECK(GlyphOf(ESection::Row, "", MayPress("star")) == EGlyph::News);
	CHECK(GlyphOf(ESection::Home, "", MayPress("star")) == EGlyph::News);
	CHECK(GlyphOf(ESection::Row, "WON", MayPress("inf")) == EGlyph::Sign);
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
			Look::FStroke Strokes[4];
			const int Count = Look::MarkOf(Glyph, Scale, Strokes);
			bSome = bSome && Count > 0 && Count <= 4;
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
	// every screen, a sign pressed only where its finest part (its thinnest stroke or its narrowest gap or counter, the
	// less) holds 1.5 px, else the seal takes its drawn mark (never a smudge, never a gap filled in). Brian's window on the
	// Mac's fine pixels is about 1786 high: 1.84 px a unit; a 1080 window 1.11; 1600 by 1000, the reference, 1.03; 1280
	// by 720, 0.74. (TestSealSigns measures the pictures themselves.)
	{
		const double Mac = (1000.0 / 900.0) * (1786.0 / 1080.0), Full = 1000.0 / 900.0, Small = (1000.0 / 900.0) * (720.0 / 1080.0);
		const double FromHere = (1000.0 / 900.0) * (960.0 / 1080.0);
		const double Finer[] = { Full, (1000.0 / 900.0) * (1200.0 / 1080.0), (1000.0 / 900.0) * (1440.0 / 1080.0), Mac, 2.0 * Mac };
		const char* const Pressed[] = { "aa", "arm", "art", "bmr", "cav", "cv", "day", "eng", "ftr", "hull", "inf", "mot", "mtn", "para", "ss", "tp" };
		auto Holds = [](const char* Sign, double Scale) { return Look::SignHolds(Look::SignFinest(Sign), 192.0, 128.0, Scale); };
		bool bMeasured = true, bFinest = true, bFiner = true, bJudged = true, bBelow = true;
		for (const char* Sign : Pressed)
		{
			// (both measures in the table, each at least 16 of 192 less half a pixel for the antialiasing; the finest the less)
			bMeasured = bMeasured && MayPress(Sign) && Look::SignThinnest(Sign) >= 15.5 && Look::SignNarrowestGap(Sign) >= 15.5;
			bFinest = bFinest && Look::SignFinest(Sign) == std::min(Look::SignThinnest(Sign), Look::SignNarrowestGap(Sign));
			// (every seal sign is pressed at 1920 by 1080 and on every finer screen, Brian's among them: no three lines there)
			for (const double Scale : Finer)
			{
				bFiner = bFiner && Holds(Sign, Scale);
			}
			// (and at 1600 by 1000, where the seals are judged, and from a window 960 high)
			bJudged = bJudged && Holds(Sign, Reference) && Holds(Sign, FromHere);
			// (below 1080p the three lines do show: at 1280 by 720 every seal takes them)
			bBelow = bBelow && !Holds(Sign, Small);
		}
		CHECK(bMeasured);
		CHECK(bFinest);
		CHECK(bFiner);
		CHECK(bJudged);
		CHECK(bBelow);
		// (the measure is the finest part at the screen's scale, at the edge of 1.5 px)
		const double Edge = 1.5 / (Look::SignFinest("inf") * Look::FitSign(192.0, 128.0).X / Look::SignPictureW);
		CHECK(Holds("inf", Edge * 1.0001) && !Holds("inf", Edge * 0.9999));
		// (a gap counts as a stroke does: the parachute's counter is its finest part, and where its cords would hold but
		// the counter would fill in, it is not pressed)
		CHECK(Look::SignNarrowestGap("para") < Look::SignThinnest("para"));
		const double Between = 1.5 / (0.5 * (Look::SignNarrowestGap("para") + Look::SignThinnest("para")) * Look::FitSign(192.0, 128.0).X / Look::SignPictureW);
		CHECK(Look::SignHolds(Look::SignThinnest("para"), 192.0, 128.0, Between) && !Holds("para", Between));
		// (the same sign drawn twice as large keeps the same measure: it is a share of the picture's width)
		CHECK(Look::SignHolds(Look::SignFinest("inf"), 384.0, 256.0, Mac) == Holds("inf", Mac));
		// (no seal picture, no measure, no press: the star and the night raid's, which MayPress refuses, and a sign unknown)
		CHECK(Look::SignFinest("star") == 0.0 && Look::SignFinest("night") == 0.0 && Look::SignFinest("") == 0.0);
		CHECK(Look::SignThinnest("star") == 0.0 && Look::SignNarrowestGap("night") == 0.0 && Look::SignFinest("zz") == 0.0);
		CHECK(!Holds("star", 3.0) && !Holds("night", 3.0) && !Holds("", 3.0) && !Look::SignHolds(0.0, 192.0, 128.0, 100.0));
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
}

/**
 * The seal set (the art director's ruling of 22 November, 06:45Z), measured from its pictures: every order sign in
 * Content/CityOrders that may be pressed has its seal picture in Content/CityOrders/Seal (192 by 128, white on clear,
 * so the layer's tint still works), its thinnest stroke and narrowest gap or counter measured here no thinner than
 * 16 of its 192 pixels (15.5 allowed: the measure reads the edge at half its alpha, so the antialiasing can take a
 * quarter to half a pixel), and as EdgeSeals' table has them; the star and the night raid's sign have none.
 */
static void TestSealSigns(const char* Folder)
{
	if (!Folder)
	{
		std::printf("(the order signs' folder was not given: the seal pictures were not measured)\n");
		return;
	}
	const std::filesystem::path Orders(Folder), Seals = Orders / "Seal";
	int Found = 0, Pictures = 0;
	bool bEach = true, bRead = true, bWhite = true, bFloor = true, bTable = true, bRefused = true, bStray = true;
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
		for (size_t Each = 0; Each < Picture.Rgba.size(); Each += 4)
		{
			bWhite = bWhite && Picture.Rgba[Each] == 255 && Picture.Rgba[Each + 1] == 255 && Picture.Rgba[Each + 2] == 255;
		}
		const double Floor = 16.0 / 192.0 * Picture.W - 0.5;
		const double Stroke = MeasureSign(Picture, false), Gap = MeasureSign(Picture, true);
		const double TableStroke = Look::SignThinnest(Sign), TableGap = Look::SignNarrowestGap(Sign);
		std::printf("seal %-4s stroke %6.3f (table %6.3f)  gap %6.3f (table %6.3f)  finest %.2f px at 1920 by 1080\n", Sign.c_str(), Stroke, TableStroke, Gap, TableGap,
			std::min(Stroke, Gap) * Look::FitSign(Picture.W, Picture.H).X / Look::SignPictureW * (1000.0 / 900.0));
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
	CHECK(!Failed && !SealsFailed && Found >= 18 && Pictures == 16);
	CHECK(bEach);
	CHECK(bRead);
	CHECK(bWhite);
	CHECK(bFloor);
	CHECK(bTable);
	CHECK(bRefused && !std::filesystem::exists(Seals / "seal-star.png") && !std::filesystem::exists(Seals / "seal-night.png"));
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
	std::printf("edge seals: %d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
