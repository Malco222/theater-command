#include "Battle/LandMen.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <set>
#include <tuple>
#include <utility>

namespace LandMen
{
	namespace
	{

		// ---------------------------------------------------------------------------------------------------------
		// A lean JSON reader. The soldier files are 1.5 to 2.6 MB, almost all of it long lists of plain numbers (the
		// meshes' points and the clips' keys), and the recording is 4 MB. A reader that makes a value for every number
		// spends most of its time and memory on those (AirFeed's took 60 ms and some 50 MB for soldier_GER.json), so
		// here a list of plain numbers is kept as one flat list of doubles, and only the rest is a tree.
		// ---------------------------------------------------------------------------------------------------------
		struct J
		{
			enum Type : uint8_t { Null, Bool, Num, Str, Arr, Obj };
			Type K = Null;
			bool B = false;
			double N = 0.0;
			std::string S;
			std::vector<J> Items;        // an array's values, unless they are all plain numbers
			std::vector<double> Nums;    // an array of plain numbers, flat
			bool bNums = false;
			std::vector<std::string> Keys;
			std::vector<J> Vals;

			const J& operator[](const char* Key) const
			{
				if (K == Obj)
				{
					for (size_t I = 0; I < Keys.size(); ++I)
					{
						if (Keys[I] == Key) return Vals[I];
					}
				}
				return None();
			}
			const J& At(size_t I) const { return K == Arr && !bNums && I < Items.size() ? Items[I] : None(); }
			size_t Size() const { return K == Arr ? (bNums ? Nums.size() : Items.size()) : K == Obj ? Vals.size() : 0; }
			/** An array's I-th number, whichever way the array is kept. */
			double Number(size_t I, double Default = 0.0) const
			{
				if (K != Arr) return Default;
				if (bNums) return I < Nums.size() ? Nums[I] : Default;
				return I < Items.size() && Items[I].K == Num ? Items[I].N : Default;
			}
			bool Has(const char* Key) const { return !(*this)[Key].IsNull(); }
			bool IsNull() const { return K == Null; }
			double Num0(double Default = 0.0) const { return K == Num ? N : K == Bool ? (B ? 1.0 : 0.0) : Default; }
			std::string Text(const char* Default = "") const { return K == Str ? S : std::string(Default); }
			static const J& None()
			{
				static const J Empty;
				return Empty;
			}
		};

		class Reader
		{
		public:
			Reader(const std::string& Source) : P(Source.data()), E(Source.data() + Source.size()) {}
			bool Read(J& Out, std::string& OutError)
			{
				if (!Value(Out)) { OutError = Error.empty() ? std::string("bad JSON") : Error; return false; }
				return true;
			}
		protected:
			const char* P = nullptr;
			const char* E = nullptr;
			std::string Error;
			int Depth = 0;

			void Ws()
			{
				while (P < E && (*P == ' ' || *P == '\n' || *P == '\r' || *P == '\t')) ++P;
			}
			bool Fail(const char* Why)
			{
				if (Error.empty()) Error = Why;
				return false;
			}
			static double Pow10(int X)
			{
				static const double Table[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
					1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };
				return X >= 0 && X <= 22 ? Table[X] : std::pow(10.0, X);
			}
			bool Number(double& Out)
			{
				const char* Start = P;
				bool bNeg = false;
				if (P < E && *P == '-') { bNeg = true; ++P; }
				uint64_t Mant = 0;
				int Digits = 0, Exp10 = 0;
				bool bAny = false;
				while (P < E && *P >= '0' && *P <= '9')
				{
					bAny = true;
					if (Digits < 18) { Mant = Mant * 10 + static_cast<uint64_t>(*P - '0'); if (Mant) ++Digits; }
					else ++Exp10;
					++P;
				}
				if (P < E && *P == '.')
				{
					++P;
					while (P < E && *P >= '0' && *P <= '9')
					{
						bAny = true;
						if (Digits < 18) { Mant = Mant * 10 + static_cast<uint64_t>(*P - '0'); if (Mant) ++Digits; --Exp10; }
						++P;
					}
				}
				if (!bAny) { P = Start; return Fail("a bad number"); }
				if (P < E && (*P == 'e' || *P == 'E'))
				{
					++P;
					bool bNegExp = false;
					if (P < E && (*P == '+' || *P == '-')) { bNegExp = *P == '-'; ++P; }
					int X = 0;
					while (P < E && *P >= '0' && *P <= '9') { if (X < 10000) X = X * 10 + (*P - '0'); ++P; }
					Exp10 += bNegExp ? -X : X;
				}
				double V = static_cast<double>(Mant);
				if (Exp10 > 0) V *= Pow10(Exp10);
				else if (Exp10 < 0) V /= Pow10(-Exp10);
				Out = bNeg ? -V : V;
				return true;
			}
			bool String(std::string& Out)
			{
				++P; // the opening quote
				Out.clear();
				while (P < E)
				{
					const char C = *P++;
					if (C == '"') return true;
					if (C != '\\') { Out.push_back(C); continue; }
					if (P >= E) break;
					const char Esc = *P++;
					switch (Esc)
					{
					case 'n': Out.push_back('\n'); break;
					case 't': Out.push_back('\t'); break;
					case 'r': Out.push_back('\r'); break;
					case 'b': Out.push_back('\b'); break;
					case 'f': Out.push_back('\f'); break;
					case 'u':
					{
						// (Kept as UTF-8. The soldier files and recordings carry the odd accented letter in their notes.)
						if (E - P < 4) return Fail("a short \\u escape");
						unsigned Code = static_cast<unsigned>(std::strtoul(std::string(P, P + 4).c_str(), nullptr, 16));
						P += 4;
						if (Code < 0x80) Out.push_back(static_cast<char>(Code));
						else if (Code < 0x800)
						{
							Out.push_back(static_cast<char>(0xC0 | (Code >> 6)));
							Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
						}
						else
						{
							Out.push_back(static_cast<char>(0xE0 | (Code >> 12)));
							Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
							Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
						}
						break;
					}
					default: Out.push_back(Esc); break;
					}
				}
				return Fail("a string left open");
			}
			bool Value(J& Out)
			{
				Ws();
				if (P >= E) return Fail("the text ended early");
				if (++Depth > 128) return Fail("nested too deep");
				bool bOk = true;
				const char C = *P;
				if (C == '{')
				{
					Out.K = J::Obj;
					++P;
					Ws();
					if (P < E && *P == '}') ++P;
					else
					{
						while (bOk)
						{
							Ws();
							if (P >= E || *P != '"') { bOk = Fail("a name expected"); break; }
							Out.Keys.emplace_back();
							if (!String(Out.Keys.back())) { bOk = false; break; }
							Ws();
							if (P >= E || *P != ':') { bOk = Fail("a colon expected"); break; }
							++P;
							Out.Vals.emplace_back();
							if (!Value(Out.Vals.back())) { bOk = false; break; }
							Ws();
							if (P < E && *P == ',') { ++P; continue; }
							if (P < E && *P == '}') { ++P; break; }
							bOk = Fail("a comma or a brace expected");
						}
					}
				}
				else if (C == '[')
				{
					Out.K = J::Arr;
					Out.bNums = true;
					++P;
					Ws();
					if (P < E && *P == ']') ++P;
					else
					{
						while (bOk)
						{
							Ws();
							if (P >= E) { bOk = Fail("a list left open"); break; }
							const char D = *P;
							if (Out.bNums && (D == '-' || (D >= '0' && D <= '9')))
							{
								double V = 0.0;
								if (!Number(V)) { bOk = false; break; }
								Out.Nums.push_back(V);
							}
							else
							{
								if (Out.bNums)
								{
									// Not all numbers after all: what was read goes into the tree.
									Out.bNums = false;
									Out.Items.reserve(Out.Nums.size() + 4);
									for (const double V : Out.Nums)
									{
										J Item;
										Item.K = J::Num;
										Item.N = V;
										Out.Items.push_back(std::move(Item));
									}
									Out.Nums.clear();
								}
								Out.Items.emplace_back();
								if (!Value(Out.Items.back())) { bOk = false; break; }
							}
							Ws();
							if (P < E && *P == ',') { ++P; continue; }
							if (P < E && *P == ']') { ++P; break; }
							bOk = Fail("a comma or a bracket expected");
						}
					}
				}
				else if (C == '"')
				{
					Out.K = J::Str;
					bOk = String(Out.S);
				}
				else if (C == 't' || C == 'f')
				{
					const bool bTrue = C == 't';
					const size_t Len = bTrue ? 4 : 5;
					if (static_cast<size_t>(E - P) < Len || std::strncmp(P, bTrue ? "true" : "false", Len) != 0) bOk = Fail("a bad word");
					else { P += Len; Out.K = J::Bool; Out.B = bTrue; }
				}
				else if (C == 'n')
				{
					if (E - P < 4 || std::strncmp(P, "null", 4) != 0) bOk = Fail("a bad word");
					else { P += 4; Out.K = J::Null; }
				}
				else
				{
					Out.K = J::Num;
					bOk = Number(Out.N);
				}
				--Depth;
				return bOk;
			}
		};

		bool ReadJson(const std::string& Text, J& Out, std::string& OutError)
		{
			Reader R(Text);
			return R.Read(Out, OutError);
		}

		/**
		 * JSON read as it goes, for a live battle's deltas: Extend runs on the game thread between frames, and a delta of
		 * a few hundred kilobytes read into a tree costs milliseconds, most of them making and freeing its nodes. Here
		 * the caller walks the members and lists it wants and the rest is skipped; numbers are read as the tree reads
		 * them (Reader's own), so a delta gives exactly what the same text read whole would.
		 */
		/** A name is this word (the length known at compile time: no strlen, as == with a C string would make). */
		template <size_t N> inline bool Is(const std::string& Name, const char (&Word)[N])
		{
			return Name.size() == N - 1 && std::memcmp(Name.data(), Word, N - 1) == 0;
		}

		class Scan : public Reader
		{
		public:
			explicit Scan(const std::string& Source) : Reader(Source) {}

			/** A plain value as the tree would hold it (a list or an object is skipped and only its kind kept). */
			struct Plain
			{
				J::Type K = J::Null;
				bool B = false;
				double N = 0.0;
				std::string S;
				/** As J::Num0: a number, a truth as 1 or 0, else the default. */
				double Num0(double Default = 0.0) const { return K == J::Num ? N : K == J::Bool ? (B ? 1.0 : 0.0) : Default; }
				/** As J::Number on a list's item: a number, else the default. */
				double Number(double Default = 0.0) const { return K == J::Num ? N : Default; }
				std::string Text(const char* Default = "") const { return K == J::Str ? S : std::string(Default); }
			};

			const std::string& Problem() const { return Error; }

			/** Each member of an object, its name to Each, which reads its value (or skips it). */
			template <typename F> bool Members(F&& Each)
			{
				Ws();
				if (P >= E || *P != '{') return Fail("an object expected");
				++P;
				Ws();
				if (P < E && *P == '}') { ++P; return true; }
				std::string Key;    // (its own: Each may read an object within)
				for (;;)
				{
					Ws();
					if (P >= E || *P != '"') return Fail("a name expected");
					if (!String(Key)) return false;
					Ws();
					if (P >= E || *P != ':') return Fail("a colon expected");
					++P;
					if (!Each(static_cast<const std::string&>(Key))) return false;
					Ws();
					if (P < E && *P == ',') { ++P; continue; }
					if (P < E && *P == '}') { ++P; return true; }
					return Fail("a comma or a brace expected");
				}
			}

			/** Each item of a list, its number to Each, which reads it (or skips it). */
			template <typename F> bool Items(F&& Each)
			{
				Ws();
				if (P >= E || *P != '[') return Fail("a list expected");
				++P;
				Ws();
				if (P < E && *P == ']') { ++P; return true; }
				for (size_t I = 0;; ++I)
				{
					if (!Each(I)) return false;
					Ws();
					if (P < E && *P == ',') { ++P; continue; }
					if (P < E && *P == ']') { ++P; return true; }
					return Fail("a comma or a bracket expected");
				}
			}

			/** Whether the next value is a list, or an object. */
			bool AtList()
			{
				Ws();
				return P < E && *P == '[';
			}
			bool AtObject()
			{
				Ws();
				return P < E && *P == '{';
			}

			/** A plain value (a list or an object skipped). */
			bool Value(Plain& Out)
			{
				Ws();
				Out.K = J::Null;
				if (P >= E) return Fail("the text ended early");
				const char C = *P;
				if (C == '"') { Out.K = J::Str; return String(Out.S); }
				if (C == '{' || C == '[') { Out.K = C == '{' ? J::Obj : J::Arr; return Skip(); }
				if (C == 't' || C == 'f' || C == 'n')
				{
					const char* Word = C == 't' ? "true" : C == 'f' ? "false" : "null";
					const size_t Len = std::strlen(Word);
					if (static_cast<size_t>(E - P) < Len || std::strncmp(P, Word, Len) != 0) return Fail("a bad word");
					P += Len;
					if (C != 'n') { Out.K = J::Bool; Out.B = C == 't'; }
					return true;
				}
				Out.K = J::Num;
				return Number(Out.N);
			}

			/** Any value, passed over. */
			bool Skip()
			{
				Ws();
				if (P >= E) return Fail("the text ended early");
				if (*P == '"') return SkipString();
				if (*P == '{' || *P == '[')
				{
					int Open = 0;
					while (P < E)
					{
						const char C = *P;
						if (C == '"') { if (!SkipString()) return false; continue; }
						++P;
						if (C == '{' || C == '[') ++Open;
						else if ((C == '}' || C == ']') && --Open == 0) return true;
					}
					return Fail("a list left open");
				}
				const char* From = P;
				while (P < E && *P != ',' && *P != '}' && *P != ']' && *P != ' ' && *P != '\n' && *P != '\r' && *P != '\t') ++P;
				return P > From || Fail("a value expected");
			}

		private:
			bool SkipString()
			{
				++P;
				while (P < E)
				{
					const char C = *P++;
					if (C == '"') return true;
					if (C == '\\') ++P;
				}
				return Fail("a string left open");
			}
		};

		// ---------------------------------------------------------------------------------------------------------
		// Small maths.
		// ---------------------------------------------------------------------------------------------------------
		inline Quat QNormal(const Quat& Q)
		{
			const float L = std::sqrt(Q.W * Q.W + Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z);
			if (L < 1e-12f) return Quat();
			const float I = 1.f / L;
			return { Q.W * I, Q.X * I, Q.Y * I, Q.Z * I };
		}

		inline Quat QSlerp(const Quat& A, Quat B, float T)
		{
			float Dot = A.W * B.W + A.X * B.X + A.Y * B.Y + A.Z * B.Z;
			if (Dot < 0.f) { B = { -B.W, -B.X, -B.Y, -B.Z }; Dot = -Dot; }
			float WA = 1.f - T, WB = T;
			if (Dot < 0.9995f)
			{
				// (Keys 15 to a second are close together, so most turns take the straight path below; a blend between
				// two clips can be far apart, and those are slerped.)
				const float Theta = std::acos(std::min(Dot, 1.f));
				const float S = std::sin(Theta);
				WA = std::sin((1.f - T) * Theta) / S;
				WB = std::sin(T * Theta) / S;
			}
			return QNormal({ A.W * WA + B.W * WB, A.X * WA + B.X * WB, A.Y * WA + B.Y * WB, A.Z * WA + B.Z * WB });
		}

		inline void QMat(const Quat& Q, float* R)
		{
			const float W = Q.W, X = Q.X, Y = Q.Y, Z = Q.Z;
			R[0] = 1.f - 2.f * (Y * Y + Z * Z); R[1] = 2.f * (X * Y - W * Z);       R[2] = 2.f * (X * Z + W * Y);
			R[3] = 2.f * (X * Y + W * Z);       R[4] = 1.f - 2.f * (X * X + Z * Z); R[5] = 2.f * (Y * Z - W * X);
			R[6] = 2.f * (X * Z - W * Y);       R[7] = 2.f * (Y * Z + W * X);       R[8] = 1.f - 2.f * (X * X + Y * Y);
		}

		inline void M3Mul(const float* A, const float* B, float* Out)
		{
			for (int R = 0; R < 3; ++R)
			{
				for (int C = 0; C < 3; ++C)
				{
					Out[R * 3 + C] = A[R * 3] * B[C] + A[R * 3 + 1] * B[3 + C] + A[R * 3 + 2] * B[6 + C];
				}
			}
		}

		inline Vec3 M3Vec(const float* R, const Vec3& V)
		{
			return { R[0] * V.X + R[1] * V.Y + R[2] * V.Z, R[3] * V.X + R[4] * V.Y + R[5] * V.Z, R[6] * V.X + R[7] * V.Y + R[8] * V.Z };
		}

		inline Vec3 Sub(const Vec3& A, const Vec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }

		/** A then B as turns: (A * B)(v) = A(B(v)). */
		inline Quat QMul(const Quat& A, const Quat& B)
		{
			return { A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z, A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
				A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X, A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W };
		}

		/** The turn of a 3x3 (row by row), as QMat writes it. */
		inline Quat QOfMat(const float* R)
		{
			Quat Q;
			const float Tr = R[0] + R[4] + R[8];
			if (Tr > 0.f)
			{
				const float S = std::sqrt(Tr + 1.f) * 2.f;
				Q = { 0.25f * S, (R[7] - R[5]) / S, (R[2] - R[6]) / S, (R[3] - R[1]) / S };
			}
			else if (R[0] > R[4] && R[0] > R[8])
			{
				const float S = std::sqrt(1.f + R[0] - R[4] - R[8]) * 2.f;
				Q = { (R[7] - R[5]) / S, 0.25f * S, (R[1] + R[3]) / S, (R[2] + R[6]) / S };
			}
			else if (R[4] > R[8])
			{
				const float S = std::sqrt(1.f + R[4] - R[0] - R[8]) * 2.f;
				Q = { (R[2] - R[6]) / S, (R[1] + R[3]) / S, 0.25f * S, (R[5] + R[7]) / S };
			}
			else
			{
				const float S = std::sqrt(1.f + R[8] - R[0] - R[4]) * 2.f;
				Q = { (R[3] - R[1]) / S, (R[2] + R[6]) / S, (R[5] + R[7]) / S, 0.25f * S };
			}
			return QNormal(Q);
		}
		inline Vec3 Add(const Vec3& A, const Vec3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		inline Vec3 Scale(const Vec3& A, float K) { return { A.X * K, A.Y * K, A.Z * K }; }
		inline float Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		inline Vec3 Cross(const Vec3& A, const Vec3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		inline float Length(const Vec3& A) { return std::sqrt(Dot(A, A)); }
		inline Vec3 Unit(const Vec3& A)
		{
			const float L = Length(A);
			return L > 1e-9f ? Scale(A, 1.f / L) : Vec3 { 0.f, 0.f, 1.f };
		}

		/** A turn about a unit axis (Rodrigues), as a 3x3. */
		void AxisTurn(const Vec3& K, float Angle, float* R)
		{
			const float C = std::cos(Angle), S = std::sin(Angle), T = 1.f - C;
			R[0] = C + K.X * K.X * T;       R[1] = K.X * K.Y * T - K.Z * S; R[2] = K.X * K.Z * T + K.Y * S;
			R[3] = K.Y * K.X * T + K.Z * S; R[4] = C + K.Y * K.Y * T;       R[5] = K.Y * K.Z * T - K.X * S;
			R[6] = K.Z * K.X * T - K.Y * S; R[7] = K.Z * K.Y * T + K.X * S; R[8] = C + K.Z * K.Z * T;
		}

		/** A turn about a point: the frame Rot(p - Pivot) + Pivot. */
		Xform TurnAbout(const Vec3& Pivot, const float* R)
		{
			Xform X;
			std::memcpy(X.R, R, sizeof(X.R));
			const Vec3 RP = M3Vec(R, Pivot);
			X.T[0] = Pivot.X - RP.X;
			X.T[1] = Pivot.Y - RP.Y;
			X.T[2] = Pivot.Z - RP.Z;
			return X;
		}

		inline float Clamp01(float X) { return X < 0.f ? 0.f : X > 1.f ? 1.f : X; }
		inline float Smooth(float X) { X = Clamp01(X); return X * X * (3.f - 2.f * X); }

		/** The angle from A to B the short way round. */
		inline double Turn(double A, double B)
		{
			double D = B - A;
			while (D > 3.14159265358979) D -= 6.28318530717959;
			while (D < -3.14159265358979) D += 6.28318530717959;
			return D;
		}

		/** A whole number mixed into another, the same on every machine (splitmix64): the men's own dice. */
		inline uint64_t Mix(uint64_t X)
		{
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			return X ^ (X >> 31);
		}
		inline double Unit01(uint64_t X) { return static_cast<double>(Mix(X) >> 11) * (1.0 / 9007199254740992.0); }

		/** Whether an army's section leaders carried a sub-machine gun as their issue in 1944: the German Gruppenfuehrer
		 * his MP 40, the Soviet his PPSh-41, the British and Commonwealth (and the Polish in the west) his Sten. The
		 * American squad leader's was the M1 rifle (a Thompson was a substitute, not his issue). */
		bool LeaderCarriesSmg(const std::string& Army)
		{
			return Army == "GER" || Army == "SOV" || Army == "GBR" || Army == "UK" || Army == "CAN" || Army == "POL";
		}

		uint32_t Hex(const std::string& S, uint32_t Default)
		{
			if (S.size() < 6) return Default;
			// (the six characters kept in a named string: End points into them, and a temporary would be gone before
			// it is read. Pre-check's sanitizer run on the sailor files, 22 November)
			const std::string Six = S.substr(S.size() - 6);
			char* End = nullptr;
			const unsigned long V = std::strtoul(Six.c_str(), &End, 16);
			return End && *End == 0 ? static_cast<uint32_t>(V) : Default;
		}

		int MonthOf(const std::string& YyyyMm)
		{
			// "1944-07" -> 1944 * 12 + 6.
			if (YyyyMm.size() < 7) return 0;
			return std::atoi(YyyyMm.substr(0, 4).c_str()) * 12 + std::atoi(YyyyMm.substr(5, 2).c_str()) - 1;
		}

		void ReadMesh(const J& Src, Mesh& Out, bool bInk, bool bWear)
		{
			const J& Pt = Src["points"];
			Out.Points = static_cast<int>(Pt.Size() / 3);
			const size_t N = static_cast<size_t>(Out.Points);
			Out.At.resize(N * 3);
			Out.Facing.resize(N * 3);
			for (size_t I = 0; I < N * 3; ++I) Out.At[I] = static_cast<float>(Pt.Number(I));
			const J& Fa = Src["facings"];
			for (size_t I = 0; I < N * 3; ++I) Out.Facing[I] = static_cast<float>(Fa.Number(I, I % 3 == 2 ? 1.0 : 0.0));
			Out.Paint.resize(N);
			Out.Part.resize(N);
			Out.Part2.resize(N);
			Out.W2.resize(N);
			const J& Pa = Src["paints"];
			const J& P1 = Src["part"];
			const J& P2 = Src["part2"];
			const J& W2 = Src["w2"];
			for (size_t I = 0; I < N; ++I)
			{
				Out.Paint[I] = static_cast<uint8_t>(Pa.Number(I));
				Out.Part[I] = static_cast<uint8_t>(P1.Number(I));
				Out.Part2[I] = static_cast<uint8_t>(P2.Number(I, P1.Number(I)));
				Out.W2[I] = static_cast<float>(W2.Number(I));
			}
			const J& Sh = Src["shade"];
			if (Sh.Size() == N)
			{
				Out.Shade.resize(N);
				for (size_t I = 0; I < N; ++I) Out.Shade[I] = static_cast<float>(Sh.Number(I, 1.0));
			}
			// (the soldier files' tint, beside shade: an rgb a point, mud browner and dust paler; held to a sane span)
			const J& Ti = Src["tint"];
			if (Ti.Size() == N * 3)
			{
				Out.Tint.resize(N * 3);
				for (size_t I = 0; I < N * 3; ++I) Out.Tint[I] = static_cast<float>(std::min(2.0, std::max(0.0, Ti.Number(I, 1.0))));
			}
			// (release 2's wear and weather, tc.Look.Worn, only where asked: a byte a point each, the value times 255; an
			// array kept only whole, so a file without them, or with a short one, keeps the look it had)
			if (bWear)
			{
				const auto Bytes = [&Src, N](const char* Key, double Default, std::vector<uint8_t>& Into) {
					const J& A = Src[Key];
					if (A.Size() != N) return;
					Into.resize(N);
					for (size_t I = 0; I < N; ++I) Into[I] = static_cast<uint8_t>(std::lround(std::clamp(A.Number(I, Default), 0.0, 1.0) * 255.0));
				};
				Bytes("shade_clean", 1.0, Out.ShadeClean);
				Bytes("dirt", 0.0, Out.Dirt);
				Bytes("dirt_wet", 0.0, Out.DirtWet);
				Bytes("wet", 0.0, Out.Wet);
				Bytes("snow", 0.0, Out.Snow);
			}
			if (bInk)
			{
				const J& Ik = Src["ink"];
				if (Ik.Size() == N)
				{
					Out.Ink.resize(N);
					for (size_t I = 0; I < N; ++I) Out.Ink[I] = static_cast<int16_t>(Ik.Number(I, -1.0));
				}
			}
			const J& Tr = Src["triangles"];
			Out.Triangles.resize(Tr.Size());
			for (size_t I = 0; I < Tr.Size(); ++I) Out.Triangles[I] = static_cast<uint32_t>(Tr.Number(I));
		}

		Vec3 V3(const J& A, Vec3 Default = {})
		{
			if (A.Size() < 3) return Default;
			return { static_cast<float>(A.Number(0)), static_cast<float>(A.Number(1)), static_cast<float>(A.Number(2)) };
		}
	}

	// -------------------------------------------------------------------------------------------------------------
	// Frames.
	// -------------------------------------------------------------------------------------------------------------
	Vec3 Xform::Point(const Vec3& P) const
	{
		return { R[0] * P.X + R[1] * P.Y + R[2] * P.Z + T[0], R[3] * P.X + R[4] * P.Y + R[5] * P.Z + T[1],
			R[6] * P.X + R[7] * P.Y + R[8] * P.Z + T[2] };
	}

	Vec3 Xform::Turn(const Vec3& V) const
	{
		return M3Vec(R, V);
	}

	Xform Compose(const Xform& A, const Xform& B)
	{
		Xform Out;
		M3Mul(A.R, B.R, Out.R);
		const Vec3 T = A.Point({ B.T[0], B.T[1], B.T[2] });
		Out.T[0] = T.X;
		Out.T[1] = T.Y;
		Out.T[2] = T.Z;
		return Out;
	}

	const std::vector<std::string>& PaintNames()
	{
		static const std::vector<std::string> Names = { "tunic", "trousers", "boots", "lower", "straps", "helmet", "pack", "skin",
			"wood", "steel", "coat", "coat_dark", "leather", "blanket", "hoof" };
		return Names;
	}

	Weapon WeaponOf(const std::string& W)
	{
		if (W == "rifle") return Weapon::Rifle;
		if (W == "mg") return Weapon::Mg;
		if (W == "at") return Weapon::At;
		if (W == "grenade") return Weapon::Grenade;
		if (W == "mortar") return Weapon::Mortar;
		return Weapon::Other;
	}

	const char* WeaponName(Weapon W)
	{
		switch (W)
		{
		case Weapon::Rifle: return "rifle";
		case Weapon::Mg: return "mg";
		case Weapon::At: return "at";
		case Weapon::Grenade: return "grenade";
		case Weapon::Mortar: return "mortar";
		case Weapon::Other: break;
		}
		return "other";
	}

	// -------------------------------------------------------------------------------------------------------------
	// The soldier file.
	// -------------------------------------------------------------------------------------------------------------
	bool Soldier::Load(const std::string& Text, std::string& OutError, bool bWear)
	{
		J Root;
		if (!ReadJson(Text, Root, OutError)) return false;
		Name = Root["name"].Text();
		Weapon = Root["weapon"].Text("rifle");
		// The file's own paints past the army's fifteen (new_paints: the medic's white and red, the sergeant's
		// chevrons, the bazooka's rocket), each in the file's own colour: the armies' table names none of them.
		OwnPaints.clear();
		OwnPaintNames.clear();
		const J& Np = Root["new_paints"];
		for (size_t I = 0; I < Np.Size() && Np.K == J::Obj; ++I)
		{
			const int Ix = static_cast<int>(Np.Vals[I].Num0(-1.0));
			const J& Rgb = Root["colours"][Np.Keys[I].c_str()];
			if (Ix >= static_cast<int>(PaintNames().size()) && Ix < 256 && Rgb.K == J::Str) OwnPaints[Ix] = Hex(Rgb.S, 0x808080);
			// (and its name, for the worn look's roughness and its gore paints: only with the wear)
			if (bWear && Ix >= static_cast<int>(PaintNames().size()) && Ix < 256) OwnPaintNames[Ix] = Np.Keys[I];
		}

		const J& Bs = Root["bones"];
		if (Bs.Size() == 0 || Bs.Size() > static_cast<size_t>(MaxBones))
		{
			OutError = "the soldier file has no bones, or more than the crowd can carry";
			return false;
		}
		Bones.clear();
		for (size_t I = 0; I < Bs.Size(); ++I)
		{
			const J& B = Bs.At(I);
			Bone Bo;
			Bo.Name = B["name"].Text();
			Bo.Parent = B["parent"].IsNull() ? -1 : static_cast<int>(B["parent"].Num0(-1));
			Bo.Head = V3(B["head"]);
			// (The skinning runs parents first, in the file's order; every file writes a bone after its parent.)
			if (Bo.Parent >= static_cast<int>(I))
			{
				OutError = "a bone comes before its parent: " + Bo.Name;
				return false;
			}
			Bones.push_back(Bo);
		}
		OnRoot.clear();
		for (size_t I = 1; I < Bones.size(); ++I)
		{
			if (Bones[I].Parent == 0) OnRoot.push_back(static_cast<int>(I));
		}
		Gun = BoneIndex("weapon");
		GunBones.assign(Bones.size(), false);
		for (size_t I = 0; I < Bones.size(); ++I)
		{
			for (int B = static_cast<int>(I); B >= 0; B = Bones[static_cast<size_t>(B)].Parent)
			{
				if (B == Gun) { GunBones[I] = true; break; }
			}
		}

		Sockets.clear();
		const J& Ss = Root["sockets"];
		for (size_t I = 0; I < Ss.Size(); ++I)
		{
			const J& S = Ss.At(I);
			Socket So;
			So.Name = S["name"].Text();
			So.Bone = static_cast<int>(S["part"].Num0(0));
			So.At = V3(S["at"]);
			So.Facing = V3(S["facing"], { 1.f, 0.f, 0.f });
			Sockets.push_back(So);
		}

		Clips.clear();
		const J& Cs = Root["clips"];
		for (size_t I = 0; I < Cs.Size(); ++I)
		{
			const J& C = Cs.Vals[I];
			Clip Cl;
			Cl.Name = Cs.Keys[I];
			Cl.Weapon = C["weapon"].Text("rifle");
			Cl.Long = static_cast<float>(C["long"].Num0(1.0));
			Cl.Stride = static_cast<float>(C["stride"].Num0(0.0));
			Cl.bLoop = C["loop"].K == J::Bool ? C["loop"].B : C["loop"].Num0() != 0.0;
			Cl.StyleOf = C["style_of"].Text("");
			// (bullet hits: where the fall or wound struck him, the soldiers thread's "hit": {bone, side, from}, where given)
			if (C.Has("hit"))
			{
				Cl.Hit.Bone = C["hit"]["bone"].Text("");
				Cl.Hit.Side = C["hit"]["side"].Text("");
				Cl.Hit.From = C["hit"]["from"].Text("");
			}
			if (C["end_offset"].Size() >= 3)
			{
				Cl.bEndOffset = true;
				Cl.EndOffset = V3(C["end_offset"]);
			}
			Cl.RotOf.assign(Bones.size(), -1);
			Cl.MoveOf.assign(Bones.size(), -1);
			const J& Ts = C["tracks"];
			for (size_t K = 0; K < Ts.Size(); ++K)
			{
				const J& Tr = Ts.At(K);
				const int B = static_cast<int>(Tr["part"].Num0(-1));
				if (B < 0 || B >= static_cast<int>(Bones.size())) continue;
				const J& Rs = Tr["rot"];
				if (Rs.Size() > 0)
				{
					Keys Ks;
					for (size_t Q = 0; Q < Rs.Size(); ++Q)
					{
						const J& R = Rs.At(Q);
						Ks.T.push_back(static_cast<float>(R.Number(0)));
						Ks.Rot.push_back(QNormal({ static_cast<float>(R.Number(1, 1.0)), static_cast<float>(R.Number(2)),
							static_cast<float>(R.Number(3)), static_cast<float>(R.Number(4)) }));
					}
					Cl.RotOf[static_cast<size_t>(B)] = static_cast<int>(Cl.Rots.size());
					Cl.Rots.push_back(std::move(Ks));
				}
				const J& Ms = Tr["move"];
				if (Ms.Size() > 0)
				{
					Keys Ks;
					for (size_t Q = 0; Q < Ms.Size(); ++Q)
					{
						const J& Mv = Ms.At(Q);
						Ks.T.push_back(static_cast<float>(Mv.Number(0)));
						Ks.Move.push_back({ static_cast<float>(Mv.Number(1)), static_cast<float>(Mv.Number(2)), static_cast<float>(Mv.Number(3)) });
					}
					Cl.MoveOf[static_cast<size_t>(B)] = static_cast<int>(Cl.Moves.size());
					Cl.Moves.push_back(std::move(Ks));
				}
			}
			const J& Es = C["events"];
			for (size_t K = 0; K < Es.Size(); ++K)
			{
				const J& Ev = Es.At(K);
				ClipEvent E;
				E.T = static_cast<float>(Ev["t"].Num0());
				E.Until = static_cast<float>(Ev["until"].Num0(-1.0));
				E.Fx = Ev["fx"].Text();
				E.Cue = Ev["cue"].Text();
				E.SocketName = Ev["socket"].Text();
				E.BoneName = Ev["bone"].Text();
				E.bFirstLoopOnly = Ev["first_loop_only"].K == J::Bool ? Ev["first_loop_only"].B : false;
				Cl.Events.push_back(E);
			}
			std::sort(Cl.Events.begin(), Cl.Events.end(), [](const ClipEvent& A, const ClipEvent& B) { return A.T < B.T; });
			// (a tank clip's places: the vehicle and socket his root goes to, and its offset; the tank commanders' files)
			const J& Ps = C["places"];
			for (size_t K = 0; K < Ps.Size(); ++K)
			{
				const J& P = Ps.At(K);
				Clip::Seat St;
				St.Piece = P["piece"].Text();
				St.Socket = P["socket"].Text();
				St.Offset = V3(P["root_offset"]);
				if (!St.Piece.empty() && !St.Socket.empty()) Cl.Seats.push_back(std::move(St));
			}
			// (a prop's keys: any of the clip's fields holding "track_fields", starting with "t", and "track", a key a row;
			// and any such field of the clip's "props", where the tankers' files keep theirs: "binoculars")
			const auto ReadProps = [&Cl](const J& Holder)
			{
				for (size_t K = 0; Holder.K == J::Obj && K < Holder.Vals.size(); ++K)
				{
					const J& Pk = Holder.Vals[K];
					const J& Fs = Pk["track_fields"];
					const J& Tk = Pk["track"];
					if (Pk.K != J::Obj || Fs.Size() < 2 || Tk.Size() == 0 || Fs.At(0).Text() != "t") continue;
					Clip::PropKeys Pr;
					Pr.Name = Holder.Keys[K];
					for (size_t F = 1; F < Fs.Size(); ++F) Pr.Fields.push_back(Fs.At(F).Text());
					const size_t Width = Pr.Fields.size();
					bool bOrdered = true;
					for (size_t R = 0; R < Tk.Size(); ++R)
					{
						const J& Row = Tk.At(R);
						if (Row.Size() < Width + 1) continue;
						const float T = static_cast<float>(Row.Number(0));
						bOrdered = bOrdered && (Pr.T.empty() || T >= Pr.T.back());
						Pr.T.push_back(T);
						for (size_t F = 0; F < Width; ++F) Pr.Values.push_back(static_cast<float>(Row.Number(F + 1)));
					}
					if (!Pr.T.empty() && bOrdered && !Cl.PropOf(Pr.Name)) Cl.Props.push_back(std::move(Pr));
				}
			};
			ReadProps(C);
			ReadProps(C["props"]);
			Clips.push_back(std::move(Cl));
		}
		for (Clip& Cl : Clips)
		{
			for (ClipEvent& E : Cl.Events)
			{
				E.Socket = E.SocketName.empty() ? -1 : SocketIndex(E.SocketName);
				E.Bone = E.BoneName.empty() ? -1 : BoneIndex(E.BoneName);
				if (E.Fx == "muzzle_flash" && Cl.FlashT < 0.f) Cl.FlashT = E.T;
				if (E.Fx == "grenade_release" && Cl.ReleaseT < 0.f) Cl.ReleaseT = E.T;
				if (Cl.LeftFoot < 0.f && Cl.Long > 0.f && (E.BoneName == "foot_l" || E.BoneName == "lowerarm_l")) Cl.LeftFoot = E.T / Cl.Long;
			}
			if (Cl.LeftFoot < 0.f) Cl.LeftFoot = 0.f;
		}
		// (release 2's run styles: each clip with its styles; a style of no clip, or of a style, stands alone)
		Styles.assign(Clips.size(), std::vector<int>());
		for (size_t I = 0; I < Clips.size(); ++I)
		{
			if (Clips[I].StyleOf.empty()) continue;
			const int B = ClipIndex(Clips[I].StyleOf);
			if (B < 0 || !Clips[static_cast<size_t>(B)].StyleOf.empty() || !Clips[I].bLoop || !Clips[static_cast<size_t>(B)].bLoop) continue;
			std::vector<int>& V = Styles[static_cast<size_t>(B)];
			if (V.empty()) V.push_back(B);
			V.push_back(static_cast<int>(I));
		}

		const char* LodNames[LodCount] = { "near", "mid", "far" };
		for (int L = 0; L < LodCount; ++L)
		{
			PackFirst[L] = PackCount[L] = 0;
			const J& Src = Root[LodNames[L]];
			if (Src.IsNull())
			{
				// (An old file with no mid: the far stands in.)
				if (L == Mid && !Root["far"].IsNull()) ReadMesh(Root["far"], Meshes[L], false, bWear);
				continue;
			}
			ReadMesh(Src, Meshes[L], false, bWear);
			const J& Ht = Root["helmet_tris"][LodNames[L]];
			HelmetFirst[L] = static_cast<int>(Ht.Number(0, 0.0));
			HelmetCount[L] = static_cast<int>(Ht.Number(1, 0.0));
			// (Effects (R2): his pack and webbing's run, pack_tris, as helmet_tris; one that is not two whole numbers
			// lying within the level's triangles, or that reaches into his helmet's run, is left alone, so a man without
			// his pack is drawn whole, and a crewman with his helmet on keeps all of it)
			const J& Pk = Root["pack_tris"][LodNames[L]];
			const double First = Pk.Number(0, -1.0), Count = Pk.Number(1, 0.0);
			const double Tris = static_cast<double>(Meshes[L].Triangles.size() / 3);
			const double HelmetFrom = static_cast<double>(HelmetFirst[L]), HelmetTo = HelmetFrom + static_cast<double>(HelmetCount[L]);
			const bool bIntoHelmet = HelmetCount[L] > 0 && First < HelmetTo && HelmetFrom < First + Count;
			if (Pk.Size() == 2 && First >= 0.0 && Count >= 1.0 && First == std::floor(First) && Count == std::floor(Count) && First + Count <= Tris && !bIntoHelmet)
			{
				PackFirst[L] = static_cast<int>(First);
				PackCount[L] = static_cast<int>(Count);
			}
		}
		if (Meshes[Near].Points == 0)
		{
			OutError = "the soldier file has no near mesh";
			return false;
		}
		for (int L = 0; L < LodCount; ++L)
		{
			const Mesh& Me = Meshes[L];
			for (size_t I = 0; I < static_cast<size_t>(Me.Points); ++I)
			{
				if (Me.Part[I] >= Bones.size() || Me.Part2[I] >= Bones.size())
				{
					OutError = "a point rides on a bone the file does not have";
					return false;
				}
			}
			for (const uint32_t T : Me.Triangles)
			{
				if (T >= static_cast<uint32_t>(Me.Points))
				{
					OutError = "a triangle names a point the mesh does not have";
					return false;
				}
			}
		}

		// (release 2's worn look, only with the wear: the face's detail face_detail.py laid in each level's tint, and the
		// eyeballs' shade it set, Detail; a level's record kept only whole, every point one the level has, else none (the
		// worn face then plain skin, as a file without the record). A mid read from the far, an old file's, the far's.)
		for (int L = 0; L < LodCount; ++L) Detail[L] = FaceDetail();
		if (bWear)
		{
			const J& Recs = Root["face_detail"]["lods"];
			for (int L = 0; L < LodCount; ++L)
			{
				const J& Rec = Recs[L == Mid && Root["mid"].IsNull() ? "far" : LodNames[L]];
				const J& At = Rec["at"];
				const J& Mu = Rec["mul"];
				const J& Eyes = Rec["shade"];
				const double Pts = static_cast<double>(Meshes[L].Points);
				FaceDetail Fd;
				bool bOk = Mu.Size() == At.Size() * 3;
				for (size_t K = 0; bOk && K < At.Size(); ++K)
				{
					const double P = At.Number(K, -1.0);
					bOk = P >= 0.0 && P < Pts;
					Fd.At.push_back(static_cast<int>(P));
				}
				for (size_t K = 0; bOk && K < Mu.Size(); ++K) Fd.Mul.push_back(static_cast<float>(std::clamp(Mu.Number(K, 1.0), 0.0, 2.0)));
				for (size_t K = 0; bOk && Eyes.K == J::Obj && K < Eyes.Size(); ++K)
				{
					const double P = std::atof(Eyes.Keys[K].c_str()), Was = Eyes.Vals[K].Num0(-1.0);
					bOk = P >= 0.0 && P < Pts && Was > 0.0;
					Fd.EyeAt.push_back(static_cast<int>(P));
					Fd.EyeWas.push_back(static_cast<float>(Was));
				}
				if (!bOk) continue;
				MendEyes(Meshes[L], Fd);
				Detail[L] = std::move(Fd);
			}
		}

		Faces.clear();
		const J& Fs = Root["faces"];
		for (size_t I = 0; I < Fs.Size(); ++I)
		{
			const J& F = Fs.At(I);
			Face Fc;
			Fc.Name = F["name"].Text();
			Fc.Tone = V3(F["tone"], { 1.f, 1.f, 1.f });
			for (int L = 0; L < LodCount; ++L)
			{
				const J& Lv = F[LodNames[L]];
				Face::Level& Out = Fc.Lods[L];
				for (size_t K = 0; K < Lv["at"].Size(); ++K) Out.At.push_back(static_cast<int>(Lv["at"].Number(K)));
				for (size_t K = 0; K < Lv["move"].Size(); ++K) Out.Move.push_back(static_cast<float>(Lv["move"].Number(K)));
				for (size_t K = 0; K < Lv["facing"].Size(); ++K) Out.Facing.push_back(static_cast<float>(Lv["facing"].Number(K)));
				for (size_t K = 0; K < Lv["tint_at"].Size(); ++K) Out.TintAt.push_back(static_cast<int>(Lv["tint_at"].Number(K)));
				for (size_t K = 0; K < Lv["tint"].Size(); ++K) Out.Tint.push_back(static_cast<float>(Lv["tint"].Number(K)));
				// (A face that would move a point the mesh does not have is left plain at that level.)
				const int Pts = Meshes[L].Points;
				bool bOk = Out.Move.size() == Out.At.size() * 3 && Out.Tint.size() == Out.TintAt.size() * 3;
				for (const int A : Out.At) bOk = bOk && A >= 0 && A < Pts;
				for (const int A : Out.TintAt) bOk = bOk && A >= 0 && A < Pts;
				if (!bOk) Out = Face::Level();
			}
			Faces.push_back(std::move(Fc));
		}
		if (Faces.empty()) Faces.emplace_back();

		Inks.clear();
		const J& Is = Root["inks"];
		for (size_t I = 0; I < Is.Size(); ++I) Inks.push_back(Hex(Is.At(I).Text(), 0x808080));

		Finishes.clear();
		const J& Hs = Root["helmets"];
		for (size_t I = 0; I < Hs.Size(); ++I)
		{
			const J& H = Hs.At(I);
			Finish Fi;
			Fi.Name = H["name"].Text();
			Fi.From = MonthOf(H["from"].Text("1900-01"));
			Fi.To = MonthOf(H["to"].Text("2100-01"));
			Fi.Season = H["season"].Text("any");
			Fi.Front = H["front"].Text("any");
			Fi.Share = H["share"].Num0(1.0);
			bool bOk = true;
			for (int L = 0; L < LodCount; ++L)
			{
				ReadMesh(H[LodNames[L]], Fi.Lods[L], true, bWear);
				for (size_t K = 0; K < static_cast<size_t>(Fi.Lods[L].Points); ++K)
				{
					bOk = bOk && Fi.Lods[L].Part[K] < Bones.size() && Fi.Lods[L].Part2[K] < Bones.size();
					if (!Fi.Lods[L].Ink.empty() && Fi.Lods[L].Ink[K] >= static_cast<int>(Inks.size())) Fi.Lods[L].Ink[K] = -1;
				}
				for (const uint32_t T : Fi.Lods[L].Triangles) bOk = bOk && T < static_cast<uint32_t>(Fi.Lods[L].Points);
			}
			if (bOk) Finishes.push_back(std::move(Fi));
		}

		// The physics-baked falls, when the soldiers thread has added them: deaths[stance][kind] = clip names. Names the
		// file does not have as clips are left out here, so the picker never chooses a fall it cannot play.
		// (and a gore twin's, deaths_gore, the same way: Effects (R2), Crowd::AddGore)
		Deaths.clear();
		DeathsGore.clear();
		for (const auto& [Key, Out] : { std::make_pair("deaths", &Deaths), std::make_pair("deaths_gore", &DeathsGore) })
		{
			const J& Ds = Root[Key];
			for (size_t I = 0; I < Ds.Size() && Ds.K == J::Obj; ++I)
			{
				const J& ByKind = Ds.Vals[I];
				for (size_t K = 0; K < ByKind.Size() && ByKind.K == J::Obj; ++K)
				{
					const J& List = ByKind.Vals[K];
					for (size_t N = 0; N < List.Size(); ++N)
					{
						const std::string Clip = List.At(N).Text();
						if (ClipIndex(Clip) >= 0) (*Out)[Ds.Keys[I]][ByKind.Keys[K]].push_back(Clip);
					}
				}
			}
		}
		// (a gore twin's falls by where each one's burst was against him, gore_from[clip]: front, behind, left, right or
		// under; a gore fall the map does not name, by its own blast_from. Effects (R2), Crowd::SetGore)
		GoreFrom.clear();
		{
			const J& Gf = Root["gore_from"];
			for (size_t I = 0; I < Gf.Size() && Gf.K == J::Obj; ++I)
			{
				const std::string Side = Gf.Vals[I].Text();
				if (!Side.empty() && ClipIndex(Gf.Keys[I]) >= 0) GoreFrom[Gf.Keys[I]] = Side;
			}
			for (const auto& St : DeathsGore)
			{
				for (const auto& Kd : St.second)
				{
					for (const std::string& Clip : Kd.second)
					{
						const std::string Side = Cs[Clip.c_str()]["blast_from"].Text();
						if (!Side.empty()) GoreFrom.emplace(Clip, Side);
					}
				}
			}
		}
		// (a gore twin's cuts and joined skin: Effects (R2); a point out of its level, or a mask past the cuts, drops
		// the whole joined list of that level, so the twin falls cracked as before rather than torn)
		Cuts.clear();
		for (Joined& Jn : Joins) Jn = Joined();
		const J& Gore = Root["gore"];
		const J& CutList = Gore["cuts"];
		for (size_t I = 0; I < CutList.Size(); ++I)
		{
			const J& Cj = CutList.At(I);
			Cut Row;
			Row.Bone = Cj["bone"].Text();
			Row.BoneIx = BoneIndex(Row.Bone);
			if (Row.BoneIx < 0 || Bones[static_cast<size_t>(Row.BoneIx)].Parent < 0) { Cuts.clear(); break; }
			for (int L = 0; L < LodCount; ++L)
			{
				const J& Lv = Cj[LodNames[L]];
				for (int K = 0; K < 2; ++K)
				{
					Row.Stump[L][K] = static_cast<int>(Lv["stump"]["cap_points"].Number(static_cast<size_t>(K)));
					Row.Limb[L][K] = static_cast<int>(Lv["limb"]["cap_points"].Number(static_cast<size_t>(K)));
				}
			}
			Cuts.push_back(std::move(Row));
		}
		const J& Joint = Gore["joined"];
		for (int L = 0; L < LodCount && !Cuts.empty(); ++L)
		{
			const J& Lv = Joint[LodNames[L]];
			const J& Pts = Lv["points"];
			const size_t N = Pts.Size();
			Joined& Jn = Joins[L];
			bool bOk = Lv["part"].Size() == N && Lv["part2"].Size() == N && Lv["w2"].Size() == N && Lv["cuts"].Size() == N;
			for (size_t K = 0; bOk && K < N; ++K)
			{
				const double P = Pts.Number(K, -1.0), Mask = Lv["cuts"].Number(K, -1.0);
				const double P1 = Lv["part"].Number(K, -1.0), P2 = Lv["part2"].Number(K, -1.0);
				bOk = P >= 0.0 && P < Meshes[L].Points && Mask >= 0.0 && Mask < static_cast<double>(1u << Cuts.size())
					&& P1 >= 0.0 && P1 < static_cast<double>(Bones.size()) && P2 >= 0.0 && P2 < static_cast<double>(Bones.size());
				if (!bOk) break;
				Jn.Point.push_back(static_cast<uint32_t>(P));
				Jn.Part.push_back(static_cast<uint8_t>(P1));
				Jn.Part2.push_back(static_cast<uint8_t>(P2));
				Jn.W2.push_back(static_cast<float>(Lv["w2"].Number(K)));
				Jn.Mask.push_back(static_cast<uint16_t>(Mask));
			}
			if (!bOk) Jn = Joined();
		}
		return true;
	}

	void Soldier::MendEyes(Mesh& Into, const FaceDetail& Fd)
	{
		// (face_detail.py set each eyeball's shade to the skin's about the eye, lighter than the socket's shadow shade.py
		// had given it, and left its clean shadow at the old: the clean shadow moved by the same ratio, so with no dirt on
		// it (the eyes have none) the worn look draws the eye as the baked look does)
		const size_t N = static_cast<size_t>(Into.Points);
		if (Into.ShadeClean.size() != N || Into.Shade.size() != N) return;
		for (size_t K = 0; K < Fd.EyeAt.size() && K < Fd.EyeWas.size(); ++K)
		{
			const size_t P = static_cast<size_t>(Fd.EyeAt[K]);
			if (P >= N || !(Fd.EyeWas[K] > 0.f)) continue;
			const double Clean = static_cast<double>(Into.ShadeClean[P]) / 255.0 * static_cast<double>(Into.Shade[P]) / static_cast<double>(Fd.EyeWas[K]);
			Into.ShadeClean[P] = static_cast<uint8_t>(std::lround(std::clamp(Clean, 0.0, 1.0) * 255.0));
		}
	}

	bool Soldier::TakeFaceDetail(const Soldier& File)
	{
		bool bTook = false;
		for (int L = 0; L < LodCount; ++L)
		{
			const FaceDetail& Fd = File.Detail[L];
			const Mesh& Fm = File.Meshes[L];
			Mesh& Me = Meshes[L];
			if ((Fd.At.empty() && Fd.EyeAt.empty()) || !Detail[L].At.empty() || !Detail[L].EyeAt.empty() || Me.Points < Fm.Points) continue;
			// (the file's points one for one: the same places and paints)
			const size_t N = static_cast<size_t>(Fm.Points);
			bool bSame = Me.Paint.size() >= N && Fm.Paint.size() >= N && Me.At.size() >= N * 3 && Fm.At.size() >= N * 3;
			for (size_t P = 0; bSame && P < N; ++P) bSame = Me.Paint[P] == Fm.Paint[P];
			for (size_t P = 0; bSame && P < N * 3; ++P) bSame = std::fabs(Me.At[P] - Fm.At[P]) < 1e-4f;
			if (!bSame) continue;
			Detail[L] = Fd;
			MendEyes(Me, Fd);
			bTook = true;
		}
		return bTook;
	}

	int Soldier::ClipIndex(const std::string& N) const
	{
		for (size_t I = 0; I < Clips.size(); ++I)
		{
			if (Clips[I].Name == N) return static_cast<int>(I);
		}
		return -1;
	}

	int Soldier::BoneIndex(const std::string& N) const
	{
		for (size_t I = 0; I < Bones.size(); ++I)
		{
			if (Bones[I].Name == N) return static_cast<int>(I);
		}
		return -1;
	}

	const Clip::PropKeys* Clip::PropOf(const std::string& Track) const
	{
		for (const PropKeys& P : Props)
		{
			if (P.Name == Track) return &P;
		}
		return nullptr;
	}

	void Clip::PropKeys::Sample(float At, bool bLooped, float Length, float* Out) const
	{
		const size_t Width = Fields.size();
		if (T.empty() || Width == 0) return;
		if (bLooped && Length > 0.f)
		{
			At = std::fmod(At, Length);
			if (At < 0.f) At += Length;
		}
		if (At <= T.front() || T.size() == 1)
		{
			std::copy_n(Values.data(), Width, Out);
			return;
		}
		if (At >= T.back())
		{
			std::copy_n(Values.data() + (T.size() - 1) * Width, Width, Out);
			return;
		}
		const size_t B = static_cast<size_t>(std::upper_bound(T.begin(), T.end(), At) - T.begin());
		const size_t A = B - 1;
		const float Span = T[B] - T[A];
		const float W = Span > 0.f ? (At - T[A]) / Span : 0.f;
		for (size_t F = 0; F < Width; ++F)
		{
			Out[F] = Values[A * Width + F] + (Values[B * Width + F] - Values[A * Width + F]) * W;
		}
	}

	const Clip::Seat* Clip::SeatOn(const std::string& Piece) const
	{
		for (const Seat& St : Seats)
		{
			if (St.Piece == Piece) return &St;
		}
		return nullptr;
	}

	int Soldier::SocketIndex(const std::string& N) const
	{
		for (size_t I = 0; I < Sockets.size(); ++I)
		{
			if (Sockets[I].Name == N) return static_cast<int>(I);
		}
		return -1;
	}

	// -------------------------------------------------------------------------------------------------------------
	// Poses.
	// -------------------------------------------------------------------------------------------------------------
	void RestPose(Pose& Out)
	{
		for (int B = 0; B < MaxBones; ++B)
		{
			Out.Rot[B] = Quat();
			Out.Move[B] = Vec3();
		}
	}

	namespace
	{
		/** The key at or before T, and how far on to the next (keys 15 a second: a guess, then a step or two). */
		inline void Bracket(const std::vector<float>& Ts, float T, size_t& OutI, float& OutU)
		{
			const size_t N = Ts.size();
			if (N <= 1 || T <= Ts[0]) { OutI = 0; OutU = 0.f; return; }
			if (T >= Ts[N - 1]) { OutI = N - 1; OutU = 0.f; return; }
			size_t I = static_cast<size_t>(std::max(0.f, T * 15.f));
			if (I > N - 2) I = N - 2;
			while (I > 0 && Ts[I] > T) --I;
			while (I + 2 < N && Ts[I + 1] <= T) ++I;
			const float Span = Ts[I + 1] - Ts[I];
			OutI = I;
			OutU = Span > 1e-6f ? Clamp01((T - Ts[I]) / Span) : 0.f;
		}
	}

	void Sample(const Clip& C, float Time, Pose& Out)
	{
		float T = Time;
		if (C.bLoop && C.Long > 0.f)
		{
			T = std::fmod(T, C.Long);
			if (T < 0.f) T += C.Long;
		}
		else T = std::min(std::max(T, 0.f), C.Long);
		const size_t NB = C.RotOf.size();
		for (size_t B = 0; B < NB && B < static_cast<size_t>(MaxBones); ++B)
		{
			const int R = C.RotOf[B];
			if (R < 0) Out.Rot[B] = Quat();
			else
			{
				const Keys& K = C.Rots[static_cast<size_t>(R)];
				size_t I = 0;
				float U = 0.f;
				Bracket(K.T, T, I, U);
				Out.Rot[B] = U > 0.f ? QSlerp(K.Rot[I], K.Rot[I + 1], U) : K.Rot[I];
			}
			const int M = C.MoveOf[B];
			if (M < 0) Out.Move[B] = Vec3();
			else
			{
				const Keys& K = C.Moves[static_cast<size_t>(M)];
				size_t I = 0;
				float U = 0.f;
				Bracket(K.T, T, I, U);
				if (U > 0.f)
				{
					const Vec3& A = K.Move[I];
					const Vec3& Bv = K.Move[I + 1];
					Out.Move[B] = { A.X + (Bv.X - A.X) * U, A.Y + (Bv.Y - A.Y) * U, A.Z + (Bv.Z - A.Z) * U };
				}
				else Out.Move[B] = K.Move[I];
			}
		}
	}

	void Blend(const Pose& A, const Pose& B, float W, Pose& Out)
	{
		if (W <= 0.f) { if (&Out != &A) Out = A; return; }
		if (W >= 1.f) { if (&Out != &B) Out = B; return; }
		for (int I = 0; I < MaxBones; ++I)
		{
			Out.Rot[I] = QSlerp(A.Rot[I], B.Rot[I], W);
			Out.Move[I] = { A.Move[I].X + (B.Move[I].X - A.Move[I].X) * W, A.Move[I].Y + (B.Move[I].Y - A.Move[I].Y) * W,
				A.Move[I].Z + (B.Move[I].Z - A.Move[I].Z) * W };
		}
	}

	void BoneFrames(const Soldier& S, const Pose& P, Xform* Out)
	{
		// The files' own rule (their `about`): G = G_parent * q, X = X_parent + G_parent * (head - head_parent + move),
		// and a point p on the bone goes to X + G * (p - head); so the frame's shift is X - G * head.
		float G[MaxBones][9];
		Vec3 X[MaxBones];
		const size_t NB = std::min(S.Bones.size(), static_cast<size_t>(MaxBones));
		for (size_t B = 0; B < NB; ++B)
		{
			const Bone& Bo = S.Bones[B];
			float Q[9];
			QMat(P.Rot[B], Q);
			if (Bo.Parent < 0)
			{
				std::memcpy(G[B], Q, sizeof(Q));
				X[B] = Add(Bo.Head, P.Move[B]);
			}
			else
			{
				const size_t Pa = static_cast<size_t>(Bo.Parent);
				M3Mul(G[Pa], Q, G[B]);
				const Vec3 Local = Add(Sub(Bo.Head, S.Bones[Pa].Head), P.Move[B]);
				X[B] = Add(X[Pa], M3Vec(G[Pa], Local));
			}
			Xform& F = Out[B];
			std::memcpy(F.R, G[B], sizeof(F.R));
			const Vec3 GH = M3Vec(G[B], Bo.Head);
			F.T[0] = X[B].X - GH.X;
			F.T[1] = X[B].Y - GH.Y;
			F.T[2] = X[B].Z - GH.Z;
		}
	}

	Xform WorldOf(const Placing& P, const GroundFn& GroundAt, float* OutGround)
	{
		// The man's own axes are x forward, y to his RIGHT, z up; the field's x east, y north, z up, and his heading is
		// clockwise from north. Forward goes to (sin f, cos f) and right to (cos f, -sin f): a mirror as well as a turn,
		// since his axes are left-handed and the field's right-handed, which is why the files' triangles, wound one way
		// in his axes, face outward the other way in the field's.
		const float Sf = std::sin(P.Facing), Cf = std::cos(P.Facing);
		const float Rf[9] = { Sf, Cf, 0.f, Cf, -Sf, 0.f, 0.f, 0.f, 1.f };
		const double PX = P.X + static_cast<double>(Rf[0] * P.Pivot.X + Rf[1] * P.Pivot.Y);
		const double PY = P.Y + static_cast<double>(Rf[3] * P.Pivot.X + Rf[4] * P.Pivot.Y);
		const Ground G = GroundAt ? GroundAt(PX, PY) : Ground();
		float Tilt[9] = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f };
		const Vec3 N = Unit(G.Normal);
		if (P.Lie > 0.f && N.Z < 0.99999f)
		{
			// Lying, he lies along the slope: the turn that takes straight up onto the ground's normal, as far as he lies.
			const Vec3 Axis = Unit(Cross({ 0.f, 0.f, 1.f }, N));
			AxisTurn(Axis, std::acos(std::min(1.f, std::max(-1.f, N.Z))) * Clamp01(P.Lie), Tilt);
		}
		Xform W;
		M3Mul(Tilt, Rf, W.R);
		const Vec3 RP = M3Vec(W.R, P.Pivot);
		W.T[0] = static_cast<float>(PX) - RP.X;
		W.T[1] = static_cast<float>(PY) - RP.Y;
		W.T[2] = G.Height - RP.Z + P.Lift;
		if (OutGround) *OutGround = G.Height;
		return W;
	}

	namespace
	{
		/** One point of a mesh skinned (SkinMesh's step): its two bones' frames, part2's by its weight. */
		inline void SkinPoint(size_t I, const float* At, const float* Fa, const uint8_t* P1, const uint8_t* P2, const float* W2,
			const Xform* Frames, float* OutAt, float* OutFacing)
		{
			const float X = At[I * 3], Y = At[I * 3 + 1], Z = At[I * 3 + 2];
			const Xform& A = Frames[P1[I]];
			float PX = A.R[0] * X + A.R[1] * Y + A.R[2] * Z + A.T[0];
			float PY = A.R[3] * X + A.R[4] * Y + A.R[5] * Z + A.T[1];
			float PZ = A.R[6] * X + A.R[7] * Y + A.R[8] * Z + A.T[2];
			const float W = W2[I];
			const bool bTwo = W > 0.f && P2[I] != P1[I];
			if (bTwo)
			{
				const Xform& B = Frames[P2[I]];
				const float QX = B.R[0] * X + B.R[1] * Y + B.R[2] * Z + B.T[0];
				const float QY = B.R[3] * X + B.R[4] * Y + B.R[5] * Z + B.T[1];
				const float QZ = B.R[6] * X + B.R[7] * Y + B.R[8] * Z + B.T[2];
				PX += (QX - PX) * W;
				PY += (QY - PY) * W;
				PZ += (QZ - PZ) * W;
			}
			OutAt[I * 3] = PX;
			OutAt[I * 3 + 1] = PY;
			OutAt[I * 3 + 2] = PZ;
			if (!OutFacing) return;
			const float NX = Fa[I * 3], NY = Fa[I * 3 + 1], NZ = Fa[I * 3 + 2];
			float FX = A.R[0] * NX + A.R[1] * NY + A.R[2] * NZ;
			float FY = A.R[3] * NX + A.R[4] * NY + A.R[5] * NZ;
			float FZ = A.R[6] * NX + A.R[7] * NY + A.R[8] * NZ;
			if (bTwo)
			{
				const Xform& B = Frames[P2[I]];
				FX += (B.R[0] * NX + B.R[1] * NY + B.R[2] * NZ - FX) * W;
				FY += (B.R[3] * NX + B.R[4] * NY + B.R[5] * NZ - FY) * W;
				FZ += (B.R[6] * NX + B.R[7] * NY + B.R[8] * NZ - FZ) * W;
				const float L = FX * FX + FY * FY + FZ * FZ;
				const float Inv = L > 1e-12f ? 1.f / std::sqrt(L) : 0.f;
				FX *= Inv;
				FY *= Inv;
				FZ *= Inv;
			}
			OutFacing[I * 3] = FX;
			OutFacing[I * 3 + 1] = FY;
			OutFacing[I * 3 + 2] = FZ;
		}
	}

	void SkinMesh(const Mesh& M, const Xform* Frames, float* OutAt, float* OutFacing, const float* RestAt, const float* RestFacing)
	{
		const float* At = RestAt ? RestAt : M.At.data();
		const float* Fa = RestFacing ? RestFacing : M.Facing.data();
		const size_t N = static_cast<size_t>(M.Points);
		for (size_t I = 0; I < N; ++I) SkinPoint(I, At, Fa, M.Part.data(), M.Part2.data(), M.W2.data(), Frames, OutAt, OutFacing);
	}

	// -------------------------------------------------------------------------------------------------------------
	// The crowd.
	// -------------------------------------------------------------------------------------------------------------
	namespace
	{
		/** How a man stands: what the transitions and the falls go by. */
		enum Class : uint8_t { Up = 0, Kneel = 1, Prone = 2 };

		/** How a pose of the rules is drawn with the soldier's clips. */
		struct Stance
		{
			int Clip = -1;
			enum How : uint8_t { Gait, Loop, Hold } Play = Hold;
			float HoldT = 0.f;
			Class Cl = Up;
			bool bRifleDown = false;
			bool bFiring = false;           // a firing pose: its clip's shots come from the rounds
			bool bMoving = false;           // he moved just after the change (a horse holder walks his horses)
			float Lift = 0.f;               // carried on a stretcher: the canvas's height over the ground, metres
			bool bUnderCanvas = false;      // riding in a lorry (pose mounted): under her canvas, not drawn (Crowd::Visible) unless seated (SeatFn)
			uint8_t Aboard = 0;             // in a vehicle's bed (SeatFn): 1 seated on the bench, 2 kneeling on the floor
			// the rules' vehicle (her k in his block) and seat, -1 where not said; OffAt (Men in lorries, the Battle rules
			// card's row), the battle time his turn to get down over her tailboard comes (-1: not told to get down)
			int SeatOn = -1, Seat = -1;
			double OffAt = -1.0;
			std::string Pose, Cover;
			double T = 0.0;                 // when the rules gave it (battle time)
		};

		struct Segment
		{
			int Clip = -1;
			enum Kind : uint8_t { Gait, Loop, Hold, Once } How = Hold;
			double Start = 0.0;
			double Dur = 0.0;
			float From = 0.f, To = 0.f, HoldT = 0.f;
			float Phase = 0.f;              // a gait's place in its loop (share); a loop's start (share)
			float Rate = 1.f;
			float LieA = 0.f, LieB = 0.f;
			float BlendIn = 0.2f;
			bool bShift = false;            // go_prone: its end offset taken back as it plays, so he lies where he stood
			bool bRifleDown = false;
			bool bFiring = false;
			bool bCapped = false;
			Class EndClass = Up;
			int Loops = 0;
			float LastT = -1.f;             // the clip's time last frame, for its events
			// Bullet hits (bWounded): a wounded man's loop down there, turned about his root by Yaw and moved by Shift
			// (his own axes) so it lies as his fall left him; bWound: one of his wounded segments (his fall or what he does
			// after), so the rules' stances wait.
			bool bTurned = false, bWound = false;
			float Yaw = 0.f, ShiftX = 0.f, ShiftY = 0.f;
		};

		struct Shot
		{
			double T = 0.0;                 // the flash (a grenade's release)
			Weapon W = Weapon::Rifle;
			// The plan, made from his pose at the time:
			enum Kind : uint8_t { Clip, Burst, Marching, Mortar } How = Clip;
			int ClipIx = -1;
			float Lead = 0.f;               // clip time of the flash (the release)
			double Start = 0.0, End = 0.0;
			float Rate2 = 1.f;              // after the flash, squeezed where the next round comes sooner
			int Bullets = 1;
			int Done = 0;                   // bullets already shown
			float LastC = -1.f;             // its clip's time last frame, for its events (the bolt)
		};

		struct TrackData
		{
			int B = 0, K = 0, J = 0;    // J: a follower of key man K in his section, 0 the key man
			std::string Role;
			bool bFo = false;
			std::vector<double> T, X, Y;
			std::vector<float> F;
			struct Change
			{
				double T = 0.0;
				std::string Pose, Cover;
				bool bHidden = false;
				int SeatOn = -1, Seat = -1;   // aboard: his vehicle's k in his block and his seat in her bed (-1: not said)
				double OffAt = -1.0;          // and (Men in lorries) when his turn to get down over her tailboard comes (-1: not yet told)
			};
			std::vector<Change> Ch;
			double D = -1.0, OutT = -1.0;
			// Bullet hits (bWounded): struck and not killed (wd), on his feet again (back), and whether bearers carry him
			// (cy), as the feed reads them (LandFeed's ReadHurt); -1 none.
			double Wd = -1.0, Back = -1.0;
			bool bCarried = false;
			// A live battle's (Extend): the last point is provisional (where he stands at the delta's time, after his
			// kept points), and whether he is on the map yet (TrackOf, and his man), which he is from his first point.
			bool bTail = false;
			bool bPlaced = false;
			int Man = -1;               // his man, or -1 (a tank's, a gun's, a lorry's track)

			/** Where he was at time Tm (lerped between kept points, the facing turned the short way) and his speed. */
			void At(double Tm, size_t& Cursor, double& OX, double& OY, float& OF, float& OV) const
			{
				const size_t N = T.size();
				if (N == 0) { OX = OY = 0.0; OF = 0.f; OV = 0.f; return; }
				// (the cursor backs up to the point at or before Tm: a provisional last point replaced by a nearer one
				// leaves it past the end, and going back in time leaves it ahead; either way the same point is found)
				if (Cursor >= N) Cursor = N - 1;
				while (Cursor > 0 && T[Cursor] > Tm) --Cursor;
				while (Cursor + 1 < N && T[Cursor + 1] <= Tm) ++Cursor;
				if (Cursor + 1 >= N || Tm <= T[0])
				{
					const size_t I = Tm <= T[0] ? 0 : N - 1;
					OX = X[I]; OY = Y[I]; OF = F[I]; OV = 0.f;
					return;
				}
				const size_t I = Cursor;
				const double Span = T[I + 1] - T[I];
				const double U = Span > 1e-9 ? std::min(1.0, std::max(0.0, (Tm - T[I]) / Span)) : 0.0;
				OX = X[I] + (X[I + 1] - X[I]) * U;
				OY = Y[I] + (Y[I + 1] - Y[I]) * U;
				OF = static_cast<float>(F[I] + Turn(F[I], F[I + 1]) * U);
				OV = Span > 1e-9 ? static_cast<float>(std::hypot(X[I + 1] - X[I], Y[I + 1] - Y[I]) / Span) : 0.f;
			}

			/** A cursor at the last point at or before Tm (0 when none), so At need not walk the track from its start. */
			size_t CursorAt(double Tm) const
			{
				const auto It = std::upper_bound(T.begin(), T.end(), Tm);
				return It == T.begin() ? 0 : static_cast<size_t>(It - T.begin()) - 1;
			}

			/** The last kept point's time (the provisional one is not kept), or -1e18 with none. */
			double KeptUntil() const
			{
				const size_t Kept = T.size() - (bTail && !T.empty() ? 1 : 0);
				return Kept > 0 ? T[Kept - 1] : -1e18;
			}

			/** Who he is, as a recording's track or a live delta's new track gives it: block, model, follower, role. */
			void ReadWho(const struct J& Src)
			{
				B = static_cast<int>(Src["b"].Num0());
				K = static_cast<int>(Src["k"].Num0());
				J = std::max(0, static_cast<int>(Src["j"].Num0()));
				Role = Src["r"].Text();
				bFo = Src["fo"].Num0() != 0.0;
			}

			/** Kept points [t, x, y, f], appended. */
			void ReadPoints(const struct J& At)
			{
				for (size_t P = 0; P < At.Size(); ++P)
				{
					const struct J& Pt = At.At(P);
					T.push_back(Pt.Number(0));
					X.push_back(Pt.Number(1));
					Y.push_back(Pt.Number(2));
					F.push_back(static_cast<float>(Pt.Number(3)));
				}
			}

			/** Changes [t, pose, cover, aimX, aimY, hidden, buttoned] and, aboard, [..., seatOn, seat], appended. */
			void ReadChanges(const struct J& Chs)
			{
				for (size_t P = 0; P < Chs.Size(); ++P)
				{
					const struct J& C = Chs.At(P);
					Change Cg;
					Cg.T = C.At(0).Num0();
					Cg.Pose = C.At(1).Text("idle");
					Cg.Cover = C.At(2).Text("none");
					Cg.bHidden = C.At(5).Num0() != 0.0;
					// (aboard, with Men in lorries on: [..., seatOn, seat] and, told to get down, when his turn comes)
					if (C.Size() >= 9)
					{
						Cg.SeatOn = static_cast<int>(C.At(7).Num0(-1.0));
						Cg.Seat = static_cast<int>(C.At(8).Num0(-1.0));
						Cg.OffAt = C.Size() >= 10 ? C.At(9).Num0(-1.0) : -1.0;
					}
					Ch.push_back(Cg);
				}
			}
		};

		struct ArmyColours
		{
			uint32_t Paint[16] = {};
			std::string StandIn;
			bool bFileFinishes = true;      // pick from the soldier file's helmet finishes (false: his own helmet, our paint)
		};

		struct Variant
		{
			// A soldier's mesh at one level, with his own helmet or without it (a finish drawn instead), and (Effects, R2)
			// with his pack and webbing or without them (a man out of his machine, AddRider's bNoPack): the runs left out
			// (VariantCuts) and the points only they use, so those are neither skinned nor sent.
			Mesh M;
			std::vector<int> ToCompact;     // the file's point -> this mesh's, or -1
		};
		/** The runs of a soldier file's triangles a variant leaves out (bits): his own helmet's (helmet_tris), and his pack
		 * and webbing's (pack_tris; Effects, R2). */
		enum VariantCuts : int
		{
			CutHelmet = 1,
			CutPack = 2,
		};

		struct FaceRest
		{
			std::vector<float> At, Facing;
		};

		/** A fall's place in the Fates rule: the last four shown, newest first. */
		struct FallMemory
		{
			std::deque<std::string> Recent;
			double Weight(const std::string& Clip, size_t Candidates) const
			{
				static const double ByAge[4] = { 0.0, 0.35, 0.6, 0.85 };
				for (size_t I = 0; I < Recent.size() && I < 4; ++I)
				{
					if (Recent[I] == Clip) return I == 0 && Candidates == 1 ? 1.0 : ByAge[I];
				}
				return 1.0;
			}
			void Shown(const std::string& Clip)
			{
				Recent.push_front(Clip);
				while (Recent.size() > 4) Recent.pop_back();
			}
		};
	}

	namespace
	{
		/** A round as the men read it: who fired it (by, k, j), when and with what, and whom it struck (on, ok, oj). */
		struct RoundRow
		{
			bool bBy = false, bByK = false, bOnK = false; // (by, k and ok written)
			bool bMate = false; // (m: a shell blast's further man, the shot booked once on the round before it)
			double By = 0.0, ByK = 0.0, ByJ = 0.0, On = 0.0, OnK = 0.0, OnJ = 0.0;
			double T = 0.0, X = 0.0, Y = 0.0;
			std::string W, R;

			/** One member of a round's object (a live delta's, read as it goes). */
			void Set(const std::string& Key, const Scan::Plain& V)
			{
				if (Is(Key, "by")) { bBy = V.K != J::Null; By = V.Num0(); }
				else if (Is(Key, "k")) { bByK = V.K != J::Null; ByK = V.Num0(); }
				else if (Is(Key, "j")) ByJ = V.Num0();
				else if (Is(Key, "on")) On = V.Num0();
				else if (Is(Key, "ok")) { bOnK = V.K != J::Null; OnK = V.Num0(); }
				else if (Is(Key, "oj")) OnJ = V.Num0();
				else if (Is(Key, "t")) T = V.Num0();
				else if (Is(Key, "x")) X = V.Num0();
				else if (Is(Key, "y")) Y = V.Num0();
				else if (Is(Key, "w")) W = V.Text();
				else if (Is(Key, "r")) R = V.Text();
				else if (Is(Key, "m")) bMate = V.Num0() != 0.0;
			}
		};

		RoundRow RowOf(const J& Rd)
		{
			RoundRow Row;
			Row.bBy = Rd.Has("by");
			Row.By = Rd["by"].Num0();
			Row.bByK = Rd.Has("k");
			Row.ByK = Rd["k"].Num0();
			Row.ByJ = Rd["j"].Num0();
			Row.bOnK = Rd.Has("ok");
			Row.On = Rd["on"].Num0();
			Row.OnK = Rd["ok"].Num0();
			Row.OnJ = Rd["oj"].Num0();
			Row.T = Rd["t"].Num0();
			Row.X = Rd["x"].Num0();
			Row.Y = Rd["y"].Num0();
			Row.W = Rd["w"].Text();
			Row.R = Rd["r"].Text();
			Row.bMate = Rd["m"].Num0() != 0.0;
			return Row;
		}

		/** A gore fall that is the big throw: of kind thrown, or any die_blast_* (some listed under another kind too). */
		bool IsThrow(const std::string& Kind, const std::string& Name) { return Kind == "thrown" || Name.rfind("die_blast_", 0) == 0; }

		/** Effects (R2): whether a gored man may take a gore fall of his twin's: any not a throw; a throw only where his
		 * round may throw him (bBig, Crowd's BigThrow), and only one from its side (gore_from is Side: where the burst was
		 * under him and no gun is known, only the throw from under, the prone one; a424a37e gave a man on his feet there a
		 * throw from any side, with twins that hold no fall from under for his stance); one from under (gore_from under: a
		 * lower leg off, or the prone throw) only where the burst was under him (Side under: the deaths thread, 24 Nov, the
		 * burst within a metre). */
		bool MayTake(const Soldier& Tw, const std::string& Kind, const std::string& Name, bool bBig, const std::string& Side)
		{
			const auto From = Tw.GoreFrom.find(Name);
			if (From != Tw.GoreFrom.end() && From->second == "under" && Side != "under") return false;
			if (!IsThrow(Kind, Name)) return true;
			return bBig && From != Tw.GoreFrom.end() && From->second == Side;
		}

		/** Effects (R2): whether a gore fall of his twin's is from under him (gore_from under) and he may take it where
		 * the burst was under him (the prone throw only where bBig). */
		bool FromUnder(const Soldier& Tw, const std::string& Kind, const std::string& Name, bool bBig)
		{
			const auto From = Tw.GoreFrom.find(Name);
			return From != Tw.GoreFrom.end() && From->second == "under" && MayTake(Tw, Kind, Name, bBig, "under");
		}

		/** The kinds of falls a man of stance St takes from a deaths table (ChooseFall's): his stance's, else the
		 * nearest (cover: kneel; run: stand; prone: kneel), then stand, kneel, the table's first. None from an empty
		 * table, or for a man lying where it has no prone falls (he goes limp). */
		const std::map<std::string, std::vector<std::string>>* KindsOf(const std::map<std::string, std::map<std::string, std::vector<std::string>>>& Table,
			const std::string& St)
		{
			if (Table.empty() || (St == "prone" && Table.find("prone") == Table.end())) return nullptr;
			const char* Order[] = { St.c_str(), St == "cover" ? "kneel" : St == "run" ? "stand" : St == "prone" ? "kneel" : "stand", "stand", "kneel" };
			for (const char* Name : Order)
			{
				auto Found = Table.find(Name);
				if (Found != Table.end() && !Found->second.empty()) return &Found->second;
			}
			return &Table.begin()->second;
		}

		using GoreKinds = std::map<std::string, std::vector<std::string>>;
		using GoreList = std::vector<std::pair<std::string, std::string>>;

		/** Effects (R2): whether his stance's gore falls hold one from under him he may take (FromUnder). */
		bool AnyFromUnder(const Soldier& Tw, const GoreKinds& Kinds, bool bBig)
		{
			for (const auto& KV : Kinds)
			{
				for (const std::string& Name : KV.second)
				{
					if (FromUnder(Tw, KV.first, Name, bBig)) return true;
				}
			}
			return false;
		}

		/** Effects (R2): the side his gore falls are read by (GoreSide): the burst's; a burst under him, under where his
		 * stance's gore falls hold one from under he may take, else his gun's side where known (bAway, the gun's place),
		 * else under with no point to be thrown away from. */
		GoreStrike ReadBy(const Soldier& Tw, const GoreKinds& Kinds, const GoreStrike& Strike)
		{
			GoreStrike By = Strike;
			if (Strike.Side != "under" || AnyFromUnder(Tw, Kinds, Strike.bBig)) return By;
			if (!Strike.Gun.empty()) By.Side = Strike.Gun;
			else By.bAway = false;
			return By;
		}

		/** Effects (R2): the least any limb a gore fall takes off (its limb_off events') moves away from a point at PX, PY
		 * in his own axes, along the line from it to the limb's joint as he was struck (m; less than 0 toward it), read
		 * from the fall's first frame and its last: FallsBySide's half metre, and against his gun GoreFalls' falls from
		 * under. 1e9 for a fall that takes none off. */
		double LimbsAway(const Soldier& Tw, const Clip& Cl, double PX, double PY)
		{
			Xform AtFirst[MaxBones], AtLast[MaxBones];
			Pose First, End;
			Sample(Cl, 0.f, First);
			Sample(Cl, Cl.Long, End);
			BoneFrames(Tw, First, AtFirst);
			BoneFrames(Tw, End, AtLast);
			double Least = 1e9;
			for (const ClipEvent& E : Cl.Events)
			{
				if (E.Fx != "limb_off" || E.Bone < 0 || E.Bone >= MaxBones) continue;
				const Vec3 Head = Tw.Bones[static_cast<size_t>(E.Bone)].Head;
				const Vec3 A = AtFirst[E.Bone].Point(Head), B = AtLast[E.Bone].Point(Head);
				const double OX = static_cast<double>(A.X) - PX, OY = static_cast<double>(A.Y) - PY;
				const double Along = ((static_cast<double>(B.X) - static_cast<double>(A.X)) * OX + (static_cast<double>(B.Y) - static_cast<double>(A.Y)) * OY)
					/ std::max(1e-6, std::hypot(OX, OY));
				Least = std::min(Least, Along);
			}
			return Least;
		}

		/** Effects (R2): the gore falls he may be given by the side Strike.Side (GoreFalls, but a burst under him read
		 * there first). */
		GoreList FallsBySide(const Soldier& Tw, const GoreKinds& Kinds, const GoreStrike& Strike)
		{
			GoreList Out;
			// (the art director, 23 Nov 17:52Z: the limb flies away from the blast; the deaths thread, 20:1xZ: weigh only the
			// kind matching the burst, the big throw only for guns and howitzers) Those he may take (MayTake: no throw but
			// a field gun's or a howitzer's, and none from another side of him; none from under but for a burst under him);
			// of them those whose gore_from is where the burst was against him (in front, the falls from the front, of kind
			// back; behind, forward; at his left or his right, the side falls from that side), where his stance's gore falls
			// hold any.
			auto May = [&](const std::string& Kind, const std::string& Name) { return MayTake(Tw, Kind, Name, Strike.bBig, Strike.Side); };
			auto FromBurst = [&](const std::string& Kind, const std::string& Name) {
				const auto Side = Tw.GoreFrom.find(Name);
				return Side != Tw.GoreFrom.end() && Side->second == Strike.Side && May(Kind, Name);
			};
			bool bMatched = false, bAny = false;
			for (const auto& KV : Kinds)
			{
				for (const std::string& Name : KV.second)
				{
					bMatched = bMatched || FromBurst(KV.first, Name);
					bAny = bAny || May(KV.first, Name);
				}
			}
			// (where widened past his side's falls, below, never to one from the side opposite it, front and behind, left
			// and right: such a fall throws him toward the burst, his body and often a limb (the verifier of f6f8b75c: a
			// man standing with a burst ahead on his left given die_leg_off_b, from behind, his pelvis 1.1 to 1.7 m toward
			// it). Nor to one from under, which MayTake gives only where the burst was under him (a lower leg off, or the
			// prone throw: the burst within a metre; widened to, die_leg_off_d moved a standing man's body as much as
			// 0.23 m toward a burst or a gun 45 degrees round on his left). A burst under him is no point to be thrown
			// away from, so none is widened then. Widened so, he keeps his side's own falls, so never none)
			auto Opposite = [&](const std::string& Name) {
				const auto From = Tw.GoreFrom.find(Name);
				if (From == Tw.GoreFrom.end()) return false;
				const std::string& Fr = From->second;
				const std::string& Sd = Strike.Side;
				return (Sd == "front" && Fr == "behind") || (Sd == "behind" && Fr == "front") || (Sd == "left" && Fr == "right") || (Sd == "right" && Fr == "left");
			};
			bool bWide = false;
			auto Base = [&](const std::string& Kind, const std::string& Name) {
				if (!bAny) return true;
				if (bMatched && !bWide) return FromBurst(Kind, Name);
				return May(Kind, Name) && !(bWide && Opposite(Name));
			};
			// (and of those, where he is thrown away from a point (the burst, or the gun), the falls whose every limb moves
			// at least half a metre away from it, along the line from it to the limb's joint as he was struck, or failing
			// any the one whose limbs move farthest so: a limb flying across the burst, or through it to land beyond, is
			// not thrown away from it. A side is a quarter round, and the twins' falls fling a limb off their side's middle
			// (their standing and running falls from the front a left arm or leg back and to his left, toward a burst 38
			// degrees or more round on his left): where none from his side moves its limbs half a metre away, all he may
			// take but those from the opposite side are weighed so (the verifier of 35ffc951: never a limb toward the burst
			// for a fall from his side when another he may take throws it away). Read from the fall's first frame and its
			// last, in his own axes: the same whatever frame his fall is first played on and however lately he was posed,
			// his own pose as struck within 0.35 m of the fall's first at a cut, a running man's calf the most)
			std::map<std::string, double> Gain;
			double Floor = -1e9;
			if (Strike.bAway)
			{
				auto Measure = [&]() {
					for (const auto& KV : Kinds)
					{
						for (const std::string& Name : KV.second)
						{
							const int Ix = Tw.ClipIndex(Name);
							if (Ix < 0 || !Base(KV.first, Name) || Gain.count(Name)) continue;
							Gain[Name] = LimbsAway(Tw, Tw.Clips[static_cast<size_t>(Ix)], Strike.X, Strike.Y);
						}
					}
					Floor = -1e9;
					for (const auto& G : Gain) Floor = std::max(Floor, std::min(0.5, G.second));
				};
				Measure();
				if (bMatched && Floor < 0.5)
				{
					bWide = true;
					Measure();
				}
			}
			for (const auto& KV : Kinds)
			{
				for (const std::string& Name : KV.second)
				{
					if (!Base(KV.first, Name)) continue;
					if (!Gain.empty())
					{
						const auto G = Gain.find(Name);
						if (G == Gain.end() || G->second < Floor) continue;
					}
					std::pair<std::string, std::string> Fall(KV.first, Name);
					if (std::find(Out.begin(), Out.end(), Fall) == Out.end()) Out.push_back(std::move(Fall));
				}
			}
			return Out;
		}
	}

	std::string GoreSide(const Soldier& Tw, const GoreStrike& Strike)
	{
		const GoreKinds* Kinds = KindsOf(Tw.DeathsGore, Strike.Stance);
		if (!Kinds) return std::string();
		const GoreStrike By = ReadBy(Tw, *Kinds, Strike);
		for (const auto& KV : *Kinds)
		{
			for (const std::string& Name : KV.second)
			{
				if (MayTake(Tw, KV.first, Name, By.bBig, By.Side)) return By.Side;
			}
		}
		return std::string();
	}

	std::vector<std::pair<std::string, std::string>> GoreFalls(const Soldier& Tw, const GoreStrike& Strike)
	{
		const GoreKinds* Kinds = KindsOf(Tw.DeathsGore, Strike.Stance);
		if (!Kinds) return {};
		// (a burst under him, within 1 m whatever his stance: the deaths thread, 24 Nov, its falls from under, a lower leg
		// off on his feet or a knee, and lying the throw from under him. Those he may take; a burst under him is no point to
		// be thrown away from, but his gun is, below. A field gun's or a howitzer's shell may throw him too, but only as his
		// gun's side would (the falls from there, of them the throws: from that side alone, his limbs and his body thrown
		// away from the gun, as before); the kind from under weighed as the kind matching the burst is, ChooseFall. Where
		// his stance's gore falls hold none from under he may take (lying, but for the prone throw, none), by his gun's
		// side as for a burst there (ReadBy), or where it is not known all he may take but a throw (MayTake: a throw only
		// from his side, and under him only the prone one is from under))
		if (Strike.Side == "under" && AnyFromUnder(Tw, *Kinds, Strike.bBig))
		{
			GoreList Thrown;
			if (Strike.bBig && !Strike.Gun.empty() && Strike.bAway)
			{
				GoreStrike ByGun = Strike;
				ByGun.Side = Strike.Gun;
				for (const auto& Fall : FallsBySide(Tw, *Kinds, ByGun))
				{
					if (IsThrow(Fall.first, Fall.second)) Thrown.push_back(Fall);
				}
			}
			// (his gun known (Gun, and bAway: the gun's place): of his falls from under only those whose limb does not move
			// toward the gun, along the line from it to the limb's joint as he was struck (LimbsAway, as a limb is read
			// against a burst), or where every one moves it toward the gun those that move it least so (never none). The
			// art director, 23 Nov 17:52Z: the limb flies away from the blast; with one fall from under each stance it flew
			// toward the gun about half the time. The deaths thread's mirrors (die_leg_off_f standing and running,
			// die_leg_off_g kneeling and in cover: the other leg, flung to the other side) give him at least one that flies
			// away from a gun ahead (mostly both) or at either side, but for a gun a few metres off on his right just short
			// of his back (1 to 3 m off, 133 to 135 degrees round, standing or running: a probe's 1-degree grid); with it
			// behind him both legs may fly back toward it (the test prints those cells). His gun not known, all of them,
			// either leg; a stance with one from under (the twins before; lying, the prone throw) is given it whatever his
			// gun's side, as before)
			std::map<std::string, double> Gain;
			double Floor = -1e9;
			if (!Strike.Gun.empty() && Strike.bAway)
			{
				double Best = -1e9;
				for (const auto& KV : *Kinds)
				{
					for (const std::string& Name : KV.second)
					{
						const int Ix = Tw.ClipIndex(Name);
						if (Ix < 0 || !FromUnder(Tw, KV.first, Name, Strike.bBig) || Gain.count(Name)) continue;
						const double G = LimbsAway(Tw, Tw.Clips[static_cast<size_t>(Ix)], Strike.X, Strike.Y);
						Gain[Name] = G;
						Best = std::max(Best, G);
					}
				}
				Floor = std::min(0.0, Best);
			}
			GoreList Out;
			for (const auto& KV : *Kinds)
			{
				for (const std::string& Name : KV.second)
				{
					std::pair<std::string, std::string> Fall(KV.first, Name);
					const bool bUnder = FromUnder(Tw, KV.first, Name, Strike.bBig);
					if (!bUnder && std::find(Thrown.begin(), Thrown.end(), Fall) == Thrown.end()) continue;
					if (bUnder)
					{
						const auto G = Gain.find(Name);
						if (G != Gain.end() && G->second < Floor) continue;
					}
					if (std::find(Out.begin(), Out.end(), Fall) == Out.end()) Out.push_back(std::move(Fall));
				}
			}
			return Out;
		}
		return FallsBySide(Tw, *Kinds, ReadBy(Tw, *Kinds, Strike));
	}

	struct Crowd::Impl
	{
		/**
		 * The pose bank (SO1): one far mesh (a soldier file's, with or without his own helmet) baked at each of the
		 * bank's poses in the man's own axes, and for each of the file's clips the bank's poses it shows (its own cycle,
		 * or its `instead`'s; empty: none, he is posed and skinned).
		 */
		struct Bank
		{
			int Points = 0, Bones = 0;
			std::vector<char> Have;                 // pose k baked (the file has its clip)
			std::vector<float> At, Facing;          // pose k, point p: (k * Points + p) * 3
			std::vector<Xform> Model;               // pose k, bone b: k * Bones + b (the bones in his own axes)
			std::vector<Pose> Poses;                // pose k's turns and moves (a fall blends from them)
			std::vector<std::vector<int>> CycleOf;  // the file's clip -> the bank's poses it shows
		};

		struct Man
		{
			ManInfo Info;
			int Track = -1;
			const Soldier* S = nullptr;
			const ArmyColours* Colours = nullptr;
			// (uniforms that age: his army's row before the fading, and how worn his side is, 0 fresh to 1 after a year
			// and a half in the line; his patches and mismatched kit drawn where his own look is made, LookOf)
			const ArmyColours* Fresh = nullptr;
			float WornA = 0.f;
			// (release 2, the worn look's wear (SetWearMix): his side's months in the line where uniforms age and the
			// recording gives them, under 0 where not; his W, ManInfo::Wear, is read from it: SettleWear)
			double WearMonths = -1.0;
			std::vector<Stance> Stances;    // one per change of the track
			std::vector<Shot> Shots;
			bool bShotsDirty = false;
			size_t ShotCur = 0;
			float PhaseSeed = 0.f;
			float StylePick = 0.f;          // which of a clip's styles is his (SetRunStyles), a share
			std::string FallKindWanted;
			// The kill: what struck him, from where.
			double KillT = -1.0;
			int KillBy = -1, KillK = -1, KillJ = 0;
			Weapon KillW = Weapon::Other;
			std::string KillWeapon;
			double KillX = 0.0, KillY = 0.0;
			/** Every round that killed him, in the recording's order: the one nearest his fall is his kill (ChooseKill),
			 * chosen again when a live delta brings another, or brings his fall after them. */
			struct Kill
			{
				double T = 0.0, X = 0.0, Y = 0.0;
				int By = -1, K = -1, J = 0;
				std::string Weapon;
			};
			std::vector<Kill> Kills;
			// A live battle's: his stances before Firm are settled (each read his speed between kept points, which never
			// change); the rest read it from a provisional point or past his last, and are made again as his track grows.
			// bIdle: he had no change yet, so his one stance is the idle Build gives such a man.
			size_t Firm = 0;
			bool bIdle = false;
			/** He serves a support team's weapon drawn at his place (Crowd::Serve): 1 lying behind it, 2 kneeling at it. */
			uint8_t Serves = 0;

			// Now.
			enum Life : uint8_t { Alive, Falling, Settling, Rest } State = Alive;
			size_t AtCursor = 0, ChCursor = 0;
			int Shown = -1;                 // the change whose stance is shown (or being turned to)
			Class EndClass = Up;
			Segment Prev, Cur;
			bool bPrev = false;
			std::deque<Segment> Queue;
			double X = 0.0, Y = 0.0;
			float F = 0.f, V = 0.f;
			float Fade = 1.f;
			bool bVisible = true;
			bool bUnderCanvas = false;      // alive and riding in a lorry's bed now (his shown stance's): not drawn
			// Leaving with his block (release 2, SetLeaveWalk): on his way off, and how long he takes to get to his feet first.
			bool bLeaving = false;
			float LeaveHold = 0.f;
			double Last = -1.0;
			uint32_t Stamp = 0;
			Lod L = Far;
			float EyeD = 1e9f;              // his distance from the eye at the last Update, as the picture sees it (over the zoom)
			Pose Now;
			Xform Model[MaxBones];          // the bones in his own axes
			Xform World;
			Xform Frames[MaxBones];         // the bones on the field
			Placing Place;
			float Footing = 0.f;            // the ground's height read for World (PlaceAsPosed), once bFooting
			bool bFooting = false;
			float Lie = 0.f;
			float RifleDown = 0.f;
			bool bShooting = false;
			float ShotW = 0.f;
			// The fall.
			int FallClip = -1;
			double FallAt = -1.0, FallDur = 0.0;
			Pose FallFrom;
			float FallLie0 = 0.f;
			Vec3 FallPivot;                 // where his pelvis comes to rest, in his own axes (z 0)
			float FallLastT = -1.f;
			// His mesh at each level and its face's rest points (made in Build, read by Skin without the maps, so that
			// Skin may run for different men at once).
			const Variant* Var[LodCount] = { nullptr, nullptr, nullptr };
			const FaceRest* FaceAt[LodCount] = { nullptr, nullptr, nullptr };
			// The pose bank (SO1): his far mesh's bake (its poses' bones are every level's), each banked level's bake and
			// the points his face moves there (skinned over the bake), and the bank's pose he shows (-1: posed and skinned).
			const Bank* Banked = nullptr;
			const Bank* BankAt[LodCount] = { nullptr, nullptr, nullptr };
			const std::vector<int>* FaceMoved[LodCount] = { nullptr, nullptr, nullptr };
			int BankK = -1;
			int BankKey = -1;               // his baked set (BankKey): his far variant, helmet finish and army colours
			int BodyKey = -1;               // his body set (BodyKey): his soldier file, his own helmet or none, army colours
			// The mesh a vertex-animation material draws (BankShow): bake A to bake B by W; and the last Update's (TSR).
			int ShowA = -1, ShowB = -1, WasA = -1, WasB = -1;
			float ShowW = 0.f, WasW = 0.f;
			// At rest: the world frames he froze with, and each level skinned once on first asking.
			bool bSkinned[LodCount] = { false, false, false };
			std::vector<float> RestAt[LodCount], RestFacing[LodCount];
			// Looks.
			bool bLook[LodCount] = { false, false, false };
			Look Looks[LodCount];
			// A rider (a tank's commander in his hatch, Crowd::AddRider): no track; posed each frame by what the scene
			// says (SetRide).
			bool bRider = false;
			int LookTrack = -1;             // a rider dressed as a man of the recording's (AddRiderLike): that man's track, for his patches
			Crowd::Ride RideNow;
			// Effects (R2): fallen as his file's gore twin (S the twin, OwnS his own file till a replay from the start
			// gives it back); MeshNo counts the swaps, so the scene makes his triangles again.
			const Soldier* OwnS = nullptr;
			bool bGore = false;
			uint32_t MeshNo = 0;
			// Effects (R2): a rider drawn bareheaded, his file's own helmet left off and no finish drawn in its place (a
			// downed pilot in an infantry file: AddRider's bBareHead).
			bool bBareHead = false;
			// Effects (R2): a rider drawn without his pack and webbing, his file's pack_tris left off and nothing drawn in
			// their place (a downed pilot, or a tank's crewman bailing out, in an infantry file: AddRider's bNoPack).
			bool bNoPack = false;
			// Bullet hits (SetHits). The jolt (bJolt): from JoltAt for a third of a second; the way away from the shot
			// in his own axes (level), the bend and the twist (radians, the twist's sign the struck shoulder's: + his
			// right), the pulse's pace (w, 1/s), an mg's second kick (seconds after the first, 0 none), and the knee a
			// leg fall buckles (the calf bone, -1 none).
			double JoltAt = -1.0;
			Vec3 JoltAway { -1.f, 0.f, 0.f };
			float JoltBend = 0.f, JoltTwist = 0.f, JoltOmega = 20.f, JoltSecond = 0.f, JoltKnee = 0.f;
			int JoltCalf = -1;
			// Wounded (bWounded): wd and back from his track, not carried; when his fall down there ends, where his pelvis
			// lies then (his own axes, z 0), the fall's last pose (his rifle stays where it fell), his breathing (held:
			// no loop), and when he got up again (his body moved back to his root over the get-up: UpShift).
			double Wd = -1.0, Back = -1.0;
			bool bCarried = false;
			double WoundEnds = -1.0;
			Vec3 WoundPivot;
			Pose WoundEnd;
			bool bBreathe = false;
			double UpAt = -1.0;
			Vec3 UpShift;
		};

		std::map<std::string, std::shared_ptr<const Soldier>> Soldiers;
		// Effects (R2): each file's gore twin as the crowd keeps it (AddGore: the twin's mesh with the file's clips), on
		// or off (SetGore), and the men gored this play.
		std::map<std::string, std::shared_ptr<const Soldier>> Gores;
		bool bGore = false;
		int GoredN = 0;
		Crowd::Hits HitsNow;    // bullet hits (SetHits): all off, as before
		std::map<std::string, ArmyColours> Armies;
		std::vector<TrackData> Tracks;
		std::map<std::tuple<int, int, int>, int> TrackOf;   // (block, k, j)
		std::vector<Man> Men;
		std::map<std::tuple<int, int, int>, int> ManOf;     // (block, k, j)
		std::vector<int> TrackOfN;  // a live delta's track number (n) -> Tracks
		/** A live delta's news of one track, as read (Extend): who a new one is, and where in the delta's lists its new
		 * kept points and changes are. */
		struct News
		{
			int N = -1;
			bool bWho = false;      // b written: who a new track is
			int B = 0, K = 0, J = 0;
			std::string Role;
			bool bFo = false;
			size_t AtFrom = 0, AtTo = 0, ChFrom = 0, ChTo = 0;
			bool bD = false, bOut = false;
			double D = 0.0, Out = 0.0;
		};
		// (kept from one delta to the next, so reading one makes nothing new)
		std::vector<News> NewsRead;
		std::vector<double> PointsRead;            // [t, x, y, f] each
		std::vector<TrackData::Change> ChangesRead;
		std::vector<double> TailsRead;
		std::vector<RoundRow> RoundsRead;
		std::map<int, std::pair<std::string, std::string>> BlockArmy; // block id -> (army, file army)
		std::map<int, std::string> BlockCal; // block id -> its guns' calibre as the recording writes it (cal), where it does
		// (release 2, SetLeaveWalk) block id -> the way to its side's own edge along y: -1 the attack's (y 0), +1 the defence's
		std::map<int, int> BlockOwn;
		bool bLeaveWalk = false;
		/** Uniforms that age (release 2, behind its switch, SetWorn): each block's side's months in the line (the
		 * recording's sides.<side>.worn), the faded rows made from the armies' (one a row and wear, so no more sets of
		 * men alike), and whether the look is on. */
		std::map<int, double> BlockWorn;
		std::map<std::string, double> ArmyWorn; // (an army's months, for a rider, who has no block: AddRider)
		std::map<std::tuple<const ArmyColours*, int, int>, ArmyColours> AgedRows;
		bool bWornLook = false;
		bool bRunStyles = false;        // each man his own runner (SetRunStyles)
		double WornForced = -1.0;
		/** The men's worn look (release 2, tc.Look.Worn; SetWearMix): the field's soil, the rain, the snow, and whether each
		 * point's roughness is written. Off unless set: every look as before. */
		Crowd::WearMix Wear;
		bool bSections = false;     // the recording has followers (the drill's sections)
		GroundFn GroundAt;
		SeatFn SeatAt;              // the men aboard vehicles: their benches (SetSeats)
		uint64_t Seed = 1;
		int MonthNo = 0;
		std::string SeasonName = "summer", Front = "west";
		/** The armies drawn with another's soldier file for want of their own (MakeMan), each said once: Crowd::StandIns. */
		std::set<std::string> StandInSaid;
		std::vector<std::string> StandInLines;
		double EyeX = 1e9, EyeY = 1e9, EyeZ = 1e9;
		double EyeZoom = 1.0; // (SetEye's Zoom)
		double Clock = -1.0;  // the battle time of the last Update
		bool bThrift = false; // far men posed every few frames (SetThrift)
		Crowd::RunFn Run;     // the men stepped over the cores (SetRun)
		std::vector<std::vector<Event>> RunOut; // each run's events, joined in the men's order
		/** Life seconds a battle second (SetLife): a clip's seconds are life's, laid on the battle's clock at 1 / Life. */
		double Life = 1.0;
		double NearTo = 25.0, MidTo = 120.0; // the detail levels' bounds (SetLevels)
		uint64_t Frame = 0;   // Updates since the start, for the thrifty frames' turns
		bool bBuilt = false;
		FallMemory Falls;
		std::string RecordingText;  // kept for a replay from the start (Update going back)
		Crowd::Options With;
		std::map<std::tuple<const Soldier*, int, int>, Variant> Variants;           // soldier, runs cut (VariantCuts), level
		std::map<std::tuple<const Soldier*, int, int, int>, FaceRest> FaceRests;   // soldier, runs cut, level, face

		// The pose bank (SO1): as read (ReadPoseBank), and each far variant's bake.
		struct BankEntry
		{
			std::string Clip;
			float Phase = 0.f;
		};
		std::vector<BankEntry> BankSpec;
		std::map<std::string, std::vector<int>> BankCycles;
		std::map<std::string, std::string> BankInstead;
		bool bBankOn = false;
		double BankMs = 0.0;
		std::map<std::string, double> BankMsBy;
		std::map<const Variant*, Bank> Banks;
		std::map<const FaceRest*, std::vector<int>> FaceMovedAt;
		/** A baked set (BankKey): the men drawn alike out there, the same far triangles and colours (the file's own face). */
		struct BankSet
		{
			const Soldier* S = nullptr;
			const Bank* B = nullptr;
			int Finish = -1;
			float W = DefaultWear;  // (the wear its look is made at: SetWearMix)
			Look Lk;
		};
		std::vector<BankSet> BankSets;
		// (far variant, finish, colours row, and the wear in twentieths with the worn look on, -1 with it off: WearKeyOf)
		std::map<std::tuple<const Variant*, int, const ArmyColours*, int>, int> BankSetOf;
		/** Mid men banked too (SetPoseBankMid): their pose is the bank's, and their mid mesh is baked as the far one. */
		bool bBankMid = false;
		/**
		 * A body set (BodyKey; the vertex-animation form): the men drawn alike out to 25 m but for their helmet's finish
		 * and their face, the same soldier file (with his own helmet, or none where a finish is drawn), army colours. Each
		 * banked level's bake, body only, and its look (the file's own face); the finishes ride one bone (HeadBone), so a
		 * helmet is drawn as its own instance on that bone's frame.
		 */
		struct BodySet
		{
			const Soldier* S = nullptr;
			const ArmyColours* Colours = nullptr;
			float W = DefaultWear;  // (the wear its looks are made at: SetWearMix)
			const Variant* Var[LodCount] = { nullptr, nullptr, nullptr };
			const Bank* B[LodCount] = { nullptr, nullptr, nullptr };
			Look Lk[LodCount];
			int HeadBone = -1;
			std::vector<Look> Helmets[LodCount];   // each finish of the file at that level, alone
		};
		std::vector<BodySet> BodySets;
		// (far variant, colours row, and the wear in twentieths with the worn look on, -1 with it off: WearKeyOf)
		std::map<std::tuple<const Variant*, const ArmyColours*, int>, int> BodySetOf;

		/** Which levels show the bank's poses: far always (with a bank), mid when asked (SetPoseBankMid). */
		bool BanksAt(Lod L) const { return L == Far || (L == Mid && bBankMid); }

		/** The bank baked for a soldier file's far variant (once, timed), or null without a bank. */
		const Bank* BankOf(const Soldier& S, const Variant& V)
		{
			if (BankSpec.empty()) return nullptr;
			const auto Found = Banks.find(&V);
			if (Found != Banks.end()) return &Found->second;
			const auto T0 = std::chrono::steady_clock::now();
			Bank& B = Banks[&V];
			// (poses NP to 2 NP - 1 are the first NP with his rifle laid down: a gun's crew and the spotter hold their
			// aim so, LowerRifle)
			const size_t NP = BankSpec.size();
			B.Points = V.M.Points;
			B.Bones = static_cast<int>(std::min(S.Bones.size(), static_cast<size_t>(MaxBones)));
			const size_t NPt = static_cast<size_t>(B.Points), NB = static_cast<size_t>(B.Bones);
			B.Have.assign(2 * NP, 0);
			B.At.assign(2 * NP * NPt * 3, 0.f);
			B.Facing.assign(2 * NP * NPt * 3, 0.f);
			B.Model.assign(2 * NP * NB, Xform());
			B.Poses.resize(2 * NP);
			const bool bGun = S.WeaponBone() >= 0 && S.SocketIndex("socket_muzzle") >= 0;
			for (size_t K = 0; K < NP; ++K)
			{
				const int Ci = S.ClipIndex(BankSpec[K].Clip);
				if (Ci < 0) continue;
				const Clip& C = S.Clips[static_cast<size_t>(Ci)];
				Sample(C, BankSpec[K].Phase * C.Long, B.Poses[K]);
				Xform* Fr = &B.Model[K * NB];
				BoneFrames(S, B.Poses[K], Fr);
				SkinMesh(V.M, Fr, &B.At[K * NPt * 3], &B.Facing[K * NPt * 3]);
				B.Have[K] = 1;
				if (!bGun) continue;
				const size_t Kd = NP + K;
				B.Poses[Kd] = B.Poses[K];
				Xform* Fd = &B.Model[Kd * NB];
				std::copy(Fr, Fr + NB, Fd);
				LowerRifleOf(S, Fd, 1.f, 0.035f);
				SkinMesh(V.M, Fd, &B.At[Kd * NPt * 3], &B.Facing[Kd * NPt * 3]);
				B.Have[Kd] = 1;
			}
			auto CycleNamed = [&](const std::string& Name) {
				std::vector<int> Out;
				const auto It = BankCycles.find(Name);
				if (It == BankCycles.end()) return Out;
				for (const int K : It->second)
				{
					if (K >= 0 && static_cast<size_t>(K) < NP && B.Have[static_cast<size_t>(K)]) Out.push_back(K);
				}
				return Out;
			};
			B.CycleOf.assign(S.Clips.size(), std::vector<int>());
			for (size_t Ci = 0; Ci < S.Clips.size(); ++Ci)
			{
				std::vector<int> Cy = CycleNamed(S.Clips[Ci].Name);
				if (Cy.empty())
				{
					const auto It = BankInstead.find(S.Clips[Ci].Name);
					if (It != BankInstead.end()) Cy = CycleNamed(It->second);
				}
				// (a run style with no poses of its own: the poses of the clip it is a style of, or of that clip's stand-in)
				if (Cy.empty() && !S.Clips[Ci].StyleOf.empty())
				{
					Cy = CycleNamed(S.Clips[Ci].StyleOf);
					const auto It = BankInstead.find(S.Clips[Ci].StyleOf);
					if (Cy.empty() && It != BankInstead.end()) Cy = CycleNamed(It->second);
				}
				B.CycleOf[Ci] = std::move(Cy);
			}
			const double Took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
			BankMs += Took;
			BankMsBy[S.Name] += Took;
			return &B;
		}

		/** The far points a face moves from the file's own (its variant's rest): the bank was baked with the file's. */
		const std::vector<int>* FaceMovedOf(const Variant& V, const FaceRest& R)
		{
			const auto Found = FaceMovedAt.find(&R);
			if (Found != FaceMovedAt.end()) return &Found->second;
			std::vector<int>& Out = FaceMovedAt[&R];
			for (int P = 0; P < V.M.Points; ++P)
			{
				const size_t I = static_cast<size_t>(P) * 3;
				bool bMoved = false;
				for (size_t A = 0; A < 3; ++A) bMoved = bMoved || R.At[I + A] != V.M.At[I + A] || R.Facing[I + A] != V.M.Facing[I + A];
				if (bMoved) Out.push_back(P);
			}
			return &Out;
		}

		// ---------------------------------------------------------------------------------------------------------
		// The men's worn look (release 2, tc.Look.Worn; SetWearMix): the soldier files' own recipe for their wear and
		// weather (each file's about_wear), with the field's soil for their dust, and each point's roughness by its part.
		// ---------------------------------------------------------------------------------------------------------

		/** A mesh carries the wear (read with it, Soldier::Load's bWear, and every array whole). */
		static bool HasWear(const Mesh& M)
		{
			const size_t N = static_cast<size_t>(M.Points);
			return N > 0 && M.ShadeClean.size() == N && M.Dirt.size() == N && M.DirtWet.size() == N && M.Wet.size() == N && M.Snow.size() == N;
		}

		/** Each paint's roughness by its part (the art director's 25 Nov numbers: cloth 0.85 to 0.95, skin 0.5 to 0.6,
		 * helmets 0.6 to 0.7, leather 0.4 to 0.5, a weapon's metal 0.35 to 0.5), and whether it is gore (no dirt, wet or
		 * snow on it), for a paint number of that file: the army's fifteen by the table, the file's own by their names
		 * (Soldier::OwnPaintNames), any other the cloth's. */
		struct PaintWear
		{
			float Rough[256];
			bool bGore[256];
		};
		static void PaintWearOf(const Soldier& S, PaintWear& Out)
		{
			// (tunic, trousers, boots, lower, straps, helmet, pack, skin, wood, steel, coat, coat_dark, leather, blanket, hoof)
			static const float ArmyRough[] = { 0.90f, 0.90f, 0.45f, 0.88f, 0.80f, 0.65f, 0.90f, 0.55f, 0.55f, 0.42f, 0.92f, 0.92f, 0.45f, 0.95f, 0.60f };
			for (int Pa = 0; Pa < 256; ++Pa)
			{
				Out.Rough[Pa] = 0.90f;
				Out.bGore[Pa] = false;
			}
			for (size_t Pa = 0; Pa < sizeof(ArmyRough) / sizeof(ArmyRough[0]) && Pa < PaintNames().size(); ++Pa) Out.Rough[Pa] = ArmyRough[Pa];
			for (const auto& Own : S.OwnPaintNames)
			{
				if (Own.first < 0 || Own.first > 255) continue;
				std::string Name = Own.second;
				for (char& Ch : Name) Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
				const auto Has = [&Name](std::initializer_list<const char*> Words) {
					for (const char* Wd : Words)
					{
						if (Name.find(Wd) != std::string::npos) return true;
					}
					return false;
				};
				float R = 0.90f;
				if (Has({ "skin" })) R = 0.55f;
				else if (Has({ "eye", "iris" })) R = 0.35f;
				else if (Has({ "blood", "wound" })) R = 0.35f;
				else if (Has({ "bone" })) R = 0.55f;
				else if (Has({ "leather", "boot", "belt" })) R = 0.45f;
				else if (Has({ "helmet" })) R = 0.65f;
				else if (Has({ "steel", "metal", "brass", "badge", "buckle", "rocket", "tin" })) R = 0.42f;
				else if (Has({ "glass", "lens" })) R = 0.30f;
				Out.Rough[Own.first] = R;
				Out.bGore[Own.first] = Has({ "wound", "bone", "blood" });
			}
		}

		/** One point's wear as the mix reads it: its clean shadow, how dirty it is (d, his wear W in it), how wet it
		 * darkens and how much snow it holds. A gore paint's, or a mesh without the wear (its roughness alone): none. */
		struct WornAt
		{
			float Clean = 1.f, D = 0.f, Wet = 0.f, Snow = 0.f;
		};
		static WornAt WornOf(const Mesh& M, size_t P, const Crowd::WearMix& Wm, float W, bool bGore)
		{
			WornAt A;
			A.Clean = static_cast<float>(M.ShadeClean[P]) / 255.f;
			if (bGore) return A;
			const float Dry = static_cast<float>(M.Dirt[P]) / 255.f, Rainy = static_cast<float>(M.DirtWet[P]) / 255.f;
			A.D = (Dry + (Rainy - Dry) * Wm.Rain) * W;
			A.Wet = static_cast<float>(M.Wet[P]) / 255.f;
			A.Snow = static_cast<float>(M.Snow[P]) / 255.f;
			return A;
		}

		/** The files' recipe (about_wear), sRGB 0 to 1: the paint (Rgb, 0xRRGGBB: the army's, the file's own or a
		 * finish's ink) times the clean shadow, darkened where soaked, times K (the face's tone, tint and detail; null:
		 * 1); toward the soil (the field's, wetter toward its mud, in the same shadow) by 0.85 x d; then toward the snow's
		 * white. */
		static void WornColour(const Crowd::WearMix& Wm, const WornAt& A, uint32_t Rgb, const float* K, uint8_t* Out)
		{
			// (the files' mud over their dust, 0.27, 0.21, 0.15 over 0.60, 0.54, 0.43: the field's soil wet; and snow)
			static const float MudOfDust[3] = { 0.45f, 0.39f, 0.35f };
			static const float SnowWhite[3] = { 0.92f, 0.94f, 0.97f };
			for (int C = 0; C < 3; ++C)
			{
				const float Paint = static_cast<float>((Rgb >> (16 - 8 * C)) & 0xFF) / 255.f;
				const float Base = Paint * A.Clean * (1.f - 0.35f * A.Wet * Wm.Rain) * (K ? K[C] : 1.f);
				const float Soil = Wm.Soil[C] * (1.f + (MudOfDust[C] - 1.f) * Wm.Rain) * A.Clean;
				float V = Base + (Soil - Base) * 0.85f * A.D;
				const float White = SnowWhite[C] * (0.6f + 0.4f * A.Clean);
				V += (White - V) * 0.9f * A.Snow * Wm.Snow;
				Out[C] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, V * 255.f + 0.5f)));
			}
		}

		/** A point's roughness: its part's (PaintWear), duller with dry dust, a sheen soaked, toward 0.7 under snow; as the
		 * byte the worn inks read (over 255), held to 1 to 253 (255 is theirs for "keep the ink's own"). */
		static uint8_t WornRough(float Part, const WornAt& A, const Crowd::WearMix& Wm)
		{
			float R = Part;
			R += (0.95f - R) * 0.6f * A.D * (1.f - Wm.Rain);
			R += (0.55f - R) * 0.5f * A.Wet * Wm.Rain;
			R += (0.70f - R) * A.Snow * Wm.Snow;
			return static_cast<uint8_t>(std::clamp(std::lround(R * 255.f), 1L, 253L));
		}

		/** His wear (ManInfo::Wear) from his months in the line (WearMonths: 0 months 0.1, six weeks or more 1), 0.8 where
		 * they are not known (the art director's ruling of 25 November: her band is for every man in the line, so the
		 * default man is well worn; the files' own baked look is W 0.6), the mix's where it forces one; in twentieths, as
		 * it keys his sets. */
		void SettleWear(Man& M) const
		{
			double W = M.WearMonths > 0.0 ? std::clamp(0.1 + 0.9 * M.WearMonths / 1.5, 0.1, 1.0) : DefaultWear;
			if (Wear.W >= 0.f) W = Wear.W;
			M.Info.Wear = static_cast<float>(std::lround(std::clamp(W, 0.0, 1.0) * 20.0)) / 20.f;
		}

		/** What of his wear keys his sets (BankSetOf, BodySetOf): his W in twentieths with the worn look on; one -1 for all
		 * with it off, so the sets are as many and numbered as before. */
		int WearKeyOf(const Man& M) const { return Wear.bOn ? static_cast<int>(std::lround(M.Info.Wear * 20.f)) : -1; }

		/** A look at level L: the variant's triangles and the finish's after them, each point's army colour (or the
		 * finish's ink) times its shade; skin times the face's tone and its hair, brows and stubble times their tint.
		 * With the worn look on (Wm.bOn), each point the files' wear recipe at his wear W (WornColour) where the mesh
		 * carries it, the face's detail (Soldier::Detail) in place of the mesh's tint; and with Wm.bRough, each point's
		 * roughness (Look::Rough). */
		static void MakeLook(const Soldier& S, const Variant& V, const Finish* Fi, const ArmyColours& Colours, const Face* Fc, Lod L, const Crowd::WearMix& Wm, float W,
			Look& Lk)
		{
			const int Body = V.M.Points;
			const int Extra = Fi ? Fi->Lods[L].Points : 0;
			Lk.Points = Body + Extra;
			Lk.Triangles = V.M.Triangles;
			if (Fi)
			{
				for (const uint32_t T : Fi->Lods[L].Triangles) Lk.Triangles.push_back(T + static_cast<uint32_t>(Body));
			}
			Lk.Paint.resize(static_cast<size_t>(Lk.Points));
			Lk.Rgb.resize(static_cast<size_t>(Lk.Points) * 3);
			// Each point: its army's colour for its paint (or the finish's ink), times its shade; skin times the face's tone,
			// and the face's hair, brows and stubble times their tint.
			std::vector<float> Mul(static_cast<size_t>(Body) * 3, 1.f);
			const int SkinPaint = 7;
			for (int P = 0; P < Body; ++P)
			{
				if (V.M.Paint[static_cast<size_t>(P)] == SkinPaint && Fc)
				{
					Mul[static_cast<size_t>(P) * 3] = Fc->Tone.X;
					Mul[static_cast<size_t>(P) * 3 + 1] = Fc->Tone.Y;
					Mul[static_cast<size_t>(P) * 3 + 2] = Fc->Tone.Z;
				}
			}
			if (Fc)
			{
				const Face::Level& Lv = Fc->Lods[L];
				for (size_t K = 0; K < Lv.TintAt.size(); ++K)
				{
					const int C = V.ToCompact[static_cast<size_t>(Lv.TintAt[K])];
					if (C < 0) continue;
					for (int A = 0; A < 3; ++A) Mul[static_cast<size_t>(C) * 3 + static_cast<size_t>(A)] *= Lv.Tint[K * 3 + static_cast<size_t>(A)];
				}
			}
			// (the worn look, where the mesh carries the wear: the recipe leaves the mesh's own tint out, so the face's
			// detail face_detail.py laid in it (Soldier::Detail: the lash lines, the eyes, the lips and folds, the brim's
			// shade; a faceless file's hair and brows) goes into Mul with the face's tone and tint)
			const bool bWorn = Wm.bOn && HasWear(V.M);
			if (bWorn)
			{
				const Soldier::FaceDetail& Fd = S.Detail[L];
				for (size_t K = 0; K < Fd.At.size() && K * 3 + 2 < Fd.Mul.size(); ++K)
				{
					const size_t Full = static_cast<size_t>(Fd.At[K]);
					const int C = Full < V.ToCompact.size() ? V.ToCompact[Full] : -1;
					if (C < 0) continue;
					for (size_t Cc = 0; Cc < 3; ++Cc) Mul[static_cast<size_t>(C) * 3 + Cc] *= Fd.Mul[K * 3 + Cc];
				}
			}
			auto Put =[&](size_t At, uint32_t Rgb, float Shade, const float* K) {
				const float Ch[3] = { static_cast<float>((Rgb >> 16) & 0xFF), static_cast<float>((Rgb >> 8) & 0xFF), static_cast<float>(Rgb & 0xFF) };
				for (int A = 0; A < 3; ++A)
				{
					const float V2 = Ch[A] * Shade * (K ? K[A] : 1.f);
					Lk.Rgb[At * 3 + static_cast<size_t>(A)] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, V2 + 0.5f)));
				}
			};
			// (a paint the army's table names: its colour there; one past them, the file's own (OwnPaints); else the tunic's)
			const size_t ArmyPaints = PaintNames().size();
			auto ColourOf = [&](uint8_t Pa) -> uint32_t {
				if (Pa < ArmyPaints) return Colours.Paint[Pa];
				const auto Own = S.OwnPaints.find(Pa);
				return Own != S.OwnPaints.end() ? Own->second : Colours.Paint[0];
			};
			// (the worn look: the recipe where the mesh carries the wear, else as before; the roughness only where asked)
			Lk.Rough.clear();
			const bool bRough = Wm.bOn && Wm.bRough;
			if (bRough) Lk.Rough.resize(static_cast<size_t>(Lk.Points));
			PaintWear Pw;
			if (Wm.bOn) PaintWearOf(S, Pw);
			for (int P = 0; P < Body; ++P)
			{
				const size_t Pz = static_cast<size_t>(P);
				const uint8_t Pa = V.M.Paint[Pz];
				Lk.Paint[Pz] = Pa;
				if (bWorn)
				{
					// (the mesh's own tint left out: but for the face's detail, in Mul above, it is the baked look's earthy
					// cast, the dry look at W 0.6, which the recipe makes afresh from the field's soil)
					const WornAt A = WornOf(V.M, Pz, Wm, W, Pw.bGore[Pa]);
					WornColour(Wm, A, ColourOf(Pa), &Mul[Pz * 3], &Lk.Rgb[Pz * 3]);
					if (bRough) Lk.Rough[Pz] = WornRough(Pw.Rough[Pa], A, Wm);
					continue;
				}
				float K[3] = { Mul[Pz * 3], Mul[Pz * 3 + 1], Mul[Pz * 3 + 2] };
				if (V.M.Tint.size() == static_cast<size_t>(Body) * 3)
					for (size_t A = 0; A < 3; ++A) K[A] *= V.M.Tint[Pz * 3 + A];
				Put(Pz, ColourOf(Pa), V.M.Shade.empty() ? 1.f : V.M.Shade[Pz], K);
				if (bRough) Lk.Rough[Pz] = WornRough(Pw.Rough[Pa], WornAt(), Wm);
			}
			if (Fi) FinishColours(S, Fi->Lods[L], Colours, Wm, W, Lk, static_cast<size_t>(Body));
		}

		/** A finish's points' paints and colours (its ink, or the army's colour for its paint, times its shade and tint),
		 * written into a look from point At on; with the worn look on, the ink or paint by the files' wear recipe where
		 * the finish carries it, and with Wm.bRough each point's roughness (into Lk.Rough, already made whole). */
		static void FinishColours(const Soldier& S, const Mesh& Fm, const ArmyColours& Colours, const Crowd::WearMix& Wm, float W, Look& Lk, size_t At)
		{
			const size_t ArmyPaints = PaintNames().size();
			const bool bWorn = Wm.bOn && HasWear(Fm);
			const bool bRough = Wm.bOn && Wm.bRough && Lk.Rough.size() >= At + static_cast<size_t>(Fm.Points);
			PaintWear Pw;
			if (Wm.bOn) PaintWearOf(S, Pw);
			for (int P = 0; P < Fm.Points; ++P)
			{
				const size_t Pz = static_cast<size_t>(P);
				const uint8_t Pa = Fm.Paint[Pz];
				const int Ink = Fm.Ink.empty() ? -1 : Fm.Ink[Pz];
				Lk.Paint[At + Pz] = Pa;
				uint32_t Rgb = 0;
				if (Ink >= 0) Rgb = S.Inks[static_cast<size_t>(Ink)];
				else if (Pa < ArmyPaints) Rgb = Colours.Paint[Pa];
				else
				{
					const auto Own = S.OwnPaints.find(Pa);
					Rgb = Own != S.OwnPaints.end() ? Own->second : Colours.Paint[0];
				}
				if (bWorn)
				{
					const WornAt A = WornOf(Fm, Pz, Wm, W, Pw.bGore[Pa]);
					WornColour(Wm, A, Rgb, nullptr, &Lk.Rgb[(At + Pz) * 3]);
					if (bRough) Lk.Rough[At + Pz] = WornRough(Pw.Rough[Pa], A, Wm);
					continue;
				}
				if (bRough) Lk.Rough[At + Pz] = WornRough(Pw.Rough[Pa], WornAt(), Wm);
				const float Shade = Fm.Shade.empty() ? 1.f : Fm.Shade[Pz];
				const float* K = Fm.Tint.size() == static_cast<size_t>(Fm.Points) * 3 ? &Fm.Tint[Pz * 3] : nullptr;
				const float Ch[3] = { static_cast<float>((Rgb >> 16) & 0xFF), static_cast<float>((Rgb >> 8) & 0xFF), static_cast<float>(Rgb & 0xFF) };
				for (int A = 0; A < 3; ++A)
				{
					const float V2 = Ch[A] * Shade * (K ? K[A] : 1.f);
					Lk.Rgb[(At + Pz) * 3 + static_cast<size_t>(A)] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, V2 + 0.5f)));
				}
			}
		}

		/** A finish alone at level L (a helmet drawn as its own instance): its points' look, its own triangles. */
		static void HelmetLookOf(const Soldier& S, const Finish& Fi, const ArmyColours& Colours, Lod L, const Crowd::WearMix& Wm, float W, Look& Lk)
		{
			const Mesh& Fm = Fi.Lods[L];
			Lk.Points = Fm.Points;
			Lk.Triangles = Fm.Triangles;
			Lk.Paint.assign(static_cast<size_t>(Fm.Points), 0);
			Lk.Rgb.assign(static_cast<size_t>(Fm.Points) * 3, 0);
			Lk.Rough.clear();
			if (Wm.bOn && Wm.bRough) Lk.Rough.assign(static_cast<size_t>(Fm.Points), 255);
			FinishColours(S, Fm, Colours, Wm, W, Lk, 0);
		}

		void AttachBank(Man& M)
		{
			M.Banked = M.Var[Far] ? BankOf(*M.S, *M.Var[Far]) : nullptr;
			for (int L = 0; L < LodCount; ++L)
			{
				const Lod Lv = static_cast<Lod>(L);
				M.BankAt[L] = M.Banked && Lv != Near && BanksAt(Lv) && M.Var[L] ? (Lv == Far ? M.Banked : BankOf(*M.S, *M.Var[L])) : nullptr;
				M.FaceMoved[L] = M.BankAt[L] && M.FaceAt[L] ? FaceMovedOf(*M.Var[L], *M.FaceAt[L]) : nullptr;
			}
			M.BankK = -1;
			M.ShowA = M.ShowB = M.WasA = M.WasB = -1;
			M.ShowW = M.WasW = 0.f;
			M.BankKey = -1;
			M.BodyKey = -1;
			if (!M.Banked || !M.Colours) return;
			AttachBody(M);
			// (his wear in the key with the worn look on: two men whose W differs are never drawn with one look)
			const auto Key = std::make_tuple(M.Var[Far], M.Info.Finish, M.Colours, WearKeyOf(M));
			const auto Found = BankSetOf.find(Key);
			if (Found != BankSetOf.end()) { M.BankKey = Found->second; return; }
			BankSet Set;
			Set.S = M.S;
			Set.B = M.Banked;
			Set.Finish = M.Info.Finish;
			Set.W = M.Info.Wear;
			const Finish* Fi = Set.Finish >= 0 ? &M.S->Finishes[static_cast<size_t>(Set.Finish)] : nullptr;
			MakeLook(*M.S, *M.Var[Far], Fi, *M.Colours, nullptr, Far, Wear, Set.W, Set.Lk);
			M.BankKey = static_cast<int>(BankSets.size());
			BankSetOf[Key] = M.BankKey;
			BankSets.push_back(std::move(Set));
		}

		/** The bank's pose nearest clip ClipIx at time T (a loop's phase round the circle; a one-shot's clamped), or -1. */
		int PickBank(const Man& M, int ClipIx, float T) const
		{
			const Bank* B = M.Banked;
			if (!B || ClipIx < 0 || static_cast<size_t>(ClipIx) >= B->CycleOf.size()) return -1;
			const std::vector<int>& Cy = B->CycleOf[static_cast<size_t>(ClipIx)];
			if (Cy.empty()) return -1;
			const Clip& C = M.S->Clips[static_cast<size_t>(ClipIx)];
			float U = C.Long > 0.f ? T / C.Long : 0.f;
			if (C.bLoop) U -= std::floor(U);
			else U = Clamp01(U);
			int Best = -1;
			float BestD = 2.f;
			for (const int K : Cy)
			{
				float D = std::fabs(BankSpec[static_cast<size_t>(K)].Phase - U);
				if (C.bLoop) D = std::min(D, 1.f - D);
				if (D < BestD) { BestD = D; Best = K; }
			}
			return Best;
		}

		/** Uniforms that age: the army's row with its cloth faded by the sun and the wash, Step twentieths of the way to a
		 * year and a half in the line: greyer (some 28 in 100 of the colour gone at the most) and paler (a twelfth
		 * lighter); the webbing and pack half that (canvas, already pale). Cloth only: tunic, trousers, coat and blanket,
		 * webbing and pack; boots, leather, helmet, skin and steel as they were. Made once a row and step (AgedRows), so
		 * the men of a side stay one set alike. */
		const ArmyColours& AgedRow(const ArmyColours& Row, int Step, int Garment)
		{
			const auto Key = std::make_tuple(&Row, Step, Garment);
			const auto Found = AgedRows.find(Key);
			if (Found != AgedRows.end()) return Found->second;
			ArmyColours Out = Row;
			for (const int Pa : { 0, 1, 3, 4, 6, 10, 11, 13 })
			{
				const double A = (Pa == 3 || Pa == 4 || Pa == 6 ? 0.5 : 1.0) * Step / 20.0;
				const uint32_t C = Row.Paint[Pa];
				double Ch[3] = { static_cast<double>((C >> 16) & 0xFF), static_cast<double>((C >> 8) & 0xFF), static_cast<double>(C & 0xFF) };
				const double Grey = 0.2126 * Ch[0] + 0.7152 * Ch[1] + 0.0722 * Ch[2];
				uint32_t Faded = 0;
				for (int K = 0; K < 3; ++K)
				{
					const double V = (Ch[K] + (Grey - Ch[K]) * 0.28 * A) * (1.0 + 0.08 * A);
					Faded = (Faded << 8) | static_cast<uint32_t>(std::clamp(std::lround(V), 0L, 255L));
				}
				Out.Paint[Pa] = Faded;
			}
			// (a newer tunic (0) or trousers (1): the row's own colour before the fading, a twentieth darker)
			if (Garment == 0 || Garment == 1)
			{
				const uint32_t C = Row.Paint[Garment];
				uint32_t Newer = 0;
				for (int K = 0; K < 3; ++K) Newer = (Newer << 8) | static_cast<uint32_t>(std::lround(((C >> (16 - 8 * K)) & 0xFF) * 0.95));
				Out.Paint[Garment] = Newer;
			}
			return AgedRows.emplace(Key, Out).first->second;
		}

		/** His own look's wear on top of his faded row (the near men, whose looks are their own; too small to see past
		 * them): with his side's wear, up to some 55 in 100 men with a patch or two of newer cloth at an elbow or a knee,
		 * each point's baked colour scaled by the cloth it now is over the cloth it was. (His newer tunic or trousers is
		 * his row's, AgedRow, so the same at every range.) */
		void AgeLook(const Man& Mn, Lod L, Look& Lk) const
		{
			if (!Mn.Fresh || Mn.WornA <= 0.f) return;
			const Soldier& S = *Mn.S;
			const Variant& V = *Mn.Var[L];
			const double A = Mn.WornA;
			// (a rider has no track: his own seed, PhaseSeed, tells his patches from another rider's)
			const int LookOf = Mn.Track >= 0 ? Mn.Track : Mn.LookTrack;
			const uint64_t Who = LookOf >= 0 ? (static_cast<uint64_t>(static_cast<uint32_t>(LookOf)) << 20) ^ 0xA6EDull
				: static_cast<uint64_t>(static_cast<double>(Mn.PhaseSeed) * 4294967296.0) ^ 0xB1DEull;
			auto Ratio = [](uint32_t To, uint32_t From, double Value, float* Out) {
				for (int K = 0; K < 3; ++K)
				{
					const double F = static_cast<double>((From >> (16 - 8 * K)) & 0xFF), T = static_cast<double>((To >> (16 - 8 * K)) & 0xFF);
					Out[K] = static_cast<float>(F > 0.5 ? T * Value / F : 1.0);
				}
			};
			auto Scale = [&](size_t P, const float* R) {
				for (size_t K = 0; K < 3; ++K)
				{
					const float C = static_cast<float>(Lk.Rgb[P * 3 + K]) * R[K];
					Lk.Rgb[P * 3 + K] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, C + 0.5f)));
				}
			};
			const size_t Body = static_cast<size_t>(V.M.Points);
			// (a patch or two of newer cloth, a darker square at an elbow or a knee)
			if (Unit01(Who ^ 0x4ull) < 0.55 * A)
			{
				static const char* const Joints[] = { "lowerarm_l", "lowerarm_r", "calf_l", "calf_r" };
				const int Count = Unit01(Who ^ 0x5ull) < 0.4 * A ? 2 : 1;
				const int First = static_cast<int>(Unit01(Who ^ 0x6ull) * 4.0) % 4;
				for (int N = 0; N < Count; ++N)
				{
					const int Bi = S.BoneIndex(Joints[(First + N * 2 + N) % 4]);
					if (Bi < 0) continue;
					const Vec3& H = S.Bones[static_cast<size_t>(Bi)].Head;
					for (size_t P = 0; P < Body && P < Lk.Paint.size(); ++P)
					{
						const int Pa = Lk.Paint[P];
						if (Pa != 0 && Pa != 1) continue;
						const double DX = V.M.At[P * 3] - H.X, DY = V.M.At[P * 3 + 1] - H.Y, DZ = V.M.At[P * 3 + 2] - H.Z;
						if (DX * DX + DY * DY + DZ * DZ > 0.09 * 0.09) continue;
						float R[3];
						Ratio(Mn.Fresh->Paint[Pa], Mn.Colours->Paint[Pa], 0.86, R);
						Scale(P, R);
					}
				}
			}
		}

		/** His body set (BodyKey): made the first time a man of it is met, each banked level's bake and look. */
		void AttachBody(Man& M)
		{
			// (his wear in the key with the worn look on, as BankSetOf's)
			const auto Key = std::make_tuple(M.Var[Far], M.Colours, WearKeyOf(M));
			const auto Found = BodySetOf.find(Key);
			int K = -1;
			if (Found != BodySetOf.end()) K = Found->second;
			else
			{
				K = static_cast<int>(BodySets.size());
				BodySetOf[Key] = K;
				BodySet Set;
				Set.S = M.S;
				Set.Colours = M.Colours;
				Set.W = M.Info.Wear;
				// (the bone the finishes ride: every soldier file's finishes are rigid on the head)
				for (const Finish& Fi : M.S->Finishes)
				{
					if (Fi.Lods[Far].Points > 0) { Set.HeadBone = Fi.Lods[Far].Part[0]; break; }
				}
				if (Set.HeadBone < 0) Set.HeadBone = M.S->BoneIndex("head");
				BodySets.push_back(std::move(Set));
			}
			M.BodyKey = K;
			BodySet& Set = BodySets[static_cast<size_t>(K)];
			for (int L = Mid; L < LodCount; ++L)
			{
				if (Set.B[L] || !M.BankAt[L]) continue;
				const Lod Lv = static_cast<Lod>(L);
				Set.Var[L] = M.Var[L];
				Set.B[L] = M.BankAt[L];
				MakeLook(*M.S, *M.Var[L], nullptr, *M.Colours, nullptr, Lv, Wear, Set.W, Set.Lk[L]);
				Set.Helmets[L].resize(M.S->Finishes.size());
				for (size_t F = 0; F < M.S->Finishes.size(); ++F) HelmetLookOf(*M.S, M.S->Finishes[F], *M.Colours, Lv, Wear, Set.W, Set.Helmets[L][F]);
			}
		}

		/** Clip ClipIx at time T as the two of the bank's poses either side of where it has got to (a loop's round the
		 * circle; a one-shot's clamped at its ends) and the second's share; false where the bank has none of it. */
		bool BracketBank(const Man& M, int ClipIx, float T, int& OutA, int& OutB, float& OutW) const
		{
			const Bank* B = M.Banked;
			if (!B || ClipIx < 0 || static_cast<size_t>(ClipIx) >= B->CycleOf.size()) return false;
			const std::vector<int>& Cy = B->CycleOf[static_cast<size_t>(ClipIx)];
			if (Cy.empty()) return false;
			const Clip& C = M.S->Clips[static_cast<size_t>(ClipIx)];
			float U = C.Long > 0.f ? T / C.Long : 0.f;
			if (C.bLoop) U -= std::floor(U);
			else U = Clamp01(U);
			// (the pose at or before U, and the one after it; round the circle for a loop)
			int Lo = -1, Hi = -1;
			float LoD = 2.f, HiD = 2.f;
			for (const int K : Cy)
			{
				const float Ph = BankSpec[static_cast<size_t>(K)].Phase;
				float Back = U - Ph, Ahead = Ph - U;
				if (C.bLoop)
				{
					Back -= std::floor(Back);
					Ahead -= std::floor(Ahead);
				}
				if (Back >= 0.f && Back < LoD) { LoD = Back; Lo = K; }
				if (Ahead > 0.f && Ahead < HiD) { HiD = Ahead; Hi = K; }
			}
			if (Lo < 0 || Hi < 0 || Lo == Hi)
			{
				OutA = OutB = Lo >= 0 ? Lo : Hi;
				OutW = 0.f;
				return OutA >= 0;
			}
			OutA = Lo;
			OutB = Hi;
			OutW = LoD / std::max(1e-6f, LoD + HiD);
			return true;
		}

		/** A banked man's body at a banked level: the bake moved by his place, his face's moved points skinned on his
		 * frames. */
		static void SkinBanked(const Man& M, Lod L, float* OutAt, float* OutFacing)
		{
			const Bank& B = *M.BankAt[L];
			const size_t Off = static_cast<size_t>(M.BankK) * static_cast<size_t>(B.Points) * 3;
			const float* Pa = B.At.data() + Off;
			const float* Fa = B.Facing.data() + Off;
			const Xform& W = M.World;
			const size_t N = static_cast<size_t>(B.Points);
			for (size_t I = 0; I < N * 3; I += 3)
			{
				const float X = Pa[I], Y = Pa[I + 1], Z = Pa[I + 2];
				OutAt[I] = W.R[0] * X + W.R[1] * Y + W.R[2] * Z + W.T[0];
				OutAt[I + 1] = W.R[3] * X + W.R[4] * Y + W.R[5] * Z + W.T[1];
				OutAt[I + 2] = W.R[6] * X + W.R[7] * Y + W.R[8] * Z + W.T[2];
				if (!OutFacing) continue;
				const float NX = Fa[I], NY = Fa[I + 1], NZ = Fa[I + 2];
				OutFacing[I] = W.R[0] * NX + W.R[1] * NY + W.R[2] * NZ;
				OutFacing[I + 1] = W.R[3] * NX + W.R[4] * NY + W.R[5] * NZ;
				OutFacing[I + 2] = W.R[6] * NX + W.R[7] * NY + W.R[8] * NZ;
			}
			if (M.FaceMoved[L] && !M.FaceMoved[L]->empty())
			{
				const Mesh& Me = M.Var[L]->M;
				const FaceRest& R = *M.FaceAt[L];
				for (const int P : *M.FaceMoved[L])
				{
					SkinPoint(static_cast<size_t>(P), R.At.data(), R.Facing.data(), Me.Part.data(), Me.Part2.data(), Me.W2.data(), M.Frames,
						OutAt, OutFacing);
				}
			}
		}

		/** His file's mesh at level L with those runs of its triangles left out (VariantCuts), made once: the points only
		 * the runs use left out (a point a kept triangle uses stays), the rest in the file's order, and the triangles in
		 * the file's order without the runs'. A run the file has not at that level is no cut (CutsOf leaves a pack cut
		 * off there, so such a man shares the mesh of the man with his pack). */
		const Variant& VariantOf(const Soldier* S, int Cuts, int L)
		{
			const auto Key = std::make_tuple(S, Cuts, L);
			auto Found = Variants.find(Key);
			if (Found != Variants.end()) return Found->second;
			Variant& V = Variants[Key];
			const Mesh& Full = S->Meshes[L];
			const size_t N = static_cast<size_t>(Full.Points);
			const size_t Tris = Full.Triangles.size() / 3;
			std::vector<bool> Keep(N, true);
			// (the runs left out, [first, first + count) each: his own helmet's, then (Effects, R2) his pack's)
			size_t RunFirst[2] = { Tris, Tris }, RunCount[2] = { 0, 0 };
			if ((Cuts & CutHelmet) != 0 && S->HelmetCount[L] > 0)
			{
				RunFirst[0] = static_cast<size_t>(S->HelmetFirst[L]);
				RunCount[0] = static_cast<size_t>(S->HelmetCount[L]);
			}
			if ((Cuts & CutPack) != 0 && S->PackCount[L] > 0)
			{
				RunFirst[1] = static_cast<size_t>(S->PackFirst[L]);
				RunCount[1] = static_cast<size_t>(S->PackCount[L]);
			}
			const auto Cut = [&RunFirst, &RunCount](size_t T) {
				return (T >= RunFirst[0] && T < RunFirst[0] + RunCount[0]) || (T >= RunFirst[1] && T < RunFirst[1] + RunCount[1]);
			};
			if (RunCount[0] > 0 || RunCount[1] > 0)
			{
				// The points only the runs' triangles use (in the snow files a few of the helmet's are shared with the hood,
				// and a strap's may be the cloth's: kept).
				std::vector<uint8_t> Use(N, 0);
				for (size_t T = 0; T < Tris; ++T)
				{
					const bool bCut = Cut(T);
					for (int C = 0; C < 3; ++C) Use[Full.Triangles[T * 3 + static_cast<size_t>(C)]] |= bCut ? 1 : 2;
				}
				for (size_t I = 0; I < N; ++I) Keep[I] = Use[I] != 1;
			}
			V.ToCompact.assign(N, -1);
			Mesh& M = V.M;
			for (size_t I = 0; I < N; ++I)
			{
				if (!Keep[I]) continue;
				V.ToCompact[I] = M.Points++;
				for (int C = 0; C < 3; ++C)
				{
					M.At.push_back(Full.At[I * 3 + static_cast<size_t>(C)]);
					M.Facing.push_back(Full.Facing[I * 3 + static_cast<size_t>(C)]);
				}
				M.Paint.push_back(Full.Paint[I]);
				M.Part.push_back(Full.Part[I]);
				M.Part2.push_back(Full.Part2[I]);
				M.W2.push_back(Full.W2[I]);
				if (!Full.Shade.empty()) M.Shade.push_back(Full.Shade[I]);
				if (!Full.Tint.empty())
					for (size_t C = 0; C < 3; ++C) M.Tint.push_back(Full.Tint[I * 3 + C]);
				// (release 2's wear and weather, where the file was read with it: SetWearMix)
				if (!Full.ShadeClean.empty()) M.ShadeClean.push_back(Full.ShadeClean[I]);
				if (!Full.Dirt.empty()) M.Dirt.push_back(Full.Dirt[I]);
				if (!Full.DirtWet.empty()) M.DirtWet.push_back(Full.DirtWet[I]);
				if (!Full.Wet.empty()) M.Wet.push_back(Full.Wet[I]);
				if (!Full.Snow.empty()) M.Snow.push_back(Full.Snow[I]);
			}
			for (size_t T = 0; T < Tris; ++T)
			{
				if (Cut(T)) continue;
				for (int C = 0; C < 3; ++C) M.Triangles.push_back(static_cast<uint32_t>(V.ToCompact[Full.Triangles[T * 3 + static_cast<size_t>(C)]]));
			}
			return V;
		}

		const FaceRest& FaceRestOf(const Soldier* S, int Cuts, int L, int FaceIx)
		{
			const auto Key = std::make_tuple(S, Cuts, L, FaceIx);
			auto Found = FaceRests.find(Key);
			if (Found != FaceRests.end()) return Found->second;
			const Variant& V = VariantOf(S, Cuts, L);
			FaceRest& R = FaceRests[Key];
			R.At = V.M.At;
			R.Facing = V.M.Facing;
			if (FaceIx >= 0 && FaceIx < static_cast<int>(S->Faces.size()))
			{
				const Face::Level& Lv = S->Faces[static_cast<size_t>(FaceIx)].Lods[L];
				for (size_t K = 0; K < Lv.At.size(); ++K)
				{
					const int C = V.ToCompact[static_cast<size_t>(Lv.At[K])];
					if (C < 0) continue;
					for (int A = 0; A < 3; ++A)
					{
						R.At[static_cast<size_t>(C) * 3 + static_cast<size_t>(A)] += Lv.Move[K * 3 + static_cast<size_t>(A)];
						if (Lv.Facing.size() == Lv.Move.size()) R.Facing[static_cast<size_t>(C) * 3 + static_cast<size_t>(A)] = Lv.Facing[K * 3 + static_cast<size_t>(A)];
					}
				}
			}
			return R;
		}

		bool OwnHelmet(const Man& M) const { return M.Info.Finish < 0 && !M.bBareHead; }
		/** The runs his mesh at level L leaves out (VariantCuts): his own helmet where a finish is drawn over him or he is
		 * bareheaded; his pack where he goes without it (Effects, R2) and his file marks it at that level. */
		int CutsOf(const Man& M, int L) const
		{
			return (OwnHelmet(M) ? 0 : CutHelmet) | (M.bNoPack && M.S->PackCount[L] > 0 ? CutPack : 0);
		}

		// ---------------------------------------------------------------------------------------------------------
		// Poses of the rules, drawn.
		// ---------------------------------------------------------------------------------------------------------
		/** The rules' cover a kneeling man fires over (drill.ts CoverKind): fieldworks, a wall or a window, a tree, a rock, a wreck. */
		static bool LowCover(const std::string& Cover)
		{
			return Cover == "works" || Cover == "building" || Cover == "woods" || Cover == "rock" || Cover == "wreck";
		}

		Stance StanceOf(const Soldier& S, const std::string& Role, const std::string& Pose, bool bMoving, int Serves = 0, const std::string& Cover = std::string()) const
		{
			Stance St;
			St.Pose = Pose;
			auto Use = [&](const char* Clip, Stance::How How, Class Cl) {
				St.Clip = S.ClipIndex(Clip);
				St.Play = How;
				St.Cl = Cl;
			};
			const bool bGunner = S.Weapon == "lmg";
			if (Serves != 0 && Pose == "wait" && (Role == "hmg" || Role == "mortar"))
			{
				// (release 2's "teamguns": set down a moment, he is at his weapon still, as deployed, not kneeling apart
				// with his rifle up)
				Stance AtHer = StanceOf(S, Role, "deployed", bMoving, Serves, Cover);
				AtHer.Pose = Pose;
				return AtHer;
			}
			// A section's machine gunner halted (release 2; Brian, 25 November 16:06Z: "a soldier would never shoot this
			// like a rifle position"): he is down behind his gun on her bipod, waiting as he fires (the rules have him
			// fire so already, lmg_burst). He kneels with her at his hip (his file's fire_kneel, a hip hold) only where
			// something low stands before him to fire over, and stands with her at his hip only for the bursts of fire on
			// the move (fire_stand). A rifleman kneels to wait as before.
			if (bGunner && !bMoving && (Pose == "wait" || Pose == "fire_kneel") && !LowCover(Cover) && S.ClipIndex("fire_prone") >= 0)
			{
				Use("fire_prone", Stance::Hold, Prone);
				St.bFiring = true;
				return St;
			}
			if (Pose == "walk" || Pose == "run" || Pose == "advance" || Pose == "creep" || Pose == "flee") Use(Pose.c_str(), Stance::Gait, Up);
			else if (Pose == "crawl") Use("crawl", Stance::Gait, Prone);
			else if (Pose == "idle") Use("idle", Stance::Loop, Up);
			else if (Pose == "cower") Use("cower", Stance::Loop, Kneel);
			else if (Pose == "fire_stand" || Pose == "throw_grenade") { Use("fire_stand", Stance::Hold, Up); St.bFiring = true; }
			else if (Pose == "fire_kneel" || Pose == "wait") { Use("fire_kneel", Stance::Hold, Kneel); St.bFiring = true; }
			else if (Pose == "fire_prone" || Pose == "hide") { Use("fire_prone", Stance::Hold, Prone); St.bFiring = true; }
			else if (Pose == "observe" && S.ClipIndex("radio_call") >= 0)
			{
				// The radio man (the American's soldier_USA_radio): kneeling, the handset at his ear, calling the guns in
				// (radio_call, his own 8 s loop: the squelch, his words, the call going through).
				Use("radio_call", Stance::Loop, Kneel);
				St.bRifleDown = true;
			}
			else if (Pose == "observe")
			{
				// The spotter lying with his glasses up: fire_prone with his rifle laid down in front of him (there is no
				// glasses clip; his hands stay up where they held the rifle, which reads as glasses at a distance).
				Use("fire_prone", Stance::Hold, Prone);
				St.bRifleDown = true;
			}
			else if (Pose == "lmg_burst")
			{
				// The gunner's clip puts the gun on its bipod; on a rifleman's file it would put his rifle where the gun
				// goes (the README), so a rifleman holds fire_prone's aim and his bursts come from the rounds.
				Use(bGunner ? "lmg_burst" : "fire_prone", Stance::Hold, Prone);
				St.bFiring = true;
			}
			else if (Role == "hmg" && (Pose == "fire" || Pose == "deployed"))
			{
				// A support team's heavy machine gun, drawn with riflemen until it has a model: the gunner lying behind
				// his gun, as fire_prone holds (lmg_burst where the file carries the gun). A proper team needs its own
				// file: the gun on its tripod (a weapon bone of its own, its traverse and the belt), the gunner sitting or
				// lying behind it with his hands on the grips, the loader beside him feeding the belt, and a burst clip
				// with the tripod's own recoil; and the crew carrying gun, tripod and boxes when it moves.
				Use(bGunner ? "lmg_burst" : "fire_prone", Stance::Hold, Prone);
				St.bFiring = true;
				if (Serves != 0)
				{
					// (release 2's "teamguns": his gun stands at his place on its mount, Battle/LandTeamGun, so his rifle
					// is laid down and his hands are on her: lying behind a low gun, where fire_prone's hands reach her
					// grips; kneeling behind a high one or one on wheels, as fire_kneel holds, until his file brings the
					// soldiers' own clips, hmg_fire_prone and hmg_fire_sit)
					const char* Own = Serves == 2 ? "hmg_fire_sit" : "hmg_fire_prone";
					const char* Else = Serves == 2 ? "fire_kneel" : "fire_prone";
					Use(S.ClipIndex(Own) >= 0 ? Own : Else, Stance::Hold, Serves == 2 ? Kneel : Prone);
					St.bRifleDown = true;
				}
			}
			else if (Role == "mortar" && Serves != 0 && (Pose == "fire" || Pose == "deployed"))
			{
				// (release 2's "teamguns": his tube stands at his right knee, so his rifle is laid down and his hands are
				// up at her sight: mortar_sight where his file has it, else fire_kneel's hands, which are there)
				Use(S.ClipIndex("mortar_sight") >= 0 ? "mortar_sight" : "fire_kneel", Stance::Hold, Kneel);
				St.bRifleDown = true;
			}
			else if (Role == "mortar" && (Pose == "fire" || Pose == "deployed"))
			{
				// A support team's mortar, drawn with riflemen until it has a model: each man kneels by his tube, rifle at
				// the ready, as wait holds. A proper team needs the tube on its baseplate and bipod (a prop at his knee),
				// a clip of the loader dropping the bomb and ducking with his hands over his ears, the gunner laying the
				// sight between rounds, and the bearers with their ammunition cases.
				Use("fire_kneel", Stance::Hold, Kneel);
			}
			else if ((Role == "loader" || Role == "crew") && (Pose == "fire" || Pose == "deployed"))
			{
				// A gun's crew and an MG's or a tube's number two serve the gun, not their rifles: kneeling at it, a crew
				// man's rifle laid down by him, a loader's in his hands (no clip of his own yet: a crew's of loading and
				// laying, a number two's of feeding the belt or dropping the bomb, are wanted).
				Use("fire_kneel", Stance::Hold, Kneel);
				St.bRifleDown = Role == "crew";
			}
			else if (Pose == "fire" || Pose == "deployed") { Use("fire_kneel", Stance::Hold, Kneel); St.bFiring = true; }
			else if (Pose == "mounted" && S.ClipIndex("car_ride_back") >= 0)
			{
				// Aboard a lorry or a half-track, his file with release 2's seated clips: sitting on her bench as a car's
				// back seat holds him (the scene's SeatFn puts him there, facing inboard), until the bench's own clips come.
				Use("car_ride_back", Stance::Loop, Kneel);
				St.Aboard = 1;
				// (a lorry that Men in lorries rides keeps him hidden under her canvas, her rider drawn on her bench
				// instead: ALandBattle::RideLorries; elsewhere he is seated and drawn)
				St.bUnderCanvas = true;
			}
			else if (Pose == "mounted" || Pose == "ride")
			{
				// Aboard a lorry with no seated clip in his file: kneeling on her bed's floor by his bench (SeatFn). On a
				// horse: kneeling where the rules have him (no rider's clip yet).
				Use("fire_kneel", Stance::Hold, Kneel);
				// (in a lorry he is not drawn where the rules have him: he knelt on the ground under her, "troops are stuck
				// under the truck", Brian, 23 November 14:10Z. With Men in lorries on, the lorries' riders are drawn on her
				// benches (ALandBattle::RideLorries) and he is hidden under her canvas; in a carrier, or a lorry with the
				// row off, he is seated in her bed (SeatFn).)
				St.bUnderCanvas = Pose == "mounted";
				if (Pose == "mounted") St.Aboard = 2;
			}
			else if (Pose == "horses") Use(bMoving ? "walk" : "idle", bMoving ? Stance::Gait : Stance::Loop, Up);
			// Release 2's stretcher bearers and linemen (the tactics thread's poses; the soldiers thread's field clips,
			// each file's own; a file without them goes as its pose bank's `instead`: walk, fire_kneel)
			else if (Pose == "stretcher_front" || Pose == "stretcher_rear")
			{
				const bool bOwn = S.ClipIndex(Pose.c_str()) >= 0;
				Use(bOwn ? Pose.c_str() : bMoving ? "walk" : "idle", bMoving ? Stance::Gait : bOwn ? Stance::Hold : Stance::Loop, Up);
				St.bRifleDown = !bOwn; // (his rifle slung across his back in the clip; laid down where he walks it)
			}
			else if (Pose == "wounded" || Pose == "carried")
			{
				// Alive on his back, head to his front: waiting on the ground (wounded_waiting: a hand on the wound, an
				// arm lifted once a loop to call the bearers, his rifle laid by him), or carried head first on the canvas
				// (carried_wounded: breathing, a knee drawn up; Lift, the stretcher's `lie` over the bearers' fists).
				const char* Own = Pose == "carried" ? "carried_wounded" : S.ClipIndex("wounded_waiting") >= 0 ? "wounded_waiting" : "carried_wounded";
				if (S.ClipIndex(Own) >= 0) Use(Own, Stance::Loop, Prone);
				else Use("fire_prone", Stance::Hold, Prone);
				St.bRifleDown = true;
				St.Lift = Pose == "carried" ? CarriedLift : 0.f;
			}
			else if (Pose == "splice")
			{
				if (S.ClipIndex("splice_kneel") >= 0) Use("splice_kneel", Stance::Loop, Kneel);
				else Use("fire_kneel", Stance::Hold, Kneel);
				St.bRifleDown = true;
			}
			else if (Pose == "reel")
			{
				// The lineman walking his line out, the reel before him (reel_walk; a file without it walks).
				const bool bOwn = S.ClipIndex("reel_walk") >= 0;
				Use(bOwn ? "reel_walk" : bMoving ? "walk" : "idle", bOwn || bMoving ? Stance::Gait : Stance::Loop, Up);
				St.bRifleDown = !bOwn;
			}
			else Use("idle", Stance::Loop, Up);
			if (St.Clip < 0) { St.Clip = S.ClipIndex("idle"); St.Play = Stance::Loop; }
			if (St.Clip < 0) St.Clip = 0;
			return St;
		}

		static Segment::Kind KindOf(Stance::How H)
		{
			return H == Stance::Gait ? Segment::Gait : H == Stance::Loop ? Segment::Loop : Segment::Hold;
		}

		/** His own runner for a gait's clip (SetRunStyles): one of the clip's styles by his StylePick, else the clip. */
		int StyleOf(const Man& M, int Clip) const
		{
			if (!bRunStyles || Clip < 0 || !M.S || static_cast<size_t>(Clip) >= M.S->Styles.size()) return Clip;
			const std::vector<int>& V = M.S->Styles[static_cast<size_t>(Clip)];
			if (V.size() < 2) return Clip;
			const size_t K = std::min(V.size() - 1, static_cast<size_t>(M.StylePick * static_cast<float>(V.size())));
			return V[K];
		}

		Segment SegmentOf(const Man& M, const Stance& St, double Start, float BlendIn) const
		{
			Segment G;
			G.Clip = St.Clip;
			G.How = KindOf(St.Play);
			G.Start = Start;
			G.HoldT = St.HoldT;
			G.BlendIn = BlendIn;
			G.bRifleDown = St.bRifleDown;
			G.bFiring = St.bFiring;
			G.EndClass = St.Cl;
			G.LieA = G.LieB = St.Cl == Prone ? 1.f : 0.f;
			G.Rate = G.How == Segment::Loop ? M.Info.Jitter * static_cast<float>(Life) : 1.f;
			G.Phase = M.PhaseSeed;
			if (G.How == Segment::Gait) G.Clip = StyleOf(M, G.Clip);
			if (bClipShotsRef && *bClipShotsRef && St.bFiring && G.How == Segment::Hold)
			{
				// The test's switch: the firing poses loop their own clips again, shots and all.
				G.How = Segment::Loop;
				G.Rate = M.Info.Jitter * static_cast<float>(Life);
			}
			return G;
		}

		const bool* bClipShotsRef = nullptr;

		/** A one-shot of a clip from one time to another over Dur seconds. */
		Segment OnceOf(int ClipIx, float From, float To, double Start, double Dur, Class EndCl, float BlendIn) const
		{
			Segment G;
			G.Clip = ClipIx;
			G.How = Segment::Once;
			G.From = From;
			G.To = To;
			G.Start = Start;
			G.Dur = std::max(0.05, Dur);
			G.BlendIn = BlendIn;
			G.EndClass = EndCl;
			return G;
		}

		/** The time a turn from one class to another takes at its natural pace, and its clip. */
		struct Turnover
		{
			int Clip = -1;
			float From = 0.f, To = 0.f;
			double Natural = 0.0, Least = 0.0;
			bool bShift = false;
			float LieA = 0.f, LieB = 0.f;
		};

		Turnover TurnoverOf(const Soldier& S, Class From, Class To, bool bOffQuick) const
		{
			Turnover T;
			const int Kneel = S.ClipIndex("kneel_down"), Stand = S.ClipIndex("stand_up"), ProneIx = S.ClipIndex("go_prone");
			const float ProneLong = ProneIx >= 0 ? S.Clips[static_cast<size_t>(ProneIx)].Long : 1.9f;
			// go_prone is on one knee about half way down (its knee_down event): a kneeling man starts there, and a man
			// lying gets up to one knee by playing it back to there.
			float KneeAt = ProneLong * 0.49f;
			if (ProneIx >= 0)
			{
				for (const ClipEvent& E : S.Clips[static_cast<size_t>(ProneIx)].Events)
				{
					if (E.Fx == "knee_down") { KneeAt = E.T; break; }
				}
			}
			if (From == Up && To == Kneel && Kneel >= 0)
			{
				T.Clip = Kneel; T.From = 0.f; T.To = S.Clips[static_cast<size_t>(Kneel)].Long; T.Natural = T.To; T.Least = 0.25;
			}
			else if (From == Kneel && To == Up && Stand >= 0)
			{
				T.Clip = Stand; T.From = 0.f; T.To = S.Clips[static_cast<size_t>(Stand)].Long; T.Natural = T.To;
				T.Least = bOffQuick ? 0.3 : 0.35;
			}
			else if (To == Prone && From != Prone && ProneIx >= 0)
			{
				T.Clip = ProneIx; T.From = From == Kneel ? KneeAt : 0.f; T.To = ProneLong;
				T.Natural = T.To - T.From; T.Least = From == Kneel ? 0.4 : 0.6;
				T.bShift = true; T.LieA = T.From / ProneLong; T.LieB = 1.f;
			}
			else if (From == Prone && To != Prone && ProneIx >= 0)
			{
				// Getting up: go_prone played backwards, as the README has it (1.0 s to his feet, half that to a knee).
				T.Clip = ProneIx; T.From = ProneLong; T.To = To == Kneel ? KneeAt : 0.f;
				T.Natural = To == Kneel ? 0.5 : 1.0; T.Least = bOffQuick ? 0.35 : 0.4;
				T.bShift = true; T.LieA = 1.f; T.LieB = T.To / ProneLong;
			}
			return T;
		}

		/** The next round's start after a time, for squeezing a turn so he is on aim by then. */
		double NextShotStart(const Man& M, double After) const
		{
			for (size_t I = M.ShotCur; I < M.Shots.size(); ++I)
			{
				if (M.Shots[I].How == Shot::Clip || M.Shots[I].How == Shot::Burst)
				{
					if (M.Shots[I].Start >= After) return M.Shots[I].Start;
				}
			}
			return 1e18;
		}

		void TurnTo(Man& M, int ChIx, double At, double Until)
		{
			const double NextChange = static_cast<size_t>(ChIx) + 1 < M.Stances.size() ? M.Stances[static_cast<size_t>(ChIx) + 1].T + M.Info.Delay : 1e18;
			TurnToStance(M, M.Stances[static_cast<size_t>(ChIx)], At, Until, NextChange);
		}

		/** A stance turned to: one of his changes' (TurnTo), or the gait he leaves the field in (LeaveAt). */
		void TurnToStance(Man& M, const Stance& St, double At, double Until, double NextChange)
		{
			// A new stance from the rules: turn to it as a man would. Kneeling, going prone and getting up have their own
			// clips (kneel_down, go_prone, stand_up, go_prone backwards), squeezed to fit before he must move, fire or
			// change again (the squad brain changes pose every second or two in a bound); within a class it is a fifth
			// of a second's blend.
			const Class From = M.EndClass;
			M.Queue.clear();
			double Begin = At;
			const bool bOffQuick = St.Play == Stance::Gait;
			if (From != St.Cl)
			{
				const Turnover T = TurnoverOf(*M.S, From, St.Cl, bOffQuick);
				if (T.Clip >= 0)
				{
					double Avail = Until > At ? Until - At : 0.6 * (NextChange - At);
					Avail = std::min(Avail, NextShotStart(M, At) - At);
					// (the clips' seconds are life's: on the battle's clock, 1 / Life of them)
					const double Natural = T.Natural / std::max(0.5, static_cast<double>(M.Info.Jitter)) / Life;
					double Dur = std::min(Natural, Avail);
					if (bOffQuick && From == Prone) Dur = std::min(Dur, 0.45 / Life);
					if (bOffQuick && From == Kneel) Dur = std::min(Dur, 0.35 / Life);
					if (Dur >= T.Least * 0.6 / Life)
					{
						Dur = std::max(Dur, T.Least * 0.6 / Life);
						Segment G = OnceOf(T.Clip, T.From, T.To, At, Dur, St.Cl, 0.15f);
						G.bShift = T.bShift;
						G.LieA = T.LieA;
						G.LieB = T.LieB;
						Start(M, G, At);
						Begin = At + Dur;
					}
				}
			}
			Segment G = SegmentOf(M, St, Begin, Begin > At ? 0.12f : 0.2f);
			if (Begin > At) M.Queue.push_back(G);
			else
			{
				// Within a class, the same clip held at the same time (wait to fire_kneel, hide to fire_prone) is the same
				// pose: nothing to blend.
				const Segment& C = M.Cur;
				const bool bSame = C.Clip == G.Clip && C.How == G.How && C.How == Segment::Hold && C.HoldT == G.HoldT && C.bRifleDown == G.bRifleDown;
				if (bSame) { M.Cur.bFiring = G.bFiring; M.Cur.EndClass = G.EndClass; }
				else
				{
					if (G.How == Segment::Gait && M.Cur.How == Segment::Gait && M.Cur.Clip >= 0)
					{
						// Keep step from one gait to the next: the left foot falls where it would have.
						const Clip& A = M.S->Clips[static_cast<size_t>(M.Cur.Clip)];
						const Clip& B = M.S->Clips[static_cast<size_t>(G.Clip)];
						float P = M.Cur.Phase - A.LeftFoot + B.LeftFoot;
						P -= std::floor(P);
						G.Phase = P;
					}
					Start(M, G, At);
				}
			}
			M.EndClass = St.Cl;
		}

		void Start(Man& M, const Segment& G, double At)
		{
			M.Prev = M.Cur;
			M.bPrev = M.Cur.Clip >= 0;
			M.Cur = G;
			M.Cur.Start = At;
			M.Cur.LastT = -1.f;
		}

		/** A segment's clip time now (and its lie), moving a gait's phase on by its rate. */
		float ClipTime(const Man& M, const Segment& G, double Now, float& OutLie) const
		{
			const Clip& C = M.S->Clips[static_cast<size_t>(G.Clip)];
			OutLie = G.LieA;
			switch (G.How)
			{
			case Segment::Gait: return G.Phase * C.Long;
			case Segment::Loop:
			{
				const double T = (Now - G.Start) * G.Rate + G.Phase * C.Long;
				return static_cast<float>(std::fmod(std::max(0.0, T), static_cast<double>(std::max(C.Long, 0.01f))));
			}
			case Segment::Hold: return G.HoldT;
			case Segment::Once: break;
			}
			const float U = static_cast<float>(Clamp01(static_cast<float>((Now - G.Start) / G.Dur)));
			OutLie = G.LieA + (G.LieB - G.LieA) * U;
			return G.From + (G.To - G.From) * U;
		}

		void SampleSegment(const Man& M, const Segment& G, double Now, Pose& Out, float& OutLie, float& OutT) const
		{
			const Clip& C = M.S->Clips[static_cast<size_t>(G.Clip)];
			OutT = ClipTime(M, G, Now, OutLie);
			Sample(C, OutT, Out);
			if (G.bTurned) TurnRoot(*M.S, Out, G.Yaw, G.ShiftX, G.ShiftY);
			if (G.bShift && C.bEndOffset && C.Long > 0.f)
			{
				// go_prone carries his pelvis forward by its end offset as he goes down; taken back along the way, he lies
				// where the rules put him (and gets up there), not 0.6 m on.
				const float K = OutT / C.Long;
				for (const int B : M.S->RootChildren())
				{
					Out.Move[B].X -= C.EndOffset.X * K;
					Out.Move[B].Y -= C.EndOffset.Y * K;
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// Shots.
		// ---------------------------------------------------------------------------------------------------------
		const Stance& StanceAt(const Man& M, double T) const
		{
			// (The stance the rules gave at that moment of his own timeline.)
			const double Tl = T - M.Info.Delay;
			size_t I = 0;
			while (I + 1 < M.Stances.size() && M.Stances[I + 1].T <= Tl) ++I;
			return M.Stances[I];
		}

		void PlanShots(Man& M)
		{
			// (Planned again whenever rounds are booked: a live battle's come a delta at a time. What was already shown
			// of a round, its bullets and its clip's last time, stays shown; Reset starts them over. Each round's plan
			// is made again because the next one cuts a burst short and squeezes a clip.)
			std::stable_sort(M.Shots.begin(), M.Shots.end(), [](const Shot& A, const Shot& B) { return A.T < B.T; });
			const Soldier& S = *M.S;
			for (size_t I = 0; I < M.Shots.size(); ++I)
			{
				Shot& Sh = M.Shots[I];
				const Stance& St = M.Stances.empty() ? Stance() : StanceAt(M, Sh.T);
				Sh.Rate2 = 1.f;
				if (Sh.W == Weapon::Mortar)
				{
					Sh.How = Shot::Mortar;
					Sh.Start = Sh.End = Sh.T;
					continue;
				}
				if (Sh.W == Weapon::Grenade)
				{
					Sh.How = Shot::Clip;
					Sh.ClipIx = S.ClipIndex("throw_grenade");
				}
				else if (St.Play == Stance::Gait)
				{
					// Fired on the move (the rules can fire from an advance): the flash from his muzzle as he goes, his
					// gait unbroken. Marching fire, as the Americans were taught to use.
					Sh.How = Shot::Marching;
					Sh.Start = Sh.End = Sh.T;
					continue;
				}
				else
				{
					const char* ByClass = St.Cl == Prone ? "fire_prone" : St.Cl == Kneel ? "fire_kneel" : "fire_stand";
					if (Sh.W == Weapon::Mg)
					{
						Sh.How = Shot::Burst;
						Sh.ClipIx = S.ClipIndex(S.Weapon == "lmg" && St.Cl == Prone ? "lmg_burst" : ByClass);
					}
					else
					{
						Sh.How = Shot::Clip;
						Sh.ClipIx = S.ClipIndex(ByClass);
					}
				}
				if (Sh.ClipIx < 0) { Sh.How = Shot::Marching; Sh.Start = Sh.End = Sh.T; continue; }
				const Clip& C = S.Clips[static_cast<size_t>(Sh.ClipIx)];
				if (Sh.How == Shot::Burst)
				{
					// One mg round is one burst of 6 to 10 (the rules' rounds are bursts), ten a second.
					Sh.Bullets = 6 + static_cast<int>(Unit01(Seed ^ (static_cast<uint64_t>(M.Info.Block) << 40) ^ (static_cast<uint64_t>(M.Info.K) << 24)
						^ (static_cast<uint64_t>(M.Info.J) << 52) ^ I) * 5.0);
					Sh.Lead = C.FlashT >= 0.f ? C.FlashT : 0.1f;
					Sh.Start = Sh.T - Sh.Lead / Life;
					Sh.End = Sh.T + (Sh.Bullets - 1) * Gap() + 0.15 / Life;
				}
				else
				{
					Sh.Lead = Sh.W == Weapon::Grenade ? (C.ReleaseT >= 0.f ? C.ReleaseT : 0.95f) : (C.FlashT >= 0.f ? C.FlashT : 0.1f);
					Sh.Start = Sh.T - Sh.Lead / Life;
					Sh.End = Sh.Start + C.Long / Life;
				}
			}
			// A burst ends before his next round whatever it is: one fired on the move just after it included (its bullets
			// would run on past that round's own; a crowd of sections fires often enough for it to happen).
			for (size_t I = 0; I + 1 < M.Shots.size(); ++I)
			{
				Shot& A = M.Shots[I];
				const Shot& B = M.Shots[I + 1];
				if (A.How != Shot::Burst || B.How == Shot::Clip || B.How == Shot::Burst) continue;
				if (A.T + (A.Bullets - 1) * Gap() < B.T) continue;
				A.Bullets = std::max(1, std::min(A.Bullets, static_cast<int>(std::floor((B.T - A.T) / Gap()))));
				A.End = std::min(A.End, B.T);
			}
			// Where the next round comes before this one's clip is done, the part after the flash is played faster to
			// end on aim as the next begins (a burst is cut short); the flash itself always falls on its round's time.
			for (size_t I = 0; I + 1 < M.Shots.size(); ++I)
			{
				Shot& A = M.Shots[I];
				if (A.How != Shot::Clip && A.How != Shot::Burst) continue;
				size_t N = I + 1;
				while (N < M.Shots.size() && M.Shots[N].How != Shot::Clip && M.Shots[N].How != Shot::Burst) ++N;
				if (N >= M.Shots.size()) break;
				const Shot& B = M.Shots[N];
				if (A.End <= B.Start) continue;
				if (A.How == Shot::Burst)
				{
					A.Bullets = std::max(1, std::min(A.Bullets, static_cast<int>(std::floor((B.T - A.T) / Gap()))));
					A.End = std::min(A.End, B.Start);
				}
				else
				{
					const Clip& C = S.Clips[static_cast<size_t>(A.ClipIx)];
					const double Avail = (B.Start - A.Start) * Life - A.Lead; // (clip seconds)
					const double After = C.Long - A.Lead;
					if (Avail > 0.05 && After > 0.0) A.Rate2 = static_cast<float>(std::min(3.0, After / Avail));
					A.End = std::max(A.Start + A.Lead / Life, B.Start);
				}
			}
			M.ShotCur = 0;
			M.bShotsDirty = false;
		}

		/** A burst's bullets apart on the battle's clock: ten a second of life. */
		double Gap() const { return 0.1 / Life; }

		/** The shot's clip time at Now (a burst: the aim with a kick each bullet). */
		float ShotClipTime(const Shot& Sh, const Clip& C, double Now, float& OutKick) const
		{
			OutKick = 0.f;
			const double Tau = (Now - Sh.Start) * Life; // (clip seconds)
			if (Tau <= 0.0) return 0.f;
			if (Sh.How == Shot::Burst)
			{
				// Held on aim; each bullet kicks him back to the shot's own frame (the recoil) and he settles before the
				// next. A gunner's lmg_burst plays itself (its shoulder shake).
				if (C.Name == "lmg_burst") return static_cast<float>(std::min(Tau, static_cast<double>(C.Long)));
				const double Since = (Now - Sh.T) * Life;
				if (Since < 0.0) return 0.f;
				const int B = static_cast<int>(std::floor(Since / 0.1));
				const double In = Since - B * 0.1;
				// After the last bullet the kick is over and he stays on aim.
				if (B < Sh.Bullets) OutKick = static_cast<float>(1.0 - In / 0.1);
				return 0.f;
			}
			if (Tau <= Sh.Lead) return static_cast<float>(Tau);
			return std::min(C.Long, static_cast<float>(Sh.Lead + (Tau - Sh.Lead) * Sh.Rate2));
		}

		// ---------------------------------------------------------------------------------------------------------
		// The falls.
		// ---------------------------------------------------------------------------------------------------------
		std::string StanceForFall(const Man& M, double Now) const
		{
			// How he stood when he was hit, as drawn (a man half way down to the ground counts as kneeling).
			const Segment& G = M.Cur;
			std::string St = "stand";
			if (G.Clip >= 0)
			{
				const std::string& Name = M.S->Clips[static_cast<size_t>(G.Clip)].Name;
				if (G.How == Segment::Once)
				{
					const float U = static_cast<float>(Clamp01(static_cast<float>((Now - G.Start) / G.Dur)));
					if (Name == "go_prone") St = G.To > G.From ? (U < 0.5f ? "kneel" : "prone") : (U < 0.5f ? "prone" : G.EndClass == Up && U > 0.8f ? "stand" : "kneel");
					else if (Name == "kneel_down") St = U < 0.5f ? "stand" : "kneel";
					else if (Name == "stand_up") St = U < 0.5f ? "kneel" : "stand";
					else St = G.EndClass == Prone ? "prone" : G.EndClass == Kneel ? "kneel" : "stand";
				}
				else if (G.How == Segment::Gait) St = G.EndClass == Prone ? "prone" : "run";
				else St = G.EndClass == Prone ? "prone" : G.EndClass == Kneel ? "kneel" : "stand";
			}
			// (bullet hits, bVariety: a man walking, or just halted from a gait, on his feet takes the standing falls, wherever
			// he stands; a man running still the running ones)
			if (HitsNow.bVariety && G.Clip >= 0 && G.EndClass == Up)
			{
				const std::string& Name = M.S->Clips[static_cast<size_t>(G.Clip)].Name;
				const bool bWalk = G.How == Segment::Gait && (Name == "walk" || Name == "creep" || Name == "reel_walk" || M.V < 2.2f * static_cast<float>(Life));
				const bool bHalted = G.How != Segment::Gait && G.How != Segment::Once && M.Prev.Clip >= 0 && M.Prev.How == Segment::Gait
					&& M.Prev.EndClass == Up && (Now - G.Start) * Life < 1.5;
				if (bWalk || bHalted) return "stand";
			}
			if (St == "stand" || St == "kneel")
			{
				const TrackData& Tr = Tracks[static_cast<size_t>(M.Track)];
				std::string Cover = "none";
				for (const TrackData::Change& C : Tr.Ch)
				{
					if (C.T <= Now - M.Info.Delay + 1e-6) Cover = C.Cover;
				}
				if (Cover == "works" || Cover == "building" || Cover == "woods" || Cover == "rock" || Cover == "wreck" || Cover == "crater" || Cover == "crest")
					St = "cover";
			}
			return St;
		}

		/** Effects (R2): a man as the round that killed him struck him, and what a gore fall throws him away from. */
		struct Blast
		{
			double X = 0.0, Y = 0.0, F = 0.0; // where he stood and his heading as it struck him, by his track
			double PX = 0.0, PY = 0.0;        // the point he is thrown away from (bAway): the burst, or the gun's place
			bool bAway = false;               // none: the burst under him and the gun not known
			std::string Side;                 // front|behind|left|right|under (the gore falls' own words); empty: no round
			std::string Gun;                  // the burst under him: the gun's side (front|behind|left|right), where known
		};

		/**
		 * Effects (R2): where the round that killed him burst against his facing, in the gore falls' own words
		 * (gore_from), and the point a gore fall throws him and his limb away from. A burst 1 m or more from him: "front"
		 * or "behind" within 45 degrees of his nose or his back, else "left" or "right" (his right clockwise of his
		 * heading, as the field's headings run, WorldOf); away from the burst. One within 1 m (the rules lay 8 in 10 shell
		 * kills on the man, which says nothing of where the shell truly fell), whatever his stance: "under" (the deaths
		 * thread, 24 Nov: its falls from under, a lower leg off), and the gun's side as it fired (its track at the round,
		 * as FromWhere reads it) for a throw or where his stance has no fall from under he may take, away from the gun;
		 * no gun's side where it is not known or stood within 1 m of him. Where he stood and his heading by his track at
		 * the round (Dead less his lag, as GoreTwinFor reads them), so the same whatever frame his fall is first played
		 * on. Read before he is gored (GoreTwinFor) and as his fall is laid (StartFall).
		 */
		Blast BlastOf(const Man& M) const
		{
			Blast B;
			B.X = M.X;
			B.Y = M.Y;
			B.F = static_cast<double>(M.F);
			{
				float F = 0.f, V = 0.f;
				size_t Cursor = 0;
				const TrackData& Tr = Tracks[static_cast<size_t>(M.Track)];
				if (!Tr.T.empty())
				{
					Tr.At(M.Info.Dead - M.Info.Delay, Cursor, B.X, B.Y, F, V);
					B.F = static_cast<double>(F);
				}
			}
			if (M.KillT < 0.0) return B;
			B.PX = M.KillX;
			B.PY = M.KillY;
			auto SideOf = [&B](double PX, double PY) {
				const double Off = Turn(B.F, std::atan2(PX - B.X, PY - B.Y)), Eighth = 0.785398163397448;
				return std::string(std::fabs(Off) < Eighth ? "front" : std::fabs(Off) > 3.0 * Eighth ? "behind" : Off > 0.0 ? "right" : "left");
			};
			if ((B.PX - B.X) * (B.PX - B.X) + (B.PY - B.Y) * (B.PY - B.Y) < 1.0)
			{
				B.Side = "under";
				const auto Found = TrackOf.find({ M.KillBy, M.KillK, M.KillJ });
				if (Found == TrackOf.end()) return B;
				double SX = 0.0, SY = 0.0;
				size_t Cursor = 0;
				float F = 0.f, V = 0.f;
				Tracks[static_cast<size_t>(Found->second)].At(M.KillT, Cursor, SX, SY, F, V);
				if ((SX - B.X) * (SX - B.X) + (SY - B.Y) * (SY - B.Y) < 1.0) return B;
				B.PX = SX;
				B.PY = SY;
				B.bAway = true;
				B.Gun = SideOf(SX, SY);
				return B;
			}
			B.bAway = true;
			B.Side = SideOf(B.PX, B.PY);
			return B;
		}

		/** Effects (R2): his GoreStrike (LandMen::GoreFalls, GoreSide) from how the round struck him (BlastOf): the point
		 * he is thrown away from in his own axes as struck (x forward, y right), and whether it may throw him. */
		GoreStrike StrikeOf(const Man& M, const std::string& St, const Blast& Bl) const
		{
			GoreStrike Strike;
			Strike.Stance = St;
			Strike.Side = Bl.Side;
			Strike.Gun = Bl.Gun;
			Strike.bAway = Bl.bAway;
			Strike.bBig = BigThrow(M);
			if (Bl.bAway)
			{
				const double DX = Bl.PX - Bl.X, DY = Bl.PY - Bl.Y, Sf = std::sin(Bl.F), Cf = std::cos(Bl.F);
				Strike.X = DX * Sf + DY * Cf;
				Strike.Y = DX * Cf - DY * Sf;
			}
			return Strike;
		}

		/** Effects (R2): whether the round that killed him may throw him (a gore fall's big throw): a field gun's, a
		 * howitzer's or a heavy gun's shell (the art director, 23 Nov 17:52Z: die_blast for field guns and howitzers), the
		 * round's w "gun" from a block whose cal is not "light" (no cal written: a howitzer's, as the Fates table reads
		 * it, LandKit's WeaponWord). A light gun's, a tank's, a mortar's or a grenade's never. */
		bool BigThrow(const Man& M) const
		{
			if (M.KillWeapon != "gun") return false;
			const auto Found = BlockCal.find(M.KillBy);
			return Found == BlockCal.end() || Found->second != "light";
		}

		std::string FromWhere(const Man& M) const
		{
			// Where the round that killed him came from, against his facing: front, back or side. A shell from the gun's
			// side; with no shooter known, the front (a man faces what he fights).
			if (M.KillT < 0.0) return "front";
			// (Effects (R2): a gored man is thrown away from the burst, or from the gun where it burst under him and his
			// falls are read by its side: that side, as StartFall has just read it, GoreSide. A burst under him read below
			// from the gun as for any man, front within 70 degrees of his nose and back past 110, not by the 45-degree
			// quarter his throw is read by: it weighs none of his falls from under nor the throw, ChooseFall)
			const std::string& Burst = M.Info.GoreFrom;
			if (!Burst.empty() && Burst != "under") return Burst == "front" ? "front" : Burst == "behind" ? "back" : "side";
			double SX = 0.0, SY = 0.0;
			bool bKnown = false;
			auto Found = TrackOf.find({ M.KillBy, M.KillK, M.KillJ });
			if (Found != TrackOf.end())
			{
				const TrackData& Tr = Tracks[static_cast<size_t>(Found->second)];
				size_t Cursor = 0;
				float F = 0.f, V = 0.f;
				Tr.At(M.KillT, Cursor, SX, SY, F, V);
				bKnown = true;
			}
			if (!bKnown) return "front";
			const double DX = SX - M.X, DY = SY - M.Y;
			if (DX * DX + DY * DY < 1.0) return "front";
			const double Bearing = std::atan2(DX, DY);
			const double Off = std::fabs(Turn(M.F, Bearing));
			return Off < 1.22 ? "front" : Off > 1.92 ? "back" : "side";
		}

		int ChooseFall(Man& M, const std::string& St, const std::string& From, double At = -1.0, bool bWound = false)
		{
			const Soldier& S = *M.S;
			// (bullet hits, bVariety: the neighbours' rule hard and with or without sections, and his own rate and start
			// where no other clip is left; bWound: a wounded man's fall, which does not kill him)
			const bool bVar = HitsNow.bVariety;
			M.Info.FallRate = 1.f;
			M.Info.FallSkip = 0.f;
			Dice D(Seed ^ Mix(static_cast<uint64_t>(M.Info.Block) * 1000003ull + static_cast<uint64_t>(M.Info.K))
				^ (M.Info.J > 0 ? Mix(0x5EC7ull + static_cast<uint64_t>(M.Info.J)) : 0ull));
			M.Info.FallStance = St;
			M.Info.FallFrom = From;
			const bool bShell = M.KillW == Weapon::Mortar || M.KillWeapon == "gun" || M.KillWeapon == "tank" || M.KillW == Weapon::Grenade;
			// Run over (infantry with tanks: an enemy tank's tracks): knocked flat under her, never thrown or spun.
			const bool bCrushed = M.KillWeapon == "track";
			// (Effects (R2): fallen as his file's gore twin, from its gore falls, weighed alike)
			const std::map<std::string, std::map<std::string, std::vector<std::string>>>& Table = M.bGore ? S.DeathsGore : S.Deaths;
			if (!Table.empty())
			{
				// The physics-baked falls: weigh each kind by how he was struck and how he stood, then each clip of the
				// kind by the Fates rule (never the last fall again unless it is the only one; the 2nd to 4th last
				// 0.35, 0.6, 0.85), so a battle's dead never fall twice alike running.
				const std::map<std::string, std::vector<std::string>>* Kinds = KindsOf(Table, St);
				// (bVariety: a man lying in a file with no prone falls takes the kneeling ones, never limp)
				if (!Kinds && bVar && !Table.empty()) Kinds = KindsOf(Table, "kneel");
				// (bWound: the soldiers thread's wound falls where his file has them, each weighed alike; else the table's)
				std::map<std::string, std::vector<std::string>> WoundKinds;
				if (bWound)
				{
					for (const char* Part : { "arm", "gut", "leg" })
					{
						if (S.ClipIndex(std::string("wound_") + Part) >= 0) WoundKinds[Part].push_back(std::string("wound_") + Part);
					}
					if (!WoundKinds.empty()) Kinds = &WoundKinds;
				}
				if (Kinds)
				{
					std::vector<std::pair<std::string, double>> Odds;
					std::vector<std::string> KindOfPick;
					// Effects (R2), a gored man: only the falls GoreFalls gives him (those from where the burst was against him,
					// BlastOf, of them those whose limbs move away from the burst or the gun; where none from there he may take,
					// of all he may take: no throw but a field gun's or a howitzer's, BigThrow, and none from another side of
					// him; where none from there moves its limbs half a metre away, of all he may take but those from the side
					// opposite; a burst under him, his falls from under (his gun known, those whose limb flies not toward it,
					// or where all do the least so), and such a shell's throw from his gun's side), each kind's weight shared
					// among its falls taken, the kinds weighed as for any man and the kind from under as below. Never no fall
					// (GoreTwinFor gores no man whose stance's gore falls hold none he may take; were it so, all of them).
					const bool bBig = BigThrow(M);
					std::vector<std::pair<std::string, std::string>> Given;
					if (M.bGore) Given = GoreFalls(S, StrikeOf(M, St, BlastOf(M)));
					auto Takes = [&](const std::string& Kind, const std::string& Name) {
						return !M.bGore || std::find(Given.begin(), Given.end(), std::make_pair(Kind, Name)) != Given.end();
					};
					size_t Candidates = 0;
					for (const auto& KV : *Kinds)
					{
						for (const std::string& Name : KV.second) Candidates += Takes(KV.first, Name) ? 1 : 0;
					}
					// A section's men fall close together: no fall a body within 15 m already shows, where another will do
					// (the Fates rule above runs in time; this one on the ground, among neighbours). Only with sections,
					// so a recording without them falls exactly as it did.
					std::vector<std::string> Beside, Recent;
					if (bSections || bVar)
					{
						for (const Man& O : Men)
						{
							// (bVariety: a wounded man's fall counts as a dead man's; one in the last 8 s is never taken
							// again within 15 m where another is left, Recent)
							const std::string* Shown = nullptr;
							double When = -1.0;
							if (&O == &M) continue;
							if (O.State != Man::Alive && !O.Info.FallClip.empty()) { Shown = &O.Info.FallClip; When = O.Info.FallStarted; }
							else if (bVar && O.Info.WoundStarted >= 0.0 && !O.Info.WoundClip.empty()) { Shown = &O.Info.WoundClip; When = O.Info.WoundStarted; }
							else continue;
							if ((O.X - M.X) * (O.X - M.X) + (O.Y - M.Y) * (O.Y - M.Y) < 225.0)
							{
								Beside.push_back(*Shown);
								if (bVar && At - When <= 8.0) Recent.push_back(*Shown);
							}
						}
					}
					for (const auto& KV : *Kinds)
					{
						const std::string& Kind = KV.first;
						size_t Taken = 0;
						for (const std::string& Name : KV.second) Taken += Takes(Kind, Name) ? 1 : 0;
						if (Taken == 0) continue;
						double W = Kind == "spin" ? 0.6 : Kind == "thrown" ? 0.2 : Kind == "crawl" ? 0.3 : 1.0;
						// (wounded: never thrown; dragging himself on (crawl) as likely as any, more with the fire behind him,
						// so he drags himself from it, less with it ahead)
						if (bWound && WoundKinds.empty()) W = Kind == "thrown" ? 0.0 : Kind == "crawl" ? (From == "back" ? 3.0 : From == "side" ? 1.0 : From == "front" ? 0.4 : 2.0) : W;
						if (From == "front") W *= Kind == "back" ? 3.0 : Kind == "knees" ? 1.5 : Kind == "forward" ? 0.4 : 1.0;
						if (From == "back") W *= Kind == "forward" ? 3.0 : Kind == "back" ? 0.3 : 1.0;
						if (From == "side") W *= Kind == "side" ? 3.0 : Kind == "spin" ? 2.0 : 1.0;
						if (St == "run") W *= Kind == "forward" ? 2.0 : Kind == "spin" ? 1.3 : 1.0;
						if (St == "kneel" || St == "cover") W *= Kind == "knees" ? 2.5 : Kind == "side" ? 1.5 : 1.0;
						if (St == "prone") W *= Kind == "crawl" ? 3.0 : 1.0;
						// (a gored man: the big throw for a field gun's, a howitzer's or a heavy gun's shell; a light gun's, a
						// tank's, a mortar's or a grenade's knocks him down, an arm or a leg off, by where it burst: its throws
						// are not taken above)
						if (bShell) W *= Kind == "thrown" ? (M.bGore && !bBig ? 1.0 : 40.0) : Kind == "spin" || Kind == "side" ? 1.3 : 1.0;
						// (a gored man the burst was under, within 1 m: the kind from under, a lower leg off (the deaths thread,
						// 24 Nov), weighed three times, as the kind matching the burst is above (back for one in front, forward
						// behind, side at his side; From, read from his gun as for any man, weighs none of his falls from under
						// nor the throw). So where a field gun's or a howitzer's shell bursting on him is offered a throw from
						// his gun's side, the throw weighs 8 (0.2, 40 times) to the fall from under's 3 whatever his stance and
						// his gun's side, before the Fates and the section rules. Read by his gun's side (b470998e) it was the
						// same for a man standing with his gun ahead or behind, or running with it ahead, but the matching kind
						// weighed more elsewhere: standing or running with it at his side 3.9 (3, 1.3 for a shell's side
						// fall), running with it behind 6 (forward twice running), kneeling with it on his left 5.85 (side 1.5
						// kneeling, 1.3); so such men are thrown somewhat more often now. Every other round kills him in a
						// fall from under)
						if (M.bGore && M.Info.GoreFrom == "under") W *= Kind == "under" ? 3.0 : 1.0;
						if (bCrushed) W *= Kind == "back" || Kind == "forward" ? 6.0 : Kind == "thrown" || Kind == "spin" ? 0.05 : 1.0;
						if (!M.FallKindWanted.empty() && Kind == M.FallKindWanted) W *= 50.0;
						// (bVariety: where a rifle or an mg round struck him by his own dice, head, gut or leg, four times;
						// a wound fall of the soldiers thread's by its part, arm where his dice gave the head or the chest)
						else if (bVar && !M.Info.HitPart.empty() && Kind == M.Info.HitPart) W *= 4.0;
						else if (bVar && bWound && !WoundKinds.empty() && M.Info.HitPart.empty() && Kind == "arm") W *= 4.0;
						for (const std::string& Clip : KV.second)
						{
							if (!Takes(Kind, Clip)) continue;
							const bool bBeside = Candidates > 1 && std::find(Beside.begin(), Beside.end(), Clip) != Beside.end();
							Odds.push_back({ Clip, W / static_cast<double>(Taken) * Falls.Weight(Clip, Candidates) * (bBeside ? 0.05 : 1.0) });
							KindOfPick.push_back(Kind);
						}
					}
					double Sum = 0.0;
					for (const auto& O : Odds) Sum += O.second;
					// (bVariety: none a body within 15 m showed in the last 8 s, where any other has a weight)
					if (bVar && !Recent.empty())
					{
						double Hard = 0.0;
						for (const auto& O : Odds) Hard += std::find(Recent.begin(), Recent.end(), O.first) == Recent.end() ? O.second : 0.0;
						if (Hard > 0.0)
						{
							for (auto& O : Odds) if (std::find(Recent.begin(), Recent.end(), O.first) != Recent.end()) O.second = 0.0;
							Sum = Hard;
						}
					}
					if (Sum <= 0.0) for (auto& O : Odds) { O.second = 1.0; Sum += 1.0; }
					double R = D.U() * Sum;
					size_t Pick = Odds.size() - 1;
					for (size_t I = 0; I < Odds.size(); ++I)
					{
						R -= Odds[I].second;
						if (R <= 0.0) { Pick = I; break; }
					}
					if (!Odds.empty())
					{
						M.Info.FallKind = KindOfPick[Pick];
						Falls.Shown(Odds[Pick].first);
						// (bVariety: no other clip was left; the same one at his own rate, 0.85 to 0.95 or 1.05 to 1.15,
						// and start, 0 to 0.1 s in, by his own dice)
						if (bVar && std::find(Recent.begin(), Recent.end(), Odds[Pick].first) != Recent.end())
						{
							Dice V(OwnSeed(M, 0x7A41E7ull));
							const double Up = V.U();
							const double By = V.U();
							const double In = V.U();
							M.Info.FallRate = static_cast<float>(Up < 0.5 ? 0.95 - 0.1 * By : 1.05 + 0.1 * By);
							M.Info.FallSkip = static_cast<float>(0.1 * In);
						}
						return S.ClipIndex(Odds[Pick].first);
					}
				}
			}
			// The README's table until the deaths map comes: a man lying keeps his aim and goes limp; moving, he falls
			// forward; kneeling or in cover, die_kneel; standing, die_back when hit from the front (or the side), and
			// die_forward from behind.
			if (St == "prone") { M.Info.FallKind = "limp"; return -1; }
			const char* Name = St == "run" ? "die_forward" : St == "kneel" || St == "cover" ? "die_kneel" : From == "back" ? "die_forward" : "die_back";
			M.Info.FallKind = St == "run" || From == "back" ? "forward" : St == "kneel" || St == "cover" ? "knees" : "back";
			int Ix = S.ClipIndex(Name);
			if (Ix < 0)
			{
				for (size_t I = 0; I < S.Clips.size(); ++I)
				{
					if (S.Clips[I].Name.rfind("die_", 0) == 0) { Ix = static_cast<int>(I); break; }
				}
			}
			if (Ix >= 0) Falls.Shown(S.Clips[static_cast<size_t>(Ix)].Name);
			return Ix;
		}

		struct Dice
		{
			uint64_t State;
			explicit Dice(uint64_t S) : State(S ? S : 1) {}
			double U() { State = Mix(State); return static_cast<double>(State >> 11) * (1.0 / 9007199254740992.0); }
		};

		/** Bullet hits: a man's own dice for a purpose (Salt), apart from ChooseFall's and the gore's, so theirs keep
		 * their order. */
		uint64_t OwnSeed(const Man& M, uint64_t Salt) const
		{
			return Seed ^ Mix(Salt + static_cast<uint64_t>(M.Info.Block) * 1000003ull + static_cast<uint64_t>(M.Info.K))
				^ (M.Info.J > 0 ? Mix(Salt * 3ull + static_cast<uint64_t>(M.Info.J)) : 0ull);
		}

		/** The pelvis's place in his own axes for a pose (the fall's rest pivot). */
		Vec3 PelvisOf(const Man& M, const Pose& P) const
		{
			Xform Fr[MaxBones];
			BoneFrames(*M.S, P, Fr);
			const int Pelvis = M.S->BoneIndex("pelvis");
			if (Pelvis < 0) return {};
			const Vec3 At = Fr[Pelvis].Point(M.S->Bones[static_cast<size_t>(Pelvis)].Head);
			return { At.X, At.Y, 0.f };
		}

		void StartFall(Man& M, double D)
		{
			const std::string St = StanceForFall(M, D);
			// (Effects (R2): the side a gored man's falls are read by, his burst's or under, or his gun's where it burst
			// under him and he has no fall from under he may take (GoreSide), for FromWhere and ChooseFall; empty for a man
			// not gored)
			M.Info.GoreFrom = M.bGore ? GoreSide(*M.S, StrikeOf(M, St, BlastOf(M))) : std::string();
			const std::string From = FromWhere(M);
			// (bullet hits, bVariety: where a rifle or an mg round struck him, by his own dice, for his fall's kind)
			M.Info.HitPart = HitsNow.bVariety && (M.KillW == Weapon::Rifle || M.KillW == Weapon::Mg) ? HitPartOf(M) : std::string();
			M.FallClip = ChooseFall(M, St, From, D);
			M.FallAt = D;
			M.FallFrom = M.Now;
			M.FallLie0 = M.Lie;
			M.FallLastT = -1.f;
			M.Info.FallStarted = D;
			if (M.FallClip >= 0)
			{
				const Clip& C = M.S->Clips[static_cast<size_t>(M.FallClip)];
				M.Info.FallClip = C.Name;
				M.FallDur = C.Long / Life;
				if (M.Info.FallRate != 1.f || M.Info.FallSkip != 0.f) M.FallDur = (C.Long - M.Info.FallSkip) / (Life * M.Info.FallRate);
				Pose End;
				Sample(C, C.Long, End);
				// "Move the root by end_offset": he comes to rest where his pelvis ends, and the turn onto the ground's
				// slope is made there.
				M.FallPivot = PelvisOf(M, End);
				if (C.bEndOffset)
				{
					M.FallPivot.X = C.EndOffset.X;
					M.FallPivot.Y = C.EndOffset.Y;
				}
				M.State = Man::Falling;
			}
			else
			{
				M.Info.FallClip = "settle";
				M.FallDur = 0.6 / Life;
				M.FallPivot = Vec3();
				M.State = Man::Settling;
			}
			if (HitsNow.bJolt) JoltFrom(M, D, false);
		}

		// ---------------------------------------------------------------------------------------------------------
		// Bullet hits (SetHits): the jolt, where he was struck, and the wounded who stay down.
		// ---------------------------------------------------------------------------------------------------------
		/** Where a rifle or an mg round struck him, by his own dice: the head about 1 in 10, the gut 3 in 20, a leg 3 in
		 * 20, else the chest (empty). */
		std::string HitPartOf(const Man& M) const
		{
			Dice H(OwnSeed(M, 0x417ull));
			const double U = H.U();
			return U < 0.10 ? "head" : U < 0.25 ? "gut" : U < 0.40 ? "leg" : "";
		}

		/** His jolt, from At: away from the shooter (his kill's track as it fired, as FromWhere reads it; none known, or a
		 * wounded man's, from the front), sized by the weapon (rifle 1, mg 0.8 and a second kick 70-110 ms on) times 0.7
		 * to 1.3 by his own dice; a leg fall (die_leg_*, or its "hit" a thigh or a calf) buckles a knee instead of the
		 * twist. Only a rifle's or an mg's kill, or a wound. */
		void JoltFrom(Man& M, double At, bool bWound)
		{
			M.JoltAt = -1.0;
			const bool bMg = !bWound && M.KillW == Weapon::Mg;
			if (!bWound && M.KillW != Weapon::Rifle && !bMg) return;
			Dice JD(OwnSeed(M, 0x10175ull));
			const double Big = JD.U();
			const double Pace = JD.U();
			const double Second = JD.U();
			const double Shoulder = JD.U();
			const double Knee = JD.U();
			const float Size = (bMg ? 0.8f : 1.f) * static_cast<float>(0.7 + 0.6 * Big);
			Vec3 Away { -1.f, 0.f, 0.f };
			if (!bWound && M.KillT >= 0.0)
			{
				const auto Found = TrackOf.find({ M.KillBy, M.KillK, M.KillJ });
				if (Found != TrackOf.end())
				{
					double SX = 0.0, SY = 0.0;
					size_t Cursor = 0;
					float F = 0.f, V = 0.f;
					Tracks[static_cast<size_t>(Found->second)].At(M.KillT, Cursor, SX, SY, F, V);
					const double DX = SX - M.X, DY = SY - M.Y, Sf = std::sin(static_cast<double>(M.F)), Cf = std::cos(static_cast<double>(M.F));
					const double Fwd = DX * Sf + DY * Cf, Right = DX * Cf - DY * Sf, L = std::hypot(Fwd, Right);
					if (L >= 1.0) Away = { static_cast<float>(-Fwd / L), static_cast<float>(-Right / L), 0.f };
				}
			}
			M.JoltAway = Away;
			// (the struck shoulder: the shooter's side, his right where the shot comes from his right; from ahead or
			// behind, either by his dice)
			const float Side = Away.Y < -0.25f ? 1.f : Away.Y > 0.25f ? -1.f : Shoulder < 0.5 ? 1.f : -1.f;
			const float Deg = 0.0174532925f;
			M.JoltBend = 16.f * Deg * Size;
			M.JoltTwist = Side * 14.f * Deg * Size;
			M.JoltOmega = static_cast<float>(18.0 + 4.0 * Pace);
			M.JoltSecond = bMg ? static_cast<float>(0.07 + 0.04 * Second) : 0.f;
			M.JoltCalf = -1;
			M.JoltKnee = 0.f;
			const int Ci = bWound ? M.Cur.Clip : M.FallClip;
			if (Ci >= 0)
			{
				const Clip& C = M.S->Clips[static_cast<size_t>(Ci)];
				const bool bLeg = C.Hit.Bone.empty() ? C.Name.rfind("die_leg_", 0) == 0 || C.Name == "wound_leg"
					: C.Hit.Bone.rfind("thigh", 0) == 0 || C.Hit.Bone.rfind("calf", 0) == 0;
				if (bLeg)
				{
					const bool bRight = C.Hit.Side == "right" || (C.Hit.Side != "left" && Side > 0.f);
					M.JoltCalf = M.S->BoneIndex(bRight ? "calf_r" : "calf_l");
					M.JoltKnee = static_cast<float>(15.0 + 10.0 * Knee) * Deg * Size;
					M.JoltTwist = 0.f;
				}
			}
			M.JoltAt = At;
		}

		/** The jolt's pulse at U seconds in (U = T, or T less a lag), as it stands T seconds in: w U e^(1 - w U), peaking
		 * at 1 a twentieth of a second in, an mg's second kick half as big; the whole faded out by T from 0.2 s to none
		 * at all from 0.33 s. */
		static double JoltPulse(const Man& M, double U, double T)
		{
			if (T <= 0.0 || T >= 0.33) return 0.0;
			auto G = [&M](double V) { if (V <= 0.0) return 0.0; const double X = static_cast<double>(M.JoltOmega) * V; return X * std::exp(1.0 - X); };
			double P = G(U);
			if (M.JoltSecond > 0.f) P += 0.5 * G(U - static_cast<double>(M.JoltSecond));
			return P * (1.0 - static_cast<double>(Smooth(static_cast<float>((T - 0.2) / 0.13))));
		}

		/** Bone B turned by Angle about K (his own axes, unit) at its head: a turn made in its parent's frame as framed in
		 * Fr (its subtree goes with it). */
		static void TurnBone(const Soldier& S, const Xform* Fr, Pose& P, int B, const Vec3& K, float Angle)
		{
			if (B < 0 || B >= MaxBones || Angle == 0.f) return;
			const int Pa = S.Bones[static_cast<size_t>(B)].Parent;
			Vec3 L = K;
			if (Pa >= 0)
			{
				const float* R = Fr[Pa].R;
				L = { R[0] * K.X + R[3] * K.Y + R[6] * K.Z, R[1] * K.X + R[4] * K.Y + R[7] * K.Z, R[2] * K.X + R[5] * K.Y + R[8] * K.Z };
			}
			L = Unit(L);
			const float H = 0.5f * Angle, Sn = std::sin(H);
			const Quat D { std::cos(H), L.X * Sn, L.Y * Sn, L.Z * Sn };
			P.Rot[B] = QNormal(QMul(D, P.Rot[B]));
		}

		/** Whether bone B hangs under bone Top (or is it). */
		static bool Under(const Soldier& S, int B, int Top)
		{
			for (int Q = B; Q >= 0; Q = S.Bones[static_cast<size_t>(Q)].Parent)
			{
				if (Q == Top) return true;
			}
			return false;
		}

		/**
		 * The jolt (bJolt), a layer over his pose as it is now (his stance's, the fall's blend from it, the fall): spine_01,
		 * spine_02 and chest bent away from the shot (3, 3.5 and 3.5 tenths of it) and twisted toward the struck shoulder;
		 * neck and head turned back by half, the head 40 ms behind (so it holds still at first, then follows at half);
		 * the pelvis and the clavicles never (the pouches and feet hang on the one, collar and straps on the other). The
		 * rifle on bone 'weapon' (the root's, not a hand's) moved with the chest, so his fists keep it. A leg fall: the
		 * knee buckled and half the bend, no twist. Nothing from 0.33 s on: the pose is his own to the bit.
		 */
		void Jolt(const Man& M, Pose& P, double Now) const
		{
			// (life's seconds: the clips' clock, Life of them a battle second)
			const double T = (Now - M.JoltAt) * Life;
			if (T <= 0.0 || T >= 0.33) return;
			const double Pn = JoltPulse(M, T, T);
			const double Ph = JoltPulse(M, T - 0.04, T);
			const Soldier& S = *M.S;
			const int Sp1 = S.BoneIndex("spine_01"), Sp2 = S.BoneIndex("spine_02"), Ch = S.BoneIndex("chest");
			const int Nk = S.BoneIndex("neck"), Hd = S.BoneIndex("head");
			const int Top = Sp1 >= 0 ? Sp1 : Sp2 >= 0 ? Sp2 : Ch;
			const int Upper = Ch >= 0 ? Ch : Sp2;
			if (Top < 0 || Upper < 0) return;
			Xform Fr[MaxBones];
			BoneFrames(S, P, Fr);
			const Vec3 Base = Fr[Top].Point(S.Bones[static_cast<size_t>(Top)].Head);
			const int Tip = Nk >= 0 ? Nk : Upper;
			const Vec3 TipAt = Fr[Tip].Point(S.Bones[static_cast<size_t>(Tip)].Head);
			const Vec3 Spine = Unit(Sub(TipAt, Base));
			// (bent toward 'away' as it lies across his spine; a man lying with the shot along him heaves up off the ground)
			Vec3 Across = Sub(M.JoltAway, Scale(Spine, Dot(M.JoltAway, Spine)));
			if (Length(Across) < 0.3f) Across = Sub(Vec3 { 0.f, 0.f, 1.f }, Scale(Spine, Spine.Z));
			const Vec3 BendAxis = Unit(Cross(Spine, Unit(Across)));
			const bool bLeg = M.JoltCalf >= 0;
			const float Bend = M.JoltBend * (bLeg ? 0.5f : 1.f);
			const float Twist = M.JoltTwist;
			const float Share[3] = { 0.3f, 0.35f, 0.35f };
			const int Spines[3] = { Sp1, Sp2, Ch };
			for (int K = 0; K < 3; ++K)
			{
				TurnBone(S, Fr, P, Spines[K], BendAxis, static_cast<float>(Pn) * Bend * Share[K]);
				TurnBone(S, Fr, P, Spines[K], Spine, static_cast<float>(Pn) * Twist * Share[K]);
			}
			// (the head: held still at first, then at half the chest's turn, 40 ms behind)
			const float Back = static_cast<float>(Pn - 0.5 * Ph);
			TurnBone(S, Fr, P, Nk, BendAxis, -0.5f * Back * Bend);
			TurnBone(S, Fr, P, Nk, Spine, -0.5f * Back * Twist);
			TurnBone(S, Fr, P, Hd, BendAxis, -0.5f * Back * Bend);
			TurnBone(S, Fr, P, Hd, Spine, -0.5f * Back * Twist);
			// (a leg: the knee gives, his shin back about his right)
			if (bLeg) TurnBone(S, Fr, P, M.JoltCalf, Vec3 { 0.f, 1.f, 0.f }, static_cast<float>(Pn) * M.JoltKnee);
			// The rifle: moved as the chest moved, about the root's frame it hangs from, so his fists keep their hold.
			const int Gun = S.WeaponBone();
			if (Gun < 0 || Gun >= MaxBones || Under(S, Gun, Top)) return;
			const int Pa = S.Bones[static_cast<size_t>(Gun)].Parent;
			Xform Now2[MaxBones];
			BoneFrames(S, P, Now2);
			// D = chest now * chest before^-1, a rigid move in his own axes
			Xform Inv;
			const float* R0 = Fr[Upper].R;
			const float Rt[9] = { R0[0], R0[3], R0[6], R0[1], R0[4], R0[7], R0[2], R0[5], R0[8] };
			std::memcpy(Inv.R, Rt, sizeof(Rt));
			const Vec3 Tn = M3Vec(Rt, Vec3 { Fr[Upper].T[0], Fr[Upper].T[1], Fr[Upper].T[2] });
			Inv.T[0] = -Tn.X;
			Inv.T[1] = -Tn.Y;
			Inv.T[2] = -Tn.Z;
			const Xform D = Compose(Now2[Upper], Inv);
			const Xform Wn = Compose(D, Fr[Gun]);
			// back into the weapon's own turn and move from its parent (G = G_parent q; X = X_parent + G_parent (head - head_parent + move))
			const Vec3 Head = S.Bones[static_cast<size_t>(Gun)].Head;
			const Vec3 X = Wn.Point(Head);
			if (Pa < 0)
			{
				P.Rot[Gun] = QOfMat(Wn.R);
				P.Move[Gun] = Sub(X, Head);
			}
			else
			{
				const float* G = Now2[Pa].R;
				const float Gt[9] = { G[0], G[3], G[6], G[1], G[4], G[7], G[2], G[5], G[8] };
				float Q[9];
				M3Mul(Gt, Wn.R, Q);
				const Vec3 PaHead = S.Bones[static_cast<size_t>(Pa)].Head;
				const Vec3 Local = M3Vec(Gt, Sub(X, Now2[Pa].Point(PaHead)));
				P.Rot[Gun] = QOfMat(Q);
				P.Move[Gun] = Sub(Local, Sub(Head, PaHead));
			}
		}

		/** A wounded man held on his fall's last frame breathes (bWounded): spine_02 and chest heave 2 degrees either way
		 * and roll a little, about 0.3 times a second at his own rate and phase, coming in over his first second down. */
		void Breathe(const Man& M, Pose& P, double Now) const
		{
			const double T = (Now - M.WoundEnds) * Life;
			if (T <= 0.0) return;
			const Soldier& S = *M.S;
			const int Sp1 = S.BoneIndex("spine_01"), Sp2 = S.BoneIndex("spine_02"), Ch = S.BoneIndex("chest"), Nk = S.BoneIndex("neck");
			if (Sp2 < 0 && Ch < 0) return;
			Xform Fr[MaxBones];
			BoneFrames(S, P, Fr);
			const int A = Sp1 >= 0 ? Sp1 : Sp2 >= 0 ? Sp2 : Ch, B = Nk >= 0 ? Nk : Ch >= 0 ? Ch : Sp2;
			const Vec3 Spine = Unit(Sub(Fr[B].Point(S.Bones[static_cast<size_t>(B)].Head), Fr[A].Point(S.Bones[static_cast<size_t>(A)].Head)));
			Vec3 Side = Cross(Spine, Vec3 { 0.f, 0.f, 1.f });
			Side = Length(Side) < 0.3f ? Vec3 { 0.f, 1.f, 0.f } : Unit(Side);
			const double Ph = 6.28318530717959 * (0.3 * static_cast<double>(M.Info.WoundRate) * T + static_cast<double>(M.Info.WoundPhase));
			const float In = Smooth(static_cast<float>(T));
			const float Heave = In * 0.0349066f * static_cast<float>(std::sin(Ph));
			const float Roll = In * 0.0122173f * static_cast<float>(std::sin(0.5 * Ph + 1.3));
			TurnBone(S, Fr, P, Sp2, Side, 0.5f * Heave);
			TurnBone(S, Fr, P, Ch, Side, 0.5f * Heave);
			TurnBone(S, Fr, P, Sp2, Spine, Roll);
		}

		/** Whether a loop's pose at its start lies as a fall's last pose does (both on his back), and the turn about his
		 * root (Yaw, radians) and the move (his own axes) that lay its pelvis and its head where the fall left them. */
		bool LiesAlike(const Soldier& S, const Pose& End, const Pose& Loop, float& OutYaw, float& OutX, float& OutY) const
		{
			const int Pe = S.BoneIndex("pelvis"), Hd = S.BoneIndex("head"), Ch = S.BoneIndex("chest");
			if (Pe < 0 || Hd < 0 || Ch < 0) return false;
			Xform A[MaxBones], B[MaxBones];
			BoneFrames(S, End, A);
			BoneFrames(S, Loop, B);
			// (the chest's front, its own x, up: on his back)
			if (A[Ch].R[6] < 0.5f || B[Ch].R[6] < 0.5f) return false;
			const Vec3 PA = A[Pe].Point(S.Bones[static_cast<size_t>(Pe)].Head), HA = A[Hd].Point(S.Bones[static_cast<size_t>(Hd)].Head);
			const Vec3 PB = B[Pe].Point(S.Bones[static_cast<size_t>(Pe)].Head), HB = B[Hd].Point(S.Bones[static_cast<size_t>(Hd)].Head);
			OutYaw = static_cast<float>(std::atan2(HA.Y - PA.Y, HA.X - PA.X) - std::atan2(HB.Y - PB.Y, HB.X - PB.X));
			const float C = std::cos(OutYaw), Sn = std::sin(OutYaw);
			OutX = PA.X - (C * PB.X - Sn * PB.Y);
			OutY = PA.Y - (Sn * PB.X + C * PB.Y);
			return true;
		}

		/** A pose turned about his root's ground point by Yaw and moved by X, Y (his own axes): the whole man, rigid. */
		static void TurnRoot(const Soldier& S, Pose& P, float Yaw, float X, float Y)
		{
			if (S.Bones.empty()) return;
			const Quat Q { std::cos(0.5f * Yaw), 0.f, 0.f, std::sin(0.5f * Yaw) };
			P.Rot[0] = QNormal(QMul(Q, P.Rot[0]));
			const Vec3 Head = S.Bones[0].Head;
			const Vec3 At = Add(Head, P.Move[0]);
			const float C = std::cos(Yaw), Sn = std::sin(Yaw);
			const Vec3 Turned { C * At.X - Sn * At.Y + X, Sn * At.X + C * At.Y + Y, At.Z };
			P.Move[0] = Sub(Turned, Head);
		}

		/**
		 * A man struck and not killed goes down (bWounded; wd and not carried): his fall chosen as a dead man's is, through
		 * the Fates and neighbours (ChooseFall's wound falls: the soldiers thread's wound_arm, wound_gut, wound_leg where his
		 * file has them, else his stance's falls, never thrown, dragging himself on (crawl) as likely as any), the jolt
		 * over it. Then, each man his own: the loop the soldiers thread makes for where he was struck
		 * (wounded_clutch_arm, _gut, _leg; wounded_writhe), else wounded_waiting, where it lies as his fall left him (on
		 * his back: turned about him to lie there, never face down to his back); else his fall's last frame held,
		 * breathing. His own phase and rate. The rules' stances wait till back.
		 */
		void StartWound(Man& M, double At)
		{
			const Soldier& S = *M.S;
			const std::string St = StanceForFall(M, At);
			M.Info.HitPart = HitsNow.bVariety ? HitPartOf(M) : std::string();
			const std::string Kind0 = M.Info.FallKind, Stance0 = M.Info.FallStance, From0 = M.Info.FallFrom;
			int Ci = ChooseFall(M, St, std::string(), At, true);
			M.Info.FallKind = Kind0;
			M.Info.FallStance = Stance0;
			M.Info.FallFrom = From0;
			const float Rate = M.Info.FallRate, Skip = M.Info.FallSkip;
			M.Info.FallRate = 1.f;
			M.Info.FallSkip = 0.f;
			if (Ci < 0) Ci = S.ClipIndex("go_prone");
			if (Ci < 0) return;
			const Clip& C = S.Clips[static_cast<size_t>(Ci)];
			M.Info.Wounded = M.Wd;
			M.Info.Back = M.Back;
			M.Info.WoundStarted = At;
			M.Info.WoundClip = C.Name;
			Sample(C, C.Long, M.WoundEnd);
			M.WoundPivot = PelvisOf(M, M.WoundEnd);
			if (C.bEndOffset)
			{
				M.WoundPivot.X = C.EndOffset.X;
				M.WoundPivot.Y = C.EndOffset.Y;
			}
			Segment G = OnceOf(Ci, Skip, C.Long, At, (C.Long - Skip) / (Life * Rate), Prone, 0.12f);
			G.LieA = M.Lie;
			G.LieB = 1.f;
			G.bWound = true;
			M.Queue.clear();
			Start(M, G, At);
			M.WoundEnds = At + G.Dur;
			M.EndClass = Prone;
			Dice WD(OwnSeed(M, 0x3D0D5ull));
			const double Ph = WD.U();
			const double Rt = WD.U();
			M.Info.WoundPhase = static_cast<float>(Ph);
			M.Info.WoundRate = static_cast<float>(0.85 + 0.3 * Rt);
			// What he does down there: the wanted loops by name, then the stand-in, where it lies as he does.
			const std::string Part = C.Hit.Bone.rfind("thigh", 0) == 0 || C.Hit.Bone.rfind("calf", 0) == 0 || C.Name == "wound_leg" || C.Name.rfind("die_leg_", 0) == 0 ? "leg"
				: C.Hit.Bone.rfind("spine", 0) == 0 || C.Hit.Bone == "pelvis" || C.Name == "wound_gut" || C.Name.rfind("die_gut_", 0) == 0 ? "gut" : "arm";
			const std::string Wanted[3] = { "wounded_clutch_" + Part, "wounded_writhe", "wounded_waiting" };
			M.bBreathe = true;
			M.Info.WoundLoop = "hold";
			Segment Down;
			Down.Clip = Ci;
			Down.How = Segment::Hold;
			Down.HoldT = C.Long;
			Down.BlendIn = 0.f;
			Down.EndClass = Prone;
			Down.LieA = Down.LieB = 1.f;
			Down.bWound = true;
			for (const std::string& Name : Wanted)
			{
				const int Li = S.ClipIndex(Name);
				if (Li < 0) continue;
				Pose Lp;
				Sample(S.Clips[static_cast<size_t>(Li)], 0.f, Lp);
				float Yaw = 0.f, X = 0.f, Y = 0.f;
				if (!LiesAlike(S, M.WoundEnd, Lp, Yaw, X, Y)) continue;
				Down.Clip = Li;
				Down.How = Segment::Loop;
				Down.Phase = M.Info.WoundPhase;
				Down.Rate = M.Info.Jitter * static_cast<float>(Life) * M.Info.WoundRate;
				Down.BlendIn = 0.8f;
				Down.bRifleDown = true;
				Down.bTurned = true;
				Down.Yaw = Yaw;
				Down.ShiftX = X;
				Down.ShiftY = Y;
				M.bBreathe = false;
				M.Info.WoundLoop = Name;
				break;
			}
			M.Queue.push_back(Down);
			if (HitsNow.bJolt) JoltFrom(M, At, true);
		}

		// ---------------------------------------------------------------------------------------------------------
		// After the frames: the rifle laid down, the head dropped.
		// ---------------------------------------------------------------------------------------------------------
		void LowerRifle(const Man& M, Xform* Fr, float W, float Rest) const { LowerRifleOf(*M.S, Fr, W, Rest); }
		static void LowerRifleOf(const Soldier& S, Xform* Fr, float W, float Rest)
		{
			// The rifle turned about his grip until its muzzle is on the ground (Rest metres up), as far as W: the spotter
			// laying it down for his glasses, a dead man's rifle sinking from his hands.
			if (W <= 0.f) return;
			const int Gun = S.WeaponBone();
			const int Muz = S.SocketIndex("socket_muzzle");
			if (Gun < 0 || Muz < 0) return;
			const Socket& So = S.Sockets[static_cast<size_t>(Muz)];
			const Vec3 Grip = Fr[Gun].Point(S.Bones[static_cast<size_t>(Gun)].Head);
			const Vec3 Tip = Fr[So.Bone].Point(So.At);
			const Vec3 D = Sub(Tip, Grip);
			const float H = std::sqrt(D.X * D.X + D.Y * D.Y);
			const float L = Length(D);
			if (H < 0.05f || L < 0.1f) return;
			const float E0 = std::atan2(D.Z, H);
			const float Want = std::asin(std::max(-1.f, std::min(1.f, (Rest - Grip.Z) / L)));
			const float Angle = (E0 - Want) * W;
			if (std::fabs(Angle) < 1e-5f) return;
			const Vec3 Axis = Unit(Cross({ 0.f, 0.f, 1.f }, { D.X / H, D.Y / H, 0.f }));
			float R[9];
			AxisTurn(Axis, Angle, R);
			const Xform T = TurnAbout(Grip, R);
			for (size_t B = 0; B < S.Bones.size(); ++B)
			{
				if (S.OnWeapon(static_cast<int>(B))) Fr[B] = Compose(T, Fr[B]);
			}
		}

		void DropHead(const Man& M, Xform* Fr, float Angle) const
		{
			// A man killed lying at his aim: his head drops onto his forearms, turned about his neck across his body.
			if (Angle <= 0.f) return;
			const Soldier& S = *M.S;
			const int Neck = S.BoneIndex("neck"), Chest = S.BoneIndex("chest");
			if (Neck < 0 || Chest < 0) return;
			const Vec3 Pivot = Fr[Neck].Point(S.Bones[static_cast<size_t>(Neck)].Head);
			const Vec3 Across = Unit({ Fr[Chest].R[1], Fr[Chest].R[4], Fr[Chest].R[7] });
			float R[9];
			AxisTurn(Across, Angle, R);
			const Xform T = TurnAbout(Pivot, R);
			for (size_t B = 0; B < S.Bones.size(); ++B)
			{
				bool bUnder = false;
				for (int P = static_cast<int>(B); P >= 0; P = S.Bones[static_cast<size_t>(P)].Parent)
				{
					if (P == Neck) { bUnder = true; break; }
				}
				if (bUnder) Fr[B] = Compose(T, Fr[B]);
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// One man, one frame.
		// ---------------------------------------------------------------------------------------------------------
		void EmitClipEvents(Man& M, const Clip& C, float A, float B, bool bWrapped, bool bFirstLoop, bool bShots, double Now,
			std::vector<Event>& Out) const
		{
			if (C.Events.empty()) return;
			auto In = [&](float T) {
				if (!bWrapped) return T > A && T <= B;
				return T > A || T <= B;
			};
			for (const ClipEvent& E : C.Events)
			{
				if (!In(E.T)) continue;
				if (E.bFirstLoopOnly && !bFirstLoop) continue;
				// The flash and a grenade's release are the round's own (ShotEvents, on the round's time), not the clip's.
				if ((E.Fx == "muzzle_flash" || E.Fx == "grenade_release") && !bShots) continue;
				Event Ev;
				Ev.T = Now;
				Ev.Man = static_cast<int>(&M - Men.data());
				Ev.Fx = E.Fx;
				Ev.Cue = E.Cue;
				Ev.Army = M.Info.Army;
				if (E.Fx == "muzzle_flash") Ev.Gun = C.Name == "lmg_burst" ? Weapon::Mg : Weapon::Rifle;
				PointOf(M, E.Socket, E.Bone, E.Fx == "rifle_drop", Ev.At, Ev.Dir);
				Out.push_back(Ev);
			}
		}

		void PointOf(const Man& M, int SocketIx, int BoneIx, bool bRifle, Vec3& OutAt, Vec3& OutDir) const
		{
			const Soldier& S = *M.S;
			if (SocketIx >= 0)
			{
				const Socket& So = S.Sockets[static_cast<size_t>(SocketIx)];
				OutAt = M.Frames[So.Bone].Point(So.At);
				OutDir = Unit(M.Frames[So.Bone].Turn(So.Facing));
				return;
			}
			int B = BoneIx;
			if (B < 0 && bRifle) B = S.WeaponBone();
			if (B >= 0)
			{
				OutAt = M.Frames[B].Point(S.Bones[static_cast<size_t>(B)].Head);
				OutDir = Unit(M.World.Turn({ 1.f, 0.f, 0.f }));
				return;
			}
			OutAt = M.World.Point({ 0.f, 0.f, 0.f });
			OutDir = Unit(M.World.Turn({ 1.f, 0.f, 0.f }));
		}

		void ShotEvents(Man& M, double Last, double Now, std::vector<Event>& Out) const
		{
			// One event a round (each bullet of a burst, the cue on the first), at the round's own time, from his muzzle.
			for (size_t I = M.ShotCur; I < M.Shots.size(); ++I)
			{
				Shot& Sh = M.Shots[I];
				if (Sh.T > Now) break;
				const int Bullets = Sh.How == Shot::Burst ? Sh.Bullets : 1;
				while (Sh.Done < Bullets)
				{
					const double Tb = Sh.T + Sh.Done * Gap();
					if (Tb > Now) break;
					// A dead man fires no more (a round booked after his fall, or the rest of a burst he was hit in).
					if (M.Info.Dead >= 0.0 && Tb > M.Info.Dead) { Sh.Done = Bullets; break; }
					Event Ev;
					Ev.T = Tb;
					Ev.Man = static_cast<int>(&M - Men.data());
					Ev.Army = M.Info.Army;
					Ev.Gun = Sh.W;
					Ev.Round = Sh.Done;
					if (Sh.How == Shot::Mortar)
					{
						// The crew's tube stands at his knee (drawn with no tube yet): the round leaves from there.
						Ev.Fx = "mortar";
						Ev.Cue = "mortar_fire";
						Ev.At = M.World.Point({ 0.55f, 0.15f, 0.25f });
						Ev.Dir = Unit(M.World.Turn({ 0.5f, 0.f, 0.87f }));
					}
					else if (Sh.W == Weapon::Grenade)
					{
						Ev.Fx = "grenade_release";
						PointOf(M, M.S->SocketIndex("socket_hand_r"), -1, false, Ev.At, Ev.Dir);
					}
					else
					{
						Ev.Fx = "muzzle_flash";
						Ev.Cue = Sh.Done > 0 ? "" : Sh.W == Weapon::Mg ? "lmg_burst" : Sh.W == Weapon::At ? "at_shot" : "rifle_shot";
						PointOf(M, M.S->SocketIndex("socket_muzzle"), -1, false, Ev.At, Ev.Dir);
					}
					(void)Last;
					Out.push_back(Ev);
					++Sh.Done;
				}
			}
		}

		/** Every how many frames a man is posed when the crowd is thrifty (SetThrift): every frame near the eye and
		 * while he falls; then by his distance, every 2nd frame to 50 m, every 3rd to 120 m, every 4th beyond. */
		static int EveryOf(const Man& M)
		{
			// (a man within 25 m posed every frame, whatever his level: SetLevels may draw him at Mid there)
			if (M.State != Man::Alive || M.L == Near || M.EyeD < 25.f || M.Last < 0.0) return 1;
			return M.EyeD < 50.f ? 2 : M.EyeD < 120.f ? 3 : 4;
		}

		/** A rider one frame: shown or not as the scene says; posed by his clip at its time, his root on his seat. */
		static void StepRider(Man& M, double Now)
		{
			const Crowd::Ride& R = M.RideNow;
			const int NC = M.S ? static_cast<int>(M.S->Clips.size()) : 0;
			M.bVisible = R.bShown && R.Clip >= 0 && R.Clip < NC;
			M.Fade = 1.f;
			if (!M.bVisible) return;
			const Soldier& S = *M.S;
			const Clip& C = S.Clips[static_cast<size_t>(R.Clip)];
			Sample(C, R.Time, M.Now);
			if (R.W < 1.f && R.FromClip >= 0 && R.FromClip < NC)
			{
				// (the clip he leaves, blended out: a rider never falls, so the fall's pose is free to hold it)
				Sample(S.Clips[static_cast<size_t>(R.FromClip)], R.FromTime, M.FallFrom);
				const Vec3 Root = M.Now.Move[0];
				Blend(M.FallFrom, M.Now, std::max(0.f, R.W), M.Now);
				if (R.bOwnRoot) M.Now.Move[0] = Root; // (Effects (R2): a bailing crewman's root the clip's own, his seat moved on)
			}
			BoneFrames(S, M.Now, M.Model);
			M.World = R.Seat;
			// (his gun put away under his feet while a tank clip plays: its bones drawn to a point, so not drawn at all)
			const bool bStowed = C.Weapon == "none" || R.bUnarmed; // (Effects (R2): a bailing crewman, unarmed)
			const size_t NB = S.Bones.size();
			for (size_t B = 0; B < NB; ++B)
			{
				M.Frames[B] = Compose(M.World, M.Model[B]);
				if (bStowed && S.OnWeapon(static_cast<int>(B)))
				{
					for (float& E : M.Frames[B].R) E = 0.f;
				}
			}
			M.X = R.Seat.T[0];
			M.Y = R.Seat.T[1];
			// (his heading: his forward, the seat's first column, as (sin f, cos f))
			M.F = std::atan2(R.Seat.R[0], R.Seat.R[3]);
			M.Last = Now;
			++M.Stamp;
		}

		/** M.Shown while he leaves in the gait LeaveAt gave him: no change of his track's (they count from 0; -1 is none). */
		static constexpr int LeaveShown = -2;

		/**
		 * A living man on foot whose block has left the battle (release 2, SetLeaveWalk; Brian, 25 November 17:11Z: "some
		 * soldiers freeze and then disapear"). The rules keep his last point a tenth of a second before he is out, and a
		 * track read past its last point has no speed, so "carries on at his last speed" stopped every such man dead,
		 * mid-stride if he was moving, and the scene cut him half a second later. Here he goes on from his last kept
		 * point along the last stretch of his track that has length (within a second and a half of it), at its speed,
		 * facing as the rules last had him; a man who was standing, kneeling, lying or cowering gets to his feet, turns
		 * and walks for his own side's edge at 1.4 m/s. He is in a gait all the way (his own if he was in one on his
		 * feet, else a walk, a run over 3 m/s), whole for 3 s after he is out and gone half a second later; the scene
		 * hides him sooner, the first frame he is out of the picture (Crowd::LeftUnseen). The tracked vehicles' way,
		 * LandFeed::SpotAt, for men.
		 */
		void LeaveAt(Man& M, const TrackData& Tr, double Tl, double Now, float& F, float& V)
		{
			const size_t N = Tr.T.size();
			const double TL = Tr.T[N - 1], XL = Tr.X[N - 1], YL = Tr.Y[N - 1];
			const float FaceL = Tr.F[N - 1];
			double Dx = 0.0, Dy = 0.0, Speed = 0.0;
			for (size_t I = N - 1; I > 0 && TL - Tr.T[I] <= 1.5; --I)
			{
				const double Lx = Tr.X[I] - Tr.X[I - 1], Ly = Tr.Y[I] - Tr.Y[I - 1];
				const double Len = std::hypot(Lx, Ly), Span = Tr.T[I] - Tr.T[I - 1];
				if (Len > 0.05 && Span > 1e-6)
				{
					Dx = Lx / Len;
					Dy = Ly / Len;
					Speed = std::min(7.0, Len / Span);
					break;
				}
			}
			const bool bWasStill = Speed < 1.2;
			if (bWasStill)
			{
				// (his own edge: the attack's is y 0, the defence's the field's far side; a block of no side, away from
				// where he faced, which was the enemy)
				const auto Own = BlockOwn.find(M.Info.Block);
				if (Own != BlockOwn.end() && Own->second != 0)
				{
					Dx = 0.0;
					Dy = static_cast<double>(Own->second);
				}
				else
				{
					Dx = -std::sin(static_cast<double>(FaceL));
					Dy = -std::cos(static_cast<double>(FaceL));
				}
				Speed = 1.4;
			}
			if (!M.bLeaving)
			{
				M.bLeaving = true;
				// (on his feet and in a gait already, he keeps it; else he is turned to one, getting up first, and
				// stays where he is while he does)
				const Segment& Cur = M.Queue.empty() ? M.Cur : M.Queue.back();
				const bool bInGait = !bWasStill && Cur.How == Segment::Gait && Cur.Clip >= 0 && M.EndClass == Up;
				M.LeaveHold = bWasStill ? (M.EndClass == Prone ? 0.45f : M.EndClass == Kneel ? 0.35f : 0.15f) : 0.f;
				if (!bInGait)
				{
					const Stance Gone = StanceOf(*M.S, Tr.Role, Speed > 3.0 ? "run" : "walk", true);
					if (Gone.Clip >= 0) TurnToStance(M, Gone, Now, M.LeaveHold > 0.f ? Now + static_cast<double>(M.LeaveHold) : -1.0, 1e18);
				}
				M.Shown = LeaveShown;
			}
			const double Since = Tl - TL, Going = std::max(0.0, Since - static_cast<double>(M.LeaveHold));
			M.X = XL + Dx * Speed * Going;
			M.Y = YL + Dy * Speed * Going;
			V = Going > 0.0 ? static_cast<float>(Speed) : 0.f;
			// (a man who was still turns the short way to where he goes as he gets up; a moving man faces as he did)
			F = FaceL;
			if (bWasStill)
			{
				const double Head = std::atan2(Dx, Dy);
				const double Turn = std::clamp(Since / std::max(0.3, static_cast<double>(M.LeaveHold) + 0.25), 0.0, 1.0);
				F = static_cast<float>(static_cast<double>(FaceL) + std::remainder(Head - static_cast<double>(FaceL), 2.0 * 3.14159265358979) * Turn);
			}
			const double Past = Tl - M.Info.Out;
			M.Fade = Past <= 3.0 ? 1.f : static_cast<float>(std::max(0.0, 1.0 - (Past - 3.0) / 0.5));
			if (M.Fade <= 0.f) M.bVisible = false;
		}

		/** One man one frame. bPose false (a thrifty frame, far from the eye): all he does, his place, his turns of
		 * pose, his shots and his clips' events, but not his bones (they keep the last frame's; his Stamp stays). A man
		 * who starts to fall is posed. */
		void Step(Man& M, double Now, double Dt, std::vector<Event>& Out, bool bQuiet, bool bPose = true)
		{
			if (M.bRider)
			{
				StepRider(M, Now);
				return;
			}
			if (M.State == Man::Rest || !M.bVisible) return;
			// Effects (R2): his fall a gore one (SetGore): drawn as his file's twin from the frame he is struck, swapped
			// before his file is read here, so the fall StartFall picks below is the twin's
			if (bGore && !M.bGore && M.State == Man::Alive && M.Info.Dead >= 0.0 && Now >= M.Info.Dead)
			{
				if (const Soldier* Twin = GoreTwinFor(M, StanceForFall(M, M.Info.Dead))) ToTwin(M, Twin);
			}
			const Soldier& S = *M.S;
			const TrackData& Tr = Tracks[static_cast<size_t>(M.Track)];
			const double Tl = Now - M.Info.Delay;
			if (M.bShotsDirty) PlanShots(M);

			// Where he is: the track lerped on his own timeline; a man who left with his block carries on at his last
			// speed and facing and fades out over a second. A falling man stays where he was hit.
			if (M.State == Man::Alive)
			{
				float F = 0.f, V = 0.f;
				const bool bLeaves = bLeaveWalk && M.Info.Out >= 0.0 && !Tr.T.empty() && Tl > Tr.T.back()
					&& !(M.Shown >= 0 && static_cast<size_t>(M.Shown) < M.Stances.size() && (M.Stances[static_cast<size_t>(M.Shown)].Aboard || M.Stances[static_cast<size_t>(M.Shown)].Lift > 0.f));
				if (M.bLeaving && !bLeaves)
				{
					// (back on the list, or the film wound back: the rules' stance again)
					M.bLeaving = false;
					if (M.Shown == LeaveShown) M.Shown = -1;
				}
				if (bLeaves)
				{
					LeaveAt(M, Tr, Tl, Now, F, V);
					if (!M.bVisible) return;
				}
				else if (M.Info.Out >= 0.0 && Tl > M.Info.Out)
				{
					double X0 = 0.0, Y0 = 0.0;
					size_t Cursor = 0;
					float VOut = 0.f;
					Tr.At(M.Info.Out - 1e-6, Cursor, X0, Y0, F, VOut);
					const double Go = (Tl - M.Info.Out) * VOut;
					M.X = X0 + std::sin(F) * Go;
					M.Y = Y0 + std::cos(F) * Go;
					V = VOut;
					M.Fade = static_cast<float>(std::max(0.0, 1.0 - (Tl - M.Info.Out)));
					if (M.Fade <= 0.f) { M.bVisible = false; return; }
				}
				else Tr.At(Tl, M.AtCursor, M.X, M.Y, F, V);
				// (His facing is the rules', lerped the short way between kept points: smoother than the way of his
				// kept points, which zig-zag a little every few tenths of a second. Drawn square to that way instead, he
				// would twitch at every kept point; as it is, his feet slip a little sideways where it wanders.)
				M.F = F;
				M.V = V;
			}
			if (M.State == Man::Alive && M.Info.Dead >= 0.0 && Now >= M.Info.Dead)
			{
				StartFall(M, M.Info.Dead);
				if (!bQuiet && M.FallClip < 0)
				{
					Event Ev;
					Ev.T = M.Info.Dead;
					Ev.Man = static_cast<int>(&M - Men.data());
					Ev.Fx = "slump";
					Ev.Army = M.Info.Army;
					PointOf(M, -1, S.BoneIndex("chest"), false, Ev.At, Ev.Dir);
					Out.push_back(Ev);
				}
			}
			if (M.State != Man::Alive) bPose = true;
			// (bullet hits: posed every frame while his jolt lasts)
			if (M.JoltAt >= 0.0 && Now >= M.JoltAt && (Now - M.JoltAt) * Life < 0.33) bPose = true;

			Pose P;
			float Lie = 0.f, RifleDown = 0.f, HeadDrop = 0.f;
			Placing Pl;
			Pl.X = M.X;
			Pl.Y = M.Y;
			Pl.Facing = M.F;
			bool bRest = false;
			int BankK = -1;        // the bank's pose he shows (SO1), or -1
			int ShowA = -1, ShowB = -1;   // the mesh a vertex-animation material draws: bake A to bake B by ShowW
			float ShowW = 0.f;
			bool bDownA = false, bDownB = false;   // (each side with his rifle laid down: the bake's laid-down copy)
			bool bSample = bPose;  // his clips sampled (posed and not banked)
			bool bDown = false;    // (bullet hits, bWounded: down wounded, the rules' stances waiting till back)
			if (M.State == Man::Alive && HitsNow.bWounded && M.Wd >= 0.0 && !M.bCarried && Now >= M.Wd && !(M.Info.Dead >= 0.0 && M.Info.Dead <= M.Wd))
			{
				if (M.Back < 0.0 || Now < M.Back)
				{
					if (M.Info.WoundStarted < 0.0) StartWound(M, M.Wd);
					bDown = M.Info.WoundStarted >= 0.0;
				}
				else if (M.Info.WoundStarted >= 0.0 && M.UpAt < 0.0)
				{
					// Up again at back: from lying (TurnTo from Prone to the rules' stance now), his body brought back
					// from where his fall left it to his place over the get-up.
					M.UpAt = Now;
					M.UpShift = M.WoundPivot;
					M.EndClass = Prone;
					M.Shown = -1;
					M.bBreathe = false;
				}
			}
			if (M.State == Man::Alive)
			{
				// The rules' stance on his timeline; a man about to get up and move starts getting up early, so he is
				// on his feet when the rules have him go (he would otherwise slide up off the ground at a run).
				while (M.ChCursor + 1 < M.Stances.size() && M.Stances[M.ChCursor + 1].T <= Tl) ++M.ChCursor;
				if (M.ChCursor > 0 && M.Stances[M.ChCursor].T > Tl) M.ChCursor = 0;
				// (leaving with his block: the gait LeaveAt turned him to, whatever his last change was)
				int Want = M.bLeaving ? LeaveShown : static_cast<int>(M.ChCursor);
				double Until = -1.0;
				if (!M.bLeaving && M.ChCursor + 1 < M.Stances.size())
				{
					const Stance& Next = M.Stances[M.ChCursor + 1];
					if (Next.Play == Stance::Gait && Next.Cl == Up && M.EndClass != Up)
					{
						const double Lead = M.EndClass == Prone ? 0.45 : 0.35;
						if (Next.T - Tl <= Lead && M.Shown != static_cast<int>(M.ChCursor) + 1)
						{
							Want = static_cast<int>(M.ChCursor) + 1;
							Until = Now + (Next.T - Tl);
						}
					}
				}
				if (!M.bLeaving && M.Shown == static_cast<int>(M.ChCursor) + 1 && Want == static_cast<int>(M.ChCursor)) Want = M.Shown;
				if (Want != M.Shown && Want >= 0 && !bDown)
				{
					TurnTo(M, Want, Now, Until);
					M.Shown = Want;
				}
				// Move on through the turn's queue.
				while (M.Cur.How == Segment::Once && !M.Queue.empty() && Now >= M.Cur.Start + M.Cur.Dur)
				{
					Segment Next = M.Queue.front();
					M.Queue.pop_front();
					const double At = M.Cur.Start + M.Cur.Dur;
					Start(M, Next, At);
				}
				// A gait's step goes on at the ground's pace: the clip at his speed over its own, no faster than 1.6 times
				// (the battle runs faster than life; past that the feet slide, as the contract accepts).
				auto Pace = [&](Segment& G) {
					if (G.How != Segment::Gait || G.Clip < 0) return;
					const Clip& C = S.Clips[static_cast<size_t>(G.Clip)];
					const float Sp = C.Speed();
					// (his speed is the battle's, its clock Life times life's: on life's clock the clip may play 1.6 times)
					const float Most = 1.6f * static_cast<float>(Life);
					float Rate = Sp > 0.f ? M.V / Sp : 0.f;
					G.bCapped = Rate > Most;
					Rate = std::min(Rate, Most);
					G.Rate = Rate;
					if (C.Long > 0.f)
					{
						const float Before = G.Phase;
						G.Phase += static_cast<float>(Dt) * Rate / C.Long;
						if (G.Phase >= 1.f) { G.Phase -= std::floor(G.Phase); if (Before < 1.f) ++G.Loops; }
					}
				};
				Pace(M.Cur);
				if (M.bPrev) Pace(M.Prev);
				const float W = M.Cur.BlendIn > 0.f ? Smooth(static_cast<float>((Now - M.Cur.Start) * Life / M.Cur.BlendIn)) : 1.f;

				// The pose bank (SO1): a far man shows the bank's pose nearest to where the clip shown has got to (of two
				// blending, the one more than half in); going prone slides him, so it is posed as before. Banked, he is
				// placed every frame (a thrifty frame too: no clip is sampled nor bone framed, only his place).
				// The mesh drawn by a vertex-animation material (BankShow) goes smoothly between them: the clip's poses either
				// side of where it has got to, or two clips' nearest by their blend.
				if (bBankOn && M.Banked && BanksAt(M.L) && !M.Cur.bShift && !(M.bPrev && W < 1.f && M.Prev.bShift) && !M.Cur.bTurned && !(M.bPrev && W < 1.f && M.Prev.bTurned))
				{
					const Segment& G = M.bPrev && W < 0.5f ? M.Prev : M.Cur;
					float LieG = 0.f;
					if (G.Clip >= 0)
					{
						const float Tg = ClipTime(M, G, Now, LieG);
						BankK = PickBank(M, G.Clip, Tg);
						if (BankK >= 0 && !BracketBank(M, G.Clip, Tg, ShowA, ShowB, ShowW)) { ShowA = ShowB = BankK; ShowW = 0.f; }
						bDownA = bDownB = G.bRifleDown;
					}
					if (BankK >= 0)
					{
						bPose = true;
						if (M.bPrev && W < 1.f && M.Prev.Clip >= 0 && M.Cur.Clip >= 0)
						{
							const int Ka = PickBank(M, M.Prev.Clip, ClipTime(M, M.Prev, Now, LieG));
							const int Kb = PickBank(M, M.Cur.Clip, ClipTime(M, M.Cur, Now, LieG));
							if (Ka >= 0 && Kb >= 0)
							{
								ShowA = Ka;
								ShowB = Kb;
								ShowW = W;
								bDownA = M.Prev.bRifleDown;
								bDownB = M.Cur.bRifleDown;
							}
						}
					}
				}
				bSample = bPose && BankK < 0;

				Pose A;
				float LieB = 0.f, TB = 0.f;
				if (bSample) SampleSegment(M, M.Cur, Now, P, LieB, TB);
				else TB = ClipTime(M, M.Cur, Now, LieB);
				Lie = LieB;
				RifleDown = M.Cur.bRifleDown ? W : 0.f;
				if (M.bPrev && W < 1.f)
				{
					float LieA = 0.f, TA = 0.f;
					if (bSample)
					{
						SampleSegment(M, M.Prev, Now, A, LieA, TA);
						Blend(A, P, W, P);
					}
					else ClipTime(M, M.Prev, Now, LieA);
					Lie = LieA + (LieB - LieA) * W;
					if (M.Prev.bRifleDown) RifleDown += 1.f - W;
				}
				else M.bPrev = false;

				// The clip's own events (footsteps, knees, falls, kit): from the segment shown.
				if (!bQuiet && M.Cur.Clip >= 0)
				{
					const Clip& C = S.Clips[static_cast<size_t>(M.Cur.Clip)];
					const float Was = M.Cur.LastT;
					if (Was >= 0.f && M.Cur.How != Segment::Hold)
					{
						const bool bBack = M.Cur.How == Segment::Once && M.Cur.To < M.Cur.From;
						const bool bWrapped = (M.Cur.How == Segment::Gait || M.Cur.How == Segment::Loop) && TB < Was;
						if (!bBack && (TB > Was || bWrapped))
						{
							EmitClipEvents(M, C, Was, TB, bWrapped, M.Cur.Loops == 0 || (M.Cur.Loops == 1 && bWrapped), bClipShotsRef && *bClipShotsRef, Now, Out);
						}
					}
				}
				M.Cur.LastT = TB;

				// The rounds: around each, the firing clip's shot over his held aim.
				M.bShooting = false;
				M.ShotW = 0.f;
				while (M.ShotCur < M.Shots.size() && M.Shots[M.ShotCur].End + 0.3 < Now && M.Shots[M.ShotCur].Done >= (M.Shots[M.ShotCur].How == Shot::Burst ? M.Shots[M.ShotCur].Bullets : 1)) ++M.ShotCur;
				for (size_t I = M.ShotCur; I < M.Shots.size() && !bDown; ++I)
				{
					Shot& Sh = M.Shots[I];
					if (Sh.Start - 0.15 / Life > Now) break;
					if (Sh.How != Shot::Clip && Sh.How != Shot::Burst) continue;
					if (Now > Sh.End + 0.2 / Life) continue;
					const Clip& C = S.Clips[static_cast<size_t>(Sh.ClipIx)];
					float Kick = 0.f;
					const float Ct = ShotClipTime(Sh, C, Now, Kick);
					Pose Q;
					if (bSample) Sample(C, Ct, Q);
					if (bSample && Kick > 0.f && C.FlashT >= 0.f)
					{
						Pose K;
						Sample(C, C.FlashT + 0.06f, K);
						Blend(Q, K, Kick * 0.7f, Q);
					}
					const float In = Smooth(static_cast<float>((Now - (Sh.Start - 0.15 / Life)) * Life / 0.15));
					const float OutW = 1.f - Smooth(static_cast<float>((Now - Sh.End) * Life / 0.2));
					const float Ws = std::min(In, OutW);
					if (Ws > 0.f)
					{
						if (bSample) Blend(P, Q, Ws, P);
						else if (BankK >= 0)
						{
							// (banked: the firing clip's pose while the shot is mostly shown, if the bank has it; the mesh
							// drawn goes from the pose before it to the shot's by its weight, then through the shot's poses)
							const int Ks = PickBank(M, Sh.ClipIx, Ct);
							if (Ks >= 0)
							{
								const int Held = ShowW < 0.5f ? ShowA : ShowB;
								const bool bHeldDown = ShowW < 0.5f ? bDownA : bDownB;
								if (Ws >= 0.5f) BankK = Ks;
								if (Ws < 1.f) { ShowA = Held; ShowB = Ks; ShowW = Ws; bDownA = bHeldDown; }
								else if (!BracketBank(M, Sh.ClipIx, Ct, ShowA, ShowB, ShowW)) { ShowA = ShowB = Ks; ShowW = 0.f; }
								if (Ws >= 1.f) bDownA = false;
								bDownB = false;
							}
						}
						M.bShooting = true;
						M.ShotW = Ws;
						RifleDown *= 1.f - Ws;
						if (C.Name == "throw_grenade") Lie *= 1.f - Ws;
					}
					if (!bQuiet && Sh.How == Shot::Clip && Ct > Sh.LastC && Sh.LastC >= 0.f)
					{
						// Its bolt and its throw (the muzzle flash is the round's own, below).
						EmitClipEvents(M, C, Sh.LastC, Ct, false, true, false, Now, Out);
					}
					Sh.LastC = Ct;
					break;
				}
			}
			else
			{
				// Falling, or a man killed lying going limp.
				const double Since = Now - M.FallAt;
				const float U = static_cast<float>(Clamp01(static_cast<float>(Since / std::max(0.05, M.FallDur))));
				if (M.State == Man::Falling)
				{
					const Clip& C = S.Clips[static_cast<size_t>(M.FallClip)];
					// (bullet hits, bVariety: his own rate and start where no other clip was left)
					const double Into = M.Info.FallRate != 1.f || M.Info.FallSkip != 0.f ? M.Info.FallSkip + Since * Life * M.Info.FallRate : Since * Life;
					const float Ct = static_cast<float>(std::min(Into, static_cast<double>(C.Long)));
					Sample(C, Ct, P);
					Blend(M.FallFrom, P, Smooth(static_cast<float>(Since * Life / 0.12)), P);
					if (!bQuiet && Ct > M.FallLastT) EmitClipEvents(M, C, M.FallLastT < 0.f ? -1.f : M.FallLastT, Ct, false, true, false, Now, Out);
					M.FallLastT = Ct;
					// Over the second half of the fall he comes onto the ground's slope, about where his pelvis will rest.
					const float K = Smooth((U - 0.5f) * 2.f);
					Lie = M.FallLie0 + (1.f - M.FallLie0) * K;
					Pl.Pivot = Scale(M.FallPivot, K);
				}
				else
				{
					P = M.FallFrom;
					Lie = M.FallLie0;
					const float K = Smooth(U) * (2.f - Smooth(U)) * 0.5f + Smooth(U) * 0.5f;
					HeadDrop = 0.42f * K;
					RifleDown = K;
				}
				bRest = Since >= M.FallDur;
			}

			// Frames: in his own axes, then on the field.
			M.Lie = Lie;
			M.RifleDown = RifleDown;
			M.Last = Now;
			if (!bPose)
			{
				// (a thrifty frame: his bones as they were; his shots still go off on time, from his muzzle as last posed)
				if (!bQuiet) ShotEvents(M, M.Last, Now, Out);
				return;
			}
			if (BankK >= 0 && RifleDown >= 0.5f && M.Banked->Have[BankSpec.size() + static_cast<size_t>(BankK)])
			{
				BankK += static_cast<int>(BankSpec.size());   // (his rifle laid down: the bake's laid-down copy)
			}
			if (BankK >= 0)
			{
				// (the mesh drawn: each side's rifle laid down as its own clip holds it, so a blend into a gun crew's hold
				// lays it down as it goes)
				const size_t NP = BankSpec.size();
				auto Down = [&](int K) { return K >= 0 && static_cast<size_t>(K) < NP && M.Banked->Have[NP + static_cast<size_t>(K)] ? K + static_cast<int>(NP) : K; };
				if (ShowA < 0) { ShowA = ShowB = BankK; ShowW = 0.f; }
				else
				{
					if (bDownA) ShowA = Down(ShowA);
					if (bDownB) ShowB = Down(ShowB);
				}
				if (ShowA == ShowB) ShowW = 0.f;
			}
			else ShowA = ShowB = -1, ShowW = 0.f;
			M.WasA = M.ShowA;
			M.WasB = M.ShowB;
			M.WasW = M.ShowW;
			M.ShowA = ShowA;
			M.ShowB = ShowB;
			M.ShowW = ShowW;
			// (banked and still in the same pose, his mesh is as it was: only his place moved, WorldFrame; Stamp stays)
			const bool bSameBank = BankK >= 0 && BankK == M.BankK;
			M.BankK = BankK;
			if (BankK >= 0)
			{
				// (banked: his bones are the bake's, as it was framed; nothing lowered or dropped out there)
				const Bank& Bk = *M.Banked;
				M.Now = Bk.Poses[static_cast<size_t>(BankK)];
				const Xform* From = &Bk.Model[static_cast<size_t>(BankK) * static_cast<size_t>(Bk.Bones)];
				std::copy(From, From + Bk.Bones, M.Model);
			}
			else
			{
				// (bullet hits: the jolt and a wounded man's breathing, layers over his pose)
				if (M.JoltAt >= 0.0) Jolt(M, P, Now);
				if (M.bBreathe && bDown) Breathe(M, P, Now);
				M.Now = P;
				BoneFrames(S, P, M.Model);
				if (RifleDown > 0.f) LowerRifle(M, M.Model, std::min(1.f, RifleDown), M.State == Man::Alive ? 0.035f : 0.02f);
				if (HeadDrop > 0.f) DropHead(M, M.Model, HeadDrop);
			}
			Pl.Lie = Lie;
			// (bullet hits, bWounded: down, laid on the slope about where his pelvis comes to rest; up again, his body
			// brought back from there to his place over the get-up)
			if (bDown) Pl.Pivot = Scale(M.WoundPivot, Lie);
			if (M.State == Man::Alive && M.UpAt >= 0.0 && !bDown)
			{
				const float K = 1.f - Smooth(static_cast<float>((Now - M.UpAt) * Life / 1.2));
				if (K > 0.f)
				{
					const double Sf = std::sin(static_cast<double>(Pl.Facing)), Cf = std::cos(static_cast<double>(Pl.Facing));
					Pl.X += (Sf * static_cast<double>(M.UpShift.X) + Cf * static_cast<double>(M.UpShift.Y)) * static_cast<double>(K);
					Pl.Y += (Cf * static_cast<double>(M.UpShift.X) - Sf * static_cast<double>(M.UpShift.Y)) * static_cast<double>(K);
				}
			}
			// (carried: raised onto the stretcher over his first second on it, as the bearers lift him)
			if (M.State == Man::Alive && M.Shown >= 0 && static_cast<size_t>(M.Shown) < M.Stances.size() && M.Stances[static_cast<size_t>(M.Shown)].Lift > 0.f)
			{
				const Stance& Lifted = M.Stances[static_cast<size_t>(M.Shown)];
				Pl.Lift = Lifted.Lift * static_cast<float>(std::clamp(Now - Lifted.T, 0.0, 1.0));
			}
			// (aboard: on his vehicle's bench, or kneeling on her floor by it, as the scene says she stands now; where no
			// vehicle carries him there, where the rules have him, hidden under the canvas of a lorry Men in lorries rides)
			bool bSeated = false;
			if (M.State == Man::Alive && SeatAt && M.Shown >= 0 && static_cast<size_t>(M.Shown) < M.Stances.size() && M.Stances[static_cast<size_t>(M.Shown)].Aboard)
			{
				SeatPlace Seat;
				const Stance& On = M.Stances[static_cast<size_t>(M.Shown)];
				if (SeatAt(M.X, M.Y, M.Info.Block, On.SeatOn, On.Seat, Seat))
				{
					const bool bSits = On.Aboard == 1 && Seat.bBench;
					// (kneeling: a step in off his bench, his knees toward the bed's middle)
					const double In = bSits || !Seat.bBench ? 0.0 : 0.25;
					Pl.X = Seat.X + std::sin(Seat.Facing) * In;
					Pl.Y = Seat.Y + std::cos(Seat.Facing) * In;
					Pl.Facing = Seat.Facing;
					Pl.Lie = 0.f;
					Pl.Pivot = Vec3();
					const float Under = GroundAt ? GroundAt(Pl.X, Pl.Y).Height : 0.f;
					Pl.Lift = static_cast<float>((bSits ? Seat.Z : Seat.Floor) - static_cast<double>(Under));
					bSeated = true;
				}
			}
			M.bUnderCanvas = !bSeated && M.State == Man::Alive && M.Shown >= 0 && static_cast<size_t>(M.Shown) < M.Stances.size()
				&& M.Stances[static_cast<size_t>(M.Shown)].bUnderCanvas;
			M.Place = Pl;
			M.World = WorldOf(Pl, GroundAt, &M.Footing);
			M.bFooting = true;
			const size_t NB = S.Bones.size();
			for (size_t B = 0; B < NB; ++B) M.Frames[B] = Compose(M.World, M.Model[B]);
			if (!bQuiet) ShotEvents(M, M.Last, Now, Out);
			else
			{
				for (size_t I = M.ShotCur; I < M.Shots.size() && M.Shots[I].T <= Now; ++I) M.Shots[I].Done = M.Shots[I].How == Shot::Burst ? M.Shots[I].Bullets : 1;
			}
			if (bRest)
			{
				// At rest for good: on the ground where he fell, laid on its slope about his pelvis. Skinned once per
				// level when first asked, and never again.
				Placing R;
				R.X = M.X;
				R.Y = M.Y;
				R.Facing = M.F;
				R.Lie = 1.f;
				R.Pivot = M.State == Man::Falling ? M.FallPivot : Vec3();
				M.Place = R;
				M.World = WorldOf(R, GroundAt, &M.Footing);
				M.bFooting = true;
				for (size_t B = 0; B < NB; ++B) M.Frames[B] = Compose(M.World, M.Model[B]);
				M.State = Man::Rest;
				M.Info.Rested = Now;
			}
			if (!bSameBank) ++M.Stamp;
		}

		/** One man back to the start of the battle (a man new to a live battle starts there too). */
		static void ResetMan(Man& M)
		{
			M.State = Man::Alive;
			M.BankK = -1;
			M.ShowA = M.ShowB = M.WasA = M.WasB = -1;
			M.ShowW = M.WasW = 0.f;
			M.AtCursor = M.ChCursor = 0;
			M.Shown = -1;
			M.EndClass = M.Stances.empty() ? Up : M.Stances[0].Cl;
			M.Prev = M.Cur = Segment();
			M.bPrev = false;
			M.Queue.clear();
			M.Fade = 1.f;
			M.bVisible = true;
			M.bLeaving = false;
			M.LeaveHold = 0.f;
			M.Last = -1.0;
			M.FallClip = -1;
			M.Info.FallClip.clear();
			M.Info.FallKind.clear();
			M.Info.GoreFrom.clear();
			M.Info.FallStarted = M.Info.Rested = -1.0;
			// (bullet hits)
			M.Info.FallRate = 1.f;
			M.Info.FallSkip = 0.f;
			M.Info.HitPart.clear();
			M.Info.Wounded = M.Info.Back = M.Info.WoundStarted = -1.0;
			M.Info.WoundClip.clear();
			M.Info.WoundLoop.clear();
			M.JoltAt = M.WoundEnds = M.UpAt = -1.0;
			M.bBreathe = false;
			for (int L = 0; L < LodCount; ++L) { M.bSkinned[L] = false; M.RestAt[L].clear(); M.RestFacing[L].clear(); }
			for (Shot& Sh : M.Shots) { Sh.Done = 0; Sh.LastC = -1.f; }
			M.ShotCur = 0;
			RestPose(M.Now);
		}

		void Reset()
		{
			for (Man& M : Men)
			{
				if (M.bGore) FromTwin(M); // (Effects (R2): his own file again, to fall afresh)
				ResetMan(M);
			}
			GoredN = 0;
			Falls = FallMemory();
			Clock = -1.0;
		}

		// ---------------------------------------------------------------------------------------------------------
		// Effects (R2): the gore falls (SetGore). A man killed by a shell, a tank's gun or a grenade near its burst may
		// fall as his file's gore twin: the same man, his mesh the twin's (split at the eight cuts, its caps hidden in him
		// till a limb goes), his fall one of the twin's, in which an arm or a leg is taken off and flies on its own bone.
		// ---------------------------------------------------------------------------------------------------------
		/** His file's twin when his fall is to be a gore one (the burst's odds by how far he stood from it), or null. */
		const Soldier* GoreTwinFor(const Man& M, const std::string& St) const
		{
			if (M.bRider || M.KillT < 0.0) return nullptr;
			const auto Found = Gores.find(M.Info.FileArmy);
			if (Found == Gores.end() || !Found->second) return nullptr;
			// How often (the art director, 23 Nov 17:52Z: about 1 shell kill in 8 to 10 a gore fall, rarer and stronger,
			// grenades only close): a man within 2 m of the burst Near (a field gun's or a howitzer's shell 0.55, a tank's
			// or a mortar's 0.35, a grenade's 0.25), falling away steeply past 2 m (by e every 1.5 m) to none past Reach.
			// The rules lay most bursts on the man they kill (8 in 10 shell kills, his own spot), which says nothing of
			// how close the shell truly fell: such a man OnHim (a gun's 0.2, the others 0.12). Measured on the rules'
			// Normandy and Prokhorovka, shell blasts off and on: 1 shell kill in 7.6 to 11 a gore fall, while a man lying
			// took any shell; with twins whose prone falls are only the throw from under him, 1 in 12 to 22.5 as the
			// recordings are (2 of Normandy's 45 shell kills, 4 of Prokhorovka's 48; these odds summed over them expect
			// 2.8 and 5.1); with the lying falls of 24 Nov (below), 7 and 7, 1 in 6.4 and 6.9, where these odds summed over
			// the same kills expect 4.3 and 5.6 (1 in 10.5 and 8.6; 9.8 of the 93, 1 in 9.5, within the art director's 1
			// in 8 to 10). The 14 are those two seeded battles' dice, about 1.5 standard deviations above that (8 of them
			// lying, by a mortar's or a tank's shell; b470998e gored the same 14 with those twins). Not changed here.
			double Near = 0.0, OnHim = 0.0, Reach = 0.0;
			if (M.KillWeapon == "gun") Near = 0.55, OnHim = 0.2, Reach = 8.0;
			else if (M.KillWeapon == "tank") Near = 0.35, OnHim = 0.12, Reach = 6.0;
			else if (M.KillW == Weapon::Mortar) Near = 0.35, OnHim = 0.12, Reach = 6.0;
			else if (M.KillW == Weapon::Grenade) Near = 0.25, OnHim = 0.12, Reach = 2.0;
			else return nullptr;
			// (where he stood when it struck him, by his track: the same whatever frame his fall is first played on)
			const Blast Bl = BlastOf(M);
			const double Far = std::hypot(M.KillX - Bl.X, M.KillY - Bl.Y);
			double Odds = Far < 0.25 ? OnHim : Far > Reach ? 0.0 : Far <= 2.0 ? Near : Near * std::exp(-(Far - 2.0) / 1.5);
			if (St == "prone" || St == "cover") Odds *= 0.5; // (lying, or behind cover: his body under the blast)
			// None where his stance's gore falls hold none he may take (GoreSide; the deaths thread, 20:1xZ: the big
			// throw only for guns and howitzers, MayTake): a man lying is gored by any shell as other men are where the
			// twin's prone falls hold falls that are not throws (the deaths thread, 24 Nov); where they hold only the throw
			// from under him, only by a gun's shell that burst on him. He then falls as any man; asked before his dice, so
			// no man's dice change.
			if (GoreSide(*Found->second, StrikeOf(M, St, Bl)).empty()) return nullptr;
			Dice D(Seed ^ Mix(0x60AEull + static_cast<uint64_t>(M.Info.Block) * 1000003ull + static_cast<uint64_t>(M.Info.K))
				^ (M.Info.J > 0 ? Mix(0x60AE5ull + static_cast<uint64_t>(M.Info.J)) : 0ull));
			return D.U() < Odds ? Found->second.get() : nullptr;
		}

		/** His mesh at each level and its face's rest points for the file he is drawn with now; his triangles and
		 * colours made again, his bank's bake his own file's or (a gore twin) none, so he is posed and skinned. */
		void Remesh(Man& M)
		{
			for (int L = 0; L < LodCount; ++L)
			{
				M.FaceAt[L] = &FaceRestOf(M.S, CutsOf(M, L), L, M.Info.Face);
				M.Var[L] = &VariantOf(M.S, CutsOf(M, L), L);
				M.bLook[L] = false;
				M.bSkinned[L] = false;
				M.RestAt[L].clear();
				M.RestFacing[L].clear();
			}
			if (M.bGore)
			{
				M.Banked = nullptr;
				for (int L = 0; L < LodCount; ++L) { M.BankAt[L] = nullptr; M.FaceMoved[L] = nullptr; }
				M.BankK = -1;
				M.ShowA = M.ShowB = M.WasA = M.WasB = -1;
				M.ShowW = M.WasW = 0.f;
				M.BankKey = M.BodyKey = -1;
			}
			else AttachBank(M);
			++M.MeshNo;
			++M.Stamp;
		}

		/** The worn look's mix changed (SetWearMix) after men were made: each man's wear settled again and his looks made
		 * again (his MeshNo moves, so a scene makes his triangles and colours again); every set of men alike made anew
		 * from its key (the bakes kept: they are the pose bank's, not the look's), each banked man attached again. */
		void Relook()
		{
			for (Man& M : Men)
			{
				SettleWear(M);
				for (int L = 0; L < LodCount; ++L) M.bLook[L] = false;
				++M.MeshNo;
				++M.Stamp;
			}
			if (BankSets.empty() && BodySets.empty()) return;
			BankSets.clear();
			BankSetOf.clear();
			BodySets.clear();
			BodySetOf.clear();
			for (Man& M : Men)
			{
				// (as ReadPoseBank attaches them, but not a rider, nor a man gored: neither is banked, Remesh)
				if (M.S && !M.bRider && !M.bGore) AttachBank(M);
			}
		}

		void ToTwin(Man& M, const Soldier* Twin)
		{
			M.OwnS = M.S;
			M.S = Twin;
			M.bGore = true;
			Remesh(M);
			++GoredN;
		}

		void FromTwin(Man& M)
		{
			if (!M.OwnS) return;
			M.S = M.OwnS;
			M.OwnS = nullptr;
			M.bGore = false;
			Remesh(M);
		}

		// ---------------------------------------------------------------------------------------------------------
		// The men from the tracks. Build reads a whole recording, Extend a live battle a delta at a time; both make a
		// track's man, his stances, his shots and his kill with these, so a battle grown delta by delta is the battle
		// read whole.
		// ---------------------------------------------------------------------------------------------------------
		/** A track on the map, found by its block, model and follower (a round's shooter, a section's key man). */
		void Place(int TrackIx)
		{
			TrackData& Tr = Tracks[static_cast<size_t>(TrackIx)];
			Tr.bPlaced = true;
			TrackOf[{ Tr.B, Tr.K, Tr.J }] = TrackIx;
			bSections = bSections || Tr.J > 0;
		}

		/**
		 * The man of a track of a man's role (tanks, guns and lorries are the scene's own; and a section's followers'
		 * roles: its leader, an MG's or a tube's number two and the ammunition bearers, a gun's crew), appended: his
		 * soldier file, his looks, his stances, his meshes. False only when no soldier file draws his army.
		 */
		bool MakeMan(int TrackIx, std::string& OutError)
		{
			TrackData& Tr = Tracks[static_cast<size_t>(TrackIx)];
			static const char* MenRoles[] = { "officer", "rifle", "mg", "at", "sapper", "holder", "fo", "hmg", "mortar", "leader", "loader", "crew" };
			bool bMan = false;
			for (const char* R : MenRoles) bMan = bMan || Tr.Role == R;
			if (!bMan) return true;
			const auto Found = BlockArmy.find(Tr.B);
			const std::string Army = Found != BlockArmy.end() ? Found->second.first : "GER";
			std::string File = Found != BlockArmy.end() ? Found->second.second : Army;
			// (the army's own soldier file when it has one, whatever the recording stood in for it when it was made)
			if (Soldiers.find(Army) != Soldiers.end()) File = Army;
			// (his colours: the army's row for this front where armies.json has one, "UK@desert" his khaki drill, else the
			// army's own row)
			auto Colours = Armies.find(Army + "@" + Front);
			if (Colours == Armies.end()) Colours = Armies.find(Army);
			// (an army with no row at all: armies.json's "_default", plain khaki on the Pole's body, never unpainted; its
			// stand-in body only where the recording's block named none of its own that was read)
			bool bDefaultRow = false;
			if (Colours == Armies.end()) { Colours = Armies.find("_default"); bDefaultRow = true; }
			const bool bBlockOwn = bDefaultRow && File != Army && Soldiers.find(File) != Soldiers.end();
			if (Colours != Armies.end() && !Colours->second.StandIn.empty() && Soldiers.find(Army) == Soldiers.end() && !bBlockOwn)
			{
				File = Colours->second.StandIn;
				if (Soldiers.find(File) != Soldiers.end() && StandInSaid.insert(Army).second)
					StandInLines.push_back("the " + Army + " men drawn with soldier_" + File + " in their own paints (armies.json's stand-in: no soldier_" + Army + ".json yet)");
			}
			// (his dress for the front and the season where the soldiers thread has made it: the Italian's sahariana in the
			// desert, soldier_ITA_desert; the Soviet's quilted winter dress, soldier_SOV_winter, else his snow suit)
			const std::string Dressed = File;
			if (Front == "desert" && Soldiers.find(File + "_desert") != Soldiers.end()) File += "_desert";
			else if (SeasonName == "winter" && Soldiers.find(File + "_winter") != Soldiers.end()) File += "_winter";
			else if (SeasonName == "winter" && Soldiers.find(File + "_snow") != Soldiers.end()) File += "_snow";
			// (that dress in its own colours where armies.json has a row for it: "ITA_desert" the sahariana's sand,
			// "SOV_winter" the felt boots, "SOV_snow" the white suit)
			if (File != Dressed)
			{
				const auto Dress = Armies.find(File);
				if (Dress != Armies.end()) Colours = Dress;
			}
			// (a machine gunner from the army's gunner's file where it has one: his section's own gun, soldier_<ARMY>_lmg,
			// the German's MG 42, and before July 1942 the earlier one where it is made, _lmg34, his MG 34 (release 2: the
			// scene reads those files only on trial "gunners"); else the American's BAR, soldier_USA_bar)
			if (Tr.Role == "mg")
			{
				const bool bEarly = MonthNo < 1942 * 12 + 6;
				if (bEarly && Soldiers.find(File + "_lmg34") != Soldiers.end()) File += "_lmg34";
				else if (Soldiers.find(File + "_lmg") != Soldiers.end()) File += "_lmg";
				else if (Soldiers.find(File + "_bar") != Soldiers.end()) File += "_bar";
			}
			// (a section's leader with his nation's leader's arm: the Gruppenfuehrer's MP 40, the Soviet squad leader's
			// PPSh, the British section leader's Sten, from the army's _smg file where one is made; until then, and for
			// the American squad leader, whose issue in 1944 was the M1 rifle, the army's rifleman's file)
			if (Tr.Role == "leader" && LeaderCarriesSmg(Army) && Soldiers.find(File + "_smg") != Soldiers.end()) File += "_smg";
			// (the American squad leader from the soldiers thread's _nco file: a staff sergeant's chevrons and the Thompson
			// that many in Normandy carried in place of the M1; the sound's smgCue plays its bursts)
			else if (Tr.Role == "leader" && Soldiers.find(File + "_nco") != Soldiers.end()) File += "_nco";
			// (an anti-tank team: its gunner from the army's _bazooka file, and his number two, the loader who follows
			// him, from its _loader file, where the army has them: the American's 2.36-inch rocket launcher team)
			if (Tr.Role == "at" && Soldiers.find(File + "_bazooka") != Soldiers.end()) File += "_bazooka";
			// (a battery's spotter with the army's radio man where it has one: the American's SCR-300 on his back)
			if (Tr.Role == "fo" && Soldiers.find(File + "_radio") != Soldiers.end()) File += "_radio";
			if (Tr.Role == "loader" && Tr.J > 0 && Soldiers.find(File + "_loader") != Soldiers.end())
			{
				const auto Key = TrackOf.find({ Tr.B, Tr.K, 0 });
				if (Key != TrackOf.end() && Tracks[static_cast<size_t>(Key->second)].Role == "at") File += "_loader";
			}
			auto Sol = Soldiers.find(File);
			if (Sol == Soldiers.end()) Sol = Soldiers.find(Army);
			// (never a side left blank for a file not made or not found: the army's stand-in in armies.json, else the Pole's
			// body, the nearest cut (the American was drawn with it before his own came), else any file read; in his own
			// army's paints, and said once a battle: StandIns)
			if (Sol == Soldiers.end() || !Sol->second)
			{
				const std::string Asked = File;
				if (Colours != Armies.end() && !Colours->second.StandIn.empty()) Sol = Soldiers.find(Colours->second.StandIn);
				if (Sol == Soldiers.end() || !Sol->second) Sol = Soldiers.find("POL");
				if (Sol == Soldiers.end() || !Sol->second)
					for (Sol = Soldiers.begin(); Sol != Soldiers.end() && !Sol->second; ++Sol) {}
				if (Sol == Soldiers.end() || !Sol->second)
				{
					OutError = "no soldier file for the " + Army + " men (" + Asked + "), and none read to stand in";
					return false;
				}
				if (StandInSaid.insert(Army).second)
					StandInLines.push_back("the " + Army + " men drawn with soldier_" + Sol->first + " in their own paints (no soldier_" + Asked + ".json read)");
			}
			// (made in place: a man is some 9 KB, too big to make and then copy in)
			Man& Mn = Men.emplace_back();
			Mn.Track = TrackIx;
			Mn.S = Sol->second.get();
			static const ArmyColours Plain;
			Mn.Colours = Colours != Armies.end() ? &Colours->second : &Plain;
			// (uniforms that age: his side's months in the line fade his army's cloth, a row made once a wear)
			if (bWornLook)
			{
				const auto Worn = BlockWorn.find(Tr.B);
				const double Months = WornForced >= 0.0 ? WornForced : Worn != BlockWorn.end() ? Worn->second : 0.0;
				if (Months > 0.0)
				{
					Mn.WearMonths = Months; // (and his wear for the worn look: SettleWear)
					Mn.Fresh = Mn.Colours;
					Mn.WornA = static_cast<float>(std::clamp(Months / 18.0, 0.0, 1.0));
					// (up to some 45 in 100 men in a newer tunic or trousers than the rest of their kit, drawn piecemeal:
					// his own colour at every range, so a row of its own, one of three a side)
					const uint64_t Who = (static_cast<uint64_t>(static_cast<uint32_t>(TrackIx)) << 20) ^ 0xA6EDull;
					const int Garment = Unit01(Who ^ 0x1ull) < 0.45 * Mn.WornA ? (Unit01(Who ^ 0x2ull) < 0.65 ? 1 : 0) : -1;
					Mn.Colours = &AgedRow(*Mn.Fresh, static_cast<int>(std::lround(Mn.WornA * 20.f)), Garment);
				}
			}
			SettleWear(Mn);
			Mn.Info.Block = Tr.B;
			Mn.Info.K = Tr.K;
			Mn.Info.J = Tr.J;
			Mn.Info.Role = Tr.Role;
			Mn.Info.Army = Army;
			Mn.Info.FileArmy = Sol->first;
			Mn.Info.bFo = Tr.bFo;
			Mn.Info.Dead = Tr.D;
			Mn.Info.Out = Tr.D >= 0.0 ? -1.0 : Tr.OutT; // (a dead man never leaves: he lies where he fell)
			Mn.Wd = Tr.Wd;
			Mn.Back = Tr.Back;
			Mn.bCarried = Tr.bCarried;
			// His own looks and his own small differences, from his block and model, so he keeps them all battle and
			// from one run to the next: the face (a hash mod 8), the helmet's finish (a second hash, by share), a lag of
			// up to a fifth of a second behind the recording, a play rate of 0.95 to 1.05 on his still loops and turns
			// of pose, and where in its loop his first step falls.
			// (a follower's own: his j stirred in, so a key man's looks are as they were before sections)
			const uint64_t Who = ((static_cast<uint64_t>(static_cast<uint32_t>(Tr.B)) << 32) ^ static_cast<uint64_t>(static_cast<uint32_t>(Tr.K)))
				^ (Tr.J > 0 ? Mix(0xF0110ull + static_cast<uint64_t>(Tr.J)) : 0ull);
			const Soldier& S = *Mn.S;
			Mn.Info.Face = S.Faces.empty() ? 0 : static_cast<int>(Mix(Who ^ 0xFACEull) % 8ull) % static_cast<int>(S.Faces.size());
			Mn.Info.Finish = -1;
			if (Mn.Colours->bFileFinishes)
			{
				std::vector<std::pair<int, double>> Fit;
				double Sum = 0.0;
				for (size_t F = 0; F < S.Finishes.size(); ++F)
				{
					// (Brian's standing rule, 26 November 2026: a man's markings and paint never hang on the year. A finish fits
					// by its season and its front; the "from" and "to" months an older soldier file may still carry are not
					// asked, so no battle's date leaves his helmet bare of its badge or paint.)
					const Finish& Fi = S.Finishes[F];
					const bool bSeason = Fi.Season == "any" || Fi.Season == SeasonName;
					const bool bFront = Fi.Front == "any" || Fi.Front == Front;
					if (bSeason && bFront && Fi.Share > 0.0) { Fit.push_back({ static_cast<int>(F), Fi.Share }); Sum += Fi.Share; }
				}
				if (!Fit.empty())
				{
					double R = Unit01(Who ^ 0x4E1A7Eull) * Sum;
					Mn.Info.Finish = Fit.back().first;
					for (const auto& F : Fit)
					{
						R -= F.second;
						if (R <= 0.0) { Mn.Info.Finish = F.first; break; }
					}
				}
				// (No finish fits the season and front: his own helmet, painted his army's helmet colour.)
			}
			Mn.Info.Delay = 0.2 * Unit01(Who ^ 0xDE1Aull);
			Mn.Info.Jitter = static_cast<float>(0.95 + 0.1 * Unit01(Who ^ 0x7177E5ull));
			Mn.PhaseSeed = static_cast<float>(Unit01(Who ^ 0x57E9ull));
			Mn.StylePick = static_cast<float>(Unit01(Who ^ 0x5717E5ull));
			MakeStances(Mn);
			// His mesh without his helmet (where a finish is drawn) and his face's rest points, made now rather than on
			// the frame he first comes into view (a few milliseconds each the first time).
			for (int L = 0; L < LodCount; ++L)
			{
				Mn.FaceAt[L] = &FaceRestOf(Mn.S, CutsOf(Mn, L), L, Mn.Info.Face);
				Mn.Var[L] = &VariantOf(Mn.S, CutsOf(Mn, L), L);
			}
			AttachBank(Mn);
			Tr.Man = static_cast<int>(Men.size()) - 1;
			ManOf[{ Tr.B, Tr.K, Tr.J }] = Tr.Man;
			return true;
		}

		/**
		 * His stances, one per change of his track, each from the change's pose (a horse holder's by whether he moves
		 * just after it); a man with no change yet stands idle. Made from Firm on: a live battle's track grows, and a
		 * stance that read his speed past his last kept point is made again when the next ones come.
		 */
		void MakeStances(Man& Mn)
		{
			const TrackData& Tr = Tracks[static_cast<size_t>(Mn.Track)];
			const Soldier& S = *Mn.S;
			if (Mn.bIdle && !Tr.Ch.empty())
			{
				// (his first changes came: they take the idle's place, and he turns to the first of them)
				Mn.Stances.clear();
				Mn.Firm = 0;
				Mn.bIdle = false;
				Mn.Shown = -1;
			}
			for (size_t C = Mn.Firm; C < Tr.Ch.size(); ++C)
			{
				const TrackData::Change& Cg = Tr.Ch[C];
				size_t Cursor = Tr.CursorAt(Cg.T + 0.05);
				double X = 0.0, Y = 0.0;
				float F = 0.f, V = 0.f;
				Tr.At(Cg.T + 0.05, Cursor, X, Y, F, V);
				const bool bMoving = V > 0.2f;
				// (the same pose, moving or not as before: the same stance)
				if (C < Mn.Stances.size() && Mn.Stances[C].bMoving == bMoving) continue;
				// (and his own soldier and role: a pose he took a little before, moving or not alike, is drawn alike)
				const Stance* Like = nullptr;
				for (size_t B = std::min(C, Mn.Stances.size()), Back = 0; B > 0 && Back < 8 && !Like; --B, ++Back)
				{
					const Stance& O = Mn.Stances[B - 1];
					// (a gunner's waiting stance is by his cover too: lying in the open, kneeling behind something low)
					if (O.bMoving == bMoving && O.Pose == Cg.Pose && (S.Weapon != "lmg" || LowCover(O.Cover) == LowCover(Cg.Cover))) Like = &O;
				}
				Stance St = Like ? *Like : StanceOf(S, Tr.Role, Cg.Pose, bMoving, Mn.Serves, Cg.Cover);
				St.T = Cg.T;
				St.SeatOn = Cg.SeatOn;
				St.Seat = Cg.Seat;
				St.OffAt = Cg.OffAt;
				St.Cover = Cg.Cover;
				St.bMoving = bMoving;
				if (C < Mn.Stances.size()) Mn.Stances[C] = std::move(St);
				else Mn.Stances.push_back(std::move(St));
			}
			if (Mn.Stances.empty())
			{
				Stance St = StanceOf(S, Tr.Role, "idle", false);
				St.T = Tr.T.front();
				Mn.Stances.push_back(St);
				Mn.bIdle = true;
			}
			const double Kept = Tr.KeptUntil();
			while (Mn.Firm < Tr.Ch.size() && Tr.Ch[Mn.Firm].T + 0.05 < Kept) ++Mn.Firm;
		}

		void Fire(int Ix, double T, Weapon W)
		{
			if (Ix < 0 || Ix >= static_cast<int>(Men.size())) return;
			Man& Mn = Men[static_cast<size_t>(Ix)];
			Shot Sh;
			Sh.T = T;
			Sh.W = W;
			Mn.Shots.push_back(Sh);
			Mn.bShotsDirty = true;
		}

		/** A round: his shot booked to the man who fired it, and a kill kept for the man it struck (Struck: his number). */
		void Book(const RoundRow& R, int* Struck)
		{
			// (the man of the section who fired it, j, and the man it struck, oj: 0, or none written, the key man)
			if (Struck) *Struck = -1;
			// (no shot for a blast's further man, or for a man run over: the tank's tracks, not a gun)
			if (R.bBy && R.bByK && !R.bMate && R.W != "track")
			{
				const auto Found = ManOf.find({ static_cast<int>(R.By), static_cast<int>(R.ByK), std::max(0, static_cast<int>(R.ByJ)) });
				if (Found != ManOf.end()) Fire(Found->second, R.T, WeaponOf(R.W));
			}
			if (R.R == "kill" && R.bOnK)
			{
				const auto Found = ManOf.find({ static_cast<int>(R.On), static_cast<int>(R.OnK), std::max(0, static_cast<int>(R.OnJ)) });
				if (Found == ManOf.end()) return;
				Man::Kill K;
				K.T = R.T;
				K.By = R.bBy ? static_cast<int>(R.By) : -1;
				K.K = R.bByK ? static_cast<int>(R.ByK) : -1;
				K.J = std::max(0, static_cast<int>(R.ByJ));
				K.Weapon = R.W;
				K.X = R.X;
				K.Y = R.Y;
				Men[static_cast<size_t>(Found->second)].Kills.push_back(std::move(K));
				if (Struck) *Struck = Found->second;
			}
		}

		/** The round a dead man fell to, of those that killed him: the one nearest his fall (the first of equals). */
		static void ChooseKill(Man& Mn)
		{
			Mn.KillT = -1.0;
			Mn.KillBy = Mn.KillK = -1;
			Mn.KillJ = 0;
			Mn.KillW = Weapon::Other;
			Mn.KillWeapon.clear();
			Mn.KillX = Mn.KillY = 0.0;
			if (Mn.Info.Dead < 0.0) return;
			for (const Man::Kill& K : Mn.Kills)
			{
				if (Mn.KillT >= 0.0 && std::fabs(K.T - Mn.Info.Dead) >= std::fabs(Mn.KillT - Mn.Info.Dead)) continue;
				Mn.KillT = K.T;
				Mn.KillBy = K.By;
				Mn.KillK = K.K;
				Mn.KillJ = K.J;
				Mn.KillWeapon = K.Weapon;
				Mn.KillW = WeaponOf(Mn.KillWeapon);
				Mn.KillX = K.X;
				Mn.KillY = K.Y;
			}
		}
	};

	Crowd::Crowd() : Inner(new Impl())
	{
		Inner->bClipShotsRef = &bClipShots;
	}

	Crowd::~Crowd() = default;

	void Crowd::AddSoldier(const std::string& FileArmy, std::shared_ptr<const Soldier> S)
	{
		Inner->Soldiers[FileArmy] = std::move(S);
	}

	bool Crowd::AddGore(const std::string& FileArmy, const Soldier& Twin, std::string& OutWhy)
	{
		Impl& I = *Inner;
		const auto Base = I.Soldiers.find(FileArmy);
		if (Base == I.Soldiers.end() || !Base->second)
		{
			OutWhy = "no soldier file of that name added to be its twin";
			return false;
		}
		const Soldier& B = *Base->second;
		if (Twin.DeathsGore.empty())
		{
			OutWhy = "it has no gore falls (deaths_gore)";
			return false;
		}
		bool bSame = Twin.Bones.size() == B.Bones.size();
		for (size_t K = 0; bSame && K < B.Bones.size(); ++K)
		{
			bSame = Twin.Bones[K].Name == B.Bones[K].Name && Twin.Bones[K].Parent == B.Bones[K].Parent;
		}
		if (!bSame)
		{
			OutWhy = "its bones are not its file's";
			return false;
		}
		// The twin's mesh, faces, helmets and paints (wound, bone, blood) with the file's own clips, sockets and falls,
		// its gore falls and their sockets (wound_<bone>, end_<bone>) after them: every clip number a man of the file
		// holds stays his when he is swapped to it.
		auto M = std::make_shared<Soldier>(Twin);
		M->Weapon = B.Weapon;
		M->Clips = B.Clips;
		M->Deaths = B.Deaths;
		for (const Clip& C : Twin.Clips)
		{
			if (B.ClipIndex(C.Name) < 0) M->Clips.push_back(C);
		}
		M->Sockets = B.Sockets;
		for (const Socket& So : Twin.Sockets)
		{
			if (B.SocketIndex(So.Name) < 0) M->Sockets.push_back(So);
		}
		for (Clip& C : M->Clips)
		{
			for (ClipEvent& E : C.Events)
			{
				E.Socket = E.SocketName.empty() ? -1 : M->SocketIndex(E.SocketName);
				E.Bone = E.BoneName.empty() ? -1 : M->BoneIndex(E.BoneName);
			}
		}
		M->Styles = B.Styles;
		M->Styles.resize(M->Clips.size());
		for (const auto& P : B.OwnPaints) M->OwnPaints.insert(P);
		for (const auto& P : B.OwnPaintNames) M->OwnPaintNames.insert(P);
		// (release 2's worn look, where both were read with the wear: the twin's points are the file's one for one before
		// the gore's, but only the file records the face's detail laid in their tint and the eyes' shade it set)
		M->TakeFaceDetail(B);
		I.Gores[FileArmy] = std::move(M);
		return true;
	}

	void Crowd::SetGore(bool bOn) { Inner->bGore = bOn; }
	void Crowd::SetLeaveWalk(bool bOn) { Inner->bLeaveWalk = bOn; }
	bool Crowd::Leaving(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		return Mn.bLeaving && Mn.State == Impl::Man::Alive && Mn.bVisible;
	}
	void Crowd::LeftUnseen(int Man)
	{
		Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (Mn.bLeaving && Mn.State == Impl::Man::Alive) { Mn.Fade = 0.f; Mn.bVisible = false; }
	}
	void Crowd::SetHits(const Hits& H) { Inner->HitsNow = H; }
	const Crowd::Hits& Crowd::HitsOf() const { return Inner->HitsNow; }

	bool Crowd::PoseOf(int Man, Pose& Out) const
	{
		if (Man < 0 || Man >= Count()) return false;
		Out = Inner->Men[static_cast<size_t>(Man)].Now;
		return true;
	}

	bool Crowd::StruckPlace(int Man, Vec3& OutAt, std::string& OutPart) const
	{
		if (Man < 0 || Man >= Count()) return false;
		const Impl& I = *Inner;
		const Impl::Man& M = I.Men[static_cast<size_t>(Man)];
		if (!M.S) return false;
		const Soldier& S = *M.S;
		// The clip he went down in: his fall's, or a wounded man's.
		int Ci = M.FallClip;
		if (Ci < 0 && M.Info.WoundStarted >= 0.0) Ci = S.ClipIndex(M.Info.WoundClip);
		const bool bPosed = M.Last >= 0.0 && (M.State != Impl::Man::Alive || M.Info.WoundStarted >= 0.0);
		if (Ci >= 0 && bPosed)
		{
			const Clip& C = S.Clips[static_cast<size_t>(Ci)];
			std::string Bone = C.Hit.Bone;
			if (Bone.empty())
			{
				if (C.Name.rfind("die_head_", 0) == 0) Bone = "head";
				else if (C.Name.rfind("die_gut_", 0) == 0 || C.Name == "wound_gut") Bone = "spine_01";
				else if (C.Name.rfind("die_leg_", 0) == 0 || C.Name == "wound_leg")
					Bone = M.JoltCalf >= 0 && S.Bones[static_cast<size_t>(M.JoltCalf)].Name == "calf_l" ? "thigh_l" : "thigh_r";
			}
			if (Bone == "thigh" || Bone == "calf" || Bone == "upperarm" || Bone == "clavicle" || Bone == "hand")
				Bone += C.Hit.Side == "left" ? "_l" : "_r";
			int B = Bone.empty() ? -1 : S.BoneIndex(Bone);
			if (B >= 0)
			{
				// (a bone's middle: its head and its first child's head halfway; the head bone a hand over its joint)
				Vec3 At = M.Frames[B].Point(S.Bones[static_cast<size_t>(B)].Head);
				int Child = -1;
				for (size_t K = 0; K < S.Bones.size(); ++K)
				{
					if (S.Bones[K].Parent == B && !S.OnWeapon(static_cast<int>(K))) { Child = static_cast<int>(K); break; }
				}
				if (Bone == "head") At = M.Frames[B].Point(Add(S.Bones[static_cast<size_t>(B)].Head, Vec3 { 0.f, 0.f, 0.1f }));
				else if (Child >= 0)
				{
					const Vec3 To = M.Frames[Child].Point(S.Bones[static_cast<size_t>(Child)].Head);
					At = Scale(Add(At, To), 0.5f);
				}
				OutAt = At;
				OutPart = Bone;
				return true;
			}
			// (else his chest, a hand toward the shooter: JoltAway is from him, in his own axes)
			const int Ch = S.BoneIndex("chest");
			if (Ch >= 0)
			{
				const Vec3 At = M.Frames[Ch].Point(S.Bones[static_cast<size_t>(Ch)].Head);
				const Vec3 Toward = M.World.Turn(Scale(M.JoltAway, -0.12f));
				OutAt = Add(At, M.JoltAt >= 0.0 ? Toward : Vec3());
				OutPart = "chest";
				return true;
			}
		}
		// Not known: his stance's height over his place.
		double X = 0.0, Y = 0.0, Z = 0.0;
		float F = 0.f;
		if (!PlaceOf(Man, X, Y, Z, F)) return false;
		const Playing Pl = PlayingOf(Man);
		float H = 1.35f;
		if (Pl.Lie > 0.5f) H = 0.25f;
		else if (M.EndClass == Kneel || Pl.Clip == "fire_kneel" || Pl.Clip == "kneel_down") H = 0.85f;
		else if (Pl.bGait && Pl.Speed > 2.2f * static_cast<float>(I.Life)) H = 1.3f;
		OutAt = { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z) + H };
		OutPart = "body";
		return true;
	}
	int Crowd::Gored() const { return Inner->GoredN; }
	uint32_t Crowd::MeshNo(int Man) const { return Inner->Men[static_cast<size_t>(Man)].MeshNo; }

	bool Crowd::ReadArmies(const std::string& Text, std::string& OutError)
	{
		J Root;
		if (!ReadJson(Text, Root, OutError)) return false;
		const J& As = Root["armies"];
		if (As.K != J::Obj) { OutError = "armies.json has no armies"; return false; }
		const std::vector<std::string>& Names = PaintNames();
		for (size_t I = 0; I < As.Size(); ++I)
		{
			const J& A = As.Vals[I];
			ArmyColours C;
			for (size_t P = 0; P < Names.size() && P < 16; ++P)
			{
				const J& Entry = A["paints"][Names[P].c_str()];
				C.Paint[P] = Hex(Entry.K == J::Str ? Entry.S : Entry["rgb"].Text("808080"), 0x808080);
			}
			C.StandIn = A["standIn"].Text();
			C.bFileFinishes = A["helmets"].Text("file") != "own";
			Inner->Armies[As.Keys[I]] = C;
		}
		return true;
	}

	bool Crowd::Build(const std::string& RecordingText, GroundFn GroundAt, std::string& OutError)
	{
		return Build(RecordingText, std::move(GroundAt), Options(), OutError);
	}

	bool Crowd::Build(const std::string& RecordingText, GroundFn GroundAt, const Options& With, std::string& OutError)
	{
		Impl& I = *Inner;
		J Root;
		if (!ReadJson(RecordingText, Root, OutError)) return false;
		I.GroundAt = std::move(GroundAt);
		I.With = With;
		I.Seed = static_cast<uint64_t>(Root["seed"].Num0(1.0)) * 0x9E3779B97F4A7C15ull + 17;
		I.Tracks.clear();
		I.TrackOf.clear();
		I.TrackOfN.clear();
		I.Men.clear();
		I.ManOf.clear();
		I.bSections = false;
		I.StandInSaid.clear();
		I.StandInLines.clear();

		// The battle's month, season and front, for the helmets' finishes.
		{
			static const char* Months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September",
				"October", "November", "December" };
			const std::string Date = Root["date"].Text();
			int Month = 6, Year = 1944;
			for (int K = 0; K < 12; ++K)
			{
				if (Date.find(Months[K]) != std::string::npos) Month = K;
			}
			for (size_t K = 0; K + 4 <= Date.size(); ++K)
			{
				if (Date[K] == '1' && Date[K + 1] == '9' && std::isdigit(static_cast<unsigned char>(Date[K + 2])) && std::isdigit(static_cast<unsigned char>(Date[K + 3])))
					Year = std::atoi(Date.substr(K, 4).c_str());
			}
			I.MonthNo = Year * 12 + Month;
			// (April to October is summer dress and summer finishes; the rest winter.)
			I.SeasonName = Month >= 3 && Month <= 9 ? "summer" : "winter";
			I.Front = With.Front;
			if (I.Front.empty())
			{
				// From the place on the earth, roughly: the lead can say it outright.
				const J& LL = Root["sky"]["latLon"];
				const double Lat = LL.Number(0, 49.0), Lon = LL.Number(1, 0.0);
				I.Front = FrontAt(Lat, Lon);
			}
		}

		// Each block's army and the soldier file its men are drawn from.
		I.BlockArmy.clear();
		I.BlockCal.clear();
		I.BlockOwn.clear();
		I.BlockWorn.clear();
		I.ArmyWorn.clear();
		I.AgedRows.clear();
		const J& Bs = Root["blocks"];
		for (size_t K = 0; K < Bs.Size(); ++K)
		{
			const J& B = Bs.At(K);
			const std::string Army = B["army"].Text(B["nat"].Text("GER").c_str());
			std::string File = B["standIn"].Text(Army.c_str());
			I.BlockArmy[static_cast<int>(B["id"].Num0())] = { Army, File };
			// (Effects (R2): its guns' calibre, for a gore fall's big throw: BigThrow)
			if (!B["cal"].Text().empty()) I.BlockCal[static_cast<int>(B["id"].Num0())] = B["cal"].Text();
			// (uniforms that age: his side's months in the line, the campaign's, where the recording has them)
			const std::string Side = B["side"].Text();
			I.BlockOwn[static_cast<int>(B["id"].Num0())] = Side == "att" ? -1 : Side == "def" ? 1 : 0;
			if (!Side.empty())
			{
				const double Months = std::max(0.0, Root["sides"][Side.c_str()]["worn"].Num0(0.0));
				I.BlockWorn[static_cast<int>(B["id"].Num0())] = Months;
				I.ArmyWorn[Army] = std::max(I.ArmyWorn[Army], Months);
				// (and by his nation, which is what a rider asks by: a block whose army is named apart from its nation)
				const std::string Nat = B["nat"].Text();
				if (!Nat.empty() && Nat != Army) I.ArmyWorn[Nat] = std::max(I.ArmyWorn[Nat], Months);
			}
		}

		// The tracks (a live battle's header has none: Extend brings them), then their men.
		const J& Ms = Root["men"];
		for (size_t K = 0; K < Ms.Size(); ++K)
		{
			const J& Src = Ms.At(K);
			TrackData Tr;
			Tr.ReadWho(Src);
			Tr.ReadPoints(Src["at"]);
			Tr.ReadChanges(Src["ch"]);
			if (Src.Has("d")) Tr.D = Src["d"].Num0();
			if (Src.Has("out")) Tr.OutT = Src["out"].Num0();
			if (Src["wd"].K == J::Num) Tr.Wd = Src["wd"].Num0();
			if (Src["back"].K == J::Num) Tr.Back = Src["back"].Num0();
			Tr.bCarried = Src["cy"].Size() > 0;
			if (Tr.T.empty()) continue;
			I.Tracks.push_back(std::move(Tr));
			I.Place(static_cast<int>(I.Tracks.size()) - 1);
		}
		// (room made first: a man is too big to copy about as the list grows)
		I.Men.reserve(I.Tracks.size());
		for (size_t K = 0; K < I.Tracks.size(); ++K)
		{
			if (!I.MakeMan(static_cast<int>(K), OutError)) return false;
		}

		// The rounds: the men's shots, and the round each dead man fell to (where it came from, for his fall).
		if (With.bReadRounds)
		{
			const J& Rs = Root["rounds"];
			for (size_t K = 0; K < Rs.Size(); ++K) I.Book(RowOf(Rs.At(K)), nullptr);
			for (Impl::Man& Mn : I.Men) Impl::ChooseKill(Mn);
		}
		I.Reset();
		I.bBuilt = true;
		return true;
	}

	bool Crowd::Extend(const std::string& DeltaText, std::string& OutError)
	{
		Impl& I = *Inner;
		if (!I.bBuilt)
		{
			OutError = "a live battle's delta before its header (Build)";
			return false;
		}
		// The delta read whole before anything of it is taken (a bad one changes nothing), into flat lists: each track's
		// news, its new kept points and changes, the tails, the rounds. Read as it goes, not into a tree: see Scan.
		std::vector<Impl::News>& Read = I.NewsRead;
		std::vector<double>& Pts = I.PointsRead;
		std::vector<TrackData::Change>& Chs = I.ChangesRead;
		std::vector<double>& Tails = I.TailsRead;
		std::vector<RoundRow>& Rounds = I.RoundsRead;
		Read.clear();
		Pts.clear();
		Chs.clear();
		Tails.clear();
		Rounds.clear();
		double Now = 0.0;
		bool bNow = false, bReset = false;
		Scan In(DeltaText);
		Scan::Plain V;
		// (a track's kept points, [t, x, y, f] each, as TrackData::ReadPoints reads them)
		auto ReadPoints = [&]() {
			return In.Items([&](size_t) {
				const size_t At = Pts.size();
				Pts.resize(At + 4, 0.0);
				if (!In.AtList()) return In.Skip();
				return In.Items([&](size_t K) {
					if (!In.Value(V)) return false;
					if (K < 4) Pts[At + K] = V.Number();
					return true;
				});
			});
		};
		// (his changes, [t, pose, cover, aimX, aimY, hidden, buttoned] each, and aboard [..., seatOn, seat, off], as
		// TrackData::ReadChanges reads them)
		Scan::Plain Item[10];
		auto ReadChanges = [&]() {
			return In.Items([&](size_t) {
				TrackData::Change Cg;
				for (Scan::Plain& It : Item) It.K = J::Null;
				bool bAllNumbers = true;   // (a list of numbers alone holds no items, as the tree keeps it)
				bool bOk = true;
				if (!In.AtList()) bOk = In.Skip();
				else bOk = In.Items([&](size_t K) {
					Scan::Plain& Into = K < 10 ? Item[K] : V;
					if (!In.Value(Into)) return false;
					bAllNumbers = bAllNumbers && Into.K == J::Num;
					return true;
				});
				if (bAllNumbers) for (Scan::Plain& It : Item) It.K = J::Null;
				Cg.T = Item[0].Num0();
				Cg.Pose = Item[1].Text("idle");
				Cg.Cover = Item[2].Text("none");
				Cg.bHidden = Item[5].Num0() != 0.0;
				if (Item[7].K == J::Num && Item[8].K == J::Num)
				{
					Cg.SeatOn = static_cast<int>(Item[7].N);
					Cg.Seat = static_cast<int>(Item[8].N);
					Cg.OffAt = Item[9].K == J::Num ? Item[9].N : -1.0;
				}
				Chs.push_back(std::move(Cg));
				return bOk;
			});
		};
		const bool bRead = In.Members([&](const std::string& Key) {
			if (Is(Key, "t")) { if (!In.Value(V)) return false; bNow = V.K == J::Num; Now = V.N; return true; }
			if (Is(Key, "reset")) { if (!In.Value(V)) return false; bReset = V.K == J::Bool && V.B; return true; }
			if (Is(Key, "tails"))
			{
				if (!In.AtList()) return In.Skip();
				// (n, x, y, f: n -1 where it is not a number, as the tree reads it)
				return In.Items([&](size_t K) {
					if (!In.Value(V)) return false;
					Tails.push_back(V.Number(K % 4 == 0 ? -1.0 : 0.0));
					return true;
				});
			}
			if (Is(Key, "rounds"))
			{
				if (!In.AtList()) return In.Skip();
				return In.Items([&](size_t) {
					Rounds.emplace_back();
					RoundRow& Row = Rounds.back();
					if (!In.AtObject()) return In.Skip(); // (not a round: nothing of it is read)
					return In.Members([&](const std::string& Field) {
						if (!In.Value(V)) return false;
						Row.Set(Field, V);
						return true;
					});
				});
			}
			if (!Is(Key, "men") || !In.AtList()) return In.Skip();
			return In.Items([&](size_t) {
				Read.emplace_back();
				Impl::News& Nw = Read.back();
				Nw.AtFrom = Nw.AtTo = Pts.size() / 4;
				Nw.ChFrom = Nw.ChTo = Chs.size();
				return In.Members([&](const std::string& Field) {
					if (Is(Field, "at") || Is(Field, "ch"))
					{
						if (!In.AtList()) return In.Skip();
						const bool bAt = Is(Field, "at");
						// (a field written twice: the first is the one read, as the tree has it)
						if (bAt ? Nw.AtTo > Nw.AtFrom : Nw.ChTo > Nw.ChFrom) return In.Skip();
						if (bAt) { Nw.AtFrom = Pts.size() / 4; if (!ReadPoints()) return false; Nw.AtTo = Pts.size() / 4; }
						else { Nw.ChFrom = Chs.size(); if (!ReadChanges()) return false; Nw.ChTo = Chs.size(); }
						return true;
					}
					if (!In.Value(V)) return false;
					if (Is(Field, "n")) Nw.N = static_cast<int>(V.Num0(-1.0));
					else if (Is(Field, "b")) { Nw.bWho = V.K != J::Null; Nw.B = static_cast<int>(V.Num0()); }
					else if (Is(Field, "k")) Nw.K = static_cast<int>(V.Num0());
					else if (Is(Field, "j")) Nw.J = std::max(0, static_cast<int>(V.Num0()));
					else if (Is(Field, "r")) Nw.Role = V.Text();
					else if (Is(Field, "fo")) Nw.bFo = V.Num0() != 0.0;
					else if (Is(Field, "d")) { Nw.bD = V.K != J::Null; Nw.D = V.Num0(); }
					else if (Is(Field, "out")) { Nw.bOut = V.K != J::Null; Nw.Out = V.Num0(); }
					return true;
				});
			});
		});
		if (!bRead)
		{
			OutError = In.Problem().empty() ? std::string("bad JSON") : In.Problem();
			return false;
		}
		if (!bNow)
		{
			OutError = "not a live ground battle's delta (no t)";
			return false;
		}
		if (bReset)
		{
			// The men placed again before the clock ran: every track and man is dropped, and the battle begins again
			// with this delta (its track numbers from 0), as from the header.
			I.Tracks.clear();
			I.TrackOf.clear();
			I.TrackOfN.clear();
			I.Men.clear();
			I.ManOf.clear();
			I.bSections = false;
			I.Reset();
		}

		// The tracks' news. A new track's n is the count so far, and it says who it is that first time.
		std::vector<char> Seen(I.TrackOfN.size(), 0);   // the tracks this delta touched, by n
		auto Touch = [&](int N) {
			if (Seen.size() <= static_cast<size_t>(N)) Seen.resize(static_cast<size_t>(N) + 1, 0);
			Seen[static_cast<size_t>(N)] = 1;
		};
		for (Impl::News& Nw : Read)
		{
			const int N = Nw.N;
			if (N < 0 || N > static_cast<int>(I.TrackOfN.size()))
			{
				OutError = "a delta names track " + std::to_string(N) + " of " + std::to_string(I.TrackOfN.size());
				return false;
			}
			if (N == static_cast<int>(I.TrackOfN.size()))
			{
				if (!Nw.bWho)
				{
					OutError = "a new track " + std::to_string(N) + " without who it is";
					return false;
				}
				TrackData Tr;
				Tr.B = Nw.B;
				Tr.K = Nw.K;
				Tr.J = Nw.J;
				Tr.Role = std::move(Nw.Role);
				Tr.bFo = Nw.bFo;
				I.TrackOfN.push_back(static_cast<int>(I.Tracks.size()));
				I.Tracks.push_back(std::move(Tr));
			}
			TrackData& Tr = I.Tracks[static_cast<size_t>(I.TrackOfN[static_cast<size_t>(N)])];
			if (Nw.AtTo > Nw.AtFrom)
			{
				// The provisional last point gives way to the new kept ones; it stays only while it is still later.
				double TailT = 0.0, TailX = 0.0, TailY = 0.0;
				float TailF = 0.f;
				const bool bHadTail = Tr.bTail && !Tr.T.empty();
				if (bHadTail)
				{
					TailT = Tr.T.back(); TailX = Tr.X.back(); TailY = Tr.Y.back(); TailF = Tr.F.back();
					Tr.T.pop_back(); Tr.X.pop_back(); Tr.Y.pop_back(); Tr.F.pop_back();
					Tr.bTail = false;
				}
				for (size_t P = Nw.AtFrom; P < Nw.AtTo; ++P)
				{
					Tr.T.push_back(Pts[P * 4]);
					Tr.X.push_back(Pts[P * 4 + 1]);
					Tr.Y.push_back(Pts[P * 4 + 2]);
					Tr.F.push_back(static_cast<float>(Pts[P * 4 + 3]));
				}
				if (bHadTail && TailT > Tr.T.back())
				{
					Tr.T.push_back(TailT); Tr.X.push_back(TailX); Tr.Y.push_back(TailY); Tr.F.push_back(TailF);
					Tr.bTail = true;
				}
			}
			for (size_t C = Nw.ChFrom; C < Nw.ChTo; ++C) Tr.Ch.push_back(std::move(Chs[C]));
			if (Nw.bD) Tr.D = Nw.D;
			// (out -1: he is back on the list)
			if (Nw.bOut) Tr.OutT = Nw.Out < 0.0 ? -1.0 : Nw.Out;
			Touch(N);
		}
		// Where each moving man stands now, [n, x, y, f] at t: his provisional last point, in place of the one before.
		for (size_t K = 0; K + 3 < Tails.size(); K += 4)
		{
			const int N = static_cast<int>(Tails[K]);
			if (N < 0 || N >= static_cast<int>(I.TrackOfN.size()))
			{
				OutError = "a delta's tail names track " + std::to_string(N);
				return false;
			}
			TrackData& Tr = I.Tracks[static_cast<size_t>(I.TrackOfN[static_cast<size_t>(N)])];
			if (Tr.bTail && !Tr.T.empty()) { Tr.T.pop_back(); Tr.X.pop_back(); Tr.Y.pop_back(); Tr.F.pop_back(); }
			Tr.T.push_back(Now);
			Tr.X.push_back(Tails[K + 1]);
			Tr.Y.push_back(Tails[K + 2]);
			Tr.F.push_back(static_cast<float>(Tails[K + 3]));
			Tr.bTail = true;
			Touch(N);
		}
		std::vector<int> Grown;     // (in the order first seen)
		for (size_t N = 0; N < Seen.size(); ++N)
		{
			if (Seen[N]) Grown.push_back(static_cast<int>(N));
		}

		// A track goes on the map with its first point, all of this delta's first (a section's loader looks for its
		// key man), then their men, in the order first seen: a man's number never changes, and Find finds him.
		// The men already here take their track's news: his fall, his leaving, his new stances.
		std::vector<char> Placed(Grown.size(), 0);
		size_t NewMen = 0;
		for (size_t G = 0; G < Grown.size(); ++G)
		{
			const int Ix = I.TrackOfN[static_cast<size_t>(Grown[G])];
			const TrackData& Tr = I.Tracks[static_cast<size_t>(Ix)];
			if (!Tr.bPlaced && !Tr.T.empty()) { I.Place(Ix); Placed[G] = 1; ++NewMen; }
		}
		// (room made first: a man is too big to copy about as the list grows)
		I.Men.reserve(I.Men.size() + NewMen);
		std::vector<int> Dirty;     // men whose kill is chosen again
		for (size_t G = 0; G < Grown.size(); ++G)
		{
			const int Ix = I.TrackOfN[static_cast<size_t>(Grown[G])];
			if (Placed[G])
			{
				if (!I.MakeMan(Ix, OutError)) return false;
				const int Mx = I.Tracks[static_cast<size_t>(Ix)].Man;
				if (Mx >= 0) Impl::ResetMan(I.Men[static_cast<size_t>(Mx)]);
				continue;
			}
			const TrackData& Tr = I.Tracks[static_cast<size_t>(Ix)];
			if (Tr.Man < 0) continue;
			Impl::Man& Mn = I.Men[static_cast<size_t>(Tr.Man)];
			if (Mn.Info.Dead != Tr.D) Dirty.push_back(Tr.Man);
			Mn.Info.Dead = Tr.D;
			Mn.Info.Out = Tr.D >= 0.0 ? -1.0 : Tr.OutT;
			// (back on the list after his block left: whole again, as the battle read whole never had him leave)
			if (Mn.Info.Out < 0.0 && Mn.State == Impl::Man::Alive)
			{
				Mn.Fade = 1.f;
				Mn.bVisible = true;
				if (Mn.bLeaving) { Mn.bLeaving = false; if (Mn.Shown == Impl::LeaveShown) Mn.Shown = -1; }
			}
			// (only a change new or not yet settled is made again: most men in a delta have only moved)
			if (Mn.Firm < Tr.Ch.size()) I.MakeStances(Mn);
		}

		// The new rounds: shots booked (each man's planned again with them before his next frame), kills kept.
		if (I.With.bReadRounds)
		{
			for (const RoundRow& Row : Rounds)
			{
				int Struck = -1;
				I.Book(Row, &Struck);
				if (Struck >= 0) Dirty.push_back(Struck);
			}
		}
		for (const int Mx : Dirty) Impl::ChooseKill(I.Men[static_cast<size_t>(Mx)]);
		return true;
	}

	void Crowd::Fire(int Man, double T, Weapon W)
	{
		Inner->Fire(Man, T, W);
	}

	void Crowd::SetEye(double X, double Y, double Z, double Zoom)
	{
		Inner->EyeX = X;
		Inner->EyeY = Y;
		Inner->EyeZ = Z;
		Inner->EyeZoom = Zoom > 0.05 && Zoom < 20.0 ? Zoom : 1.0;
	}

	void Crowd::SetSeats(SeatFn Fn)
	{
		Inner->SeatAt = std::move(Fn);
	}

	bool Crowd::AboardOf(int Man, int& OutBlock, int& OutSeatOn, int& OutSeat) const
	{
		if (Man < 0 || Man >= Count()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (Mn.bRider || Mn.State != Impl::Man::Alive || Mn.Shown < 0 || static_cast<size_t>(Mn.Shown) >= Mn.Stances.size()) return false;
		const Stance& St = Mn.Stances[static_cast<size_t>(Mn.Shown)];
		if (!St.Aboard) return false;
		OutBlock = Mn.Info.Block;
		OutSeatOn = St.SeatOn;
		OutSeat = St.Seat;
		return true;
	}

	void Crowd::Update(double BattleTime)
	{
		Impl& I = *Inner;
		Outbox.clear();
		if (!I.bBuilt) return;
		if (I.Clock >= 0.0 && BattleTime + 1e-9 < I.Clock)
		{
			// Back in time (the replay's scrubber): start over and play up to the time silently, a tenth at a time.
			I.Reset();
			std::vector<Event> Quiet;
			for (double T = 0.0; T < BattleTime; T += 0.1)
			{
				for (Impl::Man& Mn : I.Men) I.Step(Mn, T, I.Clock < 0.0 ? 0.0 : T - I.Clock, Quiet, true);
				I.Clock = T;
			}
		}
		const double Dt = I.Clock < 0.0 ? 0.0 : std::min(0.25, BattleTime - I.Clock);
		++I.Frame;
		const auto StepOne = [&I, BattleTime, Dt](size_t K, std::vector<Event>& Out)
		{
			Impl::Man& Mn = I.Men[K];
			// (thrifty: a far man posed in his turn, the men spread over the frames by their number)
			const int Every = I.bThrift ? Impl::EveryOf(Mn) : 1;
			I.Step(Mn, BattleTime, Dt, Out, false, Every == 1 || (I.Frame + K) % static_cast<uint64_t>(Every) == 0);
			// The level of detail by the eye's distance, with a little give so a man at the edge does not flicker.
			if (!Mn.bVisible) return;
			const double DX = Mn.X - I.EyeX, DY = Mn.Y - I.EyeY, DZ = static_cast<double>(Mn.World.T[2]) - I.EyeZ;
			// (as the picture sees him: a long lens draws a far man as big as a near one, and he is posed and drawn so)
			const double D = std::sqrt(DX * DX + DY * DY + DZ * DZ) / I.EyeZoom;
			Mn.EyeD = static_cast<float>(D);
			const double Give = 1.05;
			Lod L = D < I.NearTo ? Near : D < I.MidTo ? Mid : Far;
			if (Mn.L == Near && L == Mid && D < I.NearTo * Give) L = Near;
			if (Mn.L == Mid && L == Far && D < I.MidTo * Give) L = Mid;
			Mn.L = L;
		};
		const size_t N = I.Men.size();
		if (!I.Run || N < 512)
		{
			for (size_t K = 0; K < N; ++K) StepOne(K, Outbox);
		}
		else
		{
			// Over the cores (SetRun), in stretches of the men's order. A man who starts his fall this frame chooses it
			// by the falls just shown (FallMemory, the crowd's one): he is stepped alone, between the stretches before
			// and after him, so the falls are chosen in the men's order as one after another. (Asked a little wider
			// than Step's own test, never narrower: one stepped alone who does not fall after all is the same man.)
			const auto MayFall = [&I, BattleTime](size_t K)
			{
				const Impl::Man& Mn = I.Men[K];
				return !Mn.bRider && Mn.State == Impl::Man::Alive && ((Mn.Info.Dead >= 0.0 && BattleTime >= Mn.Info.Dead)
					|| (I.HitsNow.bWounded && Mn.Wd >= 0.0 && Mn.Info.WoundStarted < 0.0 && BattleTime >= Mn.Wd)); // (bullet hits: a wound's fall, chosen as a dead man's)
			};
			size_t From = 0;
			while (From < N)
			{
				size_t To = From;
				while (To < N && !MayFall(To)) ++To;
				const size_t Count = To - From;
				if (Count >= 256)
				{
					const int Runs = static_cast<int>(std::min<size_t>(16, Count / 128));
					if (I.RunOut.size() < static_cast<size_t>(Runs)) I.RunOut.resize(static_cast<size_t>(Runs));
					for (int R = 0; R < Runs; ++R) I.RunOut[static_cast<size_t>(R)].clear();
					const size_t Base = From;
					I.Run(Runs, [&I, &StepOne, Base, Count, Runs](int R)
					{
						const size_t A = Base + Count * static_cast<size_t>(R) / static_cast<size_t>(Runs);
						const size_t B = Base + Count * static_cast<size_t>(R + 1) / static_cast<size_t>(Runs);
						std::vector<Event>& Out = I.RunOut[static_cast<size_t>(R)];
						for (size_t K = A; K < B; ++K) StepOne(K, Out);
					});
					for (int R = 0; R < Runs; ++R)
					{
						const std::vector<Event>& Out = I.RunOut[static_cast<size_t>(R)];
						Outbox.insert(Outbox.end(), Out.begin(), Out.end());
					}
				}
				else
				{
					for (size_t K = From; K < To; ++K) StepOne(K, Outbox);
				}
				if (To < N) StepOne(To, Outbox);
				From = To + 1;
			}
		}
		std::stable_sort(Outbox.begin(), Outbox.end(), [](const Event& A, const Event& B) { return A.T < B.T; });
		I.Clock = BattleTime;
	}

	int Crowd::Count() const { return static_cast<int>(Inner->Men.size()); }

	int Crowd::Find(int Block, int K, int J) const
	{
		const auto Found = Inner->ManOf.find({ Block, K, J });
		return Found == Inner->ManOf.end() ? -1 : Found->second;
	}

	const ManInfo& Crowd::Info(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Info; }
	bool Crowd::Visible(int Man) const
	{
		const Impl::Man& M = Inner->Men[static_cast<size_t>(Man)];
		return M.bVisible && !M.bUnderCanvas;
	}
	float Crowd::Fade(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Fade; }
	bool Crowd::Frozen(int Man) const { return Inner->Men[static_cast<size_t>(Man)].State == Impl::Man::Rest; }
	bool Crowd::IsFalling(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		return Mn.State == Impl::Man::Falling || Mn.State == Impl::Man::Settling;
	}
	void Crowd::Serve(int Man, int How)
	{
		if (Man < 0 || Man >= static_cast<int>(Inner->Men.size()))
		{
			return;
		}
		Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		const uint8_t Now = static_cast<uint8_t>(How < 0 ? 0 : How > 2 ? 2 : How);
		if (Mn.Serves == Now)
		{
			return;
		}
		// (his stances taken again now, with his weapon served, as when his first changes come: a battle read whole makes
		// them only once, so nothing later would; and he turns to the one he is in on his next frame)
		Mn.Serves = Now;
		Mn.Stances.clear();
		Mn.Firm = 0;
		Mn.bIdle = false;
		Mn.Shown = -1;
		Mn.ChCursor = 0;
		Inner->MakeStances(Mn);
	}
	void Crowd::SetThrift(bool bOn) { Inner->bThrift = bOn; }
	void Crowd::SetRun(RunFn Run) { Inner->Run = std::move(Run); }
	void Crowd::SetLife(double LifePerBattleSecond) { Inner->Life = std::clamp(LifePerBattleSecond, 0.25, 4.0); }
	void Crowd::SetRunStyles(bool bOn) { Inner->bRunStyles = bOn; }
	void Crowd::SetWorn(bool bOn, double ForcedMonths)
	{
		Inner->bWornLook = bOn;
		Inner->WornForced = ForcedMonths;
	}
	void Crowd::SetWearMix(const WearMix& Wearing)
	{
		Impl& I = *Inner;
		// (held to their spans: the soil and the weather 0 to 1, a forced wear at most 1; a NaN as none)
		const auto Unit = [](float V) { return V >= 0.f ? std::min(V, 1.f) : 0.f; };
		WearMix Now;
		Now.bOn = Wearing.bOn;
		Now.bRough = Wearing.bOn && Wearing.bRough;
		for (int C = 0; C < 3; ++C) Now.Soil[C] = Unit(Wearing.Soil[C]);
		Now.Rain = Unit(Wearing.Rain);
		Now.Snow = Unit(Wearing.Snow);
		Now.W = Wearing.W >= 0.f ? std::min(Wearing.W, 1.f) : -1.f;
		const WearMix& Was = I.Wear;
		const bool bSame = Now.bOn == Was.bOn && Now.bRough == Was.bRough && Now.Soil[0] == Was.Soil[0] && Now.Soil[1] == Was.Soil[1] && Now.Soil[2] == Was.Soil[2]
			&& Now.Rain == Was.Rain && Now.Snow == Was.Snow && Now.W == Was.W;
		const bool bStillOff = !Now.bOn && !Was.bOn;
		I.Wear = Now;
		if (bSame) return;
		// (off before and off now: no look changes, only each man's wear as it reads)
		if (bStillOff)
		{
			for (Impl::Man& M : I.Men) I.SettleWear(M);
			return;
		}
		I.Relook();
	}
	void Crowd::SetLevels(double NearTo, double MidTo)
	{
		Inner->NearTo = std::clamp(NearTo, 1.0, 400.0);
		Inner->MidTo = std::clamp(MidTo, Inner->NearTo, 2000.0);
	}
	int Crowd::PoseEvery(int Man) const { return Inner->bThrift ? Impl::EveryOf(Inner->Men[static_cast<size_t>(Man)]) : 1; }

	bool Crowd::SkinReadsGround(int Man, Lod L) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		return Mn.State == Impl::Man::Rest && !Mn.bSkinned[L] && static_cast<bool>(Inner->GroundAt);
	}
	int Crowd::Unsettle(double X, double Y, double Reach)
	{
		if (!Inner->GroundAt || !(Reach > 0.0))
		{
			return 0;
		}
		// (a body lies within some 2 m of his root, his head or his feet, but a thrown or blown one up to 7 m off it: so
		// first by his root within that, then by where his bones lie, each within the hole's reach and half a metre)
		const double Near = Reach + 8.0, Close = Reach + 0.5;
		int Laid = 0;
		for (Impl::Man& M : Inner->Men)
		{
			if (M.State != Impl::Man::Rest || !M.S || (M.X - X) * (M.X - X) + (M.Y - Y) * (M.Y - Y) > Near * Near)
			{
				continue;
			}
			const size_t NB = M.S->Bones.size();
			bool bOn = false;
			for (size_t B = 0; B < NB && !bOn; ++B)
			{
				const double Bx = M.Frames[B].T[0] - X, By = M.Frames[B].T[1] - Y;
				bOn = Bx * Bx + By * By <= Close * Close;
			}
			if (!bOn)
			{
				continue;
			}
			M.World = WorldOf(M.Place, Inner->GroundAt, &M.Footing);
			M.bFooting = true;
			for (size_t B = 0; B < NB; ++B) M.Frames[B] = Compose(M.World, M.Model[B]);
			for (int L = 0; L < LodCount; ++L) M.bSkinned[L] = false;
			++M.Stamp;
			++Laid;
		}
		return Laid;
	}
	uint32_t Crowd::Stamp(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Stamp; }
	Lod Crowd::LodOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].L; }
	float Crowd::SeenDistance(int Man) const { return Inner->Men[static_cast<size_t>(Man)].EyeD; }
	const Xform* Crowd::FramesOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Frames; }
	const Soldier* Crowd::SoldierOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].S; }

	bool Crowd::ReadPoseBank(const std::string& Text, std::string& OutError)
	{
		J Root;
		if (!ReadJson(Text, Root, OutError)) return false;
		const J& Ps = Root["poses"];
		if (Ps.K != J::Arr || Ps.Size() == 0)
		{
			OutError = "the pose bank has no poses";
			return false;
		}
		std::vector<Impl::BankEntry> Spec;
		std::map<std::string, std::vector<int>> Cycles;
		for (size_t K = 0; K < Ps.Size(); ++K)
		{
			const J& E = Ps.At(K);
			Impl::BankEntry B;
			B.Clip = E["clip"].Text();
			B.Phase = Clamp01(static_cast<float>(E["phase"].Num0()));
			if (B.Clip.empty())
			{
				OutError = "the pose bank's pose " + std::to_string(K) + " names no clip";
				return false;
			}
			Cycles[B.Clip].push_back(static_cast<int>(K));
			Spec.push_back(std::move(B));
		}
		// (its cycles as written, else each clip's poses in their order)
		const J& Cy = Root["cycles"];
		if (Cy.K == J::Obj)
		{
			Cycles.clear();
			for (size_t I = 0; I < Cy.Size(); ++I)
			{
				std::vector<int>& Out = Cycles[Cy.Keys[I]];
				const J& List = Cy.Vals[I];
				for (size_t N = 0; N < List.Size(); ++N)
				{
					const int K = static_cast<int>(List.Number(N, -1.0));
					if (K < 0 || static_cast<size_t>(K) >= Spec.size())
					{
						OutError = "the pose bank's cycle " + Cy.Keys[I] + " names no pose " + std::to_string(K);
						return false;
					}
					Out.push_back(K);
				}
			}
		}
		std::map<std::string, std::string> Instead;
		const J& In = Root["instead"];
		for (size_t I = 0; In.K == J::Obj && I < In.Size(); ++I)
		{
			if (In.Vals[I].K == J::Str) Instead[In.Keys[I]] = In.Vals[I].S;
		}
		Impl& I = *Inner;
		I.BankSpec = std::move(Spec);
		I.BankCycles = std::move(Cycles);
		I.BankInstead = std::move(Instead);
		I.Banks.clear();
		I.BankSets.clear();
		I.BankSetOf.clear();
		I.BodySets.clear();   // (their bakes went with Banks: made again as the men are attached below)
		I.BodySetOf.clear();
		I.BankMs = 0.0;
		I.BankMsBy.clear();
		I.bBankOn = true;
		for (Impl::Man& M : I.Men)
		{
			if (M.S && !M.bRider) I.AttachBank(M);
		}
		return true;
	}

	void Crowd::SetPoseBank(bool bOn)
	{
		// (off, no man shows a banked pose from now; the last Update's stays in his bones until the next)
		Inner->bBankOn = bOn && !Inner->BankSpec.empty();
	}
	bool Crowd::PoseBankOn() const { return Inner->bBankOn; }
	double Crowd::PoseBankMs() const { return Inner->BankMs; }
	const std::map<std::string, double>& Crowd::PoseBankMsByFile() const { return Inner->BankMsBy; }
	int Crowd::BankPoses() const { return 2 * static_cast<int>(Inner->BankSpec.size()); }
	const std::string& Crowd::BankClip(int K) const { return Inner->BankSpec[static_cast<size_t>(K) % Inner->BankSpec.size()].Clip; }
	float Crowd::BankPhase(int K) const { return Inner->BankSpec[static_cast<size_t>(K) % Inner->BankSpec.size()].Phase; }
	bool Crowd::BankRifleDown(int K) const { return static_cast<size_t>(K) >= Inner->BankSpec.size(); }
	int Crowd::BankPose(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		return Inner->bBankOn && Inner->BanksAt(Mn.L) ? Mn.BankK : -1;
	}
	int Crowd::BankKey(int Man) const { return Inner->Men[static_cast<size_t>(Man)].BankKey; }
	int Crowd::BankKeys() const { return static_cast<int>(Inner->BankSets.size()); }
	bool Crowd::BankMesh(int Key, int Pose, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const
	{
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BankSets.size()) return false;
		const Impl::BankSet& Set = I.BankSets[static_cast<size_t>(Key)];
		const Impl::Bank& B = *Set.B;
		if (Pose < 0 || static_cast<size_t>(Pose) >= B.Have.size() || !B.Have[static_cast<size_t>(Pose)]) return false;
		const Finish* Fi = Set.Finish >= 0 ? &Set.S->Finishes[static_cast<size_t>(Set.Finish)] : nullptr;
		const size_t Body = static_cast<size_t>(B.Points), Extra = Fi ? static_cast<size_t>(Fi->Lods[Far].Points) : 0;
		OutPoints.resize((Body + Extra) * 3);
		OutFacings.resize((Body + Extra) * 3);
		const size_t Off = static_cast<size_t>(Pose) * Body * 3;
		std::copy_n(B.At.data() + Off, Body * 3, OutPoints.data());
		std::copy_n(B.Facing.data() + Off, Body * 3, OutFacings.data());
		// (his helmet's finish skinned on the pose's bones, in his own axes as the body)
		if (Fi) SkinMesh(Fi->Lods[Far], &B.Model[static_cast<size_t>(Pose) * static_cast<size_t>(B.Bones)], OutPoints.data() + Body * 3, OutFacings.data() + Body * 3);
		return true;
	}
	const Look& Crowd::BankLook(int Key) const { return Inner->BankSets[static_cast<size_t>(Key)].Lk; }
	const Xform& Crowd::WorldFrame(int Man) const { return Inner->Men[static_cast<size_t>(Man)].World; }

	void Crowd::SetPoseBankMid(bool bOn)
	{
		Impl& I = *Inner;
		if (I.bBankMid == bOn) return;
		I.bBankMid = bOn;
		// (on: each man's mid mesh baked now, if the crowd is built; off, the mid bakes stay, unused)
		for (Impl::Man& M : I.Men)
		{
			if (!M.S || !M.Banked) continue;
			M.BankAt[Mid] = bOn && M.Var[Mid] ? I.BankOf(*M.S, *M.Var[Mid]) : nullptr;
			M.FaceMoved[Mid] = M.BankAt[Mid] && M.FaceAt[Mid] ? I.FaceMovedOf(*M.Var[Mid], *M.FaceAt[Mid]) : nullptr;
			if (bOn && M.Colours) I.AttachBody(M);
		}
	}
	bool Crowd::PoseBankMid() const { return Inner->bBankMid; }
	int Crowd::BankLevel(int Man) const
	{
		return BankPose(Man) >= 0 ? static_cast<int>(Inner->Men[static_cast<size_t>(Man)].L) : -1;
	}
	int Crowd::BodyKey(int Man) const { return Inner->Men[static_cast<size_t>(Man)].BodyKey; }
	int Crowd::BodyKeys() const { return static_cast<int>(Inner->BodySets.size()); }
	int Crowd::BankBodyPoints(int Key, Lod L) const
	{
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BodySets.size()) return 0;
		const Impl::Bank* B = I.BodySets[static_cast<size_t>(Key)].B[L];
		return B ? B->Points : 0;
	}
	bool Crowd::BankBody(int Key, Lod L, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const
	{
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BodySets.size()) return false;
		const Impl::Bank* B = I.BodySets[static_cast<size_t>(Key)].B[L];
		if (!B) return false;
		// (the bake is kept pose-major already: one copy, the poses the file lacks left zero)
		OutPoints = B->At;
		OutFacings = B->Facing;
		return true;
	}
	bool Crowd::BankHas(int Key, int Pose) const
	{
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BodySets.size()) return false;
		const Impl::Bank* B = I.BodySets[static_cast<size_t>(Key)].B[Far];
		return B && Pose >= 0 && static_cast<size_t>(Pose) < B->Have.size() && B->Have[static_cast<size_t>(Pose)];
	}
	const Look& Crowd::BankBodyLook(int Key, Lod L) const
	{
		static const Look None;
		return Key >= 0 && static_cast<size_t>(Key) < Inner->BodySets.size() ? Inner->BodySets[static_cast<size_t>(Key)].Lk[L] : None;
	}
	Crowd::Show Crowd::BankShow(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		Show Out;
		if (BankPose(Man) < 0) return Out;
		Out.A = Mn.ShowA;
		Out.B = Mn.ShowB;
		Out.W = Mn.ShowW;
		return Out;
	}
	Crowd::Show Crowd::BankShowBefore(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		Show Out;
		if (!Inner->bBankOn) return Out;
		Out.A = Mn.WasA;
		Out.B = Mn.WasB;
		Out.W = Mn.WasW;
		return Out;
	}
	Vec3 Crowd::FaceTone(int Man) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		const int F = Mn.Info.Face;
		return F >= 0 && static_cast<size_t>(F) < Mn.S->Faces.size() ? Mn.S->Faces[static_cast<size_t>(F)].Tone : Vec3 { 1.f, 1.f, 1.f };
	}
	int Crowd::BankHeadBone(int Key) const
	{
		const Impl& I = *Inner;
		return Key >= 0 && static_cast<size_t>(Key) < I.BodySets.size() ? I.BodySets[static_cast<size_t>(Key)].HeadBone : -1;
	}
	bool Crowd::BankHead(int Key, int Pose, Xform& Out) const
	{
		const Impl& I = *Inner;
		if (!BankHas(Key, Pose)) return false;
		const Impl::BodySet& Set = I.BodySets[static_cast<size_t>(Key)];
		const Impl::Bank& B = *Set.B[Far];
		if (Set.HeadBone < 0 || Set.HeadBone >= B.Bones) return false;
		Out = B.Model[static_cast<size_t>(Pose) * static_cast<size_t>(B.Bones) + static_cast<size_t>(Set.HeadBone)];
		return true;
	}
	bool Crowd::HelmetFrame(int Man, Xform& Out) const
	{
		const Impl& I = *Inner;
		const Impl::Man& Mn = I.Men[static_cast<size_t>(Man)];
		if (Mn.BodyKey < 0 && Mn.S->Finishes.empty()) return false;
		const int Hb = Mn.BodyKey >= 0 ? I.BodySets[static_cast<size_t>(Mn.BodyKey)].HeadBone : Mn.S->BoneIndex("head");
		if (Hb < 0) return false;
		const Show Sh = BankShow(Man);
		if (Sh.A < 0 || Mn.State == Impl::Man::Rest)
		{
			Out = Mn.Frames[Hb];
			return true;
		}
		// (between the two poses' head frames as the body goes between them: the turn lerped and made square again)
		const Impl::Bank& B = *Mn.Banked;
		const Xform& Fa = B.Model[static_cast<size_t>(Sh.A) * static_cast<size_t>(B.Bones) + static_cast<size_t>(Hb)];
		const Xform& Fb = B.Model[static_cast<size_t>(Sh.B) * static_cast<size_t>(B.Bones) + static_cast<size_t>(Hb)];
		Xform H;
		for (int K = 0; K < 9; ++K) H.R[K] = Fa.R[K] + (Fb.R[K] - Fa.R[K]) * Sh.W;
		for (int K = 0; K < 3; ++K) H.T[K] = Fa.T[K] + (Fb.T[K] - Fa.T[K]) * Sh.W;
		if (Sh.W > 0.f)
		{
			// (Gram-Schmidt on the columns)
			Vec3 C0 { H.R[0], H.R[3], H.R[6] }, C1 { H.R[1], H.R[4], H.R[7] };
			C0 = Unit(C0);
			const float D = Dot(C0, C1);
			C1 = Unit({ C1.X - C0.X * D, C1.Y - C0.Y * D, C1.Z - C0.Z * D });
			const Vec3 C2 = Cross(C0, C1);
			const float R[9] = { C0.X, C1.X, C2.X, C0.Y, C1.Y, C2.Y, C0.Z, C1.Z, C2.Z };
			// (the head frames' shift is X - G * head: keep the head bone's own point where the lerp put it)
			const Vec3 Hd = Mn.S->Bones[static_cast<size_t>(Hb)].Head;
			const Vec3 P { H.R[0] * Hd.X + H.R[1] * Hd.Y + H.R[2] * Hd.Z + H.T[0], H.R[3] * Hd.X + H.R[4] * Hd.Y + H.R[5] * Hd.Z + H.T[1],
				H.R[6] * Hd.X + H.R[7] * Hd.Y + H.R[8] * Hd.Z + H.T[2] };
			std::copy(R, R + 9, H.R);
			H.T[0] = P.X - (R[0] * Hd.X + R[1] * Hd.Y + R[2] * Hd.Z);
			H.T[1] = P.Y - (R[3] * Hd.X + R[4] * Hd.Y + R[5] * Hd.Z);
			H.T[2] = P.Z - (R[6] * Hd.X + R[7] * Hd.Y + R[8] * Hd.Z);
		}
		Out = Compose(Mn.World, H);
		return true;
	}
	int Crowd::HelmetOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Info.Finish; }
	const Look& Crowd::HelmetLook(int Key, int Finish, Lod L) const
	{
		static const Look None;
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BodySets.size()) return None;
		const std::vector<Look>& Hs = I.BodySets[static_cast<size_t>(Key)].Helmets[L];
		return Finish >= 0 && static_cast<size_t>(Finish) < Hs.size() ? Hs[static_cast<size_t>(Finish)] : None;
	}
	bool Crowd::HelmetMesh(int Key, int Finish, Lod L, std::vector<float>& OutPoints, std::vector<float>& OutFacings) const
	{
		const Impl& I = *Inner;
		if (Key < 0 || static_cast<size_t>(Key) >= I.BodySets.size()) return false;
		const Soldier& S = *I.BodySets[static_cast<size_t>(Key)].S;
		if (Finish < 0 || static_cast<size_t>(Finish) >= S.Finishes.size()) return false;
		const Mesh& Fm = S.Finishes[static_cast<size_t>(Finish)].Lods[L];
		OutPoints = Fm.At;
		OutFacings = Fm.Facing;
		return true;
	}
	int Crowd::Month() const { return Inner->MonthNo; }
	const std::string& Crowd::Season() const { return Inner->SeasonName; }
	const std::vector<std::string>& Crowd::StandIns() const { return Inner->StandInLines; }
	std::string Crowd::StandInOf(const std::string& Army) const
	{
		const auto Found = Inner->Armies.find(Army);
		return Found != Inner->Armies.end() ? Found->second.StandIn : std::string();
	}
	const char* FrontAt(double Lat, double Lon)
	{
		// (any longitude, however far past the line, in one step; a place that is no number at all is dressed as no
		// place given, 49 N 0 E: the pre-check's note of 25 November, a loop here would never end on an infinite one)
		if (!std::isfinite(Lat) || !std::isfinite(Lon))
		{
			Lat = 49.0;
			Lon = 0.0;
		}
		Lon = std::remainder(Lon, 360.0);
		if (Lat < 36.0 && Lon > -18.0 && Lon < 40.0) return "desert";
		// (the China coast and the country behind it. Formosa, the Pescadores and the north of Luzon, east of 119.5 and
		// south of 26, are the Pacific's: the Japan and Pacific thread's finding of 25 November)
		if (Lon >= 95.0 && Lon < 125.0 && Lat > 18.0 && !(Lon >= 119.5 && Lat < 26.0)) return "china";
		// (the Pacific runs on past the date line: Hawaii, Midway, the Aleutians and Alaska, out to the sea off the
		// Americas' own coast. Before, a place east of the line was read as a negative longitude and dressed for the west.)
		if (Lon >= 95.0 || Lon < -125.0 || (Lat < 20.0 && Lon > 60.0)) return "pacific";
		if (Lat >= 36.0 && Lat < 47.0 && Lon >= 6.0 && Lon < 24.0) return "south";
		if (Lon >= 15.0) return "east";
		return "west";
	}

	const std::string& Crowd::FrontName() const { return Inner->Front; }

	void Crowd::SetFallKind(int Man, const std::string& Kind)
	{
		if (Man >= 0 && Man < Count()) Inner->Men[static_cast<size_t>(Man)].FallKindWanted = Kind;
	}

	uint32_t Crowd::PaintColour(const std::string& Army, int Paint) const
	{
		const auto Found = Inner->Armies.find(Army);
		if (Found == Inner->Armies.end() || Paint < 0 || Paint >= 16) return 0x808080;
		return Found->second.Paint[Paint];
	}

	int Crowd::PointsOf(int Man, Lod L) const
	{
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		int N = Mn.Var[L]->M.Points;
		if (Mn.Info.Finish >= 0) N += Mn.S->Finishes[static_cast<size_t>(Mn.Info.Finish)].Lods[L].Points;
		return N;
	}

	const Look& Crowd::LookOf(int Man, Lod L)
	{
		Impl& I = *Inner;
		Impl::Man& Mn = I.Men[static_cast<size_t>(Man)];
		Look& Lk = Mn.Looks[L];
		if (Mn.bLook[L]) return Lk;
		Mn.bLook[L] = true;
		const Soldier& S = *Mn.S;
		const Finish* Fi = Mn.Info.Finish >= 0 ? &S.Finishes[static_cast<size_t>(Mn.Info.Finish)] : nullptr;
		const Face* Fc = Mn.Info.Face >= 0 && Mn.Info.Face < static_cast<int>(S.Faces.size()) ? &S.Faces[static_cast<size_t>(Mn.Info.Face)] : nullptr;
		Impl::MakeLook(S, *Mn.Var[L], Fi, *Mn.Colours, Fc, L, I.Wear, Mn.Info.Wear, Lk);
		I.AgeLook(Mn, L, Lk);
		return Lk;
	}

	int Crowd::Skin(int Man, Lod L, float* OutAt, float* OutFacing)
	{
		// (only this man's own state is read and written, and the tables made in Build only read: different men may be
		// skinned at once, from different threads; a body's first skin at a level also reads the ground, SkinReadsGround)
		Impl& I = *Inner;
		Impl::Man& Mn = I.Men[static_cast<size_t>(Man)];
		const Variant& V = *Mn.Var[L];
		const Finish* Fi = Mn.Info.Finish >= 0 ? &Mn.S->Finishes[static_cast<size_t>(Mn.Info.Finish)] : nullptr;
		const int Body = V.M.Points;
		const int Total = Body + (Fi ? Fi->Lods[L].Points : 0);
		const bool bRest = Mn.State == Impl::Man::Rest;
		if (bRest && Mn.bSkinned[L])
		{
			std::memcpy(OutAt, Mn.RestAt[L].data(), sizeof(float) * static_cast<size_t>(Total) * 3);
			if (OutFacing) std::memcpy(OutFacing, Mn.RestFacing[L].data(), sizeof(float) * static_cast<size_t>(Total) * 3);
			return Total;
		}
		float* At = OutAt;
		float* Fa = OutFacing;
		if (bRest)
		{
			Mn.RestAt[L].resize(static_cast<size_t>(Total) * 3);
			Mn.RestFacing[L].resize(static_cast<size_t>(Total) * 3);
			At = Mn.RestAt[L].data();
			Fa = Mn.RestFacing[L].data();
		}
		if (I.bBankOn && Mn.BankK >= 0 && !bRest && Mn.BankAt[L] && I.BanksAt(L))
		{
			Impl::SkinBanked(Mn, L, At, Fa);
		}
		else
		{
			const FaceRest& R = *Mn.FaceAt[L];
			SkinMesh(V.M, Mn.Frames, At, Fa, R.At.data(), R.Facing.data());
		}
		if (Mn.bGore && !Mn.S->Joins[L].Point.empty())
		{
			// Effects (R2): a gore twin's joined skin (the art director, 18:34Z: the caps hidden in him till his limb
			// goes): the points at each whole joint skinned with his own weights, so no bent joint cracks open on its
			// caps; at a cut whose bone has left its joint, the twin's own hard weights, as SkinMesh laid them.
			const Soldier& Tw = *Mn.S;
			uint32_t Off = 0;
			for (size_t K = 0; K < Tw.Cuts.size(); ++K)
			{
				const int B = Tw.Cuts[K].BoneIx;
				const Vec3 Head = Tw.Bones[static_cast<size_t>(B)].Head;
				const Vec3 A = Mn.Frames[B].Point(Head), Q = Mn.Frames[Tw.Bones[static_cast<size_t>(B)].Parent].Point(Head);
				const float Dx = A.X - Q.X, Dy = A.Y - Q.Y, Dz = A.Z - Q.Z;
				if (Dx * Dx + Dy * Dy + Dz * Dz > 1e-4f) Off |= 1u << K;
			}
			const Soldier::Joined& Jn = Tw.Joins[L];
			const FaceRest& R = *Mn.FaceAt[L];
			for (size_t K = 0; K < Jn.Point.size(); ++K)
			{
				if ((Jn.Mask[K] & Off) != 0u || Jn.Point[K] >= V.ToCompact.size()) continue;
				const int P = V.ToCompact[Jn.Point[K]];
				if (P < 0) continue;
				const size_t O = static_cast<size_t>(P) * 3;
				SkinPoint(0, R.At.data() + O, R.Facing.data() + O, &Jn.Part[K], &Jn.Part2[K], &Jn.W2[K], Mn.Frames, At + O, Fa ? Fa + O : nullptr);
			}
		}
		if (Fi) SkinMesh(Fi->Lods[L], Mn.Frames, At + static_cast<size_t>(Body) * 3, Fa ? Fa + static_cast<size_t>(Body) * 3 : nullptr);
		if (bRest)
		{
			// A body is laid on the ground once: where the fall left a part of him a little under the ground's face (the
			// falls are captured on a flat floor; the kit on his belt and his chest can dip some centimetres), it is
			// pressed up onto it, so nothing of him shows through the ground and he lies on it, not in it.
			if (I.GroundAt)
			{
				for (int P = 0; P < Total; ++P)
				{
					float* Pt = At + static_cast<size_t>(P) * 3;
					const float G = I.GroundAt(Pt[0], Pt[1]).Height + 0.004f;
					if (Pt[2] < G) Pt[2] = G;
				}
			}
			Mn.bSkinned[L] = true;
			std::memcpy(OutAt, At, sizeof(float) * static_cast<size_t>(Total) * 3);
			if (OutFacing) std::memcpy(OutFacing, Fa, sizeof(float) * static_cast<size_t>(Total) * 3);
		}
		return Total;
	}

	bool Crowd::SocketAt(int Man, const std::string& SocketName, Vec3& OutAt, Vec3& OutDir) const
	{
		if (Man < 0 || Man >= Count()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		const int Ix = Mn.S->SocketIndex(SocketName);
		if (Ix < 0) return false;
		Inner->PointOf(Mn, Ix, -1, false, OutAt, OutDir);
		return true;
	}

	bool Crowd::Muzzle(int Man, Vec3& OutAt, Vec3& OutDir) const
	{
		return SocketAt(Man, "socket_muzzle", OutAt, OutDir);
	}

	bool Crowd::PlaceOf(int Man, double& OutX, double& OutY, double& OutZ, float& OutFacing) const
	{
		if (Man < 0 || Man >= Count()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		OutX = Mn.X;
		OutY = Mn.Y;
		OutZ = Mn.bRider ? static_cast<double>(Mn.World.T[2]) : Inner->GroundAt ? Inner->GroundAt(Mn.X, Mn.Y).Height : 0.0;
		OutFacing = Mn.F;
		return true;
	}

	bool Crowd::PlaceAsPosed(int Man, double& OutX, double& OutY, double& OutZ, float& OutFacing) const
	{
		if (Man < 0 || Man >= Count()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (!Mn.bRider && !Mn.bFooting) return PlaceOf(Man, OutX, OutY, OutZ, OutFacing);
		OutX = Mn.X;
		OutY = Mn.Y;
		OutZ = Mn.bRider ? static_cast<double>(Mn.World.T[2]) : static_cast<double>(Mn.Footing);
		OutFacing = Mn.F;
		return true;
	}

	std::string Crowd::RiderFileOf(const std::string& Army, std::string& OutRow) const
	{
		const Impl& I = *Inner;
		const auto Has = [&I](const std::string& F) { return I.Soldiers.find(F) != I.Soldiers.end(); };
		auto Colours = I.Armies.find(Army + "@" + I.Front);
		if (Colours == I.Armies.end()) Colours = I.Armies.find(Army);
		OutRow = Colours != I.Armies.end() ? Colours->first : std::string();
		std::string File = Army;
		if (!Has(File))
		{
			const auto Row = I.Armies.find(Army);
			File = Row != I.Armies.end() && !Row->second.StandIn.empty() && Has(Row->second.StandIn) ? Row->second.StandIn : std::string("POL");
		}
		if (!Has(File)) return std::string();
		// (his dress for the front and the season, as MakeMan's; in its own colours where armies.json has a row for it)
		const std::string Plain = File;
		if (I.Front == "desert" && Has(File + "_desert")) File += "_desert";
		else if (I.SeasonName == "winter" && Has(File + "_winter")) File += "_winter";
		else if (I.SeasonName == "winter" && Has(File + "_snow")) File += "_snow";
		if (File != Plain && I.Armies.find(File) != I.Armies.end()) OutRow = File;
		return File;
	}

	Crowd::Dress Crowd::PilotDress(const std::string& Nation) const
	{
		Dress D;
		const std::string Crew = Nation == "USA" ? "USA_tanker" : Nation == "GER" ? "GER_panzer" : std::string();
		if (!Crew.empty() && SoldierFile(Crew))
		{
			// (his tankers' file: his own head and kit, as it is drawn)
			D.File = Crew;
			D.bTankers = true;
			return D;
		}
		D.File = RiderFileOf(Nation, D.Row);
		D.bBareHead = true;
		D.bNoPack = true;
		return D;
	}

	Crowd::Dress Crowd::CrewDress(const std::string& Nation) const
	{
		Dress D;
		const std::string Crew = Nation == "USA" ? "USA_tanker" : Nation == "GER" ? "GER_panzer" : std::string();
		D.bTankers = !Crew.empty() && SoldierFile(Crew);
		if (D.bTankers)
		{
			D.File = Crew;
			return D;
		}
		// (his army's own infantry figure, his helmet on, his pack and webbing off)
		D.File = SoldierFile(Nation) ? Nation : std::string();
		D.bNoPack = true;
		return D;
	}

	int Crowd::AddRider(const std::string& FileArmy, const std::string& Army, uint64_t Seed, const std::string& Row, bool bBareHead, bool bNoPack)
	{
		Impl& I = *Inner;
		const auto Sol = I.Soldiers.find(FileArmy);
		if (!I.bBuilt || Sol == I.Soldiers.end() || !Sol->second) return -1;
		static const ArmyColours Plain;
		auto Colours = I.Armies.find(Row.empty() ? Army : Row);
		if (Colours == I.Armies.end()) Colours = I.Armies.find("_default"); // (never unpainted: MakeMan)
		// (made in place: a man is some 9 KB)
		Impl::Man& Mn = I.Men.emplace_back();
		Mn.bRider = true;
		Mn.Track = -1;
		Mn.S = Sol->second.get();
		Mn.Colours = Colours != I.Armies.end() ? &Colours->second : &Plain;
		// (uniforms that age: his army's months, as its men in the field wear them; the pre-check, 23 November)
		if (I.bWornLook)
		{
			const auto Worn = I.ArmyWorn.find(Army);
			const double Months = I.WornForced >= 0.0 ? I.WornForced : Worn != I.ArmyWorn.end() ? Worn->second : 0.0;
			if (Months > 0.0)
			{
				Mn.WearMonths = Months; // (and his wear for the worn look: SettleWear)
				Mn.Fresh = Mn.Colours;
				Mn.WornA = static_cast<float>(std::clamp(Months / 18.0, 0.0, 1.0));
				const int Garment = Unit01(Seed ^ 0xA6ED1ull) < 0.45 * Mn.WornA ? (Unit01(Seed ^ 0xA6ED2ull) < 0.65 ? 1 : 0) : -1;
				Mn.Colours = &I.AgedRow(*Mn.Fresh, static_cast<int>(std::lround(Mn.WornA * 20.f)), Garment);
			}
		}
		I.SettleWear(Mn);
		Mn.Info.Block = -1;
		Mn.Info.K = -1;
		Mn.Info.Role = "commander";
		Mn.Info.Army = Army;
		Mn.Info.FileArmy = FileArmy;
		const Soldier& S = *Mn.S;
		Mn.Info.Face = S.Faces.empty() ? 0 : static_cast<int>(Mix(Seed ^ 0xFACEull) % 8ull) % static_cast<int>(S.Faces.size());
		// (his own head: the tank helmet and goggles, the side cap and headphones; no steel helmet's finish over them.
		// Bareheaded, not even his file's own: a downed pilot in his army's infantry file, who wears no steel helmet)
		Mn.Info.Finish = -1;
		Mn.bBareHead = bBareHead;
		// (Effects (R2): no pack, his file's pack and webbing left off where it marks them: a man out of his machine)
		Mn.bNoPack = bNoPack;
		Mn.PhaseSeed = static_cast<float>(Unit01(Seed ^ 0x57E9ull));
		Mn.StylePick = static_cast<float>(Unit01(Seed ^ 0x5717E5ull));
		Mn.bVisible = false;
		RestPose(Mn.Now);
		for (int L = 0; L < LodCount; ++L)
		{
			Mn.FaceAt[L] = &I.FaceRestOf(Mn.S, I.CutsOf(Mn, L), L, Mn.Info.Face);
			Mn.Var[L] = &I.VariantOf(Mn.S, I.CutsOf(Mn, L), L);
		}
		return static_cast<int>(I.Men.size()) - 1;
	}

	void Crowd::SetRide(int Man, const Ride& R)
	{
		if (Man < 0 || Man >= Count()) return;
		Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (Mn.bRider) Mn.RideNow = R;
	}

	bool Crowd::IsRider(int Man) const
	{
		return Man >= 0 && Man < Count() && Inner->Men[static_cast<size_t>(Man)].bRider;
	}

	bool Crowd::AboardOf(int Man, int& OutBlock, int& OutSeatOn, int& OutSeat, double& OutDown) const
	{
		OutBlock = OutSeatOn = OutSeat = -1;
		OutDown = -1.0;
		if (Man < 0 || Man >= Count()) return false;
		const Impl& I = *Inner;
		const Impl::Man& M = I.Men[static_cast<size_t>(Man)];
		if (M.bRider || M.State != Impl::Man::Alive || M.Shown < 0 || static_cast<size_t>(M.Shown) >= M.Stances.size()) return false;
		const Stance& St = M.Stances[static_cast<size_t>(M.Shown)];
		if (!St.bUnderCanvas || St.SeatOn < 0 || St.Seat < 0) return false;
		OutBlock = M.Info.Block;
		OutSeatOn = St.SeatOn;
		OutSeat = St.Seat;
		// (on his own clock, a little behind the battle's: his Delay)
		if (St.OffAt >= 0.0 && I.Clock - M.Info.Delay >= St.OffAt) OutDown = I.Clock - M.Info.Delay - St.OffAt;
		return true;
	}

	int Crowd::AddRiderLike(int Man)
	{
		Impl& I = *Inner;
		if (!I.bBuilt || Man < 0 || Man >= Count()) return -1;
		// (his look taken before the crowd grows: a man is made in place, and the men may move)
		const Impl::Man& From = I.Men[static_cast<size_t>(Man)];
		if (From.bRider || !From.S || !From.Colours) return -1;
		const Soldier* S = From.S;
		const ArmyColours* Colours = From.Colours;
		const ArmyColours* Fresh = From.Fresh;
		const float WornA = From.WornA, PhaseSeed = From.PhaseSeed, StylePick = From.StylePick;
		const double WearMonths = From.WearMonths;
		const int Track = From.Track;
		ManInfo Info = From.Info;
		const Variant* Var[LodCount];
		const FaceRest* FaceAt[LodCount];
		for (int L = 0; L < LodCount; ++L)
		{
			Var[L] = From.Var[L];
			FaceAt[L] = From.FaceAt[L];
		}
		Impl::Man& Mn = I.Men.emplace_back();
		Mn.bRider = true;
		Mn.Track = -1;
		Mn.LookTrack = Track;
		Mn.S = S;
		Mn.Colours = Colours;
		Mn.Fresh = Fresh;
		Mn.WornA = WornA;
		Mn.WearMonths = WearMonths; // (his wear, ManInfo::Wear, comes with his Info)
		Mn.Info = std::move(Info);
		Mn.Info.Block = -1;
		Mn.Info.K = -1;
		Mn.Info.Role = "rider";
		Mn.Info.Dead = Mn.Info.Out = -1.0;
		Mn.Info.Delay = 0.0;
		Mn.PhaseSeed = PhaseSeed;
		Mn.StylePick = StylePick;
		Mn.bVisible = false;
		RestPose(Mn.Now);
		for (int L = 0; L < LodCount; ++L)
		{
			Mn.Var[L] = Var[L];
			Mn.FaceAt[L] = FaceAt[L];
		}
		return static_cast<int>(I.Men.size()) - 1;
	}

	const Soldier* Crowd::SoldierFile(const std::string& FileArmy) const
	{
		const auto Found = Inner->Soldiers.find(FileArmy);
		return Found == Inner->Soldiers.end() ? nullptr : Found->second.get();
	}

	float Crowd::LieOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Lie; }

	Playing Crowd::PlayingOf(int Man) const
	{
		Playing P;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		P.Speed = Mn.V;
		P.Lie = Mn.Lie;
		P.bShooting = Mn.bShooting;
		if (Mn.State != Impl::Man::Alive)
		{
			P.Clip = Mn.Info.FallClip;
			return P;
		}
		if (Mn.Cur.Clip < 0) return P;
		const Clip& C = Mn.S->Clips[static_cast<size_t>(Mn.Cur.Clip)];
		P.Clip = C.Name;
		P.Time = Mn.Cur.LastT;
		P.bGait = Mn.Cur.How == Segment::Gait;
		P.Rate = Mn.Cur.How == Segment::Gait ? Mn.Cur.Rate : Mn.Cur.How == Segment::Loop ? Mn.Cur.Rate : Mn.Cur.How == Segment::Once ? static_cast<float>((Mn.Cur.To - Mn.Cur.From) / Mn.Cur.Dur) : 0.f;
		P.bCapped = Mn.Cur.bCapped;
		P.bBlending = Mn.bPrev;
		P.bRifleDown = Mn.Cur.bRifleDown;
		return P;
	}

	bool Crowd::PropAt(int Man, const std::string& Track, std::vector<float>& Out) const
	{
		if (Man < 0 || static_cast<size_t>(Man) >= Inner->Men.size()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (!Mn.S) return false;
		// (a rider's clip and its time are his seat's, SetRide; a man on his feet shows his own)
		int ClipIx = Mn.Cur.Clip;
		float At = Mn.Cur.LastT;
		if (Mn.bRider)
		{
			if (!Mn.RideNow.bShown) return false;
			ClipIx = Mn.RideNow.Clip;
			At = Mn.RideNow.Time;
		}
		else if (Mn.State != Impl::Man::Alive || At < 0.f) return false;
		if (ClipIx < 0 || static_cast<size_t>(ClipIx) >= Mn.S->Clips.size()) return false;
		const Clip& C = Mn.S->Clips[static_cast<size_t>(ClipIx)];
		const Clip::PropKeys* P = C.PropOf(Track);
		if (!P) return false;
		Out.resize(P->Fields.size());
		P->Sample(At, C.bLoop, C.Long, Out.data());
		return true;
	}

	bool Crowd::PropFrame(int Man, const std::string& Track, Xform& Out) const
	{
		if (Man < 0 || static_cast<size_t>(Man) >= Inner->Men.size()) return false;
		const Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		if (!Mn.S) return false;
		const int ClipIx = Mn.bRider ? Mn.RideNow.Clip : Mn.Cur.Clip;
		if (ClipIx < 0 || static_cast<size_t>(ClipIx) >= Mn.S->Clips.size()) return false;
		const Clip::PropKeys* P = Mn.S->Clips[static_cast<size_t>(ClipIx)].PropOf(Track);
		std::vector<float> Keys;
		if (!P || !PropAt(Man, Track, Keys)) return false;
		// (its fields by name: where it is, how it is turned, and whether it is in his hands at all)
		float At[3] = { 0.f, 0.f, 0.f };
		Quat Q;
		bool bPlaced = false;
		for (size_t F = 0; F < P->Fields.size() && F < Keys.size(); ++F)
		{
			const std::string& Name = P->Fields[F];
			if (Name == "x") { At[0] = Keys[F]; bPlaced = true; }
			else if (Name == "y") At[1] = Keys[F];
			else if (Name == "z") At[2] = Keys[F];
			else if (Name == "qw") Q.W = Keys[F];
			else if (Name == "qx") Q.X = Keys[F];
			else if (Name == "qy") Q.Y = Keys[F];
			else if (Name == "qz") Q.Z = Keys[F];
			else if (Name == "shown" && Keys[F] < 0.5f) return false;
		}
		if (!bPlaced) return false;
		Xform Own;
		QMat(QNormal(Q), Own.R);
		Own.T[0] = At[0];
		Own.T[1] = At[1];
		Own.T[2] = At[2];
		Out = Compose(Mn.bRider ? Mn.RideNow.Seat : Mn.World, Own);
		return true;
	}
}
