#include "Battle/AirUnitColours.h"

#include "Battle/AirScrew.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <tuple>

namespace AirUnitColours
{
	namespace
	{
		// ---- A small reader for the model and palette files: a number array (a model's points, megabytes of them) is
		// kept as numbers, never a value each.
		struct Node
		{
			enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };
			Type Kind = Type::Null;
			bool Flag = false;
			double Value = 0.0;
			std::string Text;
			std::vector<double> Numbers; // an array of numbers only
			std::vector<Node> Items;     // any other array
			std::vector<std::string> Names; // an object's fields' names, and their values beside them
			std::vector<Node> Fields;

			const Node* Get(const char* Name) const
			{
				for (size_t K = 0; K < Names.size() && K < Fields.size(); ++K)
				{
					if (Names[K] == Name) return &Fields[K];
				}
				return nullptr;
			}
			double Num(const char* Name, double Default) const
			{
				const Node* N = Get(Name);
				return N && N->Kind == Type::Number ? N->Value : Default;
			}
			std::string Str(const char* Name) const
			{
				const Node* N = Get(Name);
				return N && N->Kind == Type::String ? N->Text : std::string();
			}
			bool IsArray() const { return Kind == Type::Array; }
			size_t Size() const { return Numbers.empty() ? Items.size() : Numbers.size(); }
		};

		class Reader
		{
		public:
			explicit Reader(const std::string& Source) : At(Source.c_str()), End(Source.c_str() + Source.size()) {}

			bool Read(Node& Out, std::string& Why)
			{
				if (!Value(Out, 0))
				{
					Why = Fault.empty() ? "not JSON" : Fault;
					return false;
				}
				return true;
			}

		private:
			const char* At;
			const char* End;
			std::string Fault;

			void Space()
			{
				while (At < End && (*At == ' ' || *At == '\n' || *At == '\r' || *At == '\t')) ++At;
			}
			bool Fail(const char* Why)
			{
				if (Fault.empty()) Fault = Why;
				return false;
			}
			bool Number(double& Out)
			{
				char* Next = nullptr;
				Out = std::strtod(At, &Next);
				if (Next == At) return Fail("a number that does not read");
				At = Next;
				return true;
			}
			bool String(std::string& Out)
			{
				++At; // the opening quote
				Out.clear();
				while (At < End && *At != '"')
				{
					if (*At != '\\')
					{
						Out.push_back(*At++);
						continue;
					}
					++At;
					if (At >= End) break;
					const char C = *At++;
					switch (C)
					{
					case 'n': Out.push_back('\n'); break;
					case 't': Out.push_back('\t'); break;
					case 'r': Out.push_back('\r'); break;
					case 'b': Out.push_back('\b'); break;
					case 'f': Out.push_back('\f'); break;
					case 'u':
					{
						if (End - At < 4) return Fail("a short \\u");
						const unsigned Code = static_cast<unsigned>(std::strtoul(std::string(At, At + 4).c_str(), nullptr, 16));
						At += 4;
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
					default: Out.push_back(C); break;
					}
				}
				if (At >= End) return Fail("a string left open");
				++At;
				return true;
			}
			bool Value(Node& Out, int Depth)
			{
				if (Depth > 64) return Fail("nested too deep");
				Space();
				if (At >= End) return Fail("ends early");
				const char C = *At;
				if (C == '{')
				{
					Out.Kind = Node::Type::Object;
					++At;
					Space();
					if (At < End && *At == '}') { ++At; return true; }
					for (;;)
					{
						Space();
						if (At >= End || *At != '"') return Fail("a field's name");
						std::string Name;
						if (!String(Name)) return false;
						Space();
						if (At >= End || *At != ':') return Fail("a colon");
						++At;
						Out.Names.push_back(std::move(Name));
						Out.Fields.emplace_back();
						if (!Value(Out.Fields.back(), Depth + 1)) return false;
						Space();
						if (At < End && *At == ',') { ++At; continue; }
						if (At < End && *At == '}') { ++At; return true; }
						return Fail("an object left open");
					}
				}
				if (C == '[')
				{
					Out.Kind = Node::Type::Array;
					++At;
					Space();
					if (At < End && *At == ']') { ++At; return true; }
					bool bNumbers = true;
					for (;;)
					{
						Space();
						if (At >= End) return Fail("an array left open");
						const char D = *At;
						if (bNumbers && (D == '-' || (D >= '0' && D <= '9')))
						{
							double V = 0.0;
							if (!Number(V)) return false;
							Out.Numbers.push_back(V);
						}
						else
						{
							if (bNumbers && !Out.Numbers.empty())
							{
								for (const double V : Out.Numbers)
								{
									Node N;
									N.Kind = Node::Type::Number;
									N.Value = V;
									Out.Items.push_back(std::move(N));
								}
								Out.Numbers.clear();
							}
							bNumbers = false;
							Out.Items.emplace_back();
							if (!Value(Out.Items.back(), Depth + 1)) return false;
						}
						Space();
						if (At < End && *At == ',') { ++At; continue; }
						if (At < End && *At == ']') { ++At; return true; }
						return Fail("an array left open");
					}
				}
				if (C == '"')
				{
					Out.Kind = Node::Type::String;
					return String(Out.Text);
				}
				if (C == 't' && End - At >= 4 && std::strncmp(At, "true", 4) == 0) { Out.Kind = Node::Type::Bool; Out.Flag = true; At += 4; return true; }
				if (C == 'f' && End - At >= 5 && std::strncmp(At, "false", 5) == 0) { Out.Kind = Node::Type::Bool; At += 5; return true; }
				if (C == 'n' && End - At >= 4 && std::strncmp(At, "null", 4) == 0) { At += 4; return true; }
				Out.Kind = Node::Type::Number;
				return Number(Out.Value);
			}
		};

		/** An array's numbers, whichever way it was kept. */
		std::vector<double> NumbersOf(const Node* N)
		{
			std::vector<double> Out;
			if (!N || !N->IsArray()) return Out;
			if (!N->Numbers.empty()) return N->Numbers;
			for (const Node& I : N->Items)
			{
				if (I.Kind == Node::Type::Number) Out.push_back(I.Value);
			}
			return Out;
		}
		std::vector<int> IntsOf(const Node* N)
		{
			std::vector<int> Out;
			for (const double V : NumbersOf(N)) Out.push_back(static_cast<int>(std::lround(V)));
			return Out;
		}

		const std::vector<std::string>& PlainPaints()
		{
			static const std::vector<std::string> Names = { "upper", "upper2", "under", "nose", "boss", "glass", "frame", "steel", "tyre",
				"blade", "line", "soot", "engine", "leg", "bomb", "stain", "glare", "glint" };
			return Names;
		}

		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }

		// ---- The places of a model.
		struct Point
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		Point PointOf(const Mesh& M, size_t K) { return { M.X(K), M.Y(K), M.Z(K) }; }
		double FacingY(const Mesh& M, size_t K) { return M.Facings.size() > K * 3 + 2 ? M.Facings[K * 3 + 1] : 0.0; }
		double FacingZ(const Mesh& M, size_t K) { return M.Facings.size() > K * 3 + 2 ? M.Facings[K * 3 + 2] : 0.0; }
		int PartOf(const Mesh& M, size_t K) { return K < M.Parts.size() ? M.Parts[K] : 0; }
		PaintIs IsOf(const Found& F, int Paint) { return Paint >= 0 && static_cast<size_t>(Paint) < F.Paints.size() ? F.Paints[static_cast<size_t>(Paint)] : PaintIs::Never; }
		/** What a paint is to the regions: a line of the coat (LinesTakeCoat) is the coat where it lies. */
		PaintIs AsCoat(PaintIs Is) { return Is == PaintIs::Line ? PaintIs::Coat : Is; }
		double ShadeOf(const Found& F, int Paint) { return Paint >= 0 && static_cast<size_t>(Paint) < F.Shade.size() ? F.Shade[static_cast<size_t>(Paint)] : 1.0; }
		/** An earned colour as a line of the coat wears it: each channel by its shade (unchanged at 1). */
		Rgb Shaded(Rgb C, double K)
		{
			if (K == 1.0) return C;
			auto One = [K](uint8_t V) { return static_cast<uint8_t>(std::clamp(std::lround(V * K), 0L, 255L)); };
			return Rgb{ One(C.R), One(C.G), One(C.B) };
		}

		double RootAt(const Tail& T, double X) { return T.RootA + T.RootB * X; }

		/** The hinge's x at a height (an older file's hinge line, its ends carried on straight). */
		double HingeAt(const Tail& T, double Z)
		{
			const auto& H = T.Hinge;
			if (H.size() < 2) return -1e9;
			size_t K = 0;
			while (K + 2 < H.size() && Z > H[K + 1].second) ++K;
			const auto& A = H[K];
			const auto& B = H[K + 1];
			const double Span = B.second - A.second;
			if (std::fabs(Span) < 1e-9) return A.first;
			return A.first + (B.first - A.first) * (Z - A.second) / Span;
		}

		/** Which part of a tail a point is (or none): within its slab, above its root (a fin), or its rudder. */
		enum class OnTail : uint8_t { None, Fin, Rudder };
		OnTail TailPlace(const Tail& T, double X, double Y, double Z, int Part, double XFinMost)
		{
			if (T.RudderPart >= 0 && Part == T.RudderPart) return OnTail::Rudder;
			if (std::fabs(Y - T.Y) > T.Slab || X > XFinMost || Z < RootAt(T, X) || Z > T.ZMax + 0.01) return OnTail::None;
			if (T.RudderPart >= 0) return OnTail::Fin;
			if (!T.Hinge.empty() && X < HingeAt(T, Z)) return OnTail::Rudder;
			return OnTail::Fin;
		}
		bool PartTakes(TailPart Area, OnTail Place)
		{
			return Place != OnTail::None && (Area == TailPart::Both || (Area == TailPart::Fin && Place == OnTail::Fin) || (Area == TailPart::Rudder && Place == OnTail::Rudder));
		}

		/** A cut: the line A x + C z = D in a tail's plane (every cut is upright across the span: a plane holding y), and
		 * where it is a piece of an outline, the box of its segment (cut only where the box reaches). */
		struct Line
		{
			double A = 1.0, C = 0.0, D = 0.0;
			bool bBounded = false;
			double X0 = 0.0, Z0 = 0.0, X1 = 0.0, Z1 = 0.0;
		};
		Line Through(double Ax, double Az, double Bx, double Bz, bool bBounded)
		{
			Line L;
			L.A = -(Bz - Az);
			L.C = Bx - Ax;
			const double N = std::hypot(L.A, L.C);
			if (N < 1e-12) { L.A = 1.0; L.C = 0.0; L.D = Ax; return L; }
			L.A /= N;
			L.C /= N;
			L.D = L.A * Ax + L.C * Az;
			L.bBounded = bBounded;
			L.X0 = std::min(Ax, Bx);
			L.X1 = std::max(Ax, Bx);
			L.Z0 = std::min(Az, Bz);
			L.Z1 = std::max(Az, Bz);
			return L;
		}
		Line Level(double Z) { Line L; L.A = 0.0; L.C = 1.0; L.D = Z; return L; }
		Line Upright(double X) { Line L; L.A = 1.0; L.C = 0.0; L.D = X; return L; }

		using Outline = std::vector<std::pair<double, double>>;
		/** A convex outline in x and z, counter-clockwise, grown outward by Grow on every side. */
		Outline Grown(Outline C, double Grow)
		{
			double Twice = 0.0;
			for (size_t K = 0; K < C.size(); ++K)
			{
				const auto& A = C[K];
				const auto& B = C[(K + 1) % C.size()];
				Twice += A.first * B.second - B.first * A.second;
			}
			if (Twice < 0.0) std::reverse(C.begin(), C.end());
			const size_t N = C.size();
			std::vector<double> NX(N), NZ(N), D(N);
			for (size_t K = 0; K < N; ++K)
			{
				const auto& A = C[K];
				const auto& B = C[(K + 1) % N];
				const double DX = B.first - A.first, DZ = B.second - A.second, Len = std::hypot(DX, DZ);
				NX[K] = Len > 1e-12 ? DZ / Len : 0.0; // (outward, the outline running counter-clockwise)
				NZ[K] = Len > 1e-12 ? -DX / Len : 0.0;
				D[K] = NX[K] * A.first + NZ[K] * A.second + Grow;
			}
			Outline Out(N);
			for (size_t K = 0; K < N; ++K)
			{
				const size_t J = (K + N - 1) % N; // (the side before this corner, and the side after it)
				const double Det = NX[J] * NZ[K] - NZ[J] * NX[K];
				if (std::fabs(Det) < 1e-9)
				{
					Out[K] = { C[K].first + Grow * NX[K], C[K].second + Grow * NZ[K] };
					continue;
				}
				Out[K] = { (D[J] * NZ[K] - NZ[J] * D[K]) / Det, (NX[J] * D[K] - D[J] * NX[K]) / Det };
			}
			return Out;
		}

		/** Where a marking's middle lies on a tail (x and z, on the face Side sees): the part of the skin under it, from the
		 * near mesh's triangle there (a newer file's rudder is its own part); the place itself where no skin is found. */
		OnTail MarkOn(const Model& M, const Found& F, const Tail& T, double X, double Y, double Z, double Side)
		{
			const Mesh& N = M.Near;
			double Best = 1e18;
			OnTail Out = OnTail::None;
			bool bAny = false;
			for (size_t K = 0; K + 2 < N.Triangles.size(); K += 3)
			{
				const size_t A = static_cast<size_t>(N.Triangles[K]), B = static_cast<size_t>(N.Triangles[K + 1]), C = static_cast<size_t>(N.Triangles[K + 2]);
				if (IsOf(F, N.Paints[A]) != PaintIs::Coat || IsOf(F, N.Paints[B]) != PaintIs::Coat || IsOf(F, N.Paints[C]) != PaintIs::Coat) continue;
				if (std::fabs((N.Y(A) + N.Y(B) + N.Y(C)) / 3.0 - T.Y) > T.Slab) continue;
				if (Side != 0.0 && (FacingY(N, A) + FacingY(N, B) + FacingY(N, C)) * Side <= 0.0) continue;
				const double X0 = N.X(A), Z0 = N.Z(A), X1 = N.X(B) - X0, Z1 = N.Z(B) - Z0, X2 = N.X(C) - X0, Z2 = N.Z(C) - Z0;
				const double Det = X1 * Z2 - X2 * Z1;
				if (std::fabs(Det) < 1e-10) continue;
				const double U = ((X - X0) * Z2 - X2 * (Z - Z0)) / Det, V = (X1 * (Z - Z0) - (X - X0) * Z1) / Det;
				if (U < -1e-9 || V < -1e-9 || U + V > 1.0 + 1e-9) continue;
				const double YAt = N.Y(A) + U * (N.Y(B) - N.Y(A)) + V * (N.Y(C) - N.Y(A));
				if (std::fabs(YAt - Y) >= Best) continue;
				Best = std::fabs(YAt - Y);
				Out = TailPlace(T, X, YAt, Z, PartOf(N, A), T.XMax);
				bAny = true;
			}
			return bAny ? Out : TailPlace(T, X, T.Y, Z, -1, T.XMax);
		}

		/** A patch under a tail marking (or one piece of it): a disc (Round), a box, or a four-cornered outline (Outline,
		 * its corners counter-clockwise in x and z), on one face of the tail (Side: the sign of a facing across the span
		 * that sees it; 0 either). */
		struct Patch
		{
			int Mark = -1;
			bool bMirrored = false;
			int Tail = -1;
			bool bRound = true;
			double X = 0.0, Z = 0.0, R = 0.0, HalfX = 0.0, HalfZ = 0.0;
			std::vector<std::pair<double, double>> Outline;
			double Side = 0.0;
			double ZTop = 0.0; // the highest point of its marking's patch (all its pieces)
			static constexpr int Sides = 24; // a disc is laid as a 24-sided figure round it, its sides touching the circle
			bool Holds(double PX, double PZ, double FaceY) const
			{
				if (Side != 0.0 && FaceY * Side < 0.0) return false; // (the other face of the fin: not this marking's)
				if (!Outline.empty())
				{
					for (size_t K = 0; K < Outline.size(); ++K)
					{
						const auto& A = Outline[K];
						const auto& B = Outline[(K + 1) % Outline.size()];
						if ((B.first - A.first) * (PZ - A.second) - (B.second - A.second) * (PX - A.first) < 0.0) return false;
					}
					return true;
				}
				if (!bRound) return std::fabs(PX - X) <= HalfX && std::fabs(PZ - Z) <= HalfZ;
				const double DX = PX - X, DZ = PZ - Z, Far = std::hypot(DX, DZ);
				if (Far <= R) return true;
				const double Turn = 2.0 * 3.14159265358979 / Sides;
				double A = std::atan2(DZ, DX);
				if (A < 0.0) A += 2.0 * 3.14159265358979;
				const double Mid = (std::floor(A / Turn) + 0.5) * Turn;
				return Far * std::cos(A - Mid) <= R;
			}
		};

		/** What the coat of one look is drawing, ready to ask of a place. */
		struct Painter
		{
			const Model* M = nullptr;
			const Found* F = nullptr;
			const Palette* P = nullptr;
			const Scheme* S = nullptr;
			Look L;
			bool bNight = false;
			TailPart Area = TailPart::Both;
			std::vector<double> FinMost; // each tail's foremost fin point's x
			std::vector<Patch> Patches;
			std::vector<std::vector<Line>> TailLines; // each tail's cuts
			std::vector<Line> EngineLines;

			/** What a place of the skin takes: its x, y and z, its paint's class, its part, and its facing across the span
			 * (FaceY: which face of a fin it is on). */
			Takes At(double X, double Y, double Z, PaintIs Is, int Part, double FaceY, int* PatchOf = nullptr) const
			{
				if (Is == PaintIs::Boss || Is == PaintIs::Hub)
				{
					if (L.Tier < 1) return Takes::Keep;
					for (const Engine& E : F->Engines)
					{
						if (!E.bSpinner || E.bCap || (Is == PaintIs::Hub) != E.bHubStandIn) continue;
						const double Off = std::hypot(Y - E.Hub[1], Z - E.Hub[2]);
						if (Off <= (E.bHubStandIn ? 0.3 : 0.6) && X >= E.SpinnerBack - 0.02 && X <= E.SpinnerFront + 0.02) return Takes::Spinner;
					}
					return Takes::Keep;
				}
				if (Is != PaintIs::Coat) return Takes::Keep;
				for (size_t T = 0; T < F->Tails.size(); ++T)
				{
					const Tail& Each = F->Tails[T];
					const OnTail Place = TailPlace(Each, X, Y, Z, Part, FinMost[T]);
					if (!PartTakes(Area, Place)) continue;
					const Frame& Fr = Each.Frames[static_cast<int>(Area)];
					if (!Fr.bAny || Fr.High() <= 1e-6) return Takes::Keep;
					const double H = (Z - Fr.ZLow) / Fr.High();
					if (L.Tier >= 3)
					{
						// (a marking's patch first, and whole: by night, where it reaches the pattern's half, it is laid
						// below the half too, so that the marking is never half on its patch and half on the camouflage)
						for (const Patch& Pa : Patches)
						{
							if (Pa.Tail == static_cast<int>(T) && (!bNight || Pa.ZTop >= Fr.ZLow + 0.5 * Fr.High()) && Pa.Holds(X, Z, FaceY))
							{
								if (PatchOf) *PatchOf = static_cast<int>(&Pa - Patches.data());
								return Takes::Patch;
							}
						}
						if (bNight && H < 0.5) return Takes::Keep;
						return FirstAt(S->Pattern, Fr, Each.Normal, X, Z) ? Takes::First : Takes::Second;
					}
					if (!F->bFighter && H >= 5.0 / 6.0) return Takes::Tip;
					return Takes::Keep;
				}
				for (const Engine& E : F->Engines)
				{
					const double Off = std::hypot(Y - E.Hub[1], Z - E.Hub[2]);
					if (Off > E.Reach || X > E.CowlFront + 0.02) continue;
					if (E.bCap)
					{
						if (X >= E.CowlFront - E.Cap) return L.Tier >= 1 ? Takes::Spinner : Takes::Keep;
						if (X >= E.CowlFront - E.Cap - E.Band) return L.Tier >= 2 ? Takes::Ring : Takes::Keep;
						continue;
					}
					if (X >= E.CowlFront - E.Band) return L.Tier >= 2 ? Takes::Ring : Takes::Keep;
				}
				return Takes::Keep;
			}

			Rgb ColourOf(Takes What) const
			{
				switch (What)
				{
				case Takes::Spinner:
				case Takes::Tip:
					return P->Paint(S->Spinner, bNight);
				case Takes::Ring:
					return P->Paint(S->Ring, bNight);
				case Takes::First:
				case Takes::Patch:
					return P->Paint(S->First, bNight);
				case Takes::Second:
					return P->Paint(S->Second, bNight);
				default:
					return Rgb();
				}
			}
		};

		double Linear(uint8_t V)
		{
			const double C = V / 255.0;
			return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
		}
		uint8_t Encoded(double L)
		{
			const double C = L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(Clamp(L, 0.0, 1.0), 1.0 / 2.4) - 0.055;
			return static_cast<uint8_t>(std::lround(Clamp(C, 0.0, 1.0) * 255.0));
		}
		/** Two colours mixed as the game mixes paints (FMath::Lerp of linear colours, written back as sRGB). */
		Rgb Mixed(Rgb A, Rgb B, double T)
		{
			auto One = [T](uint8_t X, uint8_t Y) { return Encoded(Linear(X) + (Linear(Y) - Linear(X)) * T); };
			return { One(A.R, B.R), One(A.G, B.G), One(A.B, B.B) };
		}
		/** The soot the game lays on a point (CampaignMiniatures: toward the twelfth paint, the plain build's soot, 0.72 at
		 * the most). */
		Rgb Sooted(const Model& M, Rgb C, double Soot)
		{
			if (Soot <= 0.0 || M.Colours.size() <= 11) return C;
			return Mixed(C, M.Colours[11], 0.72 * std::min(Soot, 100.0) / 100.0);
		}

		// ---- A triangle parted along lines: each piece a list of places among its corners (weights).
		struct Bary
		{
			double W[3] = { 0.0, 0.0, 0.0 };
		};
		using Piece = std::vector<Bary>;

		void SplitBy(const Piece& In, const Line& L, const Point Corners[3], std::vector<Piece>& Out)
		{
			auto Place = [&](const Bary& B) {
				return Point{ B.W[0] * Corners[0].X + B.W[1] * Corners[1].X + B.W[2] * Corners[2].X, 0.0,
					B.W[0] * Corners[0].Z + B.W[1] * Corners[1].Z + B.W[2] * Corners[2].Z };
			};
			constexpr double Eps = 1e-7;
			std::vector<double> F(In.size());
			bool bBelow = false, bAbove = false;
			double XLo = 1e18, XHi = -1e18, ZLo = 1e18, ZHi = -1e18;
			for (size_t K = 0; K < In.size(); ++K)
			{
				const Point P = Place(In[K]);
				F[K] = L.A * P.X + L.C * P.Z - L.D;
				bBelow = bBelow || F[K] < -Eps;
				bAbove = bAbove || F[K] > Eps;
				XLo = std::min(XLo, P.X);
				XHi = std::max(XHi, P.X);
				ZLo = std::min(ZLo, P.Z);
				ZHi = std::max(ZHi, P.Z);
			}
			if (!bBelow || !bAbove || (L.bBounded && (XHi < L.X0 - 1e-4 || XLo > L.X1 + 1e-4 || ZHi < L.Z0 - 1e-4 || ZLo > L.Z1 + 1e-4)))
			{
				Out.push_back(In);
				return;
			}
			Piece Lo, Hi;
			for (size_t K = 0; K < In.size(); ++K)
			{
				const size_t J = (K + 1) % In.size();
				const double A = F[K], B = F[J];
				if (A <= Eps) Lo.push_back(In[K]);
				if (A >= -Eps) Hi.push_back(In[K]);
				if ((A < -Eps && B > Eps) || (A > Eps && B < -Eps))
				{
					const double T = A / (A - B);
					Bary M;
					for (int I = 0; I < 3; ++I) M.W[I] = In[K].W[I] + (In[J].W[I] - In[K].W[I]) * T;
					Lo.push_back(M);
					Hi.push_back(M);
				}
			}
			if (Lo.size() >= 3) Out.push_back(std::move(Lo));
			if (Hi.size() >= 3) Out.push_back(std::move(Hi));
		}

		/** The plain average of the points of a list, by axis. */
		struct Spread
		{
			double Lo = 1e18, Hi = -1e18;
			void Take(double V) { Lo = std::min(Lo, V); Hi = std::max(Hi, V); }
			bool Any() const { return Hi >= Lo; }
		};

		double Median(std::vector<double> V)
		{
			if (V.empty()) return 0.0;
			std::nth_element(V.begin(), V.begin() + static_cast<std::ptrdiff_t>(V.size() / 2), V.end());
			return V[V.size() / 2];
		}
	}

	bool RgbOf(const std::string& Hex, Rgb& Out)
	{
		const std::string H = !Hex.empty() && Hex[0] == '#' ? Hex.substr(1) : Hex;
		if (H.size() != 6 || H.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
		const unsigned long V = std::strtoul(H.c_str(), nullptr, 16);
		Out = { static_cast<uint8_t>((V >> 16) & 0xFF), static_cast<uint8_t>((V >> 8) & 0xFF), static_cast<uint8_t>(V & 0xFF) };
		return true;
	}

	const Scheme* Palette::Of(const std::string& Nation, int Number) const
	{
		const auto Found = Nations.find(NationOf(Nation));
		if (Found == Nations.end() || Number < 0) return nullptr;
		for (const Scheme& S : Found->second)
		{
			if (S.Number == Number) return &S;
		}
		return nullptr;
	}

	Rgb Palette::Paint(const std::string& Name, bool bNight) const
	{
		const auto Found = Colours.find(Name);
		const Rgb C = Found == Colours.end() ? Rgb{ 128, 128, 128 } : Found->second;
		if (!bNight) return C;
		if (Name == Cream) return CreamAtNight;
		auto Dull = [this](uint8_t V) { return static_cast<uint8_t>(std::lround(Clamp(V * NightMultiply, 0.0, 255.0))); };
		return { Dull(C.R), Dull(C.G), Dull(C.B) };
	}

	bool ReadPalette(const std::string& Text, Palette& Out, std::string& Why)
	{
		Node Root;
		if (!Reader(Text).Read(Root, Why)) return false;
		Out = Palette();
		const Node* Colours = Root.Get("colours");
		if (!Colours || Colours->Kind != Node::Type::Object) { Why = "no colours"; return false; }
		for (size_t K = 0; K < Colours->Names.size(); ++K)
		{
			Rgb C;
			if (!RgbOf(Colours->Fields[K].Str("srgb"), C)) { Why = "colour " + Colours->Names[K] + " has no srgb"; return false; }
			Out.Colours[Colours->Names[K]] = C;
		}
		if (const Node* Night = Root.Get("night"))
		{
			Out.NightMultiply = Night->Num("multiply", Out.NightMultiply);
			Rgb Cream;
			if (RgbOf(Night->Str("bone_becomes"), Cream)) Out.CreamAtNight = Cream;
		}
		if (const Node* Patterns = Root.Get("patterns"))
		{
			for (size_t K = 0; K < Patterns->Names.size(); ++K) Out.Patterns[Patterns->Names[K]] = Patterns->Fields[K].Text;
		}
		const Node* Nations = Root.Get("nations");
		if (!Nations || Nations->Kind != Node::Type::Object) { Why = "no nations"; return false; }
		for (size_t N = 0; N < Nations->Names.size(); ++N)
		{
			const std::string& Nation = Nations->Names[N];
			std::vector<Scheme>& List = Out.Nations[Nation];
			for (const Node& Row : Nations->Fields[N].Items)
			{
				Scheme S;
				S.Number = static_cast<int>(Row.Num("scheme", -1.0));
				S.Spinner = Row.Str("spinner");
				S.Ring = Row.Str("ring");
				S.Pattern = Row.Str("pattern");
				S.Words = Row.Str("words");
				const Node* Tail = Row.Get("tail");
				if (Tail && Tail->Items.size() == 2)
				{
					S.First = Tail->Items[0].Text;
					S.Second = Tail->Items[1].Text;
				}
				if (S.Number < 0 || !Out.Colours.count(S.Spinner) || !Out.Colours.count(S.Ring) || !Out.Colours.count(S.First) || !Out.Colours.count(S.Second)
					|| !Out.Patterns.count(S.Pattern))
				{
					Why = Nation + " scheme " + std::to_string(S.Number) + " names a colour or pattern the palette does not have";
					return false;
				}
				List.push_back(S);
			}
		}
		return true;
	}

	bool LookOf(bool bOn, const std::map<int, CorpsMark>& Marks, int Corps, const Palette& P, bool bNightBattle, int Month, Look& Out)
	{
		Out = Look();
		if (!bOn || Corps < 0) return false;
		const auto Found = Marks.find(Corps);
		if (Found == Marks.end() || !P.Of(Found->second.Nation, Found->second.Scheme)) return false;
		Out.Nation = NationOf(Found->second.Nation);
		Out.Scheme = Found->second.Scheme;
		Out.Tier = Found->second.Tier;
		Out.bNightBattle = bNightBattle;
		const int M = ((Month % 12) + 12) % 12;
		Out.bWinter = M == 11 || M == 0 || M == 1;
		return Out.Valid();
	}

	int Model::Paint(const char* Name) const
	{
		for (size_t K = 0; K < PaintNames.size(); ++K)
		{
			if (PaintNames[K] == Name) return static_cast<int>(K);
		}
		return -1;
	}

	bool ReadModel(const std::string& Text, const std::string& Kind, Model& Out, std::string& Why)
	{
		Node Root;
		if (!Reader(Text).Read(Root, Why)) return false;
		Out = Model();
		Out.Kind = Kind;
		if (Root.Str("kind") != "aircraft") { Why = "not an aircraft"; return false; }
		Out.Role = Root.Str("role");
		Out.Engines = static_cast<int>(Root.Num("engines", 0.0));
		Out.Jets = static_cast<int>(Root.Num("jets", 0.0));
		if (const Node* Names = Root.Get("paint_names"))
		{
			for (const Node& N : Names->Items) Out.PaintNames.push_back(N.Text);
		}
		if (Out.PaintNames.empty()) Out.PaintNames = PlainPaints();
		if (const Node* Colours = Root.Get("colours"))
		{
			for (const Node& N : Colours->Items)
			{
				Rgb C{ 128, 128, 128 };
				RgbOf(N.Text, C);
				Out.Colours.push_back(C);
			}
		}
		if (const Node* Parts = Root.Get("parts"))
		{
			for (const Node& N : Parts->Items)
			{
				Part P;
				P.Name = N.Str("name");
				const std::vector<double> Pivot = NumbersOf(N.Get("pivot")), Axis = NumbersOf(N.Get("axis"));
				for (int K = 0; K < 3; ++K)
				{
					if (Pivot.size() == 3) P.Pivot[K] = Pivot[static_cast<size_t>(K)];
					if (Axis.size() == 3) P.Axis[K] = Axis[static_cast<size_t>(K)];
				}
				Out.Parts.push_back(P);
			}
		}
		if (const Node* Marks = Root.Get("marks"); Marks && Marks->IsArray())
		{
			for (const Node& N : Marks->Items)
			{
				MarkPlace Mk;
				Mk.Tag = N.Str("tag");
				Mk.Place = N.Str("place");
				Mk.Radius = N.Num("radius", 0.3);
				const Node* Both = N.Get("both");
				Mk.bBoth = Both && Both->Kind == Node::Type::Bool && Both->Flag;
				const std::vector<double> At = NumbersOf(N.Get("at")), Facing = NumbersOf(N.Get("facing"));
				if (At.size() < 3 || Facing.size() < 3) continue;
				for (int K = 0; K < 3; ++K)
				{
					Mk.At[K] = At[static_cast<size_t>(K)];
					Mk.Facing[K] = Facing[static_cast<size_t>(K)];
				}
				if (const Node* Quad = N.Get("quad"); Quad && Quad->Items.size() == 4)
				{
					for (const Node& Corner : Quad->Items)
					{
						const std::vector<double> XZ = NumbersOf(&Corner);
						if (XZ.size() >= 2) Mk.Quad.emplace_back(XZ[0], XZ[1]);
					}
					if (Mk.Quad.size() != 4) Mk.Quad.clear();
				}
				Out.Marks.push_back(Mk);
			}
		}
		for (const bool bFar : { false, true })
		{
			const Node* Part = Root.Get(bFar ? "far" : "near");
			Mesh& M = bFar ? Out.Far : Out.Near;
			if (!Part)
			{
				if (!bFar) { Why = "no near mesh"; return false; }
				continue;
			}
			M.Points = NumbersOf(Part->Get("points"));
			M.Facings = NumbersOf(Part->Get("facings"));
			M.Paints = IntsOf(Part->Get("paints"));
			M.Triangles = IntsOf(Part->Get("triangles"));
			M.Parts = IntsOf(Part->Get("part"));
			M.Soot = IntsOf(Part->Get("soot"));
			M.Blend = IntsOf(Part->Get("blend"));
			if (M.Points.size() != M.Paints.size() * 3 || M.Facings.size() != M.Points.size() || M.Triangles.size() % 3 != 0)
			{
				Why = std::string(bFar ? "far" : "near") + " mesh not whole";
				return false;
			}
			if (M.Parts.size() != M.Paints.size()) M.Parts.clear();
			if (M.Soot.size() != M.Paints.size()) M.Soot.clear();
			if (M.Blend.size() != M.Paints.size()) M.Blend.clear();
			for (const int T : M.Triangles)
			{
				if (T < 0 || static_cast<size_t>(T) >= M.Paints.size()) { Why = "a triangle's corner past the points"; return false; }
			}
		}
		// Fighter or bomber: the file's role ("fighter"; "bomber", "dive bomber", "torpedo bomber"); a file with none is one
		// of the old plain build's (Tools/build_plane.py: the B-17, Lancaster, He 111, the twin stand-in, the Stuka and the
		// Sturmovik), every one a bomber.
		Out.bFighter = Out.Role.find("fighter") != std::string::npos;
		const int Under = Out.Paint("under");
		if (Under >= 0 && static_cast<size_t>(Under) < Out.Colours.size())
		{
			const Rgb U = Out.Colours[static_cast<size_t>(Under)];
			const int Most = std::max({ U.R, U.G, U.B }), Least = std::min({ U.R, U.G, U.B });
			Out.bBlackUnder = Most <= 0x30 && Most - Least <= 0x10;
		}
		return true;
	}

	bool LinesTakeCoat(const std::string& Kind)
	{
		return Kind == "B17Fine";
	}

	TailPart PartFor(const std::string& Nation)
	{
		const std::string N = NationOf(Nation);
		if (N == "UK") return TailPart::Rudder;
		if (N == "FRA") return TailPart::Fin;
		return TailPart::Both;
	}

	Found Find(const Model& M)
	{
		Found F;
		F.bFighter = M.bFighter;
		const Mesh& N = M.Near;
		F.Paints.assign(M.PaintNames.size(), PaintIs::Never);
		for (size_t K = 0; K < M.PaintNames.size(); ++K)
		{
			const std::string& Name = M.PaintNames[K];
			if (Name == "upper" || Name == "upper2" || Name == "under" || Name == "nose") F.Paints[K] = PaintIs::Coat;
			else if (Name == "boss" || Name == "hub") F.Paints[K] = PaintIs::Boss;
			else if (Name == "leg") F.Paints[K] = PaintIs::Hub;
		}
		// (the finer B-17's rivet and seam lines: the coat where they lie, each a shade of it as her file paints the line
		// against the coat it runs over, upper for rivet and seam, under for rivet_under and seam_under)
		F.Shade.assign(M.PaintNames.size(), 1.0);
		if (LinesTakeCoat(M.Kind))
		{
			auto Light = [&M](int Paint)
			{
				if (Paint < 0 || static_cast<size_t>(Paint) >= M.Colours.size()) return -1.0;
				const Rgb& C = M.Colours[static_cast<size_t>(Paint)];
				return 0.2126 * C.R + 0.7152 * C.G + 0.0722 * C.B;
			};
			const double Upper = Light(M.Paint("upper")), Under = Light(M.Paint("under"));
			for (size_t K = 0; K < M.PaintNames.size(); ++K)
			{
				const std::string& Name = M.PaintNames[K];
				const bool bUpper = Name == "rivet" || Name == "seam", bUnder = Name == "rivet_under" || Name == "seam_under";
				const double Coat = bUpper ? Upper : Under, Own = Light(static_cast<int>(K));
				if ((!bUpper && !bUnder) || Coat <= 0.0 || Own < 0.0) continue;
				F.Paints[K] = PaintIs::Line;
				F.Shade[K] = std::clamp(Own / Coat, 0.4, 1.6);
			}
		}
		const size_t Count = N.Count();
		if (Count == 0) { F.Note = "no points"; return F; }
		Spread XAll, ZAll;
		std::vector<double> CoatZ;
		for (size_t K = 0; K < Count; ++K)
		{
			XAll.Take(N.X(K));
			ZAll.Take(N.Z(K));
			if (IsOf(F, N.Paints[K]) == PaintIs::Coat) CoatZ.push_back(N.Z(K));
		}
		const double Long = XAll.Hi - XAll.Lo;
		const double ZMid = Median(CoatZ);

		// ---- The engines: each airscrew found from its blades (Battle/AirScrew, as the game finds them to turn).
		{
			std::vector<AirScrew::Vec3> Points(Count);
			std::vector<bool> Blade(Count, false);
			for (size_t K = 0; K < Count; ++K)
			{
				Points[K] = { N.X(K), N.Y(K), N.Z(K) };
				const int P = N.Paints[K];
				const std::string& Name = P >= 0 && static_cast<size_t>(P) < M.PaintNames.size() ? M.PaintNames[static_cast<size_t>(P)] : std::string();
				Blade[K] = Name.rfind("blade", 0) == 0;
			}
			const AirScrew::Found Screws = AirScrew::Find(Points, Blade, N.Triangles);
			for (const AirScrew::Screw& S : Screws.Screws)
			{
				if (std::fabs(S.Axis.X) < 0.95 || S.Radius < 0.5 || S.Radius > 2.8) continue;
				Engine E;
				E.Hub[0] = S.Hub.X;
				E.Hub[1] = S.Hub.Y;
				E.Hub[2] = S.Hub.Z;
				F.Engines.push_back(E);
			}
		}
		auto OffAxis = [&N](size_t K, const Engine& E) { return std::hypot(N.Y(K) - E.Hub[1], N.Z(K) - E.Hub[2]); };
		for (Engine& E : F.Engines)
		{
			Spread Boss, Hub;
			for (size_t K = 0; K < Count; ++K)
			{
				const PaintIs Is = IsOf(F, N.Paints[K]);
				const double X = N.X(K);
				if (Is == PaintIs::Boss && OffAxis(K, E) <= 0.6 && X >= E.Hub[0] - 1.5 && X <= E.Hub[0] + 1.0) Boss.Take(X);
				if (Is == PaintIs::Hub && OffAxis(K, E) <= 0.3 && X >= E.Hub[0] - 0.6 && X <= E.Hub[0] + 0.6) Hub.Take(X);
			}
			if (Boss.Any())
			{
				E.bSpinner = true;
				E.SpinnerFront = Boss.Hi;
				E.SpinnerBack = Boss.Lo;
			}
			else if (Hub.Any())
			{
				E.bSpinner = true;
				E.bHubStandIn = true;
				E.SpinnerFront = Hub.Hi;
				E.SpinnerBack = Hub.Lo;
			}
			else
			{
				E.SpinnerFront = E.SpinnerBack = E.Hub[0];
			}
			// (the cowling's front: the foremost of its coat about the axis, behind the spinner's front)
			Spread Cowl;
			const double Ahead = std::max(E.SpinnerFront, E.Hub[0]) + 0.05;
			for (size_t K = 0; K < Count; ++K)
			{
				if (IsOf(F, N.Paints[K]) != PaintIs::Coat) continue;
				const double Off = OffAxis(K, E), X = N.X(K);
				if (Off >= 0.08 && Off <= 1.0 && X <= Ahead && X >= E.SpinnerBack - 2.5) Cowl.Take(X);
			}
			const double Spinner = E.bSpinner ? E.SpinnerFront - E.SpinnerBack : 0.3;
			E.Band = Spinner < 0.2 ? Spinner : Clamp(0.5 * Spinner, 0.2, Spinner);
			if (!Cowl.Any())
			{
				E.CowlFront = E.SpinnerBack;
				E.Band = 0.0;
				E.Reach = 0.0;
				continue;
			}
			E.CowlFront = Cowl.Hi;
			// (how far out the band reaches: the cowling's skin about the axis over the band and a little behind it, where a
			// cowling that swells behind its lip is still the cowling; out to the first gap in it, so that a wing or a
			// fuselage beyond is never reached)
			double Reach = 0.0; // (from the lip's own reach ...)
			std::vector<double> Radii;
			for (size_t K = 0; K < Count; ++K)
			{
				if (IsOf(F, N.Paints[K]) != PaintIs::Coat) continue;
				const double Off = OffAxis(K, E), X = N.X(K);
				if (Off <= 1.0 && X >= E.CowlFront - E.Band && X <= E.CowlFront + 0.02) Reach = std::max(Reach, Off);
				if (Off <= 1.5 && X >= E.CowlFront - 2.0 * E.Band - 0.3 && X <= E.CowlFront + 0.02) Radii.push_back(Off);
			}
			// (... out over the skin just behind it, to the first gap)
			std::sort(Radii.begin(), Radii.end());
			for (const double R : Radii)
			{
				if (R <= Reach) continue;
				if (R - Reach > 0.2) break;
				Reach = R;
			}
			E.Reach = Reach + 0.03;
		}
		// A fighter flies only its own airscrew's colours (none has two); a jet fighter's nose cap stands for its spinner.
		if (F.bFighter && F.Engines.empty())
		{
			Spread Tip;
			for (size_t K = 0; K < Count; ++K)
			{
				if (IsOf(F, N.Paints[K]) == PaintIs::Coat && std::fabs(N.Y(K)) < 1.0) Tip.Take(N.X(K));
			}
			if (Tip.Any())
			{
				Engine E;
				E.bCap = true;
				E.bSpinner = true;
				E.Cap = 0.4;
				E.Band = 0.2;
				E.CowlFront = Tip.Hi;
				E.SpinnerFront = Tip.Hi;
				E.SpinnerBack = Tip.Hi - E.Cap;
				double SY = 0.0, SZ = 0.0;
				int Near = 0;
				for (size_t K = 0; K < Count; ++K)
				{
					if (IsOf(F, N.Paints[K]) == PaintIs::Coat && std::fabs(N.Y(K)) < 1.0 && N.X(K) >= Tip.Hi - 0.15)
					{
						SY += N.Y(K);
						SZ += N.Z(K);
						++Near;
					}
				}
				E.Hub[0] = Tip.Hi;
				E.Hub[1] = Near ? SY / Near : 0.0;
				E.Hub[2] = Near ? SZ / Near : 0.0;
				double Reach = 0.0;
				for (size_t K = 0; K < Count; ++K)
				{
					if (IsOf(F, N.Paints[K]) == PaintIs::Coat && N.X(K) >= Tip.Hi - E.Cap - E.Band && std::hypot(N.Y(K) - E.Hub[1], N.Z(K) - E.Hub[2]) <= 1.0)
						Reach = std::max(Reach, std::hypot(N.Y(K) - E.Hub[1], N.Z(K) - E.Hub[2]));
				}
				E.Reach = Reach + 0.02;
				F.Engines.push_back(E);
				F.Note += "a jet: its nose cap stands for the spinner. ";
			}
		}
		if (F.Engines.empty()) F.Note += "no airscrew found. ";

		// ---- The tails. The aft third of the machine; its fins' tops; for each fin its slab, its root and its rudder.
		const double AftOf = XAll.Lo + 0.35 * Long;
		std::vector<size_t> Aft;
		for (size_t K = 0; K < Count; ++K)
		{
			if (N.X(K) <= AftOf && IsOf(F, N.Paints[K]) == PaintIs::Coat) Aft.push_back(K);
		}
		std::vector<int> Rudders;
		for (size_t P = 0; P < M.Parts.size(); ++P)
		{
			if (M.Parts[P].Name.rfind("rudder", 0) == 0) Rudders.push_back(static_cast<int>(P));
		}
		std::vector<std::pair<double, int>> Fins; // (y, rudder part)
		if (!Rudders.empty() && !N.Parts.empty())
		{
			for (const int R : Rudders)
			{
				double SY = 0.0;
				int Of = 0;
				for (size_t K = 0; K < Count; ++K)
				{
					if (N.Parts[K] == R) { SY += N.Y(K); ++Of; }
				}
				if (Of > 0) Fins.emplace_back(std::fabs(SY / Of) < 0.3 ? 0.0 : SY / Of, R);
			}
		}
		if (Fins.empty() && !Aft.empty())
		{
			double Top = -1e18;
			for (const size_t K : Aft) Top = std::max(Top, N.Z(K));
			const double From = Top - 0.3 * (Top - ZMid);
			std::vector<double> Port, Starboard;
			for (const size_t K : Aft)
			{
				if (N.Z(K) < From) continue;
				(N.Y(K) < 0.0 ? Port : Starboard).push_back(N.Y(K));
			}
			const double PortMid = Median(Port), StarboardMid = Median(Starboard);
			if (!Port.empty() && !Starboard.empty() && PortMid < -0.5 && StarboardMid > 0.5)
			{
				Fins.emplace_back(PortMid, -1);
				Fins.emplace_back(StarboardMid, -1);
			}
			else
			{
				Fins.emplace_back(0.0, -1);
			}
		}
		const int LinePaint = M.Paint("line");
		for (const auto& Fin : Fins)
		{
			Tail T;
			T.Y = Fin.first;
			T.RudderPart = Fin.second;
			const bool bTwin = std::fabs(T.Y) > 0.5;
			// (its top, and the upper part of it: the fin's own thickness there)
			double Top = -1e18;
			for (const size_t K : Aft)
			{
				if (std::fabs(N.Y(K) - T.Y) <= 0.6) Top = std::max(Top, N.Z(K));
			}
			if (Top < -1e17) continue;
			T.ZMax = Top; // (its box's top, for TailPlace; the rest of its box once its points are known)
			const double Upper = Top - 0.3 * (Top - ZMid);
			double Thick = 0.0;
			Spread UpperX;
			for (const size_t K : Aft)
			{
				if (std::fabs(N.Y(K) - T.Y) <= 0.6 && N.Z(K) >= Upper)
				{
					Thick = std::max(Thick, std::fabs(N.Y(K) - T.Y));
					UpperX.Take(N.X(K));
				}
			}
			T.Slab = std::max(2.2 * Thick, Thick + 0.04);
			// The root: the skin the fin stands on (the fuselage's top for a single fin, the tailplane's for a twin), as the
			// highest skin facing up within the slab, or just outside it, in each stretch along the fin; a straight line
			// through them (z = RootA + RootB x), so that the colour's foot is a clean line.
			const double Below = Top - 0.25 * (Top - ZMid);
			const double Fore = UpperX.Hi + 0.6 * (UpperX.Hi - UpperX.Lo) + 0.2;
			const double Bin = std::max(0.05, Long / 80.0);
			std::map<int, double> Under;
			for (const size_t K : Aft)
			{
				const double DY = N.Y(K) - T.Y, X = N.X(K), Z = N.Z(K);
				if (Z >= Below || X > Fore) continue;
				const double Up = FacingZ(N, K);
				bool bSkin = false;
				if (std::fabs(DY) <= T.Slab) bSkin = Up >= 0.7;
				else if (!bTwin && std::fabs(DY) <= T.Slab + 0.3) bSkin = Up >= 0.1 && FacingY(N, K) * (DY > 0.0 ? 1.0 : -1.0) >= 0.15;
				else if (bTwin && std::fabs(DY) <= T.Slab + 0.5) bSkin = Up >= 0.7 && DY * T.Y < 0.0;
				if (!bSkin) continue;
				const int Key = static_cast<int>(std::floor(X / Bin));
				const auto Have = Under.find(Key);
				if (Have == Under.end() || Z > Have->second) Under[Key] = Z;
			}
			if (Under.size() >= 2)
			{
				double SX = 0.0, SZ = 0.0, SXX = 0.0, SXZ = 0.0;
				for (const auto& B : Under)
				{
					const double X = (B.first + 0.5) * Bin;
					SX += X;
					SZ += B.second;
					SXX += X * X;
					SXZ += X * B.second;
				}
				const double Num = static_cast<double>(Under.size());
				const double Den = Num * SXX - SX * SX;
				T.RootB = std::fabs(Den) > 1e-12 ? Clamp((Num * SXZ - SX * SZ) / Den, -0.6, 0.6) : 0.0;
				T.RootA = (SZ - T.RootB * SX) / Num;
				// (not above any of the skin it was drawn through: lifted to clear the highest of it a little, so that none of
				// the fuselage's top takes the fin's colour)
				double Lift = 0.0;
				for (const auto& B : Under)
				{
					const double X = (B.first + 0.5) * Bin;
					Lift = std::max(Lift, B.second - (T.RootA + T.RootB * X));
				}
				T.RootA += Lift + 0.01;
			}
			else
			{
				T.RootA = ZMid + 0.35 * (Top - ZMid);
				F.Note += "a fin's root not found (laid at a third of its height). ";
			}
			// Its foremost point: the fin's sides (facing across the span) above the root, within the slab.
			double FinMost = -1e18;
			for (const size_t K : Aft)
			{
				if (std::fabs(N.Y(K) - T.Y) <= T.Slab && N.Z(K) >= RootAt(T, N.X(K)) + 0.02 && std::fabs(FacingY(N, K)) >= 0.4) FinMost = std::max(FinMost, N.X(K));
			}
			T.XMax = FinMost > -1e17 ? FinMost + 0.02 : UpperX.Hi;
			// An older file's rudder: aft of the hinge line it paints on the fin (paint "line"), from the root up.
			if (T.RudderPart < 0 && LinePaint >= 0)
			{
				const double Foot = RootAt(T, UpperX.Lo), High = Top - Foot;
				std::map<int, std::vector<double>> Rows;
				const double Step = std::max(0.05, High / 10.0);
				for (size_t K = 0; K < Count; ++K)
				{
					if (N.Paints[K] != LinePaint || std::fabs(N.Y(K) - T.Y) > T.Slab || N.X(K) > T.XMax) continue;
					const double Z = N.Z(K);
					if (Z < RootAt(T, N.X(K)) + 0.08 * High || Z > Top) continue;
					Rows[static_cast<int>(std::floor((Z - Foot) / Step))].push_back(N.X(K));
				}
				for (auto& Row : Rows)
				{
					T.Hinge.emplace_back(Median(Row.second), Foot + (Row.first + 0.5) * Step);
				}
				if (T.Hinge.size() >= 3)
				{
					// (carried on straight past its ends, under the root and over the top)
					const auto A = T.Hinge[0], B = T.Hinge[1], Y = T.Hinge[T.Hinge.size() - 2], Z = T.Hinge.back();
					auto Along = [](std::pair<double, double> P, std::pair<double, double> Q, double ToZ) {
						const double DZ = Q.second - P.second;
						return std::make_pair(std::fabs(DZ) < 1e-9 ? P.first : P.first + (Q.first - P.first) * (ToZ - P.second) / DZ, ToZ);
					};
					T.Hinge.insert(T.Hinge.begin(), Along(A, B, Foot - 1.0));
					T.Hinge.push_back(Along(Y, Z, Top + 1.0));
				}
				else
				{
					T.Hinge.clear();
					F.Note += "a fin with no hinge line (no rudder of its own). ";
				}
			}
			// The hinge's way: a newer file's rudder axis, an older file's hinge line; upright where it has neither.
			double HX = 0.0, HZ = 1.0;
			if (T.RudderPart >= 0)
			{
				const Part& R = M.Parts[static_cast<size_t>(T.RudderPart)];
				HX = R.Axis[0];
				HZ = R.Axis[2];
			}
			else if (T.Hinge.size() >= 2)
			{
				HX = T.Hinge.back().first - T.Hinge.front().first;
				HZ = T.Hinge.back().second - T.Hinge.front().second;
			}
			if (HZ < 0.0) { HX = -HX; HZ = -HZ; }
			const double HL = std::hypot(HX, HZ);
			if (HL > 1e-9) { HX /= HL; HZ /= HL; } else { HX = 0.0; HZ = 1.0; }
			T.Normal[0] = HZ;
			T.Normal[1] = -HX;
			F.Tails.push_back(T);
		}
		// Each tail's box and frames, from the near mesh's points on it.
		for (Tail& T : F.Tails)
		{
			Spread BX, BZ;
			std::vector<std::pair<double, double>> Pts[3];
			for (size_t K = 0; K < Count; ++K)
			{
				if (IsOf(F, N.Paints[K]) != PaintIs::Coat) continue;
				const OnTail Place = TailPlace(T, N.X(K), N.Y(K), N.Z(K), PartOf(N, K), T.XMax);
				if (Place == OnTail::None) continue;
				BX.Take(N.X(K));
				BZ.Take(N.Z(K));
				for (int A = 0; A < 3; ++A)
				{
					if (PartTakes(static_cast<TailPart>(A), Place)) Pts[A].emplace_back(N.X(K), N.Z(K));
				}
			}
			if (!BX.Any()) continue;
			T.XMin = BX.Lo;
			T.ZMin = BZ.Lo;
			T.ZMax = std::max(T.ZMax, BZ.Hi);
			T.XMax = std::max(T.XMax, BX.Hi);
			for (int A = 0; A < 3; ++A)
			{
				Frame& Fr = T.Frames[A];
				if (Pts[A].size() < 6) continue;
				Fr.bAny = true;
				Fr.ZLow = 1e18;
				Fr.ZHigh = -1e18;
				Fr.XMin = 1e18;
				Fr.XMax = -1e18;
				for (const auto& P : Pts[A])
				{
					Fr.ZLow = std::min(Fr.ZLow, P.second);
					Fr.ZHigh = std::max(Fr.ZHigh, P.second);
					Fr.XMin = std::min(Fr.XMin, P.first);
					Fr.XMax = std::max(Fr.XMax, P.first);
				}
				const double H = Fr.High();
				Fr.XLeadTop = -1e18;
				Fr.XTrailLow = 1e18;
				Fr.SFore = -1e18;
				Fr.SAft = 1e18;
				for (const auto& P : Pts[A])
				{
					if (P.second >= Fr.ZHigh - 0.12 * H) Fr.XLeadTop = std::max(Fr.XLeadTop, P.first);
					if (P.second <= Fr.ZLow + 0.12 * H) Fr.XTrailLow = std::min(Fr.XTrailLow, P.first);
					const double S = T.Normal[0] * P.first + T.Normal[1] * P.second;
					Fr.SFore = std::max(Fr.SFore, S);
					Fr.SAft = std::min(Fr.SAft, S);
				}
			}
		}

		// ---- A far shape made without spinners (the finer B-17's, Content/Models/B17Fine.json: no boss or hub point on it,
		// its airscrews' domes in the nacelles' "engine" grey, its cowlings' lips a ring further back than the near
		// shape's). So that steps 1 and 2 show past 300 m as they do close up, on it alone: that grey within each spinner's
		// place stands for the spinner, and each band is laid behind the far shape's own lip. A far shape with a spinner
		// of its own, every other model's, is laid as it always was (FarPaints and FarCowlFront left empty).
		const Mesh& Fa = M.Far;
		const int Grey = M.Paint("engine");
		bool bFarSpinner = false; // (a point of it a spinner where Places would make one)
		for (size_t K = 0; K < Fa.Count() && !bFarSpinner; ++K)
		{
			const PaintIs Is = IsOf(F, Fa.Paints[K]);
			if (Is != PaintIs::Boss && Is != PaintIs::Hub) continue;
			for (const Engine& E : F.Engines)
			{
				if (!E.bSpinner || E.bCap || (Is == PaintIs::Hub) != E.bHubStandIn) continue;
				bFarSpinner = bFarSpinner || (std::hypot(Fa.Y(K) - E.Hub[1], Fa.Z(K) - E.Hub[2]) <= (E.bHubStandIn ? 0.3 : 0.6) && Fa.X(K) >= E.SpinnerBack - 0.02 && Fa.X(K) <= E.SpinnerFront + 0.02);
			}
		}
		if (Fa.Count() > 0 && !bFarSpinner && Grey >= 0 && static_cast<size_t>(Grey) < F.Paints.size() && !F.Engines.empty() && !F.Engines.front().bCap)
		{
			F.FarPaints = F.Paints;
			F.FarPaints[static_cast<size_t>(Grey)] = F.Engines.front().bHubStandIn ? PaintIs::Hub : PaintIs::Boss;
			for (const Engine& E : F.Engines)
			{
				// (the far shape's own lip: its foremost coat about the axis, behind the near lip, as the near lip is found)
				Spread Lip;
				for (size_t K = 0; K < Fa.Count(); ++K)
				{
					if (AsCoat(IsOf(F, Fa.Paints[K])) != PaintIs::Coat) continue;
					const double Off = std::hypot(Fa.Y(K) - E.Hub[1], Fa.Z(K) - E.Hub[2]), X = Fa.X(K);
					if (Off >= 0.08 && Off <= E.Reach && X <= E.CowlFront + 0.02 && X >= E.SpinnerBack - 2.5) Lip.Take(X);
				}
				F.FarCowlFront.push_back(Lip.Any() && E.Band > 0.0 ? std::min(Lip.Hi, E.CowlFront) : E.CowlFront);
			}
		}
		return F;
	}

	Found OnShape(const Found& F, bool bFar)
	{
		Found Out = F;
		if (!bFar) return Out;
		if (!F.FarPaints.empty()) Out.Paints = F.FarPaints;
		for (size_t K = 0; K < Out.Engines.size() && K < F.FarCowlFront.size(); ++K) Out.Engines[K].CowlFront = F.FarCowlFront[K];
		return Out;
	}

	bool FirstAt(const std::string& Pattern, const Frame& F, const double Normal[2], double X, double Z)
	{
		const double H = F.High();
		if (H <= 1e-9) return true;
		const double Up = (Z - F.ZLow) / H;
		if (Pattern == "tipped") return Up < 0.8;
		if (Pattern == "bands")
		{
			const int K = static_cast<int>(Clamp(std::floor(Up * 5.0), 0.0, 4.0));
			return K % 2 == 0;
		}
		if (Pattern == "checks")
		{
			const double Side = H / 5.0;
			const long Row = static_cast<long>(std::floor((F.ZHigh - Z) / Side));
			const long Col = static_cast<long>(std::floor((F.XLeadTop - X) / Side));
			return (((Row + Col) % 2) + 2) % 2 == 0;
		}
		if (Pattern == "stripes")
		{
			const double Chord = F.SFore - F.SAft;
			if (Chord <= 1e-9) return true;
			const double S = Normal[0] * X + Normal[1] * Z;
			const int K = static_cast<int>(Clamp(std::floor((F.SFore - S) / Chord * 6.0), 0.0, 5.0));
			return K % 2 == 0;
		}
		if (Pattern == "halved")
		{
			// (the slant from the top front corner to the bottom rear corner: first above and behind it)
			const double AX = F.XLeadTop, AZ = F.ZHigh, BX = F.XTrailLow, BZ = F.ZLow;
			const double Side = (BX - AX) * (Z - AZ) - (BZ - AZ) * (X - AX);
			return Side <= 0.0;
		}
		return true;
	}

	void Places(const Model& M, const Found& Regions, bool bFar, std::vector<Where>& Out, std::vector<int>* TailOf)
	{
		const Found F = OnShape(Regions, bFar); // (a far shape made without spinners laid by its own stand-ins: Find)
		const Mesh& Me = bFar ? M.Far : M.Near;
		Out.assign(Me.Count(), Where::None);
		if (TailOf) TailOf->assign(Me.Count(), -1);
		for (size_t K = 0; K < Me.Count(); ++K)
		{
			const PaintIs Is = AsCoat(IsOf(F, Me.Paints[K]));
			const double X = Me.X(K), Y = Me.Y(K), Z = Me.Z(K);
			if (Is == PaintIs::Boss || Is == PaintIs::Hub)
			{
				for (const Engine& E : F.Engines)
				{
					if (!E.bSpinner || E.bCap || (Is == PaintIs::Hub) != E.bHubStandIn) continue;
					if (std::hypot(Y - E.Hub[1], Z - E.Hub[2]) <= (E.bHubStandIn ? 0.3 : 0.6) && X >= E.SpinnerBack - 0.02 && X <= E.SpinnerFront + 0.02) Out[K] = Where::Spinner;
				}
				continue;
			}
			if (Is != PaintIs::Coat) continue;
			bool bTail = false;
			for (size_t T = 0; T < F.Tails.size() && !bTail; ++T)
			{
				const OnTail Place = TailPlace(F.Tails[T], X, Y, Z, PartOf(Me, K), F.Tails[T].XMax);
				if (Place == OnTail::None) continue;
				Out[K] = Place == OnTail::Fin ? Where::Fin : Where::Rudder;
				if (TailOf) (*TailOf)[K] = static_cast<int>(T);
				bTail = true;
			}
			if (bTail) continue;
			for (const Engine& E : F.Engines)
			{
				if (std::hypot(Y - E.Hub[1], Z - E.Hub[2]) > E.Reach || X > E.CowlFront + 0.02) continue;
				if (E.bCap && X >= E.CowlFront - E.Cap) { Out[K] = Where::Spinner; break; }
				if (X >= E.CowlFront - E.Cap - E.Band) { Out[K] = Where::Ring; break; }
			}
		}
	}

	bool NightFinish(const Model& M, const Look& L)
	{
		return L.bNightBattle || M.bBlackUnder;
	}

	std::string KeyOf(const Model& M, const Look& L)
	{
		return L.Nation + " " + std::to_string(L.Scheme) + " t" + std::to_string(L.Tier) + (NightFinish(M, L) ? " night" : " day");
	}

	int Coat::Count(Takes What) const
	{
		int N = 0;
		for (const Takes T : Took) N += T == What ? 1 : 0;
		return N;
	}

	Coat Paint(const Model& M, const Found& Regions, bool bFar, const Palette& P, const Look& L)
	{
		const Found F = OnShape(Regions, bFar); // (a far shape made without spinners laid by its own stand-ins: Find)
		Coat Out;
		const Mesh& Me = bFar ? M.Far : M.Near;
		Out.Took.assign(Me.Count(), Takes::Keep);
		const Scheme* S = L.Valid() ? P.Of(L.Nation, L.Scheme) : nullptr;
		if (!S || Me.Count() == 0) return Out;
		Painter Pa;
		Pa.M = &M;
		Pa.F = &F;
		Pa.P = &P;
		Pa.S = S;
		Pa.L = L;
		Pa.bNight = NightFinish(M, L);
		Pa.Area = PartFor(L.Nation);
		Out.bNight = Pa.bNight;
		for (const Tail& T : F.Tails) Pa.FinMost.push_back(T.XMax);
		// The plain patches under the tail's markings, where the pattern is checks or stripes (the brief, section 2): for a
		// marking whose middle lies on a part of the tail this nation colours (Britain's fin flash and France's rudder
		// stripes are on the parts they leave plain, and need none), shaped as the marking and a little larger, on its face.
		if (L.Tier >= 3 && (S->Pattern == "checks" || S->Pattern == "stripes"))
		{
			for (size_t K = 0; K < M.Marks.size(); ++K)
			{
				const MarkPlace& Mk = M.Marks[K];
				if (Mk.Tag == "*FIN" && !L.bFinStripes) continue; // (a plain machine's rudder stripes, which this nation has none of)
				for (const bool bMirrored : { false, true })
				{
					if (bMirrored && !Mk.bBoth) continue;
					const double X = Mk.At[0], Y = bMirrored ? -Mk.At[1] : Mk.At[1], Z = Mk.At[2], R = Mk.Radius;
					const double FaceY = bMirrored ? -Mk.Facing[1] : Mk.Facing[1];
					// (the face it is drawn on, as the miniatures draw it: a hair off the skin on the side it faces)
					const double Side = std::fabs(FaceY) < 0.3 ? 0.0 : FaceY > 0.0 ? 1.0 : -1.0;
					for (size_t T = 0; T < F.Tails.size(); ++T)
					{
						const Tail& Each = F.Tails[T];
						if (std::fabs(Y - Each.Y) > Each.Slab + 0.15 || X < Each.XMin - R || X > Each.XMax + R || Z < Each.ZMin - R || Z > Each.ZMax + R) continue;
						if (!PartTakes(Pa.Area, MarkOn(M, F, Each, X, Y, Z, Side))) continue;
						Patch Pt;
						Pt.Mark = static_cast<int>(K);
						Pt.bMirrored = bMirrored;
						Pt.Tail = static_cast<int>(T);
						Pt.Side = Side;
						auto Lay = [&Pa](Patch Piece) {
							if (!Piece.Outline.empty())
							{
								Piece.ZTop = -1e18;
								for (const auto& C : Piece.Outline) Piece.ZTop = std::max(Piece.ZTop, C.second);
							}
							else
							{
								Piece.ZTop = Piece.Z + (Piece.bRound ? Piece.R / std::cos(3.14159265358979 / Patch::Sides) : Piece.HalfZ);
							}
							Pa.Patches.push_back(std::move(Piece));
						};
						if (Mk.Quad.size() == 4)
						{
							// (its own four-cornered place, the lower leading corner first; a little larger on every side)
							const auto LowLead = Mk.Quad[0], LowTrail = Mk.Quad[1], HighTrail = Mk.Quad[2], HighLead = Mk.Quad[3];
							auto Place = [&](double U, double V) {
								const double AX = LowLead.first + (LowTrail.first - LowLead.first) * U, AZ = LowLead.second + (LowTrail.second - LowLead.second) * U;
								const double BX = HighLead.first + (HighTrail.first - HighLead.first) * U, BZ = HighLead.second + (HighTrail.second - HighLead.second) * U;
								return std::make_pair(AX + (BX - AX) * V, AZ + (BZ - AZ) * V);
							};
							auto Apart = [](std::pair<double, double> From, std::pair<double, double> To) { return std::hypot(To.first - From.first, To.second - From.second); };
							const double Chord = std::max(0.05, Apart(Place(0.0, 0.5), Place(1.0, 0.5))), Tall = std::max(0.05, Apart(Place(0.5, 0.0), Place(0.5, 1.0)));
							auto Piece = [&](double U0, double V0, double U1, double V1) {
								// (a little larger: 2 cm and a twenty-fifth of the piece's own narrower way, on every side)
								const double Grow = 0.02 + 0.04 * std::min(Chord * (U1 - U0), Tall * (V1 - V0));
								Patch One = Pt;
								One.bRound = false;
								One.Outline = Grown({ Place(U0, V0), Place(U1, V0), Place(U1, V1), Place(U0, V1) }, Grow);
								Lay(std::move(One));
							};
							if (Mk.Tag == "ITA-CROSS")
							{
								// (Savoy's white cross is the marking, and the field behind it the pattern (the brief: "fin and
								// rudder, as the field behind the white cross"): a patch under its two arms and its shield, as
								// CampaignMiniatures draws them: the arms a quarter of the chord wide, the whole height and the
								// whole chord; the shield a third of the chord across and 1.24 times that high)
								const double ArmU = 0.12, ArmV = 0.12 * Chord / Tall;
								const double ShieldU = 0.17, ShieldV = 0.62 * 0.34 * Chord / Tall;
								Piece(0.5 - ArmU, 0.0, 0.5 + ArmU, 1.0);
								Piece(0.0, 0.5 - ArmV, 1.0, 0.5 + ArmV);
								Piece(0.5 - ShieldU, 0.5 - ShieldV, 0.5 + ShieldU, 0.5 + ShieldV);
							}
							else
							{
								Piece(0.0, 0.0, 1.0, 1.0);
							}
						}
						else if (Mk.Tag == "#" || Mk.Tag.find("FIN") != std::string::npos)
						{
							// (a serial's figures or a fin flash: a box)
							Pt.bRound = false;
							Pt.X = X;
							Pt.Z = Z;
							Pt.HalfX = 1.15 * R;
							Pt.HalfZ = 1.15 * R;
							Lay(Pt);
						}
						else
						{
							Pt.X = X;
							Pt.Z = Z;
							Pt.R = 1.25 * R;
							Lay(Pt);
						}
					}
				}
			}
			// (a marking's pieces, Savoy's cross's arms and shield, laid or left by night as one: each holds the highest of
			// them)
			for (Patch& Each : Pa.Patches)
			{
				for (const Patch& Other : Pa.Patches)
				{
					if (Other.Mark == Each.Mark && Other.bMirrored == Each.bMirrored && Other.Tail == Each.Tail) Each.ZTop = std::max(Each.ZTop, Other.ZTop);
				}
			}
		}
		// Each tail's cuts: its root (and an older file's hinge), the pattern's edges, the night's half, the bomber's tip,
		// and the patches' outlines.
		Pa.TailLines.resize(F.Tails.size());
		for (size_t T = 0; T < F.Tails.size(); ++T)
		{
			const Tail& Each = F.Tails[T];
			const Frame& Fr = Each.Frames[static_cast<int>(Pa.Area)];
			std::vector<Line>& Lines = Pa.TailLines[T];
			Line Root;
			Root.A = -Each.RootB;
			Root.C = 1.0;
			Root.D = Each.RootA;
			const double RN = std::hypot(Root.A, Root.C);
			Root.A /= RN;
			Root.C /= RN;
			Root.D /= RN;
			Lines.push_back(Root);
			for (size_t K = 0; K + 1 < Each.Hinge.size(); ++K)
			{
				Lines.push_back(Through(Each.Hinge[K].first, Each.Hinge[K].second, Each.Hinge[K + 1].first, Each.Hinge[K + 1].second, true));
			}
			if (!Fr.bAny || Fr.High() <= 1e-6) continue; // (a frame with no height has no pattern to cut: the painter keeps it)
			const double H = Fr.High();
			if (L.Tier < 3)
			{
				if (!F.bFighter) Lines.push_back(Level(Fr.ZLow + H * 5.0 / 6.0));
				continue;
			}
			if (Pa.bNight) Lines.push_back(Level(Fr.ZLow + 0.5 * H));
			const std::string& Pattern = S->Pattern;
			if (Pattern == "tipped") Lines.push_back(Level(Fr.ZLow + 0.8 * H));
			else if (Pattern == "bands")
			{
				for (int K = 1; K < 5; ++K) Lines.push_back(Level(Fr.ZLow + H * K / 5.0));
			}
			else if (Pattern == "checks")
			{
				const double Side = H / 5.0;
				for (int K = 1; K < 5; ++K) Lines.push_back(Level(Fr.ZHigh - Side * K));
				const int From = static_cast<int>(std::floor((Fr.XLeadTop - Each.XMax) / Side)) - 1;
				const int To = static_cast<int>(std::ceil((Fr.XLeadTop - Each.XMin) / Side)) + 1;
				for (int K = From; K <= To; ++K) Lines.push_back(Upright(Fr.XLeadTop - Side * K));
			}
			else if (Pattern == "stripes")
			{
				for (int K = 1; K < 6; ++K)
				{
					Line St;
					St.A = Each.Normal[0];
					St.C = Each.Normal[1];
					St.D = Fr.SFore - (Fr.SFore - Fr.SAft) * K / 6.0;
					Lines.push_back(St);
				}
			}
			else if (Pattern == "halved")
			{
				Lines.push_back(Through(Fr.XLeadTop, Fr.ZHigh, Fr.XTrailLow, Fr.ZLow, false));
			}
			for (const Patch& Pt : Pa.Patches)
			{
				if (Pt.Tail != static_cast<int>(T)) continue;
				if (!Pt.Outline.empty())
				{
					for (size_t K = 0; K < Pt.Outline.size(); ++K)
					{
						const auto& A = Pt.Outline[K];
						const auto& B = Pt.Outline[(K + 1) % Pt.Outline.size()];
						Lines.push_back(Through(A.first, A.second, B.first, B.second, true));
					}
				}
				else if (Pt.bRound)
				{
					constexpr int Sides = Patch::Sides;
					for (int K = 0; K < Sides; ++K)
					{
						// (the outline a little outside the disc, so that the disc lies within it)
						const double A0 = 2.0 * 3.14159265358979 * K / Sides, A1 = 2.0 * 3.14159265358979 * (K + 1) / Sides;
						const double RR = Pt.R / std::cos(3.14159265358979 / Sides);
						Lines.push_back(Through(Pt.X + RR * std::cos(A0), Pt.Z + RR * std::sin(A0), Pt.X + RR * std::cos(A1), Pt.Z + RR * std::sin(A1), true));
					}
				}
				else
				{
					const double X0 = Pt.X - Pt.HalfX, X1 = Pt.X + Pt.HalfX, Z0 = Pt.Z - Pt.HalfZ, Z1 = Pt.Z + Pt.HalfZ;
					Lines.push_back(Through(X0, Z0, X1, Z0, true));
					Lines.push_back(Through(X1, Z0, X1, Z1, true));
					Lines.push_back(Through(X1, Z1, X0, Z1, true));
					Lines.push_back(Through(X0, Z1, X0, Z0, true));
				}
			}
		}
		for (const Engine& E : F.Engines)
		{
			if (E.bCap)
			{
				if (L.Tier >= 1) Pa.EngineLines.push_back(Upright(E.CowlFront - E.Cap));
				if (L.Tier >= 2) Pa.EngineLines.push_back(Upright(E.CowlFront - E.Cap - E.Band));
			}
			else if (L.Tier >= 2 && E.Band > 0.0)
			{
				Pa.EngineLines.push_back(Upright(E.CowlFront - E.Band));
			}
		}
		auto Soot = [&Me](int K) { return K >= 0 && static_cast<size_t>(K) < Me.Soot.size() ? static_cast<double>(Me.Soot[static_cast<size_t>(K)]) : 0.0; };
		std::set<std::tuple<int, int, int>> Laid;
		// Each point's own.
		for (size_t K = 0; K < Me.Count(); ++K)
		{
			int Under = -1;
			const Takes T = Pa.At(Me.X(K), Me.Y(K), Me.Z(K), AsCoat(IsOf(F, Me.Paints[K])), PartOf(Me, K), FacingY(Me, K), &Under);
			Out.Took[K] = T;
			if (T == Takes::Keep) continue;
			Out.Points.emplace_back(static_cast<int>(K), Sooted(M, Shaded(Pa.ColourOf(T), ShadeOf(F, Me.Paints[K])), Soot(static_cast<int>(K))));
			if (Under >= 0)
			{
				const Patch& Pt = Pa.Patches[static_cast<size_t>(Under)];
				Laid.emplace(Pt.Mark, Pt.bMirrored ? 1 : 0, Pt.Tail);
			}
		}
		// Each triangle within reach of an earned region, parted along its cuts.
		struct Box
		{
			double X0, X1, Y0, Y1, Z0, Z1;
			const std::vector<Line>* Lines;
		};
		std::vector<Box> Boxes;
		for (size_t T = 0; T < F.Tails.size(); ++T)
		{
			const Tail& Each = F.Tails[T];
			Boxes.push_back({ Each.XMin - 0.1, Each.XMax + 0.1, Each.Y - Each.Slab - 0.05, Each.Y + Each.Slab + 0.05, Each.ZMin - 0.2, Each.ZMax + 0.1, &Pa.TailLines[T] });
		}
		for (const Engine& E : F.Engines)
		{
			const double Back = std::min(E.SpinnerBack, E.CowlFront - E.Band - E.Cap) - 0.1;
			Boxes.push_back({ Back, std::max(E.SpinnerFront, E.CowlFront) + 0.1, E.Hub[1] - E.Reach - 0.1, E.Hub[1] + E.Reach + 0.1, E.Hub[2] - E.Reach - 0.1,
				E.Hub[2] + E.Reach + 0.1, &Pa.EngineLines });
		}
		for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
		{
			const int C[3] = { Me.Triangles[T], Me.Triangles[T + 1], Me.Triangles[T + 2] };
			const PaintIs Is = AsCoat(IsOf(F, Me.Paints[static_cast<size_t>(C[0])]));
			if (Is == PaintIs::Never || AsCoat(IsOf(F, Me.Paints[static_cast<size_t>(C[1])])) != Is || AsCoat(IsOf(F, Me.Paints[static_cast<size_t>(C[2])])) != Is) continue;
			Point Corners[3];
			Spread X, Y, Z;
			for (int I = 0; I < 3; ++I)
			{
				Corners[I] = PointOf(Me, static_cast<size_t>(C[I]));
				X.Take(Corners[I].X);
				Y.Take(Corners[I].Y);
				Z.Take(Corners[I].Z);
			}
			std::vector<const std::vector<Line>*> Cuts;
			for (const Box& B : Boxes)
			{
				if (X.Hi < B.X0 || X.Lo > B.X1 || Y.Hi < B.Y0 || Y.Lo > B.Y1 || Z.Hi < B.Z0 || Z.Lo > B.Z1) continue;
				Cuts.push_back(B.Lines);
			}
			const bool bAnyEarned = Out.Took[static_cast<size_t>(C[0])] != Takes::Keep || Out.Took[static_cast<size_t>(C[1])] != Takes::Keep || Out.Took[static_cast<size_t>(C[2])] != Takes::Keep;
			if (Cuts.empty() && !bAnyEarned) continue;
			std::vector<Piece> Pieces;
			Piece Whole(3);
			for (int I = 0; I < 3; ++I) Whole[static_cast<size_t>(I)].W[I] = 1.0;
			Pieces.push_back(Whole);
			for (const std::vector<Line>* Lines : Cuts)
			{
				for (const Line& Li : *Lines)
				{
					std::vector<Piece> Next;
					for (const Piece& Pc : Pieces) SplitBy(Pc, Li, Corners, Next);
					Pieces.swap(Next);
				}
			}
			const int Part = PartOf(Me, static_cast<size_t>(C[0]));
			std::vector<Takes> Labels;
			Labels.reserve(Pieces.size());
			for (const Piece& Pc : Pieces)
			{
				Bary Mid;
				for (const Bary& B : Pc)
					for (int I = 0; I < 3; ++I) Mid.W[I] += B.W[I] / static_cast<double>(Pc.size());
				const double PX = Mid.W[0] * Corners[0].X + Mid.W[1] * Corners[1].X + Mid.W[2] * Corners[2].X;
				const double PY = Mid.W[0] * Corners[0].Y + Mid.W[1] * Corners[1].Y + Mid.W[2] * Corners[2].Y;
				const double PZ = Mid.W[0] * Corners[0].Z + Mid.W[1] * Corners[1].Z + Mid.W[2] * Corners[2].Z;
				const double PF = Mid.W[0] * FacingY(Me, static_cast<size_t>(C[0])) + Mid.W[1] * FacingY(Me, static_cast<size_t>(C[1])) + Mid.W[2] * FacingY(Me, static_cast<size_t>(C[2]));
				int Under = -1;
				Labels.push_back(Pa.At(PX, PY, PZ, Is, Part, PF, &Under));
				if (Under >= 0)
				{
					const Patch& Pt = Pa.Patches[static_cast<size_t>(Under)];
					Laid.emplace(Pt.Mark, Pt.bMirrored ? 1 : 0, Pt.Tail);
				}
			}
			if (Pieces.size() == 1 && Labels[0] == Out.Took[static_cast<size_t>(C[0])] && Labels[0] == Out.Took[static_cast<size_t>(C[1])]
				&& Labels[0] == Out.Took[static_cast<size_t>(C[2])])
			{
				continue; // (whole, and its corners already take what it takes)
			}
			Cut Ct;
			for (int I = 0; I < 3; ++I) Ct.Triangle[I] = C[I];
			for (size_t K = 0; K < Pieces.size(); ++K)
			{
				const Piece& Pc = Pieces[K];
				const Takes What = Labels[K];
				std::vector<int> Index;
				for (const Bary& B : Pc)
				{
					Corner Co;
					for (int I = 0; I < 3; ++I)
					{
						Co.From[I] = C[I];
						Co.Weight[I] = B.W[I];
					}
					Co.bEarned = What != Takes::Keep;
					Co.What = What;
					if (Co.bEarned)
					{
						const double Sooting = B.W[0] * Soot(C[0]) + B.W[1] * Soot(C[1]) + B.W[2] * Soot(C[2]);
						// (a piece of a line's triangle a shade of the coat's colour, mixed as its corners are; any other exactly as before)
						const double S0 = ShadeOf(F, Me.Paints[static_cast<size_t>(C[0])]), S1 = ShadeOf(F, Me.Paints[static_cast<size_t>(C[1])]),
									 S2 = ShadeOf(F, Me.Paints[static_cast<size_t>(C[2])]);
						const double Shading = S0 == 1.0 && S1 == 1.0 && S2 == 1.0 ? 1.0 : B.W[0] * S0 + B.W[1] * S1 + B.W[2] * S2;
						Co.Colour = Sooted(M, Shaded(Pa.ColourOf(What), Shading), Sooting);
					}
					Index.push_back(static_cast<int>(Ct.Corners.size()));
					Ct.Corners.push_back(Co);
				}
				for (size_t I = 1; I + 1 < Index.size(); ++I)
				{
					Ct.Pieces.push_back(Index[0]);
					Ct.Pieces.push_back(Index[I]);
					Ct.Pieces.push_back(Index[I + 1]);
				}
			}
			Out.Cuts.push_back(std::move(Ct));
		}
		for (const auto& Each : Laid)
		{
			PatchLaid Pl;
			Pl.Mark = std::get<0>(Each);
			Pl.bMirrored = std::get<1>(Each) != 0;
			Pl.Tail = std::get<2>(Each);
			Out.Patches.push_back(Pl);
		}
		return Out;
	}

	void LayTriangles(const Coat& C, const std::vector<int>& Triangles, int PointCount, int FirstNew, std::vector<int>& Out, std::vector<std::pair<int, int>>& New)
	{
		Out.clear();
		New.clear();
		Out.reserve(Triangles.size());
		std::map<std::tuple<int, int, int>, size_t> CutOf;
		for (size_t K = 0; K < C.Cuts.size(); ++K)
		{
			int T[3] = { C.Cuts[K].Triangle[0], C.Cuts[K].Triangle[1], C.Cuts[K].Triangle[2] };
			std::sort(T, T + 3);
			CutOf[std::make_tuple(T[0], T[1], T[2])] = K;
		}
		// (each cut's corners numbered once, the first time one of its triangles is laid)
		std::vector<int> FirstOf(C.Cuts.size(), -1);
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const int A = Triangles[T], B = Triangles[T + 1], D = Triangles[T + 2];
			int Sorted[3] = { A, B, D };
			std::sort(Sorted, Sorted + 3);
			const auto Found = A < PointCount && B < PointCount && D < PointCount ? CutOf.find(std::make_tuple(Sorted[0], Sorted[1], Sorted[2])) : CutOf.end();
			if (Found == CutOf.end())
			{
				Out.insert(Out.end(), { A, B, D });
				continue;
			}
			const Cut& Ct = C.Cuts[Found->second];
			if (FirstOf[Found->second] < 0)
			{
				FirstOf[Found->second] = FirstNew + static_cast<int>(New.size());
				for (size_t J = 0; J < Ct.Corners.size(); ++J) New.emplace_back(static_cast<int>(Found->second), static_cast<int>(J));
			}
			const int Base = FirstOf[Found->second];
			// (the shape's corners an even turn of the file's, the pieces run as they were cut; an odd one, each turned over)
			const int* F = Ct.Triangle;
			const bool bOver = !((A == F[0] && B == F[1]) || (A == F[1] && B == F[2]) || (A == F[2] && B == F[0]));
			for (size_t P = 0; P + 2 < Ct.Pieces.size(); P += 3)
			{
				if (bOver) Out.insert(Out.end(), { Base + Ct.Pieces[P], Base + Ct.Pieces[P + 2], Base + Ct.Pieces[P + 1] });
				else Out.insert(Out.end(), { Base + Ct.Pieces[P], Base + Ct.Pieces[P + 1], Base + Ct.Pieces[P + 2] });
			}
		}
	}

	std::vector<Rgb> BaseColours(const Model& M, bool bFar)
	{
		const Mesh& Me = bFar ? M.Far : M.Near;
		std::vector<Rgb> Out(Me.Count());
		for (size_t K = 0; K < Me.Count(); ++K)
		{
			const int Paint = Me.Paints[K];
			Rgb C = Paint >= 0 && static_cast<size_t>(Paint) < M.Colours.size() ? M.Colours[static_cast<size_t>(Paint)] : Rgb{ 128, 128, 128 };
			if (Paint == 0 && M.Colours.size() > 1 && K < Me.Blend.size() && Me.Blend[K] > 0) C = Mixed(M.Colours[0], M.Colours[1], std::min(Me.Blend[K], 100) / 100.0);
			Out[K] = Sooted(M, C, K < Me.Soot.size() ? static_cast<double>(Me.Soot[K]) : 0.0);
		}
		return Out;
	}
}
