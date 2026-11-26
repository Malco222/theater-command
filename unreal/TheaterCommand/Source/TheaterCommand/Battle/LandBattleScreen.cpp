#include "Battle/LandBattleScreen.h"
#include "Battle/LandLive.h"

#include "Battle/LandBattle.h"
#include "Battle/LandBattleState.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
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
	/** The water on the map of the field: a dark blue-grey of its own, never slate, which is the enemy's (the UI director,
	 * 26 November 2026). */
	const FLinearColor MapWater = FLinearColor(FColor::FromHex(TEXT("34424C")));

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
	// (the night before the attack: no words and no marks while its shots play)
	if (S.NightNow.bNight)
	{
		return LayerId;
	}
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
	// (his clock's and camera's buttons, compact, in the top bar beside the clock where it has the room: the UI director,
	// 18:26Z; the same widths as drawn below)
	const FSlateFontInfo TopFace = Typed(10.0f, true, 30), TopCapFace = Typed(10.0f, true, 0);
	struct FTop { const TCHAR* Label; const TCHAR* Key; const TCHAR* Action; bool bLit; };
	TArray<FTop> Tops;
	if (bGeneral && !S.bCard)
	{
		const bool bDeploying = S.General.Clipboard.Deploying();
		// (groups on the number keys, release 2: the numbers are his groups' and the pace is on the minus and plus keys,
		// so the pace's buttons lose their number marks and one small muted mark of the two keys stands after them; the
		// UI director, 26 November 2026. A part with no action is that mark: words only, no button.)
		const bool bGroupKeys = FLandLive::RuleNow(TEXT("groups"));
		if (!bDeploying)
		{
			Tops.Add({ TEXT("PAUSE"), TEXT("SPACE"), TEXT("speed:0"), S.SpeedNow <= 0.0 });
			Tops.Add({ TEXT("\u00D71"), bGroupKeys ? TEXT("") : TEXT("1"), TEXT("speed:1"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 1.0) });
			Tops.Add({ TEXT("\u00D72"), bGroupKeys ? TEXT("") : TEXT("2"), TEXT("speed:2"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 2.0) });
			Tops.Add({ TEXT("\u00D74"), bGroupKeys ? TEXT("") : TEXT("3"), TEXT("speed:4"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 4.0) });
			if (bGroupKeys)
			{
				Tops.Add({ TEXT("\u2212  +"), TEXT(""), TEXT(""), false });
			}
		}
		Tops.Add({ TEXT("FILM"), TEXT("C"), TEXT("general:film"), false });
		// (his view from above in one press, lit while it is on; pressed again, back as it was)
		Tops.Add({ TEXT("TOP VIEW"), TEXT("O"), TEXT("general:topview"), S.GeneralEye.TopView() });
	}
	auto TopWidth = [&](const FTop& P)
	{
		// (the pace keys' small mark, and a button with no key mark: no cap's room)
		return !P.Action[0] ? WideOf(P.Label, TopFace) + 6.0 : WideOf(P.Label, TopFace) + 18.0 + (P.Key[0] ? WideOf(P.Key, TopCapFace) + 16.0 : 0.0);
	};
	double TopsW = 0.0;
	for (const FTop& P : Tops)
	{
		TopsW += TopWidth(P) + 5.0;
	}
	// (clear of the tug of war and its flags in the middle: 190 either side of the middle, the flag and a margin)
	bControlsOnTop = Tops.Num() > 0 && Size.X - 22.0 - TopsW >= Size.X * 0.5 + 190.0 + 10.0 + 27.0 + 14.0;
	if (bGeneral)
	{
		CardsH = PaintGeneral(Geometry, OutDrawElements, LayerId);
		// (his screen's layers: 0 to 17 its field, bars and cards, 18 to 30 the map of the field, 31 and 32 the plate of
		// the unit under the pointer)
		LayerId += 33;
	}
	else
	{
		// (away from his screen, the film or a card up: no plate left over for when he comes back, till the pointer moves;
		// and no map of the field to press)
		HoverBlock = -1;
		TagBoxes.Reset();
		MapFrame = LandFieldMap::Frame();
	}

	// ---- The top bar: the battle's name and place at the left, the clock at the right, the bar in the middle. ----
	const double BarH = 66.0;
	Fill(0.0, 0.0, Size.X, BarH, Leather, 0);
	Fill(0.0, BarH, Size.X, 1.0, Brass, 1);
	LastTopShare = Size.Y > 0.0 ? FMath::Clamp((BarH + 1.0) / Size.Y, 0.0, 0.5) : 0.0;
	Write(Wide(W.Title), 22.0, 9.0, Serif(TEXT("Bold"), 16.0f), Cream, 2);
	Write(Wide(W.Place.empty() ? W.Date : W.Place + ", " + W.Date), 22.0, 36.0, Typed(10.0f), Muted, 2);
	{
		const FString Clock = Wide(W.Clock);
		const FSlateFontInfo ClockFace = Typed(16.0f, true);
		Write(Clock, Size.X - 22.0 - WideOf(Clock, ClockFace), 12.0, ClockFace, Cream, 2);
		if (bGeneral && bControlsOnTop)
		{
			// (his PAUSE, x1, x2, x4 and FILM under the clock, each with its key's cap; the lit one says the pace, so no
			// small "x1" besides)
			double X = Size.X - 22.0 - TopsW + 5.0;
			const double Y = 36.0, Ht = 24.0;
			for (const FTop& P : Tops)
			{
				const double Wd = TopWidth(P);
				if (!P.Action[0])
				{
					// (the pace keys' mark: small and muted, words only)
					Write(P.Label, X + 1.0, Y + 4.0, TopFace, Muted, 5);
					X += Wd + 5.0;
					continue;
				}
				const FLinearColor Ink = P.bLit ? DeskInk : Cream, Ground = P.bLit ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather;
				Fill(X, Y, Wd, Ht, Brass, 3);
				Fill(X + 1.0, Y + 1.0, Wd - 2.0, Ht - 2.0, Ground, 4);
				const double LabelW = WideOf(P.Label, TopFace);
				Write(P.Label, X + 9.0, Y + 4.0, TopFace, Ink, 5);
				if (P.Key[0])
				{
					const double CapX = X + 9.0 + LabelW + 7.0, CapW = WideOf(P.Key, TopCapFace) + 9.0;
					Fill(CapX, Y + 4.0, CapW, Ht - 8.0, Ink * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 5);
					Fill(CapX + 1.0, Y + 5.0, CapW - 2.0, Ht - 10.0, Ground, 6);
					Write(P.Key, CapX + 4.5, Y + 4.0, TopCapFace, Ink, 7);
				}
				Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + Wd), static_cast<float>(Y + Ht)), P.Action });
				X += Wd + 5.0;
			}
		}
		else if (!bGeneral)
		{
			const FString Pace = S.SpeedNow <= 0.0 ? FString(TEXT("HELD")) : FString::Printf(TEXT("x%s"), *FString::SanitizeFloat(S.SpeedNow, 0));
			const FSlateFontInfo PaceFace = SmallCaps();
			Write(Pace, Size.X - 22.0 - WideOf(Pace, PaceFace), 38.0, PaceFace, S.SpeedNow <= 0.0 ? Amber : Muted, 2);
		}
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
		if (bGeneral)
		{
			// (his screen: on the desk strip under the title band, one fixed place, the newest at the top, three lines at most,
			// the older fading; the foot is the cards' alone: the UI director, 18:26Z)
			double NY = BarH + 4.0;
			int32 Shown = 0;
			for (int32 K = Pieces.Num() - 1; K >= 0 && Shown < 3; --K)
			{
				const LandDesk::Line& L = W.Lines[static_cast<size_t>(K)];
				const float Fade = static_cast<float>(FMath::Clamp(1.0 - (L.Age - 0.7) / 0.3, 0.0, 1.0)) * (Shown == 0 ? 1.0f : Shown == 1 ? 0.8f : 0.6f);
				for (const FString& Text : Pieces[K])
				{
					if (Shown >= 3)
					{
						break;
					}
					if (Fade > 0.0f)
					{
						Fill(18.0, NY, WideOf(Text, Face) + 16.0, 19.0, Leather * FLinearColor(1.0f, 1.0f, 1.0f, 0.88f * Fade), 2);
						Write(Text, 26.0, NY + 1.0, Face, (L.Side == 0 ? Cream : Paper) * FLinearColor(1.0f, 1.0f, 1.0f, Fade), 3);
					}
					NY += 20.0;
					++Shown;
				}
			}
			Pieces.Reset();
		}
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

	// ---- (Camera specials, release 2, -TheaterLandNickCard) A corps' earned name on the picture while the film holds
	// on its first kill: low on the left, in quotes, old gold on a dark scroll with brass edges, as the map letters it over
	// the flag; up in a quarter second, down as the shot is left. ----
	{
		const FString* Earned = S.Pose.Card >= 0 && !bGeneral ? S.NickOfBlock.Find(S.Pose.Card) : nullptr;
		if (!Earned || Earned->IsEmpty())
		{
			CardUp = -1;
		}
		else
		{
			if (CardUp != S.Pose.Card)
			{
				CardUp = S.Pose.Card;
				CardSince = S.Real;
			}
			const float Up = static_cast<float>(FMath::Clamp((S.Real - CardSince) / 0.25, 0.0, 1.0));
			const FString Said = FString::Printf(TEXT("\u201C%s\u201D"), **Earned);
			const FSlateFontInfo NameFace = Serif(TEXT("Bold"), 22.0f);
			const double Wd = WideOf(Said, NameFace) + 36.0, Ht = 40.0;
			const double X = FMath::Max(24.0, Size.X * 0.07), Y = Size.Y - 150.0 - CardsH - Ht;
			const FLinearColor Fade(1.0f, 1.0f, 1.0f, Up);
			Fill(X, Y, Wd, Ht, Brass * Fade, 8);
			Fill(X + 1.0, Y + 1.0, Wd - 2.0, Ht - 2.0, Leather * FLinearColor(1.0f, 1.0f, 1.0f, 0.92f * Up), 9);
			Fill(X + 4.0, Y + 4.0, Wd - 8.0, 1.0, Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f * Up), 10);
			Fill(X + 4.0, Y + Ht - 5.0, Wd - 8.0, 1.0, Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f * Up), 10);
			Write(Said, X + 18.0, Y + 6.0, NameFace, BrightBrass * Fade, 11);
		}
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
		// (the verdict as a stamp in the kit's brass, never a box where a close button goes; "Close" read as "shut this",
		// so a close result is a narrow one: the UI director, 18:26Z)
		FString Stamp = Wide(W.Level);
		if (Stamp.StartsWith(TEXT("Close ")))
		{
			Stamp = TEXT("A narrow ") + Stamp.Mid(6).ToLower();
		}
		else if (!Stamp.IsEmpty())
		{
			const FString First = Stamp.Left(1).ToLower();
			Stamp = (First == TEXT("a") || First == TEXT("e") || First == TEXT("i") || First == TEXT("o") || First == TEXT("u") ? TEXT("An ") : TEXT("A ")) + Stamp.ToLower();
		}
		Stamp = Stamp.ToUpper();
		const FSlateFontInfo StampFace = Typed(12.0f, true, 60);
		const double SW = Stamp.IsEmpty() ? 0.0 : WideOf(Stamp, StampFace) + 26.0;
		const FString Title = Wide(S.Rec.Title);
		// (a column of about 70 characters, centred, a line's space between paragraphs)
		const double Room = WideOf(FString::ChrN(70, TCHAR('n')), LineFace);
		const double CW = FMath::Min(Room + 52.0, FMath::Max(360.0, Size.X - 40.0));
		const double Inner = CW - 52.0;
		const TArray<FString> TitleLines = Wrap(Title, TitleFace, Inner);
		TArray<TArray<FString>> Paras;
		int32 LineCount = 0;
		for (const std::string& L : W.After)
		{
			Paras.Add(Wrap(Wide(L), LineFace, Inner));
			LineCount += Paras.Last().Num();
		}
		const double StampH = Stamp.IsEmpty() ? 0.0 : 40.0;
		const double CH = 18.0 + 16.0 + 28.0 * TitleLines.Num() + 10.0 + StampH + 12.0 + 18.0 + 18.0 * LineCount + 10.0 * FMath::Max(0, Paras.Num() - 1) + 62.0;
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
		CY2 += 10.0;
		// (the result in the clerk's ink inside a brass seal's two rings, not red: red is a signal to act, and a result is
		// news that has arrived; the art director on cut 4, by rulings 149(g) and 160)
		if (!Stamp.IsEmpty())
		{
			const double SX = CX + 26.0, SH = 32.0;
			Fill(SX, CY2, SW, SH, Brass, 22);
			Fill(SX + 2.0, CY2 + 2.0, SW - 4.0, SH - 4.0, Paper, 23);
			Fill(SX + 4.0, CY2 + 4.0, SW - 8.0, SH - 8.0, Brass, 23);
			Fill(SX + 5.0, CY2 + 5.0, SW - 10.0, SH - 10.0, Paper, 24);
			Write(Stamp, SX + 13.0, CY2 + 6.0, StampFace, DeskInk, 25);
			CY2 += StampH;
		}
		CY2 += 12.0;
		Write(TEXT("ON THE FIELD"), CX + 26.0, CY2, SmallCaps(), InkSoft, 22);
		CY2 += 18.0;
		for (int32 K = 0; K < Paras.Num(); ++K)
		{
			for (const FString& Each : Paras[K])
			{
				Write(Each, CX + 26.0, CY2, LineFace, DeskInk, 22);
				CY2 += 18.0;
			}
			CY2 += 10.0;
		}
		// (CONTINUE takes Enter, its key's cap shown as the others: the keys rule)
		{
			const FSlateFontInfo Face = Typed(10.0f, true, 30), CapFace = Typed(10.0f, true, 0);
			const FString Label = TEXT("CONTINUE"), Key = TEXT("ENTER");
			const double LabelW = WideOf(Label, Face), CapW = WideOf(Key, CapFace) + 10.0;
			const double Wd = LabelW + 22.0 + CapW + 8.0, Ht = 26.0;
			const double BX = CX + CW - 26.0 - Wd, BY = CY + CH - 44.0;
			Fill(BX, BY, Wd, Ht, Brass, 25);
			Fill(BX + 1.0, BY + 1.0, Wd - 2.0, Ht - 2.0, FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f), 26);
			Write(Label, BX + 11.0, BY + 5.0, Face, DeskInk, 27);
			const double CapX = BX + 11.0 + LabelW + 8.0;
			Fill(CapX, BY + 4.0, CapW, Ht - 8.0, DeskInk * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 27);
			Fill(CapX + 1.0, BY + 5.0, CapW - 2.0, Ht - 10.0, FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f), 28);
			Write(Key, CapX + 5.0, BY + 5.0, CapFace, DeskInk, 29);
			Buttons.Add({ FSlateRect(static_cast<float>(BX), static_cast<float>(BY), static_cast<float>(BX + Wd), static_cast<float>(BY + Ht)), FString(TEXT("continue")) });
		}
	}
	return LayerId + 30;
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
	// (one word, one meaning: the artillery's GUNS, the support teams' own; the UI director, 18:26Z)
	auto KindWord = [](const LandCommand::Tag& T)
	{
		if (T.Team == "mor") return TEXT("MORTARS");
		if (T.Team == "mg") return TEXT("MACHINE-GUNS");
		const std::string& Kind = T.Kind;
		return Kind == "inf" ? TEXT("RIFLES") : Kind == "arm" ? TEXT("TANKS") : Kind == "art" ? TEXT("GUNS") : Kind == "mot" ? TEXT("MOTORISED") : Kind == "eng" ? TEXT("ENGINEERS")
			: Kind == "mtn" ? TEXT("MOUNTAIN") : Kind == "cav" ? TEXT("CAVALRY") : Kind == "fort" ? TEXT("WORKS") : TEXT("");
	};
	// What it is doing now, in plain words, once the battle runs (the production lead, 22 November 23:10Z: plain feedback
	// on what each unit is doing): the longer words first and a shorter where the card has no room; empty where a chip
	// already says how it stands, or nothing is worth saying. Busy words in the desk's ink, a quiet one in the soft ink.
	auto DoingWords = [](const LandCommand::Tag& T, const TCHAR*& OutShort, bool& bOutBusy) -> const TCHAR*
	{
		OutShort = nullptr;
		bOutBusy = true;
		const std::string& Act = T.Act;
		const std::string& Order = T.OrderKind;
		if (Act == "down" || Act == "rout" || T.State == "routing" || T.State == "gone" || T.State == "withdrawn")
		{
			return nullptr;
		}
		if (Order == "withdraw" && (Act == "march" || Act == "run" || Act == "turn"))
		{
			return TEXT("FALLING BACK");
		}
		if (T.State == "wavering")
		{
			return TEXT("SHAKEN");
		}
		if (T.bSearch)
		{
			OutShort = TEXT("FIRING");
			return TEXT("FIRING INTO COVER");
		}
		if (Act == "fight")
		{
			return TEXT("FIGHTING");
		}
		if (Act == "run")
		{
			OutShort = TEXT("RUNNING UP");
			return Order == "attack" ? TEXT("CHARGING") : TEXT("AT THE DOUBLE");
		}
		if (Act == "march")
		{
			return Order == "attack" ? TEXT("ADVANCING") : TEXT("MOVING");
		}
		if (Act == "turn")
		{
			return TEXT("TURNING");
		}
		bOutBusy = false;
		if (T.bHidden)
		{
			return TEXT("IN AMBUSH");
		}
		return T.OrderIn > 0.5 ? TEXT("AWAITING ORDERS") : TEXT("HOLDING");
	};
	// Who commands it (the UI director, 18:26Z: two ideas, not three words): his staff, or himself; an order on the air
	// is his, with the radio's sign and the time it has left beside it (OutAir, battle seconds; -1 none on the air).
	auto OwnerOf = [](const LandCommand::Tag& T, FString& OutWords, FLinearColor& OutPaper, FLinearColor& OutInk, double& OutAir)
	{
		OutWords.Reset();
		OutAir = -1.0;
		switch (T.Says)
		{
		case LandCommand::Chip::Staff: OutWords = TEXT("STAFF"); OutPaper = Track; OutInk = Cream; break;
		case LandCommand::Chip::Yours: OutWords = TEXT("YOURS"); OutPaper = Brass; OutInk = DeskInk; break;
		// (an order with under half a second left is as good as there: no "0:00" beside it, 22 November 23:15Z)
		case LandCommand::Chip::Radio: OutWords = TEXT("YOURS"); OutPaper = Brass; OutInk = DeskInk; OutAir = T.OrderIn >= 0.5 ? T.OrderIn : -1.0; break;
		default: break;
		}
	};
	// How it stands, where that is news: running, holding until relieved (when help is due; help late, its men break
	// sooner), gone from the field.
	auto StateOf = [ClockNow = Clip.Now()](const LandCommand::Tag& T, FString& OutWords, FLinearColor& OutPaper, FLinearColor& OutInk)
	{
		OutWords.Reset();
		if (!T.bOrderable && T.Says == LandCommand::Chip::Gone) { OutWords = TEXT("GONE"); OutPaper = Track; OutInk = Muted; return; }
		if (T.State == "routing") { OutWords = TEXT("RUNNING"); OutPaper = Alarm; OutInk = Cream; return; }
		if (T.bWaits && !T.bWaitFree)
		{
			const int32 Secs = FMath::Max(0, FMath::CeilToInt32(T.WaitDue - ClockNow));
			OutWords = T.bWaitLate ? FString(TEXT("HELP LATE")) : Secs > 0 ? FString::Printf(TEXT("HELP IN %d:%02d"), Secs / 60, Secs % 60) : FString(TEXT("DUG IN"));
			OutPaper = T.bWaitLate ? Alarm : LeafInk;
			OutInk = Cream;
		}
	};
	// An order's time on the air, in the clock's own style ("0:01", never "1s").
	auto AirWords = [](double Air) { const int32 Secs = FMath::Max(0, FMath::CeilToInt32(Air)); return FString::Printf(TEXT("%d:%02d"), Secs / 60, Secs % 60); };
	// The radio's sign (the kit's stroke style): a mast and its two waves, in a Box-square from (X, Y).
	auto RadioSign = [&](double X, double Y, double Box, const FLinearColor& Ink, int32 Layer)
	{
		const double K = Box / 16.0;
		const float Stroke = static_cast<float>(FMath::Max(1.2, 1.5 * K));
		auto At = [&](double Px, double Py) { return FVector2D(X + Px * K, Y + Py * K); };
		Lines({ At(8, 6), At(8, 15) }, Ink, Stroke, Layer);
		Lines({ At(5, 15), At(11, 15) }, Ink, Stroke, Layer);
		Lines({ At(5, 3), At(3.5, 6), At(5, 9) }, Ink, Stroke, Layer);
		Lines({ At(11, 3), At(12.5, 6), At(11, 9) }, Ink, Stroke, Layer);
		Lines({ At(2.5, 1), At(0.5, 6), At(2.5, 11) }, Ink, Stroke, Layer);
		Lines({ At(13.5, 1), At(15.5, 6), At(13.5, 11) }, Ink, Stroke, Layer);
	};
	// The kit's signs (the UI director's kit/icons.json, a 24 box, a 1.6 stroke, no fill), as polylines: its curves and
	// arcs walked in short steps. Box the box's side on the screen, from its top left corner (X, Y).
	auto Cubic = [](TArray<FVector2D>& Out, FVector2D P0, FVector2D P1, FVector2D P2, FVector2D P3)
	{
		for (int32 I = Out.Num() == 0 ? 0 : 1; I <= 8; ++I)
		{
			const double U = I / 8.0, V = 1.0 - U;
			Out.Add(P0 * (V * V * V) + P1 * (3.0 * V * V * U) + P2 * (3.0 * V * U * U) + P3 * (U * U * U));
		}
	};
	auto Arc = [](TArray<FVector2D>& Out, FVector2D Mid, double Radius, double From, double To)
	{
		for (int32 I = Out.Num() == 0 ? 0 : 1; I <= 12; ++I)
		{
			const double A = From + (To - From) * I / 12.0;
			Out.Add(Mid + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
	};
	auto KitSign = [&](const TCHAR* Which, double X, double Y, double Box, const FLinearColor& Ink, int32 Layer)
	{
		const double K = Box / 24.0;
		const float Stroke = static_cast<float>(FMath::Max(1.2, 1.6 * K));
		auto P = [&](double Px, double Py) { return FVector2D(X + Px * K, Y + Py * K); };
		auto Path = [&](TArray<FVector2D> Points) { Lines(Points, Ink, Stroke, Layer); };
		auto Dot = [&](double Px, double Py) { Lines({ P(Px - 0.4, Py), P(Px + 0.4, Py) }, Ink, Stroke, Layer); };
		const FString Sign(Which);
		if (Sign == TEXT("hourglass"))
		{
			Path({ P(7, 4), P(17, 4) });
			Path({ P(7, 20), P(17, 20) });
			TArray<FVector2D> L, R;
			Cubic(L, P(8, 4), P(8, 8), P(12, 9), P(12, 12));
			Cubic(L, P(12, 12), P(12, 15), P(8, 16), P(8, 20));
			Cubic(R, P(16, 4), P(16, 8), P(12, 9), P(12, 12));
			Cubic(R, P(12, 12), P(12, 15), P(16, 16), P(16, 20));
			Path(L);
			Path(R);
		}
		else if (Sign == TEXT("infantry"))
		{
			TArray<FVector2D> Dome, Face;
			Arc(Dome, P(12, 12), 6.5 * K, UE_DOUBLE_PI, 2.0 * UE_DOUBLE_PI);
			Path(Dome);
			Path({ P(4, 12), P(20, 12) });
			Cubic(Face, P(8.5, 12), P(8.5, 17), P(10, 20), P(12, 20));
			Cubic(Face, P(12, 20), P(14, 20), P(15.5, 17), P(15.5, 12));
			Path(Face);
		}
		else if (Sign == TEXT("tank"))
		{
			TArray<FVector2D> Tread = { P(5, 14), P(19, 14) };
			Arc(Tread, P(19, 17), 3.0 * K, -0.5 * UE_DOUBLE_PI, 0.5 * UE_DOUBLE_PI);
			Tread.Add(P(5, 20));
			Arc(Tread, P(5, 17), 3.0 * K, 0.5 * UE_DOUBLE_PI, 1.5 * UE_DOUBLE_PI);
			Path(Tread);
			Path({ P(7, 14), P(7, 11), P(15, 11), P(16.5, 14) });
			Path({ P(12, 11), P(12, 8), P(15, 8) });
			Path({ P(15, 9.5), P(21.5, 9.5) });
			Dot(7, 17);
			Dot(11, 17);
			Dot(15, 17);
			Dot(19, 17);
		}
		else if (Sign == TEXT("gun"))
		{
			TArray<FVector2D> Wheel;
			Arc(Wheel, P(8, 16), 4.0 * K, 0.0, 2.0 * UE_DOUBLE_PI);
			Path(Wheel);
			Path({ P(8, 16), P(20, 7) });
			Path({ P(10.5, 12.8), P(5, 20) });
			Path({ P(17.8, 8.6), P(18.9, 10.1) });
		}
		else if (Sign == TEXT("anti-tank"))
		{
			TArray<FVector2D> Wheel;
			Arc(Wheel, P(10.5, 16.8), 3.7 * K, 0.0, 2.0 * UE_DOUBLE_PI);
			Path(Wheel);
			Path({ P(8, 9.6), P(22.4, 9.6) });
			Path({ P(22.4, 7.6), P(22.4, 11.6) });
			Path({ P(10.5, 4.2), P(10.5, 13) });
			Path({ P(8, 9.6), P(2, 20.4) });
		}
		else if (Sign == TEXT("machine-gun"))
		{
			Path({ P(5, 7.8), P(11.2, 7.8), P(11.2, 11.2), P(5, 11.2), P(5, 7.8) });
			Path({ P(11.2, 9.5), P(21.5, 9.5) });
			Path({ P(8.6, 11.2), P(4.6, 20) });
			Path({ P(8.6, 11.2), P(13.4, 20) });
			Path({ P(5, 9.5), P(2.6, 9.5) });
			Path({ P(2.6, 8), P(2.6, 11) });
		}
		else if (Sign == TEXT("mortar"))
		{
			Path({ P(7.7, 18.5), P(14.1, 4.5), P(16.3, 5.5), P(9.9, 19.5), P(7.7, 18.5) });
			Path({ P(4, 20.5), P(13.2, 20.5) });
			Path({ P(13.4, 11.8), P(19.8, 20.5) });
		}
	};
	// (a unit's kind as the kit signs it, for a card with no room for its word)
	auto KindSign = [](const LandCommand::Tag& T)
	{
		if (T.Team == "mor") return TEXT("mortar");
		if (T.Team == "mg") return TEXT("machine-gun");
		if (T.Team == "at") return TEXT("anti-tank");
		return T.Kind == "arm" ? TEXT("tank") : T.Kind == "art" ? TEXT("gun") : TEXT("infantry");
	};
	// One chip: its paper, its words (and the radio's sign and time first, where an order is on the air), from (X, Y);
	// its width. Measured only, nothing drawn, when bDraw is false.
	const FSlateFontInfo ChipFace = Typed(11.0f, true, 0);
	// A unit's group number (groups on the number keys, release 2): a small square chip in the lit key mark's look, the
	// desk's ink on brass, from its top left corner (X, Y); 16 a side (the UI director, 26 November 2026).
	const FSlateFontInfo TiedFace = Typed(10.0f, true, 0);
	auto TiedChip = [&](int Group, double X, double Y, int32 Layer)
	{
		const FString Digit = FString::FromInt(Group);
		Fill(X, Y, 16.0, 16.0, DeskInk * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), Layer);
		Fill(X + 1.0, Y + 1.0, 14.0, 14.0, FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f), Layer + 1);
		Write(Digit, X + (16.0 - WideOf(Digit, TiedFace)) * 0.5, Y + 0.5, TiedFace, DeskInk, Layer + 2);
	};
	auto DrawChip = [&](const FString& Words, const FLinearColor& ChipPaper, const FLinearColor& ChipInk, double Air, double X, double Y, double Ht, bool bDraw, int32 Layer) -> double
	{
		const FString Time = Air >= 0.0 ? AirWords(Air) : FString();
		const double SignW = Air >= 0.0 ? 16.0 + WideOf(Time, ChipFace) + 8.0 : 0.0;
		const double Wd = (Words.IsEmpty() ? 0.0 : WideOf(Words, ChipFace) + 10.0) + SignW;
		if (bDraw && Wd > 0.0)
		{
			Fill(X, Y, Wd, Ht, ChipPaper, Layer);
			double WX = X + 5.0;
			if (Air >= 0.0)
			{
				RadioSign(WX, Y + (Ht - 13.0) * 0.5, 13.0, ChipInk, Layer + 1);
				Write(Time, WX + 16.0, Y + (Ht - 17.0) * 0.5, ChipFace, ChipInk, Layer + 1);
				WX += SignW;
			}
			if (!Words.IsEmpty())
			{
				Write(Words, WX, Y + (Ht - 17.0) * 0.5, ChipFace, ChipInk, Layer + 1);
			}
		}
		return Wd;
	};

	// The foot (the UI director, 18:26Z: give the field back): his cards in one row along the bottom, the orders of the
	// unit in hand just above them only while one is in hand. Worked out first, so the tags keep clear of it.
	const bool bHand = !Clip.Chosen().empty();
	// (release 2, each its own Options row, off until passed: groups on the number keys, and drawing up while he places
	// his army; Brian, 26 November 2026)
	const bool bGroupKeys = FLandLive::RuleNow(TEXT("groups"));
	const bool bDrawUp = Clip.Deploying() && FLandLive::RuleNow(TEXT("drawup"));
	const double CardH = 44.0, CardGap = 6.0, RowY = Size.Y - CardH - 10.0;
	const double OrdersY = RowY - 28.0 - 8.0;
	// (release 2, its own Options row, off until passed: the map of the field at the bottom right; Brian, 26 November
	// 2026. Open, everything at the right of the foot ends 10 clear of its left edge, the cards' row and the bar above
	// it alike; folded, FIELD MAP stands at the foot row's right end: the UI director, 26 November 2026, 03:51Z. One
	// rule for both sides, 05:35Z: his own ground at the map's foot. The map is turned half round where the ground his
	// side draws up on lies in the field's north half, which today is the defence's; a field that names no such
	// ground turns for the defence.)
	const bool bFieldMap = FLandLive::RuleNow(TEXT("fieldmap")) && S.Rec.Ground.W > 0.0 && S.Rec.Ground.H > 0.0;
	const double* MapOwn = Clip.Side() == 1 ? S.Rec.Ground.ZoneDef : S.Rec.Ground.ZoneAtt;
	const bool bMapTurned = MapOwn[2] > MapOwn[0] && MapOwn[3] > MapOwn[1] ? 0.5 * (MapOwn[1] + MapOwn[3]) > 0.5 * S.Rec.Ground.H : Clip.Side() == 1;
	MapFrame = bFieldMap && !bMapFolded ? LandFieldMap::Lay(Size.X, Size.Y, S.Rec.Ground.W, S.Rec.Ground.H, bMapTurned) : LandFieldMap::Frame();
	const double FootRight = MapFrame.Valid() ? MapFrame.LeftOf() : Size.X - 18.0;
	// (at deploy BEGIN THE BATTLE stands where the order bar stands in battle, above the row at its right: the UI director,
	// 20:52Z, so the cards keep the row's whole width)
	const double FieldBottom = (bHand || Clip.Deploying() ? OrdersY : RowY) - 6.0;
	const double FieldTop = 72.0;

	// ---- Each unit's ground from high up: its real frontage washed on the ground in its side's colour, soft, fading as
	// the eye comes down (the art director, 18:29Z: the pin's stem no longer ends over empty ground). Filled a scan line
	// at a time, back and forth in one stroke. ----
	if (G.PrintInk > 0.01)
	{
		for (const FLandGeneralShown::Mark& M : G.Marks)
		{
			const LandCommand::Tag* T = M.bPrint ? Clip.TagOf(M.Block) : nullptr;
			if (!T)
			{
				continue;
			}
			FVector2D Q[4];
			double Low = 1e12, High = -1e12;
			for (int32 K = 0; K < 4; ++K)
			{
				Q[K] = Local(M.Print[K * 2], M.Print[K * 2 + 1]);
				Low = FMath::Min(Low, Q[K].Y);
				High = FMath::Max(High, Q[K].Y);
			}
			if (High - Low < 1.0 || High - Low > Size.Y)
			{
				continue;
			}
			const FLinearColor Ink = (T->bMine ? Brass : Slate) * FLinearColor(1.0f, 1.0f, 1.0f, static_cast<float>(G.PrintInk));
			TArray<FVector2D> Zig;
			bool bRightward = true;
			for (double Row = Low + 1.0; Row < High; Row += 2.0)
			{
				double Left = 1e12, Right = -1e12;
				for (int32 K = 0; K < 4; ++K)
				{
					const FVector2D A = Q[K], B = Q[(K + 1) % 4];
					if ((A.Y <= Row && B.Y > Row) || (B.Y <= Row && A.Y > Row))
					{
						const double Cross = A.X + (Row - A.Y) / (B.Y - A.Y) * (B.X - A.X);
						Left = FMath::Min(Left, Cross);
						Right = FMath::Max(Right, Cross);
					}
				}
				if (Right > Left)
				{
					Zig.Add(FVector2D(bRightward ? Left : Right, Row));
					Zig.Add(FVector2D(bRightward ? Right : Left, Row));
					bRightward = !bRightward;
				}
			}
			Lines(Zig, Ink, 2.0f, 1);
			Lines({ Q[0], Q[1], Q[2], Q[3], Q[0] }, Ink * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f), 3.0f, 1);
		}
	}

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

	// ---- The spotter's mark (release 2, -TheaterLandSpotter): looking down on the field, a pale ring on the ground
	// round each of his fighting units; a hidden enemy inside it is seen. Every unit's ring for hidden men on foot; a
	// unit in hand's brighter, with dashed rings farther out for hidden guns and teams and for hidden tanks and
	// motorised units, each with its word: the rules' own kinds (their machine gun and mortar teams count with the
	// guns, their motorised infantry with the tanks). ----
	for (const FLandGeneralShown::SightStroke& R : G.Sights)
	{
		TArray<FVector2D> Points;
		Points.Reserve(static_cast<int32>(R.Points.size()));
		for (const std::pair<double, double>& P : R.Points)
		{
			Points.Add(Local(P.first, P.second));
		}
		const FLinearColor Ink = R.bChosen ? BrightBrass * FLinearColor(1.0f, 1.0f, 1.0f, R.Kind == 0 ? 0.95f : 0.7f) : Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.45f);
		Lines(Points, Ink, R.bChosen && R.Kind == 0 ? 2.0f : 1.5f, 1);
	}
	// (the mark's own lines, drawn under the views' chips at the top right further down: worked out here, so that a
	// ring's word is not written under them. What a ring is, by the rules' kinds; how far off men lying in wait open
	// fire, which is mostly farther than they are seen; who does not wait at all (guns, tanks and the mortar team,
	// which this screen calls MORTARS and the rules count with the guns, so it is named: the machine gun team alone
	// of the teams has a hold of its own; and any block sent to attack or given a target, which as the rules stand
	// makes a hidden machine gun team one of those); what the ring does for one that has fired; and, where smoke is
	// on call, that a screen hides him. The distances are LandSight::Finds, the rules' own, and
	// Tools/land_play/run.sh holds each line to the rules.)
	TArray<FString> SpotterLines;
	const FSlateFontInfo SpotterFace = Typed(10.0f, true, 30);
	double SpotterLeft = Size.X, SpotterBottom = 0.0;
	if (G.bSights)
	{
		const LandSight::Finds Finds;
		SpotterLines.Add(bHand ? TEXT("RINGS: A HIDDEN ENEMY OF THE RING'S KIND IS SEEN INSIDE IT") : TEXT("RINGS: HIDDEN MEN ON FOOT ARE SEEN INSIDE ONE. PICK A UNIT FOR GUNS AND TANKS"));
		SpotterLines.Add(FString::Printf(TEXT("MEN LYING IN WAIT FIRE ON TROOPS IN PLAIN SIGHT WITHIN %d m (MACHINE-GUNS %d m, IN FOREST %d m)"), static_cast<int32>(Finds.Ambush + 0.5), static_cast<int32>(Finds.AmbushMg + 0.5),
			static_cast<int32>(Finds.AmbushForest + 0.5)));
		SpotterLines.Add(TEXT("HIDDEN GUNS, MORTARS AND TANKS, AND ANY SENT TO ATTACK OR GIVEN A TARGET, DO NOT WAIT: FULL RANGE"));
		SpotterLines.Add(TEXT("ONE THAT HAS FIRED IS PLACED AT ONCE INSIDE ITS RING: YOUR FIRE TELLS ON IT IN FULL"));
		if (FLandLive::RuleNow(TEXT("smoke")))
		{
			SpotterLines.Add(TEXT("NONE IS SEEN THROUGH A SMOKE SCREEN"));
		}
		for (const FString& Line : SpotterLines)
		{
			SpotterLeft = FMath::Min(SpotterLeft, Size.X - 22.0 - WideOf(Line, SpotterFace) - 12.0);
		}
		// (the chips stand at 78, 22 high; the lines from 8 under them, 20 each)
		SpotterBottom = 78.0 + 22.0 + 8.0 + 20.0 * SpotterLines.Num();
	}
	// (the words' plates already set this painting, each as its left, its top and its width: no word is written on
	// another's. From high up a unit's three rings stand closer together on the screen than one word is long.)
	TArray<FVector> WordsAt;
	for (const FLandGeneralShown::SightWord& W : G.SightWords)
	{
		const FVector2D At = Local(W.X, W.Y);
		const FString Words = FString::Printf(TEXT("%s %d m"), W.Kind == 0 ? TEXT("ON FOOT") : W.Kind == 1 ? TEXT("GUNS AND TEAMS") : TEXT("TANKS AND MOTORISED"), static_cast<int32>(W.Metres + 0.5));
		const FSlateFontInfo Face = Typed(10.0f, true, 30);
		// (kept on the field: hung to the left of its point where the right edge is near, clear of the bar above and
		// the cards below)
		const double PlateW = WideOf(Words, Face) + 10.0;
		const double PX = At.X + 6.0 + PlateW <= Size.X - 8.0 ? At.X + 6.0 : FMath::Max(8.0, At.X - 6.0 - PlateW);
		const double WordLowest = FMath::Max(FieldTop, FieldBottom - 18.0);
		double WordOwn = FMath::Clamp(At.Y - 9.0, FieldTop, WordLowest);
		// (and clear of the views' chips and the mark's own lines at the top right: dropped just under them)
		if (PX + PlateW > SpotterLeft - 4.0 && WordOwn < SpotterBottom + 2.0)
		{
			WordOwn = FMath::Min(SpotterBottom + 4.0, WordLowest);
		}
		// (and clear of the words already set: its own row or, where that is taken, the nearest free row of those hard
		// under or over one of them, or just under the mark's lines. Go -2 its own row, -1 under the lines, then two
		// for each word set.)
		double PY = WordOwn, WordNearest = -1.0;
		for (int32 Go = -2; Go < 2 * WordsAt.Num(); ++Go)
		{
			const double WordRow = FMath::Clamp(Go == -2 ? WordOwn : Go == -1 ? SpotterBottom + 4.0 : WordsAt[Go / 2].Y + (Go % 2 == 0 ? 20.0 : -20.0), FieldTop, WordLowest);
			const double WordMoved = FMath::Abs(WordRow - WordOwn);
			if (WordNearest >= 0.0 && WordMoved >= WordNearest)
			{
				continue;
			}
			bool bWordFree = !(PX + PlateW > SpotterLeft - 4.0 && WordRow < SpotterBottom + 2.0);
			for (const FVector& Set : WordsAt)
			{
				bWordFree = bWordFree && (PX + PlateW + 2.0 <= Set.X || Set.X + Set.Z + 2.0 <= PX || WordRow + 20.0 <= Set.Y || Set.Y + 20.0 <= WordRow);
			}
			if (bWordFree)
			{
				PY = WordRow;
				WordNearest = WordMoved;
			}
		}
		WordsAt.Add(FVector(PX, PY, PlateW));
		Fill(PX, PY, PlateW, 18.0, Plate, 2);
		Write(Words, PX + 5.0, PY + 1.0, Face, BrightBrass, 3);
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
		const FSlateFontInfo Face = Typed(11.0f, true, 0);
		const FString Name = !T->bKnown ? FString(TEXT("?")) : Wide(T->Short.empty() ? T->Name : T->Short).ToUpper();
		FString OwnerWords, StateWords;
		FLinearColor OwnerPaper = Track, OwnerInk = Cream, StatePaper = Track, StateInk = Cream;
		double Air = -1.0;
		if (!bFoe)
		{
			OwnerOf(*T, OwnerWords, OwnerPaper, OwnerInk, Air);
			StateOf(*T, StateWords, StatePaper, StateInk);
		}
		else if (T->bVague && T->bKnown)
		{
			// (found by its fire, its men not yet plain: the historian's check, 22 November)
			StateWords = TEXT("FIRE SEEN");
			StatePaper = Slate;
			StateInk = Cream;
		}
		// (his own, hidden in woods from the enemy: the forest mark after its name)
		const bool bWoods = !bFoe && T->bHidden;
		const double TreeW = bWoods ? 20.0 : 0.0;
		const double TH = 21.0;
		const double NameW = WideOf(Name, Face);
		const double OwnerW = DrawChip(OwnerWords, OwnerPaper, OwnerInk, Air, 0.0, 0.0, TH, false, 0);
		const double StateW = DrawChip(StateWords, StatePaper, StateInk, -1.0, 0.0, 0.0, TH, false, 0);
		// (its group's number before its name, release 2: his own units only)
		const double TiedW = !bFoe && bGroupKeys && T->Group > 0 ? 19.0 : 0.0;
		const double TW = TiedW + NameW + 12.0 + TreeW + OwnerW + StateW;
		// (kept inside the field: above the foot, under the top bar, inside the edges; pinned at the edge with a small arrow
		// toward its unit where the unit is off it: the UI director, 18:26Z)
		const double WantX = Top.X - TW * 0.5, WantY = Top.Y - TH;
		const double TX = FMath::Clamp(WantX, 8.0, FMath::Max(8.0, Size.X - TW - 8.0));
		const double TY = FMath::Clamp(WantY, FieldTop, FMath::Max(FieldTop, FieldBottom - TH - 4.0));
		const bool bPinned = FMath::Abs(TX - WantX) > 0.5 || FMath::Abs(TY - WantY) > 0.5;
		if (!bPinned)
		{
			Lines({ Foot, Top }, (bFoe ? Slate : Brass) * FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), 1.5f, 3);
			Fill(Foot.X - 2.0, Foot.Y - 2.0, 4.0, 4.0, bFoe ? Slate : Brass, 3);
		}
		else
		{
			// (the arrow: from the tag's edge nearest the unit, the way to it)
			const FVector2D Mid(TX + TW * 0.5, TY + TH * 0.5);
			const FVector2D Way = (Top - Mid).GetSafeNormal();
			if (!Way.IsNearlyZero())
			{
				const double Reach = FMath::Min(FMath::Abs(Way.X) > 1e-6 ? (TW * 0.5) / FMath::Abs(Way.X) : 1e9, FMath::Abs(Way.Y) > 1e-6 ? (TH * 0.5) / FMath::Abs(Way.Y) : 1e9);
				const FVector2D Base = Mid + Way * (Reach + 2.0), Tip = Base + Way * 9.0, Across(-Way.Y, Way.X);
				Lines({ Base + Across * 5.0, Tip, Base - Across * 5.0, Base + Across * 5.0 }, bFoe ? Slate : BrightBrass, 2.0f, 6);
			}
		}
		if (T->bChosen)
		{
			Fill(TX - 2.0, TY - 2.0, TW + 4.0, TH + 4.0 + 4.0, BrightBrass, 4);
		}
		Fill(TX, TY, TW, TH, bFoe ? Slate : Paper, 5);
		if (TiedW > 0.0)
		{
			TiedChip(T->Group, TX + 4.0, TY + 2.5, 6);
		}
		Write(Name, TX + 6.0 + TiedW, TY + 2.0, Face, bFoe ? Cream : DeskInk, 6);
		if (bWoods)
		{
			Forest(TX + TiedW + NameW + 9.0, TY + 1.5, 18.0, LeafInk, 6);
		}
		const double ChipX = TX + TiedW + NameW + 12.0 + TreeW;
		DrawChip(OwnerWords, OwnerPaper, OwnerInk, Air, ChipX, TY, TH, true, 6);
		DrawChip(StateWords, StatePaper, StateInk, -1.0, ChipX + OwnerW, TY, TH, true, 6);
		// (its strength, a thin bar under the tag: what is left of it)
		if (T->bKnown)
		{
			Fill(TX, TY + TH, TW, 3.0, Leather, 5);
			Fill(TX, TY + TH, TW * FMath::Clamp(T->Str, 0.0, 1.0), 3.0, bFoe ? Cream : T->Mor < 0.4 ? Amber : Brass, 6);
		}
		TagBoxes.Add({ FSlateRect(static_cast<float>(TX - 2.0), static_cast<float>(TY - 2.0), static_cast<float>(TX + TW + 2.0), static_cast<float>(TY + TH + 5.0)), M.Block });
	}

	// ---- The corps coming up from next door (neighbours join the fight, release 2): who, where and how soon. ----
	for (const FLandGeneralShown::Comer& C : G.Comers)
	{
		const LandCommand::Neighbour& N = C.What;
		const FVector2D At = Local(C.X, C.Y);
		// (his own by name, with its men; the enemy's his staff has word of, only what it is)
		const FString Name = N.bMine ? (N.Name.empty() ? FString(TEXT("YOUR NEIGHBOURS")) : Wide(N.Name).ToUpper())
			: FString::Printf(TEXT("ENEMY %s"), N.Kind == "arm" ? TEXT("ARMOUR") : N.Kind == "art" ? TEXT("GUNS") : N.Kind == "mot" ? TEXT("MOTORISED") : TEXT("INFANTRY"));
		const int32 Secs = FMath::Max(0, FMath::CeilToInt32(N.In));
		const FString When = N.In <= 0.5 ? FString(TEXT("ARRIVING")) : FString::Printf(TEXT("IN %d:%02d"), Secs / 60, Secs % 60);
		const FString Under = N.bMine
			? (N.Men > 0 ? FString::Printf(TEXT("COMING UP %s  \u00B7  %s MEN"), *When, *FText::AsNumber(N.Men).ToString()) : FString::Printf(TEXT("COMING UP %s"), *When))
			: FString::Printf(TEXT("REPORTED  \u00B7  %s"), *When);
		const FSlateFontInfo NameFace = Typed(11.0f, true, 30), UnderFace = Typed(10.5f, true, 0);
		const double CW = FMath::Max(WideOf(Name, NameFace), WideOf(Under, UnderFace)) + 16.0, CH = 38.0;
		const bool bSoon = N.In < 30.0;
		const FLinearColor PaperInk = N.bMine ? Paper : Slate, WordInk = N.bMine ? DeskInk : Cream;
		const FLinearColor SoonInk = !bSoon ? WordInk : N.bMine ? Alarm : Amber;
		double CX = At.X - CW * 0.5, CY = At.Y - CH * 0.5;
		if (C.bThere)
		{
			// (over the place it walks on: a stalk down to a ring on the ground there)
			CY = At.Y - CH - 34.0;
			Lines({ FVector2D(At.X, CY + CH), At }, (N.bMine ? Brass : Slate) * FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), 1.5f, 7);
			TArray<FVector2D> Ring;
			for (int32 K = 0; K <= 16; ++K)
			{
				const double A = 2.0 * UE_DOUBLE_PI * K / 16.0;
				Ring.Add(At + FVector2D(FMath::Cos(A) * 10.0, FMath::Sin(A) * 5.5));
			}
			Lines(Ring, N.bMine ? BrightBrass : Cream, 2.0f, 7);
		}
		else
		{
			// (at the screen's margin: an arrow off the chip's edge, the way to it)
			const FVector2D Way(C.WayX, C.WayY), Across(-C.WayY, C.WayX);
			const double Reach = FMath::Min(FMath::Abs(Way.X) > 1e-6 ? CW * 0.5 / FMath::Abs(Way.X) : 1e9, FMath::Abs(Way.Y) > 1e-6 ? CH * 0.5 / FMath::Abs(Way.Y) : 1e9);
			const FVector2D Base = At + Way * (Reach + 3.0), Tip = Base + Way * 14.0;
			Lines({ Base + Across * 8.0, Tip, Base - Across * 8.0, Base + Across * 8.0 }, N.bMine ? BrightBrass : Cream, 2.5f, 7);
		}
		Fill(CX - 1.0, CY - 1.0, CW + 2.0, CH + 2.0, N.bMine ? Brass : Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f), 7);
		Fill(CX, CY, CW, CH, PaperInk, 8);
		Write(Name, CX + 8.0, CY + 3.0, NameFace, WordInk, 9);
		Write(Under, CX + 8.0, CY + 19.0, UnderFace, SoonInk, 9);
	}

	// (the film in a corner's window, release 2, where it is up: under the three news lines at the left and the setup's
	// words, which end at 128; worked out here so the despatch paper can stand under it)
	UTextureRenderTarget2D* const Film = Scene->CornerPicture();
	const double FilmW = FMath::Clamp(Size.X * 0.22, 240.0, 420.0), FilmH = FilmW * 9.0 / 16.0, FilmX = 18.0, FilmY = FieldTop + 3.0 * 20.0 + 8.0;

	// ---- His despatch riders (the rider ahead, release 2): a motorcycle mark where each rides; a click on one reads out his
	// despatch beside him. ----
	for (const FLandGeneralShown::RiderMark& R : G.RiderMarks)
	{
		const FVector2D At = Local(R.X, R.Y);
		const bool bRead = R.Corps == G.RiderRead;
		const FLinearColor Ink = bRead ? BrightBrass : Brass;
		// (two wheels, the frame between, the man on it)
		TArray<FVector2D> Back, Front;
		for (int32 K = 0; K <= 12; ++K)
		{
			const double A = 2.0 * UE_DOUBLE_PI * K / 12.0;
			Back.Add(At + FVector2D(-7.0 + FMath::Cos(A) * 4.5, FMath::Sin(A) * 4.5));
			Front.Add(At + FVector2D(7.0 + FMath::Cos(A) * 4.5, FMath::Sin(A) * 4.5));
		}
		Fill(At.X - 13.0, At.Y - 16.0, 26.0, 23.0, Plate, 6);
		Lines(Back, Ink, 1.5f, 7);
		Lines(Front, Ink, 1.5f, 7);
		Lines({ At + FVector2D(-7.0, 0.0), At + FVector2D(-1.0, -5.0), At + FVector2D(7.0, 0.0) }, Ink, 1.5f, 7);
		Fill(At.X - 2.5, At.Y - 13.0, 5.0, 6.0, Ink, 7);
		if (bRead && !R.Says.empty())
		{
			const FSlateFontInfo Face = Serif(TEXT("Regular"), 13.0f), Head = Typed(10.5f, true, 30);
			const double Room = 340.0;
			TArray<FString> Said;
			{
				TArray<FString> Pieces;
				Wide(R.Says).ParseIntoArray(Pieces, TEXT(" "), true);
				FString Now;
				for (const FString& Piece : Pieces)
				{
					const FString Tried = Now.IsEmpty() ? Piece : Now + TEXT(" ") + Piece;
					if (!Now.IsEmpty() && WideOf(Tried, Face) > Room)
					{
						Said.Add(Now);
						Now = Piece;
					}
					else
					{
						Now = Tried;
					}
				}
				if (!Now.IsEmpty())
				{
					Said.Add(Now);
				}
			}
			const double CW = Room + 20.0, CH = 30.0 + 19.0 * Said.Num();
			// (on the desk strip under the title band, below the news lines, so the foot stays the cards' alone: the UI
			// director, 18:26Z; under the film's window while that is up)
			const double CX = 18.0, CY = Film ? FilmY + FilmH + 12.0 : FieldTop + 3.0 * 20.0 + 8.0;
			Fill(CX - 1.0, CY - 1.0, CW + 2.0, CH + 2.0, Brass, 10);
			Fill(CX, CY, CW, CH, Paper, 10);
			Write(TEXT("DESPATCH RIDER"), CX + 10.0, CY + 6.0, Head, InkSoft, 11);
			for (int32 K = 0; K < Said.Num(); ++K)
			{
				Write(Said[K], CX + 10.0, CY + 24.0 + 19.0 * K, Face, DeskInk, 11);
			}
		}
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
	if (G.bSmoke)
	{
		// (the smoke's line: grey and broad, as the screen will stand)
		const FVector2D A = Local(G.SmokeEnds[0], G.SmokeEnds[1]), B = Local(G.SmokeEnds[2], G.SmokeEnds[3]);
		Lines({ A, B }, FLinearColor(0.82f, 0.82f, 0.78f, 0.55f), 9.0f, 8);
		Lines({ A, B }, Cream, 1.5f, 9);
	}
	if (G.bGapAt)
	{
		// (where the cutters would go through: the hedge parted, two posts and the way between)
		const FVector2D At = Local(G.GapAtX, G.GapAtY);
		Fill(At.X - 9.0, At.Y - 7.0, 3.0, 14.0, BrightBrass, 9);
		Fill(At.X + 6.0, At.Y - 7.0, 3.0, 14.0, BrightBrass, 9);
		Lines({ FVector2D(At.X - 4.0, At.Y), FVector2D(At.X + 4.0, At.Y) }, Cream, 1.5f, 9);
	}
	if (G.bCarry)
	{
		TArray<FVector2D> Ring;
		for (int32 K = 0; K <= 4; ++K)
		{
			Ring.Add(Local(G.Corners[(K % 4) * 2], G.Corners[(K % 4) * 2 + 1]));
		}
		Lines(Ring, !G.bCarryOk ? Amber : G.bCarryWoods ? Leaf : Cream, 2.5f, 9);
		// (several carried as one, release 2: the others' grounds where they would be set down, a thinner ring each;
		// amber, all of them, where any one of them cannot stand)
		for (size_t MoreAt = 0; MoreAt + 8 <= G.CarryMore.size(); MoreAt += 8)
		{
			TArray<FVector2D> MoreRing;
			for (int32 K = 0; K <= 4; ++K)
			{
				MoreRing.Add(Local(G.CarryMore[MoreAt + (K % 4) * 2], G.CarryMore[MoreAt + (K % 4) * 2 + 1]));
			}
			Lines(MoreRing, !G.bCarryOk ? Amber : Cream, 1.5f, 9);
		}
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

	// ---- The film in a corner (release 2, -TheaterLandCorner; Brian, 24 November 20:38Z): the film director's shot in
	// a small window at the left under the news lines while he commands from his own view, in a brass frame; a click on
	// it, or C, gives the film the whole screen. ----
	if (Film)
	{
		if (CornerBrush.GetResourceObject() != Film)
		{
			CornerBrush.SetResourceObject(Film);
			CornerBrush.ImageSize = FVector2D(480.0f, 270.0f);
			CornerBrush.DrawAs = ESlateBrushDrawType::Image;
		}
		const double Wd = FilmW, Ht = FilmH, X = FilmX, Y = FilmY;
		Fill(X - 2.0, Y - 2.0, Wd + 4.0, Ht + 4.0, Brass, 10);
		Fill(X - 1.0, Y - 1.0, Wd + 2.0, Ht + 2.0, DeskInk, 11);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 12, Geometry.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X, Y))), &CornerBrush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
		// (its name and its key on a small plate at its foot)
		const FSlateFontInfo FilmFace = Typed(10.0f, true, 30);
		const FString FilmWords = TEXT("THE FILM: C FILLS THE SCREEN");
		Fill(X, Y + Ht - 20.0, WideOf(FilmWords, FilmFace) + 14.0, 20.0, Plate, 13);
		Write(FilmWords, X + 7.0, Y + Ht - 18.0, FilmFace, Cream, 14);
		// (its own word, not the FILM buttons' on the bars: the window stands on the field, where only the left
		// button presses it, OnMouseButtonDown)
		Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + Wd), static_cast<float>(Y + Ht)), FString(TEXT("window:film")) });
	}

	// ---- His kept views (release 2; Brian, 24 November 20:38Z, saved views): four small chips under the clock, each
	// with its number: lit where he stands at one, plain where one is kept, faint where none is yet. A click flies
	// there (never kept: it is kept, where his own hand's last move brings the view to rest, LandGeneralEye's
	// KeepView); Shift and a click keeps it anew. F1 to F4, or Option with 1 to 4, do the same. ----
	{
		const FSlateFontInfo ViewsFace = Typed(10.0f, true, 30);
		const double ChipW = 24.0, ChipH = 22.0, ChipGap = 4.0;
		const FString ViewsWord = TEXT("VIEWS");
		const double ChipsW = ChipW * LandGeneralEye::ViewCount + ChipGap * (LandGeneralEye::ViewCount - 1), WordW = WideOf(ViewsWord, ViewsFace);
		double X = Size.X - 22.0 - ChipsW;
		const double Y = 78.0;
		Fill(X - WordW - 17.0, Y - 4.0, WordW + 17.0 + ChipsW + 6.0, ChipH + 8.0, Plate, 10);
		Write(ViewsWord, X - WordW - 9.0, Y + 3.0, ViewsFace, Muted, 11);
		for (int32 K = 0; K < LandGeneralEye::ViewCount; ++K)
		{
			const bool bKept = S.GeneralEye.ViewKept(K), bAt = S.GeneralEye.ViewAt() == K;
			const FLinearColor Ink = bAt ? DeskInk : bKept ? Cream : Muted;
			Fill(X, Y, ChipW, ChipH, bKept ? Brass : Track, 11);
			Fill(X + 1.0, Y + 1.0, ChipW - 2.0, ChipH - 2.0, bAt ? FLinearColor(Brass.R, Brass.G, Brass.B, 1.0f) : Leather, 12);
			const FString Number = FString::FromInt(K + 1);
			Write(Number, X + (ChipW - WideOf(Number, ChipFace)) * 0.5, Y + 3.0, ChipFace, Ink, 13);
			Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(X + ChipW), static_cast<float>(Y + ChipH)), FString::Printf(TEXT("general:view:%d"), K) });
			X += ChipW + ChipGap;
		}
		// (the spotter's mark's lines, under them, while its rings are up: worked out with the rings' words above)
		for (int32 K = 0; K < SpotterLines.Num(); ++K)
		{
			const double LineW = WideOf(SpotterLines[K], SpotterFace), LineY = Y + ChipH + 8.0 + 20.0 * K;
			Fill(Size.X - 22.0 - LineW - 12.0, LineY, LineW + 18.0, 20.0, Plate, 10);
			Write(SpotterLines[K], Size.X - 22.0 - LineW - 3.0, LineY + 2.0, SpotterFace, K == 0 ? Cream : Muted, 11);
		}
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

	// ---- The setup step's word, on the desk strip under the title band: one line in sentence case, then the woods'. ----
	if (Clip.Deploying())
	{
		// (with drawing up on, release 2: the UI director's line of 26 November 2026)
		TArray<FString> Said = { FString(bDrawUp ? TEXT("Place your units on your ground (the brass line): drag one, or draw a box round several and drag them as one, or DRAW UP. The clock waits.")
			: TEXT("Drag your units anywhere on your ground (the brass line), then begin the battle. The clock waits.")) };
		if (!G.WoodsField.empty())
		{
			Said.Add(TEXT("Units in the woods, inside the green lines, start hidden from the enemy until it comes close or they open fire."));
		}
		double SY = 76.0;
		const FSlateFontInfo Face = Serif(TEXT("Regular"), 13.0f);
		for (int32 K = 0; K < Said.Num(); ++K)
		{
			const double WW = WideOf(Said[K], Face);
			Fill(Size.X * 0.5 - WW * 0.5 - 10.0, SY, WW + 20.0, 24.0, Plate, 10);
			Write(Said[K], Size.X * 0.5 - WW * 0.5, SY + 3.0, Face, K == 0 ? BrightBrass : Leaf, 11);
			SY += 26.0;
		}
	}

	// ---- After the break (release 2, the battle rule `pursue`; Brian, 26 November 2026 03:53Z: "if the enemy retreats,
	// i have the option to continue on with the battle or end it"). Once the clock shown has come up to the break: while
	// he is asked, one line on the desk strip and his two answers under it, side by side, PURSUE lit; while his pursuit
	// runs, CALL OFF THE PURSUIT there; where his own army broke and the enemy pursues, one line. The rules decide what
	// each press does and refuse what is not his to give (sim.ts, "After the break"). The words are the UI director's
	// (26 November 2026, 13:32Z; ui-director/battle-pursue-UI-MARKS.md, A to D): the other army by the rules' own name
	// for it, and END THE BATTLE and CALL OFF THE PURSUIT with no key, since a press of either cannot be taken back.
	// The battle's buttons take no tips. ----
	if (S.Rec.bLive && !S.Rec.bOver && S.Rec.After != 0 && S.Rec.Me >= 0 && S.Battle >= S.Rec.AfterAt - 1e-6)
	{
		const bool bAskedNow = S.Rec.After == 1 && S.Rec.AfterWinner == S.Rec.Me;
		const bool bPursuingNow = S.Rec.After == 2 && S.Rec.AfterWinner == S.Rec.Me;
		const bool bPursuedNow = S.Rec.After == 2 && S.Rec.AfterBroke == S.Rec.Me;
		const FSlateFontInfo AfterFace = Serif(TEXT("Regular"), 13.0f);
		// (the day's clock ends a pursuit, so while one runs the line says how long the day has left; where the rules
		// give the pursuit a clock of its own, the row's "until", how long that has left)
		// (her words for that case, 18:12Z: "at the latest", since the field's own ends can come first)
		const bool bOwnClock = S.Rec.AfterUntil > 0.0;
		const int32 DayLeft = FMath::Max(0, static_cast<int32>(FMath::CeilToDouble((bOwnClock ? S.Rec.AfterUntil : S.Rec.Limit) - S.Battle)));
		// (the army that broke opens its line as the rules name it; the one that pursues stands inside its line, so its
		// first letter goes small: "the German army")
		const FString BrokeName = S.Rec.AfterBrokeName.empty() ? FString(TEXT("The enemy army")) : Wide(S.Rec.AfterBrokeName);
		FString ByName = S.Rec.AfterWinnerName.empty() ? FString(TEXT("the enemy army")) : Wide(S.Rec.AfterWinnerName);
		ByName = ByName.Left(1).ToLower() + ByName.Mid(1);
		const FString AfterWord = bAskedNow ? FString::Printf(TEXT("%s has broken. Pursue it, or end the battle now. The clock stands until you choose."), *BrokeName)
			: bPursuingNow ? (bOwnClock ? FString::Printf(TEXT("Your army pursues. The pursuit ends in %d:%02d at the latest."), DayLeft / 60, DayLeft % 60)
				: FString::Printf(TEXT("Your army pursues. The day ends in %d:%02d."), DayLeft / 60, DayLeft % 60))
			: bPursuedNow ? (bOwnClock ? FString::Printf(TEXT("Your army has broken and falls back to your own edge of the field, with %s in pursuit. The pursuit ends in %d:%02d at the latest."), *ByName, DayLeft / 60, DayLeft % 60)
				: FString::Printf(TEXT("Your army has broken and falls back to your own edge of the field, with %s in pursuit. The day ends in %d:%02d."), *ByName, DayLeft / 60, DayLeft % 60)) : FString();
		double AfterY = 76.0;
		if (!AfterWord.IsEmpty())
		{
			const double AfterWide = WideOf(AfterWord, AfterFace);
			Fill(Size.X * 0.5 - AfterWide * 0.5 - 10.0, AfterY, AfterWide + 20.0, 24.0, Plate, 10);
			Write(AfterWord, Size.X * 0.5 - AfterWide * 0.5, AfterY + 3.0, AfterFace, BrightBrass, 11);
			AfterY += 30.0;
		}
		if (bAskedNow && S.AfterSent != S.Rec.After)
		{
			const double PursueW = ButtonWidth(TEXT("PURSUE"), TEXT("P")), EndW = ButtonWidth(TEXT("END THE BATTLE"), TEXT(""));
			const double AfterX = Size.X * 0.5 - (PursueW + 6.0 + EndW) * 0.5;
			Button(TEXT("PURSUE"), TEXT("P"), TEXT("general:pursue"), AfterX, AfterY, true, false, 12);
			Button(TEXT("END THE BATTLE"), TEXT(""), TEXT("general:endbattle"), AfterX + PursueW + 6.0, AfterY, false, false, 12);
		}
		else if (bPursuingNow && S.AfterSent != S.Rec.After)
		{
			const double OffW = ButtonWidth(TEXT("CALL OFF THE PURSUIT"), TEXT(""));
			Button(TEXT("CALL OFF THE PURSUIT"), TEXT(""), TEXT("general:endbattle"), Size.X * 0.5 - OffW * 0.5, AfterY, false, false, 12);
		}
	}

	// ---- The foot: his units' cards in one row (never wrapping: it scrolls), and at its right end BEGIN THE BATTLE while he
	// deploys, and the clock's and the camera's buttons where the top bar has no room for them. ----
	double RowRight = FootRight;
	{
		struct FPart { const TCHAR* Label; const TCHAR* Key; const TCHAR* Action; bool bLit; };
		TArray<FPart> Ends;
		if (Clip.Deploying())
		{
			Button(TEXT("BEGIN THE BATTLE"), TEXT("SPACE"), TEXT("general:begin"), FootRight - ButtonWidth(TEXT("BEGIN THE BATTLE"), TEXT("SPACE")), OrdersY, true, false, 12);
		}
		if (!bControlsOnTop)
		{
			if (!Clip.Deploying())
			{
				// (groups on the number keys, release 2: the pace's buttons lose their number marks and one small muted
				// mark of the minus and plus keys stands after them: a part with no action, words only)
				Ends.Add({ TEXT("PAUSE"), TEXT("SPACE"), TEXT("speed:0"), S.SpeedNow <= 0.0 });
				Ends.Add({ TEXT("\u00D71"), bGroupKeys ? TEXT("") : TEXT("1"), TEXT("speed:1"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 1.0) });
				Ends.Add({ TEXT("\u00D72"), bGroupKeys ? TEXT("") : TEXT("2"), TEXT("speed:2"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 2.0) });
				Ends.Add({ TEXT("\u00D74"), bGroupKeys ? TEXT("") : TEXT("3"), TEXT("speed:4"), S.SpeedNow > 0.0 && FMath::IsNearlyEqual(S.Speed, 4.0) });
				if (bGroupKeys)
				{
					Ends.Add({ TEXT("\u2212  +"), TEXT(""), TEXT(""), false });
				}
			}
			Ends.Add({ TEXT("FILM"), TEXT("C"), TEXT("general:film"), false });
			Ends.Add({ TEXT("TOP VIEW"), TEXT("O"), TEXT("general:topview"), S.GeneralEye.TopView() });
		}
		// (the map of the field folded away leaves its button at the row's right end, wherever the clock's buttons stand)
		if (bFieldMap && bMapFolded)
		{
			Ends.Add({ TEXT("FIELD MAP"), TEXT("M"), TEXT("map:fold"), false });
		}
		double Whole = 0.0;
		for (const FPart& P : Ends)
		{
			Whole += (P.Action[0] ? ButtonWidth(P.Label, P.Key) : WideOf(P.Label, BarFace)) + 6.0;
		}
		double X = FootRight - Whole + 6.0;
		RowRight = Ends.Num() > 0 ? X - 10.0 : FootRight;
		for (const FPart& P : Ends)
		{
			if (!P.Action[0])
			{
				Write(P.Label, X, RowY + (CardH - 28.0) * 0.5 + 5.0, BarFace, Muted, 14);
				X += WideOf(P.Label, BarFace) + 6.0;
				continue;
			}
			X = Button(P.Label, P.Key, P.Action, X, RowY + (CardH - 28.0) * 0.5, P.bLit, false, 12);
		}
	}
	double CardsH = Size.Y - RowY;
	{
		TArray<const LandCommand::Tag*> Mine;
		for (const LandCommand::Tag& T : Clip.Tags())
		{
			if (T.bMine)
			{
				Mine.Add(&T);
			}
		}
		const int32 Count = Mine.Num();
		const double ArrowW = 28.0;
		double Room = RowRight - 18.0;
		double CW = Count > 0 ? FMath::Clamp((Room - CardGap * (Count - 1)) / Count, 112.0, 178.0) : 178.0;
		int32 Fit = FMath::Max(1, static_cast<int32>((Room + CardGap) / (CW + CardGap)));
		int32 First = 0;
		const bool bScroll = Fit < Count;
		if (bScroll)
		{
			Room -= 2.0 * (ArrowW + CardGap);
			Fit = FMath::Max(1, static_cast<int32>((Room + CardGap) / (CW + CardGap)));
			First = FMath::Clamp(CardFirst, 0, Count - Fit);
		}
		CardFirst = First;
		// (the UI director, 20:52Z: line 1 the full name alone, bold at about 13 px as Brian sees it; line 2 the kind word at
		// about 12, and at its right the command chip and at most one mark of how it stands)
		const FSlateFontInfo NameFace = Typed(9.5f, true, 0), SmallNameFace = Typed(8.0f, true, 0), KindFace = Typed(8.8f, false, 0);
		double X = 18.0;
		auto Arrow = [&](bool bBack, double AX, bool bLive)
		{
			Fill(AX, RowY, ArrowW, CardH, bLive ? Brass : Track, 12);
			Fill(AX + 1.0, RowY + 1.0, ArrowW - 2.0, CardH - 2.0, Leather, 13);
			const double MX = AX + ArrowW * 0.5, MY = RowY + CardH * 0.5, D = bBack ? -1.0 : 1.0;
			Lines({ FVector2D(MX - 4.0 * D, MY - 8.0), FVector2D(MX + 4.0 * D, MY), FVector2D(MX - 4.0 * D, MY + 8.0) }, bLive ? BrightBrass : Muted, 2.0f, 14);
			if (bLive)
			{
				Buttons.Add({ FSlateRect(static_cast<float>(AX), static_cast<float>(RowY), static_cast<float>(AX + ArrowW), static_cast<float>(RowY + CardH)), bBack ? FString(TEXT("cards:back")) : FString(TEXT("cards:next")) });
			}
		};
		if (bScroll)
		{
			Arrow(true, X, First > 0);
			X += ArrowW + CardGap;
		}
		for (int32 K = First; K < FMath::Min(Count, First + Fit); ++K)
		{
			const LandCommand::Tag& T = *Mine[K];
			const bool bGone = !T.bOrderable;
			if (T.bChosen)
			{
				Fill(X - 2.0, RowY - 2.0, CW + 4.0, CardH + 4.0, BrightBrass, 12);
			}
			Fill(X, RowY, CW, CardH, bGone ? Track : Paper, 13);
			// (its group's number, release 2: a small chip at the card's top left, its name after it; a click on the chip
			// takes the whole group in hand)
			const double TiedW = bGroupKeys && T.Group > 0 ? 19.0 : 0.0;
			if (TiedW > 0.0)
			{
				TiedChip(T.Group, X + 4.0, RowY + 3.0, 14);
			}
			// (line 1: its name and who commands it; line 2: its kind and how it stands)
			FString OwnerWords, StateWords;
			FLinearColor OwnerPaper = Track, OwnerInk = Cream, StatePaper = Track, StateInk = Cream;
			double Air = -1.0;
			if (!bGone)
			{
				OwnerOf(T, OwnerWords, OwnerPaper, OwnerInk, Air);
			}
			StateOf(T, StateWords, StatePaper, StateInk);
			const double ChipH = 18.0, LineY = RowY + 22.0;
			// Line 1: its full name, never cut. A name wider than the card steps down to about 11 px first, still over the
			// floor; only one too wide at 11 takes the army's short words (the UI director, 20:56Z).
			FString Name = Wide(T.Short.empty() ? T.Name : T.Short).ToUpper();
			FSlateFontInfo LineFace = NameFace;
			if (WideOf(Name, LineFace) > CW - 12.0 - TiedW)
			{
				LineFace = SmallNameFace;
			}
			if (WideOf(Name, LineFace) > CW - 12.0 - TiedW)
			{
				static const TCHAR* const Shorter[][2] = { { TEXT("MACHINE-GUN"), TEXT("MG") }, { TEXT("PANZERGRENADIER"), TEXT("PZ.GREN.") }, { TEXT("INFANTRY"), TEXT("INF.") },
					{ TEXT("ARMORED"), TEXT("ARMD.") }, { TEXT("ARMOURED"), TEXT("ARMD.") }, { TEXT("ARTILLERY"), TEXT("ARTY.") }, { TEXT("MOTORIZED"), TEXT("MOT.") },
					{ TEXT("MOTORISED"), TEXT("MOT.") }, { TEXT("MOUNTAIN"), TEXT("MTN.") }, { TEXT("DIVISION"), TEXT("DIV.") }, { TEXT("REGIMENT"), TEXT("REGT.") },
					{ TEXT("BRIGADE"), TEXT("BDE.") }, { TEXT("BATTALION"), TEXT("BN.") } };
				for (const auto& Pair : Shorter)
				{
					if (WideOf(Name, LineFace) <= CW - 12.0 - TiedW)
					{
						break;
					}
					Name.ReplaceInline(Pair[0], Pair[1], ESearchCase::CaseSensitive);
				}
			}
			Write(Name, X + 6.0 + TiedW, RowY + 4.0 + (LineFace.Size < NameFace.Size ? 1.0 : 0.0), LineFace, bGone ? Muted : DeskInk, 14);
			// Line 2, from the right: who commands it, then at most one mark of how it stands (gone, running, help late or
			// dug in as a chip; help coming as the hourglass and its time; else hidden in the woods), then its kind.
			double Right = X + CW - 4.0;
			if (!OwnerWords.IsEmpty() || Air >= 0.0)
			{
				const double OwnerW = DrawChip(OwnerWords, OwnerPaper, OwnerInk, Air, 0.0, 0.0, ChipH, false, 0);
				DrawChip(OwnerWords, OwnerPaper, OwnerInk, Air, Right - OwnerW, LineY, ChipH, true, 14);
				Right -= OwnerW + 4.0;
			}
			if (StateWords.StartsWith(TEXT("HELP IN ")))
			{
				const FString Time = StateWords.RightChop(8);
				const double MarkW = 15.0 + WideOf(Time, ChipFace) + 8.0;
				Fill(Right - MarkW, LineY, MarkW, ChipH, StatePaper, 14);
				KitSign(TEXT("hourglass"), Right - MarkW + 2.0, LineY + 2.0, 14.0, StateInk, 15);
				Write(Time, Right - MarkW + 16.0, LineY + (ChipH - 17.0) * 0.5, ChipFace, StateInk, 15);
				Right -= MarkW + 4.0;
			}
			else if (!StateWords.IsEmpty())
			{
				const double StateW = DrawChip(StateWords, StatePaper, StateInk, -1.0, 0.0, 0.0, ChipH, false, 0);
				DrawChip(StateWords, StatePaper, StateInk, -1.0, Right - StateW, LineY, ChipH, true, 14);
				Right -= StateW + 4.0;
			}
			else if (T.bHidden)
			{
				// (hidden in woods from the enemy: Brian's forest mark on the tile, 22 November 04:19Z)
				Forest(Right - 18.0, LineY, 18.0, LeafInk, 14);
				Right -= 22.0;
			}
			// (once the battle runs, what it is doing; else, or where neither its words nor their short form fit, its kind's
			// word, or where the line has no room for that, the kit's sign: the plate still names it)
			const TCHAR* DoingShort = nullptr;
			bool bBusy = true;
			const TCHAR* Doing = Clip.Deploying() || !StateWords.IsEmpty() || bGone ? nullptr : DoingWords(T, DoingShort, bBusy);
			const TCHAR* DoingFits = Doing && WideOf(Doing, KindFace) <= Right - (X + 6.0) ? Doing
				: DoingShort && WideOf(DoingShort, KindFace) <= Right - (X + 6.0) ? DoingShort : nullptr;
			const FString Kind = KindWord(T);
			if (DoingFits)
			{
				Write(DoingFits, X + 6.0, LineY + 1.0, KindFace, bBusy ? DeskInk : InkSoft, 14);
			}
			else if (WideOf(Kind, KindFace) <= Right - (X + 6.0))
			{
				Write(Kind, X + 6.0, LineY + 1.0, KindFace, InkSoft, 14);
			}
			else if (Right - (X + 6.0) >= 16.0)
			{
				KitSign(KindSign(T), X + 5.0, LineY + 1.0, 16.0, InkSoft, 14);
			}
			Fill(X, RowY + CardH - 3.0, CW, 3.0, Leather, 14);
			Fill(X, RowY + CardH - 3.0, CW * FMath::Clamp(T.Str, 0.0, 1.0), 3.0, T.Mor < 0.4 ? Amber : Brass, 15);
			if (T.bOrderable)
			{
				Buttons.Add({ FSlateRect(static_cast<float>(X), static_cast<float>(RowY), static_cast<float>(X + CW), static_cast<float>(RowY + CardH)), FString::Printf(TEXT("pick:%d"), T.Block) });
				// (the chip's own button, after the card's so that it is the one a press on it finds)
				if (TiedW > 0.0)
				{
					Buttons.Add({ FSlateRect(static_cast<float>(X + 2.0), static_cast<float>(RowY + 1.0), static_cast<float>(X + 22.0), static_cast<float>(RowY + 21.0)), FString::Printf(TEXT("general:group:%d"), T.Group) });
				}
			}
			X += CW + CardGap;
		}
		if (bScroll)
		{
			Arrow(false, X, First + Fit < Count);
		}
	}

	// ---- While he places his army: SEND A PATROL for men on foot in his hand, where he attacks and night patrols are on
	// (release 2; Brian, 22 November 17:33Z), at the left of the bar BEGIN THE BATTLE stands on. ----
	if (bHand && Clip.Deploying() && Clip.Side() == 0 && !Clip.Waiters().empty() && FLandLive::RuleNow(TEXT("patrol")) && FLandLive::RuleNow(TEXT("hide")))
	{
		Button(TEXT("SEND A PATROL TONIGHT"), TEXT(""), TEXT("general:patrol"), 18.0, OrdersY, false, false, 12);
	}

	// ---- While he places his army: DRAW UP (release 2; Brian, 26 November 2026: "i should be able to move my units in a
	// formation before the battle begins during the deployed phase"), on the bar's row between SEND A PATROL TONIGHT and
	// BEGIN THE BATTLE: a small muted label with its key, then LINE, TWO LINES, WEDGE, COLUMN. With units in hand they
	// act on those, with none on all of his; the one last drawn is lit until he drags a unit by hand. Under the pointer
	// a button says what a click would do, in a sentence above the row. Words and places the UI director's (26
	// November 2026, 03:51Z and 04:22Z). ----
	if (bDrawUp)
	{
		struct FDrawn { const TCHAR* Label; const char* Kind; const TCHAR* Action; const TCHAR* Would; };
		static const FDrawn Drawn[] = {
			{ TEXT("LINE"), "line", TEXT("general:drawup:line"), TEXT("Draw up {who} side by side, facing the enemy, {guns} behind.") },
			{ TEXT("TWO LINES"), "two", TEXT("general:drawup:two"), TEXT("Draw up {who} in two lines, one behind the other, {guns} behind.") },
			{ TEXT("WEDGE"), "wedge", TEXT("general:drawup:wedge"), TEXT("Draw up {who} in a wedge, its point toward the enemy, {guns} behind.") },
			{ TEXT("COLUMN"), "column", TEXT("general:drawup:column"), TEXT("Draw up {who} one behind another, {guns} at the rear.") } };
		const FString DrawWord = TEXT("DRAW UP"), DrawKey = TEXT("U"), NoKey = FString();
		const double DrawCapW = WideOf(DrawKey, CapFace) + 10.0;
		const double DrawLabelW = WideOf(DrawWord, BarFace) + 8.0 + DrawCapW + 12.0;
		double DrawW = DrawLabelW - 6.0;
		for (const FDrawn& Form : Drawn)
		{
			DrawW += ButtonWidth(Form.Label, NoKey) + 6.0;
		}
		// (centred between the patrol's button, its room kept whether it shows or not, and BEGIN THE BATTLE)
		const double DrawLeft = 18.0 + ButtonWidth(TEXT("SEND A PATROL TONIGHT"), NoKey) + 14.0;
		const double DrawRight = FootRight - ButtonWidth(TEXT("BEGIN THE BATTLE"), TEXT("SPACE")) - 14.0;
		const double DrawX = FMath::Max(DrawLeft, (DrawLeft + DrawRight - DrawW) * 0.5);
		// (the label and its key's cap on a plate, muted: it names the four, it is not a button)
		Fill(DrawX - 8.0, OrdersY, DrawLabelW - 2.0, 28.0, Plate, 12);
		Write(DrawWord, DrawX, OrdersY + 5.0, BarFace, Muted, 13);
		{
			const double CapX = DrawX + WideOf(DrawWord, BarFace) + 8.0;
			Fill(CapX, OrdersY + 4.0, DrawCapW, 20.0, Muted * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 13);
			Fill(CapX + 1.0, OrdersY + 5.0, DrawCapW - 2.0, 18.0, Leather, 14);
			Write(DrawKey, CapX + 5.0, OrdersY + 5.0, CapFace, Muted, 15);
		}
		const FDrawn* OverForm = nullptr;
		double FormX = DrawX + DrawLabelW;
		for (const FDrawn& Form : Drawn)
		{
			const double FormW = ButtonWidth(Form.Label, NoKey);
			if (GeneralHeld < 0 && HoverAt.X >= FormX && HoverAt.X < FormX + FormW && HoverAt.Y >= OrdersY && HoverAt.Y < OrdersY + 28.0)
			{
				OverForm = &Form;
			}
			FormX = Button(Form.Label, NoKey, Form.Action, FormX, OrdersY, G.DrawnUp == Form.Kind, false, 12);
		}
		if (OverForm)
		{
			// ("those in hand", or "your units" with none in hand; "and support teams" only where his army has them)
			bool bTeams = false;
			for (const LandCommand::Tag& Each : Clip.Tags())
			{
				bTeams = bTeams || (Each.bMine && !Each.Team.empty());
			}
			const FString Would = FString(OverForm->Would).Replace(TEXT("{who}"), bHand ? TEXT("those in hand") : TEXT("your units"), ESearchCase::CaseSensitive)
				.Replace(TEXT("{guns}"), bTeams ? TEXT("guns and support teams") : TEXT("guns"), ESearchCase::CaseSensitive);
			const FSlateFontInfo WouldFace = Serif(TEXT("Regular"), 13.0f);
			const double WouldW = WideOf(Would, WouldFace);
			const double WouldX = FMath::Clamp(DrawX + DrawW * 0.5 - WouldW * 0.5, 28.0, FMath::Max(28.0, FootRight - 10.0 - WouldW));
			Fill(WouldX - 10.0, OrdersY - 30.0, WouldW + 20.0, 24.0, Plate, 15);
			Write(Would, WouldX, OrdersY - 27.0, WouldFace, BrightBrass, 16);
		}
	}

	// ---- The orders of the unit in hand, just above the cards, only while one is in hand and only those it can give
	// (the UI director, 18:26Z: none dimmed, CUT A GAP, LAY SMOKE and HOLD TILL RELIEVED only where they can be used). ----
	if (bHand && !Clip.Deploying())
	{
		struct FPart { const TCHAR* Label; const TCHAR* Key; const TCHAR* Action; bool bLit; };
		TArray<FPart> Parts;
		Parts.Add({ TEXT("AT THE DOUBLE"), TEXT("G"), TEXT("general:double"), G.Armed == "double" });
		Parts.Add({ TEXT("HOLD"), TEXT("H"), TEXT("general:hold"), false });
		Parts.Add({ TEXT("FIRE ON"), TEXT("F"), TEXT("general:fire"), G.Armed == "fire" });
		// (a tank with hedge cutters, a section with smoke left; Brian, 22 November 14:19Z)
		if (Clip.Cutter() >= 0)
		{
			Parts.Add({ TEXT("CUT A GAP"), TEXT("X"), TEXT("general:gap"), G.Armed == "gap" });
		}
		if (!Clip.Smokers().empty())
		{
			Parts.Add({ TEXT("LAY SMOKE"), TEXT("K"), TEXT("general:smoke"), G.Armed == "smoke" });
		}
		Parts.Add({ TEXT("FALL BACK"), TEXT("R"), TEXT("general:fallback"), false });
		// (hold until relieved: while a corps of his is coming up from next door, for the units on foot in hand)
		if (Clip.HelpComing() && !Clip.Waiters().empty())
		{
			Parts.Add({ TEXT("HOLD TILL RELIEVED"), TEXT("T"), TEXT("general:wait"), false });
		}
		Parts.Add({ TEXT("BACK TO STAFF"), TEXT("B"), TEXT("general:staff"), false });
		// (the film with the unit in his hand: Ride with this unit)
		Parts.Add({ TEXT("RIDE WITH THEM"), TEXT("V"), TEXT("general:ride"), false });
		// (every order has its key, so a common order is one press with the mouse on the field, 22 November 23:20Z; where
		// the bar would run past the screen, the key marks go and the keys still work)
		double Whole = 0.0;
		for (const FPart& P : Parts)
		{
			Whole += ButtonWidth(P.Label, P.Key) + 6.0;
		}
		if (Whole > FootRight - 18.0)
		{
			Whole = 0.0;
			for (FPart& P : Parts)
			{
				P.Key = TEXT("");
				Whole += ButtonWidth(P.Label, P.Key) + 6.0;
			}
		}
		double X = FMath::Max(18.0, FootRight - Whole + 6.0);
		for (const FPart& P : Parts)
		{
			X = Button(P.Label, P.Key, P.Action, X, OrdersY, P.bLit, false, 12);
		}
		CardsH = Size.Y - OrdersY;
	}

	// ---- The map of the field in the corner (release 2, its own Options row; Brian, 26 November 2026: "its would be
	// helpful to see a small map of the battle on the bottom right to know where everyone is"): the field small on the
	// screen's plate in a brass hairline, its woods, water and buildings, his ground while he places his army, his units
	// at their frontage and facing (those in hand bright, with a cream edge), the enemy the field shows him in slate,
	// and the outline of what his view sees. A click glides his view there and a drag takes it with the pointer; the
	// chevron at its top right, or M, folds it away. The UI director's marks (26 November 2026, 03:51Z); what it shows
	// is the design director's ruling (03:50Z): the player's sight and nothing more (LandCommand::General::OnMap). The
	// field keeps no mark of an enemy gone from sight, so the map has none to show. ----
	if (MapFrame.Valid())
	{
		const double MX = MapFrame.X, MY = MapFrame.Y, MW = MapFrame.W, MH = MapFrame.H;
		const int32 MapLayer = 18;
		const LandFeed::Field& Land = S.Rec.Ground;
		Fill(MX - 1.0, MY - 1.0, MW + 2.0, MH + 2.0, Plate, MapLayer);
		const FLinearColor Hair = Brass * FLinearColor(1.0f, 1.0f, 1.0f, 0.4f);
		Fill(MX - 1.0, MY - 1.0, MW + 2.0, 1.0, Hair, MapLayer + 1);
		Fill(MX - 1.0, MY + MH, MW + 2.0, 1.0, Hair, MapLayer + 1);
		Fill(MX - 1.0, MY, 1.0, MH, Hair, MapLayer + 1);
		Fill(MX + MW, MY, 1.0, MH, Hair, MapLayer + 1);
		Fill(MX, MY, MW, MH, Leather, MapLayer + 1);
		// (its woods, water and buildings as runs of small squares: made anew only where the map's shape or the field
		// under it is another, told by a count over the field's cells each frame, FNV-1a)
		{
			uint64 Told = 14695981039346656037ull;
			auto Tell = [&Told](uint64 Part) { Told = (Told ^ Part) * 1099511628211ull; };
			Tell(static_cast<uint64>(static_cast<int64>(MW * 64.0)));
			Tell(static_cast<uint64>(static_cast<int64>(MH * 64.0)));
			Tell(MapFrame.bTurned ? 1ull : 0ull);
			Tell(static_cast<uint64>(Land.Cols));
			Tell(static_cast<uint64>(Land.Rows));
			Tell(static_cast<uint64>(Land.Grounds.size()));
			Tell(static_cast<uint64>(Land.Ground.size()));
			Tell(static_cast<uint64>(Land.Storeys.size()));
			for (const uint8 Kind : Land.Ground)
			{
				Tell(static_cast<uint64>(Kind));
			}
			for (const uint8 Tall : Land.Storeys)
			{
				Tell(Tall > 0 ? 1ull : 0ull);
			}
			if (Told != MapPaintOf)
			{
				MapPaint = LandFieldMap::Paints(Land, MapFrame);
				MapPaintOf = Told;
			}
		}
		const FLinearColor WoodsInk = LeafInk * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);
		for (const LandFieldMap::Run& One : MapPaint.Runs)
		{
			double RunX = 0.0, RunY = 0.0, RunW = 0.0, RunH = 0.0;
			LandFieldMap::RunBox(MapPaint, MapFrame, One, RunX, RunY, RunW, RunH);
			if (RunW > 0.0 && RunH > 0.0)
			{
				Fill(RunX, RunY, RunW, RunH, One.What == LandFieldMap::Paint::Water ? MapWater : One.What == LandFieldMap::Paint::Building ? InkSoft : WoodsInk, MapLayer + 2);
			}
		}
		// (his ground while he places his army: the brass line, 1 wide, held to the map's box)
		if (Clip.Deploying())
		{
			const double* Room = Clip.Room();
			if (Room[2] > Room[0] && Room[3] > Room[1])
			{
				double NearX = 0.0, NearY = 0.0, FarX = 0.0, FarY = 0.0;
				MapFrame.ToMap(Room[0], Room[1], NearX, NearY);
				MapFrame.ToMap(Room[2], Room[3], FarX, FarY);
				const double RoomL = FMath::Clamp(FMath::Min(NearX, FarX), MX, MX + MW - 1.0), RoomR = FMath::Clamp(FMath::Max(NearX, FarX), MX + 1.0, MX + MW);
				const double RoomT = FMath::Clamp(FMath::Min(NearY, FarY), MY, MY + MH - 1.0), RoomB = FMath::Clamp(FMath::Max(NearY, FarY), MY + 1.0, MY + MH);
				Fill(RoomL, RoomT, RoomR - RoomL, 1.0, Brass, MapLayer + 3);
				Fill(RoomL, RoomB - 1.0, RoomR - RoomL, 1.0, Brass, MapLayer + 3);
				Fill(RoomL, RoomT + 1.0, 1.0, FMath::Max(0.0, RoomB - RoomT - 2.0), Brass, MapLayer + 3);
				Fill(RoomR - 1.0, RoomT + 1.0, 1.0, FMath::Max(0.0, RoomB - RoomT - 2.0), Brass, MapLayer + 3);
			}
		}
		// (a block's mark: a stroke along its frontage, as thick as it is deep, never smaller than LandFieldMap's least;
		// Edge more all round, for the cream edge under a unit in hand; its ends held to the map's box)
		auto Stroke = [&](double FieldX, double FieldY, double Front, double Depth, double Facing, double Edge, const FLinearColor& Ink, int32 Layer)
		{
			double Ax = 0.0, Ay = 0.0, Bx = 0.0, By = 0.0, Thick = 0.0;
			LandFieldMap::Mark(MapFrame, FieldX, FieldY, Front, Depth, Facing, Ax, Ay, Bx, By, Thick);
			const double Long = FMath::Sqrt((Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay));
			if (Edge > 0.0 && Long > 1e-6)
			{
				const double Ux = (Bx - Ax) / Long, Uy = (By - Ay) / Long;
				Ax -= Ux * Edge;
				Ay -= Uy * Edge;
				Bx += Ux * Edge;
				By += Uy * Edge;
			}
			const FVector2D EndA(FMath::Clamp(Ax, MX, MX + MW), FMath::Clamp(Ay, MY, MY + MH)), EndB(FMath::Clamp(Bx, MX, MX + MW), FMath::Clamp(By, MY, MY + MH));
			Lines({ EndA, EndB }, Ink, static_cast<float>(Thick + 2.0 * Edge), Layer);
		};
		for (const LandCommand::Tag* Each : Clip.OnMap())
		{
			if (!Each->bMine && !(Each->bKnown && !Each->bVague))
			{
				// (an enemy not yet met, or found only by its fire: a hollow slate outline of the least size, level, 1
				// wide, where the field shows it, so that it never reads as a unit he has seen: the UI director, 05:35Z)
				double AtX = 0.0, AtY = 0.0;
				MapFrame.ToMap(Each->X, Each->Y, AtX, AtY);
				const double BoxW = LandFieldMap::MarkLong, BoxH = LandFieldMap::MarkDeep;
				const double BoxX = FMath::Clamp(FMath::RoundToDouble(AtX - 0.5 * BoxW), MX, FMath::Max(MX, MX + MW - BoxW));
				const double BoxY = FMath::Clamp(FMath::RoundToDouble(AtY - 0.5 * BoxH), MY, FMath::Max(MY, MY + MH - BoxH));
				Fill(BoxX, BoxY, BoxW, 1.0, Slate, MapLayer + 4);
				Fill(BoxX, BoxY + BoxH - 1.0, BoxW, 1.0, Slate, MapLayer + 4);
				Fill(BoxX, BoxY + 1.0, 1.0, BoxH - 2.0, Slate, MapLayer + 4);
				Fill(BoxX + BoxW - 1.0, BoxY + 1.0, 1.0, BoxH - 2.0, Slate, MapLayer + 4);
			}
			else if (!Each->bMine)
			{
				// (the enemy he has seen, in slate, at its frontage and facing)
				Stroke(Each->X, Each->Y, Each->W, Each->D, Each->Face, 0.0, Slate, MapLayer + 4);
			}
			else if (!Each->bChosen)
			{
				Stroke(Each->X, Each->Y, Each->W, Each->D, Each->Face, 0.0, Brass, MapLayer + 5);
			}
			else
			{
				Stroke(Each->X, Each->Y, Each->W, Each->D, Each->Face, 1.0, Cream, MapLayer + 6);
				Stroke(Each->X, Each->Y, Each->W, Each->D, Each->Face, 0.0, BrightBrass, MapLayer + 7);
			}
		}
		// (where he is looking: the ground under his screen's four corners, cut to the map's box, in cream)
		if (G.MapView.size() >= 6)
		{
			std::vector<std::pair<double, double>> Corners;
			for (std::size_t K = 0; K + 1 < G.MapView.size(); K += 2)
			{
				Corners.push_back({ G.MapView[K], G.MapView[K + 1] });
			}
			TArray<FVector2D> Seen;
			for (const std::pair<double, double>& Point : LandFieldMap::Outline(MapFrame, Corners))
			{
				Seen.Add(FVector2D(Point.first, Point.second));
			}
			Lines(Seen, Cream * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 1.0f, MapLayer + 8);
		}
		// (the fold's chevron and its key's cap on a small plate at the map's top right)
		const FString FoldKey = TEXT("M");
		const double FoldCapW = WideOf(FoldKey, CapFace) + 10.0;
		const double FoldW = 6.0 + 9.0 + 5.0 + FoldCapW + 3.0, FoldH = 24.0;
		const double FoldX = MX + MW - FoldW - 2.0, FoldY = MY + 2.0;
		Fill(FoldX, FoldY, FoldW, FoldH, Plate, MapLayer + 9);
		Lines({ FVector2D(FoldX + 6.0, FoldY + 9.5), FVector2D(FoldX + 10.5, FoldY + 14.5), FVector2D(FoldX + 15.0, FoldY + 9.5) }, Cream, 1.5f, MapLayer + 10);
		Fill(FoldX + 20.0, FoldY + 2.0, FoldCapW, 20.0, Muted * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), MapLayer + 10);
		Fill(FoldX + 21.0, FoldY + 3.0, FoldCapW - 2.0, 18.0, Leather, MapLayer + 11);
		Write(FoldKey, FoldX + 25.0, FoldY + 3.0, CapFace, Muted, MapLayer + 12);
		// (the whole map looks where it is pressed; the fold's plate, added after it, is taken first)
		Buttons.Add({ FSlateRect(static_cast<float>(MX), static_cast<float>(MY), static_cast<float>(MX + MW), static_cast<float>(MY + MH)), FString(TEXT("map:look")) });
		Buttons.Add({ FSlateRect(static_cast<float>(FoldX - 2.0), static_cast<float>(MY), static_cast<float>(MX + MW), static_cast<float>(FoldY + FoldH + 2.0)), FString(TEXT("map:fold")) });
	}

	// ---- The plate of the unit under the pointer: its whole name, what it belongs to, its real men and the drawing ratio
	// (the historian, 22 November 05:50Z, on Brian's 04:14Z rule: what is on the screen reflects the real numbers, the ratio
	// said where it is not full scale). An enemy not met says nothing. On paper, as the map's hover cards, above its tag or
	// beside it, never over it (the UI director, 18:26Z). ----
	const LandCommand::Tag* Over = HoverBlock >= 0 && GeneralHeld < 0 ? Clip.TagOf(HoverBlock) : nullptr;
	FVector2D PlateAt = HoverAt;
	// (or the plate the walk pressed in code holds up, by its tag: -TheaterLandScript has no pointer to hover with)
	for (const FLandGeneralShown::Mark& M : G.Marks)
	{
		if (G.ShowPlate >= 0 && M.Block == G.ShowPlate)
		{
			Over = Clip.TagOf(M.Block);
			PlateAt = Local(M.X, M.Y);
		}
	}
	if (Over && Over->bKnown && (Over->bMine || !Over->bHidden) && !Over->Name.empty())
	{
		const int Men = Over->Men >= 0 ? Over->Men : Over->Men0;
		// (about: to the hundred from a thousand up, else to the ten)
		const int Step = Men >= 1000 ? 100 : 10;
		const int About = FMath::Max(Step, (Men + Step / 2) / Step * Step);
		TArray<FString> Said;
		{
			const FString Many = FText::AsNumber(About).ToString();
			const FString Belongs = Wide(Over->Parent);
			// (its kind first, in words, as its card may show only the kit's sign: the UI director, 20:52Z)
			FString Kind = KindWord(*Over);
			Kind = Kind.IsEmpty() ? Kind : Kind.Left(1) + Kind.RightChop(1).ToLower() + TEXT(": ");
			if (!Belongs.IsEmpty())
			{
				Said.Add(Kind + (Men > 0 ? FString::Printf(TEXT("%s, about %s men."), *Belongs, *Many) : Belongs + TEXT(".")));
			}
			else if (Men > 0)
			{
				Said.Add(Kind.IsEmpty() ? FString::Printf(TEXT("About %s men."), *Many) : Kind + FString::Printf(TEXT("about %s men."), *Many));
			}
		}
		// (his unit, once the battle runs: what it is doing now, the card's words as a sentence)
		if (Over->bMine && !Clip.Deploying())
		{
			const TCHAR* NowShort = nullptr;
			bool bNowBusy = true;
			if (const TCHAR* NowDoing = DoingWords(*Over, NowShort, bNowBusy))
			{
				const FString NowSaid(NowDoing);
				Said.Add(TEXT("Now ") + NowSaid.ToLower() + TEXT("."));
			}
		}
		// (his unit holding till relieved: when help arrives, the card's hourglass in words)
		if (Over->bMine && Over->bWaits && !Over->bWaitFree)
		{
			const int32 Secs = FMath::Max(0, FMath::CeilToInt32(Over->WaitDue - Clip.Now()));
			Said.Add(Over->bWaitLate ? FString(TEXT("Help is late: its men will break sooner.")) : Secs > 0 ? FString::Printf(TEXT("Help arrives in %d:%02d."), Secs / 60, Secs % 60)
				: FString(TEXT("Dug in, waiting for help.")));
		}
		if (Over->Ratio > 0)
		{
			// (1:20 as written, never "x20", which reads as a multiplier on the men: the historian)
			Said.Add(FString::Printf(TEXT("Drawn at 1:%d: one figure stands for %d men."), Over->Ratio, Over->Ratio));
		}
		const FString Named = Wide(Over->Name).ToUpper();
		const FSlateFontInfo NameFace = Typed(11.0f, true, 30), SaidFace = Serif(TEXT("Regular"), 13.0f);
		double PW = WideOf(Named, NameFace);
		for (const FString& Line : Said)
		{
			PW = FMath::Max(PW, WideOf(Line, SaidFace));
		}
		PW += 20.0;
		const double PH = 26.0 + 19.0 * Said.Num();
		// (its tag as drawn this frame, where it has one; else the pointer)
		const FTagBox* Box = nullptr;
		for (const FTagBox& B : TagBoxes)
		{
			if (B.Block == Over->Block)
			{
				Box = &B;
			}
		}
		double PX = 0.0, PY = 0.0;
		if (Box)
		{
			const double BL = Box->Rect.Left, BT = Box->Rect.Top, BR = Box->Rect.Right, BB = Box->Rect.Bottom;
			PX = (BL + BR) * 0.5 - PW * 0.5;
			PY = BT - PH - 6.0;
			if (PY < FieldTop)
			{
				// (no room above: beside it, on the side with more room)
				PY = (BT + BB) * 0.5 - PH * 0.5;
				PX = Size.X - BR >= BL ? BR + 8.0 : BL - PW - 8.0;
			}
		}
		else
		{
			PX = PlateAt.X + 16.0;
			PY = PlateAt.Y - PH - 16.0;
		}
		PX = FMath::Clamp(PX, 8.0, FMath::Max(8.0, Size.X - PW - 8.0));
		PY = FMath::Clamp(PY, FieldTop, FMath::Max(FieldTop, Size.Y - PH - 8.0));
		// (over everything of his screen, the map of the field too: layers 31 and 32)
		Fill(PX - 1.0, PY - 1.0, PW + 2.0, PH + 2.0, Brass, 31);
		Fill(PX, PY, PW, PH, Paper, 31);
		Write(Named, PX + 10.0, PY + 5.0, NameFace, Over->bMine ? DeskInk : Slate, 32);
		for (int32 K = 0; K < Said.Num(); ++K)
		{
			Write(Said[K], PX + 10.0, PY + 24.0 + 19.0 * K, SaidFace, InkSoft, 32);
		}
	}
	return CardsH;
}

bool SLandBattleScreen::PressDrawn(const FString& Action)
{
	for (const FButton& Shown : Buttons)
	{
		if (Shown.Action == Action)
		{
			Do(Action);
			return true;
		}
	}
	return false;
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
	else if (Action == TEXT("window:film"))
	{
		// (the film's window on the field, pressed: as the FILM button)
		Scene->GeneralDo(FString(TEXT("film")));
	}
	else if (Action.StartsWith(TEXT("general:view:")))
	{
		// (a kept view's chip: flown to; with Shift, kept anew, LandGeneralEye's KeepView)
		Scene->GeneralDo((FSlateApplication::Get().GetModifierKeys().IsShiftDown() ? FString(TEXT("keepview:")) : FString(TEXT("view:"))) + Action.Mid(13));
	}
	else if (Action.StartsWith(TEXT("general:group:")))
	{
		// (a group's chip on a card, release 2: its units taken in hand; with Shift, added to those in hand)
		Scene->GeneralDo((FSlateApplication::Get().GetModifierKeys().IsShiftDown() ? FString(TEXT("addgroup:")) : FString(TEXT("group:"))) + Action.Mid(14));
	}
	else if (Action == TEXT("map:fold"))
	{
		// (the map of the field folded away or brought back: its chevron, the FIELD MAP button, or M)
		bMapFolded = !bMapFolded;
	}
	else if (Action == TEXT("map:look"))
	{
		// (his view glided to the place on the field's map under the pointer, held to the field while he drags off its
		// box; the walk's press, with no pointer on the map, looks at the field's middle)
		if (MapFrame.Valid())
		{
			double FieldX = MapFrame.FieldW * 0.5, FieldY = MapFrame.FieldH * 0.5;
			if (bMapHeld || MapFrame.Holds(HoverAt.X, HoverAt.Y))
			{
				MapFrame.ToField(HoverAt.X, HoverAt.Y, FieldX, FieldY);
			}
			Scene->GeneralMapLook(FieldX, FieldY);
		}
	}
	else if (Action.StartsWith(TEXT("general:")))
	{
		Scene->GeneralDo(Action.Mid(8));
	}
	else if (Action == TEXT("cards:next") || Action == TEXT("cards:back"))
	{
		// (the card row scrolled a card at a time; the painting keeps it in range)
		CardFirst = FMath::Max(0, CardFirst + (Action == TEXT("cards:next") ? 1 : -1));
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
	// (a click while the night before the attack plays: on to the dawn)
	if (ALandBattle* Night = Battle.Get())
	{
		const FLandBattleState* NightState = Night->GetState();
		if (NightState && NightState->NightNow.bNight)
		{
			Night->Press(TEXT("night"));
			return FReply::Handled();
		}
	}
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (const FButton* Hit = ButtonAt(Local))
	{
		// (the map of the field, release 2: the left button looks there and, held, takes his view with the pointer; no
		// other button does anything on it, so an order's click over the map gives nothing)
		if (Hit->Action == TEXT("map:look"))
		{
			if (Event.GetEffectingButton() == EKeys::LeftMouseButton && GeneralHeld < 0 && !bSliding)
			{
				HoverAt = Local;
				bMapHeld = true;
				Do(Hit->Action);
				return HasMouseCapture() ? FReply::Handled() : FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Handled();
		}
		// (the film's window, the views' chips and the map's fold stand on the field: only the left button presses them,
		// so an order given with the right button over one does not start the film, fly the view or fold the map)
		const bool bOnField = Hit->Action == TEXT("window:film") || Hit->Action.StartsWith(TEXT("general:view:")) || Hit->Action == TEXT("map:fold");
		if (!bOnField || Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			Do(Hit->Action);
		}
		return FReply::Handled();
	}
	if (IsGeneral())
	{
		// His map: the left button picks up and boxes, the right gives orders and draws lines; both his, not the camera's.
		const FKey Which = Event.GetEffectingButton();
		const int32 Button = Which == EKeys::LeftMouseButton ? 0 : Which == EKeys::RightMouseButton ? 1 : -1;
		// (not while he holds the map of the field: one hold at a time)
		if (Button >= 0 && GeneralHeld < 0 && !bMapHeld)
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
	if (bMapHeld && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// (his hold on the map of the field let go: the view stays where he took it)
		bMapHeld = false;
		return GeneralHeld >= 0 || bSliding ? FReply::Handled() : FReply::Handled().ReleaseMouseCapture();
	}
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
	bMapHeld = false;
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
	// With Option (Alt) held the slide tilts and turns the view instead: fingers up look further down on the field (a
	// slide the screen's height, 120 degrees), across turns it (the screen's width, 180 degrees).
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Delta = Event.GetGestureDelta();
	switch (Event.GetGestureType())
	{
	case EGestureEvent::Scroll:
		if (Size.X > 0.0 && Size.Y > 0.0 && (Event.IsAltDown() || FSlateApplication::Get().GetModifierKeys().IsAltDown()))
		{
			S->GeneralIn.Tilt -= Delta.Y / Size.Y * 120.0;
			S->GeneralIn.Turn -= Delta.X / Size.X * 180.0;
		}
		else if (Size.X > 0.0 && Size.Y > 0.0)
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
	S->GeneralTiltHeld = On(64u) - On(128u);
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
	// (W or S let go ends its tilt too, Shift or not: held with Shift it tilted the view, 64 and 128)
	ViewKeys &= ~(Bit | (Bit == 4u || Bit == 8u ? Bit << 4 : 0u));
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
			if (bSliding && Size.X > 0.0 && Size.Y > 0.0 && Event.IsAltDown())
			{
				// (the middle drag with Alt (Option) tilts and turns the view, as the trackpad's slide does)
				In.Tilt -= Event.GetCursorDelta().Y / Size.Y * 120.0;
				In.Turn -= Event.GetCursorDelta().X / Size.X * 180.0;
			}
			else if (bSliding && Size.X > 0.0 && Size.Y > 0.0)
			{
				In.SlideRight -= Event.GetCursorDelta().X / Size.X;
				In.SlideAhead += Event.GetCursorDelta().Y / Size.Y;
			}
			// (the pointer at the very edge of the screen moves the view that way; not while he drags a box or a unit, nor
			// while he holds the map of the field)
			const double Edge = 4.0;
			In.EdgeRight = bMapHeld ? 0.0 : Local.X >= Size.X - Edge ? 1.0 : Local.X <= Edge ? -1.0 : 0.0;
			In.EdgeAhead = bMapHeld ? 0.0 : Local.Y <= Edge ? 1.0 : Local.Y >= Size.Y - Edge ? -1.0 : 0.0;
			In.bLockEdgePan = GeneralHeld >= 0;
		}
		if (bMapHeld)
		{
			// (his hold on the map of the field, release 2: the view goes with the pointer, held to the field; no tag's
			// plate comes up and no word by the pointer meanwhile)
			HoverAt = Local;
			HoverBlock = -1;
			Do(TEXT("map:look"));
			Battle->GeneralMove(FVector2D(-1.0, -1.0), -1);
			return FReply::Handled();
		}
		// (the block under the pointer, a tag or a card, for its plate)
		HoverAt = Local;
		// (a button stands over any tag under it: the film's window and the views' chips are on the field, and a tag
		// behind one is not to be had through it)
		const FButton* Over = ButtonAt(Local);
		const int32 TagHere = Over ? -1 : TagUnder(Local);
		HoverBlock = !Over ? TagHere : Over->Action.StartsWith(TEXT("pick:")) ? FCString::Atoi(*Over->Action.Mid(5)) : -1;
		// (over his bar or a card, no word by the pointer)
		Battle->GeneralMove(GeneralHeld < 0 && Over ? FVector2D(-1.0, -1.0) : ToPixels(Geometry, Local), TagHere);
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
	// (a key while the night before the attack plays: on to the dawn)
	if (S->NightNow.bNight)
	{
		Scene->Press(TEXT("night"));
		return FReply::Handled();
	}
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
		uint32 Bit = Key == EKeys::A || Key == EKeys::Left ? 1u : Key == EKeys::D || Key == EKeys::Right ? 2u : Key == EKeys::W || Key == EKeys::Up ? 4u
			: Key == EKeys::S || Key == EKeys::Down ? 8u : Key == EKeys::Q ? 16u : Key == EKeys::E ? 32u : 0u;
		// (Shift with W or S, or the up and down arrows: his view tilted further down or back, not moved)
		if (Event.IsShiftDown() && (Bit == 4u || Bit == 8u))
		{
			Bit <<= 4;
		}
		if (S->General.bFilm)
		{
			if (Key == EKeys::Escape || Key == EKeys::C) { Scene->GeneralDo(TEXT("map")); return FReply::Handled(); }
		}
		else if (Bit != 0u)
		{
			// (his view's keys, held: the scene moves the view while they are down. W or S walks, or with Shift
			// tilts, never both at once: the key repeats, so Shift pressed or let go while it is held switches which)
			if (Bit == 4u || Bit == 64u || Bit == 8u || Bit == 128u)
			{
				const uint32 Pair = Bit == 4u || Bit == 64u ? (4u | 64u) : (8u | 128u);
				ViewKeys &= ~Pair;
			}
			ViewKeys |= Bit;
			HandViewKeys();
			return FReply::Handled();
		}
		else if (const int32 ViewKey = Key == EKeys::F1 ? 0 : Key == EKeys::F2 ? 1 : Key == EKeys::F3 ? 2 : Key == EKeys::F4 ? 3
			: !Event.IsAltDown() ? -1 : Key == EKeys::One ? 0 : Key == EKeys::Two ? 1 : Key == EKeys::Three ? 2 : Key == EKeys::Four ? 3 : -1; ViewKey >= 0)
		{
			// (his kept views: F1 to F4, or Option with 1 to 4 where the Mac keeps the F keys for itself, fly to one;
			// with Shift the view is kept under that number, LandGeneralEye's KeepView)
			if (!Event.IsRepeat())
			{
				Scene->GeneralDo(FString::Printf(TEXT("%s:%d"), Event.IsShiftDown() ? TEXT("keepview") : TEXT("view"), ViewKey));
			}
			return FReply::Handled();
		}
		else if (const int32 GroupKey = !FLandLive::RuleNow(TEXT("groups")) ? 0 : Key == EKeys::One ? 1 : Key == EKeys::Two ? 2 : Key == EKeys::Three ? 3
			: Key == EKeys::Four ? 4 : Key == EKeys::Five ? 5 : Key == EKeys::Six ? 6 : Key == EKeys::Seven ? 7 : Key == EKeys::Eight ? 8 : Key == EKeys::Nine ? 9 : 0; GroupKey > 0)
		{
			// (groups on the number keys, release 2; Brian, 26 November 2026: Cmd on a Mac, Ctrl elsewhere, with a number
			// ties those in hand to it; the number alone takes its units in hand, with Shift adds them to those in hand,
			// and pressed twice quickly glides the view to them. Unreal hands a Mac's Cmd over as Control and its Ctrl
			// as Command, so either is taken. In the setup step and in the fight alike.)
			if (!Event.IsRepeat())
			{
				Scene->GeneralDo(FString::Printf(TEXT("%s:%d"), Event.IsControlDown() || Event.IsCommandDown() ? TEXT("tie") : Event.IsShiftDown() ? TEXT("addgroup") : TEXT("group"), GroupKey));
			}
			return FReply::Handled();
		}
		else if (const int32 PaceStep = !FLandLive::RuleNow(TEXT("groups")) ? 0 : Key == EKeys::Hyphen || Key == EKeys::Subtract ? -1 : Key == EKeys::Equals || Key == EKeys::Add ? 1 : 0; PaceStep != 0)
		{
			// (with the groups on the numbers the pace is on the minus and plus keys, the numeric pad's too: a step slower
			// or faster through x1, x2 and x4, as the pace's own buttons. Not while he places his army: the clock waits
			// there, and SPACE or ENTER begins the battle.)
			if (!bSetup && !Event.IsRepeat())
			{
				const double PaceNow = S->SpeedNow > 0.0 ? S->SpeedNow : SpeedBefore > 0.0 ? SpeedBefore : 1.0;
				Pace(PaceStep > 0 ? (PaceNow < 1.5 ? 2.0 : 4.0) : (PaceNow > 3.0 ? 2.0 : 1.0));
			}
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
		else if (const TCHAR* BarOrder = Key == EKeys::G ? TEXT("general:double") : Key == EKeys::F ? TEXT("general:fire") : Key == EKeys::X ? TEXT("general:gap")
			: Key == EKeys::K ? TEXT("general:smoke") : Key == EKeys::T ? TEXT("general:wait") : Key == EKeys::B ? TEXT("general:staff") : nullptr; BarOrder && !bSetup)
		{
			// (the orders bar's keys: each presses its button where the bar shows it, and does nothing where it does not)
			PressDrawn(BarOrder);
			return FReply::Handled();
		}
		else if (Key == EKeys::Tab) { Scene->GeneralDo(Event.IsShiftDown() ? TEXT("back") : TEXT("next")); return FReply::Handled(); }
		else if (Key == EKeys::Escape) { Scene->GeneralDo(TEXT("clear")); return FReply::Handled(); }
		else if (Key == EKeys::C) { Scene->GeneralDo(TEXT("film")); return FReply::Handled(); }
		else if (Key == EKeys::O) { Scene->GeneralDo(TEXT("topview")); return FReply::Handled(); }
		// (M folds the map of the field away and brings it back, release 2; with its row off the key is nobody's)
		else if (Key == EKeys::M && FLandLive::RuleNow(TEXT("fieldmap")))
		{
			if (!Event.IsRepeat())
			{
				Do(TEXT("map:fold"));
			}
			return FReply::Handled();
		}
		else if (Key == EKeys::V && !bSetup) { Scene->GeneralDo(TEXT("ride")); return FReply::Handled(); }
		else if (Key == EKeys::Enter && bSetup) { Scene->GeneralDo(TEXT("begin")); return FReply::Handled(); }
		// (after the break, release 2 `pursue`: P pursues while he is asked; the scene and the rules refuse it at any other time)
		else if (Key == EKeys::P && !bSetup && !Event.IsRepeat() && FLandLive::RuleNow(TEXT("pursue"))) { Scene->GeneralDo(TEXT("pursue")); return FReply::Handled(); }
		// (U, "draw up", while he places his army: each press draws them up in the next of the four, Line first; release 2.
		// The scene does nothing with the row off, or once the battle has begun.)
		else if (Key == EKeys::U && bSetup) { Scene->GeneralDo(TEXT("drawup")); return FReply::Handled(); }
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
