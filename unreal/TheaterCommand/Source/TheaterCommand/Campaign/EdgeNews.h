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
	/** Ruling 64's class (EdgeSeals::ClassOf): 1 and 2 are red, and pulse until opened. */
	int32 Class = 3;
	/** The row's tone on the card: 1 good news, -1 bad, 2 something that blocks or warns, 0 neither; 3 next month's, in grey. */
	int32 Tone = 0;
	/** What is pressed into it (EdgeSeals::EGlyph), and the order sign it names where it is one ("inf", "hull", "star"...). */
	int32 Glyph = 0;
	FString Sign;
	/** The row's eight words, for the pointer. */
	FString Words;
	/** Its place: a province, or a sea of the chart; INDEX_NONE for neither. */
	int32 Region = INDEX_NONE;
	int32 Sea = INDEX_NONE;
	/** How many items the "+N" seal stands for. */
	int32 More = 0;
	/** Pressed this month: it shows opened, dimmed, until the next month's seals are laid. */
	bool bOpened = false;

	bool HasPlace() const { return Region != INDEX_NONE || Sea != INDEX_NONE; }
	bool IsRed() const { return Class >= 1 && Class <= 2; }
};

/** The month's seals, and the row of the card a seal last opened. */
struct FEdgeNewsBoard
{
	/** Up to about nine seals, which the screen lays along the edge: the most an edge carries, the last of them "+N" where more items fold into it. */
	static constexpr int32 MostSeals = 9;

	/** In card order. Empty until a month's card falls due, and again as the next month resolves. */
	TArray<FEdgeNewsSeal> Seals;
	FString Date;
	/** When they were laid, and a number that grows each time they are: the screen slides them in once for each. */
	double LaidAt = 0.0;
	int32 Revision = 0;
	/** The row a seal opened the card at (its part and its place; -1 for the "+N" seal, which opens the card at its head), the card's page it opened, and when. */
	int32 FocusSection = -1;
	int32 FocusIndex = -1;
	int32 FocusPage = -1;
	double FocusAt = 0.0;

	/** Sets out the seals for a month's card: its items in its order, the first MostSeals - 1 and "+N" where there are more. */
	void LayFrom(const FMonthCard& Card);
	/** Takes every seal away (the month that follows resolves). */
	void Clear();
};
