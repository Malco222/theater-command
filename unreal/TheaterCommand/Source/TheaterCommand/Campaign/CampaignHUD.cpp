#include "Campaign/CampaignHUD.h"
#include "Campaign/TheaterSpeed.h"
#include "Campaign/CampaignText.h"

#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Battle/LandBattle.h"
#include "Battle/LandLive.h"
#include "EngineUtils.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "Campaign/CampaignTextures.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * The frame's own scale inside Unreal's game layer. (18 November 2026. It was "the window's height over 900". But
	 * Unreal already draws everything on the game's screen at the window's height over 1080, so the frame grew with the
	 * SQUARE of the window's height: right at the 1600 by 1000 it is drawn and reviewed at, 77 hundredths of its proper
	 * size at Brian's 1441 by 878, 60 at the 776 he played at before, and double at the screen's fine pixels, where a
	 * card filled the whole window. It is one number now, so that the frame keeps the same size against the window at
	 * every size: what it is at 1600 by 1000. "-TheaterOldFrameScale" is the old rule.)
	 */
	/**
	 * A question card's lines, marked for its lettering: a line all in capitals is a section's head and takes the card
	 * label's style; the rest take the style named (none: the card's plain line). What the rules wrote is kept letter
	 * for letter: the marks' own signs are made safe first.
	 */
	FText CardMarkup(const FString& Plain, const TCHAR* LineStyle = nullptr)
	{
		TArray<FString> Lines;
		Plain.ParseIntoArray(Lines, TEXT("\n"), false);
		FString Marked;
		for (int32 K = 0; K < Lines.Num(); ++K)
		{
			const FString& Line = Lines[K];
			int32 Capitals = 0, Small = 0;
			for (const TCHAR Char : Line)
			{
				Capitals += FChar::IsUpper(Char) ? 1 : 0;
				Small += FChar::IsLower(Char) ? 1 : 0;
			}
			const FString Safe = Line.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")).Replace(TEXT("\""), TEXT("&quot;"));
			const bool bHead = Capitals >= 3 && Small == 0;
			Marked += (K > 0 ? TEXT("\n") : TEXT(""));
			Marked += Safe.IsEmpty() ? Safe : bHead ? TEXT("<Head>") + Safe + TEXT("</>") : LineStyle ? FString::Printf(TEXT("<%s>"), LineStyle) + Safe + TEXT("</>") : Safe;
		}
		return FText::FromString(Marked);
	}

	float FrameScale(double PixelsHigh)
	{
		static const bool bOldRule = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldFrameScale"));
		return bOldRule ? FMath::Clamp(static_cast<float>(PixelsHigh / 900.0), 0.7f, 3.5f) : 1000.0f / 900.0f;
	}
}

namespace
{
	const FLinearColor PanelFill(0.012f, 0.010f, 0.008f, 0.88f);
	const FLinearColor CardFill(0.045f, 0.040f, 0.034f, 0.96f);
	const FLinearColor CardHover(0.085f, 0.075f, 0.062f, 0.98f);
	const FLinearColor Brass = FLinearColor(FColor::FromHex(TEXT("B08D57")));
	const FLinearColor BrightBrass = FLinearColor(FColor::FromHex(TEXT("E0B872")));
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("EDE6D6")));
	const FLinearColor Muted = FLinearColor(FColor::FromHex(TEXT("A89F8D")));
	const FLinearColor Faint = FLinearColor(FColor::FromHex(TEXT("6E675B")));
	const FLinearColor Alarm = FLinearColor(FColor::FromHex(TEXT("E08A6A")));
	const FLinearColor Gain = FLinearColor(FColor::FromHex(TEXT("9CC08A")));

	/**
	 * The map's own lettering is a serif, as an atlas's is: PT Serif (ParaType; SIL Open Font License 1.1), read
	 * from the project's own Content/Fonts folder if it is there, and otherwise from the Mac's system fonts, where
	 * the same face is installed. Weight is Regular, Italic or Bold. Without either file the plain face is used.
	 */
	FSlateFontInfo Serif(const TCHAR* Weight, float Size, int32 Spacing = 0)
	{
		static TSharedPtr<FStandaloneCompositeFont> Face;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			const FString Own = FPaths::ProjectContentDir() / TEXT("Fonts");
			const FString System = TEXT("/System/Library/Fonts/Supplemental/PTSerif.ttc");
			auto Add = [](FStandaloneCompositeFont& Into, const TCHAR* Name, const FString& File, int32 SubFace)
			{
				FTypefaceEntry& Entry = Into.DefaultTypeface.Fonts.Emplace_GetRef(FName(Name));
				Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad, SubFace);
			};
			if (FPaths::FileExists(Own / TEXT("PTSerif-Regular.ttf")) && FPaths::FileExists(Own / TEXT("PTSerif-Italic.ttf")) && FPaths::FileExists(Own / TEXT("PTSerif-Bold.ttf")))
			{
				Face = MakeShared<FStandaloneCompositeFont>();
				Add(*Face, TEXT("Regular"), Own / TEXT("PTSerif-Regular.ttf"), 0);
				Add(*Face, TEXT("Italic"), Own / TEXT("PTSerif-Italic.ttf"), 0);
				Add(*Face, TEXT("Bold"), Own / TEXT("PTSerif-Bold.ttf"), 0);
			}
			else if (FPaths::FileExists(System))
			{
				// (in the system's collection the faces stand in this order: regular, italic, bold italic, bold)
				Face = MakeShared<FStandaloneCompositeFont>();
				Add(*Face, TEXT("Regular"), System, 0);
				Add(*Face, TEXT("Italic"), System, 1);
				Add(*Face, TEXT("Bold"), System, 3);
			}
		}
		if (!Face.IsValid())
		{
			FSlateFontInfo Plain = FCoreStyle::GetDefaultFontStyle(FCString::Stricmp(Weight, TEXT("Bold")) == 0 ? TEXT("Bold") : FCString::Stricmp(Weight, TEXT("Italic")) == 0 ? TEXT("Italic") : TEXT("Regular"), Size);
			Plain.LetterSpacing = Spacing;
			return Plain;
		}
		FSlateFontInfo Info(Face, Size, FName(Weight));
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo Typed(float Size, bool bBold = false, int32 Spacing = 0);

	/**
	 * The frame is lettered in two faces only: the serif for names and titles, and the typewriter for everything
	 * else. What asks for a large size is a title or a name and is set in the serif, and so is anything in italic
	 * (a typewriter has none); the rest is typed, a little smaller than asked, since every typed letter is as
	 * wide as the widest, and with less of the letter-spacing the old plain face needed.
	 */
	FSlateFontInfo Font(const TCHAR* Weight, int32 Size, int32 Spacing = 0)
	{
		const bool bBold = FCString::Stricmp(Weight, TEXT("Bold")) == 0, bItalic = FCString::Stricmp(Weight, TEXT("Italic")) == 0;
		if (Size >= 16 || bItalic)
		{
			return Serif(Weight, static_cast<float>(Size), Spacing / 2);
		}
		return Typed(Size * 0.9f, bBold, Spacing / 4);
	}

	/**
	 * The frame's typewriter face, for what a clerk would have typed: the treasury, the dispatches, the labels of
	 * the doors, and every line that is not a name or a title. It is Courier Prime (Quote-Unquote Apps; SIL Open
	 * Font License 1.1), read from the project's own Content/Fonts folder beside its licence. No lettering reads
	 * a font from the machine it runs on. Until that file is in the folder the engine's own fixed face stands in.
	 */
	FSlateFontInfo Typed(float Size, bool bBold, int32 Spacing)
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
				// (a tick, a small triangle or an arrow the typewriter has no key for is cut from the engine's own face of
				// signs, the one that drew them before the typewriter came: the plain face has none of them either, and
				// a sign that no face has is drawn as an empty box)
				const FString Plain = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), Wide = FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
				if (FPaths::FileExists(Wide))
				{
					Add(Face->FallbackTypeface.Typeface, TEXT("Regular"), Wide);
					Add(Face->FallbackTypeface.Typeface, TEXT("Bold"), Wide);
				}
				else if (FPaths::FileExists(Plain))
				{
					Add(Face->FallbackTypeface.Typeface, TEXT("Regular"), Plain);
					Add(Face->FallbackTypeface.Typeface, TEXT("Bold"), Plain);
				}
			}
		}
		if (!Face.IsValid())
		{
			FSlateFontInfo Plain = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size);
			Plain.LetterSpacing = Spacing;
			return Plain;
		}
		FSlateFontInfo Info(Face, Size, FName(bBold ? TEXT("Bold") : TEXT("Regular")));
		Info.LetterSpacing = Spacing;
		return Info;
	}

	/**
	 * The frame's one size of small capital label (the UI director's type floor, ruled on build 1638): the larger
	 * label size, 10 as the player sees it, with its letter-spacing brought down to 0.04 em so the word grows by a
	 * few per cent in width, not a quarter. The smaller size that stood beside it, about 8, is retired.
	 */
	FSlateFontInfo SmallCaps()
	{
		return Typed(9.0f, true, 40);
	}

	/** Where the pieces of the frame are on the screen this frame (absolute): a town's plate or a province's name is not drawn under one. */
	TArray<FSlateRect> FrameRects;

	// The frame is made of the things on a staff officer's desk in 1939: calendar paper and typing paper, written
	// on in ink; brown leather; and brass.
	const FLinearColor Paper = FLinearColor(FColor::FromHex(TEXT("CDBE9B")));
	const FLinearColor TypingPaper = FLinearColor(FColor::FromHex(TEXT("D3C6A4")));
	const FLinearColor DeskInk = FLinearColor(FColor::FromHex(TEXT("2B2118")));
	const FLinearColor InkSoft = FLinearColor(FColor::FromHex(TEXT("5B4A38")));
	const FLinearColor InkRed = FLinearColor(FColor::FromHex(TEXT("8E2C20")));
	const FLinearColor Binding = FLinearColor(FColor::FromHex(TEXT("7A2F22")));
	const FLinearColor Leather = FLinearColor(FColor::FromHex(TEXT("2A1C12"))); // (the UI director, check 19: every panel's fill is solid; only the veil is see-through)
	const FLinearColor DoorBrass = FLinearColor(FColor::FromHex(TEXT("C9A55A")));
	const FLinearColor PillBrass = FLinearColor(FColor::FromHex(TEXT("B8934D")));

	/** The pieces of the frame, in a widget's own measure and a little larger than life: what is drawn there would lie under one. */
	TArray<FBox2D> FrameBoxes(const FGeometry& Geometry)
	{
		TArray<FBox2D> Out;
		for (const FSlateRect& Rect : FrameRects)
		{
			const FVector2D Min = Geometry.AbsoluteToLocal(FVector2D(Rect.Left, Rect.Top)), Max = Geometry.AbsoluteToLocal(FVector2D(Rect.Right, Rect.Bottom));
			Out.Add(FBox2D(Min - FVector2D(4.0, 4.0), Max + FVector2D(4.0, 4.0)));
		}
		return Out;
	}

	bool UnderFrame(const TArray<FBox2D>& Frame, const FBox2D& Box)
	{
		for (const FBox2D& Each : Frame)
		{
			if (Each.Intersect(Box))
			{
				return true;
			}
		}
		return false;
	}

	/** Which month of the year a date such as "September 1939" names: 1 to 12, or 12 if it names none. */
	int32 MonthOf(const FString& Date)
	{
		static const TCHAR* Months[12] = { TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December") };
		for (int32 Month = 0; Month < 12; ++Month)
		{
			if (Date.StartsWith(Months[Month]))
			{
				return Month + 1;
			}
		}
		return 12;
	}

	/**
	 * A small sign drawn in line, as if engraved: the signs on the six doors, and the dial on the control that
	 * ends the month. Kinds: 0 a shell (the arsenal), 1 a star (the generals), 2 a pair of dividers (research),
	 * 3 a sealed letter (diplomacy), 4 an open book (the ledger), 5 a medal (honours), 6 a clock's dial, whose
	 * short hand stands at the hour it is given.
	 */
	class SLineIcon : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLineIcon) : _Kind(0), _Size(24.0f), _Ink(FLinearColor::White), _Hand(12.0f), _Weight(1.5f) {}
			SLATE_ARGUMENT(int32, Kind)
			SLATE_ARGUMENT(float, Size)
			SLATE_ATTRIBUTE(FLinearColor, Ink)
			SLATE_ATTRIBUTE(float, Hand)
			SLATE_ARGUMENT(float, Weight)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Kind = Args._Kind;
			Size = Args._Size;
			InkColour = Args._Ink;
			Hand = Args._Hand;
			Weight = Args._Weight;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			TArray<TArray<FVector2D>> Strokes;
			auto Line = [&Strokes](std::initializer_list<FVector2D> Points) { Strokes.Emplace(Points); };
			auto Ring = [&Strokes](double X, double Y, double R, double From = 0.0, double To = 360.0)
			{
				TArray<FVector2D>& Stroke = Strokes.AddDefaulted_GetRef();
				const int32 Steps = FMath::Max(4, FMath::RoundToInt32((To - From) / 15.0));
				for (int32 K = 0; K <= Steps; ++K)
				{
					const double Angle = FMath::DegreesToRadians(From + (To - From) * K / Steps);
					Stroke.Add(FVector2D(X + R * FMath::Sin(Angle), Y - R * FMath::Cos(Angle)));
				}
			};
			switch (Kind)
			{
			case 0: // a shell standing on its base, with its driving band
				Line({ { 0.36, 0.9 }, { 0.36, 0.44 }, { 0.39, 0.3 }, { 0.44, 0.18 }, { 0.5, 0.1 }, { 0.56, 0.18 }, { 0.61, 0.3 }, { 0.64, 0.44 }, { 0.64, 0.9 }, { 0.36, 0.9 } });
				Line({ { 0.36, 0.78 }, { 0.64, 0.78 } });
				Line({ { 0.36, 0.44 }, { 0.64, 0.44 } });
				break;
			case 1: // a star of five points
			{
				TArray<FVector2D>& Star = Strokes.AddDefaulted_GetRef();
				for (int32 K = 0; K <= 10; ++K)
				{
					const double Angle = FMath::DegreesToRadians(36.0 * K), R = K % 2 == 0 ? 0.43 : 0.175;
					Star.Add(FVector2D(0.5 + R * FMath::Sin(Angle), 0.53 - R * FMath::Cos(Angle)));
				}
				break;
			}
			case 2: // a pair of dividers, and the arc they have drawn
				Ring(0.5, 0.2, 0.075);
				Line({ { 0.47, 0.27 }, { 0.28, 0.86 } });
				Line({ { 0.53, 0.27 }, { 0.72, 0.86 } });
				Line({ { 0.39, 0.54 }, { 0.61, 0.54 } });
				Ring(0.5, 0.2, 0.72, 160.0, 200.0);
				break;
			case 3: // a letter, sealed
				Line({ { 0.13, 0.27 }, { 0.87, 0.27 }, { 0.87, 0.75 }, { 0.13, 0.75 }, { 0.13, 0.27 } });
				Line({ { 0.13, 0.27 }, { 0.5, 0.55 }, { 0.87, 0.27 } });
				Line({ { 0.13, 0.75 }, { 0.4, 0.5 } });
				Line({ { 0.87, 0.75 }, { 0.6, 0.5 } });
				Ring(0.5, 0.56, 0.06);
				break;
			case 4: // a book lying open
				Line({ { 0.5, 0.3 }, { 0.13, 0.23 }, { 0.13, 0.73 }, { 0.5, 0.8 }, { 0.87, 0.73 }, { 0.87, 0.23 }, { 0.5, 0.3 }, { 0.5, 0.8 } });
				Line({ { 0.21, 0.37 }, { 0.42, 0.41 } });
				Line({ { 0.21, 0.5 }, { 0.42, 0.54 } });
				Line({ { 0.58, 0.41 }, { 0.79, 0.37 } });
				Line({ { 0.58, 0.54 }, { 0.79, 0.5 } });
				break;
			case 5: // a medal on its ribbon
				Line({ { 0.35, 0.08 }, { 0.65, 0.08 }, { 0.58, 0.42 }, { 0.42, 0.42 }, { 0.35, 0.08 } });
				Line({ { 0.5, 0.08 }, { 0.5, 0.42 } });
				Ring(0.5, 0.67, 0.23);
				Line({ { 0.5, 0.55 }, { 0.5, 0.79 } });
				Line({ { 0.38, 0.67 }, { 0.62, 0.67 } });
				break;
			case 7: // a tick
				Line({ { 0.14, 0.55 }, { 0.4, 0.8 }, { 0.88, 0.2 } });
				break;
			case 9: // an agent: a hat with its brim pulled down, over a raised collar
				Line({ { 0.1, 0.42 }, { 0.9, 0.42 } });
				Line({ { 0.27, 0.42 }, { 0.31, 0.16 }, { 0.44, 0.1 }, { 0.56, 0.1 }, { 0.69, 0.16 }, { 0.73, 0.42 } });
				Line({ { 0.29, 0.31 }, { 0.71, 0.31 } });
				Line({ { 0.2, 0.92 }, { 0.3, 0.6 }, { 0.5, 0.78 }, { 0.7, 0.6 }, { 0.8, 0.92 } });
				Line({ { 0.5, 0.78 }, { 0.5, 0.92 } });
				break;
			case 8: // a chevron, pointing on
				Line({ { 0.36, 0.2 }, { 0.66, 0.5 }, { 0.36, 0.8 } });
				break;
			case 10: // (rules 84.2.6, sheet 35) the warning sign: a triangle, and the mark inside it
				Line({ { 0.5, 0.1 }, { 0.93, 0.86 }, { 0.07, 0.86 }, { 0.5, 0.1 } });
				Line({ { 0.5, 0.36 }, { 0.5, 0.6 } });
				Line({ { 0.5, 0.7 }, { 0.5, 0.75 } });
				break;
			default: // a clock's dial: the long hand at twelve, the short one at the hour
			{
				Ring(0.5, 0.5, 0.45);
				for (int32 Hour = 0; Hour < 12; ++Hour)
				{
					const double Angle = FMath::DegreesToRadians(30.0 * Hour), Inner = Hour % 3 == 0 ? 0.33 : 0.38;
					Line({ { 0.5 + Inner * FMath::Sin(Angle), 0.5 - Inner * FMath::Cos(Angle) }, { 0.5 + 0.43 * FMath::Sin(Angle), 0.5 - 0.43 * FMath::Cos(Angle) } });
				}
				Line({ { 0.5, 0.5 }, { 0.5, 0.17 } });
				const double At = FMath::DegreesToRadians(30.0 * Hand.Get());
				Line({ { 0.5, 0.5 }, { 0.5 + 0.22 * FMath::Sin(At), 0.5 - 0.22 * FMath::Cos(At) } });
				break;
			}
			}
			const FVector2D Box = Geometry.GetLocalSize();
			const FLinearColor Colour = InkColour.Get() * InWidgetStyle.GetColorAndOpacityTint();
			for (TArray<FVector2D>& Stroke : Strokes)
			{
				for (FVector2D& Point : Stroke)
				{
					Point *= Box;
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Stroke, ESlateDrawEffect::None, Colour, true, Weight);
			}
			return LayerId;
		}

	private:
		int32 Kind = 0;
		float Size = 24.0f;
		TAttribute<FLinearColor> InkColour;
		TAttribute<float> Hand;
		float Weight = 1.5f;
	};

	FButtonStyle MakeButtonStyle(const FLinearColor& Fill, const FLinearColor& Hover, const FLinearColor& Outline, float OutlineWidth)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(Fill, 5.0f, Outline, OutlineWidth));
		Style.SetHovered(FSlateRoundedBoxBrush(Hover, 5.0f, BrightBrass, OutlineWidth));
		Style.SetPressed(FSlateRoundedBoxBrush(Hover, 5.0f, BrightBrass, OutlineWidth + 0.5f));
		Style.SetDisabled(FSlateRoundedBoxBrush(Fill.CopyWithNewOpacity(0.6f), 5.0f, Faint.CopyWithNewOpacity(0.5f), 1.0f));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
		return Style;
	}

	/**
	 * The panels drawn over the campaign table. Most of them only show things and let clicks through to the
	 * map; the army tray and the End Month button take the mouse.
	 */
	/** A picture taken in the studio, drawn without regard to its alpha (a scene capture leaves that empty). */
	class SPortrait : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPortrait) {}
			SLATE_ARGUMENT(TSharedPtr<FSlateBrush>, Picture)
			SLATE_ARGUMENT(FVector2D, Size)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Picture = Args._Picture;
			Size = Args._Size;
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return Size; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			if (Picture.IsValid() && Picture->GetResourceObject())
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Picture.Get(), ESlateDrawEffect::IgnoreTextureAlpha, InWidgetStyle.GetColorAndOpacityTint());
			}
			return LayerId;
		}

	private:
		TSharedPtr<FSlateBrush> Picture;
		FVector2D Size = FVector2D(64.0, 40.0);
	};

	/** The small map in the corner: the countries in their colours, the armies as marks, and the part on screen boxed. A click goes there. */
	/**
	 * The names of the countries and the seas, lettered across the map as an atlas letters them: spaced serif
	 * capitals in cream with a dark edge, each letter lying on the ground where it stands (drawn leaning and
	 * foreshortened as the ground there is seen), so that a word follows its country's long axis and its arc.
	 */
	class SMapLettering : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMapLettering) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.0, 16.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			THEATER_SPEED_SCOPE(PaintMapLettering);
			if (!Controller.IsValid())
			{
				return LayerId;
			}
			TArray<FMapLetter> Letters;
			Controller->GetMapLetters(Letters);
			const float Moon = Controller->GetMoonlight();
			const float Sheet = Controller->GetPaper(); // (on the staff's paper map the names are in ink, not in cream)
			if (Letters.Num() == 0)
			{
				return LayerId;
			}
			const float Scale = FMath::Max(Geometry.Scale, 0.01f);
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const TArray<FBox2D> Frame = FrameBoxes(Geometry);
			// A few sizes of type only, so the letters are cut once and not afresh for every size on the screen.
			static const float Sizes[] = { 10.0f, 14.0f, 20.0f, 28.0f, 40.0f, 56.0f, 80.0f };
			for (const FMapLetter& Each : Letters)
			{
				// On the screen a capital is as high as its "up" is long; a capital is about seven tenths of its type size.
				const double High = Each.Up.Size() / Scale, Wide = Each.Along.Size() / Scale;
				if (High < 2.5 || Wide < 2.5)
				{
					continue;
				}
				float Size = Sizes[6];
				for (const float Cut : Sizes)
				{
					if (Cut * 0.7f >= High)
					{
						Size = Cut;
						break;
					}
				}
				const bool bWhole = !Each.Whole.IsEmpty();
				FSlateFontInfo Type = Serif(Each.bSea || bWhole ? TEXT("Italic") : TEXT("Bold"), Size);
				if (!Each.bSea)
				{
					// (a soft dark edge, so cream letters hold on pale ground and on snow)
					Type.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt32(Size / 22.0f));
					Type.OutlineSettings.OutlineColor = FMath::Lerp(FLinearColor(0.02f, 0.013f, 0.01f, 0.55f * Each.Alpha), FLinearColor(0.8f, 0.72f, 0.55f, 0.0f), Sheet);
					if (Each.bShadowed)
					{
						// (The art director, 18 November 2026: a province's name that has only its town's roofs to lie on keeps
						// its place, "slightly smaller with a soft dark shadow over the roofs": a broad dark edge, so that no
						// roof's pale line joins a letter.)
						Type.OutlineSettings.OutlineSize = FMath::Max(2, FMath::RoundToInt32(Size / 9.0f));
						Type.OutlineSettings.OutlineColor = FMath::Lerp(FLinearColor(0.02f, 0.013f, 0.01f, 0.72f * Each.Alpha), FLinearColor(0.8f, 0.72f, 0.55f, 0.0f), Sheet);
					}
				}
				const FString Glyph = bWhole ? Each.Whole : FString(1, &Each.Char);
				const FVector2D Box = Measure->Measure(Glyph, Type);
				// The letter's own right and down, as they lie on the screen: its type size is as long as a capital
				// is high divided by seven tenths.
				const double Unit = Size * 0.7;
				const FVector2D Right = Each.Along / Scale / Unit, Down = -Each.Up / Scale / Unit;
				const FSlateRenderTransform Lying(FMatrix2x2(static_cast<float>(Right.X), static_cast<float>(Right.Y), static_cast<float>(Down.X), static_cast<float>(Down.Y)));
				{
					// A name is not lettered under a piece of the frame: a province's name not at all if any of it
					// would lie there, and of a country's or a sea's spread-out name the letters that would.
					const FVector2D Middle = Each.At / Scale;
					const FVector2D Half(FMath::Abs(Right.X) * Box.X * 0.5 + FMath::Abs(Down.X) * Box.Y * 0.5, FMath::Abs(Right.Y) * Box.X * 0.5 + FMath::Abs(Down.Y) * Box.Y * 0.5);
					if (UnderFrame(Frame, FBox2D(Middle - Half, Middle + Half)))
					{
						continue;
					}
				}
				const FGeometry Place = Geometry.MakeChild(Box, FSlateLayoutTransform(Each.At / Scale - Box * 0.5), Lying, FVector2D(0.5, 0.5));
				// (by moonlight the names sink back: a pale blue-grey, and fainter, so the lights of the towns lead the eye)
				// (a sea's name on a sea the admiral's chart veils is in dark chart ink at every height)
				const FLinearColor Day = Each.bInk ? FLinearColor(FColor(60, 80, 95)) : FMath::Lerp(Each.bSea ? FLinearColor(FColor(214, 232, 236)) : FLinearColor(FColor(244, 232, 204)),
					Each.bSea ? FLinearColor(FColor(58, 78, 88)) : FLinearColor(FColor(52, 36, 22)), Sheet * (1.0f - Moon));
				const FLinearColor Ink = FMath::Lerp(Day, FLinearColor(FColor(168, 184, 212)), Moon).CopyWithNewOpacity(Each.Alpha * (1.0f - 0.42f * Moon));
				if (bWhole)
				{
					// (The art director, 18 November 2026: "Hollamd", "Gelderlamd": at the smaller sizes the last letters of
					// a province's name stood doubled. Drawn as one piece of text and laid on the ground by a turn and a
					// squeeze, a word came out as two sets of its letters, one set out a tenth wider than the other; it
					// was so with the engine's edge and without it, and with the letters alone. A country's name, drawn a
					// letter at a time, never did it. So a province's name is drawn the same way: each letter by itself,
					// set where the word's own measure puts it.)
					double Start = -0.5 * Box.X;
					for (int32 K = 0; K < Glyph.Len(); ++K)
					{
						const FString Letter = Glyph.Mid(K, 1);
						const FVector2D LetterBox = Measure->Measure(Letter, Type);
						// (where the word's measure, with its letters' own pairing, puts this letter's left edge)
						const double Left = Start + static_cast<double>(Measure->Measure(Glyph.Left(K), Type).X);
						if (Letter != TEXT(" "))
						{
							const FVector2D Middle = Each.At / Scale + Right * (Left + 0.5 * LetterBox.X);
							const FGeometry Stood = Geometry.MakeChild(LetterBox, FSlateLayoutTransform(Middle - LetterBox * 0.5), Lying, FVector2D(0.5, 0.5));
							FSlateDrawElement::MakeText(OutDrawElements, LayerId, Stood.ToPaintGeometry(), Letter, Type, ESlateDrawEffect::None, Ink);
						}
					}
					continue;
				}
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, Place.ToPaintGeometry(), Glyph, Type, ESlateDrawEffect::None, Ink);
			}
			return LayerId;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
	};

	/**
	 * THE RESEARCH TREE AS A WALL CHART (Brian, 16 November 2026: "make the research tree a lot more visually
	 * appealing"; the art director's research-tree brief). One chart that holds every technology with no scrolling:
	 * five shelves, one a branch, run left to right through the four eras. Brass is finished, a lit card is being
	 * worked on, a cream edge is ready to begin, a faint dashed card is locked. One line runs into each card from the
	 * technology it grows from; any other need is a rivet on the card; no line ever crosses a card. A look only: every
	 * number and sentence on it is one the game already has. The places are the art director's own
	 * (Content/UI/research-tree-layout.json), on the 1600 by 1000 frame.
	 */
	struct FChartCard
	{
		FString Lane;
		int32 Row = 1, Step = 1;
		double X = 0.0, Y = 0.0;
		FString From, Line;
		TArray<FString> Also;
	};
	struct FChartLane
	{
		FString Id, Label;
		double Top = 0.0, High = 0.0;
		int32 Rows = 3;
	};
	struct FChartLayout
	{
		bool bRead = false, bGood = false;
		TMap<FString, FChartCard> Cards;
		TArray<FChartLane> Lanes;
	};
	const FChartLayout& ChartLayout()
	{
		static FChartLayout Kept;
		if (Kept.bRead)
		{
			return Kept;
		}
		Kept.bRead = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("UI/research-tree-layout.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("The research chart's layout (Content/UI/research-tree-layout.json) could not be read: the list is shown instead"));
			return Kept;
		}
		const TArray<TSharedPtr<FJsonValue>>* Lanes = nullptr;
		if (Root->TryGetArrayField(TEXT("lanes"), Lanes))
		{
			for (const TSharedPtr<FJsonValue>& Each : *Lanes)
			{
				const TSharedPtr<FJsonObject> Lane = Each->AsObject();
				if (Lane.IsValid())
				{
					FChartLane& Out = Kept.Lanes.AddDefaulted_GetRef();
					Lane->TryGetStringField(TEXT("id"), Out.Id);
					Lane->TryGetStringField(TEXT("label"), Out.Label);
					Lane->TryGetNumberField(TEXT("top"), Out.Top);
					Lane->TryGetNumberField(TEXT("h"), Out.High);
					Lane->TryGetNumberField(TEXT("rows"), Out.Rows);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Cards = nullptr;
		if (Root->TryGetObjectField(TEXT("cards"), Cards) && Cards && Cards->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Each : (*Cards)->Values)
			{
				const TSharedPtr<FJsonObject> Card = Each.Value->AsObject();
				if (Card.IsValid())
				{
					FChartCard& Out = Kept.Cards.Add(FString(Each.Key));
					Card->TryGetStringField(TEXT("lane"), Out.Lane);
					Card->TryGetNumberField(TEXT("row"), Out.Row);
					Card->TryGetNumberField(TEXT("step"), Out.Step);
					Card->TryGetNumberField(TEXT("x"), Out.X);
					Card->TryGetNumberField(TEXT("y"), Out.Y);
					Card->TryGetStringField(TEXT("line_from"), Out.From);
					Card->TryGetStringField(TEXT("line"), Out.Line);
					Card->TryGetStringArrayField(TEXT("also_needs"), Out.Also);
				}
			}
		}
		Kept.bGood = Kept.Lanes.Num() > 0 && Kept.Cards.Num() > 0;
		return Kept;
	}

	class SResearchChart : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SResearchChart) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Player)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Player = Args._Player;
		}

		// (the panel's own place and size on the 1600 by 1000 frame)
		static constexpr double PanelX = 14.0, PanelY = 12.0, PanelW = 1572.0, PanelH = 976.0, CardW = 292.0, CardH = 38.0;

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(PanelW, PanelH); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FChartLayout& Chart = ChartLayout();
			if (!Player.IsValid() || !Chart.bGood)
			{
				return LayerId;
			}
			const FResearch& Labs = Player->GetResearch();
			static const FSlateColorBrush Solid(FLinearColor::White);
			// (The brief's sizes are in pixels of the 1600 by 1000 frame; a face here is asked for in points, which are a third larger.)
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			const FLinearColor Desk900(FColor::FromHex(TEXT("1D1610"))), Desk800(FColor::FromHex(TEXT("2A2118"))), Desk700(FColor::FromHex(TEXT("3A2E22"))), Brass700(FColor::FromHex(TEXT("7A6038")));
			const FLinearColor Lit(FColor::FromHex(TEXT("F2D79A"))), Amber(FColor::FromHex(TEXT("E0A13C"))), Track(FColor::FromHex(TEXT("5D4C37"))), Dim(FColor::FromHex(TEXT("8F8573")));
			const FLinearColor BrassGround(FColor::FromHex(TEXT("4A3A20")));
			// Everything below is set out on the frame's own measures: a point of the frame is a point here, less the panel's corner.
			auto At = [](double X, double Y) { return FVector2D(X - PanelX, Y - PanelY); };
			auto Fill = [&](double X, double Y, double W, double H, const FLinearColor& Ink, int32 Layer)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(W, H), FSlateLayoutTransform(At(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
			};
			auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
			{
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(1200.0, 40.0), FSlateLayoutTransform(At(X, Y))), Words, Face, ESlateDrawEffect::None, Ink);
			};
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto Wide = [&Measure](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face).X); };
			// (words that must fit a breadth lose whole words from the end, never a letter)
			auto Fitted = [&](FString Words, const FSlateFontInfo& Face, double Room)
			{
				// (ruling 149: no sentence is ever cut with dots. Words that must fit a breadth lose whole words from the end.)
				while (Wide(Words, Face) > Room)
				{
					int32 Space = INDEX_NONE;
					if (!Words.FindLastChar(TEXT(' '), Space) || Space <= 0)
					{
						break;
					}
					Words = Words.Left(Space);
					while (Words.Len() > 1 && (Words.EndsWith(TEXT(",")) || Words.EndsWith(TEXT(":")) || Words.EndsWith(TEXT(";")) || Words.EndsWith(TEXT(" "))))
					{
						Words.LeftChopInline(1);
					}
				}
				return Words;
			};
			// A line through points; dashed where a dash and a gap are given.
			auto Stroke = [&](const TArray<FVector2D>& Points, const FLinearColor& Ink, float Thick, int32 Layer, double Dash = 0.0, double Gap = 0.0)
			{
				TArray<FVector2D> Local;
				for (const FVector2D& Point : Points)
				{
					Local.Add(At(Point.X, Point.Y));
				}
				if (Dash <= 0.0)
				{
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Local, ESlateDrawEffect::None, Ink, true, Thick);
					return;
				}
				double Carried = 0.0;
				for (int32 K = 0; K + 1 < Local.Num(); ++K)
				{
					const FVector2D From = Local[K], To = Local[K + 1];
					const double Long = FVector2D::Distance(From, To);
					if (Long < 0.01)
					{
						continue;
					}
					const FVector2D Way = (To - From) / Long;
					for (double Along = -Carried; Along < Long; Along += Dash + Gap)
					{
						const double A = FMath::Max(Along, 0.0), B = FMath::Min(Along + Dash, Long);
						if (B > A)
						{
							FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), { From + Way * A, From + Way * B }, ESlateDrawEffect::None, Ink, true, Thick);
						}
					}
					Carried = FMath::Fmod(Long + Carried, Dash + Gap);
				}
			};
			auto Edge = [&](double X, double Y, double W, double H, const FLinearColor& Ink, float Thick, int32 Layer, double Dash = 0.0, double Gap = 0.0)
			{
				Stroke({ FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H), FVector2D(X, Y) }, Ink, Thick, Layer, Dash, Gap);
			};
			auto Round = [&](double X, double Y, double R, const FLinearColor& Ink, float Thick, int32 Layer, double Dash = 0.0, double Gap = 0.0)
			{
				TArray<FVector2D> Points;
				for (int32 K = 0; K <= 24; ++K)
				{
					const double Angle = K * UE_DOUBLE_TWO_PI / 24.0;
					Points.Add(FVector2D(X + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R));
				}
				Stroke(Points, Ink, Thick, Layer, Dash, Gap);
			};
			auto Disc = [&](double X, double Y, double R, const FLinearColor& Ink, int32 Layer)
			{
				Round(X, Y, R * 0.5, Ink, static_cast<float>(R), Layer);
			};
			// The signs: a state's (tick, hourglass, open ring, padlock), a clock, and a branch's.
			auto Sign = [&](const FString& What, double X, double Y, double S, const FLinearColor& Ink, int32 Layer)
			{
				auto P = [X, Y, S](double DX, double DY) { return FVector2D(X + DX * S, Y + DY * S); };
				const float Pen = static_cast<float>(FMath::Max(1.4 * S, 1.2));
				if (What == TEXT("done"))
				{
					Stroke({ P(-5.0, 0.2), P(-1.6, 3.8), P(5.2, -4.2) }, Ink, Pen * 1.3f, Layer);
				}
				else if (What == TEXT("work"))
				{
					Stroke({ P(-3.8, -5.0), P(3.8, -5.0), P(-3.8, 5.0), P(3.8, 5.0), P(-3.8, -5.0) }, Ink, Pen, Layer);
				}
				else if (What == TEXT("ready"))
				{
					Round(X, Y, 4.4 * S, Ink, Pen, Layer);
				}
				else if (What == TEXT("locked"))
				{
					Stroke({ P(-4.2, -0.6), P(4.2, -0.6), P(4.2, 5.2), P(-4.2, 5.2), P(-4.2, -0.6) }, Ink, Pen, Layer);
					Stroke({ P(-2.8, -0.6), P(-2.8, -3.2), P(-1.4, -5.0), P(1.4, -5.0), P(2.8, -3.2), P(2.8, -0.6) }, Ink, Pen, Layer);
				}
				else if (What == TEXT("clock"))
				{
					Round(X, Y, 4.6 * S, Ink, Pen, Layer);
					Stroke({ P(0.0, -2.8), P(0.0, 0.0), P(2.2, 1.2) }, Ink, Pen, Layer);
				}
				else if (What == TEXT("land"))
				{
					// (a tank, side on: its hull, its turret and its gun)
					Stroke({ P(-5.6, 1.0), P(5.6, 1.0), P(4.4, 4.2), P(-4.4, 4.2), P(-5.6, 1.0) }, Ink, Pen, Layer);
					Stroke({ P(-2.6, 1.0), P(-2.0, -2.2), P(2.2, -2.2), P(2.8, 1.0) }, Ink, Pen, Layer);
					Stroke({ P(2.2, -1.0), P(6.4, -1.0) }, Ink, Pen, Layer);
				}
				else if (What == TEXT("air"))
				{
					// (an aircraft from above: its body, its wings and its tail)
					Stroke({ P(0.0, -5.8), P(0.0, 5.4) }, Ink, Pen * 1.2f, Layer);
					Stroke({ P(-6.0, -0.4), P(0.0, -2.2), P(6.0, -0.4) }, Ink, Pen, Layer);
					Stroke({ P(-2.6, 4.8), P(0.0, 3.6), P(2.6, 4.8) }, Ink, Pen, Layer);
				}
				else if (What == TEXT("naval"))
				{
					// (an anchor)
					Stroke({ P(0.0, -3.6), P(0.0, 5.2) }, Ink, Pen, Layer);
					Stroke({ P(-2.8, -1.6), P(2.8, -1.6) }, Ink, Pen, Layer);
					Stroke({ P(-5.0, 1.6), P(-3.6, 4.2), P(0.0, 5.4), P(3.6, 4.2), P(5.0, 1.6) }, Ink, Pen, Layer);
					Round(X, Y - 4.8 * S, 1.3 * S, Ink, Pen, Layer);
				}
				else if (What == TEXT("industry"))
				{
					// (a works: a saw-toothed roof and a chimney)
					Stroke({ P(-5.8, 4.6), P(-5.8, -0.6), P(-2.6, 1.6), P(-2.6, -0.6), P(0.6, 1.6), P(0.6, -0.6), P(3.0, 1.2), P(3.0, -5.2), P(5.4, -5.2), P(5.4, 4.6), P(-5.8, 4.6) }, Ink, Pen, Layer);
				}
				else
				{
					// (doctrine: an open book)
					Stroke({ P(0.0, -3.4), P(-5.8, -4.6), P(-5.8, 3.4), P(0.0, 4.6), P(5.8, 3.4), P(5.8, -4.6), P(0.0, -3.4), P(0.0, 4.6) }, Ink, Pen, Layer);
				}
			};
			const FSlateFontInfo Label11 = Typed(Px(11.0f), true, 60), Typed10 = Typed(Px(11.0f)), Typed12 = Typed(Px(12.0f)), Name14 = Serif(TEXT("Bold"), Px(14.0f)), Name18 = Serif(TEXT("Bold"), Px(18.0f));
			TMap<FString, const FTechNode*> ById;
			for (const FTechNode& Node : Labs.Nodes)
			{
				ById.Add(Node.Id, &Node);
			}
			const FTechNode* Picked = ById.FindRef(ChosenId());
			const FString PointedId = Player->StagedChartPoint.IsEmpty() ? Hovered : Player->StagedChartPoint;
			auto StateInk = [&](const FString& State) { return State == TEXT("done") ? BrightBrass : State == TEXT("work") ? Lit : State == TEXT("ready") ? Cream : Dim; };

			// ---- The panel: solid, edged, with its four clamps.
			Fill(PanelX, PanelY, PanelW, PanelH, Desk900, 0);
			Edge(PanelX + 0.5, PanelY + 0.5, PanelW - 1.0, PanelH - 1.0, Brass700, 1.0f, 1);
			for (const FVector2D& Corner : { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(1.0, 1.0) })
			{
				const double CX = PanelX + 5.0 + Corner.X * (PanelW - 10.0), CY = PanelY + 5.0 + Corner.Y * (PanelH - 10.0);
				const double DX = Corner.X > 0.5 ? -14.0 : 14.0, DY = Corner.Y > 0.5 ? -14.0 : 14.0;
				Stroke({ FVector2D(CX + DX, CY), FVector2D(CX, CY), FVector2D(CX, CY + DY) }, Brass, 2.0f, 2);
			}

			// ---- The head: whose laboratories and when, how many are done, and a plate for each research team.
			Write(FString::Printf(TEXT("THE LABORATORIES \u00B7 %s \u00B7 %s"), *Labs.Nation.ToUpper(), *Labs.Date.ToUpper()), 30.0, 22.0, Typed(Px(11.0f), true, 30), Brass, 2);
			Write(FString::Printf(TEXT("%d of %d technologies"), Labs.Done, Labs.Total), 30.0, 38.0, Serif(TEXT("Bold"), Px(24.0f)), Cream, 2);
			{
				TArray<const FTechNode*> AtWork;
				for (const FTechNode& Node : Labs.Nodes)
				{
					if (Node.State == TEXT("work"))
					{
						AtWork.Add(&Node);
					}
				}
				const int32 Plates = FMath::Clamp(FMath::Max(Labs.Teams, AtWork.Num()), 1, 4);
				const double PlateW = Plates <= 2 ? 340.0 : (912.0 - (Plates - 1) * 8.0) / Plates, PlateX = 372.0;
				for (int32 Team = 0; Team < Plates; ++Team)
				{
					const double X = PlateX + Team * (PlateW + 8.0), Y = 23.0;
					const FTechNode* Work = AtWork.IsValidIndex(Team) ? AtWork[Team] : nullptr;
					Fill(X, Y, PlateW, 44.0, Work ? Desk700 : Desk800, 1);
					Edge(X + 0.5, Y + 0.5, PlateW - 1.0, 43.0, Work ? Lit : Cream.CopyWithNewOpacity(0.7f), 1.0f, 2);
					Write(FString::Printf(TEXT("TEAM %d"), Team + 1), X + 10.0, Y + 5.0, Typed(Px(11.0f), true, 60), Brass, 3);
					if (Work)
					{
						const FString Months = FString::Printf(TEXT("%d of %d months"), FMath::Max(Work->Of - Work->Left, 0), Work->Of);
						Write(Months, X + PlateW - 10.0 - Wide(Months, Typed10), Y + 5.0, Typed10, Muted, 3);
						Write(Fitted(Work->Name, Name14, PlateW - 20.0), X + 10.0, Y + 17.0, Name14, Cream, 3);
						Fill(X + 10.0, Y + 38.0, PlateW - 20.0, 3.0, Track, 3);
						Fill(X + 10.0, Y + 38.0, (PlateW - 20.0) * FMath::Clamp(Work->Of > 0 ? static_cast<double>(Work->Of - Work->Left) / Work->Of : 0.0, 0.0, 1.0), 3.0, Lit, 4);
					}
					else
					{
						// (ruling 140: an idle team's plate reads NO WORK)
						Write(TEXT("NO WORK"), X + PlateW - 10.0 - Wide(TEXT("NO WORK"), Typed(Px(11.0f), true, 60)), Y + 5.0, Typed(Px(11.0f), true, 60), Amber, 3);
						Write(TEXT("Choose a technology for it"), X + 10.0, Y + 19.0, Serif(TEXT("Italic"), Px(13.0f)), Cream, 3);
					}
				}
			}
			// (the two buttons of the head: the staff's, and the stud that closes the chart)
			{
				const FViewDesk* Desk = Player->FindDesk(TEXT("research"));
				if (Desk)
				{
					const FBox2D Box = StaffBox();
					const bool bOver = Over == 2;
					Fill(Box.Min.X, Box.Min.Y, Box.GetSize().X, Box.GetSize().Y, bOver ? Desk700 : Desk800, 1);
					Edge(Box.Min.X + 0.5, Box.Min.Y + 0.5, Box.GetSize().X - 1.0, Box.GetSize().Y - 1.0, Desk->bStaff ? BrightBrass : Brass700, 1.0f, 2);
					const FString Says = Desk->bStaff ? TEXT("TAKE IT BACK") : TEXT("LET MY STAFF RUN IT");
					Write(Says, Box.GetCenter().X - Wide(Says, Typed(Px(11.0f), true, 60)) * 0.5, Box.Min.Y + 10.0, Typed(Px(11.0f), true, 60), Desk->bStaff ? BrightBrass : Muted, 3);
				}
				const FBox2D Stud = CloseBox();
				Round(Stud.GetCenter().X, Stud.GetCenter().Y, 13.0, Over == 3 ? BrightBrass : Brass, 1.5f, 2);
				Stroke({ Stud.GetCenter() + FVector2D(-5.0, -5.0), Stud.GetCenter() + FVector2D(5.0, 5.0) }, Over == 3 ? BrightBrass : Cream, 1.8f, 3);
				Stroke({ Stud.GetCenter() + FVector2D(-5.0, 5.0), Stud.GetCenter() + FVector2D(5.0, -5.0) }, Over == 3 ? BrightBrass : Cream, 1.8f, 3);
			}

			// ---- The ruler of years: each era's name under a brass rule, a pointer at this month, amber dots over the years ahead.
			{
				int32 Year = 1939, Month = 0;
				{
					static const TCHAR* Names[] = { TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December") };
					FString MonthName, YearText;
					if (Labs.Date.Split(TEXT(" "), &MonthName, &YearText))
					{
						Year = FCString::Atoi(*YearText);
						for (int32 K = 0; K < 12; ++K)
						{
							Month = MonthName == Names[K] ? K : Month;
						}
					}
				}
				// (the years each step spans: before the war, 1940 to 1941, 1942 to 1943, 1944 to 1945)
				auto WhereIs = [](double When)
				{
					const double Starts[5] = { 1938.0, 1940.0, 1942.0, 1944.0, 1946.0 };
					for (int32 Step = 0; Step < 4; ++Step)
					{
						if (When < Starts[Step + 1] || Step == 3)
						{
							const double Share = FMath::Clamp((When - Starts[Step]) / (Starts[Step + 1] - Starts[Step]), 0.0, 1.0);
							return 172.0 + Step * 368.0 + Share * CardW;
						}
					}
					return 172.0;
				};
				for (int32 Step = 0; Step < 4; ++Step)
				{
					const double X = 172.0 + Step * 368.0;
					Fill(X, 84.0, CardW, 1.5, Brass, 2);
					Write(Labs.Eras.IsValidIndex(Step + 1) ? Labs.Eras[Step + 1].ToUpper() : FString(), X, 91.0, Label11, Muted, 2);
				}
				const double Today = Year + Month / 12.0;
				for (int32 Ahead = FMath::Max(Year + 1, 1940); Ahead <= 1945; ++Ahead)
				{
					Disc(WhereIs(Ahead), 84.5, 2.6, Amber, 3);
				}
				const double PointerX = WhereIs(Today);
				Stroke({ FVector2D(PointerX - 5.0, 76.0), FVector2D(PointerX + 5.0, 76.0), FVector2D(PointerX, 83.0), FVector2D(PointerX - 5.0, 76.0) }, BrightBrass, 2.0f, 4);
			}

			// ---- The shelves, each with its plate: the branch's sign in a ring, its name, how many are done, and a meter.
			for (const FChartLane& Lane : Chart.Lanes)
			{
				Fill(22.0, Lane.Top, PanelW - 16.0, Lane.High, Desk800.CopyWithNewOpacity(0.55f), 1);
				Fill(22.0, Lane.Top, 128.0, Lane.High, Desk800, 2);
				Edge(22.5, Lane.Top + 0.5, 127.0, Lane.High - 1.0, Brass700, 1.0f, 3);
				Round(52.0, Lane.Top + 34.0, 15.0, Brass, 1.5f, 3);
				Sign(Lane.Id, 52.0, Lane.Top + 34.0, 1.45, BrightBrass, 4);
				Write(Lane.Label, 34.0, Lane.Top + 56.0, Name18, Cream, 3);
				for (const FTechBranch& Branch : Labs.Branches)
				{
					if (Branch.Id == Lane.Id)
					{
						Write(FString::Printf(TEXT("%d OF %d"), Branch.Done, Branch.Total), 34.0, Lane.Top + 84.0, Typed(Px(11.0f), true, 60), Brass, 3);
						Fill(34.0, Lane.Top + 100.0, 104.0, 4.0, Track, 3);
						Fill(34.0, Lane.Top + 100.0, 104.0 * (Branch.Total > 0 ? static_cast<double>(Branch.Done) / Branch.Total : 0.0), 4.0, BrightBrass, 4);
					}
				}
			}

			// ---- The lines. One runs into each card from the technology it grows from, and wears the state of the card it leads to.
			auto LaneOf = [&Chart](const FString& Id) -> const FChartLane*
			{
				return Chart.Lanes.FindByPredicate([&Id](const FChartLane& Each) { return Each.Id == Id; });
			};
			auto Road = [&](const FChartCard& From, const FChartCard& To)
			{
				TArray<FVector2D> Points;
				if (To.Line == TEXT("under"))
				{
					// (it ducks under the card between, in the band below the shelf's last row, leaving and entering by the foot of each card)
					const FChartLane* Lane = LaneOf(To.Lane);
					const double Band = Lane ? Lane->Top + Lane->High - 3.0 : To.Y + CardH + 7.0;
					Points = { FVector2D(From.X + CardW - 22.0, From.Y + CardH), FVector2D(From.X + CardW - 22.0, Band), FVector2D(To.X + 22.0, Band), FVector2D(To.X + 22.0, To.Y + CardH) };
				}
				else if (FMath::Abs(From.Y - To.Y) > 0.5)
				{
					// (it bends in the gutter before the card it leads to)
					const double Gutter = To.X - 38.0;
					Points = { FVector2D(From.X + CardW, From.Y + CardH * 0.5), FVector2D(Gutter, From.Y + CardH * 0.5), FVector2D(Gutter, To.Y + CardH * 0.5), FVector2D(To.X, To.Y + CardH * 0.5) };
				}
				else
				{
					Points = { FVector2D(From.X + CardW, From.Y + CardH * 0.5), FVector2D(To.X, To.Y + CardH * 0.5) };
				}
				return Points;
			};
			// (a need that is not the card's own line: out by the gutter after the card it comes from, along the gap
			// above the row it goes to, and in by the gutter before it. It may cross lines; it never crosses a card.)
			auto RivetRoad = [&](const FChartCard& From, const FChartCard& To)
			{
				const double Out = From.X + CardW + 30.0, In = To.X - 30.0, Gap = To.Y - 5.0;
				return TArray<FVector2D>({ FVector2D(From.X + CardW, From.Y + CardH * 0.5), FVector2D(Out, From.Y + CardH * 0.5), FVector2D(Out, Gap), FVector2D(In, Gap), FVector2D(In, To.Y + CardH * 0.5), FVector2D(To.X, To.Y + CardH * 0.5) });
			};
			for (const TPair<FString, FChartCard>& Each : Chart.Cards)
			{
				const FChartCard& Card = Each.Value;
				const FChartCard* From = Card.From.IsEmpty() ? nullptr : Chart.Cards.Find(Card.From);
				const FTechNode* Node = ById.FindRef(Each.Key);
				if (!From || !Node)
				{
					continue;
				}
				const TArray<FVector2D> Points = Road(*From, Card);
				if (Node->State == TEXT("done"))
				{
					Stroke(Points, BrightBrass.CopyWithNewOpacity(0.9f), 2.5f, 3);
				}
				else if (Node->State == TEXT("work"))
				{
					Stroke(Points, Lit, 2.0f, 3, 7.0, 5.0);
				}
				else if (Node->State == TEXT("ready"))
				{
					Stroke(Points, Cream.CopyWithNewOpacity(0.75f), 1.2f, 3);
				}
				else
				{
					Stroke(Points, Dim.CopyWithNewOpacity(0.8f), 1.6f, 3, 1.6, 4.0);
				}
				Disc(Points[0].X, Points[0].Y, 2.6, StateInk(Node->State), 4);
				Disc(Points.Last().X, Points.Last().Y, 2.6, StateInk(Node->State), 4);
			}
			// (when a card is chosen or pointed at, all its roads light: every line in and out, and the rivets' lines too)
			for (const FString& Shown : { ChosenId(), PointedId })
			{
				const FChartCard* Mine = Shown.IsEmpty() ? nullptr : Chart.Cards.Find(Shown);
				const FTechNode* Node = ById.FindRef(Shown);
				if (!Mine || !Node)
				{
					continue;
				}
				const FLinearColor Pale(FColor::FromHex(TEXT("F0DCA8")));
				if (const FChartCard* From = Mine->From.IsEmpty() ? nullptr : Chart.Cards.Find(Mine->From))
				{
					Stroke(Road(*From, *Mine), Pale, 2.0f, 5);
				}
				for (const FString& Need : Node->NeedIds)
				{
					const FChartCard* Other = Need == Mine->From ? nullptr : Chart.Cards.Find(Need);
					if (Other)
					{
						Stroke(RivetRoad(*Other, *Mine), Pale, 2.0f, 5);
					}
				}
				for (const FTechNode& Later : Labs.Nodes)
				{
					const FChartCard* Next = Later.NeedIds.Contains(Shown) ? Chart.Cards.Find(Later.Id) : nullptr;
					if (Next)
					{
						Stroke(Next->From == Shown ? Road(*Mine, *Next) : RivetRoad(*Mine, *Next), Pale, 2.0f, 5);
					}
				}
			}

			// ---- The cards.
			for (const TPair<FString, FChartCard>& Each : Chart.Cards)
			{
				const FChartCard& Card = Each.Value;
				const FTechNode* Node = ById.FindRef(Each.Key);
				if (!Node)
				{
					continue;
				}
				const bool bChosen = Picked == Node, bPointed = PointedId == Node->Id;
				const FString& State = Node->State;
				const double X = Card.X, Y = Card.Y;
				if (State == TEXT("done"))
				{
					Fill(X, Y, CardW, CardH, bPointed ? BrassGround * 1.25f : BrassGround, 6);
					Edge(X + 0.5, Y + 0.5, CardW - 1.0, CardH - 1.0, Brass, 1.2f, 7);
				}
				else if (State == TEXT("work"))
				{
					Fill(X, Y, CardW, CardH, bPointed ? Desk700 * 1.2f : Desk700, 6);
					Edge(X + 0.5, Y + 0.5, CardW - 1.0, CardH - 1.0, Lit, 1.6f, 7);
				}
				else if (State == TEXT("ready"))
				{
					Fill(X, Y, CardW, CardH, bPointed ? Desk700 : Desk800, 6);
					Edge(X + 0.5, Y + 0.5, CardW - 1.0, CardH - 1.0, Cream.CopyWithNewOpacity(0.85f), 1.0f, 7);
				}
				else
				{
					Fill(X, Y, CardW, CardH, bPointed ? Desk800 : Desk900, 6);
					Edge(X + 0.5, Y + 0.5, CardW - 1.0, CardH - 1.0, Dim.CopyWithNewOpacity(0.7f), 1.0f, 7, 4.0, 3.0);
				}
				if (bChosen)
				{
					// (the chosen card: a pale edge, and a second line inside it)
					Edge(X - 1.5, Y - 1.5, CardW + 3.0, CardH + 3.0, FLinearColor(FColor::FromHex(TEXT("F0DCA8"))), 1.6f, 8);
					Edge(X + 3.5, Y + 3.5, CardW - 7.0, CardH - 7.0, Cream.CopyWithNewOpacity(0.45f), 1.0f, 8);
				}
				// (its state's sign in a disc: a state is never told by colour alone)
				const FLinearColor Ink = StateInk(State);
				Round(X + 19.0, Y + 19.0, 11.0, Ink.CopyWithNewOpacity(State == TEXT("locked") ? 0.6f : 0.9f), 1.2f, 8);
				Sign(State, X + 19.0, Y + 19.0, 1.0, Ink, 9);
				// The right end of the card: the months (amber after a small clock when it is ahead of its time), or
				// the work done with a bar along the card's foot; then its tag and its rivets, right to left.
				double Right = X + CardW - 9.0;
				if (State == TEXT("ready"))
				{
					const bool bAhead = Node->Base > 0 && Node->Months > Node->Base;
					const FString Months = FString::Printf(TEXT("%d mo"), Node->Months);
					Right -= Wide(Months, Typed12);
					Write(Months, Right, Y + 12.0, Typed12, bAhead ? Amber : Muted, 9);
					if (bAhead)
					{
						Right -= 12.0;
						Sign(TEXT("clock"), Right + 3.0, Y + 19.0, 0.9, Amber, 9);
					}
					Right -= 8.0;
				}
				else if (State == TEXT("work"))
				{
					const FString Months = FString::Printf(TEXT("%d of %d"), FMath::Max(Node->Of - Node->Left, 0), Node->Of);
					Right -= Wide(Months, Typed12);
					Write(Months, Right, Y + 12.0, Typed12, Lit, 9);
					Right -= 8.0;
					Fill(X + 1.0, Y + CardH - 4.0, CardW - 2.0, 3.0, Track, 8);
					Fill(X + 1.0, Y + CardH - 4.0, (CardW - 2.0) * FMath::Clamp(Node->Of > 0 ? static_cast<double>(Node->Of - Node->Left) / Node->Of : 0.0, 0.0, 1.0), 3.0, Lit, 9);
				}
				if (!Node->Tag.IsEmpty() && State != TEXT("done"))
				{
					const FSlateFontInfo TagFace = Typed(Px(10.0f), true, 40);
					const double TagW = Wide(Node->Tag, TagFace) + 10.0;
					Right -= TagW;
					Edge(Right + 0.5, Y + 11.5, TagW - 1.0, 15.0, Brass, 1.0f, 8);
					Write(Node->Tag, Right + 5.0, Y + 14.0, TagFace, BrightBrass, 9);
					Right -= 6.0;
				}
				for (int32 K = Node->NeedIds.Num() - 1; K >= 0; --K)
				{
					// (a rivet: a need that is not the card's own line, with the sign of the branch it comes from; brass
					// when he has that technology, a faint dashed ring when he has not)
					if (Node->NeedIds[K] == Card.From)
					{
						continue;
					}
					const FChartCard* Other = Chart.Cards.Find(Node->NeedIds[K]);
					const bool bHas = Node->NeedDone.IsValidIndex(K) && Node->NeedDone[K];
					Right -= 17.0;
					if (bHas)
					{
						Disc(Right + 8.5, Y + 19.0, 8.5, BrassGround * 1.6f, 8);
						Round(Right + 8.5, Y + 19.0, 8.5, BrightBrass, 1.2f, 9);
					}
					else
					{
						Round(Right + 8.5, Y + 19.0, 8.5, Dim, 1.2f, 9, 3.0, 2.5);
					}
					Sign(Other ? Other->Lane : FString(TEXT("doctrine")), Right + 8.5, Y + 19.0, 0.78, bHas ? BrightBrass : Dim, 10);
					Right -= 5.0;
				}
				Write(Fitted(Node->Name, Name14, Right - (X + 38.0)), X + 38.0, Y + 10.0, Name14, State == TEXT("locked") ? Dim : Cream, 9);
			}

			// ---- The chosen technology's strip, in the game's own words.
			Fill(22.0, 903.0, PanelW - 16.0, 56.0, Desk800, 1);
			Edge(22.5, 903.5, PanelW - 17.0, 55.0, Brass700, 1.0f, 2);
			if (Picked)
			{
				const FTechNode& Node = *Picked;
				Round(46.0, 918.0, 11.0, StateInk(Node.State), 1.2f, 3);
				Sign(Node.State, 46.0, 918.0, 1.0, StateInk(Node.State), 4);
				Write(Node.Name, 66.0, 904.0, Name18, Cream, 3);
				double Cursor = 66.0 + Wide(Node.Name, Name18) + 16.0;
				{
					FString BranchName = Node.Branch.ToUpper();
					for (const FTechBranch& Branch : Labs.Branches)
					{
						BranchName = Branch.Id == Node.Branch ? Branch.Label.ToUpper() : BranchName;
					}
					const FString Where = BranchName + TEXT("  \u00B7  ") + (Labs.Eras.IsValidIndex(Node.Tier) ? Labs.Eras[Node.Tier].ToUpper() : FString());
					Write(Where, Cursor, 910.0, Typed(Px(11.0f), true, 60), Brass, 3);
					Cursor += Wide(Where, Typed(Px(11.0f), true, 60)) + 18.0;
				}
				{
					const bool bAhead = Node.Base > 0 && Node.Months > Node.Base;
					const FString Months = Node.State == TEXT("done") ? FString(TEXT("Researched."))
						: Node.State == TEXT("work") ? FString::Printf(TEXT("A team is at work on it: %d of %d months."), FMath::Max(Node.Of - Node.Left, 0), Node.Of)
						: bAhead ? FString::Printf(TEXT("%d months now: it is ahead of its time. %d months from %d."), Node.Months, Node.Base, Node.Year)
						: FString::Printf(TEXT("%d months of one team's work."), Node.Months);
					Write(Months, Cursor, 909.0, Typed12, bAhead && Node.State != TEXT("done") && Node.State != TEXT("work") ? Amber : Muted, 3);
				}
				// (what it needs, a tick or a cross by each name; and what it leads to, naming the branch when it is another one)
				// (it begins where the name ends: the name is two typed lines tall, and a line begun under it ran through its feet)
				double Along = 66.0 + Wide(Node.Name, Name18) + 16.0;
				const FSlateFontInfo Small = Typed(Px(11.0f), true, 60), Plain = Typed(Px(11.0f));
				if (Node.NeedIds.Num() > 0)
				{
					Write(TEXT("NEEDS"), Along, 922.0, Small, Brass, 3);
					Along += Wide(TEXT("NEEDS"), Small) + 10.0;
					for (int32 K = 0; K < Node.NeedIds.Num(); ++K)
					{
						const bool bHas = Node.NeedDone.IsValidIndex(K) && Node.NeedDone[K];
						if (bHas)
						{
							Stroke({ FVector2D(Along, 929.0), FVector2D(Along + 3.0, 932.5), FVector2D(Along + 8.5, 924.5) }, BrightBrass, 1.8f, 4);
						}
						else
						{
							// (ruling 160: red means act by a date or lose. A need not yet met is said in paper.)
							Stroke({ FVector2D(Along + 0.5, 925.0), FVector2D(Along + 8.0, 932.5) }, Dim, 1.8f, 4);
							Stroke({ FVector2D(Along + 0.5, 932.5), FVector2D(Along + 8.0, 925.0) }, Dim, 1.8f, 4);
						}
						Along += 13.0;
						const FString Name = Node.NeedNames.IsValidIndex(K) ? Node.NeedNames[K] : Node.NeedIds[K];
						Write(Name, Along, 921.0, Plain, bHas ? Cream : Muted, 3);
						Along += Wide(Name, Plain) + 14.0;
					}
					Along += 12.0;
				}
				{
					TArray<FString> Leads;
					for (const FTechNode& Later : Labs.Nodes)
					{
						if (Later.NeedIds.Contains(Node.Id))
						{
							FString Says = Later.Name;
							if (Later.Branch != Node.Branch)
							{
								for (const FTechBranch& Branch : Labs.Branches)
								{
									Says = Branch.Id == Later.Branch ? Later.Name + TEXT(" (") + Branch.Label + TEXT(")") : Says;
								}
							}
							Leads.Add(Says);
						}
					}
					if (Leads.Num() > 0)
					{
						Write(TEXT("LEADS TO"), Along, 922.0, Small, Brass, 3);
						Along += Wide(TEXT("LEADS TO"), Small) + 10.0;
						Write(Fitted(FString::Join(Leads, TEXT(", ")), Plain, 1290.0 - Along), Along, 921.0, Plain, Cream, 3);
					}
				}
				{
					// (what it is and what it does; and when it is locked, what it waits on, in the alarm's colour)
					// (the art director's review of build 1448: the sentence stood on the strip's lower rule; the strip's three lines are set higher, and it now clears the rule by more than eight pixels)
					const FSlateFontInfo Body = Serif(TEXT("Regular"), Px(15.0f));
					const FString Waits = Node.State == TEXT("locked") && !Node.Needs.IsEmpty() ? FString::Printf(TEXT("Needs %s first."), *Node.Needs.Replace(TEXT(", "), TEXT(" and "))) : FString();
					const double WaitsW = Waits.IsEmpty() ? 0.0 : Wide(Waits, Body) + 12.0;
					const FString Says = Fitted(Node.Text + TEXT(" ") + Node.Rule, Body, 1290.0 - 66.0 - WaitsW);
					Write(Says, 66.0, 933.0, Body, Muted, 3);
					if (!Waits.IsEmpty())
					{
						Write(Waits, 66.0 + Wide(Says, Body) + 12.0, 933.0, Body, Cream, 3);
					}
				}
				// (the one brass button: RESEARCH and its months, dimmed when the card is locked or every team is busy; STOP on a card in work)
				const FBox2D Button = ActBox();
				const bool bStop = Node.State == TEXT("work") && Node.bStop, bCan = Node.bCan;
				if (Node.State != TEXT("done"))
				{
					const bool bLive = bStop || bCan;
					const FLinearColor Ground = !bLive ? Desk700 : bStop ? Desk700 : (Over == 1 ? BrightBrass : Brass);
					Fill(Button.Min.X, Button.Min.Y, Button.GetSize().X, Button.GetSize().Y, Ground, 3);
					Edge(Button.Min.X + 0.5, Button.Min.Y + 0.5, Button.GetSize().X - 1.0, Button.GetSize().Y - 1.0, bLive ? BrightBrass : Brass700, 1.0f, 4);
					const FString Says = bStop ? FString(TEXT("STOP")) : FString::Printf(TEXT("RESEARCH  \u00B7  %d MONTHS"), Node.Months);
					const FSlateFontInfo Face = Typed(Px(12.0f), true, 80);
					Write(Says, Button.GetCenter().X - Wide(Says, Face) * 0.5, Button.Min.Y + 12.0, Face, !bLive ? Dim : bStop ? Cream : FLinearColor(FColor::FromHex(TEXT("1F1808"))), 5);
					if (!bLive && !Node.Why.IsEmpty())
					{
						// (why it cannot be begun now, whole, on the strip's first line and ending where the button begins)
						const FSlateFontInfo WhyFace = Typed(Px(11.0f));
						const FString WhySays = Fitted(Node.Why, WhyFace, 560.0);
						Write(WhySays, Button.Min.X - 18.0 - Wide(WhySays, WhyFace), 910.0, WhyFace, Cream, 5);
					}
				}
			}
			else
			{
				Write(TEXT("Choose a technology to read what it is and what it leads to."), 46.0, 922.0, Serif(TEXT("Italic"), Px(15.0f)), Muted, 3);
			}

			// ---- The key; and, where the staff runs research, its own line.
			{
				double Along = 30.0;
				const FViewDesk* Desk = Player->FindDesk(TEXT("research"));
				if (Desk && Desk->bStaff && !Desk->Line.IsEmpty())
				{
					Write(Fitted(Desk->Line, Serif(TEXT("Italic"), Px(12.0f)), 760.0), Along, 966.0, Serif(TEXT("Italic"), Px(12.0f)), Cream, 3);
					Along = 820.0;
				}
				static const TCHAR* States[] = { TEXT("done"), TEXT("work"), TEXT("ready"), TEXT("locked") };
				static const TCHAR* Words[] = { TEXT("Researched"), TEXT("Being worked on"), TEXT("Ready to begin"), TEXT("Locked") };
				for (int32 K = 0; K < 4; ++K)
				{
					Sign(States[K], Along + 6.0, 974.0, 0.8, StateInk(States[K]), 3);
					Write(Words[K], Along + 18.0, 967.0, Typed10, Muted, 3);
					Along += 18.0 + Wide(Words[K], Typed10) + 22.0;
				}
				Round(Along + 7.0, 974.0, 6.5, Dim, 1.0f, 3, 3.0, 2.5);
				Write(TEXT("A rivet: it also needs a technology from elsewhere"), Along + 20.0, 967.0, Typed10, Muted, 3);
				Along += 20.0 + Wide(TEXT("A rivet: it also needs a technology from elsewhere"), Typed10) + 22.0;
				Sign(TEXT("clock"), Along + 6.0, 974.0, 0.9, Amber, 3);
				Write(TEXT("Ahead of its time: it takes longer now"), Along + 18.0, 967.0, Typed10, Amber, 3);
			}
			return LayerId + 11;
		}

		virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			const FVector2D Here = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) + FVector2D(PanelX, PanelY);
			Hovered = CardAt(Here);
			Over = ActBox().IsInside(Here) ? 1 : StaffBox().IsInside(Here) ? 2 : CloseBox().IsInside(Here) ? 3 : 0;
			return FReply::Handled();
		}

		virtual void OnMouseLeave(const FPointerEvent& Event) override
		{
			Hovered.Reset();
			Over = 0;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (!Player.IsValid() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Handled();
			}
			const FVector2D Here = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) + FVector2D(PanelX, PanelY);
			const FString Card = CardAt(Here);
			if (!Card.IsEmpty())
			{
				Chosen = Card;
			}
			else if (CloseBox().IsInside(Here))
			{
				Player->CloseResearch();
			}
			else if (StaffBox().IsInside(Here) && Player->FindDesk(TEXT("research")))
			{
				Player->PressDesk(TEXT("research"));
			}
			else if (ActBox().IsInside(Here))
			{
				const FString Id = ChosenId();
				for (const FTechNode& Node : Player->GetResearch().Nodes)
				{
					if (Node.Id == Id && Node.State == TEXT("work") && Node.bStop)
					{
						Player->StopTech(Id);
					}
					else if (Node.Id == Id && Node.bCan)
					{
						Player->StartTech(Id);
					}
				}
			}
			return FReply::Handled();
		}

	private:
		static FBox2D ActBox() { return FBox2D(FVector2D(1318.0, 911.0), FVector2D(1568.0, 951.0)); }
		static FBox2D StaffBox() { return FBox2D(FVector2D(1300.0, 28.0), FVector2D(1500.0, 62.0)); }
		static FBox2D CloseBox() { return FBox2D(FVector2D(1530.0, 30.0), FVector2D(1562.0, 62.0)); }

		FString CardAt(const FVector2D& Here) const
		{
			for (const TPair<FString, FChartCard>& Each : ChartLayout().Cards)
			{
				if (Here.X >= Each.Value.X && Here.X <= Each.Value.X + CardW && Here.Y >= Each.Value.Y && Here.Y <= Each.Value.Y + CardH)
				{
					return Each.Key;
				}
			}
			return FString();
		}

		/** The card whose strip is shown: the one he chose; before he has chosen, a team's work, or the first he could begin. */
		FString ChosenId() const
		{
			if (Player.IsValid() && !Player->StagedChartChoice.IsEmpty())
			{
				return Player->StagedChartChoice; // (for the photographer)
			}
			if (!Chosen.IsEmpty() || !Player.IsValid())
			{
				return Chosen;
			}
			FString First;
			for (const FTechNode& Node : Player->GetResearch().Nodes)
			{
				if (Node.State == TEXT("work"))
				{
					return Node.Id;
				}
				if (First.IsEmpty() && Node.State == TEXT("ready"))
				{
					First = Node.Id;
				}
			}
			return First;
		}

		TWeakObjectPtr<ACampaignPlayerController> Player;
		FString Chosen, Hovered;
		int32 Over = 0; // the button under the pointer: 1 the strip's, 2 the staff's, 3 the stud that closes
	};

	/**
	 * (Rules version 84.) The support weapons' signs, the art director's (art-director/works/weapon-signs.json, with the
	 * three the browser adds: a tank, a gun, and an eye for what is scouted): strokes of one weight on a box of 24,
	 * drawn here on a box `Size` across with its corner at `Corner`.
	 */
	void PaintWeaponSign(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geometry, const FString& Sign, const FVector2D& Corner, double Size, const FLinearColor& Ink)
	{
		const double K = Size / 24.0;
		const float Pen = static_cast<float>(FMath::Max(1.2, 1.9 * K));
		auto P = [&Corner, K](double X, double Y) { return FVector2D(Corner.X + X * K, Corner.Y + Y * K); };
		auto Line = [&](const TArray<FVector2D>& Points)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, Pen);
		};
		auto Bow = [&](double CX, double CY, double RX, double RY, double From, double To)
		{
			TArray<FVector2D> Points;
			const int32 Steps = FMath::Max(4, FMath::CeilToInt32(FMath::Abs(To - From) / 24.0));
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Angle = FMath::DegreesToRadians(FMath::Lerp(From, To, static_cast<double>(Step) / Steps));
				Points.Add(P(CX + FMath::Cos(Angle) * RX, CY + FMath::Sin(Angle) * RY));
			}
			return Points;
		};
		auto Ring = [&](double CX, double CY, double R) { Line(Bow(CX, CY, R, R, 0.0, 360.0)); };
		if (Sign == TEXT("anti-tank"))
		{
			// (a gun on its wheel, the barrel level, its trail to the ground)
			Ring(10.5, 16.8, 3.7);
			Line({ P(8.0, 9.6), P(22.4, 9.6) });
			Line({ P(22.4, 7.6), P(22.4, 11.6) });
			Line({ P(10.5, 4.2), P(10.5, 13.0) });
			Line({ P(8.0, 9.6), P(2.0, 20.4) });
		}
		else if (Sign == TEXT("machine-gun"))
		{
			// (a gun on its tripod)
			Line({ P(5.0, 7.8), P(11.2, 7.8), P(11.2, 11.2), P(5.0, 11.2), P(5.0, 7.8) });
			Line({ P(11.2, 9.5), P(21.5, 9.5) });
			Line({ P(4.6, 20.0), P(8.6, 11.2), P(13.4, 20.0) });
			Line({ P(5.0, 9.5), P(2.6, 9.5) });
			Line({ P(2.6, 8.0), P(2.6, 11.0) });
		}
		else if (Sign == TEXT("mortar"))
		{
			// (a tube on its base plate, with its leg)
			Line({ P(7.7, 18.5), P(14.1, 4.5), P(16.3, 5.5), P(9.9, 19.5), P(7.7, 18.5) });
			Line({ P(4.0, 20.5), P(13.2, 20.5) });
			Line({ P(13.4, 11.8), P(19.8, 20.5) });
		}
		else if (Sign == TEXT("sniper"))
		{
			// (a telescopic sight)
			Ring(12.0, 12.0, 7.0);
			Line({ P(12.0, 2.2), P(12.0, 7.8) });
			Line({ P(12.0, 16.2), P(12.0, 21.8) });
			Line({ P(2.2, 12.0), P(7.8, 12.0) });
			Line({ P(16.2, 12.0), P(21.8, 12.0) });
			Line({ P(11.6, 12.0), P(12.4, 12.0) });
		}
		else if (Sign == TEXT("tank"))
		{
			// (a tank from the side: its track, its hull, its turret and its gun)
			TArray<FVector2D> Track = Bow(19.0, 17.0, 3.0, 3.0, -90.0, 90.0);
			Track.Append(Bow(5.0, 17.0, 3.0, 3.0, 90.0, 270.0));
			Track.Add(P(19.0, 14.0));
			Line(Track);
			Line({ P(7.0, 14.0), P(7.0, 11.0), P(15.0, 11.0), P(16.5, 14.0) });
			Line({ P(12.0, 11.0), P(12.0, 8.0), P(15.0, 8.0) });
			Line({ P(15.0, 9.5), P(21.5, 9.5) });
			for (const double Wheel : { 7.0, 11.0, 15.0, 19.0 })
			{
				Line({ P(Wheel - 0.4, 17.0), P(Wheel + 0.4, 17.0) });
			}
		}
		else if (Sign == TEXT("gun"))
		{
			// (a field gun: its wheel, its barrel raised, its trail)
			Ring(8.0, 16.0, 4.0);
			Line({ P(8.0, 16.0), P(20.0, 7.0) });
			Line({ P(10.5, 12.8), P(5.0, 20.0) });
			Line({ P(17.8, 8.6), P(18.9, 10.1) });
		}
		else if (Sign == TEXT("defence"))
		{
			// (a shield: the walls of a town)
			Line({ P(4.5, 4.0), P(19.5, 4.0), P(19.5, 12.0), P(17.6, 16.6), P(12.0, 21.0), P(6.4, 16.6), P(4.5, 12.0), P(4.5, 4.0) });
		}
		else if (Sign == TEXT("scouted"))
		{
			// (an eye)
			TArray<FVector2D> Lids = Bow(12.0, 15.2, 10.4, 9.7, 198.0, 342.0);
			Lids.Append(Bow(12.0, 8.8, 10.4, 9.7, 18.0, 162.0));
			const FVector2D Corner0 = Lids[0]; // (a copy: an array is never handed its own element)
			Lids.Add(Corner0);
			Line(Lids);
			Ring(12.0, 12.0, 3.0);
		}
	}

	/** One of the weapons' signs as a thing of its own, for a row of them: on a corps' card. */
	class SWeaponSign : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWeaponSign) : _Size(15.0f), _Ink(FLinearColor::White) {}
			SLATE_ARGUMENT(FString, Sign)
			SLATE_ARGUMENT(float, Size)
			SLATE_ARGUMENT(FLinearColor, Ink)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Sign = Args._Sign;
			Size = Args._Size;
			Ink = Args._Ink;
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			PaintWeaponSign(OutDrawElements, LayerId + 1, Geometry, Sign, FVector2D::ZeroVector, Size, Ink);
			return LayerId + 1;
		}

	private:
		FString Sign;
		float Size = 15.0f;
		FLinearColor Ink = FLinearColor::White;
	};

	/** A space, or the space that holds two words together (CampaignText::Phrases sets those inside a phrase). */
	bool IsGap(TCHAR Char)
	{
		return Char == TEXT(' ') || Char == static_cast<TCHAR>(0x00A0);
	}

	/** Words with the spaces at either end taken off, the held ones too. */
	FString TrimGaps(const FString& Words)
	{
		int32 First = 0, Last = Words.Len();
		while (First < Last && IsGap(Words[First]))
		{
			++First;
		}
		while (Last > First && IsGap(Words[Last - 1]))
		{
			--Last;
		}
		return Words.Mid(First, Last - First);
	}

	/**
	 * (The UI director's review of build 1628, item 2.) How wide a line is drawn, in the widget's own units, at the
	 * scale it is painted at. Lettering is laid at the size it is shown (at 1,441 by 878 a card paints below its own
	 * size), and each letter's step rounds to a whole pixel there, so a line measured at scale 1 can be drawn a twentieth
	 * wider than it measured: the defender's roster, set to the right, ran a letter past its column.
	 */
	double PaintWide(const FString& Words, const FSlateFontInfo& Face, double Scale = 1.0)
	{
		const double At = Scale > 0.01 ? Scale : 1.0;
		return static_cast<double>(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Words, Face, static_cast<float>(At)).X) / At;
	}

	/**
	 * (The UI director, check 19: no word is cut at an edge, and no line begins or ends with a dot.) A phrase set to a
	 * measure: where it is wider it folds at its spaces, and with bBracket a name folds before its bracket first
	 * ("Third Army" over "(2nd Division and 1st Cavalry Division)"). A word is never cut: one wider than the measure
	 * stands alone on its line. Scale is the one it is painted at (PaintWide).
	 */
	void FoldWords(const FString& Phrase, const FSlateFontInfo& Face, double Room, TArray<FString>& Out, bool bBracket = false, double Scale = 1.0)
	{
		auto Wide = [&Face, Scale](const FString& Words) { return PaintWide(Words, Face, Scale); };
		TArray<FString> Pieces;
		int32 Bracket = INDEX_NONE;
		if (bBracket && Wide(Phrase) > Room && Phrase.FindChar(TEXT('('), Bracket) && Bracket > 0)
		{
			Pieces.Add(TrimGaps(Phrase.Left(Bracket)));
			Pieces.Add(TrimGaps(Phrase.Mid(Bracket)));
		}
		else
		{
			Pieces.Add(Phrase);
		}
		for (const FString& Piece : Pieces)
		{
			if (Piece.IsEmpty())
			{
				continue;
			}
			if (Wide(Piece) <= Room)
			{
				Out.Add(Piece);
				continue;
			}
			// (its words, parted at every space, the held ones too)
			TArray<FString> Words;
			FString Word;
			for (int32 At = 0; At < Piece.Len(); ++At)
			{
				if (IsGap(Piece[At]))
				{
					if (!Word.IsEmpty())
					{
						Words.Add(Word);
						Word.Reset();
					}
				}
				else
				{
					Word.AppendChar(Piece[At]);
				}
			}
			if (!Word.IsEmpty())
			{
				Words.Add(Word);
			}
			FString Line;
			for (const FString& Each : Words)
			{
				const FString Longer = Line.IsEmpty() ? Each : Line + TEXT(" ") + Each;
				if (!Line.IsEmpty() && Wide(Longer) > Room)
				{
					Out.Add(Line);
					Line = Each;
				}
				else
				{
					Line = Longer;
				}
			}
			if (!Line.IsEmpty())
			{
				Out.Add(Line);
			}
		}
	}

	/**
	 * (Check 19.) A line of phrases parted by dots ("Odds of 1.5 to 1 in your favour  ·  about 62% to take Mexico
	 * City"), folded to a measure. A fold falls where a dot stands, and the dot goes with it: the fold parts the two
	 * facts already, so no line begins or ends with a dot. A phrase wider than the measure on its own folds at a space.
	 * A line break in the text stays one. Scale is the one it is painted at (PaintWide).
	 */
	void FoldPhrases(const FString& Text, const FSlateFontInfo& Face, double Room, TArray<FString>& Out, double Scale = 1.0)
	{
		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		if (Room < 24.0)
		{
			Out.Append(Paragraphs); // (not laid out yet: nothing to fold to)
			return;
		}
		auto Wide = [&Face, Scale](const FString& Words) { return PaintWide(Words, Face, Scale); };
		const TCHAR Dot = static_cast<TCHAR>(0x00B7);
		for (const FString& Paragraph : Paragraphs)
		{
			// The phrases, and the dots between them with their spaces, as written.
			TArray<FString> Phrases, Seps;
			int32 Start = 0;
			for (int32 At = 0; At < Paragraph.Len(); ++At)
			{
				if (Paragraph[At] != Dot)
				{
					continue;
				}
				int32 Left = At, Right = At + 1;
				while (Left > Start && IsGap(Paragraph[Left - 1]))
				{
					--Left;
				}
				while (Right < Paragraph.Len() && IsGap(Paragraph[Right]))
				{
					++Right;
				}
				Phrases.Add(TrimGaps(Paragraph.Mid(Start, Left - Start)));
				Seps.Add(Paragraph.Mid(Left, Right - Left));
				Start = Right;
				At = Right - 1;
			}
			Phrases.Add(TrimGaps(Paragraph.Mid(Start)));
			FString Line;
			for (int32 Index = 0; Index < Phrases.Num(); ++Index)
			{
				const FString& Phrase = Phrases[Index];
				if (Phrase.IsEmpty())
				{
					continue;
				}
				if (!Line.IsEmpty())
				{
					const FString Longer = Line + Seps[Index - 1] + Phrase;
					if (Wide(Longer) <= Room)
					{
						Line = Longer;
						continue;
					}
					Out.Add(Line); // (the fold falls at the dot, and the dot goes with it)
					Line.Reset();
				}
				TArray<FString> Folded;
				FoldWords(Phrase, Face, Room, Folded, false, Scale);
				for (int32 K = 0; K + 1 < Folded.Num(); ++K)
				{
					Out.Add(Folded[K]);
				}
				if (Folded.Num() > 0)
				{
					Line = Folded.Last();
				}
			}
			Out.Add(Line); // (an empty paragraph stays an empty line)
		}
	}

	/**
	 * (Check 19.) A line of phrases parted by dots, drawn folded to its own width by FoldPhrases: in place of a text
	 * block that wraps by itself, whose folds left the dot hanging at a line's end. Its slot must give it the whole
	 * width it may take (HAlign_Fill); it is set left, centred or right within it.
	 */
	class SPhraseText : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPhraseText) : _Ink(FLinearColor::White), _Justify(ETextJustify::Left), _LineHigh(0.0f), _Guess(600.0f) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ARGUMENT(FSlateFontInfo, Face)
			SLATE_ARGUMENT(FLinearColor, Ink)
			SLATE_ARGUMENT(ETextJustify::Type, Justify)
			SLATE_ARGUMENT(float, LineHigh) // the step from one line to the next; 0 for the face's own
			SLATE_ARGUMENT(float, Guess) // the width it folds to before it is first drawn
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Text = Args._Text;
			Face = Args._Face;
			Ink = Args._Ink;
			Justify = Args._Justify;
			LineHigh = Args._LineHigh;
			Room = FMath::Max(static_cast<double>(Args._Guess), 24.0);
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScale) const override
		{
			TArray<FString> Lines;
			FoldPhrases(Text.Get().ToString(), Face, Room, Lines, LayoutScale);
			double Widest = 0.0;
			for (const FString& Line : Lines)
			{
				Widest = FMath::Max(Widest, PaintWide(Line, Face, LayoutScale));
			}
			return FVector2D(FMath::Min(Widest, Room), Lines.Num() * Step());
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			// (the width it is given is the width it folds to from now on)
			Room = FMath::Max(static_cast<double>(Geometry.GetLocalSize().X), 24.0);
			TArray<FString> Lines;
			FoldPhrases(Text.Get().ToString(), Face, Room, Lines, Geometry.Scale);
			const double High = Step();
			const FLinearColor Shown = Style.GetColorAndOpacityTint() * Ink;
			double Y = 0.0;
			for (const FString& Line : Lines)
			{
				const double Wide = PaintWide(Line, Face, Geometry.Scale);
				const double X = Justify == ETextJustify::Center ? 0.5 * (Room - Wide) : Justify == ETextJustify::Right ? Room - Wide : 0.0;
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide + 4.0, High), FSlateLayoutTransform(FVector2D(FMath::Max(X, 0.0), Y))), Line, Face, ESlateDrawEffect::None, Shown);
				Y += High;
			}
			return LayerId;
		}

	private:
		double Step() const
		{
			return LineHigh > 0.0f ? static_cast<double>(LineHigh) : static_cast<double>(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(Face));
		}

		TAttribute<FText> Text;
		FSlateFontInfo Face;
		FLinearColor Ink = FLinearColor::White;
		ETextJustify::Type Justify = ETextJustify::Left;
		float LineHigh = 0.0f;
		mutable double Room = 600.0;
	};

	/**
	 * (The UI director, check 19; the art director's review of 16:40 UTC, 17 November.) One side's corps on the battle
	 * card, in the roster's two forms. Where every corps of the side fits its column on one line, each stands as on the
	 * kit: "XIX Corps · armoured, 2 divisions · 30,000 men". Where any does not, every corps of that side takes two
	 * lines: the name alone in the lighter paper (folded before its bracket, else at a space, where it is wider than
	 * the column) and under it the figures in the darker, with no dot between the two, and 5 between one corps and the
	 * next. The right side is set to the right. Typed 12 on 17, the sheet's pixels. (The UI director's review of build
	 * 1628, item 2: every line wraps inside its half of the card, at any width; the figures too fold, at their dots,
	 * where they are wider than the column, and every width is measured at the scale the card is painted at.)
	 */
	class SRosterBlock : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRosterBlock) : _bOurs(true) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Player)
			SLATE_ARGUMENT(bool, bOurs)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Player = Args._Player;
			bOurs = Args._bOurs;
		}

		static constexpr double LineHigh = 17.0, Between = 5.0;

		virtual FVector2D ComputeDesiredSize(float LayoutScale) const override
		{
			TArray<FRow> Rows;
			const double High = Lay(Room, Rows, LayoutScale);
			double Widest = 0.0;
			for (const FRow& Row : Rows)
			{
				Widest = FMath::Max(Widest, PaintWide(Row.Words, Type(), LayoutScale));
			}
			return FVector2D(FMath::Min(Widest, Room), High);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			Room = FMath::Max(static_cast<double>(Geometry.GetLocalSize().X), 24.0);
			TArray<FRow> Rows;
			Lay(Room, Rows, Geometry.Scale);
			const FLinearColor Paper300(FColor::FromHex(TEXT("CDBF9C")));
			const FSlateFontInfo Face = Type();
			for (const FRow& Row : Rows)
			{
				const double Wide = PaintWide(Row.Words, Face, Geometry.Scale);
				const double X = bOurs ? 0.0 : FMath::Max(Room - Wide, 0.0);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide + 4.0, LineHigh), FSlateLayoutTransform(FVector2D(X, Row.Y))), Row.Words, Face,
					ESlateDrawEffect::None, Style.GetColorAndOpacityTint() * (Row.bFigures ? Paper300 : Cream));
			}
			return LayerId;
		}

	private:
		struct FRow
		{
			FString Words;
			bool bFigures = false; // the line of figures under a name, in the darker paper
			double Y = 0.0;
		};

		static FSlateFontInfo Type() { return Typed(9.0f); } // (typed 12: the sheet's pixels are three quarters of a point)

		/** The rows at a width and the scale they are painted at, top to bottom; returns the height they take. */
		double Lay(double Width, TArray<FRow>& Rows, double Scale) const
		{
			if (!Player.IsValid())
			{
				return 0.0;
			}
			const FFaceOff& Face = Player->GetFaceOff();
			const TArray<FRosterLine>& Roster = bOurs ? Face.OurRoster : Face.TheirRoster;
			const FSlateFontInfo With = Type();
			auto Whole = [](const FRosterLine& Corps) { return Corps.Figures.IsEmpty() ? Corps.Name : Corps.Name + TEXT(" \u00B7 ") + Corps.Figures; };
			// One form for the whole side: one line a corps only where every corps fits.
			bool bOneLine = true;
			for (const FRosterLine& Corps : Roster)
			{
				bOneLine = bOneLine && PaintWide(Whole(Corps), With, Scale) <= Width;
			}
			auto Add = [&Rows](const FString& Words, bool bFigures, double Y)
			{
				FRow& Row = Rows.AddDefaulted_GetRef();
				Row.Words = Words;
				Row.bFigures = bFigures;
				Row.Y = Y;
			};
			double Y = 0.0;
			for (int32 Index = 0; Index < Roster.Num(); ++Index)
			{
				const FRosterLine& Corps = Roster[Index];
				if (bOneLine)
				{
					Add(Whole(Corps), false, Y);
					Y += LineHigh;
					continue;
				}
				Y += Index > 0 ? Between : 0.0;
				TArray<FString> Name;
				FoldWords(Corps.Name, With, Width, Name, true, Scale);
				for (const FString& Line : Name)
				{
					Add(Line, false, Y);
					Y += LineHigh;
				}
				if (!Corps.Figures.IsEmpty())
				{
					// (the figures fold where they are wider than the column, at a dot, and the dot goes with the fold)
					TArray<FString> Figures;
					FoldPhrases(Corps.Figures, With, Width, Figures, Scale);
					for (const FString& Line : Figures)
					{
						Add(Line, true, Y);
						Y += LineHigh;
					}
				}
			}
			return Y;
		}

		TWeakObjectPtr<ACampaignPlayerController> Player;
		bool bOurs = true;
		mutable double Room = 370.0;
	};

	/**
	 * (Rules version 84; rulings 149(d), 164(f) and 174.) WHAT YOU FACE, on the battle card just above the odds: a
	 * sign and a sentence for each thing the defence will do to what he brings, four at most, facts and never a
	 * warning; and beside the heading, in italics, what is not scouted. Every sentence is the rules' own. With bPaid
	 * it is the result's line instead: who paid most of it and to what, under the price.
	 */
	class SFaceBlock : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFaceBlock) : _bPaid(false) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Player)
			SLATE_ARGUMENT(bool, bPaid)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Player = Args._Player;
			bPaid = Args._bPaid;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		static constexpr double BlockWide = 820.0, LineHigh = 22.0, SignSize = 17.0;

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			if (!Player.IsValid())
			{
				return FVector2D(BlockWide, 0.0);
			}
			const FFaceOff& Face = Player->GetFaceOff();
			if (bPaid)
			{
				return FVector2D(BlockWide, Face.bResolved && !Face.Paid.IsEmpty() ? LineHigh + 6.0 : 0.0);
			}
			int32 Lines = 0;
			for (const FFaceLine& Line : Face.Face)
			{
				Lines += Line.bUnknown ? 0 : 1;
			}
			return FVector2D(BlockWide, Face.bResolved || Face.Face.Num() == 0 ? 0.0 : 9.0 + 20.0 + Lines * LineHigh + 2.0);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (!Player.IsValid())
			{
				return LayerId;
			}
			const FFaceOff& Face = Player->GetFaceOff();
			static const FSlateColorBrush Solid(FLinearColor::White);
			const FSlateFontInfo Sentence = Serif(TEXT("Regular"), 11.5f), Slanted = Serif(TEXT("Italic"), 10.5f), Head = Typed(8.5f, true, 140);
			auto Wide = [&Geometry](const FString& Words, const FSlateFontInfo& With) { return PaintWide(Words, With, Geometry.Scale); };
			auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& With, const FLinearColor& Ink)
			{
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(900.0, 30.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, With, ESlateDrawEffect::None, Ink);
			};
			const double Width = Geometry.GetLocalSize().X;
			if (bPaid)
			{
				if (!Face.bResolved || Face.Paid.IsEmpty())
				{
					return LayerId;
				}
				// One line, set in the middle: its sign, where the rules give one, and the sentence.
				const bool bSign = !Face.PaidSign.IsEmpty();
				const double Whole = Wide(Face.Paid, Sentence) + (bSign ? SignSize + 8.0 : 0.0);
				double X = FMath::Max(0.5 * (Width - Whole), 0.0);
				if (bSign)
				{
					PaintWeaponSign(OutDrawElements, LayerId + 1, Geometry, Face.PaidSign, FVector2D(X, 6.0), SignSize, Brass);
					X += SignSize + 8.0;
				}
				Write(Face.Paid, X, 4.0, Sentence, Cream);
				return LayerId + 1;
			}
			if (Face.bResolved || Face.Face.Num() == 0)
			{
				return LayerId;
			}
			// A thin rule; the heading; and beside it, at the right, what is not scouted.
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(Width, 1.0), FSlateLayoutTransform(FVector2D(0.0, 0.0))), &Solid, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("7A6038"))).CopyWithNewOpacity(0.7f));
			Write(TEXT("WHAT YOU FACE"), 0.0, 11.0, Head, Brass);
			for (const FFaceLine& Line : Face.Face)
			{
				if (Line.bUnknown)
				{
					const double Left = Width - Wide(Line.Text, Slanted);
					PaintWeaponSign(OutDrawElements, LayerId + 1, Geometry, Line.Sign, FVector2D(Left - SignSize - 7.0, 9.0), SignSize - 2.0, Muted);
					Write(Line.Text, Left, 8.0, Slanted, Muted);
					break;
				}
			}
			double Y = 9.0 + 20.0;
			for (const FFaceLine& Line : Face.Face)
			{
				if (Line.bUnknown)
				{
					continue;
				}
				PaintWeaponSign(OutDrawElements, LayerId + 1, Geometry, Line.Sign, FVector2D(0.0, Y + 2.0), SignSize, Brass);
				Write(Line.Text, SignSize + 9.0, Y, Sentence, Cream);
				Y += LineHigh;
			}
			return LayerId + 1;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Player;
		bool bPaid = false;
	};

	/** The agent's picture for his card's well: the art director's studio picture of her own sculpt (Content/UI/agent-well.png). Read once; null if it is not there. */
	const FSlateBrush* AgentWellBrush()
	{
		static TSharedPtr<FSlateBrush> Kept;
		static bool bAsked = false;
		if (!bAsked)
		{
			bAsked = true;
			if (UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("agent-well.png")))
			{
				Texture->AddToRoot();
				Kept = MakeShared<FSlateBrush>();
				Kept->SetResourceObject(Texture);
				Kept->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Kept->DrawAs = ESlateBrushDrawType::Image;
			}
		}
		return Kept.Get();
	}

	/**
	 * (Rulings 153 and 169; the UI director's sheet 19.) The card of the agent in hand, in the army card's seat, 699 by
	 * 327. The three first answers lead: where he is, what he is doing, what it risks. Then what he sees, as figures in
	 * columns, and the work he can be set there with its odds: the lamp marks the work in force, and a row is pressed
	 * to change it. Work that is refused stays on the list under a lock, with the rules' one sentence saying why. While
	 * he cannot be given orders the card says so once, in the rules' words. Every sentence on it is the rules' own.
	 */
	class SAgentCard : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAgentCard) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Player)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Player = Args._Player;
		}

		static constexpr double CardW = 699.0, CardH = 327.0;

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(CardW, CardH); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FViewAgent* Agent = Player.IsValid() ? Player->GetAgentInHand() : nullptr;
			if (!Agent)
			{
				return LayerId;
			}
			static const FSlateColorBrush Solid(FLinearColor::White);
			// (The sheet's sizes are in pixels of the 1600 by 1000 frame; a face here is asked for in points, which are a third larger.)
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			const FLinearColor Desk900(FColor::FromHex(TEXT("1D1610"))), Desk800(FColor::FromHex(TEXT("2A2118"))), Desk700(FColor::FromHex(TEXT("3A2E22"))), Brass700(FColor::FromHex(TEXT("7A6038")));
			const FLinearColor Amber(FColor::FromHex(TEXT("E0A13C"))), Dim(FColor::FromHex(TEXT("8F8573")));
			auto Fill = [&](double X, double Y, double W, double H, const FLinearColor& Ink, int32 Layer)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(W, H), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Ink);
			};
			auto Picture = [&](const FSlateBrush* Brush, double X, double Y, double W, double H, int32 Layer)
			{
				if (Brush)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(W, H), FSlateLayoutTransform(FVector2D(X, Y))), Brush, ESlateDrawEffect::None, FLinearColor::White);
				}
			};
			auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
			{
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(700.0, 40.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, Face, ESlateDrawEffect::None, Ink);
			};
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto Wide = [&Measure](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face).X); };
			auto WriteRight = [&](const FString& Words, double Right, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, int32 Layer)
			{
				Write(Words, Right - Wide(Words, Face), Y, Face, Ink, Layer);
			};
			// A sentence set in lines no wider than its room, broken at whole words: where the next line would begin.
			auto Set = [&](const FString& Words, double X, double Y, const FSlateFontInfo& Face, const FLinearColor& Ink, double Room, double Lead, int32 Layer)
			{
				TArray<FString> Each;
				Words.ParseIntoArray(Each, TEXT(" "));
				FString Line;
				for (const FString& Word : Each)
				{
					const FString Longer = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
					if (!Line.IsEmpty() && Wide(Longer, Face) > Room)
					{
						Write(Line, X, Y, Face, Ink, Layer);
						Y += Lead;
						Line = Word;
					}
					else
					{
						Line = Longer;
					}
				}
				if (!Line.IsEmpty())
				{
					Write(Line, X, Y, Face, Ink, Layer);
					Y += Lead;
				}
				return Y;
			};
			auto Stroke = [&](const TArray<FVector2D>& Points, const FLinearColor& Ink, float Thick, int32 Layer)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, Thick);
			};
			auto Edge = [&](double X, double Y, double W, double H, const FLinearColor& Ink, float Thick, int32 Layer)
			{
				Stroke({ FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H), FVector2D(X, Y) }, Ink, Thick, Layer);
			};
			auto Round = [&](double X, double Y, double R, const FLinearColor& Ink, float Thick, int32 Layer)
			{
				TArray<FVector2D> Points;
				for (int32 K = 0; K <= 20; ++K)
				{
					const double Angle = K * UE_DOUBLE_TWO_PI / 20.0;
					Points.Add(FVector2D(X + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R));
				}
				Stroke(Points, Ink, Thick, Layer);
			};
			auto Disc = [&](double X, double Y, double R, const FLinearColor& Ink, int32 Layer)
			{
				Round(X, Y, R * 0.5, Ink, static_cast<float>(R), Layer);
			};
			// The small signs of the fact strip and the list: a pin, the agent's hat, a warning, a tick, an hourglass, a padlock.
			auto Sign = [&](int32 What, double X, double Y, const FLinearColor& Ink, int32 Layer)
			{
				auto P = [X, Y](double DX, double DY) { return FVector2D(X + DX, Y + DY); };
				const float Pen = 1.4f;
				if (What == 0)
				{
					Round(X, Y - 1.6, 3.6, Ink, Pen, Layer);
					Stroke({ P(-3.0, 0.6), P(0.0, 6.0), P(3.0, 0.6) }, Ink, Pen, Layer);
					Disc(X, Y - 1.6, 1.2, Ink, Layer);
				}
				else if (What == 1)
				{
					Stroke({ P(-2.6, -1.6), P(-1.9, -5.4), P(1.9, -5.4), P(2.6, -1.6) }, Ink, Pen, Layer);
					Stroke({ P(-5.6, -1.4), P(5.6, -1.4) }, Ink, Pen, Layer);
					Stroke({ P(-4.6, 5.6), P(-4.0, 2.6), P(-2.0, 1.4), P(2.0, 1.4), P(4.0, 2.6), P(4.6, 5.6) }, Ink, Pen, Layer);
				}
				else if (What == 2)
				{
					Stroke({ P(0.0, -5.6), P(6.0, 5.0), P(-6.0, 5.0), P(0.0, -5.6) }, Ink, Pen, Layer);
					Stroke({ P(0.0, -1.6), P(0.0, 1.8) }, Ink, Pen, Layer);
					Disc(X, Y + 3.4, 0.8, Ink, Layer);
				}
				else if (What == 3)
				{
					Stroke({ P(-5.0, 0.2), P(-1.6, 3.8), P(5.2, -4.2) }, Ink, Pen * 1.2f, Layer);
				}
				else if (What == 4)
				{
					Stroke({ P(-3.8, -5.0), P(3.8, -5.0), P(-3.8, 5.0), P(3.8, 5.0), P(-3.8, -5.0) }, Ink, Pen, Layer);
				}
				else
				{
					Stroke({ P(-3.6, -0.4), P(3.6, -0.4), P(3.6, 4.6), P(-3.6, 4.6), P(-3.6, -0.4) }, Ink, Pen, Layer);
					Stroke({ P(-2.4, -0.4), P(-2.4, -2.8), P(-1.2, -4.4), P(1.2, -4.4), P(2.4, -2.8), P(2.4, -0.4) }, Ink, Pen, Layer);
				}
			};
			const FSlateFontInfo Head10 = Typed(Px(11.0f), true, 80), Small10 = Typed(Px(11.0f)), Typed12 = Typed(Px(12.0f)), Bold12 = Typed(Px(12.0f), true), Bold13 = Typed(Px(13.0f), true);
			const FSlateFontInfo Name25 = Serif(TEXT("Regular"), Px(25.0f)), Row14 = Serif(TEXT("Bold"), Px(14.0f)), Body13 = Serif(TEXT("Regular"), Px(13.0f)), Body15 = Serif(TEXT("Regular"), Px(15.0f)), Hint14 = Serif(TEXT("Italic"), Px(14.0f));

			// ---- The panel: solid, edged, with its four clamps.
			Fill(0.0, 0.0, CardW, CardH, Desk900, 0);
			Edge(0.5, 0.5, CardW - 1.0, CardH - 1.0, Brass700, 1.0f, 1);
			for (const FVector2D& Corner : { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(1.0, 1.0) })
			{
				const double CX = 5.0 + Corner.X * (CardW - 10.0), CY = 5.0 + Corner.Y * (CardH - 10.0);
				const double DX = Corner.X > 0.5 ? -13.0 : 13.0, DY = Corner.Y > 0.5 ? -13.0 : 13.0;
				Stroke({ FVector2D(CX + DX, CY), FVector2D(CX, CY), FVector2D(CX, CY + DY) }, Brass, 2.0f, 2);
			}

			// ---- The name row: the flag of the country he serves, his name, his rank on a plate with its pips, and the door to the room.
			const FString Mine = Player->GetSession() ? Player->GetSession()->GetView().Human : FString();
			Picture(FlagOf(Mine), 18.0, 14.0, 40.0, 27.0, 2);
			const FString Title = TEXT("Agent ") + Agent->Name;
			Write(Title, 70.0, 9.0, Name25, Cream, 2);
			double Along = 70.0 + Wide(Title, Name25) + 14.0;
			{
				const FString Rank = Agent->Rank.ToUpper();
				const double Plate = Wide(Rank, Head10) + 14.0;
				Edge(Along + 0.5, 19.5, Plate, 17.0, Brass, 1.0f, 2);
				Write(Rank, Along + 7.0, 22.0, Head10, BrightBrass, 2);
				Along += Plate + 12.0;
				for (int32 Pip = 0; Pip < FMath::Clamp(Agent->Ranks, 1, 5); ++Pip)
				{
					if (Pip < Agent->Skill)
					{
						Disc(Along + 4.0 + 11.0 * Pip, 28.0, 3.8, BrightBrass, 2);
					}
					else
					{
						Round(Along + 4.0 + 11.0 * Pip, 28.0, 3.4, Dim, 1.2f, 2);
					}
				}
			}
			{
				const FBox2D Door = DoorBox();
				Fill(Door.Min.X, Door.Min.Y, Door.GetSize().X, Door.GetSize().Y, Desk800, 2);
				Edge(Door.Min.X + 0.5, Door.Min.Y + 0.5, Door.GetSize().X - 1.0, Door.GetSize().Y - 1.0, Over == -2 ? BrightBrass : Brass700, 1.0f, 3);
				const FString Says = TEXT("OPEN THE SECRET SERVICE");
				Write(Says, Door.GetCenter().X - Wide(Says, Head10) * 0.5, Door.Min.Y + 8.0, Head10, Over == -2 ? BrightBrass : Cream, 3);
			}

			// ---- The fact strip: where he is, what he is doing, what it risks.
			Edge(18.5, 50.5, 662.0, 29.0, Brass700, 1.0f, 2);
			{
				double X = 34.0;
				Sign(0, X, 65.0, Brass, 2);
				X += 14.0;
				Write(Agent->Where, X, 58.0, Bold12, Cream, 2);
				X += Wide(Agent->Where, Bold12);
				const FString Whose = TEXT("  ·  ") + Agent->HostName;
				Write(Whose, X, 58.0, Typed12, Muted, 2);
				X += Wide(Whose, Typed12) + 14.0;
				Stroke({ FVector2D(X, 51.0), FVector2D(X, 79.0) }, Brass700, 1.0f, 2);
				X += 16.0;
				// (What he is doing, in the rules' own sentence: its first words lead, the rest follows quietly.)
				FString Lead = Agent->Doing, Rest;
				if (!Agent->Doing.Split(TEXT(": "), &Lead, &Rest))
				{
					if (Agent->Doing.Split(TEXT(" at "), &Lead, &Rest))
					{
						Rest = TEXT("at ") + Rest;
					}
				}
				Sign(Agent->bAtWork ? 1 : 4, X, 65.0, Brass, 2);
				X += 14.0;
				Write(Lead, X, 58.0, Bold12, Cream, 2);
				X += Wide(Lead, Bold12);
				if (!Rest.IsEmpty())
				{
					const FString After = TEXT("  ·  ") + Rest;
					Write(After, X, 58.0, Typed12, Muted, 2);
					X += Wide(After, Typed12);
				}
				if (Agent->bAtWork)
				{
					// (Ruling 169: a chance of 10% or more that he is caught is amber wherever it stands on the card.)
					X += 14.0;
					Stroke({ FVector2D(X, 51.0), FVector2D(X, 79.0) }, Brass700, 1.0f, 2);
					X += 16.0;
					if (Agent->bAbroad)
					{
						const FLinearColor Risk = Agent->Caught >= 10 ? Amber : Cream;
						Sign(2, X, 65.0, Agent->Caught >= 10 ? Amber : Brass, 2);
						X += 14.0;
						const FString Chance = FString::Printf(TEXT("%d%%"), Agent->Caught);
						Write(Chance, X, 58.0, Bold12, Risk, 2);
						Write(TEXT(" caught a month"), X + Wide(Chance, Bold12), 58.0, Typed12, Muted, 2);
					}
					else
					{
						Sign(3, X, 65.0, Brass, 2);
						X += 14.0;
						Write(TEXT("No risk"), X, 58.0, Bold12, Cream, 2);
						Write(TEXT(" at home"), X + Wide(TEXT("No risk"), Bold12), 58.0, Typed12, Muted, 2);
					}
				}
			}

			// ---- His picture, in its well; under it, his experience toward the next rank.
			Fill(18.0, 88.0, 100.0, 124.0, Desk800, 2);
			Picture(AgentWellBrush(), 18.0, 88.0, 100.0, 124.0, 3);
			Edge(18.5, 88.5, 99.0, 123.0, Brass700, 1.0f, 4);
			if (!Agent->Toward.IsEmpty())
			{
				Write(FString::Printf(TEXT("%d of %d toward"), Agent->Xp, Agent->Next), 18.0, 216.0, Small10, Muted, 2);
				Write(Agent->Toward, 18.0, 229.0, Small10, Muted, 2);
			}
			else
			{
				Write(Agent->Rank, 18.0, 216.0, Small10, Muted, 2);
			}

			if (!Agent->bAtWork)
			{
				// ---- On the road, or in training: one sentence, and what he will do when he is at work.
				double Y = Set(Agent->Doing + TEXT("."), 132.0, 90.0, Body15, Cream, 540.0, 22.0, 2);
				Set(Agent->Will, 132.0, Y + 6.0, Body13, Muted, 540.0, 18.0, 2);
			}
			else
			{
				// ---- What he sees (abroad), or the threat he guards against (at home).
				if (Agent->bAbroad)
				{
					Write(TEXT("IN SIGHT"), 132.0, 90.0, Head10, Brass, 2);
					WriteRight(TEXT("CORPS"), 286.0, 90.0, Small10, Muted, 2);
					WriteRight(TEXT("DIVISIONS"), 358.0, 90.0, Small10, Muted, 2);
					const int32 Shown = Agent->Sees.Num() > 5 ? 4 : Agent->Sees.Num();
					for (int32 Row = 0; Row < Shown; ++Row)
					{
						const FAgentSight& Seen = Agent->Sees[Row];
						const double Y = 106.0 + 26.0 * Row;
						Picture(FlagOf(Seen.Nation), 134.0, Y + 8.0, 15.0, 10.0, 2);
						Write(Seen.Name, 156.0, Y + 4.0, Row14, Cream, 2);
						WriteRight(FString::FromInt(Seen.Corps), 286.0, Y + 6.0, Bold13, Cream, 2);
						WriteRight(FString::FromInt(Seen.Divisions), 358.0, Y + 6.0, Bold13, Cream, 2);
						Fill(132.0, Y + 25.0, 226.0, 1.0, Desk700, 2);
					}
					if (Agent->Sees.Num() > Shown)
					{
						Write(FString::Printf(TEXT("and %d more armies"), Agent->Sees.Num() - Shown), 156.0, 106.0 + 26.0 * Shown + 5.0, Body13, Muted, 2);
					}
					if (Agent->Sees.Num() == 0)
					{
						Write(TEXT("No army is in sight."), 132.0, 110.0, Serif(TEXT("Italic"), Px(13.0f)), Muted, 2);
					}
				}
				else
				{
					Write(TEXT("SABOTEURS AT HOME"), 132.0, 90.0, Head10, Brass, 2);
					Set(Agent->Home, 132.0, 108.0, Body13, Muted, 226.0, 17.0, 2);
				}

				// ---- The work he can be set where he stands, with its odds. The lamp marks the work in force.
				Write(TEXT("WORK AT ") + Agent->Where.ToUpper(), 372.0, 90.0, Head10, Brass, 2);
				if (Agent->bAbroad)
				{
					WriteRight(TEXT("COMES OFF"), 612.0, 90.0, Small10, Muted, 2);
				}
				WriteRight(TEXT("CAUGHT"), 681.0, 90.0, Small10, Muted, 2);
				const FString Cause = CommonCause(*Agent);
				for (int32 Row = 0; Row < FMath::Min(Agent->Work.Num(), 6); ++Row)
				{
					const FAgentWork& Work = Agent->Work[Row];
					const FBox2D Box = RowBox(Row);
					const double Y = Box.Min.Y, Middle = Y + 13.0;
					const bool bLocked = !Work.bNow && !Work.Why.IsEmpty();
					const bool bPointed = Over == Row && Work.bOk;
					if (bPointed)
					{
						Fill(Box.Min.X, Y, Box.GetSize().X, 25.0, Desk800, 1);
					}
					if (Work.bNow)
					{
						Round(382.0, Middle, 6.0, Brass, 1.2f, 2);
						Disc(382.0, Middle, 3.6, BrightBrass, 3);
					}
					else if (bLocked)
					{
						Sign(5, 382.0, Middle, Dim, 2);
					}
					else
					{
						Round(382.0, Middle, 4.0, bPointed ? BrightBrass : Muted, 1.3f, 2);
					}
					Write(Work.Label, 396.0, Y + 4.0, Row14, Work.bNow ? BrightBrass : bLocked ? Dim : bPointed ? BrightBrass : Cream, 2);
					if (bLocked && Work.Why != Cause)
					{
						// (a row with a cause of its own carries it in the figures' place)
						Set(Work.Why, 396.0 + Wide(Work.Label, Row14) + 12.0, Y + 7.0, Small10, Muted, 285.0 - Wide(Work.Label, Row14) - 12.0, 12.0, 2);
					}
					else if (!bLocked)
					{
						if (Work.Success >= 0)
						{
							WriteRight(FString::Printf(TEXT("%d%%"), Work.Success), 612.0, Y + 6.0, Bold13, Cream, 2);
						}
						else if (Agent->bAbroad)
						{
							WriteRight(TEXT("·"), 606.0, Y + 6.0, Bold13, Muted, 2);
						}
						if (!Agent->bAbroad)
						{
							WriteRight(TEXT("no risk"), 681.0, Y + 7.0, Typed12, Muted, 2);
						}
						else if (Work.Success >= 0)
						{
							WriteRight(FString::Printf(TEXT("%d%%"), Work.Caught), 681.0, Y + 6.0, Bold13, Work.Caught >= 10 ? Amber : Cream, 2);
						}
						else
						{
							// (work that goes on month after month: the chance each month)
							const FString Each = TEXT(" a month");
							WriteRight(Each, 681.0, Y + 8.0, Small10, Muted, 2);
							WriteRight(FString::Printf(TEXT("%d%%"), Work.Caught), 681.0 - Wide(Each, Small10), Y + 6.0, Bold13, Work.Caught >= 10 ? Amber : Cream, 2);
						}
					}
					Fill(372.0, Y + 25.0, 309.0, 1.0, Desk700, 2);
				}
				{
					// One cause, one sentence under the list; and at home, what standing guard is.
					const double Under = 106.0 + 26.0 * FMath::Min(Agent->Work.Num(), 6) + 8.0;
					if (!Cause.IsEmpty())
					{
						Set(Cause, 372.0, Under, Body13, Cream, 309.0, 17.0, 2);
					}
					else if (!Agent->bAbroad)
					{
						Set(Agent->Will, 372.0, Under, Body13, Muted, 309.0, 17.0, 2);
					}
				}
			}

			// ---- The foot: how he is sent; or, while he cannot be, the rules' own reason.
			Write(Player->GetAgentHint(), 18.0, 298.0, Hint14, Muted, 2);
			return LayerId + 6;
		}

		virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			const int32 Before = Over;
			Over = At(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			if (Over != Before)
			{
				// (the long sentence for each piece of work stays off the card: it comes up on the pointer)
				const FViewAgent* Agent = Player.IsValid() ? Player->GetAgentInHand() : nullptr;
				FString Says;
				if (Agent && Agent->Work.IsValidIndex(Over))
				{
					const FAgentWork& Work = Agent->Work[Over];
					Says = Work.Text;
					if (!Work.Why.IsEmpty() && !Work.bNow)
					{
						Says += TEXT(" ") + Work.Why;
					}
				}
				else if (Over == -2)
				{
					Says = TEXT("Your agents, and recruiting more.");
				}
				SetToolTipText(FText::FromString(Says));
			}
			return FReply::Handled();
		}

		virtual void OnMouseLeave(const FPointerEvent& Event) override
		{
			Over = -1;
			SetToolTipText(FText::GetEmpty());
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (!Player.IsValid() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Handled();
			}
			const int32 Pressed = At(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			const FViewAgent* Agent = Player->GetAgentInHand();
			if (Pressed == -2)
			{
				Player->OpenPage(TEXT("spies"));
			}
			else if (Agent && Agent->Work.IsValidIndex(Pressed) && Agent->Work[Pressed].bOk)
			{
				const FString Act = Agent->Work[Pressed].Act; // (kept: the order lays the view out afresh)
				Player->SetAgentWork(Act);
			}
			return FReply::Handled();
		}

	private:
		static FBox2D DoorBox() { return FBox2D(FVector2D(508.0, 14.0), FVector2D(681.0, 42.0)); }
		static FBox2D RowBox(int32 Row) { return FBox2D(FVector2D(372.0, 106.0 + 26.0 * Row), FVector2D(681.0, 132.0 + 26.0 * Row)); }

		/** What the pointer is on: a row of work by its place, -2 the door to the room, -1 nothing. */
		int32 At(const FVector2D& Here) const
		{
			if (DoorBox().IsInside(Here))
			{
				return -2;
			}
			const FViewAgent* Agent = Player.IsValid() ? Player->GetAgentInHand() : nullptr;
			if (Agent && Agent->bAtWork)
			{
				for (int32 Row = 0; Row < FMath::Min(Agent->Work.Num(), 6); ++Row)
				{
					if (RowBox(Row).IsInside(Here))
					{
						return Row;
					}
				}
			}
			return -1;
		}

		/** The reason most of the refused work shares, in the rules' sentence: it is said once, under the list. */
		static FString CommonCause(const FViewAgent& Agent)
		{
			FString Most;
			int32 MostCount = 0;
			for (const FAgentWork& Work : Agent.Work)
			{
				if (Work.bNow || Work.Why.IsEmpty())
				{
					continue;
				}
				int32 Count = 0;
				for (const FAgentWork& Other : Agent.Work)
				{
					Count += !Other.bNow && Other.Why == Work.Why ? 1 : 0;
				}
				if (Count > MostCount)
				{
					MostCount = Count;
					Most = Work.Why;
				}
			}
			return Most;
		}

		const FSlateBrush* FlagOf(const FString& Tag) const
		{
			if (Tag.IsEmpty())
			{
				return nullptr;
			}
			if (const TSharedPtr<FSlateBrush>* Found = Flags.Find(Tag))
			{
				return Found->Get();
			}
			ACampaignMapActor* Table = Player.IsValid() ? Player->GetMap() : nullptr;
			UTexture2D* Cloth = Table ? Table->GetFlagTexture(Tag) : nullptr;
			if (!Cloth)
			{
				return nullptr; // (not ready yet: asked again at the next look)
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Cloth);
			Brush->ImageSize = FVector2D(96.0f, 64.0f);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			Flags.Add(Tag, Brush);
			return Brush.Get();
		}

		TWeakObjectPtr<ACampaignPlayerController> Player;
		mutable TMap<FString, TSharedPtr<FSlateBrush>> Flags;
		int32 Over = -1;
	};

	/**
	 * The art director's sign for a kind of unit on order (Content/CityOrders/order-<key>.png): white on a clear
	 * ground, to be tinted. Read once for the life of the game; null for a key she has not drawn.
	 */
	const FSlateBrush* OrderSignBrush(const FString& Key)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Kept;
		if (const TSharedPtr<FSlateBrush>* Found = Kept.Find(Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateBrush> Brush;
		if (UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("CityOrders") / (TEXT("order-") + Key + TEXT(".png"))))
		{
			Texture->AddToRoot();
			Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Texture);
			Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Brush->DrawAs = ESlateBrushDrawType::Image;
		}
		Kept.Add(Key, Brush);
		return Brush.Get();
	}

	/** Where the towns' name plates and their badges were last drawn, in the screen's own units: what a sky's plate steps clear of. */
	TArray<FBox2D> DrawnTownPlates;

	/**
	 * (Ruling 127; the art director's weather-sky brief, section 4.) The sky's plates: in the sky view, one for each
	 * weather zone in sight. A disc with the sky's sign, the zone's name in small brass capitals, then the word and
	 * what it means: "Clear sky everything flies", "Cloud flying is harder", "Closed sky nothing flies". No red and
	 * no green: the weather takes no side. A plate stands among the parts of its zone that are in sight, keeps to
	 * the open map, and steps down clear of a flag or of another plate.
	 */
	class SSkyPlates : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSkyPlates) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
			// (a filled round of each radius up to sixteen points: a disc is a box with its corners rounded right off)
			for (int32 Radius = 1; Radius <= 16; ++Radius)
			{
				Rounds.Add(FSlateRoundedBoxBrush(FLinearColor::White, static_cast<float>(Radius)));
			}
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.0, 16.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			THEATER_SPEED_SCOPE(PaintSkyPlates);
			const ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			const float Shown = Table ? Table->GetSkyView() : 0.0f;
			if (Shown <= 0.01f || Table->GetSkyPlates().Num() == 0)
			{
				return LayerId;
			}
			static const FSlateColorBrush Solid(FLinearColor::White);
			const float Scale = FMath::Max(Geometry.Scale, 0.01f);
			const FVector2D Size = Geometry.GetLocalSize();
			// (The brief's sizes are pixels of the 1600 by 1000 frame. This screen is 900 of its own units high.)
			const double U = Size.Y / 1000.0;
			auto Face = [U](float Pixels) { return static_cast<float>(Pixels * 0.75 * U); };
			const FSlateFontInfo NameFace = Typed(Face(11.0f), true, 100), WordFace = Serif(TEXT("Bold"), Face(15.0f)), MeansFace = Typed(Face(12.0f));
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto Wide = [&Measure](const FString& Words, const FSlateFontInfo& With) { return static_cast<double>(Measure->Measure(Words, With).X); };
			const FLinearColor Desk(FColor::FromHex(TEXT("1D1610"))), Brass700(FColor::FromHex(TEXT("7A6038"))), Pale(FColor::FromHex(TEXT("EDE6D6"))), Ink(FColor::FromHex(TEXT("1F1808")));
			auto Faded = [Shown](FLinearColor Colour, float Strength = 1.0f) { Colour.A *= Shown * Strength; return Colour; };
			auto Fill = [&](double X, double Y, double W, double H, const FLinearColor& Colour, int32 Layer)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(W, H), FSlateLayoutTransform(FVector2D(X, Y))), &Solid, ESlateDrawEffect::None, Colour);
			};
			auto Stroke = [&](const TArray<FVector2D>& Points, const FLinearColor& Colour, float Thick, int32 Layer)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thick);
			};
			auto Bend = [](double X, double Y, double R, double From, double To)
			{
				TArray<FVector2D> Points;
				const int32 Steps = FMath::Max(3, FMath::CeilToInt32(FMath::Abs(To - From) / 20.0));
				for (int32 K = 0; K <= Steps; ++K)
				{
					const double Angle = FMath::DegreesToRadians(FMath::Lerp(From, To, static_cast<double>(K) / Steps));
					Points.Add(FVector2D(X + FMath::Cos(Angle) * R, Y - FMath::Sin(Angle) * R));
				}
				return Points;
			};
			auto Write = [&](const FString& Words, double X, double Y, const FSlateFontInfo& With, const FLinearColor& Colour, int32 Layer)
			{
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(600.0, 40.0), FSlateLayoutTransform(FVector2D(X, Y))), Words, With, ESlateDrawEffect::None, Colour);
			};
			// A filled round.
			auto Dot = [&](double X, double Y, double R, const FLinearColor& Colour, int32 Layer)
			{
				const int32 Which = FMath::Clamp(FMath::FloorToInt32(R), 1, Rounds.Num()) - 1;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + Layer, Geometry.ToPaintGeometry(FVector2D(2.0 * R, 2.0 * R), FSlateLayoutTransform(FVector2D(X - R, Y - R))), &Rounds[Which], ESlateDrawEffect::None, Colour);
			};
			// What a plate must stand clear of: the armies' colours, the towns' name plates, and the plates laid before it.
			TArray<FBox2D> Taken;
			Controller->GetColoursOnScreen(Taken);
			for (FBox2D& Flag : Taken)
			{
				Flag.Min /= Scale;
				Flag.Max /= Scale;
			}
			// (the plates the towns' own layer has just drawn, as it drew them: not every town it knows of has one)
			Taken.Append(DrawnTownPlates);
			for (const ACampaignMapActor::FSkyPlate& Plate : Table->GetSkyPlates())
			{
				// Among the parts of its zone that are in sight.
				FVector2D Sum = FVector2D::ZeroVector;
				int32 Seen = 0;
				for (const FVector& Ground : Plate.Grounds)
				{
					FVector2D Screen;
					if (Controller->ProjectWorldLocationToScreen(Ground, Screen, false))
					{
						Screen /= Scale;
						if (Screen.X > 0.0 && Screen.Y > 0.0 && Screen.X < Size.X && Screen.Y < Size.Y)
						{
							Sum += Screen;
							++Seen;
						}
					}
				}
				if (Seen == 0)
				{
					continue;
				}
				const FString Name = Plate.Name.ToUpper();
				const FString Word = Plate.Sky == 2 ? TEXT("Closed sky") : Plate.Sky == 1 ? TEXT("Cloud") : TEXT("Clear sky");
				const FString Means = Plate.Sky == 2 ? TEXT("nothing flies") : Plate.Sky == 1 ? TEXT("flying is harder") : TEXT("everything flies");
				const double High = 44.0 * U, Disc = 15.0 * U;
				const double TextLeft = 12.0 * U + 2.0 * Disc + 12.0 * U;
				const double PlateWide = TextLeft + FMath::Max(Wide(Name, NameFace), Wide(Word, WordFace) + 9.0 * U + Wide(Means, MeansFace)) + 16.0 * U;
				// Kept to the open map: clear of the calendar, the bar along the top and the column down the right.
				FVector2D Corner = Sum / Seen - FVector2D(0.5 * PlateWide, 0.5 * High);
				Corner.X = FMath::Clamp(Corner.X, 20.0 * U, FMath::Max(Size.X - 290.0 * U - PlateWide, 20.0 * U));
				Corner.Y = FMath::Clamp(Corner.Y, 270.0 * U, FMath::Max(Size.Y - 70.0 * U - High, 270.0 * U));
				// (It steps aside to the nearest clear place, trying round about itself: it went straight down before, and
				// from high up, where the flags stand thick, every plate ended at the foot of the screen far from its zone.)
				{
					const FVector2D Wanted = Corner;
					double Least = 1.0e18;
					for (int32 DY = -6; DY <= 6; ++DY)
					{
						for (int32 DX = -4; DX <= 4; ++DX)
						{
							const FVector2D Try(FMath::Clamp(Wanted.X + DX * 0.5 * PlateWide, 20.0 * U, FMath::Max(Size.X - 290.0 * U - PlateWide, 20.0 * U)),
								FMath::Clamp(Wanted.Y + DY * 30.0 * U, 270.0 * U, FMath::Max(Size.Y - 70.0 * U - High, 270.0 * U)));
							const FBox2D Box(Try - FVector2D(4.0, 4.0), Try + FVector2D(PlateWide + 4.0, High + 4.0));
							int32 Hits = 0;
							for (const FBox2D& Other : Taken)
							{
								Hits += Box.Intersect(Other) ? 1 : 0;
							}
							// (clear before near; and where nowhere is clear, the place that covers least)
							const double Cost = Hits * 4000.0 + FVector2D::Distance(Try, Wanted);
							if (Cost < Least)
							{
								Least = Cost;
								Corner = Try;
							}
						}
					}
				}
				Taken.Add(FBox2D(Corner, Corner + FVector2D(PlateWide, High)));
				Fill(Corner.X, Corner.Y, PlateWide, High, Faded(Desk, 0.94f), 0);
				Stroke({ Corner + FVector2D(0.5, 0.5), Corner + FVector2D(PlateWide - 0.5, 0.5), Corner + FVector2D(PlateWide - 0.5, High - 0.5), Corner + FVector2D(0.5, High - 0.5), Corner + FVector2D(0.5, 0.5) }, Faded(Brass700), 1.0f, 1);
				// The sign in its disc. Clear: a brass sun in a brass ring. Cloud: a pale sun behind a cloud in a pale
				// ring. Closed: a dark rain cloud on a filled pale disc, the loudest of the three.
				const double CX = Corner.X + 12.0 * U + Disc, CY = Corner.Y + 0.5 * High;
				const FLinearColor Sign = Faded(Plate.Sky == 2 ? Ink : Plate.Sky == 1 ? Pale : BrightBrass);
				const float Pen = static_cast<float>(1.5 * U);
				auto P = [CX, CY, U](double DX, double DY) { return FVector2D(CX + DX * U, CY + DY * U); };
				if (Plate.Sky == 2)
				{
					Dot(CX, CY, Disc, Faded(Pale), 1);
				}
				else
				{
					Stroke(Bend(CX, CY, Disc - 0.75 * U, 0.0, 360.0), Sign, Pen, 1);
				}
				if (Plate.Sky == 0)
				{
					Stroke(Bend(CX, CY, 3.6 * U, 0.0, 360.0), Sign, Pen, 2);
					for (int32 Ray = 0; Ray < 8; ++Ray)
					{
						const double Angle = Ray * UE_DOUBLE_PI / 4.0;
						Stroke({ P(FMath::Cos(Angle) * 5.8, FMath::Sin(Angle) * 5.8), P(FMath::Cos(Angle) * 8.2, FMath::Sin(Angle) * 8.2) }, Sign, Pen, 2);
					}
				}
				else
				{
					// The cloud: three humps over a flat foot. Filled for a closed sky, an outline for cloud.
					auto Cloud = [&](double OX, double OY, bool bSolid)
					{
						if (bSolid)
						{
							Dot(CX + (OX - 3.8) * U, CY + (OY + 1.6) * U, 3.4 * U, Sign, 2);
							Dot(CX + (OX + 0.4) * U, CY + (OY - 0.8) * U, 4.6 * U, Sign, 2);
							Dot(CX + (OX + 4.4) * U, CY + (OY + 1.9) * U, 3.1 * U, Sign, 2);
							Fill(CX + (OX - 3.8) * U, CY + (OY + 1.4) * U, 8.2 * U, 3.6 * U, Sign, 2);
						}
						else
						{
							TArray<FVector2D> Outline = Bend(CX + (OX - 3.6) * U, CY + (OY + 1.6) * U, 3.2 * U, 270.0, 110.0);
							Outline.Append(Bend(CX + (OX + 0.4) * U, CY + (OY - 0.8) * U, 4.2 * U, 160.0, 20.0));
							Outline.Append(Bend(CX + (OX + 4.4) * U, CY + (OY + 2.0) * U, 2.8 * U, 80.0, -90.0));
							Outline.Add(P(OX - 3.6, OY + 4.8));
							Stroke(Outline, Sign, Pen, 2);
						}
					};
					if (Plate.Sky == 1)
					{
						// (the sun behind it, up and to the right, with three of its rays)
						Stroke(Bend(CX + 4.2 * U, CY - 3.6 * U, 2.6 * U, -20.0, 200.0), Sign, Pen, 2);
						for (const double Angle : { 20.0, 65.0, 110.0 })
						{
							const double Rad = FMath::DegreesToRadians(Angle);
							Stroke({ P(4.2 + FMath::Cos(Rad) * 4.2, -3.6 - FMath::Sin(Rad) * 4.2), P(4.2 + FMath::Cos(Rad) * 6.0, -3.6 - FMath::Sin(Rad) * 6.0) }, Sign, Pen, 2);
						}
						Cloud(-1.0, 1.4, false);
					}
					else
					{
						Cloud(-0.3, -2.4, true);
						for (const double Drop : { -3.4, 0.0, 3.4 })
						{
							Stroke({ P(Drop + 0.9, 3.4), P(Drop - 0.5, 7.4) }, Sign, static_cast<float>(1.7 * U), 2);
						}
					}
				}
				Write(Name, Corner.X + TextLeft, Corner.Y + 6.0 * U, NameFace, Faded(BrightBrass), 2);
				Write(Word, Corner.X + TextLeft, Corner.Y + 20.0 * U, WordFace, Faded(Cream), 2);
				Write(Means, Corner.X + TextLeft + Wide(Word, WordFace) + 9.0 * U, Corner.Y + 23.5 * U, MeansFace, Faded(Muted), 2);
			}
			return LayerId + 3;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
		TArray<FSlateRoundedBoxBrush> Rounds;
	};

	/**
	 * The name plates of the towns: each hangs before its town and faces the player, so a name can be read from
	 * any side of the table and is never hidden by the models. A stripe in the holder's colour says whose it is.
	 * Where plates would crowd one another the place that matters more keeps its plate.
	 */
	class STownPlates : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STownPlates) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
			Plate = FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.031f, 0.026f, 0.86f), 3.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 1.0f);
			CapitalPlate = FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.031f, 0.026f, 0.92f), 3.0f, FLinearColor(FColor::FromHex(TEXT("B8975A"))).CopyWithNewOpacity(0.85f), 1.0f);
			Stripe = FSlateRoundedBoxBrush(FLinearColor::White, 1.5f);
			Disc = FSlateRoundedBoxBrush(FLinearColor::White, 12.0f);
			DiscSmall = FSlateRoundedBoxBrush(FLinearColor::White, 6.5f);
			// (on the staff's paper map a plate is a paper label: paper, a thin edge of ink, the name in ink)
			PaperLabel = FSlateRoundedBoxBrush(FLinearColor::White, 2.0f, FLinearColor(0.12f, 0.08f, 0.045f, 0.85f), 1.0f);
			Lamp = FSlateRoundedBoxBrush(FLinearColor::White, 9.0f);
			LampLit = FSlateRoundedBoxBrush(FLinearColor::White, 8.0f);
			LampFar = FSlateRoundedBoxBrush(FLinearColor::White, 6.0f);
			LampFarLit = FSlateRoundedBoxBrush(FLinearColor::White, 4.5f);
			LampBar = FSlateRoundedBoxBrush(FLinearColor::White, 1.0f);
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.0, 16.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			THEATER_SPEED_SCOPE(PaintTownPlates);
			if (!Controller.IsValid())
			{
				return LayerId;
			}
			TArray<FTownPlate> Plates;
			Controller->GetTownPlates(Plates);
			// (ruling 86: where the gates' marks are drawn, for the pointer; and the badges drawn, for them to step clear of)
			TArray<TPair<int32, FBox2D>> GateBoxes;
			TArray<FBox2D> Badges;
			if (Plates.Num() == 0)
			{
				Controller->NoteGateMarks(GateBoxes, Geometry.Scale);
				DrawnTownPlates.Reset();
				return LayerId;
			}
			const float Scale = FMath::Max(Geometry.Scale, 0.01f);
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo CapitalFont = Serif(TEXT("Bold"), 12.0f, 20);
			const FSlateFontInfo TownFont = Serif(TEXT("Bold"), 10.0f, 10);
			const FSlateFontInfo LineFont = Font(TEXT("Regular"), 9);
			const FSlateFontInfo FarFont = Serif(TEXT("Bold"), 9.0f, 10);
			// From the height where the table is the staff's paper map, the plates are paper labels lettered in ink
			// (leather plates were the heaviest marks on the sheet, and lay across the countries' names).
			const bool bLabels = Controller->GetPaper() > 0.5f;
			const float Moon = FMath::Clamp(Controller->GetMoonlight(), 0.0f, 1.0f);
			const FLinearColor LabelPaper = FLinearColor(FColor::FromHex(TEXT("DDD0AE"))) * FMath::Lerp(1.0f, 0.45f, Moon);
			const FLinearColor NameInk = bLabels ? FLinearColor(FColor::FromHex(TEXT("2B2118"))) : FLinearColor(FColor::FromHex(TEXT("F2E8D5")));
			const FLinearColor LineInk = bLabels ? FLinearColor(FColor::FromHex(TEXT("5B4A38"))) : FLinearColor(FColor::FromHex(TEXT("B9AC93")));
			TArray<FBox2D> Taken;
			const TArray<FBox2D> Frame = FrameBoxes(Geometry);
			// (where the armies' colours are: a town's plate is not drawn across a flag or its strength plate)
			TArray<FBox2D> Colours;
			Controller->GetColoursOnScreen(Colours);
			for (FBox2D& Flag : Colours)
			{
				Flag.Min /= Scale;
				Flag.Max /= Scale;
			}
			// (Rulings 46 and 57; the art director's brief.) A city's order badge, left to right: its holder's colour,
			// the sign of the unit ready soonest, how many of it, a plus for other kinds, the months to go in a brass
			// disc. As high as a name plate at working height and as a harbour badge from further off.
			const FLinearColor BadgeCream(FColor::FromHex(TEXT("F4EEDA"))), BadgeBrass(FColor::FromHex(TEXT("C9A04C")));
			// (The art director's round 34: on the small plate the disc came out seven points across and its number
			// could not be read. There it is thirteen points of solid brass, a pill for two figures, and its number
			// is the size of the harbour badge's.)
			auto DiscWide = [&](const FTownPlate& Each, double Tall)
			{
				if (Tall > 19.0)
				{
					return Tall * 0.74;
				}
				return FMath::Max(13.0, Measure->Measure(FString::FromInt(FMath::Max(Each.Months, 0)), Font(TEXT("Bold"), 12)).X + 5.0);
			};
			auto OrdersWide = [&](const FTownPlate& Each, double Tall)
			{
				const FSlateFontInfo Type = Font(TEXT("Bold"), Tall > 19.0 ? 14 : 10);
				double Wide = FMath::Max(Tall * 0.14, 2.0) + 6.0 + Tall * 0.99 + 5.0;
				if (Each.Stage == 0 && Each.OrderCount > 1)
				{
					Wide += Measure->Measure(FString::Printf(TEXT("\u00D7%d"), Each.OrderCount), Type).X + 4.0;
				}
				if (Each.Stage == 0 && Each.bPlus)
				{
					Wide += Measure->Measure(TEXT("+"), Type).X + 3.0;
				}
				if (Each.Stage <= 1)
				{
					Wide += DiscWide(Each, Tall) + 4.0;
				}
				return Wide;
			};
			auto DrawOrders = [&](const FTownPlate& Each, const FVector2D& Corner, double Tall)
			{
				const FSlateFontInfo Type = Font(TEXT("Bold"), Tall > 19.0 ? 14 : 10);
				const double Wide = OrdersWide(Each, Tall), Bar = FMath::Max(Tall * 0.14, 2.0);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide, Tall), FSlateLayoutTransform(Corner)), &Plate, ESlateDrawEffect::None, Plate.GetTint(InWidgetStyle));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(Bar, Tall - 6.0), FSlateLayoutTransform(Corner + FVector2D(3.0, 3.0))), &Stripe, ESlateDrawEffect::None, Each.Colour.CopyWithNewOpacity(1.0f));
				double X = 3.0 + Bar + 4.0;
				const double SignHigh = Tall * 0.66, SignWide = SignHigh * 1.5;
				if (const FSlateBrush* Sign = OrderSignBrush(Each.Sign))
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(SignWide, SignHigh), FSlateLayoutTransform(Corner + FVector2D(X, (Tall - SignHigh) * 0.5))), Sign, ESlateDrawEffect::None, BadgeCream);
				}
				if (Each.bStar)
				{
					if (const FSlateBrush* Star = OrderSignBrush(TEXT("star")))
					{
						const double StarWide = FMath::Max(Tall * 0.4, 7.5); // (round 35: about seven points, on the small plate too)
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(FVector2D(StarWide, StarWide), FSlateLayoutTransform(Corner + FVector2D(X + SignWide - StarWide * 0.6, 1.0))), Star, ESlateDrawEffect::None, BadgeBrass);
					}
				}
				X += SignWide + 4.0;
				if (Each.Stage == 0 && Each.OrderCount > 1)
				{
					const FString Count = FString::Printf(TEXT("\u00D7%d"), Each.OrderCount);
					const FVector2D Size = Measure->Measure(Count, Type);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Corner + FVector2D(X, (Tall - Size.Y) * 0.5))), Count, Type, ESlateDrawEffect::None, BadgeCream);
					X += Size.X + 4.0;
				}
				if (Each.Stage == 0 && Each.bPlus)
				{
					const FVector2D Size = Measure->Measure(TEXT("+"), Type);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Corner + FVector2D(X, (Tall - Size.Y) * 0.5))), TEXT("+"), Type, ESlateDrawEffect::None, BadgeBrass);
					X += Size.X + 3.0;
				}
				if (Each.Stage <= 1)
				{
					const double Across = Tall > 19.0 ? Tall * 0.74 : 13.0, AcrossWide = DiscWide(Each, Tall);
					const FString Months = FString::FromInt(FMath::Max(Each.Months, 0));
					const FSlateFontInfo Numeral = Font(TEXT("Bold"), Tall > 19.0 ? 11 : 12);
					const FVector2D Size = Measure->Measure(Months, Numeral);
					const FVector2D DiscAt = Corner + FVector2D(X, (Tall - Across) * 0.5);
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(AcrossWide, Across), FSlateLayoutTransform(DiscAt)), Tall > 19.0 ? &Disc : &DiscSmall, ESlateDrawEffect::None, BadgeBrass);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(DiscAt + FVector2D((AcrossWide - Size.X) * 0.5, (Across - Size.Y) * 0.5))), Months, Numeral, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("1F1808"))));
				}
				if (Each.bSpied)
				{
					// (Another country's city, known through an agent: the plate wears a dashed amber frame, as everything
					// only reported does.)
					const FLinearColor Reported(FColor(232, 168, 62));
					const FPaintGeometry Whole = Geometry.ToPaintGeometry();
					auto Dashes = [&](const FVector2D& From, const FVector2D& To)
					{
						const double Long = FVector2D::Distance(From, To);
						const FVector2D Way = (To - From) / FMath::Max(Long, 1.0);
						for (double Run = 0.0; Run < Long; Run += 6.0)
						{
							TArray<FVector2D> Dash;
							Dash.Add(From + Way * Run);
							Dash.Add(From + Way * FMath::Min(Run + 3.5, Long));
							FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, Whole, Dash, ESlateDrawEffect::None, Reported, true, 1.3f);
						}
					};
					const FVector2D A = Corner + FVector2D(-1.5, -1.5), B = Corner + FVector2D(Wide + 1.5, Tall + 1.5);
					Dashes(A, FVector2D(B.X, A.Y));
					Dashes(FVector2D(B.X, A.Y), B);
					Dashes(B, FVector2D(A.X, B.Y));
					Dashes(FVector2D(A.X, B.Y), A);
				}
			};
			for (const FTownPlate& Each : Plates)
			{
				if (Each.bOrders && Each.Name.IsEmpty())
				{
					// (a badge by itself, where its city's name plate is not shown from this height)
					const double Tall = 16.0, Wide = OrdersWide(Each, Tall);
					const FVector2D Corner(Each.At.X / Scale - Wide * 0.5, Each.At.Y / Scale - Tall - 2.0);
					if (!UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(Wide, Tall))))
					{
						DrawOrders(Each, Corner, Tall);
						Badges.Add(FBox2D(Corner, Corner + FVector2D(Wide, Tall)));
					}
					continue;
				}
				if (Each.bGate)
				{
					// (Ruling 86; the art director's brief and her lamps.) A sea gate's mark. From far up the lamp alone,
					// twelve across: a plain green disc, or a red disc with a cream bar. Nearer, a small dark plate in the
					// name plates' look with the holder's flag and the lamp, eighteen across, with a padlock on it for
					// close up and for eyes that do not tell red from green; and close up, its name after the lamp.
					const FLinearColor Lit = Each.bGateOpen ? FLinearColor(FColor::FromHex(TEXT("9CC08A"))) : FLinearColor(FColor::FromHex(TEXT("CF5A41")));
					const FLinearColor Dark = Each.bGateOpen ? FLinearColor(FColor::FromHex(TEXT("1D2A17"))) : FLinearColor(FColor::FromHex(TEXT("2A110B")));
					const bool bLampOnly = Each.Stage >= 2;
					const FVector2D NameSize = Each.Stage == 0 ? FVector2D(Measure->Measure(Each.Name, TownFont)) : FVector2D::ZeroVector;
					const double Wide = bLampOnly ? 12.0 : 52.0 + (Each.Stage == 0 ? NameSize.X + 9.0 : 0.0), High = bLampOnly ? 12.0 : 22.0;
					FVector2D Corner(Each.At.X / Scale - Wide * 0.5, Each.At.Y / Scale - High * 0.5);
					// (it steps clear of flags, name plates and badges as a name plate does: to one side or down)
					for (int32 Pass = 0; Pass < 3; ++Pass)
					{
						const FBox2D Here(Corner - FVector2D(2.0, 1.0), Corner + FVector2D(Wide + 2.0, High + 1.0));
						auto Crosses = [&Here](const FBox2D& Other) { return Other.Intersect(Here); };
						const FBox2D* Across = Colours.FindByPredicate(Crosses);
						Across = Across ? Across : Taken.FindByPredicate(Crosses);
						Across = Across ? Across : Badges.FindByPredicate(Crosses);
						if (!Across)
						{
							break;
						}
						const double Left = Here.Max.X - Across->Min.X, Right = Across->Max.X - Here.Min.X, Down = Across->Max.Y - Here.Min.Y;
						if (Down <= High * 2.0 && Down <= FMath::Min(Left, Right))
						{
							Corner.Y += Down + 1.0;
						}
						else
						{
							Corner.X += Left < Right ? -(Left + 1.0) : Right + 1.0;
						}
					}
					if (UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(Wide, High))))
					{
						continue;
					}
					GateBoxes.Add(TPair<int32, FBox2D>(Each.Gate, FBox2D(Corner * Scale, (Corner + FVector2D(Wide, High)) * Scale)));
					const FPaintGeometry Whole = Geometry.ToPaintGeometry();
					if (bLampOnly)
					{
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(12.0, 12.0), FSlateLayoutTransform(Corner)), &LampFar, ESlateDrawEffect::None, Dark);
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(9.0, 9.0), FSlateLayoutTransform(Corner + FVector2D(1.5, 1.5))), &LampFarLit, ESlateDrawEffect::None, Lit);
						if (!Each.bGateOpen)
						{
							FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(FVector2D(6.4, 2.2), FSlateLayoutTransform(Corner + FVector2D(2.8, 4.9))), &LampBar, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("F2E8D5"))));
						}
						continue;
					}
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide, High), FSlateLayoutTransform(Corner)), &Plate, ESlateDrawEffect::None, Plate.GetTint(InWidgetStyle));
					if (const FSlateBrush* Flag = GateFlag(Each.Flag))
					{
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(24.0, 16.0), FSlateLayoutTransform(Corner + FVector2D(3.0, 3.0))), Flag, ESlateDrawEffect::None, FLinearColor::White);
					}
					// (the lamp: a dark rim, the lit disc, and the padlock in the rim's own dark)
					const FVector2D LampAt = Corner + FVector2D(31.0, 2.0);
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(18.0, 18.0), FSlateLayoutTransform(LampAt)), &Lamp, ESlateDrawEffect::None, Dark);
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(FVector2D(16.0, 16.0), FSlateLayoutTransform(LampAt + FVector2D(1.0, 1.0))), &LampLit, ESlateDrawEffect::None, Lit);
					const double Unit = 18.0 / 16.0; // (her lamps are drawn in a box of sixteen)
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, Geometry.ToPaintGeometry(FVector2D(7.4 * Unit, 5.2 * Unit), FSlateLayoutTransform(LampAt + FVector2D(4.3, 7.6) * Unit)), &LampBar, ESlateDrawEffect::None, Dark);
					TArray<FVector2D> Shackle;
					const double Foot = Each.bGateOpen ? 5.0 : 5.6;
					Shackle.Add(LampAt + FVector2D(5.9, 7.8) * Unit);
					for (int32 K = 0; K <= 8; ++K)
					{
						// (the bow of the padlock: closed it comes down into the body again, open its far end stands free)
						const double Turn = PI * (1.0 - K / 8.0);
						Shackle.Add(LampAt + FVector2D(8.0 + 2.1 * FMath::Cos(Turn), Foot - (Each.bGateOpen ? 0.5 * K / 8.0 : 0.0) - 2.1 * FMath::Sin(Turn)) * Unit);
					}
					if (!Each.bGateOpen)
					{
						Shackle.Add(LampAt + FVector2D(10.1, 7.8) * Unit);
					}
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, Whole, Shackle, ESlateDrawEffect::None, Dark, true, 1.7f);
					if (Each.Stage == 0)
					{
						FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(Corner + FVector2D(55.0, (High - NameSize.Y) * 0.5))), Each.Name, TownFont, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("F2E8D5"))));
					}
					continue;
				}
				if (Each.bSeal)
				{
					// A directive's seal: a small brass plate with a dark figure, the months left to carry it out.
					const FSlateFontInfo SealFont = Font(TEXT("Bold"), 10);
					const FString Months = Each.Name + TEXT(" mo");
					const FVector2D SealSize = Measure->Measure(Months, SealFont);
					const double SealWide = SealSize.X + 14.0, SealHigh = 18.0;
					const FVector2D Corner(Each.At.X / Scale - SealWide * 0.5, Each.At.Y / Scale);
					if (UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(SealWide, SealHigh))))
					{
						continue;
					}
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(SealWide + 2.0, SealHigh + 2.0), FSlateLayoutTransform(Corner - FVector2D(1.0, 1.0))), &Stripe, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("3A2E12"))));
					// (the cabinet's directive is brass; the player's own promise is sealing-wax red)
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(SealWide, SealHigh), FSlateLayoutTransform(Corner)), &Stripe, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(Each.bVow ? TEXT("9C2B22") : TEXT("C9A227"))));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(SealSize, FSlateLayoutTransform(Corner + FVector2D(7.0, 2.0))), Months, SealFont, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(Each.bVow ? TEXT("F6E7D2") : TEXT("1F1808"))));
					Taken.Add(FBox2D(Corner - FVector2D(3.0, 2.0), Corner + FVector2D(SealWide + 3.0, SealHigh + 2.0)));
					continue;
				}
				if (Each.bCrowd)
				{
					// The armies put away behind a crowded province's one flag: a small brass chip with their number.
					const FSlateFontInfo CountFont = Font(TEXT("Bold"), 12);
					const FVector2D CountSize = Measure->Measure(Each.Name, CountFont);
					const double ChipWide = CountSize.X + 12.0, ChipHigh = 17.0;
					const FVector2D Corner(Each.At.X / Scale - ChipWide * 0.5, Each.At.Y / Scale);
					if (UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(ChipWide, ChipHigh))))
					{
						continue;
					}
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(ChipWide + 2.0, ChipHigh + 2.0), FSlateLayoutTransform(Corner - FVector2D(1.0, 1.0))), &Stripe, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("3A2E12"))));
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(ChipWide, ChipHigh), FSlateLayoutTransform(Corner)), &Stripe, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("C9A55A"))));
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(CountSize, FSlateLayoutTransform(Corner + FVector2D(6.0, (ChipHigh - CountSize.Y) * 0.5))), Each.Name, CountFont, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("1F1808"))));
					continue;
				}
				if (Each.bHarbour)
				{
					// Ships in harbour: a small plate with an anchor drawn on it and the number of ships.
					const FSlateFontInfo CountFont = Font(TEXT("Bold"), 13); // (the typewriter's figures are small for their size: set a size up, in bold)
					const FVector2D CountSize = Measure->Measure(Each.Name, CountFont);
					const double BadgeWide = 31.0 + CountSize.X, BadgeHigh = 17.0;
					const FVector2D Corner(Each.At.X / Scale - BadgeWide * 0.5, Each.At.Y / Scale);
					if (UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(BadgeWide, BadgeHigh))))
					{
						continue;
					}
					Badges.Add(FBox2D(Corner, Corner + FVector2D(BadgeWide, BadgeHigh)));
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(BadgeWide, BadgeHigh), FSlateLayoutTransform(Corner)), &Plate, ESlateDrawEffect::None, Plate.GetTint(InWidgetStyle));
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(4.0, BadgeHigh - 6.0), FSlateLayoutTransform(Corner + FVector2D(4.0, 3.0))), &Stripe, ESlateDrawEffect::None, Each.Colour);
					// (the anchor: a ring, a shank with its stock across, and the two flukes on their curved arm)
					const FVector2D Middle = Corner + FVector2D(17.0, BadgeHigh * 0.5);
					const FPaintGeometry Whole = Geometry.ToPaintGeometry();
					auto Stroke = [&](std::initializer_list<FVector2D> Points, float Thick)
					{
						TArray<FVector2D> Line;
						for (const FVector2D& Point : Points)
						{
							Line.Add(Middle + Point);
						}
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Whole, Line, ESlateDrawEffect::None, NameInk, true, Thick);
					};
					Stroke({ { 0.0, -3.6 }, { 0.0, 5.0 } }, 1.5f);
					Stroke({ { -2.6, -2.2 }, { 2.6, -2.2 } }, 1.4f);
					Stroke({ { -1.0, -5.2 }, { 1.0, -5.2 }, { 1.0, -3.8 }, { -1.0, -3.8 }, { -1.0, -5.2 } }, 1.0f);
					Stroke({ { -4.4, 1.6 }, { -3.2, 4.0 }, { 0.0, 5.2 }, { 3.2, 4.0 }, { 4.4, 1.6 } }, 1.5f);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(CountSize, FSlateLayoutTransform(Corner + FVector2D(25.0, (BadgeHigh - CountSize.Y) * 0.5))), Each.Name, CountFont, ESlateDrawEffect::None, NameInk);
					continue;
				}
				if (Each.bAirfield)
				{
					// An airfield in use: a small plate with a little aircraft drawn on it and the number of groups.
					const FSlateFontInfo CountFont = Font(TEXT("Bold"), 13); // (the typewriter's figures are small for their size: set a size up, in bold)
					const FVector2D CountSize = Measure->Measure(Each.Name, CountFont);
					const double BadgeWide = 31.0 + CountSize.X, BadgeHigh = 17.0;
					const FVector2D Corner(Each.At.X / Scale - BadgeWide * 0.5, Each.At.Y / Scale);
					if (UnderFrame(Frame, FBox2D(Corner, Corner + FVector2D(BadgeWide, BadgeHigh))))
					{
						continue;
					}
					Badges.Add(FBox2D(Corner, Corner + FVector2D(BadgeWide, BadgeHigh)));
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(BadgeWide, BadgeHigh), FSlateLayoutTransform(Corner)), &Plate, ESlateDrawEffect::None, Plate.GetTint(InWidgetStyle));
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(4.0, BadgeHigh - 6.0), FSlateLayoutTransform(Corner + FVector2D(4.0, 3.0))), &Stripe, ESlateDrawEffect::None, Each.Colour);
					// (the aircraft, seen from above with its nose up the plate: a body, wings and a tailplane)
					const FVector2D Middle = Corner + FVector2D(17.0, BadgeHigh * 0.5);
					const FPaintGeometry Whole = Geometry.ToPaintGeometry();
					auto Stroke = [&](const FVector2D& From, const FVector2D& To, float Thick)
					{
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Whole, TArray<FVector2D>{ Middle + From, Middle + To }, ESlateDrawEffect::None, NameInk, true, Thick);
					};
					Stroke(FVector2D(0.0, -5.0), FVector2D(0.0, 5.0), 2.0f);
					Stroke(FVector2D(-5.5, -0.8), FVector2D(5.5, -0.8), 2.2f);
					Stroke(FVector2D(-2.4, 4.2), FVector2D(2.4, 4.2), 1.6f);
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(CountSize, FSlateLayoutTransform(Corner + FVector2D(25.0, (BadgeHigh - CountSize.Y) * 0.5))), Each.Name, CountFont, ESlateDrawEffect::None, NameInk);
					continue;
				}
				// (from far off the capitals' plates are small, so the countries' own names can still be read)
				const FSlateFontInfo& NameFont = Each.bFar ? FarFont : Each.bCapital ? CapitalFont : TownFont;
				// (a capital's star is cut from the plain face: the serif has none)
				const bool bStar = Each.bCapital && !Each.bFar;
				const FString Shown = Each.Name;
				const double StarWide = bStar ? 15.0 : 0.0; // (the star is drawn, not lettered: the typewriter has no star)
				FVector2D NameSize = Measure->Measure(Shown, NameFont);
				NameSize.X += StarWide;
				const FVector2D LineSize = Each.Line.IsEmpty() ? FVector2D::ZeroVector : FVector2D(Measure->Measure(Each.Line, LineFont));
				const double Wide = FMath::Max(NameSize.X, LineSize.X) + 19.0;
				const double High = NameSize.Y + (Each.Line.IsEmpty() ? 0.0 : LineSize.Y - 1.0) + 5.0;
				FVector2D Corner(Each.At.X / Scale - Wide * 0.5, Each.At.Y / Scale);
				// The plate steps aside from an army's colours: the shortest way clear, to one side or down (never up,
				// across its own town), and no further than its own width or twice its height.
				for (int32 Pass = 0; Pass < 2; ++Pass)
				{
					const FBox2D Here(Corner - FVector2D(2.0, 1.0), Corner + FVector2D(Wide + 2.0, High + 1.0));
					const FBox2D* Across = Colours.FindByPredicate([&Here](const FBox2D& Flag) { return Flag.Intersect(Here); });
					if (!Across)
					{
						break;
					}
					const double Left = Here.Max.X - Across->Min.X, Right = Across->Max.X - Here.Min.X, Down = Across->Max.Y - Here.Min.Y;
					if (Down <= High * 2.0 && Down <= FMath::Min(Left, Right))
					{
						Corner.Y += Down + 1.0;
					}
					else if (FMath::Min(Left, Right) <= Wide)
					{
						Corner.X += Left < Right ? -(Left + 1.0) : Right + 1.0;
					}
					else
					{
						break;
					}
				}
				const FBox2D Box(Corner - FVector2D(3.0, 2.0), Corner + FVector2D(Wide + 3.0, High + 2.0));
				bool bCrowded = UnderFrame(Frame, Box); // (a plate that would sit under a piece of the frame is not drawn)
				for (const FBox2D& Other : Taken)
				{
					if (Other.Intersect(Box))
					{
						bCrowded = true;
						break;
					}
				}
				if (bCrowded)
				{
					continue;
				}
				Taken.Add(Box);
				const FSlateBrush& Back = bLabels ? PaperLabel : Each.bCapital && !Each.bFar ? CapitalPlate : Plate;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide, High), FSlateLayoutTransform(Corner)), &Back, ESlateDrawEffect::None, bLabels ? LabelPaper.CopyWithNewOpacity(0.92f) : Back.GetTint(InWidgetStyle));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(4.0, High - 6.0), FSlateLayoutTransform(Corner + FVector2D(4.0, 3.0))), &Stripe, ESlateDrawEffect::None, Each.Colour.CopyWithNewOpacity(1.0f));
				if (Each.bOrders)
				{
					// (the city's order badge: directly above its name plate, left edges in line)
					const double Tall = Each.Stage == 0 ? 22.0 : 16.0;
					DrawOrders(Each, Corner - FVector2D(0.0, Tall + 2.0), Tall);
					Badges.Add(FBox2D(Corner - FVector2D(0.0, Tall + 2.0), Corner + FVector2D(OrdersWide(Each, Tall), -2.0)));
				}
				if (bStar)
				{
					// A capital's star, drawn as the frame's signs are drawn: the five strokes of a star in one line, close
					// enough together at this size to fill it.
					const FVector2D Heart = Corner + FVector2D(13.0 + 5.5, High * 0.5);
					TArray<FVector2D> Strokes;
					for (int32 Point = 0; Point <= 5; ++Point)
					{
						const double Angle = FMath::DegreesToRadians(144.0 * Point);
						Strokes.Add(Heart + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * 5.6);
					}
					const FLinearColor StarInk = bLabels ? FLinearColor(FColor::FromHex(TEXT("8E2C20"))) : FLinearColor(FColor::FromHex(TEXT("E0B872")));
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), Strokes, ESlateDrawEffect::None, StarInk, true, 1.8f);
					for (int32 Point = 0; Point < 5; ++Point)
					{
						// (and a stroke from its heart to each point, so that the star is solid and not a knot of lines)
						FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), TArray<FVector2D>{ Heart, FMath::Lerp(Heart, Strokes[Point], 0.72) }, ESlateDrawEffect::None, StarInk, true, 2.6f);
					}
				}
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(Corner + FVector2D(13.0 + StarWide, 2.0))), Shown, NameFont, ESlateDrawEffect::None, NameInk);
				if (!Each.Line.IsEmpty())
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(LineSize, FSlateLayoutTransform(Corner + FVector2D(13.0, 1.0 + NameSize.Y))), Each.Line, LineFont, ESlateDrawEffect::None, LineInk);
				}
			}
			Controller->NoteGateMarks(GateBoxes, Scale);
			DrawnTownPlates = Taken;
			return LayerId + 1;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
		FSlateBrush Plate;
		FSlateBrush CapitalPlate;
		FSlateBrush Stripe;
		FSlateBrush Disc; // the brass disc of a city's order badge
		FSlateBrush DiscSmall; // and the smaller one of the badge seen from further off
		FSlateBrush PaperLabel;
		// (Ruling 86.) A gate's lamp: its dark rim and its lit disc, at both sizes; the padlock's body and the far lamp's bar.
		FSlateBrush Lamp, LampLit, LampFar, LampFarLit, LampBar;
		mutable TMap<FString, TSharedPtr<FSlateBrush>> GateFlags;
		/** The holder's flag, as the map keeps it; nothing until the picture is ready. */
		const FSlateBrush* GateFlag(const FString& Tag) const
		{
			if (const TSharedPtr<FSlateBrush>* Found = GateFlags.Find(Tag))
			{
				return Found->Get();
			}
			ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			UTexture2D* Picture = (Table && !Tag.IsEmpty()) ? Table->GetFlagTexture(Tag) : nullptr;
			if (!Picture)
			{
				return nullptr; // not ready yet: asked again next time
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Picture);
			Brush->ImageSize = FVector2D(96.0f, 64.0f);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			GateFlags.Add(Tag, Brush);
			return Brush.Get();
		}
	};

	class SMinimap : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMinimap) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
			SLATE_ARGUMENT(float, High)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
			High = Args._High;
			Picture.DrawAs = ESlateBrushDrawType::Image;
			West.DrawAs = ESlateBrushDrawType::Image;
			East.DrawAs = ESlateBrushDrawType::Image;
			Dot.DrawAs = ESlateBrushDrawType::Image;
			Dot.TintColor = FSlateColor(FLinearColor::White);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			double Share = 0.87;
			// (the whole world's small map is 340 by 187, the world's own shape: wider and lower than the map of one theatre)
			float Tall = High;
			if (Controller.IsValid())
			{
				if (const UTexture2D* Texture = Controller->GetMinimapTexture())
				{
					Share = static_cast<double>(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
				}
				if (Controller->IsMinimapWorld())
				{
					Tall = 187.0f;
				}
			}
			return FVector2D(Tall * Share, Tall);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			THEATER_SPEED_SCOPE(PaintMinimap);
			if (!Controller.IsValid())
			{
				return LayerId;
			}
			UTexture2D* Texture = Controller->GetMinimapTexture();
			if (!Texture)
			{
				return LayerId;
			}
			if (Picture.GetResourceObject() != Texture)
			{
				Picture.SetResourceObject(Texture);
				Picture.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				West.SetResourceObject(Texture);
				West.ImageSize = Picture.ImageSize;
				East.SetResourceObject(Texture);
				East.ImageSize = Picture.ImageSize;
			}
			const FVector2D Size = Geometry.GetLocalSize();
			// (by night the small map goes to moonlight with the table: darker and bluer, its marks as bright as ever)
			const float Moon = FMath::Clamp(Controller->GetMoonlight(), 0.0f, 1.0f);
			const FLinearColor Lit = FMath::Lerp(FLinearColor::White, FLinearColor(0.34f, 0.45f, 0.7f), Moon);

			TArray<FVector> Dots;
			FBox2D Looking(ForceInit);
			Controller->GetMinimapMarks(Dots, Looking);
			const bool bWorld = Controller->IsMinimapWorld();
			if (bWorld)
			{
				// The whole world, centred on the longitude the eye is over: the world slides under a box that stays
				// in the middle, and comes round again at either side, so that the picture has no seam and the box
				// is never split in two. While the mouse is held in it the map is held still; let go, it eases to
				// its new centre in about a quarter of a second.
				const double Target = Controller->GetMinimapCentre();
				if (!bCentred)
				{
					bCentred = true;
					Centre = Target;
				}
				else if (!bHeld)
				{
					double Way = Target - Centre;
					Way -= FMath::RoundToDouble(Way); // (the short way round)
					Centre = FMath::Abs(Way) < 0.004 ? Target : Centre + Way * FMath::Min(1.0, static_cast<double>(Args.GetDeltaTime()) * 14.0);
					Centre -= FMath::FloorToDouble(Centre);
				}
				double Shift = Centre - 0.5;
				Shift -= FMath::FloorToDouble(Shift);
				// (the picture in two parts: from the shift to its east edge on the left, from its west edge to the shift on the right)
				const double Left = (1.0 - Shift) * Size.X;
				West.SetUVRegion(FBox2f(FVector2f(static_cast<float>(Shift), 0.0f), FVector2f(1.0f, 1.0f)));
				East.SetUVRegion(FBox2f(FVector2f(0.0f, 0.0f), FVector2f(static_cast<float>(Shift), 1.0f)));
				if (Left > 0.5)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Left, Size.Y), FSlateLayoutTransform(FVector2D::ZeroVector)), &West, ESlateDrawEffect::IgnoreTextureAlpha, Lit);
				}
				if (Size.X - Left > 0.5)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Size.X - Left, Size.Y), FSlateLayoutTransform(FVector2D(Left, 0.0))), &East, ESlateDrawEffect::IgnoreTextureAlpha, Lit);
				}
				for (FVector& Mark : Dots)
				{
					Mark.X -= Shift;
					Mark.X -= FMath::FloorToDouble(Mark.X);
				}
				if (Looking.bIsValid)
				{
					const double Half = FMath::Min((Looking.Max.X - Looking.Min.X) * 0.5, 0.5);
					double Middle = (Looking.Max.X + Looking.Min.X) * 0.5 - Shift;
					Middle -= FMath::FloorToDouble(Middle);
					Looking.Min.X = Middle - Half;
					Looking.Max.X = Middle + Half;
				}
			}
			else
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), &Picture, ESlateDrawEffect::IgnoreTextureAlpha, Lit);
			}
			static const FLinearColor Inks[] = { FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), FLinearColor(0.45f, 0.8f, 1.0f, 1.0f), FLinearColor(1.0f, 0.2f, 0.12f, 1.0f), FLinearColor(0.1f, 0.1f, 0.1f, 0.85f) };
			for (const FVector& Mark : Dots)
			{
				const int32 Whose = FMath::Clamp(FMath::RoundToInt32(Mark.Z), 0, 3);
				// (on the whole world's map a country is a few dozen points wide: the marks are smaller there, or they bury it)
				const double Wide = (Whose == 0 ? 5.0 : Whose == 3 ? 3.0 : 4.0) * (bWorld ? 0.6 : 1.0);
				const FVector2D At(Mark.X * Size.X - Wide * 0.5, Mark.Y * Size.Y - Wide * 0.5);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(Wide, Wide), FSlateLayoutTransform(At)), &Dot, ESlateDrawEffect::None, Inks[Whose]);
			}
			if (Looking.bIsValid)
			{
				// The part of the map on screen, as a box of four thin lines, kept inside the picture.
				const FVector2D A(FMath::Clamp(Looking.Min.X, 0.0, 1.0) * Size.X, FMath::Clamp(Looking.Min.Y, 0.0, 1.0) * Size.Y);
				const FVector2D B(FMath::Clamp(Looking.Max.X, 0.0, 1.0) * Size.X, FMath::Clamp(Looking.Max.Y, 0.0, 1.0) * Size.Y);
				TArray<FVector2D> Box;
				Box.Add(A);
				Box.Add(FVector2D(B.X, A.Y));
				Box.Add(B);
				Box.Add(FVector2D(A.X, B.Y));
				Box.Add(A);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(), Box, ESlateDrawEffect::None, FLinearColor(1.0f, 0.9f, 0.55f, 0.95f), true, 1.5f);
			}
			return LayerId + 2;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			bHeld = true;
			Jump(Geometry, Event);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}

		virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (bHeld && HasMouseCapture())
			{
				Jump(Geometry, Event);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (bHeld)
			{
				bHeld = false;
				return FReply::Handled().ReleaseMouseCapture();
			}
			return FReply::Unhandled();
		}

	private:
		void Jump(const FGeometry& Geometry, const FPointerEvent& Event) const
		{
			const FVector2D Size = Geometry.GetLocalSize();
			const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
			if (Controller.IsValid() && Size.X > 0.0 && Size.Y > 0.0)
			{
				// (on the whole world's map a place is told from where the map is centred, which is held still while the mouse is down)
				const double Across = Controller->IsMinimapWorld() ? Centre - 0.5 + Local.X / Size.X : Local.X / Size.X;
				Controller->JumpToMapShare(FVector2D(Across, Local.Y / Size.Y));
			}
		}

		TWeakObjectPtr<ACampaignPlayerController> Controller;
		float High = 200.0f;
		mutable FSlateBrush Picture;
		mutable FSlateBrush West, East; // (the whole world's picture, drawn in two parts either side of where it comes round)
		mutable double Centre = 0.5; // (how far round the world the middle of the small map is: 0 at the picture's west edge, 1 at its east)
		mutable bool bCentred = false;
		FSlateBrush Dot;
		bool bHeld = false;
	};

	/**
	 * (Rules 84.3.3, FIRST COMMAND; the art director's look brief of 16 November, accepted by ruling 147.) The staff's
	 * note: a staff officer's paper note clipped to the edge of the map table, the kit's own paper, faces and brass.
	 * Form 0 is the note at its home (340 wide), 1 the slip beside a card that veils the map (148 at most), 2 the strip
	 * laid across a card: lesson 1 on the first month's card, or any note where the card leaves no margin for the slip
	 * (it is never hidden). The brief's measures are pixels of the 1600 by 1000 frame: Scale turns them into the
	 * widget's own (0.9 on the table's frame of 900 high, 1 inside a card, which has its own).
	 */
	class SStaffNote : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SStaffNote) : _Form(0), _Scale(0.9f), _SlipWide(148.0f), _FromRight(false) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
			SLATE_ATTRIBUTE(int32, Form)
			SLATE_ARGUMENT(float, Scale)
			SLATE_ATTRIBUTE(float, SlipWide) // the slip's width, in the brief's pixels
			SLATE_ATTRIBUTE(bool, FromRight) // the second home slides in from the right
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
			Form = Args._Form;
			K = Args._Scale;
			SlipWide = Args._SlipWide;
			FromRight = Args._FromRight;
		}

		/** What the note shows and how far it has come or gone: false where there is nothing to draw. */
		bool What(FLessonNote& Out, float& Alpha, double& Slide, bool& bStamped, double& StampAge) const
		{
			Alpha = 1.0f;
			Slide = 0.0;
			bStamped = false;
			StampAge = -1.0;
			if (!Controller.IsValid())
			{
				return false;
			}
			const ACampaignPlayerController* C = Controller.Get();
			if (Form.Get() == 2 && C->IsLessonLaid())
			{
				Out = C->GetLaidLesson();
				return true;
			}
			if (C->HasLessonNote())
			{
				Out = C->GetLessonNote();
				// (it slides 12 px in from its edge over 220 ms, and comes up as it does)
				const double Age = FMath::Clamp(C->GetLessonAge() / 0.22, 0.0, 1.0);
				const double Ease = 1.0 - FMath::Pow(1.0 - Age, 3.0);
				Slide = (1.0 - Ease) * 12.0;
				Alpha = static_cast<float>(FMath::Clamp(Age * 1.6, 0.0, 1.0));
				bStamped = C->IsLessonStamped() && Out.Kind == TEXT("lesson");
				StampAge = bStamped ? C->GetLessonStampAge() : -1.0;
				return true;
			}
			float Faded = 0.0f;
			if (C->GetLessonLeaving(Out, Faded))
			{
				// (it leaves by fading in 110 ms, with no travel; a stamped note keeps its stamp, landed, as it goes)
				Alpha = 1.0f - Faded;
				bStamped = C->IsLessonGoneStamped();
				StampAge = bStamped ? 1.0 : -1.0;
				return true;
			}
			return false;
		}

		/** The note set out at a width (in the widget's own units): its lines, its parts and their places. */
		struct FLaid
		{
			double W = 0.0, H = 0.0, Top = 0.0; // the paper, and where it starts under the clip
			bool bClip = true, bStrip = false, bTip = false, bDeed = false;
			FString Label, Count, Quiet;
			FVector2D LabelAt, CountAt, QuietAt, QuietSize, TagSize;
			TArray<FString> HeadLines, TextLines, DeedLines;
			FVector2D HeadAt, TextAt, DeedAt, BoxAt;
			double HeadLine = 0.0, TextLine = 0.0, DeedLine = 0.0, Box = 0.0, RuleY = -1.0, DividerX = -1.0, DividerTop = 0.0, DividerBottom = 0.0;
			FSlateFontInfo LabelFace, CountFace, QuietFace, HeadFace, TextFace, DeedFace, StampFace;
			FVector2D StampAt, StampSize;
		};

		static void Wrap(const FString& Words, const FSlateFontInfo& Face, double Room, TArray<FString>& Out)
		{
			Out.Reset();
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			TArray<FString> Each;
			Words.ParseIntoArrayWS(Each);
			FString Line;
			for (const FString& Word : Each)
			{
				const FString Tried = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure->Measure(Tried, Face).X > Room)
				{
					Out.Add(Line);
					Line = Word;
				}
				else
				{
					Line = Tried;
				}
			}
			if (!Line.IsEmpty())
			{
				Out.Add(Line);
			}
		}

		/** (The laid strip's sentences: as many lines as the measure needs, of lengths as even as they will go.) */
		static void WrapEven(const FString& Words, const FSlateFontInfo& Face, double Room, TArray<FString>& Out)
		{
			Wrap(Words, Face, Room, Out);
			const int32 Lines = Out.Num();
			if (Lines < 2)
			{
				return;
			}
			double Low = Room / Lines, High = Room;
			TArray<FString> Tried;
			for (int32 Step = 0; Step < 12; ++Step)
			{
				const double Mid = 0.5 * (Low + High);
				Wrap(Words, Face, Mid, Tried);
				if (Tried.Num() <= Lines)
				{
					High = Mid;
				}
				else
				{
					Low = Mid;
				}
			}
			Wrap(Words, Face, High, Out);
		}

		/** The note set out, kept while the note, its stamp and the width are the same (the strip's even lines are sought
		 * a dozen times over, and the note is asked for its size and drawn every frame). */
		FLaid Lay(const FLessonNote& Note, bool bStamped, double Width) const
		{
			const FString Key = FString::Printf(TEXT("%d|%d|%.2f|%s|%s|%d|%d|%s|%s|%s"), Form.Get(), bStamped ? 1 : 0, Width, *Note.Kind, *Note.Id, Note.N, Note.Of, *Note.Head, *Note.Text, *Note.Deed);
			if (Key != LaidKey)
			{
				LaidKey = Key;
				LaidKept = LayAfresh(Note, bStamped, Width);
			}
			return LaidKept;
		}

		FLaid LayAfresh(const FLessonNote& Note, bool bStamped, double Width) const
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto P = [this](double Pixels) { return Pixels * K; };
			auto Pt = [this](double Pixels) { return static_cast<float>(Pixels * K * 0.75); }; // (a face is asked for in points, a third larger than pixels)
			auto Wide = [&Measure](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face).X); };
			const int32 Shape = Form.Get();
			FLaid L;
			L.bStrip = Shape == 2;
			L.bClip = !L.bStrip;
			L.bTip = Note.Kind == TEXT("tip");
			const bool bSlip = Shape == 1, bLesson = Note.Kind == TEXT("lesson"), bFirst = bLesson && Note.Id == TEXT("month");
			L.W = Width;
			L.Top = L.bClip ? P(8.0) : 0.0;
			L.Label = L.bTip ? TEXT("TIP") : TEXT("STAFF NOTE");
			L.Count = bLesson ? FString::Printf(TEXT("%d OF %d"), Note.N, Note.Of) : FString();
			L.Quiet = bStamped || bFirst ? FString() : bLesson ? TEXT("LATER") : TEXT("CLOSE");
			L.bDeed = bLesson && !Note.Deed.IsEmpty() && !bFirst;
			L.LabelFace = Typed(Pt(11.0), true, L.bTip ? 60 : 140);
			L.CountFace = Typed(Pt(11.0));
			L.QuietFace = Typed(Pt(11.0), true, 40);
			L.HeadFace = Serif(TEXT("Bold"), Pt(bSlip ? 16.0 : 18.0));
			L.TextFace = Serif(TEXT("Regular"), Pt(bSlip ? 14.0 : 15.0));
			L.DeedFace = Typed(Pt(bSlip ? 12.0 : 13.0));
			L.StampFace = Serif(TEXT("Bold"), Pt(bSlip ? 17.0 : 22.0), 60);
			L.HeadLine = P(bSlip ? 20.0 : 24.0);
			L.TextLine = P(bSlip ? 20.0 : 22.0);
			L.DeedLine = P(bSlip ? 16.0 : 18.0);
			L.Box = P(bSlip ? 15.0 : 18.0);
			const double Row = P(16.0);
			L.QuietSize = FVector2D(L.Quiet.IsEmpty() ? 0.0 : Wide(L.Quiet, L.QuietFace), Row);
			L.TagSize = L.bTip ? FVector2D(Wide(L.Label, L.LabelFace) + P(12.0), Row) : FVector2D::ZeroVector;
			L.StampSize = FVector2D(Wide(TEXT("DONE"), L.StampFace) + P(bSlip ? 16.0 : 22.0), P(bSlip ? 25.0 : 34.0));
			if (L.bStrip)
			{
				// The strip: STAFF NOTE and its number over the heading in a column 176 wide, a dashed rule, then the
				// sentences at a measure of at most 820, their lines as even as they go; the deed line under them.
				const double PadX = P(17.0), PadY = P(11.0), Column = P(176.0), Gap = P(28.0);
				const double TextX = PadX + Column + Gap + Gap;
				const double Room = FMath::Max(P(200.0), FMath::Min(P(820.0), Width - TextX - PadX - (L.Quiet.IsEmpty() ? 0.0 : L.QuietSize.X + P(20.0))));
				Wrap(Note.Head, L.HeadFace, Column, L.HeadLines);
				WrapEven(Note.Text, L.TextFace, Room, L.TextLines);
				if (L.bDeed)
				{
					Wrap(Note.Deed, L.DeedFace, Room - L.Box - P(10.0) - (bStamped ? L.StampSize.X + P(16.0) : 0.0), L.DeedLines);
				}
				const double Left = Row + P(2.0) + L.HeadLines.Num() * L.HeadLine;
				const double Right = L.TextLines.Num() * L.TextLine + (L.bDeed ? P(8.0) + FMath::Max(L.Box, L.DeedLines.Num() * L.DeedLine) : 0.0);
				const double Inner = FMath::Max3(Left, Right, P(44.0));
				L.H = FMath::Max(P(68.0), Inner + 2.0 * PadY);
				const double LeftTop = PadY + (L.H - 2.0 * PadY - Left) * 0.5, RightTop = PadY + (L.H - 2.0 * PadY - Right) * 0.5;
				L.LabelAt = FVector2D(PadX, LeftTop);
				L.CountAt = FVector2D(PadX + Wide(L.Label, L.LabelFace) + P(10.0), LeftTop);
				L.HeadAt = FVector2D(PadX, LeftTop + Row + P(2.0));
				L.DividerX = PadX + Column + Gap;
				L.DividerTop = PadY;
				L.DividerBottom = L.H - PadY;
				L.TextAt = FVector2D(TextX, RightTop);
				L.BoxAt = FVector2D(TextX, RightTop + L.TextLines.Num() * L.TextLine + P(8.0));
				L.DeedAt = FVector2D(TextX + L.Box + P(10.0), L.BoxAt.Y + (L.Box - L.DeedLine) * 0.5);
				L.QuietAt = FVector2D(Width - PadX - L.QuietSize.X, PadY);
				L.StampAt = FVector2D(Width - P(14.0) - L.StampSize.X, L.H - P(8.0) - L.StampSize.Y);
				return L;
			}
			// The note (and the slip, the same note narrower): the head row, the heading, the sentences, the deed line.
			const double PadTop = P(bSlip ? 16.0 : L.bTip ? 16.0 : 18.0), PadX = P(bSlip ? 11.0 : 16.0), PadBottom = P(bSlip ? 10.0 : L.bTip ? 13.0 : 12.0);
			const double Inner = Width - 2.0 * PadX;
			double Y = L.Top + PadTop;
			L.LabelAt = FVector2D(PadX, Y);
			if (bSlip && !L.bTip)
			{
				// (the head on two rows: STAFF NOTE, then the number with LATER at the right)
				Y += Row + P(1.0);
				L.CountAt = FVector2D(PadX, Y);
				L.QuietAt = FVector2D(Width - PadX - L.QuietSize.X, Y);
			}
			else
			{
				L.CountAt = FVector2D(PadX + Wide(L.Label, L.LabelFace) + P(10.0), Y);
				L.QuietAt = FVector2D(Width - PadX - L.QuietSize.X, Y);
			}
			Y += Row + P(bSlip ? 7.0 : 8.0);
			const FString Head = L.bTip ? FString() : Note.Head;
			if (!Head.IsEmpty())
			{
				Wrap(Head, L.HeadFace, Inner, L.HeadLines);
				L.HeadAt = FVector2D(PadX, Y);
				Y += L.HeadLines.Num() * L.HeadLine + P(4.0);
			}
			Wrap(Note.Text, L.TextFace, Inner, L.TextLines);
			L.TextAt = FVector2D(PadX, Y);
			Y += L.TextLines.Num() * L.TextLine;
			if (L.bDeed)
			{
				Y += P(10.0);
				L.RuleY = Y;
				Y += P(9.0);
				// (stamped, the deed line keeps clear of the stamp beside it; on the slip the stamp goes under it)
				Wrap(Note.Deed, L.DeedFace, Inner - L.Box - P(10.0) - (bStamped && !bSlip ? P(112.0) : 0.0), L.DeedLines);
				L.BoxAt = FVector2D(PadX, Y);
				L.DeedAt = FVector2D(PadX + L.Box + P(10.0), Y + FMath::Max(0.0, (L.Box - L.DeedLine) * 0.5));
				Y += FMath::Max(L.Box, L.DeedLines.Num() * L.DeedLine);
			}
			if (bStamped && bSlip)
			{
				Y += P(30.0);
			}
			Y += PadBottom;
			L.H = Y - L.Top;
			L.StampAt = FVector2D(Width - P(bSlip ? 8.0 : 14.0) - L.StampSize.X, Y - P(bSlip ? 6.0 : 8.0) - L.StampSize.Y);
			return L;
		}

		double WidthNow() const
		{
			const int32 Shape = Form.Get();
			return Shape == 1 ? FMath::Max(80.0, static_cast<double>(SlipWide.Get())) * K : Shape == 2 ? (LastWidth > 0.0 ? LastWidth : 1244.0) : 340.0 * K;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			FLessonNote Note;
			float Alpha = 1.0f;
			double Slide = 0.0, StampAge = -1.0;
			bool bStamped = false;
			if (!What(Note, Alpha, Slide, bStamped, StampAge))
			{
				return FVector2D(Form.Get() == 2 ? 0.0 : WidthNow(), 0.0);
			}
			const double Width = WidthNow();
			const FLaid L = Lay(Note, bStamped, Width);
			// (a strip asks for no width of its own: it lies across whatever the card gives it)
			return FVector2D(L.bStrip ? 0.0 : Width, L.Top + L.H + 6.0 * K);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (Form.Get() == 2)
			{
				LastWidth = Geometry.GetLocalSize().X;
			}
			FLessonNote Note;
			float Alpha = 1.0f;
			double Slide = 0.0, StampAge = -1.0;
			bool bStamped = false;
			if (!What(Note, Alpha, Slide, bStamped, StampAge))
			{
				return LayerId;
			}
			const FLaid L = Lay(Note, bStamped, Form.Get() == 2 ? Geometry.GetLocalSize().X : WidthNow());
			Shown = L;
			auto P = [this](double Pixels) { return Pixels * K; };
			const FLinearColor Paper100(FColor::FromHex(TEXT("EDE6D6"))), Paper300(FColor::FromHex(TEXT("CDBF9C"))), Ink900(FColor::FromHex(TEXT("2A2118"))), Ink600(FColor::FromHex(TEXT("5C5040")));
			const FLinearColor Brass700(FColor::FromHex(TEXT("8A6B3B"))), ClipBrass(FColor::FromHex(TEXT("C79D55"))), ClipLight(FColor::FromHex(TEXT("ECC884"))), GainDeep(FColor::FromHex(TEXT("3F6B36"))), SteelDeep(FColor::FromHex(TEXT("2F5068")));
			auto A = [Alpha](const FLinearColor& Ink) { return Ink.CopyWithNewOpacity(Ink.A * Alpha); };
			// (the home note slides in from its own edge: the left one from the left, the second home from the right)
			const double From = FromRight.Get() ? Slide : -Slide;
			const FVector2D O(From, 0.0);
			auto Place = [&](const FVector2D& At, const FVector2D& Size) { return Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(At + O)); };
			static const FSlateColorBrush Solid(FLinearColor::White);
			static const FSlateRoundedBoxBrush Round(FLinearColor::White, 4.0f);
			static const FSlateRoundedBoxBrush Shadow(FLinearColor::White, 4.0f);
			int32 Layer = LayerId;
			const FVector2D PaperAt(0.0, L.Top), PaperSize(L.W, L.H);
			if (!L.bStrip)
			{
				// The kit's card shadow, soft under the paper.
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(PaperAt + FVector2D(0.0, P(4.0)), PaperSize + FVector2D(0.0, P(2.0))), &Shadow, ESlateDrawEffect::None, A(FLinearColor(0.0f, 0.0f, 0.0f, 0.32f)));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(PaperAt + FVector2D(-P(2.0), P(1.0)), PaperSize + FVector2D(P(4.0), P(8.0))), &Shadow, ESlateDrawEffect::None, A(FLinearColor(0.0f, 0.0f, 0.0f, 0.14f)));
			}
			++Layer;
			{
				// The paper: paper-100 going to paper-200 at its foot, a 1 px paper-300 edge, 2 px corners.
				// (a box is drawn in the colour it is handed, not its brush's: the brush gives the corners and the edge)
				const FSlateRoundedBoxBrush Sheet(Paper100, 2.0f, A(Paper300), 1.0f);
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(PaperAt, PaperSize), &Sheet, ESlateDrawEffect::None, A(Paper100));
				const FLinearColor Foot = FLinearColor(FColor::FromHex(TEXT("E2D7BC")));
				const int32 Bands = 8;
				for (int32 Band = 0; Band < Bands; ++Band)
				{
					const double Y0 = L.Top + L.H * (0.35 + 0.65 * Band / Bands), Y1 = L.Top + L.H * (0.35 + 0.65 * (Band + 1) / Bands);
					FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(FVector2D(1.0, Y0), FVector2D(L.W - 2.0, FMath::Min(Y1, L.Top + L.H - 1.0) - Y0)), &Solid, ESlateDrawEffect::None, A(Foot.CopyWithNewOpacity((Band + 0.5f) / Bands)));
				}
			}
			++Layer;
			if (L.bClip)
			{
				// The brass clip, 44 by 15, 26 in from the left and standing 8 above the paper's top (34 wide on the slip).
				const bool bSlip = Form.Get() == 1;
				const FVector2D ClipAt(P(bSlip ? 14.0 : 26.0), 0.0), ClipSize(P(bSlip ? 34.0 : 44.0), P(15.0));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(ClipAt + FVector2D(0.0, P(2.0)), ClipSize), &Round, ESlateDrawEffect::None, A(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f)));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, Place(ClipAt, ClipSize), &Round, ESlateDrawEffect::None, A(ClipBrass));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, Place(ClipAt + FVector2D(P(2.0), P(1.0)), FVector2D(ClipSize.X - P(4.0), ClipSize.Y * 0.4)), &Round, ESlateDrawEffect::None, A(ClipLight.CopyWithNewOpacity(0.85f)));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, Place(ClipAt + FVector2D(P(4.0), P(1.0)), FVector2D(ClipSize.X - P(8.0), 1.0)), &Solid, ESlateDrawEffect::None, A(FLinearColor(1.0f, 0.96f, 0.84f, 0.75f)));
			}
			Layer += 2;
			auto Write = [&](const FString& Words, const FVector2D& At, const FSlateFontInfo& Face, const FLinearColor& Ink)
			{
				FSlateDrawElement::MakeText(OutDrawElements, Layer, Place(At, FVector2D(L.W, 60.0)), Words, Face, ESlateDrawEffect::None, A(Ink));
			};
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto Lines = [&](const TArray<FString>& Each, const FVector2D& At, double Line, const FSlateFontInfo& Face, const FLinearColor& Ink)
			{
				// (each line set in the middle of its leading, as a page sets "18 on 24")
				const double Inset = FMath::Max(0.0, (Line - Measure->GetMaxCharacterHeight(Face)) * 0.5);
				for (int32 K2 = 0; K2 < Each.Num(); ++K2)
				{
					Write(Each[K2], At + FVector2D(0.0, Inset + K2 * Line), Face, Ink);
				}
			};
			const double RowInset = FMath::Max(0.0, (P(16.0) - Measure->GetMaxCharacterHeight(L.LabelFace)) * 0.5);
			// The head row: STAFF NOTE (or the TIP tag on steel-deep), the number, and LATER or CLOSE at the right.
			if (L.bTip)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(L.LabelAt + FVector2D(0.0, -P(1.0)), L.TagSize + FVector2D(0.0, P(2.0))), &Solid, ESlateDrawEffect::None, A(SteelDeep));
				Write(L.Label, L.LabelAt + FVector2D(P(6.0), RowInset), L.LabelFace, Paper100);
			}
			else
			{
				Write(L.Label, L.LabelAt + FVector2D(0.0, RowInset), L.LabelFace, Ink900);
			}
			if (!L.Count.IsEmpty())
			{
				Write(L.Count, L.CountAt + FVector2D(0.0, RowInset), L.CountFace, Ink600);
			}
			if (!L.Quiet.IsEmpty())
			{
				// (the kit's quiet button: an underlined word over a 1 px brass-700 rule)
				Write(L.Quiet, L.QuietAt + FVector2D(0.0, RowInset), L.QuietFace, Ink600);
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Place(L.QuietAt + FVector2D(0.0, P(15.0)), FVector2D(L.QuietSize.X, 1.0)), &Solid, ESlateDrawEffect::None, A(Brass700));
			}
			if (L.HeadLines.Num() > 0)
			{
				Lines(L.HeadLines, L.HeadAt, L.HeadLine, L.HeadFace, Ink900);
			}
			Lines(L.TextLines, L.TextAt, L.TextLine, L.TextFace, Ink900);
			auto Dashes = [&](const FVector2D& From2, const FVector2D& To2, const FLinearColor& Ink)
			{
				const double Long = FVector2D::Distance(From2, To2);
				if (Long < 1.0)
				{
					return;
				}
				const FVector2D Way = (To2 - From2) / Long;
				for (double Along = 0.0; Along < Long; Along += P(7.0))
				{
					FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), { From2 + O + Way * Along, From2 + O + Way * FMath::Min(Along + P(4.0), Long) }, ESlateDrawEffect::None, A(Ink), true, 1.0f);
				}
			};
			if (L.DividerX >= 0.0)
			{
				Dashes(FVector2D(L.DividerX, L.DividerTop), FVector2D(L.DividerX, L.DividerBottom), Ink600.CopyWithNewOpacity(0.45f));
			}
			if (L.bDeed)
			{
				if (L.RuleY >= 0.0)
				{
					Dashes(FVector2D(P(L.bStrip ? 0.0 : (Form.Get() == 1 ? 11.0 : 16.0)), L.RuleY), FVector2D(L.W - P(Form.Get() == 1 ? 11.0 : 16.0), L.RuleY), Ink600.CopyWithNewOpacity(0.6f));
				}
				// The deed's box: 1.5 px of ink-600, gain-deep and ticked once the deed is done.
				const FLinearColor BoxInk = bStamped ? GainDeep : Ink600;
				const FVector2D B0 = L.BoxAt + O, B1 = B0 + FVector2D(L.Box, L.Box);
				FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), { B0, FVector2D(B1.X, B0.Y), B1, FVector2D(B0.X, B1.Y), B0 }, ESlateDrawEffect::None, A(BoxInk), true, 1.5f);
				if (bStamped)
				{
					const double U = L.Box / 16.0;
					FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), { B0 + FVector2D(3.0, 8.4) * U, B0 + FVector2D(6.2, 11.5) * U, B0 + FVector2D(13.0, 4.6) * U }, ESlateDrawEffect::None, A(GainDeep), true, 2.0f);
				}
				Lines(L.DeedLines, L.DeedAt, L.DeedLine, L.DeedFace, Ink900);
			}
			if (bStamped && StampAge >= 0.0)
			{
				// DONE: the kit's stamp in its won colour, turned 6 degrees, landing with a thump (220 ms).
				const double T = FMath::Clamp(StampAge / 0.22, 0.0, 1.0);
				const float Grow = static_cast<float>(1.0 + 0.45 * FMath::Pow(1.0 - T, 2.0));
				const float StampAlpha = static_cast<float>(FMath::Clamp(T * 2.5, 0.0, 1.0)) * Alpha;
				const float Turn = FMath::DegreesToRadians(-6.0f), Cos = FMath::Cos(Turn) * Grow, Sin = FMath::Sin(Turn) * Grow;
				const FGeometry Stamp = Geometry.MakeChild(L.StampSize, FSlateLayoutTransform(L.StampAt + O), FSlateRenderTransform(FMatrix2x2(Cos, Sin, -Sin, Cos)), FVector2D(0.5, 0.5));
				const FSlateRoundedBoxBrush Ring(FLinearColor::Transparent, 3.0f, GainDeep.CopyWithNewOpacity(0.92f * StampAlpha), static_cast<float>(P(3.0)));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, Stamp.ToPaintGeometry(), &Ring, ESlateDrawEffect::None, FLinearColor::Transparent);
				const FVector2D Size = Measure->Measure(TEXT("DONE"), L.StampFace);
				FSlateDrawElement::MakeText(OutDrawElements, Layer + 1, Stamp.ToPaintGeometry(Size, FSlateLayoutTransform((L.StampSize - Size) * 0.5)), FString(TEXT("DONE")), L.StampFace, ESlateDrawEffect::None, GainDeep.CopyWithNewOpacity(0.92f * StampAlpha));
			}
			return Layer + 2;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (!Controller.IsValid() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Handled();
			}
			ACampaignPlayerController* C = Controller.Get();
			if (Form.Get() == 2 && C->IsLessonLaid())
			{
				return FReply::Unhandled(); // (lesson 1 lies on the card: a click there is the card's)
			}
			if (C->IsLessonStamped())
			{
				C->PressLesson(TEXT("send")); // (ruling 147: a click sends a stamped note at once)
				return FReply::Handled();
			}
			const FVector2D Here = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
			if (!Shown.Quiet.IsEmpty() && C->HasLessonNote())
			{
				const FBox2D Quiet(Shown.QuietAt - FVector2D(6.0, 6.0), Shown.QuietAt + Shown.QuietSize + FVector2D(6.0, 6.0));
				if (Quiet.IsInside(Here))
				{
					const FLessonNote& Note = C->GetLessonNote();
					C->PressLesson(Note.Kind == TEXT("lesson") ? TEXT("later") : Note.Kind == TEXT("tip") ? TEXT("tip") : TEXT("close"));
				}
			}
			return FReply::Handled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override
		{
			if (!Controller.IsValid() || (Form.Get() == 2 && Controller->IsLessonLaid()))
			{
				return FCursorReply::Unhandled();
			}
			if (Controller->IsLessonStamped())
			{
				return FCursorReply::Cursor(EMouseCursor::Hand);
			}
			const FVector2D Here = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
			const FBox2D Quiet(Shown.QuietAt - FVector2D(6.0, 6.0), Shown.QuietAt + Shown.QuietSize + FVector2D(6.0, 6.0));
			return !Shown.Quiet.IsEmpty() && Quiet.IsInside(Here) ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
		TAttribute<int32> Form;
		TAttribute<float> SlipWide;
		TAttribute<bool> FromRight;
		float K = 0.9f;
		mutable double LastWidth = 0.0;
		mutable FLaid Shown;
		mutable FString LaidKey;
		mutable FLaid LaidKept;
	};

	/**
	 * (Rules 84.3.3; the art director's first-hour brief, section 3, and the kit's tc-pointer.) The staff note's pointer
	 * on a button or a line of a card: four brass corner clamps, like a draughtsman's corner marks, 9 px clear of the
	 * thing and breathing out to 13 and back in two seconds. Each arm is 3 px of brass-300 inside 6 px of desk-950 at
	 * 90%, with a 5 px glow of brass-300; 12 long, or 16 round a thing 60 px or more each way. It takes no clicks.
	 * (On the map the same clamps lie on the ground under the pieces, and do not breathe: ACampaignMapActor::ShowPointer.)
	 */
	class SLessonPointer : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLessonPointer) {}
			SLATE_ATTRIBUTE(FBox2D, Frame) // the thing framed, in the widget's own measure; invalid for nothing
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Frame = Args._Frame;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D::ZeroVector;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FBox2D Thing = Frame.Get();
			if (!Thing.bIsValid)
			{
				return LayerId;
			}
			const double K = 0.9; // (the brief's pixels are the 1600 by 1000 frame's)
			const double Breath = 0.5 - 0.5 * FMath::Cos(FPlatformTime::Seconds() * PI); // (out and back in two seconds)
			const double Clear = (9.0 + 4.0 * Breath) * K;
			const FVector2D Size = Thing.GetSize();
			const double Arm = (Size.X >= 60.0 * K && Size.Y >= 60.0 * K ? 16.0 : 12.0) * K;
			const FVector2D A = Thing.Min - FVector2D(Clear, Clear), B = Thing.Max + FVector2D(Clear, Clear);
			static const FSlateColorBrush Solid(FLinearColor::White);
			auto Rect = [&](int32 Layer, double X0, double Y0, double X1, double Y1, const FLinearColor& Ink)
			{
				const double Left = FMath::Min(X0, X1), Top = FMath::Min(Y0, Y1);
				const FVector2D Box(FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0));
				if (Box.X > 0.0 && Box.Y > 0.0)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(Box, FSlateLayoutTransform(FVector2D(Left, Top))), &Solid, ESlateDrawEffect::None, Ink);
				}
			};
			// The four clamps, each an arm along the frame's edge and one down its side, Inset in from the frame's corner,
			// Thick across and Long along (the arm along the edge takes the corner's square: no square is laid twice).
			auto Clamps = [&](int32 Layer, double Inset, double Thick, double Long, const FLinearColor& Ink)
			{
				const FVector2D Corners[4] = { A, FVector2D(B.X, A.Y), FVector2D(A.X, B.Y), B };
				const FVector2D Ways[4] = { FVector2D(1.0, 1.0), FVector2D(-1.0, 1.0), FVector2D(1.0, -1.0), FVector2D(-1.0, -1.0) };
				for (int32 Each = 0; Each < 4; ++Each)
				{
					const FVector2D C = Corners[Each], W = Ways[Each];
					Rect(Layer, C.X + W.X * Inset, C.Y + W.Y * Inset, C.X + W.X * (Inset + Long), C.Y + W.Y * (Inset + Thick), Ink);
					Rect(Layer, C.X + W.X * Inset, C.Y + W.Y * (Inset + Thick), C.X + W.X * (Inset + Thick), C.Y + W.Y * (Inset + Long), Ink);
				}
			};
			const FLinearColor Brass300(FColor::FromHex(TEXT("E0B872"))), Desk950(FColor::FromHex(TEXT("0E0B08")));
			// The glow: the dark clamps' shape spread 5 px in brass, fading as it goes (the kit's drop-shadow at 75%).
			const double Spreads[3] = { 4.5, 3.0, 1.5 };
			const float Glows[3] = { 0.06f, 0.12f, 0.2f };
			for (int32 Step = 0; Step < 3; ++Step)
			{
				const double E = Spreads[Step] * K;
				Clamps(LayerId, -E, 6.0 * K + 2.0 * E, Arm + 6.0 * K + 2.0 * E, Brass300.CopyWithNewOpacity(Glows[Step]));
			}
			Clamps(LayerId + 1, 0.0, 6.0 * K, Arm + 6.0 * K, Desk950.CopyWithNewOpacity(0.9f));
			Clamps(LayerId + 2, 1.5 * K, 3.0 * K, Arm + 1.5 * K, Brass300);
			return LayerId + 3;
		}

	private:
		TAttribute<FBox2D> Frame;
	};

	/**
	 * (The kit's shadow-lit) The lamp's light round a box: 18 px of brass-300 fading out from its edge. The box's own
	 * brass-300 rim is its 1 px ring. Drawn before the box, which covers what falls inside it.
	 */
	void PaintLampGlow(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geometry, const FVector2D& At, const FVector2D& Size, double Radius, double K)
	{
		const FLinearColor Brass300(FColor::FromHex(TEXT("E0B872")));
		const double Spreads[4] = { 15.0, 10.0, 5.5, 2.0 };
		const float Glows[4] = { 0.035f, 0.06f, 0.09f, 0.14f };
		for (int32 Step = 0; Step < 4; ++Step)
		{
			const double E = Spreads[Step] * K;
			const FSlateRoundedBoxBrush Halo(FLinearColor::White, static_cast<float>(Radius + E));
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(Size + FVector2D(2.0 * E, 2.0 * E), FSlateLayoutTransform(At - FVector2D(E, E))), &Halo,
				ESlateDrawEffect::None, Brass300.CopyWithNewOpacity(Glows[Step]));
		}
	}

	/** (Ruling 141) The lamp on the door's FIRST COMMAND until the ten lessons are done: its light, round the button over it. */
	class SLampGlow : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLampGlow) : _Lit(true) {}
			SLATE_ATTRIBUTE(bool, Lit)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Lit = Args._Lit;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D::ZeroVector;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (Lit.Get())
			{
				PaintLampGlow(OutDrawElements, LayerId, Geometry, FVector2D::ZeroVector, Geometry.GetLocalSize(), 5.0, 0.9);
			}
			return LayerId + 1;
		}

	private:
		TAttribute<bool> Lit;
	};

	/**
	 * (Rules 84.3.3; the art director's look brief, section 6; the kit's tc-panel, tc-step, tc-seg and tc-close.) The
	 * Guide's first page: a kit panel 640 wide, THE GUIDE · FIRST COMMAND over "The ten lessons" and one line of state,
	 * then the ten in rows 38 high under a small label for each month. Done: a brass-700 edge, a brass-500 disc with a
	 * tick, and SHOW ME AGAIN at the right. Waiting: a brass-300 edge, its number in a ring, WAITING FOR YOU; a click
	 * brings its note back, and only the first waiting row carries the lamp's glow. To come: a dashed edge, faint, "when
	 * its moment comes". At the foot the setting with the kit's joined ON and OFF, and the way on to the typed page of
	 * rules, the Guide's second (ruling 147). The kit's round stud closes it, as Esc and H do. The brief's measures are
	 * pixels of the 1600 by 1000 frame: 0.9 of them on the table's frame of 900 high.
	 */
	class SLessonGuide : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLessonGuide) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Controller = Args._Controller;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScale) const override
		{
			const FLaid& L = Laid(LayoutScale);
			return L.bValid ? FVector2D(L.W, L.H) : FVector2D::ZeroVector;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FLaid& L = Laid(Geometry.Scale);
			if (!L.bValid)
			{
				return LayerId;
			}
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto P = [](double Pixels) { return Pixels * K; };
			const FLinearColor Desk900(FColor::FromHex(TEXT("17120D"))), Desk800(FColor::FromHex(TEXT("211A13"))), Desk700(FColor::FromHex(TEXT("2D241A"))), Desk600(FColor::FromHex(TEXT("3D3125")));
			const FLinearColor Line(FColor::FromHex(TEXT("4B3D2D"))), Brass700(FColor::FromHex(TEXT("8A6B3B"))), Brass500(FColor::FromHex(TEXT("B08D57"))), Brass300(FColor::FromHex(TEXT("E0B872")));
			const FLinearColor Brass100(FColor::FromHex(TEXT("F4DEA8"))), OnBrass(FColor::FromHex(TEXT("1E1608"))), Paper100(FColor::FromHex(TEXT("EDE6D6"))), Paper200(FColor::FromHex(TEXT("E2D7BC")));
			const FLinearColor TextMuted(FColor::FromHex(TEXT("A89F8D"))), TextFaint(FColor::FromHex(TEXT("8F8573"))), SegLight(FColor::FromHex(TEXT("ECC884")));
			static const FSlateColorBrush Solid(FLinearColor::White);
			auto Fill = [&](int32 Layer, const FVector2D& At, const FVector2D& Size, const FLinearColor& Ink)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(At)), &Solid, ESlateDrawEffect::None, Ink);
			};
			// (a rounded plate: its fill, and an edge of its own colour; a clear fill leaves the edge alone)
			auto Plate = [&](int32 Layer, const FVector2D& At, const FVector2D& Size, double Radius, const FLinearColor& Ink, const FLinearColor& Edge, double Thick)
			{
				const FSlateRoundedBoxBrush Brush(FLinearColor::White, static_cast<float>(Radius), Edge, static_cast<float>(Thick));
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(At)), &Brush, ESlateDrawEffect::None, Ink);
			};
			auto Stroke = [&](int32 Layer, const TArray<FVector2D>& Points, const FLinearColor& Ink, double Thick)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Ink, true, static_cast<float>(Thick));
			};
			auto Dashes = [&](int32 Layer, const FVector2D& From, const FVector2D& To, const FLinearColor& Ink)
			{
				const double Long = FVector2D::Distance(From, To);
				const FVector2D Way = Long > 0.0 ? (To - From) / Long : FVector2D::ZeroVector;
				for (double Along = 0.0; Along < Long; Along += P(7.0))
				{
					Stroke(Layer, { From + Way * Along, From + Way * FMath::Min(Along + P(4.0), Long) }, Ink, 1.0);
				}
			};
			auto Write = [&](int32 Layer, const FString& Words, const FVector2D& At, const FSlateFontInfo& Face, const FLinearColor& Ink)
			{
				FSlateDrawElement::MakeText(OutDrawElements, Layer, Geometry.ToPaintGeometry(Measure->Measure(Words, Face), FSlateLayoutTransform(At)), Words, Face, ESlateDrawEffect::None, Ink);
			};
			// (words set in the middle of a band: from its left, or ending at its right)
			auto WriteIn = [&](int32 Layer, const FString& Words, double X, double Top, double High, const FSlateFontInfo& Face, const FLinearColor& Ink, bool bFromRight)
			{
				const FVector2D Size = Measure->Measure(Words, Face);
				Write(Layer, Words, FVector2D(bFromRight ? X - Size.X : X, Top + (High - Size.Y) * 0.5), Face, Ink);
				return Size;
			};
			auto Hovered = [&L, this](int32 Spot) { return Spot != INDEX_NONE && Spot == HoverSpot && L.Spots.IsValidIndex(Spot); };

			// ---- The panel: opaque desk, a brass-700 edge, and its four clamps.
			Plate(LayerId, FVector2D::ZeroVector, FVector2D(L.W, L.H), P(4.0), Desk900, Brass700, 1.0);
			for (const FVector2D& Corner : { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(1.0, 1.0) })
			{
				const double CX = P(5.0) + Corner.X * (L.W - P(10.0)), CY = P(5.0) + Corner.Y * (L.H - P(10.0));
				const double DX = Corner.X > 0.5 ? -P(14.0) : P(14.0), DY = Corner.Y > 0.5 ? -P(14.0) : P(14.0);
				Stroke(LayerId + 1, { FVector2D(CX + DX, CY), FVector2D(CX, CY), FVector2D(CX, CY + DY) }, Brass500, P(2.0));
			}
			const int32 Layer = LayerId + 2;

			// ---- The head, and the round stud that closes it.
			Write(Layer, L.Label, FVector2D(P(Pad), P(L.LabelY)), L.LabelFace, Brass500);
			Write(Layer, L.Head, FVector2D(P(Pad), P(L.HeadY)), L.HeadFace, Paper100);
			Write(Layer, L.State, FVector2D(P(Pad), P(L.StateY)), L.StateFace, TextMuted);
			{
				const FSpot& Close = L.Spots[L.CloseSpot];
				const bool bHot = Hovered(L.CloseSpot);
				Plate(Layer, Close.Box.Min, Close.Box.GetSize(), Close.Box.GetSize().X * 0.5, bHot ? Desk600 : Desk700, bHot ? Brass300 : Brass700, 1.0);
				const FVector2D C = Close.Box.GetCenter();
				const double Arm = P(5.5);
				Stroke(Layer + 1, { C + FVector2D(-Arm, -Arm), C + FVector2D(Arm, Arm) }, bHot ? Brass100 : Paper100, P(1.6));
				Stroke(Layer + 1, { C + FVector2D(Arm, -Arm), C + FVector2D(-Arm, Arm) }, bHot ? Brass100 : Paper100, P(1.6));
			}

			// ---- The months, and the ten in their rows.
			for (const FLabelLaid& Label : L.Labels)
			{
				Write(Layer, Label.Words, FVector2D(P(Pad), P(Label.Y)), L.LabelFace, Brass500);
			}
			const double RowX = P(Pad), RowW = L.W - 2.0 * P(Pad);
			for (const FRowLaid& Each : L.Rows)
			{
				const double RowH = P(Each.High);
				const FLessonRow& Row = Each.Row;
				const FVector2D At(RowX, P(Each.Y)), Size(RowW, RowH);
				const bool bDone = Row.State == TEXT("done"), bNow = Row.State == TEXT("now");
				const bool bHot = Hovered(Each.Spot);
				if (bNow && Row.bLit)
				{
					PaintLampGlow(OutDrawElements, Layer, Geometry, At, Size, P(4.0), K);
				}
				if (bDone)
				{
					Plate(Layer + 1, At, Size, P(4.0), Desk800, Brass700, 1.0);
				}
				else if (bNow)
				{
					Plate(Layer + 1, At, Size, P(4.0), Desk700, bHot ? Brass100 : Brass300, 1.0);
				}
				else
				{
					// (to come: no ground of its own, and a dashed edge)
					const FVector2D A = At + FVector2D(0.5, 0.5), B = At + Size - FVector2D(0.5, 0.5);
					Dashes(Layer + 1, A, FVector2D(B.X, A.Y), Line);
					Dashes(Layer + 1, FVector2D(B.X, A.Y), B, Line);
					Dashes(Layer + 1, B, FVector2D(A.X, B.Y), Line);
					Dashes(Layer + 1, FVector2D(A.X, B.Y), A, Line);
				}
				// Its disc: 22 across, 8 in from the left.
				const double Disc = P(22.0);
				const FVector2D DiscAt(At.X + P(8.0), At.Y + (RowH - Disc) * 0.5);
				if (bDone)
				{
					Plate(Layer + 2, DiscAt, FVector2D(Disc, Disc), Disc * 0.5, Brass500, Brass500, P(1.5));
					const double U = P(13.0) / 16.0;
					const FVector2D O = DiscAt + FVector2D(Disc, Disc) * 0.5 - FVector2D(P(6.5), P(6.5));
					Stroke(Layer + 3, { O + FVector2D(3.0, 8.4) * U, O + FVector2D(6.2, 11.5) * U, O + FVector2D(13.0, 4.6) * U }, OnBrass, P(2.0));
				}
				else
				{
					const FLinearColor RingInk = bNow ? Brass300 : TextFaint;
					Plate(Layer + 2, DiscAt, FVector2D(Disc, Disc), Disc * 0.5, FLinearColor::Transparent, RingInk, P(1.5));
					const FString Number = FString::FromInt(Row.N);
					const FVector2D NumberSize = Measure->Measure(Number, L.NumberFace);
					Write(Layer + 3, Number, DiscAt + (FVector2D(Disc, Disc) - NumberSize) * 0.5, L.NumberFace, RingInk);
				}
				// Its name, and what stands at its right. (The name's lines stand together in the row's middle.)
				const double NameX = DiscAt.X + Disc + P(12.0), RightX = At.X + RowW - P(12.0);
				const double Step = P(L.NameStep), NamesTop = At.Y + (RowH - Step * Each.Names.Num()) * 0.5;
				auto WriteName = [&](const FSlateFontInfo& WithFace, const FLinearColor& WithInk)
				{
					for (int32 Nth = 0; Nth < Each.Names.Num(); ++Nth)
					{
						WriteIn(Layer + 2, Each.Names[Nth], NameX, NamesTop + Nth * Step, Step, WithFace, WithInk, false);
					}
				};
				if (bDone)
				{
					// SHOW ME AGAIN: the kit's quiet word, underlined 2 under its letters.
					const bool bAgainHot = Hovered(Each.Spot);
					const FVector2D Word = WriteIn(Layer + 2, L.Again, RightX, At.Y, RowH - P(2.0), L.QuietFace, bAgainHot ? Brass100 : Brass300, true);
					Fill(Layer + 2, FVector2D(RightX - Word.X, At.Y + (RowH - P(2.0) + Word.Y) * 0.5 + P(1.0)), FVector2D(Word.X, 1.0), bAgainHot ? Brass100 : Brass700);
					WriteName(L.NameFace, Paper200);
				}
				else if (bNow)
				{
					WriteIn(Layer + 2, L.Waiting, RightX, At.Y, RowH, L.WaitFace, Brass100, true);
					WriteName(L.NameFace, Brass100);
				}
				else
				{
					WriteIn(Layer + 2, L.Later, RightX, At.Y, RowH, L.LaterFace, TextFaint, true);
					WriteName(L.LaterNameFace, TextMuted);
				}
			}

			// ---- The foot: a rule, the setting and its joined ON and OFF, and the way on to the rules.
			Fill(Layer, FVector2D(P(Pad), P(L.RuleY)), FVector2D(L.W - 2.0 * P(Pad), 1.0), Line);
			WriteIn(Layer, L.Setting, P(Pad), P(L.SettingY), P(36.0), L.SettingFace, Paper100, false);
			{
				const FSpot& On = L.Spots[L.OnSpot];
				const FSpot& Off = L.Spots[L.OffSpot];
				const FBox2D Seg(On.Box.Min, Off.Box.Max);
				Plate(Layer, Seg.Min, Seg.GetSize(), P(4.0), Desk800, Brass700, 1.0);
				for (const int32 Spot : { L.OnSpot, L.OffSpot })
				{
					const FSpot& Part = L.Spots[Spot];
					const bool bChosen = (Spot == L.OnSpot) == L.bOn;
					const FVector2D In = Part.Box.Min + FVector2D(1.0, 1.0), InSize = Part.Box.GetSize() - FVector2D(2.0, 2.0);
					if (bChosen)
					{
						// (the kit's lit half: brass, a little lighter at the top)
						Fill(Layer + 1, In, InSize, Brass300);
						Fill(Layer + 1, In, FVector2D(InSize.X, InSize.Y * 0.5), SegLight.CopyWithNewOpacity(0.55f));
					}
					else if (Hovered(Spot))
					{
						Fill(Layer + 1, In, InSize, Desk700);
					}
					const FVector2D WordSize = Measure->Measure(Part.Act == TEXT("on") ? L.OnWord : L.OffWord, L.SegFace);
					Write(Layer + 2, Part.Act == TEXT("on") ? L.OnWord : L.OffWord, Part.Box.GetCenter() - WordSize * 0.5, L.SegFace, bChosen ? OnBrass : Hovered(Spot) ? Paper100 : TextMuted);
				}
				Fill(Layer + 2, FVector2D(Off.Box.Min.X, Seg.Min.Y + 1.0), FVector2D(1.0, Seg.GetSize().Y - 2.0), Line);
			}
			{
				// "How a month works", the kit's quiet word: the way on to the typed page of rules.
				const bool bHot = Hovered(L.RulesSpot);
				const FVector2D Size = WriteIn(Layer, L.RulesWord, P(Pad), P(L.RulesY), P(18.0), L.QuietFace, bHot ? Brass100 : Brass300, false);
				Fill(Layer, FVector2D(P(Pad), P(L.RulesY) + (P(18.0) + Size.Y) * 0.5 + P(2.0)), FVector2D(Size.X, 1.0), bHot ? Brass100 : Brass700);
			}
			return Layer + 4;
		}

		virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			HoverSpot = SpotAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			return FReply::Unhandled();
		}

		virtual void OnMouseLeave(const FPointerEvent& Event) override
		{
			HoverSpot = INDEX_NONE;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			if (!Controller.IsValid() || Event.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Handled();
			}
			const FLaid& L = Laid();
			const int32 Spot = SpotAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
			if (!L.Spots.IsValidIndex(Spot))
			{
				return FReply::Handled(); // (the page takes its clicks: nothing behind it is pressed)
			}
			const FString Act = L.Spots[Spot].Act;
			ACampaignPlayerController* C = Controller.Get();
			if (Act == TEXT("close"))
			{
				C->CloseLessonGuide();
			}
			else if (Act == TEXT("rules"))
			{
				C->OpenHowToPlay();
			}
			else if ((Act == TEXT("on") && L.bOn) || (Act == TEXT("off") && !L.bOn))
			{
				// (the half already chosen: nothing to change)
			}
			else
			{
				C->PressGuide(Act);
			}
			return FReply::Handled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override
		{
			return Laid().Spots.IsValidIndex(SpotAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()))) ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
		}

	private:
		static constexpr double K = 0.9;
		static constexpr double Pad = 28.0;

		/** A thing on the page that takes a click (in the widget's own measure), and what it does. */
		struct FSpot
		{
			FBox2D Box = FBox2D(ForceInit);
			FString Act;
		};
		struct FRowLaid
		{
			FLessonRow Row;
			TArray<FString> Names; // (its name, on as many lines as it takes: never cut short)
			double Y = 0.0;
			double High = 38.0; // (38, and a line more for each further line of its name)
			int32 Spot = INDEX_NONE;
		};
		struct FLabelLaid
		{
			FString Words;
			double Y = 0.0;
		};
		/** The page set out: in the brief's pixels down the page (P() turns them into the widget's), its width already turned. */
		struct FLaid
		{
			bool bValid = false;
			int32 Revision = INDEX_NONE;
			double Scale = 1.0; // (the scale its names were folded at)
			double W = 0.0, H = 0.0, NameStep = 0.0;
			FString Label, Head, State, Setting;
			FString Again = TEXT("SHOW ME AGAIN"), Waiting = TEXT("WAITING FOR YOU"), Later = TEXT("when its moment comes");
			FString OnWord = TEXT("ON"), OffWord = TEXT("OFF"), RulesWord = TEXT("HOW A MONTH WORKS ›");
			bool bOn = true;
			double LabelY = 0.0, HeadY = 0.0, StateY = 0.0, RuleY = 0.0, SettingY = 0.0, RulesY = 0.0;
			TArray<FLabelLaid> Labels;
			TArray<FRowLaid> Rows;
			TArray<FSpot> Spots;
			int32 CloseSpot = INDEX_NONE, OnSpot = INDEX_NONE, OffSpot = INDEX_NONE, RulesSpot = INDEX_NONE;
			FSlateFontInfo LabelFace, HeadFace, StateFace, NameFace, LaterNameFace, NumberFace, QuietFace, WaitFace, LaterFace, SettingFace, SegFace;
		};

		/**
		 * The page as the controller has it now, set out again only when it changes, or when it is drawn at another
		 * scale (its names fold at the scale they are painted at; Scale 0 keeps the last).
		 */
		const FLaid& Laid(double Scale = 0.0) const
		{
			if (!Controller.IsValid() || !Controller->IsLessonGuideOpen())
			{
				return Kept; // (as last set out: it is not drawn while closed)
			}
			const int32 Revision = Controller->GetLessonGuideRevision();
			const double At = Scale > 0.01 ? Scale : Kept.bValid ? Kept.Scale : 1.0;
			const bool bChanged = !Kept.bValid || Kept.Revision != Revision;
			if (bChanged || !FMath::IsNearlyEqual(Kept.Scale, At, 0.001))
			{
				Kept = Lay(Controller->GetLessonGuide(), At);
				Kept.Revision = Revision;
				Kept.Scale = At;
				if (bChanged)
				{
					HoverSpot = INDEX_NONE;
				}
			}
			return Kept;
		}

		static FLaid Lay(const FLessonGuide& Guide, double Scale)
		{
			FLaid L;
			if (!Guide.bValid)
			{
				return L;
			}
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto P = [](double Pixels) { return Pixels * K; };
			auto Pt = [](double Pixels) { return static_cast<float>(Pixels * K * 0.75); }; // (a face is asked for in points)
			auto Wide = [&Measure](const FString& Words, const FSlateFontInfo& Face) { return static_cast<double>(Measure->Measure(Words, Face).X); };
			L.bValid = true;
			L.W = P(640.0);
			L.Label = Guide.Label.ToUpper();
			L.Head = Guide.Head;
			L.State = Guide.State;
			L.Setting = Guide.Setting;
			L.bOn = Guide.bOn;
			L.LabelFace = Typed(Pt(11.0), true, 140);
			L.HeadFace = Serif(TEXT("Bold"), Pt(24.0));
			L.StateFace = Serif(TEXT("Italic"), Pt(15.0));
			L.NameFace = Serif(TEXT("Bold"), Pt(14.0));
			L.LaterNameFace = Serif(TEXT("Regular"), Pt(14.0));
			L.NumberFace = Typed(Pt(11.0), true);
			L.QuietFace = Typed(Pt(11.0), true, 60);
			L.WaitFace = Typed(Pt(11.0), true, 100);
			L.LaterFace = Typed(Pt(12.0));
			L.SettingFace = Typed(Pt(13.0));
			L.SegFace = Typed(Pt(13.0), true, 80);
			// (a name's further line: the step of its face, in the brief's pixels)
			L.NameStep = static_cast<double>(Measure->GetMaxCharacterHeight(L.NameFace)) / K;
			// The close stud, 32 across, at the top right.
			FSpot& Close = L.Spots.AddDefaulted_GetRef();
			Close.Box = FBox2D(FVector2D(L.W - P(19.0 + 32.0), P(18.0)), FVector2D(L.W - P(19.0), P(50.0)));
			Close.Act = TEXT("close");
			L.CloseSpot = 0;
			double Y = 26.0;
			L.LabelY = Y;
			Y += 14.0 + 6.0;
			L.HeadY = Y;
			Y += 30.0 + 2.0;
			L.StateY = Y;
			Y += 22.0;
			const double RowW = 640.0 - 2.0 * Pad;
			for (int32 Group = 0; Group < Guide.Groups.Num(); ++Group)
			{
				Y += 14.0;
				const FString Words = Guide.GroupLabels.IsValidIndex(Group) ? Guide.GroupLabels[Group].ToUpper() : FString();
				if (!Words.IsEmpty())
				{
					FLabelLaid& Label = L.Labels.AddDefaulted_GetRef();
					Label.Words = Words;
					Label.Y = Y;
					Y += 14.0 + 8.0;
				}
				for (int32 Index = 0; Index < Guide.Groups[Group].Num(); ++Index)
				{
					if (Index > 0)
					{
						Y += 6.0;
					}
					FRowLaid& Each = L.Rows.AddDefaulted_GetRef();
					Each.Row = Guide.Groups[Group][Index];
					Each.Y = Y;
					const bool bDone = Each.Row.State == TEXT("done"), bNow = Each.Row.State == TEXT("now");
					const FString& Right = bDone ? L.Again : bNow ? L.Waiting : L.Later;
					const FSlateFontInfo& RightFace = bDone ? L.QuietFace : bNow ? L.WaitFace : L.LaterFace;
					const FSlateFontInfo& NameFace = bDone || bNow ? L.NameFace : L.LaterNameFace;
					// (the name keeps clear of what stands at the right; where it is longer it goes on to a second line
					// and the row grows, never cut short with three dots: the UI director, 08:57 on the review of build
					// 1628. It folds at the scale it is painted at.)
					const double Room = P(RowW - 8.0 - 22.0 - 12.0 - 12.0 - 16.0) - PaintWide(Right, RightFace, Scale);
					FoldWords(Each.Row.Head, NameFace, Room, Each.Names, false, Scale);
					if (Each.Names.Num() == 0)
					{
						Each.Names.Add(FString());
					}
					Each.High = 38.0 + (Each.Names.Num() - 1) * L.NameStep;
					const double Middle = Y + Each.High * 0.5;
					if (bDone)
					{
						// SHOW ME AGAIN, a quiet word at the right: only the word takes the click.
						const double RightEdge = P(Pad + RowW - 12.0), WordW = Wide(L.Again, L.QuietFace);
						FSpot& Again = L.Spots.AddDefaulted_GetRef();
						Again.Box = FBox2D(FVector2D(RightEdge - WordW - P(6.0), P(Middle - 13.0)), FVector2D(RightEdge + P(6.0), P(Middle + 13.0)));
						Again.Act = TEXT("again:") + Each.Row.Id;
						Each.Spot = L.Spots.Num() - 1;
					}
					else if (bNow)
					{
						// A row waiting for him: the whole row brings its note back.
						FSpot& Back = L.Spots.AddDefaulted_GetRef();
						Back.Box = FBox2D(FVector2D(P(Pad), P(Y)), FVector2D(P(Pad + RowW), P(Y + Each.High)));
						Back.Act = TEXT("back:") + Each.Row.Id;
						Each.Spot = L.Spots.Num() - 1;
					}
					Y += Each.High;
				}
			}
			Y += 18.0;
			L.RuleY = Y;
			Y += 1.0 + 14.0;
			L.SettingY = Y;
			{
				// ON and OFF, joined, at the right: each its word and 16 either side, 36 high with the edge.
				const double OnW = Wide(L.OnWord, L.SegFace) + P(32.0), OffW = Wide(L.OffWord, L.SegFace) + P(32.0);
				const double Right = P(640.0 - Pad), Top = P(Y);
				FSpot& Off = L.Spots.AddDefaulted_GetRef();
				Off.Box = FBox2D(FVector2D(Right - OffW, Top), FVector2D(Right, Top + P(36.0)));
				Off.Act = TEXT("off");
				L.OffSpot = L.Spots.Num() - 1;
				FSpot& On = L.Spots.AddDefaulted_GetRef();
				On.Box = FBox2D(FVector2D(Right - OffW - OnW, Top), FVector2D(Right - OffW, Top + P(36.0)));
				On.Act = TEXT("on");
				L.OnSpot = L.Spots.Num() - 1;
			}
			Y += 36.0 + 10.0;
			L.RulesY = Y;
			{
				FSpot& Rules = L.Spots.AddDefaulted_GetRef();
				Rules.Box = FBox2D(FVector2D(P(Pad) - P(4.0), P(Y) - P(3.0)), FVector2D(P(Pad) + Wide(L.RulesWord, L.QuietFace) + P(4.0), P(Y + 21.0)));
				Rules.Act = TEXT("rules");
				L.RulesSpot = L.Spots.Num() - 1;
			}
			Y += 18.0 + 24.0;
			L.H = P(Y);
			return L;
		}

		int32 SpotAt(const FVector2D& Here) const
		{
			const FLaid& L = Laid();
			for (int32 Index = 0; Index < L.Spots.Num(); ++Index)
			{
				if (L.Spots[Index].Box.IsInside(Here))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		TWeakObjectPtr<ACampaignPlayerController> Controller;
		mutable FLaid Kept;
		mutable int32 HoverSpot = INDEX_NONE;
	};

	class SCampaignOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCampaignOverlay) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ACampaignPlayerController>, Controller)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Controller = InArgs._Controller;
			{
				// The tips that come up under the pointer are the engine's own: they are given the frame's typed face and
				// a slip of its paper, once, before anything that has one is made.
				static bool bDressed = false;
				if (!bDressed)
				{
					bDressed = true;
					FSlateStyleSet* Style = static_cast<FSlateStyleSet*>(const_cast<ISlateStyle*>(&FCoreStyle::Get()));
					Style->Set("ToolTip.Font", Typed(9.5f));
					Style->Set("ToolTip.LargerFont", Typed(10.5f));
					Style->Set("ToolTip.BrightBackground", new FSlateRoundedBoxBrush(Paper, 3.0f, FLinearColor(0.05f, 0.035f, 0.02f, 0.9f), 1.0f));
					Style->Set("ToolTip.Background", new FSlateRoundedBoxBrush(Paper, 3.0f, FLinearColor(0.05f, 0.035f, 0.02f, 0.9f), 1.0f));
				}
			}
			// (the panels are brown leather with a brass edge; the date is a desk calendar; the dispatches are typed)
			PanelBrush = FSlateRoundedBoxBrush(Leather, 6.0f, Brass.CopyWithNewOpacity(0.85f), 1.5f);
			BigCardBrush = FSlateRoundedBoxBrush(Leather.CopyWithNewOpacity(0.975f), 6.0f, Brass.CopyWithNewOpacity(0.85f), 1.5f);
			PaperBrush = FSlateRoundedBoxBrush(Paper, 5.0f, FLinearColor(0.05f, 0.035f, 0.02f, 0.9f), 1.0f);
			BindingBrush = FSlateRoundedBoxBrush(Binding, FVector4(4.0f, 4.0f, 0.0f, 0.0f));
			StripBrush = FSlateRoundedBoxBrush(TypingPaper, 2.0f, FLinearColor(0.05f, 0.035f, 0.02f, 0.75f), 1.0f);
			BezelBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("8F7238"))), 7.0f, FLinearColor(FColor::FromHex(TEXT("E2C27C"))), 1.5f);
			BezelInner = FSlateRoundedBoxBrush(FLinearColor(0.01f, 0.008f, 0.006f, 1.0f), 3.0f);
			BadgeBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("E0B872"))), 8.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f), 1.0f);
			{
				// The six doors: round, dark, with a brass rim that brightens under the hand.
				const FLinearColor Dark(0.03f, 0.022f, 0.015f, 0.96f), Lit(0.085f, 0.06f, 0.035f, 0.98f), Bright(FColor::FromHex(TEXT("F0D28A")));
				DoorStyle.SetNormal(FSlateRoundedBoxBrush(Dark, 23.0f, DoorBrass, 2.0f));
				DoorStyle.SetHovered(FSlateRoundedBoxBrush(Lit, 23.0f, Bright, 2.5f));
				DoorStyle.SetPressed(FSlateRoundedBoxBrush(Lit, 23.0f, Bright, 3.0f));
				DoorStyle.SetDisabled(FSlateRoundedBoxBrush(Dark.CopyWithNewOpacity(0.6f), 23.0f, Faint, 1.0f));
				DoorStyle.SetNormalPadding(FMargin(0.0f));
				DoorStyle.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				// The control that ends the month: a pill of solid brass.
				const FLinearColor Edge(FColor::FromHex(TEXT("4A3614")));
				PillStyle.SetNormal(FSlateRoundedBoxBrush(PillBrass, 25.0f, Edge, 2.0f));
				PillStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("D2AC62"))), 25.0f, Edge, 2.0f));
				PillStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("A07C3C"))), 25.0f, Edge, 2.5f));
				PillStyle.SetDisabled(FSlateRoundedBoxBrush(PillBrass.CopyWithNewOpacity(0.6f), 25.0f, Edge, 1.0f));
				PillStyle.SetNormalPadding(FMargin(0.0f));
				PillStyle.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				// The prompts under it, and the guide and the menu: plates of leather with a brass edge.
				PromptStyle = MakeButtonStyle(Leather, FLinearColor(FColor::FromHex(TEXT("3D2A1A"))).CopyWithNewOpacity(0.97f), Brass.CopyWithNewOpacity(0.85f), 1.5f);
				PillShine = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 21.0f, FLinearColor(FColor::FromHex(TEXT("EBD096"))).CopyWithNewOpacity(0.75f), 1.0f);
			}
			ChipBrush = FSlateRoundedBoxBrush(FLinearColor::White, 3.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			BarBrush = FSlateRoundedBoxBrush(FLinearColor::White, 1.5f);
			// (The raid card, to the art director's brief: a cream border like a print's round the target's picture, a
			// round brass ring for its plus and minus, and one button of solid brass that flies the raid.)
			PrintBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("E9E0C8"))), 1.0f);
			// (Ruling 88, to the art director's numbers.) END THE MONTH's two looks: dark leather with a brass rim while
			// armies wait, and lit brass with a pale halo when every army has its orders.
			{
				const FLinearColor DarkFill(FColor::FromHex(TEXT("2E241A"))), DarkRim(FColor::FromHex(TEXT("8A7245")));
				EndDarkStyle.SetNormal(FSlateRoundedBoxBrush(DarkFill, 25.0f, DarkRim, 2.0f));
				EndDarkStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("3D2F21"))), 25.0f, BrightBrass, 2.0f));
				EndDarkStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("241C14"))), 25.0f, BrightBrass, 2.5f));
				EndDarkStyle.SetDisabled(FSlateRoundedBoxBrush(DarkFill.CopyWithNewOpacity(0.6f), 25.0f, DarkRim, 1.0f));
				EndDarkStyle.SetNormalPadding(FMargin(0.0f));
				EndDarkStyle.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				const FLinearColor LitFill(FColor::FromHex(TEXT("E9C679"))), Halo(FColor::FromHex(TEXT("F0D28A")));
				EndLitStyle.SetNormal(FSlateRoundedBoxBrush(LitFill, 25.0f, Halo, 2.5f));
				EndLitStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("F2D38C"))), 25.0f, FLinearColor(FColor::FromHex(TEXT("FFF0C0"))), 2.5f));
				EndLitStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("CFAA5C"))), 25.0f, Halo, 3.0f));
				EndLitStyle.SetDisabled(FSlateRoundedBoxBrush(LitFill.CopyWithNewOpacity(0.6f), 25.0f, Halo, 1.0f));
				EndLitStyle.SetNormalPadding(FMargin(0.0f));
				EndLitStyle.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				NoBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 21.0f);
				KeyCapBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 3.0f, FLinearColor::White, 1.0f);
				CountDiscBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("2B2118"))), 16.0f);
				QuietChipBrush = FSlateRoundedBoxBrush(DarkFill.CopyWithNewOpacity(0.94f), 4.0f, FLinearColor(FColor::FromHex(TEXT("9CC08A"))).CopyWithNewOpacity(0.55f), 1.0f);
				// (ruling 87: the slip on the army's card, typing paper with the calendar's red bar)
				const FLinearColor SlipPaper(FColor::FromHex(TEXT("D8CBA8")));
				SlipBrush = FSlateRoundedBoxBrush(SlipPaper, 2.0f, FLinearColor(0.05f, 0.035f, 0.02f, 0.85f), 1.0f);
				SlipBarBrush = FSlateRoundedBoxBrush(Binding, FVector4(2.0f, 0.0f, 0.0f, 2.0f));
				SlipButton = FButtonStyle();
				SlipButton.SetNormal(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("C9BB96"))), 3.0f, DeskInk.CopyWithNewOpacity(0.7f), 1.0f));
				SlipButton.SetHovered(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("E6DBBB"))), 3.0f, DeskInk, 1.5f));
				SlipButton.SetPressed(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("B8AA86"))), 3.0f, DeskInk, 1.5f));
				SlipButton.SetNormalPadding(FMargin(0.0f));
				SlipButton.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				NoButton = FButtonStyle();
				NoButton.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource());
				NoButton.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				{
					// (Rules 84.2.6; the UI director's sheet 35.) A block as wide as the tray's rows, not a pill.
					const FLinearColor AlarmGround(FColor::FromHex(TEXT("1D0F0C"))), AlarmLit(FColor::FromHex(TEXT("2A1611")));
					AlarmStyle = FButtonStyle();
					AlarmStyle.SetNormal(FSlateRoundedBoxBrush(AlarmGround, 2.0f, Alarm, 1.0f));
					AlarmStyle.SetHovered(FSlateRoundedBoxBrush(AlarmLit, 2.0f, Alarm, 1.0f));
					AlarmStyle.SetPressed(FSlateRoundedBoxBrush(AlarmLit, 2.0f, Alarm, 1.5f));
					AlarmStyle.SetDisabled(FSlateRoundedBoxBrush(AlarmGround, 2.0f, Alarm, 1.0f));
					AlarmStyle.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
					AlarmEdgeBrush = FSlateRoundedBoxBrush(Alarm, FVector4(2.0f, 0.0f, 0.0f, 2.0f));
					AlarmGroundBrush = FSlateRoundedBoxBrush(AlarmGround, 2.0f, Alarm, 1.0f);
					AlarmCountBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 2.0f, FLinearColor(FColor::FromHex(TEXT("E2D7BC"))).CopyWithNewOpacity(0.6f), 1.0f);
				}
				{
					// (Rules 84.5; the UI director's sheet 34.) The march-up card's rows: a box with a hairline between rows,
					// the chosen row on a warmer ground with a 3px brass edge, and the staff's pick as a small brass tag.
					ApproachBoxBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("20160E"))), 2.0f, Brass.CopyWithNewOpacity(0.45f), 1.0f);
					ApproachLitBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("3A2918"))), 0.0f);
					ApproachEdgeBrush = FSlateRoundedBoxBrush(BrightBrass, 0.0f);
					ApproachHairBrush = FSlateRoundedBoxBrush(Brass.CopyWithNewOpacity(0.35f), 0.0f);
					PickTagBrush = FSlateRoundedBoxBrush(BrightBrass, 2.0f);
					RadioRingBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 7.0f, BrightBrass, 1.5f);
					RadioDotBrush = FSlateRoundedBoxBrush(BrightBrass, 3.0f);
				}
				RearChipBrush = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("2E241A"))).CopyWithNewOpacity(0.94f), 4.0f, FLinearColor(FColor::FromHex(TEXT("6F5D38"))), 1.0f);
				RearRingBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 8.0f, FLinearColor::White, 1.2f);
				// (rulings 86 and 87: the marks inside the pointer's box)
				TipPlain = FTextBlockStyle().SetFont(Font(TEXT("Regular"), 11)).SetColorAndOpacity(FSlateColor(Cream));
				TipStyles = MakeShared<FSlateStyleSet>(TEXT("TheaterTipMarks"));
				TipStyles->Set("Brass", FTextBlockStyle(TipPlain).SetFont(Font(TEXT("Bold"), 11)).SetColorAndOpacity(FSlateColor(BrightBrass)));
				TipStyles->Set("Red", FTextBlockStyle(TipPlain).SetFont(Font(TEXT("Bold"), 11)).SetColorAndOpacity(FSlateColor(Alarm)));
				// (the railway brief, section 8: a train's line in the lit railway's blue-white; a break's opening with the
				// box's emphasis; the kept march's line after it, dim)
				TipStyles->Set("Rail", FTextBlockStyle(TipPlain).SetFont(Font(TEXT("Bold"), 11)).SetColorAndOpacity(FSlateColor(FLinearColor(FColor(198, 228, 255)))));
				TipStyles->Set("Strong", FTextBlockStyle(TipPlain).SetFont(Font(TEXT("Bold"), 11)));
				TipStyles->Set("Dim", FTextBlockStyle(TipPlain).SetColorAndOpacity(FSlateColor(Muted)));
				// (The UI director, 18 November 2026, on the landing card: its section heads "take the card label style,
				// brass small capitals at the label size", as the attack card's WHAT YOU FACE does: SmallCaps(), the frame's
				// one size of small capital label.)
				CardPlain = FTextBlockStyle().SetFont(Font(TEXT("Regular"), 13)).SetColorAndOpacity(FSlateColor(Muted));
				CardStyles = MakeShared<FSlateStyleSet>(TEXT("TheaterCardMarks"));
				CardStyles->Set("Head", FTextBlockStyle(CardPlain).SetFont(SmallCaps()).SetColorAndOpacity(FSlateColor(Brass)));
				CardStyles->Set("Amber", FTextBlockStyle(CardPlain).SetColorAndOpacity(FSlateColor(FLinearColor(FColor::FromHex(TEXT("D9A441"))))));
				CardStyles->Set("Red", FTextBlockStyle(CardPlain).SetColorAndOpacity(FSlateColor(Alarm)));
			}
			// (the month's card, to the art director's brief: boxes (62, 58, 54), the red band, the stamp, a sign's disc, a tag)
			MonthBoxBrush = FSlateRoundedBoxBrush(FLinearColor(FColor(62, 58, 54)), 4.0f);
			MonthBandBrush = FSlateRoundedBoxBrush(FLinearColor(FColor(104, 34, 26)), 3.0f, FLinearColor(FColor(208, 86, 61)), 1.0f);
			// (Ruling 160: news that is not a warning, WAR, FALLEN, FREED, is brass on a dark ground; and the small plate a label can stand on.)
			MonthNewsBrush = FSlateRoundedBoxBrush(FLinearColor(FColor(96, 71, 32)), 3.0f, FLinearColor(FColor(224, 184, 114)), 1.0f);
			MonthLabelBrush = FSlateRoundedBoxBrush(FLinearColor::White, 2.0f);
			MonthStampBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 3.0f, FLinearColor::White, 2.5f);
			MonthDiscBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f), 18.0f, BrightBrass, 1.5f);
			MonthTagBrush = FSlateRoundedBoxBrush(FLinearColor::White, 2.0f);
			RoundStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 17.0f, BrightBrass, 2.0f));
			RoundStyle.SetHovered(FSlateRoundedBoxBrush(Brass.CopyWithNewOpacity(0.45f), 17.0f, BrightBrass, 2.0f));
			RoundStyle.SetPressed(FSlateRoundedBoxBrush(Brass.CopyWithNewOpacity(0.7f), 17.0f, BrightBrass, 2.5f));
			RoundStyle.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.2f), 17.0f, Faint, 1.5f));
			RoundStyle.SetNormalPadding(FMargin(0.0f));
			RoundStyle.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
			BrassStyle = MakeButtonStyle(FLinearColor(FColor::FromHex(TEXT("D2A04E"))), FLinearColor(FColor::FromHex(TEXT("E6BC6C"))), FLinearColor(FColor::FromHex(TEXT("F0CF8A"))), 1.5f);
			BrassStyle.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("D2A04E"))).CopyWithNewOpacity(0.25f), 4.0f, Faint, 1.0f));
			CardBrush = FSlateRoundedBoxBrush(CardFill, 5.0f, Faint.CopyWithNewOpacity(0.55f), 1.0f);
			// (Ruling 107(d).) The two ways of a bombing raid, each a button: the chosen one framed in brass, the other
			// quietly; a hollow ring waits where the CHOSEN tag would be; and a quiet tag for a group that stays at home.
			WayChosenStyle = MakeButtonStyle(CardFill, CardFill, BrightBrass, 2.0f);
			WayQuietStyle = MakeButtonStyle(CardFill, CardHover, FLinearColor(FColor(104, 92, 72)), 1.0f);
			WayRingBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 9.0f, BrightBrass, 1.5f);
			WayTagBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 2.0f, Faint, 1.0f);
			// (The defences brief.) A pip of a place's guns: brass up to its level, a hollow ring in a quiet grey after it.
			PipBrush = FSlateRoundedBoxBrush(FLinearColor::White, 4.0f);
			PipRingBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 4.0f, FLinearColor(FColor(128, 116, 96)), 1.0f);
			DotBrush = FSlateRoundedBoxBrush(FLinearColor::White, 6.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 1.0f);
			PickedCard = MakeButtonStyle(CardFill, CardHover, Brass, 1.5f);
			LeftCard = MakeButtonStyle(CardFill.CopyWithNewOpacity(0.7f), CardHover, Faint.CopyWithNewOpacity(0.6f), 1.0f);
			EndMonthStyle = MakeButtonStyle(FLinearColor(0.10f, 0.075f, 0.04f, 0.97f), FLinearColor(0.17f, 0.125f, 0.065f, 0.98f), Brass, 1.5f);

			auto Region = [this]() -> const FRegionPanelInfo*
			{
				return Controller.IsValid() ? &Controller->GetRegionPanel() : nullptr;
			};
			// (Brian's memorial card) One ship on the plate: her name, and under it who she was and the men lost with her.
			auto MemorialRow = [this](int32 Index) -> TSharedRef<SWidget>
			{
				return SNew(SVerticalBox)
					.Visibility_Lambda([this, Index]() { return Controller.IsValid() && Controller->GetMemorial().Rows.IsValidIndex(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, Index == 0 ? 9.0f : 13.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Serif(TEXT("Bold"), 22)).ColorAndOpacity(Cream)
						.Text_Lambda([this, Index]() { return Controller.IsValid() && Controller->GetMemorial().Rows.IsValidIndex(Index) ? FText::FromString(Controller->GetMemorial().Rows[Index].Name) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Typed(11)).ColorAndOpacity(Muted)
						.Text_Lambda([this, Index]() { return Controller.IsValid() && Controller->GetMemorial().Rows.IsValidIndex(Index) ? FText::FromString(Controller->GetMemorial().Rows[Index].Line) : FText::GetEmpty(); })
					];
			};
			auto Tray = [this]() -> const FArmyTray*
			{
				return Controller.IsValid() ? &Controller->GetArmyTray() : nullptr;
			};
			auto Face = [this]() -> const FFaceOff*
			{
				return Controller.IsValid() ? &Controller->GetFaceOff() : nullptr;
			};
			ScrimBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f);
			DeepScrimBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f);
			FilmBrush = FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.012f, 0.012f, 1.0f), 0.0f);
			ClearBrush = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 0.0f);
			PlainButton = MakeButtonStyle(CardFill, CardHover, Faint, 1.0f);
			PlateButton = MakeButtonStyle(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.35f, 0.22f, 0.08f, 0.16f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 1.0f);
			// (Rules 84.3.3; ruling 141) FIRST COMMAND on the door: the kit's dark button, its brass-300 rim lit by the lamp
			// until the ten lessons are done, then its plain brass-700 one.
			FirstDoorLit = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("2D241A"))), 5.0f, FLinearColor(FColor::FromHex(TEXT("E0B872"))), 1.5f);
			FirstDoorPlain = FSlateRoundedBoxBrush(FLinearColor(FColor::FromHex(TEXT("2D241A"))), 5.0f, FLinearColor(FColor::FromHex(TEXT("8A6B3B"))), 1.0f);
			ChosenCard = MakeButtonStyle(FLinearColor(0.12f, 0.09f, 0.045f, 0.98f), FLinearColor(0.17f, 0.125f, 0.065f, 0.98f), BrightBrass, 2.0f);
			FootBrush = FSlateRoundedBoxBrush(FLinearColor(0.03f, 0.026f, 0.021f, 0.98f), 5.0f, Brass.CopyWithNewOpacity(0.8f), 1.0f);
			WatchBrush = FSlateRoundedBoxBrush(FLinearColor(0.16f, 0.15f, 0.13f, 1.0f), 2.0f, Faint, 1.0f);

			// One side of the face-off: whose army it is, who leads it, how strong it is, and its corps.
			auto Side = [this, Face](bool bOurs, EHorizontalAlignment Align) -> TSharedRef<SWidget>
			{
				const ETextJustify::Type Justify = Align == HAlign_Right ? ETextJustify::Right : ETextJustify::Left;
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(42.0f).HeightOverride(28.0f)
							[
								SNew(SImage).Image_Lambda([this, Face, bOurs]() { const FFaceOff* F = Face(); return FlagBrush(F ? (bOurs ? F->OurTag : F->TheirTag) : FString()); })
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 160)).ColorAndOpacity(Brass)
							.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? FText::FromString((bOurs ? F->OurName : F->TheirName).ToString().ToUpper()) : FText::GetEmpty(); })
						]
					]
					// (The UI director's review of build 1628, item 2: every line wraps inside its half of the card, the
					// defender's set to the right; the slots give each the column's whole width.)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 22)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(Justify)
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurGeneral : F->TheirGeneral) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(SPhraseText).Face(Font(TEXT("Regular"), 11)).Ink(Muted).Justify(Justify).Guess(370.0f)
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurStrength : F->TheirStrength) : FText::GetEmpty(); })
					]
					// (Check 19.) The corps, in the roster's two forms; the slot gives it the column's whole width.
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(SRosterBlock).Player(Controller).bOurs(bOurs)
						.Visibility_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F && (bOurs ? F->OurRoster : F->TheirRoster).Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					]
					// A town's guard, said in a sentence where the corps would be listed.
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).Justification(Justify).LineHeightPercentage(1.25f).AutoWrapText(true)
						.Visibility_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F && (bOurs ? F->OurRoster : F->TheirRoster).Num() == 0 && !(bOurs ? F->OurUnits : F->TheirUnits).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurUnits : F->TheirUnits) : FText::GetEmpty(); })
					];
			};

			// (Brian, 21 November 2026: "have each sides flag on either end of the bar".) A side's flag for its end of
			// the card's balance bar: the player's side on the left, as the bar's own colours lie.
			auto BarFlag = [this, Face](bool bOurs) -> TSharedRef<SWidget>
			{
				return SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)
					[
						SNew(SImage).Image_Lambda([this, Face, bOurs]() { const FFaceOff* F = Face(); return FlagBrush(F ? (bOurs ? F->OurTag : F->TheirTag) : FString()); })
					];
			};

			ChildSlot
			[
				SNew(SOverlay)
				// With the frame put away for a clean picture (the F key), only the map's own lettering is left, and
				// for a moment a line that says how to bring the frame back.
				+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
				[
					SNew(SMapLettering).Controller(Controller)
					.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsFrameHidden() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(24.0f)
				[
					SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(18.0f, 8.0f))
					.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsFrameHidden() && Controller->IsFrameHintShown() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("The frame is put away. Press F to bring it back."))).Font(Typed(12)).ColorAndOpacity(Cream)
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
				[
				SNew(SDPIScaler)
				.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsFrameHidden() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
				.DPIScale_Lambda([]()
				{
					FVector2D Size(1920.0, 1080.0);
					if (GEngine && GEngine->GameViewport)
					{
						GEngine->GameViewport->GetViewportSize(Size);
					}
					return FrameScale(Size.Y);
				})
				[
					SAssignNew(ScreenRoot, SOverlay)

					// Under everything else on the screen: the names lettered across the map, and over them the name
					// plates of the towns on the table.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SMapLettering).Controller(Controller)
					]
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(STownPlates).Controller(Controller)
					]
					// (Ruling 127.) The sky's plates, in the sky view.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SSkyPlates).Controller(Controller)
					]

					// The date plate: a desk calendar. A red binding along its top, the month in red capitals over the
					// year in heavy black, who the player is in a clerk's italic, a ruled line, and under it what the
					// treasury holds, typed.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.0f)
					[
						Edge(TopLeftPiece, SNew(SBorder).BorderImage(&PaperBrush).Padding(FMargin(1.0f))
						.BorderBackgroundColor_Lambda([this]() { return FSlateColor(PaperLight()); }) // (paper is not so white by night)
						.ColorAndOpacity_Lambda([this]() { return InkLight(); }) // (and the ink on it is darker)
						.Visibility_Lambda([this]() { return PlatesAway() ? EVisibility::Collapsed : EVisibility::Visible; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the plate is not the map
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								// (The UI director, 11:19 on build 1638: the mark rises to the label size, and as a mark it is the
								// one label that keeps its wide tracking. The binding grows round it.)
								SNew(SBorder).BorderImage(&BindingBrush).Padding(FMargin(19.0f, 3.0f, 19.0f, 3.0f))
								.BorderBackgroundColor_Lambda([this]() { return FSlateColor(PaperLight()); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("THEATER COMMAND"))).Font(Typed(9.0f, true, 80)).ColorAndOpacity(TypingPaper.CopyWithNewOpacity(0.92f))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(19.0f, 6.0f, 24.0f, 8.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(Serif(TEXT("Bold"), 12, 300)).ColorAndOpacity(InkRed)
									.Text_Lambda([this]()
									{
										// (the month, without its year: "SEPTEMBER")
										FString Date = Controller.IsValid() ? Controller->GetDateText().ToString() : FString(), Month, Year;
										return FText::FromString((Date.Split(TEXT(" "), &Month, &Year, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Month : Date).ToUpper());
									})
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, -4.0f, 0.0f, -3.0f)
								[
									SNew(STextBlock).Font(Serif(TEXT("Bold"), 27)).ColorAndOpacity(DeskInk)
									.Text_Lambda([this]()
									{
										FString Date = Controller.IsValid() ? Controller->GetDateText().ToString() : FString(), Month, Year;
										return FText::FromString(Date.Split(TEXT(" "), &Month, &Year, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Year : FString());
									})
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(Serif(TEXT("Italic"), 11)).ColorAndOpacity(InkSoft)
									.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetRulesNote() : FText::GetEmpty(); })
								]
								// (Rules version 66) How deep the campaign is played, and the door to the staff's room.
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
								[
									Aimed(TEXT("depth"), SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f)).IsFocusable(false)
									.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetDepthLine().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
									.ToolTipText(FText::FromString(TEXT("How deep you play: which parts of the war are on your own desk and which your staff runs for you. Click to open YOUR STAFF, where the depth or any one desk can be changed in any month.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenStaff(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Font(Serif(TEXT("Italic"), 11)).ColorAndOpacity(InkRed)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetDepthLine() : FText::GetEmpty(); })
									])
								]
								// The weather, when it is not fair: mud in the east, or the eastern winter.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(296.0f)
									.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWeatherText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									[
										SNew(STextBlock).Font(Serif(TEXT("Regular"), 10)).ColorAndOpacity(InkSoft).AutoWrapText(true)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWeatherText() : FText::GetEmpty(); })
									]
								]
								// The ruled line.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
								[
									SNew(SBox).HeightOverride(1.0f)
									.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetTreasuryText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									[
										SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(DeskInk.CopyWithNewOpacity(0.45f))
									]
								]
								// The treasury, always in view: what is in hand, and what comes in, goes out and is left each
								// month. It is a button: pressing it opens the ledger.
								+ SVerticalBox::Slot().AutoHeight().Padding(-8.0f, 2.0f, 0.0f, -4.0f)
								[
									SNew(SButton).ButtonStyle(&PlateButton).ContentPadding(FMargin(8.0f, 3.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("Open the ledger: where the money comes from, what the forces cost, and what every province gives.")))
									.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetTreasuryText().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenLedger(); } return FReply::Handled(); })
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock).Font(Typed(13, true))
											.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->IsInDebt() ? InkRed : DeskInk); })
											.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetTreasuryText() : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Typed(9)).ColorAndOpacity(InkSoft)
											.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetBudgetText() : FText::GetEmpty(); })
											.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetBudgetText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										]
									]
								]
								// Oil and supplies, for a power that keeps stocks of them: how full the depots are and how long they last.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox)
									.Visibility_Lambda([this]() { return StoresShown() ? EVisibility::Visible : EVisibility::Collapsed; })
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)[MakeGauge(TEXT("OIL"), 0)]
									+ SHorizontalBox::Slot().AutoWidth()[MakeGauge(TEXT("SUPPLIES"), 1)]
								]
								// The reserve of men (version 59), for every nation that keeps one: a line of its own.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox)
									.Visibility_Lambda([this]() { return StockOf(2) ? EVisibility::Visible : EVisibility::Collapsed; })
									+ SHorizontalBox::Slot().AutoWidth().Padding(-6.0f, -2.0f, 0.0f, -2.0f)
									[
										// (a button: pressing it opens the count of men and the call-up)
										SNew(SButton).ButtonStyle(&PlateButton).ContentPadding(FMargin(6.0f, 2.0f)).IsFocusable(false)
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenMen(); } return FReply::Handled(); })
										[
											MakeGauge(TEXT("MEN"), 2)
										]
									]
								]
							]
						])
					]

					// A tip that follows the pointer over the map: the province under it, in a few words. (The UI director,
					// 18 November 2026, on build 1596: the pointer's slip stays whole. See PlaceHoverTip.)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
					.Padding(TAttribute<FMargin>::CreateLambda([this]() { return PlaceHoverTip(); }))
					[
						SAssignNew(HoverTipBox, SBorder).BorderImage(&PanelBrush).Padding(FMargin(10.0f, 6.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetHoverTip().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).LineHeightPercentage(1.15f).WrapTextAt(420.0f)
								.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetHoverTip() : FText::GetEmpty(); })
							]
							// (Rulings 86 and 87.) Under it, what is said with a mark of its own colour in it: brass for "No
							// moves left this month." and "Open to your ships.", the refusing red where the answer is no.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(SRichTextBlock).TextStyle(&TipPlain).DecoratorStyleSet(TipStyles.Get()).LineHeightPercentage(1.15f).WrapTextAt(420.0f)
								.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetHoverTipMarked().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetHoverTipMarked() : FText::GetEmpty(); })
							]
						]
					]

					// News for the player, for a few seconds at a time.
					// (The art director's ruling, as for the map's plates: the line keeps to the gap between the calendar on
					// the left and the chips on the right, whatever their widths today, and folds onto a second line
					// sooner than cross either or lose its end.)
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
					.Padding(TAttribute<FMargin>::CreateLambda([this]() { return NoticeGap(nullptr); }))
					[
						Piece(SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(24.0f, 10.0f))
						.Visibility_Lambda([this]()
						{
							// (release 1.5.1: while the READY plate offers SHOW ME, its button can be pressed; the rest stays out of the pointer's way)
							return Controller.IsValid() && !Controller->GetNotice().IsEmpty() && !Controller->GetNewsreel().bOn ? (Controller->CanShowReady() ? EVisibility::SelfHitTestInvisible : EVisibility::HitTestInvisible) : EVisibility::Collapsed;
						})
						[
							SNew(SVerticalBox)
							// ---- (Release 1.5.1, new units ready; Brian, 24 November 2026 18:30Z) READY FOR ORDERS, in brass with
							// his flag, over the plate's lines while it names his new units.
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 5.0f)
							[
								SNew(SHorizontalBox)
								.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetReadyHeading().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(21.0f).HeightOverride(14.0f)
									[
										SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() ? Controller->GetReadyNation() : FString()); })
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Typed(10)).ColorAndOpacity(BrightBrass)
									.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() ? Controller->GetReadyHeading() : FString()); })
								]
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Typed(13)).ColorAndOpacity(Cream).Justification(ETextJustify::Center).LineHeightPercentage(1.1f)
								// (folded at the width of the gap less the plate's own margins: a short line keeps its own width)
								.WrapTextAt_Lambda([this]() { float Width = 800.0f; NoticeGap(&Width); return FMath::Max(Width - 52.0f, 240.0f); })
								.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetNotice() : FText::GetEmpty(); })
								.Visibility(EVisibility::HitTestInvisible)
							]
							// SHOW ME: the eye goes to each new unit in turn, and a new army is put in his hand.
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 7.0f, 0.0f, 0.0f)
							[
								SNew(SBox)
								.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->CanShowReady() ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									MapButton(TEXT("SHOW ME  \u00B7  G"), TEXT("Go to each new unit in turn: the eye flies to it, and a new army is put in your hand, ready for its first orders."), [this]() { Controller->ShowReady(); })
								]
							]
							// ---- (end release 1.5.1)
						])
					]

					// The adviser: the first steps of a campaign, each ticked as it is done. He can be sent away.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.0f, 276.0f, 0.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(360.0f)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetAdviser().bOn && Controller->IsMapShown() ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(18.0f, 12.0f, 14.0f, 13.0f))
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("YOUR CHIEF OF STAFF SUGGESTS"))).Font(SmallCaps()).ColorAndOpacity(Brass)
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(7.0f, 2.0f)).IsFocusable(false)
										.ToolTipText(FText::FromString(TEXT("Send him away. H opens the guide whenever you want it.")))
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->DismissAdviser(); } return FReply::Handled(); })
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("DISMISS"))).Font(SmallCaps()).ColorAndOpacity(Muted)
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SAssignNew(AdviserSteps, SVerticalBox)
								]
							]
						]
					]

					// While the month is replayed: the flags of the nations whose moves are shown, in the order they
					// made them, with the one on the table now standing larger.
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24.0f, 164.0f, 24.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(12.0f, 7.0f))
						.Visibility_Lambda([this]() { return WatchShown.Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SAssignNew(WatchStrip, SHorizontalBox)
						]
					]

					// (release 1) The bigger unit card: docked straight above the army's tray, so that it never covers the
					// army in hand (the art director's ruling of round 17).
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
					.Padding(TAttribute<FMargin>::CreateLambda([this]() { return BigCardPlace(); }))
					[
						SAssignNew(BigCardShow, SBox).Visibility(EVisibility::HitTestInvisible)
					]

					// The top right corner: the six doors, the guide and the menu, and the control that ends the month;
					// and under them the month's news for a while after the month turns.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.0f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this]() { return PlatesAway() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							Piece(SNew(SHorizontalBox)
							// The six doors, on a strip of leather: round and dark, each with its sign engraved in brass.
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(10.0f, 7.0f, 10.0f, 5.0f))
								.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the strip is not the map
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth()[Aimed(TEXT("arsenal"), MakeDoor(0, TEXT("ARSENAL"), TEXT("The arsenal: order corps, aircraft, ships, factories and other works. (1)"), [this]() { Controller->OpenArsenal(); }))]
									+ SHorizontalBox::Slot().AutoWidth()[MakeDoor(1, TEXT("GENERALS"), TEXT("Your generals: who commands what, who is waiting for a command, and the corps that have nobody. (2)"), [this]() { Controller->OpenGenerals(); })]
									+ SHorizontalBox::Slot().AutoWidth()[MakeDoor(2, TEXT("RESEARCH"), TEXT("The design offices: put design teams on the branches whose new weapons you want early. The number is the teams standing idle. (3)"), [this]() { Controller->OpenResearch(); }, [this]() { return Controller->GetIdleTeams(); })]
									+ SHorizontalBox::Slot().AutoWidth()[MakeDoor(3, TEXT("DIPLOMACY"), TEXT("The foreign office: who is at war, where every neutral stands, courting them, and declaring war. (4)"), [this]() { Controller->OpenDiplomacy(); })]
									// (Ruling 163: the secret service's own door, where the rules open that room. The room keeps its name.)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SBox)
										.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetSession() && Controller->GetSession()->GetView().PageIds.Contains(TEXT("spies")) ? EVisibility::Visible : EVisibility::Collapsed; })
										[
											MakeDoor(9, TEXT("SPIES"), TEXT("The secret service: your agents, where they are and what they report, and recruiting more."), [this]() { Controller->OpenPage(TEXT("spies")); })
										]
									]
									+ SHorizontalBox::Slot().AutoWidth()[MakeDoor(4, TEXT("LEDGER"), TEXT("The ledger: where the money comes from, what the forces cost, and what every province gives. (5)"), [this]() { Controller->OpenLedger(); })]
									+ SHorizontalBox::Slot().AutoWidth()[MakeDoor(5, TEXT("HONOURS"), TEXT("The roll of honour: the formations that have made a name, and the men who have fallen."), [this]() { Controller->OpenRoll(); })]
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SButton).ButtonStyle(&PromptStyle).ContentPadding(FMargin(14.0f, 6.0f)).IsFocusable(false).HAlign(HAlign_Center)
										.ToolTipText(FText::FromString(TEXT("The Guide: the ten lessons of FIRST COMMAND, done, waiting and to come; and how to play, the table, a month and what every button is for. (H)")))
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenGuide(); } return FReply::Handled(); })
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("GUIDE"))).Font(SmallCaps()).ColorAndOpacity(Cream.CopyWithNewOpacity(0.92f))
										]
									]
									// (Rules 84.3.3; the look brief, section 6: the kit's tc-count on GUIDE's top right corner, the notes he
									// has folded away with LATER. None waiting, no badge.)
									+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.0f, -8.0f, -8.0f, 0.0f))
									[
										SNew(SBorder).BorderImage(&BadgeBrush).Padding(FMargin(5.0f, 0.0f))
										.Visibility_Lambda([this]()
										{
											const FCampaignSession* Rules = Controller.IsValid() ? Controller->GetSession() : nullptr;
											return Rules && !Rules->GetView().Human.IsEmpty() && Controller->GetLessonsWaiting() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
										})
										[
											SNew(STextBlock).Font(Typed(10, true)).ColorAndOpacity(DeskInk)
											.Text_Lambda([this]() { return FText::AsNumber(Controller.IsValid() ? Controller->GetLessonsWaiting() : 0); })
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SButton).ButtonStyle(&PromptStyle).ContentPadding(FMargin(14.0f, 6.0f)).IsFocusable(false).HAlign(HAlign_Center)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenMenu(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("MENU"))).Font(SmallCaps()).ColorAndOpacity(Cream.CopyWithNewOpacity(0.92f))
									]
								]
							]
							// The control that ends the month: a pill of brass with a dial, whose short hand stands at the
							// month of the year, and under its words how many armies have yet to move.
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								EndPills()
							])
						]
						// On the eve of war: the staff's plan for placing the army, carried out at a press.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							Chip(SNew(SButton).ButtonStyle(&PromptStyle).ContentPadding(FMargin(16.0f, 6.0f)).IsFocusable(false)
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->HasEvePlan() ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(FText::FromString(TEXT("The staff moves the army by rail to where it advises, strongest corps first. It does the most when it is the first thing done; you can still move any corps yourself afterwards.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->PlaceByPlan(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Typed(8.5f, true)).ColorAndOpacity(BrightBrass)
								.Text(FText::FromString(TEXT("PLACE THE ARMY AS THE STAFF ADVISES")))
							])
						]
						// (Ruling 88.) NEXT ARMY, directly under END THE MONTH and as wide: the brass one while armies wait for
						// orders, and a quiet green chip when every army has them.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, -2.0f, 0.0f, 10.0f)
						[
							Chip(NextArmyPill())
						]
						// (Rules 84.2.6; sheet 35.) The alarm that names the cure, first in the tray, above every other row.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeAlarm()
						]
						// (Rules 84.2.6.) His own guns left alone round an enemy town: the siege's own row, in the rules' words.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeGunsAlone(0)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeGunsAlone(1)
						]
						// And the rest of it: a general with no command, a design team with no work, nothing on order.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeToDo(0)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeToDo(1)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeToDo(2)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeToDo(3)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							MakeToDo(4)
						]
						// The small map: every country in its colour, the armies as marks, the part on screen boxed.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 4.0f, 0.0f, 12.0f)
						[
							Edge(SmallMapPiece, SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(6.0f))
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMapShown() ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(FText::FromString(TEXT("The whole map. White marks are your armies, blue your allies', red the enemy's. Click or drag to look somewhere else.")))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									// (the small map sits in a bezel of brass, like the glass of an instrument)
									SNew(SBorder).BorderImage(&BezelBrush).Padding(FMargin(4.0f))
									[
										SNew(SBorder).BorderImage(&BezelInner).Padding(FMargin(1.5f))
										[
											SNew(SMinimap).Controller(Controller).High(232.0f)
										]
									]
								]
								// (Under the map of one theatre the five buttons stand in three rows; under the wider map of the whole
								// world, in two: three and two.)
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(SVerticalBox)
									.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMinimapWorld() ? EVisibility::Collapsed : EVisibility::Visible; })
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)[MapButton(TEXT("FORCES"), TEXT("Every army, fleet and airfield you command, each with a way to go to it. (L)"), [this]() { Controller->OpenForces(); })]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)[MapButton(TEXT("REPORT"), TEXT("What every nation did last month, each with a way to go and look. (R)"), [this]() { Controller->OpenReports(); })]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)[MapButton(TEXT("GOALS"), TEXT("What winning means for your country, and how far you have got. (O)"), [this]() { Controller->OpenGoals(); })]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)[MapButton(TEXT("CAPITAL"), TEXT("Look at your capital. (C, or Home)"), [this]() { Controller->GoToCapital(); })]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
									[
										MapButton(TEXT("MAP VIEWS"), TEXT("Steps through the views of the map, here and on the table: the map as it is; every country in flat colour; the wealth view, gold by what a province pays; friend and foe, your own ground green, your allies' blue, your enemies' red; and the admiral's chart, a veil over the seas you have not searched. (Y)\nAlso: hold B to lift every piece off the table and read the bare ground. Shift with 6, 7, 8 or 9 keeps the view you are looking at, and the key alone comes back to it."), [this]() { Controller->ToggleWealth(); })
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(SVerticalBox)
									.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMinimapWorld() ? EVisibility::Visible : EVisibility::Collapsed; })
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)[MapButton(TEXT("FORCES"), TEXT("Every army, fleet and airfield you command, each with a way to go to it. (L)"), [this]() { Controller->OpenForces(); })]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 3.0f, 0.0f)[MapButton(TEXT("REPORT"), TEXT("What every nation did last month, each with a way to go and look. (R)"), [this]() { Controller->OpenReports(); })]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)[MapButton(TEXT("GOALS"), TEXT("What winning means for your country, and how far you have got. (O)"), [this]() { Controller->OpenGoals(); })]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)[MapButton(TEXT("CAPITAL"), TEXT("Look at your capital. (C, or Home)"), [this]() { Controller->GoToCapital(); })]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)[MapButton(TEXT("MAP VIEWS"), TEXT("Steps through the views of the map, here and on the table: the map as it is; every country in flat colour; the wealth view, gold by what a province pays; friend and foe, your own ground green, your allies' blue, your enemies' red; and the admiral's chart, a veil over the seas you have not searched. (Y)\nAlso: hold B to lift every piece off the table and read the bare ground. Shift with 6, 7, 8 or 9 keeps the view you are looking at, and the key alone comes back to it."), [this]() { Controller->ToggleWealth(); })]
									]
								]
							])
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
						Piece(SNew(SBox).WidthOverride(400.0f)
						.Visibility_Lambda([this]()
						{
							return Controller.IsValid() && !Controller->GetDispatches().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							// (a strip of typing paper off the signals office's machine: DISPATCH in red, the news typed under it;
							// a little room is left at its right edge for a wider face)
							SNew(SBorder).BorderImage(&StripBrush).Padding(FMargin(18.0f, 11.0f, 26.0f, 14.0f))
							.BorderBackgroundColor_Lambda([this]() { return FSlateColor(PaperLight()); })
							.ColorAndOpacity_Lambda([this]() { return InkLight(); })
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the news is not the map
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("DISPATCH"))).Font(Typed(10, true, 320)).ColorAndOpacity(InkRed)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(SBox).HeightOverride(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(InkRed.CopyWithNewOpacity(0.55f))]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Typed(10.5f)).ColorAndOpacity(DeskInk).AutoWrapText(true).LineHeightPercentage(1.2f)
									.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetDispatches() : FText::GetEmpty(); })
								]
							]
						])
					]
					]

					// (Brian's memorial card, 19 November 2026.) A battleship or a carrier lost at sea is named, low on the
					// screen and clear of the plates: where, who she was, and the men lost with her. It takes no click and
					// stops no play; it fades in, stands a few seconds and fades out.
					// (The art director's ruling the same day: low centre, its bottom edge pinned seven hundredths of the
					// frame's height above the bottom, so a taller plate grows upward; three named ships at most and the
					// count of the rest; no dimming by night. The UI director: the count is typed as the lines are.
					// The plate is a piece of the frame and takes the frame's own scale with the window's height.)
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
					.Padding(TAttribute<FMargin>::CreateLambda([this]()
					{
						const float High = ScreenRoot.IsValid() ? static_cast<float>(ScreenRoot->GetTickSpaceGeometry().GetLocalSize().Y) : 1000.0f;
						return FMargin(24.0f, 0.0f, 24.0f, FMath::Max(0.07f * High, 56.0f));
					}))
					[
						Piece(SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(36.0f, 14.0f, 36.0f, 17.0f))
						.Visibility_Lambda([this]()
						{
							return Controller.IsValid() && Controller->GetMemorialShown() > 0.0f && !Controller->GetNewsreel().bOn ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						})
						.BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, Controller.IsValid() ? Controller->GetMemorialShown() : 0.0f)); })
						.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, Controller.IsValid() ? Controller->GetMemorialShown() : 0.0f); })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Brass)
								.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetMemorial().Heading) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 7.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(120.0f).HeightOverride(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Brass.CopyWithNewOpacity(0.6f))]
							]
							+ SVerticalBox::Slot().AutoHeight()[MemorialRow(0)]
							+ SVerticalBox::Slot().AutoHeight()[MemorialRow(1)]
							+ SVerticalBox::Slot().AutoHeight()[MemorialRow(2)]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 11.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Typed(11)).ColorAndOpacity(Muted)
								.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetMemorial().More.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetMemorial().More) : FText::GetEmpty(); })
							]
						])
					]

					// Along the bottom: the territory panel and the army tray.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bVisible ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 16.0f, 0.0f)
						[
							Piece(SNew(SBox).WidthOverride(390.0f)
							.Visibility_Lambda([Region]()
							{
								const FRegionPanelInfo* Info = Region();
								return Info && Info->bVisible ? EVisibility::Visible : EVisibility::Collapsed;
							})
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(20.0f, 14.0f, 20.0f, 16.0f))
								.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the panel is not the map
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 22)).ColorAndOpacity(Cream)
										.Text_Lambda([Region]() { const FRegionPanelInfo* Info = Region(); return Info ? Info->Name : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)
											[
												SNew(SImage).Image_Lambda([this, Region]() { const FRegionPanelInfo* Info = Region(); return FlagBrush(Info ? Info->OwnerTag : FString()); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
											.Text_Lambda([Region]() { const FRegionPanelInfo* Info = Region(); return Info ? Info->OwnerLine : FText::GetEmpty(); })
										]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										// (check 19: it folds where a dot stands, and the dot goes)
										SNew(SPhraseText).Face(Font(TEXT("Regular"), 12)).Ink(Muted).Guess(340.0f)
										.Text_Lambda([Region]() { const FRegionPanelInfo* Info = Region(); return Info ? Info->Details : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("FORCES"))).Font(SmallCaps()).ColorAndOpacity(Brass)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
										.Text_Lambda([Region]() { const FRegionPanelInfo* Info = Region(); return Info ? Info->Forces : FText::GetEmpty(); })
									]
									// On home ground: what the province holds, and what can be raised or built there.
									// (It can be a long list, so it scrolls within a fixed height and never climbs over the date plate.)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox).MaxDesiredHeight(330.0f)
										[
											SNew(SScrollBox)
											+ SScrollBox::Slot()
											[
												SAssignNew(ProvinceBox, SVerticalBox)
											]
										]
									]
								]
							])
						]

						// The town of the province picked, when nothing is in hand: its building plots as pictures. A click on
						// a plot lays out above the bar what could go up on it, and a click on one of those orders it.
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							Piece(SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(16.0f, 11.0f, 14.0f, 13.0f))
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsTownBarShown() ? EVisibility::Visible : EVisibility::Collapsed; })
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the bar is not the map
							[
								SAssignNew(TownBarBox, SVerticalBox)
							])
						]

						// The army in hand: its general, and a card for each corps. A click on a card leaves that corps behind.
						// (It takes the room that is left on the screen and no more: with more corps than fit, the row of cards scrolls.)
						+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left).VAlign(VAlign_Bottom)
						[
							SNew(SVerticalBox)
							// (Ruling 87.) The slip: the game speaking, the first times an army with no moves left is picked up.
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(12.0f, 0.0f, 0.0f, 0.0f)
							[
								MarchSlip()
							]
							// (Rulings 153 and 169.) The agent in hand: his card, in the army card's seat.
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
							[
								SNew(SBox).WidthOverride(699.0f).HeightOverride(327.0f)
								.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->HasAgentInHand() && Controller->GetAgentInHand() ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									SNew(SAgentCard).Player(Controller)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
							[
							Edge(TrayPiece, SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(18.0f, 12.0f, 10.0f, 14.0f))
							.Visibility_Lambda([Tray]()
							{
								const FArmyTray* Info = Tray();
								return Info && Info->bVisible ? EVisibility::Visible : EVisibility::Collapsed;
							})
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the tray is not the map
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
									[
										SNew(SBox).WidthOverride(36.0f).HeightOverride(24.0f)
										[
											SNew(SImage).Image_Lambda([this, Tray]() { const FArmyTray* Info = Tray(); return FlagBrush(Info ? Info->Tag : FString()); })
										]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 20)).ColorAndOpacity(Cream)
										.Text_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info ? Info->Title : FText::GetEmpty(); })
									]
									// What the army answers as it is picked up, for a moment.
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 4.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Italic"), 14)).ColorAndOpacity(BrightBrass)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetArmySaid() : FText::GetEmpty(); })
									]
									// (Ruling 88.) STAND THIS MONTH, at the right of the title row and over PUT DOWN: it takes the army
									// in hand out of the round of armies until the next month. It stands away from the "If attacked"
									// orders on purpose, since two orders on one card begin with STAND.
									+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										.Visibility_Lambda([this, Tray]() { const FArmyTray* Info = Tray(); return Info && Info->bVisible && !Info->bLook && !Info->bFleet && Controller.IsValid() && Controller->GetSession() && Controller->GetSession()->GetView().bHumanUp ? EVisibility::Visible : EVisibility::Collapsed; })
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("This month:"))).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted)
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsStandingThisMonth() ? EVisibility::Collapsed : EVisibility::Visible; })
											.ToolTipText(FText::FromString(TEXT("This army stays where it is this month and is asked for no more orders: NEXT ARMY passes it by. It can still be given an order.")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->StandThisMonth(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("STAND THIS MONTH"))).Font(SmallCaps()).ColorAndOpacity(Muted)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsStandingThisMonth() ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText(FText::FromString(TEXT("This army stands this month. Press to take that back: it waits for orders again.")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->StandThisMonth(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("STANDING THIS MONTH"))).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
											]
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 8.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)
									.Text_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info ? Info->Subtitle : FText::GetEmpty(); })
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 9.0f, 0.0f, 0.0f)
								[
									SAssignNew(TrayOrders, SHorizontalBox)
								]
								// (Ruling 118; the railway brief, section 2a.) Under the chips: what the trains will do for the army this month.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 0.0f)
								[
									SAssignNew(TrayRail, SHorizontalBox)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
								[
									SNew(SScrollBox).Orientation(Orient_Horizontal)
									+ SScrollBox::Slot()[SAssignNew(Cards, SHorizontalBox)]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 8.0f, 0.0f)
								[
									SNew(SHorizontalBox)
									// (One voice after a press.) While the lesson's slip is up the foot is blank: the slip says it once.
									.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMarchSlipShown() ? EVisibility::Hidden : EVisibility::Visible; })
									// (Ruling 87.) "No moves left this month." in brass, before what to do about it.
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(BrightBrass)
										.Visibility_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info && !Info->HintLead.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info ? Info->HintLead : FText::GetEmpty(); })
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted)
										.Text_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info ? Info->Hint : FText::GetEmpty(); })
									]
								]
							])
							]
						]

					]

					// The reminder of the controls is folded away into a "?" in the corner: it opens while the pointer is
					// on the mark or the ? key is held, and it stands open through a new player's first month. With an
					// army in hand the tray says what to do, and takes the room.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this, Tray]() { const FArmyTray* Info = Tray(); return (Info && Info->bVisible) || (Controller.IsValid() && (Controller->IsTownBarShown() || !Controller->IsMapShown())) ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							Piece(SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(16.0f, 9.0f))
							.Visibility_Lambda([this]() { return HelpOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Drag or W A S D: move the map    Scroll: zoom    Right-drag or Q / E: turn    C: your capital    Y: map views    T: day or night\nClick a flag, a fleet or an airfield; double-click a place to go there. Right-click where an army or a fleet should go    Tab: next army    Esc: put it down    Enter: end the month\n1 to 5: the screens    L: forces    R: report    O: goals    H: how to play    ?: these keys")))
								.Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).Justification(ETextJustify::Right)
							])
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							Piece(SNew(SBox).WidthOverride(34.0f).HeightOverride(34.0f)
							[
								SAssignNew(HelpMark, SButton).ButtonStyle(&DoorStyle).ContentPadding(FMargin(0.0f)).IsFocusable(false).HAlign(HAlign_Center).VAlign(VAlign_Center)
								.ToolTipText(FText::FromString(TEXT("The keys. Point here, or hold the ? key, to see them; click to keep them open or fold them away.")))
								.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ToggleHelp(); } return FReply::Handled(); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("?"))).Font(Serif(TEXT("Bold"), 16)).ColorAndOpacity(DoorBrass)
								]
							])
						]
					]

					// The face-off: the two armies drawn up before an attack, and afterwards what the battle came to.
					// It sits over everything else and nothing behind it can be clicked.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(912.0f)
							// (the frame's own height, not the window's: the two differ by the frame's scale, and by the window's measure a
							// tall card ran past the bottom of Brian's window before it began to scroll)
							.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(FMath::Max(520.0f, (ScreenRoot.IsValid() ? static_cast<float>(ScreenRoot->GetTickSpaceGeometry().GetLocalSize().Y) : 972.0f) - 44.0f)); })
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(34.0f, 24.0f, 22.0f, 26.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Title : FText::GetEmpty(); })
									]

									// Everything between the title and the orders scrolls when the card is taller than the screen, so the orders are never pushed off it.
									+ SVerticalBox::Slot().FillHeight(1.0f)
									[
										SNew(SScrollBox)
										+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
										[
											SNew(SVerticalBox)
									// What the battle came to, once it has been fought.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bResolved ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Bold"), 26)).ColorAndOpacity(BrightBrass)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ResultTitle : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(20.0f, 4.0f, 20.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ResultDetail : FText::GetEmpty(); })
										]
										// (Version 84) Who paid, and to what.
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
										[
											SNew(SFaceBlock).Player(Controller).bPaid(true)
										]
									]

									// The two armies.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f)
										[
											Side(true, HAlign_Left)
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18.0f, 0.0f)
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("against"))).Font(Font(TEXT("Italic"), 13)).ColorAndOpacity(Faint)
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f)
										[
											Side(false, HAlign_Right)
										]
									]

									// (Rules 84.1; the UI director's sheet 21.) Before a march on a guarded town each of the two
									// forecasts is headed by its order's own words, so nobody has to match a forecast to a button.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Brass)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && (F->bMarch || F->bStorm) && !F->bResolved ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->NowLabel : FText::GetEmpty(); })
									]
									// The balance of strength between them, each side's flag at its end of the bar (Brian, 21 November
									// 2026). The flags stand 8 taller than the bar, so 4 is taken off the space above the row and 4 off
									// the space under it: the card is as tall as it was, and nothing else on it moves.
									+ SVerticalBox::Slot().AutoHeight().Padding(TAttribute<FMargin>::CreateLambda([Face]() { const FFaceOff* F = Face(); return FMargin(0.0f, F && (F->bMarch || F->bStorm) && !F->bResolved ? 2.0f : 16.0f, 0.0f, 0.0f); }))
									[
										// (a pick that cannot go has no forecast, and no bar: the UI director, 17 November)
										SNew(SHorizontalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->Alone.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
										[
											BarFlag(true)
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SBox).HeightOverride(10.0f)
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot()
												.FillWidth(TAttribute<float>::CreateLambda([Face]() { const FFaceOff* F = Face(); return FMath::Clamp(F ? F->Balance : 0.5f, 0.02f, 0.98f); }))
												[
													SNew(SBorder).BorderImage(&BarBrush)
													.BorderBackgroundColor_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F ? F->OurColor : FLinearColor::Gray); })
												]
												+ SHorizontalBox::Slot()
												.FillWidth(TAttribute<float>::CreateLambda([Face]() { const FFaceOff* F = Face(); return 1.0f - FMath::Clamp(F ? F->Balance : 0.5f, 0.02f, 0.98f); }))
												.Padding(3.0f, 0.0f, 0.0f, 0.0f)
												[
													SNew(SBorder).BorderImage(&BarBrush)
													.BorderBackgroundColor_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F ? F->TheirColor : FLinearColor::Gray); })
												]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
										[
											BarFlag(false)
										]
									]
									// (a pick that cannot go has no bar row above this line, so it keeps the 7 it had)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(TAttribute<FMargin>::CreateLambda([Face]() { const FFaceOff* F = Face(); return FMargin(0.0f, F && !F->Alone.IsEmpty() ? 7.0f : 3.0f, 0.0f, 0.0f); }))
									[
										// (check 19: a long place's name folds the line at its dot, and the dot goes)
										SNew(SPhraseText).Face(Font(TEXT("Bold"), 13)).Ink(Cream).Justify(ETextJustify::Center).Guess(840.0f)
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->OddsLine : FText::GetEmpty(); })
									]
									// (Rules 84.1.) The first forecast's sentence, opening with the game's own verdict.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(40.0f, 4.0f, 40.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center).LineHeightPercentage(1.2f)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && (F->bMarch || F->bStorm) && !F->bResolved && !F->NowSays.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->NowSays : FText::GetEmpty(); })
									]
									// The second forecast, on the same scale under the first: the same assault made ready, a month later.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && (F->bMarch || F->bStorm) && !F->bResolved && (!F->LaterOdds.IsEmpty() || !F->LaterSays.IsEmpty()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SBox).HeightOverride(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FSlateColor(Brass.CopyWithNewOpacity(0.35f)))]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Brass)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->LaterLabel : FText::GetEmpty(); })
										]
										// (the same flags at its ends, so that the two bars keep one scale; the same 4 and 4)
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
										[
											SNew(SHorizontalBox)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->LaterOdds.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
											[
												BarFlag(true)
											]
											+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
											[
												SNew(SBox).HeightOverride(10.0f)
												[
													SNew(SHorizontalBox)
													+ SHorizontalBox::Slot()
													.FillWidth(TAttribute<float>::CreateLambda([Face]() { const FFaceOff* F = Face(); return FMath::Clamp(F ? F->LaterBalance : 0.5f, 0.02f, 0.98f); }))
													[
														SNew(SBorder).BorderImage(&BarBrush)
														.BorderBackgroundColor_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F ? F->OurColor : FLinearColor::Gray); })
													]
													+ SHorizontalBox::Slot()
													.FillWidth(TAttribute<float>::CreateLambda([Face]() { const FFaceOff* F = Face(); return 1.0f - FMath::Clamp(F ? F->LaterBalance : 0.5f, 0.02f, 0.98f); }))
													.Padding(3.0f, 0.0f, 0.0f, 0.0f)
													[
														SNew(SBorder).BorderImage(&BarBrush)
														.BorderBackgroundColor_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F ? F->TheirColor : FLinearColor::Gray); })
													]
												]
											]
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
											[
												BarFlag(false)
											]
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 3.0f, 0.0f, 0.0f)
										[
											SNew(SPhraseText).Face(Font(TEXT("Bold"), 13)).Ink(Cream).Justify(ETextJustify::Center).Guess(840.0f)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->LaterOdds.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->LaterOdds : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(40.0f, 4.0f, 40.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center).LineHeightPercentage(1.2f)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->LaterSays : FText::GetEmpty(); })
										]
									]
									// (Version 63) The bill before the order: what it is likely to cost each side in men, either way.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(40.0f, 6.0f, 40.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center).LineHeightPercentage(1.2f)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->Cost.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Cost : FText::GetEmpty(); })
									]
									// (Version 84, ruling 174; ruling 203 for its place) WHAT YOU FACE: after the verdict and what it should cost, before the prize.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SFaceBlock).Player(Controller)
									]
									// The prize: what taking the place is worth.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40.0f, 8.0f, 40.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(BrightBrass).AutoWrapText(true).Justification(ETextJustify::Center)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->Prize.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Prize : FText::GetEmpty(); })
									]

									// (Version 63) The staff has brought in more corps than were picked: said, with the way back to the pick.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40.0f, 12.0f, 40.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && F->bWidened ? EVisibility::Visible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Opened : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 7.0f)).IsFocusable(false)
											.ToolTipText(FText::FromString(TEXT("Sends in only the corps you had in hand when you ordered the attack.")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->SendOnlyPicked(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("ONLY THE CORPS I PICKED"))).Font(Font(TEXT("Bold"), 10, 140)).ColorAndOpacity(BrightBrass)
											]
										]
									]

									// More corps stand in reach than were picked: what sending everyone would come to.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40.0f, 12.0f, 40.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && F->MassIds.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Mass : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 7.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->SendEveryone(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("SEND EVERYONE WHO CAN REACH"))).Font(Font(TEXT("Bold"), 10, 140)).ColorAndOpacity(BrightBrass)
											]
										]
									]

									// (Version 64) Soften the line first: a probe by one corps, or the bombers on its fieldworks.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && F->bSoften ? EVisibility::Visible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("SOFTEN THE LINE FIRST"))).Font(SmallCaps()).ColorAndOpacity(Brass)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
											.Text_Lambda([Face]()
											{
												const FFaceOff* F = Face();
												return !F ? FText::GetEmpty() : F->SoftenDone.IsEmpty() ? F->SoftenHead : FText::FromString(F->SoftenHead.ToString() + TEXT("  ") + F->SoftenDone.ToString());
											})
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
											[
												SNew(SBox).MinDesiredWidth(210.0f)
												[
													SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 6.0f)).IsFocusable(false).HAlign(HAlign_Center)
													.IsEnabled_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bProbeOk; })
													.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ProbeLine(); } return FReply::Handled(); })
													[
														SNew(STextBlock).Font(SmallCaps())
														.ColorAndOpacity_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F && F->bProbeOk ? BrightBrass : Faint); })
														.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ProbeLabel : FText::GetEmpty(); })
													]
												]
											]
											+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ProbeText : FText::GetEmpty(); })
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
										[
											SNew(SHorizontalBox)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bBomb ? EVisibility::Visible : EVisibility::Collapsed; })
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
											[
												SNew(SBox).MinDesiredWidth(210.0f)
												[
													SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 6.0f)).IsFocusable(false).HAlign(HAlign_Center)
													.IsEnabled_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bBombOk; })
													.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->BombLine(); } return FReply::Handled(); })
													[
														SNew(STextBlock).Text(FText::FromString(TEXT("BOMB THE LINE"))).Font(SmallCaps())
														.ColorAndOpacity_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F && F->bBombOk ? BrightBrass : Faint); })
													]
												]
											]
											+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->BombText : FText::GetEmpty(); })
											]
										]
									]

									// The Battle Ticket: what the map says about this fight.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("THE BATTLE TICKET"))).Font(SmallCaps()).ColorAndOpacity(Brass)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->Alone.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
									[
											// (Version 63) A long ticket opens folded to the lines that matter most.
										SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.3f)
										.Text_Lambda([this, Face]()
										{
											const FFaceOff* F = Face();
											return !F ? FText::GetEmpty() : F->TicketHidden > 0 && Controller.IsValid() && !Controller->IsTicketAll() ? F->TicketKey : F->Ticket;
										})
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->TicketHidden > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ToggleTicket(); } return FReply::Handled(); })
										[
											SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Brass)
											.Text_Lambda([this, Face]()
											{
												const FFaceOff* F = Face();
												return FText::FromString(!F ? FString() : Controller.IsValid() && Controller->IsTicketAll() ? FString(TEXT("THE MAIN LINES ONLY")) : FString::Printf(TEXT("SHOW ALL %d LINES"), F->TicketLines));
											})
										]
									]

										]
									]

									// (The UI director, 17 November.) Why the pick cannot go, in the rules' sentence, over the foot: it
									// stays in sight however the card scrolls, and SEND THEM IN carries it on its hover too.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 18.0f, 12.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->Alone.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Alone : FText::GetEmpty(); })
									]
									// What to do about it. (The foot stands at the right: CALL IT OFF, then the order.)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 20.0f, 12.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											// (release 2; Brian, 22 November 21:21Z: asked before each campaign battle, FIGHT IT YOURSELF as the
											// general or SEND THEM IN and let the generals decide)
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->CanFightLive() ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText(FText::FromString(TEXT("Fight this battle yourself as the general: place your units, then order them in real time. SEND THEM IN lets your generals decide it.")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->FightLive(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("FIGHT IT YOURSELF"))).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(BrightBrass)
											]
										]
										// (The foot's places never change: CALL IT OFF, then SEND THEM IN, then the siege's order where there is one. Only the light moves.)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											// (...and the light is on CALL IT OFF, which takes Enter as well as Esc.)
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->bMarch && !F->bStorm && F->CallOffLit() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseFaceOff(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("CALL IT OFF"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), BrightBrass)]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), BrightBrass)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											// (the UI director, ruling 183: on the storm's card CALL IT OFF's place stays empty, with no dimmed button;
											// and where the light is on CALL IT OFF, the lit one stands for it)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bStorm && !F->bResolved ? EVisibility::Hidden : F && F->CallOffLit() && !F->bResolved && !F->bMarch ? EVisibility::Collapsed : EVisibility::Visible; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseFaceOff(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(Cream)
													.Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bResolved ? TEXT("CONTINUE") : TEXT("CALL IT OFF")); })
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													SNew(SBox).Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
													[
														KeyCap(TEXT("ESC"), Cream)
													]
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													// (CONTINUE takes Enter and Space. The builder's night picture 41b: its key cap stands clear of
													// the word, as every other cap does.)
													SNew(SBox).Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bResolved ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
													[
														KeyCap(TEXT("ENTER"), Cream)
													]
												]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											Aimed(TEXT("send"), SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->bMarch && !F->bStorm && !F->CallOffLit() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmAttack(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bSally ? TEXT("SALLY OUT") : TEXT("SEND THEM IN")); }).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													// (a card that opened by itself takes no Enter, and shows no key: ruling 183)
													SNew(SBox).Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bArrival ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
													[
														KeyCap(TEXT("ENTER"), BrightBrass)
													]
												]
											])
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											// (A long shot or worse: SEND THEM IN keeps its place as a plain button that takes a click only.
											// A pick that cannot go: it keeps its place dimmed, takes no click, and its hover says why.)
											Aimed(TEXT("send"), SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->bMarch && !F->bStorm && F->CallOffLit() ? EVisibility::Visible : EVisibility::Collapsed; })
											.IsEnabled_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->Alone.IsEmpty(); })
											.ToolTipText_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Alone : FText::GetEmpty(); })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmAttack(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bSally ? TEXT("SALLY OUT") : TEXT("SEND THEM IN")); }).Font(Font(TEXT("Bold"), 11, 140))
												.ColorAndOpacity_Lambda([Face]() { const FFaceOff* F = Face(); return FSlateColor(F && !F->Alone.IsEmpty() ? Faint : Cream); })
											])
										]
										// (Rules 84.1; rulings 177(b) and 178(c).) Before a march on a guarded town: the two orders in the
										// forecasts' order, after CALL IT OFF. They never change places: only the light moves, to the
										// order its own forecast favours.
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											Aimed(TEXT("siege"), SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && (F->bMarch || F->bStorm) && !F->bFavourSiege ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("Every corps of yours round the town goes in.") : TEXT("The army marches in and goes at the town as it arrives: a hasty assault, off the march.")); })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmMarch(true); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("SEND THEM IN"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													// (a card that opened by itself takes no Enter, and shows no key: ruling 183)
													SNew(SBox).Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bArrival ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
													[
														KeyCap(TEXT("ENTER"), BrightBrass)
													]
												]
											])
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && (F->bMarch || F->bStorm) && F->bFavourSiege ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("Every corps of yours round the town goes in.") : TEXT("The army marches in and goes at the town as it arrives: a hasty assault, off the march.")); })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmMarch(true); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("SEND THEM IN"))).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(Cream)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											Aimed(TEXT("siege"), SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && (F->bMarch || F->bStorm) && F->bFavourSiege ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("The siege goes on. Nothing changes this month.") : TEXT("The army marches in and closes round the town. Nothing is risked this month, and next month the assault goes in made ready.")); })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmMarch(false); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
													.Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("KEEP UP THE SIEGE") : TEXT("LAY SIEGE FIRST")); })
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													SNew(SBox).Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bArrival ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
													[
														KeyCap(TEXT("ENTER"), BrightBrass)
													]
												]
											])
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && (F->bMarch || F->bStorm) && !F->bFavourSiege ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("The siege goes on. Nothing changes this month.") : TEXT("The army marches in and closes round the town. Nothing is risked this month, and next month the assault goes in made ready.")); })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmMarch(false); } return FReply::Handled(); })
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(Cream)
												.Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bStorm ? TEXT("KEEP UP THE SIEGE") : TEXT("LAY SIEGE FIRST")); })
											]
										]
									]
								]
							]
						]
					]

					// A page over the table: a unit's service record, or the roll of honour.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetDocument().bValid ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(820.0f).MaxDesiredHeight(780.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(34.0f, 24.0f, 22.0f, 24.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream).AutoWrapText(true)
										.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetDocument().Title) : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										SAssignNew(PageRibbons, SBox)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 12.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.2f)
										.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetDocument().Subtitle) : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SScrollBox)
										+ SScrollBox::Slot().Padding(0.0f, 0.0f, 14.0f, 0.0f)
										[
											SAssignNew(PageSections, SVerticalBox)
										]
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 12.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										// A room that leads on from this page: from the roll of honour, what can be made of the honours at home.
										+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(20.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetDocument().Door.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenHeroes(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(BrightBrass)
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetDocument().DoorLabel) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseDocument(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Cream)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), Cream)] // (item 6: the way out shows its key)
											]
										]
									]
								]
							]
						]
					]

					// The arsenal: everything that can be ordered.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsArsenalOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1320.0f).HeightOverride(800.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(28.0f, 20.0f, 24.0f, 20.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("THE ARSENAL"))).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 24))
												.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->GetArsenal().Money < 0 ? Alarm : Cream); })
												.Text_Lambda([this]()
												{
													return Controller.IsValid() ? FText::FromString(FString::Printf(TEXT("%s in the treasury to spend"), *Cash(Controller->GetArsenal().Money))) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([this]() { return Controller.IsValid() ? ArsenalSummary(Controller->GetArsenal()) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseArsenal(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									// (rules version 66: who runs this desk, and the way to change hands)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										DeskBar(TEXT("build"))
									]
									// (Rules 84.3.3; the art director's look brief, section 10.) Lesson 10's deed is done here, so its
									// note is laid across the room on every screen, under the head and the desk's row and above the
									// columns, which move down by the strip and 12. The strip's room is made as the room opens and
									// kept until it closes (the closing note, due next, takes it), unless he puts the note away. While
									// the room is kept the note inside is Hidden, never Collapsed, when it is not laid there: a box with
									// a collapsed child asks for nothing, min height or not, and the columns would jump. A collapsed box
									// is asked only for its visibility, so the kept height is let go there.
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox).Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
										.Visibility_Lambda([this]()
										{
											if (Controller.IsValid() && Controller->IsArsenalStrip())
											{
												return EVisibility::Visible;
											}
											ArsenalStripHigh = 0.0f;
											return EVisibility::Collapsed;
										})
										.MinDesiredHeight_Lambda([this]()
										{
											if (ArsenalStrip.IsValid())
											{
												ArsenalStripHigh = FMath::Max(ArsenalStripHigh, static_cast<float>(ArsenalStrip->GetDesiredSize().Y) + 12.0f);
											}
											return FOptionalSize(ArsenalStripHigh);
										})
										[
											SAssignNew(ArsenalStrip, SStaffNote).Controller(Controller).Form(2).Scale(1.0f)
											.Visibility_Lambda([this]()
											{
												if (!Controller.IsValid() || !Controller->IsArsenalStrip())
												{
													return EVisibility::Collapsed;
												}
												// (laid here, leaving too, so the stamp's fade draws; otherwise it holds the room unseen)
												return bLessonInLaid ? EVisibility::Visible : EVisibility::Hidden;
											})
										]
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SAssignNew(ArsenalColumns, SHorizontalBox)
									]
								]
							]
						]
					]

					// The ledger: the country's books.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsLedgerOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1120.0f).HeightOverride(760.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() && Controller->GetSession() ? Controller->GetLedgerNationTag() : FString()); })
											]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
												.Text_Lambda([this]()
												{
													const FLedger* Books = Controller.IsValid() ? &Controller->GetLedger() : nullptr;
													return Books ? FText::FromString(FString::Printf(TEXT("THE LEDGER  \u00B7  %s  \u00B7  %s"), *Books->Nation.ToUpper(), *Books->Date.ToUpper())) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26))
												.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->GetLedger().Money < 0 ? Alarm : Cream); })
												.Text_Lambda([this]()
												{
													const FLedger* Books = Controller.IsValid() ? &Controller->GetLedger() : nullptr;
													return Books ? FText::FromString(FString::Printf(TEXT("%s in the treasury"), *Cash(Books->Money))) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([this]() { return Controller.IsValid() ? LedgerSummary(Controller->GetLedger()) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseLedger(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 18.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 22.0f, 0.0f)
										[
											SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(LedgerIn, SVerticalBox)]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 22.0f, 0.0f)
										[
											SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(LedgerOut, SVerticalBox)]
										]
										+ SHorizontalBox::Slot().FillWidth(1.15f)
										[
											SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(LedgerProvinces, SVerticalBox)]
										]
									]
								]
							]
						]
					]

					// A room the rules set out themselves: the secret service, the last act.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f, 10.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsPageOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							// (the raid card is laid out to the art director's sheet, about 1176 by 932: the room is larger for it,
							// and keeps only its title and CLOSE at its head)
							// (The art director's brief: the month's card shrinks to fit the screen before it would scroll. A scale
							// box that only ever scales down: every other page has a size that fits, and is left as it is.)
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
							[
							SAssignNew(PageCardBox, SBox).WidthOverride_Lambda([this]() { return FOptionalSize(IsMonthCard() ? 1300.0f : IsRaidCard() ? 1180.0f : 1100.0f); }).HeightOverride_Lambda([this]() { return IsMonthCard() ? FOptionalSize() : FOptionalSize(IsRaidCard() ? 934.0f : 780.0f); })
							// (the month's card is as tall as what it has to tell: a quiet month is a small card)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
											// (the month's card has its own title row and its own buttons)
											.Visibility_Lambda([this]() { return IsMonthCard() ? EVisibility::Collapsed : EVisibility::Visible; })
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											.Visibility_Lambda([this]() { return IsRaidCard() ? EVisibility::Collapsed : EVisibility::Visible; })
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() && Controller->GetSession() ? Controller->GetLedgerNationTag() : FString()); })
											]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetPage().Title) : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
												.Visibility_Lambda([this]() { return IsRaidCard() ? EVisibility::Collapsed : EVisibility::Visible; })
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetPage().Headline) : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Visibility_Lambda([this]() { return IsRaidCard() ? EVisibility::Collapsed : EVisibility::Visible; })
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetPage().Note) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ClosePage(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									// (Rules 84.3.3) The staff's note laid in the card, under its head, where the card leaves no margin
									// for the slip: it is never hidden.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										SNew(SStaffNote).Controller(Controller).Form(2).Scale(1.0f)
										.Visibility_Lambda([this]() { return !IsMonthCard() && bLessonInLaid && Controller.IsValid() && Controller->HasLessonNote() ? EVisibility::Visible : EVisibility::Collapsed; })
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(PageBody, SVerticalBox)]
									]
									// (what a page pins to its foot stays on the screen whatever scrolls above it: the raid card's buttons)
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(PageFoot, SVerticalBox)]
								]
							]
							]
						]
					]

					// The generals: who commands what, who waits, and the corps that have nobody.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsGeneralsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1220.0f).HeightOverride(800.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() && Controller->GetSession() ? Controller->GetLedgerNationTag() : FString()); })
											]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
												.Text_Lambda([this]()
												{
													const FGeneralStaff* List = Controller.IsValid() ? &Controller->GetGenerals() : nullptr;
													return List ? FText::FromString(FString::Printf(TEXT("THE GENERALS  \u00B7  %s  \u00B7  %s"), *List->Nation.ToUpper(), *List->Date.ToUpper())) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
												.Text_Lambda([this]()
												{
													const FGeneralStaff* List = Controller.IsValid() ? &Controller->GetGenerals() : nullptr;
													return List ? FText::FromString(FString::Printf(TEXT("%d in command, %d waiting, %d corps without a general"), List->Commanding, List->Waiting, List->Vacant.Num())) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([this]()
												{
													const FGeneralStaff* List = Controller.IsValid() ? &Controller->GetGenerals() : nullptr;
													return List ? FText::FromString(List->Note + FString::Printf(TEXT(" You have %d political capital."), List->Capital)) : FText::GetEmpty();
												})
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseGenerals(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.6f).Padding(0.0f, 0.0f, 22.0f, 0.0f)
										[
											SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)[SAssignNew(GeneralsList, SVerticalBox)]
										]
										+ SHorizontalBox::Slot().FillWidth(0.9f)
										[
											SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(GeneralsVacant, SVerticalBox)]
										]
									]
								]
							]
						]
					]

					// The research tree as a wall chart (the grand campaign): the whole screen, with the map dimmed behind.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsResearchOpen() && Controller->GetResearch().bTree && ChartLayout().bGood ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							// (fitted to the screen whatever its size: the chart is never cut and never scrolls)
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
							[
								SNew(SBox).WidthOverride(1572.0f).HeightOverride(976.0f)
								[
									SNew(SResearchChart).Player(Controller)
								]
							]
						]
					]

					// The design offices: every branch of the armed forces, and the teams at work on it.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsResearchOpen() && !(Controller->GetResearch().bTree && ChartLayout().bGood) ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1220.0f).HeightOverride(800.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() && Controller->GetSession() ? Controller->GetLedgerNationTag() : FString()); })
											]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
												.Text_Lambda([this]()
												{
													const FResearch* Labs = Controller.IsValid() ? &Controller->GetResearch() : nullptr;
													return Labs ? FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s  \u00B7  %s"), Labs->bTree ? TEXT("THE LABORATORIES") : TEXT("THE DESIGN OFFICES"), *Labs->Nation.ToUpper(), *Labs->Date.ToUpper())) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26))
												.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->GetResearch().Free > 0 ? BrightBrass : Cream); })
												.Text_Lambda([this]()
												{
													const FResearch* Labs = Controller.IsValid() ? &Controller->GetResearch() : nullptr;
													if (!Labs || !Labs->bOwn)
													{
														return FText::FromString(TEXT("No design offices of its own"));
													}
													if (Labs->bTree)
													{
														return FText::FromString(Labs->Free > 0
															? FString::Printf(TEXT("%d research team%s, %d idle  \u00B7  %d of %d technologies"), Labs->Teams, Labs->Teams == 1 ? TEXT("") : TEXT("s"), Labs->Free, Labs->Done, Labs->Total)
															: FString::Printf(TEXT("%d research team%s, all at work  \u00B7  %d of %d technologies"), Labs->Teams, Labs->Teams == 1 ? TEXT("") : TEXT("s"), Labs->Done, Labs->Total));
													}
													return FText::FromString(Labs->Free > 0
														? FString::Printf(TEXT("%d design teams, %d idle"), Labs->Teams, Labs->Free)
														: FString::Printf(TEXT("%d design teams, all at work"), Labs->Teams));
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetResearch().Note) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseResearch(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									// (rules version 66: who runs this desk, and the way to change hands)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										DeskBar(TEXT("research"))
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.75f).Padding(0.0f, 0.0f, 22.0f, 0.0f)
										[
											SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)[SAssignNew(ResearchFields, SVerticalBox)]
										]
										+ SHorizontalBox::Slot().FillWidth(0.75f)
										[
											SNew(SScrollBox)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetResearch().bTree ? EVisibility::Collapsed : EVisibility::Visible; })
											+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(ResearchOthers, SVerticalBox)]
										]
									]
								]
							]
						]
					]

					// The foreign office: the powers at war, and every country at peace on the track between the blocs.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsDiplomacyOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1180.0f).HeightOverride(780.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() && Controller->GetSession() ? Controller->GetLedgerNationTag() : FString()); })
											]
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Brass)
												.Text_Lambda([this]()
												{
													const FDiplomacy* Office = Controller.IsValid() ? &Controller->GetDiplomacy() : nullptr;
													return Office ? FText::FromString(FString::Printf(TEXT("THE FOREIGN OFFICE  \u00B7  %s  \u00B7  %s"), *Office->Nation.ToUpper(), *Office->Date.ToUpper())) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
												.Text_Lambda([this]()
												{
													const FDiplomacy* Office = Controller.IsValid() ? &Controller->GetDiplomacy() : nullptr;
													return Office ? FText::FromString(FString::Printf(TEXT("%d political capital"), Office->Capital)) : FText::GetEmpty();
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
												.Text_Lambda([this]() { return Controller.IsValid() ? DiplomacySummary(Controller->GetDiplomacy()) : FText::GetEmpty(); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 9.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseDiplomacy(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
									]
									// (rules version 66: who runs this desk, and the way to change hands)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										DeskBar(TEXT("diplomacy"))
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 18.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(0.95f).Padding(0.0f, 0.0f, 26.0f, 0.0f)
										[
											SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)[SAssignNew(DiplomacyPowers, SVerticalBox)]
										]
										+ SHorizontalBox::Slot().FillWidth(1.6f)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DiplomacyHead, SVerticalBox)]
											+ SVerticalBox::Slot().FillHeight(1.0f)
											[
												SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(DiplomacyRows, SVerticalBox)]
											]
										]
									]
								]
							]
						]
					]

					// (Rules 84.3.3; the art director's look brief, section 6.) The Guide's first page: the ten lessons, over
					// the veil, above any card left open under it. GUIDE or H opens it in a war he leads; the stud, Esc or H
					// puts it away.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsLessonGuideOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SLessonGuide).Controller(Controller)
						]
					]

					// (Rules 84.3.3, FIRST COMMAND; the art director's look brief, ruling 147.) The staff's note: at its home
					// under the date card, at its second home under the small map, or as the slip in the left margin of a
					// card that veils the map. Over every card and room, under the films and the great moments.
					// (Its pointer on a button or a card's line first, under the note: see SLessonPointer.)
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SLessonPointer).Visibility(EVisibility::HitTestInvisible)
						.Frame_Lambda([this]() { return PointerFrame; })
					]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
					.Padding(TAttribute<FMargin>::CreateLambda([this]() { return FMargin(static_cast<float>(LessonAt.X), static_cast<float>(LessonAt.Y), 0.0f, 0.0f); }))
					[
						SAssignNew(LessonNoteWidget, SStaffNote).Controller(Controller).Scale(0.9f)
						.Form_Lambda([this]() { return bLessonSlip ? 1 : 0; })
						.SlipWide_Lambda([this]() { return LessonSlipWide; })
						.FromRight_Lambda([this]() { return LessonHome == 1 && !bLessonSlip; })
						.Visibility_Lambda([this]() { return bLessonShown ? EVisibility::Visible : EVisibility::Collapsed; })
					]

					// The newsreel: the map behind is in black and white; a black band above carries the masthead and one
					// below the story being told. A click goes on to the next story.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ClearBrush).Padding(FMargin(0.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetNewsreel().bOn && Controller->GetNewsreel().Kind != 2 ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) { if (Controller.IsValid()) { Controller->NextNewsCard(); } return FReply::Handled(); })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBorder).BorderImage(&FilmBrush).Padding(FMargin(48.0f, 26.0f, 48.0f, 22.0f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 30, 420)).ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.9f))
										.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && Controller->GetNewsreel().Kind == 1 ? TEXT("THE SITUATION") : TEXT("THEATER NEWS")); })
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 16, 260)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.68f))
										.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetNewsreel().Date.ToUpper()) : FText::GetEmpty(); })
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SSpacer)]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBorder).BorderImage(&FilmBrush).Padding(FMargin(48.0f, 22.0f, 48.0f, 20.0f))
								[
									SNew(SBox).MinDesiredHeight(150.0f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 28, 160)).ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.93f))
												.Text_Lambda([this]()
												{
													if (!Controller.IsValid())
													{
														return FText::GetEmpty();
													}
													const FNewsreelShow& Reel = Controller->GetNewsreel();
													return FText::FromString(Reel.Cards.IsValidIndex(Reel.Index) ? Reel.Cards[Reel.Index].Title : FString(Reel.Kind == 1 ? TEXT("YOUR COMMAND") : TEXT("THE MONTH IN REVIEW")));
												})
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 18)).ColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.8f)).AutoWrapText(true).LineHeightPercentage(1.2f)
											.Text_Lambda([this]()
											{
												if (!Controller.IsValid())
												{
													return FText::GetEmpty();
												}
												const FNewsreelShow& Reel = Controller->GetNewsreel();
												return FText::FromString(Reel.Cards.IsValidIndex(Reel.Index) ? Reel.Cards[Reel.Index].Text : FString(Reel.Kind == 1 ? TEXT("The war as it stands on the day you take over.") : TEXT("The war as it stood when the month closed.")));
											})
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.48f))
											.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && Controller->GetNewsreel().Kind == 1
												? TEXT("Click: go on    Space: skip the flight    MENU, then OPTIONS, turns it off")
												: TEXT("Click: the next story    Space: skip the newsreel    K: no more newsreels")); })
										]
									]
								]
							]
						]
					]

					// The title card that opens a year: the year in great figures over the darkened table, and how the
					// player stands as it begins. A click or Space goes on.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetNewsreel().bOn && Controller->GetNewsreel().Kind == 2 ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) { if (Controller.IsValid()) { Controller->NextNewsCard(); } return FReply::Handled(); })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 13, 420)).ColorAndOpacity(Brass)
								.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetNewsreel().Year.Kicker.ToUpper()) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 132, 120)).ColorAndOpacity(Cream)
								.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetNewsreel().Year.Year) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(SBox).WidthOverride(420.0f).HeightOverride(2.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Brass)]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Regular"), 30, 160)).ColorAndOpacity(BrightBrass)
								.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetNewsreel().Year.Title.ToUpper()) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(860.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 16)).ColorAndOpacity(FLinearColor(0.86f, 0.84f, 0.78f)).AutoWrapText(true).Justification(ETextJustify::Center).LineHeightPercentage(1.45f)
									.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(FString::Join(Controller->GetNewsreel().Year.Lines, TEXT("\n"))) : FText::GetEmpty(); })
								]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 30.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
								.Text(FText::FromString(TEXT("Click or Space to go on")))
							]
						]
					]

					// A great moment: a nation eliminated or liberated, told across the whole screen under its flag.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetAnnouncement() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) { if (Controller.IsValid()) { Controller->DismissAnnouncement(); } return FReply::Handled(); })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 18.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 300)).ColorAndOpacity(Brass)
								.Text_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return Told ? FText::FromString(Told->Kicker.ToUpper()) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(SBox).WidthOverride(300.0f).HeightOverride(200.0f)
								[
									SNew(SImage).Image_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return FlagBrush(Told ? Told->Tag : FString()); })
								]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 30.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 40, 200))
								.ColorAndOpacity_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return FSlateColor(Told && Told->bGood ? BrightBrass : Alarm); })
								.Text_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return Told ? FText::FromString(Told->Title) : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(760.0f).HAlign(HAlign_Center)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 17)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center)
									.Text_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return Told ? FText::FromString(Told->Text) : FText::GetEmpty(); })
								]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 12.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(760.0f).HAlign(HAlign_Center)
								[
									SNew(STextBlock).Font(Font(TEXT("Italic"), 14)).ColorAndOpacity(Muted).AutoWrapText(true).Justification(ETextJustify::Center)
									.Text_Lambda([this]() { const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr; return Told ? FText::FromString(Told->More) : FText::GetEmpty(); })
								]
							]
							// Three figures for the foot: who did it, how much ground, when.
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 24.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 160)).ColorAndOpacity(Cream)
								.Text_Lambda([this]()
								{
									const FAnnouncement* Told = Controller.IsValid() ? Controller->GetAnnouncement() : nullptr;
									TArray<FString> Parts;
									for (int32 Index = 0; Told && Index < Told->FactLabels.Num() && Index < Told->FactValues.Num(); ++Index)
									{
										Parts.Add(FString::Printf(TEXT("%s:  %s"), *Told->FactLabels[Index].ToUpper(), *Told->FactValues[Index]));
									}
									return FText::FromString(FString::Join(Parts, TEXT("          ")));
								})
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 30.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted)
								.Text(FText::FromString(TEXT("Click, or press Space, to go on.")))
							]
						]
					]

					// (Rulings 88 and 90.) The question at the month's end: a small card over a half-dark veil. SHOW ME is the
					// brass one and takes Enter (and Tab); END THE MONTH ends it, with a click only (Rules 84.3.1: Enter
					// presses the lit button, and the lit button is the safe one); and a tick stops the asking.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetEndAsk().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(620.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f, 30.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("BEFORE THE MONTH ENDS"))).Font(Typed(9, true, 200)).ColorAndOpacity(Brass)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Serif(TEXT("Bold"), 22)).ColorAndOpacity(Cream).AutoWrapText(true)
										.Text_Lambda([this]()
										{
											// (Ruling 116: the rules' own headline, true of every army counted: an army that has fought
											// with one corps has had an order, but it still has moves left.)
											const FString Asked = Controller.IsValid() ? Controller->GetRoundQuestion() : FString();
											if (!Asked.IsEmpty())
											{
												return FText::FromString(Asked);
											}
											const int32 Count = Controller.IsValid() ? Controller->GetEndAsk().Count : 0;
											// (where some are armies raised this month, they are not all "at the front")
											const bool bAllAtFront = !Controller.IsValid() || Controller->GetEndAsk().Raised <= 0;
											return FText::FromString(Count == 1 ? FString(bAllAtFront ? TEXT("1 army at the front has no orders") : TEXT("1 newly raised army has no orders"))
												: bAllAtFront ? FString::Printf(TEXT("%d armies at the front have no orders"), Count) : FString::Printf(TEXT("%d armies have no orders"), Count));
										})
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.2f)
										.Text_Lambda([this]()
										{
											const int32 Raised = Controller.IsValid() ? Controller->GetEndAsk().Raised : 0;
											// (the headline says who they are; the line beneath says what it means and what is left out)
											if (Controller.IsValid() && !Controller->GetRoundQuestion().IsEmpty())
											{
												return FText::FromString(TEXT("No order stands for them, and they can still move or fight this month. Armies in the rear are not counted."));
											}
											return FText::FromString(Raised <= 0 ? FString(TEXT("They have not moved this month and no order stands for them. Armies in the rear are not counted."))
												: FString::Printf(TEXT("They have not moved this month and no order stands for them: %d newly raised, the rest at the front. Armies in the rear are not counted."), Raised));
										})
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&BrassStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.ToolTipText(FText::FromString(TEXT("Goes to the first of them. (Enter)")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerEndAsk(true); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("SHOW ME"))).Font(Font(TEXT("Bold"), 12, 220)).ColorAndOpacity(DeskInk)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), DeskInk)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.ToolTipText(FText::FromString(TEXT("Ends the month as it stands.")))
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerEndAsk(false); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("END THE MONTH"))).Font(Font(TEXT("Bold"), 12, 200)).ColorAndOpacity(Muted)
											]
										]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0.0f)).IsFocusable(false)
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ToggleEndAskWanted(); } return FReply::Handled(); })
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
											[
												SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
												[
													SNew(SBorder).BorderImage(&KeyCapBrush).BorderBackgroundColor(Muted).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
													[
														SNew(SBox)
														.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->IsEndAskWanted() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
														[
															SNew(SLineIcon).Kind(7).Size(11.0f).Weight(1.8f).Ink(BrightBrass)
														]
													]
												]
											]
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("Do not ask me this again. Settings brings it back."))).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)
											]
										]
									]
								]
							]
						]
					]

					// The question before a war: asked when an army is sent onto the ground of a country at peace.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							// (a landing's question carries the reckoning and the holding of the sea: it is broader, so that its lines do not
							// fold; the march-up card is the landing card's width, sheet 34's 880)
							// (The UI director, 18 November 2026: the landing card stood 822 of Brian's 893 and was the tallest in the
							// game. Its flag stands beside its title, 48 high, not above it; and should a card still be taller than
							// the window, its lines scroll inside it: the title, the verdict and the two buttons never leave the window.)
							SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(Controller.IsValid() && Controller->GetWarPrompt().bLanding ? 900.0f : Controller.IsValid() && Controller->GetWarPrompt().bApproach ? 880.0f : 640.0f); })
							.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(FMath::Max(520.0f, (ScreenRoot.IsValid() ? static_cast<float>(ScreenRoot->GetTickSpaceGeometry().GetLocalSize().Y) : 972.0f) - 44.0f)); })
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f, 30.0f, 24.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(SBox).WidthOverride(132.0f).HeightOverride(88.0f)
										.Visibility_Lambda([this]() { return Controller.IsValid() && (Controller->GetWarPrompt().bApproach || Controller->GetWarPrompt().bLanding) ? EVisibility::Collapsed : EVisibility::Visible; })
										[
											SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() ? Controller->GetWarPrompt().Tag : FString()); })
										]
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(TAttribute<FMargin>::CreateLambda([this]() { return FMargin(0.0f, Controller.IsValid() && Controller->GetWarPrompt().bLanding ? 0.0f : 16.0f, 0.0f, 0.0f); }))
									[
										SNew(SHorizontalBox)
										.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().bApproach ? EVisibility::Collapsed : EVisibility::Visible; })
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
										[
											SNew(SBox).WidthOverride(72.0f).HeightOverride(48.0f)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().bLanding ? EVisibility::Visible : EVisibility::Collapsed; })
											[
												SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() ? Controller->GetWarPrompt().Tag : FString()); })
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
											.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Title : FText::GetEmpty(); })
										]
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										// (Rules 84.5; sheet 34.) The march-up card stands in the flag's and the title's place.
										ApproachCard()
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										// (Rules version 74, the design director's ruling 24: a landing's headline speaks for the whole
										// venture and is never more cheerful than the worst line on the card. Red when the force can
										// land and then be cut off, amber when one man in four or more would be lost at sea.)
										SNew(STextBlock).Font(Font(TEXT("Bold"), 20)).AutoWrapText(true).Justification(ETextJustify::Center)
										.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().Verdict.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
										.ColorAndOpacity_Lambda([this]()
										{
											const int32 Tone = Controller.IsValid() ? Controller->GetWarPrompt().VerdictTone : 0;
											return FSlateColor(Tone == 3 ? Alarm : Tone == 2 ? FLinearColor(FColor::FromHex(TEXT("D9A441"))) : Tone == 1 ? FLinearColor(FColor::FromHex(TEXT("9DB37A"))) : Cream);
										})
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Verdict : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 13)).AutoWrapText(true).Justification(ETextJustify::Center).LineHeightPercentage(1.2f)
										.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().VerdictUnder.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
										.ColorAndOpacity_Lambda([this]()
										{
											const int32 Tone = Controller.IsValid() ? Controller->GetWarPrompt().UnderTone : 3;
											return FSlateColor(Tone == 2 ? FLinearColor(FColor::FromHex(TEXT("D9A441"))) : Alarm);
										})
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().VerdictUnder : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										// (what scrolls, where the card is taller than the window: the card's lines)
										SNew(SScrollBox).ScrollBarThickness(FVector2D(4.0, 4.0))
										+ SScrollBox::Slot()
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												// (a section's head, a line all in capitals, takes the card label's style: brass small capitals)
												SNew(SRichTextBlock).TextStyle(&CardPlain).DecoratorStyleSet(CardStyles.Get()).AutoWrapText(true).LineHeightPercentage(1.2f)
												.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().Body.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
												.Text_Lambda([this]() { return Controller.IsValid() ? CardMarkup(Controller->GetWarPrompt().Body.ToString()) : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
											[
												// (Rules version 83: the test of strength, the card's last note. Plain where the staff would
												// expect to hold, amber where it is an even thing, red where they would not expect the force to
												// be still ashore. It never stops him.)
												SNew(SRichTextBlock).TextStyle(&CardPlain).DecoratorStyleSet(CardStyles.Get()).AutoWrapText(true).LineHeightPercentage(1.2f)
												.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().Ashore.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
												.Text_Lambda([this]()
												{
													if (!Controller.IsValid())
													{
														return FText::GetEmpty();
													}
													const int32 Tone = Controller->GetWarPrompt().AshoreTone;
													return CardMarkup(Controller->GetWarPrompt().Ashore.ToString(), Tone == 3 ? TEXT("Red") : Tone == 2 ? TEXT("Amber") : nullptr);
												})
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.2f)
												.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().Last.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
												.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Last : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
											[
												// (rules version 73: the warning that the sea behind the beach could be closed, in red)
												SNew(STextBlock).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Alarm).AutoWrapText(true).LineHeightPercentage(1.2f)
												.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().Warning.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
												.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Warning : FText::GetEmpty(); })
											]
										]
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 22.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										// (The UI director's 84.3 check, item 6: one rule for every card. Enter presses the lit button,
										// and on a question the lit button is the safe one: before a declaration of war NOT NOW is lit and
										// takes Enter and Esc, and the declaration takes a click. The order cards, the landing and the
										// gate keep their order lit, and Enter gives it, as in the browser (the UI director, 17 November,
										// in place of ruling 138(1)'s words). The way out always shows ESC. The places never change:
										// only the light moves.)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().IsWarQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseWarPrompt(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
													.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Cancel : FText::GetEmpty(); })
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), BrightBrass)]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), BrightBrass)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().IsWarQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseWarPrompt(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
													.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Cancel : FText::GetEmpty(); })
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), Muted)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWarPrompt().IsWarQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmWar(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
													.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Confirm : FText::GetEmpty(); })
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
												[
													// (the lit order is given with Enter, the landing's and the gate's as well: its key cap says so)
													KeyCap(TEXT("ENTER"), BrightBrass)
												]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().IsWarQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmWar(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Cream)
												.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Confirm : FText::GetEmpty(); })
											]
										]
									]
								]
							]
						]
					]

					// The opening screen: where to begin and as whom.
					// (The UI director, 18 November 2026, on build 1596, and at 03:48, 03:53 and 04:00 on 23c9ebe, b604af6
					// and e09457e.) The card is the kit's page, card-lg: 1,240 wide, and as high as the screen less space-12
					// (48) above and below, its type at full size, so that under the title band (FIRST COMMAND on its own
					// row, CONTINUE at the end of the pages' row) the seven great powers, the three settings and BEGIN AS ...
					// all show without scrolling; the smaller nations take what height is left. (The give order of 08:57,
					// capped at 09:04, after 07:10 and the review of build 1628, item 4.) First the margins, only to bring
					// in a second whole row of the smaller nations, never a third: where the 48s show fewer and standing
					// up to 16 from the screen's top and foot shows two, the card stands as tall as the two rows and the MORE
					// plate's band under them, and the 48s give only that much. Then the list, down to one whole row and
					// its plate, never less; where even that is more than the screen less its 16s, the card is drawn
					// smaller, and never under 0.95 of its size; and only past that, on the smallest frame (1,280 by 720),
					// does the list go down to its plate alone. The summary at the foot is as tall for every nation, so what
					// the card needs does not change with the pick. (See LookAtList for what it needs.)
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(16.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
							[
							SAssignNew(OpeningCardBox, SBox).WidthOverride(1240.0f)
							.HeightOverride_Lambda([this]()
							{
								// (876 on a frame 972 high. Since build 1638 the frame keeps one size against the window
								// (FrameScale), so it stands 972 high in a window of any size, and the card is laid out
								// the same in each; only a window narrower than 4 to 3 draws it smaller, by its width.)
								const double High = ScreenRoot.IsValid() ? ScreenRoot->GetTickSpaceGeometry().GetLocalSize().Y : 972.0;
								// (never drawn under 0.95 of its size: past that the list is cut at its foot instead)
								const double Room = FMath::Max(High - 32.0, 1.0);
								return FOptionalSize(static_cast<float>(FMath::Max(High - 96.0, FMath::Min(static_cast<double>(OpeningNeed), Room / 0.95))));
							})
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(36.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SOverlay)
										+ SOverlay::Slot().HAlign(HAlign_Center)
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("THEATER COMMAND"))).Font(Font(TEXT("Bold"), 30, 420)).ColorAndOpacity(Cream)
										]
										// (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".)
										// SAVE and LOAD on the title band, at its right, clear of the title: SAVE over a war under way
										// keeps a copy of it and opens the saved wars; LOAD opens them, where there are any. (The row of
										// pages and CONTINUE below is full at Brian's frame: they are not put there.)
										+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
											[
												SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 6.0f)).IsFocusable(false)
												.ToolTipText(FText::FromString(TEXT("Keep a copy of this war as it stands, beside the one the game keeps by itself each month. Keep as many as you like.")))
												.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bCanReturn ? EVisibility::Visible : EVisibility::Collapsed; })
												.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->SaveWar(); } return FReply::Handled(); })
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("SAVE"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(BrightBrass)
												]
											]
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
											[
												SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 6.0f)).IsFocusable(false)
												.ToolTipText(FText::FromString(TEXT("The wars you have saved: load one, or throw one away.")))
												.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().SavedWars.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
												.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ShowSavedWars(); } return FReply::Handled(); })
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("LOAD"))).Font(Font(TEXT("Bold"), 11, 160))
													.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->GetOpening().Tab == 3 ? BrightBrass : Cream); })
												]
											]
										]
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("1939 TO 1945"))).Font(Font(TEXT("Bold"), 12, 520)).ColorAndOpacity(Brass)
									]

									// (Rules 84.3.3; ruling 141.) FIRST COMMAND on the title band, on its own row above the pages' row and
									// CONTINUE (the UI director, 04:00): lit with the kit's lamp, never brass, until the ten lessons are done; "New here? Learn as you play.", and then
									// "The ten lessons again." It begins the campaign the rules name for it, after ruling 130's question
									// where a campaign is saved.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 10.0f, 0.0f)
									[
										SNew(SOverlay)
										.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().Door.bValid ? EVisibility::Visible : EVisibility::Collapsed; })
										+ SOverlay::Slot()
										[
											SNew(SLampGlow).Visibility(EVisibility::HitTestInvisible)
											.Lit_Lambda([this]() { return Controller.IsValid() && !Controller->GetOpening().Door.bAllDone; })
										]
										+ SOverlay::Slot()
										[
											SNew(SBorder).Padding(FMargin(0.0f))
											.BorderImage_Lambda([this]() { return Controller.IsValid() && !Controller->GetOpening().Door.bAllDone ? &FirstDoorLit : &FirstDoorPlain; })
											[
												SNew(SButton).ButtonStyle(&PlateButton).ContentPadding(FMargin(24.0f, 7.0f, 24.0f, 8.0f)).IsFocusable(false)
												.ToolTipText(FText::FromString(TEXT("Germany, September 1939, at Cadet and Field command: your staff runs every desk but the armies, and ten staff notes teach the war as you play it.")))
												.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AskToBegin(4); } return FReply::Handled(); })
												[
													SNew(SVerticalBox)
													+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
													[
														SNew(STextBlock).Font(Serif(TEXT("Bold"), 17.0f, 80))
														.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && !Controller->GetOpening().Door.bAllDone ? FLinearColor(FColor::FromHex(TEXT("F4DEA8"))) : Cream); })
														.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetOpening().Door.Label.ToUpper()) : FText::GetEmpty(); })
													]
													+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 1.0f, 0.0f, 0.0f)
													[
														SNew(STextBlock).Font(Typed(8.5f, false, 120)).ColorAndOpacity(Cream.CopyWithNewOpacity(0.86f))
														.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetOpening().Door.Line.ToUpper()) : FText::GetEmpty(); })
													]
												]
											]
										]
									]

									// The pages (the grand campaign, the historical scenarios, the options), and at the row's right end the
									// way back to a campaign. (The UI director, 18 November 2026, 04:00, for the frame of Brian's Mac, 870
									// high, and so at every size, one layout.) CONTINUE joins the pages' row as a button of one line:
									// CONTINUE in typed capitals, then the campaign saved on this computer in the typed face of what is
									// said ("Germany, July 1940, free play"), on the brass edge as before; with nothing saved there is no
									// CONTINUE. BACK TO THE WAR, for the menu opened over a campaign under way, stands in the same place.
									// The pages stand centred in the room left of it.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 10.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
										[
											SAssignNew(OpeningTabs, SHorizontalBox)
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(20.0f, 8.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bCanReturn ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseMenu(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("BACK TO THE WAR"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), BrightBrass)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(20.0f, 8.0f)).IsFocusable(false)
											.Visibility_Lambda([this]()
											{
												return Controller.IsValid() && !Controller->GetOpening().bCanReturn && !Controller->GetOpening().SavedLine.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
											})
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ContinueCampaign(); } return FReply::Handled(); })
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("CONTINUE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
												[
													SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream)
													.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetOpening().SavedLine : FText::GetEmpty(); })
												]
											]
										]
									]
									// What the page offers. (The UI director, on build 1596.) The list stands above the settings, never
									// under them: its area ends 12 above the panel's top edge. Where it runs on below what shows, a plate
									// counts what is below ("24 MORE ▼"); a click on the plate scrolls it down by one row of cards. (03:48:
									// the full triangle, a size under the plate's type, so that it sits at the letters' height and not
									// above them. The small one is in neither the typewriter nor, for certain, the face of signs it falls
									// back on, and a sign no face has is drawn as an empty box.) (The UI director's review of build 1628,
									// items 4 and 5.) The list shows whole rows only, no half row: it is as high as the rows that fit
									// whole, and the plate sits in its own band under the last of them, never on a card; what room is
									// over stands under the plate (see LookAtList).
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SAssignNew(ListArea, SBox).VAlign(VAlign_Top)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(SBox)
												.HeightOverride_Lambda([this]()
												{
													// (until the list is first measured, the whole area: see LookAtList; and never quite nothing, so
													// that it is still laid out and measured where the list is down to its plate alone)
													return FOptionalSize(FMath::Max(ListViewHigh >= 0.0f ? ListViewHigh : ListArea.IsValid() ? static_cast<float>(ListArea->GetTickSpaceGeometry().GetLocalSize().Y) : 0.0f, 1.0f));
												})
												[
													SAssignNew(StartsScroll, SScrollBox)
													// (the checker's fault 2: a wheel event put back on its line has moved nothing, and would otherwise
													// pass on to the map under the card)
													.ConsumeMouseWheel(EConsumeMouseWheel::Always)
													.OnUserScrolled_Lambda([this](float Offset) { SnapList(Offset); })
													+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
													[
														SAssignNew(Starts, SVerticalBox)
													]
												]
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 12.0f, 0.0f)
											[
												SNew(SBox).HeightOverride(PlateBand).HAlign(HAlign_Center).VAlign(VAlign_Center)
												.Visibility_Lambda([this]() { return bListRunsOn ? EVisibility::Visible : EVisibility::Collapsed; })
												[
													SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(14.0f, 3.0f, 14.0f, 4.0f)).IsFocusable(false)
													.OnClicked_Lambda([this]() { ScrollListOn(); return FReply::Handled(); })
													[
														SNew(SHorizontalBox)
														+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
														[
															SNew(STextBlock).Font(Font(TEXT("Bold"), 10, 160)).ColorAndOpacity(BrightBrass)
															.Text_Lambda([this]()
															{
																return FText::FromString(ListBelow > 0 ? FString::Printf(TEXT("%d MORE"), ListBelow) : FString(TEXT("MORE")));
															})
														]
														// (on the letters' line: both lines are measured by the typewriter's face, so their feet meet)
														+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6.0f, 0.0f, 0.0f, 0.0f)
														[
															SNew(STextBlock).Font(Font(TEXT("Bold"), 9)).ColorAndOpacity(BrightBrass).Text(FText::FromString(TEXT("\u25BC")))
														]
													]
												]
											]
										]
									]
									// The country chosen for the grand campaign, and the word to begin.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 10.0f, 0.0f)
									[
										SAssignNew(OpeningFoot, SBox)
									]
								]
							]
							]
						]
					]

					// (Ruling 130.) Nothing replaces a saved campaign without first asking and naming what would be
					// lost. The question put before a new war begins, over the opening screen. Keeping the campaign
					// is the bright button.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bVisible && Controller->GetOpening().AskKind != 0 ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(Controller.IsValid() && Controller->GetOpening().AskKind == 5 ? 840.0f : 640.0f); })
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(36.0f, 26.0f))
								[
									// (Ruling 132(b): the design director's words. "This replaces your saved campaign: Germany,
									// March 1941.", and the two answers, "Keep it" and "Start the new one".)
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 15)).ColorAndOpacity(Cream).AutoWrapText(true).LineHeightPercentage(1.2f).Justification(ETextJustify::Center)
										.Text(FText::FromString(TEXT("This replaces your saved campaign:")))
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 17)).ColorAndOpacity(BrightBrass).AutoWrapText(true).Justification(ETextJustify::Center)
										.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetOpening().SavedLine.ToString() + TEXT(".")) : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 24.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerBegin(false); } return FReply::Handled(); })
											[
												// (the UI director's 84.3 check, item 6: the lit, safe answer shows both its keys)
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("KEEP IT"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), BrightBrass)]
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), BrightBrass)]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerBegin(true); } return FReply::Handled(); })
											[
												// (release 1.5.1: before a saved war is loaded over it, "LOAD ANYWAY")
												SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
												.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && Controller->GetOpening().AskKind == 5 ? TEXT("LOAD ANYWAY") : TEXT("START THE NEW ONE")); })
											]
										]
										// (Release 1.5.1.) Before a saved war is loaded over the campaign kept by itself, a third answer that
										// loses nothing: that campaign is kept as a saved war, and then the other is loaded.
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().AskKind == 5 ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerLoadKeeping(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("SAVE IT, THEN LOAD"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Cream)
											]
										]
									]
								]
							]
						]
					]
				]
				]
			];

			SetVisibility(EVisibility::SelfHitTestInvisible);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			UpdateBigCard();
			PlaceLessonNote();
			PlaceLessonPointer();
			LookAtList();
			// Where the pieces of the frame are now, for the plates and names that must keep out from under them. (A piece
			// counts only while it is on the screen, every box it stands in shown: one in a row that is put away, as the
			// bottom row is behind the start screen, keeps no rectangle.)
			FrameRects.Reset();
			for (const TSharedRef<SWidget>& Each : FramePieces)
			{
				if (IsShownNow(Each))
				{
					// (as it was last painted, in the window's own measure: the plates and the names are drawn in that)
					const FSlateRect Rect = Each->GetPaintSpaceGeometry().GetLayoutBoundingRect();
					if (Rect.GetSize().X > 1.0 && Rect.GetSize().Y > 1.0)
					{
						FrameRects.Add(Rect);
					}
				}
			}
			// (The UI director's 84.3 check, item 3.) And for the hand that frames an army's reach clear of them: the same pieces,
			// in the viewport's own pixels, which is how the map's places are put on the screen.
			if (Controller.IsValid())
			{
				FVector2D ViewSize(0.0, 0.0);
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(ViewSize);
				}
				const FVector2D Local = AllottedGeometry.GetLocalSize();
				TArray<FBox2D> OnScreen;
				if (Local.X > 1.0 && Local.Y > 1.0 && ViewSize.X > 1.0 && ViewSize.Y > 1.0)
				{
					const FVector2D Scale(ViewSize.X / Local.X, ViewSize.Y / Local.Y);
					for (const FSlateRect& Rect : FrameRects)
					{
						OnScreen.Add(FBox2D(AllottedGeometry.AbsoluteToLocal(FVector2D(Rect.Left, Rect.Top)) * Scale, AllottedGeometry.AbsoluteToLocal(FVector2D(Rect.Right, Rect.Bottom)) * Scale));
					}
				}
				Controller->SetFrameOnScreen(MoveTemp(OnScreen));
			}
			// The adviser's steps, ticked as they are done.
			if (Controller.IsValid() && AdviserSteps.IsValid() && Controller->GetAdviser().Revision != ShownAdviser)
			{
				const FAdviser& Adviser = Controller->GetAdviser();
				ShownAdviser = Adviser.Revision;
				AdviserSteps->ClearChildren();
				static const TCHAR* Steps[5] = {
					TEXT("Pick up an army: click one of your flags, or press Tab."),
					TEXT("Send it: right-click ground that lights up. Red ground is an attack, and you are asked first."),
					TEXT("Look at a place: click a town. THE PRIZE says what taking it is worth; RECRUIT raises troops."),
					TEXT("Click an airfield to plan a raid, or one of your fleets to send it to sea."),
					TEXT("End the month with Enter and watch the others move. Space hurries it."),
				};
				for (int32 Step = 0; Step < 5; ++Step)
				{
					const bool bDone = Adviser.bDone[Step];
					AdviserSteps->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(bDone ? TEXT("\u2713") : FString::Printf(TEXT("%d"), Step + 1))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(bDone ? FLinearColor(FColor::FromHex(TEXT("8FBF6A"))) : BrightBrass)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Steps[Step])).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(bDone ? Faint : Cream).AutoWrapText(true)
						]
					];
				}
			}
			// The order of play, while the month is replayed.
			if (Controller.IsValid() && WatchStrip.IsValid())
			{
				TArray<FString> Tags;
				int32 Now = INDEX_NONE;
				Controller->GetWatchOrder(Tags, Now);
				if (Tags != WatchShown || Now != WatchNow)
				{
					WatchShown = Tags;
					WatchNow = Now;
					WatchStrip->ClearChildren();
					// A long month: the flags either side of the one moving, so the strip keeps to the middle of the screen.
					const int32 First = FMath::Clamp(Now - 7, 0, FMath::Max(0, Tags.Num() - 15)), Last = FMath::Min(Tags.Num(), First + 15);
					for (int32 Index = First; Index < Last; ++Index)
					{
						const bool bNow = Index == Now;
						WatchStrip->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f)
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(bNow ? BrightBrass : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)).Padding(FMargin(bNow ? 2.0f : 0.0f))
							[
								SNew(SBox).WidthOverride(bNow ? 45.0f : 27.0f).HeightOverride(bNow ? 30.0f : 18.0f)
								[
									SNew(SImage).Image(FlagBrush(Tags[Index])).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, bNow ? 1.0f : (Index < Now ? 0.35f : 0.75f)))
								]
							]
						];
					}
				}
			}
			// The province section of the territory panel.
			if (Controller.IsValid() && ProvinceBox.IsValid() && Controller->GetRegionPanel().Revision != ShownProvince)
			{
				const FRegionPanelInfo& Info = Controller->GetRegionPanel();
				ShownProvince = Info.Revision;
				ProvinceBox->ClearChildren();
				if (!Info.DeclareTag.IsEmpty())
				{
					ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("DIPLOMACY"))).Font(SmallCaps()).ColorAndOpacity(Brass)
					];
					ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Info.DiplomacyLine).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
					ProvinceBox->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(14.0f, 6.0f)).IsFocusable(false)
						.OnClicked_Lambda([this]()
						{
							if (Controller.IsValid())
							{
								Controller->OfferWarOnSelected();
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("DECLARE WAR"))).Font(Font(TEXT("Bold"), 10, 160)).ColorAndOpacity(FLinearColor(FColor::FromHex(TEXT("E08A6A"))))
						]
					];
				}
				if (Info.Options.Num() > 0 || Info.Forming.Num() > 0 || !Info.ProvinceNote.IsEmpty() || !Info.Holdings.IsEmpty())
				{
					// What can be ordered here is sorted under two tabs, so the panel stays small: troops to raise,
					// and work on the ground itself (forts, bridges, railways, engineers). A fleet's or an air
					// fleet's orders are one list.
					TArray<const FProvinceOption*> Recruits, Works;
					int32 ProvinceTab = Controller->GetProvinceTab();
					const bool bProvince = Info.OptionsTitle.ToString() == TEXT("PROVINCE");
					for (const FProvinceOption& Option : Info.Options)
					{
						(bProvince && (Option.Action == TEXT("fort") || Option.Action.StartsWith(TEXT("work:"))) ? Works : Recruits).Add(&Option);
					}
					if (!bProvince)
					{
						ProvinceTab = 1;
					}
					else if ((ProvinceTab == 1 && Recruits.Num() == 0) || (ProvinceTab == 2 && Works.Num() == 0))
					{
						ProvinceTab = 0;
					}
					auto TabButton = [this, &ProvinceTab](const FString& Label, int32 Tab)
					{
						const bool bOn = ProvinceTab == Tab;
						return SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(10.0f, 4.0f)).IsFocusable(false)
							.OnClicked_Lambda([this, Tab]() { if (Controller.IsValid()) { Controller->SetProvinceTab(Tab); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(bOn ? BrightBrass : Muted)
							];
					};
					if (bProvince && (Recruits.Num() > 0 || Works.Num() > 0))
					{
						TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
						Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)[TabButton(TEXT("PROVINCE"), 0)];
						if (Recruits.Num() > 0)
						{
							Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)[TabButton(FString::Printf(TEXT("RECRUIT  %d"), Recruits.Num()), 1)];
						}
						if (Works.Num() > 0)
						{
							// (on ground that is not the player's there is nothing to build: what is offered there is an order)
							Tabs->AddSlot().AutoWidth()[TabButton(FString::Printf(TEXT("%s  %d"), Recruits.Num() > 0 ? TEXT("WORKS") : TEXT("ORDERS"), Works.Num()), 2)];
						}
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[Tabs];
					}
					else
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.OptionsTitle).Font(SmallCaps()).ColorAndOpacity(Brass)
						];
					}
					// (Ruling 46: "Forming here". A header that sums it up, then a line an order: its sign, what it is, a bar
					// of the months done out of the months needed, and the month it is ready.)
					if (Info.Forming.Num() > 0)
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("FORMING HERE"))).Font(SmallCaps()).ColorAndOpacity(Brass)
						];
						if (!Info.FormingHead.IsEmpty())
						{
							ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Info.FormingHead)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
							];
						}
					}
					for (const FOrderLine& Order : Info.Forming)
					{
						const FSlateBrush* Sign = Order.Sign.IsEmpty() ? nullptr : OrderSignBrush(Order.Sign);
						const FString What = Order.Sign.IsEmpty() ? Order.Name : Order.Words + TEXT("  \u00B7  ") + Order.Name;
						const FString When = Order.Months <= 0 ? FString(TEXT("ready now")) : FString::Printf(TEXT("ready in %d month%s (%s)"), Order.Months, Order.Months == 1 ? TEXT("") : TEXT("s"), *Order.Due);
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 8.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(24.0f).HeightOverride(16.0f)
								[
									Sign ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Sign).ColorAndOpacity(Order.bStar ? BrightBrass : Cream)) : SNullWidget::NullWidget
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(BrightBrass).AutoWrapText(true)
									.Text(FText::FromString(What + TEXT(", ") + When))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(SBox).HeightOverride(4.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(Order.Share, 0.001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Brass)]
										+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(1.0f - Order.Share, 0.001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
									]
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(8.0f, 3.0f)).IsFocusable(false)
								.Visibility(Order.bCanCancel ? EVisibility::Visible : EVisibility::Collapsed)
								.OnClicked_Lambda([this, Id = Order.Id]()
								{
									if (Controller.IsValid())
									{
										Controller->CancelOrder(Id);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("TAKE BACK"))).Font(SmallCaps()).ColorAndOpacity(Muted)
								]
							]
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.Prize.IsEmpty())
					{
						// The prize: what the player gets for taking the place, or has from holding it.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)
						[
							SNew(SBox).ToolTipText(Info.PrizeTip)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Text(FText::FromString(Info.bPrizeToTake ? TEXT("THE PRIZE") : TEXT("WHAT IT PAYS"))).Font(SmallCaps()).ColorAndOpacity(Brass)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(Info.Prize).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Info.bPrizeToTake ? BrightBrass : Cream).AutoWrapText(true)
								]
							]
						];
					}
					if (ProvinceTab == 0 || !bProvince)
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.Holdings).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.LeftWorksText.IsEmpty())
					{
						// (Version 64) Fieldworks standing empty here, for a corps of the player's side to take over.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.LeftWorksTitle).Font(SmallCaps()).ColorAndOpacity(Brass).AutoWrapText(true)
						];
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.LeftWorksText).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.12f)
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.PocketText.IsEmpty())
					{
						// (Version 63) Cut off from the rest of its country: what its troops live on, and for how long.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.PocketTitle).Font(SmallCaps()).ColorAndOpacity(Info.bPocketSpent ? Alarm : BrightBrass).AutoWrapText(true)
						];
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.PocketText).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.12f)
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.SiegeText.IsEmpty())
					{
						// The town as a fortress, and how its siege stands.
						// (The UI director, 18 November 2026: in the heads' brass when the player is the besieger; red only on
						// a town of his own, whose stores run out by a date.)
						const bool bBesieged = Info.bSiegeOwn;
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.SiegeTitle).Font(SmallCaps()).ColorAndOpacity(bBesieged ? Alarm : Brass)
						];
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.SiegeText).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.12f)
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.DirectiveText.IsEmpty())
					{
						// A directive of the war cabinet names this province.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.DirectiveText).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(BrightBrass).AutoWrapText(true).LineHeightPercentage(1.12f)
						];
					}
					if ((ProvinceTab == 0 || !bProvince) && !Info.PlacesText.IsEmpty())
					{
						// The places of the province that are prizes of their own, and what happens to them if it is lost.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.PlacesTitle).Font(SmallCaps()).ColorAndOpacity(Brass)
						];
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.PlacesText).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.12f)
						];
					}
					if (ProvinceTab == 0 && bProvince && Info.Plots.Num() > 0)
					{
						// The town: its building plots as a row of lots, each with what stands on it, and the way to its plan.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Brass)
							.Text(FText::FromString(FString::Printf(TEXT("THE TOWN  \u00B7  %d PLOT%s%s"), Info.Plots.Num(), Info.Plots.Num() == 1 ? TEXT("") : TEXT("S"),
								Info.PlotsFree > 0 ? *FString::Printf(TEXT(", %d FREE"), Info.PlotsFree) : TEXT(""))))
						];
						TSharedRef<SWrapBox> Lots = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
						for (const FString& Plot : Info.Plots)
						{
							const bool bFree = Plot == TEXT("Free plot");
							Lots->AddSlot()
							[
								SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(7.0f, 3.0f))
								[
									SNew(STextBlock).Text(FText::FromString(Plot)).Font(Font(bFree ? TEXT("Italic") : TEXT("Bold"), 9)).ColorAndOpacity(bFree ? Faint : Cream)
								]
							];
						}
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)[Lots];
						ProvinceBox->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(Info.bPlotsMine && Info.PlotsCanBuild > 0 ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(TEXT("The town plan: every plot, what stands on it, and everything that could be built here: commerce, farming, learning, training, medicine and administration.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenTownPlan(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(Info.bPlotsMine && Info.PlotsCanBuild > 0 ? BrightBrass : Muted)
								.Text(FText::FromString(Info.bPlotsMine && Info.PlotsCanBuild > 0 ? FString::Printf(TEXT("TOWN PLAN  \u00B7  %d TO BUILD"), Info.PlotsCanBuild) : FString(TEXT("TOWN PLAN"))))
							]
						];
					}
					if (!Info.ProvinceNote.IsEmpty() && ProvinceTab != 2)
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.ProvinceNote).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
						];
					}
					// Each thing that can be ordered is one short row: its name, and under it what it costs in a
					// single line. The rest, and why it cannot be ordered if it cannot, is in the tip.
					const TArray<const FProvinceOption*>& Shown = ProvinceTab == 2 ? Works : Recruits;
					if (ProvinceTab == 1 && bProvince)
					{
						// What can be raised here, as cards two abreast: a picture of what it is, its name, and what it
						// costs in money and months. A click orders it.
						TSharedPtr<SHorizontalBox> Row;
						int32 InRow = 0;
						for (const FProvinceOption* Each : Shown)
						{
							const FProvinceOption& Option = *Each;
							const FString Name = Option.Label.StartsWith(TEXT("Raise: ")) ? Option.Label.Mid(7) : Option.Label;
							const FString Tip = Option.Why.IsEmpty() ? FString::Printf(TEXT("%s\n%s"), *Option.Label, *Option.Detail) : FString::Printf(TEXT("%s\n%s\n%s"), *Option.Label, *Option.Detail, *Option.Why);
							const TSharedPtr<FSlateBrush> Portrait = PortraitOf(ACampaignMapActor::ModelFor(Info.OwnerTag, Option.Action));
							const TSharedRef<SWidget> Picture = Portrait.IsValid()
								? StaticCastSharedRef<SWidget>(SNew(SPortrait).Picture(Portrait).Size(FVector2D(66.0, 40.0)))
								: KindPicture(Option.Action == TEXT("heavy") ? FString(TEXT("arm")) : Option.Action, Option.bEnabled ? Cream : Faint);
							if (!Row.IsValid() || InRow == 2)
							{
								ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)[SAssignNew(Row, SHorizontalBox)];
								InRow = 0;
							}
							Row->AddSlot().FillWidth(1.0f).Padding(InRow == 0 ? 0.0f : 3.0f, 0.0f, InRow == 0 ? 3.0f : 0.0f, 0.0f)
							[
								SNew(SButton).ButtonStyle(Option.bEnabled ? &PlainButton : &LeftCard).ContentPadding(FMargin(5.0f, 5.0f)).IsFocusable(false)
								.ToolTipText(FText::FromString(Tip))
								.OnClicked_Lambda([this, Action = Option.Action, bCan = Option.bEnabled, Why = Option.Why]()
								{
									// (one that cannot be afforded can still be pressed, and says why: its price stays readable)
									if (Controller.IsValid())
									{
										if (bCan)
										{
											Controller->ProvinceAction(Action);
										}
										else
										{
											Controller->SayWhy(Why);
										}
									}
									return FReply::Handled();
								})
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
									[
										SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(2.0f))[Picture]
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock).Text(FText::FromString(Name.Replace(TEXT(" corps"), TEXT("")))).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Option.bEnabled ? Cream : Muted)
											.AutoWrapText(true) // (no order is cut mid-word: a long name takes a second line)
										]
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Option.bEnabled ? BrightBrass : Alarm.CopyWithNewOpacity(0.9f))
											.Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d mo"), *FText::AsNumber(Option.Cost).ToString(), Option.Months)))
										]
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted)
											.Text(FText::FromString(FString::Printf(TEXT("%d division%s"), Option.Divisions, Option.Divisions == 1 ? TEXT("") : TEXT("s"))))
										]
									]
								]
							];
							++InRow;
						}
						if (Row.IsValid() && InRow == 1)
						{
							Row->AddSlot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)[SNew(SSpacer)];
						}
					}
					else if (ProvinceTab != 0)
					{
						for (const FProvinceOption* Each : Shown)
						{
							const FProvinceOption& Option = *Each;
							const FString Label = Option.Label.StartsWith(TEXT("Raise: ")) ? Option.Label.Mid(7) : Option.Label;
							const FString Tip = Option.Why.IsEmpty() ? FString::Printf(TEXT("%s\n%s"), *Option.Label, *Option.Detail) : FString::Printf(TEXT("%s\n%s\n%s"), *Option.Label, *Option.Detail, *Option.Why);
							ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 4.0f)).IsFocusable(false).IsEnabled(Option.bEnabled)
								.ToolTipText(FText::FromString(Tip))
								.OnClicked_Lambda([this, Action = Option.Action]()
								{
									if (Controller.IsValid())
									{
										Controller->ProvinceAction(Action);
									}
									return FReply::Handled();
								})
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										// (Ruling of 17:16 UTC, 17 November: no order row is cut mid-word. The name and the reason
										// wrap, and the row grows to hold them: "It goes into the town as its garrison: it fig..." was cut.)
										SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Option.bEnabled ? Cream : Faint)
										.AutoWrapText(true)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
									[
										// (its facts are parted by dots, "2 divisions · costs 1,200 · ready in 3 months": check 19,
										// a fold falls at a dot and the dot goes)
										SNew(SPhraseText).Face(Font(TEXT("Regular"), 9)).Ink(Option.bEnabled ? Muted : Faint).Guess(340.0f)
										.Text(FText::FromString(Option.bEnabled || Option.Why.IsEmpty() ? Option.Detail : Option.Why))
									]
								]
							];
						}
					}
				}
			}

			// Lay out the page (a service record or the roll of honour) when it changes.
			if (Controller.IsValid() && PageSections.IsValid() && Controller->GetDocumentRevision() != ShownPage)
			{
				ShownPage = Controller->GetDocumentRevision();
				const FDocumentPage& Page = Controller->GetDocument();
				PageRibbons->SetContent(MakeRibbons(Page.Ribbons, 12));
				PageSections->ClearChildren();
				for (const FDocumentSection& Section : Page.Sections)
				{
					PageSections->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Section.Heading.ToUpper())).Font(Font(TEXT("Bold"), 10, 220)).ColorAndOpacity(Brass)
					];
					PageSections->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 18.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Section.Body)).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).LineHeightPercentage(1.25f)
					];
				}
			}

			// Set the arsenal out when it is opened, and again after every order.
			if (Controller.IsValid() && ArsenalColumns.IsValid() && Controller->IsArsenalOpen() && Controller->GetArsenal().Revision != ShownArsenal)
			{
				ShownArsenal = Controller->GetArsenal().Revision;
				LayOutArsenal(Controller->GetArsenal());
			}

			// Set a room of the rules' own out when it is opened, and again after anything is pressed in it.
			if (Controller.IsValid() && PageBody.IsValid() && Controller->IsPageOpen() && Controller->GetPage().Revision != ShownRoom)
			{
				ShownRoom = Controller->GetPage().Revision;
				LayOutPage(Controller->GetPage());
			}

			// Set the generals out when their list is opened, and again after every appointment.
			if (Controller.IsValid() && GeneralsList.IsValid() && Controller->IsGeneralsOpen() && Controller->GetGenerals().Revision != ShownGenerals)
			{
				ShownGenerals = Controller->GetGenerals().Revision;
				LayOutGenerals(Controller->GetGenerals());
			}

			// Set the design offices out when they are opened, and again whenever a team is moved.
			if (Controller.IsValid() && ResearchFields.IsValid() && Controller->IsResearchOpen() && Controller->GetResearch().Revision != ShownResearch)
			{
				ShownResearch = Controller->GetResearch().Revision;
				LayOutResearch(Controller->GetResearch());
			}

			// Set the foreign office out when it is opened, and again after every move made there.
			if (Controller.IsValid() && DiplomacyRows.IsValid() && Controller->IsDiplomacyOpen() && Controller->GetDiplomacy().Revision != ShownDiplomacy)
			{
				ShownDiplomacy = Controller->GetDiplomacy().Revision;
				LayOutDiplomacy(Controller->GetDiplomacy());
			}

			// Write the ledger out when it is opened.
			if (Controller.IsValid() && LedgerIn.IsValid() && Controller->IsLedgerOpen() && Controller->GetLedger().Revision != ShownLedger)
			{
				ShownLedger = Controller->GetLedger().Revision;
				LayOutLedger(Controller->GetLedger());
			}

			// Lay out the opening screen's choices when they change.
			if (Controller.IsValid() && Starts.IsValid() && Controller->GetOpening().Revision != ShownOpening)
			{
				ShownOpening = Controller->GetOpening().Revision;
				LayOutOpening(Controller->GetOpening());
				// (Release 1, the UI director, 22 November 2026, for Brian's "when you click something on the menu it
				// flickers".) This runs while the screen is being drawn, after Slate has measured it, so the card's new
				// lines would be drawn this frame with no size, all at the list's top, and set right only on the next: a
				// flash on every click. Measure the card again now, at the scale it is drawn at, so the frame that first
				// shows the new lines shows them in their places.
				if (OpeningCardBox.IsValid())
				{
					const float CardScale = OpeningCardBox->GetTickSpaceGeometry().Scale;
					OpeningCardBox->SlatePrepass(CardScale > 0.0f ? CardScale : 1.0f);
				}
			}

			// Lay the town's bar out again whenever it has changed.
			if (Controller.IsValid() && TownBarBox.IsValid() && Controller->GetTownBar().Revision != ShownTownBar)
			{
				ShownTownBar = Controller->GetTownBar().Revision;
				LayOutTownBar(Controller->GetTownBar());
			}

			// Deal the cards again whenever the army in hand has changed.
			if (Controller.IsValid() && Cards.IsValid() && Controller->GetArmyTray().Revision != ShownRevision)
			{
				ShownRevision = Controller->GetArmyTray().Revision;

				// The army's standing order for the day the enemy comes, and the railway.
				TrayOrders->ClearChildren();
				const FArmyTray& Army = Controller->GetArmyTray();
				// (Ruling 118; the art director's railway brief, section 2a.) One line of words, not a button: whether the
				// army can ride this month, in the rules' words. At its head a piece of railway: lit in the railway's
				// blue-white when it can, dark and ending in the red buffer bar when it cannot.
				if (TrayRail.IsValid())
				{
					TrayRail->ClearChildren();
					if (!Army.bLook && !Army.bFleet && !Army.RailLine.IsEmpty())
					{
						const FLinearColor RailLit(FColor(198, 228, 255)), RailDark(FColor(128, 116, 96)), Buffer(FColor(226, 62, 44));
						const FLinearColor& Metal = Army.bRailCan ? RailLit : RailDark;
						TSharedRef<SOverlay> Piece = SNew(SOverlay)
							+ SOverlay::Slot().VAlign(VAlign_Center)[SNew(SBox).HeightOverride(6.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(FColor(18, 20, 24)).CopyWithNewOpacity(0.8f))]]
							+ SOverlay::Slot().VAlign(VAlign_Center)[SNew(SBox).HeightOverride(2.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Metal)]];
						for (int32 K = 0; K < 4; ++K)
						{
							Piece->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(3.0f + 6.5f * K, 0.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(1.5f).HeightOverride(6.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Metal)]
							];
						}
						if (!Army.bRailCan)
						{
							Piece->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(3.0f).HeightOverride(9.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Buffer)]
							];
						}
						// (as many whole sentences as the line holds, never a sentence cut: the rest is in its tip)
						FString Shown = Army.RailLine;
						while (Shown.Len() > 104)
						{
							int32 Stop = INDEX_NONE;
							const FString Shorter = Shown.LeftChop(1);
							if (!Shorter.FindLastChar(TEXT('.'), Stop) || Stop < 20)
							{
								break;
							}
							Shown = Shown.Left(Stop + 1);
						}
						const bool bStrong = Army.bRailCan && !Army.RailStrong.IsEmpty() && Shown.StartsWith(Army.RailStrong);
						TrayRail->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 7.0f, 9.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(27.0f).HeightOverride(10.0f)[Piece]
						];
						if (bStrong)
						{
							TrayRail->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 7.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Army.RailStrong)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(RailLit)
							];
						}
						TrayRail->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 7.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(bStrong ? Shown.Mid(Army.RailStrong.Len()) : Shown)).Font(Font(TEXT("Regular"), 10))
							.ColorAndOpacity(Army.bRailCan ? FLinearColor(FColor(214, 206, 188)) : FLinearColor(FColor(162, 153, 135)))
							.ToolTipText(FText::FromString(Army.RailLine))
						];
					}
				}
				auto OrderButton = [this](const TCHAR* Label, bool bOn, TFunction<void()> Do)
				{
					return SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(bOn ? BrightBrass : Muted)
						];
				};
				if (Army.bFleet)
				{
					// A fleet in hand: what it does at sea, the way home, and a card for each ship. (1.5.2) Held where this month's
					// battle left it, or cut off beyond a shut strait, the orders refused are dimmed, each saying why, as its page does.
					TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("At sea:"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
					];
					TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						SNew(SBox).IsEnabled(Army.FleetSeaWhy.IsEmpty()).ToolTipText(FText::FromString(!Army.FleetSeaWhy.IsEmpty() ? Army.FleetSeaWhy : FString(TEXT("Seeks out the enemy's warships in its sea and guards the convoys that cross it."))))
						[
							OrderButton(TEXT("PATROL"), Army.FleetNext == TEXT("patrol"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("patrol")); } })
						]
					];
					TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Army.bFleetGrand ? 5.0f : 22.0f, 0.0f)
					[
						SNew(SBox).IsEnabled(Army.FleetSeaWhy.IsEmpty()).ToolTipText(FText::FromString(!Army.FleetSeaWhy.IsEmpty() ? Army.FleetSeaWhy : FString(TEXT("Goes for merchant ships and keeps clear of a fight with the enemy fleet."))))
						[
							OrderButton(TEXT("RAID SHIPPING"), Army.FleetNext == TEXT("raid"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("raid")); } })
						]
					];
					if (Army.bFleetGrand)
					{
						TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)
						[
							SNew(SBox).IsEnabled(Army.FleetSeaWhy.IsEmpty()).ToolTipText(FText::FromString(!Army.FleetSeaWhy.IsEmpty() ? Army.FleetSeaWhy : FString(TEXT("Its destroyers sweep the sea for submarines: the more destroyers, the more it sinks. The pointer over a sea says what hunting there would do."))))
							[
								OrderButton(TEXT("HUNT SUBMARINES"), Army.FleetNext == TEXT("hunt"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("hunt")); } })
							]
						];
					}
					TrayOrders->AddSlot().AutoWidth()
					[
						SNew(SBox).IsEnabled(Army.FleetHomeWhy.IsEmpty()).ToolTipText(FText::FromString(!Army.FleetHomeWhy.IsEmpty() ? Army.FleetHomeWhy : !Army.FleetHomeTip.IsEmpty() ? Army.FleetHomeTip : FString(TEXT("Safe from the enemy fleet, and takes no part at sea."))))
						[
							OrderButton(TEXT("RETURN TO HARBOUR"), Army.FleetStance == TEXT("port"), [this]() { if (Controller.IsValid()) { Controller->FleetToHarbour(); } })
						]
					];
					if (Army.bFleetGrand)
					{
						TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(TEXT("Everything this fleet could be told to do: every sea in reach with the enemy known there, shipping lanes to escort, coasts to shell, and splitting ships off or joining another fleet.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenFleetOrders(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("ORDERS: ESCORT, SHELL, SPLIT"))).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
							]
						];
					}
					if (Army.FleetAircraft > 0)
					{
						// A fleet with carriers: the way to its aircraft.
						TrayOrders->AddSlot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(TEXT("The fighters and strike aircraft its carriers bring. Send them to search a sea for the enemy, to strike a fleet your side has found, or to raid ships in a harbour.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenFleetWings(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
								.Text(FText::FromString(FString::Printf(TEXT("CARRIER AIRCRAFT  \u00B7  %d"), Army.FleetAircraft)))
							]
						];
					}
					if (!Army.FleetWarning.IsEmpty())
					{
						TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Army.FleetWarning).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Alarm)
						];
					}
					TrayOrders->AddSlot().FillWidth(1.0f)[SNew(SSpacer)];
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 8.0f, 0.0f)
					[
						OrderButton(TEXT("PUT DOWN"), false, [this]() { if (Controller.IsValid()) { Controller->PutDownFleet(); } })
					];

					Cards->ClearChildren();
					BigCardTargets.Reset(); // (the cards the bigger card answers to are noted afresh as they are made)
					BigCardRevision = -1;
					if (ACampaignMapActor* Table = Controller->GetMap())
					{
						if (UTextureRenderTarget2D* Stand = Table->GetShowpiece())
						{
							if (!ShowBrush.IsValid())
							{
								ShowBrush = MakeShared<FSlateBrush>();
								ShowBrush->ImageSize = FVector2D(420.0, 260.0);
								ShowBrush->DrawAs = ESlateBrushDrawType::Image;
							}
							ShowBrush->SetResourceObject(Stand);
							Cards->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(5.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SNew(SPortrait).Picture(ShowBrush).Size(FVector2D(194.0, 120.0))]
									+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 4.0f, 2.0f, 0.0f)
									[
										SNew(SBox).WidthOverride(190.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted).AutoWrapText(true)
											.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetShowTitle() : FText::GetEmpty(); })
										]
									]
								]
							];
						}
					}
					for (const FShipCard& Ship : Army.Ships)
					{
						const TSharedPtr<FSlateBrush> Portrait = PortraitOf(Ship.Model);
						const TSharedRef<SWidget> Picture = Portrait.IsValid()
							? StaticCastSharedRef<SWidget>(SNew(SPortrait).Picture(Portrait).Size(FVector2D(74.0, 45.0)))
							: StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(74.0f).HeightOverride(45.0f));
						Cards->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PickedCard).ContentPadding(FMargin(6.0f, 6.0f, 6.0f, 7.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(FString::Printf(TEXT("%s. %s"), *Ship.Name.ToString(), *Ship.Role.ToString())))
							.OnHovered_Lambda([this, Model = Ship.Model, Name = Ship.Name]() { if (Controller.IsValid()) { Controller->ShowShipModel(Model, Name); } })
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(4.0f))[Picture]]
								+ SVerticalBox::Slot().AutoHeight().Padding(1.0f, 5.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(Ship.Name).Font(Font(TEXT("Bold"), 9)).ColorAndOpacity(Cream)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(1.0f, 1.0f, 0.0f, 0.0f)
								[
									// What the class is for, in three words.
									SNew(SBox).WidthOverride(82.0f)
									[
										SNew(STextBlock).Text(Ship.Brief).Font(Font(TEXT("Regular"), 8)).ColorAndOpacity(Muted).AutoWrapText(true)
									]
								]
							]
						];
					}
				}
				else
				{
				// (an army only being looked at has no orders to give: its cards, and a way to put them away)
				if (!Army.bLook)
				{
				TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("If attacked:"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
				if (Controller.IsValid() && Controller->IsAutoDig())
				{
					// (Version 58, the grand campaign: whoever stands a month digs in unasked, so there are two orders, not three)
					TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						OrderButton(TEXT("STAND AND DIG IN"), Army.Posture != TEXT("back"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("stand")); } })
					];
				}
				else
				{
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					OrderButton(TEXT("STAND"), Army.Posture == TEXT("stand"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("stand")); } })
				];
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					OrderButton(TEXT("DIG IN"), Army.Posture == TEXT("dig"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("dig")); } })
				];
				}
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)
				[
					OrderButton(TEXT("FALL BACK"), Army.Posture == TEXT("back"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("back")); } })
				];
				TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Travel:"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
				// (Ruling 118: the BY RAIL switch is gone. Inside the march's reach a right-click marches; beyond it a
				// right-click takes the train. The line under the chips says what the trains will do.)
				TrayOrders->AddSlot().AutoWidth()
				[
					OrderButton(TEXT("BY SEA"), Army.bBySea, [this]() { if (Controller.IsValid()) { Controller->ToggleSea(); } })
				];
				// Drafts: taken as they come, or none, to keep the veterans together.
				TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).ToolTipText(FText::FromString(TEXT("Lit: these corps keep their veterans together and take no drafts, so they stay seasoned but do not recover their strength. Unlit: they take drafts as they come, their ranks fill, and the new men water down their experience.")))
					[
						OrderButton(TEXT("NO DRAFTS"), Army.bLean, [this]() { if (Controller.IsValid()) { Controller->ToggleDrafts(); } })
					]
				];
				// Allied corps beside the army that could be asked to march with it.
				if (!Army.JoinButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(TEXT("An ally's corps standing here can march with your army: it keeps its own flag and pay, but takes your orders, attacks beside your corps, and the ground you take together is yours. It goes home when you send it back, or if it is left a month with none of your troops beside it. It does not sail, dig in or get promoted.")))
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AskAlliesToJoin(); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.JoinButton).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
						]
					];
				}
				// (Brian, 18 November 2026.) The town's orders, on the army's own tray: storm the town it stands round (the
				// one lit button: it is the attack he was looking for), go into the town it stands in, or come out again.
				// (The UI director, 18 November 2026: one lit button on a card. A lit edge is the one thing the staff points
				// at, not a state: STORM is lit only while the staff's forecast favours the assault; GARRISON, TAKE THE
				// FIELD and FOLD WORN CORPS are plain. STAND AND DIG IN is lit as the chosen half of its pair.)
				if (!Army.StormButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(Army.bStormFavoured ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.ToolTipText(Army.StormTip)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->TrayTownOrder(0); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.StormButton).Font(SmallCaps()).ColorAndOpacity(Army.bStormFavoured ? BrightBrass : Muted)
						]
					];
				}
				if (!Army.GarrisonButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.ToolTipText(Army.GarrisonTip)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->TrayTownOrder(1); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.GarrisonButton).Font(SmallCaps()).ColorAndOpacity(Muted)
						]
					];
				}
				if (!Army.FieldButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(TEXT("The corps in hand leave the town and stand in the open country of the province again, where they can march and attack.")))
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->TrayTownOrder(2); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.FieldButton).Font(SmallCaps()).ColorAndOpacity(Muted)
						]
					];
				}
				// (Version 64) Two corps of the army worn thin: the card that makes one of them.
				if (!Army.FoldButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.ToolTipText(Army.FoldTip)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenFold(); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.FoldButton).Font(SmallCaps()).ColorAndOpacity(Muted)
						]
					];
				}
				}
				// The place clicked once and waiting for the word, and the way to let the army go.
				TrayOrders->AddSlot().FillWidth(1.0f)[SNew(SSpacer)];
				if (!Army.PendingButton.IsEmpty())
				{
					TrayOrders->AddSlot().AutoWidth().Padding(22.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(14.0f, 4.0f)).IsFocusable(false)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmPending(); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Army.PendingButton).Font(Font(TEXT("Bold"), 10, 140)).ColorAndOpacity(BrightBrass)
						]
					];
				}
				TrayOrders->AddSlot().AutoWidth().Padding(Army.PendingButton.IsEmpty() ? 22.0f : 0.0f, 0.0f, 8.0f, 0.0f)
				[
					OrderButton(Army.bLook ? TEXT("CLOSE") : TEXT("PUT DOWN"), false, [this, bLook = Army.bLook]() { if (Controller.IsValid()) { if (bLook) { Controller->StopLooking(); } else { Controller->PutDownArmy(); } } })
				];

				Cards->ClearChildren();
					BigCardTargets.Reset(); // (the cards the bigger card answers to are noted afresh as they are made)
					BigCardRevision = -1;
				// The model of the corps pointed at, large and turning, with its name under it.
				if (ACampaignMapActor* Table = Controller->GetMap())
				{
					if (UTextureRenderTarget2D* Stand = Table->GetShowpiece())
					{
						if (!ShowBrush.IsValid())
						{
							ShowBrush = MakeShared<FSlateBrush>();
							ShowBrush->ImageSize = FVector2D(420.0, 260.0);
							ShowBrush->DrawAs = ESlateBrushDrawType::Image;
						}
						ShowBrush->SetResourceObject(Stand);
						Cards->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(5.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(SPortrait).Picture(ShowBrush).Size(FVector2D(194.0, 120.0))]
								+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 4.0f, 2.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(190.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted).AutoWrapText(true)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetShowTitle() : FText::GetEmpty(); })
									]
								]
							]
						];
					}
				}
				for (const FTrayCard& Card : Controller->GetArmyTray().Cards)
				{
					Cards->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						MakeCard(Card)
					];
				}
				}
			}
		}

	private:
		/** Notes a widget as a piece of the frame: the towns' plates and the provinces' names are not drawn under it. */
		template <typename WidgetType>
		TSharedRef<WidgetType> Piece(TSharedRef<WidgetType> Widget)
		{
			FramePieces.Add(Widget);
			return Widget;
		}

		/**
		 * Where the news line may stand: the margins that keep it between the calendar on the left and the chips on the
		 * right, whatever their widths today; and, if asked, how wide that gap is.
		 */
		FMargin NoticeGap(float* OutWidth) const
		{
			// (The UI director, 18 November 2026, on the by-sea notice at Brian's window: its end ran under NEXT ARMY.
			// "Its width is bounded by the plates on either side, 16 clear of each, and it wraps inside." The plates'
			// edges are read where they stand this frame, in the frame's own measure: no window size, and no scale
			// of the screen's, comes into it, so no size of window or fineness of pixel can put it out.)
			const float Clear = 16.0f;
			if (!ScreenRoot.IsValid())
			{
				if (OutWidth)
				{
					*OutWidth = 800.0f;
				}
				return FMargin(24.0f, 112.0f, 24.0f, 0.0f);
			}
			const FGeometry& Frame = ScreenRoot->GetTickSpaceGeometry();
			const float Whole = static_cast<float>(Frame.GetLocalSize().X);
			auto Edges = [&Frame](const TWeakPtr<SWidget>& Kept, float& OutLeft, float& OutRight)
			{
				const TSharedPtr<SWidget> Widget = Kept.Pin();
				if (!Widget.IsValid() || !Widget->GetVisibility().IsVisible())
				{
					return false;
				}
				const FGeometry& Its = Widget->GetTickSpaceGeometry();
				if (Its.GetLocalSize().X < 1.0f || Its.GetLocalSize().Y < 1.0f)
				{
					return false;
				}
				OutLeft = static_cast<float>(Frame.AbsoluteToLocal(Its.GetAbsolutePosition()).X);
				OutRight = static_cast<float>(Frame.AbsoluteToLocal(Its.GetAbsolutePosition() + Its.GetAbsoluteSize()).X);
				return OutRight > OutLeft;
			};
			float From = 24.0f, To = Whole - 24.0f, Left = 0.0f, Right = 0.0f;
			if (Edges(TopLeftPiece, Left, Right))
			{
				From = FMath::Max(From, Right + Clear);
			}
			for (const TWeakPtr<SWidget>& Each : TopChips)
			{
				// (a chip of the right-hand column: whatever of it lies in the right half of the frame)
				if (Edges(Each, Left, Right) && Left > Whole * 0.5f)
				{
					To = FMath::Min(To, Left - Clear);
				}
			}
			To = FMath::Max(To, From + 292.0f); // (never narrower than a short line and its plate)
			if (OutWidth)
			{
				*OutWidth = To - From;
			}
			return FMargin(From, 112.0f, FMath::Max(Whole - To, 0.0f), 0.0f);
		}

		/** Whether a widget is on the screen now: it, and every box it stands in up to the screen's own, is shown. */
		bool IsShownNow(const TSharedRef<SWidget>& Widget) const
		{
			TSharedPtr<SWidget> At = Widget;
			for (int32 Guard = 0; At.IsValid() && Guard < 64; ++Guard)
			{
				if (!At->GetVisibility().IsVisible())
				{
					return false;
				}
				if (At.Get() == ScreenRoot.Get())
				{
					return true;
				}
				At = At->GetParentWidget();
			}
			return false;
		}

		/**
		 * Where the pointer's slip stands, in the screen's own measure. (The UI director, 18 November 2026, on build
		 * 1596: over North Carolina the slip ran under the right-hand panel and down into the army's tray, cut short.)
		 * It stands wholly inside the window and 8 clear of every panel, tray and plate of the frame (and of the staff's
		 * note, the bigger unit card and an agent in hand). It opens below and right of the pointer; where that would
		 * cross a panel it flips left, where it would cross the tray it flips above, and both where both; where it fits
		 * nowhere whole by the pointer, it stands in the clear map at the place nearest the pointer, never over it. A
		 * gate's slip opens under its mark, or over it, and never covers it (the art director's round 36).
		 */
		FMargin PlaceHoverTip() const
		{
			if (!Controller.IsValid() || Controller->GetHoverTip().IsEmpty())
			{
				return FMargin(0.0f); // (no slip, nothing to place)
			}
			FVector2D Size(1920.0, 1080.0);
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(Size);
			}
			// (The screen's units to a pixel of the window: this screen's own scale, and the engine's for the window's
			// size on top of it. In a window that is not 1,080 high the two differ, and by the first alone the box
			// stood off from the pointer: in the pictures' window, 7 parts in 100 up and to the left.)
			const double Scale = FrameScale(Size.Y) * FMath::Max(static_cast<double>(GetTickSpaceGeometry().Scale), 0.01);
			const FVector2D Screen = Size / Scale;
			const FVector2D At = Controller.IsValid() ? Controller->GetPointer() / Scale : FVector2D::ZeroVector;
			const FVector2D Tip = HoverTipBox.IsValid() ? HoverTipBox->GetDesiredSize() : FVector2D(300.0, 60.0);
			const double Clear = 8.0;
			// What it keeps clear of, each grown by the 8.
			TArray<FBox2D> Keep;
			for (const TSharedRef<SWidget>& Each : FramePieces)
			{
				if (IsShownNow(Each))
				{
					const FBox2D Box = InFrame(TWeakPtr<SWidget>(Each));
					if (Box.bIsValid)
					{
						Keep.Add(Box.ExpandBy(Clear));
					}
				}
			}
			if (bLessonShown && LessonNoteWidget.IsValid() && IsShownNow(LessonNoteWidget.ToSharedRef()))
			{
				const FBox2D Note = InFrame(LessonNoteWidget);
				if (Note.bIsValid)
				{
					Keep.Add(Note.ExpandBy(Clear));
				}
			}
			if (BigCardShow.IsValid() && IsShownNow(BigCardShow.ToSharedRef()))
			{
				const FBox2D Card = InFrame(BigCardShow);
				if (Card.bIsValid)
				{
					Keep.Add(Card.ExpandBy(Clear));
				}
			}
			// (The art director's review of the agent piece: the slip never covers him while he is in hand.)
			FBox2D Held(ForceInit);
			if (Controller.IsValid() && Controller->GetAgentInHandBox(Held))
			{
				Keep.Add(FBox2D(Held.Min / Scale, Held.Max / Scale).ExpandBy(Clear));
			}
			auto Whole = [&Keep, &Screen, &Tip, Clear](const FVector2D& Corner)
			{
				// (Measured a hair inside its own edges: a box that only touches another counts as crossing it, and a
				// slip set flush against a kept piece, the window's edge or a gate's mark, as every try is, is whole.)
				const FVector2D Hair(0.05, 0.05);
				const FBox2D Box(Corner + Hair, Corner + Tip - Hair);
				if (Box.Min.X < Clear || Box.Min.Y < Clear || Box.Max.X > Screen.X - Clear || Box.Max.Y > Screen.Y - Clear)
				{
					return false;
				}
				for (const FBox2D& Each : Keep)
				{
					if (Box.Intersect(Each))
					{
						return false;
					}
				}
				return true;
			};
			TArray<FVector2D, TInlineAllocator<4>> Tries;
			FVector2D Near = At; // (what the slip stands nearest to when it fits nowhere by its own rule)
			FBox2D Mark(ForceInit);
			if (Controller.IsValid() && Controller->GetTipAnchor(Mark))
			{
				// (the mark's place is in the screen's own units already) Under the plate, six clear, or over it.
				Keep.Add(Mark.ExpandBy(6.0));
				const double Left = FMath::Max(Mark.Min.X, Clear);
				Tries.Add(FVector2D(Left, Mark.Max.Y + 6.0));
				Tries.Add(FVector2D(Left, Mark.Min.Y - 6.0 - Tip.Y));
				Tries.Add(FVector2D(Mark.Max.X - Tip.X, Mark.Max.Y + 6.0));
				Tries.Add(FVector2D(Mark.Max.X - Tip.X, Mark.Min.Y - 6.0 - Tip.Y));
				Near = Mark.GetCenter();
			}
			else
			{
				const double Right = At.X + 18.0, Left = At.X - 14.0 - Tip.X, Below = At.Y + 20.0, Above = At.Y - 12.0 - Tip.Y;
				Tries.Add(FVector2D(Right, Below)); // below and right
				Tries.Add(FVector2D(Left, Below)); // flipped left, off a panel
				Tries.Add(FVector2D(Right, Above)); // flipped above, off the tray
				Tries.Add(FVector2D(Left, Above)); // both
			}
			for (const FVector2D& Corner : Tries)
			{
				if (Whole(Corner))
				{
					return FMargin(static_cast<float>(Corner.X), static_cast<float>(Corner.Y), 0.0f, 0.0f);
				}
			}
			// Nowhere whole by its rule: in the clear map nearest the pointer. Only a place against the window's edge, a
			// kept piece's edge or the rule's own lines can be the nearest such place, so only those are tried.
			TArray<double> Xs = { Clear, Screen.X - Clear - Tip.X }, Ys = { Clear, Screen.Y - Clear - Tip.Y };
			for (const FVector2D& Corner : Tries)
			{
				Xs.Add(Corner.X);
				Ys.Add(Corner.Y);
			}
			for (const FBox2D& Each : Keep)
			{
				Xs.Add(Each.Max.X);
				Xs.Add(Each.Min.X - Tip.X);
				Ys.Add(Each.Max.Y);
				Ys.Add(Each.Min.Y - Tip.Y);
			}
			double Best = TNumericLimits<double>::Max();
			FVector2D Chosen(FMath::Clamp(At.X + 18.0, Clear, FMath::Max(Screen.X - Clear - Tip.X, Clear)), FMath::Clamp(At.Y + 20.0, Clear, FMath::Max(Screen.Y - Clear - Tip.Y, Clear)));
			for (const double X : Xs)
			{
				for (const double Y : Ys)
				{
					const FVector2D Corner(X, Y);
					const FBox2D Box(Corner, Corner + Tip);
					if (Box.ExpandBy(4.0).IsInside(At) || !Whole(Corner))
					{
						continue; // (never over the pointer, nor anywhere it is not whole)
					}
					const double Far = FMath::Min(FMath::Min(FVector2D::DistSquared(Near, Box.Min), FVector2D::DistSquared(Near, Box.Max)),
						FMath::Min(FVector2D::DistSquared(Near, FVector2D(Box.Min.X, Box.Max.Y)), FVector2D::DistSquared(Near, FVector2D(Box.Max.X, Box.Min.Y))));
					if (Far < Best)
					{
						Best = Far;
						Chosen = Corner;
					}
				}
			}
			return FMargin(static_cast<float>(Chosen.X), static_cast<float>(Chosen.Y), 0.0f, 0.0f);
		}

		/** A piece of the frame as it stands in the frame's own measure (900 high); invalid when it is not on the screen. */
		FBox2D InFrame(const TWeakPtr<SWidget>& Kept) const
		{
			const TSharedPtr<SWidget> Widget = Kept.Pin();
			if (!ScreenRoot.IsValid() || !Widget.IsValid() || !Widget->GetVisibility().IsVisible())
			{
				return FBox2D(ForceInit);
			}
			const FGeometry& Root = ScreenRoot->GetTickSpaceGeometry();
			const FGeometry& Own = Widget->GetTickSpaceGeometry();
			const FVector2D At = Own.GetAbsolutePosition(), Size = Own.GetAbsoluteSize();
			if (Size.X < 1.0 || Size.Y < 1.0)
			{
				return FBox2D(ForceInit);
			}
			const FVector2D Min = Root.AbsoluteToLocal(At), Max = Root.AbsoluteToLocal(At + Size);
			return FBox2D(Min, Max);
		}

		/**
		 * (Rules 84.3.3; the art director's look brief, section 2, and ruling 147(a).) Where the staff's note stands this
		 * frame. Beside a card that veils the map it is the slip in the card's left margin, 148 wide at most and never
		 * touching the card; where the margin is too narrow (under 112), it is laid in the card under its head. Otherwise
		 * it is at home under the date card, or at its second home under the small map when the army in hand or the
		 * flag it points at would lie under the first; where both would, it is the slip at the left edge. It chooses as it
		 * comes and does not move while he reads. The measures are the brief's pixels of the 1600 by 1000 frame, at 0.9.
		 */
		void PlaceLessonNote()
		{
			bLessonShown = bLessonSlip = bLessonInLaid = false;
			if (!Controller.IsValid() || !ScreenRoot.IsValid())
			{
				return;
			}
			Controller->SetLessonNoteHidden(false);
			FLessonNote Gone;
			float Faded = 0.0f;
			const bool bNote = Controller->HasLessonNote();
			if (!bNote && !Controller->GetLessonLeaving(Gone, Faded))
			{
				return;
			}
			const double K = 0.9;
			const FVector2D Screen = ScreenRoot->GetTickSpaceGeometry().GetLocalSize();
			if (Controller->IsArsenalStrip() && !Controller->IsPageOpen() && !Controller->GetDocument().bValid)
			{
				bLessonInLaid = true; // (laid across the arsenal: the brief, section 10)
				return;
			}
			if (Controller->IsLessonBesideCard())
			{
				// The card's left edge and top: a room's card as it is drawn (it may be scaled down to fit), the others
				// centred at their own widths.
				double Left = 0.0, Top = 14.0 * K;
				const FBox2D Card = Controller->IsPageOpen() ? InFrame(PageCardBox) : FBox2D(ForceInit);
				if (Card.bIsValid)
				{
					Left = Card.Min.X;
					Top = FMath::Max(Top, Card.Min.Y);
				}
				else
				{
					const FFaceOff& Face = Controller->GetFaceOff();
					const double CardWide = Controller->IsPageOpen() ? (IsMonthCard() ? 1300.0 : IsRaidCard() ? 1180.0 : 1100.0)
						: Controller->GetDocument().bValid ? 820.0 : Controller->IsArsenalOpen() ? 1320.0 : Controller->IsLedgerOpen() ? 1120.0
						: Controller->IsGeneralsOpen() ? 1220.0 : Controller->IsDiplomacyOpen() ? 1180.0 : Face.bVisible ? 912.0
						: Controller->IsResearchOpen() && !(Controller->GetResearch().bTree && ChartLayout().bGood) ? 1220.0 : Screen.X;
					const double High = Face.bVisible ? 560.0 : 780.0;
					Left = (Screen.X - CardWide) * 0.5;
					Top = FMath::Max(Top, (Screen.Y - High) * 0.5);
				}
				const double Room = Left - 14.0 * K;
				if (Room >= 112.0 * K)
				{
					bLessonShown = bLessonSlip = true;
					LessonSlipWide = static_cast<float>(FMath::Min(148.0, Room / K));
					LessonAt = FVector2D(7.0 * K, Top);
				}
				else if (Controller->IsPageOpen())
				{
					bLessonInLaid = true; // (laid in the card under its head instead: never hidden)
				}
				else
				{
					// (the art director's brief, section 10: in a room with no margin, any other note waits unseen, and is
					// back at its home the moment the room closes; a stamp owed lands then)
					Controller->SetLessonNoteHidden(true);
				}
				return;
			}
			// At home: the paper's top 19 under the date card, the clip standing 8 above it; the second home's right
			// edge with the small map's, 13 under it.
			const FBox2D Date = InFrame(TopLeftPiece), Small = InFrame(SmallMapPiece);
			const FVector2D Home1(25.0 * K, (Date.bIsValid ? Date.Max.Y : 247.0 * K) + 11.0 * K);
			const FVector2D Home2(Small.bIsValid ? Small.Max.X - 340.0 * K : Screen.X - 365.0 * K, (Small.bIsValid ? Small.Max.Y : 607.0 * K) + 5.0 * K);
			if (bNote && Controller->GetLessonArrival() != LessonPlacedArrival)
			{
				LessonPlacedArrival = Controller->GetLessonArrival();
				FVector2D Size(340.0 * K, 240.0 * K);
				if (LessonNoteWidget.IsValid())
				{
					Size.Y = FMath::Max(Size.Y * 0.5, static_cast<double>(LessonNoteWidget->GetDesiredSize().Y));
				}
				FVector2D Pixels(1920.0, 1080.0);
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(Pixels);
				}
				// (window pixels to this screen's units: the frame's own scale and, on top of it, Unreal's for the window's size. The pre-checker's find of 18 November 2026: by the first alone these came out two thirds too large in Brian's window.)
				const double Scale = FrameScale(Pixels.Y) * FMath::Max(static_cast<double>(GetTickSpaceGeometry().Scale), 0.01);
				TArray<FBox2D> Keep;
				Controller->GetLessonKeepClear(Keep);
				auto Covers = [&Keep, Scale, Size](const FVector2D& At)
				{
					const FBox2D Note(At, At + Size);
					for (const FBox2D& Each : Keep)
					{
						if (Note.Intersect(FBox2D(Each.Min / Scale, Each.Max / Scale)))
						{
							return true;
						}
					}
					return false;
				};
				LessonHome = !Covers(Home1) ? 0 : !Covers(Home2) ? 1 : 2;
			}
			bLessonShown = true;
			bLessonSlip = LessonHome == 2;
			LessonSlipWide = 148.0f;
			LessonAt = LessonHome == 1 ? Home2 : LessonHome == 2 ? FVector2D(7.0 * K, Home1.Y) : Home1;
		}

		/** (Rules 84.3.3) A thing the staff note's pointer may frame, by the name the controller aims at (FLessonAim). */
		template <typename WidgetType>
		TSharedRef<WidgetType> Aimed(const FName& Name, TSharedRef<WidgetType> Widget)
		{
			AimPieces.Emplace(Name, TWeakPtr<SWidget>(Widget));
			return Widget;
		}

		/**
		 * (Rules 84.3.3; the brief, section 3; trap 9.) What the pointer frames on the screen this frame: the first thing
		 * shown by the name the controller aims at, unless it is scrolled out of its card. And where the note stands, in
		 * the viewport's pixels, for a place pointed at on the ground to keep out from under it.
		 */
		void PlaceLessonPointer()
		{
			PointerFrame = FBox2D(ForceInit);
			AimPieces.RemoveAll([](const TPair<FName, TWeakPtr<SWidget>>& Each) { return !Each.Value.IsValid(); });
			if (!Controller.IsValid() || !ScreenRoot.IsValid())
			{
				return;
			}
			{
				FBox2D NotePixels(ForceInit);
				const FBox2D Note = bLessonShown && LessonNoteWidget.IsValid() ? InFrame(LessonNoteWidget) : FBox2D(ForceInit);
				FVector2D Pixels(0.0, 0.0);
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(Pixels);
				}
				const FVector2D Screen = ScreenRoot->GetTickSpaceGeometry().GetLocalSize();
				if (Note.bIsValid && Screen.X > 1.0 && Screen.Y > 1.0)
				{
					const FVector2D Scale(Pixels.X / Screen.X, Pixels.Y / Screen.Y);
					NotePixels = FBox2D(Note.Min * Scale, Note.Max * Scale);
				}
				Controller->SetLessonNoteOnScreen(NotePixels);
			}
			const FLessonAim& Aim = Controller->GetLessonAim();
			if (Aim.Kind != FLessonAim::Screen)
			{
				return;
			}
			const FBox2D Card = Controller->IsPageOpen() ? InFrame(PageCardBox) : FBox2D(ForceInit);
			for (const TPair<FName, TWeakPtr<SWidget>>& Each : AimPieces)
			{
				if (Each.Key != Aim.Name)
				{
					continue;
				}
				const FBox2D Box = InFrame(Each.Value);
				if (!Box.bIsValid)
				{
					continue;
				}
				// (Scrolled out of its card, it is not pointed at: the clamps would hang over the card's head round nothing.
				// So it is tried against the card, and against every scroll box it lies in.)
				const double Middle = Box.GetCenter().Y;
				if (Card.bIsValid && (Middle < Card.Min.Y || Middle > Card.Max.Y))
				{
					continue;
				}
				bool bScrolledOut = false;
				static const FName ScrollType(TEXT("SScrollBox"));
				const TSharedPtr<SWidget> Widget = Each.Value.Pin();
				for (TSharedPtr<SWidget> Up = Widget.IsValid() ? Widget->GetParentWidget() : nullptr; Up.IsValid() && !bScrolledOut; Up = Up->GetParentWidget())
				{
					if (Up->GetType() == ScrollType)
					{
						const FBox2D Held = InFrame(Up);
						bScrolledOut = Held.bIsValid && (Middle < Held.Min.Y || Middle > Held.Max.Y);
					}
				}
				if (bScrolledOut)
				{
					continue;
				}
				PointerFrame = Box;
				return;
			}
		}

		/** A piece of the frame that the news line must keep clear of on its right: one of the chips. */
		template <typename WidgetType>
		TSharedRef<WidgetType> Chip(TSharedRef<WidgetType> Widget)
		{
			TopChips.Add(Widget);
			return Piece(Widget);
		}

		/** And the one it must keep clear of on its left. */
		template <typename WidgetType>
		TSharedRef<WidgetType> Edge(TWeakPtr<SWidget>& Kept, TSharedRef<WidgetType> Widget)
		{
			Kept = Widget;
			return Piece(Widget);
		}


		/** What the ink on the frame's paper is multiplied by: nothing by day; by night it is darker, as the paper is. */
		FLinearColor InkLight() const
		{
			const float Moon = Controller.IsValid() ? FMath::Clamp(Controller->GetMoonlight(), 0.0f, 1.0f) : 0.0f;
			return FMath::Lerp(FLinearColor::White, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f), Moon);
		}

		/** The light the frame's paper lies in: the day's, or by night the lamp's, a good deal dimmer and a little cool. */
		FLinearColor PaperLight() const
		{
			const float Moon = Controller.IsValid() ? FMath::Clamp(Controller->GetMoonlight(), 0.0f, 1.0f) : 0.0f;
			return FMath::Lerp(FLinearColor::White, FLinearColor(0.36f, 0.38f, 0.42f), Moon);
		}

		/** Whether the reminder of the keys is open: asked for, or the pointer is on its mark. */
		bool HelpOpen() const
		{
			return (Controller.IsValid() && Controller->IsHelpShown()) || (HelpMark.IsValid() && HelpMark->IsHovered());
		}

		/**
		 * Whether the campaign's own plates are put away: the date plate, and the top right corner with the doors, the
		 * guide, the menu and END THE MONTH. They are while the newsreel runs, and (the UI director, 10:57 on build
		 * 1638) while the start screen or its options page is up, opened at the start or over a campaign under way:
		 * the map alone lies behind the card, veiled as it is.
		 */
		bool PlatesAway() const
		{
			return Controller.IsValid() && (Controller->GetNewsreel().bOn || Controller->GetOpening().bVisible);
		}

		/**
		 * One of the six doors: a round dark button with a brass rim and its sign engraved on it, its name typed
		 * under it, and (for the design offices) a brass tally of what is waiting behind it.
		 */
		TSharedRef<SWidget> MakeDoor(int32 Sign, const TCHAR* Label, const TCHAR* Tip, TFunction<void()> Do, TFunction<int32()> Waiting = nullptr)
		{
			return SNew(SBox).WidthOverride(80.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SOverlay)
						+ SOverlay::Slot().Padding(FMargin(5.0f, 0.0f))
						[
							SNew(SBox).WidthOverride(46.0f).HeightOverride(46.0f)
							[
								SNew(SButton).ButtonStyle(&DoorStyle).ContentPadding(FMargin(0.0f)).IsFocusable(false).HAlign(HAlign_Center).VAlign(VAlign_Center)
								.ToolTipText(FText::FromString(Tip))
								.OnClicked_Lambda([this, Do]() { if (Controller.IsValid()) { Do(); } return FReply::Handled(); })
								[
									SNew(SLineIcon).Kind(Sign).Size(25.0f).Ink(DoorBrass)
								]
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)
						[
							SNew(SBorder).BorderImage(&BadgeBrush).Padding(FMargin(5.0f, 0.0f))
							.Visibility_Lambda([this, Waiting]() { return Waiting && Controller.IsValid() && Waiting() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								SNew(STextBlock).Font(Typed(10, true)).ColorAndOpacity(DeskInk)
								.Text_Lambda([this, Waiting]() { return FText::AsNumber(Waiting && Controller.IsValid() ? Waiting() : 0); })
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(Cream.CopyWithNewOpacity(0.92f))
					]
				];
		}

		/** One of the small buttons under the minimap. */
		TSharedRef<SWidget> MapButton(const TCHAR* Label, const TCHAR* Tip, TFunction<void()> Do)
		{
			return SNew(SButton).ButtonStyle(&PromptStyle).ContentPadding(FMargin(4.0f, 5.0f)).IsFocusable(false).HAlign(HAlign_Center)
				.ToolTipText(FText::FromString(Tip))
				.OnClicked_Lambda([this, Do]() { if (Controller.IsValid()) { Do(); } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(Cream.CopyWithNewOpacity(0.85f))
				];
		}

		/** Money with its thousands marked off; a minus for a debt, and a plus for a gain when asked for. */
		static FString Cash(int32 Amount, bool bSigned = false)
		{
			const FString Figure = FText::AsNumber(FMath::Abs(Amount)).ToString();
			return Amount < 0 ? TEXT("\u2212") + Figure : (bSigned && Amount > 0 ? TEXT("+") + Figure : Figure);
		}

		static FText LedgerSummary(const FLedger& Books)
		{
			if (!Books.bBudget)
			{
				return FText::FromString(FString::Printf(TEXT("%s comes in each month. In the historical scenarios the forces cost nothing to keep; in the grand campaign they are paid for every month."), *Cash(Books.Gross)));
			}
			FString Line = FString::Printf(TEXT("Each month %s comes in and %s goes to keep the forces, which leaves %s for new orders and replacements."),
				*Cash(Books.Gross), *Cash(Books.UpkeepTotal), *Cash(Books.Net, true));
			if (Books.Money < 0)
			{
				Line += FString::Printf(TEXT(" The country is in debt (%d%% of the %s it may owe). At the limit unpaid corps rest badly and men drift away, so cut costs or take richer ground."),
					FMath::RoundToInt32(Books.Debt * 100.0f), *Cash(Books.DebtFloor));
			}
			else if (Books.Net < 0)
			{
				Line += TEXT(" More goes out than comes in: the treasury is shrinking.");
			}
			return FText::FromString(Line);
		}

		static FText ArsenalSummary(const FArsenal& Stock)
		{
			FString Line = Stock.Region.IsEmpty()
				? FString(TEXT("No province of yours is selected, so works cannot be built: close this, click one of your provinces, and open the arsenal again. Corps, aircraft and ships go where the notes say."))
				: FString::Printf(TEXT("With %s in mind: works are built there, and new corps form up there if it has the industry they need."), *Stock.Region);
			if (!Stock.bBudget)
			{
				Line += TEXT(" Aircraft, ships, factories and the other works are part of the grand campaign; a historical scenario offers corps and fortifications only.");
			}
			return FText::FromString(Line);
		}

		/** One thing on offer: what it is and what you get, what it costs now and each month after, how long it takes, and where it goes. */
		TSharedRef<SWidget> MakeOffer(const FArsenalOffer& Offer)
		{
			const bool bCan = Offer.Why.IsEmpty();
			// (The UI director, 18 November 2026, on build 1640 at Brian's window: a card that could not be ordered stood
			// at 1.44 to 1 against its ground and could not be read. Its name and its lines keep a muted ink at 4.5 to 1
			// or better; only its picture and its price are dimmed, and its reason stays. It is no pressed-out button
			// either: a button that is switched off is greyed whole by the screen. The lines of a card that can be
			// ordered take the same ink: 5.1 to 1 on the card, where the frame's muted ink stood at 4.4.)
			const FLinearColor Lines(FColor::FromHex(TEXT("B4AB98")));
			FString Terms = FString::Printf(TEXT("%s  \u00B7  %d month%s"), *Cash(Offer.Cost), Offer.Months, Offer.Months == 1 ? TEXT("") : TEXT("s"));
			if (Offer.Upkeep > 0)
			{
				Terms += FString::Printf(TEXT("  \u00B7  then %s a month"), *Cash(Offer.Upkeep));
			}
			// (Seen at Brian's window, 18 November 2026: "Destroyer flotilla" beside "4 destroyers" read "Destroye:", its
			// first word cut where the two had no room in the column. What is delivered stands beside the name only
			// where the name's longest word and it have room together: 19 typed letters beside a picture, 26 without.
			// Otherwise it opens the line below, as a long description always has.)
			int32 LongestWord = 0;
			{
				TArray<FString> NameWords;
				Offer.Label.ParseIntoArray(NameWords, TEXT(" "));
				for (const FString& Word : NameWords)
				{
					LongestWord = FMath::Max(LongestWord, Word.Len());
				}
			}
			const bool bPictured = Offer.Key.StartsWith(TEXT("corps:")) || Offer.Key.StartsWith(TEXT("cadre:")) || Offer.Key.StartsWith(TEXT("air:")) || Offer.Key.StartsWith(TEXT("ship:")) || Offer.Key == TEXT("elite") || Offer.Key == TEXT("para");
			const bool bBeside = Offer.Size.Len() <= 16 && LongestWord + Offer.Size.Len() <= (bPictured ? 19 : 26);
			TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Offer.Label)).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(bCan ? Cream : Lines).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(bBeside ? Offer.Size : FString())).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Lines)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Terms)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(bCan ? BrightBrass : Faint).AutoWrapText(true)
				];
			// A long description of what is delivered goes on the line below instead of beside the name.
			const FString Where = !bBeside && !Offer.Size.IsEmpty() ? Offer.Size.Left(1).ToUpper() + Offer.Size.Mid(1) + TEXT(". ") + Offer.Note : Offer.Note;
			if (!Where.IsEmpty())
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Where)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Lines).AutoWrapText(true)
				];
			}
			if (!bCan)
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					// (the UI director, 18 November 2026: a shortfall is neither lost nor due by a date; it takes the cards' italic ink, not red)
					SNew(STextBlock).Text(FText::FromString(Offer.Why)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Lines).AutoWrapText(true)
				];
			}
			// A picture of what is on offer, where the game has a model of it: this nation's own tank, aircraft or ship.
			const FString Tag = Controller.IsValid() ? Controller->GetPlayerTag() : FString();
			FString Model;
			if (Offer.Key.StartsWith(TEXT("corps:"))) { Model = ACampaignMapActor::ModelFor(Tag, Offer.Key.Mid(6)); }
			else if (Offer.Key.StartsWith(TEXT("cadre:"))) { Model = ACampaignMapActor::ModelFor(Tag, Offer.Kind.IsEmpty() ? FString(TEXT("inf")) : Offer.Kind); } // (Version 64) a folded corps, raised again
			else if (Offer.Key == TEXT("elite")) { Model = ACampaignMapActor::ModelFor(Tag, TEXT("heavy")); }
			else if (Offer.Key == TEXT("para")) { Model = ACampaignMapActor::ModelFor(Tag, TEXT("inf")); }
			else if (Offer.Key == TEXT("air:ftr")) { Model = FMiniatureMesh::MainFighterOf(Tag); }
			else if (Offer.Key == TEXT("air:bmr")) { Model = FMiniatureMesh::MainBomberOf(Tag); }
			else if (Offer.Key == TEXT("ship:bb")) { Model = TEXT("Battleship:") + Tag; }
			else if (Offer.Key == TEXT("ship:cv")) { Model = TEXT("Carrier:") + Tag; }
			else if (Offer.Key == TEXT("ship:ca")) { Model = TEXT("Cruiser:") + Tag; }
			else if (Offer.Key == TEXT("ship:dd")) { Model = TEXT("Destroyer:") + Tag; }
			else if (Offer.Key == TEXT("ship:ss")) { Model = TEXT("Submarine:") + Tag; }
			const TSharedPtr<FSlateBrush> Portrait = Model.IsEmpty() ? TSharedPtr<FSlateBrush>() : PortraitOf(Model);
			TSharedRef<SHorizontalBox> Face = SNew(SHorizontalBox);
			if (Portrait.IsValid())
			{
				Face->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(2.0f)).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, bCan ? 1.0f : 0.4f))
					[
						SNew(SPortrait).Picture(Portrait).Size(FVector2D(78.0, 48.0))
					]
				];
			}
			Face->AddSlot().FillWidth(1.0f)[Words];
			if (!bCan)
			{
				// (a card, not a button: it takes no press, and nothing greys it)
				return SNew(SBorder).BorderImage(&PlainButton.Normal).Padding(FMargin(9.0f, 7.0f, 11.0f, 8.0f))
					.ToolTipText(FText::FromString(Offer.Blurb))
					[
						Face
					];
			}
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(9.0f, 7.0f, 11.0f, 8.0f)).IsFocusable(false)
				.ToolTipText(FText::FromString(Offer.Blurb))
				.OnClicked_Lambda([this, Key = Offer.Key]()
				{
					if (Controller.IsValid())
					{
						Controller->OrderFromArsenal(Key);
					}
					return FReply::Handled();
				})
				[
					Face
				];
		}

		/** The arsenal in four columns: the army, the air force, the navy, and works for the province in mind. */
		void LayOutArsenal(const FArsenal& Stock)
		{
			ArsenalColumns->ClearChildren();
			bool bOrderAimed = false;
			const TCHAR* Groups[][2] = { { TEXT("army"), TEXT("THE ARMY") }, { TEXT("air"), TEXT("THE AIR FORCE") }, { TEXT("navy"), TEXT("THE NAVY") }, { TEXT("works"), TEXT("WORKS") } };
			for (int32 Column = 0; Column < 4; ++Column)
			{
				TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
				FString Title = Groups[Column][1];
				if (Column == 3 && !Stock.Region.IsEmpty())
				{
					Title = FString::Printf(TEXT("WORKS AT %s"), *Stock.Region.ToUpper());
				}
				List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Title)).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				int32 Count = 0;
				for (const FArsenalOffer& Offer : Stock.Offers)
				{
					if (Offer.Group == Groups[Column][0])
					{
						// (Rules 84.3.3) Lesson 10's pointer, in the arsenal with the build desk his own: the first thing he can order.
						const bool bFirstLit = !bOrderAimed && Offer.Why.IsEmpty();
						bOrderAimed |= bFirstLit;
						List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[bFirstLit ? Aimed(TEXT("order"), MakeOffer(Offer)) : MakeOffer(Offer)];
						++Count;
					}
				}
				if (Count == 0)
				{
					List->AddSlot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
						.Text(FText::FromString(Column == 3 && Stock.Region.IsEmpty() ? TEXT("Select one of your own provinces first.") : TEXT("Nothing on offer here in this campaign.")))
					];
				}
				// What is already on order goes under the army's column, where there is room.
				if (Column == 0)
				{
					List->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("ON ORDER"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
					];
					if (Stock.Orders.Num() == 0)
					{
						List->AddSlot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Nothing yet. Click anything that is lit to order it."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
						];
					}
					for (const FLedgerLine& Line : Stock.Orders)
					{
						List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[LedgerRow(Line.Label, FString(), Cash(Line.Amount), Muted)];
					}
				}
				ArsenalColumns->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Column == 3 ? 0.0f : 16.0f, 0.0f)
				[
					SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 8.0f, 0.0f)[List]
				];
			}
		}

		/** A row of buttons that wraps onto the next line when there are many. */
		TSharedRef<SWidget> PageButtons(const TArray<FPageButton>& Buttons)
		{
			TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0f, 5.0f));
			for (const FPageButton& Button : Buttons)
			{
				Box->AddSlot()
				[
					Aimed(FName(*(TEXT("act:") + Button.Act)), OfficeButton(*Button.Label, Button.bDanger ? Alarm : BrightBrass, Button.bOk, Button.Tip, [this, Act = Button.Act]() { if (Controller.IsValid()) { Controller->PressPage(Act); } }))
				];
			}
			return Box;
		}

		/** A room of the rules' own: its buttons, then each section with its words and its rows. */
		/** Whether the room open is a raid being planned, which is set out as a card of its own. */
		bool IsRaidCard() const { return Controller.IsValid() && Controller->IsPageOpen() && Controller->GetPage().Card.bValid; }

		/** A key's name on a small cap, as on a keyboard: beside the button it works. */
		/**
		 * (Ruling 87; the art director's brief.) A slip of typing paper with the red binding bar down its left edge:
		 * NO MOVES LEFT THIS MONTH, what a right-click does, and GOT IT. Paper on leather is the calendar's own
		 * contrast, so it reads as the game speaking and not as an error.
		 */
		TSharedRef<SWidget> MarchSlip()
		{
			return SNew(SBorder).BorderImage(&SlipBrush).Padding(FMargin(0.0f))
				.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMarchSlipShown() ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the slip is not the map
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(7.0f)[SNew(SBorder).BorderImage(&SlipBarBrush)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(13.0f, 8.0f, 18.0f, 9.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("NO MOVES LEFT THIS MONTH"))).Font(Typed(10, true, 120)).ColorAndOpacity(InkRed)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Right-click a friendly place and this army marches there by itself, starting next month.")))
							.Font(Typed(9)).ColorAndOpacity(DeskInk).LineHeightPercentage(1.1f).WrapTextAt(404.0f)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&SlipButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->DismissMarchSlip(); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("GOT IT"))).Font(Typed(9, true, 120)).ColorAndOpacity(DeskInk)
						]
					]
				];
		}

		/** The name a country goes by on a card ("United States"), from the map's own list; its tag where it has none. */
		FString NationName(const FString& Tag) const
		{
			const ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			const FCampaignNation* Nation = Table ? Table->GetData().FindNation(Tag) : nullptr;
			return Nation ? Nation->Name : Tag;
		}

		/**
		 * (Rules 84.5; the UI director's sheet 34, "The road before the order".) The march up and the attack planned as
		 * one order, in the landing card's parts and nothing new: the kicker and the date, the title, the two sides with
		 * THE ROAD between them, the headline (what he gets on arrival: bright where he can attack this month, amber where
		 * the attack waits), the staff's pick in a sentence, and the approaches as rows he can press. Every word is the
		 * rules' or the sheet's; the controller sets them.
		 */
		TSharedRef<SWidget> ApproachCard()
		{
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			auto Card = [this]() -> const FWarPrompt* { return Controller.IsValid() ? &Controller->GetWarPrompt() : nullptr; };
			const FLinearColor Amber(FColor::FromHex(TEXT("E0A13C")));
			auto Hair = [this]() -> TSharedRef<SWidget>
			{
				return SNew(SBox).HeightOverride(1.0f)[SNew(SImage).Image(&ApproachHairBrush)];
			};
			// One side of the card: the flag and the country's name over the army's name and its numbers.
			auto Side = [this, Card, Px](bool bOurs) -> TSharedRef<SWidget>
			{
				const EHorizontalAlignment Align = bOurs ? HAlign_Left : HAlign_Right;
				auto Flag = [this, Card, bOurs]() -> TSharedRef<SWidget>
				{
					return SNew(SBox).WidthOverride(38.0f).HeightOverride(22.0f)
						[
							SNew(SImage).Image_Lambda([this, Card, bOurs]() { const FWarPrompt* P = Card(); return FlagBrush(P ? (bOurs ? P->OursTag : P->Tag) : FString()); })
						];
				};
				TSharedRef<STextBlock> Country = SNew(STextBlock).Font(Typed(Px(10.0f), true, 60)).ColorAndOpacity(Brass)
					.Text_Lambda([this, Card, bOurs]() { const FWarPrompt* P = Card(); return P ? FText::FromString(NationName(bOurs ? P->OursTag : P->Tag).ToUpper()) : FText::GetEmpty(); });
				TSharedRef<SHorizontalBox> Head = SNew(SHorizontalBox);
				if (bOurs)
				{
					Head->AddSlot().AutoWidth().VAlign(VAlign_Center)[Flag()];
					Head->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[Country];
				}
				else
				{
					Head->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)[Country];
					Head->AddSlot().AutoWidth().VAlign(VAlign_Center)[Flag()];
				}
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align)[Head]
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Serif(TEXT("Regular"), Px(28.0f))).ColorAndOpacity(Cream)
						.Text_Lambda([Card, bOurs]() { const FWarPrompt* P = Card(); return P ? FText::FromString(bOurs ? P->OursName : P->FoeName) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Typed(Px(13.0f))).ColorAndOpacity(Muted)
						.Text_Lambda([Card, bOurs]() { const FWarPrompt* P = Card(); return P ? FText::FromString(bOurs ? P->OursLine : P->FoeLine) : FText::GetEmpty(); })
					];
			};
			return SNew(SVerticalBox)
				.Visibility_Lambda([Card]() { const FWarPrompt* P = Card(); return P && P->bApproach ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Typed(Px(11.0f), true, 60)).ColorAndOpacity(Brass)
					.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? FText::FromString(P->Kicker) : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
					.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? P->Title : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 14.0f)[Hair()]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)[Side(true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(170.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Font(Typed(Px(10.0f), true, 60)).ColorAndOpacity(Brass).Text(FText::FromString(TEXT("THE ROAD")))
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Serif(TEXT("Italic"), Px(15.0f))).ColorAndOpacity(Muted).AutoWrapText(true).Justification(ETextJustify::Center)
								.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? FText::FromString(P->Road) : FText::GetEmpty(); })
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)[Side(false)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 16.0f)[Hair()]
				+ SVerticalBox::Slot().AutoHeight()
				[
					// (the headline is what he gets on arrival: bright where he can attack this month, amber where it waits)
					SNew(STextBlock).Font(Serif(TEXT("Bold"), Px(26.0f))).AutoWrapText(true)
					.ColorAndOpacity_Lambda([Card, Amber]() { const FWarPrompt* P = Card(); return FSlateColor(P && P->bHeadAmber ? Amber : Cream); })
					.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? P->Headline : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Serif(TEXT("Regular"), Px(16.0f))).ColorAndOpacity(Cream).AutoWrapText(true).LineHeightPercentage(1.15f)
					.Visibility_Lambda([Card]() { const FWarPrompt* P = Card(); return P && !P->Under.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? P->Under : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
				[
					// THE APPROACH: the rows in a box, the staff's pick first and lit. A press on a row takes that way: the
					// road, the headline and the map follow it.
					SNew(SBorder).BorderImage(&ApproachBoxBrush).Padding(FMargin(1.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(12.0f, 8.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Typed(Px(10.0f), true, 60)).ColorAndOpacity(Brass)
								.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? P->WaysHead : FText::GetEmpty(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Typed(Px(11.0f))).ColorAndOpacity(Muted)
								.Text_Lambda([Card]() { const FWarPrompt* P = Card(); return P ? P->WaysNote : FText::GetEmpty(); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight()[ApproachRow(0)]
						+ SVerticalBox::Slot().AutoHeight()[ApproachRow(1)]
						+ SVerticalBox::Slot().AutoHeight()[ApproachRow(2)]
						+ SVerticalBox::Slot().AutoHeight()[ApproachRow(3)]
					]
				];
		}

		/**
		 * (Sheet 34.) One approach on the march-up card: a hairline over it, the radio mark, the province, its ground in
		 * the game's own word and its river ("no river to cross", or "across the Vistula" with the name in the bright
		 * ink), and the moves left on arrival ("no move left" in amber). The first row is the staff's pick and says so.
		 * The one chosen stands on a warmer ground with a brass edge.
		 */
		TSharedRef<SWidget> ApproachRow(int32 Index)
		{
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			auto Way = [this, Index]() -> const FApproachRow* { return Controller.IsValid() && Controller->GetWarPrompt().Ways.IsValidIndex(Index) ? &Controller->GetWarPrompt().Ways[Index] : nullptr; };
			auto Lit = [this, Index]() { return Controller.IsValid() && Controller->GetWarPrompt().Way == Index; };
			const FLinearColor Amber(FColor::FromHex(TEXT("E0A13C")));
			return SNew(SVerticalBox)
				.Visibility_Lambda([Way]() { return Way() ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(1.0f)[SNew(SImage).Image(&ApproachHairBrush)]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SButton).ButtonStyle(&NoButton).ContentPadding(FMargin(0.0f)).IsFocusable(false)
					.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PickApproach(Index); } return FReply::Handled(); })
					[
						SNew(SBorder).Padding(FMargin(0.0f))
						.BorderImage_Lambda([this, Lit]() -> const FSlateBrush* { return Lit() ? &ApproachLitBrush : &NoBrush; })
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								// (the chosen row's brass edge)
								SNew(SBox).WidthOverride(3.0f)
								[
									SNew(SImage).Image(&ApproachEdgeBrush)
									.Visibility_Lambda([Lit]() { return Lit() ? EVisibility::Visible : EVisibility::Hidden; })
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(10.0f, 8.0f, 12.0f, 8.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									// (the radio mark, drawn rather than lettered, so that no face is wanting the glyph: a brass
									// ring, with a dot in it on the chosen row)
									SNew(SBox).WidthOverride(14.0f).HeightOverride(14.0f)
									[
										SNew(SOverlay)
										+ SOverlay::Slot()[SNew(SImage).Image(&RadioRingBrush)]
										+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
										[
											SNew(SBox).WidthOverride(6.0f).HeightOverride(6.0f)
											.Visibility_Lambda([Lit]() { return Lit() ? EVisibility::Visible : EVisibility::Hidden; })
											[
												SNew(SImage).Image(&RadioDotBrush)
											]
										]
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(170.0f)
									[
										SNew(STextBlock).Font(Serif(TEXT("Bold"), Px(18.0f))).ColorAndOpacity(Cream)
										.Text_Lambda([Way]() { const FApproachRow* Row = Way(); return Row ? FText::FromString(Row->Name) : FText::GetEmpty(); })
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									// (the ground in the game's own word, then the river: its name in the bright ink, since it
									// weakens the attack; "no river to cross" in the quiet one)
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock).Font(Typed(Px(12.0f))).ColorAndOpacity(Muted)
										.Text_Lambda([Way]()
										{
											const FApproachRow* Row = Way();
											if (!Row) { return FText::GetEmpty(); }
											const FString Ground = Row->Ground.IsEmpty() ? FString() : FCampaignMapData::TerrainLabel(Row->Ground) + TEXT("  \u00B7  ");
											return FText::FromString(Ground);
										})
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Font_Lambda([Way, Px]() { const FApproachRow* Row = Way(); return Typed(Px(12.0f), Row && !Row->River.IsEmpty()); })
										.ColorAndOpacity_Lambda([Way]() { const FApproachRow* Row = Way(); return FSlateColor(Row && !Row->River.IsEmpty() ? BrightBrass : Muted); })
										.Text_Lambda([Way]() { const FApproachRow* Row = Way(); return Row ? FText::FromString(Row->RiverWords.IsEmpty() ? Row->How : Row->RiverWords) : FText::GetEmpty(); })
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Typed(Px(12.0f), true))
									.ColorAndOpacity_Lambda([Way, Amber]() { const FApproachRow* Row = Way(); return FSlateColor(Row && Row->Left <= 0 ? Amber : Cream); })
									.Text_Lambda([Way]()
									{
										const FApproachRow* Row = Way();
										if (!Row) { return FText::GetEmpty(); }
										if (Row->Left <= 0) { return FText::FromString(TEXT("no move left")); }
										return FText::FromString(FString::Printf(TEXT("%d %s left"), Row->Left, Row->Left == 1 ? TEXT("move") : TEXT("moves")));
									})
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									// (the first row is the staff's pick, and says so; a lone row needs no tag, as on the sheet's amber
									// card, where the note over it says it is the only one in reach)
									SNew(SBorder).BorderImage(&PickTagBrush).Padding(FMargin(6.0f, 1.0f, 6.0f, 2.0f))
									.Visibility_Lambda([this, Index]() { return Index == 0 && Controller.IsValid() && Controller->GetWarPrompt().Ways.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
									[
										SNew(STextBlock).Font(Typed(Px(10.0f), true, 20)).ColorAndOpacity(FLinearColor(FColor::FromHex(TEXT("1F1808"))))
										.Text(FText::FromString(TEXT("The staff's pick")))
									]
								]
							]
						]
					]
				];
		}

		TSharedRef<SWidget> KeyCap(const TCHAR* Key, const FLinearColor& Ink)
		{
			return SNew(SBorder).BorderImage(&KeyCapBrush).BorderBackgroundColor(Ink.CopyWithNewOpacity(0.85f)).Padding(FMargin(5.0f, 0.0f, 5.0f, 1.0f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Key)).Font(Typed(7.0f, true)).ColorAndOpacity(Ink)
				];
		}

		/**
		 * (Rulings 88 and 101.) True while armies at the front wait untouched and with no order: NEXT ARMY is then the
		 * bright button, and END THE MONTH the dark one. The rear never holds the month, and a country at peace opens
		 * with END THE MONTH lit.
		 */
		bool ArmiesWait() const { return Controller.IsValid() && !Controller->IsWatching() && Controller->ArmiesIdleAtFront() > 0; }
		/** (Ruling 101.) The quieter number: the armies that can still move and are not counted on NEXT ARMY. */
		int32 ArmiesBesides() const { return Controller.IsValid() && !Controller->IsWatching() ? Controller->ArmiesInRear() : 0; }

		/**
		 * (Ruling 88; the art director's brief: "one bright button, and it is always the next thing to press".) The
		 * control that ends the month, in its two looks, one of them shown: dark leather with brass words while armies
		 * wait for orders (it is plainly a button, and it is never barred); lit brass when every army has its orders,
		 * and while the other nations move (when it reads SKIP). The dial's short hand stands at the month of the year.
		 */
		TSharedRef<SWidget> EndPills()
		{
			const FLinearColor DarkInk = BrightBrass;
			auto Make = [this](bool bDark, const FLinearColor& Ink) -> TSharedRef<SWidget>
			{
				return SNew(SButton).ButtonStyle(bDark ? &EndDarkStyle : &EndLitStyle).ContentPadding(FMargin(3.0f)).IsFocusable(false)
					.Visibility_Lambda([this, bDark]() { return ArmiesWait() == bDark ? EVisibility::Visible : EVisibility::Collapsed; })
					.ToolTipText(FText::FromString(bDark ? TEXT("End the month at any time: every other nation moves, and the next month begins. Armies still wait for orders: NEXT ARMY goes to them. (Enter)")
						: TEXT("End the month: every other nation moves, and the next month begins. (Enter)")))
					.OnClicked_Lambda([this]()
					{
						if (Controller.IsValid())
						{
							if (Controller->IsWatching())
							{
								Controller->SkipWatching();
							}
							else
							{
								Controller->EndMonth();
							}
						}
						return FReply::Handled();
					})
					[
						SNew(SBox).WidthOverride(246.0f)
						[
							SNew(SBorder).BorderImage(bDark ? &NoBrush : &PillShine).Padding(FMargin(9.0f, 5.0f, 14.0f, 5.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SLineIcon).Kind(6).Size(34.0f).Weight(1.6f).Ink(Ink)
									.Hand_Lambda([this]() { return static_cast<float>(MonthOf(Controller.IsValid() ? Controller->GetDateText().ToString() : FString())); })
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Serif(TEXT("Bold"), 14, 120)).ColorAndOpacity(Ink)
										.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && Controller->IsWatching() ? TEXT("SKIP") : TEXT("END THE MONTH")); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(Typed(9)).ColorAndOpacity(Ink.CopyWithNewOpacity(0.85f))
											.Text_Lambda([this, bDark]()
											{
												if (Controller.IsValid() && Controller->IsWatching())
												{
													return FText::FromString(TEXT("the other nations move"));
												}
												return FText::FromString(bDark ? TEXT("at any time") : TEXT("ready"));
											})
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SBox)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsWatching() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
											[
												KeyCap(TEXT("ENTER"), Ink)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SBox)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsWatching() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											[
												KeyCap(TEXT("SPACE"), Ink)
											]
										]
									]
								]
							]
						]
					];
			};
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Aimed(TEXT("end"), Make(true, DarkInk))]
				+ SVerticalBox::Slot().AutoHeight()[Aimed(TEXT("end"), Make(false, DeskInk))];
		}

		/**
		 * (Ruling 88.) NEXT ARMY: while armies wait, the brass pill END THE MONTH used to wear, with the count on an
		 * ink disc, "yet to move" and the Tab key's cap, and a chevron; when none wait, a quiet chip with a green tick
		 * that says every army has its orders.
		 */
		TSharedRef<SWidget> NextArmyPill()
		{
			const FLinearColor CountInk(FColor::FromHex(TEXT("F0D28A")));
			const FLinearColor RearInk(FColor::FromHex(TEXT("B9A06A")));
			auto Shown = [this]() { return Controller.IsValid() && Controller->GetSession() && !Controller->IsWatching() && Controller->GetSession()->GetView().bHumanUp; };
			return SNew(SVerticalBox)
				.Visibility_Lambda([Shown]() { return Shown() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
				// 1. Armies at the front remain: the one bright thing.
				+ SVerticalBox::Slot().AutoHeight()
				[
					// (wider by a little where its line names both kinds: "12 at the front and 1 newly raised")
					SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(Controller.IsValid() && Controller->IsRoundMixed() ? 282.0f : 252.0f); }).HeightOverride(46.0f)
					.Visibility_Lambda([this]() { return ArmiesWait() ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SButton).ButtonStyle(&PillStyle).ContentPadding(FMargin(3.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(TEXT("Goes to the next army that still waits for orders this month: the front first, then the rear; in each, those not yet touched before the others, then the nearest. The number is the armies at the front with no order yet. STAND THIS MONTH on an army's card takes it out of the round. (Tab)")))
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->NextArmy(); } return FReply::Handled(); })
						[
							SNew(SBorder).BorderImage(&PillShine).Padding(FMargin(6.0f, 2.0f, 14.0f, 2.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(32.0f).HeightOverride(32.0f)
									[
										SNew(SBorder).BorderImage(&CountDiscBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
										[
											SNew(STextBlock).Font(Serif(TEXT("Bold"), 14)).ColorAndOpacity(CountInk)
											.Text_Lambda([this]() { return FText::AsNumber(Controller.IsValid() ? Controller->ArmiesIdleAtFront() : 0); })
										]
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("NEXT ARMY"))).Font(Serif(TEXT("Bold"), 14, 120)).ColorAndOpacity(DeskInk)
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(9.0f, 1.0f, 0.0f, 0.0f)[KeyCap(TEXT("TAB"), DeskInk)]
										// (where the line below names both kinds it needs the whole width, and the chevron rides up here)
										+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center)
										[
											SNew(SBox)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsRoundMixed() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											[
												SNew(SLineIcon).Kind(8).Size(16.0f).Weight(2.2f).Ink(DeskInk)
											]
										]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, -1.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											// (where the disc counts only armies raised this month, and none at the front, the line says so)
											SNew(STextBlock).ColorAndOpacity(DeskInk.CopyWithNewOpacity(0.85f)).Font(Typed(9))
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsRoundMixed() && !Controller->GetRoundWords().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
											.Text_Lambda([this]()
											{
												return FText::FromString(Controller.IsValid() && Controller->IsRoundNewlyRaised() ? TEXT("newly raised") : TEXT("at the front"));
											})
										]
										// (Ruling 111(f): where it counts both kinds, the rules' own words say how many of each. The longer
										// line is set smaller and has the whole width; were it ever longer still, it shrinks to fit and is
										// never cut short.)
										+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
										[
											SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).HAlign(HAlign_Left)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsRoundMixed() && !Controller->GetRoundWords().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											[
												SNew(STextBlock).ColorAndOpacity(DeskInk.CopyWithNewOpacity(0.85f)).Font(Typed(7.5f))
												.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() ? Controller->GetRoundWords() : FString()); })
											]
										]
										// (Ruling 101.) The quieter number, at about 60 percent of the ink's strength: the rest that can
										// still move. With nobody in the rear the line reads "at the front" alone.
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Typed(9)).ColorAndOpacity(DeskInk.CopyWithNewOpacity(0.6f))
											.Visibility_Lambda([this]() { return ArmiesBesides() > 0 && !(Controller.IsValid() && Controller->IsRoundMixed()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("· rear %d"), ArmiesBesides())); })
										]
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SBox)
									.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsRoundMixed() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
									[
										SNew(SLineIcon).Kind(8).Size(16.0f).Weight(2.2f).Ink(DeskInk)
									]
								]
							]
						]
					]
				]
				// 2. The front is done and the rear is not: a quiet chip, still a button. It never lights and never
				// breathes, and never holds the light from END THE MONTH.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(252.0f).HeightOverride(25.0f)
					.Visibility_Lambda([this]() { return !ArmiesWait() && ArmiesBesides() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SButton).ButtonStyle(&NoButton).ContentPadding(FMargin(0.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(TEXT("Every army at the front has its orders. This goes round the armies in the rear that can still move. They do not hold the month. (Tab)")))
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->NextArmy(); } return FReply::Handled(); })
						[
							SNew(SBorder).BorderImage(&RearChipBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
								[
									SNew(SBox).MinDesiredWidth(26.0f).HeightOverride(16.0f)
									[
										SNew(SBorder).BorderImage(&RearRingBrush).BorderBackgroundColor(RearInk).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(5.0f, 0.0f, 5.0f, 1.0f))
										[
											SNew(STextBlock).Font(Serif(TEXT("Bold"), 9)).ColorAndOpacity(RearInk)
											.Text_Lambda([this]() { return FText::AsNumber(ArmiesBesides()); })
										]
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("NEXT IN THE REAR"))).Font(Typed(8.5f, true)).ColorAndOpacity(RearInk)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("TAB"), RearInk)]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SLineIcon).Kind(8).Size(10.0f).Weight(1.6f).Ink(RearInk)
								]
							]
						]
					]
				]
				// 3. Every army has its orders.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(252.0f).HeightOverride(25.0f)
					.Visibility_Lambda([this]() { return !ArmiesWait() && ArmiesBesides() <= 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SBorder).BorderImage(&QuietChipBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SLineIcon).Kind(7).Size(13.0f).Weight(1.8f).Ink(Gain)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("EVERY ARMY HAS ITS ORDERS"))).Font(Typed(8.5f, true)).ColorAndOpacity(Gain)
							]
						]
					]
				];
		}
		bool IsMonthCard() const { return Controller.IsValid() && Controller->IsPageOpen() && Controller->GetPage().Month.bValid; }

		/**
		 * (Ruling 64.) The art director's picture for the month's card, by its file's own name ("troops", "town",
		 * "fieldworks", "fleet", "harbour", "factories", "rail-yard", "airfield"): the small one for a row, the plain
		 * print for the lead story (the ringed print of the raid card where there is no plain one).
		 */
		TSharedPtr<FSlateBrush> MonthPictureOf(const FString& Name, bool bSmall)
		{
			if (Name.IsEmpty())
			{
				return nullptr;
			}
			const FString Key = Name + (bSmall ? TEXT("-small") : TEXT("-plain"));
			if (const TSharedPtr<FSlateBrush>* Kept = MonthPictures.Find(Key))
			{
				return *Kept;
			}
			static TMap<FString, UTexture2D*> Read; // (kept for the life of the game, like the target pictures)
			UTexture2D* Texture = Read.FindRef(Key);
			if (!Texture && !Read.Contains(Key))
			{
				const FString Folder = FPaths::ProjectContentDir() / TEXT("RaidCard");
				FString File = Folder / (TEXT("target-") + Key + TEXT(".png"));
				if (!FPaths::FileExists(File) && !bSmall)
				{
					File = Folder / (TEXT("target-") + Name + TEXT(".png"));
				}
				if (FPaths::FileExists(File))
				{
					Texture = FImageUtils::ImportFileAsTexture2D(File);
				}
				if (Texture)
				{
					Texture->AddToRoot();
				}
				Read.Add(Key, Texture);
			}
			TSharedPtr<FSlateBrush> Brush;
			if (Texture)
			{
				Brush = MakeShared<FSlateBrush>();
				Brush->SetResourceObject(Texture);
				Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush->DrawAs = ESlateBrushDrawType::Image;
			}
			MonthPictures.Add(Key, Brush);
			return Brush;
		}

		/** One of the player's fronts on the month's card: its picture, its sign, the place, what happened, the figures and the way to the map. */
		TSharedRef<SWidget> MonthRowOf(const FMonthRow& Told)
		{
			const FLinearColor Good(FColor(147, 181, 108)), Bad(FColor(208, 86, 61)), Warn(FColor(224, 161, 60)), Ink(FColor::FromHex(TEXT("1F1808")));
			const FLinearColor Edge = Told.Tone == 1 ? Good : Told.Tone == -1 ? Bad : Told.Tone == 2 ? Warn : Muted;
			TSharedRef<SHorizontalBox> Top = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Told.Place)).Font(Serif(TEXT("Bold"), 16)).ColorAndOpacity(Cream)
				];
			if (!Told.Tag.IsEmpty())
			{
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&MonthTagBrush).BorderBackgroundColor(Edge).Padding(FMargin(7.0f, 2.0f, 7.0f, 2.0f))
					[
						SNew(STextBlock).Text(FText::FromString(Told.Tag)).Font(Font(TEXT("Bold"), 10, 160)).ColorAndOpacity(Ink)
					]
				];
			}
			if (!Told.JoinWord.IsEmpty() && !Told.B.IsEmpty())
			{
				// (Rules 84.2.6; the UI director's words for ruling 211, section 5.) The word that goes with the tag, the other side's
				// flag and its name, so the row reads whole with no picture: "WON from [flag] Mexico", "BESIEGED by [flag] Canada".
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Told.JoinWord)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(26.0f).HeightOverride(17.0f)[SNew(SImage).Image(FlagBrush(Told.B))]
				];
				if (!Told.JoinName.IsEmpty())
				{
					Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Told.JoinName)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
					];
				}
			}
			if (!Told.A.IsEmpty() && Told.JoinWord.IsEmpty())
			{
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(26.0f).HeightOverride(17.0f)[SNew(SImage).Image(FlagBrush(Told.A))]
				];
			}
			if (!Told.A.IsEmpty() && !Told.B.IsEmpty() && Told.JoinWord.IsEmpty())
			{
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(7.0f, 0.0f, 7.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("against"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
				Top->AddSlot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(26.0f).HeightOverride(17.0f)[SNew(SImage).Image(FlagBrush(Told.B))]
				];
			}
			TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(5.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Edge)]
				];
			const TSharedPtr<FSlateBrush> Painted = MonthPictureOf(Told.Picture, true);
			if (Painted.IsValid())
			{
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(92.0f).HeightOverride(59.0f)[SNew(SImage).Image(Painted.Get())]
				];
			}
			if (const FSlateBrush* Sign = Told.Sign.IsEmpty() ? nullptr : OrderSignBrush(Told.Sign))
			{
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(36.0f).HeightOverride(36.0f)
					[
						SNew(SBorder).BorderImage(&MonthDiscBrush).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f))
						[
							SNew(SBox).WidthOverride(22.0f).HeightOverride(15.0f)[SNew(SImage).Image(Sign).ColorAndOpacity(Cream)]
						]
					]
				];
			}
			Line->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Top]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					// (The UI director's review of build 1628: no sentence is cut with three dots. The rules give it whole,
					// and it wraps to a second line and the row grows; a dot falls at a fold, as check 19 has it.)
					SNew(SPhraseText).Text(FText::FromString(Told.Text)).Face(Font(TEXT("Regular"), 11)).Ink(Cream.CopyWithNewOpacity(0.88f)).Guess(420.0f)
				]
			];
			for (const FMonthFigure& Figure : Told.Figures)
			{
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Text(FText::FromString(Figure.Value)).Font(Serif(TEXT("Bold"), 16)).ColorAndOpacity(Cream)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Text(FText::FromString(Figure.Label)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted)
					]
				];
			}
			if (Told.Region >= 0 || Told.Sea >= 0)
			{
				// (a province has a place on the map and a sea has one on the chart: either way the row carries the pin)
				const FString Act = Told.Region >= 0 ? FString::Printf(TEXT("goto:region:%d"), Told.Region) : FString::Printf(TEXT("goto:sea:%d"), Told.Sea);
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Shows the place on the map.")))
					.OnClicked_Lambda([this, Act]() { if (Controller.IsValid()) { Controller->PressPage(Act); } return FReply::Handled(); })
					[
						SNew(SBox).WidthOverride(11.0f).HeightOverride(11.0f)[SNew(SImage).Image(&DotBrush).ColorAndOpacity(BrightBrass)]
					]
				];
			}
			const TSharedRef<SBox> Row = SNew(SBox).HeightOverride(71.0f)
				[
					SNew(SBorder).BorderImage(&MonthBoxBrush).Padding(FMargin(0.0f))[Line]
				];
			// (Rules 84.3.3) A line that carries a pin is one lesson 6 points at, the whole line framed.
			return Told.Region >= 0 || Told.Sea >= 0 ? Aimed(TEXT("line"), Row) : Row;
		}

		/**
		 * (Rulings 64 and 65; the art director's brief. Brian, 16 November 2026: "i dont love the turn update UI telling
		 * you what happend. its very plain and hard to read".) The month on one card, top to bottom: the title with WATCH
		 * IT AGAIN and GIVE ORDERS; the red band, for news that changes the war; the month's score in boxes; the lead
		 * story with its picture; the player's fronts as rows; and AT HOME, WHAT YOUR STAFF DID and ELSEWHERE along the
		 * bottom. A front page, not a telegram: the eye gets the headline, the score, the picture, then the small stories.
		 */
		void LayOutMonthCard(const FMonthCard& Card)
		{
			const FLinearColor Good(FColor(147, 181, 108)), Bad(FColor(208, 86, 61)), Warn(FColor(224, 161, 60)), Ink(FColor::FromHex(TEXT("1F1808")));
			auto ToneInk = [&](int32 Tone) { return Tone == 1 ? Good : Tone == -1 ? Bad : Tone == 2 ? Warn : Cream; };
			auto Heading = [this](const FString& Words) -> TSharedRef<SWidget> { return SNew(STextBlock).Text(FText::FromString(Words)).Font(Font(TEXT("Bold"), 10, 220)).ColorAndOpacity(BrightBrass); };
			auto Go = [this](const FString& Act) { return [this, Act]() { if (Controller.IsValid() && !Act.IsEmpty()) { Controller->PressPage(Act); } return FReply::Handled(); }; };
			auto Flag = [this](const FString& Tag, float Wide, float High) -> TSharedRef<SWidget> { return SNew(SBox).WidthOverride(Wide).HeightOverride(High)[SNew(SImage).Image(FlagBrush(Tag))]; };
			auto ShowOnMap = [this, &Go](int32 Region, bool bAlarm = false) -> TSharedRef<SWidget>
			{
				// (Rules 84.2.6; the words of ruling 211, section 2: on the warning it does what a press on the alarm's row does.)
				return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 8.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Shows the place on the map.")))
					.OnClicked_Lambda(Go(FString::Printf(TEXT("goto:%s:%d"), bAlarm ? TEXT("alarm") : TEXT("region"), Region)))
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("SHOW ON MAP"))).Font(Font(TEXT("Bold"), 10, 200)).ColorAndOpacity(Cream)
					];
			};
			if (ShownMonthDate != Card.Date)
			{
				ShownMonthDate = Card.Date;
				bMonthRowsOpen = false;
			}

			// 1. The title, and the two buttons.
			const bool bAgain = Controller.IsValid() && Controller->CanWatchAgain();
			const bool bFilm = Controller.IsValid() && Controller->HasLastReel(); // (ruling 203: the newsreel is a press away on the card)
			PageBody->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Heading(TEXT("DISPATCHES  ·  THE MONTH IN ONE LOOK"))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						// (ruling 123(b): the card that opens a campaign is headed by the moment, "As September 1939 opens")
						SNew(STextBlock).Text(FText::FromString(Card.bOpening ? FString::Printf(TEXT("As %s opens"), *Card.Date) : Card.Date)).Font(Serif(TEXT("Bold"), 30)).ColorAndOpacity(Cream)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 10.0f)).IsFocusable(false)
					.Visibility(bFilm ? EVisibility::Visible : EVisibility::Collapsed)
					.ToolTipText(FText::FromString(TEXT("The month's great stories as film. This report comes back when it is over; Space or Enter ends it.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->PlayLastReel(); } return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("THE NEWSREEL"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Cream)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 10.0f)).IsFocusable(false).IsEnabled(bAgain)
					.ToolTipText(FText::FromString(bAgain ? TEXT("Shows the other countries' moves of the month on the table. This report comes back when they are over.") : TEXT("The month's moves are kept only until the game is put away.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->WatchAgain(); } return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("TOUR THE MAP"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(bAgain ? Cream : Faint)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Aimed(TEXT("giveorders"), SNew(SButton).ButtonStyle(&BrassStyle).ContentPadding(FMargin(30.0f, 11.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Puts the report away. Enter does the same, and R opens it again.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ClosePage(); } return FReply::Handled(); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("GIVE ORDERS"))).Font(Font(TEXT("Bold"), 12, 240)).ColorAndOpacity(Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), Ink)] // (item 6: the lit button shows its key)
					])
				]
			];

			// (Rules 84.3.3; the art director, ruling 149.) Lesson 1 laid across the first month's card under its head,
			// above the score boxes and any warning band: the card moves down to make room and nothing on it is covered.
			// Here too lies any note the card leaves no margin for (the month card on a screen narrower than 16 by 9).
			PageBody->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(SStaffNote).Controller(Controller).Form(2).Scale(1.0f)
				.Visibility_Lambda([this]() { return Controller.IsValid() && (Controller->IsLessonLaid() || (bLessonInLaid && Controller->HasLessonNote())) ? EVisibility::Visible : EVisibility::Collapsed; })
			];

			// 2. The red band: at most two, for news that changes the war.
			for (const FMonthBand& Band : Card.Bands)
			{
				// (Ruling 160: red is "act", never "bad". Only a WARNING is a red band; WAR, FALLEN and FREED are news, and
				// stand in brass on a dark ground. A warning whose month of grace is spent wears its label on a cream
				// plate in red letters and a cream edge; a FREED by his own side takes the rows' green edge.)
				const bool bWarning = Band.Kind == TEXT("WARNING");
				const FLinearColor Brass300(FColor(224, 184, 114)), NewsInk(FColor(31, 24, 8));
				const FLinearColor EdgeInk = Band.bGood ? Good : Band.bSpent ? Cream : bWarning ? Bad : Brass300;
				const FString Label = Band.Title.IsEmpty() ? Band.Kind : Band.Kind + TEXT("  \u00B7  ") + Band.Title.ToUpper();
				TSharedRef<SWidget> KindTag = bWarning && !Band.bSpent
					? StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 10, 220)).ColorAndOpacity(Cream))
					: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(&MonthLabelBrush).BorderBackgroundColor(Band.bSpent ? Cream : Brass300).Padding(FMargin(7.0f, 2.0f, 7.0f, 1.0f))
						[
							SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 10, 220)).ColorAndOpacity(Band.bSpent ? FLinearColor(FColor(150, 40, 28)) : NewsInk)
						]);
				TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
					[
						KindTag
					];
				for (const FString& Tag : Band.Flags)
				{
					Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)[Flag(Tag, 38.0f, 24.0f)];
				}
				// (ruling 149: no headline is cut with dots: it wraps)
				Line->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Band.Text)).Font(Serif(TEXT("Bold"), 18)).ColorAndOpacity(Cream)
					.AutoWrapText(true).ToolTipText(FText::FromString(Band.Tip.IsEmpty() ? Band.Text : Band.Tip))
				];
				// (The sentence that says what to do stands small where the date stood; with none, the date.)
				if (!Band.Note.IsEmpty())
				{
					Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 14.0f, 0.0f)
					[
						SNew(SBox).MaxDesiredWidth(300.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Band.Note)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
						]
					];
				}
				else if (Band.Week > 0 || !Band.When.IsEmpty())
				{
					Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 14.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Band.When.IsEmpty() ? FString::Printf(TEXT("week %d"), Band.Week) : Band.When)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream.CopyWithNewOpacity(0.8f))
					];
				}
				if (Band.Region >= 0)
				{
					Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[ShowOnMap(Band.Region, Band.bAlarm)];
				}
				// (A second line that must be read whole is printed in full under the headline: the band is as tall as
				// it needs. Rulings 133(a) and 167: it may run to three lines, and it is never cut.)
				TSharedRef<SVerticalBox> Whole = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Line];
				if (!Band.Sub.IsEmpty())
				{
					Whole->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Band.Sub)).Font(Serif(TEXT("Regular"), 12.5f)).ColorAndOpacity(Cream.CopyWithNewOpacity(0.92f)).AutoWrapText(true).LineHeightPercentage(1.15f)
					];
				}
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(bWarning ? &MonthBandBrush : &MonthNewsBrush).Padding(FMargin(0.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(6.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(EdgeInk)]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(16.0f, 9.0f, 12.0f, 9.0f))[Whole]
					]
				];
			}

			// 3. The score: a box for each thing there is to count, one large figure in each.
			if (Card.Score.Num() > 0)
			{
				TSharedRef<SHorizontalBox> Boxes = SNew(SHorizontalBox);
				for (int32 K = 0; K < Card.Score.Num(); ++K)
				{
					const FMonthScore& Box = Card.Score[K];
					TSharedRef<SHorizontalBox> Label = SNew(SHorizontalBox);
					if (const FSlateBrush* Sign = Box.Sign.IsEmpty() || Box.Sign == TEXT("flag") ? nullptr : OrderSignBrush(Box.Sign))
					{
						Label->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(18.0f).HeightOverride(12.0f)[SNew(SImage).Image(Sign).ColorAndOpacity(Muted)]
						];
					}
					Label->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Box.Label)).Font(SmallCaps()).ColorAndOpacity(Muted)
					];
					Boxes->AddSlot().FillWidth(1.0f).Padding(K == 0 ? 0.0f : 10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&MonthBoxBrush).Padding(FMargin(12.0f, 8.0f, 12.0f, 9.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Label]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(Box.Figure)).Font(Serif(TEXT("Bold"), 26)).ColorAndOpacity(ToneInk(Box.Tone))
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(Box.Sub)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Cream.CopyWithNewOpacity(0.85f))
							]
						]
					];
				}
				// (The art director's round 36: a lone score box ran the whole width of the card. No box is wider than a
				// quarter of it: fewer than four keep a quarter each, and stand at the left.)
				if (Card.Score.Num() < 4)
				{
					Boxes->AddSlot().FillWidth(static_cast<float>(4 - Card.Score.Num())).Padding(10.0f, 0.0f, 0.0f, 0.0f)[SNew(SSpacer)];
				}
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[Boxes];
			}

			// 4 and 5. The lead story with its picture, and the player's fronts as rows beside it.
			if (!Card.Lead.bValid && Card.Rows.Num() == 0)
			{
				// (a quiet month, or a band and no fighting: one line, and no picture)
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 30.0f, 0.0f, 30.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Card.None.IsEmpty() ? FString(TEXT("A quiet month.")) : Card.None)).Font(Serif(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
				];
				if (!Card.Foot.IsEmpty())
				{
					PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Card.Foot)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
				}
			}
			else
			{
				TSharedRef<SHorizontalBox> Middle = SNew(SHorizontalBox);
				if (Card.Lead.bValid)
				{
					const FMonthLead& Lead = Card.Lead;
					const FLinearColor StampInk = ToneInk(Lead.Tone);
					const TSharedPtr<FSlateBrush> Painted = MonthPictureOf(Lead.Picture, false);
					TSharedRef<SOverlay> Print = SNew(SOverlay);
					if (Painted.IsValid())
					{
						Print->AddSlot()[SNew(SImage).Image(Painted.Get())];
					}
					Print->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(10.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).Padding(FMargin(8.0f, 3.0f, 8.0f, 3.0f))
						[
							SNew(STextBlock).Text(FText::FromString(Lead.Tag)).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
						]
					];
					if (!Lead.Stamp.IsEmpty())
					{
						Print->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f, 18.0f, 18.0f, 0.0f)
						[
							SNew(SBorder).BorderImage(&MonthStampBrush).BorderBackgroundColor(StampInk).Padding(FMargin(13.0f, 1.0f, 13.0f, 3.0f))
							.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-6.0f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))
							[
								SNew(STextBlock).Text(FText::FromString(Lead.Stamp)).Font(Serif(TEXT("Bold"), 20, 120)).ColorAndOpacity(StampInk)
							]
						];
					}
					{
						TSharedRef<SHorizontalBox> Whose = SNew(SHorizontalBox);
						auto Side = [this, &Whose, &Flag](const FString& Tag, const FString& Name)
						{
							if (!Tag.IsEmpty())
							{
								Whose->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Flag(Tag, 26.0f, 17.0f)];
							}
							Whose->AddSlot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream)
							];
						};
						Side(Lead.A, Lead.AName);
						if (!Lead.BName.IsEmpty())
						{
							Whose->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("against"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
							];
							Side(Lead.B, Lead.BName);
						}
						Print->AddSlot().VAlign(VAlign_Bottom)
						[
							SNew(SBorder).BorderImage(&ScrimBrush).Padding(FMargin(14.0f, 7.0f, 14.0f, 10.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Text(FText::FromString(Lead.Headline)).Font(Serif(TEXT("Bold"), Lead.Headline.Len() > 30 ? 20 : 24)).ColorAndOpacity(Cream).AutoWrapText(true)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[Whose]
							]
						];
					}
					TSharedRef<SVerticalBox> LeadBox = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Heading(TEXT("THE LEAD STORY"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(SBorder).BorderImage(&PrintBrush).Padding(FMargin(5.0f))
							[
								SNew(SBox).WidthOverride(460.0f).HeightOverride(292.0f)[Print]
							]
						];
					if (!Lead.Line.IsEmpty())
					{
						LeadBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Lead.Line)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
						];
					}
					if (Lead.Figures.Num() > 0)
					{
						TSharedRef<SHorizontalBox> Three = SNew(SHorizontalBox);
						for (int32 K = 0; K < Lead.Figures.Num(); ++K)
						{
							Three->AddSlot().FillWidth(1.0f).Padding(K == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SBorder).BorderImage(&MonthBoxBrush).Padding(FMargin(10.0f, 5.0f, 10.0f, 6.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Text(FText::FromString(Lead.Figures[K].Value)).Font(Serif(TEXT("Bold"), 18)).ColorAndOpacity(Cream)
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Text(FText::FromString(Lead.Figures[K].Label)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted)
									]
								]
							];
						}
						LeadBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Three];
					}
					if (Lead.Region >= 0 || Lead.Report >= 0)
					{
						TSharedRef<SHorizontalBox> Doors = SNew(SHorizontalBox);
						if (Lead.Region >= 0)
						{
							Doors->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)[ShowOnMap(Lead.Region)];
						}
						if (Lead.Report >= 0)
						{
							Doors->AddSlot().AutoWidth()
							[
								SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 8.0f)).IsFocusable(false)
								.ToolTipText(FText::FromString(TEXT("Who fought, what each lost, and what the map fed into the fight.")))
								.OnClicked_Lambda(Go(FString::Printf(TEXT("open:battle:%d"), Lead.Report)))
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("READ THE BATTLE REPORT"))).Font(Font(TEXT("Bold"), 10, 200)).ColorAndOpacity(Cream)
								]
							];
						}
						LeadBox->AddSlot().AutoHeight().Padding(0.0f, 9.0f, 0.0f, 0.0f)[Doors];
					}
					Middle->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)[SNew(SBox).WidthOverride(470.0f)[LeadBox]];
				}

				TSharedRef<SVerticalBox> Fronts = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Heading(TEXT("YOUR FRONTS  ·  THE LAND FIRST, THEN SEA AND AIR"))];
				const int32 Most = 6; // (with the lead story, about seven in all: ruling 64)
				const bool bMore = Card.Rows.Num() > Most;
				const int32 Shown = bMore && !bMonthRowsOpen ? Most : Card.Rows.Num();
				for (int32 K = 0; K < Shown; ++K)
				{
					Fronts->AddSlot().AutoHeight().Padding(0.0f, K == 0 ? 8.0f : 6.0f, 0.0f, 0.0f)[MonthRowOf(Card.Rows[K])];
				}
				if (Card.Rows.Num() == 0)
				{
					Fronts->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Nothing else was fought for on your fronts."))).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)
					];
				}
				if (!Card.Foot.IsEmpty() || bMore)
				{
					TSharedRef<SHorizontalBox> Under = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, bMore ? 12.0f : 0.0f, 0.0f)
						[
							// (no line is cut: it wraps, the UI director's review of build 1628)
							SNew(STextBlock).Text(FText::FromString(Card.Foot)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
						];
					if (bMore)
					{
						Under->AddSlot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(bMonthRowsOpen ? TEXT("Folds the list to its first rows.") : TEXT("Unfolds every row of the month.")))
							.OnClicked_Lambda([this]() { bMonthRowsOpen = !bMonthRowsOpen; ShownRoom = -1; return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(bMonthRowsOpen ? FString(TEXT("THE FIRST ROWS ONLY")) : Card.Count)).Font(Font(TEXT("Bold"), 10, 200)).ColorAndOpacity(BrightBrass)
							]
						];
					}
					Fronts->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)[Under];
				}
				Middle->AddSlot().FillWidth(1.0f)[Fronts];
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)[Middle];
			}

			// 6. Along the bottom: at home, what the staff did, and the rest of the war.
			{
				TSharedRef<SHorizontalBox> Bottom = SNew(SHorizontalBox);
				auto Boxed = [this, &Bottom, &Heading](const FString& Title, TSharedRef<SVerticalBox> Lines, bool bFirst)
				{
					Bottom->AddSlot().FillWidth(1.0f).Padding(bFirst ? 0.0f : 12.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&MonthBoxBrush).Padding(FMargin(14.0f, 10.0f, 14.0f, 10.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Heading(Title)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)[Lines]
						]
					];
				};
				auto Quiet = [this](const TCHAR* Words) -> TSharedRef<SWidget> { return SNew(STextBlock).Text(FText::FromString(Words)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true); };

				TSharedRef<SVerticalBox> Home = SNew(SVerticalBox);
				// (The art director's brief, "What the browser build taught": no word and no name is cut, so a line is
				// given whole and wraps. In a box where no line has a sign, the lines start at the box's left edge. A
				// city that finds no room beside its unit goes whole to a second line, in grey.)
				const bool bSigns = Card.Home.ContainsByPredicate([](const FMonthHome& Each) { return !Each.Sign.IsEmpty(); });
				for (const FMonthHome& Each : Card.Home)
				{
					const FLinearColor Lit = Each.bGrey ? Muted : BrightBrass, Said = Each.bGrey ? Muted : Cream;
					TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
					if (bSigns)
					{
						const FSlateBrush* Sign = Each.Sign.IsEmpty() ? nullptr : OrderSignBrush(Each.Sign);
						const bool bStar = Each.Sign == TEXT("star");
						Line->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 1.0f, 0.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(30.0f).HeightOverride(17.0f).HAlign(HAlign_Left).VAlign(VAlign_Center)
							[
								Sign ? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(bStar ? 15.0f : 25.0f).HeightOverride(bStar ? 15.0f : 17.0f)[SNew(SImage).Image(Sign).ColorAndOpacity(Lit)]) : StaticCastSharedRef<SWidget>(SNew(SSpacer))
							]
						];
					}
					if (!Each.Count.IsEmpty())
					{
						Line->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(6.0f, -3.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Each.Count)).Font(Serif(TEXT("Bold"), 16)).ColorAndOpacity(Lit)
						];
					}
					const bool bUnder = !Each.Where.IsEmpty() && Each.Text.Len() + 0.86 * Each.Where.Len() + 1.0 > 28.0;
					TSharedRef<SVerticalBox> Words = SNew(SVerticalBox);
					Words->AddSlot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Each.Where.IsEmpty() || bUnder ? Each.Text : Each.Text + TEXT("  ·  ") + Each.Where)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Said)
						.AutoWrapText(true).ToolTipText(FText::FromString(Each.Tip))
					];
					if (bUnder)
					{
						Words->AddSlot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(Each.Where)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
						];
					}
					Line->AddSlot().FillWidth(1.0f).VAlign(VAlign_Top).Padding((bSigns && Each.Count.IsEmpty()) ? 6.0f : 0.0f, 1.0f, 0.0f, 0.0f)[Words];
					Home->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 2.0f)[Line];
				}
				if (Card.Home.Num() == 0)
				{
					Home->AddSlot().AutoHeight()[Quiet(TEXT("Nothing was delivered this month, and nothing is due next month."))];
				}
				Boxed(TEXT("AT HOME"), Home, true);

				TSharedRef<SVerticalBox> Staff = SNew(SVerticalBox);
				for (const FMonthStaff& Each : Card.Staff)
				{
					TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Each.Text)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
							.ToolTipText(FText::FromString(Each.Tip))
						];
					if (!Each.Act.IsEmpty())
					{
						// (a desk's first line carries its button)
						Line->AddSlot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 6.0f)).IsFocusable(false).IsEnabled(Each.bOk)
							.ToolTipText(FText::FromString(Each.bOk ? TEXT("What your staff ordered this month stands. From next month the desk is yours to run.") : TEXT("The desk is yours again.")))
							.OnClicked_Lambda(Go(Each.Act))
							[
								SNew(STextBlock).Text(FText::FromString(Each.bOk ? TEXT("TAKE THE DESK BACK") : TEXT("THE DESK IS YOURS"))).Font(SmallCaps()).ColorAndOpacity(Each.bOk ? Cream : Faint)
							]
						];
					}
					Staff->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 2.0f)[Line];
				}
				if (Card.Staff.Num() == 0)
				{
					Staff->AddSlot().AutoHeight()[Quiet(TEXT("Your staff had nothing to tell of the desks it runs."))];
				}
				Boxed(TEXT("WHAT YOUR STAFF DID  ·  IT STANDS"), Staff, false);

				TSharedRef<SVerticalBox> Else = SNew(SVerticalBox);
				for (const FMonthElse& Each : Card.Elsewhere)
				{
					Else->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)[Flag(Each.Tag, 26.0f, 17.0f)]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(Each.Text)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
							.ToolTipText(FText::FromString(Each.Tip))
						]
					];
				}
				if (Card.Elsewhere.Num() == 0)
				{
					Else->AddSlot().AutoHeight()[Quiet(TEXT("Nothing was fought for elsewhere."))];
				}
				Boxed(TEXT("ELSEWHERE IN THE WAR"), Else, false);
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)[Bottom];
			}

			PageBody->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("R opens this report again at any time."))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
				// (what found no line on the card is counted, not lost)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(Card.Rest)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				]
			];
		}

		/**
		 * (Ruling 54.) The art director's picture of a kind of target, read once from the game's own files
		 * (Content/RaidCard): the briefing photograph, with her grease-pencil ring on the large one. Null for a kind she
		 * has not drawn, or if its file is not there: the table's own studio then stands in.
		 */
		TSharedPtr<FSlateBrush> TargetPictureOf(const FString& Sort, bool bSmall)
		{
			const TCHAR* Name = Sort == TEXT("factories") ? TEXT("factories") : Sort == TEXT("rail") ? TEXT("rail-yard") : Sort == TEXT("harbour") ? TEXT("harbour")
				: Sort == TEXT("airfield") ? TEXT("airfield") : Sort == TEXT("works") ? TEXT("fieldworks") : Sort == TEXT("troops") ? TEXT("troops") : Sort == TEXT("fleet") ? TEXT("fleet")
				: Sort == TEXT("town") ? TEXT("town") : nullptr;
			if (!Name)
			{
				return nullptr;
			}
			const FString Key = FString(Name) + (bSmall ? TEXT("-small") : TEXT(""));
			if (const TSharedPtr<FSlateBrush>* Kept = TargetPictures.Find(Key))
			{
				return *Kept;
			}
			// (a picture read from a file belongs to nobody: it is kept for the life of the game, and read once however many tables are laid)
			static TMap<FString, UTexture2D*> Read;
			UTexture2D* Texture = Read.FindRef(Key);
			if (!Texture && !Read.Contains(Key))
			{
				Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("RaidCard") / (TEXT("target-") + Key + TEXT(".png")));
				if (Texture)
				{
					Texture->AddToRoot();
				}
				Read.Add(Key, Texture);
			}
			TSharedPtr<FSlateBrush> Brush;
			if (Texture)
			{
				Brush = MakeShared<FSlateBrush>();
				Brush->SetResourceObject(Texture);
				Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush->DrawAs = ESlateBrushDrawType::Image;
			}
			TargetPictures.Add(Key, Brush);
			return Brush;
		}

		/**
		 * A machine's small studio picture with its dark ground taken away, so that on a card it stands on the row's own
		 * colour (the art director's round 32: "a flat ground in the row's colour, not noisy black"). The ground is found
		 * from the picture's border inward, so nothing dark inside the machine is lost. Made once for each machine.
		 */
		TSharedPtr<FSlateBrush> CutOutOf(const FString& Model)
		{
			if (const TSharedPtr<FSlateBrush>* Kept = CutOuts.Find(Model))
			{
				return *Kept;
			}
			static TMap<FString, UTexture2D*> Cut; // (kept for the life of the game, like the target pictures)
			UTexture2D* Texture = Cut.FindRef(Model);
			ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			if (!Texture && !Cut.Contains(Model) && Table)
			{
				UTextureRenderTarget2D* Picture = Table->GetPortrait(Model, false);
				FTextureRenderTargetResource* Resource = Picture ? Picture->GameThread_GetRenderTargetResource() : nullptr;
				TArray<FColor> Pixels;
				if (Resource && Resource->ReadPixels(Pixels) && Pixels.Num() == Picture->SizeX * Picture->SizeY && Pixels.Num() > 0)
				{
					const int32 W = Picture->SizeX, High = Picture->SizeY;
					TArray<uint8> Ground;
					Ground.Init(0, W * High);
					TArray<int32> Open;
					auto Try = [&](int32 X, int32 Y)
					{
						if (X < 0 || Y < 0 || X >= W || Y >= High)
						{
							return;
						}
						const int32 Idx = Y * W + X;
						const FColor& Seen = Pixels[Idx];
						if (!Ground[Idx] && FMath::Max3(Seen.R, Seen.G, Seen.B) < 52)
						{
							Ground[Idx] = 1;
							Open.Add(Idx);
						}
					};
					for (int32 X = 0; X < W; ++X)
					{
						Try(X, 0);
						Try(X, High - 1);
					}
					for (int32 Y = 0; Y < High; ++Y)
					{
						Try(0, Y);
						Try(W - 1, Y);
					}
					while (Open.Num() > 0)
					{
						const int32 Idx = Open.Pop(EAllowShrinking::No);
						const int32 X = Idx % W, Y = Idx / W;
						Try(X + 1, Y);
						Try(X - 1, Y);
						Try(X, Y + 1);
						Try(X, Y - 1);
					}
					for (int32 Idx = 0; Idx < Pixels.Num(); ++Idx)
					{
						Pixels[Idx].A = Ground[Idx] ? 0 : 255;
					}
					Texture = CampaignTextures::Create(W, High, Pixels, true, false, FName(*(TEXT("CutOut_") + Model.Replace(TEXT(":"), TEXT("_")))));
					if (Texture)
					{
						Texture->AddToRoot();
					}
				}
				Cut.Add(Model, Texture);
			}
			TSharedPtr<FSlateBrush> Brush;
			if (Texture)
			{
				Brush = MakeShared<FSlateBrush>();
				Brush->SetResourceObject(Texture);
				Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush->DrawAs = ESlateBrushDrawType::Image;
			}
			else
			{
				Brush = PortraitOf(Model); // (the studio's own picture, where it could not be read back)
			}
			CutOuts.Add(Model, Brush);
			return Brush;
		}

		/** A meter on the raid card: a bar of the colour given on a dark bed. */
		TSharedRef<SWidget> RaidMeter(float Share, const FLinearColor& Ink, float High = 7.0f)
		{
			return SNew(SBox).HeightOverride(High)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(Share, 0.001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Ink)]
					+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(1.0f - Share, 0.001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))]
				];
		}

		/** (The defences brief.) A place's anti-aircraft guns as three pips: brass up to the level, a hollow ring after it. */
		TSharedRef<SWidget> GunPips(int32 Level, float Size, float Gap)
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 K = 0; K < 3; ++K)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(K == 0 ? 0.0f : Gap, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(Size).HeightOverride(Size)
					[
						K < Level ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(&PipBrush).ColorAndOpacity(BrightBrass)) : StaticCastSharedRef<SWidget>(SNew(SImage).Image(&PipRingBrush))
					]
				];
			}
			return Row;
		}

		/**
		 * One air group on the raid card (the art director's sheet): its machine's picture, its name, its airfield and how
		 * far it has to fly, how many aircraft as the largest thing on it, and how ready it is. One flying the raid has a
		 * round minus (the lead group a LEADS tag and a brass frame); one that could join a round plus; one that cannot
		 * is dimmed and says why. (Morale and its reason join it when the rules have them.)
		 */
		TSharedRef<SWidget> RaidSeatCard(const FRaidSeat& Seat)
		{
			const FLinearColor Red = FLinearColor(FColor::FromHex(TEXT("CF5A41")));
			// (an escort picked for a raid planned by night stays on the card, dimmed, and stays at home)
			const bool bCan = Seat.Why.IsEmpty() && !Seat.bStays;
			const FString Model = Seat.Kind == TEXT("ftr") ? FMiniatureMesh::MainFighterOf(Seat.Nation) : FMiniatureMesh::MainBomberOf(Seat.Nation);
			const TSharedPtr<FSlateBrush> Plate = Model.IsEmpty() ? nullptr : CutOutOf(Model);
			const FString Craft = Seat.Kind == TEXT("ftr") ? TEXT("fighters") : Seat.Kind == TEXT("bmr") ? TEXT("bombers") : TEXT("aircraft");
			// (its airfield and how far it has to fly; a fighter group goes as escort)
			// (1.5.2: the distance in the bridge's own words where it sends them; a wing out of reach has no distance, and goes as nobody's escort)
			const FString Dist = Seat.Far.IsEmpty() ? FString::Printf(TEXT("%s km"), *FText::AsNumber(Seat.Km).ToString()) : Seat.Far;
			const FString Where = Seat.Kind == TEXT("ftr") && !Seat.bOutOfReach ? FString::Printf(TEXT("%s \u00B7 escort, %s"), *Seat.Base, *Dist)
				: FString::Printf(TEXT("%s \u00B7 %s"), *Seat.Base, *Dist);
			TSharedRef<SHorizontalBox> Head = SNew(SHorizontalBox);
			if (Plate.IsValid())
			{
				Head->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(104.0f).HeightOverride(64.0f)[SNew(SImage).Image(Plate.Get()).ColorAndOpacity(bCan ? FLinearColor::White : FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))]
				];
			}
			Head->AddSlot().FillWidth(1.0f).VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SPhraseText).Text(FText::FromString(Seat.Name)).Face(Font(TEXT("Bold"), 13)).Ink(bCan ? Cream : Faint).Guess(226.0f) // (on to a second line where it must be, never cut)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(SPhraseText).Text(FText::FromString(Where)).Face(Font(TEXT("Regular"), 10)).Ink(bCan ? Muted : Faint).Guess(226.0f)
					.ToolTipText(FText::FromString(Seat.bOutOfReach ? FString::Printf(TEXT("%s%s."), Seat.bAfloat ? TEXT("Aboard the ") : TEXT("Based at "), *Seat.Base)
						: FString::Printf(TEXT("%s%s. %s to fly to the target."), Seat.bAfloat ? TEXT("Aboard the ") : TEXT("Based at "), *Seat.Base, *Dist)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Text(FText::AsNumber(Seat.Aircraft)).Font(Serif(TEXT("Bold"), 22)).ColorAndOpacity(bCan ? BrightBrass : Faint)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(7.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Craft)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(bCan ? Muted : Faint)
					]
				]
			];
			if (Seat.bDayOnly)
			{
				Head->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&WayTagBrush).Padding(FMargin(6.0f, 2.0f, 6.0f, 2.0f))
					.ToolTipText(FText::FromString(Seat.bStays ? TEXT("No escort flies by night. Planned by day again, it goes with the bombers.") : TEXT("It could join the raid by day. No escort flies by night.")))
					[
						SNew(STextBlock).Text(FText::FromString(Seat.bStays ? TEXT("STAYS HOME") : TEXT("BY DAY ONLY"))).Font(SmallCaps()).ColorAndOpacity(Muted)
					]
				];
			}
			if (Seat.bLead)
			{
				Head->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(BrightBrass).Padding(FMargin(7.0f, 2.0f, 7.0f, 2.0f))
					.ToolTipText(FText::FromString(TEXT("The raid is planned round this group. Plan from another group's page to lead with that one.")))
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("LEADS"))).Font(SmallCaps()).ColorAndOpacity(FLinearColor(FColor::FromHex(TEXT("1F1808"))))
					]
				];
			}
			else if (Seat.Why.IsEmpty())
			{
				const FString Tip = Seat.bLocked ? FString(TEXT("It goes because it is ordered to stick with its bombers. Change its standing order to take it off escort."))
					: Seat.bPicked ? FString(TEXT("Leave it out of the raid."))
					: Seat.Kind == TEXT("ftr") ? FString(TEXT("Add it as escort: it leaves its patrol for the rest of the month.")) : FString(TEXT("Add it: bombers that meet up lose fewer of their number."));
				Head->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(34.0f).HeightOverride(34.0f)
					[
						SNew(SButton).ButtonStyle(&RoundStyle).ContentPadding(FMargin(0.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center).IsFocusable(false).IsEnabled(!Seat.bLocked)
						.ToolTipText(FText::FromString(Tip))
						.OnClicked_Lambda([this, Act = Seat.Act]() { if (Controller.IsValid() && !Act.IsEmpty()) { Controller->PressPage(Act); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(Seat.bPicked ? TEXT("−") : TEXT("+"))).Font(Font(TEXT("Bold"), 17)).ColorAndOpacity(Seat.bLocked ? Faint : BrightBrass)
						]
					]
				];
			}
			TSharedRef<SVerticalBox> Whole = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Head];
			Whole->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox).ToolTipText(FText::FromString(TEXT("How fit the group is to fly. Rest and refit bring it back; a raid and a move both cost it.")))
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("READY"))).Font(SmallCaps()).ColorAndOpacity(bCan ? Muted : Faint)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					RaidMeter(Seat.Ready, !bCan ? Faint : Seat.Ready < 0.5f ? Red : BrightBrass, 6.0f)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Seat.Ready * 100.0f)))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(bCan ? Cream : Faint)
				]
			];
			if (!bCan)
			{
				Whole->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					// (the reason is a sentence of the rules': it runs on from "Cannot fly", so its capital goes)
					SNew(STextBlock).Text(FText::FromString(Seat.bStays ? FString(TEXT("Stays home: no escort by night")) : TEXT("Cannot fly: ") + Seat.Why.Left(1).ToLower() + Seat.Why.Mid(1))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
				];
			}
			return SNew(SBox).WidthOverride(364.0f)
				[
					SNew(SBorder).BorderImage(Seat.bLead ? &BigCardBrush : &CardBrush).Padding(FMargin(11.0f, 10.0f, 11.0f, 10.0f))[Whole]
				];
		}

		/**
		 * (Brian's ask of 15 November 2026, the design director's ruling 54, the art director's sheet.) The raid card.
		 * Top left the target's picture as a print, with its kind, its name, its holder and how far; under it the one
		 * line of prose on the card. Top right the forecast as numbers and bars. Then the other targets in reach as a
		 * strip of small pictures; the groups flying the raid; the groups that could join; and one brass button.
		 */
		void LayOutRaidCard(const FRaidCard& Card)
		{
			const FLinearColor Red = FLinearColor(FColor::FromHex(TEXT("CF5A41")));
			const FLinearColor Ink = FLinearColor(FColor::FromHex(TEXT("1F1808")));
			auto Number = [](int32 N) { return FText::AsNumber(N).ToString(); };
			auto Heading = [this](const FString& Words) -> TSharedRef<SWidget> { return SNew(STextBlock).Text(FText::FromString(Words)).Font(Font(TEXT("Bold"), 10, 220)).ColorAndOpacity(Brass); };
			auto Go = [this](const FString& Act) { return [this, Act]() { if (Controller.IsValid() && !Act.IsEmpty()) { Controller->PressPage(Act); } return FReply::Handled(); }; };

			// The target, as the print a crew is briefed from.
			const TSharedPtr<FSlateBrush> Painted = TargetPictureOf(Card.Target.Sort, false);
			const TSharedPtr<FSlateBrush> Scene = Painted.IsValid() ? Painted : Card.Target.Picture.IsEmpty() ? nullptr : PortraitOf(Card.Target.Picture, true);
			const FString KindWord = Card.Target.Sort == TEXT("factories") ? TEXT("FACTORIES") : Card.Target.Sort == TEXT("rail") ? TEXT("RAIL YARD") : Card.Target.Sort == TEXT("harbour") ? TEXT("HARBOUR")
				: Card.Target.Sort == TEXT("airfield") ? TEXT("AIRFIELD") : Card.Target.Sort == TEXT("works") ? TEXT("FIELDWORKS") : Card.Target.Sort == TEXT("troops") ? TEXT("TROOPS")
				: Card.Target.Sort == TEXT("fleet") ? TEXT("A FLEET") : TEXT("A TOWN");
			// (a fleet is titled by its sea, with whose ships they are on the line below: the art director's round 32)
			const FString& TargetTitle = Card.Target.Title.IsEmpty() ? Card.Target.Name : Card.Target.Title;
			FString Under = Card.Target.Who.IsEmpty() ? Card.HolderName : Card.Target.Who;
			if (Card.Target.Km > 0)
			{
				Under += FString::Printf(TEXT("%s%s km%s"), Under.IsEmpty() ? TEXT("") : TEXT("  ·  "), *Number(Card.Target.Km), Card.From.IsEmpty() ? TEXT("") : *(TEXT(" from ") + Card.From));
			}
			TSharedRef<SOverlay> Print = SNew(SOverlay);
			if (Scene.IsValid())
			{
				Print->AddSlot()[SNew(SImage).Image(Scene.Get())];
			}
			Print->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(10.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SBorder).BorderImage(&ScrimBrush).Padding(FMargin(8.0f, 3.0f, 8.0f, 3.0f))
				[
					SNew(STextBlock).Text(FText::FromString(KindWord)).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
				]
			];
			{
				TSharedRef<SHorizontalBox> Whose = SNew(SHorizontalBox);
				if (!Card.Target.Holder.IsEmpty() && Card.Target.Holder != TEXT("X"))
				{
					Whose->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(24.0f).HeightOverride(16.0f)[SNew(SImage).Image(FlagBrush(Card.Target.Holder))]
					];
				}
				Whose->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SPhraseText).Text(FText::FromString(Under)).Face(Font(TEXT("Regular"), 11)).Ink(Cream).Guess(380.0f) // (on to a second line where it must be, never cut)
				];
				Print->AddSlot().VAlign(VAlign_Bottom)
				[
					SNew(SBorder).BorderImage(&ScrimBrush).Padding(FMargin(14.0f, 6.0f, 14.0f, 9.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							// (a long name, as a fleet's is, is set smaller and may take a second line)
							SNew(STextBlock).Text(FText::FromString(TargetTitle)).Font(Serif(TEXT("Bold"), TargetTitle.Len() > 22 ? 18 : 24)).ColorAndOpacity(Cream).AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)[Whose]
					]
				];
			}
			TSharedRef<SVerticalBox> TargetBox = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(&PrintBrush).Padding(FMargin(5.0f))
					[
						SNew(SBox).WidthOverride(446.0f).HeightOverride(279.0f)[Print]
					]
				];
			if (!Card.Effect.IsEmpty() || !Card.What.IsEmpty())
			{
				TargetBox->AddSlot().AutoHeight().Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(456.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Card.Effect.IsEmpty() ? Card.What : Card.Effect)).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).LineHeightPercentage(1.12f)
						.ToolTipText(FText::FromString(Card.Effects.IsEmpty() ? Card.What : Card.What.IsEmpty() ? Card.Effects : Card.What + TEXT(".\n") + Card.Effects))
					]
				];
			}

			// The forecast: one large number with its verdict, who goes and who rises, what it costs, what a hit does.
			const bool bTwoWays = Card.Ways.Num() == 2;
			TSharedRef<SVerticalBox> Forecast = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Heading(bTwoWays && !Card.WaysLabel.IsEmpty() ? Card.WaysLabel : FString(TEXT("THE FORECAST")))];
			if (bTwoWays)
			{
				// (Brian, 16 November 2026; ruling 107(d); the art director's brief.) A bombing raid by day and by night,
				// side by side: two whole small forecasts, the way two price tags hang side by side. A row above them
				// holds what is common to both. The whole column is the button; the one not chosen is never dimmed.
				{
					FString Who;
					for (const FRaidMeet& Meet : Card.Meets)
					{
						Who += FString::Printf(TEXT("%s%s at %s: about %s of %s fighters"), Who.IsEmpty() ? TEXT("") : TEXT("\n"), *Meet.Name, *Meet.Base, *Number(Meet.Able), *Number(Meet.Fighters));
					}
					auto Count = [this](const FString& Figure, const FString& Of) -> TSharedRef<SWidget>
					{
						return SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[SNew(STextBlock).Text(FText::FromString(Figure)).Font(Font(TEXT("Bold"), 16)).ColorAndOpacity(Cream)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(7.0f, 0.0f, 0.0f, 2.0f)[SNew(STextBlock).Text(FText::FromString(Of)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)];
					};
					// (The art director's defences brief: three items in a row, with a thin rule between them: the bombers
					// going, the enemy's fighters in reach, and the guns round the target.)
					auto Rule = [this]() -> TSharedRef<SWidget>
					{
						return SNew(SBox).WidthOverride(1.0f).HeightOverride(20.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Faint.CopyWithNewOpacity(0.5f))];
					};
					TSharedRef<SHorizontalBox> Fighters = SNew(SHorizontalBox).ToolTipText(FText::FromString(Card.Meets.Num() == 0 ? FString(TEXT("No enemy fighters are in reach of the target.")) : Who));
					if (Card.Meets.Num() > 0 && !Card.Meets[0].Tag.IsEmpty())
					{
						Fighters->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(21.0f).HeightOverride(14.0f)[SNew(SImage).Image(FlagBrush(Card.Meets[0].Tag))]
						];
					}
					Fighters->AddSlot().AutoWidth().VAlign(VAlign_Center)[Count(Card.WaysFoes, Card.WaysFoesWords.Replace(TEXT("enemy fighters"), TEXT("fighters")))];
					TSharedRef<SHorizontalBox> Guns = SNew(SHorizontalBox).ToolTipText(FText::FromString(Card.WaysGunsTip));
					if (const FSlateBrush* Sign = OrderSignBrush(TEXT("aa")))
					{
						Guns->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(23.0f).HeightOverride(15.0f)[SNew(SImage).Image(Sign).ColorAndOpacity(Card.WaysGuns > 0 ? Cream : Faint)]
						];
					}
					Guns->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)[GunPips(Card.WaysGuns, 7.0f, 3.5f)];
					Guns->AddSlot().AutoWidth().VAlign(VAlign_Center)[Count(Card.WaysGunsFigure, Card.WaysGunsNoun)];
					if (!Card.WaysGunsTail.IsEmpty())
					{
						// (with no guns: "· light flak only", small and dim)
						Guns->AddSlot().AutoWidth().VAlign(VAlign_Bottom).Padding(6.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Card.WaysGunsTail)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Faint)
						];
					}
					TSharedRef<SHorizontalBox> Common = SNew(SHorizontalBox);
					Common->AddSlot().AutoWidth().VAlign(VAlign_Center)[Count(Number(Card.WaysBombers), Card.WaysBombers == 1 ? TEXT("bomber") : TEXT("bombers"))];
					Common->AddSlot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Rule()];
					Common->AddSlot().AutoWidth().VAlign(VAlign_Center)[Fighters];
					Common->AddSlot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Rule()];
					Common->AddSlot().AutoWidth().VAlign(VAlign_Center)[Guns];
					Forecast->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(13.0f, 6.0f, 13.0f, 6.0f))
						[
							Common
						]
					];
				}
				auto Column = [&](const FRaidWay& Way) -> TSharedRef<SWidget>
				{
					const FLinearColor Lead = Way.bChosen ? BrightBrass : Cream;
					TSharedRef<SHorizontalBox> Top = SNew(SHorizontalBox);
					if (const FSlateBrush* Sign = OrderSignBrush(Way.bNight ? TEXT("night") : TEXT("day")))
					{
						Top->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)[SNew(SImage).Image(Sign).ColorAndOpacity(Lead)]
						];
					}
					Top->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Way.Label)).Font(Font(TEXT("Bold"), 11, 240)).ColorAndOpacity(Lead)
					];
					if (Way.bChosen)
					{
						Top->AddSlot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(BrightBrass).Padding(FMargin(8.0f, 2.0f, 8.0f, 2.0f))
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("CHOSEN"))).Font(SmallCaps()).ColorAndOpacity(Ink)
							]
						];
					}
					else
					{
						Top->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(18.0f).HeightOverride(18.0f)[SNew(SImage).Image(&WayRingBrush)]];
					}
					TSharedRef<SVerticalBox> Inside = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Top];
					if (!Way.Why.IsEmpty())
					{
						// (it cannot be flown this way this time: the reason stands in place of its figures)
						Inside->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Way.Why)).Font(Serif(TEXT("Bold"), 15)).ColorAndOpacity(Red).AutoWrapText(true)
						];
					}
					else
					{
						Inside->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox).ToolTipText(FText::FromString(Way.bChosen ? Card.Ticket : FString()))
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
							[
								SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Way.Success * 100.0f)))).Font(Serif(TEXT("Bold"), 25)).ColorAndOpacity(BrightBrass)
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom).Padding(9.0f, 0.0f, 0.0f, 5.0f)
							[
								SNew(SPhraseText).Text(FText::FromString(Way.Verdict)).Face(Serif(TEXT("Bold"), 11)).Ink(Cream).Guess(130.0f) // (on to a second line where it must be, never cut)
							]
						];
						Inside->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[RaidMeter(Way.Success, BrightBrass, 4.0f)];
						auto Lost = [&](const TCHAR* Name, const FString& Says, float Share) -> TSharedRef<SWidget>
						{
							return SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Name)).Font(SmallCaps()).ColorAndOpacity(Muted)]
									// (the rules' own words, "2% · about 7", with a little more room about the dot)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Says.Replace(TEXT(" \u00B7 "), TEXT("  \u00B7  ")))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream)]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[RaidMeter(Share, Red, 4.0f)];
						};
						Inside->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Lost(TEXT("BOMBERS LOST"), Way.Lost, Way.LostShare)];
						if (!Way.GunsWords.IsEmpty())
						{
							// (The defences brief: what is left of the bombers lost fell to fighters, and he can see that at a
							// glance, which is the reason to add an escort.)
							Inside->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Way.GunsWords)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted.CopyWithNewOpacity(0.8f))]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Way.ToGuns)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Cream)]
							];
						}
						if (Way.Hits.Num() > 0)
						{
							Inside->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("WHAT A HIT DOES"))).Font(SmallCaps()).ColorAndOpacity(Brass)]
								+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Way.StopNote)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted.CopyWithNewOpacity(0.8f))]
							];
							for (int32 K = 0; K < Way.Hits.Num() && K < 3; ++K)
							{
								const bool bDim = Way.HitDim.IsValidIndex(K) && Way.HitDim[K];
								if (Way.Hits[K].Key.IsEmpty())
								{
									// (a sentence with no figure: no industry there)
									Inside->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(Way.Hits[K].Value)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
									];
									continue;
								}
								Inside->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox).ToolTipText(FText::FromString(Way.bChosen ? Card.Effects : FString()))
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
									[
										SNew(SBox).WidthOverride(58.0f).HAlign(HAlign_Right)
										[
											SNew(STextBlock).Text(FText::FromString(Way.Hits[K].Key)).Font(Serif(TEXT("Bold"), 15)).ColorAndOpacity(bDim ? Faint : Cream)
										]
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom).Padding(10.0f, 0.0f, 0.0f, 2.0f)
									[
										SNew(SPhraseText).Text(FText::FromString(Way.Hits[K].Value)).Face(Font(TEXT("Regular"), 10)).Ink(bDim ? Faint : Muted).Guess(130.0f) // (on to a second line where it must be, never cut)
									]
								];
							}
						}
					}
					// (the foot line, at the foot whatever stands above it)
					Inside->AddSlot().FillHeight(1.0f)[SNew(SSpacer)];
					Inside->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 5.0f)[SNew(SBox).HeightOverride(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Faint.CopyWithNewOpacity(0.45f))]];
					if (Way.Why.IsEmpty() && !Way.FightersLost.IsEmpty())
					{
						// (The art director's defences brief, section 4: the column cannot grow, so when an escort flies the
						// foot line gives way to FIGHTERS LOST, in the BOMBERS LOST type and without a bar. Nothing in the
						// column moves when an escort is added or dropped.)
						Inside->AddSlot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("FIGHTERS LOST"))).Font(SmallCaps()).ColorAndOpacity(Muted)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Way.FightersLost.Replace(TEXT(" \u00B7 "), TEXT("  \u00B7  ")))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream)]
						];
					}
					else
					{
						Inside->AddSlot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(Way.Foot)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted.CopyWithNewOpacity(0.8f))
						];
					}
					return SNew(SButton).ButtonStyle(Way.bChosen ? &WayChosenStyle : &WayQuietStyle).ContentPadding(FMargin(12.0f, 9.0f, 12.0f, 8.0f)).HAlign(HAlign_Fill).VAlign(VAlign_Fill).IsFocusable(false)
						.ToolTipText(FText::FromString(Way.Tip))
						.OnClicked_Lambda(Go(Way.Act))
						[
							Inside
						];
				};
				Forecast->AddSlot().AutoHeight().Padding(0.0f, 9.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)[Column(Card.Ways[0])]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(11.0f, 0.0f, 0.0f, 0.0f)[Column(Card.Ways[1])]
				];
			}
			else if (!Card.Why.IsEmpty())
			{
				Forecast->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Card.Why)).Font(Serif(TEXT("Bold"), 20)).ColorAndOpacity(Red).AutoWrapText(true)
				];
			}
			else
			{
				Forecast->AddSlot().AutoHeight()
				[
					SNew(SHorizontalBox).ToolTipText(FText::FromString(Card.Ticket))
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Card.Success * 100.0f)))).Font(Serif(TEXT("Bold"), 44)).ColorAndOpacity(BrightBrass)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom).Padding(16.0f, 0.0f, 0.0f, 9.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Card.Verdict)).Font(Serif(TEXT("Bold"), 20)).ColorAndOpacity(Cream)
					]
				];
				Forecast->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[RaidMeter(Card.Success, BrightBrass, 9.0f)];
				// (who goes, and who rises to meet them)
				{
					int32 Able = 0;
					FString Who;
					for (const FRaidMeet& Meet : Card.Meets)
					{
						Able += Meet.Able;
						Who += FString::Printf(TEXT("%s%s at %s: about %s of %s fighters"), Who.IsEmpty() ? TEXT("") : TEXT("\n"), *Meet.Name, *Meet.Base, *Number(Meet.Able), *Number(Meet.Fighters));
					}
					auto Count = [this](const FString& Figure, const FString& Of) -> TSharedRef<SWidget>
					{
						return SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[SNew(STextBlock).Text(FText::FromString(Figure)).Font(Font(TEXT("Bold"), 17)).ColorAndOpacity(Cream)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(7.0f, 0.0f, 0.0f, 2.0f)[SNew(STextBlock).Text(FText::FromString(Of)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)];
					};
					TSharedRef<SHorizontalBox> Going = SNew(SHorizontalBox);
					if (Card.Bombers > 0)
					{
						Going->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 26.0f, 0.0f)[Count(Number(Card.Bombers), TEXT("bombers"))];
					}
					if (Card.Fighters > 0)
					{
						Going->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 26.0f, 0.0f)[Count(Number(Card.Fighters), TEXT("fighters"))];
					}
					Going->AddSlot().FillWidth(1.0f)[SNew(SSpacer)];
					if (Card.Meets.Num() > 0 && !Card.Meets[0].Tag.IsEmpty())
					{
						Going->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)[SNew(SImage).Image(FlagBrush(Card.Meets[0].Tag))]
						];
					}
					Going->AddSlot().AutoWidth().VAlign(VAlign_Center)[Count(Card.Meets.Num() == 0 ? FString(TEXT("No")) : Number(Able), Card.Meets.Num() == 0 ? TEXT("fighters in reach") : TEXT("fighters rise"))];
					Forecast->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(14.0f, 9.0f, 14.0f, 9.0f))
						.ToolTipText(FText::FromString(Card.Meets.Num() == 0 ? FString(TEXT("No enemy fighters are in reach of the target.")) : Who + (Card.FoeLoss > 0 ? FString::Printf(TEXT("\nThey are likely to lose about %s."), *Number(Card.FoeLoss)) : FString())))
						[
							Going
						]
					];
				}
				auto Lost = [&](const TCHAR* Name, float Share, int32 Aircraft, const FString& Tip) -> TSharedRef<SWidget>
				{
					return SNew(SHorizontalBox).ToolTipText(FText::FromString(Tip))
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(130.0f)[SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(TEXT("Bold"), 10, 180)).ColorAndOpacity(Muted)]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[RaidMeter(Share, Red, 9.0f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(150.0f).HAlign(HAlign_Right)
							[
								SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d%%  ·  about %s"), FMath::RoundToInt32(Share * 100.0f), *Number(Aircraft)))).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream)
							]
						];
				};
				if (Card.Bombers > 0)
				{
					Forecast->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[Lost(TEXT("BOMBERS LOST"), Card.BomberShare, Card.BomberLoss, Card.Massed)];
					if (!Card.GunsWords.IsEmpty())
					{
						Forecast->AddSlot().AutoHeight().Padding(130.0f, 3.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Card.GunsWords)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted.CopyWithNewOpacity(0.8f))]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Card.ToGuns)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Cream)]
						];
					}
				}
				if (Card.Fighters > 0)
				{
					Forecast->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Lost(TEXT("FIGHTERS LOST"), static_cast<float>(Card.FighterLoss) / FMath::Max(Card.Fighters, 1), Card.FighterLoss, Card.OffPatrol)];
				}
				if (Card.Hits.Num() > 0)
				{
					Forecast->AddSlot().AutoHeight().Padding(0.0f, 13.0f, 0.0f, 6.0f)[Heading(TEXT("WHAT A HIT DOES"))];
					TSharedRef<SHorizontalBox> Boxes = SNew(SHorizontalBox);
					for (int32 K = 0; K < 3; ++K)
					{
						TSharedRef<SWidget> Inside = SNullWidget::NullWidget;
						if (Card.Hits.IsValidIndex(K))
						{
							Inside = SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(12.0f, 6.0f, 12.0f, 8.0f)).ToolTipText(FText::FromString(Card.Effects))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Card.Hits[K].Key)).Font(Serif(TEXT("Bold"), 19)).ColorAndOpacity(Cream)]
									+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Card.Hits[K].Value)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)]
								];
						}
						Boxes->AddSlot().FillWidth(1.0f).Padding(K == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f)[Inside];
					}
					Forecast->AddSlot().AutoHeight()[Boxes];
				}
			}
			PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[TargetBox]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(26.0f, 0.0f, 0.0f, 0.0f)[Forecast]
			];

			// The other targets in reach, nearest first: a strip of small pictures to step through. The one on the card is framed in brass.
			if (Card.Strip.Num() > 1)
			{
				static const FButtonStyle& Bare = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder"));
				TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
				for (const FRaidStop& Stop : Card.Strip)
				{
					const TSharedPtr<FSlateBrush> SmallPainted = TargetPictureOf(Stop.Sort, true);
					const TSharedPtr<FSlateBrush> Small = SmallPainted.IsValid() ? SmallPainted : Stop.Picture.IsEmpty() ? nullptr : PortraitOf(Stop.Picture);
					TSharedRef<SVerticalBox> Chip = SNew(SVerticalBox);
					Chip->AddSlot().AutoHeight()
					[
						SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Stop.bNow ? BrightBrass : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)).Padding(FMargin(3.0f))
						[
							SNew(SBox).WidthOverride(108.0f).HeightOverride(67.0f)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[Small.IsValid() ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Small.Get())) : SNullWidget::NullWidget]
								+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(4.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(SBox).WidthOverride(19.0f).HeightOverride(13.0f)
									.Visibility(Stop.Holder.IsEmpty() || Stop.Holder == TEXT("X") ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
									[
										SNew(SImage).Image(FlagBrush(Stop.Holder))
									]
								]
								// (The defences brief: its guns as three small pips on a dark tab, so that he can pick the
								// softer target before he clicks through them. A target with no guns has no tab.)
								+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f, 4.0f, 4.0f, 0.0f)
								[
									SNew(SBorder).BorderImage(&ScrimBrush).Padding(FMargin(5.0f, 3.0f, 5.0f, 3.0f))
									.Visibility(Stop.Guns > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
									[
										GunPips(Stop.Guns, 6.0f, 3.0f)
									]
								]
							]
						]
					];
					Chip->AddSlot().AutoHeight().Padding(3.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(108.0f)
						[
							// (a long name takes a second line: "Western Approaches" was cut short)
							SNew(STextBlock).Text(FText::FromString(Stop.Title.IsEmpty() ? Stop.Name : Stop.Title)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Stop.bNow ? BrightBrass : Cream).AutoWrapText(true)
						]
					];
					Chip->AddSlot().AutoHeight().Padding(3.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Stop.Km > 0 ? Number(Stop.Km) + TEXT(" km") : FString())).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted)
					];
					Strip->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&Bare).ContentPadding(FMargin(0.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(Stop.bNow ? FString(TEXT("The target on the card.")) : FString::Printf(TEXT("Plan the raid against %s instead."), *Stop.Name)))
						.OnClicked_Lambda(Go(Stop.bNow ? FString() : Stop.Act))
						[
							Chip
						]
					];
				}
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Heading(FString::Printf(TEXT("%d TARGET%s IN REACH, NEAREST FIRST"), FMath::Max(Card.InReach, Card.Strip.Num()), FMath::Max(Card.InReach, Card.Strip.Num()) == 1 ? TEXT("") : TEXT("S")))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).ButtonStyle(&Bare).ContentPadding(FMargin(6.0f, 0.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(TEXT("Every target in reach as a list, with every figure in words.")))
						.OnClicked_Lambda(Go(Card.AllAct))
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("THE WHOLE LIST"))).Font(SmallCaps()).ColorAndOpacity(Muted)
						]
					]
				];
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SScrollBox).Orientation(Orient_Horizontal).ScrollBarVisibility(EVisibility::Collapsed) + SScrollBox::Slot()[Strip]
				];
			}

			// The groups: three cards across, and a row scrolls sideways when it holds more.
			TArray<const FRaidSeat*> Going, Could, Cannot;
			for (const FRaidSeat& Seat : Card.Seats)
			{
				(Seat.bPicked ? Going : Seat.Why.IsEmpty() ? Could : Cannot).Add(&Seat);
			}
			Going.StableSort([](const FRaidSeat& A, const FRaidSeat& B) { return A.bLead && !B.bLead; });
			if (Cannot.Num() > 6)
			{
				Cannot.SetNum(6);
			}
			Could.Append(Cannot);
			auto Deal = [&](const FString& Words, const TArray<const FRaidSeat*>& Seats, const TCHAR* None)
			{
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)[Heading(Words)];
				if (Seats.Num() == 0)
				{
					PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
					[
						SNew(STextBlock).Text(FText::FromString(None)).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
					];
					return;
				}
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (const FRaidSeat* Seat : Seats)
				{
					Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)[RaidSeatCard(*Seat)];
				}
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SScrollBox).Orientation(Orient_Horizontal) + SScrollBox::Slot()[Row]
				];
			};
			Deal(TEXT("FLYING THE RAID"), Going, TEXT("No group is picked. Add one from those that could join."));
			Deal(TEXT("COULD JOIN"), Could, TEXT("No other group can reach it this month."));

			// One brass button flies the raid. (Pinned to the card's foot: it names what he is about to do, and stays on
			// the screen while he chooses.)
			(PageFoot.IsValid() ? PageFoot : PageBody)->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 12.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					Aimed(TEXT("otherraid"), SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(20.0f, 10.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("The kinds of raid, to pick another.")))
					.OnClicked_Lambda(Go(Card.OtherAct))
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("ANOTHER KIND OF RAID"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Cream)
					])
				]
				// (release 1.6; Brian, 23 November 06:52Z: the campaign's air fights open as 3D air battles) FLY IT YOURSELF beside
				// FLY THE RAID, as FIGHT IT YOURSELF stands beside SEND THEM IN: a bombing raid by day or a fight for the sky, flown
				// by him in the air battle; only where the Battle rules card's "Fly raids yourself" is on and the rules have a
				// battle in the air for the raid as picked (CanFlyLive)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
					.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->CanFlyLive() ? EVisibility::Visible : EVisibility::Collapsed; })
					.ToolTipText(FText::FromString(TEXT("Fly this raid yourself: give your groups their orders in the battle in the air. FLY THE RAID lets your crews fly it.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->FlyLive(); } return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("FLY IT YOURSELF"))).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(BrightBrass)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Aimed(TEXT("fly"), SNew(SButton).ButtonStyle(&BrassStyle).ContentPadding(FMargin(38.0f, 11.0f)).IsFocusable(false).IsEnabled(Card.Fly.bOk)
					.ToolTipText(FText::FromString(Card.Fly.Tip))
					.OnClicked_Lambda(Go(Card.Fly.Act))
					[
						SNew(STextBlock).Text(FText::FromString(Card.Fly.Label.IsEmpty() ? FString(TEXT("FLY THE RAID")) : Card.Fly.Label)).Font(Font(TEXT("Bold"), 12, 240)).ColorAndOpacity(Card.Fly.bOk ? Ink : Faint)
					])
				]
			];
		}

		void LayOutPage(const FOfficePage& Page)
		{
			PageBody->ClearChildren();
			if (PageFoot.IsValid())
			{
				PageFoot->ClearChildren();
			}
			if (Page.Top.Num() > 0)
			{
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)[PageButtons(Page.Top)];
			}
			if (Page.Month.bValid)
			{
				LayOutMonthCard(Page.Month);
				return;
			}
			if (Page.Card.bValid)
			{
				LayOutRaidCard(Page.Card);
				return;
			}
			for (const FPageSection& Section : Page.Sections)
			{
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Section.Heading)).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				if (!Section.Text.IsEmpty())
				{
					PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Section.Text)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f)
					];
				}
				for (const FPageRow& Entry : Section.Rows)
				{
					TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(Entry.Title)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
						];
					if (!Entry.Alarm.IsEmpty())
					{
						// (rules version 73: a warning that goes with the row, in red under its title)
						Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 2.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Entry.Alarm)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Alarm).AutoWrapText(true)
						];
					}
					for (const FString& Line : Entry.Lines)
					{
						Words->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Line)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
						];
					}
					if (Entry.Meter >= 0.0f)
					{
						// (Rules version 71: how far a test for winning the world is met, as a bar under its words.)
						Words->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f, 0.0f, 2.0f)
						[
							SNew(SBox).WidthOverride(360.0f)[Meter(Entry.Meter, Entry.Meter >= 1.0f ? Gain : BrightBrass, TEXT("Met"))]
						];
					}
					if (Entry.Buttons.Num() > 0)
					{
						Words->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PageButtons(Entry.Buttons)];
					}
					TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
					if (!Entry.Tag.IsEmpty())
					{
						Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(36.0f).HeightOverride(24.0f)[SNew(SImage).Image(FlagBrush(Entry.Tag))]
						];
					}
					Row->AddSlot().FillWidth(1.0f)[Words];
					if (!Entry.Picture.IsEmpty())
					{
						// (The art director's ruling of round 21: the fine model of an aircraft earns its keep where the
						// player reads about it, so an air group's row carries its machine large and well lit, in the
						// studio's own light: the fighter, or the bomber where the group is mostly bombers.)
						TArray<FString> Parts;
						Entry.Picture.ParseIntoArray(Parts, TEXT(":"));
						const FString Model = Parts.Num() >= 3 && Parts[0] == TEXT("air") ? (Parts[1] == TEXT("bmr") ? FMiniatureMesh::MainBomberOf(Parts[2]) : FMiniatureMesh::MainFighterOf(Parts[2])) : FString();
						const TSharedPtr<FSlateBrush> Plate = Model.IsEmpty() ? nullptr : PortraitOf(Model, true);
						if (Plate.IsValid())
						{
							Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(14.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(290.0f).HeightOverride(176.0f)[SNew(SImage).Image(Plate.Get())]
							];
						}
					}
					PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(12.0f, 8.0f, 12.0f, 9.0f))[Row]
					];
				}
				PageBody->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(8.0f)];
			}
		}

		/** One general: his rank and level, where he is, his four skills, his traits and his record. */
		TSharedRef<SWidget> MakeGeneral(const FGeneralEntry& General)
		{
			const FLinearColor NameInk = General.bCommands ? Cream : General.bWaiting ? BrightBrass : Muted;
			TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(General.Name)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(NameInk).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  LEVEL %d"), *General.Rank.ToUpper(), General.Level))).Font(SmallCaps()).ColorAndOpacity(Brass)
					]
				]
				// How far he is toward his next level.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SBox).HeightOverride(3.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(General.Progress, 0.0001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Brass)]
						+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(1.0f - General.Progress, 0.0001f, 1.0f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f))]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(General.Where)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(General.bWaiting ? BrightBrass : Cream).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(General.Record)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
			// His four skills, each as pips, with a + when he has a point to spend.
			TSharedRef<SVerticalBox> Skills = SNew(SVerticalBox);
			for (int32 First = 0; First < General.Skills.Num(); First += 2)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 Column = 0; Column < 2; ++Column)
				{
					if (!General.Skills.IsValidIndex(First + Column))
					{
						Row->AddSlot().FillWidth(1.0f)[SNew(SSpacer)];
						continue;
					}
					const FGeneralSkill& Skill = General.Skills[First + Column];
					TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
					for (int32 Pip = 0; Pip < Skill.Max; ++Pip)
					{
						Pips->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 3.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(9.0f).HeightOverride(9.0f)[SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(Pip < Skill.Value ? BrightBrass : FLinearColor(1.0f, 1.0f, 1.0f, 0.12f))]
						];
					}
					TSharedRef<SHorizontalBox> Cell = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(74.0f).ToolTipText(FText::FromString(Skill.Blurb))
							[
								SNew(STextBlock).Text(FText::FromString(Skill.Label)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pips];
					if (Skill.bMore)
					{
						Cell->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							OfficeButton(TEXT("+"), BrightBrass, true, Skill.Blurb, [this, Id = General.Id, Key = Skill.Id]() { if (Controller.IsValid()) { Controller->SpendSkill(Id, Key); } })
						];
					}
					Row->AddSlot().FillWidth(1.0f)[Cell];
				}
				Skills->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)[Row];
			}
			Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Skills];
			if (General.Points > 0)
			{
				Card->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Gain)
					.Text(FText::FromString(FString::Printf(TEXT("%d skill point%s to spend: press a +."), General.Points, General.Points == 1 ? TEXT("") : TEXT("s"))))
				];
			}
			for (int32 Trait = 0; Trait < General.Traits.Num(); ++Trait)
			{
				Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(General.Traits[Trait])).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(General.TraitGood.IsValidIndex(Trait) && !General.TraitGood[Trait] ? Alarm : Gain).AutoWrapText(true)
				];
			}
			if (General.bRelieve)
			{
				Card->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					OfficeButton(TEXT("RELIEVE"), Muted, true, TEXT("Take him off his corps and put him back on the list, with no hard feelings. He can be given another command."),
						[this, Id = General.Id]() { if (Controller.IsValid()) { Controller->RelieveGeneral(Id); } })
				];
			}
			if (General.bSack)
			{
				const bool bAsked = Controller.IsValid() && Controller->GetSackAsked() == General.Id;
				Card->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					OfficeButton(bAsked ? TEXT("DISMISS HIM FOR GOOD? PRESS AGAIN") : TEXT("DISMISS"), bAsked ? Alarm : Muted, true,
						General.bPopular ? TEXT("He leaves the army for good and cannot be recalled. He is popular with his men: his corps loses cohesion and you lose political capital.")
							: TEXT("He leaves the army for good and cannot be recalled."),
						[this, Id = General.Id]() { if (Controller.IsValid()) { Controller->SackGeneral(Id); } })
				];
			}
			return SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(12.0f, 9.0f, 12.0f, 10.0f))[Card];
		}

		/** The generals two abreast, those waiting for a command first; beside them the corps that have nobody. */
		void LayOutGenerals(const FGeneralStaff& List)
		{
			GeneralsList->ClearChildren();
			auto AddGroup = [this, &List](const TCHAR* Title, bool bWaiting)
			{
				TArray<const FGeneralEntry*> Group;
				for (const FGeneralEntry& General : List.Generals)
				{
					if (General.bWaiting == bWaiting)
					{
						Group.Add(&General);
					}
				}
				if (Group.Num() == 0)
				{
					return;
				}
				GeneralsList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Title)).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				for (int32 First = 0; First < Group.Num(); First += 2)
				{
					TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
					for (int32 Column = 0; Column < 2; ++Column)
					{
						Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Column == 0 ? 10.0f : 0.0f, 0.0f)
						[
							Group.IsValidIndex(First + Column) ? MakeGeneral(*Group[First + Column]) : StaticCastSharedRef<SWidget>(SNew(SSpacer))
						];
					}
					GeneralsList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Row];
				}
				GeneralsList->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(8.0f)];
			};
			AddGroup(TEXT("WAITING FOR A COMMAND"), true);
			AddGroup(TEXT("IN COMMAND, AND THE WOUNDED"), false);
			if (List.Generals.Num() == 0)
			{
				GeneralsList->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("The nation has no generals yet. Promote an officer from a corps on the right."))).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
				];
			}

			GeneralsVacant->ClearChildren();
			GeneralsVacant->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("CORPS WITHOUT A GENERAL"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
			];
			if (List.Vacant.Num() == 0)
			{
				GeneralsVacant->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Every corps has its general."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
				];
			}
			for (const FVacantCorps& Corps : List.Vacant)
			{
				TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Corps.Name)).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Corps.Line)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
				TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
				if (Corps.BestId != INDEX_NONE)
				{
					Buttons->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						OfficeButton(*(TEXT("APPOINT ") + Corps.BestName.ToUpper()), BrightBrass, true, TEXT("The general on the waiting list who suits this corps best takes command of it."),
							[this, General = Corps.BestId, Id = Corps.Corps]() { if (Controller.IsValid()) { Controller->AssignGeneral(General, Id); } })
					];
				}
				Buttons->AddSlot().AutoHeight().HAlign(HAlign_Left)
				[
					OfficeButton(*FString::Printf(TEXT("PROMOTE AN OFFICER  \u00B7  %d CAPITAL"), List.PromoteCost), BrightBrass, Corps.bPromote,
						Corps.bPromote ? FString(TEXT("An unknown officer of this corps takes command at level 1. He is a rising star and learns faster than the old guard.")) : Corps.WhyPromote,
						[this, Id = Corps.Corps]() { if (Controller.IsValid()) { Controller->PromoteOfficer(Id); } })
				];
				Card->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)[Buttons];
				GeneralsVacant->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Card];
			}
		}

		/** Something set at one point along a strip: a mark, or the line for today. */
		TSharedRef<SWidget> AlongStrip(float At, float Width, float Height, const FLinearColor& Ink, const FString& Tip)
		{
			const float Where = FMath::Clamp(At, 0.0f, 1.0f);
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(Where, 0.0001f))[SNew(SSpacer)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(Width).HeightOverride(Height).ToolTipText(FText::FromString(Tip))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Ink)]
				]
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Where, 0.0001f))[SNew(SSpacer)];
		}

		/** One branch of the design offices: what is in service, what comes next, the strip of the war, and the teams on it. */
		TSharedRef<SWidget> MakeResearchField(const FResearchField& Field)
		{
			const FLinearColor PaceInk = Field.Ahead > 0 ? Gain : Field.Ahead < 0 ? Alarm : Muted;
			// The strip: the whole war from left to right, the designers' progress filled in, history's dates as marks, today as a white line.
			TSharedRef<SOverlay> Strip = SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(6.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.09f))]
				]
				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(Field.Fill, 0.0001f, 1.0f))
					[
						SNew(SBox).HeightOverride(6.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Brass)]
					]
					+ SHorizontalBox::Slot().FillWidth(FMath::Clamp(1.0f - Field.Fill, 0.0001f, 1.0f))[SNew(SSpacer)]
				];
			for (const FResearchMark& Mark : Field.Marks)
			{
				Strip->AddSlot().VAlign(VAlign_Center)[AlongStrip(Mark.At, 3.0f, 12.0f, Mark.bGot ? BrightBrass : FLinearColor(1.0f, 1.0f, 1.0f, 0.4f), Mark.Name)];
			}
			Strip->AddSlot().VAlign(VAlign_Center)[AlongStrip(Field.Today, 2.0f, 18.0f, FLinearColor::White, TEXT("History today"))];

			TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
			for (int32 Pip = 0; Pip < Field.Max; ++Pip)
			{
				Pips->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(14.0f).HeightOverride(14.0f)[SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(Pip < Field.Teams ? BrightBrass : FLinearColor(1.0f, 1.0f, 1.0f, 0.12f))]
				];
			}
			TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Field.Label)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Field.Pace.ToUpper())).Font(SmallCaps()).ColorAndOpacity(PaceInk)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("In service: ") + Field.Now)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(BrightBrass).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Field.Next)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
				[
					SNew(SBox).HeightOverride(20.0f)[Strip]
				];
			if (!Field.Effect.IsEmpty())
			{
				Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Field.Effect)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Field.bGood ? Gain : Alarm).AutoWrapText(true)
				];
			}
			Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					OfficeButton(TEXT("\u2212"), BrightBrass, Field.bFewer, TEXT("Take a team off this branch."), [this, Id = Field.Id, N = Field.Teams - 1]() { if (Controller.IsValid()) { Controller->SetResearchTeams(Id, N); } })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)[Pips]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					OfficeButton(TEXT("+"), BrightBrass, Field.bMore, TEXT("Put another team on this branch. It needs an idle team."), [this, Id = Field.Id, N = Field.Teams + 1]() { if (Controller.IsValid()) { Controller->SetResearchTeams(Id, N); } })
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Field.Work)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Field.Teams == 0 ? Alarm : Muted).AutoWrapText(true)
				]
			];
			return SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(12.0f, 9.0f, 12.0f, 10.0f))[Card];
		}

		/** The design offices: the branches two abreast, and beside them more teams to found and what the other powers have. */
		/** One technology of the tree: its name, where it stands, what it does, and the button that starts or stops it. */
		TSharedRef<SWidget> MakeTech(const FTechNode& Node)
		{
			const bool bDone = Node.State == TEXT("done"), bWork = Node.State == TEXT("work"), bReady = Node.State == TEXT("ready");
			const FLinearColor Ink = bDone ? Gain : bWork ? BrightBrass : bReady ? Cream : Faint;
			FString Stands;
			if (bDone)
			{
				Stands = TEXT("Researched");
			}
			else if (bWork)
			{
				Stands = FString::Printf(TEXT("In work: %d month%s to go"), Node.Left, Node.Left == 1 ? TEXT("") : TEXT("s"));
			}
			else if (bReady)
			{
				Stands = FString::Printf(TEXT("%d month%s of a team's work"), Node.Months, Node.Months == 1 ? TEXT("") : TEXT("s"));
			}
			else
			{
				Stands = TEXT("Needs ") + Node.Needs;
			}
			TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Node.Name)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Ink).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Stands)).Font(Font(bWork ? TEXT("Bold") : TEXT("Regular"), 9)).ColorAndOpacity(bWork ? BrightBrass : Muted).AutoWrapText(true)
				];
			if (bWork && Node.Of > 0)
			{
				const float Share = FMath::Clamp(1.0f - static_cast<float>(Node.Left) / Node.Of, 0.0f, 1.0f);
				Card->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(SBox).HeightOverride(3.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(FMath::Max(Share, 0.0001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(BrightBrass)]
						+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Share, 0.0001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
					]
				];
			}
			Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Node.Rule)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bDone ? Muted : Faint).AutoWrapText(true)
			];
			if (Node.bCan)
			{
				Card->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					OfficeButton(TEXT("RESEARCH"), BrightBrass, true, TEXT("Put a free research team to work on this."), [this, Id = Node.Id]() { if (Controller.IsValid()) { Controller->StartTech(Id); } })
				];
			}
			else if (bReady && !Node.Why.IsEmpty())
			{
				Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Node.Why)).Font(Font(TEXT("Italic"), 9)).ColorAndOpacity(Faint).AutoWrapText(true)
				];
			}
			if (Node.bStop)
			{
				Card->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					OfficeButton(TEXT("STOP"), Muted, true, TEXT("Take the team off this. The work done on it is lost."), [this, Id = Node.Id]() { if (Controller.IsValid()) { Controller->StopTech(Id); } })
				];
			}
			return SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(9.0f, 6.0f, 9.0f, 7.0f))
				.ToolTipText(FText::FromString(Node.Text + TEXT("\n") + Node.Rule))
				[
					Card
				];
		}

		/** The technology tree: a column for each branch, and down each column the four steps of the war. */
		void LayOutTech(const FResearch& Labs)
		{
			ResearchFields->ClearChildren();
			ResearchOthers->ClearChildren();
			TSharedRef<SHorizontalBox> Columns = SNew(SHorizontalBox);
			for (int32 Column = 0; Column < Labs.Branches.Num(); ++Column)
			{
				const FTechBranch& Branch = Labs.Branches[Column];
				TSharedRef<SVerticalBox> List = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d OF %d"), *Branch.Label.ToUpper(), Branch.Done, Branch.Total))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Branch.Text)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
				for (int32 Tier = 1; Tier <= 4; ++Tier)
				{
					bool bHeaded = false;
					for (const FTechNode& Node : Labs.Nodes)
					{
						if (Node.Branch != Branch.Id || Node.Tier != Tier)
						{
							continue;
						}
						if (!bHeaded)
						{
							bHeaded = true;
							List->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 4.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Labs.Eras.IsValidIndex(Tier) ? Labs.Eras[Tier].ToUpper() : FString())).Font(SmallCaps()).ColorAndOpacity(Faint)
							];
						}
						List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)[MakeTech(Node)];
					}
				}
				Columns->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Column == Labs.Branches.Num() - 1 ? 0.0f : 10.0f, 0.0f)[List];
			}
			ResearchFields->AddSlot().AutoHeight()[Columns];
		}

		void LayOutResearch(const FResearch& Labs)
		{
			if (Labs.bTree)
			{
				LayOutTech(Labs);
				return;
			}
			ResearchFields->ClearChildren();
			if (Labs.bOwn)
			{
				ResearchFields->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
					.Text(FText::FromString(TEXT("Each strip is the whole war. The brass bar is how far your designers have come, the marks are each design's date in history, and the white line is today.")))
				];
			}
			for (int32 First = 0; First < Labs.Fields.Num(); First += 2)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 Column = 0; Column < 2; ++Column)
				{
					const int32 Index = First + Column;
					Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Column == 0 ? 10.0f : 0.0f, 0.0f)
					[
						Labs.Fields.IsValidIndex(Index) ? MakeResearchField(Labs.Fields[Index]) : StaticCastSharedRef<SWidget>(SNew(SSpacer))
					];
				}
				ResearchFields->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Row];
			}
			if (!Labs.bOwn)
			{
				ResearchFields->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(TEXT("A power that pushes one branch hard gets its designs early; one that neglects a branch falls behind. What the great powers have in service today is listed on the right.")))
				];
			}

			ResearchOthers->ClearChildren();
			if (Labs.bOwn)
			{
				ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("MORE DESIGN TEAMS"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				if (!Labs.bBureau)
				{
					ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("The nation has all the design teams its engineers can staff."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
					];
				}
				else
				{
					TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Found a new design team"))).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Labs.bBureauCan ? Cream : Faint)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s from the treasury"), *Cash(Labs.BureauCost)))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Labs.bBureauCan ? BrightBrass : Alarm)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
							.Text(FText::FromString(Labs.BureauWhy.IsEmpty() ? FString(TEXT("One more team to put on any branch, for the rest of the war.")) : Labs.BureauWhy))
						];
					ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 7.0f, 11.0f, 8.0f)).IsFocusable(false).IsEnabled(Labs.bBureauCan)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->FoundResearchBureau(); } return FReply::Handled(); })
						[
							Words
						]
					];
				}
			}
			ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("WHAT THE OTHER POWERS HAVE IN SERVICE"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass).AutoWrapText(true)
			];
			for (const FResearchPower& Power : Labs.Others)
			{
				ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)[SNew(SImage).Image(FlagBrush(Power.Tag))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Power.Doctrine.IsEmpty() ? Power.Name : Power.Name + TEXT("  \u00B7  ") + Power.Doctrine)).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
					]
				];
				for (int32 Line = 0; Line < Power.Labels.Num() && Line < Power.Designs.Num(); ++Line)
				{
					ResearchOthers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(100.0f)[SNew(STextBlock).Text(FText::FromString(Power.Labels[Line].Replace(TEXT("Infantry weapons and guns"), TEXT("Infantry")).Replace(TEXT("Radar and signals"), TEXT("Radar")).Replace(TEXT("War production"), TEXT("Production")).Replace(TEXT("Army doctrine"), TEXT("Doctrine")))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Faint)]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Power.Designs[Line])).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
						]
					];
				}
				ResearchOthers->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(12.0f)];
			}
		}

		static FText DiplomacySummary(const FDiplomacy& Office)
		{
			FString Line = FString::Printf(TEXT("%d more each month, up to %d.  "), Office.Income, Office.Cap);
			if (Office.Bloc.Contains(TEXT("Axis")) || Office.Bloc.Contains(TEXT("Allies")))
			{
				Line += FString::Printf(TEXT("%d trade partner%s.  "), Office.Partners, Office.Partners == 1 ? TEXT("") : TEXT("s"));
			}
			Line += Office.Wind;
			if (!Office.bUp)
			{
				Line += TEXT("  It is not your month, so nothing can be done here now.");
			}
			return FText::FromString(Line);
		}

		/** The track a country at peace stands on: the Allies at the left, the Axis at the right, and a dot where its opinion is. */
		TSharedRef<SWidget> OpinionTrack(int32 Opinion, const FLinearColor& Dot)
		{
			const float Where = FMath::Clamp(0.5f + Opinion / 200.0f, 0.0f, 1.0f);
			const FLinearColor Line(1.0f, 1.0f, 1.0f, 0.14f);
			const FLinearColor Mark(1.0f, 1.0f, 1.0f, 0.32f);
			auto Tick = [this, &Mark](float Height)
			{
				return SNew(SBox).WidthOverride(2.0f).HeightOverride(Height).VAlign(VAlign_Fill)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Mark)];
			};
			// The inner marks are where a country starts to lean; the outer marks are where it asks to join.
			return SNew(SBox).HeightOverride(14.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(2.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Line)]
				]
				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.15f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Tick(8.0f)]
					+ SHorizontalBox::Slot().FillWidth(0.20f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Tick(8.0f)]
					+ SHorizontalBox::Slot().FillWidth(0.15f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Tick(14.0f)]
					+ SHorizontalBox::Slot().FillWidth(0.15f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Tick(8.0f)]
					+ SHorizontalBox::Slot().FillWidth(0.20f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Tick(8.0f)]
					+ SHorizontalBox::Slot().FillWidth(0.15f)[SNew(SSpacer)]
				]
				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(Where, 0.0001f))[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(12.0f).HeightOverride(12.0f)[SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(Dot)]
					]
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Where, 0.0001f))[SNew(SSpacer)]
				]
			];
		}

		/** A small button on a row of the foreign office. */
		/**
		 * (Rules version 66) A bar at the head of a desk's own screen. Where the player's staff runs the desk it says
		 * so, gives the staff's line for the month and offers to take the desk back; where the player runs it, it is
		 * one quiet button that hands it over. Nothing at all outside the grand campaign.
		 */
		TSharedRef<SWidget> DeskBar(const TCHAR* Desk)
		{
			const FString Id(Desk);
			auto Found = [this, Id]() -> const FViewDesk* { return Controller.IsValid() ? Controller->FindDesk(Id) : nullptr; };
			return SNew(SBorder).BorderImage(&BarBrush).Padding(FMargin(12.0f, 6.0f))
				.BorderBackgroundColor_Lambda([Found]() { const FViewDesk* D = Found(); return FSlateColor(D && D->bStaff ? FLinearColor(0.72f, 0.55f, 0.24f, 0.16f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); })
				.Visibility_Lambda([Found]() { return Found() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
						.Text_Lambda([Found]() { const FViewDesk* D = Found(); return D ? FText::FromString(D->Line) : FText::GetEmpty(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Aimed(FName(*(TEXT("deskbar:") + Id)), SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
						.ToolTipText_Lambda([Found]() { const FViewDesk* D = Found(); return D ? FText::FromString(D->Tip) : FText::GetEmpty(); })
						.OnClicked_Lambda([this, Id]() { if (Controller.IsValid()) { Controller->PressDesk(Id); } return FReply::Handled(); })
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 10, 160))
							.ColorAndOpacity_Lambda([Found]() { const FViewDesk* D = Found(); return FSlateColor(D && D->bStaff ? BrightBrass : Muted); })
							.Text_Lambda([Found]() { const FViewDesk* D = Found(); return D ? FText::FromString(D->Label) : FText::GetEmpty(); })
						])
					]
				];
		}

		TSharedRef<SWidget> OfficeButton(const TCHAR* Label, const FLinearColor& Ink, bool bEnabled, const FString& Tip, TFunction<void()> Pressed)
		{
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false).IsEnabled(bEnabled)
				.ToolTipText(FText::FromString(Tip))
				.OnClicked_Lambda([Pressed]() { Pressed(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 10, 160)).ColorAndOpacity(bEnabled ? Ink : Faint)
				];
		}

		/** A country asked into the League: its flag, its name and its reason, and the button that asks it. */
		TSharedRef<SWidget> LeagueRow(const FLeagueAnswer& Answer, const TCHAR* Label, const FLinearColor& Ink, bool bEnabled, TFunction<void()> Pressed)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 8.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(27.0f).HeightOverride(18.0f)[SNew(SImage).Image(FlagBrush(Answer.Tag))]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Answer.Name)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Answer.Why)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted).AutoWrapText(true)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					OfficeButton(Label, Ink, bEnabled, Answer.Why, Pressed)
				];
		}

		/** The foreign office: the powers at war down the left, the countries at peace on their track down the right. */
		void LayOutDiplomacy(const FDiplomacy& Office)
		{
			const float Mine = Office.Bloc.Contains(TEXT("Axis")) ? 1.0f : Office.Bloc.Contains(TEXT("Allies")) ? -1.0f : 0.0f;

			DiplomacyPowers->ClearChildren();
			const FLinearColor Teal = FLinearColor(FColor::FromHex(TEXT("5CC4B6")));
			// The doors to the rooms beyond this one: the secret service, and the last act when its time comes.
			{
				TArray<FString> Ids, Labels;
				if (Controller.IsValid())
				{
					Controller->GetPageLinks(Ids, Labels);
				}
				if (Ids.Num() > 0)
				{
					TSharedRef<SWrapBox> Doors = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0f, 5.0f));
					for (int32 Door = 0; Door < Ids.Num() && Door < Labels.Num(); ++Door)
					{
						Doors->AddSlot()
						[
							OfficeButton(*Labels[Door], BrightBrass, true, TEXT("Open this room."), [this, Id = Ids[Door]]() { if (Controller.IsValid()) { Controller->OpenPage(Id); } })
						];
					}
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)[Doors];
				}
			}
			const FString Asked = Controller.IsValid() ? Controller->GetPathAsked() : FString();

			// A small country's own path: join a bloc, stand alone, or found or join the League.
			if (Office.Paths.Num() > 0)
			{
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("YOUR COUNTRY'S PATH"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Office.PathLead)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
				];
				for (const FPathOption& Option : Office.Paths)
				{
					const bool bLeagueWay = Option.Id == TEXT("found") || Option.Id == TEXT("join");
					const FLinearColor Ink = Option.bNow ? (bLeagueWay ? Teal : BrightBrass) : Option.bOk ? Cream : Faint;
					TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(Option.Title)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Ink)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(Option.bNow ? TEXT("YOUR PATH TODAY") : TEXT(""))).Font(SmallCaps()).ColorAndOpacity(Gain)
							]
						];
					if (!Option.Text.IsEmpty())
					{
						Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Option.Text)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
						];
					}
					if (!Option.bOk && !Option.bNow && !Option.Why.IsEmpty())
					{
						Card->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Option.Why)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
						];
					}
					if (Option.bOk && !Option.bNow && Option.Id != TEXT("found"))
					{
						// Two presses: the first asks, the second takes the path.
						if (Asked == Option.Id)
						{
							Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Option.bFinal ? TEXT("An alliance, once joined, is not left. Are you sure?") : TEXT("Are you sure?"))).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(Alarm).AutoWrapText(true)
							];
							Card->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
								[
									OfficeButton(*(TEXT("YES, ") + Option.Title.ToUpper()), BrightBrass, true, FString(), [this, Id = Option.Id]() { if (Controller.IsValid()) { Controller->PressPath(Id); } })
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									OfficeButton(TEXT("NO"), Muted, true, FString(), [this]() { if (Controller.IsValid()) { Controller->PressPath(FString()); } })
								]
							];
						}
						else
						{
							Card->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f, 0.0f, 0.0f)
							[
								OfficeButton(*Option.Title.ToUpper(), BrightBrass, true, FString(), [this, Id = Option.Id]() { if (Controller.IsValid()) { Controller->PressPath(Id); } })
							];
						}
					}
					if (Option.Id == TEXT("found") && Option.bOk)
					{
						// The countries that would found it with the player: willing ones first, each with its reason.
						int32 Refusing = 0;
						for (const FLeagueAnswer& Answer : Office.Founders)
						{
							if (!Answer.bOk)
							{
								++Refusing;
								continue;
							}
							const bool bAsked = Asked == TEXT("found:") + Answer.Tag;
							Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[LeagueRow(Answer, bAsked ? TEXT("YES, FOUND IT") : TEXT("FOUND IT TOGETHER"), bAsked ? Teal : BrightBrass, true,
								[this, Tag = Answer.Tag]() { if (Controller.IsValid()) { Controller->PressPath(TEXT("found"), Tag); } })];
						}
						if (Refusing > 0)
						{
							Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
								.Text(FText::FromString(FString::Printf(TEXT("%d other %s would refuse today: they lean toward a bloc, or see no danger yet."), Refusing, Refusing == 1 ? TEXT("country") : TEXT("countries"))))
							];
						}
					}
					if (Option.Id == TEXT("join") && Option.bNow && Office.Invites.Num() > 0)
					{
						Card->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 10, 160)).ColorAndOpacity(Brass)
							.Text(FText::FromString(FString::Printf(TEXT("INVITE A COUNTRY  \u00B7  %d POLITICAL CAPITAL EACH"), Office.InviteCost)))
						];
						int32 Refusing = 0;
						for (const FLeagueAnswer& Answer : Office.Invites)
						{
							if (!Answer.bOk)
							{
								++Refusing;
								continue;
							}
							Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[LeagueRow(Answer, TEXT("INVITE"), BrightBrass, Answer.bCan,
								[this, Tag = Answer.Tag]() { if (Controller.IsValid()) { Controller->InviteToLeague(Tag); } })];
						}
						if (Refusing > 0)
						{
							Card->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
								.Text(FText::FromString(FString::Printf(TEXT("%d other %s would refuse today."), Refusing, Refusing == 1 ? TEXT("country") : TEXT("countries"))))
							];
						}
					}
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(11.0f, 8.0f, 11.0f, 9.0f))[Card]
					];
				}
				DiplomacyPowers->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(10.0f)];
			}

			// The League, once it has been founded: its members stand by one another.
			if (Office.bLeague)
			{
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Office.LeagueName.ToUpper())).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Teal)
				];
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(FString::Printf(TEXT("A third alliance, of smaller nations, that belongs to neither the Axis nor the Allies. Founded %s. War on one member is war on them all. Its ground is teal on the map."), *Office.LeagueSince)))
				];
				if (Office.Members.Num() == 0)
				{
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("None of its members is left."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
					];
				}
				for (const FDiplomacyPower& Member : Office.Members)
				{
					TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(36.0f).HeightOverride(24.0f)[SNew(SImage).Image(FlagBrush(Member.Tag))]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(STextBlock).Text(FText::FromString(Member.Name)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Member.Side == TEXT("own") ? BrightBrass : Teal)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
									.Text(FText::FromString(FString::Printf(TEXT("%s%d division%s"), Member.Side == TEXT("own") ? TEXT("You  \u00B7  ") : TEXT(""), Member.Divisions, Member.Divisions == 1 ? TEXT("") : TEXT("s"))))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Member.Note)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
							]
						];
					if (Member.bWar)
					{
						Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean: the whole League stands by it. Nothing happens until you confirm."), *ACampaignPlayerController::WithThe(Member.Name)),
								[this, Tag = Member.Tag]() { if (Controller.IsValid()) { Controller->AskWarOn(Tag); } })
						];
					}
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Row];
				}
				DiplomacyPowers->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(10.0f)];
			}

			// (Rules version 70) Claims and ambitions, worded as the rules' own page words them: who claims what ground
			// from whom, and how far it has gone. A click on one shows its ground on the map. The block stands above the
			// powers at war when any claim is on the player's own ground, and below them when none is.
			const bool bClaimOnMe = Office.Claims.ContainsByPredicate([](const FDiplomacy::FClaim& Claim) { return Claim.bOnYou; });
			auto ShowClaims = [this, &Office]()
			{
				if (Office.ClaimsLead.IsEmpty())
				{
					return;
				}
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("CLAIMS AND AMBITIONS"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).Text(FText::FromString(Office.ClaimsLead))
				];
				for (const FDiplomacy::FClaim& Claim : Office.Claims)
				{
					TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream).AutoWrapText(true)
							.Text(FText::FromString(Claim.Title + (Claim.bOnYou ? TEXT("  \u00B7  YOUR GROUND") : TEXT(""))))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).Text(FText::FromString(Claim.From))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Claim.State == 2 ? Alarm : Claim.State == 1 ? BrightBrass : Faint).AutoWrapText(true).Text(FText::FromString(Claim.Tag))
						];
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(8.0f, 5.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(Claim.Region >= 0 ? TEXT("Show this ground on the map.") : TEXT("")))
						.OnClicked_Lambda([this, Region = Claim.Region]()
						{
							if (Controller.IsValid() && Region >= 0)
							{
								Controller->CloseDiplomacy();
								Controller->ShowProvince(Region);
							}
							return FReply::Handled();
						})
						[
							Lines
						]
					];
				}
				DiplomacyPowers->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(12.0f)];
			};
			if (bClaimOnMe)
			{
				ShowClaims();
			}


			DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("THE POWERS AT WAR"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
			];
			DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
				.Text(FText::FromString(TEXT("The bar shows how much fight a nation has left. It drains as its capital, its territory and its army are lost.")))
			];
			if (Office.Powers.Num() == 0)
			{
				DiplomacyPowers->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Nobody is at war."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
				];
			}
			for (const FDiplomacyPower& Power : Office.Powers)
			{
				const FLinearColor Ink = !Power.bAlive ? Faint : Power.Side == TEXT("own") ? BrightBrass : Power.Side == TEXT("ally") ? Gain : Power.Side == TEXT("enemy") ? Alarm : Cream;
				const TCHAR* Standing = Power.Side == TEXT("own") ? TEXT("You") : Power.Side == TEXT("ally") ? TEXT("Your ally") : Power.Side == TEXT("enemy") ? TEXT("Your enemy") : TEXT("At peace with you");
				TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(Power.Name)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
							.Text(FText::FromString(Power.bAlive ? FString::Printf(TEXT("%d division%s"), Power.Divisions, Power.Divisions == 1 ? TEXT("") : TEXT("s")) : FString(TEXT("Fallen"))))
						]
					];
				if (Power.bAlive)
				{
					Words->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox).HeightOverride(5.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(Power.Will, 0.0001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Power.Will < 0.35f ? Alarm : Ink)]
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Power.Will, 0.0001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f))]
						]
					];
				}
				Words->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Power.bAlive ? FString::Printf(TEXT("%s. %s"), Standing, *Power.Note) : Power.Note)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Power.bAlive ? Muted : Faint).AutoWrapText(true)
				];
				if (Power.bAlive && !Power.StaffLabel.IsEmpty())
				{
					// (Version 65) Its general staff: its character in a word, and what it is doing. A main effort aimed at the
					// player or his allies is written as a threat, with a way to go and look at the place.
					TSharedRef<SHorizontalBox> Staff = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 1.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Power.StaffLabel.ToUpper())).Font(SmallCaps()).ColorAndOpacity(Power.bThreat ? Alarm : Brass)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
						[
							SNew(STextBlock).Text(FText::FromString(Power.StaffLine)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Power.bThreat ? Alarm : Muted).AutoWrapText(true)
						];
					if (Power.bThreat && Power.ThreatRegion != INDEX_NONE)
					{
						Staff->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							OfficeButton(*FString::Printf(TEXT("GO TO %s"), *Power.ThreatName.ToUpper()), Alarm, true, TEXT("Its main effort is aimed here. Looks at the place on the table."),
								[this, Region = Power.ThreatRegion]() { if (Controller.IsValid()) { Controller->CloseDiplomacy(); Controller->ShowProvince(Region); } })
						];
					}
					Words->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox).ToolTipText(FText::FromString(Power.StaffSays))[Staff]
					];
				}
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(36.0f).HeightOverride(24.0f)[SNew(SImage).Image(FlagBrush(Power.Tag)).ColorAndOpacity(Power.bAlive ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.45f))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[Words];
				if (Power.bWar)
				{
					Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean. Nothing happens until you confirm."), *ACampaignPlayerController::WithThe(Power.Name)),
							[this, Tag = Power.Tag]() { if (Controller.IsValid()) { Controller->AskWarOn(Tag); } })
					];
				}
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[Row];
			}
			if (!bClaimOnMe)
			{
				ShowClaims();
			}

			// The head of the track stays put while the countries scroll under it.
			DiplomacyHead->ClearChildren();
			DiplomacyHead->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("THE COUNTRIES AT PEACE"))).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
			];
			DiplomacyHead->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 12.0f, 8.0f)
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
				.Text(FText::FromString(TEXT("Every country stands somewhere between the two blocs. The dot is its public opinion: past the inner marks it leans and trades with that side, past the outer marks it joins. ") + Office.Help))
			];
			DiplomacyHead->AddSlot().AutoHeight().Padding(250.0f, 0.0f, 186.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)[SNew(STextBlock).Text(FText::FromString(TEXT("ALLIES"))).Font(SmallCaps()).ColorAndOpacity(Mine < 0.0f ? Gain : Mine > 0.0f ? Alarm : Muted)]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("INDEPENDENT"))).Font(SmallCaps()).ColorAndOpacity(Faint)]
				+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)[SNew(STextBlock).Text(FText::FromString(TEXT("AXIS"))).Font(SmallCaps()).ColorAndOpacity(Mine > 0.0f ? Gain : Mine < 0.0f ? Alarm : Muted)]
			];

			DiplomacyRows->ClearChildren();
			if (Office.Rows.Num() == 0)
			{
				DiplomacyRows->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("No country is at peace any longer."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint)
				];
			}
			for (const FDiplomacyRow& Entry : Office.Rows)
			{
				const float Toward = Entry.Opinion * Mine;
				const FLinearColor Dot = Mine == 0.0f || FMath::Abs(Entry.Opinion) < 8 ? Cream : Toward > 0.0f ? Gain : Alarm;
				FString Under = Entry.Label;
				if (Entry.bTrades)
				{
					Under += TEXT("  \u00B7  trades with you");
				}
				TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Entry.Name)).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Under)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
					];
				TSharedRef<SVerticalBox> Middle = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[OpinionTrack(Entry.Opinion, Dot)];
				FString Small = FString::Printf(TEXT("%d division%s"), Entry.Divisions, Entry.Divisions == 1 ? TEXT("") : TEXT("s"));
				if (!Entry.Word.IsEmpty())
				{
					Small += TEXT("  \u00B7  ") + Entry.Word;
				}
				if (!Entry.Friends.IsEmpty())
				{
					Small += TEXT("  \u00B7  ") + Entry.Friends;
				}
				Middle->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Small)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
				];
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
				if (Entry.bCourt)
				{
					const FString Tip = Entry.bCanCourt
						? FString::Printf(TEXT("Send envoys to %s for %d political capital. %s."), *Entry.Name, Office.Cost, *Entry.Word)
						: Entry.Why;
					Buttons->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						OfficeButton(TEXT("COURT"), BrightBrass, Entry.bCanCourt, Tip, [this, Tag = Entry.Tag]() { if (Controller.IsValid()) { Controller->CourtNation(Tag); } })
					];
				}
				if (Entry.bWar)
				{
					Buttons->AddSlot().AutoWidth()
					[
						OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean. Nothing happens until you confirm."), *ACampaignPlayerController::WithThe(Entry.Name)),
							[this, Tag = Entry.Tag]() { if (Controller.IsValid()) { Controller->AskWarOn(Tag); } })
					];
				}
				DiplomacyRows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 9.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 10.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(36.0f).HeightOverride(24.0f)[SNew(SImage).Image(FlagBrush(Entry.Tag))]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SBox).WidthOverride(204.0f)[Words]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top).Padding(0.0f, 2.0f, 12.0f, 0.0f)[Middle]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SBox).WidthOverride(150.0f).HAlign(HAlign_Right)[Buttons]
					]
				];
			}
		}

		/** One line of the books: what, how many, how much. */
		TSharedRef<SWidget> LedgerRow(const FString& Label, const FString& Count, const FString& Amount, const FLinearColor& AmountColour, bool bStrong = false)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(bStrong ? TEXT("Bold") : TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Count)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Faint)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).MinDesiredWidth(74.0f).HAlign(HAlign_Right)
					[
						SNew(STextBlock).Text(FText::FromString(Amount)).Font(Font(bStrong ? TEXT("Bold") : TEXT("Regular"), 12)).ColorAndOpacity(AmountColour)
					]
				];
		}

		void LayOutLedger(const FLedger& Books)
		{
			auto Heading = [](TSharedPtr<SVerticalBox>& Box, const FString& Text, float Top)
			{
				Box->AddSlot().AutoHeight().Padding(0.0f, Top, 0.0f, 8.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass)
				];
			};
			auto Rule = [this](TSharedPtr<SVerticalBox>& Box)
			{
				Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 6.0f)
				[
					SNew(SBox).HeightOverride(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Faint)]
				];
			};
			auto Add = [](TSharedPtr<SVerticalBox>& Box, TSharedRef<SWidget> Row)
			{
				Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)[Row];
			};

			// ---- Money in.
			LedgerIn->ClearChildren();
			Heading(LedgerIn, TEXT("MONEY IN, EACH MONTH"), 0.0f);
			for (const FLedgerLine& Line : Books.Income)
			{
				Add(LedgerIn, LedgerRow(Line.Label, Line.Count > 0 ? FText::AsNumber(Line.Count).ToString() : FString(), Cash(Line.Amount), Cream));
			}
			for (const FLedgerLine& Line : Books.Changes)
			{
				Add(LedgerIn, LedgerRow(Line.Label, FString(), Cash(Line.Amount, true), Line.Amount < 0 ? Alarm : Gain));
			}
			Rule(LedgerIn);
			Add(LedgerIn, LedgerRow(TEXT("Comes in each month"), FString(), Cash(Books.Gross), BrightBrass, true));

			Heading(LedgerIn, TEXT("ON ORDER, ALREADY PAID FOR"), 22.0f);
			if (Books.Orders.Num() == 0)
			{
				LedgerIn->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Nothing is on order. New corps and works are ordered from a province's panel."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
				];
			}
			for (const FLedgerLine& Line : Books.Orders)
			{
				Add(LedgerIn, LedgerRow(Line.Label, FString(), Cash(Line.Amount), Muted));
			}
			if (Books.Orders.Num() > 1)
			{
				Rule(LedgerIn);
				Add(LedgerIn, LedgerRow(TEXT("Spent on what is forming"), FString(), Cash(Books.OnOrder), Muted, true));
			}

			// ---- Money out.
			LedgerOut->ClearChildren();
			Heading(LedgerOut, TEXT("UPKEEP, EACH MONTH"), 0.0f);
			if (!Books.bBudget)
			{
				LedgerOut->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
					.Text(FText::FromString(TEXT("In a historical scenario the forces cost nothing to keep, so that history's timetable holds. In the grand campaign every division, ship and aircraft is paid for each month.")))
				];
			}
			for (const FLedgerLine& Line : Books.Upkeep)
			{
				Add(LedgerOut, LedgerRow(Line.Label, Line.Count > 0 ? FText::AsNumber(Line.Count).ToString() : FString(), Cash(Line.Amount), Cream));
			}
			if (Books.bBudget)
			{
				Rule(LedgerOut);
				Add(LedgerOut, LedgerRow(TEXT("Goes out each month"), FString(), Cash(Books.UpkeepTotal), BrightBrass, true));
				LedgerOut->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
				[
					LedgerRow(TEXT("Left each month"), FString(), Cash(Books.Net, true), Books.Net < 0 ? Alarm : Gain, true)
				];
				LedgerOut->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
					.Text(FText::FromString(TEXT("The small figures are how many: divisions, ships, aircraft. A country at peace pays half. What is left pays for replacements first, then for whatever you order.")))
				];
			}

			// ---- Every province, richest first.
			LedgerProvinces->ClearChildren();
			Heading(LedgerProvinces, FString::Printf(TEXT("WHAT EACH PROVINCE GIVES  \u00B7  %d HELD"), Books.Provinces.Num()), 0.0f);
			for (const FLedgerProvince& Province : Books.Provinces)
			{
				TArray<FString> Parts;
				auto Part = [&Parts](const TCHAR* What, int32 Amount)
				{
					if (Amount > 0)
					{
						Parts.Add(FString::Printf(TEXT("%s %s"), What, *Cash(Amount)));
					}
				};
				Part(TEXT("factories"), Province.Factories);
				Part(TEXT("land"), Province.Land);
				Part(TEXT("harbour"), Province.Port);
				Part(TEXT("oil"), Province.Oil);
				Part(TEXT("capital"), Province.Capital);
				LedgerProvinces->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 5.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Show this province on the table.")))
					.OnClicked_Lambda([this, Region = Province.Region]() { if (Controller.IsValid()) { Controller->ShowProvince(Region); } return FReply::Handled(); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream)
								.Text(FText::FromString(Province.bHome ? Province.Name : Province.Name + TEXT("  (occupied)")))
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
								.Text(FText::FromString(FString::Join(Parts, TEXT("  \u00B7  "))))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Cash(Province.Total))).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(BrightBrass)
						]
					]
				];
			}
		}

		/** A nation's flag as a brush for the panels. A nation with no tag gets a plain dark chip. */
		const FSlateBrush* FlagBrush(const FString& Tag)
		{
			if (Tag.IsEmpty())
			{
				return &WatchBrush;
			}
			if (const TSharedPtr<FSlateBrush>* Found = FlagBrushes.Find(Tag))
			{
				return Found->Get();
			}
			ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			UTexture2D* Picture = Table ? Table->GetFlagTexture(Tag) : nullptr;
			if (!Picture)
			{
				return &WatchBrush; // not ready yet: ask again next time
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Picture);
			Brush->ImageSize = FVector2D(96.0f, 64.0f);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			FlagBrushes.Add(Tag, Brush);
			return Brush.Get();
		}

		TSharedRef<SWidget> MakeFlag(const FString& Tag, float Width)
		{
			return SNew(SBox).WidthOverride(Width).HeightOverride(Width / 1.5f)
			[
				SNew(SImage).Image(FlagBrush(Tag))
			];
		}

		/** Lays widgets out in rows of so many columns, each column the same width. */
		/** The town's bar: a picture for every building plot, and above it the choices for the plot that is picked. */
		void LayOutTownBar(const FTownBar& Bar)
		{
			TownBarBox->ClearChildren();
			if (Bar.Plots.Num() == 0)
			{
				return;
			}
			auto Picture = [this](const FString& Model, const FVector2D& Size) -> TSharedRef<SWidget>
			{
				const TSharedPtr<FSlateBrush> Portrait = PortraitOf(Model);
				return Portrait.IsValid()
					? StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(2.0f))[SNew(SPortrait).Picture(Portrait).Size(Size)])
					: StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(static_cast<float>(Size.X)).HeightOverride(static_cast<float>(Size.Y)));
			};
			auto ModelOf = [&Bar](int32 No, int32 Level) { return FString::Printf(TEXT("Civic:%d:%d:%s"), No, Level, *Bar.Tag); };

			// What could go up on the plot that is picked.
			if (Bar.bMine && Bar.Plots.IsValidIndex(Bar.Picked))
			{
				const FTownPlot& On = Bar.Plots[Bar.Picked];
				TArray<const FTownOffer*> Choices;
				for (const FTownOffer& Offer : Bar.Offers)
				{
					const bool bStands = Bar.Plots.ContainsByPredicate([&Offer](const FTownPlot& Plot) { return Plot.Line == Offer.Line; });
					if (On.Line.IsEmpty() ? !bStands : (Offer.Line == On.Line && !On.bBuilding && Offer.Has < 3))
					{
						Choices.Add(&Offer);
					}
				}
				const FString Heading = On.Line.IsEmpty() ? FString(TEXT("WHAT TO BUILD ON THIS PLOT"))
					: On.bBuilding ? FString::Printf(TEXT("%s IS GOING UP, DUE %s"), *On.Label.ToUpper(), *On.Due.ToUpper())
					: Choices.Num() > 0 ? FString(TEXT("RAISE IT A LEVEL")) : FString(TEXT("COMPLETE: IT CAN BE RAISED NO HIGHER"));
				TownBarBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Heading)).Font(SmallCaps()).ColorAndOpacity(Brass)
				];
				if (Choices.Num() > 0)
				{
					TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
					for (const FTownOffer* Each : Choices)
					{
						const FTownOffer& Offer = *Each;
						const bool bCan = Offer.Why.IsEmpty();
						const FString Tip = FString::Printf(TEXT("%s: %s\n%s\n%s%s"), *Offer.Name, *Offer.Label, *Offer.Effect, *Offer.Blurb, bCan ? TEXT("") : *(TEXT("\n") + Offer.Why));
						Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(130.0f)
							[
								SNew(SButton).ButtonStyle(bCan ? &PlainButton : &LeftCard).ContentPadding(FMargin(5.0f, 5.0f)).IsFocusable(false)
								.ToolTipText(FText::FromString(Tip))
								.OnClicked_Lambda([this, Line = Offer.Line, bCan, Why = Offer.Why]()
								{
									if (Controller.IsValid())
									{
										// (one that cannot be ordered can still be pressed, and says why)
										if (bCan) { Controller->OrderTownBuilding(Line); } else { Controller->SayWhy(Why); }
									}
									return FReply::Handled();
								})
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										Picture(ModelOf(Offer.No, Offer.Level), FVector2D(114.0, 69.0))
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(Offer.Label)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(bCan ? Cream : Muted).AutoWrapText(true) // (no order is cut mid-word)
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bCan ? BrightBrass : Alarm.CopyWithNewOpacity(0.9f))
										.Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d mo"), *FText::AsNumber(Offer.Cost).ToString(), Offer.Months)))
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(SBox).HeightOverride(46.0f)
										[
											SNew(STextBlock).Text(FText::FromString(Offer.Effect)).Font(Font(TEXT("Regular"), 8)).ColorAndOpacity(Muted).AutoWrapText(true)
										]
									]
								]
							]
						];
					}
					TownBarBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Row];
				}
			}

			// The town, and its plots.
			TownBarBox->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(30.0f).HeightOverride(20.0f)[SNew(SImage).Image(FlagBrush(Bar.Tag))]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Text(Bar.Title).Font(Font(TEXT("Regular"), 18)).ColorAndOpacity(Cream)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock).Text(Bar.Sub).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)
				]
			];
			TSharedRef<SHorizontalBox> Lots = SNew(SHorizontalBox);
			for (int32 Index = 0; Index < Bar.Plots.Num(); ++Index)
			{
				const FTownPlot& Plot = Bar.Plots[Index];
				const bool bFree = Plot.Line.IsEmpty(), bOn = Index == Bar.Picked;
				// (a first level still going up is pictured as the builders' scaffolding)
				const FString Model = bFree ? FString(TEXT("Civic:lot")) : ModelOf(Plot.No, Plot.Level);
				const FString Under = bFree ? (Bar.bMine ? FString(TEXT("Click to choose a building")) : FString(TEXT("Nothing built")))
					: Plot.bBuilding ? FString::Printf(TEXT("%s: due %s"), Plot.Level > 0 ? TEXT("Next level going up") : TEXT("Going up"), *Plot.Due)
					: Plot.Does;
				TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
				for (int32 Pip = 0; Pip < 3 && !bFree; ++Pip)
				{
					Pips->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(14.0f).HeightOverride(4.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Pip < Plot.Level ? BrightBrass : Faint.CopyWithNewOpacity(0.5f))]
					];
				}
				Lots->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(178.0f)
					[
						SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(6.0f, 6.0f)).IsFocusable(false)
						.ToolTipText(FText::FromString(bFree ? FString(TEXT("A free plot. Each plot takes one kind of building, which can then be raised two more levels.")) : FString::Printf(TEXT("%s: %s\n%s"), *Plot.Name, Plot.Level > 0 ? *Plot.Label : TEXT("going up"), *Under)))
						.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PickTownPlot(Index); } return FReply::Handled(); })
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								Picture(Model, FVector2D(160.0, 97.0))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(bFree ? Muted : Cream).AutoWrapText(true) // (no name is cut mid-word)
									.Text(FText::FromString(bFree ? FString(TEXT("Free plot")) : Plot.Level > 0 ? Plot.Label : Plot.Name))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pips]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
							[
								SNew(SBox).HeightOverride(24.0f)
								[
									SNew(STextBlock).Text(FText::FromString(Under)).Font(Font(TEXT("Regular"), 8)).ColorAndOpacity(Plot.bBuilding || (bFree && Bar.bMine) ? BrightBrass : Muted).AutoWrapText(true)
								]
							]
						]
					]
				];
			}
			TownBarBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Lots];
		}

		/**
		 * (Rules 84.2.6; ruling 211(c); the UI director's sheet 35.) The alarm that names the cure: first in the tray, above
		 * every other row, the month an enemy army stands on his home ground or a home town of his falls or is besieged. Its
		 * sentences are the rules' own, in order, the first bold; where there are several it ends with a small boxed "1 of
		 * 3". It is the one thing in the tray allowed to stand three lines tall. A press puts the nearest help in hand and
		 * centres the town.
		 */
		TSharedRef<SWidget> MakeAlarm()
		{
			// (the sheet's sizes are pixels of the 1600 by 1000 frame; a face is asked for in points, a third larger)
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			const FLinearColor Paper100(FColor::FromHex(TEXT("EDE6D6"))), Paper200(FColor::FromHex(TEXT("E2D7BC")));
			auto SentenceAt = [this](int32 Index) -> FString
			{
				const FHomeAlarm* Raised = Controller.IsValid() ? Controller->GetAlarm() : nullptr;
				return Raised && Raised->Lines.IsValidIndex(Index) ? Raised->Lines[Index] : FString();
			};
			return Chip(SNew(SBox).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f)).WidthOverride(320.0f)
				.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetAlarm() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).ButtonStyle(&AlarmStyle).ContentPadding(FMargin(0.0f)).IsFocusable(false)
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->PressAlarm(); } return FReply::Handled(); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(5.0f)[SNew(SBorder).BorderImage(&AlarmEdgeBrush)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(10.0f, 10.0f, 8.0f, 0.0f)
						[
							SNew(SLineIcon).Kind(10).Size(18.0f).Ink(Alarm).Weight(1.6f)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 8.0f, 12.0f, 9.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Serif(TEXT("Bold"), Px(15.0f))).ColorAndOpacity(Paper100).AutoWrapText(true).LineHeightPercentage(1.1f)
								.Text_Lambda([SentenceAt]() { return FText::FromString(SentenceAt(0)); })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Serif(TEXT("Regular"), Px(13.0f))).ColorAndOpacity(Paper200).AutoWrapText(true).LineHeightPercentage(1.15f)
								.Text_Lambda([this]()
								{
									const FHomeAlarm* Raised = Controller.IsValid() ? Controller->GetAlarm() : nullptr;
									TArray<FString> Rest;
									for (int32 Index = 1; Raised && Index < Raised->Lines.Num(); ++Index)
									{
										Rest.Add(Raised->Lines[Index]);
									}
									return FText::FromString(FString::Join(Rest, TEXT(" ")));
								})
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SBorder).BorderImage(&AlarmCountBrush).Padding(FMargin(5.0f, 1.0f))
								.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetAlarmCount() > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									SNew(STextBlock).Font(Typed(7.5f, true)).ColorAndOpacity(Paper200)
									.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(FString::Printf(TEXT("%d of %d"), Controller->GetAlarmPlace(), Controller->GetAlarmCount())) : FText::GetEmpty(); })
								]
							]
						]
					]
				]);
		}

		/**
		 * (Rules 84.2.6; ruling 211(a) as passed at 16:54 UTC.) His own guns left alone round an enemy town: his siege's own
		 * row, in the rules' sentence. Red, in the alarm's dress, while infantry or tanks of his can reach the town this month;
		 * plain where none can. A press centres the town; on a red row it also puts in hand the infantry or tanks that can
		 * come (rules 84.3), and the paper says where to right-click.
		 */
		TSharedRef<SWidget> MakeGunsAlone(int32 Index)
		{
			auto Px = [](float Pixels) { return Pixels * 0.75f; };
			const FLinearColor Paper100(FColor::FromHex(TEXT("EDE6D6")));
			auto Red = [this, Index]() { const FGunsAlone* Row = Controller.IsValid() ? Controller->GetGunsAlone(Index) : nullptr; return Row && Row->bRed; };
			return Chip(SNew(SBox).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f)).WidthOverride(320.0f)
				.Visibility_Lambda([this, Index]() { return Controller.IsValid() && Controller->GetGunsAlone(Index) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).ButtonStyle(&NoButton).ContentPadding(FMargin(0.0f)).IsFocusable(false)
					.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PressGunsAlone(Index); } return FReply::Handled(); })
					[
						// (the red row's block where the cure is in his hands; the tray's plain leather where it is not)
						SNew(SBorder).Padding(FMargin(0.0f))
						.BorderImage_Lambda([this, Red]() -> const FSlateBrush* { return Red() ? &AlarmGroundBrush : &PromptStyle.Normal; })
						[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(5.0f).Visibility_Lambda([Red]() { return Red() ? EVisibility::Visible : EVisibility::Hidden; })
							[
								SNew(SBorder).BorderImage(&AlarmEdgeBrush)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(10.0f, 7.0f, 12.0f, 8.0f)
						[
							SNew(STextBlock).Font(Serif(TEXT("Regular"), Px(13.0f))).AutoWrapText(true).LineHeightPercentage(1.15f)
							.ColorAndOpacity_Lambda([Red, Paper100]() { return FSlateColor(Red() ? Paper100 : Muted); })
							.Text_Lambda([this, Index]() { const FGunsAlone* Row = Controller.IsValid() ? Controller->GetGunsAlone(Index) : nullptr; return FText::FromString(Row ? Row->Line : FString()); })
						]
						]
					]
				]);
		}

		/** One of the chips under the END THE MONTH button that say what is still worth doing; a click opens the screen for it. */
		TSharedRef<SWidget> MakeToDo(int32 Index)
		{
			// (a plate of leather with a brass edge, its words typed; a little room is left in it for a wider face)
			return Chip(SNew(SBox).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
				.Visibility_Lambda([this, Index]() { return Controller.IsValid() && !Controller->GetToDoText(Index).IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).ButtonStyle(&PromptStyle).ContentPadding(FMargin(16.0f, 6.0f)).IsFocusable(false)
					.ToolTipText_Lambda([this, Index]() { return FText::FromString(Controller.IsValid() ? Controller->GetToDoTip(Index) : FString()); })
					.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PressToDo(Index); } return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Typed(8.5f, true)).ColorAndOpacity(BrightBrass.CopyWithNewOpacity(0.9f))
						.Text_Lambda([this, Index]() { return FText::FromString(Controller.IsValid() ? Controller->GetToDoText(Index) : FString()); })
					]
				]);
		}

		void AddGrid(const TArray<TSharedRef<SWidget>>& Cells, int32 Columns)
		{
			for (int32 First = 0; First < Cells.Num(); First += Columns)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 Column = 0; Column < Columns; ++Column)
				{
					const int32 Index = First + Column;
					Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Column == Columns - 1 ? 0.0f : 8.0f, 0.0f)
					[
						Cells.IsValidIndex(Index) ? Cells[Index] : StaticCastSharedRef<SWidget>(SNew(SSpacer))
					];
				}
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[Row];
				ListRows.Emplace(TWeakPtr<SWidget>(Row), FMath::Min(Columns, Cells.Num() - First)); // (for the plate's count)
			}
		}

		/**
		 * (The UI director, on build 1596.) Whether the opening card's list runs on below what shows, how many cards are
		 * not wholly in sight below, and how high the list stands: looked at once a frame, from where the list's rows
		 * stand inside it now. Rows scrolled out of sight are not painted, so their places are worked out afresh here
		 * rather than read from their last painting. (The UI director's review of build 1628, items 4 and 5.) The list
		 * shows no half row: where it runs on, it stands as high as the lines that fit whole above the plate's band, and
		 * the plate sits in that band under the last of them; what room is over stands under the plate. A line is each
		 * of the list's own parts: a heading, a row of cards, a line of the controls. (09:18) A heading travels with the
		 * line under it, as one: it never ends the list above the plate, and the list never rests on that line with the
		 * heading scrolled away above it.
		 */
		void LookAtList()
		{
			bListRunsOn = false;
			ListBelow = 0;
			if (!StartsScroll.IsValid() || !Starts.IsValid() || !ListArea.IsValid() || !Controller.IsValid() || !Controller->GetOpening().bVisible)
			{
				return;
			}
			const FGeometry& View = StartsScroll->GetTickSpaceGeometry();
			const FGeometry& Along = Starts->GetTickSpaceGeometry();
			const double Area = ListArea->GetTickSpaceGeometry().GetLocalSize().Y;
			// (The checker's note C: the first frame the card shows, the list and its scroll have not been laid out yet,
			// and a scroll with no size takes its whole list for what runs on below. Until they have a width, nothing is
			// measured and no plate is shown. A list squeezed to no height still has its width, and is measured.)
			if (View.GetLocalSize().X <= 0.0f || Along.GetLocalSize().X <= 0.0f || ListArea->GetTickSpaceGeometry().GetLocalSize().X <= 0.0f
				|| (OpeningCardBox.IsValid() && OpeningCardBox->GetTickSpaceGeometry().GetLocalSize().X <= 0.0f))
			{
				return;
			}
			FArrangedChildren Arranged(EVisibility::Visible);
			Starts->ArrangeChildren(Along, Arranged);
			// Each line's top and foot inside the list, wherever the list is scrolled to.
			ListTops.Reset();
			ListFeet.Reset();
			for (int32 Index = 0; Index < Arranged.Num(); ++Index)
			{
				const FGeometry& Each = Arranged[Index].Geometry;
				ListTops.Add(static_cast<float>(Along.AbsoluteToLocal(Each.GetAbsolutePosition()).Y));
				ListFeet.Add(static_cast<float>(Along.AbsoluteToLocal(Each.LocalToAbsolute(FVector2D(0.0, Each.GetLocalSize().Y))).Y));
			}
			// (The UI director, 09:18.) Which lines are headings: each travels with the line under it, as one. And how
			// many cards each line holds, for the plate's count.
			ListHeadAt.Init(false, Arranged.Num());
			ListCountAt.Init(0, Arranged.Num());
			for (int32 Index = 0; Index < Arranged.Num(); ++Index)
			{
				for (const TWeakPtr<SWidget>& Head : ListHeads)
				{
					if (Head.Pin().Get() == &Arranged[Index].Widget.Get())
					{
						ListHeadAt[Index] = true;
						break;
					}
				}
				for (const TPair<TWeakPtr<SWidget>, int32>& Kept : ListRows)
				{
					if (Kept.Key.Pin().Get() == &Arranged[Index].Widget.Get())
					{
						ListCountAt[Index] = Kept.Value;
						break;
					}
				}
			}
			ListAreaHigh = Area;
			// (The UI director, 03:53 and 07:10.) What the card needs: all it holds outside the list, and the list down to
			// the foot of the first row of the smaller nations, on the grand campaign's page (the great powers above it),
			// or of its first row on another page, with the plate's band under it. The rest of the card does not change
			// with the card's height, or with the nation picked, so this holds still as the card grows.
			// (08:57: margins before a row; capped at 09:04.) On the grand campaign's page the margins give to bring in a
			// second whole row of the smaller nations, never a third: where the 48s show fewer and standing up to 16
			// from the screen's top and foot shows two, the card stands as tall as the two rows and their plate's band, and
			// the margins give only that much (at Brian's window, the 19 that show Brazil's second row). Where the 48s
			// show two already, or 16 would not show two, it keeps its 48s; a larger frame may show a third row within
			// them, but never by trimming the margins.
			float Need = 0.0f;
			const int32 Keep = Controller->GetOpening().Tab == 0 && ListRows.Num() > 1 ? 1 : 0;
			const double ScreenHigh = ScreenRoot.IsValid() ? ScreenRoot->GetTickSpaceGeometry().GetLocalSize().Y : 0.0;
			if (OpeningCardBox.IsValid() && ListRows.IsValidIndex(Keep))
			{
				const double Outside = OpeningCardBox->GetTickSpaceGeometry().GetLocalSize().Y - Area;
				const double ListEnd = ListFeet.Num() > 0 ? static_cast<double>(ListFeet.Last()) : 0.0;
				int32 Within48 = -1;
				int32 Within16 = -1;
				float NeedTwo = 0.0f; // (the card's need with two whole rows)
				for (int32 CardRow = Keep; CardRow < ListRows.Num(); ++CardRow)
				{
					const TSharedPtr<SWidget> RowWidget = ListRows[CardRow].Key.Pin();
					int32 RowAt = INDEX_NONE;
					for (int32 Nth = 0; RowWidget.IsValid() && Nth < Arranged.Num(); ++Nth)
					{
						if (&Arranged[Nth].Widget.Get() == RowWidget.Get())
						{
							RowAt = Nth;
							break;
						}
					}
					if (RowAt == INDEX_NONE)
					{
						continue;
					}
					// (the last line of the list needs no plate under it)
					const double RowFoot = static_cast<double>(ListFeet[RowAt]);
					const float RowNeed = static_cast<float>(FMath::CeilToDouble(Outside + RowFoot + (RowFoot >= ListEnd - 0.5 ? 0.0 : static_cast<double>(PlateBand))));
					if (CardRow == Keep)
					{
						Need = RowNeed;
					}
					if (Keep == 0 || ScreenHigh <= 0.0)
					{
						break;
					}
					if (RowNeed <= ScreenHigh - 96.0 + 0.5)
					{
						Within48 = CardRow;
					}
					if (RowNeed <= ScreenHigh - 32.0 + 0.5)
					{
						Within16 = CardRow;
					}
					if (CardRow == Keep + 1)
					{
						NeedTwo = RowNeed;
						break; // (no row past the second is ever bought with the margins)
					}
				}
				if (Keep > 0 && Within48 < Keep + 1 && Within16 >= Keep + 1)
				{
					Need = NeedTwo;
				}
			}
			// (What the card needs now turns on the screen's height too: a window drawn to a new size measures afresh.)
			if (!FMath::IsNearlyEqual(ScreenHigh, OpeningHigh, 0.5))
			{
				OpeningHigh = ScreenHigh;
				OpeningSettle = FMath::Max(OpeningSettle, 3);
			}
			// (The checker's note E.) Drawn smaller, the card's text is measured at the scale it is drawn at, a line a whole
			// pixel high, so what it needs moves a little with the scale, and the scale with what it needs: left free, it
			// could tremble between two scales for good. For its first frames after it is laid out it takes what it
			// measures; after that it only grows, and so comes to rest, a few units slack at most.
			// (Release 1, 22 November 2026.) Those first frames count only once the list has its size: a list just laid
			// out that still measures as nothing would drop the card to its least height for a frame, and stand it up
			// again on the next, a jump on every click.
			const bool bListMeasured = ListFeet.Num() > 0 && ListFeet.Last() > 0.5f;
			if ((OpeningSettle > 0 && bListMeasured) || Need > OpeningNeed)
			{
				OpeningNeed = Need;
			}
			if (OpeningSettle > 0 && bListMeasured)
			{
				--OpeningSettle;
			}
			// Where the list's view begins inside the list: how far it is asked to be scrolled, not where it was last drawn
			// (the checker's fault 3: drawn, a step near the foot can fall short, the box still sized for the old top).
			double Top = FMath::Max(static_cast<double>(StartsScroll->GetScrollOffset()), 0.0);
			const double End = ListFeet.Num() > 0 ? static_cast<double>(ListFeet.Last()) : 0.0;
			// Where the list rests: with a line's top at the top of its view, the list's very top for the first line, and
			// never with a heading's line alone above it (09:18). (The checker's fault 1.) The list's last stop: the first
			// from which the rest fits whole in the list's area. The list never rests below it, so its foot is never
			// pulled up into a line. (Both kept from the last frame while a list just laid out still measures as nothing.)
			if (End > 0.5)
			{
				ListStops.Reset();
				for (int32 Index = 0; Index < ListTops.Num(); ++Index)
				{
					if (Index == 0 || !ListHeadAt[Index - 1])
					{
						ListStops.Add(Index == 0 ? 0.0f : ListTops[Index]);
					}
				}
				ListLastStop = 0.0f;
				for (const float Stop : ListStops)
				{
					ListLastStop = Stop;
					if (End - static_cast<double>(Stop) <= Area + 0.5)
					{
						break;
					}
				}
			}
			// (A belt: a view left off a line's top, by a page changed or a window drawn to a new size, is set on the line
			// at or above it, never past the last stop. Not while the bar is held: the bar's own snap sees to it. Not in
			// the frames after a laying out either, while new rows may not yet have their sizes: a list measured as
			// nothing would send the view back to the top on every pick.)
			const bool bHeld = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
			if (!bHeld && OpeningSettle == 0 && End > 0.5 && ListStops.Num() > 0 && StartsScroll.IsValid())
			{
				float On = 0.0f;
				for (int32 Index = 0; Index < ListStops.Num() && ListStops[Index] <= static_cast<float>(Top) + 0.5f; ++Index)
				{
					On = ListStops[Index];
				}
				// ("-TheaterOptionsEnd" rests the OPTIONS page on its last stop: for a picture of the credits at its end,
				// which the replay tool cannot scroll to)
				static const bool bFromEnd = FParse::Param(FCommandLine::Get(), TEXT("TheaterOptionsEnd"));
				if (bFromEnd && Controller.IsValid() && Controller->GetOpening().Tab == 2)
				{
					On = ListLastStop;
				}
				// ("-TheaterOpeningScroll" rests the scenarios on their last stop the same way: for the Mac's picture of the
				// practice and Battle rules cards at the list's foot, under the 38 MORE it cannot press; a scroll to the end
				// when the list is laid out was undone by the rows still taking their sizes, 22 November 22:25Z)
				static const bool bScenariosFromEnd = FParse::Param(FCommandLine::Get(), TEXT("TheaterOpeningScroll"));
				if (bScenariosFromEnd && Controller.IsValid() && Controller->GetOpening().Tab != 2
					&& !(Controller->GetOpening().Tab == 0 && Controller->GetOpening().Years.Num() > 0))
				{
					On = ListLastStop;
				}
				On = FMath::Min(On, ListLastStop);
				if (!FMath::IsNearlyEqual(static_cast<double>(On), Top, 0.5))
				{
					StartsScroll->SetScrollOffset(On);
					ListRest = On;
					Top = On;
				}
			}
			SizeList(Top);
		}

		/**
		 * How high the list stands with its view's top at Top, whether it runs on, and how many cards are not wholly in
		 * sight below, from the lines LookAtList last measured. Called once a frame, and straight after the list is
		 * scrolled, so that the box has its new height in the same frame (the checker's fault 3: events come before the
		 * frame's measuring). Where what is left below fits in the list's area, it shows all of it, and there is no plate.
		 */
		void SizeList(double Top)
		{
			bListRunsOn = false;
			ListBelow = 0;
			const double Area = ListAreaHigh;
			const double End = ListFeet.Num() > 0 ? static_cast<double>(ListFeet.Last()) : 0.0;
			ListViewTop = static_cast<float>(Top);
			if (End - Top <= Area + 0.5)
			{
				// (no taller than what is left: a view taller than its list would have the box draw it from higher up)
				ListViewHigh = static_cast<float>(FMath::Min(Area, FMath::Max(End - Top, 1.0)));
				return;
			}
			bListRunsOn = true;
			// As high as the lines that fit whole above the plate's band; none at all, only on the smallest frame.
			const double Room = FMath::Max(Area - static_cast<double>(PlateBand), 0.0);
			double Fit = 0.0;
			for (int32 Index = 0; Index < ListFeet.Num(); ++Index)
			{
				// (09:18: a heading never ends the list; the room it would take stays under the plate)
				const double Foot = static_cast<double>(ListFeet[Index]) - Top;
				if (Foot > 0.5 && Foot <= Room + 0.5 && !(ListHeadAt.IsValidIndex(Index) && ListHeadAt[Index]))
				{
					Fit = FMath::Max(Fit, Foot);
				}
			}
			ListViewHigh = static_cast<float>(FMath::Min(FMath::CeilToDouble(Fit), Room));
			// The cards not wholly in sight below, for the plate's count (nations, never headings).
			for (int32 Index = 0; Index < ListFeet.Num() && Index < ListCountAt.Num(); ++Index)
			{
				if (ListCountAt[Index] > 0 && static_cast<double>(ListFeet[Index]) - Top > static_cast<double>(ListViewHigh) + 0.5)
				{
					ListBelow += ListCountAt[Index];
				}
			}
		}

		/**
		 * The plate's click: the list down until the first line not wholly in sight shows whole at the foot, its top on
		 * a line's top (by one row of cards, where the rows below are of one height), never past its end.
		 */
		void ScrollListOn()
		{
			if (!StartsScroll.IsValid() || !bListRunsOn)
			{
				return;
			}
			float To = -1.0f;
			float Holds = -1.0f; // (the stop that holds the first line not wholly in sight)
			for (int32 Index = 0; Index < ListFeet.Num(); ++Index)
			{
				if (ListFeet[Index] - ListViewTop > ListViewHigh + 0.5f)
				{
					// (a heading comes into sight with the line under it)
					const int32 Whole = ListHeadAt.IsValidIndex(Index) && ListHeadAt[Index] && ListFeet.IsValidIndex(Index + 1) ? Index + 1 : Index;
					To = ListViewTop + (ListFeet[Whole] - (ListViewTop + ListViewHigh));
					for (const float Stop : ListStops)
					{
						if (Stop <= ListTops[Index] + 0.5f)
						{
							Holds = Stop;
						}
					}
					break;
				}
			}
			if (To < 0.0f)
			{
				return;
			}
			for (const float Line : ListStops)
			{
				if (Line > ListViewTop + 0.5f && Line >= To - 0.5f)
				{
					To = Line;
					break;
				}
			}
			// (the checker's note: where a heading and its row stand taller than the view, the step goes no farther than
			// the heading's own stop, so the heading is never passed by)
			if (Holds > ListViewTop + 0.5f)
			{
				To = FMath::Min(To, Holds);
			}
			To = FMath::Min(To, ListLastStop);
			StartsScroll->SetScrollOffset(To);
			ListRest = To;
			ListOwed = 0.0f;
			ListWheelAt = 0.0;
			SizeList(To);
		}


		/**
		 * (Items 4 and 5.) A scroll by the wheel or the bar comes to rest with a line's top at the list's top, so that the
		 * foot of the list stays on a whole line and the plate's band does not move under the pointer as the rows go by.
		 * (The checker's fault 2.) A trackpad sends one swipe as a run of small wheel events, so the wheel does not move
		 * a line an event: what it moves is owed, and the list stays on its line until the debt passes a third of the
		 * next line's height (at most 24), then steps one line that way; the first event after a pause steps a line at
		 * once, so that one notch of a wheel, however slow, moves one line. A debt left a quarter of a second is
		 * forgotten. The bar, held and dragged, rests on the line nearest the thumb.
		 */
		void SnapList(float Offset)
		{
			if (!StartsScroll.IsValid() || ListStops.Num() == 0)
			{
				return;
			}
			const float Last = ListLastStop;
			float To = 0.0f;
			if (FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
			{
				float Nearest = TNumericLimits<float>::Max();
				for (const float Stop : ListStops)
				{
					if (Stop <= Last + 0.5f && FMath::Abs(Stop - Offset) < Nearest)
					{
						Nearest = FMath::Abs(Stop - Offset);
						To = Stop;
					}
				}
				ListOwed = 0.0f;
				ListWheelAt = 0.0;
			}
			else
			{
				const double Now = FPlatformTime::Seconds();
				const bool bFresh = Now - ListWheelAt > 0.25;
				const float Rest = bFresh ? ListViewTop : ListRest;
				ListOwed = (bFresh ? 0.0f : ListOwed) + (Offset - Rest);
				ListWheelAt = Now;
				To = Rest;
				// (The checker's note: a Mac wheel turned slowly sends a notch as a small delta, which alone would never
				// pass the debt's mark. The first event after a pause steps a line at once, whatever its size; those that
				// follow it within the quarter second owe as above.)
				const float Mark = bFresh && FMath::Abs(ListOwed) > 0.5f ? 0.0f : 24.0f;
				if (ListOwed > 0.0f)
				{
					// (the next line down, not past the last stop)
					float Next = -1.0f;
					for (const float Stop : ListStops)
					{
						if (Stop > Rest + 0.5f)
						{
							Next = Stop;
							break;
						}
					}
					if (Next < 0.0f || Rest >= Last - 0.5f)
					{
						ListOwed = 0.0f; // (at the last stop: nothing more to owe)
					}
					else if (ListOwed >= FMath::Min((Next - Rest) / 3.0f, Mark))
					{
						To = FMath::Min(Next, Last);
						ListOwed = 0.0f;
					}
				}
				else if (ListOwed < 0.0f)
				{
					// (the line above)
					float Above = -1.0f;
					for (int32 Index = ListStops.Num() - 1; Index >= 0; --Index)
					{
						if (ListStops[Index] < Rest - 0.5f)
						{
							Above = ListStops[Index];
							break;
						}
					}
					if (Above < 0.0f)
					{
						ListOwed = 0.0f; // (at the top)
					}
					else if (-ListOwed >= FMath::Min((Rest - Above) / 3.0f, Mark))
					{
						To = Above;
						ListOwed = 0.0f;
					}
				}
			}
			ListRest = To;
			// (every event, however small: a move left in the box would be counted again by the next one)
			StartsScroll->SetScrollOffset(To);
			SizeList(To);
		}

		bool StoresShown() const
		{
			return Controller.IsValid() && Controller->GetStock(true) != nullptr;
		}

		/** Which: 0 oil, 1 supplies, 2 the reserve of men. */
		const FViewStock* StockOf(int32 Which) const
		{
			return !Controller.IsValid() ? nullptr : Which == 2 ? Controller->GetMen() : Controller->GetStock(Which == 0);
		}

		/** A small gauge on the date plate: a stock's name, how full its depots are, and how long it lasts. */
		TSharedRef<SWidget> MakeGauge(const TCHAR* Label, int32 Which)
		{
			auto Low = [this, Which]()
			{
				const FViewStock* Stock = StockOf(Which);
				return Stock && (Stock->bShort || (Stock->Months >= 0 && Stock->Months <= 6));
			};
			auto Share = [this, Which]()
			{
				const FViewStock* Stock = StockOf(Which);
				return Stock && Stock->Cap > 0.0f ? FMath::Clamp(Stock->Stock / Stock->Cap, 0.0f, 1.0f) : 0.0f;
			};
			return SNew(SHorizontalBox)
				.ToolTipText_Lambda([this, Which]() { const FViewStock* Stock = StockOf(Which); return Stock ? FText::FromString(Stock->Tip) : FText::GetEmpty(); })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Typed(9, true, 120))
					.ColorAndOpacity_Lambda([Low]() { return FSlateColor(Low() ? InkRed : InkSoft); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(40.0f).HeightOverride(6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Share]() { return FMath::Max(Share(), 0.0001f); }))
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor_Lambda([Low]() { return FSlateColor(Low() ? InkRed : DeskInk); })
						]
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Share]() { return FMath::Max(1.0f - Share(), 0.0001f); }))
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(DeskInk.CopyWithNewOpacity(0.18f))
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Typed(10))
					.ColorAndOpacity_Lambda([Low]() { return FSlateColor(Low() ? InkRed : InkSoft); })
					.Text_Lambda([this, Which]()
					{
						const FViewStock* Stock = StockOf(Which);
						if (!Stock)
						{
							return FText::GetEmpty();
						}
						if (!Stock->Word.IsEmpty())
						{
							return FText::FromString(Stock->Word.ToLower()); // (the reserve of men: how it stands, in a word)
						}
						if (Stock->bShort || Stock->Months == 0)
						{
							return FText::FromString(TEXT("empty"));
						}
						return FText::FromString(Stock->Months < 0 ? FString(Stock->Net > 0.05f ? TEXT("growing") : TEXT("steady")) : FString::Printf(TEXT("%d months"), Stock->Months));
					})
				];
		}

		/** The names of a country's abilities in a line: each ability's line begins with its name and a full stop. */
		static FString AbilityNames(const TArray<FString>& Lines)
		{
			TArray<FString> Names;
			for (const FString& Line : Lines)
			{
				FString Name = Line, Rest;
				Line.Split(TEXT(". "), &Name, &Rest);
				Names.Add(Name.Replace(TEXT("Special formation, from the arsenal: "), TEXT("special formation: ")));
			}
			return TEXT("Its own ways:  ") + FString::Join(Names, TEXT("  \u00B7  "));
		}

		/**
		 * (Release 1.5, the UI director, 24 November 2026: Brian, "when you click on the country name on the home screen it
		 * flickers".) A line of the opening card that folds to its width. Slate folds a line that wraps by itself
		 * (AutoWrapText) only once it has been drawn: a line just laid out stands on one line for its first frame and folds
		 * on the next, and all under it jumps. Every click on the card lays it out afresh, so every click jumped: the list
		 * under the year's note, the foot under its three settings, and with them the card's height. Each such line now
		 * folds, on its first frame, at the width the line in its place had when last drawn, and after that at its own
		 * width, as before. Until a line in its place has been drawn it folds as it always did.
		 */
		TSharedRef<STextBlock> Folding(const FString& Place, const TSharedRef<STextBlock>& Line)
		{
			const TWeakPtr<STextBlock> Self = Line;
			Line->SetWrapTextAt(TAttribute<float>::CreateLambda([this, Self, Place]()
			{
				const TSharedPtr<STextBlock> Shown = Self.Pin();
				const float Had = Shown.IsValid() ? static_cast<float>(Shown->GetTickSpaceGeometry().GetLocalSize().X) : 0.0f;
				float& Kept = OpeningFolds.FindOrAdd(Place);
				if (Had > 1.0f)
				{
					Kept = Had;
				}
				return Kept; // (with AutoWrapText Slate folds at the lesser of this and the width it was last drawn at)
			}));
			return Line;
		}

		/**
		 * One country to lead in the grand campaign. (The UI director, 18 November 2026, on build 1596.) A compact card:
		 * the flag and the name only, 93 high in the frame's own measure, seven great powers to
		 * a row and the others eight; the name wraps onto a second line rather than run past the card. Its status line and
		 * its sentence are in the summary at the panel's foot once it is picked, and on its hover before.
		 */
		TSharedRef<SWidget> MakeNation(const FOpeningNation& Nation, int32 Index, bool bChosen)
		{
			const FString Status = Nation.Standing.ToString(), Sentence = Nation.Blurb.ToString();
			const FString Hover = Sentence.IsEmpty() ? Status : Status.IsEmpty() ? Sentence : Status + TEXT("\n") + Sentence;
			return SNew(SBox).HeightOverride(93.0f)
				[
					SNew(SButton).ButtonStyle(bChosen ? &ChosenCard : &PlainButton).IsFocusable(false)
					.ContentPadding(FMargin(6.0f, 7.0f, 6.0f, 7.0f)).HAlign(HAlign_Fill).VAlign(VAlign_Center)
					.ToolTipText(FText::FromString(Hover))
					.OnClicked_Lambda([this, Index]()
					{
						if (Controller.IsValid())
						{
							Controller->PickGrandNation(Index);
						}
						return FReply::Handled();
					})
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							MakeFlag(Nation.Tag, 60.0f) // (40 tall: 03:53)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							Folding(Nation.bMajor ? TEXT("Nation.Great") : TEXT("Nation.Other"), SNew(STextBlock).Text(Nation.Name).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream)
							.AutoWrapText(true).Justification(ETextJustify::Center))
						]
					]
				];
		}

		/** The opening screen's two pages, and what stands on the one showing. */
		void LayOutOpening(const FOpening& Opening)
		{
			auto Heading = [](const FString& Text)
			{
				return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(TEXT("Bold"), 11, 200)).ColorAndOpacity(Brass);
			};
			// (The UI director, 09:18.) A heading of the list travels with the line under it, as one: it never ends the
			// list above the plate (see LookAtList).
			auto ListHead = [this, &Heading](const FString& Text) -> TSharedRef<SWidget>
			{
				const TSharedRef<SWidget> Made = Heading(Text);
				ListHeads.Add(TWeakPtr<SWidget>(Made));
				return Made;
			};

			// The tabs.
			OpeningTabs->ClearChildren();
			{
				const TCHAR* Names[] = { TEXT("THE GRAND CAMPAIGN"), TEXT("HISTORICAL SCENARIOS"), TEXT("OPTIONS") };
				for (int32 Tab = Opening.Years.Num() > 0 ? 0 : 1; Tab < 3; ++Tab)
				{
					const bool bOn = Opening.Tab == Tab;
					OpeningTabs->AddSlot().AutoWidth().Padding(5.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(26.0f, 8.0f)).IsFocusable(false)
						.OnClicked_Lambda([this, Tab]() { if (Controller.IsValid()) { Controller->ShowOpeningTab(Tab); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(Names[Tab])).Font(Font(TEXT("Bold"), 12, 200)).ColorAndOpacity(bOn ? BrightBrass : Muted)
						]
					];
				}
			}

			Starts->ClearChildren();
			ListRows.Reset();
			ListHeads.Reset();
			OpeningSettle = 3; // (see LookAtList)
			// (The checker's note: a new page opens at its top, not at the last page's place.)
			if (Opening.Tab != ListTab && StartsScroll.IsValid())
			{
				ListTab = Opening.Tab;
				StartsScroll->SetScrollOffset(0.0f);
				ListRest = 0.0f;
				ListOwed = 0.0f;
				ListWheelAt = 0.0;
			}
			OpeningFoot->SetContent(SNullWidget::NullWidget);
			OpeningFoot->SetVisibility(EVisibility::Collapsed);

			if (Opening.Tab == 3)
			{
				// ---- (Release 1.5.1; Brian, 24 November 2026: "yes add save button so i can save multiple games".) The saved
				// wars, the newest first: each its line as CONTINUE says it, when it was saved, LOAD and DELETE. DELETE asks on
				// the row first, and KEEP IT is its lit answer (ruling 134(f)).
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					ListHead(TEXT("SAVED WARS"))
				];
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					Folding(TEXT("Saves.Intro"), SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(Opening.bCanReturn
						? TEXT("Each SAVE keeps a copy of the war as it stands, as many as you like. The game still keeps the war you are playing by itself each month, for CONTINUE.")
						: TEXT("The wars you have kept with SAVE, the newest first. CONTINUE is the war the game keeps by itself each month."))))
				];
				if (!Opening.SavesNote.IsEmpty())
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
					[
						Folding(TEXT("Saves.Note"), SNew(STextBlock).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Opening.JustSaved.IsEmpty() ? Cream : BrightBrass).AutoWrapText(true)
						.Text(Opening.SavesNote))
					];
				}
				if (Opening.SavedWars.Num() == 0)
				{
					Starts->AddSlot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
						.Text(FText::FromString(TEXT("No saved wars yet. During a war, open this menu with Esc and press SAVE.")))
					];
					return;
				}
				static const TCHAR* const MonthNames[] = { TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"), TEXT("June"),
					TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December") };
				TArray<TSharedRef<SWidget>> Cells;
				for (const FSavedWar& War : Opening.SavedWars)
				{
					const FString Id = War.Id;
					const bool bJust = Id == Opening.JustSaved, bAsk = Id == Opening.AskDelete;
					const FString When = War.When.GetYear() > 1
						? FString::Printf(TEXT("Saved %d %s %d, %02d:%02d"), War.When.GetDay(), MonthNames[FMath::Clamp(War.When.GetMonth(), 1, 12) - 1], War.When.GetYear(), War.When.GetHour(), War.When.GetMinute())
						: FString(TEXT("Saved"));
					TSharedRef<SHorizontalBox> Answers = SNew(SHorizontalBox);
					if (bAsk)
					{
						Answers->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Throw this saved war away for good?"))).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream)
						];
						Answers->AddSlot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(16.0f, 7.0f)).IsFocusable(false)
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerDeleteWar(false); } return FReply::Handled(); })
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("KEEP IT"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(BrightBrass)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), BrightBrass)]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ESC"), BrightBrass)]
							]
						];
						Answers->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 7.0f)).IsFocusable(false)
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AnswerDeleteWar(true); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("DELETE"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Muted)
							]
						];
					}
					else
					{
						Answers->AddSlot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(112.0f)
							[
								SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(6.0f, 7.0f)).IsFocusable(false).HAlign(HAlign_Center)
								.ToolTipText(FText::FromString(TEXT("Lay this war on the table again, as it stood when it was saved.")))
								.OnClicked_Lambda([this, Id]() { if (Controller.IsValid()) { Controller->AskToLoadWar(Id); } return FReply::Handled(); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("LOAD"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(BrightBrass)
								]
							]
						];
						Answers->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(112.0f)
							[
								SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(6.0f, 7.0f)).IsFocusable(false).HAlign(HAlign_Center)
								.OnClicked_Lambda([this, Id]() { if (Controller.IsValid()) { Controller->AskToDeleteWar(Id); } return FReply::Handled(); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("DELETE"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Muted)
								]
							]
						];
					}
					Cells.Add(
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(14.0f, 9.0f, 10.0f, 10.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									Folding(TEXT("Saves.Line"), SNew(STextBlock).Text(FText::FromString(War.Line)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream).AutoWrapText(true))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(bJust ? BrightBrass : Muted)
									.Text(FText::FromString(bJust ? When + TEXT("  \u00B7  JUST SAVED") : When))
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Answers]
						]);
				}
				AddGrid(Cells, 1);
				return;
			}

			if (Opening.Tab == 2)
			{
				// ---- The options: the game's standing choices, each with what it does and a button that changes it.
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					Folding(TEXT("Options.Intro"), SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(TEXT("How the game looks, sounds and handles. What you choose here is remembered the next time you play."))))
				];
				const TArray<FGameOption> Options = Controller.IsValid() ? Controller->GetOptions() : TArray<FGameOption>();
				TArray<TSharedRef<SWidget>> Cells;
				for (const FGameOption& Option : Options)
				{
					const FString Id = Option.Id;
					Cells.Add(
						SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(14.0f, 9.0f, 10.0f, 10.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									Folding(TEXT("Options.Name"), SNew(STextBlock).Text(Option.Name).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream).AutoWrapText(true))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									Folding(TEXT("Options.Blurb"), SNew(STextBlock).Text(Option.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f))
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(138.0f)
								[
									SNew(SButton).ButtonStyle(Option.bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(6.0f, 8.0f)).IsFocusable(false).HAlign(HAlign_Center)
									.OnClicked_Lambda([this, Id]() { if (Controller.IsValid()) { Controller->PressOption(Id); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(Option.State).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Option.bOn ? BrightBrass : Muted)
									]
								]
							]
						]);
				}
				AddGrid(Cells, 2);
				Starts->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 6.0f)
				[
					ListHead(TEXT("THE CONTROLS"))
				];
				const TCHAR* Controls[][2] =
				{
					{ TEXT("Move the view"), TEXT("drag the table, or W A S D or the arrow keys; double-click a place to centre on it") },
					{ TEXT("Zoom, turn and tilt"), TEXT("scroll, or + and -; drag with the right button, or Q and E") },
					{ TEXT("Views"), TEXT("Shift with 6, 7, 8 or 9 keeps the view you are looking at; the key alone comes back to it") },
					{ TEXT("The bare ground"), TEXT("hold B to lift every piece off the table; hold P to ink every province line") },
					{ TEXT("Armies and fleets"), TEXT("click a flag or a fleet to take it in hand; right-click where it should go; Esc puts it down; Tab goes to the next army yet to move") },
					{ TEXT("The month"), TEXT("Enter ends it; Space skips what is being shown") },
					{ TEXT("Screens"), TEXT("1 to 5 open the top bar's; L forces, R report, O goals, C your capital, H how to play") },
					{ TEXT("Map views"), TEXT("Y steps through them: flat colours, wealth, friend and foe, and the admiral's chart of the seas you have searched") },
				};
				for (const auto& Line : Controls)
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(170.0f)
							[
								SNew(STextBlock).Text(FText::FromString(Line[0])).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Cream)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							Folding(TEXT("Options.Control"), SNew(STextBlock).Text(FText::FromString(Line[1])).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true))
						]
					];
				}
				// ---- The credits (the UI director, 18 November 2026): the last section of this page, so that MENU reaches
				// them too. A line for each creator, A to Z: the name, each recording's title in quotes, and its licence
				// (each title its own where one creator's differ). Read from Sounds/Real/credits.json in the game's
				// folder, the one list CREDITS.md is made from; with no list there is no section.
				{
					struct FCredited
					{
						FString Creator;
						TArray<TPair<FString, FString>> Titles; // a recording's title, and its licence
					};
					static TArray<FCredited> Credits;
					static bool bRead = false;
					if (!bRead)
					{
						bRead = true;
						FString Text;
						TSharedPtr<FJsonObject> Root;
						const TArray<TSharedPtr<FJsonValue>>* Sounds = nullptr;
						if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Sounds/Real/credits.json")))
							&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid() && Root->TryGetArrayField(TEXT("sounds"), Sounds))
						{
							for (const TSharedPtr<FJsonValue>& Sound : *Sounds)
							{
								const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
								if (!Sound->AsObject().IsValid() || !Sound->AsObject()->TryGetArrayField(TEXT("sources"), Sources))
								{
									continue;
								}
								for (const TSharedPtr<FJsonValue>& Source : *Sources)
								{
									FString Creator, Title, Licence;
									if (!Source->AsObject().IsValid() || !Source->AsObject()->TryGetStringField(TEXT("creator"), Creator) || Creator.IsEmpty())
									{
										continue; // (public domain, or given to it: no credit is owed)
									}
									Source->AsObject()->TryGetStringField(TEXT("title"), Title);
									Source->AsObject()->TryGetStringField(TEXT("licence"), Licence);
									FCredited* Theirs = Credits.FindByPredicate([&Creator](const FCredited& Each) { return Each.Creator == Creator; });
									if (!Theirs)
									{
										Theirs = &Credits.AddDefaulted_GetRef();
										Theirs->Creator = Creator;
									}
									Theirs->Titles.AddUnique(TPair<FString, FString>(Title, Licence));
								}
							}
							Credits.Sort([](const FCredited& A, const FCredited& B) { return A.Creator.Compare(B.Creator, ESearchCase::IgnoreCase) < 0; });
						}
					}
					if (Credits.Num() > 0)
					{
						Starts->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 6.0f)
						[
							Heading(TEXT("CREDITS"))
						];
						Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
						[
							Folding(TEXT("Options.Credits"), SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
							.Text(FText::FromString(TEXT("Sounds recorded by others and shared on freesound.org and Wikimedia Commons, used under Creative Commons licences and cut and mixed for this game. Every licence and link is in CREDITS.md, in the game's folder."))))
						];
						const FLinearColor Body(FColor::FromHex(TEXT("B4AB98"))); // (4.5 to 1 or better on the card)
						for (const FCredited& Each : Credits)
						{
							bool bOneLicence = true;
							for (const TPair<FString, FString>& Title : Each.Titles)
							{
								bOneLicence = bOneLicence && Title.Value == Each.Titles[0].Value;
							}
							FString Said;
							for (const TPair<FString, FString>& Title : Each.Titles)
							{
								Said += (Said.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("\u201C%s\u201D"), *Title.Key) + (bOneLicence ? FString() : TEXT("  \u00B7  ") + Title.Value);
							}
							if (bOneLicence)
							{
								Said += TEXT("  \u00B7  ") + Each.Titles[0].Value;
							}
							Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 14.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(Each.Creator)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom)
								[
									Folding(TEXT("Options.Credit.") + Each.Creator, SNew(STextBlock).Text(FText::FromString(Said)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Body).AutoWrapText(true))
								]
							];
						}
					}
				}
				return;
			}

			if (Opening.Tab == 0 && Opening.Years.Num() > 0)
			{
				// ---- The grand campaign: a day to take over on, and a country.
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					Folding(TEXT("Grand.Intro"), SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(TEXT("Pick a country and a year. From the day you take over, the war is yours to shape.")))) // (03:53: one line)
				];
				TSharedRef<SHorizontalBox> YearRow = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
					[
						Heading(TEXT("TAKE OVER IN"))
					];
				for (int32 Index = 0; Index < Opening.Years.Num(); ++Index)
				{
					const bool bOn = Index == Opening.YearIndex;
					YearRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(18.0f, 6.0f)).IsFocusable(false)
						.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PickGrandYear(Index); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Opening.Years[Index].Year).Font(Font(TEXT("Bold"), 14, 60)).ColorAndOpacity(bOn ? BrightBrass : Muted)
						]
					];
				}
				const FOpeningYear& Year = Opening.Years[FMath::Clamp(Opening.YearIndex, 0, Opening.Years.Num() - 1)];
				// (The UI director, on build 1596: every great power in sight.) The day's note stands beside the years
				// rather than under them, so that the great powers' row comes up into sight.
				YearRow->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(STextBlock).Text(Year.Title).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						Folding(FString::Printf(TEXT("Grand.YearNote.%d"), Opening.YearIndex), SNew(STextBlock).Text(Year.Note).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true))
					]
				];
				Starts->AddSlot().AutoHeight()[YearRow];

				if (Opening.bWorking)
				{
					Starts->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 70.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Italic"), 15)).ColorAndOpacity(Cream)
						.Text(FText::FromString(FString::Printf(TEXT("History is being played up to %s. This takes a few seconds."), *Year.Title.ToString())))
					];
					return;
				}

				TArray<TSharedRef<SWidget>> Great, Small, Rest;
				for (int32 Index = 0; Index < Opening.Nations.Num(); ++Index)
				{
					(Opening.Nations[Index].bMajor ? Great : Opening.Nations[Index].bSmall ? Small : Rest).Add(MakeNation(Opening.Nations[Index], Index, Index == Opening.NationIndex));
				}
				// (The UI director, on build 1596.) The great powers in one row of seven compact cards, in the list's own
				// order; the smaller nations after them in the same cards, eight to a row. (03:53: the label and the row
				// about 120 in all.)
				Starts->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)[ListHead(TEXT("THE GREAT POWERS"))];
				AddGrid(Great, 7);
				if (Small.Num() > 0)
				{
					// (03:53: the label says it; the note that was under it is gone.)
					Starts->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 8.0f)[ListHead(TEXT("OR A SMALLER NATION \u00B7 CHOOSE YOUR OWN PATH"))];
					AddGrid(Small, 8);
				}
				if (Rest.Num() > 0)
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 8.0f)[ListHead(TEXT("OTHER COUNTRIES"))];
					AddGrid(Rest, 8);
				}
				if (!Opening.GrandNote.IsEmpty())
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 4.0f)
					[
						Folding(TEXT("Grand.Note"), SNew(STextBlock).Text(Opening.GrandNote).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true))
					];
				}

				// The foot of the page: the country chosen, and the word to begin. (The UI director, on build 1596.) The
				// summary names the pick: its name with JUST WATCH and BEGIN AS ... beside it, then its status line in
				// italics, then the card's sentence, then the facts row and its ways as before. Under the name the lines
				// run the summary's whole width, so that a long sentence takes as few lines as it can.
				const bool bChosen = Opening.Nations.IsValidIndex(Opening.NationIndex);
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
				TSharedRef<SWidget> Foot = SNullWidget::NullWidget;
				// How hard the war is to be: five levels named for ranks, the chosen one lit, with what it changes beside them.
				TSharedRef<SHorizontalBox> Difficulty = SNew(SHorizontalBox);
				if (Opening.Levels.Num() > 0)
				{
					TSharedRef<SHorizontalBox> Ranks = SNew(SHorizontalBox);
					for (int32 LevelIndex = 0; LevelIndex < Opening.Levels.Num(); ++LevelIndex)
					{
						const FGrandLevel& Level = Opening.Levels[LevelIndex];
						const bool bPicked = LevelIndex == Opening.LevelIndex;
						Ranks->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(bPicked ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(10.0f, 5.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(Level.Blurb + TEXT("\n") + FString::Join(Level.Lines, TEXT("\n"))))
							.OnClicked_Lambda([this, LevelIndex]() { if (Controller.IsValid()) { Controller->PickLevel(LevelIndex); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(Level.Name.ToUpper())).Font(SmallCaps()).ColorAndOpacity(bPicked ? BrightBrass : Muted)
							]
						];
					}
					const FString Says = Opening.Levels.IsValidIndex(Opening.LevelIndex) ? Opening.Levels[Opening.LevelIndex].Blurb : FString();
					Difficulty->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBox).MinDesiredWidth(80.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("DIFFICULTY"))).Font(SmallCaps()).ColorAndOpacity(Brass)]
					];
					Difficulty->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[
						Ranks
					];
					Difficulty->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						Folding(TEXT("Foot.Difficulty"), SNew(STextBlock).Text(FText::FromString(Says)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted).AutoWrapText(true))
					];
				}
				// (Rules version 66) How deep it is played: three depths, the chosen one lit, with what it means beside them.
				TSharedRef<SHorizontalBox> HowDeep = SNew(SHorizontalBox);
				if (Opening.Depths.Num() > 0)
				{
					TSharedRef<SHorizontalBox> Steps = SNew(SHorizontalBox);
					for (int32 DepthIndex = 0; DepthIndex < Opening.Depths.Num(); ++DepthIndex)
					{
						const FGrandLevel& Depth = Opening.Depths[DepthIndex];
						const bool bPicked = DepthIndex == Opening.DepthIndex;
						Steps->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(bPicked ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(10.0f, 5.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(Depth.Name + TEXT(" (") + Depth.Under + TEXT(")\n") + Depth.Blurb + TEXT("\nOil, supply and manpower are in the game at every depth. You can change the depth, or any one desk, in any month.")))
							.OnClicked_Lambda([this, DepthIndex]() { if (Controller.IsValid()) { Controller->PickDepth(DepthIndex); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(Depth.Name.ToUpper())).Font(SmallCaps()).ColorAndOpacity(bPicked ? BrightBrass : Muted)
							]
						];
					}
					const FString Says = Opening.Depths.IsValidIndex(Opening.DepthIndex) ? Opening.Depths[Opening.DepthIndex].Under + TEXT(". ") + Opening.Depths[Opening.DepthIndex].Blurb : FString();
					HowDeep->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBox).MinDesiredWidth(80.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("HOW DEEP"))).Font(SmallCaps()).ColorAndOpacity(Brass)]
					];
					HowDeep->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[
						Steps
					];
					HowDeep->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						Folding(TEXT("Foot.Depth"), SNew(STextBlock).Text(FText::FromString(Says)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted).AutoWrapText(true))
					];
				}
				// (Rules version 70) The history dial: five stops, the chosen one lit, with what it means beside them.
				TSharedRef<SHorizontalBox> Dial = SNew(SHorizontalBox);
				if (Opening.Histories.Num() > 0)
				{
					TSharedRef<SHorizontalBox> Stops = SNew(SHorizontalBox);
					for (int32 StopIndex = 0; StopIndex < Opening.Histories.Num(); ++StopIndex)
					{
						const FGrandLevel& Stop = Opening.Histories[StopIndex];
						const bool bPicked = StopIndex == Opening.HistoryIndex;
						Stops->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(bPicked ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(10.0f, 5.0f)).IsFocusable(false)
							.ToolTipText(FText::FromString(FString::Printf(TEXT("%d. %s (%s)\n%s%s%s\nThe history dial can be turned again in any month."), Stop.Stop, *Stop.Name, *Stop.Under, *Stop.Blurb,
								Stop.Lines.Num() > 0 ? TEXT("\n") : TEXT(""), *FString::Join(Stop.Lines, TEXT("\n")))))
							.OnClicked_Lambda([this, StopIndex]() { if (Controller.IsValid()) { Controller->PickHistory(StopIndex); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(Stop.Name.ToUpper())).Font(SmallCaps()).ColorAndOpacity(bPicked ? BrightBrass : Muted)
							]
						];
					}
					const FString Says = Opening.Histories.IsValidIndex(Opening.HistoryIndex) ? Opening.Histories[Opening.HistoryIndex].Under + TEXT(". ") + Opening.Histories[Opening.HistoryIndex].Blurb : FString();
					Dial->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBox).MinDesiredWidth(80.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("HISTORY"))).Font(SmallCaps()).ColorAndOpacity(Brass)]
					];
					Dial->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[
						Stops
					];
					Dial->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						Folding(TEXT("Foot.History"), SNew(STextBlock).Text(FText::FromString(Says)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted).AutoWrapText(true))
					];
				}
				Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(14.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 9.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Nobody leads: every nation decides for itself, and you watch the war play out.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AskToBegin(2); } return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("JUST WATCH"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Muted)
					]
				];
				// (Without a pick the BEGIN button keeps its room, unseen, so that the row is as high either way.)
				Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 12.0f)).IsFocusable(false)
					.Visibility(bChosen ? EVisibility::Visible : EVisibility::Hidden)
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->AskToBegin(1); } return FReply::Handled(); })
					[
						// (the UI director's 84.3 check, item 6: the door's lit button takes Enter, and says so)
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 14, 200)).ColorAndOpacity(BrightBrass)
							.Text(FText::FromString(bChosen ? FString::Printf(TEXT("BEGIN AS %s"), *Opening.Nations[Opening.NationIndex].Name.ToString().ToUpper()) : FString(TEXT("BEGIN"))))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[KeyCap(TEXT("ENTER"), BrightBrass)]
					]
				];
				// (The UI director's review of build 1628, item 4: the list shows the same rows whichever nation is picked,
				// so that nothing jumps under the pointer as the player chooses.) The summary stands as high as the tallest
				// nation's: every nation's lines stand in it, one over another, folded at the summary's own width, and only
				// the picked one's show; the others keep their room unseen. Nothing picked, none show, in the same room.
				// (The summary is laid out afresh at every pick; its width is the same at every pick, and is kept, so that a new
				// pick folds at once at the width the last one had.)
				auto Wide = [this]() -> float
				{
					const float Had = FootSaid.IsValid() ? static_cast<float>(FootSaid->GetTickSpaceGeometry().GetLocalSize().X) : 0.0f;
					FootWide = Had > 1.0f ? Had : FootWide;
					return FootWide > 1.0f ? FootWide : 1000.0f; // (before it is first laid out: about the summary's width on the card)
				};
				TSharedRef<SOverlay> Said = SAssignNew(FootSaid, SOverlay);
				for (int32 Index = 0; Index < Opening.Nations.Num(); ++Index)
				{
					const FOpeningNation& Nation = Opening.Nations[Index];
					const bool bShown = Index == Opening.NationIndex;
					TSharedRef<SVerticalBox> Part = SNew(SVerticalBox).Visibility(bShown ? EVisibility::Visible : EVisibility::Hidden);
					if (!Nation.Standing.IsEmpty())
					{
						Part->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Nation.Standing).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Brass).WrapTextAt_Lambda(Wide)
						];
					}
					if (!Nation.Blurb.IsEmpty())
					{
						Part->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Nation.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).WrapTextAt_Lambda(Wide)
						];
					}
					// The facts: the date and what the country has, "September 1939 · 30 corps, 36 provinces". (The UI director,
					// 10:57 on build 1638: the war is said once, in the italic status above, and not again here.)
					Part->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Brass).WrapTextAt_Lambda(Wide)
						.Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s"), *Year.Title.ToString(), *Nation.Strength.ToString())))
					];
					// What the country does in its own way: its abilities and its special formation, by name.
					// What each one does is said when the mouse rests on the line, and in the game's own guide.
					if (Nation.Abilities.Num() > 0)
					{
						Part->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).WrapTextAt_Lambda(Wide)
							.ToolTipText(bShown ? FText::FromString(FString::Join(Nation.Abilities, TEXT("\n"))) : FText::GetEmpty())
							.Text(FText::FromString(AbilityNames(Nation.Abilities)))
						];
					}
					Said->AddSlot()[Part];
				}
				TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							bChosen
								? StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Opening.Nations[Opening.NationIndex].Name).Font(Font(TEXT("Regular"), 22)).ColorAndOpacity(Cream))
								: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(TEXT("Choose a country above to lead it."))).Font(Font(TEXT("Italic"), 13)).ColorAndOpacity(Muted))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							Buttons
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Said
					];
				Foot = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 4.0f, 16.0f, 0.0f)
					[
						// (the flag's room is kept without a pick, so that the lines fold at the same width)
						bChosen ? MakeFlag(Opening.Nations[Opening.NationIndex].Tag, 96.0f) : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(96.0f, 1.0f)))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
					[
						Lines
					];
				OpeningFoot->SetContent(SNew(SBorder).BorderImage(&FootBrush).Padding(FMargin(16.0f, 10.0f, 16.0f, 12.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						Difficulty
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Opening.Histories.Num() > 0 ? 6.0f : 8.0f)
					[
						HowDeep
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Opening.Histories.Num() > 0 ? 8.0f : 0.0f)
					[
						Dial
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Foot
					]
				]);
				OpeningFoot->SetVisibility(EVisibility::Visible);
				return;
			}

			// ---- The scenarios: single campaigns in which the other nations keep to history's plan.
			Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
			[
				Folding(TEXT("Scenarios.Intro"), SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
				.Text(FText::FromString(TEXT("Single campaigns from the war as it was fought. You lead one side; the other nations keep to history's plan. Or watch one play out."))))
			];
			for (int32 GroupIndex = 0; GroupIndex < Opening.Groups.Num(); ++GroupIndex)
			{
				Starts->AddSlot().AutoHeight().Padding(0.0f, GroupIndex == 0 ? 0.0f : 14.0f, 0.0f, 8.0f)
				[
					ListHead(Opening.Groups[GroupIndex].Title.ToString().ToUpper())
				];
				TArray<TSharedRef<SWidget>> Cells;
				for (int32 SideIndex = 0; SideIndex < Opening.Groups[GroupIndex].Sides.Num(); ++SideIndex)
				{
					Cells.Add(MakeStart(Opening.Groups[GroupIndex].Sides[SideIndex], GroupIndex, SideIndex));
				}
				AddGrid(Cells, 2);
			}

			// ---- (release 2) The practice battle: Normandy fought in the 3D scene, the player its American general
			// (Brian, 22 November 03:09Z: "do 1 2 and 3", the third this door). A battle on its own: no campaign changes.
			Starts->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
			[
				ListHead(TEXT("PRACTICE BATTLE: YOU COMMAND"))
			];
			{
				TArray<TSharedRef<SWidget>> Cells;
				Cells.Add(MakePractice());
				Cells.Add(MakeBattleRules());
				AddGrid(Cells, 2);
				// (for the Mac's pictures, which cannot scroll by hand: "-TheaterOpeningScroll" opens the list at its foot,
				// where the practice and Battle rules cards are)
				if (StartsScroll.IsValid() && FParse::Param(FCommandLine::Get(), TEXT("TheaterOpeningScroll")))
				{
					StartsScroll->ScrollToEnd();
				}
			}
		}

		/** (release 2) The practice battle's card: the hedgerows before Le Mesnil, July 1944, fought live as he commands. */
		TSharedRef<SWidget> MakePractice()
		{
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						MakeFlag(TEXT("USA"), 36.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Normandy, July 1944"))).Font(Font(TEXT("Bold"), 14)).ColorAndOpacity(Cream)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					Folding(TEXT("Practice.Blurb"), SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f)
					.Text(FText::FromString(TEXT("The hedgerows before Le Mesnil. You are the American general: place your units on your ground, then click a unit and click where it goes or whom it attacks. Your staff fights the units you leave to them."))))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Folding(TEXT("Practice.Aside"), SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Brass).AutoWrapText(true).LineHeightPercentage(1.15f)
					.Text(FText::FromString(TEXT("A battle on its own, in the 3D field: nothing in a campaign changes."))))
				];
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(14.0f, 10.0f, 14.0f, 12.0f)).IsFocusable(false)
				.OnClicked_Lambda([this]()
				{
					UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr;
					// (one at a time: a second click while the first is opening does nothing)
					if (!World || TActorIterator<ALandBattle>(World))
					{
						return FReply::Handled();
					}
					if (ALandBattle* Land = World->SpawnActor<ALandBattle>(FVector::ZeroVector, FRotator::ZeroRotator))
					{
						Land->LiveWhenReady(TEXT("normandy-1944"), TEXT("att"), 0, true);
					}
					return FReply::Handled();
				})
				[
					Body
				];
		}

		/**
		 * (release 2) The battle rules beside the practice card, each a line he clicks to switch on or off (Brian, 22
		 * November 17:33Z: "make it where we can toggle it off if someone doesnt want it"). Kept in his settings, read when
		 * a battle opens (FLandLive's rules).
		 */
		TSharedRef<SWidget> MakeBattleRules()
		{
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Battle rules"))).Font(Font(TEXT("Bold"), 14)).ColorAndOpacity(Cream)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 4.0f)
				[
					Folding(TEXT("Rules.Intro"), SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Brass).AutoWrapText(true)
					.Text(FText::FromString(TEXT("Click a rule to switch it on or off for your next battle."))))
				];
			// (the UI director, 22 November 20:56Z: each line a kit switch, lit brass when on and unlit when off, with ON
			// or OFF beside it so it never rests on the word alone; nothing on the card is red)
			static const FSlateRoundedBoxBrush SwitchTrack(FLinearColor::White, 8.0f, FLinearColor(FColor::FromHex(TEXT("8A6E44"))), 1.0f);
			static const FSlateRoundedBoxBrush SwitchKnob(FLinearColor::White, 6.0f);
			FLandLive::FlipFromCommandLine();
			for (int32 Index = 0; Index < FLandLive::RuleCount(); ++Index)
			{
				const FLandLive::FRule& Rule = FLandLive::RuleAt(Index);
				Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(6.0f, 4.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(Rule.Tip))
					.OnClicked_Lambda([Index]()
					{
						FLandLive::FlipRule(Index);
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							// The switch: a 30 by 16 track, brass and its knob at the right when on; the dark track and a
							// muted knob at the left when off.
							SNew(SBox).WidthOverride(30.0f).HeightOverride(16.0f)
							[
								SNew(SBorder).BorderImage(&SwitchTrack).Padding(FMargin(2.0f))
								.BorderBackgroundColor_Lambda([Index]() { return FSlateColor(FLandLive::RuleOn(Index) ? BrightBrass : FLinearColor(FColor::FromHex(TEXT("2A231B")))); })
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SBox).WidthOverride_Lambda([Index]() { return FOptionalSize(FLandLive::RuleOn(Index) ? 14.0f : 0.0f); })
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(SBox).WidthOverride(12.0f).HeightOverride(12.0f)
										[
											SNew(SBorder).BorderImage(&SwitchKnob).Padding(0.0f)
											.BorderBackgroundColor_Lambda([Index]() { return FSlateColor(FLandLive::RuleOn(Index) ? FLinearColor(FColor::FromHex(TEXT("1E1810"))) : Muted); })
										]
									]
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(30.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 10))
								.Text_Lambda([Index]() { return FText::FromString(FLandLive::RuleOn(Index) ? TEXT("ON") : TEXT("OFF")); })
								.ColorAndOpacity_Lambda([Index]() { return FSlateColor(FLandLive::RuleOn(Index) ? BrightBrass : Muted); })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(Rule.Label)).Font(Font(TEXT("Bold"), 11))
								.ColorAndOpacity_Lambda([Index]() { return FSlateColor(FLandLive::RuleOn(Index) ? Cream : Muted); })
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								Folding(TEXT("Rules.Tip"), SNew(STextBlock).Text(FText::FromString(Rule.Tip)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true))
							]
						]
					]
				];
			}
			return SNew(SBorder).BorderImage(&PlainButton.Normal).Padding(FMargin(14.0f, 10.0f, 14.0f, 12.0f))
				[
					Body
				];
		}

		/** One choice on the opening screen: a nation to lead or a war to watch, and what lies ahead. */
		TSharedRef<SWidget> MakeStart(const FOpeningSide& Side, int32 GroupIndex, int32 SideIndex)
		{
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						MakeFlag(Side.Tag, 36.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Side.Name).Font(Font(TEXT("Bold"), 14)).ColorAndOpacity(Cream)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					Folding(TEXT("Start.Blurb"), SNew(STextBlock).Text(Side.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f))
				];
			if (!Side.Aims.IsEmpty())
			{
				Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Folding(TEXT("Start.Aims"), SNew(STextBlock).Text(Side.Aims).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Brass).AutoWrapText(true).LineHeightPercentage(1.15f))
				];
			}
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(14.0f, 10.0f, 14.0f, 12.0f)).IsFocusable(false)
				.OnClicked_Lambda([this, GroupIndex, SideIndex]()
				{
					if (Controller.IsValid())
					{
						Controller->AskToBegin(3, GroupIndex, SideIndex);
					}
					return FReply::Handled();
				})
				[
					Body
				];
		}

		/** The studio's picture of a model, as something to draw; null if there is no such model. Kept once made. */
		TSharedPtr<FSlateBrush> PortraitOf(const FString& Model, bool bLarge = false)
		{
			TMap<FString, TSharedPtr<FSlateBrush>>& Brushes = bLarge ? LargePortraitBrushes : PortraitBrushes;
			if (const TSharedPtr<FSlateBrush>* Kept = Brushes.Find(Model))
			{
				return *Kept;
			}
			TSharedPtr<FSlateBrush> Made;
			ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			if (UTextureRenderTarget2D* Picture = Table ? Table->GetPortrait(Model, bLarge) : nullptr)
			{
				Made = MakeShared<FSlateBrush>();
				Made->SetResourceObject(Picture);
				Made->ImageSize = bLarge ? FVector2D(552.0, 336.0) : FVector2D(184.0, 112.0);
				Made->DrawAs = ESlateBrushDrawType::Image;
			}
			Brushes.Add(Model, Made);
			return Made;
		}

		/**
		 * (release 1) The bigger card, shown when the pointer rests on a unit's card: a large picture of what it is,
		 * how many men it has and how much of it is left, how it holds together, its weight in attack and defence,
		 * what it is for, and the ground it is at its best and its worst on. Every figure is the rules' own. Part is
		 * the one division pointed at, or null for the corps as a whole.
		 */
		TSharedRef<SWidget> MakeBigCard(const FTrayCard& Card, const FViewDivision* Part, const FString& Kind, const FString& Tag, const FString& Foot)
		{
			const FString Arm = Kind == TEXT("heavy") ? FString(TEXT("arm")) : Kind;
			const FArmNote* Note = Controller.IsValid() ? Controller->FindArm(Arm) : nullptr;
			// (Laid out on its side, the picture on the left and the figures beside it, so that it stands clear of the
			// army's tray below it; the notes on its ground run under both.)
			const float Wide = 330.0f, PictureWide = 290.0f;
			TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
			TSharedRef<SVerticalBox> Under = SNew(SVerticalBox);
			const TSharedPtr<FSlateBrush> Portrait = PortraitOf(ACampaignMapActor::ModelFor(Tag, Kind), true);
			// Its name, and what it is.
			const FString Name = Part ? Part->Full : Card.Nickname.IsEmpty() ? Card.Name.ToString() : FString::Printf(TEXT("%s \u201C%s\u201D"), *Card.Name.ToString(), *Card.Nickname.ToString());
			Lines->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream).WrapTextAt(Wide)];
			const int32 Men = Part ? Part->Men : Card.Men;
			const float Left = Part ? Part->Strength : Card.Strength;
			const FString About = Card.Stale > 0 ? FString(TEXT("about ")) : FString();
			TArray<FString> What;
			{
				// (the frame's own word for the arm, as on the small card: "armoured", not the rules' table heading)
				const FString Word = FCampaignMapData::CorpsKindLabel(Arm);
				What.Add(Word.Left(1).ToUpper() + Word.Mid(1));
			}
			if (!Part)
			{
				What.Add(FString::Printf(TEXT("%d division%s"), Card.Divisions, Card.Divisions == 1 ? TEXT("") : TEXT("s")));
			}
			else if (!Part->Grade.IsEmpty())
			{
				What.Add(Part->bHardened ? Part->Grade + TEXT(", battle-hardened") : Part->Grade);
			}
			if (Men > 0)
			{
				What.Add(FString::Printf(TEXT("%s%s men"), *About, *FText::AsNumber(Men).ToString()));
			}
			Lines->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 7.0f)[SNew(STextBlock).Text(FText::FromString(FString::Join(What, TEXT("  \u00B7  ")))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).WrapTextAt(Wide)];
			// How much of it is left, and how it holds together: a bar each, with its figure or its word.
			auto Bar = [this, &Lines](const TCHAR* Label, float Share, const FLinearColor& Colour, const FString& Word)
			{
				const float Full = FMath::Clamp(Share, 0.0f, 1.0f);
				Lines->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(76.0f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(Brass)]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(120.0f).HeightOverride(6.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(Full, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Colour)]
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Full, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Word)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Cream)]
				];
			};
			Bar(TEXT("STRENGTH"), Left, Left < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("8FBF6A"))), FString::Printf(TEXT("%s%d%%"), *About, FMath::RoundToInt32(Left * 100.0f)));
			if (Card.Stale == 0)
			{
				Bar(TEXT("MORALE"), Card.Morale, Card.Morale < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("6FA3D8"))),
					Card.bMood && !Card.MoraleWord.IsEmpty() ? Card.MoraleWord : FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Card.Morale * 100.0f)));
				if (Card.bMood)
				{
					Bar(TEXT("REST"), Card.Rest, Card.Rest < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("C9A227"))), Card.bResting ? Card.RestWord + TEXT(", resting") : Card.RestWord);
				}
			}
			if (Note)
			{
				// Its weight in a fight, what it is for, and its ground.
				Lines->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 5.0f)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("ATTACK %d      DEFENCE %d"), Note->Attack, Note->Defence))).Font(Typed(10, true)).ColorAndOpacity(BrightBrass)
				];
				if (!Note->Role.IsEmpty())
				{
					Lines->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[SNew(STextBlock).Text(FText::FromString(Note->Role)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Cream).WrapTextAt(Wide).LineHeightPercentage(1.1f)];
				}
				const float Full = Wide + (Portrait.IsValid() ? PictureWide + 14.0f : 0.0f);
				auto Ground = [this, &Under, Full](const TCHAR* Label, const FString& Text, const FLinearColor& Colour)
				{
					if (Text.IsEmpty())
					{
						return;
					}
					Under->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(128.0f).Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))[SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(Colour)]]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(Text.Left(1).ToUpper() + Text.Mid(1) + TEXT("."))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Cream).WrapTextAt(Full - 128.0f).LineHeightPercentage(1.1f)]
					];
				};
				Ground(TEXT("AT ITS BEST IN"), Note->Good, FLinearColor(FColor::FromHex(TEXT("8FBF6A"))));
				Ground(TEXT("AT ITS WORST IN"), Note->Poor, FLinearColor(FColor::FromHex(TEXT("D98A6A"))));
			}
			if (!Part && !Card.Service.IsEmpty())
			{
				Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[SNew(STextBlock).Text(FText::FromString(Card.Service)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted).WrapTextAt(Wide)];
			}
			if (!Foot.IsEmpty())
			{
				// (the design director's ruling: at the strength of the card's other words, or it cannot be read)
				Under->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)[SNew(STextBlock).Text(FText::FromString(Foot)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Cream.CopyWithNewOpacity(0.68f))];
			}
			TSharedRef<SHorizontalBox> Top = SNew(SHorizontalBox);
			if (Portrait.IsValid())
			{
				Top->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 14.0f, 0.0f)[SNew(SPortrait).Picture(Portrait).Size(FVector2D(PictureWide, PictureWide * 336.0 / 552.0))];
			}
			Top->AddSlot().AutoWidth().VAlign(VAlign_Top)[SNew(SBox).WidthOverride(Wide)[Lines]];
			// (on a backing that lets nothing of the table through: the card is read, not looked past)
			return SNew(SBorder).BorderImage(&BigCardBrush).Padding(FMargin(14.0f, 13.0f, 14.0f, 11.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Top]
					+ SVerticalBox::Slot().AutoHeight()[Under]
				];
		}

		/** A small picture of a kind of troops, made of plain shapes: a tank, a gun, a lorry, a horseman, men on foot. */
		TSharedRef<SWidget> KindPicture(const FString& Kind, const FLinearColor& Ink)
		{
			TSharedRef<SOverlay> Picture = SNew(SOverlay);
			auto Shape = [this, &Picture, &Ink](float X, float Y, float W, float H, bool bRound = false)
			{
				Picture->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(X, Y, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(W).HeightOverride(H)[SNew(SBorder).BorderImage(bRound ? &DotBrush : &BarBrush).BorderBackgroundColor(Ink)]
				];
			};
			auto Man = [&Shape](float X, float Y)
			{
				Shape(X + 1.0f, Y, 5.0f, 5.0f, true); // head and helmet
				Shape(X, Y + 5.5f, 7.0f, 9.0f); // coat
				Shape(X + 0.5f, Y + 14.5f, 2.5f, 6.0f); // legs
				Shape(X + 4.0f, Y + 14.5f, 2.5f, 6.0f);
				Shape(X + 7.5f, Y + 1.0f, 1.5f, 13.0f); // rifle at the shoulder
			};
			if (Kind == TEXT("arm"))
			{
				Shape(4.0f, 15.0f, 36.0f, 8.0f); // hull
				Shape(2.0f, 21.0f, 40.0f, 4.0f); // tracks
				Shape(14.0f, 8.0f, 15.0f, 8.0f); // turret
				Shape(28.0f, 10.5f, 15.0f, 2.5f); // gun
			}
			else if (Kind == TEXT("art"))
			{
				Shape(8.0f, 9.0f, 30.0f, 3.5f); // barrel
				Shape(12.0f, 11.0f, 12.0f, 6.0f); // breech and shield
				Shape(13.0f, 14.0f, 11.0f, 11.0f, true); // wheel
				Shape(2.0f, 21.0f, 14.0f, 2.5f); // trail
			}
			else if (Kind == TEXT("mot"))
			{
				Shape(4.0f, 9.0f, 24.0f, 11.0f); // body under its canvas
				Shape(29.0f, 12.0f, 10.0f, 8.0f); // cab
				Shape(8.0f, 18.0f, 8.0f, 8.0f, true); // wheels
				Shape(28.0f, 18.0f, 8.0f, 8.0f, true);
			}
			else if (Kind == TEXT("cav"))
			{
				Shape(8.0f, 13.0f, 24.0f, 7.0f); // the horse
				Shape(30.0f, 7.0f, 5.0f, 9.0f); // its neck
				Shape(32.0f, 5.0f, 8.0f, 4.0f); // and head
				Shape(9.0f, 19.0f, 2.5f, 8.0f);
				Shape(15.0f, 19.0f, 2.5f, 8.0f);
				Shape(24.0f, 19.0f, 2.5f, 8.0f);
				Shape(29.0f, 19.0f, 2.5f, 8.0f);
				Shape(17.0f, 2.0f, 5.0f, 5.0f, true); // the rider
				Shape(16.0f, 7.0f, 7.0f, 7.0f);
			}
			else if (Kind == TEXT("fort"))
			{
				Shape(4.0f, 12.0f, 36.0f, 13.0f); // the bunker
				Shape(9.0f, 8.0f, 26.0f, 5.0f);
				Picture->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(14.0f, 16.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(3.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f))]
				];
			}
			else if (Kind == TEXT("mtn"))
			{
				Man(6.0f, 5.0f);
				Shape(22.0f, 20.0f, 20.0f, 5.0f); // a mountain behind him, in steps
				Shape(25.0f, 15.0f, 14.0f, 5.0f);
				Shape(28.0f, 10.0f, 8.0f, 5.0f);
				Shape(30.5f, 6.0f, 3.0f, 4.0f);
			}
			else if (Kind == TEXT("eng"))
			{
				Man(6.0f, 5.0f);
				Shape(20.0f, 16.0f, 22.0f, 3.0f); // a bridge span
				Shape(22.0f, 19.0f, 3.0f, 7.0f);
				Shape(37.0f, 19.0f, 3.0f, 7.0f);
			}
			else
			{
				Man(5.0f, 5.0f);
				Man(18.0f, 5.0f);
				Man(31.0f, 5.0f);
			}
			return SNew(SBox).WidthOverride(46.0f).HeightOverride(28.0f)[Picture];
		}

		/** A thin meter: how much is left of something. */
		TSharedRef<SWidget> Meter(float Share, const FLinearColor& Ink, const TCHAR* Tip)
		{
			const float Left = FMath::Clamp(Share, 0.0f, 1.0f);
			return SNew(SBox).HeightOverride(4.0f).ToolTipText(FText::FromString(FString::Printf(TEXT("%s: %d%%"), Tip, FMath::RoundToInt32(Left * 100.0f))))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(Left, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Ink)]
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Left, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
				];
		}

		/**
		 * (Rules version 84.) The row of weapon signs on a corps' card: its anti-tank guns with their level, machine guns
		 * and mortars once their technology is in, snipers always. A sign not yet earned is left out and its cell stays
		 * empty, so that each sign keeps its place in the row. And "Digging" for a corps that has begun to dig where
		 * the first month's digging shows no works. Nothing for a corps the rule counts no weapons for.
		 */
		TSharedRef<SWidget> CarryRow(const FTrayCard& Card, bool bGoing)
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			const FLinearColor Ink = bGoing ? Brass : Faint;
			if (Card.Carry.Num() >= 4)
			{
				auto Cell = [this, &Row, &Ink, bGoing](bool bHas, const TCHAR* Sign, const FString& Says, const FString& Figure)
				{
					Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(SBox).MinDesiredWidth(15.0f).HeightOverride(15.0f).ToolTipText(FText::FromString(bHas ? Says : FString()))
						[
							bHas
							? StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWeaponSign).Sign(Sign).Size(15.0f).Ink(Ink)]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Figure.IsEmpty() ? 0.0f : 3.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(Figure)).Font(Typed(9.0f, true)).ColorAndOpacity(bGoing ? Cream : Faint)
								])
							: StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(15.0, 15.0)))
						]
					];
				};
				Cell(true, TEXT("anti-tank"), FString::Printf(TEXT("Anti-tank guns, level %d"), Card.Carry[0]), FString::FromInt(Card.Carry[0]));
				Cell(Card.Carry[1] > 0, TEXT("machine-gun"), TEXT("Machine guns"), FString());
				Cell(Card.Carry[2] > 0, TEXT("mortar"), TEXT("Mortars"), FString());
				Cell(Card.Carry[3] > 0, TEXT("sniper"), TEXT("Snipers"), FString());
			}
			if (Card.bDigging)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(Card.Carry.Num() >= 4 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("DIGGING  \u00B7  NO WORKS YET"))).Font(SmallCaps()).ColorAndOpacity(bGoing ? Muted : Faint)
					.ToolTipText(FText::FromString(TEXT("Digging in. There is rock under the sand here: in the open desert fieldworks come a month late. After another whole month where it stands its works are half finished.")))
				];
			}
			return Row;
		}

		/**
		 * One corps of the army in hand, as a card for each of its divisions: a picture of what it is, how much of
		 * it is left, how well it holds together, and how seasoned it is. A click on any of them tells the corps to
		 * come along or stay behind.
		 */
		TSharedRef<SWidget> MakeCard(const FTrayCard& Card)
		{
			// (a card of an army only being looked at is shown as plainly as one in hand, where it is known)
			const bool bGoing = Card.bLook ? !Card.bUnknown : (Card.bPicked && Card.bCanOrder);
			const FLinearColor Ink = bGoing ? Cream : Muted;
			const FString MovesLine = Card.bLook ? (Card.bUnknown ? FString(TEXT("kind and strength not known"))
					: Card.Stale > 0 ? FString::Printf(TEXT("from a report %d month%s old"), Card.Stale, Card.Stale == 1 ? TEXT("") : TEXT("s")) : FString(TEXT("as it stands today")))
				: !Card.bCanOrder ? (!Card.NoOrders.IsEmpty() ? Card.NoOrders : FString(Card.KindId == TEXT("fort") ? TEXT("fortress troops: they do not move") : TEXT("holds its ground")))
				: (!Card.bPicked ? FString(TEXT("staying behind"))
					: !Card.Marching.IsEmpty() ? Card.Marching // (release 1: a corps on a standing march says where to, and how long)
					: (Card.Moves <= 0 && Card.MovesMax > 0) ? FString(TEXT("No moves left this month")) // (ruling 87)
					: FString::Printf(TEXT("moves %d of %d"), Card.Moves, Card.MovesMax));
			const FLinearColor StrengthInk = Card.Strength < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("8FBF6A")));
			const FLinearColor MoraleInk = Card.Morale < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("6FA3D8")));
			const int32 Pips = FMath::Clamp(1 + Card.Experience / 22, 1, 5);
			const FString Tip = Card.bUnknown ? FString(TEXT("A corps in plain view that nobody has identified: its kind and its strength are not known. A battle, an agent on watch or a reconnaissance flight would show what it is."))
				: FString::Printf(TEXT("%s: %s, %d division%s. Strength %s%d%%%s. %s.%s"),
				*Card.Name.ToString(), *Card.Kind.ToString(), Card.Divisions, Card.Divisions == 1 ? TEXT("") : TEXT("s"),
				Card.Stale > 0 ? TEXT("about ") : TEXT(""), FMath::RoundToInt32(Card.Strength * 100.0f),
				Card.Stale > 0 ? TEXT("") : *FString::Printf(TEXT(", morale %d%%"), FMath::RoundToInt32(Card.Morale * 100.0f)),
				Card.Service.IsEmpty() ? *Card.Grade.ToString() : *Card.Service,
				Card.bLook ? TEXT("") : TEXT(" Click to tell this corps to come along or stay behind."));

			// The picture on each card is the game's own model of what the division is: this nation's tank of the
			// year, its gun, its soldier. Where there is no model yet, a plain drawing stands in.
			const FString Tag = !Card.Ally.IsEmpty() ? Card.Ally : Controller.IsValid() ? Controller->GetArmyTray().Tag : FString();
			// (Release 2, breakup.ts; Brian 24 November 2026 18:02Z: "see each individual unit ... so I can break it up".)
			// Where breaking corps up is on, each division's tile takes a click: picked, it is lit in brass and marked to
			// split off; SPLIT OFF under the tiles makes the picked ones a corps of their own.
			const bool bBreakable = !Card.bLook && Card.bCanBreak && Controller.IsValid();
			const int32 Picked = bBreakable ? Controller->DivisionsPicked(Card.CorpsId) : 0;
			TSharedRef<SHorizontalBox> Divisions = SNew(SHorizontalBox);
			const int32 Count = Card.Parts.Num() > 0 ? FMath::Min(Card.Parts.Num(), 12) : FMath::Clamp(Card.Divisions, 1, 12);
			for (int32 Division = 0; Division < Count; ++Division)
			{
				// Each division is named and has its own strength and its own stripes, where the rules keep them.
				const FViewDivision* Part = Card.Parts.IsValidIndex(Division) ? &Card.Parts[Division] : nullptr;
				const FString Kind = Part && !Part->Kind.IsEmpty() && Card.KindId != TEXT("heavy") ? Part->Kind : Card.KindId;
				const TSharedPtr<FSlateBrush> Portrait = PortraitOf(ACampaignMapActor::ModelFor(Tag, Kind));
				const TSharedRef<SWidget> Picture = Card.bUnknown
					? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(74.0f).HeightOverride(45.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("?"))).Font(Font(TEXT("Bold"), 26)).ColorAndOpacity(Muted)
						])
					: Portrait.IsValid()
					? StaticCastSharedRef<SWidget>(SNew(SPortrait).Picture(Portrait).Size(FVector2D(74.0, 45.0)))
					: KindPicture(Kind == TEXT("heavy") ? FString(TEXT("arm")) : Kind, Ink);
				const float Left = Part ? Part->Strength : Card.Strength;
				const int32 Lit = Part ? FMath::Clamp(Part->Stripes, 0, 4) : Pips - 1;
				TSharedRef<SHorizontalBox> Stripes = SNew(SHorizontalBox);
				for (int32 Pip = 0; Pip < 4; ++Pip)
				{
					Stripes->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(8.0f).HeightOverride(3.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Pip < Lit ? BrightBrass : FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
					];
				}
				const FString PartTip = Part
					? FString::Printf(TEXT("%s\n%s%s. %s men, strength %d%%."), *Part->Full, *Part->Grade, Part->bHardened ? TEXT(", battle-hardened") : TEXT(""), *FText::AsNumber(Part->Men).ToString(), FMath::RoundToInt32(Part->Strength * 100.0f))
					: Tip;
				TSharedRef<SVerticalBox> Face = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Picture];
				if (Part)
				{
					Face->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						// (a division's name folds to a second line and is not cut short; every tile keeps room for two)
						SNew(SBox).WidthOverride(74.0f).MinDesiredHeight(21.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Part->Name)).Font(Font(TEXT("Bold"), 8)).ColorAndOpacity(bGoing ? Cream : Faint).AutoWrapText(true).LineHeightPercentage(0.95f)
						]
					];
				}
				const bool bMarked = bBreakable && Controller->IsDivisionPicked(Card.CorpsId, Division);
				if (!Card.bUnknown)
				{
					Face->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Meter(Left, bGoing ? (Left < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("8FBF6A")))) : Faint, Card.Stale > 0 ? TEXT("Strength, about") : TEXT("Strength"))];
					if (!Card.bMood && !Part && Card.Stale == 0)
					{
						Face->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[Meter(Card.Morale, bGoing ? MoraleInk : Faint, TEXT("Morale"))];
					}
					Face->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[Stripes];
				}
				if (bMarked)
				{
					Face->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("SPLITS OFF"))).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
					];
				}
				// (release 1) Point at a division's tile for the bigger card of that division: it is laid out above the tray.
				const TSharedRef<SWidget> Tile = Card.bUnknown
					? StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f)).ToolTipText(FText::FromString(PartTip))[Face])
					: BigCardTarget(SNew(SBorder).BorderImage(&CardBrush).BorderBackgroundColor(bMarked ? BrightBrass : FLinearColor::White).Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))[Face], Card.CorpsId, Part ? Division : INDEX_NONE);
				Divisions->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					!bBreakable || Card.bUnknown ? Tile
					// (its backing keeps the press on the tile: it would otherwise fall through to the card's own press,
					// which leaves the whole corps behind)
					: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(&NoBrush).Padding(FMargin(0.0f))
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						.OnMouseDoubleClick_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0.0f)).IsFocusable(false)
							.OnClicked_Lambda([this, Id = Card.CorpsId, Pick = Division]() { if (Controller.IsValid()) { Controller->ToggleDivision(Id, Pick); } return FReply::Handled(); })
							[
								Tile
							]
						])
				];
			}
			// SPLIT OFF and REJOIN, under the tiles: each with a backing that keeps its press from the card's own.
			TSharedRef<SHorizontalBox> BreakRow = SNew(SHorizontalBox);
			auto BreakButton = [this](const FString& Label, const FString& Tip, bool bLit, TFunction<void()> Press) -> TSharedRef<SWidget>
			{
				return SNew(SBorder).BorderImage(&NoBrush).Padding(FMargin(0.0f))
					.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					.OnMouseDoubleClick_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
					[
						SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(8.0f, 2.0f)).IsFocusable(false).IsEnabled(bLit)
						.ToolTipText(FText::FromString(Tip))
						.OnClicked_Lambda([Press]() { Press(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(bLit ? BrightBrass : Faint)
						]
					];
			};
			bool bBreakRow = false;
			if (bBreakable)
			{
				bBreakRow = true;
				if (Picked == 0)
				{
					BreakRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Click division tiles to split them off"))).Font(Font(TEXT("Italic"), 9)).ColorAndOpacity(Faint)
					];
				}
				else
				{
					const bool bLit = Picked < Card.Divisions;
					BreakRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						BreakButton(FString::Printf(TEXT("SPLIT OFF %d"), Picked),
							bLit ? FString::Printf(TEXT("The %d division%s picked become%s a corps of %s own here, and stay%s behind when the army moves."), Picked, Picked == 1 ? TEXT("") : TEXT("s"), Picked == 1 ? TEXT("s") : TEXT(""), Picked == 1 ? TEXT("its") : TEXT("their"), Picked == 1 ? TEXT("s") : TEXT(""))
								: FString(TEXT("Some divisions have to stay with the corps: leave at least one tile unpicked.")),
							bLit, [this, Id = Card.CorpsId]() { if (Controller.IsValid()) { Controller->SplitOffDivisions(Id); } })
					];
				}
			}
			else if (!Card.bLook && !Card.bUnknown && !Card.BreakWhy.IsEmpty() && Card.Divisions >= 2)
			{
				// (the pre-check's read, 24 November: where breaking up is on and this corps cannot be, the card says why,
				// faint, in place of the hint: a garrison, a corps at home or marching with an ally, another's month)
				bBreakRow = true;
				BreakRow->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Card.BreakWhy)).Font(Font(TEXT("Italic"), 9)).ColorAndOpacity(Faint).AutoWrapText(true)
				];
			}
			if (!Card.bLook && Card.RejoinId != INDEX_NONE && Controller.IsValid())
			{
				bBreakRow = true;
				BreakRow->AddSlot().AutoWidth().VAlign(VAlign_Center)
				[
					BreakButton(FString::Printf(TEXT("REJOIN %s"), *Card.RejoinName.ToUpper()),
						FString::Printf(TEXT("Its divisions go back into %s, which stands here with it."), *Card.RejoinName), true,
						[this, Id = Card.CorpsId, Into = Card.RejoinId]() { if (Controller.IsValid()) { Controller->RejoinCorps(Id, Into); } })
				];
			}

			// Morale and rest belong to the whole corps: a bar each with its word, and which way morale is going.
			auto MoodBar = [this, bGoing](const TCHAR* Label, const FString& Word, float Share, const FLinearColor& Colour, const FString& Arrow)
			{
				const float Left = FMath::Clamp(Share, 0.0f, 1.0f);
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(52.0f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallCaps()).ColorAndOpacity(bGoing ? Brass : Faint)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(56.0f).HeightOverride(5.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(Left, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(bGoing ? Colour : Faint)]
							+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Left, 0.001f))[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Arrow.IsEmpty() ? Word : FString::Printf(TEXT("%s %s"), *Word, *Arrow))).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bGoing ? Cream : Faint)
					];
			};
			const FString Arrow = Card.Trend == TEXT("rising") ? FString(TEXT("\u25B2")) : Card.Trend == TEXT("falling") ? FString(TEXT("\u25BC")) : FString();
			const FLinearColor RestInk = Card.Rest < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("C9A227")));
			const TSharedRef<SWidget> Mood = Card.bMood
				? StaticCastSharedRef<SWidget>(SNew(SBox).ToolTipText(FText::FromString(Card.MoodTip))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[MoodBar(TEXT("MORALE"), Card.MoraleWord, Card.Morale, MoraleInk, Arrow)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[MoodBar(TEXT("REST"), Card.bResting ? Card.RestWord + TEXT(", resting") : Card.RestWord, Card.Rest, RestInk, FString())]
					])
				: StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(0.0, 0.0)));

			// (release 1) Point at the corps' card for the bigger card of the corps, laid out above the tray. One nobody
			// has identified keeps its plain line.
			const TSharedRef<SButton> Whole = SNew(SButton).ButtonStyle(bGoing ? &PickedCard : &LeftCard).ContentPadding(FMargin(8.0f, 6.0f, 4.0f, 7.0f))
				.IsFocusable(false).IsEnabled((Card.bCanOrder || Card.bLook) && Card.Apart.IsEmpty())
				.ToolTipText(Card.bUnknown ? FText::FromString(Tip) : !Card.Apart.IsEmpty() ? FText::FromString(Card.Apart) : FText::GetEmpty())
				.OnHovered_Lambda([this, Id = Card.CorpsId]() { if (Controller.IsValid()) { Controller->ShowCorpsModel(Id); } })
				.OnClicked_Lambda([this, Id = Card.CorpsId]()
				{
					if (Controller.IsValid())
					{
						Controller->ToggleCorps(Id);
					}
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, Card.Ally.IsEmpty() ? 0.0f : 6.0f, 0.0f)
						[
							// An ally's corps marching with the army carries its own flag.
							SNew(SBox).WidthOverride(Card.Ally.IsEmpty() ? 0.0f : 21.0f).HeightOverride(14.0f)
							.Visibility(Card.Ally.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
							[
								SNew(SImage).Image(FlagBrush(Card.Ally))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Card.Nickname.IsEmpty() ? Card.Name : FText::FromString(FString::Printf(TEXT("%s \u201C%s\u201D"), *Card.Name.ToString(), *Card.Nickname.ToString()))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							MakeRibbons(Card.Honours, 4)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(6.0f, 1.0f)).IsFocusable(false)
							.Visibility(Card.Ally.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							.ToolTipText(FText::FromString(TEXT("Send this corps back to its own country's command.")))
							.OnClicked_Lambda([this, Id = Card.CorpsId]() { if (Controller.IsValid()) { Controller->SendBackCorps(Id); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("SEND BACK"))).Font(SmallCaps()).ColorAndOpacity(BrightBrass)
							]
						]
						// The way to the corps' service record: its honours and its diary.
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(6.0f, 1.0f)).IsFocusable(false)
							.Visibility(Card.bLook ? EVisibility::Collapsed : EVisibility::Visible)
							.ToolTipText(FText::FromString(TEXT("Its service record: what it has done, its honours and its diary.")))
							.OnClicked_Lambda([this, Id = Card.CorpsId]() { if (Controller.IsValid()) { Controller->OpenRecord(Id); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("RECORD"))).Font(SmallCaps()).ColorAndOpacity(Muted)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 4.0f, 5.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bGoing ? BrightBrass : Faint)
						.Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s  \u00B7  %s"), *Card.Kind.ToString(), *Card.Grade.ToString(), *MovesLine)))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 4.0f, Card.bMood ? 6.0f : 0.0f)[Mood]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 4.0f, Card.Carry.Num() >= 4 || Card.bDigging ? 6.0f : 0.0f)[CarryRow(Card, bGoing)]
					+ SVerticalBox::Slot().AutoHeight()[Divisions]
					// (release 2, breakup.ts) SPLIT OFF and REJOIN
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, bBreakRow ? 6.0f : 0.0f, 4.0f, 0.0f)
					[
						bBreakRow ? StaticCastSharedRef<SWidget>(BreakRow) : SNullWidget::NullWidget
					]
				];
			if (!Card.bUnknown)
			{
				BigCardTargets.Add({ Whole, Card.CorpsId, INDEX_NONE });
			}
			return Whole;
		}

		/** Notes a division's tile as something the bigger card is shown for while the pointer rests on it. */
		TSharedRef<SWidget> BigCardTarget(TSharedRef<SWidget> Tile, int32 CorpsId, int32 Part)
		{
			BigCardTargets.Add({ Tile, CorpsId, Part });
			return Tile;
		}

		/**
		 * (release 1) Lays the bigger card out above the tray for whatever card or tile the pointer rests on, and takes
		 * it away when it rests on none. ("-TheaterBigCard" keeps the army's first corps' card up for a picture, and
		 * "-TheaterBigCard=2" its first division's.)
		 */
		void UpdateBigCard()
		{
			if (!BigCardShow.IsValid() || !Controller.IsValid())
			{
				return;
			}
			int32 CorpsId = INDEX_NONE, Part = INDEX_NONE;
			for (const FBigCardTarget& Target : BigCardTargets)
			{
				const TSharedPtr<SWidget> Widget = Target.Widget.Pin();
				if (Widget.IsValid() && Widget->IsHovered() && (CorpsId == INDEX_NONE || Target.Part != INDEX_NONE))
				{
					CorpsId = Target.CorpsId;
					Part = Target.Part;
				}
			}
			const FArmyTray& Tray = Controller->GetArmyTray();
			static int32 Which = 0;
			static const bool bForPicture = FParse::Param(FCommandLine::Get(), TEXT("TheaterBigCard")) || FParse::Value(FCommandLine::Get(), TEXT("TheaterBigCard="), Which);
			if (CorpsId == INDEX_NONE && bForPicture && Tray.bVisible && Tray.Cards.Num() > 0 && !Tray.Cards[0].bUnknown)
			{
				CorpsId = Tray.Cards[0].CorpsId;
				Part = Which == 2 && Tray.Cards[0].Parts.Num() > 0 ? 0 : INDEX_NONE;
			}
			const FTrayCard* Card = Tray.bVisible && CorpsId != INDEX_NONE ? Tray.Cards.FindByPredicate([CorpsId](const FTrayCard& Each) { return Each.CorpsId == CorpsId; }) : nullptr;
			if (!Card)
			{
				CorpsId = Part = INDEX_NONE;
			}
			if (CorpsId == BigCardCorps && Part == BigCardPart && Tray.Revision == BigCardRevision)
			{
				return;
			}
			BigCardCorps = CorpsId;
			BigCardPart = Part;
			BigCardRevision = Tray.Revision;
			if (!Card)
			{
				BigCardShow->SetContent(SNullWidget::NullWidget);
				return;
			}
			const FViewDivision* Division = Card->Parts.IsValidIndex(Part) ? &Card->Parts[Part] : nullptr;
			const FString Tag = !Card->Ally.IsEmpty() ? Card->Ally : Tray.Tag;
			BigCardShow->SetContent(MakeBigCard(*Card, Division, Division && !Division->Kind.IsEmpty() && Card->KindId != TEXT("heavy") ? Division->Kind : Card->KindId, Tag,
				Card->bLook ? FString() : FString(TEXT("Click to tell this corps to come along or stay behind."))));
		}

		/** Where the bigger card stands: straight above the army's tray, their left edges in one line. */
		FMargin BigCardPlace() const
		{
			const TSharedPtr<SWidget> Tray = TrayPiece.Pin();
			if (!Tray.IsValid() || !BigCardShow.IsValid())
			{
				return FMargin(24.0f, 262.0f, 0.0f, 0.0f);
			}
			FVector2D Size(1920.0, 1080.0);
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(Size);
			}
			// (window pixels to this screen's units: the frame's own scale and, on top of it, Unreal's for the window's size. The pre-checker's find of 18 November 2026: by the first alone these came out two thirds too large in Brian's window.)
			const float Scale = FrameScale(Size.Y) * FMath::Max(GetTickSpaceGeometry().Scale, 0.01f);
			const FVector2D At = (FVector2D(Tray->GetTickSpaceGeometry().GetAbsolutePosition()) - FVector2D(GetTickSpaceGeometry().GetAbsolutePosition())) / Scale;
			const float High = static_cast<float>(BigCardShow->GetDesiredSize().Y);
			return FMargin(static_cast<float>(At.X), FMath::Max(static_cast<float>(At.Y) - High - 8.0f, 8.0f), 0.0f, 0.0f);
		}

		/** A unit's honours as a row of small two-coloured ribbons. */
		TSharedRef<SWidget> MakeRibbons(const TArray<FHonourRibbon>& Honours, int32 Most)
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 i = 0; i < Honours.Num() && i < Most; ++i)
			{
				Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(8.0f).ToolTipText(FText::FromString(Honours[i].Title))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(Honours[i].First))]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(Honours[i].Second))]
					]
				];
			}
			return Row;
		}

		TWeakObjectPtr<ACampaignPlayerController> Controller;
		TSharedPtr<SHorizontalBox> Cards;
		int32 ShownRevision = -1;
		FSlateBrush PanelBrush;
		FSlateBrush FirstDoorLit, FirstDoorPlain; // (rules 84.3.3) FIRST COMMAND's button on the door, lit and plain
		FSlateBrush PaperBrush;
		FSlateBrush BindingBrush;
		FSlateBrush StripBrush;
		FSlateBrush BezelBrush;
		FSlateBrush BezelInner;
		FSlateBrush BadgeBrush;
		FSlateBrush PillShine;
		FButtonStyle DoorStyle;
		FButtonStyle PillStyle;
		FButtonStyle RoundStyle, BrassStyle; // the raid card's round plus and minus, and its one brass button
		FSlateBrush PrintBrush; // the cream border of the target's picture
		FButtonStyle EndDarkStyle, EndLitStyle; // (ruling 88) END THE MONTH while armies wait, and when none do
		FSlateBrush NoBrush, KeyCapBrush, CountDiscBrush, QuietChipBrush;
		FSlateBrush SlipBrush, SlipBarBrush; // (ruling 87) the slip on the army's card
		FButtonStyle SlipButton;
		FButtonStyle NoButton; // (ruling 101) a button that is only what it holds: the quiet chip
		FButtonStyle AlarmStyle; // (rules 84.2.6, sheet 35) the alarm's red row: a block on #1d0f0c with a 1px edge in the alarm's red
		FSlateBrush AlarmEdgeBrush, AlarmCountBrush; // and its 5px left edge, and the small box round its "1 of 3"
		FSlateBrush AlarmGroundBrush; // (rules 84.2.6) the same block, for a row that is pressed through a plain button
		FSlateBrush ApproachBoxBrush, ApproachLitBrush, ApproachEdgeBrush, ApproachHairBrush, PickTagBrush; // (rules 84.5, sheet 34) the march-up card's rows
		FSlateBrush RadioRingBrush, RadioDotBrush; // and the radio mark on each row
		FSlateBrush RearChipBrush, RearRingBrush; // (ruling 101) NEXT IN THE REAR: dim leather, and the hollow ring round its number
		FTextBlockStyle TipPlain; // (rulings 86 and 87) the pointer's box, and the marks inside it
		TSharedPtr<FSlateStyleSet> TipStyles;
		FTextBlockStyle CardPlain; // a question card's lines, and the marks inside them: a section's head, an amber line, a red one
		TSharedPtr<FSlateStyleSet> CardStyles;
		FSlateBrush MonthBoxBrush, MonthBandBrush, MonthStampBrush, MonthDiscBrush, MonthTagBrush; // the month's card
		FSlateBrush MonthNewsBrush, MonthLabelBrush;
		TMap<FString, TSharedPtr<FSlateBrush>> MonthPictures; // its pictures, as they are read
		bool bMonthRowsOpen = false; // its rows past the seventh unfolded
		FString ShownMonthDate; // the month it was last laid out for
		TMap<FString, TSharedPtr<FSlateBrush>> TargetPictures; // the art director's pictures of the kinds of target, as they are read
		TMap<FString, TSharedPtr<FSlateBrush>> CutOuts; // the machines' small pictures with their ground taken away
		TSharedPtr<SWidget> HelpMark;
		TArray<TSharedRef<SWidget>> FramePieces;
		FButtonStyle PromptStyle;
		FSlateBrush ChipBrush;
		FSlateBrush BarBrush;
		FSlateBrush DotBrush;
		FSlateBrush CardBrush;
		FButtonStyle WayChosenStyle, WayQuietStyle;
		FSlateBrush WayRingBrush, WayTagBrush;
		FSlateBrush PipBrush, PipRingBrush;
		FButtonStyle PickedCard;
		FButtonStyle LeftCard;
		FButtonStyle EndMonthStyle;
		FButtonStyle PlainButton;
		FSlateBrush ScrimBrush;
		FSlateBrush DeepScrimBrush;
		FSlateBrush FilmBrush;
		FSlateBrush ClearBrush;
		TSharedPtr<SVerticalBox> Starts;
		// (The UI director, on build 1596.) The opening card's list as it scrolls: its rows of cards and how many cards
		// each holds, and, as looked at each frame, whether it runs on below what shows, how many cards are not wholly
		// in sight below, and how far one row of them is.
		TSharedPtr<SScrollBox> StartsScroll;
		// (03:53) The opening card, and the height it needs to hold whole all that is ruled to stay (0 until it is known).
		TSharedPtr<SBox> OpeningCardBox;
		float OpeningNeed = 0.0f;
		int32 OpeningSettle = 0; // (frames after a laying out in which what is measured is taken as it is)
		double OpeningHigh = 0.0; // (the screen's height the card was last measured for)
		TArray<TPair<TWeakPtr<SWidget>, int32>> ListRows;
		bool bListRunsOn = false;
		int32 ListBelow = 0;
		// (The UI director's review of build 1628, items 4 and 5.) The list's whole area, the height its view stands at (the
		// whole rows that fit; under 0 until it is first measured, when it takes the whole area), how far it is scrolled,
		// each line's top and foot inside it, and the plate's own band under the last whole row.
		TSharedPtr<SBox> ListArea;
		float ListViewHigh = -1.0f;
		float ListViewTop = 0.0f;
		TArray<float> ListTops, ListFeet;
		TArray<float> ListStops; // (where the list may rest: see LookAtList)
		TArray<bool> ListHeadAt; // (which of the list's lines are headings)
		TArray<int32> ListCountAt; // (how many cards each of the list's lines holds)
		double ListAreaHigh = 0.0; // (the list's area, as last measured)
		TArray<TWeakPtr<SWidget>> ListHeads; // (the headings, as laid out)
		float ListLastStop = 0.0f; // (the lowest the list rests: from there the rest fits whole)
		float ListRest = 0.0f; // (the line's top the list rests on, as the wheel last left it)
		float ListOwed = 0.0f; // (what the wheel has moved and the list not yet: see SnapList)
		double ListWheelAt = 0.0; // (when the wheel last moved it)
		int32 ListTab = INDEX_NONE; // (the page the list was last laid out for)
		static constexpr float PlateBand = 28.0f; // (the plate, about 21 high, with 3 or 4 above and below it)
		TSharedPtr<SHorizontalBox> OpeningTabs;
		TSharedPtr<SBox> OpeningFoot;
		TSharedPtr<SOverlay> FootSaid; // (item 4: every nation's lines, one over another, the picked one's shown)
		float FootWide = 0.0f; // (and the width they fold at, as last laid out)
		TMap<FString, float> OpeningFolds; // (each folding line's width as last drawn, by its place on the card: see Folding)
		FSlateBrush FootBrush;
		FSlateBrush WatchBrush;
		FButtonStyle ChosenCard;
		FButtonStyle PlateButton;
		TSharedPtr<SHorizontalBox> ArsenalColumns;
		int32 ShownArsenal = -1;
		TMap<FString, TSharedPtr<FSlateBrush>> PortraitBrushes;
		TMap<FString, TSharedPtr<FSlateBrush>> LargePortraitBrushes; // (for the bigger unit card)
		TSharedPtr<SBox> BigCardShow;
		TSharedPtr<SBorder> HoverTipBox;
		struct FBigCardTarget
		{
			TWeakPtr<SWidget> Widget;
			int32 CorpsId = INDEX_NONE;
			int32 Part = INDEX_NONE; // which of its divisions, or none for the corps' own card
		};
		TArray<FBigCardTarget> BigCardTargets;
		int32 BigCardCorps = INDEX_NONE, BigCardPart = INDEX_NONE, BigCardRevision = -1;
		TWeakPtr<SWidget> TrayPiece; // the army's tray: the bigger card stands on its top edge
		FSlateBrush BigCardBrush; // the bigger card's backing: leather that lets nothing through
		TSharedPtr<FSlateBrush> ShowBrush;
		TSharedPtr<SVerticalBox> PageBody;
		TSharedPtr<SVerticalBox> PageFoot;
		int32 ShownRoom = -1;
		TSharedPtr<SVerticalBox> GeneralsList;
		TSharedPtr<SVerticalBox> GeneralsVacant;
		int32 ShownGenerals = -1;
		TSharedPtr<SVerticalBox> ResearchFields;
		TSharedPtr<SVerticalBox> ResearchOthers;
		int32 ShownResearch = -1;
		TSharedPtr<SVerticalBox> DiplomacyPowers;
		TSharedPtr<SVerticalBox> DiplomacyHead;
		TSharedPtr<SVerticalBox> DiplomacyRows;
		int32 ShownDiplomacy = -1;
		TSharedPtr<SVerticalBox> LedgerIn;
		TSharedPtr<SVerticalBox> LedgerOut;
		TSharedPtr<SVerticalBox> LedgerProvinces;
		int32 ShownLedger = -1;
		TMap<FString, TSharedPtr<FSlateBrush>> FlagBrushes;
		int32 ShownOpening = -1;
		TSharedPtr<SVerticalBox> ProvinceBox;
		TSharedPtr<SHorizontalBox> TrayOrders;
		TSharedPtr<SHorizontalBox> TrayRail;
		TSharedPtr<SVerticalBox> TownBarBox;
		int32 ShownTownBar = -1;
		int32 ShownProvince = -1;
		TSharedPtr<SVerticalBox> AdviserSteps;
		int32 ShownAdviser = -1;
		TSharedPtr<SHorizontalBox> WatchStrip;
		TWeakPtr<SWidget> TopLeftPiece; // the calendar plate, and
		TArray<TWeakPtr<SWidget>> TopChips; // the chips down the right: the news line keeps between them
		// (Rules 84.3.3) FIRST COMMAND: where the staff's note stands (see PlaceLessonNote), and what it is measured by.
		TSharedPtr<SOverlay> ScreenRoot; // the frame's own measure, 900 high
		TWeakPtr<SWidget> SmallMapPiece; // the small map's panel: the second home is under it
		TSharedPtr<SBox> PageCardBox; // a room's card, which the slip stands beside
		TSharedPtr<SStaffNote> LessonNoteWidget;
		TSharedPtr<SStaffNote> ArsenalStrip; // (lesson 10 laid across the arsenal, and the room it keeps)
		float ArsenalStripHigh = 0.0f;
		FVector2D LessonAt = FVector2D::ZeroVector;
		bool bLessonShown = false, bLessonSlip = false, bLessonInLaid = false;
		float LessonSlipWide = 148.0f;
		int32 LessonHome = 0; // 0 under the date card, 1 under the small map, 2 the slip at the left edge (ruling 147(a))
		int32 LessonPlacedArrival = -1;
		TArray<TPair<FName, TWeakPtr<SWidget>>> AimPieces; // what the note's pointer may frame on the screen, by name (see Aimed)
		FBox2D PointerFrame = FBox2D(ForceInit); // what it frames this frame, in the frame's own measure
		TArray<FString> WatchShown;
		int32 WatchNow = INDEX_NONE;
		TSharedPtr<SVerticalBox> PageSections;
		TSharedPtr<SBox> PageRibbons;
		int32 ShownPage = -1;
	};
}

void ACampaignHUD::BeginPlay()
{
	Super::BeginPlay();
	if (GEngine && GEngine->GameViewport)
	{
		Overlay = SNew(SCampaignOverlay).Controller(Cast<ACampaignPlayerController>(GetOwningPlayerController()));
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
	}
}

void ACampaignHUD::ShowFrame(bool bShow)
{
	// (the frame stands as the viewport gave it, open to the pointer: a battle's screen lies over it meanwhile)
	if (Overlay.IsValid())
	{
		Overlay->SetVisibility(bShow ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

void ACampaignHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(EndPlayReason);
}
