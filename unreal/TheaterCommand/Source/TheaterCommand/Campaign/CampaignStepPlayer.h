#pragma once

#include "CoreMinimal.h"

class ACampaignPlayerController;
class AAirBattle;
class ALandBattle;
class SLandBattleScreen;
struct FKey;

/**
 * A development tool, not part of the game. Started with "-TheaterSteps=<file>" beside "-TheaterShots=<folder>", it
 * plays the playtester's step file in the picture window: every step is done as the player's own hand would do it
 * (a click on a province, a right-click with what is in hand, a press on a button found by its words), and for
 * every step it writes one line of steps.jsonl and a picture. The same file is played on the browser game, which
 * writes the same log: laid side by side, a step that works there and fails here is a fault of this table.
 *
 * (Brian, 17 November 2026: "i want the playtester to really click everything to make sure it works in the 3d game
 * too". The rule every step is held to: a click does something, or the game says why not.)
 *
 * An air battle he flies himself covers the map, and the map's keys do not reach it. Two verbs play it as his hand on
 * the battle screen does (release 1.5's Mac runs, Part B):
 * - "air-key WHAT": a key of the air battle screen. continue (Return on the after card; KEEP FIGHTING on a question),
 *   chase (C: the chase view on or off), next / prev / up / down (the arrows in the chase view: along the squadron, or
 *   to the last or next squadron), pause / play / space (Space), x0.5 / x1 / x2 / x4 (1 to 4), skip, escape. As on the
 *   keyboard, any key pressed during the opening flyover only skips it.
 * - "air-wait WHEN": the picture waits on the battle. A number is the battle's clock (212: battle second 212); a number
 *   and s is real seconds from the line (20s); flyover waits for the opening flyover to end; card for the after card. A
 *   wait for seconds ends early if the after card comes. At most 15 minutes. Its frames are timed as any step's, so a
 *   wait reads the battle's frame rate over that span.
 * Neither waits for the table to be still first (the raid keeps it busy until it is handed back), and the press that
 * opens the raid (FLY IT YOURSELF) takes its picture a second in, in the flyover. The picture is the battle screen.
 *
 * A land battle he fights himself (FIGHT IT YOURSELF) covers the map too. Three verbs play it with his own keys and
 * clicks, sent through Slate (FSlateApplication's key and mouse events), so the battle screen's own handlers take them
 * (release 1.5's ALL FALL BACK checks, the release lead 23 November 19:11Z):
 * - "land-key KEY": L, Esc, H, G, R, F, X, K, T, B, Tab, C, V, W / A / S / D, Q / E, the arrows, Space, Enter, 1 to 3.
 *   The view's keys (W A S D, Q E, the arrows) are held a second, as a hand holds them, and let up before the picture.
 * - "land-click WHAT": a button by its words (ALL FALL BACK: the question's own where it is up, else the bar's; KEEP
 *   FIGHTING; FILM; CONTINUE; or action:<what it does> for any other), "field" (the left button mid-screen),
 *   "field-right" (the right button there) or "drag" (a box dragged across the middle with the left button).
 * - "land-wait WHEN": battle seconds (150), real seconds from the line (20s), "begun" (the night and the placing over),
 *   "contact" (the armies met) or "card" (the after card). At most 15 minutes.
 * Each logs what the screen holds as its picture is taken: the battle second and pace (or paused), whether ALL FALL
 * BACK's question is up, whether the army is falling back, and what the step changed: an order sent, the hand, the
 * view moved, the pace. Expect words for them: does, nothing (no order, hand, question, film or pace changed), asks (the
 * question up), closed (no question and not falling back), falls-back, view, paused, running, order (an order went),
 * card (the after card).
 */
class FStepPlayer
{
public:
	/** Reads the step file. False where there is none, or nothing in it. */
	bool Load(const FString& File, const FString& InFolder);
	/** One frame of the run. False when the last step has been played and logged. */
	bool Tick(ACampaignPlayerController& Player, float DeltaSeconds);

private:
	struct FStep
	{
		int32 Line = 0;
		FString Verb, Target, Expect;
		bool bReplay = false; // played again after a restore, to put the card or the army back: not logged
		bool bSweep = false; // one press of a "press-each"
	};
	/** What the game holds and shows at a moment. */
	struct FSeen
	{
		FString Open, Words, Hand, Digest, Holder;
		int64 Stamp = 0;
		int64 Bare = 0; // the same reading with no province named
		int32 Notices = 0; // how many sentences the paper line had been given
	};

	FSeen Look(ACampaignPlayerController& Player, int32 RegionId) const;
	void Act(ACampaignPlayerController& Player, const FStep& Step);
	void Log(ACampaignPlayerController& Player, const FStep& Step);
	void Sweep(ACampaignPlayerController& Player, const FStep& Step);
	bool Press(ACampaignPlayerController& Player, const FString& Words);
	void ActAir(ACampaignPlayerController& Player, const FStep& Step);
	/** (air-wait) True while what the step waits for has not come. */
	bool AirWaiting() const;
	/** (an air step) The battle as the picture is taken, for the log's note. */
	FString AirNow() const;
	/** The land battle's field as a step reads it (a land step). */
	struct FLandSeen
	{
		bool bThere = false, bAsk = false, bBack = false, bFilm = false, bCard = false, bNight = false, bSetup = false;
		double Battle = 0.0, Speed = 0.0, Ping = -10.0;
		int32 Hand = 0;
		double Eye[3] = { 0.0, 0.0, 0.0 }, LookAt[3] = { 0.0, 0.0, 0.0 };
		FString Armed, Said, Held; // Held: the units in hand, by number (Tab moves the hand without changing its size)
	};
	void ActLand(ACampaignPlayerController& Player, const FStep& Step);
	void LandKey(SLandBattleScreen& Screen, const FString& Low);
	void LandClick(SLandBattleScreen& Screen, const FString& Low);
	/** One press of a mouse button on the battle screen, from one point to another (the same point for a click). */
	void LandMouse(SLandBattleScreen& Screen, const FKey& Button, const FVector2D& FromLocal, const FVector2D& ToLocal);
	FLandSeen LandLook() const;
	/** (land-wait) True while what the step waits for has not come. */
	bool LandWaiting() const;
	/** (a land step) The field as the picture is taken, and what the step changed, for the log's note. */
	FString LandNow(const FLandSeen& Now) const;

	TArray<FStep> Steps;
	FString Folder;
	double ActBegan = 0.0, LastTickAt = 0.0; // the clock at the press, and at the last frame counted
	int32 LookFrames = 0; // frames let go by after the hook's own reading, before the press
	// What the step's name was found to be, before the press was timed.
	bool bFound = false;
	FString FoundName;
	int32 FoundRegion = INDEX_NONE, FoundSea = INDEX_NONE, FoundCorps = INDEX_NONE, FoundCorpsRegion = INDEX_NONE, FoundFleet = INDEX_NONE, FoundGroup = INDEX_NONE;
	FString StepFolder; // where the step file lies: a saved game named in it is looked for here
	int32 At = 0, Phase = 0, Logged = 0;
	float Clock = 0.0f;
	FSeen Before;
	int32 TargetRegion = INDEX_NONE; // the province the step names, for its holder in the digest
	bool bMissing = false, bMute = false, bError = false, bSaidWhy = false, bStaged = false, bMonth = false;
	FString Note;
	TMap<FString, FString> Saves; // the snapshots, by name
	TArray<FString> FrameButtons; // the buttons that stand on the bare table: a sweep leaves them alone
	TArray<FStep> Context; // the last steps that opened a card or put something in hand: replayed after a sweep's restore
	FString SweepSave;
	TArray<FStep> SweepContext;
	bool bSweepDirty = false;
	// How smooth the step was: how long the order itself held the game, when the first result showed and when the
	// screen last changed (milliseconds from the input), every frame's length, and the game's, the drawing's and the
	// graphics chip's share of a frame.
	double ActMs = 0.0, FirstMs = -1.0, LastMs = -1.0, SinceAct = 0.0;
	TArray<float> Frames;
	double GameMs = 0.0, DrawMs = 0.0, ChipMs = 0.0;
	FString ShownBefore, ShownLast;
	TArray<FString> Screens; // (a month's end) each screen that came up on the way
	bool bPan = false; // ten seconds of pan and zoom, for the frame rate
	bool bLook = false; // the eye goes to a place and the picture waits for its fine ground ("look-at")
	float LookSettled = 0.0f;
	int32 OnScreen = -1; // (a press) 1 where the whole button lay inside the window, 0 where part of it lay beyond the edge, -1 for no button
	// (an air step) the battle it was given to; what it did; what it waits for; the speed a pause held
	bool bAir = false, bAirDid = false, bAirHandBack = false;
	float AirSettle = 0.3f;
	TWeakObjectPtr<AAirBattle> AirOpen;
	FString AirWait;
	double AirSpeedBefore = 1.0;
	// (a land step) the battle it was given to; what it did; what it waits for; the field as it began; a view key held
	// through the step's settle; what its expect word made of it
	bool bLand = false, bLandDid = false, bLandHandBack = false;
	float LandSettle = 0.8f;
	TWeakObjectPtr<ALandBattle> LandOpen;
	FString LandWait, LandPass;
	FName LandHeld;
	FLandSeen LandBefore;
};
