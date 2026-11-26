#pragma once

/**
 * (Release 2, the campaign map; Brian, 26 November 2026: "have the paint be on it regardless of the year".) One look
 * for the whole war: which of a country's dated marks stands, with it and without.
 *
 * The art director's table dates the marks of four Commonwealth air forces: each wore the British roundel until its
 * own came in, part of the way through the war, and one of them changed again after that. Without one look the mark
 * that stands is the last one the campaign's month has reached, as before. With it the last of them stands in every
 * month, the first as well as the last: a country never shows the mark it had before, and never changes.
 *
 * Plain arithmetic with no engine in it, so that the cloud can check it (Tools/one_look/run.sh).
 * UnitTable::MarkEra, UnitTable::SetTurn and the table's reader use it.
 */
namespace TheaterOneLook
{
	/** A month counted from the year 0, as the table's "1942-06" is counted: June 1942 is 1942 * 12 + 5. */
	inline constexpr int MonthOf(int Year, int Month) { return Year * 12 + Month - 1; }

	/** The campaign's month at a turn: turn 1 is September 1939, and turn 16 December 1940. */
	inline constexpr int MonthOfTurn(int Turn) { return 1939 * 12 + 7 + Turn; }

	/**
	 * Which of a country's dated marks stands: 0 for none, else one more than its place in the table's list.
	 * DatedFrom holds the month each stands from, earliest first; it may be null where Count is nothing.
	 */
	inline int EraOf(const int* DatedFrom, int Count, int MonthNow, bool bOneLook)
	{
		if (Count <= 0)
		{
			return 0;
		}
		if (bOneLook)
		{
			return Count;
		}
		int Era = 0;
		for (int K = 0; K < Count; ++K)
		{
			if (DatedFrom[K] <= MonthNow)
			{
				Era = K + 1;
			}
		}
		return Era;
	}
}
