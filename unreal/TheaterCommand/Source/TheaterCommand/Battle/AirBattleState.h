#pragma once

#include "CoreMinimal.h"
#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirLetters.h"
#include "Battle/AirNoseArt.h"
#include "Battle/AirShow.h"
#include "Battle/AirUnitColours.h"

#include <map>
#include <memory>
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
	/** The clouds in two rooms: the near clouds turned as the eye moves, the far ones when it has moved far enough to
	 * show, with their shadows (AirSky::Rooms; the Mac's Midway stutter, 19 November). */
	AirSky::Rooms SkyRooms;
	/** The field as laid, the painter's own copy: a turn painted aside reads only this, so it shares nothing with the
	 * frame (a deck keeps a memo of its tiles' lifts, which the flight core's falls ask every frame). */
	AirSky::Field SkyAside;
	/** GS6: the clouds turned on the graphics chip (AAirBattle::LaySky; none when the rooms turn them, as with
	 * "-TheaterSkyChip=0"): their table, laid on the game's thread from SkyAside, and the painter of their shadows
	 * alone, which lays them in SkyRooms's Shadows as the far room would. */
	std::shared_ptr<AirSky::Chip> SkyChip;
	AirSky::Painter SkyShade;

	/** A recorded battle (Content/Data/AirBattles/<key>.json), played back: no orders are taken. */
	bool bReplay = true;
	/** (Release 2, the scramble offer) An enemy raid he meets himself (TC.scrambleStart): a failed end says his staff
	 * meets it, as the month then does, not that FLY THE RAID still sends his crews. */
	bool bScramble = false;
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
	/** In the close view, the box the ridden aircraft fills on the screen this frame, in the viewport's pixels (no tag
	 * and no dispatch covers it); bRiddenOn false when there is none. */
	FVector2D RiddenMin = FVector2D::ZeroVector, RiddenMax = FVector2D::ZeroVector;
	bool bRiddenOn = false;
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
	/** The chase view: the squadron he chose to ride (a double click on its tag, a click on one in the close view, or a
	 * step), which the view keeps to, stepping on to the next of its aircraft as one goes down; -1 while the close view
	 * picks its own (the group in hand, else the fight). */
	int RideGroup = -1;
	/** The squadron of the aircraft ridden now, -1 when none. */
	int RiddenGroup() const
	{
		for (const AirShow::PlaneOut& P : Show.Planes())
		{
			if (P.Key == Subject.Plane) return P.Group;
		}
		return -1;
	}
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
	/**
	 * (1.5.1, the opening held: the Mac's cold Rhineland raids, 24 November) The raid's first frames, while the draw
	 * thread takes in the scene just made (about a second a frame for two frames, every raid), are drawn behind a black
	 * frame with the raid's title, the clock held; it lifts once the frames come quick again (AAirBattle::WarmTick).
	 * Curtain 1 is all black, easing to 0 as the flyover begins.
	 */
	bool bWarming = false;
	double Curtain = 0.0;
	int FlyPictures = 0;        // (the picture run: the flyover's shots taken, one bit each)
	FVector GlideEye = FVector::ZeroVector;
	FRotator GlideFacing = FRotator::ZeroRotator;

	/** The Battle rules card's "Finer B-17" (rule "b17Fine", release 2), read as the battle began (AAirBattle::B17FineRead):
	 * -1 not yet read, 0 off (the older B-17, Content/Models/B17.json), 1 on (B17Fine.json). */
	int B17FineOn = -1;

	/** The Battle rules card's "Turning airscrews" (rule "flyPropSpin", release 2), read when the battle's first shape is
	 * made (AAirBattle::PropSpinOn): -1 not yet read, 0 off, 1 on. Looks only: nothing the rules count reads it. */
	int PropSpinOn = -1;

	/** The Battle rules card's "Bomber markings" (rule "flyBomberMarks", release 2; AirBattleLetters.cpp), read once a
	 * battle as its shapes are first made ready (AAirBattle::LettersOnNow): -1 not yet read, 0 off, 1 on; the markings'
	 * sheet (Content/Data/AirMarks/b17-fine-marks.json), read then; and which letters each finer B-17 wears. Looks only:
	 * nothing the rules count reads it, and it never reads the date. */
	int LettersOn = -1;
	AirLetters::Sheet LettersSheet;
	AirLetters::Book Letters;

	/** The Battle rules card's "B-17 paint sheen" (rule "flyB17Sheen", release 2), read as the battle began, after "Worn
	 * B-17 paint" (AAirBattle::B17SheenRead), and set back to 0 if M_AirPaint is missing or older than the build:
	 * -1 not yet read, 0 off, 1 on. Looks only: nothing the rules count reads it. */
	int B17SheenOn = -1;

	/** The Battle rules card's "Silver Mustang" (rule "flyMustangMetal", release 2), read as the battle began
	 * (AAirBattle::MustangMetalRead), and set back to 0 if her file or M_AirPaint is missing or M_AirPaint is older than
	 * the build: -1 not yet read, 0 off, 1 on. Looks only: her flight is the Mustang's; nothing the rules count reads it. */
	int MustangMetalOn = -1;
	/** The Battle rules card's "Far airscrews a faint blur" (rule "flyPropBlur", release 2), read with PropSpinOn
	 * (AAirBattle::PropBlurOn): -1 not yet read, 0 off, 1 on. Looks only: nothing the rules count reads it. */
	int PropBlurOn = -1;

	/** The bombers' nose art (AirBattleNoseArt.cpp, release 2): its switch as read when the battle's first frame was drawn
	 * (-1 not yet read, 0 off, 1 on); the list and the panels' text (Content/Data/NoseArt), read then; each model's panel
	 * once asked for (Cols 0: none); whether each model the battle names has one; and which painting each aircraft wears. */
	int NoseArtOn = -1;
	bool bNoseArtRead = false;
	AirNoseArt::Index NoseArtList;
	std::string NoseArtPanelsText;
	/** (the finer B-17's own panel, Content/Data/NoseArt/panels-fine.json, read with the rest only while B17FineOn) */
	std::string NoseArtFinePanelsText;
	std::map<std::string, AirNoseArt::Panel> NoseArtPanels;
	std::map<std::string, bool> NoseArtModels;
	AirNoseArt::Book NoseArt;

	/** The groups' earned colours (AirBattleUnitColours.cpp, release 2): its switch as read when the battle's first
	 * aircraft was made (-1 not yet read, 0 off, 1 on); the art director's palette (Content/Data/Air/unit-colours.json),
	 * read then; each model's file and the regions found on it, read when a group first wears colours on it (a model whose
	 * file does not read is kept empty, and flies plain); the look being laid; and how many looks were written to the log. */
	int UnitColoursOn = -1;
	AirUnitColours::Palette UnitColoursPalette;
	struct FUnitColoursModel
	{
		bool bRead = false;
		AirUnitColours::Model Model;
		AirUnitColours::Found Found;
	};
	std::map<std::string, FUnitColoursModel> UnitColoursModels;
	AirUnitColours::Look UnitColoursLook;
	std::string UnitColoursKind;
	std::set<std::string> UnitColoursLogged;

	/** The effects kit's air parts of 26 November (Brian: more of a film's dramatic detail in the battles), each behind its
	 * row on the Battle rules card, read once as the battle opens, before its kits begin (AirBattle.cpp's AirRowsRead): -1 not
	 * yet read, 0 off (every frame as before), 1 on. OilFilmOn, the row "flyOilFilm": a strike's Part (fuel, oil, coolant)
	 * handed to the kit's AirHit, and battle-fx-air-spurt.json laid on both kits. Looks only. */
	int OilFilmOn = -1;
	/** VapourOn, the row "flyVapour": the kit's contrails (VapourMove, VapourStop) for every aircraft over the day's trail
	 * height, near shapes and far copies alike, and battle-fx-air-vapour.json laid on both kits (AAirBattle::Draw).
	 * VapourKeys: the aircraft the kit was told trail on the last frame drawn (sorted), rebuilt every frame from that frame
	 * alone (VapourWas the frame before, kept only to spare an allocation), so a clock sought back or a battle closed leaves
	 * nothing stale: a key that trails no longer is stopped once. VapourEngines: each model's engines in its own frame
	 * (AirShow::ShapeOf), asked once a battle (its B-17 follows the row b17Fine, read as the battle began). */
	int VapourOn = -1;
	std::vector<int> VapourKeys;
	std::vector<int> VapourWas;
	std::map<std::pair<std::string, bool>, std::vector<AirShow::Vec>> VapourEngines;

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
