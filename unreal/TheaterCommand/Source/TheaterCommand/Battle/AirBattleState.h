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
	/** The effects kit's room for falling debris (the kit's AirFall), and the scene's room for smoke trails. */
	AirShow::DebrisRoom Debris;
	AirShow::TrailRoom Trails;
	/** The day's sky (AirSky): where it is laid from, the clouds as laid, and their painter's work. */
	AirSky::Placing SkyPlace;
	AirSky::Field Sky;
	AirSky::Painter SkyPainter;
	std::vector<AirSky::Vec> SkyPoints;
	std::vector<uint8_t> SkyRgba;
	std::vector<AirSky::Vec> SkyShadows;
	std::vector<uint8_t> SkyShadowRgba;

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
	/** The tags as the screen last drew them, in the viewport's pixels: a click inside one takes its group. A block of
	 * tags that touched shows two and a line for the rest; that line is its first tag's. */
	struct FTagBox
	{
		int Id = -1;
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
	};
	mutable std::vector<FTagBox> TagBoxes;
	FVector2D ViewportPixels = FVector2D(1600.0, 1000.0);
	/** A line the screen shows for a moment (an order the battle would not take). */
	std::string Note;
	double NoteAge = 0.0;
	/** The close view is on (C), and what it rides. */
	bool bClose = false;
	AirDesk::Subject Subject;
	/**
	 * The flyover before the battle (Brian's, approved 19 November 2026): its shots, the real seconds into them, then
	 * the glide into the strategy view. While it is on the battle runs at x1 and the screen stands in a film's frame,
	 * black above and below, which draws back over the glide (FlyBars, 1 to 0).
	 */
	AirDesk::Flyover Fly;
	bool bFlying = false;
	bool bFlyTried = false;     // looked for once, when the battle's first groups stand
	double FlyT = 0.0;
	double GlideT = -1.0;       // real seconds into the glide; -1 before it
	double GlideLong = 2.5;     // 0.8 when a click or a key cut the shots short
	double FlyBars = 0.0;
	double FlyEndRealT = -1.0;  // the scene's real clock when it ended
	int FlyPictures = 0;        // (the picture run: the flyover's shots taken, one bit each)
	FVector GlideEye = FVector::ZeroVector;
	FRotator GlideFacing = FRotator::ZeroRotator;

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
