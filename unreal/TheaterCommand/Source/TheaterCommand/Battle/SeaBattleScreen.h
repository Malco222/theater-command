#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class ASeaBattle;
class UTexture2D;

/**
 * The 3D sea battle's screen over the scene (release 2). For now the tug-of-war bar alone, at the top: the player's
 * flag at the left end and the enemy's at the right, his brass gold running from his flag and their slate from theirs,
 * meeting at the knot with the card's small gap; a pale mark at the middle, where it is level. No ticks, no label, no
 * words (the UI director's ruling of 21 November: "the battle bar is the release 1 card's bar, moving"). The knot is
 * the scene's (ASeaBattle::GetBar, eased there); this only draws, and takes no input.
 */
class SSeaBattleScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSeaBattleScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ASeaBattle>, Battle)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600.0, 1000.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<ASeaBattle> Battle;
	/** Each flag's brush, made the first time its cloth is drawn. */
	mutable FSlateBrush MineBrush, TheirsBrush;
	mutable TWeakObjectPtr<UTexture2D> MineCloth, TheirsCloth;
};
