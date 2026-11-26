#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

/**
 * The jungle and the Pacific islands (one region, two looks: Which() by bIsland).
 *
 * The jungle (Guadalcanal, Kokoda and Buna, Burma; 1942-45): a wall of green with no horizon. The rules' woods are lowland
 * rainforest: a closed canopy from fewer, bigger trees (the kit's giants and its canopy trees scaled up to 22-36 m), slim
 * sub-canopy trees under them, bananas and young palms, and round the eye the floor's own growth (ferns and gingers,
 * saplings and rattan, leaf litter) as clutter; thickets through it, and at its edge a wall of them shoulder to shoulder
 * from the ground up, columns of climbers and small trees under the big crowns. Pale trunks, very different
 * thicknesses, lianas hanging in loops. The rules' open land is kunai grass 1-2 m, tan-green, in clumps and patches
 * over red laterite, with trodden paths through it and burnt patches; its fields are coconut plantations in straight
 * rows of tall palms each its own height and lean, over short grass (rice paddies on the Asian mainland); its rough the
 * open grassy ridges. Tracks of red or yellow-brown mud, their edges broken by the grass, grass on the hump, water in
 * the ruts. Villages of houses on stilts among bananas and palms, tin roofs patchy with rust. Beyond the field the
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
			KBamboo = 13,   // Bamboo on the Asian mainland (Burma, Malaya); elsewhere in the jungle a fallen log, built whole
			KStump = 14,    // CoconutPalmTall again, its crown shot away (KitLook cuts it): the islands' snapped palms; the
				               // jungle's curtain of climbers at the woods' edge, built whole
			KThicket = 15   // the jungle's thickets; an island's pandanus (built whole in KitLook: the kit has neither, and
				               // Boulder's small file carries the slot, as for the others built whole)
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
		void Ribbon(Writer& W, const std::vector<V3>& P, const std::vector<double>& Wide, const V3& Across, const Tone& Foot, const Tone& Tip, double Lift = 0.6)
		{
			if (P.size() < 2) return;
			const V3 A = Norm(Across);
			// (each face its own points, its normal leaning out of its own side, as Writer::Blade: a face seen from behind
			// is lit as its front, so a back face given the front's normal drew near black: the kunai's dark stems)
			std::vector<uint32_t> L, R, BL, BR;
			for (size_t K = 0; K < P.size(); ++K)
			{
				const V3 Up = Norm(P[std::min(K + 1, P.size() - 1)] - P[K > 0 ? K - 1 : 0]);
				V3 N = Norm(Cross(A, Up));
				N.Z = std::fabs(N.Z) + Lift;
				N = Norm(N);
				const Tone C = Mixed(Foot, Tip, static_cast<double>(K) / static_cast<double>(P.size() - 1));
				const double H = 0.5 * Wide[K];
				L.push_back(W.Vert(P[K].X - A.X * H, P[K].Y - A.Y * H, P[K].Z - A.Z * H, N.X, N.Y, N.Z, C));
				R.push_back(W.Vert(P[K].X + A.X * H, P[K].Y + A.Y * H, P[K].Z + A.Z * H, N.X, N.Y, N.Z, C));
				BL.push_back(W.Vert(P[K].X - A.X * H, P[K].Y - A.Y * H, P[K].Z - A.Z * H, -N.X, -N.Y, N.Z, C));
				BR.push_back(W.Vert(P[K].X + A.X * H, P[K].Y + A.Y * H, P[K].Z + A.Z * H, -N.X, -N.Y, N.Z, C));
			}
			for (size_t K = 0; K + 1 < P.size(); ++K)
			{
				W.Tri(L[K], R[K], R[K + 1]);
				W.Tri(BR[K], BL[K], BR[K + 1]);
				if (Wide[K + 1] > 1e-6)
				{
					W.Tri(L[K], R[K + 1], L[K + 1]);
					W.Tri(BR[K + 1], BL[K], BL[K + 1]);
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

		/** A closed lumpy ball about C (RX and RY across, RZ high; Rings from top to foot, Round about), lit Lit on top and Under
		 * beneath: the core of a mass of leaves, or one of a far thicket's few. */
		void Blob(Writer& W, const V3& C, double RX, double RY, double RZ, int Rings, int Round, uint64_t Key, const Tone& Lit, const Tone& Under)
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
					Id.push_back(W.Vert(C.X + RX * Push * X, C.Y + RY * Push * Y, C.Z + RZ * Push * Z, X, Y, Z, Cv));
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
		void Leafy(Writer& W, const V3& C, double RX, double RY, double RZ, int Leaves, uint64_t Key, const Tone& Lit, const Tone& Under)
		{
			Blob(W, C, 0.7 * RX, 0.7 * RY, 0.7 * RZ, 5, 7, Key, Mixed(Under, Lit, 0.55), Under);
			Rng R(Key ^ 0x1EAF);
			for (int L = 0; L < Leaves; ++L)
			{
				const double Th = R.Range(0.0, 2.0 * Pi), Z = R.Range(-0.5, 0.97), Long = R.Range(0.45, 0.8), Wide = R.Range(0.2, 0.34), Pale = R.Next(), Spin = R.Range(-0.7, 0.7);
				const double S = std::sqrt(std::max(0.0, 1.0 - Z * Z));
				const V3 N{ S * std::cos(Th), S * std::sin(Th), Z };
				const V3 Root = C + V3{ N.X * RX * 0.7, N.Y * RY * 0.7, N.Z * RZ * 0.7 };
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
				bPlumeTrial = F.Tried("kunai");
				bCanopy = !bIsland && F.Tried("canopy");
				bSand = bIsland && F.Tried("islandsand");
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
				// The patches' noises read once onto an 8 m grid over the field and 300 m round it (the paint, the clutter and
				// the planting ask them many times; four noises each is too dear), straight from the noise beyond it.
				GX0 = -300.0;
				GY0 = -300.0;
				GNX = static_cast<int>((Wf + 600.0) / GStep) + 2;
				GNY = static_cast<int>((Df + 600.0) / GStep) + 2;
				Grid.resize(static_cast<size_t>(Chans) * static_cast<size_t>(GNX) * static_cast<size_t>(GNY));
				for (int J = 0; J < GNY; ++J)
					for (int I = 0; I < GNX; ++I)
						for (int C = 0; C < Chans; ++C)
							Grid[(static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)) * Chans + static_cast<size_t>(C)] = static_cast<float>(Chan(GX0 + I * GStep, GY0 + J * GStep, C));
				}
			Look Which() const override { return bIsland ? Look::Island : Look::Jungle; }
			// (on trial "canopy" the jungle a second step down the budget's ladder: draw distances in the game only)
			int DrawStep() const override { return bCanopy ? 2 : Region::DrawStep(); }

			// ------------------------------------------------------------------ noise with no lattice showing

			/** A noise turned and bent by another, at two scales, so neither its square cells nor diamonds show (about -1 to 1,
			 * most within a third of 0); Scale in metres. */
			double Organic(double X, double Y, double Scale, uint64_t K) const
			{
				const double Bend = 0.5 * Scale, F = 1.0 / (1.9 * Scale);
				const double WX = Bend * Noise(X * F + 3.1, Y * F - 0.7, K ^ 0x0A1), WY = Bend * Noise(X * F - 2.3, Y * F + 4.4, K ^ 0x0A2);
				const double U = 0.8 * (X + WX) - 0.6 * (Y + WY), V = 0.6 * (X + WX) + 0.8 * (Y + WY);
				return 0.72 * Noise(U / Scale, V / Scale, K) + 0.4 * Noise(V / (0.55 * Scale) + 1.7, -U / (0.55 * Scale), K ^ 0x0A3);
			}
			/** The grid's channels. Both looks: 0 the parcels' ragged edges. The jungle: 1 the kunai's thin spots, 2 its burns, 3
			 * its paths' line, 4 where it has paths. The islands: 1 the ground cover's drifts, 2 the sand's dry crests and grey
			 * bands, 3 the litter under the palms. */
			double Chan(double X, double Y, int C) const
			{
				if (C == 0) return Organic(X, Y, 55.0, Seed ^ 0xE01);
				if (C == 5) return Organic(X, Y, 24.0, Seed ^ 0xE09);
				if (C == 6) return Organic(X, Y, 62.0, Seed ^ 0xE0A);
				if (bIsland) return C == 1 ? Organic(X, Y, 22.0, Seed ^ 0xE02) : C == 2 ? Organic(X, Y, 70.0, Seed ^ 0xE03) : C == 3 ? Organic(X, Y, 26.0, Seed ^ 0xE04) : Organic(X, Y, 150.0, Seed ^ 0xC1D);
				return C == 1 ? Organic(X, Y, 28.0, Seed ^ 0xE05) : C == 2 ? Organic(X, Y, 42.0, Seed ^ 0xE06) : C == 3 ? Organic(X, Y, 80.0, Seed ^ 0xE07) : Organic(X, Y, 220.0, Seed ^ 0xE08);
			}
			/** The ground's own breakup at 5-40 m, carried by its colour where the clutter has dropped out (the general's
			 * camera sees the oblique): soft drifts lighter and warmer, darker and cooler, from the grid's two breakup
			 * channels (warped noise, its finest some 13 m and its broadest 60 m) and, finest, a noise of 11 m cells turned
			 * and bent by them, read at each point (nothing finer, as the 4.2 m vertices would show it in cells). */
			Tone Breakup(const Tone& T, double X, double Y, double Amount) const
			{
				const double Fine = Read(X, Y, 5), Broad = Read(X, Y, 6);
				const double U = 0.8 * X - 0.6 * Y + 9.0 * Broad, V = 0.6 * X + 0.8 * Y + 9.0 * Fine;
				const double Grain = Noise(U / 11.0 + 0.4, V / 11.0 - 0.9, Seed ^ 0xE0B);
				return Varied(T, 1.0 + Amount * (0.2 * Broad - 0.3 * Fine + 0.18 * Grain), Amount * (0.08 * Broad - 0.06 * Fine + 0.04 * Grain));
			}
			/** A channel here, read between the grid's points; beyond the grid straight from the noise (the jungle's thin
			 * spots, burns and paths left out there: the haze has them). */
			double Read(double X, double Y, int C) const
			{
				const double U = (X - GX0) / GStep, V = (Y - GY0) / GStep;
				if (!(U >= 0.0 && V >= 0.0 && U < GNX - 1.0 && V < GNY - 1.0))
				{
					if (C == 0 || bIsland) return Chan(X, Y, C);
					return C >= 5 ? 0.0 : C == 2 || C == 4 ? -1.0 : 1.0;
				}
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = (static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)) * Chans + static_cast<size_t>(C);
				const size_t Row = static_cast<size_t>(GNX) * Chans;
				const double A = Grid[K], B = Grid[K + Chans], Cq = Grid[K + Row], D = Grid[K + Row + Chans];
				const double Top = A + (B - A) * FU, Bottom = Cq + (D - Cq) * FU;
				return Top + (Bottom - Top) * FV;
			}

			/** How far a parcel's own crop shows: 0 at its edge (where the kunai, or an island's grove floor, runs on from the
			 * next parcel, the same both sides) to 1 inside it; the edge ragged, so no parcel's straight side shows. */
			double Inward(const Spot& S) const { return Smooth(0.0, 1.0, (S.Edge - 16.0 - 42.0 * Read(S.X, S.Y, 0)) / 12.0); }
			/** The band's woods eaten back raggedly from the field's edge, kunai there, so no straight wall frames the battle. */
			bool Eaten(double X, double Y) const
			{
				if (bIsland) return false;
				const double D = Outside(X, Y);
				return D > 0.0 && D < 25.0 + 60.0 * Read(X, Y, 0);
			}
			/** The kunai's thin spots (0 to 1), its trodden paths (0 to 1, a metre wide), its burns (0 to 1, black ash). */
			double ThinAt(double X, double Y) const { return Smooth(-0.38, -0.55, Read(X, Y, 1)); }
			/** On trial "canopy": the woods gathered into clumps (1 a clump's heart, 0.25-0.75 its rim, under 0.25 a
			 * clearing: kClearing), from the grid's 62 m channel 6, the broad breakup that already lightens the floor where
			 * it runs high (Breakup), so the clearings fall where the floor is lighter; no new channel, no draw. Patches 25-60
			 * m across. Held at full heart past 70 m out (the band's wall, seen from afar, closes the view) and in the woods'
			 * outer 12 m, easing to free at 25 m in, so no clearing opens within some 20 m of the woods' edge: a small wood
			 * keeps its body, never a ring of trees round a hole, and its face stands whole (its roof thinned there instead:
			 * EdgeFace). The thresholds from a quantile count of the woods' ground within 70 m of the field on a 2 m raster
			 * (Panama City; San Jose's the same field): kClumpLo at the 44th centile of -channel 6, kClumpHi at the 72nd, so the
			 * woods there, with the holds, are 58% heart (0.75 and over), 12% rim and 30% clearing (under 0.25); clearings
			 * median 28 m across, 0.4% of their area in slivers under 9 m (25 Nov 2026). */
			double ClumpAt(double X, double Y, double Edge) const
			{
				const double Out = Outside(X, Y);
				if (Out > 70.0) return 1.0;
				double H = Smooth(kClumpLo, kClumpHi, -Read(X, Y, 6));
				H = std::max(H, Smooth(40.0, 70.0, Out));
				return std::max(H, Smooth(25.0, 12.0, Edge));
			}
			/** On trial "canopy": islands of trees in the open kunai (0 to 1; an island where it is 0.5 or more), the same
			 * channel's far tail on the heart's side, its edge sharp (Isle 0.5 at the 95th centile of the open kunai's ground:
			 * 4.9% of it, some 230 islands, median 26 m across); its outer tail (0.02 and more) a few metres round each. */
			double IsleAt(double X, double Y) const { return Smooth(kIsleLo, kIsleHi, -Read(X, Y, 6)); }
			/** On trial "canopy": a yard's or a garden's groves of bananas and palms (0 to 1), the grid's 62 m channel 6 (the
			 * woods' clumps' own, so a grove is 25-60 m across, not the spacing of two palms), a grove (0.5 and more) on 35% of
			 * the yards' ground and the mean kGroveMean. */
			double YardGrove(double X, double Y) const { return Smooth(kYardLo, kYardHi, -Read(X, Y, 6)); }
			/** On trial "canopy": the floor's shade (and its clutter's) under the canopy, 1 in a clearing to kCanopyShade / kLitter
			 * at a heart (a ramp some 12 m wide over the rim), so a heart's floor is kCanopyShade of a clearing's; a yard's under
			 * its groves to kYardShade. 1 elsewhere and off the trial. */
			double CanopyShade(const Spot& S) const
			{
				if (!bCanopy) return 1.0;
				if (S.Kind == KWoods) return Eaten(S.X, S.Y) ? 1.0 : Lerp(1.0, kCanopyShade / kLitter, Smooth(0.25, 0.75, ClumpAt(S.X, S.Y, S.Edge)));
				if (S.Kind == KVillage || S.Kind == KYard) return Lerp(1.0, kYardShade, YardGrove(S.X, S.Y));
				return 1.0;
			}
			double TrodAt(double X, double Y) const
			{
				const double Mask = Smooth(-0.15, 0.0, Read(X, Y, 4));
				return Mask > 0.0 ? Mask * Smooth(1.4, 0.5, 75.0 * std::fabs(Read(X, Y, 3))) : 0.0;
			}
			/** An island's groves: 1 inside one, 0 on the open sand between them (some four in ten of the grove's land). */
			double GroveAt(double X, double Y) const { return Smooth(-0.12, 0.08, Read(X, Y, 4)); }
			double BurnAt(double X, double Y) const { return Smooth(0.66, 0.76, Read(X, Y, 2) + 0.6 * Read(X, Y, 4)); } // (some 3 in 100, in a few areas)

			// ------------------------------------------------------------------ the land

			double Outside(double X, double Y) const
			{
				const double DX = std::max({ 0.0, -X, X - Wf }), DY = std::max({ 0.0, -Y, Y - Df });
				return std::sqrt(DX * DX + DY * DY);
			}
			/** An island's shore: metres inland of the waterline (negative out on the reef). The islet is its own shape, long
			 * and narrow along the field's length: a middle line that tilts and wanders across the field's and a half-width
			 * that holds the field's sides (never nearer them than some 60 m: the recording's ground stands up to 10 m over
			 * the sea and falls to the beach), its coast bent in bays and points at two scales, its west end a rounded tip
			 * some 250 m out and its east a spit trailing 1.8 km to a point; the sea at the corners. */
			double Shore(double X, double Y) const
			{
				const double D = Outside(X, Y);
				if (D <= 0.0) return 1000.0;
				const double Mid = 0.5 * Df + 0.05 * (X - 0.5 * Wf) + 130.0 * Noise(X / 1100.0 + 0.3, 0.7, Seed ^ 0x5E0D);
				double Half = 0.5 * Df + 120.0;
				const double West = -X - 10.0, East = X - Wf - 40.0;
				if (West > 0.0) Half *= std::sqrt(std::max(0.0, 1.0 - (West / 240.0) * (West / 240.0)));
				if (East > 0.0) Half *= std::pow(std::max(0.0, 1.0 - East / 1800.0), 1.6);
				const double U = 0.8 * X - 0.6 * Y, V = 0.6 * X + 0.8 * Y;
				const double Bend = 95.0 * Noise(U / 420.0 + 0.4, V / 420.0, Seed ^ 0x5E0A) + 40.0 * Noise(V / 110.0, -U / 110.0 + 2.2, Seed ^ 0x5E0C);
				const double W = Half + Bend - std::fabs(Y - Mid);
				// (held off the field by a soft floor, 60 m less the distance out: the larger of the two, its corner rounded)
				const double Hold = 60.0 - D;
				return 0.5 * (W + Hold + std::sqrt((W - Hold) * (W - Hold) + 900.0));
			}
			/** The reef flat's width out from the waterline to its surf line (some 55-190 m: the drop to deep water in sight
			 * from the shore). */
			double Reef(double X, double Y) const { return std::max(55.0, 120.0 + 70.0 * Noise(X / 700.0 + 0.2, Y / 700.0, Seed ^ 0x5E0B)); }

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
			// (the jungle's woods beyond the field some two grounds in five, in blocks, so no solid wall frames the battle; the
			// kunai between)
			double WoodsEdge() const override { return bIsland ? 0.95 : 0.08; }

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

			/** The kunai, the jungle's open cover and every open parcel's at its edge: a sea of tan-green grass 1-2 m, the
			 * thatch matted under it; thinning in spots to red earth; trodden paths through it; burns black ash. */
			Tone KunaiGround(const Spot& S) const
			{
				const Tone Red = bClay ? Rgb(150, 116, 70) : Rgb(158, 92, 56);
				Tone T = Mixed(Rgb(108, 108, 60), Rgb(146, 136, 84), Clamp01(0.2 + 0.45 * Dry + 0.3 * S.N1));
				// (the grass in drifts some 10-40 m across, darker and greener where it stands thick, paler straw between, so it
				// never reads as a lawn from above when the clutter has gone; smooth, never a step, as a track mark's edge takes
				// the ground's colour a hair off its own point)
				T = Mixed(T, Rgb(84, 86, 48), 0.4 * Smooth(-0.1, 0.6, Read(S.X, S.Y, 5)));
				T = Breakup(T, S.X, S.Y, 1.0);
				T = Mixed(T, Mixed(Rgb(118, 110, 64), Red, Clamp01(0.4 + 0.4 * S.N2)), 0.65 * ThinAt(S.X, S.Y));
				T = Mixed(T, Mixed(Rgb(150, 104, 70), Rgb(146, 134, 92), Clamp01(0.4 + 0.4 * S.N1)), 0.85 * TrodAt(S.X, S.Y));
				T = Mixed(T, Mixed(Rgb(38, 36, 33), Rgb(70, 67, 60), Clamp01(0.35 + 0.5 * S.N2)), BurnAt(S.X, S.Y));
				// (on trial "canopy" the shade under an island of trees in it)
				if (bCanopy) T = Varied(T, Lerp(1.0, kGroveShade, Smooth(0.2, 0.7, IsleAt(S.X, S.Y))), 0.0);
				return Varied(T, 1.0 + 0.04 * S.Broad, 0.01);
			}

			/** A track's broken edge (the grass coming in over it) and the grass on its hump between the ruts. */
			double Rag(double X, double Y) const { return Noise(X / 5.0 + 0.3, Y / 5.0 - 1.1, Seed ^ 0x7A9); }
			double Hump(double X, double Y) const { return Noise(X / 7.0 - 0.8, Y / 7.0 + 0.2, Seed ^ 0x7AA); }

			Tone Track(const Spot& S) const
			{
				// red laterite (yellow-brown clay in Papua), redder and yellower along it: the ruts wet and dark, water
				// standing in them and across the floor in its low places; grass on the hump between the ruts here and
				// there; its broken edge fraying straight into the kunai round it (no verge of its own: from above two green
				// stripes read as a race track)
				const double Off = std::fabs(S.Lane + S.LaneHalf);
				const Tone Earth = bClay ? Mixed(Rgb(146, 112, 66), Rgb(162, 132, 86), Clamp01(0.5 + 0.7 * S.N1)) : Mixed(Rgb(155, 89, 54), Rgb(166, 112, 68), Clamp01(0.4 + 0.7 * S.N1));
				const Tone Wet = bClay ? Rgb(106, 84, 52) : Rgb(108, 62, 42);
				const Tone Water = Rgb(124, 118, 98), Grass = Rgb(112, 118, 62);
				const double Rut = Smooth(0.5, 0.12, std::fabs(Off - 0.9));
				Tone T = Mixed(Earth, Wet, 0.2 + 0.3 * (1.0 - Dry) + 0.4 * Rut);
				const double Pool = std::max(Rut * Smooth(0.0, 0.3, S.N2 + 0.25 * (1.0 - Dry)), Smooth(0.38, 0.52, S.N1 - 0.25 * Dry) * Smooth(0.4, 0.0, S.N2));
				T = Mixed(T, Water, 0.9 * Pool);
				const double OnHump = 0.75 * Smooth(0.5, 0.2, Off) * Smooth(0.0, 0.35, Hump(S.X, S.Y));
				T = Mixed(T, Varied(Mixed(Grass, Rgb(140, 132, 80), 0.4), 1.0 + 0.1 * S.N2, 0.0), OnHump * (1.0 - Pool));
				T = Varied(T, 1.0 + 0.06 * S.N2, 0.0);
				const double Edge = Smooth(-0.9, 0.15, S.Lane + 1.3 * Rag(S.X, S.Y));
				return Mixed(T, KunaiGround(S), Edge);
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
					const Tone Yard = Varied(Breakup(Mixed(Garden_, Swept, Bare), S.X, S.Y, 0.7), 1.0, 0.02);
					// (on trial "canopy" the shade of its groves of bananas and palms on it)
					return bCanopy ? Varied(Yard, CanopyShade(S), 0.0) : Yard;
				}
				case KWater: return Varied(Rgb(108, 100, 70), 1.0 + 0.04 * S.N1, 0.0); // (brown-green river water)
				case KMarsh: return Varied(Mixed(Rgb(56, 48, 38), Rgb(72, 84, 42), 0.4 + 0.4 * S.N1), 1.0 + 0.06 * S.N2, 0.0);
				case KWorks: return Varied(Mixed(Rgb(150, 86, 54), Rgb(112, 70, 46), 0.4 + 0.3 * S.N2), 1.0, 0.0);
				case KWoods:
				{
					if (Eaten(S.X, S.Y)) return KunaiGround(S);
					// the forest floor: leaf litter brown, rust and yellow in drifts, damp patches, moss (dim under the canopy)
					const Tone Litter = Rgb(110, 88, 58), Rust = Rgb(122, 84, 52), Yellow = Rgb(124, 106, 64), Damp = Rgb(80, 68, 48), Moss = Rgb(80, 96, 48);
					Tone T = Mixed(Litter, S.N1 > 0.0 ? Rust : Yellow, Clamp01(0.6 * std::fabs(S.N1)));
					const double Fine = Read(S.X, S.Y, 5);
					T = Mixed(T, Damp, 0.35 * Smooth(0.1, 0.5, -Fine));
					T = Mixed(T, Moss, 0.35 * Smooth(0.15, 0.55, Fine));
					const Tone Floor = Varied(Breakup(T, S.X, S.Y, 0.8), 1.0 + 0.05 * S.Broad, 0.02);
					if (!bCanopy) return Floor;
					// (on trial "canopy": a clearing's floor the kunai's, the litter dark in the shade under a clump's heart)
					const double Hc = ClumpAt(S.X, S.Y, S.Edge);
					return Varied(Mixed(Floor, KunaiGround(S), Smooth(0.35, 0.15, Hc)), CanopyShade(S), 0.0);
				}
				default: break;
				}
				// the open land: the kunai, and a parcel's own crop showing inside its ragged edge
				const Tone K = KunaiGround(S);
				if (C == Kunai) return K;
				const double In = Inward(S);
				if (In <= 0.0) return K;
				const Tone Red = bClay ? Rgb(150, 116, 70) : Rgb(158, 92, 56);
				switch (C)
				{
				case Ridge:
				{
					Tone T = Mixed(Rgb(136, 134, 74), Rgb(164, 150, 92), Clamp01(0.3 + 0.4 * Dry - 0.3 * Read(S.X, S.Y, 5)));
					T = Mixed(T, Red, 0.26 + 0.2 * S.N1);
					return Mixed(K, Varied(Breakup(T, S.X, S.Y, 0.8), 1.0 + 0.05 * S.Broad, 0.01), In);
				}
				case Plantation:
				{
					// short grass the cattle kept down, the kunai's own straw-green, the palms' fallen fronds and husks browning it
					// in drifts
					Tone T = Mixed(Rgb(126, 124, 64), Rgb(152, 142, 78), Clamp01(0.35 - 0.4 * Read(S.X, S.Y, 5) + 0.2 * Dry));
					T = Mixed(T, Rgb(132, 110, 72), 0.18 * Smooth(0.0, 0.7, S.N1));
					return Mixed(K, Varied(Breakup(T, S.X, S.Y, 0.8), 1.0 + 0.04 * S.Broad, 0.0), In);
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
					return Mixed(K, Varied(T, 1.0 + 0.05 * S.N2, 0.0), In);
					}
					case Garden:
					{
						Tone T = Mixed(Rgb(124, 124, 60), Red, 0.3 + 0.3 * S.N1);
						return Mixed(K, Breakup(T, S.X, S.Y, 0.8), In);
					}
					default: return K;
					}
			}

			/** The sea round an island, W metres inland of the waterline (less than nothing), in bands: the wash over wet
			 * sand, the pale turquoise shallows, the reef flat (teal, mottled with dark brown-green coral heads and weed,
			 * pale sand channels), its broken white surf line at the reef's edge, the deep blue beyond. */
			Tone Sea(double W, double Rf, double N1, double N2) const
			{
				// In bands out from the beach: wet sand under a skin of water, pale sandy shallows, the reef flat mottled with
				// sand channels, weed and dark coral heads, the white surf line at its edge, the drop to deep blue.
				const double Out = -W;
				const Tone Wash = bSand ? Rgb(146, 138, 110) : Rgb(196, 196, 174), Pale = Rgb(172, 194, 176), Flat = Rgb(124, 148, 122), Drop = Rgb(44, 100, 132), Deep = Rgb(26, 60, 106);
				Tone T = Mixed(Wash, Pale, Smooth(0.0, 12.0, Out));
				T = Mixed(T, Flat, Smooth(22.0, 20.0 + 0.45 * Rf, Out));
				const double Reefy = Smooth(18.0, 50.0, Out) * Smooth(Rf + 4.0, Rf - 10.0, Out);
				T = Mixed(T, Rgb(176, 188, 162), 0.5 * Reefy * Smooth(0.2, 0.55, -N1 - 0.3 * N2));       // (sand channels)
				T = Mixed(T, Rgb(104, 116, 80), 0.55 * Reefy * Smooth(0.0, 0.45, N1 - 0.25 * N2));       // (weed)
				T = Mixed(T, Rgb(86, 86, 64), 0.8 * Reefy * Smooth(0.1, 0.45, N2 + 0.6 * N1));           // (coral heads)
				T = Mixed(T, Rgb(96, 138, 132), 0.35 * Smooth(Rf * 0.6, Rf, Out));                       // (deeper toward the edge)
				const double Surf = Smooth(9.0 + 6.0 * N1, 0.0, std::fabs(Out - Rf - 4.0 * N2));
				T = Mixed(T, Drop, Smooth(Rf + 2.0, Rf + 12.0, Out));
				T = Mixed(T, Deep, Smooth(Rf + 12.0, Rf + 70.0, Out));
				T = Mixed(T, Rgb(232, 236, 232), 0.92 * Surf);
				return Varied(T, 1.0 + 0.03 * N1, 0.0);
			}

			Tone IslandGround(const Spot& S, int C) const
			{
				const double W = Shore(S.X, S.Y);
				if (W < 0.0) return Sea(W, Reef(S.X, S.Y), S.N1, S.N2);
				const double Crest = Read(S.X, S.Y, 2);
				// (on trial "islandsand": warm coral sand, not the white that reads as snow under palms, and its wet band at the
				// water darker, as sand is where the wash has just left it)
				const Tone Dry_ = bSand ? Mixed(Rgb(202, 178, 130), Rgb(218, 194, 148), Smooth(-0.1, 0.35, Crest))
										: Mixed(Rgb(232, 226, 206), Rgb(246, 242, 228), Smooth(-0.1, 0.35, Crest));
				const Tone Wet = bSand ? Rgb(134, 116, 86) : Rgb(186, 180, 160), Wrack = bSand ? Rgb(114, 96, 68) : Rgb(150, 134, 104);
				if (W < 32.0)
				{
					// the beach: the wet band at the water greyer, the tide's wrack of weed and husks, dry sand whiter to the crest
					Tone T = bSand ? Mixed(Wet, Dry_, Smooth(4.0, 10.0 + 3.0 * S.N1, W)) : Mixed(Wet, Dry_, Smooth(2.5, 7.0 + 3.0 * S.N1, W));
					T = Mixed(T, Wrack, 0.7 * Smooth(1.6, 0.0, std::fabs(W - 7.5 - 1.5 * S.N1)) * (0.5 + 0.5 * Smooth(-0.4, 0.4, S.N2)));
					if (S.Kind != KLane) return Breakup(T, S.X, S.Y, 0.5);
				}
				switch (S.Kind)
				{
				case KLane:
				{
					// crushed coral: white, the ruts a little greyer
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = Smooth(0.45, 0.12, std::fabs(Off - 0.9));
					Tone T = bSand ? Mixed(Rgb(218, 202, 168), Rgb(184, 166, 134), 0.6 * Rut) : Mixed(Rgb(226, 220, 202), Rgb(196, 188, 168), 0.6 * Rut);
					if (S.Lane > 0.0) T = Mixed(T, bSand ? Rgb(204, 182, 138) : Rgb(214, 206, 180), Smooth(0.2, 1.6, S.Lane));
					return Varied(T, 1.0 + 0.03 * S.N2, 0.0);
				}
				case KVillage:
				case KYard: return Varied(Mixed(bSand ? Rgb(200, 176, 134) : Rgb(214, 204, 178), Rgb(150, 160, 92), S.bNear ? 0.05 : 0.35 * Smooth(0.0, 0.6, S.N1)), 1.0 + 0.04 * S.N2, 0.0);
				case KWater: return Sea(-60.0 - 80.0 * (0.5 + 0.5 * S.N1), 1000.0, S.N1, S.N2);
				case KMarsh: return Varied(Mixed(Rgb(150, 146, 108), Rgb(96, 112, 62), 0.4 + 0.3 * S.N1), 1.0, 0.0);
				case KWorks: return Varied(bSand ? Mixed(Rgb(196, 172, 132), Rgb(164, 144, 110), 0.4 + 0.3 * S.N2) : Mixed(Rgb(212, 202, 176), Rgb(176, 166, 140), 0.4 + 0.3 * S.N2), 1.0, 0.0);
				case KWoods: return Floor(S, 1.0, 1.5, 1.2);
				default: break;
				}
				// the grove's floor (the plantation's and the strand's showing inside their ragged edges)
				const double In = Inward(S);
				if (C == Strand) return Floor(S, Lerp(1.0, 0.45, In), Lerp(1.0, 0.4, In), Lerp(1.0, 1.3, In));
				if (C == Plantation) return Floor(S, Lerp(1.0, 1.15, In), Lerp(1.0, 0.8, In), 1.0);
				return Floor(S, 1.0, 0.2 + 0.8 * GroveAt(S.X, S.Y), 1.0); // (the litter under the groves, the sand between them clean)
				}

				/** An island's sand under its palms: whiter on the dry crests, greyer in the low bands, darker in drifts of frond
				 * litter and husks, the ground cover (creepers, grass) in drifts over it; Cover, Litter and Bare scale those. */
				Tone Floor(const Spot& S, double Cover, double Litter, double Bare) const
				{
					const double Crest = Read(S.X, S.Y, 2), Drift = Read(S.X, S.Y, 1), Lit_ = Read(S.X, S.Y, 3);
					Tone T = bSand ? Mixed(Rgb(190, 168, 122), Rgb(212, 190, 144), Clamp01(Bare * Smooth(-0.1, 0.4, Crest)))
								   : Mixed(Rgb(212, 202, 174), Rgb(238, 232, 212), Clamp01(Bare * Smooth(-0.1, 0.4, Crest)));
					T = Mixed(T, bSand ? Rgb(164, 148, 116) : Rgb(178, 172, 150), 0.6 * Smooth(-0.1, -0.4, Crest));
					T = Breakup(T, S.X, S.Y, 0.6);
					const double Fine = Read(S.X, S.Y, 5);
					// (on trial "islandsand" the frond litter greyer, so the sand in the palms' shade goes grey-tan, not mustard)
					T = Mixed(T, bSand ? Mixed(Rgb(150, 138, 116), Rgb(124, 114, 98), Clamp01(0.5 + 0.8 * Fine)) : Mixed(Rgb(158, 140, 108), Rgb(132, 116, 90), Clamp01(0.5 + 0.8 * Fine)), Clamp01(0.6 * Litter * Smooth(-0.35, 0.4, Lit_)));
					const double Green = Clamp01(Cover * Smooth(0.2, 0.42, Drift));
					// (on trial "islandsand" the creepers a cooler green and thinner over the sand: in the palms' shade the old
					// yellow-green read as mustard)
					T = bSand ? Mixed(T, Mixed(Rgb(100, 124, 78), Rgb(136, 146, 104), Clamp01(0.5 - 0.9 * Fine)), 0.45 * Green)
							  : Mixed(T, Mixed(Rgb(112, 130, 62), Rgb(150, 154, 96), Clamp01(0.5 - 0.9 * Fine)), 0.6 * Green);
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
					// the grove's floor: creepers and grass in drifts over the sand (none between), husks and fronds where the
					// litter drifts under the palms, bits of coral; less of all on the strand, more in a dense grove
					const double In = S.Kind == KWoods ? 0.0 : Inward(S);
					const double Scale = S.Kind == KWoods ? 1.2 : S.Crop == Strand ? Lerp(1.0, 0.5, In) : 1.0;
					const double Cover = Smooth(0.2, 0.42, Read(S.X, S.Y, 1)) * Scale, Litter = Smooth(-0.15, 0.3, Read(S.X, S.Y, 3));
					if (Pick < 0.45) return Cover > 0.02 ? Grown{ B::Clut4, 2.4 * Cover } : Pick < 0.06 ? Grown{ B::Clut4, 0.2 } : G;
					if (Pick < 0.62) return Cover > 0.25 ? Grown{ B::Clut1, 1.3 * Cover } : G;
					if (Pick < 0.88) return Grown{ B::Clut5, (0.12 + 0.4 * Litter) * Scale };
					return Grown{ B::Stone, 0.25 };
				}
				switch (S.Kind)
				{
				case KLane:
				{
					// (grass over the track's broken edges and on its hump, the kunai tall on the verge)
					const double Edge = S.Lane + 1.3 * Rag(S.X, S.Y);
					if (Edge > 0.6) return KunaiGrow(S, Pick);
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
					if (Eaten(S.X, S.Y)) return KunaiGrow(S, Pick);
					// (on trial "canopy": a clearing the kunai's, the ferns thinner in a clump's dark heart)
					const double Hc = bCanopy ? ClumpAt(S.X, S.Y, S.Edge) : 1.0;
					if (bCanopy && Hc < kClearing) return KunaiGrow(S, Pick);
					const double Fern = bCanopy ? 1.0 - 0.4 * Smooth(0.5, 0.9, Hc) : 1.0;
					// the floor: ferns and gingers, saplings and rattan, leaf litter thick over it; a wall of it at the forest's edge
					const double Edge = Smooth(12.0, 3.0, S.Edge);
					if (Pick < 0.4) return Grown{ B::Clut1, (0.95 + 0.6 * Edge) * Fern };
					if (Pick < 0.54) return Grown{ B::Clut2, 0.45 + 0.4 * Edge };
					if (Pick < 0.96) return Grown{ B::Clut3, 1.0 };
					return Grown{ B::Stone, 0.25 };
				}
				default: break;
				}
				// the open land: the kunai, and inside a parcel's ragged edge its own crop (Pick shared between them, so each
				// stays uniform)
				const int C = S.Crop >= 0 ? S.Crop : Kunai;
				if (C == Kunai) return KunaiGrow(S, Pick);
				const double In = Inward(S);
				if (Pick >= In) return KunaiGrow(S, (Pick - In) / std::max(1e-6, 1.0 - In));
				Pick /= In;
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
				default: return KunaiGrow(S, Pick);
				}
				}

				/** The kunai's clutter: a sea of it, wide clumps shoulder to shoulder (fewer where it thins); a trodden path bare but
				 * for a tuft; a burn only charred stubble. */
				Grown KunaiGrow(const Spot& S, double Pick) const
				{
					using B = LandField::Base;
					if (BurnAt(S.X, S.Y) > 0.5) return Pick < 0.35 ? Grown{ B::Clut4, 0.5 } : Grown{};
					if (TrodAt(S.X, S.Y) > 0.45) return Pick < 0.3 ? Grown{ B::Clut4, 0.5 } : Grown{};
					return Pick < 0.9 ? Grown{ B::Clut0, 0.62 * (1.0 - 0.55 * ThinAt(S.X, S.Y)) } : Grown{ B::Clut4, 0.6 };
				}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				using B = LandField::Base;
				const int C = S.Crop >= 0 ? S.Crop : -1;
				const bool bBurnt = !bIsland && S.Kind != KWoods && S.Kind != KLane && BurnAt(S.X, S.Y) > 0.5;
				// (on trial "canopy" the floor's growth as dark as the floor it stands on, so no fern glows in the shade: 1 off it)
				const double Shade = CanopyShade(S);
				auto Size = [&P](double Sc, double Sz) {
					P.Scale = static_cast<float>(Sc);
					P.ScaleZ = static_cast<float>(Sz);
				};
				switch (P.Kind)
				{
				case B::Clut0:
				{
					// kunai: 1-2 m on the plain, short inside the ridges and the plantations' regrowth; straw in the dry
					const double Tall = S.Kind == KMarsh ? 0.9 + 0.3 * U5 : 0.85 + 0.4 * U5;
					const double Short = C == Ridge ? 0.45 + 0.25 * U5 : 0.4 + 0.35 * U5;
					const double High = (S.Kind == KOpen || S.Kind == KField || S.Kind == KRough) && (C == Ridge || C == Plantation) ? Lerp(Tall, Short, Inward(S)) : Tall;
					Size(0.85 + 0.4 * U4, High);
					const double Straw = Clamp01(Dry + 0.35 * (U6 - 0.5));
					P.Tint = S.Kind == KMarsh ? Tint(0.82, 1.0, 0.86) : Tint(0.97 + 0.08 * Straw, 1.0, 0.9 + 0.08 * U7);
					// (on trial, one clump in six its plumed twin, drawn in Poppies' slot, which the jungle never lays: KunaiClump)
					// (on trial "canopy", a fifth of all the clumps, 0.24 of the five in six that would take no plume, drawn only
					// within 30 m: the same clump in Flowers' slot, which the jungle never lays; with "kunai" or without it, the
					// same clumps: code-cost's KunaiNear)
					if (bPlumeTrial || bCanopy)
					{
						const uint64_t QX = static_cast<uint64_t>(static_cast<int64_t>(std::lround(P.X * 4.0f)));
						const uint64_t QY = static_cast<uint64_t>(static_cast<int64_t>(std::lround(P.Y * 4.0f)));
						const bool bPlume = Unit(Hash(Seed ^ 0x6B1Eull, QX, QY)) < 1.0 / 6.0;
						if (bPlume && bPlumeTrial) P.Kind = B::Poppies;
						else if (!bPlume && bCanopy && Unit(Hash(Seed ^ 0x7417ull, QX, QY)) < 0.24) P.Kind = B::Flowers;
					}
					return true;
				}
				case B::Clut1:
				{
					if (bIsland) Size(0.9 + 0.6 * U4, 0.32 + 0.22 * U5); // (an island's creepers, low)
					else Size(0.65 + 0.75 * U4, 0.7 + 0.7 * U5);
					const double L = (0.8 + 0.4 * U6) * Shade;
					P.Tint = bIsland ? Tint(1.05 * L, 1.1 * L, 0.85 * L) : Tint(0.95 * L, L, 0.9 * L);
					return true;
				}
				case B::Clut2:
				{
					Size(0.8 + 0.4 * U4, 0.65 + 0.8 * U5);
					const double L = (0.85 + 0.3 * U6) * Shade;
					P.Tint = Tint(L, L, L * (0.9 + 0.1 * U7));
					return true;
				}
				case B::Clut3:
				{
					Size(0.8 + 0.7 * U4, 1.0);
					const double L = (0.8 + 0.4 * U6) * Shade;
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
				case KGrass: return bIsland ? "Boulder" : "ElephantGrass"; // (an island's shore shrub, built whole)
				case KUnder: return bIsland ? nullptr : "Undergrowth";
				case KStilt: return "StiltHouse";
				case KStiltTin: return "StiltHouseTin";
				case KThatch: return "ThatchHouse";
				case KReeds: return "Reeds";
				case KBoulder: return bIsland ? nullptr : "Boulder";
				case KBamboo: return bIsland ? nullptr : bMainland ? "Bamboo" : "Boulder"; // (off the mainland a fallen log, built whole)
				case KStump: return bIsland ? "CoconutPalmTall" : "Boulder";               // (the jungle's edge curtain, built whole)
				case KThicket: return "Boulder";                                            // (the jungle's thicket, an island's pandanus)
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

			/** Bark mottled mid-dark grey-brown, blotched with pale lichen and green moss, at a point of a trunk (X across it,
			 * Z up it): the blotches a hand to a forearm across. */
			static Tone Bark(double X, double Z, bool bDark, uint64_t Key)
			{
				const double U = Unit(Hash(Key, static_cast<uint64_t>(std::lround(X * 3.0) + 4096), static_cast<uint64_t>(std::lround(Z * 2.5) + 4096)));
				const double V = Unit(Hash(Key ^ 0xBA7, static_cast<uint64_t>(std::lround(X * 3.0) + 4096), static_cast<uint64_t>(std::lround(Z * 2.5) + 4096)));
				Tone T = Mixed(bDark ? Rgb(54, 50, 43) : Rgb(72, 67, 57), Rgb(62, 57, 48), V);
				if (U < 0.28) T = Mixed(T, Rgb(118, 116, 104), 0.8);   // (lichen, grey-white)
				else if (U > 0.86) T = Mixed(T, Rgb(54, 68, 34), 0.7); // (moss)
				return T;
			}

			/** Bark mottled, blotched with lichen and moss, for a tree whose bark paints are Bark and BarkDark. */
			static void PaleBark(LandField::Mesh& M, int BR, int BG, int BB, int DR, int DG, int DB, uint64_t Key)
			{
				for (size_t K = 0; K < M.Colours.size(); ++K)
				{
					LandField::Rgb8& C = M.Colours[K];
					const bool bBark = Same(C, BR, BG, BB), bDark = Same(C, DR, DG, DB);
					if (!bBark && !bDark) continue;
					C = Byte(Bark(M.Positions[3 * K], M.Positions[3 * K + 2], bDark, Key));
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
					Leafy(W, V3{ Mass[K][0], Mass[K][1], Mass[K][2] }, Mass[K][3], Mass[K][3], Mass[K][4], static_cast<int>(Mass[K][5]), Hash(Seed, 0x7C1D, static_cast<uint64_t>(K)), Lk, Uk);
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
					Blob(F, V3{ Lump[K][0], Lump[K][1], Lump[K][2] }, Lump[K][3], Lump[K][3], Lump[K][4], 4, 6, Hash(Seed, 0x7C1E, static_cast<uint64_t>(K)), Mixed(Lit, Rgb(96, 124, 56), 0.15 * K), Under);
				// (on trial "canopy" its middle level its own far one, not the kit file's boulder: the grey boulders seen from 60 m)
				if (bCanopy) M.Mid = M.Far;
			}

			/** The undergrowth's far level, the kit's a stub: five low lumps of leaves spread unevenly, as wide as its near. */
			void UnderFar(LandField::BaseMesh& M) const
			{
				M.Far = LandField::Mesh();
				Writer F(M.Far);
				const Tone Lit = Rgb(74, 100, 46), Under = Rgb(36, 52, 28);
				const double Lump[5][6] = { { 0.0, 0.0, 0.4, 0.9, 0.7, 0.45 }, { 0.9, 0.4, 0.35, 0.7, 0.5, 0.4 }, { -0.8, -0.3, 0.45, 0.75, 0.6, 0.5 }, { 0.3, -0.8, 0.3, 0.6, 0.5, 0.32 }, { -0.4, 0.8, 0.55, 0.55, 0.45, 0.4 } };
				for (int K = 0; K < 5; ++K) Blob(F, V3{ Lump[K][0], Lump[K][1], Lump[K][2] }, Lump[K][3], Lump[K][4], Lump[K][5], 4, 6, Hash(Seed, 0x7D1, static_cast<uint64_t>(K)), Lit, Under);
			}

			/** Buttress roots on a giant: five fins standing out from its trunk's foot, 3-4.5 m up it and 2-3.5 m out, their
			 * free edges sweeping down concave to the ground; barked as the trunk. */
			static void Buttress(LandField::Mesh& M, uint64_t Key)
			{
				// (the trunk's foot: its middle and girth from the bark's points within a metre of the ground)
				double SX = 0.0, SY = 0.0;
				int N = 0;
				std::vector<size_t> Foot;
				for (size_t K = 0; K < M.Colours.size(); ++K)
					if (M.Positions[3 * K + 2] < 1.0)
					{
						Foot.push_back(K);
						SX += M.Positions[3 * K];
						SY += M.Positions[3 * K + 1];
						++N;
					}
				if (N < 3) return;
				const double CX = SX / N, CY = SY / N;
				double Rad = 0.0;
				for (const size_t K : Foot) Rad += std::hypot(M.Positions[3 * K] - CX, M.Positions[3 * K + 1] - CY);
				Rad = std::clamp(Rad / N, 0.3, 2.0);
				Writer W(M);
				Rng R(Key);
				for (int F = 0; F < 5; ++F)
				{
					const double A = 2.0 * Pi * F / 5.0 + R.Range(-0.35, 0.35), High = R.Range(3.0, 4.5), Out = R.Range(2.0, 3.5), Thick = 0.2;
					const V3 Rd{ std::cos(A), std::sin(A), 0.0 }, Ac{ -std::sin(A), std::cos(A), 0.0 };
					const V3 Root{ CX + Rd.X * 0.8 * Rad, CY + Rd.Y * 0.8 * Rad, 0.0 };
					std::vector<V3> Edge; // the free edge, from its top on the trunk to its foot out on the ground
					for (int K = 0; K <= 4; ++K)
					{
						const double T = K / 4.0;
						Edge.push_back(Root + Rd * (Out * std::pow(T, 0.7)) + V3{ 0, 0, High * std::pow(1.0 - T, 1.8) - 0.1 });
					}
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const V3 Off = Ac * (0.5 * Thick * Side);
						const V3 Nn = Ac * static_cast<double>(Side);
						const uint32_t Q0 = W.Vert(Root.X + Off.X, Root.Y + Off.Y, -0.1, Nn.X, Nn.Y, 0.2, Bark(Side * 0.3, 0.0, true, Key));
						std::vector<uint32_t> E;
						for (const V3& P : Edge) E.push_back(W.Vert(P.X + Off.X, P.Y + Off.Y, P.Z, Nn.X, Nn.Y, 0.2, Bark(P.X - Root.X + Side * 0.3, P.Z, P.Z < 0.6, Key)));
						for (size_t K = 0; K + 1 < E.size(); ++K)
						{
							W.Tri(Q0, E[K], E[K + 1]);
							W.Tri(Q0, E[K + 1], E[K]);
						}
					}
					for (size_t K = 0; K + 1 < Edge.size(); ++K)
					{
						// (the fin's rim along its free edge)
						const V3 Up = Norm(Cross(Edge[K + 1] - Edge[K], Ac));
						const V3 L0 = Edge[K] - Ac * (0.5 * Thick), R0 = Edge[K] + Ac * (0.5 * Thick), L1 = Edge[K + 1] - Ac * (0.5 * Thick), R1 = Edge[K + 1] + Ac * (0.5 * Thick);
						const Tone C = Bark(Edge[K].X - Root.X, Edge[K].Z, false, Key ^ 0x5);
						const uint32_t A0 = W.Vert(L0.X, L0.Y, L0.Z, Up.X, Up.Y, std::fabs(Up.Z), C), A1 = W.Vert(R0.X, R0.Y, R0.Z, Up.X, Up.Y, std::fabs(Up.Z), C);
						const uint32_t B0 = W.Vert(L1.X, L1.Y, L1.Z, Up.X, Up.Y, std::fabs(Up.Z), C), B1 = W.Vert(R1.X, R1.Y, R1.Z, Up.X, Up.Y, std::fabs(Up.Z), C);
						W.Tri(A0, A1, B1);
						W.Tri(A0, B1, B0);
						W.Tri(A1, A0, B1);
						W.Tri(B1, A0, B0);
					}
				}
			}

			/** A curtain of climbers at the forest's edge, its face to +y: masses of leaves from the ground to some 16 m, 13 m
			 * across, three to a row, vines hanging down its face to the ground; far off, a sheet of six flat lumps. */
			void Curtain(LandField::BaseMesh& M) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ 0xC17A);
				const Tone Lit = Rgb(76, 106, 46), Under = Rgb(32, 48, 24);
				for (int Row = 0; Row < 6; ++Row)
					for (int Q = 0; Q < 3; ++Q)
					{
						const double X = 4.3 * (Q - 1) + R.Range(-0.6, 0.6) + (Row % 2 ? 1.1 : -1.1), Z = 1.3 + 2.6 * Row + R.Range(-0.4, 0.4), Y = R.Range(-0.5, 0.3);
						const double L = R.Range(0.85, 1.15), Warm = R.Range(-0.06, 0.06);
						Leafy(W, V3{ X, Y, Z }, 2.7, 1.1, 1.65, 15, Hash(Seed, 0xC17B, static_cast<uint64_t>(Row * 3 + Q)), Varied(Lit, L, Warm), Varied(Under, L, Warm));
					}
				for (int K = 0; K < 10; ++K)
				{
					// a vine hanging down its face, leaves along it
					const double X0 = R.Range(-4.2, 4.2), Z0 = R.Range(13.0, 16.0), Low = R.Range(0.2, 2.0), Sway = R.Range(0.2, 0.6);
					std::vector<V3> Line;
					std::vector<double> Wide;
					for (int S = 0; S <= 8; ++S)
					{
						const double T = S / 8.0;
						Line.push_back(V3{ X0 + Sway * std::sin(T * 5.0 + K), 0.9 + 0.3 * std::sin(T * 3.0), Z0 - (Z0 - Low) * T });
						Wide.push_back(0.05);
					}
					Ribbon(W, Line, Wide, V3{ 1, 0, 0 }, Rgb(64, 84, 40), Rgb(76, 98, 46));
					for (int S = 1; S < 8; ++S)
						for (int Sd = -1; Sd <= 1; Sd += 2)
						{
							const V3& P = Line[static_cast<size_t>(S)];
							const V3 Tip = P + V3{ 0.3 * Sd, 0.22, -0.12 };
							W.Blade(P.X, P.Y, P.Z, Tip.X, Tip.Y, Tip.Z, 0.16, Mixed(Under, Lit, 0.5), Mixed(Lit, Rgb(112, 140, 62), 0.4));
						}
				}
				Writer F(M.Far);
				const double Lump[6][6] = { { -3.4, 0.0, 3.0, 3.6, 1.1, 3.2 }, { 3.2, -0.1, 3.6, 3.6, 1.1, 3.6 }, { -0.2, -0.2, 8.2, 4.4, 1.1, 3.4 },
											{ -3.8, -0.2, 12.6, 3.2, 1.0, 3.0 }, { 3.6, -0.2, 11.8, 3.4, 1.0, 3.4 }, { 0.0, 0.0, 3.4, 2.6, 1.2, 2.8 } };
				for (int K = 0; K < 6; ++K) Blob(F, V3{ Lump[K][0], Lump[K][1], Lump[K][2] }, Lump[K][3], Lump[K][4], Lump[K][5], 4, 6, Hash(Seed, 0xC17C, static_cast<uint64_t>(K)), Mixed(Lit, Rgb(92, 120, 54), 0.1 * K), Under);
				if (bCanopy) M.Mid = M.Far; // (as the thicket's)
			}

			/** A fallen trunk rotting on the forest floor, along x, 6 m long and half sunk: mossy on its top, a broken branch or
			 * two, its ends pale. */
			void Log(LandField::BaseMesh& M) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				for (int Lv = 0; Lv < 2; ++Lv)
				{
					Writer W(Lv ? M.Far : M.Near);
					const int Round = Lv ? 5 : 8, Segs = Lv ? 1 : 4;
					const double Rad = 0.34, Long = 6.0;
					std::vector<std::vector<uint32_t>> Ring;
					for (int S = 0; S <= Segs; ++S)
					{
						const double T = static_cast<double>(S) / Segs, X = -0.5 * Long + Long * T, Bow = 0.25 * std::sin(T * Pi), Rr = Rad * (1.0 - 0.25 * T);
						std::vector<uint32_t> One;
						for (int K = 0; K < Round; ++K)
						{
							const double A = 2.0 * Pi * K / Round, NY = std::cos(A), NZ = std::sin(A);
							Tone C = Bark(X, 2.0 * A, NZ < 0.0, Seed ^ 0x106);
							if (NZ > 0.4) C = Mixed(C, Rgb(70, 92, 42), 0.55 * Smooth(0.4, 0.9, NZ)); // (moss along its top)
							One.push_back(W.Vert(X, Bow + Rr * NY, 0.7 * Rr + Rr * NZ, 0.0, NY, NZ, C));
						}
						Ring.push_back(One);
					}
					for (int S = 0; S < Segs; ++S)
						for (int K = 0; K < Round; ++K)
						{
							const uint32_t A = Ring[static_cast<size_t>(S)][static_cast<size_t>(K)], B = Ring[static_cast<size_t>(S)][static_cast<size_t>((K + 1) % Round)];
							const uint32_t C = Ring[static_cast<size_t>(S + 1)][static_cast<size_t>((K + 1) % Round)], D = Ring[static_cast<size_t>(S + 1)][static_cast<size_t>(K)];
							W.Tri(A, B, C);
							W.Tri(A, C, D);
						}
					for (int End = 0; End < 2; ++End)
					{
						// (the ends: pale broken wood)
						const std::vector<uint32_t>& E = Ring[End ? static_cast<size_t>(Segs) : 0];
						const double X = End ? 0.5 * Long : -0.5 * Long, Rr = Rad * (End ? 0.75 : 1.0);
						const uint32_t Mid = W.Vert(X, End ? 0.0 : 0.0, 0.7 * Rr, End ? 1.0 : -1.0, 0.0, 0.0, Rgb(150, 128, 96));
						for (int K = 0; K < Round; ++K)
						{
							W.Tri(Mid, E[static_cast<size_t>(K)], E[static_cast<size_t>((K + 1) % Round)]);
							W.Tri(Mid, E[static_cast<size_t>((K + 1) % Round)], E[static_cast<size_t>(K)]);
						}
					}
					if (!Lv)
					{
						W.Box(-0.8, 0.2, 0.75, 0.05, 0.4, 0.05, 0.5, Rgb(92, 84, 70));
						W.Box(1.5, -0.25, 0.65, 0.04, 0.35, 0.04, -0.7, Rgb(92, 84, 70));
					}
				}
			}

			/** An island's low shore shrub (Scaevola, beach heliotrope): small heads of waxy leaves on woody stems spread wide
			 * and low, uneven, about 1 m high and 5 m across, sand between them; far off, a low mat of eight flat lumps. */
			void Shrub(LandField::BaseMesh& M) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ 0x5B1);
				const Tone Lit = Rgb(108, 136, 66), Under = Rgb(46, 66, 34);
				// (a shore shrub sprawling low and open, wider than it is high: woody stems out from its root to a dozen
				// small leafy heads at different heights, sand between them)
				for (int K = 0; K < 11; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = 2.1 * std::sqrt(R.Next()), RX = R.Range(0.45, 0.8), RY = R.Range(0.4, 0.7), RZ = R.Range(0.24, 0.42);
					const double Lift = R.Range(0.0, 0.55), L = R.Range(0.85, 1.12);
					const V3 C{ D * std::cos(A), D * std::sin(A), RZ * 0.8 + Lift * (1.0 - D / 2.4) };
					Ribbon(W, { V3{ 0, 0, 0.02 }, V3{ 0.5 * C.X, 0.5 * C.Y, 0.55 * C.Z + 0.08 }, C }, { 0.05, 0.04, 0.025 }, V3{ -std::sin(A), std::cos(A), 0.0 }, Rgb(96, 84, 66), Rgb(88, 80, 62));
					Leafy(W, C, RX, RY, RZ, 11, Hash(Seed, 0x5B2, static_cast<uint64_t>(K)), Varied(Lit, L, 0.0), Varied(Under, L, 0.0));
				}
				Writer F(M.Far);
				// (far off a low spreading mat of small heads, not balls)
				const double Lump[8][6] = { { 0.0, 0.0, 0.6, 0.7, 0.6, 0.32 }, { 1.3, 0.5, 0.42, 0.7, 0.5, 0.26 }, { -1.3, -0.6, 0.36, 0.7, 0.55, 0.24 }, { 0.3, -1.5, 0.3, 0.6, 0.45, 0.22 },
											{ -0.6, 1.5, 0.34, 0.55, 0.5, 0.24 }, { 1.7, -1.0, 0.26, 0.5, 0.45, 0.2 }, { -1.9, 0.6, 0.24, 0.5, 0.4, 0.2 }, { 0.8, 1.9, 0.22, 0.45, 0.4, 0.18 } };
				for (int K = 0; K < 8; ++K) Blob(F, V3{ Lump[K][0], Lump[K][1], Lump[K][2] }, Lump[K][3], Lump[K][4], Lump[K][5], 3, 6, Hash(Seed, 0x5B3, static_cast<uint64_t>(K)), Mixed(Lit, Under, 0.12 * (K % 3)), Under);
			}

			/** A pandanus: a leaning grey trunk 4 m on a cone of stilt roots, forking at its head, each branch ending in a spiral
			 * tuft of long strap leaves arching over and drooping, a few gone dry. */
			void Pandanus(LandField::BaseMesh& M) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Rng R(Seed ^ 0x9A7D);
				const Tone Wood = Rgb(128, 118, 98), WoodDark = Rgb(98, 90, 76);
				const V3 Lean{ 0.9, 0.2, 0.0 };
				const V3 Top = Lean + V3{ 0, 0, 3.8 };
				std::vector<V3> Heads;
				for (int B = 0; B < 3; ++B)
				{
					const double A = 2.0 * Pi * B / 3.0 + 0.4;
					Heads.push_back(Top + V3{ 0.9 * std::cos(A), 0.9 * std::sin(A), 0.6 + 0.3 * B });
				}
				for (int Lv = 0; Lv < 2; ++Lv)
				{
					Writer W(Lv ? M.Far : M.Near);
					// the trunk and its branches, crossed strips
					const std::vector<V3> Trunk = { V3{ 0, 0, 0.9 }, Lean * 0.45 + V3{ 0, 0, 2.3 }, Top };
					Ribbon(W, Trunk, { 0.2, 0.17, 0.14 }, V3{ 1, 0, 0 }, WoodDark, Wood);
					Ribbon(W, Trunk, { 0.2, 0.17, 0.14 }, V3{ 0, 1, 0 }, WoodDark, Wood);
					for (const V3& Hd : Heads)
					{
						Ribbon(W, { Top, Hd }, { 0.12, 0.09 }, V3{ 1, 0, 0 }, Wood, Wood);
						if (!Lv) Ribbon(W, { Top, Hd }, { 0.12, 0.09 }, V3{ 0, 1, 0 }, Wood, Wood);
					}
					// the stilt roots: from the trunk's foot out to the ground
					for (int K = 0; K < (Lv ? 4 : 7); ++K)
					{
						const double A = 2.0 * Pi * K / (Lv ? 4.0 : 7.0) + 0.2, Up = 0.7 + 0.5 * std::fabs(std::sin(K * 1.9));
						const V3 Out{ 0.85 * std::cos(A), 0.85 * std::sin(A), 0.0 };
						const V3 Hi{ 0.1 * std::cos(A), 0.1 * std::sin(A), Up };
						Ribbon(W, { Hi, (Hi + Out) * 0.5 + V3{ 0, 0, 0.12 }, Out + V3{ 0, 0, -0.05 } }, { 0.07, 0.06, 0.05 }, V3{ -std::sin(A), std::cos(A), 0.0 }, Wood, WoodDark);
					}
					// the leaf tufts
					for (size_t B = 0; B < Heads.size(); ++B)
					{
						const V3& Hd = Heads[B];
						const int Leaves = Lv ? 5 : 18;
						for (int L = 0; L < Leaves; ++L)
						{
							const double A = 2.0 * Pi * L / Leaves * 2.618 + static_cast<double>(B), Rise = 0.35 + 0.55 * std::fabs(std::sin(L * 1.7 + B)), Long = Lv ? 1.3 : 1.15 + 0.4 * std::fabs(std::cos(L * 2.3));
							const V3 Out{ std::cos(A), std::sin(A), 0.0 };
							const bool bDry = (L + static_cast<int>(B)) % 7 == 3;
							const Tone Foot = bDry ? Rgb(138, 120, 82) : Rgb(70, 94, 48), Tip = bDry ? Rgb(160, 140, 96) : Rgb(112, 136, 66);
							const std::vector<V3> Line = { Hd, Hd + Out * (0.35 * Long) + V3{ 0, 0, Rise * 0.6 }, Hd + Out * (0.75 * Long) + V3{ 0, 0, Rise * 0.55 - 0.15 }, Hd + Out * Long + V3{ 0, 0, Rise * 0.1 - 0.55 } };
							// (far off fewer and broader)
							Ribbon(W, Line, Lv ? std::vector<double>{ 0.08, 0.3, 0.24, 0.0 } : std::vector<double>{ 0.05, 0.09, 0.07, 0.0 }, V3{ -Out.Y, Out.X, 0.0 }, Foot, Tip);
						}
					}
				}
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
				// (the trees, drawn far off as the bocage's are, a crown of some 40 triangles: the rainforest's, the palms, the
				// bamboo; not the bananas, the shot palms' bare poles or the thickets)
				M.bTree = K == KTree || K == KGiant || K == KPalm || K == KPalmTall || K == KPalmYoung || (K == KBamboo && bMainland);
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
						Buttress(M.Near, Seed ^ 0xB77);
						Buttress(M.Far, Seed ^ 0xB77);
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
				if (!bIsland)
				{
					Curtain(M);
					M.Paint = 1;
					M.FarFrom = 80.0f;
					Sight(0.5f, 0.5f, 0.95f, 0.0f);
					break;
				}
				if (bRead)
				{
					Snap(M.Near, 9.0, true);
					Snap(M.Far, 9.0, false);
				}
					M.Paint = 0;
					M.FarFrom = 100.0f;
					Sight(0.5f, 0.5f, 0.3f, 0.35f);
					break;
				case KBamboo:
				if (!bMainland)
				{
					Log(M);
					M.Paint = 0;
					M.FarFrom = 60.0f;
					Sight(0.5f, 0.5f, 0.3f, 0.0f);
					break;
				}
				[[fallthrough]];
				case KBanana:
					M.Paint = 1;
					M.FarFrom = K == KBamboo ? 80.0f : 60.0f;
					Sight(0.55f, 0.45f, 0.8f, 0.2f);
					if (bCanopy && K == KBanana) M.Class = LandField::BaseMesh::Draw::Shrub; // (on trial "canopy": culled and its shadow cut at the shrubs')
					break;
				case KUnder:
					if (bRead) UnderFar(M);
					M.Paint = 1;
					M.FarFrom = 55.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				case KThicket:
				if (bIsland)
				{
					Pandanus(M);
					M.Paint = 1;
					M.FarFrom = 80.0f;
					Sight(0.75f, 0.25f, 0.6f, 0.15f);
					break;
				}
				Thicket(M);
					M.Paint = 1;
					M.FarFrom = 70.0f;
					Sight(0.5f, 0.5f, 0.92f, 0.0f);
					if (bCanopy) M.Class = LandField::BaseMesh::Draw::Shrub; // (on trial "canopy", as the banana)
					break;
				case KGrass:
				if (bIsland)
				{
					Shrub(M);
					M.Paint = 1;
					M.FarFrom = 70.0f;
					Sight(0.5f, 0.5f, 0.85f, 0.0f);
					break;
				}
				if (bCanopy) M.Class = LandField::BaseMesh::Draw::Shrub; // (on trial "canopy" the kunai's tufts, as the banana)
				[[fallthrough]];
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

			/** The kunai's clump (clutter Clut0), and on trial (Fd.Tried("kunai")) the clump with its plume in Poppies' place,
			 * laid for one clump in six: the art director's "keep about 1 in 6", 19:57Z. Its blades the same either way. */
			void KunaiClump(LandField::BaseMesh& M, bool bPlume) const
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ 0xC10);
				// Kunai: a patch of it some 1.3 m across, saw-edged blades 0.75-1.5 m from many feet, leaning every way and
				// bending over, pale yellow-green, straw at their tips; one slim stem with a small soft plume of fine
				// filaments, low among the blades.
				for (int B = 0; B < 46; ++B)
				{
					const double A = R.Range(0.0, 2.0 * Pi), R0 = 0.62 * std::sqrt(R.Next()), High = R.Range(0.75, 1.5), Lean = R.Range(0.1, 0.5);
					const double Bend = R.Range(0.1, 0.45), Green = R.Next(), Turn = R.Range(-1.1, 1.1);
					const V3 Out{ std::cos(A + Turn), std::sin(A + Turn), 0.0 };
					const V3 Foot{ R0 * std::cos(A), R0 * std::sin(A), 0.0 };
					const std::vector<V3> Line = { Foot, Foot + Out * (0.25 * Lean * High) + V3{ 0, 0, 0.5 * High }, Foot + Out * (0.65 * Lean * High) + V3{ 0, 0, 0.85 * High },
												   Foot + Out * ((Lean + Bend) * High) + V3{ 0, 0, (0.95 - Bend) * High } };
					const Tone Tip = Mixed(Rgb(160, 152, 104), Rgb(140, 142, 90), Green);
					// (lit as from above and a little outward, as a thin blade turned from the sun draws near black)
					Ribbon(W, Line, { 0.022, 0.018, 0.012, 0.0 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(156, 144, 96), Tip, 1.8); // (pale at the foot as kunai is, never a dark reed)
				}
				if (bPlume)
				{
					const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(1.3, 1.5);
					const V3 Out{ std::cos(A), std::sin(A), 0.0 };
					const V3 Top = Out * 0.12 + V3{ 0, 0, High };
					Ribbon(W, { V3{}, Out * 0.05 + V3{ 0, 0, 0.6 * High }, Top }, { 0.008, 0.007, 0.005 }, V3{ -Out.Y, Out.X, 0.0 }, Rgb(156, 146, 98), Rgb(164, 154, 104), 1.8);
					for (int F = 0; F < 16; ++F)
					{
						// (the plume: a soft spindle of fine silky hairs up the stem's last hand's breadth, a dull cream,
						// never a star of white strips)
						const double Fa = A + 2.4 * F, At = 0.012 * F, Up = 1.0 - 0.04 * F;
						const V3 Dir = Norm(V3{ 0.32 * std::cos(Fa) + 0.25 * Out.X, 0.32 * std::sin(Fa) + 0.25 * Out.Y, Up });
						const V3 Root = Top + V3{ 0, 0, At - 0.06 }, Mid = Root + Dir * 0.05, End = Root + Dir * (0.09 + 0.002 * F);
						Ribbon(W, { Root, Mid, End }, { 0.002, 0.007, 0.0 }, V3{ -Dir.Y, Dir.X, 0.0 }, Rgb(150, 142, 118), Rgb(176, 168, 146), 1.8);
					}
				}
				M.FarFrom = 48.0f;
				M.Paint = 2;
			}

			bool OwnClutter(LandField::Base B, LandField::BaseMesh& M) const override
			{
				// (on trial "canopy" a fifth of the kunai's clumps, drawn only within 30 m, in Flowers' slot: the clump as Clut0's)
				if (B == LandField::Base::Flowers && bCanopy)
				{
					KunaiClump(M, !bPlumeTrial);
					M.FarFrom = 30.0f;
					return true;
				}
				if (B != LandField::Base::Poppies || !bPlumeTrial) return false;
				KunaiClump(M, true);
				return true;
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
					KunaiClump(M, !bPlumeTrial);
					break;
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
					for (int L = 0; L < 26; ++L)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = 0.85 * std::sqrt(R.Next()), Turn = R.Range(0.0, Pi), Long = R.Range(0.08, 0.16), Z = R.Range(0.008, 0.03);
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
					// A palm's droppings: three husks, ovoid and grey-brown (one still green-brown), half sunk in the sand and
					// scattered; a dead frond lying curved and broken in two, brown-grey, its leaflets curled up at their tips
					// and gone in places.
					for (int H = 0; H < 3; ++H)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.4, 1.5), Turn = R.Range(0.0, Pi), Big = R.Range(0.85, 1.15);
						const size_t From = M.Near.Colours.size();
						const Tone Lit = H == 2 ? Rgb(110, 108, 66) : Rgb(122, 102, 76), Under = H == 2 ? Rgb(76, 76, 48) : Rgb(82, 70, 54);
						const V3 C{ D * std::cos(A), D * std::sin(A), 0.012 * Big };
						Blob(W, C, 0.13 * Big, 0.095 * Big, 0.09 * Big, 4, 6, Hash(Seed, 0x4E5, static_cast<uint64_t>(H)), Lit, Under);
						for (size_t V = From; V < M.Near.Colours.size(); ++V)
						{
							// (turned about its own middle)
							float* P = &M.Near.Positions[3 * V];
							float* N = &M.Near.Normals[3 * V];
							const double DX = P[0] - C.X, DY = P[1] - C.Y, CT = std::cos(Turn), ST = std::sin(Turn);
							P[0] = static_cast<float>(C.X + DX * CT - DY * ST);
							P[1] = static_cast<float>(C.Y + DX * ST + DY * CT);
							const double NX = N[0], NY = N[1];
							N[0] = static_cast<float>(NX * CT - NY * ST);
							N[1] = static_cast<float>(NX * ST + NY * CT);
						}
					}
					const double A = R.Range(0.0, 2.0 * Pi), Curve = R.Range(0.25, 0.5);
					const V3 Out{ std::cos(A), std::sin(A), 0.0 }, Side{ -Out.Y, Out.X, 0.0 };
					const V3 Base0 = Out * -1.2 + Side * 0.3;
					auto Along = [&](double T, int Piece) {
						// (the rachis bowed to one side; its outer piece kinked where it broke)
						V3 P = Base0 + Out * (2.5 * T) + Side * (Curve * std::sin(Pi * T)) + V3{ 0, 0, 0.03 + 0.05 * T * T };
						if (Piece) P = P + Side * (0.35 * (T - 0.55)) + Out * 0.08;
						return P;
					};
					const Tone Grey = Rgb(132, 120, 98), Pale = Rgb(154, 142, 116), Dark = Rgb(112, 100, 80);
					for (int Piece = 0; Piece < 2; ++Piece)
					{
						const double T0 = Piece ? 0.58 : 0.0, T1 = Piece ? 1.0 : 0.53;
						Ribbon(W, { Along(T0, Piece), Along(0.5 * (T0 + T1), Piece), Along(T1, Piece) }, { 0.06 - 0.03 * T0, 0.05 - 0.03 * T0, 0.035 - 0.03 * T0 }, V3{ 0, 0, 1 }, Grey, Dark);
					}
					double Lift = 0.0, Skew = 0.0, GroupSd = 1.0;
					for (int L = 0; L < 72; ++L)
					{
						// (leaflets close set, drying in tangled groups of four lying on the sand or lifting a little off it at their
						// own slant, curling up at their tips, each its own length where it broke, gone in places)
						if (L % 4 == 0)
						{
							Lift = R.Range(-0.05, 0.32);
							Skew = R.Range(-0.35, 0.35);
							GroupSd = R.Next() < 0.5 ? -1.0 : 1.0;
						}
						const double T = 0.04 + 0.94 * L / 72.0, Broke = R.Range(0.45, 1.0), Gone = R.Next(), Curl = R.Range(0.03, 0.12), Splay = R.Range(-0.15, 0.15);
						if (Gone < 0.15 || (T > 0.53 && T < 0.58)) continue;
						const double Long = 0.85 * (1.0 - 0.45 * T) * Broke, Sd = (L % 2) ? GroupSd : -GroupSd;
						const int Piece = T > 0.55 ? 1 : 0;
						const V3 Root = Along(T, Piece) + V3{ 0, 0, 0.02 };
						const V3 Dir = Norm(Side * Sd + Out * (0.5 + Skew + Splay));
						const double Rise = Lift * Long;
						const V3 Mid = Root + Dir * (0.45 * Long) + V3{ 0, 0, std::max(-0.02, 0.75 * Rise) }, Tip = Root + Dir * (0.9 * Long) + V3{ 0, 0, std::max(-0.01, 0.4 * Rise) + Curl - 0.02 };
						Ribbon(W, { Root, Mid, Tip }, { 0.04, 0.045, 0.0 }, Cross(Dir, V3{ 0, 0, 1 }), Mixed(Grey, Dark, Gone), Mixed(Pale, Grey, Gone));
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
						// (on trial "canopy" its new plants from a second stream of the cell's own, made only on the trial, so the
						// first stream's draws stay where they were: R2's own place in the cell, and a thing stood there)
						std::optional<Rng> R2;
						if (bCanopy) R2.emplace(Hash(Key0 ^ 0xCA90ull, static_cast<uint64_t>(I), static_cast<uint64_t>(J)));
						double QX = X, QY = Y;
						auto Elsewhere2 = [&R2, &QX, &QY, X0, Y0]() {
							const double A = R2->Next();
							const double Bq = R2->Next();
							QX = X0 + A * Cell;
							QY = Y0 + Bq * Cell;
						};
						auto Chance2 = [&R2](double P) { return R2->Next() < P; };
						auto Stand2 = [&H, &R2, &Leaf](B Kind, double AX, double AY, double Room, double ScLo, double ScHi, double SzLo, double SzHi, double LLo, double LHi, double Warm) {
							const double Yw = R2->Range(0.0, 2.0 * Pi);
							const double Sc = R2->Range(ScLo, ScHi);
							const double Sz = R2->Range(SzLo, SzHi);
							const double Lf = R2->Range(LLo, LHi);
							Put(H, Kind, AX, AY, Room, Yw, Sc, Sz, Leaf(Lf, Warm));
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
							// the scrub: low shore shrubs spreading wide and uneven, now and then a pandanus on its stilt roots
							auto Scrub = [&](double AX, double AY, double PShrub, double PPandanus) {
								// (in thickets where the ground cover drifts, spreading; a lone shrub now and then on the open sand; half
								// as many as the sand would hold, so from above they are no speckle)
								const double Thick = Smooth(-0.05, 0.35, Read(AX, AY, 1));
								if (R.Next() < PShrub * (0.06 + 0.85 * Thick)) Stand(B::Kit6, AX, AY, 0.8, 0.7, 1.3, 0.7, 1.2, 0.9, 1.1, 0.03);
								Elsewhere();
								if (R.Next() < PPandanus * (0.15 + 0.7 * Thick)) Stand(B::Kit15, PX, PY, 0.8, 0.8, 1.2, 0.8, 1.25, 0.9, 1.1, 0.0);
							};
							if (W < 40.0)
							{
								// the strand along the beach's crest: scrub, young palms, a few old ones leaning out over the sand
								Scrub(X, Y, 0.45, 0.14);
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
								if (R.Next() < 0.12) Stand(B::Kit0, PX, PY, 0.8, 0.45, 0.8, 0.5, 0.85, 0.9, 1.05, 0.0);
								Elsewhere();
								Scrub(PX, PY, 0.45, 0.2);
								Elsewhere();
								if (R.Next() < 0.1) Stand(B::Kit5, PX, PY, 0.6, 0.8, 1.2, 0.8, 1.3, 0.95, 1.05, 0.0);
								continue;
							default: break;
							}
							// (a plantation or the strand inside its ragged edge, the grove outside it)
							const double Side = S.Crop == Plantation || S.Crop == Strand ? R.Next() : 1.0;
							if (S.Crop == Plantation && Side < Inward(S))
							{
								Rows(S, X0, Y0, Cell, R, [&](double AX, double AY, double Way) { Palm(AX, AY, Way); });
								Elsewhere();
								Scrub(PX, PY, 0.1, 0.0);
								continue;
							}
							if (S.Crop == Strand && Side < Inward(S))
							{
								if (R.Next() < 0.15) Palm(X, Y, Yaw);
								Elsewhere();
								Scrub(PX, PY, 0.3, 0.06);
								continue;
							}
							// the grove: palms in groves a hundred metres and more across with open sand between them (from above,
							// never a speckle of lone crowns), some 130 a hectare in a grove, in clumps and gaps (two or three
							// together, each leaning its own way); scrub and a broadleaf tree here and there under them
							const double InGrove = GroveAt(X, Y);
							const double Clump = Noise(X / 34.0 + 0.4, Y / 34.0 - 2.2, Seed ^ 0xC1C);
							if (R.Next() < std::clamp(0.64 + 0.8 * Clump, 0.1, 0.92) * (0.05 + 0.95 * InGrove)) Palm(X, Y, Yaw);
							Elsewhere();
							if (InGrove > 0.5 && Clump > 0.2 && R.Next() < 0.35)
							{
								const double Turn = R.Range(-1.2, 1.2);
								Palm(PX, PY, Yaw + Turn);
							}
							Elsewhere();
							Scrub(PX, PY, 0.22 * (0.4 + 0.6 * InGrove), 0.05);
							Elsewhere();
							if (R.Next() < 0.03 * InGrove) Stand(B::Kit0, PX, PY, 0.8, 0.5, 0.8, 0.5, 0.8, 0.95, 1.05, 0.0);
							continue;
						}
						// ---- the jungle
						const double Warm = R.Range(-0.04, 0.06);
						// (on trial "canopy" a yard's and a garden's bananas and palms gathered into groves, as many in all: 1 off it)
						const double FGrove = bCanopy ? 0.15 + 0.85 * YardGrove(X, Y) / kGroveMean : 1.0;
						// (on trial "canopy", from R2: an island of trees in the kunai, its core a few canopy trees over a
						// sub-canopy and undergrowth, its rim thickets and bananas; a clearing's bananas, young palms and
						// saplings, thicker toward its rim)
						auto Island = [&](double Isle) {
							const double Core = Smooth(0.6, 0.95, Isle), Rim = 1.0 - Core; // (Isle 0.5 and more)
							Elsewhere2();
							const double U2 = R2->Next();
							if (U2 < IsleGiant * Core) Stand2(B::Kit1, QX, QY, 1.6, 0.8, 1.05, 0.9, 1.15, 0.85, 1.05, Warm);
							else if (U2 < (IsleGiant + IsleCanopy) * Core) Stand2(B::Kit0, QX, QY, 1.0, 1.0, 1.5, 1.0, 1.4, 0.82, 1.05, Warm);
							Elsewhere2();
							if (Chance2(IsleSub)) Stand2(B::Kit0, QX, QY, 0.5, 0.5, 0.9, 0.5, 0.95, 0.85, 1.1, 0.0);
							Elsewhere2();
							if (Chance2(IsleThicket * Rim)) Stand2(B::Kit15, QX, QY, 0.5, 0.8, 1.3, 0.8, 1.35, 0.85, 1.12, Warm);
							Elsewhere2();
							if (Chance2(IsleUnder)) Stand2(B::Kit7, QX, QY, 0.5, 1.2, 2.2, 1.2, 2.4, 0.85, 1.1, 0.0);
							Elsewhere2();
							if (Chance2(IsleBanana * Rim)) Stand2(B::Kit5, QX, QY, 0.5, 0.8, 1.3, 0.85, 1.5, 0.85, 1.05, 0.0);
						};
						auto Glade = [&](double Hc) {
							const double Margin = 0.3 + 0.7 * Smooth(0.05, kClearing, Hc);
							Elsewhere2();
							if (Chance2(GladeBanana * Margin)) Stand2(B::Kit5, QX, QY, 0.5, 0.8, 1.3, 0.85, 1.5, 0.85, 1.05, 0.0);
							Elsewhere2();
							if (Chance2(GladeYoung * Margin)) Stand2(B::Kit4, QX, QY, 0.5, 0.4, 0.7, 0.45, 0.8, 0.85, 1.0, 0.0);
							Elsewhere2();
							if (Chance2(GladeUnder * Margin)) Stand2(B::Kit7, QX, QY, 0.5, 1.0, 1.8, 1.0, 2.0, 0.85, 1.1, 0.0);
							Elsewhere2();
							if (Chance2(GladeSapling * Margin)) Stand2(B::Kit0, QX, QY, 0.5, 0.3, 0.5, 0.3, 0.55, 0.85, 1.1, 0.0);
							// (the rim's skirt run on a few metres into the clearing, so the clump meets it with leaves, not bare boles)
							const double Skirt = Smooth(0.12, kClearing, Hc);
							Elsewhere2();
							if (Chance2(SkirtThicket * Skirt)) Stand2(B::Kit15, QX, QY, 0.5, 0.8, 1.3, 0.8, 1.35, 0.85, 1.12, Warm);
							Elsewhere2();
							if (Chance2(SkirtUnder * Skirt)) Stand2(B::Kit7, QX, QY, 0.5, 1.2, 2.2, 1.2, 2.4, 0.85, 1.1, 0.0);
						};
						// the kunai: clumps of it standing over the sea of grass in patches, none on its paths, its burns
						// or where it thins; now and then a clump of trees and undergrowth with a tree in it, or a wild palm
						auto KunaiHere = [&](double FClear) {
							if (BurnAt(X, Y) > 0.5) return;
							// (on trial "canopy": islands of trees standing in it, the lone clump gone, a wild palm only on an
							// island's fringe, so no pole stands alone on the plain; in a woods' clearing the tufts FClear of the
							// plain's; each factor 1 off the trial)
							const double Isle = bCanopy ? IsleAt(X, Y) : 0.0;
							const double FTuft = bCanopy ? 1.0 - Isle : 1.0, FLone = bCanopy ? 0.0 : 1.0;
							const double FPalm = bCanopy ? (Isle >= 0.02 ? 1.5 : 0.0) : 1.0;
							const double Clumps = Organic(X, Y, 24.0, Seed ^ 0xC14);
							const double Want = (Clumps > -0.05 ? 0.85 : 0.25) * (1.0 - ThinAt(X, Y)) * (1.0 - TrodAt(X, Y)) * FTuft * FClear;
							if (R.Next() < Want) Tuft(B::Kit6, X, Y, 0.6, 1.2, 1.8, 0.4, 0.56, Tint(1.15 + 0.08 * Dry, 0.95, 1.0)); // (tan-green, not lime, from above)
							if (R.Next() < 0.008 * FLone)
							{
								Stand(B::Kit0, X, Y, 0.8, 0.55, 0.95, 0.6, 1.0, 0.85, 1.12, Warm);
								for (int K = 0; K < 4; ++K)
								{
									const double A = R.Range(0.0, 2.0 * Pi);
									const double D = R.Range(2.0, 6.0);
									Stand(K == 3 ? B::Kit5 : K == 2 ? B::Kit15 : B::Kit7, X + D * std::cos(A), Y + D * std::sin(A), 0.5, 1.0, 1.8, 0.8, 1.6, 0.85, 1.1, 0.0);
								}
							}
							Elsewhere();
							if (R.Next() < 0.01 * FPalm) Stand(B::Kit2, PX, PY, 0.5, 0.85, 1.1, 0.8, 1.1, 0.9, 1.1, 0.0);
							if (bCanopy && Isle >= 0.5) Island(Isle);
						};
						switch (S.Kind)
						{
						case KVillage:
						case KYard:
							// bananas and young palms round the houses, an old palm or two (on trial "canopy" gathered into groves,
							// about as many in all)
							if (R.Next() < 0.35 * FGrove) Stand(B::Kit5, X, Y, 0.6, 0.8, 1.25, 0.8, 1.3, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.12 * FGrove) Stand(B::Kit4, PX, PY, 0.5, 0.8, 1.1, 0.8, 1.15, 0.9, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.06 * FGrove) Stand(B::Kit2, PX, PY, 0.5, 0.85, 1.1, 0.85, 1.15, 0.9, 1.1, 0.0);
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
							if (Eaten(X, Y))
							{
								KunaiHere(1.0);
								continue;
							}
							// the rainforest: one roof of big crowns 21-28 m up, merging, with the emergent giants standing
							// above it (34-42 m); slim sub-canopy trees, bananas and young palms; at its edge a curtain of
							// climbers from the canopy to the ground before a wall of thickets
							const bool bEdge = S.Edge < 10.0, bDeep = Outside(X, Y) > 70.0;
							// (on trial "canopy" the woods in clumps: a clearing the kunai's, with bananas and young palms; in a
							// clump's heart more and bigger canopy trees, their crowns overlapping and darker, little under them;
							// on its rim a skirt of thickets and undergrowth and a middle storey. Every factor is 1 off the trial,
							// and every draw of R stays where it was: only what it is weighed against changes)
							const double Hc = bCanopy ? ClumpAt(X, Y, S.Edge) : 1.0;
							if (bCanopy && Hc < kClearing)
							{
								KunaiHere(GladeTuft);
								Glade(Hc);
								continue;
							}
							const double Hh = bCanopy ? Smooth(0.5, 0.9, Hc) : 1.0; // (1 a heart, 0 a rim)
							// (the woods' face, its outer 22 m or so, held whole by ClumpAt, its roof a little thinner than a heart's:
							// EdgeFace's crowns in the outer 10 m, EdgeBack's behind them; its middle storey a rim's)
							const double Face = bCanopy && !bDeep ? Smooth(28.0, 22.0, S.Edge) : 0.0;
							const double Hr = bCanopy ? std::min(Hh, Smooth(12.0, 25.0, S.Edge)) : 1.0;
							const double Roof = Lerp(EdgeFace, EdgeBack, Smooth(10.0, 14.0, S.Edge));
							const double FGiant = bCanopy ? Lerp(GiantRim, GiantHeart, Hh) / 0.08 : 1.0;
							const double FCanopy = bCanopy ? Lerp(Lerp(GiantRim + CanopyRim, (bDeep ? GiantHeart + CanopyDeep : GiantHeart + CanopyHeart), Hh), Roof, Face) / 0.48 : 1.0;
							const double FScLo = bCanopy ? Lerp(1.0, ScLoHeart / 1.6, Hh) : 1.0, FScHi = bCanopy ? Lerp(1.0, ScHiHeart / 2.1, Hh) : 1.0;
							const double FSzLo = bCanopy ? Lerp(1.0, SzLoHeart / 1.3, Hh) : 1.0, FSzHi = bCanopy ? Lerp(1.0, SzHiHeart / 1.7, Hh) : 1.0;
							const double FLLo = bCanopy ? Lerp(1.0, LeafLoHeart / 0.85, Hh) : 1.0, FLHi = bCanopy ? Lerp(1.0, LeafHiHeart / 1.12, Hh) : 1.0;
							const double U = R.Next();
							if (U < 0.08 * FGiant) Stand(B::Kit1, X, Y, 1.6, 0.95, 1.25, 1.05, 1.3, 0.85 * FLLo, 1.12 * FLHi, Warm);
							else if (U < 0.48 * FCanopy) Stand(B::Kit0, X, Y, 1.0, 1.6 * FScLo, 2.1 * FScHi, 1.3 * FSzLo, 1.7 * FSzHi, 0.85 * FLLo, 1.12 * FLHi, Warm);
							// (deep in the band, seen only from afar: the canopy and its sub-canopy alone)
							Elsewhere();
							const double FSub = !bCanopy || bEdge ? 1.0 : bDeep ? SubDeep / 0.35 : Lerp(SubRim, SubHeart, Hr) / 0.75;
							const double FSubSc = bCanopy && !bEdge ? Lerp(1.0 / 0.42 * 0.6, 1.0, Hr) : 1.0, FSubScHi = bCanopy && !bEdge ? Lerp(1.0 / 0.85, 1.0, Hr) : 1.0;
							const double FSubSz = bCanopy && !bEdge ? Lerp(0.6 / 0.45, 1.0, Hr) : 1.0, FSubSzHi = bCanopy && !bEdge ? Lerp(1.0 / 0.95, 1.0, Hr) : 1.0;
							if (R.Next() < (bEdge ? 1.0 : bDeep ? 0.35 : 0.75) * FSub)
							{
								if (bEdge) Stand(B::Kit0, PX, PY, 0.5, 0.3, 0.6, 0.32, 0.65, 0.85, 1.1, 0.0);
								else Stand(B::Kit0, PX, PY, 0.5, 0.42 * FSubSc, 0.85 * FSubScHi, 0.45 * FSubSz, 0.95 * FSubSzHi, 0.85, 1.1, 0.0);
							}
							if (bDeep && !bEdge) continue;
							if (bEdge)
							{
								// the curtain at the edge itself, its face turned out (the way the woods' edge lies, from
								// the edge's distance a little either side)
								if (S.Edge < 8.0)
								{
									auto In_ = [](const Spot& Q) { return Q.Kind == KWoods ? Q.Edge : -Q.Edge; };
									double GX = In_(H.At(X + 3.0, Y)) - In_(H.At(X - 3.0, Y)), GY = In_(H.At(X, Y + 3.0)) - In_(H.At(X, Y - 3.0));
									const double GL = std::hypot(GX, GY);
									const double Sc = R.Range(0.85, 1.15);
									const double Sz = R.Range(0.85, 1.2);
									const double Lf = R.Range(0.88, 1.1);
									if (GL > 0.5)
									{
										GX /= GL;
										GY /= GL;
										// (only where the woods run deep behind it: a lone clump on the plain takes none)
										const Spot Behind = H.At(X + GX * 15.0, Y + GY * 15.0);
										const double Back = std::max(0.0, S.Edge - 1.5);
										// (on trial "canopy" not before a clearing either: the curtain hangs from a roof behind it, and on the
										// face's thinner roof only CurtainFace of them, from R2, so the face is not a wall of climbers)
										const bool bRoof = !bCanopy || (ClumpAt(X + GX * 15.0, Y + GY * 15.0, Behind.Edge) >= kClearing && Chance2(CurtainFace));
										if (Behind.Kind == KWoods && Behind.Edge > 6.0 && bRoof) Put(H, B::Kit14, X - GX * Back, Y - GY * Back, 0.5, std::atan2(GX, -GY), Sc, Sz, Leaf(Lf, Warm));
									}
								}
								for (int K = 0; K < 2; ++K)
								{
									Elsewhere();
									Stand(B::Kit15, PX, PY, 0.5, 0.9, 1.45, 0.9, 1.5, 0.85, 1.12, Warm);
								}
								Elsewhere();
								Stand(B::Kit0, PX, PY, 0.5, 0.45, 0.75, 0.42, 0.7, 0.85, 1.1, 0.0);
							}
							else
							{
								Elsewhere();
								const double FThicket = bCanopy ? Lerp(ThicketRim, ThicketHeart, Hh) / 0.3 : 1.0;
								if (R.Next() < 0.3 * FThicket) Stand(B::Kit15, PX, PY, 0.5, 0.7, 1.1, 0.7, 1.2, 0.85, 1.12, Warm);
							}
							// (on trial "canopy" no banana or young palm in a heart, more on a rim; the woods' outer edge as it was)
							const double FBanana = bCanopy && !bEdge ? Lerp(BananaRim, 0.0, Hh) / 0.35 : 1.0;
							const double FUnder = bCanopy && !bEdge ? Lerp(1.0, UnderHeart, Hh) / 0.8 : 1.0;
							const double FYoung = bCanopy && !bEdge ? Lerp(YoungRim, 0.0, Hh) / 0.16 : 1.0;
							Elsewhere();
							if (R.Next() < (bEdge ? 0.4 : 0.35) * FBanana) Stand(B::Kit5, PX, PY, 0.5, 0.8, 1.35, 0.85, 1.5, 0.85, 1.05, 0.0);
							Elsewhere();
							if (R.Next() < (bEdge ? 0.6 : 0.8) * FUnder) Stand(B::Kit7, PX, PY, 0.5, 1.2, 2.2, 1.2, 2.4, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.16 * FYoung) Stand(B::Kit4, PX, PY, 0.5, 0.35, 0.55, 0.35, 0.6, 0.85, 0.95, 0.0);
							// (bamboo on the mainland; elsewhere a fallen trunk rotting on the floor)
							Elsewhere();
							if (R.Next() < (bMainland ? 0.04 : 0.12)) Stand(B::Kit13, PX, PY, bMainland ? 1.2 : 0.8, 0.8, 1.2, 0.8, 1.2, 0.95, 1.05, 0.0);
							// (on trial "canopy" the rim's skirt to the ground, facing its clearing: one more thicket and undergrowth)
							if (bCanopy && !bEdge && Hh < 1.0)
							{
								const double Rim = 1.0 - Hh;
								Elsewhere2();
								if (Chance2(SkirtThicket * Rim)) Stand2(B::Kit15, QX, QY, 0.5, 0.8, 1.3, 0.8, 1.35, 0.85, 1.12, Warm);
								Elsewhere2();
								if (Chance2(SkirtUnder * Rim)) Stand2(B::Kit7, QX, QY, 0.5, 1.2, 2.2, 1.2, 2.4, 0.85, 1.1, 0.0);
							}
							continue;
						}
						default: break;
						}
						const int C = S.Crop >= 0 ? S.Crop : Kunai;
						if (C != Kunai)
						{
							// (a parcel's own crop inside its ragged edge, the kunai outside it)
							const double Side = R.Next();
							if (Side >= Inward(S))
							{
								KunaiHere(1.0);
								continue;
							}
						}
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
								// (the fronds yellow-green in the sun, near the kunai's own hue from above, not the forest's)
								const LandField::Rgb8 Sun = Tint(1.3 * Lf, 1.04 * Lf, 1.08 * Lf);
								if (Pick < 0.7) Put(H, B::Kit3, AX, AY, 0.5, Way - LeanTall, Sc, Sz, Sun);
								else Put(H, B::Kit2, AX, AY, 0.5, Way - LeanPalm, Sc, Sz * 1.15, Sun);
							});
							Elsewhere();
							if (R.Next() < 0.05) Stand(B::Kit7, PX, PY, 0.5, 0.8, 1.2, 0.7, 1.1, 0.9, 1.1, 0.0);
							break;
						}
						case Paddy:
							if (S.Edge < 6.0 && R.Next() < 0.12) Stand(B::Kit2, X, Y, 0.5, 0.85, 1.1, 0.85, 1.1, 0.9, 1.1, 0.0);
							break;
						case Ridge:
							if (R.Next() < 0.25) Tuft(B::Kit6, X, Y, 0.6, 0.7, 1.0, 0.3, 0.42, Tint(1.17 + 0.1 * Dry, 0.95, 1.0));
							Elsewhere();
							if (R.Next() < 0.05) Tuft(B::Kit12, PX, PY, 0.9, 0.6, 1.4, 0.6, 1.2, Tint(1.12, 0.92, 0.8));
							Elsewhere();
							if (R.Next() < 0.012) Stand(B::Kit0, PX, PY, 0.8, 0.7, 1.1, 0.7, 1.1, 0.85, 1.12, Warm);
							break;
						case Garden:
							if (R.Next() < 0.45 * FGrove) Stand(B::Kit5, X, Y, 0.5, 0.8, 1.2, 0.8, 1.3, 0.85, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.1 * FGrove) Stand(B::Kit4, PX, PY, 0.5, 0.8, 1.1, 0.8, 1.15, 0.9, 1.1, 0.0);
							Elsewhere();
							if (R.Next() < 0.25) Tuft(B::Kit6, PX, PY, 0.6, 0.7, 1.0, 0.4, 0.55, Tint(1.1, 0.97, 0.95));
							break;
						default: KunaiHere(1.0); break;
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
			/** The plumed kunai on trial (Field::Tried("kunai")): one clump in six with its plume, the rest without (KunaiClump). */
			bool bPlumeTrial = false;
			/** The woods gathered into clumps with clearings, islands of trees in the kunai, groves in the yards, shade
			 * painted under the canopy and the jungle's cost cut, on trial (Field::Tried("canopy")), the jungle only (ClumpAt). */
			bool bCanopy = false;
			/** On trial "canopy": the floor's value at a clump's full heart against a clearing's (the plans asked 0.62 and 0.78;
			 * the painter casts every shadow and the Mac few, so this one number is the Mac's to calibrate); kLitter the leaf
			 * litter's own value against the kunai ground's in paint (85 against 114, measured), so the litter is shaded by
			 * kCanopyShade / kLitter; the shade under an island in the kunai, and under a yard's grove. */
			static constexpr double kCanopyShade = 0.68, kLitter = 0.77, kGroveShade = 0.82, kYardShade = 0.90;
			/** On trial "canopy": ClumpAt under this a clearing (kunai); the channel's thresholds (ClumpAt, IsleAt, YardGrove). */
			static constexpr double kClearing = 0.25;
			/** On trial "canopy": the woods' chances on a clump's rim and at its heart (plan-speed's table; Plants): the giant's
			 * and the canopy tree's (its share of U over the giant's; in the band past 70 m CanopyDeep), its size at a heart, the
			 * sub-canopy's, the thicket's, the banana's, the undergrowth's, the young palm's; the rim's skirt; an island's; a
			 * clearing's. */
			static constexpr double GiantRim = 0.04, GiantHeart = 0.10, CanopyRim = 0.25, CanopyHeart = 0.54, CanopyDeep = 0.42;
			static constexpr double ScLoHeart = 1.6, ScHiHeart = 2.4, SzLoHeart = 1.25, SzHiHeart = 2.0, LeafLoHeart = 0.75, LeafHiHeart = 1.05;
			/** On trial "canopy": the woods' face's crowns (giant and canopy tree, of U) in its outer 10 m and behind them to
			 * some 25 m in, and the share of its climbers' curtains hung (Plants). */
			static constexpr double EdgeFace = 0.39, EdgeBack = 0.25, CurtainFace = 0.7;
			static constexpr double SubRim = 0.9, SubHeart = 0.30, SubDeep = 0.18, ThicketRim = 0.55, ThicketHeart = 0.15;
			static constexpr double BananaRim = 0.45, UnderHeart = 0.9, YoungRim = 0.2, SkirtThicket = 0.35, SkirtUnder = 0.5;
			static constexpr double IsleGiant = 0.03, IsleCanopy = 0.45, IsleSub = 0.15, IsleThicket = 1.0, IsleUnder = 0.8, IsleBanana = 0.6;
			static constexpr double GladeBanana = 0.35, GladeYoung = 0.08, GladeUnder = 0.15, GladeSapling = 0.10, GladeTuft = 0.6;
			static constexpr double kClumpLo = -0.06, kClumpHi = 0.24, kIsleLo = 0.57, kIsleHi = 0.63, kYardLo = 0.30, kYardHi = 0.375, kGroveMean = 0.35;
			/** Warm coral sand and a dark wet band at the water on trial (Field::Tried("islandsand")), the islands only. */
			bool bSand = false;
			bool bMainland = false, bClay = false;
			double Dry = 0.5;
			double SeaZ = 0.0;
			double Wf = 0.0, Df = 0.0;
			static constexpr int Chans = 7;
			static constexpr double GStep = 8.0;
			double GX0 = 0.0, GY0 = 0.0;
			int GNX = 0, GNY = 0;
			std::vector<float> Grid;
			};
	}

	std::unique_ptr<Region> MakeTropic(const LandField::Field& F, uint64_t S, bool bIsland) { return std::make_unique<Tropic>(F, S, bIsland); }
}
