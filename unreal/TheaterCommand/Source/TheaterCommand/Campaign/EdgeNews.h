#pragma once

#include "CoreMinimal.h"

struct FMonthCard;

/**
 * (Release 2, campaign map plan item 5, "News at the map's edge"; Brian's card of 16 November 2026, ruling 131:
 * "Reports arrive as seals on the edge of the map, so only your own battles stop the month.")
 *
 * One seal on the map's edge: an item of the month's card (ruling 64(g): the same items, in the same order), what is
 * pressed into it, its colour, its eight words for the pointer, and the place it points at, where the row has one
 * (ruling 64(d)). Display only: nothing here is saved. The seals are laid again from the month's card.
 */
struct FEdgeNewsSeal
{
	/** The card's part (EdgeSeals::ESection: 0 a band, 1 the lead story, 2 a front's row, 3 home, 4 the staff, 5 elsewhere, 6 "+N") and the item's place in it. */
	int32 Section = 0;
	int32 Index = 0;
	/** Ruling 64's class (EdgeSeals::ClassOf). */
	int32 Class = 3;
	/** The row's tone on the card: 1 good news (a band: a FREED his own side won), -1 bad, 2 something that blocks or warns, 0 neither; 3 next month's, in grey. */
	int32 Tone = 0;
	/**
	 * The colour it wears (EdgeSeals::EWax, from EdgeSeals::LookOf: the colour of its row on the card, the art
	 * director's ruling of 22 November), and whether it pulses until opened: a dated warning (red), and war and peace
	 * and the news (the news band's brass, or green for a freeing his own side did).
	 */
	int32 Wax = 4; // (EdgeSeals::EWax::Brass, plain: EdgeNews.cpp checks the number)
	bool bPulse = false;
	/** What is pressed into it (EdgeSeals::EGlyph), and the order sign its row names ("inf", "hull"...; a "star" is never pressed: EdgeSeals::MayPress). */
	int32 Glyph = 0;
	FString Sign;
	/** The row's eight words, for the pointer. */
	FString Words;
	/** Its place: a province, or a sea of the chart; INDEX_NONE for neither. */
	int32 Region = INDEX_NONE;
	int32 Sea = INDEX_NONE;
	/** Pressed this month: it shows opened, dimmed, until the next month's seals are laid. */
	bool bOpened = false;

	bool HasPlace() const { return Region != INDEX_NONE || Sea != INDEX_NONE; }
};

/** The month's seals, and the row of the card a seal last opened. */
struct FEdgeNewsBoard
{
	/** At most nine seals stand on the edge: every item where the card has no more, else eight and "+N" for the rest. */
	static constexpr int32 MostSeals = 9;

	/**
	 * The card's first items as seals, in card order (at most MostSeals), and how many items the card has in all. Empty
	 * until a month's card falls due, and again as the next month resolves. How many of them stand is the screen's to
	 * say, each frame, from the room the edge has (EdgeSeals::FLayout::LayRanked): the rest fold into the "+N" seal.
	 */
	TArray<FEdgeNewsSeal> Seals;
	int32 Items = 0;
	/** The "+N" seal: the items that do not stand, counted; a press opens the card at its head. */
	FEdgeNewsSeal Rest;
	FString Date;
	/** When they were laid, and a number that grows each time they are: the screen slides them in once for each. */
	double LaidAt = 0.0;
	int32 Revision = 0;
	/** The row a seal opened the card at (its part and its place; -1 for the "+N" seal, which opens the card at its head), the card's page it opened, and when. */
	int32 FocusSection = -1;
	int32 FocusIndex = -1;
	int32 FocusPage = -1;
	double FocusAt = 0.0;

	/** Sets out the seals for a month's card: its first MostSeals items in its order, and the "+N" seal for the rest. */
	void LayFrom(const FMonthCard& Card);
	/** Takes every seal away (the month that follows resolves). */
	void Clear();
};
