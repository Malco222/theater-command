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
	bool bDismiss = false; // put that screen away again
	bool bAnnounce = false; // show the screen that tells of a nation's fall
	bool bGuide = false; // open the page that says how to play
	bool bWorks = false; // stand a full set of works in the region looked at: factories, a shipyard, guns and an airfield
	int32 ProvinceTab = 0; // the tab of the province panel to show
	bool bWealth = false; // the wealth view of the map
	bool bNewsreel = false; // show a newsreel story over the map
	bool bFleet = false; // pick up the player's largest fleet, as an army is picked up
	bool bFleetSails = false; // and send it to the farthest sea it can reach
	bool bGenerals = false; // open the generals' list, and give the first corps without one the man who suits it
	bool bResearch = false; // open the design offices, and put a second team on armour
	FString Path; // in the foreign office: press this path ("found:DEN" names the partner)
	int32 PathPresses = 1; // once asks, twice takes it
	bool bArsenal = false; // open the arsenal, and order an artillery corps from it
	bool bWar = false; // answer yes to the question about declaring war
	FString Click; // click this region, as the mouse would
	int32 OpeningTab = -1; // on the opening screen: show this page
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
	FString Folder;
	int32 Current = -1;
	float Clock = 0.0f;
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
