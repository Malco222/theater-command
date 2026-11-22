#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/Function.h"
#include "Async/Future.h"
#include "CampaignGameMode.generated.h"

class FCampaignSession;
class ACampaignMapActor;
class UAudioComponent;

/**
 * Sets the campaign up when the game starts: lays the table, lights it, and
 * gives the player a view of it. The level itself is empty; everything on the
 * table is built from the campaign data.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACampaignGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	/** (Release 2, tc.Speed.SaveAside) A save still being written is finished before the game lets the campaign go. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Turns the music off, or on again. Returns true if it is now playing. */
	bool ToggleMusic();
	bool HasMusic() const { return Music != nullptr; }
	bool IsMusicOn() const { return bMusicOn; }
	/** How loud the music plays: 0 soft, 1 as it was written, 2 full. */
	int32 GetMusicLevel() const { return MusicLevel; }
	void SetMusicLevel(int32 Level);
	/**
	 * The music follows the war: "theme" on the opening screen, "quiet" for a country at peace, "war" for one at war,
	 * "battle" while there is fighting to watch. One piece fades out as the next fades in. Where the score has been
	 * brought in (Brian's songs and the three written to go with them), each mood has two or three pieces that take
	 * turns; without it the three made pieces play as they did, the theme standing for the war as well.
	 */
	void SetMusicMood(const FString& Mood);
	/** Plays the music softly under a voice (the newsreel's), and brings it back up afterwards. */
	void DuckMusic(bool bDown);
	/**
	 * (Brian's score.) One of the player's wars has ended: The Long Walk Home is played once, now. When it has ended
	 * the pieces of the mood take their turns again. Nothing happens where the score has not been brought in.
	 */
	void PlayHomecoming();

	/** The campaign being played: the rules, and the latest picture of the war. */
	FCampaignSession* GetSession() const { return Session.Get(); }

	/**
	 * Starts a new campaign and lays its armies on the table. Human is empty to watch. Seed sets the dice:
	 * the same seed gives the same war as long as the same orders are given.
	 */
	bool StartCampaign(const FString& Human, int32 From, const FString& ViewMode, int32 Seed = 1939, const FString& Level = FString(), const FString& Depth = FString(), int32 History = 0, bool bFirstCommand = false);

	/** Picks up the campaign saved on this computer. */
	bool ContinueCampaign();
	/** (The playtester's click-through.) The campaign as it stood when this text was saved, laid on the table again. */
	bool RestoreCampaign(const FString& SaveText);

	/**
	 * Writes the campaign to this computer, so it can be continued another day. bAside (release 2, the month's own
	 * save, tc.Speed.SaveAside): the war is written out on the rules' own thread and onto the disk on a worker thread,
	 * a frame or a few later, and nothing of it is done in this frame. Otherwise at once, as before.
	 */
	void SaveCampaign(bool bAside = false);

	/** A line about the campaign saved on this computer, or empty if there is none. */
	FString DescribeSavedCampaign();

	/** (Release 2, tc.Speed.WarSpread) The next LayOutWar lays the war a step a frame (LayNextStep), not all at once. */
	void LayNextWarInSteps(bool bSteps) { bLayInSteps = bSteps; }
	/** Lays one step of a war being laid; false where none was left (the war is laid). */
	bool LayNextStep();

private:
	void LayOutWar();
	bool bLayInSteps = false;
	TArray<TFunction<void()>> LaySteps;
	static FString SaveFile();
	/** (tc.Speed.SaveAside) The text onto the disk on a worker thread, after any written before it: a file beside the save, then put in its place. */
	void WriteSave(FString&& Text);
	/** Waits for a save still being written, where there is one: before the save is read, or one is written at once. */
	void WaitForSave();
	TFuture<void> SaveWriting;
	/** Each save asked for, counted: a save given aside is written only where none was asked after it. */
	int32 SavesAsked = 0;

	TSharedPtr<FCampaignSession> Session;

	UPROPERTY(Transient)
	TObjectPtr<ACampaignMapActor> Table;

	/** The campaign's music, playing on a loop. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;
	bool bMusicOn = true;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAudioComponent>> Pieces; // every piece that has been made, by its mood
	FString MusicMood = TEXT("theme");
	bool bMusicDucked = false;
	// The score (Music/Score in the game's folder): its pieces, which of them each mood plays in turn, and whose turn it is.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ScorePlayers;
	TMap<FString, TArray<int32>> ScoreOf;
	TMap<FString, int32> ScoreTurn;
	int32 HomePiece = INDEX_NONE; // The Long Walk Home
	bool bHomePlaying = false;
	double PieceEndsAt = 0.0; // when the piece now playing will have run its length (a piece of the score is played once through)
	/** Starts the piece now in hand, fading it in, and notes when it will end. */
	void StartPiece(float FadeSeconds);
	/** The piece a mood plays now: the score's, in its turn, or the made piece. Nothing where there is neither. */
	UAudioComponent* PieceFor(const FString& Mood) const;
	/** How loud the music is to be now: the player's setting, the opening screen's theme clearly heard, and the newsreel's voice over it. */
	float NowVolume() const;
	float MusicVolume = 0.55f;
	int32 MusicLevel = 1;
};
