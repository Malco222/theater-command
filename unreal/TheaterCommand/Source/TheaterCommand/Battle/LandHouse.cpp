#include "Battle/LandHouse.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
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
			OpBelfry = 11, // the tower's belfry opening, round-headed, its louvres sloping out
			OpShop = 12    // (a village house's) its shop's front: pilasters and a fascia on the wall round it, its display
			               // windows over a stall riser, its glazed door
		};
		/** Whether an opening runs down to the floor (the plinth cut for it, no sill under it). */
		bool Grounded(int What) { return What == OpDoor || What == OpCart || What == OpStable || What == OpChDoor || What == OpHidden || What == OpShop; }
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
			Col Render, Shop; // (a village house's) its lime render; its shopfront's paint
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
			// (a village house, Spec Kind 6: the farmhouse's hand, Kind 0, with its own looks)
			bool bVillage = false;
			int Finish = 0; // its walls: 0 the bocage's rubble, 1 cream limestone rubble (bLime), 2 lime render over the rubble, its dressed work bare
			/** Where a rendered wall's render has come away to the rubble (Finish 2): on face Face about (A, Z), W across and H
			 * up from there at most, ragged by its key. */
			struct Spall
			{
				int Face = 0;
				double A = 0.0, Z = 0.0, W = 0.5, H = 0.3;
				uint64_t Key = 1;
			};
			std::vector<Spall> Spalls;
			/** A shell splinter's or a bullet's strike in the render, R across. */
			struct Pock
			{
				int Face = 0;
				double A = 0.0, Z = 0.0, R = 0.03;
				uint64_t Key = 1;
			};
			std::vector<Pock> Pocks;
			/** A crack in the render, along its points (A, Z) on face Face, W wide: from an opening's corner, where the wall
			 * settled, or down from the eaves. */
			struct Crack
			{
				int Face = 0;
				std::vector<std::pair<double, double>> Pts;
				double W = 0.008;
			};
			std::vector<Crack> Cracks;
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

		/** (a rendered wall's, Finish 2) how high its render reaches at A along face F: the wall's top; where its top is
		 * broken, a band below the break, the render cracked off the rubble along it (the lowest of the top within 30 cm
		 * either side, less 22 to 62 cm, so a straight line between two places under 30 cm apart stays under the top). */
		double RenderTop(const Plan& P, const Face& Fc, int F, double A)
		{
			if (P.Hurt < 2) return TopOf(P, Fc, F, A);
			double Lo = 1e9;
			bool bBroke = false;
			for (double X = A - 0.3; X <= A + 0.3 + 1e-9; X += 0.05)
			{
				const double Xc = std::clamp(X, 0.01, Fc.Len - 0.01), T = TopOf(P, Fc, F, Xc);
				Lo = std::min(Lo, T);
				if (T < IntactTop(P, Fc, Xc) - 1e-6) bBroke = true;
			}
			if (!bBroke) return TopOf(P, Fc, F, A);
			// (its line broken small as well as large: the render cracked off in flakes along it)
			return Lo - (0.22 + 0.4 * Noise01(A / 0.6, 2.0 + F, P.Key ^ 0x2E7)) + 0.045 * Noise(A / 0.1, 6.0 + F, P.Key ^ 0x2E9);
		}

		/** A spall's gap in a course of the render: its left and right edges at the course's bed, its middle and its top. */
		struct RenderGap
		{
			double L[3] = { 0.0, 0.0, 0.0 }, R[3] = { 0.0, 0.0, 0.0 };
			double Lo() const { return std::min({ L[0], L[1], L[2] }); }
			double Hi() const { return std::max({ R[0], R[1], R[2] }); }
		};
		/** The gaps in course I of face F's render, left to right: each spall's ragged outline (a squared ellipse its key
		 * makes uneven, in and out, leaning, broad at its top and its foot). */
		std::vector<RenderGap> GapsIn(const Plan& P, int F, size_t I)
		{
			std::vector<RenderGap> Out;
			if (P.Finish != 2 || I + 1 >= P.Course.size()) return Out;
			const double Zs[3] = { P.Course[I], 0.5 * (P.Course[I] + P.Course[I + 1]), P.Course[I + 1] };
			for (const Plan::Spall& S : P.Spalls)
			{
				if (S.Face != F || Zs[2] <= S.Z - S.H + 1e-6 || Zs[0] >= S.Z + S.H - 1e-6) continue;
				RenderGap G;
				double Most = 0.0;
				for (int K = 0; K < 3; ++K)
				{
					// (squarer than an ellipse, so its top and foot are broad and ragged rather than coming to a point; its
					// two sides in and out on their own, in lobes and in smaller bites)
					const double T = std::fabs(Zs[K] - S.Z) / S.H, HW = T < 1.0 ? S.W * std::pow(1.0 - std::pow(T, 2.6), 1.0 / 2.6) : 0.0;
					const double Mid = S.A + 0.28 * S.W * Noise(Zs[K] / 0.5, 5.3, S.Key);
					G.L[K] = Mid - HW * (0.5 + 0.7 * Noise01(Zs[K] / 0.21, 3.1, S.Key) + 0.12 * Noise(Zs[K] / 0.08, 1.9, S.Key));
					G.R[K] = Mid + HW * (0.5 + 0.7 * Noise01(Zs[K] / 0.21, 7.7, S.Key) + 0.12 * Noise(Zs[K] / 0.08, 4.4, S.Key));
					Most = std::max(Most, G.R[K] - G.L[K]);
				}
				if (Most > 0.02) Out.push_back(G);
			}
			std::sort(Out.begin(), Out.end(), [](const RenderGap& X, const RenderGap& Y) { return X.L[1] < Y.L[1]; });
			return Out;
		}
		/** Whether a place on face F is in a spall's gap (its render come away). */
		bool InSpall(const Plan& P, int F, double A, double Z)
		{
			if (P.Finish != 2) return false;
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				if (Z < P.Course[I] || Z >= P.Course[I + 1]) continue;
				const double Zm = 0.5 * (P.Course[I] + P.Course[I + 1]);
				const bool bLow = Z < Zm;
				const double T = bLow ? (Z - P.Course[I]) / (Zm - P.Course[I]) : (Z - Zm) / (P.Course[I + 1] - Zm);
				for (const RenderGap& G : GapsIn(P, F, I))
				{
					const int K = bLow ? 0 : 1;
					if (A > Lerp(G.L[K], G.L[K + 1], T) && A < Lerp(G.R[K], G.R[K + 1], T)) return true;
				}
				return false;
			}
			return false;
		}

		/**
		 * A village house's wear, once its openings are laid: its shop's glass and shutters as the house stands; where its
		 * walls are rendered, the render come away to the rubble in patches (low along its foot where the damp lifts it,
		 * here and there up the walls, more where it was hit or burnt), and the strikes of shell splinters and bullets in it,
		 * a spray of them round a burst or a line of them along a burst of fire; the far tone of its rendered walls.
		 */
		void VillageWear(Plan& P)
		{
			for (Opening& O : P.Ops)
			{
				if (O.What != OpShop) continue;
				const uint64_t H = Hash(P.Key, 0x540C, static_cast<uint64_t>(std::llround(O.A0 * 20.0)));
				O.Shutters = Unit(H) < 0.3 ? 1 : 0; // (its shutter boards up over the glass, or down)
				O.Cracked = P.Hurt == 1 ? 1 + static_cast<int>(Unit(Mix(H)) * 3.0) : P.Hurt == 0 && Unit(Mix(H ^ 1)) < 0.25 ? 1 : 0;
				O.bGlass = P.Hurt < 2;
				O.bCharred = P.Hurt == 3;
				O.bLeaf = P.Hurt < 3;
				if (P.Hurt == 3) O.Shutters = 0;
			}
			if (P.Finish != 2) return;
			Look& K = P.K;
			K.Wall = Scaled(Mixed(K.Render, Mixed(K.StoneMean, K.Mortar, WallMortar), 0.08), 0.955);
			Rng RS(P.Key ^ 0x5BA1);
			for (int F = 0; F < 4; ++F)
			{
				const Face Fc = FaceOf(P, F);
				const double Many = P.Hurt == 0 ? 0.16 : P.Hurt == 1 ? 0.24 : P.Hurt == 2 ? 0.3 : 0.42;
				const int Want = std::max(0, static_cast<int>(std::lround(Fc.Len * Many + RS.Range(-0.4, 0.6))));
				const int Foot = std::max(0, static_cast<int>(std::lround(Fc.Len * 0.1 + RS.Range(-0.3, 0.7))));
				int Got = 0, GotFoot = 0;
				for (int Try = 0; Try < 8 * (Want + Foot) && (Got < Want || GotFoot < Foot); ++Try)
				{
					const bool bFoot = GotFoot < Foot && (Got >= Want || RS.Next() < 0.5);
					Plan::Spall Sp;
					Sp.Face = F;
					Sp.Key = Hash(P.Key ^ 0x5BA2, static_cast<uint64_t>(F * 256 + Try));
					Sp.W = bFoot ? RS.Range(0.4, 1.1) : RS.Range(0.22, 0.75);
					Sp.H = bFoot ? RS.Range(0.16, 0.34) : RS.Range(0.16, 0.5);
					Sp.A = RS.Range(0.6 + Sp.W, std::max(0.61 + Sp.W, Fc.Len - 0.6 - Sp.W));
					const double Hi = Fc.bGable ? IntactTop(P, Fc, Sp.A) : P.EW;
					Sp.Z = bFoot ? RS.Range(0.5 + Sp.H, 0.75 + Sp.H) : RS.Range(0.55 + Sp.H, std::max(0.56 + Sp.H, Hi - 0.5 - Sp.H));
					const double X0 = Sp.A - 1.7 * Sp.W, X1 = Sp.A + 1.7 * Sp.W, Z0 = Sp.Z - Sp.H, Z1 = Sp.Z + Sp.H;
					bool bOk = X0 > 0.3 && X1 < Fc.Len - 0.3;
					// (well under the render's top, a course and more, for the collars of render drawn round it)
					for (double X = X0 - 0.1; bOk && X <= X1 + 0.1 + 1e-9; X += 0.2)
						if (Z1 > RenderTop(P, Fc, F, std::clamp(X, 0.01, Fc.Len - 0.01)) - 0.45) bOk = false;
					for (const Opening& O : P.Ops)
						if (bOk && O.Face == F && X1 > O.A0 - 0.15 && X0 < O.A1 + 0.15 && Z1 > O.Z0 - 0.15 && Z0 < O.Z1 + (O.What == OpShop ? 0.7 : 0.15)) bOk = false;
					for (const Plan::Spall& Q : P.Spalls)
						if (bOk && Q.Face == F && X1 > Q.A - 1.7 * Q.W - 0.15 && X0 < Q.A + 1.7 * Q.W + 0.15 && Z1 > Q.Z - Q.H - 0.1 && Z0 < Q.Z + Q.H + 0.1) bOk = false;
					if (!bOk) continue;
					P.Spalls.push_back(Sp);
					++(bFoot ? GotFoot : Got);
				}
			}
			Rng RP(P.Key ^ 0x90C4);
			std::vector<std::pair<int, std::pair<double, double>>> Bursts; // (each burst's face and middle, for its cracks)
			const int Sprays = P.Hurt == 0 ? (RP.Next() < 0.45 ? 1 : 0) : P.Hurt == 1 ? 1 + static_cast<int>(RP.Below(2)) : P.Hurt == 2 ? 2 + static_cast<int>(RP.Below(2)) : 1 + static_cast<int>(RP.Below(2));
			for (int K2 = 0; K2 < Sprays; ++K2)
			{
				const int F = RP.Next() < 0.55 ? 0 : static_cast<int>(RP.Below(4));
				const Face Fc = FaceOf(P, F);
				const bool bLine = RP.Next() < 0.4;
				const double CA = RP.Range(1.0, std::max(1.01, Fc.Len - 1.0)), CZ = RP.Range(0.9, std::max(0.91, std::min(P.EW, 4.6) - 0.5));
				const int N = bLine ? 8 + static_cast<int>(RP.Below(9)) : 22 + static_cast<int>(RP.Below(24));
				if (!bLine) Bursts.push_back({ F, { CA, CZ } });
				const double Sig = RP.Range(0.55, 1.1), Len = RP.Range(1.2, 3.0), Slant = RP.Range(-0.35, 0.35);
				for (int J = 0; J < N; ++J)
				{
					Plan::Pock Q;
					Q.Face = F;
					Q.Key = Hash(P.Key ^ 0x90C5, static_cast<uint64_t>(K2 * 64 + J));
					if (bLine)
					{
						const double T = RP.Range(-0.5, 0.5);
						Q.A = CA + T * Len;
						Q.Z = CZ + T * Len * Slant + RP.Range(-0.06, 0.06);
						Q.R = RP.Range(0.018, 0.035);
					}
					else
					{
						const double Rr = 0.7 * Sig * std::sqrt(-2.0 * std::log(std::max(1e-6, RP.Next()))), Th = RP.Range(0.0, 2.0 * Pi);
						Q.A = CA + Rr * std::cos(Th);
						Q.Z = CZ + 0.8 * Rr * std::sin(Th);
						Q.R = RP.Range(0.015, 0.03) + 0.03 * std::exp(-Rr / 0.4);
					}
					if (Q.A < 0.1 || Q.A > Fc.Len - 0.1 || Q.Z < 0.4 || Q.Z > RenderTop(P, Fc, F, Q.A) - 0.08) continue;
					bool bOk = true;
					for (const Opening& O : P.Ops)
						if (O.Face == F && Q.A > O.A0 - 0.06 && Q.A < O.A1 + 0.06 && Q.Z > O.Z0 - 0.06 && Q.Z < O.Z1 + (O.What == OpShop ? 0.62 : 0.06)) bOk = false;
					if (bOk) P.Pocks.push_back(Q);
				}
			}
			// The render's cracks: from its openings' corners, out and up from a head's or out and down from a sill's, where
			// the wall settled round them; a long one down from the eaves now and then; and round a burst, from where it
			// struck. Each wanders as a crack does, a few centimetres off its line every few centimetres; it stops at an
			// opening, at a spall or at the render's top.
			Rng RK(P.Key ^ 0xC4AC);
			auto Lay = [&](int F, double A, double Z, double Ang, double Len, double W) {
				const Face Fc = FaceOf(P, F);
				Plan::Crack C;
				C.Face = F;
				C.W = W;
				const int N = std::max(3, static_cast<int>(std::ceil(Len / 0.11)));
				const double Step = Len / N;
				for (int J = 0; J <= N; ++J)
				{
					if (A < 0.15 || A > Fc.Len - 0.15 || Z < 0.45 || Z > RenderTop(P, Fc, F, A) - 0.06 || InSpall(P, F, A, Z)) break;
					bool bIn = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && O.What != OpHidden && A > O.A0 - 0.05 && A < O.A1 + 0.05 && Z > O.Z0 - 0.05 && Z < O.Z1 + (O.What == OpShop ? 0.62 : 0.05)) bIn = true;
					if (bIn) break;
					C.Pts.push_back({ A, Z });
					Ang += RK.Range(-0.45, 0.45);
					A += Step * std::sin(Ang);
					Z += Step * std::cos(Ang);
				}
				if (C.Pts.size() >= 3) P.Cracks.push_back(C);
			};
			for (const Opening& O : P.Ops)
			{
				if (O.What == OpHidden || O.Face < 0 || O.Face > 3) continue;
				for (int Corner = 0; Corner < 4; ++Corner)
				{
					const bool bHead = Corner < 2, bLeft = Corner % 2 == 0;
					if (RK.Next() > (bHead ? 0.2 + 0.1 * P.Hurt : 0.12 + 0.06 * P.Hurt)) continue;
					const double Out = RK.Range(0.35, 0.95) * (bLeft ? -1.0 : 1.0);
					const double A = (bLeft ? O.A0 - 0.2 : O.A1 + 0.2) + RK.Range(-0.04, 0.04);
					const double Z = bHead ? O.Z1 + (O.What == OpShop ? 0.66 : 0.22) : O.Z0 - 0.12;
					const double Deep_ = RK.Range(0.005, 0.009) + 0.0015 * P.Hurt;
					const double Size_ = RK.Range(0.3, 1.1);
					Lay(O.Face, A, Z, bHead ? Out : Pi - Out, Size_, Deep_);
				}
			}
			for (int F = 0; F < 4; ++F)
			{
				const Face Fc = FaceOf(P, F);
				if (Fc.bGable || RK.Next() > 0.35 + 0.15 * P.Hurt) continue;
				const double A = RK.Range(0.8, std::max(0.81, Fc.Len - 0.8));
				const double Deep_ = RK.Range(0.006, 0.01) + 0.0015 * P.Hurt;
				const double Size_ = RK.Range(0.8, 2.4);
				const double Turn_ = Pi + RK.Range(-0.15, 0.15);
				Lay(F, A, RenderTop(P, Fc, F, A) - 0.08, Turn_, Size_, Deep_);
			}
			for (const auto& [F, M] : Bursts)
			{
				const int N = 2 + static_cast<int>(RK.Below(3));
				for (int J = 0; J < N; ++J)
				{
					const double Ang = RK.Range(0.0, 2.0 * Pi), R0 = RK.Range(0.04, 0.12);
					const double Deep_ = RK.Range(0.006, 0.011);
					const double Size_ = RK.Range(0.3, 0.9);
					Lay(F, M.first + R0 * std::sin(Ang), M.second + R0 * std::cos(Ang), Ang, Size_, Deep_);
				}
			}
		}

		Plan MakePlan(const Spec& S, double NaveL = 18.0, double NaveD = 8.6)
		{
			Plan P;
			P.S = S;
			P.Key = S.Key ? S.Key : 1;
			P.L = S.Long;
			P.D = S.Deep;
			P.Hurt = std::clamp(S.Hurt, 0, 3);
			// (a village house: the farmhouse's hand, its looks its own)
			P.bVillage = S.Kind == 6;
			P.Kind = P.bVillage ? 0 : std::clamp(S.Kind, 0, 5);
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
			// (a village house's walls: the rubble, the cream limestone, or a lime render over the rubble with its quoins and
			// its openings' dressed stone left bare, as the bourg's houses stood)
			if (P.bVillage)
			{
				const double U = Unit(Mix(P.Key ^ 0x11E5));
				P.Finish = U < 0.28 ? 1 : U < 0.74 ? 2 : 0;
				P.bLime = P.Finish == 1;
			}
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
			// (a village house's render: lime, off-white, cream, grey, buff or a faded ochre; its shopfront's paint)
			if (P.bVillage)
			{
				static const Col Renders[6] = { Rgb(198, 190, 172), Rgb(196, 182, 150), Rgb(182, 180, 170), Rgb(192, 176, 160), Rgb(190, 172, 132), Rgb(176, 170, 156) };
				K.Render = Varied(Renders[static_cast<size_t>(Unit(Mix(P.Key ^ 0x2E40)) * 6.0) % 6], 0.95 + 0.07 * Unit(Mix(P.Key ^ 0x2E41)), 0.0);
				static const Col Shops[5] = { Rgb(54, 78, 62), Rgb(106, 46, 40), Rgb(58, 72, 92), Rgb(72, 54, 40), Rgb(88, 104, 112) };
				K.Shop = Shops[static_cast<size_t>(Unit(Mix(P.Key ^ 0x5400)) * 5.0) % 5];
			}
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
					// (a village house of two storeys or more, three bays or more: now and then a shop on its ground floor, in
					// the bays one side of its door, two at most, its own door in it)
					int Shop0 = -1, Shop1 = -2;
					if (P.bVillage && bFront && Storeys >= 2 && Bays >= 3 && Unit(Mix(P.Key ^ 0x5409)) < 0.42)
					{
						const int Left = Door, Right = Bays - 1 - Door;
						if (Left > Right || (Left == Right && Unit(Mix(P.Key ^ 0x540A)) < 0.5))
						{
							Shop1 = Door - 1;
							Shop0 = std::max(0, Door - 2);
						}
						else
						{
							Shop0 = Door + 1;
							Shop1 = std::min(Bays - 1, Door + 2);
						}
					}
					if (Shop0 >= 0)
					{
						Opening O;
						O.Face = F;
						O.What = OpShop;
						O.A0 = std::max(0.86, Fc.Len * Shop0 / Bays + 0.34);
						O.A1 = std::min(Fc.Len - 0.86, Fc.Len * (Shop1 + 1) / Bays - 0.34);
						O.Z0 = 0.0;
						O.Z1 = Snap(P, 2.55);
						P.Ops.push_back(O);
					}
					for (int Row = 0; Row < Storeys; ++Row)
						for (int B = 0; B < Bays; ++B)
						{
							if (Row == 0 && B >= Shop0 && B <= Shop1) continue;
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
			if (P.bVillage) VillageWear(P);
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
			const Col Own = StoneOwn(P, H);
			if (P.Finish == 2)
			{
				// (a rendered wall's rubble, bared where the render came away (the art director's village round 1: not a
				// flat pillow, which read hollow from above, like a pigeonhole): a solid stone standing out of its mortar,
				// its sides showing, its face hewn in four uneven facets, each corner standing out its own way; all of it
				// under the render's face, which stood over it)
				double D[4];
				for (int K = 0; K < 4; ++K) D[K] = std::min(0.017, (0.008 + 0.009 * Unit(Hash(H, 0x60 + static_cast<uint64_t>(K)))) * std::min(Proud, 1.2));
				const double DM = std::min(0.0215, std::max({ D[0], D[1], D[2], D[3] }) + 0.002 + 0.004 * Unit(Mix(H ^ 3)));
				const V3 Peak = At(Fc, MA, MZ, DM);
				V3 Rim[4], Foot[4];
				for (int K = 0; K < 4; ++K)
				{
					Rim[K] = At(Fc, CA[K], CZ[K], D[K]);
					Foot[K] = At(Fc, CA[K], CZ[K], 0.0);
				}
				for (int K = 0; K < 4; ++K)
				{
					const int K1 = (K + 1) % 4;
					const double FA = (MA + CA[K] + CA[K1]) / 3.0, FZ = (MZ + CZ[K] + CZ[K1]) / 3.0;
					const Col Face = Scaled(Weathered(P, Fc, F, FA, FZ, Own, true), 0.95 + 0.1 * Unit(Hash(H, 0x64 + static_cast<uint64_t>(K))));
					B.Tri1(Peak, Rim[K], Rim[K1], Face, Fc.N);
					// (its side, down into the mortar: a bed's top lit, its foot's underside dark)
					const V3 Out = K == 0 ? Up * -1.0 : K == 1 ? Fc.T : K == 2 ? Up : Fc.T * -1.0;
					B.Quad1(Foot[K], Foot[K1], Rim[K1], Rim[K], Scaled(Face, K == 2 ? 0.98 : K == 0 ? 0.7 : 0.84), Out);
				}
				return;
			}
			const double Bulge = (0.009 + 0.014 * Unit(Mix(H ^ 3))) * Proud;
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
		/** A shop's front: how far its pilasters stand out either side of its opening, and the top of its fascia. */
		constexpr double ShopPilaster = 0.25;
		double ShopFascia(const Opening& O) { return O.Z1 + 0.46; }
		/** A rendered wall's face, out from the mortar's plane; its dressed work standing a little proud of it. */
		constexpr double RenderD = 0.024, RenderProud = 0.03;

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
				// (a shop's front: its pilasters and its fascia stand over the wall round it, nothing laid behind them)
				if (O.What == OpShop)
				{
					if (Z1 <= ShopFascia(O) + 1e-6) Res.push_back({ O.A0 - ShopPilaster, O.A1 + ShopPilaster });
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
			if (O.What == OpShop) return 0.1; // (a shop's front: its glass and riser just inside the wall's face)
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

		/** A rendered wall's render at a place: lime, a little uneven, a patch made good here and there a shade off; weathered
		 * as the stone (damp at the foot, the rain's streaks, the sills' drips, a fire's soot). */
		Col RenderAt(const Plan& P, const Face& Fc, int F, double A, double Z)
		{
			Col C = Varied(P.K.Render, 0.955 + 0.06 * Noise(A / 1.4, Z / 1.1, P.Key ^ (0x2E1 + static_cast<uint64_t>(F))) + 0.025 * Noise(A / 0.33, Z / 0.27, P.Key ^ 0x2E2), 0.0);
			C = Mixed(C, Varied(P.K.Render, 0.9, 0.02), 0.6 * Smooth(0.68, 0.8, Noise01(A / 1.7 + 7.0 * F, Z / 1.3, P.Key ^ 0x2E3)));
			// (the dirt the rain brings down under the eaves, or under a gable's verge: darkest just under them, in runs where
			// the gutter overflows or the verge drips, fading a metre or so down; the mud the rain splashes up its foot)
			const double Lip = Fc.bGable ? IntactTop(P, Fc, A) : P.EW;
			if (Z < Lip)
			{
				const double Runs = Smooth(0.35, 0.8, Noise01(A / 0.4 + 11.0 * F, 0.7, P.Key ^ 0x2E4));
				const double Reach = 0.35 + 1.1 * Noise01(A / 0.55 + 3.0 * F, 1.9, P.Key ^ 0x2E5);
				C = Scaled(C, 1.0 - (0.05 + 0.13 * Runs) * std::exp(-(Lip - Z) / Reach));
			}
			const double Splash = 0.45 + 0.3 * Noise01(A / 0.6 + 4.0 * F, 1.3, P.Key ^ 0x2E6);
			C = Mixed(C, Rgb(120, 108, 90), 0.22 * Smooth(Splash + 0.12, Splash - 0.12, Z));
			// (the rain's streaks down it, broad, as the render's pieces carry them: the stone's finer ones would come out
			// as smudges)
			C = Scaled(C, 1.0 - 0.06 * Smooth(0.3, 0.8, Noise01(A / 1.1 + 5.0 * F, Z / 2.4, P.Key ^ 0x2E8)));
			return Weathered(P, Fc, F, A, Z, C, false);
		}
		/** The render's broken edge: fresh lime, paler than its face. */
		Col RenderEdge(const Col& C) { return Scaled(Mixed(C, Rgb(226, 220, 204), 0.35), 0.9); }

		/**
		 * A rendered wall's render over course I of face F (Finish 2): over each stretch the stones would be laid along
		 * (Free, between its quoins and its openings' dressed work, lapping a little under them), round the spalls' gaps,
		 * whose broken edges show its thickness; up to the wall's top (cut to a gable's rake), or where the top is broken
		 * to a band below it, its edge there broken too.
		 */
		void RenderCourse(Pen& B, const Plan& P, const Face& Fc, int F, size_t I, const std::vector<std::pair<double, double>>& Free, const std::vector<RenderGap>& Gaps)
		{
			const double Z0 = P.Course[I], Z1 = P.Course[I + 1], Zm = 0.5 * (Z0 + Z1);
			const double Zs[3] = { Z0, Zm, Z1 };
			auto RC = [&](double A, double Z) { return RenderAt(P, Fc, F, A, Z); };
			auto RT = [&](double A) { return RenderTop(P, Fc, F, std::clamp(A, 0.01, Fc.Len - 0.01)); };
			struct Edge
			{
				double X[3];
				bool bGap;
			};
			for (const auto& [F0, F1] : Free)
			{
				const double E0 = F0 > 0.0 ? F0 - 0.008 : F0, E1 = F1 < Fc.Len ? F1 + 0.008 : F1;
				std::vector<std::pair<Edge, Edge>> Pieces;
				// (round each gap a collar 6 cm wide drawn beside it, the stretches between the collars plain, so only the
				// collars' pieces go uncut by the render's top: the spalls lie well under it, collars and all)
				Edge Left{ { E0, E0, E0 }, false };
				for (const RenderGap& G : Gaps)
				{
					if (G.Hi() <= F0 || G.Lo() >= F1) continue;
					const double C0 = std::clamp(G.Lo() - 0.06, E0, E1), C1 = std::clamp(G.Hi() + 0.06, E0, E1);
					if (C0 > Left.X[0] + 0.01)
					{
						Pieces.push_back({ Left, Edge{ { C0, C0, C0 }, false } });
						Left = Edge{ { C0, C0, C0 }, false };
					}
					Edge Right{ { std::clamp(G.L[0], E0, E1), std::clamp(G.L[1], E0, E1), std::clamp(G.L[2], E0, E1) }, true };
					Pieces.push_back({ Left, Right });
					Pieces.push_back({ Edge{ { std::clamp(G.R[0], E0, E1), std::clamp(G.R[1], E0, E1), std::clamp(G.R[2], E0, E1) }, true }, Edge{ { C1, C1, C1 }, false } });
					Left = Edge{ { C1, C1, C1 }, false };
				}
				Pieces.push_back({ Left, Edge{ { E1, E1, E1 }, false } });
				for (auto& [Lf, Rt] : Pieces)
				{
					double Wd = 0.0;
					for (int K = 0; K < 3; ++K)
					{
						if (Rt.X[K] < Lf.X[K]) Rt.X[K] = Lf.X[K] = 0.5 * (Lf.X[K] + Rt.X[K]);
						Wd = std::max(Wd, Rt.X[K] - Lf.X[K]);
					}
					if (Wd < 0.01) continue;
					if (Lf.bGap || Rt.bGap)
					{
						// (beside a gap: its two halves, bed to middle and middle to top, cut along between the edges; the
						// spalls lie well under the render's top)
						const int N = std::max(1, static_cast<int>(std::ceil(Wd / 0.45)));
						for (int H = 0; H < 2; ++H)
							for (int K = 0; K < N; ++K)
							{
								const double XA0 = Lerp(Lf.X[H], Rt.X[H], double(K) / N), XA1 = Lerp(Lf.X[H], Rt.X[H], double(K + 1) / N);
								const double XB0 = Lerp(Lf.X[H + 1], Rt.X[H + 1], double(K) / N), XB1 = Lerp(Lf.X[H + 1], Rt.X[H + 1], double(K + 1) / N);
								B.Quad(At(Fc, XA0, Zs[H], RenderD), At(Fc, XA1, Zs[H], RenderD), At(Fc, XB1, Zs[H + 1], RenderD), At(Fc, XB0, Zs[H + 1], RenderD),
									RC(XA0, Zs[H]), RC(XA1, Zs[H]), RC(XB1, Zs[H + 1]), RC(XB0, Zs[H + 1]), Fc.N);
							}
						// (its broken edges, the render's thickness, facing into the gap)
						for (int Sd = 0; Sd < 2; ++Sd)
						{
							const Edge& E = Sd == 0 ? Lf : Rt;
							if (!E.bGap) continue;
							for (int H = 0; H < 2; ++H)
							{
								if (std::fabs(E.X[H] - E.X[H + 1]) < 1e-6 && (Rt.X[H] - Lf.X[H]) < 1e-6) continue;
								const Col C = RenderEdge(RC(E.X[H], Zs[H]));
								B.Quad1(At(Fc, E.X[H], Zs[H], RenderD), At(Fc, E.X[H + 1], Zs[H + 1], RenderD), At(Fc, E.X[H + 1], Zs[H + 1], 0.004), At(Fc, E.X[H], Zs[H], 0.004), C, Fc.T * (Sd == 0 ? -1.0 : 1.0));
							}
						}
						continue;
					}
					// (a plain stretch: in pieces of 45 cm, fine enough to carry its streaks and stains, or up near the
					// render's top 28 cm, cut to it, split at a gable's rake's apex and kicks)
					const double XL = Lf.X[0], XR = Rt.X[0];
					double Low = 1e9;
					for (double X = XL; X <= XR + 1e-9; X += 0.25) Low = std::min(Low, RT(X));
					Low = std::min(Low, RT(XR));
					const bool bUp = Z1 > Low - 1e-6;
					if (Z0 >= std::max({ RT(XL), RT(XR), RT(0.5 * (XL + XR)) }) + 0.3) continue;
					std::vector<double> Cuts{ XL };
					// (along a broken top's band, finer, so its edge's flakes show)
					const bool bBandHere = bUp && P.Hurt >= 2 && RT(0.5 * (XL + XR)) < TopOf(P, Fc, F, std::clamp(0.5 * (XL + XR), 0.01, Fc.Len - 0.01)) - 0.05;
					const double Step = bBandHere ? 0.1 : bUp ? 0.28 : 0.45;
					const int N = std::max(1, static_cast<int>(std::ceil((XR - XL) / Step)));
					for (int K = 1; K < N; ++K) Cuts.push_back(XL + (XR - XL) * K / N);
					if (Fc.bGable && bUp)
						for (double E : { 0.5 * Fc.Len, 0.5 * Fc.Len - P.Vb, 0.5 * Fc.Len + P.Vb })
							if (E > XL + 1e-4 && E < XR - 1e-4) Cuts.push_back(E);
					// (under a sill's ends, across its two drips, so the pieces carry them as streaks, not smudges)
					for (const Opening& O : P.Ops)
						if (O.Face == F && Silled(O.What) && Z0 < O.Z0 && Z1 > O.Z0 - 2.3)
							for (const double S : { O.A0 + 0.06, O.A1 - 0.06 })
								for (const double E : { S - 0.11, S, S + 0.11 })
									if (E > XL + 0.02 && E < XR - 0.02) Cuts.push_back(E);
					std::sort(Cuts.begin(), Cuts.end());
					Cuts.push_back(XR);
					for (size_t K = 0; K + 1 < Cuts.size(); ++K)
					{
						const double C0 = Cuts[K], C1 = Cuts[K + 1];
						if (C1 - C0 < 1e-4) continue;
						if (!bUp)
						{
							B.Quad(At(Fc, C0, Z0, RenderD), At(Fc, C1, Z0, RenderD), At(Fc, C1, Z1, RenderD), At(Fc, C0, Z1, RenderD), RC(C0, Z0), RC(C1, Z0), RC(C1, Z1), RC(C0, Z1), Fc.N);
							continue;
						}
						const double L0 = RT(C0 + 1e-4), L1 = RT(C1 - 1e-4);
						if (std::max(L0, L1) <= Z0 + 1e-6) continue;
						Cut(B, Fc, C0, C1, Z0, Z1, L0, L1, RenderD, Fc.N, RC);
						// (where it stops short of a broken top: its broken edge along there)
						const bool bBand = P.Hurt >= 2 && L0 < TopOf(P, Fc, F, C0 + 1e-4) - 0.05 && L1 < TopOf(P, Fc, F, C1 - 1e-4) - 0.05;
						if (bBand && std::min(L0, L1) < Z1 && std::max(L0, L1) > Z0)
						{
							double A0 = C0, A1 = C1, T0 = L0, T1 = L1;
							// (the line's part within the course)
							auto Clip = [&](double Z, bool bLow) {
								if (std::fabs(L1 - L0) < 1e-9) return;
								const double X = C0 + (C1 - C0) * (Z - L0) / (L1 - L0);
								if (bLow ? (T0 < Z) : (T0 > Z)) { A0 = X; T0 = Z; }
								if (bLow ? (T1 < Z) : (T1 > Z)) { A1 = X; T1 = Z; }
							};
							Clip(Z0, true);
							Clip(Z1, false);
							if (A1 - A0 > 1e-4 && T0 >= Z0 - 1e-6 && T1 >= Z0 - 1e-6 && T0 <= Z1 + 1e-6 && T1 <= Z1 + 1e-6)
							{
								const Col C = RenderEdge(RC(0.5 * (A0 + A1), 0.5 * (T0 + T1)));
								B.Quad1(At(Fc, A0, T0, RenderD), At(Fc, A1, T1, RenderD), At(Fc, A1, T1, 0.0), At(Fc, A0, T0, 0.0), C, Up + Fc.N * 0.3);
							}
						}
					}
				}
			}
		}

		/** A rendered wall's broken top, over A0 to A1 at T (the art director's village round 1: solid, not a smooth
		 * slab): the tops of its stones laid along it in two rows, the face's and the inner face's, each a block a few
		 * centimetres high, its top uneven, the rubble core's mortar between and round them; sooted on a shell. */
		void CapStones(Pen& B, const Plan& P, const Face& Fc, int F, double A0, double A1, double T)
		{
			for (int Row = 0; Row < 2; ++Row)
			{
				double A = A0 + 0.01;
				while (A1 - A > 0.08)
				{
					const uint64_t H = Hash(P.Key ^ 0xCA95, static_cast<uint64_t>(F * 8 + Row), static_cast<uint64_t>(std::llround(A * 1000.0)));
					double Ln = 0.12 + 0.24 * Unit(H);
					if (A1 - A - Ln < 0.1) Ln = A1 - A - 0.01;
					const double SA0 = A, SA1 = A + Ln;
					A = SA1 + 0.012 + 0.022 * Unit(Mix(H ^ 1));
					if (Ln < 0.06 || Unit(Mix(H ^ 5)) < 0.12) continue; // (now and then one gone, the core showing)
					const double Dp = 0.13 + 0.13 * Unit(Mix(H ^ 2)), In = 0.012 + 0.02 * Unit(Mix(H ^ 3));
					const double DO = Row == 0 ? -In : -P.Thick + In + Dp, DI = DO - Dp; // (its face's side, then its back)
					double Hh[4];
					const double Rise = 0.015 + 0.04 * Unit(Mix(H ^ 6));
					for (int K = 0; K < 4; ++K) Hh[K] = T + Rise * (0.4 + 1.2 * Unit(Hash(H, 0x70 + static_cast<uint64_t>(K)))); // (set askew)
					Col Own = Varied(StoneOwn(P, H), 0.94, 0.0);
					if (P.Hurt == 3) Own = Mixed(Own, P.K.Soot, 0.35 + 0.25 * Unit(Mix(H ^ 4)));
					else Own = Mixed(Own, Rgb(168, 160, 142), 0.15); // (its dust)
					const V3 C0 = At(Fc, SA0, Hh[0], DO), C1 = At(Fc, SA1, Hh[1], DO), C2 = At(Fc, SA1, Hh[2], DI), C3 = At(Fc, SA0, Hh[3], DI);
					B.Tri1(C0, C1, C2, Own, Up);
					B.Tri1(C0, C2, C3, Scaled(Own, 0.97), Up);
					const V3 F0 = At(Fc, SA0, T, DO), F1 = At(Fc, SA1, T, DO), F2 = At(Fc, SA1, T, DI), F3 = At(Fc, SA0, T, DI);
					B.Quad1(F0, F1, C1, C0, Scaled(Own, 0.86), Fc.N);
					B.Quad1(F2, F3, C3, C2, Scaled(Own, 0.8), Fc.N * -1.0);
					B.Quad1(F1, F2, C2, C1, Scaled(Own, 0.82), Fc.T);
					B.Quad1(F3, F0, C0, C3, Scaled(Own, 0.82), Fc.T * -1.0);
				}
			}
		}

		void WallNear(Pen& B, const Plan& P, int F, bool bInside)
		{
			const Face Fc = FaceOf(P, F);
			const bool bRender = P.Finish == 2;
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
				// (a rendered wall: its render over the course, the stones laid only where it has come away)
				const std::vector<RenderGap> Gaps = bRender ? GapsIn(P, F, I) : std::vector<RenderGap>{};
				if (bRender) RenderCourse(B, P, Fc, F, I, Free, Gaps);
				auto Bare = [&](double X0, double X1) {
					for (const RenderGap& G : Gaps)
						if (X1 > G.Lo() - 0.03 && X0 < G.Hi() + 0.03) return true;
					if (P.Hurt < 2) return false;
					double Lo = 1e9;
					for (int K = 0; K <= 4; ++K) Lo = std::min(Lo, RenderTop(P, Fc, F, std::clamp(Lerp(X0, X1, K / 4.0), 0.01, Fc.Len - 0.01)));
					return Z1 > Lo - 0.06;
				};
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
						// (a rendered wall's rubble, seen only where the render came away: smaller, the render's own backing
						// rather than the farmhouse's face stones; the art director's village round 1 carry)
						if (bRender) Ln = std::min(Ln, 0.16 + 0.22 * Unit(Hash(P.Key ^ 0x5B1E, static_cast<uint64_t>(F * 4096) + I, static_cast<uint64_t>(std::llround(A * 100.0)))));
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
						if (bRender && !Bare(SA0, SA1)) continue;
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
						if (Kind < 0.05 && Ln > 0.65 && !bRender)
						{
							// a through-stone, long and standing proud
							Pillow(B, P, Fc, F, SA0, SA1, B0, B1, T0, T1, H, 1.5);
						}
						else if (!bRender && Kind < 0.13 && ZUp < 1e8 && (Fc.bGable || ZUp <= P.EW + 1e-6) && ZUp <= LowTop(SA0, SA1) + 1e-6
						         && std::none_of(ResUp.begin(), ResUp.end(), [&](const std::pair<double, double>& Q) { return Q.first < SA1 + 0.02 && Q.second > SA0 - 0.02; })
						         && !(Fc.bGable && (Sloped(P, Fc, F, SA0) && ZUp > std::min(Top(SA0), Top(SA1)) - 0.05)))
						{
							// a tall one, up through the course above
							Pillow(B, P, Fc, F, SA0, SA1, B0, B1, ZUp + Wave(I + 2, SA0), ZUp + Wave(I + 2, SA1), H, 1.2);
							Standing.push_back({ SA0, SA1 });
						}
						else if ((Kind < 0.5 || (bRender && Kind < 0.85)) && Hi > 0.2)
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
				const double QP = bRender ? RenderProud : 0.018;
				if (Z1 <= Top(0.02) + 1e-6 && !CornerHid(P, F, false, Z0, Z1)) Block(B, Fc, -QP, Q0 - 0.008, Z0 + 0.005, Z1 - 0.005, QP, C0);
				if (Z1 <= Top(Fc.Len - 0.02) + 1e-6 && !CornerHid(P, F, true, Z0, Z1)) Block(B, Fc, Fc.Len - Q1 + 0.008, Fc.Len + QP, Z0 + 0.005, Z1 - 0.005, QP, C1);
			}
			// The openings' dressed jambs, lintels and sills (a shop's front its own).
			const double JP = bRender ? RenderProud - 0.002 : 0.016, LP = bRender ? RenderProud : 0.018;
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F || O.What == OpHidden || O.What == OpShop) continue;
				const double TopHere = std::min(Top(O.A0 - 0.2), Top(O.A1 + 0.2));
				for (size_t I = 0; I + 1 < P.Course.size(); ++I)
				{
					const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
					if (Z1 > TopHere + 1e-6) break;
					const uint64_t H = Hash(P.Key, 0x1A, I * 64 + static_cast<uint64_t>(std::llround(O.A0 * 10.0)));
					const Col C = Varied(P.K.Dressed, 0.95 + 0.09 * Unit(H), 0.0);
					if (Z0 >= O.Z0 - 1e-6 && Z1 <= SpringOf(O) + 1e-6)
					{
						Block(B, Fc, O.A0 - JambLen(P, I, -1) + 0.008, O.A0, Z0 + 0.005, Z1 - 0.005, JP, Weathered(P, Fc, F, O.A0 - 0.15, Z0, C));
						Block(B, Fc, O.A1, O.A1 + JambLen(P, I, 1) - 0.008, Z0 + 0.005, Z1 - 0.005, JP, Weathered(P, Fc, F, O.A1 + 0.15, Z0, Varied(C, 1.02, 0.0)));
					}
					else if (std::fabs(Z0 - O.Z1) < 1e-6 && O.Arch <= 0.0) // (a round head's voussoirs: OpeningNear's)
					{
						const Col LC = Weathered(P, Fc, F, 0.5 * (O.A0 + O.A1), Z0, C);
						if (O.What == 1)
						{
							// (the door's lintel in three, its keystone proud and standing above the course)
							const double M = 0.5 * (O.A0 + O.A1);
							Block(B, Fc, O.A0 - LintelOver + 0.008, M - 0.15, Z0 + 0.005, Z1 - 0.005, LP, LC);
							Block(B, Fc, M + 0.15, O.A1 + LintelOver - 0.008, Z0 + 0.005, Z1 - 0.005, LP, Varied(LC, 0.97, 0.0));
							Block(B, Fc, M - 0.15, M + 0.15, Z0 - 0.04, Z1 + 0.06, LP + 0.017, Varied(LC, 1.04, 0.0));
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
						else Block(B, Fc, O.A0 - LintelOf(O) + 0.008, O.A1 + LintelOf(O) - 0.008, Z0 + 0.005, Z1 - 0.005, LP, LC);
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
					// (a rendered wall's, seen only where the render came away: the rubble's old bedding mortar, browner and
					// dustier than a pointed wall's, so its stones stand out of it)
					const Col MortarHere = P.Finish == 2 ? Mixed(P.K.Mortar, Rgb(128, 116, 98), 0.45) : P.K.Mortar;
					auto MC = [&](double A, double Z) { return Weathered(P, Fc, F, A, Z, Varied(MortarHere, 0.96 + 0.06 * Noise(A / 0.7, Z / 0.7, P.Key ^ 0x30), 0.0), true); };
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
						Col Core = Mixed(P.K.Mortar, Rgb(120, 110, 96), 0.45 + 0.3 * Noise01(AM / 0.3, 1.0, P.Key ^ 0xC0E));
						if (P.Finish == 2) Core = Scaled(Core, 0.86); // (a rendered wall's core, between its top's stones)
						B.Quad1(At(Fc, A0, T0, 0.0), At(Fc, A1, T0, 0.0), At(Fc, A1, T0, -P.Thick), At(Fc, A0, T0, -P.Thick), P.Hurt == 3 ? Mixed(Core, P.K.Soot, 0.3) : Core, Up);
						if (P.Finish == 2) CapStones(B, P, Fc, F, A0, A1, T0);
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
			// (a rendered wall's strikes: each a ragged pit in the render, its broken rim paler, its middle dark where it went
			// in; none where the render has come away, nor on the dressed work)
			if (bRender)
				for (const Plan::Pock& Q : P.Pocks)
				{
					if (Q.Face != F || InSpall(P, F, Q.A, Q.Z)) continue;
					bool bOn = false;
					for (size_t I = 0; I + 1 < P.Course.size(); ++I)
						if (Q.Z >= P.Course[I] && Q.Z < P.Course[I + 1])
						{
							bOn = true;
							for (const auto& [R0, R1] : DressedIn(P, Fc, F, I))
								if (Q.A + Q.R > R0 && Q.A - Q.R < R1) bOn = false;
							break;
						}
					if (!bOn) continue;
					const Col Own = RenderAt(P, Fc, F, Q.A, Q.Z);
					// (the rim fresh lime, uneven, here broad, there hardly broken; the pit off its middle, dark where it went in)
					const double Fresh = 0.2 + 0.2 * Unit(Mix(Q.Key ^ 0x31));
					const Col Rim = Mixed(Own, Rgb(226, 221, 206), Fresh), Pit = Mixed(Rgb(74, 68, 58), Own, 0.2 + 0.15 * Unit(Mix(Q.Key ^ 0x32)));
					constexpr int NV = 9;
					double Rr[NV];
					for (int J = 0; J < NV; ++J) Rr[J] = Q.R * (0.5 + 0.8 * Unit(Hash(Q.Key, static_cast<uint64_t>(J))));
					const double OffA = 0.18 * Q.R * (Unit(Mix(Q.Key ^ 0x33)) - 0.5), OffZ = 0.18 * Q.R * (Unit(Mix(Q.Key ^ 0x34)) - 0.5);
					for (int Ring = 0; Ring < 2; ++Ring)
					{
						const double D = RenderD + (Ring == 0 ? 0.0012 : 0.0018), K = Ring == 0 ? 1.0 : 0.42 + 0.12 * Unit(Mix(Q.Key ^ 0x35));
						const double MA = Q.A + (Ring == 0 ? 0.0 : OffA), MZ = Q.Z + (Ring == 0 ? 0.0 : OffZ);
						const Col C = Ring == 0 ? Rim : Pit;
						const uint32_t IM = B.Vert(At(Fc, MA, MZ, D), Fc.N, C);
						uint32_t Ix[NV];
						for (int J = 0; J < NV; ++J)
						{
							const double Ang = 2.0 * Pi * J / NV + 0.4 * Unit(Q.Key);
							// (its rim's outer edge fading into the render round it)
							const Col CE = Ring == 0 ? Mixed(Rim, Own, 0.45) : Pit;
							Ix[J] = B.Vert(At(Fc, MA + K * Rr[J] * std::cos(Ang), MZ + K * Rr[J] * (Ring == 0 ? 1.0 : 0.8) * std::sin(Ang), D), Fc.N, CE);
						}
						for (int J = 0; J < NV; ++J) B.Tri(IM, Ix[J], Ix[(J + 1) % NV], Fc.N);
					}
				}
			// (its cracks: a dark line, dirt in it, each segment's ends mitred to the next's)
			if (bRender)
				for (const Plan::Crack& Cr : P.Cracks)
				{
					if (Cr.Face != F) continue;
					const size_t N = Cr.Pts.size();
					std::vector<std::pair<double, double>> Nrm(N);
					for (size_t J = 0; J < N; ++J)
					{
						const auto& Pa = Cr.Pts[J == 0 ? 0 : J - 1];
						const auto& Pb = Cr.Pts[J + 1 < N ? J + 1 : N - 1];
						const double DA = Pb.first - Pa.first, DZ = Pb.second - Pa.second, L = std::max(1e-6, std::hypot(DA, DZ));
						// (narrowing to nothing at its ends)
						const double Half = 0.5 * Cr.W * (J == 0 || J + 1 == N ? 0.25 : 0.7 + 0.6 * Unit(Hash(P.Key ^ 0xC4AD, static_cast<uint64_t>(J), static_cast<uint64_t>(std::llround(Cr.Pts[0].first * 100.0)))));
						Nrm[J] = { -DZ / L * Half, DA / L * Half };
					}
					for (size_t J = 0; J + 1 < N; ++J)
					{
						const auto& [A0, Z0] = Cr.Pts[J];
						const auto& [A1, Z1] = Cr.Pts[J + 1];
						const Col C0 = Mixed(RenderAt(P, Fc, F, A0, Z0), Rgb(62, 58, 50), 0.62), C1 = Mixed(RenderAt(P, Fc, F, A1, Z1), Rgb(62, 58, 50), 0.62);
						const double D = RenderD + 0.0009;
						B.Quad(At(Fc, A0 - Nrm[J].first, Z0 - Nrm[J].second, D), At(Fc, A0 + Nrm[J].first, Z0 + Nrm[J].second, D),
							At(Fc, A1 + Nrm[J + 1].first, Z1 + Nrm[J + 1].second, D), At(Fc, A1 - Nrm[J + 1].first, Z1 - Nrm[J + 1].second, D), C0, C0, C1, C1, Fc.N);
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
						const V3 C = At(Fc, A, Z, bRender ? 0.042 : 0.03);
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
				const double Yaw_ = R.Range(0.0, Pi), C_ = R.Range(0.5, 0.8), B_ = R.Range(0.7, 1.0), A_ = R.Range(0.9, 1.3);
				HayMound(B, P, V3{ HX, HY, 0.02 }, A_, B_, C_, Yaw_, Hash(K0, 0x4B, static_cast<uint64_t>(std::llround((L.X + 50.0) * 10.0))), 0.72 * FloorK(HX, HY));
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

		/** A shop's trade painted along its fascia: one word or two of the bourg's trades, in plain capitals, as a sign
		 * writer drew them, Hgt high, centred on (AM, ZM) at D out from the face. Each letter in strokes on a grid two wide
		 * and four high. */
		void Lettering(Pen& B, const Face& Fc, const char* Word, double AM, double ZM, double Hgt, double D, const Col& C)
		{
			struct Glyph
			{
				char Ch;
				const char* Strokes; // (x0 y0 x1 y1, each stroke four numbers)
			};
			static const Glyph Font[] = {
				{ 'A', "0 0 0 3  0 3 1 4  1 4 2 3  2 3 2 0  0 1.8 2 1.8" },
				{ 'B', "0 0 0 4  0 4 1.4 4  1.4 4 2 3.4  2 3.4 2 2.6  2 2.6 1.4 2  0 2 1.4 2  1.4 2 2 1.4  2 1.4 2 0.6  2 0.6 1.4 0  1.4 0 0 0" },
				{ 'C', "2 4 0.6 4  0.6 4 0 3.4  0 3.4 0 0.6  0 0.6 0.6 0  0.6 0 2 0" },
				{ 'E', "2 4 0 4  0 4 0 0  0 0 2 0  0 2 1.5 2" },
				{ 'F', "2 4 0 4  0 4 0 0  0 2 1.5 2" },
				{ 'G', "2 4 0.6 4  0.6 4 0 3.4  0 3.4 0 0.6  0 0.6 0.6 0  0.6 0 2 0  2 0 2 1.8  2 1.8 1.1 1.8" },
				{ 'H', "0 0 0 4  2 0 2 4  0 2 2 2" },
				{ 'I', "1 0 1 4  0.4 4 1.6 4  0.4 0 1.6 0" },
				{ 'L', "0 4 0 0  0 0 2 0" },
				{ 'M', "0 0 0 4  0 4 1 2  1 2 2 4  2 4 2 0" },
				{ 'N', "0 0 0 4  0 4 2 0  2 0 2 4" },
				{ 'O', "0.6 0 1.4 0  1.4 0 2 0.6  2 0.6 2 3.4  2 3.4 1.4 4  1.4 4 0.6 4  0.6 4 0 3.4  0 3.4 0 0.6  0 0.6 0.6 0" },
				{ 'P', "0 0 0 4  0 4 1.4 4  1.4 4 2 3.4  2 3.4 2 2.6  2 2.6 1.4 2  1.4 2 0 2" },
				{ 'Q', "0.6 0 1.4 0  1.4 0 2 0.6  2 0.6 2 3.4  2 3.4 1.4 4  1.4 4 0.6 4  0.6 4 0 3.4  0 3.4 0 0.6  0 0.6 0.6 0  1.2 0.9 2.1 -0.2" },
				{ 'R', "0 0 0 4  0 4 1.4 4  1.4 4 2 3.4  2 3.4 2 2.6  2 2.6 1.4 2  1.4 2 0 2  1 2 2 0" },
				{ 'T', "0 4 2 4  1 4 1 0" },
				{ 'U', "0 4 0 0.6  0 0.6 0.6 0  0.6 0 1.4 0  1.4 0 2 0.6  2 0.6 2 4" },
			};
			const double U = Hgt / 4.0, Adv = Hgt * 0.72, Th = Hgt * 0.13;
			const int N = static_cast<int>(std::strlen(Word));
			double A = AM - 0.5 * (N * Adv - Hgt * 0.22);
			const double Z = ZM - 0.5 * Hgt;
			for (int I = 0; I < N; ++I, A += Adv)
			{
				const Glyph* G = nullptr;
				for (const Glyph& Gl : Font)
					if (Gl.Ch == Word[I]) G = &Gl;
				if (!G) continue;
				double V[4];
				int K = 0;
				const char* S = G->Strokes;
				char* End = nullptr;
				for (double X = std::strtod(S, &End); End != S; X = std::strtod(S, &End))
				{
					S = End;
					V[K++] = X;
					if (K < 4) continue;
					K = 0;
					const double A0 = A + V[0] * U, Z0 = Z + V[1] * U, A1 = A + V[2] * U, Z1 = Z + V[3] * U;
					const double L = std::max(1e-6, std::hypot(A1 - A0, Z1 - Z0)), DA = (A1 - A0) / L * 0.5 * Th, DZ = (Z1 - Z0) / L * 0.5 * Th;
					B.Quad1(At(Fc, A0 - DA + DZ, Z0 - DZ - DA, D), At(Fc, A1 + DA + DZ, Z1 + DZ - DA, D), At(Fc, A1 + DA - DZ, Z1 + DZ + DA, D), At(Fc, A0 - DA - DZ, Z0 - DZ + DA, D), C, Fc.N);
				}
			}
		}

		/** A shop's trade on its fascia (OpShop, H its hash): the longest of the house's choices that fits along it, and
		 * how high its capitals stand; none where none fits. */
		const char* TradeOf(const Opening& O, uint64_t H, double& Hgt)
		{
			static const char* const Trades[] = { "BOULANGERIE", "EPICERIE", "BOUCHERIE", "MERCERIE", "QUINCAILLERIE", "CAFE TABAC", "PHARMACIE", "CHARCUTERIE", "HOTEL", "CAFE", "TABAC" };
			constexpr int NT = static_cast<int>(sizeof(Trades) / sizeof(Trades[0]));
			// (the fascia 30 cm past the opening each side, its lined border 4.7 cm in; 15 cm clear inside the border each end)
			const double Room = O.A1 - O.A0 + 0.6 - 2.0 * 0.047 - 0.3, Hgt0 = std::min(0.24, ShopFascia(O) - O.Z1 - 0.04 - 2.0 * 0.047 - 0.08);
			const int First = static_cast<int>(Unit(Mix(H ^ 0x62)) * NT);
			for (int J = 0; J < NT; ++J)
			{
				const char* Wd = Trades[(First + J) % NT];
				const double Need = Room / (0.72 * static_cast<double>(std::strlen(Wd)) - 0.22);
				if (Need >= 0.16)
				{
					Hgt = std::min(Hgt0, Need);
					return Wd;
				}
			}
			return nullptr;
		}
		/** Its trade's paint: the fascia's gilt, or a cream. */
		Col InkOf(uint64_t H, const Col& Gilt) { return Unit(Mix(H ^ 0x63)) < 0.55 ? Gilt : Mixed(Rgb(228, 220, 196), Rgb(150, 146, 136), 0.2 * Unit(Mix(H ^ 0x64))); }
		/** A display light's glass, the dim shop through its foot, the sky in it higher up (V 0 at its foot to 1 at its
		 * head), as the near level's averages it across. */
		Col ShopGlass(const Plan& P, double V) { return Mixed(Rgb(46, 42, 38), Mixed(P.K.Glass, P.K.Sky, 0.6), std::pow(Smooth(0.05, 1.0, V), 1.3) * 0.9 + 0.04); }

		/**
		 * A village shop's front, near (OpShop): applied to the wall round its opening, as the bourg's shops were, two
		 * pilasters on their plinths with a sunk panel up each and a console at its head, a fascia over the opening with a
		 * lined border and its moulding, a cornice capped in zinc; in the opening its frame, display windows over a
		 * panelled stall riser, their mullions and their top lights, the glass giving back the sky; its door at one end or
		 * the middle, set back, glazed over a panel, a step before it. Its boards up over the glass, or not; its glass
		 * broken where the blast took it, gone where the house was wrecked; burnt, its timbers charred, its fascia part
		 * gone, no glass.
		 */
		void ShopNear(Pen& B, const Plan& P, const Opening& O, const Face& Fc, int F)
		{
			const uint64_t H = Hash(P.Key, 0x5401, static_cast<uint64_t>(std::llround(O.A0 * 20.0)));
			const double TopHere = std::min(TopOf(P, Fc, F, O.A0 - 0.3), TopOf(P, Fc, F, O.A1 + 0.3));
			if (TopHere < 1.0) return;
			const bool bBurnt = O.bCharred;
			const double ZT = std::min(O.Z1, TopHere), W = O.A1 - O.A0, AM = 0.5 * (O.A0 + O.A1);
			auto Paint = [&](double A, double Z, double K) {
				if (bBurnt) return Varied(P.K.Char, K * (0.85 + 0.35 * Noise01(A / 0.3, Z / 0.4, H)), 0.0);
				// (the paint faded and worn in patches toward its grey undercoat)
				return Scaled(Mixed(P.K.Shop, Rgb(150, 146, 136), 0.22 * Smooth(0.55, 0.85, Noise01(A / 0.45, Z / 0.6, H ^ 0x5A))), K);
			};
			const V3 T = Fc.T, N = Fc.N;
			// (a box on the face: A0 to A1, Z0 to Z1, D0 to D1 out from it; no back)
			auto Slab = [&](double A0, double A1, double Z0, double Z1, double D0, double D1, const Col& C, const Col* Top = nullptr) {
				B.Box(At(Fc, 0.5 * (A0 + A1), 0.5 * (Z0 + Z1), 0.5 * (D0 + D1)), T * (0.5 * (A1 - A0)), N * (0.5 * (D1 - D0)), Up * (0.5 * (Z1 - Z0)), C, 4, Top);
			};
			// The pilasters.
			for (int S = -1; S <= 1; S += 2)
			{
				const double PA0 = S < 0 ? O.A0 - ShopPilaster : O.A1 - 0.02, PA1 = S < 0 ? O.A0 + 0.02 : O.A1 + ShopPilaster;
				const double Hi = std::min(ZT + 0.04, TopOf(P, Fc, F, 0.5 * (PA0 + PA1)) - 0.05);
				if (Hi < 0.4) continue;
				Slab(PA0 - 0.02, PA1 + 0.02, 0.0, 0.36, 0.0, 0.11, Paint(PA0, 0.2, 0.82));
				Slab(PA0, PA1, 0.36, Hi, 0.0, 0.09, Paint(PA0, 1.4, 1.0));
				if (Hi > 1.4)
					B.Quad1(At(Fc, PA0 + 0.045, 0.62, 0.0905), At(Fc, PA1 - 0.045, 0.62, 0.0905), At(Fc, PA1 - 0.045, std::min(Hi, ZT) - 0.48, 0.0905), At(Fc, PA0 + 0.045, std::min(Hi, ZT) - 0.48, 0.0905), Paint(PA0, 1.4, 0.84), N);
				if (Hi >= ZT) Slab(PA0 - 0.03, PA1 + 0.03, ZT - 0.3, ZT + 0.04, 0.0, 0.125, Paint(PA0, ZT, 0.94));
			}
			// The fascia, its moulding under it, its lined border and its cornice (burnt: part of it, ragged, the rest gone).
			if (TopHere > ShopFascia(O) + 0.1)
			{
				double FA0 = O.A0 - 0.3, FA1 = O.A1 + 0.3;
				if (bBurnt)
				{
					const double Keep = 0.35 + 0.4 * Unit(Mix(H ^ 0xFA));
					if (Unit(Mix(H ^ 0xFB)) < 0.5) FA1 = FA0 + Keep * (FA1 - FA0);
					else FA0 = FA1 - Keep * (FA1 - FA0);
				}
				const double Z0 = ZT + 0.04, Z1 = ShopFascia(O);
				Slab(FA0, FA1, Z0, Z1, 0.0, 0.1, Paint(AM, Z0, 1.0));
				Slab(FA0 - 0.02, FA1 + 0.02, ZT, Z0 + 0.01, 0.0, 0.12, Paint(AM, ZT, 0.86));
				if (!bBurnt)
				{
					const Col Zn = Varied(P.K.Zinc, 0.92, 0.0);
					Slab(FA0 - 0.06, FA1 + 0.06, Z1, Z1 + 0.08, 0.0, 0.17, Paint(AM, Z1, 0.95), &Zn);
					const Col Gilt = Mixed(Rgb(196, 180, 128), Rgb(150, 146, 136), 0.3 * Unit(Mix(H ^ 0x61)));
					const double D = 0.1008, In = 0.035, L = 0.012;
					B.Quad1(At(Fc, FA0 + In, Z0 + In, D), At(Fc, FA1 - In, Z0 + In, D), At(Fc, FA1 - In, Z0 + In + L, D), At(Fc, FA0 + In, Z0 + In + L, D), Gilt, N);
					B.Quad1(At(Fc, FA0 + In, Z1 - In - L, D), At(Fc, FA1 - In, Z1 - In - L, D), At(Fc, FA1 - In, Z1 - In, D), At(Fc, FA0 + In, Z1 - In, D), Gilt, N);
					B.Quad1(At(Fc, FA0 + In, Z0 + In, D), At(Fc, FA0 + In + L, Z0 + In, D), At(Fc, FA0 + In + L, Z1 - In, D), At(Fc, FA0 + In, Z1 - In, D), Gilt, N);
					B.Quad1(At(Fc, FA1 - In - L, Z0 + In, D), At(Fc, FA1 - In, Z0 + In, D), At(Fc, FA1 - In, Z1 - In, D), At(Fc, FA1 - In - L, Z1 - In, D), Gilt, N);
					// (its trade painted along it, in gilt or in cream)
					double Hgt = 0.0;
					if (const char* Word = TradeOf(O, H, Hgt)) Lettering(B, Fc, Word, AM, 0.5 * (Z0 + Z1), Hgt, 0.1012, InkOf(H, Gilt));
				}
				else
				{
					// (its burnt end, ragged)
					const double E = Unit(Mix(H ^ 0xFB)) < 0.5 ? FA1 : FA0, Dir = E == FA1 ? 1.0 : -1.0;
					B.Tri1(At(Fc, E, Z0, 0.1), At(Fc, E, Z1, 0.1), At(Fc, E + Dir * 0.22, Z0 + 0.12, 0.1), P.K.Char, N);
				}
			}
			// Its door: at one end, or the middle of a wide front.
			const double DW = 0.92, In0 = O.A0 + 0.09, In1 = O.A1 - 0.09;
			const bool bMidDoor = W > 3.6;
			const int DSide = Unit(Mix(H ^ 0xD0)) < 0.5 ? -1 : 1;
			const double DA0 = bMidDoor ? AM - 0.5 * DW : DSide < 0 ? In0 : In1 - DW, DA1 = DA0 + DW;
			const Col Fr = bBurnt ? P.K.Char : Paint(AM, 1.0, 0.9);
			// The frame: its posts, its head rail, its transom.
			Slab(O.A0, In0, 0.0, ZT, -0.1, -0.01, Fr);
			Slab(In1, O.A1, 0.0, ZT, -0.1, -0.01, Fr);
			Slab(O.A0, O.A1, ZT - 0.07, ZT, -0.1, -0.01, Fr);
			if (!bBurnt) Slab(In0, In1, ZT - 0.34, ZT - 0.27, -0.1, -0.02, Fr);
			for (double X : { DA0 - 0.07, DA1 })
				if (X > In0 + 0.01 && X + 0.07 < In1 - 0.01) Slab(X, X + 0.07, 0.0, ZT - 0.27, -0.1, -0.01, Fr);
			auto GlassAt = [&](double M, bool bTop) { return Mixed(P.K.Glass, P.K.Sky, std::min(0.9, M + (bTop ? 0.2 : 0.0))); };
			// (a light of glass, Z0 to Z1, A0 to A1, at D: whole, cracked, or gone)
			int Light = 0;
			auto Pane = [&](double A0, double A1, double Z0, double Z1, double D, double M) {
				const uint64_t LH = Mix(H + static_cast<uint64_t>(++Light) * 0x9E37);
				if (!O.bGlass || bBurnt)
				{
					if (!(P.Hurt >= 2)) B.Quad1(At(Fc, A0, Z0, D), At(Fc, A1, Z0, D), At(Fc, A1, Z1, D), At(Fc, A0, Z1, D), Rgb(16, 15, 14), N);
					return;
				}
				const bool bBroke = static_cast<int>(Unit(LH) * 5.0) < O.Cracked;
				if (bBroke)
				{
					B.Quad1(At(Fc, A0, Z0, D), At(Fc, A1, Z0, D), At(Fc, A1, Z1, D), At(Fc, A0, Z1, D), Rgb(16, 15, 14), N);
					B.Tri1(At(Fc, A0, Z1, D + 0.003), At(Fc, A0 + 0.55 * (A1 - A0), Z1, D + 0.003), At(Fc, A0, Z0 + 0.45 * (Z1 - Z0), D + 0.003), GlassAt(M, false), N);
					B.Tri1(At(Fc, A1, Z0, D + 0.003), At(Fc, A1, Z0 + 0.3 * (Z1 - Z0), D + 0.003), At(Fc, A1 - 0.35 * (A1 - A0), Z0, D + 0.003), GlassAt(M, false), N);
					return;
				}
				if (Z1 - Z0 < 0.6)
				{
					B.Quad(At(Fc, A0, Z0, D), At(Fc, A1, Z0, D), At(Fc, A1, Z1, D), At(Fc, A0, Z1, D), GlassAt(M, false), GlassAt(M, false), GlassAt(M, true), GlassAt(M, true), N);
					return;
				}
				// (a display light: the dim shop seen through its foot, the street's sky in it above, and a sheen slanting
				// across it where the light catches the glass)
				const Col Within = Varied(Rgb(46, 42, 38), 0.9 + 0.2 * Unit(Mix(LH ^ 1)), 0.0), Above = Mixed(P.K.Glass, P.K.Sky, std::min(0.85, M + 0.25));
				const double Band = 0.55 + 0.45 * Unit(Mix(LH ^ 2)), Lean = 0.5 + 0.4 * Unit(Mix(LH ^ 3));
				auto Shine = [&](double U, double V) {
					Col C = Mixed(Within, Above, std::pow(Smooth(0.05, 1.0, V), 1.3) * 0.9);
					const double Dg = U * Lean + V * 0.7 - Band;
					return Mixed(C, Scaled(P.K.Sky, 1.04), 0.34 * std::exp(-Sq(Dg / 0.12)));
				};
				constexpr int NU = 4, NV = 5;
				for (int I = 0; I < NU; ++I)
					for (int J = 0; J < NV; ++J)
					{
						const double U0 = double(I) / NU, U1 = double(I + 1) / NU, V0 = double(J) / NV, V1 = double(J + 1) / NV;
						const double X0 = Lerp(A0, A1, U0), X1 = Lerp(A0, A1, U1), Y0 = Lerp(Z0, Z1, V0), Y1 = Lerp(Z0, Z1, V1);
						B.Quad(At(Fc, X0, Y0, D), At(Fc, X1, Y0, D), At(Fc, X1, Y1, D), At(Fc, X0, Y1, D), Shine(U0, V0), Shine(U1, V0), Shine(U1, V1), Shine(U0, V1), N);
					}
			};
			// The display windows either side of the door.
			std::vector<std::pair<double, double>> Spans;
			if (DA0 - 0.07 > In0 + 0.3) Spans.push_back({ In0, DA0 - 0.07 });
			if (DA1 + 0.07 < In1 - 0.3) Spans.push_back({ DA1 + 0.07, In1 });
			for (const auto& [S0, S1] : Spans)
			{
				// the stall riser, a raised panel in it, the cill over it
				Slab(S0, S1, 0.0, 0.62, -0.09, -0.03, Paint(S0, 0.3, 0.86));
				if (!bBurnt) B.Quad1(At(Fc, S0 + 0.08, 0.12, -0.028), At(Fc, S1 - 0.08, 0.12, -0.028), At(Fc, S1 - 0.08, 0.5, -0.028), At(Fc, S0 + 0.08, 0.5, -0.028), Paint(S0, 0.3, 1.02), N);
				const Col CillTop = Paint(S0, 0.65, 1.1);
				Slab(S0 - 0.01, S1 + 0.01, 0.62, 0.69, -0.1, 0.0, Paint(S0, 0.65, 0.92), &CillTop);
				const int NL = std::max(1, static_cast<int>(std::lround((S1 - S0) / 0.85)));
				for (int K = 1; K < NL; ++K)
				{
					const double X = S0 + (S1 - S0) * K / NL;
					if (!bBurnt || Unit(Mix(H + static_cast<uint64_t>(K))) < 0.5) Slab(X - 0.025, X + 0.025, 0.69, ZT - 0.27, -0.1, -0.03, Fr);
				}
				for (int K = 0; K < NL; ++K)
				{
					const double X0 = S0 + (S1 - S0) * K / NL + (K > 0 ? 0.025 : 0.0), X1 = S0 + (S1 - S0) * (K + 1) / NL - (K + 1 < NL ? 0.025 : 0.0);
					const double M = 0.24 + 0.24 * Unit(Mix(H ^ (0x6A + static_cast<uint64_t>(K))));
					Pane(X0, X1, 0.69, ZT - 0.34, -0.075, M);
					// (its top lights, two over each, a bar between)
					for (int J = 0; J < 2; ++J)
					{
						const double Y0 = X0 + (X1 - X0) * J / 2.0 + (J > 0 ? 0.012 : 0.0), Y1 = X0 + (X1 - X0) * (J + 1) / 2.0 - (J == 0 ? 0.012 : 0.0);
						Pane(Y0, Y1, ZT - 0.27, ZT - 0.07, -0.075, M + 0.1);
					}
					if (!bBurnt) Slab(0.5 * (X0 + X1) - 0.012, 0.5 * (X0 + X1) + 0.012, ZT - 0.27, ZT - 0.07, -0.09, -0.05, Fr);
				}
				// (its shutter boards up over the glass, two iron bars across them)
				if (O.Shutters == 1 && !bBurnt)
				{
					const int NB = std::max(2, static_cast<int>((S1 - S0) / 0.15));
					for (int K = 0; K < NB; ++K)
					{
						const double X0 = S0 + (S1 - S0) * K / NB + 0.004, X1 = S0 + (S1 - S0) * (K + 1) / NB - 0.004;
						const Col C = Varied(Mixed(P.K.Shop, Rgb(120, 112, 98), 0.35), 0.86 + 0.2 * Unit(Mix(H ^ (0xB0 + static_cast<uint64_t>(K)))), 0.0);
						B.Quad(At(Fc, X0, 0.69, -0.002), At(Fc, X1, 0.69, -0.002), At(Fc, X1, ZT - 0.34, -0.002), At(Fc, X0, ZT - 0.34, -0.002), Scaled(C, 0.9), Scaled(C, 0.9), C, C, N);
					}
					B.Quad1(At(Fc, S0, 0.69, -0.006), At(Fc, S1, 0.69, -0.006), At(Fc, S1, ZT - 0.34, -0.006), At(Fc, S0, ZT - 0.34, -0.006), Rgb(28, 26, 24), N);
					for (double Z : { 1.15, ZT - 0.75 }) Slab(S0 - 0.02, S1 + 0.02, Z - 0.025, Z + 0.025, -0.002, 0.016, P.K.Iron);
				}
			}
			// The door, set back in its own reveal, the transom light over it.
			{
				const double DD = -0.32, DT = ZT - 0.34;
				const Col Rv = bBurnt ? P.K.Char : Paint(DA0, 1.0, 0.72);
				B.Quad1(At(Fc, DA0, 0.0, -0.07), At(Fc, DA0, 0.0, DD), At(Fc, DA0, DT, DD), At(Fc, DA0, DT, -0.07), Rv, T);
				B.Quad1(At(Fc, DA1, 0.0, -0.07), At(Fc, DA1, 0.0, DD), At(Fc, DA1, DT, DD), At(Fc, DA1, DT, -0.07), Rv, T * -1.0);
				B.Quad1(At(Fc, DA0, DT, -0.07), At(Fc, DA1, DT, -0.07), At(Fc, DA1, DT, DD), At(Fc, DA0, DT, DD), Scaled(Rv, 0.7), Up * -1.0);
				B.Quad1(At(Fc, DA0, 0.02, 0.0), At(Fc, DA1, 0.02, 0.0), At(Fc, DA1, 0.02, DD), At(Fc, DA0, 0.02, DD), Rgb(128, 118, 104), Up);
				Pane(DA0, DA1, ZT - 0.27, ZT - 0.07, -0.075, 0.42);
				if (O.bLeaf && !bBurnt)
				{
					const Col Lf = Paint(DA0, 0.5, 0.95);
					Slab(DA0, DA1, 0.02, DT, DD - 0.05, DD, Scaled(Lf, 0.9));
					B.Quad1(At(Fc, DA0 + 0.1, 0.16, DD + 0.003), At(Fc, DA1 - 0.1, 0.16, DD + 0.003), At(Fc, DA1 - 0.1, 0.86, DD + 0.003), At(Fc, DA0 + 0.1, 0.86, DD + 0.003), Lf, N);
					const double GM = 0.5 * (DA0 + DA1);
					Pane(DA0 + 0.1, GM - 0.015, 1.0, DT - 0.12, DD + 0.002, 0.3);
					Pane(GM + 0.015, DA1 - 0.1, 1.0, DT - 0.12, DD + 0.002, 0.34);
					Slab(DA1 - 0.16, DA1 - 0.12, 0.98, 1.06, DD, DD + 0.04, Rgb(150, 120, 62));
				}
				else if (P.Hurt < 2) B.Quad1(At(Fc, DA0, 0.02, DD), At(Fc, DA1, 0.02, DD), At(Fc, DA1, DT, DD), At(Fc, DA0, DT, DD), Rgb(16, 15, 14), N);
				// (the step before it)
				B.Box(At(Fc, 0.5 * (DA0 + DA1), -0.6, 0.22), T * (0.5 * DW + 0.12), N * 0.2, Up * 0.44, Weathered(P, Fc, F, 0.5 * (DA0 + DA1), 0.0, Varied(P.K.Dressed, 0.86, 0.0)), 0);
			}
		}

		/** A village shop's front, plain: the middle's pilasters, fascia and cornice as slabs, its riser, its glass or its
		 * boards, its door; far off its fascia, its pilasters and its glass painted on the wall. */
		void ShopPlain(Pen& B, const Plan& P, const Opening& O, const Face& Fc, int F, bool bMid)
		{
			const double TopHere = std::min(TopOf(P, Fc, F, O.A0 - 0.3), TopOf(P, Fc, F, O.A1 + 0.3));
			if (TopHere < 1.0) return;
			const bool bBurnt = O.bCharred;
			const double ZT = std::min(O.Z1, TopHere), AM = 0.5 * (O.A0 + O.A1);
			const Col Paint = bBurnt ? P.K.Char : Mixed(P.K.Shop, Rgb(150, 146, 136), 0.08);
			const Col Glass = Scaled(Mixed(ShopGlass(P, 0.1), ShopGlass(P, 0.9), 0.5), 0.84), Dark = Rgb(16, 15, 14);
			const bool bLit = O.Shutters != 1 && O.bGlass && !bBurnt;
			const Col Fill = O.Shutters == 1 ? Varied(Mixed(P.K.Shop, Rgb(120, 112, 98), 0.35), 0.95, 0.0) : bLit ? Glass : Dark;
			auto Flat = [&](double A0, double A1, double Z0, double Z1, double D, const Col& C) {
				B.Quad1(At(Fc, A0, Z0, D), At(Fc, A1, Z0, D), At(Fc, A1, Z1, D), At(Fc, A0, Z1, D), C, Fc.N);
			};
			auto Slab = [&](double A0, double A1, double Z0, double Z1, double D1, const Col& C) {
				B.Box(At(Fc, 0.5 * (A0 + A1), 0.5 * (Z0 + Z1), 0.5 * D1), Fc.T * (0.5 * (A1 - A0)), Fc.N * (0.5 * D1), Up * (0.5 * (Z1 - Z0)), C, 4 | 16);
			};
			const bool bFascia = TopHere > ShopFascia(O) + 0.1;
			const uint64_t H = Hash(P.Key, 0x5401, static_cast<uint64_t>(std::llround(O.A0 * 20.0)));
			const double In0 = O.A0 + 0.09, In1 = O.A1 - 0.09, DW = 0.92;
			const double DA0 = O.A1 - O.A0 > 3.6 ? AM - 0.5 * DW : Unit(Mix(H ^ 0xD0)) < 0.5 ? In0 : In1 - DW;
			// (its trade, too small to read: a paler band along the fascia, as the eye mixes the letters with it)
			auto Trade = [&](double D) {
				double Hgt = 0.0;
				if (bFascia && !bBurnt)
					if (const char* Word = TradeOf(O, H, Hgt))
					{
						const double WL = 0.5 * (std::strlen(Word) * 0.72 - 0.22) * Hgt, ZF = 0.5 * (ZT + 0.04 + ShopFascia(O));
						const Col Gilt = Mixed(Rgb(196, 180, 128), Rgb(150, 146, 136), 0.3 * Unit(Mix(H ^ 0x61)));
						Flat(AM - WL, AM + WL, ZF - 0.5 * Hgt, ZF + 0.5 * Hgt, D, Mixed(Paint, InkOf(H, Gilt), 0.38));
					}
			};
			if (!bMid)
			{
				// (far off, painted on the wall (the art director's village round 1: not a dark slab in a frame, which read
				// as a garage's door): its stall riser, its glass, two mullions, its door, its fascia and its trade)
				Flat(O.A0, O.A1, 0.0, ZT, 0.012, bBurnt || O.Shutters == 1 ? Scaled(Fill, 0.9) : Fill);
				if (!bBurnt)
				{
					Flat(O.A0, O.A1, 0.0, 0.62, 0.016, Scaled(Paint, 0.86));
					Flat(O.A0, O.A1, ZT - 0.34, ZT, 0.016, Scaled(Paint, 0.9));
					if (O.Shutters != 1)
						for (const double T : { 1.0 / 3.0, 2.0 / 3.0 })
						{
							const double X = O.A0 + (O.A1 - O.A0) * T;
							if (X < DA0 - 0.08 || X > DA0 + DW + 0.08) Flat(X - 0.045, X + 0.045, 0.62, ZT - 0.34, 0.016, Scaled(Paint, 0.9));
						}
					Flat(DA0 - 0.05, DA0 + DW + 0.05, 0.0, ZT - 0.34, 0.02, Scaled(Paint, 0.9));
					Flat(DA0, DA0 + DW, 0.05, ZT - 0.4, 0.022, O.bLeaf ? Scaled(Paint, 0.8) : Dark);
				}
				Flat(O.A0 - ShopPilaster, O.A0, 0.0, ZT, 0.03, Paint);
				Flat(O.A1, O.A1 + ShopPilaster, 0.0, ZT, 0.03, Paint);
				if (bFascia) Flat(O.A0 - 0.3, O.A1 + 0.3, ZT, ShopFascia(O) + 0.08, 0.03, Scaled(Paint, 0.95));
				Trade(0.032);
				return;
			}
			Slab(O.A0 - ShopPilaster, O.A0 + 0.02, 0.0, std::min(ZT + 0.04, TopHere), 0.09, Paint);
			Slab(O.A1 - 0.02, O.A1 + ShopPilaster, 0.0, std::min(ZT + 0.04, TopHere), 0.09, Paint);
			if (bFascia)
			{
				double FA0 = O.A0 - 0.3, FA1 = O.A1 + 0.3;
				if (bBurnt) FA1 = FA0 + 0.55 * (FA1 - FA0);
				Slab(FA0, FA1, ZT, ShopFascia(O), 0.1, Paint);
				if (!bBurnt) Slab(FA0 - 0.06, FA1 + 0.06, ShopFascia(O), ShopFascia(O) + 0.08, 0.17, Scaled(Paint, 0.95));
			}
			Trade(0.1012);
			Flat(O.A0, O.A1, 0.0, 0.62, -0.03, Scaled(Paint, 0.86));
			if (bLit)
			{
				// (its glass dim at its foot, the sky in it higher, as the near level's)
				const double G0 = 0.62, G1 = ZT - 0.34, GM = 0.5 * (G0 + G1);
				// (a shade darker than the near level's light at its middle: its frame, its bars and its door darken it there)
				const Col C0 = Scaled(ShopGlass(P, 0.0), 0.84), CM = Scaled(ShopGlass(P, 0.5), 0.84), C1 = Scaled(ShopGlass(P, 1.0), 0.84);
				B.Quad(At(Fc, O.A0, G0, -0.075), At(Fc, O.A1, G0, -0.075), At(Fc, O.A1, GM, -0.075), At(Fc, O.A0, GM, -0.075), C0, C0, CM, CM, Fc.N);
				B.Quad(At(Fc, O.A0, GM, -0.075), At(Fc, O.A1, GM, -0.075), At(Fc, O.A1, G1, -0.075), At(Fc, O.A0, G1, -0.075), CM, CM, C1, C1, Fc.N);
				Flat(O.A0, O.A1, G1, ZT, -0.075, Mixed(P.K.Glass, P.K.Sky, 0.42));
				// (its mullions)
				const int NL = std::max(1, static_cast<int>(std::lround((O.A1 - O.A0) / 0.85)));
				for (int K = 1; K < NL; ++K)
				{
					const double X = O.A0 + (O.A1 - O.A0) * K / NL;
					if (X < DA0 - 0.06 || X > DA0 + DW + 0.06) Flat(X - 0.03, X + 0.03, 0.62, ZT - 0.27, -0.06, Scaled(Paint, 0.9));
				}
			}
			else if (bBurnt)
			{
				// (burnt: open on the shell's dark, as the near level's, its charred posts, a mullion or two and its head
				// rail standing, not a black slab)
				for (const auto& [X0, X1] : { std::pair<double, double>{ O.A0, In0 }, { In1, O.A1 }, { DA0 - 0.07, DA0 }, { DA0 + DW, DA0 + DW + 0.07 } })
					if (X1 > X0 + 0.01) Flat(X0, X1, 0.0, ZT, -0.06, Paint);
				const int NL = std::max(1, static_cast<int>(std::lround((O.A1 - O.A0) / 0.85)));
				for (int K = 1; K < NL; ++K)
				{
					const double X = O.A0 + (O.A1 - O.A0) * K / NL;
					if ((X < DA0 - 0.12 || X > DA0 + DW + 0.12) && Unit(Mix(H + static_cast<uint64_t>(K))) < 0.5) Flat(X - 0.03, X + 0.03, 0.62, ZT - 0.27, -0.06, Paint);
				}
				Flat(O.A0, O.A1, ZT - 0.07, ZT, -0.06, Paint);
				return;
			}
			else Flat(O.A0, O.A1, 0.62, ZT, -0.075, Fill);
			if (!bBurnt) Flat(O.A0, O.A1, ZT - 0.34, ZT - 0.27, -0.06, Scaled(Paint, 0.9));
			Flat(DA0, DA0 + DW, 0.0, ZT - 0.34, -0.07, O.bLeaf && !bBurnt ? Scaled(Paint, 0.8) : Dark);
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
			if (O.What == OpShop)
			{
				ShopNear(B, P, O, Fc, F);
				return;
			}
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

		void Heap(Pen& B, const Plan& P, V3 O, double RX, double RY, double High, double Yaw, int NA, int NR, uint64_t Seed, bool bBlocks, double Many = 10.0, double Big = 1.0);

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
				// (the middle's with its broken stones on it too, fewer and bigger, so it is no smooth pale dome, an upturned
				// bowl: the art director's village round 1)
				Heap(B, P, C, Len, Out, High, std::atan2(Fc.T.Y, Fc.T.X), Level == 0 ? 16 : Level == 1 ? 12 : 6, Level == 0 ? 4 : Level == 1 ? 3 : 2, Seed, Level <= 1, Level == 0 ? 10.0 : 4.5, Level == 0 ? 1.0 : 1.35);
			}
		}

		/** A heap of fallen stone and earth, lumpy, its rim sunk well into the ground; stones and slates on it. */
		void Heap(Pen& B, const Plan& P, V3 O, double RX, double RY, double High, double Yaw, int NA, int NR, uint64_t Seed, bool bBlocks, double Many, double Big)
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
			const int N = static_cast<int>(Many * RX * RY);
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
					const double Soot_ = R.Range(0.0, 0.4), Wy_ = R.Range(0.07, 0.12), Wx_ = R.Range(0.1, 0.16);
					B.Box(At3 + Up * 0.03, AX * Wx_, AY * Wy_, AZ * 0.006, Mixed(P.K.Roof, P.K.Soot, Soot_), 16);
				}
				else
				{
					const double Sz = R.Range(0.1, 0.24) * Big;
					const double Hz_ = R.Range(0.6, 1.0), Lx_ = R.Range(1.0, 1.8);
					B.Box(At3, AX * Sz * Lx_, AY * Sz, AZ * Sz * Hz_, StoneOwn(P, Hash(Seed, static_cast<uint64_t>(K))), 16);
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
					const double Yaw_ = R.Range(0.0, Pi), C_ = R.Range(0.5, 0.9), B_ = R.Range(0.8, 1.3), A_ = R.Range(1.0, 1.8), Y_ = R.Range(-IY + 1.0, IY - 1.0);
					HayMound(B, P, V3{ X, Y_, P.Loft }, A_, B_, C_, Yaw_, Hash(P.Key ^ 0x4A71, static_cast<uint64_t>(K)), P.Hurt == 2 ? 0.8 : 1.0);
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
					else
					{
						const double Soot_ = R.Range(0.0, 0.5), Wy_ = R.Range(0.07, 0.11), Wx_ = R.Range(0.1, 0.15);
						B.Box(At3 + Up * 0.008, AX * Wx_, AY * Wy_, Up * 0.007, Mixed(P.K.Roof, P.K.Soot, Soot_), 16);
					}
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
		 * stands whole there (its own top then), and none for the church, nor a shell's last gable (already a line).
		 */
		constexpr double MidStep = 0.3;
		std::vector<double> MidLine(const Plan& P, int F)
		{
			const Face Fc = FaceOf(P, F);
			std::vector<double> Z;
			// (a barn's or a byre's; a village house's, the art director's village round 1: its shell's middle stepped
			// in square notches where the near's line runs ragged; and so the farmhouse's, the village round 2 carry)
			if (P.Kind > 2 || P.Hurt < 2 || KeptGable(P, Fc)) return Z;
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
				bool bRender = false; // (a rendered wall's render)
			};
			const bool bRender = P.Finish == 2;
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
				if (Fc.bGable ? Z0 > P.Ridge : Z0 >= P.EW - 1e-6) break;
				std::vector<std::pair<double, double>> Res = DressedIn(P, Fc, F, I, false);
				std::sort(Res.begin(), Res.end());
				// (the course's stretches: dressed, or stones laid along what is free)
				std::vector<Run> Runs;
				Rng R(Hash(P.Key ^ 0x31D, static_cast<uint64_t>(F), I));
				const std::vector<RenderGap> Gaps = bRender ? GapsIn(P, F, I) : std::vector<RenderGap>{};
				auto Lay = [&](double S0, double S1) {
					if (bRender)
					{
						// (a rendered wall: its render in pieces, and where it has come away the stones, each gap as the mean of
						// its edges up the course; the band under a broken top bare too)
						std::vector<double> Cuts{ S0 };
						for (const RenderGap& G : Gaps)
						{
							const double GL = (G.L[0] + G.L[1] + G.L[2]) / 3.0, GR = (G.R[0] + G.R[1] + G.R[2]) / 3.0;
							if (GR > S0 && GL < S1 && GR - GL > 0.03)
							{
								Cuts.push_back(std::clamp(GL, S0, S1));
								Cuts.push_back(std::clamp(GR, S0, S1));
							}
						}
						Cuts.push_back(S1);
						std::sort(Cuts.begin(), Cuts.end());
						for (size_t K = 0; K + 1 < Cuts.size(); ++K)
						{
							const double C0 = Cuts[K], C1 = Cuts[K + 1], CM = 0.5 * (C0 + C1);
							if (C1 - C0 < 0.005) continue;
							bool bGap = false;
							for (const RenderGap& G : Gaps)
								if (CM > (G.L[0] + G.L[1] + G.L[2]) / 3.0 && CM < (G.R[0] + G.R[1] + G.R[2]) / 3.0) bGap = true;
							if (bGap)
							{
								Runs.push_back({ C0, C1, false, false });
								continue;
							}
							const int N = std::max(1, static_cast<int>(std::ceil((C1 - C0) / 1.2)));
							for (int J = 0; J < N; ++J)
							{
								const double X0 = C0 + (C1 - C0) * J / N, X1 = C0 + (C1 - C0) * (J + 1) / N;
								const bool bBand = P.Hurt >= 2 && P.Course[I + 1] > RenderTop(P, Fc, F, 0.5 * (X0 + X1)) - 0.05;
								Runs.push_back({ X0, X1, false, !bBand });
							}
						}
						return;
					}
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
						if (Ru.bRender) Cut(B, Fc, C0, C1, Z0, Z1, L0, L1, 0.006, Fc.N, [&](double A, double Z) { return RenderAt(P, Fc, F, A, Z); });
						else Cut(B, Fc, C0, C1, Z0, Z1, L0, L1, Ru.bDressed ? 0.012 : 0.0, Fc.N, [&](double A, double Z) { return Weathered(P, Fc, F, A, Z, Own, true); });
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
				if (O.What == OpShop)
				{
					ShopPlain(B, P, O, Fc, F, bMid);
					continue;
				}
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
					// (far off: painted on the wall, the openings dark, the shutters beside them; the art director's village
					// round 1: the glass as dark as the middle level's, deep in its reveal (about 43, 52, 64), not the sky's
					// pale, which on a pale wall left the house blank; the door its own paint, as the middle level's lit)
					Col C = O.What == 1 ? (O.bLeaf ? P.K.Door : Rgb(16, 15, 14)) : O.Shutters == 1 ? Scaled(P.K.Shutter, 0.9) : Scaled(P.K.Glass, 0.8);
					if (!O.bGlass && O.What != 1) C = Rgb(16, 15, 14);
					B.Quad1(At(Fc, O.A0, O.Z0, 0.012), At(Fc, O.A1, O.Z0, 0.012), At(Fc, O.A1, Z1, 0.012), At(Fc, O.A0, Z1, 0.012), C, Fc.N);
					// (a window's open shutters; a door has none, though its plan's draw gives it a shutters' state)
					if (O.What != 1 && (O.Shutters == 0 || O.Shutters == 2))
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
			// A gable down at the middle level (the art director's village round 2 carry: one slope's dark underside stood
			// past the other's end as a closed black gable): both slopes' deck back to the last whole truss, straight
			// across; past it the purlins run on as the near's do, the last sagging, and a rafter or two broken short.
			const bool bTruss = bMid && P.Hurt == 2;
			double Truss = 1e9;
			if (bTruss)
				for (int Side = -1; Side <= 1; Side += 2)
					for (double Sd = 0.0; Sd <= P.Ls + 1e-6; Sd += 0.25) Truss = std::min(Truss, CutAt(P, Side, Sd) + 0.14);
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
						double XA = X0 + (X1 - X0) * I / NX, XB = X0 + (X1 - X0) * (I + 1) / NX;
						bool bCutL = false, bCutR = false;
						if (bTruss && P.Broken > 0)
						{
							if (XA >= Truss - 0.05) continue;
							if (XB > Truss - 0.05) { XB = Truss; bCutR = true; }
						}
						else if (bTruss)
						{
							if (XB <= -Truss + 0.05) continue;
							if (XA < -Truss + 0.05) { XA = -Truss; bCutL = true; }
						}
						const double XM = 0.5 * (XA + XB);
						if (bRagged && Side == P.HoleSide && std::fabs(XM - P.HoleX) < 1.4 * P.HoleRX + 0.5 * (XB - XA) && SM - 0.5 * (S1 - S0) < P.HoleS + 1.4 * P.HoleRS && SM + 0.5 * (S1 - S0) > P.HoleS - 2.6 * P.HoleRS)
						{
							Ragged(Side, K, I, XA, XB, S0, S1);
							continue;
						}
						if (!bTruss && !DeckThere(P, Side, XM, SM)) continue;
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
						if (I == 0 || bCutL) B.Quad1(A0.P, A3.P, U3.P, U0.P, bCutL ? Rgb(40, 32, 28) : Scaled(C, 0.6), V3{ -1.0, 0.0, 0.0 });
						if (I == NX - 1 || bCutR) B.Quad1(A1.P, A2.P, U2.P, U1.P, bCutR ? Rgb(40, 32, 28) : Scaled(C, 0.6), V3{ 1.0, 0.0, 0.0 });
						if (!bTruss && I > 0 && !DeckThere(P, Side, XM - (XB - XA), SM)) B.Quad1(A0.P, A3.P, U3.P, U0.P, Rgb(40, 32, 28), V3{ -1.0, 0.0, 0.0 });
						if (!bTruss && I < NX - 1 && !DeckThere(P, Side, XM + (XB - XA), SM)) B.Quad1(A1.P, A2.P, U2.P, U1.P, Rgb(40, 32, 28), V3{ 1.0, 0.0, 0.0 });
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
			if (bTruss)
			{
				const Col Wood = Mixed(Rgb(92, 72, 54), P.K.Char, 0.5), Burnt = Mixed(Wood, P.K.Char, 0.6);
				for (int Side = -1; Side <= 1; Side += 2)
				{
					// (the purlins, from under the deck to where the near's end, the upper one sagging)
					for (int K = 0; K < 2; ++K)
					{
						const double S = P.Lk + P.Lm * (K == 0 ? 0.32 : 0.66);
						const double End = CutAt(P, Side, S) + 0.7 + 0.6 * Unit(Hash(P.Key, 0x9A1, static_cast<uint64_t>(K * 2 + (Side > 0))));
						// (their ends under the deck and at the sag's joint hidden, so not drawn)
						const RoofPt A = RoofAt(P, Side, P.Broken * (Truss - 0.3), S, -0.25);
						RoofPt Bp = RoofAt(P, Side, P.Broken * End, S, -0.25);
						if (K == 0)
						{
							B.Beam(A.P, Bp.P, 0.16, 0.2, Mixed(Wood, P.K.Char, 0.3), A.N, 1);
							continue;
						}
						const RoofPt M = RoofAt(P, Side, P.Broken * std::max(Truss + 0.1, End - 1.2), S, -0.25);
						Bp.P = Bp.P - Up * 0.45;
						B.Beam(A.P, M.P, 0.16, 0.2, Wood, A.N, 1);
						B.Beam(M.P, Bp.P, 0.16, 0.2, Burnt, A.N, 1);
					}
					// (a rafter broken short past the truss, from the eaves up)
					const double XR = Truss + 0.35 + 0.3 * Unit(Hash(P.Key, 0x9A7, static_cast<uint64_t>(Side > 0)));
					const double To = P.Ls * (0.35 + 0.35 * Unit(Hash(P.Key, 0x9A8, static_cast<uint64_t>(Side > 0))));
					const RoofPt R0 = RoofAt(P, Side, P.Broken * XR, SlopeAt(P, 0.5 * P.D - P.Thick), -0.1), R1 = RoofAt(P, Side, P.Broken * XR, To, -0.1);
					B.Beam(R0.P, R1.P, 0.075, 0.13, Burnt, R0.N, 1);
				}
			}
			// the ridge, a rounded cap
			const double RX0 = ChimneyAt(P, -1) ? -0.5 * P.L + 0.6 : X0, RX1 = ChimneyAt(P, 1) ? 0.5 * P.L - 0.6 : X1;
			double CX0 = RX0, CX1 = RX1;
			if (P.Hurt == 2)
			{
				const double Cut = bTruss ? Truss : std::min(CutAt(P, 1, P.Ls), CutAt(P, -1, P.Ls)) - 0.2;
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
						if (P.Broken > 0) GX1 = std::min({ GX1, CutAt(P, Side, 0.2) - 0.3, Truss - 0.1 });
						else GX0 = std::max({ GX0, -(CutAt(P, Side, 0.2) - 0.3), -(Truss - 0.1) });
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
				// (the middle's with its broken stones on it, as the shells' foot heaps, so it is no smooth dark dome: the art
				// director's village round 2 carry, the farmhouse's middle)
				Heap(B, P, V3{ P.Broken * (0.5 * P.L + 1.1), 0.0, -0.15 }, 2.3, 0.5 * P.D + 0.9, 1.5, 0.0, bMid ? 12 : 8, bMid ? 4 : 2, P.Key ^ 0x4EA9, bMid, 3.0, 1.5);
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

		// ================================================================== the east's houses

		/**
		 * The izba (Kind 7; release 2, the same trial: Brian 24 November, every theatre's houses to the vehicles' detail): a
		 * Russian village's log house as the east's villages lay it (LandFarm's KIzba and KIzbaThatch, its footprint 7.5 by
		 * 7.2 m). Round logs of pine laid in thirteen courses, each sitting in the groove of the one below, saddle-notched at
		 * the corners where their ends run on past the walls, the end grain pale with its rings and a check; a foundation of
		 * field boulders under them; windows small, three to the gable on the street, each in its carved frame (a pediment
		 * over its frieze, an apron with scallops under its sill), its sash in a T, a white curtain behind the glass, its two
		 * painted shutters open on the logs (now and then shut); a boarded gable with its attic window, carved barge boards
		 * and a hanging towel board at the apex; a porch on two posts before the plank door; the roof of overlapping planks
		 * under a ridge log carved at its end into a horse's head (Roof 0), or of thick thatch in courses under a straw ridge
		 * held by crossed poles (Roof 1); a brick chimney; a woodpile stacked along the back wall, a bench by the door, a
		 * barrel. Its own frame as the others': x along its length, y across (the porch on the +y side), z up from the
		 * ground at its middle; the walls stand a little back from the middle so the porch keeps inside the footprint.
		 */
		namespace Izb
		{
			/** An opening in a wall: its face (0 front, 1 the -x gable, 2 back, 3 the +x gable), its middle along the wall
			 * (U, the wall's own way), width, sill and head; what it is (0 a window, 1 the door, 2 an attic window). */
			struct Gap
			{
				int Face = 0;
				double U = 0.0, W = 0.7, Z0 = 1.3, Z1 = 2.3;
				int What = 0;
				bool bShut = false;
			};

			struct Plan
			{
				Spec S;
				bool bThatch = false;
				double A = 3.3, Bh = 2.85, Yc = -0.3; // half the long walls and the gables between the corners' logs; the walls' middle across
				double R = 0.12, Hc = 0.2, Z0 = 0.54; // a log's radius, a course's rise, the lowest long-wall log's middle
				double Ext = 0.28;                    // how far the logs run on past the corners
				double OvE = 0.5, OvG = 0.2;          // the roof out past the long walls, and past the logs' ends at the gables
				int Courses = 13;
				double Tan = 0.87, CosP = 0.75, SinP = 0.66, Ridge = 5.8, Thick = 0.1;
				double Zt = 3.1, Zg = 3.0; // the long walls' top and the gables' last log's crown
				double Xr = 4.0, Se = 3.5, Qe = 4.6; // the roof's half-length, its eaves out from the ridge across and down its slope
				double Chx = -0.6;
				Col Log, LogEnd, Paint, Carve, Shutter, Mark, Bare, Plank, Thatch, Brick, Stone, Glass, Sky, Door, Dark;
				double Snow = 0.0; // the snow lying on its roof, 0 to 1 (Spec's, eased)
				bool bBareShut = false; // its shutters bare wood
				std::vector<Gap> Gaps;
			};

			struct Wall
			{
				V3 C, T, N;      // its middle on the line of the logs' middles; along it; out from it
				double Half = 3; // to the corners' logs' middles
				double ZOff = 0; // its courses: the gables' half a course above the long walls'
			};
			Wall WallOf(const Plan& P, int F)
			{
				Wall W;
				switch (F)
				{
				case 0: W = { V3{ 0.0, P.Yc + P.Bh, 0.0 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, P.A, 0.0 }; break;
				case 2: W = { V3{ 0.0, P.Yc - P.Bh, 0.0 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, P.A, 0.0 }; break;
				case 1: W = { V3{ -P.A, P.Yc, 0.0 }, V3{ 0.0, 1.0, 0.0 }, V3{ -1.0, 0.0, 0.0 }, P.Bh, 0.5 * P.Hc }; break;
				default: W = { V3{ P.A, P.Yc, 0.0 }, V3{ 0.0, 1.0, 0.0 }, V3{ 1.0, 0.0, 0.0 }, P.Bh, 0.5 * P.Hc }; break;
				}
				return W;
			}
			V3 On(const Wall& W, double U, double Z, double D) { return W.C + W.T * U + W.N * D + V3{ 0.0, 0.0, Z }; }
			double CourseZ(const Plan& P, const Wall& W, int K) { return P.Z0 + W.ZOff + K * P.Hc; }
			/** The seam over course K (an opening's sill or head stands on one). */
			double Seam(const Plan& P, const Wall& W, int K) { return CourseZ(P, W, K) + 0.5 * P.Hc; }
			uint64_t LogSeed(const Plan& P, int F, int K) { return Hash(P.S.Key ^ 0x1D6, static_cast<uint64_t>(F * 64 + K)); }
			/** The courses a wall has: a gable one fewer, its last log half a course under the long walls' top. */
			int CoursesOf(const Plan& P, int F) { return F == 1 || F == 3 ? P.Courses - 1 : P.Courses; }
			/** How far past its corner's middle a log at Z may run (Want) before the roof's slope takes it: the gables' top
			 * logs cut back under the eaves (never short of the corner's crossing). */
			double EndReach(const Plan& P, int F, double Z, double Want)
			{
				if (F == 0 || F == 2) return Want;
				const double S = (P.Ridge - 0.5 * P.Thick / P.CosP - 0.03 - Z - P.R) / P.Tan;
				return std::max(P.R + 0.02, std::min(Want, S - P.Bh));
			}

			/** How far the porch's floor stands out from the front's logs; how many steps down from its floor at Zf. */
			const double PorchOut = 0.9;
			int PorchSteps(double Zf) { return std::max(2, static_cast<int>(std::round(Zf / 0.2))); }

			Plan Make(const Spec& S)
			{
				Plan P;
				P.S = S;
				P.bThatch = S.Roof == 1;
				Rng Rg(S.Key ^ 0x12BA);
				P.R = 0.112 + 0.016 * Rg.Next();
				P.Hc = 1.72 * P.R;
				P.Ext = 0.24 + 0.08 * Rg.Next();
				// (all of it inside its footprint: the roof's verges at its ends; across it the porch, 0.9 m out, at the front and
				// the woodpile, half a metre, at the back)
				P.OvG = S.Roof == 1 ? 0.3 : 0.2;
				P.A = std::max(2.4, 0.5 * S.Long - P.R - P.Ext - P.OvG - 0.05);
				const double Half = 0.5 * S.Deep - 0.03;
				P.Bh = std::max(2.2, 0.5 * (2.0 * Half - 2.0 * P.R - PorchOut - 0.5));
				P.Yc = Half - P.Bh - P.R - PorchOut;
				P.Z0 = 0.4 + P.R;
				const double Pitch = (P.bThatch ? 46.0 : 41.0) * Pi / 180.0;
				P.Tan = std::tan(Pitch);
				P.CosP = std::cos(Pitch);
				P.SinP = std::sin(Pitch);
				P.Zt = P.Z0 + (P.Courses - 1) * P.Hc + P.R;
				P.Zg = P.Zt - 0.5 * P.Hc; // (the gables' last log under the long walls' top one, the roof clear of its ends)
				P.Thick = P.bThatch ? 0.34 : 0.12;
				P.Ridge = P.Zt + P.Thick + (P.Bh + P.R) * P.Tan;
				P.OvE = 0.5;
				P.Se = P.Bh + P.R + P.OvE;
				P.Qe = P.Se / P.CosP;
				P.Xr = P.A + P.R + P.Ext + P.OvG;
				P.Chx = -0.25 * P.A + 0.3 * (Rg.Next() - 0.5);
				// Its colours: the logs grey-brown, weathered silver on some, browner on others. Its paint one colour a house,
				// years old, gone dull and chalky (a third of the way to chalk): a faded blue, white, a faded green or an
				// ochre-red; the carving's small cut accents white on a coloured frame, bare wood on a white one; the shutters
				// bare wood, white, or the frames' colour, each with its diamond in the other (art director, 24 Nov: no
				// yellow with blue, no new paint). The planks silver-grey; the grey wood the paint flakes to.
				const double UWood = Rg.Next(), UPaint = Rg.Next(), UShut = Rg.Next();
				P.Log = UWood < 0.4 ? Rgb(118, 110, 98) : UWood < 0.75 ? Rgb(110, 98, 84) : Rgb(100, 84, 66);
				P.LogEnd = Mixed(P.Log, Rgb(176, 160, 132), 0.55);
				const Col Chalk = Rgb(180, 178, 170);
				const Col White = Mixed(Rgb(214, 210, 198), Chalk, 0.25);
				P.Bare = Rgb(124, 118, 106);
				const bool bWhite = UPaint >= 0.34 && UPaint < 0.58;
				P.Paint = bWhite ? White : Mixed(UPaint < 0.34 ? Rgb(96, 128, 150) : UPaint < 0.8 ? Rgb(98, 126, 94) : Rgb(140, 72, 52), Chalk, 0.35);
				P.Carve = bWhite ? P.Bare : White;
				P.Shutter = UShut < 0.4 ? Rgb(128, 120, 106) : UShut < 0.65 ? White : P.Paint;
				P.bBareShut = UShut < 0.4;
				P.Mark = UShut < 0.4 ? P.Paint : bWhite ? P.Bare : UShut < 0.65 ? P.Paint : White;
				P.Plank = Rgb(112, 107, 98);
				const double UThatch = Rg.Next(), UBrick = Rg.Next();
				// (aged rye straw, grey-brown: old, greyer, newer; the art director's 95 to 115 lit on its sunny slope)
				P.Thatch = UThatch < 0.5 ? Rgb(96, 90, 78) : UThatch < 0.8 ? Rgb(90, 88, 82) : Rgb(102, 92, 76);
				P.Snow = Smooth(0.03, 0.5, S.Snow);
				P.Brick = UBrick < 0.55 ? Rgb(128, 80, 64) : Rgb(198, 194, 184); // (bare old brick, or limewashed)
				P.Stone = Rgb(124, 118, 108);
				P.Glass = Rgb(30, 34, 38);
				P.Sky = Rgb(150, 160, 170);
				P.Door = Rgb(96, 78, 60);
				P.Dark = Rgb(30, 26, 22);
				// The openings: the door near the front's -x end under its porch, two windows to its +x; three to the +x gable
				// (the street's), one each to the -x gable and the back; an attic window in each gable.
				const Wall WF = WallOf(P, 0), WB = WallOf(P, 2), WG = WallOf(P, 3), WH = WallOf(P, 1);
				auto Win = [&](int F, const Wall& W, double U) {
					Gap G;
					G.Face = F;
					G.U = U;
					G.W = 0.66 + 0.06 * Rg.Next();
					G.Z0 = Seam(P, W, 3);
					G.Z1 = Seam(P, W, 8);
					G.bShut = Rg.Next() < 0.1;
					P.Gaps.push_back(G);
				};
				Gap D;
				D.Face = 0;
				D.U = -(P.A - 0.95);
				D.W = 0.88;
				D.Z0 = Seam(P, WF, 0);
				D.Z1 = Seam(P, WF, 9);
				D.What = 1;
				P.Gaps.push_back(D);
				// (each window spans its frame and its two shutters open, about 1.7 m: the front's two between the door's frame and
				// the corner, the gable's three as far apart as it allows, or two)
				const double Lo = D.U + 0.5 * D.W + 0.15, Hi = P.A - P.R - 0.05, Mf = 0.5 * (Lo + Hi);
				Win(0, WF, Mf - 0.88);
				Win(0, WF, Mf + 0.88);
				const double G3 = std::min(1.8, P.Bh - 1.0);
				if (G3 >= 1.7)
				{
					Win(3, WG, -G3);
					Win(3, WG, 0.0);
					Win(3, WG, G3);
				}
				else
				{
					Win(3, WG, -0.88);
					Win(3, WG, 0.88);
				}
				Win(1, WH, 0.25 * P.Bh);
				Win(2, WB, 0.35 * P.A);
				for (int F : { 1, 3 })
				{
					Gap At;
					At.Face = F;
					At.U = 0.0;
					At.W = 0.46;
					At.Z0 = P.Zg + 0.5;
					At.Z1 = P.Zg + 1.08;
					At.What = 2;
					P.Gaps.push_back(At);
				}
				return P;
			}

			/** Paint worn by the years: flaked back toward the grey wood under it, the more the lower and wetter the piece
			 * (Low, 0 to 1), each piece its own. */
			Col Worn(const Plan& P, const Col& C, double Low, uint64_t Seed) { return Mixed(C, P.Bare, std::clamp(Low * (0.35 + 0.4 * Unit(Seed)), 0.0, 0.8)); }
			/** Flakes of paint gone to the wood: N small ragged patches on a painted face (wall W, across U0..U1, up Z0..Z1, D out),
			 * the more toward its foot. */
			void Flakes(Pen& B, const Plan& P, const Wall& W, double U0, double U1, double Z0, double Z1, double D, int N, uint64_t Seed)
			{
				Rng Rf(Seed);
				for (int K = 0; K < N; ++K)
				{
					const double Uc = Rf.Range(U0 + 0.02, U1 - 0.02), Lo = Rf.Next(), Zc = Z0 + 0.02 + (Z1 - Z0 - 0.04) * Lo * Lo, Rr = Rf.Range(0.012, 0.035);
					const Col C = Varied(P.Bare, Rf.Range(0.88, 1.06), 0.0);
					const uint32_t Cn = B.Vert(On(W, Uc, Zc, D), W.N, C);
					std::vector<uint32_t> Rim;
					for (int J = 0; J < 6; ++J)
					{
						const double Th = 2.0 * Pi * J / 6, Rj = Rr * Rf.Range(0.55, 1.3);
						Rim.push_back(B.Vert(On(W, Uc + 1.4 * Rj * std::cos(Th), Zc + Rj * std::sin(Th), D), W.N, C));
					}
					for (size_t J = 0; J < 6; ++J) B.Tri(Cn, Rim[J], Rim[(J + 1) % 6], W.N);
				}
			}

			/** A point on the roof's top: along its length X, down its slope Q from the ridge, Lift off it, on side Sd. */
			V3 RoofAt(const Plan& P, int Sd, double X, double Q, double Lift)
			{
				return V3{ X, P.Yc + Sd * (Q * P.CosP + Lift * P.SinP), P.Ridge - Q * P.SinP + Lift * P.CosP };
			}
			V3 RoofN(const Plan& P, int Sd) { return V3{ 0.0, Sd * P.SinP, P.CosP }; }
			/** The roof's underside over a place across (S out from the ridge). */
			double UnderAt(const Plan& P, double S) { return P.Ridge - S * P.Tan - P.Thick / P.CosP; }

			/** The thatch's straw at a place on its slope: along it X, down from the ridge Q, on side Sd (greyer toward the
			 * eaves, moss in patches, more on the back, a newer patch here and there). */
			Col StrawCol(const Plan& P, int Sd, double X, double Q)
			{
				// (combed straw: its lines down the slope each a little paler or darker; broad weathering over them)
				Col C = Scaled(P.Thatch, 0.92 + 0.14 * Noise01(X / 0.05, Q / 0.9, P.S.Key ^ 0x57A0));
				C = Scaled(C, 0.94 + 0.1 * Noise01(X / 0.9, Q / 0.7, P.S.Key ^ 0x57A5));
				C = Mixed(C, Rgb(124, 116, 102), 0.25 * Smooth(0.3 * P.Qe, P.Qe, Q));
				C = Mixed(C, Rgb(80, 88, 60), (Sd < 0 ? 0.45 : 0.12) * Smooth(0.6, 0.82, Noise01(X / 1.1, Q / 0.9, P.S.Key ^ 0x57A1)));
				return Mixed(C, Rgb(170, 150, 110), 0.2 * Smooth(0.72, 0.9, Noise01(X / 0.8, Q / 0.6, P.S.Key ^ 0x57A2)));
			}
			/** A roof plank's colour at a place on it: its own C, grained, lichen and moss in patches, darker to the eaves. */
			Col PlankCol(const Plan& P, int Sd, const Col& C, uint64_t Seed, double Xp, double Q)
			{
				Col Cp = Scaled(C, 0.94 + 0.1 * Noise01(Xp / 0.15, Q / 1.2, Seed));
				Cp = Mixed(Cp, Rgb(96, 104, 74), (Sd < 0 ? 0.55 : 0.3) * Smooth(0.55, 0.85, Noise01(Xp / 0.9, Q / 0.8, P.S.Key ^ 0x3055)));
				return Mixed(Cp, Rgb(70, 64, 56), 0.25 * Smooth(P.Qe - 0.8, P.Qe, Q));
			}
			/** The roof's planks as the near level lays them, one side's: each plank's run across (Xa to Xb), its foot, its
			 * lift, whether it is an upper one, its own colour and seed. */
			struct RoofPlank
			{
				double Xa, Xb, Foot, Lift;
				bool bUpper;
				Col C;
				uint64_t Seed;
			};
			std::vector<RoofPlank> PlanksOf(const Plan& P, int Sd)
			{
				std::vector<RoofPlank> Out;
				Rng Rp(Hash(P.S.Key ^ 0x9A4C, static_cast<uint64_t>(Sd + 2)));
				double X = -P.Xr;
				int K = 0;
				while (X < P.Xr - 0.02)
				{
					RoofPlank Pk;
					Pk.bUpper = K % 2 == 1;
					const double Wd = Pk.bUpper ? Rp.Range(0.2, 0.27) : Rp.Range(0.1, 0.14);
					Pk.Xa = X - (Pk.bUpper ? 0.04 : 0.0);
					Pk.Xb = std::min(P.Xr, X + Wd + (Pk.bUpper ? 0.04 : 0.0));
					Pk.Foot = P.Qe + Rp.Range(-0.05, 0.05);
					Pk.Lift = Pk.bUpper ? 0.03 : 0.0;
					Pk.Seed = Hash(P.S.Key ^ 0x9A4D, static_cast<uint64_t>(K * 4 + Sd + 2));
					Pk.C = Varied(P.Plank, 0.84 + 0.24 * Unit(Pk.Seed), 0.03 * (Unit(Pk.Seed ^ 1) - 0.5));
					Out.push_back(Pk);
					X += Wd;
					++K;
				}
				return Out;
			}

			/** A log's colour at a place along it (T) and round it (Ang, 0 out, up positive), at its height Z. */
			Col LogCol(const Plan& P, uint64_t Seed, double T, double Ang, double Z)
			{
				Col C = Varied(P.Log, 0.9 + 0.18 * Unit(Seed), 0.04 * (Unit(Seed ^ 3) - 0.5));
				C = Scaled(C, 0.93 + 0.12 * Noise01(T / 0.8, 1.7, Seed));
				C = Mixed(C, Rgb(150, 146, 138), 0.18 * Smooth(0.2, 1.2, Ang) * Noise01(T / 1.6, 3.1, Seed ^ 9)); // (silvered on top)
				C = Scaled(C, 0.8 + 0.2 * Smooth(-1.4, 0.2, Ang));                                                  // (the crease under it)
				C = Mixed(C, Rgb(72, 64, 54), 0.35 * Smooth(1.0, 0.45, Z));                                          // (splash at the foot)
				C = Mixed(C, Rgb(84, 92, 66), 0.25 * Smooth(0.9, 0.6, Z) * Smooth(0.55, 0.8, Noise01(T / 0.7, Z / 0.3, Seed ^ 21))); // (moss low down)
				return C;
			}
			double LogRad(const Plan& P, uint64_t Seed, double T) { return P.R * (0.96 + 0.08 * Unit(Seed ^ 5)) * (1.0 + 0.045 * Noise(T / 1.1, 0.4, Seed ^ 0x77)); }
			V3 LogBend(uint64_t Seed, double T, V3 Out) { return V3{ 0.0, 0.0, 0.007 * Noise(T / 1.7, 0.9, Seed ^ 0x51) } + Out * (0.006 * Noise(T / 1.3, 2.2, Seed ^ 0x52)); }

			/** Part of a log's round, along Ax from T0 to T1 (metres from O), its angles A0 to A1 in NA facets. */
			void Round(Pen& B, const Plan& P, V3 O, V3 Ax, V3 Out, double T0, double T1, double A0, double A1, int NA, uint64_t Seed, double Step = 0.55)
			{
				if (T1 - T0 < 0.01) return;
				const V3 UpV = Cross(Ax, Out) * -1.0; // (up, as Out turns toward it)
				const V3 Upx = Dot(UpV, Up) < 0.0 ? UpV * -1.0 : UpV;
				const int NT = std::max(1, static_cast<int>(std::ceil((T1 - T0) / Step)));
				std::vector<uint32_t> Prev, Cur;
				for (int I = 0; I <= NT; ++I)
				{
					const double T = T0 + (T1 - T0) * I / NT;
					const double Rr = LogRad(P, Seed, T);
					const V3 Mid = O + Ax * T + LogBend(Seed, T, Out);
					Cur.clear();
					for (int J = 0; J <= NA; ++J)
					{
						const double An = A0 + (A1 - A0) * J / NA;
						const V3 Rd = Out * std::cos(An) + Upx * std::sin(An);
						Cur.push_back(B.Vert(Mid + Rd * Rr, Rd, LogCol(P, Seed, T, std::atan2(std::sin(An), std::cos(An) + 1e-9), Mid.Z)));
					}
					if (I > 0)
						for (int J = 0; J < NA; ++J)
						{
							const double An = A0 + (A1 - A0) * (J + 0.5) / NA;
							const V3 Rd = Out * std::cos(An) + Upx * std::sin(An);
							B.Tri(Prev[static_cast<size_t>(J)], Cur[static_cast<size_t>(J)], Cur[static_cast<size_t>(J) + 1], Rd);
							B.Tri(Prev[static_cast<size_t>(J)], Cur[static_cast<size_t>(J) + 1], Prev[static_cast<size_t>(J) + 1], Rd);
						}
					std::swap(Prev, Cur);
				}
				// (its checks: a dark split along it here and there on its upper side; and a knot or two)
				Rng Rk(Seed ^ 0xC4EC);
				const int Checks = static_cast<int>((T1 - T0) / 1.4 + Rk.Next());
				for (int K = 0; K < Checks; ++K)
				{
					const double Ta = Rk.Range(T0, std::max(T0, T1 - 0.6)), Tb = std::min(T1, Ta + Rk.Range(0.4, 1.3)), An = Rk.Range(0.05, 0.75);
					if (An < A0 || An > A1) continue;
					const V3 Rd = Out * std::cos(An) + Upx * std::sin(An);
					const V3 Side = Cross(Ax, Rd);
					const V3 Ma = O + Ax * Ta + LogBend(Seed, Ta, Out) + Rd * (LogRad(P, Seed, Ta) + 0.0015);
					const V3 Mb = O + Ax * Tb + LogBend(Seed, Tb, Out) + Rd * (LogRad(P, Seed, Tb) + 0.0015);
					const double W = Rk.Range(0.003, 0.007);
					B.Quad1(Ma - Side * W, Mb - Side * (0.3 * W), Mb + Side * (0.3 * W), Ma + Side * W, Rgb(46, 40, 34), Rd);
				}
				const int Knots = static_cast<int>((T1 - T0) / 1.1 + Rk.Next());
				for (int K = 0; K < Knots; ++K)
				{
					const double Tk = Rk.Range(T0 + 0.05, std::max(T0 + 0.06, T1 - 0.05)), An = Rk.Range(-0.4, 0.6);
					if (An < A0 || An > A1) continue;
					const V3 Rd = Out * std::cos(An) + Upx * std::sin(An);
					const V3 Side = Cross(Ax, Rd);
					const V3 Mk = O + Ax * Tk + LogBend(Seed, Tk, Out) + Rd * (LogRad(P, Seed, Tk) + 0.002);
					const double Kr = Rk.Range(0.012, 0.026);
					const uint32_t Cn = B.Vert(Mk, Rd, Rgb(54, 44, 34));
					std::vector<uint32_t> Rim;
					for (int J = 0; J <= 6; ++J)
					{
						const double Th = 2.0 * Pi * J / 6;
						Rim.push_back(B.Vert(Mk + Ax * (1.4 * Kr * std::cos(Th)) + Side * (Kr * std::sin(Th)), Rd, Rgb(74, 62, 48)));
					}
					for (int J = 0; J < 6; ++J) B.Tri(Cn, Rim[static_cast<size_t>(J)], Rim[static_cast<size_t>(J) + 1], Rd);
				}
			}
			/** A log's end grain at T along Ax (the end facing Ax): pale, its pith dark, its rings, a check from the middle out. */
			void EndGrain(Pen& B, const Plan& P, V3 O, V3 Ax, V3 Out, double T, uint64_t Seed)
			{
				const V3 UpV = Cross(Ax, Out) * -1.0;
				const V3 Upx = Dot(UpV, Up) < 0.0 ? UpV * -1.0 : UpV;
				const double Rr = LogRad(P, Seed, T);
				const V3 Mid = O + Ax * T + LogBend(Seed, T, Out);
				Col C = Varied(P.LogEnd, 0.88 + 0.2 * Unit(Seed ^ 0xE1), 0.0);
				C = Mixed(C, Rgb(96, 92, 86), 0.4 * Unit(Seed ^ 0xE2)); // (greyed by the years on some)
				const int N = 10;
				const uint32_t Pith = B.Vert(Mid + Ax * 0.004, Ax, Scaled(C, 0.62));
				std::vector<uint32_t> In, Rim;
				for (int J = 0; J <= N; ++J)
				{
					const double Th = 2.0 * Pi * J / N;
					const V3 Rd = Out * std::cos(Th) + Upx * std::sin(Th);
					In.push_back(B.Vert(Mid + Ax * 0.003 + Rd * (0.45 * Rr), Ax, Scaled(C, 0.92 + 0.08 * std::cos(3.0 * Th))));
					Rim.push_back(B.Vert(Mid + Rd * Rr, Ax, Scaled(C, 0.78)));
				}
				for (int J = 0; J < N; ++J)
				{
					B.Tri(Pith, In[static_cast<size_t>(J)], In[static_cast<size_t>(J) + 1], Ax);
					B.Tri(In[static_cast<size_t>(J)], Rim[static_cast<size_t>(J)], Rim[static_cast<size_t>(J) + 1], Ax);
					B.Tri(In[static_cast<size_t>(J)], Rim[static_cast<size_t>(J) + 1], In[static_cast<size_t>(J) + 1], Ax);
				}
				const double Th = 2.0 * Pi * Unit(Seed ^ 0xE3);
				const V3 Rd = Out * std::cos(Th) + Upx * std::sin(Th), Sd = Cross(Ax, Rd);
				B.Tri1(Mid + Ax * 0.005 + Sd * 0.004, Mid + Ax * 0.005 - Sd * 0.004, Mid + Ax * 0.005 + Rd * (0.95 * Rr), Rgb(40, 34, 28), Ax);
			}

			/** The walls' logs: each course its log, cut at the openings, running on past the corners to its end grain. */
			void Logs(Pen& B, const Plan& P)
			{
				for (int F = 0; F < 4; ++F)
				{
					const Wall W = WallOf(P, F);
					for (int K = 0; K < CoursesOf(P, F); ++K)
					{
						const uint64_t Seed = LogSeed(P, F, K);
						const double Z = CourseZ(P, W, K);
						const V3 O = W.C + V3{ 0.0, 0.0, Z };
						// (the stretches between the openings it passes)
						std::vector<std::pair<double, double>> Cut;
						for (const Gap& G : P.Gaps)
							if (G.Face == F && G.What != 2 && Z > G.Z0 && Z < G.Z1) Cut.push_back({ G.U - 0.5 * G.W, G.U + 0.5 * G.W });
						std::sort(Cut.begin(), Cut.end());
						double From = -W.Half + P.R;
						const bool bTop = K == P.Courses - 1;
						for (size_t I = 0; I <= Cut.size(); ++I)
						{
							const double To = I < Cut.size() ? Cut[I].first : W.Half - P.R;
							Round(B, P, O, W.T, W.N, From, To, -0.5 * Pi, bTop ? 0.85 * Pi : 0.5 * Pi, bTop ? 7 : 5, Seed);
							if (I < Cut.size()) From = Cut[I].second;
						}
						// (its two ends past the corners, round all about, and their end grain)
						for (int E = -1; E <= 1; E += 2)
						{
							const double Run = P.Ext * (0.82 + 0.36 * Unit(Seed ^ static_cast<uint64_t>(E + 7)));
							const double T0 = E * (W.Half - P.R), T1 = E * (W.Half + EndReach(P, F, Z, P.R + Run));
							Round(B, P, O, W.T, W.N, std::min(T0, T1), std::max(T0, T1), -Pi, Pi, 8, Seed, 0.3);
							EndGrain(B, P, O, W.T * static_cast<double>(E), W.N, std::fabs(T1), Seed ^ static_cast<uint64_t>(E + 3));
						}
					}
					// (the gables' gap under their first log, half a course over the long walls' first, filled by a half log)
					if (F == 1 || F == 3)
					{
						const uint64_t Seed = LogSeed(P, F, 63);
						const V3 O = W.C + V3{ 0.0, 0.0, P.Z0 - 0.5 * P.Hc };
						Round(B, P, O, W.T, W.N, -W.Half + P.R, W.Half - P.R, -0.5 * Pi, 0.2 * Pi, 3, Seed);
					}
				}
			}

			/** A field boulder along a wall, its middle Uc, length L: an egg lying in the earth (flat at its back against the
			 * wall's line), its crown at Zt, Rv its half height, Dd proud of D0. */
			void Boulder(Pen& B, const Wall& W, double Uc, double L, double Zt, double Rv, double Dd, const Col& C, uint64_t Seed)
			{
				const double Zc = Zt - Rv, D0 = 0.05;
				const double Ang[5] = { -0.5 * Pi, -0.25 * Pi, 0.0, 0.25 * Pi, 0.5 * Pi };
				const double Lat[5] = { -0.42 * Pi, -0.2 * Pi, 0.0, 0.2 * Pi, 0.38 * Pi };
				std::vector<uint32_t> Ring[5];
				std::vector<V3> At[5];
				auto Tone = [&](double Z, double Nz, uint64_t K) {
					Col Cc = Scaled(C, 0.88 + 0.22 * Unit(Seed ^ K) + 0.06 * Nz);
					Cc = Mixed(Cc, Rgb(146, 148, 124), 0.45 * Smooth(0.66, 0.88, Unit(Hash(Seed, K ^ 0x11)))); // (lichen)
					return Mixed(Cc, Rgb(66, 58, 48), 0.5 * Smooth(0.12, -0.05, Z));                             // (the earth on its foot)
				};
				for (int R = 0; R < 5; ++R)
					for (int I = 0; I < 5; ++I)
					{
						const double Bt = Lat[R], Jit = 1.0 + 0.28 * (Unit(Hash(Seed, static_cast<uint64_t>(R * 8 + I))) - 0.5);
						const double Cu = std::sin(Ang[I]) * std::cos(Bt), Cd = std::cos(Ang[I]) * std::cos(Bt), Cz = std::sin(Bt);
						const double Z = Zc + Rv * Cz * (R == 0 ? 1.0 : Jit);
						const V3 Pt = On(W, Uc + 0.5 * L * Cu * Jit, Z, D0 + Dd * Cd * Jit);
						const V3 Nn = Norm(W.T * (Cu / (0.5 * L)) + W.N * (Cd / Dd) + Up * (Cz / Rv));
						Ring[R].push_back(B.Vert(Pt, Nn, Tone(Z, Cz, static_cast<uint64_t>(R * 8 + I))));
						At[R].push_back(Pt);
					}
				// (each facet turned out from its middle)
				const V3 Mc = On(W, Uc, Zc, D0 - 0.02);
				auto Out = [&](V3 A, V3 Bb, V3 Cc) { return (A + Bb + Cc) * (1.0 / 3.0) - Mc; };
				for (int R = 0; R < 4; ++R)
					for (size_t I = 0; I < 4; ++I)
					{
						B.Tri(Ring[R][I], Ring[R][I + 1], Ring[R + 1][I + 1], Out(At[R][I], At[R][I + 1], At[R + 1][I + 1]));
						B.Tri(Ring[R][I], Ring[R + 1][I + 1], Ring[R + 1][I], Out(At[R][I], At[R + 1][I + 1], At[R + 1][I]));
					}
				const V3 Ct = On(W, Uc, Zt, D0 + 0.2 * Dd);
				const uint32_t Cap = B.Vert(Ct, Up, Tone(Zt, 1.0, 0x77));
				for (size_t I = 0; I < 4; ++I) B.Tri(Ring[4][I], Ring[4][I + 1], Cap, Out(At[4][I], At[4][I + 1], Ct));
			}

			/** The boulders along a wall's foot as the near level lays them: each one's middle along it, length, crown, half
			 * height, how far proud, colour and seed; and the foot's top and its run past the corners. */
			struct Stone
			{
				double Uc, L, Zt, Rv, Dd;
				Col C;
				uint64_t Seed;
			};
			double FootTop(const Plan& P, const Wall& W) { return P.Z0 + W.ZOff - P.R * 0.8; }
			double FootRun(const Plan& P, const Wall& W) { return W.Half + P.R + 0.12; }
			std::vector<Stone> StonesOf(const Plan& P, int F)
			{
				const Wall W = WallOf(P, F);
				const double Top = FootTop(P, W), Run = FootRun(P, W);
				std::vector<Stone> Out;
				Rng Rs(Hash(P.S.Key ^ 0xF0071, static_cast<uint64_t>(F)));
				double U = -Run;
				int K = 0;
				while (U < Run - 0.08)
				{
					const double L = std::min(Rs.Range(0.45, 0.8), Run - U + 0.06), Zt = Top - Rs.Range(0.0, 0.1);
					const double Tn = Rs.Next(), Hu = Rs.Next(), Rv = 0.5 * (Zt + 0.12) * Rs.Range(0.9, 1.1), Dd = Rs.Range(0.1, 0.17);
					Out.push_back({ U + 0.5 * L, L + 0.08, Zt, Rv, Dd, Varied(P.Stone, 0.58 + 0.28 * Tn, 0.06 * (Hu - 0.5)), Hash(P.S.Key ^ 0xB0D, static_cast<uint64_t>(F * 64 + K++)) });
					U += L;
				}
				return Out;
			}

			/** Field boulders under the logs, packed close along each wall and out under the corners, the earth between them. */
			void Footing(Pen& B, const Plan& P)
			{
				for (int F = 0; F < 4; ++F)
				{
					const Wall W = WallOf(P, F);
					const double Top = FootTop(P, W), Bot = -std::max(0.2, P.S.Below), Run = FootRun(P, W);
					B.Quad1(On(W, -Run, Bot, 0.05), On(W, Run, Bot, 0.05), On(W, Run, Top - 0.02, 0.05), On(W, -Run, Top - 0.02, 0.05), Rgb(62, 56, 48), W.N);
					for (const Stone& St : StonesOf(P, F)) Boulder(B, W, St.Uc, St.L, St.Zt, St.Rv, St.Dd, St.C, St.Seed);
				}
			}

			/** A box from its middle and its three half sizes along a wall's own ways (along, out, up). */
			void WBox(Pen& B, const Wall& W, double U, double Z, double D, double HU, double HD, double HZ, const Col& C, int Skip = 4)
			{
				B.Box(On(W, U, Z, D), W.T * HU, W.N * HD, V3{ 0.0, 0.0, HZ }, C, Skip);
			}

			/** A window, its carved frame, its sash and glass and curtain, its shutters. */
			void Window(Pen& B, const Plan& P, const Gap& G, uint64_t Seed)
			{
				const Wall W = WallOf(P, G.Face);
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Z0 = G.Z0, Z1 = G.Z1, F = P.R; // (the logs' crowns)
				const bool bAttic = G.What == 2;
				const Col Pt = Varied(P.Paint, 0.96 + 0.06 * Unit(Seed), 0.0), Cv = P.Carve;
				const double Sash = bAttic ? F - 0.06 : 0.0;
				// (its reveal: the jamb boards round it, from the sash out to the frame)
				const Col Jamb = Scaled(Pt, 0.8);
				B.Quad1(On(W, U0, Z0, Sash), On(W, U0, Z1, Sash), On(W, U0, Z1, F + 0.02), On(W, U0, Z0, F + 0.02), Jamb, W.T);
				B.Quad1(On(W, U1, Z0, Sash), On(W, U1, Z1, Sash), On(W, U1, Z1, F + 0.02), On(W, U1, Z0, F + 0.02), Jamb, W.T * -1.0);
				B.Quad1(On(W, U0, Z1, Sash), On(W, U1, Z1, Sash), On(W, U1, Z1, F + 0.02), On(W, U0, Z1, F + 0.02), Scaled(Jamb, 0.7), V3{ 0.0, 0.0, -1.0 });
				B.Quad1(On(W, U0, Z0, Sash), On(W, U1, Z0, Sash), On(W, U1, Z0, F + 0.02), On(W, U0, Z0, F + 0.02), Jamb, Up);
				// (the glass: the sky in its upper panes, a white curtain behind the lower)
				const double ZT = Z0 + 0.64 * (Z1 - Z0);
				auto Pane = [&](double A0, double A1, double Za, double Zb, bool bCurtain) {
					const double M = 0.25 + 0.35 * Noise01(G.U * 3.0 + A0, Za, Seed);
					const Col Hi = Mixed(P.Glass, P.Sky, M + 0.2), Lo = Mixed(P.Glass, P.Sky, M * 0.6);
					if (!bCurtain)
					{
						B.Quad(On(W, A0, Za, Sash - 0.01), On(W, A1, Za, Sash - 0.01), On(W, A1, Zb, Sash - 0.01), On(W, A0, Zb, Sash - 0.01), Lo, Lo, Hi, Hi, W.N);
						return;
					}
					const double Zc = Za + 0.62 * (Zb - Za);
					const Col Cu = Mixed(Rgb(176, 172, 162), P.Sky, 0.15);
					B.Quad(On(W, A0, Za, Sash - 0.01), On(W, A1, Za, Sash - 0.01), On(W, A1, Zc, Sash - 0.01), On(W, A0, Zc, Sash - 0.01), Scaled(Cu, 0.78), Scaled(Cu, 0.78), Cu, Cu, W.N);
					B.Quad(On(W, A0, Zc, Sash - 0.01), On(W, A1, Zc, Sash - 0.01), On(W, A1, Zb, Sash - 0.01), On(W, A0, Zb, Sash - 0.01), Lo, Lo, Hi, Hi, W.N);
				};
				if (G.bShut)
				{
					B.Quad1(On(W, U0, Z0, Sash - 0.01), On(W, U1, Z0, Sash - 0.01), On(W, U1, Z1, Sash - 0.01), On(W, U0, Z1, Sash - 0.01), P.Glass, W.N);
				}
				else if (bAttic)
				{
					Pane(U0, G.U, Z0, Z1, false);
					Pane(G.U, U1, Z0, Z1, false);
				}
				else
				{
					Pane(U0, G.U, Z0, ZT, true);
					Pane(G.U, U1, Z0, ZT, true);
					Pane(U0, U1, ZT, Z1, false);
				}
				// (its sash in a T: the frame round it, its middle bar, its transom)
				const Col Sb = Scaled(Pt, 0.92);
				const double Bw = 0.022;
				WBox(B, W, U0 + Bw, 0.5 * (Z0 + Z1), Sash + 0.012, Bw, 0.02, 0.5 * (Z1 - Z0), Sb);
				WBox(B, W, U1 - Bw, 0.5 * (Z0 + Z1), Sash + 0.012, Bw, 0.02, 0.5 * (Z1 - Z0), Sb);
				WBox(B, W, G.U, 0.5 * (Z0 + Z1), Sash + 0.012, 0.6 * Bw, 0.02, 0.5 * (Z1 - Z0), Sb);
				WBox(B, W, G.U, Z1 - Bw, Sash + 0.012, 0.5 * G.W, 0.02, Bw, Sb);
				WBox(B, W, G.U, Z0 + Bw, Sash + 0.012, 0.5 * G.W, 0.02, Bw, Sb);
				if (!bAttic) WBox(B, W, G.U, ZT, Sash + 0.012, 0.5 * G.W, 0.02, 0.6 * Bw, Sb);
				// (its carved frame on the logs: the side boards, the frieze and its cornice, the pediment with its diamond, the
				// sill and the apron under it with its scallops)
				const double Fd = F + 0.012, Sw = bAttic ? 0.09 : 0.12;
				WBox(B, W, U0 - 0.5 * Sw, 0.5 * (Z0 + Z1), Fd, 0.5 * Sw, 0.013, 0.5 * (Z1 - Z0) + 0.03, Worn(P, Pt, 0.3, Seed ^ 0xF1));
				WBox(B, W, U1 + 0.5 * Sw, 0.5 * (Z0 + Z1), Fd, 0.5 * Sw, 0.013, 0.5 * (Z1 - Z0) + 0.03, Worn(P, Pt, 0.3, Seed ^ 0xF2));
				const double Fh = bAttic ? 0.1 : 0.15, Ew = bAttic ? 0.12 : 0.2;
				WBox(B, W, G.U, Z1 + 0.03 + 0.5 * Fh, Fd + 0.004, 0.5 * G.W + Ew, 0.015, 0.5 * Fh, Pt);
				// (the frieze's carving: a band of small diamonds in the other paint)
				const int ND = bAttic ? 3 : 5;
				for (int K = 0; K < ND; ++K)
				{
					const double Uk = U0 - Ew + 0.1 + (G.W + 2.0 * Ew - 0.2) * (K + 0.5) / ND, Zk = Z1 + 0.03 + 0.5 * Fh, Rk = 0.38 * Fh;
					B.Quad1(On(W, Uk - Rk, Zk, Fd + 0.021), On(W, Uk, Zk - Rk, Fd + 0.021), On(W, Uk + Rk, Zk, Fd + 0.021), On(W, Uk, Zk + Rk, Fd + 0.021), Cv, W.N);
				}
				const double Zc0 = Z1 + 0.03 + Fh, Cw = 0.5 * G.W + Ew + 0.05;
				WBox(B, W, G.U, Zc0 + 0.022, Fd + 0.03, Cw, 0.045, 0.022, Scaled(Pt, 1.02), 4);
				// (teeth hung under the cornice)
				const int NTe = static_cast<int>(2.0 * Cw / 0.07);
				for (int K = 0; K < NTe; ++K)
				{
					const double Ua = G.U - Cw + 2.0 * Cw * K / NTe, Ub = G.U - Cw + 2.0 * Cw * (K + 1) / NTe;
					B.Tri1(On(W, Ua, Zc0, Fd + 0.07), On(W, Ub, Zc0, Fd + 0.07), On(W, 0.5 * (Ua + Ub), Zc0 - 0.035, Fd + 0.07), Pt, W.N);
				}
				const double Ph = bAttic ? 0.14 : 0.22, Pz = Zc0 + 0.044;
				const V3 PA = On(W, G.U - Cw + 0.04, Pz, Fd + 0.02), PB = On(W, G.U + Cw - 0.04, Pz, Fd + 0.02), PC = On(W, G.U, Pz + Ph, Fd + 0.02);
				B.Tri1(PA, PB, PC, Pt, W.N);
				B.Quad1(PA, PC, PC + W.N * -0.03, PA + W.N * -0.03, Scaled(Pt, 0.9), Cross(W.N, PC - PA) * -1.0 + Up * 0.1);
				B.Quad1(PB, PC, PC + W.N * -0.03, PB + W.N * -0.03, Scaled(Pt, 0.9), Cross(PC - PB, W.N) * -1.0 + Up * 0.1);
				{
					const double Zd = Pz + 0.38 * Ph, Rd = 0.26 * Ph;
					B.Quad1(On(W, G.U - 1.3 * Rd, Zd, Fd + 0.022), On(W, G.U, Zd - Rd, Fd + 0.022), On(W, G.U + 1.3 * Rd, Zd, Fd + 0.022), On(W, G.U, Zd + Rd, Fd + 0.022), Cv, W.N);
				}
				// (the pediment's own little cornice along its rakes, a ridge of paint)
				WBox(B, W, G.U - 0.5 * (Cw - 0.04), Pz + 0.5 * Ph, Fd + 0.03, 0.5 * (Cw - 0.04) + 0.02, 0.02, 0.01, Scaled(Pt, 0.95), 4);
				if (bAttic) return;
				// sill, apron, scallops (the paint flaked most from the sill, where the wet lies, and the apron under it)
				WBox(B, W, G.U, Z0 - 0.018, F + 0.035, 0.5 * G.W + 0.1, 0.05, 0.018, Worn(P, Pt, 1.0, Seed ^ 0xF3), 0);
				const double Ah = 0.12;
				const Col Ap = Worn(P, Pt, 0.7, Seed ^ 0xF4);
				WBox(B, W, G.U, Z0 - 0.036 - 0.5 * Ah, Fd, 0.5 * G.W + Sw, 0.013, 0.5 * Ah, Ap);
				Flakes(B, P, W, U0 - Sw, U1 + Sw, Z0 - 0.036 - Ah, Z0 - 0.036, Fd + 0.0135, 3, Seed ^ 0xF5);
				const int NS = 4;
				for (int K = 0; K < NS; ++K)
				{
					const double Ua = U0 - Sw + (G.W + 2.0 * Sw) * K / NS, Ub = U0 - Sw + (G.W + 2.0 * Sw) * (K + 1) / NS, Um = 0.5 * (Ua + Ub);
					const double Zb = Z0 - 0.036 - Ah, Lr = 0.5 * (Ub - Ua);
					const uint32_t Cn = B.Vert(On(W, Um, Zb, Fd + 0.013), W.N, Ap);
					std::vector<uint32_t> Rim;
					for (int J = 0; J <= 4; ++J)
					{
						const double Th = Pi * J / 4;
						Rim.push_back(B.Vert(On(W, Um - Lr * std::cos(Th), Zb - 0.55 * Lr * std::sin(Th), Fd + 0.013), W.N, Ap));
					}
					for (int J = 0; J < 4; ++J) B.Tri(Cn, Rim[static_cast<size_t>(J)], Rim[static_cast<size_t>(J) + 1], W.N);
				}
				// shutters: open flat on the logs either side (or shut across the window), each with its painted diamond, its
				// rails and its strap hinges
				const double Lw = 0.5 * G.W + 0.01, Lh = Z1 - Z0 + 0.04, Sd = F + 0.035;
				const Col Sc = Varied(P.Shutter, 0.95 + 0.08 * Unit(Seed ^ 0x51), 0.0);
				for (int Side = -1; Side <= 1; Side += 2)
				{
					double Um;
					double Dd = Sd;
					if (G.bShut) Um = G.U + Side * 0.5 * Lw, Dd = F + 0.03;
					else Um = (Side < 0 ? U0 - Sw - 0.5 * Lw : U1 + Sw + 0.5 * Lw);
					const double Zm = 0.5 * (Z0 + Z1);
					const uint64_t Sh = Seed ^ (Side < 0 ? 0x5A0 : 0x5A1);
					WBox(B, W, Um, Zm, Dd, 0.5 * Lw - 0.004, 0.012, 0.5 * Lh, Worn(P, Sc, 0.25, Sh));
					WBox(B, W, Um, Zm + 0.5 * Lh - 0.04, Dd + 0.014, 0.5 * Lw - 0.004, 0.005, 0.03, Scaled(Worn(P, Sc, 0.3, Sh ^ 1), 0.9));
					WBox(B, W, Um, Zm - 0.5 * Lh + 0.04, Dd + 0.014, 0.5 * Lw - 0.004, 0.005, 0.03, Scaled(Worn(P, Sc, 0.9, Sh ^ 2), 0.9));
					const double Rd = 0.32 * Lw;
					B.Quad1(On(W, Um - Rd, Zm, Dd + 0.0125), On(W, Um, Zm - 1.5 * Rd, Dd + 0.0125), On(W, Um + Rd, Zm, Dd + 0.0125), On(W, Um, Zm + 1.5 * Rd, Dd + 0.0125), Worn(P, P.Mark, 0.2, Sh ^ 3), W.N);
					// (a painted shutter's paint flaked to the wood here and there, most toward its foot)
					if (!P.bBareShut) Flakes(B, P, W, Um - 0.5 * Lw, Um + 0.5 * Lw, Zm - 0.5 * Lh + 0.08, Zm - 0.1 * Lh, Dd + 0.0135, 3, Sh ^ 4);
					const double Hu = G.bShut ? G.U + Side * (Lw - 0.08) : (Side < 0 ? U0 - Sw - 0.08 : U1 + Sw + 0.08);
					for (double Zh : { Z0 + 0.12, Z1 - 0.12 })
						WBox(B, W, 0.5 * (Hu + Um), Zh, Dd + 0.015, 0.5 * std::fabs(Um - Hu) + 0.04, 0.004, 0.012, Rgb(40, 38, 36));
				}
			}

			/** The door: a ledged plank door in its frame, its ring, its sill board. */
			void Door(Pen& B, const Plan& P, const Gap& G)
			{
				const Wall W = WallOf(P, G.Face);
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, F = P.R;
				const Col Jamb = Scaled(P.Door, 0.85);
				B.Quad1(On(W, U0, G.Z0, -0.02), On(W, U0, G.Z1, -0.02), On(W, U0, G.Z1, F + 0.02), On(W, U0, G.Z0, F + 0.02), Jamb, W.T);
				B.Quad1(On(W, U1, G.Z0, -0.02), On(W, U1, G.Z1, -0.02), On(W, U1, G.Z1, F + 0.02), On(W, U1, G.Z0, F + 0.02), Jamb, W.T * -1.0);
				B.Quad1(On(W, U0, G.Z1, -0.02), On(W, U1, G.Z1, -0.02), On(W, U1, G.Z1, F + 0.02), On(W, U0, G.Z1, F + 0.02), Scaled(Jamb, 0.7), V3{ 0.0, 0.0, -1.0 });
				Rng Rd(P.S.Key ^ 0xD00F);
				const int N = 6;
				for (int K = 0; K < N; ++K)
				{
					const double Ua = U0 + G.W * K / N, Ub = U0 + G.W * (K + 1) / N;
					const Col C = Varied(P.Door, 0.88 + 0.2 * Rd.Next(), 0.0);
					WBox(B, W, 0.5 * (Ua + Ub), 0.5 * (G.Z0 + G.Z1), 0.0 + 0.004 * (K % 2), 0.5 * (Ub - Ua) - 0.003, 0.018, 0.5 * (G.Z1 - G.Z0), C);
				}
				for (double Zl : { G.Z0 + 0.25, G.Z1 - 0.3 })
					WBox(B, W, G.U, Zl, 0.03, 0.5 * G.W - 0.06, 0.012, 0.06, Scaled(P.Door, 0.9));
				const V3 Ring = On(W, U1 - 0.12, G.Z0 + 1.0, 0.05);
				B.Tube(Ring - W.N * 0.02, Ring + W.N * 0.01, 0.025, 0.025, 6, Rgb(36, 34, 32), true);
				// (its frame boards on the logs)
				WBox(B, W, U0 - 0.06, 0.5 * (G.Z0 + G.Z1), F + 0.012, 0.06, 0.013, 0.5 * (G.Z1 - G.Z0) + 0.06, P.Paint);
				WBox(B, W, U1 + 0.06, 0.5 * (G.Z0 + G.Z1), F + 0.012, 0.06, 0.013, 0.5 * (G.Z1 - G.Z0) + 0.06, P.Paint);
				WBox(B, W, G.U, G.Z1 + 0.09, F + 0.014, 0.5 * G.W + 0.15, 0.015, 0.07, P.Paint);
			}

			/** The porch before the door: its floor and steps, two posts with their bellies, the rails and balusters, its little
			 * gabled roof of planks with carved barge boards. */
			void Porch(Pen& B, const Plan& P, const Gap& G)
			{
				const Wall W = WallOf(P, G.Face);
				const double F = P.R, Zf = G.Z0 - 0.02, Hw = 0.82, Out = PorchOut;
				const Col Wd = Rgb(118, 104, 86), Wd2 = Rgb(104, 92, 78);
				// (its floor of planks across, on its sill logs)
				for (int K = 0; K < 5; ++K)
				{
					const double D0 = F + Out * K / 5, D1 = F + Out * (K + 1) / 5;
					WBox(B, W, G.U, Zf - 0.02, 0.5 * (D0 + D1), Hw, 0.5 * (D1 - D0) - 0.004, 0.022, Varied(Wd, 0.92 + 0.04 * (K % 3), 0.0), 0);
				}
				WBox(B, W, G.U, 0.5 * (Zf - 0.04), F + 0.5 * Out, Hw - 0.04, 0.5 * Out - 0.04, 0.5 * (Zf - 0.04), Scaled(Wd2, 0.7), 0);
				// (its steps down at its +x side, along the wall)
				const int Steps = PorchSteps(Zf);
				for (int K = 1; K <= Steps; ++K)
				{
					const double Ztop = Zf - Zf * K / (Steps + 1.0), Us = G.U + Hw + 0.13 + 0.26 * (K - 1);
					WBox(B, W, Us, 0.5 * Ztop, F + 0.5 * (Out - 0.05), 0.13, 0.5 * (Out - 0.05), 0.5 * Ztop, Varied(Wd, 0.9 + 0.05 * K, 0.0), 0);
				}
				// (its two posts at the floor's outer corners, a bulge carved midway up each)
				const double Zr = std::min(G.Z1 + 0.42, P.Zt - 0.15), Zp = Zr, Zrl = Zf + 0.85;
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const double Up0 = G.U + Side * (Hw - 0.07), Dp = F + Out - 0.07;
					WBox(B, W, Up0, 0.5 * (Zf + Zp), Dp, 0.055, 0.055, 0.5 * (Zp - Zf), Wd, 16);
					WBox(B, W, Up0, Zf + 0.55 * (Zp - Zf), Dp, 0.075, 0.075, 0.16, Varied(Wd, 1.04, 0.0), 0);
					WBox(B, W, Up0, Zf + 0.55 * (Zp - Zf) + 0.19, Dp, 0.065, 0.065, 0.03, Varied(Wd, 1.04, 0.0), 0);
					WBox(B, W, Up0, Zf + 0.55 * (Zp - Zf) - 0.19, Dp, 0.065, 0.065, 0.03, Varied(Wd, 1.04, 0.0), 0);
				}
				// (its rails: along its -x side and across its front, the flat balusters under them sawn to a shape; open at its
				// +x side to the steps)
				WBox(B, W, G.U - (Hw - 0.07), Zrl, F + 0.5 * (Out - 0.07), 0.035, 0.5 * (Out - 0.07), 0.03, Wd, 0);
				for (int K = 0; K < 4; ++K)
				{
					const double Db = F + 0.1 + (Out - 0.3) * K / 3.0;
					WBox(B, W, G.U - (Hw - 0.07), 0.5 * (Zf + Zrl), Db, 0.012, 0.045, 0.5 * (Zrl - Zf) - 0.03, Varied(P.Paint, 0.9, 0.0), 0);
					WBox(B, W, G.U - (Hw - 0.07), 0.5 * (Zf + Zrl), Db, 0.014, 0.065, 0.06, Varied(P.Paint, 0.94, 0.0), 0);
				}
				WBox(B, W, G.U, Zrl, F + Out - 0.07, Hw - 0.07, 0.035, 0.03, Wd, 0);
				for (int K = 0; K < 8; ++K)
				{
					const double Ub = G.U - Hw + 0.2 + (2.0 * Hw - 0.4) * K / 7.0;
					WBox(B, W, Ub, 0.5 * (Zf + Zrl), F + Out - 0.07, 0.045, 0.012, 0.5 * (Zrl - Zf) - 0.03, Varied(P.Paint, 0.9, 0.0), 0);
					WBox(B, W, Ub, 0.5 * (Zf + Zrl), F + Out - 0.07, 0.065, 0.014, 0.06, Varied(P.Paint, 0.94, 0.0), 0);
				}
				// (the beams the roof sits on, out from the wall to the posts' heads, and across them)
				for (int Side = -1; Side <= 1; Side += 2)
					WBox(B, W, G.U + Side * (Hw - 0.07), Zp + 0.05, F + 0.5 * (Out + 0.1), 0.06, 0.5 * (Out + 0.1), 0.05, Wd2, 0);
				WBox(B, W, G.U, Zp + 0.05, F + Out - 0.07, Hw + 0.02, 0.06, 0.05, Wd2, 0);
				// (its roof: two slopes of planks, the ridge running out from the wall)
				const double Rw = Hw + 0.2, Rise = 0.32, Ro = Out + 0.1, Zb = Zp + 0.1;
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const int N = 6;
					for (int K = 0; K < N; ++K)
					{
						const double D0 = F + Ro * K / N, D1 = F + Ro * (K + 1) / N;
						const Col C = Varied(P.Plank, 0.9 + 0.1 * ((K * 7 + Side) % 3) / 2.0, 0.0);
						const V3 A0 = On(W, G.U, Zb + Rise, D0), A1 = On(W, G.U, Zb + Rise, D1), B1 = On(W, G.U + Side * Rw, Zb, D1), B0 = On(W, G.U + Side * Rw, Zb, D0);
						B.Quad1(A0, A1, B1, B0, C, Up + W.T * (Side * 0.4));
						B.Quad1(A0 + V3{ 0, 0, -0.03 }, A1 + V3{ 0, 0, -0.03 }, B1 + V3{ 0, 0, -0.03 }, B0 + V3{ 0, 0, -0.03 }, Scaled(Wd2, 0.6), V3{ 0, 0, -1 });
					}
					const V3 E0 = On(W, G.U + Side * Rw, Zb, F), E1 = On(W, G.U + Side * Rw, Zb, F + Ro);
					B.Quad1(E0, E1, E1 + V3{ 0, 0, -0.03 }, E0 + V3{ 0, 0, -0.03 }, Scaled(P.Plank, 0.7), W.T * static_cast<double>(Side));
					// (its barge board at the open end, teeth along its foot)
					const V3 Ta = On(W, G.U, Zb + Rise - 0.02, F + Ro + 0.01), Tb = On(W, G.U + Side * Rw, Zb - 0.02, F + Ro + 0.01);
					const V3 Dn{ 0.0, 0.0, -0.16 };
					B.Quad1(Ta, Tb, Tb + Dn, Ta + Dn, P.Paint, W.N);
					for (int K = 0; K < 7; ++K)
					{
						const V3 Pa = Ta + (Tb - Ta) * (K / 7.0) + Dn, Pb = Ta + (Tb - Ta) * ((K + 1) / 7.0) + Dn;
						B.Tri1(Pa, Pb, (Pa + Pb) * 0.5 + V3{ 0, 0, -0.06 }, P.Paint, W.N);
					}
				}
			}

			/** A gable's boards over its last log, up under the roof: boards and battens, its attic window cut in, a cornice
			 * along its foot. */
			void Gable(Pen& B, const Plan& P, int F, uint64_t Seed)
			{
				const Wall W = WallOf(P, F);
				const double Df = P.R + 0.01;
				const double Reach = (P.Ridge - P.Thick / P.CosP - P.Zg) / P.Tan; // (out from the ridge to where its rake meets its foot)
				const Gap* At = nullptr;
				for (const Gap& G : P.Gaps)
					if (G.Face == F && G.What == 2) At = &G;
				Rng Rb(Seed);
				const double Bw = 0.2;
				const int N = static_cast<int>(std::ceil(2.0 * Reach / Bw));
				const double Zf = P.Zg - 0.04;
				auto Rake = [&](double U) { return UnderAt(P, std::fabs(U)) - 0.01; };
				for (int K = 0; K < N; ++K)
				{
					const double U0 = -Reach + 2.0 * Reach * K / N, U1 = -Reach + 2.0 * Reach * (K + 1) / N;
					const double Tn = Rb.Next(), Hu = Rb.Next();
					const Col C = Mixed(Varied(P.Plank, 0.86 + 0.2 * Tn, 0.03 * (Hu - 0.5)), Rgb(82, 76, 68), 0.2);
					auto Piece = [&](double Za0, double Za1, double Zb0, double Zb1) {
						if (Za1 - Za0 < 0.02 && Zb1 - Zb0 < 0.02) return;
						B.Quad(On(W, U0, Za0, Df), On(W, U1, Zb0, Df), On(W, U1, Zb1, Df), On(W, U0, Za1, Df), Scaled(C, 0.86), Scaled(C, 0.86), C, C, W.N);
					};
					const double T0 = std::max(Zf, Rake(U0)), T1 = std::max(Zf, Rake(U1));
					if (At && U1 > At->U - 0.5 * At->W - 0.1 && U0 < At->U + 0.5 * At->W + 0.1)
					{
						Piece(Zf, At->Z0 - 0.05, Zf, At->Z0 - 0.05);
						Piece(At->Z1 + 0.32, T0, At->Z1 + 0.32, T1);
					}
					else Piece(Zf, T0, Zf, T1);
					// (a batten over its joint)
					if (K > 0)
					{
						const double Tb = Rake(U0);
						if (Tb > Zf + 0.05)
						{
							const bool bCut = At && std::fabs(U0 - At->U) < 0.5 * At->W + 0.1;
							if (bCut)
							{
								WBox(B, W, U0, 0.5 * (Zf + At->Z0 - 0.05), Df + 0.014, 0.025, 0.012, 0.5 * (At->Z0 - 0.05 - Zf), Scaled(C, 0.95), 4);
								if (Tb > At->Z1 + 0.35) WBox(B, W, U0, 0.5 * (At->Z1 + 0.32 + Tb), Df + 0.014, 0.025, 0.012, 0.5 * (Tb - At->Z1 - 0.32), Scaled(C, 0.95), 4);
							}
							else WBox(B, W, U0, 0.5 * (Zf + Tb), Df + 0.014, 0.025, 0.012, 0.5 * (Tb - Zf), Scaled(C, 0.95), 4);
						}
					}
				}
				// (the cornice along its foot, between the long walls' top logs where their ends come through)
				const double Cu = W.Half - P.R - 0.01;
				WBox(B, W, 0.0, P.Zg + 0.03, P.R + 0.05, Cu, 0.05, 0.06, Scaled(P.Plank, 0.95), 4);
				B.Quad1(On(W, -Cu, P.Zg + 0.09, P.R), On(W, Cu, P.Zg + 0.09, P.R), On(W, Cu, P.Zg + 0.09, P.R + 0.1), On(W, -Cu, P.Zg + 0.09, P.R + 0.1), P.Plank, Up);
				if (At) Window(B, P, *At, Seed ^ 0xA77);
			}

			/** A folk sun rosette cut in a board (facing N, Sd its sideways): a ring, and six petals inside it, each a lens
			 * from the middle to the ring, as drawn with compasses; Rr the ring's outer radius. */
			void Rosette(Pen& B, V3 O, V3 N, V3 Sd, double Rr, const Col& C, const Col& Ground)
			{
				const V3 Upw = Cross(N, Sd) * (Dot(Cross(N, Sd), Up) < 0.0 ? -1.0 : 1.0);
				auto At = [&](double A, double R) { return O + Sd * (R * std::cos(A)) + Upw * (R * std::sin(A)); };
				const int NR = 18;
				const double Ri = 0.8 * Rr;
				for (int J = 0; J < NR; ++J)
				{
					const double A0 = 2.0 * Pi * J / NR, A1 = 2.0 * Pi * (J + 1) / NR;
					B.Quad1(At(A0, Ri), At(A1, Ri), At(A1, Rr), At(A0, Rr), C, N);
				}
				const double Pr = 0.74 * Rr;
				for (int K = 0; K < 6; ++K)
				{
					const double A = Pi / 2.0 + Pi * K / 3.0;
					// (a petal: the lens between two arcs, its tips at the middle and at the ring; four steps along it)
					const V3 Ax = Sd * std::cos(A) + Upw * std::sin(A), Ac = Sd * -std::sin(A) + Upw * std::cos(A);
					const uint32_t T0 = B.Vert(O + N * 0.001, N, C), T1 = B.Vert(O + Ax * Pr + N * 0.001, N, C);
					uint32_t L[3], R[3];
					for (int J = 0; J < 3; ++J)
					{
						const double T = (J + 1) / 4.0, W = 0.36 * Pr * std::pow(std::sin(Pi * T), 0.7);
						L[J] = B.Vert(O + Ax * (Pr * T) + Ac * W + N * 0.001, N, C);
						R[J] = B.Vert(O + Ax * (Pr * T) - Ac * W + N * 0.001, N, C);
					}
					B.Tri(T0, L[0], R[0], N);
					for (int J = 0; J < 2; ++J)
					{
						B.Tri(L[J], R[J], R[J + 1], N);
						B.Tri(L[J], R[J + 1], L[J + 1], N);
					}
					B.Tri(L[2], R[2], T1, N);
				}
				// (its middle: a round boss, the board's paint)
				const uint32_t Bc = B.Vert(O + N * 0.002, N, Ground);
				for (int J = 0; J < 8; ++J)
					B.Tri(Bc, B.Vert(At(2.0 * Pi * J / 8, 0.13 * Rr) + N * 0.002, N, Ground), B.Vert(At(2.0 * Pi * (J + 1) / 8, 0.13 * Rr) + N * 0.002, N, Ground), N);
			}

			/** The barge boards along a gable's two rakes (carved teeth along their feet) and the towel board at its apex. */
			void Barge(Pen& B, const Plan& P, int End)
			{
				const double X = End * (P.Xr + 0.015);
				const V3 Nx{ static_cast<double>(End), 0.0, 0.0 };
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const V3 Ta = RoofAt(P, Sd, X, 0.0, -0.02), Tb = RoofAt(P, Sd, X, P.Qe + 0.02, -0.02);
					const V3 Dn{ 0.0, 0.0, -0.24 };
					B.Box((Ta + Tb + Dn) * 0.5, (Tb - Ta) * 0.5, Nx * 0.018, Dn * 0.5, P.Paint, 0);
					const int NT = static_cast<int>(std::floor(Dot(Tb - Ta, Tb - Ta) > 0 ? std::sqrt(Dot(Tb - Ta, Tb - Ta)) / 0.12 : 1));
					for (int K = 0; K < NT; ++K)
					{
						const V3 Pa = Ta + (Tb - Ta) * (static_cast<double>(K) / NT) + Dn, Pb = Ta + (Tb - Ta) * (static_cast<double>(K + 1) / NT) + Dn;
						const V3 Pc = (Pa + Pb) * 0.5 + V3{ 0.0, 0.0, -0.065 };
						B.Tri1(Pa + Nx * 0.018, Pb + Nx * 0.018, Pc + Nx * 0.018, P.Paint, Nx);
						B.Tri1(Pa - Nx * 0.018, Pb - Nx * 0.018, Pc - Nx * 0.018, Scaled(P.Paint, 0.85), Nx * -1.0);
					}
				}
				// (the towel board, hung at the apex, its foot cut in three teeth, a sun carved on it)
				const V3 Ap = V3{ X + End * 0.02, P.Yc, P.Ridge - 0.08 };
				const double Tw = 0.13, Tl = 0.95;
				B.Box(Ap + V3{ 0.0, 0.0, -0.5 * Tl }, Nx * 0.016, V3{ 0.0, Tw, 0.0 }, V3{ 0.0, 0.0, 0.5 * Tl }, P.Paint, 0);
				for (int K = 0; K < 3; ++K)
				{
					const double Ya = P.Yc - Tw + 2.0 * Tw * K / 3.0, Yb = P.Yc - Tw + 2.0 * Tw * (K + 1) / 3.0;
					B.Tri1(V3{ Ap.X + End * 0.017, Ya, Ap.Z - Tl }, V3{ Ap.X + End * 0.017, Yb, Ap.Z - Tl }, V3{ Ap.X + End * 0.017, 0.5 * (Ya + Yb), Ap.Z - Tl - 0.09 }, P.Paint, Nx);
				}
				// (the sun: a folk rosette, six petals in a ring, cut in; never a star)
				Rosette(B, Ap + V3{ End * 0.018, 0.0, -0.42 }, Nx, V3{ 0.0, 1.0, 0.0 }, 0.1, P.Carve, P.Paint);
			}

			/** The roof of planks: two layers, the upper's over the lower's joints and standing proud, each plank warped a
			 * little, its foot ragged at the eaves, lichen and moss on them (more on the back); boards along the ridge under the
			 * ridge log, carved at its +x end into a horse's head. */
			void Planks(Pen& B, const Plan& P)
			{
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const V3 Nr = RoofN(P, Sd);
					for (const RoofPlank& Pk : PlanksOf(P, Sd))
					{
						const bool bUpper = Pk.bUpper;
						const double Xa = Pk.Xa, Xb = Pk.Xb, Foot = Pk.Foot, Lift = Pk.Lift, X = Xa;
						const uint64_t Seed = Pk.Seed;
						const Col C = Pk.C;
						auto Paint = [&](double Xp, double Q) { return PlankCol(P, Sd, C, Seed, Xp, Q); };
						const int NQ = 4;
						for (int I = 0; I < NQ; ++I)
						{
							const double Q0 = 0.05 + (Foot - 0.05) * I / NQ, Q1 = 0.05 + (Foot - 0.05) * (I + 1) / NQ;
							const double W0 = Lift + 0.008 * Noise(Q0 / 0.9, X, Seed), W1 = Lift + 0.008 * Noise(Q1 / 0.9, X, Seed);
							const V3 A0 = RoofAt(P, Sd, Xa, Q0, W0), A1 = RoofAt(P, Sd, Xb, Q0, W0), A2 = RoofAt(P, Sd, Xb, Q1, W1), A3 = RoofAt(P, Sd, Xa, Q1, W1);
							B.Quad(A0, A1, A2, A3, Paint(Xa, Q0), Paint(Xb, Q0), Paint(Xb, Q1), Paint(Xa, Q1), Nr);
							if (bUpper)
							{
								B.Quad1(A0, A3, RoofAt(P, Sd, Xa, Q1, 0.0), RoofAt(P, Sd, Xa, Q0, 0.0), Scaled(C, 0.7), V3{ -1.0, 0.0, 0.0 });
								B.Quad1(A1, A2, RoofAt(P, Sd, Xb, Q1, 0.0), RoofAt(P, Sd, Xb, Q0, 0.0), Scaled(C, 0.7), V3{ 1.0, 0.0, 0.0 });
							}
						}
						// (its foot, the plank's end grain, a little dark)
						const V3 E0 = RoofAt(P, Sd, Xa, Foot, Lift), E1 = RoofAt(P, Sd, Xb, Foot, Lift);
						B.Quad1(E0, E1, RoofAt(P, Sd, Xb, Foot, Lift - 0.03), RoofAt(P, Sd, Xa, Foot, Lift - 0.03), Scaled(C, 0.6), V3{ 0.0, Sd * P.CosP, -P.SinP });
					}
					// (the roof's underside out past the walls: the eaves', and the gables' over the logs' ends)
					const Col Un = Rgb(62, 56, 48);
					const double Qw = (P.Bh + P.R) / P.CosP;
					B.Quad1(RoofAt(P, Sd, -P.Xr, Qw, -P.Thick), RoofAt(P, Sd, P.Xr, Qw, -P.Thick), RoofAt(P, Sd, P.Xr, P.Qe, -0.04), RoofAt(P, Sd, -P.Xr, P.Qe, -0.04), Un, Nr * -1.0);
					for (int End = -1; End <= 1; End += 2)
					{
						const double Xi = End * (P.A + P.R), Xo = End * P.Xr;
						B.Quad1(RoofAt(P, Sd, Xi, 0.0, -0.04), RoofAt(P, Sd, Xo, 0.0, -0.04), RoofAt(P, Sd, Xo, Qw, -0.04), RoofAt(P, Sd, Xi, Qw, -0.04), Un, Nr * -1.0);
					}
					// (the eaves' fascia board)
					const V3 F0 = RoofAt(P, Sd, -P.Xr, P.Qe - 0.02, -0.03), F1 = RoofAt(P, Sd, P.Xr, P.Qe - 0.02, -0.03);
					B.Quad1(F0, F1, F1 + V3{ 0.0, 0.0, -0.14 }, F0 + V3{ 0.0, 0.0, -0.14 }, Scaled(P.Plank, 0.85), V3{ 0.0, static_cast<double>(Sd), 0.0 });
					// (the ridge's boards, either side)
					B.Box((RoofAt(P, Sd, 0.0, 0.0, 0.05) + RoofAt(P, Sd, 0.0, 0.24, 0.05)) * 0.5, V3{ P.Xr + 0.02, 0.0, 0.0 }, (RoofAt(P, Sd, 0.0, 0.24, 0.05) - RoofAt(P, Sd, 0.0, 0.0, 0.05)) * 0.5, Nr * 0.016, Varied(P.Plank, 0.92, 0.0), 0);
				}
				// (the ridge log, split, its round up; its +x end a horse's head)
				const uint64_t Seed = Hash(P.S.Key, 0x0C47);
				Plan Q = P;
				Q.R = 0.13;
				Round(B, Q, V3{ 0.0, P.Yc, P.Ridge + 0.07 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, -P.Xr - 0.08, P.Xr + 0.06, 0.0, Pi, 6, Seed, 0.9);
				EndGrain(B, Q, V3{ 0.0, P.Yc, P.Ridge + 0.07 }, V3{ -1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, P.Xr + 0.08, Seed ^ 1);
				const Col Hc = Varied(P.Log, 1.05, 0.0);
				auto Piece = [&](std::initializer_list<std::pair<double, double>> Poly) {
					std::vector<std::pair<double, double>> Pp(Poly);
					const V3 O{ P.Xr + 0.04, P.Yc, P.Ridge + 0.02 };
					auto At = [&](size_t I, double S) { return O + V3{ Pp[I].first, S * 0.055, Pp[I].second }; };
					double Cx = 0.0, Cz = 0.0;
					for (const auto& Pq : Pp)
					{
						Cx += Pq.first / Pp.size();
						Cz += Pq.second / Pp.size();
					}
					for (size_t I = 1; I + 1 < Pp.size(); ++I)
					{
						B.Tri1(At(0, 1.0), At(I, 1.0), At(I + 1, 1.0), Hc, V3{ 0.0, 1.0, 0.0 });
						B.Tri1(At(0, -1.0), At(I, -1.0), At(I + 1, -1.0), Scaled(Hc, 0.9), V3{ 0.0, -1.0, 0.0 });
					}
					for (size_t I = 0; I < Pp.size(); ++I)
					{
						const size_t J = (I + 1) % Pp.size();
						const V3 Mid = (At(I, 0.0) + At(J, 0.0)) * 0.5;
						B.Quad1(At(I, 1.0), At(J, 1.0), At(J, -1.0), At(I, -1.0), Scaled(Hc, 0.92), Mid - (O + V3{ Cx, 0.0, Cz }));
					}
				};
				Piece({ { -0.05, -0.02 }, { 0.3, -0.02 }, { 0.5, 0.2 }, { 0.22, 0.32 }, { -0.05, 0.14 } });
				Piece({ { 0.42, 0.13 }, { 0.66, 0.2 }, { 0.73, 0.3 }, { 0.6, 0.39 }, { 0.43, 0.36 } });
				Piece({ { 0.46, 0.34 }, { 0.53, 0.5 }, { 0.57, 0.36 } });
			}

			/** The roof of thatch: courses of straw, each lapping the one below with its butt end showing, tufts sticking out
			 * of it, darker and mossy toward the eaves and on the back; a thick ragged eave; a straw ridge held by crossed
			 * poles; its verges at the gables rolled thick. */
			void Thatch(Pen& B, const Plan& P)
			{
				// Aged rye straw combed down the slope, each course's butts barely proud of the next; the eaves cut in a clean
				// line, their top rounded over into the cut; the verges rolled round (art director, 24 Nov: soft, never slabs).
				const double Step = 0.3, Cell = 0.15, Lip = 0.016, Rl = 0.12; // a course; a cell across; a butt's rise; the rolls' radius
				const double Qt = P.Qe - Rl, Xt = P.Xr - Rl;                  // where the top runs out into the rolls
				const int NQ = static_cast<int>(std::ceil(Qt / Step)), NX = static_cast<int>(std::ceil(2.0 * Xt / Cell)), NE = 3, NB = 3;
				const double Ae = 0.5 * Pi - std::atan2(P.SinP, P.CosP); // (the eaves' roll: from the slope's normal round to the level)
				const double Qw = (P.Bh + P.R) / P.CosP;                  // (the underside back to the wall)
				// (the butts' rise dies away toward the verges, the roll round them smooth)
				auto Lift = [&](double X, double Q, double Fr) { return 0.01 * Noise(X / 0.4, Q / 0.3, P.S.Key ^ 0x57A3) + Lip * Fr * Smooth(Xt, Xt - 0.35, std::fabs(X)); };
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const V3 Nr = RoofN(P, Sd), Dn{ 0.0, Sd * P.CosP, -P.SinP }, Ho{ 0.0, static_cast<double>(Sd), 0.0 };
					const V3 R0 = RoofAt(P, Sd, 0.0, 0.0, 0.0);
					auto Straw = [&](double X, double Q) { return StrawCol(P, Sd, X, Q); };
					// (how far straight down from a point to the underside)
					auto Drop = [&](V3 E) { return (Dot(E - R0, Nr) + P.Thick) / P.CosP; };
					auto Cut = [&](double X, double Z) { return Scaled(Mixed(Straw(X, P.Qe), Rgb(150, 136, 108), 0.3), 0.74 + 0.12 * Noise01(X / 0.05, Z / 0.4, P.S.Key ^ 0x57A6)); };
					// (the top, course by course, each course's straws a little prouder toward its butt)
					for (int I = 0; I < NQ; ++I)
					{
						const double Q0 = Qt * I / NQ, Q1 = Qt * (I + 1) / NQ, F1 = I + 1 < NQ ? 1.0 : 0.0;
						for (int J = 0; J < NX; ++J)
						{
							const double X0 = -Xt + 2.0 * Xt * J / NX, X1 = -Xt + 2.0 * Xt * (J + 1) / NX;
							const V3 A0 = RoofAt(P, Sd, X0, Q0, Lift(X0, Q0, 0.0)), A1 = RoofAt(P, Sd, X1, Q0, Lift(X1, Q0, 0.0));
							const V3 A2 = RoofAt(P, Sd, X1, Q1, Lift(X1, Q1, F1)), A3 = RoofAt(P, Sd, X0, Q1, Lift(X0, Q1, F1));
							B.Quad(A0, A1, A2, A3, Straw(X0, Q0), Straw(X1, Q0), Straw(X1, Q1), Straw(X0, Q1), Nr);
							if (I + 1 < NQ)
							{
								const V3 B2 = RoofAt(P, Sd, X1, Q1, Lift(X1, Q1, 0.0)), B3 = RoofAt(P, Sd, X0, Q1, Lift(X0, Q1, 0.0));
								const Col Ca = Scaled(Straw(X0, Q1), 0.84), Cb = Scaled(Straw(X1, Q1), 0.84);
								B.Quad(A3, A2, B2, B3, Ca, Cb, Cb, Ca, Dn);
							}
							// (its straws combed down: long thin wisps lying along the slope, paler and darker)
							Rng Rt(Hash(P.S.Key ^ 0x7F7, static_cast<uint64_t>((I * 128 + J) * 2 + (Sd > 0))));
							for (int K = 0; K < 2; ++K)
							{
								const double Xs = Rt.Range(X0, X1), Qs = Rt.Range(Q0, Q1 - 0.08), Ln = Rt.Range(0.12, 0.3), Wd = Rt.Range(0.006, 0.013);
								const double Dx = Rt.Range(-0.02, 0.02), Pale = Rt.Range(0.0, 0.4), Tone = Rt.Range(0.8, 1.1);
								const double Qf = std::min(Q1 - 0.01, Qs + Ln);
								const V3 Bt = RoofAt(P, Sd, Xs, Qs, Lift(Xs, Qs, F1 * (Qs - Q0) / (Q1 - Q0)) + 0.004);
								const V3 Tt = RoofAt(P, Sd, Xs + Dx, Qf, Lift(Xs + Dx, Qf, F1 * (Qf - Q0) / (Q1 - Q0)) + 0.004);
								B.Tri1(Bt + V3{ -Wd, 0.0, 0.0 }, Bt + V3{ Wd, 0.0, 0.0 }, Tt, Scaled(Mixed(Straw(Xs, Qs), Rgb(178, 160, 124), Pale), Tone), Nr);
							}
						}
					}
					// (the eaves: the top rolled over into the cut, the cut straight down to a clean foot, the underside dark)
					for (int J = 0; J < NX; ++J)
					{
						const double Xa = -Xt + 2.0 * Xt * J / NX, Xb = -Xt + 2.0 * Xt * (J + 1) / NX;
						const V3 Ca = RoofAt(P, Sd, Xa, Qt, Lift(Xa, Qt, 0.0)) - Nr * Rl, Cb = RoofAt(P, Sd, Xb, Qt, Lift(Xb, Qt, 0.0)) - Nr * Rl;
						for (int K = 0; K < NE; ++K)
						{
							const double An0 = Ae * K / NE, An1 = Ae * (K + 1) / NE;
							const V3 D0 = Nr * std::cos(An0) + Dn * std::sin(An0), D1 = Nr * std::cos(An1) + Dn * std::sin(An1);
							const Col Sa = Mixed(Straw(Xa, P.Qe), Cut(Xa, 0.0), static_cast<double>(K + 1) / NE), Sb = Mixed(Straw(Xb, P.Qe), Cut(Xb, 0.0), static_cast<double>(K + 1) / NE);
							B.Quad(Ca + D0 * Rl, Cb + D0 * Rl, Cb + D1 * Rl, Ca + D1 * Rl, Sa, Sb, Sb, Sa, (D0 + D1) * 0.5);
						}
						const V3 Ea = Ca + Ho * Rl, Eb = Cb + Ho * Rl, Fa = Ea - Up * Drop(Ea), Fb = Eb - Up * Drop(Eb);
						B.Quad(Ea, Eb, Fb, Fa, Cut(Xa, Ea.Z), Cut(Xb, Eb.Z), Scaled(Cut(Xb, Fb.Z), 0.8), Scaled(Cut(Xa, Fa.Z), 0.8), Ho);
						B.Quad1(Fa, Fb, RoofAt(P, Sd, Xb, Qw, -P.Thick), RoofAt(P, Sd, Xa, Qw, -P.Thick), Rgb(58, 52, 44), Nr * -1.0);
					}
					// (the verges: the top rolled round over each gable's edge into a cut face along the rake, course by course)
					for (int End = -1; End <= 1; End += 2)
					{
						const V3 Xh{ static_cast<double>(End), 0.0, 0.0 };
						const double Xe = End * Xt;
						auto Ring = [&](double Q, double Fr, std::vector<V3>& Out) {
							Out.clear();
							const V3 C = RoofAt(P, Sd, Xe, Q, Lift(Xe, Q, Fr)) - Nr * Rl;
							for (int K = 0; K <= NB; ++K)
							{
								const double An = 0.5 * Pi * K / NB;
								Out.push_back(C + (Nr * std::cos(An) + Xh * std::sin(An)) * Rl);
							}
						};
						std::vector<V3> Ra, Rb;
						for (int I = 0; I < NQ; ++I)
						{
							const double Q0 = Qt * I / NQ, Q1 = Qt * (I + 1) / NQ, F1 = I + 1 < NQ ? 1.0 : 0.0;
							Ring(Q0, 0.0, Ra);
							Ring(Q1, F1, Rb);
							const Col Sa = Straw(Xe, Q0), Sb = Straw(Xe, Q1);
							for (int K = 0; K < NB; ++K)
							{
								const double An = 0.5 * Pi * (K + 0.5) / NB;
								const double Kf = static_cast<double>(K + 1) / NB;
								B.Quad(Ra[static_cast<size_t>(K)], Rb[static_cast<size_t>(K)], Rb[static_cast<size_t>(K) + 1], Ra[static_cast<size_t>(K) + 1], Sa, Sb, Scaled(Sb, 1.0 - 0.15 * Kf), Scaled(Sa, 1.0 - 0.15 * Kf), Nr * std::cos(An) + Xh * std::sin(An));
							}
							const V3 Ea = Ra.back(), Eb = Rb.back(), Fa = Ea - Up * Drop(Ea), Fb = Eb - Up * Drop(Eb);
							B.Quad(Ea, Eb, Fb, Fa, Scaled(Sa, 0.8), Scaled(Sb, 0.8), Scaled(Sb, 0.66), Scaled(Sa, 0.66), Xh);
							B.Quad1(Fa, Fb, Fb - Xh * (P.OvG + 0.06), Fa - Xh * (P.OvG + 0.06), Rgb(58, 52, 44), V3{ 0.0, 0.0, -1.0 });
						}
						// (the corner, where the eaves' roll and the verge's meet: a quarter of a ball, and its cut below)
						const V3 C = RoofAt(P, Sd, Xe, Qt, Lift(Xe, Qt, 0.0)) - Nr * Rl;
						auto Dir = [&](double A, double Bb) { return (Nr * std::cos(A) + Dn * std::sin(A)) * std::cos(Bb) + Xh * std::sin(Bb); };
						for (int K = 0; K < NE; ++K)
							for (int L = 0; L < NB; ++L)
							{
								const double A0 = Ae * K / NE, A1 = Ae * (K + 1) / NE, B0 = 0.5 * Pi * L / NB, B1 = 0.5 * Pi * (L + 1) / NB;
								const Col Cc = Mixed(Straw(Xe, P.Qe), Cut(Xe, 0.0), (K + 0.5) / NE);
								B.Quad1(C + Dir(A0, B0) * Rl, C + Dir(A1, B0) * Rl, C + Dir(A1, B1) * Rl, C + Dir(A0, B1) * Rl, Cc, Dir(0.5 * (A0 + A1), 0.5 * (B0 + B1)));
							}
						for (int L = 0; L < NB; ++L)
						{
							const double B0 = 0.5 * Pi * L / NB, B1 = 0.5 * Pi * (L + 1) / NB;
							const V3 Ea = C + Dir(Ae, B0) * Rl, Eb = C + Dir(Ae, B1) * Rl, Fa = Ea - Up * Drop(Ea), Fb = Eb - Up * Drop(Eb);
							B.Quad1(Ea, Eb, Fb, Fa, Cut(Xe, Ea.Z), Dir(Ae, 0.5 * (B0 + B1)));
						}
					}
				}
				// (the ridge: a roll of straw, and the crossed poles pegged over it)
				{
					const int NXr = static_cast<int>(std::ceil((2.0 * P.Xr + 0.1) / 0.3)), NA = 7;
					const V3 Rc{ 0.0, P.Yc, P.Ridge + 0.02 };
					std::vector<uint32_t> Prev, Cur;
					for (int I = 0; I <= NXr; ++I)
					{
						const double X = -P.Xr - 0.05 + (2.0 * P.Xr + 0.1) * I / NXr;
						Cur.clear();
						for (int J = 0; J <= NA; ++J)
						{
							const double An = -0.12 * Pi + 1.24 * Pi * J / NA;
							const double Rr = 0.2 * (1.0 + 0.08 * Noise(X / 0.4, J * 0.7, P.S.Key ^ 0x2D6E));
							const V3 Rd{ 0.0, std::cos(An), std::sin(An) };
							const Col C = Scaled(Mixed(P.Thatch, Rgb(112, 104, 90), 0.35), 0.88 + 0.22 * Noise01(X / 0.15, J * 0.9, P.S.Key ^ 0x2D6F));
							Cur.push_back(B.Vert(Rc + V3{ X, 0.0, 0.0 } + Rd * Rr, Rd, C));
						}
						if (I > 0)
							for (size_t J = 0; J < static_cast<size_t>(NA); ++J)
							{
								const double An = -0.12 * Pi + 1.24 * Pi * (static_cast<double>(J) + 0.5) / NA;
								const V3 Rd{ 0.0, std::cos(An), std::sin(An) };
								B.Tri(Prev[J], Cur[J], Cur[J + 1], Rd);
								B.Tri(Prev[J], Cur[J + 1], Prev[J + 1], Rd);
							}
						if (I == 0 || I == NXr)
						{
							const V3 Ax{ I == 0 ? -1.0 : 1.0, 0.0, 0.0 };
							const uint32_t Mc = B.Vert(Rc + V3{ X, 0.0, 0.0 } + Ax * 0.04, Ax, Scaled(P.Thatch, 0.8));
							for (size_t J = 0; J < static_cast<size_t>(NA); ++J) B.Tri(Mc, Cur[J], Cur[J + 1], Ax);
						}
						Prev = Cur;
					}
				}
				const int Pairs = static_cast<int>(2.0 * P.Xr / 0.75);
				for (int K = 0; K <= Pairs; ++K)
				{
					const double X = -P.Xr + 0.15 + (2.0 * P.Xr - 0.3) * K / Pairs;
					for (int Sd = -1; Sd <= 1; Sd += 2)
					{
						const V3 Foot = RoofAt(P, Sd, X, 0.55, 0.03), Top = V3{ X, P.Yc - Sd * 0.2, P.Ridge + 0.42 };
						B.Beam(Foot, Top, 0.045, 0.045, Rgb(96, 82, 64), Up, 0);
					}
				}
			}

			/** The brick chimney through the roof, course by course, corbelled at its top and sooted. */
			void Chimney(Pen& B, const Plan& P)
			{
				const double X = P.Chx, Y = P.Yc + 0.32, H = 0.27;
				const double Base = UnderAt(P, std::fabs(Y - P.Yc)) - 0.05, Top = P.Ridge + (P.bThatch ? 0.75 : 0.6);
				const double Ch = 0.077;
				const int N = static_cast<int>((Top - Base) / Ch);
				for (int K = 0; K < N; ++K)
				{
					const double Z0 = Base + K * Ch, Z1 = Z0 + Ch;
					const bool bCap = K >= N - 2;
					const double Hh = H + (bCap ? 0.035 : 0.0);
					Col C = Varied(P.Brick, 0.88 + 0.18 * Unit(Hash(P.S.Key ^ 0xB71C, static_cast<uint64_t>(K))), 0.03);
					C = Mixed(C, Rgb(40, 36, 32), 0.6 * Smooth(Top - 0.5, Top, Z1));
					const Col Mortar = Mixed(Rgb(168, 160, 146), Rgb(50, 46, 42), 0.6 * Smooth(Top - 0.5, Top, Z1));
					for (int F = 0; F < 4; ++F)
					{
						const double Th = 0.5 * Pi * F;
						const V3 Nn{ std::cos(Th), std::sin(Th), 0.0 }, Tt{ -std::sin(Th), std::cos(Th), 0.0 };
						const V3 Cn = V3{ X, Y, 0.0 } + Nn * Hh;
						B.Quad1(Cn + Tt * -Hh + V3{ 0, 0, Z0 + 0.011 }, Cn + Tt * Hh + V3{ 0, 0, Z0 + 0.011 }, Cn + Tt * Hh + V3{ 0, 0, Z1 }, Cn + Tt * -Hh + V3{ 0, 0, Z1 }, C, Nn);
						B.Quad1(Cn + Tt * -Hh + V3{ 0, 0, Z0 } - Nn * 0.006, Cn + Tt * Hh + V3{ 0, 0, Z0 } - Nn * 0.006, Cn + Tt * Hh + V3{ 0, 0, Z0 + 0.011 } - Nn * 0.006, Cn + Tt * -Hh + V3{ 0, 0, Z0 + 0.011 } - Nn * 0.006, Mortar, Nn);
					}
				}
				const double Zt = Base + N * Ch;
				B.Quad1(V3{ X - H - 0.035, Y - H - 0.035, Zt }, V3{ X + H + 0.035, Y - H - 0.035, Zt }, V3{ X + H + 0.035, Y + H + 0.035, Zt }, V3{ X - H - 0.035, Y + H + 0.035, Zt }, Rgb(46, 42, 38), Up);
				B.Quad1(V3{ X - 0.12, Y - 0.12, Zt + 0.002 }, V3{ X + 0.12, Y - 0.12, Zt + 0.002 }, V3{ X + 0.12, Y + 0.12, Zt + 0.002 }, V3{ X - 0.12, Y + 0.12, Zt + 0.002 }, Rgb(14, 12, 10), Up);
			}

			/** Along the back wall: firewood split and stacked, its ends out, under two planks; a bench by the door; a barrel
			 * at the porch's corner. */
			void Yard(Pen& B, const Plan& P)
			{
				const Wall W = WallOf(P, 2);
				Rng Rw(P.S.Key ^ 0x60D);
				const double U0 = -0.8 * P.A, U1 = 0.1 * P.A, Hp = 1.35, D0 = P.R + 0.1, D1 = D0 + 0.38;
				WBox(B, W, 0.5 * (U0 + U1), 0.5 * Hp, 0.5 * (D0 + D1), 0.5 * (U1 - U0), 0.5 * (D1 - D0) - 0.01, 0.5 * Hp, Rgb(48, 40, 32), 0);
				for (double Z = 0.07; Z < Hp - 0.05; Z += 0.125)
				{
					double U = U0 + Rw.Range(0.0, 0.05);
					while (U < U1 - 0.06)
					{
						const double Rr = Rw.Range(0.05, 0.075), Uc = U + Rr;
						const Col C = Mixed(Rgb(156, 128, 92), Rgb(118, 110, 98), Rw.Range(0.1, 0.9));
						const double Dz = Rw.Range(-0.01, 0.01), Dd = Rw.Range(-0.03, 0.02);
						const V3 Cn = On(W, Uc, Z + Dz, D1 + Dd);
						const int Sides = 5;
						const double Rot = Rw.Range(0.0, 2.0 * Pi);
						const uint32_t Mid = B.Vert(Cn, W.N, Scaled(C, 0.86));
						std::vector<uint32_t> Rim;
						for (int J = 0; J <= Sides; ++J)
						{
							const double Th = Rot + 2.0 * Pi * J / Sides;
							Rim.push_back(B.Vert(Cn + W.T * (Rr * std::cos(Th)) + V3{ 0.0, 0.0, 0.85 * Rr * std::sin(Th) }, W.N, C));
						}
						for (int J = 0; J < Sides; ++J) B.Tri(Mid, Rim[static_cast<size_t>(J)], Rim[static_cast<size_t>(J) + 1], W.N);
						U += 2.0 * Rr + 0.006;
					}
				}
				for (int K = 0; K < 2; ++K) WBox(B, W, 0.5 * (U0 + U1), Hp + 0.02 + 0.02 * K, D0 + 0.1 + 0.2 * K, 0.5 * (U1 - U0) + 0.12, 0.11, 0.012, Varied(P.Plank, 0.9 + 0.06 * K, 0.0), 0);
				// (the bench by the door)
				const Wall WF = WallOf(P, 0);
				for (const Gap& G : P.Gaps)
					if (G.What == 1)
					{
						const double Ub = G.U + 0.82 + 0.26 * PorchSteps(G.Z0 - 0.02) + 0.75;
						WBox(B, WF, Ub, 0.45, P.R + 0.25, 0.6, 0.15, 0.025, Rgb(116, 100, 82), 0);
						for (int Side = -1; Side <= 1; Side += 2) WBox(B, WF, Ub + Side * 0.48, 0.21, P.R + 0.25, 0.03, 0.12, 0.21, Rgb(96, 84, 70), 16);
						// (and the barrel beyond the bench)
						const V3 Bc = On(WF, Ub + 1.0, 0.0, P.R + 0.4);
						B.Tube(Bc, Bc + V3{ 0.0, 0.0, 0.82 }, 0.3, 0.3, 10, Rgb(98, 80, 62), true);
						for (double Zh : { 0.16, 0.66 }) B.Tube(Bc + V3{ 0.0, 0.0, Zh }, Bc + V3{ 0.0, 0.0, Zh + 0.04 }, 0.305, 0.305, 10, Rgb(50, 46, 42), false);
						B.Quad1(Bc + V3{ -0.26, -0.26, 0.76 }, Bc + V3{ 0.26, -0.26, 0.76 }, Bc + V3{ 0.26, 0.26, 0.76 }, Bc + V3{ -0.26, 0.26, 0.76 }, Rgb(24, 28, 30), Up);
					}
			}

			/** The eaves' frieze along the long walls under the roof: a board, its foot cut in teeth. */
			void Frieze(Pen& B, const Plan& P)
			{
				for (int F : { 0, 2 })
				{
					const Wall W = WallOf(P, F);
					const double Zb = P.Zt - 0.3, Half = W.Half + P.R;
					WBox(B, W, 0.0, Zb + 0.12, P.R + 0.012, Half, 0.012, 0.12, P.Paint, 4);
					const int NT = static_cast<int>(2.0 * Half / 0.14);
					for (int K = 0; K < NT; ++K)
					{
						const double Ua = -Half + 2.0 * Half * K / NT, Ub = -Half + 2.0 * Half * (K + 1) / NT;
						B.Tri1(On(W, Ua, Zb, P.R + 0.025), On(W, Ub, Zb, P.R + 0.025), On(W, 0.5 * (Ua + Ub), Zb - 0.07, P.R + 0.025), P.Paint, W.N);
					}
				}
			}

			/** The snow on the roof (Spec's Snow): a blanket over each slope, a little uneven, thinner toward the ridge where the
			 * wind takes it and by the chimney's warmth; at the eaves it runs on a hand past them and curls over, rounded, at
			 * the verges it rounds down onto the roof; over the ridge log or the straw roll a cap (the horse's head and the
			 * poles stand out of it). Cell its grid's size: the near level's 0.25 m, the middle's a metre. */
			void SnowCap(Pen& B, const Plan& P, double Cell)
			{
				if (P.Snow <= 0.0) return;
				const double D0 = 0.06 + 0.12 * P.Snow, Base = P.bThatch ? 0.025 : 0.05, Over = 0.1, Xs = P.Xr - 0.02;
				const V3 Ch{ P.Chx, P.Yc + 0.32, 0.0 };
				const Col White = Rgb(236, 238, 242), Shade = Rgb(204, 210, 220);
				auto Depth = [&](int Sd, double X, double Q) {
					double D = D0 * (0.8 + 0.35 * Noise01(X / 1.4, Q / 1.1, P.S.Key ^ 0x5A0F));
					D *= 0.55 + 0.45 * Smooth(0.0, 0.9, Q);
					const V3 At = RoofAt(P, Sd, X, Q, 0.0);
					return D * (0.2 + 0.8 * Smooth(0.3, 1.2, std::hypot(At.X - Ch.X, At.Y - Ch.Y)));
				};
				auto Tint = [&](double X, double Q) { return Mixed(White, Shade, 0.4 * Noise01(X / 0.6, Q / 0.5, P.S.Key ^ 0x5A10)); };
				const int NX = std::max(2, static_cast<int>(std::ceil(2.0 * Xs / Cell))), NQ = std::max(2, static_cast<int>(std::ceil(P.Qe / Cell)));
				const int NC = Cell < 0.5 ? 4 : 2; // (the eaves' curl, in steps)
				const double Top = P.bThatch ? 0.27 : 0.23; // (over the straw roll, or the ridge log)
				std::vector<V3> Ridge[2];
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const V3 Nr = RoofN(P, Sd), Dn{ 0.0, Sd * P.CosP, -P.SinP };
					// (its top: the slope's grid, and a row past the eaves)
					std::vector<std::vector<V3>> G(static_cast<size_t>(NQ) + 2);
					std::vector<double> Qs(static_cast<size_t>(NQ) + 2);
					for (int I = 0; I <= NQ + 1; ++I)
					{
						const double Q = I <= NQ ? P.Qe * I / NQ : P.Qe + Over;
						Qs[static_cast<size_t>(I)] = Q;
						for (int J = 0; J <= NX; ++J)
						{
							const double X = -Xs + 2.0 * Xs * J / NX;
							const double D = I <= NQ ? Depth(Sd, X, Q) : 0.85 * Depth(Sd, X, P.Qe);
							G[static_cast<size_t>(I)].push_back(RoofAt(P, Sd, X, Q, Base + D));
						}
					}
					for (int I = 0; I <= NQ; ++I)
						for (int J = 0; J < NX; ++J)
						{
							const size_t Ii = static_cast<size_t>(I), Jj = static_cast<size_t>(J);
							const double Xa = -Xs + 2.0 * Xs * J / NX, Xb = -Xs + 2.0 * Xs * (J + 1) / NX;
							B.Quad(G[Ii][Jj], G[Ii][Jj + 1], G[Ii + 1][Jj + 1], G[Ii + 1][Jj], Tint(Xa, Qs[Ii]), Tint(Xb, Qs[Ii]), Tint(Xb, Qs[Ii + 1]), Tint(Xa, Qs[Ii + 1]), Nr);
						}
					Ridge[Sd > 0 ? 1 : 0] = G[0];
					// (the eaves: the lip curled over and back under to the roof's edge)
					const std::vector<V3>& E = G[static_cast<size_t>(NQ) + 1];
					for (int J = 0; J < NX; ++J)
					{
						const size_t Jj = static_cast<size_t>(J);
						const double Xa = -Xs + 2.0 * Xs * J / NX, Xb = -Xs + 2.0 * Xs * (J + 1) / NX;
						const double Ra = 0.5 * (Dot(E[Jj] - RoofAt(P, Sd, Xa, P.Qe + Over, 0.0), Nr) - Base + 0.04), Rb = 0.5 * (Dot(E[Jj + 1] - RoofAt(P, Sd, Xb, P.Qe + Over, 0.0), Nr) - Base + 0.04);
						V3 Pa = E[Jj], Pb = E[Jj + 1];
						for (int K = 1; K <= NC; ++K)
						{
							const double An = Pi * K / NC;
							const V3 Da = (Nr * std::cos(An) + Dn * std::sin(An)) * Ra + E[Jj] - Nr * Ra, Db = (Nr * std::cos(An) + Dn * std::sin(An)) * Rb + E[Jj + 1] - Nr * Rb;
							const double Am = Pi * (K - 0.5) / NC;
							const Col C = Mixed(White, Shade, 0.3 + 0.5 * static_cast<double>(K) / NC);
							B.Quad1(Pa, Pb, Db, Da, C, Nr * std::cos(Am) + Dn * std::sin(Am));
							Pa = Da;
							Pb = Db;
						}
						B.Quad1(Pa, Pb, RoofAt(P, Sd, Xb, P.Qe - 0.02, Base - 0.01), RoofAt(P, Sd, Xa, P.Qe - 0.02, Base - 0.01), Shade, Nr * -1.0);
					}
					// (the verges: rounded down onto the roof's edge)
					for (int End = -1; End <= 1; End += 2)
					{
						const size_t Jj = End < 0 ? 0 : static_cast<size_t>(NX);
						const V3 Xh{ static_cast<double>(End), 0.0, 0.0 };
						for (int I = 0; I <= NQ; ++I)
						{
							const size_t Ii = static_cast<size_t>(I);
							auto Mid = [&](size_t K) { return G[K][Jj] + Xh * 0.03 - Nr * (0.45 * Dot(G[K][Jj] - RoofAt(P, Sd, G[K][Jj].X, Qs[K], 0.0), Nr) - 0.45 * Base); };
							auto Foot = [&](size_t K) { return RoofAt(P, Sd, End * (Xs + 0.045), Qs[K], Base - 0.015); };
							B.Quad1(G[Ii][Jj], G[Ii + 1][Jj], Mid(Ii + 1), Mid(Ii), White, Nr + Xh);
							B.Quad1(Mid(Ii), Mid(Ii + 1), Foot(Ii + 1), Foot(Ii), Shade, Xh);
						}
					}
				}
				// (the ridge: a cap over the log or the roll, from one slope's top row to the other's)
				for (size_t J = 0; J + 1 < Ridge[0].size(); ++J)
				{
					const double Xa = Ridge[0][J].X, Xb = Ridge[0][J + 1].X;
					const V3 Ta{ Xa, P.Yc, std::max(P.Ridge + Top, 0.5 * (Ridge[0][J].Z + Ridge[1][J].Z) + 0.04) }, Tb{ Xb, P.Yc, std::max(P.Ridge + Top, 0.5 * (Ridge[0][J + 1].Z + Ridge[1][J + 1].Z) + 0.04) };
					B.Quad1(Ridge[0][J], Ridge[0][J + 1], Tb, Ta, White, V3{ 0.0, -0.4, 1.0 });
					B.Quad1(Ridge[1][J], Ridge[1][J + 1], Tb, Ta, White, V3{ 0.0, 0.4, 1.0 });
				}
			}

			// ---------------------------------------------------------------- the middle and far levels

			/** The middle level (from 90 m): each log a bent strip of two facets, its ends at the corners as stubs, the
			 * openings their paint and glass, the gables' boards in strips, the roof in bands, porch, chimney and woodpile as
			 * boxes. */
			void Mid(Pen& B, const Plan& P)
			{
				for (int F = 0; F < 4; ++F)
				{
					const Wall W = WallOf(P, F);
					for (int K = 0; K < CoursesOf(P, F); ++K)
					{
						const uint64_t Seed = LogSeed(P, F, K);
						const double Z = CourseZ(P, W, K);
						const double Ex = EndReach(P, F, Z, P.R + P.Ext * 0.9);
						std::vector<std::pair<double, double>> Cut;
						for (const Gap& G : P.Gaps)
							if (G.Face == F && G.What != 2 && Z > G.Z0 && Z < G.Z1) Cut.push_back({ G.U - 0.5 * G.W, G.U + 0.5 * G.W });
						std::sort(Cut.begin(), Cut.end());
						double From = -W.Half - Ex;
						for (size_t I = 0; I <= Cut.size(); ++I)
						{
							const double To = I < Cut.size() ? Cut[I].first : W.Half + Ex;
							const double Ang[3] = { -0.45 * Pi, 0.0, 0.45 * Pi };
							std::vector<uint32_t> Row[3];
							const int NT = std::max(1, static_cast<int>((To - From) / 2.0));
							for (int A = 0; A < 3; ++A)
								for (int J = 0; J <= NT; ++J)
								{
									const double T = From + (To - From) * J / NT;
									const V3 Rd = W.N * std::cos(Ang[A]) + Up * std::sin(Ang[A]);
									Row[A].push_back(B.Vert(On(W, T, Z, 0.0) + Rd * P.R, Rd, LogCol(P, Seed, T, Ang[A], Z)));
								}
							for (int A = 0; A < 2; ++A)
								for (int J = 0; J < NT; ++J)
								{
									const V3 Rd = W.N * std::cos(0.5 * (Ang[A] + Ang[A + 1])) + Up * std::sin(0.5 * (Ang[A] + Ang[A + 1]));
									B.Tri(Row[A][static_cast<size_t>(J)], Row[A][static_cast<size_t>(J) + 1], Row[A + 1][static_cast<size_t>(J) + 1], Rd);
									B.Tri(Row[A][static_cast<size_t>(J)], Row[A + 1][static_cast<size_t>(J) + 1], Row[A + 1][static_cast<size_t>(J)], Rd);
								}
							if (I < Cut.size()) From = Cut[I].second;
						}
						// (its ends' grain at the corners)
						for (int E = -1; E <= 1; E += 2)
						{
							const double T = E * (W.Half + Ex);
							const V3 C0 = On(W, T, Z, 0.0);
							const Col Eg = Varied(P.LogEnd, 0.85, 0.0);
							const V3 Tn = W.T * static_cast<double>(E);
							B.Quad1(C0 + W.N * P.R + V3{ 0, 0, -0.75 * P.R }, C0 + W.N * P.R + V3{ 0, 0, 0.75 * P.R }, C0 - W.N * P.R + V3{ 0, 0, 0.75 * P.R }, C0 - W.N * P.R + V3{ 0, 0, -0.75 * P.R }, Eg, Tn);
						}
					}
				}
				// (the openings: the door in its frame; each window's carved frame and pediment, its curtain under the sky in its
				// glass, its sash's T, its shutters open beside it or shut over it)
				for (const Gap& G : P.Gaps)
				{
					const Wall W = WallOf(P, G.Face);
					const double F = P.R + 0.01, Sw = G.What == 2 ? 0.09 : 0.12;
					auto Flat = [&](double U0, double U1, double Za, double Zb, double D, const Col& C) {
						B.Quad1(On(W, U0, Za, D), On(W, U1, Za, D), On(W, U1, Zb, D), On(W, U0, Zb, D), C, W.N);
					};
					if (G.What == 1)
					{
						Flat(G.U - 0.5 * G.W - 0.12, G.U + 0.5 * G.W + 0.12, G.Z0, G.Z1 + 0.16, F, P.Paint);
						Flat(G.U - 0.5 * G.W, G.U + 0.5 * G.W, G.Z0, G.Z1, F + 0.005, Scaled(P.Door, 0.8));
						continue;
					}
					Flat(G.U - 0.5 * G.W - Sw, G.U + 0.5 * G.W + Sw, G.Z0 - 0.15, G.Z1 + 0.22, F, P.Paint);
					B.Tri1(On(W, G.U - 0.5 * G.W - Sw - 0.04, G.Z1 + 0.22, F), On(W, G.U + 0.5 * G.W + Sw + 0.04, G.Z1 + 0.22, F), On(W, G.U, G.Z1 + 0.22 + (G.What == 2 ? 0.14 : 0.22), F), P.Paint, W.N);
					const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Lw = 0.5 * G.W + 0.01;
					if (G.bShut)
					{
						Flat(U0, U1, G.Z0 - 0.02, G.Z1 + 0.02, F + 0.03, P.Shutter);
						continue;
					}
					const Col Gl = Mixed(P.Glass, P.Sky, 0.36), Cu = Mixed(Rgb(176, 172, 162), P.Sky, 0.15);
					const double ZT = G.Z0 + 0.64 * (G.Z1 - G.Z0);
					if (G.What == 2) Flat(U0, U1, G.Z0, G.Z1, F + 0.005, Gl);
					else
					{
						const double Zc = G.Z0 + 0.62 * (ZT - G.Z0);
						Flat(U0, U1, G.Z0, Zc, F + 0.005, Scaled(Cu, 0.88));
						Flat(U0, U1, Zc, G.Z1, F + 0.005, Gl);
						const Col Sb = Scaled(P.Paint, 0.92);
						Flat(G.U - 0.016, G.U + 0.016, G.Z0, G.Z1, F + 0.008, Sb);
						Flat(U0, U1, ZT - 0.016, ZT + 0.016, F + 0.008, Sb);
					}
					if (G.What == 2) continue;
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double Ua = Side < 0 ? U0 - Sw - Lw : U1 + Sw, Ub = Ua + Lw;
						Flat(Ua, Ub, G.Z0 - 0.02, G.Z1 + 0.02, F + 0.03, P.Shutter);
					}
				}
				// (the eaves' frieze along the long walls, its teeth half its colour)
				for (int F : { 0, 2 })
				{
					const Wall W = WallOf(P, F);
					const double Zb = P.Zt - 0.3, Half = W.Half + P.R;
					B.Quad1(On(W, -Half, Zb, P.R + 0.02), On(W, Half, Zb, P.R + 0.02), On(W, Half, Zb + 0.24, P.R + 0.02), On(W, -Half, Zb + 0.24, P.R + 0.02), P.Paint, W.N);
					B.Quad(On(W, -Half, Zb - 0.07, P.R + 0.025), On(W, Half, Zb - 0.07, P.R + 0.025), On(W, Half, Zb, P.R + 0.025), On(W, -Half, Zb, P.R + 0.025), Mixed(P.Paint, P.Log, 0.55), Mixed(P.Paint, P.Log, 0.55), P.Paint, P.Paint, W.N);
				}
				// (the gables' boards above the last log, in strips, a batten's shadow down each joint, the cornice at their foot)
				for (int F : { 1, 3 })
				{
					const Wall W = WallOf(P, F);
					const double Reach = (P.Ridge - P.Thick / P.CosP - P.Zg) / P.Tan;
					const int N = 8;
					for (int K = 0; K < N; ++K)
					{
						const double U0 = -Reach + 2.0 * Reach * K / N, U1 = -Reach + 2.0 * Reach * (K + 1) / N;
						const Col C = Scaled(Varied(Mixed(P.Plank, Rgb(82, 76, 68), 0.2), K % 2 ? 0.92 : 1.0, 0.0), 0.93);
						B.Quad(On(W, U0, P.Zg, P.R + 0.01), On(W, U1, P.Zg, P.R + 0.01), On(W, U1, std::max(P.Zg, UnderAt(P, std::fabs(U1))), P.R + 0.01), On(W, U0, std::max(P.Zg, UnderAt(P, std::fabs(U0))), P.R + 0.01), Scaled(C, 0.86), Scaled(C, 0.86), C, C, W.N);
					}
					const double Cu = W.Half - P.R - 0.01;
					B.Quad1(On(W, -Cu, P.Zg - 0.03, P.R + 0.1), On(W, Cu, P.Zg - 0.03, P.R + 0.1), On(W, Cu, P.Zg + 0.09, P.R + 0.1), On(W, -Cu, P.Zg + 0.09, P.R + 0.1), Scaled(P.Plank, 0.95), W.N);
				}
				// (the roof: each plank as the near lays it, in two lengths down the slope, the under ones in the upper ones'
				// shadow; or the thatch in its courses, its straws' paleness in its colour)
				const double Qw = (P.Bh + P.R) / P.CosP;
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const V3 Nr = RoofN(P, Sd);
					if (!P.bThatch)
						for (const RoofPlank& Pk : PlanksOf(P, Sd))
						{
							const Col C = Pk.bUpper ? Pk.C : Scaled(Pk.C, 0.8);
							const double Qm = 0.5 * Pk.Foot, Lift = Pk.Lift + 0.012;
							for (int I = 0; I < 2; ++I)
							{
								const double Q0 = I == 0 ? 0.05 : Qm, Q1 = I == 0 ? Qm : Pk.Foot;
								B.Quad(RoofAt(P, Sd, Pk.Xa, Q0, Lift), RoofAt(P, Sd, Pk.Xb, Q0, Lift), RoofAt(P, Sd, Pk.Xb, Q1, Lift), RoofAt(P, Sd, Pk.Xa, Q1, Lift), PlankCol(P, Sd, C, Pk.Seed, Pk.Xa, Q0), PlankCol(P, Sd, C, Pk.Seed, Pk.Xb, Q0), PlankCol(P, Sd, C, Pk.Seed, Pk.Xb, Q1), PlankCol(P, Sd, C, Pk.Seed, Pk.Xa, Q1), Nr);
							}
						}
					else
					{
						const int NX = 14, NQ = static_cast<int>(std::ceil(P.Qe / 0.34));
						auto Sc = [&](double X, double Q) { return Mixed(StrawCol(P, Sd, X, Q), Rgb(170, 150, 110), 0.1); };
						for (int J = 0; J < NX; ++J)
							for (int I = 0; I < NQ; ++I)
							{
								const double X0 = -P.Xr + 2.0 * P.Xr * J / NX, X1 = -P.Xr + 2.0 * P.Xr * (J + 1) / NX, Q0 = P.Qe * I / NQ, Q1 = P.Qe * (I + 1) / NQ;
								B.Quad(RoofAt(P, Sd, X0, Q0, 0.03), RoofAt(P, Sd, X1, Q0, 0.03), RoofAt(P, Sd, X1, Q1, 0.06), RoofAt(P, Sd, X0, Q1, 0.06), Sc(X0, Q0), Sc(X1, Q0), Scaled(Sc(X1, Q1), 0.9), Scaled(Sc(X0, Q1), 0.9), Nr);
							}
					}
					const double Ed = P.bThatch ? 0.3 : 0.12;
					const V3 E0 = RoofAt(P, Sd, -P.Xr, P.Qe, 0.02), E1 = RoofAt(P, Sd, P.Xr, P.Qe, 0.02);
					B.Quad1(E0, E1, E1 + V3{ 0, 0, -Ed }, E0 + V3{ 0, 0, -Ed }, Scaled(P.bThatch ? P.Thatch : P.Plank, 0.65), V3{ 0.0, static_cast<double>(Sd), 0.0 });
					B.Quad1(E0 + V3{ 0, 0, -Ed }, E1 + V3{ 0, 0, -Ed }, RoofAt(P, Sd, P.Xr, Qw, -P.Thick), RoofAt(P, Sd, -P.Xr, Qw, -P.Thick), Rgb(58, 52, 44), Nr * -1.0);
					for (int End = -1; End <= 1; End += 2)
					{
						const V3 V0 = RoofAt(P, Sd, End * P.Xr, 0.0, 0.02), V1 = RoofAt(P, Sd, End * P.Xr, P.Qe, 0.02);
						B.Quad1(V0, V1, V1 + V3{ 0, 0, -0.24 }, V0 + V3{ 0, 0, -0.24 }, P.bThatch ? Scaled(StrawCol(P, Sd, End * P.Xr, 0.5 * P.Qe), 0.85) : P.Paint, V3{ static_cast<double>(End), 0.0, 0.0 });
						B.Quad1(V0 + V3{ 0, 0, -0.24 }, V1 + V3{ 0, 0, -0.24 }, RoofAt(P, Sd, End * (P.A + P.R), P.Qe, -0.1), RoofAt(P, Sd, End * (P.A + P.R), 0.0, -0.1), Rgb(58, 52, 44), V3{ 0.0, 0.0, -1.0 });
					}
				}
				// (the ridge log, or the straw roll and its crossed poles)
				const double Rr = P.bThatch ? 0.2 : 0.13;
				for (int A = 0; A < 4; ++A)
				{
					const double An0 = Pi * A / 4.0, An1 = Pi * (A + 1) / 4.0;
					const V3 R0{ 0.0, std::cos(An0), std::sin(An0) }, R1{ 0.0, std::cos(An1), std::sin(An1) };
					const V3 C0{ -P.Xr - 0.05, P.Yc, P.Ridge + 0.06 }, C1{ P.Xr + 0.05, P.Yc, P.Ridge + 0.06 };
					const Col C = P.bThatch ? Mixed(P.Thatch, Rgb(112, 104, 90), 0.35) : P.Log;
					B.Quad1(C0 + R0 * Rr, C1 + R0 * Rr, C1 + R1 * Rr, C0 + R1 * Rr, Scaled(C, 0.9 + 0.1 * std::sin(0.5 * (An0 + An1))), (R0 + R1) * 0.5);
				}
				if (!P.bThatch)
				{
					// (the horse's head at the ridge log's +x end, its two flat sides; the towel board at each gable's apex)
					const Col Hc = Varied(P.Log, 1.05, 0.0);
					const V3 O{ P.Xr + 0.04, P.Yc, P.Ridge + 0.02 };
					auto Piece = [&](std::initializer_list<std::pair<double, double>> Poly) {
						std::vector<std::pair<double, double>> Pp(Poly);
						for (size_t I = 1; I + 1 < Pp.size(); ++I)
							for (int Sd = -1; Sd <= 1; Sd += 2)
							{
								auto At = [&](size_t J) { return O + V3{ Pp[J].first, Sd * 0.055, Pp[J].second }; };
								B.Tri1(At(0), At(I), At(I + 1), Sd > 0 ? Hc : Scaled(Hc, 0.9), V3{ 0.0, static_cast<double>(Sd), 0.0 });
							}
					};
					Piece({ { -0.05, -0.02 }, { 0.3, -0.02 }, { 0.5, 0.2 }, { 0.22, 0.32 }, { -0.05, 0.14 } });
					Piece({ { 0.42, 0.13 }, { 0.66, 0.2 }, { 0.73, 0.3 }, { 0.6, 0.39 }, { 0.43, 0.36 } });
					for (int End = -1; End <= 1; End += 2)
					{
						const double X = End * (P.Xr + 0.035);
						B.Quad1(V3{ X, P.Yc - 0.13, P.Ridge - 1.03 }, V3{ X, P.Yc + 0.13, P.Ridge - 1.03 }, V3{ X, P.Yc + 0.13, P.Ridge - 0.08 }, V3{ X, P.Yc - 0.13, P.Ridge - 0.08 }, P.Paint, V3{ static_cast<double>(End), 0.0, 0.0 });
					}
				}
				else
				{
					const int Pairs = static_cast<int>(2.0 * P.Xr / 0.75);
					for (int K = 0; K <= Pairs; ++K)
					{
						const double X = -P.Xr + 0.15 + (2.0 * P.Xr - 0.3) * K / Pairs;
						for (int Sd = -1; Sd <= 1; Sd += 2)
						{
							const V3 Foot = RoofAt(P, Sd, X, 0.55, 0.03), Top = V3{ X, P.Yc - Sd * 0.2, P.Ridge + 0.42 };
							B.Quad1(Foot + V3{ -0.025, 0, 0 }, Foot + V3{ 0.025, 0, 0 }, Top + V3{ 0.025, 0, 0 }, Top + V3{ -0.025, 0, 0 }, Rgb(96, 82, 64), V3{ 0.0, Sd * 0.6, 0.8 });
						}
					}
				}
				// (the porch: its floor and steps, its posts, its rails' balusters in their paint, its little roof and barge
				// board)
				for (const Gap& G : P.Gaps)
					if (G.What == 1)
					{
						const Wall W = WallOf(P, 0);
						const double F = P.R, Zf = G.Z0 - 0.02, Hw = 0.82, Out = PorchOut;
						const Col Wd = Rgb(118, 104, 86), Wd2 = Rgb(104, 92, 78);
						WBox(B, W, G.U, 0.5 * Zf, F + 0.5 * Out, Hw, 0.5 * Out, 0.5 * Zf, Scaled(Wd2, 0.7), 16);
						B.Quad1(On(W, G.U - Hw, Zf + 0.002, F), On(W, G.U + Hw, Zf + 0.002, F), On(W, G.U + Hw, Zf + 0.002, F + Out), On(W, G.U - Hw, Zf + 0.002, F + Out), Wd, Up);
						const double Sr = 0.26 * PorchSteps(Zf);
						B.Quad1(On(W, G.U + Hw, Zf, F + 0.05), On(W, G.U + Hw, Zf, F + Out), On(W, G.U + Hw + Sr, 0.0, F + Out), On(W, G.U + Hw + Sr, 0.0, F + 0.05), Scaled(Wd, 0.85), W.T + Up);
						B.Tri1(On(W, G.U + Hw, Zf, F + Out), On(W, G.U + Hw + Sr, 0.0, F + Out), On(W, G.U + Hw, 0.0, F + Out), Scaled(Wd2, 0.7), W.N);
						const double Zp = std::min(G.Z1 + 0.42, P.Zt - 0.15), Zrl = Zf + 0.85;
						const Col Rl = Mixed(P.Paint, Scaled(Wd2, 0.5), 0.5);
						for (int Side = -1; Side <= 1; Side += 2)
						{
							const double Up0 = G.U + Side * (Hw - 0.07);
							WBox(B, W, Up0, 0.5 * (Zf + Zp), F + Out - 0.07, 0.06, 0.06, 0.5 * (Zp - Zf), Wd, 16);
							const double Rw = Hw + 0.2, Rise = 0.32, Ro = Out + 0.1, Zb = Zp + 0.1;
							B.Quad1(On(W, G.U, Zb + Rise, F), On(W, G.U, Zb + Rise, F + Ro), On(W, G.U + Side * Rw, Zb, F + Ro), On(W, G.U + Side * Rw, Zb, F), P.Plank, Up + W.T * (Side * 0.4));
							B.Quad1(On(W, G.U, Zb + Rise, F), On(W, G.U, Zb + Rise, F + Ro), On(W, G.U + Side * Rw, Zb, F + Ro), On(W, G.U + Side * Rw, Zb, F), Scaled(Wd2, 0.6), V3{ 0.0, 0.0, -1.0 });
							B.Quad1(On(W, G.U, Zb + Rise - 0.02, F + Ro + 0.01), On(W, G.U + Side * Rw, Zb - 0.02, F + Ro + 0.01), On(W, G.U + Side * Rw, Zb - 0.21, F + Ro + 0.01), On(W, G.U, Zb + Rise - 0.21, F + Ro + 0.01), P.Paint, W.N);
						}
						const double Ur = G.U - (Hw - 0.07);
						B.Quad1(On(W, Ur, Zf, F + 0.05), On(W, Ur, Zf, F + Out - 0.1), On(W, Ur, Zrl, F + Out - 0.1), On(W, Ur, Zrl, F + 0.05), Rl, W.T * -1.0);
						B.Quad1(On(W, Ur, Zf, F + 0.05), On(W, Ur, Zf, F + Out - 0.1), On(W, Ur, Zrl, F + Out - 0.1), On(W, Ur, Zrl, F + 0.05), Scaled(Rl, 0.9), W.T);
						B.Quad1(On(W, G.U - Hw + 0.1, Zf, F + Out - 0.07), On(W, G.U + Hw - 0.1, Zf, F + Out - 0.07), On(W, G.U + Hw - 0.1, Zrl, F + Out - 0.07), On(W, G.U - Hw + 0.1, Zrl, F + Out - 0.07), Rl, W.N);
					}
				// (the chimney, its top sooted; the woodpile along the back under its planks)
				{
					const double X = P.Chx, Y = P.Yc + 0.32, H = 0.27;
					const double Base = UnderAt(P, std::fabs(Y - P.Yc)) - 0.05, Top = P.Ridge + (P.bThatch ? 0.75 : 0.6);
					const Col Cb = Mixed(P.Brick, Rgb(168, 160, 146), 0.12);
					B.Box(V3{ X, Y, 0.5 * (Base + Top - 0.4) }, V3{ H, 0, 0 }, V3{ 0, H, 0 }, V3{ 0, 0, 0.5 * (Top - 0.4 - Base) }, Cb, 16 | 32);
					B.Box(V3{ X, Y, Top - 0.2 }, V3{ H + 0.01, 0, 0 }, V3{ 0, H + 0.01, 0 }, V3{ 0, 0, 0.2 }, Mixed(Cb, Rgb(40, 36, 32), 0.4), 16);
				}
				{
					const Wall W = WallOf(P, 2);
					const double U0 = -0.8 * P.A, U1 = 0.1 * P.A;
					WBox(B, W, 0.5 * (U0 + U1), 0.675, P.R + 0.29, 0.5 * (U1 - U0), 0.19, 0.675, Rgb(132, 114, 90), 16);
					WBox(B, W, 0.5 * (U0 + U1), 1.39, P.R + 0.3, 0.5 * (U1 - U0) + 0.12, 0.21, 0.02, P.Plank, 16);
				}
				// (the footing: each boulder as the near lays it, its face, its crown and its ends in facets, the earth behind)
				for (int F = 0; F < 4; ++F)
				{
					const Wall W = WallOf(P, F);
					const double Top = FootTop(P, W), Run = FootRun(P, W);
					B.Quad1(On(W, -Run, -0.25, 0.05), On(W, Run, -0.25, 0.05), On(W, Run, Top - 0.02, 0.05), On(W, -Run, Top - 0.02, 0.05), Rgb(62, 56, 48), W.N);
					for (const Stone& St : StonesOf(P, F))
					{
						// (each its own shape, from its own seed: its face wider or narrower and set off its middle, its crown
						// leaning, one end lower; never a row of the same tooth)
						const double Sa = Unit(St.Seed ^ 0x1D1), Sb = Unit(St.Seed ^ 0x1D2), Sc = Unit(St.Seed ^ 0x1D3), Se = Unit(St.Seed ^ 0x1D4);
						const double Ua = St.Uc - 0.5 * St.L, Ub = St.Uc + 0.5 * St.L, Off = 0.12 * (Sa - 0.5) * St.L;
						const double Ia = St.Uc + Off - (0.22 + 0.14 * Sb) * St.L, Ib = St.Uc + Off + (0.22 + 0.14 * Sc) * St.L;
						const double Zm = St.Zt - St.Rv, Zs = St.Zt - (0.3 + 0.3 * Se) * St.Rv, D0 = 0.05, D1 = 0.05 + St.Dd;
						const double Lean = 0.06 * (Sb - Sc) * St.Rv;
						const V3 FA = On(W, Ia, -0.06, D1), FB = On(W, Ib, -0.06, D1), FC = On(W, Ib, Zs - Lean, D1), FD = On(W, Ia, Zs + Lean, D1);
						const V3 TA = On(W, St.Uc + Off - (0.15 + 0.15 * Sc) * St.L, St.Zt - std::max(0.0, Lean), D0 + 0.35 * St.Dd), TB = On(W, St.Uc + Off + (0.15 + 0.15 * Sb) * St.L, St.Zt + std::min(0.0, Lean), D0 + 0.35 * St.Dd);
						const V3 LA = On(W, Ua, Zm, D0), RA = On(W, Ub, Zm, D0);
						B.Quad1(FA, FB, FC, FD, Mixed(St.C, Rgb(70, 62, 50), 0.15), W.N);
						B.Quad1(FD, FC, TB, TA, Scaled(St.C, 1.04), W.N + Up);
						B.Quad1(LA, FA, FD, TA, Scaled(St.C, 0.9), W.N - W.T);
						B.Quad1(FB, RA, TB, FC, Scaled(St.C, 0.9), W.N + W.T);
					}
				}
			}

			/** A band of the roof's slope as the near level paints it, averaged: down from Q0 to Q1 on side Sd. */
			Col RoofTone(const Plan& P, int Sd, double Q0, double Q1)
			{
				double R = 0.0, G = 0.0, Bl = 0.0, Wt = 0.0;
				auto Add = [&](const Col& C, double W) {
					R += W * C.R;
					G += W * C.G;
					Bl += W * C.B;
					Wt += W;
				};
				if (!P.bThatch)
				{
					for (const RoofPlank& Pk : PlanksOf(P, Sd))
						for (int J = 0; J < 4; ++J)
						{
							const double Q = Q0 + (Q1 - Q0) * (J + 0.5) / 4.0;
							Add(PlankCol(P, Sd, Pk.bUpper ? Pk.C : Scaled(Pk.C, 0.8), Pk.Seed, 0.5 * (Pk.Xa + Pk.Xb), Q), Pk.Xb - Pk.Xa);
						}
				}
				else
					for (int I = 0; I < 16; ++I)
						for (int J = 0; J < 4; ++J)
							Add(Mixed(StrawCol(P, Sd, -P.Xr + 2.0 * P.Xr * (I + 0.5) / 16.0, Q0 + (Q1 - Q0) * (J + 0.5) / 4.0), Rgb(170, 150, 110), 0.1), 1.0);
				return Mixed(Rgb(R / Wt, G / Wt, Bl / Wt), Rgb(230, 234, 240), 0.85 * P.Snow);
			}

			/** The far level (from 300 m): the walls in bands of the logs' tone (their creases in it), the footing's stone, the
			 * windows in their frames, the eaves' frieze, the gables, the roof's two slopes in bands of its own tone, the barge
			 * boards, ridge, chimney and porch. */
			void Far(Pen& B, const Plan& P)
			{
				for (int F = 0; F < 4; ++F)
				{
					const Wall W = WallOf(P, F);
					const double Z0 = P.Z0 + W.ZOff - P.R, Z1 = F % 2 == 0 ? P.Zt : P.Zg;
					const int NB = 4;
					for (int K = 0; K < NB; ++K)
					{
						const double Za = Z0 + (Z1 - Z0) * K / NB, Zb = Z0 + (Z1 - Z0) * (K + 1) / NB;
						const Col C = Scaled(LogCol(P, LogSeed(P, F, K * 3), 0.0, 0.15, 0.5 * (Za + Zb)), 0.84);
						const double Ex = F % 2 == 0 ? P.R + P.Ext : EndReach(P, F, Zb - P.R, P.R + P.Ext);
						B.Quad1(On(W, -W.Half - Ex, Za, P.R), On(W, W.Half + Ex, Za, P.R), On(W, W.Half + Ex, Zb, P.R), On(W, -W.Half - Ex, Zb, P.R), C, W.N);
					}
					const double Run = FootRun(P, W);
					B.Quad1(On(W, -Run, -0.2, P.R + 0.08), On(W, Run, -0.2, P.R + 0.08), On(W, Run, Z0, P.R + 0.08), On(W, -Run, Z0, P.R + 0.08), Mixed(Scaled(P.Stone, 0.72), Rgb(62, 56, 48), 0.3), W.N);
				}
				for (const Gap& G : P.Gaps)
				{
					const Wall W = WallOf(P, G.Face);
					const double F = P.R + 0.015;
					auto Flat = [&](double U0, double U1, double Za, double Zb, double D, const Col& C) {
						B.Quad1(On(W, U0, Za, D), On(W, U1, Za, D), On(W, U1, Zb, D), On(W, U0, Zb, D), C, W.N);
					};
					if (G.What == 1)
					{
						Flat(G.U - 0.5 * G.W - 0.12, G.U + 0.5 * G.W + 0.12, G.Z0, G.Z1 + 0.16, F, P.Paint);
						Flat(G.U - 0.5 * G.W, G.U + 0.5 * G.W, G.Z0, G.Z1, F + 0.005, Scaled(P.Door, 0.8));
						continue;
					}
					const double Sw = 0.12;
					Flat(G.U - 0.5 * G.W - Sw, G.U + 0.5 * G.W + Sw, G.Z0 - 0.15, G.Z1 + 0.3, F, P.Paint);
					const Col Gl = G.What == 2 ? Mixed(P.Glass, P.Sky, 0.36) : Mixed(Mixed(P.Glass, P.Sky, 0.36), Rgb(176, 172, 162), 0.35);
					Flat(G.U - 0.5 * G.W, G.U + 0.5 * G.W, G.Z0, G.Z1, F + 0.005, G.bShut ? P.Shutter : Gl);
					if (G.What == 2 || G.bShut) continue;
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double Ua = Side < 0 ? G.U - 0.5 * G.W - Sw - 0.5 * G.W : G.U + 0.5 * G.W + Sw, Ub = Ua + 0.5 * G.W;
						Flat(Ua, Ub, G.Z0, G.Z1, F + 0.01, P.Shutter);
					}
				}
				for (int F : { 0, 2 })
				{
					const Wall W = WallOf(P, F);
					const double Zb = P.Zt - 0.33, Half = W.Half + P.R;
					B.Quad1(On(W, -Half, Zb, P.R + 0.02), On(W, Half, Zb, P.R + 0.02), On(W, Half, Zb + 0.27, P.R + 0.02), On(W, -Half, Zb + 0.27, P.R + 0.02), P.Paint, W.N);
				}
				for (int F : { 1, 3 })
				{
					const Wall W = WallOf(P, F);
					const double Reach = (P.Ridge - P.Thick / P.CosP - P.Zg) / P.Tan;
					B.Tri1(On(W, -Reach, P.Zg, P.R + 0.01), On(W, Reach, P.Zg, P.R + 0.01), On(W, 0.0, UnderAt(P, 0.0), P.R + 0.01), Scaled(Mixed(P.Plank, Rgb(82, 76, 68), 0.2), 0.87), W.N);
				}
				const double Qw = (P.Bh + P.R) / P.CosP;
				for (int Sd = -1; Sd <= 1; Sd += 2)
				{
					const double Qm = 0.5 * P.Qe;
					B.Quad1(RoofAt(P, Sd, -P.Xr, 0.0, 0.03), RoofAt(P, Sd, P.Xr, 0.0, 0.03), RoofAt(P, Sd, P.Xr, Qm, 0.03), RoofAt(P, Sd, -P.Xr, Qm, 0.03), RoofTone(P, Sd, 0.0, Qm), RoofN(P, Sd));
					B.Quad1(RoofAt(P, Sd, -P.Xr, Qm, 0.03), RoofAt(P, Sd, P.Xr, Qm, 0.03), RoofAt(P, Sd, P.Xr, P.Qe, 0.03), RoofAt(P, Sd, -P.Xr, P.Qe, 0.03), RoofTone(P, Sd, Qm, P.Qe), RoofN(P, Sd));
					const double Ed = P.bThatch ? 0.3 : 0.12;
					const V3 E0 = RoofAt(P, Sd, -P.Xr, P.Qe, 0.03), E1 = RoofAt(P, Sd, P.Xr, P.Qe, 0.03);
					B.Quad1(E0, E1, E1 + V3{ 0, 0, -Ed }, E0 + V3{ 0, 0, -Ed }, Scaled(P.bThatch ? P.Thatch : P.Plank, 0.65), V3{ 0.0, static_cast<double>(Sd), 0.0 });
					B.Quad1(E0 + V3{ 0, 0, -Ed }, E1 + V3{ 0, 0, -Ed }, RoofAt(P, Sd, P.Xr, Qw, -P.Thick), RoofAt(P, Sd, -P.Xr, Qw, -P.Thick), Rgb(58, 52, 44), RoofN(P, Sd) * -1.0);
					for (int End = -1; End <= 1; End += 2)
					{
						const V3 V0 = RoofAt(P, Sd, End * P.Xr, 0.0, 0.03), V1 = RoofAt(P, Sd, End * P.Xr, P.Qe, 0.03);
						B.Quad1(V0, V1, V1 + V3{ 0, 0, -0.24 }, V0 + V3{ 0, 0, -0.24 }, P.bThatch ? Scaled(P.Thatch, 0.8) : P.Paint, V3{ static_cast<double>(End), 0.0, 0.0 });
					}
					// (the ridge's roll or log, its two upper facets)
					const double Rr = P.bThatch ? 0.2 : 0.13;
					const V3 C0{ -P.Xr - 0.05, P.Yc, P.Ridge + 0.06 }, C1{ P.Xr + 0.05, P.Yc, P.Ridge + 0.06 };
					const V3 R0{ 0.0, Sd * 0.7, 0.7 }, R1{ 0.0, static_cast<double>(Sd), 0.0 };
					B.Quad1(C0 + V3{ 0, 0, Rr }, C1 + V3{ 0, 0, Rr }, C1 + R1 * Rr, C0 + R1 * Rr, P.bThatch ? Mixed(P.Thatch, Rgb(112, 104, 90), 0.35) : P.Log, R0);
				}
				{
					const double X = P.Chx, Y = P.Yc + 0.32, H = 0.27;
					const double Base = UnderAt(P, std::fabs(Y - P.Yc)) - 0.05, Top = P.Ridge + (P.bThatch ? 0.75 : 0.6);
					B.Box(V3{ X, Y, 0.5 * (Base + Top) }, V3{ H, 0, 0 }, V3{ 0, H, 0 }, V3{ 0, 0, 0.5 * (Top - Base) }, Mixed(Mixed(P.Brick, Rgb(168, 160, 146), 0.12), Rgb(40, 36, 32), 0.12), 16);
				}
				for (const Gap& G : P.Gaps)
					if (G.What == 1)
					{
						const Wall W = WallOf(P, 0);
						const double F = P.R, Zf = G.Z0 - 0.02, Hw = 0.82, Out = PorchOut;
						WBox(B, W, G.U, 0.5 * Zf, F + 0.5 * Out, Hw, 0.5 * Out, 0.5 * Zf, Rgb(88, 78, 66), 16);
						const double Sr = 0.26 * PorchSteps(Zf);
						B.Quad1(On(W, G.U + Hw, Zf, F + 0.05), On(W, G.U + Hw, Zf, F + Out), On(W, G.U + Hw + Sr, 0.0, F + Out), On(W, G.U + Hw + Sr, 0.0, F + 0.05), Rgb(100, 88, 74), W.T + Up);
						const double Zp = std::min(G.Z1 + 0.42, P.Zt - 0.15), Zb = Zp + 0.1;
						const Col Rl = Mixed(P.Paint, Rgb(52, 46, 39), 0.5);
						B.Quad1(On(W, G.U - Hw + 0.07, Zf, F + Out - 0.05), On(W, G.U - Hw + 0.07, Zf, F + 0.05), On(W, G.U - Hw + 0.07, Zf + 0.85, F + 0.05), On(W, G.U - Hw + 0.07, Zf + 0.85, F + Out - 0.05), Rl, W.T * -1.0);
						B.Quad1(On(W, G.U - Hw + 0.1, Zf, F + Out - 0.07), On(W, G.U + Hw - 0.1, Zf, F + Out - 0.07), On(W, G.U + Hw - 0.1, Zf + 0.85, F + Out - 0.07), On(W, G.U - Hw + 0.1, Zf + 0.85, F + Out - 0.07), Rl, W.N);
						for (int Side = -1; Side <= 1; Side += 2)
							B.Quad1(On(W, G.U, Zb + 0.32, F), On(W, G.U, Zb + 0.32, F + Out + 0.1), On(W, G.U + Side * 1.02, Zb, F + Out + 0.1), On(W, G.U + Side * 1.02, Zb, F), P.Plank, Up + W.T * (Side * 0.4));
						B.Quad1(On(W, G.U - 1.02, Zb - 0.2, F + Out + 0.11), On(W, G.U + 1.02, Zb - 0.2, F + Out + 0.11), On(W, G.U + 1.02, Zb, F + Out + 0.11), On(W, G.U - 1.02, Zb, F + Out + 0.11), Mixed(P.Paint, Rgb(70, 62, 52), 0.4), W.N);
					}
			}

			void Build(const Spec& S, Levels& Out)
			{
				Out = Levels();
				const Plan P = Make(S);
				{
					Pen B(Out.Near);
					Footing(B, P);
					Logs(B, P);
					for (const Gap& G : P.Gaps)
					{
						if (G.What == 0) Window(B, P, G, Hash(P.S.Key ^ 0x3171, static_cast<uint64_t>(G.Face * 16) + static_cast<uint64_t>(std::llround(G.U * 10.0 + 100.0))));
						if (G.What == 1)
						{
							Door(B, P, G);
							Porch(B, P, G);
						}
					}
					Gable(B, P, 1, P.S.Key ^ 0x6AB1);
					Gable(B, P, 3, P.S.Key ^ 0x6AB3);
					Frieze(B, P);
					if (P.bThatch) Thatch(B, P);
					else
					{
						Planks(B, P);
						Barge(B, P, -1);
						Barge(B, P, 1);
					}
					Chimney(B, P);
					Yard(B, P);
					SnowCap(B, P, 0.25);
				}
				{
					Pen B(Out.Mid);
					Mid(B, P);
					SnowCap(B, P, 1.0);
				}
				{
					Pen B(Out.Far);
					Far(B, P);
				}
			}
		}

		/*
		 * The east's khata (Kind 8; release 2, the trial "houses"): the cottage of the steppe's villages, where the kit's
		 * Khata stands (its footprint, 9.18 by 6.46 m, the eaves' reach). Walls of clay daubed over wattle, soft at their
		 * corners and a little wavy, limewashed white (a blue or a cream wash on some), the wash greyed in long patches, gone
		 * dirty low down and under the sills, flaked to the yellow clay here and there, cracked from the windows' corners;
		 * the prizba, the clay bench round its foot, painted (red-brown, brown, ochre or blue-grey); small windows deep in
		 * the walls, their casements and the band round them painted one colour, plank shutters on some; a plank door with
		 * its step; a hipped roof of rye thatch a third of a metre thick, its hips rounded, its courses combed, its eaves cut
		 * clean low over the walls on the rafters' ends; a whitewashed chimney through it by the ridge.
		 */
		namespace Kht
		{
			/** An opening: its face (0 front, +y; 1 the -x end; 2 back; 3 the +x end), its middle along the face's way (FaceOf's
			 * T), width, sill and head; what it is (0 a window, 1 the door); its shutters shut. */
			struct Gap
			{
				int Face = 0;
				double U = 0.0, W = 0.7, Z0 = 1.0, Z1 = 1.8;
				int What = 0;
				bool bShut = false;
			};

			struct Plan
			{
				Spec S;
				double Ex = 4.55, Ey = 3.19, Ov = 0.65, Wx = 3.9, Wy = 2.54, Wr = 0.16; // the eaves' half sizes; the overhang; the walls' (their outer face); their corners' radius
				double Ph = 0.45, Pb = 0.3, Df = 0.14;                                   // the prizba's height and reach; how deep the windows sit
				double Zw = 2.45, Ze = 1.9;                                              // the walls' top; the eaves' foot
				double Tan = 0.9, CosP = 0.74, SinP = 0.67, Pitch = 0.73, Thick = 0.32, Tb = 0.43; // the pitch; the thatch's thickness, and straight up
				double Rc = 0.5, Top = 2.99, Rho = 0.3;                                  // the hips' rounding; the inset where the slopes give way to the ridge's roll, its radius
				double Chx = 1.3, Chy = 0.35;
				double Snow = 0.0;
				bool bShutters = false, bBand = false, bBareDoor = false;
				Col Wash, Clay, Plinth, Frame, Door, Thatch, Glass, Sky, Bare, Brick;
				std::vector<Gap> Gaps;
			};

			/** A face of the walls: its middle on the outer face at the ground, its way along (round the house
			 * counter-clockwise seen from above), out from it, and the half length of its straight part. */
			struct Face
			{
				V3 C, T, N;
				double Half = 1.0;
			};
			Face FaceOf(const Plan& P, int F)
			{
				switch (F)
				{
				case 3: return { V3{ P.Wx, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, V3{ 1.0, 0.0, 0.0 }, P.Wy - P.Wr };
				case 0: return { V3{ 0.0, P.Wy, 0.0 }, V3{ -1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, P.Wx - P.Wr };
				case 1: return { V3{ -P.Wx, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, V3{ -1.0, 0.0, 0.0 }, P.Wy - P.Wr };
				default: return { V3{ 0.0, -P.Wy, 0.0 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, P.Wx - P.Wr };
				}
			}
			/** A face's way along for a place on it given in the house's own x (the front and back) or y (the ends). */
			double UOf(const Plan& P, int F, double Along) { const Face Fc = FaceOf(P, F); return F % 2 == 0 ? Along * Fc.T.X : Along * Fc.T.Y; }
			/** A paint gone toward the bare wood under it, by Low (0 none, 1 much). */
			Col Worn(const Col& C, const Col& Bare, double Low) { return Mixed(C, Bare, 0.55 * Clamp01(Low)); }

			Plan Make(const Spec& S)
			{
				Plan P;
				P.S = S;
				Rng Rg(S.Key ^ 0x6A7A);
				P.Ex = 0.5 * S.Long - 0.04;
				P.Ey = 0.5 * S.Deep - 0.04;
				P.Ov = std::clamp(0.62 + 0.08 * Rg.Next(), 0.4, 0.3 * P.Ey);
				P.Wx = P.Ex - P.Ov;
				P.Wy = P.Ey - P.Ov;
				P.Zw = 2.36 + 0.16 * Rg.Next();
				P.Pitch = (40.0 + 4.0 * Rg.Next()) * Pi / 180.0;
				P.Tan = std::tan(P.Pitch);
				P.CosP = std::cos(P.Pitch);
				P.SinP = std::sin(P.Pitch);
				P.Thick = 0.3 + 0.05 * Rg.Next();
				P.Tb = P.Thick / P.CosP;
				P.Ze = P.Zw + 0.05 - P.Ov * P.Tan;
				P.Top = P.Ey - 0.2;
				P.Rho = 0.2 / P.SinP;
				P.Rc = std::min(0.5, P.Top - 0.05);
				P.Snow = Smooth(0.03, 0.5, S.Snow);
				// Its colours: the wash white (a blue wash on some, a cream on others), the clay under it; the prizba's paint; one
				// paint for the casements, the band round them and the shutters (a faded blue, a faded green, an old brown or an
				// ochre-red), worn chalky; the door the same or bare grey wood.
				const double UWash = Rg.Next(), UPlinth = Rg.Next(), UFrame = Rg.Next(), UShut = Rg.Next(), UBand = Rg.Next(), UDoor = Rg.Next();
				const Col Chalk = Rgb(180, 178, 170);
				P.Wash = UWash < 0.55 ? Rgb(222, 221, 212) : UWash < 0.8 ? Rgb(208, 218, 226) : Rgb(226, 218, 196);
				// (the clay under the wash a warm grey-brown; the prizba red-brown, an earth brown, ochre or a dark blue-grey,
				// never a neutral grey: art director, 24 Nov)
				P.Clay = Rgb(140, 115, 85);
				P.Plinth = UPlinth < 0.4 ? Rgb(124, 70, 54) : UPlinth < 0.6 ? Rgb(108, 78, 60) : UPlinth < 0.8 ? Rgb(148, 120, 84) : Rgb(112, 120, 128);
				P.Plinth = Mixed(P.Plinth, Chalk, 0.1);
				P.Frame = Mixed(UFrame < 0.4 ? Rgb(84, 120, 150) : UFrame < 0.7 ? Rgb(92, 124, 92) : UFrame < 0.88 ? Rgb(104, 84, 64) : Rgb(140, 72, 52), Chalk, 0.3);
				P.bShutters = UShut < 0.55;
				P.bBand = UBand < 0.6;
				P.bBareDoor = UDoor < 0.5;
				P.Bare = Rgb(122, 116, 104);
				P.Door = P.bBareDoor ? Rgb(112, 100, 86) : P.Frame;
				const double UThatch = Rg.Next();
				P.Thatch = UThatch < 0.5 ? Rgb(100, 92, 78) : UThatch < 0.8 ? Rgb(94, 90, 82) : Rgb(106, 94, 76);
				P.Glass = Rgb(30, 34, 38);
				P.Sky = Rgb(150, 160, 170);
				P.Brick = Rgb(128, 82, 66);
				// The openings: the door into the passage near the front's middle; the living room's two windows to one side of
				// it, two more in its end wall and one at the back; the store room's small window to the other side (the plan
				// turned end for end on some, its chimney over the living room).
				const double Side = Rg.Next() < 0.5 ? 1.0 : -1.0, Dx = (-0.45 + 0.3 * (Rg.Next() - 0.5)) * Side;
				const double Ww = 0.68 + 0.08 * Rg.Next();
				P.Chx = 1.3 * Side;
				P.Chy = Rg.Next() < 0.5 ? 0.35 : -0.35;
				auto Add = [&](int F, double Along, double W, double Z0, double Z1, int What) {
					Gap G;
					G.Face = F;
					G.U = UOf(P, F, Along);
					G.W = W;
					G.Z0 = Z0;
					G.Z1 = Z1;
					G.What = What;
					G.bShut = What == 0 && P.bShutters && Rg.Next() < 0.12;
					P.Gaps.push_back(G);
				};
				Add(0, Dx, 0.86, P.Ph, P.Ph + 1.72, 1);
				const double Lo = std::fabs(Dx) + 0.43 + 0.3 + 0.5 * Ww + (P.bShutters ? 0.5 * Ww : 0.0), Hi = P.Wx - P.Wr - 0.3 - 0.5 * Ww - (P.bShutters ? 0.5 * Ww : 0.0);
				Add(0, Side * (Lo + 0.1 * (Hi - Lo)), Ww, 1.0, 1.84, 0);
				Add(0, Side * Hi, Ww, 1.0, 1.84, 0);
				Add(0, -Side * std::min(P.Wx - P.Wr - 0.6, std::fabs(Dx) + 1.5), 0.52, 1.14, 1.7, 0);
				const double Ye = std::min(1.05, P.Wy - P.Wr - 0.3 - Ww);
				Add(Side > 0 ? 3 : 1, Ye, Ww, 1.0, 1.84, 0);
				Add(Side > 0 ? 3 : 1, -Ye, Ww, 1.0, 1.84, 0);
				Add(2, Side * 0.5 * (Lo + Hi), Ww, 1.0, 1.84, 0);
				return P;
			}

			// ---------------------------------------------------------------- the walls' loop

			/** A column of the walls' loop: its foot on the line round the walls (z 0), out from it, how far round the walls it
			 * is (for the noise: the same on every loop), its face and its place along it (Face -1 round a corner). */
			struct Column
			{
				V3 B, H;
				double Pm = 0.0;
				int F = -1;
				double U = 0.0;
			};
			/** The loop round the walls, Grow out from their face (the prizba's edge), in steps of about Step along the straight
			 * and NC round each corner; the openings' edges among its columns where asked. */
			std::vector<Column> Loop(const Plan& P, double Step, int NC, double Grow, bool bEdges)
			{
				std::vector<Column> Out;
				double Pm = 0.0;
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4; ++I)
				{
					const int F = Order[I];
					const Face Fc = FaceOf(P, F), Nx = FaceOf(P, Order[(I + 1) % 4]);
					std::vector<double> Us, Edges;
					const int N = std::max(1, static_cast<int>(std::ceil(2.0 * Fc.Half / Step)));
					for (int K = 0; K < N; ++K) Us.push_back(-Fc.Half + 2.0 * Fc.Half * K / N);
					if (bEdges)
						for (const Gap& G : P.Gaps)
							if (G.Face == F)
							{
								Edges.push_back(G.U - 0.5 * G.W);
								Edges.push_back(G.U + 0.5 * G.W);
							}
					std::vector<double> All = Us;
					All.insert(All.end(), Edges.begin(), Edges.end());
					std::sort(All.begin(), All.end());
					std::vector<double> Kept;
					for (const double U : All)
					{
						const bool bEdge = std::find(Edges.begin(), Edges.end(), U) != Edges.end();
						if (!Kept.empty() && U - Kept.back() < 0.04)
						{
							if (bEdge && Kept.size() > 1) Kept.back() = U;
							continue;
						}
						Kept.push_back(U);
					}
					for (const double U : Kept) Out.push_back({ Fc.C + Fc.T * U + Fc.N * Grow, Fc.N, Pm + U + Fc.Half, F, U });
					Pm += 2.0 * Fc.Half;
					const V3 Cc = Fc.C + Fc.T * Fc.Half - Fc.N * P.Wr;
					for (int K = 0; K < NC; ++K)
					{
						const double A = 0.5 * Pi * K / NC;
						const V3 H = Fc.N * std::cos(A) + Nx.N * std::sin(A);
						Out.push_back({ Cc + H * (P.Wr + Grow), H, Pm + P.Wr * A, K == 0 ? F : -1, K == 0 ? Fc.Half : 0.0 });
					}
					Pm += 0.5 * Pi * P.Wr;
				}
				return Out;
			}
			/** How far the clay stands out of the walls' true line at a place (round the walls Pm, up Z): it was daubed by hand,
			 * a little wavy, a little bellied. */
			double Disp(const Plan& P, double Pm, double Z)
			{
				return 0.012 * Noise(Pm / 0.8, Z / 0.7, P.S.Key ^ 0xD1) + 0.005 * Noise(Pm / 0.22, Z / 0.25, P.S.Key ^ 0xD2) + 0.008 * std::sin(Pi * Clamp01((Z - P.Ph) / (P.Zw - P.Ph)));
			}
			V3 WallPt(const Plan& P, const Column& C, double Z, double Off = 0.0) { return C.B + C.H * (Disp(P, C.Pm, Z) + Off) + V3{ 0.0, 0.0, Z }; }
			V3 WallN(const Plan& P, const Column& C, double Z)
			{
				const V3 T = Cross(Up, C.H);
				const double Du = (Disp(P, C.Pm + 0.02, Z) - Disp(P, C.Pm - 0.02, Z)) / 0.04, Dz = (Disp(P, C.Pm, Z + 0.02) - Disp(P, C.Pm, Z - 0.02)) / 0.04;
				return Norm(C.H - T * Du - Up * Dz);
			}
			/** A place on a straight face, as a column: its way along U, its face F. */
			Column At(const Plan& P, int F, double U)
			{
				const Face Fc = FaceOf(P, F);
				double Pm = 0.0;
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4 && Order[I] != F; ++I) Pm += 2.0 * FaceOf(P, Order[I]).Half + 0.5 * Pi * P.Wr;
				return { Fc.C + Fc.T * U, Fc.N, Pm + U + Fc.Half, F, U };
			}

			/** The limewash's colour at a place: greyed in long streaks down it, dirty low down from the splash and under
			 * each window's sill. (Where it has flaked to the clay is Wear's: crisp patches, never soft stains.) */
			Col WashCol(const Plan& P, const Column& C, double Z)
			{
				Col W = Scaled(P.Wash, 0.95 + 0.07 * Noise01(C.Pm / 1.5, Z / 1.2, P.S.Key ^ 0xA1));
				W = Mixed(W, Rgb(160, 156, 146), 0.3 * Smooth(0.58, 0.85, Noise01(C.Pm / 0.45, Z / 2.6, P.S.Key ^ 0xA2)));
				W = Mixed(W, Rgb(150, 136, 114), 0.55 * Smooth(P.Ph + 0.5, P.Ph, Z));
				// (grey streaks run down from the eaves' drip)
				W = Mixed(W, Rgb(150, 148, 140), 0.22 * Smooth(P.Zw - 1.2, P.Zw - 0.1, Z) * Smooth(0.55, 0.8, Noise01(C.Pm / 0.18, Z / 3.0, P.S.Key ^ 0xA4)));
				for (const Gap& G : P.Gaps)
					if (G.What == 0 && G.Face == C.F && std::fabs(C.U - G.U) < 0.5 * G.W + 0.05 && Z < G.Z0)
						W = Mixed(W, Rgb(138, 130, 116), 0.4 * Smooth(G.Z0 - 0.9, G.Z0 - 0.06, Z) * Smooth(0.5 * G.W + 0.05, 0.3 * G.W, std::fabs(C.U - G.U)));
				return W;
			}
			/** The prizba's paint at a place (its column C, up Z; Edge 0 to 1, how near the top's outer edge, where feet and
			 * rain wear it): worn, rubbed through to the clay in patches, most on that edge, round the corners and by the
			 * door's step; a damp band at its foot, its top ragged with the mud splashed up. */
			Col PlinthCol(const Plan& P, const Column& C, double Z, double Edge)
			{
				Col K = Scaled(P.Plinth, 0.93 + 0.1 * Noise01(C.Pm / 0.7, Z / 0.4, P.S.Key ^ 0xB1));
				double Rub = Noise01(C.Pm / 0.3, Z / 0.5, P.S.Key ^ 0xB2) + 0.16 * Edge + (C.F < 0 ? 0.1 : 0.0);
				for (const Gap& G : P.Gaps)
					if (G.What == 1 && G.Face == C.F) Rub += 0.2 * Smooth(0.75, 0.3, std::fabs(C.U - G.U)) * Edge;
				K = Mixed(K, Mixed(Scaled(P.Clay, 0.9), P.Plinth, 0.35), 0.8 * Smooth(0.74, 0.8, Rub));
				const double Wet = 0.12 + 0.1 * Noise01(C.Pm / 0.2, 0.5, P.S.Key ^ 0xB3);
				return Mixed(K, Rgb(74, 64, 52), 0.62 * Smooth(Wet + 0.04, Wet - 0.04, Z));
			}
			/** The prizba's lie at a place round the walls (Pm): how far its top stands above or below its height, and its edge
			 * out or in from its reach; daubed by hand, a few cm each way. */
			double PrizUp(const Plan& P, double Pm) { return 0.018 * Noise(Pm / 1.1, 0.5, P.S.Key ^ 0xB5) + 0.007 * Noise(Pm / 0.35, 1.5, P.S.Key ^ 0xB6); }
			double PrizOut(const Plan& P, double Pm) { return 0.022 * Noise(Pm / 1.4, 2.5, P.S.Key ^ 0xB7); }

			/** Its rows up the walls: every 20 cm or so from under the prizba's top to the walls' top, and each opening's sill
			 * and head among them. */
			std::vector<double> Rows(const Plan& P, double Step)
			{
				std::vector<double> All, Edges;
				const double Z0 = P.Ph - 0.06;
				const int N = std::max(1, static_cast<int>(std::ceil((P.Zw - Z0) / Step)));
				for (int K = 0; K <= N; ++K) All.push_back(Z0 + (P.Zw - Z0) * K / N);
				for (const Gap& G : P.Gaps)
				{
					Edges.push_back(G.Z0);
					Edges.push_back(G.Z1);
				}
				All.insert(All.end(), Edges.begin(), Edges.end());
				std::sort(All.begin(), All.end());
				std::vector<double> Kept;
				for (const double Z : All)
				{
					const bool bEdge = std::find(Edges.begin(), Edges.end(), Z) != Edges.end();
					if (!Kept.empty() && Z - Kept.back() < 0.04)
					{
						if (bEdge && Kept.size() > 1) Kept.back() = Z;
						continue;
					}
					Kept.push_back(Z);
				}
				if (Kept.back() < P.Zw - 1e-6) Kept.back() = P.Zw;
				return Kept;
			}
			/** Whether a cell of the walls is in an opening. */
			bool InGap(const Plan& P, const Column& A, const Column& Bc, double Za, double Zb)
			{
				if (A.F < 0 || A.F != Bc.F) return false;
				const double Um = 0.5 * (A.U + Bc.U), Zm = 0.5 * (Za + Zb);
				for (const Gap& G : P.Gaps)
					if (G.Face == A.F && Um > G.U - 0.5 * G.W && Um < G.U + 0.5 * G.W && Zm > G.Z0 && Zm < G.Z1) return true;
				return false;
			}

			// ---------------------------------------------------------------- the roof's rings

			/** A place round the roof's ring at an inset: on the eaves' rounded rectangle drawn in by the inset (z 0), out from
			 * it, and how far round it the eaves' own ring has its place (for the straw's lines down the slope). */
			struct RingPt
			{
				V3 P, H;
				double U = 0.0;
			};
			std::vector<RingPt> RingAt(const Plan& P, double Inset, int NL, int NS, int NC)
			{
				std::vector<RingPt> Out;
				const double A = P.Ex - Inset, Bh = P.Ey - Inset, R = std::clamp(std::min(P.Rc, Bh - 0.02), 0.0, P.Rc);
				const double Ra = std::max(0.0, A - R), Rb = std::max(0.0, Bh - R);
				const double Per = 4.0 * (P.Ex - P.Rc) + 4.0 * (P.Ey - P.Rc) + 2.0 * Pi * P.Rc;
				const int N = 2 * NL + 2 * NS + 4 * NC;
				auto Push = [&](V3 Pt, V3 H) { Out.push_back({ Pt, H, Per * static_cast<double>(Out.size()) / N }); };
				for (int K = 0; K < NS; ++K) Push(V3{ A, -Rb + 2.0 * Rb * K / NS, 0.0 }, V3{ 1.0, 0.0, 0.0 });
				auto Corner = [&](double Cx, double Cy, double A0) {
					for (int K = 0; K < NC; ++K)
					{
						const double An = A0 + 0.5 * Pi * K / NC;
						const V3 H{ std::cos(An), std::sin(An), 0.0 };
						Push(V3{ Cx, Cy, 0.0 } + H * R, H);
					}
				};
				Corner(Ra, Rb, 0.0);
				for (int K = 0; K < NL; ++K) Push(V3{ Ra - 2.0 * Ra * K / NL, Bh, 0.0 }, V3{ 0.0, 1.0, 0.0 });
				Corner(-Ra, Rb, 0.5 * Pi);
				for (int K = 0; K < NS; ++K) Push(V3{ -A, Rb - 2.0 * Rb * K / NS, 0.0 }, V3{ -1.0, 0.0, 0.0 });
				Corner(-Ra, -Rb, Pi);
				for (int K = 0; K < NL; ++K) Push(V3{ -Ra + 2.0 * Ra * K / NL, -Bh, 0.0 }, V3{ 0.0, -1.0, 0.0 });
				Corner(Ra, -Rb, 1.5 * Pi);
				return Out;
			}
			/** The thatch's top over an inset (its outer face, before any lift); its underside. */
			double TopZ(const Plan& P, double Inset) { return P.Ze + P.Tb + Inset * P.Tan; }
			double UnderZ(const Plan& P, double Inset) { return P.Ze + Inset * P.Tan; }
			V3 SlopeN(const Plan& P, V3 H) { return H * P.SinP + Up * P.CosP; }
			/** The ridge's roll: its ring J of NR (0 at the slopes' top, NR at the ridge): its inset, its rise over the slopes' top,
			 * the way out from it (as the slope's normal turns up). */
			void RollAt(const Plan& P, int J, int NR, double& Inset, double& Rise, double& Th)
			{
				Th = P.Pitch * (1.0 - static_cast<double>(J) / NR);
				Inset = P.Top + 0.2 - P.Rho * std::sin(Th);
				Rise = P.Rho * (std::cos(Th) - std::cos(P.Pitch));
			}

			/** The straw's colour at a place (round the roof U, down the slope from its top E, the ring's way out H): its lines
			 * combed down the slope (where Fine), weathered broadly, greyer toward the eaves, moss on the back. */
			Col StrawAt(const Plan& P, double U, double E, V3 H, bool bFine)
			{
				Col C = bFine ? Scaled(P.Thatch, 0.92 + 0.14 * Noise01(U / 0.05, E / 0.9, P.S.Key ^ 0x57B0)) : P.Thatch;
				C = Scaled(C, 0.94 + 0.1 * Noise01(U / 0.9, E / 0.7, P.S.Key ^ 0x57B5));
				const double Eb = (P.Top) / P.CosP;
				C = Mixed(C, Rgb(124, 116, 102), 0.25 * Smooth(0.3 * Eb, Eb, E));
				C = Mixed(C, Rgb(80, 88, 60), (0.12 + 0.3 * Smooth(0.2, -0.8, H.Y)) * Smooth(0.6, 0.82, Noise01(U / 1.1, E / 0.9, P.S.Key ^ 0x57B1)));
				return Mixed(C, Rgb(170, 150, 110), 0.2 * Smooth(0.72, 0.9, Noise01(U / 0.8, E / 0.6, P.S.Key ^ 0x57B2)));
			}

			// ---------------------------------------------------------------- the near level

			void Walls(Pen& B, const Plan& P)
			{
				const std::vector<Column> L = Loop(P, 0.16, 4, 0.0, true);
				const std::vector<double> Z = Rows(P, 0.18);
				const size_t NCl = L.size(), NR = Z.size();
				std::vector<uint32_t> Id(NCl * NR);
				for (size_t C = 0; C < NCl; ++C)
					for (size_t R = 0; R < NR; ++R) Id[C * NR + R] = B.Vert(WallPt(P, L[C], Z[R]), WallN(P, L[C], Z[R]), WashCol(P, L[C], Z[R]));
				for (size_t C = 0; C < NCl; ++C)
				{
					const size_t D = (C + 1) % NCl;
					for (size_t R = 0; R + 1 < NR; ++R)
					{
						if (InGap(P, L[C], L[D], Z[R], Z[R + 1])) continue;
						const V3 Hm = Norm(L[C].H + L[D].H);
						B.Tri(Id[C * NR + R], Id[D * NR + R], Id[D * NR + R + 1], Hm);
						B.Tri(Id[C * NR + R], Id[D * NR + R + 1], Id[C * NR + R + 1], Hm);
					}
				}
				// (each opening's reveal: the clay turned in to the frame, the wash on it, its head in shadow)
				for (const Gap& G : P.Gaps)
				{
					const double Dp = G.What == 1 ? 0.1 : P.Df;
					std::vector<const Column*> On;
					for (const Column& C : L)
						if (C.F == G.Face && C.U > G.U - 0.5 * G.W - 1e-6 && C.U < G.U + 0.5 * G.W + 1e-6) On.push_back(&C);
					std::vector<double> Zs;
					for (const double Zr : Z)
						if (Zr > G.Z0 - 1e-6 && Zr < G.Z1 + 1e-6) Zs.push_back(Zr);
					if (On.size() < 2 || Zs.size() < 2) continue;
					const Face Fc = FaceOf(P, G.Face);
					auto In = [&](const Column& C, double Zr) { return C.B - C.H * Dp + V3{ 0.0, 0.0, Zr }; };
					for (size_t K = 0; K + 1 < On.size(); ++K)
					{
						const Column &Ca = *On[K], &Cb = *On[K + 1];
						B.Quad(WallPt(P, Ca, G.Z1), WallPt(P, Cb, G.Z1), In(Cb, G.Z1), In(Ca, G.Z1), Scaled(WashCol(P, Ca, G.Z1), 0.66), Scaled(WashCol(P, Cb, G.Z1), 0.66), Scaled(P.Wash, 0.6), Scaled(P.Wash, 0.6), V3{ 0.0, 0.0, -1.0 });
						if (G.What == 0) B.Quad(WallPt(P, Ca, G.Z0), WallPt(P, Cb, G.Z0), In(Cb, G.Z0), In(Ca, G.Z0), Scaled(WashCol(P, Ca, G.Z0), 0.92), Scaled(WashCol(P, Cb, G.Z0), 0.92), Scaled(P.Wash, 0.85), Scaled(P.Wash, 0.85), Up);
					}
					for (int End = 0; End < 2; ++End)
					{
						const Column& C = End == 0 ? *On.front() : *On.back();
						const V3 Inw = Fc.T * (End == 0 ? 1.0 : -1.0);
						for (size_t K = 0; K + 1 < Zs.size(); ++K)
							B.Quad(WallPt(P, C, Zs[K]), WallPt(P, C, Zs[K + 1]), In(C, Zs[K + 1]), In(C, Zs[K]), Scaled(WashCol(P, C, Zs[K]), 0.8), Scaled(WashCol(P, C, Zs[K + 1]), 0.8), Scaled(P.Wash, 0.74), Scaled(P.Wash, 0.74), Inw);
					}
				}
			}

			/** A flake of the wash come away at (U, Z) on a face, Off out from it: the clay bared under it (C), shadowed toward
			 * its rim under the paint's broken edge, and that edge itself standing a few mm proud and catching the light (art
			 * director, 24 Nov: crisp, never soft stains); N corners round, each its own reach (Rx across, Rz up). */
			void Patch(Pen& B, const Plan& P, int F, double U, double Z, double Rx, double Rz, double Off, const Col& C, Rng& R)
			{
				const Column Cm = At(P, F, U);
				const uint32_t Cn = B.Vert(WallPt(P, Cm, Z, Off), WallN(P, Cm, Z), C);
				std::vector<uint32_t> Rim, Brk;
				const int N = 11;
				for (int K = 0; K < N; ++K)
				{
					const double Th = 2.0 * Pi * K / N, Rr = R.Range(0.75, 1.15), Ex = R.Range(0.006, 0.011);
					const double Cs = std::cos(Th), Sn = std::sin(Th);
					const Column Ck = At(P, F, U + Rx * Rr * Cs), Cl = At(P, F, U + (Rx * Rr + Ex) * Cs);
					const double Zk = Z + Rz * Rr * Sn, Zl = Z + (Rz * Rr + Ex) * Sn;
					Rim.push_back(B.Vert(WallPt(P, Ck, Zk, Off), WallN(P, Ck, Zk), Scaled(C, 0.78)));
					Brk.push_back(B.Vert(WallPt(P, Cl, Zl, Off + 0.003), WallN(P, Cl, Zl), Scaled(WashCol(P, Cl, Zl), 1.05)));
				}
				for (size_t K = 0; K < static_cast<size_t>(N); ++K)
				{
					const size_t J = (K + 1) % static_cast<size_t>(N);
					B.Tri(Cn, Rim[K], Rim[J], Cm.H);
					B.Tri(Rim[K], Brk[K], Brk[J], Cm.H);
					B.Tri(Rim[K], Brk[J], Rim[J], Cm.H);
				}
			}
			/** Whether a place on a face keeps clear of its openings by Pad. */
			bool Clear(const Plan& P, int F, double U, double Z, double Pad)
			{
				const Face Fc = FaceOf(P, F);
				if (std::fabs(U) > Fc.Half - Pad) return false;
				for (const Gap& G : P.Gaps)
					if (G.Face == F && std::fabs(U - G.U) < 0.5 * G.W + Pad + (P.bShutters && G.What == 0 ? 0.5 * G.W + 0.05 : 0.0) && Z > G.Z0 - Pad - 0.1 && Z < G.Z1 + Pad) return false;
				return true;
			}
			/** The wash come away to the clay in crisp patches (more low down); the cracks from the openings' corners. */
			void Wear(Pen& B, const Plan& P)
			{
				Rng R(P.S.Key ^ 0xF1A4);
				const int NF = 7 + static_cast<int>(6.0 * R.Next());
				for (int K = 0; K < NF; ++K)
				{
					const int F = R.Next() < 0.7 ? (R.Next() < 0.5 ? 0 : 2) : (R.Next() < 0.5 ? 1 : 3);
					const Face Fc = FaceOf(P, F);
					const double U = R.Range(-Fc.Half, Fc.Half), Lo = R.Next(), Z = P.Ph + 0.05 + (P.Zw - P.Ph - 0.4) * Lo * Lo * Lo;
					const double Rx = R.Range(0.03, 0.12), Rz = Rx * R.Range(0.5, 1.1);
					if (!Clear(P, F, U, Z, Rx + 0.05)) continue;
					const double Tn = R.Range(0.86, 1.04), Hu = 0.03 * (R.Next() - 0.5);
					Patch(B, P, F, U, Z, Rx, Rz, 0.004, Varied(P.Clay, Tn, Hu), R);
				}
				for (const Gap& G : P.Gaps)
				{
					if (G.What != 0 || R.Next() < 0.35) continue;
					const double Sx = R.Next() < 0.5 ? -1.0 : 1.0, Sz = R.Next() < 0.6 ? -1.0 : 1.0;
					double U = G.U + Sx * (0.5 * G.W + 0.02), Z = Sz < 0.0 ? G.Z0 - 0.02 : G.Z1 + 0.02;
					if (P.bShutters) U += Sx * (0.5 * G.W + 0.04);
					const int NS = 4 + static_cast<int>(3.0 * R.Next());
					for (int K = 0; K < NS; ++K)
					{
						const double Ln = R.Range(0.06, 0.14), Dir = R.Range(0.3, 1.2);
						const double U1 = U + Sx * Ln * std::cos(Dir), Z1 = Z + Sz * Ln * std::sin(Dir);
						if (Z1 < P.Ph + 0.05 || Z1 > P.Zw - 0.05 || std::fabs(U1) > FaceOf(P, G.Face).Half - 0.05) break;
						const Column Ca = At(P, G.Face, U), Cb = At(P, G.Face, U1);
						const double Wd = 0.004 * (1.0 - 0.15 * K);
						const V3 Sa = WallPt(P, Ca, Z, 0.005), Sb = WallPt(P, Cb, Z1, 0.005);
						const V3 Across = Norm(Cross(Ca.H, Sb - Sa)) * Wd;
						B.Quad1(Sa - Across, Sb - Across * 0.7, Sb + Across * 0.7, Sa + Across, Rgb(74, 66, 58), Ca.H);
						U = U1;
						Z = Z1;
					}
				}
			}

			/** The prizba: a clay bench round the walls' foot, its top falling a little outward, its edge rounded, its face
			 * down into the ground; painted, worn. */
			void Prizba(Pen& B, const Plan& P, double Step, int NC, bool bFine)
			{
				// Its profile from the wall out and down (out from the walls' face, up): the top falling away from the wall, its
				// edge rolled round softly into the face, the face down into the ground; Edge, how worn each row is by feet and
				// rain. The middle level keeps every other row.
				struct Row
				{
					double G, Z, Edge;
					bool bTop, bLie;
				};
				const double Bot = -std::max(0.2, P.S.Below);
				const std::vector<Row> All = {
					{ -0.03, 0.0, 0.0, true, false },
					{ 0.55 * P.Pb, -0.022, 0.3, true, true },
					{ P.Pb - 0.07, -0.04, 0.85, true, true },
					{ P.Pb - 0.025, -0.058, 1.0, true, true },
					{ P.Pb, -0.1, 0.75, true, true },
					{ P.Pb + 0.004, 0.27, 0.15, false, true },
					{ P.Pb + 0.008, 0.17, 0.0, false, true },
					{ P.Pb + 0.014, 0.08, 0.0, false, true },
					{ P.Pb + 0.03, Bot, 0.0, false, true },
				};
				std::vector<Row> Rw;
				for (size_t I = 0; I < All.size(); ++I)
					if (bFine || I % 2 == 0) Rw.push_back(All[I]);
				const std::vector<Column> L = Loop(P, Step, NC, 0.0, false);
				const size_t N = L.size(), NR = Rw.size();
				// (its lie: a few cm up or down and out or in, held true by the door so its step and threshold sit on it)
				const Gap* Dr = nullptr;
				for (const Gap& G : P.Gaps)
					if (G.What == 1) Dr = &G;
				auto Held = [&](const Column& C) { return Dr && C.F == Dr->Face ? Smooth(0.5 * Dr->W, 0.5 * Dr->W + 0.5, std::fabs(C.U - Dr->U)) : 1.0; };
				auto Place = [&](const Column& C, const Row& R) {
					const double Hd = Held(C), Zt = P.Ph + PrizUp(P, C.Pm) * Hd;
					const double G = R.G + (R.bLie ? PrizOut(P, C.Pm) * Hd : 0.0);
					return std::pair<double, double>{ G, R.bTop ? Zt + R.Z : R.Z };
				};
				std::vector<uint32_t> Id(N * NR);
				for (size_t C = 0; C < N; ++C)
					for (size_t R = 0; R < NR; ++R)
					{
						const auto [G, Z] = Place(L[C], Rw[R]);
						const auto [Ga, Za] = Place(L[C], Rw[R == 0 ? 0 : R - 1]);
						const auto [Gb, Zb] = Place(L[C], Rw[R + 1 < NR ? R + 1 : R]);
						// (the way out from it: across its profile, smooth over the rolled edge)
						const double Dg = Gb - Ga, Dz = Zb - Za, Ln = std::max(1e-9, std::sqrt(Dg * Dg + Dz * Dz));
						const V3 Nv = Norm(L[C].H * (-Dz / Ln) + Up * (Dg / Ln));
						// (the middle level's paint a shade darker: its five rows' smoother normals light it some 16 lighter than the
						// near's at the switch; art director, 24 Nov)
						Id[C * NR + R] = B.Vert(L[C].B + L[C].H * G + V3{ 0.0, 0.0, Z }, Nv, Scaled(PlinthCol(P, L[C], Z, Rw[R].Edge), bFine ? 1.0 : 0.89));
					}
				for (size_t C = 0; C < N; ++C)
				{
					const size_t D = (C + 1) % N;
					const V3 Hm = Norm(L[C].H + L[D].H);
					for (size_t R = 0; R + 1 < NR; ++R)
					{
						const auto [Ga, Za] = Place(L[C], Rw[R]);
						const auto [Gb, Zb] = Place(L[C], Rw[R + 1]);
						const V3 To = Hm * (Za - Zb) + Up * (Gb - Ga);
						B.Tri(Id[C * NR + R], Id[D * NR + R], Id[D * NR + R + 1], To);
						B.Tri(Id[C * NR + R], Id[D * NR + R + 1], Id[C * NR + R + 1], To);
					}
				}
				if (!bFine) return;
				// (the mud splashed up its face from the yard: small dark spots over the damp band)
				Rng Rs(P.S.Key ^ 0xB9);
				for (int K = 0; K < 30; ++K)
				{
					const size_t Ca = static_cast<size_t>(Rs.Next() * static_cast<double>(N)) % N, Cb = (Ca + 1) % N;
					const double T = Rs.Next(), Sz = Rs.Range(0.006, 0.02), An = Rs.Range(0.0, 2.0 * Pi), Up_ = Rs.Range(0.0, 0.16), Tn = Rs.Range(0.8, 1.1);
					const Column& A = L[Ca];
					const V3 H = Norm(A.H + (L[Cb].H - A.H) * T), Tg = Norm(Cross(Up, H));
					const double Pm = A.Pm + (L[Cb].Pm - A.Pm) * T, Hd = Held(A);
					const double Z = 0.12 + 0.1 * Noise01(Pm / 0.2, 0.5, P.S.Key ^ 0xB3) + Up_;
					if (Z > P.Ph - 0.14) continue;
					const V3 Ct = A.B + (L[Cb].B - A.B) * T + H * (P.Pb + 0.018 + PrizOut(P, Pm) * Hd) + V3{ 0.0, 0.0, Z };
					auto Pt = [&](double Ax) { return Ct + (Tg * std::cos(An + Ax) + Up * std::sin(An + Ax)) * Sz; };
					B.Tri1(Pt(0.0), Pt(2.2), Pt(4.1), Scaled(Rgb(82, 70, 56), Tn), H);
				}
			}

			/** A window deep in the clay: its frame and casements in the paint (two leaves, three panes each), the sky in its
			 * glass and a curtain behind the lower panes, its sill board out over the wash, its painted band round it on the
			 * wall, its plank shutters open flat either side (or shut). */
			void Window(Pen& B, const Plan& P, const Gap& G, uint64_t Seed)
			{
				const Column Cm = At(P, G.Face, G.U);
				const Face Fc = FaceOf(P, G.Face);
				const V3 T = Fc.T, N = Fc.N;
				auto Pt = [&](double U, double Z, double D) { return Fc.C + T * U + N * D + V3{ 0.0, 0.0, Z }; };
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Z0 = G.Z0, Z1 = G.Z1, D = -P.Df;
				const Col Fr = Mixed(P.Frame, P.Bare, 0.1 + 0.2 * Unit(Seed));
				auto Bx = [&](double U, double Z, double Dd, double HU, double HD, double HZ, const Col& C) { B.Box(Pt(U, Z, Dd), T * HU, N * HD, V3{ 0.0, 0.0, HZ }, C, 0); };
				// (the glass: the sky in the upper panes, a white curtain behind the lower)
				const double M = 0.25 + 0.35 * Noise01(G.U * 3.0, Z0, Seed);
				const Col Hi = Mixed(P.Glass, P.Sky, M + 0.2), Lo = Mixed(P.Glass, P.Sky, M * 0.6), Cu = Mixed(Rgb(178, 174, 164), P.Sky, 0.12);
				if (G.bShut)
					B.Quad1(Pt(U0, Z0, D - 0.03), Pt(U1, Z0, D - 0.03), Pt(U1, Z1, D - 0.03), Pt(U0, Z1, D - 0.03), P.Glass, N);
				else
				{
					const double Zc = Z0 + 0.45 * (Z1 - Z0);
					B.Quad(Pt(U0, Z0, D - 0.03), Pt(U1, Z0, D - 0.03), Pt(U1, Zc, D - 0.03), Pt(U0, Zc, D - 0.03), Scaled(Cu, 0.78), Scaled(Cu, 0.78), Cu, Cu, N);
					B.Quad(Pt(U0, Zc, D - 0.03), Pt(U1, Zc, D - 0.03), Pt(U1, Z1, D - 0.03), Pt(U0, Z1, D - 0.03), Lo, Lo, Hi, Hi, N);
				}
				// (the frame round it and the casements' bars: two leaves, each in three panes)
				const double Bw = 0.03;
				Bx(U0 + Bw, 0.5 * (Z0 + Z1), D, Bw, 0.03, 0.5 * (Z1 - Z0), Fr);
				Bx(U1 - Bw, 0.5 * (Z0 + Z1), D, Bw, 0.03, 0.5 * (Z1 - Z0), Fr);
				Bx(G.U, Z1 - Bw, D, 0.5 * G.W, 0.03, Bw, Fr);
				Bx(G.U, Z0 + Bw, D, 0.5 * G.W, 0.03, Bw, Worn(Fr, P.Bare, 0.5));
				Bx(G.U, 0.5 * (Z0 + Z1), D + 0.005, 0.016, 0.025, 0.5 * (Z1 - Z0) - Bw, Fr);
				for (const double Zb : { Z0 + (Z1 - Z0) / 3.0, Z0 + 2.0 * (Z1 - Z0) / 3.0 }) Bx(G.U, Zb, D + 0.005, 0.5 * G.W - Bw, 0.018, 0.011, Scaled(Fr, 0.95));
				// (the sill board, out over the wash, its paint gone to the wood)
				Bx(G.U, Z0 - 0.015, -0.5 * P.Df + 0.04, 0.5 * G.W + 0.05, 0.5 * P.Df + 0.06, 0.015, Worn(Fr, P.Bare, 0.85));
				// (the band of paint round it on the clay)
				if (P.bBand)
				{
					const double Bd = 0.085, Gx = P.bShutters ? 0.0 : 0.0;
					const Col Bc = Mixed(P.Frame, P.Wash, 0.12);
					auto Strip = [&](double Ua, double Ub, double Za, double Zb) {
						const int NU = std::max(1, static_cast<int>(std::ceil((Ub - Ua) / 0.1))), NZ = std::max(1, static_cast<int>(std::ceil((Zb - Za) / 0.1)));
						for (int I = 0; I < NU; ++I)
							for (int J = 0; J < NZ; ++J)
							{
								const double A0 = Ua + (Ub - Ua) * I / NU, A1 = Ua + (Ub - Ua) * (I + 1) / NU, B0 = Za + (Zb - Za) * J / NZ, B1 = Za + (Zb - Za) * (J + 1) / NZ;
								const Column C0 = At(P, G.Face, A0), C1 = At(P, G.Face, A1);
								B.Quad1(WallPt(P, C0, B0, 0.005), WallPt(P, C1, B0, 0.005), WallPt(P, C1, B1, 0.005), WallPt(P, C0, B1, 0.005), Varied(Bc, 0.95 + 0.08 * Noise01(A0 / 0.2, B0 / 0.2, Seed), 0.0), N);
							}
					};
					Strip(U0 - Bd - Gx, U1 + Bd + Gx, Z1, Z1 + Bd);
					Strip(U0 - Bd - Gx, U1 + Bd + Gx, Z0 - Bd, Z0 - 0.03);
					Strip(U0 - Bd - Gx, U0, Z0 - 0.03, Z1);
					Strip(U1, U1 + Bd + Gx, Z0 - 0.03, Z1);
				}
				if (!P.bShutters) return;
				// (the shutters: two of planks, ledged, open flat on the clay either side, or shut across it)
				const double Lw = 0.5 * G.W + 0.01, Lh = Z1 - Z0 + 0.08, Zm = 0.5 * (Z0 + Z1);
				const Col Sc = Mixed(P.Frame, P.Bare, 0.15 + 0.25 * Unit(Seed ^ 0x5C));
				for (int Side = -1; Side <= 1; Side += 2)
				{
					const double Um = G.bShut ? G.U + Side * 0.5 * Lw : (Side < 0 ? U0 - 0.5 * Lw - 0.02 : U1 + 0.5 * Lw + 0.02);
					const double Dd = G.bShut ? 0.03 : 0.04 + Disp(P, Cm.Pm + (Um - G.U), Zm);
					for (int K = 0; K < 3; ++K)
					{
						const double Ua = Um - 0.5 * Lw + Lw * K / 3.0 + 0.004, Ub = Um - 0.5 * Lw + Lw * (K + 1) / 3.0 - 0.004;
						Bx(0.5 * (Ua + Ub), Zm, Dd, 0.5 * (Ub - Ua), 0.012, 0.5 * Lh, Varied(Sc, 0.93 + 0.1 * Unit(Seed ^ static_cast<uint64_t>(K * 2 + Side + 7)), 0.0));
					}
					Bx(Um, Zm - 0.5 * Lh + 0.08, Dd + 0.016, 0.5 * Lw - 0.02, 0.006, 0.035, Worn(Sc, P.Bare, 0.8));
					Bx(Um, Zm + 0.5 * Lh - 0.08, Dd + 0.016, 0.5 * Lw - 0.02, 0.006, 0.035, Scaled(Sc, 0.92));
					const double Hu = G.bShut ? G.U + Side * (Lw - 0.06) : (Side < 0 ? U0 - 0.04 : U1 + 0.04);
					for (const double Zh : { Zm - 0.5 * Lh + 0.08, Zm + 0.5 * Lh - 0.08 }) Bx(0.5 * (Hu + Um), Zh, Dd + 0.022, 0.5 * std::fabs(Um - Hu) + 0.03, 0.004, 0.011, Rgb(44, 40, 36));
				}
			}

			/** The door: a plank door set in its frame, ledged and braced, a ring for its latch; the threshold on the prizba;
			 * a step before it. */
			void Door(Pen& B, const Plan& P, const Gap& G, bool bFine)
			{
				const Face Fc = FaceOf(P, G.Face);
				const V3 T = Fc.T, N = Fc.N;
				auto Pt = [&](double U, double Z, double D) { return Fc.C + T * U + N * D + V3{ 0.0, 0.0, Z }; };
				auto Bx = [&](double U, double Z, double Dd, double HU, double HD, double HZ, const Col& C) { B.Box(Pt(U, Z, Dd), T * HU, N * HD, V3{ 0.0, 0.0, HZ }, C, 0); };
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Z0 = G.Z0, Z1 = G.Z1, D = -0.1;
				const Col Fr = Mixed(P.Frame, P.Bare, 0.2);
				if (bFine)
				{
					const int NP = 5;
					for (int K = 0; K < NP; ++K)
					{
						const double Ua = U0 + 0.04 + (G.W - 0.08) * K / NP + 0.003, Ub = U0 + 0.04 + (G.W - 0.08) * (K + 1) / NP - 0.003;
						Bx(0.5 * (Ua + Ub), 0.5 * (Z0 + Z1) - 0.01, D, 0.5 * (Ub - Ua), 0.02, 0.5 * (Z1 - Z0) - 0.04, Varied(P.Door, 0.9 + 0.12 * Unit(Hash(P.S.Key ^ 0xD00, static_cast<uint64_t>(K))), 0.0));
					}
					for (const double Zl : { Z0 + 0.25, 0.5 * (Z0 + Z1), Z1 - 0.3 }) Bx(G.U, Zl, D + 0.025, 0.5 * G.W - 0.08, 0.01, 0.05, Scaled(P.Door, 0.88));
					const V3 Ra = Pt(U0 + 0.12, Z0 + 0.3, D + 0.03), Rb = Pt(U1 - 0.12, 0.5 * (Z0 + Z1) - 0.05, D + 0.03);
					B.Beam(Ra, Rb, 0.09, 0.02, Scaled(P.Door, 0.86), N);
					B.Tube(Pt(U1 - 0.14, 0.5 * (Z0 + Z1) + 0.05, D + 0.045), Pt(U1 - 0.14, 0.5 * (Z0 + Z1) + 0.05, D + 0.06), 0.035, 0.035, 8, Rgb(46, 42, 38), true);
				}
				else B.Quad1(Pt(U0, Z0, D + 0.02), Pt(U1, Z0, D + 0.02), Pt(U1, Z1, D + 0.02), Pt(U0, Z1, D + 0.02), Scaled(P.Door, 0.9), N);
				// (its frame: two posts and a lintel in the paint, a little proud of the clay)
				Bx(U0 - 0.04, 0.5 * (Z0 + Z1) + 0.02, 0.0, 0.05, 0.04, 0.5 * (Z1 - Z0) + 0.06, Fr);
				Bx(U1 + 0.04, 0.5 * (Z0 + Z1) + 0.02, 0.0, 0.05, 0.04, 0.5 * (Z1 - Z0) + 0.06, Fr);
				Bx(G.U, Z1 + 0.06, 0.0, 0.5 * G.W + 0.12, 0.045, 0.05, Fr);
				// (the threshold, and a step of old planks on a stone before the prizba)
				Bx(G.U, Z0 + 0.015, -0.04, 0.5 * G.W, 0.08, 0.015, Rgb(104, 94, 80));
				Bx(G.U, 0.11, P.Pb + 0.2, 0.55, 0.18, 0.11, Rgb(118, 110, 98));
				if (bFine) Bx(G.U, 0.235, P.Pb + 0.2, 0.52, 0.17, 0.015, Rgb(108, 96, 80));
			}

			/** The thatch: its slopes course by course round the hips (each ring of the roof a rounded rectangle), the straws
			 * combed down, each course's butts a little proud of the next; the ridge rolled round; the eaves rounded over into a
			 * clean-cut face, straight along its foot, the underside dark over the rafters' ends. */
			void Thatch(Pen& B, const Plan& P)
			{
				const int NL = 36, NS = 24, NC = 5, NR = 4;
				const double Rl = 0.12, Lip = 0.016, Step = 0.3, Shade = 0.06;
				const double Eb = P.Top / P.CosP, Er = Rl; // (down the slope from its top: the slopes' length; where the eaves' roll begins, from the foot)
				const int NQ = std::max(2, static_cast<int>(std::ceil((Eb - Er) / Step)));
				const std::vector<RingPt> Shape = RingAt(P, 0.0, NL, NS, NC);
				const size_t N = Shape.size();
				// The courses (art director, 24 Nov: never drawn lines, never shingles): each its own depth, a fifth more or less
				// than its neighbours; its butts' line wandering a few cm round the roof; the butts barely proud, dying away
				// toward the hips; a soft shadow under each butt on the course below, not a dark line.
				std::vector<double> Cum(static_cast<size_t>(NQ) + 1, 0.0), En(static_cast<size_t>(NQ) + 1, 0.0);
				for (int I = 0; I < NQ; ++I) Cum[static_cast<size_t>(I) + 1] = Cum[static_cast<size_t>(I)] + 1.0 + 0.4 * (Unit(Hash(P.S.Key ^ 0x57C0, static_cast<uint64_t>(I))) - 0.5);
				for (int I = 0; I <= NQ; ++I) En[static_cast<size_t>(I)] = (Eb - Er) * Cum[static_cast<size_t>(I)] / Cum[static_cast<size_t>(NQ)];
				// (each butts' line: its own E at each place round the roof; the slopes' top and the eaves' roll stay true)
				auto Ec = [&](int I, size_t K) { return En[static_cast<size_t>(I)] + (I > 0 && I < NQ ? 0.035 * Noise(Shape[K].U / 1.3, 1.7 * I, P.S.Key ^ 0x57C1) : 0.0); };
				// (how far a place on a ring at an inset is from the nearest hip, as a share of the butts' rise kept)
				auto HipOf = [&](const V3& Pt, double Inset) {
					const double Ha = P.Ex - Inset, Hb = P.Ey - Inset, Hr = std::clamp(std::min(P.Rc, Hb - 0.02), 0.0, P.Rc);
					const double Sa = std::max(0.0, Ha - Hr), Sb = std::max(0.0, Hb - Hr);
					return Smooth(0.0, 0.4, std::max(0.0, std::max(Sa - std::fabs(Pt.X), Sb - std::fabs(Pt.Y))));
				};
				auto Lift = [&](double U, double E, double Fr, double Hf) { return 0.01 * Noise(U / 0.4, E / 0.3, P.S.Key ^ 0x57B3) + Lip * Fr * Hf; };
				std::vector<std::vector<RingPt>> Q(static_cast<size_t>(NQ) + 1);
				for (int I = 0; I <= NQ; ++I) Q[static_cast<size_t>(I)] = RingAt(P, (Eb - En[static_cast<size_t>(I)]) * P.CosP, NL, NS, NC);
				// (a ring on butts' line I, Dn further down the slope, its straws Fr of a lip proud (by the place's own course
				// depth where Per), lit as the slope (or as a butt's face), its straw's colour scaled by Tone and by Shadow where
				// the butt above shades it)
				auto Ring = [&](int I, double Dn, double Fr, bool bPer, bool bButt, double Shadow, std::vector<uint32_t>& Out) {
					Out.clear();
					const std::vector<RingPt>& Qi = Q[static_cast<size_t>(I)];
					const double Si = (Eb - En[static_cast<size_t>(I)]) * P.CosP;
					for (size_t K = 0; K < N; ++K)
					{
						const RingPt& Rp = Qi[K];
						const double E = Ec(I, K) + Dn, Hf = HipOf(Rp.P, Si);
						const double Fk = bPer && I + 1 <= NQ ? Fr * Dn / std::max(0.05, Ec(I + 1, K) - Ec(I, K)) : Fr;
						const V3 Nn = SlopeN(P, Rp.H), Dw = Rp.H * P.CosP - Up * P.SinP;
						const V3 At_ = Rp.P + Rp.H * ((E - En[static_cast<size_t>(I)]) * P.CosP) + V3{ 0.0, 0.0, TopZ(P, (Eb - E) * P.CosP) } + Nn * Lift(Rp.U, E, Fk, Hf);
						const Col C = StrawAt(P, Rp.U, E, Rp.H, true);
						Out.push_back(B.Vert(At_, bButt ? Norm(Nn + Dw) : Nn, Scaled(C, bButt ? 0.93 : 1.0 - Shadow * Hf)));
					}
				};
				std::vector<uint32_t> Va, Vm, Vb, Vc, Vd;
				auto Strip = [&](const std::vector<uint32_t>& A, const std::vector<uint32_t>& Bv, V3 Toward) {
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						const V3 To = Toward.Z > 0.9 ? SlopeN(P, Norm(Shape[K].H + Shape[J].H)) : Norm(Shape[K].H + Shape[J].H) * P.CosP - Up * P.SinP;
						B.Tri(A[K], Bv[K], Bv[J], To);
						B.Tri(A[K], Bv[J], A[J], To);
					}
				};
				for (int I = 0; I < NQ; ++I)
				{
					const double F1 = I + 1 < NQ ? 1.0 : 0.0;
					// (the course's top, in the shadow of the butts above it, then out of it, then down to its own butts)
					if (I > 0)
					{
						Ring(I, 0.0, 0.0, false, false, 0.15, Va);
						Ring(I, Shade, F1, true, false, 0.0, Vm);
						Strip(Va, Vm, Up);
						Ring(I + 1, 0.0, F1, false, false, 0.0, Vb);
						Strip(Vm, Vb, Up);
					}
					else
					{
						Ring(I, 0.0, 0.0, false, false, 0.0, Va);
						Ring(I + 1, 0.0, F1, false, false, 0.0, Vb);
						Strip(Va, Vb, Up);
					}
					if (I + 1 < NQ)
					{
						Ring(I + 1, 0.0, 1.0, false, true, 0.0, Vc);
						Ring(I + 1, 0.0, 0.0, false, true, 0.0, Vd);
						Strip(Vc, Vd, V3{ 0.0, 0.0, -1.0 });
					}
					// (its straws: thin wisps lying down the slope, paler and darker)
					const std::vector<RingPt>&Q0 = Q[static_cast<size_t>(I)], &Q1 = Q[static_cast<size_t>(I) + 1];
					const double S0 = (Eb - En[static_cast<size_t>(I)]) * P.CosP;
					for (size_t K = 0; K < N; ++K)
					{
						Rng Rt(Hash(P.S.Key ^ 0x7F8, static_cast<uint64_t>(I) * 4096 + K));
						for (int W = 0; W < 2; ++W)
						{
							const double A = Rt.Next(), Fa = Rt.Range(0.0, 0.55), Ln = Rt.Range(0.3, 0.6), Wd = Rt.Range(0.006, 0.013), Pale = Rt.Range(0.0, 0.4), Tone = Rt.Range(0.8, 1.1);
							const size_t J = (K + 1) % N;
							const double Fb = std::min(0.97, Fa + Ln);
							const V3 H = Norm(Shape[K].H + (Shape[J].H - Shape[K].H) * A);
							const double E0 = Lerp(Ec(I, K), Ec(I, J), A), E1 = Lerp(Ec(I + 1, K), Ec(I + 1, J), A);
							const double Hf = HipOf(Q0[K].P + (Q0[J].P - Q0[K].P) * A, S0);
							auto Pt = [&](double Fq) {
								const V3 Pa = Q0[K].P + (Q0[J].P - Q0[K].P) * A, Pb = Q1[K].P + (Q1[J].P - Q1[K].P) * A;
								const double E = E0 + (E1 - E0) * Fq, Enm = En[static_cast<size_t>(I)] + (En[static_cast<size_t>(I) + 1] - En[static_cast<size_t>(I)]) * Fq;
								return Pa + (Pb - Pa) * Fq + H * ((E - Enm) * P.CosP) + V3{ 0.0, 0.0, TopZ(P, (Eb - E) * P.CosP) } + SlopeN(P, H) * (Lift(Shape[K].U, E, F1 * Fq, Hf) + 0.004);
							};
							const V3 Bt = Pt(Fa), Tt = Pt(Fb), Hm = Norm(Shape[K].H + Shape[J].H);
							const V3 Side = Norm(Cross(Up, Hm)) * Wd;
							B.Tri1(Bt - Side, Bt + Side, Tt, Scaled(Mixed(StrawAt(P, Shape[K].U, E0, Hm, true), Rgb(178, 160, 124), Pale), Tone), SlopeN(P, Hm));
						}
					}
				}
				// (the ridge: the slopes' top rolled round over it)
				{
					std::vector<uint32_t> Pv, Cv;
					for (int J = 0; J <= NR; ++J)
					{
						double S = 0.0, Rise = 0.0, Th = 0.0;
						RollAt(P, J, NR, S, Rise, Th);
						Cv.clear();
						for (const RingPt& Rp : RingAt(P, S, NL, NS, NC))
						{
							const V3 Nn = Rp.H * std::sin(Th) + Up * std::cos(Th);
							Cv.push_back(B.Vert(Rp.P + V3{ 0.0, 0.0, TopZ(P, P.Top) + Rise } + Nn * Lift(Rp.U, 0.0, 0.0, 0.0), Nn, Scaled(StrawAt(P, Rp.U, 0.0, Rp.H, true), 0.97 + 0.06 * J / NR)));
						}
						if (J > 0)
							for (size_t K = 0; K < N; ++K)
							{
								const size_t Jn = (K + 1) % N;
								B.Tri(Pv[K], Cv[K], Cv[Jn], Up);
								B.Tri(Pv[K], Cv[Jn], Pv[Jn], Up);
							}
						Pv = Cv;
					}
				}
				// (the eaves: the top rolled over into the cut, the cut straight down to a ragged foot, the underside dark back
				// to the walls; the cut face the straws' ends seen end-on, a dense stipple, darker toward the foot, a few wisps
				// hanging: art director, 24 Nov, never a smooth band)
				{
					const double Ae = 0.5 * Pi - P.Pitch;
					const double Se = Er * P.CosP; // (the inset where the roll begins)
					const std::vector<RingPt> R0 = RingAt(P, Se, NL, NS, NC), Rin = RingAt(P, P.Ov + 0.3, NL, NS, NC);
					const int NA = 3;
					std::vector<std::vector<V3>> Arc(static_cast<size_t>(NA) + 1);
					std::vector<V3> Foot;
					for (size_t K = 0; K < N; ++K)
					{
						const V3 Nn = SlopeN(P, R0[K].H), Dn = R0[K].H * P.CosP - Up * P.SinP;
						const V3 Tp = R0[K].P + V3{ 0.0, 0.0, TopZ(P, Se) } + Nn * Lift(R0[K].U, Eb - Er, 0.0, 0.0);
						const V3 C = Tp - Nn * Rl;
						for (int A = 0; A <= NA; ++A)
						{
							const double An = Ae * A / NA;
							Arc[static_cast<size_t>(A)].push_back(C + (Nn * std::cos(An) + Dn * std::sin(An)) * Rl);
						}
						const V3 E = Arc.back().back();
						const double Si = Se - (E - Tp).X * R0[K].H.X - (E - Tp).Y * R0[K].H.Y;
						Foot.push_back(V3{ E.X, E.Y, UnderZ(P, Si) - 0.03 * Noise01(R0[K].U / 0.16, 3.3, P.S.Key ^ 0x57C2) });
					}
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						const V3 Hm = Norm(R0[K].H + R0[J].H);
						const Col Sa = StrawAt(P, R0[K].U, Eb, R0[K].H, true), Sb = StrawAt(P, R0[J].U, Eb, R0[J].H, true);
						const Col Ca = Scaled(Mixed(Sa, Rgb(150, 136, 108), 0.3), 0.76 + 0.12 * Noise01(R0[K].U / 0.05, 1.0, P.S.Key ^ 0x57B6)), Cb = Scaled(Mixed(Sb, Rgb(150, 136, 108), 0.3), 0.76 + 0.12 * Noise01(R0[J].U / 0.05, 1.0, P.S.Key ^ 0x57B6));
						for (int A = 0; A < NA; ++A)
						{
							const double Am = Ae * (A + 0.5) / NA, Kf = (A + 1.0) / NA;
							B.Quad(Arc[static_cast<size_t>(A)][K], Arc[static_cast<size_t>(A)][J], Arc[static_cast<size_t>(A) + 1][J], Arc[static_cast<size_t>(A) + 1][K], Mixed(Sa, Ca, Kf - 1.0 / NA), Mixed(Sb, Cb, Kf - 1.0 / NA), Mixed(Sb, Cb, Kf), Mixed(Sa, Ca, Kf), SlopeN(P, Hm) * std::cos(Am) + (Hm * P.CosP - Up * P.SinP) * std::sin(Am));
						}
						const V3 Ma = (Arc.back()[K] + Foot[K]) * 0.5, Mb = (Arc.back()[J] + Foot[J]) * 0.5;
						B.Quad(Arc.back()[K], Arc.back()[J], Mb, Ma, Ca, Cb, Scaled(Cb, 0.84), Scaled(Ca, 0.84), Hm);
						B.Quad(Ma, Mb, Foot[J], Foot[K], Scaled(Ca, 0.84), Scaled(Cb, 0.84), Scaled(Cb, 0.66), Scaled(Ca, 0.66), Hm);
						B.Quad1(Foot[K], Foot[J], Rin[J].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rin[K].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rgb(56, 50, 42), Up * -1.0);
						// (the straws' cut ends: fine dark dots and short dashes, more of them low down, a few pale)
						Rng Rbt(Hash(P.S.Key ^ 0x57BE, static_cast<uint64_t>(K)));
						const V3 Tg = Norm(Arc.back()[J] - Arc.back()[K]);
						for (int M = 0; M < 24; ++M)
						{
							const double T = Rbt.Next(), Dw = 0.03 + 0.95 * std::sqrt(Rbt.Next()), Kind = Rbt.Next(), Tone = Rbt.Range(0.42, 0.62), Sz = Rbt.Range(0.003, 0.0065), Ln = Rbt.Range(0.012, 0.03);
							const V3 Top_ = Arc.back()[K] + (Arc.back()[J] - Arc.back()[K]) * T, Bot_ = Foot[K] + (Foot[J] - Foot[K]) * T;
							const V3 Dg = Norm(Bot_ - Top_), Pc = Top_ + (Bot_ - Top_) * Dw + Hm * 0.004;
							const Col Base = Mixed(Ca, Scaled(Ca, 0.66), Dw);
							const Col Ck = Kind < 0.22 ? Scaled(Mixed(Base, Rgb(176, 158, 120), 0.5), 1.05) : Scaled(Base, Tone);
							if (Kind < 0.62) B.Tri1(Pc + Tg * Sz, Pc - Tg * (0.5 * Sz) + Dg * (0.87 * Sz), Pc - Tg * (0.5 * Sz) - Dg * (0.87 * Sz), Ck, Hm);
							else B.Tri1(Pc - Tg * (0.45 * Sz), Pc + Tg * (0.45 * Sz), Pc + Dg * Ln, Ck, Hm);
						}
						// (its foot ragged: short tufts hanging below the cut, and now and then a longer wisp)
						for (int M = 0; M < 3; ++M)
						{
							const double T = Rbt.Next(), Wd = Rbt.Range(0.006, 0.018), Lr = Rbt.Next(), Sk = Rbt.Range(-0.012, 0.012), Tone = Rbt.Range(0.58, 0.74);
							const double Ln = 0.006 + 0.032 * Lr * Lr;
							const V3 Ft = Foot[K] + (Foot[J] - Foot[K]) * T + Hm * 0.002;
							B.Tri1(Ft - Tg * Wd, Ft + Tg * Wd, Ft + Tg * Sk - Up * Ln, Scaled(Ca, Tone), Hm);
						}
						if (Rbt.Next() < 0.45)
						{
							const double T = Rbt.Next(), Wd = Rbt.Range(0.003, 0.005), Ln = Rbt.Range(0.04, 0.08), Sk = Rbt.Range(-0.02, 0.02), Pale = Rbt.Next();
							const V3 Ft = Foot[K] + (Foot[J] - Foot[K]) * T + Hm * 0.003;
							B.Tri1(Ft - Tg * Wd, Ft + Tg * Wd, Ft + Tg * Sk - Up * Ln, Pale < 0.5 ? Scaled(Mixed(Ca, Rgb(176, 158, 120), 0.45), 1.05) : Scaled(Ca, 0.6), Hm);
						}
					}
				}
			}

			/** The rafters' ends under the eaves: round poles out from the walls' top to the cut, grey with age. */
			void Rafters(Pen& B, const Plan& P)
			{
				for (int F = 0; F < 4; ++F)
				{
					const Face Fc = FaceOf(P, F);
					const int N = std::max(2, static_cast<int>(2.0 * Fc.Half / 0.8));
					for (int K = 0; K <= N; ++K)
					{
						const double U = -Fc.Half + 0.1 + (2.0 * Fc.Half - 0.2) * K / N;
						const V3 A = Fc.C + Fc.T * U + Fc.N * -0.1 + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.1) - 0.06 };
						const V3 Bp = Fc.C + Fc.T * U + Fc.N * (P.Ov - 0.22) + V3{ 0.0, 0.0, UnderZ(P, 0.22) - 0.06 };
						B.Tube(A, Bp, 0.05, 0.045, 6, Varied(Rgb(104, 96, 84), 0.9 + 0.15 * Unit(Hash(P.S.Key ^ 0x2AF, static_cast<uint64_t>(F * 64 + K))), 0.0), true);
					}
				}
			}

			/** The chimney: whitewashed clay over its brick, sooted toward its top, two courses of bare brick for its cap. */
			void Chimney(Pen& B, const Plan& P, bool bFine)
			{
				const double H = 0.24, X = P.Chx, Y = P.Chy;
				const double Ins = std::min(P.Ey - std::fabs(Y) - H, P.Ex - std::fabs(X) - H);
				const double Z0 = UnderZ(P, std::max(0.0, Ins)) - 0.1, Zt = TopZ(P, P.Top) + P.Rho * (1.0 - std::cos(P.Pitch)) + 0.55;
				const int NZ = bFine ? 6 : 1;
				for (int F = 0; F < 4; ++F)
				{
					const double Th = 0.5 * Pi * F;
					const V3 Nn{ std::cos(Th), std::sin(Th), 0.0 }, Tt{ -std::sin(Th), std::cos(Th), 0.0 };
					const V3 Cn = V3{ X, Y, 0.0 } + Nn * H;
					for (int K = 0; K < NZ; ++K)
					{
						const double Za = Z0 + (Zt - 0.16 - Z0) * K / NZ, Zb = Z0 + (Zt - 0.16 - Z0) * (K + 1) / NZ;
						auto Cl = [&](double Z) { return Mixed(Scaled(P.Wash, 0.92), Rgb(52, 48, 44), 0.75 * Smooth(Zt - 0.7, Zt - 0.12, Z)); };
						B.Quad(Cn + Tt * -H + V3{ 0, 0, Za }, Cn + Tt * H + V3{ 0, 0, Za }, Cn + Tt * H + V3{ 0, 0, Zb }, Cn + Tt * -H + V3{ 0, 0, Zb }, Cl(Za), Cl(Za), Cl(Zb), Cl(Zb), Nn);
					}
				}
				const Col Bk = Mixed(P.Brick, Rgb(40, 36, 32), 0.45);
				B.Box(V3{ X, Y, Zt - 0.12 }, V3{ H + 0.03, 0.0, 0.0 }, V3{ 0.0, H + 0.03, 0.0 }, V3{ 0.0, 0.0, 0.04 }, Bk, 0);
				B.Box(V3{ X, Y, Zt - 0.04 }, V3{ H + 0.015, 0.0, 0.0 }, V3{ 0.0, H + 0.015, 0.0 }, V3{ 0.0, 0.0, 0.04 }, Scaled(Bk, 0.85), 32);
				B.Quad1(V3{ X - H - 0.015, Y - H - 0.015, Zt }, V3{ X + H + 0.015, Y - H - 0.015, Zt }, V3{ X + H + 0.015, Y + H + 0.015, Zt }, V3{ X - H - 0.015, Y + H + 0.015, Zt }, Rgb(40, 36, 32), Up);
				B.Quad1(V3{ X - 0.1, Y - 0.1, Zt + 0.002 }, V3{ X + 0.1, Y - 0.1, Zt + 0.002 }, V3{ X + 0.1, Y + 0.1, Zt + 0.002 }, V3{ X - 0.1, Y + 0.1, Zt + 0.002 }, Rgb(14, 12, 10), Up);
			}

			/** The snow on the roof (Spec's Snow): a blanket round the slopes and over the ridge, thinner toward the top and by
			 * the chimney, a hand past the eaves and curled over them. */
			void SnowCap(Pen& B, const Plan& P, int NL, int NS, int NC, int NQ)
			{
				if (P.Snow <= 0.0) return;
				const double D0 = 0.06 + 0.12 * P.Snow, Base = 0.03, Over = 0.06;
				const Col White = Rgb(236, 238, 242), Shade = Rgb(204, 210, 220);
				const double Eb = P.Top / P.CosP;
				auto Depth = [&](const RingPt& Rp, double E, V3 At_) {
					double D = D0 * (0.8 + 0.35 * Noise01(Rp.U / 1.4, E / 1.1, P.S.Key ^ 0x5A1F));
					D *= 0.55 + 0.45 * Smooth(0.0, 0.9, E);
					return D * (0.2 + 0.8 * Smooth(0.3, 1.1, std::hypot(At_.X - P.Chx, At_.Y - P.Chy)));
				};
				std::vector<std::vector<V3>> G;
				std::vector<std::vector<Col>> Cc;
				std::vector<std::vector<V3>> Ns;
				const std::vector<RingPt> Shape = RingAt(P, 0.0, NL, NS, NC);
				const size_t N = Shape.size();
				// (the rings: over the ridge's roll, down the slopes, and one past the eaves)
				const int NR = 2;
				for (int J = NR; J >= 0; --J)
				{
					double S = 0.0, Rise = 0.0, Th = 0.0;
					RollAt(P, J, NR, S, Rise, Th);
					std::vector<V3> Row, Nr_;
					std::vector<Col> Cr;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC))
					{
						const V3 Nn = Rp.H * std::sin(Th) + Up * std::cos(Th);
						const V3 Base_ = Rp.P + V3{ 0.0, 0.0, TopZ(P, P.Top) + Rise };
						Row.push_back(Base_ + Nn * (Base + 0.55 * Depth(Rp, 0.0, Base_)));
						Nr_.push_back(Nn);
						Cr.push_back(White);
					}
					G.push_back(Row);
					Ns.push_back(Nr_);
					Cc.push_back(Cr);
				}
				for (int I = 1; I <= NQ + 1; ++I)
				{
					const double E = I <= NQ ? Eb * I / NQ : Eb + Over / P.CosP;
					const double S = (Eb - E) * P.CosP;
					std::vector<V3> Row, Nr_;
					std::vector<Col> Cr;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC))
					{
						const V3 Nn = SlopeN(P, Rp.H);
						const V3 Base_ = Rp.P + V3{ 0.0, 0.0, TopZ(P, S) };
						const double D = I <= NQ ? Depth(Rp, E, Base_) : 0.85 * Depth(Rp, Eb, Base_);
						Row.push_back(Base_ + Nn * (Base + D));
						Nr_.push_back(Nn);
						Cr.push_back(Mixed(White, Shade, 0.4 * Noise01(Rp.U / 0.6, E / 0.5, P.S.Key ^ 0x5A20)));
					}
					G.push_back(Row);
					Ns.push_back(Nr_);
					Cc.push_back(Cr);
				}
				for (size_t I = 0; I + 1 < G.size(); ++I)
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						const uint32_t A0 = B.Vert(G[I][K], Ns[I][K], Cc[I][K]), A1 = B.Vert(G[I][J], Ns[I][J], Cc[I][J]), B1 = B.Vert(G[I + 1][J], Ns[I + 1][J], Cc[I + 1][J]), B0 = B.Vert(G[I + 1][K], Ns[I + 1][K], Cc[I + 1][K]);
						B.Tri(A0, A1, B1, Up);
						B.Tri(A0, B1, B0, Up);
					}
				// (the eaves: the lip curled over and back under to the thatch's edge)
				const std::vector<V3>& E = G.back();
				const std::vector<RingPt> Rim = RingAt(P, 0.04, NL, NS, NC);
				const int NCu = NL > 12 ? 4 : 2;
				for (size_t K = 0; K < N; ++K)
				{
					const size_t J = (K + 1) % N;
					const V3 Ha = Shape[K].H, Hb = Shape[J].H;
					const V3 Na = SlopeN(P, Ha), Nb = SlopeN(P, Hb), Da = Ha * P.CosP - Up * P.SinP, Db = Hb * P.CosP - Up * P.SinP;
					const double Ra = 0.5 * (D0 * 0.85 + 0.04), Rb = Ra;
					V3 Pa = E[K], Pb = E[J];
					for (int C = 1; C <= NCu; ++C)
					{
						const double An = Pi * C / NCu, Am = Pi * (C - 0.5) / NCu;
						const V3 Qa = E[K] - Na * Ra + (Na * std::cos(An) + Da * std::sin(An)) * Ra, Qb = E[J] - Nb * Rb + (Nb * std::cos(An) + Db * std::sin(An)) * Rb;
						B.Quad1(Pa, Pb, Qb, Qa, Mixed(White, Shade, 0.3 + 0.5 * static_cast<double>(C) / NCu), Norm(Ha + Hb) * std::sin(Am) + Up * std::cos(Am));
						Pa = Qa;
						Pb = Qb;
					}
					B.Quad1(Pa, Pb, Rim[J].P + V3{ 0.0, 0.0, TopZ(P, 0.04) - 0.02 }, Rim[K].P + V3{ 0.0, 0.0, TopZ(P, 0.04) - 0.02 }, Shade, Up * -1.0);
				}
			}

			// ---------------------------------------------------------------- the middle and far levels

			/** The middle level (from 90 m): the walls in cells of the wash's colour, the openings their glass and paint in
			 * their reveals, the shutters flat; the prizba; the roof in rings, its eaves' cut and underside; the chimney. */
			void Mid(Pen& B, const Plan& P)
			{
				const std::vector<Column> L = Loop(P, 0.9, 2, 0.0, true);
				const std::vector<double> Z = Rows(P, 0.7);
				const size_t NCl = L.size(), NR = Z.size();
				for (size_t C = 0; C < NCl; ++C)
				{
					const size_t D = (C + 1) % NCl;
					for (size_t R = 0; R + 1 < NR; ++R)
					{
						if (InGap(P, L[C], L[D], Z[R], Z[R + 1])) continue;
						auto V = [&](const Column& Cl, double Zr) { return Cl.B + V3{ 0.0, 0.0, Zr }; };
						B.Quad(V(L[C], Z[R]), V(L[D], Z[R]), V(L[D], Z[R + 1]), V(L[C], Z[R + 1]), WashCol(P, L[C], Z[R]), WashCol(P, L[D], Z[R]), WashCol(P, L[D], Z[R + 1]), WashCol(P, L[C], Z[R + 1]), Norm(L[C].H + L[D].H));
					}
				}
				for (const Gap& G : P.Gaps)
				{
					const Face Fc = FaceOf(P, G.Face);
					auto Pt = [&](double U, double Zr, double Dd) { return Fc.C + Fc.T * U + Fc.N * Dd + V3{ 0.0, 0.0, Zr }; };
					const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Dp = G.What == 1 ? -0.1 : -P.Df;
					// (the reveal, the glass or the door at its back, the frame's paint round it)
					B.Quad1(Pt(U0, G.Z1, 0.0), Pt(U1, G.Z1, 0.0), Pt(U1, G.Z1, Dp), Pt(U0, G.Z1, Dp), Scaled(P.Wash, 0.6), V3{ 0.0, 0.0, -1.0 });
					B.Quad1(Pt(U0, G.Z0, 0.0), Pt(U0, G.Z1, 0.0), Pt(U0, G.Z1, Dp), Pt(U0, G.Z0, Dp), Scaled(P.Wash, 0.76), Fc.T);
					B.Quad1(Pt(U1, G.Z0, 0.0), Pt(U1, G.Z1, 0.0), Pt(U1, G.Z1, Dp), Pt(U1, G.Z0, Dp), Scaled(P.Wash, 0.76), Fc.T * -1.0);
					if (G.What == 1)
					{
						B.Quad1(Pt(U0, G.Z0, Dp), Pt(U1, G.Z0, Dp), Pt(U1, G.Z1, Dp), Pt(U0, G.Z1, Dp), Scaled(P.Door, 0.88), Fc.N);
						const Col Fr = Mixed(P.Frame, P.Bare, 0.2);
						B.Quad1(Pt(U0 - 0.09, G.Z0, 0.04), Pt(U0, G.Z0, 0.04), Pt(U0, G.Z1 + 0.11, 0.04), Pt(U0 - 0.09, G.Z1 + 0.11, 0.04), Fr, Fc.N);
						B.Quad1(Pt(U1, G.Z0, 0.04), Pt(U1 + 0.09, G.Z0, 0.04), Pt(U1 + 0.09, G.Z1 + 0.11, 0.04), Pt(U1, G.Z1 + 0.11, 0.04), Fr, Fc.N);
						B.Quad1(Pt(U0, G.Z1, 0.04), Pt(U1, G.Z1, 0.04), Pt(U1, G.Z1 + 0.11, 0.04), Pt(U0, G.Z1 + 0.11, 0.04), Fr, Fc.N);
						B.Box(Pt(G.U, 0.11, P.Pb + 0.2), Fc.T * 0.55, Fc.N * 0.18, V3{ 0.0, 0.0, 0.11 }, Rgb(118, 110, 98), 16);
						continue;
					}
					B.Quad1(Pt(U0, G.Z0, 0.0), Pt(U1, G.Z0, 0.0), Pt(U1, G.Z0, Dp), Pt(U0, G.Z0, Dp), Scaled(P.Wash, 0.9), Up);
					const Col Fr = Mixed(P.Frame, P.Bare, 0.2);
					const Col Gl = G.bShut ? P.Glass : Mixed(Mixed(P.Glass, P.Sky, 0.4), Rgb(176, 172, 162), 0.3);
					B.Quad1(Pt(U0, G.Z0, Dp), Pt(U1, G.Z0, Dp), Pt(U1, G.Z1, Dp), Pt(U0, G.Z1, Dp), Mixed(Gl, Fr, 0.3), Fc.N);
					B.Quad1(Pt(G.U - 0.02, G.Z0, Dp + 0.01), Pt(G.U + 0.02, G.Z0, Dp + 0.01), Pt(G.U + 0.02, G.Z1, Dp + 0.01), Pt(G.U - 0.02, G.Z1, Dp + 0.01), Fr, Fc.N);
					if (P.bBand)
					{
						const Col Bc = Mixed(P.Frame, P.Wash, 0.12);
						const double Bd = 0.085;
						B.Quad1(Pt(U0 - Bd, G.Z1, 0.012), Pt(U1 + Bd, G.Z1, 0.012), Pt(U1 + Bd, G.Z1 + Bd, 0.012), Pt(U0 - Bd, G.Z1 + Bd, 0.012), Bc, Fc.N);
						B.Quad1(Pt(U0 - Bd, G.Z0 - Bd, 0.012), Pt(U1 + Bd, G.Z0 - Bd, 0.012), Pt(U1 + Bd, G.Z0, 0.012), Pt(U0 - Bd, G.Z0, 0.012), Bc, Fc.N);
						B.Quad1(Pt(U0 - Bd, G.Z0, 0.012), Pt(U0, G.Z0, 0.012), Pt(U0, G.Z1, 0.012), Pt(U0 - Bd, G.Z1, 0.012), Bc, Fc.N);
						B.Quad1(Pt(U1, G.Z0, 0.012), Pt(U1 + Bd, G.Z0, 0.012), Pt(U1 + Bd, G.Z1, 0.012), Pt(U1, G.Z1, 0.012), Bc, Fc.N);
					}
					B.Box(Pt(G.U, G.Z0 - 0.015, 0.02), Fc.T * (0.5 * G.W + 0.05), Fc.N * 0.05, V3{ 0.0, 0.0, 0.015 }, Mixed(Fr, P.Bare, 0.6), 16);
					if (P.bShutters && !G.bShut)
					{
						const Col Sc = Mixed(P.Frame, P.Bare, 0.3);
						for (int Side = -1; Side <= 1; Side += 2)
						{
							const double Ua = Side < 0 ? U0 - 0.5 * G.W - 0.03 : U1 + 0.02, Ub = Ua + 0.5 * G.W + 0.01;
							B.Quad1(Pt(Ua, G.Z0 - 0.04, 0.05), Pt(Ub, G.Z0 - 0.04, 0.05), Pt(Ub, G.Z1 + 0.04, 0.05), Pt(Ua, G.Z1 + 0.04, 0.05), Sc, Fc.N);
						}
					}
				}
				Prizba(B, P, 1.2, 2, false);
				// (the roof: its rings round the slopes, the ridge's roll, the eaves' cut and its underside)
				const int NL = 8, NS = 5, NC = 2, NQ = 5;
				const double Eb = P.Top / P.CosP;
				const std::vector<RingPt> Shape = RingAt(P, 0.0, NL, NS, NC);
				const size_t N = Shape.size();
				std::vector<std::vector<uint32_t>> Rg;
				for (int J = 2; J >= 0; --J)
				{
					double S = 0.0, Rise = 0.0, Th = 0.0;
					RollAt(P, J, 2, S, Rise, Th);
					std::vector<uint32_t> Row;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC))
					{
						const V3 Nn = Rp.H * std::sin(Th) + Up * std::cos(Th);
						Row.push_back(B.Vert(Rp.P + V3{ 0.0, 0.0, TopZ(P, P.Top) + Rise }, Nn, StrawAt(P, Rp.U, 0.0, Rp.H, false)));
					}
					Rg.push_back(Row);
				}
				for (int I = 1; I <= NQ; ++I)
				{
					const double E = (Eb - 0.1) * I / NQ, S = (Eb - E) * P.CosP;
					std::vector<uint32_t> Row;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC)) Row.push_back(B.Vert(Rp.P + V3{ 0.0, 0.0, TopZ(P, S) + 0.01 }, SlopeN(P, Rp.H), Scaled(StrawAt(P, Rp.U, E, Rp.H, false), 0.97)));
					Rg.push_back(Row);
				}
				for (size_t I = 0; I + 1 < Rg.size(); ++I)
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						B.Tri(Rg[I][K], Rg[I][J], Rg[I + 1][J], Up);
						B.Tri(Rg[I][K], Rg[I + 1][J], Rg[I + 1][K], Up);
					}
				{
					const double S0 = 0.1 * P.CosP;
					const std::vector<RingPt> R0 = RingAt(P, S0, NL, NS, NC), R1 = RingAt(P, 0.02, NL, NS, NC), Rin = RingAt(P, P.Ov + 0.3, NL, NS, NC);
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						const V3 Hm = Norm(R0[K].H + R0[J].H);
						const Col Ca = Scaled(Mixed(StrawAt(P, R0[K].U, Eb, R0[K].H, false), Rgb(150, 136, 108), 0.3), 0.8), Cb = Scaled(Mixed(StrawAt(P, R0[J].U, Eb, R0[J].H, false), Rgb(150, 136, 108), 0.3), 0.8);
						const V3 Ta = R0[K].P + V3{ 0.0, 0.0, TopZ(P, S0) + 0.01 }, Tb_ = R0[J].P + V3{ 0.0, 0.0, TopZ(P, S0) + 0.01 };
						const V3 Ka = R1[K].P + V3{ 0.0, 0.0, TopZ(P, S0) - 0.08 }, Kb = R1[J].P + V3{ 0.0, 0.0, TopZ(P, S0) - 0.08 };
						const V3 Fa = R1[K].P + V3{ 0.0, 0.0, UnderZ(P, 0.02) }, Fb = R1[J].P + V3{ 0.0, 0.0, UnderZ(P, 0.02) };
						B.Quad(Ta, Tb_, Kb, Ka, Scaled(Ca, 1.1), Scaled(Cb, 1.1), Ca, Cb, SlopeN(P, Hm) + Hm);
						B.Quad(Ka, Kb, Fb, Fa, Ca, Cb, Scaled(Cb, 0.85), Scaled(Ca, 0.85), Hm);
						B.Quad1(Fa, Fb, Rin[J].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rin[K].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rgb(56, 50, 42), Up * -1.0);
					}
				}
				Chimney(B, P, false);
				SnowCap(B, P, 8, 5, 2, 3);
			}

			/** The far level (from 300 m): the walls in their wash, each face's tone; the windows and door painted on; the
			 * prizba's band; the roof's slopes in two rings and its eaves' cut; the chimney. */
			void Far(Pen& B, const Plan& P)
			{
				const std::vector<Column> L = Loop(P, 100.0, 1, 0.0, false);
				const size_t NCl = L.size();
				for (size_t C = 0; C < NCl; ++C)
				{
					const size_t D = (C + 1) % NCl;
					// (each face's wash as the near level paints it, averaged)
					double R = 0.0, Gr = 0.0, Bl = 0.0;
					int Nw = 0;
					for (int I = 0; I < 8; ++I)
						for (int J = 0; J < 6; ++J)
						{
							Column Cm = L[C];
							Cm.Pm = L[C].Pm + (L[D].Pm > L[C].Pm ? (L[D].Pm - L[C].Pm) : 0.3) * (I + 0.5) / 8.0;
							Cm.U = L[C].U + (L[D].U - L[C].U) * (I + 0.5) / 8.0;
							const Col Cw = WashCol(P, Cm, P.Ph + (P.Zw - P.Ph) * (J + 0.5) / 6.0);
							R += Cw.R;
							Gr += Cw.G;
							Bl += Cw.B;
							++Nw;
						}
					const Col Cf = Rgb(R / Nw, Gr / Nw, Bl / Nw);
					B.Quad1(L[C].B + V3{ 0.0, 0.0, P.Ph - 0.06 }, L[D].B + V3{ 0.0, 0.0, P.Ph - 0.06 }, L[D].B + V3{ 0.0, 0.0, P.Zw }, L[C].B + V3{ 0.0, 0.0, P.Zw }, Cf, Norm(L[C].H + L[D].H));
				}
				{
					const std::vector<Column> Fo = Loop(P, 100.0, 1, P.Pb, false);
					for (size_t C = 0; C < Fo.size(); ++C)
					{
						const size_t D = (C + 1) % Fo.size();
						const Col Pc = Mixed(P.Plinth, P.Clay, 0.15);
						B.Quad1(Fo[C].B + V3{ 0.0, 0.0, -0.2 }, Fo[D].B + V3{ 0.0, 0.0, -0.2 }, Fo[D].B + V3{ 0.0, 0.0, P.Ph }, Fo[C].B + V3{ 0.0, 0.0, P.Ph }, Pc, Norm(Fo[C].H + Fo[D].H));
						B.Quad1(Fo[C].B + V3{ 0.0, 0.0, P.Ph }, Fo[D].B + V3{ 0.0, 0.0, P.Ph }, L[D].B + V3{ 0.0, 0.0, P.Ph }, L[C].B + V3{ 0.0, 0.0, P.Ph }, Mixed(Pc, Rgb(150, 140, 122), 0.3), Up);
					}
				}
				for (const Gap& G : P.Gaps)
				{
					const Face Fc = FaceOf(P, G.Face);
					auto Pt = [&](double U, double Zr) { return Fc.C + Fc.T * U + Fc.N * 0.015 + V3{ 0.0, 0.0, Zr }; };
					const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W;
					if (G.What == 1)
					{
						B.Quad1(Pt(U0, G.Z0), Pt(U1, G.Z0), Pt(U1, G.Z1), Pt(U0, G.Z1), Scaled(P.Door, 0.7), Fc.N);
						continue;
					}
					const Col Fr = Mixed(P.Frame, P.Bare, 0.2);
					const Col Gl = Mixed(Mixed(Mixed(P.Glass, P.Sky, 0.4), Rgb(176, 172, 162), 0.3), Fr, 0.3);
					const double Bd = P.bBand ? 0.085 : 0.0;
					if (P.bBand) B.Quad1(Pt(U0 - Bd, G.Z0 - Bd), Pt(U1 + Bd, G.Z0 - Bd), Pt(U1 + Bd, G.Z1 + Bd), Pt(U0 - Bd, G.Z1 + Bd), Mixed(P.Frame, P.Wash, 0.12), Fc.N);
					B.Quad1(Pt(U0, G.Z0) + Fc.N * 0.005, Pt(U1, G.Z0) + Fc.N * 0.005, Pt(U1, G.Z1) + Fc.N * 0.005, Pt(U0, G.Z1) + Fc.N * 0.005, Scaled(Gl, 0.8), Fc.N);
					if (P.bShutters && !G.bShut)
					{
						const Col Sc = Mixed(P.Frame, P.Bare, 0.3);
						B.Quad1(Pt(U0 - 0.5 * G.W - 0.03, G.Z0 - 0.04) + Fc.N * 0.01, Pt(U0 - 0.02, G.Z0 - 0.04) + Fc.N * 0.01, Pt(U0 - 0.02, G.Z1 + 0.04) + Fc.N * 0.01, Pt(U0 - 0.5 * G.W - 0.03, G.Z1 + 0.04) + Fc.N * 0.01, Sc, Fc.N);
						B.Quad1(Pt(U1 + 0.02, G.Z0 - 0.04) + Fc.N * 0.01, Pt(U1 + 0.5 * G.W + 0.03, G.Z0 - 0.04) + Fc.N * 0.01, Pt(U1 + 0.5 * G.W + 0.03, G.Z1 + 0.04) + Fc.N * 0.01, Pt(U1 + 0.02, G.Z1 + 0.04) + Fc.N * 0.01, Sc, Fc.N);
					}
				}
				// (the roof: the slopes in two bands to a rounded top, the eaves' cut and its underside)
				const int NL = 1, NS = 1, NC = 1;
				const double Eb = P.Top / P.CosP;
				const std::vector<RingPt> Shape = RingAt(P, 0.0, NL, NS, NC);
				const size_t N = Shape.size();
				auto Tone = [&](double E0, double E1) {
					double R = 0.0, Gr = 0.0, Bl = 0.0;
					int Nw = 0;
					for (int I = 0; I < 24; ++I)
						for (int J = 0; J < 4; ++J)
						{
							const double U = 30.0 * (I + 0.5) / 24.0, E = E0 + (E1 - E0) * (J + 0.5) / 4.0;
							const Col C = StrawAt(P, U, E, V3{ std::cos(2.0 * Pi * I / 24.0), std::sin(2.0 * Pi * I / 24.0), 0.0 }, true);
							R += C.R;
							Gr += C.G;
							Bl += C.B;
							++Nw;
						}
					return Mixed(Rgb(R / Nw, Gr / Nw, Bl / Nw), Rgb(230, 234, 240), 0.85 * P.Snow);
				};
				const double Es[4] = { 0.0, 0.0, 0.5 * Eb, Eb - 0.1 };
				std::vector<std::vector<V3>> Rg;
				std::vector<double> Ss;
				{
					double S = 0.0, Rise = 0.0, Th = 0.0;
					RollAt(P, 1, 1, S, Rise, Th);
					std::vector<V3> Row;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC)) Row.push_back(Rp.P + V3{ 0.0, 0.0, TopZ(P, P.Top) + Rise });
					Rg.push_back(Row);
				}
				for (int I = 1; I < 4; ++I)
				{
					const double S = (Eb - Es[I]) * P.CosP;
					std::vector<V3> Row;
					for (const RingPt& Rp : RingAt(P, S, NL, NS, NC)) Row.push_back(Rp.P + V3{ 0.0, 0.0, TopZ(P, S) });
					Rg.push_back(Row);
				}
				const Col Tones[3] = { Tone(0.0, 0.2), Tone(0.0, 0.5 * Eb), Tone(0.5 * Eb, Eb) };
				for (size_t I = 0; I + 1 < Rg.size(); ++I)
					for (size_t K = 0; K < N; ++K)
					{
						const size_t J = (K + 1) % N;
						const V3 Hm = Norm(Shape[K].H + Shape[J].H);
						const V3 Nn = I == 0 ? Up + Hm * 0.3 : SlopeN(P, Hm);
						if (Dot(Rg[I][J] - Rg[I][K], Rg[I][J] - Rg[I][K]) < 1e-12) B.Tri1(Rg[I][K], Rg[I + 1][J], Rg[I + 1][K], Tones[I], Nn);
						else B.Quad1(Rg[I][K], Rg[I][J], Rg[I + 1][J], Rg[I + 1][K], Tones[I], Nn);
					}
				const std::vector<RingPt> Rin = RingAt(P, P.Ov + 0.3, NL, NS, NC);
				for (size_t K = 0; K < N; ++K)
				{
					const size_t J = (K + 1) % N;
					const V3 Hm = Norm(Shape[K].H + Shape[J].H);
					const V3 Ta = Rg.back()[K], Tb_ = Rg.back()[J];
					const V3 Fa{ Ta.X, Ta.Y, UnderZ(P, (Eb - Es[3]) * P.CosP) }, Fb{ Tb_.X, Tb_.Y, UnderZ(P, (Eb - Es[3]) * P.CosP) };
					B.Quad1(Ta, Tb_, Fb, Fa, Scaled(Tones[2], 0.7), Hm);
					B.Quad1(Fa, Fb, Rin[J].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rin[K].P + V3{ 0.0, 0.0, UnderZ(P, P.Ov + 0.3) }, Rgb(56, 50, 42), Up * -1.0);
				}
				Chimney(B, P, false);
			}

			void Build(const Spec& S, Levels& Out)
			{
				Out = Levels();
				const Plan P = Make(S);
				{
					Pen B(Out.Near);
					Walls(B, P);
					Wear(B, P);
					Prizba(B, P, 0.25, 4, true);
					for (const Gap& G : P.Gaps)
					{
						if (G.What == 0) Window(B, P, G, Hash(P.S.Key ^ 0x3172, static_cast<uint64_t>(G.Face * 16) + static_cast<uint64_t>(std::llround(G.U * 10.0 + 100.0))));
						else Door(B, P, G, true);
					}
					Thatch(B, P);
					Rafters(B, P);
					Chimney(B, P, true);
					SnowCap(B, P, 24, 16, 4, 12);
				}
				{
					Pen B(Out.Mid);
					Mid(B, P);
				}
				{
					Pen B(Out.Far);
					Far(B, P);
				}
			}
		}


		// ====================================================================== the burnt-out house (Kind 9)

		namespace Brn
		{
			/** What the fire left of an izba (Roof 0: its log walls burnt down to their lowest courses) or a khata (Roof 1:
			 * its clay walls standing roofless to a broken top): the stove and its chimney standing over the ashes. */
			struct Plan
			{
				Spec S;
				double Hx = 3.75, Hy = 3.6;                                // half its footprint
				bool bClay = false;                                        // a khata's clay walls (else an izba's logs)
				double Snow = 0.0;
				double Sx = 0.0, Sy = 0.0, Sa = 0.78, Sb = 1.0, Sh = 1.7;  // the stove's middle, its half sizes, its top
				double Cx = 0.0, Cy = 0.0, Cw = 0.32, Cz0 = 2.4, Ct = 4.6; // the chimney's middle, half width, where its plaster ends, top
				double Lean = 0.0;                                         // the chimney's lean toward +x, metres a metre
				double My = 1.0;                                           // the stove's mouth faces +y (1) or -y (-1)
				double Wt = 0.15, Rl = 0.13;                               // the walls' line in from the footprint's edge; a log's radius
				Col Wash, Soot, Brick, Ash, Char, Clay, Mortar, Earth;
			};
			Plan Make(const Spec& S)
			{
				Plan P;
				P.S = S;
				P.bClay = S.Roof == 1;
				P.Hx = 0.5 * S.Long;
				P.Hy = 0.5 * S.Deep;
				P.Snow = Smooth(0.03, 0.5, S.Snow);
				Rng Rg(S.Key ^ 0xB0A7);
				const double Side = Rg.Next() < 0.5 ? 1.0 : -1.0, Back = Rg.Next() < 0.5 ? 1.0 : -1.0;
				P.Wt = P.bClay ? 0.22 : 0.15;
				P.Sa = 0.74 + 0.08 * Rg.Next();
				P.Sb = 0.95 + 0.12 * Rg.Next();
				P.Sh = 1.62 + 0.14 * Rg.Next();
				// (the stove in a back corner, a hand off the walls, its mouth into the room; the chimney over its mouth's end)
				P.Sx = Side * (P.Hx - P.Wt - 0.45 - P.Sa);
				P.Sy = Back * (P.Hy - P.Wt - 0.4 - P.Sb);
				P.My = -Back;
				P.Cw = 0.3 + 0.04 * Rg.Next();
				P.Cx = P.Sx;
				P.Cy = P.Sy + P.My * (P.Sb - P.Cw - 0.12);
				P.Cz0 = P.Sh + 0.75;
				P.Ct = 4.3 + 0.7 * Rg.Next();
				P.Lean = 0.025 * (Rg.Next() - 0.5);
				P.Rl = 0.12 + 0.02 * Rg.Next();
				const double UBrick = Rg.Next();
				P.Brick = UBrick < 0.6 ? Rgb(128, 66, 50) : Rgb(136, 82, 62);
				P.Wash = Rgb(196, 192, 182);
				P.Soot = Rgb(36, 33, 31);
				P.Ash = Rgb(120, 117, 110);
				P.Char = Rgb(30, 27, 25);
				P.Clay = Rgb(140, 115, 85);
				P.Mortar = Rgb(98, 92, 84);
				P.Earth = Rgb(86, 74, 60);
				return P;
			}

			/** The snow over what faces up, as the snow lies (a colour pass over a level, after it is built): on the ashes the
			 * wind has scoured it off in a few patches (bScour), where they show through dark; what stands over them keeps its
			 * cap. */
			void SnowOver(LandField::Mesh& M, const Plan& P, bool bScour)
			{
				if (P.Snow <= 0.0) return;
				for (size_t I = 0; I < M.Colours.size(); ++I)
				{
					const double Nz = M.Normals[I * 3 + 2], X = M.Positions[I * 3], Y = M.Positions[I * 3 + 1], Z = M.Positions[I * 3 + 2];
					double K = P.Snow * Smooth(0.42, 0.85, Nz) * (Z > 3.0 ? 0.6 : 1.0);
					if (bScour && Z < 0.45) K *= 1.0 - 0.9 * Smooth(0.62, 0.74, Noise01(X / 1.2, Y / 1.2, P.S.Key ^ 0xB0AD)) * Smooth(0.45, 0.3, Z);
					if (K <= 0.0) continue;
					LandField::Rgb8& C = M.Colours[I];
					C.R = static_cast<uint8_t>(std::lround(C.R + (240.0 - C.R) * K));
					C.G = static_cast<uint8_t>(std::lround(C.G + (242.0 - C.G) * K));
					C.B = static_cast<uint8_t>(std::lround(C.B + (246.0 - C.B) * K));
				}
			}

			// ---------------------------------------------------------------- the ashes

			/** The ashes' height over the floor at a place (x, y in its own frame): a low drift of ash, charcoal and fallen
			 * clay, heaped toward the stove where the ceiling's clay came down (and along the inside of a khata's walls, where
			 * its thatch and its ceiling fell), lumpy under snow. An izba's sinks under the yard toward the footprint's edge,
			 * so on any ground it lies in the yard and never on it as a slab; a khata's runs to its walls. */
			double AshAt(const Plan& P, double X, double Y)
			{
				const double In = std::max(0.0, std::min(P.Hx - std::fabs(X), P.Hy - std::fabs(Y)));
				const double Heap = 0.22 * std::exp(-(Sq(X - P.Sx) + Sq(Y - P.Sy)) / 2.6);
				double H = 0.07 + 0.11 * Noise01(X / 1.3, Y / 1.3, P.S.Key ^ 0xB0A1) + 0.03 * Noise(X / 0.3, Y / 0.3, P.S.Key ^ 0xB0A2) + Heap;
				if (P.bClay) H += 0.14 * Smooth(1.6, 0.5, In) * (0.35 + 0.65 * Noise01(X / 0.7, Y / 0.7, P.S.Key ^ 0xB0AB));
				H += 0.05 * P.Snow * Noise01(X / 0.5, Y / 0.5, P.S.Key ^ 0xB0AC);
				if (P.bClay) return 0.03 + H * Smooth(-0.05, 0.5, In);
				// (the line it comes up out of the yard along wanders: here half a metre in from the edge, there a metre)
				const double Rim = 0.85 + 0.4 * Noise(X / 1.1, Y / 1.1, P.S.Key ^ 0xB0AE);
				return H * Smooth(0.0, Rim + 0.25, In) - 0.3 * Smooth(Rim, 0.0, In);
			}
			Col AshCol(const Plan& P, double X, double Y, double Z, bool bFine = true)
			{
				// (a dark grey bed of ash and charcoal, lighter in soft drifts where the ash lies thick, flecked black with
				// charcoal, a little white ash, the ceiling's clay burnt red-brown in patches, more of it in a khata)
				Col C = Mixed(Rgb(54, 51, 48), Rgb(92, 89, 84), Smooth(0.25, 0.85, Noise01(X / 0.9, Y / 0.9, P.S.Key ^ 0xB0A3)));
				// (its flecks only where its points lie close enough to draw them: the middle level's broad colour alone)
				if (bFine)
				{
					C = Scaled(C, 0.92 + 0.12 * Noise01(X / 0.18, Y / 0.18, P.S.Key ^ 0xB0A6));
					C = Mixed(C, P.Char, 0.55 * Smooth(0.66, 0.78, Noise01(X / 0.12, Y / 0.12, P.S.Key ^ 0xB0A7)));               // (charcoal)
					C = Mixed(C, Rgb(132, 128, 121), 0.3 * Smooth(0.68, 0.82, Noise01(X / 0.35, Y / 0.35, P.S.Key ^ 0xB0A4)));   // (white ash)
				}
				else C = Mixed(C, Rgb(118, 114, 108), 0.18); // (the white ash the near level's flecks add, on average)
				C = Mixed(C, Rgb(104, 78, 58), (P.bClay ? 0.45 : 0.3) * Smooth(0.6, 0.78, Noise01(X / 0.6, Y / 0.6, P.S.Key ^ 0xB0A5))); // (burnt clay)
				// (trodden into the yard's earth where it thins to the ground)
				return Mixed(C, P.Earth, Smooth(0.07, -0.1, Z));
			}
			/** The ashes as a sheet, Cell apart: an izba's over its footprint, its rim under the yard; a khata's to the
			 * middle of its walls' thickness; its edge run down into the ground where the ground falls away under it. */
			void Ashes(Pen& B, const Plan& P, double Cell)
			{
				const double Ex = P.bClay ? P.Hx - P.Wt : P.Hx, Ey = P.bClay ? P.Hy - P.Wt : P.Hy, X0 = -Ex, Y0 = -Ey;
				const int NX = std::max(2, static_cast<int>(std::ceil(2.0 * Ex / Cell))), NY = std::max(2, static_cast<int>(std::ceil(2.0 * Ey / Cell)));
				const double Dx = 2.0 * Ex / NX, Dy = 2.0 * Ey / NY;
				std::vector<uint32_t> Id(static_cast<size_t>((NX + 1) * (NY + 1)));
				for (int J = 0; J <= NY; ++J)
					for (int I = 0; I <= NX; ++I)
					{
						const double X = X0 + Dx * I, Y = Y0 + Dy * J, Z = AshAt(P, X, Y);
						const double Gx = (AshAt(P, X + 0.05, Y) - AshAt(P, X - 0.05, Y)) / 0.1, Gy = (AshAt(P, X, Y + 0.05) - AshAt(P, X, Y - 0.05)) / 0.1;
						Id[static_cast<size_t>(J * (NX + 1) + I)] = B.Vert(V3{ X, Y, Z }, Norm(V3{ -Gx, -Gy, 1.0 }), AshCol(P, X, Y, Z, Cell < 0.5));
					}
				for (int J = 0; J < NY; ++J)
					for (int I = 0; I < NX; ++I)
					{
						const size_t A = static_cast<size_t>(J * (NX + 1) + I);
						B.Tri(Id[A], Id[A + 1], Id[A + static_cast<size_t>(NX) + 2], Up);
						B.Tri(Id[A], Id[A + static_cast<size_t>(NX) + 2], Id[A + static_cast<size_t>(NX) + 1], Up);
					}
				// (its edge down into the ground: one face a side)
				const double Bot = -std::max(0.3, P.S.Below), Zc = AshAt(P, X0, Y0);
				B.Quad1(V3{ X0, Y0, Zc }, V3{ -X0, Y0, Zc }, V3{ -X0, Y0, Bot }, V3{ X0, Y0, Bot }, P.Earth, V3{ 0.0, -1.0, 0.0 });
				B.Quad1(V3{ X0, -Y0, Zc }, V3{ -X0, -Y0, Zc }, V3{ -X0, -Y0, Bot }, V3{ X0, -Y0, Bot }, P.Earth, V3{ 0.0, 1.0, 0.0 });
				B.Quad1(V3{ X0, Y0, Zc }, V3{ X0, -Y0, Zc }, V3{ X0, -Y0, Bot }, V3{ X0, Y0, Bot }, P.Earth, V3{ -1.0, 0.0, 0.0 });
				B.Quad1(V3{ -X0, Y0, Zc }, V3{ -X0, -Y0, Zc }, V3{ -X0, -Y0, Bot }, V3{ -X0, Y0, Bot }, P.Earth, V3{ 1.0, 0.0, 0.0 });
			}

			// ---------------------------------------------------------------- the charred timber

			/** A charred timber from A to B, radius R. Its ends where the fire ate them (bEndA, bEndB): burnt down to a taper,
			 * off its middle, or broken off ragged and hollow; else sawn and capped. Fine (the near level): the char checked
			 * into blocks, each its own face a little proud over a black core, a dark crack between them, a block gone here
			 * and there; grey-white ash bloomed on the faces that look up; brown wood left on its underside by Brown (a
			 * sill log low by its footing stones). Else (the middle level) a plain tube in the same colours. */
			void Charred(Pen& B, const Plan& P, V3 A, V3 Bv, double R, bool bFine, bool bEndA, bool bEndB, uint64_t Seed, double Brown = 0.2)
			{
				const V3 Ax = Bv - A;
				const double L = std::sqrt(Dot(Ax, Ax));
				if (L < 0.05) return;
				const V3 Al = Ax * (1.0 / L);
				const V3 E1 = Norm(Cross(std::fabs(Al.Z) < 0.9 ? Up : V3{ 1.0, 0.0, 0.0 }, Al)), E2 = Cross(Al, E1);
				// (a thick timber's char in ten strips round it; a pole's in eight)
				const bool bThick = R >= 0.08;
				const int Sides = !bFine ? 5 : bThick ? 10 : 8;
				const int NS = std::max(2, static_cast<int>(std::ceil(L / (!bFine ? 0.7 : R >= 0.08 ? 0.095 : 0.11))));
				// (each burnt end a blunt, ragged break; some burnt down thinner over their last 25 to 45 cm first, to no less
				// than half their thickness: none to a point)
				const bool bTaperA = bEndA && Unit(Hash(Seed, 0xE0)) < 0.6, bTaperB = bEndB && Unit(Hash(Seed, 0xE1)) < 0.6;
				const double La = std::min(0.45 * L, 0.25 + 0.2 * Unit(Hash(Seed, 0xE2))), Lb = std::min(0.45 * L, 0.25 + 0.2 * Unit(Hash(Seed, 0xE3)));
				const double Sway = 2.0 * Pi * Unit(Hash(Seed, 0xE4));
				const double ThinA = 0.5 + 0.3 * Unit(Hash(Seed, 0xE5)), ThinB = 0.5 + 0.3 * Unit(Hash(Seed, 0xE6));
				auto Bite = [&](double Th) { return std::min(0.7 * L / NS, 0.13 * Noise01(Th / 0.9, 0.5, Seed ^ 0x54) + 0.05 * Noise01(Th / 0.25, 1.5, Seed ^ 0x55)); };
				// (the rings a little unevenly along it, so no two blocks run the same length)
				auto Ring = [&](int I) { return L * (I + (I == 0 || I == NS ? 0.0 : 0.62 * (Unit(Hash(Seed ^ 0x59, static_cast<uint64_t>(I))) - 0.5))) / NS; };
				// (where ring I lies along it at an angle round it: a broken end's ring set back by its bite)
				auto Along = [&](int I, double Th) {
					const double D = Ring(I);
					if (I == 0 && bEndA) return D + Bite(Th);
					if (I == NS && bEndB) return D - Bite(Th + 2.0);
					return D;
				};
				auto Thin = [&](double D) {
					double F = 0.94 + 0.08 * Noise(D / 0.35, 0.5, Seed);
					if (bEndA) F *= bTaperA ? ThinA + (1.0 - ThinA) * std::pow(Smooth(0.0, La, D), 0.8) : 0.8 + 0.2 * Smooth(0.0, 0.1, D);
					if (bEndB) F *= bTaperB ? ThinB + (1.0 - ThinB) * std::pow(Smooth(0.0, Lb, L - D), 0.8) : 0.8 + 0.2 * Smooth(0.0, 0.1, L - D);
					return F;
				};
				// (an end burnt thinner burns off its middle: the log's axis drawn a little aside as it thins)
				auto Mid = [&](double D) {
					const double Off = (bTaperA ? 1.0 - Smooth(0.0, La, D) : 0.0) + (bTaperB ? 1.0 - Smooth(0.0, Lb, L - D) : 0.0);
					return A + Al * D + (E1 * std::cos(Sway) + E2 * std::sin(Sway)) * (0.2 * R * Off);
				};
				// (its skin: black char; grey-white ash bloomed in patches on what looks up, a block here ashed over, the next
				// bare; brown wood left on its underside low down)
				auto Skin = [&](double D, double Th, double Z, double Tone, double Ashed) {
					const V3 Rd = E1 * std::cos(Th) + E2 * std::sin(Th);
					Col C = Scaled(Mixed(P.Char, Rgb(58, 56, 55), 0.3), Tone);
					const double Patch = Smooth(0.46, 0.7, Noise01(D / 0.5, Th / 1.3, Seed ^ 0x53));
					C = Mixed(C, Rgb(166, 162, 154), Smooth(0.2, 0.85, Rd.Z) * (0.05 + 0.75 * Patch * (0.35 + 0.65 * Ashed)));
					return Mixed(C, Rgb(104, 78, 54), Brown * Smooth(-0.1, -0.7, Rd.Z) * Smooth(0.6, 0.2, Z) * Smooth(0.3, 0.55, Noise01(D / 0.5, 0.5, Seed ^ 0x57)));
				};
				// (the core: black, well under the blocks, so the widest of them clears its edges and the cracks between them
				// are deep, and up to them at its ends; the whole of the middle level's timber, in the skin's colours)
				const int CoreSides = bFine ? 6 : Sides, CoreEvery = bFine ? 2 : 1;
				std::vector<uint32_t> Prev, Cur;
				for (int I = 0; I <= NS; I += (I + CoreEvery > NS && I < NS) ? NS - I : CoreEvery)
				{
					Cur.clear();
					for (int K = 0; K < CoreSides; ++K)
					{
						const double Th = 2.0 * Pi * K / CoreSides, D = Along(I, Th);
						const V3 Rd = E1 * std::cos(Th) + E2 * std::sin(Th);
						const V3 Pt = Mid(D) + Rd * (R * Thin(D) * (!bFine ? 1.0 : I == 0 || I == NS ? 0.965 : bThick ? 0.87 : 0.82));
						Cur.push_back(B.Vert(Pt, Rd, bFine ? Rgb(15, 14, 13) : Skin(D, Th, Pt.Z, 0.9 + 0.2 * Unit(Hash(Seed ^ 0x52, static_cast<uint64_t>(I * 16 + K))), 0.5)));
					}
					if (!Prev.empty())
						for (int K = 0; K < CoreSides; ++K)
						{
							const int Kn = (K + 1) % CoreSides;
							const double Th = 2.0 * Pi * (K + 0.5) / CoreSides;
							const V3 Out = E1 * std::cos(Th) + E2 * std::sin(Th);
							B.Tri(Prev[static_cast<size_t>(K)], Cur[static_cast<size_t>(K)], Cur[static_cast<size_t>(Kn)], Out);
							B.Tri(Prev[static_cast<size_t>(K)], Cur[static_cast<size_t>(Kn)], Prev[static_cast<size_t>(Kn)], Out);
						}
					// (the ends: a burnt one's face a little hollowed behind its ragged rim; a sawn end flat)
					if (I == 0 || I == NS)
					{
						const bool bBurnt = I == 0 ? bEndA : bEndB;
						const V3 Out = Al * (I == 0 ? -1.0 : 1.0);
						const double D = Ring(I);
						const uint32_t Tip = B.Vert(Mid(D) + Out * (bBurnt ? -0.03 - 0.04 * Unit(Hash(Seed, static_cast<uint64_t>(0xE7 + I))) : 0.0), Out, bBurnt ? Rgb(20, 18, 17) : Rgb(52, 44, 38));
						for (int K = 0; K < CoreSides; ++K) B.Tri(Tip, Cur[static_cast<size_t>(K)], Cur[static_cast<size_t>((K + 1) % CoreSides)], Out);
					}
					Prev = Cur;
					if (I == NS) break;
				}
				if (!bFine) return;
				// (the blocks: the char checked along the grain into strips of uneven widths, one to two, their edges wandering
				// along the log; each strip broken across into blocks of its own lengths, one to two again, so no crack across
				// runs on from a strip into the next and no two rows lie alike; the cracks across the grain the wider, the
				// deeper. Each block drawn in from its neighbours by its cracks' half widths and set at its own height over
				// the core.)
				const double Lm = bThick ? 0.105 : 0.14;
				std::vector<double> Edge(static_cast<size_t>(Sides) + 1, 0.0);
				for (int K = 0; K < Sides; ++K) Edge[static_cast<size_t>(K) + 1] = Edge[static_cast<size_t>(K)] + (bThick ? 0.7 : 0.75) + (bThick ? 0.6 : 0.5) * Unit(Hash(Seed ^ 0x5B, static_cast<uint64_t>(K)));
				const double Turn = Unit(Hash(Seed ^ 0x5A, 0));
				auto Bound = [&](int K, double D) {
					const int Km = K % Sides;
					return 2.0 * Pi * (K / Sides + Edge[static_cast<size_t>(Km)] / Edge[static_cast<size_t>(Sides)] + Turn + 0.12 * Noise(D / 0.45, 1.7 * Km, Seed ^ 0x5C) / Sides);
				};
				// (a burnt end's blocks stop at its ragged rim)
				auto From = [&](double Th) { return bEndA ? Along(0, Th) : 0.0; };
				auto Upto = [&](double Th) { return bEndB ? Along(NS, Th) : L; };
				for (int K = 0; K < Sides; ++K)
				{
					double Da = -Lm * Unit(Hash(Seed ^ 0x5D, static_cast<uint64_t>(K)));
					for (int J = 0; Da < L; ++J)
					{
						const uint64_t Kb = Hash(Seed ^ 0x58, static_cast<uint64_t>(K * 256 + J));
						const double D0 = Da, D1 = Da + Lm * (0.65 + 0.7 * Unit(Hash(Kb, 5)));
						Da = D1;
						if (Unit(Kb) < 0.07) continue; // (a block flaked off: the core shows)
						const double Gx = 0.003 + 0.003 * Unit(Hash(Kb, 4)), Gt = (0.0015 + 0.0015 * Unit(Hash(Kb, 6))) / R;
						const double Lift = 1.0 + 0.04 * Unit(Hash(Kb, 1)), Tone = 0.68 + 0.34 * Unit(Hash(Kb, 2)), Ashed = Unit(Hash(Kb, 3));
						// (its four corners: each end's two on the strip's edges there, kept within the log's ends)
						double Dc[4], Tc[4];
						for (int Q = 0; Q < 4; ++Q)
						{
							const bool bFar = Q >= 2, bLeft = Q == 0 || Q == 3;
							const double Dq = bFar ? D1 - Gx : D0 + Gx;
							const double Th = bLeft ? Bound(K, Dq) + Gt : Bound(K + 1, Dq) - Gt;
							Tc[Q] = Th;
							Dc[Q] = std::clamp(Dq, From(Th), std::max(From(Th), Upto(Th)));
						}
						if (std::min(Dc[3] - Dc[0], Dc[2] - Dc[1]) < 0.015 || std::min(Tc[1] - Tc[0], Tc[2] - Tc[3]) < 0.02) continue;
						uint32_t Ix[4];
						for (int Q = 0; Q < 4; ++Q)
						{
							const V3 Rd = E1 * std::cos(Tc[Q]) + E2 * std::sin(Tc[Q]);
							const V3 Pt = Mid(Dc[Q]) + Rd * (R * Thin(Dc[Q]) * Lift);
							Ix[Q] = B.Vert(Pt, Rd, Skin(Dc[Q], Tc[Q], Pt.Z, Tone, Ashed));
						}
						const double Tm = 0.25 * (Tc[0] + Tc[1] + Tc[2] + Tc[3]);
						const V3 Out = E1 * std::cos(Tm) + E2 * std::sin(Tm);
						B.Tri(Ix[0], Ix[1], Ix[2], Out);
						B.Tri(Ix[0], Ix[2], Ix[3], Out);
					}
				}
				// (a sawn end's face out to the blocks' edge, so its rim is no pipe's)
				for (int End = 0; End < 2; ++End)
				{
					if (End == 0 ? bEndA : bEndB) continue;
					const V3 Out = Al * (End == 0 ? -1.0 : 1.0);
					const double D = End == 0 ? 0.0 : L;
					const uint32_t Mid0 = B.Vert(Mid(D) + Out * 0.004, Out, Rgb(40, 34, 29));
					std::vector<uint32_t> Rim;
					for (int K = 0; K < Sides; ++K)
					{
						const double Th = 2.0 * Pi * K / Sides;
						Rim.push_back(B.Vert(Mid(D) + Out * 0.004 + (E1 * std::cos(Th) + E2 * std::sin(Th)) * (R * Thin(D) * 1.045), Out, Rgb(24, 21, 19)));
					}
					for (int K = 0; K < Sides; ++K) B.Tri(Mid0, Rim[static_cast<size_t>(K)], Rim[static_cast<size_t>((K + 1) % Sides)], Out);
				}
			}

			/** A field stone of the footing at (X, Y): a boulder set in the yard's ground, Rw round, its top Top over the
			 * floor (its foot well under it, for where the ground falls away). */
			void Stone(Pen& B, const Plan& P, double X, double Y, double Rw, double Top, uint64_t Seed)
			{
				const int NA = 7, NB = 4;
				// (a round crown some 0.26 m high over a foot that runs straight down under the yard)
				const double Zb = -0.4, Rv = 0.26, Zd = Top - Rv;
				std::vector<std::vector<uint32_t>> Rg(NB + 1);
				const Col C = Varied(Rgb(94, 90, 84), 0.85 + 0.25 * Unit(Hash(Seed, 1)), 0.03 * (Unit(Hash(Seed, 2)) - 0.5));
				for (int J = 0; J <= NB; ++J)
				{
					const double Bt = J == 0 ? 0.0 : 0.5 * Pi * (J - 1) / (NB - 0.4);
					for (int I = 0; I < NA; ++I)
					{
						const double Th = 2.0 * Pi * I / NA, Jt = 1.0 + 0.25 * (Unit(Hash(Seed, static_cast<uint64_t>(J * 16 + I + 4))) - 0.5);
						const V3 Rd{ std::cos(Th) * std::cos(Bt), std::sin(Th) * std::cos(Bt), std::sin(Bt) };
						const V3 Pt{ X + Rd.X * Rw * Jt, Y + Rd.Y * Rw * Jt, J == 0 ? Zb : Zd + Rd.Z * Rv * Jt };
						Rg[static_cast<size_t>(J)].push_back(B.Vert(Pt, Rd, Mixed(Scaled(C, 0.9 + 0.12 * Rd.Z), P.Soot, 0.25 + 0.35 * Smooth(0.3, 0.9, Rd.Z))));
					}
				}
				for (int J = 0; J < NB; ++J)
					for (int I = 0; I < NA; ++I)
					{
						const size_t A = static_cast<size_t>(I), Bn = static_cast<size_t>((I + 1) % NA);
						const double Th = 2.0 * Pi * (I + 0.5) / NA;
						const V3 Out{ std::cos(Th), std::sin(Th), 0.5 };
						B.Tri(Rg[static_cast<size_t>(J)][A], Rg[static_cast<size_t>(J)][Bn], Rg[static_cast<size_t>(J) + 1][Bn], Out);
						B.Tri(Rg[static_cast<size_t>(J)][A], Rg[static_cast<size_t>(J) + 1][Bn], Rg[static_cast<size_t>(J) + 1][A], Out);
					}
				const uint32_t Crown = B.Vert(V3{ X, Y, Top }, Up, Mixed(C, P.Soot, 0.6));
				for (int I = 0; I < NA; ++I) B.Tri(Rg[NB][static_cast<size_t>(I)], Rg[NB][static_cast<size_t>((I + 1) % NA)], Crown, Up);
			}

			/** The izba's log walls as the fire left them: the sill logs on their footing stones, burnt through in places,
			 * a course or two above them here and there, each log charred to its core at its broken ends; the corners'
			 * crossed ends. Fine: the near level (else the middle's, fewer rings and sides). */
			void Logs(Pen& B, const Plan& P, bool bFine)
			{
				const double Lx = P.Hx - P.Wt, Ly = P.Hy - P.Wt, R = P.Rl, Z0 = 0.26;
				// (the footing stones: at the corners and along each wall)
				if (bFine)
					for (int Sx = -1; Sx <= 1; ++Sx)
						for (int Sy = -1; Sy <= 1; ++Sy)
						{
							if (Sx == 0 && Sy == 0) continue;
							const uint64_t Sd = Hash(P.S.Key ^ 0xB057, static_cast<uint64_t>((Sx + 1) * 3 + Sy + 1));
							Stone(B, P, Sx * Lx, Sy * Ly, 0.27 + 0.06 * Unit(Sd), 0.24 + 0.04 * Unit(Hash(Sd, 9)), Sd);
						}
				for (int W = 0; W < 4; ++W)
				{
					const bool bAlongX = W < 2;
					const double Sg = W % 2 == 0 ? 1.0 : -1.0, Half = (bAlongX ? Lx : Ly) + 0.14;
					// (a log lies only where the one under it still does: none left hanging over a gap)
					std::vector<int> Under;
					for (int C = 0; C < 3; ++C)
					{
						const double Z = Z0 + R + (C + (bAlongX ? 0.0 : 0.5)) * 1.7 * R;
						// (where this log still lies: walked in 30 cm steps, the fire's reach growing up the wall)
						const int NSt = static_cast<int>(std::ceil(2.0 * Half / 0.3));
						std::vector<int> Kept(static_cast<size_t>(NSt), 0);
						for (int K = 0; K < NSt; ++K)
						{
							const double U = -Half + 2.0 * Half * (K + 0.5) / NSt;
							const double V = Noise01(U / 1.4 + 7.0 * W, 3.0 * C, P.S.Key ^ 0xB0C1) + 0.17 * C + (C == 0 ? -0.12 : 0.0);
							Kept[static_cast<size_t>(K)] = V < 0.56 && (C == 0 || Under[static_cast<size_t>(K)]) ? 1 : 0;
						}
						// (a stub too short to draw is no log, for this course or the one over it)
						for (int K = 0; K < NSt;)
						{
							int E = K;
							while (E < NSt && Kept[static_cast<size_t>(E)] == Kept[static_cast<size_t>(K)]) ++E;
							if (Kept[static_cast<size_t>(K)] && 2.0 * Half * (E - K) / NSt <= 0.35)
								for (int J = K; J < E; ++J) Kept[static_cast<size_t>(J)] = 0;
							K = E;
						}
						Under = Kept;
						int K = 0;
						while (K < NSt)
						{
							if (!Kept[static_cast<size_t>(K)])
							{
								++K;
								continue;
							}
							int E = K;
							while (E + 1 < NSt && Kept[static_cast<size_t>(E) + 1]) ++E;
							const double Ua = -Half + 2.0 * Half * K / NSt, Ub = -Half + 2.0 * Half * (E + 1) / NSt;
							if (Ub - Ua > 0.35)
							{
								auto At = [&](double U) { return bAlongX ? V3{ U, Sg * Ly, Z } : V3{ Sg * Lx, U, Z }; };
								const uint64_t Sd = Hash(P.S.Key ^ 0xB0C2, static_cast<uint64_t>(W * 64 + C * 16 + K));
								Charred(B, P, At(Ua), At(Ub), R, bFine, true, true, Sd, C == 0 ? 0.85 : 0.15);
							}
							K = E + 1;
						}
					}
				}
			}

			/** The fallen timbers inside: the ceiling's beams and the roof's poles come down charred across the ashes, one or
			 * two leaning on the stove or on a wall's stub. */
			void Fallen(Pen& B, const Plan& P, bool bFine)
			{
				Rng Rf(P.S.Key ^ 0xB0F1);
				// (fewer in a khata, whose roof's timbers ClayRoof leans on its walls)
				const int N = (P.bClay ? 3 : 6) + static_cast<int>((P.bClay ? 3.0 : 4.0) * Rf.Next());
				for (int K = 0; K < N; ++K)
				{
					const double X = Rf.Range(-P.Hx + 0.8, P.Hx - 0.8), Y = Rf.Range(-P.Hy + 0.8, P.Hy - 0.8), An = Rf.Range(0.0, Pi), Ln = Rf.Range(1.4, 3.6), R = Rf.Range(0.07, 0.11), Lift = Rf.Next();
					const double Ca = std::cos(An), Sa = std::sin(An);
					V3 A{ X - 0.5 * Ln * Ca, Y - 0.5 * Ln * Sa, 0.0 }, Bv{ X + 0.5 * Ln * Ca, Y + 0.5 * Ln * Sa, 0.0 };
					// (kept where the ashes lie over the yard: in from an izba's sunk rim, inside a khata's walls)
					const double Keep = P.bClay ? 0.65 : 1.0;
					A.X = std::clamp(A.X, -P.Hx + Keep, P.Hx - Keep);
					A.Y = std::clamp(A.Y, -P.Hy + Keep, P.Hy - Keep);
					Bv.X = std::clamp(Bv.X, -P.Hx + Keep, P.Hx - Keep);
					Bv.Y = std::clamp(Bv.Y, -P.Hy + Keep, P.Hy - Keep);
					A.Z = AshAt(P, A.X, A.Y) + 0.6 * R;
					Bv.Z = AshAt(P, Bv.X, Bv.Y) + 0.6 * R;
					// (one end up on the stove, where it falls near it, or propped on the heap)
					if (Lift < 0.3 && std::fabs(Bv.X - P.Sx) < P.Sa + 0.6 && std::fabs(Bv.Y - P.Sy) < P.Sb + 0.6) Bv.Z = P.Sh + R;
					else if (Lift < 0.45) Bv.Z += 0.35;
					Charred(B, P, A, Bv, R, bFine, true, true, Hash(P.S.Key ^ 0xB0F2, static_cast<uint64_t>(K)));
				}
			}

			// ---------------------------------------------------------------- the clay walls (a khata's)

			/** A khata's openings as its walls had them: the door in its front (the wall the stove's mouth looks to), a window
			 * either side of it, one in the back away from the stove, one in the gable the stove is not against; each its wall
			 * (0 +y, 1 -y, 2 +x, 3 -x), its middle along it, its half width, its sill and its head. */
			struct Opening
			{
				int W = 0;
				double U = 0.0, Hw = 0.4, Z0 = 0.8, Z1 = 1.6;
			};
			std::vector<Opening> Openings(const Plan& P)
			{
				std::vector<Opening> O;
				const int Front = P.My < 0.0 ? 1 : 0, Back = 1 - Front, Gable = P.Sx > 0.0 ? 3 : 2;
				const double Lx = P.Hx - P.Wt, Ly = P.Hy - P.Wt, Side = P.Sx > 0.0 ? 1.0 : -1.0;
				Rng Ro(P.S.Key ^ 0xB0D9);
				const double Du = Ro.Range(-0.12, 0.12) * Lx;
				const double Wu = Ro.Range(0.5, 0.6) * Lx;
				const double Bu = -Side * Ro.Range(0.3, 0.5) * Lx;
				const double Gu = Ro.Range(-0.15, 0.15) * Ly;
				O.push_back({ Front, Du, 0.45, -0.3, 1.86 });
				O.push_back({ Front, Du - Wu, 0.36, 0.82, 1.62 });
				O.push_back({ Front, Du + Wu, 0.36, 0.82, 1.62 });
				O.push_back({ Back, Bu, 0.36, 0.82, 1.62 });
				O.push_back({ Gable, Gu, 0.32, 0.86, 1.6 });
				return O;
			}
			/** The clay walls' broken top over a place along a wall (W its side, U along it): standing roofless mostly to
			 * near the eaves, crumbled in lumps along it, a breach or two down toward the knee where the roof's fall brought
			 * them down; higher at the corners, where the walls hold each other up. */
			double ClayTop(const Plan& P, int W, double U, double Ul, double Half)
			{
				const double Cn = Smooth(Half - 1.2, Half - 0.15, std::fabs(U));
				// (a breach's sides broken back a lump of clay at a time: half its fall with the place, half with the lump Ul
				// stands in, so its side runs down in a broken slope and not a stair)
				const double Uq = (std::floor(Ul / 0.42 + 0.29 * W) + 0.5) * 0.42;
				const double Bn = 0.65 * Noise01(U / 2.2 + 9.0 * W, 0.4, P.S.Key ^ 0xB0D1) + 0.35 * Noise01(Uq / 2.2 + 9.0 * W, 0.4, P.S.Key ^ 0xB0D1) + 0.07 * Noise(U / 0.3 + 2.0 * W, 1.7, P.S.Key ^ 0xB0DE);
				const double Breach = Smooth(0.58, 0.8, Bn) * (1.0 - Cn);
				const double Lump = std::floor(Ul / 0.32 + 0.37 * W), Um = (Lump + 0.5 - 0.37 * W) * 0.32;
				const uint64_t Kl = Hash(P.S.Key ^ 0xB0D2, static_cast<uint64_t>(Lump + 4096.0 + 512.0 * W));
				// (no wall's top left straight: a bite out of it here and there, a lump or three wide, and a lump alone fallen
				// from it; less of both at the corners)
				const double Bite = 0.3 * Smooth(0.6, 0.8, 0.5 * Noise01(U / 0.85 + 5.0 * W, 2.3, P.S.Key ^ 0xB0E3) + 0.5 * Noise01(Um / 0.85 + 5.0 * W, 2.3, P.S.Key ^ 0xB0E3));
				const double Gone = Unit(Hash(Kl, 1)) < 0.13 ? 0.07 + 0.13 * Unit(Hash(Kl, 2)) : 0.0;
				const double V = 2.02 + 0.16 * Noise(U / 1.1 + 4.0 * W, 0.9, P.S.Key ^ 0xB0DA) - 1.5 * Breach
					+ 0.13 * (Unit(Kl) - 0.5) - (Bite + Gone) * (1.0 - 0.7 * Cn) + 0.12 * Cn;
				return std::clamp(V, 0.3, 2.3);
			}
			/** A column of the clay wall's top as it broke, lump by lump: the lump of the wall round Uc (some 0.3 m of it,
			 * standing or fallen as one) has its top its own height and tilt, here at U along it; a small step from it to
			 * the next. */
			double LumpTop(const Plan& P, int W, double Uc, double U, double Half)
			{
				const double Lump = std::floor(Uc / 0.32 + 0.37 * W), Um = (Lump + 0.5 - 0.37 * W) * 0.32;
				const double Tilt = 0.1 * (Unit(Hash(P.S.Key ^ 0xB0E0, static_cast<uint64_t>(Lump + 4096.0 + 512.0 * W))) - 0.5);
				return std::clamp(ClayTop(P, W, U, Uc, Half) + Tilt * (U - Um) / 0.32, 0.25, 2.35);
			}
			/** The clay walls' line: a wall's half length (the long walls run through the corners), its thickness's half,
			 * its columns' width as its level draws them, and its top at a place along it. */
			struct ClayLine
			{
				double Half = 1.0, Th = 0.2;
				int NC = 2;
				double Ua(int K) const { return -Half + 2.0 * Half * K / NC; }
				double Mid(int K) const { return -Half + 2.0 * Half * (K + 0.5) / NC; }
				int At(double U) const { return std::clamp(static_cast<int>(std::floor((U + Half) / (2.0 * Half) * NC)), 0, NC - 1); }
			};
			ClayLine LineOf(const Plan& P, int W, bool bFine)
			{
				ClayLine Ln;
				Ln.Half = W < 2 ? P.Hx - P.Wt + Ln.Th : P.Hy - P.Wt - Ln.Th;
				Ln.NC = std::max(2, static_cast<int>(std::ceil(2.0 * Ln.Half / (bFine ? 0.18 : 0.5))));
				return Ln;
			}
			double WallTop(const Plan& P, int W, double U, bool bFine)
			{
				const ClayLine Ln = LineOf(P, W, bFine);
				return LumpTop(P, W, Ln.Mid(Ln.At(U)), U, Ln.Half);
			}
			/** The clay wall's colour. Outside (Face 0): its whitewash greyed by the smoke, the fire's soot high on it where
			 * the roof burnt over it, running down it in streaks, and in a plume up over each window and the door where the
			 * fire came out; the yard's earth splashed up its foot. Inside (1): its limewash black with the fire, most of
			 * all up the wall, fallen to the clay in patches. In its breaks and on its broken top (2): the clay's own,
			 * crumbled dry and pale, burnt dark in places. */
			Col ClayWall(const Plan& P, const std::vector<Opening>& Ops, int W, double U, double Z, int Face)
			{
				const uint64_t Kw = P.S.Key ^ static_cast<uint64_t>(W * 131);
				if (Face == 2) return Mixed(Scaled(Rgb(128, 112, 92), 0.82 + 0.22 * Noise01(U / 0.21, Z / 0.17, Kw ^ 0xB0D8)), P.Soot, 0.12 + 0.45 * Smooth(0.4, 0.8, Noise01(U / 0.4, Z / 0.3, Kw ^ 0xB0E1)));
				if (Face == 1)
				{
					const double Run = Noise01(U / 0.4 + 3.0 * W, Z / 2.6, P.S.Key ^ 0xB0D5);
					Col C = Scaled(Mixed(P.Wash, Rgb(140, 136, 128), 0.45), 0.8 + 0.12 * Noise01(U / 0.9, Z / 0.8, Kw ^ 0xB0D3));
					C = Mixed(C, P.Soot, Clamp01(0.42 + 0.5 * Smooth(0.3, 1.9, Z) + 0.25 * (Run - 0.5)));
					C = Mixed(C, Mixed(Scaled(P.Clay, 0.75), P.Soot, 0.35), 0.75 * Smooth(0.6, 0.66, Noise01(U / 0.55, Z / 0.45, Kw ^ 0xB0E2)));
					return Mixed(C, P.Ash, 0.35 * Smooth(0.5, 0.0, Z));
				}
				Col C = Scaled(Mixed(P.Wash, Rgb(150, 146, 138), 0.3), 0.93 + 0.07 * Noise01(U / 1.1, Z / 0.9, Kw ^ 0xB0D4));
				// (the soot from the eaves down: a band under the eaves' line and, here and there along it, a streak run down
				// from it, each its own width and reach and none wider apart than the wall's columns can draw, all faded out
				// by the wall's middle; under them the wash plain)
				const double Wide = 0.7 * Noise01(U / 0.75 + 3.0 * W, Z / 3.0, P.S.Key ^ 0xB0D5) + 0.3 * Noise01(U / 0.33 + 7.0 * W, Z / 3.0, P.S.Key ^ 0xB0E4);
				const double Streak = Smooth(0.52, 0.68, Wide);
				const double Reach = 0.4 + 0.75 * Streak * (0.45 + 0.55 * Noise01(U / 1.3 + 5.0 * W, 0.5, P.S.Key ^ 0xB0DF));
				double Sk = 0.05 + (0.5 + 0.35 * Streak) * Smooth(2.2 - Reach, 2.15, Z);
				// (the plumes: up from each opening's head, widening as they rise)
				for (const Opening& O : Ops)
				{
					if (O.W != W) continue;
					const double Rise = Z - O.Z1 + 0.3, Wd = O.Hw + 0.15 + 0.5 * std::max(0.0, Rise);
					Sk += 1.0 * Smooth(-0.05, 0.25, Rise) * Smooth(Wd, 0.5 * Wd, std::fabs(U - O.U)) * (0.75 + 0.25 * Noise01(U / 0.3, Z / 0.5, P.S.Key ^ 0xB0DB));
				}
				C = Mixed(C, P.Soot, Clamp01(Sk));
				C = Mixed(C, Mixed(P.Clay, P.Earth, 0.5), 0.5 * Smooth(0.32 + 0.1 * Noise(U / 0.35, 0.5, Kw ^ 0xB0D6), 0.02, Z));
				return Mixed(C, P.Earth, 0.5 * Smooth(0.2, -0.05, Z));
			}
			/** The khata's clay walls as the fire left them: each wall in columns along it and rows up it, a column standing
			 * to its lump's broken top (a step from one lump's to the next, the clay's section showing in it); its openings'
			 * cells left out with their reveals through the wall's thickness, ragged as the clay broke; the door's charred
			 * posts. */
			void ClayWalls(Pen& B, const Plan& P, bool bFine)
			{
				const double Lx = P.Hx - P.Wt, Ly = P.Hy - P.Wt, Rs = bFine ? 0.2 : 0.6;
				const std::vector<Opening> Ops = Openings(P);
				for (int W = 0; W < 4; ++W)
				{
					const bool bAlongX = W < 2;
					const ClayLine Ln = LineOf(P, W, bFine);
					const double Sg = W % 2 == 0 ? 1.0 : -1.0, Half = Ln.Half, Th = Ln.Th;
					const V3 T = bAlongX ? V3{ 1.0, 0.0, 0.0 } : V3{ 0.0, 1.0, 0.0 }, N = bAlongX ? V3{ 0.0, Sg, 0.0 } : V3{ Sg, 0.0, 0.0 };
					const V3 O = bAlongX ? V3{ 0.0, Sg * Ly, 0.0 } : V3{ Sg * Lx, 0.0, 0.0 };
					const int NC = Ln.NC;
					std::vector<double> Ta, Tb;
					double Most = 0.0;
					for (int K = 0; K < NC; ++K)
					{
						Ta.push_back(LumpTop(P, W, Ln.Mid(K), Ln.Ua(K), Half));
						Tb.push_back(LumpTop(P, W, Ln.Mid(K), Ln.Ua(K + 1), Half));
						Most = std::max({ Most, Ta.back(), Tb.back() });
					}
					const int NR = std::max(1, static_cast<int>(std::ceil((Most + 0.25) / Rs)));
					auto Zrow = [&](int R) { return -0.25 + Rs * R; };
					// (the wall's face swells in and out a little, hand-laid; not at its ends, so the corners close)
					auto Pt = [&](double U, double Z, double D) { return O + T * U + N * D + V3{ 0.0, 0.0, Z } + N * (0.012 * Smooth(Half, Half - 0.35, std::fabs(U)) * Noise(U / 0.5, Z / 0.5, P.S.Key ^ 0xB0D7)); };
					auto Wc = [&](double U, double Z, int Face) { return ClayWall(P, Ops, W, U, Z, Face); };
					// (a cell: the wall's (1), an opening's (2, its edges ragged as the clay broke), or over the column's top (0))
					std::vector<char> Cell(static_cast<size_t>(NC * NR), 0);
					for (int K = 0; K < NC; ++K)
						for (int R = 0; R < NR; ++R)
						{
							if (Zrow(R) >= std::max(Ta[static_cast<size_t>(K)], Tb[static_cast<size_t>(K)]) - 1e-6) continue;
							const double Um = Ln.Mid(K), Zm = Zrow(R) + 0.5 * Rs;
							char What = 1;
							for (const Opening& Op : Ops)
							{
								if (Op.W != W) continue;
								const double Rag = 0.05 * Noise(Zm / 0.25, Um / 0.25, P.S.Key ^ 0xB0DC);
								if (std::fabs(Um - Op.U) < Op.Hw + Rag && Zm > Op.Z0 + Rag && Zm < Op.Z1 + 1.5 * Rag) What = 2;
							}
							Cell[static_cast<size_t>(K * NR + R)] = What;
						}
					auto Is = [&](int K, int R) { return K >= 0 && K < NC && R >= 0 && R < NR ? Cell[static_cast<size_t>(K * NR + R)] : char(0); };
					for (int K = 0; K < NC; ++K)
					{
						const double Ua = Ln.Ua(K), Ub = Ln.Ua(K + 1), TopA = Ta[static_cast<size_t>(K)], TopB = Tb[static_cast<size_t>(K)];
						int Last = -1;
						for (int R = 0; R < NR; ++R)
						{
							const char What = Is(K, R);
							if (!What) continue;
							Last = R;
							const double Za0 = std::min(Zrow(R), TopA), Za1 = std::min(Zrow(R + 1), TopA), Zb0 = std::min(Zrow(R), TopB), Zb1 = std::min(Zrow(R + 1), TopB);
							if (What == 1)
							{
								for (int Side = 0; Side < 2; ++Side)
								{
									const double D = Side == 0 ? Th : -Th;
									B.Quad(Pt(Ua, Za0, D), Pt(Ub, Zb0, D), Pt(Ub, Zb1, D), Pt(Ua, Za1, D), Wc(Ua, Za0, Side), Wc(Ub, Zb0, Side), Wc(Ub, Zb1, Side), Wc(Ua, Za1, Side), N * (Side == 0 ? 1.0 : -1.0));
								}
								continue;
							}
							// (an opening's cell: the reveals where the wall stands beside, under or over it)
							auto Reveal = [&](double U0, double Z0, double U1, double Z1, V3 Face) {
								B.Quad(Pt(U0, Z0, Th), Pt(U1, Z1, Th), Pt(U1, Z1, -Th), Pt(U0, Z0, -Th), Wc(U0, Z0, 2), Wc(U1, Z1, 2), Wc(U1, Z1 + 0.3, 2), Wc(U0, Z0 + 0.3, 2), Face);
							};
							if (Is(K - 1, R) == 1) Reveal(Ua, Za0, Ua, std::min(Za1, Tb[static_cast<size_t>(K) - 1]), T);
							if (Is(K + 1, R) == 1) Reveal(Ub, Zb0, Ub, std::min(Zb1, Ta[static_cast<size_t>(K) + 1]), T * -1.0);
							if (Is(K, R - 1) == 1) Reveal(Ua, Za0, Ub, Zb0, Up);
							if (Is(K, R + 1) == 1) Reveal(Ua, Za1, Ub, Zb1, V3{ 0.0, 0.0, -1.0 });
						}
						// (its broken top: the clay's section, crumbled)
						if (Last >= 0 && Is(K, Last) == 1)
							B.Quad(Pt(Ua, TopA, Th), Pt(Ub, TopB, Th), Pt(Ub, TopB, -Th), Pt(Ua, TopA, -Th), Wc(Ua, TopA, 2), Wc(Ub, TopB, 2), Wc(Ub, TopB + 0.5, 2), Wc(Ua, TopA + 0.5, 2), Up);
						// (the step to the next column's top, where one lump stands over the other)
						if (K + 1 < NC)
						{
							const double Hn = Ta[static_cast<size_t>(K) + 1], Lo = std::min(TopB, Hn), Hi = std::max(TopB, Hn);
							const int Tall = TopB > Hn ? K : K + 1, Rm = std::clamp(static_cast<int>(std::floor((0.5 * (Lo + Hi) + 0.25) / Rs)), 0, NR - 1);
							if (Hi - Lo > 0.004 && Is(Tall, Rm) == 1)
								B.Quad(Pt(Ub, Lo, Th), Pt(Ub, Lo, -Th), Pt(Ub, Hi, -Th), Pt(Ub, Hi, Th), Wc(Ub, Lo, 2), Wc(Ub + 0.2, Lo, 2), Wc(Ub + 0.2, Hi, 2), Wc(Ub, Hi, 2), T * (TopB > Hn ? 1.0 : -1.0));
						}
					}
					// (the walls' ends at the corners, closed)
					for (int End = -1; End <= 1; End += 2)
					{
						const double U = End * Half, Tt = End < 0 ? Ta.front() : Tb.back();
						// (a long wall's end is the corner's outside, washed as the gable it meets, row by row as the wall is; a
						// gable's is hidden in the corner)
						const int Fe = bAlongX ? 0 : 2;
						for (int R = 0; R < NR && Zrow(R) < Tt - 1e-6; ++R)
						{
							const double Z0 = Zrow(R), Z1 = std::min(Zrow(R + 1), Tt);
							const Col Lo = Wc(U, Z0, Fe), Hi = Wc(U, Z1, Fe);
							B.Quad(Pt(U, Z0, Th), Pt(U, Z0, -Th), Pt(U, Z1, -Th), Pt(U, Z1, Th), Lo, Lo, Hi, Hi, T * static_cast<double>(End));
						}
					}
					// (its foot down into the ground, where the ground falls away under it)
					const double Bot = -std::max(0.25, P.S.Below);
					if (Bot < -0.26)
					{
						const Col Fc = Wc(0.0, -0.25, 0);
						B.Quad1(Pt(-Half, Bot, Th), Pt(Half, Bot, Th), Pt(Half, -0.25, Th), Pt(-Half, -0.25, Th), Fc, N);
						if (bAlongX)
							for (int End = -1; End <= 1; End += 2) B.Quad1(Pt(End * Half, Bot, Th), Pt(End * Half, Bot, -Th), Pt(End * Half, -0.25, -Th), Pt(End * Half, -0.25, Th), Fc, T * static_cast<double>(End));
					}
					// (the door's posts, charred, standing in its reveals)
					if (bFine)
						for (const Opening& Op : Ops)
							if (Op.W == W && Op.Z0 < 0.0)
								for (int Sd = -1; Sd <= 1; Sd += 2)
								{
									const double U = Op.U + Sd * (Op.Hw - 0.05);
									const uint64_t Sd_ = Hash(P.S.Key ^ 0xB0DD, static_cast<uint64_t>(Sd + 2));
									const double Hi = std::min(Op.Z1 + 0.05, 0.9 + 0.9 * Unit(Sd_));
									Charred(B, P, O + T * U + V3{ 0.0, 0.0, -0.1 }, O + T * U + V3{ 0.0, 0.0, Hi }, 0.065, true, false, true, Sd_);
								}
				}
			}
			/** The wash flaked off the clay low on the walls' outsides, where the wet reached it: crisp patches, the clay
			 * bared and shadowed toward its rim under the wash's broken edge, that edge a few mm proud. */
			void Flakes(Pen& B, const Plan& P)
			{
				const double Lx = P.Hx - P.Wt, Ly = P.Hy - P.Wt;
				const std::vector<Opening> Ops = Openings(P);
				Rng R(P.S.Key ^ 0xB0FA);
				const int NF = 10 + static_cast<int>(7.0 * R.Next());
				for (int K = 0; K < NF; ++K)
				{
					const double Pick = R.Next(), Uu = R.Next(), Low = R.Next(), Rx = R.Range(0.04, 0.13), Squat = R.Range(0.5, 1.1), Tone = R.Range(0.85, 1.05);
					const int W = Pick < 0.35 ? 0 : Pick < 0.7 ? 1 : Pick < 0.85 ? 2 : 3;
					const bool bAlongX = W < 2;
					const ClayLine Ln = LineOf(P, W, true);
					const double Sg = W % 2 == 0 ? 1.0 : -1.0, Rz = Rx * Squat, U = (2.0 * Uu - 1.0) * (Ln.Half - 0.25), Z = 0.14 + Rz + 1.0 * Low * Low * Low;
					const V3 T = bAlongX ? V3{ 1.0, 0.0, 0.0 } : V3{ 0.0, 1.0, 0.0 }, N = bAlongX ? V3{ 0.0, Sg, 0.0 } : V3{ Sg, 0.0, 0.0 };
					const V3 O = bAlongX ? V3{ 0.0, Sg * Ly, 0.0 } : V3{ Sg * Lx, 0.0, 0.0 };
					// (its rim's reaches drawn whether it is kept or not, so one flake's loss moves no other)
					const int NE = 10;
					std::vector<std::pair<double, double>> Reach;
					for (int E = 0; E < NE; ++E)
					{
						const double Rr = R.Range(0.7, 1.15), Ex = R.Range(0.006, 0.011);
						Reach.push_back({ Rr, Ex });
					}
					if (Z + Rz > std::min({ WallTop(P, W, U - Rx, true), WallTop(P, W, U, true), WallTop(P, W, U + Rx, true) }) - 0.12) continue;
					bool bClear = true;
					for (const Opening& Op : Ops)
						if (Op.W == W && std::fabs(U - Op.U) < Op.Hw + Rx + 0.07 && Z > Op.Z0 - Rz - 0.08 && Z < Op.Z1 + Rz + 0.07) bClear = false;
					if (!bClear) continue;
					auto Pt = [&](double Uq, double Zq, double Off) { return O + T * Uq + N * (Ln.Th + Off) + V3{ 0.0, 0.0, Zq } + N * (0.012 * Smooth(Ln.Half, Ln.Half - 0.35, std::fabs(Uq)) * Noise(Uq / 0.5, Zq / 0.5, P.S.Key ^ 0xB0D7)); };
					const Col Cl = Scaled(Mixed(P.Clay, P.Soot, 0.18), Tone);
					const uint32_t Cn = B.Vert(Pt(U, Z, 0.003), N, Cl);
					std::vector<uint32_t> Rim, Brk;
					for (int E = 0; E < NE; ++E)
					{
						const double An = 2.0 * Pi * E / NE, Cs = std::cos(An), Sn = std::sin(An);
						const auto [Rr, Ex] = Reach[static_cast<size_t>(E)];
						const double Ur = U + Rx * Rr * Cs, Zr = Z + Rz * Rr * Sn, Ue = U + (Rx * Rr + Ex) * Cs, Ze = Z + (Rz * Rr + Ex) * Sn;
						Rim.push_back(B.Vert(Pt(Ur, Zr, 0.003), N, Scaled(Cl, 0.78)));
						Brk.push_back(B.Vert(Pt(Ue, Ze, 0.006), N, Scaled(ClayWall(P, Ops, W, Ue, Ze, 0), 1.05)));
					}
					for (size_t E = 0; E < static_cast<size_t>(NE); ++E)
					{
						const size_t J = (E + 1) % static_cast<size_t>(NE);
						B.Tri(Cn, Rim[E], Rim[J], N);
						B.Tri(Rim[E], Brk[E], Brk[J], N);
						B.Tri(Rim[E], Brk[J], Rim[J], N);
					}
				}
			}
			/** What a khata's roof left: its thatch burnt to the ashes heaped inside the walls; a few of its rafter poles,
			 * charred, leaning in from the walls' tops where they still stand; a ceiling beam or two with one end still on a
			 * wall and the other come down across the room. Nothing lies where the stove and its chimney stand. */
			void ClayRoof(Pen& B, const Plan& P, bool bFine)
			{
				const double Lx = P.Hx - P.Wt, Ly = P.Hy - P.Wt, Th = 0.2;
				Rng R(P.S.Key ^ 0xB0FB);
				auto Clear = [&](V3 A, V3 Bv, double Rad) {
					for (int K = 0; K <= 10; ++K)
					{
						const V3 Q = A + (Bv - A) * (K / 10.0);
						if (std::fabs(Q.X - P.Sx) < P.Sa + Rad + 0.05 && std::fabs(Q.Y - P.Sy) < P.Sb + Rad + 0.05 && Q.Z < P.Sh + Rad + 0.03) return false;
						if (std::fabs(Q.X - P.Cx) < P.Cw + Rad + 0.06 && std::fabs(Q.Y - P.Cy) < P.Cw + Rad + 0.06) return false;
					}
					return true;
				};
				const int NP = 6 + static_cast<int>(3.0 * R.Next());
				for (int K = 0; K < NP + 2; ++K)
				{
					const bool bBeam = K >= NP;
					const double Pick = R.Next(), U = R.Range(-Lx + 0.5, Lx - 0.5), Skew = R.Range(-0.5, 0.5), Far = R.Next(), Over = R.Range(0.3, 0.7), Thick = R.Next();
					const double Side = Pick < 0.5 ? 1.0 : -1.0, In = bBeam ? 2.0 * Ly - 1.6 + 1.1 * Far : 1.1 + 1.2 * Far, Rad = bBeam ? 0.08 + 0.02 * Thick : 0.045 + 0.015 * Thick;
					const int W = Side > 0.0 ? 0 : 1;
					const double H = std::max({ WallTop(P, W, U - 0.12, true), WallTop(P, W, U, true), WallTop(P, W, U + 0.12, true) });
					if (H < 1.0) continue;
					// (it bears on the wall top's inner edge and runs on up over the wall; its foot in the ashes)
					const V3 On{ U, Side * (Ly - Th), H + Rad };
					V3 Foot{ std::clamp(U + Skew, -Lx + 0.4, Lx - 0.4), std::clamp(Side * (Ly - Th - In), -Ly + 0.5, Ly - 0.5), 0.0 };
					Foot.Z = AshAt(P, Foot.X, Foot.Y) + 0.5 * Rad;
					const V3 End = On + Norm(On - Foot) * Over;
					if (!Clear(Foot, End, Rad)) continue;
					Charred(B, P, Foot, End, Rad, bFine, true, true, Hash(P.S.Key ^ 0xB0FC, static_cast<uint64_t>(K)), 0.15);
				}
			}

			// ---------------------------------------------------------------- the stove and its chimney

			/** The stove's lime plaster at a place on a face (F: 0 +x, 1 +y, 2 -x, 3 -y, 4 its top; U along it, Z up): once
			 * whitewashed, grey-black from the fire, streaked, blackest in the plume over its mouth. */
			Col PlasterCol(const Plan& P, int F, double U, double Z)
			{
				const uint64_t Kf = P.S.Key ^ static_cast<uint64_t>(F * 7);
				Col C = Scaled(P.Wash, 0.86 + 0.14 * Noise01(U / 0.5, Z / 0.5, Kf ^ 0xB0E1));
				// (the smoke's yellow-brown stain over it in clouds; then the soot: in runs down it, in blots, heaviest at
				// its top where the room's fire rolled over it; its top black)
				C = Mixed(C, Rgb(150, 132, 108), 0.35 * Noise01(U / 0.9, Z / 0.7, Kf ^ 0xB0E8));
				const double Streak = Noise01(U / 0.1, Z / 1.2, Kf ^ 0xB0E2), Blot = Noise01(U / 0.35, Z / 0.3, Kf ^ 0xB0E9);
				const double K = 0.16 + 0.45 * Smooth(0.45, 0.85, Streak) * Smooth(0.2, 1.2, Z) + 0.45 * Smooth(0.9, P.Sh + 0.3, Z) + 0.3 * Smooth(0.55, 0.8, Blot) + (F == 4 ? 0.25 : 0.0) + 0.2 * Smooth(P.Sh + 0.05, P.Sh + 0.5, Z);
				return Mixed(C, P.Soot, Clamp01(K));
			}
			/** A face of the stove or the chimney's plastered part: its middle at the floor, along it, out from it, half its
			 * length. */
			struct SFace
			{
				V3 O, T, N;
				double Half;
			};
			SFace StoveFace(const Plan& P, int F)
			{
				const V3 C{ P.Sx, P.Sy, 0.0 };
				switch (F)
				{
				case 0: return { C + V3{ P.Sa, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, V3{ 1.0, 0.0, 0.0 }, P.Sb };
				case 1: return { C + V3{ 0.0, P.Sb, 0.0 }, V3{ -1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, P.Sa };
				case 2: return { C + V3{ -P.Sa, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, V3{ -1.0, 0.0, 0.0 }, P.Sb };
				default: return { C + V3{ 0.0, -P.Sb, 0.0 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, P.Sa };
				}
			}
			/** The stove's mouth: on its face toward My, its sill, its spring (where the arch begins) and its half width; the
			 * arch a half circle over it. Its fire-box under it (the podpechek), lower and smaller, square. */
			struct Mouths
			{
				double Z0 = 0.74, Z1 = 1.08, Hw = 0.26, Lz0 = 0.08, Lz1 = 0.36, Lw = 0.22;
			};
			/** One face of the stove: its plaster in cells Cell apart (cut round the mouth and the fire-box where it has
			 * them), a dark recess behind each opening, the plume of soot over the mouth. Fine: the plaster torn away from
			 * the bricks in a few patches, each a hole in it with a ragged outline, the plaster's broken edge standing a
			 * finger proud of the sooted bricks behind it. */
			void StoveSide(Pen& B, const Plan& P, int F, double Cell, bool bFine)
			{
				const SFace Fc = StoveFace(P, F);
				const Mouths Mo;
				const bool bMouth = (F == 1 && P.My > 0.0) || (F == 3 && P.My < 0.0);
				std::vector<double> Us, Zs;
				const int NU = std::max(1, static_cast<int>(std::ceil(2.0 * Fc.Half / Cell))), NZ = std::max(1, static_cast<int>(std::ceil((P.Sh + 0.1) / Cell)));
				for (int K = 0; K <= NU; ++K) Us.push_back(-Fc.Half + 2.0 * Fc.Half * K / NU);
				for (int K = 0; K <= NZ; ++K) Zs.push_back(-0.1 + (P.Sh + 0.1) * K / NZ);
				if (bMouth)
				{
					for (const double U : { -Mo.Hw, Mo.Hw, -Mo.Lw, Mo.Lw }) Us.push_back(U);
					for (const double Z : { Mo.Z0, Mo.Z1, Mo.Z1 + Mo.Hw, Mo.Lz0, Mo.Lz1 }) Zs.push_back(Z);
					std::sort(Us.begin(), Us.end());
					std::sort(Zs.begin(), Zs.end());
					auto Thin = [](std::vector<double>& V) {
						std::vector<double> K;
						for (const double X : V)
							if (K.empty() || X - K.back() > 0.02) K.push_back(X);
						V = K;
					};
					Thin(Us);
					Thin(Zs);
				}
				auto At = [&](double U, double Z) { return Fc.O + Fc.T * U + V3{ 0.0, 0.0, Z } + Fc.N * (0.008 * Noise(U / 0.3, Z / 0.3, P.S.Key ^ 0xB0E3)); };
				auto Colr = [&](double U, double Z) {
					Col C = PlasterCol(P, F, U, Z);
					if (bMouth)
					{
						// (the plume: up from the mouth's arch, widening, the plaster black)
						const double Up_ = Z - (Mo.Z1 + 0.5 * Mo.Hw), Wd = Mo.Hw + 0.12 + 0.55 * std::max(0.0, Up_);
						C = Mixed(C, Scaled(P.Soot, 0.7), 0.95 * Smooth(-0.45, 0.05, Up_) * Smooth(Wd, 0.5 * Wd, std::fabs(U)));
					}
					return C;
				};
				const size_t NUs = Us.size(), NZs = Zs.size();
				// (where the plaster has come away: more low down, one in three near a corner where it comes off first; each
				// a sheet's broken outline, a few corners with straight breaks between them, kept on the face and clear of its
				// openings and of each other; the grid's cells round it left out, to be filled to its outline below)
				struct Torn
				{
					double U = 0.0, Z = 0.0, Ru = 0.2, Rz = 0.2;
					std::vector<std::pair<double, double>> Rim; // (its outline, in order round its middle)
					size_t Ia = 0, Ib = 0, Ja = 0, Jb = 0;      // (the grid lines that box it)
					double U0 = 0.0, U1 = 0.0, Z0 = 0.0, Z1 = 0.0;
					uint64_t Seed = 0;
				};
				std::vector<Torn> Patches;
				std::vector<char> Gone((NUs - 1) * (NZs - 1), 0);
				if (bFine)
				{
					auto Tear = [&](double Uat, double Zat, double Ru, double Rz, uint64_t Seed) {
						Torn Tn;
						Tn.Seed = Seed;
						Tn.U = Uat;
						Tn.Z = Zat;
						Tn.Ru = Ru;
						Tn.Rz = Rz;
						const int NCn = 6 + static_cast<int>(Hash(Tn.Seed, 77) % 3);
						std::vector<std::pair<double, double>> Cn;
						for (int E = 0; E < NCn; ++E)
						{
							const uint64_t He = Hash(Tn.Seed, static_cast<uint64_t>(E));
							const double Th = 2.0 * Pi * (E + 0.6 * (Unit(Hash(He, 1)) - 0.5)) / NCn, Rr = 0.55 + 0.6 * Unit(Hash(He, 2));
							Cn.push_back({ Rr * std::cos(Th), Rr * std::sin(Th) });
						}
						std::vector<std::pair<double, std::pair<double, double>>> Round;
						auto Put = [&](double X, double Y) {
							const double Uq = std::clamp(Tn.U + Ru * X, -Fc.Half + 0.05, Fc.Half - 0.05), Zq = std::clamp(Tn.Z + Rz * Y, 0.06, P.Sh - 0.07);
							Round.push_back({ std::atan2((Zq - Tn.Z) / Rz, (Uq - Tn.U) / Ru), { Uq, Zq } });
						};
						for (int E = 0; E < NCn; ++E)
						{
							const auto [Xa, Ya] = Cn[static_cast<size_t>(E)];
							const auto [Xb, Yb] = Cn[static_cast<size_t>((E + 1) % NCn)];
							const double Jg = 1.0 + 0.16 * (Unit(Hash(Tn.Seed ^ 0xB3, static_cast<uint64_t>(E))) - 0.5), Sl = 0.35 + 0.3 * Unit(Hash(Tn.Seed ^ 0xB4, static_cast<uint64_t>(E)));
							Put(Xa, Ya);
							Put((Xa + (Xb - Xa) * Sl) * Jg, (Ya + (Yb - Ya) * Sl) * Jg);
						}
						std::sort(Round.begin(), Round.end());
						Tn.U0 = Tn.Z0 = 1e9;
						Tn.U1 = Tn.Z1 = -1e9;
						for (const auto& Q : Round)
						{
							if (!Tn.Rim.empty() && std::fabs(Q.second.first - Tn.Rim.back().first) + std::fabs(Q.second.second - Tn.Rim.back().second) < 0.008) continue;
							Tn.Rim.push_back(Q.second);
							Tn.U0 = std::min(Tn.U0, Q.second.first);
							Tn.U1 = std::max(Tn.U1, Q.second.first);
							Tn.Z0 = std::min(Tn.Z0, Q.second.second);
							Tn.Z1 = std::max(Tn.Z1, Q.second.second);
						}
						if (Tn.Rim.size() < 5 || Tn.U1 - Tn.U0 < 0.08 || Tn.Z1 - Tn.Z0 < 0.06) return false;
						// (the grid lines that box it, a finger clear of it)
						Tn.Ia = 0;
						Tn.Ib = NUs - 1;
						Tn.Ja = 0;
						Tn.Jb = NZs - 1;
						while (Tn.Ia + 1 < NUs && Us[Tn.Ia + 1] <= Tn.U0 - 0.012) ++Tn.Ia;
						while (Tn.Ib > 0 && Us[Tn.Ib - 1] >= Tn.U1 + 0.012) --Tn.Ib;
						while (Tn.Ja + 1 < NZs && Zs[Tn.Ja + 1] <= Tn.Z0 - 0.012) ++Tn.Ja;
						while (Tn.Jb > 0 && Zs[Tn.Jb - 1] >= Tn.Z1 + 0.012) --Tn.Jb;
						if (Tn.Ib <= Tn.Ia || Tn.Jb <= Tn.Ja) return false;
						if (bMouth && Us[Tn.Ia] < Mo.Hw + 0.02 && Us[Tn.Ib] > -Mo.Hw - 0.02 && Zs[Tn.Jb] > Mo.Z0 - 0.1) return false;
						if (bMouth && Us[Tn.Ia] < Mo.Lw + 0.03 && Us[Tn.Ib] > -Mo.Lw - 0.03 && Zs[Tn.Ja] < Mo.Lz1 + 0.03) return false;
						bool bFree = true;
						for (size_t I = Tn.Ia > 0 ? Tn.Ia - 1 : 0; I < std::min(NUs - 1, Tn.Ib + 1) && bFree; ++I)
							for (size_t J = Tn.Ja > 0 ? Tn.Ja - 1 : 0; J < std::min(NZs - 1, Tn.Jb + 1); ++J)
								if (Gone[I * (NZs - 1) + J])
								{
									bFree = false;
									break;
								}
						if (!bFree) return false;
						for (size_t I = Tn.Ia; I < Tn.Ib; ++I)
							for (size_t J = Tn.Ja; J < Tn.Jb; ++J) Gone[I * (NZs - 1) + J] = 1;
						Patches.push_back(Tn);
						return true;
					};
					// (more of them low down; one in three near a corner, where the plaster comes off first)
					Rng Rp(Hash(P.S.Key ^ 0xB0E4, static_cast<uint64_t>(F)));
					const int NP = 3 + static_cast<int>(4.0 * Rp.Next());
					for (int K = 0; K < NP; ++K)
					{
						const double Lo = Rp.Next(), Ur = Rp.Range(-Fc.Half + 0.2, Fc.Half - 0.2), Ru = Rp.Range(0.13, 0.32), Rz = Rp.Range(0.11, 0.26), Uc = Rp.Next();
						Tear(Uc < 0.33 ? (Ur < 0.0 ? -1.0 : 1.0) * (Fc.Half - std::max(0.4 * Ru, 0.12)) : Ur, 0.48 + (P.Sh - 0.8) * std::pow(Lo, 1.4), Ru, Rz, Hash(P.S.Key ^ 0xB0E5, static_cast<uint64_t>(F * 16 + K)));
					}
					// (and beside the mouth, on the side the oven fork does not lean, where the heat cracked it: a narrow one)
					if (bMouth)
					{
						const uint64_t Sm = Hash(P.S.Key ^ 0xB0E5, 0xA0);
						const double In = Mo.Hw + 0.26, Out = Fc.Half - 0.05;
						if (Out - In > 0.16) Tear((F == 1 ? 1.0 : -1.0) * 0.5 * (In + Out), 0.8 + 0.45 * Unit(Sm), 0.5 * (Out - In) / 1.15 + 0.02, 0.17 + 0.09 * Unit(Hash(Sm, 1)), Sm);
					}
				}
				std::vector<uint32_t> Id(NUs * NZs);
				for (size_t I = 0; I < NUs; ++I)
					for (size_t J = 0; J < NZs; ++J) Id[I * NZs + J] = B.Vert(At(Us[I], Zs[J]), Fc.N, Colr(Us[I], Zs[J]));
				for (size_t I = 0; I + 1 < NUs; ++I)
					for (size_t J = 0; J + 1 < NZs; ++J)
					{
						const double Um = 0.5 * (Us[I] + Us[I + 1]), Zm = 0.5 * (Zs[J] + Zs[J + 1]);
						if (bMouth && std::fabs(Um) < Mo.Hw && Zm > Mo.Z0 && Zm < Mo.Z1 + Mo.Hw) continue;
						if (bMouth && std::fabs(Um) < Mo.Lw && Zm > Mo.Lz0 && Zm < Mo.Lz1) continue;
						if (Gone[I * (NZs - 1) + J]) continue;
						B.Tri(Id[I * NZs + J], Id[(I + 1) * NZs + J], Id[(I + 1) * NZs + J + 1], Fc.N);
						B.Tri(Id[I * NZs + J], Id[(I + 1) * NZs + J + 1], Id[I * NZs + J + 1], Fc.N);
					}
				if (bMouth)
				{
					// (the arch's spandrels: the band over the mouth's spring filled round the half circle)
					const int NA = bFine ? 10 : 4;
					for (int Sd = -1; Sd <= 1; Sd += 2)
					{
						const V3 Cn = At(Sd * Mo.Hw, Mo.Z1 + Mo.Hw);
						const uint32_t Ic = B.Vert(Cn, Fc.N, Colr(Sd * Mo.Hw, Mo.Z1 + Mo.Hw));
						for (int K = 0; K < NA / 2; ++K)
						{
							const double A0 = Sd > 0 ? 0.5 * Pi * K / (NA / 2) : Pi - 0.5 * Pi * K / (NA / 2), A1 = Sd > 0 ? 0.5 * Pi * (K + 1) / (NA / 2) : Pi - 0.5 * Pi * (K + 1) / (NA / 2);
							const double Ua = Mo.Hw * std::cos(A0), Za = Mo.Z1 + Mo.Hw * std::sin(A0), Ub = Mo.Hw * std::cos(A1), Zb = Mo.Z1 + Mo.Hw * std::sin(A1);
							const uint32_t Ia = B.Vert(At(Ua, Za), Fc.N, Colr(Ua, Za)), Ib = B.Vert(At(Ub, Zb), Fc.N, Colr(Ub, Zb));
							B.Tri(Ic, Ia, Ib, Fc.N);
						}
					}
					// (the recess behind the mouth: its reveal round the arch and down its jambs, its hearth of ash, its back black)
					const double Dp = 0.5;
					const Col In = Rgb(26, 23, 21);
					auto Rv = [&](double U, double Z, double D) { return Fc.O + Fc.T * U + V3{ 0.0, 0.0, Z } - Fc.N * D; };
					std::vector<std::pair<double, double>> Edge;
					Edge.push_back({ Mo.Hw, Mo.Z0 });
					for (int K = 0; K <= NA; ++K)
					{
						const double A = Pi * K / NA;
						Edge.push_back({ Mo.Hw * std::cos(A), Mo.Z1 + Mo.Hw * std::sin(A) });
					}
					Edge.push_back({ -Mo.Hw, Mo.Z0 });
					for (size_t K = 0; K + 1 < Edge.size(); ++K)
					{
						const auto [Ua, Za] = Edge[K];
						const auto [Ub, Zb] = Edge[K + 1];
						const V3 Mid{ 0.0, 0.0, 0.0 };
						(void)Mid;
						const V3 Inw = Norm(Fc.T * (-(Ua + Ub) * 0.5) + V3{ 0.0, 0.0, Mo.Z1 - 0.5 * (Za + Zb) });
						B.Quad(Rv(Ua, Za, 0.0), Rv(Ub, Zb, 0.0), Rv(Ub, Zb, Dp), Rv(Ua, Za, Dp), Scaled(P.Soot, 0.75), Scaled(P.Soot, 0.75), In, In, Inw);
					}
					B.Quad(Rv(-Mo.Hw, Mo.Z0, 0.0), Rv(Mo.Hw, Mo.Z0, 0.0), Rv(Mo.Hw, Mo.Z0, Dp), Rv(-Mo.Hw, Mo.Z0, Dp), Rgb(104, 100, 94), Rgb(104, 100, 94), Rgb(70, 66, 62), Rgb(70, 66, 62), Up);
					const uint32_t Bc = B.Vert(Rv(0.0, Mo.Z1, Dp), Fc.N, In);
					for (size_t K = 0; K + 1 < Edge.size(); ++K) B.Tri(Bc, B.Vert(Rv(Edge[K].first, Edge[K].second, Dp), Fc.N, In), B.Vert(Rv(Edge[K + 1].first, Edge[K + 1].second, Dp), Fc.N, In), Fc.N);
					B.Tri(Bc, B.Vert(Rv(-Mo.Hw, Mo.Z0, Dp), Fc.N, In), B.Vert(Rv(Mo.Hw, Mo.Z0, Dp), Fc.N, In), Fc.N);
					// (the fire-box under it: square, shallow, black)
					const double Dl = 0.35;
					B.Quad1(Rv(-Mo.Lw, Mo.Lz0, Dl), Rv(Mo.Lw, Mo.Lz0, Dl), Rv(Mo.Lw, Mo.Lz1, Dl), Rv(-Mo.Lw, Mo.Lz1, Dl), In, Fc.N);
					B.Quad(Rv(-Mo.Lw, Mo.Lz1, 0.0), Rv(Mo.Lw, Mo.Lz1, 0.0), Rv(Mo.Lw, Mo.Lz1, Dl), Rv(-Mo.Lw, Mo.Lz1, Dl), Scaled(P.Soot, 0.8), Scaled(P.Soot, 0.8), In, In, V3{ 0.0, 0.0, -1.0 });
					B.Quad(Rv(-Mo.Lw, Mo.Lz0, 0.0), Rv(Mo.Lw, Mo.Lz0, 0.0), Rv(Mo.Lw, Mo.Lz0, Dl), Rv(-Mo.Lw, Mo.Lz0, Dl), P.Ash, P.Ash, In, In, Up);
					for (int Sd = -1; Sd <= 1; Sd += 2)
					{
						B.Quad(Rv(Sd * Mo.Lw, Mo.Lz0, 0.0), Rv(Sd * Mo.Lw, Mo.Lz1, 0.0), Rv(Sd * Mo.Lw, Mo.Lz1, Dl), Rv(Sd * Mo.Lw, Mo.Lz0, Dl), Scaled(P.Soot, 0.8), Scaled(P.Soot, 0.8), In, In, Fc.T * static_cast<double>(-Sd));
					}
					// (the shelf before the mouth, the pripechek: a ledge of the plaster, worn)
					const Col Ld = Mixed(P.Wash, P.Soot, 0.55);
					B.Box(Fc.O + Fc.N * 0.05 + V3{ 0.0, 0.0, Mo.Z0 - 0.04 }, Fc.T * (Mo.Hw + 0.12), Fc.N * 0.05, V3{ 0.0, 0.0, 0.04 }, Ld, 0);
				}
				// (each torn patch: the plaster from the grid's box in to its outline; its broken edge down to the bricks; the
				// mortar behind them and the bricks themselves, sooted, darker up under the plaster's edge)
				const double Dp = 0.016, Ch = 0.077, Bl = 0.262;
				for (const Torn& Tn : Patches)
				{
					auto Ang = [&](double U, double Z) { return std::atan2((Z - Tn.Z) / Tn.Rz, (U - Tn.U) / Tn.Ru); };
					std::vector<std::pair<double, std::pair<double, double>>> Box;
					for (size_t I = Tn.Ia; I < Tn.Ib; ++I)
					{
						Box.push_back({ Ang(Us[I], Zs[Tn.Ja]), { Us[I], Zs[Tn.Ja] } });
						Box.push_back({ Ang(Us[I + 1], Zs[Tn.Jb]), { Us[I + 1], Zs[Tn.Jb] } });
					}
					for (size_t J = Tn.Ja; J < Tn.Jb; ++J)
					{
						Box.push_back({ Ang(Us[Tn.Ib], Zs[J]), { Us[Tn.Ib], Zs[J] } });
						Box.push_back({ Ang(Us[Tn.Ia], Zs[J + 1]), { Us[Tn.Ia], Zs[J + 1] } });
					}
					std::sort(Box.begin(), Box.end());
					const size_t No = Box.size(), Ni = Tn.Rim.size();
					std::vector<uint32_t> Io, Ii;
					std::vector<double> Ao, Ai;
					for (const auto& Q : Box)
					{
						Io.push_back(B.Vert(At(Q.second.first, Q.second.second), Fc.N, Colr(Q.second.first, Q.second.second)));
						Ao.push_back(Q.first);
					}
					for (const auto& Q : Tn.Rim)
					{
						Ii.push_back(B.Vert(At(Q.first, Q.second), Fc.N, Colr(Q.first, Q.second)));
						Ai.push_back(Ang(Q.first, Q.second));
					}
					Ao.push_back(Ao.front() + 2.0 * Pi);
					Ai.push_back(Ai.front() + 2.0 * Pi);
					for (size_t I = 0, J = 0; I < No || J < Ni;)
					{
						if (J >= Ni || (I < No && Ao[I + 1] <= Ai[J + 1]))
						{
							B.Tri(Io[I], Io[(I + 1) % No], Ii[J % Ni], Fc.N);
							++I;
						}
						else
						{
							B.Tri(Ii[J], Ii[(J + 1) % Ni], Io[I % No], Fc.N);
							++J;
						}
					}
					// (the plaster's broken edge: its thickness, lime-grey at the face, dark down in the shadow by the bricks)
					for (size_t K = 0; K < Ni; ++K)
					{
						const auto [Ua, Za] = Tn.Rim[K];
						const auto [Ub, Zb] = Tn.Rim[(K + 1) % Ni];
						const V3 In = Norm(Fc.T * (Tn.U - 0.5 * (Ua + Ub)) + V3{ 0.0, 0.0, Tn.Z - 0.5 * (Za + Zb) });
						const Col Ca = Mixed(Scaled(Colr(Ua, Za), 0.8), P.Mortar, 0.35), Cb = Mixed(Scaled(Colr(Ub, Zb), 0.8), P.Mortar, 0.35), Dk = Mixed(P.Mortar, P.Soot, 0.7);
						B.Quad(At(Ua, Za), At(Ub, Zb), At(Ub, Zb) - Fc.N * Dp, At(Ua, Za) - Fc.N * Dp, Ca, Cb, Dk, Dk, In);
					}
					// (the mortar behind, then the bricks: courses 77 mm, each brick 262 mm, a half brick's step course to course)
					const double Ua = Tn.U0 - 0.01, Ub = Tn.U1 + 0.01, Za = Tn.Z0 - 0.01, Zb = Tn.Z1 + 0.01;
					const Col Mt = Mixed(P.Mortar, P.Soot, 0.7);
					B.Quad1(At(Ua, Za) - Fc.N * Dp, At(Ub, Za) - Fc.N * Dp, At(Ub, Zb) - Fc.N * Dp, At(Ua, Zb) - Fc.N * Dp, Mt, Fc.N);
					for (int Rr = static_cast<int>(std::floor(Za / Ch)); Rr <= static_cast<int>(std::floor(Zb / Ch)); ++Rr)
					{
						const double Sh = (Rr % 2 == 0) ? 0.0 : 0.5 * Bl;
						const double B0 = std::max(Za, Rr * Ch + 0.006), B1 = std::min(Zb, (Rr + 1) * Ch - 0.006);
						if (B1 - B0 < 0.012) continue;
						for (int Cc = static_cast<int>(std::floor((Ua - Sh) / Bl)); Cc <= static_cast<int>(std::floor((Ub - Sh) / Bl)); ++Cc)
						{
							const double A0 = std::max(Ua, Cc * Bl + Sh + 0.006), A1 = std::min(Ub, (Cc + 1) * Bl + Sh - 0.006);
							if (A1 - A0 < 0.012) continue;
							const uint64_t Kb = static_cast<uint64_t>((Rr + 64) * 128 + Cc + 64);
							const double Tone = 0.78 + 0.3 * Unit(Hash(Tn.Seed ^ 0xB1, Kb)), Sink = Dp - 0.004 - 0.003 * Unit(Hash(Tn.Seed ^ 0xB5, Kb));
							// (sooted: more under the patch's top, where the smoke rolled up behind the plaster's edge)
							const double Sk = 0.58 + 0.25 * Unit(Hash(Tn.Seed ^ 0xB2, Kb)) + 0.2 * Smooth(Tn.Z, Tn.Z1, B1);
							const Col Bc = Mixed(Mixed(Scaled(P.Brick, Tone), Rgb(92, 84, 78), 0.25), P.Soot, Clamp01(Sk));
							B.Quad(At(A0, B0) - Fc.N * Sink, At(A1, B0) - Fc.N * Sink, At(A1, B1) - Fc.N * Sink, At(A0, B1) - Fc.N * Sink, Bc, Bc, Scaled(Bc, 0.82), Scaled(Bc, 0.82), Fc.N);
						}
					}
				}
			}
			/** The stove's top, the sleeping ledge: plaster under ash and fallen clay. */
			void StoveTop(Pen& B, const Plan& P, double Cell)
			{
				const int NU = std::max(1, static_cast<int>(std::ceil(2.0 * P.Sa / Cell))), NV = std::max(1, static_cast<int>(std::ceil(2.0 * P.Sb / Cell)));
				std::vector<uint32_t> Id(static_cast<size_t>((NU + 1) * (NV + 1)));
				for (int J = 0; J <= NV; ++J)
					for (int I = 0; I <= NU; ++I)
					{
						const double X = P.Sx - P.Sa + 2.0 * P.Sa * I / NU, Y = P.Sy - P.Sb + 2.0 * P.Sb * J / NV;
						const double Z = P.Sh + 0.04 * Noise01(X / 0.3, Y / 0.3, P.S.Key ^ 0xB0E6) * (I > 0 && I < NU && J > 0 && J < NV ? 1.0 : 0.0);
						Id[static_cast<size_t>(J * (NU + 1) + I)] = B.Vert(V3{ X, Y, Z }, Up, Mixed(PlasterCol(P, 4, X, Y), AshCol(P, X, Y, 0.1), 0.5 * Noise01(X / 0.25, Y / 0.25, P.S.Key ^ 0xB0E7)));
					}
				for (int J = 0; J < NV; ++J)
					for (int I = 0; I < NU; ++I)
					{
						const size_t A = static_cast<size_t>(J * (NU + 1) + I);
						B.Tri(Id[A], Id[A + 1], Id[A + static_cast<size_t>(NU) + 2], Up);
						B.Tri(Id[A], Id[A + static_cast<size_t>(NU) + 2], Id[A + static_cast<size_t>(NU) + 1], Up);
					}
			}

			/** A place on the chimney's face F (0 +x, 1 +y, 2 -x, 3 -y) at U along it, Z up, Off out: leaning a little. */
			V3 ChimAt(const Plan& P, int F, double U, double Z, double Off)
			{
				const double W = P.Cw + Off;
				V3 Pt;
				switch (F)
				{
				case 0: Pt = V3{ P.Cx + W, P.Cy + U, Z }; break;
				case 1: Pt = V3{ P.Cx - U, P.Cy + W, Z }; break;
				case 2: Pt = V3{ P.Cx - W, P.Cy - U, Z }; break;
				default: Pt = V3{ P.Cx + U, P.Cy - W, Z }; break;
				}
				Pt.X += (Z - P.Sh) * P.Lean;
				return Pt;
			}
			V3 ChimN(int F) { return F == 0 ? V3{ 1.0, 0.0, 0.0 } : F == 1 ? V3{ 0.0, 1.0, 0.0 } : F == 2 ? V3{ -1.0, 0.0, 0.0 } : V3{ 0.0, -1.0, 0.0 }; }
			/** The chimney: plastered as the stove up to where the ceiling was, bare brick above it, course by course, each
			 * brick its own and proud of its joints, blacker toward the top; its cap a course stepped out, a few bricks gone
			 * from its top; its flue dark. Fine: brick by brick (else its faces painted in courses' bands). */
			void Chimney(Pen& B, const Plan& P, bool bFine)
			{
				const double Ch = 0.077, Bl = 0.262, Fw = 0.07;
				const int NC = static_cast<int>(std::floor((P.Ct - P.Cz0) / Ch)), NT = std::min(4, NC), NCore = NC - NT;
				const double Zc = P.Cz0 + NCore * Ch;
				// (a brick: its red dulled by the fire and sooted: black toward its top, where the smoke poured out; black
				// over the stretch that stood in the burning roof, most on the face to the room's middle; the soot run down
				// it from both in narrow streaks; the lime wash it had left on it in a few greyed patches low down)
				const int Fr = P.Cx > 0.0 ? 2 : 0;
				auto BrickCol = [&](int F, double U, double Z, uint64_t K) {
					const double Tn = 0.78 + 0.3 * Unit(Hash(P.S.Key ^ 0xB0B1, K));
					Col C = Mixed(Scaled(P.Brick, Tn), Rgb(92, 84, 78), 0.25);
					const double Up_ = Smooth(P.Ct - 1.9, P.Ct - 0.15, Z), Roof = (F == Fr ? 1.0 : 0.6) * Smooth(P.Cz0 + 1.7, P.Cz0 + 0.3, Z);
					const double Streak = Smooth(0.42, 0.8, Noise01(U / 0.07 + 5.0 * F, Z / 1.7, P.S.Key ^ 0xB0B2));
					C = Mixed(C, P.Soot, Clamp01(0.3 + 0.55 * Up_ + 0.38 * Roof + 0.4 * Streak * (0.45 + 0.55 * std::max(Up_, Roof) + (F == Fr ? 0.35 : 0.0)) + 0.15 * Unit(Hash(P.S.Key ^ 0xB0B3, K))));
					const double Lime = Noise01(U / 0.35 + 2.0 * F, Z / 0.5, P.S.Key ^ 0xB0B5);
					return Mixed(C, Mixed(P.Wash, P.Soot, 0.6), 0.32 * Smooth(0.72, 0.78, Lime) * (1.0 - Up_) * (1.0 - Roof));
				};
				for (int F = 0; F < 4; ++F)
				{
					const V3 N = ChimN(F);
					// (the plastered part, over the stove's top up to the old ceiling)
					const int NZp = bFine ? 8 : 2;
					for (int K = 0; K < NZp; ++K)
					{
						const double Za = P.Sh + (P.Cz0 - P.Sh) * K / NZp, Zb = P.Sh + (P.Cz0 - P.Sh) * (K + 1) / NZp;
						for (int I = 0; I < (bFine ? 4 : 1); ++I)
						{
							const double Ua = -P.Cw + 2.0 * P.Cw * I / (bFine ? 4 : 1), Ub = -P.Cw + 2.0 * P.Cw * (I + 1) / (bFine ? 4 : 1);
							B.Quad(ChimAt(P, F, Ua, Za, 0.015), ChimAt(P, F, Ub, Za, 0.015), ChimAt(P, F, Ub, Zb, 0.015), ChimAt(P, F, Ua, Zb, 0.015), PlasterCol(P, F, Ua + 3.0, Za), PlasterCol(P, F, Ub + 3.0, Za), PlasterCol(P, F, Ub + 3.0, Zb), PlasterCol(P, F, Ua + 3.0, Zb), N);
						}
					}
					// (the plaster's top edge, broken)
					B.Quad1(ChimAt(P, F, -P.Cw - 0.015, P.Cz0, 0.015), ChimAt(P, F, P.Cw + 0.015, P.Cz0, 0.015), ChimAt(P, F, P.Cw, P.Cz0, 0.0), ChimAt(P, F, -P.Cw, P.Cz0, 0.0), Mixed(P.Wash, P.Soot, 0.6), Up);
					// (the brick part: the joints behind, then the bricks; below its ragged top)
					if (!bFine)
					{
						const int NB = 6;
						for (int K = 0; K < NB; ++K)
						{
							const double Za = P.Cz0 + (P.Ct - P.Cz0) * K / NB, Zb = P.Cz0 + (P.Ct - P.Cz0) * (K + 1) / NB;
							const Col Ca = Mixed(BrickCol(F, 0.0, Za, static_cast<uint64_t>(F * 64 + K)), P.Mortar, 0.15), Cb = Mixed(BrickCol(F, 0.0, Zb, static_cast<uint64_t>(F * 64 + K + 1)), P.Mortar, 0.15);
							B.Quad(ChimAt(P, F, -P.Cw, Za, 0.003), ChimAt(P, F, P.Cw, Za, 0.003), ChimAt(P, F, P.Cw, Zb, 0.003), ChimAt(P, F, -P.Cw, Zb, 0.003), Ca, Ca, Cb, Cb, N);
						}
						continue;
					}
					// (the joints: sooted as the bricks are, blacker up it and where the roof burnt)
					for (int K = 0; K < 6; ++K)
					{
						const double Za = P.Cz0 + (Zc - P.Cz0) * K / 6.0, Zb = P.Cz0 + (Zc - P.Cz0) * (K + 1) / 6.0;
						auto Joint = [&](double Z) { return Mixed(P.Mortar, P.Soot, Clamp01(0.55 + 0.35 * Smooth(P.Ct - 1.9, P.Ct - 0.15, Z) + 0.3 * Smooth(P.Cz0 + 1.7, P.Cz0 + 0.3, Z))); };
						B.Quad(ChimAt(P, F, -P.Cw, Za, 0.0), ChimAt(P, F, P.Cw, Za, 0.0), ChimAt(P, F, P.Cw, Zb, 0.0), ChimAt(P, F, -P.Cw, Zb, 0.0), Joint(Za), Joint(Za), Joint(Zb), Joint(Zb), N);
					}
					for (int Rr = 0; Rr < NCore; ++Rr)
					{
						const double Zb = P.Cz0 + Rr * Ch, Sh = ((Rr + F) % 2 == 0) ? 0.0 : 0.5 * Bl;
						for (double Ub = -P.Cw - Sh; Ub < P.Cw - 0.02; Ub += Bl)
						{
							const double Ua = std::max(-P.Cw, Ub), Ue = std::min(P.Cw, Ub + Bl);
							if (Ue - Ua < 0.04) continue;
							const uint64_t Kb = static_cast<uint64_t>(F * 4096 + Rr * 32) + static_cast<uint64_t>(std::llround((Ub + 1.0) * 10.0));
							const Col Bc = BrickCol(F, 0.5 * (Ua + Ue), Zb, Kb);
							const double A0 = Ua + (Ua > -P.Cw + 1e-6 ? 0.006 : 0.0), A1 = Ue - (Ue < P.Cw - 1e-6 ? 0.006 : 0.0);
							const V3 P0 = ChimAt(P, F, A0, Zb + 0.006, 0.006), P1 = ChimAt(P, F, A1, Zb + 0.006, 0.006), P2 = ChimAt(P, F, A1, Zb + Ch - 0.006, 0.006), P3 = ChimAt(P, F, A0, Zb + Ch - 0.006, 0.006);
							B.Quad1(P0, P1, P2, P3, Bc, N);
							B.Quad1(P3, P2, ChimAt(P, F, A1, Zb + Ch - 0.006, 0.0), ChimAt(P, F, A0, Zb + Ch - 0.006, 0.0), Scaled(Bc, 1.12), Up);
						}
					}
				}
				const Col Tc = Mixed(P.Mortar, P.Soot, 0.85);
				const double Zt = bFine ? Zc : P.Ct;
				const V3 C0{ P.Cx + (Zt - P.Sh) * P.Lean, P.Cy, Zt };
				const V3 T0 = ChimAt(P, 3, -P.Cw, Zt, 0.0), T1 = ChimAt(P, 3, P.Cw, Zt, 0.0), T2 = ChimAt(P, 1, -P.Cw, Zt, 0.0), T3 = ChimAt(P, 1, P.Cw, Zt, 0.0);
				const V3 F0 = C0 + V3{ -Fw, -Fw, 0.0 }, F1 = C0 + V3{ Fw, -Fw, 0.0 }, F2 = C0 + V3{ Fw, Fw, 0.0 }, F3 = C0 + V3{ -Fw, Fw, 0.0 };
				B.Quad1(T0, T1, F1, F0, Tc, Up);
				B.Quad1(T1, T2, F2, F1, Tc, Up);
				B.Quad1(T2, T3, F3, F2, Tc, Up);
				B.Quad1(T3, T0, F0, F3, Tc, Up);
				const Col Flue = Rgb(14, 12, 11);
				if (!bFine)
				{
					B.Quad1(F0, F1, F2, F3, Flue, Up);
					return;
				}
				// (the flue: a dark shaft down into it)
				const V3 Dn{ 0.0, 0.0, -0.6 };
				B.Quad1(F0, F1, F1 + Dn, F0 + Dn, Flue, V3{ 0.0, 1.0, 0.0 });
				B.Quad1(F1, F2, F2 + Dn, F1 + Dn, Flue, V3{ -1.0, 0.0, 0.0 });
				B.Quad1(F2, F3, F3 + Dn, F2 + Dn, Flue, V3{ 0.0, -1.0, 0.0 });
				B.Quad1(F3, F0, F0 + Dn, F3 + Dn, Flue, V3{ 1.0, 0.0, 0.0 });
				B.Quad1(F0 + Dn, F1 + Dn, F2 + Dn, F3 + Dn, Flue, Up);
				// (its ragged top: the last courses brick by brick, each wall's bricks running through to the flue and each
				// course's corners its other pair of walls', gone where the fire and the frost loosened them, more of them the
				// higher, never one left over a gap)
				const double Tk = P.Cw - Fw;
				for (int J = 0; J < NT; ++J)
				{
					const int Rr = NCore + J;
					const double Zb = P.Cz0 + Rr * Ch, Lz = (Zb + 0.5 * Ch - P.Sh) * P.Lean;
					for (int F = 0; F < 4; ++F)
					{
						const double Span = (F % 2) == (Rr % 2) ? P.Cw : Fw, Sh = ((Rr + F) % 2 == 0) ? 0.0 : 0.5 * Bl;
						const V3 N = ChimN(F), Tg = F == 0 ? V3{ 0.0, 1.0, 0.0 } : F == 1 ? V3{ -1.0, 0.0, 0.0 } : F == 2 ? V3{ 0.0, -1.0, 0.0 } : V3{ 1.0, 0.0, 0.0 };
						for (double Ub = -Span - Sh; Ub < Span - 0.02; Ub += Bl)
						{
							const double Ua = std::max(-Span, Ub), Ue = std::min(Span, Ub + Bl);
							if (Ue - Ua < 0.04) continue;
							const double Um = 0.5 * (Ua + Ue), Round = F * 2.0 * P.Cw + Um + P.Cw;
							const uint64_t Kb = static_cast<uint64_t>(F * 4096 + Rr * 32) + static_cast<uint64_t>(std::llround((Ub + 1.0) * 10.0));
							const double Hold = NT * Clamp01(0.05 + 1.0 * Noise01(Round / 0.4, 0.5, P.S.Key ^ 0xB0B6));
							if (J + 0.7 * Unit(Hash(P.S.Key ^ 0xB0B4, Kb)) > Hold) continue;
							const V3 Mid = V3{ P.Cx + Lz, P.Cy, Zb + 0.5 * Ch } + N * (P.Cw - 0.5 * Tk) + Tg * Um;
							B.Box(Mid, Tg * (0.5 * (Ue - Ua) - 0.006), N * (0.5 * Tk - 0.004), V3{ 0.0, 0.0, 0.5 * Ch - 0.005 }, BrickCol(F, Um, Zb, Kb), 16);
						}
					}
				}
			}

			/** The cast-iron pots and the stove's oven fork left by it, and the bricks fallen from the chimney's top round
			 * its foot. */
			void Leavings(Pen& B, const Plan& P)
			{
				Rng Rl(P.S.Key ^ 0xB0C9);
				const V3 Mouth{ P.Sx, P.Sy + P.My * (P.Sb + 0.5), 0.0 };
				for (int K = 0; K < 2; ++K)
				{
					const double X = Mouth.X + Rl.Range(-0.6, 0.6), Y = Mouth.Y + P.My * Rl.Range(0.0, 0.5), R = Rl.Range(0.12, 0.16), Tip = Rl.Next();
					const double Z = AshAt(P, X, Y);
					const V3 Base{ X, Y, Z - 0.02 };
					const V3 Ax = Tip < 0.5 ? Up : Norm(V3{ 0.6, 0.3, 0.75 });
					B.Tube(Base, Base + Ax * (R * 0.9), R * 0.75, R, 10, Rgb(40, 38, 36), false, true);
					B.Tube(Base + Ax * (R * 0.9), Base + Ax * (R * 1.1), R, R * 0.92, 10, Rgb(34, 32, 30), false);
				}
				// (the oven fork: a pole and its iron horns, leaning on the stove)
				{
					const V3 A{ P.Sx + P.Sa * 0.6, P.Sy + P.My * (P.Sb + 0.7), AshAt(P, P.Sx + P.Sa * 0.6, P.Sy + P.My * (P.Sb + 0.7)) };
					const V3 Bt{ P.Sx + P.Sa * 0.6, P.Sy + P.My * (P.Sb + 0.03), P.Sh - 0.1 };
					B.Tube(A, Bt, 0.02, 0.018, 6, Rgb(40, 34, 30), true, true);
					B.Tube(Bt, Bt + V3{ 0.12, 0.0, 0.08 }, 0.008, 0.006, 4, Rgb(48, 44, 42), true);
					B.Tube(Bt, Bt + V3{ -0.12, 0.0, 0.08 }, 0.008, 0.006, 4, Rgb(48, 44, 42), true);
				}
				// (bricks fallen from the chimney's top, lying round its foot)
				for (int K = 0; K < 14; ++K)
				{
					const double An = Rl.Range(0.0, 2.0 * Pi), D = Rl.Range(0.35, 1.5), Yw = Rl.Range(0.0, Pi), Tn = Rl.Range(0.75, 1.05);
					const double X = P.Cx + D * std::cos(An), Y = P.Cy + D * std::sin(An);
					if (std::fabs(X - P.Sx) < P.Sa + 0.05 && std::fabs(Y - P.Sy) < P.Sb + 0.05) continue;
					const double Z = AshAt(P, X, Y) + 0.02;
					B.Box(V3{ X, Y, Z }, V3{ std::cos(Yw), std::sin(Yw), 0.0 } * 0.125, V3{ -std::sin(Yw), std::cos(Yw), 0.0 } * 0.06, V3{ 0.0, 0.0, 0.032 }, Mixed(Scaled(P.Brick, Tn), P.Soot, 0.3), 16);
				}
			}

			// ---------------------------------------------------------------- the middle and far levels

			void Mid(Pen& B, const Plan& P)
			{
				Ashes(B, P, 1.4);
				for (int F = 0; F < 4; ++F) StoveSide(B, P, F, 0.6, false);
				StoveTop(B, P, 0.8);
				Chimney(B, P, false);
				if (P.bClay)
				{
					ClayWalls(B, P, false);
					ClayRoof(B, P, false);
				}
				else Logs(B, P, false);
				Fallen(B, P, false);
			}
			void Far(Pen& B, const Plan& P)
			{
				// (the colours as the middle level's read from afar: its ash bed, its stove, its chimney, its walls' remains)
				const Col Ash = Mixed(Mixed(Mixed(Rgb(54, 51, 48), Rgb(92, 89, 84), 0.5), Rgb(118, 114, 108), 0.18), Rgb(104, 78, 58), P.bClay ? 0.2 : 0.1);
				const double Ins = P.bClay ? P.Wt + 0.2 : 0.75;
				B.Quad1(V3{ -P.Hx + Ins, -P.Hy + Ins, 0.06 }, V3{ P.Hx - Ins, -P.Hy + Ins, 0.06 }, V3{ P.Hx - Ins, P.Hy - Ins, 0.06 }, V3{ -P.Hx + Ins, P.Hy - Ins, 0.06 }, Ash, Up);
				const Col Stove = Mixed(P.Wash, P.Soot, 0.5);
				B.Box(V3{ P.Sx, P.Sy, 0.5 * P.Sh }, V3{ P.Sa, 0.0, 0.0 }, V3{ 0.0, P.Sb, 0.0 }, V3{ 0.0, 0.0, 0.5 * P.Sh }, Stove, 16);
				const double Lx = (P.Ct - P.Sh) * P.Lean * 0.5;
				B.Box(V3{ P.Cx + Lx, P.Cy, 0.5 * (P.Sh + P.Ct) }, V3{ P.Cw, 0.0, 0.0 }, V3{ 0.0, P.Cw, 0.0 }, V3{ 0.0, 0.0, 0.5 * (P.Ct - P.Sh) }, Mixed(Mixed(P.Brick, Rgb(92, 84, 78), 0.25), P.Soot, 0.8), 16);
				// (the walls' remains: an izba's a few broken dark lengths of its sill logs, no frame; a khata's clay walls
				// standing, washed outside)
				const double Lx_ = P.Hx - P.Wt, Ly = P.Hy - P.Wt;
				if (!P.bClay)
				{
					const Col Wc = Mixed(P.Char, P.Ash, 0.2);
					for (int W = 0; W < 4; ++W)
						for (int K = 0; K < 2; ++K)
						{
							const uint64_t H = Hash(P.S.Key ^ 0xB0FD, static_cast<uint64_t>(W * 4 + K));
							if (Unit(Hash(H, 3)) < 0.3) continue;
							const bool bAlongX = W < 2;
							const double Sg = W % 2 == 0 ? 1.0 : -1.0, Half = bAlongX ? Lx_ : Ly, Len = 0.45 + 0.8 * Unit(Hash(H, 1)), U = (K == 0 ? -1.0 : 1.0) * Half * (0.22 + 0.5 * Unit(Hash(H, 2)));
							B.Box(bAlongX ? V3{ U, Sg * Ly, 0.36 } : V3{ Sg * Lx_, U, 0.36 }, bAlongX ? V3{ Len, 0.0, 0.0 } : V3{ 0.0, Len, 0.0 }, bAlongX ? V3{ 0.0, 0.13, 0.0 } : V3{ 0.13, 0.0, 0.0 }, V3{ 0.0, 0.0, 0.13 }, Wc, 16);
						}
					return;
				}
				const double Hh = 0.85, Tw = 0.2;
				const Col Wc = Mixed(Mixed(P.Wash, Rgb(150, 146, 138), 0.3), P.Soot, 0.35);
				B.Box(V3{ 0.0, Ly, Hh }, V3{ Lx_ + Tw, 0.0, 0.0 }, V3{ 0.0, Tw, 0.0 }, V3{ 0.0, 0.0, Hh }, Wc, 16);
				B.Box(V3{ 0.0, -Ly, Hh }, V3{ Lx_ + Tw, 0.0, 0.0 }, V3{ 0.0, Tw, 0.0 }, V3{ 0.0, 0.0, Hh }, Wc, 16);
				B.Box(V3{ Lx_, 0.0, Hh }, V3{ Tw, 0.0, 0.0 }, V3{ 0.0, Ly - Tw, 0.0 }, V3{ 0.0, 0.0, Hh }, Wc, 16);
				B.Box(V3{ -Lx_, 0.0, Hh }, V3{ Tw, 0.0, 0.0 }, V3{ 0.0, Ly - Tw, 0.0 }, V3{ 0.0, 0.0, Hh }, Wc, 16);
			}

			void Build(const Spec& S, Levels& Out)
			{
				Out = Levels();
				const Plan P = Make(S);
				{
					Pen B(Out.Near);
					Ashes(B, P, 0.3);
					for (int F = 0; F < 4; ++F) StoveSide(B, P, F, 0.1, true);
					StoveTop(B, P, 0.15);
					Chimney(B, P, true);
					if (P.bClay)
					{
						ClayWalls(B, P, true);
						Flakes(B, P);
						ClayRoof(B, P, true);
					}
					else Logs(B, P, true);
					Fallen(B, P, true);
					Leavings(B, P);
				}
				{
					Pen B(Out.Mid);
					Mid(B, P);
				}
				{
					Pen B(Out.Far);
					Far(B, P);
				}
				SnowOver(Out.Near, P, true);
				SnowOver(Out.Mid, P, true);
				SnowOver(Out.Far, P, false);
			}
		}

		// ====================================================================== the desert's flat-roofed house (Kind 10)

		/**
		 * The flat-roofed house of a North African village (Kind 10; release 2, the trial "deserthouses": the desert's kit houses at
		 * the vehicles' detail), on the kit's footprint, some 11 m square: the main room across the back of a walled yard, a
		 * lower wing beside it, the yard's gate in the front (+y). Mud brick under a plaster laid by hand: the walls lean in as
		 * they rise, their faces a little wavy, their corners and their parapets' tops rounded, the parapet stood up into a
		 * horn at each corner on some. Limewashed (Roof 0), or left in its ochre mud with the yard's wall in bare brick (Roof 1),
		 * or limewashed with the main room of two storeys (Roof 2). Built with the main room at -x; half of them mirrored end
		 * for end. Its doors ledged planks under a timber lintel, their paint chalked; its windows small, shuttered or barred;
		 * its roof beams' ends out through the wall under the parapet, clay spouts to carry the rain clear and the run each has
		 * left down the wall; a stair up the wing's front to its roof, a bench by the door, the bread oven in the yard's corner,
		 * the water jars, a shade of palm fronds on poles; the mud bricks bare in their courses where the plaster has come away,
		 * up from a wall's foot or down from under its parapet.
		 */
		namespace Dsr
		{
			/** An opening in a block's wall: its face (3 +x, 0 +y, 1 -x, 2 -y), its middle along the face's way, its width, sill
			 * and head; what it is (0 a window, 1 a door, 2 the yard's gate: the wall left out from the ground to its top). */
			struct Gap
			{
				int Face = 0;
				double U = 0.0, W = 0.6, Z0 = 1.4, Z1 = 2.0;
				int What = 0;
				int Look = 0; // (a window's: 0 its shutter shut, 1 its bars; 3 no window but a small three-cornered niche)
			};

			/** A block of the house, its rooms under one roof; or the yard's wall round the footprint (no roof: its inner face
			 * down to the yard's ground). */
			/** A spout through a block's parapet at its roof's deck: its face, its place along it. */
			struct Spout
			{
				int F = 0;
				double U = 0.0;
			};

			struct Block
			{
				double Cx = 0.0, Cy = 0.0, Hx = 3.0, Hy = 2.4; // its middle; its half sizes at the floor (its outer face)
				double Zr = 3.0, Zp = 3.4;                     // its roof's deck; its parapet's top
				double Rc = 0.16, Tp = 0.3;                    // its corners' radius; its parapet's thickness
				double Horn = 0.0;                             // how far its parapet stands up at each corner
				double PierU = 0.0, PierW = 0.0, PierUp = 0.0; // a gate in its face 0: the wall's top raised either side of it
				// (its face 0's parapet let down to a kerb from here to its -x end and round that corner: a stair's head)
				double NotchFrom = 1e9;
				std::vector<Spout> Spouts;
				uint64_t Key = 1;
				bool bYard = false;
				std::vector<Gap> Gaps;
				double X0() const { return Cx - Hx; }
				double X1() const { return Cx + Hx; }
				double Y0() const { return Cy - Hy; }
				double Y1() const { return Cy + Hy; }
			};

			struct Plan
			{
				Spec S;
				double Hx = 5.4, Hy = 5.4;                         // the footprint's half sizes
				double Side = 1.0;                                 // -1: the whole mirrored end for end
				bool bMud = false, bTwo = false, bBand = false, bRug = false, bShade = false, bNiche = false;
				bool bAlone = false;                               // (the tomb's: its room stands clear inside its court's wall)
				double Bt = 0.022;                                 // the walls' batter: in for each metre up
				Block Main, Wing, Yard;
				Col Wash, Mud, Brick, Roof, Paint, Wood, Dust, Earth;
				// (its stair up the wing's front: its landing against the main room's wall, its steps down toward the wing's door)
				int Steps = 10;
				double Rise = 0.23, Run = 0.25, Land = 0.8, StairW = 0.78;
				double OvenX = 0.0, OvenY = 0.0, OvenTo = 0.0; // the bread oven in the yard's corner; the way its mouth looks
				int Jars = 2;
				/** A place the plaster has come away to the mud bricks, hung from an edge of its wall and never a badge in its
				 * middle: a stretch round a block's walls, from Pm0 to Pm1 of their own running metre (round a corner where it
				 * must). Hang 0: up from the wall's foot, where the damp lifts it, a long low band at most Tall high; 1: down
				 * from under the parapet's roll, at most Tall deep; 2: a wall never plastered above a man's waist (a wavy line
				 * about ZAt), bare from there to under its top. Its ends run out, or (bCutA, bCutB) stop square against a room's
				 * wall. bFallen: what fell lies at the wall's foot. For those who ask what is where: its face, its middle along
				 * it, its heights. */
				struct Scab
				{
					int Blk = 0, Hang = 0;
					double Pm0 = 0.0, Pm1 = 1.0, Tall = 0.4, ZAt = 0.0;
					uint64_t Seed = 1;
					bool bCutA = false, bCutB = false, bFallen = false;
					int F = 0;
					double U = 0.0, Z0 = 0.0, Z1 = 0.5;
				};
				/** A crack up a wall's plaster. */
				struct Crack
				{
					int Blk = 0, F = 0;
					double U = 0.0, Z0 = 0.0, Z1 = 1.0;
					uint64_t Seed = 1;
				};
				std::vector<Scab> Scabs;
				std::vector<Crack> Cracks;
			};
			const Block& BlockOf(const Plan& P, int I) { return I == 0 ? P.Main : I == 1 ? P.Wing : P.Yard; }

			/** A face of a block: its middle on the outer face at the floor, its way along (round the block counter-clockwise
			 * seen from above), out from it, the half length of its straight part, and how far out from the block's middle it
			 * stands. */
			struct Frame
			{
				V3 C, T, N;
				double Half = 1.0, Out = 1.0;
			};
			Frame FaceOf(const Block& Bk, int F)
			{
				const V3 M{ Bk.Cx, Bk.Cy, 0.0 };
				switch (F)
				{
				case 3: return { M + V3{ Bk.Hx, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, V3{ 1.0, 0.0, 0.0 }, Bk.Hy - Bk.Rc, Bk.Hx };
				case 0: return { M + V3{ 0.0, Bk.Hy, 0.0 }, V3{ -1.0, 0.0, 0.0 }, V3{ 0.0, 1.0, 0.0 }, Bk.Hx - Bk.Rc, Bk.Hy };
				case 1: return { M + V3{ -Bk.Hx, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, V3{ -1.0, 0.0, 0.0 }, Bk.Hy - Bk.Rc, Bk.Hx };
				default: return { M + V3{ 0.0, -Bk.Hy, 0.0 }, V3{ 1.0, 0.0, 0.0 }, V3{ 0.0, -1.0, 0.0 }, Bk.Hx - Bk.Rc, Bk.Hy };
				}
			}
			/** A face's way along for a place on it given in the house's own x (faces 0 and 2) or y (1 and 3). */
			double UOf(const Block& Bk, int F, double Along)
			{
				const Frame Fc = FaceOf(Bk, F);
				return F % 2 == 0 ? (Along - Bk.Cx) * Fc.T.X : (Along - Bk.Cy) * Fc.T.Y;
			}

			/** An opening's middle on its wall's line at the floor. */
			V3 MidOf(const Block& Bk, const Gap& G)
			{
				const Frame Fc = FaceOf(Bk, G.Face);
				return Fc.C + Fc.T * G.U;
			}

			/** How far round a block's walls a place on one of its faces is: from face 3's start, round by 0, 1 and 2, each
			 * corner's quarter after its face. */
			double PmOf(const Block& Bk, int F, double U)
			{
				double Pm = 0.0;
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4 && Order[I] != F; ++I) Pm += 2.0 * FaceOf(Bk, Order[I]).Half + 0.5 * Pi * Bk.Rc;
				return Pm + U + FaceOf(Bk, F).Half;
			}

			Plan Make(const Spec& S)
			{
				Plan P;
				P.S = S;
				Rng Rg(S.Key ^ 0xD5E7);
				P.Hx = 0.5 * S.Long - 0.1;
				P.Hy = 0.5 * S.Deep - 0.1;
				P.bMud = S.Roof == 1;
				P.bTwo = S.Roof == 2;
				const double USide = Rg.Next(), UHorn = Rg.Next(), UHornUp = Rg.Next(), UBand = Rg.Next(), URug = Rg.Next(), UShade = Rg.Next();
				const double UPaint = Rg.Next(), UWash = Rg.Next(), ULong = Rg.Next(), UDeep = Rg.Next(), UHigh = Rg.Next(), UPar = Rg.Next();
				const double UWingDeep = Rg.Next(), UWingHigh = Rg.Next(), UWingPar = Rg.Next(), UYard = Rg.Next(), UGate = Rg.Next();
				P.Side = USide < 0.5 ? 1.0 : -1.0;
				P.bBand = UBand < 0.6;
				P.bRug = URug < 0.4;
				P.bShade = UShade < 0.65;
				// Its colours: the limewash white, cream or a little blue; the mud plaster the ground's own earth, a step or two
				// under the sand's tone, its bricks a shade darker; the roof's mud paler; one paint for the doors and shutters (a
				// faded blue, a turquoise, a green or an old brown), chalky.
				// (the bare mud house's walls a darker earth than what shows through a limewashed one's wear: sunlit in the
				// field's stills about 185, 160, 122 against sand at 223, 211, 181)
				const Col Chalk = Rgb(196, 190, 176);
				P.Wash = UWash < 0.6 ? Rgb(238, 234, 222) : UWash < 0.85 ? Rgb(240, 230, 206) : Rgb(228, 234, 236);
				P.Mud = Varied(P.bMud ? Rgb(136, 118, 98) : Rgb(198, 171, 130), 0.97 + 0.07 * UWash, 0.0);
				P.Brick = Scaled(P.Mud, 0.88);
				P.Roof = P.bMud ? Rgb(212, 190, 150) : Rgb(224, 212, 188);
				P.Paint = Mixed(UPaint < 0.4 ? Rgb(76, 124, 158) : UPaint < 0.62 ? Rgb(78, 141, 138) : UPaint < 0.82 ? Rgb(92, 126, 92) : Rgb(122, 88, 58), Chalk, 0.28);
				P.Wood = Rgb(128, 112, 92);
				P.Dust = Rgb(214, 190, 146);
				P.Earth = Rgb(170, 138, 98);
				// The yard's wall round the footprint, a little inside the house's own faces.
				P.Yard.bYard = true;
				P.Yard.Hx = P.Hx - 0.07;
				P.Yard.Hy = P.Hy - 0.07;
				P.Yard.Zr = 0.0;
				P.Yard.Zp = 1.85 + 0.25 * UYard;
				P.Yard.Rc = 0.3;
				P.Yard.Tp = 0.34;
				P.Yard.Key = Hash(S.Key, 0xD0);
				// The main room across the back at -x; the lower wing beside it, set a little in from the back.
				const double Lm = (0.54 + 0.05 * ULong) * 2.0 * P.Hx, Dm = std::min(4.5 + 0.5 * UDeep, 0.95 * P.Hy);
				P.Main.Hx = 0.5 * Lm;
				P.Main.Hy = 0.5 * Dm;
				P.Main.Cx = -P.Hx + P.Main.Hx;
				P.Main.Cy = -P.Hy + P.Main.Hy;
				P.Main.Zr = P.bTwo ? 5.55 + 0.3 * UHigh : 2.95 + 0.3 * UHigh;
				P.Main.Zp = P.Main.Zr + 0.36 + 0.1 * UPar;
				P.Main.Horn = UHorn < 0.45 ? 0.2 + 0.12 * UHornUp : 0.0;
				P.Main.Key = Hash(S.Key, 0xD1);
				const double Wx0 = P.Main.X1() - 0.14, Wy0 = -P.Hy + 0.25, Dw = std::min(3.0 + 0.4 * UWingDeep, Dm - 0.9);
				P.Wing.Hx = 0.5 * (P.Hx - Wx0);
				P.Wing.Hy = 0.5 * Dw;
				P.Wing.Cx = Wx0 + P.Wing.Hx;
				P.Wing.Cy = Wy0 + P.Wing.Hy;
				P.Wing.Zr = 2.35 + 0.2 * UWingHigh;
				P.Wing.Zp = P.Wing.Zr + 0.28 + 0.08 * UWingPar;
				P.Wing.Horn = 0.7 * P.Main.Horn;
				P.Wing.Key = Hash(S.Key, 0xD2);
				// The openings. The main room's door and a window onto the yard, a small high window or two in its back; the
				// wing's door by its outer end (its stair runs up the rest of its front) and a small window in that end; an
				// upper room's windows and its door out onto the wing's roof; the yard's gate.
				auto Add = [](Block& Bk, int F, double Along, double W, double Z0, double Z1, int What, int Look) {
					Gap G;
					G.Face = F;
					G.U = UOf(Bk, F, Along);
					G.W = W;
					G.Z0 = Z0;
					G.Z1 = Z1;
					G.What = What;
					G.Look = Look;
					Bk.Gaps.push_back(G);
				};
				const double UDoor = Rg.Next(), UWin = Rg.Next(), UBack = Rg.Next(), UBack2 = Rg.Next(), ULookA = Rg.Next(), ULookB = Rg.Next(), ULookC = Rg.Next();
				const double DoorX = P.Main.Cx + (0.15 + 0.2 * UDoor) * P.Main.Hx;
				Add(P.Main, 0, DoorX, 0.95, 0.1, 2.02, 1, 0);
				Add(P.Main, 0, P.Main.Cx - (0.5 + 0.15 * UWin) * P.Main.Hx, 0.56, 1.3, 1.98, 0, ULookA < 0.6 ? 0 : 1);
				Add(P.Main, 2, P.Main.Cx + (0.6 * UBack - 0.5) * P.Main.Hx, 0.4, 1.86, 2.28, 0, 1);
				if (UBack2 < 0.5) Add(P.Main, 2, P.Main.Cx + (0.35 + 0.3 * UBack2) * P.Main.Hx, 0.4, 1.86, 2.28, 0, 1);
				Add(P.Wing, 0, P.Wing.X1() - 0.78, 0.86, 0.1, 1.92, 1, 0);
				Add(P.Wing, 3, P.Wing.Cy + 0.2, 0.44, 1.36, 1.86, 0, ULookB < 0.5 ? 0 : 1);
				if (P.bTwo)
				{
					const double Zu = 3.75, Zh = 4.6;
					Add(P.Main, 0, P.Main.Cx - 0.48 * P.Main.Hx, 0.62, Zu, Zh, 0, 0);
					Add(P.Main, 0, P.Main.Cx + 0.42 * P.Main.Hx, 0.62, Zu, Zh, 0, 0);
					Add(P.Main, 2, P.Main.Cx - 0.1 * P.Main.Hx, 0.62, Zu, Zh, 0, ULookC < 0.5 ? 0 : 1);
					Add(P.Main, 3, P.Wing.Cy, 0.8, P.Wing.Zr + 0.12, P.Wing.Zr + 1.95, 1, 0);
				}
				const double GateX = (0.05 + 0.17 * UGate) * P.Hx;
				Add(P.Yard, 0, GateX, 1.5, 0.0, 0.0, 2, 0);
				P.Yard.PierU = P.Yard.Gaps.back().U;
				P.Yard.PierW = 1.5;
				P.Yard.PierUp = 0.32;
				// The stair up the wing's front to its roof: its landing against the main room's wall, the wing's parapet let down
				// to a kerb there; its steps down toward the wing's door, as steep as the room left for them makes them.
				const double Zs = P.Wing.Zr - 0.08;
				P.Steps = std::max(6, static_cast<int>(std::lround(Zs / 0.235)));
				P.Rise = Zs / P.Steps;
				P.Run = std::clamp(((P.Wing.X1() - 1.31) - (P.Main.X1() + P.Land)) / (P.Steps - 1), 0.2, 0.3);
				P.Wing.NotchFrom = UOf(P.Wing, 0, P.Main.X1() + P.Land);
				// The spouts: the main room's roof out through its back and, by its far end, into the yard; the wing's out of its end.
				const double USp = Rg.Next(), UNiche = Rg.Next(), UJars = Rg.Next();
				P.Main.Spouts.push_back({ 2, UOf(P.Main, 2, P.Main.Cx + (0.72 + 0.13 * USp) * P.Main.Hx) });
				P.Main.Spouts.push_back({ 0, UOf(P.Main, 0, P.Main.X0() + 0.42) });
				P.Wing.Spouts.push_back({ 3, -0.35 * P.Wing.Hy });
				// (the three small niches over the main room's door, three-cornered, let into the wall: openings of their own
				// kind, after all the rest; on a house of one storey, whose front has no other opening near them)
				P.bNiche = UNiche < 0.55 && !P.bTwo;
				if (P.bNiche)
				{
					const Gap Dr = P.Main.Gaps[0];
					for (int K = -1; K <= 1; ++K)
					{
						Gap G;
						G.Face = 0;
						G.U = Dr.U + 0.3 * K;
						G.W = 0.18;
						G.Z0 = Dr.Z1 + 0.38;
						G.Z1 = G.Z0 + 0.17;
						G.What = 0;
						G.Look = 3;
						P.Main.Gaps.push_back(G);
					}
				}
				P.Jars = UJars < 0.5 ? 2 : 3;
				// The oven in the yard's corner before the main room, its mouth to the yard's middle.
				P.OvenX = P.Yard.X0() + P.Yard.Tp + 0.85;
				P.OvenY = P.Yard.Y1() - P.Yard.Tp - 0.85;
				P.OvenTo = -0.25 * Pi;
				// Where the plaster has come away to the bricks, each place hung from an edge of its wall and none a badge in
				// its middle: up from the foot in a long low band, where the damp lifts it; down from under the parapet, where
				// the rain gets in behind it, along a wall or round a corner of it. On the bare mud house the yard's wall was
				// never plastered above a man's waist: bare from there to its top, the whole way round from the rooms to the
				// gate's posts.
				// (a house well kept, one of its years, one let go: how often a face of it has lost a patch)
				const double UWear = Unit(Hash(S.Key, 0x5CAA));
				const int Wear = UWear < 0.35 ? 0 : UWear < 0.8 ? 1 : 2;
				const double More = P.bMud ? 1.3 : 1.0, AtFoot[3] = { 0.05, 0.16, 0.34 }, UpHigh[3] = { 0.02, 0.08, 0.2 };
				// (whether a stretch of a face, U its middle and Hw its half length, is clear of the face's openings from Z0 to
				// Z1, and of its spouts where they matter; bEnds: and of the face's two ends)
				auto Clear = [&](const Block& Bk, int F, double U, double Hw, double Z0, double Z1, bool bSpouts, bool bEnds) {
					if (bEnds && std::fabs(U) + Hw > FaceOf(Bk, F).Half - 0.12) return false;
					for (const Gap& G : Bk.Gaps)
						if (G.Face == F && std::fabs(U - G.U) < Hw + 0.5 * G.W + 0.3 && (G.What == 2 || (Z0 < G.Z1 + 0.35 && Z1 > G.Z0 - 0.3))) return false;
					if (bSpouts)
						for (const Spout& Sp : Bk.Spouts)
							if (Sp.F == F && std::fabs(U - Sp.U) < Hw + 0.3) return false;
					return true;
				};
				auto Put = [&](int Blk, int F, int Hang, double U, double Pm0, double Pm1, double Tall, double Z0, double Z1, uint64_t Seed) {
					Plan::Scab Sc;
					Sc.Blk = Blk;
					Sc.Hang = Hang;
					Sc.F = F;
					Sc.U = U;
					Sc.Pm0 = Pm0;
					Sc.Pm1 = Pm1;
					Sc.Tall = Tall;
					Sc.Z0 = Z0;
					Sc.Z1 = Z1;
					Sc.Seed = Seed;
					// (apart from the others on its walls, the loop round them closing on itself)
					const Block& Bk = BlockOf(P, Blk);
					const double All = PmOf(Bk, 2, FaceOf(Bk, 2).Half) + 0.5 * Pi * Bk.Rc;
					for (const Plan::Scab& O : P.Scabs)
						for (int Lap = -1; Lap <= 1; ++Lap)
							if (O.Blk == Blk && Pm0 + Lap * All < O.Pm1 + 0.4 && Pm1 + Lap * All > O.Pm0 - 0.4 && Z0 < O.Z1 + 0.3 && Z1 > O.Z0 - 0.3) return false;
					// (what falls from high on a street wall lies at its foot; the yard is swept)
					Sc.bFallen = Hang == 1 && (Pm1 - Pm0) * Tall > 0.5 && (Blk == 2 || F == 2 || (Blk == 0 && F == 1) || (Blk == 1 && F == 3));
					P.Scabs.push_back(Sc);
					return true;
				};
				// (the yard's wall stands free only before the rooms: a face's stretch there, along its own way)
				const double Out1 = (P.Yard.Cy - P.Main.Y1()) - 0.35, Out3 = (P.Wing.Y1() - P.Yard.Cy) + 0.35;
				const int Where[8][2] = { { 0, 0 }, { 0, 2 }, { 0, 1 }, { 1, 3 }, { 1, 2 }, { 2, 0 }, { 2, 1 }, { 2, 3 } };
				for (int I = 0; I < 8; ++I)
				{
					const int Blk = Where[I][0], F = Where[I][1];
					const Block& Bk = BlockOf(P, Blk);
					const double Half = FaceOf(Bk, F).Half, Ztop = Bk.Zp - (Blk == 2 ? 0.1 : 0.08);
					auto Free = [&](double U, double Hw) { return !(Blk == 2 && F == 1 && U + Hw > Out1) && !(Blk == 2 && F == 3 && U - Hw < Out3); };
					Rng Rs(Hash(S.Key, 0x5CAB, static_cast<uint64_t>(I)));
					const double UFoot = Rs.Next(), UTop = Rs.Next();
					// (a stretch of its foot, 3 to 4.6 m of it and 20 to 50 cm high, its ends thinning away over a metre each:
					// not along the rooms' fronts, where the yard is kept)
					if (I > 0 && UFoot < AtFoot[Wear] * More)
						for (int Try = 0; Try < 6; ++Try)
						{
							const double Ua = Rs.Next(), Ub = Rs.Next(), Uc = Rs.Next();
							const double Hw = std::min(1.5 + 0.8 * Ub, Half - 0.2), Tall = 0.26 + 0.24 * Uc, U = (2.0 * Ua - 1.0) * (Half - 0.12 - Hw);
							if (Hw < 0.7 || !Free(U, Hw) || !Clear(Bk, F, U, Hw, -0.2, Tall, false, true)) continue;
							if (Put(Blk, F, 0, U, PmOf(Bk, F, U - Hw), PmOf(Bk, F, U + Hw), Tall, -0.2, Tall, Hash(S.Key, 0x5CAC, static_cast<uint64_t>(I * 8 + Try)))) break;
						}
					// (down from under its parapet; the bare mud house's yard wall is bare there already)
					if (!(P.bMud && Blk == 2) && UTop < UpHigh[Wear] * More)
						for (int Try = 0; Try < 6; ++Try)
						{
							const double Ua = Rs.Next(), Ub = Rs.Next(), Uc = Rs.Next();
							const double Hw = 0.45 + 0.65 * Ub, Tall = Blk == 2 ? 0.3 + 0.35 * Uc : 0.35 + 0.55 * Uc, U = (2.0 * Ua - 1.0) * std::max(0.0, Half - 0.12 - Hw);
							if (!Free(U, Hw) || !Clear(Bk, F, U, Hw, Ztop - Tall, Ztop, true, true)) continue;
							if (Put(Blk, F, 1, U, PmOf(Bk, F, U - Hw), PmOf(Bk, F, U + Hw), Tall, Ztop - Tall, Ztop, Hash(S.Key, 0x5CAD, static_cast<uint64_t>(I * 8 + Try)))) break;
						}
				}
				// (and down from under the parapet round a corner, where a wall weathers first: the main room's and the wing's
				// street corners at the back, the yard wall's two at the front; so far along the face before the corner, so far
				// along the one after)
				const int Corner[4][3] = { { 0, 1, 2 }, { 1, 2, 3 }, { 2, 3, 0 }, { 2, 0, 1 } };
				for (int I = 0; I < 4; ++I)
				{
					const int Blk = Corner[I][0], Fa = Corner[I][1], Fb = Corner[I][2];
					const Block& Bk = BlockOf(P, Blk);
					Rng Rs(Hash(S.Key, 0x5CAE, static_cast<uint64_t>(I)));
					const double UHas = Rs.Next(), Ua = Rs.Next(), Ub = Rs.Next(), Uc = Rs.Next();
					if ((P.bMud && Blk == 2) || UHas > 0.8 * UpHigh[Wear] * More) continue;
					const double Ha = FaceOf(Bk, Fa).Half, Hb = FaceOf(Bk, Fb).Half, La = 0.25 + 0.75 * Ua, Lb = 0.25 + 0.75 * Ub;
					const double Ztop = Bk.Zp - (Blk == 2 ? 0.1 : 0.08), Tall = Blk == 2 ? 0.3 + 0.3 * Uc : 0.35 + 0.5 * Uc;
					if (!Clear(Bk, Fa, Ha - 0.5 * La, 0.5 * La, Ztop - Tall, Ztop + Bk.Horn, true, false) || !Clear(Bk, Fb, -Hb + 0.5 * Lb, 0.5 * Lb, Ztop - Tall, Ztop + Bk.Horn, true, false)) continue;
					// (the wing's is where the loop round its walls closes: on round it)
					const double Pm0 = PmOf(Bk, Fa, Ha - La), Pm1 = PmOf(Bk, Fb, -Hb + Lb) + (Fb == 3 ? PmOf(Bk, 2, FaceOf(Bk, 2).Half) + 0.5 * Pi * Bk.Rc : 0.0);
					Put(Blk, Fa, 1, Ha - 0.5 * La, Pm0, Pm1, Tall, Ztop - Tall, Ztop + Bk.Horn, Hash(S.Key, 0x5CAF, static_cast<uint64_t>(I)));
				}
				if (P.bMud)
				{
					// (from the wing's wall up the yard's side and round to short of the gate's post, the plaster carried up the
					// post; from short of its other post round and down the far side to the main room's wall)
					const Block& Yd = P.Yard;
					const double Ga = Yd.PierU - 0.5 * Yd.PierW - 0.62, Gb = Yd.PierU + 0.5 * Yd.PierW + 0.62, H0 = FaceOf(Yd, 0).Half;
					for (int I = 0; I < 2; ++I)
					{
						Plan::Scab Sc;
						Sc.Blk = 2;
						Sc.Hang = 2;
						Sc.Pm0 = I == 0 ? PmOf(Yd, 3, UOf(Yd, 3, P.Wing.Y1() - 0.09)) : PmOf(Yd, 0, Gb);
						Sc.Pm1 = I == 0 ? PmOf(Yd, 0, Ga) : PmOf(Yd, 1, UOf(Yd, 1, P.Main.Y1() - 0.09));
						Sc.bCutA = I == 0;
						Sc.bCutB = I == 1;
						Sc.ZAt = 0.8 + 0.2 * Unit(Hash(S.Key, 0x5CB2, static_cast<uint64_t>(I)));
						Sc.Seed = Hash(S.Key, 0x5CB3, static_cast<uint64_t>(I));
						Sc.F = 0;
						Sc.U = I == 0 ? 0.5 * (Ga - H0) : 0.5 * (Gb + H0);
						Sc.Z0 = Sc.ZAt;
						Sc.Z1 = Yd.Zp - 0.1;
						Sc.Tall = Sc.Z1 - Sc.Z0;
						if (Sc.Pm1 - Sc.Pm0 > 1.2) P.Scabs.push_back(Sc);
					}
				}
				// Its cracks: up from one end of a window's head or the other to the parapet, up the yard's wall from its foot;
				// not every wall has one.
				auto Split = [&](int Blk, int F, double U, double Z0, double Z1, uint64_t Seed) {
					Plan::Crack Ck;
					Ck.Blk = Blk;
					Ck.F = F;
					Ck.U = U;
					Ck.Z0 = Z0;
					Ck.Z1 = Z1;
					Ck.Seed = Seed;
					P.Cracks.push_back(Ck);
				};
				const double UCa = Rg.Next(), UCb = Rg.Next(), UCc = Rg.Next(), UCd = Rg.Next();
				{
					const Gap& Wn = P.Main.Gaps[1];
					const double Sa = Unit(Hash(S.Key, 0xC4D)) < 0.5 ? 1.0 : -1.0, Sb = Unit(Hash(S.Key, 0xC4E)) < 0.5 ? 1.0 : -1.0;
					if (UCa < 0.55) Split(0, 0, Wn.U + Sa * (0.5 * Wn.W + 0.1), Wn.Z1 + 0.1, std::min(P.Main.Zr - 0.2, Wn.Z1 + 0.9 + 1.2 * UCa), Hash(S.Key, 0xC4A));
					const Gap& Bw = P.Main.Gaps[2];
					if (UCb < 0.45) Split(0, 2, Bw.U + Sb * (0.5 * Bw.W + 0.1), Bw.Z1 + 0.1, std::min(P.Main.Zr - 0.2, Bw.Z1 + 0.8 + 1.4 * UCb), Hash(S.Key, 0xC4B));
					if (UCc < 0.55) Split(2, 0, -FaceOf(P.Yard, 0).Half + 0.6 + 1.4 * UCd, 0.08, P.Yard.Zp - 0.25, Hash(S.Key, 0xC4C));
				}
				return P;
			}

			// ---------------------------------------------------------------- the walls' loop

			/** A column of a block's walls: its face and its place along it; or (bCorner) the corner after that face, U its
			 * angle there; the way out from the wall; how far round the walls it is (for the noise: the same on every loop). */
			struct Column
			{
				int F = 0;
				bool bCorner = false;
				double U = 0.0;
				V3 H;
				double Pm = 0.0;
			};
			/** A column's place on the line round the walls at the floor, drawn in by D (out where D is under 0): the sides come
			 * in and the corners' radius with them, to a sharp corner where D passes it. */
			V3 Ring(const Block& Bk, const Column& C, double D)
			{
				const double R = std::max(0.0, Bk.Rc - D), Ax = Bk.Hx - D - R, Ay = Bk.Hy - D - R;
				if (C.bCorner)
				{
					// (the corner after its face: +x +y after 3, -x +y after 0, -x -y after 1, +x -y after 2)
					const double Sx = (C.F == 3 || C.F == 2) ? 1.0 : -1.0, Sy = (C.F == 3 || C.F == 0) ? 1.0 : -1.0;
					return V3{ Bk.Cx + Sx * Ax + C.H.X * R, Bk.Cy + Sy * Ay + C.H.Y * R, 0.0 };
				}
				const bool bX = C.F == 3 || C.F == 1; // (a face across x: its way along is y)
				const double Lim = bX ? Ay : Ax, U = std::clamp(C.U, -Lim, Lim);
				const Frame Fc = FaceOf(Bk, C.F);
				return V3{ Bk.Cx, Bk.Cy, 0.0 } + Fc.N * (Fc.Out - D) + Fc.T * U;
			}
			/** The loop round a block's walls, in steps of about Step along the straight and NC round each corner; the
			 * openings' edges among its columns (the gate's always; the rest where asked). */
			std::vector<Column> Loop(const Block& Bk, double Step, int NC, bool bEdges)
			{
				std::vector<Column> Out;
				double Pm = 0.0;
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4; ++I)
				{
					const int F = Order[I];
					const Frame Fc = FaceOf(Bk, F);
					std::vector<double> All, Edges;
					const int N = std::max(1, static_cast<int>(std::ceil(2.0 * Fc.Half / Step)));
					for (int K = 0; K < N; ++K) All.push_back(-Fc.Half + 2.0 * Fc.Half * K / N);
					for (const Gap& G : Bk.Gaps)
						if (G.Face == F && (bEdges || G.What == 2))
						{
							Edges.push_back(G.U - 0.5 * G.W);
							Edges.push_back(G.U + 0.5 * G.W);
						}
					if (F == 0 && Bk.NotchFrom < 1e8)
					{
						Edges.push_back(Bk.NotchFrom - 0.1);
						Edges.push_back(Bk.NotchFrom);
					}
					if (bEdges)
						for (const Spout& Sp : Bk.Spouts)
							if (Sp.F == F)
							{
								Edges.push_back(Sp.U - 0.08);
								Edges.push_back(Sp.U);
								Edges.push_back(Sp.U + 0.08);
							}
					All.insert(All.end(), Edges.begin(), Edges.end());
					std::sort(All.begin(), All.end());
					std::vector<double> Kept;
					for (const double U : All)
					{
						if (U < -Fc.Half || U > Fc.Half - 1e-9) continue;
						const bool bEdge = std::find(Edges.begin(), Edges.end(), U) != Edges.end();
						// (no column of the wall's own through a niche: its square's corners are all the wall has there)
						bool bNiche = false;
						for (const Gap& G : Bk.Gaps)
							if (bEdges && !bEdge && G.Look == 3 && G.Face == F && std::fabs(U - G.U) < 0.5 * G.W - 1e-6) bNiche = true;
						if (bNiche) continue;
						if (!Kept.empty() && U - Kept.back() < 0.04)
						{
							if (bEdge && Kept.size() > 1) Kept.back() = U;
							continue;
						}
						Kept.push_back(U);
					}
					for (const double U : Kept) Out.push_back({ F, false, U, Fc.N, Pm + U + Fc.Half });
					Pm += 2.0 * Fc.Half;
					Out.push_back({ F, false, Fc.Half, Fc.N, Pm });
					for (int K = 1; K < NC; ++K)
					{
						const double A = 0.5 * Pi * (I + static_cast<double>(K) / NC);
						Out.push_back({ F, true, A, V3{ std::cos(A), std::sin(A), 0.0 }, Pm + 0.5 * Pi * Bk.Rc * K / NC });
					}
					Pm += 0.5 * Pi * Bk.Rc;
				}
				return Out;
			}
			/** A place on a straight face, as a column. */
			Column At(const Block& Bk, int F, double U)
			{
				double Pm = 0.0;
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4 && Order[I] != F; ++I) Pm += 2.0 * FaceOf(Bk, Order[I]).Half + 0.5 * Pi * Bk.Rc;
				const Frame Fc = FaceOf(Bk, F);
				return { F, false, U, Fc.N, Pm + U + Fc.Half };
			}
			/** How far the plaster stands out of the wall's true line at a place: laid by hand, wavy, a little bellied. */
			/** (No finer than what is laid on the walls can follow from its own corners: the bricks, the patches of paint.) */
			double Disp(const Block& Bk, double Pm, double Z) { return 0.014 * Noise(Pm / 1.1, Z / 0.9, Bk.Key ^ 0xD1) + 0.005 * Noise(Pm / 0.5, Z / 0.45, Bk.Key ^ 0xD2); }
			/** How far in a wall's true line stands at a height: it leans in as it rises, and its foot is thickened out. */
			double Lean(const Plan& P, double Z) { return P.Bt * std::max(0.0, Z) - 0.035 * Smooth(0.45, 0.0, Z); }
			/** A place on a block's outer face (Off out from it). */
			V3 WallPt(const Plan& P, const Block& Bk, const Column& C, double Z, double Off = 0.0)
			{
				return Ring(Bk, C, Lean(P, Z) - Disp(Bk, C.Pm, Z) - Off) + V3{ 0.0, 0.0, Z };
			}
			V3 WallN(const Plan& P, const Block& Bk, const Column& C, double Z)
			{
				const V3 T = Cross(Up, C.H);
				const double Du = (Disp(Bk, C.Pm + 0.03, Z) - Disp(Bk, C.Pm - 0.03, Z)) / 0.06, Dz = (Disp(Bk, C.Pm, Z + 0.03) - Disp(Bk, C.Pm, Z - 0.03)) / 0.06;
				const double Dl = (Lean(P, Z + 0.03) - Lean(P, Z - 0.03)) / 0.06;
				return Norm(C.H - T * Du - Up * (Dz - Dl));
			}
			/** The parapet's top over a column: a little up and down along it, stood up toward each corner where it has horns,
			 * and either side of a gate. */
			double TopAt(const Block& Bk, const Column& C)
			{
				const double Near = C.bCorner ? 0.0 : FaceOf(Bk, C.F).Half - std::fabs(C.U);
				double Z = Bk.Zp + (Bk.bYard ? 0.05 : 0.025) * Noise(C.Pm / 1.3, 0.5, Bk.Key ^ 0xD3) + 0.012 * Noise(C.Pm / 0.35, 1.5, Bk.Key ^ 0xD4);
				Z += Bk.Horn * std::pow(Smooth(0.6, 0.0, Near), 1.6);
				if (Bk.PierW > 0.0 && !C.bCorner && C.F == 0) Z += Bk.PierUp * Smooth(0.5 * Bk.PierW + 0.55, 0.5 * Bk.PierW + 0.2, std::fabs(C.U - Bk.PierU));
				if (Bk.NotchFrom < 1e8)
				{
					const double In = C.F == 0 ? (C.bCorner ? 1.0 : Smooth(Bk.NotchFrom - 0.1, Bk.NotchFrom, C.U)) : C.F == 1 ? 1.0 : 0.0;
					Z = Lerp(Z, Bk.Zr + 0.1, In);
				}
				return Z;
			}
			/** The rows up a block's outer face: from under the ground to its roof's deck (the yard's wall: to under its top),
			 * each opening's sill and head among them. */
			std::vector<double> Rows(const Plan& P, const Block& Bk, double Step, bool bEdges)
			{
				const double Bot = -std::max(0.25, P.S.Below), Zm = Bk.bYard ? Bk.Zp - 0.4 : Bk.Zr;
				std::vector<double> All{ Bot }, Edges;
				const int N = std::max(1, static_cast<int>(std::ceil(Zm / Step)));
				for (int K = 0; K <= N; ++K) All.push_back(Zm * K / N);
				if (bEdges)
					for (const Gap& G : Bk.Gaps)
						if (G.What != 2)
						{
							Edges.push_back(G.Z0);
							Edges.push_back(G.Z1);
						}
				All.insert(All.end(), Edges.begin(), Edges.end());
				std::sort(All.begin(), All.end());
				std::vector<double> Kept;
				for (const double Z : All)
				{
					const bool bEdge = std::find(Edges.begin(), Edges.end(), Z) != Edges.end();
					// (nor a row through one)
					bool bNiche = false;
					for (const Gap& G : Bk.Gaps)
						if (bEdges && !bEdge && G.Look == 3 && Z > G.Z0 + 1e-6 && Z < G.Z1 - 1e-6) bNiche = true;
					if (bNiche) continue;
					if (!Kept.empty() && Z - Kept.back() < 0.04)
					{
						if (bEdge && Kept.size() > 1) Kept.back() = Z;
						continue;
					}
					Kept.push_back(Z);
				}
				if (Kept.back() < Zm - 1e-6) Kept.back() = Zm;
				return Kept;
			}
			/** Whether a cell of a wall is in an opening (the gate's: the whole height). */
			bool InGap(const Block& Bk, const Column& A, const Column& Bc, double Za, double Zb, bool bCut)
			{
				if (A.bCorner || Bc.bCorner || A.F != Bc.F) return false;
				const double Um = 0.5 * (A.U + Bc.U), Zm = 0.5 * (Za + Zb);
				for (const Gap& G : Bk.Gaps)
				{
					if (G.Face != A.F || Um < G.U - 0.5 * G.W || Um > G.U + 0.5 * G.W) continue;
					if (G.What == 2 || (bCut && Zm > G.Z0 && Zm < G.Z1)) return true;
				}
				return false;
			}
			/** Whether a place is under the house's own roofs (the yard's wall is left out there: the rooms' walls stand). */
			bool Indoors(const Plan& P, V3 Q)
			{
				if (P.bAlone) return false;
				return (Q.X < P.Main.X1() - 0.15 && Q.Y < P.Main.Y1() - 0.15) || (Q.X > P.Wing.X0() && Q.Y < P.Wing.Y1() - 0.15);
			}

			/** The wall's colour at a place (Part 0 its outer face, 1 its parapet's top, 2 the parapet's inner face). Limewash:
			 * the mud's warmth through it where it is thin, grey-brown runs down from the parapet (none narrower than the wall's
			 * columns can draw), the yard's dust and splash up its foot. Bare mud: its plaster patched a little darker, runnels
			 * down from the parapet, damp earth at its foot, its top bleached. Under a spout, the roof's own mud the rain has
			 * washed down the wall: the mud plaster's colour and never darker, a narrow run at the spout that widens and pales
			 * as it falls, and what the falling water has splashed back up the wall's foot. */
			Col WallCol(const Plan& P, const Block& Bk, const Column& C, double Z, int Part)
			{
				const uint64_t K = Bk.Key;
				const double Foot = Smooth(0.75 + 0.35 * Noise(C.Pm / 0.8, 0.5, K ^ 0xA3), 0.04, Z);
				const double Run = Smooth(0.5, 0.72, Noise01(C.Pm / 0.6, Z / 4.0, K ^ 0xA4)) * Smooth(Bk.Zp - 2.3, Bk.Zp - 0.15, Z);
				double Wet = 0.0, Splash = 0.0;
				if (!C.bCorner && Part == 0)
					for (const Spout& Sp : Bk.Spouts)
					{
						const double Under = Bk.Zr - Z;
						if (Sp.F != C.F || Under < -0.02) continue;
						// (how far down the wall it has fallen, of the whole way; its width there; its edge wandering)
						const double Fall = Clamp01(Under / std::max(1.0, Bk.Zr)), Wide = 0.05 + 0.17 * Fall;
						const double Off = std::fabs(C.U - Sp.U + 0.03 * Noise(Z / 0.5, 3.0, K ^ 0xA6) * Clamp01(Under / 0.5));
						Wet = std::max(Wet, Smooth(Wide + 0.03 + 0.1 * Fall, 0.6 * Wide, Off) * (0.85 - 0.5 * Fall));
						Splash = std::max(Splash, Smooth(1.0, 0.45, std::hypot((C.U - Sp.U) / 0.55, std::max(0.0, Z) / 0.5)));
					}
				if (P.bMud)
				{
					Col M = Scaled(P.Mud, 0.92 + 0.11 * Noise01(C.Pm / 1.4, Z / 1.1, K ^ 0xA1));
					// (patched a little darker where it was mended; straw-pale where the last coat was a different pit's mud)
					M = Mixed(M, Scaled(P.Mud, 0.9), 0.5 * Smooth(0.55, 0.8, Noise01(C.Pm / 0.6, Z / 0.5, K ^ 0xA2)));
					M = Mixed(M, Mixed(P.Mud, P.Dust, 0.25), 0.4 * Smooth(0.5, 0.78, Noise01(C.Pm / 2.1, Z / 1.7, K ^ 0xA5)));
					M = Mixed(M, Scaled(P.Mud, 0.82), 0.45 * Run);
					// (the spout's run on mud: washed paler)
					M = Mixed(M, Mixed(P.Mud, P.Dust, 0.5), 0.6 * Wet);
					M = Mixed(M, Mixed(P.Earth, P.Dust, 0.2), 0.55 * Foot);
					M = Mixed(M, Mixed(P.Mud, P.Dust, 0.5), 0.4 * Splash);
					M = Mixed(M, P.Dust, 0.3 * Smooth(Bk.Zp - 0.9, Bk.Zp, Z));
					if (Part == 1) M = Mixed(M, P.Dust, 0.4);
					return M;
				}
				Col W = Scaled(P.Wash, 0.93 + 0.08 * Noise01(C.Pm / 1.6, Z / 1.3, K ^ 0xA1));
				// (the mud's warmth through it where it is thin; old coats gone grey-buff in broad clouds)
				W = Mixed(W, Mixed(P.Wash, P.Mud, 0.5), 0.62 * Smooth(0.5, 0.78, Noise01(C.Pm / 1.0, Z / 0.8, K ^ 0xA2)));
				W = Mixed(W, Rgb(200, 188, 164), 0.4 * Smooth(0.42, 0.75, Noise01(C.Pm / 2.3, Z / 1.9, K ^ 0xA5)));
				W = Mixed(W, Rgb(166, 148, 122), 0.45 * Run);
				W = Mixed(W, Mixed(P.Mud, P.Dust, 0.3), Wet);
				W = Mixed(W, P.Dust, 0.82 * Foot);
				W = Mixed(W, Mixed(P.Mud, P.Earth, 0.35), 0.5 * Splash);
				if (Part == 1) W = Mixed(W, P.Dust, 0.25);
				return W;
			}

			/** A block's walls: its outer face in columns and rows, leaning in, the openings' cells left out (where bCut) with
			 * their reveals; its parapet rolled over at the top and down its inner face to the roof's deck (the yard's wall: to
			 * the yard's ground; left out where the rooms stand, and at its gate, the wall's ends there closed). */
			void Shell(Pen& B, const Plan& P, const Block& Bk, double Step, double RowStep, int NC, bool bFine, bool bCut)
			{
				const std::vector<Column> L = Loop(Bk, Step, NC, bCut);
				const std::vector<double> Z = Rows(P, Bk, RowStep, bCut);
				const size_t N = L.size(), NZ = Z.size();
				const double Rr = Bk.bYard ? 0.1 : 0.08, Foot = Bk.bYard ? -0.25 : Bk.Zr - 0.07;
				// (the parapet's profile over the rows: how far in, how far under its top (under 0: at its foot), which part of
				// it, the way out from it there)
				struct CapRow
				{
					double In, Down;
					int Part;
					double Nh, Nu;
				};
				std::vector<CapRow> Cap;
				Cap.push_back({ 0.0, Rr, 0, 1.0, 0.0 });
				if (bFine) Cap.push_back({ 0.3 * Rr, 0.3 * Rr, 0, 0.7, 0.7 });
				Cap.push_back({ Rr, 0.0, 1, 0.0, 1.0 });
				Cap.push_back({ Bk.Tp - Rr, 0.0, 1, 0.0, 1.0 });
				if (bFine) Cap.push_back({ Bk.Tp - 0.3 * Rr, 0.3 * Rr, 2, -0.7, 0.7 });
				Cap.push_back({ Bk.Tp, Rr, 2, -1.0, 0.0 });
				Cap.push_back({ Bk.Tp, -1.0, 2, -1.0, 0.0 });
				const size_t NR = NZ + Cap.size();
				auto CapZ = [&](const Column& C, const CapRow& R) { return R.Down < 0.0 ? Foot : TopAt(Bk, C) - R.Down; };
				auto CapPt = [&](const Column& C, const CapRow& R) {
					const double Zc = CapZ(C, R), Wave = R.Part == 0 ? 1.0 : R.Part == 1 ? 0.5 : 0.0;
					return Ring(Bk, C, Lean(P, Zc) - Wave * Disp(Bk, C.Pm, Zc) + R.In) + V3{ 0.0, 0.0, Zc };
				};
				std::vector<uint32_t> Id(N * NR);
				for (size_t C = 0; C < N; ++C)
				{
					for (size_t R = 0; R < NZ; ++R) Id[C * NR + R] = B.Vert(WallPt(P, Bk, L[C], Z[R]), WallN(P, Bk, L[C], Z[R]), WallCol(P, Bk, L[C], Z[R], 0));
					for (size_t K = 0; K < Cap.size(); ++K)
					{
						const CapRow& R = Cap[K];
						Col Cc = WallCol(P, Bk, L[C], CapZ(L[C], R), R.Part);
						// (the parapet's inner face in the roof's own light: a shade down, and the roof's dust at its foot)
						if (R.Part == 2) Cc = Mixed(Scaled(Cc, 0.95), P.Dust, R.Down < 0.0 ? 0.5 : 0.1);
						Id[C * NR + NZ + K] = B.Vert(CapPt(L[C], R), Norm(L[C].H * R.Nh + Up * R.Nu), Cc);
					}
				}
				for (size_t C = 0; C < N; ++C)
				{
					const size_t D = (C + 1) % N;
					if (Bk.bYard && Indoors(P, (Ring(Bk, L[C], 0.0) + Ring(Bk, L[D], 0.0)) * 0.5)) continue;
					const V3 Hm = Norm(L[C].H + L[D].H);
					for (size_t R = 0; R + 1 < NR; ++R)
					{
						const bool bWall = R + 1 < NZ;
						if (InGap(Bk, L[C], L[D], bWall ? Z[R] : 1.0, bWall ? Z[R + 1] : 1.0, bWall && bCut)) continue;
						V3 To = Hm;
						if (R + 1 >= NZ)
						{
							const CapRow& Ra = Cap[R + 1 - NZ];
							const double Nh = R >= NZ ? 0.5 * (Cap[R - NZ].Nh + Ra.Nh) : 0.5 * (1.0 + Ra.Nh), Nu = R >= NZ ? 0.5 * (Cap[R - NZ].Nu + Ra.Nu) : 0.5 * Ra.Nu;
							To = Hm * Nh + Up * Nu;
						}
						B.Tri(Id[C * NR + R], Id[D * NR + R], Id[D * NR + R + 1], To);
						B.Tri(Id[C * NR + R], Id[D * NR + R + 1], Id[C * NR + R + 1], To);
					}
				}
				for (const Gap& G : Bk.Gaps)
				{
					const Frame Fc = FaceOf(Bk, G.Face);
					if (G.What == 2)
					{
						// (the gate: the wall's two ends closed, through its thickness)
						for (int End = 0; End < 2; ++End)
						{
							const Column C = At(Bk, G.Face, G.U + (End == 0 ? -0.5 : 0.5) * G.W);
							const double Bot = Z.front(), Top = TopAt(Bk, C);
							const V3 Inw = Fc.T * (End == 0 ? 1.0 : -1.0);
							auto Pt = [&](double In, double Zc) { return Ring(Bk, C, Lean(P, Zc) + In) + V3{ 0.0, 0.0, Zc }; };
							const Col Lo = WallCol(P, Bk, C, 0.0, 0), Md = WallCol(P, Bk, C, 0.9, 0), Hi = Scaled(WallCol(P, Bk, C, Top - Rr, 0), 0.97);
							B.Quad(Pt(0.0, Bot), Pt(Bk.Tp, Bot), Pt(Bk.Tp, 0.9), Pt(0.0, 0.9), Lo, Lo, Md, Md, Inw);
							B.Quad(Pt(0.0, 0.9), Pt(Bk.Tp, 0.9), Pt(Bk.Tp, Top - Rr), Pt(0.0, Top - Rr), Md, Md, Hi, Hi, Inw);
							B.Quad(Pt(0.0, Top - Rr), Pt(Bk.Tp, Top - Rr), Pt(Bk.Tp - Rr, Top), Pt(Rr, Top), Hi, Hi, Hi, Hi, Inw);
						}
						continue;
					}
					if (!bCut || G.Look == 3) continue;
					// (an opening's reveal: the plaster turned in through the wall to its frame, its head in shadow)
					const double Dp = G.What == 1 ? 0.26 : 0.2;
					std::vector<const Column*> On;
					for (const Column& C : L)
						if (!C.bCorner && C.F == G.Face && C.U > G.U - 0.5 * G.W - 1e-6 && C.U < G.U + 0.5 * G.W + 1e-6) On.push_back(&C);
					std::vector<double> Zs;
					for (const double Zr : Z)
						if (Zr > G.Z0 - 1e-6 && Zr < G.Z1 + 1e-6) Zs.push_back(Zr);
					if (On.size() < 2 || Zs.size() < 2) continue;
					auto In = [&](const Column& C, double Zr) { return Ring(Bk, C, Lean(P, Zr) + Dp) + V3{ 0.0, 0.0, Zr }; };
					auto Wc = [&](const Column& C, double Zr, double K) { return Scaled(WallCol(P, Bk, C, Zr, 0), K); };
					for (size_t K = 0; K + 1 < On.size(); ++K)
					{
						const Column &Ca = *On[K], &Cb = *On[K + 1];
						B.Quad(WallPt(P, Bk, Ca, G.Z1), WallPt(P, Bk, Cb, G.Z1), In(Cb, G.Z1), In(Ca, G.Z1), Wc(Ca, G.Z1, 0.7), Wc(Cb, G.Z1, 0.7), Wc(Cb, G.Z1, 0.56), Wc(Ca, G.Z1, 0.56), V3{ 0.0, 0.0, -1.0 });
						B.Quad(WallPt(P, Bk, Ca, G.Z0), WallPt(P, Bk, Cb, G.Z0), In(Cb, G.Z0), In(Ca, G.Z0), Wc(Ca, G.Z0, 0.95), Wc(Cb, G.Z0, 0.95), Wc(Cb, G.Z0, 0.86), Wc(Ca, G.Z0, 0.86), Up);
					}
					for (int End = 0; End < 2; ++End)
					{
						const Column& C = End == 0 ? *On.front() : *On.back();
						const V3 Inw = Fc.T * (End == 0 ? 1.0 : -1.0);
						for (size_t K = 0; K + 1 < Zs.size(); ++K)
							B.Quad(WallPt(P, Bk, C, Zs[K]), WallPt(P, Bk, C, Zs[K + 1]), In(C, Zs[K + 1]), In(C, Zs[K]), Wc(C, Zs[K], 0.84), Wc(C, Zs[K + 1], 0.84), Wc(C, Zs[K + 1], 0.72), Wc(C, Zs[K], 0.72), Inw);
					}
				}
			}

			/** The roof's mud at a place on a block's deck: paler and darker where it was patched, the dust drifted against the
			 * parapet's foot. Edge: how far in from the parapet. */
			Col DeckCol(const Plan& P, const Block& Bk, double X, double Y, double Edge)
			{
				Col C = Scaled(P.Roof, 0.93 + 0.1 * Noise01(X / 0.9, Y / 0.9, Bk.Key ^ 0xE1));
				C = Mixed(C, Scaled(P.Roof, 0.82), 0.55 * Smooth(0.6, 0.72, Noise01(X / 1.3, Y / 1.1, Bk.Key ^ 0xE2)));
				return Mixed(C, P.Dust, 0.45 * Smooth(0.45, 0.0, Edge));
			}
			/** A block's roof: its deck inside the parapet, a little dished, its edge run in under the parapet's foot. */
			void Deck(Pen& B, const Plan& P, const Block& Bk, double Step)
			{
				const double In = Lean(P, Bk.Zr) + Bk.Tp - 0.04, Ax = Bk.Hx - In, Ay = Bk.Hy - In;
				const int NX = std::max(1, static_cast<int>(std::ceil(2.0 * Ax / Step))), NY = std::max(1, static_cast<int>(std::ceil(2.0 * Ay / Step)));
				std::vector<uint32_t> Id(static_cast<size_t>((NX + 1) * (NY + 1)));
				for (int I = 0; I <= NX; ++I)
					for (int J = 0; J <= NY; ++J)
					{
						const double X = -Ax + 2.0 * Ax * I / NX, Y = -Ay + 2.0 * Ay * J / NY, Edge = std::min(Ax - std::fabs(X), Ay - std::fabs(Y));
						const double Zd = Bk.Zr - 0.02 - 0.04 * Smooth(0.0, 1.2, Edge) + 0.012 * Noise(X / 0.7, Y / 0.7, Bk.Key ^ 0xE3) * Smooth(0.0, 0.3, Edge);
						Id[static_cast<size_t>(I * (NY + 1) + J)] = B.Vert(V3{ Bk.Cx + X, Bk.Cy + Y, Zd }, Up, DeckCol(P, Bk, Bk.Cx + X, Bk.Cy + Y, Edge));
					}
				for (int I = 0; I < NX; ++I)
					for (int J = 0; J < NY; ++J)
					{
						const size_t A = static_cast<size_t>(I * (NY + 1) + J), C = static_cast<size_t>((I + 1) * (NY + 1) + J);
						B.Tri(Id[A], Id[C], Id[C + 1], Up);
						B.Tri(Id[A], Id[C + 1], Id[A + 1], Up);
					}
			}

			/** The whole mirrored end for end (x for -x), its triangles turned back to face out. */
			void MirrorX(LandField::Mesh& M)
			{
				for (size_t I = 0; I + 2 < M.Positions.size(); I += 3)
				{
					M.Positions[I] = -M.Positions[I];
					M.Normals[I] = -M.Normals[I];
				}
				for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3) std::swap(M.Triangles[T + 1], M.Triangles[T + 2]);
			}

			// ---------------------------------------------------------------- what is laid on the walls

			/** A place on a block's wall as it is drawn: along a face, up it, Off out from the plaster. */
			V3 OnWall(const Plan& P, const Block& Bk, int F, double U, double Z, double Off) { return WallPt(P, Bk, At(Bk, F, U), Z, Off); }
			/** A place in an opening: In inside the wall's true line as it stands at the height Zl (plumb: a door's leaf does not
			 * lean with its wall). */
			V3 InWall(const Plan& P, const Block& Bk, int F, double U, double Z, double Zl, double In) { return Ring(Bk, At(Bk, F, U), Lean(P, Zl) + In) + V3{ 0.0, 0.0, Z }; }

			/** A patch laid on a wall, Off out from its plaster and following its waves: from Ua to Ub along the face and Za to Zb
			 * up it, in cells no wider than Cell; its colour from Of(U, Z). */
			template <typename Fn> void Decal(Pen& B, const Plan& P, const Block& Bk, int F, double Ua, double Ub, double Za, double Zb, double Off, double Cell, Fn Of)
			{
				const int NU = std::max(1, static_cast<int>(std::ceil((Ub - Ua) / Cell))), NZ = std::max(1, static_cast<int>(std::ceil((Zb - Za) / (1.35 * Cell))));
				const V3 Out = FaceOf(Bk, F).N;
				std::vector<uint32_t> Id(static_cast<size_t>((NU + 1) * (NZ + 1)));
				for (int I = 0; I <= NU; ++I)
					for (int J = 0; J <= NZ; ++J)
					{
						const double U = Lerp(Ua, Ub, static_cast<double>(I) / NU), Z = Lerp(Za, Zb, static_cast<double>(J) / NZ);
						Id[static_cast<size_t>(I * (NZ + 1) + J)] = B.Vert(OnWall(P, Bk, F, U, Z, Off), WallN(P, Bk, At(Bk, F, U), Z), Of(U, Z));
					}
				for (int I = 0; I < NU; ++I)
					for (int J = 0; J < NZ; ++J)
					{
						const size_t A = static_cast<size_t>(I * (NZ + 1) + J), C = static_cast<size_t>((I + 1) * (NZ + 1) + J);
						B.Tri(Id[A], Id[C], Id[C + 1], Out);
						B.Tri(Id[A], Id[C + 1], Id[A + 1], Out);
					}
			}

			// ---------------------------------------------------------------- where the plaster has come away

			/** How far it is round a block's walls. */
			double Round(const Block& Bk)
			{
				double Pm = 0.0;
				for (int F = 0; F < 4; ++F) Pm += 2.0 * FaceOf(Bk, F).Half + 0.5 * Pi * Bk.Rc;
				return Pm;
			}
			/** A place round a block's walls, as a column: on a face's straight, or round the corner after it. */
			Column Along(const Block& Bk, double Pm)
			{
				const double All = Round(Bk), Arc = 0.5 * Pi * Bk.Rc;
				double From = 0.0, Left = Pm - All * std::floor(Pm / All);
				const int Order[4] = { 3, 0, 1, 2 };
				for (int I = 0; I < 4; ++I)
				{
					const Frame Fc = FaceOf(Bk, Order[I]);
					if (Left <= 2.0 * Fc.Half) return { Order[I], false, Left - Fc.Half, Fc.N, From + Left };
					Left -= 2.0 * Fc.Half;
					From += 2.0 * Fc.Half;
					if (Left < Arc)
					{
						const double A = 0.5 * Pi * (I + Left / Arc);
						return { Order[I], true, A, V3{ std::cos(A), std::sin(A), 0.0 }, From + Left };
					}
					Left -= Arc;
					From += Arc;
				}
				return At(Bk, 3, -FaceOf(Bk, 3).Half);
			}
			/** A place on a block's wall as its shell is drawn, Off out from it: the wall's own line up to where its rows end
			 * (its roof's deck; the yard's wall: 0.4 under its top), and over that the one strip from there to under the
			 * parapet's roll. */
			V3 Skin(const Plan& P, const Block& Bk, const Column& C, double Z, double Off)
			{
				const double Zm = Bk.bYard ? Bk.Zp - 0.4 : Bk.Zr;
				if (Z <= Zm) return WallPt(P, Bk, C, Z, Off);
				const double Zt = TopAt(Bk, C) - (Bk.bYard ? 0.1 : 0.08);
				const V3 A = WallPt(P, Bk, C, Zm, Off), E = WallPt(P, Bk, C, Zt, Off);
				return A + (E - A) * Clamp01((Z - Zm) / std::max(1e-6, Zt - Zm));
			}

			/** How far up a patch's bare bricks reach at a place round the walls: from Lo to Hi (none where Hi is not over Lo). */
			struct Bare
			{
				double Lo = 0.0, Hi = 0.0;
			};
			/** How far in from a patch's ends a place is, of the stretch each end takes to run out, each its own (0 at an end,
			 * 1 clear of both); an end cut square against a room's wall takes none. A foot's ends take a metre or so each,
			 * never over a third of its length. */
			double EndsOf(const Plan::Scab& S, double Pm)
			{
				const double Slow = S.Hang == 2 ? 1.7 : 0.7, Most = 0.35 * (S.Pm1 - S.Pm0), Ua = Unit(Hash(S.Seed, 0x41)), Ub = Unit(Hash(S.Seed, 0x42));
				const double Ra = S.Hang == 0 ? std::min(Most, 1.0 + 0.4 * Ua) : Slow * (0.28 + 0.3 * Ua), Rb = S.Hang == 0 ? std::min(Most, 1.0 + 0.4 * Ub) : Slow * (0.28 + 0.3 * Ub);
				const double Ea = S.bCutA ? 1.0 : Smooth(0.0, Ra, Pm - S.Pm0), Eb = S.bCutB ? 1.0 : Smooth(0.0, Rb, S.Pm1 - Pm);
				return std::min(Ea, Eb);
			}
			Bare BareOf(const Plan& P, const Block& Bk, const Plan::Scab& S, double Pm)
			{
				const double Top = TopAt(Bk, Along(Bk, Pm)) - (Bk.bYard ? 0.1 : 0.08) - 0.014;
				const double X = (Pm - S.Pm0) / std::max(1e-6, S.Pm1 - S.Pm0);
				const double E = EndsOf(S, Pm);
				const double Rag = 0.03 * Noise(Pm / 0.17, 1.5, S.Seed ^ 0x22) + 0.012 * Noise(Pm / 0.05, 2.5, S.Seed ^ 0x23);
				const double Lump = Noise01(Pm / 0.5, 0.5, S.Seed ^ 0x21);
				switch (S.Hang)
				{
				case 0:
				{
					// (a foot: a long low band; its top uneven, in steps where the plaster broke off along a course; its
					// ends run down to the ground with no step in them, and on under the floor's level where the ground lies
					// lower; Bricks thins it away into the wall's coat along them, so from far off it is no box against the
					// wall)
					const double Rise = E * (0.05 + S.Tall * (0.5 + 0.5 * Noise01(Pm / 0.95, 0.5, S.Seed ^ 0x24)));
					const double Up = -0.05 + Lerp(Rise, 0.118 * std::round(Rise / 0.118), 0.65 * Smooth(0.8, 1.0, E)) + 0.3 * E * Rag;
					const double Deep = std::max(-std::max(0.25, P.S.Below), -0.72);
					return { Deep, Lerp(Deep, Up, Smooth(0.0, 0.3, E)) };
				}
				case 1:
				{
					// (down from under the parapet: deepest where it will, in two or three tongues, its lower edge ragged)
					const double Skew = 0.8 * (Unit(Hash(S.Seed, 0x43)) - 0.5), Tongue = 0.3 + 0.7 * Noise01(Pm / 0.3, 2.5, S.Seed ^ 0x24);
					const double Deep = S.Tall * std::pow(E, 0.45) * (0.4 + 0.6 * Lump) * Tongue * (1.0 + Skew * (2.0 * X - 1.0)) + E * Rag;
					return { Top - std::max(0.0, Deep), Top };
				}
				default:
				{
					// (never plastered above the waist: from a wavy line to under the wall's top, the line climbing to the top
					// where the plaster was carried up a gate's post)
					const double Waist = S.ZAt + 0.07 * Noise(Pm / 0.9, 0.5, S.Seed ^ 0x21) + 0.035 * Noise(Pm / 0.28, 1.5, S.Seed ^ 0x22) + 0.01 * Noise(Pm / 0.07, 2.5, S.Seed ^ 0x23);
					return { Lerp(Top, Waist, E), Top };
				}
				}
			}

			/** A patch's bare bricks. The mud mortar they lie in, laid on the wall from the patch's lower edge to its upper: no
			 * line drawn round it, the plaster's thickness showing only as its shadow along the upper edge and, where the lower
			 * edge is the plaster's own, its broken lip in the light there. On it the bricks of the wall's own courses, which
			 * run on under the plaster on every side and are cut by its edge: each its own tone and none far from the rest,
			 * their joints a shade darker, some worn round; a foot's lowest course eaten hollow by the damp. From far off (not
			 * bFine): its tone alone. */
			void Bricks(Pen& B, const Plan& P, const Plan::Scab& S, bool bFine)
			{
				const Block& Bk = BlockOf(P, S.Blk);
				const double Off = bFine ? (S.Hang == 0 ? 0.013 : 0.01) : 0.05, Zm = Bk.bYard ? Bk.Zp - 0.4 : Bk.Zr, SkimMost = 0.5, Under = 0.8;
				// (under a limewashed wall the damp leaves its salts on the bricks it bared: pale, within a step of the wall's
				// own value from across the street; the bare mud house's are near its wall's already)
				const double Pale = P.bMud ? 0.0 : 0.4;
				const Col Salt = Mixed(P.Dust, P.Wash, 0.35);
				// (its two edges lie down onto the plaster: nothing of it stands off the wall to throw a line of shade below it)
				const double Flat = bFine ? 0.15 * Off : Off;
				const Col Joint = Scaled(P.Brick, 0.86), Lip = P.bMud ? Scaled(P.Mud, 1.08) : Mixed(P.Wash, P.Mud, 0.5);
				// Its stations round the wall (from far off: few, and none on a corner's round, which is one flat there).
				std::vector<double> St;
				{
					const double Step = bFine ? (S.Hang == 2 ? 0.13 : S.Hang == 0 ? 0.085 : 0.06) : 0.9;
					const int N = std::max(2, static_cast<int>(std::ceil((S.Pm1 - S.Pm0) / Step)));
					for (int K = 0; K <= N; ++K) St.push_back(Lerp(S.Pm0, S.Pm1, static_cast<double>(K) / N));
					if (!bFine)
					{
						double E0 = 0.0;
						const int Order[4] = { 3, 0, 1, 2 };
						for (int I = 0; I < 4; ++I)
						{
							E0 += 2.0 * FaceOf(Bk, Order[I]).Half;
							const double E1 = E0 + 0.5 * Pi * Bk.Rc;
							St.erase(std::remove_if(St.begin(), St.end(), [&](double Pm) { return Pm > E0 - 1e-6 && Pm < E1 + 1e-6; }), St.end());
							if (S.Pm0 < E0 && S.Pm1 > E0) St.push_back(E0 - 1e-6);
							if (S.Pm0 < E1 && S.Pm1 > E1) St.push_back(E1 + 1e-6);
							E0 = E1;
						}
						std::sort(St.begin(), St.end());
					}
				}
				// Its rows up each station: the plaster's lip along a lower edge of its own, the mortar, the shadow under the
				// upper edge; a foot's from under the ground, its hollow lowest course.
				struct Row
				{
					int From; // (0: so far up from its lower edge; 1: so far under its upper; 2: so much of the way between; 3: at that height)
					double D;
					int Tone; // (0 the mortar; 1 the shadow under the plaster's edge; 2 the plaster's lip; 3 a foot's hollow, 4 its shadowed top)
					bool bEdge; // (an edge of the patch: down on the plaster)
				};
				std::vector<Row> Rw;
				if (!bFine) Rw = { { 0, 0.0, 0, false }, { 1, 0.0, 0, false } };
				else
				{
					if (S.Hang == 0) Rw = { { 0, 0.0, 3, false }, { 3, 0.0, 3, false }, { 3, 0.1, 4, false }, { 3, 0.128, 0, false } };
					else Rw = { { 0, 0.0, 2, true }, { 0, 0.018, 2, false }, { 0, 0.023, 0, false } };
					const int NB = std::max(1, static_cast<int>(std::ceil(S.Tall / 0.16)));
					for (int J = 1; J < NB; ++J) Rw.push_back({ 2, static_cast<double>(J) / NB, 0, false });
					Rw.push_back({ 1, 0.02, 0, false });
					Rw.push_back({ 1, 0.011, 1, false });
					Rw.push_back({ 1, 0.0, 1, true });
				}
				const size_t NS = St.size(), NR = Rw.size();
				std::vector<uint32_t> Id(NS * NR);
				std::vector<double> Zs(NS * NR);
				double Zlo = 1e9, Zhi = -1e9;
				for (size_t I = 0; I < NS; ++I)
				{
					const Column C = Along(Bk, St[I]);
					Bare R = BareOf(P, Bk, S, St[I]);
					if (R.Hi > R.Lo)
					{
						Zlo = std::min(Zlo, R.Lo);
						Zhi = std::max(Zhi, R.Hi);
					}
					else R.Lo = R.Hi = 0.5 * (R.Lo + R.Hi);
					const double LoB = S.Hang == 0 ? std::min(0.128, R.Hi) : std::min(R.Lo + 0.023, R.Hi), HiB = std::max(LoB, R.Hi - 0.02);
					// (a foot's upper half still skimmed with what is left of the wall's own coat, more of it the higher: its
					// top within a step of the wall's value, its bricks bare only low down)
					const Col Coat = S.Hang == 0 ? WallCol(P, Bk, C, std::max(0.0, R.Hi) + 0.03, 0) : P.Mud, Skim = Scaled(Coat, Under);
					const double Thin = S.Hang == 0 ? Smooth(0.0, 0.7, EndsOf(S, St[I])) : 1.0;
					double Was = R.Lo;
					for (size_t J = 0; J < NR; ++J)
					{
						const Row& Q = Rw[J];
						const double Zq = Q.From == 0 ? R.Lo + Q.D : Q.From == 1 ? R.Hi - Q.D : Q.From == 2 ? Lerp(LoB, HiB, Q.D) : Q.D;
						const double Z = std::max(Was, std::clamp(Zq, R.Lo, R.Hi));
						Was = Z;
						const double Tone = Noise01(St[I] / 0.45, Z / 0.35, S.Seed ^ 0x12);
						Col Cc = !bFine ? Scaled(Mixed(P.Brick, Joint, 0.35), 0.95 + 0.08 * Tone) : Q.Tone == 1 ? Scaled(Joint, S.Hang == 0 ? 0.95 : 0.85) : Q.Tone == 2 ? Lip : Scaled(Joint, (Q.Tone == 3 ? 0.84 : Q.Tone == 4 ? 0.7 : 0.94) + 0.1 * Tone);
						// (the ground's dust on what stands in it)
						Cc = Mixed(Cc, P.Dust, (S.Hang == 0 ? 0.54 : 0.42) * Smooth(0.3, 0.0, Z));
						// (and along its ends the coat not yet gone, thinner toward the middle)
						if (S.Hang == 0) Cc = Mixed(Coat, Mixed(Mixed(Cc, Salt, Pale), Skim, SkimMost * Smooth(0.3, 1.0, Z / std::max(0.06, R.Hi))), Thin);
						Zs[I * NR + J] = Z;
						Id[I * NR + J] = B.Vert(Skin(P, Bk, C, Z, Q.bEdge ? Flat : Off), WallN(P, Bk, C, std::min(Z, Zm)), Cc);
					}
				}
				for (size_t I = 0; I + 1 < NS; ++I)
				{
					const V3 Out = Norm(Along(Bk, St[I]).H + Along(Bk, St[I + 1]).H);
					for (size_t J = 0; J + 1 < NR; ++J)
					{
						const size_t A = I * NR + J, C = (I + 1) * NR + J;
						if (Zs[A + 1] - Zs[A] < 1e-5 && Zs[C + 1] - Zs[C] < 1e-5) continue;
						B.Tri(Id[A], Id[C], Id[C + 1], Out);
						B.Tri(Id[A], Id[C + 1], Id[A + 1], Out);
					}
				}
				if (!bFine || Zhi <= Zlo) return;
				// The bricks: the wall's own courses, wandering a little along it, the same under every patch on it; each
				// brick's ends where its own course breaks joint.
				const double Ch = 0.118, Bl = 0.3, Lift = Off + 0.006, EdgeLo = S.Hang == 0 ? 0.0 : 0.026, EdgeHi = 0.016;
				auto Wander = [&](double Pm) { return 0.012 * Noise(Pm / 1.7, 0.3, Bk.Key ^ 0xB1C); };
				auto JointAt = [&](int K, int I) {
					return (I + ((K & 1) != 0 ? 0.5 : 0.0) + 0.1 * (Unit(Hash(Bk.Key ^ 0xB1D, static_cast<uint64_t>(K + 4096), static_cast<uint64_t>(I + 65536))) - 0.5)) * Bl;
				};
				// (a foot's brick: the wall's coat it is skimmed toward, and the band's top where it stands)
				Col SkimTo = P.Mud, SkimUnder = P.Mud;
				double SkimHi = 1.0;
				auto Put = [&](double Pm, double Z, const Col& C) {
					const Column Cn = Along(Bk, Pm);
					return B.Vert(Skin(P, Bk, Cn, Z, Lift), WallN(P, Bk, Cn, std::min(Z, Zm)), S.Hang == 0 ? Mixed(SkimTo, Mixed(Mixed(C, Salt, Pale), SkimUnder, SkimMost * Smooth(0.3, 1.0, Z / SkimHi)), Smooth(0.0, 0.7, EndsOf(S, Pm))) : C);
				};
				const int K0 = static_cast<int>(std::floor((Zlo - 0.02) / Ch)), K1 = static_cast<int>(std::ceil((Zhi + 0.02) / Ch));
				const int I0 = static_cast<int>(std::floor(S.Pm0 / Bl)) - 1, I1 = static_cast<int>(std::ceil(S.Pm1 / Bl)) + 1;
				for (int K = K0; K < K1; ++K)
					for (int I = I0; I < I1; ++I)
					{
						const double Pa = std::max(S.Pm0, JointAt(K, I) + 0.006), Pb = std::min(S.Pm1, JointAt(K, I + 1) - 0.006);
						if (Pb - Pa < 0.03) continue;
						const uint64_t Kb = Hash(Bk.Key ^ 0xB1E, static_cast<uint64_t>(K + 4096), static_cast<uint64_t>(I + 65536));
						// (a foot's lowest course, and what is under the ground: eaten back, in its own shade; one here and there gone)
						const bool bHollow = S.Hang == 0 && K <= 0;
						if (bHollow && Unit(Hash(Kb, 4)) < 0.2) continue;
						const double Light = (bHollow ? 0.93 : 1.0) * (0.96 + 0.08 * Unit(Hash(Kb, 1))), In = bHollow ? 0.008 : 0.0;
						// (a foot's bricks dusty as the plaster beside them is)
						const Col Cb = Mixed(Varied(P.Brick, Light, 0.03 * (Unit(Hash(Kb, 2)) - 0.5)), P.Dust, S.Hang == 0 ? 0.3 : 0.0), Cl = Scaled(Cb, 0.95);
						// (its lower and upper edges at five places along it, cut by the patch's own)
						double Pm[5], Za[5], Zb[5], A0[5], B0[5];
						bool bWhole = true, bAny = false;
						for (int Q = 0; Q < 5; ++Q)
						{
							Pm[Q] = Lerp(Pa, Pb, 0.25 * Q);
							const Bare R = BareOf(P, Bk, S, Pm[Q]);
							A0[Q] = K * Ch + Wander(Pm[Q]) + 0.005 + In;
							B0[Q] = (K + 1) * Ch + Wander(Pm[Q]) - 0.006 - 2.0 * In;
							Za[Q] = std::max(A0[Q], R.Lo + EdgeLo);
							Zb[Q] = std::min(B0[Q], R.Hi - EdgeHi);
							if (Za[Q] > A0[Q] || Zb[Q] < B0[Q]) bWhole = false;
							if (Zb[Q] - Za[Q] > 0.012) bAny = true;
							else if (Zb[Q] <= Za[Q]) Za[Q] = Zb[Q] = std::clamp(0.5 * (Za[Q] + Zb[Q]), A0[Q], B0[Q]);
						}
						if (!bAny) continue;
						const V3 Out = Along(Bk, Pm[2]).H;
						if (S.Hang == 0)
						{
							SkimHi = std::max(0.06, BareOf(P, Bk, S, Pm[2]).Hi);
							SkimTo = WallCol(P, Bk, Along(Bk, Pm[2]), SkimHi + 0.03, 0);
							SkimUnder = Scaled(SkimTo, Under);
						}
						if (bWhole && Unit(Hash(Kb, 3)) < 0.14)
						{
							// (worn: its four corners gone)
							const double Cut = 0.007 + 0.009 * Unit(Hash(Kb, 5));
							const uint32_t Mid = Put(Pm[2], 0.5 * (A0[2] + B0[2]), Scaled(Cb, 0.975));
							const uint32_t Rim[10] = {
								Put(Pa + Cut, A0[0], Cl), Put(Pm[2], A0[2], Cl), Put(Pb - Cut, A0[4], Cl), Put(Pb, A0[4] + Cut, Cl), Put(Pb, B0[4] - Cut, Cb),
								Put(Pb - Cut, B0[4], Cb), Put(Pm[2], B0[2], Cb), Put(Pa + Cut, B0[0], Cb), Put(Pa, B0[0] - Cut, Cb), Put(Pa, A0[0] + Cut, Cl) };
							for (int Q = 0; Q < 10; ++Q) B.Tri(Mid, Rim[Q], Rim[(Q + 1) % 10], Out);
							continue;
						}
						const int Stp = bWhole ? 2 : 1;
						for (int Q = 0; Q + Stp < 5; Q += Stp)
						{
							if (Zb[Q] - Za[Q] < 1e-4 && Zb[Q + Stp] - Za[Q + Stp] < 1e-4) continue;
							const uint32_t Va = Put(Pm[Q], Za[Q], Cl), Vb = Put(Pm[Q + Stp], Za[Q + Stp], Cl), Vc = Put(Pm[Q + Stp], Zb[Q + Stp], Cb), Vd = Put(Pm[Q], Zb[Q], Cb);
							B.Tri(Va, Vb, Vc, Out);
							B.Tri(Va, Vc, Vd, Out);
						}
					}
			}

			/** Under a patch high on a street wall, what fell from it: a low heap against the wall's foot, run down into the
			 * ground, the plaster's crumbs lying on it. */
			void Fallen(Pen& B, const Plan& P, const Plan::Scab& S)
			{
				const Block& Bk = BlockOf(P, S.Blk);
				const double Len = std::clamp(0.8 * (S.Pm1 - S.Pm0), 0.6, 1.5), Pc = 0.5 * (S.Pm0 + S.Pm1) + 0.2 * (Unit(Hash(S.Seed, 0xF1)) - 0.5);
				// (the mud of the plaster, crumbled, whatever the wash over it was; a few flakes of that wash still whole)
				const Col Heap = Mixed(Scaled(P.Mud, 0.93), P.Dust, P.bMud ? 0.3 : 0.0), Chip = P.bMud ? Scaled(P.Mud, 1.05) : Mixed(P.Wash, P.Dust, 0.15);
				// (the heap's top at a place along it and so far out from the wall's foot)
				auto Bump = [&](double Pm) { return std::pow(std::max(0.0, std::sin(Pi * Clamp01((Pm - Pc) / Len + 0.5))), 0.6) * (0.75 + 0.25 * Noise01(Pm / 0.2, 0.5, S.Seed ^ 0xF2)); };
				auto Reach = [&](double Pm) { return 0.05 + 0.2 * Bump(Pm) * (0.55 + 0.45 * Noise01(Pm / 0.16, 1.5, S.Seed ^ 0xF5)); };
				auto High = [&](double Pm, double Out) {
					const double Bm = Bump(Pm), Rm = Reach(Pm), Zw = -0.07 + 0.15 * Bm, Zr = -0.07 + 0.09 * Bm;
					return Out < Rm ? Lerp(Zw, Zr, Out / Rm) : Lerp(Zr, -0.32, Clamp01((Out - Rm) / 0.3));
				};
				auto At3 = [&](double Pm, double Out, double Up3) { return Ring(Bk, Along(Bk, Pm), -0.02 - std::max(0.0, Out)) + V3{ 0.0, 0.0, High(Pm, std::max(0.0, Out)) + Up3 }; };
				const int N = 12;
				uint32_t Was[3] = { 0, 0, 0 };
				for (int I = 0; I <= N; ++I)
				{
					const double Pm = Pc + Len * (static_cast<double>(I) / N - 0.5), Rm = Reach(Pm);
					const V3 H = Along(Bk, Pm).H;
					const uint32_t Now[3] = { B.Vert(At3(Pm, 0.0, 0.0), Norm(H + Up), Heap), B.Vert(At3(Pm, Rm, 0.0), Norm(H * 0.5 + Up), Scaled(Heap, 0.96)), B.Vert(At3(Pm, Rm + 0.3, 0.0), Norm(H + Up), Mixed(Heap, P.Dust, 0.5)) };
					if (I > 0)
						for (int J = 0; J < 2; ++J)
						{
							B.Tri(Was[J], Now[J], Now[J + 1], Up);
							B.Tri(Was[J], Now[J + 1], Was[J + 1], Up);
						}
					for (int J = 0; J < 3; ++J) Was[J] = Now[J];
				}
				// (the crumbs: flakes of the plaster, each tipped its own way)
				const int NC = 7 + static_cast<int>(4.0 * Unit(Hash(S.Seed, 0xF3)));
				for (int K = 0; K < NC; ++K)
				{
					const uint64_t Kc = Hash(S.Seed, 0xF40 + static_cast<uint64_t>(K));
					const double Pm = Pc + 0.8 * Len * (Unit(Hash(Kc, 1)) - 0.5), Out = 0.03 + 0.8 * Reach(Pm) * Unit(Hash(Kc, 2)), Sz = 0.022 + 0.03 * Unit(Hash(Kc, 3)), Turn = Pi * Unit(Hash(Kc, 4));
					const double Ca = Sz * std::cos(Turn), Sa = Sz * std::sin(Turn), Tip = 0.012 + 0.02 * Unit(Hash(Kc, 5));
					const Col C = Scaled(Chip, 0.9 + 0.14 * Unit(Hash(Kc, 6)));
					B.Quad(At3(Pm - Ca, Out - Sa, 0.006), At3(Pm + Sa, Out - Ca, 0.006 + Tip), At3(Pm + Ca, Out + Sa, 0.008), At3(Pm - Sa, Out + Ca, 0.006), C, C, Scaled(C, 0.92), Scaled(C, 0.92), Up);
				}
			}

			/** A crack up a wall's plaster: a dark line wandering as it climbs, widest at its middle. */
			void CrackLine(Pen& B, const Plan& P, const Plan::Crack& Ck)
			{
				const Block& Bk = BlockOf(P, Ck.Blk);
				const V3 Out = FaceOf(Bk, Ck.F).N;
				const int N = std::max(3, static_cast<int>(std::ceil((Ck.Z1 - Ck.Z0) / 0.1)));
				const double Drift = 0.02 * (Unit(Hash(Ck.Seed, 0x99)) - 0.5);
				double U = Ck.U;
				V3 La, Lb;
				Col Lc;
				for (int K = 0; K <= N; ++K)
				{
					const uint64_t Kk = Hash(Ck.Seed, static_cast<uint64_t>(K));
					const double Z = Lerp(Ck.Z0, Ck.Z1, static_cast<double>(K) / N), Wd = 0.002 + 0.006 * std::pow(std::sin(Pi * K / N), 0.7);
					// (it wanders a little at each step, and now and then steps aside)
					if (K > 0) U += Drift + 0.045 * (Unit(Hash(Kk, 1)) - 0.5) + (Unit(Hash(Kk, 2)) < 0.14 ? 0.09 * (Unit(Hash(Kk, 3)) - 0.5) : 0.0);
					const V3 A = OnWall(P, Bk, Ck.F, U - Wd, Z, 0.007), C = OnWall(P, Bk, Ck.F, U + Wd, Z, 0.007);
					const Col Cc = Scaled(WallCol(P, Bk, At(Bk, Ck.F, U), Z, 0), 0.4);
					if (K > 0) B.Quad(La, Lb, C, A, Lc, Lc, Cc, Cc, Out);
					La = A;
					Lb = C;
					Lc = Cc;
				}
			}

			/** The timber over an opening's head, its ends in the wall either side, its face a little proud of the plaster. */
			void Lintel(Pen& B, const Plan& P, const Block& Bk, const Gap& G, double Past, double Hh, uint64_t Seed)
			{
				const Frame Fc = FaceOf(Bk, G.Face);
				const double Z = G.Z1 + Hh + 0.004;
				const Col C = Varied(Mixed(P.Wood, Rgb(150, 140, 126), 0.4), 0.86 + 0.18 * Unit(Hash(Seed, 0x60)), 0.02);
				B.Box(InWall(P, Bk, G.Face, G.U, Z, Z, 0.03), Fc.T * (0.5 * G.W + Past), Fc.N * 0.068, Up * Hh, C, 4);
			}

			/** A door's leaf in its opening: ledged planks in a rough frame, a nail's head through each plank at each ledge, a
			 * bolt; its paint chalked, worn to the wood toward its foot (bBare: never painted). A timber over its head (but under
			 * an arch: not bLintel), a stone at its foot. */
			void Door(Pen& B, const Plan& P, const Block& Bk, const Gap& G, uint64_t Seed, bool bBare, bool bLintel = true)
			{
				const Frame Fc = FaceOf(Bk, G.Face);
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Fw = 0.06;
				auto Pt = [&](double U, double Z, double In) { return InWall(P, Bk, G.Face, U, Z, G.Z1, In); };
				const Col Paint = bBare ? Varied(P.Wood, 0.92 + 0.12 * Unit(Hash(Seed, 1)), 0.03) : P.Paint, Bare = Mixed(P.Wood, P.Dust, 0.3), Dark = Rgb(30, 26, 24);
				const Col Post = bBare ? Scaled(Paint, 0.9) : Mixed(Scaled(P.Paint, 0.88), P.Wood, 0.25);
				// (its frame: two jambs and a head)
				B.Box(Pt(U0 + 0.5 * Fw, 0.5 * (G.Z0 + G.Z1), 0.15), Fc.T * (0.5 * Fw), Fc.N * 0.045, Up * (0.5 * (G.Z1 - G.Z0)), Post, 4 | 16 | 32);
				B.Box(Pt(U1 - 0.5 * Fw, 0.5 * (G.Z0 + G.Z1), 0.15), Fc.T * (0.5 * Fw), Fc.N * 0.045, Up * (0.5 * (G.Z1 - G.Z0)), Post, 4 | 16 | 32);
				B.Box(Pt(G.U, G.Z1 - 0.5 * Fw, 0.15), Fc.T * (0.5 * G.W - Fw), Fc.N * 0.045, Up * (0.5 * Fw), Post, 1 | 2 | 4 | 32);
				// (its planks, the dark behind their joints)
				const double L0 = U0 + Fw, L1 = U1 - Fw, Zb = G.Z0 + 0.012, Zt = G.Z1 - Fw;
				B.Quad1(Pt(L0, Zb, 0.2), Pt(L1, Zb, 0.2), Pt(L1, Zt, 0.2), Pt(L0, Zt, 0.2), Dark, Fc.N);
				const int Np = std::max(3, static_cast<int>(std::lround((L1 - L0) / 0.17)));
				for (int K = 0; K < Np; ++K)
				{
					const uint64_t Kp = Hash(Seed, 0x40 + static_cast<uint64_t>(K));
					const double Ua = L0 + (L1 - L0) * K / Np + 0.004, Ub = L0 + (L1 - L0) * (K + 1) / Np - 0.004;
					const double Light = 0.9 + 0.16 * Unit(Hash(Kp, 1)), Zw = G.Z0 + 0.28 + 0.3 * Unit(Hash(Kp, 2));
					const Col Mid = Scaled(Paint, Light), Low = Mixed(Mid, Bare, bBare ? 0.3 : 0.75), High = Scaled(Mid, 0.9);
					B.Quad(Pt(Ua, Zb, 0.178), Pt(Ub, Zb, 0.178), Pt(Ub, Zw, 0.178), Pt(Ua, Zw, 0.178), Low, Low, Mid, Mid, Fc.N);
					B.Quad(Pt(Ua, Zw, 0.178), Pt(Ub, Zw, 0.178), Pt(Ub, Zt, 0.178), Pt(Ua, Zt, 0.178), Mid, Mid, High, High, Fc.N);
				}
				for (int K = 0; K < 3; ++K)
				{
					const double Z = K == 0 ? G.Z0 + 0.24 : K == 1 ? 0.5 * (G.Z0 + Zt) : Zt - 0.22;
					B.Box(Pt(G.U, Z, 0.163), Fc.T * (0.5 * (L1 - L0) - 0.012), Fc.N * 0.015, Up * 0.045, Scaled(Paint, K == 0 ? 0.8 : 0.92), 4);
					for (int J = 0; J < Np; ++J)
					{
						const double Un = L0 + (L1 - L0) * (J + 0.5) / Np;
						B.Quad1(Pt(Un - 0.011, Z - 0.011, 0.145), Pt(Un + 0.011, Z - 0.011, 0.145), Pt(Un + 0.011, Z + 0.011, 0.145), Pt(Un - 0.011, Z + 0.011, 0.145), Rgb(52, 46, 42), Fc.N);
					}
				}
				B.Box(Pt(L1 - 0.12, G.Z0 + 1.24, 0.166), Fc.T * 0.07, Fc.N * 0.012, Up * 0.02, Rgb(60, 52, 46), 4);
				// (the stone at its foot; the timber over its head)
				const Col Stone = Mixed(P.Dust, Rgb(168, 160, 148), 0.5), StoneTop = Scaled(Stone, 1.06);
				B.Box(Pt(G.U, G.Z0 - 0.12, -0.03), Fc.T * (0.5 * G.W + 0.07), Fc.N * 0.24, Up * 0.12, Stone, 4 | 16, &StoneTop);
				if (bLintel) Lintel(B, P, Bk, G, 0.17, 0.065, Seed);
			}

			/** A window: a board over its head; a plank shutter shut in it, or iron bars through a flat cross bar and the room
			 * dark behind. */
			void Window(Pen& B, const Plan& P, const Block& Bk, const Gap& G, uint64_t Seed)
			{
				const Frame Fc = FaceOf(Bk, G.Face);
				const double U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W;
				auto Pt = [&](double U, double Z, double In) { return InWall(P, Bk, G.Face, U, Z, G.Z1, In); };
				const Col Dark = Rgb(26, 23, 21);
				Lintel(B, P, Bk, G, 0.12, 0.045, Seed);
				if (G.Look == 1)
				{
					B.Quad1(Pt(U0, G.Z0, 0.17), Pt(U1, G.Z0, 0.17), Pt(U1, G.Z1, 0.17), Pt(U0, G.Z1, 0.17), Dark, Fc.N);
					const Col Iron = Varied(Rgb(70, 58, 50), 0.9 + 0.2 * Unit(Hash(Seed, 1)), 0.06);
					const int Nb = std::max(2, static_cast<int>(std::lround(G.W / 0.14)) - 1);
					for (int K = 0; K < Nb; ++K)
					{
						const double U = U0 + G.W * (K + 1.0) / (Nb + 1.0);
						B.Beam(Pt(U, G.Z0 - 0.02, 0.07), Pt(U, G.Z1 + 0.02, 0.07), 0.02, 0.02, Iron, Fc.N);
					}
					B.Beam(Pt(U0 - 0.02, 0.5 * (G.Z0 + G.Z1), 0.062), Pt(U1 + 0.02, 0.5 * (G.Z0 + G.Z1), 0.062), 0.03, 0.008, Iron, Fc.N);
					return;
				}
				B.Quad1(Pt(U0, G.Z0, 0.13), Pt(U1, G.Z0, 0.13), Pt(U1, G.Z1, 0.13), Pt(U0, G.Z1, 0.13), Dark, Fc.N);
				const int Np = std::max(2, static_cast<int>(std::lround(G.W / 0.17)));
				for (int K = 0; K < Np; ++K)
				{
					const uint64_t Kp = Hash(Seed, 0x40 + static_cast<uint64_t>(K));
					const double Ua = U0 + G.W * K / Np + 0.004, Ub = U0 + G.W * (K + 1) / Np - 0.004;
					const Col Mid = Scaled(P.Paint, 0.88 + 0.18 * Unit(Hash(Kp, 1))), Low = Mixed(Mid, Mixed(P.Wood, P.Dust, 0.3), 0.5);
					B.Quad(Pt(Ua, G.Z0 + 0.006, 0.11), Pt(Ub, G.Z0 + 0.006, 0.11), Pt(Ub, G.Z1 - 0.006, 0.11), Pt(Ua, G.Z1 - 0.006, 0.11), Low, Low, Mid, Mid, Fc.N);
				}
				for (int K = 0; K < 2; ++K)
				{
					const double Z = K == 0 ? G.Z0 + 0.12 : G.Z1 - 0.12;
					B.Box(Pt(G.U, Z, 0.097), Fc.T * (0.5 * G.W - 0.02), Fc.N * 0.013, Up * 0.03, Scaled(P.Paint, 0.9), 4);
				}
			}

			/** The band brushed round an opening: limewash on a mud wall, the doors' paint on a limewashed one; thin, its width
			 * the hand's. */
			void Surround(Pen& B, const Plan& P, const Block& Bk, const Gap& G, uint64_t Seed, double Off, double Cell)
			{
				const bool bDoor = G.What == 1;
				const double Ws = bDoor ? 0.15 : 0.11, U0 = G.U - 0.5 * G.W, U1 = G.U + 0.5 * G.W, Zh = G.Z1 + (bDoor ? 0.14 : 0.1), Zf = bDoor ? G.Z0 - 0.06 : G.Z0 - Ws;
				const Col Base = P.bMud ? P.Wash : Mixed(P.Paint, P.Wash, 0.12);
				const V3 Out = FaceOf(Bk, G.Face).N;
				auto Of = [&](double U, double Z) {
					Col C = Scaled(Base, 0.92 + 0.1 * Noise01(U / 0.3, Z / 0.3, Seed));
					if (P.bMud) C = Mixed(C, P.Mud, 0.3 * Smooth(0.5, 0.8, Noise01(U / 0.2, Z / 0.25, Seed ^ 0x21)));
					return bDoor ? Mixed(C, P.Dust, 0.6 * Smooth(G.Z0 + 0.45, G.Z0, Z)) : C;
				};
				// (how wide the brush left it at a place along one of its four strips)
				auto Wide = [&](int Strip, double Along) { return Ws * (0.78 + 0.5 * Noise01(Along / 0.21, 1.7 * Strip, Seed ^ 0x22)); };
				auto Put = [&](double U, double Z) { return B.Vert(OnWall(P, Bk, G.Face, U, Z, Off), WallN(P, Bk, At(Bk, G.Face, U), Z), Of(U, Z)); };
				// (a strip: its inner edge straight from A to C, its outer edge Sg's way from it)
				auto Strip = [&](int Which, bool bUpright, double Fixed, double A, double C, double Sg) {
					const int N = std::max(1, static_cast<int>(std::ceil((C - A) / Cell)));
					uint32_t Wa = 0, Wb = 0;
					for (int I = 0; I <= N; ++I)
					{
						const double T = Lerp(A, C, static_cast<double>(I) / N), Wd = Sg * Wide(Which, T);
						const uint32_t Na = bUpright ? Put(Fixed, T) : Put(T, Fixed), Nb = bUpright ? Put(Fixed + Wd, T) : Put(T, Fixed + Wd);
						if (I > 0)
						{
							B.Tri(Wa, Wb, Nb, Out);
							B.Tri(Wa, Nb, Na, Out);
						}
						Wa = Na;
						Wb = Nb;
					}
				};
				Strip(0, true, U0, Zf, Zh, -1.0);
				Strip(1, true, U1, Zf, Zh, 1.0);
				Strip(2, false, Zh, U0 - Wide(0, Zh), U1 + Wide(1, Zh), 1.0);
				if (!bDoor) Strip(3, false, G.Z0, U0, U1, -1.0);
			}

			/** An opening from far off: its door's or shutter's colour, or its dark. */
			void Blank(Pen& B, const Plan& P, const Block& Bk, const Gap& G, bool bBare)
			{
				const Col C = G.What == 1 ? Scaled(bBare ? P.Wood : P.Paint, 0.8) : G.Look == 1 ? Rgb(40, 35, 32) : Scaled(P.Paint, 0.8);
				Decal(B, P, Bk, G.Face, G.U - 0.5 * G.W, G.U + 0.5 * G.W, G.Z0, G.Z1, 0.05, 1.5, [&](double, double) { return C; });
			}

			/** The roof beams' ends out through a face under the parapet: palm trunks, each its own girth and reach. */
			void BeamEnds(Pen& B, const Plan& P, const Block& Bk, int F, double Z, uint64_t Seed)
			{
				const Frame Fc = FaceOf(Bk, F);
				double U = -Fc.Half + 0.35 + 0.3 * Unit(Hash(Seed, 1));
				for (int K = 0; U < Fc.Half - 0.3; ++K)
				{
					const uint64_t Kb = Hash(Seed, 0x10 + static_cast<uint64_t>(K));
					// (a pole gone, its place left empty: the row the more uneven for it)
					bool bFree = Unit(Hash(Kb, 5)) > 0.12;
					for (const Gap& G : Bk.Gaps)
						if (G.Look != 3 && G.Face == F && std::fabs(U - G.U) < 0.5 * G.W + 0.25 && Z > G.Z0 - 0.2 && Z < G.Z1 + 0.3) bFree = false;
					for (const Spout& Sp : Bk.Spouts)
						if (Sp.F == F && std::fabs(U - Sp.U) < 0.22) bFree = false;
					if (bFree)
					{
						const double R = 0.05 + 0.03 * Unit(Hash(Kb, 1)), Len = 0.07 + 0.35 * Unit(Hash(Kb, 2));
						const Col C = Varied(P.Wood, 0.8 + 0.3 * Unit(Hash(Kb, 3)), 0.03);
						B.Tube(InWall(P, Bk, F, U, Z, Z, 0.08), InWall(P, Bk, F, U, Z - 0.015, Z, -Len), R, 0.93 * R, 7, C, true);
					}
					U += 0.52 + 0.78 * Unit(Hash(Kb, 4));
				}
			}

			/** A block's spouts: a clay pipe out through the parapet's foot, tipped down a little, its bore dark. */
			void Pipes(Pen& B, const Plan& P, const Block& Bk, bool bFine)
			{
				int N = 0;
				for (const Spout& Sp : Bk.Spouts)
				{
					const uint64_t Ks = Hash(Bk.Key, 0x5F0 + static_cast<uint64_t>(N++));
					const double Z = Bk.Zr + 0.03, Len = 0.42 + 0.2 * Unit(Hash(Ks, 1));
					const V3 A = InWall(P, Bk, Sp.F, Sp.U, Z, Z, 0.1), E = InWall(P, Bk, Sp.F, Sp.U, Z - 0.16 * Len, Z, -Len), Ax = Norm(E - A);
					const Col Clay = Varied(Rgb(176, 120, 86), 0.88 + 0.2 * Unit(Hash(Ks, 2)), 0.03);
					B.Tube(A, E, 0.055, 0.05, bFine ? 8 : 4, Clay, true);
					if (bFine) B.Tube(E - Ax * 0.01, E + Ax * 0.003, 0.034, 0.034, 8, Rgb(34, 28, 25), true);
				}
			}

			// ---------------------------------------------------------------- the stair, the bench, the yard's things

			/** The stair up the wing's front: mud brick plastered with the walls, a landing at its head against the main room,
			 * its treads worn pale at their edges. */
			void Stair(Pen& B, const Plan& P, bool bFine)
			{
				const Block& W = P.Wing;
				const double Ya = W.Y1() - 0.08, Yb = W.Y1() + P.StairW, Bot = -std::max(0.25, P.S.Below), Nose = bFine ? 0.03 : 0.0;
				const V3 Fwd{ 1.0, 0.0, 0.0 }, Side{ 0.0, 1.0, 0.0 };
				auto Plaster = [&](double X, double Z) { return WallCol(P, W, At(W, 0, UOf(W, 0, X)), Z, 0); };
				double Xa = P.Main.X1() - 0.1;
				for (int K = 0; K < P.Steps; ++K)
				{
					const uint64_t Ks = Hash(P.S.Key, 0x700 + static_cast<uint64_t>(K));
					const double Zt = P.Rise * (P.Steps - K) + (K == 0 ? 0.0 : 0.012 * (Unit(Hash(Ks, 1)) - 0.5));
					const double Xb = K == 0 ? P.Main.X1() + P.Land : Xa + P.Run, Zl = K + 1 < P.Steps ? P.Rise * (P.Steps - K - 1) - 0.02 : Bot;
					const Col Wc = Plaster(0.5 * (Xa + Xb), Zt), Tread = Mixed(Wc, P.Dust, 0.55), Worn = Mixed(Tread, Scaled(P.Dust, 1.05), 0.6), Riser = Scaled(Mixed(Wc, P.Dust, 0.2), 0.9);
					B.Quad(V3{ Xa, Ya, Zt }, V3{ Xb - Nose, Ya, Zt }, V3{ Xb - Nose, Yb, Zt }, V3{ Xa, Yb, Zt }, Tread, Worn, Worn, Tread, Up);
					if (bFine) B.Quad1(V3{ Xb - Nose, Ya, Zt }, V3{ Xb, Ya, Zt - Nose }, V3{ Xb, Yb, Zt - Nose }, V3{ Xb - Nose, Yb, Zt }, Worn, Norm(Fwd + Up));
					B.Quad1(V3{ Xb, Ya, Zl }, V3{ Xb, Yb, Zl }, V3{ Xb, Yb, Zt - Nose }, V3{ Xb, Ya, Zt - Nose }, Riser, Fwd);
					// (its outer side, down into the ground)
					const double Zm = std::min(0.5, Zt);
					B.Quad(V3{ Xa, Yb, Bot }, V3{ Xb, Yb, Bot }, V3{ Xb, Yb, Zm }, V3{ Xa, Yb, Zm }, Plaster(Xa, 0.0), Plaster(Xb, 0.0), Plaster(Xb, Zm), Plaster(Xa, Zm), Side);
					if (Zt > Zm + 1e-6) B.Quad(V3{ Xa, Yb, Zm }, V3{ Xb, Yb, Zm }, V3{ Xb, Yb, Zt }, V3{ Xa, Yb, Zt }, Plaster(Xa, Zm), Plaster(Xb, Zm), Plaster(Xb, Zt), Plaster(Xa, Zt), Side);
					Xa = Xb;
				}
			}

			/** The bench of plastered mud along the main room's front, between its window and its door. */
			void Bench(Pen& B, const Plan& P, bool bFine)
			{
				const Block& M = P.Main;
				const double Xa = MidOf(M, M.Gaps[1]).X - 0.35, Xb = MidOf(M, M.Gaps[0]).X - 0.5 * M.Gaps[0].W - 0.3, Y0 = M.Y1();
				if (Xb - Xa < 0.8) return;
				// (its section: out from the wall, up from the floor; its front edge rounded by sitting)
				const double Pr[5][2] = { { 0.46, -0.2 }, { 0.46, 0.33 }, { 0.42, 0.4 }, { 0.33, 0.43 }, { -0.06, 0.43 } };
				const int N = bFine ? std::max(2, static_cast<int>(std::ceil((Xb - Xa) / 0.45))) : 1;
				auto Cl = [&](double X, int R) {
					const Col Wc = WallCol(P, M, At(M, 0, UOf(M, 0, X)), std::max(0.0, Pr[R][1]), 0);
					return R >= 2 ? Mixed(Wc, P.Dust, 0.5) : Wc;
				};
				for (int I = 0; I < N; ++I)
				{
					const double Xi = Lerp(Xa, Xb, static_cast<double>(I) / N), Xj = Lerp(Xa, Xb, static_cast<double>(I + 1) / N);
					for (int R = 0; R < 4; ++R)
					{
						const V3 To = Norm(V3{ 0.0, Pr[R + 1][1] - Pr[R][1], Pr[R][0] - Pr[R + 1][0] });
						B.Quad(V3{ Xi, Y0 + Pr[R][0], Pr[R][1] }, V3{ Xj, Y0 + Pr[R][0], Pr[R][1] }, V3{ Xj, Y0 + Pr[R + 1][0], Pr[R + 1][1] }, V3{ Xi, Y0 + Pr[R + 1][0], Pr[R + 1][1] }, Cl(Xi, R), Cl(Xj, R), Cl(Xj, R + 1), Cl(Xi, R + 1), To);
					}
				}
				for (int End = 0; End < 2; ++End)
				{
					const double X = End == 0 ? Xa : Xb;
					const Col C = Scaled(Cl(X, 0), 0.95);
					const V3 Q{ X, Y0 - 0.06, -0.2 }, To{ End == 0 ? -1.0 : 1.0, 0.0, 0.0 };
					for (int R = 0; R < 4; ++R) B.Tri1(Q, V3{ X, Y0 + Pr[R][0], Pr[R][1] }, V3{ X, Y0 + Pr[R + 1][0], Pr[R + 1][1] }, C, To);
				}
			}

			/** A turned shape about a plumb axis: its radius and height at each ring up its profile, Sides round it; its colour
			 * from Of(ring, angle). */
			template <typename Fn> void Lathe(Pen& B, V3 O, const std::vector<std::pair<double, double>>& Pr, int Sides, Fn Of)
			{
				const int NR = static_cast<int>(Pr.size());
				std::vector<uint32_t> Id(static_cast<size_t>(NR * (Sides + 1)));
				for (int R = 0; R < NR; ++R)
				{
					const size_t Ra = static_cast<size_t>(std::max(0, R - 1)), Rb = static_cast<size_t>(std::min(NR - 1, R + 1));
					const double Dr = Pr[Rb].first - Pr[Ra].first, Dz = Pr[Rb].second - Pr[Ra].second;
					for (int K = 0; K <= Sides; ++K)
					{
						const double A = 2.0 * Pi * K / Sides;
						const V3 H{ std::cos(A), std::sin(A), 0.0 };
						Id[static_cast<size_t>(R * (Sides + 1) + K)] = B.Vert(O + H * Pr[static_cast<size_t>(R)].first + Up * Pr[static_cast<size_t>(R)].second, Norm(H * Dz - Up * Dr), Of(R, A));
					}
				}
				for (int R = 0; R + 1 < NR; ++R)
				{
					const double Dr = Pr[static_cast<size_t>(R + 1)].first - Pr[static_cast<size_t>(R)].first, Dz = Pr[static_cast<size_t>(R + 1)].second - Pr[static_cast<size_t>(R)].second;
					for (int K = 0; K < Sides; ++K)
					{
						const double A = 2.0 * Pi * (K + 0.5) / Sides;
						const V3 To = Norm(V3{ std::cos(A) * Dz, std::sin(A) * Dz, -Dr });
						const size_t I = static_cast<size_t>(R * (Sides + 1) + K), J = I + static_cast<size_t>(Sides + 1);
						B.Tri(Id[I], Id[I + 1], Id[J + 1], To);
						B.Tri(Id[I], Id[J + 1], Id[J], To);
					}
				}
			}

			/** The bread oven's dome: its radius at a height. */
			double OvenR(double Z) { return std::max(0.03, 0.56 * std::sqrt(std::max(0.0, 1.0 - Sq(Clamp01((Z - 0.16) / 0.64))))); }
			/** The bread oven in the yard's corner: a dome of mud on a low foot, its mouth an arch with a lip of mud, the smoke
			 * black up its face from the mouth and round its vent. */
			void Oven(Pen& B, const Plan& P, bool bFine)
			{
				const V3 O{ P.OvenX, P.OvenY, 0.0 }, Dm{ std::cos(P.OvenTo), std::sin(P.OvenTo), 0.0 }, Sd = Cross(Up, Dm);
				std::vector<std::pair<double, double>> Pr{ { 0.68, -0.25 }, { 0.68, 0.1 }, { 0.62, 0.15 } };
				const int NR = bFine ? 8 : 4;
				for (int K = 0; K <= NR; ++K)
				{
					const double Z = 0.16 + 0.64 * K / NR;
					Pr.push_back({ OvenR(Z), Z });
				}
				Pr.push_back({ 0.012, 0.785 });
				const Col Mud = Varied(P.Mud, 0.96, 0.02), Soot = Rgb(48, 43, 40);
				const int Last = static_cast<int>(Pr.size()) - 1;
				Lathe(B, O, Pr, bFine ? 14 : 7, [&](int R, double A) {
					const double Z = Pr[static_cast<size_t>(R)].second, Da = std::remainder(A - P.OvenTo, 2.0 * Pi);
					if (R == Last) return Soot;
					Col C = Scaled(Mud, 0.92 + 0.14 * Noise01(1.6 * std::cos(A) + Z / 0.3, 1.6 * std::sin(A), P.S.Key ^ 0x0E1));
					C = Mixed(C, Soot, 0.8 * Smooth(0.8, 0.3, std::fabs(Da)) * Smooth(0.2, 0.42, Z) * Smooth(0.72, 0.56, Z));
					C = Mixed(C, Soot, 0.5 * Smooth(0.7, 0.79, Z));
					return Mixed(C, P.Dust, 0.5 * Smooth(0.14, 0.0, Z));
				});
				// (its mouth: across it and up; laid on the dome a little out from it)
				const double Ms[7][2] = { { -0.17, 0.16 }, { -0.17, 0.33 }, { -0.115, 0.44 }, { 0.0, 0.48 }, { 0.115, 0.44 }, { 0.17, 0.33 }, { 0.17, 0.16 } };
				auto On = [&](double Across, double Z, double Off) {
					const double R = OvenR(std::max(0.16, Z)) + Off;
					return O + Dm * std::sqrt(std::max(0.01, R * R - Across * Across)) + Sd * Across + Up * Z;
				};
				const Col Black = Rgb(24, 21, 19);
				if (!bFine)
				{
					B.Quad1(On(-0.17, 0.16, 0.03), On(0.17, 0.16, 0.03), On(0.14, 0.44, 0.03), On(-0.14, 0.44, 0.03), Black, Dm);
					return;
				}
				const V3 Mc = On(0.0, 0.3, 0.014);
				auto Lip = [&](int J, double Sc) { return On(Ms[J][0] * Sc, 0.3 + (Ms[J][1] - 0.3) * (J == 0 || J == 6 ? 1.0 : Sc), 0.03); };
				B.Tri1(Mc, On(Ms[6][0], Ms[6][1], 0.014), On(Ms[0][0], Ms[0][1], 0.014), Black, Dm);
				for (int K = 0; K < 6; ++K)
				{
					B.Tri1(Mc, On(Ms[K][0], Ms[K][1], 0.014), On(Ms[K + 1][0], Ms[K + 1][1], 0.014), Black, Dm);
					const Col Ca = Mixed(Mud, Soot, K >= 1 && K <= 4 ? 0.7 : 0.35);
					B.Quad1(Lip(K, 1.0), Lip(K + 1, 1.0), Lip(K + 1, 1.32), Lip(K, 1.32), Ca, Dm);
				}
				// (the brushwood for it, along the wall beside it)
				const V3 Pile{ P.OvenX + 1.25, P.Yard.Y1() - P.Yard.Tp - 0.34, 0.0 };
				for (int K = 0; K < 9; ++K)
				{
					const uint64_t Kk = Hash(P.S.Key, 0x800 + static_cast<uint64_t>(K));
					const double A = 0.5 * (Unit(Hash(Kk, 1)) - 0.5), L = 0.55 + 0.5 * Unit(Hash(Kk, 2)), R = 0.011 + 0.01 * Unit(Hash(Kk, 3));
					const V3 Ax{ std::cos(A), std::sin(A), 0.0 };
					const V3 M = Pile + V3{ 0.3 * (Unit(Hash(Kk, 4)) - 0.5), 0.16 * (Unit(Hash(Kk, 5)) - 0.5), 0.03 + 0.035 * (K / 3) };
					B.Tube(M - Ax * (0.5 * L) + Up * (0.03 * Unit(Hash(Kk, 6))), M + Ax * (0.5 * L) + Up * (0.05 * Unit(Hash(Kk, 7))), R, 0.7 * R, 5, Varied(Rgb(118, 98, 76), 0.8 + 0.3 * Unit(Hash(Kk, 8)), 0.03), true, true);
				}
			}

			/** A water jar of red clay, Sc its size: dark where the water sweats through its belly, dust on its shoulder. */
			void Jar(Pen& B, V3 O, double Sc, uint64_t Seed, int Sides)
			{
				const double Pr0[9][2] = { { 0.07, 0.0 }, { 0.17, 0.1 }, { 0.24, 0.26 }, { 0.25, 0.38 }, { 0.2, 0.52 }, { 0.12, 0.62 }, { 0.13, 0.7 }, { 0.095, 0.7 }, { 0.02, 0.6 } };
				std::vector<std::pair<double, double>> Pr;
				for (const auto& Q : Pr0) Pr.push_back({ Q[0] * Sc, Q[1] * Sc });
				const Col Clay = Varied(Rgb(176, 124, 90), 0.88 + 0.2 * Unit(Hash(Seed, 1)), 0.04 * (Unit(Hash(Seed, 2)) - 0.5));
				Lathe(B, O, Pr, Sides, [&](int R, double A) {
					if (R >= 7) return Rgb(30, 26, 24);
					const double Z = Pr0[R][1];
					Col C = Scaled(Clay, 0.94 + 0.1 * Noise01(1.5 * std::cos(A) + 4.0 * Z, 1.5 * std::sin(A), Seed));
					C = Mixed(C, Scaled(Clay, 0.7), 0.6 * Smooth(0.42, 0.2, Z));
					return Mixed(C, Rgb(214, 190, 146), 0.35 * Smooth(0.4, 0.56, Z));
				});
			}
			/** Where the water jars stand: along the main room's front, past its door. */
			V3 JarAt(const Plan& P, int K)
			{
				const Block& M = P.Main;
				return V3{ MidOf(M, M.Gaps[0]).X + 0.5 * M.Gaps[0].W + 0.42 + 0.5 * K, M.Y1() + 0.3 + 0.05 * (K % 2), -0.03 };
			}
			void Jars(Pen& B, const Plan& P, bool bFine)
			{
				for (int K = 0; K < P.Jars; ++K)
				{
					const V3 O = JarAt(P, K);
					if (O.X > P.Main.X1() - 0.28) break;
					Jar(B, O, 0.9 + 0.3 * Unit(Hash(P.S.Key, 0x850 + static_cast<uint64_t>(K))), Hash(P.S.Key, 0x860 + static_cast<uint64_t>(K)), bFine ? 9 : 5);
				}
			}

			/** The shade in the yard's far corner: palm fronds laid over poles, a post under its open corner and another along
			 * its yard side, the walls' tops carrying the rest; the fronds' ends ragged over the open sides. */
			void Shade(Pen& B, const Plan& P, bool bFine)
			{
				const Block& Yd = P.Yard;
				const double Xe = Yd.X1() - 0.1, Ye = Yd.Y1() - 0.1, Xs = Xe - 2.55, Ys = Ye - 2.2, Zu = Yd.Zp + 0.07;
				const Col Pole = Varied(P.Wood, 0.9, 0.03), Frond = Rgb(188, 168, 128);
				const int Sides = bFine ? 7 : 4;
				const V3 Pa{ Xs + 0.1, Ys + 0.1, 0.0 }, Pb{ Xs + 1.35, Ys + 0.1, 0.0 };
				B.Tube(Pa + Up * -0.25, Pa + Up * Zu, 0.065, 0.055, Sides, Pole, false);
				B.Tube(Pb + Up * -0.25, Pb + Up * Zu, 0.06, 0.05, Sides, Scaled(Pole, 0.93), false);
				B.Tube(V3{ Xs - 0.1, Ys + 0.1, Zu + 0.04 }, V3{ Xe, Ys + 0.1, Zu + 0.04 }, 0.045, 0.04, Sides, Pole, true, true);
				B.Tube(V3{ Xs + 0.1, Ys - 0.1, Zu + 0.04 }, V3{ Xs + 0.1, Ye, Zu + 0.04 }, 0.045, 0.04, Sides, Pole, true, true);
				if (bFine)
					for (int K = 0; K < 5; ++K)
					{
						const double X = Xs + 0.5 + (Xe - Xs - 0.65) * K / 4.0;
						B.Tube(V3{ X, Ys - 0.12, Zu + 0.1 }, V3{ X, Ye, Zu + 0.1 }, 0.028, 0.024, 5, Scaled(Pole, 0.9 + 0.04 * K), true, true);
					}
				// (the fronds: laid side by side across the rafters, each a stem with its leaflets falling away either side, its own
				// tone, lapping its neighbours, its end ragged over the open side; the dark under them)
				const int NF = bFine ? 11 : 3, NS = bFine ? 7 : 2;
				const double Xa = Xs - 0.15, Xb = Xe + 0.05, Ya = Ys - 0.2, Yb = Ye + 0.05, Wf = (Xb - Xa) / NF;
				for (int I = 0; I < NF; ++I)
				{
					const uint64_t Kf = Hash(P.S.Key, 0x900 + static_cast<uint64_t>(I));
					const double Xm = Xa + (I + 0.5) * Wf + (bFine ? 0.04 * (Unit(Hash(Kf, 1)) - 0.5) : 0.0), Hw = Wf * (bFine ? 0.56 + 0.14 * Unit(Hash(Kf, 2)) : 0.5);
					const double Y0 = Ya - (bFine ? 0.25 * Unit(Hash(Kf, 3)) : 0.1), Lift = bFine ? 0.035 * Unit(Hash(Kf, 4)) + (I % 2 == 0 ? 0.0 : 0.03) : 0.02;
					const Col C = Varied(Frond, bFine ? 0.72 + 0.4 * Unit(Hash(Kf, 5)) : 0.9, bFine ? 0.05 * (Unit(Hash(Kf, 6)) - 0.5) : 0.0);
					uint32_t Was[3] = { 0, 0, 0 };
					for (int J = 0; J <= NS; ++J)
					{
						const double Y = Lerp(Y0, Yb, static_cast<double>(J) / NS), X = Xm + (bFine ? 0.03 * Noise(Y / 0.6, 1.7 * I, P.S.Key ^ 0x9B) : 0.0);
						const double Z = Zu + 0.2 + Lift + (bFine ? 0.025 * Noise(Xm / 0.5, Y / 0.7, P.S.Key ^ 0x9A) : 0.0) - (J == 0 ? 0.07 : 0.0);
						const double Light = 0.92 + 0.14 * Noise01(Y / 0.3, 2.3 * I, P.S.Key ^ 0x9C);
						const uint32_t Now[3] = {
							B.Vert(V3{ X - Hw, Y, Z - 0.035 }, Norm(V3{ -0.5, 0.0, 1.0 }), Scaled(C, 0.82 * Light)),
							B.Vert(V3{ X, Y, Z }, Up, Scaled(C, 1.06 * Light)),
							B.Vert(V3{ X + Hw, Y, Z - 0.035 }, Norm(V3{ 0.5, 0.0, 1.0 }), Scaled(C, 0.82 * Light)) };
						if (J > 0)
							for (int H = 0; H < 2; ++H)
							{
								B.Tri(Was[H], Was[H + 1], Now[H + 1], Up);
								B.Tri(Was[H], Now[H + 1], Now[H], Up);
							}
						for (int H = 0; H < 3; ++H) Was[H] = Now[H];
					}
				}
				B.Quad1(V3{ Xa, Ya, Zu + 0.13 }, V3{ Xb, Ya, Zu + 0.13 }, V3{ Xb, Yb, Zu + 0.13 }, V3{ Xa, Yb, Zu + 0.13 }, Scaled(Frond, 0.4), Up * -1.0);
			}

			/** A rug hung over the wing's parapet to air, its hem down the wall toward the yard: plainly a rug, and no flag.
			 * Narrow stripes of dyed wool across it, a broad one never beside its like, a band of small diamonds and one of
			 * saw teeth among them, the warp's ends a fringe along its hem. Its wools one family to a rug (madder with ochre,
			 * brown and wine; camel with brown, madder and ochre; indigo with ochre, brown and olive), sun-faded: never red,
			 * white and blue together, nor black, white and red. */
			void Rug(Pen& B, const Plan& P)
			{
				const Block& W = P.Wing;
				const double Uc = UOf(W, 0, W.X1() - 1.85), Hw = 0.4, Hang = 0.82;
				const int NK = 10;
				const uint64_t Key = Hash(P.S.Key, 0xB20);
				const V3 N = FaceOf(W, 0).N;
				// Its wools: its field's, then those of its narrow stripes.
				const Col Madder = Rgb(146, 64, 50), Wine = Rgb(112, 48, 42), Ochre = Rgb(186, 148, 84), Camel = Rgb(172, 140, 100), Brown = Rgb(98, 70, 50), Indigo = Rgb(56, 68, 100), Olive = Rgb(110, 112, 70);
				const int Fam = std::min(2, static_cast<int>(3.0 * Unit(Hash(Key, 1))));
				const Col Wool[3][4] = { { Madder, Ochre, Brown, Wine }, { Camel, Brown, Madder, Ochre }, { Indigo, Ochre, Brown, Olive } };
				// Its way from its outer hem: up the wall's face to under the parapet's roll, over the roll, across the top,
				// down inside. Each turn of it: how far along, how far in from the wall's line, how far under the wall's top.
				const double Sn[8] = { 0.0, Hang - 0.09, Hang - 0.02, Hang + 0.04, Hang + W.Tp - 0.05, Hang + W.Tp + 0.01, Hang + W.Tp + 0.08, Hang + W.Tp + 0.34 };
				const double In[8] = { 0.0, 0.0, 0.008, 0.045, W.Tp - 0.045, W.Tp - 0.008, W.Tp + 0.016, W.Tp + 0.016 };
				const double Dn[8] = { Hang, 0.09, 0.03, -0.016, -0.016, 0.03, 0.09, 0.35 };
				const double Wv[8] = { 1.0, 1.0, 1.0, 0.75, 0.5, 0.0, 0.0, 0.0 };
				auto Pt = [&](double U, double Sv) {
					const Column C = At(W, 0, U);
					const double Zt = TopAt(W, C);
					if (Sv <= Sn[1]) return OnWall(P, W, 0, U, Zt - Lerp(Hang, 0.09, Sv / Sn[1]), 0.016);
					int J = 1;
					while (J < 6 && Sv > Sn[J + 1]) ++J;
					auto Turn = [&](int I) { return I == 1 ? OnWall(P, W, 0, U, Zt - 0.09, 0.016) : Ring(W, C, Lean(P, Zt) + In[I] - Wv[I] * Disp(W, C.Pm, Zt)) + V3{ 0.0, 0.0, Zt - Dn[I] }; };
					const V3 A = Turn(J), E = Turn(J + 1);
					return A + (E - A) * Clamp01((Sv - Sn[J]) / (Sn[J + 1] - Sn[J]));
				};
				// Its bands from the hem: a few narrow stripes, a broader one of the field, and so on; the diamonds low on what
				// hangs down the wall, the saw teeth higher.
				struct Band
				{
					double S0, S1;
					int Wool, Fig, On; // (its wool; 0 plain, 1 diamonds, 2 saw teeth, in that wool on the wool On)
				};
				std::vector<Band> Bands;
				{
					double Sv = 0.0;
					int Fig = 0;
					auto Add = [&](double Wide, int Wl, int Fg, int On) {
						Bands.push_back({ Sv, Sv + Wide, Wl, Fg, On });
						Sv += Wide;
					};
					Add(0.03, 2, 0, 0);
					for (int G = 0; Sv < Sn[7]; ++G)
					{
						const uint64_t Kg = Hash(Key, 0x200 + static_cast<uint64_t>(G));
						Add(0.045 + 0.025 * Unit(Hash(Kg, 1)), 0, 0, 0);
						if (Fig == 0 && Sv > 0.1)
						{
							Add(0.02, 1, 0, 0);
							Add(0.09, 1, 1, 2);
							Add(0.02, 1, 0, 0);
							Fig = 1;
							continue;
						}
						if (Fig == 1 && Sv > 0.44 && Sv < Sn[1] - 0.14)
						{
							Add(0.018, 2, 0, 0);
							Add(0.07, 3, 2, 0);
							Add(0.018, 2, 0, 0);
							Fig = 2;
							continue;
						}
						const int Nn = 2 + static_cast<int>(1.999 * Unit(Hash(Kg, 2))), First = static_cast<int>(2.999 * Unit(Hash(Kg, 3)));
						for (int K = 0; K < Nn; ++K) Add(0.02 + 0.02 * Unit(Hash(Kg, 0x10 + static_cast<uint64_t>(K))), 1 + (First + K) % 3, 0, 0);
					}
				}
				// Its rows: every band's two edges (a figured band's middle too), and the turns of its way.
				std::vector<double> Rows;
				for (const Band& Bd : Bands)
				{
					Rows.push_back(Bd.S0);
					if (Bd.Fig == 1) Rows.push_back(0.5 * (Bd.S0 + Bd.S1));
				}
				for (int I = 1; I < 7; ++I) Rows.push_back(Sn[I]);
				Rows.push_back(Sn[7]);
				std::sort(Rows.begin(), Rows.end());
				{
					std::vector<double> Kept;
					for (const double Sv : Rows)
						if (Sv <= Sn[7] && (Kept.empty() || Sv - Kept.back() > 0.004)) Kept.push_back(Sv);
					Rows = Kept;
				}
				const size_t NR = Rows.size();
				std::vector<V3> G(NR * static_cast<size_t>(NK + 1));
				for (size_t R = 0; R < NR; ++R)
					for (int K = 0; K <= NK; ++K)
					{
						const double U = Uc + Hw * (2.0 * K / NK - 1.0);
						// (its hem sags a little between its threads)
						G[R * static_cast<size_t>(NK + 1) + static_cast<size_t>(K)] = Pt(U, Rows[R]) - (R == 0 ? Up * (0.006 + 0.008 * Noise01(U / 0.2, 1.0, Key ^ 0xB1)) : V3{ 0.0, 0.0, 0.0 });
					}
				auto Dyed = [&](int Wl, uint64_t Kd) { return Mixed(Scaled(Wool[Fam][Wl], 0.9 + 0.14 * Unit(Kd)), P.Dust, 0.12); };
				for (size_t R = 0; R + 1 < NR; ++R)
				{
					const double Sm = 0.5 * (Rows[R] + Rows[R + 1]);
					size_t Ib = 0;
					while (Ib + 1 < Bands.size() && Sm > Bands[Ib].S1) ++Ib;
					const Band& Bd = Bands[Ib];
					const V3 To = Sm < Sn[1] ? N : Sm < Sn[3] ? Norm(N + Up) : Sm < Sn[4] ? Up : Sm < Sn[6] ? Norm(Up - N) : N * -1.0;
					const Col Cw = Dyed(Bd.Wool, Hash(Key, 0x300 + Ib)), Co = Dyed(Bd.On, Hash(Key, 0x400 + Ib));
					// (a figured band's lower half, or its upper)
					const bool bUpper = Bd.Fig == 1 && Sm > 0.5 * (Bd.S0 + Bd.S1);
					for (int K = 0; K < NK; ++K)
					{
						const size_t A = R * static_cast<size_t>(NK + 1) + static_cast<size_t>(K), C = A + static_cast<size_t>(NK + 1);
						// (the weave: each hand's width of it a breath lighter or darker)
						const double Weave = 0.975 + 0.05 * Unit(Hash(Key, 0x500 + static_cast<uint64_t>(K), Ib));
						const Col Ca = Scaled(Cw, Weave), Cg = Scaled(Co, Weave);
						if (Bd.Fig == 0)
						{
							B.Quad1(G[A], G[A + 1], G[C + 1], G[C], Ca, To);
							continue;
						}
						const V3 Lo = (G[A] + G[A + 1]) * 0.5, Hi = (G[C] + G[C + 1]) * 0.5;
						if (Bd.Fig == 2)
						{
							// (a saw tooth: its point up)
							B.Tri1(G[A], G[A + 1], Hi, Ca, To);
							B.Tri1(G[A], Hi, G[C], Cg, To);
							B.Tri1(G[A + 1], G[C + 1], Hi, Cg, To);
						}
						else if (!bUpper)
						{
							// (a diamond's lower half: from its point at the band's foot out to its two side points)
							B.Tri1(Lo, G[C + 1], G[C], Ca, To);
							B.Tri1(G[A], Lo, G[C], Cg, To);
							B.Tri1(Lo, G[A + 1], G[C + 1], Cg, To);
						}
						else
						{
							B.Tri1(G[A], G[A + 1], Hi, Ca, To);
							B.Tri1(G[A], Hi, G[C], Cg, To);
							B.Tri1(G[A + 1], G[C + 1], Hi, Cg, To);
						}
					}
				}
				// (its fringe: the warp's ends, undyed, two to a hand's width, down the wall from its hem)
				for (int K = 0; K < 2 * NK; ++K)
				{
					const uint64_t Kf = Hash(Key, 0x600 + static_cast<uint64_t>(K));
					const double U = Uc + Hw * ((K + 0.5) / NK - 1.0) + 0.006 * (Unit(Hash(Kf, 1)) - 0.5), Len = 0.045 + 0.03 * Unit(Hash(Kf, 2)), Lean2 = 0.012 * (Unit(Hash(Kf, 3)) - 0.5);
					const double Zh = TopAt(W, At(W, 0, U)) - Hang - 0.004;
					const Col C = Varied(Rgb(186, 170, 140), 0.88 + 0.16 * Unit(Hash(Kf, 4)), 0.02);
					B.Quad1(OnWall(P, W, 0, U - 0.007 + Lean2, Zh - Len, 0.014), OnWall(P, W, 0, U + 0.007 + Lean2, Zh - Len, 0.014), OnWall(P, W, 0, U + 0.008, Zh, 0.015), OnWall(P, W, 0, U - 0.008, Zh, 0.015), C, N);
				}
			}

			/** The three small niches over the main room's door, three-cornered: each let 13 cm into the wall, its sides turned
			 * in from the plaster's own colour at its mouth through dark brown, black only at its back; the wall made good
			 * round its three corners, where the shell leaves a square out for it. */
			void Niches(Pen& B, const Plan& P)
			{
				const Block& M = P.Main;
				const Col Brown = Rgb(140, 108, 78), Dark = Rgb(92, 70, 52), Black = Rgb(52, 41, 32);
				for (const Gap& G : M.Gaps)
				{
					if (G.Look != 3) continue;
					const Column Ca = At(M, G.Face, G.U - 0.5 * G.W), Cb = At(M, G.Face, G.U + 0.5 * G.W);
					const V3 N = FaceOf(M, G.Face).N;
					// (the square's corners as the shell has them; the niche's point at the middle of the square's head)
					const V3 A = WallPt(P, M, Ca, G.Z0), E = WallPt(P, M, Cb, G.Z0), Ah = WallPt(P, M, Ca, G.Z1), Eh = WallPt(P, M, Cb, G.Z1), T = (Ah + Eh) * 0.5;
					const Col Wa = WallCol(P, M, Ca, G.Z0, 0), We = WallCol(P, M, Cb, G.Z0, 0), Wah = WallCol(P, M, Ca, G.Z1, 0), Weh = WallCol(P, M, Cb, G.Z1, 0), Wt = Mixed(Wah, Weh, 0.5);
					const V3 Na = WallN(P, M, Ca, G.Z0), Ne = WallN(P, M, Cb, G.Z0), Nah = WallN(P, M, Ca, G.Z1), Neh = WallN(P, M, Cb, G.Z1), Nt = Norm(Nah + Neh);
					{
						const uint32_t Va = B.Vert(A, Na, Wa), Vt = B.Vert(T, Nt, Wt), Vah = B.Vert(Ah, Nah, Wah);
						B.Tri(Va, Vt, Vah, N);
						const uint32_t Ve = B.Vert(E, Ne, We), Veh = B.Vert(Eh, Neh, Weh), Vt2 = B.Vert(T, Nt, Wt);
						B.Tri(Ve, Veh, Vt2, N);
					}
					// (its mouth's three corners; a ring of it part of the way in, smaller; its back)
					const V3 Mid = (A + E + T) * (1.0 / 3.0), Mouth[3] = { A, E, T };
					const Col Wm[3] = { Wa, We, Wt };
					V3 Part[3], Back[3];
					for (int K = 0; K < 3; ++K)
					{
						Part[K] = Mid + (Mouth[K] - Mid) * 0.8 - N * 0.08;
						Back[K] = Mid + (Mouth[K] - Mid) * 0.55 - N * 0.13;
					}
					for (int K = 0; K < 3; ++K)
					{
						// (its sill, then its two slopes: the sill catches the sky, one slope the light along the wall)
						const int J = (K + 1) % 3;
						const V3 To = Norm(Mid - (Mouth[K] + Mouth[J]) * 0.5 + N * 0.05);
						const double Lit = K == 0 ? 0.94 : K == 1 ? 0.64 : 0.84;
						const Col Deep = Mixed(Scaled(Wt, 0.5 * Lit), Brown, 0.6), Last = Dark;
						B.Quad(Mouth[K], Mouth[J], Part[J], Part[K], Scaled(Wm[K], Lit), Scaled(Wm[J], Lit), Deep, Deep, To);
						B.Quad(Part[K], Part[J], Back[J], Back[K], Deep, Deep, Last, Last, To);
					}
					B.Tri1(Back[0], Back[1], Back[2], Black, N);
				}
			}

			/** The ladder from the wing's roof up to the main room's: two poles, its rungs lashed across. */
			void Ladder(Pen& B, const Plan& P)
			{
				const Block& M = P.Main;
				const double Yc = P.Wing.Cy + 0.3;
				const V3 Foot{ M.X1() + 0.72, Yc, P.Wing.Zr - 0.04 }, Head{ M.X1() - Lean(P, M.Zp) + 0.05, Yc, M.Zp + 0.02 };
				const V3 Ax = Norm(Head - Foot), End = Head + Ax * 0.4;
				const Col Wood = Varied(P.Wood, 0.95, 0.03);
				for (int Sd = -1; Sd <= 1; Sd += 2) B.Beam(Foot + V3{ 0.0, 0.2 * Sd, 0.0 }, End + V3{ 0.0, 0.2 * Sd, 0.0 }, 0.045, 0.035, Scaled(Wood, Sd < 0 ? 0.92 : 1.0));
				const double Len = std::sqrt(Dot(End - Foot, End - Foot));
				for (int K = 1; 0.29 * K < Len - 0.1; ++K)
					B.Tube(Foot + Ax * (0.29 * K) + V3{ 0.0, -0.22, 0.0 }, Foot + Ax * (0.29 * K) + V3{ 0.0, 0.22, 0.0 }, 0.015, 0.015, 5, Scaled(Wood, 0.9), true, true);
			}

			/** On the wing's roof: a reed mat with dates drying on it, two small pots by the parapet. */
			void RoofThings(Pen& B, const Plan& P)
			{
				const Block& W = P.Wing;
				const double Xc = W.Cx + 0.35, Yc = W.Cy - 0.25, Z = W.Zr - 0.025;
				const int NX = 9, NY = 6;
				std::vector<uint32_t> Id(static_cast<size_t>((NX + 1) * (NY + 1)));
				for (int I = 0; I <= NX; ++I)
					for (int J = 0; J <= NY; ++J)
					{
						const uint64_t K = Hash(P.S.Key, 0xC00 + static_cast<uint64_t>(I * 16 + J));
						// (the mat's own straw round its edge and here and there between the dates)
						const bool bEdge = I == 0 || J == 0 || I == NX || J == NY || Unit(Hash(K, 1)) < 0.12;
						const Col C = bEdge ? Varied(Rgb(188, 168, 118), 0.9 + 0.15 * Unit(K), 0.02) : Varied(Rgb(112, 60, 38), 0.8 + 0.35 * Unit(K), 0.08 * (Unit(Hash(K, 2)) - 0.5));
						Id[static_cast<size_t>(I * (NY + 1) + J)] = B.Vert(V3{ Xc + 1.1 * (static_cast<double>(I) / NX - 0.5), Yc + 0.75 * (static_cast<double>(J) / NY - 0.5), Z + (bEdge ? 0.0 : 0.02) }, Up, C);
					}
				for (int I = 0; I < NX; ++I)
					for (int J = 0; J < NY; ++J)
					{
						const size_t A = static_cast<size_t>(I * (NY + 1) + J), C = static_cast<size_t>((I + 1) * (NY + 1) + J);
						B.Tri(Id[A], Id[C], Id[C + 1], Up);
						B.Tri(Id[A], Id[C + 1], Id[A + 1], Up);
					}
				Jar(B, V3{ W.X1() - 0.78, W.Y0() + 0.72, W.Zr - 0.05 }, 0.5, Hash(P.S.Key, 0xC40), 8);
				Jar(B, V3{ W.X1() - 1.12, W.Y0() + 0.66, W.Zr - 0.05 }, 0.38, Hash(P.S.Key, 0xC41), 8);
			}

			/** The yard's gate: two leaves of rough planks on their ledges and a brace, grey with the sun; one shut, one swung in. */
			void Gate(Pen& B, const Plan& P, bool bFine)
			{
				const Block& Yd = P.Yard;
				const Gap& G = Yd.Gaps[0];
				const Frame Fc = FaceOf(Yd, 0);
				for (int Lf = 0; Lf < 2; ++Lf)
				{
					const uint64_t Kl = Hash(P.S.Key, 0xD00 + static_cast<uint64_t>(Lf));
					const double Sg = Lf == 0 ? 1.0 : -1.0, Wd = 0.5 * G.W - 0.07, Ht = 1.6 + 0.12 * Unit(Hash(Kl, 1)), Sw = Lf == 0 ? 0.05 : 0.75 + 0.5 * Unit(Hash(Kl, 2));
					const V3 Hinge = Ring(Yd, At(Yd, 0, G.U - Sg * (0.5 * G.W - 0.04)), Lean(P, 1.0) + 0.5 * Yd.Tp);
					// (its way from its hinge: along the wall to the gate's middle, swung in)
					const V3 Dir = Fc.T * (Sg * std::cos(Sw)) - Fc.N * std::sin(Sw), Nr = Cross(Up, Dir);
					const Col Wood = Varied(Mixed(P.Wood, Rgb(150, 142, 130), 0.45), 0.92 + 0.12 * Unit(Hash(Kl, 3)), 0.02);
					if (!bFine)
					{
						B.Box(Hinge + Dir * (0.5 * Wd) + Up * (0.06 + 0.5 * Ht), Dir * (0.5 * Wd), Nr * 0.02, Up * (0.5 * Ht), Scaled(Wood, 0.95), 16);
						continue;
					}
					const int Np = 6;
					for (int K = 0; K < Np; ++K)
					{
						const uint64_t Kp = Hash(Kl, 0x20 + static_cast<uint64_t>(K));
						const double Hk = Ht + 0.07 * (Unit(Hash(Kp, 1)) - 0.5);
						B.Box(Hinge + Dir * ((K + 0.5) * Wd / Np) + Up * (0.06 + 0.5 * Hk), Dir * (0.5 * Wd / Np - 0.007), Nr * 0.014, Up * (0.5 * Hk), Varied(Wood, 0.86 + 0.26 * Unit(Hash(Kp, 2)), 0.02), 16);
					}
					// (its ledges and its brace on the yard's side)
					const double In = Dot(Nr, Fc.N) < 0.0 ? 1.0 : -1.0;
					for (int K = 0; K < 2; ++K)
						B.Box(Hinge + Dir * (0.5 * Wd) + Nr * (In * 0.027) + Up * (K == 0 ? 0.4 : Ht - 0.28), Dir * (0.5 * Wd - 0.02), Nr * 0.013, Up * 0.04, Scaled(Wood, 0.9));
					B.Beam(Hinge + Dir * 0.05 + Nr * (In * 0.027) + Up * 0.45, Hinge + Dir * (Wd - 0.05) + Nr * (In * 0.027) + Up * (Ht - 0.33), 0.07, 0.024, Scaled(Wood, 0.86), Nr);
				}
			}

			/** Whether a place is well inside the rooms. */
			bool InRoom(const Plan& P, double X, double Y, double In) { return (X < P.Main.X1() - In && Y < P.Main.Y1() - In) || (X > P.Wing.X0() + In && Y < P.Wing.Y1() - In); }

			/** The yard's floor of beaten earth, in cells of about Step: paler where feet have packed it from the gate to the
			 * doors, grey with ash round the oven, dark where the jars drip; the gate's sill. */
			void Floor(Pen& B, const Plan& P, double Step)
			{
				const Block& Yd = P.Yard;
				const double Ax = Yd.Hx - 0.2, Ay = Yd.Hy - 0.2;
				const int NX = std::max(1, static_cast<int>(std::ceil(2.0 * Ax / Step))), NY = std::max(1, static_cast<int>(std::ceil(2.0 * Ay / Step)));
				const V3 Gt = MidOf(Yd, Yd.Gaps[0]), Da = MidOf(P.Main, P.Main.Gaps[0]), Db = MidOf(P.Wing, P.Wing.Gaps[0]), Jr = JarAt(P, 0);
				auto Reach = [](V3 Q, V3 A, V3 C) {
					const V3 D = C - A;
					const V3 E = Q - (A + D * Clamp01(Dot(Q - A, D) / std::max(1e-9, Dot(D, D))));
					return std::hypot(E.X, E.Y);
				};
				auto Of = [&](double X, double Y) {
					const V3 Q{ X, Y, 0.0 };
					Col C = Mixed(P.Earth, P.Dust, 0.4 + 0.35 * Noise01(X / 1.3, Y / 1.3, P.S.Key ^ 0xF1));
					C = Scaled(C, 0.94 + 0.1 * Noise01(X / 0.4, Y / 0.4, P.S.Key ^ 0xF2));
					C = Mixed(C, Scaled(P.Dust, 1.03), 0.55 * Smooth(0.75, 0.2, std::min(Reach(Q, Gt, Da), Reach(Q, Gt, Db))));
					C = Mixed(C, Rgb(126, 118, 110), 0.55 * Smooth(1.25, 0.6, std::hypot(X - P.OvenX, Y - P.OvenY)));
					return Mixed(C, Scaled(P.Earth, 0.7), 0.5 * Smooth(0.9, 0.3, std::hypot(X - Jr.X - 0.3, Y - Jr.Y)));
				};
				std::vector<uint32_t> Id(static_cast<size_t>((NX + 1) * (NY + 1)));
				for (int I = 0; I <= NX; ++I)
					for (int J = 0; J <= NY; ++J)
					{
						const double X = Yd.Cx - Ax + 2.0 * Ax * I / NX, Y = Yd.Cy - Ay + 2.0 * Ay * J / NY;
						Id[static_cast<size_t>(I * (NY + 1) + J)] = B.Vert(V3{ X, Y, 0.01 * Noise(X / 0.8, Y / 0.8, P.S.Key ^ 0xF3) }, Up, Of(X, Y));
					}
				for (int I = 0; I < NX; ++I)
					for (int J = 0; J < NY; ++J)
					{
						if (InRoom(P, Yd.Cx - Ax + 2.0 * Ax * (I + 0.5) / NX, Yd.Cy - Ay + 2.0 * Ay * (J + 0.5) / NY, 0.3)) continue;
						const size_t A = static_cast<size_t>(I * (NY + 1) + J), C = static_cast<size_t>((I + 1) * (NY + 1) + J);
						B.Tri(Id[A], Id[C], Id[C + 1], Up);
						B.Tri(Id[A], Id[C + 1], Id[A + 1], Up);
					}
				// (and out through the gate, a ramp of trodden earth down to the ground outside)
				const double Gw = 0.5 * Yd.Gaps[0].W + 0.02, Ya = Yd.Y1() - 0.25, Yb = Yd.Y1() + 0.45;
				B.Quad(V3{ Gt.X - Gw, Ya, 0.012 }, V3{ Gt.X + Gw, Ya, 0.012 }, V3{ Gt.X + Gw, Yb, -0.3 }, V3{ Gt.X - Gw, Yb, -0.3 }, Of(Gt.X - Gw, Ya), Of(Gt.X + Gw, Ya), Of(Gt.X + Gw, Ya), Of(Gt.X - Gw, Ya), Up);
			}

			/** What stands in and on the house, each at its own fineness. */
			void Fittings(Pen& B, const Plan& P, bool bFine)
			{
				int N = 0;
				for (int I = 0; I < 2; ++I)
				{
					const Block& Bk = BlockOf(P, I);
					for (const Gap& G : Bk.Gaps)
					{
						if (G.Look == 3) continue; // (a niche: no leaf, no shutter)
						const uint64_t Seed = Hash(P.S.Key, 0xA00 + static_cast<uint64_t>(N++));
						// (the wing's door, and an upper room's onto its roof, never painted)
						const bool bBare = I == 1 || G.Face == 3;
						if (!bFine) Blank(B, P, Bk, G, bBare);
						else if (G.What == 1) Door(B, P, Bk, G, Seed, bBare);
						else Window(B, P, Bk, G, Seed);
						if ((P.bMud || P.bBand) && !(G.What == 1 && G.Face == 3)) Surround(B, P, Bk, G, Seed, bFine ? 0.012 : 0.055, bFine ? 0.22 : 1.5);
					}
					Pipes(B, P, Bk, bFine);
				}
				for (const Plan::Scab& S : P.Scabs)
				{
					Bricks(B, P, S, bFine);
					if (bFine && S.bFallen) Fallen(B, P, S);
				}
				Stair(B, P, bFine);
				Bench(B, P, bFine);
				Oven(B, P, bFine);
				Jars(B, P, bFine);
				Gate(B, P, bFine);
				if (P.bShade) Shade(B, P, bFine);
				if (!bFine) return;
				BeamEnds(B, P, P.Main, 0, P.Main.Zr - 0.13, Hash(P.S.Key, 0xBE0));
				BeamEnds(B, P, P.Main, 2, P.Main.Zr - 0.13, Hash(P.S.Key, 0xBE1));
				BeamEnds(B, P, P.Wing, 2, P.Wing.Zr - 0.13, Hash(P.S.Key, 0xBE2));
				if (P.bTwo)
				{
					BeamEnds(B, P, P.Main, 0, 2.95, Hash(P.S.Key, 0xBE3));
					BeamEnds(B, P, P.Main, 2, 2.95, Hash(P.S.Key, 0xBE4));
				}
				for (const Plan::Crack& Ck : P.Cracks) CrackLine(B, P, Ck);
				if (P.bNiche) Niches(B, P);
				if (P.bRug) Rug(B, P);
				if (!P.bTwo) Ladder(B, P);
				RoofThings(B, P);
			}

			void Near(Pen& B, const Plan& P)
			{
				Shell(B, P, P.Main, 0.22, 0.24, 4, true, true);
				Shell(B, P, P.Wing, 0.22, 0.24, 4, true, true);
				Shell(B, P, P.Yard, 0.24, 0.26, 5, true, true);
				Deck(B, P, P.Main, 0.4);
				Deck(B, P, P.Wing, 0.4);
				Floor(B, P, 0.45);
				Fittings(B, P, true);
			}
			void Mid(Pen& B, const Plan& P)
			{
				Shell(B, P, P.Main, 1.1, 1.0, 1, false, false);
				Shell(B, P, P.Wing, 1.1, 1.0, 1, false, false);
				Shell(B, P, P.Yard, 1.3, 1.0, 1, false, false);
				Deck(B, P, P.Main, 2.0);
				Deck(B, P, P.Wing, 2.0);
				Floor(B, P, 2.4);
				Fittings(B, P, false);
			}
			/** From far off: its two rooms and its yard's wall as boxes, the yard's floor, the main room's door. */
			void Far(Pen& B, const Plan& P)
			{
				const double Bot = -std::max(0.25, P.S.Below);
				for (const Block* Bk : { &P.Main, &P.Wing })
				{
					const Col W = WallCol(P, *Bk, At(*Bk, 0, 0.0), 0.6 * Bk->Zp, 0), Top = Mixed(P.Roof, W, 0.3);
					const double In = 0.5 * P.Bt * Bk->Zp;
					B.Box(V3{ Bk->Cx, Bk->Cy, 0.5 * (Bk->Zp + Bot) }, V3{ Bk->Hx - In, 0.0, 0.0 }, V3{ 0.0, Bk->Hy - In, 0.0 }, V3{ 0.0, 0.0, 0.5 * (Bk->Zp - Bot) }, W, 16, &Top);
				}
				const Block& Yd = P.Yard;
				const Col Wc = WallCol(P, Yd, At(Yd, 0, 0.0), 0.6 * Yd.Zp, 0);
				auto Run = [&](double Xa, double Ya, double Xb, double Yb) {
					B.Box(V3{ 0.5 * (Xa + Xb), 0.5 * (Ya + Yb), 0.5 * (Yd.Zp + Bot) }, V3{ 0.5 * (Xb - Xa), 0.0, 0.0 }, V3{ 0.0, 0.5 * (Yb - Ya), 0.0 }, V3{ 0.0, 0.0, 0.5 * (Yd.Zp - Bot) }, Wc, 16);
				};
				const double Gx = MidOf(Yd, Yd.Gaps[0]).X, Gw = 0.5 * Yd.Gaps[0].W, T = Yd.Tp;
				Run(Yd.X0(), P.Main.Y1() - 0.1, Yd.X0() + T, Yd.Y1());
				Run(Yd.X1() - T, P.Wing.Y1() - 0.1, Yd.X1(), Yd.Y1());
				Run(Yd.X0(), Yd.Y1() - T, Gx - Gw, Yd.Y1());
				Run(Gx + Gw, Yd.Y1() - T, Yd.X1(), Yd.Y1());
				const Col Fl = Mixed(P.Earth, P.Dust, 0.6);
				B.Quad1(V3{ Yd.X0() + T, P.Wing.Y1(), 0.0 }, V3{ Yd.X1() - T, P.Wing.Y1(), 0.0 }, V3{ Yd.X1() - T, Yd.Y1() - T, 0.0 }, V3{ Yd.X0() + T, Yd.Y1() - T, 0.0 }, Fl, Up);
				B.Box(V3{ Gx, Yd.Y1() - 0.5 * T, 0.9 }, V3{ Gw, 0.0, 0.0 }, V3{ 0.0, 0.03, 0.0 }, V3{ 0.0, 0.0, 0.8 }, Scaled(Mixed(P.Wood, Rgb(150, 142, 130), 0.45), 0.8), 16);
				if (P.bShade) B.Box(V3{ Yd.X1() - 1.4, Yd.Y1() - 1.25, Yd.Zp + 0.2 }, V3{ 1.35, 0.0, 0.0 }, V3{ 0.0, 1.2, 0.0 }, V3{ 0.0, 0.0, 0.06 }, Scaled(Rgb(188, 168, 128), 0.85));
				const Gap& Dr = P.Main.Gaps[0];
				const double Dx = MidOf(P.Main, Dr).X, Dy = P.Main.Y1() + 0.03;
				B.Quad1(V3{ Dx - 0.5 * Dr.W, Dy, Dr.Z0 }, V3{ Dx + 0.5 * Dr.W, Dy, Dr.Z0 }, V3{ Dx + 0.5 * Dr.W, Dy, Dr.Z1 }, V3{ Dx - 0.5 * Dr.W, Dy, Dr.Z1 }, Scaled(P.Paint, 0.75), V3{ 0.0, 1.0, 0.0 });
			}

			void Build(const Spec& S, Levels& Out)
			{
				Out = Levels();
				const Plan P = Make(S);
				{
					Pen B(Out.Near);
					Near(B, P);
				}
				{
					Pen B(Out.Mid);
					Mid(B, P);
				}
				{
					Pen B(Out.Far);
					Far(B, P);
				}
				if (P.Side < 0.0)
					for (LandField::Mesh* M : { &Out.Near, &Out.Mid, &Out.Far }) MirrorX(*M);
			}
		}
	}

	double EavesOf(const Spec& S) { return S.Kind == 10 ? Dsr::Make(S).Main.Zr : S.Kind == 7 ? Izb::Make(S).Zt : S.Kind == 8 ? Kht::Make(S).Zw : S.Kind == 9 ? Brn::Make(S).Sh : MakePlan(S).E; }
	double RidgeOf(const Spec& S)
	{
		if (S.Kind == 10)
		{
			const Dsr::Plan P = Dsr::Make(S);
			return P.Main.Zp + P.Main.Horn; // (its main room's parapet, to its horns' tips)
		}
		if (S.Kind == 9) return Brn::Make(S).Ct;
		if (S.Kind == 8)
		{
			const Kht::Plan P = Kht::Make(S);
			return Kht::TopZ(P, P.Top) + P.Rho * (1.0 - P.CosP); // (the ridge's roll over the slopes' top)
		}
		return S.Kind == 7 ? Izb::Make(S).Ridge : MakePlan(S).Ridge;
	}

	void Holes(const Spec& S, std::vector<Hole>& Out)
	{
		Out.clear();
		if (S.Kind == 10)
		{
			// (the desert's house: its rooms' windows, 0, and doors, 1; its yard's gate, 18; turned with the house where it is
			// mirrored. For the pictures: its oven, 19; its stair, 20; a spout and the run under it, 21; its shade, 22; its
			// water jars, 23; a place its plaster has come away down from under a parapet, 24; what lies on the wing's roof,
			// 25; a wall's top never plastered, 26; a foot the damp has stripped, 27; a niche over its door, 31; the rug
			// over its wing's parapet, 34)
			const Dsr::Plan Q = Dsr::Make(S);
			for (const Dsr::Block* Bk : { &Q.Main, &Q.Wing, &Q.Yard })
				for (const Dsr::Gap& G : Bk->Gaps)
				{
					const Dsr::Frame Fc = Dsr::FaceOf(*Bk, G.Face);
					const V3 M = Fc.C + Fc.T * G.U;
					Hole H;
					H.Face = Q.Side < 0.0 && G.Face % 2 == 1 ? 4 - G.Face : G.Face;
					H.X = Q.Side * M.X;
					H.Y = M.Y;
					H.Z0 = G.What == 2 ? 0.0 : G.Z0;
					H.Z1 = G.What == 2 ? Bk->Zp : G.Z1;
					H.Wide = G.W;
					H.What = G.What == 2 ? 18 : G.Look == 3 ? 31 : G.What;
					Out.push_back(H);
				}
			auto Mark = [&](int F, double X, double Y, double Z0, double Z1, double Wide, int What) {
				Hole H;
				H.Face = Q.Side < 0.0 && F % 2 == 1 ? 4 - F : F;
				H.X = Q.Side * X;
				H.Y = Y;
				H.Z0 = Z0;
				H.Z1 = Z1;
				H.Wide = Wide;
				H.What = What;
				Out.push_back(H);
			};
			Mark(0, Q.OvenX, Q.OvenY, 0.0, 0.8, 1.3, 19);
			Mark(0, Q.Main.X1() + Q.Land + 0.5 * (Q.Steps - 1) * Q.Run, Q.Wing.Y1() + Q.StairW, 0.0, Q.Wing.Zr, Q.StairW, 20);
			for (const Dsr::Block* Bk : { &Q.Main, &Q.Wing })
				for (const Dsr::Spout& Sp : Bk->Spouts)
				{
					const Dsr::Frame Fc = Dsr::FaceOf(*Bk, Sp.F);
					const V3 M = Fc.C + Fc.T * Sp.U;
					Mark(Sp.F, M.X, M.Y, Bk->Zr - 1.6, Bk->Zr, 0.3, 21);
				}
			if (Q.bShade) Mark(0, Q.Yard.X1() - 1.4, Q.Yard.Y1() - 1.2, 0.0, Q.Yard.Zp + 0.3, 2.5, 22);
			{
				const V3 J = Dsr::JarAt(Q, 0);
				Mark(0, J.X + 0.25, J.Y, 0.0, 0.7, 1.0, 23);
			}
			for (const Dsr::Plan::Scab& Sc : Q.Scabs)
			{
				const Dsr::Frame Fc = Dsr::FaceOf(Dsr::BlockOf(Q, Sc.Blk), Sc.F);
				const V3 M = Fc.C + Fc.T * Sc.U;
				const int Kinds[3] = { 27, 24, 26 };
				Mark(Sc.F, M.X, M.Y, std::max(0.0, Sc.Z0), Sc.Z1, Sc.Hang == 2 ? 3.0 : Sc.Pm1 - Sc.Pm0, Kinds[std::clamp(Sc.Hang, 0, 2)]);
			}
			if (Q.bRug) Mark(0, Q.Wing.X1() - 1.85, Q.Wing.Y1(), Q.Wing.Zp - 0.82, Q.Wing.Zp, 0.8, 34);
			Mark(0, Q.Wing.Cx, Q.Wing.Cy, Q.Wing.Zr, Q.Wing.Zr, 1.2, 25);
			return;
		}
		if (S.Kind == 9)
		{
			// (the burnt-out house: nothing to see through; for the pictures, its stove's mouth, 15, its chimney, 16, and its stove's side to the room, 17)
			const Brn::Plan Q = Brn::Make(S);
			const Brn::Mouths Mo;
			Hole H;
			H.Face = Q.My > 0.0 ? 0 : 2;
			H.X = Q.Sx;
			H.Y = Q.Sy + Q.My * Q.Sb;
			H.Z0 = Mo.Z0;
			H.Z1 = Mo.Z1 + Mo.Hw;
			H.Wide = 2.0 * Mo.Hw;
			H.What = 15;
			Out.push_back(H);
			H.X = Q.Cx + 0.5 * (Q.Ct - Q.Sh) * Q.Lean;
			H.Y = Q.Cy;
			H.Z0 = Q.Cz0;
			H.Z1 = Q.Ct;
			H.Wide = 2.0 * Q.Cw;
			H.What = 16;
			Out.push_back(H);
			// (17: the stove's side to the room's middle)
			H.Face = Q.Sx > 0.0 ? 3 : 1;
			H.X = Q.Sx + (Q.Sx > 0.0 ? -Q.Sa : Q.Sa);
			H.Y = Q.Sy;
			H.Z0 = 0.0;
			H.Z1 = Q.Sh;
			H.Wide = 2.0 * Q.Sb;
			H.What = 17;
			Out.push_back(H);
			return;
		}
		if (S.Kind == 7)
		{
			const Izb::Plan Q = Izb::Make(S);
			for (const Izb::Gap& G : Q.Gaps)
			{
				const Izb::Wall W = Izb::WallOf(Q, G.Face);
				const V3 M = Izb::On(W, G.U, 0.0, Q.R);
				Hole H;
				H.Face = G.Face;
				H.X = M.X;
				H.Y = M.Y;
				H.Z0 = G.Z0;
				H.Z1 = G.Z1;
				H.Wide = G.W;
				H.What = G.What;
				Out.push_back(H);
			}
			return;
		}
		if (S.Kind == 8)
		{
			const Kht::Plan Q = Kht::Make(S);
			for (const Kht::Gap& G : Q.Gaps)
			{
				const Kht::Face Fc = Kht::FaceOf(Q, G.Face);
				const V3 M = Fc.C + Fc.T * G.U;
				Hole H;
				H.Face = G.Face;
				H.X = M.X;
				H.Y = M.Y;
				H.Z0 = G.Z0;
				H.Z1 = G.Z1;
				H.Wide = G.W;
				H.What = G.What;
				Out.push_back(H);
			}
			return;
		}
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
		// (a village house's render come away, 13, and the strikes in it, 14)
		auto Mark = [&](int F, double A, double Z0, double Z1, double Wide, int What) {
			const V3 M = At(FaceOf(P, F), A, 0.0, 0.0);
			Hole H;
			H.Face = F;
			H.X = M.X;
			H.Y = M.Y;
			H.Z0 = Z0;
			H.Z1 = Z1;
			H.Wide = Wide;
			H.What = What;
			Out.push_back(H);
		};
		for (const Plan::Spall& Sp : P.Spalls) Mark(Sp.Face, Sp.A, Sp.Z - Sp.H, Sp.Z + Sp.H, 2.0 * Sp.W, 13);
		for (const Plan::Pock& Q : P.Pocks) Mark(Q.Face, Q.A, Q.Z, Q.Z, 2.0 * Q.R, 14);
	}

	void Build(const Spec& S, Levels& Out)
	{
		if (S.Kind == 3)
		{
			BuildChurch(S, Out);
			return;
		}
		if (S.Kind == 7)
		{
			Izb::Build(S, Out);
			return;
		}
		if (S.Kind == 8)
		{
			Kht::Build(S, Out);
			return;
		}
		if (S.Kind == 9)
		{
			Brn::Build(S, Out);
			return;
		}
		if (S.Kind == 10)
		{
			Dsr::Build(S, Out);
			return;
		}
		Out = Levels();
		BuildPart(MakePlan(S), Out);
	}
}
