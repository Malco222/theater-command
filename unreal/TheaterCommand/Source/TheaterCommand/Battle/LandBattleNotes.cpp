// Field notes on a live ground battle (release 2, -TheaterFieldNotes): ALandBattle's part. It begins the desk once his
// general's screen first comes up, reads the battle's facts for it each frame (his screen, the setup step, his view,
// his hand, his orders, the pace, the unit under his pointer, the clock), and writes the notebook out when a note is
// stamped or the notes are turned off. The note itself is drawn by SLandBattleScreen (FieldNotes.h has the rest).

#include "Battle/LandBattle.h"

#include "Battle/FieldNotesStore.h"
#include "Battle/LandBattleState.h"
#include "TheaterCommand.h"

namespace
{
	/** The desk's notebook into the game's one notebook (which may hold a tip seen since the battle began), written out. */
	void Keep(const FieldNotes::Book& Now)
	{
		FieldNotes::Book& Kept = FieldNotesStore::Book();
		for (const std::string& Id : Now.Done)
		{
			Kept.MarkDone(Id);
		}
		Kept.bOn = Now.bOn;
		Kept.bClosed = Kept.bClosed || Now.bClosed;
		FieldNotesStore::Save();
	}
}

void ALandBattle::TickFieldNotes()
{
	if (!State.IsValid())
	{
		return;
	}
	FLandBattleState& S = *State;
	FLandGeneralShown& G = S.General;
	if (!S.bFieldBegun)
	{
		if (!G.bOn || !FieldNotesStore::FeatureOn())
		{
			return;
		}
		S.bFieldBegun = true;
		// (he chose to fight one himself: the attack card's tip about it has done its work)
		FieldNotesStore::FightTipSeen();
		const FieldNotes::Words* Words = FieldNotesStore::Words();
		if (!Words)
		{
			return;
		}
		const FieldNotes::Book& Book = FieldNotesStore::Book();
		S.FieldDesk.Begin(*Words, Book, FieldNotesStore::Shown());
		int32 Done = 0;
		for (const FieldNotes::Note& N : Words->Notes)
		{
			Done += Book.IsDone(N.Id) ? 1 : 0;
		}
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): field notes %s (%d of %d done)"), Book.bOn ? TEXT("on") : TEXT("off"), Done, static_cast<int32>(Words->Notes.size()));
	}
	if (!S.FieldDesk.Showing())
	{
		return;
	}
	FieldNotes::Facts F;
	F.bGeneral = G.bOn && !G.bFilm;
	F.bBusy = S.bCard || S.bOver || G.bAllBackAsk || S.NightNow.bNight;
	F.bDeploying = G.Clipboard.Deploying();
	F.Placed = S.FieldPlaced;
	F.Orders = S.FieldOrders;
	if (S.bGeneralEye)
	{
		const auto Look = S.GeneralEye.LookAt();
		F.LookX = Look.X;
		F.LookY = Look.Y;
		F.Heading = S.GeneralEye.Heading();
		F.Zoom = S.GeneralEye.Zoom();
		F.bTopView = S.GeneralEye.TopView();
	}
	F.InHand = static_cast<int>(G.Clipboard.Chosen().size());
	F.Hover = S.FieldHover;
	F.Speed = S.Speed;
	F.SpeedNow = S.SpeedNow;
	F.bOwnSpeed = S.bOwnSpeed;
	F.Battle = S.Battle;
	F.Real = S.Real;
	const FieldNotes::Up Before = S.FieldDesk.Now();
	if (S.FieldDesk.Tick(F))
	{
		Keep(S.FieldDesk.TheBook());
	}
	const FieldNotes::Up& After = S.FieldDesk.Now();
	if (After.bShown && (!Before.bShown || Before.Index != After.Index || (After.bStamped && !Before.bStamped)))
	{
		if (const FieldNotes::Note* N = S.FieldDesk.Current())
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): field note \"%s\" %s at %.1f s"), UTF8_TO_TCHAR(N->Id.c_str()), After.bStamped ? TEXT("DONE") : TEXT("up"), S.Battle);
		}
	}
}

void ALandBattle::FieldNotesDo(const FString& Word)
{
	if (!State.IsValid())
	{
		return;
	}
	FieldNotes::Desk& Desk = State->FieldDesk;
	if (Word == TEXT("later"))
	{
		Desk.Later();
	}
	else if (Word == TEXT("off"))
	{
		if (Desk.Off())
		{
			UE_LOG(LogTheater, Display, TEXT("Land battle (live): field notes turned off"));
			Keep(Desk.TheBook());
			FieldNotesStore::OffInBattle();
			// (once, where they come back: the UI director 28 November 00:08Z)
			if (const FieldNotes::Words* Words = FieldNotesStore::Words(); Words && !Words->OffLine.empty())
			{
				State->Note = Words->OffLine;
				State->NoteAge = 0.0;
				State->NoteFor = 5.0;
			}
		}
	}
	else if (Word == TEXT("close"))
	{
		Desk.Close();
	}
}
