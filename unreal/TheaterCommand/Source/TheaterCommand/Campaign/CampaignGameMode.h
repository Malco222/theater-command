#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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

	/** Turns the music off, or on again. Returns true if it is now playing. */
	bool ToggleMusic();
	bool HasMusic() const { return Music != nullptr; }
	bool IsMusicOn() const { return bMusicOn; }
	/** How loud the music plays: 0 soft, 1 as it was written, 2 full. */
	int32 GetMusicLevel() const { return MusicLevel; }
	void SetMusicLevel(int32 Level);
	/**
	 * The music follows the war: "theme" for a country at war, "quiet" for one at peace, "battle" while there is
	 * fighting to watch. One piece fades out as the next fades in. A piece that has not been made is passed over.
	 */
	void SetMusicMood(const FString& Mood);
	/** Plays the music softly under a voice (the newsreel's), and brings it back up afterwards. */
	void DuckMusic(bool bDown);

	/** The campaign being played: the rules, and the latest picture of the war. */
	FCampaignSession* GetSession() const { return Session.Get(); }

	/**
	 * Starts a new campaign and lays its armies on the table. Human is empty to watch. Seed sets the dice:
	 * the same seed gives the same war as long as the same orders are given.
	 */
	bool StartCampaign(const FString& Human, int32 From, const FString& ViewMode, int32 Seed = 1939, const FString& Level = FString(), const FString& Depth = FString());

	/** Picks up the campaign saved on this computer. */
	bool ContinueCampaign();

	/** Writes the campaign to this computer, so it can be continued another day. */
	void SaveCampaign();

	/** A line about the campaign saved on this computer, or empty if there is none. */
	FString DescribeSavedCampaign();

private:
	void LayOutWar();
	static FString SaveFile();

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
	float MusicVolume = 0.55f;
	int32 MusicLevel = 1;
};
