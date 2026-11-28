#pragma once

// Field notes (release 2): a new general's first battle on the field, taught as he plays it. FIRST COMMAND's staff note
// (ruling 129; its look, ruling 147) carried onto the general's screen of a live ground battle: one note at a time, a
// sentence or two and ONE deed; when he does it, the note is stamped DONE and the next comes when its moment does. It
// never takes the controls, never covers the after card or a question, and folds away with LATER (this battle) or
// NOTES OFF (for good). The campaign's month and its armies are FIRST COMMAND's; the field is these.
//
// Like the yellow notes a driving instructor sticks on the dashboard for the first drive: one at a time, each peeled
// off once its manoeuvre is done, none left on for the hundredth drive.
//
// Plain C++, no engine: the words (Content/Data/Battle/field-notes.json), the player's notebook (which notes are done,
// whether they are on), and the desk that says which note is up this frame from the battle's facts. The scene fills the
// facts (LandBattleNotes.cpp), the screen draws the note (LandBattleScreen.cpp). Tools/field_notes/run.sh tests it.

#include <string>
#include <vector>

namespace FieldNotes
{
	/** One note: its name, when it may come (setup: only while he sets out his army; fight: once the clock runs; any),
	 * what the brass corners frame (begin, pause, tag, bar, none), its heading, its sentences and its deed line. */
	struct Note
	{
		std::string Id, When, Aim, Heading, Text, Deed;
	};

	struct Words
	{
		/** The head row's word ("STAFF NOTE", as FIRST COMMAND's: the UI director 28 November 00:08Z). */
		std::string Label;
		std::vector<Note> Notes;
		/** The closing note (no deed), once every note is done. */
		Note Closing;
		/** The attack card's one-line tip the first time it offers FIGHT IT YOURSELF. */
		std::string FightTip;
		/** Said once, in the middle line, when NOTES OFF is pressed: where they come back. */
		std::string OffLine;
	};

	/** The words from the data file's text; false (and why) when it cannot be read or a note lacks a part. */
	bool ReadWords(const std::string& Json, Words& Out, std::string& OutWhy);
	/** Words in a piece of text, as ruling 147 counts them (split on spaces). */
	int CountWords(const std::string& Text);

	/** The player's notebook, kept with him, not with a save. */
	struct Book
	{
		bool bOn = true;
		std::vector<std::string> Done;
		/** Tips seen once (the attack card's "fight"). */
		std::vector<std::string> Seen;
		/** The closing note has been shown. */
		bool bClosed = false;
		/** NOTES OFF pressed in a battle: the campaign map's staff notes are still to be turned off (one switch), kept
		 * in the notebook so that a game quit from the battle still owes it at the next start. */
		bool bMapOffOwed = false;
		/** The practice battle's PRACTISE WITH STAFF NOTES pressed on the front door (release 2): the campaign map's are
		 * still to be turned on (one switch), owed as the off is. */
		bool bMapOnOwed = false;

		bool IsDone(const std::string& Id) const;
		bool HasSeen(const std::string& Id) const;
		void MarkDone(const std::string& Id);
		void MarkSeen(const std::string& Id);
		/** Every note done (by these words' list). */
		bool AllDone(const Words& W) const;
		/** On again; when every note was done, all of them armed again (FIRST COMMAND's rule for its ten). */
		void ArmAgain(const Words& W);
		/** From the first note again (the practice battle's PRACTISE WITH STAFF NOTES): on, none done, the closing note
		 * not yet shown, and the map's owed on in place of any off. The tips seen stay seen. */
		void StartOver();
	};

	/** A notebook from its file's text (an empty or unreadable text: a new player's). */
	Book ReadBook(const std::string& Json);
	std::string WriteBook(const Book& B);

	/** What the battle is doing this frame, as the desk needs it. */
	struct Facts
	{
		/** His general's screen is up (a live battle of his own, the film not holding the camera). */
		bool bGeneral = false;
		/** Something that a note never covers is up: the after card, ALL FALL BACK's question, the night's shots. */
		bool bBusy = false;
		/** He is setting out his army (the setup step, the clock still). */
		bool bDeploying = false;
		/** Units he has set down in the setup step, and orders he has given, so far this battle. */
		int Placed = 0, Orders = 0;
		/** His view: where it looks (field metres), its heading (radians), its zoom, and the top view. */
		double LookX = 0.0, LookY = 0.0, Heading = 0.0, Zoom = 1.0;
		bool bTopView = false;
		/** Units in his hand, and the unit under his pointer (-1 none). */
		int InHand = 0;
		int Hover = -1;
		/** The pace asked (0 held, 1, 2, 4), whether the scene still sets it itself, and the pace this frame. */
		double Speed = 1.0, SpeedNow = 1.0;
		bool bOwnSpeed = true;
		/** Battle seconds shown, and real seconds since the battle opened. */
		double Battle = 0.0, Real = 0.0;
	};

	/** The note as it stands this frame. */
	struct Up
	{
		/** Which: a note (its index in Words::Notes), the closing note (-1), or none. */
		bool bShown = false;
		int Index = -1;
		/** Its count in the head row ("3 OF 7"); the closing note none (0). */
		int Number = 0, Of = 0;
		/** Stamped DONE, and real seconds since; real seconds it has been up. */
		bool bStamped = false;
		double StampAge = 0.0, Age = 0.0;
	};

	/**
	 * One battle's desk: which note is up, when its deed is done, the stamp, the gap, the closing note. Begin it once a
	 * battle with the words and the player's notebook; Tick it each frame; it keeps its own copy of the notebook and
	 * says when that changed, for the scene to write it out.
	 */
	class Desk
	{
	public:
		/** How long the stamp stays (ruling 147: two seconds at most), the least a note is up before its deed counts, the
		 * quiet between two notes, and how far his view must move to count (metres, radians, zoom's share). */
		static constexpr double StampStays = 1.8, LeastUp = 1.0, Between = 1.5;
		static constexpr double LookMetres = 25.0, LookTurn = 0.35, LookZoom = 0.2;

		void Begin(const Words& W, const Book& B, bool bShow);
		/** This frame; true when the notebook changed (save it). */
		bool Tick(const Facts& F);
		const Up& Now() const { return Shown; }
		/** The note up, or the closing note; null none. */
		const Note* Current() const;
		const Words& TheWords() const { return W; }
		const Book& TheBook() const { return B; }
		bool Showing() const { return bShow && B.bOn; }

		/** LATER: this note put away for this battle (it comes again in the next). */
		void Later();
		/** NOTES OFF: none again till they are turned on (true: the notebook changed). */
		bool Off();
		/** A click on a stamped note or the closing note sends it at once. */
		void Close();

	private:
		Words W;
		Book B;
		bool bShow = false;
		Up Shown;
		/** The facts when the note went up (its deed is read against them) and the latest. */
		Facts At, Last;
		double ShownAt = 0.0, StampAt = 0.0, QuietTill = 0.0;
		/** The note is held hidden while something it never covers is up; real seconds hidden do not count as read. */
		double HiddenFor = 0.0;
		bool bHidden = false;
		std::vector<std::string> LaterNow;

		bool Eligible(const Note& N, const Facts& F) const;
		bool DeedDone(const Note& N, const Facts& F) const;
		void Raise(int Index, const Facts& F);
		void Lower(double Real);
	};
}
