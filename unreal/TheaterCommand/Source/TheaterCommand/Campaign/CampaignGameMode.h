#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Campaign/CampaignView.h"
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
	/**
	 * A battle scene's own score (the sound director's mix.json music; Brian marked Zero Hour, The Field Is Ours and The
	 * Line Gives Way Good on 19 November 2026). While it runs, the map's moods wait. Zero Hour loops until first contact;
	 * then the battle's piece (The Advance or Trench Line At Twilight, in turn) at the level the scene asks for, under the
	 * music setting, as the fight thickens and thins; at the end The Field Is Ours or The Line Gives Way, after which the
	 * moods take over again. Without Zero Hour the battle's piece plays from the start; without the score the battle
	 * mood plays as it did.
	 */
	void BeginBattleScore();
	/** First contact: Zero Hour crossfades into the battle's piece over CrossfadeSeconds, at Db. */
	void BattleScoreContact(float Db, float CrossfadeSeconds);
	/** The battle's music at Db under the music setting (0: as the setting plays it), reached over FadeSeconds. */
	void SetBattleScoreLevel(float Db, float FadeSeconds);
	/** The battle is over: Outcome 1 won, 0 lost, -1 neither. The battle's piece goes, the stinger plays once through,
	 * and when it has ended the moods take over again. */
	void EndBattleScore(int32 Outcome);
	/** The battle scene is closing: the moods take over at once (a stinger still playing is heard out). */
	void StopBattleScore();
	bool IsBattleScoreOn() const { return bBattleScore; }

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

	/** Writes the campaign to this computer, so it can be continued another day. */
	void SaveCampaign();

	/** A line about the campaign saved on this computer, or empty if there is none. */
	FString DescribeSavedCampaign();

	/**
	 * (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) The wars the
	 * player keeps with SAVE, beside the campaign kept by itself (which CONTINUE picks up, and which is written over
	 * every month as before). SaveWarAs keeps a copy of the war under way; KeepAutosaveAsWar keeps the campaign kept by
	 * itself as one (before another war is loaded over it); SavedWars lists them, the newest first; LoadSavedWar lays
	 * one on the table as CONTINUE does (Why says what stopped it); DeleteSavedWar throws one away; IsAutosaveKept says
	 * whether the campaign kept by itself is kept as a saved war too, word for word, so that loading another loses nothing.
	 */
	bool SaveWarAs(FSavedWar& Out);
	bool KeepAutosaveAsWar(FSavedWar& Out);
	TArray<FSavedWar> SavedWars() const;
	bool LoadSavedWar(const FString& Id, FString& Why);
	bool DeleteSavedWar(const FString& Id);
	bool IsAutosaveKept() const;

private:
	void LayOutWar();
	static FString SaveFile();
	static FString SavedWarsFolder();
	static bool IsSavedWarId(const FString& Id);
	bool WriteSavedWar(const FString& Text, FSavedWar& Out);

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
	// A battle scene's score (BeginBattleScore): its three pieces of the score, where it stands, and its level.
	int32 ZeroHourPiece = INDEX_NONE, WonPiece = INDEX_NONE, LostPiece = INDEX_NONE, AdvancePiece = INDEX_NONE;
	bool bBattleScore = false, bBattleContact = false, bBattleEnding = false;
	bool bBattleSceneUp = false; // a battle scene is on the screen (from BeginBattleScore to StopBattleScore), score or none
	bool bStingerUp = false;     // the battle's ending is its stinger (not a fade with none)
	float BattleScoreDb = 0.0f;
	double BattleScoreEndsAt = 0.0; // the stinger's end, and a moment after: the moods take over then
	double FadeInUntil = 0.0;       // when the piece last begun has faded all the way in
	/** How long a change of level should take: at least FadeSeconds, and never cutting short a piece's fade-in. */
	float FadeLeft(float FadeSeconds) const;
};
