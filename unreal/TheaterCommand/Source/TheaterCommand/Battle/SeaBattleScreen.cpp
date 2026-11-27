#include "Battle/SeaBattleScreen.h"

#include "Battle/SeaBattle.h"
#include "Battle/SeaWordsCore.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SViewport.h"

namespace SeaScreenParts
{
	// The bar's colours (the UI director's ruling, 21 November, as the air thread's picture has them): the player's brass
	// gold and the enemy's slate, held for the whole battle and never red; the pale mark at the middle; the dark plate
	// under it all, so that it reads over a bright sky.
	const FLinearColor Gold = FLinearColor(FColor::FromHex(TEXT("C9A24A")));
	const FLinearColor Slate = FLinearColor(FColor::FromHex(TEXT("6F7B88")));
	const FLinearColor Pale = FLinearColor(FColor::FromHex(TEXT("EFE6CF")));
	const FLinearColor Plate = FLinearColor(0.02f, 0.015f, 0.01f, 0.55f);
	// The torpedo words' plain cream (the UI director, 25 November 13:27Z: "plain cream like the state words, no colour"):
	// the air screen's own cream (AirBattleScreen.cpp's Cream: his own tags' names, his dispatches), on the bar's dark plate.
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("EDE6D6")));

	/** The bar and its flags, in the screen's own units (the window's height makes them larger, as the air screen's). */
	constexpr double BarWide = 400.0, BarHigh = 8.0, FlagWide = 27.0, FlagHigh = 18.0, FlagGap = 10.0, Top = 16.0;
	/** The card's small gap where the two colours meet (the knot), and how near an end the knot is ever drawn. */
	constexpr double KnotGap = 3.0, KnotRoom = 0.02;
	/** The torpedo words: a tag's height and the stalk under it down to its ship, and the line's plate, its height and how
	 * far over the screen's foot it stands (clear of the month scene's way back to the map, 34 over it). */
	constexpr double TagHigh = 18.0, TagStalk = 8.0, LineHigh = 26.0, LineFoot = 64.0;

	static void Cloth(FSlateBrush& Brush, TWeakObjectPtr<UTexture2D>& Held, UTexture2D* Texture)
	{
		if (Held.Get() == Texture)
		{
			return;
		}
		Held = Texture;
		Brush = FSlateBrush();
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(FlagWide, FlagHigh);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
}

void SSeaBattleScreen::Construct(const FArguments& Args)
{
	Battle = Args._Battle;
	// (inside the campaign, the month's scene: the screen takes the keys and the pointer, to skip it; else it takes none)
	SetVisibility(Battle.IsValid() && Battle->IsMonthScene() ? EVisibility::Visible : EVisibility::HitTestInvisible);
}

bool SSeaBattleScreen::SupportsKeyboardFocus() const
{
	return Battle.IsValid() && Battle->IsMonthScene();
}

FReply SSeaBattleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Battle.IsValid() && Battle->IsMonthScene())
	{
		// U1 hook (under water): a modifier key alone never skips the scene (Shift+U begins with Shift), and U takes the
		// camera under the sea or back up; any other key skips, as before.
		if (Event.GetKey().IsModifierKey())
		{
			return FReply::Unhandled();
		}
		if (Event.GetKey() == EKeys::U)
		{
			// (a key held down repeats: only its first press counts, so holding U never bobs the eye in and out, nor holding
			// Shift+U flips the switch and writes the setting over and over)
			if (!Event.IsRepeat())
			{
				Battle->UnderKey(Event.IsShiftDown());
			}
			return FReply::Handled();
		}
		Battle->SkipMonthScene();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SSeaBattleScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Battle.IsValid() && Battle->IsMonthScene())
	{
		Battle->SkipMonthScene();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SSeaBattleScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace SeaScreenParts;
	if (!Battle.IsValid())
	{
		return LayerId;
	}
	static const FSlateColorBrush Solid(FLinearColor::White);
	// The month's scene: its caption low over the sea as it ends, on a dark plate, and the way back to the map.
	int32 Over = LayerId;
	// (the caption's plate and the way back's words, where drawn: no torpedo tag over them)
	double CaptionL = 0.0, CaptionT = 0.0, CaptionR = -1.0, CaptionB = -1.0;
	double BackL = 0.0, BackT = 0.0, BackR = -1.0, BackB = -1.0;
	if (Battle->IsMonthScene())
	{
		const FSlateFontInfo Face = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15);
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const float TextScale = FMath::Max(Geometry.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
		auto WideOf = [&Measure, TextScale](const FString& Words, const FSlateFontInfo& Font) { return static_cast<double>(Measure->Measure(Words, Font, TextScale).X) / TextScale; };
		auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Font, const FLinearColor& Ink, int32 Layer)
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(WideOf(Words, Font) + 8.0, 40.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, Font, ESlateDrawEffect::None, Ink);
		};
		const FVector2D Size = Geometry.GetLocalSize();
		TArray<FString> Lines;
		float Shown = 0.0f;
		if (Battle->GetCaption(Lines, Shown) && Shown > 0.0f)
		{
			// (each line broken at spaces to two thirds of the screen's width)
			const double Room = FMath::Max(300.0, Size.X * 0.66);
			TArray<FString> Rows;
			for (const FString& Line : Lines)
			{
				TArray<FString> Words;
				Line.ParseIntoArray(Words, TEXT(" "), true);
				FString Now;
				for (const FString& Word : Words)
				{
					const FString Tried = Now.IsEmpty() ? Word : Now + TEXT(" ") + Word;
					if (!Now.IsEmpty() && WideOf(Tried, Face) > Room)
					{
						Rows.Add(Now);
						Now = Word;
					}
					else
					{
						Now = Tried;
					}
				}
				if (!Now.IsEmpty())
				{
					Rows.Add(Now);
				}
			}
			const double RowHigh = 26.0;
			double Widest = 0.0;
			for (const FString& Row : Rows)
			{
				Widest = FMath::Max(Widest, WideOf(Row, Face));
			}
			const double Top0 = Size.Y * 0.78 - Rows.Num() * RowHigh * 0.5;
			const FLinearColor Under = Plate * FLinearColor(1.0f, 1.0f, 1.0f, Shown);
			const FLinearColor Ink = Pale * FLinearColor(1.0f, 1.0f, 1.0f, Shown);
			if (Widest > 0.0 && Size.X > 0.0)
			{
				const double PlateX = (Size.X - Widest) * 0.5 - 18.0;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Widest + 36.0, Rows.Num() * RowHigh + 20.0), FSlateLayoutTransform(FVector2D(PlateX, Top0 - 10.0))), &Solid, ESlateDrawEffect::None, Under);
				CaptionL = PlateX;
				CaptionT = Top0 - 10.0;
				CaptionR = PlateX + Widest + 36.0;
				CaptionB = CaptionT + Rows.Num() * RowHigh + 20.0;
				for (int32 K = 0; K < Rows.Num(); ++K)
				{
					Write(Rows[K], (Size.X - WideOf(Rows[K], Face)) * 0.5, Top0 + K * RowHigh, Face, Ink, 1);
				}
			}
		}
		const FString Back = TEXT("Any key or click returns to the map");
		Write(Back, Size.X - WideOf(Back, Small) - 24.0, Size.Y - 34.0, Small, Pale * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f), 1);
		// (its words' room, a 10 point line, and 4 round it)
		BackL = Size.X - WideOf(Back, Small) - 24.0 - 4.0;
		BackT = Size.Y - 34.0 - 4.0;
		BackR = Size.X - 24.0 + 4.0;
		BackB = Size.Y - 34.0 + 18.0 + 4.0;
		Over = LayerId + 2;
	}
	// The torpedo words (Battle/SeaWordsCore; the naval rules behind their "Friendly torpedoes" switch): the camera's
	// subject's line, one line at the foot of the screen (none while the month scene's caption is up), and a small tag in
	// plain cream over the leading ship of each of the player's own pieces holding its salvo or turning clear of one; none
	// over the bar, the line, or the month scene's caption and way back, and never one on another (SeaWords::Lay: the
	// subject's first, then the nearest, each on its ship or lifted just over the one in its way).
	{
		TArray<ASeaBattle::FTorpedoTag> Placed;
		FString Said;
		if (Battle->GetTorpedoWords(Placed, Said))
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const float TextScale = FMath::Max(Geometry.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
			auto WideOf = [&Measure, TextScale](const FString& Words, const FSlateFontInfo& Font) { return static_cast<double>(Measure->Measure(Words, Font, TextScale).X) / TextScale; };
			auto Plated = [&](double X, double Y, double Wd, double Ht, const FLinearColor& Ink, int32 Layer)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
			};
			auto Lettered = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Font, int32 Layer)
			{
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(WideOf(Words, Font) + 8.0, 40.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, Font, ESlateDrawEffect::None, Cream);
			};
			const FVector2D Room = Geometry.GetLocalSize();
			// (the line's plate, where it is drawn: no tag over it)
			double LineL = 0.0, LineT = 0.0, LineR = -1.0, LineB = -1.0;
			TArray<FString> MonthLines;
			float MonthShown = 0.0f;
			const bool bMonthCaption = Battle->IsMonthScene() && Battle->GetCaption(MonthLines, MonthShown) && MonthShown > 0.0f;
			if (!Said.IsEmpty() && !bMonthCaption)
			{
				// (one line always: a smaller face where the screen is narrow)
				FSlateFontInfo LineFace = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
				for (int32 Points = 13; Points >= 10 && WideOf(Said, LineFace) > Room.X - 64.0; --Points)
				{
					LineFace = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), static_cast<float>(Points));
				}
				const double LineWide = WideOf(Said, LineFace);
				LineL = (Room.X - LineWide) * 0.5 - 14.0;
				LineR = LineL + LineWide + 28.0;
				LineB = Room.Y - LineFoot;
				LineT = LineB - LineHigh;
				Plated(LineL, LineT, LineR - LineL, LineHigh, Plate, 3);
				Lettered(Said, LineL + 14.0, LineT + 4.0, LineFace, 4);
			}
			// (the places no tag may cover: the bar's plate where the bar is up, as drawn below, and 4 round it; the line's plate
			// where it is drawn; in the month scene the caption's plate while it is up, and the way back's words)
			std::vector<SeaWords::Box> Kept;
			double KnotNow = 0.5;
			UTexture2D* FlagMine = nullptr;
			UTexture2D* FlagTheirs = nullptr;
			if (Battle->GetBar(KnotNow, FlagMine, FlagTheirs))
			{
				const double BarHalf = BarWide * 0.5 + FlagGap + FlagWide + 10.0 + 4.0;
				Kept.push_back(SeaWords::Box{ Room.X * 0.5 - BarHalf, -1.0e9, Room.X * 0.5 + BarHalf, Top + FlagHigh + 8.0 + 4.0 });
			}
			if (LineR > LineL)
			{
				Kept.push_back(SeaWords::Box{ LineL, LineT, LineR, LineB });
			}
			if (bMonthCaption && CaptionR > CaptionL)
			{
				Kept.push_back(SeaWords::Box{ CaptionL, CaptionT, CaptionR, CaptionB });
			}
			if (Battle->IsMonthScene() && BackR > BackL)
			{
				Kept.push_back(SeaWords::Box{ BackL, BackT, BackR, BackB });
			}
			const FSlateFontInfo TagFace = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
			std::vector<SeaWords::Spot> Spots;
			Spots.reserve(static_cast<size_t>(Placed.Num()));
			for (const ASeaBattle::FTorpedoTag& One : Placed)
			{
				SeaWords::Spot Foot;
				Foot.X = One.At.X * Room.X;
				Foot.Y = One.At.Y * Room.Y;
				Foot.Wide = WideOf(One.Text, TagFace) + 12.0;
				Foot.Deep = One.Deep;
				Foot.Piece = One.Piece;
				Foot.bSubject = One.bSubject;
				Spots.push_back(Foot);
			}
			const FLinearColor StalkInk = Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f);
			for (const SeaWords::Laid& Stood : SeaWords::Lay(Spots, Kept, TagHigh, TagStalk))
			{
				const ASeaBattle::FTorpedoTag& One = Placed[static_cast<int32>(Stood.Of)];
				Plated(Stood.Plate.L, Stood.Plate.T, Stood.Plate.R - Stood.Plate.L, TagHigh, Plate, 3);
				for (const std::pair<double, double>& Bit : Stood.Stalk)
				{
					Plated(Stood.FootX - 0.5, Bit.first, 1.0, Bit.second - Bit.first, StalkInk, 3);
				}
				Lettered(One.Text, Stood.Plate.L + 6.0, Stood.Plate.T + 2.0, TagFace, 4);
			}
			Over = FMath::Max(Over, LayerId + 5);
		}
	}
	double Knot = 0.5;
	UTexture2D* Mine = nullptr;
	UTexture2D* Theirs = nullptr;
	if (!Battle->GetBar(Knot, Mine, Theirs))
	{
		return Over;
	}
	auto Fill = [&](double X, double Y, double Wd, double Ht, const FLinearColor& Ink, int32 Layer)
	{
		if (Wd > 0.0 && Ht > 0.0)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
		}
	};
	const double Middle = Geometry.GetLocalSize().X * 0.5, BarX = Middle - BarWide * 0.5, BarY = Top + (FlagHigh - BarHigh) * 0.5;
	const double LeftFlag = BarX - FlagGap - FlagWide, RightFlag = BarX + BarWide + FlagGap;
	Fill(LeftFlag - 10.0, Top - 8.0, RightFlag + FlagWide + 10.0 - (LeftFlag - 10.0), FlagHigh + 16.0, Plate, 0);
	// His gold from his flag and their slate from theirs, meeting at the knot: the side winning holds more of the bar,
	// the line between pushed toward the loser's flag like a front line.
	const double At = BarX + BarWide * FMath::Clamp(Knot, KnotRoom, 1.0 - KnotRoom);
	Fill(BarX, BarY, At - KnotGap * 0.5 - BarX, BarHigh, Gold, 1);
	Fill(At + KnotGap * 0.5, BarY, BarX + BarWide - (At + KnotGap * 0.5), BarHigh, Slate, 1);
	// The pale mark at the middle: level.
	Fill(Middle - 1.0, BarY - 4.0, 2.0, BarHigh + 8.0, Pale * FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), 2);
	// The flags: the card's own, about a line of text high; they are the label.
	if (Mine)
	{
		Cloth(MineBrush, MineCloth, Mine);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(FlagWide, FlagHigh), FSlateLayoutTransform(FVector2D(LeftFlag, Top))), &MineBrush);
	}
	if (Theirs)
	{
		Cloth(TheirsBrush, TheirsCloth, Theirs);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(FlagWide, FlagHigh), FSlateLayoutTransform(FVector2D(RightFlag, Top))), &TheirsBrush);
	}
	return FMath::Max(LayerId + 3, Over);
}

// ---- (D1) The moment after the month's scene ----------------------------------------------------------------------

void SSeaKeyGuard::Raise(APlayerController* Player)
{
	if (!Player || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	const TSharedRef<SSeaKeyGuard> Guard = SNew(SSeaKeyGuard).Controller(Player);
	GEngine->GameViewport->AddViewportWidgetContent(Guard, 21);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Guard);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Player->SetInputMode(Mode);
}

void SSeaKeyGuard::Construct(const FArguments& Args)
{
	Controller = Args._Controller;
	LastAt = FPlatformTime::Seconds();
	SetVisibility(EVisibility::Visible); // (it takes the pointer's clicks over the whole map, unseen)
	RegisterActiveTimer(0.05f, FWidgetActiveTimerDelegate::CreateSP(this, &SSeaKeyGuard::Check));
}

FReply SSeaKeyGuard::Held()
{
	LastAt = FPlatformTime::Seconds();
	return FReply::Handled();
}

FReply SSeaKeyGuard::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return Held();
}

FReply SSeaKeyGuard::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return Held();
}

FReply SSeaKeyGuard::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return Held();
}

FReply SSeaKeyGuard::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return Held();
}

FReply SSeaKeyGuard::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return Held();
}

EActiveTimerReturnType SSeaKeyGuard::Check(double Now, float Delta)
{
	const bool bHeld = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Num() > 0;
	if (bHeld || FPlatformTime::Seconds() - LastAt < 0.5)
	{
		return EActiveTimerReturnType::Continue;
	}
	// (the campaign's own mode, as its controller sets it: the pointer in sight while a drag turns the table, and never
	// locked in; not where a screen raised since, a battle's, has taken the keys for itself)
	const TSharedPtr<SWidget> Focus = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
	const TSharedPtr<SWidget> Game = GEngine && GEngine->GameViewport ? TSharedPtr<SWidget>(GEngine->GameViewport->GetGameViewportWidget()) : nullptr;
	const bool bTaken = Focus.IsValid() && Focus.Get() != this && Focus != Game && Focus->GetType() != FName(TEXT("SWindow"));
	APlayerController* Given = Controller.Get();
	if (Given && !bTaken)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Given->SetInputMode(Mode);
	}
	// (the viewport holds it; once taken away, nothing does)
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(AsShared());
	}
	return EActiveTimerReturnType::Stop;
}
