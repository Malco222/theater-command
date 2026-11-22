#pragma once

#include "CoreMinimal.h"
#include "Battle/LandEye.h"
#include "Battle/LandKit.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <vector>

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UObject;
class USceneComponent;

/**
 * The ground battle's far tanks, guns and lorries as instances (TK1 of the battle-scale plan's work list, rank 4), and
 * which of them is in the picture (TK3's stopgap: track links posed only there).
 *
 * TK1. Today each moving part of a vehicle is its own shadow-casting procedural mesh, moved every frame: 856 of them in
 * Normandy, 1,572 at Prokhorovka. Past the near/far hand-over (FarFrom, the 140 m of ALandBattle::DrawArmour) a vehicle
 * is drawn here instead as at most two instances of meshes made once a model at load (LandKit::SplitFar): her hull piece
 * (every body not under the turret, at rest) at her root body's place, and her turret piece (the turret, the gun at its
 * rest lay, the hatches shut) at her turret body's place, which is her hull's place turned by her turret's bearing (and
 * goes with a thrown turret). One instanced mesh a model and piece holds every vehicle of that model; a vehicle updates
 * her two instances' places (only when they moved) and her soot, and her own meshes' sections are hidden
 * (SetMeshSectionVisible: no rebuild) and no longer moved. Coming back within FarFrom she is drawn by her own meshes
 * again, the same frame.
 *
 * What she varies from her model's other vehicles: her place, her turret's, her soot (how far she has burnt through,
 * 0 to 1: per-instance custom number 0), and whether she is shown (her fade, the fog of war: an instance not shown is
 * shrunk to nothing). Her paint is her model's (a model is painted once a nation and month): nothing else differs.
 *
 * Her soot needs M_LandSteel (Tools/build_content.py: M_MapSurface with each point taken toward "BurntColour" by its
 * soot weight, the mesh's first texture coordinate's U, times the instance's custom number 0, in the sRGB the mesh
 * keeps, as LandBattle's FromBody does for the near meshes). Without it the pieces are drawn on the steel's own ink and a
 * far wreck shows her paint unburnt (said once in the log).
 *
 * A vehicle is drawn by her pieces only while her bodies stand within a give of where the pieces draw them (LandKit::
 * FarOff; the give 0.25 m at the hand-over, 1.8 mm a metre beyond: about two pixels of the usual lens): a tank or a
 * lorry always (0.22 m at worst over the two recorded battles), a gun while her barrel is near its rest lay and her
 * trails are spread. A gun keeps her own far meshes for the rest (a howitzer laid high stands 0.5 m off, her trails swung
 * for the tow 1.9 m). A body the scene may hide while she stands (LandKit::MayHide: a manned roof gun's cradle) is never
 * merged and keeps its own meshes, far and near, as before.
 *
 * "-TheaterLandNoCrowd" turns TK1 off (every body drawn by its own meshes, as before: the A/B). "-TheaterLandLinksAlways"
 * turns TK3 off (every near moving tank's links posed every frame, as before).
 */
class FLandCrowd
{
public:
	/** The scene's world point of a field point (ALandBattle::World). */
	using WorldFn = TFunction<FVector(double X, double Y, double Z)>;

	/** Where the near level hands over to the far, metres from the eye to her root (ALandBattle::DrawArmour's). */
	static constexpr double FarFrom = 140.0;

	/**
	 * Owner: the scene (the instanced meshes are its components, on Parent). Steel: the near meshes' ink (SteelInk), whose
	 * settings the pieces' ink takes. Keep: an array of the scene's that holds what is made here from the collector.
	 */
	FLandCrowd(AActor* InOwner, USceneComponent* InParent, UMaterialInterface* InSteel, double InUnitsPerMetre, WorldFn InWorld,
		TArray<TObjectPtr<UObject>>& InKeep);

	// ---- TK1, from MakeBodies ----

	/**
	 * Vehicle Vehicle (her Battle::Vehicles() number) is drawn from Model: the model's pieces made if they are not yet,
	 * and her instances taken (a vehicle enlisted again gives back her old ones first). False: she is not drawn by
	 * instances (TK1 off, or her model's pieces could not be made): every body of hers keeps its own far mesh.
	 */
	bool Enlist(int32 Vehicle, const LandKit::Model& Model);
	/** Whether body Body of vehicle Vehicle (just enlisted) needs no far section of its own: it is merged into a piece
	 * and her model does not keep its far meshes (a gun keeps hers, for when she cannot be drawn at rest). */
	bool TakesFar(int32 Vehicle, int32 Body) const;

	// ---- TK1 and TK3, from DrawArmour, before the bodies ----

	/**
	 * Every vehicle this frame: whether she is in the picture (Seen), and whether she is drawn by her pieces (far, shown,
	 * near enough her rest), her instances put where her root and turret bodies are and her soot given; the others'
	 * instances shrunk to nothing. EyeAt: the eye (field metres), as DrawArmour measures the hand-over from. Hidden: a
	 * vehicle not drawn (the fog of war; her fade is read here).
	 */
	void Draw(const std::vector<LandKit::Vehicle>& All, const LandEye::Pose& Pose, const LandPlay::Vec& EyeAt, TFunctionRef<bool(int32)> Hidden);
	/** Whether body Body of vehicle Vehicle is drawn by her instances this frame (her own mesh's sections hidden). */
	bool Holds(int32 Vehicle, int32 Body) const;

	// ---- TK3, from DrawArmour ----

	/** Whether a near tank's track links are posed now: she is in the picture (or close by), or her track mesh was
	 * drawn on the screen a moment ago (bRendered); counted for the log. */
	bool LinksDue(int32 Vehicle, bool bRendered);

private:
	/** One model's pieces: their instanced meshes, the instances given back, how many are shown. */
	struct FKind
	{
		const LandKit::Model* Model = nullptr;
		FString Name, Nation;
		LandKit::FarSplit Split;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Copies[2];
		TArray<int32> Free[2];
		int32 Live[2] = { 0, 0 };
		bool bShownNow[2] = { false, false };
		bool bDirty[2] = { false, false };
		bool bKeepsFar = false;
		bool bMade = false;
	};
	/** One vehicle: her model's kind, her instance in each piece, and what was last put there. */
	struct FOne
	{
		int32 Kind = -1;
		int32 Slot[2] = { -1, -1 };
		bool bOn[2] = { false, false };
		FVector At[2] = { FVector::ZeroVector, FVector::ZeroVector };
		FQuat Q[2] = { FQuat::Identity, FQuat::Identity };
		float Soot = -1.0f;
		bool bHeld = false;
		bool bSeen = true;
	};

	int32 KindFor(const LandKit::Model& Model);
	UMaterialInterface* InkFor(const LandKit::Model& Model);
	void Park(FOne& One, int32 Piece, const FVector& Where);
	void Release(FOne& One);
	void Tell(int32 Shown);

	TWeakObjectPtr<AActor> Owner;
	TWeakObjectPtr<USceneComponent> Parent;
	TWeakObjectPtr<UMaterialInterface> Steel;
	TWeakObjectPtr<UMaterialInterface> SootParent;
	bool bSootLooked = false;
	double U = 100.0;
	WorldFn World;
	TArray<TObjectPtr<UObject>>* Keep = nullptr;
	bool bOff = false, bLinksAlways = false;
	// (std::vector: a kind holds LandKit's own vectors, which are moved, never copied bit by bit as a TArray would)
	std::vector<FKind> Kinds;
	std::vector<FOne> Ones;
	// The log's tally (every 10 s): frames, far vehicles drawn by their pieces and by their own meshes, links posed and
	// held out of the picture.
	double TellAt = -1.0;
	int64 Frames = 0, HeldFrames = 0, OwnFarFrames = 0, LinksPosed = 0, LinksHeld = 0;
};
