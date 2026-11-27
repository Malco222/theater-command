#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class APlayerController;
class ASeaBattle;
class UTexture2D;

/**
 * The 3D sea battle's screen over the scene (release 2). For now the tug-of-war bar alone, at the top: the player's
 * flag at the left end and the enemy's at the right, his brass gold running from his flag and their slate from theirs,
 * meeting at the knot with the card's small gap; a pale mark at the middle, where it is level. No ticks, no label, no
 * words (the UI director's ruling of 21 November: "the battle bar is the release 1 card's bar, moving"). The knot is
 * the scene's (ASeaBattle::GetBar, eased there); this only draws, and takes no input.
 *
 * The month's submarine scene (D1, ruling 241; ASeaBattle::BeginMonthScene) is played inside the campaign: there the
 * screen also shows the scene's caption, the month's lines, low over the sea as it ends, and takes the keys and the
 * pointer, any key or click skipping the scene back to the map.
 *
 * With the sea free eye on (-TheaterSeaFreeEye=1, a full battle watched live; Battle/SEA-FREE-EYE.md) the screen also
 * takes the free camera's keys (W A S D and the arrows, Q, E, Shift with them, J, Tab, C, U), the wheel, a trackpad's
 * slide, pinch and twist, and the left button's clicks and double clicks, and hands them to the scene; every other key
 * and button it leaves unhandled, for the campaign as with the switch off.
 */
class SSeaBattleScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSeaBattleScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ASeaBattle>, Battle)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600.0, 1000.0); }
	// (the month's scene: a key or a click skips it; the sea free eye: the camera's keys, wheel, trackpad and clicks)
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnTouchGesture(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnFocusLost(const FFocusEvent& Event) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	/** A pressed or let go key handed to the sea free eye as the glue knows it (with Shift, and whether Control, Command
	 * or Option is held); true when the glue's SeaFreeEye::ScreenKey says it is the screen's (Handled), false for one it
	 * leaves to the campaign. */
	bool FreeKeyOf(const FKeyEvent& Event, bool bDown);
	/** A left click or double click at the screen's point, handed to the scene as a share of the picture. */
	FReply FreeClickAt(const FGeometry& Geometry, const FPointerEvent& Event, bool bDouble);
	TWeakObjectPtr<ASeaBattle> Battle;
	/** Each flag's brush, made the first time its cloth is drawn. */
	mutable FSlateBrush MineBrush, TheirsBrush;
	mutable TWeakObjectPtr<UTexture2D> MineCloth, TheirsCloth;
};

/**
 * (D1) The moment after the month's scene: an empty, unseen guard over the map that takes every key and click until
 * none has come for half a second and no button is held, then gives the keys and the pointer back to the campaign
 * (its own mode, as the controller sets it) and takes itself away. The key or click that skipped the scene, or one
 * hurried after it, so never reaches the map (where Enter would end the month), as the controller keeps a card's own
 * presses from the map.
 */
class SSeaKeyGuard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSeaKeyGuard) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Controller)
	SLATE_END_ARGS()

	/** Puts one over the map and gives it the keys and the pointer. */
	static void Raise(APlayerController* Player);

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0, 1.0); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		return LayerId;
	}

private:
	EActiveTimerReturnType Check(double Now, float Delta);
	FReply Held();
	TWeakObjectPtr<APlayerController> Controller;
	double LastAt = 0.0;
};
