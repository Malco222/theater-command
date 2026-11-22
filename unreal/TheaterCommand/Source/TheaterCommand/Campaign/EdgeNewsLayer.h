#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/SlateRect.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <initializer_list>

class ACampaignPlayerController;
class SScrollBox;
class SWidget;
struct FSlateBrush;

/**
 * (Release 2, campaign map plan item 5, "News at the map's edge"; ruling 131.) The screen's side of the month's seals:
 *
 * MakeEdgeNewsLayer: the layer laid over the map, under the frame, that holds the seals. Each slides in from the edge
 * nearest its row's place, in card order, about 0.12 s apart, and keeps off every piece of the frame (the right
 * column, the date strip, the tray, the minimap) and every panel that stands over the map (the staff's note, the
 * adviser): KeepOffOf fills an array with them, in absolute measure, as the frame last laid them out (EdgeNewsKeepOff).
 * As many stand as the edge has room for, in the card's ranking, and the rest fold into the "+N" seal. Classes 1 and 2
 * are red and pulse slowly until opened. The pointer on a seal gives its eight words and pings its place on the map; a
 * press opens the month's card at its row. SignOf gives an order sign's picture by its key (Content/CityOrders), and
 * CountFace is the face the "+N" is pressed in.
 *
 * EdgeNewsKeepOff: what the seals keep off, into Out (which keeps its room between frames): the frame's pieces (Frame,
 * absolute), and for each of Panels that stands on the screen now, the box that holds it among the layers of the
 * screen's root (Root): it and every box between it and the root shown. A panel may be given by any widget inside it.
 *
 * FEdgeNewsCardMarks: on the month's card, the row a seal opened is framed in brass and scrolled to, and the fold of
 * the fronts' rows opens for a row under it. The HUD calls Begin as it lays the card out, Unfolds where the rows fold,
 * Mark round each item of the card, and Tick once a frame.
 */
TSharedRef<SWidget> MakeEdgeNewsLayer(const TWeakObjectPtr<ACampaignPlayerController>& Player, TFunction<const FSlateBrush*(const FString&)> SignOf, TFunction<void(TArray<FSlateRect>&)> KeepOffOf, const FSlateFontInfo& CountFace);
void EdgeNewsKeepOff(const TArray<FSlateRect>& Frame, const TSharedPtr<SWidget>& Root, std::initializer_list<TSharedPtr<SWidget>> Panels, TArray<FSlateRect>& Out);

struct FEdgeNewsCardMarks
{
	/** The card's parts, as Mark takes them (EdgeSeals::ESection). */
	static constexpr int32 Band = 0;
	static constexpr int32 Lead = 1;
	static constexpr int32 Row = 2;
	static constexpr int32 Home = 3;
	static constexpr int32 Staff = 4;
	static constexpr int32 Elsewhere = 5;

	/** The month card's scroll box (the HUD's page body sits in it). */
	TSharedPtr<SScrollBox> Scroll;

	/** As the card is laid out: whether this is its first laying since a seal opened it. */
	void Begin(const TWeakObjectPtr<ACampaignPlayerController>& InPlayer);
	/** True where the row a seal has just opened the card at lies under the fold of the fronts' rows (Folded rows shown before it). */
	bool Unfolds(int32 Folded) const;
	/** One item of the card, framed while it is the row a seal opened (Section: EdgeSeals::ESection). Changes nothing of its layout. */
	TSharedRef<SWidget> Mark(int32 Section, int32 Index, const TSharedRef<SWidget>& Item);
	/** Scrolls the opened row into view a few frames after the card is laid out, once its rows have their sizes (the "+N" seal: to the card's head). */
	void Tick();

private:
	TWeakObjectPtr<ACampaignPlayerController> Player;
	bool bFresh = false;
	int32 SeenPage = -1;
	TWeakPtr<SWidget> Target;
	int32 FramesLeft = 0;
	/** The "+N" seal opened the card: it goes to the card's head, not to a row. */
	bool bToHead = false;
};
