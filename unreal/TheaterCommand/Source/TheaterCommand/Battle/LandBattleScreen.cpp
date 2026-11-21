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
	const FLinearColor InkRed = FLinearColor(FColor::FromHex(TEXT("8E2C20")));
	const FLinearColor Leather = FLinearColor(FColor::FromHex(TEXT("2A1C12")));
	const FLinearColor Track = FLinearColor(FColor::FromHex(TEXT("5D4C37")));

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

	// ---- The dispatches: bottom left, the newest last, fading as they age. ----
	{
		const FSlateFontInfo Face = Typed(10.0f);
		double DY = Size.Y - 60.0 - 20.0 * W.Lines.size();
		for (const LandDesk::Line& L : W.Lines)
		{
			const float Fade = static_cast<float>(FMath::Clamp(1.0 - (L.Age - 0.7) / 0.3, 0.0, 1.0));
			if (Fade <= 0.0f)
			{
				DY += 20.0;
				continue;
			}
			const FLinearColor Ink = (L.Side == 0 ? Cream : Paper) * FLinearColor(1.0f, 1.0f, 1.0f, Fade);
			const FString Text = Wide(L.Text);
			Fill(18.0, DY - 2.0, WideOf(Text, Face) + 12.0, 19.0, FLinearColor(0.02f, 0.015f, 0.01f, 0.5f * Fade), 2);
			Write(Text, 24.0, DY, Face, Ink, 3);
			DY += 20.0;
		}
	}

	// ---- A short note in the middle (the shot held, the camera let go). ----
	if (!S.Note.empty())
	{
		const FString Said = Wide(S.Note);
		const FSlateFontInfo SaidFace = Typed(10.5f, true);
		const double X = Size.X * 0.5 - WideOf(Said, SaidFace) * 0.5;
		Fill(X - 6.0, Size.Y - 94.0, WideOf(Said, SaidFace) + 12.0, 20.0, Plate, 6);
		Write(Said, X, Size.Y - 92.0, SaidFace, BrightBrass, 7);
	}

	// ---- The strip, bottom right: the speeds, a new shot, this shot held. ----
	if (!S.bCard)
	{
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		const TCHAR* Labels[] = { TEXT("II"), TEXT("x1"), TEXT("x2"), TEXT("x4"), TEXT("NEW SHOT"), TEXT("HOLD SHOT") };
		const TCHAR* Actions[] = { TEXT("speed:0"), TEXT("speed:1"), TEXT("speed:2"), TEXT("speed:4"), TEXT("cut"), TEXT("hold") };
		double Whole = 0.0;
		for (const TCHAR* Label : Labels)
		{
			Whole += WideOf(Label, Face) + 28.0;
		}
		double X = Size.X - 22.0 - Whole;
		const double Y = Size.Y - 44.0;
		for (int32 K = 0; K < 6; ++K)
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
		if (!Stamp.IsEmpty())
		{
			Fill(CX + CW - 26.0 - SW, CY + 18.0, SW, 26.0, InkRed, 22);
			Fill(CX + CW - 26.0 - SW + 2.0, CY + 20.0, SW - 4.0, 22.0, Paper, 23);
			Write(Stamp, CX + CW - 26.0 - SW + 9.0, CY + 22.0, StampFace, InkRed, 24);
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
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SLandBattleScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bDragging)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SLandBattleScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bDragging || !Battle.IsValid())
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
	}
	else if (Key == EKeys::SpaceBar)
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
