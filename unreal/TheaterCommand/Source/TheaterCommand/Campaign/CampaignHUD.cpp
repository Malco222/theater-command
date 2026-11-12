#include "Campaign/CampaignHUD.h"

#include "Campaign/CampaignPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor PanelFill(0.012f, 0.010f, 0.008f, 0.86f);
	const FLinearColor Brass = FLinearColor(FColor::FromHex(TEXT("B08D57")));
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("EDE6D6")));
	const FLinearColor Muted = FLinearColor(FColor::FromHex(TEXT("A89F8D")));

	FSlateFontInfo Font(const TCHAR* Weight, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	/** The panels drawn over the campaign table. Nothing here takes the mouse: clicks pass through to the map. */
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

			auto Panel = [this]() -> const FRegionPanelInfo*
			{
				return Controller.IsValid() ? &Controller->GetRegionPanel() : nullptr;
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
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("THEATER COMMAND"))).Font(Font(TEXT("Bold"), 10, 260)).ColorAndOpacity(Brass)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("September 1939"))).Font(Font(TEXT("Regular"), 26)).ColorAndOpacity(Cream)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Europe on the eve of war"))).Font(Font(TEXT("Italic"), 12)).ColorAndOpacity(Muted)
							]
						]
					]

					// The territory panel.
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SBox).WidthOverride(430.0f)
						.Visibility_Lambda([Panel]()
						{
							const FRegionPanelInfo* Info = Panel();
							return Info && Info->bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						})
						[
							SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(20.0f, 14.0f, 20.0f, 16.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 24)).ColorAndOpacity(Cream)
									.Text_Lambda([Panel]() { const FRegionPanelInfo* Info = Panel(); return Info ? Info->Name : FText::GetEmpty(); })
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 9.0f, 0.0f)
									[
										SNew(SBox).WidthOverride(22.0f).HeightOverride(14.0f)
										[
											SNew(SBorder).BorderImage(&ChipBrush)
											.BorderBackgroundColor_Lambda([Panel]() { const FRegionPanelInfo* Info = Panel(); return FSlateColor(Info ? Info->OwnerColor : FLinearColor::Gray); })
										]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock).Font(Font(TEXT("Bold"), 13)).ColorAndOpacity(Cream)
										.Text_Lambda([Panel]() { const FRegionPanelInfo* Info = Panel(); return Info ? Info->OwnerLine : FText::GetEmpty(); })
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Muted).AutoWrapText(true)
									.Text_Lambda([Panel]() { const FRegionPanelInfo* Info = Panel(); return Info ? Info->Details : FText::GetEmpty(); })
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("FORCES"))).Font(Font(TEXT("Bold"), 9, 240)).ColorAndOpacity(Brass)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Font(Font(TEXT("Regular"), 12)).ColorAndOpacity(Cream).AutoWrapText(true)
									.Text_Lambda([Panel]() { const FRegionPanelInfo* Info = Panel(); return Info ? Info->Forces : FText::GetEmpty(); })
								]
							]
						]
					]

					// The reminder of the controls.
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24.0f)
					[
						SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(16.0f, 9.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Drag: move the map     Scroll: zoom     Right-drag or Q / E: turn     Click: pick a territory     Esc: put it down")))
							.Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(Muted)
						]
					]
				]
			];

			SetVisibility(EVisibility::HitTestInvisible);
		}

	private:
		TWeakObjectPtr<ACampaignPlayerController> Controller;
		FSlateBrush PanelBrush;
		FSlateBrush ChipBrush;
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
