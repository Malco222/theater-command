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
 *
 * A live battle with a side of the player's own (release 2) is his to command: the general's screen over the same
 * bar, each block's paper tag on its brass stalk, his orders' routes, his units' cards along the bottom and the order
 * bar (ALandBattle's LandBattleCommand.cpp says where and what; this draws it and hands it his pointer and keys). Keys
 * there: Space pauses (orders still go), 1 2 3 life, twice, four times, H hold, R fall back, Tab his next unit, C the
 * film (this screen as before), Esc the map again or his hand put down. His view (LandGeneralEye): W A S D or the arrows
 * move it, Q E turn it, the wheel or a pinch zooms, a two-finger slide or the middle button drags the ground, the
 * pointer at the screen's edge moves it, a double click on the ground glides there, a unit's card clicked twice follows
 * that unit.
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
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnTouchGesture(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void OnFocusLost(const FFocusEvent& Event) override;

private:
	TWeakObjectPtr<ALandBattle> Battle;
	struct FButton
	{
		FSlateRect Rect;
		FString Action;
	};
	mutable TArray<FButton> Buttons;
	/** The tags as drawn this frame, for a click on one (local units). */
	struct FTagBox
	{
		FSlateRect Rect;
		int32 Block = -1;
	};
	mutable TArray<FTagBox> TagBoxes;
	/** The general's mouse button held (-1 none): his drags are his, not the camera's look aside. */
	int32 GeneralHeld = -1;
	/** His view's keys held (W A S D and the arrows, Q E), a bit each; and the middle button sliding the view. */
	uint32 ViewKeys = 0;
	bool bSliding = false;
	/** The block under the pointer (a tag or one of his cards; -1 none), and where the pointer is (local units): its
	 * plate names what it belongs to, its real men and the drawing ratio. */
	mutable int32 HoverBlock = -1;
	FVector2D HoverAt = FVector2D(-1.0, -1.0);
	/** The view's keys held handed to the scene (and let go of all at once when the screen loses the keyboard). */
	void HandViewKeys();
	/** The two flags' brushes, made once their pictures are there. */
	mutable FSlateBrush FlagBrush[2];
	mutable bool bFlagMade[2] = { false, false };
	bool bDragging = false;
	double SpeedBefore = 1.0;

	const FButton* ButtonAt(const FVector2D& Local) const;
	void Do(const FString& Action);
	/** The general's screen is up (a live battle of his, the film not holding the camera). */
	bool IsGeneral() const;
	/** The viewport's pixels from this widget's own units, and back. */
	FVector2D ToPixels(const FGeometry& Geometry, const FVector2D& Local) const;
	FVector2D ToLocal(const FGeometry& Geometry, const FVector2D& Pixels) const;
	int32 TagUnder(const FVector2D& Local) const;
	/** The general's tags, routes, drags, cards and order bar; the height his cards take at the bottom. */
	double PaintGeneral(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
};
