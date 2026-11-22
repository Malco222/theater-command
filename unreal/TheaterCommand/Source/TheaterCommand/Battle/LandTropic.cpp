#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * The jungle and the Pacific islands (one region, two looks: Which() by bIsland).
 *
 * The jungle (Guadalcanal, Kokoda and Buna, Burma; 1942-45): a wall of green with no horizon. The rules' woods are lowland
 * rainforest: a closed canopy from fewer, bigger trees (the kit's giants and its canopy trees scaled up to 22-36 m), slim
 * sub-canopy trees under them, bananas and young palms, and round the eye the floor's own growth (ferns and gingers,
 * saplings and rattan, leaf litter) as clutter; the edges a solid wall of undergrowth and small trees. Pale trunks, very
 * different thicknesses, lianas hanging in loops. The rules' open land is kunai grass 1-2 m over red laterite, bright
 * against the forest, with burnt patches; its fields are coconut plantations in ruler-straight rows of tall leaning
 * palms over short grass (rice paddies on the Asian mainland); its rough the open grassy ridges. Tracks of red or
 * yellow-brown mud, puddled in their ruts. Villages of houses on stilts among bananas and palms. Beyond the field the
 * forest closes in on every side and climbs into hills under the humid haze.
 *
 * The islands (Tarawa, Saipan, Peleliu; 1943-45): dazzling coral sand under palm groves, flat, the shore laid beyond the
 * field on every side (a beach falling 1:15 to the water, its wet band and tide-wrack line, the turquoise reef flat out
 * to its white surf line, the deep blue beyond); a third of the palms snapped to poles by the bombardment, their fronds
 * hanging brown; a seawall of coconut logs along the north beach; huts of thatch and tin.
 *
 * Kit: battle-models/asia (Content/Models/Asia). Colours sRGB as the eye sees them (look-targets section 7), at their
 * true colours: the jungle and the islands have their own grade.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Kunai = 0,      // the jungle's open land: kunai grass 1-2 m, red earth through it
			Ridge = 1,      // its rough: an open grassy ridge, the grass short, laterite and a few boulders
			Plantation = 2, // coconut palms in ruler-straight rows over short grass (its fields; the islands' too)
			Paddy = 3,      // rice behind low bunds (the Asian mainland's fields)
			Garden = 4,     // a native garden gone back: bananas, taro, young palms, grass
			Grove = 5,      // an island's palm grove over sand and creeping vines
			Strand = 6      // an island's open coral sand and beach scrub
		};

		enum Slot : int
		{
			KTree = 0,      // JungleTree: sub-canopy and (scaled up) canopy
			KGiant = 1,     // JungleGiant: emergents and the canopy's big crowns
			KPalm = 2,      // CoconutPalm
			KPalmTall = 3,  // CoconutPalmTall: the plantations' tall leaning palms
			KPalmYoung = 4, // CoconutPalmYoung
			KBanana = 5,
			KGrass = 6,     // ElephantGrass (kunai clumps, scaled down)
			KUnder = 7,     // Undergrowth
			KStilt = 8,     // StiltHouse
			KStiltTin = 9,  // StiltHouseTin
			KThatch = 10,   // ThatchHouse
			KReeds = 11,
			KBoulder = 12,
			KBamboo = 13,   // Bamboo (the Asian mainland only: Burma, Malaya)
			KStump = 14,    // CoconutPalmTall again, its crown shot away (KitLook cuts it): the islands' snapped palms
			KThicket = 15   // the jungle's thickets, built whole in KitLook (the kit has none; Boulder's small file carries the slot)
		};

		struct V3
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
		};
		V3 operator+(const V3& A, const V3& B) { return V3{ A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 operator-(const V3& A, const V3& B) { return V3{ A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 operator*(const V3& A, double K) { return V3{ A.X * K, A.Y * K, A.Z * K }; }
		V3 Cross(const V3& A, const V3& B) { return V3{ A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Norm(const V3& A)
		{
			const double L = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
			return L > 1e-12 ? A * (1.0 / L) : V3{ 0.0, 0.0, 1.0 };
		}

		/** A ribbon along a line of points (a blade, a frond, a stem, a liana), both faces, Wide across at each point (its
		 * last may be 0, a point), facing square to Across; coloured along it from Foot to Tip. Lit as leaves are, from
		 * above on both faces. */
		void Ribbon(Writer& W, const std::vector<V3>& P, const std::vector<double>& Wide, const V3& Across, const Tone& Foot, const Tone& Tip)
		{
			if (P.size() < 2) return;
			const V3 A = Norm(Across);
			std::vector<uint32_t> L, R;
			for (size_t K = 0; K < P.size(); ++K)
			{
				const V3 Up = Norm(P[std::min(K + 1, P.size() - 1)] - P[K > 0 ? K - 1 : 0]);
				V3 N = Norm(Cross(A, Up));
				N.Z = std::fabs(N.Z) + 0.6;
				N = Norm(N);
				const Tone C = Mixed(Foot, Tip, static_cast<double>(K) / static_cast<double>(P.size() - 1));
				const double H = 0.5 * Wide[K];
				L.push_back(W.Vert(P[K].X - A.X * H, P[K].Y - A.Y * H, P[K].Z - A.Z * H, N.X, N.Y, N.Z, C));
				R.push_back(W.Vert(P[K].X + A.X * H, P[K].Y + A.Y * H, P[K].Z + A.Z * H, N.X, N.Y, N.Z, C));
			}
			for (size_t K = 0; K + 1 < P.size(); ++K)
			{
				W.Tri(L[K], R[K], R[K + 1]);
				W.Tri(R[K], L[K], R[K + 1]);
				if (Wide[K + 1] > 1e-6)
				{
					W.Tri(L[K], R[K + 1], L[K + 1]);
					W.Tri(R[K + 1], L[K], L[K + 1]);
				}
			}
		}

		/** A mesh with only the triangles Keep says yes to (and only their points). */
		template <typename F>
		LandField::Mesh Kept(const LandField::Mesh& M, F Keep)
		{
			LandField::Mesh Out;
			std::vector<int64_t> Map(M.Colours.size(), -1);
			for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
			{
				const uint32_t I[3] = { M.Triangles[T], M.Triangles[T + 1], M.Triangles[T + 2] };
				if (!Keep(I[0], I[1], I[2])) continue;
				for (const uint32_t V : I)
				{
					if (Map[V] < 0)
					{
						Map[V] = static_cast<int64_t>(Out.Colours.size());
						Out.Positions.insert(Out.Positions.end(), { M.Positions[3 * V], M.Positions[3 * V + 1], M.Positions[3 * V + 2] });
						Out.Normals.insert(Out.Normals.end(), { M.Normals[3 * V], M.Normals[3 * V + 1], M.Normals[3 * V + 2] });
						Out.Colours.push_back(M.Colours[V]);
					}
					Out.Triangles.push_back(static_cast<uint32_t>(Map[V]));
				}
			}
			return Out;
		}

		/** A closed lumpy ball about C (RX across, RZ high; Rings from top to foot, Round about), lit Lit on top and Under
		 * beneath: the core of a mass of leaves, or one of a far thicket's few. */
		void Blob(Writer& W, const V3& C, double RX, double RZ, int Rings, int Round, uint64_t Key, const Tone& Lit, const Tone& Under)
		{
			const uint32_t Top = W.Vert(C.X, C.Y, C.Z + RZ, 0, 0, 1, Lit);
			std::vector<uint32_t> Id;
			for (int R = 1; R < Rings; ++R)
			{
				const double Phi = Pi * R / Rings;
				for (int K = 0; K < Round; ++K)
				{
					const double Th = 2.0 * Pi * (K + 0.5 * (R % 2)) / Round;
					const double Push = 0.66 + 0.68 * Unit(Hash(Key, static_cast<uint64_t>(R), static_cast<uint64_t>(K)));
					const double Mottle = 0.78 + 0.44 * Unit(Hash(Key ^ 0x707, static_cast<uint64_t>(R), static_cast<uint64_t>(K)));
					const double X = std::sin(Phi) * std::cos(Th), Y = std::sin(Phi) * std::sin(Th), Z = std::cos(Phi);
					// (mottled, the lumps that stand out lit the more)
					const Tone Cv = Varied(Mixed(Under, Lit, Clamp01(0.5 + 0.5 * Z + 0.4 * (Push - 1.0))), Mottle, 0.0);
					Id.push_back(W.Vert(C.X + RX * Push * X, C.Y + RX * Push * Y, C.Z + RZ * Push * Z, X, Y, Z, Cv));
				}
			}
			const uint32_t Foot = W.Vert(C.X, C.Y, C.Z - 0.85 * RZ, 0, 0, -1, Under);
			const auto At = [&Id, Round](int R, int K) { return Id[static_cast<size_t>(R * Round + (K % Round))]; };
			for (int K = 0; K < Round; ++K) W.Tri(Top, At(0, K), At(0, K + 1));
			for (int R = 0; R + 2 < Rings; ++R)
				for (int K = 0; K < Round; ++K)
				{
					W.Tri(At(R, K), At(R + 1, K), At(R + 1, K + 1));
					W.Tri(At(R, K), At(R + 1, K + 1), At(R, K + 1));
				}
			for (int K = 0; K < Round; ++K) W.Tri(Foot, At(Rings - 2, K + 1), At(Rings - 2, K));
		}

		/** A mass of leaves: a dark lumpy core half hidden under broad leaves lying over it like shingles, each drooping
		 * out and down from where it grows, its face to the light. */
		void Leafy(Writer& W, const V3& C, double RX, double RZ, int Leaves, uint64_t Key, const Tone& Lit, const Tone& Under)
		{
			Blob(W, C, 0.7 * RX, 0.7 * RZ, 5, 7, Key, Mixed(Under, Lit, 0.55), Under);
			Rng R(Key ^ 0x1EAF);
			for (int L = 0; L < Leaves; ++L)
			{
				const double Th = R.Range(0.0, 2.0 * Pi), Z = R.Range(-0.5, 0.97), Long = R.Range(0.45, 0.8), Wide = R.Range(0.2, 0.34), Pale = R.Next(), Spin = R.Range(-0.7, 0.7);
				const double S = std::sqrt(std::max(0.0, 1.0 - Z * Z));
				const V3 N{ S * std::cos(Th), S * std::sin(Th), Z };
				const V3 Root = C + V3{ N.X * RX * 0.7, N.Y * RX * 0.7, N.Z * RZ * 0.7 };
				// (down the surface, turned a little either way, and out from it)
				const V3 Down = S > 0.15 ? Norm(V3{ 0, 0, -1 } + N * Z) : Norm(V3{ std::cos(Th + Spin), std::sin(Th + Spin), 0.0 });
				const V3 Side = Cross(N, Down);
				const V3 Run = Norm(Down * std::cos(Spin) + Side * std::sin(Spin) + N * 0.55);
				const Tone Foot = Mixed(Under, Lit, 0.45 + 0.25 * Z), Tip = Mixed(Lit, Rgb(120, 148, 66), 0.55 * Pale);
				Ribbon(W, { Root, Root + Run * (0.45 * Long), Root + Run * Long + V3{ 0, 0, -0.08 * Long } }, { 0.03, Wide, 0.0 }, Cross(N, Run), Foot, Tip);
			}
		}

		bool Same(const LandField::Rgb8& C, int R, int G, int B) { return C.R == R && C.G == G && C.B == B; }
		/** Every point of one paint's colour to another (a kit tree's shade leaves, read as black balls against the haze). */
		void Repaint(LandField::Mesh& M, int R, int G, int B, const Tone& To)
		{
			for (LandField::Rgb8& C : M.Colours)
				if (Same(C, R, G, B)) C = Byte(To);
		}

		class Tropic final : public Region
		{
		public:
			Tropic(const LandField::Field& F, uint64_t S, bool bInIsland) : Region(F, S), bIsland(bInIsland)
			{
				// (the mainland of Asia: paddies in its fields and bamboo in its forest; Papua's clay yellow-brown, the
				// Solomons' and Burma's laterite red)
				bMainland = F.Lon < 106.0 && F.Lat > 4.0;
				bClay = F.Lon > 140.0 && F.Lon < 151.5 && F.Lat < -2.0 && F.Lat > -11.0;
				// The dry season (the kunai straw, the tracks hard): south of the line May to October, north of it
				// November to April (Burma's monsoon from late May).
				const int M = F.Month;
				const bool bSouthDry = M >= 5 && M <= 10;
				Dry = (F.Lat < 0.0) == bSouthDry ? 0.75 : 0.2;
				if (M == 4 || M == 10 || M == 11 || M == 5) Dry = 0.5 * (Dry + 0.45);
				double Lo = 1e9;
				for (const double H : F.Height) Lo = std::min(Lo, H);
				if (F.Height.empty()) Lo = 0.0;
				SeaZ = Lo - 1.2;
				Wf = F.Width();
				Df = F.Depth();
			}
			Look Which() const override { return bIsland ? Look::Island : Look::Jungle; }

			// ------------------------------------------------------------------ the land

			double Outside(double X, double Y) const
			{
				const double DX = std::max({ 0.0, -X, X - Wf }), DY = std::max({ 0.0, -Y, Y - Df });
				return std::sqrt(DX * DX + DY * DY);
			}
			/** An island's shore: metres inland of the waterline (negative out on the reef), the beach some 100 to 180 m
			 * beyond the field's edge on every side. */
			double Shore(double X, double Y) const
			{
				const double D = Outside(X, Y);
				if (D <= 0.0) return 1000.0;
				return 140.0 + 38.0 * Noise(X / 260.0 + 3.7, Y / 260.0 - 1.3, Seed ^ 0x5E0A) - D;
			}
			/** The reef flat's width out from the waterline to its surf line. */
			double Reef(double X, double Y) const { return 430.0 + 180.0 * Noise(X / 900.0, Y / 900.0, Seed ^ 0x5E0B); }

			double Relief(double X, double Y, double Z) const override
			{
				if (!bIsland) return Z;
				const double D = Outside(X, Y);
				if (D <= 0.0) return Z;
				const double W = Shore(X, Y), Curve = D * D / 12.74e6;
				// (the land falls from the field's edge to the beach's crest; the beach 1:15 down to the water; the reef
				// flat and the sea beyond it level)
				const double Crest = SeaZ + 1.5;
				const double Land = Lerp(Z, Crest + 0.3 - Curve, Smooth(0.0, std::max(30.0, D + W - 30.0), D));
				const double Beach = SeaZ + 1.5 * Smooth(0.0, 24.0, W) - Curve;
				if (W >= 30.0) return Land;
				if (W <= 24.0) return Beach;
				return Lerp(Beach, Land, Smooth(24.0, 30.0, W));
			}
			double Rolling(double X, double Y, double MeanZ) const override
			{
				if (bIsland) return SeaZ + 1.8;
				// (the forest climbing into ridges and hills, higher the farther out: Mount Austen behind the Lunga, the
				// Owen Stanleys behind Buna; the haze takes them)
				const double D = Outside(X, Y);
				const double Hills = 22.0 + 55.0 * (0.5 + 0.5 * Fbm(X, Y, 2300.0, 3, Seed ^ 0xB3));
				return MeanZ + 7.0 * Fbm(X, Y, 650.0, 2, Seed ^ 0xB1) + Smooth(350.0, 4200.0, D) * Hills;
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return bIsland ? 0.05 : 0.16; }
			double LaneHalf(bool bVillage, double U) const override { return bIsland ? (bVillage ? 3.0 : 2.7) + 0.6 * U : (bVillage ? 2.6 : 2.1) + 0.7 * U; }

			// ------------------------------------------------------------------ the parcels

			double PlotMin() const override { return bIsland ? 26000.0 : 30000.0; }
			double PlotMax() const override { return bIsland ? 60000.0 : 75000.0; }
			double FarTilled() const override { return bIsland ? 0.6 : 0.3; }
			double WoodsEdge() const override { return bIsland ? 0.95 : -0.22; }

			int CropOf(int Kind, double U) const override
			{
				if (bIsland)
				{
					if (Kind == KField) return Plantation;
					if (Kind == KRough) return Strand;
					return U < 0.82 ? Grove : Strand;
				}
				if (Kind == KField) return bMainland ? Paddy : Plantation;
				if (Kind == KRough) return Ridge;
				return U < 0.8 ? Kunai : U < 0.9 ? Ridge : Garden;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Kunai: return "kunai";
				case Ridge: return "grassy ridge";
				case Plantation: return "coconut plantation";
				case Paddy: return "paddy";
				case Garden: return "garden";
				case Grove: return "palm grove";
				case Strand: return "strand";
				default: return "";
				}
			}

			// ------------------------------------------------------------------ colours

			/** Kunai burnt black in patches (some parcels): the same mask for the ground's paint and its clutter. */
			bool Burnt(uint64_t Key, double X, double Y) const
			{
				if (Unit(Hash(Key, 0xB0B)) > 0.16) return false;
				return Noise(X / 38.0 + 1.3, Y / 38.0 - 2.1, Seed ^ 0xB0C) > 0.48;
			}

			/** The kunai's big patches: low where it thins to short grass over red earth. */
			double Thin(double X, double Y) const { return Noise(X / 19.0 + 2.1, Y / 19.0 - 0.6, Seed ^ 0xC13) - 0.1; }
			/** How trodden the kunai is here, 0 to 1: the paths men and pigs have worn through it, a metre wide, wandering,
			 * running out. */
			double Trod(double X, double Y) const
			{
				const double Mask = Smooth(-0.2, 0.05, Noise(X / 170.0 + 4.1, Y / 170.0 - 2.7, Seed ^ 0x7A8));
				if (Mask <= 0.0) return 0.0;
				const double Off = 60.0 * std::fabs(Noise(X / 60.0 - 1.7, Y / 60.0 + 0.9, Seed ^ 0x7A7)); // (some metres off its line)
				return Mask * Smooth(1.4, 0.5, Off);
			}
			/** A track's broken edge (the grass coming in over it) and the grass on its hump between the ruts. */
			double Rag(double X, double Y) const { return Noise(X / 5.0 + 0.3, Y / 5.0 - 1.1, Seed ^ 0x7A9); }
			double Hump(double X, double Y) const { return Noise(X / 7.0 - 0.8, Y / 7.0 + 0.2, Seed ^ 0x7AA); }

			Tone Track(const Spot& S) const
			{
				// red laterite (yellow-brown clay in Papua), redder and yellower along it: the ruts wet and dark, water
				// standing in them and across the floor in its low places; grass on the hump between the ruts here and
				// there, creeping in over its broken edges, the verge
				const double Off = std::fabs(S.Lane + S.LaneHalf);
				const Tone Earth = bClay ? Mixed(Rgb(146, 112, 66), Rgb(162, 132, 86), Clamp01(0.5 + 0.7 * S.N1)) : Mixed(Rgb(155, 89, 54), Rgb(166, 112, 68), Clamp01(0.4 + 0.7 * S.N1));
				const Tone Wet = bClay ? Rgb(106, 84, 52) : Rgb(108, 62, 42);
				const Tone Water = Rgb(124, 118, 98), Grass = Rgb(112, 118, 62);
				const double Rut = Smooth(0.5, 0.12, std::fabs(Off - 0.9));
				Tone T = Mixed(Earth, Wet, 0.2 + 0.3 * (1.0 - Dry) + 0.4 * Rut);
				const double Pool = std::max(Rut * Smooth(0.0, 0.3, S.N2 + 0.25 * (1.0 - Dry)), Smooth(0.38, 0.52, S.N1 - 0.25 * Dry) * Smooth(0.4, 0.0, S.N2));
				T = Mixed(T, Water, 0.9 * Pool);
				const double OnHump = 0.75 * Smooth(0.5, 0.2, Off) * Smooth(0.0, 0.35, Hump(S.X, S.Y));
				const double Edge = Smooth(-0.9, 0.15, S.Lane + 0.8 * Rag(S.X, S.Y));
				T = Mixed(T, Varied(Grass, 1.0 + 0.1 * S.N2, 0.0), std::max(OnHump, Edge) * (1.0 - Pool));
				return Varied(T, 1.0 + 0.06 * S.N2, 0.0);
			}

			Tone JungleGround(const Spot& S, int C) const
			{
				switch (S.Kind)
				{
				case KLane: return Track(S);
				case KVillage:
				case KYard:
				{
					// a swept bare-earth plaza round the houses, gardens and grass between them
					const Tone Swept = bClay ? Rgb(170, 138, 92) : Rgb(170, 116, 78), Garden_ = Rgb(108, 122, 54);
					const double Bare = S.bNear ? 0.9 : Smooth(-0.2, 0.45, S.N1);
					return Varied(Mixed(Garden_, Swept, Bare), 1.0 + 0.06 * S.N2, 0.02);
				}
				case KWater: return Varied(Rgb(108, 100, 70), 1.0 + 0.04 * S.N1, 0.0); // (brown-green river water)
				case KMarsh: return Varied(Mixed(Rgb(56, 48, 38), Rgb(72, 84, 42), 0.4 + 0.4 * S.N1), 1.0 + 0.06 * S.N2, 0.0);
				case KWorks: return Varied(Mixed(Rgb(150, 86, 54), Rgb(112, 70, 46), 0.4 + 0.3 * S.N2), 1.0, 0.0);
				case KWoods:
				{
					// the forest floor: leaf litter, damp dark patches, moss (dim under the canopy; the light its own)
					const Tone Litter = Rgb(104, 86, 58), Damp = Rgb(72, 62, 44), Moss = Rgb(78, 94, 48);
					Tone T = Mixed(Litter, Damp, 0.3 + 0.3 * S.N1);
					T = Mixed(T, Moss, 0.45 * Smooth(-0.1, 0.6, S.N2));
					return Varied(T, 1.0 + 0.05 * S.Broad, 0.02);
				}
				default: break;
				}
				const Tone Red = bClay ? Rgb(150, 116, 70) : Rgb(158, 92, 56);
				switch (C)
				{
				case Ridge:
				{
					Tone T = Mixed(Rgb(146, 148, 74), Rgb(178, 162, 98), 0.3 + 0.4 * Dry + 0.2 * S.N2);
					T = Mixed(T, Red, 0.28 + 0.22 * S.N1);
					return Varied(T, 1.0 + 0.05 * S.Broad, 0.01);
				}
				case Plantation:
				{
					// short grass the cattle kept down, the palms' fallen fronds and husks browning it in drifts
					Tone T = Mixed(Rgb(98, 124, 50), Rgb(126, 140, 62), 0.4 + 0.35 * S.N2 + 0.2 * Dry);
					T = Mixed(T, Rgb(132, 110, 72), 0.18 * Smooth(0.0, 0.7, S.N1));
					return Varied(T, 1.0 + 0.04 * S.Broad, 0.0);
				}
				case Paddy:
				{
					// plots of 12-30 m behind bunds: flooded (the sky in them), green rice, or the dry season's stubble
					const double P = 18.0 + 8.0 * Unit(Hash(S.Key, 0xBADD));
					const double A = S.Along / P, B = S.Across / (0.7 * P);
					const double FA = A - std::floor(A), FB = B - std::floor(B);
					const double Bund = std::max(Smooth(0.6 / P, 0.0, std::min(FA, 1.0 - FA)), Smooth(0.6 / (0.7 * P), 0.0, std::min(FB, 1.0 - FB)));
					const uint64_t Plot = Hash(S.Key, static_cast<uint64_t>(std::floor(A) + 512.0), static_cast<uint64_t>(std::floor(B) + 512.0));
					const bool bFlood = Unit(Plot) < 0.85 - 0.75 * Dry;
					Tone T = bFlood ? Rgb(128, 134, 118) : Dry > 0.5 ? Rgb(170, 150, 100) : Rgb(104, 142, 56);
					T = Mixed(T, Rgb(116, 118, 64), Bund);
					return Varied(T, 1.0 + 0.05 * S.N2, 0.0);
				}
				case Garden:
				{
					Tone T = Mixed(Rgb(104, 128, 52), Red, 0.3 + 0.3 * S.N1);
					return Varied(T, 1.0 + 0.06 * S.N2, 0.0);
				}
				default:
				{
					// kunai: tan-green, straw when old, the tussocks darker; thinning in patches to short grass over red earth;
					// trodden paths through it; burnt patches black
					Tone T = Mixed(Rgb(122, 128, 74), Rgb(166, 158, 106), 0.2 + 0.45 * Dry + 0.3 * S.N1);
					T = Mixed(T, Rgb(92, 98, 56), 0.35 * Smooth(-0.3, 0.7, S.N2));
					T = Mixed(T, Mixed(Rgb(126, 128, 76), Red, Clamp01(0.25 + 0.35 * S.N2)), 0.7 * Smooth(-0.32, -0.5, Thin(S.X, S.Y)));
					T = Mixed(T, Red, 0.12 * Smooth(0.45, 0.9, -S.N2));
					T = Mixed(T, Mixed(Rgb(150, 104, 70), Rgb(146, 134, 92), Clamp01(0.4 + 0.4 * S.N1)), 0.85 * Trod(S.X, S.Y));
					if (Burnt(S.Key, S.X, S.Y)) T = Mixed(Rgb(66, 60, 50), Rgb(104, 94, 72), 0.3 + 0.3 * S.N2);
					return Varied(T, 1.0 + 0.04 * S.Broad, 0.01);
				}
				}
			}

			/** The sea round an island, W metres inland of the waterline (less than nothing): the shallow wash, the
			 * turquoise reef flat mottled with coral heads and brown algae, its white surf line, the deep blue beyond. */
			Tone Sea(double W, double Rf, double N1, double N2) const
			{
				const double Out = -W;
				const Tone Wash = Rgb(176, 220, 206), Flat = Rgb(104, 204, 192), Algae = Rgb(112, 160, 128), Deep = Rgb(30, 90, 140);
				Tone T = Mixed(Wash, Flat, Smooth(0.0, 30.0, Out));
				T = Mixed(T, Algae, 0.35 * Smooth(0.15, 0.6, N2) * Smooth(40.0, 120.0, Out));
				T = Mixed(T, Rgb(70, 170, 184), 0.4 * Smooth(Rf * 0.5, Rf * 0.95, Out)); // (deeper toward the edge)
				const double Surf = Smooth(22.0, 0.0, std::fabs(Out - Rf));
				T = Mixed(T, Rgb(232, 240, 238), 0.9 * Surf);
				T = Mixed(T, Deep, Smooth(Rf + 4.0, Rf + 60.0, Out));
				return Varied(T, 1.0 + 0.03 * N1, 0.0);
			}

			Tone IslandGround(const Spot& S, int C) const
			{
				const double W = Shore(S.X, S.Y);
				if (W < 0.0) return Sea(W, Reef(S.X, S.Y), S.N1, S.N2);
				const Tone Dry_ = Rgb(240, 234, 214), Wet = Rgb(198, 190, 166), Wrack = Rgb(164, 146, 112);
				if (W < 32.0)
				{
					// the beach: the wet band at the water, the tide's wrack of weed and husks, dry sand to the crest
					Tone T = Mixed(Wet, Dry_, Smooth(2.5, 6.0, W));
					T = Mixed(T, Wrack, 0.7 * Smooth(1.6, 0.0, std::fabs(W - 7.5)) * (0.5 + 0.5 * Smooth(-0.4, 0.4, S.N2)));
					if (S.Kind != KLane) return Varied(T, 1.0 + 0.025 * S.N2, 0.0);
				}
				switch (S.Kind)
				{
				case KLane:
				{
					// crushed coral: white, the ruts a little greyer
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = Smooth(0.45, 0.12, std::fabs(Off - 0.9));
					Tone T = Mixed(Rgb(226, 220, 202), Rgb(196, 188, 168), 0.6 * Rut);
					if (S.Lane > 0.0) T = Mixed(T, Rgb(214, 206, 180), Smooth(0.2, 1.6, S.Lane));
					return Varied(T, 1.0 + 0.03 * S.N2, 0.0);
				}
				case KVillage:
				case KYard: return Varied(Mixed(Rgb(214, 204, 178), Rgb(150, 160, 92), S.bNear ? 0.05 : 0.35 * Smooth(0.0, 0.6, S.N1)), 1.0 + 0.04 * S.N2, 0.0);
				case KWater: return Sea(-60.0 - 80.0 * (0.5 + 0.5 * S.N1), 1000.0, S.N1, S.N2);
				case KMarsh: return Varied(Mixed(Rgb(150, 146, 108), Rgb(96, 112, 62), 0.4 + 0.3 * S.N1), 1.0, 0.0);
				case KWorks: return Varied(Mixed(Rgb(212, 202, 176), Rgb(176, 166, 140), 0.4 + 0.3 * S.N2), 1.0, 0.0);
				case KWoods:
				{
					const Tone T = Mixed(Rgb(176, 160, 120), Rgb(110, 126, 60), 0.35 + 0.3 * S.N1);
					return Varied(T, 1.0 + 0.05 * S.N2, 0.0);
				}
				default: break;
				}
				// the grove's floor: coral sand under creeping vines and grass, fallen fronds and husks
				const Tone Sand = Rgb(232, 224, 200), Creep = Rgb(122, 142, 64), Litter = Rgb(170, 146, 102);
				const double Green = C == Strand ? 0.12 + 0.2 * S.N1 : C == Plantation ? 0.48 + 0.28 * S.N1 : 0.42 + 0.3 * S.N1;
				Tone T = Mixed(Sand, Creep, Clamp01(Green + 0.2 * S.N2));
				T = Mixed(T, Litter, 0.3 * Smooth(-0.2, 0.6, -S.N2));
				return Varied(T, 1.0 + 0.03 * S.Broad, 0.0);
			}

			Tone Colour(const Spot& S) const override
			{
				const int C = S.Crop >= 0 ? S.Crop : bIsland ? Grove : Kunai;
				return bIsland ? IslandGround(S, C) : JungleGround(S, C);
			}

			Tone Far(const Spot& S, bool bWoods, double /*Gap*/) const override
			{
				if (bIsland)
				{
					const double W = Shore(S.X, S.Y);
					if (W < 0.0) return Sea(W, Reef(S.X, S.Y), S.N1, 0.0);
					Spot Q = S;
					Q.N2 = 0.0;
					return IslandGround(Q, S.Crop >= 0 ? S.Crop : Grove);
				}
				if (bWoods)
				{
					// the canopy far off: crowns lit and shadowed in patches, a few paler emergents
					Tone T = Mixed(Rgb(58, 88, 38), Rgb(40, 62, 30), 0.3 + 0.35 * S.N1);
					T = Mixed(T, Rgb(86, 112, 50), 0.35 * Smooth(0.3, 0.8, S.N1));
					return Varied(T, 1.0 + 0.08 * S.Broad, 0.02 * S.Broad);
				}
				Spot Q = S;
				Q.N2 = 0.0;
				return JungleGround(Q, S.Crop >= 0 ? S.Crop : Kunai);
			}

			// ------------------------------------------------------------------ the clutter round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				using B = LandField::Base;
				Grown G;
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/) return G;
				if (bIsland)
				{
					const double W = Shore(S.X, S.Y);
					if (W < 1.5) return G;
					if (W < 32.0)
					{
						// (the beach: husks and fronds along the wrack line, bits of coral, a tuft or two by the crest)
						if (std::fabs(W - 7.5) < 2.5) return Pick < 0.6 ? Grown{ B::Clut5, 0.25 } : Grown{ B::Stone, 0.3 };
						return W > 24.0 && Pick < 0.4 ? Grown{ B::Clut4, 0.3 } : Grown{ B::Stone, 0.08 };
					}
					if (S.Kind == KLane) return S.Lane > 0.4 ? Grown{ B::Clut4, 0.4 } : Grown{ B::Stone, 0.06 };
					if (S.Kind == KVillage || S.Kind == KYard) return Pick < 0.5 ? Grown{ B::Clut4, 0.3 } : Grown{ B::Clut5, 0.08 };
					if (S.Kind == KWorks) return Grown{ B::Stone, 0.4 };
					if (S.Kind == KWoods) return Pick < 0.3 ? Grown{ B::Clut1, 0.4 } : Pick < 0.75 ? Grown{ B::Clut4, 1.0 } : Grown{ B::Clut5, 0.45 };
					if (S.Crop == Strand) return Pick < 0.4 ? Grown{ B::Clut4, 0.45 } : Pick < 0.7 ? Grown{ B::Stone, 0.35 } : Grown{ B::Clut5, 0.15 };
					// the grove: vines and grass in patches over the sand, fronds and husks under the palms
					const double Patch = Noise(S.X / 11.0, S.Y / 11.0, Seed ^ 0xC11);
					if (Pick < 0.68) return Grown{ B::Clut4, std::clamp(1.0 + 1.2 * Patch, 0.25, 1.9) };
					if (Pick < 0.9) return Grown{ B::Clut5, 0.45 };
					return Grown{ B::Stone, 0.3 };
				}
				switch (S.Kind)
				{
				case KLane:
				{
					// (grass over the track's broken edges and on its hump, the kunai tall on the verge)
					const double Edge = S.Lane + 0.8 * Rag(S.X, S.Y);
					if (Edge > 1.2) return Pick < 0.55 ? Grown{ B::Clut4, 0.9 } : Grown{ B::Clut0, 0.6 };
					if (Edge > -0.35) return Grown{ B::Clut4, 1.0 };
					if (std::fabs(S.Lane + S.LaneHalf) < 0.35 && Hump(S.X, S.Y) > 0.12) return Grown{ B::Clut4, 0.7 };
					return G;
				}
				case KVillage:
				case KYard: return S.bNear ? G : Pick < 0.6 ? Grown{ B::Clut4, 0.35 } : Pick < 0.85 ? Grown{ B::Clut1, 0.12 } : Grown{ B::Clut3, 0.15 };
				case KMarsh: return Grown{ B::Clut0, 0.9 };
				case KWorks: return Grown{ B::Stone, 0.35 };
				case KWoods:
				{
					// the floor: ferns and gingers, saplings and rattan, leaf litter; a wall of it at the forest's edge
					const double Edge = Smooth(12.0, 3.0, S.Edge);
					if (Pick < 0.42) return Grown{ B::Clut1, 0.95 + 0.6 * Edge };
					if (Pick < 0.56) return Grown{ B::Clut2, 0.42 + 0.4 * Edge };
					if (Pick < 0.94) return Grown{ B::Clut3, 0.55 };
					return Grown{ B::Stone, 0.25 };
				}
				default: break;
				}
				const int C = S.Crop >= 0 ? S.Crop : Kunai;
				const double Patch = Noise(S.X / 9.0 + 0.7, S.Y / 9.0 - 0.3, Seed ^ 0xC12);
				switch (C)
				{
				case Ridge: return Pick < 0.55 ? Grown{ B::Clut0, 0.6 } : Pick < 0.85 ? Grown{ B::Clut4, 0.7 } : Grown{ B::Stone, 0.4 };
				case Plantation:
					// (short grass between the rows, fronds and husks under the palms, regrowth coming back in patches)
					if (Pick < 0.6) return Grown{ B::Clut4, 1.0 };
					if (Pick < 0.75) return Grown{ B::Clut5, 0.35 };
					return Patch > 0.25 ? Grown{ B::Clut0, 0.5 } : Grown{ B::Clut3, 0.25 };
				case Paddy: return Grown{ B::Clut4, Dry > 0.5 ? 0.4 : 1.0 };
				case Garden: return Pick < 0.4 ? Grown{ B::Clut1, 0.6 } : Pick < 0.8 ? Grown{ B::Clut0, 0.6 } : Grown{ B::Clut4, 0.6 };
				default:
					// kunai, thick, thinning in patches to short grass over red earth; a trodden path bare but for a tuft; burnt
					// patches only stubble
					if (Burnt(S.Key, S.X, S.Y)) return Pick < 0.5 ? Grown{ B::Clut4, 0.6 } : G;
					if (Trod(S.X, S.Y) > 0.45) return Pick < 0.3 ? Grown{ B::Clut4, 0.5 } : G;
					{
						const double Th = Thin(S.X, S.Y);
						if (Th < -0.42) return Pick < 0.65 ? Grown{ B::Clut4, 0.9 } : Pick < 0.75 ? Grown{ B::Clut0, 0.5 } : G;
						const double Dense = std::clamp(1.35 + 0.4 * Patch, 0.8, 1.7) * (0.35 + 0.65 * Smooth(-0.42, -0.15, Th));
						return Pick < 0.88 ? Grown{ B::Clut0, Dense } : Grown{ B::Clut4, 0.7 };
					}
				}
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				using B = LandField::Base;
				const int C = S.Crop >= 0 ? S.Crop : -1;
				const bool bBurnt = !bIsland && S.Kind != KWoods && C == Kunai && Burnt(S.Key, S.X, S.Y);
				auto Size = [&P](double Sc, double Sz) {
					P.Scale = static_cast<float>(Sc);
					P.ScaleZ = static_cast<float>(Sz);
				};
				switch (P.Kind)
				{
				case B::Clut0:
				{
					// kunai: 1-2 m on the plain, short on the ridges and in the plantations' regrowth; straw in the dry
					const double High = C == Ridge ? 0.45 + 0.25 * U5 : C == Plantation ? 0.4 + 0.35 * U5 : S.Kind == KMarsh ? 0.9 + 0.3 * U5 : 0.92 + 0.48 * U5;
					Size(0.75 + 0.5 * U4, High);
					const double Straw = Clamp01(Dry + 0.35 * (U6 - 0.5));
					P.Tint = S.Kind == KMarsh ? Tint(0.82, 1.0, 0.86) : Tint(0.96 + 0.1 * Straw, 1.0, 0.94 + 0.08 * U7);
					return true;
				}
				case B::Clut1:
				{
					Size(0.65 + 0.75 * U4, 0.7 + 0.7 * U5);
					const double L = 0.8 + 0.4 * U6;
					P.Tint = bIsland ? Tint(1.05 * L, 1.1 * L, 0.85 * L) : Tint(0.95 * L, L, 0.9 * L);
					return true;
				}
				case B::Clut2:
				{
					Size(0.8 + 0.4 * U4, 0.65 + 0.8 * U5);
					const double L = 0.85 + 0.3 * U6;
					P.Tint = Tint(L, L, L * (0.9 + 0.1 * U7));
					return true;
				}
				case B::Clut3:
				{
					Size(0.8 + 0.7 * U4, 1.0);
					const double L = 0.8 + 0.4 * U6;
					P.Tint = Tint(L * (1.0 + 0.1 * U7), L, L * 0.95);
					return true;
				}
				case B::Clut4:
				{
					Size(0.7 + 0.6 * U4, 0.6 + 0.8 * U5);
					if (bBurnt) P.Tint = Tint(0.42, 0.38, 0.34); // (charred stubble)
					else if (bIsland) P.Tint = Tint(1.0 + 0.15 * U6, 1.0 + 0.08 * U6, 0.85);
					else if (C == Paddy) P.Tint = Dry > 0.5 ? Tint(1.35, 1.12, 0.8) : Tint(0.95, 1.12, 0.85);
					else P.Tint = Tint(0.95 + 0.25 * Dry * U6, 1.0, 0.86 + 0.08 * U7);
					return true;
				}
				case B::Clut5:
				{
					Size(0.8 + 0.4 * U4, 0.8 + 0.4 * U5);
					const double L = 0.85 + 0.3 * U6;
					P.Tint = Tint(L, L, L);
					return true;
				}
				case B::Stone:
				{
					Size(0.35 + 1.0 * U4 * U4, 0.5 + 0.4 * U5);
					// (coral bits white and grey on an island; laterite nodules and grey stones in the jungle)
					if (bIsland) P.Tint = U6 < 0.3 ? Tint(1.25, 1.24, 1.2) : Tint(1.55, 1.52, 1.44);
					else P.Tint = U6 < 0.5 ? Tint(1.3, 0.86, 0.66) : Tint(1.05, 1.0, 0.92);
					return true;
				}
				default: return false;
				}
			}

			// ------------------------------------------------------------------ the kit

			const char* Folder() const override { return "Asia"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KTree: return "JungleTree";
				case KGiant: return bIsland ? nullptr : "JungleGiant";
				case KPalm: return "CoconutPalm";
				case KPalmTall: return "CoconutPalmTall";
				case KPalmYoung: return "CoconutPalmYoung";
				case KBanana: return "Banana";
				case KGrass: return bIsland ? nullptr : "ElephantGrass";
				case KUnder: return "Undergrowth";
				case KStilt: return "StiltHouse";
				case KStiltTin: return "StiltHouseTin";
				case KThatch: return "ThatchHouse";
				case KReeds: return "Reeds";
				case KBoulder: return bIsland ? nullptr : "Boulder";
				case KBamboo: return bMainland && !bIsland ? "Bamboo" : nullptr;
				case KStump: return bIsland ? "CoconutPalmTall" : nullptr;
				case KThicket: return bIsland ? nullptr : "Boulder";
				default: return nullptr;
				}
			}

			/** Hanging lianas on a tree (its near level): loops swagged between two branches and cords hanging toward the
			 * ground, woody grey-brown and green. High: the tree's height; Reach: its crown's. */
			void Lianas(LandField::Mesh& M, double High, double Reach, uint64_t Key) const
			{
				Writer W(M);
				Rng R(Key);
				for (int K = 0; K < 7; ++K)
				{
					const double A0 = R.Range(0.0, 2.0 * Pi), A1 = A0 + R.Range(0.6, 1.6), Z0 = R.Range(0.45, 0.7) * High, Z1 = R.Range(0.4, 0.7) * High;
					const double R0 = R.Range(0.25, 0.55) * Reach, R1 = R.Range(0.25, 0.55) * Reach, Low = R.Range(0.06, 0.3) * High;
					const bool bCord = K >= 4;
					const V3 P0{ R0 * std::cos(A0), R0 * std::sin(A0), Z0 }, P1{ R1 * std::cos(A1), R1 * std::sin(A1), Z1 };
					std::vector<V3> Line;
					std::vector<double> Wide;
					constexpr int Steps = 12;
					for (int S = 0; S <= Steps; ++S)
					{
						const double T = static_cast<double>(S) / Steps;
						V3 P = bCord ? P0 + (V3{ P0.X * 0.2, P0.Y * 0.2, Low } - P0) * T : P0 + (P1 - P0) * T;
						if (!bCord) P.Z -= (Lerp(Z0, Z1, T) - Low) * 4.0 * T * (1.0 - T);
						Line.push_back(P);
						Wide.push_back(bCord ? 0.03 : 0.06 - 0.02 * std::fabs(T - 0.5));
					}
					const V3 Run = Line.back() - Line.front();
					const V3 Across = Norm(V3{ -Run.Y, Run.X, 0.0 });
					const Tone Wood = Rgb(96, 86, 62), Green = Rgb(70, 86, 46);
					Ribbon(W, Line, Wide, Across, bCord ? Green : Wood, bCord ? Mixed(Green, Wood, 0.4) : Wood);
					Ribbon(W, Line, Wide, Cross(Across, V3{ 0, 0, 1 }), bCord ? Green : Wood, Wood);
				}
			}

			/** Pale bark, blotched with lichen and moss, for a tree whose bark paints are Bark and BarkDark. */
			static void PaleBark(LandField::Mesh& M, int BR, int BG, int BB, int DR, int DG, int DB, uint64_t Key)
			{
				for (size_t K = 0; K < M.Colours.size(); ++K)
				{
					LandField::Rgb8& C = M.Colours[K];
					const bool bBark = Same(C, BR, BG, BB), bDark = Same(C, DR, DG, DB);
					if (!bBark && !bDark) continue;
					const double U = Unit(Hash(Key, static_cast<uint64_t>(std::lround(M.Positions[3 * K] * 3.0) + 4096), static_cast<uint64_t>(std::lround(M.Positions[3 * K + 2] * 2.0) + 4096)));
					Tone T = bBark ? Rgb(138, 134, 116) : Rgb(104, 100, 86);
					if (U < 0.22) T = Mixed(T, Rgb(184, 182, 166), 0.6);      // (lichen, whitish)
					else if (U > 0.84) T = Mixed(T, Rgb(80, 96, 50), 0.6);    // (moss)
					C = Byte(T);
				}
			}

			/** A coconut palm snapped by the shelling: its trunk below Cut, a splintered pale top, a few fronds hanging dead. */
			static void Snap(LandField::Mesh& M, double Cut, bool bNear)
			{
				auto Bark = [&M](uint32_t V) {
					const LandField::Rgb8& C = M.Colours[V];
					return Same(C, 124, 114, 98) || Same(C, 86, 78, 68);
				};
				double SX = 0.0, SY = 0.0;
				int N = 0;
				for (size_t K = 0; K < M.Colours.size(); ++K)
					if (Bark(static_cast<uint32_t>(K)) && std::fabs(M.Positions[3 * K + 2] - Cut) < 0.8)
					{
						SX += M.Positions[3 * K];
						SY += M.Positions[3 * K + 1];
						++N;
					}
				const V3 Top{ N ? SX / N : 0.0, N ? SY / N : 0.0, Cut };
				M = Kept(M, [&M, &Bark, Cut](uint32_t A, uint32_t B, uint32_t C) {
					return Bark(A) && Bark(B) && Bark(C) && M.Positions[3 * A + 2] <= Cut && M.Positions[3 * B + 2] <= Cut && M.Positions[3 * C + 2] <= Cut;
				});
				Writer W(M);
				const Tone Splinter = Rgb(212, 190, 144), Grey = Rgb(150, 138, 116);
				for (int K = 0; K < (bNear ? 9 : 4); ++K)
				{
					const double A = 2.0 * Pi * K / (bNear ? 9.0 : 4.0) + 0.3 * std::sin(K * 2.7);
					const double Up = 0.4 + 0.9 * (0.5 + 0.5 * std::sin(K * 4.1));
					const V3 F0{ Top.X + 0.16 * std::cos(A), Top.Y + 0.16 * std::sin(A), Cut - 0.5 };
					W.Blade(F0.X, F0.Y, F0.Z, Top.X + 0.1 * std::cos(A), Top.Y + 0.1 * std::sin(A), Cut + Up, 0.12, Grey, Splinter);
				}
				if (!bNear) return;
				for (int K = 0; K < 4; ++K)
				{
					// a dead frond hanging from the top, its leaflets gone ragged
					const double A = 1.3 + K * 1.7;
					const V3 Out{ std::cos(A), std::sin(A), 0.0 };
					const double Long = 2.6 + 0.5 * (K % 2);
					std::vector<V3> Line;
					std::vector<double> Wide;
					for (int S = 0; S <= 6; ++S)
					{
						const double T = S / 6.0;
						Line.push_back(Top + Out * (0.2 + 0.9 * std::sin(T * 1.3)) + V3{ 0, 0, -0.3 - Long * T * T });
						Wide.push_back(0.5 * (1.0 - T) + 0.08);
					}
					Ribbon(W, Line, Wide, V3{ -Out.Y, Out.X, 0.0 }, Rgb(150, 128, 84), Rgb(128, 106, 70));
				}
			}

			/** A thicket at the forest's edge (some 4.5 m high and 4.5 across): masses of leaves from the ground up, slim
			 * pale stems through them, vines hanging off its top; far off, three lumps of it. */
			void Thicket(LandField::BaseMesh& M) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ 0x7C1C);
				const Tone Lit = Rgb(78, 108, 48), Under = Rgb(34, 50, 26);
				for (int K = 0; K < 3; ++K)
				{
					const double A = 2.0 * Pi * K / 3.0 + 0.4, High = 3.2 + 0.4 * K;
					const V3 Top{ 0.5 * std::cos(A), 0.5 * std::sin(A), High };
					const std::vector<V3> Line = { V3{ 0.15 * std::cos(A), 0.15 * std::sin(A), 0.0 }, Top * 0.5, Top };
					Ribbon(W, Line, { 0.06, 0.05, 0.03 }, V3{ 1, 0, 0 }, Rgb(126, 120, 98), Rgb(112, 112, 86));
					Ribbon(W, Line, { 0.06, 0.05, 0.03 }, V3{ 0, 1, 0 }, Rgb(126, 120, 98), Rgb(112, 112, 86));
				}
				// (x, y, z of its middle, its half width and half height, its leaves)
				const double Mass[7][6] = { { 0.0, 0.0, 0.75, 1.9, 0.85, 44 }, { 0.9, 0.5, 1.6, 1.2, 0.9, 30 }, { -0.8, -0.6, 1.8, 1.2, 1.0, 30 }, { 0.3, -0.9, 2.6, 1.1, 0.9, 26 },
							{ -0.4, 0.7, 3.0, 1.1, 0.9, 26 },  { 0.2, 0.1, 3.9, 1.0, 0.75, 24 }, { 1.2, -0.3, 3.3, 0.8, 0.7, 20 } };
				for (int K = 0; K < 7; ++K)
				{
					const double L = R.Range(0.85, 1.15), Warm = R.Range(-0.06, 0.06);
					const Tone Lk = Varied(Lit, L, Warm), Uk = Varied(Under, L, Warm);
					Leafy(W, V3{ Mass[K][0], Mass[K][1], Mass[K][2] }, Mass[K][3], Mass[K][4], static_cast<int>(Mass[K][5]), Hash(Seed, 0x7C1D, static_cast<uint64_t>(K)), Lk, Uk);
				}
				for (int K = 0; K < 4; ++K)
				{
					// a vine hanging off the top, swaying out
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.9, 1.5), Z0 = R.Range(3.0, 4.2), Drop = R.Range(1.6, 2.8);
					const V3 Out{ std::cos(A), std::sin(A), 0.0 };
					const V3 P0 = Out * D + V3{ 0, 0, Z0 };
					Ribbon(W, { P0, P0 + Out * 0.25 + V3{ 0, 0, -0.4 * Drop }, P0 + Out * 0.3 + V3{ 0, 0, -0.75 * Drop }, P0 + Out * 0.22 + V3{ 0, 0, -Drop } }, { 0.12, 0.1, 0.08, 0.0 },
						   V3{ -Out.Y, Out.X, 0.0 }, Rgb(56, 80, 36), Rgb(82, 110, 50));
				}
				// (far off: five lumps of it, mottled, as its masses lie)
				Writer F(M.Far);
				const double Lump[5][5] = { { 0.0, 0.0, 0.9, 2.1, 1.0 }, { 0.9, 0.5, 2.0, 1.4, 1.1 }, { -0.8, -0.6, 2.1, 1.4, 1.2 }, { -0.2, 0.4, 3.3, 1.3, 1.0 }, { 0.6, -0.4, 3.8, 1.0, 0.8 } };
				for (int K = 0; K < 5; ++K)
					Blob(F, V3{ Lump[K][0], Lump[K][1], Lump[K][2] }, Lump[K][3], Lump[K][4], 4, 6, Hash(Seed, 0x7C1E, static_cast<uint64_t>(K)), Mixed(Lit, Rgb(96, 124, 56), 0.15 * K), Under);
			}

			/** The undergrowth's far level, the kit's a stub: three low lumps of leaves as wide as its near. */
			void UnderFar(LandField::BaseMesh& M) const
			{
				M.Far = LandField::Mesh();
				Writer F(M.Far);
				const Tone Lit = Rgb(74, 100, 46), Under = Rgb(36, 52, 28);
				Blob(F, V3{ 0.0, 0.0, 0.35 }, 1.15, 0.42, 4, 6, Hash(Seed, 0x7D1), Lit, Under);
				Blob(F, V3{ 0.7, 0.45, 0.45 }, 0.75, 0.42, 4, 6, Hash(Seed, 0x7D2), Lit, Under);
				Blob(F, V3{ -0.7, -0.35, 0.4 }, 0.8, 0.4, 4, 6, Hash(Seed, 0x7D3), Lit, Under);
			}

			/** Tin roofs and walls gone patchy with rust: the kit's bright and dull tin rusted in blotches a metre or so
			 * across, streaked darker where the rain runs. */
			static void Rust(LandField::Mesh& M, uint64_t Key)
			{
				for (size_t K = 0; K < M.Colours.size(); ++K)
				{
					LandField::Rgb8& C = M.Colours[K];
					const bool bTin = Same(C, 156, 162, 164), bDull = Same(C, 122, 126, 124);
					if (!bTin && !bDull) continue;
					const auto Q = [&M, K](int Axis, double Per) { return static_cast<uint64_t>(std::lround(M.Positions[3 * K + static_cast<size_t>(Axis)] / Per) + 4096); };
					const double U = Unit(Hash(Key, Q(0, 1.1), Q(1, 1.1) * 8192 + Q(2, 0.9)));
					const Tone Tin = bTin ? Rgb(150, 154, 152) : Rgb(118, 120, 116);
					Tone T = Tin;
					if (U < 0.3) T = Mixed(Tin, Rgb(138, 90, 62), 0.8);
					else if (U < 0.55) T = Mixed(Tin, Rgb(124, 92, 70), 0.45);
					else if (U > 0.85) T = Varied(Tin, 0.8, 0.02);
					C = Byte(T);
				}
			}

			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Sight = [&M](float Mid, float Half, float Across, float Trunk) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
					M.SightTrunk = Trunk;
				};
				const bool bRead = M.Name.empty() ? false : Fd.Kit.count(M.Name) > 0;
				switch (K)
				{
				case KTree:
					if (bRead)
					{
						PaleBark(M.Near, 110, 102, 88, 74, 64, 56, Seed ^ 0xBA4);
						PaleBark(M.Far, 110, 102, 88, 74, 64, 56, Seed ^ 0xBA4);
						Repaint(M.Near, 44, 62, 38, Rgb(60, 84, 44));
						Repaint(M.Far, 44, 62, 38, Rgb(60, 84, 44));
						if (!bIsland) Lianas(M.Near, 16.0, 6.0, Seed ^ 0x11A);
					}
					M.Paint = 1;
					M.FarFrom = 90.0f;
					Sight(0.62f, 0.38f, 0.75f, 0.6f);
					break;
				case KGiant:
					if (bRead)
					{
						PaleBark(M.Near, 154, 148, 132, 74, 64, 56, Seed ^ 0xBA5);
						PaleBark(M.Far, 154, 148, 132, 74, 64, 56, Seed ^ 0xBA5);
						Repaint(M.Near, 44, 62, 38, Rgb(60, 84, 44));
						Repaint(M.Far, 44, 62, 38, Rgb(60, 84, 44));
						Lianas(M.Near, 32.0, 11.0, Seed ^ 0x11B);
					}
					M.Paint = 1;
					M.FarFrom = 130.0f;
					Sight(0.7f, 0.3f, 0.7f, 1.3f);
					break;
				case KPalm:
				case KPalmTall:
				case KPalmYoung:
					M.Paint = 1;
					M.FarFrom = 120.0f;
					Sight(0.92f, 0.08f, 0.6f, 0.35f);
					break;
				case KStump:
					if (bRead)
					{
						Snap(M.Near, 9.0, true);
						Snap(M.Far, 9.0, false);
					}
					M.Paint = 0;
					M.FarFrom = 100.0f;
					Sight(0.5f, 0.5f, 0.3f, 0.35f);
					break;
				case KBanana:
				case KBamboo:
					M.Paint = 1;
					M.FarFrom = K == KBamboo ? 80.0f : 60.0f;
					Sight(0.55f, 0.45f, 0.8f, 0.2f);
					break;
				case KUnder:
					if (bRead) UnderFar(M);
					M.Paint = 1;
					M.FarFrom = 55.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				case KThicket:
					Thicket(M);
					M.Paint = 1;
					M.FarFrom = 70.0f;
					Sight(0.5f, 0.5f, 0.92f, 0.0f);
					break;
				case KGrass:
				case KReeds:
					M.Paint = 2;
					M.FarFrom = 60.0f;
					Sight(0.55f, 0.45f, 0.8f, 0.0f);
					break;
				case KStilt:
				case KStiltTin:
				case KThatch:
					if (bRead && K == KStiltTin)
					{
						Rust(M.Near, Seed ^ 0x7157);
						Rust(M.Far, Seed ^ 0x7157);
					}
					M.Paint = 0;
					M.FarFrom = 260.0f;
					Sight(0.5f, 0.5f, 0.9f, 0.0f);
					break;
				case KBoulder:
					M.Paint = 0;
					M.FarFrom = 150.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				default: break;
				}
			}

			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KStilt:
				case KStiltTin:
				case KThatch:
					W.Box(0.0, 0.0, 2.2, 4.0, 2.6, 0.9, 0.0, Rgb(150, 126, 90));
					W.Box(0.0, 0.0, 3.9, 4.6, 3.2, 0.8, 0.0, Rgb(150, 130, 100));
					break;
				case KBoulder: W.Rock(0.0, 0.0, 0.8, 0.7, 11, Rgb(150, 140, 120), Rgb(96, 90, 80)); break;
				case KGrass:
				case KReeds:
				case KUnder:
					W.Rock(0.0, 0.0, 0.8, 1.4, 7, Rgb(140, 150, 70), Rgb(90, 100, 50));
					break;
				default:
					W.Box(0.0, 0.0, 5.0, 0.18, 0.18, 5.0, 0.0, Rgb(150, 144, 128));
					W.Rock(0.0, 0.0, 3.0, 3.0, 5, Rgb(80, 110, 48), Rgb(46, 66, 32));
					break;
				}
				M.Far = M.Near;
				M.FarFrom = 300.0f;
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ (0xC10 + static_cast<uint64_t>(K)));
				switch (K)
				{
				case 0:
				{
					// Kunai: a clump of saw-edged blades 1-1.6 m, leaning out and bending over, yellow-green to straw at
					// their tips; two or three stems carrying white silky plumes.
					for (int B = 0; B < 30; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), R0 = R.Range(0.0, 0.18), High = R.Range(0.8, 1.55), Lean = R.Range(0.12, 0.55);
						const double Bend = R.Range(0.1, 0.45), Green = R.Next();
						const V3 Out{ std::cos(A), std::sin(A), 0.0 };
						const V3 Foot{ R0 * Out.X, R0 * Out.Y, 0.0 };
						const std::vector<V3> Line = { Foot, Foot + Out * (0.25 * Lean * High) + V3{ 0, 0, 0.5 * High }, Foot + Out * (0.65 * Lean * High) + V3{ 0, 0, 0.85 * High },
													   Foot + Out * ((Lean + Bend) * High) + V3{ 0, 0, (0.95 - Bend) * High } };
						const Tone Tip = Mixed(Rgb(178, 168, 120), Rgb(138, 146, 92), Green);
						Ribbon(W, Line, { 0.019, 0.016, 0.011, 0.0 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(96, 100, 62), Tip);
					}
					for (int B = 0; B < 3; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(1.4, 1.75);
						const V3 Out{ std::cos(A), std::sin(A), 0.0 };
						const V3 Top = Out * 0.2 + V3{ 0, 0, High };
						Ribbon(W, { V3{}, Out * 0.08 + V3{ 0, 0, 0.6 * High }, Top }, { 0.012, 0.01, 0.008 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(120, 120, 64), Rgb(170, 160, 104));
						Ribbon(W, { Top, Top + Out * 0.04 + V3{ 0, 0, 0.12 }, Top + Out * 0.1 + V3{ 0, 0, 0.22 }, Top + Out * 0.18 + V3{ 0, 0, 0.27 } }, { 0.012, 0.03, 0.026, 0.0 }, V3{ -Out.Y, Out.X, 0.0 },
							   Rgb(200, 194, 174), Rgb(234, 230, 216));
					}
					M.FarFrom = 48.0f;
					M.Paint = 2;
					break;
				}
				case 1:
				{
					// The forest floor's growth: fern fronds arching out, their leaflets either side; broad leaves of ginger
					// and taro drooping from their stalks.
					for (int F = 0; F < 6; ++F)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Long = R.Range(0.55, 0.9), Rise = R.Range(0.3, 0.55);
						const V3 Out{ std::cos(A), std::sin(A), 0.0 }, Side{ -Out.Y, Out.X, 0.0 };
						auto At = [&](double T) { return Out * (Long * T) + V3{ 0, 0, Rise * std::sin(T * 2.4) / std::sin(2.4 * 0.65) * 0.8 * (1.0 - 0.2 * T) }; };
						std::vector<V3> Line;
						for (int S = 0; S <= 5; ++S) Line.push_back(At(S / 5.0));
						Ribbon(W, Line, { 0.014, 0.012, 0.01, 0.008, 0.006, 0.0 }, Side, Rgb(56, 72, 36), Rgb(70, 96, 44));
						for (int Q = 1; Q <= 9; ++Q)
						{
							const double T = Q / 10.0, Pin = 0.17 * (1.0 - 0.75 * T) + 0.03;
							const V3 Root = At(T);
							for (int Sd = -1; Sd <= 1; Sd += 2)
							{
								const V3 Tip = Root + Side * (Sd * Pin) + Out * (0.35 * Pin) + V3{ 0, 0, -0.25 * Pin };
								W.Blade(Root.X, Root.Y, Root.Z, Tip.X, Tip.Y, Tip.Z, 0.045 * (1.0 - 0.5 * T), Rgb(44, 70, 30), Rgb(78, 112, 46));
							}
						}
					}
					for (int L = 0; L < 3; ++L)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.45, 0.85);
						const V3 Out{ std::cos(A), std::sin(A), 0.0 };
						const V3 Stalk = Out * 0.15 + V3{ 0, 0, High };
						Ribbon(W, { V3{}, Out * 0.05 + V3{ 0, 0, 0.5 * High }, Stalk }, { 0.018, 0.016, 0.012 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(70, 90, 44), Rgb(80, 104, 50));
						Ribbon(W, { Stalk, Stalk + Out * 0.1 + V3{ 0, 0, 0.03 }, Stalk + Out * 0.22 + V3{ 0, 0, 0.0 }, Stalk + Out * 0.33 + V3{ 0, 0, -0.07 }, Stalk + Out * 0.4 + V3{ 0, 0, -0.16 } },
							   { 0.03, 0.17, 0.2, 0.13, 0.0 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(50, 80, 36), Rgb(88, 124, 50));
					}
					M.FarFrom = 42.0f;
					M.Paint = 1;
					break;
				}
				case 2:
				{
					// A sapling or a rattan whip: a thin pale stem 2-3.5 m, a few leaves up it, a spray of long drooping
					// leaves at its head.
					const double High = 2.6;
					const V3 Lean{ 0.12, 0.05, 0.0 };
					const V3 Head = Lean + V3{ 0, 0, High };
					Ribbon(W, { V3{}, Lean * 0.5 + V3{ 0, 0, 0.5 * High }, Head }, { 0.035, 0.028, 0.02 }, V3{ 1, 0, 0 }, Rgb(126, 122, 98), Rgb(110, 116, 80));
					Ribbon(W, { V3{}, Lean * 0.5 + V3{ 0, 0, 0.5 * High }, Head }, { 0.035, 0.028, 0.02 }, V3{ 0, 1, 0 }, Rgb(126, 122, 98), Rgb(110, 116, 80));
					for (int L = 0; L < 9; ++L)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Long = R.Range(0.6, 1.1);
						const V3 Out{ std::cos(A), std::sin(A), 0.0 };
						const bool bSide = L >= 6;
						const V3 Root = bSide ? Lean * 0.6 + V3{ 0, 0, R.Range(0.9, 1.9) } : Head;
						const double L2 = bSide ? 0.5 * Long : Long;
						Ribbon(W, { Root, Root + Out * (0.45 * L2) + V3{ 0, 0, 0.15 * L2 }, Root + Out * (0.85 * L2) + V3{ 0, 0, -0.1 * L2 }, Root + Out * L2 + V3{ 0, 0, -0.45 * L2 } },
							   { 0.03, 0.11, 0.08, 0.0 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(44, 70, 30), Rgb(84, 116, 46));
					}
					M.FarFrom = 55.0f;
					M.Paint = 1;
					break;
				}
				case 3:
				{
					// Leaf litter: fallen leaves brown, rust and yellow, a twig.
					for (int L = 0; L < 16; ++L)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = 0.6 * std::sqrt(R.Next()), Turn = R.Range(0.0, Pi), Long = R.Range(0.08, 0.16), Z = R.Range(0.008, 0.03);
						const double Pick = R.Next();
						const Tone C = Pick < 0.35 ? Rgb(118, 86, 52) : Pick < 0.6 ? Rgb(146, 88, 46) : Pick < 0.8 ? Rgb(156, 128, 64) : Rgb(74, 58, 40);
						const double X = D * std::cos(A), Y = D * std::sin(A), CT = std::cos(Turn), ST = std::sin(Turn);
						const uint32_t P0 = W.Vert(X - CT * Long, Y - ST * Long, Z, 0, 0, 1, C), P1 = W.Vert(X - ST * 0.4 * Long, Y + CT * 0.4 * Long, Z + 0.01, 0, 0, 1, C);
						const uint32_t P2 = W.Vert(X + CT * Long, Y + ST * Long, Z, 0, 0, 1, C), P3 = W.Vert(X + ST * 0.4 * Long, Y - CT * 0.4 * Long, Z + 0.01, 0, 0, 1, Varied(C, 0.85, 0.0));
						W.Tri(P0, P3, P2);
						W.Tri(P0, P2, P1);
					}
					W.Box(0.1, -0.05, 0.02, 0.32, 0.018, 0.018, 0.6, Rgb(96, 80, 60));
					M.FarFrom = 26.0f;
					M.Paint = 0;
					break;
				}
				case 4:
				{
					// Short grass: a tuft of blades 15-35 cm (a plantation's sward, an island's creeping grass, stubble).
					for (int B = 0; B < 14; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.14, 0.34), Lean = R.Range(0.15, 0.6), R0 = R.Range(0.0, 0.08);
						const V3 Out{ std::cos(A), std::sin(A), 0.0 };
						const V3 Foot = Out * R0;
						Ribbon(W, { Foot, Foot + Out * (0.4 * Lean * High) + V3{ 0, 0, 0.6 * High }, Foot + Out * (Lean * High) + V3{ 0, 0, High } }, { 0.018, 0.012, 0.0 },
							   V3{ -Out.Y, Out.X, 0.0 }, Rgb(78, 104, 44), Mixed(Rgb(126, 148, 64), Rgb(160, 156, 92), R.Next()));
					}
					M.FarFrom = 32.0f;
					M.Paint = 2;
					break;
				}
				case 5:
				{
					// A palm's droppings: two or three husks and a fallen frond lying dry, its leaflets splayed.
					for (int H = 0; H < 3; ++H)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.2, 0.7);
						W.Rock(D * std::cos(A), D * std::sin(A), 0.11, 0.13, Hash(Seed, 0x4E5, static_cast<uint64_t>(H)), Rgb(150, 112, 70), Rgb(96, 72, 48));
					}
					const double A = R.Range(0.0, 2.0 * Pi);
					const V3 Out{ std::cos(A), std::sin(A), 0.0 }, Side{ -Out.Y, Out.X, 0.0 };
					const V3 Base0 = Out * -1.2 + V3{ 0, 0, 0.03 };
					Ribbon(W, { Base0, Base0 + Out * 1.2 + V3{ 0, 0, 0.02 }, Base0 + Out * 2.4 }, { 0.06, 0.04, 0.0 }, V3{ 0, 0, 1 }, Rgb(150, 126, 84), Rgb(140, 118, 80));
					for (int L = 0; L < 22; ++L)
					{
						const double T = 0.06 + 0.9 * L / 22.0, Long = 0.75 * (1.0 - 0.55 * T), Side_ = (L % 2) ? 1.0 : -1.0;
						const V3 Root = Base0 + Out * (2.4 * T) + V3{ 0, 0, 0.02 };
						const V3 Tip = Root + (Side * Side_ + Out * 0.5) * Long + V3{ 0, 0, 0.01 };
						W.Blade(Root.X, Root.Y, Root.Z, Tip.X, Tip.Y, Tip.Z, 0.08, Rgb(160, 134, 88), Rgb(182, 156, 104));
					}
					M.FarFrom = 34.0f;
					M.Paint = 0;
					break;
				}
				default: break;
				}
			}

			// ------------------------------------------------------------------ what stands on it

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0x51E6);
				int Built = 0;
				// (houses on posts loose round the village's track and its swept plaza, each its own way; an island's
				// huts of thatch and tin, the garrison's sheds)
				const int Want = 16 + R.Below(8);
				for (int K = 0; K < 600 && Built < Want; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * std::sqrt(R.Next()) * 0.92;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const double Yaw = R.Range(0.0, 2.0 * Pi), Kind = R.Next();
					LandField::Base B = LandField::Base::Kit8;
					double Long = 9.4, Deep = 11.2;
					if (bIsland ? Kind < 0.3 : Kind < 0.14)
					{
						B = LandField::Base::Kit9;
						Long = 10.2;
						Deep = 7.6;
					}
					else if (bIsland ? Kind < 0.75 : Kind < 0.28)
					{
						B = LandField::Base::Kit10;
						Long = 10.2;
						Deep = 7.2;
					}
					if (H.House(B, X, Y, Yaw, Long, Deep)) ++Built;
				}
				H.Tally(Built, 0, 0);
			}

			void Walls(Host& H) override
			{
				if (!bIsland) return;
				// The seawall along the north beach: coconut logs and coral some 1.5 m high, a few metres up from the
				// wrack line, broken where a track runs down to the water.
				std::vector<std::pair<double, double>> Line;
				auto Flush = [&H, &Line]() {
					if (Line.size() >= 4) H.Wall(Line, 1.5, 0.7);
					Line.clear();
				};
				for (double X = -60.0; X <= Wf + 60.0; X += 1.8)
				{
					double Y = Df + 120.0;
					for (int K = 0; K < 4; ++K) Y += Shore(X, Y) - 13.0;
					const Spot S = H.At(X, Y);
					if (S.Lane < 4.0 || !H.Free(X, Y, 0.8, true)) Flush();
					else Line.push_back({ X, Y });
				}
				Flush();
			}
			Tone WallStone() const override { return Rgb(132, 116, 92); }
			double WallIvy() const override { return 0.0; }

			/** Plant if there is room (in the laid field, off the houses, lanes and ways). */
			static void Put(Host& H, LandField::Base B, double X, double Y, double Room, double Yaw, double Scale, double ScaleZ, LandField::Rgb8 Tn)
			{
				if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tn);
			}

			void Plants(Host& H) override
			{
				using B = LandField::Base;
				const double Bd = H.Band();
				constexpr double Cell = 8.0;
				const int NI = static_cast<int>(std::ceil((H.Width() + 2.0 * Bd) / Cell)), NJ = static_cast<int>(std::ceil((H.Depth() + 2.0 * Bd) / Cell));
				const uint64_t Key0 = H.Seed() ^ 0x9A48;
				// (the palms lean seaward: CoconutPalm's crown lies 31 degrees and CoconutPalmTall's 58 degrees round from
				// its x axis, so a palm leans toward Way when it is turned Way less that)
				constexpr double LeanPalm = 31.0 * Pi / 180.0, LeanTall = 58.0 * Pi / 180.0;
				auto Leaf = [](double L, double Warm) { return Tint(L * (1.0 + Warm), L, L * (1.0 - Warm)); };
				for (int J = 0; J < NJ; ++J)
					for (int I = 0; I < NI; ++I)
					{
						Rng R(Hash(Key0, static_cast<uint64_t>(I), static_cast<uint64_t>(J)));
						const double X0 = -Bd + I * Cell, Y0 = -Bd + J * Cell;
						const double JX = R.Next(), JY = R.Next();
						const double X = X0 + JX * Cell, Y = Y0 + JY * Cell;
						const Spot S = H.At(X, Y);
						if (S.Kind == KWater || S.Kind == KLane) continue;
						// Every draw in a statement of its own, in a set order (g++ and clang take a call's arguments in
						// different orders): another place in the cell; a thing stood there at a random turn, size and shade.
						double PX = X, PY = Y;
						auto Elsewhere = [&R, &PX, &PY, X0, Y0]() {
							const double A = R.Next();
							const double Bq = R.Next();
							PX = X0 + A * Cell;
							PY = Y0 + Bq * Cell;
						};
						auto Stand = [&H, &R, &Leaf](B Kind, double AX, double AY, double Room, double ScLo, double ScHi, double SzLo, double SzHi, double LLo, double LHi, double Warm) {
							const double Yw = R.Range(0.0, 2.0 * Pi);
							const double Sc = R.Range(ScLo, ScHi);
							const double Sz = R.Range(SzLo, SzHi);
							const double Lf = R.Range(LLo, LHi);
							Put(H, Kind, AX, AY, Room, Yw, Sc, Sz, Leaf(Lf, Warm));
						};
						auto Tuft = [&H, &R](B Kind, double AX, double AY, double Room, double ScLo, double ScHi, double SzLo, double SzHi, LandField::Rgb8 Tn) {
							const double Yw = R.Range(0.0, 2.0 * Pi);
							const double Sc = R.Range(ScLo, ScHi);
							const double Sz = R.Range(SzLo, SzHi);
							Put(H, Kind, AX, AY, Room, Yw, Sc, Sz, Tn);
						};
						if (bIsland)
						{
							const double W = Shore(X, Y);
							if (W < 14.0) continue;
							// a palm: a third snapped to a pole by the bombardment, a fifth of the rest its fronds gone brown
							auto Palm = [&](double AX, double AY, double Way) {
								const double Snapped = R.Next();
								const double Brown = R.Next();
								const double Which = R.Next();
								const double Sc = R.Range(0.8, 1.25);
								const double Sz = R.Range(0.76, 1.15);
								const double Lf = R.Range(0.9, 1.08);
								const LandField::Rgb8 Tn = Brown < 0.22 ? Tint(1.32, 1.06, 0.74) : Leaf(Lf, 0.03);
								if (Snapped < 0.33) Put(H, B::Kit14, AX, AY, 0.5, Way, Sc, 0.22 + 0.8 * (Sz - 0.76) / 0.39, Leaf(Lf, 0.0));
								else if (Which < 0.55) Put(H, B::Kit3, AX, AY, 0.5, Way - LeanTall, Sc * 0.95, Sz * 0.82, Tn);
								else Put(H, B::Kit2, AX, AY, 0.5, Way - LeanPalm, Sc, Sz, Tn);
							};
							const double Yaw = R.Range(0.0, 2.0 * Pi);
							if (W < 40.0)
							{
								// the strand along the beach's crest: scrub, young palms, a few old ones leaning out over the sand
								if (R.Next() < 0.6) Stand(B::Kit7, X, Y, 0.8, 1.0, 1.7, 0.9, 1.5, 0.9, 1.1, 0.05);
								Elsewhere();
								if (R.Next() < 0.25) Stand(B::Kit4, PX, PY, 0.5, 0.6, 1.0, 0.6, 1.0, 0.95, 1.05, 0.0);
								Elsewhere();
								if (R.Next() < 0.2) Palm(PX, PY, std::atan2(PY - 0.5 * Df, PX - 0.5 * Wf));
								continue;
							}
							switch (S.Kind)
							{
							case KVillage:
							case KYard:
								if (R.Next() < 0.35) Palm(X, Y, Yaw);
								Elsewhere();
								if (R.Next() < 0.25) Stand(B::Kit5, PX, PY, 0.6, 0.8, 1.2, 0.8, 1.2, 0.95, 1.05, 0.0);
								continue;
							case KWorks:
							case KMarsh: continue;
							case KWoods:
								// a dense grove: palms over broadleaf trees, pandanus and scrub
								if (R.Next() < 0.6) Palm(X, Y, Yaw);
								Elsewhere();
								if (R.Next() < 0.25) Stand(B::Kit0, PX, PY, 0.8, 0.45, 0.8, 0.5, 0.85, 0.9, 1.05, 0.0);
								Elsewhere();
								if (R.Next() < 0.7) Stand(B::Kit7, PX, PY, 0.8, 1.0, 1.6, 1.0, 1.6, 0.85, 1.1, 0.0);
								Elsewhere();
								if (R.Next() < 0.1) Stand(B::Kit5, PX, PY, 0.6, 0.8, 1.2, 0.8, 1.3, 0.95, 1.05, 0.0);
								continue;
							default: break;
							}
							if (S.Crop == Plantation)
							{
								Rows(S, X0, Y0, Cell, R, [&](double AX, double AY, double Way) { Palm(AX, AY, Way); });
								Elsewhere();
								if (R.Next() < 0.12) Stand(B::Kit7, PX, PY, 0.8, 0.8, 1.3, 0.8, 1.3, 0.95, 1.05, 0.0);
								continue;
							}
							if (S.Crop == Strand)
							{
								if (R.Next() < 0.15) Palm(X, Y, Yaw);
								Elsewhere();
								if (R.Next() < 0.35) Stand(B::Kit7, PX, PY, 0.8, 0.9, 1.5, 0.8, 1.4, 0.9, 1.1, 0.05);
								continue;
							}
							// the grove: palms some 130 a hectare, in clumps and gaps (two or three together, each leaning its own
							// way), scrub and a broadleaf tree here and there under them
							const double Clump = Noise(X / 34.0 + 0.4, Y / 34.0 - 2.2, Seed ^ 0xC1C);
							if (R.Next() < std::clamp(0.78 + 0.9 * Clump, 0.12, 1.0)) Palm(X, Y, Yaw);
							Elsewhere();
							if (Clump > 0.2 && R.Next() < 0.5)
							{
								const double Turn = R.Range(-1.2, 1.2);
								Palm(PX, PY, Yaw + Turn);
							}
							Elsewhere();
							if (R.Next() < 0.4) Stand(B::Kit7, PX, PY, 0.8, 0.9, 1.6, 0.8, 1.5, 0.9, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.03) Stand(B::Kit0, PX, PY, 0.8, 0.5, 0.8, 0.5, 0.8, 0.95, 1.05, 0.0);
							continue;
						}
						// ---- the jungle
						const double Warm = R.Range(-0.04, 0.06);
						switch (S.Kind)
						{
						case KVillage:
						case KYard:
							// bananas and young palms round the houses, an old palm or two
							if (R.Next() < 0.35) Stand(B::Kit5, X, Y, 0.6, 0.8, 1.25, 0.8, 1.3, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.12) Stand(B::Kit4, PX, PY, 0.5, 0.8, 1.1, 0.8, 1.15, 0.9, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.06) Stand(B::Kit2, PX, PY, 0.5, 0.85, 1.1, 0.85, 1.15, 0.9, 1.1, 0.0);
							continue;
						case KMarsh:
							for (int K = 0; K < 2; ++K)
							{
								Elsewhere();
								Stand(B::Kit11, PX, PY, 0.6, 0.8, 1.2, 0.8, 1.2, 0.9, 1.1, 0.0);
							}
							if (R.Next() < 0.04) Stand(B::Kit3, X, Y, 0.5, 0.85, 1.05, 0.7, 1.0, 0.9, 1.1, 0.0);
							continue;
						case KWorks: continue;
						case KWoods:
						{
							// the rainforest: a few emergents and the canopy's big crowns (25-36 m), slim sub-canopy trees,
							// bananas and young palms; at the edge a wall of undergrowth and small trees
							const bool bEdge = S.Edge < 10.0, bDeep = Outside(X, Y) > 70.0;
							const double U = R.Next();
							if (U < 0.1) Stand(B::Kit1, X, Y, 1.6, 0.78, 1.1, 0.8, 1.1, 0.85, 1.12, Warm);
							else if (U < 0.42) Stand(B::Kit0, X, Y, 1.0, 1.3, 1.75, 1.35, 1.8, 0.85, 1.12, Warm);
							// (deep in the band, seen only from afar: the canopy and its sub-canopy alone)
							Elsewhere();
							if (R.Next() < (bEdge ? 1.0 : bDeep ? 0.35 : 0.75))
							{
								if (bEdge) Stand(B::Kit0, PX, PY, 0.5, 0.3, 0.6, 0.32, 0.65, 0.85, 1.1, 0.0);
								else Stand(B::Kit0, PX, PY, 0.5, 0.42, 0.85, 0.45, 0.95, 0.85, 1.1, 0.0);
							}
							if (bDeep && !bEdge) continue;
							// the understory: thickets, bananas, undergrowth, young palms; at the edge a wall of thickets shoulder to shoulder
							// from the ground to the small trees' crowns
							if (bEdge)
								for (int K = 0; K < 3; ++K)
								{
									Elsewhere();
									Stand(B::Kit15, PX, PY, 0.5, 0.9, 1.45, 0.9, 1.5, 0.85, 1.12, Warm);
								}
							else
							{
								Elsewhere();
								if (R.Next() < 0.3) Stand(B::Kit15, PX, PY, 0.5, 0.7, 1.1, 0.7, 1.2, 0.85, 1.12, Warm);
							}
							Elsewhere();
							if (R.Next() < (bEdge ? 0.4 : 0.35)) Stand(B::Kit5, PX, PY, 0.5, 0.8, 1.35, 0.85, 1.5, 0.85, 1.05, 0.0);
							Elsewhere();
							if (R.Next() < (bEdge ? 0.6 : 0.8)) Stand(B::Kit7, PX, PY, 0.5, 1.2, 2.2, 1.2, 2.4, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.16) Stand(B::Kit4, PX, PY, 0.5, 0.35, 0.55, 0.35, 0.6, 0.85, 0.95, 0.0);
							if (bMainland)
							{
								Elsewhere();
								if (R.Next() < 0.04) Stand(B::Kit13, PX, PY, 1.2, 0.8, 1.2, 0.8, 1.2, 0.95, 1.05, 0.0);
							}
							continue;
						}
						default: break;
						}
						const int C = S.Crop >= 0 ? S.Crop : Kunai;
						switch (C)
						{
						case Plantation:
						{
							Rows(S, X0, Y0, Cell, R, [&](double AX, double AY, double Way) {
								const double Pick = R.Next();
								// (each its own height and lean: a wider palm the lower leans the farther)
								const double Sc = R.Range(0.82, 1.24);
								const double Sz = R.Range(0.64, 1.02);
								const double Lf = R.Range(0.9, 1.08);
								if (Pick < 0.7) Put(H, B::Kit3, AX, AY, 0.5, Way - LeanTall, Sc, Sz, Leaf(Lf, 0.02));
								else Put(H, B::Kit2, AX, AY, 0.5, Way - LeanPalm, Sc, Sz * 1.15, Leaf(Lf, 0.02));
							});
							Elsewhere();
							if (R.Next() < 0.05) Stand(B::Kit7, PX, PY, 0.5, 0.8, 1.2, 0.7, 1.1, 0.9, 1.1, 0.0);
							break;
						}
						case Paddy:
							if (S.Edge < 6.0 && R.Next() < 0.12) Stand(B::Kit2, X, Y, 0.5, 0.85, 1.1, 0.85, 1.1, 0.9, 1.1, 0.0);
							break;
						case Ridge:
							if (R.Next() < 0.25) Tuft(B::Kit6, X, Y, 0.6, 0.7, 1.0, 0.3, 0.42, Tint(1.02 + 0.1 * Dry, 1.0, 0.9));
							Elsewhere();
							if (R.Next() < 0.05) Tuft(B::Kit12, PX, PY, 0.9, 0.6, 1.4, 0.6, 1.2, Tint(1.12, 0.92, 0.8));
							Elsewhere();
							if (R.Next() < 0.012) Stand(B::Kit0, PX, PY, 0.8, 0.7, 1.1, 0.7, 1.1, 0.85, 1.12, Warm);
							break;
						case Garden:
							if (R.Next() < 0.45) Stand(B::Kit5, X, Y, 0.5, 0.8, 1.2, 0.8, 1.3, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.1) Stand(B::Kit4, PX, PY, 0.5, 0.8, 1.1, 0.8, 1.15, 0.9, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.25) Tuft(B::Kit6, PX, PY, 0.6, 0.7, 1.0, 0.4, 0.55, Tint(1.0, 1.0, 0.9));
							break;
						default:
							// kunai: clumps standing above the sea of grass in patches, none on its paths or where it thins;
							// now and then a thicket (a tree, undergrowth, a banana gone wild) or a wild palm
							if (Burnt(S.Key, X, Y)) break;
							{
								const double Clumps = Noise(X / 22.0 - 1.4, Y / 22.0 + 3.3, Seed ^ 0xC14);
								const double Want = (Clumps > -0.05 ? 0.9 : 0.22) * Smooth(-0.45, -0.15, Thin(X, Y)) * (1.0 - Trod(X, Y));
								if (R.Next() < Want) Tuft(B::Kit6, X, Y, 0.6, 1.15, 1.7, 0.4, 0.58, Tint(1.0 + 0.1 * Dry, 1.0, 0.94));
							}
							if (R.Next() < 0.008)
							{
								Stand(B::Kit0, X, Y, 0.8, 0.55, 0.95, 0.6, 1.0, 0.85, 1.12, Warm);
								for (int K = 0; K < 3; ++K)
								{
									const double A = R.Range(0.0, 2.0 * Pi);
									const double D = R.Range(2.0, 5.0);
									Stand(K == 2 ? B::Kit5 : B::Kit7, X + D * std::cos(A), Y + D * std::sin(A), 0.5, 1.2, 2.0, 1.2, 2.0, 0.85, 1.1, 0.0);
								}
							}
							Elsewhere();
							if (R.Next() < 0.01) Stand(B::Kit2, PX, PY, 0.5, 0.85, 1.1, 0.8, 1.1, 0.9, 1.1, 0.0);
							break;
						}
					}
			}

			/** A plantation's rows in a cell: the parcel's own lattice, rows 8.5 m apart and palms 8 m along them, every
			 * point of it inside the cell (each belongs to one cell); the palms all leaning one way, the parcel's. */
			template <typename F>
			void Rows(const Spot& S, double X0, double Y0, double Cell, Rng& R, F Palm) const
			{
				const double Ca = std::cos(S.Angle), Sa = std::sin(S.Angle);
				const double CX = S.X - (S.Along * Ca - S.Across * Sa), CY = S.Y - (S.Along * Sa + S.Across * Ca);
				const double Along0 = 8.0 * Unit(Hash(S.Key, 0xA0)), Across0 = 8.5 * Unit(Hash(S.Key, 0xA1));
				const double Way = 2.0 * Pi * Unit(Hash(S.Key, 0xA2));
				double AMin = 1e18, AMax = -1e18, BMin = 1e18, BMax = -1e18;
				for (int K = 0; K < 4; ++K)
				{
					const double QX = X0 + ((K & 1) ? Cell : 0.0) - CX, QY = Y0 + ((K & 2) ? Cell : 0.0) - CY;
					const double A = QX * Ca + QY * Sa, Bq = -QX * Sa + QY * Ca;
					AMin = std::min(AMin, A);
					AMax = std::max(AMax, A);
					BMin = std::min(BMin, Bq);
					BMax = std::max(BMax, Bq);
				}
				for (int I = static_cast<int>(std::floor((AMin - Along0) / 8.0)); I <= static_cast<int>(std::ceil((AMax - Along0) / 8.0)); ++I)
					for (int J = static_cast<int>(std::floor((BMin - Across0) / 8.5)); J <= static_cast<int>(std::ceil((BMax - Across0) / 8.5)); ++J)
					{
						const double A = Along0 + I * 8.0, Bq = Across0 + J * 8.5;
						const double PX = CX + A * Ca - Bq * Sa, PY = CY + A * Sa + Bq * Ca;
						if (PX < X0 || PX >= X0 + Cell || PY < Y0 || PY >= Y0 + Cell) continue;
						const double Gone = R.Next(), Tilt = R.Range(-0.45, 0.45);
						if (Gone < 0.04) continue;
						Palm(PX, PY, Way + Tilt);
					}
			}

			// ------------------------------------------------------------------ the effects

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "water" || Ground == "blown") return 3;
				if (Ground == "woods" || Ground == "forest") return 2;
				if (Ground == "marsh") return 4;
				if (Ground == "buildings" || Ground == "suburb" || Ground == "industry" || Ground == "bridge") return 1;
				if (bIsland) return 5;
				// (the jungle's tracks and clearings mud in the wet season)
				if (Dry < 0.5 && (Ground == "road" || Ground == "open" || Ground == "field")) return 4;
				return Else == 5 ? 0 : Else;
			}
			const char* Earth() const override { return bIsland ? "sand" : "laterite"; }

		private:
			bool bIsland = false;
			bool bMainland = false, bClay = false;
			double Dry = 0.5;
			double SeaZ = 0.0;
			double Wf = 0.0, Df = 0.0;
		};
	}

	std::unique_ptr<Region> MakeTropic(const LandField::Field& F, uint64_t S, bool bIsland) { return std::make_unique<Tropic>(F, S, bIsland); }
}
