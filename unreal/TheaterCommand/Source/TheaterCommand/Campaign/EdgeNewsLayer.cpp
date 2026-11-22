#include "Campaign/EdgeNewsLayer.h"

#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/EdgeNews.h"
#include "Campaign/EdgeSeals.h"
#include "Campaign/TheaterSpeed.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#include <cmath>
#include <vector>

// (Release 2, campaign map plan item 5, "News at the map's edge"; Brian's card of 16 November 2026, ruling 131.)
// The month's seals on the screen. Where each lies is EdgeSeals' pure layout; this file draws them, slides them in,
// answers the pointer, and frames the row a seal opened on the month's card. A handful of widgets: one layer, one
// leaf for the ping, one leaf per seal (nine at most). Laying them again each frame allocates nothing once warm.

namespace
{
	// The UI kit's colours, as the month's card wears them on its rows' edges (CampaignHUD's MonthRowOf) and its bands
	// (LayOutMonthCard, MonthNewsBrush): a seal wears the colour of the row it opens (the art director's ruling of 22
	// November, section 1). Which colour, and whether it pulses, is EdgeSeals::LookOf's; here they are only inked.
	const FLinearColor SealGood = FLinearColor(FColor(147, 181, 108));
	const FLinearColor SealRed = FLinearColor(FColor(208, 86, 61));
	const FLinearColor SealAmber = FLinearColor(FColor(224, 161, 60));
	const FLinearColor SealGrey = FLinearColor(FColor::FromHex(TEXT("A89F8D")));
	// (brass-300: a plain brass seal's wax, the news band's rim and pressed sign, the pointer's ring, the card's frame)
	const FLinearColor SealLitBrass = FLinearColor(FColor::FromHex(TEXT("E0B872")));
	// (the news band, the kit's tc-band--news: its ground (108,80,36) shading to (85,63,29))
	const FLinearColor SealBandWax = FLinearColor(FColor(108, 80, 36));
	const FLinearColor SealBandShade = FLinearColor(FColor(85, 63, 29));
	// (the ribbon is the date strip's paper, with a fold of its darker edge down it)
	const FLinearColor SealRibbon = FLinearColor(FColor(214, 201, 168));
	const FLinearColor SealRibbonFold = FLinearColor(FColor(150, 132, 96));

	// A seal's box, in the frame's units: the disc at its head, the ribbon's two tails under it (EdgeSeals::Look).
	constexpr double SealBoxW = EdgeSeals::Look::BoxW;
	constexpr double SealBoxH = EdgeSeals::Look::BoxH;
	constexpr double SealDiscR = EdgeSeals::Look::DiscR;
	constexpr double SealMidX = EdgeSeals::Look::MidX;
	constexpr double SealMidY = EdgeSeals::Look::MidY;
	constexpr int32 SealSlots = FEdgeNewsBoard::MostSeals;

	FLinearColor SealShade(const FLinearColor& Colour, float By)
	{
		return FLinearColor(Colour.R * By, Colour.G * By, Colour.B * By, Colour.A);
	}

	/** The colour a seal's row wears on the card (EdgeSeals::EWax): the news band's is its brass-300 edge. */
	FLinearColor SealKitOf(int32 Wax)
	{
		switch (static_cast<EdgeSeals::EWax>(Wax))
		{
		case EdgeSeals::EWax::Red:
			return SealRed;
		case EdgeSeals::EWax::Green:
			return SealGood;
		case EdgeSeals::EWax::Amber:
			return SealAmber;
		case EdgeSeals::EWax::Grey:
			return SealGrey;
		case EdgeSeals::EWax::Band:
		case EdgeSeals::EWax::Brass:
			break;
		}
		return SealLitBrass;
	}

	/** The inks one seal is drawn in. */
	struct FSealInks
	{
		FLinearColor Wax; // the disc, its side toward the light; the call's glow takes it too, and its ring but on the news band's seal
		FLinearColor Shade; // the disc's side away from the light, and the runs
		FLinearColor Hollow; // the hollow the die pressed
		FLinearColor Rim;
		FLinearColor Lit; // the hollow's wall that catches the light
		FLinearColor Dark; // its wall in shadow
		FLinearColor Mark; // the pressed sign
		FLinearColor MarkWall; // the sign's other wall: lit (a dark sign) or in shadow (a brass-300 one)
		FLinearColor Call; // the call's spreading ring: the wax, or the news band's brass-300 rim
		bool bWallLit = true;
		float Shown = 1.0f;
	};

	/** A seal's inks from its colour: the kit's colour shaded, or the news band's own; dull and grey at 55% once opened. */
	FSealInks SealInksOf(const FEdgeNewsSeal& Seal)
	{
		FSealInks Inks;
		if (static_cast<EdgeSeals::EWax>(Seal.Wax) == EdgeSeals::EWax::Band)
		{
			// (the news band's own colours, so that the seal and its row match: the wax its ground, the rim and the sign
			// its brass-300, which also tells it from a plain brass seal)
			Inks.Wax = SealBandWax;
			Inks.Shade = SealBandShade;
			Inks.Hollow = SealBandShade;
			Inks.Rim = SealLitBrass;
			Inks.Lit = SealLitBrass.CopyWithNewOpacity(0.5f);
			Inks.Dark = SealShade(SealBandShade, 0.45f);
			Inks.Mark = SealLitBrass;
			Inks.MarkWall = SealShade(SealBandShade, 0.45f);
			Inks.bWallLit = false;
			// (its ring is its rim's brass-300: the band's dark wax, at 55% or less, would hardly be seen spreading over the
			// map. At 55% or less it is still dimmer than the rim it comes from, so never brighter than the seal)
			Inks.Call = SealLitBrass;
		}
		else
		{
			const FLinearColor Kit = SealKitOf(Seal.Wax);
			Inks.Wax = SealShade(Kit, 0.9f);
			Inks.Shade = SealShade(Kit, 0.71f);
			Inks.Hollow = SealShade(Kit, 0.66f);
			Inks.Rim = SealShade(Kit, 0.66f);
			Inks.Lit = FMath::Lerp(Kit, FLinearColor::White, 0.45f).CopyWithNewOpacity(0.6f);
			Inks.Dark = SealShade(Kit, 0.34f);
			Inks.Mark = Inks.Dark;
			Inks.MarkWall = Inks.Lit;
			Inks.bWallLit = true;
			Inks.Call = Inks.Wax;
		}
		if (Seal.bOpened)
		{
			// (opened: the wax goes dull and grey, and the seal stands back until the next month's are laid)
			FLinearColor* const Each[] = { &Inks.Wax, &Inks.Shade, &Inks.Hollow, &Inks.Rim, &Inks.Lit, &Inks.Dark, &Inks.Mark, &Inks.MarkWall, &Inks.Call };
			for (FLinearColor* Ink : Each)
			{
				const float Grey = 0.3f * Ink->R + 0.59f * Ink->G + 0.11f * Ink->B;
				const float Alpha = Ink->A * 0.55f;
				*Ink = FMath::Lerp(*Ink, FLinearColor(Grey, Grey, Grey, Ink->A), 0.6f).CopyWithNewOpacity(Alpha);
			}
			Inks.Shown = 0.55f;
		}
		return Inks;
	}

	/** The seals stand while the map has the screen and nothing is laid over it that they would stand under. */
	bool EdgeNewsShown(const ACampaignPlayerController* Player)
	{
		return Player && Player->GetEdgeNews().Seals.Num() > 0 && Player->IsMapShown() && !Player->IsFrameHidden() && !Player->IsWatching()
			&& !Player->IsPageOpen() && !Player->GetFaceOff().bVisible && !Player->GetWarPrompt().bVisible && Player->GetAnnouncement() == nullptr;
	}

	/** What the layer, its seals and its ping share: where each seal is now and where it is going, and the pointer. */
	struct FEdgeNewsLaid
	{
		TWeakObjectPtr<ACampaignPlayerController> Player;
		TFunction<const FSlateBrush*(const FString&)> SignOf;
		TFunction<void(TArray<FSlateRect>&)> KeepOffOf;
		TArray<FSlateRect> KeepOff;
		FSlateFontInfo CountFace;

		/** The board's laying these are for: a new one slides the seals in again. */
		int32 Revision = INDEX_NONE;
		double LaidAt = -1.0;

		/** The screen's pixels to one of the layer's units, as Tick measures it (the viewport's pixels over the layer's size); 0 until measured. */
		double PxPerUnit = 0.0;

		int32 Hovered = INDEX_NONE;
		double HoveredAt = 0.0;
		bool bPing = false;
		FVector2D PingAt = FVector2D::ZeroVector;
		FLinearColor PingInk = FLinearColor::White;

		/** A slot holds the board's seal of the same number, or the "+N" seal (bRest) after the last item that stands. */
		struct FNow
		{
			bool bLive = false; // laid, and on the screen
			bool bArrived = false; // slid all the way in
			bool bRest = false; // the "+N" seal: the items with no room, and those past the most that may stand
			int32 Edge = -1;
			double SlideAt = 0.0; // when it set off
			FVector2D At = FVector2D::ZeroVector; // its box's middle now, in the layer's units
			FVector2D Target = FVector2D::ZeroVector;
			FVector2D From = FVector2D::ZeroVector;
			int32 Folded = 0; // how many items the "+N" seal stands for, and that as it is pressed in it and as its words say
			FString Count;
			FString RestWords;
		};
		FNow Seals[SealSlots];

		EdgeSeals::FParams Params;
		EdgeSeals::FLayout Layout;
		EdgeSeals::FFold Standing;
		std::vector<EdgeSeals::FWant> Wants;
		std::vector<EdgeSeals::FBox> Reserved;
		std::vector<EdgeSeals::FSpot> Spots;
	};

	/** The seal a slot holds now: the board's of the same number, or the "+N" seal; nullptr for none. */
	const FEdgeNewsSeal* EdgeSealInSlot(const FEdgeNewsLaid& Laid, const ACampaignPlayerController* Player, int32 Slot)
	{
		if (!Player || Slot < 0 || Slot >= SealSlots)
		{
			return nullptr;
		}
		const FEdgeNewsBoard& Board = Player->GetEdgeNews();
		return Laid.Seals[Slot].bRest ? &Board.Rest : Board.Seals.IsValidIndex(Slot) ? &Board.Seals[Slot] : nullptr;
	}

	/**
	 * Draws the discs, rings and bars a seal is made of. MakeBox fills a rounded box with the TINT it is given and never
	 * reads the brush's own colour (only the outline takes OutlineSettings.Color), so each fill passes its own ink as the tint.
	 */
	struct FSealPen
	{
		FSlateWindowElementList& Out;
		const FGeometry& On;

		void Disc(int32 Layer, double MidX, double MidY, double Radius, const FLinearColor& Ink) const
		{
			const FSlateRoundedBoxBrush Round(Ink, static_cast<float>(Radius));
			FSlateDrawElement::MakeBox(Out, Layer, On.ToPaintGeometry(FVector2D(2.0 * Radius, 2.0 * Radius), FSlateLayoutTransform(FVector2D(MidX - Radius, MidY - Radius))), &Round, ESlateDrawEffect::None, Round.TintColor.GetSpecifiedColor());
		}

		void Ring(int32 Layer, double MidX, double MidY, double Radius, double Width, const FLinearColor& Ink) const
		{
			// (the fill is the tint: a hair of the ink, not quite 0, so the element is never culled as fully see-through and
			// its outline, which takes OutlineSettings.Color, still draws; at 1/255 the filled disc cannot be seen)
			const FLinearColor Hollow = Ink.CopyWithNewOpacity(1.0f / 255.0f);
			const FSlateRoundedBoxBrush Round(Hollow, static_cast<float>(Radius), Ink, static_cast<float>(Width));
			FSlateDrawElement::MakeBox(Out, Layer, On.ToPaintGeometry(FVector2D(2.0 * Radius, 2.0 * Radius), FSlateLayoutTransform(FVector2D(MidX - Radius, MidY - Radius))), &Round, ESlateDrawEffect::None, Hollow);
		}

		/** A bar with rounded ends, Long by Thick, its middle at (MidX, MidY), turned Degrees clockwise. */
		void Bar(int32 Layer, double MidX, double MidY, double Long, double Thick, double Degrees, const FLinearColor& Ink) const
		{
			const FSlateRoundedBoxBrush Round(Ink, static_cast<float>(Thick * 0.5));
			const FGeometry Turned = On.MakeChild(FVector2D(Long, Thick), FSlateLayoutTransform(FVector2D(MidX - Long * 0.5, MidY - Thick * 0.5)), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(static_cast<float>(Degrees)))), FVector2D(0.5, 0.5));
			FSlateDrawElement::MakeBox(Out, Layer, Turned.ToPaintGeometry(), &Round, ESlateDrawEffect::None, Round.TintColor.GetSpecifiedColor());
		}
	};

	/** The ping: rings spreading from the place of the seal under the pointer, where it is on the screen. */
	class SEdgeNewsPing : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEdgeNewsPing) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<FEdgeNewsLaid>& InLaid)
		{
			Laid = InLaid;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.0, 8.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (!Laid.IsValid() || !Laid->bPing)
			{
				return LayerId;
			}
			const FSealPen Pen{ OutDrawElements, Geometry };
			const double Since = FPlatformTime::Seconds() - Laid->HoveredAt;
			const FVector2D At = Laid->PingAt;
			for (int32 Wave = 0; Wave < 3; ++Wave)
			{
				const double Phase = std::fmod(Since + Wave * 0.4, 1.2) / 1.2;
				Pen.Ring(LayerId, At.X, At.Y, 6.0 + 30.0 * Phase, 2.0, Laid->PingInk.CopyWithNewOpacity(static_cast<float>(0.85 * (1.0 - Phase))));
			}
			Pen.Disc(LayerId + 1, At.X, At.Y, 5.0, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
			Pen.Disc(LayerId + 2, At.X, At.Y, 3.5, Laid->PingInk);
			return LayerId + 3;
		}

	private:
		TSharedPtr<FEdgeNewsLaid> Laid;
	};

	/** One seal: a disc of wax with its row's sign pressed in it and a short ribbon tail. The pointer reads it; a press opens its row. */
	class SEdgeNewsSealMark : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEdgeNewsSealMark) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<FEdgeNewsLaid>& InLaid, int32 InSlot)
		{
			Laid = InLaid;
			SealSlot = InSlot;
			const TWeakPtr<FEdgeNewsLaid> Weak = InLaid;
			const int32 Mine = InSlot;
			SetVisibility(TAttribute<EVisibility>::CreateLambda([Weak, Mine]()
			{
				const TSharedPtr<FEdgeNewsLaid> Shared = Weak.Pin();
				return Shared.IsValid() && Shared->Seals[Mine].bLive ? EVisibility::Visible : EVisibility::Collapsed;
			}));
			// (the row's eight words; the engine's tip, which the frame has dressed in its paper and its typed face)
			SetToolTipText(TAttribute<FText>::CreateLambda([Weak, Mine]()
			{
				const TSharedPtr<FEdgeNewsLaid> Shared = Weak.Pin();
				const FEdgeNewsSeal* Told = Shared.IsValid() ? EdgeSealInSlot(*Shared, Shared->Player.Get(), Mine) : nullptr;
				return Told ? FText::FromString(Shared->Seals[Mine].bRest ? Shared->Seals[Mine].RestWords : Told->Words) : FText::GetEmpty();
			}));
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(SealBoxW, SealBoxH); }

		virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			SLeafWidget::OnMouseEnter(MyGeometry, MouseEvent);
			if (Laid->Hovered != SealSlot)
			{
				Laid->Hovered = SealSlot;
				Laid->HoveredAt = FPlatformTime::Seconds();
			}
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			SLeafWidget::OnMouseLeave(MouseEvent);
			if (Laid->Hovered == SealSlot)
			{
				Laid->Hovered = INDEX_NONE;
			}
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled(); // (a drag of the map begun on a seal is still the map's)
			}
			if (ACampaignPlayerController* Player = Laid->Player.Get())
			{
				Laid->Hovered = INDEX_NONE;
				Player->OpenEdgeSeal(Laid->Seals[SealSlot].bRest ? INDEX_NONE : SealSlot);
			}
			return FReply::Handled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
		{
			return FCursorReply::Cursor(EMouseCursor::Hand);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			THEATER_SPEED_SCOPE(PaintEdgeSeal);
			const FEdgeNewsSeal* Held = Laid.IsValid() ? EdgeSealInSlot(*Laid, Laid->Player.Get(), SealSlot) : nullptr;
			if (!Held)
			{
				return LayerId;
			}
			const FEdgeNewsSeal& Seal = *Held;
			const double Now = FPlatformTime::Seconds();
			const bool bHot = Laid->Hovered == SealSlot;
			// (a dated warning, and war and peace and the news, call until opened: the seal breathes, a glow comes and goes
			// under it and a ring spreads from it. EdgeSeals::LookOf says which seals call; Look::CallOf how)
			const bool bCalling = Seal.bPulse && !Seal.bOpened;
			const EdgeSeals::Look::FCall Call = bCalling ? EdgeSeals::Look::CallOf(Now - Laid->LaidAt) : EdgeSeals::Look::FCall();
			const FSealInks Inks = SealInksOf(Seal);
			const float Shown = Inks.Shown;
			// (the frame's units to the screen's pixels, to hold the pressed sign's strokes to 1.5 px: as the layer measures
			// it each frame, the viewport's pixels over its own size, about 1.84 in Brian's window on the Mac's fine pixels.
			// Before the first measure, the paint's own scale: FrameScale times Unreal's for the window)
			const double PxPerUnit = Laid->PxPerUnit > 0.0 ? Laid->PxPerUnit : FMath::Max(static_cast<double>(Geometry.Scale), 0.01);

			const double Grow = bHot ? EdgeSeals::Look::HoverGrow : Call.Grow;
			const FGeometry Face = Geometry.MakeChild(FVector2D(SealBoxW, SealBoxH), FSlateLayoutTransform(static_cast<float>(Grow), FVector2D(SealMidX, SealMidY) * (1.0 - Grow)));
			const FSealPen Pen{ OutDrawElements, Face };
			int32 Layer = LayerId;

			// The call: the glow in the seal's own wax and the spreading ring in its wax (the news band's: its brass-300
			// rim), never more than 55% opaque, so never brighter than the seal; the ring rises from nothing at the rim,
			// fades as it spreads and is no wider than twice the disc.
			if (bCalling)
			{
				Pen.Disc(Layer, SealMidX, SealMidY, Call.GlowR, Inks.Wax.CopyWithNewOpacity(static_cast<float>(Call.Glow)));
				Pen.Ring(Layer + 1, SealMidX, SealMidY, Call.RingR, Call.RingW, Inks.Call.CopyWithNewOpacity(static_cast<float>(Call.Ring)));
				Layer += 2;
			}
			// The ribbon's two short tails, from under the disc: neither longer than the disc is tall.
			{
				const FLinearColor Tape = SealRibbon.CopyWithNewOpacity(Shown), Fold = SealRibbonFold.CopyWithNewOpacity(0.7f * Shown);
				for (const EdgeSeals::Look::FTail& Tail : EdgeSeals::Look::Tails)
				{
					Pen.Bar(Layer, Tail.X, Tail.Y, Tail.Long, Tail.Thick, Tail.Degrees, Tape);
					Pen.Bar(Layer + 1, Tail.X, Tail.Y, Tail.Long - 2.0, 1.1, Tail.Degrees, Fold);
				}
				Layer += 2;
			}
			// Its shadow on the map, away from the light; the four small, uneven runs of wax at the rim; the disc, lit from
			// the upper left (its shaded side, and over it the lit side moved toward the light); and the rim.
			Pen.Disc(Layer, SealMidX + 1.3, SealMidY + 2.0, SealDiscR, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Shown));
			++Layer;
			for (const EdgeSeals::Look::FRun& Run : EdgeSeals::Look::Runs)
			{
				const double Turn = FMath::DegreesToRadians(Run.Degrees);
				Pen.Disc(Layer, SealMidX + Run.Out * std::cos(Turn), SealMidY + Run.Out * std::sin(Turn), Run.Radius, Inks.Shade);
			}
			if (Seal.bOpened)
			{
				// (opened: one flat disc at 55%, neither shaded nor lit. The rim, the hollow and its lips and the sign still
				// lie over it and so read darker: the hollow about 80%, the sign about 90 to 95%)
				Pen.Disc(Layer + 1, SealMidX, SealMidY, SealDiscR, Inks.Wax);
			}
			else
			{
				Pen.Disc(Layer + 1, SealMidX, SealMidY, SealDiscR, Inks.Shade);
				Pen.Disc(Layer + 2, SealMidX + EdgeSeals::Look::LightX, SealMidY + EdgeSeals::Look::LightY, SealDiscR - 1.2, Inks.Wax);
			}
			Pen.Ring(Layer + 3, SealMidX, SealMidY, SealDiscR - 0.6, EdgeSeals::Look::Stroke(EdgeSeals::Look::RimW, PxPerUnit), Inks.Rim);
			Layer += 4;
			// The hollow the die pressed, lit from the upper left like the kit's other raised pieces: its lower-right
			// inner wall catches the light, its upper-left wall is in shadow (Look::HollowLips).
			const EdgeSeals::Look::FLips Lips = EdgeSeals::Look::HollowLips();
			Pen.Disc(Layer, SealMidX, SealMidY, EdgeSeals::Look::HollowR, Inks.Hollow);
			Pen.Ring(Layer + 1, SealMidX + Lips.LitX, SealMidY + Lips.LitY, Lips.R, 1.1, Inks.Lit);
			Pen.Ring(Layer + 2, SealMidX + Lips.ShadeX, SealMidY + Lips.ShadeY, Lips.R, 1.0, Inks.Dark.CopyWithNewOpacity(0.5f * Shown));
			Layer += 3;
			// What the die pressed: our own order sign, or a drawn mark, about 18 across with no stroke under 1.5 px. Each
			// stroke is a groove lit from the upper left: a dark one shows its lower-right wall lit; a brass-300 one (the
			// news band's) its upper-left wall in shadow. The wall is about a pixel wide (Look::WallOf).
			const double Wall = Inks.bWallLit ? EdgeSeals::Look::WallOf(PxPerUnit) : -EdgeSeals::Look::WallOf(PxPerUnit);
			PaintMark(Seal, Pen, Layer, Wall, Wall, Inks.MarkWall, PxPerUnit);
			PaintMark(Seal, Pen, Layer + 1, 0.0, 0.0, Inks.Mark, PxPerUnit);
			Layer += 2;
			// The shine on the wax's lip, toward the light, and the brass ring the pointer puts round it.
			Pen.Bar(Layer, SealMidX - 6.4, SealMidY - 10.8, 7.0, 2.4, -28.0, FLinearColor(1.0f, 1.0f, 1.0f, 0.22f * Shown));
			if (bHot)
			{
				Pen.Ring(Layer + 1, SealMidX, SealMidY, SealDiscR + 2.2, 1.5, SealLitBrass);
			}
			return Layer + 2;
		}

	private:
		/** The sign or the mark in the seal's hollow, moved (DX, DY), in one ink, its strokes held to 1.5 px at PxPerUnit. */
		void PaintMark(const FEdgeNewsSeal& Seal, const FSealPen& Pen, int32 Layer, double DX, double DY, const FLinearColor& Ink, double PxPerUnit) const
		{
			const double MX = SealMidX + DX, MY = SealMidY + DY;
			EdgeSeals::EGlyph Glyph = static_cast<EdgeSeals::EGlyph>(Seal.Glyph);
			if (Glyph == EdgeSeals::EGlyph::Sign)
			{
				const FSlateBrush* Sign = Laid->SignOf && !Seal.Sign.IsEmpty() ? Laid->SignOf(Seal.Sign) : nullptr;
				// (the order sign is pressed only where its own thinnest stroke holds 1.5 px on this screen: under that it
				// turns to a smudge, and the seal takes its drawn mark instead. Look::SignHolds)
				if (Sign && EdgeSeals::Look::SignHolds(Seal.SignThinnest, Sign->ImageSize.X, Sign->ImageSize.Y, PxPerUnit))
				{
					// (the order sign, fitted into 18 by 12.5 as it was drawn: Look::FitSign)
					const EdgeSeals::FPoint Fitted = EdgeSeals::Look::FitSign(Sign->ImageSize.X, Sign->ImageSize.Y);
					const FVector2D Size(Fitted.X, Fitted.Y);
					FSlateDrawElement::MakeBox(Pen.Out, Layer, Pen.On.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D(MX, MY) - Size * 0.5)), Sign, ESlateDrawEffect::None, Ink);
					return;
				}
				Glyph = EdgeSeals::EGlyph::News; // (a sign not drawn yet, or too fine for this screen: the plain mark)
			}
			if (Glyph == EdgeSeals::EGlyph::More)
			{
				const FString& Count = Laid->Seals[SealSlot].Count;
				if (!Count.IsEmpty() && FSlateApplication::IsInitialized())
				{
					const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Count, Laid->CountFace);
					FSlateDrawElement::MakeText(Pen.Out, Layer, Pen.On.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D(MX, MY) - Size * 0.5)), Count, Laid->CountFace, ESlateDrawEffect::None, Ink);
				}
				return;
			}
			EdgeSeals::Look::FStroke Cuts[4];
			const int32 CutCount = EdgeSeals::Look::MarkOf(Glyph, PxPerUnit, Cuts);
			for (int32 Each = 0; Each < CutCount; ++Each)
			{
				const EdgeSeals::Look::FStroke& Cut = Cuts[Each];
				switch (Cut.Kind)
				{
				case EdgeSeals::Look::EStroke::Bar:
					Pen.Bar(Layer, MX + Cut.X, MY + Cut.Y, Cut.Long, Cut.Thick, Cut.Degrees, Ink);
					break;
				case EdgeSeals::Look::EStroke::Dot:
					Pen.Disc(Layer, MX + Cut.X, MY + Cut.Y, Cut.Long * 0.5, Ink);
					break;
				case EdgeSeals::Look::EStroke::Ring:
					Pen.Ring(Layer, MX + Cut.X, MY + Cut.Y, Cut.Long * 0.5, Cut.Thick, Ink);
					break;
				}
			}
		}

		TSharedPtr<FEdgeNewsLaid> Laid;
		int32 SealSlot = 0;
	};

	/** The layer: the ping under the seals, and the seals, each placed where EdgeSeals lays it and slid in from its edge. */
	class SEdgeNewsLayer : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEdgeNewsLayer) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<FEdgeNewsLaid>& InLaid)
		{
			Laid = InLaid;
			const TWeakObjectPtr<ACampaignPlayerController> Watched = InLaid->Player;
			SetVisibility(TAttribute<EVisibility>::CreateLambda([Watched]() { return EdgeNewsShown(Watched.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; }));
			TSharedRef<SOverlay> Seals = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
			Seals->AddSlot()[SNew(SEdgeNewsPing, InLaid)];
			const TWeakPtr<FEdgeNewsLaid> Weak = InLaid;
			for (int32 Each = 0; Each < SealSlots; ++Each)
			{
				Seals->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top)
				.Padding(TAttribute<FMargin>::CreateLambda([Weak, Each]()
				{
					const TSharedPtr<FEdgeNewsLaid> Shared = Weak.Pin();
					const FVector2D At = Shared.IsValid() ? Shared->Seals[Each].At : FVector2D::ZeroVector;
					return FMargin(static_cast<float>(At.X - SealBoxW * 0.5), static_cast<float>(At.Y - SealBoxH * 0.5), 0.0f, 0.0f);
				}))
				[
					SNew(SEdgeNewsSealMark, InLaid, Each)
				];
			}
			ChildSlot
			[
				Seals
			];
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			THEATER_SPEED_SCOPE(EdgeNews);
			FEdgeNewsLaid& Shared = *Laid;
			ACampaignPlayerController* Player = Shared.Player.Get();
			Shared.bPing = false;
			if (!EdgeNewsShown(Player))
			{
				Shared.Hovered = INDEX_NONE;
				return;
			}
			const FEdgeNewsBoard& Board = Player->GetEdgeNews();
			const double Now = FPlatformTime::Seconds();
			const int32 Count = FMath::Min(Board.Seals.Num(), SealSlots);
			if (Board.Revision != Shared.Revision || Board.LaidAt != Shared.LaidAt)
			{
				// A new month's seals: each sets off from beyond its edge the first time they are seen, in card order,
				// about an eighth of a second apart.
				Shared.Revision = Board.Revision;
				Shared.LaidAt = Board.LaidAt;
				Shared.Hovered = INDEX_NONE;
				for (int32 Each = 0; Each < SealSlots; ++Each)
				{
					FEdgeNewsLaid::FNow& Seal = Shared.Seals[Each];
					Seal.bLive = false;
					Seal.bArrived = false;
					Seal.bRest = false;
					Seal.Edge = -1;
					Seal.SlideAt = Now + 0.12 * Each;
				}
			}
			const FVector2D Local = AllottedGeometry.GetLocalSize();
			if (Local.X < 1.0 || Local.Y < 1.0)
			{
				return;
			}
			// (the map's places are put on the screen in the viewport's pixels; the seals are laid in the frame's units)
			FVector2D View(0.0, 0.0);
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(View);
			}
			const FVector2D PerUnit = View.X > 1.0 && View.Y > 1.0 ? FVector2D(View.X / Local.X, View.Y / Local.Y) : FVector2D(1.0, 1.0);
			// (and so the pixels a unit of the seals takes on the screen, whatever scales Unreal and the frame put between
			// them: the seals hold their strokes to 1.5 px by it)
			Shared.PxPerUnit = View.X > 1.0 && View.Y > 1.0 ? PerUnit.Y : 0.0;
			const ACampaignMapActor* Table = Player->GetMap();
			auto PlaceOf = [Player, Table, PerUnit](const FEdgeNewsSeal& Seal, FVector2D& OutAt)
			{
				if (!Table)
				{
					return false;
				}
				const FCampaignMapData& Data = Table->GetData();
				FVector2D OnMap(0.0, 0.0);
				if (Data.Regions.IsValidIndex(Seal.Region))
				{
					OnMap = Data.Regions[Seal.Region].Center;
				}
				else if (Data.Seas.IsValidIndex(Seal.Sea))
				{
					OnMap = Data.Seas[Seal.Sea].Center;
				}
				else
				{
					return false;
				}
				FVector2D Pixel(0.0, 0.0);
				if (!Player->ProjectWorldLocationToScreen(Table->MapToWorld(OnMap.X, OnMap.Y), Pixel))
				{
					return false; // (behind the eye: the seal waits in the stack at the top)
				}
				OutAt = Pixel / PerUnit;
				return true;
			};

			// Where each seal lies, again each frame as the camera moves and the frame folds and unfolds: held still while
			// the pointer is on one, so that it does not slide away from under it.
			const bool bHold = Shared.Hovered >= 0 && Shared.Hovered < SealSlots && Shared.Seals[Shared.Hovered].bLive;
			if (!bHold)
			{
				Shared.Params.ScreenW = Local.X;
				Shared.Params.ScreenH = Local.Y;
				Shared.Reserved.clear();
				if (Shared.KeepOffOf)
				{
					// (the frame's pieces, and the panels that stand over the map and are none: the staff's note, the adviser)
					Shared.KeepOffOf(Shared.KeepOff);
					for (const FSlateRect& Piece : Shared.KeepOff)
					{
						const FVector2D Min = AllottedGeometry.AbsoluteToLocal(FVector2D(Piece.Left, Piece.Top));
						const FVector2D Max = AllottedGeometry.AbsoluteToLocal(FVector2D(Piece.Right, Piece.Bottom));
						Shared.Reserved.push_back(EdgeSeals::FBox{ Min.X, Min.Y, Max.X, Max.Y });
					}
				}
				Shared.Wants.resize(static_cast<size_t>(Count));
				for (int32 Each = 0; Each < Count; ++Each)
				{
					FVector2D At(0.0, 0.0);
					EdgeSeals::FWant& Want = Shared.Wants[static_cast<size_t>(Each)];
					Want.bPlace = Board.Seals[Each].HasPlace() && PlaceOf(Board.Seals[Each], At);
					Want.At = EdgeSeals::FPoint{ At.X, At.Y };
					Want.Kept = Shared.Seals[Each].bLive && !Shared.Seals[Each].bRest ? Shared.Seals[Each].Edge : -1;
				}
				// (as many as the edge has room for, in the card's ranking; the rest fold into "+N", so none is lost: ruling 64(g))
				Shared.Standing = Shared.Layout.LayRanked(Shared.Params, Shared.Wants, Board.Items, SealSlots, Shared.Reserved, Shared.Spots);
				for (int32 Each = 0; Each < SealSlots; ++Each)
				{
					FEdgeNewsLaid::FNow& Seal = Shared.Seals[Each];
					const bool bRestHere = Shared.Standing.More > 0 && Each == Shared.Standing.Shown;
					if ((Each >= Shared.Standing.Shown && !bRestHere) || static_cast<size_t>(Each) >= Shared.Spots.size() || !Shared.Spots[static_cast<size_t>(Each)].bShown)
					{
						Seal.bLive = false;
						continue;
					}
					const EdgeSeals::FSpot& Spot = Shared.Spots[static_cast<size_t>(Each)];
					if ((!Seal.bLive || Seal.bRest != bRestHere) && Seal.SlideAt < Now)
					{
						// (a seal that comes back, or the "+N" seal taking the slot of an item folded into it, or the item
						// taking it back: it slides in afresh from its edge)
						Seal.bArrived = false;
						Seal.SlideAt = Now;
					}
					else if (Seal.bLive && Seal.bArrived && Seal.Edge != static_cast<int32>(Spot.Edge))
					{
						// (its place has gone round a corner: it leaves and slides in again on the new edge)
						Seal.bArrived = false;
						Seal.SlideAt = Now;
					}
					Seal.bRest = bRestHere;
					if (bRestHere && Seal.Folded != Shared.Standing.More)
					{
						Seal.Folded = Shared.Standing.More;
						Seal.Count = FString::Printf(TEXT("+%d"), Seal.Folded);
						Seal.RestWords = FString::Printf(TEXT("%d more in the month's report"), Seal.Folded);
					}
					Seal.bLive = true;
					Seal.Edge = static_cast<int32>(Spot.Edge);
					Seal.Target = FVector2D(Spot.Centre.X, Spot.Centre.Y);
					Seal.From = FVector2D(Spot.From.X, Spot.From.Y);
				}
			}

			// How they move: sliding in, eased as they arrive; once in, following their spots as the camera moves.
			const double Follow = 1.0 - FMath::Exp(-9.0 * static_cast<double>(InDeltaTime));
			for (int32 Each = 0; Each < SealSlots; ++Each)
			{
				FEdgeNewsLaid::FNow& Seal = Shared.Seals[Each];
				if (!Seal.bLive)
				{
					continue;
				}
				if (!Seal.bArrived)
				{
					const double Share = EdgeSeals::SlideShare(Now - Seal.SlideAt, 0);
					Seal.At = Seal.From + (Seal.Target - Seal.From) * Share;
					Seal.bArrived = Share >= 1.0;
				}
				else
				{
					Seal.At += (Seal.Target - Seal.At) * Follow;
				}
			}

			// The ping: the place of the seal under the pointer, where it is on the screen.
			if (Shared.Hovered >= 0 && Shared.Hovered < Count && Shared.Seals[Shared.Hovered].bLive && !Shared.Seals[Shared.Hovered].bRest && Board.Seals[Shared.Hovered].HasPlace())
			{
				FVector2D At(0.0, 0.0);
				if (PlaceOf(Board.Seals[Shared.Hovered], At) && At.X >= 0.0 && At.Y >= 0.0 && At.X <= Local.X && At.Y <= Local.Y)
				{
					Shared.bPing = true;
					Shared.PingAt = At;
					Shared.PingInk = FMath::Lerp(SealKitOf(Board.Seals[Shared.Hovered].Wax), FLinearColor::White, 0.25f);
				}
			}
		}

	private:
		TSharedPtr<FEdgeNewsLaid> Laid;
	};

	/** On the month's card: the brass frame round the row a seal opened, bright at first and then steady while the card stays open. */
	class SEdgeNewsRowFrame : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEdgeNewsRowFrame) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TWeakObjectPtr<ACampaignPlayerController>& InPlayer, int32 InSection, int32 InIndex)
		{
			Player = InPlayer;
			Section = InSection;
			Index = InIndex;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(0.0, 0.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const ACampaignPlayerController* Shown = Player.Get();
			if (!Shown || !Shown->IsPageOpen())
			{
				return LayerId;
			}
			const FEdgeNewsBoard& Board = Shown->GetEdgeNews();
			if (Board.FocusSection != Section || Board.FocusIndex != Index || Board.FocusPage != Shown->GetPage().Revision)
			{
				return LayerId;
			}
			const double Since = FPlatformTime::Seconds() - Board.FocusAt;
			const double Lit = Since < 1.2 ? 1.0 : FMath::Lerp(1.0, 0.6, FMath::Clamp((Since - 1.2) / 0.8, 0.0, 1.0));
			const FSlateRoundedBoxBrush Frame(SealLitBrass.CopyWithNewOpacity(static_cast<float>(0.12 * Lit)), 4.0f, SealLitBrass.CopyWithNewOpacity(static_cast<float>(Lit)), 2.0f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), &Frame, ESlateDrawEffect::None, Frame.TintColor.GetSpecifiedColor());
			return LayerId + 1;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Player;
		int32 Section = 0;
		int32 Index = 0;
	};
}

TSharedRef<SWidget> MakeEdgeNewsLayer(const TWeakObjectPtr<ACampaignPlayerController>& Player, TFunction<const FSlateBrush*(const FString&)> SignOf, TFunction<void(TArray<FSlateRect>&)> KeepOffOf, const FSlateFontInfo& CountFace)
{
	const TSharedRef<FEdgeNewsLaid> Laid = MakeShared<FEdgeNewsLaid>();
	Laid->Player = Player;
	Laid->SignOf = MoveTemp(SignOf);
	Laid->KeepOffOf = MoveTemp(KeepOffOf);
	Laid->KeepOff.Reserve(32);
	Laid->CountFace = CountFace;
	Laid->Wants.reserve(SealSlots);
	Laid->Spots.reserve(SealSlots);
	Laid->Reserved.reserve(32);
	return SNew(SEdgeNewsLayer, Laid);
}

void EdgeNewsKeepOff(const TArray<FSlateRect>& Frame, const TSharedPtr<SWidget>& Root, std::initializer_list<TSharedPtr<SWidget>> Panels, TArray<FSlateRect>& Out)
{
	Out.Reset();
	Out.Append(Frame);
	if (!Root.IsValid())
	{
		return;
	}
	const FSlateRect Whole = Root->GetPaintSpaceGeometry().GetLayoutBoundingRect();
	for (const TSharedPtr<SWidget>& Inside : Panels)
	{
		// The panel is the box that holds it among the screen's own layers: it counts while it, and every box between it
		// and the screen, is shown (as the HUD's IsShownNow asks of a piece of the frame).
		TSharedPtr<SWidget> Panel = Inside;
		bool bStands = false;
		for (int32 Guard = 0; Panel.IsValid() && Guard < 64; ++Guard)
		{
			if (!Panel->GetVisibility().IsVisible())
			{
				break;
			}
			const TSharedPtr<SWidget> Up = Panel->GetParentWidget();
			if (Up.Get() == Root.Get())
			{
				bStands = Root->GetVisibility().IsVisible();
				break;
			}
			Panel = Up;
		}
		if (!bStands)
		{
			continue;
		}
		// (as it was last painted, in the window's own measure, as the frame's pieces are kept; a box half as wide as the
		// screen is one of its full layers, never a panel: it is not kept off, or no seal would stand)
		const FSlateRect Rect = Panel->GetPaintSpaceGeometry().GetLayoutBoundingRect();
		if (Rect.GetSize().X > 1.0 && Rect.GetSize().Y > 1.0 && Rect.GetSize().X < Whole.GetSize().X * 0.5)
		{
			Out.Add(Rect);
		}
	}
}

void FEdgeNewsCardMarks::Begin(const TWeakObjectPtr<ACampaignPlayerController>& InPlayer)
{
	Player = InPlayer;
	bFresh = false;
	const ACampaignPlayerController* Shown = Player.Get();
	if (Shown && Shown->IsPageOpen())
	{
		const int32 Page = Shown->GetPage().Revision;
		bFresh = Shown->GetEdgeNews().FocusPage == Page && SeenPage != Page;
		if (bFresh)
		{
			SeenPage = Page;
			// (the "+N" seal opens the card at its head: FocusSection -1)
			if (Shown->GetEdgeNews().FocusSection < 0)
			{
				Target.Reset();
				bToHead = true;
				FramesLeft = 3;
			}
		}
	}
}

bool FEdgeNewsCardMarks::Unfolds(int32 Folded) const
{
	const ACampaignPlayerController* Shown = Player.Get();
	return bFresh && Shown && Shown->GetEdgeNews().FocusSection == Row && Shown->GetEdgeNews().FocusIndex >= Folded;
}

TSharedRef<SWidget> FEdgeNewsCardMarks::Mark(int32 Section, int32 Index, const TSharedRef<SWidget>& Item)
{
	const ACampaignPlayerController* Shown = Player.Get();
	if (bFresh && Shown && Shown->GetEdgeNews().FocusSection == Section && Shown->GetEdgeNews().FocusIndex == Index)
	{
		Target = Item;
		FramesLeft = 3;
	}
	return SNew(SOverlay)
		+ SOverlay::Slot()[Item]
		+ SOverlay::Slot()[SNew(SEdgeNewsRowFrame, Player, Section, Index)];
}

void FEdgeNewsCardMarks::Tick()
{
	if (FramesLeft <= 0 || --FramesLeft > 0)
	{
		return;
	}
	const TSharedPtr<SWidget> Opened = Target.Pin();
	Target.Reset();
	if (bToHead)
	{
		bToHead = false;
		if (Scroll.IsValid())
		{
			Scroll->ScrollToStart();
		}
		return;
	}
	if (Opened.IsValid() && Scroll.IsValid())
	{
		Scroll->ScrollDescendantIntoView(Opened, true, EDescendantScrollDestination::Center);
	}
}
