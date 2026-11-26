#pragma once

#include "CoreMinimal.h"
#include "Battle/ShipLooks.h"
#include "Effects/BattleEffects.h"

#include <functional>
#include "GameFramework/Actor.h"
#include "AirStage.generated.h"

class ACameraActor;
class FJsonObject;
class UMaterialInstanceDynamic;
class UTexture2D;
class UProceduralMeshComponent;
class UTheaterStillMesh;

/** One ship of a set piece, as the air battle's view gives it (view.ships.pieces, src/battle/air/ships.ts). */
struct FStageShip
{
	int32 Id = 0;
	FString Class;   // "bb", "cv", "ca", "dd", "ss" (the stage test also lays "ag", the old battleship Utah)
	FString Nation;  // the navy's tag: "USA", "JAP", "UK"...
	FString Name;    // the record's name in the record's month, else empty (ruling 238(4))
	FVector2D At = FVector2D::ZeroVector; // on the board, in metres
	double Face = 0.0; // the bow's heading on the board, radians, 0 up the board and clockwise (the sim's own)
	bool bDock = false; // in a dry dock, on the stage's land
};

/**
 * The ground of a set-piece air battle (ruling 238): Pearl Harbor on Oahu, 7 December 1941, and Midway, 4 June 1942.
 * The island with its hills, the shallows, reef and lagoon, the piers, oil tanks, airfields and palms, built from
 * Content/Data/Stages/<key>.json (tools/stages on the rules branch makes it from open map data); the ships of the
 * battle at their berths or at sea; and what happens to them: a bomb or torpedo striking, and each way of going down
 * (settling on the bottom, going down at sea, rolling over, blowing up, burning, breaking in two).
 *
 * The stage knows nothing of the air battle. The battle's scene raises it under the formations with Raise, lays the
 * ships with SetShips (or SetShipsFromView), and passes on the battle's events: air:ship-hit to Hit, air:ship-sunk to
 * Sink. It ticks it once a frame on the battle's own clock.
 *
 * Everything is in the actor's own frame: X is up the board (the board's y), Y across it (the board's x), Z up from
 * the sea, UnitsPerMetre to a metre. A stage point (metres east and north of the stage's origin) lies on the board at
 * Target + turn(p - Aim), as the battle's ticket turns the map (stages.ts onBoard).
 *
 * "-TheaterStage=oahu" (or "midway", or "midway-fleet") on the command line raises the stage alone, plays a strike on
 * the record's ships, takes pictures into "-TheaterStageShots=<folder>" if given, and quits: frames for the art
 * director before the air battle's scene exists. With "-TheaterStageReel" too (oahu or midway-fleet) it plays instead
 * each look a ship can show when she survives a hit (Battle/ShipLooks), one every 44 s on Nevada or Hiryu, the
 * effects cleared before each, two pictures each aimed at the hit (1.5 s and 40 s in, late enough for a slow list,
 * trim or settle to show).
 */
UCLASS()
class THEATERCOMMAND_API AAirStage : public AActor
{
	GENERATED_BODY()

public:
	AAirStage();

	/**
	 * Builds the stage. Key: "oahu" or "midway". Aim: the raid's aim in stage metres (stages.ts aim). Turn: the
	 * ticket's turn, radians counter-clockwise. Target: the board's target in board metres. Effects: the scene's
	 * battle effects, for the hits and the fires (not owned; null for none). False, with a warning, if the stage's
	 * file is missing or unreadable. Raised again (another battle), it first takes down the stage and ships it put up.
	 * In a packaged build, Content/Data/Stages must be staged with the game, as Content/Effects is.
	 */
	bool Raise(const FString& Key, const FVector2D& Aim, double Turn, const FVector2D& Target, double InUnitsPerMetre, FBattleEffects* InEffects);

	/** The ships of the battle, as they lie at the start. A ship already laid is moved, not made again. */
	void SetShips(const TArray<FStageShip>& Ships);
	/** The same from the view's JSON: view.ships = { guns, pieces: [{ id, cls, nat, name, x, y, face, dock }] }. */
	void SetShipsFromView(const TSharedPtr<FJsonObject>& ShipsView);

	/** air:ship-hit: a bomb or torpedo strikes ship Id. Strike: "torpedo", "dive", "level" or "bomb". */
	void Hit(int32 Id, const FString& Strike, double Now);
	/** air:ship-sunk: ship Id goes down. How: "settle", "sink", "capsize", "explode", "burn" or "break". bBig: the
	 * first battleship or carrier to blow up (the biggest blast of the battle: 238(5)). */
	void Sink(int32 Id, const FString& How, bool bBig, double Now);
	/**
	 * A bomb burst on the land at WorldAt: an oil tank it lands on or beside (inside its wall or within 6 m of it) goes
	 * up, once: the effects kit's fuel ball off its roof (its width, 15 to 25 m) and the fire that burns on after it
	 * (30 minutes), as Midway's tanks on Sand Island burned on 4 June 1942. Each ball's place (world) and width
	 * (metres) is added to OutBalls, for the camera's rule 13. Returns how many tanks went up.
	 */
	int32 BombOnOil(const FVector& WorldAt, TArray<TPair<FVector, float>>& OutBalls);

	/** Once a frame, on the battle's clock (seconds). */
	void Advance(double Now);

	/** The ground's height (world units) under a world point: the land's, or the sea's. */
	double GroundZ(const FVector& Where) const;
	/**
	 * The ground's height (metres) under a board point, as GroundZ gives it, for any thread: a copy of the grid and of
	 * the frames it is read through, made here, so that it reads nothing of this actor (the clouds' painter on the
	 * engine's pool of threads; the pre-check on round 11: a weak pointer's IsValid can answer false there while the
	 * engine sweeps). 0 everywhere while the stage is not raised.
	 */
	std::function<double(double, double)> GroundAside() const;
	/** Where ship Id lies now (world), at her waterline amidships; false if there is no such ship. */
	bool ShipAt(int32 Id, FVector& Out) const;
	/** Ship Id's length (metres), the level way her bow points (world), and whether she lies in harbour (land within
	 * two kilometres); false if there is no such ship. */
	bool ShipLie(int32 Id, double& OutLong, FVector& OutBow, bool& bOutHarbour) const;
	/** The way toward the sun at the hour of the record's attack, in world axes. */
	FVector TowardSun() const;
	/**
	 * Where smoke may stand (world, at the waterline): every ship struck or sunk, and any ship within Near metres of ship
	 * Id (her neighbours, which may burn before a picture of her ends). The eye keeps clear of the columns over them.
	 */
	void SmokeFeet(int32 Id, double Near, TArray<FVector>& Out) const;

	bool IsRaised() const { return bRaised; }

	/** -TheaterStage=<key> on the command line. */
	static bool WantsTest(FString& OutKey);
	/** The stage alone, a strike played on it, pictures taken: see the class's note. */
	void BeginTest(const FString& Key);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FMeshPart
	{
		FString Name;
		TArray<FVector> Points; // stage metres
		TArray<int32> Paints;
		TArray<int32> Triangles;
	};
	struct FShip
	{
		FStageShip Given;
		TObjectPtr<UProceduralMeshComponent> Hull = nullptr;
		TObjectPtr<UProceduralMeshComponent> Fore = nullptr; // the bow's half, once she has broken in two
		FVector Base = FVector::ZeroVector; // local, at her waterline amidships
		double Yaw = 0.0;                   // degrees: her model's bow (-X) put along her heading
		double Long = 100.0, Beam = 12.0, High = 20.0; // metres
		bool bHarbour = true;
		bool bDocked = false; // on a dock's floor (DockFloor): she may burn or roll there, and goes no lower
		FString How;
		double SunkAt = -1.0;
		int32 Hits = 0;
		double FirstHitAt = -1.0; // the battle's time of her first hit (-1: none yet): how long a ship ablaze has burned
		bool bGone = false;
		// Her end's details, from her look (FShipLooks::Sink; the numbers' How kept): the side she rolls or lists to (0:
		// by her id, as before), her end's pace (its times multiplied by it), "bow", "stern", "even", "fore" or "aft",
		// her roll as she settles (degrees, below 0: the stage's own), where she breaks (a share of her length from
		// amidships, minus toward the bow).
		double Side = 0.0, Pace = 1.0, Roll = -1.0, Cut = 0.0;
		FString End;
		double NextChurn = -1.0; // when the sea next boils round her as she goes (MoveShip)
	};

	/** How far from the raid's aim a stage's land may lie and be drawn (metres); past it, land and ground are left out. */
	static constexpr double LandReach = 25000.0;

	FString StageKey;
	FVector2D StageAim = FVector2D::ZeroVector;
	double StageTurn = 0.0;
	FVector2D BoardTarget = FVector2D::ZeroVector;
	double UnitsPerMetre = 100.0;
	bool bRaised = false;
	double Clock = 0.0;

	TArray<FColor> Palette;
	TArray<FString> PaletteNames; // (the ground pass finds its paints by name: AirGround)
	TArray<FMeshPart> Parts;
	TArray<TArray<double>> Tanks, Hangars, Palms, Yard; // as the file gives them (the yard: the historian's 1941 blocks)
	double SunAzimuth = 120.0, SunElevation = 25.0;
	// The ground's height on a coarse grid, in whole metres (stage metres east and north).
	double GridE0 = 0.0, GridN0 = 0.0, GridCell = 100.0;
	int32 GridCols = 0, GridRows = 0;
	TArray<int16> GridZ;
	TSharedPtr<FJsonObject> TestBook;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TArray<TObjectPtr<UTheaterStillMesh>> StillMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> ShipMeshes;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WaterInk; // (the shallows and reef: the ground's ink without its grain)
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SeaInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SteelInk;
	/** The moving sea's slopes (CampaignTextures::MakeWaterBumps), made once, for M_StageSea. */
	UPROPERTY()
	TObjectPtr<UTexture2D> SeaRipples;
	/** Its gusts and the surf's sets (CampaignTextures::MakeWaves: the swell, red, and the slow unevenness, alpha). */
	UPROPERTY()
	TObjectPtr<UTexture2D> SeaWaves;
	/** Its whitecaps (SeaFace::MakeFoam), made once. */
	UPROPERTY()
	TObjectPtr<UTexture2D> SeaFoam;
	/** Its shore: where the water lies calm, in a lee, open to the ocean, and how far from the edge (SeaFace::MakeShore),
	 * made from this stage's land, reef and works when it is raised. */
	UPROPERTY()
	TObjectPtr<UTexture2D> SeaShore;
	/** Where the shore picture lies on the stage (metres: its west and south edges and its side), as SeaFace::MakeShore
	 * laid it; the sea and the shallows carry where each of their points falls on it as their first texture
	 * coordinate (ShoreAt). */
	double ShoreWest = -1.0, ShoreSouth = -1.0, ShoreSpan = 2.0;

	TMap<int32, FShip> Ships;
	/** How each hit on a ship looks, and the details of her end (Battle/Fates). */
	FShipLooks Looks;
	/** The oil columns burning, by ship: how much each matters (StageParts::OilRank). */
	TMap<int32, int32> OilLit;
	static constexpr int32 MostOilFires = 8;
	/** The oil tanks (Tanks' rows) a bomb has set alight (BombOnOil): each goes up once. */
	TSet<int32> TanksAlight;
	FBattleEffects* Effects = nullptr;
	TUniquePtr<FBattleEffects> OwnEffects; // the stage test's own
	FRandomStream Shown = FRandomStream(238);

	bool ReadStage(const FString& Key);
	void BuildGround();
	void BuildWorks();
	void BuildSea();
	/** The moving sea (M_StageSea) for the sea and the shallows, its waves laid along the trade wind, gusts and
	 * whitecaps over it, calm where the shore shuts it in (bShore: the stage's land is drawn, so its shore is worked
	 * out; else the open sea everywhere); null where the material has not been built yet (Tools/build_content.sh), or
	 * with -TheaterFlatSea. */
	UMaterialInstanceDynamic* MovingSea(bool bShore);
	/** Where a stage point (metres east, north) falls on the shore picture: M_StageSea reads the picture there, from the
	 * mesh's own first texture coordinate (the Mac's release 2 check: placed from the world's position on the GPU, the
	 * picture missed Pearl Harbor, and the lochs drew whitecaps and no green). */
	FVector2D ShoreAt(double East, double North) const;
	void LayShip(const FStageShip& Given);
	void MoveShip(FShip& Ship, double Now);
	void Blast(FShip& Ship, bool bBig);

	FVector2D StageToBoard(const FVector2D& Stage) const;
	FVector2D BoardToStage(const FVector2D& Board) const;
	FVector BoardToLocal(const FVector2D& Board, double ZMetres) const;
	FVector StageToLocal(const FVector& Stage) const;
	FVector StageToWorld(const FVector& Stage) const;
	double StageHeight(double East, double North) const;
	/** The floor a ship lies on at a stage point if it is inside a dock's basin (metres up from the sea), else 0. */
	double DockFloor(const FVector2D& Stage) const;
	double HeadingToYaw(double BoardHeading) const;

	// The stage test.
	struct FTestShot
	{
		FString Name;
		double At = 0.0;
		FVector Eye = FVector::ZeroVector, Look = FVector::ZeroVector; // stage metres
		double Lens = 50.0;
	};
	struct FTestBeat
	{
		double At = 0.0;
		int32 Ship = 0;
		FString What, How;
		bool bBig = false;
	};
	bool bTest = false;
	FString PendingTest;
	FString TestFolder;
	double TestStart = -1.0;
	TArray<FTestShot> TestShots;
	TArray<FTestBeat> TestBeats;
	int32 NextShot = 0, NextBeat = 0;
	double ShotTakenAt = -1.0;
	UPROPERTY()
	TObjectPtr<ACameraActor> TestCamera;
	void StartTest(const FString& Key);
	void TestPlan();
	void TestTick();
	/** A picture run that cannot go on (no stage file, no camera): says why and quits rather than hang. */
	void QuitTest(const FString& Why);
};
