#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Pictures made in code and handed to the graphics chip: the paint of the campaign
 * table, and the fine detail (grass, tree-tops, rock, sand, ripples on the water)
 * that is laid over it.
 *
 * Nothing here is an image file. The detail pictures are worked out from noise
 * when the game starts, each made so that its right edge runs into its left and
 * its bottom into its top, and can be laid side by side like tiles without a seam.
 */
namespace CampaignTextures
{
	/**
	 * Makes a picture from pixels, with the smaller copies the chip uses when it is seen from far off.
	 * bColour says the pixels are colours as the eye sees them (sRGB); otherwise they are plain numbers.
	 * bTile says the picture repeats; otherwise its edge is held.
	 */
	UTexture2D* Create(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, bool bTile, const FName& Name);

	/** Paints new pixels into a picture made by Create. The size must be the same. */
	void Update(UTexture2D* Texture, const TArray<FColor>& Pixels);

	/** The size of the detail pictures, each way. */
	constexpr int32 DetailSize = 512;

	/**
	 * Light and dark for the four kinds of ground, each in its own channel and each averaging mid-grey:
	 * red is grass (the fine speckle of model-railway flock), green is tree-tops seen from above,
	 * blue is bare rock, alpha is sand.
	 */
	void MakeGroundDetail(TArray<FColor>& OutPixels);
	/**
	 * The fields of the quilt, one pixel a field (FieldsSize each way, read without smoothing so that every field keeps
	 * its own tone): how each field's colour stands to the average of them all, from pasture through wheat to
	 * plough. Half is no change; the material doubles it and multiplies the ground by it.
	 */
	constexpr int32 FieldsSize = 64;
	void MakeFields(TArray<FColor>& OutPixels);

	/** Which way the surface leans, for the two kinds of ground that have a shape: tree-tops in red and green, rock in blue and alpha. */
	void MakeGroundBumps(TArray<FColor>& OutPixels);

	/** Which way the water's surface leans: two different sets of ripples, in red and green and in blue and alpha. */
	void MakeWaterBumps(TArray<FColor>& OutPixels);
}
