#pragma once

#include "CoreMinimal.h"
#include "Async/ParallelFor.h" // (EParallelForFlags)
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CsvProfiler.h"

class UWorld;
class APlayerController;

/**
 * (Speed, 17 November 2026. Brian: "how do we make the game run more smooth, its not very easy and enjoyable to
 * play".) What the frame costs, and the switches for the changes made to bring that down.
 *
 * The log: "-TheaterSpeedLog" on the command line, or "tc.Speed.Log 1" at the console, writes a few lines every
 * five seconds: the frames a second, the worst frame and how many went over 33 ms, the game, draw and graphics
 * chip times, how long each of the map's jobs took a frame, and every call into the rules with what it cost. Every
 * thirty seconds it adds a count of what is drawn: how many parts, how many throw shadows, and the largest meshes;
 * and of what was on screen in that moment, kind by kind (land tiles, town squares, the pieces' parts and so on).
 * (18 November 2026.) A long frame is followed by "made in that frame": the tiles of ground stood up, the still models
 * handed to the draw thread and their points, the pieces whose shadow was switched, and how many fine sheets were being
 * modelled on other threads just then. Every five seconds "made:" has their sum, how the fine sheets stand (standing,
 * on the shelf, being modelled), and how many of the long frames fell while new ground was being modelled. Each fine
 * sheet says how long its modelling took. Two more jobs are timed: EngineClearOut (the engine's own clearing out of
 * what is no longer used, about once a minute) and StillModelHandOver (the still models made ready for the draw thread
 * at the frame's end).
 *
 * The same jobs are also timed for Unreal's own frame recorder ("-csvCaptureFrames=600", or "csvprofile start" and
 * "csvprofile stop" at the console), in its "Theater" columns, beside the engine's own game, draw and chip times.
 *
 * The switches, each a console variable so that "-TheaterTime=" can try them against each other in one run:
 *   tc.Speed.TownSquares  1 (default): the towns are cut into squares of country, so that only those in view are
 *                         drawn and only those near a change are drawn again into the shadows. 0: one piece, as before.
 *   tc.Speed.SmokeTrim    1 (default): only as much of the smoke's mesh as is in use is sent to the graphics chip
 *                         each frame. 0: all of it, as before.
 *   tc.Speed.RulesMemo    1 (default): the odds and the like, asked again of the rules for the same army and the
 *                         same place with nothing changed between, are answered from the last asking. 0: always asked.
 *   tc.Speed.OneEye       1 (default): the names, plates and lamps laid over the map work out the eye's view once for
 *                         all their points. 0: the controller works the whole view out again for every point, as before.
 *   tc.Speed.FineAside    1 (default): the smaller copies of a fine sheet's pictures are made on the thread that models
 *                         the sheet, not in the frame that dresses it. 0: in that frame, as before.
 *   tc.Speed.StillMeshes  1 (default): the hand-built models that are made once and then left alone (the ground's
 *                         tiles, the towns, the trees, the world's lines and works, the pieces' flags and turf) are
 *                         drawn through Unreal's kept drawing orders (UTheaterStillMesh). 0: their drawing orders are
 *                         made again every frame, for every pass and every shadow, as before.
 *   tc.Speed.HoldersLock  1 (default): ground changing hands no longer waits for the world's fine sheets that are still
 *                         being modelled on other threads (up to two seconds on the press, in America). Their few reads
 *                         of whose each province is are kept apart from the change by a lock held for a moment, and a
 *                         sheet that was in the making has the ground that changed painted again before it is shown.
 *                         0: the change waits for every sheet in the making to be finished, as before.
 *   tc.Speed.KeptHeights  1 (default): the points the frontiers, the front line and the enemy's reach are laid on are
 *                         worked out once and kept (ACampaignMapActor::SurfacePointKept), until a fine sheet stands up
 *                         or goes over them. 0: every point is worked out again each time a line is laid, as before.
 *   tc.Speed.FogAside     1 (default): when the haze moves, the staff's paper beyond the map (the world's picture of
 *                         it and each fine sheet's) is laid on another thread and put on the table one picture a frame
 *                         after, a frame or three later than before. 0: all laid in the frame of the press, as before.
 *   tc.Speed.RegionGrid   1 (default): the province at a place is looked for among those whose bounds reach into its
 *                         square of the map (FCampaignMapData::FindRegionAt), the same province as before. 0: every
 *                         province is looked at, as before.
 *   tc.Speed.RiverInks    1 (default): where the haze moves, the rivers' inks are darkened or lightened in place
 *                         (ACampaignMapActor::RecolourRivers). 0: every river is drawn again, as before.
 *   tc.Speed.FrontWeights 1 (default): when the front changes, the weight of the front line the eye is at is drawn,
 *                         and the other weight (the staff map's, or the working one) a frame after. 0: both at once.
 *   tc.Speed.DressSteps   1 (default): a fine sheet built on another thread is dressed (its pictures and paints made)
 *                         a little at a time, about 8 ms a frame and one sheet at a time, before its tiles are stood up
 *                         out of sight as ever. 0: every sheet that is ready is dressed whole in one frame, as before.
 *   tc.Speed.BordersKept  1 (default): the frontier and province lines' points are kept from one drawing to the next
 *                         (ACampaignMapActor::BuildBordersKept), laid again only where a fine sheet has changed the
 *                         ground under them, and only the kinds of line that changed are handed to the graphics chip
 *                         again. 0: every line of the world is laid and handed over again each time, as before. Needs
 *                         tc.Speed.KeptHeights. The same lines either way.
 *   tc.Speed.PictureOnce  1 (default): a picture that is read without smoothing (the lie of the land, the fields) is
 *                         made that way from the first. 0: it is made smoothed and then made again unsmoothed, as
 *                         before, and the second making waits for the drawing thread to catch up with the frames it
 *                         has in hand (DressPaint, about 32 ms of every fine sheet in the builder's logs). The same
 *                         picture either way.
 * (The second speed detective's, 18 November 2026: the whole world smooth.)
 *   tc.Speed.SheetShelf   6 (default): a fine sheet of the world the eye has left is stood down as before (after twenty
 *                         seconds, or one once the eye is above the height the sheets are for), but what was modelled
 *                         for it (its heights, paint, pictures) is kept on a shelf, up to this many sheets, the longest
 *                         unwanted dropped first. Wanted again, its tiles are stood up from the shelf in a few frames:
 *                         it is not modelled again (some seconds on every spare core, in the builder's logs of build
 *                         1634). The same picture either way. 0: dropped whole and modelled again, as before.
 *   tc.Speed.TileTime     4 (default): the tiles of ground stood up in one frame, over every sheet together, are stopped
 *                         for that frame once they have taken this many ms (one is always stood up; never more than
 *                         five a sheet, as before). The rest come the next frame; the tiles they replace stay until
 *                         then, as they always have. 0: five a sheet whatever they take, as before (30 ms in one frame
 *                         of the builder's sweep of build 1634).
 *   tc.Speed.QuietModel   0 (default, as before): a fine sheet's modelling spreads its loops over every spare core.
 *                         1: each sheet is modelled on its one thread, two sheets on two, and the other cores are left
 *                         to the frame; the fine ground comes later. 2: its loops are spread as before, but at the
 *                         background's priority. FOR MEASURING FIRST: whether the modelling is what makes the draw
 *                         thread's long frames while new ground comes up, and which of the two cures it at the least
 *                         wait for the ground ("Speed: fine sheet ... was modelled in ... s" says the wait). The same
 *                         ground whichever.
 * And two for measuring only, which change the picture and are not to be left off without the art director:
 *   tc.Speed.TownShadows  1 (default): the near towns throw shadows. 0: they do not.
 *   tc.Speed.TrainShadows 1 (default): the trains throw shadows. 0: they do not.
 */
CSV_DECLARE_CATEGORY_EXTERN(Theater);

namespace TheaterSpeed
{
	/** True while the speed log is being written (read once a frame, on the game thread). */
	bool IsLogging();

	/** Once a frame, from the player's controller: keeps the frame count, and writes the summary when it is due. */
	void NoteFrame(float DeltaSeconds, UWorld* World);

	/** A call into the rules has been answered: how long it took, and whether it was answered from the memo. */
	void NoteRulesCall(const FString& Function, double Seconds, bool bFromMemo);

	/** The switches above, as they stand now. */
	bool TownSquares();
	bool TownShadows();
	bool SmokeTrim();
	bool RulesMemo();
	bool TrainShadows();
	bool OneEye();
	bool FineAside();
	bool StillMeshes(); // (on any thread)
	bool HoldersLock();
	bool KeptHeights();
	bool DressSteps();
	bool FogAside();
	bool RegionGrid();
	bool RiverInks();
	bool FrontWeights();
	bool PictureOnce();
	bool BordersKept();
	int32 SheetShelf(); // how many fine sheets are kept on the shelf: 0 for none, as before
	double TileTime(); // the ms of tiles stood up in a frame: 0 for no limit, as before
	int32 QuietModel(); // 0 as before, 1 one thread a sheet, 2 the background's priority

	/**
	 * (tc.Speed.QuietModel.) Stood on a thread while it models a fine sheet quietly: the loops under it that would
	 * spread over the cores (ModelLoop) run on that thread alone (1), or spread at the background's priority (2).
	 * Elsewhere, and with the switch off, they spread as they always have.
	 */
	struct FQuietModelling
	{
		explicit FQuietModelling(int32 How); // (QuietModel's number, read on the game thread and brought here)
		~FQuietModelling();
		FQuietModelling(const FQuietModelling&) = delete;
		FQuietModelling& operator=(const FQuietModelling&) = delete;

	private:
		int32 Was = 0;
	};
	/** How a loop of a sheet's modelling is to run on this thread: spread over the cores as ever, or (under an FQuietModelling) as that asks. */
	EParallelForFlags ModelLoop();

	/**
	 * What was made in a frame, for the speed log (on any thread): a long frame is followed by a line of these, and
	 * every five seconds has their sum. Counted only while the log is written.
	 */
	enum class EMade : uint8
	{
		Tiles, // tiles of ground stood up (BuildLandTile)
		StillModels, // still models handed to the draw thread (a UTheaterStillMesh's proxy made)
		StillPoints, // ...and their points
		ShadowFlips, // pieces whose body began or stopped throwing a shadow (its model is handed over again)
		SheetsModelled, // fine sheets whose modelling ended
		SheetsShelved, // fine sheets stood down onto the shelf
		SheetsFromShelf, // ...and taken off it again
		SheetsDropped, // fine sheets dropped whole
		Count
	};
	void NoteMade(EMade What, int32 HowMany = 1);
	/** Once a frame, from the map: how many fine sheets are being modelled, stand, and lie on the shelf. */
	void NoteSheets(int32 Modelling, int32 Standing, int32 Shelved);

	/**
	 * (tc.Speed.HoldersLock.) Whose each province is (its holder, its lawful owner and the League's members) is changed
	 * only on the game thread, in ACampaignMapActor::SetHolders, and read on the threads that model the world's fine
	 * sheets as well. A reader stands an FHoldersRead while it reads them (its tables of who holds what, a moment's
	 * work, not the painting that follows); the change stands an FHoldersChange while it writes them. On the game
	 * thread a reader takes nothing, as nothing can change them there while it reads.
	 */
	struct FHoldersRead
	{
		FHoldersRead();
		~FHoldersRead() { Release(); }
		/** Done reading, before the end of the block. */
		void Release();
		FHoldersRead(const FHoldersRead&) = delete;
		FHoldersRead& operator=(const FHoldersRead&) = delete;

	private:
		bool bHeld = false;
	};
	struct FHoldersChange
	{
		/** Only where some sheet is being modelled: otherwise there is nobody to keep out. */
		explicit FHoldersChange(bool bNeeded);
		~FHoldersChange() { Release(); }
		void Release();
		FHoldersChange(const FHoldersChange&) = delete;
		FHoldersChange& operator=(const FHoldersChange&) = delete;

	private:
		bool bHeld = false;
	};

	/**
	 * The eye's view, worked out once and then used for as many points as are wanted: the same answer, to the bit, as
	 * APlayerController::ProjectWorldLocationToScreen(Point, Out, false), which works the whole view out again (the
	 * camera's settings copied, the lens and the screen's shape) for every single point. Make one where a job projects
	 * many points in one go, and not across frames: the camera moves. With tc.Speed.OneEye 0, or where the view cannot
	 * be had, each point is asked of the controller as before.
	 */
	struct FEye
	{
		explicit FEye(const APlayerController* InPlayer);
		bool Project(const FVector& World, FVector2D& OutScreen) const;

	private:
		const APlayerController* Player = nullptr;
		FMatrix ViewProjection = FMatrix::Identity;
		FIntRect Rect;
		bool bOwn = false;
	};

	/** (for THEATER_SPEED_SCOPE) */
	int32 SlotFor(const TCHAR* Name);
	void AddTime(int32 Slot, uint64 Cycles);

	/** Times what is left of the block it stands in, while the speed log is being written. */
	struct FScope
	{
		explicit FScope(int32 InSlot)
			: Slot(InSlot), Start(IsLogging() ? FPlatformTime::Cycles64() : 0)
		{
		}
		~FScope()
		{
			if (Start != 0)
			{
				AddTime(Slot, FPlatformTime::Cycles64() - Start);
			}
		}
		FScope(const FScope&) = delete;
		FScope& operator=(const FScope&) = delete;

	private:
		int32 Slot;
		uint64 Start;
	};
}

/** Times the rest of the block, under a name: in the speed log, and in the frame recorder's "Theater" columns. */
#define THEATER_SPEED_SCOPE(Name) \
	CSV_SCOPED_TIMING_STAT(Theater, Name); \
	static const int32 UE_JOIN(TheaterSpeedSlot_, __LINE__) = TheaterSpeed::SlotFor(TEXT(#Name)); \
	const TheaterSpeed::FScope UE_JOIN(TheaterSpeedScope_, __LINE__)(UE_JOIN(TheaterSpeedSlot_, __LINE__))
