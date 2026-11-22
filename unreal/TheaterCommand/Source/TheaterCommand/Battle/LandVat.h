#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UObject;
class USceneComponent;
class UStaticMesh;
namespace LandMen
{
	class Crowd;
}

/**
 * The ground battle's mid and far men as one crowd of instances (the true-scale plan's SO1, in the vertex-animation form
 * the Mac study wants; the soldiers thread's bake, LandMen::Crowd::BankBody).
 *
 * Before: a man past the near level was skinned on the cores and his whole mesh sent to the card every time his pose
 * moved (mid), or drawn as an instance of a mesh made for his set in one pose (far: one mesh and component for each set
 * and pose shown, some 2,156 of them in Normandy once mid men are banked too).
 *
 * Here: each body set (men alike out to 25 m but for helmet and face: LandMen::Crowd::BodyKey) at each level (Mid, Far)
 * is ONE mesh at rest and ONE instanced component, on M_LandCrowd (Tools/build_content.py, crowd_poses). Every pose of
 * the bank is in one picture made here at load: a column a point, a row a pose, each point's way from its rest in the
 * first BankPoses() rows and its facing in the next as many, half floats, Unreal's units. Each man is one instance: his
 * place (his WorldFrame, ALandBattle's BankedAt) and nine numbers: the two poses he is between and the second's share
 * (BankShow), the same a frame before (BankShowBefore, for the anti-aliasing), and his face's tone (FaceTone). The card
 * moves every point; nothing is skinned on the cores and nothing but the instances' places and numbers is sent.
 *
 * His helmet (where his file draws its finishes apart: LandMen::Crowd::HelmetOf) is an instance of its own, one
 * component for each set, finish and level, on the men's ink (no poses: it rides his head, HelmetFrame).
 *
 * Mid instances cast a shadow, far ones none (GS3: a man's shadow ends some 80 m off as seen). A set over 2,048 points
 * at a level (the point's number is a half float on the mesh, exact to 2,048) or one the bank cannot give is not made:
 * its men are drawn as before. A man given back (skinned again near, fallen, out of sight) has his place filled by the
 * last man of his component, and the places past the last man are removed at the frame's end, so a component draws
 * only men (GS7; before, his instance was left shrunk to nothing for the next man of his set and level).
 *
 * Each component keeps every instance's place a frame before (SetHasPerInstancePrevTransforms, set before it is
 * registered: the engine reads it as the component's draw is made), so a man's walk has its motion vector for the
 * anti-aliasing, as his limbs have theirs from the poses a frame before. The engine's own update never sets that place
 * (pre-check, UE 5.8: without it an instance moved inside its component reads as still), so each move sends both: the
 * new place and the one he stood at the frame before. A man who stops, or is left where he was out of the picture, has
 * the two made one at the next frame's end, so he does not smear; a place new to him, or filled from his part's end,
 * is sent with no move.
 *
 * "-TheaterLandNoVat" draws the men as before (the A/B).
 */
class FLandVat
{
public:
	/**
	 * Owner: the scene (the instanced components are its, on Parent). Crowd: M_LandCrowd. Ink: the men's ink (ManInk),
	 * whose settings the crowd's ink takes, and on which the helmets are drawn. Keep: an array of the scene's that holds
	 * what is made here from the collector.
	 */
	FLandVat(AActor* InOwner, USceneComponent* InParent, UMaterialInterface* InCrowd, UMaterialInterface* InInk, double InUnitsPerMetre,
		TArray<TObjectPtr<UObject>>& InKeep);

	/** Makes every body set's picture, mesh and component at Mid and Far, and the helmets its men wear. False: none made
	 * (the men are drawn as before). Said in the log, with its cost. */
	bool Make(const LandMen::Crowd& Men);
	/** Whether every body set the crowd has was made at level L (LandMen::Mid or Far). */
	bool HasAll(int32 Level) const;
	/**
	 * A live battle's men come with its pieces (release 2): its header has none, so Make finds no set. Grow makes the
	 * sets and helmets met on the men added since Make or the last Grow, and the levels banked on a set since (a set is
	 * banked the first time a man of it is met, at each level the first time one stands there). Each part is tried once.
	 * The number of parts made. bAgain: the crowd was made again (a live reset): every man looked at again, and the
	 * places of men past its count given back.
	 */
	int32 Grow(const LandMen::Crowd& Men, bool bAgain = false);
	/** A set banked at the mid level whose part could not be made there (its mid men must then be skinned). */
	bool MidMissing() const { return bMidMissing; }

	/**
	 * Man Man drawn here this frame: Body is where his set's mesh stands (in Parent's frame: BankedAt of his WorldFrame,
	 * grown in the mass shot), Helmet his helmet's (null: none). Only what changed is sent. False: he cannot be drawn here
	 * (not banked, or no part made for his set and level): draw him as before, and give his instance back (Out).
	 */
	bool Place(const LandMen::Crowd& Men, int32 Man, const FTransform& Body, const FTransform* Helmet);
	/** Gives man Man's instances back (none: nothing). */
	void Out(int32 Man);
	/** At the frame's end: the men moved the frame before and not since have their place a frame before made their
	 * place now, the components whose instances moved are sent, and one holding no man is hidden. */
	void Send();

	/** The cost of Make (ms) and the pictures' size on the card (MB), for the log. */
	double MadeMs() const { return MadeMsTotal; }
	double PicturesMB() const { return PictureBytes / (1024.0 * 1024.0); }

private:
	struct FPart
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Copies;
		/** The man each place draws, in order: kept packed (GS7), places past the last man removed at Send. */
		TArray<int32> Holder;
		bool bDirty = false;
		bool bPosed = false;
	};
	struct FMan
	{
		int32 Body = -1, BodySlot = -1;
		int32 Helmet = -1, HelmetSlot = -1;
		FTransform BodyAt = FTransform::Identity, HelmetAt = FTransform::Identity;
		/** The place a frame before sent with his place now (the motion vector's other end). */
		FTransform BodyBefore = FTransform::Identity, HelmetBefore = FTransform::Identity;
		/** The frame (Frame) his body or helmet was last sent moving; 0: still, its place a frame before his place now. */
		uint32 BodyMoved = 0, HelmetMoved = 0;
		float Data[9] = {};
	};

	static int64 BodyId(int32 Key, int32 Level) { return (static_cast<int64>(Key) << 8) | static_cast<int64>(Level & 0xFF); }
	static int64 HelmetId(int32 Key, int32 Finish, int32 Level)
	{
		return (int64(1) << 62) | (static_cast<int64>(Key) << 32) | (static_cast<int64>(Finish & 0xFFFFFF) << 8) | static_cast<int64>(Level & 0xFF);
	}
	bool MakeBody(const LandMen::Crowd& Men, int32 Key, int32 Level);
	bool MakeHelmet(const LandMen::Crowd& Men, int32 Key, int32 Finish, int32 Level);
	int32 AddPart(UStaticMesh* Mesh, UMaterialInterface* PartInk, int32 Level, bool bPosed);
	/** Man Man's body (bHelmet false) or helmet put in part Wanted at At (Data: the body's nine numbers). */
	void Put(int32 Man, bool bHelmet, int32 Wanted, const FTransform& At, const float* Data);
	/** Man Man's body or helmet given back: his place filled by his part's last man, the last place left for Send. */
	void Drop(int32 Man, bool bHelmet);
	/** Instance Slot of Copies put at At, its place a frame before Before (the same: no move). */
	void SetPlace(UInstancedStaticMeshComponent* Copies, int32 Slot, const FTransform& At, const FTransform& Before);

	TWeakObjectPtr<AActor> Owner;
	TWeakObjectPtr<USceneComponent> Parent;
	TWeakObjectPtr<UMaterialInterface> CrowdInk;
	TWeakObjectPtr<UMaterialInterface> Ink;
	double U = 100.0;
	TArray<TObjectPtr<UObject>>* Keep = nullptr;
	TArray<FPart> Parts;
	TMap<int64, int32> PartOf;
	TArray<FMan> Drawn;
	/** The frame, counted at each Send (from 1), and the men sent moving this frame and the frame before (Man * 2, plus 1
	 * for his helmet): those not moved again are made still at Send. */
	uint32 Frame = 1;
	TArray<int32> MovingNow, MovingBefore;
	/** One instance's place and place a frame before, for the engine's batch update (kept: no array made a move). */
	TArray<FTransform> OneAt, OneBefore;
	TSet<int32> Keys;
	/** Grow's: the men looked at so far, the helmets met (key, finish), and the parts tried (made or not). */
	int32 GrownTo = 0;
	TSet<int64> WornMet, Tried;
	bool bMidMissing = false;
	double MadeMsTotal = 0.0;
	double PictureBytes = 0.0;
};
