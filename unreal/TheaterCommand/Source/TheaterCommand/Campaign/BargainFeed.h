#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * (Release 2: a neutral's bargains and the volunteer legions on the campaign map. Build contract
 * production/r2-bargains-legions/contract.md, part A.)
 *
 * What the map reads of them, as plain data: the answer of the rules' TC.bargainsLook() (Rules/bargains-look.ts),
 * read from its JSON. Both are switched off in release 1 (stage.json 'bargains' and 'legions'), and then the answer is
 * {"on":false} and the book is empty with bOn false.
 *
 * This file has no engine in it: std only, no exceptions, so Tools/bargains/run.sh builds it with g++ and clang and
 * checks it under the address and undefined-behaviour sanitizers. Text is UTF-8 as the rules wrote it (a "\u" escape
 * becomes its UTF-8 bytes). A figure the viewer may not see is left unset (std::nullopt), never zero: the map draws "?".
 * Nothing here is saved.
 */
namespace Bargains
{
	/** A point in map units, as FViewLane::Path has them. */
	struct FPt { double X = 0, Y = 0; };
	using FPath = std::vector<FPt>;

	/** How a bargain's goods go this month. */
	enum class EWay : uint8_t { None, Land, Sea };
	/** Its lane: no buyer, open, raided (some cargoes sunk), or cut (nothing arrives). */
	enum class EState : uint8_t { None, Open, Raided, Cut };
	/** Who the buyer is to the viewer: no buyer, the viewer, an ally, an enemy, or anybody else. */
	enum class ERel : uint8_t { None, Self, Ally, Enemy, Other };

	/** A country named in the book: its tag, its name ("Britain"), its adjective, and its side (+1 axis, -1 allies, 0 neither). */
	struct FNation
	{
		std::string Tag, Name, Adj;
		int Bloc = 0;
		/** Its name as it follows another word: "the United States", "Britain". */
		std::string The;
	};

	/** A bid the viewer can see: the bidder and its figure, in production points a month. */
	struct FBid { std::string Nat; double Offer = 0; };

	/**
	 * A round of bids, while one is open: the months it has been open, the bids the viewer can see (the rules leave
	 * out a rival's unseen bid altogether), the viewer's own bid, the steps it may bid, why it may not ("" when it may),
	 * and whether the viewer is the seller, who accepts.
	 */
	struct FRound
	{
		int Months = 1;
		std::vector<FBid> Bids;
		std::optional<double> Mine;
		std::vector<double> Steps;
		std::string CanBid;
		bool bAccept = false;
	};

	/** What reached the buyer this month: supply and oil, and the production it kept going (unset where the viewer may not read the buyer's books). */
	struct FCargo { bool bMoved = false; double Sup = 0, Oil = 0; std::optional<double> Pp; };

	/** A sea on the map: where a lane loses its cargoes, or where it was cut and the month it was first seen cut. */
	struct FSeaMark { int Sea = -1; std::string Name, Month; FPt At; };

	/** One bargain with a buyer or a round open, as the viewer's map shows it. */
	struct FRow
	{
		/** Id, the seller's tag and name, the goods' name, its sign (ore, oil, chrome, wolfram, beef, exports, base), the seller's capital's name, and the buyer ("" for none). */
		std::string Id, Seller, SellerName, Name, Sign, CapitalName, Buyer;
		/** The seller's capital (a region), the term's months left, and for a right to a base the base's region (else -1: nothing ships). */
		int Capital = -1, MonthsLeft = 0, Base = -1;
		/** The term's price a month, where the viewer can see it. */
		std::optional<double> Price;
		EWay Way{};
		EState State{};
		ERel Rel{};
		/** One of the generic rows ("its exports"); an agent of the viewer's at work in the seller's capital; in the buyer's. */
		bool bGeneric = false, bAgentSeller = false, bAgentBuyer = false;
		std::optional<FRound> Round;
		FCargo Cargo;
		/** The seller's public leaning, -100 (the Allies) to +100 (the Axis). */
		double Lean = 0;
		/** The line the goods follow, seller first (empty where nothing is drawn); the sea where most are lost; where it was cut. */
		FPath Path;
		std::optional<FSeaMark> Loss, Cut;
	};

	/** A legion of volunteers abroad. */
	struct FLegion
	{
		/** Its home's tag and name, its patron, the enemy it was sent to fight, its name, and the province it stands in ("" on its way). */
		std::string Home, HomeName, Patron, Against, Name, Place;
		/** Its corps (-1 on its way), its region (-1), its home's capital, its strength in hundredths, the month of its tour (1..Tour), its tour in months, and the months left. */
		int Corps = -1, Region = -1, HomeCapital = -1, Pct = 0, TourAt = 0, Tour = 0, MonthsLeft = 0;
		/** Called home: it leaves as the next month opens. */
		bool bRecall = false;
	};

	/** The whole answer: off, or the viewer's bargains and every legion, with the countries they name. */
	struct FBook
	{
		bool bOn = false;
		/** The viewer's tag, and the month's name ("August"). */
		std::string Viewer, Month;
		/** The turn, the term of a sale in months, and a round's length in months. */
		int Turn = 0, Term = 12, RoundLen = 1;
		std::vector<FNation> Nations;
		std::vector<FRow> Rows;
		std::vector<FLegion> Legions;

		/** The country of that tag, or nullptr. */
		const FNation* Nation(std::string_view Tag) const;
		/** The legion whose corps that is, or nullptr (never for -1). */
		const FLegion* LegionOfCorps(int Corps) const;
		/** The row of that id, or nullptr. */
		const FRow* Row(std::string_view Id) const;
	};

	/**
	 * Reads TC.bargainsLook()'s answer into Out, replacing what it held. Fields it does not know are passed over, and
	 * one missing (or of another type) keeps its default. Text that is not JSON, or not an object, leaves Out empty and
	 * off, puts the reason in Error (when given), and returns false.
	 */
	bool ReadBook(std::string_view Json, FBook& Out, std::string* Error = nullptr);
}
