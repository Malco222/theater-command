#include "Battle/Fates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Fates
{
	// ---------------------------------------------------------------------------------------------------------------
	// JSON, small and plain.

	namespace
	{
		const Json& NullJson()
		{
			static const Json None;
			return None;
		}

		struct Reader
		{
			explicit Reader(const std::string& Source) : S(Source) {}
			const std::string& S;
			size_t I = 0;
			std::string Why;

			void Space()
			{
				while (I < S.size() && (S[I] == ' ' || S[I] == '\n' || S[I] == '\r' || S[I] == '\t'))
				{
					++I;
				}
			}

			bool Fail(const char* Said)
			{
				if (Why.empty())
				{
					Why = std::string(Said) + " at character " + std::to_string(I);
				}
				return false;
			}

			bool Word(const char* W)
			{
				const size_t N = std::strlen(W);
				if (S.compare(I, N, W) != 0)
				{
					return false;
				}
				I += N;
				return true;
			}

			bool Text(std::string& Out)
			{
				if (I >= S.size() || S[I] != '"')
				{
					return Fail("a string was expected");
				}
				++I;
				Out.clear();
				while (I < S.size() && S[I] != '"')
				{
					char C = S[I++];
					if (C == '\\' && I < S.size())
					{
						const char E = S[I++];
						switch (E)
						{
						case 'n': C = '\n'; break;
						case 't': C = '\t'; break;
						case 'r': C = '\r'; break;
						case 'b': C = '\b'; break;
						case 'f': C = '\f'; break;
						case 'u':
						{
							// (the table writes plain words; a \u escape is kept as the code point's UTF-8)
							if (I + 4 > S.size())
							{
								return Fail("a short \\u escape");
							}
							const unsigned Code = static_cast<unsigned>(std::strtoul(S.substr(I, 4).c_str(), nullptr, 16));
							I += 4;
							if (Code < 0x80)
							{
								Out += static_cast<char>(Code);
							}
							else if (Code < 0x800)
							{
								Out += static_cast<char>(0xC0 | (Code >> 6));
								Out += static_cast<char>(0x80 | (Code & 0x3F));
							}
							else
							{
								Out += static_cast<char>(0xE0 | (Code >> 12));
								Out += static_cast<char>(0x80 | ((Code >> 6) & 0x3F));
								Out += static_cast<char>(0x80 | (Code & 0x3F));
							}
							continue;
						}
						default: C = E; break;
						}
					}
					Out += C;
				}
				if (I >= S.size())
				{
					return Fail("a string was not closed");
				}
				++I;
				return true;
			}

			bool Value(Json& Out, int Depth)
			{
				if (Depth > 64)
				{
					return Fail("nested too deep");
				}
				Space();
				if (I >= S.size())
				{
					return Fail("the text ended early");
				}
				const char C = S[I];
				if (C == '{')
				{
					++I;
					Out.Kind = Json::Type::Object;
					Space();
					if (I < S.size() && S[I] == '}')
					{
						++I;
						return true;
					}
					for (;;)
					{
						Space();
						std::string Key;
						if (!Text(Key))
						{
							return false;
						}
						Space();
						if (I >= S.size() || S[I] != ':')
						{
							return Fail("a ':' was expected");
						}
						++I;
						Json Item;
						if (!Value(Item, Depth + 1))
						{
							return false;
						}
						Out.Keys.push_back(std::move(Key));
						Out.Values.push_back(std::move(Item));
						Space();
						if (I < S.size() && S[I] == ',')
						{
							++I;
							continue;
						}
						if (I < S.size() && S[I] == '}')
						{
							++I;
							return true;
						}
						return Fail("a ',' or '}' was expected");
					}
				}
				if (C == '[')
				{
					++I;
					Out.Kind = Json::Type::Array;
					Space();
					if (I < S.size() && S[I] == ']')
					{
						++I;
						return true;
					}
					for (;;)
					{
						Json Item;
						if (!Value(Item, Depth + 1))
						{
							return false;
						}
						Out.Items.push_back(std::move(Item));
						Space();
						if (I < S.size() && S[I] == ',')
						{
							++I;
							continue;
						}
						if (I < S.size() && S[I] == ']')
						{
							++I;
							return true;
						}
						return Fail("a ',' or ']' was expected");
					}
				}
				if (C == '"')
				{
					Out.Kind = Json::Type::String;
					return Text(Out.Text);
				}
				if (Word("true"))
				{
					Out.Kind = Json::Type::Bool;
					Out.Flag = true;
					return true;
				}
				if (Word("false"))
				{
					Out.Kind = Json::Type::Bool;
					Out.Flag = false;
					return true;
				}
				if (Word("null"))
				{
					Out.Kind = Json::Type::Null;
					return true;
				}
				const char* Begin = S.c_str() + I;
				char* End = nullptr;
				const double Number = std::strtod(Begin, &End);
				if (End == Begin)
				{
					return Fail("a value was expected");
				}
				I += static_cast<size_t>(End - Begin);
				Out.Kind = Json::Type::Number;
				Out.Value = std::isfinite(Number) ? Number : 0.0;
				return true;
			}
		};

		double Clamp01(double X)
		{
			return !(X > 0.0) ? 0.0 : X > 1.0 ? 1.0 : X;
		}

		/** A number, a [low, high] range drawn evenly, or a dial's value ("$side"). */
		double NumberOf(const Json& Spec, const Fate& F, Dice& D, double Default)
		{
			if (Spec.IsNumber())
			{
				return Spec.Value;
			}
			if (Spec.IsArray() && Spec.Count() == 2 && Spec.At(0).IsNumber() && Spec.At(1).IsNumber())
			{
				return D.U(Spec.At(0).Value, Spec.At(1).Value);
			}
			if (Spec.IsString() && !Spec.Text.empty() && Spec.Text[0] == '$')
			{
				const std::string Name = Spec.Text.substr(1);
				const bool bNegate = !Name.empty() && Name[0] == '-';
				const double Got = F.Dial(bNegate ? Name.substr(1).c_str() : Name.c_str(), Default);
				return bNegate ? -Got : Got;
			}
			return Default;
		}

		/** Words: a word, one of a list drawn evenly, or a dial's value ("$place"). */
		std::string TextOf(const Json& Spec, const Fate& F, Dice& D)
		{
			if (Spec.IsString())
			{
				if (!Spec.Text.empty() && Spec.Text[0] == '$')
				{
					return F.DialText(Spec.Text.substr(1).c_str());
				}
				return Spec.Text;
			}
			if (Spec.IsArray() && Spec.Count() > 0)
			{
				return TextOf(Spec.At(static_cast<size_t>(D.Whole(0, static_cast<int>(Spec.Count()) - 1))), F, D);
			}
			return std::string();
		}

		std::string Format(double X)
		{
			// (a dial's value as text: whole numbers plainly, the rest to three places)
			if (std::fabs(X - std::round(X)) < 1e-9 && std::fabs(X) < 1e9)
			{
				return std::to_string(static_cast<long long>(std::llround(X)));
			}
			char Buffer[48];
			std::snprintf(Buffer, sizeof(Buffer), "%.3f", X);
			return Buffer;
		}

		void ReadWords(const Json& Spec, std::vector<std::string>& Out)
		{
			if (Spec.IsString())
			{
				Out.push_back(Spec.Text);
			}
			for (const Json& Item : Spec.Items)
			{
				if (Item.IsString())
				{
					Out.push_back(Item.Text);
				}
			}
		}

		bool Has(const std::vector<std::string>& List, const std::string& Word)
		{
			return std::find(List.begin(), List.end(), Word) != List.end();
		}
	}

	const Json& Json::operator[](const char* Key) const
	{
		if (Kind == Type::Object)
		{
			for (size_t K = 0; K < Keys.size(); ++K)
			{
				if (Keys[K] == Key)
				{
					return Values[K];
				}
			}
		}
		return NullJson();
	}

	const Json& Json::At(size_t Index) const
	{
		return Kind == Type::Array && Index < Items.size() ? Items[Index] : NullJson();
	}

	bool ReadJson(const std::string& Source, Json& OutValue, std::string& OutError)
	{
		Reader R(Source);
		OutValue = Json();
		if (!R.Value(OutValue, 0))
		{
			OutError = R.Why;
			return false;
		}
		R.Space();
		if (R.I != Source.size())
		{
			OutError = "more after the value, at character " + std::to_string(R.I);
			return false;
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The dice.

	Dice::Dice(uint64_t Seed)
		: State(Seed ? Seed : 0x9E3779B97F4A7C15ull)
	{
	}

	double Dice::U()
	{
		// (splitmix64: each call a fresh, well-mixed number from a counter)
		State += 0x9E3779B97F4A7C15ull;
		uint64_t Z = State;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
	}

	int Dice::Whole(int A, int B)
	{
		if (B <= A)
		{
			return A;
		}
		const int Got = A + static_cast<int>(U() * (B - A + 1));
		return Got > B ? B : Got;
	}

	uint64_t NameKey(const std::string& Name)
	{
		// (FNV-1a, 64 bits)
		uint64_t H = 0xCBF29CE484222325ull;
		for (const char C : Name)
		{
			H ^= static_cast<uint8_t>(C);
			H *= 0x100000001B3ull;
		}
		return H;
	}

	double Ease(double X)
	{
		const double T = Clamp01(X);
		return T * T * (3.0 - 2.0 * T);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A look's dials.

	double Fate::Dial(const char* Name, double Default) const
	{
		const auto Found = Dials.find(Name);
		if (Found == Dials.end() || Found->second.empty())
		{
			return Default;
		}
		char* End = nullptr;
		const double Got = std::strtod(Found->second.c_str(), &End);
		return End && *End == '\0' && std::isfinite(Got) ? Got : Default;
	}

	std::string Fate::DialText(const char* Name, const char* Default) const
	{
		const auto Found = Dials.find(Name);
		return Found == Dials.end() ? std::string(Default) : Found->second;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The table.

	const std::vector<std::string>& Table::Families(const std::string& Domain) const
	{
		static const std::vector<std::string> None;
		const auto Found = Known.find(Domain);
		return Found == Known.end() ? None : Found->second;
	}

	std::string Table::SizeOf(const std::string& Domain, const std::string& Weapon) const
	{
		const auto Found = Sizes.find(Domain + "|" + Weapon);
		return Found == Sizes.end() ? std::string() : Found->second;
	}

	std::string Table::GlanceOf(const std::string& Domain, const std::string& Weapon) const
	{
		const auto Found = Glances.find(Domain + "|" + Weapon);
		return Found == Glances.end() ? std::string() : Found->second;
	}

	bool Table::Load(const std::string& Text, std::string& OutError)
	{
		Json Root;
		if (!ReadJson(Text, Root, OutError))
		{
			return false;
		}
		All.clear();
		AspectOdds.clear();
		Known.clear();
		Sizes.clear();
		Glances.clear();
		for (const auto& Pair : { std::make_pair("sizes", &Sizes), std::make_pair("glances", &Glances) })
		{
			const Json& Of = Root[Pair.first];
			for (size_t K = 0; K < Of.Keys.size(); ++K)
			{
				for (size_t J = 0; J < Of.Values[K].Keys.size(); ++J)
				{
					(*Pair.second)[Of.Keys[K] + "|" + Of.Values[K].Keys[J]] = Of.Values[K].Values[J].Str();
				}
			}
		}
		const Json& Memory = Root["memory"];
		if (Memory["recent"].IsArray() && Memory["recent"].Count() > 0)
		{
			Recent.clear();
			for (const Json& W : Memory["recent"].Items)
			{
				Recent.push_back(std::max(0.0, W.Num(1.0)));
			}
		}
		SameDial = std::max(0.0, Memory["same_dial"].Num(SameDial));
		const Json& Kinds = Root["families"];
		for (size_t K = 0; K < Kinds.Keys.size(); ++K)
		{
			ReadWords(Kinds.Values[K], Known[Kinds.Keys[K]]);
		}
		const Json& Aspects = Root["aspects"];
		for (size_t K = 0; K < Aspects.Keys.size(); ++K)
		{
			const Json& Odds = Aspects.Values[K];
			for (size_t J = 0; J < Odds.Keys.size(); ++J)
			{
				AspectOdds[Aspects.Keys[K]].emplace_back(Odds.Keys[J], std::max(0.0, Odds.Values[J].Num()));
			}
		}
		const Json& Ways = Root["ways"];
		if (!Ways.IsArray() || Ways.Count() == 0)
		{
			OutError = "the table has no ways";
			return false;
		}
		for (const Json& Row : Ways.Items)
		{
			Way W;
			W.Key = Row["key"].Str();
			W.Group = Row["group"].Str();
			W.Words = Row["words"].Str();
			W.Domain = Row["domain"].Str("sea");
			W.How = Row["how"].Str();
			ReadWords(Row["families"], W.Families);
			ReadWords(Row["on"], W.On);
			W.Weight = std::max(0.0, Row["weight"].Num(1.0));
			W.bFinal = Row["final"].Truth(false);
			W.bOnce = Row["once"].Truth(false);
			const Json& By = Row["by"];
			for (size_t K = 0; K < By.Keys.size(); ++K)
			{
				std::map<std::string, double>& Of = W.By[By.Keys[K]];
				for (size_t J = 0; J < By.Values[K].Keys.size(); ++J)
				{
					Of[By.Values[K].Keys[J]] = std::max(0.0, By.Values[K].Values[J].Num(1.0));
				}
			}
			for (size_t K = 0; K < Row["if"].Keys.size(); ++K)
			{
				W.If[Row["if"].Keys[K]] = std::max(0.0, Row["if"].Values[K].Num(1.0));
			}
			for (size_t K = 0; K < Row["unless"].Keys.size(); ++K)
			{
				W.Unless[Row["unless"].Keys[K]] = std::max(0.0, Row["unless"].Values[K].Num(1.0));
			}
			for (size_t K = 0; K < Row["lean"].Keys.size(); ++K)
			{
				const Json& L = Row["lean"].Values[K];
				W.Lean[Row["lean"].Keys[K]] = { std::max(0.0, L.At(0).Num(1.0)), std::max(0.0, L.At(1).Num(1.0)) };
			}
			W.MinHurt = Row["min_hurt"].Num(0.0);
			W.MaxHurt = Row["max_hurt"].Num(1.0);
			W.MinNerve = Row["min_nerve"].Num(0.0);
			W.MaxNerve = Row["max_nerve"].Num(1.0);
			W.FromYear = static_cast<int>(Row["from_year"].Num(0.0));
			W.ToYear = static_cast<int>(Row["to_year"].Num(9999.0));
			W.Dials = Row["dials"];
			W.Beats = Row["beats"];
			if (W.Key.empty() || W.Group.empty() || W.Families.empty() || W.On.empty())
			{
				OutError = "a way needs a key, a group, families and what it shows on (" + (W.Key.empty() ? std::string("no key") : W.Key) + ")";
				return false;
			}
			const std::vector<std::string>& Of = Families(W.Domain);
			for (const std::string& F : W.Families)
			{
				if (!Of.empty() && F != "*" && !Has(Of, F))
				{
					OutError = W.Key + ": no family " + F + " in " + W.Domain;
					return false;
				}
			}
			for (const Way& Before : All)
			{
				if (Before.Key == W.Key)
				{
					OutError = "the key " + W.Key + " twice";
					return false;
				}
			}
			All.push_back(std::move(W));
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The picker.

	Director::Director(const Table& InTable, uint64_t BattleSeed)
		: Tables(InTable), Seed(BattleSeed ? BattleSeed : 1)
	{
	}

	double Director::WeightOf(const Way& W, const Blow& B, const std::string& Aspect) const
	{
		if (W.Domain != B.Domain || !Has(W.On, B.Result) || (!Has(W.Families, B.Family) && !Has(W.Families, "*")))
		{
			return 0.0;
		}
		if (!B.How.empty() && W.How != B.How)
		{
			return 0.0;
		}
		if (B.Hurt < W.MinHurt || B.Hurt > W.MaxHurt || B.Nerve < W.MinNerve || B.Nerve > W.MaxNerve || B.Year < W.FromYear || B.Year > W.ToYear)
		{
			return 0.0;
		}
		if (W.bOnce && Once.count(std::to_string(B.Unit) + "|" + std::to_string(B.Model) + "|" + W.Key))
		{
			return 0.0;
		}
		double Weight = W.Weight;
		for (const auto& Pair : W.By)
		{
			const std::string Value = Pair.first == "weapon" ? B.Weapon : Pair.first == "family" ? B.Family : Pair.first == "aspect" ? Aspect
				: Pair.first == "ground" ? B.Ground : Pair.first == "fuel" ? B.Fuel : Pair.first == "stowage" ? B.Stowage : std::string();
			const auto Found = Pair.second.find(Value);
			if (Found != Pair.second.end())
			{
				Weight *= Found->second;
			}
			else
			{
				const auto Any = Pair.second.find("*");
				Weight *= Any != Pair.second.end() ? Any->second : 0.0;
			}
		}
		const std::pair<const char*, bool> Flags[] = { { "harbour", B.bHarbour }, { "moving", B.bMoving }, { "cover", B.bCover }, { "night", B.bNight } };
		for (const auto& Flag : Flags)
		{
			const auto& Of = Flag.second ? W.If : W.Unless;
			const auto Found = Of.find(Flag.first);
			if (Found != Of.end())
			{
				Weight *= Found->second;
			}
		}
		for (const auto& Pair : W.Lean)
		{
			const double X = Pair.first == "nerve" ? Clamp01(B.Nerve) : Pair.first == "hurt" ? Clamp01(B.Hurt) : 0.0;
			Weight *= Pair.second.first + (Pair.second.second - Pair.second.first) * X;
		}
		return std::isfinite(Weight) ? std::max(0.0, Weight) : 0.0;
	}

	std::string Director::DrawAspect(const Blow& B, Dice& D) const
	{
		if (!B.Aspect.empty())
		{
			return B.Aspect;
		}
		const auto Found = Tables.Aspects().find(B.Domain);
		if (Found == Tables.Aspects().end() || Found->second.empty())
		{
			return std::string();
		}
		double Sum = 0.0;
		for (const auto& Pair : Found->second)
		{
			Sum += Pair.second;
		}
		double Roll = D.U() * Sum;
		for (const auto& Pair : Found->second)
		{
			Roll -= Pair.second;
			if (Roll < 0.0)
			{
				return Pair.first;
			}
		}
		return Found->second.back().first;
	}

	std::vector<std::pair<std::string, double>> Director::Odds(const Blow& B) const
	{
		std::vector<std::pair<std::string, double>> Out;
		const std::vector<double>& Memory = Tables.Memory();
		std::vector<double> Raw;
		std::vector<const Way*> Fits;
		for (const Way& W : Tables.Ways())
		{
			// (an undrawn aspect is weighed as the table's odds would draw it, so the odds are the picker's own)
			double Weight = 0.0;
			const auto Found = Tables.Aspects().find(B.Domain);
			if (B.Aspect.empty() && Found != Tables.Aspects().end() && !Found->second.empty())
			{
				double Sum = 0.0;
				for (const auto& Pair : Found->second)
				{
					Weight += Pair.second * WeightOf(W, B, Pair.first);
					Sum += Pair.second;
				}
				Weight = Sum > 0.0 ? Weight / Sum : 0.0;
			}
			else
			{
				Weight = WeightOf(W, B, B.Aspect);
			}
			if (Weight > 0.0)
			{
				Fits.push_back(&W);
				Raw.push_back(Weight);
			}
		}
		double Kept = 0.0;
		std::vector<double> Now = Raw;
		for (size_t K = 0; K < Fits.size(); ++K)
		{
			const auto Seen = Shown.find(Fits[K]->Group);
			if (Seen != Shown.end())
			{
				for (size_t Back = 0; Back < Seen->second.size() && Back < Memory.size(); ++Back)
				{
					if (Seen->second[Back] == Fits[K]->Key)
					{
						Now[K] *= Memory[Back];
						break;
					}
				}
			}
			Kept += Now[K];
		}
		for (size_t K = 0; K < Fits.size(); ++K)
		{
			Out.emplace_back(Fits[K]->Key, Kept > 0.0 ? Now[K] : Raw[K]);
		}
		return Out;
	}

	std::string Director::DrawDial(const Way& W, const std::string& Name, const Json& Spec, const Blow& B, const Fate& F, Dice& D)
	{
		// { "from": "end", "map": { "fore": [1, 3], "aft": [-3, -1] } }: the spec the earlier dial's value names
		if (Spec.IsObject() && Spec["from"].IsString())
		{
			const Json& Chosen = Spec["map"][F.DialText(Spec["from"].Text.c_str())];
			const Json& Spare = Spec["map"]["*"];
			return DrawDial(W, Name, Chosen.IsNull() ? Spare : Chosen, B, F, D);
		}
		const std::string StickyKey = std::to_string(B.Unit) + "|" + Name;
		const bool bSticky = Spec.IsObject() && Spec["sticky"].Truth(false);
		if (bSticky)
		{
			const auto Kept = Sticky.find(StickyKey);
			if (Kept != Sticky.end())
			{
				return Kept->second;
			}
		}
		const std::string LastKey = W.Key + "|" + Name;
		const auto Last = LastDial.find(LastKey);
		const std::string Before = Last == LastDial.end() ? std::string() : Last->second;
		std::string Got;
		// The forms: a number; [low, high] drawn evenly; a list of words or numbers, one drawn; or an object:
		// { "range": [low, high], "lean": "nerve-" } leaning toward the low end the steadier the crew (nerve+ toward the
		// high end, hurt+ and hurt- by how hurt), { "pick": [...], "w": [...] } drawn by weights, "sticky": true kept for
		// the unit once drawn. A list or pick shies from the value it took last time for this way (the table's same_dial).
		const Json& Range = Spec.IsObject() ? Spec["range"] : Spec;
		const Json& Pick = Spec.IsObject() ? Spec["pick"] : Spec;
		if (Spec.IsNumber())
		{
			Got = Format(Spec.Value);
		}
		else if (Range.IsArray() && Range.Count() == 2 && Range.At(0).IsNumber() && Range.At(1).IsNumber())
		{
			double U = D.U();
			const std::string Lean = Spec.IsObject() ? Spec["lean"].Str() : std::string();
			if (!Lean.empty())
			{
				const bool bNerve = Lean.rfind("nerve", 0) == 0;
				double Toward = bNerve ? Clamp01(B.Nerve) : Clamp01(B.Hurt);
				if (Lean.back() == '-')
				{
					Toward = 1.0 - Toward;
				}
				// half the draw is the crew's or the hull's, half is luck
				U = 0.5 * U + 0.5 * Toward;
			}
			// (a range's last value: the new one is kept at least a fifth of the range away from it, where it can be)
			if (!Before.empty())
			{
				const double Lo = Range.At(0).Value, Hi = Range.At(1).Value;
				const double Old = (std::strtod(Before.c_str(), nullptr) - Lo) / ((Hi - Lo) != 0.0 ? (Hi - Lo) : 1.0);
				if (std::fabs(U - Old) < 0.2)
				{
					U = Old + (U >= Old ? 0.2 : -0.2);
					if (U > 1.0 || U < 0.0)
					{
						U = Old + (U > 1.0 ? -0.2 : 0.2);
					}
					U = Clamp01(U);
				}
			}
			Got = Format(Range.At(0).Value + (Range.At(1).Value - Range.At(0).Value) * U);
		}
		else if (Pick.IsArray() && Pick.Count() > 0)
		{
			const Json& Weights = Spec.IsObject() ? Spec["w"] : NullJson();
			std::vector<double> Ws;
			double Sum = 0.0;
			for (size_t K = 0; K < Pick.Count(); ++K)
			{
				const Json& Item = Pick.At(K);
				const std::string Text = Item.IsString() ? Item.Text : Format(Item.Num());
				double Wt = Weights.IsArray() ? std::max(0.0, Weights.At(K).Num(1.0)) : 1.0;
				if (!Before.empty() && Text == Before && Pick.Count() > 1)
				{
					Wt *= Tables.DialRepeat();
				}
				Ws.push_back(Wt);
				Sum += Wt;
			}
			double Roll = D.U() * Sum;
			size_t Chosen = Pick.Count() - 1;
			for (size_t K = 0; K < Ws.size(); ++K)
			{
				Roll -= Ws[K];
				if (Roll < 0.0)
				{
					Chosen = K;
					break;
				}
			}
			const Json& Item = Pick.At(Chosen);
			Got = Item.IsString() ? Item.Text : Format(Item.Num());
		}
		else if (Spec.IsString())
		{
			Got = Spec.Text;
		}
		LastDial[LastKey] = Got;
		if (bSticky)
		{
			Sticky[StickyKey] = Got;
		}
		return Got;
	}

	void Director::Expand(const Way& W, const Json& Spec, const Fate& F, Dice& D, std::vector<Beat>& Out) const
	{
		(void)W;
		// "p": the step happens only so often (a fire that may or may not start); "times" and "gap": the step again and
		// again, each the gap after the last (a cook-off's pops, fires spreading along her).
		if (Spec["p"].IsNumber() || Spec["p"].IsArray() || Spec["p"].IsString())
		{
			if (D.U() >= NumberOf(Spec["p"], F, D, 1.0))
			{
				return;
			}
		}
		const int Times = std::max(1, std::min(24, static_cast<int>(std::lround(NumberOf(Spec["times"], F, D, 1.0)))));
		double T = std::max(0.0, NumberOf(Spec["t"], F, D, 0.0));
		for (int K = 0; K < Times; ++K)
		{
			Beat S;
			S.T = T;
			S.Do = TextOf(Spec["do"], F, D);
			S.At = TextOf(Spec["at"], F, D);
			S.Kind = TextOf(Spec["kind"], F, D);
			S.A = NumberOf(Spec["a"], F, D, 0.0);
			S.B = NumberOf(Spec["b"], F, D, 0.0);
			S.C = NumberOf(Spec["c"], F, D, 0.0);
			S.N = static_cast<int>(std::lround(NumberOf(Spec["n"], F, D, 0.0)));
			if (!S.Do.empty())
			{
				Out.push_back(S);
			}
			T += std::max(0.0, NumberOf(Spec["gap"], F, D, 1.0));
		}
	}

	Dice Director::DiceFor(const Blow& B)
	{
		return Dice(Seed ^ (NameKey(B.Domain + "|" + B.Family) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(B.Unit) * 0xD1B54A32D192ED03ull)
			^ (static_cast<uint64_t>(B.Model + 1) * 0xABC98388FB8FAC03ull) ^ (static_cast<uint64_t>(++Count) * 0x8CB92BA72F3D8DD7ull));
	}

	Fate Director::Show(const Blow& B, const std::string& Key)
	{
		Fate F;
		Dice D = DiceFor(B);
		F.Aspect = DrawAspect(B, D);
		for (const Way& W : Tables.Ways())
		{
			if (W.Key == Key && WeightOf(W, B, F.Aspect) > 0.0)
			{
				Build(W, B, F, D);
				break;
			}
		}
		return F;
	}

	Fate Director::Choose(const Blow& B)
	{
		Fate F;
		Dice D = DiceFor(B);
		F.Aspect = DrawAspect(B, D);
		const std::vector<double>& Memory = Tables.Memory();
		std::vector<const Way*> Fits;
		std::vector<double> Raw, Now;
		double Kept = 0.0;
		for (const Way& W : Tables.Ways())
		{
			const double Weight = WeightOf(W, B, F.Aspect);
			if (Weight <= 0.0)
			{
				continue;
			}
			double Recent = Weight;
			const auto Seen = Shown.find(W.Group);
			if (Seen != Shown.end())
			{
				for (size_t Back = 0; Back < Seen->second.size() && Back < Memory.size(); ++Back)
				{
					if (Seen->second[Back] == W.Key)
					{
						Recent *= Memory[Back];
						break;
					}
				}
			}
			Fits.push_back(&W);
			Raw.push_back(Weight);
			Now.push_back(Recent);
			Kept += Recent;
		}
		if (Fits.empty())
		{
			return F;
		}
		// Only one allowed, and it was the last shown: it is shown again (the rule's one exception).
		const std::vector<double>& Use = Kept > 0.0 ? Now : Raw;
		double Sum = 0.0;
		for (const double W : Use)
		{
			Sum += W;
		}
		double Roll = D.U() * Sum;
		size_t Chosen = Fits.size() - 1;
		for (size_t K = 0; K < Fits.size(); ++K)
		{
			Roll -= Use[K];
			if (Roll < 0.0 && Use[K] > 0.0)
			{
				Chosen = K;
				break;
			}
		}
		Build(*Fits[Chosen], B, F, D);
		return F;
	}

	void Director::Build(const Way& W, const Blow& B, Fate& F, Dice& D)
	{
		const std::vector<double>& Memory = Tables.Memory();
		F.Key = W.Key;
		F.Group = W.Group;
		F.Words = W.Words;
		F.bFinal = W.bFinal;
		F.Dials["aspect"] = F.Aspect;
		F.Dials["weapon"] = B.Weapon;
		F.Dials["family"] = B.Family;
		F.Dials["size"] = Tables.SizeOf(B.Domain, B.Weapon);
		F.Dials["glance"] = Tables.GlanceOf(B.Domain, B.Weapon);
		for (size_t K = 0; K < W.Dials.Keys.size(); ++K)
		{
			F.Dials[W.Dials.Keys[K]] = DrawDial(W, W.Dials.Keys[K], W.Dials.Values[K], B, F, D);
		}
		for (const Json& Spec : W.Beats.Items)
		{
			Expand(W, Spec, F, D, F.Beats);
		}
		std::stable_sort(F.Beats.begin(), F.Beats.end(), [](const Beat& X, const Beat& Y) { return X.T < Y.T; });
		std::vector<std::string>& Seen = Shown[W.Group];
		Seen.insert(Seen.begin(), W.Key);
		if (Seen.size() > std::max<size_t>(Memory.size(), 1))
		{
			Seen.resize(std::max<size_t>(Memory.size(), 1));
		}
		if (W.bOnce)
		{
			Once[std::to_string(B.Unit) + "|" + std::to_string(B.Model) + "|" + W.Key] = 1;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A ship's lasting pose.

	void Hull::Take(const Fate& F, double Now)
	{
		for (const Beat& S : F.Beats)
		{
			const double At = Now + S.T, For = std::max(0.5, S.C);
			if (S.Do == "list")
			{
				const double Side = S.B < 0.0 ? -1.0 : 1.0;
				const double Aim = std::max(-MostList, std::min(MostList, ListAim + Side * std::fabs(S.A)));
				Lists.push_back({ At, For, Aim - ListAim });
				ListAim = Aim;
			}
			else if (S.Do == "right")
			{
				// counter-flooding: she keeps only a share of her list (A), the rest taken back over C seconds
				const double Aim = ListAim * Clamp01(S.A);
				Lists.push_back({ At, For, Aim - ListAim });
				ListAim = Aim;
			}
			else if (S.Do == "trim")
			{
				const double Aim = std::max(-MostTrim, std::min(MostTrim, TrimAim + S.A));
				Trims.push_back({ At, For, Aim - TrimAim });
				TrimAim = Aim;
			}
			else if (S.Do == "settle")
			{
				const double Aim = std::max(0.0, std::min(MostSettle, SettleAim + std::fabs(S.A)));
				Settles.push_back({ At, For, Aim - SettleAim });
				SettleAim = Aim;
			}
		}
	}

	double Hull::Sum(const std::vector<Move>& Moves, double Now)
	{
		double Total = 0.0;
		for (const Move& M : Moves)
		{
			if (Now > M.At)
			{
				Total += M.By * Ease((Now - M.At) / M.For);
			}
		}
		return Total;
	}

	void Hull::Pose(double Now, double& OutList, double& OutTrim, double& OutSettle) const
	{
		OutList = std::max(-MostList, std::min(MostList, Sum(Lists, Now)));
		OutTrim = std::max(-MostTrim, std::min(MostTrim, Sum(Trims, Now)));
		OutSettle = std::max(0.0, std::min(MostSettle, Sum(Settles, Now)));
	}
}
