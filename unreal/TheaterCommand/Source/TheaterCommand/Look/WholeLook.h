#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class USkyLightComponent;

/**
 * The whole picture's look (Brian, 25 November 2026 03:58Z: "can we work on better overall graphics in the background?").
 *
 * A few steps that no one scene owns, laid over every scene alike: the map, the ground, the air and the sea battles. Each
 * is a console variable, off unless something turns it on: the release 2 ship list (a row of the Variable kind, with its
 * own line on OPTIONS), a launch word, or the photographer's timing rounds ("-TheaterTime=off:tc.Look.Crisp=0|on:
 * tc.Look.Crisp=1"), which flip it inside one running game so that the two pictures are taken in the same warmth. The
 * art director's first read of the three (25 Nov 04:46Z, art-director/reviews/whole-look-first-read.md) set the numbers
 * they start at.
 *
 * - tc.Look.Crisp (-TheaterLookCrisp): the picture sharpened after the upscaler, and the ground's pictures read with
 *   sixteen-fold filtering where they lie at a slant. The map and the battles are drawn at about 1080 by 670 and stretched
 *   to the Mac's 2881 by 1786, which softens every edge and every letter painted on the ground. The sharpening is the
 *   tonemapper's own (the whole look's lens, below, carries it; the menus are drawn after it, and the plates the game
 *   paints with its own cameras, the map's portraits and showpiece, are told no sharpening); the filtering is
 *   r.MaxAnisotropy, laid once the game's world is up (after the engine has laid the player's own quality settings, so
 *   what is put back is the player's), which reaches the ground battle's field pictures, the air battle's ground and
 *   every picture made as seen at a slant (the engine makes the samplers of those already made again as it changes). The campaign map's land pictures are made
 *   plain (trilinear) today, so on the map only the sharpening shows. tc.Look.CrispSharpen (0.4; the art director's ceiling 0.6 until a pan clip
 *   shows no rim) and tc.Look.CrispFiltering (16) tune it.
 * - tc.Look.Depth (-TheaterLookDepth): the soft shade where things meet and stand close (ambient occlusion) reaching
 *   as far as the battle is seen. The engine fades it out from 30 to 80 m from the eye, and nothing in the game said
 *   otherwise, so the general's views from 100 to 300 m up had none: a hedge, a wood's edge, a tank and a house sat on the
 *   field as if pasted there. Its reach is laid at the scale of a thing on the ground (2 m in the world, not a share of
 *   the view, which grew to a valley's shade far off). It is laid only while a ground battle is, the one scene that draws
 *   ambient occlusion (the map's speed trims keep it off). tc.Look.DepthStrength (0.65), tc.Look.DepthReach (metres,
 *   1500), tc.Look.DepthRadius (metres, 2), tc.Look.DepthQuality (70) and tc.Look.DepthClosed (0.4, under a closed
 *   canopy) tune it.
 * - tc.Look.Bounce (-TheaterLookBounce): every ground gives back its own colour to what faces it, as snow already did. The
 *   battle has no bounced light of its own (Lumen is off for speed), so a hull's shaded side, a hedge's underside and a
 *   man's legs took the table cubemap's dark ground on every field but snow: the desert's sand gave no warm fill and
 *   the steppe's grass no green. Read once as the field is laid (the mean of its paint on a grid), its hue pulled inside
 *   the art director's box with its value kept (red/green 0.90 to 1.15, blue/green 0.80 to 1.05: Normandy's green and
 *   the Mediterranean's red would cast faces and hulls), following the sun's true height (none with it under the horizon,
 *   whole from 15 degrees up; the picture's own sun is held 8 degrees up), at half under a closed canopy, none from water
 *   (rivers, ponds and flooded ground mirror the sky), and laid under the battle's sky light by LandBattle's
 *   HideTheTable; through a night before the attack the sky's lower half stays as shipped and the bounce is laid as the
 *   picture comes up from black on the battle (Dawn). Snow keeps its own, as shipped. tc.Look.BounceStrength (1.0), tc.Look.BounceClosed (0.5) and
 *   tc.Look.BounceHue (1; 0 leaves the hue as the field's, -TheaterLookBounceUnclamped) tune it.
 *
 * - tc.Look.Worn (-TheaterLookWorn): the ground battle's tanks, guns and lorries carry weathered paint, broken in patches
 *   and flat, not one even coat (the art director, 25 Nov 05:00Z, art-director/reviews/battle-sky-and-worn-numbers.md
 *   section 2: a large flat panel of the desert tank's hull varied by 2.3 of 255 over its whole area; her line is 10,
 *   and the ground thread's Type 97, whose own file breaks her paint, carries 12 to 19). Their ink is M_LandWorn
 *   (build_content.py): the battle's surface's own mottle (the map's pieces' paint job), read in each body's own space,
 *   in two layers that repeat at periods no multiple of each other (2.5 and 6.1 m, so no patch comes round on one hull),
 *   each vehicle with her own shift of both (no twins in a troop), and the paint's roughness taken from 0.6, a sheen, to
 *   army paint's 0.85, moving with the patches (the dark ones about 0.72, the pale 0.98). The art director's read of
 *   05:45Z set all of it. Past the hand-over (140 m) the far pieces of one model share one shift. Nothing touches an
 *   edge: wear on an edge is the vehicles' own files' and goes darker, never paler. The aircraft over the field, the old
 *   wrecks and the map's pieces keep their own paint: the steel ink itself is never changed. Read as a battle opens
 *   (WornOpen); without M_LandWorn the even coat, said once. With it on the men's inks also read each paint's roughness
 *   from their points' alpha (M_LandManWorn, M_LandCrowdWorn), which the men's looks write with it on: the soldiers'
 *   half, with their boots and knees taking the field's colour. tc.Look.WornStrength (1), tc.Look.WornRepeat (metres,
 *   2.5), tc.Look.WornRepeat2 (metres, 6.1), tc.Look.WornMix (the second layer's share, 0.5), tc.Look.WornRoughness
 *   (0.85) and tc.Look.WornSheen (how far the roughness moves with the patches, 1.3) tune it.

 * The whole look's lens: one post-process volume over the whole play world (the map and every battle are laid in the one
 * world), above the ground battle's grade volume, carrying only the sharpening and the ambient occlusion fields. No other
 * volume or camera in the game sets those fields, so it fights nothing.
 *
 * The tunings can be set from the launch line too, for the Mac's sweeps: -TheaterLookCrispSharpen=,
 * -TheaterLookCrispFiltering=, -TheaterLookDepthStrength=, -TheaterLookDepthReach= (metres), -TheaterLookDepthRadius=
 * (metres), -TheaterLookDepthQuality=, -TheaterLookDepthClosed=, -TheaterLookBounceStrength=, -TheaterLookBounceClosed=, -TheaterLookWornStrength=, -TheaterLookWornRepeat= (metres),
 * -TheaterLookWornRepeat2= (metres), -TheaterLookWornMix=, -TheaterLookWornRoughness=, -TheaterLookWornSheen=.
 *
 * Their cost in a ground battle, "-TheaterLookTime=off:tc.Look.Crisp=0,tc.Look.Depth=0|crisp:tc.Look.Crisp=1|depth:
 * tc.Look.Depth=1" (the look reel, 25 Nov: three whole runs of Normandy, timed one after another, could not tell a 3 fps
 * cost from the battle's own swings and the Mac's warming). The settings take turns every 2.5 s (0.5 settled, 2 timed;
 * -TheaterLookTimeSettle=, -TheaterLookTimeMeasure=) through the whole battle, the first, the baseline, between every
 * two others, and each other window is read against the mean of the baseline's on either side of it, as the field goes
 * and every 24 windows ("Whole look timing:"): its graphics chip time's difference, the middle and the middle half of
 * them, and their mean give or take its standard error. Frames over a tenth of a second (a still, a bake) are left out.
 */
namespace WholeLook
{
	/** At the game module's start, after the release 2 ship list has set its variables: reads the launch words, lays what
	 * is on, and starts the small clock that keeps the looks as their variables say (four times a second). */
	void Start();

	/** At the module's end: the clock stopped (the engine's settings go with the engine). */
	void Stop();

	/** tc.Look.Bounce is on: the ground battle lays its field's own colour under the sky light on every ground. */
	bool BounceOn();

	/** The mean of a field's paint (sRGB bytes, read by ColourAt) on a 24 by 24 grid over Wide by Deep metres from its
	 * south-west corner, as linear light; a point ColourAt gives alpha 0 (water) is not counted. Once a battle, as the
	 * field is laid. */
	FLinearColor GroundMean(TFunctionRef<FColor(double X, double Y)> ColourAt, double Wide, double Deep);

	/** The sine of the sun's true height for a latitude, a month (its middle) and a local solar hour, as LandBattle's
	 * SunWay reckons it but without its 8-degree floor: under 0 the sun is under the horizon. */
	double SunHeight(double Lat, int32 Month, double Hour);

	/** The light a field gives back from below, from its mean paint (GroundMean, linear): the hue pulled inside the art
	 * director's box with the value kept (unless tc.Look.BounceHue is 0), times tc.Look.BounceStrength, times
	 * tc.Look.BounceClosed under a closed canopy, times the sun's share (SunHeight the sine of the sun's height: none
	 * under the horizon, whole from 15 degrees up). Alpha 1. Read once, as the field is laid: the battle's sun stands still
	 * for the whole fight (set once at the open). If the sun is ever made to climb during a battle, the bounce must be read
	 * again with it, or a dawn attack's fill goes stale. */
	FLinearColor BounceLight(const FLinearColor& Mean, bool bClosedCanopy, double SunHeight);

	/** The ground battle says what it is fought under as its field is laid (a closed canopy: the jungle; the art director:
	 * open ground is where more than about half the opening view lies in direct sun), and that it is over. The far shade
	 * is laid at tc.Look.DepthClosed under a closed canopy. */
	void FieldLaid(bool bClosedCanopy);
	void FieldGone();

	/** tc.Look.Worn is on (its variable as it stands). */
	bool WornOn();

	/** The field's own ground as dirt (release 2, tc.Look.Worn): its mean paint (GroundMean, linear) with the hue pulled
	 * inside an earth box (red over green 1.00 to 1.25, blue over green 0.70 to 0.90) and the value kept, so Normandy's
	 * grass does not paint a hull's lower third or a man's boots green: one colour for both. Alpha 1. */
	FLinearColor FieldDust(const FLinearColor& Mean);

	/** A ground battle loads its inks (Steel its vehicles' plain ink, all its settings laid): whether this battle's
	 * vehicles wear their paint is settled here, once (the switch on and M_LandWorn built), and the last battle's worn
	 * inks forgotten. Steel itself is never changed. */
	void WornOpen(UMaterialInstanceDynamic* Steel);

	/** The ink a ground battle's vehicle's near bodies are drawn with: Steel itself unless WornOpen settled on the worn
	 * paint; then her own ink on M_LandWorn, Steel's settings copied and the patches shifted by a seed of her number
	 * (Vehicle), made once a vehicle for the battle (Outer, the scene, owns it). */
	UMaterialInterface* VehicleInk(UMaterialInstanceDynamic* Steel, int32 Vehicle, UObject* Outer);

	/** The parent the ground's far vehicles' pieces are inked on while this battle wears its paint (M_LandWorn, which has
	 * M_LandSteel's soot), nullptr otherwise: the paint does not change at the hand-over. */
	UMaterialInterface* FarParent();

	/** A far kind's ink, made on FarParent with the steel's settings copied: the worn paint's settings, the shift seeded
	 * by her model's name (Model). Nothing for an ink on another parent. */
	void WearFar(UMaterialInstanceDynamic* FarInk, const FString& Model);

	/** With tc.Look.Worn on, the men's worn parents, which read each paint's roughness from the points' alpha (no metal, no
	 * wake read from it): M_LandManWorn for the near men (bCrowd false), M_LandCrowdWorn for the crowd's instances; nullptr
	 * with it off or unbuilt (said once). */
	UMaterialInterface* MenParent(bool bCrowd);

	/** Both men's worn parents are there with the switch on: the men's looks may write each paint's roughness into their
	 * points' alpha. Never otherwise: the men's plain ink reads any alpha short of whole as part painted metal, and chips it. A
	 * look kept from battle to battle must be made again when this changes. */
	bool MenRough();

	/** The men's ink (Made, on the campaign's surface, its settings laid): with MenRough, the same settings on
	 * M_LandManWorn (Outer owns it); Made itself otherwise. The crowd's ink copies its settings from it. */
	UMaterialInstanceDynamic* MenInk(UMaterialInstanceDynamic* Made, UObject* Outer);

	/** A night before the attack plays first: the battle's sky lights keep the lower half as shipped, and Bounce is laid
	 * under them at Dawn. FieldGone forgets it. */
	void BounceAtDawn(const TArray<USkyLightComponent*>& Skies, const FLinearColor& Bounce);

	/** The battle's own picture is up (the night over, the picture still black from the dip): the bounce BounceAtDawn
	 * held is laid, once. Nothing when none waits. */
	void Dawn();
}
