#pragma once

#include "CoreMinimal.h"

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
 *   every picture made as seen at a slant, the ones already made with them. The campaign map's land pictures are made
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
 *   the Mediterranean's red would cast faces and hulls), following the sun (none with the sun under the horizon, whole
 *   from 15 degrees up), at half under a closed canopy, and laid under the battle's sky light by LandBattle's
 *   HideTheTable. Snow keeps its own, as shipped. tc.Look.BounceStrength (1.0), tc.Look.BounceClosed (0.5) and
 *   tc.Look.BounceHue (1; 0 leaves the hue as the field's, -TheaterLookBounceUnclamped) tune it.
 *
 * The whole look's lens: one post-process volume over the whole play world (the map and every battle are laid in the one
 * world), above the ground battle's grade volume, carrying only the sharpening and the ambient occlusion fields. No other
 * volume or camera in the game sets those fields, so it fights nothing.
 *
 * The tunings can be set from the launch line too, for the Mac's sweeps: -TheaterLookCrispSharpen=,
 * -TheaterLookCrispFiltering=, -TheaterLookDepthStrength=, -TheaterLookDepthReach= (metres), -TheaterLookDepthRadius=
 * (metres), -TheaterLookDepthQuality=, -TheaterLookDepthClosed=, -TheaterLookBounceStrength=, -TheaterLookBounceClosed=.
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
	 * south-west corner, as linear light. Once a battle, as the field is laid. */
	FLinearColor GroundMean(TFunctionRef<FColor(double X, double Y)> ColourAt, double Wide, double Deep);

	/** The light a field gives back from below, from its mean paint (GroundMean, linear): the hue pulled inside the art
	 * director's box with the value kept (unless tc.Look.BounceHue is 0), times tc.Look.BounceStrength, times
	 * tc.Look.BounceClosed under a closed canopy, times the sun's share (SunHeight the sine of the sun's height: none
	 * under the horizon, whole from 15 degrees up). Alpha 1. */
	FLinearColor BounceLight(const FLinearColor& Mean, bool bClosedCanopy, double SunHeight);

	/** The ground battle says what it is fought under as its field is laid (a closed canopy: the jungle; the art director:
	 * open ground is where more than about half the opening view lies in direct sun), and that it is over. The far shade
	 * is laid at tc.Look.DepthClosed under a closed canopy. */
	void FieldLaid(bool bClosedCanopy);
	void FieldGone();
}
