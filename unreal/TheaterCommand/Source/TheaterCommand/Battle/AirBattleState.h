#pragma once

#include "CoreMinimal.h"
#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirShow.h"

#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * The air battle scene's plain state (the flight core, the feed and the screen's words), kept out of AAirBattle's
 * header so that the reflection tool never reads the standard library's types. AAirBattle owns it; the battle screen
 * reads it to draw. Places are in board metres (X east, Y north, Z up), as the flight core's.
 */
struct FAirBattleState
{
	FAirBattleState() : Show(238) {}

	AirFeed::Sky Board;
	AirShow::Show Show;
	AirDesk::Desk Desk;
	AirDesk::Clock Clock;
	AirDesk::Words Words;
	AirDesk::Card Card;

	/** A recorded battle (Content/Data/AirBattles/<key>.json), played back: no orders are taken. */
	bool bReplay = true;
	AirFeed::Recording Recording;
	size_t NextView = 0;
	/** The battle time the show has been asked for, and the latest view the feed has handed it. */
	double BattleT = 0.0;
	double FedT = 0.0;
	std::string ClockTag;
	bool bOver = false;       // the battle's over line has been shown
	bool bCard = false;       // the after card is up
	double OverRealT = -1.0;  // real seconds since the over line was shown

	/** The groups in hand (his own), by id. */
	std::vector<int> Hand;
	/** The question standing: 0 none, 1 HAND IT TO THE STAFF, 2 BREAK OFF ALL. */
	int Question = 0;
	/** Each tag's place on the screen this frame, in the viewport's pixels; off the screen or behind the eye: absent. */
	std::map<int, FVector2D> TagAt;
	FVector2D ViewportPixels = FVector2D(1600.0, 1000.0);
	/** A line the screen shows for a moment (an order the battle would not take). */
	std::string Note;
	double NoteAge = 0.0;
	/** The close view is on (C), and what it rides. */
	bool bClose = false;
	AirDesk::Subject Subject;

	/** His side's own groups of one kind, as shown now. */
	std::vector<int> MineOf(bool bFighter) const
	{
		std::vector<int> Out;
		for (const AirDesk::Tag& T : Words.Tags)
		{
			if (T.bMine && !T.bGone && T.bFighter == bFighter) Out.push_back(T.Id);
		}
		return Out;
	}
	const AirDesk::Tag* TagOf(int Id) const
	{
		for (const AirDesk::Tag& T : Words.Tags)
		{
			if (T.Id == Id) return &T;
		}
		return nullptr;
	}
	bool HandHas(bool bFighter) const
	{
		for (int Id : Hand)
		{
			const AirDesk::Tag* T = TagOf(Id);
			if (T && T->bFighter == bFighter) return true;
		}
		return false;
	}
};
