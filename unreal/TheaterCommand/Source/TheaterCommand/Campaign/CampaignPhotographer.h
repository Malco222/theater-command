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
	FString Carry; // hang the army in hand over this region, as if dragged there
	FString Drop; // let go of it over this region // switch the army in hand to going by rail
		FString Order; // order this in the selected territory: a kind of corps, or "fort"
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
	int32 StartGroup = 0;
	int32 StartSide = 0;
};
