// One look for the whole war on the campaign map, its cloud test (Tools/one_look/run.sh): the game's own
// Campaign/OneLook.h, no engine, run over the months of the dated marks as the art director's table has them
// (one_look_dated.inc, written by one_look_check.py from Content/Data/Campaign/unit-table.json).

#include "Campaign/OneLook.h"

#include <cstdio>
#include <string>

using namespace TheaterOneLook;

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	struct FDated
	{
		const char* Tag;
		int Count;
		int From[4];
	};

	const FDated Table[] = {
#include "one_look_dated.inc"
	};

	/** The mark that stands in a month, reckoned another way: how many of the list's months have come. */
	int Come(const FDated& Row, int Month)
	{
		int Number = 0;
		for (int K = 0; K < Row.Count; ++K)
		{
			Number += Row.From[K] <= Month ? 1 : 0;
		}
		return Number;
	}

	std::string MonthWords(int Month)
	{
		static const char* Names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		return std::string(Names[Month % 12]) + " " + std::to_string(Month / 12);
	}
}

int main()
{
	// The months, as the table's reader and the map count them.
	Check(MonthOf(1942, 6) == 1942 * 12 + 5, "June 1942 is counted as the table's reader counted it");
	Check(MonthOfTurn(1) == MonthOf(1939, 9), "turn 1 is September 1939");
	Check(MonthOfTurn(16) == MonthOf(1940, 12), "turn 16 is December 1940");
	Check(MonthOfTurn(1) == 1939 * 12 + 8, "the month the painter starts in is what it was before");

	// A country with no dated mark has none, one look or not; a list of nothing is never read.
	Check(EraOf(nullptr, 0, MonthOf(1943, 1), false) == 0 && EraOf(nullptr, 0, MonthOf(1943, 1), true) == 0, "a country with no dated mark has none either way");

	// A list of three, by hand.
	{
		const int Three[3] = { MonthOf(1940, 1), MonthOf(1941, 1), MonthOf(1942, 1) };
		Check(EraOf(Three, 3, MonthOf(1939, 12), false) == 0, "without one look: none before the first month");
		Check(EraOf(Three, 3, MonthOf(1940, 1), false) == 1, "without one look: the first from its own month");
		Check(EraOf(Three, 3, MonthOf(1941, 6), false) == 2, "without one look: the second between the second and the third");
		Check(EraOf(Three, 3, MonthOf(1950, 1), false) == 3, "without one look: the last from its month on");
		Check(EraOf(Three, 3, MonthOf(1939, 12), true) == 3 && EraOf(Three, 3, MonthOf(1941, 6), true) == 3 && EraOf(Three, 3, MonthOf(1950, 1), true) == 3,
			"with one look: the last in every month");
	}

	// The table's own: every country whose marks it dates, every month from January 1936 to December 1950, and every turn of a long war.
	const int Rows = static_cast<int>(sizeof(Table) / sizeof(Table[0]));
	Check(Rows >= 1, "the table dates some country's marks");
	std::printf("The mark that stands (0 the British one its machines are built with; 1, 2 its own dated marks)\n");
	std::printf("  %-4s %-28s %-22s %s\n", "", "its own marks stand from", "as before: Sep 1939, Jun 1945", "one look: Sep 1939, Jun 1945");
	for (int R = 0; R < Rows; ++R)
	{
		const FDated& Row = Table[R];
		const std::string Tag = Row.Tag;
		bool bOneAlways = true, bAsBefore = true, bNeverBack = true;
		int ChangesWith = 0, ChangesWithout = 0;
		int WasWith = EraOf(Row.From, Row.Count, MonthOf(1936, 1), true), WasWithout = EraOf(Row.From, Row.Count, MonthOf(1936, 1), false);
		for (int Month = MonthOf(1936, 1); Month <= MonthOf(1950, 12); ++Month)
		{
			const int With = EraOf(Row.From, Row.Count, Month, true), Without = EraOf(Row.From, Row.Count, Month, false);
			bOneAlways = bOneAlways && With == Row.Count;
			bAsBefore = bAsBefore && Without == Come(Row, Month);
			bNeverBack = bNeverBack && Without >= WasWithout;
			ChangesWith += With != WasWith ? 1 : 0;
			ChangesWithout += Without != WasWithout ? 1 : 0;
			WasWith = With;
			WasWithout = Without;
		}
		Check(bOneAlways, Tag + ": with one look the last of its dated marks stands in every month from 1936 to 1950");
		Check(ChangesWith == 0, Tag + ": with one look its mark never changes");
		Check(bAsBefore && bNeverBack, Tag + ": without one look the mark that stands is the last its month has reached, as before");
		Check(ChangesWithout == Row.Count, Tag + ": without one look its mark changes once for each dated mark, as before");
		Check(EraOf(Row.From, Row.Count, MonthOfTurn(1), false) == 0, Tag + ": without one look it opens the war in the British mark, as before");
		Check(EraOf(Row.From, Row.Count, MonthOfTurn(1), true) == Row.Count && EraOf(Row.From, Row.Count, MonthOfTurn(-40), true) == Row.Count
			&& EraOf(Row.From, Row.Count, MonthOfTurn(400), true) == Row.Count, Tag + ": with one look it wears its own in the first turn, in a war begun early and in one that runs long");
		bool bTurns = true;
		for (int Turn = 1; Turn <= 120; ++Turn)
		{
			bTurns = bTurns && EraOf(Row.From, Row.Count, MonthOfTurn(Turn), true) == Row.Count;
		}
		Check(bTurns, Tag + ": the same in every turn of a ten-year war");
		std::string Stands;
		for (int K = 0; K < Row.Count; ++K)
		{
			Stands += (K > 0 ? ", " : "") + MonthWords(Row.From[K]);
		}
		std::printf("  %-4s %-28s %d, %-19d %d, %d\n", Row.Tag, Stands.c_str(), EraOf(Row.From, Row.Count, MonthOf(1939, 9), false), EraOf(Row.From, Row.Count, MonthOf(1945, 6), false),
			EraOf(Row.From, Row.Count, MonthOf(1939, 9), true), EraOf(Row.From, Row.Count, MonthOf(1945, 6), true));
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults == 0 ? 0 : 1;
}
