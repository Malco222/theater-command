#pragma once

#include "CoreMinimal.h"
#include "Battle/LandEye.h"
#include "Battle/LandKit.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <vector>

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
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
 * 0 to 1: per-instance custom number 0), and whether she is shown (her fade, the fog of war). A vehicle not drawn by
 * a piece holds no instance in it: each piece's list is kept packed, as the men's crowd keeps its own (LandVat: the
 * piece's last vehicle moved into the place given back, and the places past the last removed off the end once a
 * frame), so nothing hidden is drawn. Her paint is her model's (a model is painted once a nation and month): nothing
 * else differs.
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
 * Her commander's lid: a tank whose commander's lid stands half open or more (Vehicle::HatchOpen; she is unbuttoned, or
 * a wreck whose crew has bailed out) is drawn with the lid-up copy of the piece that holds it (LandKit::FarSplit::Open:
 * her open_up clip played to its end) in that piece's stead, so past the hand-over her lid stands up and her commander
 * stands clear of it, not through a shut lid. A third instanced mesh a model, no more instances a vehicle shown.
 *
 * Her motion vector: each piece's mesh keeps every instance's place a frame before (SetHasPerInstancePrevTransforms, set
 * before it is registered, Movable; LandVat's pattern for the men, 3073236c), so a far tank driving across the picture,
 * or one the lens follows, has her own motion for the anti-aliasing and the motion blur, as her near meshes have theirs.
 * Without it the engine draws an instance moved inside its mesh as still: she smears, or the lens following her blurs
 * her with the ground behind. Each move sends both places (BatchUpdateInstancesTransforms): where she is and where her
 * instance was drawn the frame before. One not moved again is made still at the next frame (or she would be drawn
 * smeared along her last step for as long as she stands); one moved into a given-back place carries her own place a
 * frame before; one newly drawn by a piece (at the hand-over, or her lid's copy swapped in) takes her anchor body's
 * place a frame before, where her own meshes or the other piece drew her, so neither change shows as a jump.
 *
 * The general's own view (Brian, 22 November 02:55Z; the art director's read of its stills, 18:27Z: "tanks read as
 * grey-blue chips ... give each tank a size floor of about 14x7 render pixels (up to 1.5x); keep the barrel at least 1
 * pixel wide, because today it is gone"): while the scene says it is up (SetHigh: his view high over the field, not his
 * film), a far vehicle that would stand smaller in the picture than "-TheaterLandGeneralTankPx=" (14,7,1.5: her length
 * and her width in the lines of the men's 665-line measure, GS3, and how much she may grow at most) is drawn grown about
 * the ground under her middle until she stands that big, by her pieces (each instance grown about that one point, so her
 * turret stays on her hull) and, through GrowOf, by her own meshes and her shade (the scene's). Her barrel is drawn
 * over by a stick of her barrel's paint from her gun's pivot to her muzzle, at least "-TheaterLandGeneralBarrelPx=" (1)
 * line wide (a fourth instanced mesh a model, one instance a vehicle, only while the view is up and she is drawn by her
 * pieces). And her pieces' ink takes "-TheaterLandGeneralTankInk=" (0.1,0.9,1,1,1: its specular, its roughness and a
 * tint) while the view is up, so her top faces give back less of the blue sky; her near ink is as it was. Out of the
 * view (or in his film) all of it is as before.
 *
 * "-TheaterLandNoCrowd" turns TK1 off (every body drawn by its own meshes, as before: the A/B). "-TheaterLandLinksAlways"
 * turns TK3 off (every near moving tank's links posed every frame, as before). "-TheaterLandCrowdNoPrev" turns the places
 * a frame before off (the pieces as before: an instance moved is drawn as still; the look A/B with -TheaterLandTankPan).
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
	 * instances given back. EyeAt: the eye (field metres), as DrawArmour measures the hand-over from. Hidden: a
	 * vehicle not drawn (the fog of war; her fade is read here).
	 */
	void Draw(const std::vector<LandKit::Vehicle>& All, const LandEye::Pose& Pose, const LandPlay::Vec& EyeAt, TFunctionRef<bool(int32)> Hidden);
	/** Whether body Body of vehicle Vehicle is drawn by her instances this frame (her own mesh's sections hidden). */
	bool Holds(int32 Vehicle, int32 Body) const;

	// ---- TK3, from DrawArmour ----

	/** Whether a near tank's track links are posed now: she is in the picture (or close by), or her track mesh was
	 * drawn on the screen a moment ago (bRendered); counted for the log. */
	bool LinksDue(int32 Vehicle, bool bRendered);

	// ---- The general's view, from DrawArmour, before Draw ----

	/**
	 * Whether the general's own view is up this frame (bOn: his view high over the field, not his film), and the picture's
	 * lines a metre at a metre off (the men's measure: MenLines / 1.8). While it is up Draw grows the far vehicles to the
	 * floor, lays the barrels' sticks and gives the pieces' ink its general's settings; off, all as before.
	 */
	void SetHigh(bool bOn, double LinesPerMetre);
	/** How much vehicle Vehicle is grown this frame (1: her own size), about the ground under her middle (her root): what
	 * her own meshes and her shade take where her pieces do not draw her. Set by Draw. */
	double GrowOf(int32 Vehicle) const;

private:
	/** The pieces drawn: the hull's, the turret's, and the lid-up copy of whichever of them holds the commander's lid;
	 * and (the general's view) the stick over her barrel, a box of her barrel's paint anchored on her gun's body. */
	static constexpr int32 PieceCount = 4;
	static constexpr int32 LidUp = 2;
	static constexpr int32 Stick = 3;

	/** One model's pieces: their instanced meshes, and who holds each instance (packed: instance I is Holder[I]'s). */
	struct FKind
	{
		const LandKit::Model* Model = nullptr;
		FString Name, Nation;
		LandKit::FarSplit Split;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Copies[PieceCount];
		TArray<int32> Holder[PieceCount];
		bool bShownNow[PieceCount] = { false, false, false, false };
		bool bDirty[PieceCount] = { false, false, false, false };
		bool bKeepsFar = false;
		bool bMade = false;
		/** Her gun's body (the body of her gun control's part) and whether she has a muzzle: the stick's two ends (-1: she
		 * has no stick). */
		int32 GunBody = -1;
		/** Her pieces' ink when made here (on M_LandSteel), and its specular, roughness and tint as made: the general's
		 * view sets its own while it is up and gives these back after. */
		TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
		float Specular = 0.35f, Roughness = 0.6f;
		FLinearColor Tint = FLinearColor::White;
	};
	/** One vehicle: her model's kind, her instance in each piece (-1: none), and what was last put there. */
	struct FOne
	{
		int32 Kind = -1;
		int32 Slot[PieceCount] = { -1, -1, -1 };
		FVector At[PieceCount] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		FQuat Q[PieceCount] = { FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity };
		/** Each instance's scale as put: her growth in the general's view (1: her own size); the stick's, its length and
		 * width in metres. */
		FVector S[PieceCount] = { FVector::OneVector, FVector::OneVector, FVector::OneVector, FVector::OneVector };
		/** The place a frame before sent with At, Q, S (the motion vector's other end; the same: still). */
		FVector Before[PieceCount] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		FQuat BeforeQ[PieceCount] = { FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity };
		FVector BeforeS[PieceCount] = { FVector::OneVector, FVector::OneVector, FVector::OneVector, FVector::OneVector };
		/** The frame (Frame) each piece's instance was last sent moving; 0: still. */
		uint32 Moved[PieceCount] = { 0, 0, 0, 0 };
		/** Each piece's anchor body's place at the frame WasFrame (drawn by a piece or not), for one newly put there. */
		FVector Was[PieceCount] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		FQuat WasQ[PieceCount] = { FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity };
		FVector WasS[PieceCount] = { FVector::OneVector, FVector::OneVector, FVector::OneVector, FVector::OneVector };
		uint32 WasFrame = 0;
		float Soot = -1.0f;
		/** How much she is grown this frame (the general's view; 1: her own size). */
		double Grow = 1.0;
		bool bHeld = false;
		bool bSeen = true;
	};

	int32 KindFor(const LandKit::Model& Model);
	/** Piece Piece of a model's split: the hull's, the turret's, or the lid-up copy (never the stick, which is no
	 * piece of the split). */
	static const LandKit::FarPiece& SplitPiece(const LandKit::FarSplit& Split, int32 Piece)
	{
		return Piece == LidUp ? Split.Open : Split.Pieces[Piece];
	}
	/** The body piece Piece is anchored on: its split piece's anchor, or (the stick) her gun's body. */
	static int32 AnchorOf(const FKind& Kind, int32 Piece)
	{
		return Piece == Stick ? Kind.GunBody : SplitPiece(Kind.Split, Piece).Anchor;
	}
	UMaterialInterface* InkFor(const LandKit::Model& Model, FKind& Kind);
	/** A kind's stick: a box a metre each way of her barrel's paint (its body's far colours, a shade darker), burnt with
	 * her; on its own instanced mesh. */
	void MakeStick(FKind& Kind, UMaterialInterface* Ink);
	/** A kind's ink set for the general's view (bOn) or given back its own settings. */
	void InkHigh(FKind& Kind, bool bOn) const;
	/** Vehicle Vehicle (One) put in piece Piece at At, Q, S: the place after the piece's last vehicle, with her soot; her
	 * place a frame before Before, BeforeQ, BeforeS (the same: no move). */
	void Put(int32 Vehicle, FOne& One, int32 Piece, const FVector& At, const FQuat& Q, const FVector& S, const FVector& Before, const FQuat& BeforeQ,
		const FVector& BeforeS);
	/** Her instance in piece Piece given back: the piece's last vehicle moved into it (Draw removes the tail). */
	void Drop(FOne& One, int32 Piece);
	void Release(FOne& One);
	/** Instance Slot of Copies put at At, Q, S (world), its place a frame before Before, BeforeQ, BeforeS (the same: no
	 * move); with the places a frame before off, put as before. */
	void SetPlace(UInstancedStaticMeshComponent* Copies, int32 Slot, const FVector& At, const FQuat& Q, const FVector& S, const FVector& Before,
		const FQuat& BeforeQ, const FVector& BeforeS);
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
	/** Each piece keeps its instances' places a frame before (off: -TheaterLandCrowdNoPrev). */
	bool bPrev = true;
	/** The general's view (SetHigh): up this frame, the picture's lines a metre at a metre off, and the floor
	 * ("-TheaterLandGeneralTankPx=": her least length and width in lines, how much she may grow), the barrel's least width
	 * in lines ("-TheaterLandGeneralBarrelPx=", 0: no stick) and the pieces' ink while it is up
	 * ("-TheaterLandGeneralTankInk=": specular, roughness, tint). bInkHigh: the inks are set for it now. */
	bool bHigh = false, bInkHigh = false;
	double HighPerMetre = 0.0;
	double FloorLong = 14.0, FloorWide = 7.0, FloorMost = 1.5, BarrelPx = 1.0;
	float HighSpecular = 0.1f, HighRoughness = 0.9f;
	FLinearColor HighTint = FLinearColor::White;
	/** The frame, counted at each Draw (from 1; 0 is "still"). */
	uint32 Frame = 1;
	/** One instance's place and place a frame before, for the engine's batch update (kept: no array made a move). */
	TArray<FTransform> OneAt, OneBefore;
	// (std::vector: a kind holds LandKit's own vectors, which are moved, never copied bit by bit as a TArray would)
	std::vector<FKind> Kinds;
	std::vector<FOne> Ones;
	// The log's tally (every 10 s): frames, far vehicles drawn by their pieces and by their own meshes, links posed and
	// held out of the picture.
	double TellAt = -1.0;
	int64 Frames = 0, HeldFrames = 0, OwnFarFrames = 0, LinksPosed = 0, LinksHeld = 0;
	// (and the places sent moving and made still again, with the places a frame before)
	int64 SentMoving = 0, MadeStill = 0;
	// (and in the general's view: the frames it was up, the far vehicles grown and their growth summed, sticks drawn)
	int64 HighFrames = 0, Grown = 0, Sticks = 0;
	double GrowSum = 0.0;
};
