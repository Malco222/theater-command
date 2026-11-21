#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class ALandBattle;

/**
 * The ground battle's screen over the 3D scene: the top bar (the battle's name, place and date; the tug-of-war bar with
 * each nation's flag at its end, the player's on the left, brass meeting slate at the knot, a pale mark at the middle;
 * HOW NEAR TO BREAKING OFF under it; the clock), the battle's dispatches as it speaks them, the speed, and the after
 * card with CONTINUE. The look is the UI director's (the release 1 card's bar, moving: her ruling of 21 November
 * 14:47Z); the words and the knot are LandDesk's. It only draws.
 *
 * Keys: Space holds the clock or lets it run, 1 to 4 the speeds, C cuts to a new shot, H holds this shot, Enter
 * CONTINUE. A drag looks a little aside; let go, the camera comes back to its shot.
 */
class SLandBattleScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLandBattleScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ALandBattle>, Battle)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600.0, 1000.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	TWeakObjectPtr<ALandBattle> Battle;
	struct FButton
	{
		FSlateRect Rect;
		FString Action;
	};
	mutable TArray<FButton> Buttons;
	/** The two flags' brushes, made once their pictures are there. */
	mutable FSlateBrush FlagBrush[2];
	mutable bool bFlagMade[2] = { false, false };
	bool bDragging = false;
	double SpeedBefore = 1.0;

	const FButton* ButtonAt(const FVector2D& Local) const;
	void Do(const FString& Action);
};
