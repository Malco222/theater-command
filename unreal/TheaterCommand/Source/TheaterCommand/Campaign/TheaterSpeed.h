#pragma once

#include "CoreMinimal.h"
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
 *   tc.Speed.DressSteps   1 (default): a fine sheet built on another thread is dressed (its pictures and paints made)
 *                         a little at a time, about 8 ms a frame and one sheet at a time, before its tiles are stood up
 *                         out of sight as ever. 0: every sheet that is ready is dressed whole in one frame, as before.
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
