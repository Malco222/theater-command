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
	/**
	 * How large the standard is drawn beside its life size, and how far it leans back toward an eye above it, in
	 * degrees. The base is not touched. (The size is what is seen: a piece that is itself drawn small keeps a
	 * standard of the same size as its neighbours'.)
	 */
	void SetStandard(float Scale, float TiltDegrees);
	/** How large the whole piece is drawn: 1, or four fifths in a crowded province. Its standard keeps the size its neighbours' have. */
	void SetSize(float Size);

	/** Makes the piece a fleet lying on the sea, or an air fleet over its airfield, instead of an army. */
	void SetFleet(ACampaignMapActor* Map, const FCampaignNation& Nation, const FViewFleet& InFleet);
	void SetAirFleet(ACampaignMapActor* Map, const FCampaignNation& Nation, const FViewAirFleet& InWing);
	bool IsArmy() const { return What == 0; }
	bool IsFleet() const { return What == 1; }

	/** Whether the figures and base cast shadows. Turned off for pieces far from what the eye is looking at: their shadows cost a great deal and cannot be seen. */
	void SetShadows(bool bCast);
	bool bCastsShadows = true;
	/** The number the rules know a fleet or an air fleet by. */
	int32 GetForceId() const { return What == 1 ? Fleet.Id : (What == 2 ? Wing.Id : INDEX_NONE); }

	/** Where the piece stands when it is not on the march. It goes there at once unless it is marching. */
	void SetRestLocation(const FVector& Location);
	const FVector& GetRestLocation() const { return RestLocation; }

	/**
	 * Lifts the piece, carries it along the points given, and sets it down at the last. With bLifted false it is
	 * not lifted on the way: it glides straight there, as a piece does that is already in the hand.
	 */
	void MarchAlong(const TArray<FVector>& Waypoints, bool bRemoveOnArrival, bool bLifted = true);
	bool IsMarching() const { return bMarching; }
	bool IsInCamp() const { return What == 0 && bCamp; }
	int32 GetWorksState() const { return What == 0 ? WorksState : 0; }
	int32 GetSupplyState() const { return What == 0 ? SupplyState : 0; }
	/** For looking at the dressing itself: shows the army as entrenched (2) or digging (1), in camp, supplied (1) or cut off (2), whatever it is. */
	void DressAs(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 Works, bool bInCamp, int32 Supply);
	/** Where its camp fire burns, in the world, for the smoke that rises from it. */
	FVector GetCampFire() const;
	/** Shows or hides the flock on every base (the map does it as the eye rises and comes down). New pieces take it as they are made. */
	void SetFlockShown(bool bShown);
	static bool bFlockNear;
	/** A fleet lying in harbour is drawn small, at anchor by its port, with no standard over it: a small badge on the port says how many ships are in (the map is the clearer for it). */
	void SetAtAnchor(bool bInHarbour);
	bool IsAtAnchor() const { return What == 1 && bAtAnchor; }
	int32 GetFleetShips() const { return What == 1 ? Fleet.Ships() : 0; }
	int32 GetFleetBase() const { return What == 1 ? Fleet.Base : INDEX_NONE; }
	const FString& GetNationTag() const { return NationTag; }
	/** Its colours, as a ball in the world: what a town's name plate must not be drawn across. False while the piece is hidden. */
	bool GetColours(FVector& OutMiddle, double& OutRadius) const;

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
	/** For an air fleet: the province its airfield is in. */
	int32 GetBaseRegion() const { return What == 2 ? Wing.Base : RegionId; }
	const FString& GetGeneral() const { return General; }
	int32 GetDivisions() const { return Divisions; }
	int32 GetMen() const { return Men; }
	const TArray<FViewCorps>& GetCorps() const { return Corps; }
	bool HasCorps(int32 CorpsId) const;

private:
	void Rebuild(ACampaignMapActor* Map, const FCampaignNation& Nation);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Size, UMaterialInterface* Material, USceneComponent* Parent = nullptr);

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

	/**
	 * The standard: the pole, the colours, the strength plate and what hangs under it. It stands on its own foot so
	 * that it can grow and lean toward the eye as the eye rises, while the base under it keeps its size on the
	 * table (art fix 8: the flags lead from working height, not the discs).
	 */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Standard;
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mast; // the pole, its finial and the plate
	float StandardScale = 1.0f, StandardTilt = 0.0f;
	FVector StandardFoot = FVector::ZeroVector; // where the standard stands, in the piece's own space
	void ApplyStandard();
	/** A point of the standard as it stands now, grown and leaning, in the piece's own space. */
	FVector OnStandard(const FVector& Point) const;

	int32 RegionId = INDEX_NONE;
	FString NationTag;
	FString General;
	int32 Divisions = 0;
	int32 Men = 0;
	TArray<FViewCorps> Corps;
	FString Signature; // what the piece was last built from, to tell when it must be rebuilt
	int32 What = 0; // 0 an army, 1 a fleet, 2 an air fleet
	FViewFleet Fleet;
	FViewAirFleet Wing;

	FVector RestLocation = FVector::ZeroVector;
	TArray<FVector> Route;
	TArray<double> RouteMarks; // distance along the route at each point
	double MarchClock = 0.0;
	double MarchTime = 1.0;
	double LiftHeight = 40.0;
	bool bMarching = false;
	// What the army is about, shown round its base so it can be read without a click: 0 nothing, 1 digging in,
	// 2 entrenched; in camp, resting; 0 not known, 1 supplied, 2 cut off.
	int32 WorksState = 0;
	bool bAtAnchor = false;
	/** The flock and tufts on the base: shown only close to, where they are seen for what they are. From working height they swallowed the figures. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Flock;
	/** From working height the pale earth read as a tan coin: there a darker earth lies over it. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> FarEarth;
	bool bCamp = false;
	int32 SupplyState = 0;
	FVector CampFireAt = FVector::ZeroVector; // in the piece's own frame
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
