// Field notes' cloud test (Tools/field_notes/run.sh): a new general's first battle played through the desk, frame by
// frame at 30 frames a second, as ALandBattle fills its facts.

#include "Battle/FieldNotes.h"
#include "Battle/LandJson.h"

#include <cstdio>
#include <string>

namespace
{
	int Failed = 0, Passed = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (bOk)
		{
			++Passed;
		}
		else
		{
			++Failed;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	/** The battle as the test plays it: the facts, the desk, the notebook as last saved, real time. */
	struct Play
	{
		FieldNotes::Desk Desk;
		FieldNotes::Facts F;
		FieldNotes::Book Saved;
		int Saves = 0;

		void Frames(double Seconds)
		{
			const int Count = static_cast<int>(Seconds * 30.0 + 0.5);
			for (int K = 0; K < Count; ++K)
			{
				F.Real += 1.0 / 30.0;
				if (F.SpeedNow > 0.0)
				{
					F.Battle += F.SpeedNow / 30.0 / 2.0;
				}
				if (Desk.Tick(F))
				{
					Saved = Desk.TheBook();
					++Saves;
				}
			}
		}

		std::string Up() const
		{
			const FieldNotes::Note* N = Desk.Current();
			return N ? N->Id : std::string("-");
		}
	};

	FieldNotes::Words Shipped(const char* Path)
	{
		std::string Text, Why;
		FieldNotes::Words W;
		Check(LandJson::ReadFile(Path, Text), std::string("the words file reads: ") + Path);
		Check(FieldNotes::ReadWords(Text, W, Why), "the words parse (" + Why + ")");
		return W;
	}
}

int main(int Argc, char** Argv)
{
	const char* Path = Argc > 1 ? Argv[1] : "Content/Data/Battle/field-notes.json";
	const FieldNotes::Words W = Shipped(Path);

	// ---- The words: ruling 147's rule (30 words at most a note), one short deed line, the tip and the closing note ----
	Check(W.Notes.size() == 7, "seven notes");
	Check(W.Label == "FIELD NOTE", "the head row's word");
	for (const FieldNotes::Note& N : W.Notes)
	{
		Check(FieldNotes::CountWords(N.Text) <= 30, N.Id + ": 30 words at most (" + std::to_string(FieldNotes::CountWords(N.Text)) + ")");
		Check(FieldNotes::CountWords(N.Deed) <= 7, N.Id + ": its deed line short");
		Check(N.When == "setup" || N.When == "fight" || N.When == "any", N.Id + ": a known moment");
		Check(N.Aim == "begin" || N.Aim == "pause" || N.Aim == "tag" || N.Aim == "bar" || N.Aim == "none", N.Id + ": a known aim");
	}
	Check(FieldNotes::CountWords(W.Closing.Text) <= 30 && !W.Closing.Heading.empty(), "the closing note");
	Check(!W.FightTip.empty() && FieldNotes::CountWords(W.FightTip) <= 30, "the attack card's tip");
	{
		FieldNotes::Words Bad;
		std::string Why;
		Check(!FieldNotes::ReadWords("{\"notes\":[{\"id\":\"a\",\"heading\":\"h\",\"text\":\"t\"}]}", Bad, Why), "a note with no deed is refused");
		Check(!FieldNotes::ReadWords("{\"notes\":[]}", Bad, Why), "no notes is refused");
		Check(!FieldNotes::ReadWords("{", Bad, Why), "broken text is refused");
	}

	// ---- The notebook: written and read back; a new player's from nothing ----
	{
		FieldNotes::Book B = FieldNotes::ReadBook("");
		Check(B.bOn && B.Done.empty() && !B.bClosed, "a new player's notebook: on, nothing done");
		B.MarkDone("look");
		B.MarkDone("look");
		B.MarkSeen("fight");
		const FieldNotes::Book Again = FieldNotes::ReadBook(FieldNotes::WriteBook(B));
		Check(Again.Done.size() == 1 && Again.IsDone("look") && Again.HasSeen("fight") && Again.bOn, "the notebook read back as written");
		Check(FieldNotes::ReadBook("not json").bOn, "an unreadable notebook: a new player's");
		FieldNotes::Book Off;
		Off.bOn = false;
		Check(!FieldNotes::ReadBook(FieldNotes::WriteBook(Off)).bOn, "NOTES OFF kept");
	}

	// ---- A first battle with a setup step, every deed done in turn ----
	{
		Play P;
		P.Desk.Begin(W, FieldNotes::Book(), true);
		P.F.bGeneral = true;
		P.F.bDeploying = true;
		P.F.SpeedNow = 0.0;
		P.F.Speed = 1.0;
		P.Frames(0.5);
		Check(P.Up() == "ground", "setting out: the first note is the ground's (" + P.Up() + ")");
		Check(P.Desk.Now().Number == 1 && P.Desk.Now().Of == 7, "1 OF 7");
		P.F.Placed = 1;
		P.Frames(0.2);
		Check(!P.Desk.Now().bStamped, "a deed done in the note's first second does not stamp it yet");
		P.Frames(1.0);
		Check(P.Desk.Now().bStamped && P.Saved.IsDone("ground") && P.Saves == 1, "a unit set down: stamped DONE and saved");
		P.Frames(2.0);
		Check(P.Up() == "-", "the stamp goes in two seconds at most, then a quiet");
		P.Frames(1.5);
		Check(P.Up() == "look", "the view's note next, while he still sets out (" + P.Up() + ")");
		P.F.LookX += 10.0;
		P.Frames(2.0);
		Check(!P.Desk.Now().bStamped, "10 m is not a look round");
		P.F.LookX += 20.0;
		P.Frames(0.1);
		Check(P.Desk.Now().bStamped, "30 m is");
		P.Frames(3.5);
		Check(P.Up() == "-", "the fight's notes wait for the clock to run (" + P.Up() + ")");
		P.F.bDeploying = false;
		P.F.SpeedNow = 4.0;
		P.F.Speed = 4.0;
		P.Frames(0.2);
		Check(P.Up() == "pick", "the clock running: pick up a unit (" + P.Up() + ")");
		P.F.InHand = 1;
		P.Frames(1.0);
		Check(P.Desk.Now().bStamped, "one in hand: stamped");
		P.Frames(3.5);
		Check(P.Up() == "order", "then the order (" + P.Up() + ")");
		P.F.Orders = 1;
		P.Frames(1.2);
		Check(P.Desk.Now().bStamped, "an order: stamped");
		P.Frames(3.5);
		Check(P.Up() == "clock", "then the pace (" + P.Up() + ")");
		P.Frames(2.0);
		Check(!P.Desk.Now().bStamped, "the scene's own pace is not his");
		P.F.Speed = 0.0;
		P.F.SpeedNow = 0.0;
		P.F.bOwnSpeed = false;
		P.Frames(0.1);
		Check(P.Desk.Now().bStamped, "PAUSE: stamped");
		P.F.Speed = P.F.SpeedNow = 1.0;
		P.Frames(3.5);
		Check(P.Up() == "decides", "then what decides it (" + P.Up() + ")");
		P.F.Hover = 3;
		P.Frames(1.2);
		Check(P.Desk.Now().bStamped, "a unit pointed at: stamped");
		P.F.Hover = -1;
		P.Frames(3.5);
		Check(P.Up() == "break", "then breaking off (" + P.Up() + ")");
		P.Frames(100.0);
		Check(!P.Desk.Now().bStamped, "a minute of real time at life is half a minute fought");
		P.Frames(25.0);
		Check(P.Up() == "-" || P.Up() == "yours", "a minute fought: done");
		Check(P.Saved.AllDone(W), "all seven done in the notebook");
		P.Frames(3.0);
		Check(P.Up() == "yours" && P.Desk.Now().Number == 0, "the closing note, with no count");
		Check(P.Saved.bClosed, "the closing note marked shown");
		P.Desk.Close();
		Check(P.Up() == "-", "a click sends the closing note");
		P.Frames(30.0);
		Check(P.Up() == "-", "nothing more");

		// The next battle with that notebook: no notes at all.
		Play Next;
		Next.Desk.Begin(W, P.Saved, true);
		Next.F.bGeneral = true;
		Next.Frames(10.0);
		Check(Next.Up() == "-", "a general who has done them all sees none");
		// FIRST COMMAND arms them again.
		FieldNotes::Book Armed = P.Saved;
		Armed.ArmAgain(W);
		Check(Armed.Done.empty() && Armed.bOn && !Armed.bClosed, "FIRST COMMAND arms all seven again once all were done");
		FieldNotes::Book Half;
		Half.bOn = false;
		Half.MarkDone("look");
		Half.ArmAgain(W);
		Check(Half.bOn && Half.IsDone("look"), "with some still to do, FIRST COMMAND turns them on and keeps the done ones");
	}

	// ---- A battle with no setup step: the ground's note waits for one; the closing note waits for all ----
	{
		Play P;
		P.Desk.Begin(W, FieldNotes::Book(), true);
		P.F.bGeneral = true;
		P.Frames(0.5);
		Check(P.Up() == "look", "no setup step: the view's note first (" + P.Up() + ")");
	}

	// ---- Held behind the after card or a question; the film; LATER; NOTES OFF; not shown at all ----
	{
		Play P;
		P.Desk.Begin(W, FieldNotes::Book(), true);
		P.F.bGeneral = true;
		P.Frames(0.5);
		P.F.bBusy = true;
		P.F.LookX = 100.0;
		P.Frames(3.0);
		Check(!P.Desk.Now().bStamped && P.Desk.Now().bShown, "behind a question: held, no deed read");
		P.F.bBusy = false;
		P.Frames(0.1);
		Check(!P.Desk.Now().bStamped, "time behind the question is not reading time");
		P.Frames(1.0);
		Check(P.Desk.Now().bStamped, "out again: its deed counts");
		P.Frames(3.5);
		Check(P.Up() == "pick", "next");
		P.F.bGeneral = false;
		P.F.InHand = 1;
		P.Frames(3.0);
		Check(!P.Desk.Now().bStamped, "under the film: held");
		P.F.bGeneral = true;
		P.F.InHand = 0;
		P.Desk.Later();
		Check(P.Up() == "-", "LATER puts it away");
		P.Frames(2.0);
		Check(P.Up() == "order", "and the next comes in its place (" + P.Up() + ")");
		Check(P.Desk.Off(), "NOTES OFF changes the notebook");
		P.Frames(5.0);
		Check(P.Up() == "-" && !P.Desk.TheBook().bOn, "NOTES OFF: none");
		Play Next;
		Next.Desk.Begin(W, P.Desk.TheBook(), true);
		Next.F.bGeneral = true;
		Next.Frames(5.0);
		Check(Next.Up() == "-", "NOTES OFF holds in the next battle");
		Play Hidden;
		Hidden.Desk.Begin(W, FieldNotes::Book(), false);
		Hidden.F.bGeneral = true;
		Hidden.Frames(5.0);
		Check(Hidden.Up() == "-", "switched off (the ship list's row, or a run): none");
		// The later note comes back in the next battle.
		FieldNotes::Book Later = FieldNotes::Book();
		Later.MarkDone("look");
		Play Again;
		Again.Desk.Begin(W, Later, true);
		Again.F.bGeneral = true;
		Again.Frames(0.5);
		Check(Again.Up() == "pick", "a note put away with LATER comes in the next battle (" + Again.Up() + ")");
	}

	// ---- The view's other moves: a turn, a zoom, the top view ----
	{
		for (int Way = 0; Way < 3; ++Way)
		{
			Play P;
			P.Desk.Begin(W, FieldNotes::Book(), true);
			P.F.bGeneral = true;
			P.F.Heading = 6.2;
			P.Frames(1.5);
			if (Way == 0)
			{
				P.F.Heading = 0.3; // (across north: 0.38 rad the short way)
			}
			else if (Way == 1)
			{
				P.F.Zoom = 1.3;
			}
			else
			{
				P.F.bTopView = true;
			}
			P.Frames(0.1);
			Check(P.Desk.Now().bStamped, "the view's deed by " + std::string(Way == 0 ? "a turn" : Way == 1 ? "a zoom" : "the top view"));
		}
	}

	std::printf("field notes: %d passed, %d failed\n", Passed, Failed);
	return Failed == 0 ? 0 : 1;
}
