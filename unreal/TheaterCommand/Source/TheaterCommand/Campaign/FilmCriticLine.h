#pragma once

// film critic: one line of the critic facts file (critic-facts.jsonl, Tools/film_critic/INTERFACE.md section 2), written
// by hand as JSON. Plain standard C++ with no Unreal type, header-only, so that Tools/film_critic/facts_line_test.cpp
// tests it with g++ alone. Only FilmCritic.cpp (and that test) include it; nothing here runs without -FilmCritic.
//
// Numbers are written with a fixed count of decimals, never in the C library's locale and never as "-0"; a number that
// is not finite is written null (a value the game could not give is null or left out, never made up). Strings are
// escaped: the quote, the backslash and the control characters; other UTF-8 passes through as it is.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace FilmCriticLine
{
	/** Appends Text as a JSON string, its quotes and escapes included. Text is UTF-8. */
	inline void AppendQuoted(std::string& Out, const std::string& Text)
	{
		static const char Hex[] = "0123456789abcdef";
		Out.push_back('"');
		for (const char Ch : Text)
		{
			const unsigned char Code = static_cast<unsigned char>(Ch);
			if (Ch == '"')
			{
				Out += "\\\"";
			}
			else if (Ch == '\\')
			{
				Out += "\\\\";
			}
			else if (Ch == '\n')
			{
				Out += "\\n";
			}
			else if (Ch == '\r')
			{
				Out += "\\r";
			}
			else if (Ch == '\t')
			{
				Out += "\\t";
			}
			else if (Code < 0x20)
			{
				Out += "\\u00";
				Out.push_back(Hex[(Code >> 4) & 0x0f]);
				Out.push_back(Hex[Code & 0x0f]);
			}
			else
			{
				Out.push_back(Ch);
			}
		}
		Out.push_back('"');
	}

	/** Appends a whole number, in plain digits. */
	inline void AppendWhole(std::string& Out, long long Value)
	{
		Out += std::to_string(Value);
	}

	/**
	 * Appends Value with Places decimals (0 to 9), rounded half away from zero: "12.500", "-0.250", "3". Not finite, or
	 * too big to write that way (beyond some 9e18 in its last decimal): null. Never "-0" (a value that rounds to zero is
	 * written without a sign).
	 */
	inline void AppendNumber(std::string& Out, double Value, int Places)
	{
		if (!std::isfinite(Value))
		{
			Out += "null";
			return;
		}
		const int Decimals = Places < 0 ? 0 : (Places > 9 ? 9 : Places);
		unsigned long long Scale = 1;
		for (int Step = 0; Step < Decimals; ++Step)
		{
			Scale *= 10ULL;
		}
		const double Scaled = Value * static_cast<double>(Scale);
		if (std::fabs(Scaled) >= 9.0e18)
		{
			Out += "null";
			return;
		}
		const long long Rounded = std::llround(Scaled);
		const unsigned long long Magnitude = Rounded < 0 ? static_cast<unsigned long long>(-Rounded) : static_cast<unsigned long long>(Rounded);
		if (Rounded < 0)
		{
			Out.push_back('-');
		}
		Out += std::to_string(Magnitude / Scale);
		if (Decimals > 0)
		{
			Out.push_back('.');
			const std::string Fraction = std::to_string(Magnitude % Scale);
			Out.append(static_cast<size_t>(Decimals) - Fraction.size(), '0');
			Out += Fraction;
		}
	}

	/**
	 * One line: {"t":..,"k":"..", the fields in the order added}, ended by Done with "}\n". Each field's name is written
	 * as given (names are this file's own words: plain letters and underscores).
	 */
	class Line
	{
	public:
		Line(double RealSeconds, const char* Kind)
		{
			Text.reserve(256);
			Text += "{\"t\":";
			AppendNumber(Text, RealSeconds, 3);
			Text += ",\"k\":";
			AppendQuoted(Text, Kind);
		}

		Line& Num(const char* Name, double Value, int Places = 3)
		{
			Key(Name);
			AppendNumber(Text, Value, Places);
			return *this;
		}
		Line& Whole(const char* Name, long long Value)
		{
			Key(Name);
			AppendWhole(Text, Value);
			return *this;
		}
		Line& Flag(const char* Name, bool bValue)
		{
			Key(Name);
			Text += bValue ? "true" : "false";
			return *this;
		}
		Line& Nothing(const char* Name)
		{
			Key(Name);
			Text += "null";
			return *this;
		}
		Line& Str(const char* Name, const std::string& Value)
		{
			Key(Name);
			AppendQuoted(Text, Value);
			return *this;
		}
		/** A string, or null when it is empty (a word the game does not keep). */
		Line& StrOrNull(const char* Name, const std::string& Value)
		{
			return Value.empty() ? Nothing(Name) : Str(Name, Value);
		}
		Line& Vec2(const char* Name, double X, double Y, int Places = 3)
		{
			Key(Name);
			Text.push_back('[');
			AppendNumber(Text, X, Places);
			Text.push_back(',');
			AppendNumber(Text, Y, Places);
			Text.push_back(']');
			return *this;
		}
		Line& Vec3(const char* Name, double X, double Y, double Z, int Places = 3)
		{
			Key(Name);
			Text.push_back('[');
			AppendNumber(Text, X, Places);
			Text.push_back(',');
			AppendNumber(Text, Y, Places);
			Text.push_back(',');
			AppendNumber(Text, Z, Places);
			Text.push_back(']');
			return *this;
		}
		/** An object of named numbers, {"attack":3,"defend":0}: a value that is not finite is null. */
		Line& Pairs(const char* Name, const std::vector<std::pair<std::string, double>>& Values, int Places = 3)
		{
			Key(Name);
			Text.push_back('{');
			bool bFirst = true;
			for (const std::pair<std::string, double>& Each : Values)
			{
				if (!bFirst)
				{
					Text.push_back(',');
				}
				bFirst = false;
				AppendQuoted(Text, Each.first);
				Text.push_back(':');
				AppendNumber(Text, Each.second, Places);
			}
			Text.push_back('}');
			return *this;
		}
		/** An array of strings. */
		Line& Strs(const char* Name, const std::vector<std::string>& Values)
		{
			Key(Name);
			Text.push_back('[');
			bool bFirst = true;
			for (const std::string& Each : Values)
			{
				if (!bFirst)
				{
					Text.push_back(',');
				}
				bFirst = false;
				AppendQuoted(Text, Each);
			}
			Text.push_back(']');
			return *this;
		}
		/** {"name":"..","at":[x,y,z]}: what a shot looks at, and where (the checker reads its place by "at"). */
		Line& NamedAt(const char* Name, const std::string& What, double X, double Y, double Z, int Places = 3)
		{
			Key(Name);
			Text += "{\"name\":";
			AppendQuoted(Text, What);
			Text += ",\"at\":[";
			AppendNumber(Text, X, Places);
			Text.push_back(',');
			AppendNumber(Text, Y, Places);
			Text.push_back(',');
			AppendNumber(Text, Z, Places);
			Text += "]}";
			return *this;
		}

		/** The line, ended: "{...}\n". */
		std::string Done() const
		{
			return Text + "}\n";
		}

	private:
		void Key(const char* Name)
		{
			Text.push_back(',');
			AppendQuoted(Text, Name);
			Text.push_back(':');
		}

		std::string Text;
	};
}
