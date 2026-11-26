#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h" // (TFuture: ModelAside)
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
 * on the shelf, being modelled), how many of the long frames fell while new ground was being modelled, and how much
 * memory the game holds. Each fine
 * sheet says how long its modelling took. Two more jobs are timed: EngineClearOut (the engine's own clearing out of
 * what is no longer used, about once a minute) and StillModelHandOver (the still models made ready for the draw thread
 * at the frame's end).
 *
 * The same jobs are also timed for Unreal's own frame recorder ("-csvCaptureFrames=600", or "csvprofile start" and
 * "csvprofile stop" at the console), in its "Theater" columns, beside the engine's own game, draw and chip times.
 *
 * The switches, each a console variable so that "-TheaterTime=" can try them against each other in one run, and
 * "-TheaterSet=tc.Speed.SheetShelf=0,tc.Speed.QuietModel=1" can set any of them for a whole run of the step player:
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
 *   tc.Speed.SheetShelf   12 (default): a fine sheet of the world the eye has left is stood down as before (after twenty
 *                         seconds, or one once the eye is above the height the sheets are for), but what was modelled
 *                         for it (its heights, paint, pictures) is kept on a shelf, up to this many sheets, the longest
 *                         unwanted dropped first. Wanted again, its tiles are stood up from the shelf in a few frames:
 *                         it is not modelled again (some seconds on every spare core, in the builder's logs of build
 *                         1634). The same picture either way. 0: dropped whole and modelled again, as before.
 *                         (A sheet kept is 60 to 80 MB by the pre-checker's count of its arrays. Measured on the Mac
 *                         on 18 November, build 1680: with four and six on the shelf the game held 40 to 100 MB more
 *                         than without, about 15 to 20 MB a sheet. The shelf was 6 then; it is 12 now because the eye
 *                         at playing height has 12 sheets standing, and going up past their height and coming down
 *                         again modelled the other six afresh. The log's "made:" line says what the game holds.)
 *                         A sheet whose ground changed hands while it lay on the shelf has that ground painted when it
 *                         comes off, one such sheet a frame.
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
 *                         (Measured 18 November, build 1680: neither 1 nor 2 helped. While a sheet is modelled the
 *                         game, draw and RHI threads each take about twice as long over the same work, and a frame is
 *                         ten times as likely to run over 33 ms, with the loops spread, single or at the background's
 *                         priority alike: the modelling thread itself is in the frame's way.)
 *                         3: each sheet on a thread of its own at the engine's lowest priority, its loops on that
 *                         thread. 4: as 3, and on a Mac the thread is marked as background work, which Apple's chips
 *                         keep to their efficiency cores, leaving the fast ones to the frame. STILL FOR MEASURING.
 *   tc.Speed.FontRoom     1 (default): Unreal is allowed eight pages for the letters it has cut (Slate's font cache;
 *                         Slate.MaxFontAtlasPagesBeforeFlush), where it had one and threw every letter away each time a
 *                         second was wanted: a 25 to 45 ms frame about ten times in a two-minute journey (the same
 *                         capture). Read once, as the table is laid. 0: one page, as before. The same letters either way.
 *   tc.Speed.StandOne     1 (default): one ready fine sheet is stood a frame, and the next the frame after. Standing a
 *                         sheet stands the coarse sheet's tiles round it afresh and hands them and its own to the draw
 *                         thread: 25 to 30 ms, and two stood in one frame made a 45 ms one (the same capture). 0: every
 *                         ready sheet in the same frame, as before. The same picture either way.
 *   tc.Speed.ShowSpread   80000 (default): when the eye crosses a height at which the towns change form (10,800 and
 *                         11,600) or the trees come and go (7,200), the squares newly shown are handed to the draw
 *                         thread that many points a frame, the nearest to the eye's point first and one always, and the
 *                         rest the frames after. All 105 squares of towns went in the one frame: a million points and
 *                         50 to 64 ms on every zoom through that height (the builder's capture 2 of 18 November, build
 *                         1684, where the log first named them). 0: every square in the same frame, as before. The same
 *                         picture either way; the far squares come up to a fifth of a second later.
 *   tc.Speed.TidyAside    1 (default): Unreal clears out what is no longer used every five minutes, and at once behind
 *                         any pause of a quarter of a second the game already has (a load, a month's end), where it
 *                         did every 61.1 s in one frame of 40 to 63 ms wherever the eye was: eleven such frames in the
 *                         five runs of the same capture. Read once, as the table is laid (gc.TimeBetweenPurgingPending-
 *                         KillObjects). 0: every 61.1 s, as before. The same picture either way. A test's run
 *                         ("-unattended") keeps its 61.1 s, which is what finds an object held too loosely, and is
 *                         cleared out behind its loads as well; a timing run asks for the five minutes by -TheaterSet.
 *                         When the game holds half a gigabyte over its least since the last clear-out, one is owed and
 *                         the next pause is taken for it whenever the last was; only at two gigabytes over is it asked
 *                         for in the open (mended after capture 3, where half a gigabyte asked in the open and a list
 *                         of loads was cleared out every half minute, in frames of 97 and 138 ms).
 * (Release 2, map run 7, 22 November 2026: the month's turn and the Y key's sides view.)
 *   tc.Speed.MonthSpread  1 (default): the month's turn is spread. The season's masks picture is sent again only when
 *                         the winter's hardness has changed (in summer it had not, and the whole picture and its smaller
 *                         copies went again every month); where the snow's weight on occupied ground moves, only the
 *                         box of the provinces it moved on is painted and sent, as ground changing hands is, not the
 *                         world; and the rest of the table (works, scars, the front line, the haze and so on) and the
 *                         campaign's monthly save come a frame and two frames after the new holders and the armies.
 *                         0: all of it in the month's frame, as before. The same picture either way.
 *   tc.Speed.SidesKept    1 (default): the sides view (the Y key's third view) is laid once on a mesh of its own and
 *                         kept while it is put away; shown again it is only recoloured, province by province, where the
 *                         sides have changed. It was laid afresh over the whole world on every press (ShowSides 211 ms
 *                         in run 7). 0: laid afresh every time, as before. The same picture either way.
 *   tc.Speed.SidesFade    0.18 (default): with SidesKept, the sides view's wash fades in and out over this many seconds,
 *                         a long frame counted as a thirtieth, so the step is never a jump even where a frame stalls.
 *                         0: at once, as before.
 * (Release 2, map run 8, 22 November 2026: what was left of the month's frame.)
 *   tc.Speed.TreesSpread  6 (default): the woods' turn for the month (SeasonTrees, about 155 ms in a month that changes
 *                         the leaves) is planted a few squares a frame, about 6 ms of them, the squares in sight first
 *                         and then the nearest; a square out of sight is not drawn, so it can wait. 0: every square in
 *                         the month's frame, as before. The same woods either way.
 * (Release 2, the UI director's bar of 22 November 2026: every press answers within a tenth of a second.)
 *   tc.Speed.CardAside    1 (default): the attack card stands the moment the army is ordered at enemy ground, named
 *                         and in the two sides' colours, the staff at work on the odds; its forecast (previewAttack,
 *                         the staff's widening with it) is worked out on the rules' own thread, as an order's answer is,
 *                         and fills the card a frame or a few later. 0: the forecast in the press's frame, as before.
 *                         The same card either way.
 *   tc.Speed.WarSpread    1 (default): a war begun or continued from the opening screen puts a plate over the screen at
 *                         once; the rules start it a few frames on, and the table is laid under the plate a step a
 *                         frame (the land, the towns, the holders, the armies, the works, the season, the war's marks,
 *                         the sides view), then the war is entered. In map run 8 the first laying took 1.2 to 1.4 s
 *                         inside a 2.2 to 2.5 s frame. 0: all in the press's frame, as before. The same table either way.
 *   tc.Speed.SaveAside    1 (default): the month's own save (the war written out each month, some 2.3 MB of text by
 *                         August 1944, 9 to 11 ms of a frame in February 1941 in map run 8 and about twice that by 1944)
 *                         is written out on the rules' own thread, as an order's answer is, and onto the disk on a worker
 *                         thread, a frame or a few later. A save at once (the opening screen's, on leaving the game)
 *                         waits for one still being written, and a save still out is never written over a later one.
 *                         0: all in the month's frame, as before. The same file either way.
 *   tc.Speed.NationsAside 1 (default): the opening screen's list of countries (TC.grand: a later year is history
 *                         played up to it with the new dice, 0.7 to 1 s for January 1941 in a cloud run, and more for
 *                         each year later) is worked out on the rules' own thread, as an order's answer is, while the
 *                         screen says so; it is asked when a year is picked and every time the screen opens, Escape
 *                         from the war included, where it was worked out in the press's frame. 0: in the press's
 *                         frame (a year picked: three frames after it), as before. The same list either way.
 *   tc.Speed.MonthAside   0 (default, until the Mac has timed it): the other nations' months, played when he ends his
 *                         (TC.endMonth: 0.35 to 0.8 s on the Europe-and-Americas chart and 1.0 to 1.7 s on the whole-world
 *                         map in a cloud run, the far map's answer some 1.9 MB of text) are played and read on the rules'
 *                         own thread, as an order's answer is, so the table's frames go on while they are; the bell rings
 *                         at the press as before, and the watch or the month's card follows once they are in. The table
 *                         takes no order meanwhile (IsAnswering), and a step run waits for it (IsBusyForSteps). A run that
 *                         takes pictures plays it at once. 1 on; 0 in the press's frame, as before. The same month either way.
 *   tc.Speed.PicturesAside 1 (default): a card's unit pictures (the arsenal's, a province's raise list, the bigger
 *                         unit card, a page's machines, a fleet's ships) that have not been taken yet are each a model
 *                         built and photographed in the studio, and a card opened for the first time took all of them in
 *                         the press's frame. The card now stands at once with each picture's place kept at its size, and
 *                         the pictures are taken one a frame after it (PortraitOwed in the log), each appearing in its
 *                         place. A run that takes pictures takes them at once. 0: all in the press's frame, as before.
 *                         The same pictures either way, a few frames later.
 * And one that is off until a run on the Mac says otherwise:
 *   tc.Speed.FrameCap     0 (default): the frames are free, as before. A cool Mac draws 80 to 97 pictures a second
 *                         for a screen that shows 60 (capture 4, runs K to N), and the chip's work for the rest is only
 *                         heat, on a Mac with no fan. 20 to 240: the game holds itself to that many frames a second
 *                         (t.MaxFPS, set by code: a value set by hand or by -TheaterSet stays as it is). -1: each frame
 *                         waits for the screen's own refresh instead (r.VSync 1). Seen at once; set back to 0, both are
 *                         put back as they stood. The same picture either way. A capped game sleeps between frames, and
 *                         macOS stretches those sleeps when the screen is dark or the window hidden (capture 4's void
 *                         run Q, 21 to 26 frames a second): so a timing with a cap wants a lit screen and the window in
 *                         front, and the log's line on held frames (below) says whether it had them.
 * And the log's line on held frames, each five seconds: the cap in force, how long the engine waited each frame to
 * keep to it and by how much its waits overran what it asked, and in how many frames the game's window was in front.
 * And the log's line on the machine itself, each five seconds, on a Mac: the system's own word on its heat (nominal,
 * moderate, heavy, trapping, sleeping: a Mac with no fan slows its own chip when it is hot) and how many of the chip's
 * cores the game and the other programs kept busy. Capture 3's four runs were on a Mac that slowed part way through
 * the first, and nothing in the log said so but the chip's time for the same view.
 * And two for measuring only, which change the picture and are not to be left off without the art director:
 *   tc.Speed.TownShadows  1 (default): the near towns throw shadows. 0: they do not.
 *   tc.Speed.TrainShadows 1 (default): the trains throw shadows. 0: they do not.
 */
CSV_DECLARE_CATEGORY_EXTERN(Theater);

namespace TheaterSpeed
{
	/** True while the speed log is being written (read once a frame, on the game thread). */
	bool IsLogging();

	/** ("-TheaterSet=name=value,...") Sets the console variables the command line names, once: as the table begins to be laid. */
	void ApplyCommandLineSets();

	/** Once a frame, from the player's controller: keeps the frame count, and writes the summary when it is due. */
	void NoteFrame(float DeltaSeconds, UWorld* World);
	/** The last five seconds the log closed (its line "Speed: N s, F frames a second (worst frame W ms ..."): false before
	 * the first. For a reader that prints them beside its own counts, so that there is one meter (the air battle's
	 * timing eye, -TheaterAirTimeEye, which turns the log on). Game thread. */
	bool LastWindow(double& Seconds, double& FramesASecond, double& WorstMs);

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
	int32 QuietModel(); // 0 as before, 1 one thread a sheet, 2 the background's priority, 3 its own lowest thread, 4 and background work on a Mac
	bool StandOne();
	int32 ShowSpread(); // the points of towns' and trees' squares newly shown in a frame: 0 for no limit, as before
	bool MonthSpread();
	bool SidesKept();
	float SidesFade(); // (tc.Speed.SidesFade) the seconds the sides view fades in and out over: 0 at once, as before
	double TreesSpread(); // (tc.Speed.TreesSpread) the ms a frame the woods' month turn may take: 0 all in the month's frame, as before
	bool CardAside(); // (tc.Speed.CardAside) the attack card's forecast asked on the rules' thread: false in the press's frame, as before
	bool WarSpread(); // (tc.Speed.WarSpread) a war begun is laid a step a frame under a plate: false all in the press's frame, as before
	bool SaveAside(); // (tc.Speed.SaveAside) the month's save written out off the game thread: false in the month's frames, as before
	bool NationsAside(); // (tc.Speed.NationsAside) the opening screen's countries worked out on the rules' thread: false in the press's frame, as before
	bool MonthAside(); // (tc.Speed.MonthAside) the other nations' months played and read on the rules' thread: false in the press's frame, as before
	bool PicturesAside(); // (tc.Speed.PicturesAside) a card's unit pictures not yet taken are taken one a frame after it stands: false all in its frame, as before
	/**
	 * A run that only takes pictures ("-TheaterShots=<folder>" with no "-TheaterSteps="): the photographer arranges each
	 * picture the moment it asks, so the switches above that spread a press over frames (WarSpread, NationsAside,
	 * PicturesAside) keep it at once there. A step run puts its pictures in a -TheaterShots= folder too, but its step
	 * player waits while the table is busy (IsBusyForSteps, a war being laid or a list being worked out among it), so it
	 * plays them as a player meets them: the Mac's A/B of each switch is a step run. (Map run 9, 23 November: with this
	 * read as any -TheaterShots= run, WarSpread's B laid the war at once and could not be measured.)
	 */
	bool PictureRun();

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
	 * (tc.Speed.QuietModel.) Sets a fine sheet's modelling going aside and returns the future that says when it is
	 * done: on the engine's pool as before (0, 1, 2), or on a thread of its own at the lowest priority (3, 4).
	 */
	TFuture<void> ModelAside(int32 How, TUniqueFunction<void()> Work);

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
	/** A still model of this kind ("LandTile", "TreeSquare") was handed to the draw thread, with so many points: a long frame's line names the kinds. */
	void NoteStillKind(FName Kind, int32 Points);
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

	/**
	 * (Release 2, the far side: the art director's seam-2, 25 November 2026.) The whole world's chart goes round, and
	 * what the screen draws at a place on the table (a town's plate, a country's name, a badge over a harbour) is given
	 * at the place's own turn of the world, counted from the chart's seam at 141 W. Looked at from the other side of
	 * the seam, that is a whole world from the eye: California had no name from over Hawaii. Told where the eye looks
	 * along the table's east (the world's +Y) and how far round the world is, FEye::Project counts every point in the
	 * turn of the world nearest the eye. Span 0 (a table with edges, and every table until this is called) leaves each
	 * point as it is given, and the answer the same to the bit. The map tells it every frame (UpdateForCamera).
	 */
	void RoundWorld(double EyeAlong, double Span);

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
