#pragma once

#include "CoreMinimal.h"
#include "Battle/ConvoyLooksCore.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <string>

class AAirStage;
class FBattleEffects;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneComponent;
namespace SeaShow
{
	struct ShipOut;
	struct Fx;
	class Show;
}

/**
 * The month's submarine scene, its looks in the sea battle's scene (ruling 241, Part D1, look only; the sea scene
 * thread's d1-hooks.md, section 3): what ASeaBattle's marked hooks hand it ("// D1 hook (convoy looks)"), drawn.
 *
 *  - A boat (a ship of class ss): her own heave for the stage, so that surfaced she rides the swell with her casing
 *    awash (the scene draws her bow wave), goes down by the bow with the water running off her casing and a swirl
 *    where her bridge went under, runs at periscope depth with only her raised periscope and its feather of spray
 *    showing, and deep is nothing but, where light falls on the sea over her, a faint swirl now and then; never a pop
 *    (her depth is the show's, eased over the 25 to 30 s of a dive). Her periscope's stick is drawn here (the stage's
 *    model has its periscopes housed); the rest is her model on the stage.
 *  - Depth charges: a pattern rolled off the stern's rails and thrown out on its throwers' arcs, one after another as
 *    the show lays it, each into the sea where the show will burst it; or the ahead thrower's ring of small splashes
 *    about 200 m ahead (ChargeDrop). Each burst as it comes: its white dome, then its column when shallow, or its boil
 *    when deep, by how deep it was set and its charge (ChargeBurst, on the effects kit's water spouts: the dome and the
 *    boil its quiet churn, the column its nearest size row, a low dome its low spurts).
 *  - A starshell: the flare under its small parachute drifting down the wind and sinking until the show's burn-out, its
 *    white-yellow lamp flickering and falling off as it burns down and sinks, its thin smoke trail (the kit's).
 *  - A boat's end: crushed (her air coming up as a hump of white water and boils, no boat) or scuttled (brought up to
 *    the surface, wherever the show still has her). The sinking itself, the men in the water, the oil and the wreckage
 *    are the sea scene's.
 *  - A rescue: a scrambling net down the rescuer's side toward the survivors while it lasts (the survivors and their
 *    rafts are the sea scene's).
 *
 * Everything that counts is the show's (the sea scene thread's SeaShow): this only looks. The timing and the shapes
 * are plain C++ (Battle/ConvoyLooksCore, checked by Tools/convoy_looks/run.sh); here only the kit's calls (always on
 * the sea, never on a deck: the scene's bOnWater held while they are made), one small mesh made only once there is
 * something to draw (its solid things on the map's surface paint, the flares' glow and the swirls on M_Glow) and up to
 * three lamps. Its mesh and lamps hang on the scene's root (so the scene keeps them alive, and they go with it); nothing
 * is done in its destructor.
 */
class FConvoyLooks
{
public:
	FConvoyLooks() = default;
	FConvoyLooks(const FConvoyLooks&) = delete;
	FConvoyLooks& operator=(const FConvoyLooks&) = delete;

	/**
	 * LayAndSteer's hook, for every ship of every step: the looks made on first use and bound to the scene (its root,
	 * stage, effects kit, show, scale, the battle's clock, whether a picture run is skipping ahead, and its bOnWater,
	 * held while the kit is called); on the first ship of a step, the step itself (the boats stepped from the show's
	 * ships, what is due handed to the kit, the charges flown, the flares lit, the mesh laid). Returns the heave she is
	 * steered with: a boat's own; any other ship's as the scene had it (her Heave less Dived). A clock gone back (another
	 * battle) starts it afresh.
	 */
	static double Steer(TUniquePtr<FConvoyLooks>& Held, USceneComponent* InRoot, AAirStage* InStage, FBattleEffects* InKit, const SeaShow::Show& InShow, double InUnitsPerMetre,
	                    double InNow, bool bInSkip, bool& bOnWater, const SeaShow::ShipOut& Ship);

	/** PlayEffects' hooks (its marked case labels): ChargeDrop, ChargeBurst, Starshell, Rescue and Sink are taken, the
	 * rest left. bInSkip: a picture run skipping ahead (only what lasts is kept: a boat scuttled is still brought up). */
	void Effect(const SeaShow::Fx& E, double InNow, bool bInSkip);

private:
	/** A boat as kept from step to step. */
	struct FBoatKept
	{
		ConvoyLook::BoatState State;
		ConvoyLook::BoatModel Model;
		ConvoyLook::Drawn Drawn;
		ConvoyLook::BoatOut Out;
		ConvoyLook::P3 At;
		double SeenAt = -1.0;
	};
	/** Something for the kit, due at a moment (battle time): a spout of a size row on the water, a low spurt (the kit's
	 * Skip, along Way) or a thrower's discharge (the kit's Muzzle, along Way). */
	enum class EDue : uint8
	{
		Spout,
		Spurt,
		Discharge,
	};
	struct FDue
	{
		double At = 0.0;
		EDue Kind = EDue::Spout;
		ConvoyLook::P3 Where, Way;
		FString Row;
	};
	/** A charge of a pattern or a bomb of the ahead thrower: when the pattern went in, and its shot. */
	struct FCharge
	{
		double T0 = 0.0;
		ConvoyLook::DropShot Shot;
		bool bAhead = false;
	};
	struct FStar
	{
		int32 Key = 0;
		ConvoyLook::StarIn In;
		bool bTrail = false;
	};
	/** A rescuer's scrambling net: her key, where the survivors are, until when. */
	struct FNet
	{
		int32 Key = 0;
		ConvoyLook::P3 Aim;
		double Until = 0.0;
	};

	TWeakObjectPtr<USceneComponent> Root;
	TWeakObjectPtr<UProceduralMeshComponent> Mesh;
	TArray<TWeakObjectPtr<UPointLightComponent>> Lamps;
	/** Bound each step (Steer): good only for that step. */
	AAirStage* Stage = nullptr;
	FBattleEffects* Kit = nullptr;
	const SeaShow::Show* Show = nullptr;
	bool* OnWater = nullptr;
	double UnitsPerMetre = 100.0;
	double Now = 0.0, SteppedAt = -1.0;
	bool bSkipping = false;
	bool bBegun = false;
	int32 SectionsLaid = 0;

	TMap<int32, FBoatKept> Boats;
	TArray<FDue> Due;
	TArray<FCharge> Charges;
	TArray<FStar> Stars;
	TArray<FNet> Nets;
	TSet<int32> Ended;

	void Begin();
	void Clear();
	void Step();
	void StepBoats();
	void PlayDue();
	void FlyCharges();
	void LightStars();
	void Lay();
	void Drop(const SeaShow::Fx& E);
	void Burst(const SeaShow::Fx& E);
	void End(const SeaShow::Fx& E, bool bInSkip);
	/** The light on the sea at a board point (0 to 1): the sky's and the burning flares'. */
	double LitAt(const ConvoyLook::P3& At) const;
	FVector World(const ConvoyLook::P3& P) const;
};
