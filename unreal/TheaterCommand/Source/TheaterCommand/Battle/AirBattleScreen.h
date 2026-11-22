#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AAirBattle;

/**
 * The air battle's screen over the 3D scene (the UI director's words, handoff sections 1, 4, 6 and 8): the top bar
 * (the sides, the card's chance and the chance now, the tug-of-war bar between the two flags, HOW NEAR TO BREAKING
 * OFF), the clock and its tag, the
 * aim slip, the near line, a tag over each formation, the dispatches, the orders strip for the groups in hand, the
 * speed, the staff's question, and the after card with CONTINUE. Everything it writes is AirDesk's; it only draws.
 *
 * Mouse: a left click takes a group of his in hand (shift adds it), a right click orders it (an enemy's tag: attack
 * it; the sky: patrol there), a drag turns the camera, the wheel draws it in and out. A double click on any tag, his
 * or the enemy's, rides behind that squadron (the chase view); in the close view a click on a tag rides that one.
 * Keys: H hold, R break off, C the close view, Space holds the clock or lets it run, 1 to 4 the speeds, Esc answers
 * KEEP FIGHTING (or leaves the close view), Enter CONTINUE; in the close view left and right step from aircraft to
 * aircraft of the squadron ridden, up and down from squadron to squadron.
 */
class SAirBattleScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAirBattleScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAirBattle>, Battle)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600.0, 1000.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	TWeakObjectPtr<AAirBattle> Battle;
	/** The buttons as last drawn (local units), each with what it does. */
	struct FButton
	{
		FSlateRect Rect;
		FString Action;
	};
	mutable TArray<FButton> Buttons;
	/** The flags at the ends of the tug-of-war bar, by nation, made once each from the table's own pictures. */
	mutable TMap<FString, TSharedPtr<struct FSlateBrush>> FlagBrushes;
	FVector2D PressedAt = FVector2D::ZeroVector;
	bool bDragging = false;
	bool bLeftDown = false;
	double SpeedBefore = 1.0;

	/** A point of the widget (local units) in the viewport's pixels, where the scene's tags are placed. */
	FVector2D ToPixels(const FGeometry& Geometry, const FVector2D& Local) const;
	FVector2D ToLocal(const FGeometry& Geometry, const FVector2D& Pixels) const;
	const FButton* ButtonAt(const FVector2D& Local) const;
	void Do(const FString& Action);
};
