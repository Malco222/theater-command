#include "Battle/LandKit.h"

#include "Battle/Fates.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <unordered_map>

namespace LandKit
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;

		double Clamp(double X, double Lo, double Hi) { return X < Lo ? Lo : X > Hi ? Hi : X; }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp((X - E0) / (E1 - E0), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		/** An angle the short way round, in (-pi, pi]. */
		double Wrap(double A)
		{
			A = std::fmod(A + Pi, 2.0 * Pi);
			if (A < 0.0)
			{
				A += 2.0 * Pi;
			}
			return A - Pi;
		}
		double WrapDeg(double A) { return Wrap(A * Rad) / Rad; }

		/** A fixed number from a few whole numbers: the same on every machine (looks only, never the battle's luck). */
		uint64_t Mix(uint64_t A, uint64_t B = 0, uint64_t C = 0)
		{
			uint64_t H = 0x9E3779B97F4A7C15ull ^ (A * 0xBF58476D1CE4E5B9ull);
			H ^= B + 0x94D049BB133111EBull + (H << 6) + (H >> 2);
			H ^= C + 0x2545F4914F6CDD1Dull + (H << 6) + (H >> 2);
			H ^= H >> 31;
			H *= 0xD6E8FEB86659FD93ull;
			H ^= H >> 32;
			return H;
		}
		double Unit01(uint64_t H) { return static_cast<double>(H >> 11) * (1.0 / 9007199254740992.0); }

		/** The near triangles boxed together for a round's cast (Body::Runs). */
		constexpr size_t TriangleRun = 64;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Small maths.

	double V3::Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }

	V3 V3::Unit(const V3& Otherwise) const
	{
		const double L = Length();
		return L > 1e-12 ? V3(X / L, Y / L, Z / L) : Otherwise;
	}

	Quat Quat::Axis(const V3& A, double Radians)
	{
		const double S = std::sin(Radians * 0.5);
		return Quat(std::cos(Radians * 0.5), A.X * S, A.Y * S, A.Z * S);
	}

	Quat Quat::operator*(const Quat& O) const
	{
		return Quat(
			W * O.W - X * O.X - Y * O.Y - Z * O.Z,
			W * O.X + X * O.W + Y * O.Z - Z * O.Y,
			W * O.Y - X * O.Z + Y * O.W + Z * O.X,
			W * O.Z + X * O.Y - Y * O.X + Z * O.W);
	}

	V3 Quat::Rotate(const V3& P) const
	{
		// p + 2w (q x p) + 2 q x (q x p), for a unit q: the same as q p q*.
		const V3 Q(X, Y, Z);
		const V3 T = Q.Cross(P) * 2.0;
		return P + T * W + Q.Cross(T);
	}

	Quat Quat::Unit() const
	{
		const double L = std::sqrt(W * W + X * X + Y * Y + Z * Z);
		return L > 1e-12 ? Quat(W / L, X / L, Y / L, Z / L) : Quat();
	}

	Quat Quat::Scaled(double Share) const
	{
		if (Share >= 1.0)
		{
			return *this;
		}
		if (Share <= 0.0)
		{
			return Quat();
		}
		// Along the short way from no turn (the clips' turns are small but for a thrown turret: slerp it).
		Quat Q = W < 0.0 ? Quat(-W, -X, -Y, -Z) : *this;
		const double Half = std::acos(Clamp(Q.W, -1.0, 1.0));
		const double S = std::sin(Half);
		if (S < 1e-9)
		{
			return Quat();
		}
		const V3 A(Q.X / S, Q.Y / S, Q.Z / S);
		return Quat::Axis(A, 2.0 * Half * Share);
	}

	Xf Xf::operator*(const Xf& Child) const
	{
		Xf Out;
		Out.Q = (Q * Child.Q).Unit();
		Out.T = Q.Rotate(Child.T) + T;
		return Out;
	}

	V3 Place::Point(const V3& Local) const { return At + Swap(Q.Rotate(Local)); }
	V3 Place::Way(const V3& Local) const { return Swap(Q.Rotate(Local)); }

	// ---------------------------------------------------------------------------------------------------------------
	// JSON.

	struct Reader
	{
		Json& D;
		const char* P = nullptr;
		const char* E = nullptr;
		std::string Error;
		std::vector<uint32_t> Stack;
		int Depth = 0;

		explicit Reader(Json& InD) : D(InD) {}

		void Space()
		{
			while (P < E && (*P == ' ' || *P == '\n' || *P == '\r' || *P == '\t'))
			{
				++P;
			}
		}
		bool Fail(const char* Why)
		{
			if (Error.empty())
			{
				Error = Why;
			}
			return false;
		}
		static bool Digit(char C) { return C >= '0' && C <= '9'; }

		uint32_t Add(Json::Type Kind)
		{
			Json::Node N;
			N.Kind = Kind;
			D.Nodes.push_back(N);
			return static_cast<uint32_t>(D.Nodes.size() - 1);
		}

		static double Ten(int N)
		{
			static const double Exact[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
				1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };
			return N <= 22 ? Exact[N] : std::pow(10.0, N);
		}

		/** A number as JSON writes one. The digits are gathered whole and scaled once: exact for the files' few
		 * significant figures, and several times quicker than strtod. */
		bool Number(double& Out)
		{
			bool bNegative = false;
			if (P < E && *P == '-')
			{
				bNegative = true;
				++P;
			}
			if (P >= E || !Digit(*P))
			{
				return Fail("a number expected");
			}
			uint64_t Mantissa = 0;
			int Kept = 0, Scale = 0;
			while (P < E && Digit(*P))
			{
				if (Kept < 19)
				{
					Mantissa = Mantissa * 10 + static_cast<uint64_t>(*P - '0');
					if (Mantissa)
					{
						++Kept;
					}
				}
				else
				{
					++Scale;
				}
				++P;
			}
			if (P < E && *P == '.')
			{
				++P;
				if (P >= E || !Digit(*P))
				{
					return Fail("a digit expected after the point");
				}
				while (P < E && Digit(*P))
				{
					if (Kept < 19)
					{
						Mantissa = Mantissa * 10 + static_cast<uint64_t>(*P - '0');
						if (Mantissa)
						{
							++Kept;
						}
						--Scale;
					}
					++P;
				}
			}
			if (P < E && (*P == 'e' || *P == 'E'))
			{
				++P;
				int Sign = 1;
				if (P < E && (*P == '+' || *P == '-'))
				{
					Sign = *P == '-' ? -1 : 1;
					++P;
				}
				if (P >= E || !Digit(*P))
				{
					return Fail("a digit expected in the exponent");
				}
				int Exp = 0;
				while (P < E && Digit(*P))
				{
					Exp = std::min(Exp * 10 + (*P - '0'), 4000);
					++P;
				}
				Scale += Sign * Exp;
			}
			double V = static_cast<double>(Mantissa);
			if (Scale > 0)
			{
				V *= Ten(Scale);
			}
			else if (Scale < 0)
			{
				V /= Ten(-Scale);
			}
			Out = bNegative ? -V : V;
			return true;
		}

		static void PutUtf8(std::string& S, uint32_t C)
		{
			if (C < 0x80)
			{
				S.push_back(static_cast<char>(C));
			}
			else if (C < 0x800)
			{
				S.push_back(static_cast<char>(0xC0 | (C >> 6)));
				S.push_back(static_cast<char>(0x80 | (C & 0x3F)));
			}
			else if (C < 0x10000)
			{
				S.push_back(static_cast<char>(0xE0 | (C >> 12)));
				S.push_back(static_cast<char>(0x80 | ((C >> 6) & 0x3F)));
				S.push_back(static_cast<char>(0x80 | (C & 0x3F)));
			}
			else
			{
				S.push_back(static_cast<char>(0xF0 | (C >> 18)));
				S.push_back(static_cast<char>(0x80 | ((C >> 12) & 0x3F)));
				S.push_back(static_cast<char>(0x80 | ((C >> 6) & 0x3F)));
				S.push_back(static_cast<char>(0x80 | (C & 0x3F)));
			}
		}

		bool Hex4(uint32_t& Out)
		{
			if (E - P < 4)
			{
				return Fail("a short \\u escape");
			}
			Out = 0;
			for (int I = 0; I < 4; ++I)
			{
				const char C = *P++;
				Out <<= 4;
				if (C >= '0' && C <= '9') Out |= static_cast<uint32_t>(C - '0');
				else if (C >= 'a' && C <= 'f') Out |= static_cast<uint32_t>(C - 'a' + 10);
				else if (C >= 'A' && C <= 'F') Out |= static_cast<uint32_t>(C - 'A' + 10);
				else return Fail("a bad \\u escape");
			}
			return true;
		}

		bool Text(uint32_t& OutFirst, uint32_t& OutLength)
		{
			++P; // the opening quote
			std::string& S = D.Strings;
			const size_t First = S.size();
			for (;;)
			{
				const char* Run = P;
				while (P < E && *P != '"' && *P != '\\')
				{
					++P;
				}
				S.append(Run, static_cast<size_t>(P - Run));
				if (P >= E)
				{
					return Fail("a string not closed");
				}
				if (*P == '"')
				{
					++P;
					break;
				}
				++P; // the backslash
				if (P >= E)
				{
					return Fail("a string not closed");
				}
				const char C = *P++;
				switch (C)
				{
				case '"': S.push_back('"'); break;
				case '\\': S.push_back('\\'); break;
				case '/': S.push_back('/'); break;
				case 'b': S.push_back('\b'); break;
				case 'f': S.push_back('\f'); break;
				case 'n': S.push_back('\n'); break;
				case 'r': S.push_back('\r'); break;
				case 't': S.push_back('\t'); break;
				case 'u':
				{
					uint32_t Code = 0;
					if (!Hex4(Code))
					{
						return false;
					}
					if (Code >= 0xD800 && Code < 0xDC00 && E - P >= 6 && P[0] == '\\' && P[1] == 'u')
					{
						P += 2;
						uint32_t Low = 0;
						if (!Hex4(Low))
						{
							return false;
						}
						Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
					}
					PutUtf8(S, Code);
					break;
				}
				default: return Fail("a bad escape");
				}
			}
			OutFirst = static_cast<uint32_t>(First);
			OutLength = static_cast<uint32_t>(S.size() - First);
			return true;
		}

		bool Literal(const char* Word)
		{
			const size_t L = std::strlen(Word);
			if (static_cast<size_t>(E - P) < L || std::memcmp(P, Word, L) != 0)
			{
				return Fail("an unknown word");
			}
			P += L;
			return true;
		}

		bool Value(uint32_t& Out)
		{
			Space();
			if (P >= E)
			{
				return Fail("the text ends early");
			}
			const char C = *P;
			if (C == '{' || C == '[')
			{
				if (++Depth > 200)
				{
					return Fail("nested too deep");
				}
				const bool bOk = C == '{' ? Object(Out) : List(Out);
				--Depth;
				return bOk;
			}
			if (C == '"')
			{
				Out = Add(Json::Type::String);
				uint32_t A = 0, B = 0;
				if (!Text(A, B))
				{
					return false;
				}
				D.Nodes[Out].A = A;
				D.Nodes[Out].B = B;
				return true;
			}
			if (C == 't' || C == 'f')
			{
				Out = Add(Json::Type::Bool);
				D.Nodes[Out].Flag = C == 't';
				return Literal(C == 't' ? "true" : "false");
			}
			if (C == 'n')
			{
				Out = Add(Json::Type::Null);
				return Literal("null");
			}
			double V = 0.0;
			if (!Number(V))
			{
				return false;
			}
			Out = Add(Json::Type::Number);
			D.Nodes[Out].Number = V;
			return true;
		}

		bool List(uint32_t& Out)
		{
			++P;
			Out = Add(Json::Type::Array);
			Space();
			if (P < E && *P == ']')
			{
				++P;
				return true;
			}
			// A list of plain numbers (the meshes' points, normals, paints and triangles; the tracks' kept points) is
			// read straight into the number list; one that turns out to hold anything else is read again the long way.
			if (P < E && (*P == '-' || Digit(*P)))
			{
				const char* Back = P;
				const size_t First = D.NumberList.size();
				bool bAll = false;
				for (;;)
				{
					double V = 0.0;
					if (!Number(V))
					{
						break;
					}
					D.NumberList.push_back(V);
					Space();
					if (P < E && *P == ',')
					{
						++P;
						Space();
						if (P < E && (*P == '-' || Digit(*P)))
						{
							continue;
						}
						break;
					}
					if (P < E && *P == ']')
					{
						++P;
						bAll = true;
					}
					break;
				}
				if (bAll)
				{
					D.Nodes[Out].bNumbers = true;
					D.Nodes[Out].A = static_cast<uint32_t>(First);
					D.Nodes[Out].B = static_cast<uint32_t>(D.NumberList.size() - First);
					return true;
				}
				D.NumberList.resize(First);
				P = Back;
				Error.clear();
			}
			const size_t Base = Stack.size();
			for (;;)
			{
				uint32_t Child = 0;
				if (!Value(Child))
				{
					return false;
				}
				Stack.push_back(Child);
				Space();
				if (P < E && *P == ',')
				{
					++P;
					continue;
				}
				if (P < E && *P == ']')
				{
					++P;
					break;
				}
				return Fail("a list not closed");
			}
			D.Nodes[Out].A = static_cast<uint32_t>(D.Kids.size());
			D.Nodes[Out].B = static_cast<uint32_t>(Stack.size() - Base);
			D.Kids.insert(D.Kids.end(), Stack.begin() + static_cast<std::ptrdiff_t>(Base), Stack.end());
			Stack.resize(Base);
			return true;
		}

		bool Object(uint32_t& Out)
		{
			++P;
			Out = Add(Json::Type::Object);
			Space();
			if (P < E && *P == '}')
			{
				++P;
				return true;
			}
			const size_t Base = Stack.size();
			for (;;)
			{
				Space();
				if (P >= E || *P != '"')
				{
					return Fail("a field name expected");
				}
				const uint32_t Key = Add(Json::Type::String);
				uint32_t A = 0, B = 0;
				if (!Text(A, B))
				{
					return false;
				}
				D.Nodes[Key].A = A;
				D.Nodes[Key].B = B;
				Space();
				if (P >= E || *P != ':')
				{
					return Fail("a colon expected");
				}
				++P;
				uint32_t Child = 0;
				if (!Value(Child))
				{
					return false;
				}
				Stack.push_back(Key);
				Stack.push_back(Child);
				Space();
				if (P < E && *P == ',')
				{
					++P;
					continue;
				}
				if (P < E && *P == '}')
				{
					++P;
					break;
				}
				return Fail("an object not closed");
			}
			D.Nodes[Out].A = static_cast<uint32_t>(D.Kids.size());
			D.Nodes[Out].B = static_cast<uint32_t>((Stack.size() - Base) / 2);
			D.Kids.insert(D.Kids.end(), Stack.begin() + static_cast<std::ptrdiff_t>(Base), Stack.end());
			Stack.resize(Base);
			return true;
		}
	};

	bool Json::Parse(const std::string& Source, std::string& OutError)
	{
		Nodes.clear();
		Kids.clear();
		NumberList.clear();
		Strings.clear();
		Nodes.reserve(Source.size() / 16 + 16);
		NumberList.reserve(Source.size() / 6 + 16);
		Reader R(*this);
		R.P = Source.data();
		R.E = Source.data() + Source.size();
		uint32_t Top = 0;
		if (!R.Value(Top))
		{
			char Where[64];
			std::snprintf(Where, sizeof(Where), " (at byte %lld)", static_cast<long long>(R.P - Source.data()));
			OutError = R.Error + Where;
			Nodes.clear();
			return false;
		}
		return true;
	}

	Json::Value Json::Root() const
	{
		Value V;
		V.Doc = this;
		V.Node = 0;
		if (Nodes.empty())
		{
			V.Doc = nullptr;
		}
		return V;
	}

	Json::Type Json::Value::Kind() const
	{
		if (!Doc)
		{
			return Type::Null;
		}
		return Sub >= 0 ? Type::Number : Doc->Nodes[Node].Kind;
	}

	double Json::Value::Num(double Default) const
	{
		if (!Doc)
		{
			return Default;
		}
		const Json::Node& N = Doc->Nodes[Node];
		if (Sub >= 0)
		{
			return Doc->NumberList[N.A + static_cast<size_t>(Sub)];
		}
		return N.Kind == Type::Number ? N.Number : N.Kind == Type::Bool ? (N.Flag ? 1.0 : 0.0) : Default;
	}

	int Json::Value::Int(int Default) const
	{
		const Type K = Kind();
		return K == Type::Number || K == Type::Bool ? static_cast<int>(std::lround(Num())) : Default;
	}

	bool Json::Value::Truth(bool Default) const
	{
		const Type K = Kind();
		return K == Type::Bool || K == Type::Number ? Num() != 0.0 : Default;
	}

	std::string Json::Value::Str(const char* Default) const
	{
		if (Kind() != Type::String)
		{
			return Default;
		}
		const Json::Node& N = Doc->Nodes[Node];
		return Doc->Strings.substr(N.A, N.B);
	}

	bool Json::Value::Is(const char* Text) const
	{
		if (Kind() != Type::String)
		{
			return false;
		}
		const Json::Node& N = Doc->Nodes[Node];
		return std::strlen(Text) == N.B && std::memcmp(Doc->Strings.data() + N.A, Text, N.B) == 0;
	}

	size_t Json::Value::Count() const
	{
		const Type K = Kind();
		return K == Type::Array || K == Type::Object ? Doc->Nodes[Node].B : 0;
	}

	Json::Value Json::Value::operator[](const char* Key) const
	{
		Value Out;
		if (Kind() != Type::Object)
		{
			return Out;
		}
		const Json::Node& N = Doc->Nodes[Node];
		const size_t L = std::strlen(Key);
		for (uint32_t I = 0; I < N.B; ++I)
		{
			const Json::Node& Name = Doc->Nodes[Doc->Kids[N.A + 2 * I]];
			if (Name.B == L && std::memcmp(Doc->Strings.data() + Name.A, Key, L) == 0)
			{
				Out.Doc = Doc;
				Out.Node = Doc->Kids[N.A + 2 * I + 1];
				return Out;
			}
		}
		return Out;
	}

	Json::Value Json::Value::At(size_t Index) const
	{
		Value Out;
		const Type K = Kind();
		if (K != Type::Array && K != Type::Object)
		{
			return Out;
		}
		const Json::Node& N = Doc->Nodes[Node];
		if (Index >= N.B)
		{
			return Out;
		}
		Out.Doc = Doc;
		if (N.bNumbers)
		{
			Out.Node = Node;
			Out.Sub = static_cast<int64_t>(Index);
		}
		else
		{
			Out.Node = Doc->Kids[N.A + (K == Type::Object ? 2 * Index + 1 : Index)];
		}
		return Out;
	}

	std::string Json::Value::Name(size_t Index) const
	{
		if (Kind() != Type::Object || Index >= Count())
		{
			return std::string();
		}
		const Json::Node& N = Doc->Nodes[Node];
		const Json::Node& Key = Doc->Nodes[Doc->Kids[N.A + 2 * Index]];
		return Doc->Strings.substr(Key.A, Key.B);
	}

	namespace
	{
		template <typename T>
		bool NumbersOf(const Json::Value& V, std::vector<T>& Out)
		{
			Out.clear();
			if (!V.IsArray())
			{
				return false;
			}
			const size_t N = V.Count();
			Out.reserve(N);
			for (size_t I = 0; I < N; ++I)
			{
				const Json::Value Item = V.At(I);
				if (!Item.IsNumber())
				{
					Out.clear();
					return false;
				}
				Out.push_back(static_cast<T>(Item.Num()));
			}
			return true;
		}
	}

	bool Json::Value::Numbers(std::vector<double>& Out) const
	{
		if (Kind() == Type::Array && Doc->Nodes[Node].bNumbers)
		{
			const Json::Node& N = Doc->Nodes[Node];
			Out.assign(Doc->NumberList.begin() + N.A, Doc->NumberList.begin() + N.A + N.B);
			return true;
		}
		return NumbersOf(*this, Out);
	}

	bool Json::Value::Numbers(std::vector<float>& Out) const
	{
		if (Kind() == Type::Array && Doc->Nodes[Node].bNumbers)
		{
			const Json::Node& N = Doc->Nodes[Node];
			Out.resize(N.B);
			for (uint32_t I = 0; I < N.B; ++I)
			{
				Out[I] = static_cast<float>(Doc->NumberList[N.A + I]);
			}
			return true;
		}
		return NumbersOf(*this, Out);
	}

	bool Json::Value::Numbers(std::vector<int>& Out) const
	{
		if (Kind() == Type::Array && Doc->Nodes[Node].bNumbers)
		{
			const Json::Node& N = Doc->Nodes[Node];
			Out.resize(N.B);
			for (uint32_t I = 0; I < N.B; ++I)
			{
				Out[I] = static_cast<int>(std::lround(Doc->NumberList[N.A + I]));
			}
			return true;
		}
		return NumbersOf(*this, Out);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The battle model.

	namespace
	{
		struct Lin
		{
			double R = 0.0, G = 0.0, B = 0.0;
			Lin() = default;
			Lin(double InR, double InG, double InB) : R(InR), G(InG), B(InB) {}
			Lin operator*(double S) const { return Lin(R * S, G * S, B * S); }
			Lin operator+(const Lin& O) const { return Lin(R + O.R, G + O.G, B + O.B); }
		};
		Lin Mix(const Lin& A, const Lin& B, double T) { return Lin(Lerp(A.R, B.R, T), Lerp(A.G, B.G, T), Lerp(A.B, B.B, T)); }
		double ToLinear(double C255)
		{
			const double C = C255 / 255.0;
			return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
		}
		uint8_t ToPicture(double L)
		{
			L = Clamp(L, 0.0, 1.0);
			const double C = L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055;
			return static_cast<uint8_t>(std::lround(Clamp(C * 255.0, 0.0, 255.0)));
		}
		Lin FromPicture(double R, double G, double B) { return Lin(ToLinear(R), ToLinear(G), ToLinear(B)); }
		Lin FromHex(const std::string& Hex)
		{
			unsigned V = 0x808080;
			if (Hex.size() >= 6)
			{
				V = static_cast<unsigned>(std::strtoul(Hex.substr(Hex.size() - 6).c_str(), nullptr, 16));
			}
			return FromPicture((V >> 16) & 255, (V >> 8) & 255, V & 255);
		}
		V3 ReadV3(const Json::Value& V, const V3& Otherwise = V3())
		{
			return V.IsArray() && V.Count() >= 3 ? V3(V.At(0).Num(), V.At(1).Num(), V.At(2).Num()) : Otherwise;
		}

		/** A smooth noise in space (value noise on a lattice), 0 to 1: the camouflage's blotches, chipped paint, soot. */
		double Lattice(int64_t X, int64_t Y, int64_t Z, uint64_t Seed)
		{
			return Unit01(Mix(static_cast<uint64_t>(X) * 73856093ull, static_cast<uint64_t>(Y) * 19349663ull,
				(static_cast<uint64_t>(Z) * 83492791ull) ^ Seed));
		}
		double Noise(const V3& P, uint64_t Seed)
		{
			const double FX = std::floor(P.X), FY = std::floor(P.Y), FZ = std::floor(P.Z);
			const int64_t X = static_cast<int64_t>(FX), Y = static_cast<int64_t>(FY), Z = static_cast<int64_t>(FZ);
			const double U = Smooth(0.0, 1.0, P.X - FX), V = Smooth(0.0, 1.0, P.Y - FY), W = Smooth(0.0, 1.0, P.Z - FZ);
			double C[2][2][2];
			for (int I = 0; I < 2; ++I)
			{
				for (int J = 0; J < 2; ++J)
				{
					for (int K = 0; K < 2; ++K)
					{
						C[I][J][K] = Lattice(X + I, Y + J, Z + K, Seed);
					}
				}
			}
			const double X00 = Lerp(C[0][0][0], C[1][0][0], U), X10 = Lerp(C[0][1][0], C[1][1][0], U);
			const double X01 = Lerp(C[0][0][1], C[1][0][1], U), X11 = Lerp(C[0][1][1], C[1][1][1], U);
			return Lerp(Lerp(X00, X10, V), Lerp(X01, X11, V), W);
		}
		/** Two octaves, a little stretched along the hull (sprayed blotches run more along a vehicle than up it), each
		 * blotch about half a metre across: big enough to break up her outline at a few hundred metres. */
		double Blotch(const V3& P, uint64_t Seed)
		{
			const V3 Q(P.X * 0.75, P.Y * 1.0, P.Z * 1.15);
			return 0.68 * Noise(Q * 1.8, Seed) + 0.32 * Noise(Q * 4.2, Seed + 7);
		}

		bool IsLinkName(const std::string& N)
		{
			if (N.size() < 9 || N.compare(0, 6, "track_") != 0 || (N[6] != 'l' && N[6] != 'r') || N[7] != '.')
			{
				return false;
			}
			for (size_t I = 8; I < N.size(); ++I)
			{
				if (N[I] < '0' || N[I] > '9')
				{
					return false;
				}
			}
			return true;
		}
		bool IsTrackName(const std::string& N) { return N == "track_l" || N == "track_r"; }

		PartKind KindOf(const std::string& K)
		{
			if (K == "root") return PartKind::Root;
			if (K == "turn") return PartKind::Turn;
			if (K == "spin") return PartKind::Spin;
			if (K == "slide") return PartKind::Slide;
			if (K == "free") return PartKind::Free;
			return PartKind::Fixed;
		}

		/** The month's row of a list of rows (from and to both counted in; a row with neither holds every month), or a null. */
		Json::Value RowFor(const Json::Value& Rows, const std::string& Key)
		{
			for (size_t I = 0; Rows.IsArray() && I < Rows.Count(); ++I)
			{
				const Json::Value Row = Rows.At(I);
				const std::string From = Row["from"].Str(""), To = Row["to"].Str("");
				if ((From.empty() || From <= Key) && (To.empty() || Key <= To))
				{
					return Row;
				}
			}
			return Json::Value();
		}
		/** The month's row of a model's swaps.by_month, or a null. */
		Json::Value MonthRow(const Json::Value& Swaps, const std::string& Key) { return RowFor(Swaps["by_month"], Key); }
		/** A row's choice for one swap: a name, or a mix's "one" (the face a game showing one look a unit takes). */
		std::string ChoiceIn(const Json::Value& Row, const std::string& Swap)
		{
			const Json::Value C = Row[Swap];
			return C.IsString() ? C.Str() : C["one"].Str("");
		}

		/** Piecewise straight between keys (times T, Width numbers a key in V), at time X; Out gets Width numbers. */
		void Sample(const std::vector<float>& T, const std::vector<float>& V, size_t Width, double X, double* Out)
		{
			const size_t N = T.size();
			if (N == 0)
			{
				return;
			}
			if (X <= T[0] || N == 1)
			{
				for (size_t I = 0; I < Width; ++I) Out[I] = V[I];
				return;
			}
			if (X >= T[N - 1])
			{
				for (size_t I = 0; I < Width; ++I) Out[I] = V[(N - 1) * Width + I];
				return;
			}
			// (a time just short of the last key can round up to it as a float, and upper_bound then gives the end)
			const size_t Hi = std::min(N - 1, static_cast<size_t>(std::upper_bound(T.begin(), T.end(), static_cast<float>(X)) - T.begin()));
			const size_t Lo = Hi - 1;
			const double Span = static_cast<double>(T[Hi]) - T[Lo];
			const double U = Span > 1e-9 ? (X - T[Lo]) / Span : 0.0;
			for (size_t I = 0; I < Width; ++I)
			{
				Out[I] = Lerp(V[Lo * Width + I], V[Hi * Width + I], U);
			}
		}
		Quat SampleRot(const ClipTrack& K, double X)
		{
			double Q[4] = { 1.0, 0.0, 0.0, 0.0 };
			// (keys a few hundredths of a second apart: a straight blend of the four numbers, kept to one hemisphere,
			// then made a unit again, is a slerp to well under a hundredth of a degree)
			const size_t N = K.RotT.size();
			if (N >= 2 && X > K.RotT[0] && X < K.RotT[N - 1])
			{
				const size_t Hi = std::min(N - 1, static_cast<size_t>(std::upper_bound(K.RotT.begin(), K.RotT.end(), static_cast<float>(X)) - K.RotT.begin()));
				const size_t Lo = Hi - 1;
				const double Span = static_cast<double>(K.RotT[Hi]) - K.RotT[Lo];
				const double U = Span > 1e-9 ? (X - K.RotT[Lo]) / Span : 0.0;
				double Dot = 0.0;
				for (size_t I = 0; I < 4; ++I) Dot += static_cast<double>(K.Rot[Lo * 4 + I]) * K.Rot[Hi * 4 + I];
				const double Sign = Dot < 0.0 ? -1.0 : 1.0;
				for (size_t I = 0; I < 4; ++I) Q[I] = Lerp(K.Rot[Lo * 4 + I], Sign * K.Rot[Hi * 4 + I], U);
			}
			else
			{
				Sample(K.RotT, K.Rot, 4, X, Q);
			}
			return Quat(Q[0], Q[1], Q[2], Q[3]).Unit();
		}

		/** The nation's coat for a model that has none of its own (the lorry stands in for every army's). */
		bool NationCoat(const Dress& D, std::string& OutName, std::string& OutUpper, std::string& OutUpper2, std::string& OutShade)
		{
			if (D.Terrain == "desert")
			{
				// On a desert field the lorry wears her owner's desert coat, as the tanks' files pick theirs there
				// (swaps.paint.by_ground; tools/battle-models/land tanks_ger.py and motorpool.py): the Germans' order of
				// March 1941 (brown-yellow, the grey-green blotched where the lorry's file lays them) to February 1942,
				// then that of March 1942 (light brown, grey) to May 1943; the British Light Stone and the Italians' own
				// sand khaki, one colour all over. Everyone else (the Americans' olive drab) as on any other ground.
				const int Key = D.Year * 12 + D.Month;
				if (D.Nation == "GER" && Key <= 1943 * 12 + 5)
				{
					const bool b41 = Key <= 1942 * 12 + 2;
					OutName = b41 ? "desert41" : "desert42";
					OutUpper = b41 ? "A2814F" : "B49B6C";
					OutUpper2 = b41 ? "6B6243" : "7F7D72";
					OutShade = b41 ? "644F30" : "6F6042";
					return true;
				}
				if (D.Nation == "UK" || D.Nation == "ITA")
				{
					const bool bUK = D.Nation == "UK";
					OutName = bUK ? "stone" : "sand";
					OutUpper = bUK ? "C9B88E" : "8A7F5E";
					OutUpper2 = OutUpper;
					OutShade = bUK ? "7C7258" : "554E3A";
					return true;
				}
			}
			if (D.Nation == "USA")
			{
				// Olive drab, one colour all over (the camouflage file: a nation with one colour fills upper2 with upper),
				// leaning brown as the paint did (the art director's mend, 21 November 2026: the old 565C3A read lime in
				// the light). Written with about twice the colour wanted on the screen, since the ground's grade keeps
				// 0.48 of the middle tones' colour (her cut 5 read, item 7: 585139 landed blue-grey, 98,108,106); the
				// same coat as the American vehicles' files (tools/battle-models/land/motorpool.py).
				OutName = "olive drab";
				OutUpper = "625024";
				OutUpper2 = "625024";
				OutShade = "3B3015";
				return true;
			}
			if (D.Nation == "GER")
			{
				// Dark yellow from February 1943 with olive green blotches where the lorry's file lays them (grey before);
				// a dull ochre, not a bright sand (the art director's mend, 21 November 2026).
				const bool bYellow = D.Year * 12 + D.Month >= 1943 * 12 + 2;
				OutName = bYellow ? "yellow" : "grey";
				OutUpper = bYellow ? "8E7E58" : "4E5459";
				OutUpper2 = bYellow ? "5A6240" : "3E4347";
				OutShade = bYellow ? "584E36" : "303437";
				return true;
			}
			return false;
		}
	}

	bool Model::RoofGun() const
	{
		const int I = PartIndex("roof_mg");
		return I >= 0 && Parts[static_cast<size_t>(I)].bShown;
	}

	int Model::PartIndex(const std::string& PartName) const
	{
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			if (Parts[I].Name == PartName)
			{
				return static_cast<int>(I);
			}
		}
		return -1;
	}

	int Model::SocketIndex(const std::string& SocketName) const
	{
		for (size_t I = 0; I < Sockets.size(); ++I)
		{
			if (Sockets[I].Name == SocketName)
			{
				return static_cast<int>(I);
			}
		}
		return -1;
	}

	const Clip* Model::FindClip(const std::string& ClipName) const
	{
		const auto It = Clips.find(ClipName);
		return It == Clips.end() ? nullptr : &It->second;
	}

	namespace
	{
		/** One level of detail as the file has it (every point's part numbered in the full list of parts). */
		struct RawMesh
		{
			std::vector<float> Points, Facings;
			std::vector<int> Paints, Parts, Blend, SootIn, Wear;
			// (the modeller's later files, from the Tiger of 22 November: the third coat colour's blend2, the road dirt
			// dust and the pale haze on what looks up, 0 to 100 a point, and shade, how open a point is to the sky, 0 to 1)
			std::vector<int> Blend2, DustIn, Haze;
			std::vector<float> ShadeIn;
			bool bDust = false;
			std::vector<uint32_t> Triangles;
			size_t Count() const { return Paints.size(); }
		};

		bool ReadRaw(const Json::Value& M, const std::vector<int>& PartNumbers, RawMesh& Out, std::string& OutError)
		{
			std::vector<int> Tri;
			if (!M["points"].Numbers(Out.Points) || !M["facings"].Numbers(Out.Facings) || !M["paints"].Numbers(Out.Paints)
				|| !M["triangles"].Numbers(Tri) || !M["part"].Numbers(Out.Parts))
			{
				OutError = "a mesh without points, facings, paints, triangles or parts";
				return false;
			}
			const size_t N = Out.Paints.size();
			if (Out.Points.size() != N * 3 || Out.Facings.size() != N * 3 || Out.Parts.size() != N || Tri.size() % 3 != 0)
			{
				OutError = "a mesh whose lists do not agree";
				return false;
			}
			M["blend"].Numbers(Out.Blend);
			M["soot"].Numbers(Out.SootIn);
			M["wear"].Numbers(Out.Wear);
			M["blend2"].Numbers(Out.Blend2);
			Out.bDust = M["dust"].Numbers(Out.DustIn) && Out.DustIn.size() == N;
			M["haze"].Numbers(Out.Haze);
			M["shade"].Numbers(Out.ShadeIn);
			Out.Blend.resize(N, 0);
			Out.SootIn.resize(N, 0);
			Out.Wear.resize(N, 0);
			Out.Blend2.resize(N, 0);
			Out.DustIn.resize(N, 0);
			Out.Haze.resize(N, 0);
			Out.ShadeIn.resize(N, 1.0f);
			Out.Triangles.resize(Tri.size());
			for (size_t I = 0; I < Tri.size(); ++I)
			{
				if (Tri[I] < 0 || static_cast<size_t>(Tri[I]) >= N)
				{
					OutError = "a triangle past the points";
					return false;
				}
				Out.Triangles[I] = static_cast<uint32_t>(Tri[I]);
			}
			// The far mesh numbers its parts without the track links (Tools/battle_vehicle.py): turn them into the full list's.
			for (int& P : Out.Parts)
			{
				if (P < 0 || static_cast<size_t>(P) >= PartNumbers.size())
				{
					OutError = "a point's part past the parts";
					return false;
				}
				P = PartNumbers[static_cast<size_t>(P)];
			}
			return true;
		}

		/** Points on a sharp edge (where the mesh splits a corner's normals, as it does on every plate's edge): the
		 * paint chips there first. 1 on an edge, 0 elsewhere. */
		std::vector<float> Edges(const RawMesh& R)
		{
			const size_t N = R.Count();
			std::vector<float> Out(N, 0.0f);
			std::unordered_map<uint64_t, uint32_t> Head;
			Head.reserve(N);
			std::vector<uint32_t> Next(N, UINT32_MAX);
			auto Key = [&R](size_t I)
			{
				const int64_t X = std::llround(R.Points[I * 3] * 2000.0), Y = std::llround(R.Points[I * 3 + 1] * 2000.0), Z = std::llround(R.Points[I * 3 + 2] * 2000.0);
				return (static_cast<uint64_t>(X & 0x1FFFFF) << 42) | (static_cast<uint64_t>(Y & 0x1FFFFF) << 21) | static_cast<uint64_t>(Z & 0x1FFFFF);
			};
			for (size_t I = 0; I < N; ++I)
			{
				const uint64_t K = Key(I);
				const auto It = Head.find(K);
				if (It == Head.end())
				{
					Head.emplace(K, static_cast<uint32_t>(I));
				}
				else
				{
					Next[I] = It->second;
					It->second = static_cast<uint32_t>(I);
				}
			}
			for (const auto& Group : Head)
			{
				double Least = 1.0;
				for (uint32_t A = Group.second; A != UINT32_MAX; A = Next[A])
				{
					for (uint32_t B = Next[A]; B != UINT32_MAX; B = Next[B])
					{
						const double D = static_cast<double>(R.Facings[A * 3]) * R.Facings[B * 3] + static_cast<double>(R.Facings[A * 3 + 1]) * R.Facings[B * 3 + 1]
							+ static_cast<double>(R.Facings[A * 3 + 2]) * R.Facings[B * 3 + 2];
						Least = std::min(Least, D);
					}
				}
				if (Least < 0.82) // more than about 35 degrees between the faces
				{
					for (uint32_t A = Group.second; A != UINT32_MAX; A = Next[A])
					{
						Out[A] = 1.0f;
					}
				}
			}
			return Out;
		}

		/** Cuts the triangles of one paint until none has an edge longer than Most (metres). The colours are laid on
		 * the points, and a hull side is one plate of two triangles: without this a blotch smaller than the plate cannot
		 * show. Each long edge is halved once for every triangle that shares it, and a triangle is cut in two, three
		 * or four by how many of its edges were halved, so no point sits in the middle of a neighbour's edge (that
		 * would open hairline cracks). */
		void Refine(RawMesh& R, int Paint, double Most)
		{
			const double Most2 = Most * Most;
			auto Length2 = [&R](uint32_t A, uint32_t B)
			{
				double S = 0.0;
				for (size_t I = 0; I < 3; ++I)
				{
					const double D = static_cast<double>(R.Points[A * 3 + I]) - R.Points[B * 3 + I];
					S += D * D;
				}
				return S;
			};
			auto EdgeKey = [](uint32_t A, uint32_t B) { return (static_cast<uint64_t>(std::min(A, B)) << 32) | std::max(A, B); };
			// (a few rounds halve a six-metre plate to a quarter of a metre)
			for (int Round = 0; Round < 8; ++Round)
			{
				std::unordered_map<uint64_t, uint32_t> Halves;
				const size_t Before = R.Triangles.size() / 3;
				for (size_t T = 0; T < Before; ++T)
				{
					const uint32_t* V = &R.Triangles[T * 3];
					if (R.Paints[V[0]] != Paint || R.Paints[V[1]] != Paint || R.Paints[V[2]] != Paint)
					{
						continue;
					}
					for (size_t E = 0; E < 3; ++E)
					{
						const uint32_t A = V[E], B = V[(E + 1) % 3];
						if (Length2(A, B) <= Most2 || !Halves.emplace(EdgeKey(A, B), static_cast<uint32_t>(R.Count())).second)
						{
							continue;
						}
						// The new point halfway along, with everything the file gives a point (the pair added in either
						// order gives the same place, so a plate's two split corners stay together).
						for (size_t I = 0; I < 3; ++I)
						{
							R.Points.push_back(0.5f * (R.Points[A * 3 + I] + R.Points[B * 3 + I]));
						}
						const V3 Facing = V3(static_cast<double>(R.Facings[A * 3]) + R.Facings[B * 3], static_cast<double>(R.Facings[A * 3 + 1]) + R.Facings[B * 3 + 1],
							static_cast<double>(R.Facings[A * 3 + 2]) + R.Facings[B * 3 + 2]).Unit();
						R.Facings.push_back(static_cast<float>(Facing.X));
						R.Facings.push_back(static_cast<float>(Facing.Y));
						R.Facings.push_back(static_cast<float>(Facing.Z));
						R.Paints.push_back(Paint);
						R.Parts.push_back(R.Parts[A]);
						R.Blend.push_back((R.Blend[A] + R.Blend[B]) / 2);
						R.SootIn.push_back((R.SootIn[A] + R.SootIn[B]) / 2);
						R.Wear.push_back((R.Wear[A] + R.Wear[B]) / 2);
						R.Blend2.push_back((R.Blend2[A] + R.Blend2[B]) / 2);
						R.DustIn.push_back((R.DustIn[A] + R.DustIn[B]) / 2);
						R.Haze.push_back((R.Haze[A] + R.Haze[B]) / 2);
						R.ShadeIn.push_back(0.5f * (R.ShadeIn[A] + R.ShadeIn[B]));
					}
				}
				if (Halves.empty())
				{
					return;
				}
				auto Half = [&](uint32_t A, uint32_t B)
				{
					const auto It = Halves.find(EdgeKey(A, B));
					return It == Halves.end() ? UINT32_MAX : It->second;
				};
				std::vector<uint32_t> Cut;
				Cut.reserve(R.Triangles.size() * 2);
				auto Add = [&Cut](uint32_t A, uint32_t B, uint32_t C)
				{
					Cut.push_back(A);
					Cut.push_back(B);
					Cut.push_back(C);
				};
				for (size_t T = 0; T < Before; ++T)
				{
					const uint32_t V[3] = { R.Triangles[T * 3], R.Triangles[T * 3 + 1], R.Triangles[T * 3 + 2] };
					const uint32_t M[3] = { Half(V[0], V[1]), Half(V[1], V[2]), Half(V[2], V[0]) };
					const int Halved = (M[0] != UINT32_MAX) + (M[1] != UINT32_MAX) + (M[2] != UINT32_MAX);
					if (Halved == 0)
					{
						Add(V[0], V[1], V[2]);
					}
					else if (Halved == 3)
					{
						Add(V[0], M[0], M[2]);
						Add(M[0], V[1], M[1]);
						Add(M[2], M[1], V[2]);
						Add(M[0], M[1], M[2]);
					}
					else if (Halved == 1)
					{
						// (the halved edge runs from V[E] to the next corner; the winding is kept)
						const size_t E = M[0] != UINT32_MAX ? 0 : (M[1] != UINT32_MAX ? 1 : 2);
						Add(V[E], M[E], V[(E + 2) % 3]);
						Add(M[E], V[(E + 1) % 3], V[(E + 2) % 3]);
					}
					else
					{
						// Two halved: the whole edge is U's, from corner A to B; the far corner C keeps a small triangle.
						const size_t U = M[0] == UINT32_MAX ? 0 : (M[1] == UINT32_MAX ? 1 : 2);
						const uint32_t A = V[U], B = V[(U + 1) % 3], C = V[(U + 2) % 3];
						const uint32_t BC = M[(U + 1) % 3], CA = M[(U + 2) % 3];
						Add(A, B, BC);
						Add(A, BC, CA);
						Add(CA, BC, C);
					}
				}
				R.Triangles.swap(Cut);
			}
		}
	}

	bool Model::Load(const std::string& Text, const Dress& D, std::string& OutError)
	{
		Json Doc;
		if (!Doc.Parse(Text, OutError))
		{
			return false;
		}
		const Json::Value R = Doc.Root();
		if (!R.IsObject() || !R["parts"].IsArray() || !R["near"].IsObject() || !R["far"].IsObject())
		{
			OutError = "not a battle model (no parts, near or far)";
			return false;
		}
		*this = Model();
		Dressed = D;
		Name = R["name"].Str();
		Title = Name;
		Kind = R["kind"].Str();
		Nation = R["nation"].Str();
		Long = R["long"].Num();
		Wide = R["wide"].Num();
		High = R["high"].Num();

		// The paints: the file's own colours, the coat of the month in place of upper, upper2 and shade.
		const Json::Value ColourList = R["colours"], PaintList = R["paint_names"];
		PaintNames.clear();
		for (size_t I = 0; I < PaintList.Count(); ++I)
		{
			PaintNames.push_back(PaintList.At(I).Str());
		}
		std::vector<Lin> Paint(std::max<size_t>(ColourList.Count(), 23), Lin(0.2, 0.2, 0.2));
		for (size_t I = 0; I < ColourList.Count(); ++I)
		{
			Paint[I] = FromHex(ColourList.At(I).Str("808080"));
		}
		auto PaintNumber = [&PaintList](const char* PaintName, int Otherwise)
		{
			for (size_t I = 0; I < PaintList.Count(); ++I)
			{
				if (PaintList.At(I).Is(PaintName))
				{
					return static_cast<int>(I);
				}
			}
			return Otherwise;
		};
		const int Upper = PaintNumber("upper", 0), Upper2 = PaintNumber("upper2", 1), Shade = PaintNumber("shade", 2);
		const int Tread = PaintNumber("track", 3), Rubber = PaintNumber("rubber", 4), Steel = PaintNumber("steel", 5);
		const int Black = PaintNumber("black", 10), Glass = PaintNumber("glass", 9), Canvas = PaintNumber("canvas", 8);
		const int Upper3 = PaintNumber("upper3", -1);
		// A file that lays its own road dirt (dust, from the modeller's Tiger of 22 November) is painted as it says, and
		// nothing of the game's own goes over it: no dust, sun fade, sprayed camouflage or chipped edges (its own wear
		// says where the paint is chipped). Its dust and haze colours: the file's, or the chosen coat's own.
		const bool bFileDust = R["near"]["dust"].IsArray();
		Lin DustColour = FromHex(R["dust_colour"].Str("9A8A6A")), HazeColour = FromHex(R["haze_colour"].Str(R["dust_colour"].Str("9A8A6A").c_str()));

		char MonthKey[16];
		std::snprintf(MonthKey, sizeof(MonthKey), "%04d-%02d", D.Year, D.Month);
		const Json::Value Swaps = R["swaps"];
		const Json::Value Row = MonthRow(Swaps, MonthKey);
		const Json::Value PaintSwap = Swaps["paint"];
		bool bCamouflage = false;
		std::string CoatUpper, CoatUpper2, CoatShade, CoatUpper3;
		if (PaintSwap["choices"].IsObject())
		{
			// The field's ground first (swaps.paint.by_ground: on a desert field the German desert coats of 1941 and 1942
			// and the British Light Stone): its rows are read as by_month's are, and a month none of them holds (or a
			// coat the file does not have) falls back to by_month.
			std::string Choice;
			if (!D.Terrain.empty())
			{
				Choice = ChoiceIn(RowFor(PaintSwap["by_ground"][D.Terrain], MonthKey), "paint");
				if (!Choice.empty() && !PaintSwap["choices"][Choice].IsObject())
				{
					Choice.clear();
				}
			}
			if (Choice.empty())
			{
				Choice = ChoiceIn(Row, "paint");
			}
			if (Choice.empty() && Nation == "GER" && PaintSwap["choices"]["yellow"].IsObject())
			{
				// Dark yellow from February 1943 (the Tiger's file names the years but has no months).
				Choice = std::string(MonthKey) >= "1943-02" ? "yellow" : "grey";
			}
			if (Choice.empty())
			{
				Choice = PaintSwap["default"].Str("");
			}
			const Json::Value Chosen = PaintSwap["choices"][Choice]["paints"];
			CoatUpper = Chosen["upper"].Str("");
			CoatUpper2 = Chosen["upper2"].Str("");
			CoatShade = Chosen["shade"].Str("");
			CoatUpper3 = Chosen["upper3"].Str("");
			const Json::Value ChoiceRow = PaintSwap["choices"][Choice];
			if (ChoiceRow["dust_colour"].IsString()) DustColour = FromHex(ChoiceRow["dust_colour"].Str());
			if (ChoiceRow["haze_colour"].IsString()) HazeColour = FromHex(ChoiceRow["haze_colour"].Str());
			Coat = Choice;
			// The dark yellow came out as a base coat with pastes of olive green and red-brown for the crews to spray in
			// blotches and bands, and by 1944 nearly every German vehicle at the front wore them. The model files give
			// the blotches only for the lorry, so they are laid here, point by point: the green goes in upper2 (the
			// file's own upper2, a darker yellow, is too near the base to read as camouflage at any distance).
			bCamouflage = Choice == "yellow" && !bFileDust;
			if (bCamouflage)
			{
				CoatUpper2 = "5A6240";
			}
		}
		else if (Nation == "any" || R["camouflage"].IsObject())
		{
			// The lorry stands in for both sides' (the real ones were the GMC two-and-a-half-tonner and the Opel Blitz):
			// painted in her owner's coat, olive drab for the Americans and dark yellow for the Germans.
			if (!NationCoat(D, Coat, CoatUpper, CoatUpper2, CoatShade))
			{
				Coat = "own";
			}
		}
		else
		{
			Coat = Nation == "USA" ? "olive drab" : "own";
		}
		if (!CoatUpper.empty()) Paint[static_cast<size_t>(Upper)] = FromHex(CoatUpper);
		if (!CoatUpper2.empty()) Paint[static_cast<size_t>(Upper2)] = FromHex(CoatUpper2);
		if (!CoatShade.empty()) Paint[static_cast<size_t>(Shade)] = FromHex(CoatShade);
		if (Upper3 >= 0 && static_cast<size_t>(Upper3) < Paint.size())
		{
			// (a coat with no third colour of its own: the second's, so blend2 changes nothing)
			Paint[static_cast<size_t>(Upper3)] = !CoatUpper3.empty() ? FromHex(CoatUpper3) : !CoatUpper2.empty() ? FromHex(CoatUpper2) : Paint[static_cast<size_t>(Upper3)];
		}
		// Captured kit (release 2, on its switch): her maker's coat as above kept for the wear, then her captor's put over
		// it, one colour brushed on in the field (no sprayed blotches): the captor's coat where the game has one already
		// (NationCoat: the Germans' grey or dark yellow, the Americans' olive drab, the British and Italian desert
		// coats), the Soviets' the T-34's own green (the tanks thread's file); any other captor keeps her maker's coat.
		Lin OldCoat[3] = { Paint[static_cast<size_t>(Upper)], Paint[static_cast<size_t>(Upper2)], Paint[static_cast<size_t>(Shade)] };
		bool bCaptured = false;
		if (!D.Captured.empty())
		{
			std::string New, NewUpper, NewUpper2, NewShade;
			if (D.Nation == "SOV")
			{
				New = "the T-34's green";
				NewUpper = "4F5E3E";
				NewUpper2 = "3F4B31";
				NewShade = "303A26";
			}
			else if (!NationCoat(D, New, NewUpper, NewUpper2, NewShade))
			{
				New.clear();
			}
			if (!New.empty())
			{
				bCaptured = true;
				bCamouflage = false;
				Coat = "captured from " + D.Captured + ", " + New;
				Paint[static_cast<size_t>(Upper)] = FromHex(NewUpper);
				Paint[static_cast<size_t>(Upper2)] = FromHex(NewUpper);
				Paint[static_cast<size_t>(Shade)] = FromHex(NewShade);
				if (Upper3 >= 0 && static_cast<size_t>(Upper3) < Paint.size()) Paint[static_cast<size_t>(Upper3)] = FromHex(NewUpper);
			}
		}
		const bool bAmerican = D.Nation == "USA";

		// The month's swaps of parts (the Panzer IV's long gun and her skirts, the lorry's tilt): parts named only by a
		// choice not taken are scaled to nothing, and the chosen face's sockets stand in for the default ones.
		std::vector<std::string> Hidden;
		std::vector<std::pair<std::string, std::string>> SocketSwaps;
		for (size_t I = 0; I < Swaps.Count(); ++I)
		{
			const std::string Group = Swaps.Name(I);
			const Json::Value S = Swaps.At(I);
			if (Group == "paint" || !S["choices"].IsObject())
			{
				continue;
			}
			// (release 2: a choice the rules picked for her, where her model has it: a motorcycle's sidecar, a staff jeep's gun)
			const auto Chosen = D.Choose.find(Group);
			std::string Pick = Chosen != D.Choose.end() && S["choices"][Chosen->second].IsObject() ? Chosen->second : ChoiceIn(Row, Group);
			// (a swap chosen by the nation she serves, where the month does not choose it: by_nation, the British
			// Sherman without the American's .50 on her roof)
			if (Pick.empty() && S["by_nation"].IsObject())
			{
				Pick = S["by_nation"][D.Nation].Str("");
			}
			if (Pick.empty() || !S["choices"][Pick].IsObject())
			{
				Pick = S["default"].Str("");
			}
			const Json::Value Choices = S["choices"];
			std::vector<std::string> Keep;
			const Json::Value KeepList = Choices[Pick]["parts"];
			for (size_t J = 0; J < KeepList.Count(); ++J)
			{
				Keep.push_back(KeepList.At(J).Str());
			}
			for (size_t C = 0; C < Choices.Count(); ++C)
			{
				if (Choices.Name(C) == Pick)
				{
					continue;
				}
				const Json::Value Others = Choices.At(C)["parts"];
				for (size_t J = 0; J < Others.Count(); ++J)
				{
					const std::string P = Others.At(J).Str();
					if (std::find(Keep.begin(), Keep.end(), P) == Keep.end())
					{
						Hidden.push_back(P);
					}
				}
			}
			const Json::Value Sock = Choices[Pick]["sockets"];
			for (size_t J = 0; J < Sock.Count(); ++J)
			{
				SocketSwaps.emplace_back(Sock.Name(J), Sock.At(J).Str());
			}
		}

		// The parts.
		const Json::Value PartList = R["parts"];
		Parts.resize(PartList.Count());
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			const Json::Value P = PartList.At(I);
			Part& Out = Parts[I];
			Out.Name = P["name"].Str();
			Out.Parent = P["parent"].Int(-1);
			if (Out.Parent >= static_cast<int>(I))
			{
				OutError = "a part listed before its parent: " + Out.Name;
				return false;
			}
			Out.Pivot = ReadV3(P["pivot"]);
			Out.Kind = KindOf(P["kind"].Str());
			Out.Axis = ReadV3(P["axis"], V3(0.0, 0.0, 1.0)).Unit();
			if (P["limits"].Count() >= 2)
			{
				Out.bLimits = true;
				Out.Lo = P["limits"].At(0).Num();
				Out.Hi = P["limits"].At(1).Num();
			}
			Out.SlideWay = ReadV3(P["slide"], V3(0.0, 0.0, 0.0));
			if (P["slide_limits"].Count() >= 2)
			{
				Out.SlideLo = P["slide_limits"].At(0).Num();
				Out.SlideHi = P["slide_limits"].At(1).Num();
			}
			Out.bLink = IsLinkName(Out.Name);
			if (Out.bLink || IsTrackName(Out.Name))
			{
				Out.Side = Out.Name[6] == 'l' ? -1 : 1;
			}
			Out.bShown = std::find(Hidden.begin(), Hidden.end(), Out.Name) == Hidden.end()
				&& (Out.Parent < 0 || Parts[static_cast<size_t>(Out.Parent)].bShown);
		}

		// The controls and the running gear.
		const Json::Value Controls = R["controls"];
		Turret = PartIndex(Controls["turret"]["part"].Str());
		TurretDps = Controls["turret"]["degrees_per_second"].Num(0.0);
		Gun = PartIndex(Controls["gun"]["part"].Str());
		Traverse = PartIndex(Controls["traverse"]["part"].Str());
		TraverseDps = Controls["traverse"]["degrees_per_second"].Num(0.0);
		for (size_t I = 0; I < Controls["hatches"].Count(); ++I)
		{
			const int H = PartIndex(Controls["hatches"].At(I).Str());
			if (H >= 0)
			{
				Hatches.push_back(H);
			}
		}
		CommanderHatch = PartIndex("hatch_commander");
		for (size_t I = 0; I < Controls["steer"]["parts"].Count(); ++I)
		{
			const int P = PartIndex(Controls["steer"]["parts"].At(I).Str());
			if (P >= 0)
			{
				Steer.push_back(P);
			}
		}
		if (Controls["steer"]["limits"].Count() >= 2)
		{
			SteerLimit = std::fabs(Controls["steer"]["limits"].At(1).Num(32.0));
		}
		for (size_t I = 0; I < Controls["trail"]["parts"].Count(); ++I)
		{
			const int P = PartIndex(Controls["trail"]["parts"].At(I).Str());
			if (P >= 0)
			{
				Trails.push_back(P);
			}
		}
		TrailClosed = Controls["trail"]["closed"].Num(0.0);
		Hull = PartIndex("hull") >= 0 ? PartIndex("hull") : PartIndex("carriage");
		Barrel = PartIndex("barrel");
		Breech = PartIndex("breech");
		const Json::Value WheelList = R["running"]["wheels"];
		for (size_t I = 0; I < WheelList.Count(); ++I)
		{
			const int P = PartIndex(WheelList.At(I)["part"].Str());
			if (P >= 0)
			{
				Parts[static_cast<size_t>(P)].Radius = WheelList.At(I)["radius"].Num(0.3);
				Parts[static_cast<size_t>(P)].Side = Parts[static_cast<size_t>(P)].Pivot.Y < 0.0 ? -1 : 1;
				Wheels.push_back(P);
			}
		}

		// The sockets (with the month's face's in place of the default ones) and the marks.
		const Json::Value SocketList = R["sockets"];
		for (size_t I = 0; I < SocketList.Count(); ++I)
		{
			const Json::Value S = SocketList.At(I);
			Socket Out;
			Out.Name = S["name"].Str();
			Out.Part = std::max(0, std::min(S["part"].Int(0), static_cast<int>(Parts.size()) - 1));
			Out.At = ReadV3(S["at"]);
			Out.Facing = ReadV3(S["facing"], V3(1.0, 0.0, 0.0)).Unit(V3(1.0, 0.0, 0.0));
			Sockets.push_back(Out);
		}
		for (const auto& [Default, Chosen] : SocketSwaps)
		{
			const int A = SocketIndex(Default), B = SocketIndex(Chosen);
			if (A >= 0 && B >= 0 && A != B)
			{
				const std::string Keep = Sockets[static_cast<size_t>(A)].Name;
				Sockets[static_cast<size_t>(A)] = Sockets[static_cast<size_t>(B)];
				Sockets[static_cast<size_t>(A)].Name = Keep;
			}
		}
		const Json::Value MarkList = R["marks"];
		for (size_t I = 0; I < MarkList.Count(); ++I)
		{
			const Json::Value S = MarkList.At(I);
			Mark Out;
			Out.Tag = S["tag"].Str();
			Out.At = ReadV3(S["at"]);
			Out.Facing = ReadV3(S["facing"], V3(1.0, 0.0, 0.0)).Unit(V3(1.0, 0.0, 0.0));
			Out.Radius = S["radius"].Num(0.2);
			Out.Part = std::max(0, PartIndex(S["part"].Str()));
			Marks.push_back(Out);
		}

		// The clips.
		const Json::Value ClipList = R["clips"];
		for (size_t I = 0; I < ClipList.Count(); ++I)
		{
			const Json::Value C = ClipList.At(I);
			Clip Out;
			Out.Name = ClipList.Name(I);
			Out.About = C["about"].Str();
			Out.Long = std::max(0.01, C["long"].Num(1.0));
			Out.bLoop = C["loop"].Truth();
			Out.Stride = C["stride"].Num(0.0);
			const Json::Value TrackList = C["tracks"];
			for (size_t J = 0; J < TrackList.Count(); ++J)
			{
				const Json::Value T = TrackList.At(J);
				ClipTrack K;
				K.Part = T["part"].Int(-1);
				if (K.Part < 0 || static_cast<size_t>(K.Part) >= Parts.size())
				{
					continue;
				}
				auto Keys = [](const Json::Value& List, size_t Width, std::vector<float>& OutT, std::vector<float>& OutV)
				{
					std::vector<double> Key;
					for (size_t Q = 0; Q < List.Count(); ++Q)
					{
						if (List.At(Q).Numbers(Key) && Key.size() >= Width + 1)
						{
							OutT.push_back(static_cast<float>(Key[0]));
							for (size_t W = 0; W < Width; ++W)
							{
								OutV.push_back(static_cast<float>(Key[1 + W]));
							}
						}
					}
				};
				Keys(T["rot"], 4, K.RotT, K.Rot);
				Keys(T["move"], 3, K.MoveT, K.Move);
				Keys(T["turn"], 1, K.TurnT, K.Turn);
				Keys(T["slide"], 1, K.SlideT, K.Slide);
				Out.Tracks.push_back(std::move(K));
			}
			const Json::Value EventList = C["events"];
			for (size_t J = 0; J < EventList.Count(); ++J)
			{
				const Json::Value E = EventList.At(J);
				ClipEvent Ev;
				Ev.T = E["t"].Num();
				Ev.Fx = E["fx"].Str();
				Ev.Socket = E["socket"].Str("");
				Ev.Cue = E["cue"].Str("");
				Ev.bCueLoops = E["cue_loop"].Truth() || (Ev.Cue.size() > 5 && Ev.Cue.compare(Ev.Cue.size() - 5, 5, "_loop") == 0);
				Out.Events.push_back(Ev);
			}
			std::sort(Out.Events.begin(), Out.Events.end(), [](const ClipEvent& A, const ClipEvent& B) { return A.T < B.T; });
			Clips[Out.Name] = std::move(Out);
		}
		LinkTrack.assign(Parts.size(), -1);
		for (const char* Side : { "tracks.left", "tracks.right" })
		{
			if (const Clip* C = FindClip(Side))
			{
				for (size_t J = 0; J < C->Tracks.size(); ++J)
				{
					LinkTrack[static_cast<size_t>(C->Tracks[J].Part)] = static_cast<int>(J);
				}
			}
		}
		if (const Clip* C = FindClip("tracks.left")) TrackStride[0] = C->Stride;
		if (const Clip* C = FindClip("tracks.right")) TrackStride[1] = C->Stride;

		// The bodies: each part that moves on its own, with the fixed parts under it; a track's links in the track.
		std::vector<int> Owner(Parts.size());
		std::vector<int> Coarse; // the far mesh's part numbers: the full list without the links
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			const Part& P = Parts[I];
			if (P.bLink && P.Parent >= 0)
			{
				Owner[I] = P.Parent;
			}
			else if (P.Kind == PartKind::Fixed && !IsTrackName(P.Name) && P.Parent >= 0)
			{
				Owner[I] = Owner[static_cast<size_t>(P.Parent)];
			}
			else
			{
				Owner[I] = static_cast<int>(I);
			}
			if (!P.bLink)
			{
				Coarse.push_back(static_cast<int>(I));
			}
			if (P.bLink)
			{
				(P.Side < 0 ? LinksL : LinksR).push_back(static_cast<int>(I));
			}
		}
		std::vector<int> Every(Parts.size());
		for (size_t I = 0; I < Every.size(); ++I) Every[I] = static_cast<int>(I);
		RawMesh Near, Far;
		if (!ReadRaw(R["near"], Every, Near, OutError) || !ReadRaw(R["far"], Coarse, Far, OutError))
		{
			return false;
		}
		if (bCamouflage)
		{
			// (near only: from far off a blotch is smaller than a pixel, and the far mesh's plates blend it well enough)
			Refine(Near, Upper, 0.25);
		}
		std::vector<size_t> Points(Parts.size(), 0);
		for (const RawMesh* Raw : { &Near, &Far })
		{
			for (int P : Raw->Parts)
			{
				if (Parts[static_cast<size_t>(P)].bShown)
				{
					++Points[static_cast<size_t>(Owner[static_cast<size_t>(P)])];
				}
			}
		}
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			if (Owner[I] != static_cast<int>(I) || !Parts[I].bShown || Points[I] == 0)
			{
				continue;
			}
			Body B;
			B.Name = Parts[I].Name;
			B.Part = static_cast<int>(I);
			B.Pivot = Parts[I].Pivot;
			B.bTrack = IsTrackName(Parts[I].Name);
			B.Side = Parts[I].Side;
			int Up = Parts[I].Parent;
			while (Up >= 0 && Parts[static_cast<size_t>(Owner[static_cast<size_t>(Up)])].Body < 0)
			{
				Up = Parts[static_cast<size_t>(Up)].Parent;
			}
			B.Parent = Up >= 0 ? Parts[static_cast<size_t>(Owner[static_cast<size_t>(Up)])].Body : -1;
			Parts[I].Body = static_cast<int>(Bodies.size());
			if (B.bTrack)
			{
				TrackBody[B.Side < 0 ? 0 : 1] = static_cast<int>(Bodies.size());
			}
			Bodies.push_back(std::move(B));
		}
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			Parts[I].Body = Parts[I].bShown ? Parts[static_cast<size_t>(Owner[I])].Body : -1;
		}
		auto Under = [this](int P, int Top)
		{
			for (; P >= 0; P = Parts[static_cast<size_t>(P)].Parent)
			{
				if (P == Top)
				{
					return true;
				}
			}
			return false;
		};

		// The fire's places, for how black each point burns (the engine deck and the hatches the flames come out of).
		std::vector<V3> Fires;
		for (const Socket& S : Sockets)
		{
			if (S.Name == "engine" || S.Name.compare(0, 5, "fire.") == 0 || S.Name == "ring")
			{
				Fires.push_back(S.At);
			}
		}
		const uint64_t Seed = Mix(std::hash<std::string>()(Name) & 0xFFFFFFFFull, 0x5EED);
		const Lin Dust = FromPicture(156, 141, 112), SunFaded = FromPicture(108, 101, 76);

		// The German three colours: olive green and red-brown sprayed over the dark yellow, each in its own blotches
		// (two noises), the green a quarter of the coat and the brown a sixth, about the share photographs of
		// Normandy's Tigers and Panzer IVs show. The colours are the campaign miniatures' (Panther: 5A6240, 6E4A34).
		const uint64_t BrownSeed = Seed + 97;
		const Lin Brown = FromHex("6E4A34");
		// The thresholds: the noise's value that leaves that share of her coat's points above it.
		const auto Threshold = [&](uint64_t Of, double Cover)
		{
			std::vector<double> Values;
			for (size_t K = 0; K < Near.Count(); ++K)
			{
				if (Near.Paints[K] == Upper)
				{
					Values.push_back(Blotch(V3(Near.Points[K * 3], Near.Points[K * 3 + 1], Near.Points[K * 3 + 2]), Of));
				}
			}
			if (Values.empty())
			{
				return 0.5;
			}
			const size_t At = static_cast<size_t>(Clamp(1.0 - Cover, 0.0, 1.0) * static_cast<double>(Values.size() - 1));
			std::nth_element(Values.begin(), Values.begin() + static_cast<std::ptrdiff_t>(At), Values.end());
			return Values[At];
		};
		const double GreenAt = bCamouflage ? Threshold(Seed, 0.26) : 0.5;
		const double BrownAt = bCamouflage ? Threshold(BrownSeed, 0.17) : 0.5;

		// Each level of detail painted and cut into the bodies.
		for (int Level = 0; Level < 2; ++Level)
		{
			const RawMesh& Raw = Level == 0 ? Near : Far;
			const size_t N = Raw.Count();
			bool bFileBlend = false;
			for (int B : Raw.Blend)
			{
				if (B > 0)
				{
					bFileBlend = true;
					break;
				}
			}
			const std::vector<float> Edge = Edges(Raw);
			std::vector<uint8_t> Colour(N * 4, 255);
			std::vector<float> Burn(N, 0.0f);
			for (size_t K = 0; K < N; ++K)
			{
				const int PaintK = std::max(0, std::min(Raw.Paints[K], static_cast<int>(Paint.size()) - 1));
				const int PartK = Raw.Parts[K];
				const V3 At(Raw.Points[K * 3], Raw.Points[K * 3 + 1], Raw.Points[K * 3 + 2]);
				const V3 Up(Raw.Facings[K * 3], Raw.Facings[K * 3 + 1], Raw.Facings[K * 3 + 2]);
				// A spinning part's blotches go round with it, and every wheel of a side wears the same ones (so a road
				// wheel is one mesh drawn many times): their noise is read in the part's own place.
				const Part& P = Parts[static_cast<size_t>(PartK)];
				const bool bSpins = P.Kind == PartKind::Spin;
				const V3 Here = bSpins ? At - P.Pivot + V3(0.0, P.Side * 5.0, 0.0) : At;
				Lin C = Paint[static_cast<size_t>(PaintK)];
				const bool bCoat = PaintK == Upper || PaintK == Upper2 || PaintK == Shade;
				if (PaintK == Upper)
				{
					Lin First = C;
					if (bAmerican && !bFileDust)
					{
						// (the art director's sheet: an American vehicle's olive is faded by the sun on what faces the sky,
						// paler and greyer, never greener: red stays over green)
						First = Mix(First, SunFaded, 0.65 * Smooth(0.5, 0.9, Up.Z));
					}
					if (bFileBlend)
					{
						C = Mix(First, Paint[static_cast<size_t>(Upper2)], Clamp(Raw.Blend[K] / 100.0, 0.0, 1.0));
						// (the third colour sprayed over the second where they meet: mix(mix(upper, upper2, blend), upper3, blend2))
						if (Upper3 >= 0 && Raw.Blend2[K] > 0)
						{
							C = Mix(C, Paint[static_cast<size_t>(Upper3)], Clamp(Raw.Blend2[K] / 100.0, 0.0, 1.0));
						}
					}
					else if (bCamouflage)
					{
						// (soft edged, as a spray gun leaves them; the brown over the green where they meet)
						C = Mix(First, Paint[static_cast<size_t>(Upper2)], Smooth(GreenAt - 0.03, GreenAt + 0.03, Blotch(Here, Seed)));
						C = Mix(C, Brown, Smooth(BrownAt - 0.03, BrownAt + 0.03, Blotch(Here, BrownSeed)));
					}
					else
					{
						C = First;
					}
				}
				// Worn edges: the paint chipped back toward a pale primer and bright steel along the plates' edges (a file
				// with its own dirt says where with its wear: chipped to the metal that far).
				// (captured: her maker's coat, where the captor's brush missed it, in strokes and patches, most low down
				// on the running gear and the lower hull, and at the edges where the new paint has chipped)
				const Lin Old = !bCaptured ? C : PaintK == Upper2 ? OldCoat[1] : PaintK == Shade ? OldCoat[2] : OldCoat[0];
				if (bCaptured && bCoat)
				{
					// (a hasty overpaint by brush: the big flat plates near all covered, some 12 in 100 of the old left in
					// streaks along the strokes; up to some 40 in 100 where a brush cannot reach: the running gear, the
					// lower hull behind the wheels; the art director's mend, 23 November)
					const double Low = Clamp(1.0 - At.Z / 0.75, 0.0, 1.0); // (behind the wheels; the skirts above it covered)
					// (a plate's edges are left to the chips below: a thin plate such as a skirt is all edges, and
					// its few corners would each carry a stripe of the old coat down the whole plate)
					const double Reach = std::max(Low * Low, bSpins ? 1.0 : 0.0);
					const double At0 = 0.71 - 0.17 * Reach;
					const double Streak = Noise(V3(Here.X * 0.8, Here.Y * 0.8, Here.Z * 9.0), Seed + 77);
					C = Mix(C, Old, 0.9 * Smooth(At0, At0 + 0.025, Streak));
				}
				if (bFileDust)
				{
					if (bCoat && Raw.Wear[K] > 0)
					{
						// (captured: the file's wear is the maker's own scuffs, mostly painted over by the captor; a
						// third of it shows the old coat through)
						C = Mix(C, bCaptured ? Old : Paint[static_cast<size_t>(Steel)], (bCaptured ? 0.35 : 1.0) * Clamp(Raw.Wear[K] / 100.0, 0.0, 1.0));
					}
				}
				else if (bCoat && Edge[K] > 0.0f)
				{
					const double Chip = Smooth(0.5, 0.78, Noise(Here * 9.0, Seed + 31));
					C = bCaptured ? Mix(C, Old, 0.25 * Chip) : Mix(C, C * 1.55 + Lin(0.012, 0.011, 0.009), 0.5 * Chip);
				}
				// Road dust, thickest low down: tracks, wheels and the lower hull of a summer Normandy.
				if (!bFileDust && (bCoat || PaintK == Tread || PaintK == Rubber || PaintK == Steel))
				{
					const double Low = Clamp(1.0 - At.Z / 1.0, 0.0, 1.0);
					C = Mix(C, Dust, 0.42 * Low * Low * (0.6 + 0.8 * Noise(Here * 3.0, Seed + 5)));
				}
				// Exhaust soot as the file lays it.
				if (Raw.SootIn[K] > 4)
				{
					C = Mix(C, Paint[static_cast<size_t>(Black)], 0.72 * std::min(Raw.SootIn[K], 100) / 100.0);
				}
				if (bFileDust)
				{
					// The file's own dirt over the finished paint: the road's dust that far to her coat's dust colour, then
					// the thin pale haze on what looks up, then the shadow in her creases (how open the point is to the sky).
					// (captured: her new coat is weeks old, not months, so half the file's road dust; the art director's
					// mend, 23 November: on a captor's green the file's pale streaks down the skirts read as the old coat)
					const double DustBy = bCaptured ? 0.5 : 1.0;
					C = Mix(C, DustColour, DustBy * Clamp(Raw.DustIn[K] / 100.0, 0.0, 1.0));
					C = Mix(C, HazeColour, DustBy * Clamp(Raw.Haze[K] / 100.0, 0.0, 1.0));
					C = C * Clamp(static_cast<double>(Raw.ShadeIn[K]), 0.0, 1.0);
				}
				Colour[K * 4] = ToPicture(C.R);
				Colour[K * 4 + 1] = ToPicture(C.G);
				Colour[K * 4 + 2] = ToPicture(C.B);
				Colour[K * 4 + 3] = 255;
				// How black she burns here: everything chars, most round the engine deck and the hatches, and what
				// faces the sky; rubber and canvas burn away to black.
				double Near2Fire = 0.0;
				for (const V3& F : Fires)
				{
					Near2Fire = std::max(Near2Fire, Smooth(2.6, 0.4, (At - F).Length()));
				}
				// (a tank that burns out keeps little of her paint anywhere: the ammunition and fuel burn for an hour)
				double B = 0.7 + 0.2 * Near2Fire + 0.1 * std::max(0.0, Up.Z);
				if (PaintK == Rubber || PaintK == Canvas || PaintK == Glass)
				{
					B = 0.95;
				}
				else if (PaintK == Tread || PaintK == Steel)
				{
					B = 0.5;
				}
				Burn[K] = static_cast<float>(Clamp(B * (0.8 + 0.4 * Noise(Here * 1.7, Seed + 77)), 0.0, 1.0));
			}

			// The cut: every point to its owner's body, in file order (a track's links one after another).
			std::vector<std::vector<uint32_t>> Members(Bodies.size());
			std::vector<std::vector<uint32_t>> ByPart(Parts.size());
			for (size_t K = 0; K < N; ++K)
			{
				const Part& P = Parts[static_cast<size_t>(Raw.Parts[K])];
				if (P.Body < 0)
				{
					continue;
				}
				if (P.bLink)
				{
					ByPart[static_cast<size_t>(Raw.Parts[K])].push_back(static_cast<uint32_t>(K));
				}
				else
				{
					Members[static_cast<size_t>(P.Body)].push_back(static_cast<uint32_t>(K));
				}
			}
			for (size_t B = 0; B < Bodies.size(); ++B)
			{
				Body& Bo = Bodies[B];
				Mesh& Out = Level == 0 ? Bo.Near : Bo.Far;
				std::vector<uint32_t>& List = Members[B];
				if (Bo.bTrack)
				{
					for (int L : Links(Bo.Side))
					{
						const std::vector<uint32_t>& Of = ByPart[static_cast<size_t>(L)];
						Out.LinkFirst.push_back(static_cast<uint32_t>(List.size()));
						Out.LinkCount.push_back(static_cast<uint32_t>(Of.size()));
						List.insert(List.end(), Of.begin(), Of.end());
					}
					if (Level == 1)
					{
						Out.LinkFirst.clear();
						Out.LinkCount.clear();
					}
				}
				Out.Points.resize(List.size() * 3);
				Out.Normals.resize(List.size() * 3);
				Out.Colours.resize(List.size() * 4);
				Out.Soot.resize(List.size());
				Out.Paints.resize(List.size());
				for (size_t J = 0; J < List.size(); ++J)
				{
					const size_t K = List[J];
					Out.Points[J * 3] = static_cast<float>(Raw.Points[K * 3] - Bo.Pivot.X);
					Out.Points[J * 3 + 1] = static_cast<float>(Raw.Points[K * 3 + 1] - Bo.Pivot.Y);
					Out.Points[J * 3 + 2] = static_cast<float>(Raw.Points[K * 3 + 2] - Bo.Pivot.Z);
					const V3 F = V3(Raw.Facings[K * 3], Raw.Facings[K * 3 + 1], Raw.Facings[K * 3 + 2]).Unit();
					Out.Normals[J * 3] = static_cast<float>(F.X);
					Out.Normals[J * 3 + 1] = static_cast<float>(F.Y);
					Out.Normals[J * 3 + 2] = static_cast<float>(F.Z);
					std::memcpy(&Out.Colours[J * 4], &Colour[K * 4], 4);
					Out.Soot[J] = Burn[K];
					Out.Paints[J] = static_cast<uint8_t>(std::clamp(Raw.Paints[K], 0, 255));
				}
			}
			std::vector<int32_t> BodyOf(N, -1), Local(N, -1);
			for (size_t B = 0; B < Bodies.size(); ++B)
			{
				const std::vector<uint32_t>& List = Members[B];
				for (size_t J = 0; J < List.size(); ++J)
				{
					BodyOf[List[J]] = static_cast<int32_t>(B);
					Local[List[J]] = static_cast<int32_t>(J);
				}
			}
			for (size_t T = 0; T + 2 < Raw.Triangles.size(); T += 3)
			{
				const uint32_t A = Raw.Triangles[T], Bv = Raw.Triangles[T + 1], C = Raw.Triangles[T + 2];
				const int32_t Of = BodyOf[A];
				if (Of < 0 || BodyOf[Bv] != Of || BodyOf[C] != Of)
				{
					continue; // a swapped-out part's (none of the files' triangles cross two bodies)
				}
				Mesh& Out = Level == 0 ? Bodies[static_cast<size_t>(Of)].Near : Bodies[static_cast<size_t>(Of)].Far;
				Out.Triangles.push_back(static_cast<uint32_t>(Local[A]));
				Out.Triangles.push_back(static_cast<uint32_t>(Local[Bv]));
				Out.Triangles.push_back(static_cast<uint32_t>(Local[C]));
			}

			// Captured kit: her captor's mark, big (the art director, 23 November: crews painted them oversize so their
			// own side would hold its fire, and the player must see whose she is at a glance): half as big again at each
			// place her file gives for her maker's marks, once on her roof and once on her hull's back plate. The Germans'
			// cross, black edged white, and the Americans' white star, as the cloud's pictures draw them; the Soviets' plain
			// red star (the tank files' own red, A82020; the art director's carry, 23 November: a captor with no mark leaves
			// the player guessing); a captor with no mark drawn yet (British, Italian), none.
			if (bCaptured && (D.Nation == "GER" || D.Nation == "USA" || D.Nation == "SOV"))
			{
				const bool bStar = D.Nation == "USA" || D.Nation == "SOV";
				const uint8_t StarR = D.Nation == "SOV" ? 168 : 214, StarG = D.Nation == "SOV" ? 32 : 211, StarB = D.Nation == "SOV" ? 32 : 196;
				auto AddMark = [&](int BodyIx, const V3& Place, const V3& Facing, double Size, const V3& UpHint) {
					if (BodyIx < 0 || static_cast<size_t>(BodyIx) >= Bodies.size()) return;
					Body& Bo = Bodies[static_cast<size_t>(BodyIx)];
					Mesh& Out = Level == 0 ? Bo.Near : Bo.Far;
					const V3 Nf = Facing.Unit(V3(1.0, 0.0, 0.0));
					V3 Upw = UpHint - Nf * Nf.Dot(UpHint);
					if (Upw.Length() < 0.3) Upw = V3(1.0, 0.0, 0.0) - Nf * Nf.X;
					Upw = Upw.Unit(V3(0.0, 0.0, 1.0));
					const V3 Across = Upw.Cross(Nf).Unit(V3(0.0, 1.0, 0.0));
					auto Point = [&](double A, double U, double Lift, uint8_t Cr, uint8_t Cg, uint8_t Cb) {
						const V3 P = Place + Nf * Lift + Across * A + Upw * U - Bo.Pivot;
						Out.Points.insert(Out.Points.end(), { static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z) });
						Out.Normals.insert(Out.Normals.end(), { static_cast<float>(Nf.X), static_cast<float>(Nf.Y), static_cast<float>(Nf.Z) });
						Out.Colours.insert(Out.Colours.end(), { Cr, Cg, Cb, 255 });
						Out.Soot.push_back(0.6f);
						return static_cast<uint32_t>(Out.Soot.size() - 1);
					};
					auto Quad = [&](double A0, double U0, double A1, double U1, double Lift, uint8_t Cr, uint8_t Cg, uint8_t Cb) {
						const uint32_t P0 = Point(A0, U0, Lift, Cr, Cg, Cb), P1 = Point(A1, U0, Lift, Cr, Cg, Cb);
						const uint32_t P2 = Point(A1, U1, Lift, Cr, Cg, Cb), P3 = Point(A0, U1, Lift, Cr, Cg, Cb);
						Out.Triangles.insert(Out.Triangles.end(), { P0, P1, P2, P0, P2, P3 }); // (counter-clockwise about her facing, as the files wind theirs)
					};
					if (bStar)
					{
						const uint32_t Mid = Point(0.0, 0.0, 0.012, StarR, StarG, StarB);
						std::vector<uint32_t> Rim;
						for (int I = 0; I < 10; ++I)
						{
							const double A = I * 36.0 * 3.14159265358979 / 180.0, Rr = I % 2 == 0 ? Size : Size * 0.382;
							Rim.push_back(Point(Rr * std::sin(A), Rr * std::cos(A), 0.012, StarR, StarG, StarB));
						}
						for (int I = 0; I < 10; ++I) Out.Triangles.insert(Out.Triangles.end(), { Mid, Rim[static_cast<size_t>((I + 1) % 10)], Rim[static_cast<size_t>(I)] });
					}
					else
					{
						const double Bar = Size * 0.36, Rim = Size * 0.09;
						Quad(-Size, -Bar, Size, Bar, 0.012, 206, 204, 194);
						Quad(-Bar, -Size, Bar, Size, 0.012, 206, 204, 194);
						Quad(-Size + Rim, -Bar + Rim, Size - Rim, Bar - Rim, 0.016, 24, 24, 24);
						Quad(-Bar + Rim, -Size + Rim, Bar - Rim, Size - Rim, 0.016, 24, 24, 24);
					}
				};
				for (const Mark& Mk : Marks)
				{
					AddMark(Parts[static_cast<size_t>(Mk.Part)].Body, Mk.At, Mk.Facing, Mk.Radius * 1.5, V3(0.0, 0.0, 1.0));
				}
				// (her roof and her back: the biggest flat plate of her turret's top (else her hull's) and of her hull's
				// back, by area, among those within half a metre of the very top or back, her files' fore being +X; only
				// where nothing of hers (any part: a hatch, a cupola, an exhaust) stands proud of the plate within the
				// mark's reach)
				auto Plate = [&](int PartIx, int Axis, double Sign, double Size, V3& OutAt) {
					if (PartIx < 0 || Parts[static_cast<size_t>(PartIx)].Body < 0) return false;
					auto Along = [Axis, Sign](const V3& P) { return Sign * (Axis == 0 ? P.X : P.Z); };
					auto Apart2 = [Axis](const V3& P, const V3& Q) {
						const double D1 = Axis == 0 ? P.Z - Q.Z : P.X - Q.X, D2 = P.Y - Q.Y;
						return D1 * D1 + D2 * D2;
					};
					struct Bit { V3 Mid; double Area, Height; };
					std::vector<Bit> Bits;
					double Top = -1e9;
					for (size_t T = 0; T + 2 < Raw.Triangles.size(); T += 3)
					{
						const uint32_t I0 = Raw.Triangles[T], I1 = Raw.Triangles[T + 1], I2 = Raw.Triangles[T + 2];
						if (Raw.Parts[I0] != PartIx || Raw.Parts[I1] != PartIx || Raw.Parts[I2] != PartIx) continue;
						const V3 A(Raw.Points[I0 * 3], Raw.Points[I0 * 3 + 1], Raw.Points[I0 * 3 + 2]);
						const V3 B(Raw.Points[I1 * 3], Raw.Points[I1 * 3 + 1], Raw.Points[I1 * 3 + 2]);
						const V3 C(Raw.Points[I2 * 3], Raw.Points[I2 * 3 + 1], Raw.Points[I2 * 3 + 2]);
						const V3 Cr = (B - A).Cross(C - A);
						const double Area = 0.5 * Cr.Length();
						if (Area < 1e-7) continue;
						const V3 Nm = Cr * (0.5 / Area);
						if (Sign * (Axis == 0 ? Nm.X : Nm.Z) < 0.8) continue;
						const V3 Mid = (A + B + C) * (1.0 / 3.0);
						Bits.push_back({ Mid, Area, Along(Mid) });
						Top = std::max(Top, Along(Mid));
					}
					Bits.erase(std::remove_if(Bits.begin(), Bits.end(), [Top](const Bit& Bt) { return Bt.Height < Top - 0.5; }), Bits.end());
					std::vector<V3> Proud;
					for (size_t K = 0; K < N; ++K)
					{
						const V3 P(Raw.Points[K * 3], Raw.Points[K * 3 + 1], Raw.Points[K * 3 + 2]);
						if (Along(P) > Top - 0.44) Proud.push_back(P);
					}
					const double Reach2 = (Size + 0.03) * (Size + 0.03);
					std::map<long, std::array<double, 4>> Bins; // (area, then the area-weighted sums of x, y, z)
					for (const Bit& Bt : Bits)
					{
						bool bClear = true;
						for (const V3& P : Proud)
						{
							if (Along(P) > Bt.Height + 0.06 && Apart2(P, Bt.Mid) < Reach2)
							{
								bClear = false;
								break;
							}
						}
						if (!bClear) continue;
						std::array<double, 4>& Bin = Bins[static_cast<long>(std::floor(Bt.Height / 0.06))];
						Bin[0] += Bt.Area;
						Bin[1] += Bt.Area * Bt.Mid.X;
						Bin[2] += Bt.Area * Bt.Mid.Y;
						Bin[3] += Bt.Area * Bt.Mid.Z;
					}
					const std::array<double, 4>* Best = nullptr;
					for (const auto& Bin : Bins)
						if (!Best || Bin.second[0] > (*Best)[0]) Best = &Bin.second;
					if (!Best || (*Best)[0] < 0.02) return false;
					OutAt = V3((*Best)[1] / (*Best)[0], (*Best)[2] / (*Best)[0], (*Best)[3] / (*Best)[0]);
					return true;
				};
				int RoofPart = PartIndex("turret");
				if (RoofPart < 0 || Parts[static_cast<size_t>(RoofPart)].Body < 0) RoofPart = PartIndex("hull");
				V3 At;
				if (Plate(RoofPart, 2, 1.0, 0.3, At)) AddMark(Parts[static_cast<size_t>(RoofPart)].Body, At, V3(0.0, 0.0, 1.0), 0.3, V3(1.0, 0.0, 0.0));
				const int BackPart = PartIndex("hull");
				if (Plate(BackPart, 0, -1.0, 0.25, At)) AddMark(Parts[static_cast<size_t>(BackPart)].Body, At, V3(-1.0, 0.0, 0.0), 0.25, V3(0.0, 0.0, 1.0));
			}
		}

		// Bodies of one shape (a side's road wheels): drawn from one mesh.
		auto Same = [](const Mesh& A, const Mesh& B)
		{
			if (A.Points.size() != B.Points.size() || A.Triangles != B.Triangles)
			{
				return false;
			}
			for (size_t I = 0; I < A.Points.size(); ++I)
			{
				if (std::fabs(A.Points[I] - B.Points[I]) > 1e-3f)
				{
					return false;
				}
			}
			for (size_t I = 0; I < A.Colours.size(); ++I)
			{
				if (std::abs(static_cast<int>(A.Colours[I]) - static_cast<int>(B.Colours[I])) > 2)
				{
					return false;
				}
			}
			return true;
		};
		for (size_t B = 1; B < Bodies.size(); ++B)
		{
			if (Bodies[B].bTrack)
			{
				continue;
			}
			for (size_t A = 0; A < B; ++A)
			{
				if (Bodies[A].SameAs < 0 && !Bodies[A].bTrack && Same(Bodies[A].Near, Bodies[B].Near) && Same(Bodies[A].Far, Bodies[B].Far))
				{
					Bodies[B].SameAs = static_cast<int>(A);
					break;
				}
			}
		}

		// Where she stands: the footprint of her tracks or wheels.
		const int TrackPart[2] = { PartIndex("track_l"), PartIndex("track_r") };
		if (TrackBody[0] >= 0 && TrackBody[1] >= 0)
		{
			double Front[2] = { -1e9, -1e9 }, Rear[2] = { 1e9, 1e9 }, Y[2] = { 0.0, 0.0 };
			for (size_t K = 0; K < Near.Count(); ++K)
			{
				const Part& P = Parts[static_cast<size_t>(Near.Parts[K])];
				if (!P.bLink || Near.Points[K * 3 + 2] > 0.03f)
				{
					continue;
				}
				const int S = P.Side < 0 ? 0 : 1;
				Front[S] = std::max(Front[S], static_cast<double>(Near.Points[K * 3]));
				Rear[S] = std::min(Rear[S], static_cast<double>(Near.Points[K * 3]));
			}
			for (int S = 0; S < 2; ++S)
			{
				Y[S] = TrackPart[S] >= 0 ? Parts[static_cast<size_t>(TrackPart[S])].Pivot.Y : (S == 0 ? -1.0 : 1.0);
				if (Front[S] < Rear[S])
				{
					Front[S] = Long * 0.33;
					Rear[S] = -Long * 0.33;
				}
			}
			Foot[0] = V3(Front[0], Y[0], 0.0);
			Foot[1] = V3(Front[1], Y[1], 0.0);
			Foot[2] = V3(Rear[0], Y[0], 0.0);
			Foot[3] = V3(Rear[1], Y[1], 0.0);
			TrackHalf = 0.5 * std::fabs(Y[1] - Y[0]);
		}
		else
		{
			// Wheels on the ground (their lowest point at the ground), and a gun's spades. (Not a wheel her swaps leave off: a
			// solo motorcycle's sidecar wheel.)
			std::vector<V3> Contacts;
			for (int P : Wheels)
			{
				const Part& W = Parts[static_cast<size_t>(P)];
				if (W.bShown && W.Pivot.Z - W.Radius < 0.05)
				{
					Contacts.emplace_back(W.Pivot.X, W.Pivot.Y, 0.0);
				}
			}
			double FrontX = -1e9, RearX = 1e9;
			for (const V3& C : Contacts)
			{
				FrontX = std::max(FrontX, C.X);
				RearX = std::min(RearX, C.X);
			}
			const int SpadeL = SocketIndex("spade.left"), SpadeR = SocketIndex("spade.right");
			double HalfY = 0.0;
			for (const V3& C : Contacts) HalfY = std::max(HalfY, std::fabs(C.Y));
			if (HalfY <= 0.0) HalfY = Wide * 0.4;
			TrackHalf = HalfY;
			if (SpadeL >= 0 && SpadeR >= 0)
			{
				Foot[0] = V3(FrontX > -1e8 ? FrontX : 0.0, -HalfY, 0.0);
				Foot[1] = V3(FrontX > -1e8 ? FrontX : 0.0, HalfY, 0.0);
				Foot[2] = V3(Sockets[static_cast<size_t>(SpadeL)].At.X, Sockets[static_cast<size_t>(SpadeL)].At.Y, 0.0);
				Foot[3] = V3(Sockets[static_cast<size_t>(SpadeR)].At.X, Sockets[static_cast<size_t>(SpadeR)].At.Y, 0.0);
			}
			else
			{
				if (FrontX < RearX + 0.5)
				{
					FrontX = Long * 0.3;
					RearX = -Long * 0.3;
				}
				Foot[0] = V3(FrontX, -HalfY, 0.0);
				Foot[1] = V3(FrontX, HalfY, 0.0);
				Foot[2] = V3(RearX, -HalfY, 0.0);
				Foot[3] = V3(RearX, HalfY, 0.0);
			}
		}

		// The boxes a scar is cast into: the hull with its running gear (not the turret, the aerial or a gun's barrel
		// beyond her), and the turret alone.
		HullLo = V3(1e9, 1e9, 1e9);
		HullHi = V3(-1e9, -1e9, -1e9);
		TurretLo = HullLo;
		TurretHi = HullHi;
		for (size_t K = 0; K < Near.Count(); ++K)
		{
			const int P = Near.Parts[K];
			const Part& Pa = Parts[static_cast<size_t>(P)];
			if (Pa.Body < 0 || Pa.Name == "aerial")
			{
				continue;
			}
			const V3 At(Near.Points[K * 3], Near.Points[K * 3 + 1], Near.Points[K * 3 + 2]);
			const bool bOnTurret = Turret >= 0 && Under(P, Turret);
			if (bOnTurret && Owner[static_cast<size_t>(P)] != Turret)
			{
				continue;
			}
			V3& Lo = bOnTurret ? TurretLo : HullLo;
			V3& Hi = bOnTurret ? TurretHi : HullHi;
			Lo = V3(std::min(Lo.X, At.X), std::min(Lo.Y, At.Y), std::min(Lo.Z, At.Z));
			Hi = V3(std::max(Hi.X, At.X), std::max(Hi.Y, At.Y), std::max(Hi.Z, At.Z));
		}
		if (TurretLo.X > TurretHi.X)
		{
			TurretLo = HullLo;
			TurretHi = HullHi;
		}
		if (Bodies.empty())
		{
			OutError = "a model with no bodies";
			return false;
		}
		// Each body's near triangles boxed a run at a time, for the rounds cast at her (Vehicle::Cast): a live battle
		// casts each round as it comes, and most runs are far off its line.
		for (Body& Bo : Bodies)
		{
			const Mesh& Me = Bo.Near;
			const size_t Count = Me.Triangles.size() / 3;
			Bo.Runs.assign(((Count + TriangleRun - 1) / TriangleRun) * 6, 0.0f);
			for (size_t Run = 0; Run * TriangleRun < Count; ++Run)
			{
				float* Box = &Bo.Runs[Run * 6];
				Box[0] = Box[1] = Box[2] = 1e30f;
				Box[3] = Box[4] = Box[5] = -1e30f;
				for (size_t K = Run * TriangleRun * 3; K < std::min(Count, (Run + 1) * TriangleRun) * 3; ++K)
				{
					const float* P = &Me.Points[static_cast<size_t>(Me.Triangles[K]) * 3];
					for (int A = 0; A < 3; ++A)
					{
						Box[A] = std::min(Box[A], P[A]);
						Box[A + 3] = std::max(Box[A + 3], P[A]);
					}
				}
			}
		}
		return true;
	}

	void Model::Play(const Clip& C, double Time, double Share, std::vector<PartState>& S, double Turned) const
	{
		if (Share <= 0.0)
		{
			return;
		}
		double T = Time;
		if (C.bLoop)
		{
			T = std::fmod(T, C.Long);
			if (T < 0.0)
			{
				T += C.Long;
			}
		}
		else
		{
			T = Clamp(T, 0.0, C.Long);
		}
		const Quat Spin = Turned != 0.0 ? Quat::Axis(V3(0.0, 0.0, 1.0), Turned * Rad) : Quat();
		for (const ClipTrack& K : C.Tracks)
		{
			const size_t P = static_cast<size_t>(K.Part);
			if (P >= S.size() || Parts[P].bLink)
			{
				continue;
			}
			PartState& St = S[P];
			if (!K.RotT.empty())
			{
				Quat Q = SampleRot(K, T).Scaled(Share);
				if (Turned != 0.0)
				{
					Q = Spin * Q * Spin.Conj();
				}
				St.Rot = (Q * St.Rot).Unit();
			}
			if (!K.MoveT.empty())
			{
				double M[3] = { 0.0, 0.0, 0.0 };
				Sample(K.MoveT, K.Move, 3, T, M);
				V3 Mv = V3(M[0], M[1], M[2]) * Share;
				if (Turned != 0.0)
				{
					Mv = Spin.Rotate(Mv);
				}
				St.Move += Mv;
			}
			if (!K.TurnT.empty())
			{
				double V = 0.0;
				Sample(K.TurnT, K.Turn, 1, T, &V);
				St.Turn += V * Share;
			}
			if (!K.SlideT.empty())
			{
				double V = 0.0;
				Sample(K.SlideT, K.Slide, 1, T, &V);
				St.Slide += V * Share;
			}
		}
	}

	void Model::Solve(const Xf& RootXf, const std::vector<PartState>& S, std::vector<Xf>& OutParts) const
	{
		OutParts.resize(Parts.size());
		for (size_t I = 0; I < Parts.size(); ++I)
		{
			const Part& P = Parts[I];
			if (P.bLink)
			{
				OutParts[I] = OutParts[static_cast<size_t>(P.Parent)];
				continue;
			}
			const PartState& St = S[I];
			Quat A;
			if ((P.Kind == PartKind::Turn || P.Kind == PartKind::Spin) && St.Turn != 0.0)
			{
				A = Quat::Axis(P.Axis, St.Turn * Rad);
			}
			Xf Local;
			Local.Q = (St.Rot * A).Unit();
			Local.T = P.Pivot + St.Move + P.SlideWay * St.Slide - Local.Q.Rotate(P.Pivot);
			OutParts[I] = P.Parent < 0 ? RootXf * Local : OutParts[static_cast<size_t>(P.Parent)] * Local;
		}
	}

	void Model::Places(const std::vector<Xf>& PartXf, std::vector<Place>& Out) const
	{
		Out.resize(Bodies.size());
		for (size_t B = 0; B < Bodies.size(); ++B)
		{
			const Xf& X = PartXf[static_cast<size_t>(Bodies[B].Part)];
			Out[B].At = Swap(X.Apply(Bodies[B].Pivot));
			Out[B].Q = X.Q;
		}
	}

	void Model::PoseLinks(int Side, double Run, float* Out) const
	{
		const int B = TrackBody[Side < 0 ? 0 : 1];
		const Clip* C = FindClip(Side < 0 ? "tracks.left" : "tracks.right");
		if (B < 0 || !Out)
		{
			return;
		}
		const Body& Bo = Bodies[static_cast<size_t>(B)];
		const Mesh& Me = Bo.Near;
		const std::vector<int>& Of = Links(Side);
		const double Stride = C && C->Stride > 1e-6 ? C->Stride : 0.15;
		double Phase = Run / Stride;
		Phase -= std::floor(Phase);
		const double T = C ? Phase * C->Long : 0.0;
		for (size_t L = 0; L < Of.size() && L < Me.LinkFirst.size(); ++L)
		{
			const Part& P = Parts[static_cast<size_t>(Of[L])];
			Quat Q;
			V3 Mv;
			const int K = LinkTrack[static_cast<size_t>(Of[L])];
			if (C && K >= 0)
			{
				const ClipTrack& Tr = C->Tracks[static_cast<size_t>(K)];
				if (!Tr.RotT.empty())
				{
					Q = SampleRot(Tr, T);
				}
				if (!Tr.MoveT.empty())
				{
					double M[3] = { 0.0, 0.0, 0.0 };
					Sample(Tr.MoveT, Tr.Move, 3, T, M);
					Mv = V3(M[0], M[1], M[2]);
				}
			}
			// x' = pivot + move + Q (x - pivot), every point of the link, kept from the track's pivot: as a matrix.
			const double XX = Q.X * Q.X, YY = Q.Y * Q.Y, ZZ = Q.Z * Q.Z, XY = Q.X * Q.Y, XZ = Q.X * Q.Z, YZ = Q.Y * Q.Z;
			const double WX = Q.W * Q.X, WY = Q.W * Q.Y, WZ = Q.W * Q.Z;
			const float M00 = static_cast<float>(1 - 2 * (YY + ZZ)), M01 = static_cast<float>(2 * (XY - WZ)), M02 = static_cast<float>(2 * (XZ + WY));
			const float M10 = static_cast<float>(2 * (XY + WZ)), M11 = static_cast<float>(1 - 2 * (XX + ZZ)), M12 = static_cast<float>(2 * (YZ - WX));
			const float M20 = static_cast<float>(2 * (XZ - WY)), M21 = static_cast<float>(2 * (YZ + WX)), M22 = static_cast<float>(1 - 2 * (XX + YY));
			const V3 Rel = P.Pivot - Bo.Pivot; // the link's pivot from the track's
			const float PX = static_cast<float>(Rel.X), PY = static_cast<float>(Rel.Y), PZ = static_cast<float>(Rel.Z);
			const float TX = static_cast<float>(Rel.X + Mv.X), TY = static_cast<float>(Rel.Y + Mv.Y), TZ = static_cast<float>(Rel.Z + Mv.Z);
			const size_t First = Me.LinkFirst[L], Count = Me.LinkCount[L];
			const float* In = Me.Points.data() + First * 3;
			float* To = Out + First * 3;
			for (size_t J = 0; J < Count; ++J)
			{
				const float X = In[J * 3] - PX, Y = In[J * 3 + 1] - PY, Z = In[J * 3 + 2] - PZ;
				To[J * 3] = TX + M00 * X + M01 * Y + M02 * Z;
				To[J * 3 + 1] = TY + M10 * X + M11 * Y + M12 * Z;
				To[J * 3 + 2] = TZ + M20 * X + M21 * Y + M22 * Z;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// One vehicle played from the recording.

	namespace
	{
		constexpr double Gravity = 9.81;

		bool EndsWith(const std::string& S, const char* Tail)
		{
			const size_t L = std::strlen(Tail);
			return S.size() >= L && S.compare(S.size() - L, L, Tail) == 0;
		}

		/** A tank gun's muzzle velocity (m/s) with its usual armour-piercing round: the drop she is laid up for. */
		double MuzzleVelocity(const std::string& ModelName)
		{
			if (ModelName.find("Tiger") != std::string::npos)
			{
				return 773.0; // 8.8 cm KwK 36, Pzgr 39
			}
			if (ModelName.find("anzer") != std::string::npos)
			{
				return 750.0; // 7.5 cm KwK 40 (L/48), Pzgr 39
			}
			if (ModelName.find("Sherman") != std::string::npos)
			{
				return 619.0; // 75 mm M3, M61
			}
			return 700.0;
		}

		/** How far up a tank gun is laid to strike at Range with the aim Rise metres above the gun: the round's drop over
		 * the range, and the slope. Degrees (a few tenths at the ranges tanks fought at in the hedgerows). */
		double DirectLay(double Range, double Rise, double Velocity)
		{
			const double R = std::max(Range, 1.0);
			return (0.5 * std::asin(Clamp(Gravity * R / (Velocity * Velocity), 0.0, 1.0)) + std::atan2(Rise, R)) / Rad;
		}

		/**
		 * How far up a howitzer is laid at Range. A battery fires the least charge that carries comfortably past the
		 * target (four fifths of the charge's greatest reach): the steepest fall of shot at the low angle. The angle in
		 * a vacuum, and an eighth more for the air's drag. Muzzle velocities about those of the 105 mm M2A1's charges 1
		 * to 7 and the leFH 18's 1 to 6 and its special charge.
		 */
		double HowitzerLay(const std::string& ModelName, double Range, double Rise)
		{
			static const double Ours[7] = { 198.0, 216.0, 238.0, 267.0, 311.0, 376.0, 472.0 };
			static const double Theirs[7] = { 200.0, 237.0, 265.0, 300.0, 342.0, 395.0, 470.0 };
			const double* Charges = ModelName.find("leFH") != std::string::npos ? Theirs : Ours;
			const double R = std::max(Range, 1.0);
			double V = Charges[6];
			for (int I = 0; I < 7; ++I)
			{
				if (Charges[I] * Charges[I] / Gravity * 0.8 >= R)
				{
					V = Charges[I];
					break;
				}
			}
			return (0.5 * std::asin(Clamp(Gravity * R / (V * V), 0.0, 1.0)) * 1.12 + std::atan2(Rise, R)) / Rad;
		}

		bool IsUnder(const Model& M, int P, int Top)
		{
			for (; P >= 0; P = M.Parts[static_cast<size_t>(P)].Parent)
			{
				if (P == Top)
				{
					return true;
				}
			}
			return false;
		}

		/** Where a ray meets a triangle (Moller and Trumbore's test): how far along Way. */
		bool RayHit(const V3& From, const V3& Way, const V3& A, const V3& B, const V3& C, double& OutT)
		{
			const V3 E1 = B - A, E2 = C - A;
			const V3 P = Way.Cross(E2);
			const double Det = E1.Dot(P);
			if (std::fabs(Det) < 1e-12)
			{
				return false;
			}
			const double Inv = 1.0 / Det;
			const V3 S = From - A;
			const double U = S.Dot(P) * Inv;
			if (U < 0.0 || U > 1.0)
			{
				return false;
			}
			const V3 Q = S.Cross(E1);
			const double V = Way.Dot(Q) * Inv;
			if (V < 0.0 || U + V > 1.0)
			{
				return false;
			}
			OutT = E2.Dot(Q) * Inv;
			return OutT > 0.0;
		}

		/** Whether a ray can meet anything in a box (lo x, y, z, hi x, y, z) nearer than Best: false only when it
		 * misses the box, a tenth of a millimetre wider all round, or meets it only beyond Best. So a run of triangles
		 * it passes by holds no hit RayHit would take (a nearer one, or the first of equals, as they come in order). */
		bool RayBox(const V3& From, const V3& Way, const float* Box, double Best)
		{
			const double O[3] = { From.X, From.Y, From.Z };
			const double D[3] = { Way.X, Way.Y, Way.Z };
			double Near = -1e300, Far = 1e300;
			for (int A = 0; A < 3; ++A)
			{
				const double Lo = static_cast<double>(Box[A]) - 1e-4, Hi = static_cast<double>(Box[A + 3]) + 1e-4;
				if (std::fabs(D[A]) < 1e-12)
				{
					if (O[A] < Lo || O[A] > Hi)
					{
						return false;
					}
					continue;
				}
				double T0 = (Lo - O[A]) / D[A], T1 = (Hi - O[A]) / D[A];
				if (T0 > T1)
				{
					std::swap(T0, T1);
				}
				Near = std::max(Near, T0);
				Far = std::min(Far, T1);
				if (Near > Far)
				{
					return false;
				}
			}
			return Far > 0.0 && Near <= Best;
		}

		/** A death clip over the pose so far: every hatch goes as far open as either opens it (a hatch already open is
		 * not shut to be thrown open again), and the gun droops no further than its stop. */
		void PlayDeath(const Model& M, const Clip& C, double Age, std::vector<PartState>& S)
		{
			std::vector<double> Open;
			Open.reserve(M.Hatches.size());
			for (int H : M.Hatches)
			{
				Open.push_back(S[static_cast<size_t>(H)].Turn);
				S[static_cast<size_t>(H)].Turn = 0.0;
			}
			// a lid's lift up its post, as its hatch: as far up as either puts it, not both added
			std::vector<std::pair<size_t, double>> Lifted;
			for (size_t P = 0; P < M.Parts.size(); ++P)
			{
				if (M.Parts[P].Kind == PartKind::Slide && M.Parts[P].Name.find("_lift") != std::string::npos)
				{
					Lifted.emplace_back(P, S[P].Slide);
					S[P].Slide = 0.0;
				}
			}
			M.Play(C, Age, 1.0, S);
			for (size_t I = 0; I < M.Hatches.size(); ++I)
			{
				double& Turn = S[static_cast<size_t>(M.Hatches[I])].Turn;
				Turn = std::max(Turn, Open[I]);
			}
			for (const auto& L : Lifted)
			{
				S[L.first].Slide = std::max(S[L.first].Slide, L.second);
			}
			if (M.Gun >= 0 && M.Parts[static_cast<size_t>(M.Gun)].bLimits)
			{
				double& Turn = S[static_cast<size_t>(M.Gun)].Turn;
				Turn = std::max(Turn, M.Parts[static_cast<size_t>(M.Gun)].Lo);
			}
		}

		/** The loop cue a clip plays at her engine (the sound of her idling or driving), or empty. */
		std::string EngineLoop(const Model& M, const char* ClipName)
		{
			if (const Clip* C = M.FindClip(ClipName))
			{
				for (const ClipEvent& E : C->Events)
				{
					if (E.Socket == "engine" && !E.Cue.empty())
					{
						return E.Cue;
					}
				}
			}
			return std::string();
		}
	}

	double Vehicle::Latest(const std::vector<double>& Times, double T)
	{
		const auto It = std::upper_bound(Times.begin(), Times.end(), T);
		return It == Times.begin() ? -1e9 : *(It - 1);
	}

	void Vehicle::PlaceAt(double T, double& OutX, double& OutY, double& OutF) const
	{
		const std::vector<double>& A = Track.At;
		const size_t N = A.size() / 4;
		OutX = OutY = OutF = 0.0;
		if (N == 0)
		{
			return;
		}
		// A wreck stays where she was struck. (The recording puts her, the instant she dies, where the killing round
		// was aimed ahead of her: 14 and 19 metres on in the Normandy dry run. She keeps the place she was seen at.)
		if (Track.Died >= 0.0 && T > Track.Died - 1e-3)
		{
			T = Track.Died - 1e-3;
		}
		if (T <= A[0] || N == 1)
		{
			OutX = A[1];
			OutY = A[2];
			OutF = A[3];
			return;
		}
		if (T >= A[(N - 1) * 4])
		{
			OutX = A[(N - 1) * 4 + 1];
			OutY = A[(N - 1) * 4 + 2];
			OutF = A[(N - 1) * 4 + 3];
			return;
		}
		size_t Lo = 0, Hi = N - 1;
		while (Hi - Lo > 1)
		{
			const size_t Mid = (Lo + Hi) / 2;
			if (A[Mid * 4] <= T)
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}
		const double Span = A[Hi * 4] - A[Lo * 4];
		const double DX = A[Hi * 4 + 1] - A[Lo * 4 + 1], DY = A[Hi * 4 + 2] - A[Lo * 4 + 2];
		// A step faster than anything on the field drives (25 m/s) is the recording putting her somewhere, not her
		// driving there: she holds her place until then.
		if (Span <= 1e-9 || std::hypot(DX, DY) > 25.0 * Span)
		{
			OutX = A[Lo * 4 + 1];
			OutY = A[Lo * 4 + 2];
			OutF = A[Lo * 4 + 3];
			return;
		}
		const double U = (T - A[Lo * 4]) / Span;
		OutX = A[Lo * 4 + 1] + DX * U;
		OutY = A[Lo * 4 + 2] + DY * U;
		OutF = Wrap(A[Lo * 4 + 3] + Wrap(A[Hi * 4 + 3] - A[Lo * 4 + 3]) * U);
	}

	Vehicle::Tick Vehicle::TickAt(double T) const
	{
		if (Ticks.empty())
		{
			return Tick();
		}
		const double Last = static_cast<double>(Ticks.size() - 1);
		const double P = Clamp(T / Step, 0.0, Last);
		const size_t I = std::min(static_cast<size_t>(P), Ticks.size() - 1);
		const size_t J = std::min(I + 1, Ticks.size() - 1);
		const double U = P - static_cast<double>(I);
		const Tick& A = Ticks[I];
		const Tick& B = Ticks[J];
		auto Straight = [U](float From, float To) { return static_cast<float>(From + (To - From) * U); };
		auto Round360 = [U](float From, float To)
		{
			return static_cast<float>(WrapDeg(From + WrapDeg(static_cast<double>(To) - From) * U));
		};
		Tick Out;
		Out.X = Straight(A.X, B.X);
		Out.Y = Straight(A.Y, B.Y);
		Out.F = static_cast<float>(Wrap(A.F + Wrap(static_cast<double>(B.F) - A.F) * U));
		Out.V = Straight(A.V, B.V);
		Out.W = Straight(A.W, B.W);
		Out.RunL = Straight(A.RunL, B.RunL);
		Out.RunR = Straight(A.RunR, B.RunR);
		Out.Turret = Round360(A.Turret, B.Turret);
		Out.Gun = Straight(A.Gun, B.Gun);
		Out.Traverse = Straight(A.Traverse, B.Traverse);
		Out.Steer = Straight(A.Steer, B.Steer);
		Out.Drive = Straight(A.Drive, B.Drive);
		return Out;
	}

	Xf Vehicle::Seat(double X, double Y, double F, double* OutPitch, double* OutRoll) const
	{
		const Model& M = *Shape;
		const double Sn = std::sin(F), Cs = std::cos(F);
		// The ground under the four corners of her footprint, and the plane that fits them best: her height, and how
		// she pitches and rolls on it.
		double A[3][3] = { { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } };
		double B[3] = { 0.0, 0.0, 0.0 };
		for (int I = 0; I < 4; ++I)
		{
			const double U = M.Foot[I].X, V = M.Foot[I].Y;
			double Z = 0.0;
			V3 Up(0.0, 0.0, 1.0);
			if (Ground)
			{
				Ground(X + U * Sn + V * Cs, Y + U * Cs - V * Sn, Z, Up);
			}
			const double Row[3] = { 1.0, U, V };
			for (int R = 0; R < 3; ++R)
			{
				for (int C = 0; C < 3; ++C)
				{
					A[R][C] += Row[R] * Row[C];
				}
				B[R] += Row[R] * Z;
			}
		}
		auto Det = [](const double Mx[3][3])
		{
			return Mx[0][0] * (Mx[1][1] * Mx[2][2] - Mx[1][2] * Mx[2][1]) - Mx[0][1] * (Mx[1][0] * Mx[2][2] - Mx[1][2] * Mx[2][0])
				+ Mx[0][2] * (Mx[1][0] * Mx[2][1] - Mx[1][1] * Mx[2][0]);
		};
		double Fit[3] = { B[0] / 4.0, 0.0, 0.0 };
		const double D = Det(A);
		if (std::fabs(D) > 1e-9)
		{
			for (int K = 0; K < 3; ++K)
			{
				double Mx[3][3];
				for (int R = 0; R < 3; ++R)
				{
					for (int C = 0; C < 3; ++C)
					{
						Mx[R][C] = C == K ? B[R] : A[R][C];
					}
				}
				Fit[K] = Det(Mx) / D;
			}
		}
		const double NoseUp = std::atan(Fit[1]), RightUp = std::atan(Fit[2]);
		if (OutPitch)
		{
			*OutPitch = NoseUp / Rad;
		}
		if (OutRoll)
		{
			*OutRoll = RightUp / Rad;
		}
		Xf Out;
		Out.Q = (Quat::Axis(V3(0.0, 0.0, 1.0), F) * Quat::Axis(V3(0.0, 1.0, 0.0), -NoseUp) * Quat::Axis(V3(1.0, 0.0, 0.0), RightUp)).Unit();
		Out.T = V3(Y, X, Fit[0]);
		return Out;
	}

	namespace
	{
		/** The passes keep their running state every this many steps (a battle second), and a pass looks at most this
		 * many steps past the one it works out (a lorry's way of going, read up to a second either side; her speed from
		 * the steps either side, smoothed over three more), and one more: how far Grow keeps clear of a change. */
		constexpr size_t ResumeEvery = 20;
		constexpr size_t LookAhead = 26;
		// Worked out again, she eases from the line the screen showed her on to the new one over a second (20 steps):
		// slower than she would be read as going (a tenth of a metre or three degrees in it).
		constexpr size_t EaseSteps = 20;

		/** The passes' working lists, kept from one vehicle to the next (working one out again allocates nothing). */
		struct PassLists
		{
			std::vector<uint8_t> bAimed, bAstern, Going;
			std::vector<double> AimX, AimY, Way, RawV, RawW, V, W;
		};
		PassLists& Lists()
		{
			thread_local PassLists Kept;
			return Kept;
		}
	}

	void Vehicle::Build(const Model& InModel, const TrackWords& InWords, const std::vector<Round>& Rounds, double InEnd,
		const GroundFn& InGround, bool bInBrewUp, int InIndex, const std::string& InSide, const std::string& InNation)
	{
		Shape = &InModel;
		Track = InWords;
		Ground = InGround;
		End = std::max(InEnd, 1.0);
		Index = InIndex;
		Side = InSide;
		Nation = InNation;
		bPinned = false;
		LastT = -1.0;
		NextEvent = 0;
		Strikes.clear();
		StrikesSeen = 0;
		FirstMove = static_cast<size_t>(-1);
		TurnFrom = 0;
		Work(0, Rounds, bInBrewUp, -1e300);

		// The pose at the battle's opening (no events yet: the first Update gives them).
		Xf First;
		Pose(0.0, PartXf, &First, &Pitch, &Roll);
		Finish(First);
	}

	void Vehicle::Grow(const TrackWords& InWords, const std::vector<Round>& Rounds, double InEnd, bool bInBrewUp)
	{
		if (!Shape || bPinned || Ticks.empty())
		{
			return;
		}
		// The first time the new words can change her passes: where her places part (from the last kept point both
		// have: a new point, or her provisional one dropped or moved, changes her way from there), her first new change
		// (an aim, going astern), her death.
		double Changed = 1e300;
		{
			const std::vector<double>& A = Track.At;
			const std::vector<double>& B = InWords.At;
			const size_t Both = std::min(A.size(), B.size()) / 4;
			size_t K = 0;
			while (K < Both && A[K * 4] == B[K * 4] && A[K * 4 + 1] == B[K * 4 + 1] && A[K * 4 + 2] == B[K * 4 + 2] && A[K * 4 + 3] == B[K * 4 + 3])
			{
				++K;
			}
			if (K * 4 < A.size() || K * 4 < B.size())
			{
				Changed = K > 0 ? A[(K - 1) * 4] : 0.0;
			}
		}
		{
			const std::vector<Change>& A = Track.Changes;
			const std::vector<Change>& B = InWords.Changes;
			auto Same = [](const Change& P, const Change& Q)
			{
				return P.T == Q.T && P.Pose == Q.Pose && P.Cover == Q.Cover && P.bAim == Q.bAim && P.AimX == Q.AimX && P.AimY == Q.AimY
					&& P.bHidden == Q.bHidden && P.bButtoned == Q.bButtoned;
			};
			size_t K = 0;
			while (K < A.size() && K < B.size() && Same(A[K], B[K]))
			{
				++K;
			}
			if (K < A.size())
			{
				Changed = std::min(Changed, A[K].T);
			}
			if (K < B.size())
			{
				Changed = std::min(Changed, B[K].T);
			}
		}
		if (InWords.Died != Track.Died)
		{
			Changed = std::min(Changed, std::min(Track.Died < 0.0 ? 1e300 : Track.Died, InWords.Died < 0.0 ? 1e300 : InWords.Died) - 0.01);
		}
		Track = InWords;
		End = std::max(InEnd, 1.0);
		// Taken up at the last battle second at least LookAhead steps before that, and before the steps that looked past
		// the end she was worked out to (what comes after them is known now).
		size_t Keep = Ticks.size() > LookAhead ? Ticks.size() - LookAhead : 0;
		if (Changed < 1e299)
		{
			const double At = std::floor(std::max(Changed, 0.0) / Step);
			Keep = std::min(Keep, At > static_cast<double>(LookAhead) ? static_cast<size_t>(At) - LookAhead : 0);
		}
		Work(Keep / ResumeEvery * ResumeEvery, Rounds, bInBrewUp, LastT >= 0.0 ? LastT : -1e300);
		if (LastT >= 0.0)
		{
			Show(LastT);
		}
		else
		{
			Xf First;
			Pose(0.0, PartXf, &First, &Pitch, &Roll);
			Finish(First);
		}
	}

	void Vehicle::Work(size_t From, const std::vector<Round>& Rounds, bool bInBrewUp, double Given)
	{
		const Model& M = *Shape;
		Died = Track.Died;
		bBrewUp = bInBrewUp && Died >= 0.0 && M.Turret >= 0 && M.FindClip("brew_up") != nullptr;
		const bool bTank = Track.Role == "tank", bGun = Track.Role == "gun", bLorry = Track.Role == "lorry";
		bTowed = false;
		// Her own offset into the looping clips, so a platoon idling side by side does not shake as one.
		Phase = 7.0 * Unit01(Mix(static_cast<uint64_t>(Track.Block), static_cast<uint64_t>(Track.K), 0x51));
		auto Alive = [this](double T) { return Died < 0.0 || T < Died; };
		auto GroundZ = [this](double PX, double PY)
		{
			double Z = 0.0;
			V3 Up(0.0, 0.0, 1.0);
			if (Ground)
			{
				Ground(PX, PY, Z, Up);
			}
			return Z;
		};
		PassLists& Li = Lists();

		// Her place every step. (Worked out again from From: the steps before it, and every pass's state as it enters
		// it, are as the last time.)
		const size_t N = static_cast<size_t>(std::ceil(End / Step)) + 2;
		auto TimeOf = [this](size_t I) { return static_cast<double>(I) * Step; };
		// (the first step the screen has not shown: from Given on)
		const size_t Seen = Given > -1e299 ? static_cast<size_t>(std::max(0.0, std::floor(Given / Step) + 1.0)) : 0;
		if (From > 0)
		{
			From = std::min(From, std::min(PathX.size(), N) - 1) / ResumeEvery * ResumeEvery;
		}
		const size_t Had = PathX.size();
		PathX.resize(N);
		PathY.resize(N);
		PathF.resize(N);
		Resumes.resize(N / ResumeEvery + 1);
		std::vector<double>& X = PathX;
		std::vector<double>& Y = PathY;
		std::vector<double>& F = PathF;
		// The steps the screen has shown keep the place they were shown at (a tank her facing too: a lorry's and a gun's
		// are worked from the recording's below), and from there she eases onto the new line: a kept point that comes
		// late, or a provisional one, may move her line under the screen by a tenth of a metre or a few degrees.
		const size_t Hold = Given > -1e299 ? std::max(From, std::min(Seen, Had)) : From;
		double EaseX = 0.0, EaseY = 0.0, EaseF = 0.0;
		for (size_t I = From; I < N; ++I)
		{
			if (I >= Hold)
			{
				PlaceAt(TimeOf(I), X[I], Y[I], F[I]);
				continue;
			}
			double NX = 0.0, NY = 0.0, NF = 0.0;
			PlaceAt(TimeOf(I), NX, NY, NF);
			F[I] = bTank ? F[I] : NF;
			if (I + 1 == Hold)
			{
				EaseX = X[I] - NX;
				EaseY = Y[I] - NY;
				EaseF = bTank ? Wrap(F[I] - NF) : 0.0;
			}
		}
		if (Given > -1e299 && Hold == From && Hold > 0)
		{
			// (still easing from the last time, or none of the steps worked out again shown yet)
			double NX = 0.0, NY = 0.0, NF = 0.0;
			PlaceAt(TimeOf(Hold - 1), NX, NY, NF);
			EaseX = X[Hold - 1] - NX;
			EaseY = Y[Hold - 1] - NY;
			EaseF = bTank ? Wrap(F[Hold - 1] - NF) : 0.0;
		}
		for (size_t I = Hold; I < std::min(N, Hold + EaseSteps) && (EaseX != 0.0 || EaseY != 0.0 || EaseF != 0.0); ++I)
		{
			const double U = 1.0 - static_cast<double>(I - Hold + 1) / static_cast<double>(EaseSteps);
			X[I] += EaseX * U;
			Y[I] += EaseY * U;
			F[I] = Wrap(F[I] + EaseF * U);
		}
		// (a lorry's way of going about a step: read over at least a metre of road, up to a second either side; the
		// recording keeps places to a tenth of a metre, which on a short step would swing her nose by several degrees)
		auto WayAt = [&X, &Y, N](size_t I, double& DX, double& DY)
		{
			bool bWay = false;
			for (size_t Look = 5; Look <= 20 && !bWay; Look += 5)
			{
				const size_t A = I >= Look ? I - Look : 0, B = std::min(N - 1, I + Look);
				DX = X[B] - X[A];
				DY = Y[B] - Y[A];
				bWay = std::hypot(DX, DY) >= 1.0;
			}
			return bWay;
		};
		if (bLorry && From > 0 && FirstMove == static_cast<size_t>(-1) && Given < -1e299)
		{
			// (a lorry faces her first way of going from the battle's start, as she stood: one that first moves now, in a
			// battle not shown yet, is worked out again from the start)
			bool bMoves = false;
			for (size_t I = From; I < N && !bMoves; ++I)
			{
				double DX = 0.0, DY = 0.0;
				bMoves = WayAt(I, DX, DY);
			}
			if (bMoves)
			{
				for (size_t I = 0; I < From; ++I)
				{
					PlaceAt(TimeOf(I), X[I], Y[I], F[I]);
				}
				From = 0;
			}
		}

		// What she is told: the last aim she was given, held while she has none.
		std::vector<uint8_t>& bAimed = Li.bAimed;
		std::vector<uint8_t>& bAstern = Li.bAstern;
		std::vector<double>& AimX = Li.AimX;
		std::vector<double>& AimY = Li.AimY;
		bAimed.resize(N);
		bAstern.resize(N);
		AimX.resize(N);
		AimY.resize(N);
		{
			size_t C = 0;
			bool bHas = false, bBack = false;
			double AX = 0.0, AY = 0.0;
			for (size_t I = From; I < N; ++I)
			{
				while (C < Track.Changes.size() && Track.Changes[C].T <= TimeOf(I) + 1e-9)
				{
					if (Track.Changes[C].bAim)
					{
						bHas = true;
						AX = Track.Changes[C].AimX;
						AY = Track.Changes[C].AimY;
					}
					bBack = Track.Changes[C].Pose == "reverse";
					++C;
				}
				bAstern[I] = bBack ? 1 : 0;
				bAimed[I] = bHas ? 1 : 0;
				AimX[I] = AX;
				AimY[I] = AY;
			}
		}

		if (bLorry)
		{
			// A lorry turns only as she rolls (the recording swings a parked one round on the spot, as it may a tank):
			// her heading is her way of going, held while she stands, no quicker than about 70 degrees a second.
			std::vector<double>& Going = Li.Way;
			Going.resize(N);
			double Last = From > 0 ? Resumes[From / ResumeEvery].Last : F[0];
			const double Stood = Last;
			size_t First = From > 0 && FirstMove < From ? FirstMove : N;
			for (size_t I = From; I < N; ++I)
			{
				if (I % ResumeEvery == 0)
				{
					Resumes[I / ResumeEvery].Last = Last;
				}
				double DX = 0.0, DY = 0.0;
				if (WayAt(I, DX, DY))
				{
					double Way = std::atan2(DX, DY);
					if (bAstern[I])
					{
						Way = Wrap(Way + Pi); // backing: her nose stays the other way
					}
					Last = Way;
					First = std::min(First, I);
				}
				Going[I] = Last;
			}
			if (FirstMove == static_cast<size_t>(-1) && First < N && Given > -1e299)
			{
				// (her first move has come after her standing was shown: she keeps it, and turns to her way at her own
				// pace so as to face it as she sets off rolling, or from the step after the last one shown if that is
				// later; turned too soon, she would stand again a moment, and brake, before she rolls)
				size_t Off = First;
				while (Off + 1 < N && std::hypot(X[Off + 1] - X[Off], Y[Off + 1] - Y[Off]) <= 0.35 * Step)
				{
					++Off;
				}
				const double Turn = std::fabs(Wrap(Going[First] - (First > From ? Going[First - 1] : Stood)));
				const size_t Turning = static_cast<size_t>(std::ceil(Turn / (1.2 * Step)));
				TurnFrom = std::max(std::max(From, Seen), std::min(First, Off > Turning ? Off - Turning : 0));
			}
			for (size_t I = std::max(From, TurnFrom); I < First && First < N; ++I)
			{
				Going[I] = Going[First];
			}
			if (From == 0)
			{
				F[0] = Going[0];
			}
			for (size_t I = std::max<size_t>(From, 1); I < N; ++I)
			{
				F[I] = Wrap(F[I - 1] + Clamp(Wrap(Going[I] - F[I - 1]), -1.2 * Step, 1.2 * Step));
			}
			FirstMove = First < N ? First : static_cast<size_t>(-1);
		}

		const Part* TraversePart = M.Traverse >= 0 ? &M.Parts[static_cast<size_t>(M.Traverse)] : nullptr;
		const double TraverseLimit = TraversePart && TraversePart->bLimits ? std::min(std::fabs(TraversePart->Lo), std::fabs(TraversePart->Hi)) : 0.0;
		if (bGun)
		{
			// A gun's carriage stays as it was dug in, its barrel laid round on the traverse; only when the aim goes past
			// the traverse do the crew lift the trail and swing her round (about 8 degrees a second). The recording's
			// small changes of a deployed gun's facing are left to the traverse.
			double Carriage = From > 0 ? Resumes[From / ResumeEvery].Carriage : F[0];
			bool bShifting = From > 0 && Resumes[From / ResumeEvery].bShifting;
			const double ShiftRate = 8.0 * Rad * Step;
			for (size_t I = From; I < N; ++I)
			{
				if (I % ResumeEvery == 0)
				{
					Resumes[I / ResumeEvery].Carriage = Carriage;
					Resumes[I / ResumeEvery].bShifting = bShifting;
				}
				const bool bRolling = I > 0 && std::hypot(X[I] - X[I - 1], Y[I] - Y[I - 1]) > 0.02;
				if (bRolling)
				{
					Carriage = F[I];
					bShifting = false;
				}
				else if (Alive(TimeOf(I)))
				{
					const double Want = bAimed[I] ? std::atan2(AimX[I] - X[I], AimY[I] - Y[I]) : F[I];
					if (std::fabs(Wrap(Want - Carriage)) / Rad > std::max(TraverseLimit - 1.0, 1.0))
					{
						bShifting = true;
					}
					if (bShifting)
					{
						Carriage = Wrap(Carriage + Clamp(Wrap(Want - Carriage), -ShiftRate, ShiftRate));
						bShifting = std::fabs(Wrap(Want - Carriage)) > 2.0 * Rad;
					}
				}
				F[I] = Carriage;
			}
		}

		// Speed along her heading and rate of turn, smoothed over a third of a second.
		std::vector<double>& RawV = Li.RawV;
		std::vector<double>& RawW = Li.RawW;
		std::vector<double>& V = Li.V;
		std::vector<double>& W = Li.W;
		RawV.resize(N);
		RawW.resize(N);
		V.resize(N);
		W.resize(N);
		for (size_t I = From >= 3 ? From - 3 : 0; I < N; ++I)
		{
			const size_t A = I > 0 ? I - 1 : 0, B = std::min(N - 1, I + 1);
			RawV[I] = 0.0;
			RawW[I] = 0.0;
			if (B == A)
			{
				continue;
			}
			const double Dt = static_cast<double>(B - A) * Step;
			RawV[I] = ((X[B] - X[A]) * std::sin(F[I]) + (Y[B] - Y[A]) * std::cos(F[I])) / Dt;
			RawW[I] = Wrap(F[B] - F[A]) / Dt;
		}
		auto Soften = [N, From](const std::vector<double>& In, std::vector<double>& Out)
		{
			for (size_t I = From; I < N; ++I)
			{
				double Sum = 0.0;
				double Count = 0.0;
				for (size_t J = I >= 3 ? I - 3 : 0; J <= std::min(N - 1, I + 3); ++J)
				{
					Sum += In[J];
					Count += 1.0;
				}
				Out[I] = Sum / Count;
			}
		};
		Soften(RawV, V);
		Soften(RawW, W);
		for (size_t I = From; I < N; ++I)
		{
			if (!Alive(TimeOf(I)))
			{
				V[I] = 0.0;
				W[I] = 0.0;
			}
		}

		// When she moves off and stops (a tenth of a second of going to move off, three tenths of standing to stop, so a
		// halt of a moment in the recording does not jerk her). Each is dated back to when she began to go or to stand.
		// A turn on the spot alone must hold TurnHolds to move her off: in a live battle the reader dates a provisional
		// point at its piece's own time, so a hull wandering inside the keeper's own slack (KEEP_F, 0.05 rad) over the
		// 0.2 to 0.4 s before the next kept point reads as 0.2 rad/s, over the turning threshold below, and a vehicle
		// standing still gave an engine note, a brake and a puff of dust she never gave in the battle read whole (the
		// pre-check's find of 23 November, an M4 at 247.65 s). A real turn on the spot of under half a second is a
		// twitch nobody sees; one that lasts is still dated back to where it began.
		// (Worked out again: her moving off and stopping by Given are as the screen showed them, and so is she through
		// those steps, while the new words still count how long she has been going or standing; one that the new words
		// date by Given, too late to be shown then, comes at the first step after it.)
		std::vector<uint8_t>& Going = Li.Going;
		Going.resize(N);
		{
			const double Soonest = TimeOf(Seen);
			auto ShownOf = [Given](const std::vector<double>& L) { return static_cast<size_t>(std::upper_bound(L.begin(), L.end(), Given) - L.begin()); };
			bool bOn = false;
			double OnFor = 0.0, OffFor = 0.0, RollFor = 0.0;
			constexpr double TurnHolds = 0.45;
			size_t KeepOffs = ShownOf(MoveOffs), KeepStops = ShownOf(Stops);
			if (From > 0)
			{
				const Resume& R = Resumes[From / ResumeEvery];
				bOn = R.bOn;
				OnFor = R.OnFor;
				OffFor = R.OffFor;
				RollFor = R.RollFor;
				KeepOffs = std::max(KeepOffs, R.MoveOffs);
				KeepStops = std::max(KeepStops, R.Stops);
			}
			MoveOffs.resize(std::min(MoveOffs.size(), KeepOffs));
			Stops.resize(std::min(Stops.size(), KeepStops));
			// (a move-off took hold the step after its date, a stop five steps after)
			auto Took = [this](double When, size_t Lag) { return static_cast<size_t>(std::max(0.0, std::round(When / Step))) + Lag; };
			size_t OffsTaken = 0, StopsTaken = 0;
			for (size_t I = From; I < N; ++I)
			{
				if (I % ResumeEvery == 0)
				{
					Resume& R = Resumes[I / ResumeEvery];
					R.bOn = bOn;
					R.OnFor = OnFor;
					R.OffFor = OffFor;
					R.RollFor = RollFor;
					R.MoveOffs = MoveOffs.size();
					R.Stops = Stops.size();
				}
				const double T = TimeOf(I);
				Going[I] = 0;
				if (!Alive(T))
				{
					continue; // (knocked out for good: nothing moves her again)
				}
				const bool bTurning = !bGun && std::fabs(W[I]) > 0.12;
				const bool bRolling = std::fabs(V[I]) > 0.35;
				const bool bGo = bRolling || bTurning;
				const bool bStill = std::fabs(V[I]) < 0.15 && (bGun || std::fabs(W[I]) < 0.06);
				if (I <= Seen || I == From)
				{
					// (as shown; from the first step not shown, as her moving off and stopping so far leave her)
					bool bNow = MoveOffs.size() > Stops.size();
					if (I < Seen)
					{
						while (OffsTaken < MoveOffs.size() && Took(MoveOffs[OffsTaken], 1) <= I)
						{
							++OffsTaken;
						}
						while (StopsTaken < Stops.size() && Took(Stops[StopsTaken], 5) <= I)
						{
							++StopsTaken;
						}
						bNow = OffsTaken > StopsTaken;
					}
					if (bNow != bOn)
					{
						bOn = bNow;
						OnFor = bOn ? OnFor : 0.0;
						RollFor = bOn ? RollFor : 0.0;
						OffFor = bOn ? 0.0 : OffFor;
					}
				}
				// (RollFor: how long of this run of going she has rolled, not only turned; a run that breaks clears it)
				if (I < Seen)
				{
					OnFor = !bOn && bGo ? OnFor + Step : 0.0;
					RollFor = !bOn && bGo ? (bRolling ? RollFor + Step : RollFor) : 0.0;
					OffFor = bOn && bStill ? OffFor + Step : 0.0;
				}
				else if (!bOn)
				{
					OnFor = bGo ? OnFor + Step : 0.0;
					RollFor = bGo ? (bRolling ? RollFor + Step : RollFor) : 0.0;
					if (OnFor > 0.1 - 1e-9 && (RollFor > 0.0 || OnFor > TurnHolds - 1e-9))
					{
						bOn = true;
						OffFor = 0.0;
						MoveOffs.push_back(std::max(Soonest, T - OnFor + Step));
					}
				}
				else
				{
					OffFor = bStill ? OffFor + Step : 0.0;
					if (OffFor > 0.3 - 1e-9)
					{
						bOn = false;
						OnFor = 0.0;
						RollFor = 0.0;
						Stops.push_back(std::max(Soonest, T - OffFor + Step));
					}
				}
				Going[I] = bOn ? 1 : 0;
			}
		}

		// The front wheels' lock for her turn (a lorry): the wheelbase over the radius of the turn.
		double Wheelbase = 3.5;
		if (!M.Steer.empty())
		{
			double Front = 0.0;
			for (int P : M.Steer)
			{
				Front += M.Parts[static_cast<size_t>(P)].Pivot.X;
			}
			Front /= static_cast<double>(M.Steer.size());
			double Rear = Front;
			for (int P : M.Wheels)
			{
				Rear = std::min(Rear, M.Parts[static_cast<size_t>(P)].Pivot.X);
			}
			if (Front - Rear > 0.5)
			{
				Wheelbase = Front - Rear;
			}
		}

		// The turret, the gun and the steering, step by step, each at its own speed.
		const Part* GunPart = M.Gun >= 0 ? &M.Parts[static_cast<size_t>(M.Gun)] : nullptr;
		const double GunLo = GunPart && GunPart->bLimits ? GunPart->Lo : -10.0;
		const double GunHi = GunPart && GunPart->bLimits ? GunPart->Hi : 25.0;
		// (a crew's hands and motors are life's: on the battle's clock, Life times as quick)
		const double TurretRate = (M.TurretDps > 0.0 ? M.TurretDps : 15.0) * Step * Life;
		const double TraverseRate = (M.TraverseDps > 0.0 ? M.TraverseDps : 4.0) * Step * Life;
		const double LayRate = 4.0 * Step * Life; // an elevating handwheel turned briskly: four degrees a second
		const double Velocity0 = MuzzleVelocity(M.Name);
		double Turret = 0.0, Lay = bGun ? 2.0 : 0.0, Trav = 0.0, SteerNow = 0.0, Drive = 0.0, RunL = 0.0, RunR = 0.0;
		double Rise = 0.0, RiseX = -1e9, RiseY = -1e9, RiseAX = -1e9, RiseAY = -1e9;
		if (From > 0)
		{
			const Resume& R = Resumes[From / ResumeEvery];
			Turret = R.Turret;
			Lay = R.Lay;
			Trav = R.Trav;
			SteerNow = R.Steer;
			Drive = R.Drive;
			RunL = R.RunL;
			RunR = R.RunR;
			Rise = R.Rise;
			RiseX = R.RiseX;
			RiseY = R.RiseY;
			RiseAX = R.RiseAX;
			RiseAY = R.RiseAY;
		}
		Ticks.resize(N);
		for (size_t I = From; I < N; ++I)
		{
			if (I % ResumeEvery == 0)
			{
				Resume& R = Resumes[I / ResumeEvery];
				R.Turret = Turret;
				R.Lay = Lay;
				R.Trav = Trav;
				R.Steer = SteerNow;
				R.Drive = Drive;
				R.RunL = RunL;
				R.RunR = RunR;
				R.Rise = Rise;
				R.RiseX = RiseX;
				R.RiseY = RiseY;
				R.RiseAX = RiseAX;
				R.RiseAY = RiseAY;
			}
			const double T = TimeOf(I);
			if (Alive(T))
			{
				const bool bAim = bAimed[I] != 0;
				const double Range = bAim ? std::hypot(AimX[I] - X[I], AimY[I] - Y[I]) : 0.0;
				const double Bearing = bAim ? std::atan2(AimX[I] - X[I], AimY[I] - Y[I]) : F[I];
				if (bAim && GunPart && (std::hypot(X[I] - RiseX, Y[I] - RiseY) > 5.0 || AimX[I] != RiseAX || AimY[I] != RiseAY))
				{
					// (the ground under her and under her aim, read again only when either has moved)
					RiseX = X[I];
					RiseY = Y[I];
					RiseAX = AimX[I];
					RiseAY = AimY[I];
					Rise = GroundZ(AimX[I], AimY[I]) + 1.0 - (GroundZ(X[I], Y[I]) + GunPart->Pivot.Z);
				}
				if (bTank && M.Turret >= 0)
				{
					const double Want = bAim ? WrapDeg((Bearing - F[I]) / Rad) : 0.0;
					Turret = WrapDeg(Turret + Clamp(WrapDeg(Want - Turret), -TurretRate, TurretRate));
				}
				if (GunPart)
				{
					double Want = Lay;
					if (bTank)
					{
						Want = bAim ? DirectLay(Range, Rise, Velocity0) : 0.0;
					}
					else if (bGun)
					{
						Want = Going[I] ? 0.0 : bAim ? HowitzerLay(M.Name, Range, Rise) : 2.0;
					}
					Want = Clamp(Want, bGun ? std::max(GunLo, 0.0) : GunLo, GunHi);
					Lay += Clamp(Want - Lay, -LayRate, LayRate);
				}
				if (bGun && TraversePart)
				{
					const double Want = Going[I] || !bAim ? 0.0 : Clamp(WrapDeg((Bearing - F[I]) / Rad), -TraverseLimit, TraverseLimit);
					Trav += Clamp(Want - Trav, -TraverseRate, TraverseRate);
				}
				if (!M.Steer.empty())
				{
					double Want = SteerNow;
					// (rolling: the lock for her turn; creeping or standing, the wheels stay where the driver left them)
					if (Going[I] && std::fabs(V[I]) > 0.4)
					{
						Want = std::atan(Wheelbase * W[I] / V[I]) / Rad;
					}
					Want = Clamp(Want, -M.SteerLimit, M.SteerLimit);
					SteerNow += Clamp(Want - SteerNow, -90.0 * Step * Life, 90.0 * Step * Life);
				}
				Drive = Going[I] ? std::min(1.0, Drive + Step * Life / 0.4) : std::max(0.0, Drive - Step * Life / 0.6);
				if (I > 0)
				{
					// (a gun's wheels roll only as she is towed; the swing of her trail is not a roll)
					const double Turn = bGun ? 0.0 : W[I] * M.TrackHalf;
					RunL += (V[I] + Turn) * Step;
					RunR += (V[I] - Turn) * Step;
				}
			}
			else
			{
				Drive = 0.0;
			}
			Tick& K = Ticks[I];
			K.X = static_cast<float>(X[I]);
			K.Y = static_cast<float>(Y[I]);
			K.F = static_cast<float>(F[I]);
			K.V = static_cast<float>(V[I]);
			K.W = static_cast<float>(W[I]);
			K.RunL = static_cast<float>(RunL);
			K.RunR = static_cast<float>(RunR);
			K.Turret = static_cast<float>(Turret);
			K.Gun = static_cast<float>(Lay);
			K.Traverse = static_cast<float>(Trav);
			K.Steer = static_cast<float>(SteerNow);
			K.Drive = static_cast<float>(Drive);
		}

		// Her hatches: open while she is not buttoned up, shut when she is (the commander's, as the open_up clip has it).
		Hatch.clear();
		HatchOpens.clear();
		if (bTank && !M.Hatches.empty())
		{
			bool bOpen = !Track.Changes.empty() && !Track.Changes[0].bButtoned;
			Hatch.push_back(-1000.0);
			HatchOpens.push_back(bOpen);
			for (size_t C = 1; C < Track.Changes.size(); ++C)
			{
				const Change& Ch = Track.Changes[C];
				if (!Alive(Ch.T))
				{
					break;
				}
				if (!Ch.bButtoned != bOpen)
				{
					bOpen = !Ch.bButtoned;
					Hatch.push_back(Ch.T);
					HatchOpens.push_back(bOpen);
				}
			}
		}

		// Her shots: the recording's stamps, and her rounds in the list (the same, as a rule; a stamp a twentieth of a
		// second from another is the same shot).
		Shots.clear();
		{
			std::vector<double> Fired = Track.Shots;
			for (const Round& R : Rounds)
			{
				if (R.By == Track.Block && R.K == Track.K && R.J == Track.J && (R.W == "tank" || R.W == "gun"))
				{
					Fired.push_back(R.T);
				}
			}
			std::sort(Fired.begin(), Fired.end());
			for (double S : Fired)
			{
				if (Alive(S) && (Shots.empty() || S - Shots.back() > 0.05))
				{
					Shots.push_back(S);
				}
			}
		}

		// The rounds that struck her, each found on her surface. (Worked out again: only the new rounds are cast, and
		// those on her turret that its training at the strike has changed for; the casting is the dear part, and a round
		// on her hull is cast the same whatever the turret's training.)
		bool bStruck = false;
		if (StrikesSeen > Rounds.size())
		{
			Strikes.clear();
			StrikesSeen = 0;
		}
		const double Settled = From > 0 ? static_cast<double>(From - 1) * Step : -1e300; // the ticks before are as they were
		for (Strike& S : Strikes)
		{
			if (S.L < Settled)
			{
				continue;
			}
			if (!bTank || M.Turret < 0 || S.Face.empty() || S.H <= 0.65)
			{
				continue;
			}
			const double TurretAt = TickAt(S.L).Turret;
			if (TurretAt != S.TurretAt)
			{
				S.bOnSurface = Cast(S.Face, S.U, S.H, TurretAt, S.Body, S.Local, S.Normal);
				S.TurretAt = TurretAt;
				S.bPlate = false;
				bStruck = true;
			}
		}
		for (size_t K = StrikesSeen; K < Rounds.size(); ++K)
		{
			const Round& R = Rounds[K];
			if (R.On != Track.Block || R.Ok != Track.K || R.Oj != Track.J || R.R.empty() || R.R == "miss")
			{
				continue;
			}
			Strike S;
			S.N = R.N;
			S.L = R.L;
			S.W = R.W;
			S.R = R.R;
			S.Face = R.Face;
			S.U = R.U;
			S.H = R.H;
			S.bHeavy = R.W == "tank" || R.W == "gun" || R.W == "at";
			const bool bFace = !R.Face.empty();
			S.TurretAt = TickAt(R.L).Turret;
			S.bOnSurface = Cast(bFace ? R.Face : "front", bFace ? R.U : 0.0, bFace ? R.H : 0.4, S.TurretAt, S.Body, S.Local, S.Normal);
			Strikes.push_back(S);
			bStruck = true;
		}
		StrikesSeen = Rounds.size();
		int KillRound = 0;
		const Strike* Kill = nullptr;
		HeavyHits.clear();
		for (const Strike& S : Strikes)
		{
			if (S.R == "kill")
			{
				KillRound = S.N;
				Kill = &S;
			}
			else if ((S.bHeavy || S.W == "mortar") && Alive(S.L))
			{
				HeavyHits.push_back(S.L);
			}
		}
		std::sort(HeavyHits.begin(), HeavyHits.end());

		// Her scars: where a round that struck her left one, the same place. (Found again only when they, the rounds
		// that struck her or her turret's training at one of them may have changed.)
		bool bScars = From == 0 || bStruck || ScarList.size() != Track.Scars.size();
		for (size_t K = 0; K < ScarList.size() && !bScars; ++K)
		{
			const ScarWord& A = ScarList[K].Word;
			const ScarWord& B = Track.Scars[K];
			bScars = A.Face != B.Face || A.Kind != B.Kind || A.U != B.U || A.H != B.H || A.From != B.From || B.From >= Settled - 0.2;
		}
		if (bScars)
		{
			ScarList.clear();
			for (const ScarWord& Wd : Track.Scars)
			{
				Scar Sc;
				Sc.Word = Wd;
				const Strike* Same = nullptr;
				for (const Strike& S : Strikes)
				{
					if (S.Face == Wd.Face && std::fabs(S.U - Wd.U) < 1e-6 && std::fabs(S.H - Wd.H) < 1e-6 && std::fabs(S.L - Wd.From) < 0.11)
					{
						Same = &S;
						break;
					}
				}
				if (Same)
				{
					Sc.Body = Same->Body;
					Sc.Local = Same->Local;
					Sc.Normal = Same->Normal;
					Sc.bOnSurface = Same->bOnSurface;
				}
				else
				{
					Sc.bOnSurface = Cast(Wd.Face, Wd.U, Wd.H, TickAt(Wd.From).Turret, Sc.Body, Sc.Local, Sc.Normal);
				}
				ScarList.push_back(Sc);
			}
		}

		// Her end: knocked out (or brewed up), and the fire that takes hold and burns to the battle's end.
		BurnFrom = -1.0;
		BurnUntil = std::max(End, LoopsUntil);
		if (Died >= 0.0)
		{
			BurnFrom = std::min(End, Died + (bBrewUp ? BrewWait + 0.6 / Life : 1.2 / Life)); // (0.6 or 1.2 s of life after the hit, or the blast)
		}

		// What she shows the effects and the sound, worked out ahead. (Worked out again: what falls by Given, the last
		// Update's time, has been given out and stays as it was; what comes after is made anew from the new words.)
		const size_t Kept = static_cast<size_t>(std::upper_bound(Timeline.begin(), Timeline.end(), Given,
			[](double Value, const Timed& Ti) { return Value < Ti.T; }) - Timeline.begin());
		Timeline.erase(Timeline.begin() + static_cast<std::ptrdiff_t>(Kept), Timeline.end());
		NextEvent = Kept;
		double Longest = 0.0; // the latest any clip's event falls into it (life's seconds)
		for (const auto& Named : M.Clips)
		{
			for (const ClipEvent& E : Named.second.Events)
			{
				Longest = std::max(Longest, E.T);
			}
		}
		auto AddClip = [this, &M, Given, Longest](const char* ClipName, double At, double Cut, bool bLoops, double Until, int RoundN, const Strike* Of)
		{
			if (At + Longest / Life <= Given)
			{
				return; // (every event of it given out already)
			}
			const Clip* C = M.FindClip(ClipName);
			if (!C)
			{
				return;
			}
			for (const ClipEvent& E : C->Events)
			{
				const double When = At + E.T / Life; // (a clip's seconds are life's)
				if (When > Cut + 1e-9 || When > End || When <= Given)
				{
					continue;
				}
				Timed Ti;
				Ti.T = When;
				Ti.E = &E;
				Ti.Fx = E.Fx;
				Ti.Cue = E.Cue;
				Ti.bLoop = bLoops || E.bCueLoops || EndsWith(E.Fx, "_loop");
				Ti.Until = Ti.bLoop ? std::max(When, Until) : When;
				if (Of && E.Socket.empty())
				{
					Ti.Round = RoundN;
					Ti.Weapon = Of->W;
					Ti.Result = Of->R;
					Ti.Face = Of->Face;
				}
				Timeline.push_back(Ti);
			}
		};
		const double Gone = Died >= 0.0 ? Died : End;
		if (bTank || bLorry)
		{
			// The engine: started at the battle's opening, then idling and driving by turns until she dies.
			AddClip("start", 0.0, Gone, false, 0.0, 0, nullptr);
			std::vector<std::pair<double, bool>> Spans;
			Spans.emplace_back(0.0, false);
			for (double T : MoveOffs)
			{
				Spans.emplace_back(T, true);
			}
			for (double T : Stops)
			{
				Spans.emplace_back(T, false);
			}
			std::stable_sort(Spans.begin(), Spans.end(), [](const std::pair<double, bool>& A, const std::pair<double, bool>& B) { return A.first < B.first; });
			for (size_t K = 0; K < Spans.size(); ++K)
			{
				const double From0 = Spans[K].first;
				const double To = K + 1 < Spans.size() ? Spans[K + 1].first : Gone;
				if (From0 >= Gone)
				{
					break;
				}
				if (Spans[K].second)
				{
					AddClip("move_off", From0, Gone, false, 0.0, 0, nullptr);
					AddClip("drive", From0, Gone, true, To, 0, nullptr);
				}
				else
				{
					if (K > 0)
					{
						AddClip("stop", From0, Gone, false, 0.0, 0, nullptr);
					}
					// (the first idle takes over as the starter's roar dies away)
					const double IdleFrom = K == 0 ? std::min(1.0, 0.5 * To) : From0;
					AddClip("idle", IdleFrom, Gone, true, To, 0, nullptr);
				}
			}
		}
		const Clip* FireClip = M.FindClip("fire");
		for (double S : Shots)
		{
			AddClip("fire", S, std::min(Gone, S + (FireClip ? FireClip->Long : 2.5) / Life), false, 0.0, 0, nullptr);
		}
		for (size_t K = 1; K < Hatch.size(); ++K)
		{
			if (HatchOpens[K])
			{
				AddClip("open_up", Hatch[K], Gone, false, 0.0, 0, nullptr);
			}
		}
		std::string HitFx = "hit_sparks", HitCue = "shell_impact_near";
		const Clip* HitClip = M.FindClip("hit");
		if (HitClip && !HitClip->Events.empty())
		{
			const ClipEvent& E = HitClip->Events.front();
			HitFx = E.Fx;
			if (!E.Cue.empty())
			{
				HitCue = E.Cue;
			}
		}
		for (const Strike& S : Strikes)
		{
			if (S.L <= Given)
			{
				continue;
			}
			Timed Ti;
			Ti.T = S.L;
			Ti.Until = S.L;
			Ti.Round = S.N;
			Ti.Weapon = S.W;
			Ti.Result = S.R;
			Ti.Face = S.Face;
			if (S.W == "mg" || S.W == "rifle")
			{
				Ti.Fx = "rattle"; // small arms only rattle on armour: sparks and a whine
				Ti.Cue = "ricochet_01";
			}
			else if (S.W == "mortar" || S.W == "grenade")
			{
				Ti.Fx = "hit_burst"; // a bomb or a grenade bursting on her
				Ti.Cue = S.W == "mortar" ? "shell_impact_near" : "";
			}
			else
			{
				Ti.Fx = HitFx;
				Ti.Cue = HitCue;
			}
			Timeline.push_back(Ti);
		}
		if (Died >= 0.0)
		{
			if (bBrewUp && BrewWait > 0.0)
			{
				// Effects (R2): knocked out first, her crew getting out, then her ammunition going (BrewWait)
				AddClip("knocked_out", Died, Died + BrewWait, false, BurnUntil, KillRound, Kill);
				AddClip("brew_up", Died + BrewWait, End, false, BurnUntil, 0, nullptr);
			}
			else
			{
				AddClip(bBrewUp ? "brew_up" : "knocked_out", Died, End, false, BurnUntil, KillRound, Kill);
			}
			if (BurnFrom >= 0.0)
			{
				AddClip("burn", BurnFrom, End, true, BurnUntil, 0, nullptr);
			}
		}
		std::stable_sort(Timeline.begin() + static_cast<std::ptrdiff_t>(Kept), Timeline.end(), [](const Timed& A, const Timed& B) { return A.T < B.T; });
		if (bKeepPlates)
		{
			KeepPlates();
		}
	}

	bool Vehicle::Cast(const std::string& Face, double U, double H, double TurretAt, int& OutBody, V3& OutLocal, V3& OutNormal) const
	{
		const Model& M = *Shape;
		OutBody = -1;
		OutLocal = V3();
		OutNormal = V3(0.0, 0.0, 1.0);
		const bool bTank = Track.Role == "tank" && M.Turret >= 0;
		// A tank's round high on a face (the recording's h over 0.65) strikes her turret, low on it her hull.
		const bool bOnTurret = bTank && H > 0.65;
		const V3 Lo = bOnTurret ? M.TurretLo : M.HullLo, Hi = bOnTurret ? M.TurretHi : M.HullHi;
		const V3 Mid = (Lo + Hi) * 0.5, Half = (Hi - Lo) * 0.5;
		const double Top = std::max(M.HullHi.Z, M.TurretHi.Z);
		// The round's way in and the way across the face, her own axes: u runs across the face (left to right seen from
		// the front, rear to front on a side), h up it, as a share of her height.
		V3 In(-1.0, 0.0, 0.0), Across(0.0, 1.0, 0.0);
		if (Face == "rear")
		{
			In = V3(1.0, 0.0, 0.0);
			Across = V3(0.0, -1.0, 0.0);
		}
		else if (Face == "left")
		{
			In = V3(0.0, 1.0, 0.0);
			Across = V3(1.0, 0.0, 0.0);
		}
		else if (Face == "right")
		{
			In = V3(0.0, -1.0, 0.0);
			Across = V3(-1.0, 0.0, 0.0);
		}
		else if (Face == "top")
		{
			In = V3(0.0, 0.0, -1.0);
			Across = V3(0.0, 1.0, 0.0);
		}
		// On the turret the face is the hull's (the battle names faces by the hull's heading): cast in the turret's own
		// frame at rest, the round's way turned back by her training.
		const Quat Back = bOnTurret ? Quat::Axis(V3(0.0, 0.0, 1.0), -TurretAt * Rad) : Quat();
		std::vector<int> Candidates;
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			const Body& Bo = M.Bodies[B];
			const Part& P = M.Parts[static_cast<size_t>(Bo.Part)];
			const bool bUnderTurret = M.Turret >= 0 && IsUnder(M, Bo.Part, M.Turret);
			if (bUnderTurret != bOnTurret && bTank)
			{
				continue;
			}
			// (not a track, the aerial or a wheel that rolls: a scar there would run round with it)
			if (Bo.bTrack || Bo.Name == "aerial" || (P.Kind == PartKind::Spin && Track.Role != "gun"))
			{
				continue;
			}
			Candidates.push_back(static_cast<int>(B));
		}
		for (int Try = 0; Try < 6; ++Try)
		{
			const double UU = U * std::pow(0.7, Try);
			V3 From, Way;
			if (Face == "top")
			{
				const double FX = bOnTurret ? Mid.X + Clamp((H - 0.8) / 0.15, -1.0, 1.0) * Half.X * 0.7
					: Lo.X + Clamp(H / 0.65, 0.05, 0.95) * (Hi.X - Lo.X);
				From = V3(FX, Mid.Y + UU * Half.Y, Hi.Z + 1.0);
				Way = In;
			}
			else
			{
				Way = Back.Rotate(In);
				const V3 A = Back.Rotate(Across);
				const double Z = Clamp(H * Top, Lo.Z + 0.05, Hi.Z - 0.05);
				const double Reach = Half.Length() + 1.0;
				const double AcrossHalf = std::fabs(A.X) * Half.X + std::fabs(A.Y) * Half.Y;
				From = V3(Mid.X, Mid.Y, Z) - Way * Reach + A * (UU * AcrossHalf);
			}
			double Best = 1e18;
			for (int B : Candidates)
			{
				const Body& Bo = M.Bodies[static_cast<size_t>(B)];
				const Mesh& Me = Bo.Near;
				const size_t Count = Me.Triangles.size() / 3;
				const V3 FromHere = From - Bo.Pivot;
				for (size_t Run = 0; Run * TriangleRun < Count; ++Run)
				{
					// (a run whose box the round misses, or meets only beyond the nearest hit so far, is passed by)
					if (Run * 6 + 5 < Bo.Runs.size() && !RayBox(FromHere, Way, &Bo.Runs[Run * 6], Best))
					{
						continue;
					}
					for (size_t K = Run * TriangleRun * 3; K < std::min(Count, (Run + 1) * TriangleRun) * 3; K += 3)
					{
						auto Corner = [&Me, &Bo](uint32_t I)
						{
							return V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]) + Bo.Pivot;
						};
						const V3 A = Corner(Me.Triangles[K]), Bv = Corner(Me.Triangles[K + 1]), C = Corner(Me.Triangles[K + 2]);
						double T = 0.0;
						if (RayHit(From, Way, A, Bv, C, T) && T < Best)
						{
							Best = T;
							OutBody = B;
							V3 Normal = (Bv - A).Cross(C - A).Unit(-Way);
							if (Normal.Dot(Way) > 0.0)
							{
								Normal = -Normal;
							}
							OutNormal = Normal;
							OutLocal = From + Way * T - Bo.Pivot;
						}
					}
				}
			}
			if (OutBody >= 0)
			{
				return true;
			}
		}
		// Nothing there (a gap in the mesh): the model's own hit socket for that face.
		const std::string Name = bOnTurret ? "hit.turret" : "hit." + Face;
		int S = M.SocketIndex(Name);
		if (S < 0)
		{
			S = M.SocketIndex("hit.front");
		}
		if (S >= 0)
		{
			const Socket& So = M.Sockets[static_cast<size_t>(S)];
			OutBody = std::max(0, M.Parts[static_cast<size_t>(So.Part)].Body);
			OutLocal = So.At - M.Bodies[static_cast<size_t>(OutBody)].Pivot;
			OutNormal = So.Facing;
			return false;
		}
		OutBody = 0;
		OutLocal = Mid - M.Bodies[0].Pivot;
		OutNormal = -In;
		return false;
	}

	void Vehicle::Pose(double T, std::vector<Xf>& OutParts, Xf* OutRoot, double* OutPitch, double* OutRoll) const
	{
		const Model& M = *Shape;
		const Tick K = TickAt(T);
		const Xf RootXf = Seat(K.X, K.Y, K.F, OutPitch, OutRoll);
		std::vector<PartState>& S = Scratch;
		S.assign(M.Parts.size(), PartState());
		const bool bDeadNow = Died >= 0.0 && T >= Died;
		if (M.Turret >= 0)
		{
			S[static_cast<size_t>(M.Turret)].Turn = K.Turret;
		}
		if (M.Gun >= 0)
		{
			S[static_cast<size_t>(M.Gun)].Turn = K.Gun;
		}
		if (M.Traverse >= 0)
		{
			S[static_cast<size_t>(M.Traverse)].Turn = K.Traverse;
		}
		for (int P : M.Steer)
		{
			S[static_cast<size_t>(P)].Turn = K.Steer;
		}
		for (int P : M.Wheels)
		{
			const Part& Wh = M.Parts[static_cast<size_t>(P)];
			const double Run = Wh.Side < 0 ? K.RunL : K.RunR;
			S[static_cast<size_t>(P)].Turn = std::fmod(Run / std::max(Wh.Radius, 0.05), 2.0 * Pi) / Rad;
		}
		// (an age on the battle's clock: the clip's own, life's seconds, Life times as many)
		auto PlayAge = [this, &M, &S](const char* ClipName, double Age, double Turned)
		{
			if (Age < 0.0)
			{
				return;
			}
			if (const Clip* C = M.FindClip(ClipName))
			{
				if (Age * Life <= C->Long)
				{
					M.Play(*C, Age * Life, 1.0, S, Turned);
				}
			}
		};
		if (!bDeadNow)
		{
			const bool bRolling = Latest(MoveOffs, T) > Latest(Stops, T);
			if (Track.Role == "gun")
			{
				if (bRolling)
				{
					if (const Clip* C = M.FindClip("towed"))
					{
						M.Play(*C, T * Life + Phase, 1.0, S); // trails closed and lifted to the hook, the carriage tipped
					}
				}
			}
			else
			{
				if (const Clip* C = M.FindClip("idle"))
				{
					M.Play(*C, T * Life + Phase, 1.0 - K.Drive, S);
				}
				if (K.Drive > 0.0f)
				{
					if (const Clip* C = M.FindClip("drive"))
					{
						M.Play(*C, T * Life + Phase, K.Drive, S);
					}
				}
				PlayAge("start", T, 0.0);
				PlayAge("move_off", T - Latest(MoveOffs, T), 0.0);
				PlayAge("stop", T - Latest(Stops, T), 0.0);
			}
			const double Shot = T - Latest(Shots, T);
			PlayAge("fire", Shot, 0.0);
			PlayAge("fire.rock", Shot, K.Turret); // the hull rocks along the gun, wherever the turret is trained
			PlayAge("hit", T - Latest(HeavyHits, T), 0.0);
		}
		if (!Hatch.empty())
		{
			if (const Clip* Open = M.FindClip("open_up"))
			{
				const size_t J = static_cast<size_t>(std::upper_bound(Hatch.begin(), Hatch.end(), T) - Hatch.begin());
				const size_t Last = J > 0 ? J - 1 : 0;
				const double Age = (T - Hatch[Last]) * Life; // (the clip's own seconds are life's)
				M.Play(*Open, HatchOpens[Last] ? Age : std::max(0.0, Open->Long - Age), 1.0, S);
			}
		}
		if (bDeadNow)
		{
			// (Effects (R2): a brew-up that waits, BrewWait, is knocked out until her ammunition goes)
			const bool bBlownNow = bBrewUp && T >= Died + BrewWait;
			if (const Clip* C = M.FindClip(bBlownNow ? "brew_up" : "knocked_out"))
			{
				PlayDeath(M, *C, (T - Died - (bBlownNow ? BrewWait : 0.0)) * Life, S); // (life's seconds, as her death's events are planned)
			}
			if (BurnFrom >= 0.0 && T >= BurnFrom)
			{
				if (const Clip* C = M.FindClip("burn"))
				{
					M.Play(*C, (T - BurnFrom) * Life, 1.0, S);
				}
			}
		}
		M.Solve(RootXf, S, OutParts);
		if (OutRoot)
		{
			*OutRoot = RootXf;
		}
	}

	void Vehicle::PosePinned(std::vector<Xf>& OutParts, Xf* OutRoot, double* OutPitch, double* OutRoll) const
	{
		const Model& M = *Shape;
		const Pin& P = Pinned;
		const Xf RootXf = Seat(P.X, P.Y, P.Heading, OutPitch, OutRoll);
		std::vector<PartState>& S = Scratch;
		S.assign(M.Parts.size(), PartState());
		if (M.Turret >= 0)
		{
			S[static_cast<size_t>(M.Turret)].Turn = P.Turret;
		}
		if (M.Gun >= 0)
		{
			S[static_cast<size_t>(M.Gun)].Turn = P.Gun;
		}
		if (M.Traverse >= 0)
		{
			S[static_cast<size_t>(M.Traverse)].Turn = P.Traverse;
		}
		for (int I : M.Steer)
		{
			S[static_cast<size_t>(I)].Turn = P.Steer;
		}
		for (int I : M.Trails)
		{
			S[static_cast<size_t>(I)].Turn = P.Trail;
		}
		for (int I : M.Hatches)
		{
			const Part& H = M.Parts[static_cast<size_t>(I)];
			S[static_cast<size_t>(I)].Turn = Clamp(P.Hatches, 0.0, 1.0) * (H.bLimits && H.Hi > 0.0 ? H.Hi : 90.0);
		}
		if (P.bOpenUp)
		{
			if (const Clip* C = M.FindClip("open_up"))
			{
				M.Play(*C, C->Long, 1.0, S);
			}
		}
		if (P.FireAge >= 0.0)
		{
			if (const Clip* C = M.FindClip("fire"))
			{
				M.Play(*C, P.FireAge, 1.0, S);
			}
			if (const Clip* C = M.FindClip("fire.rock"))
			{
				M.Play(*C, P.FireAge, 1.0, S, P.Turret);
			}
		}
		if (P.DeathAge >= 0.0)
		{
			if (const Clip* C = M.FindClip(P.bBrewUp ? "brew_up" : "knocked_out"))
			{
				PlayDeath(M, *C, P.DeathAge, S);
			}
		}
		M.Solve(RootXf, S, OutParts);
		if (OutRoot)
		{
			*OutRoot = RootXf;
		}
	}

	void Vehicle::Finish(const Xf& RootXf)
	{
		const Model& M = *Shape;
		Root.At = Swap(RootXf.T);
		Root.Q = RootXf.Q;
		M.Places(PartXf, BodyPlaces);
		HatchOpen = 0.0;
		if (M.CommanderHatch >= 0 && static_cast<size_t>(M.CommanderHatch) < Scratch.size())
		{
			const Part& H = M.Parts[static_cast<size_t>(M.CommanderHatch)];
			HatchOpen = Clamp(Scratch[static_cast<size_t>(M.CommanderHatch)].Turn / (H.bLimits && H.Hi > 0.0 ? H.Hi : 90.0), 0.0, 1.0);
		}
		Recoil = M.Barrel >= 0 && static_cast<size_t>(M.Barrel) < Scratch.size() ? Scratch[static_cast<size_t>(M.Barrel)].Slide : 0.0;
		V3 Way;
		if (!SocketNow("dust.left", DustLeft, Way))
		{
			DustLeft = Root.At;
		}
		if (!SocketNow("dust.right", DustRight, Way))
		{
			DustRight = Root.At;
		}
	}

	void Vehicle::Update(double T, std::vector<Event>* Out)
	{
		if (!Shape)
		{
			return;
		}
		Show(T);
		if (bPinned)
		{
			return;
		}
		const Model& M = *Shape;

		// What happened since the last Update: each event at its own time, she posed then.
		if (T < LastT)
		{
			// (a seek back: nothing is replayed; play on from here)
			NextEvent = static_cast<size_t>(std::upper_bound(Timeline.begin(), Timeline.end(), T,
				[](double Value, const Timed& Ti) { return Value < Ti.T; }) - Timeline.begin());
		}
		else
		{
			std::vector<Xf> Then;
			while (NextEvent < Timeline.size() && Timeline[NextEvent].T <= T)
			{
				const Timed& Ti = Timeline[NextEvent++];
				if (!Out || Ti.T <= LastT)
				{
					continue;
				}
				Event E;
				E.T = Ti.T;
				E.Vehicle = Index;
				E.Fx = Ti.Fx;
				E.Cue = Ti.Cue;
				E.bLoop = Ti.bLoop;
				E.Until = Ti.Until;
				E.Round = Ti.Round;
				E.Weapon = Ti.Weapon;
				E.Result = Ti.Result;
				E.Face = Ti.Face;
				E.Socket = Ti.E ? Ti.E->Socket : std::string();
				E.Speed = std::fabs(TickAt(Ti.T).V);
				const std::vector<Xf>* Parts = &PartXf;
				if (std::fabs(Ti.T - T) > 1e-6)
				{
					Pose(Ti.T, Then, nullptr);
					Parts = &Then;
				}
				E.At = Swap((*Parts)[0].T);
				E.Facing = V3(0.0, 0.0, 1.0);
				bool bPlaced = false;
				if (Ti.Round > 0)
				{
					for (const Strike& S : Strikes)
					{
						if (S.N == Ti.Round && S.Body >= 0)
						{
							const Body& Bo = M.Bodies[static_cast<size_t>(S.Body)];
							const Xf& X = (*Parts)[static_cast<size_t>(Bo.Part)];
							E.At = Swap(X.Apply(Bo.Pivot + S.Local));
							E.Facing = Swap(X.Q.Rotate(S.Normal));
							bPlaced = true;
							break;
						}
					}
				}
				if (!bPlaced && !E.Socket.empty())
				{
					const int SocketNumber = M.SocketIndex(E.Socket);
					if (SocketNumber >= 0)
					{
						const Socket& So = M.Sockets[static_cast<size_t>(SocketNumber)];
						const Xf& X = (*Parts)[static_cast<size_t>(So.Part)];
						E.At = Swap(X.Apply(So.At));
						E.Facing = Swap(X.Q.Rotate(So.Facing));
					}
				}
				const uint64_t Where = std::hash<std::string>()(E.Socket.empty() && Ti.Round > 0 ? std::to_string(Ti.Round) : E.Socket);
				E.Key = static_cast<int>(Mix(static_cast<uint64_t>(Index), Where, std::hash<std::string>()(E.Fx)) & 0x7FFFFFFF);
				Out->push_back(E);
			}
		}
		LastT = T;
	}

	void Vehicle::Show(double T)
	{
		const Model& M = *Shape;
		Now = T;
		Xf RootXf;
		if (bPinned)
		{
			PosePinned(PartXf, &RootXf, &Pitch, &Roll);
			Heading = Pinned.Heading;
			TurretDeg = Pinned.Turret;
			GunDeg = Pinned.Gun;
			TraverseDeg = Pinned.Traverse;
			SteerDeg = Pinned.Steer;
			Speed = 0.0;
			Velocity = V3();
			bMoving = false;
			bDead = Pinned.DeathAge >= 0.0;
			bBurning = false;
			Load = bDead ? 0.0 : 0.1;
			Finish(RootXf);
			return;
		}
		Pose(T, PartXf, &RootXf, &Pitch, &Roll);
		const Tick K = TickAt(T);
		bDead = Died >= 0.0 && T >= Died;
		Heading = K.F;
		Speed = bDead ? 0.0 : K.V;
		Velocity = V3(std::sin(Heading) * Speed, std::cos(Heading) * Speed, 0.0);
		bMoving = !bDead && Latest(MoveOffs, T) > Latest(Stops, T);
		RunLeft = K.RunL;
		RunRight = K.RunR;
		TurretDeg = K.Turret;
		GunDeg = K.Gun;
		TraverseDeg = K.Traverse;
		SteerDeg = K.Steer;
		bBurning = BurnFrom >= 0.0 && T >= BurnFrom && T <= BurnUntil;
		Soot = BurnFrom >= 0.0 ? Clamp((T - BurnFrom) / 30.0, 0.0, 1.0) : 0.0;
		Fade = Track.Out >= 0.0 ? 1.0 - Clamp(T - Track.Out, 0.0, 1.0) : 1.0;
		// How hard her engine works (the exhaust's smoke): idling, driving by her speed, and hardest pulling away.
		Load = 0.0;
		EngineCue.clear();
		if (!bDead && Track.Role != "gun")
		{
			Load = 0.1;
			if (bMoving)
			{
				Load = 0.35 + 0.4 * std::min(1.0, std::fabs(Speed) / 8.0);
				const double Away = T - Latest(MoveOffs, T);
				if (Away < 1.5)
				{
					Load = std::max(Load, 1.0 - 0.4 * Away / 1.5);
				}
			}
			EngineCue = EngineLoop(M, bMoving ? "drive" : "idle");
		}
		Finish(RootXf);
	}

	void Vehicle::PinPose(const Model& InModel, const Pin& P, const GroundFn& InGround)
	{
		Shape = &InModel;
		Ground = InGround;
		Track = TrackWords();
		Ticks.clear();
		Timeline.clear();
		Strikes.clear();
		ScarList.clear();
		Hatch.clear();
		HatchOpens.clear();
		NextEvent = 0;
		LastT = -1.0;
		Nation = InModel.Dressed.Nation;
		Died = -1.0;
		BurnFrom = -1.0;
		bBrewUp = P.bBrewUp;
		bPinned = true;
		Pinned = P;
		Update(0.0, nullptr);
	}

	bool Vehicle::SocketNow(const std::string& Name, V3& OutAt, V3& OutFacing) const
	{
		if (!Shape || PartXf.empty())
		{
			return false;
		}
		const int I = Shape->SocketIndex(Name);
		if (I < 0)
		{
			return false;
		}
		const Socket& S = Shape->Sockets[static_cast<size_t>(I)];
		const Xf& X = PartXf[static_cast<size_t>(S.Part)];
		OutAt = Swap(X.Apply(S.At));
		OutFacing = Swap(X.Q.Rotate(S.Facing));
		return true;
	}

	bool BenchesNow(const Vehicle& V, Benches& Out)
	{
		const Model& M = V.Look();
		if (M.Kind != "carrier" || M.SocketIndex("rider.1") < 0)
		{
			return false;
		}
		Out = Benches();
		Out.Block = V.Words().Block;
		Out.K = V.Words().K;
		Out.At = V.Root.At;
		const V3 Up = V.Root.Way(V3(0.0, 0.0, 1.0)).Unit();
		const V3 Ahead = V.Root.Way(V3(1.0, 0.0, 0.0));
		Out.Forward = (Ahead - Up * Ahead.Dot(Up)).Unit(V3(std::sin(V.Heading), std::cos(V.Heading), 0.0));
		Out.Right = Out.Forward.Cross(Up);
		// (each side's seats front to back, whatever her model's numbering)
		struct One { double Along; V3 At, Way; };
		std::vector<One> Left, Right;
		static const char* const Names[10] = { "rider.1", "rider.2", "rider.3", "rider.4", "rider.5", "rider.6", "rider.7", "rider.8", "rider.9", "rider.10" };
		for (const char* Name : Names)
		{
			One S;
			if (!V.SocketNow(Name, S.At, S.Way))
			{
				continue;
			}
			const V3 D = S.At - Out.At;
			S.Along = D.Dot(Out.Forward);
			(D.Dot(Out.Right) >= 0.0 ? Right : Left).push_back(S);
		}
		const auto Front = [](const One& A, const One& B) { return A.Along > B.Along; };
		std::sort(Left.begin(), Left.end(), Front);
		std::sort(Right.begin(), Right.end(), Front);
		for (const One& S : Left)
		{
			Out.Seat[Out.Left] = S.At;
			Out.Way[Out.Left] = S.Way;
			++Out.Left;
		}
		for (const One& S : Right)
		{
			Out.Seat[Out.Left + Out.Righted] = S.At;
			Out.Way[Out.Left + Out.Righted] = S.Way;
			++Out.Righted;
		}
		return Out.Left + Out.Righted > 0;
	}

	bool BenchFor(const std::vector<Benches>& All, double X, double Y, int Block, int SeatOn, int Seat, BenchSeat& Out)
	{
		const Benches* Best = nullptr;
		double BestD = 1e18, Along = 0.0, Across = 0.0;
		int Row = -1;
		bool bRightSide = false;
		// (the rules' own word: her, and his seat)
		if (SeatOn >= 0 && Seat >= 0)
		{
			for (const Benches& B : All)
			{
				if (B.Block == Block && B.K == SeatOn)
				{
					Best = &B;
					break;
				}
			}
			if (Best)
			{
				Row = Seat / 2;
				bRightSide = Seat % 2 == 1;
				const double Dx = X - Best->At.X, Dy = Y - Best->At.Y;
				Across = Dx * Best->Right.X + Dy * Best->Right.Y;
			}
		}
		if (!Best) for (const Benches& B : All)
		{
			const double Dx = X - B.At.X, Dy = Y - B.At.Y, D = Dx * Dx + Dy * Dy;
			if (D > 36.0 || D >= BestD)
			{
				continue;
			}
			const double A = Dx * B.Forward.X + Dy * B.Forward.Y, R = Dx * B.Right.X + Dy * B.Right.Y;
			if (std::fabs(R) > 1.6 || A < -5.0 || A > 3.0)
			{
				continue;
			}
			Best = &B;
			BestD = D;
			Along = A;
			Across = R;
		}
		if (!Best)
		{
			return false;
		}
		// (by his place: the drill's seat, two abreast 0.55 m either side, rows 0.6 m apart from 1.4 m forward of her root;
		// a far block's men a row each 1.1 m from 0.5 m sit as near their rows as these give)
		if (Row < 0)
		{
			Row = std::max(0, static_cast<int>(std::lround((1.4 - Along) / 0.6)));
			bRightSide = Across >= 0.0;
		}
		const bool bRight = bRightSide ? Best->Righted > 0 : Best->Left == 0;
		const int First = bRight ? Best->Left : 0, Count = bRight ? Best->Righted : Best->Left;
		if (Row < Count)
		{
			const V3& At = Best->Seat[First + Row];
			const V3& Way = Best->Way[First + Row];
			Out.At = At;
			Out.Floor = At.Z - 0.5;
			Out.Heading = std::atan2(Way.X, Way.Y);
			Out.bBench = true;
			return true;
		}
		// (no bench left for him: kneeling in the bed's middle at her front, facing aft, a little to his own side)
		const V3& Front = Best->Seat[0];
		const double FrontAlong = (Front - Best->At).Dot(Best->Forward);
		const double Back = 0.55 * static_cast<double>(Row - Count);
		const V3 At = Best->At + Best->Forward * (FrontAlong - Back) + Best->Right * (bRightSide ? 0.22 : -0.22);
		Out.Floor = Front.Z - 0.5;
		Out.At = V3(At.X, At.Y, Out.Floor);
		Out.Heading = std::atan2(-Best->Forward.X, -Best->Forward.Y);
		Out.bBench = false;
		return true;
	}

	bool Vehicle::SocketAt(const std::string& Name, double T, V3& OutAt, V3& OutFacing) const
	{
		if (!Shape)
		{
			return false;
		}
		const int I = Shape->SocketIndex(Name);
		if (I < 0)
		{
			return false;
		}
		std::vector<Xf> Then;
		if (bPinned)
		{
			PosePinned(Then, nullptr);
		}
		else
		{
			Pose(T, Then, nullptr);
		}
		const Socket& S = Shape->Sockets[static_cast<size_t>(I)];
		const Xf& X = Then[static_cast<size_t>(S.Part)];
		OutAt = Swap(X.Apply(S.At));
		OutFacing = Swap(X.Q.Rotate(S.Facing));
		return true;
	}

	void Vehicle::ScarNow(size_t I, V3& OutAt, V3& OutNormal) const
	{
		OutAt = Root.At;
		OutNormal = V3(0.0, 0.0, 1.0);
		if (I >= ScarList.size())
		{
			return;
		}
		const Scar& S = ScarList[I];
		if (S.Body < 0 || static_cast<size_t>(S.Body) >= BodyPlaces.size())
		{
			return;
		}
		const Place& P = BodyPlaces[static_cast<size_t>(S.Body)];
		OutAt = P.Point(S.Local);
		OutNormal = P.Way(S.Normal);
	}

	bool Vehicle::StruckNow(int RoundN, V3& OutAt, V3& OutNormal) const
	{
		for (const Strike& S : Strikes)
		{
			if (S.N == RoundN && S.Body >= 0 && static_cast<size_t>(S.Body) < BodyPlaces.size())
			{
				const Place& P = BodyPlaces[static_cast<size_t>(S.Body)];
				OutAt = P.Point(S.Local);
				OutNormal = P.Way(S.Normal);
				return true;
			}
		}
		return false;
	}

	namespace
	{
		/** Effects (R2): her shown bodies' boxes on the field as posed now (lo x, y, z, hi x, y, z each, from their runs'
		 * boxes, a centimetre wider all round), so the many short rays a plate takes try only the bodies they pass. A body
		 * not shown or the aerial: an empty box; one with no runs' boxes: a box round the whole field (always tried). */
		void BodyBoxes(const Model& M, const std::vector<Place>& Places, std::vector<double>& Out)
		{
			Out.assign(M.Bodies.size() * 6, 0.0);
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				double* Box = &Out[B * 6];
				Box[0] = Box[1] = Box[2] = 1e300;
				Box[3] = Box[4] = Box[5] = -1e300;
				const Body& Bo = M.Bodies[B];
				if (B >= Places.size() || Bo.Name == "aerial" || Bo.Part < 0 || !M.Parts[static_cast<size_t>(Bo.Part)].bShown || Bo.Near.Triangles.empty())
				{
					continue;
				}
				if (Bo.Runs.size() < 6)
				{
					Box[0] = Box[1] = Box[2] = -1e300;
					Box[3] = Box[4] = Box[5] = 1e300;
					continue;
				}
				double Lo[3] = { 1e300, 1e300, 1e300 }, Hi[3] = { -1e300, -1e300, -1e300 };
				for (size_t R = 0; R + 5 < Bo.Runs.size(); R += 6)
				{
					for (int A = 0; A < 3; ++A)
					{
						Lo[A] = std::min(Lo[A], static_cast<double>(Bo.Runs[R + static_cast<size_t>(A)]));
						Hi[A] = std::max(Hi[A], static_cast<double>(Bo.Runs[R + 3 + static_cast<size_t>(A)]));
					}
				}
				for (int K = 0; K < 8; ++K)
				{
					const V3 Corner = Places[B].Point(V3((K & 1) ? Hi[0] : Lo[0], (K & 2) ? Hi[1] : Lo[1], (K & 4) ? Hi[2] : Lo[2]));
					const double C[3] = { Corner.X, Corner.Y, Corner.Z };
					for (int A = 0; A < 3; ++A)
					{
						Box[A] = std::min(Box[A], C[A] - 0.01);
						Box[A + 3] = std::max(Box[A + 3], C[A] + 0.01);
					}
				}
			}
		}

		/** Whether a ray from From along Way, within Most, meets a box on the field (lo x, y, z, hi x, y, z). */
		bool RayMeetsBox(const V3& From, const V3& Way, double Most, const double* Box)
		{
			const double O[3] = { From.X, From.Y, From.Z };
			const double D[3] = { Way.X, Way.Y, Way.Z };
			double Near = 0.0, Far = Most;
			for (int A = 0; A < 3; ++A)
			{
				if (std::fabs(D[A]) < 1e-12)
				{
					if (O[A] < Box[A] || O[A] > Box[A + 3])
					{
						return false;
					}
					continue;
				}
				double T0 = (Box[A] - O[A]) / D[A], T1 = (Box[A + 3] - O[A]) / D[A];
				if (T0 > T1)
				{
					std::swap(T0, T1);
				}
				Near = std::max(Near, T0);
				Far = std::min(Far, T1);
				if (Near > Far)
				{
					return false;
				}
			}
			return true;
		}

		/** A ray across her bodies as placed (Vehicle::RayNow), trying only the bodies whose boxes it meets when Boxes are
		 * given (BodyBoxes). */
		bool RayAcross(const Model& M, const std::vector<Place>& Places, const std::vector<double>* Boxes, const V3& From, const V3& Way, double Most, double& OutT, V3& OutNormal)
		{
			OutT = Most;
			bool bHit = false;
			for (size_t B = 0; B < M.Bodies.size() && B < Places.size(); ++B)
			{
				const Body& Bo = M.Bodies[B];
				if (Bo.Name == "aerial" || Bo.Part < 0 || !M.Parts[static_cast<size_t>(Bo.Part)].bShown)
				{
					continue;
				}
				if (Boxes && B * 6 + 5 < Boxes->size() && !RayMeetsBox(From, Way, OutT, &(*Boxes)[B * 6]))
				{
					continue;
				}
				// (the ray in the body's own axes, from its pivot, as its points and its runs' boxes are kept)
				const Place& P = Places[B];
				const Quat Back = P.Q.Conj();
				const V3 O = Back.Rotate(Swap(From - P.At)), W = Back.Rotate(Swap(Way));
				const Mesh& Me = Bo.Near;
				const size_t Count = Me.Triangles.size() / 3;
				for (size_t Run = 0; Run * TriangleRun < Count; ++Run)
				{
					if (Run * 6 + 5 < Bo.Runs.size() && !RayBox(O, W, &Bo.Runs[Run * 6], OutT))
					{
						continue;
					}
					for (size_t K = Run * TriangleRun * 3; K < std::min(Count, (Run + 1) * TriangleRun) * 3; K += 3)
					{
						auto Corner = [&Me](uint32_t I)
						{
							return V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]);
						};
						const V3 A = Corner(Me.Triangles[K]), Bv = Corner(Me.Triangles[K + 1]), C = Corner(Me.Triangles[K + 2]);
						double T = 0.0;
						if (RayHit(O, W, A, Bv, C, T) && T < OutT)
						{
							OutT = T;
							V3 Normal = (Bv - A).Cross(C - A).Unit(-W);
							if (Normal.Dot(W) > 0.0)
							{
								Normal = -Normal;
							}
							OutNormal = P.Way(Normal);
							bHit = true;
						}
					}
				}
			}
			return bHit;
		}
	}

	bool Vehicle::RayNow(const V3& From, const V3& Way, double Most, double& OutT, V3& OutNormal) const
	{
		OutT = Most;
		return Shape && RayAcross(*Shape, BodyPlaces, nullptr, From, Way, Most, OutT, OutNormal);
	}

	bool Vehicle::PlateAbout(const V3& AtIn, const V3& NormalIn, const V3& In, PlateSpot& Out) const
	{
		return PlateIn(BodyPlaces, Root.Way(V3(1.0, 0.0, 0.0)), AtIn, NormalIn, In, Out);
	}

	bool Vehicle::PlateIn(const std::vector<Place>& Places, const V3& Ahead, const V3& AtIn, const V3& NormalIn, const V3& In, PlateSpot& Out) const
	{
		const V3 Up(0.0, 0.0, 1.0);
		V3 At = AtIn, N = NormalIn.Unit(Up);
		std::vector<double> Boxes;
		if (Shape)
		{
			BodyBoxes(*Shape, Places, Boxes);
		}
		auto Ray = [&](const V3& From, const V3& Way, double Most, double& OutT, V3& OutNormal)
		{
			OutT = Most;
			return Shape && RayAcross(*Shape, Places, &Boxes, From, Way, Most, OutT, OutNormal);
		};
		// What stands in front of where the scar's cast struck her (it passes through her road wheels, sprockets, idlers
		// and tracks: a scar there would run round with them): the round strikes that.
		const double Before = 3.0;
		double T = 0.0;
		V3 Nh;
		if (In.Length() > 0.5 && Ray(At - In * Before, In, Before - 0.02, T, Nh))
		{
			At = At - In * (Before - T);
			N = Nh.Unit(N);
		}
		Out.At = At;
		Out.Normal = N;
		// How far it runs flat about the strike: a 33 by 33 lattice of points 3 cm apart over its plane (out to 0.48 m
		// each way along it). A point is on it where a round coming in its way (In; or square to it where no way is given)
		// meets her first within 3 cm of the plane, and so does a ray square into it, each on a face turned within 25
		// degrees of it (no edge, no curve away, no gap); and nothing of her within Halo of the point (the half diagonal
		// of its square of the lattice) stands in front of the plane in the round's way (out to Clear: her tracks, wheels,
		// guards, her gun) or square to it (out to Lift). So every place in the squares of the points on it is clear in
		// the round's way. Her triangles that may matter are cast onto the plane both ways once each and tried only at the
		// points under them (or within Halo of them). The room: the biggest rectangle of the lattice about the strike
		// every point of which is on it; none where the round grazes the plane (within 9 degrees of it) or the middle
		// point is not on it.
		const double Facing = 0.9, Spacing = 0.03, Reach = 0.48, Clear = Before, Slack = 0.03, Lift = 0.06;
		const double Halo = Spacing * 0.7072, Wide = Reach + Spacing + Halo;
		constexpr int Grid = 33, Middle = 16;
		const V3 Way = In.Length() > 0.5 ? In.Unit(-N) : -N;
		const double Square = -Way.Dot(N);
		const V3 A = (std::fabs(N.Z) < 0.9 ? Up.Cross(N) : Ahead - N * Ahead.Dot(N)).Unit(V3(1.0, 0.0, 0.0));
		const V3 B = N.Cross(A).Unit(Up);
		const size_t Cells = static_cast<size_t>(Grid * Grid);
		std::vector<double> HitIn(Cells, 1e300), HitSquare(Cells, 1e300), TurnedIn(Cells, -1.0), TurnedSquare(Cells, -1.0);
		std::vector<char> Barred(Cells, 0);
		// (one cast of a triangle onto the plane: its corners there (Pu, Pv) and how far past the plane each lies along
		// the cast (Ps, less in front); at each point under it, how far past the plane it lies there, the nearest within
		// the band kept with how it is turned (Turn); each point it comes within Halo of with a part from BarFrom to
		// BarTo barred)
		auto Cast = [&](const double (&Pu)[3], const double (&Pv)[3], const double (&Ps)[3], double Turn, double BandLo, double BandHi, double BarFrom, double BarTo, std::vector<double>& Hit, std::vector<double>& TurnOf)
		{
			const double SLo = std::min(Ps[0], std::min(Ps[1], Ps[2])), SHi = std::max(Ps[0], std::max(Ps[1], Ps[2]));
			const bool bExact = SHi >= BandLo && SLo <= BandHi, bBar = SHi >= BarFrom && SLo < BarTo;
			if (!bExact && !bBar)
			{
				return;
			}
			const double Pad = (bBar ? Halo : 0.0) + 1e-9;
			const int I0 = std::max(0, static_cast<int>(std::ceil((std::min(Pu[0], std::min(Pu[1], Pu[2])) - Pad) / Spacing)) + Middle);
			const int I1 = std::min(Grid - 1, static_cast<int>(std::floor((std::max(Pu[0], std::max(Pu[1], Pu[2])) + Pad) / Spacing)) + Middle);
			const int J0 = std::max(0, static_cast<int>(std::ceil((std::min(Pv[0], std::min(Pv[1], Pv[2])) - Pad) / Spacing)) + Middle);
			const int J1 = std::min(Grid - 1, static_cast<int>(std::floor((std::max(Pv[0], std::max(Pv[1], Pv[2])) + Pad) / Spacing)) + Middle);
			if (I0 > I1 || J0 > J1)
			{
				return;
			}
			const double E1u = Pu[1] - Pu[0], E1v = Pv[1] - Pv[0], E2u = Pu[2] - Pu[0], E2v = Pv[2] - Pv[0];
			const double Area = E1u * E2v - E1v * E2u;
			// (seen edge on it covers no point; only its edges may bar one)
			const bool bOpen = std::fabs(Area) > 1e-12;
			const double Gu = bOpen ? ((Ps[1] - Ps[0]) * E2v - (Ps[2] - Ps[0]) * E1v) / Area : 0.0;
			const double Gv = bOpen ? (E1u * (Ps[2] - Ps[0]) - E2u * (Ps[1] - Ps[0])) / Area : 0.0;
			const double Steep = std::sqrt(Gu * Gu + Gv * Gv);
			auto Under = [&](double Qu, double Qv, double& OutS)
			{
				if (!bOpen)
				{
					return false;
				}
				const double Du = Qu - Pu[0], Dv = Qv - Pv[0];
				const double L1 = (Du * E2v - Dv * E2u) / Area, L2 = (E1u * Dv - E1v * Du) / Area;
				if (L1 < -1e-9 || L2 < -1e-9 || L1 + L2 > 1.0 + 1e-9)
				{
					return false;
				}
				OutS = Ps[0] + L1 * (Ps[1] - Ps[0]) + L2 * (Ps[2] - Ps[0]);
				return true;
			};
			for (int I = I0; I <= I1; ++I)
			{
				for (int J = J0; J <= J1; ++J)
				{
					const double Qu = (I - Middle) * Spacing, Qv = (J - Middle) * Spacing;
					const size_t Cell = static_cast<size_t>(I * Grid + J);
					double Sq = 0.0;
					if (bExact && Under(Qu, Qv, Sq) && Sq >= BandLo && Sq <= BandHi && Sq < Hit[Cell])
					{
						Hit[Cell] = Sq;
						TurnOf[Cell] = Turn;
					}
					if (!bBar || Barred[Cell])
					{
						continue;
					}
					// (how far past the plane the part of it within Halo of the point lies, least and most: at its corners
					// within Halo, where its edges cross the circle, and the circle's points up and down its slope)
					double Least = 1e300, Most = -1e300;
					auto Take = [&](double S)
					{
						Least = std::min(Least, S);
						Most = std::max(Most, S);
					};
					for (int K = 0; K < 3; ++K)
					{
						const int K1 = (K + 1) % 3;
						const double Fu = Pu[K] - Qu, Fv = Pv[K] - Qv, Du = Pu[K1] - Pu[K], Dv = Pv[K1] - Pv[K];
						const double Qa = Du * Du + Dv * Dv, Qb = 2.0 * (Fu * Du + Fv * Dv), Qc = Fu * Fu + Fv * Fv - Halo * Halo;
						if (Qc <= 0.0)
						{
							Take(Ps[K]);
						}
						const double Disc = Qb * Qb - 4.0 * Qa * Qc;
						if (Qa < 1e-18 || Disc < 0.0)
						{
							continue;
						}
						const double Root2 = std::sqrt(Disc);
						const double Ta = std::max(0.0, (-Qb - Root2) / (2.0 * Qa)), Tb = std::min(1.0, (-Qb + Root2) / (2.0 * Qa));
						if (Ta <= Tb)
						{
							Take(Ps[K] + Ta * (Ps[K1] - Ps[K]));
							Take(Ps[K] + Tb * (Ps[K1] - Ps[K]));
						}
					}
					if (Steep > 1e-12)
					{
						for (int Sign = -1; Sign <= 1; Sign += 2)
						{
							double Sx = 0.0;
							if (Under(Qu + Sign * Halo * Gu / Steep, Qv + Sign * Halo * Gv / Steep, Sx))
							{
								Take(Sx);
							}
						}
					}
					Barred[Cell] = Most >= BarFrom && Least < BarTo ? 1 : 0;
				}
			}
		};
		if (Shape && Square > 0.15)
		{
			// (each body's triangles are cast in its own axes: the plane, its ways and the two casts turned into them. A
			// point X there lies Depth = (X - At).N out of the plane; cast in the round's way it is (X - At).AIn and
			// (X - At).BIn along the plane and -Depth / Square past it; cast square, (X - At).A, (X - At).B and -Depth)
			const V3 AIn = A + N * (Way.Dot(A) / Square), BIn = B + N * (Way.Dot(B) / Square);
			const double DepthLo = -std::max(Slack * Square, Slack), DepthHi = std::max(Clear * Square, Lift);
			for (size_t Bi = 0; Bi < Shape->Bodies.size() && Bi < Places.size(); ++Bi)
			{
				const Body& Bo = Shape->Bodies[Bi];
				if (Bo.Name == "aerial" || Bo.Part < 0 || !Shape->Parts[static_cast<size_t>(Bo.Part)].bShown || Bo.Near.Triangles.empty())
				{
					continue;
				}
				const Place& P = Places[Bi];
				const Quat Back = P.Q.Conj();
				auto Into = [&Back](const V3& W) { return Back.Rotate(Swap(W)); };
				const V3 O = Into(At - P.At), Nb = Into(N), Ab = Into(A), Bb = Into(B), AInb = Into(AIn), BInb = Into(BIn), Wb = Into(Way);
				// (a run's box is passed by when it lies wholly out of the plane's reach, or beside the lattice in either cast)
				auto Spans = [&O](const float* Box, const V3& Along, double Lo, double Hi)
				{
					const V3 Mid((Box[0] + Box[3]) * 0.5, (Box[1] + Box[4]) * 0.5, (Box[2] + Box[5]) * 0.5);
					const V3 Half((Box[3] - Box[0]) * 0.5, (Box[4] - Box[1]) * 0.5, (Box[5] - Box[2]) * 0.5);
					const double C = (Mid - O).Dot(Along), R = std::fabs(Half.X * Along.X) + std::fabs(Half.Y * Along.Y) + std::fabs(Half.Z * Along.Z);
					return C + R >= Lo && C - R <= Hi;
				};
				const Mesh& Me = Bo.Near;
				const size_t Count = Me.Triangles.size() / 3;
				for (size_t Run = 0; Run * TriangleRun < Count; ++Run)
				{
					if (Run * 6 + 5 < Bo.Runs.size())
					{
						const float* Box = &Bo.Runs[Run * 6];
						if (!Spans(Box, Nb, DepthLo, DepthHi) || !((Spans(Box, AInb, -Wide, Wide) && Spans(Box, BInb, -Wide, Wide)) || (Spans(Box, Ab, -Wide, Wide) && Spans(Box, Bb, -Wide, Wide))))
						{
							continue;
						}
					}
					for (size_t K = Run * TriangleRun * 3; K < std::min(Count, (Run + 1) * TriangleRun) * 3; K += 3)
					{
						V3 X[3];
						double UIn[3], VIn[3], SIn[3], USq[3], VSq[3], SSq[3];
						for (int C = 0; C < 3; ++C)
						{
							const uint32_t Ix = Me.Triangles[K + static_cast<size_t>(C)];
							X[C] = V3(Me.Points[Ix * 3], Me.Points[Ix * 3 + 1], Me.Points[Ix * 3 + 2]) - O;
							const double Depth = X[C].Dot(Nb);
							UIn[C] = X[C].Dot(AInb);
							VIn[C] = X[C].Dot(BInb);
							SIn[C] = -Depth / Square;
							USq[C] = X[C].Dot(Ab);
							VSq[C] = X[C].Dot(Bb);
							SSq[C] = -Depth;
						}
						// (how it is turned to the plane, as each cast meets it)
						const V3 Face = (X[1] - X[0]).Cross(X[2] - X[0]).Unit(Nb);
						const double TurnIn = (Face.Dot(Wb) > 0.0 ? -Face : Face).Dot(Nb);
						Cast(UIn, VIn, SIn, TurnIn, -Clear, Slack, -Clear, -Slack, HitIn, TurnedIn);
						Cast(USq, VSq, SSq, std::fabs(Face.Dot(Nb)), -Lift, Slack, -Lift, -Slack, HitSquare, TurnedSquare);
					}
				}
			}
		}
		auto IsOn = [&](int I, int J)
		{
			const size_t Cell = static_cast<size_t>(I * Grid + J);
			return !Barred[Cell] && HitIn[Cell] >= -Slack && HitIn[Cell] <= Slack && TurnedIn[Cell] >= Facing
				&& HitSquare[Cell] >= -Slack && HitSquare[Cell] <= Slack && TurnedSquare[Cell] >= Facing;
		};
		// (the biggest rectangle about the middle with every point on it; of equals, the one nearest the strike: for each
		// run of rows through the middle, the run of columns through it on in all of them)
		int Lo[2] = { Middle, Middle }, Hi[2] = { Middle, Middle };
		const bool bRoom = Shape && Square > 0.15 && IsOn(Middle, Middle);
		if (bRoom)
		{
			int Best = 0, BestOff = 0;
			bool Column[Grid];
			for (int I0 = Middle; I0 >= 0; --I0)
			{
				for (int J = 0; J < Grid; ++J)
				{
					Column[J] = true;
					for (int I = I0; I <= Middle && Column[J]; ++I)
					{
						Column[J] = IsOn(I, J);
					}
				}
				if (!Column[Middle])
				{
					break;
				}
				for (int I1 = Middle; I1 < Grid; ++I1)
				{
					for (int J = 0; J < Grid; ++J)
					{
						Column[J] = Column[J] && IsOn(I1, J);
					}
					if (!Column[Middle])
					{
						break;
					}
					int J0 = Middle, J1 = Middle;
					while (J0 > 0 && Column[J0 - 1])
					{
						--J0;
					}
					while (J1 < Grid - 1 && Column[J1 + 1])
					{
						++J1;
					}
					const int Count = (I1 - I0 + 1) * (J1 - J0 + 1), Off = std::abs(I0 + I1 - 2 * Middle) + std::abs(J0 + J1 - 2 * Middle);
					if (Count > Best || (Count == Best && Off < BestOff))
					{
						Best = Count;
						BestOff = Off;
						Lo[0] = I0;
						Hi[0] = I1;
						Lo[1] = J0;
						Hi[1] = J1;
					}
				}
			}
		}
		const double HA = 0.5 * (Hi[0] - Lo[0]) * Spacing, HB = 0.5 * (Hi[1] - Lo[1]) * Spacing;
		Out.Mid = At + A * ((0.5 * (Lo[0] + Hi[0]) - Middle) * Spacing) + B * ((0.5 * (Lo[1] + Hi[1]) - Middle) * Spacing);
		Out.A = A * HA;
		Out.B = B * HB;
		// Her floor under it: straight down from a hand's breadth out, the first part of her met that is not this plate
		// itself (a glacis, a roof) and lies under the strike.
		Out.Floor = -1.0e30;
		const V3 Above = At + N * 0.12;
		double Tf = 0.0;
		V3 Nf;
		if (Ray(Above, V3(0.0, 0.0, -1.0), 4.0, Tf, Nf))
		{
			const V3 Hit = Above - Up * Tf;
			const bool bSame = std::fabs((Hit - At).Dot(N)) < 0.03 && Nf.Dot(N) > Facing;
			if (!bSame && Hit.Z < At.Z - 0.01)
			{
				Out.Floor = Hit.Z;
			}
		}
		return true;
	}

	namespace
	{
		/** The way a round came in on a face (as the scar's cast took it), in her hull's own axes. */
		V3 FaceIn(const std::string& Face)
		{
			return Face == "rear" ? V3(1.0, 0.0, 0.0) : Face == "left" ? V3(0.0, 1.0, 0.0) : Face == "right" ? V3(0.0, -1.0, 0.0)
				: Face == "top" ? V3(0.0, 0.0, -1.0) : V3(-1.0, 0.0, 0.0);
		}
	}

	void Vehicle::KeepPlates()
	{
		if (!Shape)
		{
			return;
		}
		std::vector<Xf> Parts;
		std::vector<Place> Places;
		for (Strike& S : Strikes)
		{
			if (S.bPlate || S.bHeavy || (S.W != "rifle" && S.W != "mg") || S.Body < 0 || static_cast<size_t>(S.Body) >= Shape->Bodies.size())
			{
				continue;
			}
			// (she posed as she stood when it struck: the plate searched about the strike, then kept in the struck body's
			// own axes, which carry it as she drives, turns and trains her turret)
			Xf RootXf;
			Pose(S.L, Parts, &RootXf);
			Shape->Places(Parts, Places);
			if (static_cast<size_t>(S.Body) >= Places.size())
			{
				continue;
			}
			const Place& P = Places[static_cast<size_t>(S.Body)];
			const Place RootThen{ Swap(RootXf.T), RootXf.Q };
			PlateSpot Got;
			if (!PlateIn(Places, RootThen.Way(V3(1.0, 0.0, 0.0)), P.Point(S.Local), P.Way(S.Normal), RootThen.Way(FaceIn(S.Face)).Unit(V3(-1.0, 0.0, 0.0)), Got))
			{
				continue;
			}
			const Quat Back = P.Q.Conj();
			auto PointIn = [&](const V3& X) { return Back.Rotate(Swap(X - P.At)); };
			auto WayIn = [&](const V3& W) { return Back.Rotate(Swap(W)); };
			S.PlateAt = PointIn(Got.At);
			S.PlateNormal = WayIn(Got.Normal);
			S.PlateMid = PointIn(Got.Mid);
			S.PlateA = WayIn(Got.A);
			S.PlateB = WayIn(Got.B);
			S.PlateDrop = Got.Floor > -1.0e29 ? Got.At.Z - Got.Floor : -1.0;
			S.bPlate = true;
		}
	}

	bool Vehicle::PlateNow(int RoundN, PlateSpot& Out) const
	{
		for (const Strike& S : Strikes)
		{
			if (S.N == RoundN && S.Body >= 0 && static_cast<size_t>(S.Body) < BodyPlaces.size())
			{
				const Place& P = BodyPlaces[static_cast<size_t>(S.Body)];
				if (S.bPlate)
				{
					// (kept: turned by her pose now; her floor as far under the strike as it was)
					Out.At = P.Point(S.PlateAt);
					Out.Normal = P.Way(S.PlateNormal);
					Out.Mid = P.Point(S.PlateMid);
					Out.A = P.Way(S.PlateA);
					Out.B = P.Way(S.PlateB);
					Out.Floor = S.PlateDrop >= 0.0 ? Out.At.Z - S.PlateDrop : -1.0e30;
					return true;
				}
				// (not kept: her skin's point alone, no room: every round there)
				Out.At = Out.Mid = P.Point(S.Local);
				Out.Normal = P.Way(S.Normal);
				Out.A = Out.B = V3();
				Out.Floor = -1.0e30;
				return true;
			}
		}
		return false;
	}

	void Vehicle::MarksNow(std::vector<MarkPlace>& Out) const
	{
		Out.clear();
		if (!Shape || PartXf.empty())
		{
			return;
		}
		for (const Mark& K : Shape->Marks)
		{
			const Part& P = Shape->Parts[static_cast<size_t>(K.Part)];
			if (!P.bShown)
			{
				continue;
			}
			const Xf& X = PartXf[static_cast<size_t>(K.Part)];
			MarkPlace Mp;
			Mp.Tag = K.Tag;
			Mp.bOwner = K.Tag == "_" || K.Tag == Nation;
			Mp.At = Swap(X.Apply(K.At));
			Mp.Facing = Swap(X.Q.Rotate(K.Facing));
			Mp.Radius = K.Radius;
			Out.push_back(Mp);
		}
	}

	void Vehicle::PoseLinks(int TrackSide, float* Out) const
	{
		if (Shape)
		{
			Shape->PoseLinks(TrackSide, TrackSide < 0 ? RunLeft : RunRight, Out);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The battle.

	namespace
	{
		int MonthOf(const std::string& Date, int Otherwise)
		{
			static const char* Names[12] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
			std::string Low;
			for (char C : Date)
			{
				Low += static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
			}
			for (int I = 0; I < 12; ++I)
			{
				if (Low.find(Names[I]) != std::string::npos)
				{
					return I + 1;
				}
			}
			return Otherwise;
		}

		int YearOf(const std::string& Date, int Otherwise)
		{
			auto IsDigit = [&Date](size_t I) { return I < Date.size() && Date[I] >= '0' && Date[I] <= '9'; };
			for (size_t I = 0; I + 4 <= Date.size(); ++I)
			{
				if (IsDigit(I) && IsDigit(I + 1) && IsDigit(I + 2) && IsDigit(I + 3) && !IsDigit(I + 4) && (I == 0 || !IsDigit(I - 1)))
				{
					return std::atoi(Date.substr(I, 4).c_str());
				}
			}
			return Otherwise;
		}

		/** How fast a track was going a moment before T (m/s), from its kept points. */
		double SpeedBefore(const TrackWords& W, double T)
		{
			const size_t N = W.At.size() / 4;
			const double When = T - 0.3;
			for (size_t I = 1; I < N; ++I)
			{
				const double T0 = W.At[(I - 1) * 4], T1 = W.At[I * 4];
				if (T0 <= When && When < T1)
				{
					const double Span = T1 - T0;
					const double D = std::hypot(W.At[I * 4 + 1] - W.At[(I - 1) * 4 + 1], W.At[I * 4 + 2] - W.At[(I - 1) * 4 + 2]);
					return Span > 1e-6 && D < 25.0 * Span ? D / Span : 0.0;
				}
			}
			return 0.0;
		}

		/** The Fates table's word for what struck: a tank's or a gun's calibre is its block's. */
		std::string WeaponWord(const std::string& W, const Block* Shooter)
		{
			if (W == "tank" || W == "gun")
			{
				return Shooter && !Shooter->Cal.empty() ? Shooter->Cal : (W == "gun" ? "howitzer" : "field");
			}
			if (W == "at")
			{
				return "light";
			}
			if (W == "mortar")
			{
				return "mortar";
			}
			return "smallarms";
		}
	}

	namespace
	{
		bool IsVehicle(const std::string& Role) { return Role == "tank" || Role == "gun" || Role == "lorry"; }

		/** A track's swaps the rules picked (release 2, "sw": {swap: choice}); none where it says none. */
		void ReadSwaps(const Json::Value& Sw, std::map<std::string, std::string>& Out)
		{
			Out.clear();
			for (size_t I = 0; Sw.IsObject() && I < Sw.Count(); ++I)
			{
				Out[Sw.Name(I)] = Sw.At(I).Str("");
			}
		}

		/** A change of a track: [t, pose, cover, aimX | null, aimY | null, hidden, buttoned]. */
		Change ReadChange(const Json::Value& C)
		{
			Change Out;
			Out.T = C.At(0).Num();
			Out.Pose = C.At(1).Str();
			Out.Cover = C.At(2).Str();
			Out.bAim = C.At(3).IsNumber() && C.At(4).IsNumber();
			if (Out.bAim)
			{
				Out.AimX = C.At(3).Num();
				Out.AimY = C.At(4).Num();
			}
			Out.bHidden = C.At(5).Truth();
			Out.bButtoned = C.At(6).Truth();
			return Out;
		}

		/** A scar: [face, u, h, kind, from]. */
		ScarWord ReadScar(const Json::Value& S)
		{
			ScarWord Out;
			Out.Face = S.At(0).Str();
			Out.U = S.At(1).Num();
			Out.H = S.At(2).Num();
			Out.Kind = S.At(3).Str();
			Out.From = S.At(4).Num();
			return Out;
		}

		Round ReadRound(const Json::Value& V)
		{
			Round Out;
			Out.N = V["n"].Int();
			Out.T = V["t"].Num();
			Out.L = V["l"].Num(Out.T);
			Out.By = V["by"].Int(-1);
			Out.K = V["k"].Int(-1);
			Out.J = std::max(0, V["j"].Int(0));
			Out.W = V["w"].Str();
			Out.R = V["r"].Str();
			Out.Face = V["fc"].Str("");
			Out.On = V["on"].Int(-1);
			Out.Ok = V["ok"].Int(-1);
			Out.Oj = std::max(0, V["oj"].Int(0));
			Out.X = V["x"].Num();
			Out.Y = V["y"].Num();
			Out.U = V["u"].Num();
			Out.H = V["h"].Num();
			Out.Fate = V["fate"].Str("");
			return Out;
		}

		/** A wreck laid where it was hit: the recording's last point jumps 14 to 22 m in a tenth of a second at her death
		 * (as LandFeed reads it too); from that jump on she keeps the place before it. */
		void PinWreck(TrackWords& W)
		{
			for (size_t P = 4; P + 3 < W.At.size(); P += 4)
			{
				if (W.At[P] < W.Died - 0.15 || W.At[P] - W.At[P - 4] > 0.6 || std::hypot(W.At[P + 1] - W.At[P - 3], W.At[P + 2] - W.At[P - 2]) < 3.0)
				{
					continue;
				}
				for (size_t K = P; K + 3 < W.At.size(); K += 4)
				{
					W.At[K + 1] = W.At[P - 3];
					W.At[K + 2] = W.At[P - 2];
				}
				break;
			}
		}

		const Block* FindBlock(const std::vector<Block>& Blocks, int Id)
		{
			for (const Block& B : Blocks)
			{
				if (B.Id == Id)
				{
					return &B;
				}
			}
			return nullptr;
		}

		void SkipSpace(const std::string& S, size_t& I)
		{
			while (I < S.size() && std::isspace(static_cast<unsigned char>(S[I])))
			{
				++I;
			}
		}

		/** Where the JSON value that begins at I (after any spaces) ends, its brackets matched and its strings passed over
		 * whole, unread; npos on a fault. */
		size_t PastValue(const std::string& S, size_t I)
		{
			SkipSpace(S, I);
			const char* const Start = S.data();
			const char* const End = Start + S.size();
			const char* P = Start + I;
			// (past a string: to its closing quote, one not escaped)
			auto PastString = [End](const char* Q)
			{
				for (++Q;;)
				{
					Q = static_cast<const char*>(std::memchr(Q, '"', static_cast<size_t>(End - Q)));
					if (!Q)
					{
						return End + 1;
					}
					const char* Back = Q;
					while (Back[-1] == '\\')
					{
						--Back;
					}
					if ((Q - Back) % 2 == 0)
					{
						return Q + 1;
					}
					++Q;
				}
			};
			if (P >= End)
			{
				return std::string::npos;
			}
			if (*P == '"')
			{
				P = PastString(P);
				return P > End ? std::string::npos : static_cast<size_t>(P - Start);
			}
			if (*P != '[' && *P != '{')
			{
				// (a bare number or word: to the comma, close or space after it)
				while (P < End && *P != ',' && *P != ']' && *P != '}' && !std::isspace(static_cast<unsigned char>(*P)))
				{
					++P;
				}
				return static_cast<size_t>(P - Start);
			}
			// A list or an object: to its own close, the brackets of strings passed over. (A table of the characters that
			// matter, so the numbers that are most of a piece go by at a look-up each; the text's closing nought is one.)
			static const struct Marks
			{
				bool bMark[256] = {};
				Marks()
				{
					for (const unsigned char C : { '"', '[', ']', '{', '}', '\0' })
					{
						bMark[C] = true;
					}
				}
			} Table;
			int Depth = 0;
			for (; P < End; ++P)
			{
				while (!Table.bMark[static_cast<unsigned char>(*P)])
				{
					++P;
				}
				if (P >= End)
				{
					break;
				}
				if (*P == '\0')
				{
					continue;
				}
				if (*P == '"')
				{
					P = PastString(P) - 1;
					if (P >= End)
					{
						break;
					}
				}
				else if (*P == '[' || *P == '{')
				{
					++Depth;
				}
				else if (--Depth == 0)
				{
					return static_cast<size_t>(P - Start) + 1;
				}
			}
			return std::string::npos;
		}

		/** A live piece with only what the vehicles read in it. Most of a piece is the news of men on foot (LandMen's):
		 * a track already known not to be a vehicle's is passed over here by its brackets, unread, and so is its
		 * provisional point, and the views, events and phases. A new track is kept (its n counts), and so is a man's
		 * news that does not begin with his n. Empty when the text is not laid out as a piece is (it is read whole). */
		std::string SkimPiece(const std::string& S, const std::vector<int>& TrackVehicle)
		{
			struct Field
			{
				std::string Name;
				size_t From = 0, To = 0;
				bool bMen = false; // the men's list, read a man at a time
			};
			std::vector<Field> Fields;
			std::vector<std::pair<size_t, size_t>> Men; // each man's news, where it stands in the text
			bool bMenRead = false;
			size_t I = 0;
			SkipSpace(S, I);
			if (I >= S.size() || S[I] != '{')
			{
				return std::string();
			}
			++I;
			for (bool bMore = true; bMore;)
			{
				SkipSpace(S, I);
				if (I >= S.size() || S[I] != '"')
				{
					return std::string();
				}
				const size_t NameEnd = PastValue(S, I);
				if (NameEnd == std::string::npos)
				{
					return std::string();
				}
				Field F;
				F.Name = S.substr(I + 1, NameEnd - I - 2);
				I = NameEnd;
				SkipSpace(S, I);
				if (I >= S.size() || S[I] != ':')
				{
					return std::string();
				}
				++I;
				SkipSpace(S, I);
				F.From = I;
				if (F.Name == "men" && S[I] == '[' && !bMenRead)
				{
					// (the men's news a man at a time: the list read through once)
					bMenRead = true;
					F.bMen = true;
					for (size_t J = I + 1;;)
					{
						SkipSpace(S, J);
						if (J < S.size() && S[J] == ']' && Men.empty())
						{
							F.To = J + 1;
							break;
						}
						const size_t E = PastValue(S, J);
						if (E == std::string::npos)
						{
							return std::string();
						}
						Men.emplace_back(J, E);
						J = E;
						SkipSpace(S, J);
						if (J < S.size() && S[J] == ']')
						{
							F.To = J + 1;
							break;
						}
						if (J >= S.size() || S[J] != ',')
						{
							return std::string();
						}
						++J;
					}
				}
				else
				{
					F.To = PastValue(S, I);
				}
				if (F.To == std::string::npos || F.To <= F.From)
				{
					return std::string();
				}
				I = F.To;
				Fields.push_back(std::move(F));
				SkipSpace(S, I);
				if (I >= S.size() || (S[I] != ',' && S[I] != '}'))
				{
					return std::string();
				}
				bMore = S[I] == ',';
				++I;
			}
			// (a reset makes every track a new one)
			bool bReset = false;
			for (const Field& F : Fields)
			{
				bReset = bReset || (F.Name == "reset" && S.compare(F.From, 4, "true") == 0);
			}
			const size_t Known = bReset ? 0 : TrackVehicle.size();
			auto Wanted = [&TrackVehicle, Known](const char* At)
			{
				char* Past = nullptr;
				const long N = std::strtol(At, &Past, 10);
				return Past == At || N < 0 || static_cast<size_t>(N) >= Known || TrackVehicle[static_cast<size_t>(N)] >= 0;
			};
			std::string Out;
			Out.reserve(S.size() / 8);
			Out += '{';
			for (const Field& F : Fields)
			{
				if (F.Name == "views" || F.Name == "events" || F.Name == "phases")
				{
					continue;
				}
				Out += Out.size() > 1 ? ",\"" : "\"";
				Out += F.Name;
				Out += "\":";
				const bool bList = S[F.From] == '[' && S[F.To - 1] == ']';
				if (F.bMen)
				{
					Out += '[';
					bool bFirst = true;
					for (const auto& [J, E] : Men)
					{
						if (S[J] != '{' || S.compare(J + 1, 4, "\"n\":") != 0 || Wanted(S.c_str() + J + 5))
						{
							Out += bFirst ? "" : ",";
							Out.append(S, J, E - J);
							bFirst = false;
						}
					}
					Out += ']';
				}
				else if (F.Name == "tails" && bList && std::find(S.begin() + static_cast<std::ptrdiff_t>(F.From) + 1, S.begin() + static_cast<std::ptrdiff_t>(F.To), '[')
					== S.begin() + static_cast<std::ptrdiff_t>(F.To))
				{
					// (four numbers a man: n, x, y, f)
					Out += '[';
					bool bFirst = true;
					const size_t Close = F.To - 1;
					for (size_t J = F.From + 1; J < Close;)
					{
						size_t E = J;
						for (int Commas = 0; E < Close; ++E)
						{
							if (S[E] == ',' && ++Commas == 4)
							{
								break;
							}
						}
						if (Wanted(S.c_str() + J))
						{
							Out += bFirst ? "" : ",";
							Out.append(S, J, E - J);
							bFirst = false;
						}
						J = E + 1;
					}
					Out += ']';
				}
				else
				{
					Out.append(S, F.From, F.To - F.From);
				}
			}
			Out += '}';
			return Out;
		}
	}

	bool ReadRecording(const Json& Doc, std::vector<Block>& OutBlocks, std::vector<TrackWords>& OutTracks,
		std::vector<Round>& OutRounds, double& OutEnd, int& OutYear, int& OutMonth, std::string& OutError)
	{
		OutBlocks.clear();
		OutTracks.clear();
		OutRounds.clear();
		const Json::Value R = Doc.Root();
		if (!R.IsObject() || !R["men"].IsArray() || !R["blocks"].IsArray())
		{
			OutError = "not a land battle recording (no blocks or men)";
			return false;
		}
		OutEnd = R["end"].Num(0.0);
		const std::string Date = R["date"].Str("");
		OutYear = YearOf(Date, 1944);
		OutMonth = MonthOf(Date, 7);
		const Json::Value Blocks = R["blocks"];
		for (size_t I = 0; I < Blocks.Count(); ++I)
		{
			const Json::Value V = Blocks.At(I);
			Block B;
			B.Id = V["id"].Int();
			B.Side = V["side"].Str();
			B.Nation = V["nat"].Str();
			B.Kind = V["kind"].Str();
			B.Model = V["model"].Str("");
			B.Cal = V["cal"].Str("");
			B.Captured = V["captured"].Str("");
			const Json::Value Mls = V["mls"];
			for (size_t J = 0; Mls.IsArray() && J < Mls.Count(); ++J)
			{
				B.Extra.push_back(Mls.At(J).Str(""));
			}
			OutBlocks.push_back(B);
		}
		const Json::Value Men = R["men"];
		std::vector<double> Row;
		for (size_t I = 0; I < Men.Count(); ++I)
		{
			const Json::Value Man = Men.At(I);
			const std::string Role = Man["r"].Str();
			if (!IsVehicle(Role))
			{
				continue;
			}
			TrackWords W;
			W.Block = Man["b"].Int();
			W.K = Man["k"].Int();
			W.J = std::max(0, Man["j"].Int(0));
			W.Role = Role;
			W.Model = Man["ml"].Str("");
			ReadSwaps(Man["sw"], W.Swaps);
			const Json::Value At = Man["at"];
			for (size_t J = 0; J < At.Count(); ++J)
			{
				if (At.At(J).Numbers(Row) && Row.size() >= 4)
				{
					W.At.insert(W.At.end(), Row.begin(), Row.begin() + 4);
				}
			}
			const Json::Value Ch = Man["ch"];
			for (size_t J = 0; J < Ch.Count(); ++J)
			{
				W.Changes.push_back(ReadChange(Ch.At(J)));
			}
			Man["s"].Numbers(W.Shots);
			std::sort(W.Shots.begin(), W.Shots.end());
			if (Man["d"].IsNumber())
			{
				W.Died = Man["d"].Num();
				PinWreck(W);
			}
			if (Man["out"].IsNumber())
			{
				W.Out = Man["out"].Num();
			}
			const Json::Value Mk = Man["mk"];
			for (size_t J = 0; J < Mk.Count(); ++J)
			{
				W.Scars.push_back(ReadScar(Mk.At(J)));
			}
			if (OutEnd <= 0.0 && W.At.size() >= 4)
			{
				OutEnd = std::max(OutEnd, W.At[W.At.size() - 4]);
			}
			OutTracks.push_back(std::move(W));
		}
		const Json::Value Rs = R["rounds"];
		for (size_t I = 0; I < Rs.Count(); ++I)
		{
			OutRounds.push_back(ReadRound(Rs.At(I)));
		}
		return true;
	}

	const Block* Battle::BlockOf(int Id) const
	{
		return FindBlock(BlockList, Id);
	}

	bool Battle::LoadLook(const Block& B, const std::string& Role, std::string& OutKey, std::string& OutError, const std::string& Carrier,
		const std::map<std::string, std::string>& Choose)
	{
		// (the lorry stands in for both sides': the real ones were the GMC two-and-a-half-ton truck and the Opel Blitz;
		// she wears her owner's coat. Release 2: where the recording names a vehicle's own model (ml), she is drawn as
		// that: an armoured infantry's carrier, a half-track of her army's or the lorry; a battery's self-propelled gun;
		// an armoured block's tank destroyer)
		const std::string ModelName = !Carrier.empty() ? Carrier : Role == "lorry" ? std::string("Truck") : B.Model;
		if (ModelName.empty())
		{
			OutError = "block " + std::to_string(B.Id) + " has a " + Role + " but names no model";
			return false;
		}
		// (captured kit, on its switch: the captor's coat over the maker's, a look of her own)
		const std::string Captured = Role != "lorry" && Carrier.empty() && (bCapturedLook || bCapturedTest) ? B.Captured : std::string();
		OutKey = ModelName + "|" + B.Nation + (Captured.empty() ? "" : "|from " + Captured);
		for (const auto& Sw : Choose)
		{
			OutKey += "|" + Sw.first + "=" + Sw.second; // (her swaps the rules picked: a look of her own)
		}
		if (Looks.count(OutKey))
		{
			return true;
		}
		std::string Text;
		if (!Reader || !Reader(ModelName + ".json", Text))
		{
			// (a model not in the game yet: the nearest generic already here, by kind and nation, in her own nation's coat,
			// said once: StandIns. A gun by her side's field gun; a light machine as the light tank; a tank as her side's
			// medium: the German's Panzer IV, the Soviet's T-34, the rest the Sherman)
			const std::string& N = B.Nation;
			const bool bAxis = N == "GER" || N == "HUN" || N == "ROM" || N == "FIN" || N == "ITA";
			static const char* const Light[] = { "Light", "L3", "L6", "Stuart", "M3L", "HaGo", "Type95", "BT", "T26", "T60", "T70", "222", "Pz38", "PzII", "Tankette" };
			bool bLight = false;
			for (const char* L : Light)
			{
				bLight = bLight || ModelName.find(L) != std::string::npos;
			}
			std::vector<std::string> Near;
			if (Role == "lorry") Near = { "Truck" }; // (a carrier not in the game yet: the lorry)
			else if (B.Kind == "art" && Carrier.empty()) Near = { N == "SOV" ? "ZiS3" : bAxis ? "leFH18" : "Howitzer105" };
			else if (bLight || N == "JAP") Near = { "LightTank" };
			else Near = { N == "SOV" ? "T34" : bAxis ? "PanzerIV" : "Sherman" };
			for (const char* Any : { "LightTank", "PanzerIV", "Sherman", "T34" })
			{
				Near.push_back(Any);
			}
			std::string Drawn;
			for (const std::string& M : Near)
			{
				if (M != ModelName && Reader && Reader(M + ".json", Text))
				{
					Drawn = M;
					break;
				}
			}
			if (Drawn.empty())
			{
				OutError = "cannot read the model " + ModelName + ".json";
				return false;
			}
			StandInLines.push_back("the " + N + " " + ModelName + " drawn as the " + Drawn + " in her own coat (no " + ModelName + ".json in Content/Models/Battle)");
		}
		Dress D;
		D.Nation = B.Nation;
		D.Year = Year;
		D.Month = Month;
		D.Terrain = Terrain;
		D.Captured = Captured;
		D.Choose = Choose;
		std::string Why;
		if (!Looks[OutKey].Load(Text, D, Why))
		{
			Looks.erase(OutKey);
			OutError = ModelName + ".json: " + Why;
			return false;
		}
		return true;
	}

	bool ReadFireOdds(const std::string& Text, std::map<std::string, FireOdds>& Out, std::string& OutError)
	{
		Json Doc;
		if (!Doc.Parse(Text, OutError))
		{
			OutError = "the performance file: " + OutError;
			return false;
		}
		const Json::Value List = Doc.Root()["vehicles"];
		if (!List.IsObject())
		{
			OutError = "the performance file has no vehicles";
			return false;
		}
		for (size_t I = 0; I < List.Count(); ++I)
		{
			const Json::Value V = List.At(I);
			const Json::Value F = V["fire_when_pierced"];
			if (!F.IsObject())
			{
				continue;
			}
			FireOdds O;
			O.Front = Clamp(F["front"].Num(), 0.0, 1.0);
			O.Side = Clamp(F["side"].Num(), 0.0, 1.0);
			O.Rear = Clamp(F["rear"].Num(), 0.0, 1.0);
			O.Explodes = Clamp(V["explodes_of_fires"].Num(), 0.0, 1.0);
			Out[List.Name(I)] = O;
		}
		return true;
	}

	std::string Battle::PickLook(const TrackWords& W, const Model& M, const std::vector<Round>& Rounds, int Index, uint8_t& OutBrew)
	{
		if (!Picker)
		{
			const bool bBrew = W.Role == "tank" && Unit01(Mix(FateSeed, static_cast<uint64_t>(W.Block), static_cast<uint64_t>(W.K))) < 0.25;
			OutBrew = bBrew ? 1 : 0;
			return bBrew ? "brew_up" : "knocked_out";
		}
		const Round* Kill = nullptr;
		for (const Round& R : Rounds)
		{
			if (R.On == W.Block && R.Ok == W.K && R.Oj == W.J && R.R == "kill")
			{
				Kill = &R;
			}
		}
		Fates::Blow Bl;
		Bl.Domain = "land";
		Bl.Family = W.Role == "lorry" ? "truck" : W.Role == "gun" ? "gun" : M.Name.find("Tiger") != std::string::npos ? "tank_heavy" : "tank_medium";
		Bl.Weapon = Kill ? WeaponWord(Kill->W, BlockOf(Kill->By)) : "field";
		Bl.Result = "lost";
		const std::string Face = Kill ? Kill->Face : std::string();
		Bl.Aspect = Face == "left" || Face == "right" ? "side" : Face;
		Bl.Year = Year;
		// (the Sherman's, the Tiger's and the Panzer IV's engines all ran on petrol, as did both lorries')
		Bl.Fuel = "petrol";
		Bl.Ground = "earth";
		Bl.bMoving = SpeedBefore(W, W.Died) > 0.5;
		Bl.Unit = W.Block;
		Bl.Model = W.K;
		// Her end: the rules' own where her killing round names it (fate: out, burn or blow, from her measured odds for
		// the face struck, so a dispatch's "blew up" is what the scene shows); else drawn here by the same odds (a tank or
		// a self-propelled gun: the fates' tank ways name theirs, blow, burn or still). The table then picks how that end
		// looks.
		const Block* Of = BlockOf(W.Block);
		// (by the model's file name, as performance-land.json keys it: her own where the recording names it, a tank
		// destroyer's or a self-propelled gun's, else her block's)
		const std::string Named = !W.Model.empty() && W.Role != "lorry" ? W.Model : Of ? Of->Model : M.Name;
		const auto Odds = Fires.find(Named);
		if (Kill && (Kill->Fate == "out" || Kill->Fate == "burn" || Kill->Fate == "blow"))
		{
			Bl.How = Kill->Fate == "out" ? "still" : Kill->Fate;
		}
		else if (Odds != Fires.end() && (Bl.Family.compare(0, 5, "tank_") == 0 || Bl.Family == "spgun"))
		{
			const uint64_t H = Mix(FateSeed ^ 0xF17Eull, static_cast<uint64_t>(W.Block) * 977u + static_cast<uint64_t>(W.K), static_cast<uint64_t>(W.J) + 1u);
			Bl.How = FireEnd(Odds->second, Bl.Aspect, Unit01(H), Unit01(Mix(H, 0xB10Bull)));
		}
		const std::string Drawn = Bl.How;
		Fates::Fate F = Picker->Choose(Bl);
		// (a self-propelled gun has no turret to throw: her ammunition going up burns her out instead; a table with no
		// ways for the end drawn shows any)
		if (F.Key.empty() && Bl.How == "blow")
		{
			Bl.How = "burn";
			F = Picker->Choose(Bl);
		}
		if (F.Key.empty() && !Bl.How.empty())
		{
			Bl.How.clear();
			F = Picker->Choose(Bl);
		}
		Ends.push_back({ Index, Named, Bl.Aspect, Drawn, F.Key });
		OutBrew = F.Key == "tank.brew_up" || F.Key == "tank.turret_off" ? 1 : 0;
		return F.Key;
	}


	double Battle::BrewWaitOf(size_t V) const
	{
		// Effects (R2): her own wait between the least and the most, by her number (life's seconds, made the battle's)
		if (!(BrewWaitMost > 0.0) || !(Life > 0.0))
		{
			return 0.0;
		}
		const double U = Unit01(Mix(FateSeed ^ 0xB5E3A17ull, static_cast<uint64_t>(V) + 1u, 0x5EC5u));
		return std::max(0.0, BrewWaitLeast + (BrewWaitMost - BrewWaitLeast) * U) / Life;
	}

	std::string FireEnd(const FireOdds& O, const std::string& Aspect, double U1, double U2)
	{
		// (a plunging shot through her roof or her engine deck: as through her side)
		const double Fire = Aspect == "front" ? O.Front : Aspect == "rear" ? O.Rear : Aspect == "side" || Aspect == "top" ? O.Side
			: (O.Front + O.Side + O.Rear) / 3.0;
		if (U1 >= Fire)
		{
			return "still";
		}
		return U2 < O.Explodes ? "blow" : "burn";
	}

	bool Battle::Load(const std::string& RecordingText, const ReadFn& Read, const GroundFn& InGround,
		const Fates::Table* FatesTable, uint64_t Seed, std::string& OutError)
	{
		Ends.clear();
		All.clear();
		Looks.clear();
		Events.clear();
		Reader = Read;
		Ground = InGround;
		FateTable = FatesTable;
		FateSeed = Seed;
		Picker = FatesTable ? std::make_shared<Fates::Director>(*FatesTable, Seed) : nullptr;
		KeyOf.clear();
		Grown.clear();
		Tailed.clear();
		Picked.clear();
		Brew.clear();
		Own.clear();
		WorkedTo.clear();
		TrackVehicle.clear();
		ByWho.clear();
		bLive = false;
		bOver = false;
		Shown = -1.0;
		Json Doc;
		if (!Doc.Parse(RecordingText, OutError))
		{
			OutError = "the recording: " + OutError;
			return false;
		}
		std::vector<TrackWords> Tracks;
		if (!ReadRecording(Doc, BlockList, Tracks, RoundList, End, Year, Month, OutError))
		{
			return false;
		}
		Terrain = Doc.Root()["field"]["place"]["terrain"].Str("plains");
		bLive = Doc.Root()["live"].Truth(false);
		Limit = Doc.Root()["limit"].Num(0.0);
		// (captured kit's off/on line on the Mac: every other tank block of the attacker drawn as the defender's first
		// tank, taken from them, in the attacker's coat; the recording's own fight, only the look changed)
		if (bCapturedTest)
		{
			const Block* Theirs = nullptr;
			for (const Block& B : BlockList)
			{
				if (B.Side == "def" && B.Kind == "arm" && !B.Model.empty() && B.Model != "LightTank") { Theirs = &B; break; }
			}
			int Nth = 0;
			for (Block& B : BlockList)
			{
				if (!Theirs || B.Side != "att" || B.Kind != "arm" || B.Model.empty() || B.Model == "LightTank" || Nth++ % 2) continue;
				B.Model = Theirs->Model;
				B.Captured = Theirs->Nation;
			}
		}

		// The models, each once a nation (the lorry stands in for both sides').
		KeyOf.resize(Tracks.size());
		for (size_t I = 0; I < Tracks.size(); ++I)
		{
			const Block* B = BlockOf(Tracks[I].Block);
			if (!B)
			{
				OutError = "a vehicle of block " + std::to_string(Tracks[I].Block) + ", which the recording does not list";
				return false;
			}
			if (!LoadLook(*B, Tracks[I].Role, KeyOf[I], OutError, Tracks[I].Model, Tracks[I].Swaps))
			{
				return false;
			}
		}
		if (bLive)
		{
			// A live battle's header has no men yet: the models its blocks name are read now (and the lorry of a side
			// with guns to tow; release 2: the half-tracks, self-propelled guns and tank destroyers its men are drawn
			// with, "mls"), so its first pieces do not wait on them.
			for (const Block& B : BlockList)
			{
				std::string Key;
				if (!B.Model.empty() && (!LoadLook(B, "tank", Key, OutError) || (B.Kind == "art" && !LoadLook(B, "lorry", Key, OutError))))
				{
					return false;
				}
				for (const std::string& Look : B.Extra)
				{
					// ("Jeep|mg=off": the model and the swaps the rules pick for her)
					std::map<std::string, std::string> Choose;
					size_t Bar = Look.find('|');
					const std::string Ml = Look.substr(0, Bar);
					while (Bar != std::string::npos)
					{
						const size_t Next = Look.find('|', Bar + 1), Eq = Look.find('=', Bar + 1);
						if (Eq != std::string::npos && Eq < Next)
						{
							Choose[Look.substr(Bar + 1, Eq - Bar - 1)] = Look.substr(Eq + 1, Next == std::string::npos ? std::string::npos : Next - Eq - 1);
						}
						Bar = Next;
					}
					if (!Ml.empty() && !LoadLook(B, Ml == "Truck" ? "lorry" : "tank", Key, OutError, Ml, Choose))
					{
						return false;
					}
				}
			}
		}

		// How each kill looks, picked in the order they fell (the picker remembers what it has just shown).
		std::vector<size_t> Order;
		for (size_t I = 0; I < Tracks.size(); ++I)
		{
			if (Tracks[I].Died >= 0.0)
			{
				Order.push_back(I);
			}
		}
		std::stable_sort(Order.begin(), Order.end(), [&Tracks](size_t A, size_t B) { return Tracks[A].Died < Tracks[B].Died; });
		Brew.assign(Tracks.size(), 0);
		Picked.assign(Tracks.size(), 0);
		std::vector<std::string> FateOf(Tracks.size());
		for (size_t I : Order)
		{
			FateOf[I] = PickLook(Tracks[I], Looks[KeyOf[I]], RoundList, static_cast<int>(I), Brew[I]);
			Picked[I] = 1;
		}

		All.resize(Tracks.size());
		for (size_t I = 0; I < Tracks.size(); ++I)
		{
			const Block* B = BlockOf(Tracks[I].Block);
			All[I].Life = Life;
			All[I].BrewWait = BrewWaitOf(I); // (Effects (R2))
			All[I].bKeepPlates = bKeepPlates; // (Effects (R2))
			All[I].Build(Looks[KeyOf[I]], Tracks[I], RoundList, End, InGround, Brew[I] != 0, static_cast<int>(I), B->Side, B->Nation);
			All[I].FateKey = FateOf[I];
		}
		WorkedTo.assign(All.size(), End);
		return true;
	}

	bool Battle::Extend(const std::string& DeltaText, std::string& OutError)
	{
		if (!bLive)
		{
			OutError = "not a live battle (Load its header first)";
			return false;
		}
		// (read skimmed of the men on foot; read whole when the skim cannot be, so a fault is named as in the text)
		Json Doc;
		const std::string Skimmed = SkimPiece(DeltaText, TrackVehicle);
		if ((Skimmed.empty() || !Doc.Parse(Skimmed, OutError)) && !Doc.Parse(DeltaText, OutError))
		{
			OutError = "a piece of the live battle: " + OutError;
			return false;
		}
		const Json::Value R = Doc.Root();
		if (!R.IsObject() || !R["t"].IsNumber() || !R["men"].IsArray())
		{
			OutError = "not a live ground battle's piece (no t or no men)";
			return false;
		}
		const double T = R["t"].Num();
		if (R["reset"].Truth(false))
		{
			// The men placed again before the clock ran: every track begins again (the rules' numbers with them), and the
			// picker with them (nothing has fallen yet).
			All.clear();
			KeyOf.clear();
			Grown.clear();
			Tailed.clear();
			Picked.clear();
			Brew.clear();
			Own.clear();
			WorkedTo.clear();
			TrackVehicle.clear();
			ByWho.clear();
			RoundList.clear();
			Events.clear();
			Picker = FateTable ? std::make_shared<Fates::Director>(*FateTable, FateSeed) : nullptr;
		}
		const size_t Before = All.size();
		std::vector<uint8_t> Moved(All.size(), 0);
		std::vector<double> Row;

		// The tracks' news. A track's n is its place in the order first seen: the next new one is the next number. A man
		// on foot is the men's (LandMen); a tank, gun or lorry is a vehicle, her words grown here.
		const Json::Value Men = R["men"];
		for (size_t I = 0; I < Men.Count(); ++I)
		{
			const Json::Value Man = Men.At(I);
			const int N = Man["n"].Int(-1);
			if (N < 0 || N > static_cast<int>(TrackVehicle.size()))
			{
				OutError = "a piece names track " + std::to_string(N) + " of " + std::to_string(TrackVehicle.size());
				return false;
			}
			if (N == static_cast<int>(TrackVehicle.size()))
			{
				if (!Man["b"].IsNumber())
				{
					OutError = "a new track " + std::to_string(N) + " without who it is";
					return false;
				}
				const std::string Role = Man["r"].Str();
				int Made = -1;
				if (IsVehicle(Role))
				{
					TrackWords W;
					W.Block = Man["b"].Int();
					W.K = Man["k"].Int();
					W.J = std::max(0, Man["j"].Int(0));
					W.Role = Role;
					W.Model = Man["ml"].Str("");
					ReadSwaps(Man["sw"], W.Swaps);
					const Block* B = BlockOf(W.Block);
					if (!B)
					{
						OutError = "a vehicle of block " + std::to_string(W.Block) + ", which the recording does not list";
						return false;
					}
					std::string Key;
					if (!LoadLook(*B, Role, Key, OutError, W.Model, W.Swaps))
					{
						return false;
					}
					Made = static_cast<int>(All.size());
					All.emplace_back();
					KeyOf.push_back(Key);
					ByWho[std::make_tuple(W.Block, W.K, W.J)] = Made;
					Grown.push_back(std::move(W));
					Tailed.push_back(0);
					Picked.push_back(0);
					Brew.push_back(0);
					Own.emplace_back();
					WorkedTo.push_back(0.0);
					Moved.push_back(1);
				}
				TrackVehicle.push_back(Made);
			}
			if (TrackVehicle[static_cast<size_t>(N)] < 0)
			{
				continue;
			}
			const size_t V = static_cast<size_t>(TrackVehicle[static_cast<size_t>(N)]);
			TrackWords& W = Grown[V];
			Moved[V] = 1;
			const Json::Value At = Man["at"];
			if (At.Count() > 0)
			{
				// (her provisional point gives way to the kept ones; it stays only while it is still later than them)
				double Tail[4] = { 0.0, 0.0, 0.0, 0.0 };
				const bool bTail = Tailed[V] && W.At.size() >= 4;
				if (bTail)
				{
					std::copy(W.At.end() - 4, W.At.end(), Tail);
					W.At.resize(W.At.size() - 4);
				}
				Tailed[V] = 0;
				for (size_t J = 0; J < At.Count(); ++J)
				{
					if (At.At(J).Numbers(Row) && Row.size() >= 4)
					{
						W.At.insert(W.At.end(), Row.begin(), Row.begin() + 4);
					}
				}
				if (bTail && (W.At.size() < 4 || Tail[0] > W.At[W.At.size() - 4]))
				{
					W.At.insert(W.At.end(), Tail, Tail + 4);
					Tailed[V] = 1;
				}
			}
			const Json::Value Ch = Man["ch"];
			for (size_t J = 0; J < Ch.Count(); ++J)
			{
				W.Changes.push_back(ReadChange(Ch.At(J)));
			}
			if (Man["s"].Numbers(Row) && !Row.empty())
			{
				W.Shots.insert(W.Shots.end(), Row.begin(), Row.end());
				std::sort(W.Shots.begin(), W.Shots.end());
			}
			if (Man["d"].IsNumber())
			{
				W.Died = Man["d"].Num();
			}
			if (Man["out"].IsNumber())
			{
				const double Out = Man["out"].Num();
				W.Out = Out < 0.0 ? -1.0 : Out; // (-1: back on the field's list)
			}
			const Json::Value Mk = Man["mk"];
			if (Mk.IsArray())
			{
				W.Scars.clear();
				for (size_t J = 0; J < Mk.Count(); ++J)
				{
					W.Scars.push_back(ReadScar(Mk.At(J)));
				}
			}
		}
		// Where each moving vehicle stands now, [n, x, y, f] at t: her provisional last point, in place of the one before.
		if (R["tails"].Numbers(Row))
		{
			for (size_t K = 0; K + 3 < Row.size(); K += 4)
			{
				const int N = static_cast<int>(Row[K]);
				if (N < 0 || N >= static_cast<int>(TrackVehicle.size()))
				{
					OutError = "a piece's provisional point names track " + std::to_string(N);
					return false;
				}
				if (TrackVehicle[static_cast<size_t>(N)] < 0)
				{
					continue;
				}
				const size_t V = static_cast<size_t>(TrackVehicle[static_cast<size_t>(N)]);
				TrackWords& W = Grown[V];
				if (Tailed[V] && W.At.size() >= 4)
				{
					W.At.resize(W.At.size() - 4);
				}
				const double Point[4] = { T, Row[K + 1], Row[K + 2], Row[K + 3] };
				W.At.insert(W.At.end(), Point, Point + 4);
				Tailed[V] = 1;
				Moved[V] = 1;
			}
		}
		// The new rounds: each to the vehicle that fired it (a tank's or a gun's) and the one it struck.
		const Json::Value Rs = R["rounds"];
		for (size_t I = 0; I < Rs.Count(); ++I)
		{
			RoundList.push_back(ReadRound(Rs.At(I)));
			const Round& Rd = RoundList.back();
			int By = -1;
			if (Rd.W == "tank" || Rd.W == "gun")
			{
				const auto Found = ByWho.find(std::make_tuple(Rd.By, Rd.K, Rd.J));
				if (Found != ByWho.end())
				{
					By = Found->second;
					Own[static_cast<size_t>(By)].push_back(Rd);
					Moved[static_cast<size_t>(By)] = 1;
				}
			}
			if (!Rd.R.empty() && Rd.R != "miss")
			{
				const auto Found = ByWho.find(std::make_tuple(Rd.On, Rd.Ok, Rd.Oj));
				if (Found != ByWho.end() && Found->second != By)
				{
					Own[static_cast<size_t>(Found->second)].push_back(Rd);
					Moved[static_cast<size_t>(Found->second)] = 1;
				}
			}
		}
		bOver = !R["over"].IsNull();
		End = bOver && R["end"].Num(0.0) > 0.0 ? R["end"].Num(0.0) : T;

		// How each new kill looks, picked as her death comes in, in the order they fell (as Load picks them).
		auto Laid = [](const TrackWords& W)
		{
			TrackWords Out = W;
			PinWreck(Out);
			return Out;
		};
		std::vector<size_t> Order;
		for (size_t V = 0; V < All.size(); ++V)
		{
			if (!Picked[V] && Grown[V].Died >= 0.0)
			{
				Order.push_back(V);
			}
		}
		std::stable_sort(Order.begin(), Order.end(), [this](size_t A, size_t B) { return Grown[A].Died < Grown[B].Died; });
		for (size_t V : Order)
		{
			All[V].FateKey = PickLook(Laid(Grown[V]), Looks[KeyOf[V]], Own[V], static_cast<int>(V), Brew[V]);
			Picked[V] = 1;
			Moved[V] = 1;
		}

		// Each vehicle worked out to the new End: a new one whole (and caught up with the screen: what is past for it is
		// not given out), one whose words grew from a little before the change, and every other one at least every three
		// quarters of a battle second (the screen keeps a second or more behind the pieces: never past what she is worked
		// out to). A fire to the battle's end burns to its limit until the end is known.
		const double Loops = bOver ? 0.0 : std::max(Limit, End);
		for (size_t V = 0; V < All.size(); ++V)
		{
			const bool bNew = V >= Before;
			if (!bNew && !Moved[V] && !bOver && End - WorkedTo[V] < 0.75)
			{
				continue;
			}
			Vehicle& Ve = All[V];
			Ve.Life = Life;
			Ve.LoopsUntil = Loops;
			Ve.BrewWait = BrewWaitOf(V); // (Effects (R2))
			Ve.bKeepPlates = bKeepPlates; // (Effects (R2))
			TrackWords Wreck;
			const TrackWords* W = &Grown[V];
			if (W->Died >= 0.0)
			{
				Wreck = Laid(*W);
				W = &Wreck;
			}
			if (bNew)
			{
				const Block* B = BlockOf(W->Block);
				Ve.Build(Looks[KeyOf[V]], *W, Own[V], End, Ground, Brew[V] != 0, static_cast<int>(V), B->Side, B->Nation);
				if (Shown >= 0.0)
				{
					Ve.Update(Shown, nullptr);
				}
			}
			else
			{
				Ve.Grow(*W, Own[V], End, Brew[V] != 0);
			}
			WorkedTo[V] = End;
		}
		return true;
	}

	void Battle::Update(double T)
	{
		Shown = T;
		for (Vehicle& V : All)
		{
			V.Update(T, &Events);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// TK1: the far level merged, for drawing far vehicles as instances.

	namespace
	{
		/** A body flag the scene hides a body by while her vehicle stands (bWhileManned: a roof gun's cradle shown only
		 * while it is manned), read where the model's bodies carry it, and false where they do not. */
		template <typename TBody>
		bool HiddenAtTimes(const TBody& B)
		{
			if constexpr (requires(const TBody& X) { static_cast<bool>(X.bWhileManned); })
			{
				return static_cast<bool>(B.bWhileManned);
			}
			else
			{
				(void)B;
				return false;
			}
		}
	}

	bool MayHide(const Body& B)
	{
		return HiddenAtTimes(B);
	}

	void SplitFar(const Model& M, FarSplit& Out, const std::vector<uint8_t>* KeepOut)
	{
		Out = FarSplit();
		const size_t N = M.Bodies.size();
		Out.PieceOf.assign(N, -1);
		Out.Kept.assign(N, 0);
		Out.StandIn.assign(N, 0);
		Out.Reach.assign(N, 0.0);
		Out.Loose.assign(N, 0);
		auto Under = [&M](int P, int Top)
		{
			for (; P >= 0 && Top >= 0; P = M.Parts[static_cast<size_t>(P)].Parent)
			{
				if (P == Top)
				{
					return true;
				}
			}
			return false;
		};
		const int TurretBody = M.Turret >= 0 ? M.Parts[static_cast<size_t>(M.Turret)].Body : -1;
		for (size_t B = 0; B < N; ++B)
		{
			if (M.Bodies[B].Parent < 0)
			{
				Out.Pieces[FarSplit::HullPiece].Anchor = static_cast<int>(B);
				break;
			}
		}
		Out.Pieces[FarSplit::TurretPiece].Anchor = TurretBody;
		// (a body the scene may hide, and every body hanging on one, stays out of the pieces: bodies come after the body
		// they hang on)
		std::vector<uint8_t> Hides(N, 0);
		for (size_t B = 0; B < N; ++B)
		{
			const Body& Bo = M.Bodies[B];
			Hides[B] = MayHide(Bo) || (KeepOut && B < KeepOut->size() && (*KeepOut)[B])
				|| (Bo.Parent >= 0 && static_cast<size_t>(Bo.Parent) < B && Hides[static_cast<size_t>(Bo.Parent)]);
			// What moves near only and is drawn at rest from afar: a hatch and what hangs on it, the barrel's recoil (and
			// the breech on it) passed by; a spinning wheel's turn passed by, its shift not.
			bool bHatch = false;
			for (int H : M.Hatches)
			{
				bHatch = bHatch || Under(Bo.Part, H);
			}
			if (bHatch || Under(Bo.Part, M.Barrel))
			{
				Out.Loose[B] = 2;
			}
			else if (M.Parts[static_cast<size_t>(Bo.Part)].Kind == PartKind::Spin)
			{
				Out.Loose[B] = 1;
			}
			const bool bFar = !Bo.Far.Triangles.empty(), bNear = !Bo.Near.Triangles.empty();
			if (!bFar && !bNear)
			{
				continue; // (nothing drawn)
			}
			const int Piece = TurretBody >= 0 && Under(Bo.Part, M.Turret) ? FarSplit::TurretPiece : FarSplit::HullPiece;
			// (a body with no near level is drawn far at every distance, by its own mesh; one with no far level, which the
			// scene draws at its near level at every distance (the Panzer IV's suspension arms, the StuG's root), stands
			// in at its near level, so the piece looks as the far bodies looked)
			if (Hides[B] || !bNear || Out.Pieces[Piece].Anchor < 0)
			{
				Out.Kept[B] = bFar ? 1 : 0;
				continue;
			}
			Out.StandIn[B] = bFar ? 0 : 1;
			Out.PieceOf[B] = Piece;
			Out.Pieces[Piece].Bodies.push_back(static_cast<int>(B));
			const Mesh& Drawn = Out.Level(M, static_cast<int>(B));
			for (size_t K = 0; K + 2 < Drawn.Points.size(); K += 3)
			{
				const V3 At(Drawn.Points[K], Drawn.Points[K + 1], Drawn.Points[K + 2]);
				Out.Reach[B] = std::max(Out.Reach[B], At.Length());
			}
		}
		// Each piece: its bodies' meshes one after another, carried from each pivot to the anchor's (at rest every part
		// stands unturned, so a body's points are its pivot's offset from the anchor's away).
		for (FarPiece& Piece : Out.Pieces)
		{
			if (Piece.Bodies.empty())
			{
				continue;
			}
			const V3 Origin = M.Bodies[static_cast<size_t>(Piece.Anchor)].Pivot;
			size_t Points = 0, Indices = 0;
			for (int B : Piece.Bodies)
			{
				Points += Out.Level(M, B).Count();
				Indices += Out.Level(M, B).Triangles.size();
			}
			Mesh& To = Piece.Merged;
			To.Points.reserve(Points * 3);
			To.Normals.reserve(Points * 3);
			To.Colours.reserve(Points * 4);
			To.Soot.reserve(Points);
			To.Triangles.reserve(Indices);
			for (int B : Piece.Bodies)
			{
				const Body& Bo = M.Bodies[static_cast<size_t>(B)];
				const Mesh& From = Out.Level(M, B);
				const uint32_t Base = static_cast<uint32_t>(To.Count());
				Piece.FirstPoint.push_back(Base);
				Piece.FirstTriangle.push_back(static_cast<uint32_t>(To.Triangles.size() / 3));
				const V3 Off = Bo.Pivot - Origin;
				const size_t Count = From.Count();
				for (size_t K = 0; K < Count; ++K)
				{
					To.Points.push_back(static_cast<float>(From.Points[K * 3] + Off.X));
					To.Points.push_back(static_cast<float>(From.Points[K * 3 + 1] + Off.Y));
					To.Points.push_back(static_cast<float>(From.Points[K * 3 + 2] + Off.Z));
					To.Normals.insert(To.Normals.end(), From.Normals.begin() + static_cast<std::ptrdiff_t>(K * 3),
						From.Normals.begin() + static_cast<std::ptrdiff_t>(K * 3 + 3));
					To.Colours.insert(To.Colours.end(), From.Colours.begin() + static_cast<std::ptrdiff_t>(K * 4),
						From.Colours.begin() + static_cast<std::ptrdiff_t>(K * 4 + 4));
					To.Soot.push_back(From.Soot[K]);
				}
				for (uint32_t I : From.Triangles)
				{
					To.Triangles.push_back(Base + I);
				}
			}
			Piece.FirstPoint.push_back(static_cast<uint32_t>(To.Count()));
			Piece.FirstTriangle.push_back(static_cast<uint32_t>(To.Triangles.size() / 3));
		}

		// The commander's lid up: the piece that holds it once more, every body of it as it stands at the end of her
		// open_up clip (each part's turn and slide from the clip, the rest at rest), carried into the anchor's frame as
		// the anchor stands then (it does not move: the clip moves the lid and what hangs on it).
		const Clip* OpenUp = M.FindClip("open_up");
		if (M.CommanderHatch < 0 || !OpenUp)
		{
			return;
		}
		for (size_t B = 0; B < N && Out.OpenOf < 0; ++B)
		{
			if (Out.PieceOf[B] >= 0 && Under(M.Bodies[B].Part, M.CommanderHatch))
			{
				Out.OpenOf = Out.PieceOf[B];
			}
		}
		if (Out.OpenOf < 0)
		{
			return;
		}
		for (const ClipTrack& T : OpenUp->Tracks)
		{
			for (size_t B = 0; B < N; ++B)
			{
				if (Out.PieceOf[B] >= 0 && Out.PieceOf[B] != Out.OpenOf && Under(M.Bodies[B].Part, T.Part))
				{
					Out.OpenOf = -1; // (the clip moves a body of the other piece too: no lid piece, she shows it shut)
					return;
				}
			}
		}
		std::vector<PartState> S(M.Parts.size());
		M.Play(*OpenUp, OpenUp->Long, 1.0, S);
		std::vector<Xf> Posed;
		M.Solve(Xf(), S, Posed);
		const FarPiece& Shut = Out.Pieces[Out.OpenOf];
		FarPiece& Up = Out.Open;
		Up.Anchor = Shut.Anchor;
		Up.Bodies = Shut.Bodies;
		Up.FirstPoint = Shut.FirstPoint;
		Up.FirstTriangle = Shut.FirstTriangle;
		Up.Merged = Shut.Merged;
		const Body& AnchorBody = M.Bodies[static_cast<size_t>(Up.Anchor)];
		const Xf& A = Posed[static_cast<size_t>(AnchorBody.Part)];
		const Quat Back = A.Q.Unit().Conj();
		for (size_t K = 0; K < Up.Bodies.size(); ++K)
		{
			const Body& Bo = M.Bodies[static_cast<size_t>(Up.Bodies[K])];
			const Xf& X = Posed[static_cast<size_t>(Bo.Part)];
			const Mesh& From = Out.Level(M, Up.Bodies[K]);
			const size_t Base = Up.FirstPoint[K];
			for (size_t P = 0; P < From.Count(); ++P)
			{
				const V3 Local(From.Points[P * 3], From.Points[P * 3 + 1], From.Points[P * 3 + 2]);
				const V3 At = Back.Rotate(X.Apply(Bo.Pivot + Local) - A.T) - AnchorBody.Pivot;
				const V3 Way = Back.Rotate(X.Q.Rotate(V3(From.Normals[P * 3], From.Normals[P * 3 + 1], From.Normals[P * 3 + 2])));
				const size_t I = (Base + P) * 3;
				Up.Merged.Points[I] = static_cast<float>(At.X);
				Up.Merged.Points[I + 1] = static_cast<float>(At.Y);
				Up.Merged.Points[I + 2] = static_cast<float>(At.Z);
				Up.Merged.Normals[I] = static_cast<float>(Way.X);
				Up.Merged.Normals[I + 1] = static_cast<float>(Way.Y);
				Up.Merged.Normals[I + 2] = static_cast<float>(Way.Z);
			}
		}
	}

	double FarOff(const Model& M, const FarSplit& Split, const std::vector<Place>& Now, std::vector<double>* PerBody)
	{
		if (PerBody)
		{
			PerBody->assign(M.Bodies.size(), 0.0);
		}
		double Worst = 0.0;
		for (const FarPiece& Piece : Split.Pieces)
		{
			if (Piece.Empty() || Piece.Anchor < 0 || static_cast<size_t>(Piece.Anchor) >= Now.size())
			{
				continue;
			}
			const Place& A = Now[static_cast<size_t>(Piece.Anchor)];
			const Quat Back = A.Q.Unit().Conj();
			const V3 Origin = M.Bodies[static_cast<size_t>(Piece.Anchor)].Pivot;
			for (int B : Piece.Bodies)
			{
				const size_t Bi = static_cast<size_t>(B);
				if (Bi >= Now.size() || Split.Loose[Bi] == 2)
				{
					continue;
				}
				// Its pivot on the anchor's frame against where the piece has it, and its turn against none (a point
				// Reach from the pivot moves by at most 2 sin(turn / 2) Reach).
				const Place& P = Now[Bi];
				const V3 Shift = Back.Rotate(Swap(P.At - A.At)) - (M.Bodies[Bi].Pivot - Origin);
				double Off = Shift.Length();
				if (Split.Loose[Bi] == 0)
				{
					const Quat Turn = Back * P.Q.Unit();
					Off += 2.0 * std::sqrt(Turn.X * Turn.X + Turn.Y * Turn.Y + Turn.Z * Turn.Z) * Split.Reach[Bi];
				}
				if (PerBody)
				{
					(*PerBody)[Bi] = Off;
				}
				Worst = std::max(Worst, Off);
			}
		}
		return Worst;
	}
}
