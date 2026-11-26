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
struct FMiniatureMesh;

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
	 * (Ruling 154.) An army he only glimpses through the haze is a flag and no more: its flag, its finial and a "?"
	 * under the cloth; no base, no pieces, no shadow, no number, and its pole gone before it reaches the ground. It
	 * reads as a report pinned on the haze, not as an army standing there. Set before SetArmy.
	 */
	void SetGlimpsed(bool bOn) { bGlimpsed = bOn; }
	/** Whether snow lies where the piece stands this month: an army of a country that whitewashes its vehicles is then painted so. Set before SetArmy. */
	void SetOnSnow(bool bOn) { bOnSnow = bOn; }
	bool IsGlimpsed() const { return bGlimpsed; }
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
	/** How long the march it is on (or was last on) takes, in seconds. */
	double MarchLasts() const { return MarchTime; }
	/**
	 * (Release 2, armies with weight: Campaign/ArmyCarry.h.) Whether pieces are carried with weight: tc.Map.ArmyWeight,
	 * or -TheaterArmyWeight on the command line. The map asks, for the dust a set-down raises.
	 */
	static bool CarriedWithWeight();
	/** Whether snow lies where it stands (see SetOnSnow). */
	bool IsOnSnow() const { return bOnSnow; }
	/**
	 * (Armies with weight.) Another piece has been set down on this one, having come the way Along points (flat on
	 * the table; nothing, and no way is known): it rocks under the weight. Nothing with the switch off.
	 */
	void TakeWeight(const FVector& Along);
	/**
	 * (Armies with weight.) The map turns every army piece to face the eye, at this yaw. One that leans (carried, or
	 * rocking after a set-down) keeps its lean and faces the new way; one that stands is turned as it always was.
	 */
	void FaceEye(float Yaw);
	/** Upright, the way it faces on the table: its turn, less any lean it has for the moment. (For a piece made beside it.) */
	FRotator Upright() const { return bHangs && bStoodKnown ? FRotator(0.0f, StoodYaw, 0.0f) : GetActorRotation(); }
	/**
	 * (Release 2, the far side.) The whole world's table goes round, and a piece is laid once, in one turn of it: from
	 * West, Span wide, along the table's east (the world's +Y). Given that, a march or a sail between two places takes
	 * the short way round (San Diego to Pearl Harbor is west across the chart's seam, not east across two oceans), and
	 * a piece lifted or under sail comes round the seam as it crosses, so it is seen to leave on the one side and to
	 * come in on the other. Nought, as it is made: a table with edges.
	 */
	void KeepRound(double West, double Span) { RoundWest = West; RoundSpan = FMath::Max(Span, 0.0); }
	/**
	 * (Release 2, the far side: the art director's seam-2, 25 November 2026.) Where the eye looks along the table's
	 * east (the world's +Y). A piece kept in its own turn of the world was a whole world from an eye on the other side
	 * of the chart's seam: the fleet at Pearl Harbor was not there from over California, and a fleet under sail was
	 * lost to sight at the seam. Told this (by the map, each frame), the piece is SHOWN in the turn of the world
	 * nearest the eye, wherever it stands, marches or sails, so it is in sight from both sides and crosses the seam
	 * without leaving the table. Its own reckoning does not change: its rest, its way and what it tells the map of
	 * them stay in the turn they were given in. Needs KeepRound's span; with bOn false it is kept as KeepRound says.
	 */
	void LookFrom(double EyeAlong, bool bOn);
	/**
	 * Where the piece is, in its own reckoning: where it is shown, brought back to the turn of the world its rest is
	 * counted in. (For "has it left its rest?": the shown place may be a whole world from the rest. See LookFrom.)
	 */
	FVector GetReckonedLocation() const;
	/**
	 * (Release 2; Brian 24 November 2026 18:13Z: "when i move a navy I want to see the boats sail to it as well".) A
	 * fleet sails along the points given, on the water: not lifted, its bows turned the way it goes and its passage wake
	 * thrown, and turned back to face the table (TableYaw) as it comes to rest. Longer ways take longer, from about a
	 * second and a half to three and a half.
	 */
	void SailAlong(const TArray<FVector>& Waypoints, float TableYaw);
	/**
	 * (Release 2, a fleet under way; Brian, 25 November 2026: "I want to keep upleveling the graphics and smoothness of
	 * how they move and act to make it feel as real as possible", "can we do that for the campaign map too?") A fleet
	 * sails a ship's course, laid and timed for her by the map (Campaign/ShipWay.h): out on her own turning circle,
	 * wide round the corners of her way, in on a circle to lie as the table wants her; slow off the mark, slow round a
	 * tight turn, carrying her way in. Her passage wake lies along the water her sterns have crossed. Given as her
	 * times, in the world: where she is at each tick (Each seconds apart), the way her bows point there (a unit, in the
	 * world's X and Y) and how far she has run. The last of it leaves her on the table's yaw. With fewer than two
	 * ticks, or tables not of one length, it does nothing.
	 */
	void SailCourse(const TArray<FVector>& At, const TArray<FVector2D>& Head, const TArray<double>& Ran, double Each, float TableYaw);
	/**
	 * (Release 2, a fleet under way, second part: Campaign/ShipRide.h. Brian, 25 November 2026: "I want to keep upleveling
	 * the graphics and smoothness of how they move and act to make it feel as real as possible".) The map tells each
	 * piece, as the eye moves, how far the eye is from what it looks at and whether this piece is by that. Near enough,
	 * each ship of a fleet heels outward as she turns and rides the swell; far off she lies level and costs nothing.
	 * Only a fleet whose ships have meshes of their own takes any notice (tc.Map.ShipRide, when the piece was built).
	 */
	void SetEye(double EyeDistance, bool bByFocus);
	/** The yaw every piece turns to, to face the eye: at once, or once a fleet under sail has come to rest. */
	void FaceTable(float Yaw);
	/**
	 * (Release 2, the campaign map: Campaign/FleetHeading.h. Brian, 26 November 2026: "the wake of the boats when moving
	 * don't follow the direction they came from".) A fleet at sea lies on the heading she came in on, her wake astern of
	 * her along it, and only her standard is turned to the eye. bOn: she lies so (the map says it of each fleet whenever
	 * the fleets are laid: tc.Map.FleetHeading, and not one in harbour); off, she faces the table as every piece does.
	 * TableYaw is the yaw the table is turned to. CameYaw is the heading she takes where she has none of her own yet, or
	 * (bTakeIt) where she has changed her place without sailing there. Never turns a fleet under sail: her sail does that.
	 */
	void LieHeaded(bool bOn, float TableYaw, float CameYaw, bool bTakeIt);
	bool LiesHeaded() const { return bHeaded; }
	bool IsSailing() const { return bMarching && bSailing; }
	bool IsInCamp() const { return What == 0 && bCamp; }
	int32 GetWorksState() const { return What == 0 ? WorksState : 0; }
	int32 GetSupplyState() const { return What == 0 ? SupplyState : 0; }
	/**
	 * For looking at the dressing itself: shows the army as entrenched (2) or digging (1), in camp, supplied (1) or cut
	 * off (2), whatever it is. Stage is its camp's (see GetCampStage); left out, it follows the works: digging is part
	 * of a trench, entrenched is dug in with the works finished.
	 */
	void DressAs(ACampaignMapActor* Map, const FCampaignNation& Nation, int32 Works, bool bInCamp, int32 Supply, int32 Stage = -1);

	/**
	 * (Release 2: camps you can see. Brian, 22 November 2026.) How far an army that stays put has got with its camp, by
	 * the months of fieldworks the rules count: 0 in the open (it came this month), 1 a month in camp (tents and a
	 * vehicle park), 2 digging its works (part of a trench), 3 dug in with them finished (trench, wire, gun pits, nets).
	 */
	int32 GetCampStage() const { return What == 0 && !bGlimpsed ? CampStage : 0; }
	/** The months written on the spade tag beside the plate: none (0) where the rules do not count them, or nothing shows yet. */
	int32 GetCampFigure() const { return CampFigure; }
	/** The months a dug-in army's works take to be finished, as the rules say (the view's DugFull): set before the pieces are laid out. */
	static int32 FullWorks;
	/**
	 * The camp the map has laid round the base, on the ground and in the world's own frame: it faces the enemy however
	 * the piece is turned toward the eye, and stays pitched when the piece is picked up. Made says what it was laid
	 * from, so that the map lays it again only when that changes; with no meshes it is struck.
	 */
	void LayCamp(const FString& Made, const FMiniatureMesh* Solid, const FMiniatureMesh* Trodden, UMaterialInterface* SolidPaint, UMaterialInterface* TroddenInk);
	const FString& GetCampMade() const { return CampMade; }
	/** Shows or hides the camp (the map does it as the eye rises and comes down). New camps take it as they are laid. */
	void SetCampShown(bool bShown);
	static bool bCampsNear;
	/** Where its camp fire burns, in the world, for the smoke that rises from it. */
	FVector GetCampFire() const;
	/** Shows or hides the flock on every base (the map does it as the eye rises and comes down). New pieces take it as they are made. */
	void SetFlockShown(bool bShown);
	static bool bFlockNear;
	/** (Release 2, nicknames.ts) Whether the names corps earn are lettered over their army's flag: the map sets it from the picture before it lays the armies. */
	static bool bNamesShown;
	/** (Release 2, new units ready; Brian 24 Nov 18:30Z) Whether his corps handed over this month wear a brass NEW tag on their pole (the option "New units ready"). */
	static bool bReadyTags;
	/** A fleet lying in harbour is drawn small, at anchor by its port, with no standard over it: a small badge on the port says how many ships are in (the map is the clearer for it). */
	void SetAtAnchor(bool bInHarbour);
	bool IsAtAnchor() const { return What == 1 && bAtAnchor; }
	int32 GetFleetShips() const { return What == 1 ? Fleet.Ships() : 0; }
	int32 GetFleetBase() const { return What == 1 ? Fleet.Base : INDEX_NONE; }
	/**
	 * (Release 2, funnel smoke: Campaign/ShipSmoke.h; the map draws it, tc.Map.ShipSmoke.) A funnel of one of her ships
	 * as it stands now, in the world.
	 */
	struct FFunnelNow
	{
		FVector Top = FVector::ZeroVector; // the middle of its top
		int32 Ship = 0; // which of her ships it is on, counted from nought
		int32 Class = 2; // 0 battleship, 1 carrier, 2 cruiser, 3 destroyer
		double Long = 0.0; // that ship's length, in the world
	};
	/**
	 * Her funnels, and how she steams: her speed as a share of her full speed, and how fast it grows in shares a second
	 * (both nought lying still, or let down from the hand). False, and nothing, for a piece that makes no smoke: one
	 * that is no fleet, a fleet known only from a report or a glimpse, one hidden, carried in the hand or over the
	 * table, or at anchor by her port. A submarine has no funnel.
	 */
	bool FunnelsNow(TArray<FFunnelNow>& OutFunnels, double& OutPace, double& OutGather) const;
	/**
	 * How she steamed Ago seconds ago, as FunnelsNow tells it for now (nought before her course began). Returned: how
	 * much of that a puff made then still keeps, 1 out at sea and falling to 0 as her course ends; 0 for a piece not
	 * under sail. See TheaterShipSmoke::Carries.
	 */
	double SteamedAt(double Ago, double& OutPace, double& OutGather) const;
	/**
	 * What must be added to a point of her, to find where it was when she was Back further astern (in the world's
	 * measure) along the water she has crossed: round her turns on a course of her own, straight astern otherwise.
	 */
	FVector AsternBy(const FVector& Point, double Back) const;
	const FString& GetNationTag() const { return NationTag; }
	/**
	 * film critic (development only, -FilmCritic; FilmCritic.cpp's fleet facts): a fleet as it stands on the table now.
	 * Its name; its wake shown (0 none, 1 patrol, 2 passage); while it sails, how far (0 to 1, else -1) and the way of
	 * its route's leg now (else zero); the way its bows point (world); its bows' and its wake's far end's points
	 * (world), bOutTail false with no wake shown. False when it is not a fleet. Reads only: nothing is changed.
	 */
	bool FilmFleet(FString& OutName, int32& OutWake, double& OutProgress, FVector& OutCourse, FVector& OutBow, FVector& OutBowAt, FVector& OutTailAt, bool& bOutTail) const;
	/** Its colours, as a ball in the world: what a town's name plate must not be drawn across. False while the piece is hidden. */
	bool GetColours(FVector& OutMiddle, double& OutRadius) const;

	/** A picked-up piece shows a bright ring under its base and is held a little off the table. */
	void SetSelected(bool bInSelected);

	/** A piece the pointer rests on shows the ring too, to say it can be picked up. */
	void SetHovered(bool bInHovered);

	/** True if a line from Origin along Direction passes through the piece: its counters, its pole or its colours. */
	bool HitByRay(const FVector& Origin, const FVector& Direction, double& OutDistance) const;

	/** True if that line passes through the piece's colours (its flag) alone. */
	bool ColoursHitByRay(const FVector& Origin, const FVector& Direction) const;

	/** Carries the piece in the hand: it hangs over this point of the table. */
	void Carry(const FVector& GroundPoint);

	/** Puts a carried piece back where it stands. */
	void ReturnToRest();

	int32 GetRegionId() const { return RegionId; }
	/** Half the width of its base, in its own units (the table's, before the piece's own size). */
	double GetBaseRadius() const { return BaseRadius; }
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
	// (Release 2: a fleet that lies on her own heading, see LieHeaded.) While she does, her standard is turned back about
	// the piece's upright to the eye's yaw, foot and all. bStandardTurned: its foot has been moved from where it was built.
	bool bHeaded = false, bStandardTurned = false;
	float EyeYaw = 0.0f; // the yaw the table is turned to, as the map last said
	float StandardTurnNow() const;
	void TurnStandard();

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
	bool bGlimpsed = false; // (ruling 154) only glimpsed: a flag and no more
	bool bOnSnow = false; // snow lies where it stands
	bool bHasWake = false; // a fleet at sea: its body carries a wake for patrol (section 1) and one for passage (section 2)
	// What the army is about, shown round its base so it can be read without a click: 0 nothing, 1 digging in,
	// 2 entrenched; in camp, resting; 0 not known, 1 supplied, 2 cut off.
	int32 WorksState = 0;
	/** How worn its flag is: 0 whole, 1 torn and faded (a battered army), 2 in tatters (one nearly spent). */
	int32 FlagWear = 0;
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
	int32 CampStage = 0; // (release 2) see GetCampStage
	int32 CampFigure = 0;
	FString CampMade; // what the camp on the ground was laid from
	/** The camp round the base (tents, vehicles, works, the trodden ground), laid by the map: see LayCamp. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Camp;
	/** The months on the spade tag beside the strength plate. */
	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> WorksLabel;
	/** (Release 2, nicknames.ts) The name the army's corps have earned, lettered on a dark scroll over its flag. */
	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> NameLabel;
	FString EarnedName; // the name lettered there; empty for none
	/** (Release 2, the marked army; marked.ts) An enemy army that took one of his towns: the town, lettered on a red tag on its pole. */
	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> MarkLabel;
	FString MarkedTown; // empty for none
	bool bTagNew = false; // (release 2, new units ready) one of his corps handed over this month: a brass NEW tag in the same place
	bool bRemoveOnArrival = false;
	bool bLiftOnMarch = true;
	bool bSailing = false; // (release 2) a fleet under sail: see SailAlong
	double RoundWest = 0.0, RoundSpan = 0.0; // (release 2, the far side) the turn of the world it is kept in: see KeepRound
	double RoundEye = 0.0; // (and where the eye looks along the world's +Y, while it is shown in the turn nearest the eye: see LookFrom)
	bool bRoundToEye = false;
	/** A place of the piece's own reckoning as it is shown: in the turn of the world nearest the eye, where the map has said so. */
	FVector Shown(const FVector& Place) const;
	float SailEndYaw = 0.0f; // the table's yaw it turns back to on arrival
	// (Release 2, a fleet under way: see SailCourse.) Her times: where she is at each tick, the way her bows point
	// there, and how far she has run, in the world. Empty when she is not on a course of her own.
	TArray<FVector> SailAt;
	TArray<FVector2D> SailHead;
	TArray<double> SailRan;
	double SailEach = 0.02;
	float SailLaidYaw = 0.0f; // the table's yaw her course was laid to end on
	TArray<FVector> WakeLaid; // her passage wake as it was built, straight astern
	TArray<FVector> WakeBent; // and as it lies now, along the water her sterns have crossed
	double WakeReach = 0.0; // how far astern of her middle the wake reaches, in her own units
	double WakeStiff = 0.0; // and how far astern of her middle she is still herself (her ships, to their sterns)
	bool bWakeBent = false;
	void BendWake(double Ran, const FVector2D& Bows, double Progress);
	void StraightenWake();
	/**
	 * (Release 2, a fleet under way, second part.) Each ship of a fleet in a small mesh of her own, about her own middle
	 * on the water, so that she can heel in her turn and ride the swell while her wake stays flat in the body. Empty
	 * unless tc.Map.ShipRide was on when the piece was built: then her ships are in the body's one mesh, as before.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Hulls;
	/** How each of those ships lies and rides (one for each hull, in the same order). */
	struct FHullRide
	{
		FVector Middle = FVector::ZeroVector; // her middle on the water, in the piece's own space
		FVector2D Ahead = FVector2D(0.0, 1.0); // the way her bows point there (a unit)
		double Long = 0.0, Beam = 0.0; // her length and beam, in the piece's own units
		int32 Class = 2; // as Campaign/ShipRide.h counts them: 0 battleship, 1 carrier, 2 cruiser, 3 destroyer, 4 submarine
		double HeelAngle = 0.0, HeelRate = 0.0; // her heel as it stands (radians, to port positive) and how fast it changes
		double Phase = 0.0; // tells her from her sisters in the swell
	};
	TArray<FHullRide> HullRides;
	double RideMuch = 0.0; // how much of the swell she shows, 0 to 1: it comes on as the eye comes near and dies as it leaves
	bool bRideNear = false; // the eye is near enough to see her ride (see SetEye)
	bool bHullsLevel = true; // every hull lies level, as it was built
	bool bInHand = false; // carried in the hand: off the water
	double SailFull = 0.0; // the furthest she goes in one tick of her course: her full speed
	/**
	 * A frame of her ships' riding. Turn is how fast her head swings (radians a second, to starboard positive) and Pace
	 * her speed as a share of her full speed: what they heel by. False when there is nothing left to see: then every
	 * hull lies level, and she need not be ticked for it.
	 */
	bool RideHulls(double Seconds, double Turn, double Pace);
	void LevelHulls();
	/** (Release 2, funnel smoke.) A funnel of one of her ships as her model was built, in the piece's own space: see FunnelsNow. */
	struct FFunnel
	{
		FVector Top = FVector::ZeroVector;
		int32 Ship = 0;
		int32 Class = 2;
		double Long = 0.0;
	};
	TArray<FFunnel> Funnels;
	/**
	 * (Release 2, armies with weight: Campaign/ArmyCarry.h.) A piece is held by its top and is heavy at its foot: as the
	 * hand carries it its foot hangs back and swings on, and as it is set down it rocks once and is still. Never a
	 * fleet, which has a ship's way of her own.
	 */
	bool bWeighty = false; // the march it is on is made with weight
	bool bHangs = false; // it leans, or may be about to: it is ticked until it is still
	double HangKept[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // its hang, as TheaterArmyCarry::FHang keeps it and in that order
	FVector2D CameAlong = FVector2D::ZeroVector; // the way it was last carried, flat on the table: the way it tips as it lands
	FVector HandWas = FVector::ZeroVector, HandNow = FVector::ZeroVector; // where the hand held it a frame ago and holds it now (the point on the table under it)
	float StoodYaw = 0.0f; // the way it faces on the table (the map turns every piece to the eye): kept while it leans
	bool bStoodKnown = false;
	/** The way it faces on the table: read from the piece while it stands upright, kept while it leans. */
	float FacingNow();
	/** Stands it at a place, leaning as its hang says. On the table it is lifted by what keeps the low edge of its base on it. */
	void StandLeaning(const FVector& Place, bool bOnTable);
	/** Upright again, with nothing left of its hang. (Its place is not touched.) */
	void StandStraight();
	/** A frame of its hang while it is not on the march: swaying in the hand, or rocking on the table after a set-down. False when it is still. */
	bool Sway(double Seconds);
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
