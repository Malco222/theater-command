#include "Battle/LandJson.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace LandJson
{
	namespace
	{
		const Json& Nothing()
		{
			static const Json Empty;
			return Empty;
		}

		const std::string& NoText()
		{
			static const std::string Empty;
			return Empty;
		}

		/** Powers of ten for the number reader, so that most numbers need no pow(). */
		double Ten(int E)
		{
			static const double Table[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };
			if (E >= 0 && E <= 22) return Table[E];
			return std::pow(10.0, E);
		}

		struct Reader
		{
			const char* P = nullptr;
			const char* End = nullptr;
			std::string Error;
			int Depth = 0;

			void Space()
			{
				while (P < End && (*P == ' ' || *P == '\t' || *P == '\n' || *P == '\r')) ++P;
			}

			bool Fail(const char* Why)
			{
				if (Error.empty()) Error = Why;
				return false;
			}

			static void PutUtf8(std::string& Out, unsigned Code)
			{
				if (Code < 0x80) Out.push_back(static_cast<char>(Code));
				else if (Code < 0x800)
				{
					Out.push_back(static_cast<char>(0xC0 | (Code >> 6)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
				else if (Code < 0x10000)
				{
					Out.push_back(static_cast<char>(0xE0 | (Code >> 12)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
				else
				{
					Out.push_back(static_cast<char>(0xF0 | (Code >> 18)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 12) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
			}

			bool Hex4(unsigned& OutCode)
			{
				if (End - P < 4) return Fail("a short \\u escape");
				OutCode = 0;
				for (int K = 0; K < 4; ++K)
				{
					const char C = P[K];
					OutCode <<= 4;
					if (C >= '0' && C <= '9') OutCode |= static_cast<unsigned>(C - '0');
					else if (C >= 'a' && C <= 'f') OutCode |= static_cast<unsigned>(C - 'a' + 10);
					else if (C >= 'A' && C <= 'F') OutCode |= static_cast<unsigned>(C - 'A' + 10);
					else return Fail("a bad \\u escape");
				}
				P += 4;
				return true;
			}

			bool ReadString(std::string& Out)
			{
				++P; // the opening quote
				Out.clear();
				while (P < End)
				{
					const char C = *P++;
					if (C == '"') return true;
					if (C != '\\') { Out.push_back(C); continue; }
					if (P >= End) break;
					const char Esc = *P++;
					switch (Esc)
					{
					case '"': Out.push_back('"'); break;
					case '\\': Out.push_back('\\'); break;
					case '/': Out.push_back('/'); break;
					case 'b': Out.push_back('\b'); break;
					case 'f': Out.push_back('\f'); break;
					case 'n': Out.push_back('\n'); break;
					case 'r': Out.push_back('\r'); break;
					case 't': Out.push_back('\t'); break;
					case 'u':
					{
						unsigned Code = 0;
						if (!Hex4(Code)) return false;
						if (Code >= 0xD800 && Code <= 0xDBFF && End - P >= 6 && P[0] == '\\' && P[1] == 'u')
						{
							P += 2;
							unsigned Low = 0;
							if (!Hex4(Low)) return false;
							if (Low >= 0xDC00 && Low <= 0xDFFF) Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
						}
						PutUtf8(Out, Code);
						break;
					}
					default: return Fail("a bad escape");
					}
				}
				return Fail("a string left open");
			}

			/** A number: sign, digits, a point and digits, an exponent. Exact to the last digit or two of a double,
			 * which is all a model's millimetres or a track's tenths of a second need. */
			bool ReadNumber(double& Out)
			{
				bool bNegative = false;
				if (*P == '-') { bNegative = true; ++P; }
				double Whole = 0.0;
				int Digits = 0;
				while (P < End && *P >= '0' && *P <= '9') { Whole = Whole * 10.0 + (*P - '0'); ++P; ++Digits; }
				int Scale = 0;
				if (P < End && *P == '.')
				{
					++P;
					while (P < End && *P >= '0' && *P <= '9') { Whole = Whole * 10.0 + (*P - '0'); ++P; ++Digits; --Scale; }
				}
				if (Digits == 0) return Fail("a bad number");
				if (P < End && (*P == 'e' || *P == 'E'))
				{
					++P;
					bool bDown = false;
					if (P < End && (*P == '+' || *P == '-')) { bDown = *P == '-'; ++P; }
					int E = 0;
					bool bAny = false;
					while (P < End && *P >= '0' && *P <= '9') { E = E * 10 + (*P - '0'); ++P; bAny = true; if (E > 400) E = 400; }
					if (!bAny) return Fail("a bad exponent");
					Scale += bDown ? -E : E;
				}
				Out = Scale >= 0 ? Whole * Ten(Scale) : Whole / Ten(-Scale);
				if (bNegative) Out = -Out;
				return true;
			}

			bool ReadValue(Json& Out)
			{
				Space();
				if (P >= End) return Fail("the text ended early");
				if (++Depth > 200) return Fail("nested too deep");
				bool bOk = true;
				const char C = *P;
				if (C == '{')
				{
					Out.Kind = Json::Type::Object;
					++P;
					Space();
					if (P < End && *P == '}') { ++P; }
					else
					{
						for (;;)
						{
							Space();
							if (P >= End || *P != '"') { bOk = Fail("a field without a name"); break; }
							std::string Key;
							if (!ReadString(Key)) { bOk = false; break; }
							Space();
							if (P >= End || *P != ':') { bOk = Fail("a field without a colon"); break; }
							++P;
							Out.Keys.push_back(std::move(Key));
							Out.Values.emplace_back();
							if (!ReadValue(Out.Values.back())) { bOk = false; break; }
							Space();
							if (P < End && *P == ',') { ++P; continue; }
							if (P < End && *P == '}') { ++P; break; }
							bOk = Fail("an object left open");
							break;
						}
					}
				}
				else if (C == '[')
				{
					Out.Kind = Json::Type::Array;
					++P;
					Space();
					if (P < End && *P == ']') { ++P; }
					else
					{
						for (;;)
						{
							Out.Items.emplace_back();
							if (!ReadValue(Out.Items.back())) { bOk = false; break; }
							Space();
							if (P < End && *P == ',') { ++P; continue; }
							if (P < End && *P == ']') { ++P; break; }
							bOk = Fail("an array left open");
							break;
						}
					}
				}
				else if (C == '"')
				{
					Out.Kind = Json::Type::String;
					bOk = ReadString(Out.Text);
				}
				else if (C == 't' && End - P >= 4 && std::strncmp(P, "true", 4) == 0) { Out.Kind = Json::Type::Bool; Out.Flag = true; P += 4; }
				else if (C == 'f' && End - P >= 5 && std::strncmp(P, "false", 5) == 0) { Out.Kind = Json::Type::Bool; Out.Flag = false; P += 5; }
				else if (C == 'n' && End - P >= 4 && std::strncmp(P, "null", 4) == 0) { Out.Kind = Json::Type::Null; P += 4; }
				else if (C == '-' || (C >= '0' && C <= '9'))
				{
					Out.Kind = Json::Type::Number;
					bOk = ReadNumber(Out.Value);
				}
				else bOk = Fail("an unexpected character");
				--Depth;
				return bOk;
			}
		};
	}

	const Json& Json::operator[](const char* Key) const
	{
		if (Kind != Type::Object) return Nothing();
		for (size_t K = 0; K < Keys.size(); ++K)
			if (Keys[K] == Key) return Values[K];
		return Nothing();
	}

	const Json& Json::At(size_t Index) const
	{
		if (Kind != Type::Array || Index >= Items.size()) return Nothing();
		return Items[Index];
	}

	int Json::Int(int Default) const
	{
		if (Kind == Type::String)
		{
			char* Stop = nullptr;
			const long N = std::strtol(Text.c_str(), &Stop, 10);
			return Stop == Text.c_str() ? Default : static_cast<int>(N);
		}
		if (Kind == Type::Bool) return Flag ? 1 : 0;
		if (Kind != Type::Number) return Default;
		return static_cast<int>(std::lround(Value));
	}

	const std::string& Json::Str() const
	{
		return Kind == Type::String ? Text : NoText();
	}

	bool Parse(const std::string& Source, Json& OutValue, std::string& OutError)
	{
		Reader R;
		R.P = Source.data();
		R.End = Source.data() + Source.size();
		OutValue = Json();
		if (!R.ReadValue(OutValue))
		{
			OutError = R.Error + " at character " + std::to_string(static_cast<long long>(R.P - Source.data()));
			return false;
		}
		R.Space();
		if (R.P != R.End)
		{
			OutError = "text after the value at character " + std::to_string(static_cast<long long>(R.P - Source.data()));
			return false;
		}
		return true;
	}

	bool ReadFile(const std::string& Path, std::string& OutText)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File) return false;
		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		OutText = Buffer.str();
		return true;
	}

	std::vector<double> Numbers(const Json& Array)
	{
		std::vector<double> Out;
		Out.reserve(Array.Count());
		for (const Json& V : Array.Items) Out.push_back(V.Num(0.0));
		return Out;
	}
}
