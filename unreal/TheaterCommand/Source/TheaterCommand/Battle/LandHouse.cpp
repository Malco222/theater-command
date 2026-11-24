#include "Battle/LandHouse.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace LandHouse
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		// ------------------------------------------------------------------ numbers

		uint64_t Mix(uint64_t H)
		{
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return H;
		}
		uint64_t Hash(uint64_t A, uint64_t B) { return Mix(A * 0x9E3779B97F4A7C15ull ^ Mix(B + 0x632BE59BD9B4E019ull)); }
		uint64_t Hash(uint64_t A, uint64_t B, uint64_t C) { return Hash(Hash(A, B), C); }
		double Unit(uint64_t H) { return static_cast<double>(H >> 11) / 9007199254740992.0; }

		struct Rng
		{
			uint64_t State;
			explicit Rng(uint64_t Seed) : State(Mix(Seed + 0x40B5E1ull)) {}
			double Next()
			{
				State += 0x9E3779B97F4A7C15ull;
				return Unit(Mix(State));
			}
			double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Next(); }
			int Below(int N) { return std::min(N - 1, static_cast<int>(Next() * N)); }
		};

		double Clamp01(double X) { return std::clamp(X, 0.0, 1.0); }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp01((X - E0) / (E1 - E0));
			return T * T * (3.0 - 2.0 * T);
		}
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Sq(double X) { return X * X; }

		double Lattice(int64_t I, int64_t J, uint64_t Seed)
		{
			return Unit(Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull + static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full + Seed * 0x165667B19E3779F9ull)) * 2.0 - 1.0;
		}
		double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }
		/** Smooth value noise, -1 to 1, its lattice one unit apart. */
		double Noise(double X, double Y, uint64_t Seed)
		{
			const double FX = std::floor(X), FY = std::floor(Y);
			const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY);
			const double U = Fade(X - FX), V = Fade(Y - FY);
			return Lerp(Lerp(Lattice(I, J, Seed), Lattice(I + 1, J, Seed), U), Lerp(Lattice(I, J + 1, Seed), Lattice(I + 1, J + 1, Seed), U), V);
		}
		/** The same, 0 to 1. */
		double Noise01(double X, double Y, uint64_t Seed) { return 0.5 + 0.5 * Noise(X, Y, Seed); }

		// ------------------------------------------------------------------ geometry and colour

		struct V3
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		V3 operator+(V3 A, V3 B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 operator-(V3 A, V3 B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 operator*(V3 A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
		double Dot(V3 A, V3 B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		V3 Cross(V3 A, V3 B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Norm(V3 A, V3 Else = { 0.0, 0.0, 1.0 })
		{
			const double L = std::sqrt(Dot(A, A));
			return L > 1e-12 ? A * (1.0 / L) : Else;
		}
		const V3 Up{ 0.0, 0.0, 1.0 };

		/** A colour while it is mixed: sRGB 0 to 255, as the field's. */
		struct Col
		{
			double R = 0.0, G = 0.0, B = 0.0;
		};
		Col Rgb(double R, double G, double B) { return { R, G, B }; }
		Col Mixed(const Col& A, const Col& B, double T)
		{
			T = Clamp01(T);
			return { Lerp(A.R, B.R, T), Lerp(A.G, B.G, T), Lerp(A.B, B.B, T) };
		}
		Col Scaled(const Col& A, double K) { return { A.R * K, A.G * K, A.B * K }; }
		Col Varied(const Col& A, double L, double W) { return { A.R * L * (1.0 + W), A.G * L, A.B * L * (1.0 - W) }; }
		LandField::Rgb8 Byte(const Col& C)
		{
			LandField::Rgb8 Out;
			Out.R = static_cast<uint8_t>(std::clamp(std::lround(C.R), 0L, 255L));
			Out.G = static_cast<uint8_t>(std::clamp(std::lround(C.G), 0L, 255L));
			Out.B = static_cast<uint8_t>(std::clamp(std::lround(C.B), 0L, 255L));
			return Out;
		}

		/** The mesh as it is written: points, triangles wound to face where they are asked to. */
		struct Pen
		{
			LandField::Mesh& M;
			explicit Pen(LandField::Mesh& In) : M(In) {}
			uint32_t Vert(V3 P, V3 N, const Col& C)
			{
				M.Positions.push_back(static_cast<float>(P.X));
				M.Positions.push_back(static_cast<float>(P.Y));
				M.Positions.push_back(static_cast<float>(P.Z));
				M.Normals.push_back(static_cast<float>(N.X));
				M.Normals.push_back(static_cast<float>(N.Y));
				M.Normals.push_back(static_cast<float>(N.Z));
				M.Colours.push_back(Byte(C));
				return static_cast<uint32_t>(M.Colours.size() - 1);
			}
			V3 PointOf(uint32_t I) const { return { M.Positions[I * 3], M.Positions[I * 3 + 1], M.Positions[I * 3 + 2] }; }
			/** A triangle of points already written, turned to face Toward. */
			void Tri(uint32_t A, uint32_t B, uint32_t C, V3 Toward)
			{
				const V3 N = Cross(PointOf(B) - PointOf(A), PointOf(C) - PointOf(A));
				if (Dot(N, Toward) < 0.0) std::swap(B, C);
				M.Triangles.push_back(A);
				M.Triangles.push_back(B);
				M.Triangles.push_back(C);
			}
			/** A flat quad, its corners in order round it, turned to face Toward, its normal its own. */
			void Quad(V3 A, V3 B, V3 C, V3 D, Col CA, Col CB, Col CC, Col CD, V3 Toward)
			{
				V3 N = Cross(B - A, C - A) + Cross(C - A, D - A);
				if (Dot(N, N) < 1e-18) return;
				N = Norm(N);
				if (Dot(N, Toward) < 0.0)
				{
					std::swap(B, D);
					std::swap(CB, CD);
					N = N * -1.0;
				}
				const uint32_t IA = Vert(A, N, CA), IB = Vert(B, N, CB), IC = Vert(C, N, CC), ID = Vert(D, N, CD);
				M.Triangles.insert(M.Triangles.end(), { IA, IB, IC, IA, IC, ID });
			}
			void Quad1(V3 A, V3 B, V3 C, V3 D, const Col& C1, V3 Toward) { Quad(A, B, C, D, C1, C1, C1, C1, Toward); }
			void Tri1(V3 A, V3 B, V3 C, const Col& C1, V3 Toward)
			{
				V3 N = Cross(B - A, C - A);
				if (Dot(N, N) < 1e-18) return;
				N = Norm(N);
				if (Dot(N, Toward) < 0.0)
				{
					std::swap(B, C);
					N = N * -1.0;
				}
				const uint32_t IA = Vert(A, N, C1), IB = Vert(B, N, C1), IC = Vert(C, N, C1);
				M.Triangles.insert(M.Triangles.end(), { IA, IB, IC });
			}
			/** A box: its middle and three half-extent vectors. Skip: faces left off (1 -X, 2 +X, 4 -Y, 8 +Y, 16 -Z, 32 +Z
			 * of its own axes). Top: its +Z face's own colour where given. */
			void Box(V3 O, V3 AX, V3 AY, V3 AZ, const Col& C, int Skip = 0, const Col* Top = nullptr)
			{
				const V3 P[8] = { O - AX - AY - AZ, O + AX - AY - AZ, O + AX + AY - AZ, O - AX + AY - AZ, O - AX - AY + AZ, O + AX - AY + AZ, O + AX + AY + AZ, O - AX + AY + AZ };
				const Col Side = Scaled(C, 0.94), Under = Scaled(C, 0.8);
				if (!(Skip & 4)) Quad1(P[0], P[1], P[5], P[4], Side, AY * -1.0);
				if (!(Skip & 2)) Quad1(P[1], P[2], P[6], P[5], C, AX);
				if (!(Skip & 8)) Quad1(P[2], P[3], P[7], P[6], Side, AY);
				if (!(Skip & 1)) Quad1(P[3], P[0], P[4], P[7], C, AX * -1.0);
				if (!(Skip & 32)) Quad1(P[4], P[5], P[6], P[7], Top ? *Top : C, AZ);
				if (!(Skip & 16)) Quad1(P[0], P[1], P[2], P[3], Under, AZ * -1.0);
			}
			/** A beam from A to B, W wide and H deep, its depth toward Hint's side (up where it lies level). */
			void Beam(V3 A, V3 B, double W, double H, const Col& C, V3 Hint = Up, int Skip = 0)
			{
				const V3 Along = B - A;
				const V3 Ax = Norm(Along);
				V3 Side = Cross(Hint, Ax);
				if (Dot(Side, Side) < 1e-8) Side = Cross(V3{ 1.0, 0.0, 0.0 }, Ax);
				Side = Norm(Side);
				const V3 Deep = Norm(Cross(Ax, Side));
				Box((A + B) * 0.5, Along * 0.5, Side * (0.5 * W), Deep * (0.5 * H), C, Skip);
			}
			/** A round post or pipe from A to B, radius RA at A and RB at B, its end at B capped where asked. */
			void Tube(V3 A, V3 B, double RA, double RB, int Sides, const Col& C, bool bCapB, bool bCapA = false)
			{
				const V3 Ax = Norm(B - A);
				V3 E1 = Cross(std::fabs(Ax.Z) < 0.9 ? Up : V3{ 1.0, 0.0, 0.0 }, Ax);
				E1 = Norm(E1);
				const V3 E2 = Cross(Ax, E1);
				std::vector<uint32_t> Ring;
				for (int K = 0; K <= Sides; ++K)
				{
					const double T = 2.0 * Pi * K / Sides;
					const V3 Rd = E1 * std::cos(T) + E2 * std::sin(T);
					const double Shade = 0.86 + 0.14 * std::cos(T);
					Ring.push_back(Vert(A + Rd * RA, Rd, Scaled(C, Shade)));
					Ring.push_back(Vert(B + Rd * RB, Rd, Scaled(C, Shade)));
				}
				for (int K = 0; K < Sides; ++K)
				{
					const uint32_t A0 = Ring[K * 2], B0 = Ring[K * 2 + 1], A1 = Ring[K * 2 + 2], B1 = Ring[K * 2 + 3];
					const double T = 2.0 * Pi * (K + 0.5) / Sides;
					const V3 Out = E1 * std::cos(T) + E2 * std::sin(T);
					Tri(A0, A1, B1, Out);
					Tri(A0, B1, B0, Out);
				}
				for (int End = 0; End < 2; ++End)
				{
					if (End == 0 ? !bCapA : !bCapB) continue;
					const V3 O = End == 0 ? A : B;
					const double R = End == 0 ? RA : RB;
					const V3 N = End == 0 ? Ax * -1.0 : Ax;
					const uint32_t Mid = Vert(O, N, Scaled(C, 0.9));
					std::vector<uint32_t> Rim;
					for (int K = 0; K <= Sides; ++K)
					{
						const double T = 2.0 * Pi * K / Sides;
						Rim.push_back(Vert(O + (E1 * std::cos(T) + E2 * std::sin(T)) * R, N, Scaled(C, 0.9)));
					}
					for (int K = 0; K < Sides; ++K) Tri(Mid, Rim[K], Rim[K + 1], N);
				}
			}
		};

		// ------------------------------------------------------------------ the plan

		/** A hole in a wall: along its face from A0 to A1, up from Z0 to Z1. */
		struct Opening
		{
			int Face = 0;
			double A0 = 0.0, A1 = 0.0, Z0 = 0.0, Z1 = 0.0;
			int What = 0;     // 0 a window, 1 the door, 2 an attic window
			int Shutters = 0; // 0 both open against the wall, 1 both shut, 2 one open and one ajar, 3 one hanging by a hinge, 4 none
			bool bGlass = true; // its glass whole (else gone, the room seen through it)
			int Cracked = 0;    // panes broken in it (a roof's hole's blast)
			bool bCharred = false;
			double Swing = 0.0; // the door's leaf swung in, radians (a barn's or byre's leaf, its cart door's first, its pitching
			                    // door's, its stable door's upper half: swung out)
			double Swing2 = 0.0; // a cart door's second leaf (hinged at A1), swung out
			bool bLeaf = true;  // the door's leaf there at all
			double Arch = 0.0;  // (the church's) its round head's rise over its springing, Z1 - Arch (0: square-headed)
		};

		/** What an opening is (Opening::What, LandHouse::Hole::What). */
		enum : int
		{
			OpWindow = 0,
			OpDoor = 1,
			OpAttic = 2,
			OpCart = 3,   // a barn's cart doors: two plank leaves under an oak lintel
			OpSlit = 4,   // a slit for a barn's air
			OpPitch = 5,  // a pitching door to a loft, high in a gable
			OpStable = 6, // a stable door, in two halves
			OpByreWin = 7, // a byre's small window, barred, unglazed, one plank shutter
			OpHidden = 8,  // (the church's) a stretch of wall another part of it stands against or in front of: not drawn
			OpChWin = 9,   // a church's round-headed window, leaded, under its voussoirs
			OpChDoor = 10, // a church's round-headed door, its two oak leaves shut under the arch
			OpBelfry = 11  // the tower's belfry opening, round-headed, its louvres sloping out
		};
		/** Whether an opening runs down to the floor (the plinth cut for it, no sill under it). */
		bool Grounded(int What) { return What == OpDoor || What == OpCart || What == OpStable || What == OpChDoor || What == OpHidden; }
		/** Whether it has a sill. */
		bool Silled(int What) { return What == OpWindow || What == OpAttic || What == OpSlit || What == OpPitch || What == OpByreWin || What == OpChWin || What == OpBelfry; }
		/** Where an opening's head springs: its top, or under a round head its springing line. */
		double SpringOf(const Opening& O) { return O.Z1 - O.Arch; }
		/** The depth of a round head's ring of voussoirs. */
		double RingOf(const Opening& O) { return std::max(0.26, 0.42 * 0.5 * (O.A1 - O.A0)); }

		/** A wall's face: from its corner O along T, out along N, Len long. Faces in order round the house: 0 the front
		 * (+y), 1 the gable at -x, 2 the back (-y), 3 the gable at +x; each face's far end meets the next one's start. */
		struct Face
		{
			V3 O, T, N;
			double Len = 0.0;
			bool bGable = false;
			int End = 0;  // a gable's end: -1 or +1
			int Side = 0; // a long wall's side: +1 the front, -1 the back
		};

		struct Look
		{
			Col Stone, Wall, Mortar, Dressed, Plaster, Roof, Deck, Boards, Shutter, Door, Frame, Glass, Sky, Iron, Zinc, Ridge, Pot, Char, Soot;
			Col Oak; // (a barn's cart doors: oak left to the weather, gone silver-grey)
			Col StoneMean; // (the stones' own colours averaged, for the levels that draw no stone)
		};

		struct Plan
		{
			Spec S;
			int Kind = 0;       // 0 the farmhouse, 1 a barn, 2 a byre
			double Loft = 5.6;  // the top floor's boards: the attic's, or a barn's or byre's hay loft
			double HS = 1.0;    // how far its broken walls come down, as a share of a two-storey house's
			double L = 14.0, D = 7.2;
			double E = 5.85;    // the eaves as LandField's (the house's storeys and its wall plate)
			double EW = 5.97;   // the long walls' top, under the roof's deck
			double Tan = 1.28, TanK = 0.62, SinP = 0.0, CosP = 1.0, SinK = 0.0, CosK = 1.0;
			double Vb = 3.25;   // where the main pitch breaks to the eaves' kick, out from the ridge
			double Over = 0.42; // the eaves' overhang past the wall
			double GO = 0.15;   // the verges' overhang past the gables
			double Ridge = 10.0, Ze = 5.8, Zb = 6.0, Lk = 0.0, Lm = 0.0, Ls = 0.0;
			double Thick = 0.6;
			double Gauge = 0.21, SlateW = 0.3, SlateT = 0.016;
			int Hurt = 0;
			int Broken = 0; // a fallen gable's end
			int Keep = 0;   // a shell's gable left standing with its chimney
			bool bLime = false; // its second look: cream limestone throughout (else the bocage's schist and granite rubble)
			bool bChimney[2] = { true, true }; // at -x and +x
			std::vector<double> Course; // the courses' lines, from the plinth's top up past the ridge
			std::vector<Opening> Ops;
			Look K;
			uint64_t Key = 1;
			double NL = 18.0, ND = 8.6; // (a church's part) its nave's length and depth, for where the parts stand against it
			double AbutZ[4] = { -1e9, -1e9, -1e9, -1e9 }; // (a church's part) how high another part stands against face F: no quoins at its corners below
			// the roof's hole
			int HoleSide = 1;
			double HoleX = 0.0, HoleS = 0.0, HoleRX = 1.3, HoleRS = 1.1;
		};

		Face FaceOf(const Plan& P, int F)
		{
			const double L = P.L, D = P.D;
			Face Fc;
			switch (F)
			{
			case 0:
				Fc = { { 0.5 * L, 0.5 * D, 0.0 }, { -1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, L, false, 0, 1 };
				break;
			case 1:
				Fc = { { -0.5 * L, 0.5 * D, 0.0 }, { 0.0, -1.0, 0.0 }, { -1.0, 0.0, 0.0 }, D, true, -1, 0 };
				break;
			case 2:
				Fc = { { -0.5 * L, -0.5 * D, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, -1.0, 0.0 }, L, false, 0, -1 };
				break;
			default:
				Fc = { { 0.5 * L, -0.5 * D, 0.0 }, { 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 }, D, true, 1, 0 };
				break;
			}
			if (P.Kind == 5) Fc.bGable = false; // (the church's tower: four walls to its cornice, no gable)
			return Fc;
		}
		/** A point of a face: A along it, Z up, Dd out from its outer plane (in, negative). */
		V3 At(const Face& Fc, double A, double Z, double Dd) { return Fc.O + Fc.T * A + Fc.N * Dd + V3{ 0.0, 0.0, Z }; }

		/** The roof's deck top at V out from the ridge line (the main pitch, then the eaves' kick). */
		double DeckZ(const Plan& P, double V)
		{
			V = std::fabs(V);
			if (V <= P.Vb) return P.Ridge - V * P.Tan;
			return P.Zb - (V - P.Vb) * P.TanK;
		}

		/** A point on the roof: Side +1 the front's slope, -1 the back's; X along it; Sd up the slope from the eaves' edge
		 * along its surface; H out from the deck's top. */
		struct RoofPt
		{
			V3 P, N;
		};
		RoofPt RoofAt(const Plan& P, int Side, double X, double Sd, double H)
		{
			double V, Z;
			V3 N;
			if (Sd <= P.Lk)
			{
				V = 0.5 * P.D + P.Over - Sd * P.CosK;
				Z = P.Ze + Sd * P.SinK;
				N = { 0.0, Side * P.SinK, P.CosK };
			}
			else
			{
				const double T = std::min(Sd, P.Ls) - P.Lk;
				V = P.Vb - T * P.CosP;
				Z = P.Zb + T * P.SinP;
				N = { 0.0, Side * P.SinP, P.CosP };
			}
			return { V3{ X, Side * V, Z } + N * H, N };
		}
		/** How far up the slope (Sd) a place V out from the ridge is. */
		double SlopeAt(const Plan& P, double V)
		{
			V = std::fabs(V);
			if (V >= P.Vb) return (0.5 * P.D + P.Over - V) / P.CosK;
			return P.Lk + (P.Vb - V) / P.CosP;
		}

		/** The highest course line at or below Z. */
		double Quant(const Plan& P, double Z)
		{
			if (Z < P.Course.front()) return Z;
			double Best = P.Course.front();
			for (double C : P.Course)
			{
				if (C > Z + 1e-9) break;
				Best = C;
			}
			return Best;
		}
		double Snap(const Plan& P, double Z)
		{
			double Best = P.Course.front();
			for (double C : P.Course)
				if (std::fabs(C - Z) < std::fabs(Best - Z)) Best = C;
			return Best;
		}

		/** The wall's top as built: the long walls' plate, the gables' rake under the deck. */
		double IntactTop(const Plan& P, const Face& Fc, double A)
		{
			if (!Fc.bGable || P.Kind == 5) return P.EW;
			const V3 W = At(Fc, A, 0.0, 0.0);
			return DeckZ(P, W.Y) - 0.03;
		}

		/** A broken wall's top goes by stone-sized pieces along it, 0.18 to 0.5 m long and no two alike (Scale times that
		 * on the far level): the piece of face F a place A is in. */
		double PieceLen(const Plan& P, int F, uint64_t K, double Scale)
		{
			return Scale * (0.18 + 0.32 * Unit(Hash(P.Key ^ 0xB1A5, static_cast<uint64_t>(F), K)));
		}
		void PieceOf(const Plan& P, int F, double A, double Scale, double& A0, double& A1, uint64_t* KOut = nullptr)
		{
			double X = 0.0;
			for (uint64_t K = 0; K < 2000; ++K)
			{
				const double W = PieceLen(P, F, K, Scale);
				if (A < X + W)
				{
					A0 = X;
					A1 = X + W;
					if (KOut) *KOut = K;
					return;
				}
				X += W;
			}
			A0 = X;
			A1 = X + 1.0;
			if (KOut) *KOut = 2000;
		}
		/** The pieces' edges along a face, inside it. */
		std::vector<double> PieceEdges(const Plan& P, int F, double Len, double Scale)
		{
			std::vector<double> E;
			double X = 0.0;
			for (uint64_t K = 0; K < 2000; ++K)
			{
				X += PieceLen(P, F, K, Scale);
				if (X >= Len - 0.01) break;
				E.push_back(X);
			}
			return E;
		}

		/** How far a broken wall's top bites down below its line at a place: now and then a deeper bite, rounded, some
		 * metre or two across, on no regular step (the art director's round 2: drops of 0.2 to 1.5 m). */
		double Bite(int F, double Along, uint64_t Seed)
		{
			return 0.55 * Noise01(Along / 0.5, 1.0 + 3.0 * F, Seed) + 1.1 * Smooth(0.6, 0.92, Noise01(Along / 1.05, 9.0 + 5.0 * F, Seed ^ 0xB17E));
		}

		/** A shell's last gable, its top ragged along its rake. */
		bool KeptGable(const Plan& P, const Face& Fc) { return P.Hurt == 3 && Fc.bGable && Fc.End == P.Keep; }

		/** The wall's top at a place, and whether it is a line its stones are cut to there (a shell's ragged rake, or a
		 * stone broken off at a slant or part way through its course), not a course's line. */
		struct TopHere
		{
			double Z = 0.0;
			bool bRag = false;
		};

		/**
		 * The wall's top where it stands: whole; or broken down stone by stone where it was hit or burnt, piece by piece
		 * along it, each piece's top a course's line, the stones at the steps' edges fallen or knocked off at a slant, now
		 * and then one broken part way (the art director's round 2: no stone standing alone like a merlon; round 3: not a
		 * staircase of flat treads and square risers); a shell's last gable ragged along its rake, straight from piece's
		 * edge to piece's edge, the stones under its chimney whole.
		 */
		TopHere TopAt(const Plan& P, const Face& Fc, int F, double A, double Scale = 1.0)
		{
			const double Intact = IntactTop(P, Fc, A);
			if (P.Hurt < 2) return { Intact, false };
			const uint64_t K = P.Key;
			double PA0 = 0.0, PA1 = 1.0;
			uint64_t Kp = 0;
			PieceOf(P, F, A, Scale, PA0, PA1, &Kp);
			const double T = std::clamp((A - PA0) / (PA1 - PA0), 0.0, 1.0);
			if (KeptGable(P, Fc))
			{
				auto RagAt = [&](double E) {
					const double U = Unit(Hash(K ^ 0x9A6, static_cast<uint64_t>(F), static_cast<uint64_t>(std::llround(E * 1000.0))));
					return 0.1 + 0.42 * U + 1.0 * Smooth(0.58, 0.9, Noise01(E / 0.9, 37.0, K ^ 0x9A));
				};
				const double Rag = RagAt(PA0) + (RagAt(PA1) - RagAt(PA0)) * T;
				const V3 W = At(Fc, A, 0.0, 0.0);
				// (a barn's or a byre's: each side of its apex broken down by much the same all along, a little ragged, then at
				// one break further down, so its top runs with the rake, straight from break to break, not swelling up to a
				// crag at the apex: the art director's barn round 2)
				if (P.Kind == 1 || P.Kind == 2)
				{
					auto Run = [&](double E) {
						const V3 We = At(Fc, E, 0.0, 0.0);
						const uint64_t Sd = We.Y >= 0.0 ? 1 : 0;
						const double Base = 0.3 + 0.25 * Unit(Hash(K ^ 0x9B0, static_cast<uint64_t>(F), Sd));
						const double Brk = (0.35 + 0.4 * Unit(Hash(K ^ 0x9B1, static_cast<uint64_t>(F), Sd))) * 0.5 * P.D;
						const double Jit = 0.1 * Unit(Hash(K ^ 0x9A6, static_cast<uint64_t>(F), static_cast<uint64_t>(std::llround(E * 1000.0))));
						return Base + Jit + (std::fabs(We.Y) > Brk ? 0.3 + 0.25 * Unit(Hash(K ^ 0x9B2, static_cast<uint64_t>(F), Sd)) : 0.0);
					};
					return { Intact - (Run(PA0) + (Run(PA1) - Run(PA0)) * T), true };
				}
				return { Intact - Rag * Smooth(0.45, 1.1, std::fabs(W.Y)), true };
			}
			// (a piece's line: the course its top comes down to, or the wall's own line where it stands whole)
			auto Line = [&](double Aq, bool& bWhole) {
				const double IntactQ = IntactTop(P, Fc, Aq);
				const V3 W = At(Fc, Aq, 0.0, 0.0);
				double Z = IntactQ;
				if (P.Hurt == 2)
				{
					const int B = P.Broken;
					// (a low byre's walls come down less far: HS)
					if (Fc.bGable && Fc.End == B)
						Z = P.E - 2.2 * P.HS - 1.4 * P.HS * Noise01(Aq / 1.2, 11.0, K) - P.HS * Bite(F, Aq, K ^ 0x2B1);
					else
					{
						const double In = B * W.X - (0.5 * P.L - 3.4);
						if (In > 0.0) Z = P.EW - std::min(1.0, In / 2.0) * P.HS * (0.8 + 1.5 * Noise01(W.X / 1.1, 5.0 + F, K) + Bite(F, W.X, K ^ 0x2B2));
					}
				}
				else
				{
					const double Along = Fc.bGable ? W.Y : W.X;
					Z = P.E - 0.3 * P.HS - 1.9 * P.HS * Noise01(Along / 2.4, 3.0 + 7.0 * F, K) - P.HS * Bite(F, Along, K ^ 0x2B3);
					// (the corners stand a little higher: the quoins hold)
					Z += 0.7 * std::exp(-Sq(std::min(Aq, Fc.Len - Aq) / 0.8));
				}
				bWhole = Z >= IntactQ - 0.02;
				return bWhole ? IntactQ : Quant(P, Z);
			};
			bool bWhole = false, bWL = false, bWR = false;
			const double Q = Line(0.5 * (PA0 + PA1), bWhole);
			if (bWhole) return { Intact, false };
			const double QL = Kp > 0 ? Line(PA0 - 0.5 * PieceLen(P, F, Kp - 1, Scale), bWL) : Q;
			const double QR = Line(PA1 + 0.5 * PieceLen(P, F, Kp + 1, Scale), bWR);
			const double Lo = Quant(P, Q - 0.05); // (the course line below its own)
			const int Above = (Q > QL + 1e-6 ? 1 : 0) + (Q > QR + 1e-6 ? 1 : 0);
			const double U = Unit(Hash(K ^ 0x7A6, static_cast<uint64_t>(F), Kp));
			const double V = Unit(Hash(K ^ 0x7A7, static_cast<uint64_t>(F), Kp));
			// (a piece standing above both its neighbours has nearly always lost its stone)
			if (Above == 2) return { std::min(Intact, U < 0.85 ? Lo : Q), false };
			if (Above == 1)
			{
				// (a step's edge: its stone fallen, or knocked off at a slant down toward the lower side, or still whole)
				if (U < 0.3) return { std::min(Intact, Lo), false };
				if (U < 0.78)
				{
					const bool bRight = Q > QR + 1e-6;
					const double Down = std::max(bRight ? QR : QL, Q - 0.35 - 0.3 * V);
					return { std::min(Intact, Q + (Down - Q) * (bRight ? T : 1.0 - T)), true };
				}
				return { std::min(Intact, Q), false };
			}
			// (in a flat run, now and then a stone broken off at a slant, or part way through its course)
			if (U < 0.14)
			{
				const double Low = Lo + (0.25 + 0.45 * V) * (Q - Lo);
				return { std::min(Intact, Q + (Low - Q) * (Unit(Mix(Hash(K ^ 0x7A8, static_cast<uint64_t>(F), Kp))) < 0.5 ? T : 1.0 - T)), true };
			}
			if (U < 0.22) return { std::min(Intact, Lo + (0.4 + 0.3 * V) * (Q - Lo)), true };
			return { std::min(Intact, Q), false };
		}
		double TopOf(const Plan& P, const Face& Fc, int F, double A, double Scale = 1.0) { return TopAt(P, Fc, F, A, Scale).Z; }
		bool Damaged(const Plan& P, const Face& Fc, int F, double A, double Scale = 1.0) { return TopOf(P, Fc, F, A, Scale) < IntactTop(P, Fc, A) - 1e-6; }
		/** Whether the wall's top at a place is a line its stones are cut to (a gable's rake, whole; a shell's last one
		 * ragged; a stone broken at a slant or part way), not a broken top's course line. */
		bool Sloped(const Plan& P, const Face& Fc, int F, double A, double Scale = 1.0)
		{
			if (KeptGable(P, Fc)) return true;
			if (P.Hurt >= 2)
			{
				const TopHere H = TopAt(P, Fc, F, A, Scale);
				if (H.Z < IntactTop(P, Fc, A) - 1e-6) return H.bRag;
			}
			return Fc.bGable;
		}
		/** A broken top's slant on a side of a piece's edge (where the stones' tops are cut to its line). */
		bool RagAt(const Plan& P, const Face& Fc, int F, double A, double Scale = 1.0) { return P.Hurt >= 2 && TopAt(P, Fc, F, A, Scale).bRag; }
		/** Whether a piece's edge breaks the wall's top: a step there, or a slant on either side of it. */
		bool EdgeBreaks(const Plan& P, const Face& Fc, int F, double E, double Scale = 1.0)
		{
			const TopHere L = TopAt(P, Fc, F, E - 0.005, Scale), R = TopAt(P, Fc, F, E + 0.005, Scale);
			return L.bRag || R.bRag || std::fabs(L.Z - R.Z) > 1e-6;
		}

		// ------------------------------------------------------------------ the plan from the spec

		Col StoneOwn(const Plan& P, uint64_t H);
		/** The share of a wall's face its mortar's joints take as the eye mixes them from afar, and how much darker the
		 * far level's flat wall and the middle level's stone cells are drawn than that mix, to match the near level's lit
		 * and shaded stones (measured on the picture sheet's views at the switches). */
		constexpr double WallMortar = 0.24, WallFar = 0.96, WallMid = 0.95;

		/** A barn's or a byre's openings and how each stands (MakePlan's, for Kind 1 and 2). */
		void BarnOpenings(Plan& P, Rng& R)
		{
			auto Add = [&](int F, double Mid, double Half, double Z0, double Z1, int What) {
				Opening O;
				O.Face = F;
				O.A0 = Mid - Half;
				O.A1 = Mid + Half;
				O.Z0 = Z0;
				O.Z1 = Z1;
				O.What = What;
				O.bGlass = false;
				O.Shutters = 4;
				P.Ops.push_back(O);
			};
			// (the first course line at least Rise over Z)
			auto Over = [&](double Z, double Rise) {
				for (double C : P.Course)
					if (C >= Z + Rise - 1e-6) return C;
				return P.Course.back();
			};
			const double L = P.L, D = P.D;
			if (P.Kind == 1)
			{
				// The barn: its cart doors about the middle of its front under their oak lintel, a slit either side; slits
				// along its back, now and then a stable door among them; a pitching door to the loft high in each gable, a
				// slit under it.
				const double Mid = 0.5 * L + R.Range(-0.8, 0.8);
				Add(0, Mid, 1.7, 0.0, Snap(P, 3.7), OpCart);
				for (double A : { 0.16 * L, 0.84 * L }) Add(0, A, 0.08, Snap(P, 1.6), Snap(P, 2.75), OpSlit);
				const int Bays = 3, DoorBay = R.Next() < 0.5 ? R.Below(Bays) : -1;
				for (int B = 0; B < Bays; ++B)
				{
					const double A = L * (B + 0.5) / Bays;
					if (B == DoorBay) Add(2, A, 0.55, 0.0, Snap(P, 2.05), OpStable);
					else Add(2, A, 0.08, Snap(P, 1.6), Snap(P, 2.75), OpSlit);
				}
				for (int F : { 1, 3 })
				{
					const double Z0 = Snap(P, P.Loft + 0.1);
					Add(F, 0.5 * D, 0.5, Z0, Over(Z0, 1.05), OpPitch);
					Add(F, 0.5 * D, 0.08, Snap(P, 1.5), Snap(P, 2.6), OpSlit);
				}
			}
			else
			{
				// The byre: two stable doors in its front, a small barred window beside each; two more such windows behind; a
				// pitching door to its loft high in one gable, a slit in the other.
				for (double T : { 0.3, 0.72 })
				{
					Add(0, L * T, 0.55, 0.0, Snap(P, 2.05), OpStable);
					Add(0, L * T + 1.75, 0.3, Snap(P, 1.2), Snap(P, 1.9), OpByreWin);
				}
				for (double T : { 0.25, 0.75 }) Add(2, L * T, 0.3, Snap(P, 1.2), Snap(P, 1.9), OpByreWin);
				const int PF = R.Next() < 0.5 ? 1 : 3;
				const double Z0 = Snap(P, P.Loft + 0.15);
				Add(PF, 0.5 * D, 0.45, Z0, Over(Z0, 0.9), OpPitch);
				Add(4 - PF, 0.5 * D, 0.08, Snap(P, 1.4), Snap(P, 2.4), OpSlit);
			}
			// How each stands: the leaves shut, or one standing open on the dark inside; a stable door's upper half open on
			// half of them; a byre window's shutter folded back or shut. Hit: leaves thrown open near a fallen gable, a
			// shutter hanging; a shell's burnt away.
			for (Opening& O : P.Ops)
			{
				const Face Fc = FaceOf(P, O.Face);
				const V3 Mid = At(Fc, 0.5 * (O.A0 + O.A1), 0.0, 0.0);
				const double U = R.Next();
				if (O.What == OpCart)
				{
					if (U < 0.3) O.Swing = R.Range(1.6, 2.7);
					if (U < 0.12) O.Swing2 = R.Range(1.4, 2.6);
				}
				else if (O.What == OpPitch) O.Swing = U < 0.45 ? R.Range(1.5, 2.6) : 0.0;
				else if (O.What == OpStable) O.Swing = U < 0.5 ? R.Range(1.4, 2.7) : 0.0;
				else if (O.What == OpByreWin) O.Shutters = U < 0.6 ? 0 : 1;
				if (P.Hurt == 2 && O.What != OpSlit && P.Broken * Mid.X > 0.5 * P.L - 4.5)
				{
					O.Swing = std::max(O.Swing, R.Range(0.5, 1.4));
					if (O.What == OpCart) O.Swing2 = std::max(O.Swing2, R.Range(0.3, 1.0));
					if (O.What == OpByreWin) O.Shutters = 3;
				}
				else if (P.Hurt == 3)
				{
					O.bCharred = true;
					O.bLeaf = false;
					O.Shutters = 4;
				}
			}
		}

		/** The deck of a church's nave's (Kind 3) or chancel's (4) roof, V out from its ridge, D its depth: as MakePlan builds
		 * them (where one part's roof stands against another's wall). */
		double ChurchDeck(int Kind, double D, double V)
		{
			const double Pitch = 56.0 * Pi / 180.0, Kick = Pitch - 21.0 * Pi / 180.0;
			const double EW = (Kind == 3 ? 7.5 : 6.2) + 0.12, Vb = 0.5 * D - 0.4, Tan = std::tan(Pitch), TanK = std::tan(Kick);
			const double Ridge = EW + 0.03 + Vb * Tan + (0.5 * D - Vb) * TanK, Zb = Ridge - Vb * Tan;
			V = std::fabs(V);
			return V <= Vb ? Ridge - V * Tan : Zb - (V - Vb) * TanK;
		}
		/** The church's chancel's and tower's sizes (LandField's LayChurch): the chancel 7.2 by 6.6 m east of the nave, the
		 * tower 5.8 m square west of it. */
		constexpr double ChancelL = 7.2, ChancelD = 6.6, TowerW = 5.8;

		/**
		 * A church part's openings (MakePlan's, Kind 3 to 5): round-headed, under their voussoirs. The nave's tall windows
		 * high in its long walls, three a side, its south door toward the west; the chancel's one a side and its east
		 * window; the tower's west door with a window over it, a slit in each side, two belfry openings in each face under
		 * its cornice. Where one part's wall stands in front of another's, the one behind is not drawn (OpHidden): the
		 * nave's west gable behind the tower's east wall, the chancel's west gable inside the nave's east one. (The walls
		 * that stay run on under the other part's roof, so no stone of theirs stands up through its slates.)
		 */
		void ChurchOpenings(Plan& P, Rng& R)
		{
			auto Add = [&](int F, double Mid, double Half, double Z0, double Spring, int What) {
				Opening O;
				O.Face = F;
				O.A0 = Mid - Half;
				O.A1 = Mid + Half;
				O.Z0 = Z0;
				O.Arch = Half;
				O.Z1 = Spring + Half;
				O.What = What;
				O.bGlass = What == OpChWin && Half > 0.2; // (the tower's slits open, dark)
				O.Shutters = 4;
				P.Ops.push_back(O);
			};
			auto Hide = [&](int F, double A0, double A1, double ZTop) {
				Opening O;
				O.Face = F;
				O.A0 = A0;
				O.A1 = A1;
				O.Z0 = -P.S.Below - 0.5;
				O.Z1 = ZTop;
				O.What = OpHidden;
				O.bGlass = false;
				O.bLeaf = false;
				O.Shutters = 4;
				P.Ops.push_back(O);
			};
			const double L = P.L, D = P.D;
			if (P.Kind == 3)
			{
				// The nave: three tall windows a side, high; its door in the front toward the tower.
				const double DoorA = 0.67 * L;
				for (int F : { 0, 2 })
					for (int K = 0; K < 3; ++K)
					{
						const double A = L * (K + 0.5) / 3.0;
						if (F == 0 && std::fabs(A - DoorA) < 1.4) continue;
						Add(F, A, 0.42, Snap(P, 2.7), Snap(P, 5.1), OpChWin);
					}
				Add(0, DoorA, 0.68, 0.0, Snap(P, 2.55), OpChDoor);
				Hide(1, 0.5 * D - 0.5 * TowerW - 0.02, 0.5 * D + 0.5 * TowerW + 0.02, P.Ridge + 1.0);
			}
			else if (P.Kind == 4)
			{
				// The chancel: a window a side, a wider one in its east gable; its west gable inside the nave's.
				for (int F : { 0, 2 }) Add(F, 0.5 * L, 0.38, Snap(P, 2.5), Snap(P, 4.5), OpChWin);
				Add(3, 0.5 * D, 0.52, Snap(P, 2.4), Snap(P, 4.5), OpChWin);
				Hide(1, -0.05, D + 0.05, P.Ridge + 1.0);
			}
			else
			{
				// The tower: its west door, a window over it; a slit in each side; two belfry openings in each face.
				Add(1, 0.5 * D, 0.8, 0.0, Snap(P, 2.75), OpChDoor);
				Add(1, 0.5 * D, 0.3, Snap(P, 5.4), Snap(P, 6.8), OpChWin);
				for (int F : { 0, 2 }) Add(F, 0.5 * L, 0.16, Snap(P, 8.2), Snap(P, 9.5), OpChWin);
				for (int F = 0; F < 4; ++F)
				{
					const double Len = FaceOf(P, F).Len;
					for (double T : { 0.32, 0.68 }) Add(F, Len * T, 0.36, Snap(P, P.E - 3.5), Snap(P, P.E - 1.6), OpBelfry);
				}
				// (its east wall's corners inside the nave's gable, to where the nave's roof meets them: no quoins there)
				P.AbutZ[3] = ChurchDeck(3, P.ND, 0.5 * L) - 0.05;
			}
			// (a holed nave's blast broke panes on its side)
			for (Opening& O : P.Ops)
				if (P.Hurt == 1 && O.What == OpChWin && FaceOf(P, O.Face).Side == P.HoleSide && R.Next() < 0.7) O.Cracked = 1 + R.Below(4);
		}

		Plan MakePlan(const Spec& S, double NaveL = 18.0, double NaveD = 8.6)
		{
			Plan P;
			P.S = S;
			P.Key = S.Key ? S.Key : 1;
			P.L = S.Long;
			P.D = S.Deep;
			P.Hurt = std::clamp(S.Hurt, 0, 3);
			P.Kind = std::clamp(S.Kind, 0, 5);
			P.NL = NaveL;
			P.ND = NaveD;
			Rng R(P.Key ^ 0xFA4A0);
			// (the barn's and the byre's pitch as LandField builds them, the house's by its roof, the church's steep)
			const double Pitch = (P.Kind >= 3 ? 56.0 : P.Kind != 0 ? 48.0 : S.Roof == 1 ? 47.0 : 52.0) * Pi / 180.0, Kick = Pitch - 21.0 * Pi / 180.0;
			P.Tan = std::tan(Pitch);
			P.TanK = std::tan(Kick);
			P.SinP = std::sin(Pitch);
			P.CosP = std::cos(Pitch);
			P.SinK = std::sin(Kick);
			P.CosK = std::cos(Kick);
			// (the eaves as LandField's: the house's storeys and its wall plate; the barn's 5 m, the byre's 3.3 m; the top
			// floor's boards, the attic's or the hay loft's over the cart doors and the stable doors)
			// (the church's nave 7.5 m to its eaves, its chancel 6.2 m, its tower 17.5 m to its cornice, as LandField's; a
			// nave's or a chancel's ceiling of boards at its wall plate)
			P.E = P.Kind == 1 ? 5.0 : P.Kind == 2 ? 3.3 : P.Kind == 3 ? 7.5 : P.Kind == 4 ? 6.2 : P.Kind == 5 ? 17.5 : std::max(1, S.Storeys) * 2.75 + 0.35;
			P.Loft = P.Kind == 1 ? 3.95 : P.Kind == 2 ? 2.45 : P.Kind >= 3 ? P.E - 0.35 : std::max(1, S.Storeys) * 2.75 + 0.1;
			P.HS = P.Kind == 1 ? 0.9 : P.Kind == 2 ? 0.6 : 1.0;
			P.EW = P.E + 0.12;
			P.Vb = 0.5 * P.D - 0.4;
			P.Ridge = P.EW + 0.03 + P.Vb * P.Tan + (0.5 * P.D - P.Vb) * P.TanK;
			P.Zb = P.Ridge - P.Vb * P.Tan;
			P.Ze = DeckZ(P, 0.5 * P.D + P.Over);
			P.Lk = (0.5 * P.D + P.Over - P.Vb) / P.CosK;
			P.Lm = P.Vb / P.CosP;
			P.Ls = P.Lk + P.Lm;
			if (P.Kind == 5) P.Ridge = P.EW; // (the tower: no roof of its own, its spire on its walls' top)
			if (S.Roof == 1)
			{
				P.Gauge = 0.175;
				P.SlateW = 0.27;
				P.SlateT = 0.02;
			}
			// The courses: from the plinth's top to the long walls' top in even courses, then on up the gables.
			{
				const double Z0 = 0.35;
				const int N = std::max(1, static_cast<int>(std::lround((P.EW - Z0) / 0.3)));
				std::vector<double> H(static_cast<size_t>(N));
				double Sum = 0.0;
				for (double& X : H)
				{
					X = R.Range(0.85, 1.15);
					Sum += X;
				}
				double Z = Z0;
				P.Course.push_back(Z);
				for (int I = 0; I < N; ++I)
				{
					Z += H[static_cast<size_t>(I)] / Sum * (P.EW - Z0);
					P.Course.push_back(I == N - 1 ? P.EW : Z);
				}
				Z = P.EW;
				while (Z < P.Ridge + 0.6)
				{
					Z += R.Range(0.26, 0.33);
					P.Course.push_back(Z);
				}
			}
			// Its paints.
			Look& K = P.K;
			// Its stone (the art director's round 2): the bocage's own grey-brown schist and granite rubble in a pale lime
			// mortar, its corners and openings dressed in limestone brought from the Caen plain; or, its second look, that
			// cream limestone throughout, as the farms toward Caen and the Bessin.
			P.bLime = Unit(Mix(P.Key ^ 0x11E5)) < (P.Kind >= 3 ? 0.55 : 0.35);
			const double Tone = R.Range(0.93, 1.05), Warm = R.Range(-0.03, 0.04);
			if (P.bLime)
			{
				K.Stone = Varied(Rgb(178, 167, 146), Tone, Warm);
				K.Mortar = Mixed(K.Stone, Rgb(200, 194, 176), 0.55);
				K.Dressed = Mixed(K.Stone, Rgb(206, 198, 180), 0.55);
			}
			else
			{
				K.Stone = Varied(Rgb(124, 117, 106), Tone, Warm);
				K.Mortar = Varied(Rgb(166, 161, 148), 0.97 + 0.06 * Unit(Mix(P.Key ^ 0x307)), 0.0);
				K.Dressed = Varied(Rgb(198, 190, 170), 0.97 + 0.06 * Unit(Mix(P.Key ^ 0x0D5)), 0.0);
			}
			K.Plaster = Rgb(170, 162, 146);
			K.Roof = S.Roof == 1 ? Varied(Rgb(146, 84, 62), R.Range(0.92, 1.06), 0.0) : Varied(Rgb(80, 84, 94), R.Range(0.94, 1.06), 0.0);
			K.Deck = S.Roof == 1 ? Rgb(60, 42, 34) : Rgb(36, 38, 42);
			K.Boards = Rgb(96, 80, 62);
			static const Col Paints[5] = { Rgb(96, 110, 118), Rgb(92, 106, 88), Rgb(110, 72, 60), Rgb(166, 156, 134), Rgb(104, 114, 104) };
			K.Shutter = Paints[R.Below(5)];
			// (its door: oak weathered grey-brown, the shutters' old paint, or an ox-blood red)
			const double DoorPaint = R.Next();
			K.Door = DoorPaint < 0.45 ? Rgb(146, 128, 104) : DoorPaint < 0.75 ? Mixed(K.Shutter, Rgb(150, 142, 126), 0.2) : Rgb(132, 72, 58);
			if (P.Kind >= 3) K.Door = DoorPaint < 0.6 ? Rgb(118, 100, 80) : Rgb(112, 60, 50); // (a church's oak, or its ox-blood)
			K.Frame = R.Next() < 0.6 ? Rgb(184, 180, 168) : Mixed(K.Shutter, Rgb(184, 180, 168), 0.3);
			// (the art director's barn round 2: silvered oak, grey and only a little warm, not new pine)
			K.Oak = Varied(Rgb(124, 118, 112), 0.95 + 0.1 * Unit(Mix(P.Key ^ 0x0A4)), 0.0);
			// (the glass dark, the sky it gives back pale: a pane between, lighter toward its top)
			K.Glass = Rgb(74, 84, 96);
			K.Sky = Rgb(184, 194, 206);
			K.Iron = Rgb(64, 50, 40);
			K.Zinc = Rgb(122, 126, 124);
			K.Ridge = Rgb(136, 82, 60);
			K.Pot = Rgb(148, 90, 64);
			K.Char = Rgb(30, 26, 23);
			// (the walls far off: the stones and their mortar as the eye mixes them, as the near level's lit pillows average)
			{
				Col Sum;
				for (uint64_t I = 0; I < 512; ++I)
				{
					const Col C = StoneOwn(P, Mix(I * 77 + 5));
					Sum = { Sum.R + C.R / 512.0, Sum.G + C.G / 512.0, Sum.B + C.B / 512.0 };
				}
				K.StoneMean = Sum;
				K.Wall = Scaled(Mixed(Sum, K.Mortar, WallMortar), WallFar);
			}
			K.Soot = Rgb(40, 36, 33);
			// (a church's three parts share their look; each lays its own stones, by its own key, from here)
			if (P.Kind >= 3)
			{
				P.Key = Hash(P.Key, 0xC40 + static_cast<uint64_t>(P.Kind));
				R = Rng(P.Key ^ 0xFA4A1);
			}
			// Its chimneys: both gables, or one.
			if (R.Next() < 0.3) P.bChimney[R.Below(2)] = false;
			// The damage's side.
			P.Broken = R.Next() < 0.5 ? -1 : 1;
			P.Keep = R.Next() < 0.5 ? -1 : 1;
			if (P.Hurt == 2) P.bChimney[P.Broken > 0 ? 1 : 0] = false;
			if (P.Hurt == 3)
			{
				P.bChimney[P.Keep > 0 ? 1 : 0] = true;
				P.bChimney[P.Keep > 0 ? 0 : 1] = false;
			}
			// (a barn and a byre: no chimney, no plaster inside, their rubble bare; their stone a shade rougher than the house's)
			if (P.Kind != 0) P.bChimney[0] = P.bChimney[1] = false;
			if (P.Kind == 1 || P.Kind == 2) K.Plaster = Mixed(Mixed(K.Mortar, K.StoneMean, 0.5), Rgb(92, 86, 76), 0.35);
			P.HoleSide = R.Next() < 0.5 ? -1 : 1;
			P.HoleX = R.Range(-0.5 * P.L + 2.6, 0.5 * P.L - 2.6);
			P.HoleS = R.Range(0.38, 0.62) * P.Ls;
			P.HoleRX = R.Range(1.1, 1.5);
			P.HoleRS = R.Range(0.9, 1.2);

			// The openings: the front in bays, windows over windows, the door in one of the middle bays; the back with
			// fewer; a gable without a chimney a small attic window, one with a ground-floor window. (A barn's and a byre's
			// their own.)
			if (P.Kind == 1 || P.Kind == 2) BarnOpenings(P, R);
			else if (P.Kind >= 3) ChurchOpenings(P, R);
			const int Storeys = std::max(1, S.Storeys);
			const double GZ0 = Snap(P, 0.95), GZ1 = Snap(P, 2.3);
			const double DZ1 = Snap(P, 2.65);
			auto UpperZ = [&](int Row, double& Z0, double& Z1) {
				Z0 = Snap(P, Row * 2.75 + 0.85);
				Z1 = Snap(P, Row * 2.75 + 2.1);
				if (Z1 > P.EW - 0.55) Z1 = Quant(P, P.EW - 0.55);
			};
			for (int F = 0; F < 4 && P.Kind == 0; ++F)
			{
				const Face Fc = FaceOf(P, F);
				if (!Fc.bGable)
				{
					const bool bFront = F == 0;
					const int Bays = std::max(2, static_cast<int>((Fc.Len - 1.0) / 2.75)) - (bFront ? 0 : 1);
					const int Door = bFront ? Bays / 2 - (Bays % 2 == 0 && R.Next() < 0.5 ? 1 : 0) : -1;
					for (int Row = 0; Row < Storeys; ++Row)
						for (int B = 0; B < Bays; ++B)
						{
							const double Mid = Fc.Len * (B + 0.5) / Bays;
							Opening O;
							O.Face = F;
							if (Row == 0 && B == Door)
							{
								O.What = 1;
								O.A0 = Mid - 0.58;
								O.A1 = Mid + 0.58;
								O.Z0 = 0.0;
								O.Z1 = DZ1;
							}
							else
							{
								if (!bFront && Row == 0 && B == Bays - 1 && R.Next() < 0.5) continue; // (a blank bay behind)
								const double W = Row == 0 ? 0.52 : 0.48;
								O.A0 = Mid - W;
								O.A1 = Mid + W;
								if (Row == 0)
								{
									O.Z0 = GZ0;
									O.Z1 = GZ1;
								}
								else UpperZ(Row, O.Z0, O.Z1);
							}
							P.Ops.push_back(O);
						}
				}
				else
				{
					const bool bChim = P.bChimney[Fc.End > 0 ? 1 : 0];
					Opening O;
					O.Face = F;
					if (!bChim)
					{
						O.What = 2;
						O.A0 = 0.5 * Fc.Len - 0.3;
						O.A1 = 0.5 * Fc.Len + 0.3;
						O.Z0 = Snap(P, P.EW + 0.35);
						O.Z1 = O.Z0;
						for (double C : P.Course)
							if (C >= O.Z0 + 0.6)
							{
								O.Z1 = C;
								break;
							}
						P.Ops.push_back(O);
					}
					else if (R.Next() < 0.6)
					{
						const double Mid = 0.5 * Fc.Len + (R.Next() < 0.5 ? -1.0 : 1.0) * 1.7;
						O.A0 = Mid - 0.45;
						O.A1 = Mid + 0.45;
						O.Z0 = GZ0;
						O.Z1 = GZ1;
						P.Ops.push_back(O);
					}
				}
			}
			// Each opening's shutters, glass and door as the house stands.
			for (Opening& O : P.Ops)
			{
				if (O.What >= OpCart) continue; // (a barn's and a byre's: BarnOpenings)
				const Face Fc = FaceOf(P, O.Face);
				const V3 Mid = At(Fc, 0.5 * (O.A0 + O.A1), 0.0, 0.0);
				const double U = R.Next();
				O.Shutters = O.What == 2 ? 4 : U < 0.5 ? 0 : U < 0.78 ? 1 : 2;
				if (P.Hurt == 1)
				{
					// (the blast that holed the roof broke panes on its side)
					if (O.What != 1 && Fc.Side == P.HoleSide && R.Next() < 0.6) O.Cracked = 1 + R.Below(3);
					if (O.Shutters == 2 && R.Next() < 0.5) O.Shutters = 3;
				}
				else if (P.Hurt == 2)
				{
					const bool bNear = P.Broken * Mid.X > 0.5 * P.L - 4.5;
					if (bNear || R.Next() < 0.25)
					{
						O.bGlass = false;
						if (O.Shutters != 1 && R.Next() < 0.5) O.Shutters = 3;
					}
					if (O.What == 1) O.Swing = bNear ? 1.1 : 0.35;
				}
				else if (P.Hurt == 3)
				{
					O.bGlass = false;
					O.bCharred = true;
					O.Shutters = R.Next() < 0.25 ? 3 : 4;
					O.bLeaf = false;
				}
			}
			// An opening the broken wall's top runs below the sill of is gone with it.
			if (P.Hurt >= 2)
				P.Ops.erase(std::remove_if(P.Ops.begin(), P.Ops.end(),
								[&](const Opening& O) {
									const Face Fc = FaceOf(P, O.Face);
									const double T = std::min(TopOf(P, Fc, O.Face, O.A0), TopOf(P, Fc, O.Face, O.A1));
									return T < O.Z0 + 0.5;
								}),
					P.Ops.end());
			return P;
		}

		/** Whether a gable's end has a chimney (End -1 or +1). */
		bool ChimneyAt(const Plan& P, int End) { return P.bChimney[End > 0 ? 1 : 0]; }

		/** How far into the roof's hole a place on its slope is: under 1 inside, 1 at its ragged edge. */
		double HoleDist(const Plan& P, int Side, double X, double Sd)
		{
			if (P.Hurt != 1 || Side != P.HoleSide) return 9.0;
			const double DX = (X - P.HoleX) / P.HoleRX, DS = (Sd - P.HoleS) / P.HoleRS;
			return std::sqrt(DX * DX + DS * DS) + 0.22 * Noise(X / 0.45, Sd / 0.45, P.Key ^ 0x401E);
		}
		/** A fallen gable's roof: where along the house (toward the broken end) a course of slates now ends. */
		double CutAt(const Plan& P, int Side, double Sd)
		{
			return 0.5 * P.L - 2.4 - 1.3 * Noise01(Sd / 0.55, 3.3 + Side, P.Key ^ 0xC07);
		}
		/** Whether the roof's deck is there at a place of its slope. */
		bool DeckThere(const Plan& P, int Side, double X, double Sd)
		{
			if (P.Hurt == 3) return false;
			if (HoleDist(P, Side, X, Sd) < 0.9) return false;
			if (P.Hurt == 2 && P.Broken * X > CutAt(P, Side, Sd) + 0.14) return false;
			return true;
		}

		// ------------------------------------------------------------------ colour of the walls

		/** How much of a burnt opening's soot plume is at a place on its face: all of it at the opening's head, fanning
		 * as it rises and mostly gone a metre and a half up. */
		double Plume(const Opening& O, double A, double Z)
		{
			if (Z < O.Z1 - 0.3) return 0.0;
			const double Rise = std::max(0.0, Z - O.Z1);
			const double Wide = 0.5 * (O.A1 - O.A0) + 0.18 + 0.3 * Rise;
			return std::exp(-Sq((A - 0.5 * (O.A0 + O.A1)) / Wide)) * std::exp(-std::max(0.0, Rise - 0.3) / 0.8) * Smooth(O.Z1 - 0.3, O.Z1, Z);
		}

		/** A church tower's ledges the rain runs off down its walls: its two string courses' undersides and its cornice's
		 * corbels (as ChurchExtras and TowerTop lay them). */
		std::vector<double> TowerDrips(const Plan& P)
		{
			double ZBel = P.EW;
			for (const Opening& O : P.Ops)
				if (O.What == OpBelfry) ZBel = std::min(ZBel, O.Z0);
			return { Snap(P, 7.8) - 0.16, Quant(P, ZBel - 0.25) - 0.16, P.EW - 0.42 };
		}

		/** The weather on a wall at a place: damp and green at its foot, streaks from the sills and the gutters' ends,
		 * lichen, a fire's soot above the openings and over the inside. */
		Col Weathered(const Plan& P, const Face& Fc, int F, double A, double Z, Col C, bool bFine = false)
		{
			const uint64_t K = P.Key;
			if (P.Hurt == 3) C = Scaled(C, 0.86); // (a shell's stone darkened all over by the fire)
			if (P.Kind == 5)
			{
				// (a church tower's ashlar aged, the art director's church round 2: under each ledge the rain's dirt washed down
				// the blocks, darkest under it and in runs where the ledge drips most, fading a metre or two down; a splash
				// band at the foot as high as the rain throws the mud; lichen in patches, a grey-green crust and the orange)
				for (const double ZD : TowerDrips(P))
				{
					if (Z > ZD + 1e-6) continue;
					const double Runs = Smooth(0.4, 0.78, Noise01(A / 0.42 + 11.0 * F, ZD * 1.7, K ^ 0x5E7));
					const double Reach = 0.7 + 1.6 * Noise01(A / 0.5 + 3.0 * F, ZD * 2.3, K ^ 0x5E9);
					C = Scaled(C, 1.0 - (0.07 + 0.15 * Runs) * std::exp(-(ZD - Z) / Reach));
				}
				const double Splash = 0.55 + 0.45 * Noise01(A / 0.7 + 4.0 * F, 1.3, K ^ 0x5B1);
				C = Scaled(C, 1.0 - 0.16 * Smooth(Splash + 0.15, Splash - 0.1, Z));
				C = Mixed(C, Rgb(132, 136, 112), 0.38 * Smooth(0.64, 0.8, Noise01(A / 0.55 + 9.0 * F, Z / 0.45, K ^ 0x11D)));
				C = Mixed(C, Rgb(184, 140, 72), 0.3 * Smooth(0.72, 0.86, Noise01(A / 0.35 + 13.0 * F, Z / 0.3, K ^ 0x11F)));
			}
			C = Mixed(C, Rgb(96, 98, 78), 0.55 * Smooth(1.1, 0.0, Z));
			if (bFine) C = Scaled(C, 1.0 - 0.07 * Smooth(0.2, 0.75, Noise01(A / 0.22, Z / 2.6, K ^ (0x57 + F)))); // rain streaks down the face
			else C = Scaled(C, 0.975);
			const double Lichen = Smooth(0.62, 0.82, Noise01(A / 0.9 + 3.0 * F, Z / 0.7, K ^ 0x11C));
			C = Mixed(C, Rgb(174, 172, 132), 0.3 * Lichen);
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				// (the sills' ends drip: two dark streaks under each)
				if (Silled(O.What) && Z < O.Z0)
					for (double S : { O.A0 + 0.06, O.A1 - 0.06 })
						C = Scaled(C, 1.0 - 0.22 * std::exp(-Sq((A - S) / 0.07)) * Smooth(O.Z0 - 2.2, O.Z0 - 0.1, Z));
				// (the fire's soot licked up the wall from each opening, black over its head and fanning as it rose, fading
				// over a metre or so: the art director's round 3)
				if (O.bCharred) C = Mixed(C, P.K.Soot, 0.95 * Plume(O, A, Z));
			}
			// (the gutters' ends spill down the long walls' corners)
			if (!Fc.bGable)
				for (double S : { 0.25, Fc.Len - 0.25 })
					C = Scaled(C, 1.0 - 0.16 * std::exp(-Sq((A - S) / 0.18)) * Smooth(0.0, P.EW, Z));
			// (a shell's walls sooted toward their tops, where the fire came over them; less up its last gable, above the fire)
			if (P.Hurt == 3) C = Mixed(C, P.K.Soot, (0.16 + 0.3 * Noise01(A / 1.3 + 5.0 * F, Z / 0.9, K ^ 0x5007)) * Smooth(P.EW - 4.0, P.EW - 0.6, Z) * (1.0 - 0.5 * Smooth(P.EW, P.EW + 2.5, Z)));
			if (P.Hurt == 2)
			{
				const V3 W = At(Fc, A, Z, 0.0);
				C = Mixed(C, P.K.Soot, 0.35 * Smooth(0.5 * P.L - 4.5, 0.5 * P.L - 1.0, P.Broken * W.X) * Smooth(1.0, 4.0, Z));
			}
			return C;
		}

		/** A stone's own colour. The bocage's rubble: grey-brown schist and granite, some browner or iron-stained, some
		 * grey, some dark, now and then a piece of limestone among them. The second look's: the limestone varied, some
		 * stones ferruginous, some greyer. */
		Col StoneOwn(const Plan& P, uint64_t H)
		{
			const double U = Unit(Mix(H ^ 0x5A));
			// (a church's tower: dressed limestone ashlar, each block its own, a few warmer or greyer)
			if (P.Kind == 5)
			{
				Col C = Varied(P.K.Dressed, 0.87 + 0.15 * Unit(H), (Unit(Mix(H)) - 0.5) * 0.05);
				if (U < 0.1) C = Mixed(C, Rgb(168, 150, 120), 0.35);
				else if (U < 0.18) C = Mixed(C, Rgb(150, 150, 146), 0.35);
				return C;
			}
			if (P.bLime)
			{
				Col C = Varied(P.K.Stone, 0.84 + 0.28 * Unit(H), (Unit(Mix(H)) - 0.5) * 0.08);
				if (U < 0.12) C = Mixed(C, Rgb(152, 126, 94), 0.5);
				else if (U < 0.2) C = Mixed(C, Rgb(150, 150, 146), 0.4);
				return C;
			}
			Col C = Varied(P.K.Stone, 0.82 + 0.34 * Unit(H), (Unit(Mix(H)) - 0.5) * 0.1);
			if (U < 0.12) C = Mixed(C, Rgb(138, 118, 96), 0.45);
			else if (U < 0.34) C = Mixed(C, Rgb(118, 119, 118), 0.55);
			else if (U < 0.44) C = Mixed(C, Rgb(88, 86, 82), 0.5);
			else if (U < 0.47) C = Mixed(C, Rgb(148, 110, 80), 0.4);
			else if (U < 0.51) C = Mixed(C, P.K.Dressed, 0.5);
			return C;
		}

		// ------------------------------------------------------------------ the walls, near

		/** A dressed block set in a face: A0 to A1, Z0 to Z1, its face Proud out from the wall, its edges chamfered. */
		void Block(Pen& B, const Face& Fc, double A0, double A1, double Z0, double Z1, double Proud, const Col& C, double Back = 0.004)
		{
			const double Ch = std::min({ 0.012, 0.25 * (A1 - A0), 0.25 * (Z1 - Z0) });
			const V3 O0 = At(Fc, A0, Z0, Back), O1 = At(Fc, A1, Z0, Back), O2 = At(Fc, A1, Z1, Back), O3 = At(Fc, A0, Z1, Back);
			const V3 I0 = At(Fc, A0 + Ch, Z0 + Ch, Proud), I1 = At(Fc, A1 - Ch, Z0 + Ch, Proud), I2 = At(Fc, A1 - Ch, Z1 - Ch, Proud), I3 = At(Fc, A0 + Ch, Z1 - Ch, Proud);
			B.Quad1(I0, I1, I2, I3, C, Fc.N);
			B.Quad1(O0, O1, I1, I0, Scaled(C, 0.8), Fc.N * 0.5 - Up);
			B.Quad1(O3, O2, I2, I3, Scaled(C, 1.04), Fc.N * 0.5 + Up);
			B.Quad1(O0, O3, I3, I0, Scaled(C, 0.92), Fc.N * 0.5 - Fc.T);
			B.Quad1(O1, O2, I2, I1, Scaled(C, 0.92), Fc.N * 0.5 + Fc.T);
		}

		/**
		 * A rubble stone: its corners in the mortar's plane, its middle swelling out, so the low sun picks each out. Its bed
		 * from (A0, ZB0) to (A1, ZB1), its top from (A0, ZT0) to (A1, ZT1); each corner drawn in by half a joint of lime
		 * mortar (the joints 1.5 to 3 cm) and by its own further amount, now and then a corner knocked off, so no two stones
		 * are alike and the joints run uneven (the art director's round 2). Proud: its swelling, as a share of a rubble
		 * stone's.
		 */
		void Pillow(Pen& B, const Plan& P, const Face& Fc, int F, double A0, double A1, double ZB0, double ZB1, double ZT0, double ZT1, uint64_t H, double Proud = 1.0)
		{
			const double Gap = 0.0075 + 0.0075 * Unit(H);
			double J[8]; // (the corners' insets: along the face 0 to 3, up it 4 to 7)
			for (int K = 0; K < 8; ++K) J[K] = Gap + 0.03 * std::pow(Unit(Hash(H, 0x10 + static_cast<uint64_t>(K))), 1.6);
			const uint64_t Knock = Mix(H ^ 0x4B);
			if (Unit(Knock) < 0.35)
			{
				const int C = static_cast<int>(Unit(Mix(Knock)) * 4.0) & 3;
				J[C] += 0.035;
				J[4 + C] += 0.035;
			}
			const double WA = A1 - A0, WZ = std::max(0.0, std::min(ZT0 - ZB0, ZT1 - ZB1));
			for (int K = 0; K < 4; ++K)
			{
				J[K] = std::min(J[K], 0.3 * WA);
				J[4 + K] = std::min(J[4 + K], 0.3 * WZ);
			}
			const double CA[4] = { A0 + J[0], A1 - J[1], A1 - J[2], A0 + J[3] };
			const double CZ[4] = { ZB0 + J[4], ZB1 + J[5], ZT1 - J[6], ZT0 - J[7] };
			double MA = 0.25 * (CA[0] + CA[1] + CA[2] + CA[3]), MZ = 0.25 * (CZ[0] + CZ[1] + CZ[2] + CZ[3]);
			MA += (Unit(Mix(H ^ 1)) - 0.5) * 0.3 * WA;
			MZ += (Unit(Mix(H ^ 2)) - 0.5) * 0.25 * WZ;
			// (its swelling's peak kept inside its own outline: on a stone cut to a gable's rake, a peak moved along it stood
			// up through the verge's slates, a pale wedge over the roof seen close)
			{
				auto LineZ = [&](int I0, int I1) {
					return std::fabs(CA[I1] - CA[I0]) < 1e-9 ? 0.5 * (CZ[I0] + CZ[I1]) : CZ[I0] + (CZ[I1] - CZ[I0]) * (MA - CA[I0]) / (CA[I1] - CA[I0]);
				};
				const double Bot = LineZ(0, 1), TopZ = LineZ(3, 2);
				if (TopZ > Bot) MZ = std::clamp(MZ, Bot + 0.15 * (TopZ - Bot), TopZ - 0.15 * (TopZ - Bot));
			}
			const double Bulge = (0.009 + 0.014 * Unit(Mix(H ^ 3))) * Proud;
			const Col Own = StoneOwn(P, H);
			const V3 Mid = At(Fc, MA, MZ, Bulge);
			const uint32_t IM = B.Vert(Mid, Fc.N, Scaled(Weathered(P, Fc, F, MA, MZ, Own, true), 1.02));
			uint32_t IC[4];
			for (int K = 0; K < 4; ++K)
			{
				const V3 C = At(Fc, CA[K], CZ[K], 0.003);
				const V3 Lean = Norm(C - Mid - Fc.N * Dot(C - Mid, Fc.N));
				IC[K] = B.Vert(C, Norm(Fc.N * 0.92 + Lean * 0.34), Scaled(Weathered(P, Fc, F, CA[K], CZ[K], Own, true), 0.9));
			}
			for (int K = 0; K < 4; ++K) B.Tri(IM, IC[K], IC[(K + 1) % 4], Fc.N);
		}

		/** An ashlar block (a church's tower): A0 to A1, Z0 to Z1, its face nearly flat, a little proud of thin joints, its
		 * corners square; its middle swelling a few millimetres so the low sun still finds it. */
		void Ashlar(Pen& B, const Plan& P, const Face& Fc, int F, double A0, double A1, double Z0, double Z1, uint64_t H)
		{
			const double J = 0.006;
			const Col Own = StoneOwn(P, H);
			const double MA = 0.5 * (A0 + A1) + (Unit(Mix(H ^ 1)) - 0.5) * 0.2 * (A1 - A0), MZ = 0.5 * (Z0 + Z1) + (Unit(Mix(H ^ 2)) - 0.5) * 0.2 * (Z1 - Z0);
			const V3 Mid = At(Fc, MA, MZ, 0.008 + 0.004 * Unit(Mix(H ^ 3)));
			const uint32_t IM = B.Vert(Mid, Fc.N, Scaled(Weathered(P, Fc, F, MA, MZ, Own, true), 1.01));
			const double CA[4] = { A0 + J, A1 - J, A1 - J, A0 + J }, CZ[4] = { Z0 + J, Z0 + J, Z1 - J, Z1 - J };
			uint32_t IC[4];
			for (int K = 0; K < 4; ++K)
			{
				const V3 C = At(Fc, CA[K], CZ[K], 0.003);
				const V3 Lean = Norm(C - Mid - Fc.N * Dot(C - Mid, Fc.N));
				IC[K] = B.Vert(C, Norm(Fc.N * 0.97 + Lean * 0.16), Scaled(Weathered(P, Fc, F, CA[K], CZ[K], Own, true), 0.95));
			}
			for (int K = 0; K < 4; ++K) B.Tri(IM, IC[K], IC[(K + 1) % 4], Fc.N);
		}

		/** The quoins' length on a face at a corner: long and short in turn up the corner, the two faces opposite. */
		double QuoinLen(const Plan& P, int Corner, size_t Course, bool bEnding)
		{
			const bool bLong = ((Course + static_cast<size_t>(Corner)) % 2 == 0) == bEnding;
			return (bLong ? 0.5 : 0.27) + 0.04 * Unit(Hash(P.Key, static_cast<uint64_t>(Corner * 64 + 8), Course));
		}
		double JambLen(const Plan& P, size_t Course, int Side)
		{
			return ((Course + static_cast<size_t>(Side > 0)) % 2 == 0 ? 0.36 : 0.21) + 0.03 * Unit(Hash(P.Key, 0x1A3B, Course * 2 + (Side > 0)));
		}
		constexpr double LintelOver = 0.3;
		/** How far an opening's lintel runs past it either side: a cart door's oak beam further, a slit's stone less. */
		double LintelOf(const Opening& O) { return O.What == OpCart ? 0.42 : O.What == OpSlit ? 0.2 : LintelOver; }
		/** A cart door's oak lintel's colour at a place along it (A along its face; H its own key). */
		Col OakAt(const Plan& P, double A, uint64_t H)
		{
			const Col Oak = Mixed(Rgb(112, 94, 72), Rgb(132, 128, 118), 0.35 + 0.2 * Unit(H)); // (oak gone silver-grey in the weather)
			return Varied(Oak, 0.9 + 0.14 * Noise01(A / 0.35, 3.0, P.Key ^ 0x0A7), 0.0);
		}

		/** Whether a place on face F is behind another part's wall (a church's: OpHidden), Z0 to Z1 up it. */
		bool HiddenAt(const Plan& P, int F, double A, double Z0, double Z1)
		{
			const double ZM = 0.5 * (Z0 + Z1);
			for (const Opening& O : P.Ops)
				if (O.What == OpHidden && O.Face == F && A > O.A0 && A < O.A1 && ZM > O.Z0 && ZM < O.Z1) return true;
			return false;
		}
		/** Whether a corner of face F (its start, or its end) is hidden or stands against another part, Z0 to Z1 up it: no
		 * quoins there. */
		bool CornerHid(const Plan& P, int F, bool bEnd, double Z0, double Z1)
		{
			if (P.Kind < 3) return false;
			const int G = bEnd ? (F + 1) % 4 : (F + 3) % 4; // (the face round the corner)
			const double ZM = 0.5 * (Z0 + Z1);
			if (P.AbutZ[F] > ZM || P.AbutZ[G] > ZM) return true;
			const double LF = FaceOf(P, F).Len, LG = FaceOf(P, G).Len;
			return HiddenAt(P, F, bEnd ? LF - 0.05 : 0.05, Z0, Z1) || HiddenAt(P, G, bEnd ? 0.05 : LG - 0.05, Z0, Z1);
		}
		/** The outer edge of what is dressed round a round head: its voussoirs' ring, and a door's hood mould over that. */
		double ExtradosOf(const Opening& O) { return 0.5 * (O.A1 - O.A0) + RingOf(O) + (O.What == OpChDoor ? 0.15 : 0.0); }

		/**
		 * What is dressed in course I of a face, from and to along it: its quoins, and its openings' jambs and lintels; a
		 * round head's jambs to its springing, then (near) the course's chord of its voussoirs' ring, laid by OpeningNear.
		 * Near, also the stretches behind another part's wall (no stone laid there).
		 */
		std::vector<std::pair<double, double>> DressedIn(const Plan& P, const Face& Fc, int F, size_t I, bool bNear = true)
		{
			const int CornerStart = (F + 3) % 4, CornerEnd = F;
			std::vector<std::pair<double, double>> Res;
			const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
			if (Z1 <= P.EW + 1e-6)
			{
				if (!CornerHid(P, F, false, Z0, Z1)) Res.push_back({ -1.0, QuoinLen(P, CornerStart, I, false) });
				if (!CornerHid(P, F, true, Z0, Z1)) Res.push_back({ Fc.Len - QuoinLen(P, CornerEnd, I, true), Fc.Len + 1.0 });
			}
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				if (O.What == OpHidden)
				{
					if (bNear && Z1 > O.Z0 + 1e-6 && Z0 < O.Z1 - 1e-6) Res.push_back({ O.A0, O.A1 });
					continue;
				}
				const double SP = SpringOf(O);
				if (Z0 >= O.Z0 - 1e-6 && Z1 <= SP + 1e-6) Res.push_back({ O.A0 - JambLen(P, I, -1), O.A1 + JambLen(P, I, 1) });
				else if (O.Arch > 0.0)
				{
					const double R2 = ExtradosOf(O), Lo = std::max(Z0, SP) - SP;
					if (bNear && Z0 >= SP - 1e-6 && Lo < R2)
					{
						const double HW = std::sqrt(R2 * R2 - Lo * Lo) + 0.02, AM = 0.5 * (O.A0 + O.A1);
						Res.push_back({ AM - HW, AM + HW });
					}
				}
				else if (std::fabs(Z0 - O.Z1) < 1e-6) Res.push_back({ O.A0 - LintelOf(O), O.A1 + LintelOf(O) });
			}
			return Res;
		}

		/**
		 * A flat piece of a face: A0 to A1 along it, from Z0 up to the lower of Z1 and the straight line from (A0, L0) to
		 * (A1, L1) (a rake's, a ragged rake's or a broken top's line over it), D out from the face, facing Toward; its
		 * colour at each corner Paint(A, Z).
		 */
		template <class FPaint>
		void Cut(Pen& B, const Face& Fc, double A0, double A1, double Z0, double Z1, double L0, double L1, double D, V3 Toward, FPaint Paint)
		{
			if (A1 - A0 < 1e-6 || Z1 - Z0 < 1e-6) return;
			const double Ra[4] = { A0, A1, A1, A0 }, Rz[4] = { Z0, Z0, Z1, Z1 };
			auto Under = [&](int K) { return L0 + (L1 - L0) * (Ra[K] - A0) / (A1 - A0) - Rz[K]; };
			double Pa[8], Pz[8];
			int N = 0;
			for (int K = 0; K < 4; ++K)
			{
				const int J = (K + 1) % 4;
				const double DK = Under(K), DJ = Under(J);
				if (DK >= 0.0)
				{
					Pa[N] = Ra[K];
					Pz[N] = Rz[K];
					++N;
				}
				if ((DK >= 0.0) != (DJ >= 0.0))
				{
					const double T = DK / (DK - DJ);
					Pa[N] = Ra[K] + (Ra[J] - Ra[K]) * T;
					Pz[N] = Rz[K] + (Rz[J] - Rz[K]) * T;
					++N;
				}
			}
			if (N < 3) return;
			double Area = 0.0;
			for (int K = 0; K < N; ++K) Area += Pa[K] * Pz[(K + 1) % N] - Pa[(K + 1) % N] * Pz[K];
			if (std::fabs(Area) < 4e-5) return;
			uint32_t Ix[8];
			for (int K = 0; K < N; ++K) Ix[K] = B.Vert(At(Fc, Pa[K], Pz[K], D), Toward, Paint(Pa[K], Pz[K]));
			for (int K = 1; K + 1 < N; ++K) B.Tri(Ix[0], Ix[K], Ix[K + 1], Toward);
		}

		/** A wall's top cut to a sloping line (a shell's ragged rake) where the line crosses the row Z0 to Z1: the wall's
		 * core showing, Thick deep. */
		void SlopeCap(Pen& B, const Face& Fc, double A0, double A1, double L0, double L1, double Z0, double Z1, double Thick, const Col& C)
		{
			if (std::max(L0, L1) <= Z0 + 1e-6 || std::min(L0, L1) >= Z1 - 1e-6) return;
			double SA = A0, SB = A1;
			if (std::fabs(L1 - L0) > 1e-9)
			{
				SA = A0 + (A1 - A0) * (std::clamp(L0, Z0, Z1) - L0) / (L1 - L0);
				SB = A0 + (A1 - A0) * (std::clamp(L1, Z0, Z1) - L0) / (L1 - L0);
			}
			const double ZA = L0 + (L1 - L0) * (SA - A0) / (A1 - A0), ZBt = L0 + (L1 - L0) * (SB - A0) / (A1 - A0);
			B.Quad1(At(Fc, SA, ZA, 0.0), At(Fc, SB, ZBt, 0.0), At(Fc, SB, ZBt, -Thick), At(Fc, SA, ZA, -Thick), C, Up);
		}

		/** The same top on a barn's or a byre's last gable, near: not a smooth band but the wall's stones broken there, two
		 * across its thickness, each its own length and colour, a little proud of the dark core between them (the art
		 * director's barn round 2). */
		void StoneCap(Pen& B, const Plan& P, const Face& Fc, int F, double A0, double A1, double L0, double L1, double Z0, double Z1, const Col& Core)
		{
			if (std::max(L0, L1) <= Z0 + 1e-6 || std::min(L0, L1) >= Z1 - 1e-6) return;
			double SA = A0, SB = A1;
			if (std::fabs(L1 - L0) > 1e-9)
			{
				SA = A0 + (A1 - A0) * (std::clamp(L0, Z0, Z1) - L0) / (L1 - L0);
				SB = A0 + (A1 - A0) * (std::clamp(L1, Z0, Z1) - L0) / (L1 - L0);
			}
			if (SB < SA) std::swap(SA, SB);
			auto LineZ = [&](double A) { return L0 + (L1 - L0) * (A - A0) / (A1 - A0); };
			B.Quad1(At(Fc, SA, LineZ(SA) - 0.01, 0.0), At(Fc, SB, LineZ(SB) - 0.01, 0.0), At(Fc, SB, LineZ(SB) - 0.01, -P.Thick), At(Fc, SA, LineZ(SA) - 0.01, -P.Thick), Scaled(Core, 0.62), Up);
			// (the stones: their joints on a fixed 0.22 m grid along the face, so a stone runs on across the cells' edges)
			const double G = 0.22;
			for (double S0 = std::floor(SA / G) * G; S0 < SB - 1e-6; S0 += G)
				for (int Row = 0; Row < 2; ++Row)
				{
					const uint64_t H = Hash(P.Key ^ 0x5C4, static_cast<uint64_t>(F * 2 + Row), static_cast<uint64_t>(std::llround((S0 + 50.0) * 100.0)));
					const double J = 0.07 * (Unit(Mix(H)) - 0.5);
					const double CA = std::max(SA, S0 + 0.012 + J), CB = std::min(SB, S0 + G - 0.012 + J);
					if (CB - CA < 0.04) continue;
					const double D0 = Row == 0 ? -0.015 : -0.5 * P.Thick - 0.01, D1 = Row == 0 ? -0.5 * P.Thick + 0.01 : -P.Thick + 0.02;
					const double Lift = 0.012 + 0.03 * Unit(Mix(H ^ 7));
					const Col C = Mixed(Mixed(StoneOwn(P, H), Rgb(120, 110, 96), 0.25), P.K.Soot, 0.38 + 0.25 * Unit(Mix(H ^ 9)));
					const V3 T0 = At(Fc, CA, LineZ(CA) + Lift, D0), T1 = At(Fc, CB, LineZ(CB) + Lift, D0), T2 = At(Fc, CB, LineZ(CB) + Lift, D1), T3 = At(Fc, CA, LineZ(CA) + Lift, D1);
					B.Quad1(T0, T1, T2, T3, C, Up);
					// (its edges down to the core, its face to the outside on the outer row)
					B.Quad1(At(Fc, CA, LineZ(CA) - 0.01, D0), At(Fc, CB, LineZ(CB) - 0.01, D0), T1, T0, Scaled(C, 0.85), Row == 0 ? Fc.N : Fc.N * -1.0);
					B.Quad1(At(Fc, CA, LineZ(CA) - 0.01, D1), At(Fc, CB, LineZ(CB) - 0.01, D1), T2, T3, Scaled(C, 0.8), Row == 0 ? Fc.N * -1.0 : Fc.N);
					B.Quad1(At(Fc, CA, LineZ(CA) - 0.01, D0), At(Fc, CA, LineZ(CA) - 0.01, D1), T3, T0, Scaled(C, 0.78), Fc.T * -1.0);
					B.Quad1(At(Fc, CB, LineZ(CB) - 0.01, D0), At(Fc, CB, LineZ(CB) - 0.01, D1), T2, T1, Scaled(C, 0.78), Fc.T);
				}
		}

		/** The opening's depth to its frame (or through the wall where the room is open). A barn's and a byre's: to their
		 * leaves, bars or dark, through the wall where their inside is drawn (a gable fallen, a shell). */
		double RevealOf(const Plan& P, const Opening& O, bool bInside)
		{
			// (a church's: its door's leaves well back under the arch, its glass deep in the wall, its belfry's louvres)
			if (O.What >= OpHidden) return O.What == OpChDoor ? 0.2 : O.What == OpBelfry ? 0.3 : O.What == OpChWin ? 0.26 : 0.0;
			if (O.What >= OpCart)
			{
				if (bInside && P.Hurt >= 2) return P.Thick;
				return O.What == OpSlit ? 0.38 : O.What == OpByreWin ? 0.26 : O.What == OpCart ? 0.1 : 0.12;
			}
			if (bInside && (!O.bGlass || O.What == 1)) return P.Thick;
			return O.What == 1 ? 0.12 : 0.23;
		}

		/**
		 * A barn's or a byre's inside face (where it shows: above the loft's boards under a holed roof, all of it where a
		 * gable fell or it burnt): no plaster, its rubble bare over its dark mortar, as the outside's: courses that wander,
		 * stones of every size, now and then two smaller over one another, each swelling out of wide joints with its corners
		 * drawn in unevenly; sooted where it burnt and over its openings, damp at its foot. From: the lowest it shows.
		 */
		void InnerRubble(Pen& B, const Plan& P, int F, double From)
		{
			const Face Fc = FaceOf(P, F);
			Rng R(Hash(P.Key ^ 0x1AB, static_cast<uint64_t>(F)));
			const double SD = -P.Thick - 0.006;
			const V3 In = Fc.N * -1.0;
			auto Low = [&](double A0, double A1) { return std::min({ TopOf(P, Fc, F, A0), TopOf(P, Fc, F, 0.5 * (A0 + A1)), TopOf(P, Fc, F, A1) }); };
			auto Wander = [&](double A, double Z) { return 0.035 * Noise(A / 0.45, Z * 3.1, P.Key ^ 0x1AF); };
			auto Colour = [&](uint64_t Hs, double A, double Z) {
				Col C = Scaled(StoneOwn(P, Hs), 0.84 + 0.2 * Unit(Mix(Hs ^ 0x5A)));
				if (P.Hurt == 3) C = Mixed(C, P.K.Soot, 0.28 + 0.4 * Smooth(0.5, 3.0, Z) * (0.35 + 0.65 * Noise01(A / 0.8, Z / 0.6, P.Key ^ 0x1AD)));
				else if (P.Hurt == 2) C = Mixed(C, P.K.Soot, 0.22 * Noise01(A / 0.8, Z / 0.6, P.Key ^ 0x1AE));
				for (const Opening& O : P.Ops)
					if (O.Face == F && O.bCharred) C = Mixed(C, P.K.Soot, 0.85 * Plume(O, A, Z));
				return Mixed(C, Rgb(70, 66, 58), 0.35 * Smooth(0.8, 0.0, Z));
			};
			// (one stone: its corners drawn in from its cell by half a joint and its own amount, its peak swelling into the room)
			auto Stone = [&](double A0, double A1, double ZB0, double ZB1, double ZT0, double ZT1, uint64_t Hs) {
				double J[8];
				for (int K = 0; K < 8; ++K) J[K] = 0.012 + 0.03 * std::pow(Unit(Hash(Hs, 0x20 + static_cast<uint64_t>(K))), 1.5);
				const double WA = A1 - A0, WZ = std::min(ZT0 - ZB0, ZT1 - ZB1);
				if (WA < 0.08 || WZ < 0.06) return;
				for (int K = 0; K < 4; ++K)
				{
					J[K] = std::min(J[K], 0.25 * WA);
					J[4 + K] = std::min(J[4 + K], 0.25 * WZ);
				}
				const double CA[4] = { A0 + J[0], A1 - J[1], A1 - J[2], A0 + J[3] };
				const double CZ[4] = { ZB0 + J[4], ZB1 + J[5], ZT1 - J[6], ZT0 - J[7] };
				const double MA = 0.25 * (CA[0] + CA[1] + CA[2] + CA[3]) + (Unit(Mix(Hs ^ 1)) - 0.5) * 0.25 * WA;
				const double MZ = 0.25 * (CZ[0] + CZ[1] + CZ[2] + CZ[3]) + (Unit(Mix(Hs ^ 2)) - 0.5) * 0.2 * WZ;
				const Col Own = Colour(Hs, MA, MZ);
				const V3 Mid = At(Fc, MA, MZ, SD - 0.01 - 0.014 * Unit(Mix(Hs ^ 3)));
				const uint32_t IM = B.Vert(Mid, In, Scaled(Own, 1.03));
				uint32_t IC[4];
				for (int K = 0; K < 4; ++K)
				{
					const V3 C = At(Fc, CA[K], CZ[K], SD);
					const V3 Lean = Norm(C - Mid - In * Dot(C - Mid, In));
					IC[K] = B.Vert(C, Norm(In * 0.92 + Lean * 0.34), Scaled(Own, 0.86));
				}
				for (int K = 0; K < 4; ++K) B.Tri(IM, IC[K], IC[(K + 1) % 4], In);
			};
			for (double SZ = std::max(0.02, From); SZ < P.Ridge;)
			{
				const double CH = R.Range(0.13, 0.3);
				for (double SA = P.Thick - R.Range(0.0, 0.3); SA < Fc.Len - P.Thick;)
				{
					const double SL = 0.16 + 0.5 * std::pow(R.Next(), 1.3);
					const double A0 = std::max(P.Thick, SA), A1 = std::min(Fc.Len - P.Thick, SA + SL);
					SA += SL;
					const double Z0 = SZ, Z1 = SZ + CH;
					if (A1 - A0 < 0.08 || Z1 + 0.04 > Low(A0, A1) - 0.02) continue;
					bool bHit = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && A1 > O.A0 - 0.04 && A0 < O.A1 + 0.04 && Z1 + 0.04 > O.Z0 - 0.04 && Z0 < O.Z1 + 0.04) bHit = true;
					if (bHit) continue;
					const uint64_t Hs = Hash(P.Key ^ 0x1AC, static_cast<uint64_t>(F * 4096) + static_cast<uint64_t>(std::llround(SZ * 100.0)), static_cast<uint64_t>(std::llround(SA * 100.0)));
					const double B0 = Z0 + (Z0 > From + 0.01 ? Wander(A0, Z0) : 0.0), B1 = Z0 + (Z0 > From + 0.01 ? Wander(A1, Z0) : 0.0);
					const double T0 = Z1 + Wander(A0, Z1), T1 = Z1 + Wander(A1, Z1);
					// (two smaller, one over the other, their bed between them not level)
					if (CH > 0.2 && Unit(Mix(Hs ^ 0x22)) < 0.35)
					{
						const double M0 = Z0 + (0.38 + 0.24 * Unit(Mix(Hs ^ 0x23))) * CH, M1 = Z0 + (0.38 + 0.24 * Unit(Mix(Hs ^ 0x24))) * CH;
						Stone(A0, A1, B0, B1, M0, M1, Hs ^ 0x25);
						Stone(A0, A1, M0, M1, T0, T1, Hs ^ 0x26);
					}
					else Stone(A0, A1, B0, B1, T0, T1, Hs);
				}
				SZ += CH;
			}
		}

		void WallNear(Pen& B, const Plan& P, int F, bool bInside)
		{
			const Face Fc = FaceOf(P, F);
			const int CornerStart = (F + 3) % 4, CornerEnd = F;
			auto Top = [&](double A) { return TopOf(P, Fc, F, A); };
			// (where the piece of the broken top a place is in ends)
			auto NextEdge = [&](double X) {
				double E0 = 0.0, E1 = 0.0;
				PieceOf(P, F, X + 1e-4, 1.0, E0, E1);
				return E1;
			};
			// (the lowest the top comes over a stretch of the wall)
			auto LowTop = [&](double XL, double XR) {
				double M = std::min(Top(XL + 0.01), Top(XR - 0.01));
				for (double X = XL + 0.05; X < XR - 0.01; X += 0.05) M = std::min(M, Top(X));
				return M;
			};
			// The stones, course by course, round what is dressed (the art director's round 2): rubble of every size, 13 to
			// 60 cm, a course's height or two or three smaller ones over one another, now and then a long through-stone or a
			// tall one standing up into the course above; their beds wandering a few centimetres off the courses' lines, so
			// no course runs straight, but for the plinth's top, the wall plate and beside the dressed work.
			auto Calm = [&](double A) {
				double K = Smooth(0.55, 0.85, std::min(A, Fc.Len - A));
				for (const Opening& O : P.Ops)
					if (O.Face == F) K = std::min(K, Smooth(0.35, 0.65, std::max(O.A0 - A, A - O.A1)));
				return K;
			};
			auto Wave = [&](size_t I, double A) {
				if (I == 0 || I >= P.Course.size() || std::fabs(P.Course[I] - P.EW) < 1e-6) return 0.0;
				return 0.032 * Noise(A / 0.5, 2.3 * static_cast<double>(I) + 17.0 * F, P.Key ^ 0xC0A5) * Calm(A);
			};
			// (what is dressed in a course: its quoins, and its openings' jambs and lintels)
			auto Dressed = [&](size_t I) { return DressedIn(P, Fc, F, I); };
			std::vector<std::pair<double, double>> Standing; // (the course below's tall stones, standing up into this one)
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
				if (Fc.bGable ? Z0 > P.Ridge : Z0 >= P.EW - 1e-6) break;
				std::vector<std::pair<double, double>> Res = Dressed(I);
				Res.insert(Res.end(), Standing.begin(), Standing.end());
				Standing.clear();
				const double ZUp = I + 2 < P.Course.size() ? P.Course[I + 2] : 1e9;
				const std::vector<std::pair<double, double>> ResUp = ZUp < P.EW + 1e-6 ? Dressed(I + 1) : std::vector<std::pair<double, double>>{};
				std::sort(Res.begin(), Res.end());
				std::vector<std::pair<double, double>> Free;
				double From = 0.0;
				for (const auto& [R0, R1] : Res)
				{
					if (R0 > From + 0.03) Free.push_back({ From, std::min(R0, Fc.Len) });
					From = std::max(From, R1);
				}
				if (From < Fc.Len - 0.03) Free.push_back({ From, Fc.Len });
				Rng R(Hash(P.Key, static_cast<uint64_t>(F), I));
				for (const auto& [F0, F1] : Free)
				{
					double A = F0;
					while (F1 - A > 0.02)
					{
						// (where a broken top is down below this course, no stone: on to its next piece)
						if (!Sloped(P, Fc, F, A + 0.01) && Top(A + 0.01) < Z1 - 1e-6)
						{
							A = NextEdge(A);
							continue;
						}
						// (a church's tower: ashlar, in courses as regular as its quarry gave them)
						if (P.Kind == 5)
						{
							double Ln = R.Range(0.5, 0.95);
							if (F1 - A - Ln < 0.25) Ln = F1 - A;
							if (Ln > 1.2) Ln = 0.5 * (F1 - A);
							Ashlar(B, P, Fc, F, A, A + Ln, Z0, Z1, Hash(P.Key ^ 0x570E, static_cast<uint64_t>(F * 4096) + I, static_cast<uint64_t>(std::llround(A * 100.0))));
							A += Ln;
							continue;
						}
						const double Kind = R.Next();
						double Ln = Kind < 0.05 ? R.Range(0.7, 0.95) : Kind < 0.13 ? R.Range(0.26, 0.48) : Kind < 0.5 ? R.Range(0.3, 0.72) : 0.13 + 0.47 * std::pow(R.Next(), 1.3);
						if (F1 - A - Ln < 0.14) Ln = F1 - A;
						if (Ln > 1.0) Ln = 0.5 * (F1 - A);
						// (nor across a step down in it, nor, under a ragged rake, across a piece's edge: the stone ends there)
						for (double Bd = NextEdge(A); Bd < A + Ln - 0.03; Bd = NextEdge(Bd))
							if ((!Sloped(P, Fc, F, Bd + 0.01) && Top(Bd + 0.01) < Z1 - 1e-6) || (Bd > A + 0.1 && (RagAt(P, Fc, F, Bd - 0.01) || RagAt(P, Fc, F, Bd + 0.01)) && Z1 > LowTop(A, A + Ln) - 1e-6))
							{
								Ln = Bd - A;
								break;
							}
						// (nor, up under a gable's rake, across the eaves' kick: the rake bends there, and a stone's top laid
						// straight across the bend stood above it)
						if (Fc.bGable)
							for (double E : { 0.5 * Fc.Len - P.Vb, 0.5 * Fc.Len + P.Vb })
								if (E > A + 0.03 && E < A + Ln - 0.03 && Z1 > LowTop(A, A + Ln) - 0.12) Ln = E - A;
						if (Ln < 0.06)
						{
							A += Ln;
							continue;
						}
						const double SA0 = A, SA1 = A + Ln;
						A = SA1;
						const double TA = Top(SA0 + 0.01), TB = Top(SA1 - 0.01);
						const uint64_t H = Hash(P.Key ^ 0x570E, static_cast<uint64_t>(F * 4096) + I, static_cast<uint64_t>(std::llround(SA0 * 100.0)));
						const double B0 = Z0 + Wave(I, SA0), B1 = Z0 + Wave(I, SA1), T0 = Z1 + Wave(I + 1, SA0), T1 = Z1 + Wave(I + 1, SA1);
						// (a slope's stones cut to it; a broken top's, only those under it at both their ends)
						if (Z1 > std::min(TA, TB) - 1e-6 && Sloped(P, Fc, F, SA0 + 0.01) && Sloped(P, Fc, F, SA1 - 0.01))
						{
							if (Z1 <= std::min(TA, TB) + 1e-6)
							{
								Pillow(B, P, Fc, F, SA0, SA1, B0, B1, T0, T1, H);
								continue;
							}
							double ZT0 = std::min(Z1, TA), ZT1 = std::min(Z1, TB);
							if (std::max(ZT0, ZT1) - Z0 < 0.07) continue;
							// (where the line comes down through its bed, the stone ends: none runs on past a gable's rake to
							// stand over the verge, nor past a broken top's slant)
							double CA0 = SA0, CA1 = SA1;
							const double Least = Z0 + 0.035;
							if (TA < Least && TB > TA) CA0 = SA0 + (SA1 - SA0) * (Least - TA) / (TB - TA);
							if (TB < Least && TA > TB) CA1 = SA1 - (SA1 - SA0) * (Least - TB) / (TA - TB);
							if (CA1 - CA0 < 0.06) continue;
							ZT0 = std::max(ZT0, Least);
							ZT1 = std::max(ZT1, Least);
							if (CA0 > SA0 || CA1 < SA1) Pillow(B, P, Fc, F, CA0, CA1, Z0 + Wave(I, CA0), Z0 + Wave(I, CA1), CA0 > SA0 ? Least : ZT0, CA1 < SA1 ? Least : ZT1, H);
							else Pillow(B, P, Fc, F, SA0, SA1, B0, B1, ZT0, ZT1, H);
							continue;
						}
						if (Z1 > LowTop(SA0, SA1) + 1e-6) continue;
						const double Hi = Z1 - Z0;
						if (Kind < 0.05 && Ln > 0.65)
						{
							// a through-stone, long and standing proud
							Pillow(B, P, Fc, F, SA0, SA1, B0, B1, T0, T1, H, 1.5);
						}
						else if (Kind < 0.13 && ZUp < 1e8 && (Fc.bGable || ZUp <= P.EW + 1e-6) && ZUp <= LowTop(SA0, SA1) + 1e-6
						         && std::none_of(ResUp.begin(), ResUp.end(), [&](const std::pair<double, double>& Q) { return Q.first < SA1 + 0.02 && Q.second > SA0 - 0.02; })
						         && !(Fc.bGable && (Sloped(P, Fc, F, SA0) && ZUp > std::min(Top(SA0), Top(SA1)) - 0.05)))
						{
							// a tall one, up through the course above
							Pillow(B, P, Fc, F, SA0, SA1, B0, B1, ZUp + Wave(I + 2, SA0), ZUp + Wave(I + 2, SA1), H, 1.2);
							Standing.push_back({ SA0, SA1 });
						}
						else if (Kind < 0.5 && Hi > 0.2)
						{
							// two or three smaller, one over another, their bed between them not level
							const double M0 = Z0 + (0.36 + 0.28 * Unit(Mix(H))) * Hi, M1 = Z0 + (0.36 + 0.28 * Unit(Mix(H ^ 9))) * Hi;
							auto MidAt = [&](double X) { return M0 + (M1 - M0) * (X - SA0) / Ln; };
							auto WaveAt = [&](double X, bool bTop) { return bTop ? Z1 + Wave(I + 1, X) : Z0 + Wave(I, X); };
							for (int Row = 0; Row < 2; ++Row)
							{
								const uint64_t RH = Mix(H ^ (0x22 + static_cast<uint64_t>(Row)));
								const bool bSplit = Ln > 0.38 && Unit(RH) < 0.6;
								const double Cut = SA0 + (0.35 + 0.3 * Unit(Mix(RH))) * Ln;
								const double Cuts[3] = { SA0, bSplit ? Cut : SA1, SA1 };
								for (int Piece = 0; Piece < (bSplit ? 2 : 1); ++Piece)
								{
									const double PA0 = Cuts[Piece], PA1 = Cuts[Piece + 1];
									if (Row == 0) Pillow(B, P, Fc, F, PA0, PA1, WaveAt(PA0, false), WaveAt(PA1, false), MidAt(PA0), MidAt(PA1), RH ^ static_cast<uint64_t>(Piece * 0x51));
									else Pillow(B, P, Fc, F, PA0, PA1, MidAt(PA0), MidAt(PA1), WaveAt(PA0, true), WaveAt(PA1, true), RH ^ static_cast<uint64_t>(Piece * 0x51));
								}
							}
						}
						else Pillow(B, P, Fc, F, SA0, SA1, B0, B1, T0, T1, H);
					}
				}
			}
			// The quoins: long and short blocks in turn up the corners, to the wall's top.
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
				if (Z1 > P.EW + 1e-6) break;
				const double Q0 = QuoinLen(P, CornerStart, I, false), Q1 = QuoinLen(P, CornerEnd, I, true);
				const uint64_t H = Hash(P.Key, 0x0C0, I * 4 + static_cast<uint64_t>(F));
				const Col C0 = Weathered(P, Fc, F, 0.2, Z0, Varied(P.K.Dressed, 0.94 + 0.1 * Unit(H), 0.0));
				const Col C1 = Weathered(P, Fc, F, Fc.Len - 0.2, Z0, Varied(P.K.Dressed, 0.94 + 0.1 * Unit(Mix(H)), 0.0));
				if (Z1 <= Top(0.02) + 1e-6 && !CornerHid(P, F, false, Z0, Z1)) Block(B, Fc, -0.018, Q0 - 0.008, Z0 + 0.005, Z1 - 0.005, 0.018, C0);
				if (Z1 <= Top(Fc.Len - 0.02) + 1e-6 && !CornerHid(P, F, true, Z0, Z1)) Block(B, Fc, Fc.Len - Q1 + 0.008, Fc.Len + 0.018, Z0 + 0.005, Z1 - 0.005, 0.018, C1);
			}
			// The openings' dressed jambs, lintels and sills.
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F || O.What == OpHidden) continue;
				const double TopHere = std::min(Top(O.A0 - 0.2), Top(O.A1 + 0.2));
				for (size_t I = 0; I + 1 < P.Course.size(); ++I)
				{
					const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
					if (Z1 > TopHere + 1e-6) break;
					const uint64_t H = Hash(P.Key, 0x1A, I * 64 + static_cast<uint64_t>(std::llround(O.A0 * 10.0)));
					const Col C = Varied(P.K.Dressed, 0.95 + 0.09 * Unit(H), 0.0);
					if (Z0 >= O.Z0 - 1e-6 && Z1 <= SpringOf(O) + 1e-6)
					{
						Block(B, Fc, O.A0 - JambLen(P, I, -1) + 0.008, O.A0, Z0 + 0.005, Z1 - 0.005, 0.016, Weathered(P, Fc, F, O.A0 - 0.15, Z0, C));
						Block(B, Fc, O.A1, O.A1 + JambLen(P, I, 1) - 0.008, Z0 + 0.005, Z1 - 0.005, 0.016, Weathered(P, Fc, F, O.A1 + 0.15, Z0, Varied(C, 1.02, 0.0)));
					}
					else if (std::fabs(Z0 - O.Z1) < 1e-6 && O.Arch <= 0.0) // (a round head's voussoirs: OpeningNear's)
					{
						const Col LC = Weathered(P, Fc, F, 0.5 * (O.A0 + O.A1), Z0, C);
						if (O.What == 1)
						{
							// (the door's lintel in three, its keystone proud and standing above the course)
							const double M = 0.5 * (O.A0 + O.A1);
							Block(B, Fc, O.A0 - LintelOver + 0.008, M - 0.15, Z0 + 0.005, Z1 - 0.005, 0.018, LC);
							Block(B, Fc, M + 0.15, O.A1 + LintelOver - 0.008, Z0 + 0.005, Z1 - 0.005, 0.018, Varied(LC, 0.97, 0.0));
							Block(B, Fc, M - 0.15, M + 0.15, Z0 - 0.04, Z1 + 0.06, 0.035, Varied(LC, 1.04, 0.0));
						}
						else if (O.What == OpCart)
						{
							// (the cart doors' lintel: one oak beam, silvered, its checks dark along it, its ends in the wall)
							const double BA0 = O.A0 - LintelOf(O) + 0.008, BA1 = O.A1 + LintelOf(O) - 0.008;
							Block(B, Fc, BA0, BA1, Z0 - 0.02, Z1 - 0.005, 0.03, Weathered(P, Fc, F, 0.5 * (O.A0 + O.A1), Z0, OakAt(P, 0.5 * (BA0 + BA1), H)));
							Rng RC(H ^ 0xC4EC);
							for (int K = 0; K < 3; ++K)
							{
								const double CA = RC.Range(BA0 + 0.3, BA1 - 1.2), CL = RC.Range(0.5, 1.1), CZ = RC.Range(Z0 + 0.06, Z1 - 0.08);
								B.Quad1(At(Fc, CA, CZ - 0.006, 0.0315), At(Fc, CA + CL, CZ - 0.004, 0.0315), At(Fc, CA + CL, CZ + 0.004, 0.0315), At(Fc, CA, CZ + 0.006, 0.0315), Rgb(40, 34, 28), Fc.N);
							}
						}
						else Block(B, Fc, O.A0 - LintelOf(O) + 0.008, O.A1 + LintelOf(O) - 0.008, Z0 + 0.005, Z1 - 0.005, 0.018, LC);
					}
				}
				if (Silled(O.What) && O.Z0 < TopHere)
				{
					// The sill: a slab out over the wall, its top falling to the front, a drip under it.
					const double Dp = RevealOf(P, O, bInside);
					const double SA0 = O.A0 - 0.08, SA1 = O.A1 + 0.08, ZT = O.Z0, ZB = O.Z0 - 0.09, Out = 0.07;
					const Col C = Weathered(P, Fc, F, 0.5 * (O.A0 + O.A1), O.Z0, Varied(P.K.Dressed, 0.97, 0.0));
					const V3 T0 = At(Fc, SA0, ZT, -0.02), T1 = At(Fc, SA1, ZT, -0.02), F0 = At(Fc, SA0, ZT - 0.018, Out), F1 = At(Fc, SA1, ZT - 0.018, Out);
					const V3 B0 = At(Fc, SA0, ZB, Out), B1 = At(Fc, SA1, ZB, Out), W0 = At(Fc, SA0, ZB, 0.0), W1 = At(Fc, SA1, ZB, 0.0);
					B.Quad1(T0, T1, F1, F0, Varied(C, 1.06, 0.0), Up + Fc.N * 0.2);
					B.Quad1(F0, F1, B1, B0, C, Fc.N);
					B.Quad1(B0, B1, W1, W0, Scaled(C, 0.6), Up * -1.0);
					B.Quad1(At(Fc, SA0, ZT, -0.02), F0, B0, W0, Scaled(C, 0.9), Fc.T * -1.0);
					B.Quad1(At(Fc, SA1, ZT, -0.02), F1, B1, W1, Scaled(C, 0.9), Fc.T);
					// (the reveal's floor behind it)
					B.Quad1(At(Fc, O.A0, ZT, -0.02), At(Fc, O.A1, ZT, -0.02), At(Fc, O.A1, ZT, -Dp), At(Fc, O.A0, ZT, -Dp), Scaled(C, 0.95), Up);
				}
			}
			// The mortar behind the stones (and the wall's core where it is broken): cells round the openings, cut to the
			// rake on a gable and to the broken top, course by course, where it is broken.
			std::vector<double> AB{ 0.0, Fc.Len }, ZB{ 0.35, P.EW };
			for (double A = 1.0; A < Fc.Len - 0.2; A += 1.0) AB.push_back(A);
			if (Fc.bGable)
			{
				AB.push_back(0.5 * Fc.Len);
				AB.push_back(0.5 * Fc.Len - P.Vb);
				AB.push_back(0.5 * Fc.Len + P.Vb);
				for (double A = 0.25; A < Fc.Len; A += 0.5) AB.push_back(A);
				ZB.push_back(P.Ridge + 0.1);
			}
			for (const Opening& O : P.Ops)
				if (O.Face == F)
				{
					AB.push_back(O.A0);
					AB.push_back(O.A1);
					ZB.push_back(O.Z0);
					ZB.push_back(O.Z1);
				}
			// (a broken top's steps and slants, at its pieces' edges; a shell's last gable cut to its ragged rake piece by piece)
			const bool bKept = KeptGable(P, Fc);
			if (P.Hurt >= 2)
				for (double E : PieceEdges(P, F, Fc.Len, 1.0))
					if (EdgeBreaks(P, Fc, F, E)) AB.push_back(E);
			std::sort(AB.begin(), AB.end());
			AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), AB.end());
			std::sort(ZB.begin(), ZB.end());
			ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), ZB.end());
			const bool bInnerAll = bInside && P.Hurt >= 2;
			// (a shell's: more rows, for its inside face's soot to vary up it)
			if (bInside && P.Hurt == 3)
			{
				for (double Z = 1.2; Z < P.EW - 0.3; Z += 1.2) ZB.push_back(Z);
				std::sort(ZB.begin(), ZB.end());
				ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), ZB.end());
			}
			const double InnerFrom = P.Hurt == 1 ? P.Loft : 0.0;
			for (size_t I = 0; I + 1 < AB.size(); ++I)
			{
				const double A0 = AB[I], A1 = AB[I + 1], AM = 0.5 * (A0 + A1);
				const double TM = Top(AM);
				const bool bSlope = Sloped(P, Fc, F, AM);
				const bool bCut = !bSlope && Damaged(P, Fc, F, AM);
				const bool bBrokeSlope = bSlope && (bKept || Damaged(P, Fc, F, AM));
				for (size_t J = 0; J + 1 < ZB.size(); ++J)
				{
					const double Z0 = ZB[J], Z1 = ZB[J + 1], ZM = 0.5 * (Z0 + Z1);
					bool bHole = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
					if (bHole) continue;
					// (the top's line over the cell)
					double L0 = Z1 + 1.0, L1 = Z1 + 1.0;
					if (bCut) L0 = L1 = TM;
					else if (bSlope)
					{
						L0 = Top(A0 + 1e-4);
						L1 = Top(A1 - 1e-4);
					}
					if (std::max(L0, L1) <= Z0 + 1e-6) continue;
					const double T0 = std::clamp(L0, Z0, Z1), T1 = std::clamp(L1, Z0, Z1);
					// (a shell's last gable: its ragged rake's top, the wall's core showing, sooted)
					if (bBrokeSlope)
					{
						const Col CoreC = Mixed(Mixed(P.K.Mortar, Rgb(120, 110, 96), 0.5 + 0.3 * Noise01(AM / 0.3, 2.0, P.Key ^ 0xC0F)), P.K.Soot, 0.6);
						if (P.Kind == 1 || P.Kind == 2) StoneCap(B, P, Fc, F, A0, A1, L0, L1, Z0, Z1, CoreC);
						else SlopeCap(B, Fc, A0, A1, L0, L1, Z0, Z1, P.Thick, CoreC);
					}
					auto MC = [&](double A, double Z) { return Weathered(P, Fc, F, A, Z, Varied(P.K.Mortar, 0.96 + 0.06 * Noise(A / 0.7, Z / 0.7, P.Key ^ 0x30), 0.0), true); };
					Cut(B, Fc, A0, A1, Z0, Z1, L0, L1, 0.0, Fc.N, MC);
					// (the inside's face, limewashed plaster, sooted where it burnt)
					const double IZ0 = std::max(Z0, InnerFrom);
					if (bInside && (bInnerAll || (P.Hurt == 1 && T0 > IZ0)) && std::max(T0, T1) > IZ0)
					{
						auto PC = [&](double A, double Z) {
							Col C = Varied(P.K.Plaster, 0.95 + 0.08 * Noise(A / 1.3, Z / 1.1, P.Key ^ 0x9A), 0.0);
							if (P.Hurt == 3)
							{
								C = Mixed(C, P.K.Soot, 0.5 + 0.42 * Smooth(0.3, 3.0, Z) * (0.3 + 0.7 * Noise01(A / 0.7, Z / 0.5, P.Key ^ 0x9B)));
								for (const Opening& O : P.Ops)
									if (O.Face == F) C = Mixed(C, P.K.Soot, 0.9 * Plume(O, A, Z));
							}
							else if (P.Hurt == 2) C = Mixed(C, P.K.Soot, 0.25 * Noise01(A / 0.8, Z / 0.6, P.Key ^ 0x9C));
							// (a barn's or a byre's: not plaster but its bare rubble's mortar, the stones laid over it by InnerRubble)
							if (P.Kind == 1 || P.Kind == 2) C = Scaled(C, 0.8);
							return Mixed(C, Rgb(70, 66, 58), 0.4 * Smooth(0.8, 0.0, Z));
						};
						Cut(B, Fc, A0, A1, IZ0, Z1, L0, L1, -P.Thick, Fc.N * -1.0, PC);
					}
					// (a broken top: the wall's rubble core, then the step down to the next cell)
					if (bCut && J + 1 < ZB.size() && T0 < Z1 - 1e-6 && T0 > Z0 + 1e-6)
					{
						const Col Core = Mixed(P.K.Mortar, Rgb(120, 110, 96), 0.45 + 0.3 * Noise01(AM / 0.3, 1.0, P.Key ^ 0xC0E));
						B.Quad1(At(Fc, A0, T0, 0.0), At(Fc, A1, T0, 0.0), At(Fc, A1, T0, -P.Thick), At(Fc, A0, T0, -P.Thick), P.Hurt == 3 ? Mixed(Core, P.K.Soot, 0.3) : Core, Up);
					}
				}
				// The step between this cell's top and the next one's (none along a slope).
				if (I + 2 < AB.size() && !(bSlope && Sloped(P, Fc, F, A1 + 0.005)))
				{
					const double TL = Top(A1 - 0.005), TN = Top(A1 + 0.005);
					if (std::fabs(TN - TL) > 1e-6)
					{
						const double Lo = std::min(TN, TL), Hi = std::max(TN, TL);
						const Col Core = Mixed(P.K.Mortar, Rgb(118, 108, 94), 0.5);
						B.Quad1(At(Fc, A1, Lo, 0.0), At(Fc, A1, Hi, 0.0), At(Fc, A1, Hi, -P.Thick), At(Fc, A1, Lo, -P.Thick), Scaled(Core, 0.9), Fc.T * (TN < TL ? 1.0 : -1.0));
					}
				}
			}
			// A barn's or a byre's inside: its rubble bare, stone by stone.
			if (bInside && (P.Kind == 1 || P.Kind == 2)) InnerRubble(B, P, F, InnerFrom);
			// A shell's inside: its plaster fallen away in patches, the rubble behind it showing, sooted (the art director's
			// round 3: not flat, clean plaster like card).
			if (bInside && P.Hurt == 3 && P.Kind == 0)
			{
				Rng R(Hash(P.Key ^ 0x9A7C, static_cast<uint64_t>(F)));
				const int N = static_cast<int>(Fc.Len * 1.3);
				for (int K = 0; K < N; ++K)
				{
					const double W = R.Range(0.45, 1.8), Hh = R.Range(0.35, 1.2);
					const double PA = R.Range(0.35 + 0.5 * W, std::max(0.36 + 0.5 * W, Fc.Len - 0.35 - 0.5 * W)), PZ = R.Range(0.6, 4.6);
					const uint64_t H = Hash(P.Key ^ 0x9A7D, static_cast<uint64_t>(F * 64 + K));
					if (PZ + 0.5 * Hh > LowTop(PA - 0.5 * W, PA + 0.5 * W) - 0.1) continue;
					bool bHit = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && PA + 0.5 * W > O.A0 - 0.12 && PA - 0.5 * W < O.A1 + 0.12 && PZ + 0.5 * Hh > O.Z0 - 0.12 && PZ - 0.5 * Hh < O.Z1 + 0.12) bHit = true;
					if (bHit) continue;
					auto RC = [&](double Z, uint64_t Hs) {
						const Col C = Varied(Mixed(StoneOwn(P, Hs), P.K.Soot, 0.15 + 0.25 * Smooth(0.5, 3.5, Z)), 0.85 + 0.3 * Unit(Mix(Hs ^ 0x5A)), 0.0);
						return Mixed(C, Rgb(70, 66, 58), 0.3 * Smooth(0.8, 0.0, Z));
					};
					// (the plaster's broken edge, paler than its sooted face, round a hollow of dark mortar with the rubble's
					// stones standing in it, less sooted than the plaster that covered them)
					constexpr int NV = 13;
					double Rr[NV];
					for (int J = 0; J < NV; ++J) Rr[J] = J % 2 ? 0.62 + 0.3 * R.Next() : 0.85 + 0.15 * R.Next(); // (ragged: in and out)
					auto Ring = [&](double D, double Grow, Col C) {
						const uint32_t IM = B.Vert(At(Fc, PA, PZ, D), Fc.N * -1.0, C);
						uint32_t Ix[NV];
						for (int J = 0; J < NV; ++J)
						{
							const double Ang = 2.0 * Pi * J / NV;
							Ix[J] = B.Vert(At(Fc, PA + (0.5 * W * Rr[J] + Grow) * std::cos(Ang), PZ + (0.5 * Hh * Rr[J] + Grow) * std::sin(Ang), D), Fc.N * -1.0, C);
						}
						for (int J = 0; J < NV; ++J) B.Tri(IM, Ix[J], Ix[(J + 1) % NV], Fc.N * -1.0);
					};
					Ring(-P.Thick - 0.006, 0.035, Mixed(P.K.Plaster, P.K.Soot, 0.42 + 0.2 * Smooth(0.5, 3.5, PZ)));
					Ring(-P.Thick - 0.014, 0.0, Mixed(Mixed(P.K.Mortar, P.K.Soot, 0.72), Rgb(70, 66, 58), 0.3 * Smooth(0.8, 0.0, PZ)));
					const double SD = -P.Thick - 0.024;
					for (double SZ = PZ - 0.5 * Hh; SZ < PZ + 0.5 * Hh; )
					{
						const double CH = R.Range(0.11, 0.2);
						for (double SA = PA - 0.5 * W - R.Range(0.0, 0.2); SA < PA + 0.5 * W; )
						{
							const double SL = R.Range(0.16, 0.4);
							double A0 = SA + 0.015, A1 = SA + SL - 0.015;
							const double Z0 = SZ + 0.015, Z1 = SZ + CH - 0.015;
							SA += SL;
							// (cut to the hollow, the stones at its edge going in under the plaster's broken lip)
							const double FZ = std::max(std::fabs(Z0 - PZ), std::fabs(Z1 - PZ)) / (0.5 * Hh * 0.7);
							if (FZ >= 1.0) continue;
							const double HW = 0.5 * W * 0.7 * std::sqrt(1.0 - FZ * FZ);
							A0 = std::max(A0, PA - HW);
							A1 = std::min(A1, PA + HW);
							if (A1 - A0 < 0.05) continue;
							const uint64_t Hs = Hash(H, static_cast<uint64_t>(std::llround(SA * 100.0) * 1000 + std::llround(SZ * 100.0)));
							const Col C = RC(0.5 * (Z0 + Z1), Hs);
							B.Quad1(At(Fc, A1, Z0, SD), At(Fc, A0, Z0, SD), At(Fc, A0, Z1, SD), At(Fc, A1, Z1, SD), C, Fc.N * -1.0);
						}
						SZ += CH;
					}
				}
			}
			// The plinth: a projecting band of larger, darker stone at the foot, its top weathered back, cut by the door.
			{
				// (its runs between the doors, and round what stands behind another part's wall)
				std::vector<std::pair<double, double>> Cuts;
				for (const Opening& O : P.Ops)
					if (O.Face == F && Grounded(O.What)) Cuts.push_back({ O.A0, O.A1 });
				std::sort(Cuts.begin(), Cuts.end());
				std::vector<std::pair<double, double>> Runs;
				double RunFrom = -0.05;
				for (const auto& [C0, C1] : Cuts)
				{
					if (C0 > RunFrom + 1e-6) Runs.push_back({ RunFrom, C0 });
					RunFrom = std::max(RunFrom, C1);
				}
				if (RunFrom < Fc.Len + 0.05 - 1e-6) Runs.push_back({ RunFrom, Fc.Len + 0.05 });
				for (const auto& [R0, R1] : Runs)
				{
					for (double A = R0; A < R1 - 1e-6; A += 1.2)
					{
						const double A1 = std::min(R1, A + 1.2);
						auto PC = [&](double X, double Z) { return Weathered(P, Fc, F, X, Z, Varied(Rgb(140, 132, 116), 0.92 + 0.1 * Noise(X / 0.5, Z, P.Key ^ 0x9E), 0.0)); };
						B.Quad(At(Fc, A, -P.S.Below, 0.05), At(Fc, A1, -P.S.Below, 0.05), At(Fc, A1, 0.3, 0.05), At(Fc, A, 0.3, 0.05), PC(A, -0.3), PC(A1, -0.3), PC(A1, 0.3), PC(A, 0.3), Fc.N);
						B.Quad(At(Fc, A, 0.3, 0.05), At(Fc, A1, 0.3, 0.05), At(Fc, A1, 0.36, 0.0), At(Fc, A, 0.36, 0.0), PC(A, 0.33), PC(A1, 0.33), PC(A1, 0.33), PC(A, 0.33), Fc.N + Up);
					}
					if (R0 > 0.0)
						B.Quad1(At(Fc, R0, -P.S.Below, 0.0), At(Fc, R0, -P.S.Below, 0.05), At(Fc, R0, 0.36, 0.0), At(Fc, R0, 0.3, 0.05), Rgb(120, 114, 100), Fc.T);
					if (R1 < Fc.Len)
						B.Quad1(At(Fc, R1, -P.S.Below, 0.0), At(Fc, R1, -P.S.Below, 0.05), At(Fc, R1, 0.36, 0.0), At(Fc, R1, 0.3, 0.05), Rgb(120, 114, 100), Fc.T * -1.0);
				}
			}
			// Iron anchors where the floor's beams are tied through the wall: an X at each end of the long walls.
			if (!Fc.bGable && P.Kind < 3)
				for (double A : { 0.85, Fc.Len - 0.85 })
				{
					const double Z = P.Kind != 0 ? P.Loft - 0.15 : std::max(1, P.S.Storeys) > 1 ? 2.75 : 1.9;
					if (Z + 0.3 > Top(A)) continue;
					for (int S = -1; S <= 1; S += 2)
					{
						const V3 C = At(Fc, A, Z, 0.03);
						const V3 Dg = Norm(Fc.T + Up * S);
						B.Box(C, Dg * 0.26, Norm(Cross(Fc.N, Dg)) * 0.022, Fc.N * 0.01, Rgb(92, 62, 44), 0);
					}
				}
		}

		/** A shutter's leaf: hinged at Hinge, lying along Across from it, High tall; Out the side its planks face. */
		void Leaf(Pen& B, const Plan& P, V3 Hinge, V3 Across, V3 Out, double Wide, double High, bool bPlanksSeen, bool bCharred, uint64_t H)
		{
			const double Th = 0.032;
			Col Paint = Varied(P.K.Shutter, 0.94 + 0.1 * Unit(H), 0.0);
			if (bCharred) Paint = Mixed(P.K.Char, Rgb(70, 60, 50), 0.3);
			const V3 Mid = Hinge + Across * (0.5 * Wide) + Up * (0.5 * High);
			// The leaf's body: its edges and its back.
			B.Box(Mid, Across * (0.5 * Wide), Up * (0.5 * High), Out * (0.5 * Th), Scaled(Paint, 0.75), bPlanksSeen ? 32 : 0);
			const V3 Front = Mid + Out * (0.5 * Th + 0.0015);
			if (bPlanksSeen)
			{
				// Four planks, each its own weathered paint, the gaps between them dark.
				const int N = 4;
				for (int K = 0; K < N; ++K)
				{
					const double U0 = -0.5 * Wide + Wide * K / N + 0.003, U1 = -0.5 * Wide + Wide * (K + 1) / N - 0.003;
					Col C = Varied(Paint, 0.93 + 0.12 * Unit(Mix(H + K)), 0.0);
					if (!bCharred && Unit(Mix(H ^ (0x70 + K))) < 0.25) C = Mixed(C, Rgb(150, 140, 122), 0.3); // (the paint worn to the grey wood)
					B.Quad(Front + Across * U0 - Up * (0.5 * High), Front + Across * U1 - Up * (0.5 * High), Front + Across * U1 + Up * (0.5 * High), Front + Across * U0 + Up * (0.5 * High), Scaled(C, 0.92), Scaled(C, 0.92), C, C, Out);
				}
			}
			else
			{
				// Its inside: two ledges and the brace between them, the iron straps along the ledges.
				const V3 Back = Mid + Out * (0.5 * Th);
				for (int K = -1; K <= 1; K += 2)
				{
					const V3 C = Back + Up * (K * (0.5 * High - 0.16)) + Out * 0.012;
					B.Box(C, Across * (0.5 * Wide - 0.01), Up * 0.05, Out * 0.012, Varied(Paint, 0.96, 0.0), 0);
					B.Box(C + Out * 0.0135 - Across * (0.5 * Wide - 0.32), Across * 0.3, Up * 0.016, Out * 0.0015, P.K.Iron, 0);
				}
				const V3 Lo = Back + Up * (-0.5 * High + 0.21) - Across * (0.5 * Wide - 0.08) + Out * 0.012;
				const V3 Hi = Back + Up * (0.5 * High - 0.21) + Across * (0.5 * Wide - 0.08) + Out * 0.012;
				B.Beam(Lo, Hi, 0.085, 0.024, Varied(Paint, 0.92, 0.0), Out);
			}
		}

		/** The dark of a barn's or a byre's inside, seen through what stands open where its inside is not drawn (whole, or
		 * only its roof holed): the reveal on through the wall, then a room's worth of dark back from it, its floor (at
		 * FloorZ; none below -1) catching a little light at its front, straw on it. */
		void Cave(Pen& B, const Plan& P, const Face& Fc, double A0, double A1, double Z0, double Z1, double Dp, double FloorZ, bool bRoom = false)
		{
			if (bRoom)
			{
				// (the room behind is drawn, BarnRoom: the reveal on through the wall in the day coming in, its floor the
				// beaten earth worn to the sill)
				const Col Rev = Varied(P.K.Dressed, 0.9, 0.0), RevIn = Scaled(Rev, 0.62);
				B.Quad(At(Fc, A0, Z0, -Dp), At(Fc, A0, Z0, -P.Thick), At(Fc, A0, Z1, -P.Thick), At(Fc, A0, Z1, -Dp), Rev, RevIn, RevIn, Rev, Fc.T);
				B.Quad(At(Fc, A1, Z0, -Dp), At(Fc, A1, Z0, -P.Thick), At(Fc, A1, Z1, -P.Thick), At(Fc, A1, Z1, -Dp), Rev, RevIn, RevIn, Rev, Fc.T * -1.0);
				B.Quad1(At(Fc, A0, Z1, -Dp), At(Fc, A1, Z1, -Dp), At(Fc, A1, Z1, -P.Thick), At(Fc, A0, Z1, -P.Thick), Scaled(Rev, 0.5), Up * -1.0);
				if (Z0 < 0.1) B.Quad1(At(Fc, A0, 0.02, -Dp), At(Fc, A1, 0.02, -Dp), At(Fc, A1, 0.02, -P.Thick), At(Fc, A0, 0.02, -P.Thick), Rgb(176, 152, 112), Up);
				return;
			}
			const Col Dark = Rgb(17, 16, 14), Rev = Scaled(Varied(P.K.Dressed, 0.9, 0.0), 0.45);
			const double Back = -P.Thick - 1.6, CA0 = A0 - 0.6, CA1 = A1 + 0.6, CZ1 = Z1 + 0.4;
			// (the reveal on through the wall, in its shadow)
			B.Quad(At(Fc, A0, Z0, -Dp), At(Fc, A0, Z0, -P.Thick), At(Fc, A0, Z1, -P.Thick), At(Fc, A0, Z1, -Dp), Rev, Dark, Dark, Rev, Fc.T);
			B.Quad(At(Fc, A1, Z0, -Dp), At(Fc, A1, Z0, -P.Thick), At(Fc, A1, Z1, -P.Thick), At(Fc, A1, Z1, -Dp), Rev, Dark, Dark, Rev, Fc.T * -1.0);
			B.Quad1(At(Fc, A0, Z1, -Dp), At(Fc, A1, Z1, -Dp), At(Fc, A1, Z1, -P.Thick), At(Fc, A0, Z1, -P.Thick), Dark, Up * -1.0);
			// (the room's dark: its back, its sides, its ceiling)
			B.Quad1(At(Fc, CA0, Z0 - 0.2, Back), At(Fc, CA1, Z0 - 0.2, Back), At(Fc, CA1, CZ1, Back), At(Fc, CA0, CZ1, Back), Dark, Fc.N);
			B.Quad1(At(Fc, CA0, Z0 - 0.2, -P.Thick), At(Fc, CA0, Z0 - 0.2, Back), At(Fc, CA0, CZ1, Back), At(Fc, CA0, CZ1, -P.Thick), Dark, Fc.T);
			B.Quad1(At(Fc, CA1, Z0 - 0.2, -P.Thick), At(Fc, CA1, Z0 - 0.2, Back), At(Fc, CA1, CZ1, Back), At(Fc, CA1, CZ1, -P.Thick), Dark, Fc.T * -1.0);
			B.Quad1(At(Fc, CA0, CZ1, -P.Thick), At(Fc, CA1, CZ1, -P.Thick), At(Fc, CA1, CZ1, Back), At(Fc, CA0, CZ1, Back), Dark, Up * -1.0);
			if (FloorZ < -1.0) return;
			// (its floor: beaten earth and straw, the light from the door on its front, the dark behind)
			const Col Straw = Rgb(84, 72, 50);
			for (int K = 0; K < 4; ++K)
			{
				const double D0 = -Dp + (Back + Dp) * K / 4.0, D1 = -Dp + (Back + Dp) * (K + 1) / 4.0;
				const Col C0 = Mixed(Straw, Dark, Smooth(0.0, 1.0, K / 4.0)), C1 = Mixed(Straw, Dark, Smooth(0.0, 1.0, (K + 1) / 4.0));
				B.Quad(At(Fc, CA0, FloorZ, D0), At(Fc, CA1, FloorZ, D0), At(Fc, CA1, FloorZ, D1), At(Fc, CA0, FloorZ, D1), C0, C0, C1, C1, Up);
			}
		}

		/** Loose hay heaped in a loft: a lumpy mound, pale gold, greyer and darker where it lies deep in the dark or the
		 * weather reached it; its rim sunk a little into the boards. */
		void HayMound(Pen& B, const Plan& P, V3 O, double RX, double RY, double High, double Yaw, uint64_t Seed, double Shade = 1.0)
		{
			const double C = std::cos(Yaw), S = std::sin(Yaw);
			constexpr int NA = 12, NR = 4;
			std::vector<V3> Pt;
			std::vector<Col> Cl;
			for (int R = 0; R <= NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const double T = static_cast<double>(R) / NR, Ang = 2.0 * Pi * A / NA;
					const double Lump = 1.0 + 0.22 * Noise(A * 0.9, R * 1.3, Seed);
					const double X = RX * T * std::cos(Ang) * Lump, Y = RY * T * std::sin(Ang) * Lump;
					const double Z = High * (1.0 - T * T) * (0.75 + 0.35 * Noise01(A * 1.7, R * 0.7, Seed ^ 9)) - (R == NR ? 0.05 : 0.0);
					Pt.push_back({ O.X + X * C - Y * S, O.Y + X * S + Y * C, O.Z + Z });
					// (each farm's hay its own: this year's paler, last year's greyer)
					Col K = Varied(Rgb(172, 150, 96), (0.82 + 0.26 * Noise01(A * 2.3, R * 1.7, Seed ^ 3)) * (0.95 + 0.08 * Unit(Mix(P.Key ^ 0x4A))), (Noise01(A * 1.1, R * 2.9, Seed ^ 5) - 0.5) * 0.12);
					K = Mixed(K, Rgb(128, 120, 98), 0.3 * Noise01(A * 0.7, R * 0.9, Seed ^ 7));
					Cl.push_back(Scaled(K, Shade * (0.7 + 0.3 * (1.0 - T))));
				}
			for (int R = 0; R < NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const int A1 = (A + 1) % NA;
					const size_t I0 = static_cast<size_t>(R * NA + A), I1 = static_cast<size_t>(R * NA + A1), I2 = static_cast<size_t>((R + 1) * NA + A1), I3 = static_cast<size_t>((R + 1) * NA + A);
					B.Quad(Pt[I0], Pt[I1], Pt[I2], Pt[I3], Cl[I0], Cl[I1], Cl[I2], Cl[I3], Up);
				}
		}

		/**
		 * A ledged and braced plank leaf, a barn's or a byre's: its hinge's foot at Hinge, lying along Across from it Wide,
		 * High up; Out the side its planks face (the outside, when it is shut). Its planks each their own weathered oak or old
		 * paint, worn to the grey wood here and there, dark in their joints, rotting and greening at its foot; its strap
		 * hinges across its face from the hinge's side, on their pintles; on its back its ledges and the braces between them,
		 * rising from the hinge's side. Charred where it burnt.
		 */
		void PlankLeaf(Pen& B, const Plan& P, V3 Hinge, V3 Across, V3 Out, double Wide, double High, int Planks, int Ledges, uint64_t H, bool bCharred, const Col* Own = nullptr)
		{
			const double Th = 0.042;
			// (Own: the leaf's own wood, a barn's silvered oak: its joints darker, wider, its foot greening grey higher up)
			const bool bOak = Own != nullptr && !bCharred;
			const Col Paint = bCharred ? Mixed(P.K.Char, Rgb(70, 60, 50), 0.3) : Own ? *Own : P.K.Door;
			const V3 Mid = Hinge + Across * (0.5 * Wide) + Up * (0.5 * High);
			B.Box(Mid, Across * (0.5 * Wide), Up * (0.5 * High), Out * (0.5 * Th), Scaled(Paint, bOak ? 0.26 : 0.42), 32);
			const V3 Front = Mid + Out * (0.5 * Th + 0.0015), Back = Mid - Out * (0.5 * Th);
			const double Gap = bOak ? 0.0065 : 0.004, FootH = bOak ? 0.5 : 0.35;
			// (its planks: now and then one's foot rotted short)
			for (int K = 0; K < Planks; ++K)
			{
				const double U0 = -0.5 * Wide + Wide * K / Planks + Gap, U1 = -0.5 * Wide + Wide * (K + 1) / Planks - Gap;
				const uint64_t PH = Mix(H + 31 * static_cast<uint64_t>(K));
				Col C = Varied(Paint, (bOak ? 0.9 : 0.86) + (bOak ? 0.18 : 0.24) * Unit(PH), 0.0);
				if (!bCharred && Unit(Mix(PH ^ 0x70)) < 0.3) C = Mixed(C, bOak ? Rgb(150, 148, 142) : Rgb(150, 142, 124), 0.35);
				const Col Foot = bCharred ? C : bOak ? Mixed(Scaled(C, 0.84), Rgb(92, 100, 86), 0.5) : Mixed(Scaled(C, 0.78), Rgb(74, 80, 58), 0.35);
				const double Rot = Unit(Mix(PH ^ 0x71)) < 0.2 ? 0.02 + 0.05 * Unit(Mix(PH ^ 0x72)) : 0.0;
				const V3 L0 = Front + Across * U0 - Up * (0.5 * High - Rot), L1 = Front + Across * U1 - Up * (0.5 * High - Rot);
				const V3 M0 = Front + Across * U0 - Up * (0.5 * High - FootH), M1 = Front + Across * U1 - Up * (0.5 * High - FootH);
				const V3 T0 = Front + Across * U0 + Up * (0.5 * High), T1 = Front + Across * U1 + Up * (0.5 * High);
				B.Quad(L0, L1, M1, M0, Foot, Foot, C, C, Out);
				B.Quad(M0, M1, T1, T0, C, C, Scaled(C, 1.04), Scaled(C, 1.04), Out);
				// (and its back, seen when the leaf stands open: the same plank out of the weather, browner, its joints dark)
				const Col CB = bCharred ? C : Varied(Mixed(Paint, Rgb(122, 102, 78), bOak ? 0.45 : 0.15), 0.84 + 0.14 * Unit(Mix(PH ^ 0x73)), 0.0);
				const V3 BackO = Out * -(Th + 0.003);
				B.Quad1(T0 + BackO, T1 + BackO, L1 + BackO, L0 + BackO, CB, Out * -1.0);
			}
			// (its ledges, the strap hinges along them on its face, their pintles; the braces on its back)
			std::vector<double> LZ;
			for (int K = 0; K < Ledges; ++K) LZ.push_back(-0.5 * High + 0.2 + (High - 0.4) * K / std::max(1, Ledges - 1));
			const Col Iron = bCharred ? P.K.Char : P.K.Iron;
			for (double Z : LZ)
			{
				B.Box(Back + Up * Z - Out * 0.013, Across * (0.5 * Wide - 0.02), Up * 0.06, Out * 0.013, Varied(Paint, 0.93, 0.0), 0);
				const double SL = std::min(0.8, 0.62 * Wide);
				B.Box(Front + Up * Z + Across * (-0.5 * Wide + 0.5 * SL) + Out * 0.004, Across * (0.5 * SL), Up * 0.022, Out * 0.004, Iron, 0);
				B.Tube(Hinge + Up * (Z + 0.5 * High - 0.06) - Across * 0.01, Hinge + Up * (Z + 0.5 * High + 0.06) - Across * 0.01, 0.016, 0.016, 5, Iron, true, true);
			}
			for (size_t K = 0; K + 1 < LZ.size(); ++K)
			{
				const V3 Lo = Back + Up * (LZ[K] + 0.06) - Across * (0.5 * Wide - 0.1) - Out * 0.013;
				const V3 Hi = Back + Up * (LZ[K + 1] - 0.06) + Across * (0.5 * Wide - 0.1) - Out * 0.013;
				B.Beam(Lo, Hi, 0.1, 0.026, Varied(Paint, 0.9, 0.0), Out * -1.0);
			}
		}

		/** The day a barn's or a byre's ground floor gets through what stands open in its long walls (its cart doors, a
		 * stable door's upper half, a byre's window): each one's middle along the house, its half width, its wall's side
		 * (+1 the front), how high its light starts, and how strong. None where its inside is drawn open (a gable fallen,
		 * a shell) or nothing stands open. */
		struct DayLight
		{
			double X = 0.0, HW = 0.5, Z0 = 0.0, K = 1.0;
			int Side = 1;
		};
		std::vector<DayLight> DayLights(const Plan& P)
		{
			std::vector<DayLight> Out;
			if ((P.Kind != 1 && P.Kind != 2) || P.Hurt >= 2) return Out;
			for (const Opening& O : P.Ops)
			{
				const Face Fc = FaceOf(P, O.Face);
				if (Fc.bGable) continue;
				auto XOf = [&](double A) { return At(Fc, A, 0.0, 0.0).X; };
				DayLight L;
				L.Side = Fc.Side;
				if (O.What == OpCart && (!O.bLeaf || O.Swing > 0.01 || O.Swing2 > 0.01))
				{
					const double AM = 0.5 * (O.A0 + O.A1);
					const bool bA = !O.bLeaf || O.Swing > 0.01, bB = !O.bLeaf || O.Swing2 > 0.01;
					const double A0 = bA ? O.A0 : AM, A1 = bB ? O.A1 : AM;
					L.X = 0.5 * (XOf(A0) + XOf(A1));
					L.HW = 0.5 * (A1 - A0);
				}
				else if (O.What == OpStable && (!O.bLeaf || O.Swing > 0.01))
				{
					L.X = XOf(0.5 * (O.A0 + O.A1));
					L.HW = 0.5 * (O.A1 - O.A0);
					L.K = 0.85;
					if (O.bLeaf) L.Z0 = O.Z0 + 0.56 * (O.Z1 - O.Z0);
				}
				else if (O.What == OpByreWin && O.Shutters != 1)
				{
					L.X = XOf(0.5 * (O.A0 + O.A1));
					L.HW = 0.5 * (O.A1 - O.A0);
					L.Z0 = O.Z0;
					L.K = 0.3;
				}
				else continue;
				Out.push_back(L);
			}
			return Out;
		}
		/** How much of the day reaches a place inside, 0 to 1 (1 at an open door's sill): on the floor, falling off some
		 * 5 m in and spreading as it goes; on what stands up facing the doors (bFloor false), further. */
		double DayAt(const Plan& P, const std::vector<DayLight>& Ls, double X, double Y, bool bFloor)
		{
			const double IY = 0.5 * P.D - P.Thick;
			double S = 0.0;
			for (const DayLight& L : Ls)
			{
				const double D = std::max(0.0, IY - L.Side * Y), E = std::max(0.0, std::fabs(X - L.X) - L.HW);
				if (bFloor)
				{
					double C = L.K * std::exp(-D / 1.7) * std::exp(-Sq(E / (0.6 + 0.35 * D)));
					if (L.Z0 > 0.3) C *= Smooth(0.0, 0.6 * L.Z0, D); // (a stable door's lower leaf shades the floor just inside)
					S += C;
				}
				else S += L.K * std::exp(-D / 4.5) * std::exp(-Sq(E / (1.0 + 0.4 * D)));
			}
			return std::min(1.0, S);
		}

		/**
		 * A barn's or a byre's ground floor, seen through what stands open (the art director's barn round 2: not a black
		 * hole): the day falling in a pool on its floor from each open door, bright at the sill and fading some 5 m in;
		 * beaten earth with old straw strewn over it and a heap of it; the walls' stones faint in the half light; the
		 * loft's boards and joists overhead. The barn's posts under the loft's beam, a ladder up to it, a cart wheel against
		 * its wall; the byre's manger and hay rack along its back wall and the partitions between its standings.
		 */
		void BarnRoom(Pen& B, const Plan& P, const std::vector<DayLight>& Ls)
		{
			if (Ls.empty()) return;
			const double IX = 0.5 * P.L - P.Thick, IY = 0.5 * P.D - P.Thick, Ceil = P.Loft - 0.04;
			const uint64_t K0 = P.Key ^ 0xB4A2;
			// (as the art director measured it: the floor some 60 to 90 at the sill, 20 to 30 by 4 to 6 m in; the back wall's
			// stones 20 to 35; the byre's rack and partitions 25 to 40)
			auto FloorK = [&](double X, double Y) { return 0.6 + 0.66 * std::pow(DayAt(P, Ls, X, Y, true), 0.6); };
			auto StandK = [&](double X, double Y) { return 0.85 + 0.45 * std::pow(DayAt(P, Ls, X, Y, false), 0.6); };
			// The floor: beaten earth and old straw, in cells, each corner in its own light.
			{
				const int NX = std::max(1, static_cast<int>(std::ceil(2.0 * IX / 0.6))), NY = std::max(1, static_cast<int>(std::ceil(2.0 * IY / 0.6)));
				auto FC = [&](double X, double Y) {
					const Col C = Mixed(Rgb(180, 156, 114), Rgb(136, 118, 92), Noise01(X / 0.7, Y / 0.7, K0));
					return Scaled(C, FloorK(X, Y));
				};
				// (beyond the day's reach the cells go three to a quad across the room: the same dark floor for fewer triangles)
				auto YOf = [&](int J) { return -IY + 2.0 * IY * J / NY; };
				for (int I = 0; I < NX; ++I)
				{
					const double X0 = -IX + 2.0 * IX * I / NX, X1 = -IX + 2.0 * IX * (I + 1) / NX;
					auto Dark = [&](int J) { return DayAt(P, Ls, X0, YOf(J), true) < 0.004 && DayAt(P, Ls, X1, YOf(J), true) < 0.004; };
					for (int J = 0; J < NY;)
					{
						int J1 = J + 1;
						while (J1 < NY && J1 - J < 3 && Dark(J) && Dark(J1) && Dark(J1 + 1)) ++J1;
						const double Y0 = YOf(J), Y1 = YOf(J1);
						B.Quad(V3{ X0, Y0, 0.02 }, V3{ X1, Y0, 0.02 }, V3{ X1, Y1, 0.02 }, V3{ X0, Y1, 0.02 }, FC(X0, Y0), FC(X1, Y0), FC(X1, Y1), FC(X0, Y1), Up);
						J = J1;
					}
				}
			}
			// The long walls' inner faces: dark mortar, the stones laid flat in it where the day reaches them across from a
			// door; plain beyond; the wall's own openings left open.
			const Col Mort = Mixed(P.K.Mortar, Rgb(70, 64, 56), 0.5);
			struct Gap
			{
				double X0, X1, Z0, Z1;
			};
			auto GapsOf = [&](int F) {
				std::vector<Gap> G;
				const Face Fc = FaceOf(P, F);
				for (const Opening& O : P.Ops)
					if (O.Face == F)
					{
						const double XA = Fc.bGable ? At(Fc, O.A0, 0.0, 0.0).Y : At(Fc, O.A0, 0.0, 0.0).X, XB = Fc.bGable ? At(Fc, O.A1, 0.0, 0.0).Y : At(Fc, O.A1, 0.0, 0.0).X;
						G.push_back({ std::min(XA, XB), std::max(XA, XB), O.Z0, O.Z1 });
					}
				return G;
			};
			// (a strip of an inner face from U0 to U1 along it, floor to the boards, round the openings in it)
			auto Strips = [&](const std::vector<Gap>& G, double U0, double U1, double Step, auto&& Draw) {
				std::vector<double> Us;
				for (double U = U0; U < U1 - 1e-6; U += Step) Us.push_back(U);
				Us.push_back(U1);
				for (const Gap& Gp : G)
					for (double E : { Gp.X0, Gp.X1 })
						if (E > U0 + 1e-3 && E < U1 - 1e-3) Us.push_back(E);
				std::sort(Us.begin(), Us.end());
				for (size_t I = 0; I + 1 < Us.size(); ++I)
				{
					const double A = Us[I], Bq = Us[I + 1], M = 0.5 * (A + Bq);
					if (Bq - A < 1e-3) continue;
					std::vector<std::pair<double, double>> Zs{ { 0.0, Ceil } };
					for (const Gap& Gp : G)
						if (M > Gp.X0 && M < Gp.X1)
						{
							std::vector<std::pair<double, double>> Nz;
							for (const auto& [Z0, Z1] : Zs)
							{
								if (Gp.Z0 > Z0) Nz.push_back({ Z0, std::min(Z1, Gp.Z0) });
								if (Gp.Z1 < Z1) Nz.push_back({ std::max(Z0, Gp.Z1), Z1 });
							}
							Zs = Nz;
						}
					for (const auto& [Z0, Z1] : Zs)
						if (Z1 > Z0 + 1e-3) Draw(A, Bq, Z0, Z1);
				}
			};
			auto InGap = [](const std::vector<Gap>& G, double X0, double X1, double Z0, double Z1) {
				for (const Gap& Gp : G)
					if (X1 > Gp.X0 - 0.02 && X0 < Gp.X1 + 0.02 && Z1 > Gp.Z0 - 0.02 && Z0 < Gp.Z1 + 0.02) return true;
				return false;
			};
			for (int WS = -1; WS <= 1; WS += 2)
			{
				const double Y = WS * IY;
				const V3 N{ 0.0, -static_cast<double>(WS), 0.0 };
				const std::vector<Gap> G = GapsOf(WS > 0 ? 0 : 2);
				std::vector<std::pair<double, double>> Spans;
				for (const DayLight& L : Ls)
					if (L.Side == -WS) Spans.push_back({ std::max(-IX, L.X - L.HW - 2.2), std::min(IX, L.X + L.HW + 2.2) });
				std::sort(Spans.begin(), Spans.end());
				std::vector<std::pair<double, double>> Merged;
				for (const auto& Sp : Spans)
				{
					if (!Merged.empty() && Sp.first <= Merged.back().second) Merged.back().second = std::max(Merged.back().second, Sp.second);
					else Merged.push_back(Sp);
				}
				Strips(G, -IX, IX, 0.6, [&](double X, double X1, double Z0, double Z1) {
					const Col C0 = Scaled(Mort, StandK(X, Y) * 0.8), C1 = Scaled(Mort, StandK(X1, Y) * 0.8);
					B.Quad(V3{ X, Y, Z0 }, V3{ X1, Y, Z0 }, V3{ X1, Y, Z1 }, V3{ X, Y, Z1 }, C0, C1, C1, C0, N);
				});
				Rng R(Hash(K0, static_cast<uint64_t>(WS + 1)));
				for (const auto& [SX0, SX1] : Merged)
					for (double Z = 0.04; Z < Ceil - 0.12;)
					{
						const double CH = std::min(R.Range(0.16, 0.3), Ceil - 0.02 - Z);
						for (double X = SX0 + R.Range(0.0, 0.2); X < SX1 - 0.1;)
						{
							const double SL = std::min(R.Range(0.26, 0.72), SX1 - X);
							if (InGap(G, X, X + SL, Z, Z + CH))
							{
								X += SL;
								continue;
							}
							const uint64_t H = Hash(K0 ^ 0x57, static_cast<uint64_t>(std::llround((X + 50.0) * 100.0)), static_cast<uint64_t>(std::llround(Z * 100.0)) + static_cast<uint64_t>(WS + 1) * 100000);
							const Col C = Scaled(Mixed(StoneOwn(P, H), Rgb(96, 90, 80), 0.25), StandK(X + 0.5 * SL, Y) * (0.88 + 0.2 * Unit(Mix(H))));
							const double In = 0.012 + 0.012 * Unit(Mix(H ^ 3));
							B.Quad1(V3{ X + 0.015, Y - WS * In, Z + 0.015 }, V3{ X + SL - 0.015, Y - WS * In, Z + 0.015 }, V3{ X + SL - 0.015, Y - WS * In, Z + CH - 0.015 }, V3{ X + 0.015, Y - WS * In, Z + CH - 0.015 }, C, N);
							X += SL;
						}
						Z += CH;
					}
			}
			// The gables' inner faces, plain in the half dark.
			for (int GS = -1; GS <= 1; GS += 2)
			{
				const double X = GS * IX;
				Strips(GapsOf(GS > 0 ? 3 : 1), -IY, IY, 0.8, [&](double Y, double Y1, double Z0, double Z1) {
					const Col C0 = Scaled(Mixed(Mort, P.K.StoneMean, 0.4), StandK(X, Y) * 0.75), C1 = Scaled(Mixed(Mort, P.K.StoneMean, 0.4), StandK(X, Y1) * 0.75);
					B.Quad(V3{ X, Y, Z0 }, V3{ X, Y1, Z0 }, V3{ X, Y1, Z1 }, V3{ X, Y, Z1 }, C0, C1, C1, C0, V3{ -static_cast<double>(GS), 0.0, 0.0 });
				});
			}
			// Overhead: the loft's boards on their joists (where its roof is holed, InsideNear lays them).
			if (P.Hurt == 0)
			{
				for (double X = -IX; X < IX - 1e-6; X += 0.9)
				{
					const double X1 = std::min(IX, X + 0.9);
					B.Quad1(V3{ X, -IY, Ceil }, V3{ X1, -IY, Ceil }, V3{ X1, IY, Ceil }, V3{ X, IY, Ceil }, Scaled(P.K.Boards, 0.55 * StandK(X, 0.0)), Up * -1.0);
				}
				for (double X = -IX + 0.3; X < IX; X += 0.6)
					B.Beam(V3{ X, -IY - 0.1, Ceil - 0.1 }, V3{ X, IY + 0.1, Ceil - 0.1 }, 0.12, 0.2, Scaled(Rgb(96, 78, 58), StandK(X, 0.0)), Up, 0);
			}
			const Col Oak = Rgb(118, 104, 84);
			Rng R(K0 ^ 0x9);
			const DayLight& L0 = Ls.front();
			if (P.Kind == 1)
			{
				// The barn: the loft's beam along it on two posts, a ladder up to the loft, a cart wheel against its wall.
				const double BY = -0.4 * L0.Side, BZ = Ceil - 0.2 - 0.12;
				B.Beam(V3{ -IX - 0.1, BY, BZ }, V3{ IX + 0.1, BY, BZ }, 0.22, 0.24, Scaled(Oak, StandK(L0.X, BY) * 0.85), Up, 0);
				for (int S = -1; S <= 1; S += 2)
				{
					const double PX = std::clamp(L0.X + S * (L0.HW + 0.9), -IX + 0.4, IX - 0.4);
					B.Box(V3{ PX, BY, 0.5 * (BZ - 0.12) }, V3{ 0.1, 0.0, 0.0 }, V3{ 0.0, 0.1, 0.0 }, V3{ 0.0, 0.0, 0.5 * (BZ - 0.12) }, Scaled(Varied(Oak, 0.95 + 0.1 * S, 0.0), StandK(PX, BY)), 16);
				}
				// (the ladder, leaning on the beam beside a post)
				{
					const double LX = std::clamp(L0.X - (L0.HW + 1.9), -IX + 0.6, IX - 0.6);
					const V3 Foot{ LX, BY + L0.Side * 1.25, 0.02 }, Head{ LX, BY + L0.Side * 0.14, BZ + 0.3 };
					const V3 Side{ 0.22, 0.0, 0.0 };
					const Col LC = Scaled(Rgb(126, 110, 88), StandK(LX, BY + L0.Side * 0.7));
					B.Beam(Foot - Side, Head - Side, 0.05, 0.07, LC, V3{ 0.0, static_cast<double>(L0.Side), 0.0 }, 0);
					B.Beam(Foot + Side, Head + Side, 0.05, 0.07, LC, V3{ 0.0, static_cast<double>(L0.Side), 0.0 }, 0);
					for (double T = 0.1; T < 0.97; T += 0.11)
					{
						const V3 M = Foot + (Head - Foot) * T;
						B.Beam(M - Side, M + Side, 0.035, 0.035, Scaled(LC, 0.95), Up, 3);
					}
				}
				// (a cart wheel against the wall across from the doors)
				{
					const double WX = std::clamp(L0.X + L0.HW + 1.3, -IX + 0.8, IX - 0.8), WY = -L0.Side * (IY - 0.2), Rd = 0.62;
					const V3 C{ WX, WY, Rd + 0.03 };
					const V3 Ax{ 1.0, 0.0, 0.0 }, Az = Norm(V3{ 0.0, L0.Side * 0.17, 1.0 });
					const Col WC = Scaled(Rgb(120, 102, 80), StandK(WX, WY));
					const int NS = 14;
					for (int K = 0; K < NS; ++K)
					{
						const double T0 = 2.0 * Pi * K / NS, T1 = 2.0 * Pi * (K + 1) / NS;
						B.Beam(C + (Ax * std::cos(T0) + Az * std::sin(T0)) * Rd, C + (Ax * std::cos(T1) + Az * std::sin(T1)) * Rd, 0.07, 0.06, K % 2 ? WC : Varied(WC, 0.94, 0.0), Cross(Ax, Az), 0);
					}
					for (int K = 0; K < 7; ++K)
					{
						const double T = 2.0 * Pi * K / 7.0;
						B.Beam(C, C + (Ax * std::cos(T) + Az * std::sin(T)) * (Rd - 0.03), 0.035, 0.035, Scaled(WC, 0.92), Cross(Ax, Az), 1);
					}
					const V3 HubN = Cross(Ax, Az);
					B.Tube(C - HubN * 0.11, C + HubN * 0.11, 0.09, 0.08, 8, Scaled(WC, 0.85), true, true);
				}
			}
			else
			{
				// The byre: the manger along its back wall, the hay rack leaning out over it, the partitions between the
				// cows' standings, each ending in a post.
				const int BS = -L0.Side; // (its back wall, across from its doors)
				const double WY = BS * IY;
				const Col Wood = Rgb(140, 120, 94);
				for (double X = -IX + 0.3; X < IX - 0.3 - 1e-6; X += 1.2)
				{
					const double X1 = std::min(IX - 0.3, X + 1.2), XM = 0.5 * (X + X1);
					const double K = StandK(XM, WY - BS * 0.6);
					// (the manger: its trough's front board, its floor of straw and old hay)
					B.Box(V3{ XM, WY - BS * 0.55, 0.6 }, V3{ 0.5 * (X1 - X), 0.0, 0.0 }, V3{ 0.0, 0.025, 0.0 }, V3{ 0.0, 0.0, 0.25 }, Scaled(Varied(Wood, 0.95, 0.0), K), 0);
					B.Quad1(V3{ X, WY, 0.55 }, V3{ X1, WY, 0.55 }, V3{ X1, WY - BS * 0.53, 0.55 }, V3{ X, WY - BS * 0.53, 0.55 }, Scaled(Rgb(150, 130, 90), K * 0.8), Up);
					// (the rack: its rails, its slats leaning out)
					B.Beam(V3{ X, WY - BS * 0.08, 1.08 }, V3{ X1, WY - BS * 0.08, 1.08 }, 0.05, 0.05, Scaled(Wood, K), Up, 3);
					B.Beam(V3{ X, WY - BS * 0.5, 1.86 }, V3{ X1, WY - BS * 0.5, 1.86 }, 0.05, 0.05, Scaled(Wood, K), Up, 3);
					for (double SX = X + 0.07; SX < X1 - 0.02; SX += 0.15)
						B.Beam(V3{ SX, WY - BS * 0.08, 1.06 }, V3{ SX, WY - BS * 0.5, 1.9 }, 0.03, 0.025, Scaled(Varied(Wood, 0.92 + 0.12 * Unit(Hash(K0, static_cast<uint64_t>(std::llround((SX + 50.0) * 100.0)))), 0.0), K), V3{ 0.0, static_cast<double>(-BS), 0.0 }, 3);
				}
				// (old hay left in the rack)
				for (double X = -IX + 1.0; X < IX - 1.0; X += 2.6)
					HayMound(B, P, V3{ X + R.Range(-0.4, 0.4), WY - BS * 0.32, 1.12 }, 0.5, 0.18, 0.35, 0.0, Hash(K0, 0x4A, static_cast<uint64_t>(std::llround((X + 50.0) * 10.0))), 0.55 * StandK(X, WY));
				for (double X = -IX + 1.9; X < IX - 0.9; X += 1.9)
				{
					const double K = StandK(X, WY - BS * 1.4);
					const Col PC = Scaled(Varied(Wood, 0.9 + 0.15 * Unit(Hash(K0, 0x9A, static_cast<uint64_t>(std::llround((X + 50.0) * 10.0)))), 0.0), K);
					const double YA = WY - BS * 0.6, YB = WY - BS * 2.2;
					B.Box(V3{ X, 0.5 * (YA + YB), 0.72 }, V3{ 0.025, 0.0, 0.0 }, V3{ 0.0, 0.5 * std::fabs(YB - YA), 0.0 }, V3{ 0.0, 0.0, 0.62 }, PC, 0);
					for (double T : { 0.25, 0.5, 0.75 })
					{
						const double Yp = YA + (YB - YA) * T;
						for (int S = -1; S <= 1; S += 2)
							B.Quad1(V3{ X + S * 0.0255, Yp - 0.008, 0.12 }, V3{ X + S * 0.0255, Yp + 0.008, 0.12 }, V3{ X + S * 0.0255, Yp + 0.008, 1.32 }, V3{ X + S * 0.0255, Yp - 0.008, 1.32 }, Scaled(PC, 0.55), V3{ static_cast<double>(S), 0.0, 0.0 });
					}
					B.Box(V3{ X, YB, 0.75 }, V3{ 0.06, 0.0, 0.0 }, V3{ 0.0, 0.06, 0.0 }, V3{ 0.0, 0.0, 0.75 }, Scaled(PC, 0.95), 16);
				}
			}
			// Old straw strewn over the floor where the day falls, and a heap of it to one side of a door.
			for (const DayLight& L : Ls)
			{
				if (L.K < 0.5) continue;
				const int N = static_cast<int>(70 * L.K);
				for (int K = 0; K < N; ++K)
				{
					const double D = 0.12 + 4.2 * std::pow(R.Next(), 1.5);
					const double X = std::clamp(L.X + R.Range(-L.HW - 0.6 - 0.3 * D, L.HW + 0.6 + 0.3 * D), -IX + 0.1, IX - 0.1), Y = L.Side * (IY - D);
					const double Ang = R.Range(0.0, Pi), Ln = R.Range(0.12, 0.38), Wd = R.Range(0.012, 0.022), Z = 0.026 + 0.012 * R.Next();
					const V3 A{ std::cos(Ang) * 0.5 * Ln, std::sin(Ang) * 0.5 * Ln, 0.0 }, Wv{ -std::sin(Ang) * 0.5 * Wd, std::cos(Ang) * 0.5 * Wd, 0.0 };
					const V3 C{ X, Y, Z };
					const Col SC = Scaled(Varied(Rgb(212, 186, 128), 0.85 + 0.25 * R.Next(), 0.0), FloorK(X, Y));
					B.Quad1(C - A - Wv, C + A - Wv, C + A + Wv + Up * 0.006, C - A + Wv + Up * 0.006, SC, Up);
				}
				const double HX = std::clamp(L.X + (R.Next() < 0.5 ? -1.0 : 1.0) * (L.HW + 1.4), -IX + 1.0, IX - 1.0), HY = L.Side * (IY - R.Range(1.3, 2.2));
				HayMound(B, P, V3{ HX, HY, 0.02 }, R.Range(0.9, 1.3), R.Range(0.7, 1.0), R.Range(0.5, 0.8), R.Range(0.0, Pi), Hash(K0, 0x4B, static_cast<uint64_t>(std::llround((L.X + 50.0) * 10.0))), 0.72 * FloorK(HX, HY));
			}
		}

		/** The barn's and the byre's openings: the cart doors, a slit, the pitching door, a stable door, a barred window. */
		void BarnOpeningNear(Pen& B, const Plan& P, const Opening& O, const Face& Fc, double Z1, double Dp, uint64_t H)
		{
			const bool bDark = P.Hurt < 2; // (its inside not drawn: the dark behind what stands open)
			const bool bRoom = bDark && !DayLights(P).empty(); // (or its ground floor drawn, BarnRoom, lit by the day coming in)
			const double W = O.A1 - O.A0, AM = 0.5 * (O.A0 + O.A1);
			// A leaf hinged at A0 (Side -1) or A1 (+1), swung out Sw: its planks facing out when shut, turned with it.
			auto Swung = [&](int Side, double Sw, double Z0, double LW, double LH, int Planks, int Ledges, uint64_t LH8, const Col* Own = nullptr) {
				const V3 Along = Fc.T * (Side < 0 ? 1.0 : -1.0);
				const V3 Across = Norm(Along * std::cos(Sw) + Fc.N * std::sin(Sw));
				const V3 Out = Norm(Fc.N * std::cos(Sw) - Along * std::sin(Sw));
				const V3 Hinge = At(Fc, (Side < 0 ? O.A0 + 0.012 : O.A1 - 0.012), Z0, Sw > 0.01 ? 0.0 : -Dp + 0.025);
				PlankLeaf(B, P, Hinge, Across, Out, LW, LH, Planks, Ledges, LH8, O.bCharred, Own);
			};
			if (O.What == OpSlit)
			{
				if (bDark) B.Quad1(At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Rgb(14, 13, 12), Fc.N);
				return;
			}
			if (O.What == OpCart)
			{
				// Two leaves of eight planks, three ledges and their braces, on strap hinges; one now and then standing
				// open on the barn's dark and its straw.
				if (bDark && (O.Swing > 0.01 || O.Swing2 > 0.01 || !O.bLeaf)) Cave(B, P, Fc, O.A0, O.A1, O.Z0, Z1, Dp, 0.02, bRoom);
				if (!O.bLeaf) return;
				const double LH = Z1 - O.Z0 - 0.08, LW = 0.5 * W - 0.014;
				Swung(-1, O.Swing, O.Z0 + 0.04, LW, LH, 8, 3, Mix(H + 1), &P.K.Oak);
				Swung(1, O.Swing2, O.Z0 + 0.04, LW, LH, 8, 3, Mix(H + 2), &P.K.Oak);
				return;
			}
			if (O.What == OpPitch)
			{
				// One leaf to the loft; open, the hay heaped inside it.
				if (bDark && (O.Swing > 0.01 || !O.bLeaf))
				{
					Cave(B, P, Fc, O.A0, O.A1, O.Z0, Z1, Dp, P.Loft);
					HayMound(B, P, At(Fc, AM, P.Loft, -P.Thick - 0.75), 1.0, 0.7, 0.85, std::atan2(Fc.T.Y, Fc.T.X), H ^ 0x4A7, 0.85);
				}
				if (O.bLeaf) Swung(-1, O.Swing, O.Z0 + 0.03, W - 0.024, Z1 - O.Z0 - 0.06, 5, 2, Mix(H + 3));
				return;
			}
			if (O.What == OpStable)
			{
				// Two halves on the same hinges' side: the lower shut, the upper shut or folded back open on the dark.
				const double ZM = O.Z0 + 0.56 * (Z1 - O.Z0);
				if (bDark && (O.Swing > 0.01 || !O.bLeaf)) Cave(B, P, Fc, O.A0, O.A1, O.Z0, Z1, Dp, 0.02, bRoom);
				if (!O.bLeaf) return;
				Swung(-1, 0.0, O.Z0 + 0.03, W - 0.024, ZM - O.Z0 - 0.04, 5, 2, Mix(H + 4));
				Swung(-1, O.Swing, ZM + 0.01, W - 0.024, Z1 - ZM - 0.05, 5, 2, Mix(H + 5));
				return;
			}
			// A byre's window: unglazed, the dark behind three iron bars; its one plank shutter hinged at A1, folded back
			// against the wall, shut in its rebate, or hanging from its lower hinge.
			if (bRoom && O.Shutters != 1) Cave(B, P, Fc, O.A0, O.A1, O.Z0, Z1, Dp, 0.02, true);
			else if (bDark) B.Quad1(At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Rgb(16, 15, 14), Fc.N);
			for (int K = 1; K <= 3; ++K)
			{
				const double A = O.A0 + W * K / 4.0;
				B.Tube(At(Fc, A, O.Z0 - 0.02, -0.5 * Dp), At(Fc, A, Z1 + 0.02, -0.5 * Dp), 0.011, 0.011, 5, O.bCharred ? P.K.Char : P.K.Iron, false);
			}
			const double High = Z1 - O.Z0 - 0.02;
			if (High < 0.3) return;
			const uint64_t LH = Mix(H + 0x5E);
			if (O.Shutters == 0) Leaf(B, P, At(Fc, O.A1 - 0.012, O.Z0 + 0.01, 0.034), Fc.T, Fc.N, W - 0.008, High, false, O.bCharred, LH);
			else if (O.Shutters == 1) Leaf(B, P, At(Fc, O.A1, O.Z0 + 0.01, -0.075), Fc.T * -1.0, Fc.N, W - 0.008, High, true, O.bCharred, LH);
			else if (O.Shutters == 3)
			{
				// (torn from its top hinge: it hangs tilted from the bottom one, swung out from the wall)
				const V3 Across = Norm(Fc.T * -std::cos(2.2) + Fc.N * std::sin(2.2));
				V3 Out = Norm(Cross(Up, Across));
				if (Dot(Out, Fc.N) < 0.0) Out = Out * -1.0;
				const double Tilt = 0.32;
				const V3 UpT = Norm(Up * std::cos(Tilt) + Across * std::sin(Tilt)), AcT = Norm(Across * std::cos(Tilt) - Up * std::sin(Tilt));
				const V3 Hg = At(Fc, O.A1, O.Z0 + 0.05, 0.01);
				B.Box(Hg + AcT * (0.5 * W) + UpT * (0.5 * High), AcT * (0.5 * W), UpT * (0.5 * High), Out * 0.016, Scaled(P.K.Shutter, 0.85), 0);
			}
		}

		// ------------------------------------------------------------------ a church's round-headed openings

		/** A round head's point: R out from its centre (AM along the face, SP up it, its springing) at angle T (0 toward A1,
		 * Pi toward A0), D out from the face. */
		V3 ArcAt(const Face& Fc, double AM, double SP, double R, double T, double D) { return At(Fc, AM + R * std::cos(T), SP + R * std::sin(T), D); }

		/**
		 * A round head's ring of dressed stone: wedges from R0 to R1 about its centre, an odd number of them so one is its
		 * key, each its own tone and weathered as the wall, their joints radial and dark, Proud out from the wall, their
		 * arrises chamfered; the mortar behind them where the opening's square runs up behind the ring (bBacking).
		 */
		void ArchRing(Pen& B, const Plan& P, const Face& Fc, int F, double AM, double SP, double R0, double R1, double Proud, uint64_t H, double Step, bool bBacking)
		{
			const double RM = 0.5 * (R0 + R1);
			int N = std::max(5, static_cast<int>(std::lround(Pi * RM / Step)));
			if (N % 2 == 0) ++N;
			const double G = 0.006 / RM, Ch = std::min(0.012, 0.25 * Proud + 0.004);
			if (bBacking)
				for (int K = 0; K < N; ++K)
				{
					const double T0 = Pi * K / N, T1 = Pi * (K + 1) / N;
					B.Quad1(ArcAt(Fc, AM, SP, R0, T0, -0.003), ArcAt(Fc, AM, SP, R0, T1, -0.003), ArcAt(Fc, AM, SP, R1, T1, -0.003), ArcAt(Fc, AM, SP, R1, T0, -0.003), Scaled(P.K.Mortar, 0.72), Fc.N);
				}
			for (int K = 0; K < N; ++K)
			{
				const double T0 = Pi * K / N + G, T1 = Pi * (K + 1) / N - G, TM = 0.5 * (T0 + T1);
				const uint64_t HK = Hash(H, 0xA2C, static_cast<uint64_t>(K));
				const Col C = Weathered(P, Fc, F, AM + RM * std::cos(TM), SP + RM * std::sin(TM), Varied(P.K.Dressed, 0.93 + 0.11 * Unit(HK), 0.0));
				const double I0 = R0 + 0.004, O0 = R1 - 0.004;
				const V3 B0 = ArcAt(Fc, AM, SP, I0, T0, 0.004), B1 = ArcAt(Fc, AM, SP, I0, T1, 0.004), B2 = ArcAt(Fc, AM, SP, O0, T1, 0.004), B3 = ArcAt(Fc, AM, SP, O0, T0, 0.004);
				const V3 F0 = ArcAt(Fc, AM, SP, I0 + Ch, T0 + Ch / I0, Proud), F1 = ArcAt(Fc, AM, SP, I0 + Ch, T1 - Ch / I0, Proud);
				const V3 F2 = ArcAt(Fc, AM, SP, O0 - Ch, T1 - Ch / O0, Proud), F3 = ArcAt(Fc, AM, SP, O0 - Ch, T0 + Ch / O0, Proud);
				const V3 Rad = Fc.T * std::cos(TM) + Up * std::sin(TM);
				const V3 Tan0 = Fc.T * -std::sin(T0) + Up * std::cos(T0), Tan1 = Fc.T * -std::sin(T1) + Up * std::cos(T1);
				B.Quad1(F0, F1, F2, F3, C, Fc.N);
				B.Quad1(B0, B1, F1, F0, Scaled(C, 0.8), Fc.N * 0.5 - Rad);
				B.Quad1(B3, B2, F2, F3, Scaled(C, 1.02), Fc.N * 0.5 + Rad);
				B.Quad1(B0, B3, F3, F0, Scaled(C, 0.9), Fc.N * 0.5 - Tan0);
				B.Quad1(B1, B2, F2, F1, Scaled(C, 0.9), Fc.N * 0.5 + Tan1);
			}
		}

		/** A round-headed opening's reveal: its sides up to the springing, its soffit round under the head, Dp deep; Segs
		 * pieces round. */
		void ArchReveal(Pen& B, const Plan& P, const Face& Fc, const Opening& O, double Dp, int Segs)
		{
			const double AM = 0.5 * (O.A0 + O.A1), SP = SpringOf(O), R = 0.5 * (O.A1 - O.A0);
			const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
			B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, SP, -Dp), At(Fc, O.A0, SP, 0.0), Scaled(Rev, 0.95), Fc.T);
			B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, SP, -Dp), At(Fc, O.A1, SP, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
			for (int I = 0; I < Segs; ++I)
			{
				const double T0 = Pi * I / Segs, T1 = Pi * (I + 1) / Segs, TM = 0.5 * (T0 + T1);
				const V3 In = (Fc.T * std::cos(TM) + Up * std::sin(TM)) * -1.0;
				// (darker under its crown, where the sky does not reach)
				const Col C = Scaled(Rev, 0.7 + 0.22 * Sq(std::cos(TM)));
				B.Quad1(ArcAt(Fc, AM, SP, R, T0, 0.0), ArcAt(Fc, AM, SP, R, T1, 0.0), ArcAt(Fc, AM, SP, R, T1, -Dp), ArcAt(Fc, AM, SP, R, T0, -Dp), C, In);
			}
		}

		/** A round-headed opening's shape filled flat at D (out from the face): its square to the springing, coloured C at
		 * its foot and CT at the springing, and its half-disc over that in CT. */
		void ArchFill(Pen& B, const Face& Fc, const Opening& O, double D, const Col& C, const Col& CT, int Fan = 8)
		{
			const double AM = 0.5 * (O.A0 + O.A1), SP = SpringOf(O), R = 0.5 * (O.A1 - O.A0);
			B.Quad(At(Fc, O.A0, O.Z0, D), At(Fc, O.A1, O.Z0, D), At(Fc, O.A1, SP, D), At(Fc, O.A0, SP, D), C, C, CT, CT, Fc.N);
			const V3 M = At(Fc, AM, SP, D);
			for (int I = 0; I < Fan; ++I) B.Tri1(M, ArcAt(Fc, AM, SP, R, Pi * I / Fan, D), ArcAt(Fc, AM, SP, R, Pi * (I + 1) / Fan, D), CT, Fc.N);
		}

		/**
		 * A church's opening, near (OpChWin, OpChDoor, OpBelfry): its reveal and soffit, its ring of voussoirs; a window's
		 * leaded glass, quarry by quarry, each pane's old glass its own tint with the sky in it, its iron saddle bars; the
		 * door's two oak leaves of upright planks cut to the arch, their long iron straps, its hood mould and label stops,
		 * its threshold and step; the belfry's louvres sloping out over the dark.
		 */
		void ChurchOpeningNear(Pen& B, const Plan& P, const Opening& O, const Face& Fc, double Dp, uint64_t H)
		{
			const int F = O.Face;
			const double AM = 0.5 * (O.A0 + O.A1), SP = SpringOf(O), R = 0.5 * (O.A1 - O.A0), W = O.A1 - O.A0;
			const double Ring = RingOf(O);
			ArchReveal(B, P, Fc, O, Dp, 12);
			ArchRing(B, P, Fc, F, AM, SP, R, R + Ring, 0.016, H, 0.24, true);
			if (O.What == OpChDoor)
			{
				// The hood mould over its ring, a narrow roll of stone, ending in a block either side at the springing.
				ArchRing(B, P, Fc, F, AM, SP, R + Ring + 0.015, R + Ring + 0.135, 0.05, H ^ 0x400D, 0.34, false);
				for (int S = -1; S <= 1; S += 2)
				{
					const double A = AM + S * (R + Ring + 0.075);
					Block(B, Fc, A - 0.09, A + 0.09, SP - 0.2, SP + 0.01, 0.055, Weathered(P, Fc, F, A, SP, Varied(P.K.Dressed, 0.96, 0.0)));
				}
				// The threshold, worn, and the step down to the churchyard's path.
				B.Quad1(At(Fc, O.A0, 0.02, 0.05), At(Fc, O.A1, 0.02, 0.05), At(Fc, O.A1, 0.02, -Dp), At(Fc, O.A0, 0.02, -Dp), Varied(P.K.Dressed, 0.8, 0.0), Up);
				B.Box(At(Fc, AM, -0.6, 0.34), Fc.T * (0.5 * W + 0.32), Fc.N * 0.29, Up * 0.44, Weathered(P, Fc, F, AM, 0.0, Varied(P.K.Dressed, 0.86, 0.0)), 0);
				// The leaves: dark behind; upright oak planks, each cut to the arch over it, the joints and the meeting
				// stiles dark between them.
				const Col Dark = Scaled(P.K.Door, 0.3);
				ArchFill(B, Fc, O, -Dp - 0.012, Dark, Dark);
				const int NP = std::max(6, 2 * static_cast<int>(std::lround(0.5 * W / 0.16)));
				auto TopAt3 = [&](double A) { return SP + std::sqrt(std::max(0.0, R * R - Sq(A - AM))) - 0.015; };
				for (int K = 0; K < NP; ++K)
				{
					const double Gap0 = K == NP / 2 ? 0.009 : 0.004, Gap1 = K + 1 == NP / 2 ? 0.009 : 0.004;
					const double A0 = O.A0 + W * K / NP + Gap0, A1 = O.A0 + W * (K + 1) / NP - Gap1;
					const uint64_t PH = Mix(H + 0x9A0 + static_cast<uint64_t>(K));
					const Col C = Varied(P.K.Door, 0.86 + 0.24 * Unit(PH), 0.0);
					B.Quad(At(Fc, A0, 0.03, -Dp), At(Fc, A1, 0.03, -Dp), At(Fc, A1, TopAt3(A1), -Dp), At(Fc, A0, TopAt3(A0), -Dp), Scaled(C, 0.8), Scaled(C, 0.8), C, C, Fc.N);
				}
				// (each leaf's long iron straps from its hinges, ending in a split curl; a ring at the meeting stiles)
				for (int S = -1; S <= 1; S += 2)
				{
					const double HA = S < 0 ? O.A0 + 0.03 : O.A1 - 0.03, LW = 0.5 * W - 0.05;
					for (double Z : { 0.42, 0.5 * (0.42 + SP - 0.2), SP - 0.2 })
					{
						const double EA = HA - S * 0.78 * LW;
						B.Box(At(Fc, 0.5 * (HA + EA), Z, -Dp + 0.007), Fc.T * (0.39 * LW), Up * 0.024, Fc.N * 0.005, P.K.Iron, 16);
						const V3 C = At(Fc, EA, Z, -Dp + 0.007);
						const V3 DA = Norm(Fc.T + Up) * 0.05, DB = Norm(Fc.T - Up) * 0.05;
						B.Box(C, DA, DB, Fc.N * 0.005, P.K.Iron, 16);
						// (a nail head at the hinge's end)
						B.Box(At(Fc, HA - S * 0.04, Z, -Dp + 0.012), Fc.T * 0.018, Up * 0.018, Fc.N * 0.006, Scaled(P.K.Iron, 0.8), 16);
					}
				}
				B.Tube(At(Fc, AM + 0.09, 1.05, -Dp + 0.02), At(Fc, AM + 0.09, 1.05, -Dp + 0.03), 0.07, 0.07, 8, P.K.Iron, true, false);
				return;
			}
			if (O.What == OpBelfry)
			{
				// The dark of the bell chamber, and its louvres: boards sloping down and out, round to the arch.
				ArchFill(B, Fc, O, -Dp, Rgb(14, 13, 12), Rgb(12, 11, 11));
				const double Ang = 0.62;
				const V3 Dir = Norm(Fc.N * std::cos(Ang) - Up * std::sin(Ang)), Th = Norm(Cross(Fc.T, Dir));
				int K = 0;
				for (double Z = O.Z0 + 0.16; Z < O.Z1 - 0.08; Z += 0.21, ++K)
				{
					const double HW = Z <= SP ? 0.5 * W - 0.01 : std::sqrt(std::max(0.0, R * R - Sq(Z - SP))) - 0.02;
					if (HW < 0.08) break;
					B.Box(At(Fc, AM, Z, -0.5 * Dp), Fc.T * HW, Dir * 0.105, Th * 0.012, Varied(Rgb(96, 90, 82), 0.88 + 0.2 * Unit(Mix(H + static_cast<uint64_t>(K))), 0.0), 0);
				}
				return;
			}
			// A window: the lead behind, its quarries, each pane's glass its own; its saddle bars. (A slit: open, dark.)
			ArchFill(B, Fc, O, -Dp - 0.004, O.bGlass ? Rgb(34, 33, 34) : Rgb(14, 13, 12), O.bGlass ? Rgb(34, 33, 34) : Rgb(12, 11, 11), 8);
			if (!O.bGlass) return;
			const int NC = std::max(2, static_cast<int>(std::lround(W / 0.17)));
			const int NR = std::max(2, static_cast<int>(std::lround((SP - O.Z0) / 0.21)));
			const int NF = std::max(1, static_cast<int>(std::lround(R / 0.19)));
			const double E = 0.007, Hi = O.Z1 - O.Z0;
			auto Glass = [&](double Z, uint64_t PH) {
				const double M = 0.12 + 0.22 * Unit(PH) + 0.12 * (Z - O.Z0) / Hi;
				Col G = Varied(Mixed(P.K.Glass, P.K.Sky, M), 1.0, (Unit(Mix(PH)) - 0.5) * 0.14);
				if (static_cast<int>(Unit(Mix(PH ^ 0xC4)) * 8.0) < O.Cracked) G = Rgb(16, 15, 14);
				return G;
			};
			for (int J = 0; J < NR + NF; ++J)
			{
				const bool bFan = J >= NR;
				const double Z0 = bFan ? SP + R * (J - NR) / NF : O.Z0 + (SP - O.Z0) * J / NR;
				const double Z1 = bFan ? SP + R * (J - NR + 1) / NF : O.Z0 + (SP - O.Z0) * (J + 1) / NR;
				const double HW0 = bFan ? std::sqrt(std::max(0.0, R * R - Sq(Z0 + E - SP))) - E : 1e9;
				const double HW1 = bFan ? std::sqrt(std::max(0.0, R * R - Sq(Z1 - E - SP))) - E : 1e9;
				for (int I = 0; I < NC; ++I)
				{
					const double A0 = O.A0 + W * I / NC + E, A1 = O.A0 + W * (I + 1) / NC - E;
					const double BA0 = std::max(A0, AM - HW0), BA1 = std::min(A1, AM + HW0);
					double TA0 = std::max(A0, AM - HW1), TA1 = std::min(A1, AM + HW1);
					if (BA1 - BA0 < 0.015) continue;
					const uint64_t PH = Hash(H ^ 0x61A55, static_cast<uint64_t>(J), static_cast<uint64_t>(I));
					const Col G0 = Glass(Z0, PH), G1 = Glass(Z1, PH ^ 1);
					const V3 Q0 = At(Fc, BA0, Z0 + E, -Dp), Q1 = At(Fc, BA1, Z0 + E, -Dp);
					if (TA1 - TA0 < 0.01)
					{
						B.Tri1(Q0, Q1, At(Fc, std::clamp(0.5 * (TA0 + TA1), BA0, BA1), Z1 - E, -Dp), G0, Fc.N);
						continue;
					}
					B.Quad(Q0, Q1, At(Fc, TA1, Z1 - E, -Dp), At(Fc, TA0, Z1 - E, -Dp), Scaled(G0, 0.94), Scaled(G0, 0.94), G1, G1, Fc.N);
				}
			}
			for (double T : { 1.0 / 3.0, 2.0 / 3.0 })
				B.Box(At(Fc, AM, O.Z0 + (SP - O.Z0) * T, -Dp + 0.011), Fc.T * (0.5 * W + 0.012), Up * 0.009, Fc.N * 0.009, P.K.Iron, 0);
		}

		/** A window's frame and two casements with their glazing bars and panes, its shutters; or the door, its fanlight
		 * and its step. Then the reveal round it. */
		void OpeningNear(Pen& B, const Plan& P, const Opening& O, bool bInside)
		{
			const int F = O.Face;
			const Face Fc = FaceOf(P, F);
			const double TopHere = std::min(TopOf(P, Fc, F, O.A0 - 0.1), TopOf(P, Fc, F, O.A1 + 0.1));
			const double Z1 = std::min(O.Z1, TopHere);
			const double Dp = RevealOf(P, O, bInside);
			if (O.What >= OpHidden)
			{
				if (O.What != OpHidden) ChurchOpeningNear(B, P, O, Fc, Dp, Hash(P.Key, 0x0BE, static_cast<uint64_t>(F * 1000) + static_cast<uint64_t>(std::llround(O.A0 * 20.0))));
				return;
			}
			const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
			// The reveal: its sides, its head (a door's floor is its threshold).
			B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, Z1, -Dp), At(Fc, O.A0, Z1, 0.0), Scaled(Rev, 0.95), Fc.T);
			B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A1, Z1, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
			if (Z1 >= O.Z1 - 1e-6) B.Quad1(At(Fc, O.A0, Z1, 0.0), At(Fc, O.A1, Z1, 0.0), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Scaled(Rev, 0.7), Up * -1.0);
			if (O.What == 1)
				B.Quad1(At(Fc, O.A0, 0.02, 0.05), At(Fc, O.A1, 0.02, 0.05), At(Fc, O.A1, 0.02, -Dp), At(Fc, O.A0, 0.02, -Dp), Varied(P.K.Dressed, 0.82, 0.0), Up);
			// (a cart door's threshold worn to the earth and the cobbles, a stable door's a stone)
			else if (Grounded(O.What))
				B.Quad1(At(Fc, O.A0, 0.02, 0.05), At(Fc, O.A1, 0.02, 0.05), At(Fc, O.A1, 0.02, -Dp), At(Fc, O.A0, 0.02, -Dp), O.What == OpCart ? Rgb(110, 98, 80) : Varied(P.K.Dressed, 0.8, 0.0), Up);
			const uint64_t H = Hash(P.Key, 0x0BE, static_cast<uint64_t>(F * 1000) + static_cast<uint64_t>(std::llround(O.A0 * 20.0)));
			if (O.What >= OpCart)
			{
				BarnOpeningNear(B, P, O, Fc, Z1, Dp, H);
				return;
			}
			const double W = O.A1 - O.A0;
			const Col Fr = O.bCharred ? P.K.Char : P.K.Frame;
			if (O.What == 1)
			{
				// The step: one stone, its top a step below the floor, running down into the ground.
				B.Box(At(Fc, 0.5 * (O.A0 + O.A1), -0.6, 0.27), Fc.T * (0.5 * W + 0.16), Fc.N * 0.22, Up * 0.44, Weathered(P, Fc, F, 0.5 * (O.A0 + O.A1), 0.0, Varied(P.K.Dressed, 0.86, 0.0)), 0);
				// The fanlight over the door, its transom and bars.
				const double ZD = O.Z1 - 0.42;
				if (Z1 > ZD + 0.1 && !O.bCharred)
				{
					B.Box(At(Fc, 0.5 * (O.A0 + O.A1), ZD + 0.03, -Dp + 0.02), Fc.T * (0.5 * W), Up * 0.035, Fc.N * 0.03, Fr, 0);
					for (int K = 0; K < 3; ++K)
					{
						const double PA0 = O.A0 + W * K / 3.0 + 0.02, PA1 = O.A0 + W * (K + 1) / 3.0 - 0.02;
						B.Quad1(At(Fc, PA0, ZD + 0.065, -Dp), At(Fc, PA1, ZD + 0.065, -Dp), At(Fc, PA1, Z1 - 0.03, -Dp), At(Fc, PA0, Z1 - 0.03, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.3 + 0.25 * Unit(Mix(H + K))), Fc.N);
						if (K > 0) B.Box(At(Fc, O.A0 + W * K / 3.0, 0.5 * (ZD + Z1), -Dp + 0.012), Fc.T * 0.02, Up * (0.5 * (Z1 - ZD)), Fc.N * 0.012, Fr, 0);
					}
				}
				// The leaf: planks with dark joints, two strap hinges, the latch; swung in about its hinge where hit.
				if (O.bLeaf)
				{
					const double LW = W - 0.02, LH = ZD;
					const double Sw = O.Swing;
					const V3 Hinge = At(Fc, O.A0 + 0.01, 0.02, -Dp + 0.03);
					const V3 Across = Norm(Fc.T * std::cos(Sw) - Fc.N * std::sin(Sw));
					const V3 Out = Norm(Cross(Across, Up)) * (Dot(Cross(Across, Up), Fc.N) >= 0.0 ? 1.0 : -1.0);
					const V3 Mid = Hinge + Across * (0.5 * LW) + Up * (0.5 * LH) - Out * 0.03;
					// (its body behind the planks, dark in their joints)
					B.Box(Mid, Across * (0.5 * LW), Up * (0.5 * LH), Out * 0.03, Scaled(P.K.Door, 0.45), 32);
					const V3 Front = Mid + Out * 0.0315;
					const int N = 6;
					for (int K = 0; K < N; ++K)
					{
						const double U0 = -0.5 * LW + LW * K / N + 0.005, U1 = -0.5 * LW + LW * (K + 1) / N - 0.005;
						const Col C = Varied(P.K.Door, 0.88 + 0.24 * Unit(Mix(H + 31 * K)), 0.0);
						B.Quad(Front + Across * U0 - Up * (0.5 * LH), Front + Across * U1 - Up * (0.5 * LH), Front + Across * U1 + Up * (0.5 * LH), Front + Across * U0 + Up * (0.5 * LH), Scaled(C, 0.85), Scaled(C, 0.85), C, C, Out);
					}
					// (three ledges across it, the strap hinges along the top and bottom ones)
					for (double Z : { 0.32, 0.5 * LH, LH - 0.32 })
					{
						const V3 LM = Front + Up * (Z - 0.5 * LH) + Out * 0.011;
						B.Box(LM, Across * (0.5 * LW - 0.03), Up * 0.055, Out * 0.011, Varied(P.K.Door, 1.05, 0.0), 0);
						if (Z != 0.5 * LH) B.Box(LM + Across * (-0.5 * LW + 0.36) + Out * 0.014, Across * 0.34, Up * 0.024, Out * 0.003, P.K.Iron, 0);
					}
					B.Box(Front + Across * (0.5 * LW - 0.1) + Up * (1.05 - 0.5 * LH) + Out * 0.024, Across * 0.02, Up * 0.06, Out * 0.012, P.K.Iron, 0);
				}
				return;
			}
			// A window: the frame round it at the reveal's back, two casements, three panes each, their bars.
			const double ZG0 = O.Z0, ZG1 = Z1;
			const double D0 = -Dp + 0.012;
			const bool bFrame = Z1 >= O.Z1 - 1e-6;
			if (O.bGlass || O.bCharred)
			{
				const double FW = 0.05;
				const Col FC = Varied(Fr, 0.98, 0.0);
				if (!O.bCharred || Unit(Mix(H ^ 0x61)) < 0.7)
				{
					B.Box(At(Fc, O.A0 + 0.5 * FW, 0.5 * (ZG0 + ZG1), D0), Fc.T * (0.5 * FW), Up * (0.5 * (ZG1 - ZG0)), Fc.N * 0.02, FC, 0);
					B.Box(At(Fc, O.A1 - 0.5 * FW, 0.5 * (ZG0 + ZG1), D0), Fc.T * (0.5 * FW), Up * (0.5 * (ZG1 - ZG0)), Fc.N * 0.02, FC, 0);
					B.Box(At(Fc, 0.5 * (O.A0 + O.A1), ZG0 + 0.5 * FW, D0), Fc.T * (0.5 * W), Up * (0.5 * FW), Fc.N * 0.02, FC, 0);
					if (bFrame) B.Box(At(Fc, 0.5 * (O.A0 + O.A1), ZG1 - 0.5 * FW, D0), Fc.T * (0.5 * W), Up * (0.5 * FW), Fc.N * 0.02, FC, 0);
				}
				if (!O.bCharred)
				{
					const double In0 = O.A0 + FW, In1 = O.A1 - FW, IZ0 = ZG0 + FW, IZ1 = ZG1 - FW;
					const double AM = 0.5 * (In0 + In1);
					// (the meeting stiles, the casements' own stiles and rails, the glazing bars)
					B.Box(At(Fc, AM, 0.5 * (IZ0 + IZ1), D0 + 0.004), Fc.T * 0.032, Up * (0.5 * (IZ1 - IZ0)), Fc.N * 0.016, FC, 0);
					for (double A : { In0 + 0.022, In1 - 0.022 }) B.Box(At(Fc, A, 0.5 * (IZ0 + IZ1), D0 + 0.004), Fc.T * 0.022, Up * (0.5 * (IZ1 - IZ0)), Fc.N * 0.014, FC, 0);
					B.Box(At(Fc, 0.5 * (In0 + In1), IZ0 + 0.035, D0 + 0.004), Fc.T * (0.5 * (In1 - In0)), Up * 0.035, Fc.N * 0.014, FC, 0);
					const double GA[2][2] = { { In0 + 0.044, AM - 0.032 }, { AM + 0.032, In1 - 0.044 } };
					const double GZ0 = IZ0 + 0.07, GZ1 = IZ1 - 0.02;
					const int Rows = O.What == 2 ? 2 : 3;
					for (int R = 1; R < Rows; ++R)
					{
						const double Z = GZ0 + (GZ1 - GZ0) * R / Rows;
						B.Box(At(Fc, 0.5 * (In0 + In1), Z, D0 + 0.002), Fc.T * (0.5 * (In1 - In0)), Up * 0.011, Fc.N * 0.01, FC, 0);
					}
					for (int S = 0; S < 2; ++S)
						for (int R = 0; R < Rows; ++R)
						{
							const double PZ0 = GZ0 + (GZ1 - GZ0) * R / Rows, PZ1 = GZ0 + (GZ1 - GZ0) * (R + 1) / Rows;
							const uint64_t PH = Mix(H + static_cast<uint64_t>(S * 8 + R));
							// (the sky in each pane, lighter toward its top, each pane's old glass its own: the art director's round 2)
							const double M = 0.26 + 0.26 * Unit(PH) + 0.06 * R / std::max(1, Rows - 1);
							Col G = Mixed(P.K.Glass, P.K.Sky, M), GT = Mixed(P.K.Glass, P.K.Sky, std::min(0.9, M + 0.22));
							const bool bBroke = static_cast<int>(Unit(Mix(PH)) * 6.0) < O.Cracked;
							if (!O.bGlass || bBroke) G = GT = Rgb(16, 15, 14);
							B.Quad(At(Fc, GA[S][0], PZ0, -Dp), At(Fc, GA[S][1], PZ0, -Dp), At(Fc, GA[S][1], PZ1, -Dp), At(Fc, GA[S][0], PZ1, -Dp), G, G, GT, GT, Fc.N);
							if (bBroke && O.bGlass)
								B.Tri1(At(Fc, GA[S][0], PZ1, -Dp + 0.003), At(Fc, GA[S][0] + 0.6 * (GA[S][1] - GA[S][0]), PZ1, -Dp + 0.003), At(Fc, GA[S][0], PZ0 + 0.3 * (PZ1 - PZ0), -Dp + 0.003), Mixed(P.K.Glass, P.K.Sky, 0.4), Fc.N);
						}
				}
			}
			else if (!bInside)
				B.Quad1(At(Fc, O.A0, ZG0, -Dp), At(Fc, O.A1, ZG0, -Dp), At(Fc, O.A1, ZG1, -Dp), At(Fc, O.A0, ZG1, -Dp), Rgb(16, 15, 14), Fc.N);
			// Its shutters.
			const double High = Z1 - O.Z0 - 0.02, Half = 0.5 * W - 0.004;
			if (High < 0.4 || O.Shutters == 4) return;
			for (int S = 0; S < 2; ++S)
			{
				const int Side = S == 0 ? -1 : 1; // the leaf hinged at A0 (-1) or A1 (+1)
				const double HA = Side < 0 ? O.A0 : O.A1;
				const uint64_t LH = Mix(H + 0x5E + static_cast<uint64_t>(S));
				int State = O.Shutters;
				if (State == 2) State = S == 0 ? 0 : 2;
				if (State == 3) State = S == 0 ? 3 : 4;
				if (State == 4) continue;
				const V3 Inward = Fc.T * (Side < 0 ? 1.0 : -1.0); // across the opening from the hinge
				if (State == 1)
				{
					// shut, in the reveal's rebate
					Leaf(B, P, At(Fc, HA, O.Z0 + 0.01, -0.075), Inward, Fc.N, Half, High, true, O.bCharred, LH);
				}
				else if (State == 0)
				{
					// folded back flat against the wall either side of the window
					Leaf(B, P, At(Fc, HA - Side * 0.012, O.Z0 + 0.01, 0.034), Inward * -1.0, Fc.N, Half, High, false, O.bCharred, LH);
				}
				else
				{
					// ajar, or hanging by its lower hinge, swung out from the wall
					const double Ang = State == 2 ? 1.75 : 2.2;
					const V3 Across = Norm(Inward * std::cos(Ang) + Fc.N * std::sin(Ang));
					V3 Out = Norm(Cross(Up, Across));
					if (Dot(Out, Fc.N) < 0.0) Out = Out * -1.0;
					if (State == 3)
					{
						// (torn from its top hinge: it hangs tilted from the bottom one)
						const double Tilt = 0.32;
						const V3 UpT = Norm(Up * std::cos(Tilt) + Across * std::sin(Tilt));
						const V3 AcT = Norm(Across * std::cos(Tilt) - Up * std::sin(Tilt));
						const V3 Hg = At(Fc, HA, O.Z0 + 0.05, 0.01);
						const V3 Mid = Hg + AcT * (0.5 * Half) + UpT * (0.5 * High);
						B.Box(Mid, AcT * (0.5 * Half), UpT * (0.5 * High), Out * 0.016, O.bCharred ? P.K.Char : Scaled(P.K.Shutter, 0.85), 0);
					}
					else Leaf(B, P, At(Fc, HA, O.Z0 + 0.01, 0.0), Across, Out, Half, High, Dot(Out, Fc.N) < 0.3, O.bCharred, LH);
				}
			}
		}

		// ------------------------------------------------------------------ the roof, near

		/** One slate (or tile) of a course: its face from its butt up to where the next course covers it, the butt's edge
		 * standing proud of the course below. */
		void Slate(Pen& B, const Plan& P, int Side, double XA, double XB, double S0, double S1, double Lift, const Col& C, bool bVergeA, bool bVergeB)
		{
			const double T = P.SlateT;
			const double HB = 1.9 * T + Lift, HT = 1.15 * T + 0.5 * Lift;
			const RoofPt A0 = RoofAt(P, Side, XA, S0, HB), A1 = RoofAt(P, Side, XB, S0, HB);
			const RoofPt A2 = RoofAt(P, Side, XB, S1, HT), A3 = RoofAt(P, Side, XA, S1, HT);
			const V3 N = A0.N;
			const Col Lo = C, Hi = Scaled(C, 0.8);
			B.Quad(A0.P, A1.P, A2.P, A3.P, Lo, Lo, Hi, Hi, N);
			const RoofPt U0 = RoofAt(P, Side, XA, S0, 1.1 * T), U1 = RoofAt(P, Side, XB, S0, 1.1 * T);
			const V3 Down = Norm(RoofAt(P, Side, 0.0, std::max(0.0, S0 - 0.05), 0.0).P - RoofAt(P, Side, 0.0, S0 + 0.05, 0.0).P);
			B.Quad1(U0.P, U1.P, A1.P, A0.P, Scaled(C, 0.5), Down);
			if (bVergeA || bVergeB)
			{
				const double X = bVergeA ? XA : XB;
				const RoofPt D0 = RoofAt(P, Side, X, S0, -0.03), D1 = RoofAt(P, Side, X, S1, -0.03);
				B.Quad1(D0.P, D1.P, bVergeA ? A3.P : A2.P, bVergeA ? A0.P : A1.P, Scaled(C, 0.62), V3{ bVergeA ? -1.0 : 1.0, 0.0, 0.0 });
			}
		}

		/** A chimney's stack on a gable (End -1 or +1): its courses, a corbelled cap and two clay pots, the mortar fillet where
		 * the slates meet it; a shell's left standing on its last gable, sooted, a pot gone, no roof to meet. */
		void StackNear(Pen& B, const Plan& P, int End)
		{
			const bool bShell = P.Hurt == 3;
			const double XI = End * (0.5 * P.L - 0.62), XO = End * (0.5 * P.L + P.GO);
			const double XM = 0.5 * (XI + XO), HX = 0.5 * std::fabs(XO - XI), HY = 0.48;
			const double ZBot = DeckZ(P, HY) - 0.3, ZTop = P.Ridge + 1.25;
			double Z = ZBot;
			int K = 0;
			while (Z < ZTop - 1e-6)
			{
				const double Z1 = std::min(ZTop, Z + 0.29);
				const uint64_t H = Hash(P.Key, 0xC4, static_cast<uint64_t>(K * 2 + (End > 0)));
				const double In = (K % 2 ? 0.004 : 0.0);
				Col C = Varied(P.K.Dressed, 0.86 + 0.12 * Unit(H), 0.0);
				C = Mixed(C, P.K.Soot, 0.4 * Smooth(ZTop - 1.2, ZTop, Z1));
				if (bShell) C = Mixed(C, P.K.Soot, 0.3 + 0.25 * Noise01(Z / 0.6, 3.0, P.Key ^ 0x5C));
				B.Box(V3{ XM, 0.0, 0.5 * (Z + Z1) }, V3{ HX - In, 0.0, 0.0 }, V3{ 0.0, HY - In, 0.0 }, V3{ 0.0, 0.0, 0.5 * (Z1 - Z) - 0.004 }, C, 16 | 32);
				Z = Z1;
				++K;
			}
			const Col Cap = Mixed(P.K.Dressed, P.K.Soot, 0.45);
			B.Box(V3{ XM, 0.0, ZTop + 0.06 }, V3{ HX + 0.07, 0.0, 0.0 }, V3{ 0.0, HY + 0.07, 0.0 }, V3{ 0.0, 0.0, 0.06 }, Cap, 0);
			const Col Flue = Rgb(30, 27, 25);
			B.Box(V3{ XM, 0.0, ZTop + 0.16 }, V3{ HX - 0.02, 0.0, 0.0 }, V3{ 0.0, HY - 0.02, 0.0 }, V3{ 0.0, 0.0, 0.04 }, Mixed(Cap, P.K.Soot, 0.4), 0, &Flue);
			for (int S = -1; S <= 1; S += 2)
			{
				if (bShell && S > 0) continue; // (one pot left of the two)
				const V3 Foot{ XM, S * 0.2, ZTop + 0.2 };
				const Col Pot = Mixed(Varied(P.K.Pot, 0.92 + 0.1 * Unit(Hash(P.Key, 0x9071, static_cast<uint64_t>(S + 2 + (End > 0) * 4))), 0.0), P.K.Soot, 0.25);
				B.Tube(Foot, Foot + Up * 0.42, 0.115, 0.09, 8, Pot, false);
				B.Tube(Foot + Up * 0.42, Foot + Up * 0.48, 0.115, 0.115, 8, Pot, true, true);
				B.Tube(Foot + Up * 0.46, Foot + Up * 0.485, 0.085, 0.085, 8, Flue, true);
			}
			// (the mortar fillet where the slates meet the stack)
			if (!bShell)
				for (int S = -1; S <= 1; S += 2)
				{
					const RoofPt F0 = RoofAt(P, S, XI, SlopeAt(P, HY), 2.0 * P.SlateT), F1 = RoofAt(P, S, XO, SlopeAt(P, HY), 2.0 * P.SlateT);
					const V3 W0{ XI, S * HY, F0.P.Z + 0.06 }, W1{ XO, S * HY, F1.P.Z + 0.06 };
					B.Quad1(F0.P + V3{ 0.0, S * 0.05, 0.0 }, F1.P + V3{ 0.0, S * 0.05, 0.0 }, W1, W0, Mixed(P.K.Mortar, Rgb(110, 106, 96), 0.4), V3{ 0.0, S * 1.0, 0.6 });
				}
		}

		void RoofNear(Pen& B, const Plan& P, bool bInside)
		{
			if (P.Hurt == 3)
			{
				if (ChimneyAt(P, P.Keep)) StackNear(B, P, P.Keep);
				return;
			}
			const double X0 = -0.5 * P.L - P.GO, X1 = 0.5 * P.L + P.GO;
			const int NC = static_cast<int>(std::ceil(P.Ls / P.Gauge - 1e-6));
			const bool bTile = P.S.Roof == 1;
			for (int Side = -1; Side <= 1; Side += 2)
			{
				// The deck under the slates, row by row along the courses (dark where it shows between them), the boards
				// under it seen from below the eaves and from inside.
				for (int K = 0; K < NC; ++K)
				{
					const double S0 = K * P.Gauge, S1 = std::min(P.Ls, (K + 1) * P.Gauge), SM = 0.5 * (S0 + S1);
					double RunFrom = 1e9;
					for (double X = X0; X <= X1 + 0.051; X += 0.1)
					{
						const double XC = std::min(X, X1);
						const bool bThere = X <= X1 + 1e-9 && DeckThere(P, Side, XC, SM);
						if (bThere && RunFrom > 1e8) RunFrom = XC;
						if ((!bThere || X > X1 - 1e-9) && RunFrom < 1e8)
						{
							const double RunTo = bThere ? X1 : XC - 0.1 + 0.05;
							const double RA = std::max(X0, RunFrom == X0 ? X0 : RunFrom - 0.05), RB = std::min(X1, RunTo);
							if (RB > RA + 0.01)
							{
								const RoofPt T0 = RoofAt(P, Side, RA, S0, 0.0), T1 = RoofAt(P, Side, RB, S0, 0.0), T2 = RoofAt(P, Side, RB, S1, 0.0), T3 = RoofAt(P, Side, RA, S1, 0.0);
								B.Quad1(T0.P, T1.P, T2.P, T3.P, P.K.Deck, T0.N);
								const RoofPt U0 = RoofAt(P, Side, RA, S0, -0.03), U1 = RoofAt(P, Side, RB, S0, -0.03), U2 = RoofAt(P, Side, RB, S1, -0.03), U3 = RoofAt(P, Side, RA, S1, -0.03);
								Col Bd = Varied(P.K.Boards, 0.9 + 0.12 * Unit(Hash(P.Key, 0xB0A, static_cast<uint64_t>(K * 2 + (Side > 0)))), 0.0);
								if (P.Hurt != 0) Bd = Mixed(Bd, P.K.Char, 0.5 * Smooth(1.9, 1.0, HoleDist(P, Side, 0.5 * (RA + RB), SM)));
								B.Quad1(U0.P, U1.P, U2.P, U3.P, Bd, U0.N * -1.0);
								// (a broken edge of the boards)
								if (RA > X0 + 1e-6) B.Quad1(T0.P, T3.P, U3.P, U0.P, Scaled(Bd, 0.7), V3{ -1.0, 0.0, 0.0 });
								if (RB < X1 - 1e-6) B.Quad1(T1.P, T2.P, U2.P, U1.P, Scaled(Bd, 0.7), V3{ 1.0, 0.0, 0.0 });
								if (K == 0) B.Quad1(T0.P, T1.P, U1.P, U0.P, Scaled(Bd, 0.6), Norm(T0.P - T3.P));
							}
							RunFrom = 1e9;
						}
					}
				}
				// The slates, course by course from the eaves, each course half a slate along from the one below.
				for (int K = 0; K < NC; ++K)
				{
					const double S0 = K * P.Gauge, S1 = std::min(P.Ls + 0.01, (K + 1) * P.Gauge);
					Rng R(Hash(P.Key ^ 0x51A7E, static_cast<uint64_t>(K), static_cast<uint64_t>(Side + 2)));
					double X = X0 - (K % 2 ? 0.5 * P.SlateW : 0.0) - R.Range(0.0, 0.06);
					while (X < X1 - 1e-6)
					{
						const double W = P.SlateW * R.Range(0.96, 1.04);
						const double XA = std::max(X0, X), XB = std::min(X1, X + W);
						X += W;
						if (XB - XA < 0.04) continue;
						const double XM = 0.5 * (XA + XB), SM = 0.5 * (S0 + S1);
						// (none where a chimney stands through the roof)
						bool bChimney = false;
						for (int End = -1; End <= 1; End += 2)
							if (ChimneyAt(P, End) && std::max(End * XA, End * XB) > 0.5 * P.L - 0.62 && RoofAt(P, Side, XM, S0, 0.0).P.Y * Side < 0.5) bChimney = true;
						if (bChimney) continue;
						const uint64_t H = Hash(P.Key, static_cast<uint64_t>(K * 512 + (Side > 0)), static_cast<uint64_t>(std::llround((XA + 20.0) * 50.0)));
						double Slip = 0.0, Lift = 0.0;
						Col C = Varied(P.K.Roof, 0.88 + 0.22 * Unit(H), bTile ? (Unit(Mix(H)) - 0.5) * 0.08 : 0.0);
						const double U = Unit(Mix(H ^ 0x77));
						if (!bTile && U < 0.08) C = Mixed(C, Rgb(98, 86, 84), 0.5);            // (a rustier slate)
						else if (!bTile && U < 0.14) C = Mixed(C, Rgb(80, 90, 86), 0.5);       // (a greener one)
						else if (bTile && U < 0.1) C = Scaled(C, 0.78);                         // (an overfired tile)
						// lichen and moss toward the eaves
						const double Low = Smooth(2.2, 0.0, S0);
						C = Mixed(C, bTile ? Rgb(96, 92, 62) : Rgb(138, 140, 118), (bTile ? 0.45 : 0.32) * Low * Smooth(0.45, 0.8, Noise01(XM / 0.8, S0 / 0.6, P.Key ^ 0x3055)));
						if (Side < 0) C = Mixed(C, Rgb(70, 78, 50), 0.3 * Smooth(1.6, 0.0, S0) * Noise01(XM / 0.5, S0 / 0.4, P.Key ^ 0x3056));
						// the roof's hole: slates gone inside it, slipped and cracked round it, sooted
						const double Hd = HoleDist(P, Side, XM, SM);
						if (Hd < 1.0) continue;
						if (Hd < 1.35 && U < 0.55)
						{
							Slip = 0.04 + 0.1 * Unit(Mix(H ^ 0x5));
							Lift = 0.012;
						}
						// a fallen gable: the courses broken off raggedly toward its end
						if (P.Hurt == 2)
						{
							const double Cut = CutAt(P, Side, SM);
							if (P.Broken * XM > Cut) continue;
							if (P.Broken * XM > Cut - 0.5 && U < 0.5)
							{
								Slip = 0.05 + 0.12 * Unit(Mix(H ^ 0x6));
								Lift = 0.015;
							}
							C = Mixed(C, P.K.Soot, 0.6 * Smooth(Cut - 1.4, Cut, P.Broken * XM));
						}
						if (P.Hurt == 1) C = Mixed(C, P.K.Soot, 0.7 * Smooth(1.8, 1.0, Hd));
						// (an odd slate slipped on a whole roof too)
						if (P.Hurt == 0 && Unit(Mix(H ^ 0x99)) < 0.004) Slip = 0.06;
						Slate(B, P, Side, XA + 0.003, XB - 0.003, std::max(0.0, S0 - Slip), std::max(0.02, S1 - Slip), Lift, C, XA <= X0 + 1e-9, XB >= X1 - 1e-9);
					}
				}
				// The rafters' feet under the eaves' overhang, from the wall's face to the edge.
				const double SW = SlopeAt(P, 0.5 * P.D);
				for (double X = -0.5 * P.L + 0.3; X <= 0.5 * P.L - 0.3 + 1e-6; X += 0.55)
				{
					if (!DeckThere(P, Side, X, 0.2)) continue;
					const RoofPt A = RoofAt(P, Side, X, 0.03, -0.03), Bp = RoofAt(P, Side, X, SW + 0.05, -0.03);
					B.Beam(A.P - A.N * 0.055, Bp.P - Bp.N * 0.055, 0.07, 0.11, Rgb(84, 66, 50), A.N, 0);
				}
				// Inside (only where the roof is open): the rafters, the battens across a hole, the purlins.
				if (bInside && P.Hurt >= 1)
				{
					for (double X = -0.5 * P.L + P.Thick + 0.25; X <= 0.5 * P.L - P.Thick - 0.25 + 1e-6; X += 0.5)
					{
						const double SA = SlopeAt(P, 0.5 * P.D - P.Thick);
						double From = -1.0;
						for (double S = SA; S <= P.Ls + 1e-6; S += 0.1)
						{
							const double Hd = HoleDist(P, Side, X, S);
							bool bThere = Hd > 0.45;
							if (P.Hurt == 2) bThere = bThere && P.Broken * X < CutAt(P, Side, S) + 0.55 + 0.4 * Unit(Hash(P.Key, 0x2AF, static_cast<uint64_t>(std::llround(X * 10.0))));
							const bool bEnd = S + 0.1 > P.Ls + 1e-6;
							if (bThere && From < 0.0) From = S;
							if ((!bThere || bEnd) && From >= 0.0)
							{
								const double To = bThere ? P.Ls : S;
								if (To > From + 0.08)
								{
									const RoofPt A = RoofAt(P, Side, X, From, -0.03), Bp = RoofAt(P, Side, X, To, -0.03);
									const double Burnt = std::max(Smooth(1.4, 0.5, HoleDist(P, Side, X, 0.5 * (From + To))), P.Hurt == 2 ? Smooth(CutAt(P, Side, To) - 1.5, CutAt(P, Side, To), P.Broken * X) : 0.0);
									B.Beam(A.P - A.N * 0.07, Bp.P - Bp.N * 0.07, 0.075, 0.13, Mixed(Rgb(96, 76, 56), P.K.Char, 0.85 * Burnt), A.N, 0);
								}
								From = -1.0;
							}
						}
					}
					if (P.Hurt == 1)
						for (int K = 0; K < NC; ++K)
						{
							const double S = K * P.Gauge + 0.05;
							double From = 1e9;
							for (double X = P.HoleX - P.HoleRX * 1.6; X <= P.HoleX + P.HoleRX * 1.6 + 1e-6; X += 0.05)
							{
								const double Hd = HoleDist(P, Side, X, S);
								const bool bIn = Hd < 0.95 && Hd > 0.5 + 0.25 * Unit(Hash(P.Key, 0xBA7, static_cast<uint64_t>(K)));
								if (bIn && From > 1e8) From = X;
								if (!bIn && From < 1e8)
								{
									if (X > From + 0.06)
									{
										const RoofPt A = RoofAt(P, Side, From, S, -0.012), Bp = RoofAt(P, Side, X, S, -0.012);
										B.Beam(A.P, Bp.P, 0.04, 0.024, Mixed(Rgb(110, 90, 66), P.K.Char, 0.6), A.N, 0);
									}
									From = 1e9;
								}
							}
						}
					if (P.Hurt == 2)
					{
						// The purlins under the rafters, run on past the broken end; the last one sagging.
						for (int K = 0; K < 2; ++K)
						{
							const double S = P.Lk + P.Lm * (K == 0 ? 0.32 : 0.66);
							const double End = P.Broken * (CutAt(P, Side, S) + 0.7 + 0.6 * Unit(Hash(P.Key, 0x9A1, static_cast<uint64_t>(K * 2 + (Side > 0)))));
							const RoofPt A = RoofAt(P, Side, -P.Broken * (0.5 * P.L - P.Thick), S, -0.25);
							const RoofPt M = RoofAt(P, Side, End - P.Broken * 1.2, S, -0.25);
							RoofPt Bp = RoofAt(P, Side, End, S, -0.25);
							if (K == 1) Bp.P = Bp.P - Up * 0.45;
							const Col Wood = Mixed(Rgb(92, 72, 54), P.K.Char, 0.5);
							B.Beam(A.P, M.P, 0.16, 0.2, Wood, A.N, 0);
							B.Beam(M.P, Bp.P, 0.16, 0.2, Mixed(Wood, P.K.Char, 0.6), A.N, 0);
						}
					}
				}
			}
			// The ridge: half-round tiles end to end, bedded on lime mortar either side.
			{
				const double RX0 = ChimneyAt(P, -1) ? -0.5 * P.L + 0.6 : X0 - 0.02, RX1 = ChimneyAt(P, 1) ? 0.5 * P.L - 0.6 : X1 + 0.02;
				const double RW = 0.36;
				const int Tiles = std::max(1, static_cast<int>(std::lround((RX1 - RX0) / RW)));
				const double ZR = P.Ridge + 0.055;
				for (int K = 0; K < Tiles; ++K)
				{
					const double XA = RX0 + (RX1 - RX0) * K / Tiles + 0.004, XB = RX0 + (RX1 - RX0) * (K + 1) / Tiles - 0.004, XM = 0.5 * (XA + XB);
					if (P.Hurt == 2 && P.Broken * XM > std::min(CutAt(P, 1, P.Ls), CutAt(P, -1, P.Ls)) - 0.2) continue;
					if (HoleDist(P, P.HoleSide, XM, P.Ls) < 1.0) continue;
					const uint64_t H = Hash(P.Key, 0x41D6E, static_cast<uint64_t>(K));
					Col C = Varied(P.K.Ridge, 0.9 + 0.16 * Unit(H), 0.0);
					C = Mixed(C, Rgb(150, 148, 120), 0.3 * Smooth(0.5, 0.85, Noise01(XM / 0.7, 2.0, P.Key ^ 0x41D)));
					if (P.Hurt == 1) C = Mixed(C, P.K.Soot, 0.6 * Smooth(1.8, 1.0, HoleDist(P, P.HoleSide, XM, P.Ls)));
					const double Rr = (K % 2 ? 0.148 : 0.142);
					const int Segs = 6;
					std::vector<uint32_t> Ring;
					for (int J = 0; J <= Segs; ++J)
					{
						const double T = Pi * J / Segs;
						const V3 Rd{ 0.0, std::cos(T), std::sin(T) };
						const Col CJ = Scaled(C, 0.86 + 0.18 * std::sin(T));
						Ring.push_back(B.Vert(V3{ XA, Rr * Rd.Y, ZR + Rr * Rd.Z - 0.06 }, Rd, CJ));
						Ring.push_back(B.Vert(V3{ XB, Rr * Rd.Y, ZR + Rr * Rd.Z - 0.06 }, Rd, CJ));
					}
					for (int J = 0; J < Segs; ++J)
					{
						const double T = Pi * (J + 0.5) / Segs;
						const V3 Out{ 0.0, std::cos(T), std::sin(T) };
						B.Tri(Ring[J * 2], Ring[J * 2 + 2], Ring[J * 2 + 3], Out);
						B.Tri(Ring[J * 2], Ring[J * 2 + 3], Ring[J * 2 + 1], Out);
					}
					for (int E = 0; E < 2; ++E)
					{
						const double X = E == 0 ? XA : XB;
						const V3 N{ E == 0 ? -1.0 : 1.0, 0.0, 0.0 };
						const uint32_t Mid = B.Vert(V3{ X, 0.0, ZR - 0.06 }, N, Scaled(C, 0.7));
						for (int J = 0; J < Segs; ++J)
						{
							const double T0 = Pi * J / Segs, T1 = Pi * (J + 1) / Segs;
							const uint32_t I0 = B.Vert(V3{ X, Rr * std::cos(T0), ZR - 0.06 + Rr * std::sin(T0) }, N, Scaled(C, 0.7));
							const uint32_t I1 = B.Vert(V3{ X, Rr * std::cos(T1), ZR - 0.06 + Rr * std::sin(T1) }, N, Scaled(C, 0.7));
							B.Tri(Mid, I0, I1, N);
						}
					}
					// (the mortar bed down to the slates either side)
					for (int S = -1; S <= 1; S += 2)
					{
						const V3 E0{ XA, S * Rr, ZR - 0.06 }, E1{ XB, S * Rr, ZR - 0.06 };
						const RoofPt F0 = RoofAt(P, S, XA, SlopeAt(P, Rr + 0.09), 1.6 * P.SlateT), F1 = RoofAt(P, S, XB, SlopeAt(P, Rr + 0.09), 1.6 * P.SlateT);
						B.Quad1(E0, E1, F1.P, F0.P, Mixed(P.K.Mortar, Rgb(120, 116, 104), 0.35), V3{ 0.0, S * 0.7, 0.7 });
					}
				}
			}
			// The chimneys: stone stacks on the gables, their courses, a corbelled cap and two clay pots.
			for (int End = -1; End <= 1; End += 2)
				if (ChimneyAt(P, End)) StackNear(B, P, End);
			// The gutters on their brackets under the eaves' edge, their downpipes to the ground, a water butt (a barn and a
			// byre have none: the rain off their eaves).
			for (int Side = -1; Side <= 1 && P.Kind == 0; Side += 2)
			{
				const double GV = 0.5 * P.D + P.Over + 0.03, GZ = P.Ze - 0.09, GR = 0.065;
				double GX0 = X0 + 0.06, GX1 = X1 - 0.06;
				if (P.Hurt == 2)
				{
					if (P.Broken > 0) GX1 = std::min(GX1, CutAt(P, Side, 0.2) - 0.3);
					else GX0 = std::max(GX0, -(CutAt(P, Side, 0.2) - 0.3));
				}
				const Col Zn = Mixed(P.K.Zinc, Rgb(70, 72, 70), 0.2);
				const int Segs = 5;
				std::vector<uint32_t> Ring;
				for (int J = 0; J <= Segs; ++J)
				{
					const double T = Pi * J / Segs; // 0 the outer lip, round underneath to the inner
					const V3 Rd{ 0.0, Side * std::cos(T), -std::sin(T) };
					const V3 C{ 0.0, Side * GV, GZ };
					Ring.push_back(B.Vert(V3{ GX0, C.Y + Rd.Y * GR, C.Z + Rd.Z * GR }, Rd, Scaled(Zn, 0.85 + 0.15 * std::cos(T))));
					Ring.push_back(B.Vert(V3{ GX1, C.Y + Rd.Y * GR, C.Z + Rd.Z * GR }, Rd, Scaled(Zn, 0.85 + 0.15 * std::cos(T))));
				}
				for (int J = 0; J < Segs; ++J)
				{
					const double T = Pi * (J + 0.5) / Segs;
					const V3 Out{ 0.0, Side * std::cos(T), -std::sin(T) };
					B.Tri(Ring[J * 2], Ring[J * 2 + 2], Ring[J * 2 + 3], Out);
					B.Tri(Ring[J * 2], Ring[J * 2 + 3], Ring[J * 2 + 1], Out);
					B.Tri(Ring[J * 2], Ring[J * 2 + 3], Ring[J * 2 + 2], Out * -1.0);
					B.Tri(Ring[J * 2], Ring[J * 2 + 1], Ring[J * 2 + 3], Out * -1.0);
				}
				// (the bead along its lip)
				B.Tube(V3{ GX0, Side * (GV + GR), GZ }, V3{ GX1, Side * (GV + GR), GZ }, 0.009, 0.009, 4, Zn, false);
				for (double X = GX0 + 0.3; X < GX1; X += 0.9)
					B.Beam(V3{ X, Side * (GV - GR - 0.01), GZ + 0.01 }, V3{ X, Side * (GV + GR + 0.005), GZ - 0.02 }, 0.02, 0.006, P.K.Iron, Up, 0);
				// The downpipe at one end, its swan neck back to the wall, its shoe; the water butt under the back's.
				const int PE = Side > 0 ? 1 : -1;
				const double PX = PE > 0 ? GX1 - 0.12 : GX0 + 0.12;
				if (P.Hurt == 2 && PE == P.Broken) continue;
				const double WV = 0.5 * P.D + 0.075, PR = 0.042;
				const V3 G0{ PX, Side * GV, GZ - 0.05 }, N1{ PX, Side * GV, GZ - 0.2 }, N2{ PX, Side * WV, GZ - 0.48 };
				const double Foot = Side < 0 ? 0.95 : 0.18;
				B.Tube(G0, N1, PR, PR, 6, Zn, false);
				B.Tube(N1, N2, PR, PR, 6, Zn, false);
				B.Tube(N2, V3{ PX, Side * WV, Foot + 0.12 }, PR, PR, 6, Zn, false);
				B.Tube(V3{ PX, Side * WV, Foot + 0.12 }, V3{ PX, Side * (WV + 0.12), Foot }, PR, PR, 6, Zn, true);
				for (double Z = GZ - 0.6; Z > Foot + 0.3; Z -= 1.6)
					B.Tube(V3{ PX, Side * WV, Z }, V3{ PX, Side * WV, Z + 0.04 }, PR + 0.012, PR + 0.012, 6, P.K.Iron, true, true);
				if (Side < 0)
				{
					// The water butt: an oak cask on two stones, its iron hoops, the water dark in it.
					const V3 C{ PX, Side * (WV + 0.3), -0.2 };
					const Col Oak = Rgb(88, 72, 54);
					const int NS = 12;
					const double Hh[4] = { 0.0, 0.33, 0.66, 1.0 }, Rr[4] = { 0.29, 0.33, 0.33, 0.29 };
					std::vector<uint32_t> Rings;
					for (int J = 0; J < 4; ++J)
						for (int S = 0; S <= NS; ++S)
						{
							const double T = 2.0 * Pi * S / NS;
							const V3 Rd{ std::cos(T), std::sin(T), (J == 0 ? -0.25 : J == 3 ? 0.25 : 0.0) };
							Rings.push_back(B.Vert(C + V3{ Rr[J] * std::cos(T), Rr[J] * std::sin(T), 0.95 * Hh[J] }, Norm(Rd), Varied(Oak, 0.85 + 0.25 * Unit(Hash(P.Key, 0xCA5, static_cast<uint64_t>(S % NS))), 0.0)));
						}
					for (int J = 0; J < 3; ++J)
						for (int S = 0; S < NS; ++S)
						{
							const uint32_t A0 = Rings[J * (NS + 1) + S], A1 = Rings[J * (NS + 1) + S + 1], B0 = Rings[(J + 1) * (NS + 1) + S], B1 = Rings[(J + 1) * (NS + 1) + S + 1];
							const double T = 2.0 * Pi * (S + 0.5) / NS;
							const V3 Out{ std::cos(T), std::sin(T), 0.0 };
							B.Tri(A0, A1, B1, Out);
							B.Tri(A0, B1, B0, Out);
						}
					for (double Z : { 0.12, 0.83 })
						B.Tube(C + Up * Z, C + Up * (Z + 0.035), Z < 0.5 ? 0.318 : 0.318, 0.318, NS, P.K.Iron, false);
					B.Tube(C + Up * 0.9, C + Up * 0.9, 0.27, 0.27, NS, Rgb(22, 24, 22), true);
				}
			}
		}

		// ------------------------------------------------------------------ inside, rubble, beams

		void Heap(Pen& B, const Plan& P, V3 O, double RX, double RY, double High, double Yaw, int NA, int NR, uint64_t Seed, bool bBlocks);

		/** A shell's rubble at the foot of its walls, outside, under where each came down lowest (the art director's round
		 * 2). Level 0 near, 1 middle, 2 far. */
		void FootHeaps(Pen& B, const Plan& P, int Level)
		{
			for (int F = 0; F < 4; ++F)
			{
				const Face Fc = FaceOf(P, F);
				if (KeptGable(P, Fc)) continue;
				// (where the wall's top, taken over a metre and a half of it, is lowest)
				double Best = 1e9, BestA = 0.5 * Fc.Len;
				for (double A = 1.0; A < Fc.Len - 1.0 + 1e-6; A += 0.25)
				{
					double Sum = 0.0;
					int N = 0;
					for (double D = -0.75; D <= 0.75 + 1e-6; D += 0.25)
					{
						Sum += TopOf(P, Fc, F, A + D);
						++N;
					}
					if (Sum / N < Best)
					{
						Best = Sum / N;
						BestA = A;
					}
				}
				const double Drop = std::clamp(P.EW - Best, 0.0, 3.0);
				const double Len = 1.3 + 0.35 * Drop, Out = 0.6 + 0.2 * Drop, High = 0.3 + 0.16 * Drop;
				const V3 C = At(Fc, BestA, -0.15, 0.25 + 0.5 * Out);
				const uint64_t Seed = Hash(P.Key ^ 0xF007, static_cast<uint64_t>(F));
				Heap(B, P, C, Len, Out, High, std::atan2(Fc.T.Y, Fc.T.X), Level == 0 ? 16 : Level == 1 ? 10 : 6, Level == 0 ? 4 : Level == 1 ? 3 : 2, Seed, Level == 0);
			}
		}

		/** A heap of fallen stone and earth, lumpy, its rim sunk well into the ground; stones and slates on it. */
		void Heap(Pen& B, const Plan& P, V3 O, double RX, double RY, double High, double Yaw, int NA, int NR, uint64_t Seed, bool bBlocks)
		{
			const double C = std::cos(Yaw), S = std::sin(Yaw);
			std::vector<V3> Pt;
			std::vector<Col> Cl;
			for (int R = 0; R <= NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const double T = static_cast<double>(R) / NR, Ang = 2.0 * Pi * A / NA;
					const double Lump = 1.0 + 0.25 * Noise(A * 0.9, R * 1.3, Seed);
					const double X = RX * T * std::cos(Ang) * Lump, Y = RY * T * std::sin(Ang) * Lump;
					const double Z = High * (1.0 - T * T) * (0.78 + 0.32 * Noise01(A * 1.7, R * 0.7, Seed ^ 9)) - (R == NR ? 0.9 : 0.0);
					Pt.push_back({ O.X + X * C - Y * S, O.Y + X * S + Y * C, O.Z + Z });
					// (the wall's own stone, broken, its mortar's dust over it: the art director's round 3)
					Col K = Varied(Mixed(P.K.StoneMean, Rgb(158, 148, 130), 0.18 + 0.32 * Noise01(A * 2.1, R * 1.9, Seed ^ 3)), (bBlocks ? 0.9 : 1.0) + 0.2 * Unit(Hash(Seed, static_cast<uint64_t>(R * NA + A))), 0.0);
					if (P.Hurt == 3) K = Mixed(K, P.K.Soot, 0.12);
					Cl.push_back(K);
				}
			for (int R = 0; R < NR; ++R)
				for (int A = 0; A < NA; ++A)
				{
					const int A1 = (A + 1) % NA;
					const size_t I0 = static_cast<size_t>(R * NA + A), I1 = static_cast<size_t>(R * NA + A1), I2 = static_cast<size_t>((R + 1) * NA + A1), I3 = static_cast<size_t>((R + 1) * NA + A);
					B.Quad(Pt[I0], Pt[I1], Pt[I2], Pt[I3], Cl[I0], Cl[I1], Cl[I2], Cl[I3], Up);
				}
			if (!bBlocks) return;
			Rng R(Seed ^ 0xB10C);
			const int N = static_cast<int>(10.0 * RX * RY);
			for (int K = 0; K < N; ++K)
			{
				const double T = std::sqrt(R.Next()) * 0.95, Ang = R.Range(0.0, 2.0 * Pi);
				const double X = RX * T * std::cos(Ang), Y = RY * T * std::sin(Ang);
				const double Z = High * (1.0 - T * T) * 0.85;
				const V3 At3{ O.X + X * C - Y * S, O.Y + X * S + Y * C, O.Z + Z };
				const double Yw = R.Range(0.0, Pi), Tl = R.Range(-0.5, 0.5);
				const V3 AX{ std::cos(Yw) * std::cos(Tl), std::sin(Yw) * std::cos(Tl), std::sin(Tl) };
				const V3 AY = Norm(Cross(Up, AX));
				const V3 AZ = Cross(AX, AY);
				if (R.Next() < 0.25)
				{
					// (a fallen slate or tile, flat)
					B.Box(At3 + Up * 0.03, AX * R.Range(0.1, 0.16), AY * R.Range(0.07, 0.12), AZ * 0.006, Mixed(P.K.Roof, P.K.Soot, R.Range(0.0, 0.4)), 16);
				}
				else
				{
					const double Sz = R.Range(0.1, 0.24);
					B.Box(At3, AX * Sz * R.Range(1.0, 1.8), AY * Sz, AZ * Sz * R.Range(0.6, 1.0), StoneOwn(P, Hash(Seed, static_cast<uint64_t>(K))), 16);
				}
			}
		}

		void InsideNear(Pen& B, const Plan& P)
		{
			const double IX = 0.5 * P.L - P.Thick, IY = 0.5 * P.D - P.Thick;
			const int Storeys = std::max(1, P.S.Storeys);
			// The ground floor: flags and beaten earth, ash where it burnt.
			if (P.Hurt >= 2)
			{
				for (double X = -IX; X < IX - 1e-6; X += 1.0)
					for (double Y = -IY; Y < IY - 1e-6; Y += 1.0)
					{
						const double X1 = std::min(IX, X + 1.0), Y1 = std::min(IY, Y + 1.0);
						auto FC = [&](double A, double Bq) {
							Col C = Varied(Rgb(96, 88, 76), 0.9 + 0.15 * Noise(A / 0.6, Bq / 0.6, P.Key ^ 0xF1), 0.0);
							if (P.Hurt == 3) C = Mixed(C, Rgb(46, 42, 38), 0.6);
							return C;
						};
						B.Quad(V3{ X, Y, 0.02 }, V3{ X1, Y, 0.02 }, V3{ X1, Y1, 0.02 }, V3{ X, Y1, 0.02 }, FC(X, Y), FC(X1, Y), FC(X1, Y1), FC(X, Y1), Up);
					}
			}
			// The upper floors: boards on joists, the boards' underside the ceiling; cut off raggedly toward a fallen
			// gable, the joists sticking out past the boards; a shell's only their charred stubs in the walls.
			auto Floor = [&](double Z, bool bAttic) {
				const double Jd = 0.2, Bt = 0.04;
				const double StripW = 0.42;
				for (double Y = -IY; Y < IY - 1e-6; Y += StripW)
				{
					const double Y1 = std::min(IY, Y + StripW);
					double XEnd = IX;
					if (P.Hurt == 2) XEnd = std::min(IX, 0.5 * P.L - (bAttic ? 2.7 : 2.2) - 0.9 * Noise01(Y / 0.5, Z, P.Key ^ 0xF100));
					const double XA = P.Broken > 0 ? -IX : -XEnd, XB = P.Broken > 0 ? XEnd : IX;
					const Col Bd = Varied(P.K.Boards, 0.9 + 0.15 * Unit(Hash(P.Key, 0xF10, static_cast<uint64_t>(std::llround((Y + 10.0) * 10.0)))), 0.0);
					const Col Top = P.Hurt == 2 ? Mixed(Bd, Rgb(140, 130, 112), 0.35) : Bd; // (dust and plaster fallen on it)
					B.Quad1(V3{ XA, Y, Z }, V3{ XB, Y, Z }, V3{ XB, Y1, Z }, V3{ XA, Y1, Z }, Top, Up);
					B.Quad1(V3{ XA, Y, Z - Bt }, V3{ XB, Y, Z - Bt }, V3{ XB, Y1, Z - Bt }, V3{ XA, Y1, Z - Bt }, Scaled(Bd, 0.8), Up * -1.0);
					const double XE = P.Broken > 0 ? XB : XA;
					B.Quad1(V3{ XE, Y, Z }, V3{ XE, Y1, Z }, V3{ XE, Y1, Z - Bt }, V3{ XE, Y, Z - Bt }, Scaled(Bd, 0.7), V3{ P.Broken * 1.0, 0.0, 0.0 });
				}
				// joists across the house, every 0.6 m
				for (double X = -IX + 0.3; X < IX; X += 0.6)
				{
					double Y0 = -IY - 0.2, Y1 = IY + 0.2;
					const double Past = P.Hurt == 2 ? P.Broken * X - (0.5 * P.L - (bAttic ? 2.7 : 2.2) - 0.9) : -1.0;
					if (Past > 1.4) continue;
					const Col Jc = Mixed(Rgb(88, 70, 52), P.K.Char, P.Hurt == 2 ? 0.4 * Smooth(-1.0, 1.0, Past) : 0.0);
					if (Past > 0.0)
					{
						// (a joist past the boards' edge, broken, its end drooping)
						const double Droop = 0.15 + 0.35 * Unit(Hash(P.Key, 0x01D, static_cast<uint64_t>(std::llround(X * 10.0))));
						Y1 = Y0 + (Y1 - Y0) * (0.35 + 0.4 * Unit(Hash(P.Key, 0x01E, static_cast<uint64_t>(std::llround(X * 10.0)))));
						B.Beam(V3{ X, Y0, Z - Bt - 0.5 * Jd }, V3{ X, Y1, Z - Bt - 0.5 * Jd - Droop }, 0.12, Jd, Jc, Up, 0);
						continue;
					}
					B.Beam(V3{ X, Y0, Z - Bt - 0.5 * Jd }, V3{ X, Y1, Z - Bt - 0.5 * Jd }, 0.12, Jd, Jc, Up, 0);
				}
			};
			if (P.Hurt == 1) Floor(P.Loft, true);
			if (P.Hurt == 2)
			{
				for (int S = 1; S < Storeys && P.Kind == 0; ++S) Floor(S * 2.75 + 0.05, false);
				Floor(P.Loft, true);
			}
			// (hay on a barn's or a byre's loft, seen where its roof is open)
			if ((P.Kind == 1 || P.Kind == 2) && (P.Hurt == 1 || P.Hurt == 2))
			{
				Rng R(P.Key ^ 0x4A70);
				const int N = P.Kind == 1 ? 4 : 2;
				for (int K = 0; K < N; ++K)
				{
					const double X = R.Range(-IX + 1.2, IX - 1.2);
					if (P.Hurt == 2 && P.Broken * X > 0.5 * P.L - 3.6) continue;
					HayMound(B, P, V3{ X, R.Range(-IY + 1.0, IY - 1.0), P.Loft }, R.Range(1.0, 1.8), R.Range(0.8, 1.3), R.Range(0.5, 0.9), R.Range(0.0, Pi), Hash(P.Key ^ 0x4A71, static_cast<uint64_t>(K)), P.Hurt == 2 ? 0.8 : 1.0);
				}
			}
			if (P.Hurt == 3)
			{
				// charred joist stubs in the long walls, their pockets dark
				for (int S = 1; S <= (P.Kind == 0 ? Storeys : 1); ++S)
				{
					const double Z = P.Kind == 0 ? S * 2.75 - 0.15 : P.Loft - 0.25;
					for (double X = -IX + 0.3; X < IX; X += 0.6)
						for (int Side = -1; Side <= 1; Side += 2)
						{
							const Face Fc = FaceOf(P, Side > 0 ? 0 : 2);
							const double A = Side > 0 ? 0.5 * P.L - X : X + 0.5 * P.L;
							if (TopOf(P, Fc, Side > 0 ? 0 : 2, A) < Z + 0.25) continue;
							const uint64_t H = Hash(P.Key, 0x57B, static_cast<uint64_t>(std::llround((X + 20.0) * 10.0)) * 4 + static_cast<uint64_t>(Side + 1) + static_cast<uint64_t>(S) * 512);
							const double Len = 0.05 + 0.5 * Unit(H);
							const V3 W0{ X, Side * IY, Z }, W1{ X, Side * (IY - Len), Z - 0.1 * Unit(Mix(H)) };
							B.Quad1(W0 + V3{ -0.07, -Side * 0.002, -0.11 }, W0 + V3{ 0.07, -Side * 0.002, -0.11 }, W0 + V3{ 0.07, -Side * 0.002, 0.11 }, W0 + V3{ -0.07, -Side * 0.002, 0.11 }, Rgb(14, 13, 12), V3{ 0.0, -Side * 1.0, 0.0 });
							if (Unit(Mix(H ^ 3)) < 0.75) B.Beam(W0, W1, 0.11, 0.18, P.K.Char, Up, 0);
						}
				}
			}
			// Rubble: inside, and a heap where a gable fell.
			if (P.Hurt == 2)
			{
				Heap(B, P, V3{ P.Broken * (0.5 * P.L + 1.1), 0.0, -0.15 }, 2.3, 0.5 * P.D + 0.9, 1.5, 0.0, 18, 6, P.Key ^ 0x4EA9, true);
				Heap(B, P, V3{ P.Broken * (0.5 * P.L - 2.2), 0.0, 0.0 }, 1.6, IY - 0.2, 0.9, 0.0, 14, 4, P.Key ^ 0x4EAA, true);
			}
			if (P.Hurt == 3)
			{
				Heap(B, P, V3{ 0.0, 0.0, 0.0 }, IX - 0.6, IY - 0.3, 0.75, 0.0, 18, 5, P.Key ^ 0x4EAB, true);
				FootHeaps(B, P, 0);
				// fallen beams, charred, from the walls' tops down into the house
				Rng R(P.Key ^ 0xBEA);
				for (int K = 0; K < 4; ++K)
				{
					const double X0 = R.Range(-IX + 0.5, IX - 0.5), X1 = X0 + R.Range(-2.5, 2.5);
					const int S = R.Next() < 0.5 ? -1 : 1;
					double ZA = R.Range(2.2, 3.4);
					// (a barn's or a byre's resting on what still stands of its wall there)
					if (P.Kind != 0)
					{
						const Face Fw = FaceOf(P, S > 0 ? 0 : 2);
						ZA = std::max(0.7, std::min(ZA * P.HS, TopOf(P, Fw, S > 0 ? 0 : 2, S > 0 ? 0.5 * P.L - X0 : X0 + 0.5 * P.L) - 0.12));
					}
					const V3 A{ X0, S * (IY + 0.2), ZA }, Bp{ std::clamp(X1, -IX + 0.3, IX - 0.3), -S * R.Range(0.0, IY), R.Range(0.3, 0.8) };
					B.Beam(A, Bp, 0.17, 0.2, Mixed(P.K.Char, Rgb(70, 58, 46), R.Range(0.0, 0.3)), Up, 0);
				}
				// rafters' charred stubs leaning from the tops of the long walls that still stand to their plate
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const Face Fc = FaceOf(P, Side > 0 ? 0 : 2);
					for (double X = -IX + 0.4; X < IX; X += 0.5)
					{
						const double A = Side > 0 ? 0.5 * P.L - X : X + 0.5 * P.L;
						if (TopOf(P, Fc, Side > 0 ? 0 : 2, A) < P.EW - 1e-6) continue;
						const uint64_t H = Hash(P.Key, 0x5AF, static_cast<uint64_t>(std::llround((X + 20.0) * 10.0)) * 2 + static_cast<uint64_t>(Side > 0));
						if (Unit(H) < 0.4) continue;
						const double Len = 0.4 + 1.6 * Unit(Mix(H));
						const RoofPt A0 = RoofAt(P, Side, X, SlopeAt(P, 0.5 * P.D), -0.1), A1 = RoofAt(P, Side, X, SlopeAt(P, 0.5 * P.D) + Len, -0.1);
						B.Beam(A0.P, A1.P - Up * (0.2 * Len * Unit(Mix(H ^ 5))), 0.075, 0.12, P.K.Char, A0.N, 0);
					}
				}
			}
			if (P.Hurt == 1)
			{
				// what fell through the hole: slates and laths on the attic's boards, a heap of them under the eaves outside
				Rng R(P.Key ^ 0x51F);
				const double Z = P.Loft;
				const RoofPt C = RoofAt(P, P.HoleSide, P.HoleX, P.HoleS, 0.0);
				for (int K = 0; K < 28; ++K)
				{
					const V3 At3{ std::clamp(C.P.X + R.Range(-1.4, 1.4), -IX + 0.2, IX - 0.2), std::clamp(C.P.Y + R.Range(-1.0, 1.0), -IY + 0.2, IY - 0.2), Z + 0.01 };
					const double Yw = R.Range(0.0, Pi);
					const V3 AX{ std::cos(Yw), std::sin(Yw), 0.0 }, AY{ -std::sin(Yw), std::cos(Yw), 0.0 };
					if (R.Next() < 0.3) B.Beam(At3, At3 + AX * R.Range(0.4, 1.0) + Up * 0.03, 0.04, 0.025, Mixed(Rgb(110, 90, 66), P.K.Char, 0.5), Up, 0);
					else B.Box(At3 + Up * 0.008, AX * R.Range(0.1, 0.15), AY * R.Range(0.07, 0.11), Up * 0.007, Mixed(P.K.Roof, P.K.Soot, R.Range(0.0, 0.5)), 16);
				}
				Heap(B, P, V3{ std::clamp(P.HoleX, -0.5 * P.L + 1.5, 0.5 * P.L - 1.5), P.HoleSide * (0.5 * P.D + 0.75), -0.25 }, 1.1, 0.5, 0.28, 0.0, 12, 3, P.Key ^ 0x51E, true);
			}
		}

		// ------------------------------------------------------------------ the middle and far levels

		/**
		 * The middle level's stones (the art director's round 2: its wall to match the near level's at the switch, 90 m
		 * off): each course in stones of 0.55 to 1.25 m, each its own colour as the eye mixes it with its mortar from there,
		 * round the dressed quoins, jambs and lintels where the near level lays them; cut to the rake and the broken top.
		 */
		/**
		 * A barn's or a byre's broken top as its middle level draws it (the art director's barn round 2: not square notches,
		 * nor standing taller than the near level's): every 0.3 m the lowest the near level's top comes within 0.3 m of
		 * there (the near lays a stone only where the top clears its whole length), straight between; NaN where the wall
		 * stands whole there (its own top then), and none for the farmhouse, nor a shell's last gable (already a line).
		 */
		constexpr double MidStep = 0.3;
		std::vector<double> MidLine(const Plan& P, int F)
		{
			const Face Fc = FaceOf(P, F);
			std::vector<double> Z;
			if ((P.Kind != 1 && P.Kind != 2) || P.Hurt < 2 || KeptGable(P, Fc)) return Z;
			const int N = static_cast<int>(std::ceil(Fc.Len / MidStep));
			for (int I = 0; I <= N; ++I)
			{
				const double S = std::min(Fc.Len, I * MidStep);
				bool bNear = false;
				double Lo = IntactTop(P, Fc, std::clamp(S, 0.01, Fc.Len - 0.01));
				for (double X = S - 0.45; X <= S + 0.45 + 1e-9; X += 0.05)
				{
					const double Xc = std::clamp(X, 0.01, Fc.Len - 0.01);
					const double T = TopOf(P, Fc, F, Xc);
					if (T < IntactTop(P, Fc, Xc) - 1e-6)
					{
						bNear = true;
						if (std::fabs(X - S) <= 0.3 + 1e-9) Lo = std::min(Lo, T);
					}
				}
				Z.push_back(bNear ? Lo : std::numeric_limits<double>::quiet_NaN());
			}
			return Z;
		}
		/** The middle's top at A along the face: its line where it has one there, else the wall's own top. */
		double MidTop(const Plan& P, const Face& Fc, int F, const std::vector<double>& Line, double A)
		{
			if (!Line.empty())
			{
				const int I = std::clamp(static_cast<int>(std::floor(A / MidStep)), 0, static_cast<int>(Line.size()) - 2);
				const double Z0 = Line[static_cast<size_t>(I)], Z1 = Line[static_cast<size_t>(I) + 1];
				if (!std::isnan(Z0) && !std::isnan(Z1)) return Z0 + (Z1 - Z0) * std::clamp((A - I * MidStep) / MidStep, 0.0, 1.0);
			}
			return TopOf(P, Fc, F, A);
		}
		bool OnMidLine(const std::vector<double>& Line, double A)
		{
			if (Line.empty()) return false;
			const int I = std::clamp(static_cast<int>(std::floor(A / MidStep)), 0, static_cast<int>(Line.size()) - 2);
			return !std::isnan(Line[static_cast<size_t>(I)]) && !std::isnan(Line[static_cast<size_t>(I) + 1]);
		}

		void MidStones(Pen& B, const Plan& P, int F)
		{
			const Face Fc = FaceOf(P, F);
			const std::vector<double> Line = MidLine(P, F);
			auto Top = [&](double A) { return TopOf(P, Fc, F, A); };
			// (where the stones are cut: the rake's apex and kicks and the openings' sides, in every course; the broken
			// top's steps and the ragged rake's turns, only in the courses up near them)
			std::vector<std::pair<double, double>> Hard; // (where, and the height a course must reach to be cut there)
			if (Fc.bGable)
				for (double E : { 0.5 * Fc.Len, 0.5 * Fc.Len - P.Vb, 0.5 * Fc.Len + P.Vb }) Hard.push_back({ E, -1e9 });
			if (P.Hurt >= 2)
			{
				for (double E : PieceEdges(P, F, Fc.Len, 1.0))
				{
					if (OnMidLine(Line, E)) continue;
					const double Lo = std::min(Top(E - 0.005), Top(E + 0.005));
					if (RagAt(P, Fc, F, E - 0.005) || RagAt(P, Fc, F, E + 0.005)) Hard.push_back({ E, Lo - 0.6 });
					else if (std::fabs(Top(E - 0.005) - Top(E + 0.005)) > 1e-6) Hard.push_back({ E, Lo });
				}
				// (a barn's or a byre's line: cut at its points in the courses up near it)
				for (size_t I = 0; I < Line.size(); ++I)
					if (!std::isnan(Line[I])) Hard.push_back({ std::min(Fc.Len, I * MidStep), Line[I] - 0.35 });
			}
			// (an opening's sides: a church's cut only in the courses beside it, its walls being tall)
			struct Side
			{
				double A, Z0, Z1;
			};
			std::vector<Side> Sides;
			for (const Opening& O : P.Ops)
				if (O.Face == F)
				{
					if (P.Kind >= 3)
					{
						Sides.push_back({ O.A0, O.Z0, O.Z1 });
						Sides.push_back({ O.A1, O.Z0, O.Z1 });
						continue;
					}
					Hard.push_back({ O.A0, -1e9 });
					Hard.push_back({ O.A1, -1e9 });
				}
			std::sort(Hard.begin(), Hard.end());
			struct Run
			{
				double A0, A1;
				bool bDressed;
			};
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
				if (Fc.bGable ? Z0 > P.Ridge : Z0 >= P.EW - 1e-6) break;
				std::vector<std::pair<double, double>> Res = DressedIn(P, Fc, F, I, false);
				std::sort(Res.begin(), Res.end());
				// (the course's stretches: dressed, or stones laid along what is free)
				std::vector<Run> Runs;
				Rng R(Hash(P.Key ^ 0x31D, static_cast<uint64_t>(F), I));
				auto Lay = [&](double S0, double S1) {
					double X = S0;
					while (S1 - X > 0.02)
					{
						// (a tower's ashlar: its even blocks two to a cell)
						double Ln = P.Kind == 5 ? R.Range(1.0, 1.8) : R.Range(0.55, 1.25);
						if (S1 - X - Ln < 0.3) Ln = S1 - X;
						if (Ln > (P.Kind == 5 ? 2.0 : 1.4)) Ln = 0.5 * (S1 - X);
						Runs.push_back({ X, X + Ln, false });
						X += Ln;
					}
				};
				double From = 0.0;
				for (const auto& [R0, R1] : Res)
				{
					if (R0 > From + 0.02) Lay(From, std::min(R0, Fc.Len));
					if (R1 > From + 1e-4 && R0 < Fc.Len) Runs.push_back({ std::max(R0, From), std::min(R1, Fc.Len), true });
					From = std::max(From, R1);
				}
				if (From < Fc.Len - 0.02) Lay(From, Fc.Len);
				for (const Run& Ru : Runs)
				{
					if (Ru.A1 - Ru.A0 < 1e-4) continue;
					std::vector<double> Cuts{ Ru.A0 };
					for (const auto& [E, Lo] : Hard)
						if (E > Ru.A0 + 1e-4 && E < Ru.A1 - 1e-4 && Z1 > Lo - 1e-6) Cuts.push_back(E);
					for (const Side& Sd : Sides)
						if (Sd.A > Ru.A0 + 1e-4 && Sd.A < Ru.A1 - 1e-4 && Z1 > Sd.Z0 + 1e-6 && Z0 < Sd.Z1 - 1e-6) Cuts.push_back(Sd.A);
					std::sort(Cuts.begin() + 1, Cuts.end());
					Cuts.push_back(Ru.A1);
					const uint64_t H = Hash(P.Key ^ 0x31E, static_cast<uint64_t>(F * 4096) + I, static_cast<uint64_t>(std::llround(Ru.A0 * 100.0)));
					Col Own = Ru.bDressed ? Varied(P.K.Dressed, 0.94 + 0.1 * Unit(H), 0.0) : Scaled(Mixed(StoneOwn(P, H), P.K.Mortar, WallMortar), WallMid);
					// (a cart door's lintel: its oak beam)
					for (const Opening& O : P.Ops)
						if (Ru.bDressed && O.Face == F && O.What == OpCart && std::fabs(Z0 - O.Z1) < 1e-6 && Ru.A0 < O.A1 + LintelOf(O) && Ru.A1 > O.A0 - LintelOf(O)) Own = OakAt(P, 0.5 * (Ru.A0 + Ru.A1), H);
					for (size_t K = 0; K + 1 < Cuts.size(); ++K)
					{
						const double C0 = Cuts[K], C1 = Cuts[K + 1], CM = 0.5 * (C0 + C1);
						bool bHole = false;
						for (const Opening& O : P.Ops)
							if (O.Face == F && Z0 >= O.Z0 - 1e-6 && (O.Arch > 0.0 ? Z0 < O.Z1 - 1e-6 : Z1 <= O.Z1 + 1e-6) && CM > O.A0 && CM < O.A1) bHole = true;
						if (bHole) continue;
						double L0 = Z1 + 1.0, L1 = Z1 + 1.0;
						if (OnMidLine(Line, CM))
						{
							L0 = MidTop(P, Fc, F, Line, C0 + 1e-4);
							L1 = MidTop(P, Fc, F, Line, C1 - 1e-4);
							if (std::max(L0, L1) <= Z0 + 0.02) continue;
						}
						else if (Sloped(P, Fc, F, CM))
						{
							L0 = Top(C0 + 1e-4);
							L1 = Top(C1 - 1e-4);
						}
						else if (Top(CM) < Z1 - 1e-6) continue;
						Cut(B, Fc, C0, C1, Z0, Z1, L0, L1, Ru.bDressed ? 0.012 : 0.0, Fc.N, [&](double A, double Z) { return Weathered(P, Fc, F, A, Z, Own, true); });
					}
				}
			}
		}

		/** A barn's or a byre's opening on the middle and far levels: its leaves and its shutter flat in its reveal (painted
		 * on the wall far off), the dark where they stand open, an open leaf a slab swung out from its hinge (the middle's). */
		void BarnOpeningPlain(Pen& B, const Plan& P, const Opening& O, const Face& Fc, double Z1, bool bMid, bool bInner)
		{
			const double W = O.A1 - O.A0, AM = 0.5 * (O.A0 + O.A1), ZM = O.Z0 + 0.56 * (Z1 - O.Z0);
			const double Dp = RevealOf(P, O, bInner);
			const Col Dark = Rgb(16, 15, 14), Door = Scaled(O.What == OpCart ? P.K.Oak : P.K.Door, bMid ? 0.95 : 0.85);
			struct Pane
			{
				double A0, A1, Z0, Z1;
				bool bLeaf;
			};
			std::vector<Pane> Panes;
			if (O.What == OpCart)
			{
				Panes.push_back({ O.A0, AM, O.Z0, Z1, O.bLeaf && O.Swing < 0.01 });
				Panes.push_back({ AM, O.A1, O.Z0, Z1, O.bLeaf && O.Swing2 < 0.01 });
			}
			else if (O.What == OpPitch) Panes.push_back({ O.A0, O.A1, O.Z0, Z1, O.bLeaf && O.Swing < 0.01 });
			else if (O.What == OpStable)
			{
				Panes.push_back({ O.A0, O.A1, O.Z0, ZM, O.bLeaf });
				Panes.push_back({ O.A0, O.A1, ZM, Z1, O.bLeaf && O.Swing < 0.01 });
			}
			else Panes.push_back({ O.A0, O.A1, O.Z0, Z1, O.What == OpByreWin && O.Shutters == 1 });
			if (bMid)
			{
				const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
				B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, Z1, -Dp), At(Fc, O.A0, Z1, 0.0), Scaled(Rev, 0.95), Fc.T);
				B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A1, Z1, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
				if (Z1 >= O.Z1 - 1e-6) B.Quad1(At(Fc, O.A0, Z1, 0.0), At(Fc, O.A1, Z1, 0.0), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Scaled(Rev, 0.7), Up * -1.0);
				B.Quad1(At(Fc, O.A0, O.Z0 + 0.02, 0.0), At(Fc, O.A1, O.Z0 + 0.02, 0.0), At(Fc, O.A1, O.Z0 + 0.02, -Dp), At(Fc, O.A0, O.Z0 + 0.02, -Dp), O.What == OpCart ? Rgb(110, 98, 80) : Rev, Up);
			}
			for (const Pane& Q : Panes)
			{
				if (!Q.bLeaf && bMid && bInner) continue; // (the inside drawn behind it)
				const Col C = Q.bLeaf ? (O.What == OpByreWin ? P.K.Shutter : Door) : Dark;
				const double D = bMid ? -Dp : 0.012;
				B.Quad1(At(Fc, Q.A0, Q.Z0, D), At(Fc, Q.A1, Q.Z0, D), At(Fc, Q.A1, Q.Z1, D), At(Fc, Q.A0, Q.Z1, D), bMid ? C : Scaled(C, 0.9), Fc.N);
			}
			if (!bMid) return;
			// (an open leaf: a slab swung out from its hinge)
			auto Slab = [&](int Side, double Sw, double ZA, double ZB, double LW) {
				const V3 Along = Fc.T * (Side < 0 ? 1.0 : -1.0);
				const V3 Across = Norm(Along * std::cos(Sw) + Fc.N * std::sin(Sw));
				const V3 Out = Norm(Fc.N * std::cos(Sw) - Along * std::sin(Sw));
				const V3 Hinge = At(Fc, Side < 0 ? O.A0 : O.A1, 0.5 * (ZA + ZB), 0.0);
				B.Box(Hinge + Across * (0.5 * LW), Across * (0.5 * LW), Up * (0.5 * (ZB - ZA)), Out * 0.021, Door, 0);
			};
			if (O.bLeaf)
			{
				if (O.What == OpCart && O.Swing > 0.01) Slab(-1, O.Swing, O.Z0 + 0.04, Z1 - 0.04, 0.5 * W);
				if (O.What == OpCart && O.Swing2 > 0.01) Slab(1, O.Swing2, O.Z0 + 0.04, Z1 - 0.04, 0.5 * W);
				if (O.What == OpPitch && O.Swing > 0.01) Slab(-1, O.Swing, O.Z0 + 0.03, Z1 - 0.03, W);
				if (O.What == OpStable && O.Swing > 0.01) Slab(-1, O.Swing, ZM, Z1 - 0.04, W);
			}
			if (O.What == OpByreWin && O.Shutters == 0)
				B.Box(At(Fc, O.A1 + 0.5 * W, 0.5 * (O.Z0 + Z1), 0.05), Fc.T * (0.5 * W), Up * (0.5 * (Z1 - O.Z0)), Fc.N * 0.016, Varied(P.K.Shutter, 0.95, 0.0), 0);
			if (Silled(O.What)) B.Box(At(Fc, AM, O.Z0 - 0.045, 0.02), Fc.T * (0.5 * W + 0.08), Fc.N * 0.05, Up * 0.045, Varied(P.K.Dressed, 1.0, 0.0), 0);
		}

		/** A church's opening on the middle and far levels: far off its shape painted on the wall (glass, oak or dark); the
		 * middle's reveal and soffit, the glass or the leaves or the dark at its back, its sill, the ring of its voussoirs
		 * a pale band proud of the stones. */
		void ChurchOpeningPlain(Pen& B, const Plan& P, const Opening& O, const Face& Fc, bool bMid)
		{
			const double AM = 0.5 * (O.A0 + O.A1), SP = SpringOf(O), R = 0.5 * (O.A1 - O.A0), W = O.A1 - O.A0;
			const Col Dark = Rgb(15, 14, 13);
			const Col Glass = Mixed(P.K.Glass, P.K.Sky, 0.2), GlassT = Mixed(P.K.Glass, P.K.Sky, 0.32);
			const bool bLit = O.What == OpChWin && O.bGlass;
			const Col C = bLit ? Glass : O.What == OpChDoor ? Scaled(P.K.Door, bMid ? 0.92 : 0.8) : Dark;
			const Col CT = bLit ? GlassT : O.What == OpChDoor ? Scaled(P.K.Door, bMid ? 0.92 : 0.8) : Dark;
			if (!bMid)
			{
				ArchFill(B, Fc, O, 0.012, Scaled(C, 0.9), Scaled(CT, 0.9), 4);
				return;
			}
			const double Dp = RevealOf(P, O, false);
			ArchReveal(B, P, Fc, O, Dp, 5);
			ArchFill(B, Fc, O, -Dp, C, CT, 6);
			// (MidStones leaves the square over its head open to the course's top its crown is in: the stone between the
			// head and that square, behind the ring)
			if (O.What != OpHidden)
			{
				double ZT = O.Z1;
				for (double Cz : P.Course)
					if (Cz >= O.Z1 - 1e-6)
					{
						ZT = Cz;
						break;
					}
				const Col SC = Weathered(P, Fc, O.Face, AM, ZT, Scaled(Mixed(P.K.StoneMean, P.K.Mortar, WallMortar), WallMid));
				std::vector<double> Ts;
				for (int K = 0; K <= 8; ++K) Ts.push_back(Pi * K / 8);
				Ts.push_back(std::atan2(ZT - SP, R));
				Ts.push_back(Pi - std::atan2(ZT - SP, R));
				std::sort(Ts.begin(), Ts.end());
				auto Edge = [&](double T) {
					const double Cs = std::cos(T), Sn = std::sin(T);
					double Kk = 1e9;
					if (std::fabs(Cs) > 1e-9) Kk = std::min(Kk, R / std::fabs(Cs));
					if (Sn > 1e-9) Kk = std::min(Kk, (ZT - SP) / Sn);
					return ArcAt(Fc, AM, SP, Kk, T, -0.002);
				};
				for (size_t K = 0; K + 1 < Ts.size(); ++K)
				{
					if (Ts[K + 1] - Ts[K] < 1e-6) continue;
					B.Quad1(ArcAt(Fc, AM, SP, R, Ts[K], -0.002), ArcAt(Fc, AM, SP, R, Ts[K + 1], -0.002), Edge(Ts[K + 1]), Edge(Ts[K]), SC, Fc.N);
				}
			}
			// (the ring: a pale band, its stones as the eye mixes them from 90 m)
			const double R1 = R + RingOf(O);
			const int N = 7;
			for (int K = 0; K < N; ++K)
			{
				const double T0 = Pi * K / N, T1 = Pi * (K + 1) / N;
				const Col RC = Weathered(P, Fc, O.Face, AM + R1 * std::cos(0.5 * (T0 + T1)), SP + R1 * std::sin(0.5 * (T0 + T1)), Varied(P.K.Dressed, 0.95 + 0.06 * (K % 2), 0.0));
				B.Quad1(ArcAt(Fc, AM, SP, R, T0, 0.014), ArcAt(Fc, AM, SP, R, T1, 0.014), ArcAt(Fc, AM, SP, R1, T1, 0.014), ArcAt(Fc, AM, SP, R1, T0, 0.014), RC, Fc.N);
			}
			if (O.What == OpChDoor)
				B.Box(At(Fc, AM, -0.6, 0.34), Fc.T * (0.5 * W + 0.32), Fc.N * 0.29, Up * 0.44, Varied(P.K.Dressed, 0.86, 0.0), 0);
			if (Silled(O.What)) B.Box(At(Fc, AM, O.Z0 - 0.045, 0.02), Fc.T * (0.5 * W + 0.08), Fc.N * 0.05, Up * 0.045, Varied(P.K.Dressed, 1.0, 0.0), 0);
		}

		/**
		 * A church's wall far off (the art director's church round 2: it read as boards): flat at its stones' mean, in bands
		 * about a metre and a half tall (the tower's two) a shade apart, its coursing faint; the foot's band damp; the corners' quoins
		 * painted paler (none on the tower, all of it dressed); cut to the gable's rake; left out where another part
		 * covers it. Its openings are painted over it (ChurchOpeningPlain).
		 */
		void ChurchWallFar(Pen& B, const Plan& P, int F)
		{
			const Face Fc = FaceOf(P, F);
			auto Top = [&](double A) { return TopOf(P, Fc, F, A, 3.0); };
			const bool bTower = P.Kind == 5;
			const double QW = 0.42, ZTop = Fc.bGable ? P.Ridge + 0.1 : P.EW;
			std::vector<double> ZB{ -P.S.Below, 0.35, ZTop };
			if (Fc.bGable) ZB.push_back(P.EW);
			for (double Z = 0.35 + (bTower ? 2.0 : 1.4); Z < ZTop - 0.5; Z += bTower ? 2.0 : 1.4) ZB.push_back(Z <= P.EW ? Snap(P, Z) : Z);
			for (const Opening& O : P.Ops)
				if (O.Face == F && O.What == OpHidden)
				{
					ZB.push_back(std::max(O.Z0, -P.S.Below));
					ZB.push_back(std::min(O.Z1, ZTop));
				}
			std::sort(ZB.begin(), ZB.end());
			ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.05; }), ZB.end());
			const Col Mean = Scaled(Mixed(bTower ? Varied(P.K.Dressed, 0.87, 0.0) : P.K.Wall, Rgb(174, 172, 132), 0.04), 0.965); // (the tower's as weathered)
			for (size_t J = 0; J + 1 < ZB.size(); ++J)
			{
				const double Z0 = ZB[J], Z1 = ZB[J + 1], ZM = 0.5 * (Z0 + Z1);
				std::vector<double> AB{ 0.0, Fc.Len };
				if (!bTower && Z0 < P.EW - 0.01)
				{
					AB.push_back(QW);
					AB.push_back(Fc.Len - QW);
				}
				if (Fc.bGable && Z1 > P.EW + 1e-6)
					for (double E : { 0.5 * Fc.Len, 0.5 * Fc.Len - P.Vb, 0.5 * Fc.Len + P.Vb }) AB.push_back(E);
				for (const Opening& O : P.Ops)
					if (O.Face == F && O.What == OpHidden && O.Z0 < Z1 - 1e-6 && O.Z1 > Z0 + 1e-6)
					{
						AB.push_back(O.A0);
						AB.push_back(O.A1);
					}
				std::sort(AB.begin(), AB.end());
				AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), AB.end());
				const double Tone = 0.985 + 0.03 * Unit(Hash(P.Key ^ 0xFA2, static_cast<uint64_t>(F), J));
				for (size_t I = 0; I + 1 < AB.size(); ++I)
				{
					const double A0 = std::max(0.0, AB[I]), A1 = std::min(Fc.Len, AB[I + 1]), AM = 0.5 * (A0 + A1);
					if (A1 - A0 < 1e-4) continue;
					bool bHole = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && O.What == OpHidden && AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
					if (bHole) continue;
					double L0 = 1e9, L1 = 1e9;
					if (Fc.bGable && Z1 > P.EW + 1e-6)
					{
						L0 = Top(A0 + 1e-4);
						L1 = Top(A1 - 1e-4);
						if (std::max(L0, L1) <= Z0 + 1e-6) continue;
					}
					const bool bQuoin = !bTower && (AM < QW || AM > Fc.Len - QW) && Z0 < P.EW - 0.01 && !CornerHid(P, F, AM > 0.5 * Fc.Len, Z0, Z1);
					Col C = Scaled(bQuoin ? Varied(Mixed(P.K.Dressed, P.K.Wall, 0.25), 0.97, 0.0) : Mean, Tone);
					C = Mixed(C, Rgb(96, 98, 78), 0.55 * Smooth(1.1, 0.0, ZM));
					if (bTower) C = Scaled(C, 1.0 - 0.1 * Smooth(1.2, 0.4, ZM));
					Cut(B, Fc, A0, A1, Z0, Z1, L0, L1, bQuoin ? 0.012 : 0.0, Fc.N, [&](double, double) { return C; });
				}
			}
		}

		/** A wall's face in cells: Step apart along it, its openings dark, its quoins and surrounds painted on (the middle
		 * level's: its stones laid by MidStones, its reveals, sills, shutters and frames); cut to the rake and the broken
		 * top, whose pieces are three times as long on the far level. */
		void WallPlain(Pen& B, const Plan& P, int F, double Step, bool bMid)
		{
			const Face Fc = FaceOf(P, F);
			const double Sc = bMid ? 1.0 : 3.0;
			auto Top = [&](double A) { return TopOf(P, Fc, F, A, Sc); };
			if (!bMid && P.Kind >= 3)
			{
				ChurchWallFar(B, P, F);
				for (const Opening& O : P.Ops)
					if (O.Face == F && O.What > OpHidden) ChurchOpeningPlain(B, P, O, Fc, false);
				return;
			}
			if (bMid) MidStones(B, P, F);
			const std::vector<double> Line = bMid ? MidLine(P, F) : std::vector<double>{};
			std::vector<double> AB{ 0.0, Fc.Len }, ZB{ bMid ? 0.35 : -P.S.Below, P.EW };
			for (double A = Step; A < Fc.Len - 0.2; A += Step) AB.push_back(A);
			if (bMid)
			{
				ZB.push_back(-P.S.Below);
				ZB.push_back(0.0);
			}
			if (Fc.bGable)
			{
				AB.push_back(0.5 * Fc.Len);
				AB.push_back(0.5 * Fc.Len - P.Vb);
				AB.push_back(0.5 * Fc.Len + P.Vb);
				ZB.push_back(P.Ridge + 0.1);
			}
			for (const Opening& O : P.Ops)
				if (O.Face == F && (bMid || O.What == OpHidden))
				{
					AB.push_back(O.A0);
					AB.push_back(O.A1);
					ZB.push_back(O.Z0);
					ZB.push_back(O.Z1);
				}
			// (the far level's quoins, painted up its corners)
			const double QW = 0.42;
			if (!bMid)
			{
				AB.push_back(QW);
				AB.push_back(Fc.Len - QW);
			}
			const bool bKept = KeptGable(P, Fc);
			if (P.Hurt >= 2)
				for (double E : PieceEdges(P, F, Fc.Len, Sc))
					if (EdgeBreaks(P, Fc, F, E, Sc) && !OnMidLine(Line, E)) AB.push_back(E);
			for (size_t I = 0; I < Line.size(); ++I)
				if (!std::isnan(Line[I])) AB.push_back(std::min(Fc.Len, I * MidStep));
			std::sort(AB.begin(), AB.end());
			AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), AB.end());
			std::sort(ZB.begin(), ZB.end());
			ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.03; }), ZB.end());
			const bool bInner = P.Hurt >= 2;
			const Col Core = Mixed(P.K.Mortar, Rgb(118, 108, 94), 0.5);
			for (size_t I = 0; I + 1 < AB.size(); ++I)
			{
				const double A0 = AB[I], A1 = AB[I + 1], AM = 0.5 * (A0 + A1);
				const double TM = Top(AM);
				// (a barn's or a byre's middle: its broken top the near's line simplified, sloping from point to point)
				const bool bLine = OnMidLine(Line, AM);
				const bool bSlope = bLine || Sloped(P, Fc, F, AM, Sc);
				const bool bCut = !bSlope && Damaged(P, Fc, F, AM, Sc);
				const bool bBrokeSlope = bLine || (bSlope && (bKept || Damaged(P, Fc, F, AM, Sc)));
				double L0 = 1e9, L1 = 1e9;
				if (bCut) L0 = L1 = TM;
				else if (bLine)
				{
					L0 = MidTop(P, Fc, F, Line, A0 + 1e-4);
					L1 = MidTop(P, Fc, F, Line, A1 - 1e-4);
				}
				else if (bSlope)
				{
					L0 = Top(A0 + 1e-4);
					L1 = Top(A1 - 1e-4);
				}
				// (the column's rows, run together where nothing needs their edge: the floor, the plinth's top, the eaves on
				// the far level, a hole's sill and head where the hole is in this column)
				std::vector<double> Rows{ ZB.front() };
				for (size_t J = 1; J + 1 < ZB.size(); ++J)
				{
					const double Z = ZB[J];
					bool bKeep = bMid ? (std::fabs(Z - 0.35) < 1e-6 || std::fabs(Z) < 1e-6) : std::fabs(Z - P.EW) < 1e-6;
					for (const Opening& O : P.Ops)
						if ((bMid || O.What == OpHidden) && O.Face == F && AM > O.A0 && AM < O.A1 && (std::fabs(Z - O.Z0) < 1e-6 || std::fabs(Z - O.Z1) < 1e-6)) bKeep = true;
					if (bKeep) Rows.push_back(Z);
				}
				Rows.push_back(ZB.back());
				for (size_t J = 0; J + 1 < Rows.size(); ++J)
				{
					const double Z0 = Rows[J], Z1 = Rows[J + 1], ZM = 0.5 * (Z0 + Z1);
					bool bHole = false;
					for (const Opening& O : P.Ops)
						if ((bMid || O.What == OpHidden) && O.Face == F && AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
					if (bHole) continue;
					if (std::max(L0, L1) <= Z0 + 1e-6) continue;
					// (the stone and its mortar as the eye mixes them; the far level's quoins paler)
					const bool bDressed = !bMid && (AM < QW || AM > Fc.Len - QW) && Z0 < P.EW - 0.01 && !CornerHid(P, F, AM > 0.5 * Fc.Len, Z0, Z1);
					const bool bPlinth = bMid && ZM < 0.35;
					auto WC = [&](double A, double Z) {
						Col C = Varied(P.K.Wall, 0.97 + 0.06 * Noise(A / 0.9, Z / 0.8, P.Key ^ 0x3A1), 0.0);
						if (P.Kind == 5) C = Varied(P.K.Dressed, 0.97 + 0.05 * Noise(A / 0.9, Z / 0.8, P.Key ^ 0x3A1), 0.0); // (a tower's ashlar)
						if (bDressed) C = Varied(Mixed(P.K.Dressed, P.K.Wall, 0.25), 0.97, 0.0);
						if (bPlinth) C = Varied(Rgb(140, 132, 116), 0.95, 0.0);
						return Weathered(P, Fc, F, A, Z, C);
					};
					if (!bMid || bPlinth) Cut(B, Fc, A0, A1, Z0, Z1, L0, L1, bDressed ? 0.012 : 0.0, Fc.N, WC);
					if (bInner && Z0 >= -0.01)
					{
						// (a barn's or a byre's bare rubble, its stones and mortar sooted as the near level's InnerRubble averages: the
						// art director's barn round 2, and its carry, not a near-black slab)
						const bool bBare = P.Kind == 1 || P.Kind == 2;
						const Col PC = P.Hurt == 3 ? (bBare ? Mixed(Mixed(P.K.StoneMean, P.K.Mortar, 0.3), P.K.Soot, 0.71) : Mixed(P.K.Plaster, P.K.Soot, 0.6)) : Mixed(P.K.Plaster, P.K.Soot, 0.2);
						Cut(B, Fc, A0, A1, Z0, Z1, L0, L1, -P.Thick, Fc.N * -1.0, [&](double, double) { return PC; });
					}
					if (bCut && TM < Z1 - 1e-6 && TM > Z0 + 1e-6)
						B.Quad1(At(Fc, A0, TM, 0.0), At(Fc, A1, TM, 0.0), At(Fc, A1, TM, -P.Thick), At(Fc, A0, TM, -P.Thick), P.Hurt == 3 ? Mixed(Core, P.K.Soot, 0.3) : Core, Up);
					if (bBrokeSlope) SlopeCap(B, Fc, A0, A1, L0, L1, Z0, Z1, P.Thick, P.Hurt == 3 ? Mixed(Core, P.K.Soot, 0.6) : Core);
				}
				// (the step between this cell's top and the next one's; none along a slope, nor along the middle's line)
				if (I + 2 < AB.size() && !(bSlope && (OnMidLine(Line, A1 + 0.005) || Sloped(P, Fc, F, A1 + 0.005, Sc))))
				{
					const double TL = Top(A1 - 0.005), TN = Top(A1 + 0.005);
					if (std::fabs(TN - TL) > 1e-6)
						B.Quad1(At(Fc, A1, std::min(TN, TL), 0.0), At(Fc, A1, std::max(TN, TL), 0.0), At(Fc, A1, std::max(TN, TL), -P.Thick), At(Fc, A1, std::min(TN, TL), -P.Thick), Rgb(110, 100, 88), Fc.T * (TN < TL ? 1.0 : -1.0));
				}
			}
			// The openings: a middle level's reveals, glass and frame, shutters as flat leaves, the door; a far level's dark.
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				const double Z1 = std::min(O.Z1, std::min(Top(O.A0 - 0.1), Top(O.A1 + 0.1)));
				const double W = O.A1 - O.A0;
				const bool bThrough = bInner && (!O.bGlass || O.What == 1);
				if (O.What >= OpHidden)
				{
					if (O.What != OpHidden) ChurchOpeningPlain(B, P, O, Fc, bMid);
					continue;
				}
				if (O.What >= OpCart)
				{
					BarnOpeningPlain(B, P, O, Fc, Z1, bMid, bInner);
					continue;
				}
				if (!bMid)
				{
					// (far off: painted on the wall, the openings dark, the shutters beside them)
					Col C = O.What == 1 ? (O.bLeaf ? Scaled(P.K.Door, 0.85) : Rgb(16, 15, 14)) : O.Shutters == 1 ? P.K.Shutter : Mixed(P.K.Glass, P.K.Sky, 0.38);
					if (!O.bGlass && O.What != 1) C = Rgb(16, 15, 14);
					B.Quad1(At(Fc, O.A0, O.Z0, 0.012), At(Fc, O.A1, O.Z0, 0.012), At(Fc, O.A1, Z1, 0.012), At(Fc, O.A0, Z1, 0.012), Scaled(C, 0.9), Fc.N);
					if (O.Shutters == 0 || O.Shutters == 2)
						for (int S = -1; S <= 1; S += 2)
						{
							const double X0 = S < 0 ? O.A0 - 0.5 * W - 0.01 : O.A1 + 0.01, X1 = X0 + 0.5 * W;
							if (S > 0 && O.Shutters == 2) continue;
							B.Quad1(At(Fc, X0, O.Z0, 0.03), At(Fc, X1, O.Z0, 0.03), At(Fc, X1, Z1, 0.03), At(Fc, X0, Z1, 0.03), Scaled(P.K.Shutter, 0.95), Fc.N);
						}
					continue;
				}
				const double Dp = bThrough ? P.Thick : O.What == 1 ? 0.12 : 0.23;
				const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
				B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, Z1, -Dp), At(Fc, O.A0, Z1, 0.0), Scaled(Rev, 0.95), Fc.T);
				B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A1, Z1, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
				if (Z1 >= O.Z1 - 1e-6) B.Quad1(At(Fc, O.A0, Z1, 0.0), At(Fc, O.A1, Z1, 0.0), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Scaled(Rev, 0.7), Up * -1.0);
				B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A0, O.Z0, -Dp), Rev, Up);
				if (O.What == 1)
				{
					if (O.bLeaf && O.Swing < 0.2) B.Quad1(At(Fc, O.A0, 0.0, -Dp), At(Fc, O.A1, 0.0, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A0, Z1 - 0.42, -Dp), P.K.Door, Fc.N);
					else if (!bThrough) B.Quad1(At(Fc, O.A0, 0.0, -Dp), At(Fc, O.A1, 0.0, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A0, Z1 - 0.42, -Dp), Rgb(16, 15, 14), Fc.N);
					if (!O.bCharred) B.Quad1(At(Fc, O.A0, Z1 - 0.42, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.42), Fc.N);
					B.Box(At(Fc, 0.5 * (O.A0 + O.A1), -0.6, 0.27), Fc.T * (0.5 * W + 0.16), Fc.N * 0.22, Up * 0.44, Varied(P.K.Dressed, 0.86, 0.0), 0);
					continue;
				}
				if (O.bGlass)
				{
					B.Quad(At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.36), Mixed(P.K.Glass, P.K.Sky, 0.36), Mixed(P.K.Glass, P.K.Sky, 0.56), Mixed(P.K.Glass, P.K.Sky, 0.56), Fc.N);
					B.Quad1(At(Fc, 0.5 * (O.A0 + O.A1) - 0.04, O.Z0, -Dp + 0.01), At(Fc, 0.5 * (O.A0 + O.A1) + 0.04, O.Z0, -Dp + 0.01), At(Fc, 0.5 * (O.A0 + O.A1) + 0.04, Z1, -Dp + 0.01), At(Fc, 0.5 * (O.A0 + O.A1) - 0.04, Z1, -Dp + 0.01), P.K.Frame, Fc.N);
				}
				else if (!bThrough)
					B.Quad1(At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Rgb(16, 15, 14), Fc.N);
				// sill
				B.Box(At(Fc, 0.5 * (O.A0 + O.A1), O.Z0 - 0.045, 0.02), Fc.T * (0.5 * W + 0.08), Fc.N * 0.05, Up * 0.045, Varied(P.K.Dressed, 1.0, 0.0), 0);
				if (O.Shutters == 4 || O.Shutters == 3) continue;
				for (int S = -1; S <= 1; S += 2)
				{
					const bool bShut = O.Shutters == 1;
					if (bShut)
					{
						const double X0 = S < 0 ? O.A0 : 0.5 * (O.A0 + O.A1), X1 = X0 + 0.5 * W;
						B.Quad1(At(Fc, X0, O.Z0, -0.06), At(Fc, X1, O.Z0, -0.06), At(Fc, X1, Z1, -0.06), At(Fc, X0, Z1, -0.06), Varied(P.K.Shutter, S < 0 ? 0.97 : 1.03, 0.0), Fc.N);
					}
					else if (!(O.Shutters == 2 && S > 0))
					{
						const double X0 = S < 0 ? O.A0 - 0.5 * W - 0.01 : O.A1 + 0.01, X1 = X0 + 0.5 * W;
						B.Box(At(Fc, 0.5 * (X0 + X1), 0.5 * (O.Z0 + Z1), 0.05), Fc.T * (0.25 * W), Up * (0.5 * (Z1 - O.Z0)), Fc.N * 0.016, Varied(P.K.Shutter, 0.95, 0.0), 0);
					}
					else
					{
						const V3 Hg = At(Fc, O.A1, O.Z0, 0.0);
						const V3 Ac = Norm(Fc.T * -std::cos(1.75) + Fc.N * std::sin(1.75));
						B.Box(Hg + Ac * (0.25 * W) + Up * (0.5 * (Z1 - O.Z0)), Ac * (0.25 * W), Up * (0.5 * (Z1 - O.Z0)), Norm(Cross(Up, Ac)) * 0.016, Varied(P.K.Shutter, 0.95, 0.0), 0);
					}
				}
			}
			// The plinth (middle): a band proud of the wall.
			if (bMid)
			{
				for (double A = -0.05; A < Fc.Len + 0.05 - 1e-6; A += 2.0)
				{
					const double A1 = std::min(Fc.Len + 0.05, A + 2.0);
					if (HiddenAt(P, F, 0.5 * (A + A1), -0.3, 0.3)) continue; // (a church's: not behind another part's wall)
					B.Quad1(At(Fc, A, -P.S.Below, 0.05), At(Fc, A1, -P.S.Below, 0.05), At(Fc, A1, 0.33, 0.05), At(Fc, A, 0.33, 0.05), Weathered(P, Fc, F, A, 0.1, Rgb(136, 128, 112)), Fc.N);
				}
			}
		}

		/** A chimney's stack on a gable, plain: a box, its cap, the middle level's pots (a shell's, sooted, one pot gone). */
		void StackPlain(Pen& B, const Plan& P, int End, bool bMid)
		{
			const bool bShell = P.Hurt == 3;
			const double XI = End * (0.5 * P.L - 0.62), XO = End * (0.5 * P.L + P.GO);
			const double XM = 0.5 * (XI + XO), HX = 0.5 * std::fabs(XO - XI), HY = 0.48;
			const double ZBot = DeckZ(P, HY) - 0.3, ZTop = P.Ridge + 1.25;
			const Col Flue = Rgb(30, 27, 25);
			const Col Stone = bShell ? Mixed(Varied(P.K.Dressed, 0.9, 0.0), P.K.Soot, 0.4) : Varied(P.K.Dressed, 0.9, 0.0);
			B.Box(V3{ XM, 0.0, 0.5 * (ZBot + ZTop) }, V3{ HX, 0.0, 0.0 }, V3{ 0.0, HY, 0.0 }, V3{ 0.0, 0.0, 0.5 * (ZTop - ZBot) }, Stone, 16, &Flue);
			B.Box(V3{ XM, 0.0, ZTop + 0.06 }, V3{ HX + 0.07, 0.0, 0.0 }, V3{ 0.0, HY + 0.07, 0.0 }, V3{ 0.0, 0.0, 0.06 }, Mixed(P.K.Dressed, P.K.Soot, 0.45), 0, &Flue);
			if (bMid)
				for (int S = -1; S <= (bShell ? -1 : 1); S += 2)
					B.Box(V3{ XM, S * 0.2, ZTop + 0.36 }, V3{ 0.1, 0.0, 0.0 }, V3{ 0.0, 0.1, 0.0 }, V3{ 0.0, 0.0, 0.24 }, Mixed(P.K.Pot, P.K.Soot, 0.25), 16, &Flue);
		}

		/** The roof in bands of courses (Rows of them to a band), cut where its hole and its broken end are. */
		void RoofPlain(Pen& B, const Plan& P, int Rows, double CellX, bool bMid)
		{
			if (P.Hurt == 3)
			{
				if (ChimneyAt(P, P.Keep)) StackPlain(B, P, P.Keep, bMid);
				return;
			}
			const double X0 = -0.5 * P.L - P.GO, X1 = 0.5 * P.L + P.GO;
			const double Band = P.Gauge * Rows;
			// (the bands' edges: every Rows courses, and the eaves' kick)
			std::vector<double> SB{ P.Lk, P.Ls };
			for (double S = 0.0; S < P.Ls - 0.05; S += Band) SB.push_back(S);
			std::sort(SB.begin(), SB.end());
			SB.erase(std::unique(SB.begin(), SB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.05; }), SB.end());
			const int NB = static_cast<int>(SB.size()) - 1;
			const int NX = std::max(1, static_cast<int>(std::lround((X1 - X0) / CellX)));
			// A church's holed roof (the art director's church round 2: it read as a skylight): round the hole the deck in
			// slates' cells (the middle's a slate course deep), gone where the hole is, its edge ragged; under it a run of
			// slates gone down each column of them, as far as the blast tore them, so they step down the slope; the cut
			// edges dark, the slates near it sooted; a charred rafter or two across it.
			const bool bRagged = P.Kind >= 3 && P.Hurt == 1;
			auto Gone = [&](int Side, double X, double Sd) {
				if (Side != P.HoleSide) return false;
				const double DX = (X - P.HoleX) / P.HoleRX;
				double DS = (Sd - P.HoleS) / P.HoleRS;
				if (DS < 0.0) DS *= 1.0 - 0.42 * Unit(Hash(P.Key ^ 0x5D0, static_cast<uint64_t>(std::floor(X / 0.42) + 1000.0)));
				return std::sqrt(DX * DX + DS * DS) + 0.22 * Noise(X / 0.45, Sd / 0.45, P.Key ^ 0x401E) < 0.9;
			};
			auto Ragged = [&](int Side, int K, int I, double XA, double XB, double S0, double S1) {
				const int NU = std::max(1, static_cast<int>(std::ceil((XB - XA) / (bMid ? 0.42 : 1.0) - 1e-6)));
				const int NS = std::max(1, static_cast<int>(std::lround((S1 - S0) / (bMid ? P.Gauge : 0.75))));
				const double DU = (XB - XA) / NU, DSd = (S1 - S0) / NS;
				for (int V = 0; V < NS; ++V)
					for (int U = 0; U < NU; ++U)
					{
						const double XA2 = XA + DU * U, XB2 = XA2 + DU, SA = S0 + DSd * V, SB2 = SA + DSd, XM = 0.5 * (XA2 + XB2), SM = 0.5 * (SA + SB2);
						if (Gone(Side, XM, SM)) continue;
						// (its slates as the whole cell's, the band's lip and shading run on across it; sooted toward the hole)
						const uint64_t H = Hash(P.Key, 0xBA4D, static_cast<uint64_t>(K * 256 + I * 2 + (Side > 0)));
						Col C = Varied(P.K.Roof, (bMid ? 0.95 : 0.7) + 0.08 * Unit(H), 0.0);
						if (!bMid && P.S.Roof == 0) C = Rgb(C.R, C.G * 1.01, C.B * 1.085);
						C = Mixed(C, Rgb(138, 140, 118), 0.2 * Smooth(2.2, 0.0, S0));
						const double H0 = bMid ? 1.9 * P.SlateT : 0.02, Dim = bMid ? 0.82 : 0.95;
						auto Lift = [&](double Sd) { return H0 + (0.02 - H0) * (Sd - S0) / (S1 - S0); };
						auto Paint = [&](double X, double Sd) { return Mixed(Scaled(C, 1.0 + (Dim - 1.0) * (Sd - S0) / (S1 - S0)), P.K.Soot, 0.7 * Smooth(1.8, 1.0, HoleDist(P, Side, X, Sd))); };
						const RoofPt A0 = RoofAt(P, Side, XA2, SA, Lift(SA)), A1 = RoofAt(P, Side, XB2, SA, Lift(SA)), A2 = RoofAt(P, Side, XB2, SB2, Lift(SB2)), A3 = RoofAt(P, Side, XA2, SB2, Lift(SB2));
						B.Quad(A0.P, A1.P, A2.P, A3.P, Paint(XA2, SA), Paint(XB2, SA), Paint(XB2, SB2), Paint(XA2, SB2), A0.N);
						const RoofPt U0 = RoofAt(P, Side, XA2, SA, -0.04), U1 = RoofAt(P, Side, XB2, SA, -0.04), U2 = RoofAt(P, Side, XB2, SB2, -0.04), U3 = RoofAt(P, Side, XA2, SB2, -0.04);
						const bool bL = Gone(Side, XM - DU, SM), bR = Gone(Side, XM + DU, SM), bD = Gone(Side, XM, SM - DSd), bU = Gone(Side, XM, SM + DSd);
						const Col Edge = Rgb(40, 32, 28);
						if (bMid && (K == 0 || bL || bR || bD || bU)) B.Quad1(U0.P, U1.P, U2.P, U3.P, P.K.Boards, U0.N * -1.0);
						// (the cut edges' thickness the middle's only: far off a few centimetres are nothing)
						if ((bMid && V == 0) || (bMid && bD)) B.Quad1(U0.P, U1.P, A1.P, A0.P, bD ? Edge : Scaled(C, 0.5), Norm(A0.P - A3.P));
						if (bMid && bU) B.Quad1(U3.P, U2.P, A2.P, A3.P, Edge, Norm(A3.P - A0.P));
						if ((I == 0 && U == 0) || (bMid && bL)) B.Quad1(A0.P, A3.P, U3.P, U0.P, bL ? Edge : Scaled(C, 0.6), V3{ -1.0, 0.0, 0.0 });
						if ((I == NX - 1 && U == NU - 1) || (bMid && bR)) B.Quad1(A1.P, A2.P, U2.P, U1.P, bR ? Edge : Scaled(C, 0.6), V3{ 1.0, 0.0, 0.0 });
					}
			};
			for (int Side = -1; Side <= 1; Side += 2)
				for (int K = 0; K < NB; ++K)
				{
					const double S0 = SB[static_cast<size_t>(K)], S1 = SB[static_cast<size_t>(K) + 1], SM = 0.5 * (S0 + S1);
					for (int I = 0; I < NX; ++I)
					{
						const double XA = X0 + (X1 - X0) * I / NX, XB = X0 + (X1 - X0) * (I + 1) / NX, XM = 0.5 * (XA + XB);
						if (bRagged && Side == P.HoleSide && std::fabs(XM - P.HoleX) < 1.4 * P.HoleRX + 0.5 * (XB - XA) && SM - 0.5 * (S1 - S0) < P.HoleS + 1.4 * P.HoleRS && SM + 0.5 * (S1 - S0) > P.HoleS - 2.6 * P.HoleRS)
						{
							Ragged(Side, K, I, XA, XB, S0, S1);
							continue;
						}
						if (!DeckThere(P, Side, XM, SM)) continue;
						const uint64_t H = Hash(P.Key, 0xBA4D, static_cast<uint64_t>(K * 256 + I * 2 + (Side > 0)));
						Col C = Varied(P.K.Roof, (bMid ? 0.95 : P.Kind >= 3 ? 0.7 : 0.8) + 0.08 * Unit(H), 0.0); // (a church's steep slates darker far off, as near)
						if (!bMid && P.S.Roof == 0) C = Rgb(C.R, C.G * 1.01, C.B * 1.085); // (the far slate keeps its blue: the art director's round 3)
						C = Mixed(C, P.S.Roof == 1 ? Rgb(96, 92, 62) : Rgb(138, 140, 118), 0.2 * Smooth(2.2, 0.0, S0));
						if (P.Hurt == 1) C = Mixed(C, P.K.Soot, 0.7 * Smooth(1.8, 1.0, HoleDist(P, Side, XM, SM)));
						if (P.Hurt == 2) C = Mixed(C, P.K.Soot, 0.6 * Smooth(CutAt(P, Side, SM) - 1.4, CutAt(P, Side, SM), P.Broken * XM));
						const double H0 = bMid ? 1.9 * P.SlateT : 0.02;
						const RoofPt A0 = RoofAt(P, Side, XA, S0, H0), A1 = RoofAt(P, Side, XB, S0, H0), A2 = RoofAt(P, Side, XB, S1, 0.02), A3 = RoofAt(P, Side, XA, S1, 0.02);
						B.Quad(A0.P, A1.P, A2.P, A3.P, C, C, Scaled(C, bMid ? 0.82 : 0.95), Scaled(C, bMid ? 0.82 : 0.95), A0.N);
						const RoofPt U0 = RoofAt(P, Side, XA, S0, -0.04), U1 = RoofAt(P, Side, XB, S0, -0.04), U2 = RoofAt(P, Side, XB, S1, -0.04), U3 = RoofAt(P, Side, XA, S1, -0.04);
						if (K == 0 || P.Hurt >= 1) B.Quad1(U0.P, U1.P, U2.P, U3.P, P.K.Boards, U0.N * -1.0);
						if (bMid || K == 0) B.Quad1(RoofAt(P, Side, XA, S0, -0.04).P, RoofAt(P, Side, XB, S0, -0.04).P, A1.P, A0.P, Scaled(C, 0.5), Norm(A0.P - A3.P));
						if (I == 0) B.Quad1(A0.P, A3.P, U3.P, U0.P, Scaled(C, 0.6), V3{ -1.0, 0.0, 0.0 });
						if (I == NX - 1) B.Quad1(A1.P, A2.P, U2.P, U1.P, Scaled(C, 0.6), V3{ 1.0, 0.0, 0.0 });
						if (I > 0 && !DeckThere(P, Side, XM - (XB - XA), SM)) B.Quad1(A0.P, A3.P, U3.P, U0.P, Rgb(40, 32, 28), V3{ -1.0, 0.0, 0.0 });
						if (I < NX - 1 && !DeckThere(P, Side, XM + (XB - XA), SM)) B.Quad1(A1.P, A2.P, U2.P, U1.P, Rgb(40, 32, 28), V3{ 1.0, 0.0, 0.0 });
					}
				}
			// (the church's hole: its charred rafters across it, the middle's two, one broken and hanging into the dark; the
			// far level's one)
			if (bRagged)
			{
				const int Sd = P.HoleSide;
				const double Sa = P.HoleS - 1.15 * P.HoleRS, Sb = P.HoleS + 1.05 * P.HoleRS;
				const double XR0 = 0.45 * std::round((P.HoleX - 0.38 * P.HoleRX) / 0.45), XR1 = XR0 + 0.9;
				const Col Burnt = Mixed(P.K.Char, Rgb(112, 102, 92), 0.06); // (charred black, 25 to 35 lit: the art director's carry)
				const RoofPt R0 = RoofAt(P, Sd, XR0, Sa, -0.11), R1 = RoofAt(P, Sd, XR0, Sb, -0.11);
				B.Beam(R0.P, R1.P, 0.08, 0.13, Burnt, R0.N);
				if (bMid)
				{
					const RoofPt T0 = RoofAt(P, Sd, XR1, Sb, -0.11), T1 = RoofAt(P, Sd, XR1, P.HoleS + 0.08, -0.11);
					B.Beam(T0.P, T1.P, 0.08, 0.13, Burnt, T0.N);
					const V3 Hang = T1.P - T0.N * 0.05, Down = RoofAt(P, Sd, XR1 + 0.12, P.HoleS - 0.55 * P.HoleRS, -0.85).P;
					B.Beam(Hang, Down, 0.08, 0.12, Scaled(Burnt, 0.85), T0.N);
				}
			}
			// the ridge, a rounded cap
			const double RX0 = ChimneyAt(P, -1) ? -0.5 * P.L + 0.6 : X0, RX1 = ChimneyAt(P, 1) ? 0.5 * P.L - 0.6 : X1;
			double CX0 = RX0, CX1 = RX1;
			if (P.Hurt == 2)
			{
				const double Cut = std::min(CutAt(P, 1, P.Ls), CutAt(P, -1, P.Ls)) - 0.2;
				if (P.Broken > 0) CX1 = std::min(CX1, Cut);
				else CX0 = std::max(CX0, -Cut);
			}
			if (CX1 > CX0)
				for (int S = -1; S <= 1; S += 2)
				{
					const V3 T0{ CX0, 0.0, P.Ridge + 0.1 }, T1{ CX1, 0.0, P.Ridge + 0.1 };
					const V3 E0{ CX0, S * 0.15, P.Ridge - 0.02 }, E1{ CX1, S * 0.15, P.Ridge - 0.02 };
					B.Quad1(T0, T1, E1, E0, Scaled(P.K.Ridge, 0.95), V3{ 0.0, S * 0.5, 1.0 });
				}
			// chimneys
			for (int End = -1; End <= 1; End += 2)
				if (ChimneyAt(P, End)) StackPlain(B, P, End, bMid);
			if (bMid && P.Kind == 0)
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const double GV = 0.5 * P.D + P.Over + 0.03, GZ = P.Ze - 0.09;
					double GX0 = X0 + 0.06, GX1 = X1 - 0.06;
					if (P.Hurt == 2)
					{
						if (P.Broken > 0) GX1 = std::min(GX1, CutAt(P, Side, 0.2) - 0.3);
						else GX0 = std::max(GX0, -(CutAt(P, Side, 0.2) - 0.3));
					}
					B.Box(V3{ 0.5 * (GX0 + GX1), Side * GV, GZ }, V3{ 0.5 * (GX1 - GX0), 0.0, 0.0 }, V3{ 0.0, 0.06, 0.0 }, V3{ 0.0, 0.0, 0.05 }, P.K.Zinc, 0);
				}
		}

		void InsidePlain(Pen& B, const Plan& P, bool bMid)
		{
			const double IX = 0.5 * P.L - P.Thick, IY = 0.5 * P.D - P.Thick;
			if (P.Hurt == 3 && P.Kind != 0)
			{
				// (a barn's or a byre's burnt floor: its fallen rubble and ash in patches, flat, as the near level's heaps
				// read from 90 m)
				const double CS = bMid ? 1.2 : 3.0;
				auto FC = [&](double X, double Y) { return Mixed(Mixed(P.K.StoneMean, Rgb(150, 136, 118), 0.5), Rgb(70, 62, 54), 0.15 + 0.35 * Noise01(X / 1.1, Y / 1.1, P.Key ^ 0xF5)); };
				for (double X = -IX; X < IX - 1e-6; X += CS)
					for (double Y = -IY; Y < IY - 1e-6; Y += CS)
					{
						const double X1 = std::min(IX, X + CS), Y1 = std::min(IY, Y + CS);
						B.Quad(V3{ X, Y, 0.02 }, V3{ X1, Y, 0.02 }, V3{ X1, Y1, 0.02 }, V3{ X, Y1, 0.02 }, FC(X, Y), FC(X1, Y), FC(X1, Y1), FC(X, Y1), Up);
					}
			}
			else if (P.Hurt >= 2) B.Quad1(V3{ -IX, -IY, 0.02 }, V3{ IX, -IY, 0.02 }, V3{ IX, IY, 0.02 }, V3{ -IX, IY, 0.02 }, P.Hurt == 3 ? Rgb(60, 54, 48) : Rgb(96, 88, 76), Up);
			if (P.Hurt == 1)
			{
				// (a church's: no loft, the dim of its nave under the hole, the rafters' black against it; its walls' inner
				// faces above it to the roof, so that nothing shows through the hole but the nave's dark)
				const double Z = P.Loft;
				B.Quad1(V3{ -IX, -IY, Z }, V3{ IX, -IY, Z }, V3{ IX, IY, Z }, V3{ -IX, IY, Z }, P.Kind >= 3 ? Rgb(64, 57, 50) : P.K.Boards, Up);
				if (P.Kind >= 3)
					for (int F = 0; F < 4; ++F)
					{
						const Face Fc = FaceOf(P, F);
						std::vector<double> AB{ 0.0, Fc.Len };
						if (Fc.bGable)
							for (double E : { 0.5 * Fc.Len - P.Vb, 0.5 * Fc.Len, 0.5 * Fc.Len + P.Vb }) AB.push_back(E);
						std::sort(AB.begin(), AB.end());
						// (a long wall's up past its plate to the deck's underside over its inner face, closing the eaves)
						const double Deck = RoofAt(P, 1, 0.0, SlopeAt(P, 0.5 * P.D - P.Thick), -0.04).P.Z + 0.03;
						for (size_t I = 0; I + 1 < AB.size(); ++I)
						{
							const double L0 = Fc.bGable ? TopOf(P, Fc, F, AB[I] + 1e-4) : Deck, L1 = Fc.bGable ? TopOf(P, Fc, F, AB[I + 1] - 1e-4) : Deck;
							Cut(B, Fc, AB[I], AB[I + 1], Z - 0.05, std::max(L0, L1) + 0.01, L0, L1, -P.Thick, Fc.N * -1.0, [](double, double) { return Rgb(50, 45, 40); });
						}
					}
			}
			if (P.Hurt == 2)
			{
				for (int S = P.Kind == 0 ? 1 : std::max(1, P.S.Storeys); S <= std::max(1, P.S.Storeys); ++S)
				{
					const double Z = P.Kind == 0 ? S * 2.75 + 0.05 : P.Loft - 0.05, XEnd = 0.5 * P.L - 2.6;
					const double XA = P.Broken > 0 ? -IX : -XEnd, XB = P.Broken > 0 ? XEnd : IX;
					B.Box(V3{ 0.5 * (XA + XB), 0.0, Z - 0.12 }, V3{ 0.5 * (XB - XA), 0.0, 0.0 }, V3{ 0.0, IY, 0.0 }, V3{ 0.0, 0.0, 0.12 }, P.K.Boards, 0);
				}
				Heap(B, P, V3{ P.Broken * (0.5 * P.L + 1.1), 0.0, -0.15 }, 2.3, 0.5 * P.D + 0.9, 1.5, 0.0, bMid ? 12 : 8, bMid ? 4 : 2, P.Key ^ 0x4EA9, false);
			}
			if (P.Hurt == 3)
			{
				// (a barn's or a byre's: its floor's ash and rubble dark, no heap over it, which read as a mattress from
				// 90 m: the art director's barn round 2)
				if (P.Kind == 0) Heap(B, P, V3{ 0.0, 0.0, 0.0 }, IX - 0.6, IY - 0.3, 0.75, 0.0, bMid ? 12 : 8, bMid ? 3 : 2, P.Key ^ 0x4EAB, false);
				FootHeaps(B, P, bMid ? 1 : 2);
			}
			if (P.Hurt == 1 && bMid) Heap(B, P, V3{ std::clamp(P.HoleX, -0.5 * P.L + 1.5, 0.5 * P.L - 1.5), P.HoleSide * (0.5 * P.D + 0.75), -0.25 }, 1.1, 0.5, 0.28, 0.0, 8, 2, P.Key ^ 0x51E, false);
		}

		// ------------------------------------------------------------------ the church's own: its cornices, the tower's top

		/**
		 * A cornice on its corbels along face F, A0 to A1 along it: the corbels from Z0 up to ZC, each a block tapering
		 * back to its foot, Step apart; the cornice's stones from ZC to ZT, Out proud of the wall. Level 0 near; 1 the
		 * middle (a band, the corbels' row a shadow under it); 2 far (the band).
		 */
		void Cornice(Pen& B, const Plan& P, int F, double A0, double A1, double Z0, double ZC, double ZT, double Out, double Step, int Level)
		{
			const Face Fc = FaceOf(P, F);
			const double AM = 0.5 * (A0 + A1);
			if (Level > 0)
			{
				B.Box(At(Fc, AM, 0.5 * (ZC + ZT), 0.5 * Out - 0.01), Fc.T * (0.5 * (A1 - A0)), Fc.N * (0.5 * Out + 0.01), Up * (0.5 * (ZT - ZC)), Weathered(P, Fc, F, AM, ZT, Varied(P.K.Dressed, 0.95, 0.0)), 0);
				if (Level == 1) B.Quad1(At(Fc, A0, Z0, 0.03), At(Fc, A1, Z0, 0.03), At(Fc, A1, ZC, 0.03), At(Fc, A0, ZC, 0.03), Weathered(P, Fc, F, AM, Z0, Scaled(P.K.Dressed, 0.66)), Fc.N);
				return;
			}
			Rng R(Hash(P.Key ^ 0xC0B1, static_cast<uint64_t>(F), static_cast<uint64_t>(std::llround(ZT * 100.0))));
			for (double A = A0; A < A1 - 1e-6;)
			{
				double E = A + R.Range(0.6, 1.1);
				if (A1 - E < 0.3) E = A1;
				Block(B, Fc, A + 0.004, E - 0.004, ZC, ZT, Out, Weathered(P, Fc, F, 0.5 * (A + E), ZT, Varied(P.K.Dressed, 0.9 + 0.12 * R.Next(), 0.0)), 0.0);
				A = E;
			}
			const double HW = 0.085, Lo = Z0 + 0.07;
			for (double A = A0 + 0.5 * Step; A < A1 - 0.3 * Step; A += Step)
			{
				const Col C = Weathered(P, Fc, F, A, Z0, Varied(P.K.Dressed, 0.86 + 0.14 * R.Next(), 0.0));
				const double PT = Out - 0.025, PB = 0.05;
				for (int S = -1; S <= 1; S += 2)
					B.Quad1(At(Fc, A + S * HW, ZC, 0.0), At(Fc, A + S * HW, ZC, PT), At(Fc, A + S * HW, Lo, PB), At(Fc, A + S * HW, Z0, 0.0), Scaled(C, 0.84), Fc.T * S);
				B.Quad1(At(Fc, A - HW, ZC, PT), At(Fc, A + HW, ZC, PT), At(Fc, A + HW, Lo, PB), At(Fc, A - HW, Lo, PB), C, Fc.N * 0.6 - Up * 0.4);
				B.Quad1(At(Fc, A - HW, Lo, PB), At(Fc, A + HW, Lo, PB), At(Fc, A + HW, Z0, 0.0), At(Fc, A - HW, Z0, 0.0), Scaled(C, 0.62), Fc.N * 0.3 - Up);
			}
		}

		/** A string course round the tower: a band of dressed stone, its top at Z, proud of the walls, its corners closed
		 * (near: stone by stone; the middle: a band; none far). Not on the east wall where it is inside the nave's roof. */
		void StringCourse(Pen& B, const Plan& P, double Z, int Level)
		{
			if (Level == 2) return;
			for (int F = 0; F < 4; ++F)
			{
				if (P.AbutZ[F] > Z) continue;
				const Face Fc = FaceOf(P, F);
				if (Level == 1)
				{
					B.Box(At(Fc, 0.5 * Fc.Len, Z - 0.08, 0.035), Fc.T * (0.5 * Fc.Len + 0.07), Fc.N * 0.035, Up * 0.08, Weathered(P, Fc, F, 0.5 * Fc.Len, Z, Varied(P.K.Dressed, 0.95, 0.0)), 0);
					continue;
				}
				Rng R(Hash(P.Key ^ 0x57C0, static_cast<uint64_t>(F), static_cast<uint64_t>(std::llround(Z * 100.0))));
				for (double A = -0.07; A < Fc.Len + 0.07 - 1e-6;)
				{
					double E = A + R.Range(0.55, 1.0);
					if (Fc.Len + 0.07 - E < 0.3) E = Fc.Len + 0.07;
					Block(B, Fc, A + 0.004, E - 0.004, Z - 0.16, Z, 0.07, Weathered(P, Fc, F, 0.5 * (A + E), Z, Varied(P.K.Dressed, 0.9 + 0.12 * R.Next(), 0.0)), 0.0);
					A = E;
				}
			}
		}

		/**
		 * The tower's top: its cornice on corbels round the walls' head; a parapet of dressed stone with its coping; a
		 * pinnacle on each corner; inside them the octagonal stone spire, course by course, each course's foot standing a
		 * little proud of the one under it, lichen on it and the weather's side darker; a lucarne at the foot of each of
		 * its four cardinal faces; its stone finial, the iron cross, and over it a gilt weathercock turned to the wind.
		 * (Level 0 near, 1 the middle, 2 far.)
		 */
		void TowerTop(Pen& B, const Plan& P, int Level)
		{
			const double S = 0.5 * P.L, EW = P.EW;
			const double ZW = EW + 0.2, ZP = EW + 0.95;
			const double Sa = S - 0.62, ZB = ZW, Za = P.E + 10.5, Hs = Za - ZB;
			const double T8 = std::tan(Pi / 8.0);
			const Col Dr = P.K.Dressed;
			// The cornice.
			for (int F = 0; F < 4; ++F) Cornice(B, P, F, -0.2, 2.0 * S + 0.2, EW - 0.42, EW - 0.05, ZW, 0.2, 0.52, Level);
			// The parapet: its blocks in two courses before its mortar, its inside face, its coping; the walk inside it.
			for (int F = 0; F < 4; ++F)
			{
				const Face Fc = FaceOf(P, F);
				const Col C = Weathered(P, Fc, F, S, ZP, Varied(Dr, 0.94, 0.0));
				if (Level == 0)
				{
					B.Quad1(At(Fc, 0.0, ZW, 0.0), At(Fc, Fc.Len, ZW, 0.0), At(Fc, Fc.Len, ZP, 0.0), At(Fc, 0.0, ZP, 0.0), Scaled(P.K.Mortar, 0.8), Fc.N);
					Rng R(Hash(P.Key ^ 0x9A4A, static_cast<uint64_t>(F)));
					for (int Row = 0; Row < 2; ++Row)
					{
						const double Z0 = ZW + Row * 0.5 * (ZP - ZW), Z1 = ZW + (Row + 1) * 0.5 * (ZP - ZW);
						for (double A = -0.05 - (Row ? 0.3 : 0.0); A < Fc.Len + 0.05 - 1e-6;)
						{
							double E = A + R.Range(0.55, 0.95);
							if (Fc.Len + 0.05 - E < 0.3) E = Fc.Len + 0.05;
							const double A0 = std::max(-0.05, A);
							Block(B, Fc, A0 + 0.004, E - 0.004, Z0 + 0.004, Z1 - 0.004, 0.045, Weathered(P, Fc, F, 0.5 * (A0 + E), Z0, Varied(Dr, 0.9 + 0.12 * R.Next(), 0.0)), 0.0);
							A = E;
						}
					}
				}
				else B.Quad1(At(Fc, -0.05, ZW, 0.03), At(Fc, Fc.Len + 0.05, ZW, 0.03), At(Fc, Fc.Len + 0.05, ZP, 0.03), At(Fc, -0.05, ZP, 0.03), C, Fc.N);
				if (Level < 2) B.Quad1(At(Fc, 0.3, ZW, -0.3), At(Fc, Fc.Len - 0.3, ZW, -0.3), At(Fc, Fc.Len - 0.3, ZP, -0.3), At(Fc, 0.3, ZP, -0.3), Scaled(C, 0.82), Fc.N * -1.0);
				B.Box(At(Fc, 0.5 * Fc.Len, ZP + 0.06, -0.125), Fc.T * (0.5 * Fc.Len + 0.08), Fc.N * 0.2, Up * 0.06, Varied(C, 1.04, 0.0), Level == 0 ? 0 : 16);
			}
			if (Level < 2) B.Quad1(V3{ -S + 0.3, -S + 0.3, ZW }, V3{ S - 0.3, -S + 0.3, ZW }, V3{ S - 0.3, S - 0.3, ZW }, V3{ -S + 0.3, S - 0.3, ZW }, Rgb(112, 108, 100), Up);
			// The pinnacles: a pier, its cap's moulding, a steep pyramid, a knob.
			for (int I = 0; I < 4; ++I)
			{
				const double CX = (I & 1 ? 1.0 : -1.0) * (S - 0.28), CY = (I & 2 ? 1.0 : -1.0) * (S - 0.28);
				const double Top = ZP + 0.45, Apex = Top + 1.35;
				const Col C = Varied(Dr, 0.92 + 0.08 * Unit(Hash(P.Key, 0x914, static_cast<uint64_t>(I))), 0.0);
				B.Box(V3{ CX, CY, 0.5 * (ZW + Top) }, V3{ 0.3, 0.0, 0.0 }, V3{ 0.0, 0.3, 0.0 }, V3{ 0.0, 0.0, 0.5 * (Top - ZW) }, C, 16);
				if (Level < 2) B.Box(V3{ CX, CY, Top + 0.05 }, V3{ 0.36, 0.0, 0.0 }, V3{ 0.0, 0.36, 0.0 }, V3{ 0.0, 0.0, 0.05 }, Varied(C, 1.05, 0.0), 16);
				const V3 Ap{ CX, CY, Apex };
				const V3 Q[4] = { { CX - 0.33, CY - 0.33, Top + 0.1 }, { CX + 0.33, CY - 0.33, Top + 0.1 }, { CX + 0.33, CY + 0.33, Top + 0.1 }, { CX - 0.33, CY + 0.33, Top + 0.1 } };
				for (int K = 0; K < 4; ++K)
				{
					const V3 M = (Q[K] + Q[(K + 1) % 4]) * 0.5;
					B.Tri1(Q[K], Q[(K + 1) % 4], Ap, Scaled(C, K == 1 || K == 2 ? 1.0 : 0.9), Norm(V3{ M.X - CX, M.Y - CY, 0.25 }));
				}
				if (Level == 0) B.Tube(Ap - Up * 0.03, Ap + Up * 0.13, 0.06, 0.03, 6, C, true);
			}
			// The spire.
			auto Rad = [&](double Z) { return Sa * (1.0 - Z / Hs); };
			auto At8 = [&](int K, double Sx, double Z, double Lip) {
				const double Ph = K * Pi / 4.0, Rr = Rad(Z) + Lip;
				return V3{ std::cos(Ph), std::sin(Ph), 0.0 } * Rr + V3{ -std::sin(Ph), std::cos(Ph), 0.0 } * (Sx * Rr * T8) + V3{ 0.0, 0.0, ZB + Z };
			};
			auto Out8 = [&](int K) { return Norm(V3{ std::cos(K * Pi / 4.0), std::sin(K * Pi / 4.0), Sa / Hs }); };
			auto Stone = [&](int K, double Z, uint64_t H) {
				Col C = Varied(Dr, 0.86 + 0.16 * Unit(H), (Unit(Mix(H)) - 0.5) * 0.04);
				const double Ph = K * Pi / 4.0;
				C = Mixed(C, Rgb(150, 152, 126), 0.4 * Smooth(0.55, 0.85, Noise01(Ph * 1.7 + Z / 0.9, Z / 0.6, P.Key ^ 0x5B1)));
				if (std::cos(Ph) < -0.1) C = Scaled(C, 0.93); // (the weather's side, west, darker)
				return Scaled(C, 1.0 - 0.07 * Z / Hs);
			};
			const double ZTop = Hs - 0.6;
			if (Level == 0)
			{
				const int NCo = std::max(6, static_cast<int>(std::lround(ZTop / 0.34)));
				const double Lip = 0.03;
				for (int Cc = 0; Cc < NCo; ++Cc)
				{
					const double Z0 = ZTop * Cc / NCo, Z1 = ZTop * (Cc + 1) / NCo;
					for (int K = 0; K < 8; ++K)
					{
						const V3 Ot = Out8(K);
						B.Quad1(At8(K, -1.0, Z0, -0.004), At8(K, 1.0, Z0, -0.004), At8(K, 1.0, Z1, -0.004), At8(K, -1.0, Z1, -0.004), Scaled(P.K.Mortar, 0.75), Ot);
						const int N = std::max(1, static_cast<int>(std::lround(2.0 * Rad(Z0) * T8 / 0.72)));
						std::vector<double> Edges{ -1.0 };
						const bool bOdd = Cc % 2 == 1 && N >= 2;
						for (int J = 1; J < N + (bOdd ? 1 : 0); ++J) Edges.push_back(-1.0 + 2.0 * (J - (bOdd ? 0.5 : 0.0)) / N);
						Edges.push_back(1.0);
						const double G = 0.007 / std::max(0.05, Rad(Z1) * T8);
						for (size_t J = 0; J + 1 < Edges.size(); ++J)
						{
							const double S0 = Edges[J] + (J == 0 ? 0.5 * G : G), S1 = Edges[J + 1] - (J + 2 == Edges.size() ? 0.5 * G : G);
							if (S1 - S0 < 0.02) continue;
							const uint64_t H = Hash(P.Key ^ 0x5B1E, static_cast<uint64_t>(Cc * 8 + K), J);
							const Col C = Stone(K, Z0, H);
							B.Quad(At8(K, S0, Z0 + 0.006, Lip), At8(K, S1, Z0 + 0.006, Lip), At8(K, S1, Z1 - 0.004, 0.0), At8(K, S0, Z1 - 0.004, 0.0), Scaled(C, 1.03), Scaled(C, 1.03), Scaled(C, 0.95), Scaled(C, 0.95), Ot);
							B.Quad1(At8(K, S0, Z0 + 0.006, 0.0), At8(K, S1, Z0 + 0.006, 0.0), At8(K, S1, Z0 + 0.006, Lip), At8(K, S0, Z0 + 0.006, Lip), Scaled(C, 0.55), Ot * 0.3 - Up);
						}
					}
				}
				for (int K = 0; K < 8; ++K) B.Tri1(At8(K, -1.0, ZTop, 0.0), At8(K, 1.0, ZTop, 0.0), V3{ 0.0, 0.0, Za }, Stone(K, ZTop, Hash(P.Key, 0x5B1F, static_cast<uint64_t>(K))), Out8(K));
			}
			else
			{
				// (plain: each face in bands, or one triangle far off)
				const int NB = Level == 1 ? 3 : 1;
				for (int K = 0; K < 8; ++K)
					for (int I = 0; I < NB; ++I)
					{
						const double Z0 = Hs * I / NB, Z1 = Hs * (I + 1) / NB;
						const Col C = Scaled(Stone(K, Z0, Hash(P.Key, 0x5B20, static_cast<uint64_t>(K * 4 + I))), Level == 1 ? 0.97 : 0.95);
						if (I + 1 == NB) B.Tri1(At8(K, -1.0, Z0, 0.0), At8(K, 1.0, Z0, 0.0), V3{ 0.0, 0.0, Za }, C, Out8(K));
						else B.Quad1(At8(K, -1.0, Z0, 0.0), At8(K, 1.0, Z0, 0.0), At8(K, 1.0, Z1, 0.0), At8(K, -1.0, Z1, 0.0), C, Out8(K));
					}
			}
			// The lucarnes at the foot of the four cardinal faces: a small gabled front with its round-headed opening dark
			// over two louvres, its sides back to the spire, its two stone slopes.
			if (Level < 2)
				for (int K = 0; K < 8; K += 2)
				{
					const double Ph = K * Pi / 4.0;
					const V3 Nn{ std::cos(Ph), std::sin(Ph), 0.0 }, Tt{ -std::sin(Ph), std::cos(Ph), 0.0 };
					const double W = 0.45, Z0 = 0.3, ZT = 1.45, ZA = 2.05, Dd = Sa + 0.04;
					auto Fr = [&](double U, double Z, double Ex) { return Nn * (Dd + Ex) + Tt * U + Up * (ZB + Z); };
					auto Bk = [&](double U, double Z) { return Nn * Rad(Z) + Tt * U + Up * (ZB + Z); };
					const Col C = Stone(K, 0.5, Hash(P.Key, 0x1C4, static_cast<uint64_t>(K)));
					B.Quad1(Fr(-W, Z0, 0.0), Fr(W, Z0, 0.0), Fr(W, ZT, 0.0), Fr(-W, ZT, 0.0), C, Nn);
					B.Tri1(Fr(-W, ZT, 0.0), Fr(W, ZT, 0.0), Fr(0.0, ZA, 0.0), C, Nn);
					for (int Sd = -1; Sd <= 1; Sd += 2)
					{
						B.Quad1(Fr(Sd * W, Z0, 0.0), Fr(Sd * W, ZT, 0.0), Bk(Sd * W, ZT), Bk(Sd * W, Z0), Scaled(C, 0.88), Tt * Sd);
						B.Quad1(Fr(Sd * (W + 0.07), ZT - 0.04, 0.06), Fr(0.0, ZA + 0.05, 0.06), Bk(0.0, ZA + 0.05), Bk(Sd * (W + 0.07), ZT - 0.04), Scaled(C, 0.8), Up + Tt * Sd);
					}
					// (its opening: dark, round-headed; near, two louvres in it)
					const double OW = 0.2, OZ0 = 0.6, OSP = 1.05;
					const Col Dk = Rgb(14, 13, 12);
					B.Quad1(Fr(-OW, OZ0, 0.004), Fr(OW, OZ0, 0.004), Fr(OW, OSP, 0.004), Fr(-OW, OSP, 0.004), Dk, Nn);
					for (int I = 0; I < 4; ++I)
						B.Tri1(Fr(0.0, OSP, 0.004), Fr(OW * std::cos(Pi * I / 4), OSP + OW * std::sin(Pi * I / 4), 0.004), Fr(OW * std::cos(Pi * (I + 1) / 4), OSP + OW * std::sin(Pi * (I + 1) / 4), 0.004), Dk, Nn);
					if (Level == 0)
						for (double Z : { 0.75, 0.95 })
							B.Box(Fr(0.0, Z, 0.0), Tt * (OW - 0.01), Norm(Nn - Up * 0.7) * 0.06, Norm(Up + Nn * 0.7) * 0.01, Rgb(92, 86, 78), 0);
				}
			// The finial, the iron cross and the weathercock.
			if (Level == 2) return;
			const Col Iron = Rgb(46, 44, 42);
			if (Level == 1)
			{
				B.Beam(V3{ 0.0, 0.0, Za - 0.1 }, V3{ 0.0, 0.0, Za + 2.1 }, 0.07, 0.07, Iron);
				B.Beam(V3{ 0.0, -0.36, Za + 1.65 }, V3{ 0.0, 0.36, Za + 1.65 }, 0.07, 0.07, Iron);
				return;
			}
			const Col Fin = Stone(0, Hs, Hash(P.Key, 0xF1A));
			B.Tube(V3{ 0.0, 0.0, Za - 0.2 }, V3{ 0.0, 0.0, Za + 0.18 }, 0.17, 0.09, 8, Fin, true);
			B.Tube(V3{ 0.0, 0.0, Za + 0.18 }, V3{ 0.0, 0.0, Za + 0.3 }, 0.09, 0.13, 8, Fin, false);
			B.Tube(V3{ 0.0, 0.0, Za + 0.3 }, V3{ 0.0, 0.0, Za + 0.44 }, 0.13, 0.0, 8, Fin, false);
			B.Tube(V3{ 0.0, 0.0, Za + 0.4 }, V3{ 0.0, 0.0, Za + 2.35 }, 0.022, 0.018, 5, Iron, true);
			// (the orb, a gilt copper ball on the rod: the art director's church round 2)
			const Col Gilt = Rgb(158, 126, 64);
			{
				const V3 OC{ 0.0, 0.0, Za + 0.75 };
				constexpr int Rings = 6, Segs = 10;
				constexpr double OR = 0.15;
				std::vector<uint32_t> Ix;
				for (int I = 0; I <= Rings; ++I)
					for (int J = 0; J <= Segs; ++J)
					{
						const double Th = Pi * I / Rings, Ph = 2.0 * Pi * J / Segs;
						const V3 N{ std::sin(Th) * std::cos(Ph), std::sin(Th) * std::sin(Ph), std::cos(Th) };
						Ix.push_back(B.Vert(OC + N * OR, N, Scaled(Gilt, 0.9 + 0.1 * N.Z)));
					}
				for (int I = 0; I < Rings; ++I)
					for (int J = 0; J < Segs; ++J)
					{
						const uint32_t A0 = Ix[static_cast<size_t>(I * (Segs + 1) + J)], A1 = Ix[static_cast<size_t>(I * (Segs + 1) + J + 1)];
						const uint32_t B0 = Ix[static_cast<size_t>((I + 1) * (Segs + 1) + J)], B1 = Ix[static_cast<size_t>((I + 1) * (Segs + 1) + J + 1)];
						const double Th = Pi * (I + 0.5) / Rings, Ph = 2.0 * Pi * (J + 0.5) / Segs;
						const V3 Out{ std::sin(Th) * std::cos(Ph), std::sin(Th) * std::sin(Ph), std::cos(Th) };
						if (I > 0) B.Tri(A0, A1, B1, Out);
						if (I < Rings - 1) B.Tri(A0, B1, B0, Out);
					}
			}
			B.Beam(V3{ 0.0, 0.0, Za + 1.05 }, V3{ 0.0, 0.0, Za + 2.0 }, 0.05, 0.05, Iron);
			B.Beam(V3{ 0.0, -0.34, Za + 1.65 }, V3{ 0.0, 0.34, Za + 1.65 }, 0.05, 0.05, Iron);
			for (int Sd = -1; Sd <= 1; Sd += 2)
				B.Box(V3{ 0.0, Sd * 0.36, Za + 1.65 }, V3{ 0.03, 0.0, 0.0 }, V3{ 0.0, 0.03, 0.0 }, V3{ 0.0, 0.0, 0.06 }, Iron);
			// (the cockerel, gilt and tarnished, over the rod, its tail to the wind: each piece of it a plate 4 to 8 cm thick,
			// so that it reads as a bird from its edge too; the art director's church round 2)
			{
				const double Yc = 2.0 * Pi * Unit(Mix(P.Key ^ 0xC0C));
				const V3 Fw{ std::cos(Yc), std::sin(Yc), 0.0 }, Sd{ -std::sin(Yc), std::cos(Yc), 0.0 };
				const V3 O{ 0.0, 0.0, Za + 2.32 };
				const Col Side = Scaled(Gilt, 0.82);
				// (a convex outline in the bird's plane, U forward and V up, made a plate Thick through)
				auto Plate = [&](std::initializer_list<std::pair<double, double>> Pts, double Thick) {
					std::vector<V3> Q;
					for (const auto& [U, V] : Pts) Q.push_back(O + Fw * U + Up * V);
					const size_t N = Q.size();
					V3 Mid{ 0.0, 0.0, 0.0 };
					for (const V3& X : Q) Mid = Mid + X * (1.0 / static_cast<double>(N));
					for (int Fc2 = -1; Fc2 <= 1; Fc2 += 2)
						for (size_t K = 1; K + 1 < N; ++K) B.Tri1(Q[0] + Sd * (0.5 * Thick * Fc2), Q[K] + Sd * (0.5 * Thick * Fc2), Q[K + 1] + Sd * (0.5 * Thick * Fc2), Gilt, Sd * Fc2);
					for (size_t K = 0; K < N; ++K)
					{
						const V3 E0 = Q[K], E1 = Q[(K + 1) % N];
						const V3 Out = Norm(Cross(E1 - E0, Sd));
						B.Quad1(E0 - Sd * (0.5 * Thick), E1 - Sd * (0.5 * Thick), E1 + Sd * (0.5 * Thick), E0 + Sd * (0.5 * Thick), Side, Dot(Out, (E0 + E1) * 0.5 - Mid) >= 0.0 ? Out : Out * -1.0);
					}
				};
				Plate({ { -0.17, 0.10 }, { -0.12, 0.03 }, { 0.0, 0.01 }, { 0.12, 0.04 }, { 0.17, 0.11 }, { 0.12, 0.17 }, { 0.0, 0.19 }, { -0.12, 0.17 } }, 0.08); // the body
				Plate({ { 0.07, 0.07 }, { 0.17, 0.11 }, { 0.22, 0.29 }, { 0.12, 0.31 } }, 0.07); // the breast and neck
				Plate({ { 0.14, 0.29 }, { 0.19, 0.27 }, { 0.235, 0.3 }, { 0.235, 0.35 }, { 0.195, 0.375 }, { 0.15, 0.345 } }, 0.06); // the head
				Plate({ { 0.23, 0.335 }, { 0.3, 0.318 }, { 0.23, 0.305 } }, 0.03);                                                  // the beak
				Plate({ { 0.145, 0.35 }, { 0.165, 0.415 }, { 0.19, 0.37 } }, 0.04);                                                  // the comb
				Plate({ { 0.18, 0.37 }, { 0.205, 0.43 }, { 0.225, 0.355 } }, 0.04);
				Plate({ { 0.205, 0.295 }, { 0.235, 0.295 }, { 0.22, 0.24 } }, 0.03); // the wattle
				// (the tail: a fan up and back from the rump, its two long sickle feathers arching over it and down)
				Plate({ { -0.11, 0.07 }, { -0.36, 0.11 }, { -0.43, 0.21 }, { -0.42, 0.33 }, { -0.35, 0.43 }, { -0.25, 0.46 }, { -0.13, 0.19 } }, 0.06);
				Plate({ { -0.22, 0.40 }, { -0.30, 0.52 }, { -0.36, 0.51 }, { -0.29, 0.40 } }, 0.04);
				Plate({ { -0.36, 0.51 }, { -0.30, 0.52 }, { -0.44, 0.46 }, { -0.47, 0.40 } }, 0.04);
				Plate({ { -0.40, 0.33 }, { -0.47, 0.40 }, { -0.50, 0.31 }, { -0.45, 0.24 } }, 0.04);
				Plate({ { -0.02, 0.03 }, { 0.025, 0.03 }, { 0.01, -0.06 }, { -0.01, -0.06 } }, 0.025); // the leg on the rod
			}
		}

		/** Rain streaks hanging from a church tower's ledges (near; the art director's church round 2): thin dark runs
		 * tapering down its ashlar, most where the ledge drips most (TowerDrips' runs); none over an opening, nor where
		 * the nave's roof covers the wall. */
		void TowerStreaks(Pen& B, const Plan& P)
		{
			const Col Mean = Varied(P.K.Dressed, 0.9, 0.0), Dirt = Rgb(56, 50, 40);
			const std::vector<double> Drips = TowerDrips(P);
			for (int F = 0; F < 4; ++F)
			{
				const Face Fc = FaceOf(P, F);
				for (size_t D = 0; D < Drips.size(); ++D)
				{
					const double ZD = Drips[D];
					if (ZD < P.AbutZ[F] + 0.3) continue;
					Rng R(Hash(P.Key ^ 0x57EA, static_cast<uint64_t>(F), D));
					for (double A = 0.55 + R.Range(0.0, 0.3); A < Fc.Len - 0.55; A += R.Range(0.16, 0.45))
					{
						const double Runs = Smooth(0.4, 0.78, Noise01(A / 0.42 + 11.0 * F, ZD * 1.7, P.Key ^ 0x5E7));
						if (R.Next() > 0.15 + 0.75 * Runs) continue;
						const double W = R.Range(0.03, 0.1), Len = R.Range(0.35, 0.9) + 1.5 * Runs * R.Next();
						double Lo = ZD - Len;
						bool bSkip = false;
						for (const Opening& O : P.Ops)
							if (O.Face == F && A + W > O.A0 - 0.35 && A - W < O.A1 + 0.35 && O.Z0 < ZD && O.Z1 + 0.6 > Lo)
							{
								if (O.Z1 + 0.6 >= ZD - 0.25) bSkip = true; // (an opening right under the ledge)
								Lo = std::max(Lo, O.Z1 + 0.6);
							}
						if (bSkip || ZD - Lo < 0.25) continue;
						const Col Head = Mixed(Mean, Dirt, 0.42 + 0.25 * Runs), Foot = Mixed(Mean, Dirt, 0.1);
						B.Quad(At(Fc, A - 0.5 * W, ZD, 0.0135), At(Fc, A + 0.5 * W, ZD, 0.0135), At(Fc, A + 0.12 * W, Lo, 0.0135), At(Fc, A - 0.12 * W, Lo, 0.0135), Head, Head, Foot, Foot, Fc.N);
					}
				}
			}
		}

		/** What a church's part has of its own, at a level: the nave's and the chancel's corbel tables under their eaves;
		 * the tower's string courses and its top. */
		void ChurchExtras(Pen& B, const Plan& P, int Level)
		{
			if (P.Kind == 3 || P.Kind == 4)
			{
				if (Level == 2) return;
				for (int F : { 0, 2 })
				{
					const Face Fc = FaceOf(P, F);
					Cornice(B, P, F, 0.0, Fc.Len, P.EW - 0.72, P.EW - 0.4, P.EW - 0.16, 0.13, 0.6, Level);
				}
				return;
			}
			if (P.Kind != 5) return;
			double ZBel = P.EW;
			for (const Opening& O : P.Ops)
				if (O.What == OpBelfry) ZBel = std::min(ZBel, O.Z0);
			StringCourse(B, P, Snap(P, 7.8), Level);
			StringCourse(B, P, Quant(P, ZBel - 0.25), Level);
			TowerTop(B, P, Level);
			if (Level == 0) TowerStreaks(B, P);
		}

		/** One part's three levels (a farmhouse, a barn, a byre, or a church's nave, chancel or tower). */
		void BuildPart(const Plan& P, Levels& Out)
		{
			const bool bInside = P.Hurt >= 1;
			{
				Pen B(Out.Near);
				for (int F = 0; F < 4; ++F) WallNear(B, P, F, bInside);
				for (const Opening& O : P.Ops) OpeningNear(B, P, O, bInside);
				BarnRoom(B, P, DayLights(P));
				if (P.Kind != 5) RoofNear(B, P, bInside);
				if (bInside) InsideNear(B, P);
				if (P.Kind >= 3) ChurchExtras(B, P, 0);
			}
			{
				Pen B(Out.Mid);
				for (int F = 0; F < 4; ++F) WallPlain(B, P, F, 1.75, true);
				if (P.Kind != 5) RoofPlain(B, P, 3, 2.4, true);
				if (bInside) InsidePlain(B, P, true);
				if (P.Kind >= 3) ChurchExtras(B, P, 1);
			}
			{
				Pen B(Out.Far);
				for (int F = 0; F < 4; ++F) WallPlain(B, P, F, 3.6, false);
				if (P.Kind != 5) RoofPlain(B, P, 40, P.Hurt == 0 ? 20.0 : 2.4, false);
				if (bInside) InsidePlain(B, P, false);
				if (P.Kind >= 3) ChurchExtras(B, P, 2);
			}
		}

		/** A mesh added to another, moved DX along x. */
		void Append(LandField::Mesh& To, const LandField::Mesh& From, double DX)
		{
			const uint32_t Base = static_cast<uint32_t>(To.Colours.size());
			for (size_t I = 0; I + 2 < From.Positions.size(); I += 3)
			{
				To.Positions.push_back(static_cast<float>(From.Positions[I] + DX));
				To.Positions.push_back(From.Positions[I + 1]);
				To.Positions.push_back(From.Positions[I + 2]);
			}
			To.Normals.insert(To.Normals.end(), From.Normals.begin(), From.Normals.end());
			To.Colours.insert(To.Colours.end(), From.Colours.begin(), From.Colours.end());
			for (uint32_t T : From.Triangles) To.Triangles.push_back(T + Base);
		}

		/** The church whole (Spec Kind 3): its nave at the middle, the chancel east of it (+x), the tower west, as LandField's
		 * LayChurch stands them; only the nave hurt (its roof holed), as the field's. */
		void BuildChurch(const Spec& S, Levels& Out)
		{
			Out = Levels();
			const double NL = S.Long, ND = S.Deep;
			struct PartAt
			{
				int Kind;
				double L, D, X;
			};
			const PartAt Parts[3] = { { 3, NL, ND, 0.0 }, { 4, ChancelL, ChancelD, 0.5 * NL + 0.5 * ChancelL }, { 5, TowerW, TowerW, -(0.5 * NL + 0.5 * TowerW) } };
			for (const PartAt& Pa : Parts)
			{
				Spec PS = S;
				PS.Kind = Pa.Kind;
				PS.Long = Pa.L;
				PS.Deep = Pa.D;
				PS.Roof = 0;
				PS.Storeys = 1;
				PS.Hurt = Pa.Kind == 3 ? std::min(S.Hurt, 1) : 0;
				Levels One;
				BuildPart(MakePlan(PS, NL, ND), One);
				Append(Out.Near, One.Near, Pa.X);
				Append(Out.Mid, One.Mid, Pa.X);
				Append(Out.Far, One.Far, Pa.X);
			}
		}
	}

	double EavesOf(const Spec& S) { return MakePlan(S).E; }
	double RidgeOf(const Spec& S) { return MakePlan(S).Ridge; }

	void Holes(const Spec& S, std::vector<Hole>& Out)
	{
		Out.clear();
		const Plan P = MakePlan(S);
		for (const Opening& O : P.Ops)
		{
			if (O.What == OpHidden) continue;
			const Face Fc = FaceOf(P, O.Face);
			const V3 M = At(Fc, 0.5 * (O.A0 + O.A1), 0.0, 0.0);
			Hole H;
			H.Face = O.Face;
			H.X = M.X;
			H.Y = M.Y;
			H.Z0 = O.Z0;
			H.Z1 = O.Z1;
			H.Wide = O.A1 - O.A0;
			H.What = O.What;
			Out.push_back(H);
		}
	}

	void Build(const Spec& S, Levels& Out)
	{
		if (S.Kind == 3)
		{
			BuildChurch(S, Out);
			return;
		}
		Out = Levels();
		BuildPart(MakePlan(S), Out);
	}
}
