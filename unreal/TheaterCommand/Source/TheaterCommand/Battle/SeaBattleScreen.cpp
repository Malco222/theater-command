#include "Battle/SeaBattleScreen.h"

#include "Battle/SeaBattle.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
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

	/** The bar and its flags, in the screen's own units (the window's height makes them larger, as the air screen's). */
	constexpr double BarWide = 400.0, BarHigh = 8.0, FlagWide = 27.0, FlagHigh = 18.0, FlagGap = 10.0, Top = 16.0;
	/** The card's small gap where the two colours meet (the knot), and how near an end the knot is ever drawn. */
	constexpr double KnotGap = 3.0, KnotRoom = 0.02;

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
				for (int32 K = 0; K < Rows.Num(); ++K)
				{
					Write(Rows[K], (Size.X - WideOf(Rows[K], Face)) * 0.5, Top0 + K * RowHigh, Face, Ink, 1);
				}
			}
		}
		const FString Back = TEXT("Any key or click returns to the map");
		Write(Back, Size.X - WideOf(Back, Small) - 24.0, Size.Y - 34.0, Small, Pale * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f), 1);
		Over = LayerId + 2;
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
