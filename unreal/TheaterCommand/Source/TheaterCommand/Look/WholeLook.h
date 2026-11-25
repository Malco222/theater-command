#pragma once

#include "CoreMinimal.h"

/**
 * The whole picture's look (Brian, 25 November 2026 03:58Z: "can we work on better overall graphics in the background?").
 *
 * A few steps that no one scene owns, laid over every scene alike: the map, the ground, the air and the sea battles. Each
 * is a console variable, off unless something turns it on: the release 2 ship list (a row of the Variable kind, with its
 * own line on OPTIONS), a launch word, or the photographer's timing rounds ("-TheaterTime=off:tc.Look.Crisp=0|on:
 * tc.Look.Crisp=1"), which flip it inside one running game so that the two pictures are taken in the same warmth.
 *
 * - tc.Look.Crisp (-TheaterLookCrisp): the picture sharpened after the upscaler. The map and the battles are drawn at
 *   about 1080 by 670 and stretched to the Mac's 2881 by 1786, which softens every edge and every letter painted on the
 *   ground; the engine's own light sharpening (r.Tonemapper.Sharpen) is laid on, and the ground's pictures are read
 *   with sixteen-fold filtering where they lie at a slant (r.MaxAnisotropy), so a field seen across keeps its grain.
 *   tc.Look.CrispSharpen and tc.Look.CrispFiltering tune it.
 * - tc.Look.Depth (-TheaterLookDepth): the soft shade where things meet and stand close (ambient occlusion) reaching
 *   as far as the battle is seen. The engine fades it out from 30 to 80 m from the eye, and nothing in the game said
 *   otherwise, so the general's views from 100 to 300 m up had none: a hedge, a wood's edge, a tank and a house sat on the
 *   field as if pasted there. Laid by a post-process volume over the whole world with only these settings in it; it
 *   shows only where a scene draws ambient occlusion at all (the ground battle does; the map's speed trims keep it off).
 *   tc.Look.DepthStrength, tc.Look.DepthReach (metres) and tc.Look.DepthQuality tune it.
 * - tc.Look.Bounce (-TheaterLookBounce): every ground gives back its own colour to what faces it, as snow already did. The
 *   battle has no bounced light of its own (Lumen is off for speed), so a hull's shaded side, a hedge's underside and a
 *   man's legs took the table cubemap's dark ground on every field but snow: the desert's sand gave no warm fill and
 *   the steppe's grass no green. Read once as the field is laid (the mean of its paint on a grid) and laid under the
 *   battle's sky light by LandBattle's HideTheTable. tc.Look.BounceStrength tunes it (snow keeps its own, 2).
 */
namespace WholeLook
{
	/** At the game module's start, after the release 2 ship list has set its variables: reads the launch words, lays what
	 * is on, and starts the small clock that keeps the looks as their variables say (four times a second). */
	void Start();

	/** At the module's end: the clock stopped. */
	void Stop();

	/** tc.Look.Bounce is on: the ground battle lays its field's own colour under the sky light on every ground. */
	bool BounceOn();
	/** How strongly (tc.Look.BounceStrength): the field's mean paint, as light, is multiplied by it. */
	float BounceStrength();
	/** The mean of a field's paint (sRGB bytes, read by ColourAt) on a 24 by 24 grid over Wide by Deep metres from its
	 * south-west corner, as linear light. Once a battle, as the field is laid. */
	FLinearColor GroundMean(TFunctionRef<FColor(double X, double Y)> ColourAt, double Wide, double Deep);
}
