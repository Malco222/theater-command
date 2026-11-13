#include "Campaign/CampaignHUD.h"

#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

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

	FSlateFontInfo Font(const TCHAR* Weight, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

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
			if (!Controller.IsValid())
			{
				return LayerId;
			}
			TArray<FMapLetter> Letters;
			Controller->GetMapLetters(Letters);
			if (Letters.Num() == 0)
			{
				return LayerId;
			}
			const float Scale = FMath::Max(Geometry.Scale, 0.01f);
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
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
					Type.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.013f, 0.01f, 0.55f * Each.Alpha);
				}
				const FString Glyph = bWhole ? Each.Whole : FString(1, &Each.Char);
				const FVector2D Box = Measure->Measure(Glyph, Type);
				// The letter's own right and down, as they lie on the screen: its type size is as long as a capital
				// is high divided by seven tenths.
				const double Unit = Size * 0.7;
				const FVector2D Right = Each.Along / Scale / Unit, Down = -Each.Up / Scale / Unit;
				const FSlateRenderTransform Lying(FMatrix2x2(static_cast<float>(Right.X), static_cast<float>(Right.Y), static_cast<float>(Down.X), static_cast<float>(Down.Y)));
				const FGeometry Place = Geometry.MakeChild(Box, FSlateLayoutTransform(Each.At / Scale - Box * 0.5), Lying, FVector2D(0.5, 0.5));
				const FLinearColor Ink = Each.bSea ? FLinearColor(FColor(214, 232, 236)).CopyWithNewOpacity(Each.Alpha) : FLinearColor(FColor(244, 232, 204)).CopyWithNewOpacity(Each.Alpha);
				FSlateDrawElement::MakeText(OutDrawElements, LayerId, Place.ToPaintGeometry(), Glyph, Type, ESlateDrawEffect::None, Ink);
			}
			return LayerId;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
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
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.0, 16.0); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			if (!Controller.IsValid())
			{
				return LayerId;
			}
			TArray<FTownPlate> Plates;
			Controller->GetTownPlates(Plates);
			if (Plates.Num() == 0)
			{
				return LayerId;
			}
			const float Scale = FMath::Max(Geometry.Scale, 0.01f);
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo CapitalFont = Serif(TEXT("Bold"), 12.0f, 20);
			const FSlateFontInfo TownFont = Serif(TEXT("Bold"), 10.0f, 10);
			const FSlateFontInfo LineFont = Font(TEXT("Regular"), 9);
			const FSlateFontInfo FarFont = Serif(TEXT("Bold"), 9.0f, 10);
			const FLinearColor NameInk(FColor::FromHex(TEXT("F2E8D5")));
			const FLinearColor LineInk(FColor::FromHex(TEXT("B9AC93")));
			TArray<FBox2D> Taken;
			for (const FTownPlate& Each : Plates)
			{
				// (from far off the capitals' plates are small, so the countries' own names can still be read)
				const FSlateFontInfo& NameFont = Each.bFar ? FarFont : Each.bCapital ? CapitalFont : TownFont;
				// (a capital's star is cut from the plain face: the serif has none)
				const bool bStar = Each.bCapital && !Each.bFar;
				const FString Shown = Each.Name;
				const FSlateFontInfo StarFont = Font(TEXT("Bold"), 10);
				const double StarWide = bStar ? Measure->Measure(TEXT("\u2605"), StarFont).X + 4.0 : 0.0;
				FVector2D NameSize = Measure->Measure(Shown, NameFont);
				NameSize.X += StarWide;
				const FVector2D LineSize = Each.Line.IsEmpty() ? FVector2D::ZeroVector : FVector2D(Measure->Measure(Each.Line, LineFont));
				const double Wide = FMath::Max(NameSize.X, LineSize.X) + 19.0;
				const double High = NameSize.Y + (Each.Line.IsEmpty() ? 0.0 : LineSize.Y - 1.0) + 5.0;
				const FVector2D Corner(Each.At.X / Scale - Wide * 0.5, Each.At.Y / Scale);
				const FBox2D Box(Corner - FVector2D(3.0, 2.0), Corner + FVector2D(Wide + 3.0, High + 2.0));
				bool bCrowded = false;
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
				const FSlateBrush& Back = Each.bCapital && !Each.bFar ? CapitalPlate : Plate;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2D(Wide, High), FSlateLayoutTransform(Corner)), &Back, ESlateDrawEffect::None, Back.GetTint(InWidgetStyle));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(4.0, High - 6.0), FSlateLayoutTransform(Corner + FVector2D(4.0, 3.0))), &Stripe, ESlateDrawEffect::None, Each.Colour.CopyWithNewOpacity(1.0f));
				if (bStar)
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(Corner + FVector2D(13.0, 3.0))), FString(TEXT("\u2605")), StarFont, ESlateDrawEffect::None, FLinearColor(FColor::FromHex(TEXT("E0B872"))));
				}
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(Corner + FVector2D(13.0 + StarWide, 2.0))), Shown, NameFont, ESlateDrawEffect::None, NameInk);
				if (!Each.Line.IsEmpty())
				{
					FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(LineSize, FSlateLayoutTransform(Corner + FVector2D(13.0, 1.0 + NameSize.Y))), Each.Line, LineFont, ESlateDrawEffect::None, LineInk);
				}
			}
			return LayerId + 1;
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
		FSlateBrush Plate;
		FSlateBrush CapitalPlate;
		FSlateBrush Stripe;
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
			Dot.DrawAs = ESlateBrushDrawType::Image;
			Dot.TintColor = FSlateColor(FLinearColor::White);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			double Share = 0.87;
			if (Controller.IsValid())
			{
				if (const UTexture2D* Texture = Controller->GetMinimapTexture())
				{
					Share = static_cast<double>(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
				}
			}
			return FVector2D(High * Share, High);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
			int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
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
			}
			const FVector2D Size = Geometry.GetLocalSize();
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), &Picture, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);

			TArray<FVector> Dots;
			FBox2D Looking(ForceInit);
			Controller->GetMinimapMarks(Dots, Looking);
			static const FLinearColor Inks[] = { FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), FLinearColor(0.45f, 0.8f, 1.0f, 1.0f), FLinearColor(1.0f, 0.2f, 0.12f, 1.0f), FLinearColor(0.1f, 0.1f, 0.1f, 0.85f) };
			for (const FVector& Mark : Dots)
			{
				const int32 Whose = FMath::Clamp(FMath::RoundToInt32(Mark.Z), 0, 3);
				const double Wide = Whose == 0 ? 5.0 : Whose == 3 ? 3.0 : 4.0;
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
				Controller->JumpToMapShare(FVector2D(Local.X / Size.X, Local.Y / Size.Y));
			}
		}

		TWeakObjectPtr<ACampaignPlayerController> Controller;
		float High = 200.0f;
		mutable FSlateBrush Picture;
		FSlateBrush Dot;
		bool bHeld = false;
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
			PanelBrush = FSlateRoundedBoxBrush(PanelFill, 6.0f, Brass.CopyWithNewOpacity(0.7f), 1.0f);
			ChipBrush = FSlateRoundedBoxBrush(FLinearColor::White, 3.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			BarBrush = FSlateRoundedBoxBrush(FLinearColor::White, 1.5f);
			CardBrush = FSlateRoundedBoxBrush(CardFill, 5.0f, Faint.CopyWithNewOpacity(0.55f), 1.0f);
			DotBrush = FSlateRoundedBoxBrush(FLinearColor::White, 6.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 1.0f);
			PickedCard = MakeButtonStyle(CardFill, CardHover, Brass, 1.5f);
			LeftCard = MakeButtonStyle(CardFill.CopyWithNewOpacity(0.7f), CardHover, Faint.CopyWithNewOpacity(0.6f), 1.0f);
			EndMonthStyle = MakeButtonStyle(FLinearColor(0.10f, 0.075f, 0.04f, 0.97f), FLinearColor(0.17f, 0.125f, 0.065f, 0.98f), Brass, 1.5f);

			auto Region = [this]() -> const FRegionPanelInfo*
			{
				return Controller.IsValid() ? &Controller->GetRegionPanel() : nullptr;
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
			PlateButton = MakeButtonStyle(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.2f, 0.16f, 0.09f, 0.6f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 1.0f);
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
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 22)).ColorAndOpacity(Cream)
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurGeneral : F->TheirGeneral) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted)
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurStrength : F->TheirStrength) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).Justification(Justify).LineHeightPercentage(1.25f).AutoWrapText(true)
						.Text_Lambda([Face, bOurs]() { const FFaceOff* F = Face(); return F ? (bOurs ? F->OurUnits : F->TheirUnits) : FText::GetEmpty(); })
					];
			};

			ChildSlot
			[
				SNew(SDPIScaler)
				.DPIScale_Lambda([]()
				{
					FVector2D Size(1920.0, 1080.0);
					if (GEngine && GEngine->GameViewport)
					{
						GEngine->GameViewport->GetViewportSize(Size);
					}
					return FMath::Clamp(static_cast<float>(Size.Y / 900.0), 0.7f, 3.5f);
				})
				[
					SNew(SOverlay)

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

					// The date plate.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(20.0f, 12.0f, 26.0f, 14.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetNewsreel().bOn ? EVisibility::Collapsed : EVisibility::Visible; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the plate is not the map
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("THEATER COMMAND"))).Font(Font(TEXT("Bold"), 10, 260)).ColorAndOpacity(Brass)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
								.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetDateText() : FText::GetEmpty(); })
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Muted)
								.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetRulesNote() : FText::GetEmpty(); })
							]
							// The weather, when it is not fair: mud in the east, or the eastern winter.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(296.0f)
								.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetWeatherText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(FLinearColor(FColor::FromHex(TEXT("C9B48A")))).AutoWrapText(true)
									.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWeatherText() : FText::GetEmpty(); })
								]
							]
							// The treasury, always in view: what is in hand, and what comes in, goes out and is left each
							// month. It is a button: pressing it opens the ledger.
							+ SVerticalBox::Slot().AutoHeight().Padding(-8.0f, 6.0f, 0.0f, -4.0f)
							[
								SNew(SButton).ButtonStyle(&PlateButton).ContentPadding(FMargin(8.0f, 4.0f)).IsFocusable(false)
								.ToolTipText(FText::FromString(TEXT("Open the ledger: where the money comes from, what the forces cost, and what every province gives.")))
								.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetTreasuryText().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
								.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenLedger(); } return FReply::Handled(); })
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 14))
										.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->IsInDebt() ? Alarm : BrightBrass); })
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetTreasuryText() : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetBudgetText() : FText::GetEmpty(); })
										.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetBudgetText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									]
								]
							]
							// Oil and supplies, for a power that keeps stocks of them: how full the depots are and how long they last.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								.Visibility_Lambda([this]() { return StoresShown() ? EVisibility::Visible : EVisibility::Collapsed; })
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)[MakeGauge(TEXT("OIL"), true)]
								+ SHorizontalBox::Slot().AutoWidth()[MakeGauge(TEXT("SUPPLIES"), false)]
							]
						]
					]

					// A tip that follows the pointer over the map: the province under it, in a few words.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
					.Padding(TAttribute<FMargin>::CreateLambda([this]()
					{
						FVector2D Size(1920.0, 1080.0);
						if (GEngine && GEngine->GameViewport)
						{
							GEngine->GameViewport->GetViewportSize(Size);
						}
						const float Scale = FMath::Clamp(static_cast<float>(Size.Y / 900.0), 0.7f, 3.5f);
						const FVector2D At = Controller.IsValid() ? Controller->GetPointer() / Scale : FVector2D::ZeroVector;
						// Kept on the screen: to the left of the pointer when it is near the right edge.
						const float Width = static_cast<float>(Size.X) / Scale;
						return FMargin(At.X > Width - 330.0f ? static_cast<float>(At.X) - 300.0f : static_cast<float>(At.X) + 18.0f, static_cast<float>(At.Y) + 20.0f, 0.0f, 0.0f);
					}))
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(10.0f, 6.0f))
						.Visibility_Lambda([this]() { return Controller.IsValid() && !Controller->GetHoverTip().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream).LineHeightPercentage(1.15f)
							.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetHoverTip() : FText::GetEmpty(); })
						]
					]

					// News for the player, for a few seconds at a time.
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24.0f, 84.0f, 24.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(22.0f, 10.0f))
						.Visibility_Lambda([this]()
						{
							return Controller.IsValid() && !Controller->GetNotice().IsEmpty() && !Controller->GetNewsreel().bOn ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						})
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 14)).ColorAndOpacity(Cream)
							.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetNotice() : FText::GetEmpty(); })
						]
					]

					// The adviser: the first steps of a campaign, each ticked as it is done. He can be sent away.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.0f, 218.0f, 0.0f, 0.0f)
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
										SNew(STextBlock).Text(FText::FromString(TEXT("YOUR CHIEF OF STAFF SUGGESTS"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(7.0f, 2.0f)).IsFocusable(false)
										.ToolTipText(FText::FromString(TEXT("Send him away. H opens the guide whenever you want it.")))
										.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->DismissAdviser(); } return FReply::Handled(); })
										[
											SNew(STextBlock).Text(FText::FromString(TEXT("DISMISS"))).Font(Font(TEXT("Bold"), 8, 140)).ColorAndOpacity(Muted)
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
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24.0f, 136.0f, 24.0f, 0.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(12.0f, 7.0f))
						.Visibility_Lambda([this]() { return WatchShown.Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SAssignNew(WatchStrip, SHorizontalBox)
						]
					]

					// The top right corner: the buttons, and under them the month's news for a while after the month turns.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.0f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetNewsreel().bOn ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 0.0f, 0.0f, 12.0f)
						[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("Order corps, aircraft, ships, factories and other works.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenArsenal(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("ARSENAL"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("Your generals: who commands what, who is waiting for a command, and the corps that have nobody.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenGenerals(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("GENERALS"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("The design offices: put design teams on the branches whose new weapons you want early.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenResearch(); } return FReply::Handled(); })
									[
										// Research teams never choose their own work: the button says when some stand idle.
										SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180))
										.ColorAndOpacity_Lambda([this]() { return FSlateColor(Controller.IsValid() && Controller->GetIdleTeams() > 0 ? BrightBrass : Muted); })
										.Text_Lambda([this]()
										{
											const int32 Idle = Controller.IsValid() ? Controller->GetIdleTeams() : 0;
											return FText::FromString(Idle > 0 ? FString::Printf(TEXT("RESEARCH  \u00B7  %d IDLE"), Idle) : FString(TEXT("RESEARCH")));
										})
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("The foreign office: who is at war, where every neutral stands, courting them, and declaring war.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenDiplomacy(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("DIPLOMACY"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenLedger(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("LEDGER"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenRoll(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("HONOURS"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("How to play: the table, a month, moving and attacking, and what every button is for.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenGuide(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("GUIDE"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(10.0f, 11.0f)).IsFocusable(false)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenMenu(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("MENU"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
								SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 11.0f)).IsFocusable(false)
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
									SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
									.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && Controller->IsWatching() ? TEXT("SKIP  (Space)") : TEXT("END THE MONTH")); })
								]
								]
						]
						// On the eve of war: the staff's plan for placing the army, carried out at a press.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->HasEvePlan() ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(FText::FromString(TEXT("The staff moves the army by rail to where it advises, strongest corps first. It does the most when it is the first thing done; you can still move any corps yourself afterwards.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->PlaceByPlan(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 120)).ColorAndOpacity(BrightBrass)
								.Text(FText::FromString(TEXT("PLACE THE ARMY AS THE STAFF ADVISES")))
							]
						]
						// What is still to do before the month is ended: armies that have not moved. A click goes to the next of them.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->ArmiesYetToMove() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(FText::FromString(TEXT("Armies that can still move this month. Click, or press Tab, to go to the next of them. You can end the month whenever you like.")))
							.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->NextArmy(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 120)).ColorAndOpacity(BrightBrass)
								.Text_Lambda([this]()
								{
									const int32 Count = Controller.IsValid() ? Controller->ArmiesYetToMove() : 0;
									return FText::FromString(FString::Printf(TEXT("%d ARM%s YET TO MOVE  \u00B7  TAB"), Count, Count == 1 ? TEXT("Y") : TEXT("IES")));
								})
							]
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
						// The small map: every country in its colour, the armies as marks, the part on screen boxed.
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 4.0f, 0.0f, 12.0f)
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(6.0f))
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsMapShown() ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(FText::FromString(TEXT("The whole map. White marks are your armies, blue your allies', red the enemy's. Click or drag to look somewhere else.")))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(SMinimap).Controller(Controller).High(236.0f)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
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
									MapButton(TEXT("MAP VIEWS"), TEXT("Steps through the views of the map, here and on the table: the map as it is; every country in flat colour; the wealth view, gold by what a province pays; and friend and foe, your own ground green, your allies' blue, your enemies' red. (Y)"), [this]() { Controller->ToggleWealth(); })
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
						SNew(SBox).WidthOverride(400.0f)
						.Visibility_Lambda([this]()
						{
							return Controller.IsValid() && !Controller->GetDispatches().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(20.0f, 14.0f, 20.0f, 16.0f))
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the news is not the map
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("DISPATCHES"))).Font(Font(TEXT("Bold"), 10, 260)).ColorAndOpacity(Brass)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true).LineHeightPercentage(1.15f)
									.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetDispatches() : FText::GetEmpty(); })
								]
							]
						]
					]
					]

					// Along the bottom: the territory panel and the army tray.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 16.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(390.0f)
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
										SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
										.Text_Lambda([Region]() { const FRegionPanelInfo* Info = Region(); return Info ? Info->Details : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("FORCES"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
							]
						]

						// The town of the province picked, when nothing is in hand: its building plots as pictures. A click on
						// a plot lays out above the bar what could go up on it, and a click on one of those orders it.
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(16.0f, 11.0f, 14.0f, 13.0f))
							.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsTownBarShown() ? EVisibility::Visible : EVisibility::Collapsed; })
							.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) // the bar is not the map
							[
								SAssignNew(TownBarBox, SVerticalBox)
							]
						]

						// The army in hand: its general, and a card for each corps. A click on a card leaves that corps behind.
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(18.0f, 12.0f, 10.0f, 14.0f))
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
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
								[
									SNew(SScrollBox).Orientation(Orient_Horizontal)
									+ SScrollBox::Slot()[SAssignNew(Cards, SHorizontalBox)]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 8.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted)
									.Text_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info ? Info->Hint : FText::GetEmpty(); })
								]
							]
						]

					]

					// The reminder of the controls. With an army in hand the tray says what to do, and takes the room.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(16.0f, 9.0f))
						.Visibility_Lambda([this, Tray]() { const FArmyTray* Info = Tray(); return (Info && Info->bVisible) || (Controller.IsValid() && Controller->IsTownBarShown()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Drag or W A S D: move the map    Scroll: zoom    Right-drag or Q / E: turn    C: your capital    Y: map views    T: day or night\nClick a flag, a fleet or an airfield; double-click a place to go there. Right-click where an army or a fleet should go    Tab: next army    Esc: put it down    Enter: end the month\n1 to 5: the screens    L: forces    R: report    O: goals    H: how to play")))
							.Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).Justification(ETextJustify::Right)
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
							SNew(SBox).WidthOverride(900.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(34.0f, 24.0f, 34.0f, 26.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Title : FText::GetEmpty(); })
									]

									// What the battle came to, once it has been fought.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
									[
										SNew(SVerticalBox)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && F->bResolved ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
										[
											SNew(STextBlock).Font(Font(TEXT("Bold"), 26)).ColorAndOpacity(BrightBrass)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ResultTitle : FText::GetEmpty(); })
										]
										+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(Cream)
											.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->ResultDetail : FText::GetEmpty(); })
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

									// The balance of strength between them.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
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
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 7.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->OddsLine : FText::GetEmpty(); })
									]
									// The prize: what taking the place is worth.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40.0f, 8.0f, 40.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(BrightBrass).AutoWrapText(true).Justification(ETextJustify::Center)
										.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved && !F->Prize.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Prize : FText::GetEmpty(); })
									]

									// The Battle Ticket: what the map says about this fight.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("THE BATTLE TICKET"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.3f)
										.Text_Lambda([Face]() { const FFaceOff* F = Face(); return F ? F->Ticket : FText::GetEmpty(); })
									]

									// What to do about it.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 24.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false).IsEnabled(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved ? EVisibility::Visible : EVisibility::Collapsed; })
											.ToolTipText(FText::FromString(TEXT("Fighting the battle yourself, in real time, comes with the battlefield.")))
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("FIGHT IT YOURSELF  (to come)"))).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(Faint)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([Face]() { const FFaceOff* F = Face(); return F && !F->bResolved ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmAttack(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("SEND THEM IN"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(22.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseFaceOff(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 11, 140)).ColorAndOpacity(Cream)
												.Text_Lambda([Face]() { const FFaceOff* F = Face(); return FText::FromString(F && F->bResolved ? TEXT("CONTINUE") : TEXT("CALL IT OFF")); })
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
												SNew(STextBlock).Text(FText::FromString(TEXT("CLOSE"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Cream)
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
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsPageOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1100.0f).HeightOverride(780.0f)
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
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetPage().Title) : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
												.Text_Lambda([this]() { return Controller.IsValid() ? FText::FromString(Controller->GetPage().Headline) : FText::GetEmpty(); })
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
											[
												SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
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
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)[SAssignNew(PageBody, SVerticalBox)]
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

					// The design offices: every branch of the armed forces, and the teams at work on it.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->IsResearchOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
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

					// The question before a war: asked when an army is sent onto the ground of a country at peace.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(640.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f, 30.0f, 24.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(SBox).WidthOverride(132.0f).HeightOverride(88.0f)
										[
											SNew(SImage).Image_Lambda([this]() { return FlagBrush(Controller.IsValid() ? Controller->GetWarPrompt().Tag : FString()); })
										]
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Title : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.2f)
										.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Body : FText::GetEmpty(); })
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 22.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseWarPrompt(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("NOT NOW"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ConfirmWar(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetWarPrompt().Confirm : FText::GetEmpty(); })
											]
										]
									]
								]
							]
						]
					]

					// The opening screen: where to begin and as whom.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&DeepScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(1080.0f).HeightOverride(820.0f)
							[
								SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(36.0f, 22.0f, 26.0f, 22.0f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("THEATER COMMAND"))).Font(Font(TEXT("Bold"), 30, 420)).ColorAndOpacity(Cream)
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("1939 TO 1945"))).Font(Font(TEXT("Bold"), 12, 520)).ColorAndOpacity(Brass)
									]

									// The campaign saved on this computer, and the way back to the one under way.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 12.0f, 10.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetOpening().bCanReturn ? EVisibility::Visible : EVisibility::Collapsed; })
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->CloseMenu(); } return FReply::Handled(); })
											[
												SNew(STextBlock).Text(FText::FromString(TEXT("BACK TO THE WAR"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
										[
											SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(24.0f, 10.0f)).IsFocusable(false)
											.Visibility_Lambda([this]()
											{
												return Controller.IsValid() && !Controller->GetOpening().bCanReturn && !Controller->GetOpening().SavedLine.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
											})
											.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->ContinueCampaign(); } return FReply::Handled(); })
											[
												SNew(SVerticalBox)
												+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
												[
													SNew(STextBlock).Text(FText::FromString(TEXT("CONTINUE YOUR CAMPAIGN"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
												]
												+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 0.0f)
												[
													SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Cream)
													.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetOpening().SavedLine : FText::GetEmpty(); })
												]
											]
										]
									]

									// The two pages: the grand campaign, and the historical scenarios.
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 14.0f, 10.0f, 0.0f)
									[
										SAssignNew(OpeningTabs, SHorizontalBox)
									]
									// What the page offers.
									+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
									[
										SNew(SScrollBox)
										+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
										[
											SAssignNew(Starts, SVerticalBox)
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
			];

			SetVisibility(EVisibility::SelfHitTestInvisible);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
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
						SNew(STextBlock).Text(FText::FromString(TEXT("DIPLOMACY"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
								SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(bOn ? BrightBrass : Muted)
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
							SNew(STextBlock).Text(Info.OptionsTitle).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
									SNew(STextBlock).Text(FText::FromString(Info.bPrizeToTake ? TEXT("THE PRIZE") : TEXT("WHAT IT PAYS"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
					if (ProvinceTab == 0 && bProvince && Info.Plots.Num() > 0)
					{
						// The town: its building plots as a row of lots, each with what stands on it, and the way to its plan.
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
								SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Info.bPlotsMine && Info.PlotsCanBuild > 0 ? BrightBrass : Muted)
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
											.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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
										SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Option.bEnabled ? Cream : Faint)
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Option.bEnabled ? Muted : Faint)
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
										.Text(FText::FromString(Option.bEnabled || Option.Why.IsEmpty() ? Option.Detail : Option.Why))
									]
								]
							];
						}
					}
					for (const FOrderLine& Order : Info.Forming)
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(BrightBrass).AutoWrapText(true)
								.Text(FText::FromString(FString::Printf(TEXT("On order: %s, due %s"), *Order.Name, *Order.Due)))
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
									SNew(STextBlock).Text(FText::FromString(TEXT("TAKE BACK"))).Font(Font(TEXT("Bold"), 8, 140)).ColorAndOpacity(Muted)
								]
							]
						];
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
				auto OrderButton = [this](const TCHAR* Label, bool bOn, TFunction<void()> Do)
				{
					return SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(11.0f, 4.0f)).IsFocusable(false)
						.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(bOn ? BrightBrass : Muted)
						];
				};
				if (Army.bFleet)
				{
					// A fleet in hand: what it does at sea, the way home, and a card for each ship.
					TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("At sea:"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
					];
					TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						SNew(SBox).ToolTipText(FText::FromString(TEXT("Seeks out the enemy's warships in its sea and guards the convoys that cross it.")))
						[
							OrderButton(TEXT("PATROL"), Army.FleetNext == TEXT("patrol"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("patrol")); } })
						]
					];
					TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Army.bFleetGrand ? 5.0f : 22.0f, 0.0f)
					[
						SNew(SBox).ToolTipText(FText::FromString(TEXT("Goes for merchant ships and keeps clear of a fight with the enemy fleet.")))
						[
							OrderButton(TEXT("RAID SHIPPING"), Army.FleetNext == TEXT("raid"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("raid")); } })
						]
					];
					if (Army.bFleetGrand)
					{
						TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)
						[
							SNew(SBox).ToolTipText(FText::FromString(TEXT("Its destroyers sweep the sea for submarines: the more destroyers, the more it sinks. The pointer over a sea says what hunting there would do.")))
							[
								OrderButton(TEXT("HUNT SUBMARINES"), Army.FleetNext == TEXT("hunt"), [this]() { if (Controller.IsValid()) { Controller->SetFleetStance(TEXT("hunt")); } })
							]
						];
					}
					TrayOrders->AddSlot().AutoWidth()
					[
						SNew(SBox).ToolTipText(FText::FromString(TEXT("Safe from the enemy fleet, and takes no part at sea.")))
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
								SNew(STextBlock).Text(FText::FromString(TEXT("ORDERS: ESCORT, SHELL, SPLIT"))).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(BrightBrass)
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
								SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(BrightBrass)
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
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					OrderButton(TEXT("STAND"), Army.Posture == TEXT("stand"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("stand")); } })
				];
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					OrderButton(TEXT("DIG IN"), Army.Posture == TEXT("dig"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("dig")); } })
				];
				TrayOrders->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)
				[
					OrderButton(TEXT("FALL BACK"), Army.Posture == TEXT("back"), [this]() { if (Controller.IsValid()) { Controller->SetStance(TEXT("back")); } })
				];
				TrayOrders->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Travel:"))).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
				];
				TrayOrders->AddSlot().AutoWidth()
				[
					OrderButton(TEXT("BY RAIL"), Army.bByRail, [this]() { if (Controller.IsValid()) { Controller->ToggleRail(); } })
				];
				TrayOrders->AddSlot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
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
							SNew(STextBlock).Text(Army.JoinButton).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(BrightBrass)
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
		/** One of the small buttons under the minimap. */
		TSharedRef<SWidget> MapButton(const TCHAR* Label, const TCHAR* Tip, TFunction<void()> Do)
		{
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(4.0f, 4.0f)).IsFocusable(false).HAlign(HAlign_Center)
				.ToolTipText(FText::FromString(Tip))
				.OnClicked_Lambda([this, Do]() { if (Controller.IsValid()) { Do(); } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(Muted)
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
				? FString(TEXT("No province of yours is selected, so works cannot be built: close this, click one of your territories, and open the arsenal again. Corps, aircraft and ships go where the notes say."))
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
			FString Terms = FString::Printf(TEXT("%s  \u00B7  %d month%s"), *Cash(Offer.Cost), Offer.Months, Offer.Months == 1 ? TEXT("") : TEXT("s"));
			if (Offer.Upkeep > 0)
			{
				Terms += FString::Printf(TEXT("  \u00B7  then %s a month"), *Cash(Offer.Upkeep));
			}
			TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Offer.Label)).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(bCan ? Cream : Faint).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Offer.Size.Len() <= 16 ? Offer.Size : FString())).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(bCan ? Muted : Faint)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Terms)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(bCan ? BrightBrass : (Offer.bPoor ? Alarm : Faint)).AutoWrapText(true)
				];
			// A long description of what is delivered goes on the line below instead of beside the name.
			const FString Where = Offer.Size.Len() > 16 ? Offer.Size.Left(1).ToUpper() + Offer.Size.Mid(1) + TEXT(". ") + Offer.Note : Offer.Note;
			if (!Where.IsEmpty())
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Where)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(bCan ? Muted : Faint).AutoWrapText(true)
				];
			}
			if (!bCan)
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Offer.Why)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Offer.bPoor ? Alarm : Muted).AutoWrapText(true)
				];
			}
			// A picture of what is on offer, where the game has a model of it: this nation's own tank, aircraft or ship.
			const FString Tag = Controller.IsValid() ? Controller->GetPlayerTag() : FString();
			FString Model;
			if (Offer.Key.StartsWith(TEXT("corps:"))) { Model = ACampaignMapActor::ModelFor(Tag, Offer.Key.Mid(6)); }
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
					SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(2.0f))
					[
						SNew(SPortrait).Picture(Portrait).Size(FVector2D(78.0, 48.0))
					]
				];
			}
			Face->AddSlot().FillWidth(1.0f)[Words];
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(9.0f, 7.0f, 11.0f, 8.0f)).IsFocusable(false).IsEnabled(bCan)
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
						List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[MakeOffer(Offer)];
						++Count;
					}
				}
				if (Count == 0)
				{
					List->AddSlot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
						.Text(FText::FromString(Column == 3 && Stock.Region.IsEmpty() ? TEXT("Select one of your own territories first.") : TEXT("Nothing on offer here in this campaign.")))
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
					OfficeButton(*Button.Label, Button.bDanger ? Alarm : BrightBrass, Button.bOk, Button.Tip, [this, Act = Button.Act]() { if (Controller.IsValid()) { Controller->PressPage(Act); } })
				];
			}
			return Box;
		}

		/** A room of the rules' own: its buttons, then each section with its words and its rows. */
		void LayOutPage(const FOfficePage& Page)
		{
			PageBody->ClearChildren();
			if (Page.Top.Num() > 0)
			{
				PageBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)[PageButtons(Page.Top)];
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
					for (const FString& Line : Entry.Lines)
					{
						Words->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Line)).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
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
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  LEVEL %d"), *General.Rank.ToUpper(), General.Level))).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(Brass)
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
						SNew(STextBlock).Text(FText::FromString(Field.Pace.ToUpper())).Font(Font(TEXT("Bold"), 9, 140)).ColorAndOpacity(PaceInk)
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
								SNew(STextBlock).Text(FText::FromString(Labs.Eras.IsValidIndex(Tier) ? Labs.Eras[Tier].ToUpper() : FString())).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Faint)
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
								SNew(STextBlock).Text(FText::FromString(Option.bNow ? TEXT("YOUR PATH TODAY") : TEXT(""))).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Gain)
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
							OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean: the whole League stands by it. Nothing happens until you confirm."), *Member.Name),
								[this, Tag = Member.Tag]() { if (Controller.IsValid()) { Controller->AskWarOn(Tag); } })
						];
					}
					DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Row];
				}
				DiplomacyPowers->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(10.0f)];
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
						OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean. Nothing happens until you confirm."), *Power.Name),
							[this, Tag = Power.Tag]() { if (Controller.IsValid()) { Controller->AskWarOn(Tag); } })
					];
				}
				DiplomacyPowers->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[Row];
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
				+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)[SNew(STextBlock).Text(FText::FromString(TEXT("ALLIES"))).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Mine < 0.0f ? Gain : Mine > 0.0f ? Alarm : Muted)]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("INDEPENDENT"))).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Faint)]
				+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)[SNew(STextBlock).Text(FText::FromString(TEXT("AXIS"))).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(Mine > 0.0f ? Gain : Mine < 0.0f ? Alarm : Muted)]
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
						OfficeButton(TEXT("WAR"), Alarm, true, FString::Printf(TEXT("See what declaring war on %s would mean. Nothing happens until you confirm."), *Entry.Name),
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
					SNew(STextBlock).Text(FText::FromString(TEXT("Nothing is on order. New corps and works are ordered from a territory's panel."))).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
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
					SNew(STextBlock).Text(FText::FromString(Heading)).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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
										SNew(STextBlock).Text(FText::FromString(Offer.Label)).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(bCan ? Cream : Muted).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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
									SNew(STextBlock).Font(Font(TEXT("Bold"), 10)).ColorAndOpacity(bFree ? Muted : Cream).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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

		/** One of the chips under the END THE MONTH button that say what is still worth doing; a click opens the screen for it. */
		TSharedRef<SWidget> MakeToDo(int32 Index)
		{
			return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
				.Visibility_Lambda([this, Index]() { return Controller.IsValid() && !Controller->GetToDoText(Index).IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 5.0f)).IsFocusable(false)
					.ToolTipText_Lambda([this, Index]() { return FText::FromString(Controller.IsValid() ? Controller->GetToDoTip(Index) : FString()); })
					.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PressToDo(Index); } return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Font(TEXT("Bold"), 9, 120)).ColorAndOpacity(Brass)
						.Text_Lambda([this, Index]() { return FText::FromString(Controller.IsValid() ? Controller->GetToDoText(Index) : FString()); })
					]
				];
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
			}
		}

		bool StoresShown() const
		{
			return Controller.IsValid() && Controller->GetStock(true) != nullptr;
		}

		const FViewStock* StockOf(bool bOil) const
		{
			return Controller.IsValid() ? Controller->GetStock(bOil) : nullptr;
		}

		/** A small gauge on the date plate: a stock's name, how full its depots are, and how long it lasts. */
		TSharedRef<SWidget> MakeGauge(const TCHAR* Label, bool bOil)
		{
			auto Low = [this, bOil]()
			{
				const FViewStock* Stock = StockOf(bOil);
				return Stock && (Stock->bShort || (Stock->Months >= 0 && Stock->Months <= 6));
			};
			auto Share = [this, bOil]()
			{
				const FViewStock* Stock = StockOf(bOil);
				return Stock && Stock->Cap > 0.0f ? FMath::Clamp(Stock->Stock / Stock->Cap, 0.0f, 1.0f) : 0.0f;
			};
			return SNew(SHorizontalBox)
				.ToolTipText_Lambda([this, bOil]() { const FViewStock* Stock = StockOf(bOil); return Stock ? FText::FromString(Stock->Tip) : FText::GetEmpty(); })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 9, 160))
					.ColorAndOpacity_Lambda([Low]() { return FSlateColor(Low() ? Alarm : Brass); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(40.0f).HeightOverride(6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Share]() { return FMath::Max(Share(), 0.0001f); }))
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor_Lambda([Low]() { return FSlateColor(Low() ? Alarm : BrightBrass); })
						]
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Share]() { return FMath::Max(1.0f - Share(), 0.0001f); }))
						[
							SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f))
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(TEXT("Regular"), 10))
					.ColorAndOpacity_Lambda([Low]() { return FSlateColor(Low() ? Alarm : Muted); })
					.Text_Lambda([this, bOil]()
					{
						const FViewStock* Stock = StockOf(bOil);
						if (!Stock)
						{
							return FText::GetEmpty();
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

		/** One country on the grand campaign's page. The great powers get a card with a word about them; the rest a line. */
		TSharedRef<SWidget> MakeNation(const FOpeningNation& Nation, int32 Index, bool bChosen)
		{
			TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Nation.Name).Font(Font(TEXT("Bold"), Nation.bMajor ? 14 : 12)).ColorAndOpacity(Cream)
				];
			if (Nation.bMajor)
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Nation.Standing).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Brass).AutoWrapText(true)
				];
				Words->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Nation.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f)
				];
				Words->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Nation.Strength).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Faint)
				];
			}
			else
			{
				Words->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s"), *Nation.Standing.ToString(), *Nation.Strength.ToString())))
					.Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(Muted).AutoWrapText(true)
				];
				if (Nation.bSmall && !Nation.Blurb.IsEmpty())
				{
					Words->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Nation.Blurb).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.1f)
					];
				}
			}
			return SNew(SButton).ButtonStyle(bChosen ? &ChosenCard : &PlainButton).IsFocusable(false)
				.ContentPadding(Nation.bMajor ? FMargin(13.0f, 11.0f, 13.0f, 12.0f) : FMargin(10.0f, 7.0f, 10.0f, 8.0f))
				.OnClicked_Lambda([this, Index]()
				{
					if (Controller.IsValid())
					{
						Controller->PickGrandNation(Index);
					}
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, Nation.bMajor ? 12.0f : 9.0f, 0.0f)
					[
						MakeFlag(Nation.Tag, Nation.bMajor ? 60.0f : 36.0f)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						Words
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
			OpeningFoot->SetContent(SNullWidget::NullWidget);
			OpeningFoot->SetVisibility(EVisibility::Collapsed);

			if (Opening.Tab == 2)
			{
				// ---- The options: the game's standing choices, each with what it does and a button that changes it.
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(TEXT("How the game looks, sounds and handles. What you choose here is remembered the next time you play.")))
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
									SNew(STextBlock).Text(Option.Name).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(Option.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f)
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
					Heading(TEXT("THE CONTROLS"))
				];
				const TCHAR* Controls[][2] =
				{
					{ TEXT("Move the view"), TEXT("drag the table, or W A S D or the arrow keys; double-click a place to centre on it") },
					{ TEXT("Zoom, turn and tilt"), TEXT("scroll, or + and -; drag with the right button, or Q and E") },
					{ TEXT("Armies and fleets"), TEXT("click a flag or a fleet to take it in hand; right-click where it should go; Esc puts it down; Tab goes to the next army yet to move") },
					{ TEXT("The month"), TEXT("Enter ends it; Space skips what is being shown") },
					{ TEXT("Screens"), TEXT("1 to 5 open the top bar's; L forces, R report, O goals, C your capital, Y map views, H how to play") },
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
							SNew(STextBlock).Text(FText::FromString(Line[1])).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
						]
					];
				}
				return;
			}

			if (Opening.Tab == 0 && Opening.Years.Num() > 0)
			{
				// ---- The grand campaign: a day to take over on, and a country.
				Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					.Text(FText::FromString(TEXT("Pick a country and the year you take it over. History is played up to that day. After it nothing is written: every nation makes its own decisions, and the war is yours to shape.")))
				];
				TSharedRef<SHorizontalBox> YearRow = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
					[
						Heading(TEXT("TAKE OVER IN"))
					];
				for (int32 Index = 0; Index < Opening.Years.Num(); ++Index)
				{
					const bool bOn = Index == Opening.YearIndex;
					YearRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(bOn ? &EndMonthStyle : &PlainButton).ContentPadding(FMargin(18.0f, 6.0f)).IsFocusable(false)
						.OnClicked_Lambda([this, Index]() { if (Controller.IsValid()) { Controller->PickGrandYear(Index); } return FReply::Handled(); })
						[
							SNew(STextBlock).Text(Opening.Years[Index].Year).Font(Font(TEXT("Bold"), 14, 60)).ColorAndOpacity(bOn ? BrightBrass : Muted)
						]
					];
				}
				Starts->AddSlot().AutoHeight()[YearRow];
				const FOpeningYear& Year = Opening.Years[FMath::Clamp(Opening.YearIndex, 0, Opening.Years.Num() - 1)];
				Starts->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(STextBlock).Text(Year.Title).Font(Font(TEXT("Bold"), 12)).ColorAndOpacity(Cream)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock).Text(Year.Note).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					]
				];

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
				Starts->AddSlot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 8.0f)[Heading(TEXT("THE GREAT POWERS"))];
				AddGrid(Great, 3);
				if (Small.Num() > 0)
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 2.0f)[Heading(TEXT("SMALL COUNTRIES, WITH A PATH OF THEIR OWN"))];
					Starts->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
						.Text(FText::FromString(TEXT("A small country chooses for itself: join the Axis, join the Allies, stand alone, or found a League of its own with a willing neighbour. Open DIPLOMACY in the game to choose.")))
					];
					AddGrid(Small, 4);
				}
				if (Rest.Num() > 0)
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 8.0f)[Heading(TEXT("OTHER COUNTRIES"))];
					AddGrid(Rest, 4);
				}
				if (!Opening.GrandNote.IsEmpty())
				{
					Starts->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock).Text(Opening.GrandNote).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Faint).AutoWrapText(true)
					];
				}

				// The foot of the page: the country chosen, and the word to begin.
				const bool bChosen = Opening.Nations.IsValidIndex(Opening.NationIndex);
				TSharedRef<SHorizontalBox> Foot = SNew(SHorizontalBox);
				if (bChosen)
				{
					const FOpeningNation& Nation = Opening.Nations[Opening.NationIndex];
					Foot->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
					[
						MakeFlag(Nation.Tag, 96.0f)
					];
					Foot->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(Nation.Name).Font(Font(TEXT("Regular"), 22)).ColorAndOpacity(Cream)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Brass).AutoWrapText(true)
							.Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s  ·  %s"), *Year.Title.ToString(), *Nation.Standing.ToString(), *Nation.Strength.ToString())))
						]
						// What the country does in its own way: its abilities and its special formation, by name.
						// What each one does is said when the mouse rests on the line, and in the game's own guide.
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true)
							.Visibility(Nation.Abilities.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed)
							.ToolTipText(FText::FromString(FString::Join(Nation.Abilities, TEXT("\n"))))
							.Text(FText::FromString(AbilityNames(Nation.Abilities)))
						]
					];
				}
				else
				{
					Foot->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Choose a country above to lead it."))).Font(Font(TEXT("Italic"), 13)).ColorAndOpacity(Muted)
					];
				}
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
								SNew(STextBlock).Text(FText::FromString(Level.Name.ToUpper())).Font(Font(TEXT("Bold"), 9, 120)).ColorAndOpacity(bPicked ? BrightBrass : Muted)
							]
						];
					}
					const FString Says = Opening.Levels.IsValidIndex(Opening.LevelIndex) ? Opening.Levels[Opening.LevelIndex].Blurb : FString();
					Difficulty->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("DIFFICULTY"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
					];
					Difficulty->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[
						Ranks
					];
					Difficulty->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(FText::FromString(Says)).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
				}
				Foot->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(14.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(16.0f, 9.0f)).IsFocusable(false)
					.ToolTipText(FText::FromString(TEXT("Nobody leads: every nation decides for itself, and you watch the war play out.")))
					.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->BeginGrandCampaign(true); } return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("JUST WATCH"))).Font(Font(TEXT("Bold"), 11, 160)).ColorAndOpacity(Muted)
					]
				];
				if (bChosen)
				{
					Foot->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).ButtonStyle(&EndMonthStyle).ContentPadding(FMargin(26.0f, 12.0f)).IsFocusable(false)
						.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->BeginGrandCampaign(false); } return FReply::Handled(); })
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 14, 200)).ColorAndOpacity(BrightBrass)
							.Text(FText::FromString(FString::Printf(TEXT("BEGIN AS %s"), *Opening.Nations[Opening.NationIndex].Name.ToString().ToUpper())))
						]
					];
				}
				OpeningFoot->SetContent(SNew(SBorder).BorderImage(&FootBrush).Padding(FMargin(16.0f, 10.0f, 16.0f, 12.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						Difficulty
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
				SNew(STextBlock).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
				.Text(FText::FromString(TEXT("Single campaigns from the war as it was fought. You lead one side; the other nations keep to history's plan. Or watch one play out.")))
			];
			for (int32 GroupIndex = 0; GroupIndex < Opening.Groups.Num(); ++GroupIndex)
			{
				Starts->AddSlot().AutoHeight().Padding(0.0f, GroupIndex == 0 ? 0.0f : 14.0f, 0.0f, 8.0f)
				[
					Heading(Opening.Groups[GroupIndex].Title.ToString().ToUpper())
				];
				TArray<TSharedRef<SWidget>> Cells;
				for (int32 SideIndex = 0; SideIndex < Opening.Groups[GroupIndex].Sides.Num(); ++SideIndex)
				{
					Cells.Add(MakeStart(Opening.Groups[GroupIndex].Sides[SideIndex], GroupIndex, SideIndex));
				}
				AddGrid(Cells, 2);
			}
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
					SNew(STextBlock).Text(Side.Blurb).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(Muted).AutoWrapText(true).LineHeightPercentage(1.15f)
				];
			if (!Side.Aims.IsEmpty())
			{
				Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Side.Aims).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(Brass).AutoWrapText(true).LineHeightPercentage(1.15f)
				];
			}
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(14.0f, 10.0f, 14.0f, 12.0f)).IsFocusable(false)
				.OnClicked_Lambda([this, GroupIndex, SideIndex]()
				{
					if (Controller.IsValid())
					{
						Controller->BeginCampaign(GroupIndex, SideIndex);
					}
					return FReply::Handled();
				})
				[
					Body
				];
		}

		/** The studio's picture of a model, as something to draw; null if there is no such model. Kept once made. */
		TSharedPtr<FSlateBrush> PortraitOf(const FString& Model)
		{
			if (const TSharedPtr<FSlateBrush>* Kept = PortraitBrushes.Find(Model))
			{
				return *Kept;
			}
			TSharedPtr<FSlateBrush> Made;
			ACampaignMapActor* Table = Controller.IsValid() ? Controller->GetMap() : nullptr;
			if (UTextureRenderTarget2D* Picture = Table ? Table->GetPortrait(Model) : nullptr)
			{
				Made = MakeShared<FSlateBrush>();
				Made->SetResourceObject(Picture);
				Made->ImageSize = FVector2D(184.0, 112.0);
				Made->DrawAs = ESlateBrushDrawType::Image;
			}
			PortraitBrushes.Add(Model, Made);
			return Made;
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
				: !Card.bCanOrder ? FString(TEXT("holds its ground"))
				: (!Card.bPicked ? FString(TEXT("staying behind"))
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
						SNew(SBox).WidthOverride(74.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Part->Name)).Font(Font(TEXT("Bold"), 8)).ColorAndOpacity(bGoing ? Cream : Faint).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
					];
				}
				if (!Card.bUnknown)
				{
					Face->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Meter(Left, bGoing ? (Left < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("8FBF6A")))) : Faint, Card.Stale > 0 ? TEXT("Strength, about") : TEXT("Strength"))];
					if (!Card.bMood && !Part && Card.Stale == 0)
					{
						Face->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[Meter(Card.Morale, bGoing ? MoraleInk : Faint, TEXT("Morale"))];
					}
					Face->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[Stripes];
				}
				Divisions->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f)).ToolTipText(FText::FromString(PartTip))
					[
						Face
					]
				];
			}

			// Morale and rest belong to the whole corps: a bar each with its word, and which way morale is going.
			auto MoodBar = [this, bGoing](const TCHAR* Label, const FString& Word, float Share, const FLinearColor& Colour, const FString& Arrow)
			{
				const float Left = FMath::Clamp(Share, 0.0f, 1.0f);
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(52.0f)[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(TEXT("Bold"), 8, 140)).ColorAndOpacity(bGoing ? Brass : Faint)]
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

			return SNew(SButton).ButtonStyle(bGoing ? &PickedCard : &LeftCard).ContentPadding(FMargin(8.0f, 6.0f, 4.0f, 7.0f))
				.IsFocusable(false).IsEnabled(Card.bCanOrder || Card.bLook).ToolTipText(FText::FromString(Tip))
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
								SNew(STextBlock).Text(FText::FromString(TEXT("SEND BACK"))).Font(Font(TEXT("Bold"), 8, 140)).ColorAndOpacity(BrightBrass)
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
								SNew(STextBlock).Text(FText::FromString(TEXT("RECORD"))).Font(Font(TEXT("Bold"), 8, 140)).ColorAndOpacity(Muted)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 4.0f, 5.0f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bGoing ? BrightBrass : Faint)
						.Text(FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s  \u00B7  %s"), *Card.Kind.ToString(), *Card.Grade.ToString(), *MovesLine)))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 4.0f, Card.bMood ? 6.0f : 0.0f)[Mood]
					+ SVerticalBox::Slot().AutoHeight()[Divisions]
				];
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
		FSlateBrush ChipBrush;
		FSlateBrush BarBrush;
		FSlateBrush DotBrush;
		FSlateBrush CardBrush;
		FButtonStyle PickedCard;
		FButtonStyle LeftCard;
		FButtonStyle EndMonthStyle;
		FButtonStyle PlainButton;
		FSlateBrush ScrimBrush;
		FSlateBrush DeepScrimBrush;
		FSlateBrush FilmBrush;
		FSlateBrush ClearBrush;
		TSharedPtr<SVerticalBox> Starts;
		TSharedPtr<SHorizontalBox> OpeningTabs;
		TSharedPtr<SBox> OpeningFoot;
		FSlateBrush FootBrush;
		FSlateBrush WatchBrush;
		FButtonStyle ChosenCard;
		FButtonStyle PlateButton;
		TSharedPtr<SHorizontalBox> ArsenalColumns;
		int32 ShownArsenal = -1;
		TMap<FString, TSharedPtr<FSlateBrush>> PortraitBrushes;
		TSharedPtr<FSlateBrush> ShowBrush;
		TSharedPtr<SVerticalBox> PageBody;
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
		TSharedPtr<SVerticalBox> TownBarBox;
		int32 ShownTownBar = -1;
		int32 ShownProvince = -1;
		TSharedPtr<SVerticalBox> AdviserSteps;
		int32 ShownAdviser = -1;
		TSharedPtr<SHorizontalBox> WatchStrip;
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

void ACampaignHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(EndPlayReason);
}
