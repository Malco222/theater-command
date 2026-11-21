#include "Battle/LandMen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
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
		private:
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

		uint32_t Hex(const std::string& S, uint32_t Default)
		{
			if (S.size() < 6) return Default;
			char* End = nullptr;
			const unsigned long V = std::strtoul(S.substr(S.size() - 6).c_str(), &End, 16);
			return End && *End == 0 ? static_cast<uint32_t>(V) : Default;
		}

		int MonthOf(const std::string& YyyyMm)
		{
			// "1944-07" -> 1944 * 12 + 6.
			if (YyyyMm.size() < 7) return 0;
			return std::atoi(YyyyMm.substr(0, 4).c_str()) * 12 + std::atoi(YyyyMm.substr(5, 2).c_str()) - 1;
		}

		void ReadMesh(const J& Src, Mesh& Out, bool bInk)
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
	bool Soldier::Load(const std::string& Text, std::string& OutError)
	{
		J Root;
		if (!ReadJson(Text, Root, OutError)) return false;
		Name = Root["name"].Text();
		Weapon = Root["weapon"].Text("rifle");

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

		const char* LodNames[LodCount] = { "near", "mid", "far" };
		for (int L = 0; L < LodCount; ++L)
		{
			const J& Src = Root[LodNames[L]];
			if (Src.IsNull())
			{
				// (An old file with no mid: the far stands in.)
				if (L == Mid && !Root["far"].IsNull()) ReadMesh(Root["far"], Meshes[L], false);
				continue;
			}
			ReadMesh(Src, Meshes[L], false);
			const J& Ht = Root["helmet_tris"][LodNames[L]];
			HelmetFirst[L] = static_cast<int>(Ht.Number(0, 0.0));
			HelmetCount[L] = static_cast<int>(Ht.Number(1, 0.0));
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
				ReadMesh(H[LodNames[L]], Fi.Lods[L], true);
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
		Deaths.clear();
		const J& Ds = Root["deaths"];
		for (size_t I = 0; I < Ds.Size() && Ds.K == J::Obj; ++I)
		{
			const J& ByKind = Ds.Vals[I];
			for (size_t K = 0; K < ByKind.Size() && ByKind.K == J::Obj; ++K)
			{
				const J& List = ByKind.Vals[K];
				for (size_t N = 0; N < List.Size(); ++N)
				{
					const std::string Clip = List.At(N).Text();
					if (ClipIndex(Clip) >= 0) Deaths[Ds.Keys[I]][ByKind.Keys[K]].push_back(Clip);
				}
			}
		}
		return true;
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

	Xform WorldOf(const Placing& P, const GroundFn& GroundAt)
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
		W.T[2] = G.Height - RP.Z;
		return W;
	}

	void SkinMesh(const Mesh& M, const Xform* Frames, float* OutAt, float* OutFacing, const float* RestAt, const float* RestFacing)
	{
		const float* At = RestAt ? RestAt : M.At.data();
		const float* Fa = RestFacing ? RestFacing : M.Facing.data();
		const size_t N = static_cast<size_t>(M.Points);
		const uint8_t* P1 = M.Part.data();
		const uint8_t* P2 = M.Part2.data();
		const float* W2 = M.W2.data();
		for (size_t I = 0; I < N; ++I)
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
			if (!OutFacing) continue;
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
			int B = 0, K = 0;
			std::string Role;
			bool bFo = false;
			std::vector<double> T, X, Y;
			std::vector<float> F;
			struct Change
			{
				double T = 0.0;
				std::string Pose, Cover;
				bool bHidden = false;
			};
			std::vector<Change> Ch;
			double D = -1.0, OutT = -1.0;

			/** Where he was at time Tm (lerped between kept points, the facing turned the short way) and his speed. */
			void At(double Tm, size_t& Cursor, double& OX, double& OY, float& OF, float& OV) const
			{
				const size_t N = T.size();
				if (N == 0) { OX = OY = 0.0; OF = 0.f; OV = 0.f; return; }
				if (Cursor >= N || T[Cursor] > Tm) Cursor = 0;
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
		};

		struct ArmyColours
		{
			uint32_t Paint[16] = {};
			std::string StandIn;
			bool bFileFinishes = true;      // pick from the soldier file's helmet finishes (false: his own helmet, our paint)
		};

		struct Variant
		{
			// A soldier's mesh at one level, with his own helmet or without it (a finish drawn instead): the points only
			// that helmet uses are left out, so they are neither skinned nor sent.
			Mesh M;
			std::vector<int> ToCompact;     // the file's point -> this mesh's, or -1
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

	struct Crowd::Impl
	{
		struct Man
		{
			ManInfo Info;
			int Track = -1;
			const Soldier* S = nullptr;
			const ArmyColours* Colours = nullptr;
			std::vector<Stance> Stances;    // one per change of the track
			std::vector<Shot> Shots;
			bool bShotsDirty = false;
			size_t ShotCur = 0;
			float PhaseSeed = 0.f;
			std::string FallKindWanted;
			// The kill: what struck him, from where.
			double KillT = -1.0;
			int KillBy = -1, KillK = -1;
			Weapon KillW = Weapon::Other;
			std::string KillWeapon;
			double KillX = 0.0, KillY = 0.0;

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
			double Last = -1.0;
			uint32_t Stamp = 0;
			Lod L = Far;
			Pose Now;
			Xform Model[MaxBones];          // the bones in his own axes
			Xform World;
			Xform Frames[MaxBones];         // the bones on the field
			Placing Place;
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
			// At rest: the world frames he froze with, and each level skinned once on first asking.
			bool bSkinned[LodCount] = { false, false, false };
			std::vector<float> RestAt[LodCount], RestFacing[LodCount];
			// Looks.
			bool bLook[LodCount] = { false, false, false };
			Look Looks[LodCount];
		};

		std::map<std::string, std::shared_ptr<const Soldier>> Soldiers;
		std::map<std::string, ArmyColours> Armies;
		std::vector<TrackData> Tracks;
		std::map<std::pair<int, int>, int> TrackOf;
		std::vector<Man> Men;
		std::map<std::pair<int, int>, int> ManOf;
		GroundFn GroundAt;
		uint64_t Seed = 1;
		int MonthNo = 0;
		std::string SeasonName = "summer", Front = "west";
		double EyeX = 1e9, EyeY = 1e9, EyeZ = 1e9;
		double Clock = -1.0;  // the battle time of the last Update
		bool bBuilt = false;
		FallMemory Falls;
		std::string RecordingText;  // kept for a replay from the start (Update going back)
		Crowd::Options With;
		std::map<std::tuple<const Soldier*, int, int>, Variant> Variants;           // soldier, own helmet, level
		std::map<std::tuple<const Soldier*, int, int, int>, FaceRest> FaceRests;   // soldier, own helmet, level, face

		const Variant& VariantOf(const Soldier* S, bool bOwnHelmet, int L)
		{
			const auto Key = std::make_tuple(S, bOwnHelmet ? 1 : 0, L);
			auto Found = Variants.find(Key);
			if (Found != Variants.end()) return Found->second;
			Variant& V = Variants[Key];
			const Mesh& Full = S->Meshes[L];
			const size_t N = static_cast<size_t>(Full.Points);
			std::vector<bool> Keep(N, true);
			size_t TriFirst = Full.Triangles.size() / 3, TriCount = 0;
			if (!bOwnHelmet && S->HelmetCount[L] > 0)
			{
				TriFirst = static_cast<size_t>(S->HelmetFirst[L]);
				TriCount = static_cast<size_t>(S->HelmetCount[L]);
				// The points only his own helmet's triangles use (in the snow files a few are shared with the hood: kept).
				std::vector<uint8_t> Use(N, 0);
				for (size_t T = 0; T < Full.Triangles.size() / 3; ++T)
				{
					const bool bHelmet = T >= TriFirst && T < TriFirst + TriCount;
					for (int C = 0; C < 3; ++C) Use[Full.Triangles[T * 3 + static_cast<size_t>(C)]] |= bHelmet ? 1 : 2;
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
			}
			for (size_t T = 0; T < Full.Triangles.size() / 3; ++T)
			{
				if (T >= TriFirst && T < TriFirst + TriCount) continue;
				for (int C = 0; C < 3; ++C) M.Triangles.push_back(static_cast<uint32_t>(V.ToCompact[Full.Triangles[T * 3 + static_cast<size_t>(C)]]));
			}
			return V;
		}

		const FaceRest& FaceRestOf(const Soldier* S, bool bOwnHelmet, int L, int FaceIx)
		{
			const auto Key = std::make_tuple(S, bOwnHelmet ? 1 : 0, L, FaceIx);
			auto Found = FaceRests.find(Key);
			if (Found != FaceRests.end()) return Found->second;
			const Variant& V = VariantOf(S, bOwnHelmet, L);
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

		bool OwnHelmet(const Man& M) const { return M.Info.Finish < 0; }

		// ---------------------------------------------------------------------------------------------------------
		// Poses of the rules, drawn.
		// ---------------------------------------------------------------------------------------------------------
		Stance StanceOf(const Soldier& S, const std::string& Role, const std::string& Pose, bool bMoving) const
		{
			Stance St;
			St.Pose = Pose;
			auto Use = [&](const char* Clip, Stance::How How, Class Cl) {
				St.Clip = S.ClipIndex(Clip);
				St.Play = How;
				St.Cl = Cl;
			};
			const bool bGunner = S.Weapon == "lmg";
			if (Pose == "walk" || Pose == "run" || Pose == "advance" || Pose == "creep" || Pose == "flee") Use(Pose.c_str(), Stance::Gait, Up);
			else if (Pose == "crawl") Use("crawl", Stance::Gait, Prone);
			else if (Pose == "idle") Use("idle", Stance::Loop, Up);
			else if (Pose == "cower") Use("cower", Stance::Loop, Kneel);
			else if (Pose == "fire_stand" || Pose == "throw_grenade") { Use("fire_stand", Stance::Hold, Up); St.bFiring = true; }
			else if (Pose == "fire_kneel" || Pose == "wait") { Use("fire_kneel", Stance::Hold, Kneel); St.bFiring = true; }
			else if (Pose == "fire_prone" || Pose == "hide") { Use("fire_prone", Stance::Hold, Prone); St.bFiring = true; }
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
			}
			else if (Role == "mortar" && (Pose == "fire" || Pose == "deployed"))
			{
				// A support team's mortar, drawn with riflemen until it has a model: each man kneels by his tube, rifle at
				// the ready, as wait holds. A proper team needs the tube on its baseplate and bipod (a prop at his knee),
				// a clip of the loader dropping the bomb and ducking with his hands over his ears, the gunner laying the
				// sight between rounds, and the bearers with their ammunition cases.
				Use("fire_kneel", Stance::Hold, Kneel);
			}
			else if (Pose == "fire" || Pose == "deployed") { Use("fire_kneel", Stance::Hold, Kneel); St.bFiring = true; }
			else if (Pose == "mounted" || Pose == "ride")
			{
				// Sitting in a lorry or on a horse: the soldier has no seated clip yet, so he kneels where his seat is
				// (the rules put him there), which at least keeps him low in the lorry's bed. A seated clip is wanted.
				Use("fire_kneel", Stance::Hold, Kneel);
			}
			else if (Pose == "horses") Use(bMoving ? "walk" : "idle", bMoving ? Stance::Gait : Stance::Loop, Up);
			else Use("idle", Stance::Loop, Up);
			if (St.Clip < 0) { St.Clip = S.ClipIndex("idle"); St.Play = Stance::Loop; }
			if (St.Clip < 0) St.Clip = 0;
			return St;
		}

		static Segment::Kind KindOf(Stance::How H)
		{
			return H == Stance::Gait ? Segment::Gait : H == Stance::Loop ? Segment::Loop : Segment::Hold;
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
			G.Rate = G.How == Segment::Loop ? M.Info.Jitter : 1.f;
			G.Phase = M.PhaseSeed;
			if (bClipShotsRef && *bClipShotsRef && St.bFiring && G.How == Segment::Hold)
			{
				// The test's switch: the firing poses loop their own clips again, shots and all.
				G.How = Segment::Loop;
				G.Rate = M.Info.Jitter;
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
			// A new stance from the rules: turn to it as a man would. Kneeling, going prone and getting up have their own
			// clips (kneel_down, go_prone, stand_up, go_prone backwards), squeezed to fit before he must move, fire or
			// change again (the squad brain changes pose every second or two in a bound); within a class it is a fifth
			// of a second's blend.
			const Stance& St = M.Stances[static_cast<size_t>(ChIx)];
			const Class From = M.EndClass;
			M.Queue.clear();
			double Begin = At;
			const double NextChange = static_cast<size_t>(ChIx) + 1 < M.Stances.size() ? M.Stances[static_cast<size_t>(ChIx) + 1].T + M.Info.Delay : 1e18;
			const bool bOffQuick = St.Play == Stance::Gait;
			if (From != St.Cl)
			{
				const Turnover T = TurnoverOf(*M.S, From, St.Cl, bOffQuick);
				if (T.Clip >= 0)
				{
					double Avail = Until > At ? Until - At : 0.6 * (NextChange - At);
					Avail = std::min(Avail, NextShotStart(M, At) - At);
					const double Natural = T.Natural / std::max(0.5, static_cast<double>(M.Info.Jitter));
					double Dur = std::min(Natural, Avail);
					if (bOffQuick && From == Prone) Dur = std::min(Dur, 0.45);
					if (bOffQuick && From == Kneel) Dur = std::min(Dur, 0.35);
					if (Dur >= T.Least * 0.6)
					{
						Dur = std::max(Dur, T.Least * 0.6);
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
			std::stable_sort(M.Shots.begin(), M.Shots.end(), [](const Shot& A, const Shot& B) { return A.T < B.T; });
			const Soldier& S = *M.S;
			for (size_t I = 0; I < M.Shots.size(); ++I)
			{
				Shot& Sh = M.Shots[I];
				Sh.Done = 0;
				Sh.LastC = -1.f;
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
					Sh.Bullets = 6 + static_cast<int>(Unit01(Seed ^ (static_cast<uint64_t>(M.Info.Block) << 40) ^ (static_cast<uint64_t>(M.Info.K) << 24) ^ I) * 5.0);
					Sh.Lead = C.FlashT >= 0.f ? C.FlashT : 0.1f;
					Sh.Start = Sh.T - Sh.Lead;
					Sh.End = Sh.T + (Sh.Bullets - 1) * 0.1 + 0.15;
				}
				else
				{
					Sh.Lead = Sh.W == Weapon::Grenade ? (C.ReleaseT >= 0.f ? C.ReleaseT : 0.95f) : (C.FlashT >= 0.f ? C.FlashT : 0.1f);
					Sh.Start = Sh.T - Sh.Lead;
					Sh.End = Sh.Start + C.Long;
				}
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
					A.Bullets = std::max(1, std::min(A.Bullets, static_cast<int>(std::floor((B.T - A.T) / 0.1))));
					A.End = std::min(A.End, B.Start);
				}
				else
				{
					const Clip& C = S.Clips[static_cast<size_t>(A.ClipIx)];
					const double Avail = B.Start - A.Start - A.Lead;
					const double After = C.Long - A.Lead;
					if (Avail > 0.05 && After > 0.0) A.Rate2 = static_cast<float>(std::min(3.0, After / Avail));
					A.End = std::max(A.Start + A.Lead, B.Start);
				}
			}
			M.ShotCur = 0;
			M.bShotsDirty = false;
		}

		/** The shot's clip time at Now (a burst: the aim with a kick each bullet). */
		float ShotClipTime(const Shot& Sh, const Clip& C, double Now, float& OutKick) const
		{
			OutKick = 0.f;
			const double Tau = Now - Sh.Start;
			if (Tau <= 0.0) return 0.f;
			if (Sh.How == Shot::Burst)
			{
				// Held on aim; each bullet kicks him back to the shot's own frame (the recoil) and he settles before the
				// next. A gunner's lmg_burst plays itself (its shoulder shake).
				if (C.Name == "lmg_burst") return static_cast<float>(std::min(Tau, static_cast<double>(C.Long)));
				const double Since = Now - Sh.T;
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

		std::string FromWhere(const Man& M) const
		{
			// Where the round that killed him came from, against his facing: front, back or side. A shell from the gun's
			// side; with no shooter known, the front (a man faces what he fights).
			if (M.KillT < 0.0) return "front";
			double SX = 0.0, SY = 0.0;
			bool bKnown = false;
			auto Found = TrackOf.find({ M.KillBy, M.KillK });
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

		int ChooseFall(Man& M, const std::string& St, const std::string& From)
		{
			const Soldier& S = *M.S;
			Dice D(Seed ^ Mix(static_cast<uint64_t>(M.Info.Block) * 1000003ull + static_cast<uint64_t>(M.Info.K)));
			M.Info.FallStance = St;
			M.Info.FallFrom = From;
			const bool bShell = M.KillW == Weapon::Mortar || M.KillWeapon == "gun" || M.KillWeapon == "tank" || M.KillW == Weapon::Grenade;
			if (!S.Deaths.empty())
			{
				// The physics-baked falls: weigh each kind by how he was struck and how he stood, then each clip of the
				// kind by the Fates rule (never the last fall again unless it is the only one; the 2nd to 4th last
				// 0.35, 0.6, 0.85), so a battle's dead never fall twice alike running.
				const std::map<std::string, std::vector<std::string>>* Kinds = nullptr;
				const char* Order[] = { St.c_str(), St == "cover" ? "kneel" : St == "run" ? "stand" : St == "prone" ? "kneel" : "stand", "stand", "kneel" };
				for (const char* Name : Order)
				{
					auto Found = S.Deaths.find(Name);
					if (Found != S.Deaths.end() && !Found->second.empty()) { Kinds = &Found->second; break; }
				}
				if (!Kinds && !S.Deaths.empty()) Kinds = &S.Deaths.begin()->second;
				if (Kinds && !(St == "prone" && S.Deaths.find("prone") == S.Deaths.end()))
				{
					std::vector<std::pair<std::string, double>> Odds;
					std::vector<std::string> KindOfPick;
					size_t Candidates = 0;
					for (const auto& KV : *Kinds) Candidates += KV.second.size();
					for (const auto& KV : *Kinds)
					{
						const std::string& Kind = KV.first;
						double W = Kind == "spin" ? 0.6 : Kind == "thrown" ? 0.2 : Kind == "crawl" ? 0.3 : 1.0;
						if (From == "front") W *= Kind == "back" ? 3.0 : Kind == "knees" ? 1.5 : Kind == "forward" ? 0.4 : 1.0;
						if (From == "back") W *= Kind == "forward" ? 3.0 : Kind == "back" ? 0.3 : 1.0;
						if (From == "side") W *= Kind == "side" ? 3.0 : Kind == "spin" ? 2.0 : 1.0;
						if (St == "run") W *= Kind == "forward" ? 2.0 : Kind == "spin" ? 1.3 : 1.0;
						if (St == "kneel" || St == "cover") W *= Kind == "knees" ? 2.5 : Kind == "side" ? 1.5 : 1.0;
						if (St == "prone") W *= Kind == "crawl" ? 3.0 : 1.0;
						if (bShell) W *= Kind == "thrown" ? 40.0 : Kind == "spin" || Kind == "side" ? 1.3 : 1.0;
						if (!M.FallKindWanted.empty() && Kind == M.FallKindWanted) W *= 50.0;
						for (const std::string& Clip : KV.second)
						{
							Odds.push_back({ Clip, W / static_cast<double>(KV.second.size()) * Falls.Weight(Clip, Candidates) });
							KindOfPick.push_back(Kind);
						}
					}
					double Sum = 0.0;
					for (const auto& O : Odds) Sum += O.second;
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
			const std::string From = FromWhere(M);
			M.FallClip = ChooseFall(M, St, From);
			M.FallAt = D;
			M.FallFrom = M.Now;
			M.FallLie0 = M.Lie;
			M.FallLastT = -1.f;
			M.Info.FallStarted = D;
			if (M.FallClip >= 0)
			{
				const Clip& C = M.S->Clips[static_cast<size_t>(M.FallClip)];
				M.Info.FallClip = C.Name;
				M.FallDur = C.Long;
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
				M.FallDur = 0.6;
				M.FallPivot = Vec3();
				M.State = Man::Settling;
			}
		}

		// ---------------------------------------------------------------------------------------------------------
		// After the frames: the rifle laid down, the head dropped.
		// ---------------------------------------------------------------------------------------------------------
		void LowerRifle(const Man& M, Xform* Fr, float W, float Rest) const
		{
			// The rifle turned about his grip until its muzzle is on the ground (Rest metres up), as far as W: the spotter
			// laying it down for his glasses, a dead man's rifle sinking from his hands.
			if (W <= 0.f) return;
			const Soldier& S = *M.S;
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
					const double Tb = Sh.T + Sh.Done * 0.1;
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

		void Step(Man& M, double Now, double Dt, std::vector<Event>& Out, bool bQuiet)
		{
			if (M.State == Man::Rest || !M.bVisible) return;
			const Soldier& S = *M.S;
			const TrackData& Tr = Tracks[static_cast<size_t>(M.Track)];
			const double Tl = Now - M.Info.Delay;
			if (M.bShotsDirty) PlanShots(M);

			// Where he is: the track lerped on his own timeline; a man who left with his block carries on at his last
			// speed and facing and fades out over a second. A falling man stays where he was hit.
			if (M.State == Man::Alive)
			{
				float F = 0.f, V = 0.f;
				if (M.Info.Out >= 0.0 && Tl > M.Info.Out)
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

			Pose P;
			float Lie = 0.f, RifleDown = 0.f, HeadDrop = 0.f;
			Placing Pl;
			Pl.X = M.X;
			Pl.Y = M.Y;
			Pl.Facing = M.F;
			bool bRest = false;
			if (M.State == Man::Alive)
			{
				// The rules' stance on his timeline; a man about to get up and move starts getting up early, so he is
				// on his feet when the rules have him go (he would otherwise slide up off the ground at a run).
				while (M.ChCursor + 1 < M.Stances.size() && M.Stances[M.ChCursor + 1].T <= Tl) ++M.ChCursor;
				if (M.ChCursor > 0 && M.Stances[M.ChCursor].T > Tl) M.ChCursor = 0;
				int Want = static_cast<int>(M.ChCursor);
				double Until = -1.0;
				if (M.ChCursor + 1 < M.Stances.size())
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
				if (M.Shown == static_cast<int>(M.ChCursor) + 1 && Want == static_cast<int>(M.ChCursor)) Want = M.Shown;
				if (Want != M.Shown)
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
					float Rate = Sp > 0.f ? M.V / Sp : 0.f;
					G.bCapped = Rate > 1.6f;
					Rate = std::min(Rate, 1.6f);
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

				Pose A;
				float LieB = 0.f, TB = 0.f;
				SampleSegment(M, M.Cur, Now, P, LieB, TB);
				Lie = LieB;
				const float W = M.Cur.BlendIn > 0.f ? Smooth(static_cast<float>((Now - M.Cur.Start) / M.Cur.BlendIn)) : 1.f;
				RifleDown = M.Cur.bRifleDown ? W : 0.f;
				if (M.bPrev && W < 1.f)
				{
					float LieA = 0.f, TA = 0.f;
					SampleSegment(M, M.Prev, Now, A, LieA, TA);
					Blend(A, P, W, P);
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
				for (size_t I = M.ShotCur; I < M.Shots.size(); ++I)
				{
					Shot& Sh = M.Shots[I];
					if (Sh.Start - 0.15 > Now) break;
					if (Sh.How != Shot::Clip && Sh.How != Shot::Burst) continue;
					if (Now > Sh.End + 0.2) continue;
					const Clip& C = S.Clips[static_cast<size_t>(Sh.ClipIx)];
					float Kick = 0.f;
					const float Ct = ShotClipTime(Sh, C, Now, Kick);
					Pose Q;
					Sample(C, Ct, Q);
					if (Kick > 0.f && C.FlashT >= 0.f)
					{
						Pose K;
						Sample(C, C.FlashT + 0.06f, K);
						Blend(Q, K, Kick * 0.7f, Q);
					}
					const float In = Smooth(static_cast<float>((Now - (Sh.Start - 0.15)) / 0.15));
					const float OutW = 1.f - Smooth(static_cast<float>((Now - Sh.End) / 0.2));
					const float Ws = std::min(In, OutW);
					if (Ws > 0.f)
					{
						Blend(P, Q, Ws, P);
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
					const float Ct = static_cast<float>(std::min(Since, static_cast<double>(C.Long)));
					Sample(C, Ct, P);
					Blend(M.FallFrom, P, Smooth(static_cast<float>(Since / 0.12)), P);
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
			M.Now = P;
			M.Lie = Lie;
			M.RifleDown = RifleDown;
			BoneFrames(S, P, M.Model);
			if (RifleDown > 0.f) LowerRifle(M, M.Model, std::min(1.f, RifleDown), M.State == Man::Alive ? 0.035f : 0.02f);
			if (HeadDrop > 0.f) DropHead(M, M.Model, HeadDrop);
			Pl.Lie = Lie;
			M.Place = Pl;
			M.World = WorldOf(Pl, GroundAt);
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
				M.World = WorldOf(R, GroundAt);
				for (size_t B = 0; B < NB; ++B) M.Frames[B] = Compose(M.World, M.Model[B]);
				M.State = Man::Rest;
				M.Info.Rested = Now;
			}
			++M.Stamp;
			M.Last = Now;
		}

		void Reset()
		{
			for (Man& M : Men)
			{
				M.State = Man::Alive;
				M.AtCursor = M.ChCursor = 0;
				M.Shown = -1;
				M.EndClass = M.Stances.empty() ? Up : M.Stances[0].Cl;
				M.Prev = M.Cur = Segment();
				M.bPrev = false;
				M.Queue.clear();
				M.Fade = 1.f;
				M.bVisible = true;
				M.Last = -1.0;
				M.FallClip = -1;
				M.Info.FallClip.clear();
				M.Info.FallKind.clear();
				M.Info.FallStarted = M.Info.Rested = -1.0;
				for (int L = 0; L < LodCount; ++L) { M.bSkinned[L] = false; M.RestAt[L].clear(); M.RestFacing[L].clear(); }
				for (Shot& Sh : M.Shots) { Sh.Done = 0; Sh.LastC = -1.f; }
				M.ShotCur = 0;
				RestPose(M.Now);
			}
			Falls = FallMemory();
			Clock = -1.0;
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
		I.Men.clear();
		I.ManOf.clear();

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
				if (Lat < 36.0 && Lon > -18.0 && Lon < 40.0) I.Front = "desert";
				else if (Lon >= 95.0 && Lon < 125.0 && Lat > 18.0) I.Front = "china";
				else if (Lon >= 95.0 || (Lat < 20.0 && Lon > 60.0)) I.Front = "pacific";
				else if (Lat >= 36.0 && Lat < 47.0 && Lon >= 6.0 && Lon < 24.0) I.Front = "south";
				else if (Lon >= 15.0) I.Front = "east";
				else I.Front = "west";
			}
		}

		// Each block's army and the soldier file its men are drawn from.
		std::map<int, std::pair<std::string, std::string>> BlockArmy; // id -> (army, file army)
		const J& Bs = Root["blocks"];
		for (size_t K = 0; K < Bs.Size(); ++K)
		{
			const J& B = Bs.At(K);
			const std::string Army = B["army"].Text(B["nat"].Text("GER").c_str());
			std::string File = B["standIn"].Text(Army.c_str());
			BlockArmy[static_cast<int>(B["id"].Num0())] = { Army, File };
		}

		const J& Ms = Root["men"];
		for (size_t K = 0; K < Ms.Size(); ++K)
		{
			const J& Src = Ms.At(K);
			TrackData Tr;
			Tr.B = static_cast<int>(Src["b"].Num0());
			Tr.K = static_cast<int>(Src["k"].Num0());
			Tr.Role = Src["r"].Text();
			Tr.bFo = Src["fo"].Num0() != 0.0;
			const J& At = Src["at"];
			for (size_t P = 0; P < At.Size(); ++P)
			{
				const J& Pt = At.At(P);
				Tr.T.push_back(Pt.Number(0));
				Tr.X.push_back(Pt.Number(1));
				Tr.Y.push_back(Pt.Number(2));
				Tr.F.push_back(static_cast<float>(Pt.Number(3)));
			}
			const J& Ch = Src["ch"];
			for (size_t P = 0; P < Ch.Size(); ++P)
			{
				const J& C = Ch.At(P);
				TrackData::Change Cg;
				Cg.T = C.At(0).Num0();
				Cg.Pose = C.At(1).Text("idle");
				Cg.Cover = C.At(2).Text("none");
				Cg.bHidden = C.At(5).Num0() != 0.0;
				Tr.Ch.push_back(Cg);
			}
			if (Src.Has("d")) Tr.D = Src["d"].Num0();
			if (Src.Has("out")) Tr.OutT = Src["out"].Num0();
			if (Tr.T.empty()) continue;
			I.TrackOf[{ Tr.B, Tr.K }] = static_cast<int>(I.Tracks.size());
			I.Tracks.push_back(std::move(Tr));
		}

		// The men: every track of a man's role (tanks, guns and lorries are the scene's own).
		static const char* MenRoles[] = { "officer", "rifle", "mg", "at", "sapper", "holder", "fo", "hmg", "mortar" };
		for (size_t K = 0; K < I.Tracks.size(); ++K)
		{
			const TrackData& Tr = I.Tracks[K];
			bool bMan = false;
			for (const char* R : MenRoles) bMan = bMan || Tr.Role == R;
			if (!bMan) continue;
			const auto Found = BlockArmy.find(Tr.B);
			const std::string Army = Found != BlockArmy.end() ? Found->second.first : "GER";
			std::string File = Found != BlockArmy.end() ? Found->second.second : Army;
			const auto Colours = I.Armies.find(Army);
			if (Colours != I.Armies.end() && !Colours->second.StandIn.empty() && I.Soldiers.find(Army) == I.Soldiers.end()) File = Colours->second.StandIn;
			auto Sol = I.Soldiers.find(File);
			if (Sol == I.Soldiers.end()) Sol = I.Soldiers.find(Army);
			if (Sol == I.Soldiers.end() || !Sol->second)
			{
				OutError = "no soldier file for the " + Army + " men (" + File + ")";
				return false;
			}
			Impl::Man Mn;
			Mn.Track = static_cast<int>(K);
			Mn.S = Sol->second.get();
			static const ArmyColours Plain;
			Mn.Colours = Colours != I.Armies.end() ? &Colours->second : &Plain;
			Mn.Info.Block = Tr.B;
			Mn.Info.K = Tr.K;
			Mn.Info.Role = Tr.Role;
			Mn.Info.Army = Army;
			Mn.Info.FileArmy = Sol->first;
			Mn.Info.bFo = Tr.bFo;
			Mn.Info.Dead = Tr.D;
			Mn.Info.Out = Tr.D >= 0.0 ? -1.0 : Tr.OutT; // (a dead man never leaves: he lies where he fell)
			// His own looks and his own small differences, from his block and model, so he keeps them all battle and
			// from one run to the next: the face (a hash mod 8), the helmet's finish (a second hash, by share), a lag of
			// up to a fifth of a second behind the recording, a play rate of 0.95 to 1.05 on his still loops and turns
			// of pose, and where in its loop his first step falls.
			const uint64_t Who = (static_cast<uint64_t>(static_cast<uint32_t>(Tr.B)) << 32) ^ static_cast<uint64_t>(static_cast<uint32_t>(Tr.K));
			const Soldier& S = *Mn.S;
			Mn.Info.Face = S.Faces.empty() ? 0 : static_cast<int>(Mix(Who ^ 0xFACEull) % 8ull) % static_cast<int>(S.Faces.size());
			Mn.Info.Finish = -1;
			if (Mn.Colours->bFileFinishes)
			{
				std::vector<std::pair<int, double>> Fit;
				double Sum = 0.0;
				for (size_t F = 0; F < S.Finishes.size(); ++F)
				{
					const Finish& Fi = S.Finishes[F];
					const bool bWhen = I.MonthNo >= Fi.From && I.MonthNo <= Fi.To;
					const bool bSeason = Fi.Season == "any" || Fi.Season == I.SeasonName;
					const bool bFront = Fi.Front == "any" || Fi.Front == I.Front;
					if (bWhen && bSeason && bFront && Fi.Share > 0.0) { Fit.push_back({ static_cast<int>(F), Fi.Share }); Sum += Fi.Share; }
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
				// (No finish fits the date: his own helmet, painted his army's helmet colour.)
			}
			Mn.Info.Delay = 0.2 * Unit01(Who ^ 0xDE1Aull);
			Mn.Info.Jitter = static_cast<float>(0.95 + 0.1 * Unit01(Who ^ 0x7177E5ull));
			Mn.PhaseSeed = static_cast<float>(Unit01(Who ^ 0x57E9ull));
			// His stances, one per change of the track.
			for (size_t C = 0; C < Tr.Ch.size(); ++C)
			{
				const TrackData::Change& Cg = Tr.Ch[C];
				size_t Cursor = 0;
				double X = 0.0, Y = 0.0;
				float F = 0.f, V = 0.f;
				Tr.At(Cg.T + 0.05, Cursor, X, Y, F, V);
				Stance St = I.StanceOf(S, Tr.Role, Cg.Pose, V > 0.2f);
				St.T = Cg.T;
				St.Cover = Cg.Cover;
				Mn.Stances.push_back(St);
			}
			if (Mn.Stances.empty())
			{
				Stance St = I.StanceOf(S, Tr.Role, "idle", false);
				St.T = Tr.T.front();
				Mn.Stances.push_back(St);
			}
			I.ManOf[{ Tr.B, Tr.K }] = static_cast<int>(I.Men.size());
			I.Men.push_back(std::move(Mn));
		}

		// The rounds: the men's shots, and the round each dead man fell to (where it came from, for his fall).
		if (With.bReadRounds)
		{
			const J& Rs = Root["rounds"];
			for (size_t K = 0; K < Rs.Size(); ++K)
			{
				const J& R = Rs.At(K);
				if (R.Has("by") && R.Has("k"))
				{
					const auto Found = I.ManOf.find({ static_cast<int>(R["by"].Num0()), static_cast<int>(R["k"].Num0()) });
					if (Found != I.ManOf.end()) Fire(Found->second, R["t"].Num0(), WeaponOf(R["w"].Text()));
				}
				if (R["r"].Text() == "kill" && R.Has("ok"))
				{
					const auto Found = I.ManOf.find({ static_cast<int>(R["on"].Num0()), static_cast<int>(R["ok"].Num0()) });
					if (Found == I.ManOf.end()) continue;
					Impl::Man& Mn = I.Men[static_cast<size_t>(Found->second)];
					const double T = R["t"].Num0();
					if (Mn.Info.Dead >= 0.0 && (Mn.KillT < 0.0 || std::fabs(T - Mn.Info.Dead) < std::fabs(Mn.KillT - Mn.Info.Dead)))
					{
						Mn.KillT = T;
						Mn.KillBy = R.Has("by") ? static_cast<int>(R["by"].Num0()) : -1;
						Mn.KillK = R.Has("k") ? static_cast<int>(R["k"].Num0()) : -1;
						Mn.KillWeapon = R["w"].Text();
						Mn.KillW = WeaponOf(Mn.KillWeapon);
						Mn.KillX = R["x"].Num0();
						Mn.KillY = R["y"].Num0();
					}
				}
			}
		}
		// Each man's mesh without his helmet (where a finish is drawn) and his face's rest points, made now rather than on
		// the frame he first comes into view (a few milliseconds each the first time).
		for (Impl::Man& Mn : I.Men)
		{
			for (int L = 0; L < LodCount; ++L) I.FaceRestOf(Mn.S, I.OwnHelmet(Mn), L, Mn.Info.Face);
		}
		I.Reset();
		I.bBuilt = true;
		return true;
	}

	void Crowd::Fire(int Man, double T, Weapon W)
	{
		if (Man < 0 || Man >= Count()) return;
		Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		Shot Sh;
		Sh.T = T;
		Sh.W = W;
		Mn.Shots.push_back(Sh);
		Mn.bShotsDirty = true;
	}

	void Crowd::SetEye(double X, double Y, double Z)
	{
		Inner->EyeX = X;
		Inner->EyeY = Y;
		Inner->EyeZ = Z;
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
		for (Impl::Man& Mn : I.Men)
		{
			I.Step(Mn, BattleTime, Dt, Outbox, false);
			// The level of detail by the eye's distance, with a little give so a man at the edge does not flicker.
			if (!Mn.bVisible) continue;
			const double DX = Mn.X - I.EyeX, DY = Mn.Y - I.EyeY, DZ = static_cast<double>(Mn.World.T[2]) - I.EyeZ;
			const double D = std::sqrt(DX * DX + DY * DY + DZ * DZ);
			const double Give = 1.05;
			Lod L = D < 25.0 ? Near : D < 120.0 ? Mid : Far;
			if (Mn.L == Near && L == Mid && D < 25.0 * Give) L = Near;
			if (Mn.L == Mid && L == Far && D < 120.0 * Give) L = Mid;
			Mn.L = L;
		}
		std::stable_sort(Outbox.begin(), Outbox.end(), [](const Event& A, const Event& B) { return A.T < B.T; });
		I.Clock = BattleTime;
	}

	int Crowd::Count() const { return static_cast<int>(Inner->Men.size()); }

	int Crowd::Find(int Block, int K) const
	{
		const auto Found = Inner->ManOf.find({ Block, K });
		return Found == Inner->ManOf.end() ? -1 : Found->second;
	}

	const ManInfo& Crowd::Info(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Info; }
	bool Crowd::Visible(int Man) const { return Inner->Men[static_cast<size_t>(Man)].bVisible; }
	float Crowd::Fade(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Fade; }
	bool Crowd::Frozen(int Man) const { return Inner->Men[static_cast<size_t>(Man)].State == Impl::Man::Rest; }
	uint32_t Crowd::Stamp(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Stamp; }
	Lod Crowd::LodOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].L; }
	const Xform* Crowd::FramesOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].Frames; }
	const Soldier* Crowd::SoldierOf(int Man) const { return Inner->Men[static_cast<size_t>(Man)].S; }
	int Crowd::Month() const { return Inner->MonthNo; }
	const std::string& Crowd::Season() const { return Inner->SeasonName; }
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
		Impl::Man& Mn = Inner->Men[static_cast<size_t>(Man)];
		int N = Inner->VariantOf(Mn.S, Inner->OwnHelmet(Mn), L).M.Points;
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
		const Variant& V = I.VariantOf(Mn.S, I.OwnHelmet(Mn), L);
		const Finish* Fi = Mn.Info.Finish >= 0 ? &S.Finishes[static_cast<size_t>(Mn.Info.Finish)] : nullptr;
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
		const Face* Fc = Mn.Info.Face >= 0 && Mn.Info.Face < static_cast<int>(S.Faces.size()) ? &S.Faces[static_cast<size_t>(Mn.Info.Face)] : nullptr;
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
		auto Put = [&](size_t At, uint32_t Rgb, float Shade, const float* K) {
			const float Ch[3] = { static_cast<float>((Rgb >> 16) & 0xFF), static_cast<float>((Rgb >> 8) & 0xFF), static_cast<float>(Rgb & 0xFF) };
			for (int A = 0; A < 3; ++A)
			{
				const float V2 = Ch[A] * Shade * (K ? K[A] : 1.f);
				Lk.Rgb[At * 3 + static_cast<size_t>(A)] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, V2 + 0.5f)));
			}
		};
		for (int P = 0; P < Body; ++P)
		{
			const size_t Pz = static_cast<size_t>(P);
			const uint8_t Pa = V.M.Paint[Pz];
			Lk.Paint[Pz] = Pa;
			Put(Pz, Mn.Colours->Paint[Pa < 16 ? Pa : 0], V.M.Shade.empty() ? 1.f : V.M.Shade[Pz], &Mul[Pz * 3]);
		}
		if (Fi)
		{
			const Mesh& Fm = Fi->Lods[L];
			for (int P = 0; P < Extra; ++P)
			{
				const size_t Pz = static_cast<size_t>(P);
				const uint8_t Pa = Fm.Paint[Pz];
				const int Ink = Fm.Ink.empty() ? -1 : Fm.Ink[Pz];
				Lk.Paint[static_cast<size_t>(Body) + Pz] = Pa;
				const uint32_t Rgb = Ink >= 0 ? S.Inks[static_cast<size_t>(Ink)] : Mn.Colours->Paint[Pa < 16 ? Pa : 0];
				Put(static_cast<size_t>(Body) + Pz, Rgb, Fm.Shade.empty() ? 1.f : Fm.Shade[Pz], nullptr);
			}
		}
		return Lk;
	}

	int Crowd::Skin(int Man, Lod L, float* OutAt, float* OutFacing)
	{
		Impl& I = *Inner;
		Impl::Man& Mn = I.Men[static_cast<size_t>(Man)];
		const bool bOwn = I.OwnHelmet(Mn);
		const Variant& V = I.VariantOf(Mn.S, bOwn, L);
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
		std::vector<float> TmpAt, TmpFacing;
		float* At = OutAt;
		float* Fa = OutFacing;
		if (bRest)
		{
			Mn.RestAt[L].resize(static_cast<size_t>(Total) * 3);
			Mn.RestFacing[L].resize(static_cast<size_t>(Total) * 3);
			At = Mn.RestAt[L].data();
			Fa = Mn.RestFacing[L].data();
		}
		const FaceRest& R = I.FaceRestOf(Mn.S, bOwn, L, Mn.Info.Face);
		SkinMesh(V.M, Mn.Frames, At, Fa, R.At.data(), R.Facing.data());
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
		OutZ = Inner->GroundAt ? Inner->GroundAt(Mn.X, Mn.Y).Height : 0.0;
		OutFacing = Mn.F;
		return true;
	}

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
		return P;
	}
}
