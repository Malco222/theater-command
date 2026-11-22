#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"

#include <atomic>
#include <string>

class FEvent;
class FRulesHost;
class FRunnableThread;

/**
 * The live ground battle's rules, on a thread of their own (release 2; Brian, 22 November 2026: "i want the ground
 * battles to be playable. im the general who controls their deployment and who to attack and move on a map with
 * tactics"; "i also want the game play to be smooth, fun, and realistic").
 *
 * The land rules (Content/Rules/land-rules.js, bundled by Tools/build_land_rules.sh from src/battle/land/live.ts and
 * Rules/land-bridge.ts) run in a rules engine of their own, apart from the campaign's (whose rules, release 1's, are
 * not touched and never wait on a battle). This thread loads them and opens the battle, then keeps it fought a little
 * ahead of what the screen shows, and hands each piece fought (a delta: JSON in the recording's own shapes) to the
 * game thread, which grows its recording from it (LandFeed::AppendDelta) and its cores with it. The player's orders go
 * the other way, in the order given, between the rules' steps.
 *
 * Like a newspaper's presses running a page ahead of the reader: the screen reads what has been printed, never the
 * page still on the press. If the rules ever fall behind, the battle's clock holds at what has been fought until they
 * catch up (ALandBattle's cap), so nothing is shown that the rules have not decided.
 */
class FLandLive : public FRunnable
{
public:
	/** The rules' answer to one of the player's orders, placings or the battle's begin, in the order asked. */
	struct FAnswer
	{
		/** "order", "place" or "begin", and what was asked (the order's JSON, or the block's id). */
		FString Kind, Asked;
		bool bOk = false;
		/** Why not, in the rules' plain words, where they said (a placing refused). */
		FString Why;
	};

	FLandLive();
	virtual ~FLandLive() override;
	FLandLive(const FLandLive&) = delete;
	FLandLive& operator=(const FLandLive&) = delete;

	/**
	 * Starts the thread, which loads the land rules and opens the set piece Key (its seed, or its first when 0) with the
	 * player on side Me ("att", "def", or "" for a film), his deployment first when bSetup. False if the thread could
	 * not start. HeaderReady() says when the battle is open.
	 */
	bool Start(const FString& Key, int32 Seed, const FString& Me, bool bSetup);
	/** The recording's header (nothing fought yet, as UTF-8 text) once the battle is open: true once, handing it over. */
	bool TakeHeader(std::string& OutHeader);
	/** True (and why) if the rules could not be loaded or the battle not opened. */
	bool Failed(FString& OutWhy) const;

	/** Game thread, each frame: the battle time shown and how far ahead of it the rules are to be kept (battle
	 * seconds: the camera's lead, more at a higher speed). */
	void Want(double Shown, double Ahead);
	/** The next delta fought (UTF-8 text, as LandFeed::AppendDelta reads it), in order: false when none waits. */
	bool TakeOne(std::string& OutDelta);
	/** One of the player's orders: TC.landOrder's command, as JSON ({ "do": "move", "units": [..], ... }). */
	void Order(const FString& CommandJson);
	/** A block placed in the setup step, facing Face (radians, 0 north, clockwise). */
	void Place(int32 Id, double X, double Y, double Face);
	/** The setup step ends: the clock may run. */
	void Begin();
	/** The 3D field is laid: its hedge lines and gateways (TC.landHedges' JSON arrays; "[]" where none), handed to the
	 * rules before the clock runs (the historian's check, 22 November: a block on a hedge line hides as in woods). The
	 * clock waits for it, or 20 s after the header. */
	void Field(const FString& HedgesJson, const FString& GatesJson);
	/** The rules' answers since the last call, in the order asked. */
	void Answers(TArray<FAnswer>& Out);

	/** Battle time the rules have reached (what the deltas taken so far go up to, or beyond). */
	double Fought() const { return FoughtT.load(); }
	bool IsOver() const { return bOver.load(); }
	/** Milliseconds the rules took for their last piece, and the most so far (for the log). */
	double LastMs() const { return LastStepMs.load(); }
	double MostMs() const { return MostStepMs.load(); }

	/** Stops the thread (after the call under way) and lets the battle and its engine go. */
	void Close();

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	struct FRequest
	{
		FString Kind, Function, Arguments, Asked;
	};

	FRunnableThread* Thread = nullptr;
	FEvent* Wake = nullptr;
	FString Key, Me;
	int32 Seed = 0;
	bool bSetup = false;

	/** (made UTF-8 here, on the rules' thread: some 70 KB a piece the game thread need not turn) */
	TQueue<std::string, EQueueMode::Spsc> Deltas;
	TQueue<FRequest, EQueueMode::Spsc> Requests;
	TQueue<FAnswer, EQueueMode::Spsc> Replies;

	mutable FCriticalSection HeaderGate;
	std::string HeaderText;
	FString Why;
	bool bHeaderReady = false, bHeaderTaken = false, bFailed = false;

	std::atomic<double> WantT { 0.0 }, AheadT { 2.5 }, FoughtT { 0.0 };
	std::atomic<double> LastStepMs { 0.0 }, MostStepMs { 0.0 };
	std::atomic<bool> bStopping { false }, bOver { false };
	/** The field's hedges are in the rules (or there were none to give): the clock may run. */
	std::atomic<bool> bFieldIn { false };

	/** The battle time a delta opens with ({"t":12.3,...), and whether it is the last ({"t":..,"end":..). */
	static double TimeOf(const std::string& Delta, bool& bOutLast);
	void Fail(const FString& InWhy);
};
