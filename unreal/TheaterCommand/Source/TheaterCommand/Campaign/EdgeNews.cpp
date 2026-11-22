#include "Campaign/EdgeNews.h"

#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "Campaign/CampaignView.h"
#include "Campaign/EdgeSeals.h"
#include "HAL/PlatformTime.h"

#include <string>
#include <vector>

// (Release 2, campaign map plan item 5, "News at the map's edge"; ruling 131.) The month's seals, from the month's
// card: the same items in the same order (ruling 64(g)). The layout and the look are in EdgeNewsLayer; what is pure
// logic (the order, the fold, the class, the eight words, where each seal lies) is in EdgeSeals, tested with g++.

namespace
{
	std::string EdgeUtf8(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	FString EdgeWordsOf(const FString& Text)
	{
		return FString(UTF8_TO_TCHAR(EdgeSeals::EightWords(EdgeUtf8(Text)).c_str()));
	}

	/** One item of the card as its seal: what is pressed into it, its colour, its words and its place. */
	FEdgeNewsSeal EdgeSealOf(const FMonthCard& Card, const EdgeSeals::FItem& Item)
	{
		FEdgeNewsSeal Seal;
		Seal.Section = static_cast<int32>(Item.Section);
		Seal.Index = Item.Index;
		FString Word;
		switch (Item.Section)
		{
		case EdgeSeals::ESection::Band:
			if (Card.Bands.IsValidIndex(Item.Index))
			{
				const FMonthBand& Band = Card.Bands[Item.Index];
				Word = Band.Kind;
				Seal.Words = EdgeWordsOf(Band.Text);
				Seal.Region = Band.Region;
				Seal.Tone = Band.bGood ? 1 : -1;
			}
			break;
		case EdgeSeals::ESection::Lead:
			Word = Card.Lead.Stamp;
			Seal.Sign = FString(UTF8_TO_TCHAR(EdgeSeals::LeadSign(EdgeUtf8(Card.Lead.Picture)).c_str()));
			Seal.Words = EdgeWordsOf(Card.Lead.Headline);
			Seal.Region = Card.Lead.Region;
			Seal.Tone = Card.Lead.Tone;
			break;
		case EdgeSeals::ESection::Row:
			if (Card.Rows.IsValidIndex(Item.Index))
			{
				const FMonthRow& Row = Card.Rows[Item.Index];
				Word = Row.Tag;
				Seal.Sign = Row.Sign;
				Seal.Words = EdgeWordsOf(FString(UTF8_TO_TCHAR(EdgeSeals::RowLine(EdgeUtf8(Row.Place), EdgeUtf8(Row.Tag), EdgeUtf8(Row.JoinWord), EdgeUtf8(Row.JoinName), EdgeUtf8(Row.Text)).c_str())));
				Seal.Region = Row.Region;
				Seal.Sea = Row.Region == INDEX_NONE ? Row.Sea : INDEX_NONE;
				Seal.Tone = Row.Tone;
			}
			break;
		case EdgeSeals::ESection::Home:
			if (Card.Home.IsValidIndex(Item.Index))
			{
				const FMonthHome& Line = Card.Home[Item.Index];
				Seal.Sign = Line.Sign;
				FString Said = Line.Count.IsEmpty() ? Line.Text : Line.Count + TEXT(" ") + Line.Text;
				if (!Line.Where.IsEmpty())
				{
					Said += TEXT(", ") + Line.Where;
				}
				Seal.Words = EdgeWordsOf(Said);
				Seal.Tone = Line.bGrey ? 3 : 0;
			}
			break;
		case EdgeSeals::ESection::Staff:
			if (Card.Staff.IsValidIndex(Item.Index))
			{
				Seal.Words = EdgeWordsOf(Card.Staff[Item.Index].Text);
			}
			break;
		case EdgeSeals::ESection::Elsewhere:
			if (Card.Elsewhere.IsValidIndex(Item.Index))
			{
				Seal.Words = EdgeWordsOf(Card.Elsewhere[Item.Index].Text);
			}
			break;
		case EdgeSeals::ESection::More:
			break;
		}
		const std::string Tagged = EdgeUtf8(Word);
		Seal.Class = EdgeSeals::ClassOf(Item.Section, Tagged);
		Seal.Glyph = static_cast<int32>(EdgeSeals::GlyphOf(Item.Section, Tagged, !Seal.Sign.IsEmpty()));
		return Seal;
	}
}

void FEdgeNewsBoard::LayFrom(const FMonthCard& Card)
{
	Seals.Reset();
	Date = Card.Date;
	std::vector<EdgeSeals::FItem> Items;
	EdgeSeals::ListItems(EdgeSeals::FCardCounts{ Card.Bands.Num(), Card.Lead.bValid, Card.Rows.Num(), Card.Home.Num(), Card.Staff.Num(), Card.Elsewhere.Num() }, Items);
	const EdgeSeals::FFold Fold = EdgeSeals::Fold(static_cast<int>(Items.size()), MostSeals);
	for (int Index = 0; Index < Fold.Shown; ++Index)
	{
		Seals.Add(EdgeSealOf(Card, Items[static_cast<size_t>(Index)]));
	}
	if (Fold.More > 0)
	{
		FEdgeNewsSeal& Rest = Seals.AddDefaulted_GetRef();
		Rest.Section = static_cast<int32>(EdgeSeals::ESection::More);
		Rest.More = Fold.More;
		Rest.Class = EdgeSeals::ClassOf(EdgeSeals::ESection::More, std::string());
		Rest.Glyph = static_cast<int32>(EdgeSeals::EGlyph::More);
		Rest.Words = FString::Printf(TEXT("%d more in the month's report"), Fold.More);
	}
}

void FEdgeNewsBoard::Clear()
{
	Seals.Reset();
	Date.Reset();
	FocusSection = -1;
	FocusIndex = -1;
	FocusPage = -1;
	++Revision;
}

void ACampaignPlayerController::LayEdgeNews(const FMonthCard* Shown)
{
	// (the card as it stands open, where it opened by itself; else the rules are asked for it, as R would open it)
	FOfficePage Asked;
	if (!Shown)
	{
		FCampaignSession* Rules = GetSession();
		if (Rules && !Rules->GetView().Human.IsEmpty())
		{
			Asked = Rules->Page(TEXT("reports"), SelectedRegion);
		}
		Shown = Asked.bValid && Asked.Month.bValid && !Asked.Month.bOpening ? &Asked.Month : nullptr;
	}
	EdgeNews.Clear();
	if (Shown)
	{
		EdgeNews.LayFrom(*Shown);
	}
	EdgeNews.LaidAt = FPlatformTime::Seconds();
	// (the post's arrival: a soft stamp, where the card has not opened to say so itself)
	if (EdgeNews.Seals.Num() > 0 && !bPageOpen && !bScripted)
	{
		CampaignSounds::Play(this, TEXT("stamp"), 0.3f, 1.15f);
	}
}

void ACampaignPlayerController::OpenEdgeSeal(int32 Seal)
{
	if (!EdgeNews.Seals.IsValidIndex(Seal) || bWatching)
	{
		return;
	}
	FEdgeNewsSeal& Pressed = EdgeNews.Seals[Seal];
	OpenPage(TEXT("reports"));
	if (!bPageOpen || !Page.Month.bValid)
	{
		return; // (no card to open: the seal stays as it was)
	}
	Pressed.bOpened = true;
	const bool bRest = Pressed.Section == static_cast<int32>(EdgeSeals::ESection::More);
	EdgeNews.FocusSection = bRest ? -1 : Pressed.Section;
	EdgeNews.FocusIndex = bRest ? -1 : Pressed.Index;
	EdgeNews.FocusPage = Page.Revision;
	EdgeNews.FocusAt = FPlatformTime::Seconds();
}
