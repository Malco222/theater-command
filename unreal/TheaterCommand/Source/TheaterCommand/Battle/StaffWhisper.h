#pragma once

// The chief of staff's whisper (release 2; Brian's yes, 28 November 2026 00:09Z): one quiet line on the general's screen
// about the unit he has just taken in hand, when there is something true and useful to say about it. The order on the
// radio and when it lands; a unit whose nerve is going; one hidden in cover, and what gives it away; a tank with hedge
// cutters; smoke to lay; guns, which fire from where they stand. Each line once a battle (a wavering unit's, once for
// each unit), never over a staff note, never under a question or the after card, and QUIET puts them away for the battle.
//
// Like a caddie at your shoulder who speaks only when the club in your hand has something worth knowing about it, and
// says it once.
//
// Plain C++, no engine: the words (Content/Data/Battle/staff-whispers.json, each line's keys and screen words checked by
// Tools/field_notes against the general's screen as the field notes' are), and the desk that says which line is up this
// frame from the unit in hand. The scene fills the facts (LandBattleNotes.cpp), the screen draws the line
// (LandBattleScreen.cpp). They follow the staff notes' one switch (the Guide's, NOTES OFF), and the ship list's row.

#include <string>
#include <vector>

namespace StaffWhisper
{
	/** One line: its name, the moment it speaks for (radio, wavering, hidden, cutters, smoke, guns), whether it is said
	 * once a battle or once for each unit ("once", "each"), and its words ({s}: the seconds till the order lands). */
	struct Line
	{
		std::string Id, When, Scope, Text;
	};

	struct Words
	{
		/** The line's tag ("CHIEF OF STAFF"). */
		std::string Label;
		std::vector<Line> Lines;
	};

	/** The words from the data file's text; false (and why) where it cannot be read, a line lacks a part, or names a
	 * moment this desk does not know. */
	bool ReadWords(const std::string& Json, Words& Out, std::string& OutWhy);

	/** What the scene knows this frame about his screen and the unit in hand (the first he picked up). */
	struct Facts
	{
		/** His screen is up (not the film), and nothing stands over it (the after card, a question, the night's card). */
		bool bGeneral = false, bBusy = false;
		/** A staff note is up: one voice at a time. */
		bool bNoteUp = false;
		/** The setup step (the orders bar's keys wait for the clock). */
		bool bDeploying = false;
		/** The first unit in his hand, -1 none; its kind (inf, arm, art, ...), its team (mg, mor, snp; empty), its state
		 * (steady, wavering, ...), and what his clipboard knows of it. */
		int Block = -1;
		std::string Kind, Team, State;
		bool bHidden = false, bCutter = false;
		int Smoke = -1;
		/** Seconds of battle till his order reaches it (-1 none on the air). */
		double OrderIn = -1.0;
		/** Every unit in his hand is a gun (art): a click on an enemy fires on it from where they stand. */
		bool bAllGuns = false;
		/** Real seconds since the battle began (the line's own clock: the battle may be paused). */
		double Real = 0.0;
	};

	/** The line's moment holds for these facts. */
	bool Holds(const Line& L, const Facts& F);

	/** The line's words for these facts ({s} the seconds till the order lands, whole). */
	std::string Said(const Line& L, const Facts& F);

	/** What is up this frame. */
	struct Up
	{
		bool bShown = false;
		/** The line's index in the words, the unit it speaks of, and how long it has been up (real seconds). */
		int Index = -1, Block = -1;
		double Age = 0.0;
		/** Its words as first said (the seconds then: they do not count down under his eyes). */
		std::string Text;
	};

	class Desk
	{
	public:
		/** A unit must stay in hand this long before a line is said of it (a quick click past it says nothing). */
		static constexpr double Settle = 0.6;
		/** A line stays up this long. */
		static constexpr double Stays = 7.0;
		/** And the staff is quiet this long after it before the next. */
		static constexpr double Between = 10.0;

		/** A battle begins; bOn: the row on and the staff notes on. */
		void Begin(const Words& InWords, bool bOn);
		/** A frame: true when the line up changes (one comes or goes). */
		bool Tick(const Facts& F);
		/** QUIET: no more lines this battle. */
		void Quiet();

		bool Showing() const { return bOn && !bQuiet; }
		const Up& Now() const { return Shown; }
		const Line* Current() const;
		const Words& TheWords() const { return W; }

	private:
		bool Said(int Index, int Block) const;

		Words W;
		bool bOn = false, bQuiet = false;
		Up Shown;
		/** The unit in hand and since when (real seconds); when the last line went. */
		int HandBlock = -1;
		double HandSince = 0.0, GoneAt = -1.0e9, Last = 0.0;
		/** Lines said this battle: (index, block; -1 for a line said once a battle). */
		std::vector<std::pair<int, int>> Done;
	};
}
