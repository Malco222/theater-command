#pragma once
// A small JSON reader for the land show's tools (the recording's field, rounds and men), so the tests need nothing
// but Battle/LandMen: the game's AirFeed reader brings the whole air show with it. Not for the game.
#include <cstdlib>
#include <string>
#include <vector>

namespace LandJson
{
	struct Value
	{
		enum Kind { Null, Bool, Number, String, Array, Object } K = Null;
		double N = 0.0;
		std::string S;
		std::vector<Value> Items;
		std::vector<std::string> Keys;

		const Value& operator[](const char* Key) const
		{
			static const Value None;
			if (K != Object) return None;
			for (size_t I = 0; I < Keys.size(); ++I) if (Keys[I] == Key) return Items[I];
			return None;
		}
		const Value& At(size_t I) const
		{
			static const Value None;
			return K == Array && I < Items.size() ? Items[I] : None;
		}
		size_t Count() const { return K == Array || K == Object ? Items.size() : 0; }
		bool Has(const char* Key) const { return (*this)[Key].K != Null; }
		double Num(double Default = 0.0) const { return K == Number ? N : K == Bool ? N : Default; }
		int Int(int Default = 0) const { return K == Number ? static_cast<int>(N) : Default; }
		std::string Str(const char* Default) const { return K == String ? S : std::string(Default); }
	};

	struct Reader
	{
		const char* P;
		const char* E;
		bool bBad = false;

		void Space() { while (P < E && (*P == ' ' || *P == '\n' || *P == '\r' || *P == '\t')) ++P; }
		bool Read(Value& V)
		{
			Space();
			if (P >= E) return false;
			const char C = *P;
			if (C == '{')
			{
				V.K = Value::Object;
				++P;
				Space();
				if (P < E && *P == '}') { ++P; return true; }
				while (P < E)
				{
					Space();
					Value Key;
					if (!Read(Key) || Key.K != Value::String) return false;
					Space();
					if (P >= E || *P != ':') return false;
					++P;
					V.Keys.push_back(Key.S);
					V.Items.emplace_back();
					if (!Read(V.Items.back())) return false;
					Space();
					if (P < E && *P == ',') { ++P; continue; }
					if (P < E && *P == '}') { ++P; return true; }
					return false;
				}
				return false;
			}
			if (C == '[')
			{
				V.K = Value::Array;
				++P;
				Space();
				if (P < E && *P == ']') { ++P; return true; }
				while (P < E)
				{
					V.Items.emplace_back();
					if (!Read(V.Items.back())) return false;
					Space();
					if (P < E && *P == ',') { ++P; continue; }
					if (P < E && *P == ']') { ++P; return true; }
					return false;
				}
				return false;
			}
			if (C == '"')
			{
				V.K = Value::String;
				++P;
				while (P < E && *P != '"')
				{
					if (*P == '\\' && P + 1 < E)
					{
						++P;
						const char X = *P;
						V.S += X == 'n' ? '\n' : X == 't' ? '\t' : X == 'u' ? '?' : X;
						if (X == 'u') P += 4;
						++P;
						continue;
					}
					V.S += *P++;
				}
				if (P >= E) return false;
				++P;
				return true;
			}
			if (C == 't' || C == 'f')
			{
				V.K = Value::Bool;
				V.N = C == 't' ? 1.0 : 0.0;
				P += C == 't' ? 4 : 5;
				return true;
			}
			if (C == 'n') { V.K = Value::Null; P += 4; return true; }
			char* End = nullptr;
			V.N = std::strtod(P, &End);
			if (End == P) return false;
			V.K = Value::Number;
			P = End;
			return true;
		}
	};

	inline bool Parse(const std::string& Text, Value& Out)
	{
		Reader R { Text.data(), Text.data() + Text.size() };
		return R.Read(Out);
	}
}
