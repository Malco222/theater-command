#pragma once

#include "CoreMinimal.h"

class ACampaignPlayerController;

/**
 * A development tool, not part of the game. Started with "-TheaterSteps=<file>" beside "-TheaterShots=<folder>", it
 * plays the playtester's step file in the picture window: every step is done as the player's own hand would do it
 * (a click on a province, a right-click with what is in hand, a press on a button found by its words), and for
 * every step it writes one line of steps.jsonl and a picture. The same file is played on the browser game, which
 * writes the same log: laid side by side, a step that works there and fails here is a fault of this table.
 *
 * (Brian, 17 November 2026: "i want the playtester to really click everything to make sure it works in the 3d game
 * too". The rule every step is held to: a click does something, or the game says why not.)
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

	TArray<FStep> Steps;
	FString Folder;
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
	int32 OnScreen = -1; // (a press) 1 where the whole button lay inside the window, 0 where part of it lay beyond the edge, -1 for no button
};
