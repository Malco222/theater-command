// Field notes' cloud test (Tools/field_notes/run.sh): a new general's first battle played through the desk, frame by
// frame at 30 frames a second, as ALandBattle fills its facts.

#include "Battle/FieldNotes.h"
#include "Battle/LandJson.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

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

	// ---- The keys a note names, against his screen's own bindings (the design director 28 November 2026 23:58Z: a
	// test that fails if a note names a key bound to another deed) ----

	std::vector<std::string> LinesOf(const std::string& Text)
	{
		std::vector<std::string> Out;
		size_t From = 0;
		while (From <= Text.size())
		{
			const size_t To = Text.find('\n', From);
			Out.push_back(Text.substr(From, To == std::string::npos ? std::string::npos : To - From));
			if (To == std::string::npos)
			{
				break;
			}
			From = To + 1;
		}
		return Out;
	}

	/** His keys: OnKeyDown's branch for the general's screen, from its head to the watcher's fallback (the film's keys). */
	std::vector<std::string> GeneralKeys(const std::vector<std::string>& All)
	{
		size_t Head = All.size();
		for (size_t K = 0; K < All.size(); ++K)
		{
			if (All[K].rfind("FReply SLandBattleScreen::OnKeyDown(", 0) == 0)
			{
				Head = K;
				break;
			}
		}
		std::vector<std::string> Out;
		for (size_t K = Head; K < All.size(); ++K)
		{
			if (All[K] == "\tif (Key == EKeys::SpaceBar)")
			{
				return Out;
			}
			Out.push_back(All[K]);
		}
		return std::vector<std::string>();
	}

	bool WordChar(char C)
	{
		return std::isalnum(static_cast<unsigned char>(C)) != 0 || C == '_';
	}

	/** Where `Lead` + the key stands whole in the line (EKeys::E is not EKeys::Escape), npos where it does not. */
	size_t Whole(const std::string& Line, const std::string& Lead, size_t From = 0)
	{
		for (size_t At = Line.find(Lead, From); At != std::string::npos; At = Line.find(Lead, At + 1))
		{
			const size_t End = At + Lead.size();
			if (End >= Line.size() || !WordChar(Line[End]))
			{
				return At;
			}
		}
		return std::string::npos;
	}

	/** The lines that bind the key plainly (`Key == EKeys::K`; Alt's kept views aside, a modifier's own). */
	std::vector<size_t> Bound(const std::vector<std::string>& G, const std::string& Key)
	{
		std::vector<size_t> Out;
		for (size_t K = 0; K < G.size(); ++K)
		{
			if (G[K].find("IsAltDown") == std::string::npos && Whole(G[K], "Key == EKeys::" + Key) != std::string::npos)
			{
				Out.push_back(K);
			}
		}
		return Out;
	}

	/** What the key does on his screen, read as the table names it: view:N (the view's bit), pace:N, pause, do:word. */
	bool DoesIt(const std::vector<std::string>& G, const std::string& Key, const std::string& Does, std::string& Why)
	{
		const std::vector<size_t> On = Bound(G, Key);
		if (On.size() != 1)
		{
			Why = std::to_string(On.size()) + " plain bindings on his screen (one wanted)";
			return false;
		}
		const std::string& Line = G[On[0]];
		if (Does.rfind("view:", 0) == 0)
		{
			size_t After = Whole(Line, "Key == EKeys::" + Key) + std::string("Key == EKeys::").size() + Key.size();
			const std::string Or = " || Key == EKeys::";
			if (Line.compare(After, Or.size(), Or) == 0)
			{
				After += Or.size();
				while (After < Line.size() && WordChar(Line[After]))
				{
					++After;
				}
			}
			const std::string Want = " ? " + Does.substr(5) + "u";
			Why = "not the view's bit " + Does.substr(5);
			return Line.compare(After, Want.size(), Want) == 0;
		}
		if (Line.find("else if (Key == EKeys::" + Key) == std::string::npos)
		{
			Why = "bound in a shared line, not a key of its own";
			return false;
		}
		if (Does.rfind("pace:", 0) == 0)
		{
			Why = "not Pace(" + Does.substr(5) + ")";
			const std::string Want = "Pace(" + Does.substr(5);
			const size_t At = Line.find(Want);
			return At != std::string::npos && At + Want.size() < Line.size() && (Line[At + Want.size()] == '.' || Line[At + Want.size()] == ')');
		}
		if (Does == "pause")
		{
			Why = "no SetSpeed(0.0) under it";
			for (size_t K = On[0]; K < G.size() && K < On[0] + 15; ++K)
			{
				if (G[K].find("SetSpeed(0.0)") != std::string::npos)
				{
					return true;
				}
			}
			return false;
		}
		if (Does.rfind("do:", 0) == 0)
		{
			Why = "not his GeneralDo(\"" + Does.substr(3) + "\")";
			return Line.find("GeneralDo(") != std::string::npos && Line.find("TEXT(\"" + Does.substr(3) + "\")") != std::string::npos;
		}
		Why = "a deed this test does not know";
		return false;
	}

	std::string Bare(std::string Word)
	{
		while (!Word.empty() && !WordChar(Word.back()))
		{
			Word.pop_back();
		}
		while (!Word.empty() && !WordChar(Word.front()))
		{
			Word.erase(Word.begin());
		}
		return Word;
	}

	std::vector<std::string> WordsOf(const std::string& Text)
	{
		std::vector<std::string> Out;
		std::string Now;
		for (const char C : Text + " ")
		{
			if (C == ' ')
			{
				if (!Now.empty())
				{
					Out.push_back(Now);
				}
				Now.clear();
			}
			else
			{
				Now += C;
			}
		}
		return Out;
	}

	bool Caps(const std::string& Word)
	{
		if (Word.size() < 2)
		{
			return false;
		}
		for (const char C : Word)
		{
			if (!std::isupper(static_cast<unsigned char>(C)))
			{
				return false;
			}
		}
		return true;
	}

	/** The keys a player reads in the words: SPACE, TAB, a lone digit, a lone capital (A and I only where the next word is
	 * not a lowercase one: "A unit" is a word, "W, A, S" a key). As EKeys names them. */
	std::vector<std::string> KeysNamed(const std::string& Text)
	{
		static const char* const Digits[] = { "Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine" };
		const std::vector<std::string> Words = WordsOf(Text);
		std::vector<std::string> Out;
		for (size_t K = 0; K < Words.size(); ++K)
		{
			const std::string W = Bare(Words[K]);
			const std::string Next = K + 1 < Words.size() ? Bare(Words[K + 1]) : std::string();
			if (W == "SPACE") { Out.push_back("SpaceBar"); }
			else if (W == "TAB") { Out.push_back("Tab"); }
			else if (W == "ENTER") { Out.push_back("Enter"); }
			else if (W == "ESC") { Out.push_back("Escape"); }
			else if (W.size() == 1 && std::isdigit(static_cast<unsigned char>(W[0]))) { Out.push_back(Digits[W[0] - '0']); }
			else if (W.size() == 1 && std::isupper(static_cast<unsigned char>(W[0])))
			{
				const bool bWord = (W == "A" || W == "I") && !Next.empty() && std::islower(static_cast<unsigned char>(Next[0]));
				if (!bWord)
				{
					Out.push_back(W);
				}
			}
		}
		return Out;
	}

	/** The screen's own words a player reads in the words: each run of capitals (keys aside), "HOW NEAR TO BREAKING OFF". */
	std::vector<std::string> NamesShown(const std::string& Text)
	{
		std::vector<std::string> Out;
		std::string Run;
		for (const std::string& Raw : WordsOf(Text + " ."))
		{
			const std::string W = Bare(Raw);
			const bool bKey = W == "SPACE" || W == "TAB" || W == "ENTER" || W == "ESC";
			if (Caps(W) && !bKey)
			{
				Run += (Run.empty() ? "" : " ") + W;
			}
			if (!Caps(W) || bKey || (!Raw.empty() && !WordChar(Raw.back())))
			{
				if (!Run.empty())
				{
					Out.push_back(Run);
				}
				Run.clear();
			}
		}
		return Out;
	}

	bool Has(const std::vector<std::string>& List, const std::string& Item)
	{
		for (const std::string& Each : List)
		{
			if (Each == Item)
			{
				return true;
			}
		}
		return false;
	}

	/** One note's keys and names against the screen: every key it names is in its table and does that deed, alone, on his
	 * screen; every table row is named; every screen word it names is drawn there. */
	void CheckKeys(const LandJson::Json& N, const std::vector<std::string>& G, const std::string& Screen)
	{
		const std::string Id = N["id"].Str("closing");
		const std::string Text = N["text"].Str("") + " " + N["deed"].Str("");
		const std::vector<std::string> Named = KeysNamed(Text);
		const LandJson::Json& Keys = N["keys"];
		std::vector<std::string> Table;
		for (size_t K = 0; K < Keys.Count(); ++K)
		{
			const std::string Key = Keys.At(K).At(0).Str(""), Does = Keys.At(K).At(1).Str("");
			Table.push_back(Key);
			std::string Why;
			const bool bDoes = DoesIt(G, Key, Does, Why);
			Check(bDoes, Id + ": " + Key + " does " + Does + " on his screen (" + (bDoes ? std::string("yes") : Why) + ")");
			Check(Has(Named, Key), Id + ": its table's key " + Key + " is named in its words");
		}
		for (const std::string& Key : Named)
		{
			Check(Has(Table, Key), Id + ": the key " + Key + " it names is in its table (what does it do?)");
		}
		const LandJson::Json& Shows = N["shows"];
		std::vector<std::string> Listed;
		for (size_t K = 0; K < Shows.Count(); ++K)
		{
			const std::string Shown = Shows.At(K).Str("");
			Listed.push_back(Shown);
			Check(Screen.find("TEXT(\"" + Shown + "\")") != std::string::npos, Id + ": \"" + Shown + "\" is drawn on his screen");
		}
		for (const std::string& Name : NamesShown(Text))
		{
			Check(Has(Listed, Name), Id + ": \"" + Name + "\" it names is in its shows");
		}
	}

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

	// ---- Every key and screen word a note names, against LandBattleScreen.cpp as it stands ----
	{
		const char* ScreenPath = Argc > 2 ? Argv[2] : "Source/TheaterCommand/Battle/LandBattleScreen.cpp";
		std::string Screen, Words;
		Check(LandJson::ReadFile(ScreenPath, Screen), std::string("his screen's source reads: ") + ScreenPath);
		const std::vector<std::string> G = GeneralKeys(LinesOf(Screen));
		Check(G.size() > 50, "his keys found in OnKeyDown (" + std::to_string(G.size()) + " lines)");
		LandJson::Json Root;
		std::string Why;
		Check(LandJson::ReadFile(Path, Words) && LandJson::Parse(Words, Root, Why), "the words read for their keys");
		const LandJson::Json& Notes = Root["notes"];
		for (size_t K = 0; K < Notes.Count(); ++K)
		{
			CheckKeys(Notes.At(K), G, Screen);
		}
		CheckKeys(Root["closing"], G, Screen);
		// (the test itself: a key bound to another deed fails, a key not in the table fails, a word not drawn fails)
		std::string Wrong;
		Check(!DoesIt(G, "H", "do:fallback", Wrong), "the test catches H named for R's deed");
		Check(!DoesIt(G, "One", "pace:4", Wrong), "the test catches 1 named for 3's pace");
		Check(!DoesIt(G, "E", "view:16", Wrong), "the test catches E named for Q's turn");
		Check(KeysNamed("A unit set down. W, A, S and D move. Press 2.") == std::vector<std::string>({ "W", "A", "S", "D", "Two" }), "the keys read out of words");
		Check(NamesShown("Press BEGIN THE BATTLE, or SPACE. FILM watches.") == std::vector<std::string>({ "BEGIN THE BATTLE", "FILM" }), "the screen's words read out of words");
	}

	// ---- The words: ruling 147's rule (30 words at most a note), one short deed line, the tip and the closing note ----
	Check(W.Notes.size() == 7, "seven notes");
	Check(W.Label == "STAFF NOTE", "the head row's word, as FIRST COMMAND's");
	Check(!W.OffLine.empty() && FieldNotes::CountWords(W.OffLine) <= 30, "NOTES OFF says where they come back");
	{
		std::string Ids;
		for (const FieldNotes::Note& N : W.Notes)
		{
			Ids += (Ids.empty() ? "" : " ") + N.Id;
		}
		Check(Ids == "ground look clock pick order decides break", "the notes' order: the pace before the first unit in hand (" + Ids + ")");
	}
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
		Off.bMapOffOwed = true;
		Check(FieldNotes::ReadBook(FieldNotes::WriteBook(Off)).bMapOffOwed, "the map's staff notes still owed off, kept across a quit");
		Check(!FieldNotes::ReadBook(FieldNotes::WriteBook(FieldNotes::Book())).bMapOffOwed && FieldNotes::WriteBook(FieldNotes::Book()).find("mapOff") == std::string::npos, "none owed: the notebook as before");
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
		Check(P.Up() == "clock", "the clock running: the pace first, before any unit is in hand (" + P.Up() + ")");
		P.Frames(2.0);
		Check(!P.Desk.Now().bStamped, "the scene's own pace is not his");
		P.F.Speed = 0.0;
		P.F.SpeedNow = 0.0;
		P.F.bOwnSpeed = false;
		P.Frames(1.0);
		Check(P.Desk.Now().bStamped, "PAUSE: stamped");
		P.Frames(3.5);
		Check(P.Up() == "pick", "then pick up a unit, the clock still held (" + P.Up() + ")");
		P.F.InHand = 1;
		P.Frames(1.0);
		Check(P.Desk.Now().bStamped, "one in hand: stamped");
		P.Frames(3.5);
		Check(P.Up() == "order", "then the order (" + P.Up() + ")");
		P.F.Orders = 1;
		P.Frames(1.2);
		Check(P.Desk.Now().bStamped, "an order: stamped");
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
		Check(P.Up() == "clock", "next (" + P.Up() + ")");
		P.F.bGeneral = false;
		P.F.SpeedNow = 0.0;
		P.Frames(3.0);
		Check(!P.Desk.Now().bStamped, "under the film: held");
		P.F.bGeneral = true;
		P.F.SpeedNow = 1.0;
		P.Desk.Later();
		Check(P.Up() == "-", "LATER puts it away");
		P.Frames(2.0);
		Check(P.Up() == "pick", "and the next comes in its place (" + P.Up() + ")");
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
		Check(Again.Up() == "clock", "a note put away with LATER comes in the next battle (" + Again.Up() + ")");
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
