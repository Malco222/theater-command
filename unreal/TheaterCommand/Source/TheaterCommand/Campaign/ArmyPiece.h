#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignView.h"
#include "ArmyPiece.generated.h"

class ACampaignMapActor;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * One nation's force in one region, standing on the table as a single piece:
 * a stack of counters, one for each corps, under the nation's standard.
 *
 * The shapes are plain blocks for now. They are stand-ins for sculpted
 * miniatures, and everything that uses a piece (where it stands, whose it is,
 * who leads it, how it is picked up and marched) will carry over unchanged
 * when the real models arrive.
 */
UCLASS()
class THEATERCOMMAND_API AArmyPiece : public AActor
{
	GENERATED_BODY()

public:
	AArmyPiece();

	virtual void Tick(float DeltaSeconds) override;

	/** Says what the piece stands for. It is rebuilt only if that has changed. */
	void SetArmy(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 InRegionId, const TArray<FViewCorps>& InCorps);

	/** Where the piece stands when it is not on the march. It goes there at once unless it is marching. */
	void SetRestLocation(const FVector& Location);
	const FVector& GetRestLocation() const { return RestLocation; }

	/** Lifts the piece, carries it along the points given, and sets it down at the last. */
	void MarchAlong(const TArray<FVector>& Waypoints, bool bRemoveOnArrival);
	bool IsMarching() const { return bMarching; }

	/** A picked-up piece shows a bright ring under its base. */
	void SetSelected(bool bInSelected);

	int32 GetRegionId() const { return RegionId; }
	const FString& GetNationTag() const { return NationTag; }
	const FString& GetGeneral() const { return General; }
	int32 GetDivisions() const { return Divisions; }
	const TArray<FViewCorps>& GetCorps() const { return Corps; }
	bool HasCorps(int32 CorpsId) const;

private:
	void Rebuild(ACampaignMapActor* Map, const FCampaignNation& Nation);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SelectionRing;

	int32 RegionId = INDEX_NONE;
	FString NationTag;
	FString General;
	int32 Divisions = 0;
	TArray<FViewCorps> Corps;
	FString Signature; // what the piece was last built from, to tell when it must be rebuilt

	FVector RestLocation = FVector::ZeroVector;
	TArray<FVector> Route;
	TArray<double> RouteMarks; // distance along the route at each point
	double MarchClock = 0.0;
	double MarchTime = 1.0;
	double LiftHeight = 40.0;
	bool bMarching = false;
	bool bRemoveOnArrival = false;
};
