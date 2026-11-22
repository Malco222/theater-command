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
// logic (the order, the fold, the class, the colour and the pulse, the eight words, where each seal lies) is in
// EdgeSeals, tested with g++.

// (FEdgeNewsSeal keeps its colour as a number, so that EdgeNews.h need not take EdgeSeals' standard headers)
static_assert(static_cast<int32>(EdgeSeals::EWax::Brass) == 4, "FEdgeNewsSeal::Wax's default is EdgeSeals::EWax::Brass");

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

	/**
	 * One item of the card as its seal: what is pressed into it, its colour, its words and its place. And, for the fold
	 * of alike runs, how it looks (OutLook: its wax, its mark, its sign and its pulse) and what the card says of it
	 * (OutSaid: the words a folded seal names its items in, EdgeSeals::FoldWords).
	 */
	FEdgeNewsSeal EdgeSealOf(const FMonthCard& Card, const EdgeSeals::FItem& Item, EdgeSeals::FAlike& OutLook, EdgeSeals::FSaid& OutSaid)
	{
		FEdgeNewsSeal Seal;
		Seal.Section = static_cast<int32>(Item.Section);
		Seal.Index = Item.Index;
		OutSaid = EdgeSeals::PlainSaid(Item.Section, std::string());
		FString Word;
		switch (Item.Section)
		{
		case EdgeSeals::ESection::Band:
			if (Card.Bands.IsValidIndex(Item.Index))
			{
				const FMonthBand& Band = Card.Bands[Item.Index];
				Word = Band.Kind;
				Seal.Words = EdgeWordsOf(Band.Text);
				OutSaid = EdgeSeals::PlainSaid(Item.Section, EdgeUtf8(Band.Text));
				Seal.Region = Band.Region;
				Seal.Tone = Band.bGood ? 1 : -1;
			}
			break;
		case EdgeSeals::ESection::Lead:
			Word = Card.Lead.Stamp;
			Seal.Sign = FString(UTF8_TO_TCHAR(EdgeSeals::LeadSign(EdgeUtf8(Card.Lead.Picture)).c_str()));
			Seal.Words = EdgeWordsOf(Card.Lead.Headline);
			OutSaid = EdgeSeals::PlainSaid(Item.Section, EdgeUtf8(Card.Lead.Headline));
			Seal.Region = Card.Lead.Region;
			Seal.Tone = Card.Lead.Tone;
			break;
		case EdgeSeals::ESection::Row:
			if (Card.Rows.IsValidIndex(Item.Index))
			{
				const FMonthRow& Row = Card.Rows[Item.Index];
				Word = Row.Tag;
				Seal.Sign = Row.Sign;
				// (its line, EdgeSeals::RowLine, is its seal's words cut to eight; its place and its head name it in a run)
				OutSaid = EdgeSeals::RowSaid(EdgeUtf8(Row.Place), EdgeUtf8(Row.Tag), EdgeUtf8(Row.JoinWord), EdgeUtf8(Row.JoinName), EdgeUtf8(Row.Text));
				Seal.Words = FString(UTF8_TO_TCHAR(EdgeSeals::EightWords(OutSaid.Line).c_str()));
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
				OutSaid = EdgeSeals::PlainSaid(Item.Section, EdgeUtf8(Said));
				Seal.Tone = Line.bGrey ? 3 : 0;
			}
			break;
		case EdgeSeals::ESection::Staff:
			if (Card.Staff.IsValidIndex(Item.Index))
			{
				Seal.Words = EdgeWordsOf(Card.Staff[Item.Index].Text);
				OutSaid = EdgeSeals::PlainSaid(Item.Section, EdgeUtf8(Card.Staff[Item.Index].Text));
			}
			break;
		case EdgeSeals::ESection::Elsewhere:
			if (Card.Elsewhere.IsValidIndex(Item.Index))
			{
				Seal.Words = EdgeWordsOf(Card.Elsewhere[Item.Index].Text);
				OutSaid = EdgeSeals::PlainSaid(Item.Section, EdgeUtf8(Card.Elsewhere[Item.Index].Text));
			}
			break;
		case EdgeSeals::ESection::More:
			break;
		}
		const std::string Tagged = EdgeUtf8(Word);
		Seal.Class = EdgeSeals::ClassOf(Item.Section, Tagged);
		// (the colour of its row on the card, and the pulse: the art director's ruling of 22 November. A band's Tone is
		// 1 where the card edges it green, Band.bGood, a FREED his own side won; a row's is its own, 1 for the same. And
		// what is pressed in: our own order signs, from the seal set, a star never. EdgeSeals::AlikeOf: LookOf, GlyphOf,
		// MayPress, the same look the fold of alike runs compares)
		const std::string Signed = EdgeUtf8(Seal.Sign);
		OutLook = EdgeSeals::AlikeOf(Item.Section, Tagged, Seal.Tone, Signed);
		Seal.Wax = static_cast<int32>(OutLook.Wax);
		Seal.bPulse = OutLook.bPulse;
		Seal.Glyph = static_cast<int32>(OutLook.Glyph);
		// (the finest part of its seal picture is looked up once, here: the screen presses it only where that holds
		// 1.5 px, EdgeNewsLayer)
		Seal.SignFinest = EdgeSeals::Look::SignFinest(Signed);
		return Seal;
	}
}

void FEdgeNewsBoard::LayFrom(const FMonthCard& Card)
{
	Seals.Reset();
	Date = Card.Date;
	std::vector<EdgeSeals::FItem> Listed;
	EdgeSeals::ListItems(EdgeSeals::FCardCounts{ Card.Bands.Num(), Card.Lead.bValid, Card.Rows.Num(), Card.Home.Num(), Card.Staff.Num(), Card.Elsewhere.Num() }, Listed);
	Items = static_cast<int32>(Listed.size());
	// Every item's seal, its look and what the card says of it; then the fold of alike runs (ruling 64(g) as amended
	// 08:55Z: three or more in a row that would look the same stand as one seal, with their count, at the first item's
	// place; a dated warning never folds); and only then the most that may stand. Display only: the card is not touched.
	TArray<FEdgeNewsSeal> Each;
	Each.Reserve(Items);
	std::vector<EdgeSeals::FAlike> Looks(Listed.size());
	std::vector<EdgeSeals::FSaid> Said(Listed.size());
	for (size_t Index = 0; Index < Listed.size(); ++Index)
	{
		Each.Add(EdgeSealOf(Card, Listed[Index], Looks[Index], Said[Index]));
	}
	std::vector<EdgeSeals::FAlikeRun> Runs;
	EdgeSeals::FoldAlike(Looks, Runs);
	// (no more than can stand: the screen folds the rest into "+N", and any it has no room for, counting their items:
	// EdgeNewsLayer)
	const int32 Kept = FMath::Min(static_cast<int32>(Runs.size()), MostSeals);
	for (int32 Index = 0; Index < Kept; ++Index)
	{
		const EdgeSeals::FAlikeRun& Run = Runs[static_cast<size_t>(Index)];
		Seals.Add(Each[Run.First]);
		FEdgeNewsSeal& Seal = Seals.Last();
		Seal.Alike = Run.Count;
		if (Run.Count > 1)
		{
			// (the run's items named in card order, and how many more: "9 declare war on Germany: Cuba, Panama, Costa Rica and 6 more")
			Seal.Words = FString(UTF8_TO_TCHAR(EdgeSeals::FoldWords(Said, Run).c_str()));
		}
	}
	Rest = FEdgeNewsSeal();
	Rest.Section = static_cast<int32>(EdgeSeals::ESection::More);
	Rest.Class = EdgeSeals::ClassOf(EdgeSeals::ESection::More, std::string());
	const EdgeSeals::FLook RestLook = EdgeSeals::LookOf(EdgeSeals::ESection::More, std::string(), 0);
	Rest.Wax = static_cast<int32>(RestLook.Wax);
	Rest.bPulse = RestLook.bPulse;
	Rest.Glyph = static_cast<int32>(EdgeSeals::EGlyph::More);
}

void FEdgeNewsBoard::Clear()
{
	Seals.Reset();
	Items = 0;
	Rest = FEdgeNewsSeal();
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
	// (INDEX_NONE: the "+N" seal, which opens the card at its head)
	const bool bRest = Seal == INDEX_NONE;
	if ((!bRest && !EdgeNews.Seals.IsValidIndex(Seal)) || EdgeNews.Seals.Num() == 0 || bWatching)
	{
		return;
	}
	OpenPage(TEXT("reports"));
	if (!bPageOpen || !Page.Month.bValid || (!bRest && !EdgeNews.Seals.IsValidIndex(Seal)))
	{
		return; // (no card to open: the seal stays as it was)
	}
	FEdgeNewsSeal& Pressed = bRest ? EdgeNews.Rest : EdgeNews.Seals[Seal];
	Pressed.bOpened = true;
	EdgeNews.FocusSection = bRest ? -1 : Pressed.Section;
	EdgeNews.FocusIndex = bRest ? -1 : Pressed.Index;
	EdgeNews.FocusPage = Page.Revision;
	EdgeNews.FocusAt = FPlatformTime::Seconds();
}
