#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetImage.h"
#include "TheaterPlaytestToolset.generated.h"

/** What a step's hand does. Each is the step-file verb of the same name: SelectProvince is "select-province". */
UENUM()
enum class ETheaterStepVerb : uint8
{
	Start,
	Load,
	Snapshot,
	Restore,
	SelectProvince,
	CheckProvince,
	SelectUnit,
	OrderProvince,
	OrderUnit,
	OrderSea,
	AskWar,
	PointAt,
	ClickMark,
	LookAt,
	PanZoom,
	OpenPage,
	Press,
	Close,
	Key,
	EndMonth,
	LandKey,
	LandClick,
	LandWait,
	AirKey,
	AirWait
};

/** How a step was judged. */
UENUM()
enum class ETheaterStepVerdict : uint8
{
	/** The game changed. */
	Did,
	/** Nothing changed, and the game said why. */
	SaidWhy,
	/** The button is dim and says nothing about why. */
	Mute,
	/** No button, place, unit or country goes by that name. */
	Missing,
	/** Something covers the button, so a click would not reach it. */
	Error,
	/** Nothing changed and nothing was said. */
	Dead
};

/** What the player is shown in the running campaign. */
USTRUCT()
struct FTheaterScreen
{
	GENERATED_BODY()

	/** Title of the card, page or province panel on top; empty when the bare map shows. */
	UPROPERTY()
	FString Open;

	/** The words shown (the notice line, the open card's or page's text, the tray's hint), joined by " | ". */
	UPROPERTY()
	FString Words;

	/** The army, fleet or agent in hand, or the army being looked at; empty when the hand is empty. */
	UPROPERTY()
	FString Hand;

	/** What the game is playing through by itself (the month's-end question, the other countries' month, a newsreel, an announcement), else the open page's title; empty when neither. */
	UPROPERTY()
	FString Showing;

	/** Whether the player's own 3D land or air battle covers the map. */
	UPROPERTY()
	bool bInBattle = false;
};

/** One played step: how it was judged, what the screen showed after it, and how smoothly the game ran meanwhile. */
USTRUCT()
struct FTheaterStepResult
{
	GENERATED_BODY()

	/** How the step was judged. */
	UPROPERTY()
	ETheaterStepVerdict Verdict = ETheaterStepVerdict::Dead;

	/** Why the step did not do what it named, or what a battle step saw. Empty when there is nothing to add. */
	UPROPERTY()
	FString Note;

	/** The screen once the step settled. After PointAt, Words holds the pointer's tip. */
	UPROPERTY()
	FTheaterScreen Screen;

	/** Milliseconds the input itself held the game. */
	UPROPERTY()
	float InputMs = 0.0f;

	/** Milliseconds from the input until the screen first changed; -1 when it never did. */
	UPROPERTY()
	float FirstChangeMs = -1.0f;

	/** Milliseconds from the input until the screen last changed; -1 when it never did. */
	UPROPERTY()
	float LastChangeMs = -1.0f;

	/** Milliseconds from the input until the step was judged. */
	UPROPERTY()
	float ElapsedMs = 0.0f;

	/** Frames per second of the slowest frame while the step played. */
	UPROPERTY()
	float SlowestFps = 0.0f;

	/** Frames per second of the median frame while the step played. */
	UPROPERTY()
	float MedianFps = 0.0f;

	/** Mean milliseconds per frame on the game thread while the step played. */
	UPROPERTY()
	float GameThreadMs = 0.0f;

	/** Mean milliseconds per frame on the render thread while the step played. */
	UPROPERTY()
	float RenderThreadMs = 0.0f;

	/** Mean milliseconds per frame on the GPU while the step played. */
	UPROPERTY()
	float GpuMs = 0.0f;

	/** After EndMonth: each screen that came up on the way, with the seconds since the input. */
	UPROPERTY()
	TArray<FString> MonthScreens;

	/** The game window after the step; empty unless a picture was asked for. */
	UPROPERTY()
	FToolsetImage Picture;
};

/** Completes with a played step's result. */
UCLASS()
class UTheaterStepAsyncResult : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:
	UFUNCTION()
	bool SetValue(const FTheaterStepResult& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FTheaterStepResult(InValue), Value);
	}

	UPROPERTY()
	FTheaterStepResult Value;
};

/**
 * Play-tests Theater Command's campaign in a running Play-In-Editor session as a player would: reads what the screen
 * shows, and plays steps with the same hand as the game's playtest step files, each judged and timed. The session must
 * be playing the campaign map.
 */
UCLASS()
class UTheaterPlaytestToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns what the player is shown now.
	 */
	UFUNCTION(meta = (AICallable), Category = "TheaterPlaytest")
	static FTheaterScreen GetScreen();

	/**
	 * Plays one step and completes once the game has settled: about a second for most steps, up to two minutes for
	 * EndMonth and up to 15 minutes for a battle wait. One step plays at a time.
	 * @param Verb What the hand does.
	 * @param Target What it acts on, as a step file writes it:
	 *   Start: country, month and dice, with an optional level ("UK 1939-09 seed 4").
	 *   Load: a saved game's full path. Snapshot, Restore: a name to keep the game under or bring it back from.
	 *   SelectProvince, CheckProvince, OrderProvince: a province. SelectUnit, OrderUnit: a corps, fleet or air group.
	 *   OrderSea: a sea. AskWar: a country.
	 *   PointAt, ClickMark: a province, "airfield <province>" or "gate <name>".
	 *   LookAt, PanZoom: a province followed by near, middle or far.
	 *   OpenPage: build, research, air, navy, powers, staff, ledger, news, generals or honours.
	 *   Press: a button's words; "WORDS#2" presses the second of several alike.
	 *   Key: tab, enter, esc, space, t, r or y. Close, EndMonth: none.
	 *   LandKey: a key of the land battle (L, Esc, H, G, R, F, X, K, T, B, Tab, C, V, W, A, S, D, Q, E, an arrow,
	 *   Space, Enter, 1 to 3). LandClick: a button's words, "field", "field-right" or "drag".
	 *   LandWait: battle seconds ("150"), real seconds ("20s"), begun, contact or card.
	 *   AirKey: continue, chase, next, prev, up, down, pause, play, space, x0.5, x1, x2, x4, skip or escape.
	 *   AirWait: battle seconds, real seconds ("20s"), flyover or card.
	 * @param bPicture Whether to return a picture of the game window after the step.
	 * @return The step's verdict, the screen after it, and the frame timing while it played.
	 */
	UFUNCTION(meta = (AICallable), Category = "TheaterPlaytest")
	static UTheaterStepAsyncResult* PlayStep(ETheaterStepVerb Verb, const FString& Target, bool bPicture = false);
};
