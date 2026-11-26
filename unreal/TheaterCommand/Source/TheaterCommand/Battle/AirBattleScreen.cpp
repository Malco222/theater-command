#include "Battle/AirBattleScreen.h"

#include "Battle/AirBattle.h"
#include "Battle/AirBattleState.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

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
	const FLinearColor Amber = FLinearColor(FColor::FromHex(TEXT("E0A13C"))); // (the campaign frame's amber)
	const FLinearColor Plate = FLinearColor(0.02f, 0.015f, 0.01f, 0.8f);      // the dark plate under words on the sky (0.6 left amber at 2:1 over a bright cloud)
	const FLinearColor DeskInk = FLinearColor(FColor::FromHex(TEXT("2B2118")));
	const FLinearColor InkSoft = FLinearColor(FColor::FromHex(TEXT("5B4A38")));
	const FLinearColor InkRed = FLinearColor(FColor::FromHex(TEXT("8E2C20")));
	const FLinearColor Leather = FLinearColor(FColor::FromHex(TEXT("2A1C12")));
	const FLinearColor Track = FLinearColor(FColor::FromHex(TEXT("5D4C37")));
	// The tug-of-war bar's two colours, held all battle and never swapped, neither ever red (the UI director, 21 November
	// 14:47Z, "as in the picture"): his the brass gold, the enemy's slate.
	const FLinearColor BarGold = FLinearColor(FColor::FromHex(TEXT("C9A24A")));
	const FLinearColor BarSlate = FLinearColor(FColor::FromHex(TEXT("6F7B88")));

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
	// (measured at the scale the words are drawn at, as the engine's own text blocks measure: each letter's advance is
	// whole pixels at that scale, some 5 per cent wider at 1600 by 900 than at the scale of 1)
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
	auto Line = [&](const TArray<FVector2D>& Points, const FLinearColor& Ink, float Thick, int32 Layer)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, Thick);
	};
	// Words broken at spaces to fit a width (a single word longer than the width stands on its own line).
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
	// A line of words on the sky, on a dark plate so that it reads over any cloud; returns how far right it reaches.
	auto OnPlate = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, double High, int32 Layer) -> double
	{
		const double Wd = WideOf(Words, Face) + 12.0;
		Fill(X - 6.0, Y - 2.0, Wd, High, Plate, Layer);
		Write(Words, X, Y, Face, Ink, Layer + 1);
		return X - 6.0 + Wd;
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

	// ---- (1.5.1) The opening held: all black with the raid's title while the scene just made is taken in behind it
	// (AAirBattle::WarmTick), then eased off over the flyover's first half second. ----
	const float CurtainA = static_cast<float>(FMath::Clamp(S.Curtain, 0.0, 1.0));
	auto Curtain = [&](int32 Layer)
	{
		Fill(0.0, 0.0, Size.X, Size.Y, FLinearColor(0.0f, 0.0f, 0.0f, CurtainA), Layer);
		Centred(Wide(W.AimTarget), Size.X * 0.5, Size.Y * 0.5 - 26.0, Serif(TEXT("Bold"), 20.0f), FLinearColor(Cream.R, Cream.G, Cream.B, Cream.A * CurtainA), Layer + 1);
		Centred(Wide(S.ClockTag), Size.X * 0.5, Size.Y * 0.5 + 12.0, Typed(13.0f, true, 30), FLinearColor(Muted.R, Muted.G, Muted.B, Muted.A * CurtainA), Layer + 1);
	};
	if (CurtainA >= 0.999f || (CurtainA > 0.0f && S.FlyBars <= 0.0))
	{
		Curtain(0);
		return LayerId + 2;
	}

	// ---- The flyover before the battle (Brian's, approved 19 November 2026): the picture in a film's frame, black above
	// and below, the raid's aim as its title and how to take command; nothing else until the frame has drawn back. ----
	if (S.FlyBars > 0.0)
	{
		const double Band = FMath::RoundToDouble(Size.Y * 0.11 * FMath::Clamp(S.FlyBars, 0.0, 1.0));
		Fill(0.0, 0.0, Size.X, Band, FLinearColor::Black, 0);
		Fill(0.0, Size.Y - Band, Size.X, Band, FLinearColor::Black, 0);
		if (S.FlyBars >= 0.999)
		{
			Centred(Wide(W.AimTarget), Size.X * 0.5, Size.Y - Band + Band * 0.18, Serif(TEXT("Bold"), 20.0f), Cream, 1);
			// (the art director on build 1750: the line under the title 1.3 times its first size, its capitals some 21
			// pixels at the Mac's 1786; like every word on this screen it grows with the window)
			Centred(Wide(S.ClockTag), Size.X * 0.5, Size.Y - Band + Band * 0.58, Typed(13.0f, true, 30), Muted, 1);
		}
		if (CurtainA > 0.0f)
		{
			Curtain(2);
			return LayerId + 4;
		}
		return LayerId + 2;
	}

	// ---- The top bar (land's form, b384): the sides, the chance, the tug-of-war bar, the bars, the clock. ----
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
		// THE TUG-OF-WAR BAR (Brian, 21 November 05:55Z: "have each sides flag on either end of the bar and then have it be
		// like a tug of war in real time so you can see which side is winning"; the UI director's ruling, 14:47Z: "the
		// battle bar is the release 1 card's bar, moving"). His colour runs from his flag on the left, the enemy's from
		// theirs on the right, and they meet at the knot with the card's small gap, so the side that is winning holds
		// more of the bar. A pale mark at the middle; no ticks, no handle, no words. The flags are the card's own, 27 by
		// 18, 8 clear of the bar.
		const double Mid = Size.X * 0.5, BW = 380.0, BX = Mid - BW * 0.5, BY = 12.0;
		const double Meet = BX + BW * FMath::Clamp(W.Knot, 0.02, 0.98), Gap = 3.0;
		Fill(BX, BY, FMath::Max(0.0, Meet - Gap * 0.5 - BX), 10.0, BarGold, 3);
		Fill(Meet + Gap * 0.5, BY, FMath::Max(0.0, BX + BW - Meet - Gap * 0.5), 10.0, BarSlate, 3);
		Fill(Mid - 1.0, BY - 3.0, 2.0, 16.0, Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f), 4);
		auto FlagOf = [this](const std::string& Nat) -> const FSlateBrush*
		{
			const FString Tag = Wide(Nat);
			if (Tag.IsEmpty() || !Battle.IsValid())
			{
				return nullptr;
			}
			if (const TSharedPtr<FSlateBrush>* Found = FlagBrushes.Find(Tag))
			{
				return Found->Get();
			}
			UTexture2D* Picture = Battle->FlagPicture(Tag);
			if (!Picture)
			{
				return nullptr; // (no table yet: asked again next frame)
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Picture);
			Brush->ImageSize = FVector2D(96.0f, 64.0f);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			FlagBrushes.Add(Tag, Brush);
			return Brush.Get();
		};
		auto DrawFlag = [&](const std::string& Nat, double X)
		{
			const FVector2D FlagSize(27.0, 18.0), At(X, BY + 5.0 - 9.0);
			if (const FSlateBrush* Brush = FlagOf(Nat))
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, Geometry.ToPaintGeometry(FlagSize, FSlateLayoutTransform(At)), Brush, ESlateDrawEffect::None, FLinearColor::White);
			}
			else
			{
				Fill(At.X, At.Y, FlagSize.X, FlagSize.Y, Track, 3); // (a plain chip, as the card's for a side with no flag)
			}
		};
		DrawFlag(W.MyNat, BX - 8.0 - 27.0);
		DrawFlag(W.TheirNat, BX + BW + 8.0);
		// HOW NEAR TO BREAKING OFF: one bar a side, no numbers; his own amber past three quarters.
		const double HY = BY + 22.0, HalfW = 150.0;
		Centred(TEXT("HOW NEAR TO BREAKING OFF"), Mid, HY + 8.0, SmallCaps(), Muted, 2);
		Fill(BX, HY, HalfW, 6.0, Track, 2);
		Fill(BX + HalfW * (1.0 - FMath::Clamp(W.MyBar, 0.0, 1.0)), HY, HalfW * FMath::Clamp(W.MyBar, 0.0, 1.0), 6.0, W.MyBar >= 0.75 ? Amber : Cream, 3);
		Fill(BX + BW - HalfW, HY, HalfW, 6.0, Track, 2);
		Fill(BX + BW - HalfW, HY, HalfW * FMath::Clamp(W.TheirBar, 0.0, 1.0), 6.0, Paper, 3);
	}

	// ---- The field's top left: the clock's tag, the kicker, the aim slip, the near line; each on a plate or slip so
	// that it reads over any sky. ----
	double Y = BarH + 12.0;
	double PanelRight = 22.0;
	if (!S.ClockTag.empty())
	{
		PanelRight = FMath::Max(PanelRight, OnPlate(Wide(S.ClockTag), 22.0, Y, Typed(10.0f, true, 30), BrightBrass, 19.0, 2));
		Y += 22.0;
	}
	if (!W.Kicker.empty())
	{
		PanelRight = FMath::Max(PanelRight, OnPlate(Wide(W.Kicker), 22.0, Y, SmallCaps(), Muted, 17.0, 2));
		Y += 20.0;
	}
	{
		// THE AIM (b397): what the raid is for, how it stands, the aircraft lost; on a slip of paper as wide as its
		// longest line (up to a third of the screen, then the words go onto a second line).
		const FSlateFontInfo TargetFace = Serif(TEXT("Bold"), 13.0f), StateFace = Typed(9.5f);
		const FString Target = Wide(W.AimTarget), Standing = Wide(W.AimState), Lost = Wide(W.AimLost);
		const double MostSlip = FMath::Max(330.0, FMath::Min(560.0, Size.X * 0.36));
		const double Longest = FMath::Max3(WideOf(Target, TargetFace), WideOf(Standing, StateFace), WideOf(Lost, StateFace));
		const double SlipW = FMath::Clamp(Longest + 22.0, 330.0, MostSlip);
		const TArray<FString> TargetLines = Wrap(Target, TargetFace, SlipW - 22.0);
		const TArray<FString> StateLines = Wrap(Standing, StateFace, SlipW - 22.0);
		const TArray<FString> LostLines = Wrap(Lost, StateFace, SlipW - 22.0);
		const double SlipH = 21.0 + 21.0 * TargetLines.Num() + 2.0 + 16.0 * (StateLines.Num() + LostLines.Num()) + 8.0;
		Fill(22.0, Y, SlipW, SlipH, Paper, 2);
		Write(TEXT("THE AIM"), 32.0, Y + 7.0, SmallCaps(), InkSoft, 3);
		double SlipY = Y + 21.0;
		for (const FString& Each : TargetLines)
		{
			Write(Each, 32.0, SlipY, TargetFace, DeskInk, 3);
			SlipY += 21.0;
		}
		SlipY += 2.0;
		for (const FString& Each : StateLines)
		{
			Write(Each, 32.0, SlipY, StateFace, DeskInk, 3);
			SlipY += 16.0;
		}
		for (const FString& Each : LostLines)
		{
			Write(Each, 32.0, SlipY, StateFace, InkSoft, 3);
			SlipY += 16.0;
		}
		PanelRight = FMath::Max(PanelRight, 22.0 + SlipW);
		Y += SlipH + 10.0;
	}
	if (!W.NearLine.empty())
	{
		// (amber, as the UI director's handoff has it: a warning that stays, not an alarm)
		PanelRight = FMath::Max(PanelRight, OnPlate(Wide(W.NearLine), 22.0, Y, Typed(11.0f, true), Amber, 20.0, 2));
		Y += 24.0;
	}

	// ---- The chase view (Brian, 21 November 2026 05:52Z: "click on the squadron and then it goes to a view of one of the
	// planes from behind so you can watch it. you can then move from one plane to the next"): top right, on a plate,
	// the squadron ridden and which of its aircraft, and the steps to the next aircraft and the next squadron. A view,
	// not an order: the steps work in a recorded battle too. ----
	double RideL = 0.0, RideT = 0.0, RideR = 0.0, RideB = 0.0;
	if (S.bClose)
	{
		const int Group = S.RiddenGroup();
		const AirDesk::Tag* Ridden = Group >= 0 ? S.TagOf(Group) : nullptr;
		if (Ridden)
		{
			const FSlateFontInfo NameFace = Serif(TEXT("Bold"), 13.0f), CountFace = Typed(9.5f), ButtonFace = Typed(10.0f, true, 30);
			// (his own choice, or the group in hand, is kept to; else the view moves with the fight every ten seconds)
			const bool bChosen = S.RideGroup >= 0 || !S.Hand.empty();
			const FString Kicker = bChosen ? FString(TEXT("RIDING WITH")) : FString(TEXT("RIDING WITH \u00B7 THE VIEW FOLLOWS THE FIGHT"));
			const FString Name = Wide(Ridden->Name);
			FString Count = Wide(AirDesk::RideCount(S.Show.Planes(), Group, S.Subject.Plane));
			if (!Ridden->Doing.empty())
			{
				Count += (Count.IsEmpty() ? TEXT("") : TEXT(" \u00B7 ")) + Wide(Ridden->Doing);
			}
			const FString Hint = TEXT("ARROW KEYS STEP TOO \u00B7 ESC: STRATEGY VIEW");
			const FString Labels[4] = { TEXT("\u2039 AIRCRAFT"), TEXT("AIRCRAFT \u203A"), TEXT("\u2039 SQUADRON"), TEXT("SQUADRON \u203A") };
			const FString Actions[4] = { TEXT("ride:prev"), TEXT("ride:next"), TEXT("ride:up"), TEXT("ride:down") };
			double Pair = 0.0;
			for (int32 K = 0; K < 4; K += 2)
			{
				Pair = FMath::Max(Pair, WideOf(Labels[K], ButtonFace) + 22.0 + 6.0 + WideOf(Labels[K + 1], ButtonFace) + 22.0);
			}
			const double Inner = FMath::Max(FMath::Max(WideOf(Kicker, SmallCaps()), WideOf(Hint, SmallCaps())),
				FMath::Max(FMath::Max(WideOf(Name, NameFace), WideOf(Count, CountFace)), Pair));
			const double PW = Inner + 24.0, PH = 158.0;
			RideR = Size.X - 22.0;
			RideL = RideR - PW;
			RideT = BarH + 12.0;
			RideB = RideT + PH;
			// (the plate near solid and the count line in paper: on the Mac's release 2 check a fighter showed through the
			// plate in Bremen 05c, and the count line read at 3.4 to 3.8 to 1)
			Fill(RideL, RideT, PW, PH, FLinearColor(Plate.R, Plate.G, Plate.B, 0.92f), 6);
			Fill(RideL, RideT, PW, 1.0, Brass, 7);
			Write(Kicker, RideL + 12.0, RideT + 9.0, SmallCaps(), BrightBrass, 7);
			Write(Name, RideL + 12.0, RideT + 23.0, NameFace, Ridden->bMine ? Cream : Paper, 7);
			Write(Count, RideL + 12.0, RideT + 46.0, CountFace, Paper, 7);
			double BX = RideL + 12.0;
			BX = Button(Labels[0], Actions[0], BX, RideT + 66.0, false, true, 7);
			Button(Labels[1], Actions[1], BX, RideT + 66.0, false, true, 7);
			BX = RideL + 12.0;
			BX = Button(Labels[2], Actions[2], BX, RideT + 98.0, false, true, 7);
			Button(Labels[3], Actions[3], BX, RideT + 98.0, false, true, 7);
			Write(Hint, RideL + 12.0, RideT + 136.0, SmallCaps(), Muted, 7);
		}
	}

	// ---- The dispatches' place: bottom left, newest last, fading as they age. In the close view they keep clear of the
	// ridden aircraft (the Mac's release 2 check: they covered it in Bremen, Midway and Oahu 03 and 05): their lines
	// broken to the room left of it; else laid on the right of it; else raised over it. ----
	struct FSentLine
	{
		FString Text;
		float Fade = 1.0f;
		bool bMine = false;
		double X = 0.0, Y = 0.0, Wd = 0.0;
	};
	TArray<FSentLine> SentLines;
	double SentL = 0.0, SentT = 0.0, SentR = 0.0, SentB = 0.0;
	double RiddenL = 0.0, RiddenT = 0.0, RiddenR = 0.0, RiddenB = 0.0;
	const bool bRidden = S.bClose && S.bRiddenOn && !S.bCard;
	if (bRidden)
	{
		const FVector2D Min = ToLocal(Geometry, S.RiddenMin), Max = ToLocal(Geometry, S.RiddenMax);
		RiddenL = Min.X - 10.0;
		RiddenT = Min.Y - 10.0;
		RiddenR = Max.X + 10.0;
		RiddenB = Max.Y + 10.0;
	}
	{
		const FSlateFontInfo Face = Typed(10.0f);
		struct FSaid
		{
			FString Text;
			float Fade = 1.0f;
			bool bMine = false;
		};
		TArray<FSaid> Live;
		for (const AirDesk::Dispatch& D : W.Dispatches)
		{
			const float Fade = static_cast<float>(FMath::Clamp(1.0 - (D.Age - 10.0) / 4.0, 0.0, 1.0));
			if (Fade > 0.0f)
			{
				Live.Add({ Wide(D.Text), Fade, D.bMine });
			}
		}
		// (riding, the last four only, and never narrower than 34 characters of its type: beside the aircraft ridden it
		// broke into a column of a few words a line, the art director on 2b177386's frames, 22 November 04:16Z)
		if (bRidden && Live.Num() > 4)
		{
			Live.RemoveAt(0, Live.Num() - 4);
		}
		const double LeastRoom = WideOf(TEXT("the quick brown fox jumps over a la"), Face) + 12.0;
		// (the lines broken to a width, the newest's top at Newest, against the left edge or the right; the box they fill)
		auto LayOut = [&](double Room, double Newest, bool bRight)
		{
			SentLines.Reset();
			for (const FSaid& One : Live)
			{
				for (const FString& Piece : Wrap(One.Text, Face, Room - 12.0))
				{
					FSentLine Sent;
					Sent.Text = Piece;
					Sent.Fade = One.Fade;
					Sent.bMine = One.bMine;
					Sent.Wd = WideOf(Piece, Face) + 12.0;
					Sent.X = bRight ? Size.X - 18.0 - Sent.Wd : 18.0;
					SentLines.Add(MoveTemp(Sent));
				}
			}
			SentL = Size.X;
			SentR = 0.0;
			double Y = Newest - 20.0 * (SentLines.Num() - 1);
			SentT = Y - 2.0;
			for (FSentLine& Sent : SentLines)
			{
				Sent.Y = Y;
				Y += 20.0;
				SentL = FMath::Min(SentL, Sent.X);
				SentR = FMath::Max(SentR, Sent.X + Sent.Wd);
			}
			SentB = Newest + 17.0;
		};
		auto Covers = [&]()
		{
			return bRidden && SentLines.Num() > 0 && SentL < RiddenR && RiddenL < SentR && SentT < RiddenB && RiddenT < SentB;
		};
		const double Newest = Size.Y - 90.0;
		LayOut(Size.X * 0.5, Newest, false);
		if (Covers() && RiddenL - 18.0 >= LeastRoom)
		{
			LayOut(RiddenL - 18.0, Newest, false);
		}
		if (Covers() && Size.X - 18.0 - RiddenR >= LeastRoom)
		{
			LayOut(Size.X - 18.0 - RiddenR, Newest, true);
		}
		if (Covers())
		{
			// (over it, the oldest lines let go until the rest fit under the top bar's panels)
			const double Over = FMath::Max(Size.X * 0.4, LeastRoom);
			LayOut(Over, RiddenT - 17.0, false);
			while (SentLines.Num() > 0 && SentT < BarH + 170.0)
			{
				Live.RemoveAt(0);
				LayOut(Over, RiddenT - 17.0, false);
			}
		}
	}

	// ---- A tag over each formation: his own in cream, the enemy's in muted paper (never red); in hand, brass. Groups
	// never stack (b384(d)): tags that would touch gather into one block, which shows the two that matter most (his
	// groups in hand, then his own, then the largest) and a line for the rest. No block covers the top bar, the aim
	// slip, the dispatches or the orders strip; one moved out of their way keeps a thin line to its formation. ----
	S.TagBoxes.clear();
	// (none under the after card: their lines ran through its panels on the Mac's release 2 check)
	if (!S.bCard)
	{
		const FSlateFontInfo NameFace = Typed(10.0f, true), UnderFace = Typed(9.0f), MoreFace = Typed(8.5f, true);
		const double TagH = 30.0, TagGap = 3.0, MoreH = 16.0, Margin = 4.0;
		struct FTagRect
		{
			double L = 0.0, T = 0.0, R = 0.0, B = 0.0;
			bool Touches(const FTagRect& Other, double Room) const
			{
				return L < Other.R + Room && Other.L < R + Room && T < Other.B + Room && Other.T < B + Room;
			}
		};
		// The places no tag may cover: the aim panel and the dispatches (the bar and the strip bound the field).
		TArray<FTagRect> Kept;
		Kept.Add({ 16.0, BarH, PanelRight + 2.0, Y });
		if (RideR > RideL)
		{
			Kept.Add({ RideL - 4.0, RideT, RideR + 4.0, RideB + 4.0 });
		}
		if (SentLines.Num() > 0)
		{
			Kept.Add({ SentL, SentT, SentR, SentB });
		}
		if (bRidden)
		{
			Kept.Add({ RiddenL, RiddenT, RiddenR, RiddenB });
		}
		const double FieldT = BarH + 1.0 + Margin, FieldB = Size.Y - 54.0 - Margin;

		struct FTagOne
		{
			const AirDesk::Tag* Tag = nullptr;
			FVector2D At = FVector2D::ZeroVector; // over its formation (local units)
			double Rank = 0.0;                    // the lower, the sooner shown
			double Wd = 0.0;
			FString Name, Under;
			bool bHand = false;
		};
		TArray<FTagOne> Ones;
		for (const AirDesk::Tag& T : W.Tags)
		{
			const auto Found = S.TagAt.find(T.Id);
			if (Found == S.TagAt.end())
			{
				continue;
			}
			FTagOne One;
			One.Tag = &T;
			One.At = ToLocal(Geometry, Found->second);
			const auto InHand = std::find(S.Hand.begin(), S.Hand.end(), T.Id);
			One.bHand = InHand != S.Hand.end();
			const double Aircraft = FMath::Clamp(static_cast<double>(FCString::Atoi(*Wide(T.Count))), 0.0, 999.0);
			One.Rank = One.bHand ? static_cast<double>(InHand - S.Hand.begin()) : (T.bMine ? 1000.0 : 2000.0) + (999.0 - Aircraft);
			One.Name = Wide(T.Name);
			One.Under = Wide(T.Count) + (T.Doing.empty() ? FString() : TEXT(", ") + Wide(T.Doing));
			One.Wd = FMath::Max(WideOf(One.Name, NameFace), WideOf(One.Under, UnderFace)) + 12.0;
			Ones.Add(MoveTemp(One));
		}

		struct FTagBlock
		{
			TArray<int32> Members; // into Ones, the one that matters most first
			FTagRect Box;
			bool bMoved = false;
		};
		// A block stands on its first tag's formation, then steps out of the kept places and inside the field.
		auto Lay = [&](FTagBlock& Block)
		{
			Block.Members.Sort([&Ones](int32 First, int32 Second) { return Ones[First].Rank < Ones[Second].Rank; });
			const FTagOne& Head = Ones[Block.Members[0]];
			const int32 Shown = FMath::Min(2, Block.Members.Num());
			const int32 Folded = Block.Members.Num() - Shown;
			double Wd = 0.0;
			for (int32 K = 0; K < Shown; ++K)
			{
				Wd = FMath::Max(Wd, Ones[Block.Members[K]].Wd);
			}
			if (Folded > 0)
			{
				Wd = FMath::Max(Wd, WideOf(FString::Printf(TEXT("+%d more groups"), Folded), MoreFace) + 12.0);
			}
			const double High = Shown * TagH + (Shown - 1) * TagGap + (Folded > 0 ? TagGap + MoreH : 0.0);
			FTagRect Box{ Head.At.X - Wd * 0.5, Head.At.Y - High, Head.At.X + Wd * 0.5, Head.At.Y };
			auto Shift = [&Box](double Dx, double Dy) { Box.L += Dx; Box.R += Dx; Box.T += Dy; Box.B += Dy; };
			auto Inside = [&Box, &Shift, &Size, FieldT, FieldB]()
			{
				Shift(FMath::Max(0.0, 4.0 - Box.L) - FMath::Max(0.0, Box.R - (Size.X - 4.0)), FMath::Max(0.0, FieldT - Box.T) - FMath::Max(0.0, Box.B - FieldB));
			};
			Inside();
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (const FTagRect& Keep : Kept)
				{
					if (!Box.Touches(Keep, Margin))
					{
						continue;
					}
					// (the smallest step that clears it and stays on the field: down, up, right or left)
					const double Steps[4][2] = { { 0.0, Keep.B + Margin - Box.T }, { 0.0, Keep.T - Margin - Box.B }, { Keep.R + Margin - Box.L, 0.0 }, { Keep.L - Margin - Box.R, 0.0 } };
					double BestStep = 1e18;
					int32 BestWay = -1;
					for (int32 Way = 0; Way < 4; ++Way)
					{
						const FTagRect Tried{ Box.L + Steps[Way][0], Box.T + Steps[Way][1], Box.R + Steps[Way][0], Box.B + Steps[Way][1] };
						const double Step = FMath::Abs(Steps[Way][0]) + FMath::Abs(Steps[Way][1]);
						if (Tried.L >= 4.0 && Tried.R <= Size.X - 4.0 && Tried.T >= FieldT && Tried.B <= FieldB && Step < BestStep)
						{
							BestStep = Step;
							BestWay = Way;
						}
					}
					if (BestWay >= 0)
					{
						Shift(Steps[BestWay][0], Steps[BestWay][1]);
					}
				}
			}
			Block.bMoved = FMath::Abs((Box.L + Box.R) * 0.5 - Head.At.X) > 1.0 || FMath::Abs(Box.B - Head.At.Y) > 1.0;
			Block.Box = Box;
		};
		TArray<FTagBlock> Blocks;
		for (int32 K = 0; K < Ones.Num(); ++K)
		{
			FTagBlock& Block = Blocks.AddDefaulted_GetRef();
			Block.Members.Add(K);
			Lay(Block);
		}
		// Blocks that touch become one, laid again on the one that matters most, until none touch.
		for (bool bJoined = true; bJoined;)
		{
			bJoined = false;
			for (int32 First = 0; First < Blocks.Num() && !bJoined; ++First)
			{
				for (int32 Second = First + 1; Second < Blocks.Num() && !bJoined; ++Second)
				{
					// (each with its stalk, the 10 under it: a stalk must not cross the next block down)
					auto WithStalk = [](const FTagRect& Rect) { FTagRect Out = Rect; Out.B += 10.0; return Out; };
					if (WithStalk(Blocks[First].Box).Touches(WithStalk(Blocks[Second].Box), 2.0))
					{
						Blocks[First].Members.Append(Blocks[Second].Members);
						Blocks.RemoveAt(Second);
						Lay(Blocks[First]);
						bJoined = true;
					}
				}
			}
		}
		auto Publish = [&](int Id, double Left, double Top, double Right, double Bottom)
		{
			FAirBattleState::FTagBox Drawn;
			Drawn.Id = Id;
			Drawn.Min = ToPixels(Geometry, FVector2D(Left, Top));
			Drawn.Max = ToPixels(Geometry, FVector2D(Right, Bottom));
			S.TagBoxes.push_back(Drawn);
		};
		for (const FTagBlock& Block : Blocks)
		{
			const FTagOne& Head = Ones[Block.Members[0]];
			const FLinearColor HeadInk = Head.bHand ? BrightBrass : Head.Tag->bMine ? Cream : Paper * FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
			// The stalk down to the formation; from a block moved out of the way, a thin line to where it flies.
			FVector2D Foot((Block.Box.L + Block.Box.R) * 0.5, Block.Box.B);
			if (Block.bMoved)
			{
				// (from the edge that faces the formation, never back up through the block; none when it flies under it)
				const FVector2D Aim(Head.At.X, Head.At.Y + 10.0);
				if (Aim.Y < Block.Box.T)
				{
					Foot.Y = Block.Box.T;
				}
				else if (Aim.Y <= Block.Box.B)
				{
					Foot = FVector2D(Aim.X < Block.Box.L ? Block.Box.L : Block.Box.R, Aim.Y);
				}
				const bool bAimInside = Aim.X >= Block.Box.L && Aim.X <= Block.Box.R && Aim.Y >= Block.Box.T && Aim.Y <= Block.Box.B;
				if (!bAimInside)
				{
					Line({ Foot, Aim }, HeadInk * FLinearColor(1.0f, 1.0f, 1.0f, 0.45f), 1.0f, 4);
				}
			}
			else
			{
				Line({ Foot, FVector2D(Foot.X, Foot.Y + 10.0) }, HeadInk * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 1.0f, 4);
			}
			const double Wd = Block.Box.R - Block.Box.L;
			double Top = Block.Box.T;
			const int32 Shown = FMath::Min(2, Block.Members.Num());
			for (int32 K = 0; K < Shown; ++K)
			{
				const FTagOne& One = Ones[Block.Members[K]];
				const FLinearColor Ink = One.bHand ? BrightBrass : One.Tag->bMine ? Cream : Paper * FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
				Fill(Block.Box.L, Top, Wd, TagH, FLinearColor(0.02f, 0.015f, 0.01f, 0.7f), 4);
				if (One.bHand)
				{
					Fill(Block.Box.L, Top + TagH - 1.0, Wd, 2.0, BrightBrass, 5);
				}
				Write(One.Name, Block.Box.L + 6.0, Top + 1.0, NameFace, Ink, 5);
				Write(One.Under, Block.Box.L + 6.0, Top + 15.0, UnderFace, Paper, 5); // (in paper: muted read at 3.4 to 1, the Mac's release 2 check)
				Publish(One.Tag->Id, Block.Box.L, Top, Block.Box.R, Top + TagH);
				Top += TagH + TagGap;
			}
			if (Block.Members.Num() > Shown)
			{
				Fill(Block.Box.L, Top, Wd, MoreH, FLinearColor(0.02f, 0.015f, 0.01f, 0.55f), 4);
				Write(FString::Printf(TEXT("+%d more groups"), Block.Members.Num() - Shown), Block.Box.L + 6.0, Top + 1.0, MoreFace, Muted, 5);
				Publish(Head.Tag->Id, Block.Box.L, Top, Block.Box.R, Top + MoreH);
			}
		}
	}

	// ---- The dispatches, where their place was found (above). Their plates as dark as the tags' (at 0.5 the oldest line
	// nearly vanished over white cloud in Oahu 04, the Mac's release 2 check). ----
	{
		const FSlateFontInfo Face = Typed(10.0f);
		for (const FSentLine& Sent : SentLines)
		{
			const FLinearColor Ink = (Sent.bMine ? Cream : Paper) * FLinearColor(1.0f, 1.0f, 1.0f, Sent.Fade);
			Fill(Sent.X, Sent.Y - 2.0, Sent.Wd, 19.0, FLinearColor(0.02f, 0.015f, 0.01f, 0.75f * Sent.Fade), 2);
			Write(Sent.Text, Sent.X + 6.0, Sent.Y, Face, Ink, 3);
		}
	}
	if (!S.Note.empty())
	{
		const FString Said = Wide(S.Note);
		const FSlateFontInfo SaidFace = Typed(10.5f, true);
		OnPlate(Said, Size.X * 0.5 - WideOf(Said, SaidFace) * 0.5, Size.Y - 92.0, SaidFace, Alarm, 20.0, 6);
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
		const FSlateFontInfo AskFace = Serif(TEXT("Regular"), 13.0f);
		const TArray<FString> Asked = Wrap(Question, AskFace, 620.0 - 44.0);
		const double QW = 620.0, QH = 112.0 + 20.0 * Asked.Num(), QX = (Size.X - QW) * 0.5, QY = Size.Y * 0.42 - QH * 0.5;
		Fill(QX - 1.0, QY - 1.0, QW + 2.0, QH + 2.0, Brass, 10);
		Fill(QX, QY, QW, QH, Leather, 11);
		for (int32 K = 0; K < Asked.Num(); ++K)
		{
			Write(Asked[K], QX + 22.0, QY + 20.0 + 20.0 * K, AskFace, Cream, 12);
		}
		double BX = QX + 22.0;
		BX = Button(S.Question == 1 ? TEXT("HAND IT OVER") : TEXT("BREAK OFF ALL"), TEXT("yes"), BX, QY + QH - 44.0, false, true, 12);
		Button(TEXT("KEEP FIGHTING"), TEXT("no"), BX, QY + QH - 44.0, true, true, 12);
	}

	// ---- The after card (the UI director, 18:45Z and 18:50Z): RAID RESULT, the title, IN THE AIR, the lines, the stamp,
	// the rows, the reckoning, CONTINUE. ----
	if (S.bCard)
	{
		const AirDesk::Card& C = S.Card;
		// The card is as wide as its longest line (660 at the least, the screen less a margin at the most); a line
		// longer still goes onto the next. Every line is laid out first, so the card is as tall as what it holds.
		const FSlateFontInfo TitleFace = Serif(TEXT("Bold"), 21.0f), LineFace = Typed(10.0f), MineFace = Typed(10.0f, true), ReckonFace = Serif(TEXT("Italic"), 11.0f);
		const FString Stamp = Wide(C.Level);
		const FSlateFontInfo StampFace = Typed(12.0f, true, 60);
		const double SW = WideOf(Stamp, StampFace) + 18.0;
		struct FCardLine
		{
			FString Words;
			bool bMine = false;
			bool bRow = false;
		};
		TArray<FCardLine> Body;
		double Longest = 0.0;
		for (const std::string& L : C.Lines)
		{
			Body.Add({ Wide(L), false, false });
			Longest = FMath::Max(Longest, WideOf(Body.Last().Words, LineFace));
		}
		for (const AirDesk::Card::Row& R : C.Rows)
		{
			Body.Add({ Wide(R.Name) + TEXT(": ") + Wide(R.Words), R.bMine, true });
			Longest = FMath::Max(Longest, WideOf(Body.Last().Words, R.bMine ? MineFace : LineFace));
		}
		const FString Reckoning = Wide(C.Reckoning);
		const FString Title = Wide(C.Title);
		Longest = FMath::Max(Longest, WideOf(Title, TitleFace) + 16.0 + SW);
		const double CW = FMath::Clamp(Longest + 52.0, 660.0, FMath::Max(660.0, Size.X - 80.0));
		const double Room = CW - 52.0;
		const TArray<FString> TitleLines = Wrap(Title, TitleFace, Room - 16.0 - SW);
		struct FLaid
		{
			FString Words;
			const FSlateFontInfo* Face = nullptr;
			FLinearColor Ink;
		};
		TArray<FLaid> Laid;
		for (const FCardLine& Each : Body)
		{
			const FSlateFontInfo& Face = Each.bMine ? MineFace : LineFace;
			for (const FString& Piece : Wrap(Each.Words, Face, Room))
			{
				Laid.Add({ Piece, &Face, Each.bRow && !Each.bMine ? InkSoft : DeskInk });
			}
		}
		const TArray<FString> ReckonLines = Reckoning.IsEmpty() ? TArray<FString>() : Wrap(Reckoning, ReckonFace, Room);
		const double CH = 18.0 + 16.0 + 28.0 * TitleLines.Num() + 12.0 + 18.0 + 18.0 * Laid.Num() + 18.0 * ReckonLines.Num() + 58.0;
		const double CX = (Size.X - CW) * 0.5, CY = FMath::Max(BarH + 20.0, (Size.Y - CH) * 0.5);
		Fill(0.0, 0.0, Size.X, Size.Y, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 20);
		Fill(CX, CY, CW, CH, Paper, 21);
		double CY2 = CY + 18.0;
		Write(Wide(C.Kicker), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 16.0;
		for (const FString& Piece : TitleLines)
		{
			Write(Piece, CX + 26.0, CY2, TitleFace, DeskInk, 22);
			CY2 += 28.0;
		}
		Fill(CX + CW - 26.0 - SW, CY + 18.0, SW, 26.0, InkRed, 22);
		Fill(CX + CW - 26.0 - SW + 2.0, CY + 20.0, SW - 4.0, 22.0, Paper, 23);
		Write(Stamp, CX + CW - 26.0 - SW + 9.0, CY + 22.0, StampFace, InkRed, 24);
		CY2 += 12.0;
		Write(TEXT("IN THE AIR"), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 18.0;
		for (const FLaid& Each : Laid)
		{
			Write(Each.Words, CX + 26.0, CY2, *Each.Face, Each.Ink, 22);
			CY2 += 18.0;
		}
		for (const FString& Piece : ReckonLines)
		{
			Write(Piece, CX + 26.0, CY2, ReckonFace, InkSoft, 22);
			CY2 += 18.0;
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
	if (Battle.IsValid() && Battle->GetState() && Battle->GetState()->bFlying)
	{
		Battle->Press(TEXT("skip")); // (any click during the flyover takes command)
		return FReply::Handled();
	}
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

FReply SAirBattleScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// (the second press of a double click comes here in place of OnMouseButtonDown)
	if (!Battle.IsValid() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FAirBattleState* S = Battle->GetState();
	if (S && S->bFlying)
	{
		Battle->Press(TEXT("skip"));
		return FReply::Handled();
	}
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (const FButton* Hit = ButtonAt(Local))
	{
		// (the chase view's steps pressed quickly twice step twice; any other button is pressed once, as before: a
		// second press of HAND IT OVER must never be read as an answer to a question no longer asked)
		if (Hit->Action.StartsWith(TEXT("ride:")))
		{
			Do(Hit->Action);
		}
		return FReply::Handled();
	}
	// The chase view: a double click on a squadron's tag, his or the enemy's, rides behind its leader.
	Battle->RideAt(ToPixels(Geometry, Local));
	return FReply::Handled();
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
	if (S && S->bFlying)
	{
		// (any key pressed during the flyover takes command; a key still held from before it began only repeats, and
		// keeps the film: the cloud pre-check's note on round 5)
		if (!Event.IsRepeat())
		{
			Battle->Press(TEXT("skip"));
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		if (S && S->Question != 0)
		{
			Battle->Answer(false);
		}
		else if (S && S->bClose && !S->bCard)
		{
			Battle->Press(TEXT("ride")); // (out of the close view, back to the strategy view)
		}
		return FReply::Handled();
	}
	if (S && S->bClose && !S->bCard && (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Up || Key == EKeys::Down))
	{
		// The chase view's steps: left and right along the squadron, up and down to the last or next squadron.
		Battle->Press(Key == EKeys::Left ? TEXT("ride:prev") : Key == EKeys::Right ? TEXT("ride:next") : Key == EKeys::Up ? TEXT("ride:up") : TEXT("ride:down"));
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
