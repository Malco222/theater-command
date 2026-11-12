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
class UTextRenderComponent;
class UProceduralMeshComponent;

/**
 * One nation's force in one region, standing on the table as a single piece:
 * a stack of counters, one for each corps, with figures standing on it for the
 * kinds of troops it holds (riflemen, a tank, a lorry, a horseman, a bunker),
 * under the nation's flag of the period with the army's strength on a plate.
 *
 * The figures are simple ones made in code (CampaignMiniatures.h), the kind a
 * board game is sold with. Sculpted miniatures can replace them, and everything
 * that uses a piece (where it stands, whose it is, who leads it, how it is
 * picked up and marched) will carry over unchanged when they do.
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

	/**
	 * Lifts the piece, carries it along the points given, and sets it down at the last. With bLifted false it is
	 * not lifted on the way: it glides straight there, as a piece does that is already in the hand.
	 */
	void MarchAlong(const TArray<FVector>& Waypoints, bool bRemoveOnArrival, bool bLifted = true);
	bool IsMarching() const { return bMarching; }

	/** A picked-up piece shows a bright ring under its base and is held a little off the table. */
	void SetSelected(bool bInSelected);

	/** A piece the pointer rests on shows the ring too, to say it can be picked up. */
	void SetHovered(bool bInHovered);

	/** True if a line from Origin along Direction passes through the piece: its counters, its pole or its colours. */
	bool HitByRay(const FVector& Origin, const FVector& Direction, double& OutDistance) const;

	/** Carries the piece in the hand: it hangs over this point of the table. */
	void Carry(const FVector& GroundPoint);

	/** Puts a carried piece back where it stands. */
	void ReturnToRest();

	int32 GetRegionId() const { return RegionId; }
	const FString& GetNationTag() const { return NationTag; }
	const FString& GetGeneral() const { return General; }
	int32 GetDivisions() const { return Divisions; }
	int32 GetMen() const { return Men; }
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

	/** The piece itself, in one mesh: its counters, the figures standing on them, the pole and the strength plate. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Body;

	/** The cloth of the standard: the nation's flag of the period. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Cloth;

	/** The army's strength, written on a plate under its colours. */
	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> StrengthLabel;

	int32 RegionId = INDEX_NONE;
	FString NationTag;
	FString General;
	int32 Divisions = 0;
	int32 Men = 0;
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
	bool bLiftOnMarch = true;
	bool bSelected = false;
	bool bHovered = false;

	// The pole and the colours, in the piece's own space, for telling whether the pointer is on them.
	FVector PoleFoot = FVector::ZeroVector;
	double BodyTop = 100.0;
	double PoleTop = 200.0;
	double BaseRadius = 60.0;
	FVector FlagCenter = FVector::ZeroVector;
	double FlagRadius = 60.0;

	void ShowRing();
	FVector HomeLocation() const;
};
