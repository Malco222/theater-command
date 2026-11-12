#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/CampaignMapData.h"
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
 * who leads it) will carry over unchanged when the real models arrive.
 */
UCLASS()
class THEATERCOMMAND_API AArmyPiece : public AActor
{
	GENERATED_BODY()

public:
	AArmyPiece();

	void Build(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 InRegionId, const TArray<FCampaignCorps>& InCorps);

	int32 GetRegionId() const { return RegionId; }
	const FString& GetNationTag() const { return NationTag; }
	const FString& GetGeneral() const { return General; }
	int32 GetDivisions() const { return Divisions; }
	const TArray<FCampaignCorps>& GetCorps() const { return Corps; }

private:
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TObjectPtr<USceneComponent> Root;

	int32 RegionId = INDEX_NONE;
	FString NationTag;
	FString General;
	int32 Divisions = 0;
	TArray<FCampaignCorps> Corps;
};
