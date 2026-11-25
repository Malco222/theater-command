#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "TheaterPlaytestToolset.h"
#include "TheaterPlaytestSubsystem.generated.h"

class ACampaignPlayerController;
class FStepPlayer;

/**
 * Keeps the playtest tools' one hand: a live run of the game's step player (Campaign/CampaignStepPlayer) on the play
 * session's campaign, ticked after the play world's actors as the photographer ticks a step file's run. A step is
 * added, played, judged and logged by the same code as a step file's, and its result is handed back when it is logged.
 * Each play session gets a fresh run, logged with its pictures to Saved/Playtest/<time>.
 */
UCLASS()
class UTheaterPlaytestSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The play session's campaign player. Null, with OutError saying why, where no play session runs the campaign. */
	static ACampaignPlayerController* FindPlayer(FString& OutError);

	/** What the player is shown now. */
	static FTheaterScreen ReadScreen(const ACampaignPlayerController& Player);

	/**
	 * Starts a step (a step file's verb and target) on the play session's campaign; Result is completed once it is
	 * logged. False, with OutError saying why, where it cannot start.
	 */
	bool Play(const FString& Verb, const FString& Target, bool bInPicture, UTheaterStepAsyncResult* Result, FString& OutError);

private:
	void OnPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void OnEndPlay(bool bIsSimulating);
	/** Completes the waiting result from the step player's last record. False while its picture is still being written. */
	bool Finish(const ACampaignPlayerController& Player);
	void Fail(const FString& Error);

	TSharedPtr<FStepPlayer> Steps;
	TWeakObjectPtr<UWorld> StepsWorld; // the play world the run belongs to

	UPROPERTY()
	TObjectPtr<UTheaterStepAsyncResult> Pending; // the step being played, until it is logged

	int32 LoggedBefore = 0;
	bool bPicture = false;
	double PictureDeadline = 0.0; // (a picture asked for) how long its file is waited for, by the clock

	FDelegateHandle TickHandle, EndHandle;
};
