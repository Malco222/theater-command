// The bargains' look test (release 2: a neutral's bargains and the volunteer legions on the campaign map). Builds
// Campaign/BargainLook (the game's own code, no engine) and holds it to the art director's look of 22 November ("Drawn
// for the build", 07:56 UTC, and its picture's source) and the build contract's section C: which rows get a lane, a
// crate, a tint, a seal and a card; every word; the card box by box at Px 1 and 1.08; where the card stands; the lane's
// casing, dashes, cut, ticks, ring and crate; the tint's strip; the seals; the plate line, our own plate and the tray;
// one card open at a time; and that the paths run every frame allocate nothing once grown. It also reads the rules' own
// look answers (Tools/bargains/fixtures/look-*.json, through Campaign/BargainFeed's ReadBook) and lays every card and
// lane in them.
// Run by Tools/bargains/run.sh with the feed and the look (one file: the allocation count is kept here too).
// Usage: look_test [fixtures folder]. The goods signs it checks for are Content/Goods/goods-*.png beside this folder,
// or *.png or *.svg in LOOK_SIGNS where that is set (the art director's signs/ folder).

#include "Campaign/BargainLook.h"
#include "look_measure.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <vector>

// ---- counting allocations: the per-frame paths must make none once their buffers have grown. (Not inlined, so that
// GCC does not take the malloc and free pair in a caller for a mismatch.)
namespace
{
	long GNews = 0;
	bool GCounting = false;
}

[[gnu::noinline]] void* operator new(std::size_t Size)
{
	if (GCounting)
	{
		++GNews;
	}
	void* Block = std::malloc(Size ? Size : 1);
	if (!Block)
	{
		std::abort();
	}
	return Block;
}
[[gnu::noinline]] void* operator new[](std::size_t Size) { return operator new(Size); }
// (the nothrow forms too: std::stable_sort takes its buffer with them, and every block here must come from malloc)
[[gnu::noinline]] void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
	if (GCounting)
	{
		++GNews;
	}
	return std::malloc(Size ? Size : 1);
}
[[gnu::noinline]] void* operator new[](std::size_t Size, const std::nothrow_t& Tag) noexcept { return operator new(Size, Tag); }
[[gnu::noinline]] void operator delete(void* Block, const std::nothrow_t&) noexcept { std::free(Block); }
[[gnu::noinline]] void operator delete[](void* Block, const std::nothrow_t&) noexcept { std::free(Block); }
[[gnu::noinline]] void operator delete(void* Block) noexcept { std::free(Block); }
[[gnu::noinline]] void operator delete(void* Block, std::size_t) noexcept { std::free(Block); }
[[gnu::noinline]] void operator delete[](void* Block) noexcept { std::free(Block); }
[[gnu::noinline]] void operator delete[](void* Block, std::size_t) noexcept { std::free(Block); }

using namespace Bargains;

namespace
{
	int GPassed = 0;
	int GFailed = 0;

	void Check(bool bOk, const char* What, int Line)
	{
		if (bOk)
		{
			++GPassed;
			return;
		}
		++GFailed;
		std::printf("FAIL line %d: %s\n", Line, What);
	}

	void CheckText(const std::string& Got, const std::string& Want, int Line)
	{
		if (Got == Want)
		{
			++GPassed;
			return;
		}
		++GFailed;
		std::printf("FAIL line %d:\n  got  \"%s\"\n  want \"%s\"\n", Line, Got.c_str(), Want.c_str());
	}

	bool Near(double A, double B, double Tol = 1e-6) { return std::fabs(A - B) <= Tol; }

#define CHECK(Cond) Check((Cond), #Cond, __LINE__)
#define CHECK_TEXT(Got, Want) CheckText((Got), (Want), __LINE__)

	const FMeasure KitMeasure = [](std::string_view Text, const FFace& Face) { return LookMeasure::Measure(Text, Face); };

	template <typename TNation>
	void SetThe(TNation& Nation, const char* The)
	{
		if constexpr (requires(TNation& N) { N.The; })
		{
			Nation.The = The;
		}
	}

	template <typename TNation>
	constexpr bool HasThe()
	{
		return requires(TNation& N) { N.The; };
	}

	FNation Nation(const char* Tag, const char* Name, const char* Adj, int Bloc, const char* The = nullptr)
	{
		FNation N;
		N.Tag = Tag;
		N.Name = Name;
		N.Adj = Adj;
		N.Bloc = Bloc;
		SetThe(N, The ? The : Name);
		return N;
	}

	uint32_t ColourOf(std::string_view Tag)
	{
		if (Tag == "UK") return 0xB5574A;
		if (Tag == "GER") return 0x3B4048;
		if (Tag == "ITA") return 0x4E6B3A;
		if (Tag == "USA") return 0x5A6F8C;
		return 0x808080;
	}

	/** The tests' viewer sees every legion as one (the game asks its view's corps: LookInOf in BargainLayer.cpp). */
	bool SeesEveryLegion(int, std::string_view)
	{
		return true;
	}

	FRound Round(std::vector<FBid> Bids, std::optional<double> Mine, std::string CanBid, bool bAccept = false)
	{
		FRound R;
		R.Months = 1;
		R.Bids = std::move(Bids);
		R.Mine = Mine;
		R.Steps = { 0.5, 1.0, 1.5, 2.0, 2.5 };
		R.CanBid = std::move(CanBid);
		R.bAccept = bAccept;
		return R;
	}

	/** Britain's seat, spring 1942: the picture's scene. */
	FBook BritainsSeat()
	{
		FBook Book;
		Book.bOn = true;
		Book.Viewer = "UK";
		Book.Month = "March";
		Book.Turn = 31;
		Book.Term = 12;
		Book.RoundLen = 1;
		Book.Nations = { Nation("UK", "Britain", "British", -1), Nation("GER", "Germany", "German", 1), Nation("POR", "Portugal", "Portuguese", 0),
			Nation("ESP", "Spain", "Spanish", 0), Nation("SWE", "Sweden", "Swedish", 0), Nation("ITA", "Italy", "Italian", 1),
			Nation("USA", "United States", "American", -1, "the United States"), Nation("SOV", "Soviet Union", "Soviet", -1, "the Soviet Union") };
		FRow Por;
		Por.Id = "por-wolfram";
		Por.Seller = "POR";
		Por.SellerName = "Portugal";
		Por.Name = "Portuguese wolfram";
		Por.Sign = "wolfram";
		Por.CapitalName = "Lisbon";
		Por.Capital = 172;
		Por.Buyer = "UK";
		Por.MonthsLeft = 8;
		Por.Way = EWay::Sea;
		Por.State = EState::Open;
		Por.Rel = ERel::Self;
		Por.Round = Round({ { "GER", 2.0 }, { "UK", 1.5 } }, 1.5, "");
		Por.Cargo = FCargo{ true, 0.0, 0.0, 0.4 };
		Por.Lean = -30.0;
		Por.Path = { { 262, 520 }, { 222, 450 }, { 245, 330 }, { 240, 220 }, { 150, 60 }, { 120, 0 } };
		FRow Esp;
		Esp.Id = "esp-wolfram";
		Esp.Seller = "ESP";
		Esp.SellerName = "Spain";
		Esp.Name = "Spanish wolfram";
		Esp.Sign = "wolfram";
		Esp.CapitalName = "Madrid";
		Esp.Capital = 159;
		Esp.Buyer = "GER";
		Esp.MonthsLeft = 10;
		Esp.Way = EWay::Land;
		Esp.State = EState::Open;
		Esp.Rel = ERel::Enemy;
		Esp.Cargo = FCargo{ false, 0.0, 0.0, std::nullopt };
		Esp.Lean = 63.7;
		Esp.Path = { { 548, 458 }, { 575, 360 }, { 650, 290 }, { 740, 170 }, { 800, 0 } };
		FRow Cut;
		Cut.Id = "esp-ore";
		Cut.Seller = "ESP";
		Cut.SellerName = "Spain";
		Cut.Name = "Spanish iron ore";
		Cut.Sign = "ore";
		Cut.CapitalName = "Madrid";
		Cut.Buyer = "GER";
		Cut.MonthsLeft = 4;
		Cut.Way = EWay::None;
		Cut.State = EState::Cut;
		Cut.Rel = ERel::Enemy;
		Cut.Lean = 63.7;
		Cut.Path = { { 520, 250 }, { 522, 222 }, { 538, 200 }, { 556, 182 }, { 580, 150 } };
		Cut.Cut = FSeaMark{ 24, "Bay of Biscay", "March", { 547, 191 } };
		Book.Rows = { Por, Esp, Cut };
		FLegion Blue;
		Blue.Home = "ESP";
		Blue.HomeName = "Spain";
		Blue.Patron = "GER";
		Blue.Against = "SOV";
		Blue.Name = "Blue Division";
		Blue.Place = "Leningrad";
		Blue.Corps = 363;
		Blue.Region = 214;
		Blue.HomeCapital = 159;
		Blue.Pct = 88;
		Blue.Tour = 24;
		Blue.TourAt = 14;
		Blue.MonthsLeft = 11;
		Book.Legions = { Blue };
		return Book;
	}

	const FRow& RowOf(const FBook& Book, const char* Id)
	{
		for (const FRow& Row : Book.Rows)
		{
			if (Row.Id == Id)
			{
				return Row;
			}
		}
		std::printf("no row %s\n", Id);
		std::abort();
	}

	const FDraw* TextNamed(const FCard& Card, const std::string& Text)
	{
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Text && Draw.Text == Text)
			{
				return &Draw;
			}
		}
		return nullptr;
	}

	std::vector<const FDraw*> TextsInFace(const FCard& Card, const FFace& Face, uint32_t Ink)
	{
		std::vector<const FDraw*> Out;
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Text && Draw.Font.Family == Face.Family && Draw.Font.Style == Face.Style && Near(Draw.Font.Size, Face.Size) && Draw.Fill.Srgb == Ink)
			{
				Out.push_back(&Draw);
			}
		}
		return Out;
	}

	bool Inside(const FRect& In, const FRect& Outer, double Tol = 1e-6)
	{
		return In.X >= Outer.X - Tol && In.Y >= Outer.Y - Tol && In.Right() <= Outer.Right() + Tol && In.Bottom() <= Outer.Bottom() + Tol;
	}

	double Cross(FPt O, FPt A, FPt B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); }

	/** True when no quad of the strip folds: every triangle turns the same way as the first that has an area (or has none). */
	bool StripUnfolded(const FStrip& Strip)
	{
		int Sign = 0;
		for (size_t I = 0; I + 1 < Strip.Outer.size(); ++I)
		{
			const double Tri[2] = { Cross(Strip.Outer[I], Strip.Inner[I], Strip.Outer[I + 1]), Cross(Strip.Inner[I], Strip.Inner[I + 1], Strip.Outer[I + 1]) };
			for (double Area : Tri)
			{
				if (std::fabs(Area) < 1e-9)
				{
					continue;
				}
				const int Here = Area > 0.0 ? 1 : -1;
				if (Sign == 0)
				{
					Sign = Here;
				}
				else if (Here != Sign)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool InPolygon(FPt P, const FPath& Ring)
	{
		bool bIn = false;
		for (size_t I = 0, J = Ring.size() - 1; I < Ring.size(); J = I++)
		{
			if ((Ring[I].Y > P.Y) != (Ring[J].Y > P.Y) && P.X < (Ring[J].X - Ring[I].X) * (P.Y - Ring[I].Y) / (Ring[J].Y - Ring[I].Y) + Ring[I].X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	// ----------------------------------------------------------------------------------------------------------------

	void TestUnitsAndColours()
	{
		CHECK(Near(PxOf(900.0, 1.0, 1.0), 1.0));
		CHECK(Near(PxOf(972.0, 1.0, 1.0), 1.08));
		CHECK(Near(PxOf(1800.0, 2.0, 1.0), 1.0));
		CHECK(Near(PxOf(1786.0, 1.0, 1.8375), 1786.0 / 900.0 / 1.8375));
		CHECK(Near(PxOf(0.0, 1.0, 1.0), 1.0));
		CHECK(Near(PixelOf(4000.0, 1.0, 1.0, 1.0), 1.48));
		CHECK(Near(PixelOf(4000.0, 2.0, 1.08, 1.0), 0.7992));
		double LaidAt = 0.0;
		CHECK(Relay(LaidAt, 4000.0) && Near(LaidAt, 4000.0));
		CHECK(!Relay(LaidAt, 4300.0) && Near(LaidAt, 4000.0)); // (7.5%)
		CHECK(Relay(LaidAt, 4340.0) && Near(LaidAt, 4340.0)); // (8.5%)
		CHECK(!Relay(LaidAt, 4340.0 * 0.92));

		// The kit's numbers are sRGB: linear by the sRGB curve, as FLinearColor(FColor) reads them, never bytes over 255.
		const FLinear Paper = LinearOf(FInk{ Kit::Paper200, 0.55f });
		CHECK(Near(Paper.R, 0.76052, 1e-4) && Near(Paper.G, 0.67954, 1e-4) && Near(Paper.B, 0.50289, 1e-4) && Near(Paper.A, 0.55, 1e-6));
		CHECK(!Near(Paper.R, 0xE2 / 255.0, 0.05));
		const FLinear Brass = LinearOf(FInk{ Kit::Brass700, 1.0f });
		CHECK(Near(Brass.R, 0.25415, 1e-4) && Near(Brass.G, 0.14703, 1e-4) && Near(Brass.B, 0.04374, 1e-4));
		const FLinear Ring = LinearOf(FInk{ Kit::Brass300, 1.0f });
		CHECK(Near(Ring.R, 0.7454, 1e-4) && Near(Ring.G, 0.47932, 1e-4) && Near(Ring.B, 0.16827, 1e-4));
		CHECK(Near(LinearOfByte(0), 0.0) && Near(LinearOfByte(255), 1.0, 1e-6) && Near(LinearOfByte(10), 0.0030353, 1e-6));
		CHECK(Kit::Paper200 == 0xE2D7BC && Kit::Paper300 == 0xCDBF9C && Kit::Ink900 == 0x2A2118 && Kit::Brass700 == 0x8A6B3B && Kit::Brass300 == 0xE0B872 && Kit::Desk900 == 0x17120D);
		CHECK(Kit::HeldInk == 0x4A3F30 && Kit::DimInk == 0x7A6A52 && Kit::HintInk == 0x5B4E3C);
		// (a hollow ring's fill: above MakeBox's cull of 1e-4, and nothing a viewer could see)
		CHECK(Kit::Hollow > 1e-4f && Near(Kit::Hollow, 1.0 / 255.0, 1e-9));
		const FDraw Hollow = RingDraw(FPt{ 10, 10 }, 12.0, 1.6, FInk{ Kit::Brass300, 1.0f });
		CHECK(Hollow.Kind == EDraw::Box && Near(Hollow.Fill.A, 1.0 / 255.0, 1e-9) && Hollow.Edge.Srgb == Kit::Brass300 && Near(Hollow.EdgeW, 1.6) && Near(Hollow.Radius, 6.0));
		CHECK(Near(Hollow.At.X, 4.0) && Near(Hollow.At.W, 12.0));
	}

	/** The folder of the goods signs' pictures: LOOK_SIGNS, or Content/Goods two folders up from this file; "" if neither is there. */
	std::string SignsFolder()
	{
		if (const char* Env = std::getenv("LOOK_SIGNS"); Env && *Env)
		{
			return Env;
		}
		std::string Here = __FILE__;
		const size_t Slash = Here.find_last_of('/');
		Here = Slash == std::string::npos ? std::string(".") : Here.substr(0, Slash);
		const std::string Goods = Here + "/../../Content/Goods";
		return std::ifstream(Goods + "/goods-ore.png").good() ? Goods : std::string();
	}

	void TestWords()
	{
		const FBook Book = BritainsSeat();
		const FRow& Por = RowOf(Book, "por-wolfram");
		const std::string Signs = SignsFolder();
		if (Signs.empty())
		{
			std::printf("(no goods signs found: Content/Goods is not in this tree and LOOK_SIGNS is not set; their files are not checked)\n");
		}
		CHECK_TEXT(std::string(NounOf("ore")), "Ore");
		CHECK_TEXT(std::string(NounOf("oil")), "Oil");
		CHECK_TEXT(std::string(NounOf("chrome")), "Chrome");
		CHECK_TEXT(std::string(NounOf("wolfram")), "Wolfram");
		CHECK_TEXT(std::string(NounOf("beef")), "Food");
		CHECK_TEXT(std::string(NounOf("exports")), "Goods");
		CHECK_TEXT(std::string(NounOf("base")), "Bases");
		CHECK_TEXT(std::string(NounOf("harbour")), "Goods");
		const char* Seven[] = { "ore", "oil", "chrome", "wolfram", "beef", "base", "exports" };
		for (const char* Sign : Seven)
		{
			const std::string Key = SignKey(Sign);
			CHECK_TEXT(Key, std::string("goods-") + Sign);
			if (!Signs.empty())
			{
				CHECK(std::ifstream(Signs + "/" + Key + ".png").good() || std::ifstream(Signs + "/" + Key + ".svg").good());
			}
		}
		CHECK_TEXT(SignKey("harbour"), "goods-exports");
		CHECK_TEXT(Caps(NounOf("wolfram")), "WOLFRAM");
		CHECK_TEXT(Figure(4.5), "4.5");
		CHECK_TEXT(Figure(2.0), "2");
		CHECK_TEXT(Figure(0.69), "0.7");
		CHECK_TEXT(Figure(14.0), "14");
		CHECK_TEXT(Figure(2.6), "2.6");
		CHECK_TEXT(Figure(5.18), "5.2");
		CHECK_TEXT(Figure(0.0), "0");
		CHECK_TEXT(Figure(-0.01), "0");
		CHECK_TEXT(HeldLine(Por, Book), "held by Britain, 5 of 12 months");
		FRow Held = Por;
		Held.MonthsLeft = 12;
		CHECK_TEXT(HeldLine(Held, Book), "held by Britain, 1 of 12 months");
		Held.MonthsLeft = 0;
		CHECK_TEXT(HeldLine(Held, Book), "held by Britain, 12 of 12 months");
		Held.MonthsLeft = 30;
		CHECK_TEXT(HeldLine(Held, Book), "held by Britain, 1 of 12 months");
		Held.Buyer.clear();
		CHECK_TEXT(HeldLine(Held, Book), "no buyer yet");
		Held.Buyer = "USA";
		Held.MonthsLeft = 8;
		CHECK_TEXT(HeldLine(Held, Book), HasThe<FNation>() ? "held by the United States, 5 of 12 months" : "held by United States, 5 of 12 months");
		CHECK_TEXT(RivalHint(Por), "Rival bids show with an agent in Lisbon, or in the bidder's capital.");

		FRow Swe;
		Swe.Name = "Swedish iron ore";
		Swe.Round = Round({}, std::nullopt, "");
		CHECK_TEXT(SealTip(Swe), "Swedish iron ore: bids this month. Click to see the bids.");
		Swe.Round->CanBid = "Only a country at war bids for it.";
		CHECK_TEXT(SealTip(Swe), "Swedish iron ore: bids this month. Only a country at war bids for it.");
		Swe.bAgentSeller = true;
		CHECK_TEXT(SealTip(Swe), "Swedish iron ore: bids this month. Click to see the bids.");

		CHECK_TEXT(CutHover(RowOf(Book, "esp-ore"), Book), "Ore for Germany lost at sea, Bay of Biscay, March");
		FRow Ore;
		Ore.Sign = "ore";
		Ore.Buyer = "GER";
		Ore.Cargo = FCargo{ true, 4.5, 2.6, 1.2 };
		FBook August = Book;
		August.Month = "August";
		CHECK_TEXT(CrateHover(Ore, August), "Ore reached Germany, August: 4.5 supply, 2.6 oil, 1.2 production");
		Ore.Cargo = FCargo{ true, 4.5, 0.0, std::nullopt };
		CHECK_TEXT(CrateHover(Ore, August), "Ore reached Germany, August: 4.5 supply, production ?");
		Ore.Cargo = FCargo{ true, 2.0, 0.0, 0.0 };
		CHECK_TEXT(CrateHover(Ore, August), "Ore reached Germany, August: 2 supply");
		CHECK_TEXT(ExitText(Por, Book), "to Britain");

		FBook Legions = Book;
		CHECK_TEXT(PlateLine(Legions, "ESP"), "1 division abroad, Leningrad");
		Legions.Legions.push_back(Legions.Legions[0]);
		CHECK_TEXT(PlateLine(Legions, "ESP"), "2 divisions abroad, Leningrad");
		Legions.Legions[1].Place = "Kiev";
		CHECK_TEXT(PlateLine(Legions, "ESP"), "2 divisions abroad");
		Legions.Legions.resize(1);
		Legions.Legions[0].Place.clear();
		Legions.Legions[0].Corps = -1;
		CHECK_TEXT(PlateLine(Legions, "ESP"), "1 division abroad");
		Legions.Legions[0] = Book.Legions[0];
		Legions.Legions[0].bRecall = true;
		CHECK_TEXT(PlateLine(Legions, "ESP"), "home from Leningrad");
		CHECK_TEXT(PlateLine(Legions, "POR"), "");
		CHECK_TEXT(TrayLine(Book.Legions[0]), "1 DIV \xC2\xB7 88% \xC2\xB7 TOUR 14 OF 24 MO");
		CHECK_TEXT(FlagTip(Book.Legions[0], Book), "Volunteers from Spain, with the German army, Leningrad front.");
		FLegion Away = Book.Legions[0];
		Away.Place.clear();
		CHECK_TEXT(FlagTip(Away, Book), "Volunteers from Spain, with the German army.");
		// (call 13: the tray's line under the title, from the rules' words: the adjectives the bridge sends, the province)
		CHECK_TEXT(LegionLine(Book.Legions[0], Book), "Spanish volunteers, with the German army, Leningrad front");
		CHECK_TEXT(LegionLine(Away, Book), "Spanish volunteers, with the German army");
		{
			FBook NoAdj = Book;
			for (FNation& Each : NoAdj.Nations)
			{
				if (Each.Tag == "ESP")
				{
					Each.Adj.clear();
				}
			}
			CHECK_TEXT(LegionLine(NoAdj.Legions[0], NoAdj), "Volunteers from Spain, with the German army, Leningrad front");
			FLegion Portuguese = Book.Legions[0];
			Portuguese.Home = "POR";
			Portuguese.HomeName.clear();
			Portuguese.Place = "Kalinin";
			CHECK_TEXT(LegionLine(Portuguese, Book), "Portuguese volunteers, with the German army, Kalinin front");
		}

		// (wrapping: whole words, no dots, at most the lines asked)
		std::vector<std::string> Lines;
		const FFace Hint = HintFace(1.0);
		CHECK(Wrap(RivalHint(Por), Hint, 208.0, KitMeasure, 4, Lines) == 2);
		CHECK_TEXT(Lines[0] + " " + Lines[1], RivalHint(Por));
		for (const std::string& Line : Lines)
		{
			CHECK(KitMeasure(Line, Hint).X <= 208.0 && Line.find("...") == std::string::npos);
		}
		const std::string Long = "The bids are in: the seller is choosing, and the season's last convoy has sailed from the port without the goods on board, so nothing more can be offered.";
		CHECK(Wrap(Long, Hint, 208.0, KitMeasure, 2, Lines) == 2);
	}

	void TestFlags()
	{
		const FPlainFlag* Spain = PlainFlagOf("ESP");
		CHECK(Spain && !Spain->bVertical && Spain->Count == 3 && Spain->Bands[0].Weight == 1 && Spain->Bands[1].Weight == 2 && Spain->Bands[2].Weight == 1);
		CHECK(Spain && Spain->Bands[0].Srgb == Spain->Bands[2].Srgb && Spain->Bands[0].Srgb == 0xB33A2E && Spain->Bands[1].Srgb == 0xC8A43A);
		const FPlainFlag* Portugal = PlainFlagOf("POR");
		CHECK(Portugal && Portugal->bVertical && Portugal->Count == 2 && Portugal->Bands[0].Weight == 2 && Portugal->Bands[1].Weight == 3);
		const FPlainFlag* Germany = PlainFlagOf("GER");
		CHECK(Germany && !Germany->bVertical && Germany->Count == 3 && Germany->Bands[0].Srgb == 0x151515 && Germany->Bands[1].Srgb == 0xEFEAE0 && Germany->Bands[2].Srgb == 0xB33A2E);
		CHECK(!PlainFlagOf("UK") && !PlainFlagOf("SWE"));
		CHECK_TEXT(FlagKey("ESP"), "ESP_PLAIN");
		CHECK_TEXT(FlagKey("POR"), "POR_PLAIN");
		CHECK_TEXT(FlagKey("GER"), "GER_PLAIN");
		CHECK_TEXT(FlagKey("UK"), "UK");
		for (const char* Tag : { "ESP", "POR", "GER" })
		{
			CHECK(FlagKey(Tag).find('~') == std::string::npos);
		}
		CHECK(Near(PennantShare, 0.7));
		// The pennant is the picture's triangle (30 long, 16 at the hoist): the hoist on the pole, the point out along the
		// cloth at half its height, the flag's picture laid so that its bands run together to the point.
		{
			const FPennantShape Pennant = PennantShape(5.0, 100.0, 10.0, 0, 1.0);
			CHECK(Near(Pennant.At[0].X, 5.0) && Near(Pennant.At[0].Y, 100.0) && Near(Pennant.At[1].X, 5.0) && Near(Pennant.At[1].Y, 100.0 - 7.0 * 16.0 / 30.0));
			CHECK(Near(Pennant.At[2].X, 5.0 + 7.0) && Near(Pennant.At[2].Y, 100.0 - 0.5 * 7.0 * 16.0 / 30.0));
			CHECK(Near(Pennant.Uv[0].X, 0.0) && Near(Pennant.Uv[0].Y, 0.0) && Near(Pennant.Uv[1].X, 0.0) && Near(Pennant.Uv[1].Y, 1.0) && Near(Pennant.Uv[2].X, 1.0) && Near(Pennant.Uv[2].Y, 0.5));
			// (under two ally pennants, FlagWidth x 0.46 at 3:2, 0.3 U apart; never more than three slots down)
			const FPennantShape Under = PennantShape(5.0, 100.0, 10.0, 2, 1.0);
			CHECK(Near(Under.At[0].Y, 100.0 - 2.0 * (10.0 * 0.46 / 1.5 + 0.3)));
			CHECK(Near(PennantShape(5.0, 100.0, 10.0, 7, 1.0).At[0].Y, PennantShape(5.0, 100.0, 10.0, 3, 1.0).At[0].Y));
			// (wound as the old quad's first triangle: hoist top, hoist foot, then out along the cloth)
			const double Cross = (Pennant.At[1].X - Pennant.At[0].X) * (Pennant.At[2].Y - Pennant.At[0].Y) - (Pennant.At[1].Y - Pennant.At[0].Y) * (Pennant.At[2].X - Pennant.At[0].X);
			CHECK(Cross > 0.0);
		}
		// Money as the game shows it: points x the book's scale, whole.
		CHECK_TEXT(Money(1.5, 100.0), "150");
		CHECK_TEXT(Money(5.18, 100.0), "518");
		CHECK_TEXT(Money(0.52, 100.0), "52");
		CHECK_TEXT(Money(0.0, 100.0), "0");
		CHECK_TEXT(Money(2.5, 10.0), "25");
		CHECK_TEXT(Money(std::numeric_limits<double>::infinity(), 100.0), "0");
		CHECK(Near(Card::FlagW / Card::FlagH, 1.5) && Near(TrayFlagW / TrayFlagH, 1.5)); // (the house's 3:2)
	}

	void TestLookAt()
	{
		FBook Book = BritainsSeat();
		FLookIn In;
		In.SrgbOf = ColourOf;
		In.SeesLegion = SeesEveryLegion;
		FLook Look;
		LookAt(Book, In, Look);
		// Lanes: Britain's own (first), Germany's named overland one, Germany's cut one with its mark.
		CHECK(Look.Lanes.size() == 3);
		CHECK(Look.Lanes.size() == 3 && Look.Lanes[0].Row == 0 && Look.Lanes[0].Ink == 0xB5574A && Look.Lanes[0].bCrate && !Look.Lanes[0].bCut);
		CHECK(Look.Lanes.size() == 3 && Look.Lanes[0].Tag == "to Britain" && Look.Lanes[0].Hover == "Wolfram reached Britain, March: 0.4 production");
		CHECK(Look.Lanes.size() == 3 && Look.Lanes[1].Row == 1 && !Look.Lanes[1].bCrate && Look.Lanes[1].Ink == 0x3B4048 && Look.Lanes[1].Tag == "to Germany");
		CHECK(Look.Lanes.size() == 3 && Look.Lanes[2].bCut && !Look.Lanes[2].bCrate && Look.Lanes[2].Hover == "Ore for Germany lost at sea, Bay of Biscay, March");
		// Tint: Portugal leans to Britain at 30 (strong); Spain is not the viewer's.
		CHECK(Look.Tints.size() == 1 && Look.Tints[0].Seller == "POR" && Look.Tints[0].Ink == 0xB5574A && Near(Look.Tints[0].Alpha, 0.22));
		// Seals: Portugal's round opens a card (the viewer may bid).
		CHECK(Look.Seals.size() == 1 && Look.Seals[0].Row == 0 && Look.Seals[0].bOpens && !Look.Seals[0].bOpen);
		CHECK(Look.Plates.size() == 1 && Look.Plates[0].Home == "ESP" && Look.Plates[0].Capital == 159 && Look.Plates[0].Line == "1 division abroad, Leningrad");
		CHECK(Look.Trays.size() == 1 && Look.Trays[0].Corps == 363 && Look.Trays[0].Flag == "ESP_PLAIN" && Look.Trays[0].Line == "1 DIV \xC2\xB7 88% \xC2\xB7 TOUR 14 OF 24 MO");
		CHECK(Look.Trays.size() == 1 && Look.Trays[0].Legion == "Spanish volunteers, with the German army, Leningrad front" && Look.Trays[0].Tip == "Volunteers from Spain, with the German army, Leningrad front.");

		// Every lane rule, one row at a time.
		auto Lane = [&Book](auto Change)
		{
			FBook One = Book;
			FRow Row = RowOf(Book, "esp-wolfram");
			Change(Row);
			One.Rows = { Row };
			return DrawsLane(One.Rows[0], One);
		};
		CHECK(Lane([](FRow&) {}));
		CHECK(!Lane([](FRow& R) { R.bGeneric = true; })); // (an enemy's generic lane: clutter)
		CHECK(!Lane([](FRow& R) { R.Rel = ERel::Ally; })); // (an ally's lane)
		CHECK(Lane([](FRow& R) { R.Rel = ERel::Ally; R.bAgentSeller = true; }));
		CHECK(Lane([](FRow& R) { R.Rel = ERel::Other; R.bAgentBuyer = true; }));
		CHECK(Lane([](FRow& R) { R.Rel = ERel::Self; R.bGeneric = true; }));
		CHECK(Lane([](FRow& R) { R.Rel = ERel::Other; R.Seller = "UK"; })); // (the viewer sells)
		CHECK(!Lane([](FRow& R) { R.Path.resize(1); }));
		CHECK(!Lane([](FRow& R) { R.Base = 559; }));
		CHECK(!Lane([](FRow& R) { R.Buyer.clear(); }));
		CHECK(!Lane([](FRow& R) { R.State = EState::Cut; })); // (cut, and no mark to cut it at)
		CHECK(Lane([](FRow& R) { R.State = EState::Cut; R.Cut = FSeaMark{ 1, "North Sea", "May", { 600, 200 } }; }));
		CHECK(Lane([](FRow& R) { R.State = EState::Raided; }));

		// At most 16, own rows first.
		FBook Many = Book;
		Many.Rows.clear();
		for (int K = 0; K < 20; ++K)
		{
			FRow Row = RowOf(Book, "esp-wolfram");
			Row.Id = "enemy-" + std::to_string(K);
			Many.Rows.push_back(Row);
		}
		Many.Rows.push_back(RowOf(Book, "por-wolfram"));
		FRow Second = RowOf(Book, "por-wolfram");
		Second.Id = "own-2";
		Many.Rows.push_back(Second);
		LookAt(Many, In, Look);
		CHECK(Look.Lanes.size() == static_cast<size_t>(MostLanes));
		CHECK(Look.Lanes.size() == 16 && Look.Lanes[0].Row == 20 && Look.Lanes[1].Row == 21 && Look.Lanes[2].Row == 0 && Look.Lanes[15].Row == 13);

		// The crate.
		FRow Crate = RowOf(Book, "por-wolfram");
		CHECK(CarriesCrate(Crate));
		Crate.Cargo = FCargo{ true, 0.0, 0.0, 0.0 };
		CHECK(!CarriesCrate(Crate));
		Crate.Cargo = FCargo{ true, 0.0, 0.0, std::nullopt };
		CHECK(CarriesCrate(Crate)); // (the production unseen: something may have come)
		Crate.Cargo = FCargo{ false, 4.5, 0.0, 1.0 };
		CHECK(!CarriesCrate(Crate));
		Crate.Cargo = FCargo{ true, 0.0, 2.6, 0.0 };
		CHECK(CarriesCrate(Crate));
		Crate.State = EState::Cut;
		CHECK(!CarriesCrate(Crate));

		// The tint: |lean| from 10, strong from 25, the buyer's colour on its side, else Germany's or Britain's.
		auto Tint = [&Book, &In](double Lean, const char* Buyer, ERel Rel, const char* Open)
		{
			FBook One = Book;
			FRow Row = RowOf(Book, "por-wolfram");
			Row.Lean = Lean;
			Row.Buyer = Buyer;
			Row.Rel = Rel;
			Row.Round.reset();
			One.Rows = { Row };
			FLookIn Here = In;
			Here.OpenRow = Open;
			FLook Out;
			LookAt(One, Here, Out);
			return Out.Tints;
		};
		CHECK(Tint(9.99, "UK", ERel::Self, "").empty());
		CHECK(Tint(10.0, "UK", ERel::Self, "").size() == 1 && Near(Tint(10.0, "UK", ERel::Self, "")[0].Alpha, 0.13));
		CHECK(Near(Tint(24.9, "UK", ERel::Self, "")[0].Alpha, 0.13) && Near(Tint(25.0, "UK", ERel::Self, "")[0].Alpha, 0.22));
		CHECK(Tint(15.0, "UK", ERel::Self, "")[0].Ink == ColourOf("GER")); // (Britain holds it, but it leans to the Axis)
		CHECK(Tint(-15.0, "UK", ERel::Self, "")[0].Ink == ColourOf("UK"));
		CHECK(Tint(-40.0, "USA", ERel::Ally, "").empty()); // (not the viewer's row)
		CHECK(Tint(-40.0, "USA", ERel::Ally, "por-wolfram").size() == 1 && Tint(-40.0, "USA", ERel::Ally, "por-wolfram")[0].Ink == ColourOf("USA"));
		CHECK(Tint(40.0, "ITA", ERel::Enemy, "por-wolfram")[0].Ink == ColourOf("ITA"));
		CHECK(Tint(-40.0, "ITA", ERel::Enemy, "por-wolfram")[0].Ink == ColourOf("UK"));
		{
			FBook Two = Book;
			FRow Other = RowOf(Book, "por-wolfram");
			Other.Id = "por-azores";
			Two.Rows.push_back(Other);
			FLook Out;
			LookAt(Two, In, Out);
			CHECK(Out.Tints.size() == 1); // (one per seller)
		}

		// The seals: those that open a card, then at most 12 quiet ones, named rows first; none quiet from far off.
		{
			FBook Seals = Book;
			Seals.Rows.clear();
			auto Add = [&Seals, &Book](const std::string& Id, bool bGeneric, FRound R, bool bAgentS = false, bool bAgentB = false)
			{
				FRow Row = RowOf(Book, "esp-wolfram");
				Row.Id = Id;
				Row.bGeneric = bGeneric;
				Row.Round = R;
				Row.bAgentSeller = bAgentS;
				Row.bAgentBuyer = bAgentB;
				Seals.Rows.push_back(Row);
			};
			const std::string No = "Only a country at war bids for it.";
			for (int K = 0; K < 10; ++K)
			{
				Add("gen-" + std::to_string(K), true, Round({}, std::nullopt, No));
			}
			for (int K = 0; K < 5; ++K)
			{
				Add("named-" + std::to_string(K), false, Round({}, std::nullopt, No));
			}
			Add("may-bid", true, Round({}, std::nullopt, ""));
			Add("has-bid", false, Round({}, 1.0, "The bids are in: the seller is choosing."));
			Add("sells", false, Round({}, std::nullopt, "It is yours to sell.", true));
			Add("agent-s", false, Round({}, std::nullopt, No), true, false);
			Add("agent-b", false, Round({}, std::nullopt, No), false, true);
			Add("bids-seen", false, Round({ { "GER", 3.0 } }, std::nullopt, No)); // (a bid seen through an agent in the bidder's capital)
			FLookIn Here = In;
			Here.OpenRow = "has-bid";
			FLook Out;
			LookAt(Seals, Here, Out);
			// (agent-b, an agent in the holder's capital alone, sees no bid: its seal is a quiet one)
			CHECK(Out.Seals.size() == 5 + 12);
			int Opens = 0, Named = 0, Generic = 0, OpenOnes = 0;
			bool bOrder = true;
			for (size_t K = 0; K < Out.Seals.size(); ++K)
			{
				const FRow& Row = Seals.Rows[Out.Seals[K].Row];
				Opens += Out.Seals[K].bOpens ? 1 : 0;
				OpenOnes += Out.Seals[K].bOpen ? 1 : 0;
				if (!Out.Seals[K].bOpens)
				{
					Named += Row.bGeneric ? 0 : 1;
					Generic += Row.bGeneric ? 1 : 0;
					bOrder = bOrder && !(Row.bGeneric == false && Generic > 0);
				}
				else
				{
					bOrder = bOrder && Named + Generic == 0;
				}
			}
			CHECK(Opens == 5 && Named == 6 && Generic == 6 && bOrder && OpenOnes == 1);
			// The card's own seal first, then the rows the viewer is in or sees bids on, then those it may bid on.
			CHECK(Out.Seals.size() >= 5 && Seals.Rows[Out.Seals[0].Row].Id == "has-bid" && Out.Seals[0].bOpen);
			CHECK(Out.Seals.size() >= 5 && Seals.Rows[Out.Seals[1].Row].Id == "sells" && Seals.Rows[Out.Seals[2].Row].Id == "agent-s" && Seals.Rows[Out.Seals[3].Row].Id == "bids-seen" && Seals.Rows[Out.Seals[4].Row].Id == "may-bid");
			Here.bFar = true;
			LookAt(Seals, Here, Out);
			CHECK(Out.Seals.size() == 5 && Out.Plates.empty() && Out.Trays.size() == 1);
		}
		// At most MostSeals in all, every one of them in reach of the layer's slots: rows the viewer is in before those it
		// may only bid on, and the quiet ones within what is left.
		{
			FBook Seals = Book;
			Seals.Rows.clear();
			for (int K = 0; K < 70; ++K)
			{
				FRow Row = RowOf(Book, "esp-wolfram");
				Row.Id = "row-" + std::to_string(K);
				Row.Round = Round({}, std::nullopt, K % 7 == 3 ? "Only a country at war bids for it." : "");
				if (K % 10 == 9)
				{
					Row.Round->Mine = 1.0;
				}
				Seals.Rows.push_back(Row);
			}
			FLook Out;
			LookAt(Seals, In, Out);
			CHECK(Out.Seals.size() == static_cast<size_t>(MostSeals));
			int Mine = 0;
			for (size_t K = 0; K < Out.Seals.size(); ++K)
			{
				Mine += Seals.Rows[Out.Seals[K].Row].Round->Mine ? 1 : 0;
				CHECK(K >= 7 || Seals.Rows[Out.Seals[K].Row].Round->Mine);
				CHECK(Out.Seals[K].bOpens);
			}
			CHECK(Mine == 7);
		}
		// The card opens where a bid is seen through an agent in a bidder's capital (the United States, not at war, with an
		// agent in Berlin, Germany bidding for wolfram Britain holds); an agent in the holder's capital alone opens none.
		{
			FBook Usa = Book;
			Usa.Viewer = "USA";
			FRow Row = RowOf(Book, "por-wolfram");
			Row.Rel = ERel::Ally;
			Row.Buyer = "UK";
			Row.bAgentSeller = false;
			Row.bAgentBuyer = false;
			Row.Round = Round({ { "GER", 3.0 } }, std::nullopt, "Only a country at war bids for it.");
			Usa.Rows = { Row };
			FLook Out;
			LookAt(Usa, In, Out);
			CHECK(OpensCard(Row) && Out.Seals.size() == 1 && Out.Seals[0].bOpens);
			CHECK_TEXT(SealTip(Row), "Portuguese wolfram: bids this month. Click to see the bids.");
			FCard Card;
			CHECK(LayCard(Row, Usa, KitMeasure, 1.0, Card));
			std::vector<FCardLine> Lines;
			CardLines(Row, Usa, Lines);
			CHECK(Lines.size() == 2 && Lines[0].Kind == ELine::Seen && Lines[0].Nat == "GER" && Lines[0].Figure == "300" && Lines[1].Kind == ELine::Others);
			Row.Round->Bids.clear();
			Row.bAgentBuyer = true;
			CHECK(!OpensCard(Row));
			CHECK_TEXT(SealTip(Row), "Portuguese wolfram: bids this month. Only a country at war bids for it.");
		}
		// Plates for every viewer, trays only for a legion with a corps.
		{
			FBook Away = Book;
			Away.Viewer = "USA";
			Away.Legions[0].Corps = -1;
			Away.Legions[0].Place.clear();
			FLook Out;
			LookAt(Away, In, Out);
			CHECK(Out.Plates.size() == 1 && Out.Plates[0].Line == "1 division abroad" && Out.Trays.empty());
		}
		// The tray's pieces only for a corps the viewer's picture shows as a legion: a Soviet viewer, at war with the patron,
		// whose view has the Blue Division's corps not identified (no vol) gets no flag and no strength, but the sender's
		// plate line, which is public, stays. Seen exactly (vol sent), the tray is there; a vol of another home is not it.
		{
			FBook Enemy = Book;
			Enemy.Viewer = "SOV";
			FLookIn Fog = In;
			Fog.SeesLegion = [](int, std::string_view) { return false; };
			FLook Out;
			LookAt(Enemy, Fog, Out);
			CHECK(Out.Trays.empty() && Out.Plates.size() == 1);
			Fog.SeesLegion = nullptr;
			LookAt(Enemy, Fog, Out);
			CHECK(Out.Trays.empty());
			Fog.SeesLegion = [](int Corps, std::string_view Home) { return Corps == 363 && Home == "ESP"; };
			LookAt(Enemy, Fog, Out);
			CHECK(Out.Trays.size() == 1 && Out.Trays[0].Corps == 363 && !Out.Trays[0].Legion.empty()); // (the legion line goes with the flag and the Courier line)
			Fog.SeesLegion = [](int Corps, std::string_view Home) { return Corps == 363 && Home == "POR"; };
			std::vector<FTrayLook> Trays;
			TraysOf(Enemy, Fog, Trays);
			CHECK(Trays.empty());
		}
		// Off: nothing.
		{
			FBook Off;
			FLook Out;
			LookAt(Off, In, Out);
			CHECK(Out.Lanes.empty() && Out.Tints.empty() && Out.Seals.empty() && Out.Plates.empty() && Out.Trays.empty());
		}
	}

	void TestOpenCard()
	{
		FBook Book = BritainsSeat();
		FRow Mine = RowOf(Book, "por-wolfram");
		Mine.Id = "swe-ore";
		Mine.Seller = "UK";
		Mine.Round = Round({ { "GER", 3.0 } }, std::nullopt, "It is yours to sell.", true);
		Book.Rows.push_back(Mine);
		FRow Quiet = RowOf(Book, "esp-wolfram");
		Quiet.Round = Round({}, std::nullopt, "Only a country at war bids for it.");
		Book.Rows.push_back(Quiet);
		FOpenCard Open;
		CHECK(Open.Update(Book) && Open.Row() == "swe-ore"); // (the seller's own round opens itself)
		CHECK(Open.Shown(true) && !Open.Shown(false));
		CHECK(Open.Press(Book, "por-wolfram") && Open.Row() == "por-wolfram"); // (one at a time)
		CHECK(!Open.Press(Book, "esp-wolfram") && Open.Row() == "por-wolfram"); // (a quiet seal opens nothing)
		CHECK(Open.Press(Book, "por-wolfram") && !Open.IsOpen()); // (its own seal closes it)
		CHECK(!Open.Update(Book) && !Open.IsOpen()); // (once a month: closed, it does not reopen itself)
		CHECK(Open.Press(Book, "por-wolfram"));
		FBook Ended = Book;
		Ended.Rows[0].Round.reset();
		CHECK(Open.Update(Ended) && !Open.IsOpen()); // (its round ended)
		CHECK(Open.Press(Book, "por-wolfram"));
		FBook Next = Book;
		Next.Turn = Book.Turn + 1;
		CHECK(Open.Update(Next) && Open.Row() == "swe-ore"); // (a new month closes it; the seller's round opens again)
		Next.Rows.pop_back();
		Next.Rows.pop_back();
		Next.Turn += 1;
		CHECK(Open.Update(Next) && !Open.IsOpen());
		CHECK(!Open.Press(Next, "nothing"));
	}

	void TestCardLines()
	{
		const FBook Book = BritainsSeat();
		std::vector<FCardLine> Lines;
		CardLines(RowOf(Book, "por-wolfram"), Book, Lines);
		CHECK(Lines.size() == 3);
		CHECK(Lines.size() == 3 && Lines[0].Kind == ELine::Own && Lines[0].Nat == "UK" && Lines[0].Figure == "150" && Lines[0].bSteps);
		CHECK(Lines.size() == 3 && Lines[1].Kind == ELine::Seen && Lines[1].Nat == "GER" && Lines[1].Figure == "200" && !Lines[1].bSteps && !Lines[1].bAccept);
		CHECK(Lines.size() == 3 && Lines[2].Kind == ELine::Others && Lines[2].Figure == "?");
		// The seller sees every bid, the highest first, each with ACCEPT; no own row, no Others.
		FRow Sell = RowOf(Book, "por-wolfram");
		FBook Seller = Book;
		Seller.Viewer = "SWE";
		Sell.Seller = "SWE";
		Sell.Rel = ERel::Other;
		Sell.Round = Round({ { "GER", 5.18 }, { "HUN", 0.52 }, { "ITA", 1.69 }, { "NOR", 0.53 }, { "ROM", 0.83 }, { "UK", 1.5 } }, std::nullopt, "It is yours to sell.", true);
		Sell.Round->Steps.clear();
		CardLines(Sell, Seller, Lines);
		std::string Order;
		bool bAll = true;
		for (const FCardLine& Line : Lines)
		{
			Order += Line.Nat + " ";
			bAll = bAll && Line.Kind == ELine::Seen && Line.bAccept;
		}
		CHECK_TEXT(Order, "GER ITA UK ROM NOR HUN ");
		CHECK(bAll);
		// An agent only: the bids it sees, and the others.
		FRow Agent = RowOf(Book, "por-wolfram");
		Agent.Round = Round({ { "GER", 2.0 } }, std::nullopt, "Only a country at war bids for it.");
		Agent.bAgentBuyer = true;
		CardLines(Agent, Book, Lines);
		CHECK(Lines.size() == 2 && Lines[0].Kind == ELine::Seen && Lines[1].Kind == ELine::Others);
		Agent.bAgentSeller = true;
		CardLines(Agent, Book, Lines);
		CHECK(Lines.size() == 1 && Lines[0].Kind == ELine::Seen); // (an agent in the seller's capital sees them all)
		// Has bid, the round closed: the own row without its steps.
		FRow Closed = RowOf(Book, "por-wolfram");
		Closed.Round->CanBid = "The bids are in: the seller is choosing.";
		CardLines(Closed, Book, Lines);
		CHECK(Lines.size() == 3 && Lines[0].Kind == ELine::Own && !Lines[0].bSteps);
		// May bid, has not: an en dash.
		FRow Fresh = RowOf(Book, "por-wolfram");
		Fresh.Round->Mine.reset();
		Fresh.Round->Bids = { { "GER", 2.0 } };
		CardLines(Fresh, Book, Lines);
		CHECK(Lines.size() == 3 && Lines[0].Figure == "\xE2\x80\x93" && Lines[0].bSteps);

		// One step each way; with no bid + sends the first step and - cannot be pressed; nothing past the ends.
		FRound R = Round({}, std::nullopt, "");
		double Pp = -1.0;
		CHECK(StepUp(R, Pp) && Near(Pp, 0.5));
		CHECK(!StepDown(R, Pp));
		R.Mine = 0.5;
		CHECK(!StepDown(R, Pp) && StepUp(R, Pp) && Near(Pp, 1.0));
		R.Mine = 2.5;
		CHECK(!StepUp(R, Pp) && StepDown(R, Pp) && Near(Pp, 2.0));
		R.Mine = 1.25;
		CHECK(StepUp(R, Pp) && Near(Pp, 1.5) && StepDown(R, Pp) && Near(Pp, 1.0));
		R.Steps.clear();
		CHECK(!StepUp(R, Pp) && !StepDown(R, Pp));
	}

	/** The card's months ring (a hollow round box 34 across), or nullptr. */
	const FDraw* RingOf(const FCard& Card, double Px)
	{
		const FDraw* Ring = nullptr;
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Box && Near(Draw.At.W, 34.0 * Px) && Near(Draw.At.H, 34.0 * Px))
			{
				Ring = &Draw;
			}
		}
		return Ring;
	}

	/**
	 * A round of three months (the code path kept for longer rounds): the ring shows 3 over MO, and the two stand centred
	 * in it by their ink (call 1): the figure's 0.59 em and the capitals' 0.58 em over baselines an ascender under each
	 * box's top; the numeral's ink middle 12 over MO's; across, on the ring's middle.
	 */
	void CheckRingAt(double Px)
	{
		FBook Book = BritainsSeat();
		Book.RoundLen = 3;
		const FRow& Row = RowOf(Book, "por-wolfram");
		CHECK(RingShown(*Row.Round, Book) && RingMonths(*Row.Round, Book) == 3);
		FCard Card;
		CHECK(LayCard(Row, Book, KitMeasure, Px, Card));
		const double OX = 11.0 * Px, OY = 9.0 * Px;
		const FDraw* Ring = RingOf(Card, Px);
		CHECK(Ring && Near(Ring->Fill.A, 1.0 / 255.0, 1e-9) && Ring->Edge.Srgb == Kit::Brass700 && Near(Ring->EdgeW, 2.0 * Px) && Near(Ring->Radius, 17.0 * Px));
		CHECK(Ring && Near(Ring->At.Right(), OX + 208.0 * Px) && Near(Ring->At.Y, OY));
		const FDraw* Months = TextNamed(Card, "3");
		const FDraw* Mo = TextNamed(Card, "MO");
		CHECK(Months && Mo && Ring && Inside(Months->At, Ring->At) && Inside(Mo->At, Ring->At));
		CHECK(Months && Near(Months->Font.Size, 14.0 * Px) && Months->Font.Family == EFamily::Courier && Months->Font.Style == EStyle::Bold && Mo && Near(Mo->Font.Size, 9.0 * Px));
		if (!Months || !Mo || !Ring)
		{
			return;
		}
		// (the ink as the kit's fonts set it: font_widths' own ascender, the figures' and the capitals' heights)
		CHECK(Near(Ink::CourierAscent, FontWidths::CourierBold.Ascent, 1e-5));
		CHECK(Near(Ink::CourierFigureH, FontWidths::CourierBold.FigureH, 0.005) && Near(Ink::CourierCapH, FontWidths::CourierBold.CapH, 0.005));
		const double FigureTop = Months->At.Y + (FontWidths::CourierBold.Ascent - FontWidths::CourierBold.FigureH) * Months->Font.Size;
		const double FigureMid = Months->At.Y + (FontWidths::CourierBold.Ascent - FontWidths::CourierBold.FigureH * 0.5) * Months->Font.Size;
		const double MoMid = Mo->At.Y + (FontWidths::CourierBold.Ascent - FontWidths::CourierBold.CapH * 0.5) * Mo->Font.Size;
		const double MoBottom = Mo->At.Y + FontWidths::CourierBold.Ascent * Mo->Font.Size;
		CHECK(Near(MoMid - FigureMid, 12.0 * Px, 0.1 * Px));
		CHECK(Near((FigureTop + MoBottom) * 0.5, Ring->At.Centre().Y, 0.1 * Px)); // (the two as one, centred by their ink)
		CHECK(Near(Months->At.Centre().X, Ring->At.Centre().X, 1e-6) && Near(Mo->At.Centre().X, Ring->At.Centre().X, 1e-6));
		// (by its box, as before, the numeral stood a pixel high at 14: its ink middle about 0.075 em over the box's)
		const double BoxMid = Months->At.Y + Months->At.H * 0.5;
		CHECK(FigureMid < BoxMid - 0.07 * Months->Font.Size && FigureMid > BoxMid - 0.08 * Months->Font.Size);
		// (and nothing else on the card moves with the ring: the held line and the rule where the contract has them)
		const FDraw* Held = TextNamed(Card, "held by Britain, 5 of 12 months");
		CHECK(Held && Near(Held->At.Y, OY + 38.0 * Px));
	}

	void CheckCardAt(double Px)
	{
		const FBook Book = BritainsSeat();
		FCard Card;
		CHECK(LayCard(RowOf(Book, "por-wolfram"), Book, KitMeasure, Px, Card));
		const double OX = 11.0 * Px, OY = 9.0 * Px;
		CHECK(Near(Card.Size.X, 230.0 * Px));
		CHECK(Card.Rows == 3 && Card.More == 0);
		CHECK(Card.Draws.size() > 2);
		// The frame first: paper-200, radius 2, and no drop shadow but the date card's 1 px dark keyline (call 4): its
		// colour, its 0.9 and its width of 1.0, which is not scaled (the date card's is not).
		const FDraw& Frame = Card.Draws[0];
		CHECK(Frame.Kind == EDraw::Box && Frame.Fill.Srgb == Kit::Paper200 && Near(Frame.Fill.A, 1.0) && Near(Frame.Radius, 2.0 * Px));
		CHECK(Frame.Edge.Srgb == Kit::Keyline && Near(Frame.Edge.A, 0.9, 1e-6) && Near(Frame.EdgeW, 1.0));
		CHECK(Near(Frame.At.X, 0.0) && Near(Frame.At.Y, 0.0) && Near(Frame.At.W, Card.Size.X) && Near(Frame.At.H, Card.Size.Y));
		for (const FDraw& Draw : Card.Draws)
		{
			CHECK(!(Draw.Kind == EDraw::Box && Draw.Fill.Srgb == Kit::Black && Draw.Fill.A > 0.0f)); // (no shadow box of any kind)
			CHECK(Draw.Kind != EDraw::Box || Draw.At.Y >= 0.0); // (nothing hangs under the card as a shadow did)
		}
		// The head.
		const FDraw* Sign = nullptr;
		for (const FDraw& Draw : Card.Draws)
		{
			Sign = Draw.Kind == EDraw::Sign ? &Draw : Sign;
		}
		CHECK(Sign && Sign->Key == "goods-wolfram" && Sign->Fill.Srgb == Kit::Brass700 && Near(Sign->At.X, OX) && Near(Sign->At.Y, OY + 6.0 * Px) && Near(Sign->At.W, 22.0 * Px) && Near(Sign->At.H, 22.0 * Px));
		const FDraw* Caps_ = TextNamed(Card, "WOLFRAM");
		CHECK(Caps_ && Near(Caps_->At.X, OX + 28.0 * Px) && Caps_->Font.Family == EFamily::Courier && Caps_->Font.Style == EStyle::Bold && Near(Caps_->Font.Size, 12.0 * Px) && Caps_->Font.Spacing == 80);
		const FDraw* Seller = TextNamed(Card, "Portugal");
		CHECK(Seller && Near(Seller->At.X, OX + 28.0 * Px) && Near(Seller->At.Y, OY + 17.0 * Px) && Seller->Font.Family == EFamily::Serif && Near(Seller->Font.Size, 12.0 * Px));
		// A round is a month: the ring would only ever say 1, and is not drawn (call 2); the held line tells the time.
		CHECK(!RingShown(*RowOf(Book, "por-wolfram").Round, Book) && RingMonths(*RowOf(Book, "por-wolfram").Round, Book) == 1);
		CHECK(!RingOf(Card, Px) && !TextNamed(Card, "1") && !TextNamed(Card, "MO"));
		CheckRingAt(Px);
		const FDraw* Held = TextNamed(Card, "held by Britain, 5 of 12 months");
		CHECK(Held && Near(Held->At.X, OX) && Near(Held->At.Y, OY + 38.0 * Px) && Held->Font.Style == EStyle::Italic && Near(Held->Font.Size, 12.0 * Px) && Held->Fill.Srgb == 0x4A3F30);
		bool bRule = false;
		for (const FDraw& Draw : Card.Draws)
		{
			bRule = bRule || (Draw.Kind == EDraw::Box && Near(Draw.At.Y, OY + 60.0 * Px) && Near(Draw.At.W, 208.0 * Px) && Near(Draw.At.H, 1.0 * Px) && Draw.Fill.Srgb == Kit::Brass700);
		}
		CHECK(bRule);
		// The rows: flags 21 x 14 at x 0, names at x 27, figures in one column ending at 161 (the steps' card).
		std::vector<const FDraw*> Flags, Figures;
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Flag || (Draw.Kind == EDraw::Box && Near(Draw.At.W, 21.0 * Px) && Near(Draw.At.H, 14.0 * Px)))
			{
				Flags.push_back(&Draw);
			}
			if (Draw.Kind == EDraw::Text && Draw.Font.Family == EFamily::Courier && Near(Draw.Font.Size, 18.0 * Px))
			{
				Figures.push_back(&Draw);
			}
		}
		CHECK(Flags.size() == 3 && Figures.size() == 3);
		for (size_t K = 0; K < Flags.size() && K < Figures.size(); ++K)
		{
			const double Mid = OY + (66.0 + 26.0 * static_cast<double>(K) + 11.0) * Px;
			CHECK(Near(Flags[K]->At.X, OX) && Near(Flags[K]->At.W, 21.0 * Px) && Near(Flags[K]->At.H, 14.0 * Px) && Near(Flags[K]->At.Centre().Y, Mid));
			CHECK(Near(Figures[K]->At.Right(), OX + 161.0 * Px) && Near(Figures[K]->At.Centre().Y, Mid));
		}
		CHECK(Flags.size() == 3 && Flags[0]->Key == "UK" && Flags[1]->Key == "GER_PLAIN" && Flags[2]->Kind == EDraw::Box);
		// "Others ?": a flag-shaped grey silhouette (call 3), the art director's desk-500 at 50%, filled, no white, no outline.
		if (Flags.size() == 3)
		{
			const FDraw& Blank = *Flags[2];
			CHECK(Blank.Fill.Srgb == Kit::Desk500 && Near(Blank.Fill.A, 0.5, 1e-6) && Near(Blank.EdgeW, 0.0) && Near(Blank.Edge.A, 0.0) && Near(Blank.Radius, 0.0));
			CHECK(Blank.Fill.Srgb != 0xFFFFFF && Blank.Fill.Srgb != Kit::Paper100 && Blank.Fill.A > Kit::Hollow);
			// (and the kit's dark ramp it stands on: past desk-600 3D3125, darker than paper, grey-brown)
			CHECK((Kit::Desk500 >> 16) > 0x3D && (Kit::Desk500 >> 16) < 0x80 && Kit::Desk500 == 0x4B3D2D);
		}
		CHECK(Figures.size() == 3 && Figures[0]->Text == "150" && Figures[1]->Text == "200" && Figures[2]->Text == "?"); // (money as the game shows it: points x 100)
		CHECK(Figures.size() == 3 && Figures[0]->Fill.Srgb == Kit::Ink900 && Figures[2]->Fill.Srgb == 0x7A6A52);
		const FDraw* Own = TextNamed(Card, "Britain");
		const FDraw* Yours = TextNamed(Card, ", yours");
		CHECK(Own && Yours && Own->Font.Style == EStyle::Bold && Yours->Font.Style == EStyle::Regular && Near(Own->At.X, OX + 27.0 * Px) && Near(Yours->At.X, Own->At.Right()));
		const FDraw* Others = TextNamed(Card, "Others");
		CHECK(Others && Near(Others->At.X, OX + 27.0 * Px));
		const FDraw* Germany = TextNamed(Card, "Germany");
		CHECK(Germany && Near(Germany->At.X, OX + 27.0 * Px));
		// The steps on the own row: - at 167-186 and + at 189-208, 19 x 19, paper-300, brass edge, Courier bold 14.
		CHECK(Card.Buttons.size() == 2);
		if (Card.Buttons.size() == 2)
		{
			const FButton& Minus_ = Card.Buttons[0];
			const FButton& Plus = Card.Buttons[1];
			const double Mid = OY + 77.0 * Px;
			CHECK(Minus_.Act == EAct::Minus && Minus_.bLive && Near(Minus_.Pp, 1.0) && Near(Minus_.At.X, OX + 167.0 * Px) && Near(Minus_.At.W, 19.0 * Px) && Near(Minus_.At.H, 19.0 * Px) && Near(Minus_.At.Centre().Y, Mid));
			CHECK(Plus.Act == EAct::Plus && Plus.bLive && Near(Plus.Pp, 2.0) && Near(Plus.At.X, OX + 189.0 * Px) && Near(Plus.At.Right(), OX + 208.0 * Px));
			const FDraw* MinusLabel = TextNamed(Card, "\xE2\x88\x92");
			const FDraw* PlusLabel = TextNamed(Card, "+");
			CHECK(MinusLabel && PlusLabel && Inside(MinusLabel->At, Minus_.At, 0.5) && Inside(PlusLabel->At, Plus.At, 3.0 * Px) && Near(PlusLabel->Font.Size, 14.0 * Px) && PlusLabel->Font.Style == EStyle::Bold);
			int StepBoxes = 0;
			for (const FDraw& Draw : Card.Draws)
			{
				if (Draw.Kind == EDraw::Box && Near(Draw.At.W, 19.0 * Px) && Draw.Fill.Srgb == Kit::Paper300 && Draw.Edge.Srgb == Kit::Brass700 && Near(Draw.Fill.A, 1.0))
				{
					++StepBoxes;
				}
			}
			CHECK(StepBoxes == 2);
		}
		// The hint, Serif 11 in 5B4E3C: how the rivals' bids are seen.
		const std::vector<const FDraw*> Hint = TextsInFace(Card, HintFace(Px), Kit::HintInk);
		std::string Joined;
		for (const FDraw* Line : Hint)
		{
			Joined += (Joined.empty() ? "" : " ") + Line->Text;
			CHECK(Line->At.Right() <= OX + 208.0 * Px + 1e-6);
		}
		CHECK_TEXT(Joined, "Rival bids show with an agent in Lisbon, or in the bidder's capital.");
		CHECK(Card.HintLines == 2 && Hint.size() == 2 && Near(Hint[0]->At.X, OX) && Hint[0]->At.Y >= OY + (66.0 + 26.0 * 2.0 + 22.0) * Px);
		// Everything on the card stands inside it; boxes are filled with their real colour (none culled).
		const FRect Whole{ 0.0, 0.0, Card.Size.X, Card.Size.Y };
		for (size_t K = 0; K < Card.Draws.size(); ++K)
		{
			const FDraw& Draw = Card.Draws[K];
			CHECK(Inside(Draw.At, Whole));
			if (Draw.Kind == EDraw::Box)
			{
				CHECK(Draw.Fill.A >= Kit::Hollow - 1e-9);
			}
		}
		// The card about 210 x 160 in the picture: here 230 wide with its edge and three rows (the picture had two).
		CHECK(Card.Size.Y > 150.0 * Px && Card.Size.Y < 210.0 * Px);
	}

	void TestCard()
	{
		CheckCardAt(1.0);
		CheckCardAt(1.08);
		// Every box at 1.08 is the one at 1 scaled: the card is laid in picture pixels times Px.
		const FBook Book = BritainsSeat();
		FCard One, Big;
		LayCard(RowOf(Book, "por-wolfram"), Book, KitMeasure, 1.0, One);
		LayCard(RowOf(Book, "por-wolfram"), Book, KitMeasure, 1.08, Big);
		bool bScaled = One.Draws.size() == Big.Draws.size();
		for (size_t K = 0; bScaled && K < One.Draws.size(); ++K)
		{
			const FRect& A = One.Draws[K].At;
			const FRect& B = Big.Draws[K].At;
			bScaled = Near(A.X * 1.08, B.X, 1e-6) && Near(A.Y * 1.08, B.Y, 1e-6) && Near(A.W * 1.08, B.W, 1e-6) && Near(A.H * 1.08, B.H, 1e-6);
		}
		CHECK(bScaled && Near(One.Size.Y * 1.08, Big.Size.Y, 1e-6));

		// The seller's card: every bid with ACCEPT (62 x 19 at 146-208), figures ending at 140, the foot "Sell to nobody".
		FBook Seller = Book;
		Seller.Viewer = "SWE";
		FRow Sell = RowOf(Book, "por-wolfram");
		Sell.Id = "swe-ore";
		Sell.Seller = "SWE";
		Sell.SellerName = "Sweden";
		Sell.Sign = "ore";
		Sell.Rel = ERel::Other;
		Sell.Buyer = "GER";
		Sell.Round = Round({ { "GER", 5.18 }, { "HUN", 0.52 }, { "ITA", 1.69 }, { "NOR", 0.53 }, { "ROM", 0.83 }, { "UK", 1.5 } }, std::nullopt, "It is yours to sell.", true);
		Seller.Nations.push_back(Nation("HUN", "Hungary", "Hungarian", 1));
		Seller.Nations.push_back(Nation("NOR", "Norway", "Norwegian", 0));
		Seller.Nations.push_back(Nation("ROM", "Romania", "Romanian", 1));
		FCard Card;
		LayCard(Sell, Seller, KitMeasure, 1.0, Card);
		CHECK(Card.Rows == 6 && Card.More == 0 && Card.Buttons.size() == 7);
		int Accepts = 0;
		for (const FButton& Button : Card.Buttons)
		{
			if (Button.Act == EAct::Accept)
			{
				++Accepts;
				CHECK(Button.bLive && Near(Button.At.X, 11.0 + 146.0) && Near(Button.At.W, 62.0) && Near(Button.At.H, 19.0));
			}
		}
		CHECK(Accepts == 6 && Card.Buttons[0].Buyer == "GER" && Near(Card.Buttons[0].Pp, 5.18) && Card.Buttons[2].Buyer == "UK");
		CHECK(Card.Buttons.back().Act == EAct::Nobody && Near(Card.Buttons.back().At.Right(), 11.0 + 208.0));
		CHECK(TextNamed(Card, "SELL TO NOBODY") && TextNamed(Card, "It is yours to sell.") && !TextNamed(Card, "Others") && TextNamed(Card, "ACCEPT"));
		// ACCEPT is the kit's own button: desk-700, brass-700 edge, corners 4, its label paper-100 in Courier Prime bold 11
		// spaced 100; the foot is the kit's quiet button on paper: ink-600 capitals over a brass-700 line 1 high.
		int KitBoxes = 0;
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Box && Near(Draw.At.W, 62.0) && Near(Draw.At.H, 19.0))
			{
				++KitBoxes;
				CHECK(Draw.Fill.Srgb == Kit::Desk700 && Near(Draw.Fill.A, 1.0) && Draw.Edge.Srgb == Kit::Brass700 && Near(Draw.EdgeW, 1.0) && Near(Draw.Radius, 4.0));
			}
			if (Draw.Kind == EDraw::Text && Draw.Text == "ACCEPT")
			{
				CHECK(Draw.Fill.Srgb == Kit::Paper100 && Draw.Font.Family == EFamily::Courier && Draw.Font.Style == EStyle::Bold && Near(Draw.Font.Size, 11.0) && Draw.Font.Spacing == 100);
			}
		}
		CHECK(KitBoxes == 6);
		if (const FDraw* Foot = TextNamed(Card, "SELL TO NOBODY"))
		{
			CHECK(Foot->Fill.Srgb == Kit::Ink600 && Near(Foot->At.Right(), 11.0 + 208.0) && Foot->Font.Spacing == 100);
			bool bLine = false;
			for (const FDraw& Draw : Card.Draws)
			{
				bLine = bLine || (Draw.Kind == EDraw::Box && Draw.Fill.Srgb == Kit::Brass700 && Near(Draw.At.H, 1.0) && Near(Draw.At.X, Foot->At.X) && Near(Draw.At.W, Foot->At.W) && Near(Draw.At.Y, Foot->At.Bottom() + 1.0));
			}
			CHECK(bLine && Near(Card.Size.Y, Foot->At.Bottom() + 2.0 + 8.0 + 1.0));
		}
		for (const FDraw& Draw : Card.Draws)
		{
			if (Draw.Kind == EDraw::Text && Draw.Font.Family == EFamily::Courier && Near(Draw.Font.Size, 18.0))
			{
				CHECK(Near(Draw.At.Right(), 11.0 + 140.0));
			}
		}
		CHECK(TextNamed(Card, "518") && TextNamed(Card, "52") && TextNamed(Card, "53")); // (5.18, 0.52 and 0.53 as the game shows money)
		// Eight bids: six rows, then "+2 more".
		Sell.Round->Bids.push_back({ "USA", 0.6 });
		Sell.Round->Bids.push_back({ "SOV", 0.7 });
		LayCard(Sell, Seller, KitMeasure, 1.0, Card);
		CHECK(Card.Rows == 6 && Card.More == 2 && TextNamed(Card, "+2 more"));
		// No bids yet.
		Sell.Round->Bids.clear();
		LayCard(Sell, Seller, KitMeasure, 1.0, Card);
		CHECK(Card.Rows == 0 && TextNamed(Card, "No bids yet.") && Card.Buttons.size() == 1 && Card.Buttons[0].Act == EAct::Nobody && TextNamed(Card, "SELL TO NOBODY"));

		// Why the viewer may not bid leads the hint, two lines at most.
		FRow Cannot = RowOf(Book, "por-wolfram");
		Cannot.Round = Round({ { "GER", 2.0 } }, std::nullopt, "No way is open for the goods between Switzerland and the United Kingdom, and none will be while the sea is closed to them and the passes are shut.");
		Cannot.bAgentBuyer = true;
		LayCard(Cannot, Book, KitMeasure, 1.0, Card);
		const std::vector<const FDraw*> Hint = TextsInFace(Card, HintFace(1.0), Kit::HintInk);
		CHECK(Hint.size() == 4 && Card.HintLines == 4);
		CHECK(Hint.size() == 4 && Hint[0]->Text.rfind("No way is open", 0) == 0 && Hint[2]->Text.rfind("Rival bids show", 0) == 0);
		CHECK(Card.Buttons.empty() && !TextNamed(Card, ", yours"));
		// A name and its ", yours" in the room left of the figure (161 - "5" 10.8 - 27 - 4 = 119.2 at Px 1): both where they
		// fit ("Soviet Union" 73.9 + 35.7); the bold name alone where only it fits ("Kingdom of Greece" 109.3); else the
		// tag. A seen bidder's name too long for its row: its tag.
		FBook Long = Book;
		Long.Nations.push_back(Nation("XXL", "The Most Serene Republic of Somewhere", "Serene", 0));
		Long.Nations.push_back(Nation("GRE", "Kingdom of Greece", "Greek", -1));
		Long.Nations.push_back(Nation("USR", "Union of Soviet Socialist Republics", "Soviet", -1));
		FRow Far = RowOf(Book, "por-wolfram");
		Far.Round->Bids = { { "XXL", 0.05 } };
		Far.Round->Mine = 0.05; // (shown as 5)
		Long.Viewer = "SOV";
		LayCard(Far, Long, KitMeasure, 1.0, Card);
		CHECK(TextNamed(Card, "Soviet Union") && TextNamed(Card, ", yours") && TextNamed(Card, "XXL") && !TextNamed(Card, "The Most Serene Republic of Somewhere"));
		Long.Viewer = "GRE";
		LayCard(Far, Long, KitMeasure, 1.0, Card);
		CHECK(TextNamed(Card, "Kingdom of Greece") && !TextNamed(Card, ", yours"));
		Long.Viewer = "USR";
		LayCard(Far, Long, KitMeasure, 1.0, Card);
		CHECK(TextNamed(Card, "USR") && !TextNamed(Card, ", yours") && !TextNamed(Card, "Union of Soviet Socialist Republics"));
		for (const FDraw& Draw : Card.Draws)
		{
			CHECK(Draw.Kind != EDraw::Text || Draw.Font.Family != EFamily::Serif || Draw.At.Right() <= 11.0 + 208.0 + 1e-6);
		}
		FRow NoRound = RowOf(Book, "esp-wolfram");
		CHECK(!LayCard(NoRound, Book, KitMeasure, 1.0, Card) && Card.Draws.empty());
	}

	/** The pin is level with the capital, runs from the card's near side, and stops PinShort x Px short of it. */
	bool PinLevel(const FPlaced& Placed, FPt Capital, double Px)
	{
		const bool bSide = Placed.Spot == 0 ? Near(Placed.PinFrom.X, Placed.Card.Right()) && Placed.PinTo.X > Placed.PinFrom.X : Near(Placed.PinFrom.X, Placed.Card.X) && Placed.PinTo.X < Placed.PinFrom.X;
		return Placed.bPin && bSide && Near(Placed.PinFrom.Y, Capital.Y) && Near(Placed.PinTo.Y, Capital.Y) && Near(Distance(Placed.PinTo, Capital), PinShort * Px) && Near(Distance(Placed.PinFrom, Capital), CardFrom * Px);
	}

	/** The card keeps KeepOff x Px off the screen's edges and every frame piece. */
	bool KeepsOff(const FRect& Card, const FRect& Screen, const std::vector<FRect>& Frame, double Px)
	{
		const double M = KeepOff * Px - 1e-9;
		bool bOk = Card.X >= Screen.X + M && Card.Y >= Screen.Y + M && Card.Right() <= Screen.Right() - M && Card.Bottom() <= Screen.Bottom() - M;
		for (const FRect& Piece : Frame)
		{
			bOk = bOk && (Card.Right() <= Piece.X - M || Card.X >= Piece.Right() + M || Card.Bottom() <= Piece.Y - M || Card.Y >= Piece.Bottom() + M);
		}
		return bOk;
	}

	void TestPlace()
	{
		const std::vector<FRect> NoRects;
		FPlaceIn In;
		In.Capital = FPt{ 720, 450 };
		In.Size = FPt{ 230, 180 };
		In.Screen = FRect{ 0, 0, 1440, 900 };
		In.Frame = &NoRects;
		CHECK(Near(KeepOff, 16.0) && Near(CardFrom, 28.0) && Near(PinInset, 12.0));
		// The two spots: 28 from the capital, level with it.
		CHECK(Near(SpotBox(0, In.Capital, In.Size, 1.0).Right(), 720.0 - 28.0) && Near(SpotBox(0, In.Capital, In.Size, 1.0).Centre().Y, 450.0));
		CHECK(Near(SpotBox(1, In.Capital, In.Size, 1.0).X, 720.0 + 28.0) && Near(SpotBox(1, In.Capital, In.Size, 1.0).Centre().Y, 450.0));
		CHECK(Near(SpotBox(1, In.Capital, In.Size, 1.08).X, 720.0 + 28.0 * 1.08));
		// Room on both sides: the left, level with the capital, its pin level.
		FPlaced Placed = PlaceCard(In);
		CHECK(Placed.Spot == 0 && Placed.bFits && Near(Placed.Intrusion, 0.0) && Near(Placed.Card.Right(), 692.0) && Near(Placed.Card.Centre().Y, 450.0));
		CHECK(PinLevel(Placed, In.Capital, 1.0) && Near(PinW, 1.2));

		// The left side, as far over as it may stand: 16 off the screen's edge fits; a hair nearer does not, and the card
		// is set on the right with its pin mirrored.
		for (double Px : { 1.0, 1.08 })
		{
			FPlaceIn Edge = In;
			Edge.Px = Px;
			Edge.Size = FPt{ 230.0 * Px, 180.0 * Px };
			Edge.Capital = FPt{ (16.0 + 28.0) * Px + Edge.Size.X, 450.0 };
			Placed = PlaceCard(Edge);
			CHECK(Placed.Spot == 0 && Placed.bFits && Near(Placed.Card.X, 16.0 * Px) && PinLevel(Placed, Edge.Capital, Px));
			Edge.Capital.X -= 0.01;
			Placed = PlaceCard(Edge);
			CHECK(Placed.Spot == 1 && Placed.bFits && Near(Placed.Card.X, Edge.Capital.X + 28.0 * Px) && PinLevel(Placed, Edge.Capital, Px) && KeepsOff(Placed.Card, Edge.Screen, NoRects, Px));
			// (and the right's own edge: a capital near it on the right keeps the left)
			Edge.Capital.X = 1440.0 - 50.0;
			Placed = PlaceCard(Edge);
			CHECK(Placed.Spot == 0 && Placed.bFits && KeepsOff(Placed.Card, Edge.Screen, NoRects, Px));
		}

		// The picture's Lisbon (262, 520) at 1440 x 900: the left would stand 4 from the screen's edge, 12 short of the
		// keep-off, so the card goes right; panned 60 east, it fits on the left.
		{
			const std::vector<FRect> Pieces = { { 0, 0, 1440, 52 }, { 0, 750, 1440, 150 }, { 990, 70, 430, 672 } };
			FPlaceIn Lisbon = In;
			Lisbon.Capital = FPt{ 262, 520 };
			Lisbon.Size = FPt{ 230, 192 };
			Lisbon.Screen = FRect{ 0, 52, 1440, 698 };
			Lisbon.Frame = &Pieces;
			Placed = PlaceCard(Lisbon);
			CHECK(Placed.Spot == 1 && Placed.bFits && Near(Placed.Card.X, 290.0) && Near(Placed.Card.Centre().Y, 520.0) && PinLevel(Placed, Lisbon.Capital, 1.0) && KeepsOff(Placed.Card, Lisbon.Screen, Pieces, 1.0));
			Lisbon.Capital.X += 60.0;
			Placed = PlaceCard(Lisbon);
			CHECK(Placed.Spot == 0 && Placed.bFits && Near(Placed.Card.X, 64.0) && PinLevel(Placed, Lisbon.Capital, 1.0) && KeepsOff(Placed.Card, Lisbon.Screen, Pieces, 1.0));
		}

		// A panel keeps the card off it as the screen's edge does: one on the left, ending 16 short of where the card would
		// stand, lets it stand; a hair wider, and the card goes right. A panel over the right as well: the left again.
		{
			const FRect LeftCard = SpotBox(0, In.Capital, In.Size, 1.0);
			std::vector<FRect> Panel = { { 0, 0, LeftCard.X - 16.0, 900 } };
			FPlaceIn Paneled = In;
			Paneled.Frame = &Panel;
			Placed = PlaceCard(Paneled);
			CHECK(Placed.Spot == 0 && Placed.bFits && KeepsOff(Placed.Card, In.Screen, Panel, 1.0));
			Panel[0].W += 0.01;
			Placed = PlaceCard(Paneled);
			CHECK(Placed.Spot == 1 && Placed.bFits && PinLevel(Placed, In.Capital, 1.0) && KeepsOff(Placed.Card, In.Screen, Panel, 1.0));
			// (a panel above the right-hand card's place, reaching down to the capital's height: the right slides down
			// clear of it, the capital still 12 inside its top, and the pin stays level)
			Panel.push_back(FRect{ 740, 0, 700, 420 });
			Placed = PlaceCard(Paneled);
			CHECK(Placed.Spot == 1 && Placed.bFits && Near(Placed.Card.Y, 420.0 + 16.0) && PinLevel(Placed, In.Capital, 1.0) && KeepsOff(Placed.Card, In.Screen, Panel, 1.0));
		}

		// Under the top bar the card slides down as little as it must, the capital kept 12 inside its top: still level.
		{
			const std::vector<FRect> Bar = { { 0, 0, 1440, 52 } };
			FPlaceIn High = In;
			High.Frame = &Bar;
			High.Capital = FPt{ 720, 100 };
			Placed = PlaceCard(High);
			CHECK(Placed.Spot == 0 && Placed.bFits && Near(Placed.Card.Y, 52.0 + 16.0) && PinLevel(Placed, High.Capital, 1.0) && KeepsOff(Placed.Card, High.Screen, Bar, 1.0));
			// (and over the screen's foot it slides up)
			High.Capital = FPt{ 720, 860 };
			Placed = PlaceCard(High);
			CHECK(Placed.Spot == 0 && Placed.bFits && Near(Placed.Card.Bottom(), 900.0 - 16.0) && PinLevel(Placed, High.Capital, 1.0));

			// No fit: a capital 8 under the bar. The card's top would have to be 68 down to keep off it, and the capital
			// 12 inside the top, so it cannot keep off on either side. It stands where it lies least within the keep-off,
			// still level and 28 from the capital: here the left, the two sides lying alike within it.
			High.Capital = FPt{ 720, 60 };
			Placed = PlaceCard(High);
			CHECK(Placed.Spot == 0 && !Placed.bFits && Placed.Intrusion > 0.0 && Near(Placed.Card.Y, 60.0 - 12.0) && PinLevel(Placed, High.Capital, 1.0));
		}
		// No fit across: a window too narrow for the card on either side. It takes the side that lies less within the
		// keep-off: the left at 255 (it reaches 19 past the edge's band, the right 29), the right at 245.
		{
			FPlaceIn Narrow = In;
			Narrow.Screen = FRect{ 0, 0, 500, 900 };
			Narrow.Capital = FPt{ 255, 450 };
			Placed = PlaceCard(Narrow);
			CHECK(Placed.Spot == 0 && !Placed.bFits && Near(Placed.Intrusion, 19.0 * 180.0, 1e-6) && PinLevel(Placed, Narrow.Capital, 1.0));
			Narrow.Capital = FPt{ 245, 450 };
			Placed = PlaceCard(Narrow);
			CHECK(Placed.Spot == 1 && !Placed.bFits && Near(Placed.Intrusion, 19.0 * 180.0, 1e-6) && PinLevel(Placed, Narrow.Capital, 1.0));
			Narrow.Capital = FPt{ 250, 450 };
			Placed = PlaceCard(Narrow);
			CHECK(Placed.Spot == 0 && !Placed.bFits);
		}
		// The keep-off's measure: the screen drawn in by 16, each piece grown by 16.
		{
			const std::vector<FRect> Piece = { { 500, 400, 10, 10 } };
			FPlaceIn Weigh = In;
			CHECK(Near(KeepOffIntrusion(FRect{ 16, 16, 100, 100 }, Weigh), 0.0) && Near(KeepOffIntrusion(FRect{ 15, 16, 100, 100 }, Weigh), 100.0));
			Weigh.Frame = &Piece;
			CHECK(Near(KeepOffIntrusion(FRect{ 300, 300, 184, 100 }, Weigh), 0.0) && Near(KeepOffIntrusion(FRect{ 300, 300, 185, 100 }, Weigh), 1.0 * 16.0));
			Weigh.Px = 1.08;
			CHECK(KeepOffIntrusion(FRect{ 300, 300, 184, 100 }, Weigh) > 0.0); // (16 x 1.08 is 17.28)
		}
		// Placed again when the capital moves more than 24, or the screen changes size; not otherwise.
		Placed = PlaceCard(In);
		CHECK(NeedsPlace(FPlaced{}, In.Capital, In.Screen, 1.0));
		CHECK(!NeedsPlace(Placed, FPt{ 720 + 20, 450 }, In.Screen, 1.0));
		CHECK(NeedsPlace(Placed, FPt{ 720 + 25, 450 }, In.Screen, 1.0));
		CHECK(!NeedsPlace(Placed, FPt{ 720 + 25, 450 }, In.Screen, 1.08));
		CHECK(NeedsPlace(Placed, In.Capital, FRect{ 0, 0, 1280, 800 }, 1.0));
	}

	void TestLane()
	{
		// Unwrap: across the world's seam (WorldMinX -3000, WorldWidth 6000).
		FPathList Pieces;
		Unwrap(FPath{ { 2900, 0 }, { -2900, 100 } }, -3000.0, 6000.0, Pieces);
		CHECK(Pieces.Num() == 2);
		if (Pieces.Num() == 2)
		{
			CHECK(Near(Pieces[0].back().X, 3000.0) && Near(Pieces[0].back().Y, 50.0) && Near(Pieces[1].front().X, -3000.0) && Near(Pieces[1].front().Y, 50.0));
			CHECK(Near(Pieces[0].front().X, 2900.0) && Near(Pieces[1].back().X, -2900.0));
		}
		// (west across the seam: 50 to it of the 200 the short way round, so a quarter of the way down: y 25)
		Unwrap(FPath{ { -2950, 0 }, { 2850, 100 }, { 2800, 120 } }, -3000.0, 6000.0, Pieces);
		CHECK(Pieces.Num() == 2 && Near(Pieces[0].back().X, -3000.0) && Near(Pieces[0].back().Y, 25.0, 1e-9) && Near(Pieces[1].front().X, 3000.0) && Near(Pieces[1].front().Y, 25.0, 1e-9) && Pieces[1].size() == 3);
		Unwrap(FPath{ { 2900, 0 }, { -2900, 100 } }, 0.0, 0.0, Pieces);
		CHECK(Pieces.Num() == 1 && Pieces[0].size() == 2);
		Unwrap(FPath{ { 0, 0 }, { 100, 0 }, { 200, 50 } }, -3000.0, 6000.0, Pieces);
		CHECK(Pieces.Num() == 1 && Pieces[0].size() == 3);
		// Densify: ShowLanes' own curve (Catmull-Rom, its end points doubled), ceil(length / Step) steps a stretch, through
		// every point; on a straight line it stays on the line and runs one way. (Its steps bunch at the ends: the dashes
		// are measured along the curve, not by its steps.)
		FPath Dense;
		Densify(FPath{ { 0, 0 }, { 100, 0 } }, 4.0, Dense);
		bool bOnLine = Dense.size() == 26 && Near(Dense.front().X, 0.0) && Near(Dense.back().X, 100.0);
		for (size_t I = 1; I < Dense.size(); ++I)
		{
			bOnLine = bOnLine && Near(Dense[I].Y, 0.0, 1e-12) && Dense[I].X > Dense[I - 1].X && Dense[I].X - Dense[I - 1].X <= 4.0 * 1.6;
		}
		CHECK(bOnLine && Near(Dense[1].X, 0.5 * (100.0 * 0.04 + 300.0 * 0.0016 - 200.0 * 0.000064), 1e-9));
		const FPath Bend = { { 0, 0 }, { 60, 10 }, { 100, 60 }, { 110, 140 } };
		Densify(Bend, 4.0, Dense);
		size_t Found = 0;
		for (const FPt& P : Bend)
		{
			Found += std::any_of(Dense.begin(), Dense.end(), [&P](const FPt& Q) { return Near(Q.X, P.X, 1e-9) && Near(Q.Y, P.Y, 1e-9); }) ? 1 : 0;
		}
		double Widest = 0.0;
		for (size_t I = 1; I < Dense.size(); ++I)
		{
			Widest = std::max(Widest, Distance(Dense[I - 1], Dense[I]));
		}
		CHECK(Found == Bend.size() && Widest <= 4.0 * 1.6);
		// The casing: 5 wide, mitered, one strip (its alpha never doubles: no quad folds over another).
		FStrip Strip;
		Casing(FPath{ { 0, 0 }, { 100, 0 } }, 2.5, Strip);
		CHECK(Strip.Outer.size() == 2 && Near(Strip.Outer[0].Y, 2.5) && Near(Strip.Inner[0].Y, -2.5) && Near(Distance(Strip.Outer[1], Strip.Inner[1]), 5.0));
		Casing(FPath{ { 0, 0 }, { 50, 0 }, { 50, 50 } }, 2.5, Strip);
		CHECK(Near(Distance(Strip.Outer[1], FPt{ 50, 0 }), 2.5 * std::sqrt(2.0), 1e-9) && StripUnfolded(Strip));
		// (a turn of 110 degrees is still mitered: 2.5 / cos 55 out on each side, under two half widths)
		{
			const double Turn = 110.0 * 3.14159265358979323846 / 180.0;
			Casing(FPath{ { 0, 0 }, { 50, 0 }, { 50.0 + 50.0 * std::cos(Turn), 50.0 * std::sin(Turn) } }, 2.5, Strip);
			const double Miter = 2.5 / std::cos(Turn * 0.5);
			CHECK(Strip.Outer.size() == 3 && Near(Distance(Strip.Outer[1], FPt{ 50, 0 }), Miter, 1e-9) && Near(Distance(Strip.Inner[1], FPt{ 50, 0 }), Miter, 1e-9) && Miter < 5.0 && StripUnfolded(Strip));
		}
		// (a turn right back, to the left: the outside, the right, is bevelled square to the way in and then to the way
		// out; the inside is held two half widths along the miter; nothing spikes and the strip does not fold)
		Casing(FPath{ { 0, 0 }, { 50, 0 }, { 0, 3 } }, 2.5, Strip);
		CHECK(Strip.Outer.size() == 4 && Strip.Inner.size() == 4 && StripUnfolded(Strip));
		if (Strip.Inner.size() == 4)
		{
			const FPt Back = FPt{ -50.0, 3.0 }; // the way out, before it is made a unit
			const double Long = std::sqrt(Back.X * Back.X + Back.Y * Back.Y);
			CHECK(Near(Strip.Inner[1].X, 50.0) && Near(Strip.Inner[1].Y, -2.5)); // (square to the way in)
			CHECK(Near(Strip.Inner[2].X, 50.0 + 2.5 * Back.Y / Long) && Near(Strip.Inner[2].Y, -2.5 * Back.X / Long)); // (square to the way out)
			CHECK(Near(Distance(Strip.Outer[1], FPt{ 50, 0 }), 5.0, 1e-9) && Near(Strip.Outer[1].X, Strip.Outer[2].X) && Strip.Outer[1].X < 50.0);
		}
		// (and to the right, mirrored: the left is bevelled)
		Casing(FPath{ { 0, 0 }, { 50, 0 }, { 0, -3 } }, 2.5, Strip);
		CHECK(Strip.Outer.size() == 4 && Near(Strip.Outer[1].Y, 2.5) && Near(Strip.Inner[1].X, Strip.Inner[2].X) && StripUnfolded(Strip));
		// (the 90 degree corner and a gentle bend keep one pair a point)
		Casing(FPath{ { 0, 0 }, { 50, 0 }, { 100, 10 } }, 2.5, Strip);
		CHECK(Strip.Outer.size() == 3 && StripUnfolded(Strip));
		Densify(FPath{ { 0, 0 }, { 30, 0 }, { 30, 8 }, { 0, 10 }, { 0, 20 }, { 40, 22 } }, 2.0, Dense);
		Casing(Dense, 2.5, Strip);
		CHECK(StripUnfolded(Strip));
		// The dashes: 10 on, 7 off, from the seller.
		FPathList Line;
		Line.Add() = FPath{ { 0, 0 }, { 100, 0 } };
		FPathList Dash;
		Dashes(Line, 10.0, 7.0, -1.0, -1.0, Dash);
		CHECK(Dash.Num() == 6 && Near(Dash[0].front().X, 0.0) && Near(Dash[0].back().X, 10.0) && Near(Dash[1].front().X, 17.0) && Near(Dash[5].front().X, 85.0) && Near(Dash[5].back().X, 95.0));
		Line.Clear();
		Line.Add() = FPath{ { 0, 0 }, { 90, 0 } };
		Dashes(Line, 10.0, 7.0, -1.0, -1.0, Dash);
		CHECK(Dash.Num() == 6 && Near(LengthOf(Dash[5]), 5.0));
		// (the pattern runs on across the wrap: a piece of 12, then one of 20)
		Line.Clear();
		Line.Add() = FPath{ { 0, 0 }, { 12, 0 } };
		Line.Add() = FPath{ { 500, 0 }, { 520, 0 } };
		Dashes(Line, 10.0, 7.0, -1.0, -1.0, Dash);
		CHECK(Dash.Num() == 2 && Near(Dash[1].front().X, 505.0) && Near(Dash[1].back().X, 515.0));
		// (none in the cut's gap)
		Line.Clear();
		Line.Add() = FPath{ { 0, 0 }, { 100, 0 } };
		Dashes(Line, 10.0, 7.0, 20.0, 44.0, Dash);
		bool bClear = true;
		for (size_t K = 0; K < Dash.Num(); ++K)
		{
			for (const FPt& P : Dash[K])
			{
				bClear = bClear && !(P.X > 20.0 + 1e-9 && P.X < 44.0 - 1e-9);
			}
		}
		CHECK(bClear && Dash.Num() == 5 && Near(Dash[1].back().X, 20.0));
		// The crate: 40% along, run from the seller in 1.6 s, slowing as it arrives.
		FPt Way;
		CHECK(Near(PointAt(Line, 0.4, &Way).X, 40.0) && Near(Way.X, 1.0) && Near(PointAt(Line, 0.0).X, 0.0) && Near(PointAt(Line, 1.0).X, 100.0));
		CHECK(Near(CrateShare(0.0), 0.0) && Near(CrateShare(1.0), 0.4) && Near(CrateShare(0.5), 0.35) && CrateShare(0.5) > 0.2 && Near(CrateShare(7.0), 0.4));
		bool bRising = true;
		for (int K = 1; K <= 100; ++K)
		{
			bRising = bRising && CrateShare(K / 100.0) > CrateShare((K - 1) / 100.0);
		}
		CHECK(bRising && Near(CrateRunT(0.8), 0.5) && Near(CrateRunT(1.6), 1.0) && Near(CrateRunT(3.0), 1.0) && Near(CrateRunT(-1.0), 0.0));
		std::vector<FDraw> Crate;
		CrateDraws(FPt{ 243, 318 }, 0xB5574A, 1.08, Crate);
		CHECK(Crate.size() == 2 && Crate[0].Kind == EDraw::Box && Near(Crate[0].At.W, 16.0 * 1.08) && Near(Crate[0].At.H, 12.0 * 1.08) && Near(Crate[0].At.Centre().X, 243.0) && Near(Crate[0].At.Centre().Y, 318.0));
		CHECK(Crate.size() == 2 && Crate[0].Fill.Srgb == Kit::Paper200 && Crate[0].Edge.Srgb == 0xB5574A && Near(Crate[0].EdgeW, 1.8 * 1.08) && Near(Crate[0].Radius, 1.5 * 1.08));
		CHECK(Crate.size() == 2 && Crate[1].Kind == EDraw::Line && Near(Crate[1].A.Y, 318.0 - 1.08) && Near(Crate[1].A.X, 243.0 - 8.0 * 1.08) && Near(Crate[1].B.X, 243.0 + 8.0 * 1.08) && Near(Crate[1].EdgeW, 1.2 * 1.08));
		// The cut: a 24 gap nearest the sea, ticks 11 across its ends, a brass ring 12 across at 1.6.
		FGap Gap = CutGap(Line, FPt{ 50, 30 }, 24.0);
		CHECK(Gap.bOk && Near(Gap.From, 38.0) && Near(Gap.To, 62.0) && Near(Gap.A.X, 38.0) && Near(Gap.B.X, 62.0) && Near(Gap.Mid.X, 50.0));
		CHECK(CutGap(Line, FPt{ 2, 5 }, 24.0).bOk && Near(CutGap(Line, FPt{ 2, 5 }, 24.0).From, 0.0) && Near(CutGap(Line, FPt{ 2, 5 }, 24.0).To, 24.0));
		CHECK(Near(CutGap(Line, FPt{ 130, 5 }, 24.0).To, 100.0) && Near(CutGap(Line, FPt{ 130, 5 }, 24.0).From, 76.0));
		FPathList Short;
		Short.Add() = FPath{ { 0, 0 }, { 20, 0 } };
		CHECK(!CutGap(Short, FPt{ 10, 0 }, 24.0).bOk);
		FTick Tick[2];
		Ticks(Gap, 11.0, Tick);
		CHECK(Near(Tick[0].A.X, 38.0) && Near(Tick[0].B.X, 38.0) && Near(std::fabs(Tick[0].A.Y - Tick[0].B.Y), 11.0) && Near(Tick[0].A.Y + Tick[0].B.Y, 0.0));
		CHECK(Near(Tick[1].A.X, 62.0) && Near(Distance(Tick[1].A, Tick[1].B), 11.0));
		const FDraw Ring = CutRing(FPt{ 547, 191 }, 1.0);
		CHECK(Near(Ring.At.W, 12.0) && Near(Ring.EdgeW, 1.6) && Ring.Edge.Srgb == Kit::Brass300 && Near(Ring.Fill.A, 1.0 / 255.0, 1e-9) && Near(Ring.At.Centre().X, 547.0));
		CHECK(Near(Lane::TickW, 2.0) && Near(Lane::DashW, 2.6) && Near(Lane::CasingW, 5.0) && Near(Lane::CasingA, 0.55) && Near(Lane::Dash, 10.0) && Near(Lane::Gap, 7.0));
		// The whole lane, cut, at a pixel of 1 and of 2 map units.
		FRow Row;
		Row.Path = { { 0, 0 }, { 100, 0 }, { 200, 0 } };
		Row.State = EState::Cut;
		Row.Cut = FSeaMark{ 3, "Bay of Biscay", "March", { 100, 10 } };
		FLaneShape Shape;
		LayLane(Row, 1.0, 0.0, 0.0, Shape);
		CHECK(Shape.Gap.bOk && Near(Shape.Gap.From, 88.0) && Near(Shape.Gap.To, 112.0) && Shape.Kept.Num() == 2 && Shape.CasingCount == 2 && Near(Shape.Length, 200.0));
		CHECK(Shape.Kept.Num() == 2 && Near(Shape.Kept[0].back().X, 88.0) && Near(Shape.Kept[1].front().X, 112.0));
		CHECK(Shape.CasingCount == 2 && Near(Distance(Shape.Casing[0].Outer[0], Shape.Casing[0].Inner[0]), 5.0));
		bool bGapClear = true;
		for (size_t K = 0; K < Shape.Dashes.Num(); ++K)
		{
			for (const FPt& P : Shape.Dashes[K])
			{
				bGapClear = bGapClear && !(P.X > 88.0 + 1e-9 && P.X < 112.0 - 1e-9);
			}
		}
		CHECK(bGapClear && Near(Shape.Ticks[0].A.X, 88.0) && Near(Shape.Ticks[1].A.X, 112.0) && Near(Distance(Shape.Ticks[0].A, Shape.Ticks[0].B), 11.0));
		LayLane(Row, 2.0, 0.0, 0.0, Shape);
		CHECK(Near(Shape.Gap.To - Shape.Gap.From, 48.0) && Near(Distance(Shape.Casing[0].Outer[0], Shape.Casing[0].Inner[0]), 10.0) && Near(LengthOf(Shape.Dashes[0]), 20.0) && Near(Shape.Dashes[1].front().X, 34.0));
		Row.State = EState::Open;
		LayLane(Row, 1.0, 0.0, 0.0, Shape);
		CHECK(!Shape.Gap.bOk && Shape.Kept.Num() == 1 && Shape.CasingCount == 1);

		// The tag: 10 inside the edge the lane leaves by, 24 beside it on the side it heads to.
		const FRect Screen{ 0, 0, 1440, 900 };
		const std::vector<FRect> NoFrame;
		const FPt Text{ 50, 16 };
		FRect Tag;
		CHECK(ExitTag(FPath{ { 262, 520 }, { 220, 300 }, { 150, 60 }, { 120, -100 } }, Screen, NoFrame, Text, 1.0, Tag));
		CHECK(Near(Tag.Y, 10.0) && Near(Tag.W, 50.0));
		{
			// (the lane at the tag's top and foot: x 150 - 30 (50 / 160) at y 10, and at y 26)
			const double XTop = 150.0 - 30.0 * (60.0 - 10.0) / 160.0;
			CHECK(Near(Tag.Right(), XTop - 24.0, 1e-6) && Near(Lane::TagGap, 24.0));
		}
		CHECK(ExitTag(FPath{ { 548, 458 }, { 760, 150 }, { 800, -50 } }, Screen, NoFrame, Text, 1.0, Tag) && Tag.X > 780.0 && Near(Tag.Y, 10.0));
		CHECK(ExitTag(FPath{ { 1300, 450 }, { 1600, 460 } }, Screen, NoFrame, Text, 1.0, Tag) && Near(Tag.Right(), 1430.0) && Tag.Y > 450.0);
		CHECK(ExitTag(FPath{ { 30, 200 }, { 10, -100 } }, Screen, NoFrame, Text, 1.0, Tag) && Tag.X >= 0.0 && Tag.X > 20.0); // (no room to its left: the other side)
		CHECK(!ExitTag(FPath{ { 100, 100 }, { 300, 300 } }, Screen, NoFrame, Text, 1.0, Tag)); // (the buyer is on the screen)
		CHECK(!ExitTag(FPath{ { -100, 100 }, { -300, 300 } }, Screen, NoFrame, Text, 1.0, Tag)); // (never on it)
		const std::vector<FRect> TopBar = { { 0, 0, 1440, 52 } };
		CHECK(!ExitTag(FPath{ { 262, 520 }, { 150, 60 }, { 120, -100 } }, Screen, TopBar, Text, 1.0, Tag)); // (in the frame)
		CHECK(ExitTag(FPath{ { 262, 520 }, { 150, 60 }, { 120, -100 } }, FRect{ 0, 52, 1440, 698 }, TopBar, Text, 1.0, Tag) && Near(Tag.Y, 62.0));

		// Across the world map's seam (-TheaterWorld: WorldMinX -500, WorldWidth 9256.14): a lane west from (100, 100) to
		// (8600, 200) is two pieces, to the seam at -500 and on from 8756.14. With the screen showing map x -700 to 300
		// (a picture pixel a map unit, x + 700 on the screen), the seller's piece runs west from screen x 800 into the seam
		// at x 200, and the buyer's piece is far off. Joined, the seam's two ends made a false step to the right, and the tag
		// stood at the right edge; piece by piece, the lane runs into the seam on the screen, and there is no tag.
		{
			FRow Seam = RowOf(BritainsSeat(), "por-wolfram");
			Seam.Path = { { 100, 100 }, { 8600, 200 } };
			FLaneShape World;
			LayLane(Seam, 1.0, -500.0, 9256.14, World);
			CHECK(World.Raw.Num() == 2 && Near(World.Raw[0].back().X, -500.0) && Near(World.Raw[1].front().X, 8756.14, 1e-6));
			FPathList OnScreen;
			FPath Joined;
			for (size_t P = 0; P < World.Raw.Num(); ++P)
			{
				FPath& Piece = OnScreen.Add();
				for (const FPt& At : World.Raw[P])
				{
					Piece.push_back(FPt{ At.X + 700.0, At.Y });
					Joined.push_back(FPt{ At.X + 700.0, At.Y });
				}
			}
			const FRect Window{ 0, 0, 1000, 900 };
			CHECK(ExitTag(Joined, Window, NoFrame, Text, 1.0, Tag) && Tag.X > 900.0); // (the old join: the tag at the wrong edge)
			CHECK(!ExitTagOfPieces(OnScreen, Window, NoFrame, Text, 1.0, Tag));
			// (the screen over the seam's far side, map x 8650 to 9650: the buyer's piece comes in at x 106 and leaves by
			// the left edge for the buyer at 8600, off the screen: the tag stands 10 inside that edge)
			OnScreen.Clear();
			for (size_t P = 0; P < World.Raw.Num(); ++P)
			{
				FPath& Piece = OnScreen.Add();
				for (const FPt& At : World.Raw[P])
				{
					Piece.push_back(FPt{ At.X - 8650.0, At.Y });
				}
			}
			CHECK(ExitTagOfPieces(OnScreen, Window, NoFrame, Text, 1.0, Tag) && Near(Tag.X, Lane::TagInside));
			// (one piece: as ExitTag)
			OnScreen.Clear();
			OnScreen.Add() = FPath{ { 262, 520 }, { 220, 300 }, { 150, 60 }, { 120, -100 } };
			FRect One;
			CHECK(ExitTagOfPieces(OnScreen, Screen, NoFrame, Text, 1.0, Tag) && ExitTag(OnScreen[0], Screen, NoFrame, Text, 1.0, One) && Near(Tag.X, One.X) && Near(Tag.Y, One.Y));
			OnScreen.Clear();
			CHECK(!ExitTagOfPieces(OnScreen, Screen, NoFrame, Text, 1.0, Tag));
		}
	}

	void TestTint()
	{
		// Two provinces of one country: their shared border is left out, the coasts kept.
		const std::vector<FPath> Two = { { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } }, { { 10, 0 }, { 20, 0 }, { 20, 10 }, { 10, 10 } } };
		std::vector<FChain> Chains;
		OuterEdges(Two, Chains);
		size_t Edges = 0;
		bool bShared = false;
		for (const FChain& Chain : Chains)
		{
			Edges += Chain.Points.size() - 1;
			for (size_t I = 1; I < Chain.Points.size(); ++I)
			{
				bShared = bShared || (Near(Chain.Points[I].X, 10.0) && Near(Chain.Points[I - 1].X, 10.0));
			}
		}
		CHECK(Chains.size() == 2 && Edges == 6 && !bShared);
		// A lone province: one closed chain, the coast all round.
		const FPath Square = { { 0, 0 }, { 100, 0 }, { 100, 100 }, { 0, 100 }, { 0, 0 } };
		OuterEdges({ Square }, Chains);
		CHECK(Chains.size() == 1 && Chains[0].bClosed && Chains[0].Points.size() == 4);
		// The strip lies inside, 13 in, whichever way the ring runs.
		for (int Way = 0; Way < 2; ++Way)
		{
			FPath Ring = Square;
			if (Way == 1)
			{
				std::reverse(Ring.begin(), Ring.end());
			}
			FTintShape Tint;
			LayTint({ Ring }, 1.0, Tint);
			CHECK(Tint.Strips.size() == 1 && Tint.Strips[0].Outer.size() == 5 && Tint.Strips[0].Inner.size() == 5);
			bool bIn = true;
			for (const FPt& P : Tint.Strips[0].Inner)
			{
				bIn = bIn && InPolygon(P, Square) && Near(std::min({ P.X, P.Y, 100.0 - P.X, 100.0 - P.Y }), 13.0, 1e-6);
			}
			CHECK(bIn && StripUnfolded(Tint.Strips[0]));
			Tint.Chains.clear();
			LayTint({ Ring }, 2.0, Tint);
			CHECK(Near(Tint.Strips[0].Inner[0].X, 26.0) || Near(Tint.Strips[0].Inner[0].X, 74.0));
		}
		CHECK(Near(TintWidth, 13.0));
		// Portugal from the picture: the strip's inner side inside the country.
		const FPath Portugal = { { 285, 330 }, { 345, 335 }, { 350, 420 }, { 330, 470 }, { 360, 540 }, { 340, 600 }, { 330, 640 }, { 300, 640 }, { 275, 580 }, { 265, 520 }, { 275, 450 }, { 280, 380 } };
		FTintShape Tint;
		LayTint({ Portugal }, 1.0, Tint);
		bool bIn = Tint.Strips.size() == 1;
		for (size_t K = 0; bIn && K < Tint.Strips[0].Inner.size(); ++K)
		{
			bIn = InPolygon(Tint.Strips[0].Inner[K], Portugal);
		}
		CHECK(bIn);
	}

	void TestSeals()
	{
		CHECK(Near(SealBoxW, 20.4) && Near(SealBoxH, 26.4) && Near(SealMid, 10.2));
		const FRect Box = SealBox(FPt{ 100, 100 }, 1.0);
		CHECK(Near(Box.X, 89.8) && Near(Box.Y, 89.8) && Near(Box.W, 20.4) && Near(Box.H, 26.4));
		const FPt Cap{ 500, 400 };
		std::vector<FRect> None, Avoid, Frame;
		std::vector<FPt> Laid;
		FPt At;
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 500.0) && Near(At.Y, 376.0));
		Laid.push_back(At);
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 524.0) && Near(At.Y, 376.0));
		Laid.push_back(At);
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 476.0) && Near(At.Y, 376.0));
		Laid.push_back(At);
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 528.0) && Near(At.Y, 400.0));
		// 20 apart: 19 is too near, 20 is not.
		Laid = { FPt{ 500, 376 + 19 } };
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 524.0));
		Laid = { FPt{ 500, 376 - 20 } };
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 500.0));
		// Clear of the plates where it can be; over them where it cannot; never in the frame.
		Laid.clear();
		Avoid = { FRect{ 400, 300, 200, 120 } };
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 500.0) && Near(At.Y, 376.0));
		Frame = { FRect{ 0, 0, 1440, 900 } };
		CHECK(!SealAt(Cap, 1.0, Frame, Avoid, Laid, At));
		Frame.clear();
		Avoid = { FRect{ 470, 360, 20, 10 } };
		CHECK(SealAt(Cap, 1.0, Frame, Avoid, Laid, At) && Near(At.X, 524.0));
		CHECK(SealAt(Cap, 1.08, None, None, Laid, At) && Near(At.Y, 400.0 - 24.0 * 1.08));
	}

	/**
	 * Call 6: while its card is shown, the open card's seal stands with its disc's middle on the card's top-right corner,
	 * where it is the card's close; only that seal moves; its box is clear of the ring (a round of three months shows one)
	 * and of every button and word on the card, and, the card being KeepOff off everything, on the screen and off the panels.
	 */
	void TestCardSeal()
	{
		CHECK(Near(CardSealAt(FRect{ 100, 200, 230, 180 }).X, 330.0) && Near(CardSealAt(FRect{ 100, 200, 230, 180 }).Y, 200.0));
		FSealLook Seal;
		Seal.bOpens = true;
		CHECK(!SealOnCard(Seal, true));
		Seal.bOpen = true;
		CHECK(SealOnCard(Seal, true) && !SealOnCard(Seal, false));
		// Two seals that open a card, one open: only it stands on the card, and none while the card is not shown.
		FBook Book = BritainsSeat();
		FRow Mine = RowOf(Book, "por-wolfram");
		Mine.Id = "swe-ore";
		Mine.Seller = "UK";
		Mine.Round = Round({ { "GER", 3.0 } }, std::nullopt, "It is yours to sell.", true);
		Book.Rows.push_back(Mine);
		FLookIn In;
		In.SrgbOf = ColourOf;
		In.SeesLegion = SeesEveryLegion;
		In.OpenRow = "por-wolfram";
		FLook Look;
		LookAt(Book, In, Look);
		int Opens = 0, OnCard = 0, OnShut = 0;
		for (const FSealLook& Each : Look.Seals)
		{
			Opens += Each.bOpens ? 1 : 0;
			if (SealOnCard(Each, true))
			{
				++OnCard;
				CHECK(Book.Rows[Each.Row].Id == "por-wolfram");
			}
			OnShut += SealOnCard(Each, false) ? 1 : 0;
		}
		CHECK(Opens == 2 && OnCard == 1 && OnShut == 0);
		In.OpenRow.clear();
		LookAt(Book, In, Look);
		OnCard = 0;
		for (const FSealLook& Each : Look.Seals)
		{
			OnCard += SealOnCard(Each, true) ? 1 : 0;
		}
		CHECK(OnCard == 0);
		// On the card: clear of the ring, the buttons and the words; a press on it shuts the card, one mid-card does not.
		for (const double Px : { 1.0, 1.08 })
		{
			FBook Long = BritainsSeat();
			Long.RoundLen = 3;
			for (const FRow* Row : { &RowOf(Long, "por-wolfram"), static_cast<const FRow*>(&Mine) })
			{
				FCard Card;
				CHECK(LayCard(*Row, Long, KitMeasure, Px, Card));
				const FRect Box = SealBox(CardSealAt(FRect{ 0.0, 0.0, Card.Size.X, Card.Size.Y }), Px);
				CHECK(Near(Box.Right() - Card.Size.X, SealMid * Px) && Near(-Box.Y, SealMid * Px) && SealMid < KeepOff);
				const FDraw* Ring = RingOf(Card, Px);
				CHECK(Ring != nullptr);
				if (Ring)
				{
					const FPt Mid = Ring->At.Centre();
					const FPt Closest{ std::clamp(Mid.X, Box.X, Box.Right()), std::clamp(Mid.Y, Box.Y, Box.Bottom()) };
					CHECK(Distance(Mid, Closest) > Ring->At.W * 0.5 + Ring->EdgeW * 0.5 + 2.0 * Px);
				}
				for (const FButton& Button : Card.Buttons)
				{
					CHECK(!Button.At.Overlaps(Box));
				}
				for (size_t K = 1; K < Card.Draws.size(); ++K)
				{
					CHECK(Card.Draws[K].Kind != EDraw::Text || !Card.Draws[K].At.Overlaps(Box));
				}
				CHECK(OnCardSeal(FPt{ Card.Size.X - 2.0 * Px, 2.0 * Px }, Card.Size, Px) && OnCardSeal(FPt{ Card.Size.X - 9.0 * Px, 15.0 * Px }, Card.Size, Px));
				CHECK(!OnCardSeal(FPt{ Card.Size.X * 0.5, Card.Size.Y * 0.5 }, Card.Size, Px) && !OnCardSeal(FPt{ Card.Size.X - 11.0 * Px, 2.0 * Px }, Card.Size, Px) && !OnCardSeal(FPt{ Card.Size.X - 2.0 * Px, 17.0 * Px }, Card.Size, Px));
			}
		}
		// A placed card's seal: on the screen and off every panel, as the card is (left, right, slid under the top bar).
		const std::vector<FRect> Frame = { { 0, 0, 1440, 52 }, { 0, 750, 1440, 150 }, { 990, 70, 430, 672 } };
		for (const FPt Capital : { FPt{ 322, 520 }, FPt{ 262, 520 }, FPt{ 600, 90 }, FPt{ 900, 700 } })
		{
			FPlaceIn Place{ Capital, FPt{ 230, 210 }, FRect{ 0, 52, 1440, 698 }, &Frame, 1.0 };
			const FPlaced Placed = PlaceCard(Place);
			const FRect Box = SealBox(CardSealAt(Placed.Card), 1.0);
			CHECK(Placed.bFits && Inside(Box, Place.Screen));
			for (const FRect& Piece : Frame)
			{
				CHECK(!Box.Overlaps(Piece));
			}
		}
	}

	/**
	 * Calls 11 and 13 on the tray card. The corner flag is the size the other tray cards' flags are: an ally's corps's in
	 * CampaignHUD's MakeCard, read from the source here so the two cannot part. The legion's line under the title is one
	 * line in the card's width, never wider: the game's one-division card is 355 px at 1441, and the line has that less the
	 * button's padding (8 and 4), its own 4 on the right and, on the left, the corner flag and its gap, so that it starts
	 * under the title's text (the look review's finding 1, option a); cut at a whole word with the ellipsis where it is too
	 * long.
	 */
	void TestTrayCard()
	{
		CHECK(Near(TrayFlagW, 21.0) && Near(TrayFlagH, 14.0));
		{
			const std::string Here = __FILE__;
			const std::string Hud = Here.substr(0, Here.find_last_of('/') + 1) + "../../Source/TheaterCommand/Campaign/CampaignHUD.cpp";
			std::ifstream File(Hud);
			std::string Line;
			bool bAllyFlag = false, bSeen = false;
			while (std::getline(File, Line))
			{
				bAllyFlag = bAllyFlag || Line.find("An ally's corps marching with the army carries its own flag.") != std::string::npos;
				const size_t At = Line.find("WidthOverride(Card.Ally.IsEmpty() ? 0.0f : ");
				if (bAllyFlag && At != std::string::npos)
				{
					double W = 0.0, H = 0.0;
					const int Read = std::sscanf(Line.c_str() + At, "WidthOverride(Card.Ally.IsEmpty() ? 0.0f : %lff).HeightOverride(%lff)", &W, &H);
					CHECK(Read == 2 && Near(W, TrayFlagW) && Near(H, TrayFlagH));
					bSeen = true;
					break;
				}
			}
			if (File.is_open())
			{
				CHECK(bSeen);
			}
			else
			{
				std::printf("(CampaignHUD.cpp not beside the tests: the tray flag's size is not read against it)\n");
			}
		}
		const FFace Face = TrayLegionFace(1.0);
		CHECK(Face.Family == EFamily::Serif && Face.Style == EStyle::Regular && Near(Face.Size, 12.0) && Near(TrayLegionFace(1.08).Size, 12.0 * 1.08));
		CHECK(Face.Family == PlateLineFace(1.0).Family && Near(Face.Size, PlateLineFace(1.0).Size));
		// (under the title's text: the corner flag's 21 and its gap of 6 while the flag shows, as in MakeCard's title row,
		// where the title follows TrayFlag; flush where no flag shows. The line shows only with the flag)
		CHECK(Near(TrayFlagGap, 6.0) && Near(TrayLegionIndent(true), 27.0) && Near(TrayLegionIndent(true), TrayFlagW + TrayFlagGap) && Near(TrayLegionIndent(false), 0.0));
		const double Flush = 355.0 - 8.0 - 4.0 - 4.0;
		const double Inside = Flush - TrayLegionIndent(true);
		CHECK(Near(Flush, 339.0) && Near(Inside, 312.0));
		const std::string Blue = "Spanish volunteers, with the German army, Leningrad front";
		const std::string Dots = "\xE2\x80\xA6";
		std::string Fit;
		// (flush with the flag the whole line would fit at 12; under the title's text it is cut at a word, at 12 and at the
		// picture's 13 alike: the face is the art director's to rule, NOTES)
		CHECK(FitLine(Blue, Face, Flush, KitMeasure, Fit) && Fit == Blue && KitMeasure(Blue, Face).X <= Flush);
		CHECK(!FitLine(Blue, Face, Inside, KitMeasure, Fit) && Fit == "Spanish volunteers, with the German army, Leningrad" + Dots && KitMeasure(Fit, Face).X <= Inside);
		const FFace Picture = FaceOf(EFamily::Serif, EStyle::Regular, 13.0, 1.0);
		CHECK(KitMeasure(Blue, Picture).X > Flush);
		CHECK(!FitLine(Blue, Picture, Inside, KitMeasure, Fit) && Fit == "Spanish volunteers, with the German army" + Dots && KitMeasure(Fit, Picture).X <= Inside);
		auto EndsDots = [&Dots](const std::string& Text) { return Text.size() >= Dots.size() && Text.compare(Text.size() - Dots.size(), Dots.size(), Dots) == 0; };
		// Too long: cut at a whole word, the comma before the cut dropped, the ellipsis after it, no wider than the line.
		const double Short = KitMeasure("Spanish volunteers, with the German army, Leningrad", Face).X;
		CHECK(!FitLine(Blue, Face, Short, KitMeasure, Fit) && Fit == "Spanish volunteers, with the German army" + Dots && KitMeasure(Fit, Face).X <= Short);
		CHECK(!FitLine(Blue, Face, 130.0, KitMeasure, Fit) && EndsDots(Fit) && KitMeasure(Fit, Face).X <= 130.0 && Blue.rfind(Fit.substr(0, Fit.size() - Dots.size()), 0) == 0);
		const size_t Head = Fit.size() - Dots.size();
		CHECK(Head > 0 && Fit[Head - 1] != ',' && Fit[Head - 1] != ' ' && (Blue[Head] == ' ' || Blue[Head] == ','));
		// A long line against the 355 px card: one line, cut, never wider; the line's box takes the card's width (the layer's
		// widget asks for none), so the card is not widened by it.
		FBook Long = BritainsSeat();
		Long.Nations.push_back(Nation("XXL", "The Most Serene Republic of Somewhere", "", 0));
		FLegion Far = Long.Legions[0];
		Far.Home = "XXL";
		Far.HomeName.clear();
		Far.Place = "Kalinin and Novgorod";
		const std::string Whole = LegionLine(Far, Long);
		CHECK_TEXT(Whole, "Volunteers from The Most Serene Republic of Somewhere, with the German army, Kalinin and Novgorod front");
		CHECK(!FitLine(Whole, Face, Inside, KitMeasure, Fit) && EndsDots(Fit) && KitMeasure(Fit, Face).X <= Inside && Fit.find('\n') == std::string::npos);
		CHECK(Whole.rfind(Fit.substr(0, Fit.size() - Dots.size()), 0) == 0 && Whole[Fit.size() - Dots.size()] == ' ' && Fit == "Volunteers from The Most Serene Republic of" + Dots);
		CHECK(!FitLine(Whole, Face, Flush, KitMeasure, Fit) && Whole[Fit.size() - Dots.size()] == ',' && Fit == "Volunteers from The Most Serene Republic of Somewhere" + Dots);
		// One word too long: by letters; nothing where not even the ellipsis fits; nothing to fit fits.
		CHECK(!FitLine("Unterseebootsflottille", Face, 40.0, KitMeasure, Fit) && EndsDots(Fit) && KitMeasure(Fit, Face).X <= 40.0 && Fit == "Unte" + Dots);
		CHECK(!FitLine(Blue, Face, 2.0, KitMeasure, Fit) && Fit.empty());
		CHECK(FitLine("", Face, 0.0, KitMeasure, Fit) && Fit.empty());
	}

	void TestPlates()
	{
		std::vector<FDraw> Sign;
		MenSign(FPt{ 0, 0 }, 16.0, 1.0, Sign);
		CHECK(Sign.size() == 3);
		bool bIn = true;
		for (const FDraw& Stroke : Sign)
		{
			bIn = bIn && Stroke.Kind == EDraw::Poly && Stroke.Edge.Srgb == Kit::Brass300 && Near(Stroke.EdgeW, 1.8);
			for (const FPt& P : Stroke.Points)
			{
				bIn = bIn && P.X >= 0.0 && P.X <= 16.0 && P.Y >= 0.0 && P.Y <= 16.0;
			}
		}
		CHECK(bIn);
		CHECK(Sign.size() == 3 && Near(Sign[0].Points.front().X, 3.0) && Near(Sign[0].Points.back().X, 13.0) && Near(Sign[0].Points[6].Y, 6.5 * 16.0 / 24.0, 1e-9));
		const std::string Line = "1 division abroad, Leningrad";
		const FPlateLineLay Lay = LayPlateLine(Line, KitMeasure, 1.0);
		const FPt TextSize = KitMeasure(Line, PlateLineFace(1.0));
		CHECK(Near(Lay.Sign.W, 16.0) && Near(Lay.TextAt.X, 21.0) && Near(Lay.Size.X, 21.0 + TextSize.X) && Near(Lay.Size.Y, std::max(16.0, TextSize.Y)));
		CHECK(Near(Lay.Sign.Y + Lay.Sign.H * 0.5, Lay.Size.Y * 0.5) && Near(Lay.TextAt.Y + TextSize.Y * 0.5, Lay.Size.Y * 0.5));
		FOwnPlate Own;
		LayOwnPlate("Madrid", Line, FPt{ 548, 458 }, KitMeasure, 1.0, Own);
		CHECK(Near(Own.Plate.X, 560.0) && Near(Own.Plate.Y, 470.0) && Near(Own.Plate.W, std::max(KitMeasure("Madrid", PlateNameFace(1.0)).X, Lay.Size.X) + 16.0));
		CHECK(Own.Draws.size() == 6 && Own.Draws[0].Fill.Srgb == Kit::Desk900 && Near(Own.Draws[0].Fill.A, 0.88, 1e-6) && Own.Draws[0].Edge.Srgb == Kit::Brass700);
		CHECK(Own.Draws.size() == 6 && Own.Draws[1].Text == "Madrid" && Near(Own.Draws[1].Font.Size, 13.0) && Own.Draws[1].Fill.Srgb == Kit::Paper200 && Own.Draws[5].Fill.Srgb == Kit::Paper300 && Own.Draws[5].Text == Line);
		for (const FDraw& Draw : Own.Draws)
		{
			CHECK(Inside(Draw.At, Own.Plate));
		}
		const std::vector<FRect> NoRects, Plate = { FRect{ 600, 480, 50, 20 } };
		CHECK(OwnPlateShown(12000.0, Own.Plate, NoRects, NoRects));
		CHECK(!OwnPlateShown(12500.0, Own.Plate, NoRects, NoRects));
		CHECK(!OwnPlateShown(9000.0, Own.Plate, Plate, NoRects));
		CHECK(!OwnPlateShown(9000.0, Own.Plate, NoRects, Plate));
		CHECK(TrayLineFace(1.0).Family == EFamily::Courier && Near(TrayLineFace(1.0).Size, 13.0) && Near(TagFace(1.08).Size, 12.0 * 1.08));
	}

	void TestNoAllocation()
	{
		const FBook Book = BritainsSeat();
		const std::vector<FRect> Frame = { { 0, 0, 1440, 52 }, { 0, 750, 1440, 150 }, { 990, 70, 430, 672 } }, Plates = { { 300, 300, 80, 30 } };
		FPlaceIn In;
		In.Capital = FPt{ 262, 520 };
		In.Size = FPt{ 230, 186 };
		In.Screen = FRect{ 0, 0, 1440, 900 };
		In.Frame = &Frame;
		std::vector<FPt> Laid;
		Laid.reserve(8);
		const FRow& Por = RowOf(Book, "por-wolfram");
		const FRow& Ore = RowOf(Book, "esp-ore");
		FLaneShape Lane;
		FLaneShape Cut;
		const FPath Portugal = { { 285, 330 }, { 345, 335 }, { 350, 420 }, { 330, 470 }, { 360, 540 }, { 340, 600 }, { 330, 640 }, { 300, 640 }, { 275, 580 }, { 265, 520 }, { 275, 450 }, { 280, 380 } };
		const std::vector<FPath> Rings = { Portugal };
		FTintShape Tint;
		// (the eye's range, laid once: the shapes' buffers grow to it as the map is first zoomed through)
		auto Sweep = [&]()
		{
			for (int Step = 0; Step < 6; ++Step)
			{
				LayLane(Por, 1.0 + Step * 0.1, -3000.0, 6000.0, Lane);
				LayLane(Ore, 1.0 + Step * 0.1, -3000.0, 6000.0, Cut);
				LayTint(Rings, 1.0 + Step * 0.1, Tint);
			}
			LayLane(Por, 1.0, -3000.0, 6000.0, Lane);
		};
		Sweep();
		const FPath OnScreen = { { 262, 520 }, { 150, 60 }, { 120, -100 } };
		FRect Tag;
		FPt At;
		double Sum = 0.0;
		// (the lane's pieces on the screen, filled each frame as the layer's Tick fills them, piece by piece)
		FPathList Pieces;
		auto Project = [&Pieces, &Lane]()
		{
			Pieces.Clear();
			for (size_t P = 0; P < Lane.Raw.Num(); ++P)
			{
				FPath& Seen = Pieces.Add();
				for (const FPt& Point : Lane.Raw[P])
				{
					Seen.push_back(FPt{ Point.X - 100.0, Point.Y });
				}
			}
		};
		Project();
		// Every frame: the card's place, the seals, the tag, the crate's point. None may allocate.
		GNews = 0;
		GCounting = true;
		for (int Frame_ = 0; Frame_ < 50; ++Frame_)
		{
			const FPlaced Placed = PlaceCard(In);
			Sum += Placed.Card.X + Placed.Intrusion + KeepOffIntrusion(Placed.Card, In);
			Laid.clear();
			SealAt(In.Capital, 1.0, Frame, Plates, Laid, At);
			Laid.push_back(At);
			Sum += ExitTag(OnScreen, In.Screen, Frame, FPt{ 50, 16 }, 1.0, Tag) ? Tag.X : 0.0;
			Project();
			Sum += ExitTagOfPieces(Pieces, In.Screen, Frame, FPt{ 50, 16 }, 1.0, Tag) ? Tag.X : 0.0;
			Sum += PointAt(Lane.Pieces, CrateShare(CrateRunT(Frame_ / 30.0))).X;
			Sum += NeedsPlace(Placed, In.Capital, In.Screen, 1.0) ? 1.0 : 0.0;
		}
		GCounting = false;
		CHECK(GNews == 0);
		if (GNews != 0)
		{
			std::printf("  %ld allocations on the paths run every frame (sum %f)\n", GNews, Sum);
		}
		// Laid again as the eye moves through the range it has been through: their buffers are reused.
		GNews = 0;
		GCounting = true;
		Sweep();
		GCounting = false;
		CHECK(GNews == 0);
		if (GNews != 0)
		{
			std::printf("  %ld allocations laying the lane and the tint again\n", GNews);
		}
		CHECK(Sum > 0.0);
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	/** The card's hint, its lines joined. */
	std::string HintOf(const FCard& Card, double Px)
	{
		std::string Joined;
		for (const FDraw* Line : TextsInFace(Card, HintFace(Px), Kit::HintInk))
		{
			Joined += (Joined.empty() ? "" : " ") + Line->Text;
		}
		return Joined;
	}

	/** The rules' own look answers, read by the feed's ReadBook: every lane and card in them laid. */
	void TestSamples(const char* Folder)
	{
		const char* Files[] = { "look-bidder-UK-August-1941.json", "look-seller-SWE-August-1941.json", "look-agent-UK-August-1941.json",
			"look-outsider-USA-August-1941.json", "look-cut-UK-June-1940.json", "look-first-UK-October-1939.json", "look-off.json" };
		int Cards = 0, Lanes = 0, Cuts = 0;
		for (const char* Name : Files)
		{
			const std::string Text = ReadAll(std::string(Folder) + "/" + Name);
			FBook Book;
			std::string Error;
			const bool bRead = ReadBook(Text, Book, &Error);
			CHECK(bRead && !Text.empty());
			FLookIn In;
			In.SrgbOf = ColourOf;
			In.SeesLegion = SeesEveryLegion;
			FLook Look;
			LookAt(Book, In, Look);
			CHECK(Book.Scale == 100.0 || !Book.bOn);
			if (std::string(Name) == "look-off.json")
			{
				CHECK(!Book.bOn && Look.Lanes.empty() && Look.Seals.empty() && Look.Plates.empty() && Look.Trays.empty());
				continue;
			}
			CHECK(Book.bOn && Look.Lanes.size() <= 16);
			// (the Blue Division serves near Minsk in August 1941; in June 1940 and October 1939 there are no legions yet)
			if (Book.Legions.empty())
			{
				CHECK(Look.Plates.empty() && Look.Trays.empty());
			}
			else
			{
				CHECK(Look.Plates.size() == 1 && Look.Trays.size() == 1);
				CHECK(Look.Plates.size() == 1 && Look.Plates[0].Line == "1 division abroad, Minsk" && Look.Plates[0].Capital == 159);
				CHECK(Look.Trays.size() == 1 && Look.Trays[0].Line == "1 DIV \xC2\xB7 100% \xC2\xB7 TOUR 1 OF 28 MO" && Look.Trays[0].Flag == "ESP_PLAIN");
				CHECK(Look.Trays.size() == 1 && Look.Trays[0].Tip == "Volunteers from Spain, with the German army, Minsk front.");
				CHECK(Look.Trays.size() == 1 && Look.Trays[0].Legion == "Spanish volunteers, with the German army, Minsk front"); // (the bridge's adjectives)
			}
			for (const FLaneLook& Each : Look.Lanes)
			{
				FLaneShape Shape;
				LayLane(Book.Rows[Each.Row], 1.48, 0.0, 0.0, Shape);
				++Lanes;
				Cuts += Each.bCut ? 1 : 0;
				CHECK(Shape.Pieces.Num() >= 1 && Shape.Dashes.Num() >= 1 && (!Each.bCut || Shape.Gap.bOk));
			}
			for (const FSealLook& Seal : Look.Seals)
			{
				if (!Seal.bOpens)
				{
					continue;
				}
				FCard Card;
				CHECK(LayCard(Book.Rows[Seal.Row], Book, KitMeasure, 1.08, Card));
				++Cards;
				const FRect Whole{ 0.0, 0.0, Card.Size.X, Card.Size.Y };
				for (size_t K = 1; K < Card.Draws.size(); ++K)
				{
					CHECK(Inside(Card.Draws[K].At, Whole));
				}
				for (const FButton& Button : Card.Buttons)
				{
					CHECK(Inside(Button.At, Whole));
				}
			}
			if (std::string(Name) == "look-seller-SWE-August-1941.json")
			{
				FCard Card;
				const FRow* Ore = Book.Row("swe-ore");
				CHECK(Ore && LayCard(*Ore, Book, KitMeasure, 1.0, Card) && Card.Rows == 6 && Card.Buttons.size() == 7 && TextNamed(Card, "SELL TO NOBODY"));
				CHECK_TEXT(HintOf(Card, 1.0), "It is yours to sell.");
			}
			if (std::string(Name) == "look-bidder-UK-August-1941.json")
			{
				// (Britain has bid 1.5 for Sweden's ore, shown as 150, which Germany holds in its last month; the rival bids unseen)
				FCard Card;
				const FRow* Ore = Book.Row("swe-ore");
				CHECK(Ore && LayCard(*Ore, Book, KitMeasure, 1.0, Card) && TextNamed(Card, "150") && TextNamed(Card, "Others") && Card.Buttons.size() == 2);
				CHECK(TextNamed(Card, "held by Germany, 12 of 12 months") && TextNamed(Card, "Britain") && TextNamed(Card, ", yours") && TextNamed(Card, "ORE") && TextNamed(Card, "Sweden"));
				CHECK_TEXT(HintOf(Card, 1.0), "Rival bids show with an agent in Stockholm, or in the bidder's capital.");
				// (Switzerland's goods: no way open to Britain, so the reason leads and there is no bid to make)
				const FRow* Swiss = Book.Row("sui-goods");
				CHECK(Swiss && !OpensCard(*Swiss));
			}
			if (std::string(Name) == "look-agent-UK-August-1941.json")
			{
				// (an agent in Stockholm: every bid seen, no Others row; Britain's own row first with its steps)
				FCard Card;
				const FRow* Ore = Book.Row("swe-ore");
				CHECK(Ore && LayCard(*Ore, Book, KitMeasure, 1.0, Card) && !TextNamed(Card, "Others") && Card.Rows == 6 && TextNamed(Card, "518") && TextNamed(Card, "150"));
				CHECK(HintOf(Card, 1.0).empty());
			}
			if (std::string(Name) == "look-outsider-USA-August-1941.json")
			{
				// (the United States may not bid and has no agent: quiet seals only, no card)
				for (const FSealLook& Seal : Look.Seals)
				{
					CHECK(!Seal.bOpens);
				}
			}
			if (std::string(Name) == "look-cut-UK-June-1940.json")
			{
				// (Finland's exports to Britain, cut in the Norwegian Sea: Britain's own generic row, so its lane is drawn)
				bool bFinland = false;
				for (const FLaneLook& Each : Look.Lanes)
				{
					if (Book.Rows[Each.Row].Id == "gen-FIN")
					{
						bFinland = true;
						CHECK(Each.bCut && !Each.bCrate && Each.Tag == "to Britain");
						CHECK_TEXT(Each.Hover, "Goods for Britain lost at sea, Norwegian Sea, June");
					}
				}
				CHECK(bFinland);
			}
		}
		CHECK(Cards >= 5 && Lanes >= 10 && Cuts >= 1);
		std::printf("samples: %d cards and %d lanes (%d cut) laid from the rules' own look\n", Cards, Lanes, Cuts);
	}
}

namespace
{
	/** Dashes as it was first written (each dash cut from its piece's start): the reference the one-pass walk must match. */
	void DashesByCutting(const FPathList& Pieces, double Dash, double Gap, double SkipFrom, double SkipTo, FPathList& Out)
	{
		Out.Clear();
		const double Period = Dash + Gap;
		if (Dash <= 0.0 || Period <= 1e-9)
		{
			return;
		}
		double Total = 0.0;
		for (size_t P = 0; P < Pieces.Num(); ++P)
		{
			Total += LengthOf(Pieces[P]);
		}
		auto Emit = [&](double U, double V)
		{
			double Start = 0.0;
			for (size_t P = 0; P < Pieces.Num(); ++P)
			{
				const double Long = LengthOf(Pieces[P]);
				const double A = std::max(U, Start), B = std::min(V, Start + Long);
				if (B > A + 1e-9)
				{
					FPath& Piece = Out.Add();
					SubPath(Pieces[P], A - Start, B - Start, Piece);
					if (Piece.size() < 2)
					{
						Out.Count--;
					}
				}
				Start += Long;
			}
		};
		for (int K = 0; K * Period < Total; ++K)
		{
			const double U = K * Period, V = std::min(U + Dash, Total);
			if (SkipTo > SkipFrom && V > SkipFrom && U < SkipTo)
			{
				if (SkipFrom > U)
				{
					Emit(U, SkipFrom);
				}
				if (SkipTo < V)
				{
					Emit(SkipTo, V);
				}
				continue;
			}
			Emit(U, V);
		}
	}

	bool SameDashes(const FPathList& A, const FPathList& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (size_t K = 0; K < A.Num(); ++K)
		{
			if (A[K].size() != B[K].size())
			{
				return false;
			}
			for (size_t I = 0; I < A[K].size(); ++I)
			{
				if (A[K][I].X != B[K][I].X || A[K][I].Y != B[K][I].Y)
				{
					return false;
				}
			}
		}
		return true;
	}

	/**
	 * The dashes in one pass: the same dashes, to the bit, as cutting each from its piece's start, on the rules' own lanes
	 * at every eye from the nearest the camera comes (750, UnitScale 10: thousands of dashes a lane) to the farthest, with
	 * a cut's gap and across the world's seam; and a lane of 200,000 points dashed in well under a second, where cutting
	 * each dash from the start would take minutes.
	 */
	void TestDashesOnePass(const char* Folder)
	{
		int Compared = 0;
		size_t Most = 0;
		const char* Files[] = { "look-cut-UK-June-1940.json", "look-bidder-UK-August-1941.json", "look-first-UK-October-1939.json" };
		for (const char* Name : Files)
		{
			FBook Book;
			if (!Folder || !ReadBook(ReadAll(std::string(Folder) + "/" + Name), Book))
			{
				continue;
			}
			FLookIn In;
			In.SrgbOf = ColourOf;
			FLook Look;
			LookAt(Book, In, Look);
			for (const double Eye : { 750.0, 1500.0, 4000.0, 12000.0, 30000.0 })
			{
				const double Pixel = PixelOf(Eye, 10.0, 1.0, 1000.0 / 900.0);
				for (const FLaneLook& Each : Look.Lanes)
				{
					FLaneShape Shape;
					LayLane(Book.Rows[Each.Row], Pixel, -500.0, Eye == 1500.0 ? 1200.0 : 0.0, Shape); // (at 1500, a narrow world: seams)
					FPathList Reference;
					DashesByCutting(Shape.Pieces, Lane::Dash * Pixel, Lane::Gap * Pixel, Shape.Gap.bOk ? Shape.Gap.From : -1.0, Shape.Gap.bOk ? Shape.Gap.To : -1.0, Reference);
					CHECK(SameDashes(Shape.Dashes, Reference));
					Most = std::max(Most, Shape.Dashes.Num());
					++Compared;
					// (and with a skip that starts before the first dash, ends past the last, or covers one piece)
					FPathList Mine;
					const double Long = Shape.Length;
					for (const auto& Skip : { std::pair<double, double>{ -5.0, Long * 0.3 }, { Long * 0.7, Long + 5.0 }, { Long * 0.4, Long * 0.41 } })
					{
						Dashes(Shape.Pieces, Lane::Dash * Pixel, Lane::Gap * Pixel, Skip.first, Skip.second, Mine);
						DashesByCutting(Shape.Pieces, Lane::Dash * Pixel, Lane::Gap * Pixel, Skip.first, Skip.second, Reference);
						CHECK(SameDashes(Mine, Reference));
					}
				}
			}
		}
		CHECK(Compared >= 60 && Most >= 1000);
		// Pieces with nothing in them, a point alone, and a piece of no length among the others.
		{
			FPathList Odd;
			Odd.Add() = FPath{};
			Odd.Add() = FPath{ { 0, 0 }, { 25, 0 } };
			Odd.Add() = FPath{ { 30, 0 } };
			Odd.Add() = FPath{ { 40, 0 }, { 40, 0 }, { 40, 0 } };
			Odd.Add() = FPath{ { 50, 0 }, { 50, 0 }, { 63, 0 }, { 63, 5 } };
			FPathList Mine, Reference;
			Dashes(Odd, 10.0, 7.0, -1.0, -1.0, Mine);
			DashesByCutting(Odd, 10.0, 7.0, -1.0, -1.0, Reference);
			CHECK(SameDashes(Mine, Reference) && Mine.Num() >= 3);
			Dashes(Odd, 10.0, 7.0, 12.0, 30.0, Mine);
			DashesByCutting(Odd, 10.0, 7.0, 12.0, 30.0, Reference);
			CHECK(SameDashes(Mine, Reference));
		}
		// The bound: 200,000 points a unit apart, a dash every 10: 20,000 dashes. Cutting each from the start is 20,000 x
		// 200,000 steps; one pass is about 400,000.
		{
			FPathList Long;
			FPath& Line = Long.Add();
			for (int K = 0; K < 200000; ++K)
			{
				Line.push_back(FPt{ static_cast<double>(K), (K % 2) * 0.01 });
			}
			FPathList Mine;
			const auto T0 = std::chrono::steady_clock::now();
			Dashes(Long, 7.0, 3.0, -1.0, -1.0, Mine);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
			CHECK(Mine.Num() >= 19990 && Mine.Num() <= 20010);
			CHECK(Ms < 1500.0);
			std::printf("dashes: %d lanes matched cut by cut (up to %zu dashes a lane); %zu dashes on 200000 points in %.1f ms\n", Compared, Most, Mine.Num(), Ms);
		}
	}

	/** October 1939, the first month with bargains: every round opens at once, and every seal must have a slot. */
	void TestFirstMonth(const char* Folder)
	{
		FBook Book;
		CHECK(Folder && ReadBook(ReadAll(std::string(Folder) + "/look-first-UK-October-1939.json"), Book) && Book.bOn && Book.Scale == 100.0);
		int Rounds = 0;
		for (const FRow& Row : Book.Rows)
		{
			Rounds += Row.Round ? 1 : 0;
		}
		FLookIn In;
		In.SrgbOf = ColourOf;
		FLook Look;
		LookAt(Book, In, Look);
		int Opens = 0;
		for (const FSealLook& Seal : Look.Seals)
		{
			Opens += Seal.bOpens ? 1 : 0;
		}
		CHECK(Rounds == 36 && Look.Seals.size() == 36 && Opens == 29);
		CHECK(Look.Seals.size() <= static_cast<size_t>(MostSeals));
		const size_t SealsNear = Look.Seals.size();
		// (the five that fell past the old 24 slots: each of them has its seal, and it opens Britain's card)
		for (const char* Id : { "gen-NOR", "gen-PAN", "gen-PER", "gen-URU", "gen-VEN" })
		{
			bool bFound = false;
			for (const FSealLook& Seal : Look.Seals)
			{
				bFound = bFound || (Book.Rows[Seal.Row].Id == Id && Seal.bOpens);
			}
			CHECK(bFound);
		}
		In.bFar = true;
		LookAt(Book, In, Look);
		CHECK(Look.Seals.size() == 29);
		std::printf("first month: %d rounds, %zu seals (%d open a card; %zu from far off), at most %d\n", Rounds, SealsNear, Opens, Look.Seals.size(), MostSeals);
	}
}

int main(int Count, char** Args)
{
	TestUnitsAndColours();
	TestWords();
	TestFlags();
	TestLookAt();
	TestOpenCard();
	TestCardLines();
	TestCard();
	TestPlace();
	TestLane();
	TestTint();
	TestSeals();
	TestCardSeal();
	TestTrayCard();
	TestPlates();
	TestNoAllocation();
	if (Count > 1)
	{
		TestSamples(Args[1]);
		TestDashesOnePass(Args[1]);
		TestFirstMonth(Args[1]);
	}
	else
	{
		std::printf("(no fixtures folder given: the rules' own look answers are not read)\n");
	}
	std::printf("bargain look: %d passed, %d failed\n", GPassed, GFailed);
	return GFailed == 0 ? 0 : 1;
}
