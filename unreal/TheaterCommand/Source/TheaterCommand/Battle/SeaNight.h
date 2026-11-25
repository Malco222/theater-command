#pragma once

#include "CoreMinimal.h"
#include "Battle/SeaNightCore.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneComponent;
class USpotLightComponent;
namespace SeaShow
{
	class Show;
}

/**
 * NIGHT FIGHTING AT SEA, its looks in the sea battle's scene (the look bible's S7: "Star shells hang and light the water;
 * gun flashes light the hulls; dark water still shows its waves"), the engine's side; the reckoning is
 * Battle/SeaNightCore's, which is engine-free and held to its brief by Tools/sea_waves/run_night.sh.
 *
 * Behind the player's switch, off by default: "-TheaterSeaNightFight=1" (Battle/SeaLookSwitches, step 11). Off, nothing
 * at all is made, nothing is read from the show and not a line is said: the scene draws exactly what it draws today.
 *
 * What it draws, when the core says a ship has unmasked her light or fired a spread of star shells:
 *  - A searchlight's beam: one shadowless spot light down its axis, so the hull it is on and the water about her are lit
 *    in the carbon arc's blue-white, and an added-light shaft on the existing M_Beam (the campaign map's searchlight
 *    paint: additive, unlit, two-sided, drawn after the lens's blur), narrow at the lamp (its real mirror) and widening
 *    on its pencil's spread, strongest at the lamp and fading out with no hard end, as much of it showing as the night's
 *    haze and smoke carry (the core's Shaft). Its lamp shows as a white heart in a halo; where its axis ends on the sea
 *    (a sweep, or a target lost) a faint pool is laid on the water.
 *  - A fleet star shell: one shadowless point light at the flare and a glow at it, the same lamp the convoy's own
 *    starshells use (ConvoyLooks: 400,000 cd reaching 3 km, white with a little yellow), flown by ConvoyLook::StarAt.
 *    In a convoy night the core sends none of its own, so the rules' starshells are never doubled.
 *
 * THE FRAME'S BUDGET (45 frames a second on the MacBook Air, which is GPU-bound): at most 7 lamps, every one shadowless
 * and falling off as the square — MostBeamLamps (4) spot lights for the beams and MostStarLamps (3) point lights for the
 * newest burning star shells; and one procedural mesh of two sections (the shafts on M_Beam, the hearts, pools and
 * flares on M_Glow) of at most about 700 triangles in all: a beam is 12 rings of a 7-wide strip (72 quads, 144
 * triangles), its heart and pool 4 quads, a flare 1. Nothing casts a shadow; nothing collides. The lamps and the mesh
 * are made only once there is something to draw, and hang on the scene's root, so the scene keeps them and they go with
 * it.
 *
 * Everything in it is drawn at the night's own strength (SeaNight::Night::Glow(), 1 at full night and less under a
 * moon), so that nothing glows by day: by day, and in twilight, the core lights nothing at all and this draws nothing.
 * Step 11's own night light, when it lands, hands its share in through SetShare; until then the share is read from the
 * recording's light (ConvoyLook::SkyLit), which no fleet action as recorded gives, so the three recorded fleet actions
 * draw nothing new even with the switch on.
 *
 * LOOKS ONLY: nothing here is read by anything the show decides, and nothing here touches the show.
 */
class FSeaNight
{
public:
	/** The scene's room: the actor the lamps and the mesh are made on, the root they hang from, and its scale. */
	struct FRoom
	{
		AActor* Owner = nullptr;
		USceneComponent* Root = nullptr;
		double UnitsPerMetre = 100.0;
	};

	FSeaNight() = default;
	FSeaNight(const FSeaNight&) = delete;
	FSeaNight& operator=(const FSeaNight&) = delete;

	/** Once, as the battle opens (ASeaBattle::OpenOn): the room, the recording's light (Show's board) and the battle's
	 * seed. bInOn: the player's switch, -TheaterSeaNightFight=1. Off, nothing is made or said. */
	void Begin(const FRoom& InRoom, const SeaShow::Show& Show, bool bInOn, uint64 Seed);
	/** The switch. */
	bool IsOn() const { return bOn; }
	/** Night fighting's night now: the recording's light with the sun well under, or the share handed in. */
	bool IsNight() const { return bOn && Reckoned.IsOn(); }
	/** Step 11's night share (0 night to 1 day) once it lands, in place of the recording's light. */
	void SetShare(double Share);
	/** How strong the night's own added light is drawn: 1 at full night, less under a moon, 0 by day. */
	double Glow() const { return bOn ? Reckoned.Glow() : 0.0; }
	/** Every frame, after the scene's own show steps are drawn (ASeaBattle::Tick, after DrawWrecks): the show's frame
	 * read, stepped, and what it says drawn. bSkip: a picture run skipping ahead (every beam out, nothing carried over). */
	void Tick(const SeaShow::Show& Show, double Now, bool bSkip, const FVector& EyeAt);
	/** The battle's end (ASeaBattle::EndPlay): every lamp out and the mesh cleared, before the room is given back. */
	void End();
	/** The night's own counts, for the frame-time line and the bench. */
	const SeaNight::Tally& Counted() const { return Reckoned.Counted(); }

private:
	/** The lamps: a spot light for each beam, a point light for each of the newest burning star shells. */
	static constexpr int32 MostBeamLamps = SeaNight::MostBeams, MostStarLamps = 3;
	/** The mesh's sections: the beams' shafts (added light), and the lamps' hearts, the beams' pools on the water and the
	 * star shells' flares (glow). */
	static constexpr int32 ShaftSection = 0, GlowSection = 1, Sections = 2;

	/** One section of the mesh, laid afresh each frame into the same arrays, their room kept, so that nothing is made
	 * afresh once they have grown (as the convoy looks' and the underwater view's are). */
	struct FLaid
	{
		TArray<FVector> Points;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FColor> Colours;

		void Reset();
		bool IsEmpty() const { return Triangles.Num() == 0; }
		int32 Add(const FVector& At, const FColor& Paint);
		/** A flat piece of added light facing the eye (or the sky): its middle, half across, half along. */
		void Flat(const FVector& Mid, const FVector& AcrossBy, const FVector& AlongBy, const FColor& Paint);
	};

	void LayBeams(const FVector& EyeAt);
	void LightStars(double Now);
	void Dark();
	UProceduralMeshComponent* Mesh();
	USpotLightComponent* BeamLamp(int32 Which);
	UPointLightComponent* StarLamp(int32 Which);
	FVector World(const SeaNight::P3& P) const { return FVector(P.Y * Room.UnitsPerMetre, P.X * Room.UnitsPerMetre, P.Z * Room.UnitsPerMetre); }

	FRoom Room;
	SeaNight::Night Reckoned;
	SeaNight::Frame Read;
	bool bOn = false, bBegun = false, bDark = true;
	/** [EST] The share of a real searchlight's candelas the spot light is given, and of the shaft's strength: the real
	 * lamps are hundreds of millions of candelas, which no scene is exposed for. "-TheaterSeaNightBeam=lamp,shaft" tries
	 * others (the Mac check rules on both). */
	double LampShare = 0.25, ShaftShare = 1.0;
	TWeakObjectPtr<UProceduralMeshComponent> Laid;
	TArray<TWeakObjectPtr<USpotLightComponent>> BeamLamps;
	TArray<TWeakObjectPtr<UPointLightComponent>> StarLamps;
	FLaid Parts[Sections];
	/** [CL] Each section's shape as it was last sent (ConvoyLook::MeshShape, read-only): while the points and triangles
	 * hold, a beam that only swung is sent again in place. */
	ConvoyLook::MeshShape Shapes[Sections];
	int32 SectionsLaid = 0;
};
