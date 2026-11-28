#pragma once

#include "CoreMinimal.h"
#include "Battle/FieldNotes.h"

/**
 * Field notes' engine side (release 2): the switch, the words file and the player's notebook (FieldNotes.h is the plain
 * part). One notebook for the whole game, read once and written whenever a note is stamped, a tip seen or the notes
 * turned off: the battle's desk and the campaign's attack card share it.
 *
 * - Shown only with -TheaterFieldNotes on the command line (the ship list's row "fieldnotes", OFF until Brian's Mac and
 *   the UI director pass it).
 * - Kept with the player in Saved/Campaign/field-notes.json, never with a save.
 * - A run (pictures, the playtester's steps, a scripted walk) keeps its own, Saved/Campaign/field-notes-runs.json, and
 *   starts with the notes off, so that no picture of a run that is not about them finds one on the screen:
 *   -TheaterFieldNotesRun starts it as a new general with them on, -TheaterKeepFieldNotes keeps what the last run left.
 *   The player's own notebook is never written by a run.
 */
namespace FieldNotesStore
{
	/** The ship list's word is on. */
	bool FeatureOn();
	/** The words, read once from Content/Data/Battle/field-notes.json; null where it cannot be read (no notes then). */
	const FieldNotes::Words* Words();
	/** The notebook, read once. */
	FieldNotes::Book& Book();
	/** Notes to be shown at all: the word on, the words read, and the notebook's own switch on. */
	bool Shown();
	/** The notebook written out (a run's to its own file). */
	void Save();
	/** FIRST COMMAND begun: the notes on, and all of them again where every one was done. */
	void ArmAgain();
	/** The Guide's staff notes switched on or off on the campaign map: the field's notes follow it (one paper, one
	 * switch), the done ones kept. */
	void SetOn(bool bOn);
	/** NOTES OFF pressed on a battle's note (the notebook already off): the staff notes are one switch, so the campaign
	 * map's go off too once it asks for its notes again (TakeOffAsked, once). */
	void OffInBattle();
	bool TakeOffAsked();
	/** The attack card's tip: its line while it is still to be seen ("" none). Asked each frame the card offers FIGHT IT
	 * YOURSELF: the card it was first shown on keeps it while it stays open; the next card to open finds it seen. */
	FString FightTip();
	/** A live battle he fights himself has begun: the tip has done its work. */
	void FightTipSeen();
}
