#include "Campaign/CampaignHUD.h"

#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"
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

					// The date plate.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(20.0f, 12.0f, 26.0f, 14.0f))
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
							return Controller.IsValid() && !Controller->GetNotice().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						})
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 14)).ColorAndOpacity(Cream)
							.Text_Lambda([this]() { return Controller.IsValid() ? Controller->GetNotice() : FText::GetEmpty(); })
						]
					]

					// The top right corner: the buttons, and under them the month's news for a while after the month turns.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.0f)
					[
						SNew(SVerticalBox)
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
										SNew(SBox).MaxDesiredHeight(455.0f)
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
						.Visibility_Lambda([Tray]() { const FArmyTray* Info = Tray(); return Info && Info->bVisible ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Drag: move the map    Scroll: zoom    Right-drag or Q / E: turn\nClick a flag: pick up the army. Click where it should go, twice, or drag the flag there    Tab: next army    Esc: put it down    Enter: end the month    1 to 5: the screens    H: how to play")))
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
					ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Info.OptionsTitle).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
					];
					ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Info.Holdings).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
					];
					if (!Info.ProvinceNote.IsEmpty())
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(Info.ProvinceNote).Font(Font(TEXT("Italic"), 11)).ColorAndOpacity(Faint).AutoWrapText(true)
						];
					}
					for (const FProvinceOption& Option : Info.Options)
					{
						ProvinceBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(12.0f, 6.0f)).IsFocusable(false).IsEnabled(Option.bEnabled)
							.ToolTipText(FText::FromString(Option.Why))
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
									SNew(STextBlock).Text(FText::FromString(Option.Label)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Option.bEnabled ? Cream : Faint)
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Option.bEnabled ? Muted : Faint).AutoWrapText(true)
									.Text(FText::FromString(Option.bEnabled || Option.Why.IsEmpty() ? Option.Detail : Option.Detail + TEXT("\n") + Option.Why))
								]
							]
						];
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
					OrderButton(TEXT("PUT DOWN"), false, [this]() { if (Controller.IsValid()) { Controller->PutDownArmy(); } })
				];

				Cards->ClearChildren();
				for (const FTrayCard& Card : Controller->GetArmyTray().Cards)
				{
					Cards->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						MakeCard(Card)
					];
				}
			}
		}

	private:
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
					SNew(STextBlock).Text(FText::FromString(Terms)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(bCan ? BrightBrass : (Offer.bPoor ? Alarm : Faint))
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
			return SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(11.0f, 7.0f, 11.0f, 8.0f)).IsFocusable(false).IsEnabled(bCan)
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
					Words
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
			if (Opening.Years.Num() > 0)
			{
				const TCHAR* Names[] = { TEXT("THE GRAND CAMPAIGN"), TEXT("HISTORICAL SCENARIOS") };
				for (int32 Tab = 0; Tab < 2; ++Tab)
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
			const bool bGoing = Card.bPicked && Card.bCanOrder;
			const FLinearColor Ink = bGoing ? Cream : Muted;
			const FString MovesLine = !Card.bCanOrder ? FString(TEXT("holds its ground"))
				: (!Card.bPicked ? FString(TEXT("staying behind"))
					: FString::Printf(TEXT("moves %d of %d"), Card.Moves, Card.MovesMax));
			const FLinearColor StrengthInk = Card.Strength < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("8FBF6A")));
			const FLinearColor MoraleInk = Card.Morale < 0.4f ? Alarm : FLinearColor(FColor::FromHex(TEXT("6FA3D8")));
			const int32 Pips = FMath::Clamp(1 + Card.Experience / 22, 1, 5);
			const FString Tip = FString::Printf(TEXT("%s: %s, %d division%s. Strength %d%%, morale %d%%. %s. Click to tell this corps to come along or stay behind."),
				*Card.Name.ToString(), *Card.Kind.ToString(), Card.Divisions, Card.Divisions == 1 ? TEXT("") : TEXT("s"),
				FMath::RoundToInt32(Card.Strength * 100.0f), FMath::RoundToInt32(Card.Morale * 100.0f), *Card.Grade.ToString());

			TSharedRef<SHorizontalBox> Divisions = SNew(SHorizontalBox);
			for (int32 Division = 0; Division < FMath::Clamp(Card.Divisions, 1, 12); ++Division)
			{
				TSharedRef<SHorizontalBox> Stripes = SNew(SHorizontalBox);
				for (int32 Pip = 0; Pip < 5; ++Pip)
				{
					Stripes->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(6.0f).HeightOverride(3.0f)[SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(Pip < Pips ? BrightBrass : FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))]
					];
				}
				Divisions->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[KindPicture(Card.KindId, Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Meter(Card.Strength, bGoing ? StrengthInk : Faint, TEXT("Strength"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[Meter(Card.Morale, bGoing ? MoraleInk : Faint, TEXT("Morale"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[Stripes]
					]
				];
			}

			return SNew(SButton).ButtonStyle(bGoing ? &PickedCard : &LeftCard).ContentPadding(FMargin(8.0f, 6.0f, 4.0f, 7.0f))
				.IsFocusable(false).IsEnabled(Card.bCanOrder).ToolTipText(FText::FromString(Tip))
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
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Card.Nickname.IsEmpty() ? Card.Name : FText::FromString(FString::Printf(TEXT("%s \u201C%s\u201D"), *Card.Name.ToString(), *Card.Nickname.ToString()))).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							MakeRibbons(Card.Honours, 4)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
						// The way to the corps' service record: its honours and its diary.
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(6.0f, 1.0f)).IsFocusable(false)
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
		TSharedPtr<SVerticalBox> Starts;
		TSharedPtr<SHorizontalBox> OpeningTabs;
		TSharedPtr<SBox> OpeningFoot;
		FSlateBrush FootBrush;
		FSlateBrush WatchBrush;
		FButtonStyle ChosenCard;
		FButtonStyle PlateButton;
		TSharedPtr<SHorizontalBox> ArsenalColumns;
		int32 ShownArsenal = -1;
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
		int32 ShownProvince = -1;
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
