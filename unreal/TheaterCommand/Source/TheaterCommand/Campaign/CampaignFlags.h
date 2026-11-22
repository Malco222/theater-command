#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * The flags the nations flew between 1939 and 1945, painted in code.
 *
 * Each flag is drawn from simple shapes (bands, crosses, discs, stars) into a
 * small picture when the game starts, so there are no image files to keep and a
 * new nation needs only a few lines here. Every flag is drawn three by two,
 * whatever its true proportions, so that all the standards on the table are the
 * same shape and only their size says how big the army is.
 *
 * Germany flies the black, white and red with the iron cross: the choice most
 * games of the period make, and the one the player asked for.
 */
namespace CampaignFlags
{
	/** The size of the picture. It is stored two by one and stretched onto a three-by-two flag. */
	constexpr int32 SizeX = 512;
	constexpr int32 SizeY = 256;

	/**
	 * Paints the flag of the nation with this tag. A nation with no flag here gets a plain colour of its own
	 * with a pale band top and bottom, and the function returns false so that the gap can be noticed.
	 */
	bool Paint(const FString& Tag, const FLinearColor& NationColour, TArray<FLinearColor>& OutPixels);

	/** Makes a texture (with smaller copies for when it is seen from far off) from a painted flag. */
	UTexture2D* MakeTexture(const TArray<FLinearColor>& Pixels, const FName& Name);

	/** Every tag that has a flag of its own, for the development contact sheet. */
	TArray<FString> KnownTags();
}
