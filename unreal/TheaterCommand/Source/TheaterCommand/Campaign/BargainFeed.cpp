#include "Campaign/BargainFeed.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Bargains
{
	namespace
	{
		/** A JSON value, as read: the book is small (some 16 KB), read once each time the map's picture changes. */
		struct FValue
		{
			enum class EKind : uint8_t { Null, Flag, Number, String, Array, Object };
			EKind Kind = EKind::Null;
			bool bFlag = false;
			double Number = 0.0;
			std::string Text;
			std::vector<FValue> Items;
			/** An object's members, in their order: Keys[K] names Items[K]. */
			std::vector<std::string> Keys;

			bool IsNumber() const { return Kind == EKind::Number; }
			bool IsString() const { return Kind == EKind::String; }
			bool IsArray() const { return Kind == EKind::Array; }
			bool IsObject() const { return Kind == EKind::Object; }

			/** An object's member, or nullptr where it has none of that name (or is no object). */
			const FValue* Get(std::string_view Key) const
			{
				if (Kind != EKind::Object)
				{
					return nullptr;
				}
				for (size_t K = 0; K < Keys.size(); ++K)
				{
					if (Keys[K] == Key)
					{
						return &Items[K];
					}
				}
				return nullptr;
			}
		};

		/** A strict JSON reader over the text, never reading past its end, with no exceptions. */
		class FReader
		{
		public:
			explicit FReader(std::string_view InText) : Text(InText) {}

			bool Document(FValue& Out)
			{
				Space();
				if (!Value(Out, 0))
				{
					return false;
				}
				Space();
				return At == Text.size() || Fail("more text after the value");
			}

			std::string Why;

		private:
			static constexpr int MostDepth = 64;
			std::string_view Text;
			size_t At = 0;

			bool Fail(const char* What)
			{
				if (Why.empty())
				{
					Why = std::string(What) + ", at character " + std::to_string(At);
				}
				return false;
			}

			void Space()
			{
				while (At < Text.size() && (Text[At] == ' ' || Text[At] == '\t' || Text[At] == '\n' || Text[At] == '\r'))
				{
					++At;
				}
			}

			bool Word(std::string_view W)
			{
				if (Text.substr(At, W.size()) == W)
				{
					At += W.size();
					return true;
				}
				return false;
			}

			bool Value(FValue& Out, int Depth)
			{
				if (Depth > MostDepth)
				{
					return Fail("nested too deep");
				}
				if (At >= Text.size())
				{
					return Fail("a value was expected");
				}
				const char C = Text[At];
				if (C == '{')
				{
					return Object(Out, Depth);
				}
				if (C == '[')
				{
					return Array(Out, Depth);
				}
				if (C == '"')
				{
					Out.Kind = FValue::EKind::String;
					return String(Out.Text);
				}
				if (Word("true"))
				{
					Out.Kind = FValue::EKind::Flag;
					Out.bFlag = true;
					return true;
				}
				if (Word("false"))
				{
					Out.Kind = FValue::EKind::Flag;
					Out.bFlag = false;
					return true;
				}
				if (Word("null"))
				{
					Out.Kind = FValue::EKind::Null;
					return true;
				}
				return NumberValue(Out);
			}

			bool Object(FValue& Out, int Depth)
			{
				++At; // '{'
				Out.Kind = FValue::EKind::Object;
				Space();
				if (At < Text.size() && Text[At] == '}')
				{
					++At;
					return true;
				}
				for (;;)
				{
					Space();
					if (At >= Text.size() || Text[At] != '"')
					{
						return Fail("a member's name was expected");
					}
					std::string Key;
					if (!String(Key))
					{
						return false;
					}
					Space();
					if (At >= Text.size() || Text[At] != ':')
					{
						return Fail("a ':' was expected");
					}
					++At;
					Space();
					FValue Item;
					if (!Value(Item, Depth + 1))
					{
						return false;
					}
					Out.Keys.push_back(std::move(Key));
					Out.Items.push_back(std::move(Item));
					Space();
					if (At < Text.size() && Text[At] == ',')
					{
						++At;
						continue;
					}
					if (At < Text.size() && Text[At] == '}')
					{
						++At;
						return true;
					}
					return Fail("a ',' or '}' was expected");
				}
			}

			bool Array(FValue& Out, int Depth)
			{
				++At; // '['
				Out.Kind = FValue::EKind::Array;
				Space();
				if (At < Text.size() && Text[At] == ']')
				{
					++At;
					return true;
				}
				for (;;)
				{
					Space();
					FValue Item;
					if (!Value(Item, Depth + 1))
					{
						return false;
					}
					Out.Items.push_back(std::move(Item));
					Space();
					if (At < Text.size() && Text[At] == ',')
					{
						++At;
						continue;
					}
					if (At < Text.size() && Text[At] == ']')
					{
						++At;
						return true;
					}
					return Fail("a ',' or ']' was expected");
				}
			}

			static int HexDigit(char C)
			{
				return C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : C >= 'A' && C <= 'F' ? C - 'A' + 10 : -1;
			}

			bool Hex4(unsigned& Out)
			{
				if (Text.size() - At < 4)
				{
					return Fail("four hex digits were expected");
				}
				Out = 0;
				for (int K = 0; K < 4; ++K)
				{
					const int D = HexDigit(Text[At + static_cast<size_t>(K)]);
					if (D < 0)
					{
						return Fail("four hex digits were expected");
					}
					Out = Out * 16u + static_cast<unsigned>(D);
				}
				At += 4;
				return true;
			}

			static void PutUtf8(std::string& Out, unsigned Code)
			{
				if (Code < 0x80u)
				{
					Out += static_cast<char>(Code);
				}
				else if (Code < 0x800u)
				{
					Out += static_cast<char>(0xC0u | (Code >> 6));
					Out += static_cast<char>(0x80u | (Code & 0x3Fu));
				}
				else if (Code < 0x10000u)
				{
					Out += static_cast<char>(0xE0u | (Code >> 12));
					Out += static_cast<char>(0x80u | ((Code >> 6) & 0x3Fu));
					Out += static_cast<char>(0x80u | (Code & 0x3Fu));
				}
				else
				{
					Out += static_cast<char>(0xF0u | (Code >> 18));
					Out += static_cast<char>(0x80u | ((Code >> 12) & 0x3Fu));
					Out += static_cast<char>(0x80u | ((Code >> 6) & 0x3Fu));
					Out += static_cast<char>(0x80u | (Code & 0x3Fu));
				}
			}

			/** A string, its escapes undone ("\u" to UTF-8; a lone half of a surrogate pair becomes U+FFFD). */
			bool String(std::string& Out)
			{
				++At; // '"'
				for (;;)
				{
					if (At >= Text.size())
					{
						return Fail("the text ends inside a string");
					}
					const char C = Text[At++];
					if (C == '"')
					{
						return true;
					}
					if (static_cast<unsigned char>(C) < 0x20u)
					{
						return Fail("a control character inside a string");
					}
					if (C != '\\')
					{
						Out += C;
						continue;
					}
					if (At >= Text.size())
					{
						return Fail("the text ends inside an escape");
					}
					const char E = Text[At++];
					switch (E)
					{
					case '"': Out += '"'; break;
					case '\\': Out += '\\'; break;
					case '/': Out += '/'; break;
					case 'b': Out += '\b'; break;
					case 'f': Out += '\f'; break;
					case 'n': Out += '\n'; break;
					case 'r': Out += '\r'; break;
					case 't': Out += '\t'; break;
					case 'u':
					{
						unsigned Code = 0;
						if (!Hex4(Code))
						{
							return false;
						}
						if (Code >= 0xD800u && Code <= 0xDBFFu)
						{
							unsigned Low = 0;
							if (Text.size() - At >= 6 && Text[At] == '\\' && Text[At + 1] == 'u')
							{
								const size_t Back = At;
								At += 2;
								if (!Hex4(Low))
								{
									return false;
								}
								if (Low >= 0xDC00u && Low <= 0xDFFFu)
								{
									Code = 0x10000u + ((Code - 0xD800u) << 10) + (Low - 0xDC00u);
								}
								else
								{
									At = Back; // (not a low half: the next escape is read on its own)
									Code = 0xFFFDu;
								}
							}
							else
							{
								Code = 0xFFFDu;
							}
						}
						else if (Code >= 0xDC00u && Code <= 0xDFFFu)
						{
							Code = 0xFFFDu;
						}
						PutUtf8(Out, Code);
						break;
					}
					default:
						return Fail("an unknown escape");
					}
				}
			}

			/** A number as JSON writes one, read with strtod from a copy of its own characters (the text need not end after it). */
			bool NumberValue(FValue& Out)
			{
				const size_t Begin = At;
				auto Digits = [this]() -> size_t
				{
					const size_t From = At;
					while (At < Text.size() && Text[At] >= '0' && Text[At] <= '9')
					{
						++At;
					}
					return At - From;
				};
				if (At < Text.size() && Text[At] == '-')
				{
					++At;
				}
				if (Digits() == 0)
				{
					At = Begin;
					return Fail("a value was expected");
				}
				if (At < Text.size() && Text[At] == '.')
				{
					++At;
					if (Digits() == 0)
					{
						return Fail("digits were expected after the point");
					}
				}
				if (At < Text.size() && (Text[At] == 'e' || Text[At] == 'E'))
				{
					++At;
					if (At < Text.size() && (Text[At] == '+' || Text[At] == '-'))
					{
						++At;
					}
					if (Digits() == 0)
					{
						return Fail("digits were expected in the exponent");
					}
				}
				const std::string Copy(Text.substr(Begin, At - Begin));
				char* End = nullptr;
				const double Got = std::strtod(Copy.c_str(), &End);
				if (End != Copy.c_str() + Copy.size())
				{
					return Fail("a number that cannot be read");
				}
				Out.Kind = FValue::EKind::Number;
				Out.Number = std::isfinite(Got) ? Got : 0.0;
				return true;
			}
		};

		// ---- From the values to the book: a member missing, or of another type, keeps the default. -------------

		std::string TextOf(const FValue& Object, std::string_view Key)
		{
			const FValue* V = Object.Get(Key);
			return V && V->IsString() ? V->Text : std::string();
		}

		double NumberOf(const FValue& Object, std::string_view Key, double Default = 0.0)
		{
			const FValue* V = Object.Get(Key);
			return V && V->IsNumber() ? V->Number : Default;
		}

		std::optional<double> MaybeNumber(const FValue& Object, std::string_view Key)
		{
			const FValue* V = Object.Get(Key);
			return V && V->IsNumber() ? std::optional<double>(V->Number) : std::nullopt;
		}

		int WholeOf(const FValue& Object, std::string_view Key, int Default)
		{
			const FValue* V = Object.Get(Key);
			if (!V || !V->IsNumber())
			{
				return Default;
			}
			const double X = std::round(V->Number);
			return X <= static_cast<double>(INT_MIN) ? INT_MIN : X >= static_cast<double>(INT_MAX) ? INT_MAX : static_cast<int>(X);
		}

		bool FlagOf(const FValue& Object, std::string_view Key)
		{
			const FValue* V = Object.Get(Key);
			return V && V->Kind == FValue::EKind::Flag && V->bFlag;
		}

		const std::vector<FValue>& ListOf(const FValue& Object, std::string_view Key)
		{
			static const std::vector<FValue> Empty;
			const FValue* V = Object.Get(Key);
			return V && V->IsArray() ? V->Items : Empty;
		}

		EWay WayOf(const std::string& Way)
		{
			return Way == "land" ? EWay::Land : Way == "sea" ? EWay::Sea : EWay::None;
		}

		EState StateOf(const std::string& State)
		{
			return State == "open" ? EState::Open : State == "raided" ? EState::Raided : State == "cut" ? EState::Cut : EState::None;
		}

		ERel RelOf(const std::string& Rel)
		{
			return Rel == "self" ? ERel::Self : Rel == "ally" ? ERel::Ally : Rel == "enemy" ? ERel::Enemy : Rel == "other" ? ERel::Other : ERel::None;
		}

		std::optional<FSeaMark> SeaMarkOf(const FValue& Row, std::string_view Key)
		{
			const FValue* V = Row.Get(Key);
			if (!V || !V->IsObject())
			{
				return std::nullopt;
			}
			FSeaMark Mark;
			Mark.Sea = WholeOf(*V, "sea", -1);
			Mark.Name = TextOf(*V, "name");
			Mark.Month = TextOf(*V, "month");
			Mark.At = FPt{ NumberOf(*V, "x"), NumberOf(*V, "y") };
			return Mark;
		}

		std::optional<FRound> RoundOf(const FValue& Row)
		{
			const FValue* V = Row.Get("round");
			if (!V || !V->IsObject())
			{
				return std::nullopt;
			}
			FRound R;
			R.Months = WholeOf(*V, "months", 1);
			for (const FValue& B : ListOf(*V, "bids"))
			{
				// (A bid the viewer may not see is left out by the rules; one sent with no figure is left out here too.)
				const FValue* Offer = B.Get("offer");
				if (B.IsObject() && Offer && Offer->IsNumber())
				{
					R.Bids.push_back(FBid{ TextOf(B, "nat"), Offer->Number });
				}
			}
			R.Mine = MaybeNumber(*V, "mine");
			for (const FValue& S : ListOf(*V, "steps"))
			{
				if (S.IsNumber())
				{
					R.Steps.push_back(S.Number);
				}
			}
			// (null: he may bid; a reason: why not)
			R.CanBid = TextOf(*V, "canBid");
			R.bAccept = FlagOf(*V, "accept");
			return R;
		}

		FRow RowOf(const FValue& V)
		{
			FRow R;
			R.Id = TextOf(V, "id");
			R.Seller = TextOf(V, "seller");
			R.SellerName = TextOf(V, "sellerName");
			R.Name = TextOf(V, "name");
			R.Sign = TextOf(V, "sign");
			R.CapitalName = TextOf(V, "capitalName");
			R.Buyer = TextOf(V, "buyer");
			R.Capital = WholeOf(V, "capital", -1);
			R.MonthsLeft = WholeOf(V, "monthsLeft", 0);
			R.Base = WholeOf(V, "base", -1);
			R.Price = MaybeNumber(V, "price");
			R.Way = WayOf(TextOf(V, "way"));
			R.State = StateOf(TextOf(V, "state"));
			R.Rel = RelOf(TextOf(V, "rel"));
			R.bGeneric = FlagOf(V, "generic");
			R.bAgentSeller = FlagOf(V, "agentSeller");
			R.bAgentBuyer = FlagOf(V, "agentBuyer");
			R.Round = RoundOf(V);
			if (const FValue* C = V.Get("cargo"); C && C->IsObject())
			{
				R.Cargo.bMoved = FlagOf(*C, "moved");
				R.Cargo.Sup = NumberOf(*C, "sup");
				R.Cargo.Oil = NumberOf(*C, "oil");
				R.Cargo.Pp = MaybeNumber(*C, "pp");
			}
			R.Lean = NumberOf(V, "lean");
			const std::vector<FValue>& Flat = ListOf(V, "path");
			R.Path.reserve(Flat.size() / 2);
			for (size_t K = 0; K + 1 < Flat.size(); K += 2)
			{
				if (Flat[K].IsNumber() && Flat[K + 1].IsNumber())
				{
					R.Path.push_back(FPt{ Flat[K].Number, Flat[K + 1].Number });
				}
			}
			R.Loss = SeaMarkOf(V, "loss");
			R.Cut = SeaMarkOf(V, "cut");
			return R;
		}

		FLegion LegionOf(const FValue& V)
		{
			FLegion L;
			L.Home = TextOf(V, "home");
			L.HomeName = TextOf(V, "homeName");
			L.Patron = TextOf(V, "patron");
			L.Against = TextOf(V, "against");
			L.Name = TextOf(V, "name");
			L.Place = TextOf(V, "place");
			L.Corps = WholeOf(V, "corps", -1);
			L.Region = WholeOf(V, "region", -1);
			L.HomeCapital = WholeOf(V, "homeCapital", -1);
			L.Pct = WholeOf(V, "pct", 0);
			L.TourAt = WholeOf(V, "tourAt", 0);
			L.Tour = WholeOf(V, "tour", 0);
			L.MonthsLeft = WholeOf(V, "monthsLeft", 0);
			L.bRecall = FlagOf(V, "recall");
			return L;
		}
	}

	const FNation* FBook::Nation(std::string_view Tag) const
	{
		for (const FNation& N : Nations)
		{
			if (N.Tag == Tag)
			{
				return &N;
			}
		}
		return nullptr;
	}

	const FLegion* FBook::LegionOfCorps(int Corps) const
	{
		if (Corps < 0)
		{
			return nullptr;
		}
		for (const FLegion& L : Legions)
		{
			if (L.Corps == Corps)
			{
				return &L;
			}
		}
		return nullptr;
	}

	const FRow* FBook::Row(std::string_view Id) const
	{
		for (const FRow& R : Rows)
		{
			if (R.Id == Id)
			{
				return &R;
			}
		}
		return nullptr;
	}

	bool ReadBook(std::string_view Json, FBook& Out, std::string* Error)
	{
		Out = FBook();
		FValue Root;
		FReader Reader(Json);
		if (!Reader.Document(Root))
		{
			if (Error)
			{
				*Error = Reader.Why;
			}
			return false;
		}
		if (!Root.IsObject())
		{
			if (Error)
			{
				*Error = "the answer is not an object";
			}
			return false;
		}
		if (Error)
		{
			Error->clear();
		}
		Out.bOn = FlagOf(Root, "on");
		if (!Out.bOn)
		{
			return true;
		}
		Out.Viewer = TextOf(Root, "viewer");
		Out.Month = TextOf(Root, "month");
		Out.Turn = WholeOf(Root, "turn", 0);
		Out.Term = WholeOf(Root, "term", 12);
		Out.RoundLen = WholeOf(Root, "round", 1);
		if (const FValue* Scale = Root.Get("scale"); Scale && Scale->IsNumber() && Scale->Number > 0.0 && Scale->Number < 1e6)
		{
			Out.Scale = Scale->Number;
		}
		if (const FValue* Nations = Root.Get("nations"); Nations && Nations->IsObject())
		{
			Out.Nations.reserve(Nations->Keys.size());
			for (size_t K = 0; K < Nations->Keys.size(); ++K)
			{
				const FValue& N = Nations->Items[K];
				if (!N.IsObject())
				{
					continue;
				}
				FNation Nation;
				Nation.Tag = Nations->Keys[K];
				Nation.Name = TextOf(N, "name");
				Nation.Adj = TextOf(N, "adj");
				const std::string Bloc = TextOf(N, "bloc");
				Nation.Bloc = Bloc == "axis" ? 1 : Bloc == "allies" ? -1 : 0;
				Nation.The = TextOf(N, "the");
				if (Nation.The.empty())
				{
					Nation.The = Nation.Name;
				}
				Out.Nations.push_back(std::move(Nation));
			}
		}
		const std::vector<FValue>& Rows = ListOf(Root, "rows");
		Out.Rows.reserve(Rows.size());
		for (const FValue& V : Rows)
		{
			if (V.IsObject())
			{
				Out.Rows.push_back(RowOf(V));
			}
		}
		const std::vector<FValue>& Legions = ListOf(Root, "legions");
		Out.Legions.reserve(Legions.size());
		for (const FValue& V : Legions)
		{
			if (V.IsObject())
			{
				Out.Legions.push_back(LegionOf(V));
			}
		}
		return true;
	}
}
