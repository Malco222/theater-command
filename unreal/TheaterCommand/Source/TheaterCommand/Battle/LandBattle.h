#pragma once

#include "CoreMinimal.h"
#include "Effects/BattleEffects.h"
#include "GameFramework/Actor.h"

#include "LandBattle.generated.h"

class ACameraActor;
class ACampaignEnvironment;
class ACampaignMapActor;
class FJsonObject;
class FLandCrowd; // TK1
class FLandVat; // SO1
class FLandLive;
class FFrameGrabber;
class SLandBattleScreen;
class SWidget;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UMeshComponent;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class USkyLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
struct FLandBattleState;
namespace LandCarpet
{
	class Squares;
}
namespace LandKit
{
	class Model;
}
struct FLandSky;

/**
 * The ground battle's 3D scene (release 2; Brian, 21 November 2026: "when a soldier gets hit have their body be on
 * the ground dead", "i want it to be realistic", the same intense feeling as the air battle, "as if im watching" a war
 * film, a feeling only, nothing copied; "lets do a normandy one with shermans vs tiger tanks and soldiers and
 * artillery", "a dry run of a land battle just like the air").
 *
 * The battle is the rules' (src/battle/land and the squad brain): a recorded battle, Content/Data/LandBattles/<key>.json
 * (Tools/land_record), is played back exactly as it was fought. Everything the scene decides is in plain C++ cores the
 * cloud test plays too (Tools/land_play, land_show, land_field, land_kit):
 *  - LandPlay: every round fired, once: its flash, its sound, where it lands and what it does there.
 *  - LandSound: what is heard (the sound director's mix): every shot on the screen, cracks, flash-to-bang, the beds.
 *  - LandEye: the camera, shot by shot among the men, a few seconds ahead of what is about to happen.
 *  - LandDesk: the words and the tug-of-war bar.
 *  - LandField: the bocage: banks and hedges, sunken lanes, fields, orchards, the village, out to the haze.
 *  - LandMen: every soldier, his clips, his shots and his fall; the dead stay where they fell to the end.
 *  - LandKit: the Shermans, the Tigers, the guns and the lorries: tracks, turrets, recoil, knock-outs and fires.
 * This actor only lays what they say into the world, plays it and draws the screen (SLandBattleScreen).
 *
 * "-TheaterLandBattle=<key>" plays a recording; with "-TheaterLandBattleShots=<folder>" it takes the art director's
 * pictures there and quits. The world: Unreal X the field's north (y), Y its east (x), Z up, 100 units a metre, as the
 * air battle's; the field's origin (its south-west corner) at the world's origin.
 */
UCLASS()
class THEATERCOMMAND_API ALandBattle : public AActor
{
	GENERATED_BODY()

public:
	ALandBattle();

	/** "-TheaterLandBattle=<key>" on the command line (normandy-1944). */
	static bool WantsReplay(FString& OutKey);
	/** Plays a recorded battle two seconds in, once the table and its light are up (so that they are put away after). */
	void ReplayWhenReady(const FString& Key);
	/** Plays a recorded battle now. False, with a warning, if it is missing or unreadable. */
	bool BeginReplay(const FString& Key);

	/**
	 * "-TheaterLandLive=<key>" on the command line (normandy-1944): the set piece fought live, the player its general
	 * (release 2; Brian, 22 November: "im the general who controls their deployment and who to attack and move on a map
	 * with tactics"). "-TheaterLandSide=" says his side: att (the attack, the default), def, or none (a film, the staffs
	 * commanding both sides); "-TheaterLandSeed=<n>" a seed of the piece's own; "-TheaterLandNoDeploy" skips the setup step;
	 * "-TheaterLandPlatoons" draws every platoon (about 2,000 men at Normandy) where sections are the default;
	 * "-TheaterLandScript=<folder>" presses his steps in code for a builder with no hand on the mouse, a still of each.
	 */
	static bool WantsLive(FString& OutKey);
	/** Opens a live battle two seconds in, as ReplayWhenReady does, its side and seed from the command line. */
	/** A live battle opened from the game's own menus (the scenarios page's practice battle): Me "att", "def" or "" (a
	 * film), Seed 0 the battle's own. */
	void LiveWhenReady(const FString& InKey, const FString& Me, int32 Seed, bool bDeploy);
	void LiveWhenReady(const FString& Key);
	/** Starts the rules on their own thread (LandLive) on the set piece Key, the player on side Me ("att", "def", or ""
	 * for a film), deploying first when bDeploy; the scene opens when the rules have opened the battle. False if not. */
	bool BeginLive(const FString& Key, int32 Seed, const FString& Me, bool bDeploy);
	/**
	 * A campaign battle fought live from its ticket (release 2, the production lead's (a); off unless CampaignLiveOn):
	 * LookJson is the campaign's TC.battleLook answer as it stands ({ "ticket": .., "live": { fog, seen, designYears,
	 * sky } }) or a bare FieldTicket, opened in the land rules (TC.landStartTicket), the player on side Me, deploying
	 * first when bDeploy. Its result, once over, is LiveResult, for the campaign's TC.battleSettleLand.
	 */
	bool BeginLiveTicket(const FString& LookJson, const FString& Me, bool bDeploy);
	void LiveTicketWhenReady(const FString& LookJson, const FString& Me, bool bDeploy);
	/** "-TheaterLandTicket=<file>": a campaign battle's look (or ticket) from a JSON file, fought live as the set piece
	 * is by -TheaterLandLive (its side and setup by the same switches): the test path for a campaign battle. */
	static bool WantsLiveTicket(FString& OutLookJson);
	void LiveTicketWhenReady(const FString& LookJson);
	/** The switch for the campaign's own battles fought live in the land rules: on, unless "-TheaterLandNoCampaignLive"
	 * or the console variable TheaterLandCampaignLive 0. Off: the attack card offers SEND THEM IN only. */
	static bool CampaignLiveOn();
	/** The live battle's result once it is over, as the campaign's TC.battleSettleLand takes it ({ "over": .. }). */
	bool LiveResult(FString& OutJson) const;
	/**
	 * (release 2) Where the campaign opened this battle for the player to fight (FIGHT IT YOURSELF): run once when he
	 * presses CONTINUE on the after card, with LiveResult's answer (empty where there is none), after the table is back.
	 */
	TFunction<void(const FString&)> HandBack;
	/**
	 * (release 2, camera specials) The names corps have earned (nicknames.ts), by the campaign's corps id, where the map
	 * shows them on the flags: given by the campaign before the battle opens. With -TheaterLandNickCard the film cards a
	 * corps' name on its first kill it can show (LandEye World::Nickname, by the block's corps). Empty: no card.
	 */
	TMap<int32, FString> CorpsNicknames;
	bool IsLive() const { return Live.IsValid(); }
	/**
	 * The general's word to the rules (LandCommand): an order (TC.landOrder's command as JSON, given at the battle time
	 * shown unless it says its own "at"), a block placed in the setup step (Face in radians, 0 north, clockwise), the setup
	 * step ended. The rules answer within a frame or two; a refusal is said on the screen in their own words.
	 */
	void OrderLive(const FString& CommandJson);
	void PlaceLive(int32 Block, double X, double Y, double Face);
	void BeginFight();
	/** The scene's state for the general's screen to change (his hand, what is armed): the screen's own. */
	FLandBattleState* EditState() { return State.Get(); }
	/**
	 * The general's pointer (LandBattleCommand.cpp), in the viewport's pixels: pressed (Button 0 the left, 1 the right),
	 * moved, let go; OnTag the block whose tag it is on (-1 none). A click picks his block up or gives the order in hand;
	 * a drag draws a box round his blocks, the right a line for them, and in the setup step carries one of his blocks.
	 */
	void GeneralPress(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag);
	void GeneralMove(const FVector2D& Pixel, int32 OnTag);
	void GeneralRelease(const FVector2D& Pixel, int32 Button, bool bShift, int32 OnTag);
	/** His word from a button or a key: double (the next order at the double), fire (fire on), hold, fallback, staff,
	 * next, back (Tab, Shift-Tab), clear (Esc), film (C), map (Esc from the film), begin. */
	void GeneralDo(const FString& Word);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// What the screen reads and asks.
	const FLandBattleState* GetState() const { return State.Get(); }
	/** 0 (held), 0.5, 1, 2 or 4. */
	void SetSpeed(double Speed);
	/** "cut" (a new shot now), "hold" (keep this shot until pressed again), "continue" (the after card). */
	void Press(const FString& Button);
	/** A drag on the screen: the hand-held camera looks a little aside (pixels), and comes back when let go. */
	void Look(const FVector2D& DeltaPixels);
	bool IsShotHeld() const { return bShotHeld; }
	/** The flag pictures for the bar's two ends: the attack's (his) and the defence's. */
	UTexture2D* FlagOf(int32 Side) const;
	/** The film in a corner (release 2, "-TheaterLandCorner"): the film director's shot drawn small this frame, for
	 * the general's screen to show in its window; null where it is off or the film has the whole screen. */
	UTextureRenderTarget2D* CornerPicture() const
	{
		if (!bCornerShown)
		{
			return nullptr;
		}
		return CornerFilm;
	}

private:
	TSharedPtr<FLandBattleState> State;
	TUniquePtr<FBattleEffects> Effects;
	TSharedPtr<SLandBattleScreen> Screen;
	TSharedPtr<SWidget> ScreenFrame;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<ACameraActor> Eye;

	/** The paint: vertex colours on the campaign's surface (sRGB in, made linear there); the ground tiles on the battle's
	 * own ground surface (M_LandSurface: the same colours with the ground's grain), or on the campaign's without it. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ThingInk;
	/** The hedges' and trees' leaves on their cards (M_LandLeaves: cut out, both faces lit; build_content.py); none
	 * without it, and then the shrubs and trees carry their solid sprigs instead. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LeafInk;
	/** The corn's stalks near a low lens (M_LandStalks: lit as foliage, faded in past the lens and out where the bundles
	 * take over; build_content.py); none without it, and then the corn keeps its bundles to the lens. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StalkInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ManInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SteelInk;
	/**
	 * The wind (release 2; Brian, 23 November: "can we make the grass, trees, and sky more visually realistic and
	 * epic?"), on with "-TheaterLandWind" (or "=strength", 1 as worked out from the recording's wind) or
	 * "-TheaterLandEpic": the grass, the corn and a region's blades (BladeInk), its low leafy plants and bushes (BushInk)
	 * and the shrubs' and trees' wood (WoodInk) on M_LandBlades (build_content.py), each bent by the gusts running over
	 * the field, as the leaves (LeafInk) and the corn's stalks (StalkInk) are, and the gusts' sheen over the grass and
	 * the corn (and the ground's, GroundInk). None, and nothing moves, with the wind off or M_LandBlades missing.
	 */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BladeInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BushInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WoodInk;
	/** The art director's round 1 (23 November): the ripe grain's own (GrainInk: the wheat, bending toward the ear and
	 * nodding back, its sheen stronger), the lively trees' (LivelyInk and their leaves LivelyLeafInk: poplars, birches,
	 * willows, bamboo, two to three times an oak's sway, their leaves quicker) and the palms' (PalmInk, PalmLeafInk:
	 * the fronds lifting and tossing). */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GrainInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LivelyInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LivelyLeafInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PalmInk;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PalmLeafInk;
	/** The clouds' shadows on the field (M_LandCloudShade, the light function on the battle's sun), on with
	 * "-TheaterLandCloudShadows" (or "=darkness", the sun's share left under a cloud, 0.2) or "-TheaterLandEpic". */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudShadeInk;
	bool bWindOn = false;
	/** The sun's shafts through the crowns, the hedges and the smoke when it stands low (under 25 degrees) before the
	 * lens ("-TheaterLandSunShafts", or "=bloom", 0.2; or "-TheaterLandEpic"). */
	bool bSunShafts = false;
	float SunShaftBloom = 0.2f;

	// ---- The ground (LandField) ----
	/** One drawn square of the field (or of the far ring): its ground at two levels, what stands on it, and the
	 * hedges', trees' and houses' instances laid into one mesh at two levels. */
	struct FTileDrawn
	{
		FBox2D Box;
		/** Each a static mesh of its own (GS8: drawn from the engine's cached draw commands), or with
		 * -TheaterLandTilesProcedural a procedural mesh as before. */
		TWeakObjectPtr<UMeshComponent> Near, Far, Built;
		/** Once a hole is dug in it, its near ground by parts (LandCrater::Dug), drawn in Near's place. */
		TArray<TWeakObjectPtr<UProceduralMeshComponent>> NearParts;
		bool bNearShown = false;
		double FarFrom = 0.0;
	};
	TArray<FTileDrawn> Tiles;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> FieldMeshes;
	/** GS8: the ground tiles' own static meshes and their components. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> GroundKept;
	/** The ground's things (hedge shrubs, trees, gates; LandField's bases) are far too many to lay into meshes (some
	 * 129,000 of them, 150 million triangles at their near level): each kind is made once as a mesh of its own, in three
	 * shades of its instances' tints, its near level the mesh's first level of detail and its far the second, and drawn
	 * by instancing. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ThingMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ThingCopies;
	/** A hedge shrub standing at a hedge cutter's gap (the recording's cuts), drawn by a few copies of its own and not
	 * the field's, so that shaking it and throwing it down touches only those: which cut, its copies (in ThingCopies)
	 * and its place there, and where it stood (the field's metres, its turn and sizes, LandField::Placed). */
	struct FCutShrub
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Copies;
		int32 Cut = -1, Index = 0;
		double X = 0.0, Y = 0.0, Z = 0.0, Yaw = 0.0, Scale = 1.0, ScaleZ = 1.0;
	};
	TArray<FCutShrub> CutShrubs;
	/** Each cut's stage (the recording's order): 0 the hedge whole, 1 she is pushing into it (from T0), 2 torn (T1). */
	TArray<uint8> CutStage;
	int32 CutsTorn = 0;

	/** Release 2's field telephones (the recording's wires): every wire's lengths of a metre or so, copies of one piece
	 * of wire laid on the ground (WireCopies, drawn out to some 60 m), and the field telephones at each wire's two
	 * ends (PhoneCopies). */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> WireCopies;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> PhoneCopies;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> PropMeshes;
	/** A length of wire: its wire (Wires' order), its two ends (the field's metres), and where it is laid now. */
	struct FWireLength
	{
		int32 Wire = 0;
		double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;
		bool bHidden = false;
	};
	TArray<FWireLength> WireLengths;
	/** A wire as it stands: its lengths in WireLengths (From, Count), cut or whole, a cut's hidden lengths and its two
	 * curled ends' places in WireCopies (-1: none yet). */
	struct FWireState
	{
		int32 Id = 0, From = 0, Count = 0, CurlA = -1, CurlB = -1;
		bool bCut = false;
		TArray<int32> Hidden;
		/** Its break (field metres) and the wire's way there; its two curled ends as laid; whether a lineman holds
		 * them in his fists this frame (MakeReels: the ends run from his hands to the ground while he splices). */
		double CutX = 0.0, CutY = 0.0, WayX = 1.0, WayY = 0.0;
		FTransform CurlAAt = FTransform::Identity, CurlBAt = FTransform::Identity;
		bool bSpliced = false;
	};
	TArray<FWireState> WireStates;
	/** The next of the recording's wire cuts and mends not yet shown, and the holes already laid when the wires were
	 * last settled on the ground. */
	int32 NextWireCut = 0, NextWireMend = 0, WireHolesSeen = 0;
	/** The bearers' stretchers (Content/Models/Battle's Stretcher, one copy a carry) and the aid posts' stretchers. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> StretcherCopies;
	/** A carry: the man carried (a track), his two bearers (front, rear: tracks), its battle times, and its copy's place
	 * in StretcherCopies (shown or put away). */
	struct FCarrySpan
	{
		int32 Carried = -1, By1 = -1, By2 = -1, Copy = -1;
		double From = 0.0, To = 0.0;
		bool bShown = false;
	};
	TArray<FCarrySpan> CarrySpans;
	/** The linemen's cable reels (Content/Models/Battle's CableReel): one copy a lineman ("ln") met, carried before him
	 * by its handles while he walks his line out (the soldiers' reel_walk and its `reel` keys), set down by his right
	 * knee while he splices, put away otherwise; the copies of a live battle's reset given back to Free. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ReelCopies;
	struct FReelMan
	{
		int32 Track = -1, Copy = -1;
		bool bShown = false;
		/** Where his reel was last put by his clip, and when (battle seconds). */
		FTransform LastAt = FTransform::Identity;
		double ShownAt = -1e9;
	};
	TArray<FReelMan> ReelMen;
	TArray<int32> FreeReels;
	int32 ReelTracksSeen = 0;
	bool bReelTried = false;
	/** The wires whose ends a lineman held last frame (their Id): curled again when he lets them go unspliced. */
	TArray<int32> SplicedWires;
	/** Each base's two shade edges (the brightness of an instance's tint, R + G + B), and for the clutter each
	 * brightness third's two warmth edges (R - G; WarmEdges, three a kind): a thing's shade is its brightness third, and
	 * the clutter's is that third split again in thirds by warmth, nine a kind, so that a blade of grass takes about its
	 * own ground's hue (a hay field's straw yellow, a lush one's green) and not the whole field's mean. */
	TArray<FVector2f> ShadeEdges;
	TArray<FVector2f> WarmEdges;
	/** How far from the eye each clutter kind is laid, metres: where it is no longer drawn (its cull) and 12 m more. */
	TArray<float> ClutterReach;
	/** The grass, corn, flowers and stones round the eye and round what it looks at (LandField::Clutter), laid in 16 m
	 * squares as they come into reach and given back as they leave it (Carpet): one instanced mesh a clutter kind and
	 * shade (ClutterCopyOf, -1 none). */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ClutterCopies;
	TArray<int32> ClutterCopyOf;
	/** GS9: a clutter mesh's shadowed twin (by ClutterCopies' number; -1 none): the corn's bundles cast their shadow
	 * only within -TheaterLandCornShadowTo= (25 m) of the eye, laid there in the twin (LandCarpet's ShadowTo). */
	TArray<int32> ClutterTwin;
	TSharedPtr<LandCarpet::Squares> Carpet;
	/** The corn drawn stalk by stalk round the lens (the art director's cut 4, fix 4): whether the stalks' meshes were
	 * made (StalkInk), and whether the eye is low enough for them now (they are shown, and the bundles cleared within
	 * StalkClear of it, only then: from above, a clearing would be a bald ring in the corn). */
	bool bStalksMade = false;
	bool bStalksShown = false;
	/** The shell holes (LandCrater): torn earth on the ground's own paint, one mesh a patch (a 64 m square of the
	 * field, by its number), one section for each 24 of its holes. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> CraterMeshes;
	/** Effects (R2): the fighters on call, one mesh a plane by its key (FlyOver). */
	UPROPERTY()
	TMap<int32, TObjectPtr<UProceduralMeshComponent>> AirSupportMeshes;
	/** The track marks (LandCrater::Ruts), the same way: one mesh a 64 m patch, a section for each 48 pieces, sent again
	 * a quarter second after a piece is laid in it. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> RutMeshes;
	double RutsSentAt = -10.0;
	/** GS8: the torn ground's pieces (a section of a patch's holes or tracks, a part of a dug tile's near ground) are sent
	 * into their procedural meshes as they change, and once a piece has stood unchanged TornSettleAfter seconds it is
	 * baked into a static mesh of its own (cached draw) and its procedural section emptied: a patch's procedural mesh
	 * then holds only its pieces still changing, and a send makes only those again. A change makes a baked piece
	 * procedural again. -TheaterLandTornProcedural: never baked, as before. */
	struct FTornPiece
	{
		/** 0: holes (CraterMeshes), 1: tracks (RutMeshes), 2: dug ground (a tile's NearParts). */
		int32 Kind = 0;
		/** The patch and section, or the tile and part. */
		int32 A = 0, B = 0;
		double ChangedAt = 0.0;
		bool bHot = false;
		TWeakObjectPtr<UStaticMeshComponent> Baked;
	};
	static int64 TornKey(int32 Kind, int32 A, int32 B)
	{
		return (static_cast<int64>(Kind) << 56) | (static_cast<int64>(A & 0xFFFFFF) << 28) | static_cast<int64>(B & 0xFFFFFFF);
	}
	TMap<int64, FTornPiece> TornPieces;
	TArray<int64> TornHot;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TornBaked;
	bool bTornBakes = true;
	double TornSettleAfter = 3.0, TornBakeMs = 1.0;
	/** For the 10 s log: sections sent, pieces baked, and the slowest frame's baking (ms). */
	int32 TornSent = 0, TornBakedCount = 0;
	double TornBakeMostMs = 0.0;

	// ---- The men (LandMen) ----
	struct FManDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Lod = -1;
		uint32 Stamp = 0xFFFFFFFFu;
		/** Whether his mesh casts a shadow now (made with one; none past about 80 m as seen). */
		bool bShadow = true;
		/** Banked far (SO1): his place among the instances of his set's pose (BankCopies; -1 none), and the frame it was
		 * last put at (his WorldFrame's R and T), so a man standing still is not put again. */
		int32 BankCopy = -1, BankSlot = -1;
		float BankFrame[12] = {};
		/** Effects (R2): his crowd mesh's number when his triangles were made (LandMen::Crowd::MeshNo: a gore fall swaps it). */
		uint32 MeshNo = 0;
	};
	TArray<FManDrawn> MenDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> MenMeshes;
	TArray<float> SkinAt, SkinFacing;

	// ---- The far men as instances of the soldier pose bank (the true-scale plan's SO1) ----
	/** One instanced mesh for each (set, pose) shown (LandMen::Crowd::BankMesh), made as first needed, a few a frame;
	 * its places given back are shrunk to nothing (BankFree) for the next man. BankCopyOf: set * poses + pose to the
	 * mesh's number (-2: cannot be made, the man skinned as before). Off with -TheaterLandNoPoseBank. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BankCopies;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> BankMeshes;
	TArray<TArray<int32>> BankFree;
	TArray<uint8> BankDirty;
	/** How many men each mesh holds now (one with none is hidden: its shrunk places would still be drawn). */
	TArray<int32> BankLive;
	TMap<int32, int32> BankCopyOf;
	/** How many banked men's meshes were made this battle, and their cost (ms), for the log. */
	int32 BankMade = 0;
	double BankMadeMs = 0.0;

	// ---- The mid and far men as one crowd of posed instances (SO1's draw: Battle/LandVat) ----
	/** None without M_LandCrowd or with -TheaterLandNoVat: then far men are drawn a mesh a set and pose (above) and mid
	 * men skinned, as before. What it makes is held in VatKept. */
	TSharedPtr<FLandVat> Vat;
	UPROPERTY()
	TArray<TObjectPtr<UObject>> VatKept;
	/** In the wide mass shot a far man is drawn no smaller than MassMinPx of the picture's lines (GS3's measure): grown
	 * about his feet ("-TheaterLandMassMinPx=", 2; 0 none). For the 10 s log: men placed as instances (mid, far), men
	 * grown and the most grown, and the card's own frame time (ms, summed). */
	double MassMinPx = 2.0;
	/** In the general's own view, high over the field, a far man is drawn no smaller than GeneralMinPx of the picture's
	 * lines ("-TheaterLandGeneralMinPx=", 4; 0 none): his men are always on the screen (the art director, 22 November). */
	double GeneralMinPx = 4.0;
	int32 VatMid = 0, VatFar = 0, MassGrown = 0;
	double MostGrow = 1.0, GpuMs = 0.0;
	/** For the 10 s log (FX2): the most smoke puffs the effects kit drew in a frame, and the room it was asked for then. */
	int32 PuffsMost = 0, PuffsMostAsked = 0;

	// ---- The tanks, guns and lorries (LandKit) ----
	struct FBodyDrawn
	{
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		int32 Vehicle = 0, Body = 0;
		bool bTrack = false;
		bool bNear = true;
		/** Drawn only while her roof gun is manned (a German tank's MG 34, its cradle part). */
		bool bWhileManned = false;
		/** A wireless aerial's length from its mounting, metres (0: any other body): put away near the picture's eye,
		 * and for the whole of the film's ride on her while the film is the picture (DrawArmour, LandEye::AerialShown). */
		double AerialLong = 0.0;
		double SootShown = 0.0;
		/** TK1: whether its far level is a section of its own (a body the crowd merges has none), and whether her
		 * instances draw it now (its sections hidden). TK3: the run its links were last posed for (-1e300: to be posed). */
		bool bFarSection = false;
		bool bHeld = false;
		double LinksRun = -1e300;
	};
	TArray<FBodyDrawn> BodiesDrawn;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> BodyMeshes;
	TArray<float> LinkPoints;
	/** TK1: far vehicles as instances of their models' merged far pieces, and TK3's who is in the picture
	 * (Battle/LandCrowd); what it makes is held here. */
	TSharedPtr<FLandCrowd> FarCrowd;
	UPROPERTY()
	TArray<TObjectPtr<UObject>> CrowdKept;

	// ---- The footprint shade (LandShade on M_LandShade: the art director's cut 4, fix 3) ----
	/** The shade's ink (none without M_LandShade: then no shade is drawn, said once), its meshes (one a kind of tank,
	 * gun or lorry, by her model's name, ShadeCopyByName) drawn by instancing, and each vehicle's instance. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShadeInk;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ShadeMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShadeCopies;
	TMap<FString, int32> ShadeCopyByName;
	struct FShadeDrawn
	{
		/** The model her shade was made for (a live battle's reset can change who she is). */
		const LandKit::Model* Model = nullptr;
		int32 Copy = -1, Slot = -1;
		bool bShown = false;
		FVector At = FVector::ZeroVector;
		FQuat Q = FQuat::Identity;
		/** Her shade's size as laid (1: her own; the general's view grows it with her and lays it as her cast shadow). */
		double Size = 1.0;
	};
	TArray<FShadeDrawn> ShadesDrawn;
	/** The vehicles' shades laid for the general's view (grown, cast away from the sun, seen to 3 km) this frame. */
	bool bShadesHigh = false;
	/** Each shade mesh's instances given back (shrunk to nothing), to be used again before a new one is added. */
	TArray<TArray<int32>> ShadeFree;
	/** The men's: a round blob under a standing man, his body's shape under a prone one, a wider blob under a body at
	 * rest; one instanced mesh each (in that order), an instance a man within 150 m as the picture sees him. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ManShadeCopies;
	struct FManShade
	{
		int32 Kind = -1, Slot = -1;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.0f;
		/** His place as last posed (field metres, PlaceAsPosed): unchanged, nothing of the ground is asked. */
		FVector Gate = FVector(1e30);
		/** The ground's up where it was last read (field metres), read again once he is half a metre from there. */
		FVector UpAt = FVector(1e30);
		FVector Up = FVector::UpVector;
	};
	TArray<FManShade> ManShades;
	TArray<TArray<int32>> ManShadeFree;
	bool bManShadesMade = false;

	// ---- The sound (LandSound, the sound director's mix) ----
	TSharedPtr<FJsonObject> Mix;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> Families;
	TSet<FName> SoundsMissing;
	/** The beds, one loop each, at the level the mixer says. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UAudioComponent>> BedPlayers;
	/** Each vehicle's engine loop, and the cue it plays. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UAudioComponent>> Engines;
	TMap<int32, FName> EngineCue;
	/** Takes that move with their shooter: the take and its track. */
	struct FMoving
	{
		TWeakObjectPtr<UAudioComponent> Player;
		int32 Track = -1;
		/** Its voice in the pool and the take it played there (-1: a player spawned for it alone). */
		int32 Voice = -1;
		int32 Use = 0;
	};
	TArray<FMoving> Moving;
	/** Takes started in the last two seconds, for the ears' dulling after a blast close by. */
	struct FRecent
	{
		TWeakObjectPtr<UAudioComponent> Player;
		double Started = 0.0;
		int32 Voice = -1;
		int32 Use = 0;
	};
	TArray<FRecent> Recent;
	/** The ears dulled by a blast close by: when it began, its low-pass, and how long down, held and back. */
	double ShockAt = -100.0, ShockLpf = 600.0, ShockDown = 0.1, ShockHold = 1.2, ShockBack = 2.5, ShockDb = -6.0;
	/** The real seconds each take of the mix lasts (read from the files as they load). */
	TMap<FName, float> TakeSeconds;
	/** The whole battle's lift down among the men this frame (mix.json land.closeView), dB: the engines take it here, the
	 * beds and takes in the mixer. */
	double LiftDb = 0.0;
	/** The master limiter at buses.master.limiterCeilingDb while the scene is up (taken off in EndPlay). */
	UPROPERTY()
	TObjectPtr<class USoundEffectSubmixPreset> Limiter;
	/** When the sound's tally was last logged (real seconds). */
	double SoundLogAt = 0.0;
	/** The one-shot takes' players, pooled (mix.json land.voices; LandSound::Voices in the state keeps which take each
	 * plays): made once and reused, not one spawned and destroyed for every take. Off with land.voices.pool false or
	 * -TheaterLandVoicesSpawned (each take spawned, as before 22 Nov). */
	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> VoicePlayers;
	bool bPoolVoices = false;
	/** The voice and its use the last StartTake played on (-1: spawned, not pooled). */
	int32 LastVoice = -1;
	int32 LastVoiceUse = 0;
	/** Short takes out of hearing where they start, given no voice (the engine's spawn makes no player for them). */
	int32 VoicesUnheard = 0;
	/** Each engine loop's name, one for each of LandSound::EngineNames' (worked out once, not every frame). */
	TArray<FName> EngineLoopNames;
	/** The buses' gains, read from the mix once each (StartTakes asked the mix's tree for every take). */
	mutable TMap<FString, double> BusDbs;

	// ---- The flags for the bar ----
	UPROPERTY()
	TObjectPtr<UTexture2D> FlagAttack;
	UPROPERTY()
	TObjectPtr<UTexture2D> FlagDefence;

	// ---- What the table had before the battle took its place (as the air battle keeps it) ----
	struct FKeptLamp { TWeakObjectPtr<UDirectionalLightComponent> Lamp; FRotator Rotation; float Intensity = 0.0f; FLinearColor Colour; float Temperature = 0.0f; float SourceAngle = 0.0f; FLinearColor SunDisc = FLinearColor::White;
		// (the wind and the clouds' shadows, 23 November: the sun's light function and shafts as the table had them)
		TWeakObjectPtr<UMaterialInterface> Function; float FunctionFade = 0.0f, FunctionDisabled = 0.0f; bool bShaftBloom = false, bShaftOcclusion = false; float BloomScale = 0.0f, BloomThreshold = 0.0f, ShaftDark = 0.0f; FColor BloomTint = FColor::White; };
	struct FKeptSky { TWeakObjectPtr<USkyLightComponent> Sky; float Intensity = 0.0f; FLinearColor Colour; bool bLowerBlack = true; FLinearColor Lower = FLinearColor::Black; };
	struct FKeptHaze { TWeakObjectPtr<UExponentialHeightFogComponent> Haze; float Start = 0.0f, Density = 0.0f, Falloff = 0.0f, MostOpacity = 0.0f; FLinearColor Colour; };
	TArray<FKeptLamp> KeptLamps;
	TArray<FKeptSky> KeptSkies;
	TArray<FKeptHaze> KeptHazes;
	TArray<TWeakObjectPtr<ACampaignEnvironment>> KeptRooms;
	TArray<TWeakObjectPtr<AActor>> KeptPieces;
	TWeakObjectPtr<ACampaignMapActor> Table;
	TWeakObjectPtr<UDirectionalLightComponent> BattleKey;
	bool bTableHidden = false;
	/** The way toward the sun (world), from the recording's place, date and hour. */
	FVector TowardSun = FVector(0.3, 0.5, 0.8);
	/** The day over the field: the recording's weather, the light it gives (key, sky fill, sky light), its clouds and
	 * its haze (FLandSky, in LandBattle.cpp). Made in HideTheTable. */
	TSharedPtr<FLandSky> SkyDay;

	// ---- The camera's own state ----
	/** A drag's look aside (degrees of yaw and pitch), easing back to nothing when let go. */
	FVector2D LookAside = FVector2D::ZeroVector;
	double LookAsideAt = -10.0;
	bool bShotHeld = false;

	// ---- The run ----
	FString PendingReplay;
	FString Key;
	double UnitsPerMetre = 100.0;
	double RealT = 0.0;
	/** The battle's clock held, the first time the game is run, until the materials' shaders are compiled. */
	bool bShadersReady = false;
	double ShaderWait = 0.0;
	bool bBegun = false;
	bool bOwnInput = false;
	/** r.ScreenPercentage as the campaign had it, while "-TheaterLandSize=" holds its own; -1 none. */
	float KeptScreenPercent = -1.0f;
	/** The map's light trims as they stood when the battle began (console variable, value), put back as it ends; and
	 * its sun's contact shadow length (under 0: not changed). */
	TArray<TPair<FString, FString>> KeptTrims;
	/** The film's shallow focus (E9): r.DepthOfFieldQuality as the battle found it (the map's 0, or a capture run's
	 * film set), the quality the scene has set now, and whether the camera's close shots may turn it on
	 * (off unless "-TheaterLandFocusLive=1", until the Mac's A/B and the art director pass it). */
	int32 FocusQualityKept = -1;
	int32 FocusQualityNow = -1;
	bool bFocusLive = false;
	float KeptContactShadow = -1.0f;
	bool bKeptContactInWorld = false;
	/** The log every 10 s: frames, the slowest, what the cores and the drawing cost. */
	double LogAt = 0.0, SlowestFrame = 0.0, CoresMs = 0.0, MenMs = 0.0, ArmourMs = 0.0, FieldMs = 0.0;
	/** ...and the camera's own line (the production lead's CD0, the Mac gate's 10 s line): LandEye's frame and the
	 * general's view, ms over the span and the slowest, the most ray steps in a frame; the cuts; and the longest gap
	 * between two frames, with how many frames it came after a cut (0: the cut's own frame), to split a shot change's
	 * hitch between the camera's choice and the renderer's start afresh. */
	double EyeMs = 0.0, EyeMostMs = 0.0, GenEyeMs = 0.0, GenEyeMostMs = 0.0, LongestGapMs = 0.0;
	int64 EyeStepsMost = 0;
	int32 EyeCuts = 0, FramesSinceCut = 1000, LongestGapSinceCut = 1000;
	/** ...and the drawing's own parts (the men's meshes, the tanks', the effects kit, the sound), the men in the picture
	 * and their meshes made again or held at the old level (the GS3 cap) over the span; the most in one frame. */
	double DrawMenMs = 0.0, DrawArmourMs = 0.0, EffectsMs = 0.0, SoundMs = 0.0;
	/** A live battle's own parts, ms over the log's 10 s: the general's view (his camera) and his screen (tags, routes,
	 * chips, the pointer); with the rules' step, said on the log's live line. */
	double GeneralViewMs = 0.0, GeneralScreenMs = 0.0;
	int32 LogFrames = 0, MenInPicture = 0, MenRebuilt = 0, RebuildsPut = 0, MostInPicture = 0;

	// ---- The picture run (-TheaterLandBattleShots) ----
	FString ShotFolder;
	/** The walk pan ("-TheaterLandWalkPan=<battle s>[,<seconds>]", the Mac recipe's look 7): from that battle second the
	 * eye stands off a walking block's flank and turns with it (LandWalkPan), a still each real second. Battle second
	 * it starts at (-1 none), how long, the stills' next real time and count, the tries left to find a walking block
	 * (10 s later each), whether it holds the eye now, and its look, smoothed. The tank pan ("-TheaterLandTankPan=", the
	 * same form; the far tanks' motion vectors, LandCrowd) uses the same: a tank driving 170 to 240 m off, past the
	 * hand-over, on a long lens (bTankPan). */
	double WalkPanAt = -1.0, WalkPanFor = 20.0, WalkPanStillAt = -1.0;
	int32 WalkPanStills = 0, WalkPanTries = 6;
	bool bWalkPanOn = false, bTankPan = false;
	FVector WalkPanLook = FVector::ZeroVector;
	void WalkPan(double Real);
	/** The dark reading for the camera director's dark rule (LandEye::World::DarkNow; the art director, cut 4 fix 6 and
	 * cut 5: a shot over 30% darker than L 25 of 255 is left): the drawn frame copied at 64 x 36 four times a real second
	 * by the engine's frame grabber, which waits on nothing, and the share of it under L 25 (-1 until the first comes
	 * back, or with no game viewport, as in the editor). Off unless "-TheaterLandDarkRead" asks for it (1.5.2: its cost on
	 * the Mac was over the patch's bar); "-TheaterLandNoDarkRead" leaves it off either way (the rule never asked).
	 * ReadDark takes the frame's length, as its neighbours do, and keeps the reading's own clock from it (DarkClock,
	 * against which the next ask and the last reading's age are measured: a reading is stale, and the rule told
	 * there is none, once no frame has been read for six tenths of a second). DarkAsks and DarkBacks count the
	 * frames asked for and those that came back, DarkReads those read (none under a result card or the night's
	 * light, nor while the general's own view is the picture drawn and not the film, nor while a drag has the lens
	 * looking aside from the shot): the battle's end line says all three, and warns where under half of those
	 * asked for came back. */
	TSharedPtr<FFrameGrabber> DarkGrab;
	double DarkShare = -1.0, DarkClock = 0.0, DarkAskAt = 0.0, DarkGotAt = 0.0;
	int32 DarkReads = 0, DarkAsks = 0, DarkBacks = 0;
	/** The film's log (the tick's, after its stills are asked for): when its last line was written, on the tick's real
	 * clock, for the four a second of "-TheaterLandPoseLog". */
	double PoseLogAt = 0.0;
	void StartDarkRead();
	void ReadDark(double Real);
	void StopDarkRead();
	/**
	 * The night before the attack (LandNight, release 2's patrol, behind the player's patrol switch): a recording that
	 * carries the patrol opens on the night, the battle's clock waiting at its start. The night's look is laid on the
	 * eye's lens over the day's grade (NightLens: the day's values kept to come back to), as a film shoots day for
	 * night: the exposure down, the colour drained and cooled, the picture's dip to black between the night and the
	 * battle; the flares are lamps that throw shadows, each with its burning glow ("-TheaterLandFlare=candelas").
	 * "-TheaterLandNoNight" leaves the night out; "-TheaterLandPatrol[=found,caught]" puts a stand-in patrol into a
	 * recording without one (LandNight::AddStandInPatrol; 1,1 when bare). A key or a click skips to the dawn.
	 */
	struct FNightLens
	{
		float Stops = 0.0f;
		FVector4 Darks = FVector4(1.0, 1.0, 1.0, 1.0), Middles = FVector4(1.0, 1.0, 1.0, 1.0), Gain = FVector4(1.0, 1.0, 1.0, 1.0);
		bool bKept = false, bOn = false;
	};
	FNightLens NightLens;
	TArray<TWeakObjectPtr<UPointLightComponent>> FlareLamps;
	TArray<TWeakObjectPtr<UProceduralMeshComponent>> FlareGlows;
	TWeakObjectPtr<UMaterialInstanceDynamic> FlareInk;
	double FlareCandela = 1.3e6;
	void StartNight();
	void NightLook();
	void FlareAt(int32 Index, const FVector& At, double Light, bool bRising);
	struct FShot
	{
		FString Name;
		/** Battle second it is taken at (the shot asked for a little before). */
		double At = 0.0;
		/** The shot asked of the camera (LandEye::Kind as a number; -1 the camera's own choice), and on whom. */
		int32 Kind = -1;
		int32 Subject = -1;
		bool bAsked = false;
		bool bTaken = false;
	};
	TArray<FShot> Shots;
	int32 ShotsTaken = 0;
	/** The camera specials in a picture run (release 2): the special on the screen now (LandEye::Special as a number, -1
	 * none), since when (real seconds), whether its still is taken, and how many were. */
	int32 SpStillKind = -1;
	double SpStillFrom = 0.0;
	bool bSpStillTaken = false;
	int32 SpStills = 0;
	double QuitAt = -1.0, CardShotAt = -1.0;
	bool bCardShot = false;

	// ---- The battle fought live (LandBattleLive.cpp) ----
	/** The rules on their own thread, fighting the battle a little ahead of the screen (none for a recording). */
	TSharedPtr<FLandLive> Live;
	FString PendingLive, LiveMe;
	/** A campaign battle's look waiting to open (LiveTicketWhenReady). */
	FString PendingTicket;
	int32 LiveSeed = 0;
	bool bLiveDeploy = false;
	/** Started, and waiting on the rules to open the battle (its header): for how long, real seconds. */
	bool bLiveWaiting = false;
	double LiveWaited = 0.0;
	/** Real seconds the clock has waited on the rules this time; whether they have stopped (and it was said). */
	double LiveHeld = 0.0;
	bool bLiveBroken = false;
	/** The tanks' meshes made again (a block placed anew in the setup step, her vehicles changed): their names' round. */
	int32 BodyRound = 0;
	/** A live battle's opening, frame by frame: true while it has the frame (the scene not open yet). */
	bool TickLiveOpening(float DeltaSeconds);
	/** Each frame before the clock: the rules' answers, then the pieces fought, laid into the recording and the cores. */
	void PumpLive();
	/** Each track's man and vehicle from TracksFrom on (all of them after a reset), and the new vehicles' meshes. */
	void MapLive(int32 TracksFrom, int32 VehiclesFrom, bool bReset, bool bVehiclesKept);
	/** The battle's clock this frame never past what the rules have fought (less a margin); and the rules kept ahead. */
	double LiveClock(double From, double To, double Real);
	void CloseLive();
	/** BeginLive's and BeginLiveTicket's own: a set piece by Key, or a campaign battle's look where LookJson is given. */
	bool BeginLiveFrom(const FString& InKey, int32 Seed, const FString& Me, bool bDeploy, const FString& LookJson);

	// ---- The general's controls (LandBattleCommand.cpp) ----
	/** His pointer pressed: where, which button, and whether it has moved past a click since. */
	FVector2D GeneralDownAt = FVector2D::ZeroVector;
	int32 GeneralButton = -1;
	bool bGeneralMoved = false;
	/** A block carried in the setup step (-1 none), and its facing. */
	int32 GeneralCarry = -1;
	double GeneralCarryFace = 0.0;
	/** His last click on open ground with an order given there: when (real seconds) and where (pixels), for a second
	 * click there that sends them at the double. */
	double GeneralClickAt = -10.0;
	FVector2D GeneralClickWhere = FVector2D::ZeroVector;
	/** Each frame, after the clock: the clipboard brought to the time shown, its tags and routes put on the screen. */
	void ShowGeneral();
	/** Each frame, after the film's pose: the general's own view takes the picture while his map is up (the film's
	 * again on C), cutting where the two switch. */
	void GeneralView(double Real);
	/** The field's ground under a pixel (metres, the field's own x east and y north), false off the field. */
	bool GroundUnder(const FVector2D& Pixel, double& OutX, double& OutY) const;
	/** A point of the field on the screen (pixels), false behind the camera. */
	bool PixelOf(double X, double Y, double Lift, FVector2D& OutPixel) const;
	/** How far a click reaches on the ground at a pixel, metres (some 24 pixels' worth there). */
	double ReachAt(const FVector2D& Pixel) const;
	/** The order in hand given (TC.landOrder's command): sent, shown at once, and the click answered on the screen. */
	/** Only: the blocks it goes to, where not all those in hand (the cutter alone, those with smoke). */
	void GiveGeneral(const FString& Command, const FString& Kind, int32 Target, const TArray<FVector2D>& Points, double PingX, double PingY, const TArray<int32>& Only = TArray<int32>());
	/** Hedge cutters: the point on a hedge (a hedgerow, a lane's bank, a garden's) within Reach of (X, Y) where a gap would
	 * be cut; false none so near. */
	bool HedgeNear(double X, double Y, double Reach, double& OutX, double& OutY) const;
	/** Smoke on call: those in hand with smoke lay it from (X0, Y0) to (X1, Y1). */
	void GiveSmoke(double X0, double Y0, double X1, double Y1);

	// ---- The general's walk pressed in code (LandBattleCommand.cpp, its choices LandScript.h) ----
	/** "-TheaterLandScript=<folder>": for a builder with no hand on the mouse, his steps pressed through his own pointer
	 * (two units carried into the woods, the battle begun, one sent forward, one at an enemy, one firing into a hedge or
	 * a wood, a unit's plate held up), a still of each into the folder and each said in the log; then three minutes at
	 * the battle's own pace, the rest at four times, the after card's still, and the game quits. Empty: none. */
	FString ScriptFolder;
	int32 ScriptStep = 0;
	/** When the step began (real seconds), and the battle time an order went. */
	double ScriptAt = 0.0, ScriptGiven = 0.0;
	/** The unit the step works with, the enemy it attacks, where it goes (field metres) and that place on the screen. */
	int32 ScriptUnit = -1, ScriptFoe = -1;
	double ScriptX = 0.0, ScriptY = 0.0;
	FVector2D ScriptPixel = FVector2D::ZeroVector;
	/** Pressed through his pointer (the place on the screen), else given directly (off the screen). */
	bool bScriptByHand = false;
	int32 ScriptStills = 0;
	/** "-TheaterLandTopViewAt=<battle seconds>" (the Mac's check of his top view, 24 November): when it is pressed (-1
	 * none; -2 not yet read) and how far the check has gone. */
	double TopCheckAt = -2.0;
	int32 TopCheckStep = 0;
	/** The Mac's check of his kept views, the spotter's mark and the film in a corner (release 2,
	 * "-TheaterLandViewsAt=<battle seconds>": -1 none; -2 not yet read) and how far it has gone. */
	double ViewsCheckAt = -2.0;
	int32 ViewsCheckStep = 0;
	/**
	 * The film in a corner (release 2; Brian, 24 November 20:38Z): while he commands from his own view, the film
	 * director's shot in a small window. A second picture of the field, drawn small through the film's lens every few
	 * frames. Off without "-TheaterLandCorner" until the Mac holds 45 fps with it and the art director has passed it;
	 * "-TheaterLandCornerWide=<pixels>" (480), "-TheaterLandCornerEvery=<frames>" (2: every other frame),
	 * "-TheaterLandCornerNoShadows" and "-TheaterLandCornerTrueWithin=<metres>" (40) are the Mac's trials. CornerOn: 1
	 * on, 0 off, -1 not yet read.
	 *
	 * One field is drawn for both lenses, and it is laid out for his own. What the window gets of its own: the men in
	 * the film's picture are posed and placed as those in his are (DrawMen), and those of them within CornerTrueWithin
	 * of the film's lens are drawn their own size, not grown as his far men are (a man 2.5 times his size before a low
	 * lens is a giant; the cost is that those few stand smaller in his own picture while the film is on them; 0: all
	 * keep his growth). Its known limits, all by his own eye alone: the men's level of detail and how often a far man
	 * is posed (a man more than 80 m from HIS eye as his picture sees him, and from the top view that is every man, is
	 * the far figure in the bank's poses in the window too, however close the film's own lens stands) and his shade
	 * (none past 150 m of his eye); the vehicles past 140 m of his eye drawn as far ones, grown in his high view, with
	 * their barrels' sticks and flatter ink; a vehicle's wireless aerial, put away by how near HIS eye stands to it
	 * (DrawArmour), so a tank the film rides keeps hers before the film's lens, the thin line through the picture
	 * that the film as the whole picture no longer has; the effects kit, which lays only what is on his own screen, thinned by
	 * his distance, its puffs in his lens's plane (a film shot outside his picture has no smoke, bursts, flashes or
	 * tracers; under his top view the puffs lie flat); and the near ground's detail and grass, laid round his eye.
	 * All of it goes by how far the film's subject is from his own eye and picture, not by how high he stands: least
	 * when the film looks at what he is looking at from near by; in full when its shot is outside his picture or far
	 * from his eye, the top view included. Mending those needs a second eye through the men, the vehicles, the
	 * effects kit and the ground: not built until the Mac's look says the window earns it.
	 */
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> CornerLens;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> CornerFilm;
	int32 CornerOn = -1, CornerWide = 480, CornerEvery = 2, CornerFrame = 0, CornerTaken = 0;
	/** bCornerLens: the window's lens is up and aimed this frame (FilmCorner). bCornerShown: and it holds a picture
	 * taken since it came up (FilmCornerTake), so the screen shows it. */
	bool bCornerLens = false, bCornerShown = false;
	/** CornerMs: what the window's pictures have cost to ask for since the last log line. CornerSendMs: what the
	 * frame's own end-of-frame sending, done before each picture, has cost (the frame pays that with or without the
	 * window). */
	double CornerMs = 0.0, CornerSendMs = 0.0, CornerTrueWithin = 40.0;
	/** Each frame after the lens is aimed: the window's lens set up and aimed where it is shown; and once the men and
	 * the vehicles are drawn, its picture taken. */
	void FilmCorner();
	void FilmCornerTake();
	/** The spotter's mark (release 2, "-TheaterLandSpotter", off without it): 1 on, 0 off, -1 not yet read. On only
	 * where the battle hides anyone (the Forest ambushes rule). */
	int32 SpotterOn = -1;
	/** His units the walk has used, and where it set those it carried into the woods. */
	TArray<int32> ScriptUsed;
	TArray<FVector2D> ScriptTaken;
	/** Each frame after his tags are on the screen: the walk's next step when its time comes. */
	void TickScript();
	void ScriptStill(const TCHAR* Name);
	void ScriptNext(int32 Step);

	bool Open();
	void LoadInks();
	void LayField();
	/** Scarred ground (release 2): the old holes of the battles fought here before, laid as the battle opens. */
	void LayScars();
	/** Mud memory (release 2): the ruts of the battles fought here in mud before, laid as the battle opens. */
	void LayOldRuts();
	/** The near things (hedges, trees, gates) laid into a tile's mesh, or taken out. */
	void LayThings();
	/** A thing's shade, 0 to 8: three times its brightness third, and its warmth third (the middle one, 1, for a thing
	 * that is not clutter). */
	int32 ShadeOf(int32 Base, uint8 R, uint8 G, uint8 B) const;
	void ShowField(const FVector& EyeWorld, double Battle);
	void LayClutter(const FVector2D& EyeAt, const FVector2D& LookAt, bool bCut, double EyeHigh);
	/** The instanced mesh of set Set (LandMen::Crowd::BankKey) in pose Pose: made now if Builds allows (counted down),
	 * else -1 for this frame; -2 if it cannot be made. */
	int32 BankCopyFor(int32 Set, int32 Pose, int32& Builds);
	/** His place among the banked instances given back (shrunk to nothing where he stood). */
	void BankOut(FManDrawn& D);
	void MakeMen();
	void DrawMen(const FVector& EyeWorld);
	void MakeArmour();
	/** The tank commanders: a rider in the crowd for each American and German tank his crew file has a place on, with
	 * her roof gun's bursts and his end read from the recording (after the replay has begun: its TankMg). */
	void MakeRiders();
	/** Each commander seated and posed for this frame (after the tanks are posed, before the men are updated). */
	void RideTanks();
	/** Effects (R2): the crews of knocked-out tanks getting out, -TheaterLandBailOut (LandBail). */
	void BailOut();
	/** Effects (R2): the fighters on call over the field, -TheaterLandAirSupport (LandAir; Battle/LandBattleAir.cpp):
	 * each plane drawn where the plan has it, its guns, the ground's guns firing up at it, and the one lost going down. */
	void FlyOver();
	/** The men on each light car and motorcycle from FromVehicle on (release 2's jeeps, field cars and motorcycles): a
	 * rider in the crowd for each seat her kind fills that his army's infantry file has a clip for. */
	void SeatLight(int32 FromVehicle);
	/** Each of them seated and posed for this frame (with RideTanks). */
	void RideLight();
	/** The lorries' and half-tracks' benches as they stand now, for the men aboard (FLandBattleState::Carriers). */
	void SeatCarriers();
	/** Each vehicle's bodies from FromVehicle on: one mesh a body, its near level as section 0 and its far as 1. */
	void MakeBodies(int32 FromVehicle);
	/** A man's or a vehicle's block is the enemy's, hidden from the player in woods and not yet found: not drawn. */
	bool HiddenMan(int32 Man) const;
	bool HiddenVehicle(int32 Vehicle) const;
	void DrawArmour(const FVector& EyeWorld);
	/** The instanced mesh of a model's footprint shade, made the first time it is asked for (-1: none). */
	int32 ShadeCopyFor(const LandKit::Model& Model);
	/** Each vehicle's footprint shade under her as she stands, moved only when she has. */
	void DrawShades();
	/** Each near man's: under him as he stands, lies or lies dead, tilted to the ground. */
	void DrawManShades();
	void HideTheTable();
	void KeepTheTableHidden();
	void LightTheBattle();
	/** The day's cumulus or deck, the air battle's own (AirSky), laid once over the field; DrawSky turns them to the eye. */
	void LaySky();
	void DrawSky();
	/** The battle's eye kept as the camera the picture is drawn through, and its grade said in the log once. */
	void KeepTheLook();
	void ShowTheTable();
	void MakeFlags();

	void PlayEffects();
	/** This frame's hedge cutters (the recording's cuts): from T0 the shrubs at the gap shaken as she pushes into the
	 * bank, at T1 the bank torn through (LandCrater's breach, sent by MakeCraters with the holes) and its shrubs thrown
	 * down the way she drove. */
	void MakeBreaches();
	/** This frame's shells, bombs and grenades into the ground: their holes, kept the whole battle. */
	void MakeCraters();
	/** The field telephones' wires laid on the ground and their telephones (none where the recording has no wires). */
	void LayWires();
	/** This frame's wire cuts and mends, and the lengths over a new hole laid on its face. */
	void MakeWires();
	/** A length of wire placed from its ends, on the ground as it is now (its holes and gaps). */
	FTransform WireAt(double X0, double Y0, double X1, double Y1) const;
	/** The stretcher bearers' stretchers, one a carry, put away until it starts; and two stretchers laid by each aid
	 * post (none where the recording carries no carries). */
	void LayStretchers();
	/** This frame's stretchers: each carry's under its man between his two bearers, the rest put away. */
	void MakeStretchers();
	/** This frame's linemen: each one's reel in his hands or by his knee, and a cut wire's two ends in his fists while
	 * he splices it (the soldiers' splice_kneel and its `hands` keys). */
	void MakeReels();
	/** This frame's tracks on the ground: each moving tank's and lorry's marks, the corn laid flat behind her. */
	void MakeRuts();
	/** The dug ground's parts (LandCrater::Dug): made when a tile is first dug in, sent again when cut. */
	void SendDug();
	/** GS8: a torn piece just sent into its procedural section (its baked mesh, if any, let go). */
	void TornChanged(int32 Kind, int32 A, int32 B);
	/** GS8: the torn pieces unchanged TornSettleAfter seconds baked into static meshes, within TornBakeMs a frame. */
	void SettleTorn();
	void PlayArmourEvents();
	void PlayMenEvents();
	void PlaySound(double RealDt);
	/** The mixer's takes due now (this frame's and those held for later: flash to bang, the 0.15 s rule). */
	void StartTakes();
	/** The sound's tally in the log, every 20 s and at the end. */
	void LogSound() const;
	void PlayEngines();
	UAudioComponent* StartTake(const TCHAR* Name, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn);
	USoundBase* SoundOf(const FName& Name, bool bLoop = false);
	USoundAttenuation* FamilyOf(const FName& Name);
	/** A pooled voice for a take (StartTake with land.voices on). */
	UAudioComponent* StartPooled(USoundBase* Played, const FName& Take, const FVector& Where, bool bSpatial, const TCHAR* Family, float Volume, float Pitch, float From, float Cut, float CutFade, float FadeIn);
	UAudioComponent* MakeVoice();
	/** The pool's players stopped since the last frame freed; the cuts due now faded out. */
	void FreeVoices();
	void FadeVoices();
	/** Whether a take started on a pooled voice still holds it (always true for a spawned one). */
	bool StillHeld(int32 Voice, int32 Use) const;
	double BusDb(const FString& Bus) const;
	void AimEye(float DeltaSeconds);
	void TakeShots();
	void ShowScreen();
	void HideScreen();
	void Finish();

	FVector World(double X, double Y, double Z) const { return FVector(Y * UnitsPerMetre, X * UnitsPerMetre, Z * UnitsPerMetre); }
	FVector WorldWay(double X, double Y, double Z) const { return FVector(Y, X, Z); }
	double GroundZ(double X, double Y) const;
	FString GroundWordAt(double X, double Y) const;
};
