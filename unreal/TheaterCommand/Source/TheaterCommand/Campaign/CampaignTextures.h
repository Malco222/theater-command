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
	 * bSharp says it is read without smoothing between its pixels (each pixel is a value of its own, not a shade
	 * between neighbours). (Speed, tc.Speed.PictureOnce: set here, the picture is made once. Set on the picture after
	 * it is made, it has to be made again, and that second making waits for the drawing thread.)
	 */
	UTexture2D* Create(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, bool bTile, const FName& Name, bool bSharp = false);

	/**
	 * (Speed, 17 November 2026.) A picture's every level worked out ahead, on any thread: the full size first, then each
	 * smaller copy exactly as Create makes them. CreateFromLevels then makes the same picture Create would make from
	 * Each[0], without that work in the frame that wants the picture.
	 */
	struct FLevels
	{
		int32 Width = 0;
		int32 Height = 0;
		bool bColour = false;
		TArray<TArray<FColor>> Each;
		TArray<FIntPoint> Sizes;
		/** Whether these are the levels Create would make from these pixels: the same size and kind, and the same pixels at full size. */
		bool Fits(int32 InWidth, int32 InHeight, bool bInColour, const TArray<FColor>& Pixels) const;
	};
	void MakeLevels(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, FLevels& Out);
	/**
	 * bSlant says the picture is mostly seen at a slant, as the ground is from a plane: it is read through the World
	 * group's anisotropic filter, which picks the smaller copy by the short side of a pixel's patch of picture and takes
	 * several samples along the long side. Plain smoothing picks the copy by the long side, and so smears the picture
	 * across the view two to four times past what the screen could show (the air battle's ground, 20 November 2026).
	 */
	UTexture2D* CreateFromLevels(const FLevels& Levels, bool bTile, const FName& Name, bool bSharp = false, bool bSlant = false);

	/** Paints new pixels into a picture made by Create. The size must be the same. */
	void Update(UTexture2D* Texture, const TArray<FColor>& Pixels);
	/**
	 * Puts a box of a picture's pixels on the table at once, without making the picture's smaller copies again: for
	 * paint that moves several times a second (ink that spreads). The smaller copies are stale until Update is called.
	 */
	void UpdateBox(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1);
	/**
	 * (Speed, 17 November 2026.) The same box, and with it the box's share of every smaller copy of the picture, so
	 * that the far view is right without the whole picture being halved and sent again: ground that changes hands
	 * costs what the province costs, not what the world costs. MaxLevel stops at that smaller copy (the air battle's
	 * craters, 20 November 2026: a crater is lost in a pixel of 64 m, and the copies above it, each made from the whole
	 * picture, cost some 20 ms a crater); the copies above it keep what they had.
	 */
	void UpdateBoxAll(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel = MAX_int32);

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

	/**
	 * The moving sea's one picture, which repeats: in red a swell's heights and in green a chop's (mid-grey is level
	 * water; both run along the picture's breadth, with crests that are short and broken), in blue scattered soft
	 * flecks for the glints, and in alpha a slow unevenness that sets each stretch of coast its own moment.
	 */
	void MakeWaves(TArray<FColor>& OutPixels);

	/**
	 * The weather's one picture, which repeats: in red soft cloud shapes that cover about two fifths of it, in green
	 * the billow inside a cloud, in blue long flat bands of mist, and in alpha thin leaning streaks of rain, or with
	 * bSnow small soft flakes.
	 */
	void MakeWeather(TArray<FColor>& OutPixels, bool bSnow);

	/**
	 * The wisps of the fog of war's haze, a picture that repeats (red): bands lying along its breadth, each with a
	 * blunt head toward the picture's right and a tail that thins away to its left, with lumps along it. Its values
	 * are spread as the haze's old wisps were, so that the haze has no more contrast than it had.
	 */
	void MakeHaze(TArray<FColor>& OutPixels);
}
