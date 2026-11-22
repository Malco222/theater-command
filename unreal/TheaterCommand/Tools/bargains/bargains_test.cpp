// The bargains' feed test (release 2: a neutral's bargains and the volunteer legions on the campaign map): reads the
// rules' real answers (fixtures/look-*.json, written by TC.bargainsLook()'s own function from a war played with both
// switches on) through Campaign/BargainFeed, the game's own code with no engine, and checks every field the map reads;
// then the reader's edges: escapes to UTF-8, fields unknown, missing or of another type, text that is not JSON, a
// text that does not end where its view does, and a few thousand damaged copies of the answers.
// Run by Tools/bargains/run.sh.

#include "Campaign/BargainFeed.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0;
	int Checks = 0;

	void Check(bool bOk, const char* What, int Line)
	{
		++Checks;
		if (!bOk)
		{
			++Failures;
			std::printf("FAILED (line %d): %s\n", Line, What);
		}
	}

#define EXPECT(Cond) Check((Cond), #Cond, __LINE__)

	bool Near(double A, double B)
	{
		return std::fabs(A - B) < 1e-9;
	}

	std::string Slurp(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream Text;
		Text << In.rdbuf();
		return Text.str();
	}

	using namespace Bargains;

	const FBook& Load(const std::string& Dir, const char* Name, FBook& Book)
	{
		std::string Error;
		const std::string Text = Slurp(Dir + "/" + Name);
		const bool bRead = ReadBook(Text, Book, &Error);
		Check(!Text.empty(), Name, __LINE__);
		Check(bRead && Error.empty(), (std::string(Name) + ": " + Error).c_str(), __LINE__);
		return Book;
	}

	/** What holds of every answer the rules give, whoever the viewer. */
	void EveryAnswer(const FBook& B, const char* Name)
	{
		const std::string Of = std::string(Name) + ": ";
		Check(B.bOn, (Of + "on").c_str(), __LINE__);
		Check(B.Nation(B.Viewer) != nullptr, (Of + "the viewer is named").c_str(), __LINE__);
		for (const FRow& R : B.Rows)
		{
			const std::string Row = Of + R.Id + ": ";
			Check(!R.Id.empty() && !R.Name.empty() && !R.Sign.empty() && R.Capital >= 0 && !R.CapitalName.empty(), (Row + "names").c_str(), __LINE__);
			Check(B.Nation(R.Seller) != nullptr, (Row + "the seller is named").c_str(), __LINE__);
			Check(R.Buyer.empty() || B.Nation(R.Buyer) != nullptr, (Row + "the buyer is named").c_str(), __LINE__);
			Check((R.Rel == ERel::None) == R.Buyer.empty(), (Row + "no buyer, no relation").c_str(), __LINE__);
			Check(R.Rel != ERel::Self || R.Buyer == B.Viewer, (Row + "self is the viewer").c_str(), __LINE__);
			Check(R.State != EState::None || R.Buyer.empty(), (Row + "a buyer's lane has a state").c_str(), __LINE__);
			Check(R.MonthsLeft >= 0 && R.MonthsLeft <= B.Term, (Row + "months left").c_str(), __LINE__);
			Check(R.Lean >= -100.0 && R.Lean <= 100.0, (Row + "lean").c_str(), __LINE__);
			Check(R.Base < 0 || R.Path.empty(), (Row + "a base right ships nothing").c_str(), __LINE__);
			for (const FPt& P : R.Path)
			{
				Check(std::isfinite(P.X) && std::isfinite(P.Y), (Row + "path").c_str(), __LINE__);
			}
			if (R.Round)
			{
				Check(R.Round->Months >= 1, (Row + "round months").c_str(), __LINE__);
				Check(!R.Round->bAccept || R.Seller == B.Viewer, (Row + "only the seller accepts").c_str(), __LINE__);
				Check(R.Round->bAccept || R.Seller != B.Viewer, (Row + "the seller accepts").c_str(), __LINE__);
				Check(!R.Round->Mine || !R.Round->bAccept, (Row + "the seller does not bid").c_str(), __LINE__);
				for (const FBid& Bid : R.Round->Bids)
				{
					Check(B.Nation(Bid.Nat) != nullptr && Bid.Offer > 0.0, (Row + "a bid").c_str(), __LINE__);
				}
				for (size_t K = 1; K < R.Round->Steps.size(); ++K)
				{
					Check(R.Round->Steps[K] > R.Round->Steps[K - 1], (Row + "steps rise").c_str(), __LINE__);
				}
			}
			if (R.Cut)
			{
				Check(R.State == EState::Cut && R.Cut->Sea >= 0 && !R.Cut->Name.empty() && !R.Cut->Month.empty() && R.Path.size() >= 2, (Row + "a cut").c_str(), __LINE__);
			}
			if (R.Loss)
			{
				Check(R.State != EState::Cut && R.Loss->Sea >= 0 && !R.Loss->Name.empty() && R.Loss->Month.empty(), (Row + "a loss").c_str(), __LINE__);
			}
		}
		for (const FLegion& L : B.Legions)
		{
			Check(B.Nation(L.Home) && B.Nation(L.Patron) && B.Nation(L.Against), (Of + "a legion's countries are named").c_str(), __LINE__);
			Check(L.Tour >= 1 && L.TourAt >= 1 && L.TourAt <= L.Tour && L.Pct >= 0 && L.Pct <= 100, (Of + "a legion's figures").c_str(), __LINE__);
			Check(L.Corps < 0 || B.LegionOfCorps(L.Corps) == &L, (Of + "a legion by its corps").c_str(), __LINE__);
		}
		for (const FNation& N : B.Nations)
		{
			Check(!N.Tag.empty() && !N.Name.empty() && !N.Adj.empty() && !N.The.empty() && N.Bloc >= -1 && N.Bloc <= 1, (Of + "a nation").c_str(), __LINE__);
		}
	}

	void TheAnswers(const std::string& Dir)
	{
		// Switched off: exactly {"on":false}.
		{
			FBook Off;
			Off.Rows.emplace_back();
			Load(Dir, "look-off.json", Off);
			EXPECT(!Off.bOn && Off.Rows.empty() && Off.Nations.empty() && Off.Legions.empty() && Off.Turn == 0);
		}

		// The player, a bidder: Britain, August 1941.
		{
			FBook B;
			Load(Dir, "look-bidder-UK-August-1941.json", B);
			EveryAnswer(B, "bidder");
			EXPECT(B.Viewer == "UK" && B.Month == "August" && B.Turn == 24 && B.Term == 12 && B.RoundLen == 1);
			EXPECT(B.Rows.size() == 24 && B.Nations.size() == 29 && B.Legions.size() == 1);
			const FNation* UK = B.Nation("UK");
			EXPECT(UK && UK->Name == "Britain" && UK->Adj == "British" && UK->Bloc == -1 && UK->The == "Britain");
			EXPECT(B.Nation("GER") && B.Nation("GER")->Bloc == 1 && B.Nation("ESP") && B.Nation("ESP")->Bloc == 0);
			EXPECT(B.Nation("DOM") == nullptr || B.Nation("DOM")->The == "the Dominican Republic");
			EXPECT(B.Nation("XXX") == nullptr && B.Nation("") == nullptr);

			const FRow* Ore = B.Row("swe-ore");
			EXPECT(Ore != nullptr);
			if (Ore)
			{
				EXPECT(Ore->Seller == "SWE" && Ore->SellerName == "Sweden" && Ore->Name == "Swedish iron ore" && Ore->Sign == "ore");
				EXPECT(Ore->Capital == 126 && Ore->CapitalName == "Stockholm" && Ore->Buyer == "GER" && Ore->MonthsLeft == 1 && Ore->Base == -1);
				EXPECT(!Ore->Price); // (Germany's price is sealed from Britain)
				EXPECT(Ore->Way == EWay::Sea && Ore->State == EState::Open && Ore->Rel == ERel::Enemy);
				EXPECT(!Ore->bGeneric && !Ore->bAgentSeller && !Ore->bAgentBuyer);
				EXPECT(Ore->Round.has_value());
				if (Ore->Round)
				{
					EXPECT(Ore->Round->Months == 1 && Ore->Round->Bids.size() == 1 && Ore->Round->Bids[0].Nat == "UK" && Near(Ore->Round->Bids[0].Offer, 1.5));
					EXPECT(Ore->Round->Mine && Near(*Ore->Round->Mine, 1.5));
					EXPECT(Ore->Round->Steps.size() == 5 && Near(Ore->Round->Steps[0], 0.5) && Near(Ore->Round->Steps[4], 2.5));
					EXPECT(Ore->Round->CanBid.empty() && !Ore->Round->bAccept);
				}
				EXPECT(Ore->Cargo.bMoved && Near(Ore->Cargo.Sup, 4.5) && Near(Ore->Cargo.Oil, 0.0) && !Ore->Cargo.Pp);
				EXPECT(Near(Ore->Lean, 15.07));
				// (the ore by the Baltic in summer: Norrbotten, the Gulf of Bothnia, the Baltic, a German harbour)
				EXPECT(Ore->Path.size() == 4 && Near(Ore->Path[0].X, 817.6) && Near(Ore->Path[0].Y, 168.0) && Near(Ore->Path[3].X, 658.2) && Near(Ore->Path[3].Y, 716.0));
				EXPECT(!Ore->Loss && !Ore->Cut);
			}
			const FRow* Chrome = B.Row("tur-chrome");
			EXPECT(Chrome && Chrome->Buyer == "UK" && Chrome->Rel == ERel::Self && Chrome->Price && Near(*Chrome->Price, 1.5));
			EXPECT(Chrome && Chrome->Way == EWay::Land && !Chrome->Round && Chrome->Cargo.Pp && Near(*Chrome->Cargo.Pp, 0.0) && Chrome->Path.size() >= 2);
			const FRow* Swiss = B.Row("sui-goods");
			EXPECT(Swiss && !Swiss->bGeneric && Swiss->Round && Swiss->Round->CanBid == "No way is open for the goods between Switzerland and the United Kingdom.");
			EXPECT(Swiss && Swiss->Round && Swiss->Round->Bids.empty() && !Swiss->Round->Mine);
			const FRow* Brazil = B.Row("bra-airfields");
			EXPECT(Brazil && Brazil->Base == 559 && Brazil->Path.empty() && Brazil->Sign == "base" && Brazil->Buyer == "USA" && Brazil->Rel == ERel::Other && !Brazil->Cargo.bMoved);
			EXPECT(B.Row("no-such-row") == nullptr);

			int Enemy = 0, Self = 0, Ally = 0, Generic = 0, Rounds = 0, Drawn = 0;
			for (const FRow& R : B.Rows)
			{
				Enemy += R.Rel == ERel::Enemy;
				Self += R.Rel == ERel::Self;
				Ally += R.Rel == ERel::Ally;
				Generic += R.bGeneric;
				Rounds += R.Round.has_value();
				Drawn += R.Path.size() >= 2;
			}
			EXPECT(Enemy == 4 && Self == 2 && Ally == 17 && Generic == 16 && Rounds == 3 && Drawn == 23);

			const FLegion* Blue = B.LegionOfCorps(363);
			EXPECT(Blue != nullptr);
			if (Blue)
			{
				EXPECT(Blue->Home == "ESP" && Blue->HomeName == "Spain" && Blue->Patron == "GER" && Blue->Against == "SOV" && Blue->Name == "Blue Division");
				EXPECT(Blue->Region == 214 && Blue->Place == "Minsk" && Blue->HomeCapital == 159 && Blue->Pct == 100);
				EXPECT(Blue->Tour == 28 && Blue->TourAt == 1 && Blue->MonthsLeft == 28 && !Blue->bRecall);
			}
			EXPECT(B.LegionOfCorps(-1) == nullptr && B.LegionOfCorps(1) == nullptr);
		}

		// A seller with an open round: Sweden sees every bid on its ore, accepts, and does not bid.
		{
			FBook B;
			Load(Dir, "look-seller-SWE-August-1941.json", B);
			EveryAnswer(B, "seller");
			const FRow* Ore = B.Row("swe-ore");
			EXPECT(Ore && Ore->Price && Near(*Ore->Price, 4.96) && Ore->Rel == ERel::Other);
			EXPECT(Ore && Ore->Round && Ore->Round->bAccept && !Ore->Round->Mine && Ore->Round->Steps.empty() && Ore->Round->CanBid == "It is yours to sell.");
			EXPECT(Ore && Ore->Round && Ore->Round->Bids.size() == 6 && Ore->Round->Bids[0].Nat == "GER" && Near(Ore->Round->Bids[0].Offer, 5.18) && Ore->Round->Bids[5].Nat == "UK");
		}

		// An outsider: the United States, at peace, no agent. No bid of anybody's is seen.
		{
			FBook B;
			Load(Dir, "look-outsider-USA-August-1941.json", B);
			EveryAnswer(B, "outsider");
			const FRow* Ore = B.Row("swe-ore");
			EXPECT(Ore && !Ore->Price && Ore->Round && Ore->Round->Bids.empty() && Ore->Round->CanBid == "Only a country at war bids for it." && Ore->Round->Steps.size() == 6);
			EXPECT(B.Nation("USA") && B.Nation("USA")->The == "the United States" && B.Nation("USA")->Name == "United States");
		}

		// The player with an agent at work in Stockholm: he sees the rival bids on Sweden's rows, and the price.
		{
			FBook B;
			Load(Dir, "look-agent-UK-August-1941.json", B);
			EveryAnswer(B, "agent");
			const FRow* Ore = B.Row("swe-ore");
			EXPECT(Ore && Ore->bAgentSeller && !Ore->bAgentBuyer && Ore->Price && Near(*Ore->Price, 4.96));
			EXPECT(Ore && Ore->Round && Ore->Round->Bids.size() == 6 && Ore->Round->Mine && Near(*Ore->Round->Mine, 1.5));
			int Agents = 0;
			for (const FRow& R : B.Rows)
			{
				Agents += R.bAgentSeller;
				EXPECT(R.bAgentSeller == (R.Seller == "SWE"));
			}
			EXPECT(Agents >= 1);
		}

		// The month a lane is first seen cut off at sea: Finland's exports to Britain, June 1940, by the Norwegian Sea.
		{
			FBook B;
			Load(Dir, "look-cut-UK-June-1940.json", B);
			EveryAnswer(B, "cut");
			EXPECT(B.Month == "June" && B.Turn == 10 && B.Rows.size() == 32 && B.Legions.empty());
			const FRow* Fin = B.Row("gen-FIN");
			EXPECT(Fin && Fin->State == EState::Cut && Fin->Way == EWay::None && Fin->Rel == ERel::Self && Fin->Path.size() == 5);
			EXPECT(Fin && Fin->Cut && Fin->Cut->Sea == 7 && Fin->Cut->Name == "Norwegian Sea" && Near(Fin->Cut->At.X, 385.7) && Near(Fin->Cut->At.Y, 208.0) && Fin->Cut->Month == "June");
			EXPECT(Fin && !Fin->Loss && !Fin->Cargo.bMoved);
			const FRow* Wolfram = B.Row("esp-wolfram");
			EXPECT(Wolfram && Wolfram->State == EState::Raided && Wolfram->Loss && Wolfram->Loss->Sea == 22 && Wolfram->Loss->Name == "Iceland Gap");
			EXPECT(Wolfram && Wolfram->Loss && Near(Wolfram->Loss->At.X, -167.1) && Near(Wolfram->Loss->At.Y, 404.0) && !Wolfram->Cut);
		}
	}

	/** The reader's edges, on answers written here. */
	void TheReader()
	{
		std::string Error;
		FBook B;

		// Escapes become UTF-8; raw UTF-8 passes as it is.
		EXPECT(ReadBook(R"({"on":true,"month":"\u00b7\u20ac\ud83d\ude00 \"q\" \\ \/ \b\f\n\r\t","viewer":"Cura\u00e7ao"})", B, &Error) && Error.empty());
		EXPECT(B.Month == "\xC2\xB7\xE2\x82\xAC\xF0\x9F\x98\x80 \"q\" \\ / \b\f\n\r\t" && B.Viewer == "Cura\xC3\xA7" "ao");
		EXPECT(ReadBook("{\"on\":true,\"viewer\":\"Cura\xC3\xA7" "ao\"}", B) && B.Viewer == "Cura\xC3\xA7" "ao");
		// (a lone half of a surrogate pair is the replacement character; an escape after a lone high half is read on its own)
		EXPECT(ReadBook(R"({"on":true,"month":"\ud800x\udc00\ud800\u0041"})", B) && B.Month == "\xEF\xBF\xBDx\xEF\xBF\xBD\xEF\xBF\xBD" "A");
		EXPECT(ReadBook(R"({"on":true,"month":"\u00"})", B, &Error) == false && !Error.empty());
		EXPECT(ReadBook(R"({"on":true,"month":"\x"})", B) == false);

		// Unknown fields are passed over, nested ones too; missing ones keep their defaults.
		EXPECT(ReadBook(R"({"_about":"x","on":true,"later":{"a":[1,{"b":null}],"c":[[]]},"rows":[{"id":"r","extra":[true,false,null,-1.5e-3]}],"legions":[{}]})", B));
		EXPECT(B.bOn && B.Viewer.empty() && B.Turn == 0 && B.Term == 12 && B.RoundLen == 1 && B.Rows.size() == 1 && B.Legions.size() == 1);
		if (B.Rows.size() == 1)
		{
			const FRow& R = B.Rows[0];
			EXPECT(R.Id == "r" && R.Capital == -1 && R.Base == -1 && R.MonthsLeft == 0 && !R.Price && R.Way == EWay::None && R.State == EState::None && R.Rel == ERel::None);
			EXPECT(!R.Round && !R.Cargo.bMoved && !R.Cargo.Pp && R.Path.empty() && !R.Loss && !R.Cut && R.Buyer.empty());
		}
		if (B.Legions.size() == 1)
		{
			EXPECT(B.Legions[0].Corps == -1 && B.Legions[0].Region == -1 && B.Legions[0].HomeCapital == -1 && B.Legions[0].Tour == 0);
		}

		// A field of another type is as if missing; a list where none is, empty; a bid with no figure, left out.
		EXPECT(ReadBook(R"({"on":true,"turn":"7","term":null,"viewer":3,"nations":[1],"rows":{"id":"x"},"legions":"none"})", B));
		EXPECT(B.bOn && B.Turn == 0 && B.Term == 12 && B.Viewer.empty() && B.Nations.empty() && B.Rows.empty() && B.Legions.empty());
		EXPECT(ReadBook(R"({"on":true,"rows":[1,"x",{"id":"a","price":"9","buyer":null,"round":{"bids":[{"nat":"GER","offer":null},{"nat":"ITA"},{"nat":"HUN","offer":2}],"mine":null,"canBid":null,"steps":[0.5,"1",1]},"cargo":{"moved":true,"pp":null},"path":[1,2,3,"4",5,6,7]}]})", B));
		EXPECT(B.Rows.size() == 1);
		if (B.Rows.size() == 1)
		{
			const FRow& R = B.Rows[0];
			EXPECT(!R.Price && R.Buyer.empty() && R.Round && R.Round->Bids.size() == 1 && R.Round->Bids[0].Nat == "HUN" && Near(R.Round->Bids[0].Offer, 2.0));
			EXPECT(R.Round && !R.Round->Mine && R.Round->CanBid.empty() && R.Round->Steps.size() == 2 && R.Round->Months == 1);
			EXPECT(R.Cargo.bMoved && !R.Cargo.Pp);
			// (pairs: (1,2), then (3,"4") is no point, then (5,6); the odd 7 is left over)
			EXPECT(R.Path.size() == 2 && Near(R.Path[0].X, 1.0) && Near(R.Path[1].X, 5.0) && Near(R.Path[1].Y, 6.0));
		}
		// A nation's side and its name after a word, where the answer gives none.
		EXPECT(ReadBook(R"({"on":true,"nations":{"A":{"name":"Aa","adj":"Ax","bloc":"axis"},"B":{"name":"Bb","bloc":"allies","the":"the Bb"},"C":{"bloc":"league"},"D":7}})", B));
		EXPECT(B.Nations.size() == 3 && B.Nation("A") && B.Nation("A")->Bloc == 1 && B.Nation("A")->The == "Aa" && B.Nation("B")->Bloc == -1 && B.Nation("B")->The == "the Bb" && B.Nation("C")->Bloc == 0 && !B.Nation("D"));

		// Off, however much else the answer says; "on" must be true itself.
		EXPECT(ReadBook(R"({"on":false,"rows":[{"id":"a"}],"turn":5})", B) && !B.bOn && B.Rows.empty() && B.Turn == 0);
		EXPECT(ReadBook(R"({"on":"yes","rows":[{"id":"a"}]})", B) && !B.bOn && B.Rows.empty());
		EXPECT(ReadBook(R"({})", B) && !B.bOn);

		// Numbers: whole numbers rounded and held within an int; a figure too large for a double is 0.
		EXPECT(ReadBook(R"({"on":true,"turn":1e12,"term":-1e12,"round":2.6,"rows":[{"lean":1e999,"capital":-0.4}]})", B));
		EXPECT(B.Turn == 2147483647 && B.Term == -2147483647 - 1 && B.RoundLen == 3 && B.Rows.size() == 1 && Near(B.Rows[0].Lean, 0.0) && B.Rows[0].Capital == 0);

		// Text that is not JSON, or not an object: false, the book emptied and off, and why.
		const char* Bad[] = {
			"", " ", "{", "}", "[", "[1,2]", "\"on\"", "true", "nul", "{\"on\":true,}", "{\"on\":true", "{\"on\" true}", "{on:true}",
			"{\"on\":tru}", "{\"a\":\"x}", "{\"a\":\"\x01\"}", "{\"a\":1} x", "{\"a\":-}", "{\"a\":1.}", "{\"a\":1e}", "{\"a\":.5}",
			"{\"a\":[1,]}", "{\"a\":[1 2]}", "{\"a\":+1}", "{\"a\":\"\\", "{\"a\":\"\\u12\"}", "{,}", "{\"a\":1,,\"b\":2}",
		};
		for (const char* Text : Bad)
		{
			FBook Full;
			Full.bOn = true;
			Full.Rows.emplace_back();
			Full.Nations.emplace_back();
			std::string Why;
			const bool bRead = ReadBook(Text, Full, &Why);
			Check(!bRead && !Why.empty() && !Full.bOn && Full.Rows.empty() && Full.Nations.empty(), Text, __LINE__);
		}
		// (no error wanted: still false)
		EXPECT(!ReadBook("{", B));
		// Error is cleared on a good read.
		Error = "stale";
		EXPECT(ReadBook("{\"on\":false}", B, &Error) && Error.empty());

		// Nested too deep: refused, never a crash.
		{
			std::string Deep = "{\"a\":";
			for (int K = 0; K < 200; ++K)
			{
				Deep += "[";
			}
			for (int K = 0; K < 200; ++K)
			{
				Deep += "]";
			}
			Deep += "}";
			EXPECT(!ReadBook(Deep, B, &Error) && Error.find("deep") != std::string::npos);
		}

		// A view that is not the end of its text: read to the view's end and no further (under the address sanitizer the
		// buffer ends exactly where the view does, with no terminator).
		{
			const std::string Whole = "{\"on\":true,\"turn\":75}";
			const size_t N = Whole.size();
			std::unique_ptr<char[]> Exact(new char[N]);
			std::memcpy(Exact.get(), Whole.data(), N);
			EXPECT(ReadBook(std::string_view(Exact.get(), N), B) && B.Turn == 75);
			EXPECT(!ReadBook(std::string_view(Exact.get(), N - 1), B));
			const std::string_view Cut(Exact.get(), N - 2); // ends inside the number: "...\"turn\":7"
			EXPECT(!ReadBook(Cut, B));
			const std::string More = "{\"on\":true,\"turn\":7}5";
			EXPECT(ReadBook(std::string_view(More.data(), More.size() - 1), B) && B.Turn == 7);
		}
	}

	/** Damaged copies of the answers: every one read or refused, never a crash (the sanitizers are the check). */
	void Damaged(const std::string& Dir)
	{
		const std::string Texts[] = { Slurp(Dir + "/look-bidder-UK-August-1941.json"), Slurp(Dir + "/look-cut-UK-June-1940.json") };
		std::mt19937 Dice(1941);
		const char Bytes[] = "{}[]\",:\\u0123456789.eE+-tfnl \x01\xC3\xA7\xFF";
		int Read = 0, Refused = 0;
		for (int K = 0; K < 3000; ++K)
		{
			std::string Text = Texts[K % 2];
			if (Text.empty())
			{
				break;
			}
			const int Hurts = 1 + static_cast<int>(Dice() % 4);
			for (int H = 0; H < Hurts; ++H)
			{
				const size_t At = Dice() % Text.size();
				switch (Dice() % 4)
				{
				case 0: Text[At] = Bytes[Dice() % (sizeof(Bytes) - 1)]; break;
				case 1: Text.erase(At, 1 + Dice() % 8); break;
				case 2: Text.insert(At, 1, Bytes[Dice() % (sizeof(Bytes) - 1)]); break;
				default: Text.resize(At); break;
				}
				if (Text.empty())
				{
					break;
				}
			}
			FBook B;
			std::string Why;
			if (ReadBook(Text, B, &Why))
			{
				++Read;
				for (const FRow& R : B.Rows)
				{
					for (const FPt& P : R.Path)
					{
						Check(std::isfinite(P.X) && std::isfinite(P.Y), "a damaged path's points are numbers", __LINE__);
					}
				}
			}
			else
			{
				++Refused;
				Check(!Why.empty() && !B.bOn && B.Rows.empty(), "a refused text leaves the book off", __LINE__);
			}
		}
		EXPECT(Read + Refused == 3000 && Refused > 0);
		std::printf("damaged answers: %d read, %d refused\n", Read, Refused);
	}
}

int main(int Argc, char** Argv)
{
	const std::string Dir = Argc > 1 ? Argv[1] : "fixtures";
	TheAnswers(Dir);
	TheReader();
	Damaged(Dir);
	std::printf("%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
