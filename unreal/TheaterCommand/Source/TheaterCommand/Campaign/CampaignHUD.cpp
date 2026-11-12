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
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 11.0f)).IsFocusable(false)
									.ToolTipText(FText::FromString(TEXT("Order corps, aircraft, ships, factories and other works.")))
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenArsenal(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("ARSENAL"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 11.0f)).IsFocusable(false)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenLedger(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("LEDGER"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 11.0f)).IsFocusable(false)
									.OnClicked_Lambda([this]() { if (Controller.IsValid()) { Controller->OpenRoll(); } return FReply::Handled(); })
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("HONOURS"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(Muted)
									]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(18.0f, 11.0f)).IsFocusable(false)
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
									+ SVerticalBox::Slot().AutoHeight()
									[
										SAssignNew(ProvinceBox, SVerticalBox)
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
									SAssignNew(Cards, SHorizontalBox)
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
							.Text(FText::FromString(TEXT("Drag: move the map    Scroll: zoom    Right-drag or Q / E: turn\nClick a flag: pick up the army. Click where it should go, twice, or drag the flag there    Esc: put it down    Enter: end the month    M: music")))
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

					// The question before a war: asked when an army is sent onto the ground of a country at peace.
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(&ScrimBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.Visibility_Lambda([this]() { return Controller.IsValid() && Controller->GetWarPrompt().bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
						[
							SNew(SBox).WidthOverride(560.0f)
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
												SNew(STextBlock).Text(FText::FromString(TEXT("DECLARE WAR"))).Font(Font(TEXT("Bold"), 12, 180)).ColorAndOpacity(BrightBrass)
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
						SNew(STextBlock).Text(FText::FromString(TEXT("PROVINCE"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
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

				TArray<TSharedRef<SWidget>> Great, Rest;
				for (int32 Index = 0; Index < Opening.Nations.Num(); ++Index)
				{
					(Opening.Nations[Index].bMajor ? Great : Rest).Add(MakeNation(Opening.Nations[Index], Index, Index == Opening.NationIndex));
				}
				Starts->AddSlot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 8.0f)[Heading(TEXT("THE GREAT POWERS"))];
				AddGrid(Great, 3);
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
					];
				}
				else
				{
					Foot->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Choose a country above to lead it."))).Font(Font(TEXT("Italic"), 13)).ColorAndOpacity(Muted)
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
				OpeningFoot->SetContent(SNew(SBorder).BorderImage(&FootBrush).Padding(FMargin(16.0f, 12.0f))[Foot]);
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

		TSharedRef<SWidget> MakeCard(const FTrayCard& Card)
		{
			const bool bGoing = Card.bPicked && Card.bCanOrder;
			const FLinearColor Ink = bGoing ? Cream : Muted;
			const FString MovesLine = !Card.bCanOrder ? FString(TEXT("Holds its ground"))
				: (!Card.bPicked ? FString(TEXT("Staying behind"))
					: FString::Printf(TEXT("Moves %d of %d"), Card.Moves, Card.MovesMax));
			const float Strength = FMath::Clamp(Card.Strength, 0.0f, 1.0f);

			TSharedRef<SWidget> CardBody = SNew(SBox).WidthOverride(172.0f)
				[
					SNew(SButton).ButtonStyle(bGoing ? &PickedCard : &LeftCard).ContentPadding(FMargin(11.0f, 8.0f, 11.0f, 9.0f))
					.IsFocusable(false).IsEnabled(Card.bCanOrder)
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
							SNew(STextBlock).Text(Card.Name).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(Ink)
						]
						// The name the army gives it, once it has earned one.
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(TEXT("Italic"), 10)).ColorAndOpacity(BrightBrass)
							.Text(FText::FromString(FString::Printf(TEXT("\u201C%s\u201D"), *Card.Nickname.ToString())))
							.Visibility(Card.Nickname.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
							.Text(FText::FromString(FString::Printf(TEXT("%s, %d division%s"), *Card.Kind.ToString(), Card.Divisions, Card.Divisions == 1 ? TEXT("") : TEXT("s"))))
						]
						// How seasoned it is, and a ribbon for each honour it carries.
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 7.0f, 0.0f)
							[
								SNew(STextBlock).Text(Card.Grade).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								MakeRibbons(Card.Honours, 5)
							]
						]
						// How much of the corps is left.
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)
						[
							SNew(SBox).HeightOverride(4.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(FMath::Max(Strength, 0.001f))
								[
									SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(bGoing ? BrightBrass : Faint)
								]
								+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.0f - Strength, 0.001f))
								[
									SNew(SBorder).BorderImage(&BarBrush).BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f))
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(MovesLine)).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(bGoing ? BrightBrass : Faint)
						]
					]
				];

			// The card, and under it the way to the corps' service record.
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					CardBody
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&PlainButton).ContentPadding(FMargin(8.0f, 3.0f)).IsFocusable(false).HAlign(HAlign_Center)
					.OnClicked_Lambda([this, Id = Card.CorpsId]()
					{
						if (Controller.IsValid())
						{
							Controller->OpenRecord(Id);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("SERVICE RECORD"))).Font(Font(TEXT("Bold"), 8, 160)).ColorAndOpacity(Muted)
					]
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
