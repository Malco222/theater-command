#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

/**
 * The ruined city (release 2, on trial "city" until the art director passes it and the Mac holds its frame rate): the
 * look bible's T4, bombed-out streets, rubble and the burnt shells of buildings, for every battle the rules fight in a
 * capital's or a city's streets (field.ts layCity: its core of blocks, its terraces, its suburbs, its industry and its
 * parks, the storeys of each built cell). The art director's sheet (art-director/battles/city-battle-look.md) sets it:
 *
 *   - rings from outside in: suburbs of detached houses in their gardens, terraces along their streets, a core of blocks
 *     three to five storeys high on a street grid with its squares, industry by itself, parks; one landmark (a spire, a
 *     dome or a station hall) seen from everywhere, so the player always knows which way the centre lies;
 *   - the region's style: brick and slate in the north and west, stone and plaster with tile roofs in the south, plaster
 *     and timber with plank and iron roofs further east;
 *   - the damage of a city fought over: roofs open to the rafters, walls broken ragged with floors showing (the breach
 *     clip rule: broken, never cut away), burnt shells soot-dark above their windows with the sky through them, rubble
 *     spilling into the streets as irregular chunks, never toy cubes; no two neighbours the same;
 *   - its ground: dark setts in the core's streets dusted pale with masonry, its courtyards ash and trodden earth, the
 *     suburbs' roads pale dust, its gardens and parks worn and torn; the city never ends in a hard edge in the middle of
 *     a field (beyond the field it runs on under the haze as a carpet of roofs).
 *
 * Generic plans only: no real city's streets. Every building is built here from boxes and lumps (no model file to read
 * or to miss), each slot's mesh at three levels (near; the middle from 90 m; plain from 300 m), each slot its own
 * name ("City..."), so no two of the scene's instanced kinds share an Unreal name (the jungle's crash, 25 November).
 * Plain C++, no engine, so the cloud test lays exactly what the game draws.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		// ------------------------------------------------------------------ the kit's slots

		/** Terrace and block buildings: 2 to 5 storeys (Row0 + (storeys - 2) * 4 + state). */
		constexpr int KRow0 = 0;
		/** A building's state: roof holed to the rafters; a burnt shell (no roof, no floors, soot over every window); half
		 * fallen (one end down to a storey, its floors hanging); a stump (its ground floor's walls ragged over a heap). */
		enum State : int
		{
			SHoled = 0,
			SGutted = 1,
			SHalf = 2,
			SStump = 3
		};
		enum Slot : int
		{
			KVilla0 = 16,   // the suburbs' detached houses: holed, burnt, half fallen, whole but for a few tiles
			KShed = 20,     // a works shed under its north-light roof, its glass gone
			KChimney = 21,  // a works chimney
			KTower = 22,    // a water tower
			KWarehouse = 23, // a gutted warehouse of five storeys
			KSpire = 24,    // the landmark: a church, its tower and spire
			KDome = 25,     // the landmark: a domed hall, its dome's skin half gone
			KStation = 26,  // the landmark: a station hall under its iron arches
			KHeap = 27,     // a rubble heap spilled into a street
			KScatter = 28,  // rubble lying low (under a quarter metre): a man steps over it
			KLoneWall = 29, // a facade standing alone, its building gone behind it
			KShellTree = 30, // a tree torn by shellfire
			KStump = 31,    // a tree's stump
			KLamp = 32,     // a street lamp
			KLampBent = 33, // a street lamp bent over
			KWreck = 34,    // a burnt-out motor car
			KSlots = 35
		};

		enum Clut : int
		{
			CBrick = 0,  // a few bricks, whole and broken
			CChunk = 1,  // a lump of masonry
			CTiles = 2,  // shards of roof tile or slate
			CTimber = 3, // a splintered beam's end, a lath
			CGrass = 4,  // a worn grass tuft
			CWeed = 5    // a dry weed
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		/** Frontage and depth of a block building of so many storeys (metres). */
		double FrontOf(int Storeys) { return Storeys <= 2 ? 9.6 : Storeys == 3 ? 13.2 : Storeys == 4 ? 16.0 : 19.5; }
		double DeepOf(int Storeys) { return Storeys <= 2 ? 9.0 : Storeys == 3 ? 11.0 : Storeys == 4 ? 12.5 : 13.5; }
		int BaysOf(int Storeys) { return Storeys <= 2 ? 2 : Storeys == 3 ? 3 : Storeys == 4 ? 4 : 5; }
		constexpr double GroundH = 3.7, StoreyH = 3.25;
		double EavesOf(int Storeys) { return GroundH + (Storeys - 1) * StoreyH; }

		// ------------------------------------------------------------------ the region's style

		enum Style : int
		{
			North, // brick and slate (north-west Europe, Britain)
			East,  // plaster over brick and timber, plank and iron roofs (Poland, Russia, the Ukraine, the Balkans' north)
			South  // stone and plaster, tile roofs (the Mediterranean, the deserts' towns, the tropics' colonial towns)
		};

		/** The colours of a style (sRGB as the eye sees them; the battle camera's grade greys the middles, so painted a
		 * little fuller; the art director's to check). */
		struct Paints
		{
			Tone Face[3];  // the walls' faces: three of the street's houses' own
			Tone Core;     // what a broken wall shows: brick, rubble stone
			Tone Trim;     // sills, cornices, quoins
			Tone Inner;    // the walls' inner faces, rooms' plaster
			Tone Roof;     // slate, tile or iron
			Tone Timber;   // rafters, joists, floors
			Tone Char;     // burnt timber
			Tone Soot;     // the soot over a burnt room's window
			Tone Dust;     // masonry dust lying
			Tone Rubble[3]; // the heaps' chunks
			bool bPlaster = false; // its faces plaster over the core (fallen in patches from the edges)
		};

		Paints PaintsOf(Style S)
		{
			Paints P;
			switch (S)
			{
			case North:
				P.Face[0] = Rgb(132, 74, 56);
				P.Face[1] = Rgb(118, 70, 58);
				P.Face[2] = Rgb(146, 92, 68);
				P.Core = Rgb(124, 76, 60);
				P.Trim = Rgb(170, 162, 146);
				P.Inner = Rgb(132, 124, 112);
				P.Roof = Rgb(76, 78, 84);
				P.Timber = Rgb(116, 94, 70);
				P.Char = Rgb(44, 38, 34);
				P.Soot = Rgb(36, 33, 31);
				P.Dust = Rgb(150, 142, 132);
				P.Rubble[0] = Rgb(116, 78, 62);
				P.Rubble[1] = Rgb(128, 120, 110);
				P.Rubble[2] = Rgb(88, 66, 56);
				break;
			case East:
				P.Face[0] = Rgb(164, 152, 130);
				P.Face[1] = Rgb(150, 146, 134);
				P.Face[2] = Rgb(170, 156, 122);
				P.Core = Rgb(124, 92, 76);
				P.Trim = Rgb(148, 142, 128);
				P.Inner = Rgb(128, 120, 108);
				P.Roof = Rgb(84, 86, 86);
				P.Timber = Rgb(104, 90, 74);
				P.Char = Rgb(42, 37, 33);
				P.Soot = Rgb(34, 31, 29);
				P.Dust = Rgb(152, 146, 134);
				P.Rubble[0] = Rgb(120, 90, 74);
				P.Rubble[1] = Rgb(136, 130, 118);
				P.Rubble[2] = Rgb(94, 88, 80);
				P.bPlaster = true;
				break;
			case South:
				P.Face[0] = Rgb(168, 156, 134);
				P.Face[1] = Rgb(178, 154, 114);
				P.Face[2] = Rgb(176, 166, 144);
				P.Core = Rgb(138, 124, 102);
				P.Trim = Rgb(176, 168, 150);
				P.Inner = Rgb(138, 128, 112);
				P.Roof = Rgb(132, 80, 60);
				P.Timber = Rgb(110, 92, 70);
				P.Char = Rgb(44, 39, 34);
				P.Soot = Rgb(36, 33, 30);
				P.Dust = Rgb(166, 158, 142);
				P.Rubble[0] = Rgb(142, 128, 106);
				P.Rubble[1] = Rgb(152, 142, 124);
				P.Rubble[2] = Rgb(118, 86, 68);
				P.bPlaster = true;
				break;
			}
			return P;
		}

		// ------------------------------------------------------------------ a small kit of shapes

		struct V3d
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		inline V3d operator+(const V3d& A, const V3d& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		inline V3d operator-(const V3d& A, const V3d& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		inline V3d operator*(const V3d& A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
		inline double Dot3(const V3d& A, const V3d& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		inline V3d Cross3(const V3d& A, const V3d& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		inline V3d Norm3(const V3d& A)
		{
			const double L = std::sqrt(Dot3(A, A));
			return L > 1e-12 ? A * (1.0 / L) : V3d{ 0.0, 0.0, 1.0 };
		}

		/** A six-faced solid from its eight corners (0-3 its foot, 4-7 its head, each four counter-clockwise from above when
		 * its axes are right-handed), each face lit flat, each corner painted by Ink(corner, face normal). No underside
		 * unless bUnder. */
		template <class InkFn>
		void Hexa(Writer& W, const V3d (&C)[8], bool bUnder, InkFn&& Ink)
		{
			static const int Faces[6][4] = { { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 1, 2, 6, 5 }, { 2, 3, 7, 6 }, { 3, 0, 4, 7 } };
			for (int F = 0; F < 6; ++F)
			{
				if (F == 0 && !bUnder) continue;
				const V3d& A = C[Faces[F][0]];
				const V3d N = Norm3(Cross3(C[Faces[F][1]] - A, C[Faces[F][2]] - A));
				uint32_t Id[4];
				for (int K = 0; K < 4; ++K)
				{
					const V3d& P = C[Faces[F][K]];
					Id[K] = W.Vert(P.X, P.Y, P.Z, N.X, N.Y, N.Z, Ink(P, N));
				}
				W.Tri(Id[0], Id[1], Id[2]);
				W.Tri(Id[0], Id[2], Id[3]);
			}
		}

		/** A box in a frame: origin O, axes U (along), V (across) and Up, from U0..U1, V0..V1, W0..W1 (V = Up x U keeps it
		 * right-handed: pass V so). */
		template <class InkFn>
		void Block(Writer& W, const V3d& O, const V3d& U, const V3d& V, const V3d& Up, double U0, double U1, double V0, double V1, double W0, double W1, bool bUnder, InkFn&& Ink)
		{
			const V3d C[8] = {
				O + U * U0 + V * V0 + Up * W0, O + U * U1 + V * V0 + Up * W0, O + U * U1 + V * V1 + Up * W0, O + U * U0 + V * V1 + Up * W0,
				O + U * U0 + V * V0 + Up * W1, O + U * U1 + V * V0 + Up * W1, O + U * U1 + V * V1 + Up * W1, O + U * U0 + V * V1 + Up * W1,
			};
			Hexa(W, C, bUnder, Ink);
		}

		/** A plain box along x/y/z, one colour, lit flat (the far level's blocks). */
		void Plain(Writer& W, double X0, double X1, double Y0, double Y1, double Z0, double Z1, const Tone& Side, const Tone& Top)
		{
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, X0, X1, Y0, Y1, Z0, Z1, false, [&](const V3d&, const V3d& N) { return N.Z > 0.5 ? Top : Side; });
		}

		/** A beam from A to B, Thick square, its colour. */
		void Beam(Writer& W, const V3d& A, const V3d& B, double Thick, const Tone& C)
		{
			const V3d U = Norm3(B - A);
			V3d Side = Cross3(V3d{ 0, 0, 1 }, U);
			if (Dot3(Side, Side) < 1e-6) Side = V3d{ 1, 0, 0 };
			Side = Norm3(Side);
			const V3d Up = Cross3(U, Side);
			const double L = std::sqrt(Dot3(B - A, B - A)), H = 0.5 * Thick;
			const Tone Lit = Varied(C, 1.08, 0.0), Dim = Varied(C, 0.8, 0.0);
			Block(W, A, U, Side, Up, 0.0, L, -H, H, -H, H, true, [&](const V3d&, const V3d& N) { return N.Z > 0.3 ? Lit : N.Z < -0.3 ? Dim : C; });
		}

		/** A lump of masonry of some 24 triangles: a squashed, tilted, irregular block, its foot a little sunk. */
		void Lump(Writer& W, double CX, double CY, double CZ, double R, double H, uint64_t S, const Tone& Lit, const Tone& Under)
		{
			const double Yaw = 2.0 * Pi * Unit(Hash(S, 1));
			const double TiltX = 0.5 * (Unit(Hash(S, 2)) - 0.5), TiltY = 0.5 * (Unit(Hash(S, 3)) - 0.5);
			const double Ca = std::cos(Yaw), Sa = std::sin(Yaw);
			auto Put = [&](double U, double V, double Z) {
				const double X = U * Ca - V * Sa, Y = U * Sa + V * Ca;
				return V3d{ CX + X, CY + Y, CZ + Z + TiltX * X + TiltY * Y };
			};
			// (a box with its corners pushed about: never a cube)
			V3d C[8];
			for (int K = 0; K < 8; ++K)
			{
				const double SU = (K == 1 || K == 2 || K == 5 || K == 6) ? 1.0 : -1.0;
				const double SV = (K == 2 || K == 3 || K == 6 || K == 7) ? 1.0 : -1.0;
				const double Pu = 0.6 + 0.7 * Unit(Hash(S, 10 + K)), Pv = 0.5 + 0.6 * Unit(Hash(S, 20 + K)), Pz = 0.7 + 0.6 * Unit(Hash(S, 30 + K));
				C[K] = Put(SU * R * Pu, SV * R * Pv, K < 4 ? -0.25 * H : H * Pz);
			}
			Hexa(W, C, false, [&](const V3d& P, const V3d& N) { return Mixed(Under, Lit, Clamp01(0.35 + 0.65 * N.Z + 0.15 * (P.Z - CZ) / std::max(H, 0.05))); });
		}

		/** A heap of rubble: a lumpy mound RX by RY, High at its crown, its skin the rubble's dust and broken brick, lumps on
		 * it (Lumps of them), a few beams thrown across it; Lod 1 fewer, Lod 2 the mound alone. Its foot runs out flush with
		 * the ground. */
		void Heap(Writer& W, double CX, double CY, double RX, double RY, double High, uint64_t S, const Paints& P, int Lod, int Lumps, int Beams)
		{
			const int Rings = Lod >= 2 ? 3 : Lod == 1 ? 4 : 6, Round = Lod >= 2 ? 8 : Lod == 1 ? 10 : 14;
			const Tone Skin = Varied(Mixed(P.Dust, P.Rubble[2], 0.5), 0.9, 0.0);
			std::vector<uint32_t> Id;
			const uint32_t Top = W.Vert(CX, CY, High, 0, 0, 1, Varied(Skin, 1.05, 0.0));
			auto HeightAt = [&](double T, double Th) {
				// (a mound with a ragged skin: its height falling off as a soft cone, pushed by the lumps under it)
				const double Push = 0.82 + 0.36 * Unit(Hash(S, static_cast<uint64_t>(std::lround(Th * 100.0)), static_cast<uint64_t>(std::lround(T * 100.0))));
				return High * (1.0 - T * T * (0.6 + 0.4 * T)) * (T >= 0.999 ? 0.0 : Push);
			};
			for (int R = 1; R <= Rings; ++R)
			{
				const double T = static_cast<double>(R) / Rings;
				for (int K = 0; K < Round; ++K)
				{
					const double Th = 2.0 * Pi * K / Round + 0.3 * R;
					const double Wob = 0.85 + 0.3 * Unit(Hash(S, 100 + R, static_cast<uint64_t>(K)));
					const double X = CX + RX * T * Wob * std::cos(Th), Y = CY + RY * T * Wob * std::sin(Th);
					const double Z = R == Rings ? -0.05 : HeightAt(T, Th);
					const double Slope = High / std::max(0.5, 0.5 * (RX + RY));
					// (its skin broken: each point's normal tipped a little its own way, so it reads as chunks, not a cloth)
					const double TipX = 0.7 * (Unit(Hash(S, 1800 + R, static_cast<uint64_t>(K))) - 0.5);
					const double TipY = 0.7 * (Unit(Hash(S, 1900 + R, static_cast<uint64_t>(K))) - 0.5);
					const V3d N = Norm3(V3d{ std::cos(Th) * Slope + TipX, std::sin(Th) * Slope + TipY, 1.0 });
					const double Mix = Unit(Hash(S, 300 + R, static_cast<uint64_t>(K)));
					const Tone C = Mix < 0.3 ? P.Rubble[0] : Mix < 0.6 ? P.Rubble[1] : Mix < 0.88 ? P.Rubble[2] : P.Char;
					const double Shade = 0.82 + 0.3 * Unit(Hash(S, 2000 + R, static_cast<uint64_t>(K)));
					Id.push_back(W.Vert(X, Y, Z, N.X, N.Y, N.Z, Varied(Mixed(Skin, C, 0.55 + 0.25 * T), Shade, 0.0)));
				}
			}
			for (int K = 0; K < Round; ++K) W.Tri(Top, Id[static_cast<size_t>(K)], Id[static_cast<size_t>((K + 1) % Round)]);
			for (int R = 0; R + 1 < Rings; ++R)
				for (int K = 0; K < Round; ++K)
				{
					const uint32_t A = Id[static_cast<size_t>(R * Round + K)], B = Id[static_cast<size_t>(R * Round + (K + 1) % Round)];
					const uint32_t C = Id[static_cast<size_t>((R + 1) * Round + (K + 1) % Round)], D = Id[static_cast<size_t>((R + 1) * Round + K)];
					W.Tri(A, D, C);
					W.Tri(A, C, B);
				}
			if (Lod >= 2) return;
			const int N = Lod == 1 ? Lumps / 3 : Lumps;
			for (int K = 0; K < N; ++K)
			{
				const double A = 2.0 * Pi * Unit(Hash(S, 500 + K)), T = std::sqrt(Unit(Hash(S, 600 + K)));
				const double X = CX + 0.95 * RX * T * std::cos(A), Y = CY + 0.95 * RY * T * std::sin(A);
				const double Z = HeightAt(T, A) * 0.9;
				const double Size = (0.22 + 0.62 * Unit(Hash(S, 700 + K))) * (1.0 - 0.4 * T);
				const double Pick = Unit(Hash(S, 800 + K));
				const Tone C = Pick < 0.45 ? P.Rubble[0] : Pick < 0.8 ? P.Rubble[1] : P.Rubble[2];
				Lump(W, X, Y, Z, Size, Size * (0.5 + 0.4 * Unit(Hash(S, 900 + K))), Hash(S, 1000 + K), Varied(C, 1.08, 0.0), Varied(C, 0.62, 0.0));
			}
			const int NB = Lod == 1 ? Beams / 2 : Beams;
			for (int K = 0; K < NB; ++K)
			{
				const double A = 2.0 * Pi * Unit(Hash(S, 1100 + K)), Len = 2.0 + 3.0 * Unit(Hash(S, 1200 + K));
				const double T = 0.5 * Unit(Hash(S, 1300 + K));
				const double X = CX + RX * T * std::cos(A), Y = CY + RY * T * std::sin(A), Z = HeightAt(T, A);
				const double Turn = 2.0 * Pi * Unit(Hash(S, 1400 + K)), Rise = 0.5 + 1.2 * Unit(Hash(S, 1500 + K));
				const V3d From{ X - 0.5 * Len * std::cos(Turn), Y - 0.5 * Len * std::sin(Turn), Z - 0.3 };
				const V3d To{ X + 0.5 * Len * std::cos(Turn), Y + 0.5 * Len * std::sin(Turn), Z + Rise };
				Beam(W, From, To, 0.16 + 0.08 * Unit(Hash(S, 1600 + K)), Unit(Hash(S, 1700 + K)) < 0.6 ? P.Char : P.Timber);
			}
		}

		// ------------------------------------------------------------------ walls with their windows, broken

		/** A hole blown through a wall: its middle (metres along, metres up) and half its size each way. */
		struct Hole
		{
			double S = 0.0, Z = 0.0, RS = 1.0, RZ = 1.0;
		};

		/** A wall to build: its outer face's line from A along T (unit, level) for Len metres, Out its outward normal
		 * (level), Thick deep; its foot at Foot (below the ground), its storeys from the floor (0) up, its bays of windows
		 * (none: a blank party wall), its top line (metres up at metres along: TopAt), how ragged its broken top is, and
		 * the holes through it. */
		struct WallLay
		{
			V3d A, T, Out;
			double Len = 10.0, Thick = 0.45, Foot = -1.0;
			double Skip = 0.0;       // its run built only from Skip to Len - Skip (an end wall between the long walls' ends, so no two faces lie in one plane at a corner)
			int Storeys = 3, Bays = 3;
			bool bShops = false;     // the ground floor's wide shop fronts (a main street's)
			bool bCornice = true;
			std::vector<std::pair<double, double>> Top; // (metres along, metres up), in order along it; none: the eaves plus a parapet
			double Rag = 0.0;        // its broken top steps up and down by up to this, brick by brick
			std::vector<Hole> Holes;
			std::vector<uint8_t> Burnt; // a storey burnt out (soot over its windows), by storey; none: none
			int Face = 0;            // which of the style's faces
			uint64_t Key = 1;
			int Lod = 0;
		};

		class Mason
		{
		public:
			Mason(Writer& InW, const Paints& InP) : W(InW), P(InP) {}

			double TopAt(const WallLay& L, double S) const
			{
				if (L.Top.empty()) return EavesOf(L.Storeys) + 0.7;
				if (S <= L.Top.front().first) return L.Top.front().second;
				for (size_t K = 1; K < L.Top.size(); ++K)
					if (S <= L.Top[K].first)
					{
						const double T = (S - L.Top[K - 1].first) / std::max(1e-6, L.Top[K].first - L.Top[K - 1].first);
						return Lerp(L.Top[K - 1].second, L.Top[K].second, T);
					}
				return L.Top.back().second;
			}

			/** The wall's face colour at (metres along, metres up): its stone or plaster, varied in broad patches, grimed
			 * low, streaked by the rain, plaster fallen from its edges to the core, soot over a burnt room's window. */
			Tone FaceAt(const WallLay& L, double S, double Z) const
			{
				const Tone& Base = P.Face[L.Face % 3];
				const double N1 = Noise(S / 2.7 + 0.37 * static_cast<double>(L.Key % 13), Z / 2.3, L.Key ^ 0xFA1);
				const double N2 = Noise(S / 0.9, Z / 6.0, L.Key ^ 0xFA2); // (rain streaks: long up and down)
				Tone C = Varied(Base, 1.0 + 0.07 * N1 + 0.04 * N2, 0.02 * N1);
				// (grime low down, the splash of the street; the ground floor's dirt over its height; the street's dust)
				C = Mixed(C, Varied(Base, 0.68, 0.03), 0.55 * Smooth(1.1, 0.1, Z) + 0.18 * Smooth(4.5, 0.5, Z));
				// (the war's own dirt: smoke and brick dust in broad patches, darker under a broken top, where the fire was)
				const double Smoke = Smooth(-0.2, 0.7, Noise(S / 5.5, Z / 4.0, L.Key ^ 0xFA4));
				const double UnderTop = L.Top.empty() ? 0.0 : Smooth(1.6, 0.0, TopAt(L, S) - Z);
				C = Mixed(C, Varied(P.Soot, 1.25, 0.0), 0.22 * Smoke + 0.4 * UnderTop * (L.Burnt.empty() ? 0.5 : 1.0));
				if (P.bPlaster)
				{
					// (the plaster come away from an edge: up from the foot, down from under the eaves, round a corner and
					// round a window's reveal, never a closed patch in the middle; its brick showing, the plaster's edge ragged)
					const double Edge = std::min({ S, L.Len - S, Z + 0.4, std::fabs(TopAt(L, S) - Z) });
					const double Bare = Smooth(0.15, -0.25, Noise(S / 1.6, Z / 1.1, L.Key ^ 0xFA3) + 0.55 - 0.42 * Smooth(0.0, 2.6, Edge));
					C = Mixed(C, Varied(P.Core, 0.95 + 0.06 * N2, 0.0), 0.75 * Bare);
				}
				// (soot over each burnt room's windows: a dark plume from its head up a storey and more, widest at its root)
				if (!L.Burnt.empty() && L.Bays > 0)
				{
					double Sooted = 0.0;
					const double Bay = L.Len / L.Bays, Win = WinOf(Bay);
					for (int K = 0; K < L.Storeys && K < static_cast<int>(L.Burnt.size()); ++K)
					{
						if (!L.Burnt[static_cast<size_t>(K)]) continue;
						const double Head = FloorZ(K) + StoreyHigh(K) - 0.55;
						const double Up = Z - (Head - 0.35);
						if (Up < 0.0 || Up > 4.2) continue;
						const double Spread = 0.55 * Win + 0.32 * Up;
						const double Along = std::fmod(S, Bay) - 0.5 * Bay;
						const double Across = Clamp01(1.0 - std::fabs(Along) / Spread);
						Sooted = std::max(Sooted, Across * std::exp(-Up / 1.7));
					}
					C = Mixed(C, P.Soot, 0.88 * Sooted);
				}
				return C;
			}

			static double WinOf(double Bay) { return std::clamp(0.44 * Bay, 1.0, 1.65); }
			static double FloorZ(int Storey) { return Storey == 0 ? 0.0 : GroundH + (Storey - 1) * StoreyH; }
			static double StoreyHigh(int Storey) { return Storey == 0 ? GroundH : StoreyH; }

			/** Build a wall. */
			void Build(const WallLay& L)
			{
				Lay = &L;
				const double Eaves = EavesOf(L.Storeys);
				const double Crown = L.Top.empty() ? Eaves + 0.7 : 0.0;
				(void)Crown;
				// The pieces: piers between the windows, the spandrels under and over each window, the plinth and the band
				// over the top storey; each cut to the wall's top and to its holes.
				if (L.Bays <= 0)
				{
					// a blank wall: courses a storey high, cut every 2.5 m
					const int Cols = std::max(1, static_cast<int>(std::ceil(L.Len / 2.6)));
					double Up = 0.0;
					double Hi = 0.0;
					for (size_t K = 0; K < L.Top.size(); ++K) Hi = std::max(Hi, L.Top[K].second);
					if (L.Top.empty()) Hi = Eaves + 0.7;
					Piece(0.0, L.Len, L.Foot, 0.0, false);
					for (int Row = 0; Up < Hi - 0.01; ++Row)
					{
						const double Next = std::min(Hi, Up + (Row == 0 ? GroundH : StoreyH));
						for (int C = 0; C < Cols; ++C) Piece(L.Len * C / Cols, L.Len * (C + 1) / Cols, Up, Next, false);
						Up = Next;
					}
					return;
				}
				const double Bay = L.Len / L.Bays, Win = WinOf(Bay);
				// the plinth, below the floor
				for (int B = 0; B < L.Bays; ++B) Piece(B * Bay, (B + 1) * Bay, L.Foot, 0.0, false);
				for (int K = 0; K < L.Storeys; ++K)
				{
					const double Z0 = FloorZ(K), H = StoreyHigh(K);
					for (int B = 0; B < L.Bays; ++B)
					{
						const double Mid = (B + 0.5) * Bay;
						// (a ground floor's shop front on a main street: wide and from near the pavement; a door in one bay
						// of the others')
						const bool bShop = K == 0 && L.bShops;
						const bool bDoor = K == 0 && !L.bShops && B == static_cast<int>(L.Key % static_cast<uint64_t>(L.Bays));
						const double WW = bShop ? 0.72 * Bay : bDoor ? std::min(1.3, Win) : Win;
						const double Sill = bShop ? 0.4 : bDoor ? 0.05 : K == 0 ? 0.95 : 0.88;
						const double Head = H - (bShop ? 0.6 : 0.55);
						const double W0 = Mid - 0.5 * WW, W1 = Mid + 0.5 * WW;
						// the pier to the left of the window (from the bay's edge; the first bay's from the corner)
						Piece(B * Bay, W0, Z0, Z0 + H, true);
						Piece(W1, (B + 1) * Bay, Z0, Z0 + H, true);
						Piece(W0, W1, Z0, Z0 + Sill, true);
						Piece(W0, W1, Z0 + Head, Z0 + H, true, true);
						// (its sill: a stone ledge under the window, proud of the face)
						if (L.Lod == 0 && !bShop && !bDoor) Ledge(W0 - 0.1, W1 + 0.1, Z0 + Sill - 0.08, Z0 + Sill, 0.08);
					}
				}
				// the band over the top storey: a parapet or the wall under the eaves, its cornice
				for (int B = 0; B < L.Bays; ++B) Piece(B * Bay, (B + 1) * Bay, Eaves, Eaves + 0.7, false);
				if (L.bCornice && L.Lod <= 1) Ledge(0.0, L.Len, Eaves + 0.05, Eaves + 0.3, 0.22);
				// a string course at the first floor
				if (L.Lod == 0 && L.Storeys >= 2) Ledge(0.0, L.Len, GroundH - 0.12, GroundH + 0.06, 0.07);
			}

		private:
			Writer& W;
			const Paints& P;
			const WallLay* Lay = nullptr;

			bool InHole(double S, double Z) const
			{
				for (const Hole& H : Lay->Holes)
				{
					// (each hole's edge ragged: its reach wanders brick by brick round it)
					const double A = std::atan2((Z - H.Z) / H.RZ, (S - H.S) / H.RS);
					const double Rag = 0.78 + 0.4 * Unit(Hash(Lay->Key ^ 0x401E, static_cast<uint64_t>(std::lround((A + Pi) * 6.0))));
					const double D = ((S - H.S) / H.RS) * ((S - H.S) / H.RS) + ((Z - H.Z) / H.RZ) * ((Z - H.Z) / H.RZ);
					if (D < Rag * Rag) return true;
				}
				return false;
			}
			bool CrossesHole(double S0, double S1, double Z0, double Z1) const
			{
				for (const Hole& H : Lay->Holes)
					if (S1 > H.S - 1.25 * H.RS && S0 < H.S + 1.25 * H.RS && Z1 > H.Z - 1.25 * H.RZ && Z0 < H.Z + 1.25 * H.RZ) return true;
				return false;
			}

			/** One box of wall from S0 to S1 along, Z0 to Z1 up; Cut its sides raw (broken), not a reveal. */
			void Emit(double S0, double S1, double Z0, double Z1, bool bCut, bool bUnder)
			{
				if (S1 - S0 < 0.02 || Z1 - Z0 < 0.02) return;
				const WallLay& L = *Lay;
				const V3d Up{ 0, 0, 1 };
				const V3d V = Cross3(Up, L.T); // (left of the run: into or out of the wall)
				const double Sign = Dot3(V, L.Out) >= 0.0 ? 1.0 : -1.0;
				const double V0 = Sign > 0.0 ? -L.Thick : 0.0, V1 = Sign > 0.0 ? 0.0 : L.Thick;
				const Tone Raw = P.Core;
				Block(W, L.A, L.T, V, Up, S0, S1, V0, V1, Z0, Z1, bUnder, [&](const V3d& Q, const V3d& N) {
					const double S = Dot3(Q - L.A, L.T);
					const double Facing = Dot3(N, L.Out);
					if (Facing > 0.5) return FaceAt(L, S, Q.Z);
					if (Facing < -0.5)
					{
						// (inside: the rooms' plaster, dark, its patches fallen to the brick, the papered walls' lines where
						// the floors were, and black where they burnt)
						const int Storey = Q.Z < GroundH ? 0 : 1 + static_cast<int>((Q.Z - GroundH) / StoreyH);
						const bool bBurnt = Storey >= 0 && Storey < static_cast<int>(L.Burnt.size()) && L.Burnt[static_cast<size_t>(Storey)];
						Tone In = Varied(P.Inner, 0.74 + 0.16 * Noise(S / 1.7, Q.Z / 1.7, L.Key ^ 0x1A), 0.015 * Noise(S / 3.1, Q.Z / 2.2, L.Key ^ 0x1E));
						// (the plaster off in patches: the brick or stone behind it)
						const double Off = Smooth(0.1, 0.55, Noise(S / 2.3, Q.Z / 1.9, L.Key ^ 0x1C));
						In = Mixed(In, Varied(P.Core, 0.82, 0.0), 0.7 * Off);
						// (a line of dirt where each floor was, and the dark of the skirting under it)
						const double Floor = Q.Z - FloorZ(std::max(0, Storey));
						In = Mixed(In, Varied(P.Char, 1.3, 0.0), 0.35 * Smooth(0.45, 0.0, std::fabs(Floor)) + 0.25 * Smooth(1.4, 0.2, Q.Z));
						return bBurnt ? Mixed(In, P.Soot, 0.6 + 0.25 * Noise(S / 1.1, Q.Z / 0.9, L.Key ^ 0x1B)) : In;
					}
					if (N.Z > 0.5) return bCut ? Varied(Raw, 1.06, 0.0) : Mixed(FaceAt(L, S, Q.Z), P.Dust, 0.35);
					if (N.Z < -0.5) return Varied(P.Trim, 0.6, 0.0);
					return bCut ? Varied(Raw, 0.92, 0.0) : Varied(FaceAt(L, S, Q.Z), 0.86, 0.0);
				});
			}

			/** A piece of wall cut to the wall's top and its holes. Reveal: its sides frame a window. */
			void Piece(double S0, double S1, double Z0, double Z1, bool bReveal, bool bUnder = false)
			{
				const WallLay& L = *Lay;
				S0 = std::max(S0, L.Skip);
				S1 = std::min(S1, L.Len - L.Skip);
				if (S1 - S0 < 0.03) return;
				const double TopMin = std::min({ TopAt(L, S0), TopAt(L, 0.5 * (S0 + S1)), TopAt(L, S1) });
				const bool bHole = !L.Holes.empty() && CrossesHole(S0, S1, Z0, Z1);
				if (TopMin - L.Rag >= Z1 && !bHole)
				{
					Emit(S0, S1, Z0, Z1, !bReveal, bUnder);
					return;
				}
				if (TopMin + L.Rag <= Z0 - 0.02 && std::max(TopAt(L, S0), TopAt(L, S1)) + L.Rag <= Z0) return;
				// broken: brick by brick along the top (columns of some half a metre), and through a hole in rows too
				const double Step = L.Lod == 0 ? 0.48 : L.Lod == 1 ? 1.1 : 2.4;
				const int Cols = std::max(1, static_cast<int>(std::ceil((S1 - S0) / Step)));
				for (int C = 0; C < Cols; ++C)
				{
					const double A = S0 + (S1 - S0) * C / Cols, B = S0 + (S1 - S0) * (C + 1) / Cols;
					const uint64_t Brick = Hash(L.Key ^ 0xB41C, static_cast<uint64_t>(std::lround(A * 20.0)), static_cast<uint64_t>(std::lround(Z0 * 10.0)));
					const double Jag = L.Rag * (Unit(Brick) - 0.5) * 2.0;
					// (courses: a broken top steps by whole courses, 0.075 m)
					const double Top = std::floor((TopAt(L, 0.5 * (A + B)) + Jag) / 0.075) * 0.075;
					const double Hi = std::min(Z1, Top);
					if (Hi <= Z0 + 0.04) continue;
					if (!bHole)
					{
						Emit(A, B, Z0, Hi, true, bUnder);
						continue;
					}
					// through the hole: rows of some 0.45 m, kept where outside it, run together up the column
					const double RowStep = L.Lod == 0 ? 0.42 : 1.0;
					const int Rows = std::max(1, static_cast<int>(std::ceil((Hi - Z0) / RowStep)));
					double RunFrom = -1.0;
					for (int R = 0; R <= Rows; ++R)
					{
						const double Za = Z0 + (Hi - Z0) * R / Rows;
						const bool bKeep = R < Rows && !InHole(0.5 * (A + B), Za + 0.5 * (Hi - Z0) / Rows);
						if (bKeep && RunFrom < 0.0) RunFrom = Za;
						if (!bKeep && RunFrom >= 0.0)
						{
							Emit(A, B, RunFrom, Za, true, true);
							RunFrom = -1.0;
						}
					}
				}
			}

			/** A ledge proud of the face (a sill, a cornice, a string course), cut to the wall's top and its holes. */
			void Ledge(double S0, double S1, double Z0, double Z1, double Proud)
			{
				const WallLay& L = *Lay;
				S0 = std::max(S0, L.Skip);
				S1 = std::min(S1, L.Len - L.Skip);
				if (S1 - S0 < 0.03) return;
				const V3d Up{ 0, 0, 1 };
				const V3d V = Cross3(Up, L.T);
				const double Sign = Dot3(V, L.Out) >= 0.0 ? 1.0 : -1.0;
				const double V0 = Sign > 0.0 ? 0.0 : -Proud, V1 = Sign > 0.0 ? Proud : 0.0;
				const double Step = 1.2;
				const int Cols = std::max(1, static_cast<int>(std::ceil((S1 - S0) / Step)));
				double RunFrom = -1.0;
				for (int C = 0; C <= Cols; ++C)
				{
					const double A = S0 + (S1 - S0) * C / Cols;
					const double Mid = A + 0.5 * (S1 - S0) / Cols;
					const bool bKeep = C < Cols && TopAt(L, Mid) - 0.6 * L.Rag > Z1 && !InHole(Mid, 0.5 * (Z0 + Z1)) && Mid > 0.05 && Mid < L.Len - 0.05;
					if (bKeep && RunFrom < 0.0) RunFrom = A;
					if (!bKeep && RunFrom >= 0.0)
					{
						const Tone Lit = P.Trim, Dim = Varied(P.Trim, 0.66, 0.0);
						Block(W, L.A, L.T, V, Up, RunFrom, A, V0, V1, Z0, Z1, true, [&](const V3d& Q, const V3d& N) {
							const double Grime = 0.85 + 0.1 * Noise(Dot3(Q - L.A, L.T) / 2.0, 0.5, L.Key ^ 0x5D);
							return N.Z > 0.5 ? Mixed(Lit, P.Dust, 0.3) : N.Z < -0.5 ? Dim : Varied(Lit, Grime, 0.0);
						});
						RunFrom = -1.0;
					}
				}
			}
		};

		// ------------------------------------------------------------------ a building of a terrace or a block

		/**
		 * A terrace's or a block's building: Long along x (its street front on -y), Deep along y, its foot at the floor (0),
		 * so many storeys, in a state; Lod 0 near, 1 the middle, 2 plain. Each its own shape from Key.
		 */
		Tone RoofInk(const Paints& P, const V3d& Q, const V3d& N, uint64_t Key);

		void RowHouse(Writer& W, const Paints& P, int Storeys, int St, uint64_t Key, int Lod, bool bShops, double Long, double Deep)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep;
			const double Eaves = EavesOf(Storeys);
			const int Face = static_cast<int>(Key % 3);
			Rng R(Key ^ 0x2EA7);
			// The top line of each wall by the state: the eaves (and the gables' triangle at the ends), or broken.
			// (front and back: along x from -HL; the ends: along y)
			auto Ragged = [&](double Len, double From, double To, double Dip, uint64_t K) {
				// a broken top from From at one end to To at the other, dipping between by Dip, wandering storey by storey
				std::vector<std::pair<double, double>> T;
				const int N = std::max(3, static_cast<int>(Len / 1.6));
				for (int I = 0; I <= N; ++I)
				{
					const double S = Len * I / N, U = static_cast<double>(I) / N;
					const double Wander = 1.1 * (Unit(Hash(K, static_cast<uint64_t>(I))) - 0.5) + 0.8 * Noise(S / 3.0, 0.5, K);
					T.push_back({ S, std::max(0.6, Lerp(From, To, U) - Dip * std::sin(Pi * U) + Wander) });
				}
				return T;
			};
			const double Ridge = Eaves + 0.42 * Deep * 0.5 * 1.6; // (a pitch of some 40 degrees over half the depth)
			std::vector<std::pair<double, double>> TopFront, TopBack, TopLeft, TopRight;
			std::vector<Hole> HolesFront, HolesBack;
			std::vector<uint8_t> Burnt(static_cast<size_t>(Storeys), 0);
			const double Fall = R.Range(0.0, 1.0) < 0.5 ? -1.0 : 1.0; // (which end a half-fallen building lost)
			switch (St)
			{
			case SHoled:
			{
				// whole walls to the eaves, the gables to the ridge; a shell's hole through the front at one storey
				const double HoleS = R.Range(0.25, 0.75) * Long, HoleZ = Mason::FloorZ(static_cast<int>(R.Range(1.0, Storeys - 0.01))) + 1.6;
				const double HoleW = R.Range(1.2, 2.0);
				const double HoleH = R.Range(1.0, 1.6);
				HolesFront.push_back({ HoleS, HoleZ, HoleW, HoleH });
				TopLeft = { { 0.0, Eaves + 0.7 }, { HD, Ridge + 0.3 }, { Deep, Eaves + 0.7 } };
				TopRight = TopLeft;
				// (one room in three burnt out: its windows sooted)
				for (int K = 0; K < Storeys; ++K)
				{
					const double U = R.Next();
					Burnt[static_cast<size_t>(K)] = U < 0.3 ? 1 : 0;
				}
				break;
			}
			case SGutted:
			{
				// a burnt shell: the walls standing, their tops broken a storey's height in places, every window sooted
				const double Loss = R.Range(1.0, 3.2);
				TopFront = Ragged(Long, Eaves + 0.5, Eaves + 0.3, Loss, Key ^ 0xF1);
				TopBack = Ragged(Long, Eaves + 0.2, Eaves + 0.6, Loss * 1.2, Key ^ 0xF2);
				TopLeft = Ragged(Deep, Eaves + 0.5, Eaves + 0.2, -2.0, Key ^ 0xF3); // (a gable's stump higher in the middle)
				TopRight = Ragged(Deep, Eaves + 0.3, Eaves + 0.5, -1.6, Key ^ 0xF4);
				for (int K = 0; K < Storeys; ++K) Burnt[static_cast<size_t>(K)] = 1;
				const double HoleS = R.Range(0.2, 0.8) * Long;
				const double HoleW = R.Range(1.0, 1.8);
				HolesBack.push_back({ HoleS, R.Range(1.5, Eaves - 2.0), HoleW, 1.4 });
				break;
			}
			case SHalf:
			{
				// one end down to its first floor or lower, the walls falling to it diagonally
				const double High = Eaves + 0.4, Low = R.Range(1.6, GroundH + 1.4);
				const double A = Fall > 0.0 ? High : Low, B = Fall > 0.0 ? Low : High;
				TopFront = Ragged(Long, A, B, -0.8, Key ^ 0xF5);
				TopBack = Ragged(Long, A + 0.2, B - 0.3, -0.5, Key ^ 0xF6);
				TopLeft = Ragged(Deep, A, A - 0.4, -1.2, Key ^ 0xF7);
				TopRight = Ragged(Deep, B, B + 0.3, 0.0, Key ^ 0xF8);
				for (int K = 0; K < Storeys; ++K)
				{
					const double U = R.Next();
					Burnt[static_cast<size_t>(K)] = U < 0.55 ? 1 : 0;
				}
				break;
			}
			default:
			{
				// a stump: the ground floor's walls a storey high or less, broken; its heap over all
				const double H1 = R.Range(1.4, GroundH + 0.6), H2 = R.Range(0.8, GroundH);
				TopFront = Ragged(Long, H1, H2, 0.4, Key ^ 0xF9);
				TopBack = Ragged(Long, H2, H1, 0.2, Key ^ 0xFA);
				TopLeft = Ragged(Deep, H1, H2 + 0.6, -0.5, Key ^ 0xFB);
				TopRight = Ragged(Deep, H2, H1, 0.3, Key ^ 0xFC);
				Burnt[0] = 1;
				break;
			}
			}
			const double Rag = St == SHoled ? 0.0 : Lod == 2 ? 0.6 : 0.35;
			Mason M(W, P);
			// the front, on -y (a main street's shops at its foot)
			WallLay F;
			F.A = { -HL, -HD, 0.0 };
			F.T = { 1, 0, 0 };
			F.Out = { 0, -1, 0 };
			F.Len = Long;
			F.Storeys = Storeys;
			F.Bays = BaysOf(Storeys) + (Long > FrontOf(Storeys) + 2.0 ? 1 : 0);
			F.bShops = bShops;
			F.Top = TopFront;
			F.Rag = Rag;
			F.Holes = HolesFront;
			F.Burnt = Burnt;
			F.Face = Face;
			F.Key = Key ^ 0xF0;
			F.Lod = Lod;
			F.bCornice = St == SHoled;
			M.Build(F);
			// the back, on +y, its face to the courtyard (from the far end, so its run's left is outward)
			WallLay B = F;
			B.A = { HL, HD, 0.0 };
			B.T = { -1, 0, 0 };
			B.Out = { 0, 1, 0 };
			B.bShops = false;
			B.Top.clear();
			for (auto It = TopBack.rbegin(); It != TopBack.rend(); ++It) B.Top.push_back({ Long - It->first, It->second });
			if (TopBack.empty()) B.Top.clear();
			B.Holes.clear();
			for (const Hole& H : HolesBack) B.Holes.push_back({ Long - H.S, H.Z, H.RS, H.RZ });
			B.Key = Key ^ 0xB0;
			B.bCornice = false;
			M.Build(B);
			// the ends: party walls, blank (a neighbour stood against them), along y
			WallLay E;
			E.A = { -HL, HD, 0.0 };
			E.T = { 0, -1, 0 };
			E.Out = { -1, 0, 0 };
			E.Len = Deep;
			E.Storeys = Storeys;
			E.Bays = 0;
			E.Rag = Rag;
			E.Face = Face;
			E.Lod = Lod;
			E.Key = Key ^ 0xE0;
			E.Burnt = Burnt;
			E.Thick = 0.38;
			E.Skip = F.Thick; // (between the front's and the back's ends)
			for (auto It = TopLeft.rbegin(); It != TopLeft.rend(); ++It) E.Top.push_back({ Deep - It->first, It->second });
			M.Build(E);
			WallLay E2 = E;
			E2.A = { HL, -HD, 0.0 };
			E2.T = { 0, 1, 0 };
			E2.Out = { 1, 0, 0 };
			E2.Top = TopRight;
			E2.Key = Key ^ 0xE1;
			M.Build(E2);
			// Inside: the floors (whole where the walls stand to them), the hanging ends of a fallen half, the joists' stubs of
			// a burnt shell; the heap of what came down.
			const double In = 0.45;
			if (Lod <= 1 && (St == SHoled || St == SHalf))
				for (int K = 1; K < Storeys; ++K)
				{
					const double Z = Mason::FloorZ(K);
					double X0 = -HL + In, X1 = HL - In;
					if (St == SHalf)
					{
						// (only as far as both long walls still stand a storey over it; its broken end hangs down)
						const int N = 24;
						double Lo = 1e9, Hi = -1e9;
						for (int I = 0; I <= N; ++I)
						{
							const double S = Long * I / N;
							if (M.TopAt(F, S) > Z + 1.8) { Lo = std::min(Lo, S); Hi = std::max(Hi, S); }
						}
						if (Hi < Lo) continue;
						X0 = std::max(X0, -HL + Lo);
						X1 = std::min(X1, -HL + Hi);
						if (X1 - X0 < 1.0) continue;
						// the hanging end: a slab tipped down from the broken edge
						const double Edge = Fall > 0.0 ? X1 : X0, Dir = Fall > 0.0 ? 1.0 : -1.0;
						const double Drop = R.Range(0.45, 0.85), Reach = R.Range(1.2, 2.4);
						const V3d A0{ Edge, -HD + In, Z }, A1{ Edge, HD - In, Z };
						const V3d U = Norm3(V3d{ Dir * std::cos(Drop), 0.0, -std::sin(Drop) });
						const V3d V0 = Norm3(V3d{ 0.0, -Dir, 0.0 });
						const V3d Up = Cross3(U, V0);
						(void)A1;
						Block(W, A0, U, V0 * -1.0, Up * -1.0, 0.0, Reach, 0.0, Deep - 2.0 * In, 0.0, 0.22, true, [&](const V3d&, const V3d& Nn) {
							return Nn.Z > 0.3 ? Mixed(P.Timber, P.Dust, 0.4) : Varied(P.Inner, 0.7, 0.0);
						});
					}
					Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, X0, X1, -HD + In, HD - In, Z - 0.24, Z, true, [&](const V3d& Q, const V3d& Nn) {
						if (Nn.Z > 0.5) return Mixed(P.Timber, P.Dust, 0.3 + 0.2 * Noise(Q.X / 2.0, Q.Y / 2.0, Key ^ 0x77));
						return Nn.Z < -0.5 ? Varied(P.Inner, 0.72, 0.0) : Varied(P.Inner, 0.85, 0.0);
					});
				}
			if (Lod == 0 && St == SGutted)
			{
				// the joists' stubs along the long walls at each floor, charred
				for (int K = 1; K < Storeys; ++K)
				{
					const double Z = Mason::FloorZ(K) - 0.12;
					for (double X = -HL + 0.8; X < HL - 0.6; X += 0.9)
					{
						const double U1 = Unit(Hash(Key ^ 0x1015, static_cast<uint64_t>(K * 100 + static_cast<int>((X + HL) * 2.0))));
						if (U1 < 0.35 || M.TopAt(F, X + HL) < Z + 0.3) continue;
						const double Reach = 0.25 + 0.6 * U1;
						Beam(W, V3d{ X, -HD + In, Z }, V3d{ X, -HD + In + Reach, Z - 0.05 * U1 }, 0.16, P.Char);
						Beam(W, V3d{ X + 0.2, HD - In, Z }, V3d{ X + 0.2, HD - In - Reach * 0.8, Z - 0.04 }, 0.16, P.Char);
					}
				}
			}
			// the floor of the ground floor: rubble and ash over it
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -HL + 0.2, HL - 0.2, -HD + 0.2, HD - 0.2, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, P.Char, St == SHoled ? 0.15 : 0.45) : P.Inner; });
			// the heap inside (a burnt shell's floors fallen, a half's, a stump's over all)
			if (St != SHoled)
			{
				const double Over = St == SStump ? 1.0 : St == SHalf ? 0.75 : 0.65;
				const double High = St == SStump ? R.Range(2.4, 3.6) : R.Range(1.4, 2.4);
				const double CX = St == SHalf ? Fall * HL * 0.35 : 0.0;
				Heap(W, CX, 0.0, HL * Over, HD * 0.95, High, Key ^ 0x4EA9, P, Lod, St == SStump ? 46 : 26, St == SStump ? 6 : 4);
			}
			// The roof (a holed one): rafters bare where its slates or tiles are gone; its ridge along x. A stack on each
			// gable.
			if (St == SHoled)
			{
				const double Pitch = std::atan2(Ridge - Eaves, HD);
				const double Slope = std::hypot(Ridge - Eaves, HD);
				const double HoleX = R.Range(-0.3, 0.3) * Long, HoleR = R.Range(0.22, 0.38) * Long;
				const double HoleSide = R.Next() < 0.5 ? -1.0 : 1.0; // (the slope the hole is in)
				for (int Side = -1; Side <= 1; Side += 2)
				{
					// each slope's frame: U along x, Down the slope from the ridge to the eaves, its outward normal
					const V3d Top{ 0.0, 0.0, Ridge };
					const V3d Down = Norm3(V3d{ 0.0, Side * std::cos(Pitch), -std::sin(Pitch) });
					const V3d U{ 1, 0, 0 };
					const V3d Nrm = Cross3(Side > 0 ? U : U * -1.0, Down) * (Side > 0 ? 1.0 : 1.0);
					const bool bHoleHere = Side == static_cast<int>(HoleSide);
					const double Spacing = Lod == 0 ? 0.75 : Lod == 1 ? 1.5 : 99.0;
					// the slates or tiles: in strips down the slope between the rafters, each strip cut back at the hole
					const double Strip = Lod == 0 ? 1.5 : Lod == 1 ? 3.0 : Long;
					for (double X = -HL - 0.3; X < HL + 0.3 - 1e-6; X += Strip)
					{
						const double X1 = std::min(HL + 0.3, X + Strip);
						double D0 = 0.0, D1 = Slope + 0.35;
						if (bHoleHere && Lod <= 1)
						{
							const double DX = 0.5 * (X + X1) - HoleX;
							if (std::fabs(DX) < HoleR)
							{
								// (the hole's reach down the slope at this strip, ragged)
								const double Rg = std::sqrt(std::max(0.0, 1.0 - (DX / HoleR) * (DX / HoleR)));
								const double J = 0.25 * (Unit(Hash(Key ^ 0x7001, static_cast<uint64_t>(std::lround(X * 4.0)))) - 0.5);
								const double Gap0 = Slope * (0.45 - 0.35 * Rg + J), Gap1 = Slope * (0.55 + 0.4 * Rg + J);
								// upper part, then lower part
								if (Gap0 > 0.2)
									Block(W, Top, U, Down, Nrm, X, X1, D0, Gap0, 0.0, 0.07, true, [&](const V3d& Q, const V3d& Nn) { return RoofInk(P, Q, Nn, Key); });
								if (Gap1 < D1 - 0.2)
									Block(W, Top, U, Down, Nrm, X, X1, Gap1, D1, 0.0, 0.07, true, [&](const V3d& Q, const V3d& Nn) { return RoofInk(P, Q, Nn, Key); });
								continue;
							}
						}
						Block(W, Top, U, Down, Nrm, X, X1, D0, D1, 0.0, 0.07, true, [&](const V3d& Q, const V3d& Nn) { return RoofInk(P, Q, Nn, Key); });
					}
					// the rafters under it, bare in the hole (some broken short)
					if (Lod <= 1)
						for (double X = -HL + 0.3; X <= HL - 0.3; X += Spacing)
						{
							double Reach = Slope;
							if (bHoleHere && std::fabs(X - HoleX) < HoleR)
							{
								const double U1 = Unit(Hash(Key ^ 0x7002, static_cast<uint64_t>(std::lround(X * 4.0))));
								if (U1 < 0.3) continue;
								if (U1 < 0.65) Reach = Slope * (0.25 + 0.4 * U1);
							}
							else if (Lod == 1) continue;
							const V3d From = Top + Down * 0.0 + Nrm * -0.1 + V3d{ X, 0.0, 0.0 };
							const V3d To = Top + Down * Reach + Nrm * -0.1 + V3d{ X, 0.0, 0.0 };
							Beam(W, From, To, 0.12, P.Timber);
						}
				}
				// the ridge and the stacks on the gables
				if (Lod <= 1)
				{
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double X = Side * (HL - 0.5);
						Block(W, V3d{ X, 0.0, 0.0 }, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -0.45, 0.45, -0.32, 0.32, Ridge - 1.0, Ridge + 1.2, false,
						      [&](const V3d& Q, const V3d& Nn) { return Nn.Z > 0.5 ? P.Soot : Mason(W, P).FaceAt(F, Q.X + HL, Q.Z); });
					}
				}
			}
		}

		/** The roof's colour at a place on it: slate or tile weathered in patches, mossy low, a few slipped. */
		Tone RoofInk(const Paints& P, const V3d& Q, const V3d& N, uint64_t Key)
		{
			if (N.Z < 0.0) return Varied(P.Timber, 0.7, 0.0);
			const double N1 = Noise(Q.X / 2.4, Q.Y / 1.3 + Q.Z, Key ^ 0x2F);
			return Varied(P.Roof, 0.95 + 0.1 * N1, 0.02 * N1);
		}

		/** A plain far level of a terrace building: its walls as blocks to their broken tops, the windows' rows darkened. */
		void RowFar(Writer& W, const Paints& P, int Storeys, int St, uint64_t Key, double Long, double Deep)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep, Eaves = EavesOf(Storeys);
			const Tone Face = Mixed(P.Face[Key % 3], Rgb(40, 38, 36), St == SHoled ? 0.22 : 0.4);
			const Tone Top = Mixed(P.Dust, P.Core, 0.5);
			Rng R(Key ^ 0x2EA7);
			const double Kept = St == SHoled ? 1.0 : St == SGutted ? R.Range(0.75, 0.95) : St == SHalf ? R.Range(0.45, 0.7) : R.Range(0.2, 0.35);
			const double High = std::max(1.2, Eaves * Kept);
			if (St == SStump)
			{
				Plain(W, -HL, HL, -HD, HD, -1.0, High, Face, Top);
				Heap(W, 0.0, 0.0, HL * 1.05, HD, R.Range(2.4, 3.6), Key ^ 0x4EA9, P, 2, 0, 0);
				return;
			}
			if (St == SHalf)
			{
				// (a step down the fallen end)
				const double Cut = R.Range(-0.2, 0.25) * Long;
				Plain(W, -HL, Cut, -HD, HD, -1.0, Eaves + 0.4, Face, Top);
				Plain(W, Cut, HL, -HD, HD, -1.0, High * 0.6, Face, Top);
				Heap(W, 0.4 * HL, 0.0, HL * 0.7, HD, 2.0, Key ^ 0x4EA9, P, 2, 0, 0);
				return;
			}
			// (a holed roof's ridge as a low prism; a shell's walls as a ring, open to the sky, its top stepped)
			if (St == SHoled)
			{
				Plain(W, -HL, HL, -HD, HD, -1.0, Eaves + 0.6, Face, Top);
				const double Ridge = Eaves + 0.42 * Deep * 0.5 * 1.6;
				const uint32_t A = W.Vert(-HL, -HD, Eaves + 0.6, 0, -0.7, 0.7, P.Roof), B = W.Vert(HL, -HD, Eaves + 0.6, 0, -0.7, 0.7, P.Roof);
				const uint32_t C = W.Vert(HL, 0.0, Ridge, 0, -0.7, 0.7, P.Roof), D = W.Vert(-HL, 0.0, Ridge, 0, -0.7, 0.7, P.Roof);
				W.Tri(A, B, C);
				W.Tri(A, C, D);
				const uint32_t A2 = W.Vert(HL, HD, Eaves + 0.6, 0, 0.7, 0.7, P.Roof), B2 = W.Vert(-HL, HD, Eaves + 0.6, 0, 0.7, 0.7, P.Roof);
				const uint32_t C2 = W.Vert(-HL, 0.0, Ridge, 0, 0.7, 0.7, P.Roof), D2 = W.Vert(HL, 0.0, Ridge, 0, 0.7, 0.7, P.Roof);
				W.Tri(A2, B2, C2);
				W.Tri(A2, C2, D2);
				return;
			}
			const double T = 0.5;
			const int Steps = 3;
			for (int K = 0; K < Steps; ++K)
			{
				const double X0 = -HL + Long * K / Steps, X1 = -HL + Long * (K + 1) / Steps;
				const double H = std::max(1.0, High + R.Range(-1.4, 1.0));
				Plain(W, X0, X1, -HD, -HD + T, -1.0, H, Face, Top);
				const double H2 = std::max(1.0, High + R.Range(-1.6, 0.8));
				Plain(W, X0, X1, HD - T, HD, -1.0, H2, Face, Top);
			}
			Plain(W, -HL, -HL + T, -HD, HD, -1.0, High, Face, Top);
			Plain(W, HL - T, HL, -HD, HD, -1.0, High, Face, Top);
		}

		// ------------------------------------------------------------------ the other buildings

		/** A suburb's detached house: Long by Deep, two storeys under a hipped roof; its state (0 holed, 1 burnt, 2 half
		 * fallen, 3 whole but for slipped tiles). */
		void Villa(Writer& W, const Paints& P, int St, uint64_t Key, int Lod, double Long, double Deep)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep;
			const int Storeys = 2;
			const double Eaves = EavesOf(Storeys) - 0.3;
			Rng R(Key ^ 0x7111A);
			if (Lod == 2)
			{
				const Tone Face = Mixed(P.Face[Key % 3], Rgb(60, 56, 52), 0.25);
				const double High = St == 2 ? Eaves * 0.55 : St == 1 ? Eaves * 0.9 : Eaves;
				Plain(W, -HL, HL, -HD, HD, -1.0, High, Face, Mixed(P.Dust, P.Core, 0.4));
				if (St == 0 || St == 3)
				{
					// (the hipped roof as a low pyramid)
					const double Peak = Eaves + 0.38 * Deep;
					const uint32_t T = W.Vert(0, 0, Peak, 0, 0, 1, P.Roof);
					const double Cx[4] = { -HL - 0.4, HL + 0.4, HL + 0.4, -HL - 0.4 }, Cy[4] = { -HD - 0.4, -HD - 0.4, HD + 0.4, HD + 0.4 };
					uint32_t C[4];
					for (int K = 0; K < 4; ++K) C[K] = W.Vert(Cx[K], Cy[K], Eaves, Cx[K] / Long, Cy[K] / Deep, 0.7, P.Roof);
					for (int K = 0; K < 4; ++K) W.Tri(T, C[K], C[(K + 1) % 4]);
				}
				return;
			}
			const double Rag = St == 0 || St == 3 ? 0.0 : 0.3;
			Mason M(W, P);
			std::vector<uint8_t> Burnt(2, St == 1 ? 1 : 0);
			if (St == 2) Burnt[0] = 1;
			auto Top = [&](double Len, double A, double B, uint64_t K) {
				std::vector<std::pair<double, double>> T;
				for (int I = 0; I <= 6; ++I)
				{
					const double U = I / 6.0;
					T.push_back({ Len * U, std::max(0.8, Lerp(A, B, U) + 1.0 * (Unit(Hash(K, static_cast<uint64_t>(I))) - 0.5)) });
				}
				return T;
			};
			const bool bOpen = St == 1 || St == 2;
			const double A = St == 2 ? Eaves + 0.3 : Eaves + 0.2, B = St == 2 ? R.Range(1.2, 3.0) : Eaves - R.Range(0.5, 1.6);
			for (int Side = 0; Side < 4; ++Side)
			{
				WallLay L;
				const double Len = Side % 2 == 0 ? Long : Deep;
				switch (Side)
				{
				case 0: L.A = { -HL, -HD, 0 }; L.T = { 1, 0, 0 }; L.Out = { 0, -1, 0 }; break;
				case 1: L.A = { HL, -HD, 0 }; L.T = { 0, 1, 0 }; L.Out = { 1, 0, 0 }; break;
				case 2: L.A = { HL, HD, 0 }; L.T = { -1, 0, 0 }; L.Out = { 0, 1, 0 }; break;
				default: L.A = { -HL, HD, 0 }; L.T = { 0, -1, 0 }; L.Out = { -1, 0, 0 }; break;
				}
				L.Len = Len;
				L.Storeys = Storeys;
				L.Bays = Side % 2 == 0 ? 3 : 2;
				L.Thick = 0.36;
				if (Side % 2 == 1) L.Skip = L.Thick; // (the ends between the long walls)
				L.Face = static_cast<int>(Key % 3);
				L.Key = Key ^ static_cast<uint64_t>(0x710 + Side);
				L.Lod = Lod;
				L.Burnt = Burnt;
				L.Rag = Rag;
				L.bCornice = !bOpen;
				if (bOpen) L.Top = Top(Len, Side < 2 ? A : B, Side < 2 ? B : A, Key ^ static_cast<uint64_t>(0x720 + Side));
				else L.Top = { { 0.0, Eaves }, { Len, Eaves } };
				if (St == 0 && Side == 0) L.Holes.push_back({ R.Range(0.3, 0.7) * Len, R.Range(1.4, 4.2), 1.0, 1.0 });
				M.Build(L);
			}
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -HL + 0.2, HL - 0.2, -HD + 0.2, HD - 0.2, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, P.Char, bOpen ? 0.5 : 0.1) : P.Inner; });
			if (bOpen)
			{
				Heap(W, St == 2 ? 0.25 * Long : 0.0, 0.0, HL * 0.8, HD * 0.85, R.Range(1.3, 2.2), Key ^ 0x4EAA, P, Lod, 22, 4);
				return;
			}
			// the first floor (seen through the windows)
			if (Lod == 0)
				Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -HL + 0.36, HL - 0.36, -HD + 0.36, HD - 0.36, GroundH - 0.24, GroundH, true,
				      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? P.Timber : Varied(P.Inner, 0.75, 0.0); });
			// the hipped roof: four slopes in strips, a hole torn in a holed house's front slope
			const double Peak = Eaves + 0.38 * Deep, Over = 0.4;
			const double HoleX = R.Range(-0.25, 0.25) * Long, HoleR = St == 0 ? R.Range(1.4, 2.4) : St == 3 ? 0.6 : 0.0;
			for (int Side = 0; Side < 4; ++Side)
			{
				// each slope a fan of strips from its eave to the ridge line (the ridge shortened to a point over a square)
				const double Ridge = std::max(0.0, HL - HD);
				V3d E0, E1, R0, R1;
				switch (Side)
				{
				case 0: E0 = { -HL - Over, -HD - Over, Eaves }; E1 = { HL + Over, -HD - Over, Eaves }; R0 = { -Ridge, 0, Peak }; R1 = { Ridge, 0, Peak }; break;
				case 1: E0 = { HL + Over, -HD - Over, Eaves }; E1 = { HL + Over, HD + Over, Eaves }; R0 = { Ridge, 0, Peak }; R1 = { Ridge, 0, Peak }; break;
				case 2: E0 = { HL + Over, HD + Over, Eaves }; E1 = { -HL - Over, HD + Over, Eaves }; R0 = { Ridge, 0, Peak }; R1 = { -Ridge, 0, Peak }; break;
				default: E0 = { -HL - Over, HD + Over, Eaves }; E1 = { -HL - Over, -HD - Over, Eaves }; R0 = { -Ridge, 0, Peak }; R1 = { -Ridge, 0, Peak }; break;
				}
				const V3d Nrm = Norm3(Cross3(E1 - E0, R0 - E0));
				const int Strips = Lod == 0 ? 8 : 3;
				for (int K = 0; K < Strips; ++K)
				{
					const double U0 = static_cast<double>(K) / Strips, U1 = static_cast<double>(K + 1) / Strips;
					const V3d A0 = E0 + (E1 - E0) * U0, A1 = E0 + (E1 - E0) * U1;
					const V3d B0 = R0 + (R1 - R0) * U0, B1 = R0 + (R1 - R0) * U1;
					const double MidX = 0.25 * (A0.X + A1.X + B0.X + B1.X);
					const bool bGap = Side == 0 && HoleR > 0.0 && std::fabs(MidX - HoleX) < HoleR && Lod == 0;
					auto Quad = [&](const V3d& Q0, const V3d& Q1, const V3d& Q2, const V3d& Q3) {
						const uint32_t I0 = W.Vert(Q0.X, Q0.Y, Q0.Z, Nrm.X, Nrm.Y, Nrm.Z, RoofInk(P, Q0, Nrm, Key));
						const uint32_t I1 = W.Vert(Q1.X, Q1.Y, Q1.Z, Nrm.X, Nrm.Y, Nrm.Z, RoofInk(P, Q1, Nrm, Key));
						const uint32_t I2 = W.Vert(Q2.X, Q2.Y, Q2.Z, Nrm.X, Nrm.Y, Nrm.Z, RoofInk(P, Q2, Nrm, Key));
						const uint32_t I3 = W.Vert(Q3.X, Q3.Y, Q3.Z, Nrm.X, Nrm.Y, Nrm.Z, RoofInk(P, Q3, Nrm, Key));
						W.Tri(I0, I1, I2);
						W.Tri(I0, I2, I3);
						// (its underside, the boards seen from below the eaves and through the hole)
						const Tone Under = Varied(P.Timber, 0.7, 0.0);
						const uint32_t J0 = W.Vert(Q0.X, Q0.Y, Q0.Z - 0.05, -Nrm.X, -Nrm.Y, -Nrm.Z, Under), J1 = W.Vert(Q1.X, Q1.Y, Q1.Z - 0.05, -Nrm.X, -Nrm.Y, -Nrm.Z, Under);
						const uint32_t J2 = W.Vert(Q2.X, Q2.Y, Q2.Z - 0.05, -Nrm.X, -Nrm.Y, -Nrm.Z, Under), J3 = W.Vert(Q3.X, Q3.Y, Q3.Z - 0.05, -Nrm.X, -Nrm.Y, -Nrm.Z, Under);
						W.Tri(J0, J2, J1);
						W.Tri(J0, J3, J2);
					};
					if (!bGap)
					{
						Quad(A0, A1, B1, B0);
						continue;
					}
					// (the hole: the strip's middle torn out, its rafter bare across it)
					const double G0 = 0.25 + 0.15 * Unit(Hash(Key ^ 0x99, static_cast<uint64_t>(K))), G1 = 0.7 + 0.15 * Unit(Hash(Key ^ 0x9A, static_cast<uint64_t>(K)));
					Quad(A0, A1, A1 + (B1 - A1) * G0, A0 + (B0 - A0) * G0);
					Quad(A0 + (B0 - A0) * G1, A1 + (B1 - A1) * G1, B1, B0);
					Beam(W, A0 + V3d{ 0, 0, -0.12 }, B0 + V3d{ 0, 0, -0.12 }, 0.1, P.Timber);
				}
			}
		}

		/** A works shed: Long by Deep, brick walls 7 m with tall windows, its north-light roof's trusses bare, the glass
		 * gone, the slates half off. */
		void Shed(Writer& W, const Paints& P, uint64_t Key, int Lod, double Long, double Deep)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep, Wall = 6.8;
			Paints Q = P;
			// (works are brick everywhere: the style's core for its faces)
			Q.Face[0] = Varied(P.Core, 1.0, 0.0);
			Q.Face[1] = Varied(P.Core, 0.92, 0.0);
			Q.Face[2] = Varied(P.Core, 1.06, 0.0);
			Q.bPlaster = false;
			if (Lod == 2)
			{
				Plain(W, -HL, HL, -HD, HD, -1.0, Wall, Mixed(Q.Face[0], Rgb(50, 46, 42), 0.25), Mixed(P.Roof, P.Dust, 0.3));
				return;
			}
			Mason M(W, Q);
			Rng R(Key ^ 0x5E3D);
			for (int Side = 0; Side < 4; ++Side)
			{
				WallLay L;
				const double Len = Side % 2 == 0 ? Long : Deep;
				switch (Side)
				{
				case 0: L.A = { -HL, -HD, 0 }; L.T = { 1, 0, 0 }; L.Out = { 0, -1, 0 }; break;
				case 1: L.A = { HL, -HD, 0 }; L.T = { 0, 1, 0 }; L.Out = { 1, 0, 0 }; break;
				case 2: L.A = { HL, HD, 0 }; L.T = { -1, 0, 0 }; L.Out = { 0, 1, 0 }; break;
				default: L.A = { -HL, HD, 0 }; L.T = { 0, -1, 0 }; L.Out = { -1, 0, 0 }; break;
				}
				L.Len = Len;
				L.Storeys = 2;
				L.Bays = std::max(2, static_cast<int>(Len / 4.2));
				L.Thick = 0.5;
				if (Side % 2 == 1) L.Skip = L.Thick; // (the ends between the long walls)
				L.Face = Side % 3;
				L.Key = Key ^ static_cast<uint64_t>(0x5E0 + Side);
				L.Lod = Lod;
				L.bCornice = false;
				L.Rag = 0.35;
				L.Top.clear();
				const int N = 8;
				for (int I = 0; I <= N; ++I)
				{
					const double U = Unit(Hash(L.Key, static_cast<uint64_t>(I)));
					L.Top.push_back({ Len * I / N, U < 0.2 ? Wall - R.Range(1.0, 3.5) : Wall });
				}
				M.Build(L);
			}
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -HL + 0.2, HL - 0.2, -HD + 0.2, HD - 0.2, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, Rgb(70, 66, 60), 0.4) : P.Inner; });
			// the saw-tooth roof's trusses across the depth, every 5 m: a steep glazed face (empty) and a long slated slope
			const int Bays = std::max(2, static_cast<int>(Long / 5.0));
			for (int K = 0; K < Bays; ++K)
			{
				const double X0 = -HL + Long * K / Bays, X1 = -HL + Long * (K + 1) / Bays;
				const double Peak = Wall + 3.2;
				const bool bDown = Unit(Hash(Key ^ 0x7055, static_cast<uint64_t>(K))) < 0.3;
				// the truss's two rafters at each end and the glazing's frame
				for (double Y = -HD + 0.3; Y <= HD - 0.3; Y += (Lod == 0 ? 3.0 : 6.0))
				{
					Beam(W, V3d{ X0, Y, Wall }, V3d{ X1 - 0.6, Y, Peak }, 0.16, Rgb(70, 62, 56));
					Beam(W, V3d{ X1 - 0.6, Y, Peak }, V3d{ X1, Y, Wall }, 0.14, Rgb(70, 62, 56));
				}
				if (bDown) continue;
				// the slated slope (its far half gone in one bay in three)
				const V3d A0{ X0, -HD, Wall }, A1{ X1 - 0.6, -HD, Peak };
				const double Keep = Unit(Hash(Key ^ 0x7056, static_cast<uint64_t>(K))) < 0.4 ? R.Range(0.3, 0.7) : 1.0;
				const V3d U = Norm3(A1 - A0);
				const V3d Across{ 0, 1, 0 };
				const V3d Nrm = Cross3(U, Across) * -1.0;
				Block(W, A0, U, Across, Norm3(Nrm), 0.0, std::sqrt(Dot3(A1 - A0, A1 - A0)), 0.0, Deep * Keep, 0.0, 0.06, true,
				      [&](const V3d& Qp, const V3d& Nn) { return RoofInk(P, Qp, Nn, Key); });
			}
		}

		/** A works chimney: round brick, tapering, some 34 m, its top bitten by a shell. */
		void Chimney(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			const int Round = Lod == 0 ? 12 : Lod == 1 ? 8 : 6;
			const int Rings = Lod == 0 ? 14 : 4;
			const double High = 34.0, R0 = 1.7, R1 = 1.0;
			const Tone Brick = Varied(P.Core, 1.0, 0.0);
			std::vector<uint32_t> Id;
			for (int J = 0; J <= Rings; ++J)
			{
				const double T = static_cast<double>(J) / Rings, Z = -1.0 + (High + 1.0) * T, Rad = Lerp(R0, R1, T);
				for (int K = 0; K < Round; ++K)
				{
					const double A = 2.0 * Pi * K / Round, X = std::cos(A), Y = std::sin(A);
					// (the shell's bite: a notch out of one side of the last few metres)
					double Bite = 0.0;
					if (T > 0.85 && std::cos(A - 1.3) > 0.4) Bite = (T - 0.85) / 0.15 * 0.8;
					const double Soot = Smooth(0.75, 1.0, T);
					const Tone C = Mixed(Varied(Brick, 0.95 + 0.1 * Noise(A * 2.0, Z / 3.0, Key), 0.0), P.Soot, 0.8 * Soot);
					Id.push_back(W.Vert(X * Rad * (1.0 - Bite * 0.6), Y * Rad * (1.0 - Bite * 0.6), Z - Bite * 2.0, X, Y, 0.0, C));
				}
			}
			for (int J = 0; J < Rings; ++J)
				for (int K = 0; K < Round; ++K)
				{
					const uint32_t A = Id[static_cast<size_t>(J * Round + K)], B = Id[static_cast<size_t>(J * Round + (K + 1) % Round)];
					const uint32_t C = Id[static_cast<size_t>((J + 1) * Round + (K + 1) % Round)], D = Id[static_cast<size_t>((J + 1) * Round + K)];
					W.Tri(A, B, C);
					W.Tri(A, C, D);
				}
			// its square base
			Plain(W, -2.2, 2.2, -2.2, 2.2, -1.0, 3.0, Varied(Brick, 0.9, 0.0), Mixed(Brick, P.Dust, 0.4));
		}

		/** A water tower: a brick drum 14 m, an iron tank on it, holed. */
		void Tower(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			const int Round = Lod == 0 ? 14 : 8;
			auto Drum = [&](double Z0, double Z1, double Rad, const Tone& C, bool bTop) {
				std::vector<uint32_t> Lo, Hi;
				for (int K = 0; K <= Round; ++K)
				{
					const double A = 2.0 * Pi * K / Round, X = std::cos(A), Y = std::sin(A);
					const Tone Cv = Varied(C, 0.94 + 0.1 * Noise(A * 2.0, Z0, Key), 0.0);
					Lo.push_back(W.Vert(X * Rad, Y * Rad, Z0, X, Y, 0, Cv));
					Hi.push_back(W.Vert(X * Rad, Y * Rad, Z1, X, Y, 0, Cv));
				}
				for (int K = 0; K < Round; ++K)
				{
					W.Tri(Lo[static_cast<size_t>(K)], Lo[static_cast<size_t>(K + 1)], Hi[static_cast<size_t>(K + 1)]);
					W.Tri(Lo[static_cast<size_t>(K)], Hi[static_cast<size_t>(K + 1)], Hi[static_cast<size_t>(K)]);
				}
				if (bTop)
				{
					const uint32_t Mid = W.Vert(0, 0, Z1 + 1.0, 0, 0, 1, Varied(C, 0.9, 0.0));
					for (int K = 0; K < Round; ++K)
					{
						const double A = 2.0 * Pi * K / Round, A2 = 2.0 * Pi * (K + 1) / Round;
						const uint32_t E0 = W.Vert(std::cos(A) * Rad, std::sin(A) * Rad, Z1, std::cos(A) * 0.5, std::sin(A) * 0.5, 0.8, Varied(C, 0.9, 0.0));
						const uint32_t E1 = W.Vert(std::cos(A2) * Rad, std::sin(A2) * Rad, Z1, std::cos(A2) * 0.5, std::sin(A2) * 0.5, 0.8, Varied(C, 0.9, 0.0));
						W.Tri(Mid, E0, E1);
					}
				}
			};
			Drum(-1.0, 14.0, 3.4, P.Core, false);
			Drum(14.0, 15.0, 4.2, P.Trim, false);
			Drum(15.0, 20.0, 5.2, Rgb(84, 78, 72), true);
			if (Lod == 0)
			{
				// (shell holes through the tank, dark)
				for (int K = 0; K < 3; ++K)
				{
					const double A = 2.0 * Pi * Unit(Hash(Key, static_cast<uint64_t>(K))), Z = 16.0 + 3.0 * Unit(Hash(Key, static_cast<uint64_t>(10 + K)));
					Lump(W, 5.25 * std::cos(A), 5.25 * std::sin(A), Z - 0.4, 0.5, 0.8, Hash(Key, static_cast<uint64_t>(20 + K)), Rgb(30, 28, 26), Rgb(24, 22, 20));
				}
			}
		}

		/** The landmark church: a nave 30 by 13 m gutted to the sky, its west tower 8 m square to 34 m and a spire on it
		 * to some 56 m, the spire's tip shot away, its ribs bare at the break. Along x, the tower at -x. */
		void Church(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			Paints Q = P;
			Q.Face[0] = Varied(P.Trim, 0.92, 0.0);
			Q.Face[1] = Varied(P.Trim, 0.86, 0.01);
			Q.Face[2] = Varied(P.Trim, 0.98, 0.0);
			Q.bPlaster = false;
			const double NL = 30.0, ND = 13.0, NH = 13.0, TS = 8.0, TH = 34.0;
			const double NX0 = -NL * 0.5 + TS * 0.5, NX1 = NX0 + NL;
			if (Lod == 2)
			{
				const Tone F = Mixed(Q.Face[0], Rgb(60, 56, 52), 0.2), T = Mixed(P.Dust, Q.Face[0], 0.5);
				Plain(W, NX0, NX1, -0.5 * ND, 0.5 * ND, -1.0, NH, F, T);
				Plain(W, NX0 - TS, NX0, -0.5 * TS, 0.5 * TS, -1.0, TH, F, T);
				const uint32_t Tip = W.Vert(NX0 - 0.5 * TS, 0.0, TH + 18.0, 0, 0, 1, P.Roof);
				const double Cx[4] = { NX0 - TS, NX0, NX0, NX0 - TS }, Cy[4] = { -0.5 * TS, -0.5 * TS, 0.5 * TS, 0.5 * TS };
				uint32_t C[4];
				for (int K = 0; K < 4; ++K) C[K] = W.Vert(Cx[K], Cy[K], TH, Cx[K] - (NX0 - 0.5 * TS), Cy[K], 0.4, P.Roof);
				for (int K = 0; K < 4; ++K) W.Tri(Tip, C[K], C[(K + 1) % 4]);
				return;
			}
			Mason M(W, Q);
			Rng R(Key ^ 0xC4C4);
			// the nave's long walls: tall arched windows (as bays of one tall storey), their tops broken, burnt inside
			for (int Side = -1; Side <= 1; Side += 2)
			{
				WallLay L;
				L.A = Side < 0 ? V3d{ NX0, -0.5 * ND, 0 } : V3d{ NX1, 0.5 * ND, 0 };
				L.T = Side < 0 ? V3d{ 1, 0, 0 } : V3d{ -1, 0, 0 };
				L.Out = { 0, static_cast<double>(Side), 0 };
				L.Len = NL;
				L.Storeys = 1;
				L.Bays = 6;
				L.Thick = 0.9;
				L.Face = 0;
				L.Key = Key ^ static_cast<uint64_t>(0xC10 + Side + 1);
				L.Lod = Lod;
				L.Burnt = { 1 };
				L.Rag = 0.35;
				for (int I = 0; I <= 10; ++I)
				{
					const double U = Unit(Hash(L.Key, static_cast<uint64_t>(I)));
					L.Top.push_back({ NL * I / 10.0, NH - (U < 0.3 ? R.Range(1.0, 4.5) : 0.0) });
				}
				M.Build(L);
				// (the tall windows: a nave storey's window is the wall from 2.5 m to 10 m: cut as holes)
			}
			// the nave's tall windows as holes in both long walls are given by the pieces of one storey of GroundH; for a
			// church the storey is taller: add the upper wall over the windows' heads to the broken top
			for (int Side = -1; Side <= 1; Side += 2)
			{
				WallLay L;
				L.A = Side < 0 ? V3d{ NX0, -0.5 * ND, 0 } : V3d{ NX1, 0.5 * ND, 0 };
				L.T = Side < 0 ? V3d{ 1, 0, 0 } : V3d{ -1, 0, 0 };
				L.Out = { 0, static_cast<double>(Side), 0 };
				L.Len = NL;
				L.Storeys = 1;
				L.Bays = 0;
				L.Thick = 0.9;
				L.Key = Key ^ static_cast<uint64_t>(0xC20 + Side + 1);
				L.Lod = Lod;
				L.Rag = 0.3;
				L.Foot = GroundH + 0.7;
				for (int I = 0; I <= 10; ++I)
				{
					const double U = Unit(Hash(Key ^ static_cast<uint64_t>(0xC10 + Side + 1), static_cast<uint64_t>(I)));
					L.Top.push_back({ NL * I / 10.0, NH - (U < 0.3 ? R.Range(1.0, 4.5) : 0.0) });
				}
				// (the window heads' pointed arches: the wall's openings run up to 9 m; the pieces between them)
				const double Bay = NL / 6.0;
				for (int B = 0; B < 6; ++B) L.Holes.push_back({ (B + 0.5) * Bay, 6.6, 0.8, 2.6 });
				M.Build(L);
			}
			// the east end (a blank gable, broken) and the tower
			{
				WallLay L;
				L.A = { NX1, -0.5 * ND, 0 };
				L.T = { 0, 1, 0 };
				L.Out = { 1, 0, 0 };
				L.Len = ND;
				L.Bays = 0;
				L.Thick = 0.9;
				L.Key = Key ^ 0xC30;
				L.Lod = Lod;
				L.Rag = 0.4;
				L.Top = { { 0.0, NH }, { 0.5 * ND, NH + 5.0 - R.Range(0.0, 4.0) }, { ND, NH } };
				L.Holes.push_back({ 0.5 * ND, 8.0, 1.8, 3.2 });
				M.Build(L);
			}
			for (int Side = 0; Side < 4; ++Side)
			{
				WallLay L;
				const double X0 = NX0 - TS, X1 = NX0, Y0 = -0.5 * TS, Y1 = 0.5 * TS;
				switch (Side)
				{
				case 0: L.A = { X0, Y0, 0 }; L.T = { 1, 0, 0 }; L.Out = { 0, -1, 0 }; break;
				case 1: L.A = { X1, Y0, 0 }; L.T = { 0, 1, 0 }; L.Out = { 1, 0, 0 }; break;
				case 2: L.A = { X1, Y1, 0 }; L.T = { -1, 0, 0 }; L.Out = { 0, 1, 0 }; break;
				default: L.A = { X0, Y1, 0 }; L.T = { 0, -1, 0 }; L.Out = { -1, 0, 0 }; break;
				}
				L.Len = TS;
				L.Bays = 0;
				L.Thick = 1.0;
				if (Side % 2 == 1) L.Skip = L.Thick; // (the ends between the long walls)
				L.Face = 1;
				L.Key = Key ^ static_cast<uint64_t>(0xC40 + Side);
				L.Lod = Lod;
				L.Top = { { 0.0, TH }, { TS, TH } };
				// the belfry's openings and a shell's scar
				L.Holes.push_back({ 0.5 * TS, TH - 4.0, 1.1, 2.0 });
				if (Side == 0) L.Holes.push_back({ 0.5 * TS, 2.2, 1.0, 2.0 }); // (the west door)
				if (Side == 1 || Side == 3) L.Holes.push_back({ 0.5 * TS + R.Range(-1.5, 1.5), R.Range(14.0, 24.0), 1.3, 1.1 });
				M.Build(L);
			}
			// the spire: eight-sided, from the tower's top to 56 m, its last metres gone, its ribs standing at the break
			{
				const double Base = TH, Tip = TH + 22.0, Break = TH + 15.0 + R.Range(0.0, 3.0);
				const double CX = NX0 - 0.5 * TS, Rad = 0.5 * TS - 0.2;
				const int Sides = 8;
				for (int K = 0; K < Sides; ++K)
				{
					const double A0 = 2.0 * Pi * (K + 0.5) / Sides, A1 = 2.0 * Pi * (K + 1.5) / Sides;
					const double T = (Break - Base) / (Tip - Base);
					const V3d P0{ CX + Rad * std::cos(A0), Rad * std::sin(A0), Base }, P1{ CX + Rad * std::cos(A1), Rad * std::sin(A1), Base };
					const V3d Q0{ CX + Rad * (1.0 - T) * std::cos(A0), Rad * (1.0 - T) * std::sin(A0), Break };
					const V3d Q1{ CX + Rad * (1.0 - T) * std::cos(A1), Rad * (1.0 - T) * std::sin(A1), Break };
					// (one face torn down further, its rib bare)
					const double Cut = K == static_cast<int>(Key % 8) ? 0.55 : K == static_cast<int>((Key / 8) % 8) ? 0.8 : 1.0;
					const V3d Q0c = P0 + (Q0 - P0) * Cut, Q1c = P1 + (Q1 - P1) * Cut;
					const V3d N = Norm3(Cross3(P1 - P0, Q0c - P0));
					const Tone C = Varied(P.Roof, 0.95 + 0.1 * Noise(A0, 0.5, Key), 0.0);
					const uint32_t I0 = W.Vert(P0.X, P0.Y, P0.Z, N.X, N.Y, N.Z, C), I1 = W.Vert(P1.X, P1.Y, P1.Z, N.X, N.Y, N.Z, C);
					const uint32_t I2 = W.Vert(Q1c.X, Q1c.Y, Q1c.Z, N.X, N.Y, N.Z, Mixed(C, P.Soot, 0.4)), I3 = W.Vert(Q0c.X, Q0c.Y, Q0c.Z, N.X, N.Y, N.Z, Mixed(C, P.Soot, 0.4));
					W.Tri(I0, I1, I2);
					W.Tri(I0, I2, I3);
					// (inside face, dark)
					const uint32_t J0 = W.Vert(P0.X, P0.Y, P0.Z, -N.X, -N.Y, -N.Z, P.Char), J1 = W.Vert(P1.X, P1.Y, P1.Z, -N.X, -N.Y, -N.Z, P.Char);
					const uint32_t J2 = W.Vert(Q1c.X, Q1c.Y, Q1c.Z, -N.X, -N.Y, -N.Z, P.Char), J3 = W.Vert(Q0c.X, Q0c.Y, Q0c.Z, -N.X, -N.Y, -N.Z, P.Char);
					W.Tri(J0, J2, J1);
					W.Tri(J0, J3, J2);
					// the rib up from the break toward where the tip was
					if (Lod == 0)
					{
						const double Up = R.Range(0.2, 0.6);
						const V3d RibTop{ CX + Rad * (1.0 - T - Up * (1.0 - T)) * std::cos(A0), Rad * (1.0 - T - Up * (1.0 - T)) * std::sin(A0), Break + Up * (Tip - Break) };
						Beam(W, Q0, RibTop, 0.18, P.Char);
					}
				}
			}
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, NX0 - TS, NX1, -0.5 * ND, 0.5 * ND, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, P.Char, 0.4) : P.Inner; });
			Heap(W, NX0 + 0.55 * NL, 0.0, 0.32 * NL, 0.4 * ND, 3.0, Key ^ 0x4EAB, P, Lod, 40, 8);
		}

		/** The landmark dome: a hall 24 m square and 15 m high, a drum on it, its dome's skin half fallen and its ribs
		 * standing (the ribs carry it from everywhere). */
		void DomeHall(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			Paints Q = P;
			Q.Face[0] = Varied(P.Trim, 0.95, 0.0);
			Q.Face[1] = Varied(P.Face[2], 1.0, 0.0);
			Q.Face[2] = Varied(P.Trim, 0.9, 0.0);
			const double S = 24.0, H = 15.0, DrumR = 9.0, DrumH = 5.0;
			if (Lod == 2)
			{
				Plain(W, -0.5 * S, 0.5 * S, -0.5 * S, 0.5 * S, -1.0, H, Mixed(Q.Face[0], Rgb(60, 56, 52), 0.2), Mixed(P.Dust, Q.Face[0], 0.5));
				Plain(W, -DrumR * 0.8, DrumR * 0.8, -DrumR * 0.8, DrumR * 0.8, H, H + DrumH + 6.0, Mixed(Q.Face[0], Rgb(60, 56, 52), 0.25), P.Roof);
				return;
			}
			Mason M(W, Q);
			Rng R(Key ^ 0xD0E);
			for (int Side = 0; Side < 4; ++Side)
			{
				WallLay L;
				switch (Side)
				{
				case 0: L.A = { -0.5 * S, -0.5 * S, 0 }; L.T = { 1, 0, 0 }; L.Out = { 0, -1, 0 }; break;
				case 1: L.A = { 0.5 * S, -0.5 * S, 0 }; L.T = { 0, 1, 0 }; L.Out = { 1, 0, 0 }; break;
				case 2: L.A = { 0.5 * S, 0.5 * S, 0 }; L.T = { -1, 0, 0 }; L.Out = { 0, 1, 0 }; break;
				default: L.A = { -0.5 * S, 0.5 * S, 0 }; L.T = { 0, -1, 0 }; L.Out = { -1, 0, 0 }; break;
				}
				L.Len = S;
				L.Storeys = 3;
				L.Bays = 5;
				L.Thick = 0.8;
				if (Side % 2 == 1) L.Skip = L.Thick; // (the ends between the long walls)
				L.Face = Side % 3;
				L.Key = Key ^ static_cast<uint64_t>(0xD10 + Side);
				L.Lod = Lod;
				L.Burnt = { 1, 1, 0 };
				L.Rag = 0.3;
				L.Top = { { 0.0, H }, { S, H } };
				if (Side == 1) L.Holes.push_back({ R.Range(6.0, 18.0), R.Range(4.0, 10.0), 2.4, 2.0 });
				M.Build(L);
			}
			// the drum: a ring of piers with windows between, round
			const int Round = Lod == 0 ? 16 : 10;
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round, A1 = A + 2.0 * Pi / Round * 0.5;
				const V3d P0{ DrumR * std::cos(A), DrumR * std::sin(A), H }, P1{ DrumR * std::cos(A1), DrumR * std::sin(A1), H };
				const V3d U = Norm3(P1 - P0), Up{ 0, 0, 1 }, V = Cross3(Up, U);
				const double L = std::sqrt(Dot3(P1 - P0, P1 - P0));
				Block(W, P0, U, V, Up, 0.0, L, -0.7, 0.0, 0.0, DrumH, false, [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? P.Dust : Varied(Q.Face[0], 0.95, 0.0); });
			}
			// the dome: ribs from the drum to the lantern; the skin between them kept on one side only
			const int Ribs = Lod == 0 ? 16 : 8, Arcs = Lod == 0 ? 8 : 4;
			const double DomeR = DrumR - 0.2, Base = H + DrumH;
			const double Torn = 2.0 * Pi * Unit(Hash(Key, 0xD20));
			for (int K = 0; K < Ribs; ++K)
			{
				const double A = 2.0 * Pi * K / Ribs, A1 = 2.0 * Pi * (K + 1) / Ribs;
				const double Gone = std::cos(A + Pi / Ribs - Torn);
				const int Stop = Gone > 0.3 ? (Unit(Hash(Key, static_cast<uint64_t>(K))) < 0.4 ? Arcs / 2 : Arcs - 1) : Arcs;
				for (int J = 0; J < Stop; ++J)
				{
					const double E0 = 0.5 * Pi * J / Arcs, E1 = 0.5 * Pi * (J + 1) / Arcs;
					auto At = [&](double Az, double El) { return V3d{ DomeR * std::cos(El) * std::cos(Az), DomeR * std::cos(El) * std::sin(Az), Base + DomeR * 0.95 * std::sin(El) }; };
					Beam(W, At(A, E0), At(A, E1), 0.35, Varied(P.Roof, 0.8, 0.0));
					// (the skin between this rib and the next, where it is kept: the side away from the hit)
					if (Gone < -0.1 || (Gone < 0.3 && J < Arcs / 2))
					{
						const V3d Q0 = At(A, E0), Q1 = At(A1, E0), Q2 = At(A1, E1), Q3 = At(A, E1);
						const V3d N = Norm3((Q0 + Q1 + Q2 + Q3) * 0.25 - V3d{ 0, 0, Base });
						const Tone C = Varied(P.Roof, 0.92 + 0.12 * Noise(A, E0, Key), 0.0);
						const uint32_t I0 = W.Vert(Q0.X, Q0.Y, Q0.Z, N.X, N.Y, N.Z, C), I1 = W.Vert(Q1.X, Q1.Y, Q1.Z, N.X, N.Y, N.Z, C);
						const uint32_t I2 = W.Vert(Q2.X, Q2.Y, Q2.Z, N.X, N.Y, N.Z, C), I3 = W.Vert(Q3.X, Q3.Y, Q3.Z, N.X, N.Y, N.Z, C);
						W.Tri(I0, I1, I2);
						W.Tri(I0, I2, I3);
						const uint32_t J0 = W.Vert(Q0.X, Q0.Y, Q0.Z, -N.X, -N.Y, -N.Z, P.Char), J1 = W.Vert(Q1.X, Q1.Y, Q1.Z, -N.X, -N.Y, -N.Z, P.Char);
						const uint32_t J2 = W.Vert(Q2.X, Q2.Y, Q2.Z, -N.X, -N.Y, -N.Z, P.Char), J3 = W.Vert(Q3.X, Q3.Y, Q3.Z, -N.X, -N.Y, -N.Z, P.Char);
						W.Tri(J0, J2, J1);
						W.Tri(J0, J3, J2);
					}
				}
			}
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -0.5 * S, 0.5 * S, -0.5 * S, 0.5 * S, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, P.Char, 0.35) : P.Inner; });
			Heap(W, 2.0, -1.0, 7.0, 6.0, 3.2, Key ^ 0x4EAC, P, Lod, 40, 6);
		}

		/** The landmark station: a hall 54 by 30 m under iron arches, the glass gone, the end screen's frame standing, a
		 * clock tower at its front. Along x, its front at -x. */
		void Station(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			const double L = 54.0, D = 30.0, Wall = 9.0, Arch = 9.0;
			Paints Q = P;
			if (Lod == 2)
			{
				Plain(W, -0.5 * L, 0.5 * L, -0.5 * D, 0.5 * D, -1.0, Wall, Mixed(P.Face[0], Rgb(60, 56, 52), 0.2), Mixed(P.Dust, P.Roof, 0.4));
				Plain(W, -0.5 * L - 6.0, -0.5 * L, -3.5, 3.5, -1.0, 30.0, Mixed(P.Trim, Rgb(60, 56, 52), 0.2), P.Roof);
				return;
			}
			Mason M(W, Q);
			Rng R(Key ^ 0x57A7);
			for (int Side = -1; Side <= 1; Side += 2)
			{
				WallLay Lw;
				Lw.A = Side < 0 ? V3d{ -0.5 * L, -0.5 * D, 0 } : V3d{ 0.5 * L, 0.5 * D, 0 };
				Lw.T = Side < 0 ? V3d{ 1, 0, 0 } : V3d{ -1, 0, 0 };
				Lw.Out = { 0, static_cast<double>(Side), 0 };
				Lw.Len = L;
				Lw.Storeys = 2;
				Lw.Bays = 9;
				Lw.Thick = 0.7;
				Lw.Face = Side < 0 ? 0 : 2;
				Lw.Key = Key ^ static_cast<uint64_t>(0x5710 + Side + 1);
				Lw.Lod = Lod;
				Lw.Rag = 0.3;
				for (int I = 0; I <= 12; ++I)
				{
					const double U = Unit(Hash(Lw.Key, static_cast<uint64_t>(I)));
					Lw.Top.push_back({ L * I / 12.0, Wall - (U < 0.25 ? R.Range(1.0, 4.0) : 0.0) });
				}
				M.Build(Lw);
			}
			// the arches: iron ribs across the depth every 6 m, a few fallen
			const int Ribs = Lod == 0 ? 10 : 5, Segs = Lod == 0 ? 10 : 6;
			for (int K = 0; K <= Ribs; ++K)
			{
				const double X = -0.5 * L + L * K / Ribs;
				if (Unit(Hash(Key ^ 0x57A0, static_cast<uint64_t>(K))) < 0.2) continue;
				for (int J = 0; J < Segs; ++J)
				{
					const double A0 = Pi * J / Segs, A1 = Pi * (J + 1) / Segs;
					const V3d P0{ X, -0.5 * D * std::cos(A0), Wall + Arch * std::sin(A0) }, P1{ X, -0.5 * D * std::cos(A1), Wall + Arch * std::sin(A1) };
					Beam(W, P0, P1, 0.3, Rgb(64, 60, 58));
				}
			}
			// the purlins along it, a few, and the front screen's frame
			for (int J = 1; J < 6; ++J)
			{
				const double A = Pi * J / 6.0;
				if (Unit(Hash(Key ^ 0x57A1, static_cast<uint64_t>(J))) < 0.35) continue;
				Beam(W, V3d{ -0.5 * L, -0.5 * D * std::cos(A), Wall + Arch * std::sin(A) }, V3d{ 0.5 * L, -0.5 * D * std::cos(A), Wall + Arch * std::sin(A) }, 0.2, Rgb(70, 66, 62));
			}
			// the clock tower at the front
			for (int Side = 0; Side < 4; ++Side)
			{
				WallLay Lw;
				const double X0 = -0.5 * L - 6.5, X1 = -0.5 * L + 0.5, Y0 = -3.5, Y1 = 3.5;
				switch (Side)
				{
				case 0: Lw.A = { X0, Y0, 0 }; Lw.T = { 1, 0, 0 }; Lw.Out = { 0, -1, 0 }; Lw.Len = X1 - X0; break;
				case 1: Lw.A = { X1, Y0, 0 }; Lw.T = { 0, 1, 0 }; Lw.Out = { 1, 0, 0 }; Lw.Len = Y1 - Y0; break;
				case 2: Lw.A = { X1, Y1, 0 }; Lw.T = { -1, 0, 0 }; Lw.Out = { 0, 1, 0 }; Lw.Len = X1 - X0; break;
				default: Lw.A = { X0, Y1, 0 }; Lw.T = { 0, -1, 0 }; Lw.Out = { -1, 0, 0 }; Lw.Len = Y1 - Y0; break;
				}
				Lw.Bays = 0;
				Lw.Thick = 0.8;
				if (Side % 2 == 1) Lw.Skip = Lw.Thick; // (the ends between the long walls)
				Lw.Face = 1;
				Lw.Key = Key ^ static_cast<uint64_t>(0x5720 + Side);
				Lw.Lod = Lod;
				Lw.Top = { { 0.0, 30.0 }, { Lw.Len, 30.0 } };
				Lw.Holes.push_back({ 0.5 * Lw.Len, 26.0, 1.2, 1.2 }); // (the clock's face, gone)
				if (Side == 0 || Side == 3) Lw.Holes.push_back({ 0.5 * Lw.Len, 2.4, 1.4, 2.2 });
				M.Build(Lw);
			}
			Block(W, V3d{}, V3d{ 1, 0, 0 }, V3d{ 0, 1, 0 }, V3d{ 0, 0, 1 }, -0.5 * L, 0.5 * L, -0.5 * D, 0.5 * D, -0.3, 0.04, false,
			      [&](const V3d&, const V3d& Nn) { return Nn.Z > 0.5 ? Mixed(P.Dust, Rgb(70, 66, 60), 0.4) : P.Inner; });
			// (one draw a line: the order of two in one call's arguments is the compiler's, and gcc and clang differ)
			const double HX = R.Range(-10.0, 10.0);
			const double HY = R.Range(-6.0, 6.0);
			Heap(W, HX, HY, 8.0, 6.0, 2.4, Key ^ 0x4EAD, P, Lod, 30, 8);
		}

		/** A facade standing alone, its building gone: 12 m long, three storeys, broken, its windows open to the sky. */
		void LoneWall(Writer& W, const Paints& P, uint64_t Key, int Lod)
		{
			Mason M(W, P);
			Rng R(Key ^ 0x10E);
			WallLay L;
			L.A = { -6.0, 0.2, 0 };
			L.T = { 1, 0, 0 };
			L.Out = { 0, -1, 0 };
			L.Len = 12.0;
			L.Storeys = 3;
			L.Bays = 3;
			L.Face = static_cast<int>(Key % 3);
			L.Key = Key;
			L.Lod = Lod;
			L.Burnt = { 1, 1, 1 };
			L.Rag = 0.4;
			for (int I = 0; I <= 8; ++I)
			{
				const double U = static_cast<double>(I) / 8.0;
				L.Top.push_back({ 12.0 * U, std::max(1.5, EavesOf(3) * (0.95 - 0.7 * std::fabs(U - 0.35)) + R.Range(-1.0, 1.0)) });
			}
			M.Build(L);
			Heap(W, 0.0, 3.0, 6.5, 3.5, 2.0, Key ^ 0x4EAE, P, Lod, 26, 4);
		}

		/** A tree torn by shellfire: its trunk snapped some metres up, a few broken limbs, a handful of leaves left. */
		void ShellTree(Writer& W, uint64_t Key, int Lod, bool bStump)
		{
			Rng R(Key ^ 0x7EE);
			const Tone Bark = Rgb(84, 74, 62), Raw = Rgb(176, 156, 120), Leaf = Rgb(78, 92, 54);
			const double High = bStump ? R.Range(0.6, 1.4) : R.Range(3.5, 6.5), Rad = bStump ? 0.32 : 0.26;
			const int Round = Lod == 0 ? 7 : 5;
			// the trunk, tapering, its top splintered (the points pulled up and down)
			std::vector<uint32_t> Lo, Hi;
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round, X = std::cos(A), Y = std::sin(A);
				const double Splinter = R.Range(-0.6, 0.8);
				Lo.push_back(W.Vert(X * Rad, Y * Rad, -0.3, X, Y, 0, Bark));
				Hi.push_back(W.Vert(X * Rad * 0.75, Y * Rad * 0.75, High + Splinter, X, Y, 0.2, Mixed(Bark, Raw, 0.6)));
			}
			for (int K = 0; K < Round; ++K)
			{
				W.Tri(Lo[static_cast<size_t>(K)], Lo[static_cast<size_t>((K + 1) % Round)], Hi[static_cast<size_t>((K + 1) % Round)]);
				W.Tri(Lo[static_cast<size_t>(K)], Hi[static_cast<size_t>((K + 1) % Round)], Hi[static_cast<size_t>(K)]);
			}
			const uint32_t Mid = W.Vert(0, 0, High + 0.2, 0, 0, 1, Raw);
			for (int K = 0; K < Round; ++K) W.Tri(Mid, Hi[static_cast<size_t>(K)], Hi[static_cast<size_t>((K + 1) % Round)]);
			if (bStump || Lod == 2) return;
			// limbs and leaves
			const int Limbs = 3 + static_cast<int>(R.Next() * 3.0);
			for (int K = 0; K < Limbs; ++K)
			{
				const double A = R.Range(0.0, 2.0 * Pi), Z = R.Range(0.5, 0.95) * High, Len = R.Range(1.0, 3.2), Lift = R.Range(0.2, 1.0);
				const V3d From{ 0.2 * std::cos(A), 0.2 * std::sin(A), Z };
				const V3d To{ Len * std::cos(A), Len * std::sin(A), Z + Lift * Len };
				Beam(W, From, To, 0.12, Bark);
				if (R.Next() < 0.6)
				{
					const double S = R.Range(0.5, 0.9);
					W.Bead(To.X, To.Y, To.Z + 0.2, S, S * 0.8, Varied(Leaf, 1.1, 0.0), Varied(Leaf, 0.7, 0.0));
				}
			}
		}

		/** A street lamp: an iron post 5.5 m, its arm and lantern; bent over where a tank or a blast caught it. */
		void Lamp(Writer& W, bool bBent)
		{
			const Tone Iron = Rgb(56, 58, 58);
			if (!bBent)
			{
				Beam(W, V3d{ 0, 0, -0.3 }, V3d{ 0, 0, 5.5 }, 0.14, Iron);
				Beam(W, V3d{ 0, 0, 5.3 }, V3d{ 0.9, 0, 5.5 }, 0.07, Iron);
				W.Box(0.95, 0.0, 5.25, 0.18, 0.18, 0.22, 0.0, Rgb(70, 70, 66));
				Plain(W, -0.22, 0.22, -0.22, 0.22, -0.3, 0.6, Iron, Iron);
				return;
			}
			Beam(W, V3d{ 0, 0, -0.3 }, V3d{ 0, 0, 1.4 }, 0.14, Iron);
			Beam(W, V3d{ 0, 0, 1.4 }, V3d{ 2.6, 0.3, 3.4 }, 0.13, Iron);
			Beam(W, V3d{ 2.6, 0.3, 3.4 }, V3d{ 4.0, 0.5, 2.6 }, 0.12, Iron);
			Plain(W, -0.22, 0.22, -0.22, 0.22, -0.3, 0.6, Iron, Iron);
		}

		/** A burnt-out motor car: its body rusted and black, its glass gone, on its rims. Along x. */
		void Wreck(Writer& W, uint64_t Key, int Lod)
		{
			Rng R(Key ^ 0xC4A);
			const Tone Rust = Rgb(92, 62, 46), Black = Rgb(40, 36, 34), Ash = Rgb(110, 104, 96);
			auto Ink = [&](const V3d& Q, const V3d& N) {
				const double K = 0.5 + 0.5 * Noise(Q.X * 1.3, Q.Y * 1.3 + Q.Z, Key);
				return N.Z > 0.5 ? Mixed(Black, Ash, 0.3 * K) : Mixed(Rust, Black, 0.6 * K);
			};
			const V3d O{}, U{ 1, 0, 0 }, V{ 0, 1, 0 }, Up{ 0, 0, 1 };
			Block(W, O, U, V, Up, -2.1, 2.1, -0.82, 0.82, 0.25, 0.95, true, Ink);
			Block(W, O, U, V, Up, -0.9, 0.9, -0.74, 0.74, 0.95, 1.45, false, Ink);
			if (Lod == 0)
			{
				// (its bonnet sprung, its wheels' rims)
				Block(W, V3d{ 1.4, 0.0, 0.98 }, Norm3(V3d{ 1.0, 0.0, 0.25 }), V, Norm3(V3d{ -0.25, 0.0, 1.0 }), -0.7, 0.7, -0.7, 0.7, 0.0, 0.04, true, Ink);
				for (int K = 0; K < 4; ++K)
				{
					const double X = K < 2 ? -1.35 : 1.35, Y = K % 2 == 0 ? -0.8 : 0.8;
					W.Box(X, Y, 0.3, 0.32, 0.08, 0.3, 0.0, Black);
				}
			}
			(void)R;
		}

		// ------------------------------------------------------------------ the city itself

		/** The city's streets between its blocks: a grid of its own (the core's old quarter on a tighter one, turned), cut
		 * by the rules' avenues and boulevards (its lanes). */
		struct Grid
		{
			double CX = 0.0, CY = 0.0, Axis = 0.0;
			double PU = 84.0, PV = 62.0, Street = 12.0;
			/** The grid's frame at a place: along (U) and across (V), from its origin. */
			void ToGrid(double X, double Y, double& U, double& V) const
			{
				const double C = std::cos(Axis), S = std::sin(Axis), DX = X - CX, DY = Y - CY;
				U = DX * C + DY * S;
				V = -DX * S + DY * C;
			}
			void ToWorld(double U, double V, double& X, double& Y) const
			{
				const double C = std::cos(Axis), S = std::sin(Axis);
				X = CX + U * C - V * S;
				Y = CY + U * S + V * C;
			}
			/** Metres into the nearest street (positive inside it, its middle the most), else negative: the distance to it. */
			double StreetAt(double X, double Y) const
			{
				double U, V;
				ToGrid(X, Y, U, V);
				const double Mu = U - PU * std::floor(U / PU), Mv = V - PV * std::floor(V / PV);
				const double Du = std::min(Mu, PU - Mu), Dv = std::min(Mv, PV - Mv); // (from the street's middle at 0 and PU)
				return 0.5 * Street - std::min(Du, Dv);
			}
		};

		class City final : public Region
		{
		public:
			City(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				const char* C = ClimateAt(F.Lat, F.Lon);
				const bool bHot = std::strcmp(C, "desert") == 0 || std::strcmp(C, "drysouth") == 0 || std::strcmp(C, "jungle") == 0 ||
				                  std::strcmp(C, "monsoon") == 0 || std::strcmp(C, "savanna") == 0;
				// (the style: brick and slate in the north and west, plaster and plank east of the Oder, stone and plaster and
				// tile in the south and wherever the climate is hot)
				Sty = bHot || F.Lat < 44.5 ? South : (F.Lon >= 15.0 && F.Lat < 62.0) ? East : North;
				Pt = PaintsOf(Sty);
				// (the open ground's grass by the climate: a worn green in the temperate north, straw by the Mediterranean, the
				// east's steppe grass, the tropics' lush)
				if (std::strcmp(C, "desert") == 0) Grass = Rgb(176, 156, 116);
				else if (std::strcmp(C, "drysouth") == 0 || std::strcmp(C, "savanna") == 0) Grass = Rgb(146, 134, 92);
				else if (std::strcmp(C, "jungle") == 0 || std::strcmp(C, "monsoon") == 0) Grass = Rgb(84, 104, 60);
				else if (std::strcmp(C, "steppe") == 0 || std::strcmp(C, "prairie") == 0) Grass = Rgb(132, 124, 82);
				else Grass = Rgb(98, 106, 68);
				if (F.Month >= 7 && F.Month <= 9) Grass = Mixed(Grass, Rgb(150, 138, 96), 0.25);
				if (F.Month == 11 || F.Month <= 3) Grass = Mixed(Grass, Rgb(112, 104, 82), 0.35);
				// the town's middle, its grid's turn, and the landmark's kind
				CX = F.bTown ? F.TownX : 0.5 * F.Width();
				CY = F.bTown ? F.TownY : 0.66 * F.Depth();
				const double Turn = 0.5 * Pi * Unit(Hash(S, 0xA415));
				Main.CX = CX;
				Main.CY = CY;
				Main.Axis = Turn;
				Old.CX = CX + 13.0;
				Old.CY = CY - 7.0;
				Old.Axis = Turn + 0.32 + 0.25 * (Unit(Hash(S, 0xA416)) - 0.5);
				Old.PU = 52.0;
				Old.PV = 44.0;
				Old.Street = 8.5;
				Suburb = Main;
				Suburb.PU = 96.0;
				Suburb.PV = 72.0;
				Suburb.Street = 9.0;
				const double Pick = Unit(Hash(S, 0xA417));
				Mark = Sty == South ? (Pick < 0.55 ? KDome : Pick < 0.8 ? KSpire : KStation) : Sty == East ? (Pick < 0.45 ? KSpire : Pick < 0.8 ? KDome : KStation) : (Pick < 0.65 ? KSpire : KStation);
			}
			Look Which() const override { return Look::City; }

			// ---------------------------------------------------------------- the rules' cells

			/** The rules' ground at a place (its name), clamped to the field (the band carries the edge's on). */
			const std::string& CellGround(double X, double Y) const
			{
				static const std::string None = "open";
				if (Fd.Cols <= 0 || Fd.Rows <= 0 || Fd.Ground.empty()) return None;
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1);
				const int J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * static_cast<size_t>(Fd.Cols) + static_cast<size_t>(I);
				if (K >= Fd.Ground.size()) return None;
				const int G = Fd.Ground[K];
				return G >= 0 && static_cast<size_t>(G) < Fd.Grounds.size() ? Fd.Grounds[static_cast<size_t>(G)] : None;
			}
			int CellStoreys(double X, double Y) const
			{
				if (Fd.Storeys.empty() || Fd.Cols <= 0) return 0;
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1);
				const int J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * static_cast<size_t>(Fd.Cols) + static_cast<size_t>(I);
				return K < Fd.Storeys.size() ? Fd.Storeys[K] : 0;
			}
			/** What the city is here: 0 none, 1 suburb, 2 terraces and core (buildings), 3 industry, 4 park or square. Beyond
			 * the band nothing; a field with no city cells at all (a recording that names the look) lays the art director's
			 * rings round its town instead. */
			int Quarter(double X, double Y) const
			{
				if (X < -Band0 || Y < -Band0 || X > Fd.Width() + Band0 || Y > Fd.Depth() + Band0) return 0;
				if (bRings)
				{
					// (the rings from the town's middle: core, terraces, suburbs)
					const double D = std::hypot(X - CX, (Y - CY) * 1.25);
					const double R = Fd.bTown ? std::max(Fd.TownR, 200.0) : 400.0;
					const double Wob = 60.0 * Noise(X / 210.0, Y / 210.0, Seed ^ 0xC17);
					// (core and terraces both buildings: StoreysAt tells them apart by the same rings)
					if (D < 1.25 * R + Wob) return 2;
					// (each 25 m cell its own, through a signed whole number: a negative double cast straight to an
					// unsigned one is undefined, and the Mac's ARM and the cloud's x86 answer it differently)
					const uint64_t CellX = static_cast<uint64_t>(static_cast<int64_t>(std::floor(X / 25.0)));
					const uint64_t CellY = static_cast<uint64_t>(static_cast<int64_t>(std::floor(Y / 25.0)));
					if (D < 2.1 * R + Wob) return Unit(Hash(Seed, CellX, CellY)) < 0.75 ? 1 : 0;
					return 0;
				}
				const std::string& G = CellGround(X, Y);
				if (G == "buildings") return 2;
				if (G == "suburb") return 1;
				if (G == "industry") return 3;
				if (G == "park") return 4;
				return 0;
			}
			/** Storeys at a place: the rules' where they give them, else by the ring. */
			int StoreysAt(double X, double Y) const
			{
				const int S = CellStoreys(X, Y);
				if (S > 0) return std::clamp(S, 2, 5);
				const double D = std::hypot(X - CX, Y - CY);
				return D < 220.0 ? 5 : D < 420.0 ? 4 : D < 600.0 ? 3 : 2;
			}
			/** The grid a place is laid on: the old quarter's (four and five storeys in the core), the suburbs', the main. */
			const Grid& GridAt(double X, double Y) const
			{
				const int Q = Quarter(X, Y);
				if (Q == 1) return Suburb;
				if (Q == 2 && StoreysAt(X, Y) >= 4 && std::hypot(X - CX, Y - CY) < 420.0) return Old;
				return Main;
			}
			/** How ruined the city is about a place, 0 to 1: the core and its edge toward the attack the worst, patchy. */
			double Ruin(double X, double Y) const
			{
				const double Patch = 0.5 + 0.5 * Noise(X / 140.0, Y / 140.0, Seed ^ 0xD4A);
				return Clamp01(0.45 + 0.4 * Patch + 0.15 * Smooth(900.0, 200.0, std::hypot(X - CX, Y - CY)));
			}

			// ---------------------------------------------------------------- the land

			double Relief(double X, double Y, double Z) const override
			{
				// (the city stands on its rules' level ground; outside it, a gentle roll)
				return Quarter(X, Y) ? Z : Z + 0.3 * Fbm(X, Y, 260.0, 2, Seed ^ 0xD1);
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.0; } // (a paved street, level with its pavements)
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 6.5 : 4.0) + 1.5 * U; } // (avenues and boulevards)
			double PlotMin() const override { return 18000.0; }
			double PlotMax() const override { return 40000.0; }
			double FarTilled() const override { return 0.3; }
			double WoodsEdge() const override { return 0.4; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return 3;
				if (Kind == KField) return U < 0.6 ? 1 : 2;
				return U < 0.85 ? 0 : 3;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case 0: return "waste";
				case 1: return "allotments";
				case 2: return "stubble";
				case 3: return "scrub";
				default: return "";
				}
			}

			// ---------------------------------------------------------------- the paint

			/** The ground of a city street: dark setts in the core, worn flags at its sides, the suburbs' pale dust; masonry
			 * dust lying in drifts (thickest by the ruins), scorched where things burnt. */
			Tone StreetTone(const Spot& S, bool bSuburb) const
			{
				const double N1 = S.N1, Dust = Smooth(-0.2, 0.6, Noise(S.X / 23.0, S.Y / 23.0, Seed ^ 0x5D) + 0.3 * S.N2);
				const Tone Setts = Varied(Rgb(84, 80, 76), 1.0 + 0.08 * N1, 0.01);
				const Tone Road = Varied(Rgb(150, 140, 120), 1.0 + 0.05 * N1, 0.02);
				Tone T = bSuburb ? Road : Setts;
				T = Mixed(T, Pt.Dust, (bSuburb ? 0.2 : 0.12) + 0.35 * Dust * Ruin(S.X, S.Y));
				// (scorch: burnt-out patches and a burnt vehicle's black)
				const double Burn = Smooth(0.55, 0.8, Noise(S.X / 31.0, S.Y / 31.0, Seed ^ 0x5E));
				return Mixed(T, Rgb(52, 48, 44), 0.45 * Burn);
			}

			Tone Colour(const Spot& S) const override
			{
				const int Q = Quarter(S.X, S.Y);
				if (S.Kind == KWater) return Rgb(70, 78, 76);
				if (S.Kind == KMarsh) return Varied(Rgb(96, 100, 76), 1.0 + 0.06 * S.N1, 0.0);
				if (S.Kind == KLane) return StreetTone(S, Q <= 1 && !(S.LaneHalf > 6.0));
				if (Q > 0 && S.Kind != KWoods)
				{
					if (S.Drawn == 9) return Mixed(Pt.Dust, Pt.Char, 0.4); // (a building's floor)
					const Grid& G = GridAt(S.X, S.Y);
					const double In = G.StreetAt(S.X, S.Y);
					if (In > -1.0) return StreetTone(S, Q == 1);
					// (the dust of the ruins lies over everything in the city, thicker where it is most broken)
					const double Lies = 0.1 + 0.3 * Ruin(S.X, S.Y);
					if (Q == 4)
					{
						// a park or square: worn grass, torn and trodden, earth thrown from shell holes
						const double Torn = Smooth(0.0, 0.5, Noise(S.X / 17.0, S.Y / 17.0, Seed ^ 0x9A) + 0.2 * S.N2);
						return Mixed(Mixed(Varied(Grass, 0.95 + 0.08 * S.N1, 0.0), Rgb(96, 84, 70), 0.55 * Torn), Pt.Dust, 0.8 * Lies);
					}
					if (Q == 1)
					{
						// a suburb's gardens: grass, beds and paths, a little trodden
						const double Bed = Smooth(0.2, 0.5, S.N2);
						return Mixed(Mixed(Varied(Grass, 0.98 + 0.08 * S.N1, 0.0), Rgb(104, 90, 72), 0.4 * Bed), Pt.Dust, 0.7 * Lies);
					}
					// a block's courtyard and the yards behind: trodden earth and ash, the rubble's dust
					const double Ash = Smooth(-0.3, 0.5, S.N1);
					return Mixed(Varied(Rgb(104, 96, 84), 1.0 + 0.05 * S.N2, 0.0), Mixed(Pt.Dust, Pt.Char, 0.5), 0.35 + 0.4 * Ash);
				}
				// the open ground about the city: grass worn by the war, allotments, waste ground and scrub
				switch (S.Kind)
				{
				case KField: return S.Crop == 1 ? Mixed(Varied(Grass, 0.95, 0.03), Rgb(108, 92, 70), 0.45 + 0.15 * S.N2) : Mixed(Grass, Rgb(170, 152, 106), 0.55);
				case KWoods: return Mixed(Varied(Grass, 0.72, 0.0), Rgb(70, 62, 50), 0.35);
				case KRough: return Mixed(Varied(Grass, 0.9, 0.02), Rgb(122, 110, 92), 0.35 + 0.2 * S.N1);
				case KWorks: return Mixed(Rgb(116, 102, 84), Grass, 0.25);
				default: break;
				}
				return Mixed(Varied(Grass, 1.0 + 0.07 * S.N1 + 0.03 * S.N2, 0.01 * S.Broad), Rgb(122, 112, 90), 0.15 + 0.15 * Smooth(0.2, 0.7, S.N1));
			}

			Tone Far(const Spot& S, bool bWoods, double /*Gap*/) const override
			{
				// (the city runs on beyond the field under the haze, as a carpet of roofs and ruins, so it never ends in a hard
				// edge in the middle of a field; fading out a kilometre or two beyond its last built edge)
				const double Ex = std::clamp(S.X, 0.0, Fd.Width()), Ey = std::clamp(S.Y, 0.0, Fd.Depth());
				const double Out = std::hypot(S.X - Ex, S.Y - Ey);
				double Built = 0.0;
				if (Quarter(Ex, Ey) > 0)
				{
					const double Reach = 900.0 + 700.0 * (0.5 + 0.5 * Noise(S.X / 700.0, S.Y / 700.0, Seed ^ 0xF4));
					Built = Smooth(Reach, 0.3 * Reach, Out);
				}
				const Tone Roofs = Mixed(Mixed(Pt.Roof, Pt.Face[0], 0.45), Rgb(64, 60, 56), 0.25 + 0.25 * (0.5 + 0.5 * S.N1));
				const Tone Land = bWoods ? Mixed(Varied(Grass, 0.65, 0.0), Rgb(58, 62, 44), 0.5) : Varied(Grass, 0.96 + 0.06 * S.Broad, 0.0);
				return Mixed(Land, Roofs, Built);
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 || S.Drawn == 10) return Grown{};
				const int Q = Quarter(S.X, S.Y);
				const double Ru = Ruin(S.X, S.Y);
				if (S.Kind == KLane)
				{
					// (the avenues swept by the traffic of the war: bricks and shards at their edges)
					const double Edge = Smooth(-2.5, 0.0, S.Lane);
					if (Pick < 0.45) return Grown{ ClutOf(CBrick), (0.15 + 0.55 * Edge) * Ru };
					if (Pick < 0.75) return Grown{ ClutOf(CTiles), (0.1 + 0.5 * Edge) * Ru };
					if (Pick < 0.9) return Grown{ ClutOf(CChunk), (0.04 + 0.2 * Edge) * Ru };
					return Grown{ ClutOf(CTimber), 0.04 * Ru };
				}
				if (Q == 2 || Q == 3)
				{
					// streets and courtyards: rubble thick, more by the walls
					const double Street = GridAt(S.X, S.Y).StreetAt(S.X, S.Y);
					const double ByWall = S.bNear ? 1.0 : Smooth(-1.0, -5.0, Street) * 0.5 + Smooth(2.0, 5.5, -Street) * 0.0;
					if (Pick < 0.38) return Grown{ ClutOf(CBrick), (0.55 + 0.6 * ByWall) * Ru };
					if (Pick < 0.6) return Grown{ ClutOf(CChunk), (0.18 + 0.3 * ByWall) * Ru };
					if (Pick < 0.82) return Grown{ ClutOf(CTiles), (0.4 + 0.4 * ByWall) * Ru };
					if (Pick < 0.92) return Grown{ ClutOf(CTimber), 0.12 * Ru };
					return Grown{ ClutOf(CWeed), 0.08 };
				}
				if (Q == 1 || Q == 4)
				{
					const double Street = GridAt(S.X, S.Y).StreetAt(S.X, S.Y);
					if (Street > -1.0)
					{
						if (Pick < 0.5) return Grown{ ClutOf(CBrick), 0.25 * Ru };
						return Pick < 0.8 ? Grown{ ClutOf(CTiles), 0.2 * Ru } : Grown{ ClutOf(CWeed), 0.12 };
					}
					if (Pick < 0.55) return Grown{ ClutOf(CGrass), 1.0 };
					if (Pick < 0.75) return Grown{ ClutOf(CWeed), 0.3 };
					if (Pick < 0.9) return Grown{ ClutOf(CBrick), 0.2 * Ru * (S.bNear ? 2.0 : 0.6) };
					return Grown{ ClutOf(CTiles), 0.15 * Ru * (S.bNear ? 2.0 : 0.5) };
				}
				if (S.Kind == KWoods || S.Kind == KMarsh) return Pick < 0.7 ? Grown{ ClutOf(CGrass), 0.8 } : Grown{ ClutOf(CWeed), 0.3 };
				if (Pick < 0.7) return Grown{ ClutOf(CGrass), 1.25 };
				if (Pick < 0.9) return Grown{ ClutOf(CWeed), 0.3 };
				return Grown{ ClutOf(CChunk), 0.03 };
			}

			bool Dress(LandField::Placed& Pl, const Spot& /*S*/, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(Pl.Kind) - static_cast<int>(LandField::Base::Clut0);
				switch (K)
				{
				case CBrick:
				case CTiles:
				case CChunk:
				{
					Pl.Scale = static_cast<float>(0.7 + 0.8 * U4 * U4);
					Pl.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					const double Pale = 0.88 + 0.22 * U7;
					// (a brick's red, a block's grey stone or plaster, by the pick)
					Pl.Tint = U6 < 0.65 ? Tint(Pale * 1.02, Pale, Pale * 0.97) : Tint(Pale * 1.1, Pale * 1.08, Pale * 1.06);
					return true;
				}
				case CTimber:
				{
					Pl.Scale = static_cast<float>(0.7 + 0.7 * U4);
					Pl.ScaleZ = static_cast<float>(0.8 + 0.4 * U5);
					const double Burnt = U6 < 0.6 ? 0.45 : 1.0;
					Pl.Tint = Tint(Burnt * (0.95 + 0.1 * U7), Burnt * 0.95, Burnt * 0.92);
					return true;
				}
				case CGrass:
				case CWeed:
				{
					Pl.Scale = static_cast<float>(0.75 + 0.5 * U4);
					Pl.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					Pl.Tint = Tint(0.95 + 0.1 * U6, 0.95 + 0.1 * U7, 0.93);
					return true;
				}
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				M.Paint = 0;
				switch (K)
				{
				case CBrick:
				{
					// three to five bricks lying about, whole and broken (a brick 0.22 by 0.1 by 0.065)
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0xB41, static_cast<uint64_t>(B))), D = 0.25 * Unit(Hash(0xB42, static_cast<uint64_t>(B)));
						const double Long = Unit(Hash(0xB43, static_cast<uint64_t>(B))) < 0.4 ? 0.11 : 0.22;
						const Tone C = B % 3 == 2 ? Varied(Pt.Rubble[1], 1.0, 0.0) : Varied(Pt.Core, 0.95 + 0.12 * Unit(Hash(0xB44, static_cast<uint64_t>(B))), 0.0);
						W.Box(D * std::cos(A), D * std::sin(A), 0.02, 0.5 * Long, 0.05, 0.034, A * 1.7, C);
					}
					M.FarFrom = 22.0f;
					break;
				}
				case CChunk:
				{
					Lump(W, 0.0, 0.0, 0.0, 0.22, 0.18, 0xC4C7, Varied(Pt.Rubble[1], 1.05, 0.0), Varied(Pt.Rubble[1], 0.65, 0.0));
					Lump(W, 0.3, 0.1, 0.0, 0.12, 0.1, 0xC4C8, Varied(Pt.Rubble[0], 1.05, 0.0), Varied(Pt.Rubble[0], 0.65, 0.0));
					M.FarFrom = 40.0f;
					break;
				}
				case CTiles:
				{
					// shards of slate or tile, lying flat-ish
					for (int B = 0; B < 6; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x711E, static_cast<uint64_t>(B))), D = 0.3 * Unit(Hash(0x711F, static_cast<uint64_t>(B)));
						const double Sz = 0.04 + 0.06 * Unit(Hash(0x7120, static_cast<uint64_t>(B)));
						W.Box(D * std::cos(A), D * std::sin(A), 0.008, Sz * 1.4, Sz, 0.008, A, Varied(Pt.Roof, 0.9 + 0.2 * Unit(Hash(0x7121, static_cast<uint64_t>(B))), 0.0));
					}
					M.FarFrom = 16.0f;
					break;
				}
				case CTimber:
				{
					Beam(W, V3d{ -0.6, 0.0, 0.04 }, V3d{ 0.6, 0.1, 0.12 }, 0.09, Pt.Timber);
					Beam(W, V3d{ -0.2, -0.3, 0.03 }, V3d{ 0.3, 0.25, 0.05 }, 0.03, Pt.Timber);
					M.FarFrom = 30.0f;
					break;
				}
				case CGrass:
				{
					// a worn tuft: some blades, straw at their tips
					const Tone Foot = Varied(Grass, 0.75, 0.0), Tip = Mixed(Grass, Rgb(170, 160, 120), 0.35);
					for (int B = 0; B < 9; ++B)
					{
						const double A = 2.0 * Pi * B / 9.0 + 0.4 * Unit(Hash(0x6A5, static_cast<uint64_t>(B)));
						const double Hgt = 0.18 + 0.18 * Unit(Hash(0x6A6, static_cast<uint64_t>(B)));
						W.Blade(0.03 * std::cos(A), 0.03 * std::sin(A), 0.0, 0.12 * std::cos(A), 0.12 * std::sin(A), Hgt, 0.035, Foot, Tip);
					}
					M.Paint = 2;
					M.FarFrom = 30.0f;
					break;
				}
				case CWeed:
				default:
				{
					// a dry weed of the waste ground, its stalks and seed heads
					const Tone Stalk = Rgb(112, 104, 76), Head = Rgb(150, 136, 104);
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * B / 5.0 + 0.5 * Unit(Hash(0x3EE, static_cast<uint64_t>(B)));
						const double Hgt = 0.45 + 0.35 * Unit(Hash(0x3EF, static_cast<uint64_t>(B)));
						W.Blade(0.0, 0.0, 0.0, 0.15 * std::cos(A), 0.15 * std::sin(A), Hgt, 0.03, Stalk, Stalk);
						W.Bead(0.15 * std::cos(A), 0.15 * std::sin(A), Hgt, 0.03, 0.06, Head, Stalk);
					}
					M.Paint = 2;
					M.FarFrom = 34.0f;
					break;
				}
				}
				M.Far = LandField::Mesh();
			}

			// ---------------------------------------------------------------- the kit (built here, no file)

			const char* Folder() const override { return ""; }
			const char* KitName(int K) const override
			{
				static const char* const RowNames[16] = { "CityRow2Holed", "CityRow2Gutted", "CityRow2Half", "CityRow2Stump", "CityRow3Holed", "CityRow3Gutted",
				                                          "CityRow3Half", "CityRow3Stump", "CityRow4Holed", "CityRow4Gutted", "CityRow4Half", "CityRow4Stump",
				                                          "CityRow5Holed", "CityRow5Gutted", "CityRow5Half", "CityRow5Stump" };
				if (K >= KRow0 && K < KRow0 + 16) return RowNames[K - KRow0];
				switch (K)
				{
				case KVilla0: return "CityVillaHoled";
				case KVilla0 + 1: return "CityVillaBurnt";
				case KVilla0 + 2: return "CityVillaHalf";
				case KVilla0 + 3: return "CityVillaWhole";
				case KShed: return "CityShed";
				case KChimney: return "CityChimney";
				case KTower: return "CityWaterTower";
				case KWarehouse: return "CityWarehouse";
				case KSpire: return "CitySpire";
				case KDome: return "CityDome";
				case KStation: return "CityStation";
				case KHeap: return "CityHeap";
				case KScatter: return "CityScatter";
				case KLoneWall: return "CityLoneWall";
				case KShellTree: return "CityShellTree";
				case KStump: return "CityTreeStump";
				case KLamp: return "CityLamp";
				case KLampBent: return "CityLampBent";
				case KWreck: return "CityCarWreck";
				default: return nullptr;
				}
			}
			bool KitBuilt(int K) const override { return K >= 0 && K < KSlots; }
			bool KitLow(int K) const override { return K == KScatter; }

			/** A slot's three levels, built: near, the middle and plain. */
			void Make(int K, int Lod, LandField::Mesh& Out) const
			{
				Writer W(Out);
				const uint64_t Key = Hash(Seed ^ 0xC17EC17Eull, static_cast<uint64_t>(K));
				if (K >= KRow0 && K < KRow0 + 16)
				{
					const int Storeys = 2 + (K - KRow0) / 4, St = (K - KRow0) % 4;
					const bool bShops = Storeys >= 4 && Unit(Hash(Key, 0x5409)) < 0.6;
					if (Lod == 2) RowFar(W, Pt, Storeys, St, Key, FrontOf(Storeys), DeepOf(Storeys));
					else RowHouse(W, Pt, Storeys, St, Key, Lod, bShops, FrontOf(Storeys), DeepOf(Storeys));
					return;
				}
				if (K >= KVilla0 && K < KVilla0 + 4)
				{
					Villa(W, Pt, K - KVilla0, Key, Lod, 10.4, 8.8);
					return;
				}
				switch (K)
				{
				case KShed: Shed(W, Pt, Key, Lod, 30.0, 18.0); break;
				case KChimney: Chimney(W, Pt, Key, Lod); break;
				case KTower: Tower(W, Pt, Key, Lod); break;
				case KWarehouse:
					if (Lod == 2) RowFar(W, Pt, 5, SGutted, Key, 28.0, 14.0);
					else RowHouse(W, Pt, 5, SGutted, Key, Lod, false, 28.0, 14.0);
					break;
				case KSpire: Church(W, Pt, Key, Lod); break;
				case KDome: DomeHall(W, Pt, Key, Lod); break;
				case KStation: Station(W, Pt, Key, Lod); break;
				case KHeap: Heap(W, 0.0, 0.0, 4.5, 3.4, 1.9, Key, Pt, Lod, 40, 5); break;
				case KScatter: Heap(W, 0.0, 0.0, 2.6, 2.0, 0.22, Key, Pt, Lod == 0 ? 1 : 2, 0, 0); break;
				case KLoneWall: LoneWall(W, Pt, Key, Lod); break;
				case KShellTree: ShellTree(W, Key, Lod, false); break;
				case KStump: ShellTree(W, Key, Lod, true); break;
				case KLamp: Lamp(W, false); break;
				case KLampBent: Lamp(W, true); break;
				case KWreck: Wreck(W, Key, Lod); break;
				default: W.Box(0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, Rgb(150, 146, 136)); break;
				}
			}

			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Mid = LandField::Mesh();
				M.Far = LandField::Mesh();
				Make(K, 0, M.Near);
				const bool bBuilding = (K >= KRow0 && K < KVilla0 + 4) || (K >= KShed && K <= KStation) || K == KLoneWall;
				if (bBuilding)
				{
					Make(K, 1, M.Mid);
					Make(K, 2, M.Far);
					M.MidFrom = 90.0f;
					M.FarFrom = 300.0f;
					M.Class = LandField::BaseMesh::Draw::House;
				}
				else
				{
					// (the heaps, trees, lamps and wrecks: near, then plain)
					Make(K, 2, M.Far);
					M.FarFrom = K == KHeap ? 160.0f : K == KScatter ? 70.0f : 110.0f;
					M.Class = K == KHeap ? LandField::BaseMesh::Draw::Big : K == KShellTree ? LandField::BaseMesh::Draw::Big : LandField::BaseMesh::Draw::Shrub;
				}
			}

			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Stands = [&M](float Mid, float Half, float Across, float Trunk) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
					M.SightTrunk = Trunk;
				};
				if ((K >= KRow0 && K < KVilla0 + 4) || (K >= KShed && K <= KStation) || K == KLoneWall) Stands(0.5f, 0.5f, 0.7f, 0.0f);
				else if (K == KHeap) Stands(0.4f, 0.4f, 0.6f, 0.0f);
				else if (K == KShellTree || K == KStump) Stands(0.5f, 0.5f, 0.1f, 0.3f);
				else if (K == KWreck) Stands(0.5f, 0.5f, 0.55f, 0.0f);
			}

			// ---------------------------------------------------------------- what stands on it

			/** A state for a building, by how ruined the city is there: no two side by side alike. */
			int StateOf(double Ru, double U) const
			{
				// (at its worst: a quarter holed, a third shells, a fifth half down, a fifth stumps)
				const double Holed = 0.62 - 0.4 * Ru, Gutted = Holed + 0.18 + 0.15 * Ru, Half = Gutted + 0.1 + 0.1 * Ru;
				return U < Holed ? SHoled : U < Gutted ? SGutted : U < Half ? SHalf : SStump;
			}

			void Sites(Host& H) override
			{
				// No city cells at all (a recording that names the look on a field of its own): the rings round its town.
				{
					int Cells = 0;
					for (size_t K = 0; K < Fd.Ground.size(); ++K)
					{
						const int G = Fd.Ground[K];
						if (G < 0 || static_cast<size_t>(G) >= Fd.Grounds.size()) continue;
						const std::string& Gn = Fd.Grounds[static_cast<size_t>(G)];
						if (Gn == "buildings" || Gn == "suburb" || Gn == "industry") ++Cells;
					}
					bRings = Cells * 12 < static_cast<int>(std::max<size_t>(1, Fd.Ground.size())) && Fd.bTown;
				}
				Band0 = H.Band() - 20.0;
				Rng R(H.Seed() ^ 0xC17A);
				int Built = 0, Damaged = 0;
				// The landmark first, by the heart: the first place near the town's middle it fits.
				{
					const double Long = Mark == KStation ? 61.0 : Mark == KSpire ? 39.0 : 25.0, Deep = Mark == KStation ? 31.0 : Mark == KSpire ? 14.0 : 25.0;
					bool bDone = false;
					for (int Ring = 0; Ring < 30 && !bDone; ++Ring)
						for (int K = 0; K < 16 && !bDone; ++K)
						{
							const double A = K * 2.0 * Pi / 16.0 + Ring * 0.41, D = 12.0 + Ring * 9.0;
							const double X = CX + D * std::cos(A), Y = CY + D * std::sin(A);
							const double Yaw = Main.Axis + (Ring % 2 ? 0.5 * Pi : 0.0);
							// (the station's centre is its hall's: its tower sticks out in front, so its long side wants more)
							const double Off = Mark == KStation ? -3.5 : Mark == KSpire ? -0.0 : 0.0;
							const double PX = X + Off * std::cos(Yaw), PY = Y + Off * std::sin(Yaw);
							if (H.House(KitOf(Mark), PX, PY, Yaw, Long, Deep))
							{
								++Built;
								++Damaged;
								bDone = true;
								MarkX = PX;
								MarkY = PY;
							}
						}
				}
				// The industry's chimneys and its water tower, before its sheds take the ground: from its own places, two
				// stacks and a tower where there is industry at all (the works seen from across the city).
				{
					std::vector<std::pair<double, double>> Works;
					for (double Y = -H.Band() + 10.0; Y < Fd.Depth() + H.Band(); Y += 20.0)
						for (double X = -H.Band() + 10.0; X < Fd.Width() + H.Band(); X += 20.0)
							if (Quarter(X, Y) == 3) Works.push_back({ X, Y });
					int Stacks = 0, Towers = 0;
					for (int K = 0; K < 120 && !Works.empty() && (Stacks < 2 || Towers < 1); ++K)
					{
						const std::pair<double, double> At = Works[static_cast<size_t>(R.Below(static_cast<int>(Works.size())))];
						const double DX = R.Range(-8.0, 8.0);
						const double DY = R.Range(-8.0, 8.0);
						const double Yaw = R.Range(0.0, 2.0 * Pi);
						const bool bTower = Stacks >= 2 || (Towers < 1 && K % 3 == 2);
						if (H.House(KitOf(bTower ? KTower : KChimney), At.first + DX, At.second + DY, Yaw, bTower ? 11.0 : 5.0, bTower ? 11.0 : 5.0))
						{
							++Built;
							(bTower ? Towers : Stacks) += 1;
						}
					}
				}
				// The blocks: each grid's blocks, walked round their four sides, a building fronting each side's street, wall
				// to wall along it; a suburb's houses each in its garden; the industry's sheds, a chimney, a water tower.
				const double Reach = std::max(Fd.Width(), Fd.Depth()) + 2.0 * H.Band();
				const Grid* Grids[3] = { &Old, &Main, &Suburb };
				int LastSlot = -1;
				for (int Gi = 0; Gi < 3; ++Gi)
				{
					const Grid& G = *Grids[Gi];
					const int NU = static_cast<int>(Reach / G.PU) + 2, NV = static_cast<int>(Reach / G.PV) + 2;
					for (int J = -NV; J <= NV; ++J)
						for (int I = -NU; I <= NU; ++I)
						{
							// the block between the streets' middles at I*PU and (I+1)*PU: its edge the street's side
							const double U0 = I * G.PU + 0.5 * G.Street + 1.6, U1 = (I + 1) * G.PU - 0.5 * G.Street - 1.6;
							const double V0 = J * G.PV + 0.5 * G.Street + 1.6, V1 = (J + 1) * G.PV - 0.5 * G.Street - 1.6;
							double MX, MY;
							G.ToWorld(0.5 * (U0 + U1), 0.5 * (V0 + V1), MX, MY);
							if (MX < -H.Band() || MY < -H.Band() || MX > Fd.Width() + H.Band() || MY > Fd.Depth() + H.Band()) continue;
							// four sides: (from, to) along the block's edge, its street outward
							const double Cs[4][4] = { { U0, V0, U1, V0 }, { U1, V0, U1, V1 }, { U1, V1, U0, V1 }, { U0, V1, U0, V0 } };
							for (int Sd = 0; Sd < 4; ++Sd)
							{
								double AX, AY, BX, BY;
								G.ToWorld(Cs[Sd][0], Cs[Sd][1], AX, AY);
								G.ToWorld(Cs[Sd][2], Cs[Sd][3], BX, BY);
								const double Len = std::hypot(BX - AX, BY - AY);
								if (Len < 4.0) continue;
								const double TX = (BX - AX) / Len, TY = (BY - AY) / Len;
								// (the street outward: the block's middle on the other side)
								double NX = TY, NY = -TX;
								if ((MX - AX) * NX + (MY - AY) * NY > 0.0)
								{
									NX = -NX;
									NY = -NY;
								}
								double S = 0.0;
								while (S < Len - 3.0)
								{
									const double PX = AX + TX * (S + 2.0), PY = AY + TY * (S + 2.0);
									const int Q = Quarter(PX - NX * 4.0, PY - NY * 4.0);
									const Grid& Want = GridAt(PX - NX * 4.0, PY - NY * 4.0);
									if (Q == 0 || Q == 4 || &Want != &G)
									{
										S += 4.0;
										continue;
									}
									const double Ru = Ruin(PX, PY);
									const double U = R.Next(), U2 = R.Next(), U3 = R.Next();
									if (Q == 1)
									{
										// a suburb's house in its garden: set back, every 20 to 30 m
										const double Long = 10.4, Deep = 8.8, Back = 4.0 + 3.0 * U2;
										if (S + Long > Len - 2.0) break;
										const double CXh = AX + TX * (S + 0.5 * Long) - NX * (Back + 0.5 * Deep), CYh = AY + TY * (S + 0.5 * Long) - NY * (Back + 0.5 * Deep);
										const int St = U < 0.32 - 0.12 * Ru ? 3 : U < 0.62 ? 0 : U < 0.85 ? 1 : 2;
										const double Yaw = std::atan2(TY, TX) + (U3 - 0.5) * 0.08;
										if (H.House(KitOf(KVilla0 + St), CXh, CYh, Yaw, Long, Deep))
										{
											++Built;
											if (St != 3) ++Damaged;
											S += Long + 9.0 + 12.0 * U3;
										}
										else S += 3.0;
										continue;
									}
									if (Q == 3)
									{
										// industry: a shed set back from its yard's gate, now and then a warehouse
										const bool bWare = U < 0.3;
										const double Long = bWare ? 28.0 : 30.0, Deep = bWare ? 14.0 : 18.0;
										if (S + Long > Len - 1.0)
										{
											S += 4.0;
											continue;
										}
										const double CXh = AX + TX * (S + 0.5 * Long) - NX * (1.5 + 0.5 * Deep), CYh = AY + TY * (S + 0.5 * Long) - NY * (1.5 + 0.5 * Deep);
										if (H.House(KitOf(bWare ? KWarehouse : KShed), CXh, CYh, std::atan2(TY, TX), Long, Deep))
										{
											++Built;
											++Damaged;
											S += Long + 6.0 + 10.0 * U2;
										}
										else S += 4.0;
										continue;
									}
									// a terrace's or the core's building, wall to wall with the last; one in eight a gap (a heap where it
									// stood), never the same slot twice running
									const int Storeys = StoreysAt(PX - NX * 4.0, PY - NY * 4.0);
									const double Long = FrontOf(Storeys), Deep = DeepOf(Storeys);
									if (S + Long > Len + 0.5) break;
									if (U2 < 0.07 + 0.08 * Ru)
									{
										// (the building gone: its heap in the street before where it stood)
										const double HX = AX + TX * (S + 0.5 * Long) + NX * 2.0, HY = AY + TY * (S + 0.5 * Long) + NY * 2.0;
										const double HeapYaw = std::atan2(TY, TX), HeapSize = 0.9 + 0.5 * U3;
										if (H.Free(HX, HY, 3.0)) H.Plant(KitOf(KHeap), HX, HY, HeapYaw, HeapSize, HeapSize, Tint(1.0, 1.0, 1.0));
										S += Long;
										continue;
									}
									int St = StateOf(Ru, U);
									int SlotK = KRow0 + (Storeys - 2) * 4 + St;
									if (SlotK == LastSlot)
									{
										St = (St + 1 + static_cast<int>(U3 * 3.0)) % 4;
										SlotK = KRow0 + (Storeys - 2) * 4 + St;
									}
									const double CXh = AX + TX * (S + 0.5 * Long) - NX * 0.5 * Deep, CYh = AY + TY * (S + 0.5 * Long) - NY * 0.5 * Deep;
									// (its front, -y as built, to the street: the yaw that turns -y onto the outward normal)
									const double Yaw = std::atan2(TY, TX);
									if (H.Row(KitOf(SlotK), CXh, CYh, Yaw, Long, Deep))
									{
										++Built;
										++Damaged;
										LastSlot = SlotK;
										// (a fallen half's or a stump's rubble spills into the street before it)
										if ((St == SHalf || St == SStump) && U3 < 0.7)
										{
											const double HX = AX + TX * (S + 0.5 * Long) + NX * 2.6, HY = AY + TY * (S + 0.5 * Long) + NY * 2.6;
											const double Sz = 0.6 + 0.4 * U3;
											if (H.Free(HX, HY, 2.0)) H.Plant(KitOf(St == SStump ? KHeap : KScatter), HX, HY, Yaw, Sz, Sz, Tint(1.0, 1.0, 1.0));
										}
										S += Long;
									}
									else S += 2.0;
								}
							}
						}
				}
				// Facades standing alone in the core, their buildings gone behind them: a few.
				for (int K = 0; K < 300; ++K)
				{
					const double X = R.Range(0.0, Fd.Width()), Y = R.Range(0.0, Fd.Depth()), Yaw = R.Range(0.0, 2.0 * Pi);
					if (Quarter(X, Y) != 2 || Ruin(X, Y) < 0.7) continue;
					if (H.House(KitOf(KLoneWall), X, Y, Yaw, 12.5, 6.0)) ++Built;
				}
				H.Tally(Built, Damaged, 0);
			}

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9C17);
				const double B = H.Band(), W = Fd.Width() + 2.0 * B, D = Fd.Depth() + 2.0 * B;
				const bool bEast = Sty == East;
				// Woods about the city: the frame's own broadleaf trees (birch in the east), and their undergrowth.
				const int Tries = static_cast<int>(W * D / 10000.0 * 120.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Kind = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.75, 1.2);
					const Spot S = H.At(X, Y);
					if (S.Kind != KWoods || !H.Free(X, Y, 2.5)) continue;
					const LandField::Base T = bEast ? (Kind < 0.45 ? LandField::Base::Birch : Kind < 0.8 ? LandField::Base::Oak : LandField::Base::Ash)
					                                : (Kind < 0.4 ? LandField::Base::Beech : Kind < 0.75 ? LandField::Base::Oak : LandField::Base::Ash);
					H.Plant(T, X, Y, Yaw, Size * 0.45, Size, Tint(0.96, 1.0, 0.94));
				}
				// The suburbs' gardens: a fruit tree or a shrub behind each house, some torn.
				const int Garden = static_cast<int>(W * D / 10000.0 * 30.0);
				for (int K = 0; K < Garden; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Kind = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.1);
					if (Quarter(X, Y) != 1 || GridAt(X, Y).StreetAt(X, Y) > -2.0 || !H.Free(X, Y, 2.0)) continue;
					if (Kind < 0.35) H.Plant(LandField::Base::Apple, X, Y, Yaw, Size * 0.5, Size, Tint(1.0, 1.0, 1.0));
					else if (Kind < 0.75) H.Plant(Kind < 0.55 ? LandField::Base::ShrubB : LandField::Base::ShrubA, X, Y, Yaw, Size * 0.6, Size * 0.8, Tint(0.95, 1.0, 0.95));
					else H.Plant(KitOf(Kind < 0.9 ? KShellTree : KStump), X, Y, Yaw, Size, Size, Tint(1.0, 1.0, 1.0));
				}
				// The parks and squares: their trees torn by the shells, stumps, a few whole.
				const int Park = static_cast<int>(W * D / 10000.0 * 60.0);
				for (int K = 0; K < Park; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Kind = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.8, 1.2);
					if (Quarter(X, Y) != 4 || !H.Free(X, Y, 2.0)) continue;
					if (Kind < 0.5) H.Plant(KitOf(KShellTree), X, Y, Yaw, Size, Size, Tint(1.0, 1.0, 1.0));
					else if (Kind < 0.8) H.Plant(KitOf(KStump), X, Y, Yaw, Size, Size, Tint(1.0, 1.0, 1.0));
					else H.Plant(bEast ? LandField::Base::Birch : LandField::Base::Elm, X, Y, Yaw, Size * 0.4, Size * 0.85, Tint(0.92, 0.95, 0.9));
				}
				// Along the avenues: their trees torn, lamps (some bent), a burnt-out car now and then; the rubble lying low.
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Ln = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					double Since = 0.0;
					for (size_t K = 1; K < Ln.size(); ++K)
					{
						const double Dx = Ln[K].first - Ln[K - 1].first, Dy = Ln[K].second - Ln[K - 1].second, Len = std::hypot(Dx, Dy);
						Since += Len;
						if (Since < 14.0 || Len < 1e-6) continue;
						Since = 0.0;
						const double Nx = -Dy / Len, Ny = Dx / Len;
						for (int Side = -1; Side <= 1; Side += 2)
						{
							const double Off = Half + 1.6;
							const double X = Ln[K].first + Nx * Off * Side, Y = Ln[K].second + Ny * Off * Side;
							const int Q = Quarter(X, Y);
							if (Q == 0) continue;
							const double U = R.Next(), Yaw = std::atan2(Dy, Dx) + (Side > 0 ? 0.5 * Pi : -0.5 * Pi), Size = R.Range(0.85, 1.15);
							if (!H.Free(X, Y, 0.8)) continue;
							if (U < 0.3) H.Plant(KitOf(KLamp), X, Y, Yaw, 1.0, Size, Tint(1.0, 1.0, 1.0));
							else if (U < 0.42) H.Plant(KitOf(KLampBent), X, Y, Yaw + R.Range(-0.6, 0.6), 1.0, 1.0, Tint(1.0, 1.0, 1.0));
							else if (U < 0.62 && Q != 3) H.Plant(KitOf(U < 0.52 ? KShellTree : KStump), X, Y, Yaw, Size, Size, Tint(1.0, 1.0, 1.0));
						}
						const double U2 = R.Next(), Side2 = R.Next() < 0.5 ? -1.0 : 1.0;
						if (U2 < 0.08)
						{
							const double X = Ln[K].first + Nx * (Half - 1.4) * Side2, Y = Ln[K].second + Ny * (Half - 1.4) * Side2;
							const double Yaw = std::atan2(Dy, Dx) + R.Range(-0.4, 0.4);
							if (Quarter(X, Y) > 0) H.Plant(KitOf(KWreck), X, Y, Yaw, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						}
					}
				}
				// Rubble lying low across the core's own streets and courtyards (a man steps over it).
				const int Scatter = static_cast<int>(W * D / 10000.0 * 14.0);
				for (int K = 0; K < Scatter; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.3), Keep = R.Next();
					const int Q = Quarter(X, Y);
					if ((Q != 2 && Q != 3) || Keep > Ruin(X, Y) || !H.Free(X, Y, 1.8, true)) continue;
					H.Plant(KitOf(KScatter), X, Y, Yaw, Size, 1.0, Tint(1.0, 1.0, 1.0));
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				// (a shell in the city throws masonry: its town word on built ground and its streets)
				if (Ground == "buildings" || Ground == "suburb" || Ground == "industry" || Ground == "road" || Ground == "park") return 1;
				return Else;
			}
			const char* Earth() const override { return "loam"; }
			int DrawStep() const override { return 1; }

		private:
			Style Sty = North;
			Paints Pt;
			Tone Grass;
			double CX = 0.0, CY = 0.0;
			Grid Main, Old, Suburb;
			int Mark = KSpire;
			double MarkX = 0.0, MarkY = 0.0;
			bool bRings = false;
			double Band0 = 230.0;
		};
	}

	std::unique_ptr<Region> MakeCity(const LandField::Field& F, uint64_t S) { return std::make_unique<City>(F, S); }

	bool IsCityField(const LandField::Field& F)
	{
		// (a capital's or a city's streets as the rules lay them: its suburbs, industry or parks, or a fifth of the field built)
		size_t Built = 0, Own = 0;
		for (const int G : F.Ground)
		{
			if (G < 0 || static_cast<size_t>(G) >= F.Grounds.size()) continue;
			const std::string& N = F.Grounds[static_cast<size_t>(G)];
			if (N == "suburb" || N == "industry" || N == "park") ++Own;
			if (N == "buildings" || N == "suburb" || N == "industry") ++Built;
		}
		if (Own > 0) return true;
		return !F.Ground.empty() && (F.Terrain == "urban" ? Built * 8 >= F.Ground.size() : Built * 5 >= F.Ground.size());
	}
}
