#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonthSeaScene.generated.h"

class ASeaBattle;

/**
 * (D1, ruling 241) The month's submarine scene, opened with the month. As a month's card lies open (or, where none
 * opens, once the table has stood free a second on a new month), the rules are asked on their own thread for the
 * month's scene (FCampaignSession::SubSceneAside, TC.subScene: the stager's staged recording, or none); once the card
 * is put away and the table has stood free a second, the scene plays (ASeaBattle::BeginMonthScene): 30 to 45 seconds
 * of the month at sea, its caption, a key or a click back to the map at any moment.
 *
 * None where nobody is led (a war watched), where the viewer's setting says "never" or "off" (FCampaignSession::
 * GetSubScene), or in a test or picture run unless "-TheaterSubScene=" asks for it (IsWanted). A month passed before
 * its scene could play drops it: only the month in hand is shown.
 */
UCLASS()
class THEATERCOMMAND_API AMonthSeaScene : public AActor
{
	GENERATED_BODY()

public:
	AMonthSeaScene();

	/** This run plays the month's scenes: a played campaign, not a scene's test, a picture run or the playtester's steps,
	 * unless "-TheaterSubScene=" asks. */
	static bool IsWanted();

	virtual void Tick(float DeltaSeconds) override;

private:
	/** The war asked of (FCampaignSession::GetWarsBegun), the month last asked of in it, and the stager's mark to count
	 * the next month's news from (-1: from the month's own). */
	int32 War = INDEX_NONE;
	int32 AskedTurn = INDEX_NONE;
	int32 Mark = -1;
	/** The rules are working on the month's question now (its answer comes back through the session's Deliver). */
	bool bAsking = false;
	/** The staged recording waiting for the table to come free, and its month; when the table was first seen free. */
	FString Waiting;
	int32 WaitingTurn = INDEX_NONE;
	double FreeSince = -1.0;
	/** The scene on the screen. */
	TWeakObjectPtr<ASeaBattle> Playing;
};
