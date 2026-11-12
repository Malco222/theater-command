#pragma once

#include "CoreMinimal.h"
#include "Campaign/CampaignView.h"

class FRulesHost;
class FJsonObject;

/**
 * One campaign in progress. It owns the rules engine, asks it questions, gives
 * it the player's orders, and keeps the latest picture of the war for
 * everything else to draw.
 */
class FCampaignSession
{
public:
	FCampaignSession();
	~FCampaignSession();

	/** Starts a new campaign with the player leading the given nation. */
	bool Start(const FString& Human, FString& OutError);

	bool IsRunning() const { return bRunning; }
	const FCampaignView& GetView() const { return View; }

	/** Where the given corps could go this month. */
	TArray<FReachRegion> Reach(const TArray<int32>& CorpsIds);

	/** Orders the given corps to march to a region. Returns the marches that actually happened. */
	TArray<FCorpsMove> Move(const TArray<int32>& CorpsIds, int32 RegionId);

	/** The odds for the given corps attacking a region, with both armies and the Battle Ticket. */
	FAttackPreview PreviewAttack(const TArray<int32>& CorpsIds, int32 RegionId);

	/** Sends the given corps in. The battle is settled at once by the rules. */
	FAttackResult Attack(const TArray<int32>& CorpsIds, int32 RegionId);

	/** Ends the player's month and plays every other nation's. Returns what happened, nation by nation. */
	FMonthResult EndMonth();

	/** Fired whenever the picture of the war has changed. */
	DECLARE_MULTICAST_DELEGATE(FOnViewChanged);
	FOnViewChanged OnViewChanged;

private:
	bool CallRules(const FString& Function, const FString& ArgumentsJson, TSharedPtr<FJsonObject>& OutObject);
	static FCampaignView ParseView(const TSharedPtr<FJsonObject>& Object);
	void TakeView(const TSharedPtr<FJsonObject>& Holder);

	TUniquePtr<FRulesHost> Host;
	FCampaignView View;
	bool bRunning = false;
};
