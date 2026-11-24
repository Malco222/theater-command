#include "Battle/LandHouse.h"

#include <algorithm>
#include <cmath>
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
			double Swing = 0.0; // the door's leaf swung in, radians
			bool bLeaf = true;  // the door's leaf there at all
		};

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
		};

		struct Plan
		{
			Spec S;
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
			bool bChimney[2] = { true, true }; // at -x and +x
			std::vector<double> Course; // the courses' lines, from the plinth's top up past the ridge
			std::vector<Opening> Ops;
			Look K;
			uint64_t Key = 1;
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
			if (!Fc.bGable) return P.EW;
			const V3 W = At(Fc, A, 0.0, 0.0);
			return DeckZ(P, W.Y) - 0.03;
		}

		/** The wall's top where it stands: whole, or broken down course by course where it was hit or burnt. */
		double TopOf(const Plan& P, const Face& Fc, int F, double A)
		{
			const double Intact = IntactTop(P, Fc, A);
			if (P.Hurt < 2) return Intact;
			const V3 W = At(Fc, A, 0.0, 0.0);
			const uint64_t K = P.Key;
			double Z = Intact;
			if (P.Hurt == 2)
			{
				const int B = P.Broken;
				if (Fc.bGable && Fc.End == B)
					Z = P.E - 2.3 - 1.7 * Noise01(A / 1.2, 11.0, K) - 0.3 * Noise01(A / 0.6, 13.0, K);
				else
				{
					const double In = B * W.X - (0.5 * P.L - 3.4);
					if (In > 0.0) Z = P.EW - std::min(1.0, In / 2.0) * (1.0 + 1.7 * Noise01(W.X / 1.1, 5.0 + F, K)) - 0.25 * Noise01(W.X / 0.6, 2.0 + F, K);
				}
			}
			else
			{
				const double Along = Fc.bGable ? W.Y : W.X;
				Z = P.E - 0.45 - 2.3 * Noise01(Along / 1.9, 3.0 + 7.0 * F, K) - 0.3 * Noise01(Along / 0.7, 7.0 + F, K);
				if (Fc.bGable && Fc.End == P.Keep) Z += (P.Ridge - P.E + 1.2) * std::exp(-Sq(W.Y / 1.7));
				// (the corners stand a little higher: the quoins hold)
				Z += 0.6 * std::exp(-Sq(std::min(A, Fc.Len - A) / 0.8));
			}
			Z = std::min(Z, Intact);
			if (Z >= Intact - 0.02) return Intact;
			return Quant(P, Z);
		}
		bool Damaged(const Plan& P, const Face& Fc, int F, double A) { return TopOf(P, Fc, F, A) < IntactTop(P, Fc, A) - 1e-6; }

		// ------------------------------------------------------------------ the plan from the spec

		Plan MakePlan(const Spec& S)
		{
			Plan P;
			P.S = S;
			P.Key = S.Key ? S.Key : 1;
			P.L = S.Long;
			P.D = S.Deep;
			P.Hurt = std::clamp(S.Hurt, 0, 3);
			Rng R(P.Key ^ 0xFA4A0);
			const double Pitch = (S.Roof == 1 ? 47.0 : 52.0) * Pi / 180.0, Kick = Pitch - 21.0 * Pi / 180.0;
			P.Tan = std::tan(Pitch);
			P.TanK = std::tan(Kick);
			P.SinP = std::sin(Pitch);
			P.CosP = std::cos(Pitch);
			P.SinK = std::sin(Kick);
			P.CosK = std::cos(Kick);
			P.E = std::max(1, S.Storeys) * 2.75 + 0.35;
			P.EW = P.E + 0.12;
			P.Vb = 0.5 * P.D - 0.4;
			P.Ridge = P.EW + 0.03 + P.Vb * P.Tan + (0.5 * P.D - P.Vb) * P.TanK;
			P.Zb = P.Ridge - P.Vb * P.Tan;
			P.Ze = DeckZ(P, 0.5 * P.D + P.Over);
			P.Lk = (0.5 * P.D + P.Over - P.Vb) / P.CosK;
			P.Lm = P.Vb / P.CosP;
			P.Ls = P.Lk + P.Lm;
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
			const double Tone = R.Range(0.93, 1.05), Warm = R.Range(-0.03, 0.04);
			K.Stone = Varied(Rgb(178, 167, 146), Tone, Warm);
			K.Mortar = Mixed(K.Stone, Rgb(200, 194, 176), 0.55);
			K.Dressed = Mixed(K.Stone, Rgb(206, 198, 180), 0.55);
			K.Plaster = Rgb(170, 162, 146);
			K.Roof = S.Roof == 1 ? Varied(Rgb(146, 84, 62), R.Range(0.92, 1.06), 0.0) : Varied(Rgb(80, 84, 94), R.Range(0.94, 1.06), 0.0);
			K.Deck = S.Roof == 1 ? Rgb(60, 42, 34) : Rgb(36, 38, 42);
			K.Boards = Rgb(96, 80, 62);
			static const Col Paints[5] = { Rgb(96, 110, 118), Rgb(92, 106, 88), Rgb(110, 72, 60), Rgb(166, 156, 134), Rgb(104, 114, 104) };
			K.Shutter = Paints[R.Below(5)];
			K.Door = R.Next() < 0.5 ? Rgb(84, 66, 50) : Scaled(K.Shutter, 0.86);
			K.Frame = R.Next() < 0.6 ? Rgb(184, 180, 168) : Mixed(K.Shutter, Rgb(184, 180, 168), 0.3);
			K.Glass = Rgb(46, 52, 60);
			K.Sky = Rgb(150, 160, 172);
			K.Iron = Rgb(64, 50, 40);
			K.Zinc = Rgb(122, 126, 124);
			K.Ridge = Rgb(136, 82, 60);
			K.Pot = Rgb(148, 90, 64);
			K.Char = Rgb(30, 26, 23);
			// (the walls far off: the stones and their mortar as the eye mixes them, as the near level's lit pillows average)
			{
				Col Sum;
				for (uint64_t I = 0; I < 256; ++I)
				{
					Col C = Varied(K.Stone, 0.86 + 0.24 * Unit(Mix(I * 77 + 5)), 0.0);
					const double U = Unit(Mix(I * 91 + 3));
					if (U < 0.12) C = Mixed(C, Rgb(152, 126, 94), 0.5);
					else if (U < 0.2) C = Mixed(C, Rgb(150, 150, 146), 0.4);
					Sum = { Sum.R + C.R / 256.0, Sum.G + C.G / 256.0, Sum.B + C.B / 256.0 };
				}
				K.Wall = Scaled(Mixed(Sum, K.Mortar, 0.18), 0.93);
			}
			K.Soot = Rgb(40, 36, 33);
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
			P.HoleSide = R.Next() < 0.5 ? -1 : 1;
			P.HoleX = R.Range(-0.5 * P.L + 2.6, 0.5 * P.L - 2.6);
			P.HoleS = R.Range(0.38, 0.62) * P.Ls;
			P.HoleRX = R.Range(1.1, 1.5);
			P.HoleRS = R.Range(0.9, 1.2);

			// The openings: the front in bays, windows over windows, the door in one of the middle bays; the back with
			// fewer; a gable without a chimney a small attic window, one with a ground-floor window.
			const int Storeys = std::max(1, S.Storeys);
			const double GZ0 = Snap(P, 0.95), GZ1 = Snap(P, 2.3);
			const double DZ1 = Snap(P, 2.65);
			auto UpperZ = [&](int Row, double& Z0, double& Z1) {
				Z0 = Snap(P, Row * 2.75 + 0.85);
				Z1 = Snap(P, Row * 2.75 + 2.1);
				if (Z1 > P.EW - 0.55) Z1 = Quant(P, P.EW - 0.55);
			};
			for (int F = 0; F < 4; ++F)
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

		/** The weather on a wall at a place: damp and green at its foot, streaks from the sills and the gutters' ends,
		 * lichen, a fire's soot above the openings and over the inside. */
		Col Weathered(const Plan& P, const Face& Fc, int F, double A, double Z, Col C, bool bFine = false)
		{
			const uint64_t K = P.Key;
			C = Mixed(C, Rgb(96, 98, 78), 0.55 * Smooth(1.1, 0.0, Z));
			if (bFine) C = Scaled(C, 1.0 - 0.07 * Smooth(0.2, 0.75, Noise01(A / 0.22, Z / 2.6, K ^ (0x57 + F)))); // rain streaks down the face
			else C = Scaled(C, 0.975);
			const double Lichen = Smooth(0.62, 0.82, Noise01(A / 0.9 + 3.0 * F, Z / 0.7, K ^ 0x11C));
			C = Mixed(C, Rgb(174, 172, 132), 0.3 * Lichen);
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				// (the sills' ends drip: two dark streaks under each)
				if (O.What != 1 && Z < O.Z0)
					for (double S : { O.A0 + 0.06, O.A1 - 0.06 })
						C = Scaled(C, 1.0 - 0.22 * std::exp(-Sq((A - S) / 0.07)) * Smooth(O.Z0 - 2.2, O.Z0 - 0.1, Z));
				if (O.bCharred && Z > O.Z1 - 0.2)
				{
					const double Rise = Z - O.Z1;
					const double Wide = 0.5 * (O.A1 - O.A0) + 0.15 + 0.35 * std::max(0.0, Rise);
					const double S = std::exp(-Sq((A - 0.5 * (O.A0 + O.A1)) / Wide)) * std::exp(-std::max(0.0, Rise) / 2.2);
					C = Mixed(C, P.K.Soot, 0.85 * S);
				}
			}
			// (the gutters' ends spill down the long walls' corners)
			if (!Fc.bGable)
				for (double S : { 0.25, Fc.Len - 0.25 })
					C = Scaled(C, 1.0 - 0.16 * std::exp(-Sq((A - S) / 0.18)) * Smooth(0.0, P.EW, Z));
			if (P.Hurt == 3) C = Mixed(C, P.K.Soot, 0.25 * Smooth(P.EW - 3.5, P.EW, Z));
			if (P.Hurt == 2)
			{
				const V3 W = At(Fc, A, Z, 0.0);
				C = Mixed(C, P.K.Soot, 0.35 * Smooth(0.5 * P.L - 4.5, 0.5 * P.L - 1.0, P.Broken * W.X) * Smooth(1.0, 4.0, Z));
			}
			return C;
		}

		/** A stone's own colour: the limestone varied, some stones ferruginous, some greyer. */
		Col StoneOwn(const Plan& P, uint64_t H)
		{
			Col C = Varied(P.K.Stone, 0.86 + 0.24 * Unit(H), (Unit(Mix(H)) - 0.5) * 0.08);
			const double U = Unit(Mix(H ^ 0x5A));
			if (U < 0.12) C = Mixed(C, Rgb(152, 126, 94), 0.5);
			else if (U < 0.2) C = Mixed(C, Rgb(150, 150, 146), 0.4);
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

		/** A rubble stone: its corners in the mortar's plane, its middle swelling out, so the low sun picks each out. */
		void Pillow(Pen& B, const Plan& P, const Face& Fc, int F, double A0, double A1, double Z0, double ZT0, double ZT1, uint64_t H)
		{
			const double Gap = 0.008 + 0.005 * Unit(H);
			// (each corner drawn in by its own amount, so the stones are rough and the joints uneven)
			double J[8];
			for (int K = 0; K < 8; ++K) J[K] = Gap + 0.022 * Sq(Unit(Hash(H, 0x10 + static_cast<uint64_t>(K))));
			const double CA[4] = { A0 + J[0], A1 - J[1], A1 - J[2], A0 + J[3] };
			const double CZ[4] = { Z0 + J[4], Z0 + J[5], ZT1 - J[6], ZT0 - J[7] };
			double MA = 0.25 * (CA[0] + CA[1] + CA[2] + CA[3]), MZ = 0.25 * (CZ[0] + CZ[1] + CZ[2] + CZ[3]);
			MA += (Unit(Mix(H ^ 1)) - 0.5) * 0.3 * (A1 - A0);
			MZ += (Unit(Mix(H ^ 2)) - 0.5) * 0.25 * (ZT0 - Z0);
			const double Bulge = 0.009 + 0.014 * Unit(Mix(H ^ 3));
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

		/** The opening's depth to its frame (or through the wall where the room is open). */
		double RevealOf(const Plan& P, const Opening& O, bool bInside)
		{
			if (bInside && (!O.bGlass || O.What == 1)) return P.Thick;
			return O.What == 1 ? 0.16 : 0.23;
		}

		void WallNear(Pen& B, const Plan& P, int F, bool bInside)
		{
			const Face Fc = FaceOf(P, F);
			const int CornerStart = (F + 3) % 4, CornerEnd = F;
			auto Top = [&](double A) { return TopOf(P, Fc, F, A); };
			// The stones, course by course, round what is dressed.
			for (size_t I = 0; I + 1 < P.Course.size(); ++I)
			{
				const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
				if (Fc.bGable ? Z0 > P.Ridge : Z0 >= P.EW - 1e-6) break;
				std::vector<std::pair<double, double>> Res;
				if (Z1 <= P.EW + 1e-6)
				{
					Res.push_back({ -1.0, QuoinLen(P, CornerStart, I, false) });
					Res.push_back({ Fc.Len - QuoinLen(P, CornerEnd, I, true), Fc.Len + 1.0 });
				}
				for (const Opening& O : P.Ops)
				{
					if (O.Face != F) continue;
					if (Z0 >= O.Z0 - 1e-6 && Z1 <= O.Z1 + 1e-6) Res.push_back({ O.A0 - JambLen(P, I, -1), O.A1 + JambLen(P, I, 1) });
					else if (std::fabs(Z0 - O.Z1) < 1e-6) Res.push_back({ O.A0 - LintelOver, O.A1 + LintelOver });
				}
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
						double Ln = R.Range(0.2, 0.66) * (Z0 < 1.2 ? 1.15 : 1.0);
						if (F1 - A - Ln < 0.16) Ln = F1 - A;
						if (Ln > 0.85) Ln = 0.5 * (F1 - A);
						const double SA0 = A, SA1 = A + Ln;
						A = SA1;
						// (stepped where the wall is broken: a stone only under the top at both its ends)
						const double TA = Top(SA0 + 0.01), TB = Top(SA1 - 0.01);
						const bool bRake = Fc.bGable && Z1 > P.EW - 1e-6 && !Damaged(P, Fc, F, SA0 + 0.01) && !Damaged(P, Fc, F, SA1 - 0.01);
						double ZT0 = Z1, ZT1 = Z1;
						if (bRake)
						{
							ZT0 = std::min(Z1, TA);
							ZT1 = std::min(Z1, TB);
							if (std::max(ZT0, ZT1) - Z0 < 0.07) continue;
							ZT0 = std::max(ZT0, Z0 + 0.035);
							ZT1 = std::max(ZT1, Z0 + 0.035);
						}
						else if (Z1 > std::min(TA, TB) + 1e-6) continue;
						const uint64_t H = Hash(P.Key ^ 0x570E, static_cast<uint64_t>(F * 4096) + I, static_cast<uint64_t>(std::llround(SA0 * 100.0)));
						if (!bRake && Z1 - Z0 > 0.25 && Unit(H) < 0.33)
						{
							const double ZM = Z0 + (0.32 + 0.36 * Unit(Mix(H))) * (Z1 - Z0);
							const double AM = SA0 + (0.35 + 0.3 * Unit(Mix(H ^ 7))) * (SA1 - SA0);
							Pillow(B, P, Fc, F, SA0, SA1, Z0, ZM, ZM, H ^ 0x11);
							Pillow(B, P, Fc, F, SA0, AM, ZM, Z1, Z1, H ^ 0x22);
							Pillow(B, P, Fc, F, AM, SA1, ZM, Z1, Z1, H ^ 0x33);
						}
						else Pillow(B, P, Fc, F, SA0, SA1, Z0, ZT0, ZT1, H);
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
				if (Z1 <= Top(0.02) + 1e-6) Block(B, Fc, -0.018, Q0 - 0.008, Z0 + 0.005, Z1 - 0.005, 0.018, C0);
				if (Z1 <= Top(Fc.Len - 0.02) + 1e-6) Block(B, Fc, Fc.Len - Q1 + 0.008, Fc.Len + 0.018, Z0 + 0.005, Z1 - 0.005, 0.018, C1);
			}
			// The openings' dressed jambs, lintels and sills.
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				const double TopHere = std::min(Top(O.A0 - 0.2), Top(O.A1 + 0.2));
				for (size_t I = 0; I + 1 < P.Course.size(); ++I)
				{
					const double Z0 = P.Course[I], Z1 = P.Course[I + 1];
					if (Z1 > TopHere + 1e-6) break;
					const uint64_t H = Hash(P.Key, 0x1A, I * 64 + static_cast<uint64_t>(std::llround(O.A0 * 10.0)));
					const Col C = Varied(P.K.Dressed, 0.95 + 0.09 * Unit(H), 0.0);
					if (Z0 >= O.Z0 - 1e-6 && Z1 <= O.Z1 + 1e-6)
					{
						Block(B, Fc, O.A0 - JambLen(P, I, -1) + 0.008, O.A0, Z0 + 0.005, Z1 - 0.005, 0.016, Weathered(P, Fc, F, O.A0 - 0.15, Z0, C));
						Block(B, Fc, O.A1, O.A1 + JambLen(P, I, 1) - 0.008, Z0 + 0.005, Z1 - 0.005, 0.016, Weathered(P, Fc, F, O.A1 + 0.15, Z0, Varied(C, 1.02, 0.0)));
					}
					else if (std::fabs(Z0 - O.Z1) < 1e-6)
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
						else Block(B, Fc, O.A0 - LintelOver + 0.008, O.A1 + LintelOver - 0.008, Z0 + 0.005, Z1 - 0.005, 0.018, LC);
					}
				}
				if (O.What != 1 && O.Z0 < TopHere)
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
			bool bBroken = false;
			for (double A = 0.05; A < Fc.Len; A += 0.1)
				if (Damaged(P, Fc, F, A)) bBroken = true;
			if (bBroken)
				for (double A = 0.0; A < Fc.Len - 0.01; A += 0.02)
					if (std::fabs(Top(A) - Top(A + 0.02)) > 1e-6) AB.push_back(A + 0.01);
			std::sort(AB.begin(), AB.end());
			AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), AB.end());
			std::sort(ZB.begin(), ZB.end());
			ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.012; }), ZB.end());
			const bool bInnerAll = bInside && P.Hurt >= 2;
			const double InnerFrom = P.Hurt == 1 ? std::max(1, P.S.Storeys) * 2.75 + 0.1 : 0.0;
			for (size_t I = 0; I + 1 < AB.size(); ++I)
			{
				const double A0 = AB[I], A1 = AB[I + 1], AM = 0.5 * (A0 + A1);
				const double TM = Top(AM);
				const bool bCut = Damaged(P, Fc, F, AM);
				for (size_t J = 0; J + 1 < ZB.size(); ++J)
				{
					const double Z0 = ZB[J], Z1 = ZB[J + 1], ZM = 0.5 * (Z0 + Z1);
					bool bHole = false;
					for (const Opening& O : P.Ops)
						if (O.Face == F && AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
					if (bHole) continue;
					double T0 = Z1, T1 = Z1;
					if (bCut) T0 = T1 = std::min(Z1, TM);
					else if (Fc.bGable)
					{
						T0 = std::min(Z1, IntactTop(P, Fc, A0));
						T1 = std::min(Z1, IntactTop(P, Fc, A1));
					}
					if (T0 <= Z0 + 1e-6 && T1 <= Z0 + 1e-6) continue;
					T0 = std::max(T0, Z0);
					T1 = std::max(T1, Z0);
					auto MC = [&](double A, double Z) { return Weathered(P, Fc, F, A, Z, Varied(P.K.Mortar, 0.96 + 0.06 * Noise(A / 0.7, Z / 0.7, P.Key ^ 0x30), 0.0), true); };
					B.Quad(At(Fc, A0, Z0, 0.0), At(Fc, A1, Z0, 0.0), At(Fc, A1, T1, 0.0), At(Fc, A0, T0, 0.0), MC(A0, Z0), MC(A1, Z0), MC(A1, T1), MC(A0, T0), Fc.N);
					// (the inside's face, limewashed plaster, sooted where it burnt)
					const double IZ0 = std::max(Z0, InnerFrom);
					if (bInside && (bInnerAll || (P.Hurt == 1 && T0 > IZ0)) && std::max(T0, T1) > IZ0)
					{
						const double I0 = std::max(T0, IZ0), I1 = std::max(T1, IZ0);
						auto PC = [&](double A, double Z) {
							Col C = Varied(P.K.Plaster, 0.95 + 0.08 * Noise(A / 1.3, Z / 1.1, P.Key ^ 0x9A), 0.0);
							if (P.Hurt == 3) C = Mixed(C, P.K.Soot, 0.35 + 0.5 * Smooth(0.5, 4.0, Z) * Noise01(A / 0.8, Z / 0.6, P.Key ^ 0x9B));
							else if (P.Hurt == 2) C = Mixed(C, P.K.Soot, 0.25 * Noise01(A / 0.8, Z / 0.6, P.Key ^ 0x9C));
							return Mixed(C, Rgb(70, 66, 58), 0.4 * Smooth(0.8, 0.0, Z));
						};
						B.Quad(At(Fc, A0, IZ0, -P.Thick), At(Fc, A1, IZ0, -P.Thick), At(Fc, A1, I1, -P.Thick), At(Fc, A0, I0, -P.Thick), PC(A0, IZ0), PC(A1, IZ0), PC(A1, I1), PC(A0, I0), Fc.N * -1.0);
					}
					// (a broken top: the wall's rubble core, then the step down to the next cell)
					if (bCut && J + 1 < ZB.size() && T0 < Z1 - 1e-6 && T0 > Z0 + 1e-6)
					{
						const Col Core = Mixed(P.K.Mortar, Rgb(120, 110, 96), 0.45 + 0.3 * Noise01(AM / 0.3, 1.0, P.Key ^ 0xC0E));
						B.Quad1(At(Fc, A0, T0, 0.0), At(Fc, A1, T0, 0.0), At(Fc, A1, T0, -P.Thick), At(Fc, A0, T0, -P.Thick), P.Hurt == 3 ? Mixed(Core, P.K.Soot, 0.3) : Core, Up);
					}
				}
				// The step between this cell's top and the next one's.
				if (bCut || Damaged(P, Fc, F, std::min(Fc.Len, A1 + 0.005)))
					if (I + 2 < AB.size())
					{
						const double TN = Top(0.5 * (A1 + AB[I + 2]));
						if (std::fabs(TN - TM) > 1e-6)
						{
							const double Lo = std::min(TN, TM), Hi = std::max(TN, TM);
							const Col Core = Mixed(P.K.Mortar, Rgb(118, 108, 94), 0.5);
							B.Quad1(At(Fc, A1, Lo, 0.0), At(Fc, A1, Hi, 0.0), At(Fc, A1, Hi, -P.Thick), At(Fc, A1, Lo, -P.Thick), Scaled(Core, 0.9), Fc.T * (TN < TM ? 1.0 : -1.0));
						}
					}
			}
			// The plinth: a projecting band of larger, darker stone at the foot, its top weathered back, cut by the door.
			{
				std::vector<std::pair<double, double>> Runs{ { -0.05, Fc.Len + 0.05 } };
				for (const Opening& O : P.Ops)
					if (O.Face == F && O.What == 1)
					{
						const double E1 = Runs.back().second;
						Runs.back().second = O.A0;
						Runs.push_back({ O.A1, E1 });
					}
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
			if (!Fc.bGable)
				for (double A : { 0.85, Fc.Len - 0.85 })
				{
					const double Z = std::max(1, P.S.Storeys) > 1 ? 2.75 : 1.9;
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

		/** A window's frame and two casements with their glazing bars and panes, its shutters; or the door, its fanlight
		 * and its step. Then the reveal round it. */
		void OpeningNear(Pen& B, const Plan& P, const Opening& O, bool bInside)
		{
			const int F = O.Face;
			const Face Fc = FaceOf(P, F);
			const double TopHere = std::min(TopOf(P, Fc, F, O.A0 - 0.1), TopOf(P, Fc, F, O.A1 + 0.1));
			const double Z1 = std::min(O.Z1, TopHere);
			const double Dp = RevealOf(P, O, bInside);
			const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
			// The reveal: its sides, its head (a door's floor is its threshold).
			B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, Z1, -Dp), At(Fc, O.A0, Z1, 0.0), Scaled(Rev, 0.95), Fc.T);
			B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A1, Z1, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
			if (Z1 >= O.Z1 - 1e-6) B.Quad1(At(Fc, O.A0, Z1, 0.0), At(Fc, O.A1, Z1, 0.0), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Scaled(Rev, 0.7), Up * -1.0);
			if (O.What == 1)
				B.Quad1(At(Fc, O.A0, 0.02, 0.05), At(Fc, O.A1, 0.02, 0.05), At(Fc, O.A1, 0.02, -Dp), At(Fc, O.A0, 0.02, -Dp), Varied(P.K.Dressed, 0.82, 0.0), Up);
			const uint64_t H = Hash(P.Key, 0x0BE, static_cast<uint64_t>(F * 1000) + static_cast<uint64_t>(std::llround(O.A0 * 20.0)));
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
						B.Quad1(At(Fc, PA0, ZD + 0.065, -Dp), At(Fc, PA1, ZD + 0.065, -Dp), At(Fc, PA1, Z1 - 0.03, -Dp), At(Fc, PA0, Z1 - 0.03, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.15 + 0.2 * Unit(Mix(H + K))), Fc.N);
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
					B.Box(Mid, Across * (0.5 * LW), Up * (0.5 * LH), Out * 0.03, Scaled(P.K.Door, 0.6), 32);
					const V3 Front = Mid + Out * 0.0315;
					const int N = 6;
					for (int K = 0; K < N; ++K)
					{
						const double U0 = -0.5 * LW + LW * K / N + 0.004, U1 = -0.5 * LW + LW * (K + 1) / N - 0.004;
						const Col C = Varied(P.K.Door, 0.9 + 0.14 * Unit(Mix(H + 31 * K)), 0.0);
						B.Quad(Front + Across * U0 - Up * (0.5 * LH), Front + Across * U1 - Up * (0.5 * LH), Front + Across * U1 + Up * (0.5 * LH), Front + Across * U0 + Up * (0.5 * LH), Scaled(C, 0.8), Scaled(C, 0.8), C, C, Out);
					}
					for (double Z : { 0.32, LH - 0.32 })
						B.Box(Front + Across * (-0.5 * LW + 0.36) + Up * (Z - 0.5 * LH) + Out * 0.003, Across * 0.34, Up * 0.024, Out * 0.003, P.K.Iron, 0);
					B.Box(Front + Across * (0.5 * LW - 0.1) + Up * (1.05 - 0.5 * LH) + Out * 0.012, Across * 0.02, Up * 0.06, Out * 0.012, P.K.Iron, 0);
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
							Col G = Mixed(P.K.Glass, P.K.Sky, 0.14 + 0.36 * Unit(PH) * (0.5 + 0.5 * (R + 1) / Rows));
							const bool bBroke = static_cast<int>(Unit(Mix(PH)) * 6.0) < O.Cracked;
							if (!O.bGlass || bBroke) G = Rgb(16, 15, 14);
							B.Quad(At(Fc, GA[S][0], PZ0, -Dp), At(Fc, GA[S][1], PZ0, -Dp), At(Fc, GA[S][1], PZ1, -Dp), At(Fc, GA[S][0], PZ1, -Dp), Scaled(G, 0.9), Scaled(G, 0.9), G, G, Fc.N);
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

		void RoofNear(Pen& B, const Plan& P, bool bInside)
		{
			if (P.Hurt == 3) return;
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
			{
				if (!ChimneyAt(P, End)) continue;
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
					const V3 Foot{ XM, S * 0.2, ZTop + 0.2 };
					const Col Pot = Mixed(Varied(P.K.Pot, 0.92 + 0.1 * Unit(Hash(P.Key, 0x9071, static_cast<uint64_t>(S + 2 + (End > 0) * 4))), 0.0), P.K.Soot, 0.25);
					B.Tube(Foot, Foot + Up * 0.42, 0.115, 0.09, 8, Pot, false);
					B.Tube(Foot + Up * 0.42, Foot + Up * 0.48, 0.115, 0.115, 8, Pot, true, true);
					B.Tube(Foot + Up * 0.46, Foot + Up * 0.485, 0.085, 0.085, 8, Flue, true);
				}
				// (the mortar fillet where the slates meet the stack)
				for (int S = -1; S <= 1; S += 2)
				{
					const RoofPt F0 = RoofAt(P, S, XI, SlopeAt(P, HY), 2.0 * P.SlateT), F1 = RoofAt(P, S, XO, SlopeAt(P, HY), 2.0 * P.SlateT);
					const V3 W0{ XI, S * HY, F0.P.Z + 0.06 }, W1{ XO, S * HY, F1.P.Z + 0.06 };
					B.Quad1(F0.P + V3{ 0.0, S * 0.05, 0.0 }, F1.P + V3{ 0.0, S * 0.05, 0.0 }, W1, W0, Mixed(P.K.Mortar, Rgb(110, 106, 96), 0.4), V3{ 0.0, S * 1.0, 0.6 });
				}
			}
			// The gutters on their brackets under the eaves' edge, their downpipes to the ground, a water butt.
			for (int Side = -1; Side <= 1; Side += 2)
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
					Col K = Varied(Mixed(P.K.Stone, Rgb(104, 92, 76), 0.3 + 0.35 * Noise01(A * 2.1, R * 1.9, Seed ^ 3)), (bBlocks ? 0.8 : 1.0) + 0.25 * Unit(Hash(Seed, static_cast<uint64_t>(R * NA + A))), 0.0);
					// (a plain level's heap a little lighter: it stands for the lit faces of the stones the near one lays on it)
					if (P.Hurt == 3) K = Mixed(K, P.K.Soot, 0.35);
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
			if (P.Hurt == 1) Floor(Storeys * 2.75 + 0.1, true);
			if (P.Hurt == 2)
			{
				for (int S = 1; S < Storeys; ++S) Floor(S * 2.75 + 0.05, false);
				Floor(Storeys * 2.75 + 0.1, true);
			}
			if (P.Hurt == 3)
			{
				// charred joist stubs in the long walls, their pockets dark
				for (int S = 1; S <= Storeys; ++S)
				{
					const double Z = S * 2.75 - 0.15;
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
				// fallen beams, charred, from the walls' tops down into the house
				Rng R(P.Key ^ 0xBEA);
				for (int K = 0; K < 4; ++K)
				{
					const double X0 = R.Range(-IX + 0.5, IX - 0.5), X1 = X0 + R.Range(-2.5, 2.5);
					const int S = R.Next() < 0.5 ? -1 : 1;
					const V3 A{ X0, S * (IY + 0.2), R.Range(2.2, 3.4) }, Bp{ std::clamp(X1, -IX + 0.3, IX - 0.3), -S * R.Range(0.0, IY), R.Range(0.3, 0.8) };
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
				const double Z = Storeys * 2.75 + 0.1;
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

		/** A wall's face in cells: Step apart along it, its openings dark, its quoins and surrounds painted on (Near: a
		 * middle level's reveals, sills, shutters and frames). */
		void WallPlain(Pen& B, const Plan& P, int F, double Step, bool bMid)
		{
			const Face Fc = FaceOf(P, F);
			auto Top = [&](double A) { return TopOf(P, Fc, F, A); };
			std::vector<double> AB{ 0.0, Fc.Len }, ZB{ bMid ? 0.36 : -P.S.Below, P.EW };
			for (double A = Step; A < Fc.Len - 0.2; A += Step) AB.push_back(A);
			if (bMid) ZB.push_back(-P.S.Below);
			if (Fc.bGable)
			{
				AB.push_back(0.5 * Fc.Len);
				AB.push_back(0.5 * Fc.Len - P.Vb);
				AB.push_back(0.5 * Fc.Len + P.Vb);
				ZB.push_back(P.Ridge + 0.1);
			}
			for (const Opening& O : P.Ops)
				if (O.Face == F && bMid)
				{
					AB.push_back(O.A0);
					AB.push_back(O.A1);
					ZB.push_back(O.Z0);
					ZB.push_back(O.Z1);
					AB.push_back(O.A0 - 0.28);
					AB.push_back(O.A1 + 0.28);
				}
			if (bMid)
			{
				AB.push_back(0.4);
				AB.push_back(Fc.Len - 0.4);
				for (double Z = 1.5; Z < P.EW - 0.3; Z += 1.5) ZB.push_back(Z);
			}
			for (double A = 0.0; A < Fc.Len - 0.01; A += 0.02)
				if ((Damaged(P, Fc, F, A) || Damaged(P, Fc, F, A + 0.02)) && std::fabs(Top(A) - Top(A + 0.02)) > 1e-6) AB.push_back(A + 0.01);
			std::sort(AB.begin(), AB.end());
			AB.erase(std::unique(AB.begin(), AB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.03; }), AB.end());
			std::sort(ZB.begin(), ZB.end());
			ZB.erase(std::unique(ZB.begin(), ZB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.03; }), ZB.end());
			const bool bInner = P.Hurt >= 2;
			for (size_t I = 0; I + 1 < AB.size(); ++I)
			{
				const double A0 = AB[I], A1 = AB[I + 1], AM = 0.5 * (A0 + A1);
				const double TM = Top(AM);
				const bool bCut = Damaged(P, Fc, F, AM);
				for (size_t J = 0; J + 1 < ZB.size(); ++J)
				{
					const double Z0 = ZB[J], Z1 = ZB[J + 1], ZM = 0.5 * (Z0 + Z1);
					bool bHole = false;
					for (const Opening& O : P.Ops)
						if (bMid && O.Face == F && AM > O.A0 && AM < O.A1 && ZM > O.Z0 && ZM < O.Z1) bHole = true;
					if (bHole) continue;
					double T0 = Z1, T1 = Z1;
					if (bCut) T0 = T1 = std::min(Z1, TM);
					else if (Fc.bGable)
					{
						T0 = std::min(Z1, IntactTop(P, Fc, A0));
						T1 = std::min(Z1, IntactTop(P, Fc, A1));
					}
					if (T0 <= Z0 + 1e-6 && T1 <= Z0 + 1e-6) continue;
					T0 = std::max(T0, Z0);
					T1 = std::max(T1, Z0);
					// (the stone and its mortar as the eye mixes them; dressed quoins and surrounds paler)
					bool bDressed = bMid && (AM < 0.4 || AM > Fc.Len - 0.4) && ZM > 0.36 && ZM < P.EW;
					for (const Opening& O : P.Ops)
						if (bMid && O.Face == F && ((AM > O.A0 - 0.28 && AM < O.A1 + 0.28 && ZM > O.Z0 && ZM < O.Z1 + 0.3)))
							bDressed = true;
					const double Plinth = (bMid ? ZM < 0.36 : false) ? 1.0 : 0.0;
					auto WC = [&](double A, double Z) {
						Col C = Varied(P.K.Wall, 0.97 + 0.06 * Noise(A / 0.9, Z / 0.8, P.Key ^ 0x3A1), 0.0);
						if (bDressed) C = Varied(P.K.Dressed, 0.98, 0.0);
						if (Plinth > 0.0) C = Varied(Rgb(140, 132, 116), 0.95, 0.0);
						return Weathered(P, Fc, F, A, Z, C);
					};
					const double Pr = bDressed ? 0.012 : 0.0;
					B.Quad(At(Fc, A0, Z0, Pr), At(Fc, A1, Z0, Pr), At(Fc, A1, T1, Pr), At(Fc, A0, T0, Pr), WC(A0, Z0), WC(A1, Z0), WC(A1, T1), WC(A0, T0), Fc.N);
					if (bInner && Z0 >= -0.01)
					{
						const Col PC = P.Hurt == 3 ? Mixed(P.K.Plaster, P.K.Soot, 0.6) : Mixed(P.K.Plaster, P.K.Soot, 0.2);
						B.Quad1(At(Fc, A0, Z0, -P.Thick), At(Fc, A1, Z0, -P.Thick), At(Fc, A1, T1, -P.Thick), At(Fc, A0, T0, -P.Thick), PC, Fc.N * -1.0);
					}
					if (bCut && T0 < Z1 - 1e-6 && T0 > Z0 + 1e-6)
						B.Quad1(At(Fc, A0, T0, 0.0), At(Fc, A1, T0, 0.0), At(Fc, A1, T0, -P.Thick), At(Fc, A0, T0, -P.Thick), Mixed(P.K.Mortar, Rgb(118, 108, 94), 0.5), Up);
				}
				if ((bCut || Damaged(P, Fc, F, std::min(Fc.Len, A1 + 0.005))) && I + 2 < AB.size())
				{
					const double TN = Top(0.5 * (A1 + AB[I + 2]));
					if (std::fabs(TN - TM) > 1e-6)
						B.Quad1(At(Fc, A1, std::min(TN, TM), 0.0), At(Fc, A1, std::max(TN, TM), 0.0), At(Fc, A1, std::max(TN, TM), -P.Thick), At(Fc, A1, std::min(TN, TM), -P.Thick), Rgb(110, 100, 88), Fc.T * (TN < TM ? 1.0 : -1.0));
				}
			}
			// The openings: a middle level's reveals, glass and frame, shutters as flat leaves, the door; a far level's dark.
			for (const Opening& O : P.Ops)
			{
				if (O.Face != F) continue;
				const double Z1 = std::min(O.Z1, std::min(Top(O.A0 - 0.1), Top(O.A1 + 0.1)));
				const double W = O.A1 - O.A0;
				const bool bThrough = bInner && (!O.bGlass || O.What == 1);
				if (!bMid)
				{
					// (far off: painted on the wall, the openings dark, the shutters beside them)
					Col C = O.What == 1 ? (O.bLeaf ? P.K.Door : Rgb(16, 15, 14)) : O.Shutters == 1 ? P.K.Shutter : Mixed(P.K.Glass, P.K.Sky, 0.12);
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
				const double Dp = bThrough ? P.Thick : O.What == 1 ? 0.16 : 0.23;
				const Col Rev = Varied(P.K.Dressed, 0.9, 0.0);
				B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A0, Z1, -Dp), At(Fc, O.A0, Z1, 0.0), Scaled(Rev, 0.95), Fc.T);
				B.Quad1(At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A1, Z1, 0.0), Scaled(Rev, 0.95), Fc.T * -1.0);
				if (Z1 >= O.Z1 - 1e-6) B.Quad1(At(Fc, O.A0, Z1, 0.0), At(Fc, O.A1, Z1, 0.0), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Scaled(Rev, 0.7), Up * -1.0);
				B.Quad1(At(Fc, O.A0, O.Z0, 0.0), At(Fc, O.A1, O.Z0, 0.0), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A0, O.Z0, -Dp), Rev, Up);
				if (O.What == 1)
				{
					if (O.bLeaf && O.Swing < 0.2) B.Quad1(At(Fc, O.A0, 0.0, -Dp), At(Fc, O.A1, 0.0, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A0, Z1 - 0.42, -Dp), P.K.Door, Fc.N);
					else if (!bThrough) B.Quad1(At(Fc, O.A0, 0.0, -Dp), At(Fc, O.A1, 0.0, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A0, Z1 - 0.42, -Dp), Rgb(16, 15, 14), Fc.N);
					if (!O.bCharred) B.Quad1(At(Fc, O.A0, Z1 - 0.42, -Dp), At(Fc, O.A1, Z1 - 0.42, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.2), Fc.N);
					B.Box(At(Fc, 0.5 * (O.A0 + O.A1), -0.6, 0.27), Fc.T * (0.5 * W + 0.16), Fc.N * 0.22, Up * 0.44, Varied(P.K.Dressed, 0.86, 0.0), 0);
					continue;
				}
				if (O.bGlass)
				{
					B.Quad1(At(Fc, O.A0, O.Z0, -Dp), At(Fc, O.A1, O.Z0, -Dp), At(Fc, O.A1, Z1, -Dp), At(Fc, O.A0, Z1, -Dp), Mixed(P.K.Glass, P.K.Sky, 0.15), Fc.N);
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
					B.Quad1(At(Fc, A, -P.S.Below, 0.05), At(Fc, A1, -P.S.Below, 0.05), At(Fc, A1, 0.33, 0.05), At(Fc, A, 0.33, 0.05), Weathered(P, Fc, F, A, 0.1, Rgb(136, 128, 112)), Fc.N);
				}
			}
		}

		/** The roof in bands of courses (Rows of them to a band), cut where its hole and its broken end are. */
		void RoofPlain(Pen& B, const Plan& P, int Rows, double CellX, bool bMid)
		{
			if (P.Hurt == 3) return;
			const double X0 = -0.5 * P.L - P.GO, X1 = 0.5 * P.L + P.GO;
			const double Band = P.Gauge * Rows;
			// (the bands' edges: every Rows courses, and the eaves' kick)
			std::vector<double> SB{ P.Lk, P.Ls };
			for (double S = 0.0; S < P.Ls - 0.05; S += Band) SB.push_back(S);
			std::sort(SB.begin(), SB.end());
			SB.erase(std::unique(SB.begin(), SB.end(), [](double X, double Y) { return std::fabs(X - Y) < 0.05; }), SB.end());
			const int NB = static_cast<int>(SB.size()) - 1;
			const int NX = std::max(1, static_cast<int>(std::lround((X1 - X0) / CellX)));
			for (int Side = -1; Side <= 1; Side += 2)
				for (int K = 0; K < NB; ++K)
				{
					const double S0 = SB[static_cast<size_t>(K)], S1 = SB[static_cast<size_t>(K) + 1], SM = 0.5 * (S0 + S1);
					for (int I = 0; I < NX; ++I)
					{
						const double XA = X0 + (X1 - X0) * I / NX, XB = X0 + (X1 - X0) * (I + 1) / NX, XM = 0.5 * (XA + XB);
						if (!DeckThere(P, Side, XM, SM)) continue;
						const uint64_t H = Hash(P.Key, 0xBA4D, static_cast<uint64_t>(K * 256 + I * 2 + (Side > 0)));
						Col C = Varied(P.K.Roof, (bMid ? 0.95 : 0.8) + 0.08 * Unit(H), 0.0);
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
			{
				if (!ChimneyAt(P, End)) continue;
				const double XI = End * (0.5 * P.L - 0.62), XO = End * (0.5 * P.L + P.GO);
				const double XM = 0.5 * (XI + XO), HX = 0.5 * std::fabs(XO - XI), HY = 0.48;
				const double ZBot = DeckZ(P, HY) - 0.3, ZTop = P.Ridge + 1.25;
				const Col Flue = Rgb(30, 27, 25);
				B.Box(V3{ XM, 0.0, 0.5 * (ZBot + ZTop) }, V3{ HX, 0.0, 0.0 }, V3{ 0.0, HY, 0.0 }, V3{ 0.0, 0.0, 0.5 * (ZTop - ZBot) }, Varied(P.K.Dressed, 0.9, 0.0), 16, &Flue);
				B.Box(V3{ XM, 0.0, ZTop + 0.06 }, V3{ HX + 0.07, 0.0, 0.0 }, V3{ 0.0, HY + 0.07, 0.0 }, V3{ 0.0, 0.0, 0.06 }, Mixed(P.K.Dressed, P.K.Soot, 0.45), 0, &Flue);
				if (bMid)
					for (int S = -1; S <= 1; S += 2)
						B.Box(V3{ XM, S * 0.2, ZTop + 0.36 }, V3{ 0.1, 0.0, 0.0 }, V3{ 0.0, 0.1, 0.0 }, V3{ 0.0, 0.0, 0.24 }, Mixed(P.K.Pot, P.K.Soot, 0.25), 16, &Flue);
			}
			if (bMid)
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
			if (P.Hurt >= 2) B.Quad1(V3{ -IX, -IY, 0.02 }, V3{ IX, -IY, 0.02 }, V3{ IX, IY, 0.02 }, V3{ -IX, IY, 0.02 }, P.Hurt == 3 ? Rgb(60, 54, 48) : Rgb(96, 88, 76), Up);
			if (P.Hurt == 1)
			{
				const double Z = std::max(1, P.S.Storeys) * 2.75 + 0.1;
				B.Quad1(V3{ -IX, -IY, Z }, V3{ IX, -IY, Z }, V3{ IX, IY, Z }, V3{ -IX, IY, Z }, P.K.Boards, Up);
			}
			if (P.Hurt == 2)
			{
				for (int S = 1; S <= std::max(1, P.S.Storeys); ++S)
				{
					const double Z = S * 2.75 + 0.05, XEnd = 0.5 * P.L - 2.6;
					const double XA = P.Broken > 0 ? -IX : -XEnd, XB = P.Broken > 0 ? XEnd : IX;
					B.Box(V3{ 0.5 * (XA + XB), 0.0, Z - 0.12 }, V3{ 0.5 * (XB - XA), 0.0, 0.0 }, V3{ 0.0, IY, 0.0 }, V3{ 0.0, 0.0, 0.12 }, P.K.Boards, 0);
				}
				Heap(B, P, V3{ P.Broken * (0.5 * P.L + 1.1), 0.0, -0.15 }, 2.3, 0.5 * P.D + 0.9, 1.5, 0.0, bMid ? 12 : 8, bMid ? 4 : 2, P.Key ^ 0x4EA9, false);
			}
			if (P.Hurt == 3) Heap(B, P, V3{ 0.0, 0.0, 0.0 }, IX - 0.6, IY - 0.3, 0.75, 0.0, bMid ? 12 : 8, bMid ? 3 : 2, P.Key ^ 0x4EAB, false);
			if (P.Hurt == 1 && bMid) Heap(B, P, V3{ std::clamp(P.HoleX, -0.5 * P.L + 1.5, 0.5 * P.L - 1.5), P.HoleSide * (0.5 * P.D + 0.75), -0.25 }, 1.1, 0.5, 0.28, 0.0, 8, 2, P.Key ^ 0x51E, false);
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
		Out = Levels();
		const Plan P = MakePlan(S);
		const bool bInside = P.Hurt >= 1;
		{
			Pen B(Out.Near);
			for (int F = 0; F < 4; ++F) WallNear(B, P, F, bInside);
			for (const Opening& O : P.Ops) OpeningNear(B, P, O, bInside);
			RoofNear(B, P, bInside);
			if (bInside) InsideNear(B, P);
		}
		{
			Pen B(Out.Mid);
			for (int F = 0; F < 4; ++F) WallPlain(B, P, F, 1.75, true);
			RoofPlain(B, P, 3, 2.4, true);
			if (bInside) InsidePlain(B, P, true);
		}
		{
			Pen B(Out.Far);
			for (int F = 0; F < 4; ++F) WallPlain(B, P, F, 3.6, false);
			RoofPlain(B, P, 40, P.Hurt == 0 ? 20.0 : 2.4, false);
			if (bInside) InsidePlain(B, P, false);
		}
	}
}
