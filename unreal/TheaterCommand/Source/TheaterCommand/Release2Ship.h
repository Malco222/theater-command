#pragma once

#include "CoreMinimal.h"

/**
 * The release 2 ship list (Brian, 25 November 2026 01:10Z: "turn them on automatically").
 *
 * Release 2 grew as dozens of pieces, each behind a switch of its own and OFF until its tests passed: launch words such
 * as -TheaterLandEpic, rows on the ground's Battle rules card, the grand campaign's rule switches on OPTIONS, and a few
 * of OPTIONS' own viewing choices. This is the one list of them all, and what each is in the build Brian plays: ON,
 * unless the Mac pass or the art director failed it, or a standing rule holds it off (then OFF, with the reason). Like a
 * new car that leaves the works with every option fitted and a switch for each on the dash: he can turn any one off on
 * OPTIONS (or on the Battle rules card), and that stays as he left it.
 *
 * - A launch-word feature: at the game module's start its words are added to the command line, as if typed, unless he
 *   turned it off on OPTIONS; turning one off or on there takes the words out or puts them back for the next battle
 *   (or the next start, where the game reads them only once). Its OPTIONS row is in the RELEASE 2 rows. A look on the
 *   ground's trial list is one word of -TheaterLandTrial=, gathered with the others that are on into that one word (a
 *   -TheaterLandTrial= typed on the launch is left as typed).
 * - A Battle rules card row: on unless he switched it off on the card (FLandLive::RuleOn asks Ships).
 * - A grand campaign rule: switched on once in each war, as the war opens, or the first time an older war (1.5, 1.5.1)
 *   is played in release 2 (TC.r2Ship, which marks the war so that his own OFF afterwards stands).
 * - An OPTIONS viewing choice: its default where options.json does not hold it yet.
 * - A console variable: set to 1 at start.
 *
 * For a test run, -TheaterR2Off leaves the whole list off (release 2 as it was before this list: everything as its own
 * switch has it) and -TheaterR2Without=Key,Key leaves the named ones off. A feature that the list holds off can still
 * be turned on for a run by typing its own launch words.
 */
namespace Release2Ship
{
	enum class EKind : uint8
	{
		Launch,       // launch words, with a row of its own on OPTIONS
		TrialWord,    // one word of the ground's -TheaterLandTrial= (the looks on trial), gathered into one launch word
		LandRule,     // a row on the ground's Battle rules card (Words = the row's key)
		CampaignRule, // the grand campaign's rule switch (Words = its OPTIONS id, as act() takes it)
		Option,       // an OPTIONS viewing choice kept in options.json (Words = its key there)
		Variable,     // a console variable set to 1 (Words = its name)
	};

	struct FFeature
	{
		/** Its name in this list, in -TheaterR2Without= and in GameUserSettings.ini [TheaterCommand.Release2]. */
		const TCHAR* Key;
		EKind Kind;
		/** OPTIONS' name and line for a launch-word feature (the others are named where they are switched; a trial word whose
		 * own switch is a Battle rules card row, such as worn uniforms, has none: empty). */
		const TCHAR* Label;
		const TCHAR* Tip;
		const TCHAR* Words;
		/** ON in the build Brian plays. */
		bool bShips;
		/** Off: why, in one line (shown in the log, and kept for the run sheet). On: who passed it, or what pass it waits on. */
		const TCHAR* Note;
		/** Launch words the game reads once per run: a change on OPTIONS counts from the next start. */
		bool bOnceARun;
	};

	/** At the game module's start: the launch words of every feature that is on, and the console variables. */
	void Apply();

	int32 Count();
	const FFeature& At(int32 Index);

	/** Whether the list leaves this feature on for this run (as it ships, as he left it, and not held back by a test word). */
	bool IsOn(int32 Index);

	/** OPTIONS: he turned a launch-word feature off or on. Saved, and its words taken out or put back. */
	void Flip(int32 Index);

	/** The default the list gives a Battle rules card row or an OPTIONS viewing choice named Words (false if none). */
	bool Ships(EKind Kind, const TCHAR* Words);

	/** The grand campaign rule switches that ship on, as the rules' TC.r2Ship takes them: ["nicknames","marked",...]. */
	FString WarSwitchesJson();

	/** The OPTIONS rows of the launch-word features that ship: id "r2:<Key>". */
	struct FRow
	{
		FString Id;
		const TCHAR* Label = TEXT("");
		FString Tip;
		bool bOn = false;
	};
	TArray<FRow> OptionRows();

	/** OPTIONS pressed "r2:<Key>": flips it and says true. */
	bool Press(const FString& Id);
}
