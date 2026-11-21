#include "Battle/SeaBattleScreen.h"

#include "Battle/SeaBattle.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

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
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SSeaBattleScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace SeaScreenParts;
	double Knot = 0.5;
	UTexture2D* Mine = nullptr;
	UTexture2D* Theirs = nullptr;
	if (!Battle.IsValid() || !Battle->GetBar(Knot, Mine, Theirs))
	{
		return LayerId;
	}
	static const FSlateColorBrush Solid(FLinearColor::White);
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
	return LayerId + 3;
}
