#include "Battle/AirBattleScreen.h"

#include "Battle/AirBattle.h"
#include "Battle/AirBattleState.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include <algorithm>

namespace
{
	// The frame's colours (the UI director's: brass, cream, muted, paper, alarm; the desk's leather and ink).
	const FLinearColor Brass = FLinearColor(FColor::FromHex(TEXT("B08D57")));
	const FLinearColor BrightBrass = FLinearColor(FColor::FromHex(TEXT("E0B872")));
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("EDE6D6")));
	const FLinearColor Muted = FLinearColor(FColor::FromHex(TEXT("A89F8D")));
	const FLinearColor Paper = FLinearColor(FColor::FromHex(TEXT("CDBE9B")));
	const FLinearColor Alarm = FLinearColor(FColor::FromHex(TEXT("E08A6A")));
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
				// (the signs the typewriter has no key for, × and ½ among them, from the engine's own face)
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

void SAirBattleScreen::Construct(const FArguments& Args)
{
	Battle = Args._Battle;
	SetVisibility(EVisibility::Visible);
}

FVector2D SAirBattleScreen::ToPixels(const FGeometry& Geometry, const FVector2D& Local) const
{
	const FAirBattleState* S = Battle.IsValid() ? Battle->GetState() : nullptr;
	const FVector2D Size = Geometry.GetLocalSize();
	if (!S || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return Local;
	}
	return FVector2D(Local.X * S->ViewportPixels.X / Size.X, Local.Y * S->ViewportPixels.Y / Size.Y);
}

FVector2D SAirBattleScreen::ToLocal(const FGeometry& Geometry, const FVector2D& Pixels) const
{
	const FAirBattleState* S = Battle.IsValid() ? Battle->GetState() : nullptr;
	const FVector2D Size = Geometry.GetLocalSize();
	if (!S || S->ViewportPixels.X <= 0.0 || S->ViewportPixels.Y <= 0.0)
	{
		return Pixels;
	}
	return FVector2D(Pixels.X * Size.X / S->ViewportPixels.X, Pixels.Y * Size.Y / S->ViewportPixels.Y);
}

const SAirBattleScreen::FButton* SAirBattleScreen::ButtonAt(const FVector2D& Local) const
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

int32 SAirBattleScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Buttons.Reset();
	const FAirBattleState* State = Battle.IsValid() ? Battle->GetState() : nullptr;
	if (!State)
	{
		return LayerId;
	}
	const FAirBattleState& S = *State;
	const AirDesk::Words& W = S.Words;
	static const FSlateColorBrush Solid(FLinearColor::White);
	const FVector2D Size = Geometry.GetLocalSize();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto WideOf = [&Measure](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face).X); };
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
	auto Line = [&](const TArray<FVector2D>& Points, const FLinearColor& Ink, float Thick, int32 Layer)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, Thick);
	};
	// A button of the strip: a brass-edged plate, lit where it is the choice in force; dimmed where it cannot be given.
	auto Button = [&](const FString& Label, const FString& Action, double X, double Y, bool bLit, bool bLive, int32 Layer) -> double
	{
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		const double Wd = WideOf(Label, Face) + 22.0, Ht = 26.0;
		Fill(X, Y, Wd, Ht, bLive ? Brass : Track, Layer);
		Fill(X + 1.0, Y + 1.0, Wd - 2.0, Ht - 2.0, bLit ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather, Layer + 1);
		Write(Label, X + 11.0, Y + 5.0, Face, bLit ? DeskInk : bLive ? Cream : Muted, Layer + 2);
		if (bLive)
		{
			Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + Wd), static_cast<float>(Y + Ht)), Action });
		}
		return X + Wd + 6.0;
	};

	// ---- The top bar (land's form, b384): the sides, the chance, the balance, the bars, the clock. ----
	const double BarH = 66.0;
	Fill(0.0, 0.0, Size.X, BarH, Leather, 0);
	Fill(0.0, BarH, Size.X, 1.0, Brass, 1);
	Write(Wide(W.MySide), 22.0, 9.0, Serif(TEXT("Bold"), 17.0f), Cream, 2);
	Write(TEXT("YOURS"), 22.0, 36.0, SmallCaps(), Brass, 2);
	Write(Wide(W.Chance), 22.0 + 96.0, 38.0, Typed(10.0f), Muted, 2);
	{
		const FString Theirs = Wide(W.TheirSide);
		const FSlateFontInfo Face = Serif(TEXT("Bold"), 17.0f);
		Write(Theirs, Size.X - 22.0 - WideOf(Theirs, Face), 9.0, Face, Paper, 2);
		const FString Clock = Wide(W.Clock);
		const FSlateFontInfo ClockFace = Typed(16.0f, true);
		Write(Clock, Size.X - 22.0 - WideOf(Clock, ClockFace), 36.0, ClockFace, Cream, 2);
	}
	{
		// The balance: his from the left, a pale mark where it stood at the start.
		const double Mid = Size.X * 0.5, BW = 380.0, BX = Mid - BW * 0.5, BY = 12.0;
		Fill(BX, BY, BW, 10.0, Track, 2);
		Fill(BX, BY, BW * FMath::Clamp(W.Balance, 0.0, 1.0), 10.0, Brass, 3);
		Fill(BX + BW * FMath::Clamp(W.BalanceStart, 0.0, 1.0) - 1.0, BY - 3.0, 2.0, 16.0, Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f), 4);
		// HOW NEAR TO BREAKING OFF: one bar a side, no numbers; his own amber past three quarters.
		const double HY = BY + 22.0, HalfW = 150.0;
		Centred(TEXT("HOW NEAR TO BREAKING OFF"), Mid, HY + 8.0, SmallCaps(), Muted, 2);
		Fill(BX, HY, HalfW, 6.0, Track, 2);
		Fill(BX + HalfW * (1.0 - FMath::Clamp(W.MyBar, 0.0, 1.0)), HY, HalfW * FMath::Clamp(W.MyBar, 0.0, 1.0), 6.0, W.MyBar >= 0.75 ? Alarm : Cream, 3);
		Fill(BX + BW - HalfW, HY, HalfW, 6.0, Track, 2);
		Fill(BX + BW - HalfW, HY, HalfW * FMath::Clamp(W.TheirBar, 0.0, 1.0), 6.0, Paper, 3);
	}

	// ---- The field's top left: the clock's tag, the kicker, the aim slip, the near line. ----
	double Y = BarH + 12.0;
	if (!S.ClockTag.empty())
	{
		Write(Wide(S.ClockTag), 22.0, Y, Typed(10.0f, true, 30), BrightBrass, 2);
		Y += 20.0;
	}
	if (!W.Kicker.empty())
	{
		Write(Wide(W.Kicker), 22.0, Y, SmallCaps(), Muted, 2);
		Y += 18.0;
	}
	{
		// THE AIM (b397): what the raid is for, how it stands, the aircraft lost; on a slip of paper.
		const double SlipW = 330.0, SlipH = 84.0;
		Fill(22.0, Y, SlipW, SlipH, Paper, 2);
		Write(TEXT("THE AIM"), 32.0, Y + 7.0, SmallCaps(), InkSoft, 3);
		Write(Wide(W.AimTarget), 32.0, Y + 21.0, Serif(TEXT("Bold"), 13.0f), DeskInk, 3);
		Write(Wide(W.AimState), 32.0, Y + 44.0, Typed(9.5f), DeskInk, 3);
		Write(Wide(W.AimLost), 32.0, Y + 60.0, Typed(9.5f), InkSoft, 3);
		Y += SlipH + 10.0;
	}
	if (!W.NearLine.empty())
	{
		Write(Wide(W.NearLine), 22.0, Y, Typed(11.0f, true), Alarm, 2);
		Y += 22.0;
	}

	// ---- A tag over each formation: his own in cream, the enemy's in muted paper (never red); in hand, brass. ----
	for (const AirDesk::Tag& T : W.Tags)
	{
		const auto Found = S.TagAt.find(T.Id);
		if (Found == S.TagAt.end())
		{
			continue;
		}
		const FVector2D At = ToLocal(Geometry, Found->second);
		const bool bHand = std::find(S.Hand.begin(), S.Hand.end(), T.Id) != S.Hand.end();
		const FLinearColor Ink = bHand ? BrightBrass : T.bMine ? Cream : Paper * FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
		const FString Name = Wide(T.Name);
		const FString Under = Wide(T.Count) + (T.Doing.empty() ? FString() : TEXT(", ") + Wide(T.Doing));
		const FSlateFontInfo NameFace = Typed(10.0f, true), UnderFace = Typed(8.5f);
		const double Wd = FMath::Max(WideOf(Name, NameFace), WideOf(Under, UnderFace)) + 12.0;
		Fill(At.X - Wd * 0.5, At.Y - 30.0, Wd, 30.0, FLinearColor(0.02f, 0.015f, 0.01f, 0.55f), 4);
		if (bHand)
		{
			Fill(At.X - Wd * 0.5, At.Y - 1.0, Wd, 2.0, BrightBrass, 5);
		}
		Write(Name, At.X - Wd * 0.5 + 6.0, At.Y - 29.0, NameFace, Ink, 5);
		Write(Under, At.X - Wd * 0.5 + 6.0, At.Y - 15.0, UnderFace, Muted, 5);
		Line({ FVector2D(At.X, At.Y), FVector2D(At.X, At.Y + 10.0) }, Ink * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 1.0f, 4);
	}

	// ---- The dispatches: bottom left, newest last, fading as they age. ----
	{
		double DY = Size.Y - 70.0 - 20.0 * W.Dispatches.size();
		for (const AirDesk::Dispatch& D : W.Dispatches)
		{
			const float Fade = static_cast<float>(FMath::Clamp(1.0 - (D.Age - 10.0) / 4.0, 0.0, 1.0));
			if (Fade <= 0.0f)
			{
				DY += 20.0;
				continue;
			}
			const FLinearColor Ink = (D.bMine ? Cream : Paper) * FLinearColor(1.0f, 1.0f, 1.0f, Fade);
			const FString Text = Wide(D.Text);
			const FSlateFontInfo Face = Typed(10.0f);
			Fill(18.0, DY - 2.0, WideOf(Text, Face) + 12.0, 19.0, FLinearColor(0.02f, 0.015f, 0.01f, 0.5f * Fade), 2);
			Write(Text, 24.0, DY, Face, Ink, 3);
			DY += 20.0;
		}
	}
	if (!S.Note.empty())
	{
		Centred(Wide(S.Note), Size.X * 0.5, Size.Y - 92.0, Typed(10.5f, true), Alarm, 6);
	}

	// ---- The orders strip (b384(e)): only what the groups in hand can take; the speed; the close view. ----
	const bool bLive = !S.bReplay;
	{
		const double SY = Size.Y - 44.0;
		Fill(0.0, SY - 10.0, Size.X, 54.0, FLinearColor(Leather.R, Leather.G, Leather.B, 0.92f), 1);
		Fill(0.0, SY - 10.0, Size.X, 1.0, Brass, 2);
		double X = 22.0;
		const bool bAny = !S.Hand.empty();
		const bool bBombers = S.HandHas(false), bFighters = S.HandHas(true);
		X = Button(TEXT("HOLD"), TEXT("hold"), X, SY, false, bLive && bAny, 3);
		X = Button(TEXT("BREAK OFF"), TEXT("breakoff"), X, SY, false, bLive && bAny, 3);
		if (bBombers || bFighters)
		{
			X += 10.0;
			Write(TEXT("HEIGHT"), X, SY + 7.0, SmallCaps(), Muted, 3);
			X += 58.0;
			X = Button(TEXT("LOW"), TEXT("low"), X, SY, false, bLive, 3);
			X = Button(TEXT("MIDDLE"), TEXT("middle"), X, SY, false, bLive, 3);
			X = Button(TEXT("HIGH"), TEXT("high"), X, SY, false, bLive, 3);
		}
		if (bBombers)
		{
			X += 10.0;
			X = Button(TEXT("JETTISON THE BOMBS"), TEXT("jettison"), X, SY, false, bLive, 3);
		}
		if (bFighters)
		{
			X += 10.0;
			X = Button(TEXT("STAY CLOSE"), TEXT("close"), X, SY, false, bLive, 3);
			X = Button(TEXT("RANGE AHEAD"), TEXT("ahead"), X, SY, false, bLive, 3);
			X = Button(TEXT("HUNT"), TEXT("hunt"), X, SY, false, bLive, 3);
		}
		X += 10.0;
		Button(S.bClose ? TEXT("STRATEGY VIEW") : TEXT("CLOSE VIEW"), TEXT("ride"), X, SY, S.bClose, true, 3);
		// The right: the speeds, BREAK OFF ALL and HAND IT TO THE STAFF (the one button that ends his part).
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		struct FSpeed { const TCHAR* Label; double Speed; const TCHAR* Action; };
		const FSpeed Speeds[] = { { TEXT("HELD"), 0.0, TEXT("speed:0") }, { TEXT("×½"), 0.5, TEXT("speed:0.5") }, { TEXT("×1"), 1.0, TEXT("speed:1") },
			{ TEXT("×2"), 2.0, TEXT("speed:2") }, { TEXT("×4"), 4.0, TEXT("speed:4") } };
		double RightW = WideOf(TEXT("HAND IT TO THE STAFF"), Face) + 22.0 + 6.0 + WideOf(TEXT("BREAK OFF ALL"), Face) + 22.0 + 18.0;
		for (const FSpeed& Each : Speeds)
		{
			RightW += WideOf(Each.Label, Face) + 28.0;
		}
		double RX = Size.X - 22.0 - RightW;
		for (const FSpeed& Each : Speeds)
		{
			RX = Button(Each.Label, Each.Action, RX, SY, FMath::IsNearlyEqual(S.Clock.Speed, Each.Speed), true, 3);
		}
		RX += 12.0;
		RX = Button(TEXT("BREAK OFF ALL"), TEXT("breakoffall"), RX, SY, false, bLive, 3);
		Button(TEXT("HAND IT TO THE STAFF"), TEXT("staff"), RX, SY, false, bLive, 3);
	}

	// ---- The staff's question, asked once, in place; the clock stands while it does. ----
	if (S.Question != 0)
	{
		const FString Question = Wide(S.Question == 1 ? S.Desk.StaffQuestion() : AirDesk::Desk::BreakOffAllQuestion());
		const double QW = 620.0, QH = 132.0, QX = (Size.X - QW) * 0.5, QY = Size.Y * 0.42 - QH * 0.5;
		Fill(QX - 1.0, QY - 1.0, QW + 2.0, QH + 2.0, Brass, 10);
		Fill(QX, QY, QW, QH, Leather, 11);
		Write(Question, QX + 22.0, QY + 20.0, Serif(TEXT("Regular"), 13.0f), Cream, 12);
		double BX = QX + 22.0;
		BX = Button(S.Question == 1 ? TEXT("HAND IT OVER") : TEXT("BREAK OFF ALL"), TEXT("yes"), BX, QY + QH - 44.0, false, true, 12);
		Button(TEXT("KEEP FIGHTING"), TEXT("no"), BX, QY + QH - 44.0, true, true, 12);
	}

	// ---- The after card (the UI director, 18:45Z and 18:50Z): RAID RESULT, the title, IN THE AIR, the lines, the stamp,
	// the rows, the reckoning, CONTINUE. ----
	if (S.bCard)
	{
		const AirDesk::Card& C = S.Card;
		const double CW = 660.0, CH = 150.0 + 18.0 * (C.Lines.size() + C.Rows.size()), CX = (Size.X - CW) * 0.5, CY = FMath::Max(BarH + 20.0, (Size.Y - CH) * 0.5);
		Fill(0.0, 0.0, Size.X, Size.Y, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 20);
		Fill(CX, CY, CW, CH, Paper, 21);
		double CY2 = CY + 18.0;
		Write(Wide(C.Kicker), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 16.0;
		Write(Wide(C.Title), CX + 26.0, CY2, Serif(TEXT("Bold"), 21.0f), DeskInk, 22);
		{
			const FString Stamp = Wide(C.Level);
			const FSlateFontInfo Face = Typed(12.0f, true, 60);
			const double SW = WideOf(Stamp, Face) + 18.0;
			Fill(CX + CW - 26.0 - SW, CY + 18.0, SW, 26.0, InkRed, 22);
			Fill(CX + CW - 26.0 - SW + 2.0, CY + 20.0, SW - 4.0, 22.0, Paper, 23);
			Write(Stamp, CX + CW - 26.0 - SW + 9.0, CY + 22.0, Face, InkRed, 24);
		}
		CY2 += 40.0;
		Write(TEXT("IN THE AIR"), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 18.0;
		for (const std::string& L : C.Lines)
		{
			Write(Wide(L), CX + 26.0, CY2, Typed(10.0f), DeskInk, 22);
			CY2 += 18.0;
		}
		for (const AirDesk::Card::Row& R : C.Rows)
		{
			Write(Wide(R.Name) + TEXT(": ") + Wide(R.Words), CX + 26.0, CY2, Typed(10.0f, R.bMine), R.bMine ? DeskInk : InkSoft, 22);
			CY2 += 18.0;
		}
		if (!C.Reckoning.empty())
		{
			Write(Wide(C.Reckoning), CX + 26.0, CY2, Serif(TEXT("Italic"), 11.0f), InkSoft, 22);
		}
		Button(TEXT("CONTINUE"), TEXT("continue"), CX + CW - 26.0 - 110.0, CY + CH - 40.0, true, true, 25);
	}
	return LayerId + 26;
}

void SAirBattleScreen::Do(const FString& Action)
{
	if (!Battle.IsValid())
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
		Battle->SetSpeed(Speed);
	}
	else if (Action == TEXT("yes") || Action == TEXT("no"))
	{
		Battle->Answer(Action == TEXT("yes"));
	}
	else
	{
		Battle->Press(Action);
	}
}

FReply SAirBattleScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (const FButton* Hit = ButtonAt(Local))
	{
		Do(Hit->Action);
		return FReply::Handled();
	}
	if (!Battle.IsValid())
	{
		return FReply::Unhandled();
	}
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Battle->OrderAt(ToPixels(Geometry, Local));
		return FReply::Handled();
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bLeftDown = true;
		bDragging = false;
		PressedAt = Local;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SAirBattleScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bLeftDown)
	{
		return FReply::Unhandled();
	}
	bLeftDown = false;
	if (!bDragging && Battle.IsValid())
	{
		Battle->PickAt(ToPixels(Geometry, Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())), Event.IsShiftDown());
	}
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SAirBattleScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bLeftDown || !Battle.IsValid())
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (!bDragging && FVector2D::Distance(Local, PressedAt) > 4.0)
	{
		bDragging = true;
	}
	if (bDragging)
	{
		Battle->Orbit(Event.GetCursorDelta());
	}
	return FReply::Handled();
}

FReply SAirBattleScreen::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Battle.IsValid())
	{
		Battle->Zoom(Event.GetWheelDelta());
	}
	return FReply::Handled();
}

FReply SAirBattleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!Battle.IsValid())
	{
		return FReply::Unhandled();
	}
	const FAirBattleState* S = Battle->GetState();
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape)
	{
		if (S && S->Question != 0)
		{
			Battle->Answer(false);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Enter && S && (S->bCard || S->Question != 0))
	{
		if (S->bCard)
		{
			Battle->Press(TEXT("continue"));
		}
		else
		{
			Battle->Answer(false); // (KEEP FIGHTING is the default)
		}
		return FReply::Handled();
	}
	if (Key == EKeys::H) { Battle->Press(TEXT("hold")); }
	else if (Key == EKeys::R) { Battle->Press(TEXT("breakoff")); }
	else if (Key == EKeys::C) { Battle->Press(TEXT("ride")); }
	else if (Key == EKeys::SpaceBar)
	{
		if (S && S->Clock.Speed > 0.0)
		{
			SpeedBefore = S->Clock.Speed;
			Battle->SetSpeed(0.0);
		}
		else
		{
			Battle->SetSpeed(SpeedBefore > 0.0 ? SpeedBefore : 1.0);
		}
	}
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
