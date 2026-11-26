#pragma once

#include "CoreMinimal.h"

class AActor;
class ACameraActor;
class APlayerController;
struct FAirBattleState;
struct FLandBattleState;

/**
 * The film critic's facts (development only): with "-FilmCritic" on the command line the game writes cheap facts about
 * what it shows, a JSON object a line, to <the run's picture folder>/critic-facts.jsonl, and with
 * "-FilmCriticStrip=<fps>,<seconds>" it also saves short film strips (JPEG, 960 px wide, at most 1500 frames a run).
 * The formats are Tools/film_critic/INTERFACE.md's sections 1 and 2; Tools/film_critic/FACTS-BUILD-NOTES.md says what
 * is written and what is not.
 *
 * Without -FilmCritic nothing here does anything: every hook in the game is behind `if (FilmCritic::On())`, which reads
 * the command line once. No hook changes the game's own state, timing or random streams.
 */
namespace FilmCritic
{
	/** True only with -FilmCritic on the command line (read once, then kept). */
	bool On();
	/** "-FilmCriticStrip=<fps>,<seconds>" given and sound: its two numbers. */
	bool StripAsked(double& OutFps, double& OutSeconds);
	/** Where the run's pictures go (-TheaterShots=, -TheaterLandBattleShots=, -TheaterAirBattleShots=,
	 * -TheaterSeaBattleShots=, -TheaterLandScript=, the first given), else Saved/FilmCritic. */
	FString Folder();

	/** Once a frame, from the campaign's player controller (PlayerTick): the map's camera and fleets, the strips' frames,
	 * the stills waiting for their camera, and the file written at most once a second. */
	void Frame(APlayerController& Player, float DeltaSeconds);
	/** At the end of play: the last lines written, the strips' grabber put away. */
	void Shutdown();

	/** A cut told to the renderer this frame (the air battle's TellCut). */
	void NoteCut();

	/** The air battle's camera as it will be drawn this frame (after its jolt), its shot and the aircraft in frame.
	 * Overview: the strategy view's name for its shot. */
	void Air(const AActor* Battle, const FAirBattleState& S, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds, const TCHAR* Overview);
	/** The land battle's camera as it will be drawn this frame (after its jolt), its men, vehicles, look and end. */
	void Land(const AActor* Battle, const FLandBattleState& S, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds);
	/** The sea battle's camera as it will be drawn this frame (after its shake): bCut on the frame of a cut, Shot the
	 * shot's name, Subject the ship it is on (-1 none). */
	void Sea(const AActor* Battle, const ACameraActor& Eye, double UnitsPerMetre, float DeltaSeconds, double BattleSeconds, bool bCut, const FString& Shot, int32 Subject);

	/** A named still has just been asked for (FScreenshotRequest): its name, as the scene calls it. */
	void Still(const FString& Name);
	/** A step of a step run has just been acted (its picture's number, the step's verb). */
	void StepActed(int32 Number, const FString& Verb, bool bReplay);
}
