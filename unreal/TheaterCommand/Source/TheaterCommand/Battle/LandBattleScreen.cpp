#include "Battle/LandBattleScreen.h"

#include "Battle/LandBattle.h"
#include "Battle/LandBattleState.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	// The frame's colours (the UI director's: brass, cream, muted, paper; the desk's leather and ink), as the air
	// battle's screen; slate, the enemy's side of the bar (her ruling: never red).
	const FLinearColor Brass = FLinearColor(FColor::FromHex(TEXT("B08D57")));
	const FLinearColor BrightBrass = FLinearColor(FColor::FromHex(TEXT("E0B872")));
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("EDE6D6")));
	const FLinearColor Muted = FLinearColor(FColor::FromHex(TEXT("A89F8D")));
	const FLinearColor Paper = FLinearColor(FColor::FromHex(TEXT("CDBE9B")));
	const FLinearColor Amber = FLinearColor(FColor::FromHex(TEXT("E0A13C")));
	const FLinearColor Slate = FLinearColor(FColor::FromHex(TEXT("6E7B85")));
	const FLinearColor Plate = FLinearColor(0.02f, 0.015f, 0.01f, 0.8f);
	const FLinearColor DeskInk = FLinearColor(FColor::FromHex(TEXT("2B2118")));
	const FLinearColor InkSoft = FLinearColor(FColor::FromHex(TEXT("5B4A38")));
	const FLinearColor Leather = FLinearColor(FColor::FromHex(TEXT("2A1C12")));
	const FLinearColor Track = FLinearColor(FColor::FromHex(TEXT("5D4C37")));
	/** The woods: their edges on the ground while he deploys, and the forest mark of a unit hidden in them (on paper, the
	 * darker). */
	const FLinearColor Leaf = FLinearColor(FColor::FromHex(TEXT("9CC08A")));     // the kit's --gain
	const FLinearColor LeafInk = FLinearColor(FColor::FromHex(TEXT("3F6B36")));  // the kit's --gain-deep
	/** The kit's --alarm-deep: something being lost (men running), the one thing besides a deadline given red. */
	const FLinearColor Alarm = FLinearColor(FColor::FromHex(TEXT("8C2F22")));

	/** PT Serif from Content/Fonts (SIL OFL 1.1), as the campaign frame's: names and titles. */
	FSlateFontInfo Serif(const TCHAR* Weight, float Size)
	{
		static TSharedPtr<FStandaloneCompositeFont> Face;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			const FString Own = FPaths::ProjectContentDir() / TEXT("Fonts");
			if (FPaths::FileExists(Own / TEXT("PTSerif-Regular.ttf")) && FPaths::FileExists(Own / TEXT("PTSerif-Bold.ttf")))
			{
				Face = MakeShared<FStandaloneCompositeFont>();
				auto Add = [](FStandaloneCompositeFont& Into, const TCHAR* Name, const FString& File)
				{
					FTypefaceEntry& Entry = Into.DefaultTypeface.Fonts.Emplace_GetRef(FName(Name));
					Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				};
				Add(*Face, TEXT("Regular"), Own / TEXT("PTSerif-Regular.ttf"));
				Add(*Face, TEXT("Bold"), Own / TEXT("PTSerif-Bold.ttf"));
				Add(*Face, TEXT("Italic"), FPaths::FileExists(Own / TEXT("PTSerif-Italic.ttf")) ? Own / TEXT("PTSerif-Italic.ttf") : Own / TEXT("PTSerif-Regular.ttf"));
			}
		}
		if (!Face.IsValid())
		{
			return FCoreStyle::GetDefaultFontStyle(FCString::Stricmp(Weight, TEXT("Bold")) == 0 ? TEXT("Bold") : TEXT("Regular"), Size);
		}
		return FSlateFontInfo(Face, Size, FName(Weight));
	}

	/** Courier Prime from Content/Fonts (SIL OFL 1.1): everything a clerk would have typed. */
	FSlateFontInfo Typed(float Size, bool bBold = false, int32 Spacing = 0)
	{
		static TSharedPtr<FStandaloneCompositeFont> Face;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			const FString Own = FPaths::ProjectContentDir() / TEXT("Fonts");
			if (FPaths::FileExists(Own / TEXT("CourierPrime-Regular.ttf")) && FPaths::FileExists(Own / TEXT("CourierPrime-Bold.ttf")))
			{
				Face = MakeShared<FStandaloneCompositeFont>();
				auto Add = [](FTypeface& Into, const TCHAR* Name, const FString& File)
				{
					FTypefaceEntry& Entry = Into.Fonts.Emplace_GetRef(FName(Name));
					Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				};
				Add(Face->DefaultTypeface, TEXT("Regular"), Own / TEXT("CourierPrime-Regular.ttf"));
				Add(Face->DefaultTypeface, TEXT("Bold"), Own / TEXT("CourierPrime-Bold.ttf"));
				const FString Wide = FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
				const FString Plain = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
				const FString Signs = FPaths::FileExists(Wide) ? Wide : Plain;
				if (FPaths::FileExists(Signs))
				{
					Add(Face->FallbackTypeface.Typeface, TEXT("Regular"), Signs);
					Add(Face->FallbackTypeface.Typeface, TEXT("Bold"), Signs);
				}
			}
		}
		FSlateFontInfo Info = Face.IsValid() ? FSlateFontInfo(Face, Size, FName(bBold ? TEXT("Bold") : TEXT("Regular"))) : FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo SmallCaps()
	{
		return Typed(9.0f, true, 40);
	}

	FString Wide(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}
}

void SLandBattleScreen::Construct(const FArguments& Args)
{
	Battle = Args._Battle;
	SetVisibility(EVisibility::Visible);
}

const SLandBattleScreen::FButton* SLandBattleScreen::ButtonAt(const FVector2D& Local) const
{
	for (int32 K = Buttons.Num() - 1; K >= 0; --K)
	{
		if (Buttons[K].Rect.ContainsPoint(Local))
		{
			return &Buttons[K];
		}
	}
	return nullptr;
}

int32 SLandBattleScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Buttons.Reset();
	const ALandBattle* Scene = Battle.Get();
	const FLandBattleState* State = Scene ? Scene->GetState() : nullptr;
	if (!State)
	{
		return LayerId;
	}
	const FLandBattleState& S = *State;
	const LandDesk::Words& W = S.Words;
	static const FSlateColorBrush Solid(FLinearColor::White);
	const FVector2D Size = Geometry.GetLocalSize();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float TextScale = FMath::Max(Geometry.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
	auto WideOf = [&Measure, TextScale](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face, TextScale).X) / TextScale; };
	auto Fill = [&](double X, double Y, double Wd, double Ht, const FLinearColor& Ink, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
	};
	auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(1400.0, 60.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, Face, ESlateDrawEffect::None, Ink);
	};
	auto Centred = [&](const FString& Words, double Middle, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
	{
		Write(Words, Middle - WideOf(Words, Face) * 0.5, Y, Face, Ink, Layer);
	};
	auto Wrap = [&WideOf](const FString& Text, const FSlateFontInfo& Face, double Room) -> TArray<FString>
	{
		TArray<FString> Pieces, Out;
		Text.ParseIntoArray(Pieces, TEXT(" "), true);
		FString Now;
		for (const FString& Piece : Pieces)
		{
			const FString Tried = Now.IsEmpty() ? Piece : Now + TEXT(" ") + Piece;
			if (!Now.IsEmpty() && WideOf(Tried, Face) > Room)
			{
				Out.Add(Now);
				Now = Piece;
			}
			else
			{
				Now = Tried;
			}
		}
		if (!Now.IsEmpty() || Out.Num() == 0)
		{
			Out.Add(Now);
		}
		return Out;
	};
	auto Button = [&](const FString& Label, const FString& Action, double X, double Y, bool bLit, int32 Layer) -> double
	{
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		const double Wd = WideOf(Label, Face) + 22.0, Ht = 26.0;
		Fill(X, Y, Wd, Ht, Brass, Layer);
		Fill(X + 1.0, Y + 1.0, Wd - 2.0, Ht - 2.0, bLit ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather, Layer + 1);
		Write(Label, X + 11.0, Y + 5.0, Face, bLit ? DeskInk : Cream, Layer + 2);
		Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + Wd), static_cast<float>(Y + Ht)), Action });
		return X + Wd + 6.0;
	};
	// A flag, three by two, with a hairline of brass round it.
	auto Flag = [&](int32 Side, double X, double Y, double Wd, double Ht, int32 Layer)
	{
		if (!bFlagMade[Side])
		{
			if (UTexture2D* Cloth = Scene->FlagOf(Side))
			{
				FlagBrush[Side].SetResourceObject(Cloth);
				FlagBrush[Side].ImageSize = FVector2D(96.0f, 64.0f);
				FlagBrush[Side].DrawAs = ESlateBrushDrawType::Image;
				bFlagMade[Side] = true;
			}
		}
		Fill(X - 1.0, Y - 1.0, Wd + 2.0, Ht + 2.0, Brass, Layer);
		if (bFlagMade[Side])
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer + 1, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &FlagBrush[Side], ESlateDrawEffect::None, FLinearColor::White);
		}
		else
		{
			Fill(X, Y, Wd, Ht, Side == 0 ? Brass : Slate, Layer + 1);
		}
	};

	// ---- The general's screen (a live battle of his own): tags, routes, cards and order bar under the rest, which is
	// laid above it (its layers after his); the dispatches and the note rise above his cards. ----
	const bool bGeneral = IsGeneral();
	double CardsH = 0.0;
	if (bGeneral)
	{
		CardsH = PaintGeneral(Geometry, OutDrawElements, LayerId);
		LayerId += 18;
	}
	else
	{
		// (away from his screen, the film or a card up: no plate left over for when he comes back, till the pointer moves)
		HoverBlock = -1;
		TagBoxes.Reset();
	}

	// ---- The top bar: the battle's name and place at the left, the clock at the right, the bar in the middle. ----
	const double BarH = 66.0;
	Fill(0.0, 0.0, Size.X, BarH, Leather, 0);
	Fill(0.0, BarH, Size.X, 1.0, Brass, 1);
	Write(Wide(W.Title), 22.0, 9.0, Serif(TEXT("Bold"), 16.0f), Cream, 2);
	Write(Wide(W.Place.empty() ? W.Date : W.Place + ", " + W.Date), 22.0, 36.0, Typed(10.0f), Muted, 2);
	{
		const FString Clock = Wide(W.Clock);
		const FSlateFontInfo ClockFace = Typed(16.0f, true);
		Write(Clock, Size.X - 22.0 - WideOf(Clock, ClockFace), 12.0, ClockFace, Cream, 2);
		const FString Pace = S.SpeedNow <= 0.0 ? FString(TEXT("HELD")) : FString::Printf(TEXT("x%s"), *FString::SanitizeFloat(S.SpeedNow, 0));
		const FSlateFontInfo PaceFace = SmallCaps();
		Write(Pace, Size.X - 22.0 - WideOf(Pace, PaceFace), 38.0, PaceFace, S.SpeedNow <= 0.0 ? Amber : Muted, 2);
	}
	{
		// The tug of war: his colour from his flag, the enemy's from theirs, meeting at the knot with the card's small
		// gap; a pale mark at the middle (level). No ticks, no words.
		const double Mid = Size.X * 0.5, BW = 380.0, BX = Mid - BW * 0.5, BY = 12.0, BH = 10.0;
		const double FlagW = 27.0, FlagH = 18.0;
		Flag(0, BX - FlagW - 10.0, BY + BH * 0.5 - FlagH * 0.5, FlagW, FlagH, 2);
		Flag(1, BX + BW + 10.0, BY + BH * 0.5 - FlagH * 0.5, FlagW, FlagH, 2);
		const double Knot = BW * FMath::Clamp(W.Knot, 0.0, 1.0);
		const double Gap = 2.0;
		Fill(BX, BY, FMath::Max(0.0, Knot - Gap * 0.5), BH, Brass, 3);
		Fill(BX + Knot + Gap * 0.5, BY, FMath::Max(0.0, BW - Knot - Gap * 0.5), BH, Slate, 3);
		Fill(Mid - 1.0, BY - 3.0, 2.0, BH + 6.0, Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f), 4);
		// HOW NEAR TO BREAKING OFF: one bar a side, no numbers; his own amber past three quarters.
		const double HY = BY + 22.0, HalfW = 150.0;
		Centred(TEXT("HOW NEAR TO BREAKING OFF"), Mid, HY + 8.0, SmallCaps(), Muted, 2);
		Fill(BX, HY, HalfW, 6.0, Track, 2);
		Fill(BX + HalfW * (1.0 - FMath::Clamp(W.BreakOwn, 0.0, 1.0)), HY, HalfW * FMath::Clamp(W.BreakOwn, 0.0, 1.0), 6.0, W.BreakOwn >= 0.75 ? Amber : Cream, 3);
		Fill(BX + BW - HalfW, HY, HalfW, 6.0, Track, 2);
		Fill(BX + BW - HalfW, HY, HalfW * FMath::Clamp(W.BreakEnemy, 0.0, 1.0), 6.0, Paper, 3);
	}

	// ---- The dispatches: bottom left, the newest last, fading as they age. A long one wraps short of the strip's buttons
	// (the second cut's after card: the breaking line ran off the screen's right edge); none under the after card, which
	// says it all. ----
	if (!S.bCard)
	{
		const FSlateFontInfo Face = Typed(10.0f);
		const double Room = FMath::Clamp(Size.X * 0.5, 320.0, 760.0);
		TArray<TArray<FString>> Pieces;
		int32 Rows = 0;
		for (const LandDesk::Line& L : W.Lines)
		{
			Pieces.Add(Wrap(Wide(L.Text), Face, Room));
			Rows += Pieces.Last().Num();
		}
		double DY = Size.Y - 60.0 - CardsH - 20.0 * Rows;
		for (int32 K = 0; K < Pieces.Num(); ++K)
		{
			const LandDesk::Line& L = W.Lines[static_cast<size_t>(K)];
			const float Fade = static_cast<float>(FMath::Clamp(1.0 - (L.Age - 0.7) / 0.3, 0.0, 1.0));
			for (const FString& Text : Pieces[K])
			{
				if (Fade > 0.0f)
				{
					const FLinearColor Ink = (L.Side == 0 ? Cream : Paper) * FLinearColor(1.0f, 1.0f, 1.0f, Fade);
					Fill(18.0, DY - 2.0, WideOf(Text, Face) + 12.0, 19.0, FLinearColor(0.02f, 0.015f, 0.01f, 0.5f * Fade), 2);
					Write(Text, 24.0, DY, Face, Ink, 3);
				}
				DY += 20.0;
			}
		}
	}

	// ---- A short note in the middle (the shot held, the camera let go). ----
	if (!S.Note.empty())
	{
		const FString Said = Wide(S.Note);
		const FSlateFontInfo SaidFace = Typed(10.5f, true);
		const double X = Size.X * 0.5 - WideOf(Said, SaidFace) * 0.5;
		Fill(X - 6.0, Size.Y - 94.0 - CardsH, WideOf(Said, SaidFace) + 12.0, 20.0, Plate, 6);
		Write(Said, X, Size.Y - 92.0 - CardsH, SaidFace, BrightBrass, 7);
	}

	// ---- The strip, bottom right: the speeds, a new shot, this shot held (and, the film over a battle of his, the map
	// again). The general's own bar stands there instead while his map is up. ----
	if (!S.bCard && !bGeneral)
	{
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		const TCHAR* Labels[] = { TEXT("II"), TEXT("x1"), TEXT("x2"), TEXT("x4"), TEXT("NEW SHOT"), TEXT("HOLD SHOT"), TEXT("MAP  ESC") };
		const TCHAR* Actions[] = { TEXT("speed:0"), TEXT("speed:1"), TEXT("speed:2"), TEXT("speed:4"), TEXT("cut"), TEXT("hold"), TEXT("general:map") };
		const int32 Count = S.General.bOn ? 7 : 6;
		double Whole = 0.0;
		for (int32 K = 0; K < Count; ++K)
		{
			Whole += WideOf(Labels[K], Face) + 28.0;
		}
		double X = Size.X - 22.0 - Whole;
		const double Y = Size.Y - 44.0;
		for (int32 K = 0; K < Count; ++K)
		{
			const bool bLit = (K == 0 && S.SpeedNow <= 0.0) || (K >= 1 && K <= 3 && !S.bOwnSpeed && FMath::IsNearlyEqual(S.Speed, K == 1 ? 1.0 : K == 2 ? 2.0 : 4.0))
				|| (K == 5 && Scene->IsShotHeld());
			X = Button(Labels[K], Actions[K], X, Y, bLit, 8);
		}
	}

	// ---- The after card: the result's stamp, the battle's name, its lines, CONTINUE. ----
	if (S.bCard)
	{
		const FSlateFontInfo TitleFace = Serif(TEXT("Bold"), 21.0f), LineFace = Typed(10.0f);
		const FString Stamp = Wide(W.Level);
		const FSlateFontInfo StampFace = Typed(12.0f, true, 60);
		const double SW = WideOf(Stamp, StampFace) + 18.0;
		const FString Title = Wide(S.Rec.Title);
		double Longest = WideOf(Title, TitleFace) + 16.0 + SW;
		for (const std::string& L : W.After)
		{
			Longest = FMath::Max(Longest, WideOf(Wide(L), LineFace));
		}
		const double CW = FMath::Clamp(Longest + 52.0, 660.0, FMath::Max(660.0, Size.X - 80.0));
		const double Room = CW - 52.0;
		const TArray<FString> TitleLines = Wrap(Title, TitleFace, Room - 16.0 - SW);
		TArray<FString> Laid;
		for (const std::string& L : W.After)
		{
			Laid.Append(Wrap(Wide(L), LineFace, Room));
		}
		const double CH = 18.0 + 16.0 + 28.0 * TitleLines.Num() + 12.0 + 18.0 + 18.0 * Laid.Num() + 58.0;
		const double CX = (Size.X - CW) * 0.5, CY = FMath::Max(BarH + 20.0, (Size.Y - CH) * 0.5);
		Fill(0.0, 0.0, Size.X, Size.Y, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 20);
		Fill(CX, CY, CW, CH, Paper, 21);
		double CY2 = CY + 18.0;
		Write(TEXT("BATTLE RESULT"), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 16.0;
		for (const FString& Piece : TitleLines)
		{
			Write(Piece, CX + 26.0, CY2, TitleFace, DeskInk, 22);
			CY2 += 28.0;
		}
		// (the result in the clerk's ink, not red: red is a signal to act, and a result is news that has arrived; the
		// art director on cut 4, by rulings 149(g) and 160)
		if (!Stamp.IsEmpty())
		{
			Fill(CX + CW - 26.0 - SW, CY + 18.0, SW, 26.0, DeskInk, 22);
			Fill(CX + CW - 26.0 - SW + 2.0, CY + 20.0, SW - 4.0, 22.0, Paper, 23);
			Write(Stamp, CX + CW - 26.0 - SW + 9.0, CY + 22.0, StampFace, DeskInk, 24);
		}
		CY2 += 12.0;
		Write(TEXT("ON THE FIELD"), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 18.0;
		for (const FString& Each : Laid)
		{
			Write(Each, CX + 26.0, CY2, LineFace, DeskInk, 22);
			CY2 += 18.0;
		}
		Button(TEXT("CONTINUE"), TEXT("continue"), CX + CW - 26.0 - 110.0, CY + CH - 40.0, true, 25);
	}
	return LayerId + 26;
}

// ---- The general's screen (a live battle of his own) ---------------------------------------------------------

bool SLandBattleScreen::IsGeneral() const
{
	const ALandBattle* Scene = Battle.Get();
	const FLandBattleState* S = Scene ? Scene->GetState() : nullptr;
	return S && S->General.bOn && !S->General.bFilm && !S->bCard;
}

FVector2D SLandBattleScreen::ToPixels(const FGeometry& Geometry, const FVector2D& Local) const
{
	const ALandBattle* Scene = Battle.Get();
	const FLandBattleState* S = Scene ? Scene->GetState() : nullptr;
	const FVector2D Size = Geometry.GetLocalSize();
	if (!S || Size.X <= 0.0 || Size.Y <= 0.0 || S->General.ViewW <= 0.0)
	{
		return Local;
	}
	return FVector2D(Local.X * S->General.ViewW / Size.X, Local.Y * S->General.ViewH / Size.Y);
}

FVector2D SLandBattleScreen::ToLocal(const FGeometry& Geometry, const FVector2D& Pixels) const
{
	const ALandBattle* Scene = Battle.Get();
	const FLandBattleState* S = Scene ? Scene->GetState() : nullptr;
	const FVector2D Size = Geometry.GetLocalSize();
	if (!S || S->General.ViewW <= 0.0 || S->General.ViewH <= 0.0)
	{
		return Pixels;
	}
	return FVector2D(Pixels.X * Size.X / S->General.ViewW, Pixels.Y * Size.Y / S->General.ViewH);
}

int32 SLandBattleScreen::TagUnder(const FVector2D& Local) const
{
	for (int32 K = TagBoxes.Num() - 1; K >= 0; --K)
	{
		if (TagBoxes[K].Rect.ContainsPoint(Local))
		{
			return TagBoxes[K].Block;
		}
	}
	return -1;
}

double SLandBattleScreen::PaintGeneral(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	TagBoxes.Reset();
	const ALandBattle* Scene = Battle.Get();
	const FLandBattleState* State = Scene ? Scene->GetState() : nullptr;
	if (!State)
	{
		return 0.0;
	}
	const FLandBattleState& S = *State;
	const FLandGeneralShown& G = S.General;
	const LandCommand::General& Clip = G.Clipboard;
	static const FSlateColorBrush Solid(FLinearColor::White);
	const FVector2D Size = Geometry.GetLocalSize();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float TextScale = FMath::Max(Geometry.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
	auto WideOf = [&Measure, TextScale](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face, TextScale).X) / TextScale; };
	auto Fill = [&](double X, double Y, double Wd, double Ht, const FLinearColor& Ink, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
	};
	auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(1400.0, 60.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, Face, ESlateDrawEffect::None, Ink);
	};
	auto Lines = [&](const TArray<FVector2D>& Points, const FLinearColor& Ink, float Thick, int32 Layer)
	{
		if (Points.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, Thick);
		}
	};
	auto Local = [&](double Px, double Py) { return ToLocal(Geometry, FVector2D(Px, Py)); };
	// A button of the order bar: its word and, where a key does the same, the key's cap after it (the UI director's
	// floor: nothing under 11 at the reference, 10 as Brian sees it at 1,441).
	const FSlateFontInfo BarFace = Typed(11.0f, true, 30), CapFace = Typed(11.0f, true, 0);
	auto ButtonWidth = [&](const FString& Label, const FString& Key)
	{
		return WideOf(Label, BarFace) + 22.0 + (Key.IsEmpty() ? 0.0 : WideOf(Key, CapFace) + 18.0);
	};
	auto Button = [&](const FString& Label, const FString& Key, const FString& Action, double X, double Y, bool bLit, bool bDim, int32 Layer) -> double
	{
		const double Wd = ButtonWidth(Label, Key), Ht = 28.0;
		const FLinearColor Ink = bLit ? DeskInk : bDim ? Muted : Cream;
		Fill(X, Y, Wd, Ht, bDim ? Track : Brass, Layer);
		Fill(X + 1.0, Y + 1.0, Wd - 2.0, Ht - 2.0, bLit ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather, Layer + 1);
		const double LabelW = WideOf(Label, BarFace);
		Write(Label, X + 11.0, Y + 5.0, BarFace, Ink, Layer + 2);
		if (!Key.IsEmpty())
		{
			// (the key's cap: a small outlined box with the key's name)
			const double CapX = X + 11.0 + LabelW + 8.0, CapW = WideOf(Key, CapFace) + 10.0;
			Fill(CapX, Y + 4.0, CapW, Ht - 8.0, Ink * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), Layer + 2);
			Fill(CapX + 1.0, Y + 5.0, CapW - 2.0, Ht - 10.0, bLit ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather, Layer + 3);
			Write(Key, CapX + 5.0, Y + 5.0, CapFace, Ink, Layer + 4);
		}
		Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + Wd), static_cast<float>(Y + Ht)), Action });
		return X + Wd + 6.0;
	};
	// The forest mark, the kit's stroke sign for "hidden in the woods" (the UI director, 22 November 05:22Z): two firs in a
	// 24 box, a 1.6 stroke, no fill; Box the box's side on the screen, from its top left corner (X, Y).
	auto Forest = [&](double X, double Y, double Box, const FLinearColor& Ink, int32 Layer)
	{
		const double K = Box / 24.0;
		const float Stroke = static_cast<float>(FMath::Max(1.2, 1.6 * K));
		auto At = [&](double Px, double Py) { return FVector2D(X + Px * K, Y + Py * K); };
		Lines({ At(9, 3), At(4, 10), At(7, 10), At(3, 16), At(15, 16), At(11, 10), At(14, 10), At(9, 3) }, Ink, Stroke, Layer);
		Lines({ At(9, 16), At(9, 21) }, Ink, Stroke, Layer);
		Lines({ At(17, 7), At(14, 12), At(16, 12), At(13, 17), At(21, 17), At(18, 12), At(20, 12), At(17, 7) }, Ink, Stroke, Layer);
		Lines({ At(17, 17), At(17, 21) }, Ink, Stroke, Layer);
	};
	auto KindWord = [](const std::string& Kind)
	{
		return Kind == "inf" ? TEXT("RIFLES") : Kind == "arm" ? TEXT("TANKS") : Kind == "art" ? TEXT("GUNS") : Kind == "mot" ? TEXT("MOTORISED") : Kind == "eng" ? TEXT("ENGINEERS")
			: Kind == "mtn" ? TEXT("MOUNTAIN") : Kind == "cav" ? TEXT("CAVALRY") : Kind == "fort" ? TEXT("WORKS") : TEXT("");
	};
	// A chip's words and colours: his staff's, his own, on the air (with the seconds where the views say them).
	auto ChipOf = [](const LandCommand::Tag& T, FString& OutWords, FLinearColor& OutPaper, FLinearColor& OutInk)
	{
		OutWords.Reset();
		if (T.State == "routing") { OutWords = TEXT("RUNNING"); OutPaper = Alarm; OutInk = Cream; return; }
		switch (T.Says)
		{
		case LandCommand::Chip::Staff: OutWords = TEXT("STAFF"); OutPaper = Track; OutInk = Cream; break;
		case LandCommand::Chip::Yours: OutWords = TEXT("YOURS"); OutPaper = Brass; OutInk = DeskInk; break;
		case LandCommand::Chip::Radio: OutWords = T.OrderIn > 0.0 ? FString::Printf(TEXT("RADIO %.0fs"), FMath::CeilToDouble(T.OrderIn)) : FString(TEXT("RADIO")); OutPaper = Track; OutInk = Cream; break;
		default: break;
		}
	};

	// ---- The woods' edges near his ground, while he deploys: a unit set down inside starts hidden. ----
	for (size_t K = 0; K + 1 < G.WoodsEdge.size(); K += 2)
	{
		Lines({ Local(G.WoodsEdge[K].first, G.WoodsEdge[K].second), Local(G.WoodsEdge[K + 1].first, G.WoodsEdge[K + 1].second) }, Leaf * FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), 1.5f, 1);
	}

	// ---- His ground, while he stands his blocks on it: a dashed brass edge along the ground. ----
	if (G.RoomEdge.size() >= 2)
	{
		for (size_t K = 0; K + 1 < G.RoomEdge.size(); K += 2)
		{
			Lines({ Local(G.RoomEdge[K].first, G.RoomEdge[K].second), Local(G.RoomEdge[K + 1].first, G.RoomEdge[K + 1].second) }, BrightBrass * FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), 2.0f, 1);
		}
	}

	// ---- The routes of his orders: brass, bright for those in hand; dashed and faint while still on the air, solid and
	// bright once it has landed. ----
	for (const FLandGeneralShown::Route& R : G.Routes)
	{
		const bool bAir = R.Says == LandCommand::Chip::Radio;
		const FLinearColor Ink = R.bBack ? Muted : bAir ? Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.5f) : R.bChosen ? BrightBrass : Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.75f);
		const float Thick = R.bChosen ? 2.5f : 1.5f;
		TArray<FVector2D> Points;
		for (const std::pair<double, double>& P : R.Points)
		{
			Points.Add(Local(P.first, P.second));
		}
		if (bAir)
		{
			for (int32 K = 0; K + 1 < Points.Num(); K += 2)
			{
				Lines({ Points[K], Points[K + 1] }, Ink, Thick, 2);
			}
		}
		else
		{
			Lines(Points, Ink, Thick, 2);
		}
		// (an attack's head: two short strokes back from its end)
		if (R.bAttack && Points.Num() >= 2)
		{
			const FVector2D End = Points.Last(), Way = (End - Points[Points.Num() - 2]).GetSafeNormal();
			const FVector2D Side(-Way.Y, Way.X);
			Lines({ End - Way * 12.0 + Side * 7.0, End, End - Way * 12.0 - Side * 7.0 }, Ink, Thick + 0.5f, 2);
		}
	}

	// ---- The ring round each block in hand, its stalk and its paper tag. ----
	for (const FLandGeneralShown::Mark& M : G.Marks)
	{
		const LandCommand::Tag* T = Clip.TagOf(M.Block);
		if (!T)
		{
			continue;
		}
		const FVector2D Top = Local(M.X, M.Y), Foot = Local(M.FootX, M.FootY);
		if (M.bCorners)
		{
			TArray<FVector2D> Ring;
			for (int32 K = 0; K <= 4; ++K)
			{
				Ring.Add(Local(M.Corners[(K % 4) * 2], M.Corners[(K % 4) * 2 + 1]));
			}
			Lines(Ring, BrightBrass, 2.0f, 3);
		}
		const bool bFoe = !T->bMine;
		Lines({ Foot, Top }, (bFoe ? Slate : Brass) * FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), 1.5f, 3);
		Fill(Foot.X - 2.0, Foot.Y - 2.0, 4.0, 4.0, bFoe ? Slate : Brass, 3);
		const FSlateFontInfo Face = Typed(11.0f, true, 0);
		const FString Name = !T->bKnown ? FString(TEXT("?")) : Wide(T->Short.empty() ? T->Name : T->Short).ToUpper();
		FString ChipWords;
		FLinearColor ChipPaper = Track, ChipInk = Cream;
		if (!bFoe)
		{
			ChipOf(*T, ChipWords, ChipPaper, ChipInk);
		}
		const FSlateFontInfo ChipFace = Typed(11.0f, true, 0);
		// (his own, hidden in woods from the enemy: the forest mark after its name)
		const bool bWoods = !bFoe && T->bHidden;
		const double TreeW = bWoods ? 20.0 : 0.0;
		const double NameW = WideOf(Name, Face), ChipW = ChipWords.IsEmpty() ? 0.0 : WideOf(ChipWords, ChipFace) + 10.0;
		const double TW = NameW + 12.0 + TreeW + ChipW, TH = 21.0;
		const double TX = Top.X - TW * 0.5, TY = Top.Y - TH;
		if (TY < 70.0)
		{
			continue;
		}
		if (T->bChosen)
		{
			Fill(TX - 2.0, TY - 2.0, TW + 4.0, TH + 4.0 + 4.0, BrightBrass, 4);
		}
		Fill(TX, TY, TW, TH, bFoe ? Slate : Paper, 5);
		Write(Name, TX + 6.0, TY + 2.0, Face, bFoe ? Cream : DeskInk, 6);
		if (bWoods)
		{
			Forest(TX + NameW + 9.0, TY + 1.5, 18.0, LeafInk, 6);
		}
		if (!ChipWords.IsEmpty())
		{
			Fill(TX + NameW + 12.0 + TreeW, TY, ChipW, TH, ChipPaper, 6);
			Write(ChipWords, TX + NameW + 17.0 + TreeW, TY + 2.0, ChipFace, ChipInk, 7);
		}
		// (its strength, a thin bar under the tag: what is left of it)
		if (T->bKnown)
		{
			Fill(TX, TY + TH, TW, 3.0, Leather, 5);
			Fill(TX, TY + TH, TW * FMath::Clamp(T->Str, 0.0, 1.0), 3.0, bFoe ? Cream : T->Mor < 0.4 ? Amber : Brass, 6);
		}
		TagBoxes.Add({ FSlateRect(static_cast<float>(TX - 2.0), static_cast<float>(TY - 2.0), static_cast<float>(TX + TW + 2.0), static_cast<float>(TY + TH + 5.0)), M.Block });
	}

	// ---- His drag: the box, the line and where each would stand on it, a block carried. ----
	if (G.bBox)
	{
		const FVector2D A = Local(G.Box[0], G.Box[1]), B = Local(G.Box[2], G.Box[3]);
		Fill(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y), Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), 8);
		Lines({ A, FVector2D(B.X, A.Y), B, FVector2D(A.X, B.Y), A }, BrightBrass, 1.0f, 9);
	}
	if (G.bLine)
	{
		Lines({ Local(G.LineEnds[0], G.LineEnds[1]), Local(G.LineEnds[2], G.LineEnds[3]) }, BrightBrass, 2.0f, 8);
		for (const std::pair<double, double>& P : G.Ghosts)
		{
			const FVector2D At = Local(P.first, P.second);
			Fill(At.X - 5.0, At.Y - 5.0, 10.0, 10.0, Cream, 9);
		}
	}
	if (G.bCarry)
	{
		TArray<FVector2D> Ring;
		for (int32 K = 0; K <= 4; ++K)
		{
			Ring.Add(Local(G.Corners[(K % 4) * 2], G.Corners[(K % 4) * 2 + 1]));
		}
		Lines(Ring, !G.bCarryOk ? Amber : G.bCarryWoods ? Leaf : Cream, 2.5f, 9);
	}
	// ---- The mark answering an order: a ring opening out where it went, gone in a second and a bit. ----
	if (G.bPingOn)
	{
		const double Age = FMath::Clamp(G.PingAge / 1.2, 0.0, 1.0);
		const FVector2D At = Local(G.PingX, G.PingY);
		const double Radius = 6.0 + 28.0 * Age;
		const FLinearColor Ink = (G.bPingAttack ? Cream : BrightBrass) * FLinearColor(1.0f, 1.0f, 1.0f, static_cast<float>(1.0 - Age));
		TArray<FVector2D> Ring;
		for (int32 K = 0; K <= 20; ++K)
		{
			const double A = 2.0 * UE_DOUBLE_PI * K / 20.0;
			// (flattened, as a ring lying on the ground looks from above and aside)
			Ring.Add(At + FVector2D(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius * 0.55));
		}
		Lines(Ring, Ink, 2.0f, 9);
		Fill(At.X - 2.0, At.Y - 2.0, 4.0, 4.0, Ink, 9);
	}

	// ---- What a click would do, by the pointer. ----
	if (!G.Would.empty() && G.PointerX >= 0.0)
	{
		const FVector2D At = Local(G.PointerX, G.PointerY);
		const FString Words = Wide(G.Would);
		const FSlateFontInfo Face = Typed(11.0f, true, 30);
		Fill(At.X + 16.0, At.Y + 16.0, WideOf(Words, Face) + 12.0, 20.0, Plate, 10);
		Write(Words, At.X + 22.0, At.Y + 17.0, Face, BrightBrass, 11);
	}

	// ---- The setup step's word, under the bar. ----
	if (Clip.Deploying())
	{
		// (the order in capitals; the woods' line a sentence, so the serif in sentence case: the UI director, 05:22Z)
		TArray<FString> Said = { TEXT("DEPLOY YOUR ARMY: DRAG YOUR UNITS ANYWHERE ON YOUR GROUND (THE BRASS LINE), THEN BEGIN THE BATTLE. THE CLOCK WAITS.") };
		if (!G.WoodsField.empty())
		{
			Said.Add(TEXT("Units in the woods, inside the green lines, start hidden from the enemy until it comes close or they open fire."));
		}
		double SY = 76.0;
		for (int32 K = 0; K < Said.Num(); ++K)
		{
			const FSlateFontInfo Face = K == 0 ? Typed(11.0f, true) : Serif(TEXT("Regular"), 13.0f);
			const double WW = WideOf(Said[K], Face);
			Fill(Size.X * 0.5 - WW * 0.5 - 10.0, SY, WW + 20.0, 24.0, Plate, 10);
			Write(Said[K], Size.X * 0.5 - WW * 0.5, SY + 3.0, Face, K == 0 ? BrightBrass : Leaf, 11);
			SY += 26.0;
		}
	}

	// ---- The order bar, bottom right: his orders, the clock's pace, the film. ----
	const bool bHand = !Clip.Chosen().empty();
	const double BarY = Size.Y - 46.0;
	{
		struct FPart { const TCHAR* Label; const TCHAR* Key; const TCHAR* Action; bool bLit; bool bDim; };
		TArray<FPart> Parts;
		if (Clip.Deploying())
		{
			Parts.Add({ TEXT("BEGIN THE BATTLE"), TEXT("SPACE"), TEXT("general:begin"), true, false });
			Parts.Add({ TEXT("FILM"), TEXT("C"), TEXT("general:film"), false, false });
		}
		else
		{
			Parts.Add({ TEXT("AT THE DOUBLE"), TEXT(""), TEXT("general:double"), G.Armed == "double", !bHand });
			Parts.Add({ TEXT("HOLD"), TEXT("H"), TEXT("general:hold"), false, !bHand });
			Parts.Add({ TEXT("FIRE ON"), TEXT(""), TEXT("general:fire"), G.Armed == "fire", !bHand });
			Parts.Add({ TEXT("FALL BACK"), TEXT("R"), TEXT("general:fallback"), false, !bHand });
			Parts.Add({ TEXT("BACK TO STAFF"), TEXT(""), TEXT("general:staff"), false, !bHand });
			Parts.Add({ TEXT("PAUSE"), TEXT("SPACE"), TEXT("speed:0"), S.SpeedNow <= 0.0, false });
			Parts.Add({ TEXT("\u00D71"), TEXT("1"), TEXT("speed:1"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 1.0), false });
			Parts.Add({ TEXT("\u00D72"), TEXT("2"), TEXT("speed:2"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 2.0), false });
			Parts.Add({ TEXT("\u00D74"), TEXT("3"), TEXT("speed:4"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 4.0), false });
			Parts.Add({ TEXT("FILM"), TEXT("C"), TEXT("general:film"), false, false });
		}
		double Whole = 0.0;
		for (const FPart& P : Parts)
		{
			Whole += ButtonWidth(P.Label, P.Key) + 6.0;
		}
		double X = Size.X - 22.0 - Whole;
		for (const FPart& P : Parts)
		{
			X = Button(P.Label, P.Key, P.Action, X, BarY, P.bLit, P.bDim, 12);
		}
	}

	// ---- His units' cards along the bottom, above the bar: a click picks one up, Shift-click adds it. ----
	double CardsH = 0.0;
	{
		const double CW = 178.0, CH = 50.0, Gap = 6.0;
		double X = 18.0, Y = BarY - CH - 10.0;
		const FSlateFontInfo NameFace = Typed(11.0f, true, 0), SmallFace = Typed(11.0f, false, 0), ChipFace = Typed(11.0f, true, 0);
		for (const LandCommand::Tag& T : Clip.Tags())
		{
			if (!T.bMine)
			{
				continue;
			}
			if (X + CW > Size.X - 18.0)
			{
				X = 18.0;
				Y -= CH + Gap;
			}
			const bool bGone = !T.bOrderable;
			if (T.bChosen)
			{
				Fill(X - 2.0, Y - 2.0, CW + 4.0, CH + 4.0, BrightBrass, 12);
			}
			Fill(X, Y, CW, CH, bGone ? Track : Paper, 13);
			Write(Wide(T.Short.empty() ? T.Name : T.Short).ToUpper().Left(T.bHidden ? 16 : 18), X + 6.0, Y + 3.0, NameFace, DeskInk, 14);
			if (T.bHidden)
			{
				// (hidden in woods from the enemy: Brian's forest mark on the tile, 22 November 04:19Z)
				Forest(X + CW - 24.0, Y + 2.0, 20.0, LeafInk, 14);
			}
			FString ChipWords;
			FLinearColor ChipPaper = Track, ChipInk = Cream;
			ChipOf(T, ChipWords, ChipPaper, ChipInk);
			if (bGone && T.Says == LandCommand::Chip::Gone)
			{
				ChipWords = TEXT("GONE");
			}
			Write(KindWord(T.Kind), X + 6.0, Y + 24.0, SmallFace, InkSoft, 14);
			if (!ChipWords.IsEmpty())
			{
				const double ChipW = WideOf(ChipWords, ChipFace) + 10.0;
				Fill(X + CW - ChipW - 4.0, Y + 23.0, ChipW, 19.0, ChipPaper, 14);
				Write(ChipWords, X + CW - ChipW + 1.0, Y + 24.0, ChipFace, ChipInk, 15);
			}
			Fill(X, Y + CH - 4.0, CW, 4.0, Leather, 14);
			Fill(X, Y + CH - 4.0, CW * FMath::Clamp(T.Str, 0.0, 1.0), 4.0, T.Mor < 0.4 ? Amber : Brass, 15);
			if (T.bOrderable)
			{
				Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + CW), static_cast<float>(Y + CH)), FString::Printf(TEXT("pick:%d"), T.Block) });
			}
			CardsH = FMath::Max(CardsH, BarY - Y);
			X += CW + Gap;
		}
	}

	// ---- The plate of the block under the pointer: its whole name, what it belongs to, its real men and the drawing
	// ratio (the historian, 22 November 05:50Z, on Brian's 04:14Z rule: what is on the screen reflects the real numbers,
	// the ratio said where it is not full scale). An enemy not met says nothing. ----
	const LandCommand::Tag* Over = HoverBlock >= 0 && GeneralHeld < 0 ? Clip.TagOf(HoverBlock) : nullptr;
	if (Over && Over->bKnown && (Over->bMine || !Over->bHidden) && !Over->Name.empty())
	{
		const int Men = Over->Men >= 0 ? Over->Men : Over->Men0;
		// (about: to the hundred from a thousand up, else to the ten)
		const int Step = Men >= 1000 ? 100 : 10;
		const int About = FMath::Max(Step, (Men + Step / 2) / Step * Step);
		FString Said = Over->Parent.empty() ? FString() : TEXT("of the ") + Wide(Over->Parent);
		if (Men > 0)
		{
			Said += (Said.IsEmpty() ? TEXT("about ") : TEXT(", about ")) + FText::AsNumber(About).ToString() + TEXT(" men");
		}
		if (Over->Ratio > 0)
		{
			// (1:20 as written, never "x20", which reads as a multiplier on the men: the historian)
			Said += FString::Printf(TEXT(", drawn 1:%d (one figure, %d men)"), Over->Ratio, Over->Ratio);
		}
		const FString Named = Wide(Over->Name).ToUpper();
		const FSlateFontInfo NameFace = Typed(11.0f, true, 30), SaidFace = Serif(TEXT("Regular"), 13.0f);
		const double PW = FMath::Max(WideOf(Named, NameFace), Said.IsEmpty() ? 0.0 : WideOf(Said, SaidFace)) + 20.0;
		const double PH = Said.IsEmpty() ? 26.0 : 48.0;
		const double PX = FMath::Clamp(HoverAt.X + 16.0, 8.0, FMath::Max(8.0, Size.X - PW - 8.0));
		const double PY = FMath::Clamp(HoverAt.Y - PH - 12.0, 8.0, FMath::Max(8.0, Size.Y - PH - 8.0));
		Fill(PX, PY, PW, PH, Plate, 16);
		Write(Named, PX + 10.0, PY + 5.0, NameFace, Over->bMine ? BrightBrass : Cream, 17);
		if (!Said.IsEmpty())
		{
			Write(Said, PX + 10.0, PY + 24.0, SaidFace, Cream, 17);
		}
	}
	return CardsH;
}

void SLandBattleScreen::Do(const FString& Action)
{
	ALandBattle* Scene = Battle.Get();
	if (!Scene)
	{
		return;
	}
	if (Action.StartsWith(TEXT("speed:")))
	{
		const double Speed = FCString::Atod(*Action.Mid(6));
		if (Speed > 0.0)
		{
			SpeedBefore = Speed;
		}
		Scene->SetSpeed(Speed);
	}
	else if (Action.StartsWith(TEXT("general:")))
	{
		Scene->GeneralDo(Action.Mid(8));
	}
	else if (Action.StartsWith(TEXT("pick:")))
	{
		// (a unit's card: picked up, or added to those in hand with Shift)
		if (FLandBattleState* S = Scene->EditState())
		{
			S->General.Clipboard.Pick(FCString::Atoi(*Action.Mid(5)), FSlateApplication::Get().GetModifierKeys().IsShiftDown());
		}
	}
	else
	{
		Scene->Press(Action);
	}
}

FReply SLandBattleScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (const FButton* Hit = ButtonAt(Local))
	{
		Do(Hit->Action);
		return FReply::Handled();
	}
	if (IsGeneral())
	{
		// His map: the left button picks up and boxes, the right gives orders and draws lines; both his, not the camera's.
		const FKey Which = Event.GetEffectingButton();
		const int32 Button = Which == EKeys::LeftMouseButton ? 0 : Which == EKeys::RightMouseButton ? 1 : -1;
		if (Button >= 0 && GeneralHeld < 0)
		{
			GeneralHeld = Button;
			Battle->GeneralPress(ToPixels(Geometry, Local), Button, Event.IsShiftDown(), TagUnder(Local));
			// (capture asked again while it is held would release it first, and its loss let this press go: the 3D
			// pre-check, 22 November 05:29Z)
			return HasMouseCapture() ? FReply::Handled() : FReply::Handled().CaptureMouse(SharedThis(this));
		}
		if (Which == EKeys::MiddleMouseButton && GeneralHeld < 0)
		{
			// (the middle button drags the ground under it, as a two-finger slide)
			bSliding = true;
			return HasMouseCapture() ? FReply::Handled() : FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Handled();
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SLandBattleScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bSliding && Event.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bSliding = false;
		return GeneralHeld >= 0 ? FReply::Handled() : FReply::Handled().ReleaseMouseCapture();
	}
	if (GeneralHeld >= 0)
	{
		const FKey Which = Event.GetEffectingButton();
		const int32 Button = Which == EKeys::LeftMouseButton ? 0 : Which == EKeys::RightMouseButton ? 1 : -1;
		if (Button != GeneralHeld)
		{
			return FReply::Handled();
		}
		GeneralHeld = -1;
		if (Battle.IsValid())
		{
			const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
			Battle->GeneralRelease(ToPixels(Geometry, Local), Button, Event.IsShiftDown(), TagUnder(Local));
		}
		// (the middle button still sliding keeps the capture)
		return bSliding ? FReply::Handled() : FReply::Handled().ReleaseMouseCapture();
	}
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bDragging)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void SLandBattleScreen::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// (the window lost the pointer mid-drag: his drag is let go where it was, nothing given)
	if (GeneralHeld >= 0 && Battle.IsValid())
	{
		Battle->GeneralDo(TEXT("letgo"));
	}
	GeneralHeld = -1;
	bDragging = false;
	bSliding = false;
}

FReply SLandBattleScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// The second press of a double click is a press like any other (his clicks count their own seconds: a second click
	// where an order went sends them at the double, one on open ground glides the view there); a unit's card clicked
	// twice is also followed by the view.
	if (IsGeneral() && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FButton* Hit = ButtonAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		if (Hit && Hit->Action.StartsWith(TEXT("pick:")) && Battle.IsValid())
		{
			Battle->GeneralDo(TEXT("follow:") + Hit->Action.Mid(5));
		}
	}
	return OnMouseButtonDown(Geometry, Event);
}

FReply SLandBattleScreen::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	FLandBattleState* S = IsGeneral() && Battle.IsValid() ? Battle->EditState() : nullptr;
	if (!S)
	{
		return FReply::Unhandled();
	}
	// (a notch nearer on the wheel turned away from him)
	S->GeneralIn.Zoom += Event.GetWheelDelta();
	return FReply::Handled();
}

FReply SLandBattleScreen::OnTouchGesture(const FGeometry& Geometry, const FPointerEvent& Event)
{
	FLandBattleState* S = IsGeneral() && Battle.IsValid() ? Battle->EditState() : nullptr;
	if (!S)
	{
		return FReply::Unhandled();
	}
	// A trackpad: two fingers sliding drag the ground under them (the view goes the other way); a pinch zooms (about
	// a notch a tenth of the fingers' spread); a twist turns the view. Handled here, the slide is not also a wheel.
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Delta = Event.GetGestureDelta();
	switch (Event.GetGestureType())
	{
	case EGestureEvent::Scroll:
		if (Size.X > 0.0 && Size.Y > 0.0)
		{
			S->GeneralIn.SlideRight -= Delta.X / Size.X;
			S->GeneralIn.SlideAhead += Delta.Y / Size.Y;
		}
		return FReply::Handled();
	case EGestureEvent::Magnify:
		S->GeneralIn.Zoom += Delta.X * 10.0;
		return FReply::Handled();
	case EGestureEvent::Rotate:
		S->GeneralIn.Turn += Delta.X;
		return FReply::Handled();
	default:
		return FReply::Unhandled();
	}
}

void SLandBattleScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	HoverBlock = -1;
	if (FLandBattleState* S = Battle.IsValid() ? Battle->EditState() : nullptr)
	{
		S->GeneralIn.EdgeRight = S->GeneralIn.EdgeAhead = 0.0;
	}
}

void SLandBattleScreen::HandViewKeys()
{
	FLandBattleState* S = Battle.IsValid() ? Battle->EditState() : nullptr;
	if (!S)
	{
		return;
	}
	auto On = [this](uint32 Bit) { return (ViewKeys & Bit) != 0 ? 1.0 : 0.0; };
	S->GeneralIn.PanRight = On(2u) - On(1u);
	S->GeneralIn.PanAhead = On(4u) - On(8u);
	S->GeneralTurnHeld = On(32u) - On(16u);
}

void SLandBattleScreen::OnFocusLost(const FFocusEvent& Event)
{
	SLeafWidget::OnFocusLost(Event);
	ViewKeys = 0;
	HandViewKeys();
}

FReply SLandBattleScreen::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	const uint32 Bit = Key == EKeys::A || Key == EKeys::Left ? 1u : Key == EKeys::D || Key == EKeys::Right ? 2u : Key == EKeys::W || Key == EKeys::Up ? 4u
		: Key == EKeys::S || Key == EKeys::Down ? 8u : Key == EKeys::Q ? 16u : Key == EKeys::E ? 32u : 0u;
	if (Bit == 0u)
	{
		return FReply::Unhandled();
	}
	ViewKeys &= ~Bit;
	HandViewKeys();
	return FReply::Handled();
}

FReply SLandBattleScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!Battle.IsValid())
	{
		return FReply::Unhandled();
	}
	if (IsGeneral())
	{
		const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		const FVector2D Size = Geometry.GetLocalSize();
		if (FLandBattleState* S = Battle->EditState())
		{
			LandGeneralEye::Input& In = S->GeneralIn;
			if (bSliding && Size.X > 0.0 && Size.Y > 0.0)
			{
				In.SlideRight -= Event.GetCursorDelta().X / Size.X;
				In.SlideAhead += Event.GetCursorDelta().Y / Size.Y;
			}
			// (the pointer at the very edge of the screen moves the view that way; not while he drags a box or a unit)
			const double Edge = 4.0;
			In.EdgeRight = Local.X >= Size.X - Edge ? 1.0 : Local.X <= Edge ? -1.0 : 0.0;
			In.EdgeAhead = Local.Y <= Edge ? 1.0 : Local.Y >= Size.Y - Edge ? -1.0 : 0.0;
			In.bLockEdgePan = GeneralHeld >= 0;
		}
		// (the block under the pointer, a tag or a card, for its plate)
		HoverAt = Local;
		HoverBlock = TagUnder(Local);
		if (const FButton* Over = HoverBlock < 0 ? ButtonAt(Local) : nullptr)
		{
			HoverBlock = Over->Action.StartsWith(TEXT("pick:")) ? FCString::Atoi(*Over->Action.Mid(5)) : -1;
		}
		// (over his bar or a card, no word by the pointer)
		Battle->GeneralMove(GeneralHeld < 0 && ButtonAt(Local) ? FVector2D(-1.0, -1.0) : ToPixels(Geometry, Local), TagUnder(Local));
		return FReply::Handled();
	}
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	Battle->Look(Event.GetCursorDelta());
	return FReply::Handled();
}

FReply SLandBattleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	ALandBattle* Scene = Battle.Get();
	const FLandBattleState* S = Scene ? Scene->GetState() : nullptr;
	if (!S)
	{
		return FReply::Unhandled();
	}
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Enter && S->bCard)
	{
		Scene->Press(TEXT("continue"));
		return FReply::Handled();
	}
	if (S->General.bOn && !S->bCard)
	{
		const bool bSetup = S->General.Clipboard.Deploying();
		// His keys. In the setup step any pace begins the battle (the clock waits there).
		auto Pace = [&](double Speed)
		{
			if (bSetup)
			{
				Scene->GeneralDo(TEXT("begin"));
			}
			Do(FString::Printf(TEXT("speed:%g"), Speed));
		};
		const uint32 Bit = Key == EKeys::A || Key == EKeys::Left ? 1u : Key == EKeys::D || Key == EKeys::Right ? 2u : Key == EKeys::W || Key == EKeys::Up ? 4u
			: Key == EKeys::S || Key == EKeys::Down ? 8u : Key == EKeys::Q ? 16u : Key == EKeys::E ? 32u : 0u;
		if (S->General.bFilm)
		{
			if (Key == EKeys::Escape || Key == EKeys::C) { Scene->GeneralDo(TEXT("map")); return FReply::Handled(); }
		}
		else if (Bit != 0u)
		{
			// (his view's keys, held: the scene moves the view while they are down)
			ViewKeys |= Bit;
			HandViewKeys();
			return FReply::Handled();
		}
		else if (Key == EKeys::SpaceBar)
		{
			if (bSetup)
			{
				Pace(1.0);
			}
			else if (S->SpeedNow > 0.0)
			{
				SpeedBefore = S->SpeedNow;
				Scene->SetSpeed(0.0);
			}
			else
			{
				Scene->SetSpeed(SpeedBefore > 0.0 ? SpeedBefore : 1.0);
			}
			return FReply::Handled();
		}
		else if (Key == EKeys::One) { Pace(1.0); return FReply::Handled(); }
		else if (Key == EKeys::Two) { Pace(2.0); return FReply::Handled(); }
		else if (Key == EKeys::Three) { Pace(4.0); return FReply::Handled(); }
		else if (Key == EKeys::H) { Scene->GeneralDo(TEXT("hold")); return FReply::Handled(); }
		else if (Key == EKeys::R) { Scene->GeneralDo(TEXT("fallback")); return FReply::Handled(); }
		else if (Key == EKeys::Tab) { Scene->GeneralDo(Event.IsShiftDown() ? TEXT("back") : TEXT("next")); return FReply::Handled(); }
		else if (Key == EKeys::Escape) { Scene->GeneralDo(TEXT("clear")); return FReply::Handled(); }
		else if (Key == EKeys::C) { Scene->GeneralDo(TEXT("film")); return FReply::Handled(); }
		else if (Key == EKeys::Enter && bSetup) { Scene->GeneralDo(TEXT("begin")); return FReply::Handled(); }
		else
		{
			return FReply::Unhandled();
		}
	}
	if (Key == EKeys::SpaceBar)
	{
		if (S->SpeedNow > 0.0)
		{
			SpeedBefore = S->SpeedNow;
			Scene->SetSpeed(0.0);
		}
		else
		{
			Scene->SetSpeed(SpeedBefore > 0.0 ? SpeedBefore : 1.0);
		}
	}
	else if (Key == EKeys::C) { Do(TEXT("cut")); }
	else if (Key == EKeys::H) { Do(TEXT("hold")); }
	else if (Key == EKeys::One) { Do(TEXT("speed:0.5")); }
	else if (Key == EKeys::Two) { Do(TEXT("speed:1")); }
	else if (Key == EKeys::Three) { Do(TEXT("speed:2")); }
	else if (Key == EKeys::Four) { Do(TEXT("speed:4")); }
	else
	{
		return FReply::Unhandled();
	}
	return FReply::Handled();
}
