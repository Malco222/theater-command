#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CampaignPhotographer.generated.h"

/**
 * A development tool, not part of the game. Started with
 * "-TheaterShots=<folder>" on the command line, it waits for the table to be
 * ready, moves the view through a fixed set of positions, saves a picture of
 * each, notes how fast the game was drawing, and quits. It is how the look of
 * the campaign map is checked after a change without anyone at the keyboard.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignPhotographer : public AActor
{
	GENERATED_BODY()

public:
	ACampaignPhotographer();

	void Begin(const FString& InFolder);

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FShot
	{
		FString Name;
		FString Region; // look at this region; empty for the middle of the map
		float Zoom = 0.6f;
		float Lens = 0.0f;
		float Yaw = 0.0f;
		float Tilt = 0.0f;
		FString Select; // pick this region, so its panel is in the picture
		FString Army; // pick up the player's army in this region
		FString Route; // show the road to this region
		FString March; // order the army there just before the picture, to catch it in the air
	FString Moment; // begin a moment at the front over this region just before the picture (the eye drops to it and is handed back): Wait says how far into it the picture is taken
		FString Attack; // order the army against this region, which opens the face-off
		bool bKeep = false; // leave the army in hand and the face-off as the last picture left them
		bool bConfirm = false; // send the attack on the face-off in
		bool bClose = false; // close the face-off
		bool bWatch = false; // end the month and watch the other nations move
		bool bOpening = false; // leave the opening screen up for this picture
		bool bMenu = false; // open the menu over the campaign
		bool bRecord = false; // open the service record of the first corps of the army in hand
		bool bRoll = false; // open the roll of honour
		bool bRail = false;
		bool bSea = false; // switch the army in hand to going by sea
	bool bLedger = false; // open the ledger
	bool bDiplomacy = false; // open the foreign office
	FString Room; // open this room of the rules' own ("spies"), and press RoomAct in it if given
	FString RoomAct;
	bool bCloseRoom = false; // then close the room, to see the table
	float Wait = 0.0f; // seconds longer to wait before taking the picture, for something that takes time to happen
	float RaidAt = -1.0f; // a raid flown in this shot is held at this share of its flight, and looked at where it is in the air
	bool bDismiss = false; // put that screen away again
	bool bAnnounce = false; // show the screen that tells of a nation's fall
	bool bGuide = false; // open the page that says how to play
	bool bLessonGuide = false; // open the Guide's list of the ten lessons (FIRST COMMAND), as GUIDE does in a war he leads
	bool bWorks = false; // stand a full set of works in the region looked at: factories, a shipyard, guns and an airfield
	int32 TownPlot = -1; // on the town's bar: click this plot, so its choices are laid out
	FString TownOrder; // and order this line of building from them
	FString Scars; // leave the scars of battle in these regions to be looked at: "Region:months since:size;..."
	FString Prizes; // mark these regions for their prizes, to be looked at: "Region:tag of the army that holds them or -:1 if burning;..."
	FString Dress; // dress the armies in these regions to be looked at: "Region:works:camp:supply;..." (works 0-2, camp 0-1, supply 0-2)
	int32 Civic = 0; // stand every kind of town building at this level beside the region looked at (4: one of each level, some going up)
	int32 ProvinceTab = 0; // the tab of the province panel to show
	bool bWealth = false; // the wealth view of the map
	bool bSides = false; // the friend-and-foe view
	FString Hover; // rest the pointer on this region, so its tip is in the picture
	FString Look; // look at the army standing in this region without picking it up: its corps as cards
	bool bJoin = false; // ask the allied corps beside one of the player's armies to join it
	bool bAdviser = false; // the adviser's first steps on the screen
	bool bNewsreel = false; // show a newsreel story over the map
	int32 Flight = -2; // play the opening flight and stop at this card of it (-1: its title)
	bool bYearCard = false; // show the title card of the year that has just opened
	bool bFleet = false; // pick up the player's largest fleet, as an army is picked up
	int32 FleetPick = -1; // (or, from 0, which of the player's fleets in the order the rules list them)
	bool bFleetSails = false;
	FString FleetDo; // with the fleet in hand: "orders" opens its room, "shell" asks about shelling the first coast in reach, "fire" shells it // and send it to the farthest sea it can reach
	bool bGenerals = false; // open the generals' list, and give the first corps without one the man who suits it
	bool bResearch = false; // open the design offices, and put a second team on armour
	FString Path; // in the foreign office: press this path ("found:DEN" names the partner)
	int32 PathPresses = 1; // once asks, twice takes it
	bool bArsenal = false; // open the arsenal, and order an artillery corps from it
	bool bWar = false; // answer yes to the question about declaring war
	FString Click; // click this region, as the mouse would
	int32 OpeningTab = -1; // on the opening screen: show this page
	FString Options; // and press these options on it, joined by commas
	int32 OpeningYear = -1; // and choose this day for the grand campaign
	FString OpeningNation; // and this country
	FString Carry; // hang the army in hand over this region, as if dragged there
	FString Drop; // let go of it over this region // switch the army in hand to going by rail
		FString Order; // order this in the selected territory: a kind of corps, or "fort"
		FString Force; // open the panel of this fleet or air fleet: "fleet:5", "air:1"
		int32 Months = 0; // end this many months first
	};

	void Arrange(const FShot& Shot);

	TArray<FShot> Shots;
	TSharedPtr<class FStepPlayer> Steps; // ("-TheaterSteps=<file>": the playtester's step file is played in place of the pictures)
	// ("-TheaterTime=<name>:<setting>=<value>,...|<name>:...": the first view is held and the render settings are
	// switched inside the one running game, round after round, so that each is timed in the same warmth of the
	// machine. This Mac has no fan: a series of separate runs reads slower run by run, whatever is being tried.)
	struct FTimeSet
	{
		FString Name;
		TArray<TPair<FString, FString>> Vars;
		TArray<float> Fps, Game, Draw, Chip;
		TArray<int32> Slow;
	};
	TArray<FTimeSet> TimeSets;
	TMap<FString, FString> TimeDefaults; // each setting touched, as it stood before the first trial
	int32 TimeRound = 0, TimeRounds = 3, TimeAt = -1, TimeFrames = 0, TimeSlow = 0;
	float TimeClock = 0.0f, TimeSum = 0.0f;
	double TimeGame = 0.0, TimeDraw = 0.0, TimeChip = 0.0;
	bool bTimePictured = false;
	/** One frame of the trials. False when every round is done and the summary is written. */
	bool TickTiming(float DeltaSeconds);
	void ApplyTimeSet(int32 Index);
	FString AmberFrom, AmberTo; // the army and the shore of a landing with an amber verdict, once one has been looked for and found
	FString Folder;
	int32 Current = -1;
	float Clock = 0.0f;
	// ("-TheaterClip=24,0.25": after the first picture, so many more of the same view, so many seconds apart, for a short clip)
	int32 ClipFrames = 0, ClipTaken = 0;
	float ClipEvery = 0.25f, ClipAt = 0.0f;
	float SheetWait = 0.0f; // how long this picture has waited for the world's fine ground
	float WarmUp = 0.0f;
	int32 Frames = 0;
	bool bTaken = false;
	bool bMarched = false;
	bool bStarted = false;
	FString GrandNation; // "-TheaterGrand=SOV,2": begin the grand campaign as this country, in the year at this place in the list
	int32 GrandYear = 0;
	int32 StartGroup = 0;
	int32 StartSide = 0;
};
