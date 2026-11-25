#include "Battle/LandScape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Italy and the Mediterranean (Sicily 1943, the mainland 1943-45, Greece and Crete 1941): the art director's look targets,
 * section 2. The field itself is the valley's floor, the rules' heights kept: small holdings of olives in loose
 * irregular rows, bush vines in their rows, wheat (gold stubble after the July harvest, ploughed or green with the sown
 * corn in winter), dry fallow, maquis on the rough; white dusty roads between dry-stone walls; a stone farmhouse with a
 * low terracotta roof on each holding, its haystacks and its dark cypresses; the village round the rules' town with its
 * bell tower and its walled cemetery of cypresses. Round the field the valley's sides rise in dry-stone terraces, and
 * beyond them the hills, always on the horizon, a hill town on the crest. By the month: Sicily's July (gold stubble,
 * dead straw grass, white dust), the Winter Line's and Cassino's winter (brown mud, wet green-brown grass, bare vines,
 * snow on the high ridges), the spring's green corn, the autumn's turning vines. In the south (Sicily, below 38.6
 * degrees) the recording's woods are old olive groves and its open ground ochre fallow with macchia in clumps, as the
 * art director's dry south set (battle-models/med/sets/drysouth.json); on the mainland its woods are holm oak and the
 * deciduous oaks, bare in the winter. The art director's first read (22 November) mended: small fields of every kind side
 * by side (stubble with its stooks, red ploughland, fallow with its thistle and fennel, olives and almonds in rows),
 * prickly pear and agave at the field edges in the south, the walls laid with the kit's dry stone, the straw stacks round
 * their poles, the ground's own colour drifting lighter and darker, warmer and cooler, softly from some 14 to 45 m (the
 * general's camera, high and oblique, sees the field without its clutter), the winter's wheat an olive green over the
 * wet earth, rain in the plough's furrows, snow on the heights.
 * Kit: battle-models/med; the prickly pear, the agave, the stook, the bare tree and the straw stack built here.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Olive = 0,  // an olive grove: old trees in loose rows, ploughed earth or dry grass under them
			Vine = 1,   // bush vines in rows
			Grain = 2,  // wheat: green in winter, ripe in May and June, stubble after the harvest, ploughed in the autumn
			Fallow = 3, // grazing and fallow: grass, thistles, stones
			Maquis = 4, // the rough: macchia scrub, garrigue and limestone
			Plough = 5, // bare ploughed earth (the summer's worked fallow, the autumn's and the winter's ploughland)
			Almond = 6  // almonds in their rows (the south's)
		};

		enum Slot : int
		{
			KOlive = 0,
			KCypress = 1,
			KMacchia = 2,
			KFarm = 3,
			KTower = 4,
			KStones = 5,
			KBoulder = 6,
			KHouseStone = 7,
			KPole = 8,
			KSangar = 9,
			KWall = 10,  // the kit's dry-stone wall, laid along the walls' lines
			KPear = 11,  // prickly pear (built here)
			KAgave = 12, // agave (built here)
			KStook = 13, // the summer's stooks of wheat sheaves (built here) ...
			KBare = 13,  // ... and the rest of the year the bare broadleaf tree in the same slot
			KStack = 14, // a straw stack round its pole (built here)
			KSpike = 15  // an agave in flower, its dry pole (built here)
		};

		enum Clut : int
		{
			CGrass = 0, // a tuft of grass (straw in summer, green-brown in winter: its tint)
			CStalk = 1, // dry stalks: wild fennel, thistle
			CStone = 2, // pale limestone stones
			CVine = 3,  // a bush vine (in leaf, turning or bare by the month)
			CShrub = 4, // garrigue: a low dark mound of thyme and cistus
			CClod = 5   // ploughed earth's clods
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		const Tone RFresh = Tone{ 92, 116, 58 };

		/** A terrace's step: its riser (metres of height a tread), the share of it the tread falls, the share of a tread's
		 * run the riser takes. */
		constexpr double Rise = 2.2, TreadFall = 0.14, RiserRun = 0.2;

		struct Farm
		{
			double X = 0.0, Y = 0.0, Yaw = 0.0;
			// its track out to the lane, and its threshing floor
			bool bTrack = false;
			double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0;
			double FloorX = 0.0, FloorY = 0.0;
		};

		struct Exit
		{
			double X = 0.0, Y = 0.0, DX = 0.0, DY = 0.0;
		};

		/** One choice of the clutter: a share of the candidates and its kind. */
		struct Share
		{
			double Part;
			LandField::Base Kind;
		};
		Grown Of(double Pick, double Total, std::initializer_list<Share> Parts)
		{
			double Sum = 0.0;
			for (const Share& P : Parts)
			{
				Sum += P.Part;
				if (Pick < Sum) return Grown{ P.Kind, Total };
			}
			return Grown{};
		}

		double SegDist(double PX, double PY, double AX, double AY, double BX, double BY)
		{
			const double DX = BX - AX, DY = BY - AY, L2 = DX * DX + DY * DY;
			const double T = L2 > 1e-9 ? std::clamp(((PX - AX) * DX + (PY - AY) * DY) / L2, 0.0, 1.0) : 0.0;
			return std::hypot(PX - AX - T * DX, PY - AY - T * DY);
		}

		/** A tapered tube from one ring to another (a trunk, a limb, a sheaf, a pole), Sides round, its ends open, its
		 * faces out. */
		void Tube(Writer& W, double X0, double Y0, double Z0, double R0, double X1, double Y1, double Z1, double R1, int Sides, const Tone& C0, const Tone& C1)
		{
			const double AX = X1 - X0, AY = Y1 - Y0, AZ = Z1 - Z0, AL = std::sqrt(AX * AX + AY * AY + AZ * AZ);
			if (AL < 1e-6 || Sides < 3) return;
			const double UX = AX / AL, UY = AY / AL, UZ = AZ / AL;
			// two axes square to it: P level, Q = U x P
			double PX = -UY, PY = UX;
			const double PL = std::hypot(PX, PY);
			if (PL < 1e-6)
			{
				PX = 1.0;
				PY = 0.0;
			}
			else
			{
				PX /= PL;
				PY /= PL;
			}
			const double QX = -UZ * PY, QY = UZ * PX, QZ = UX * PY - UY * PX;
			const uint32_t First = static_cast<uint32_t>(W.M.Colours.size());
			for (int K = 0; K < Sides; ++K)
			{
				const double A = 2.0 * Pi * K / Sides, CA = std::cos(A), SA = std::sin(A);
				const double NX = CA * PX + SA * QX, NY = CA * PY + SA * QY, NZ = SA * QZ;
				W.Vert(X0 + R0 * NX, Y0 + R0 * NY, Z0 + R0 * NZ, NX, NY, NZ, C0);
				W.Vert(X1 + R1 * NX, Y1 + R1 * NY, Z1 + R1 * NZ, NX, NY, NZ, C1);
			}
			for (int K = 0; K < Sides; ++K)
			{
				const uint32_t A0 = First + 2 * K, A1 = A0 + 1, B0 = First + 2 * ((K + 1) % Sides), B1 = B0 + 1;
				W.Tri(A0, B0, B1);
				W.Tri(A0, B1, A1);
			}
		}

		/** A flat oval pad (the prickly pear's, a fennel's umbel): its middle, its long axis A and its short axis B (unit,
		 * square to each other), its half length and half width, Rim points round; both faces, each domed Dome out of
		 * its side, its normals leaning Tilt out to the rim. */
		void Pad(Writer& W, const double C[3], const double A[3], const double B[3], double Long, double Wide, const Tone& Col, double Dome = 0.025, double Tilt = 0.45, int Rim = 8)
		{
			const double N[3] = { A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0] };
			for (int Face = 0; Face < 2; ++Face)
			{
				const double S = Face == 0 ? 1.0 : -1.0;
				const uint32_t Mid = W.Vert(C[0] + Dome * S * N[0], C[1] + Dome * S * N[1], C[2] + Dome * S * N[2], S * N[0], S * N[1], S * N[2], Col);
				for (int K = 0; K < Rim; ++K)
				{
					const double T = 2.0 * Pi * K / Rim, CT = std::cos(T), ST = std::sin(T);
					double P[3], Nr[3];
					for (int I = 0; I < 3; ++I)
					{
						P[I] = C[I] + CT * Long * A[I] + ST * Wide * B[I];
						Nr[I] = S * N[I] + Tilt * (CT * A[I] + ST * B[I]);
					}
					const double L = std::sqrt(Nr[0] * Nr[0] + Nr[1] * Nr[1] + Nr[2] * Nr[2]);
					W.Vert(P[0], P[1], P[2], Nr[0] / L, Nr[1] / L, Nr[2] / L, Varied(Col, 0.94, 0.0));
				}
				for (int K = 0; K < Rim; ++K)
				{
					const uint32_t P0 = Mid + 1 + K, P1 = Mid + 1 + (K + 1) % Rim;
					if (Face == 0) W.Tri(Mid, P0, P1);
					else W.Tri(Mid, P1, P0);
				}
			}
		}

		/** A small round thing (a fruit, a thistle's head): its middle, Rad round and Tall high, Top its colour above and
		 * Foot below; its normals out. */
		void Bead(Writer& W, double X, double Y, double Z, double Rad, double Tall, const Tone& Top, const Tone& Foot)
		{
			constexpr int Round = 6;
			const uint32_t T = W.Vert(X, Y, Z + 0.5 * Tall, 0.0, 0.0, 1.0, Top);
			const uint32_t B = W.Vert(X, Y, Z - 0.5 * Tall, 0.0, 0.0, -1.0, Foot);
			uint32_t Ring[Round];
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round, CA = std::cos(A), SA = std::sin(A);
				Ring[K] = W.Vert(X + Rad * CA, Y + Rad * SA, Z, CA, SA, 0.0, Mixed(Foot, Top, 0.55));
			}
			for (int K = 0; K < Round; ++K)
			{
				W.Tri(T, Ring[K], Ring[(K + 1) % Round]);
				W.Tri(B, Ring[(K + 1) % Round], Ring[K]);
			}
		}

		class Med final : public Region
		{
		public:
			Med(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				bWallTrial = F.Tried("walltop");
				bStoneTrial = F.Tried("medwall");
				FW = F.Width();
				FH = F.Depth();
				const int M = std::clamp(F.Month, 1, 12) - 1;
				// (the year in the valleys: the summer drought from June to September, the rains from October, mud from
				// November to March, the green of the corn and the grass from the autumn rains to May)
				static const double DryOf[12] = { 0.0, 0.0, 0.05, 0.2, 0.5, 0.85, 1.0, 1.0, 0.85, 0.45, 0.1, 0.0 };
				static const double WetOf[12] = { 1.0, 1.0, 0.75, 0.4, 0.1, 0.0, 0.0, 0.0, 0.05, 0.3, 0.8, 1.0 };
				static const double GreenOf[12] = { 0.45, 0.5, 0.75, 0.95, 0.7, 0.25, 0.05, 0.0, 0.05, 0.3, 0.45, 0.45 };
				static const double LeafOf[12] = { 0.0, 0.0, 0.0, 0.3, 0.8, 1.0, 1.0, 1.0, 1.0, 0.9, 0.35, 0.0 };
				static const double TurnOf[12] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.05, 0.2, 0.7, 1.0, 0.0 };
				static const int StageOf[12] = { 0, 0, 0, 0, 1, 2, 2, 2, 2, 3, 3, 0 };
				Dry = DryOf[M];
				Wet = WetOf[M];
				Green = GreenOf[M];
				Leaf = LeafOf[M];
				Turn = TurnOf[M];
				Stage = StageOf[M];
				bSouth = F.Lat < 38.6;
				// (the prickly pear and the agave at the field edges and round the farms: Sicily's, the far south's)
				bPears = F.Lat < 40.9;
				// (the far ridges white in the winter months: Monte Cairo over Cassino)
				bSnowCaps = (M <= 2 || M == 11) && F.Lat > 39.5;
				Dim = bSouth ? 1.0 : 1.0 - 0.3 * Wet;

				// The ground's broad mottle and the soil's red patches read once onto an 8 m grid over the field and 400 m
				// round it (the paint asks them at every point of every tile: four noises each is too dear).
				MNX = static_cast<int>((FW + 2.0 * MotMargin) / MotStep) + 2;
				MNY = static_cast<int>((FH + 2.0 * MotMargin) / MotStep) + 2;
				MotGrid.resize(4 * static_cast<size_t>(MNX) * static_cast<size_t>(MNY));
				for (int J = 0; J < MNY; ++J)
					for (int I = 0; I < MNX; ++I)
					{
						float* P = &MotGrid[4 * (static_cast<size_t>(J) * static_cast<size_t>(MNX) + static_cast<size_t>(I))];
						double Mo[4];
						MotRaw(-MotMargin + I * MotStep, -MotMargin + J * MotStep, Mo);
						for (int C = 0; C < 4; ++C) P[C] = static_cast<float>(Mo[C]);
					}

				// The hill town's crest: beyond the field, behind the rules' town as the attack looks at it.
				const double TX = F.bTown ? F.TownX : 0.5 * FW;
				HillX = std::clamp(TX - 220.0, 150.0, FW - 150.0);
				HillY = FH + 620.0;
				HillTop = 0.0;
				{
					const double D = Out(HillX, HillY);
					HillTop = Hills(HillX, HillY, D, SideAt(HillX, HillY)) + 110.0;
				}

				// The roads out of the field (LandField runs each lane that reaches the field's edge on out through the
				// band, straight, drifting up to 30 m aside): the terraces leave them a way up.
				for (int J = 0; J < F.Rows; ++J)
					for (int I = 0; I < F.Cols; ++I)
					{
						if (I != 0 && J != 0 && I != F.Cols - 1 && J != F.Rows - 1) continue;
						const size_t K = static_cast<size_t>(J) * F.Cols + I;
						if (K >= F.Ground.size()) continue;
						const int G = F.Ground[K];
						if (G < 0 || static_cast<size_t>(G) >= F.Grounds.size()) continue;
						const std::string& Word = F.Grounds[static_cast<size_t>(G)];
						if (Word != "road" && Word != "bridge") continue;
						Exit E;
						E.DX = I == 0 ? -1.0 : I == F.Cols - 1 ? 1.0 : 0.0;
						E.DY = E.DX != 0.0 ? 0.0 : J == 0 ? -1.0 : 1.0;
						E.X = (I + 0.5) * F.Cell + E.DX * 0.5 * F.Cell;
						E.Y = (J + 0.5) * F.Cell + E.DY * 0.5 * F.Cell;
						bool bNear = false;
						for (const Exit& Q : Exits)
							if (Q.DX == E.DX && Q.DY == E.DY && std::hypot(Q.X - E.X, Q.Y - E.Y) < 30.0) bNear = true;
						if (!bNear) Exits.push_back(E);
					}
			}

			Look Which() const override { return Look::Med; }

			// ------------------------------------------------------------------ the land

			/** Metres outside the field (0 inside). */
			double Out(double X, double Y) const
			{
				const double DX = std::max({ 0.0, -X, X - FW }), DY = std::max({ 0.0, -Y, Y - FH });
				return std::sqrt(DX * DX + DY * DY);
			}
			/** How high the valley's sides rise round the field (they differ side to side). */
			double SideAt(double X, double Y) const { return 58.0 + 22.0 * Noise(X / 1500.0, Y / 1500.0, Seed ^ 0x5D1); }
			/** The land's rise beyond the field: the valley's sides, then the hills. */
			double Hills(double X, double Y, double D, double Side) const
			{
				double E = Side * Smooth(0.0, 620.0, D);
				if (D > 300.0)
				{
					const double F = Fbm(X, Y, 2300.0, 2, Seed ^ 0x6B1);
					const double Ridge = 1.0 - std::fabs(Noise(X / 1600.0 + 0.37, Y / 1600.0 - 0.21, Seed ^ 0x6B2));
					const double Mount = 120.0 + 130.0 * F + 110.0 * Ridge * Ridge;
					// (on the mainland the Apennines stand behind, their ridges some 800 m and more four kilometres out:
					// Monte Cairo's over Cassino)
					const double Range = bSouth ? 1.0 : 1.0 + 1.5 * Smooth(2000.0, 4600.0, D) * (0.55 + 0.45 * Ridge);
					E += std::max(40.0, Mount) * Range * Smooth(300.0, 2700.0, D);
				}
				return E;
			}
			/** The hill town's crest's share of its height at a place (C its distance from the crest, east-west stretched;
			 * D outside the field): a flat top, steep sides falling some 500 m round, nothing at the field's edge. */
			static double HillPart(double C, double D) { return Smooth(600.0, 120.0, C) * Smooth(100.0, 300.0, D); }
			/** The distance from the hill town's crest as HillPart takes it: stretched east-west, and the face toward the
			 * field steep (its foot at the band's outer edge, so the band below it is terraced as the valley's sides are). */
			double CrestC(double X, double Y) const
			{
				const double DY = Y - HillY;
				return std::hypot((X - HillX) / 1.7, DY < 0.0 ? DY / 0.62 : DY);
			}
			/** The same with the hill town's crest. */
			double Lift(double X, double Y, double D, double Side) const
			{
				const double E = Hills(X, Y, D, Side);
				const double C = CrestC(X, Y);
				if (C >= 600.0) return E;
				const double Crest = HillTop * HillPart(C, D);
				return 0.5 * (E + Crest + std::sqrt((E - Crest) * (E - Crest) + 400.0));
			}
			/** How much of a terrace's step is laid here (0 none, 1 in full): where the valley's side is steep enough to
			 * terrace, within the band (the ring's mesh cannot hold them), not all of it (some slopes left to the maquis),
			 * and never across a road's way up. */
			double Terraced(double X, double Y, double D, double Side) const
			{
				if (D < 40.0 || D > 345.0) return 0.0;
				const double T = D / 620.0;
				const double Slope = Side * 6.0 * T * (1.0 - T) / 620.0;
				double W = Smooth(0.05, 0.085, Slope) * Smooth(345.0, 285.0, D);
				if (W <= 0.0) return 0.0;
				W *= Smooth(-0.15, 0.1, Noise(X / 520.0, Y / 520.0, Seed ^ 0x7E1));
				// (not up the hill town's steep sides: scrub and rock there)
				W *= Smooth(0.08, 0.02, HillPart(CrestC(X, Y), D));
				if (W <= 0.0) return 0.0;
				for (const Exit& E : Exits)
				{
					const double RX = X - E.X, RY = Y - E.Y;
					const double Along = RX * E.DX + RY * E.DY;
					if (Along < -30.0) continue;
					const double Across = std::fabs(-RX * E.DY + RY * E.DX);
					const double Room = 12.0 + 0.12 * std::max(0.0, Along);
					W *= Smooth(Room, Room + 16.0, Across);
				}
				return W;
			}
			/** A terrace's step at a height: a tread falling gently outward, then the riser (its deviation from the slope). */
			static double Step(double E)
			{
				const double U = E / Rise, F = U - std::floor(U);
				const double P = F < 1.0 - RiserRun ? TreadFall * F / (1.0 - RiserRun) : TreadFall + (1.0 - TreadFall) * Smooth(1.0 - RiserRun, 1.0, F);
				return Rise * (P - F);
			}

			double Relief(double X, double Y, double Z) const override
			{
				const double D = Out(X, Y);
				if (D <= 0.0) return Z; // (the field: the rules' own heights)
				const double Side = SideAt(X, Y);
				const double E = Lift(X, Y, D, Side);
				const double W = Terraced(X, Y, D, Side);
				return Z + E + (W > 0.0 ? W * Step(E) : 0.0);
			}
			double Rolling(double X, double Y, double MeanZ) const override { return MeanZ + 5.0 * Fbm(X, Y, 1500.0, 2, Seed ^ 0xB1); }
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.3; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.0 : 2.4) + 0.8 * U; }

			// ------------------------------------------------------------------ the parcels

			// (small fields, some 50 to 100 m: the art director's dry south set's 30 to 90)
			double PlotMin() const override { return 9000.0; }
			double PlotMax() const override { return 17000.0; }
			double FarTilled() const override { return 0.55; }
			double WoodsEdge() const override { return 0.32; }
			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Maquis;
				// (every kind side by side, so no view is one fallow: the wheat, its stubble in the summer, the most; the
				// ploughland, the fallow, the olives; the south's almonds, the mainland's vines)
				if (bSouth)
				{
					if (Kind == KField) return U < 0.14 ? Olive : U < 0.22 ? Vine : U < 0.29 ? Almond : U < 0.72 ? Grain : U < 0.94 ? Plough : Fallow;
					return U < 0.3 ? Fallow : U < 0.6 ? Grain : U < 0.76 ? Plough : U < 0.86 ? Olive : U < 0.93 ? Almond : Maquis;
				}
				if (Kind == KField) return U < 0.18 ? Olive : U < 0.34 ? Vine : U < 0.72 ? Grain : U < 0.94 ? Plough : Fallow;
				return U < 0.34 ? Fallow : U < 0.6 ? Grain : U < 0.76 ? Plough : U < 0.88 ? Olive : Maquis;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Olive: return "olives";
				case Vine: return "vines";
				case Grain: return Stage == 2 ? "stubble" : Stage == 3 ? "ploughed" : "wheat";
				case Fallow: return "fallow";
				case Maquis: return "maquis";
				case Plough: return "ploughed";
				case Almond: return "almonds";
				default: return "";
				}
			}

			// ------------------------------------------------------------------ colours

			/** The ground's own texture, all soft, each noise bent by a broader one and by the others so no lattice shows
			 * (-1..1 each): M its patches some 45 m across, which the crops' grounds read; Mid drifts lighter and darker
			 * some 26 m across, Fine some 14 m (gently: from above, on the map, it reads as grain); Warm drifts warmer and cooler some 36 m across. Nothing finer:
			 * the near ground's points are 4 m apart, and LandField's own mottle (N2, 7 m) draws on them as a dither (the
			 * art director's read). From the general's camera, high and oblique, the clutter has dropped out and this is
			 * the breakup the field shows (the far ground, its points 17 m apart, keeps M, Mid and Warm). */
			struct Mottle
			{
				double M = 0.0, Mid = 0.0, Fine = 0.0, Warm = 0.0;
			};
			/** M, Mid, Warm and the soil's red noise at a place, worked out (the grid's points; beyond it). */
			void MotRaw(double X, double Y, double Out[4]) const
			{
				// (bent by a broad noise of their own, some 90 m)
				const double Bend = Noise(X / 90.0 + 3.1, Y / 90.0 - 1.7, Seed ^ 0x9A6);
				Out[0] = Noise((X + 14.0 * Bend) / 46.0, (Y - 10.0 * Bend) / 46.0, Seed ^ 0x9A1);
				// (Mid on a lattice turned off the field's square, 37 degrees)
				const double U = 0.8 * X - 0.6 * Y, V = 0.6 * X + 0.8 * Y;
				Out[1] = Noise((V + 8.0 * Out[0]) / 26.0, (9.0 * Bend - U) / 26.0, Seed ^ 0x9A5);
				Out[2] = Noise((X + 9.0 * Bend) / 36.0, (Y + 6.0 * Out[0]) / 36.0, Seed ^ 0x9A4);
				Out[3] = Noise(X / 380.0, Y / 380.0, Seed ^ 0x501);
			}
			/** The grid's channel C here, read between its points (worked out beyond it). */
			double MotRead(double X, double Y, int C) const
			{
				const double U = (X + MotMargin) / MotStep, V = (Y + MotMargin) / MotStep;
				if (!(U >= 0.0 && V >= 0.0 && U < MNX - 1.0 && V < MNY - 1.0))
				{
					double Mo[4];
					MotRaw(X, Y, Mo);
					return Mo[C];
				}
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = 4 * (static_cast<size_t>(J) * static_cast<size_t>(MNX) + static_cast<size_t>(I)) + static_cast<size_t>(C);
				const size_t Row = 4 * static_cast<size_t>(MNX);
				const double A = MotGrid[K], B = MotGrid[K + 4], Cc = MotGrid[K + Row], D = MotGrid[K + Row + 4];
				return Lerp(Lerp(A, B, FU), Lerp(Cc, D, FU), FV);
			}
			Mottle MottleAt(double X, double Y, double N1) const
			{
				Mottle Out;
				const double U = (X + MotMargin) / MotStep, V = (Y + MotMargin) / MotStep;
				if (U >= 0.0 && V >= 0.0 && U < MNX - 1.0 && V < MNY - 1.0)
				{
					const int I = static_cast<int>(U), J = static_cast<int>(V);
					const double FU = U - I, FV = V - J;
					const size_t K = 4 * (static_cast<size_t>(J) * static_cast<size_t>(MNX) + static_cast<size_t>(I));
					const size_t Row = 4 * static_cast<size_t>(MNX);
					auto Read = [&](size_t C) { return Lerp(Lerp(MotGrid[K + C], MotGrid[K + 4 + C], FU), Lerp(MotGrid[K + Row + C], MotGrid[K + Row + 4 + C], FU), FV); };
					Out.M = Read(0);
					Out.Mid = Read(1);
					Out.Warm = Read(2);
				}
				else
				{
					double Mo[4];
					MotRaw(X, Y, Mo);
					Out.M = Mo[0];
					Out.Mid = Mo[1];
					Out.Warm = Mo[2];
				}
				// (Fine worked out here, bent by LandField's 30 m patches and by Mid; its lattice turned 53 degrees)
				const double U2 = 0.8 * X - 0.6 * Y, V2 = 0.6 * X + 0.8 * Y;
				Out.Fine = Noise((U2 - 6.0 * N1 + 5.0 * Out.Mid) / 14.0, (V2 + 7.0 * N1) / 14.0, Seed ^ 0x9A3);
				return Out;
			}
			/** A ground's colour textured by its mottle (a field's lightness spread near some 30 to 40 per cent from its
			 * darkest drifts to its lightest, as a field's texture, warmer and cooler by turns) and the country's broad
			 * noise. */
			static Tone Textured(const Tone& T, const Mottle& Mo, double Broad)
			{
				return Varied(T, 1.0 + 0.12 * Mo.Fine + 0.17 * Mo.Mid + 0.16 * Mo.M + 0.05 * Broad, 0.1 * Mo.Warm + 0.03 * Mo.Mid + 0.015 * Broad);
			}

			/** The earth: a pale limestone soil, or the red-brown terra rossa in broad patches; wet in the winter. */
			Tone Soil(double X, double Y, double N1) const
			{
				// (Sicily's interior mostly the pale grey-brown of its clays and marls, the red in patches)
				const double Red = Smooth(-0.2, 0.3, MotRead(X, Y, 3) + 0.15 * N1 + (bSouth ? -0.12 : 0.05));
				const Tone Pale = Mixed(Rgb(176, 162, 134), Rgb(192, 184, 164), Smooth(-0.4, 0.9, N1));
				const Tone T = Mixed(Pale, Rgb(162, 96, 64), Red);
				// (the winter's rain darkens it to a damp red-brown, never toward black: the art director on Algiers in
				// December, 24 Nov, where it measured 58,49,39 against a damp terra rossa's ~130,95,70)
				return Mixed(T, Rgb(150, 100, 70), 0.35 * Wet);
			}
			/** Ploughed earth (M the soft mottle): the furrows' shade and the clods' lit tops, darker and browner wet. */
			Tone Ploughed(const Tone& Earth, double M) const
			{
				return Varied(Mixed(Earth, Mixed(Rgb(112, 92, 72), Rgb(140, 98, 70), Wet), 0.4 + 0.04 * Wet), 0.92 + 0.08 * M, 0.0);
			}
			/** Grass by the month: dead straw in the summer, wet green-brown in the winter, green in the spring. */
			Tone Grass(double N1, double M) const
			{
				// (ochre, the art director's fallow 9C925C round it; its patches of thistle and dead weed green-brown, soft
				// and some 45 m across)
				Tone Summer = Mixed(Rgb(150, 136, 92), Rgb(184, 166, 114), Smooth(-0.8, 0.8, 0.5 * N1 - 0.5 * M));
				Summer = Mixed(Summer, Rgb(104, 102, 64), 0.6 * Smooth(0.15, 0.65, M));
				// (the winter's: green-brown after the rains, last summer's dead grass in brown patches, never lime)
				Tone Winter = Mixed(Rgb(126, 114, 80), Rgb(104, 114, 66), Clamp01(Green + 0.1 + 0.2 * N1));
				Winter = Mixed(Winter, Rgb(134, 114, 82), 0.55 * Smooth(0.1, 0.6, -M));
				return Mixed(Winter, Summer, Dry);
			}
			Tone Dust() const { return Mixed(Rgb(156, 142, 120), Rgb(230, 222, 204), Dry); }

			/** A crop's ground (M the soft mottle, N1 LandField's 30 m patches, used gently). */
			Tone Ground(int C, double X, double Y, double N1, double M, uint64_t Key) const
			{
				const Tone Earth = Soil(X, Y, N1);
				switch (C)
				{
				case Olive:
				case Almond:
				{
					// (most groves ploughed clean under the trees, the rest left to the grass)
					if (Unit(Mix(Key ^ 0x01E1)) < (C == Almond ? 0.8 : 0.55)) return Mixed(Ploughed(Earth, M), Grass(N1, M), 0.12 + 0.2 * Smooth(0.0, 0.8, M));
					return Mixed(Grass(N1, M), Earth, 0.25 + 0.2 * Smooth(-0.2, 0.6, -M));
				}
				case Vine:
				{
					const Tone Leaves = Mixed(Rgb(92, 114, 52), Mixed(Rgb(198, 162, 66), Rgb(156, 80, 46), 0.4 + 0.3 * N1), Turn);
					const Tone Stocks = Rgb(112, 96, 78);
					return Mixed(Mixed(Ploughed(Earth, M), Stocks, 0.12), Leaves, 0.32 * Leaf);
				}
				case Grain:
					switch (Stage)
					{
					case 2:
					{
						// gold stubble (the art director's B59961): straw over pale earth, the earth through it here and there
						const Tone Straw = Mixed(Rgb(172, 150, 102), Rgb(196, 176, 126), Smooth(-0.7, 0.7, 0.5 * N1 - 0.5 * M));
						return Mixed(Straw, Earth, 0.16 + 0.28 * Smooth(0.0, 0.8, M));
					}
					case 3: return Ploughed(Earth, M);
					case 1: return Mixed(Rgb(150, 156, 78), Rgb(204, 182, 106), Clamp01(0.5 + 0.5 * M));
					default:
					{
						// the sown corn: a fine fresh green over the dark earth from the winter on, its rows closing by the
						// spring
						// (in February half the earth still showing between the rows; an olive green, never lime)
						const double Cover = Clamp01(0.25 + 0.5 * Green + 0.12 * M);
						return Mixed(Ploughed(Earth, M), Mixed(Rgb(80, 110, 52), Rgb(96, 120, 60), 0.5 + 0.5 * N1), Cover);
					}
					}
				case Maquis:
				{
					// thin soil, limestone breaking through, the scrub's dark clumps
					const Tone T = Mixed(Mixed(Rgb(154, 136, 98), Earth, 0.3), Rgb(204, 196, 176), 0.5 * Smooth(0.15, 0.7, -M));
					return Mixed(T, Mixed(Rgb(78, 94, 56), Rgb(88, 100, 62), Wet), 0.5 * Smooth(0.0, 0.6, M));
				}
				case Plough: return Ploughed(Earth, M);
				default:
				{
					// grazing and fallow: dead grass, the bare stony earth showing through in patches, the macchia's thin
					// stony ground and its scrub in others
					const Tone Bare = Mixed(Earth, Rgb(190, 182, 162), 0.25);
					const Tone T = Mixed(Grass(N1, M), Bare, 0.1 + 0.35 * Smooth(-0.35, -0.85, M));
					const Tone Scrubby = Mixed(Mixed(Rgb(142, 126, 89), Earth, 0.2), Rgb(96, 100, 64), 0.3);
					return Mixed(T, Scrubby, 0.45 * Smooth(0.35, 0.8, 0.5 * N1 + 0.5 * M));
				}
				}
			}

			/** The farms' yards, threshing floors and tracks (Sites lays them): T where none is near. */
			Tone Farmyard(const Tone& T, double X, double Y, double M) const
			{
				if (FarmGrid.empty()) return T;
				const int I = static_cast<int>(std::floor((X + Margin) / FarmCell)), J = static_cast<int>(std::floor((Y + Margin) / FarmCell));
				if (I < 0 || J < 0 || I >= FarmNX || J >= FarmNY) return T;
				Tone Out = T;
				for (const uint32_t K : FarmGrid[static_cast<size_t>(J) * FarmNX + I])
				{
					const Farm& F = Farms[K];
					const double D = std::hypot(X - F.X, Y - F.Y);
					// beaten earth round the house, dust in the summer, mud in the winter
					if (D < 26.0) Out = Mixed(Out, Mixed(Rgb(104, 88, 70), Rgb(204, 188, 156), Dry), 0.85 * Smooth(24.0, 12.0, D + 3.0 * M));
					// the threshing floor: a round of laid stones, straw on it after the harvest
					const double Q = std::hypot(X - F.FloorX, Y - F.FloorY);
					if (Q < 7.0) Out = Mixed(Out, Mixed(Rgb(176, 168, 150), Rgb(218, 200, 152), 0.6 * Dry), Smooth(7.0, 5.5, Q));
					if (F.bTrack)
					{
						const double R = SegDist(X, Y, F.AX, F.AY, F.BX, F.BY);
						if (R < 2.6) Out = Mixed(Out, Varied(Dust(), 0.96, 0.0), 0.9 * Smooth(2.6, 1.1, R + 0.3 * M));
					}
				}
				return Out;
			}

			/** The way a parcel's strips and its rows run (DC, DS, a unit): the nearest road's way where it lies, turned up
			 * to 10 degrees the parcel's own way, so an area's strips share one or two ways (the art director's read of the
			 * mend, 22 November: the map shattered when each ran its parcel's own way). */
			void StripWay(const Spot& S, double& DC, double& DS) const
			{
				if (RoadDir.empty())
				{
					DC = std::cos(S.Angle);
					DS = std::sin(S.Angle);
					return;
				}
				const int I = std::clamp(static_cast<int>(std::floor((S.X + RoadMargin) / RoadStep)), 0, RNX - 1);
				const int J = std::clamp(static_cast<int>(std::floor((S.Y + RoadMargin) / RoadStep)), 0, RNY - 1);
				const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(RNX) + static_cast<size_t>(I));
				const double RC = RoadDir[K], RS = RoadDir[K + 1];
				const double Turn_ = 0.17 * (2.0 * Unit(Mix(S.Key ^ 0x57A4)) - 1.0), TC = 1.0 - 0.5 * Turn_ * Turn_;
				DC = RC * TC - RS * Turn_;
				DS = RS * TC + RC * Turn_;
			}

			/** The crop at a place: its parcel's, or where an arable parcel is worked in strips (a holding's wheat field part
			 * stubble, part ploughed already, a strip left fallow) its strip's: strips 26 to 42 m wide along the nearest
			 * road's way give or take 10 degrees (StripWay), each its own, so the fields read some 30 to 90 m (LandField's
			 * parcels are 0.8 ha at the least). Some three arable parcels in five are. */
			int CropAt(const Spot& S) const
			{
				const int C = S.Crop >= 0 ? S.Crop : S.Kind == KRough ? Maquis : S.Kind == KWoods ? Olive : Fallow;
				if ((C != Grain && C != Fallow && C != Plough) || S.Key == 0 || (S.Kind != KOpen && S.Kind != KField)) return C;
				if (Unit(Mix(S.Key ^ 0x57A1)) > 0.6) return C;
				const double W = 26.0 + 16.0 * Unit(Mix(S.Key ^ 0x57A2));
				double DC = 1.0, DS = 0.0;
				StripWay(S, DC, DS);
				const int64_t Strip = static_cast<int64_t>(std::floor((DC * S.Y - DS * S.X) / W + Unit(Mix(S.Key ^ 0x57A5))));
				// (by turns: the wheat's, the plough's, the fallow's; every other strip another's than the parcel's own, the
				// rest the parcel's own on half of them, two or three side by side then reading as one wider field)
				const int Of3 = C == Grain ? 0 : C == Plough ? 1 : 2;
				const double Turn_ = Unit(Hash(S.Key ^ 0x57A3, static_cast<uint64_t>(Strip + 1000)));
				const int Step = Strip % 2 != 0 ? (Turn_ < 0.5 ? 1 : 2) : Turn_ < 0.5 ? 0 : Turn_ < 0.75 ? 1 : 2;
				static const int Turns[3] = { Grain, Plough, Fallow };
				return Turns[(Of3 + Step) % 3];
			}

			Tone Colour(const Spot& S) const override
			{
				Tone T;
				switch (S.Kind)
				{
				case KLane:
				{
					// a white road: its crown and its wheel ruts, its dust on the verges (in the winter a grey-brown slurry,
					// the ruts wet)
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = std::exp(-((Off - 0.85) / 0.28) * ((Off - 0.85) / 0.28));
					const Tone Crown = Varied(Dust(), 1.0 + 0.03 * S.N2, 0.0);
					const Tone Pressed = Mixed(Rgb(102, 90, 74), Rgb(204, 196, 176), Dry);
					T = Mixed(Crown, Pressed, 0.55 * Rut);
					if (Wet > 0.5 && S.N2 > 0.35) T = Mixed(T, Rgb(92, 94, 96), 0.6 * Rut * Smooth(0.35, 0.6, S.N2)); // (puddles)
					// off the floor: the verge, dusty
					if (S.Lane > -0.4) T = Mixed(T, Mixed(Grass(S.N1, 0.0), Dust(), 0.45 * Dry), Smooth(-0.4, 1.6, S.Lane));
					return T;
				}
				case KVillage:
				{
					// the village's yards and gardens: beaten earth and dust, the gardens' rows of beans and tomatoes green
					// against it even in the summer (watered from the well)
					const Mottle Mo = MottleAt(S.X, S.Y, S.N1);
					const Tone Earth = Mixed(Rgb(118, 100, 80), Rgb(194, 176, 142), Dry);
					T = Mixed(Earth, Soil(S.X, S.Y, S.N1), 0.35);
					T = Mixed(T, Mixed(RFresh, Rgb(112, 122, 66), 0.5 * Dry), 0.7 * Smooth(-0.05, 0.3, S.N1) * Smooth(-0.6, 0.0, Mo.M));
					return Textured(T, Mo, 0.0);
				}
				case KYard: return Varied(Mixed(Rgb(116, 100, 80), Rgb(202, 186, 152), Dry), 1.0 + 0.05 * S.N1, 0.0);
				case KWater: return Mixed(Rgb(84, 98, 90), Rgb(96, 104, 96), Dry);
				case KMarsh: return Varied(Mixed(Rgb(98, 108, 66), Rgb(158, 148, 98), Dry), 1.0 + 0.06 * S.N1, 0.0);
				case KWorks:
				{
					// (the positions on ground too hard to dig: the macchia's stony ground, the spoil of what was dug in
					// pale heaps)
					const Mottle Mo = MottleAt(S.X, S.Y, S.N1);
					T = Ground(Maquis, S.X, S.Y, S.N1, Mo.M, S.Key);
					return Textured(Mixed(T, Rgb(196, 188, 166), 0.5 * Smooth(0.1, 0.6, S.N1)), Mo, 0.0);
				}
				case KWoods:
					if (bSouth) break; // (the south's woods are its old olive groves: their ground as theirs)
					// (the oak woods' floor: dead leaves and the evergreen undergrowth)
					T = Mixed(Mixed(Rgb(110, 98, 72), Rgb(92, 80, 62), Wet), Rgb(78, 92, 56), 0.35 + 0.3 * S.N1);
					return Textured(T, MottleAt(S.X, S.Y, S.N1), S.Broad);
				default: break;
				}
				const int C = CropAt(S);
				const Mottle Mo = MottleAt(S.X, S.Y, S.N1);
				const double M = Mo.M;
				T = Ground(C, S.X, S.Y, S.N1, M, S.Key);
				// (the roads' dust laid on the ground beside them)
				if (S.Lane < 3.5) T = Mixed(T, Dust(), 0.45 * Dry * Smooth(3.5, 0.0, S.Lane));
				// (the winter's mud on the valley floor: the roads' verges and the field gateways churned, the low wet
				// patches)
				if (Wet > 0.4)
				{
					const double Churned = std::max(0.75 * Smooth(7.0, 0.5, S.Lane), 0.35 * Smooth(2.5, 0.3, S.Edge));
					T = Mixed(T, Rgb(122, 88, 64), Wet * std::max(Churned, 0.6 * Smooth(0.5, 0.85, -M)));
					// (the rain lying in the plough's furrows, grey with the sky, where the ground is lowest: from afar a
					// sheen in drifts across the field)
					if (C == Plough || (C == Grain && Stage == 3)) T = Mixed(T, Rgb(124, 130, 136), 0.4 * Wet * Smooth(0.2, 0.75, -Mo.Mid));
				}
				T = Farmyard(T, S.X, S.Y, M);
				return Textured(T, Mo, S.Broad);
			}

			/** The land's rise beyond the field and its slope there (from the coarse grid Sites lays, else worked out). */
			void LiftAt(double X, double Y, double& OutE, double& OutSlope) const
			{
				if (LiftGrid.empty())
				{
					const double Side = SideAt(X, Y);
					OutE = Lift(X, Y, Out(X, Y), Side);
					const double EX = Lift(X + 30.0, Y, Out(X + 30.0, Y), Side), EY = Lift(X, Y + 30.0, Out(X, Y + 30.0), Side);
					OutSlope = std::hypot(EX - OutE, EY - OutE) / 30.0;
					return;
				}
				const double U = (X - GridX0) / GridStep, V = (Y - GridY0) / GridStep;
				const int I = std::clamp(static_cast<int>(std::floor(U)), 0, GridNX - 2), J = std::clamp(static_cast<int>(std::floor(V)), 0, GridNY - 2);
				const double A = Clamp01(U - I), B = Clamp01(V - J);
				const size_t K = static_cast<size_t>(J) * GridNX + I;
				const double E00 = LiftGrid[K], E10 = LiftGrid[K + 1], E01 = LiftGrid[K + GridNX], E11 = LiftGrid[K + GridNX + 1];
				OutE = Lerp(Lerp(E00, E10, A), Lerp(E01, E11, A), B);
				OutSlope = std::hypot(Lerp(E10 - E00, E11 - E01, B), Lerp(E01 - E00, E11 - E10, A)) / GridStep;
			}

			Tone Far(const Spot& S, bool bWoods, double Gap) const override
			{
				(void)Gap;
				double E = 0.0, Slope = 0.0;
				LiftAt(S.X, S.Y, E, Slope);
				// (the mainland's oak woods grey-brown in the winter, bare among their holm oaks)
				const Tone Canopy = Mixed(Mixed(Rgb(58, 72, 44), Rgb(70, 78, 54), 0.5 + 0.5 * S.N1), Rgb(92, 84, 74), bSouth ? 0.0 : 0.55 * (1.0 - Leaf));
				Tone T;
				if (bWoods && bSouth) T = Mixed(Mixed(Rgb(108, 116, 84), Soil(S.X, S.Y, S.N1), 0.35), Canopy, 0.25); // (old olive groves)
				else if (bWoods) T = Canopy;
				else
				{
					const int C = S.Crop >= 0 ? S.Crop : Fallow;
					T = Ground(C, S.X, S.Y, S.N1, 0.0, S.Key);
					// (the hills: their middle slopes terraced and grey-green with olives, the steep ones scrub and rock, the
					// high ground under oak and chestnut woods and the maquis, dark)
					const double Steep = Smooth(0.12, 0.3, Slope + 0.05 * S.N1);
					const double Mid = Smooth(0.045, 0.11, Slope) * (1.0 - Steep);
					const Tone Grove = Mixed(Mixed(Rgb(124, 132, 98), Soil(S.X, S.Y, S.N1), 0.3), Grass(S.N1, 0.0), 0.15);
					T = Mixed(T, Grove, 0.7 * Mid * Smooth(-0.5, 0.0, S.N1));
					const Tone Scrub = Mixed(Rgb(66, 82, 50), Rgb(176, 170, 150), Smooth(0.2, 0.7, S.N1));
					T = Mixed(T, Scrub, Steep);
					const double High = Smooth(30.0, 120.0, E) * Smooth(-0.15, 0.25, Noise(S.X / 650.0, S.Y / 650.0, Seed ^ 0x3D1) + 0.3 * S.N1);
					T = Mixed(T, Canopy, 0.85 * High);
				}
				// (the snow on the high ground: the Apennines' ridges behind white, the hill town's crest (some 200 m over the
				// valley) dusted in patches, thinner on the steep)
				if (bSnowCaps) T = Mixed(T, Rgb(228, 230, 236), Smooth(120.0, 200.0, E + 40.0 * S.N1) * (0.7 + 0.3 * Smooth(200.0, 400.0, E)) * (1.0 - 0.45 * Smooth(0.3, 0.6, Slope)));
				return Varied(T, 1.0 + 0.05 * S.Broad, 0.01 * S.Broad);
			}

			// ------------------------------------------------------------------ the clutter round the eye

			/** The vines' rows in a parcel: how far apart, and where the first runs. */
			static double RowGap(uint64_t Key) { return 2.0 + 0.4 * Unit(Mix(Key ^ 0x71E5)); }
			static double RowNear(const Spot& S)
			{
				const double G = RowGap(S.Key), A = S.Across - G * Unit(Mix(S.Key ^ 0x71E6));
				return A - G * std::round(A / G);
			}

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Drawn == 9 || S.Drawn == 11 || S.Kind == KWater) return Grown{};
				const LandField::Base Grass_ = ClutOf(CGrass), Stalk = ClutOf(CStalk), Stone = ClutOf(CStone), Shrub = ClutOf(CShrub), Clod = ClutOf(CClod);
				if (S.Kind == KLane || S.Drawn == 7)
				{
					if (S.Lane < -0.7) return Of(Pick, 0.06, { { 0.8, Stone }, { 0.2, Clod } }); // (the road: a few stones)
					return Of(Pick, 0.55, { { 0.7, Grass_ }, { 0.18, Stalk }, { 0.12, Stone } });
				}
				// (in patches: the spot comes without the ground's noises, so its own, cheap)
				const double Clump = Smooth(-0.4, 0.4, Noise(S.X / 5.3, S.Y / 5.3, Seed ^ 0xC1));
				const double Thick = 0.55 + 0.9 * Clump;
				if (S.Drawn == 10) return Of(Pick, 1.0, { { 0.45, Grass_ }, { 0.25, Stalk }, { 0.2, Stone }, { 0.1, Shrub } }); // (a wall's foot: weeds, fallen stones)
				switch (S.Kind)
				{
				case KVillage:
				case KYard: return Of(Pick, 0.18, { { 0.6, Grass_ }, { 0.4, Stone } });
				case KWorks: return Of(Pick, 0.15, { { 1.0, Stone } });
				case KMarsh: return Of(Pick, 1.0, { { 0.7, Grass_ }, { 0.3, Stalk } });
				case KWoods:
					if (!bSouth) return Of(Pick, 0.6 * Thick, { { 0.45, Shrub }, { 0.35, Grass_ }, { 0.2, Stone } });
					break;
				default: break;
				}
				const int C = CropAt(S);
				const LandField::Base Sward = LandField::Base::Sward;
				switch (C)
				{
				case Vine:
					if (S.Edge > 1.2 && std::fabs(RowNear(S)) < 0.22) return Grown{ ClutOf(CVine), 1.7 };
					return Of(Pick, 0.3, { { 0.6, Clod }, { 0.3, Grass_ }, { 0.1, Stone } });
				case Grain:
					if (Stage == 2) return Of(Pick, 1.0, { { 0.78, LandField::Base::Stubble }, { 0.08, Clod }, { 0.06, Grass_ }, { 0.04, Stalk }, { 0.04, Stone } }); // (the earth through the stubble in clods)
					if (Stage == 3) return Of(Pick, 0.5, { { 0.8, Clod }, { 0.15, Stone }, { 0.05, Grass_ } });
					if (Stage == 1) return Of(Pick, 0.9, { { 0.8, Grass_ }, { 0.15, Clod }, { 0.05, Stone } });
					return Of(Pick, 1.0, { { 0.95, Grass_ }, { 0.05, Stone } }); // (the sown corn's shoots; the earth between them in the ground's own colour)
				case Plough: return Of(Pick, Wet > 0.6 ? 0.7 : 0.5, { { 0.8, Clod }, { 0.15, Stone }, { 0.05, Grass_ } });
				case Olive:
				case Almond:
					if (Unit(Mix(S.Key ^ 0x01E1)) < (C == Almond ? 0.8 : 0.55)) return Of(Pick, 0.45, { { 0.6, Clod }, { 0.15, Stone }, { 0.25, Grass_ } });
					return Of(Pick, 0.8 * Thick, { { 0.35, Sward }, { 0.47, Grass_ }, { 0.1, Stalk }, { 0.08, Stone } });
				case Maquis: return Of(Pick, 0.8, { { 0.3, Shrub }, { 0.3, Stone }, { 0.3, Grass_ }, { 0.1, Stalk } });
				default:
				{
					// fallow: thistle and wild fennel in stands, the grass between, stones, a little garrigue (fewer stalks in
					// the winter, last summer's dead)
					const double Stand = Smooth(0.05, 0.5, Noise(S.X / 9.0, S.Y / 9.0, Seed ^ 0xC2)) * (0.5 + 0.5 * Dry);
					return Of(Pick, 1.2 * Thick, { { 0.24, Sward }, { 0.06, Clod }, { 0.45 - 0.25 * Stand, Grass_ }, { 0.1 + 0.3 * Stand, Stalk }, { 0.05, Stone }, { 0.03, Shrub } });
				}
				}
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				if (P.Kind == LandField::Base::Stubble)
				{
					// the stubble along the strips' way (the nearest road's), either way round
					double DC = 1.0, DS = 0.0;
					StripWay(S, DC, DS);
					P.Yaw = static_cast<float>(std::atan2(DS, DC) + (U6 < 0.5 ? 0.0 : Pi) + 0.06 * (U4 - 0.5));
					P.Scale = static_cast<float>(0.85 + 0.3 * U4);
					P.ScaleZ = static_cast<float>(0.8 + 0.4 * U5);
					const double L = 0.95 + 0.1 * U7;
					P.Tint = Tint(L, L, L);
					return true;
				}
				const int C = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				if (C < 0 || C > 5) return false; // (the sward in the ground's colour: LandField's)
				switch (C)
				{
				case CGrass:
				{
					// most tufts small, a few big; straw in the summer, green-brown in the winter, a few dead ones always
					const double Size = 0.55 + 0.85 * std::pow(U4, 1.5);
					const bool bShoots = Stage == 0 && CropAt(S) == Grain;
					P.Scale = static_cast<float>(bShoots ? 0.45 + 0.2 * U4 : Size);
					P.ScaleZ = static_cast<float>(bShoots ? 0.4 + 0.2 * U5 : Size * (0.7 + 0.5 * U5));
					const double Dead = U6 < 0.15 ? 1.0 : Dry;
					const double G = bShoots ? 1.0 : Green;
					const double R = Lerp(Lerp(0.74, 0.6, G), 1.02 + 0.08 * U7, Dead), Gc = Lerp(Lerp(0.78, 0.8, G), 0.97 + 0.05 * U7, Dead), B = Lerp(Lerp(0.56, 0.5, G), 0.86, Dead);
					P.Tint = Tint(R, Gc, B);
					return true;
				}
				case CStalk:
				{
					// (the summer's dry stalks tall; the winter's, last year's, broken and dark with the rain)
					P.Scale = static_cast<float>(0.7 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(Lerp(0.45 + 0.4 * U5, 0.65 + 0.6 * U5, Dry));
					const double L = 0.9 + 0.2 * U6;
					P.Tint = Tint(L * Lerp(0.78, 1.04, Dry), L * Lerp(0.76, 1.0, Dry), L * Lerp(0.74, 0.94, Dry));
					return true;
				}
				case CStone:
				{
					P.Scale = static_cast<float>(0.5 + 1.3 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.5 * U5);
					const double L = 0.86 + 0.22 * U6;
					P.Tint = Tint(L * 1.02, L, L * 0.96);
					return true;
				}
				case CVine:
				{
					// onto its row's line, its stock leaning a little
					const double Off = RowNear(S);
					P.X -= static_cast<float>(-std::sin(S.Angle) * Off);
					P.Y -= static_cast<float>(std::cos(S.Angle) * Off);
					P.Scale = static_cast<float>(0.8 + 0.4 * U4);
					P.ScaleZ = static_cast<float>(0.85 + 0.35 * U5);
					const double L = 0.92 + 0.16 * U6;
					P.Tint = Tint(L * (1.0 + 0.08 * U7), L, L * (1.0 - 0.06 * U7));
					return true;
				}
				case CShrub:
				{
					P.Scale = static_cast<float>(0.6 + 0.8 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.6 * U5);
					const double L = 0.88 + 0.24 * U6;
					P.Tint = Tint(L * (0.96 + 0.1 * U7), L, L * 0.96);
					return true;
				}
				default:
				{
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.4 * U5);
					if (Wet > 0.6)
					{
						// (the winter's: built wet and dark, the water standing between them along the furrows, the strips' way)
						double DC = 1.0, DS = 0.0;
						StripWay(S, DC, DS);
						P.Yaw = static_cast<float>(std::atan2(DS, DC) + 0.15 * (U7 - 0.5));
						const double L = 0.88 + 0.24 * U6;
						P.Tint = Tint(L, L, L);
						return true;
					}
					// clods in the earth's own colour (on the stubble and the fallow lower, broken by the harvest and the sheep)
					const int C_ = CropAt(S);
					if (C_ == Fallow || (C_ == Grain && Stage == 2)) P.ScaleZ = static_cast<float>(0.35 + 0.25 * U5);
					const Tone E = Ploughed(Soil(P.X, P.Y, 0.0), 0.0);
					const double L = (0.9 + 0.2 * U6) / 128.0;
					P.Tint = Tint(E.R * L, E.G * L, E.B * L);
					return true;
				}
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Rng R(Seed ^ (0xC107ull + static_cast<uint64_t>(K) * 977ull));
				switch (K)
				{
				case CGrass:
				{
					// a tuft: a score of blades, built straw-pale (the tint gives the month its colour); the winter's lower,
					// new grass among last year's, no seed heads
					const bool bWinter = Dry < 0.3;
					for (int B = 0; B < 30; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Out0 = R.Range(0.0, 0.16), High = R.Range(0.12, 0.36) * (bWinter ? 0.7 : 1.0), Lean = R.Range(0.08, 0.3);
						const double X0 = Out0 * std::cos(A), Y0 = Out0 * std::sin(A);
						const double L = R.Range(0.85, 1.12), Wide = R.Range(0.016, 0.026);
						W.Blade(X0, Y0, -0.01, X0 + Lean * std::cos(A), Y0 + Lean * std::sin(A), High, Wide, Varied(Rgb(150, 136, 100), L, 0.0), Varied(Rgb(206, 192, 146), L, 0.0));
					}
					// (a few gone to seed in the summer, their heads pale)
					for (int B = 0; B < (bWinter ? 0 : 5); ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(0.38, 0.55), Lean = R.Range(0.06, 0.16);
						const double X1 = Lean * std::cos(A), Y1 = Lean * std::sin(A);
						W.Blade(0.0, 0.0, -0.01, X1, Y1, High, 0.01, Rgb(150, 136, 98), Rgb(196, 182, 140));
						W.Blade(X1, Y1, High - 0.07, X1 * 1.15, Y1 * 1.15, High + 0.02, 0.024, Rgb(200, 186, 146), Rgb(218, 206, 168));
					}
					M.FarFrom = 45.0f;
					M.Paint = 2;
					break;
				}
				case CStalk:
				{
					// wild fennel gone to seed: three tall stems, each with its umbel at the top, flat and yellow-green, some 10
					// to 15 cm across, its rays spreading under it
					for (int B = 0; B < 3; ++B)
					{
						const double A = R.Range(0.0, 2.0 * Pi), High = R.Range(1.0, 1.55), Lean = R.Range(0.05, 0.2), Half = R.Range(0.05, 0.075);
						const double Round_ = R.Range(0.85, 1.0), Light = R.Range(0.92, 1.06);
						const double X1 = Lean * std::cos(A), Y1 = Lean * std::sin(A);
						W.Blade(0.0, 0.0, -0.02, X1, Y1, High, 0.035, Rgb(116, 100, 70), Rgb(150, 130, 88));
						for (int H = 0; H < 8; ++H)
						{
							const double B2 = H * 2.0 * Pi / 8.0 + R.Range(-0.2, 0.2);
							W.Blade(X1, Y1, High - 0.06, X1 + 0.85 * Half * std::cos(B2), Y1 + 0.85 * Half * std::sin(B2), High + 0.02, 0.01, Rgb(122, 118, 70), Rgb(146, 144, 74));
						}
						const double C[3] = { X1, Y1, High + 0.03 }, AX[3] = { std::cos(A), std::sin(A), 0.0 }, AY[3] = { -std::sin(A), std::cos(A), 0.0 };
						Pad(W, C, AX, AY, Half, Half * Round_, Varied(Rgb(152, 150, 64), Light, 0.0), 0.012, 0.25);
					}
					// and a thistle beside it: a stiff stem, its spiny leaves at the foot, two round heads, grey-purple over
					// their grey-green bracts
					{
						const double TX = 0.32, TY = 0.12, High = R.Range(0.55, 0.8);
						for (int L = 0; L < 6; ++L)
						{
							const double A = L * 2.0 * Pi / 6.0 + R.Range(-0.25, 0.25), Reach = R.Range(0.18, 0.26), Up = R.Range(0.06, 0.14);
							W.Blade(TX, TY, 0.02, TX + Reach * std::cos(A), TY + Reach * std::sin(A), Up, 0.06, Rgb(120, 112, 84), Rgb(150, 140, 106));
						}
						W.Blade(TX, TY, -0.02, TX + 0.03, TY, High, 0.03, Rgb(122, 110, 82), Rgb(150, 136, 98));
						for (int Hd = 0; Hd < 2; ++Hd)
						{
							const double HX = TX + (Hd ? 0.09 : 0.03), HZ = High - (Hd ? 0.12 : 0.0), Size = Hd ? 0.8 : 1.0;
							Bead(W, HX, TY, HZ, 0.036 * Size, 0.06 * Size, Rgb(142, 106, 140), Rgb(112, 116, 96));
						}
					}
					M.FarFrom = 55.0f;
					M.Paint = 2;
					break;
				}
				case CStone:
				{
					// a few pieces of weathered limestone, one bigger, each half sunk in the earth (the art director, 24 Nov:
					// built white and whole they read as popcorn; lit about 150-175 on the screen)
					const auto Sunk = [&W, &M](double CX, double CY, double Rad, double Ht, uint64_t Sd, const Tone& Lit, const Tone& Under)
					{
						const size_t From = M.Near.Positions.size();
						W.Rock(CX, CY, Rad, Ht, Sd, Lit, Under);
						for (size_t I = From + 2; I < M.Near.Positions.size(); I += 3) M.Near.Positions[I] -= static_cast<float>(0.45 * Ht);
					};
					Sunk(0.0, 0.0, 0.16, 0.1, 31, Rgb(164, 156, 138), Rgb(118, 110, 96));
					Sunk(0.24, 0.1, 0.07, 0.05, 32, Rgb(158, 150, 134), Rgb(114, 106, 92));
					Sunk(-0.18, 0.16, 0.06, 0.04, 33, Rgb(152, 144, 128), Rgb(110, 102, 88));
					M.FarFrom = 40.0f;
					break;
				}
				case CVine:
				{
					// a bush vine (alberello): its gnarled stock some 0.5 m, its arms; in leaf from May, turning yellow and
					// red-brown in October, bare from November to March
					const Tone Bark = Rgb(104, 88, 70), BarkLit = Rgb(132, 114, 92);
					W.Box(0.0, 0.0, 0.25, 0.035, 0.035, 0.26, 0.3, Bark);
					for (int A = 0; A < 5; ++A)
					{
						const double Th = A * 2.0 * Pi / 5.0 + R.Range(-0.3, 0.3), Reach = R.Range(0.14, 0.26);
						const double X1 = Reach * std::cos(Th), Y1 = Reach * std::sin(Th), Z1 = R.Range(0.62, 0.78);
						W.Blade(0.0, 0.0, 0.48, X1, Y1, Z1, 0.04, Bark, BarkLit);
						// (the canes: pruned short spurs in the winter, long and leafy in the summer)
						const int Canes = Leaf > 0.2 ? 2 : 1;
						for (int C = 0; C < Canes; ++C)
						{
							const double Up = Leaf > 0.2 ? R.Range(0.35, 0.6) : R.Range(0.08, 0.16);
							const double OutX = R.Range(1.2, 2.0), OutY = R.Range(1.2, 2.0);
							W.Blade(X1, Y1, Z1, X1 * OutX, Y1 * OutY, Z1 + Up, 0.018, BarkLit, Rgb(140, 120, 92));
						}
					}
					if (Leaf > 0.05)
					{
						const int Leaves = static_cast<int>(10.0 + 50.0 * Leaf);
						for (int L = 0; L < Leaves; ++L)
						{
							const double Th = R.Range(0.0, 2.0 * Pi), Rad = R.Range(0.05, 0.48) * (0.6 + 0.4 * Leaf), Z = R.Range(0.5, 1.15);
							const double X = Rad * std::cos(Th), Y = Rad * std::sin(Th);
							const double Dir = R.Range(0.0, 2.0 * Pi), Long = R.Range(0.1, 0.17);
							const double Pick = R.Next(), Light = R.Range(0.9, 1.1), Rise_ = R.Range(-0.05, 0.08), Wide = R.Range(0.1, 0.15);
							Tone Lf = Varied(Mixed(Rgb(84, 110, 46), Rgb(112, 132, 58), Pick), Light, 0.0);
							const Tone Turned = Pick < 0.6 ? Rgb(200, 166, 64) : Pick < 0.85 ? Rgb(168, 96, 48) : Rgb(140, 70, 42);
							Lf = Mixed(Lf, Turned, Turn);
							W.Blade(X, Y, Z, X + Long * std::cos(Dir), Y + Long * std::sin(Dir), Z + Rise_, Wide, Lf, Varied(Lf, 1.1, 0.0));
						}
						M.Paint = 1;
					}
					M.FarFrom = 70.0f;
					break;
				}
				case CShrub:
				{
					// garrigue: a low mound of small dark leaves, grey-green on top
					for (int L = 0; L < 70; ++L)
					{
						const double Th = R.Range(0.0, 2.0 * Pi), U = R.Next();
						const double Rad = 0.34 * std::sqrt(U), Z = 0.3 * std::sqrt(std::max(0.0, 1.0 - U)) + R.Range(0.0, 0.05);
						const double X = Rad * std::cos(Th), Y = Rad * std::sin(Th), Dir = R.Range(0.0, 2.0 * Pi), Lit = R.Range(0.3, 1.0);
						const Tone Lf = Mixed(Rgb(70, 84, 48), Rgb(124, 128, 94), Smooth(0.08, 0.36, Z) * Lit);
						W.Blade(X, Y, Z - 0.03, X + 0.06 * std::cos(Dir), Y + 0.06 * std::sin(Dir), Z + 0.04, 0.06, Varied(Lf, 0.8, 0.0), Lf);
					}
					M.FarFrom = 55.0f;
					M.Paint = 1;
					break;
				}
				default:
				{
					if (Wet > 0.6)
					{
						// the winter's: the furrows' ridges in wet clods, the rain standing in the furrows between them, grey
						// with the sky (laid along the parcel's grain). Each pool lies in its rut: banks of wet earth a little
						// proud of it either side, its edges silted, its ends drawn in and fading out (the art director, 24
						// Nov: drawn flat and hard-edged they read as grey slabs laid on the field; the clods red-brown, not
						// near-black)
						const Tone Sky = Rgb(122, 130, 138), Silt = Rgb(104, 90, 76), Crest = Rgb(128, 92, 66), Foot = Rgb(118, 86, 64);
						constexpr int Steps = 4;
						for (int F = -1; F <= 1; ++F)
						{
							const double Y0 = 0.62 * F + R.Range(-0.04, 0.04), Half = R.Range(0.035, 0.06), Long = R.Range(0.7, 1.0);
							const double Rim = R.Range(0.05, 0.08), Bend = R.Range(-0.02, 0.02);
							uint32_t Row[Steps + 1][6];
							for (int St = 0; St <= Steps; ++St)
							{
								const double T = static_cast<double>(St) / Steps, X = -Long + 2.0 * Long * T;
								const double Wide = 0.3 + 0.7 * std::sin(Pi * T), YM = Y0 + Bend * std::sin(2.0 * Pi * T);
								const bool bEnd = St == 0 || St == Steps;
								const double At[6] = { -(Half + Rim), -(Half + 0.35 * Rim), -Half, Half, Half + 0.35 * Rim, Half + Rim };
								const double Up[6] = { -0.01, 0.028, 0.01, 0.01, 0.028, -0.01 };
								const Tone Water = bEnd ? Silt : Mixed(Sky, Silt, 0.3);
								const Tone Col[6] = { Foot, Crest, Water, Water, Crest, Foot };
								for (int J = 0; J < 6; ++J) Row[St][J] = W.Vert(X, YM + Wide * At[J], Up[J], 0.0, 0.0, 1.0, Col[J]);
							}
							for (int St = 0; St < Steps; ++St)
								for (int J = 0; J < 5; ++J)
								{
									W.Tri(Row[St][J], Row[St + 1][J], Row[St + 1][J + 1]);
									W.Tri(Row[St][J], Row[St + 1][J + 1], Row[St][J + 1]);
								}
						}
						W.Rock(0.1, 0.31, 0.13, 0.08, 41, Rgb(146, 104, 74), Rgb(100, 72, 52));
						W.Rock(-0.35, -0.31, 0.1, 0.07, 42, Rgb(140, 100, 72), Rgb(96, 68, 50));
						W.Rock(0.5, -0.3, 0.08, 0.05, 43, Rgb(150, 108, 78), Rgb(104, 74, 54));
						M.FarFrom = 35.0f;
						break;
					}
					// clods: a few lumps of earth, built grey-brown (the tint gives the earth's own colour)
					W.Rock(0.0, 0.0, 0.11, 0.07, 41, Rgb(136, 130, 124), Rgb(84, 80, 76));
					W.Rock(0.2, -0.08, 0.08, 0.05, 42, Rgb(132, 126, 120), Rgb(80, 76, 72));
					W.Rock(-0.12, 0.18, 0.06, 0.04, 43, Rgb(128, 122, 116), Rgb(78, 74, 70));
					M.FarFrom = 35.0f;
					break;
				}
				}
				M.Far = LandField::Mesh();
			}

			// ------------------------------------------------------------------ the kit

			const char* Folder() const override { return "Med"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KOlive: return "OliveTree";
				case KCypress: return "Cypress";
				case KMacchia: return "Macchia";
				case KFarm: return "Farmhouse";
				case KTower: return "SlimTower";
				case KStones: return "Stones";
				case KBoulder: return "Boulder";
				case KHouseStone: return "FlatRoofHouseStone";
				case KPole: return "TelegraphPole";
				case KSangar: return "Sangar";
				case KWall: return "StoneWall";
				case KPear: return bPears ? "PricklyPear" : nullptr;
				case KAgave: return bPears ? "Agave" : nullptr;
				case KStook: return Stage == 2 ? "Stook" : "BareTree";
				case KStack: return "StrawStack";
				case KSpike: return bPears ? "AgaveSpike" : nullptr;
				default: return nullptr;
				}
			}
			bool KitBuilt(int K) const override { return K == KPear || K == KAgave || K == KStook || K == KStack || K == KSpike; }
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Stands = [&M](float Mid, float Half, float Across, float Trunk, float Far) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
					M.SightTrunk = Trunk;
					M.FarFrom = Far;
				};
				switch (K)
				{
				case KOlive:
					Stands(0.62f, 0.32f, 0.75f, 0.3f, 120.0f);
					M.Paint = 1;
					M.bTree = true; // (far off a crown with no shadow, as the bocage's trees)
					break;
				case KCypress:
					Stands(0.5f, 0.48f, 0.85f, 0.3f, 220.0f);
					M.Paint = 1;
					M.bTree = true;
					break;
				case KMacchia:
					Stands(0.5f, 0.5f, 0.8f, 0.0f, 90.0f);
					M.Paint = 1;
					break;
				case KFarm:
				case KHouseStone:
				case KTower: Stands(0.5f, 0.5f, 0.9f, 0.0f, 400.0f); break;
				case KBoulder:
				case KSangar: Stands(0.5f, 0.5f, 0.85f, 0.0f, 200.0f); break;
				case KPole: M.FarFrom = 220.0f; break;
				case KWall:
				{
					// (every stone near; far off six blocks of grey-ochre stone, light and dark, their tops stepping, in
					// place of the kit's own plain box, which read as a smooth white kerb)
					M.FarFrom = 45.0f;
					M.Far = LandField::Mesh();
					Writer F(M.Far);
					if (!bStoneTrial)
					{
						static const double Tops[6] = { 1.16, 1.27, 1.04, 1.2, 1.1, 1.24 }, Lit[6] = { 1.0, 0.86, 1.06, 0.9, 0.97, 0.84 };
						for (int P = 0; P < 6; ++P) F.Box(-2.52 + 1.01 * P, 0.03 * ((P * 5) % 3 - 1), 0.5 * Tops[P], 0.52, 0.5 - 0.03 * (P % 2), 0.5 * Tops[P], 0.04 * ((P * 7) % 3 - 1), Varied(Rgb(152, 139, 114), Lit[P], 0.0));
						break;
					}
					// (on trial, "medwall": far off two courses laid the length of the piece, their faces battered in as dry
					// stone is laid, under a pale cap of four stones' lengths, each its own height: a light line along a
					// ragged top, never the six blocks that read as a fence of dark palings. The foot a course of darker,
					// bigger stones 0.4 m high, some 30 darker than the course above it as lit, so on pale summer ground the
					// wall keeps a dark line under its pale one)
					const Tone Cap = Rgb(196, 186, 160);
					auto Face = [&F](const double (&P)[4][3], double NX, double NY, double NZ, const Tone (&C)[4]) {
						uint32_t Id[4];
						for (int Q = 0; Q < 4; ++Q) Id[Q] = F.Vert(P[Q][0], P[Q][1], P[Q][2], NX, NY, NZ, C[Q]);
						const double U[3] = { P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2] }, V[3] = { P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2] };
						const double Out = (U[1] * V[2] - U[2] * V[1]) * NX + (U[2] * V[0] - U[0] * V[2]) * NY + (U[0] * V[1] - U[1] * V[0]) * NZ;
						if (Out > 0.0)
						{
							F.Tri(Id[0], Id[1], Id[2]);
							F.Tri(Id[0], Id[2], Id[3]);
						}
						else
						{
							F.Tri(Id[0], Id[2], Id[1]);
							F.Tri(Id[0], Id[3], Id[2]);
						}
					};
					// (a course from X0 to X1, Z0 to Z1, Foot to Top metres either side of the middle: its two faces and
					// its ends, and its top where nothing lies on it; Low its colour at the foot, High at the top)
					auto Course = [&Face](double X0, double X1, double Z0, double Z1, double Foot, double Top, const Tone& Low, const Tone& High, bool bLid) {
						const double Lean = std::atan2(Foot - Top, Z1 - Z0), CL = std::cos(Lean), SL = std::sin(Lean);
						for (int S = -1; S <= 1; S += 2)
							Face({ { X0, S * Foot, Z0 }, { X1, S * Foot, Z0 }, { X1, S * Top, Z1 }, { X0, S * Top, Z1 } }, 0.0, S * CL, SL, { Low, Low, High, High });
						for (int E = -1; E <= 1; E += 2)
						{
							const double X = E < 0 ? X0 : X1;
							Face({ { X, -Foot, Z0 }, { X, Foot, Z0 }, { X, Top, Z1 }, { X, -Top, Z1 } }, E, 0.0, 0.0, { Low, Low, High, High });
						}
						if (bLid) Face({ { X0, -Top, Z1 }, { X1, -Top, Z1 }, { X1, Top, Z1 }, { X0, Top, Z1 } }, 0.0, 0.0, 1.0, { High, High, High, High });
					};
					const Tone Lower = Rgb(118, 107, 86), Upper = Rgb(152, 139, 114);
					Course(-3.03, 3.03, 0.0, 0.4, 0.46, 0.411, Varied(Lower, 0.9, 0.0), Lower, false);
					Course(-3.03, 3.03, 0.4, 0.98, 0.411, 0.34, Varied(Upper, 0.94, 0.0), Upper, false);
					static const double Ends[5] = { -3.03, -1.58, -0.12, 1.41, 3.03 }, Caps[4] = { 1.12, 1.21, 1.06, 1.17 }, Lits[4] = { 1.0, 0.93, 1.04, 0.96 };
					for (int P = 0; P < 4; ++P) Course(Ends[P], Ends[P + 1], 0.98, Caps[P], 0.34, 0.29, Varied(Cap, 0.9 * Lits[P], 0.0), Varied(Cap, Lits[P], 0.0), true);
					// (near, each stone's faces that look up lifted toward the cap's pale, the more the more they look up, so
					// the wall's top and its stones' upper edges catch a low sun and the face beneath stays the stone's own)
					for (LandField::Mesh* Lv : { &M.Near, &M.Mid })
						for (size_t P = 0; P < Lv->Colours.size() && 3 * P + 2 < Lv->Normals.size(); ++P)
						{
							const double Up = Smooth(0.6, 0.95, Lv->Normals[3 * P + 2]);
							if (Up <= 0.0) continue;
							const LandField::Rgb8 C = Lv->Colours[P];
							Lv->Colours[P] = Byte(Mixed(Tone{ static_cast<double>(C.R), static_cast<double>(C.G), static_cast<double>(C.B) }, Cap, 0.7 * Up));
						}
					break;
				}
				case KPear:
					// (lit as the agave: its pads' shaded side lifted by the light through and round them, near black else)
					Stands(0.5f, 0.5f, 0.8f, 0.0f, 110.0f);
					M.Paint = 1;
					break;
				case KAgave: M.FarFrom = 90.0f; M.Paint = 1; break;
				case KSpike: M.FarFrom = 160.0f; M.Paint = 1; break;
				case KStook:
					if (Stage == 2)
					{
						M.FarFrom = 110.0f;
						break;
					}
					// (the bare tree: seen through but for its trunk; far off a crown)
					Stands(0.6f, 0.35f, 0.35f, 0.3f, 110.0f);
					M.Paint = 2;
					M.bTree = true;
					break;
				case KStack: Stands(0.45f, 0.45f, 0.9f, 0.0f, 260.0f); break;
				default: M.FarFrom = 90.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				Writer F(M.Far);
				Rng R(Seed ^ (0xB17ull + static_cast<uint64_t>(K) * 7919ull));
				switch (K)
				{
				case KPear:
				{
					// (far off the same clump, each pad a flat diamond, no fruit: never a ball)
					Rng Same = R;
					Pear(W, R, false);
					Pear(F, Same, true);
					return;
				}
				case KAgave:
				case KSpike:
					Agave(W, R, K == KSpike);
					M.Far = M.Near;
					return;
				case KStook:
					if (Stage == 2)
					{
						Stook(W, R);
						Tube(F, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 1.25, 0.16, 6, Rgb(150, 126, 80), Rgb(206, 180, 118));
						return;
					}
					{
						Rng Same = R;
						Bare(W, R, true);
						Bare(F, Same, false);
					}
					return;
				case KStack:
					Stack(W, R, true);
					Stack(F, R, false);
					return;
				case KFarm:
					W.Box(0.0, 0.0, 3.0, 7.0, 4.2, 3.0, 0.0, Rgb(204, 188, 156));
					W.Box(0.0, 0.0, 6.6, 7.3, 4.5, 0.6, 0.0, Rgb(170, 100, 70));
					break;
				case KHouseStone: W.Box(0.0, 0.0, 1.8, 5.5, 5.5, 1.8, 0.0, Rgb(214, 202, 176)); break;
				case KTower: W.Box(0.0, 0.0, 10.0, 1.6, 1.6, 10.0, 0.0, Rgb(206, 192, 164)); break;
				case KOlive:
					W.Box(0.0, 0.0, 1.0, 0.18, 0.18, 1.0, 0.0, Rgb(112, 106, 96));
					W.Rock(0.0, 0.0, 2.2, 3.6, 5, Rgb(132, 140, 108), Rgb(92, 98, 74));
					break;
				case KCypress: W.Box(0.0, 0.0, 6.0, 0.9, 0.9, 6.0, 0.0, Rgb(46, 60, 42)); break;
				case KMacchia: W.Rock(0.0, 0.0, 0.8, 1.1, 7, Rgb(76, 92, 54), Rgb(52, 64, 38)); break;
				case KPole: W.Box(0.0, 0.0, 3.5, 0.1, 0.1, 3.5, 0.0, Rgb(110, 96, 80)); break;
				case KWall: W.Box(0.0, 0.0, 0.6, 3.0, 0.45, 0.6, 0.0, Rgb(146, 134, 110)); break;
				default: W.Rock(0.0, 0.0, 0.9, 0.7, 11, Rgb(196, 186, 164), Rgb(130, 120, 104)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			/** The prickly pear: a branching clump of flat oval paddles, each grown from the rim of the one below, some
			 * 1.8 m; the oldest at the foot woody and grey-brown, the rest grey-green; a few dull red-orange fruits on the
			 * rims of the upper ones. Far (bFar): the same clump, each pad a flat diamond, no fruit. */
			static void Pear(Writer& W, Rng& R, bool bFar)
			{
				struct Bud
				{
					double P[3], A[3];
					int Depth;
				};
				std::vector<Bud> Todo;
				for (int K = 0; K < 5; ++K)
				{
					const double Az = K * 2.0 * Pi / 5.0 + R.Range(-0.4, 0.4), Out = R.Range(0.05, 0.35), Lean = R.Range(0.1, 0.45);
					const double L = std::sqrt(1.0 + Lean * Lean);
					Todo.push_back({ { Out * std::cos(Az), Out * std::sin(Az), 0.0 }, { Lean * std::cos(Az) / L, Lean * std::sin(Az) / L, 1.0 / L }, 0 });
				}
				for (size_t Next = 0; Next < Todo.size() && Next < 64; ++Next)
				{
					const Bud Bd = Todo[Next];
					const double Long = 0.2 - 0.008 * Bd.Depth, Wide = 0.13 - 0.005 * Bd.Depth;
					// the pad's face, turned about its axis
					const double Yaw = R.Range(0.0, 2.0 * Pi), Shade = R.Range(0.9, 1.08), Fruit = R.Next();
					double B[3] = { std::cos(Yaw), std::sin(Yaw), 0.0 };
					const double Dot = B[0] * Bd.A[0] + B[1] * Bd.A[1];
					for (int I = 0; I < 3; ++I) B[I] -= Dot * Bd.A[I];
					double BL = std::sqrt(B[0] * B[0] + B[1] * B[1] + B[2] * B[2]);
					if (BL < 1e-3)
					{
						B[0] = 1.0;
						B[1] = 0.0;
						B[2] = 0.0;
						BL = 1.0;
					}
					for (double& V : B) V /= BL;
					const double C[3] = { Bd.P[0] + Long * Bd.A[0], Bd.P[1] + Long * Bd.A[1], Bd.P[2] + Long * Bd.A[2] };
					// (grey-green, the art director's 110,125,80; the oldest woody)
					const Tone Col = Bd.Depth == 0 ? Varied(Rgb(126, 120, 96), Shade, 0.0) : Varied(Mixed(Rgb(104, 120, 76), Rgb(118, 132, 86), Bd.Depth / 5.0), Shade, 0.0);
					if (bFar) Pad(W, C, Bd.A, B, Long, Wide, Col, 0.0, 0.2, 4);
					else Pad(W, C, Bd.A, B, Long, Wide, Col, 0.01, 0.2);
					// its children from its rim, one or two (three low down), leaning out and up
					const double Kids = R.Next();
					const int N = Bd.Depth >= 5 ? 0 : 1 + (Kids < 0.5 ? 1 : 0) + (Bd.Depth < 2 && Kids < 0.2 ? 1 : 0);
					for (int Kd = 0; Kd < N; ++Kd)
					{
						const double Turn = R.Range(-0.9, 0.9), Where = R.Range(-0.5, 0.5);
						Bud Kid;
						for (int I = 0; I < 3; ++I) Kid.A[I] = std::cos(Turn) * Bd.A[I] + std::sin(Turn) * B[I];
						Kid.A[2] += 0.45;
						const double AL = std::sqrt(Kid.A[0] * Kid.A[0] + Kid.A[1] * Kid.A[1] + Kid.A[2] * Kid.A[2]);
						for (int I = 0; I < 3; ++I)
						{
							Kid.A[I] /= AL;
							Kid.P[I] = C[I] + 0.85 * Long * Bd.A[I] + Where * Wide * B[I];
						}
						Kid.Depth = Bd.Depth + 1;
						Todo.push_back(Kid);
					}
					// (on some of the upper pads two to four fruits along the top of its rim, dull red-orange)
					if (Bd.Depth < 2 || Fruit > 0.45) continue;
					const int Fruits = 2 + static_cast<int>(Fruit * 4.0) % 3;
					for (int Q = 0; Q < Fruits; ++Q)
					{
						const double T = (Q - 0.5 * (Fruits - 1)) * 0.45, Ripe = R.Next();
						if (bFar) continue;
						double At[3];
						for (int I = 0; I < 3; ++I) At[I] = C[I] + 1.02 * (std::cos(T) * Long * Bd.A[I] + std::sin(T) * Wide * B[I]);
						Bead(W, At[0], At[1], At[2] + 0.015, 0.022, 0.045, Mixed(Rgb(150, 86, 54), Rgb(170, 104, 58), Ripe), Rgb(128, 104, 64));
					}
				}
			}

			/** The agave: a rosette of stiff grey-blue leaves, the outer ones low and long, a few dead ones lying brown at
			 * its foot; in flower (bSpike) its leaves yellowing and its dry pole 5 to 6 m, its branches at the top. */
			static void Agave(Writer& W, Rng& R, bool bSpike)
			{
				const int Leaves = 22;
				const Tone Foot = bSpike ? Rgb(112, 116, 90) : Rgb(96, 116, 104), Tip = bSpike ? Rgb(160, 150, 106) : Rgb(146, 164, 144);
				for (int L = 0; L < Leaves; ++L)
				{
					const double T = static_cast<double>(L) / (Leaves - 1); // (0 the outermost, 1 the heart)
					const double Az = L * 2.39996 + R.Range(-0.15, 0.15), Up = Lerp(0.35, 1.3, T) + R.Range(-0.1, 0.1), Len = Lerp(1.3, 0.7, T) * R.Range(0.9, 1.1);
					const double Light = R.Range(0.92, 1.06);
					const double X0 = 0.08 * std::cos(Az), Y0 = 0.08 * std::sin(Az), Z0 = 0.1 + 0.15 * T, Out = Len * std::cos(Up);
					W.Blade(X0, Y0, Z0, X0 + Out * std::cos(Az), Y0 + Out * std::sin(Az), Z0 + Len * std::sin(Up), Lerp(0.22, 0.14, T), Varied(Foot, Light, 0.0), Varied(Tip, Light, 0.0));
				}
				for (int L = 0; L < 4; ++L)
				{
					const double Az = R.Range(0.0, 2.0 * Pi), Len = R.Range(0.9, 1.3);
					W.Blade(0.1 * std::cos(Az), 0.1 * std::sin(Az), 0.05, Len * std::cos(Az), Len * std::sin(Az), 0.02, 0.2, Rgb(140, 120, 90), Rgb(170, 150, 112));
				}
				if (!bSpike) return;
				const Tone Dry = Rgb(132, 116, 88), DryLit = Rgb(164, 146, 110);
				const double TopX = R.Range(-0.2, 0.2), TopY = R.Range(-0.2, 0.2), Top = R.Range(5.2, 6.0);
				Tube(W, 0.0, 0.0, 0.2, 0.09, TopX, TopY, Top, 0.03, 6, Dry, DryLit);
				// its branches, each holding up its dry seed heads
				for (int B = 0; B < 8; ++B)
				{
					const double T = 0.62 + 0.36 * B / 7.0, Az = B * 2.39996 + R.Range(-0.2, 0.2), Reach = Lerp(0.75, 0.35, T) * R.Range(0.85, 1.15);
					const double BX = TopX * T, BY = TopY * T, BZ = 0.2 + (Top - 0.2) * T;
					const double EX = BX + Reach * std::cos(Az), EY = BY + Reach * std::sin(Az), EZ = BZ + 0.35 * Reach;
					W.Blade(BX, BY, BZ, EX, EY, EZ, 0.05, Dry, DryLit);
					for (int P = 0; P < 3; ++P)
					{
						const double PA = R.Range(0.0, 2.0 * Pi);
						W.Blade(EX, EY, EZ - 0.04, EX + 0.12 * std::cos(PA), EY + 0.12 * std::sin(PA), EZ + 0.12, 0.09, Rgb(118, 100, 74), Rgb(150, 128, 92));
					}
				}
			}

			/** A stook: eight sheaves stood leaning together heads up, a ninth in their middle; the straw gold, some
			 * 1.3 m. */
			static void Stook(Writer& W, Rng& R)
			{
				const Tone Butt = Rgb(150, 126, 80), Straw = Rgb(194, 166, 106), Ears = Rgb(212, 188, 128);
				for (int K = 0; K < 9; ++K)
				{
					const bool bMid = K == 8;
					const double Az = K * 2.0 * Pi / 8.0 + R.Range(-0.2, 0.2), Foot = bMid ? 0.0 : R.Range(0.34, 0.44), Head = bMid ? 0.0 : 0.1;
					const double High = bMid ? R.Range(1.3, 1.4) : R.Range(1.12, 1.28);
					const double FX = Foot * std::cos(Az), FY = Foot * std::sin(Az), HX = Head * std::cos(Az), HY = Head * std::sin(Az);
					// the butts, the band about the waist, the ears spread over it, their top closed
					const double WX = Lerp(FX, HX, 0.5), WY = Lerp(FY, HY, 0.5), WZ = 0.5 * High;
					Tube(W, FX, FY, 0.0, 0.12, WX, WY, WZ, 0.075, 5, Butt, Straw);
					Tube(W, WX, WY, WZ, 0.075, HX, HY, High, 0.16, 5, Straw, Ears);
					const uint32_t Peak = W.Vert(HX, HY, High + 0.07, 0.0, 0.0, 1.0, Ears);
					for (int S = 0; S < 5; ++S)
					{
						const double A = 2.0 * Pi * S / 5.0;
						W.Vert(HX + 0.16 * std::cos(A), HY + 0.16 * std::sin(A), High, 0.5 * std::cos(A), 0.5 * std::sin(A), 0.87, Ears);
					}
					for (int S = 0; S < 5; ++S) W.Tri(Peak, Peak + 1 + S, Peak + 1 + (S + 1) % 5);
				}
			}

			/** A broadleaf tree bare in the winter (an oak, an elm, a chestnut): its trunk, five limbs, their branches and at
			 * their ends a fine grey haze of twigs; the oaks hold a few dead brown leaves. Some 10 m. Far off (bFull false)
			 * the trunk, the limbs and the branches. */
			static void Bare(Writer& W, Rng& R, bool bFull)
			{
				const Tone Bark = Rgb(88, 82, 76), BarkLit = Rgb(108, 100, 92), Twig = Rgb(116, 106, 100), TwigTip = Rgb(142, 130, 124);
				const Tone Dead = Rgb(146, 104, 64), DeadLit = Rgb(168, 124, 78);
				Tube(W, 0.0, 0.0, -0.2, 0.34, 0.0, 0.0, 3.0, 0.24, bFull ? 7 : 5, Bark, BarkLit);
				for (int L = 0; L < 5; ++L)
				{
					const double Az = L * 2.0 * Pi / 5.0 + R.Range(-0.35, 0.35), Lean = R.Range(0.42, 0.8), Len = R.Range(3.4, 4.6), Z0 = R.Range(2.3, 3.0);
					const double DX = std::sin(Lean) * std::cos(Az), DY = std::sin(Lean) * std::sin(Az), DZ = std::cos(Lean);
					const double X0 = 0.12 * std::cos(Az), Y0 = 0.12 * std::sin(Az);
					const double X1 = X0 + Len * DX, Y1 = Y0 + Len * DY, Z1 = Z0 + Len * DZ;
					Tube(W, X0, Y0, Z0, 0.16, X1, Y1, Z1, 0.07, bFull ? 5 : 4, Bark, BarkLit);
					for (int Bn = 0; Bn < 3; ++Bn)
					{
						const double T = 0.45 + 0.27 * Bn + R.Range(-0.05, 0.05);
						const double BAz = Az + R.Range(-1.0, 1.0), BLean = std::clamp(Lean + R.Range(-0.45, 0.35), 0.15, 1.3), BLen = R.Range(1.8, 2.8);
						const double SX = Lerp(X0, X1, T), SY = Lerp(Y0, Y1, T), SZ = Lerp(Z0, Z1, T);
						const double BX = std::sin(BLean) * std::cos(BAz), BY = std::sin(BLean) * std::sin(BAz), BZ = std::cos(BLean);
						Tube(W, SX, SY, SZ, 0.06, SX + BLen * BX, SY + BLen * BY, SZ + BLen * BZ, 0.022, bFull ? 4 : 3, Bark, Twig);
						// the twigs, a spray from the branch's outer half (drawn for the near level only, but always drawn
						// from the numbers so both levels share their limbs)
						for (int Tw = 0; Tw < 8; ++Tw)
						{
							const double U = R.Range(0.35, 1.0), TAz = BAz + R.Range(-1.2, 1.2), TLean = std::clamp(BLean + R.Range(-0.7, 0.5), 0.05, 1.5), TLen = R.Range(0.7, 1.3);
							const double Leafy = R.Next(), LeafAz = R.Range(0.0, 2.0 * Pi);
							if (!bFull) continue;
							const double PX = SX + U * BLen * BX, PY = SY + U * BLen * BY, PZ = SZ + U * BLen * BZ;
							const double QX = PX + TLen * std::sin(TLean) * std::cos(TAz), QY = PY + TLen * std::sin(TLean) * std::sin(TAz), QZ = PZ + TLen * std::cos(TLean);
							W.Blade(PX, PY, PZ, QX, QY, QZ, 0.06, Twig, TwigTip);
							if (Leafy < 0.3) W.Blade(QX, QY, QZ - 0.05, QX + 0.14 * std::cos(LeafAz), QY + 0.14 * std::sin(LeafAz), QZ + 0.02, 0.1, Dead, DeadLit);
						}
					}
				}
			}

			/** A straw stack (the pagliaio): the straw built round a pole into a bulging cone some 4.4 m, the pole standing
			 * out of its top; its skin ragged, wisps out of it, grey where the rain runs, gold where it was cut. Far off
			 * (bNear false) its plain shape. */
			static void Stack(Writer& W, Rng& R, bool bNear)
			{
				static const double Prof[8][2] = { { 0.0, 1.7 }, { 0.5, 1.95 }, { 1.3, 2.05 }, { 2.1, 1.9 }, { 2.9, 1.5 }, { 3.6, 0.98 }, { 4.1, 0.5 }, { 4.4, 0.16 } };
				const int Sides = bNear ? 14 : 8;
				const Tone Gold = Rgb(194, 166, 106), Grey = Rgb(136, 126, 106), Cut = Rgb(206, 180, 118);
				const uint32_t First = static_cast<uint32_t>(W.M.Colours.size());
				for (int Rn = 0; Rn < 8; ++Rn)
				{
					const double Z = Prof[Rn][0], Rad = Prof[Rn][1];
					const int Lo = std::max(0, Rn - 1), Hi = std::min(7, Rn + 1);
					const double DR = Prof[Hi][1] - Prof[Lo][1], DZ = Prof[Hi][0] - Prof[Lo][0];
					for (int K = 0; K < Sides; ++K)
					{
						const double A = 2.0 * Pi * K / Sides + 0.13 * Rn;
						const bool bRagged = bNear && Rn > 0 && Rn < 7;
						const double Rag = bRagged ? R.Range(0.92, 1.08) : 1.0, Lift = bRagged ? R.Range(-0.07, 0.07) : 0.0, Streak = bNear ? R.Next() : 0.5;
						const double NX = std::cos(A) * DZ, NY = std::sin(A) * DZ, NZ = -DR, NL = std::sqrt(NX * NX + NY * NY + NZ * NZ);
						// (weathered grey from the top down where the rain runs, the gold between; cut fresh low on one side)
						Tone C = Mixed(Gold, Grey, Clamp01(0.25 + 0.55 * Smooth(1.0, 4.0, Z) + 0.35 * (Streak - 0.5)));
						if (bNear && Rn <= 3 && std::cos(A - 0.6) > 0.7) C = Mixed(C, Cut, 0.6);
						W.Vert(Rad * Rag * std::cos(A), Rad * Rag * std::sin(A), Z + Lift, NX / NL, NY / NL, NZ / NL, C);
					}
				}
				for (int Rn = 0; Rn + 1 < 8; ++Rn)
					for (int K = 0; K < Sides; ++K)
					{
						const uint32_t A0 = First + Rn * Sides + K, B0 = First + Rn * Sides + (K + 1) % Sides, A1 = A0 + Sides, B1 = B0 + Sides;
						W.Tri(A0, B0, B1);
						W.Tri(A0, B1, A1);
					}
				const uint32_t Peak = W.Vert(0.0, 0.0, 4.5, 0.0, 0.0, 1.0, Grey);
				for (int K = 0; K < Sides; ++K) W.Tri(First + 7 * Sides + K, First + 7 * Sides + (K + 1) % Sides, Peak);
				// the pole out of its top
				Tube(W, 0.0, 0.0, 3.9, 0.07, 0.05, 0.03, 5.7, 0.05, bNear ? 5 : 3, Rgb(100, 90, 76), Rgb(120, 108, 92));
				if (!bNear) return;
				// wisps out of its ragged skin, and the skirt of straw round its foot
				for (int K = 0; K < 60; ++K)
				{
					const double U = R.Next(), A = R.Range(0.0, 2.0 * Pi), Len = R.Range(0.2, 0.45), Droop = R.Range(-0.35, 0.05);
					const double Zf = U * 3.9;
					int Rn = 0;
					while (Rn < 6 && Prof[Rn + 1][0] < Zf) ++Rn;
					const double T = (Zf - Prof[Rn][0]) / (Prof[Rn + 1][0] - Prof[Rn][0]);
					const double Rad = Lerp(Prof[Rn][1], Prof[Rn + 1][1], T) - 0.03;
					const double X0 = Rad * std::cos(A), Y0 = Rad * std::sin(A);
					W.Blade(X0, Y0, Zf, X0 + Len * std::cos(A), Y0 + Len * std::sin(A), Zf + Len * Droop, 0.12, Mixed(Gold, Grey, 0.4), Gold);
				}
				for (int K = 0; K < 28; ++K)
				{
					const double A = K * 2.0 * Pi / 28.0 + R.Range(-0.1, 0.1), Out = R.Range(1.9, 2.25);
					W.Blade(1.85 * std::cos(A), 1.85 * std::sin(A), 0.55, Out * std::cos(A), Out * std::sin(A), 0.0, 0.3, Mixed(Gold, Grey, 0.3), Mixed(Gold, Grey, 0.5));
				}
			}

			// ------------------------------------------------------------------ what stands on it

			void Sites(Host& H) override
			{
				Rng R(H.Seed() ^ 0x51E5);
				// (the nearest road's way by 100 m squares over the field and 400 m round it, for the strips' way)
				if (H.Lanes() > 0)
				{
					RNX = static_cast<int>(std::ceil((FW + 2.0 * RoadMargin) / RoadStep));
					RNY = static_cast<int>(std::ceil((FH + 2.0 * RoadMargin) / RoadStep));
					RoadDir.assign(2 * static_cast<size_t>(RNX) * static_cast<size_t>(RNY), 0.0f);
					// (the roads in pieces some 25 m long at the least: cheap, and their way the same for squares 100 m)
					std::vector<std::array<double, 4>> Pieces;
					for (size_t L = 0; L < H.Lanes(); ++L)
					{
						const auto& Line = H.LaneLine(L);
						size_t From = 0;
						for (size_t K = 1; K < Line.size(); ++K)
						{
							const double AX = Line[From].first, AY = Line[From].second, BX = Line[K].first, BY = Line[K].second;
							if (std::hypot(BX - AX, BY - AY) < 25.0 && K + 1 < Line.size()) continue;
							if (std::hypot(BX - AX, BY - AY) > 1e-6) Pieces.push_back({ AX, AY, BX, BY });
							From = K;
						}
					}
					for (int J = 0; J < RNY; ++J)
						for (int I = 0; I < RNX; ++I)
						{
							const double X = -RoadMargin + (I + 0.5) * RoadStep, Y = -RoadMargin + (J + 0.5) * RoadStep;
							double Best = 1e18, DX = 1.0, DY = 0.0;
							for (const auto& Pc : Pieces)
							{
								const double D2 = SegDist(X, Y, Pc[0], Pc[1], Pc[2], Pc[3]);
								if (D2 < Best)
								{
									Best = D2;
									DX = Pc[2] - Pc[0];
									DY = Pc[3] - Pc[1];
								}
							}
							const double DL = std::hypot(DX, DY), Sign = DY < 0.0 || (DY == 0.0 && DX < 0.0) ? -1.0 : 1.0;
							const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(RNX) + static_cast<size_t>(I));
							RoadDir[K] = static_cast<float>(Sign * DX / DL);
							RoadDir[K + 1] = static_cast<float>(Sign * DY / DL);
						}
				}
				int Built = 0, Hurt = 0, Steads = 0;
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				if (bTown) Village(H, R, TX, TY, TR, Built, Hurt);
				Holdings(H, R, bTown, TX, TY, TR, Built, Steads);
				H.Tally(Built, Hurt, Steads);
				// (the land's rise beyond the field on a 50 m grid out to the ring's edge, for the far ground's paint)
				GridX0 = -5600.0;
				GridY0 = -5600.0;
				GridNX = static_cast<int>(std::ceil((FW + 11200.0) / GridStep)) + 1;
				GridNY = static_cast<int>(std::ceil((FH + 11200.0) / GridStep)) + 1;
				LiftGrid.assign(static_cast<size_t>(GridNX) * GridNY, 0.0f);
				for (int J = 0; J < GridNY; ++J)
					for (int I = 0; I < GridNX; ++I)
					{
						const double X = GridX0 + I * GridStep, Y = GridY0 + J * GridStep;
						LiftGrid[static_cast<size_t>(J) * GridNX + I] = static_cast<float>(Lift(X, Y, Out(X, Y), SideAt(X, Y)));
					}
				// (the farms by 100 m squares, for the ground's paint)
				FarmNX = static_cast<int>(std::ceil((FW + 2.0 * Margin) / FarmCell));
				FarmNY = static_cast<int>(std::ceil((FH + 2.0 * Margin) / FarmCell));
				FarmGrid.assign(static_cast<size_t>(FarmNX) * FarmNY, {});
				for (size_t K = 0; K < Farms.size(); ++K)
				{
					const Farm& F = Farms[K];
					double X0 = F.X - 30.0, X1 = F.X + 30.0, Y0 = F.Y - 30.0, Y1 = F.Y + 30.0;
					if (F.bTrack)
					{
						X0 = std::min({ X0, F.AX - 4.0, F.BX - 4.0 });
						X1 = std::max({ X1, F.AX + 4.0, F.BX + 4.0 });
						Y0 = std::min({ Y0, F.AY - 4.0, F.BY - 4.0 });
						Y1 = std::max({ Y1, F.AY + 4.0, F.BY + 4.0 });
					}
					const int I0 = std::max(0, static_cast<int>(std::floor((X0 + Margin) / FarmCell))), I1 = std::min(FarmNX - 1, static_cast<int>(std::floor((X1 + Margin) / FarmCell)));
					const int J0 = std::max(0, static_cast<int>(std::floor((Y0 + Margin) / FarmCell))), J1 = std::min(FarmNY - 1, static_cast<int>(std::floor((Y1 + Margin) / FarmCell)));
					for (int J = J0; J <= J1; ++J)
						for (int I = I0; I <= I1; ++I) FarmGrid[static_cast<size_t>(J) * FarmNX + I].push_back(static_cast<uint32_t>(K));
				}
			}

			/** The village round the rules' town: its bell tower over a small piazza, its houses packed along the
			 * streets and behind them, stone and plaster under terracotta (some shell-struck), its cemetery of cypresses
			 * outside. */
			void Village(Host& H, Rng& R, double TX, double TY, double TR, int& Built, int& Hurt)
			{
				// the bell tower, near the middle
				for (int K = 0; K < 80; ++K)
				{
					const double A = K * 2.39, D = 4.0 + 1.3 * K;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A), Yaw = R.Range(0.0, 0.5 * Pi);
					if (H.House(KitOf(KTower), X, Y, Yaw, 4.2, 4.2))
					{
						TowerX = X;
						TowerY = Y;
						bTower = true;
						++Built;
						break;
					}
				}
				// the piazza before it
				if (bTower) H.Clear(TowerX + 9.0, TowerY - 4.0, 9.0);
				auto Lay = [&](double X, double Y, double Yaw) {
					const double Pick = R.Next();
					const double Odds = R.Next();
					if (Pick < (bSouth ? 0.25 : 0.12))
					{
						if (H.House(KitOf(KHouseStone), X, Y, Yaw, 11.6, 11.2)) return 11.6;
					}
					else if (Pick < 0.62)
					{
						const double Long = R.Range(8.0, 12.0), Deep = R.Range(6.5, 8.0);
						const int Hit = Odds < 0.07 ? 3 : Odds < 0.15 ? 2 : Odds < 0.27 ? 1 : 0;
						if (H.Building(X, Y, Yaw, Long, Deep, 0, 2, 1, false, Hit))
						{
							if (Hit) ++Hurt;
							return Long;
						}
					}
					else if (H.House(KitOf(KFarm), X, Y, Yaw, 14.8, 8.8)) return 14.8;
					return 0.0;
				};
				// along the streets: house after house facing the road
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						double Skip = 0.0;
						for (size_t K = 1; K < Line.size(); ++K)
						{
							const double AX = Line[K - 1].first, AY = Line[K - 1].second, BX = Line[K].first, BY = Line[K].second;
							const double Len = std::hypot(BX - AX, BY - AY);
							if (Len < 1e-6) continue;
							Skip -= Len;
							if (Skip > 0.0) continue;
							if (std::hypot(BX - TX, BY - TY) > TR * 1.05) continue;
							const double UX = (BX - AX) / Len, UY = (BY - AY) / Len;
							const double Deep = 8.0;
							const double X = BX - UY * Side * (Half + 1.6 + 0.5 * Deep), Y = BY + UX * Side * (Half + 1.6 + 0.5 * Deep);
							const double Skew = R.Range(-0.04, 0.04);
							const double Laid = Lay(X, Y, std::atan2(UY, UX) + Skew);
							if (Laid > 0.0)
							{
								++Built;
								Skip = 0.5 * Laid + R.Range(4.5, 7.0);
							}
							else Skip = 1.0;
						}
					}
				}
				// and behind them, packed
				// (on the rules' built-up cells, and a few round them)
				const int Want = std::max(16, static_cast<int>(80.0 * (TR / 150.0) * (TR / 150.0)));
				for (int K = 0; K < 1500 && Built < Want; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * std::sqrt(R.Next()) * 1.15;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const double Skew = R.Range(-0.12, 0.12), Turn_ = R.Next();
					const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi);
					const double Odds = R.Next();
					if (!CellBuilt(X, Y) && Odds > 0.15) continue;
					if (Lay(X, Y, Yaw) > 0.0) ++Built;
				}
				// the cemetery: a walled square outside the village, cypresses in rows inside its walls
				for (int K = 0; K < 40 && !bCemetery; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * R.Range(1.15, 1.6);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A), Yaw = A + R.Range(-0.2, 0.2);
					const double CA = std::cos(Yaw), SA = std::sin(Yaw);
					bool bFree = true;
					for (int U = -2; U <= 2 && bFree; ++U)
						for (int V = -2; V <= 2 && bFree; ++V)
						{
							const double PX = X + U * 8.5 * CA - V * 6.0 * SA, PY = Y + U * 8.5 * SA + V * 6.0 * CA;
							if (!H.Free(PX, PY, 2.5)) bFree = false;
						}
					if (!bFree) continue;
					bCemetery = true;
					CemX = X;
					CemY = Y;
					CemYaw = Yaw;
					// its cypresses: two rows along its long walls, a pair at its gate
					for (int Row = -1; Row <= 1; Row += 2)
						for (int U = -3; U <= 3; ++U)
						{
							const double PX = X + U * 5.0 * CA - Row * 9.0 * SA, PY = Y + U * 5.0 * SA + Row * 9.0 * CA;
							const double Sz = R.Range(1.0, 1.4), TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1);
							H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Sz, Tint(1.0, 1.0, 1.0));
						}
					for (int Side = -1; Side <= 1; Side += 2)
					{
						const double PX = X - 21.0 * CA - Side * 3.0 * SA, PY = Y - 21.0 * SA + Side * 3.0 * CA;
						const double TreeYaw = R.Range(0.0, 2.0 * Pi), Tall = R.Range(1.2, 1.5);
						H.Plant(KitOf(KCypress), PX, PY, TreeYaw, 1.0, Tall, Tint(1.0, 1.0, 1.0));
					}
					H.Clear(X, Y, 19.0);
				}
			}

			/** The holdings: a stone farmhouse on each, some ten hectares apiece, its haystacks and its cypresses, a
			 * low stone shed on some; its track out to the nearest road, lined with cypresses on a few. */
			void Holdings(Host& H, Rng& R, bool bTown, double TX, double TY, double TR, int& Built, int& Steads)
			{
				const double B = H.Band(), Spacing = 330.0;
				for (double GY = -B + 0.5 * Spacing; GY < FH + B; GY += Spacing)
					for (double GX = -B + 0.5 * Spacing; GX < FW + B; GX += Spacing)
					{
						const double JX = R.Range(-0.35, 0.35) * Spacing, JY = R.Range(-0.35, 0.35) * Spacing;
						for (int Try = 0; Try < 6; ++Try)
						{
							const double X = GX + JX + R.Range(-40.0, 40.0), Y = GY + JY + R.Range(-40.0, 40.0);
							const double Skew = R.Range(-0.25, 0.25), Turn_ = R.Next();
							const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi);
							if (X < -B + 30.0 || Y < -B + 30.0 || X > FW + B - 30.0 || Y > FH + B - 30.0) continue;
							if (bTown && std::hypot(X - TX, Y - TY) < TR * 1.5) continue;
							if (!CellOpen(X, Y) || !H.Free(X, Y, 9.0)) continue;
							// (on one tread: not across a terrace's riser)
							double Lo = 1e9, Hi = -1e9;
							for (int C = 0; C < 4; ++C)
							{
								const double U = (C & 1) ? 8.0 : -8.0, V = (C & 2) ? 5.0 : -5.0;
								const double Z = H.Land(X + U * std::cos(Yaw) - V * std::sin(Yaw), Y + U * std::sin(Yaw) + V * std::cos(Yaw));
								Lo = std::min(Lo, Z);
								Hi = std::max(Hi, Z);
							}
							if (Hi - Lo > 1.1) continue;
							if (!H.House(KitOf(KFarm), X, Y, Yaw, 14.8, 8.8)) continue;
							++Built;
							++Steads;
							Farm F;
							F.X = X;
							F.Y = Y;
							F.Yaw = Yaw;
							const double CA = std::cos(Yaw), SA = std::sin(Yaw);
							// a low stone shed beside it, on some
							const double ShedOdds = R.Next(), ShedSide = R.Next();
							if (ShedOdds < 0.35)
							{
								const double Side = ShedSide < 0.5 ? -1.0 : 1.0;
								if (H.House(KitOf(KHouseStone), X + Side * 15.5 * CA + 3.0 * SA, Y + Side * 15.5 * SA - 3.0 * CA, Yaw, 11.6, 11.2)) ++Built;
							}
							// the threshing floor before the house; a straw stack or two round their poles by it
							const double FloorAlong = R.Range(-5.0, 5.0), FloorAcross = R.Range(-5.0, 5.0);
							F.FloorX = X - 15.0 * SA + FloorAlong * CA;
							F.FloorY = Y + 15.0 * CA + FloorAcross * SA;
							const double Second = R.Next();
							const int Stacks = Second < 0.4 ? 2 : 1;
							for (int K = 0; K < Stacks; ++K)
							{
								const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(9.0, 13.0);
								const double PX = F.FloorX + D * std::cos(A), PY = F.FloorY + D * std::sin(A);
								const double Sz = R.Range(0.85, 1.02), Tall = R.Range(0.88, 1.12);
								// (the summer's new straw gold; the winter's old stack weathered grey-brown, part eaten)
								const double Old = 1.0 - Dry;
								const double StackYaw = R.Range(0.0, 2.0 * Pi), Eaten = R.Next();
								if (H.Free(PX, PY, 2.4)) H.Plant(KitOf(KStack), PX, PY, StackYaw, Sz, Tall * (1.0 - 0.3 * Old * Eaten), Tint(Lerp(1.04, 0.86, Old), Lerp(1.02, 0.85, Old), Lerp(0.97, 0.84, Old)));
							}
							// the prickly pears round the farm (the south's)
							if (bPears)
							{
								const double More = R.Next();
								const int Pears = 2 + static_cast<int>(More * 3.0);
								for (int K = 0; K < Pears; ++K)
								{
									const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(11.0, 22.0), PearYaw = R.Range(0.0, 2.0 * Pi), Sz = R.Range(0.95, 1.25), Tall = R.Range(0.9, 1.15);
									const double PX = X + D * std::cos(A), PY = Y + D * std::sin(A);
									if (H.Free(PX, PY, 1.6)) H.Plant(KitOf(KPear), PX, PY, PearYaw, Sz, Sz * Tall, Tint(1.0, 1.0, 1.0));
								}
							}
							// cypresses by the house
							const int Cyp = 1 + R.Below(3);
							for (int K = 0; K < Cyp; ++K)
							{
								const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(11.0, 16.0);
								const double PX = X + D * std::cos(A), PY = Y + D * std::sin(A);
								const double TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.15), Tall = R.Range(1.0, 1.5);
								if (H.Free(PX, PY, 1.5)) H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
							}
							// its track to the nearest road
							double Best = 170.0, BX = 0.0, BY = 0.0;
							for (size_t L = 0; L < H.Lanes(); ++L)
							{
								const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
								for (size_t K = 0; K < Line.size(); K += 2)
								{
									const double D = std::hypot(Line[K].first - X, Line[K].second - Y);
									if (D < Best)
									{
										Best = D;
										BX = Line[K].first;
										BY = Line[K].second;
									}
								}
							}
							if (Best < 170.0 && Best > 25.0)
							{
								F.bTrack = true;
								const double UX = (BX - X) / Best, UY = (BY - Y) / Best;
								F.AX = X + UX * 9.0;
								F.AY = Y + UY * 9.0;
								F.BX = BX;
								F.BY = BY;
								// lined with cypresses, on a few (a villa's drive)
								const double Avenue = R.Next();
								if (Avenue < 0.3)
									for (double S = 18.0; S < Best - 8.0; S += 8.0)
										for (int Side = -1; Side <= 1; Side += 2)
										{
											// (some 7 to 9 m apart)
											const double Along = R.Range(-1.0, 1.0), TreeYaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1), Tall = R.Range(1.15, 1.45);
											const double PX = X + UX * (S + Along) - UY * Side * 3.6, PY = Y + UY * (S + Along) + UX * Side * 3.6;
											if (H.Free(PX, PY, 1.2)) H.Plant(KitOf(KCypress), PX, PY, TreeYaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
										}
							}
							Farms.push_back(F);
							break;
						}
					}
			}

			/** Whether the rules' cell here is built up (the town's). */
			bool CellBuilt(double X, double Y) const
			{
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1), J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * Fd.Cols + I;
				if (K >= Fd.Ground.size()) return false;
				const int G = Fd.Ground[K];
				if (G < 0 || static_cast<size_t>(G) >= Fd.Grounds.size()) return false;
				const std::string& W = Fd.Grounds[static_cast<size_t>(G)];
				return W == "buildings" || W == "suburb" || W == "industry";
			}

			/** Whether the rules' cell here (the nearest at the field's edge, out in the band) is open or tilled land. */
			bool CellOpen(double X, double Y) const
			{
				const int I = std::clamp(static_cast<int>(std::floor(X / Fd.Cell)), 0, Fd.Cols - 1), J = std::clamp(static_cast<int>(std::floor(Y / Fd.Cell)), 0, Fd.Rows - 1);
				const size_t K = static_cast<size_t>(J) * Fd.Cols + I;
				if (K >= Fd.Ground.size()) return true;
				const int G = Fd.Ground[K];
				if (G < 0 || static_cast<size_t>(G) >= Fd.Grounds.size()) return true;
				const std::string& W = Fd.Grounds[static_cast<size_t>(G)];
				return W == "open" || W == "field" || W == "park";
			}

			// ------------------------------------------------------------------ walls

			/** Lay a run of wall, broken where it may not stand (a road's floor, a house, the water, out of the band),
			 * with a gap now and then and a stretch fallen low. */
			void Run(Host& H, Rng& R, const std::vector<std::pair<double, double>>& Line, double High, double Thick, double GapOdds, bool bTerrace)
			{
				std::vector<std::pair<double, double>> Cur;
				double Hi = High;
				auto Flush = [&]() {
					if (Cur.size() >= 3)
					{
						// (the fields' and the roads' walls laid in the kit's dry stone, each length its own height, the top
						// stepping unevenly; the wall men take cover behind a little lower and thinner inside them. The
						// terraces' risers as the ground's own walls.)
						if (bTerrace) H.Wall(Cur, Hi, Thick);
						else
						{
							H.Wall(Cur, 0.82 * Hi, 0.7 * Thick);
							LayStone(H, R, Cur, Hi);
						}
					}
					Cur.clear();
					// (a stretch fallen or bulged: some one in six low)
					const double Fallen = R.Next(), Low = R.Range(0.45, 0.7), Even = R.Range(0.92, 1.06);
					Hi = High * (Fallen < 0.16 ? Low : Even);
				};
				double Since = 0.0, Piece = R.Range(18.0, 60.0);
				for (size_t K = 0; K < Line.size(); ++K)
				{
					const double X = Line[K].first, Y = Line[K].second;
					if (K > 0) Since += std::hypot(X - Line[K - 1].first, Y - Line[K - 1].second);
					const Spot S = H.At(X, Y);
					bool bOk = S.bLaid && S.Lane > 0.4 && !S.bNear && S.Drawn != 9 && S.Drawn != 11 && S.Drawn != 7 && S.Kind != KWater && S.Kind != KMarsh;
					if (bOk && bTerrace && S.Kind == KVillage) bOk = false;
					if (!bOk)
					{
						Flush();
						Since = 0.0;
						continue;
					}
					if (Since > Piece)
					{
						Since = 0.0;
						Piece = R.Range(18.0, 60.0);
						const double Gap = R.Next();
						if (Gap < GapOdds)
						{
							Flush();
							continue;
						}
						Cur.push_back({ X, Y });
						Flush();
					}
					Cur.push_back({ X, Y });
				}
				Flush();
			}

			/** The kit's dry-stone wall along a run, in lengths of some 4 m (its 6 m piece scaled), each its own height and
			 * shade; none where a vehicle's recorded way crosses (the wall is cut there). */
			void LayStone(Host& H, Rng& R, const std::vector<std::pair<double, double>>& Line, double Hi)
			{
				std::vector<double> At(Line.size(), 0.0);
				for (size_t K = 1; K < Line.size(); ++K) At[K] = At[K - 1] + std::hypot(Line[K].first - Line[K - 1].first, Line[K].second - Line[K - 1].second);
				const double Total = At.back();
				if (Total < 2.0) return;
				const int N = std::max(1, static_cast<int>(std::lround(Total / 4.2)));
				const double Piece = Total / N;
				size_t Seg = 0;
				auto PointAt = [&](double S, double& OutX, double& OutY) {
					while (Seg + 2 < Line.size() && At[Seg + 1] < S) ++Seg;
					const double L = At[Seg + 1] - At[Seg], T = L > 1e-9 ? std::clamp((S - At[Seg]) / L, 0.0, 1.0) : 0.0;
					OutX = Lerp(Line[Seg].first, Line[Seg + 1].first, T);
					OutY = Lerp(Line[Seg].second, Line[Seg + 1].second, T);
				};
				double AX = 0.0, AY = 0.0;
				PointAt(0.0, AX, AY);
				for (int K = 0; K < N; ++K)
				{
					double BX = 0.0, BY = 0.0;
					PointAt((K + 1) * Piece, BX, BY);
					const double Taller = R.Range(0.9, 1.1), Shade = R.Range(0.93, 1.06);
					const double L = std::hypot(BX - AX, BY - AY);
					if (L > 0.8 && !H.At(AX, AY).bVehicle && !H.At(BX, BY).bVehicle)
						H.Plant(KitOf(KWall), 0.5 * (AX + BX), 0.5 * (AY + BY), std::atan2(BY - AY, BX - AX), (L + 0.08) / 6.06, Hi * Taller / 1.29,
							bStoneTrial ? Tint(Shade, Shade, 0.98 * Shade) : Tint(0.8 * Shade, 0.8 * Shade, 0.78 * Shade)); // (on trial, "medwall": the kit's stone as made)
					AX = BX;
					AY = BY;
				}
			}

			/** On trial ("medwall"): beside a wall, on one side, the scrub that grows where the plough cannot reach: a
			 * thicket of macchia some 6 to 8 m across and 2 to 4 m high at the stone's foot, and in the south now and
			 * then a prickly pear 2 to 4 m high, one every 11 to 16 m, a gap now and then; 4,000 in all at most. Along a
			 * Share of the lines asked, on the Side asked (-1 or 1; 0 either, by the line's own draw). One thicket a place,
			 * not a clump of small bushes: each is a draw for the soldier's eye, and from the general's height and from
			 * above a bush is a dot where a thicket is a dark mark beside the pale wall (a third of the fields' walls in
			 * bushes 2 m across darkened some twenty pixels of the general's picture). Each wholly off the men's and the
			 * vehicles' ways (its reach clear), slid along the wall to a free place or left out; its own draw (Key), so the
			 * walls themselves are laid as without it. */
			void Fringe(Host& H, const std::vector<std::pair<double, double>>& Line, uint64_t Key, double Share, double Side0)
			{
				if (Line.size() < 2 || Unit(Mix(Key ^ 0xF81)) >= Share) return;
				Rng Rf(Key ^ 0xF82);
				const double Side = Side0 != 0.0 ? Side0 : Rf.Next() < 0.5 ? -1.0 : 1.0;
				double S = 0.0, Next = Rf.Range(2.0, 9.0);
				for (size_t K = 1; K < Line.size() && Fringed < 4000; ++K)
				{
					const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
					if (DL < 1e-6) continue;
					const double UX = DX / DL, UY = DY / DL;
					S += DL;
					while (Next <= S && Fringed < 4000)
					{
						const double T = 1.0 - (S - Next) / DL, Pick = Rf.Next();
						Next += Rf.Range(11.0, 16.0);
						if (Pick < 0.12) continue;
						const bool bPear = bPears && Pick > 0.88;
						const double Yaw = Rf.Range(0.0, 2.0 * Pi), Size = bPear ? Rf.Range(1.4, 2.0) : Rf.Range(3.5, 4.8), Tall = Rf.Next(), Shade = Rf.Range(0.85, 1.0);
						// (its reach: the macchia's mound 0.85 m round as made, the pear's 0.9; its edge at the wall's foot, so
						// the stone stays seen over it and the line of cover reads; nearer, it swallowed the wall)
						const double Reach = (bPear ? 0.9 : 0.85) * Size, Off = 0.35 + 0.9 * Reach + Rf.Range(0.0, 0.5);
						for (int Try = 0; Try < 5; ++Try)
						{
							const double Slide = 1.5 * static_cast<double>((Try + 1) / 2) * (Try % 2 ? 1.0 : -1.0);
							const double X = Line[K - 1].first + T * DX + UX * Slide - UY * Side * Off, Y = Line[K - 1].second + T * DY + UY * Slide + UX * Side * Off;
							const Spot Sp = H.At(X, Y);
							if (!Sp.bLaid || Sp.Drawn == 9 || Sp.Drawn == 11 || Sp.Drawn == 7 || Sp.Kind == KWater || Sp.Kind == KMarsh || Sp.Kind == KLane || Sp.Kind == KVillage || Sp.Kind == KYard || !H.Free(X, Y, Reach)) continue;
							if (bPear) H.Plant(KitOf(KPear), X, Y, Yaw, Size, Size * Lerp(0.9, 1.1, Tall), Tint(Shade, Shade, Shade));
							else H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.65, 0.95, Tall), Tint(0.8 * Shade, 0.8 * Shade, 0.8 * Shade));
							++Fringed;
							break;
						}
					}
				}
			}

			void Walls(Host& H) override
			{
				Rng R(H.Seed() ^ 0xA115);
				Terraces(H, R);
				Fields(H, R);
				// The roads between dry-stone walls: along most of their length outside the village, both sides or one.
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					const double Half = H.LaneHalf(L);
					for (int Side = -1; Side <= 1; Side += 2)
					{
						std::vector<std::pair<double, double>> Run_;
						uint64_t Chunk = 0; // (on trial, "medwall": each walled stretch its own draw for the scrub on its field side)
						double S = 0.0;
						for (size_t K = 0; K < Line.size(); ++K)
						{
							const size_t A = K > 0 ? K - 1 : K, B = K + 1 < Line.size() ? K + 1 : K;
							const double DX = Line[B].first - Line[A].first, DY = Line[B].second - Line[A].second, DL = std::hypot(DX, DY);
							if (DL < 1e-6) continue;
							if (K > 0) S += std::hypot(Line[K].first - Line[K - 1].first, Line[K].second - Line[K - 1].second);
							const double Off = Half + 1.7;
							const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
							const bool bWalled = Noise(S / 110.0, static_cast<double>(L) * 7.3 + Side * 2.1, Seed ^ 0xA1A) > -0.15;
							if (!bWalled)
							{
								const double High = R.Range(0.9, 1.2);
								if (Run_.size() >= 3) Run(H, R, Run_, High, 0.6, 0.12, false);
								if (bStoneTrial && Run_.size() >= 3) Fringe(H, Run_, Hash(Hash(L ^ 0xF84, static_cast<uint64_t>(Side + 1)), Chunk++), 0.5, Side);
								Run_.clear();
								continue;
							}
							Run_.push_back({ X, Y });
						}
						const double High = R.Range(0.9, 1.2);
						if (Run_.size() >= 3) Run(H, R, Run_, High, 0.6, 0.12, false);
						if (bStoneTrial && Run_.size() >= 3) Fringe(H, Run_, Hash(Hash(L ^ 0xF84, static_cast<uint64_t>(Side + 1)), Chunk++), 0.5, Side);
					}
				}
				// The cemetery's walls.
				if (bCemetery)
				{
					const double CA = std::cos(CemYaw), SA = std::sin(CemYaw);
					auto P = [&](double U, double V) { return std::pair<double, double>{ CemX + U * CA - V * SA, CemY + U * SA + V * CA }; };
					std::vector<std::pair<double, double>> Ring = { P(-19.0, -2.5), P(-19.0, -12.0), P(19.0, -12.0), P(19.0, 12.0), P(-19.0, 12.0), P(-19.0, 2.5) };
					std::vector<std::pair<double, double>> Fine;
					for (size_t K = 0; K + 1 < Ring.size(); ++K)
					{
						const double L = std::hypot(Ring[K + 1].first - Ring[K].first, Ring[K + 1].second - Ring[K].second);
						const int N = std::max(1, static_cast<int>(std::ceil(L / 1.8)));
						for (int J = 0; J < N; ++J)
						{
							const double T = static_cast<double>(J) / N;
							Fine.push_back({ Lerp(Ring[K].first, Ring[K + 1].first, T), Lerp(Ring[K].second, Ring[K + 1].second, T) });
						}
					}
					Fine.push_back(Ring.back());
					H.Wall(Fine, bWallTrial ? 1.2 + 0.3 * Unit(Hash(H.Seed(), 0xCE3)) : 2.1, 0.55); // (on trial the art director's 1.2 to 1.5 m)
				}
			}
			std::unordered_set<uint64_t> WoodCells;

			/** Whether the edge between two parcels is walled: the groves' and the vineyards' most often, some of the rest. */
			bool Walled(const Spot& A, const Spot& B) const
			{
				for (const Spot* S : { &A, &B })
					if (S->Key == 0 || S->Kind == KLane || S->Kind == KWater || S->Kind == KMarsh || S->Kind == KVillage || S->Kind == KYard || !S->bLaid) return false;
				const uint64_t Pair = A.Key < B.Key ? Hash(A.Key, B.Key) : Hash(B.Key, A.Key);
				const bool bTrees = A.Crop == Olive || A.Crop == Vine || B.Crop == Olive || B.Crop == Vine;
				return Unit(Mix(Pair ^ 0xF1E1D)) < (bTrees ? 0.32 : 0.1);
			}

			/** The fields' walls: along a share of the parcels' edges, found where a coarse scan sees two parcels meet and
			 * traced finer there, then laid with their gateways. */
			void Fields(Host& H, Rng& R)
			{
				const double B = H.Band(), Coarse = 20.0, Fine = 4.0;
				const int CNX = static_cast<int>(std::ceil((FW + 2.0 * B) / Coarse)) + 1, CNY = static_cast<int>(std::ceil((FH + 2.0 * B) / Coarse)) + 1;
				std::vector<uint64_t> CKey(static_cast<size_t>(CNX) * CNY, 0);
				std::vector<Spot> CSpot(CKey.size());
				for (int J = 0; J < CNY; ++J)
					for (int I = 0; I < CNX; ++I)
					{
						const Spot S = H.At(-B + I * Coarse, -B + J * Coarse);
						CKey[static_cast<size_t>(J) * CNX + I] = S.Key;
						CSpot[static_cast<size_t>(J) * CNX + I] = S;
					}
				const int Per = static_cast<int>(Coarse / Fine);
				const int FNX = (CNX - 1) * Per + 1;
				std::unordered_map<uint64_t, Spot> Cache;
				auto FineAt = [&](int I, int J) -> const Spot& {
					const uint64_t K = static_cast<uint64_t>(J) * static_cast<uint64_t>(FNX) + static_cast<uint64_t>(I);
					auto Found = Cache.find(K);
					if (Found != Cache.end()) return Found->second;
					return Cache.emplace(K, H.At(-B + I * Fine, -B + J * Fine)).first->second;
				};
				std::unordered_map<uint64_t, std::vector<std::pair<double, double>>> Groups;
				std::unordered_set<uint64_t> EdgeDone;
				// a crossing between two fine points of different parcels, found to half a metre
				auto Crossing = [&](int IA, int JA, int IB, int JB) {
					const Spot& A = FineAt(IA, JA);
					const Spot& Bs = FineAt(IB, JB);
					if (A.Key == Bs.Key || !Walled(A, Bs)) return;
					const uint64_t EdgeK = (static_cast<uint64_t>(JA) * static_cast<uint64_t>(FNX) + static_cast<uint64_t>(IA)) * 2ull + (IB != IA ? 0ull : 1ull);
					if (!EdgeDone.insert(EdgeK).second) return;
					double T0 = 0.0, T1 = 1.0;
					const double AX = -B + IA * Fine, AY = -B + JA * Fine, BX = -B + IB * Fine, BY = -B + JB * Fine;
					for (int K = 0; K < 3; ++K)
					{
						const double T = 0.5 * (T0 + T1);
						if (H.At(Lerp(AX, BX, T), Lerp(AY, BY, T)).Key == A.Key) T0 = T;
						else T1 = T;
					}
					const double T = 0.5 * (T0 + T1);
					const uint64_t Pair = A.Key < Bs.Key ? Hash(A.Key, Bs.Key) : Hash(Bs.Key, A.Key);
					Groups[Pair].push_back({ Lerp(AX, BX, T), Lerp(AY, BY, T) });
				};
				for (int J = 0; J + 1 < CNY; ++J)
					for (int I = 0; I + 1 < CNX; ++I)
					{
						const size_t K = static_cast<size_t>(J) * CNX + I;
						const Spot* Corners[4] = { &CSpot[K], &CSpot[K + 1], &CSpot[K + CNX], &CSpot[K + CNX + 1] };
						bool bAny = false;
						for (int A = 0; A < 4 && !bAny; ++A)
							for (int Bc = A + 1; Bc < 4 && !bAny; ++Bc)
								if (Corners[A]->Key != Corners[Bc]->Key && Walled(*Corners[A], *Corners[Bc])) bAny = true;
						if (!bAny) continue;
						for (int FJ = 0; FJ <= Per; ++FJ)
							for (int FI = 0; FI <= Per; ++FI)
							{
								const int GI = I * Per + FI, GJ = J * Per + FJ;
								if (FI < Per) Crossing(GI, GJ, GI + 1, GJ);
								if (FJ < Per) Crossing(GI, GJ, GI, GJ + 1);
							}
					}
				// each edge's points chained into a line (nearest next, no farther than a fine step and a half), eased, laid
				for (auto& [Pair, Pts] : Groups)
				{
					if (Pts.size() < 4) continue;
					std::vector<uint8_t> Taken(Pts.size(), 0);
					size_t Left = Pts.size();
					while (Left > 0)
					{
						size_t Start = 0;
						while (Taken[Start]) ++Start;
						std::vector<std::pair<double, double>> Line{ Pts[Start] };
						Taken[Start] = 1;
						--Left;
						for (int Dir = 0; Dir < 2; ++Dir)
							for (;;)
							{
								const std::pair<double, double> End = Dir == 0 ? Line.back() : Line.front();
								double Best = 6.2;
								size_t Pick = Pts.size();
								for (size_t Q = 0; Q < Pts.size(); ++Q)
								{
									if (Taken[Q]) continue;
									const double D = std::hypot(Pts[Q].first - End.first, Pts[Q].second - End.second);
									if (D < Best)
									{
										Best = D;
										Pick = Q;
									}
								}
								if (Pick == Pts.size()) break;
								Taken[Pick] = 1;
								--Left;
								if (Dir == 0) Line.push_back(Pts[Pick]);
								else Line.insert(Line.begin(), Pts[Pick]);
							}
						if (Line.size() < 4) continue;
						std::vector<std::pair<double, double>> Eased(Line.size());
						for (size_t K = 0; K < Line.size(); ++K)
						{
							const size_t A = K > 0 ? K - 1 : K, Bn = K + 1 < Line.size() ? K + 1 : K;
							Eased[K] = { (Line[A].first + 2.0 * Line[K].first + Line[Bn].first) / 4.0, (Line[A].second + 2.0 * Line[K].second + Line[Bn].second) / 4.0 };
						}
						const double High = R.Range(0.85, 1.25);
						Run(H, R, Eased, High, 0.55, 0.3, false);
						if (bStoneTrial) Fringe(H, Eased, Hash(Pair ^ 0xF80, Start), 0.75, 0.0);
					}
				}
			}

			/** The terraces' walls: along the foot of every riser the band's slopes are stepped in (Relief), traced on the
			 * height's contours (marching squares on the valley side's own rise). */
			void Terraces(Host& H, Rng& R)
			{
				const double B = H.Band(), Step_ = 4.0;
				const double X0 = -B, Y0 = -B;
				const int NX = static_cast<int>(std::ceil((FW + 2.0 * B) / Step_)) + 1, NY = static_cast<int>(std::ceil((FH + 2.0 * B) / Step_)) + 1;
				std::vector<float> V(static_cast<size_t>(NX) * NY, -1.0f);
				for (int J = 0; J < NY; ++J)
					for (int I = 0; I < NX; ++I)
					{
						const double X = X0 + I * Step_, Y = Y0 + J * Step_, D = Out(X, Y);
						if (D < 30.0) continue;
						V[static_cast<size_t>(J) * NX + I] = static_cast<float>(Lift(X, Y, D, SideAt(X, Y)));
					}
				const double L0 = Rise * (1.0 - RiserRun);
				auto LevelBelow = [L0](double E) { return static_cast<int>(std::floor((E - L0) / Rise)); };
				// edge ids: the bottom edge of square (I, J) is 2 (J NX + I), its left edge 2 (J NX + I) + 1; a crossing by
				// its edge and its level
				auto KeyOf = [](uint64_t Edge, int Level) { return (Edge << 8) | static_cast<uint64_t>(Level & 0xFF); };
				std::unordered_map<uint64_t, std::pair<double, double>> Where;
				std::unordered_map<uint64_t, std::vector<uint32_t>> Links;
				std::vector<std::pair<uint64_t, uint64_t>> Segs;
				auto Cross = [&](int IA, int JA, int IB, int JB, uint64_t Edge, int Level) {
					const double VA = V[static_cast<size_t>(JA) * NX + IA], VB = V[static_cast<size_t>(JB) * NX + IB];
					const double Lv = L0 + Level * Rise;
					const double T = std::clamp((Lv - VA) / (VB - VA), 0.0, 1.0);
					const uint64_t Key = KeyOf(Edge, Level);
					if (!Where.count(Key)) Where[Key] = { X0 + Lerp(IA, IB, T) * Step_, Y0 + Lerp(JA, JB, T) * Step_ };
					return Key;
				};
				for (int J = 0; J + 1 < NY; ++J)
					for (int I = 0; I + 1 < NX; ++I)
					{
						const size_t K = static_cast<size_t>(J) * NX + I;
						const double C0 = V[K], C1 = V[K + 1], C2 = V[K + NX + 1], C3 = V[K + NX];
						if (C0 < 0.0 || C1 < 0.0 || C2 < 0.0 || C3 < 0.0) continue;
						const int Lo = LevelBelow(std::min({ C0, C1, C2, C3 })) + 1, Hi = LevelBelow(std::max({ C0, C1, C2, C3 }));
						for (int Lv = Lo; Lv <= Hi; ++Lv)
						{
							if (Lv < 0) continue;
							const double At = L0 + Lv * Rise;
							const int Case = (C0 >= At ? 1 : 0) | (C1 >= At ? 2 : 0) | (C2 >= At ? 4 : 0) | (C3 >= At ? 8 : 0);
							if (Case == 0 || Case == 15) continue;
							const uint64_t EB = 2ull * K, EL = 2ull * K + 1ull, ET = 2ull * (K + NX), ER = 2ull * (K + 1) + 1ull;
							auto Bottom = [&]() { return Cross(I, J, I + 1, J, EB, Lv); };
							auto Right = [&]() { return Cross(I + 1, J, I + 1, J + 1, ER, Lv); };
							auto Top = [&]() { return Cross(I, J + 1, I + 1, J + 1, ET, Lv); };
							auto Left = [&]() { return Cross(I, J, I, J + 1, EL, Lv); };
							auto Seg = [&](uint64_t A, uint64_t Bk) {
								Links[A].push_back(static_cast<uint32_t>(Segs.size()));
								Links[Bk].push_back(static_cast<uint32_t>(Segs.size()));
								Segs.push_back({ A, Bk });
							};
							switch (Case)
							{
							case 1: case 14: Seg(Left(), Bottom()); break;
							case 2: case 13: Seg(Bottom(), Right()); break;
							case 3: case 12: Seg(Left(), Right()); break;
							case 4: case 11: Seg(Right(), Top()); break;
							case 6: case 9: Seg(Bottom(), Top()); break;
							case 7: case 8: Seg(Left(), Top()); break;
							case 5:
								Seg(Left(), Top());
								Seg(Bottom(), Right());
								break;
							default:
								Seg(Left(), Bottom());
								Seg(Right(), Top());
								break;
							}
						}
					}
				// chained into lines
				std::vector<uint8_t> Used(Segs.size(), 0);
				auto Next = [&](uint64_t At, uint32_t From) -> int {
					const auto Found = Links.find(At);
					if (Found == Links.end()) return -1;
					for (const uint32_t S : Found->second)
						if (S != From && !Used[S]) return static_cast<int>(S);
					return -1;
				};
				for (uint32_t S0 = 0; S0 < Segs.size(); ++S0)
				{
					if (Used[S0]) continue;
					Used[S0] = 1;
					std::vector<uint64_t> Keys{ Segs[S0].first, Segs[S0].second };
					for (int Dir = 0; Dir < 2; ++Dir)
					{
						uint32_t Cur = S0;
						for (;;)
						{
							const uint64_t End = Dir == 0 ? Keys.back() : Keys.front();
							const int N = Next(End, Cur);
							if (N < 0) break;
							Used[static_cast<size_t>(N)] = 1;
							Cur = static_cast<uint32_t>(N);
							const uint64_t Other = Segs[Cur].first == End ? Segs[Cur].second : Segs[Cur].first;
							if (Dir == 0) Keys.push_back(Other);
							else Keys.insert(Keys.begin(), Other);
						}
					}
					if (Keys.size() < 4) continue;
					// the wall's line, where the terraces are laid; its height the riser's
					std::vector<std::pair<double, double>> Line;
					double WSum = 0.0;
					int WN = 0;
					auto Lay = [&]() {
						if (Line.size() >= 4 && WN > 0)
						{
							Run(H, R, Line, 0.3 + (WSum / WN) * Rise * (1.0 - TreadFall), 0.6, 0.18, true);
							// olives along the tread below the wall, on most terraces
							const double Planted = R.Next();
							if (Planted < 0.7)
							{
								double Since = R.Range(0.0, 8.0);
								for (size_t K = 1; K < Line.size(); ++K)
								{
									const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second;
									Since += std::hypot(DX, DY);
									if (Since < 9.5) continue;
									Since = R.Range(-1.5, 1.5);
									const double PX = Line[K].first, PY = Line[K].second;
									const double Side = SideAt(PX, PY);
									const double GX = Lift(PX + 2.0, PY, Out(PX + 2.0, PY), Side) - Lift(PX - 2.0, PY, Out(PX - 2.0, PY), Side);
									const double GY = Lift(PX, PY + 2.0, Out(PX, PY + 2.0), Side) - Lift(PX, PY - 2.0, Out(PX, PY - 2.0), Side);
									const double GL = std::hypot(GX, GY);
									if (GL < 1e-6) continue;
									const double Down = R.Range(4.5, 8.0);
									TerraceTrees.push_back({ PX - GX / GL * Down, PY - GY / GL * Down });
								}
							}
						}
						Line.clear();
						WSum = 0.0;
						WN = 0;
					};
					for (const uint64_t K : Keys)
					{
						const std::pair<double, double> P = Where[K];
						const double D = Out(P.first, P.second);
						const double Wt = Terraced(P.first, P.second, D, SideAt(P.first, P.second));
						if (Wt < 0.45 || D > H.Band() - 4.0)
						{
							Lay();
							continue;
						}
						Line.push_back(P);
						WSum += Wt;
						++WN;
					}
					Lay();
				}
			}

			// ------------------------------------------------------------------ plants

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A47);
				Groves(H, R);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// the woods (the south's old olive groves; the mainland's holm oak, its dark evergreen crowns), the maquis on
				// the rough, macchia in clumps across the fallow, a lone olive or two in the grazing, the field corners' cairns
				// of cleared stones, the sangars on the works
				const int Tries = static_cast<int>(W * D / 80.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.75, 1.3), Shade = R.Range(0.9, 1.1), Tall = R.Next();
					if (!H.Free(X, Y, 1.0)) continue;
					const Spot S = H.At(X, Y);
					if (S.Drawn == 9 || S.Drawn == 11 || S.Drawn == 7 || S.Kind == KWater || S.Kind == KLane) continue;
					const int C = CropAt(S);
					if (S.Kind == KWorks)
					{
						// (sangars, stone breastworks, some three a hectare)
						if (Keep < 0.0225 && H.Free(X, Y, 3.0)) H.Plant(KitOf(KSangar), X, Y, Yaw, 0.85 + 0.25 * Size, 1.0, Tint(1.0, 1.0, 1.0));
						continue;
					}
					if (S.Kind == KVillage)
					{
						// (the gardens' fruit trees, dark citrus and an olive or two)
						if (Keep < 0.094) H.Plant(KitOf(KOlive), X, Y, Yaw, Size * 0.8, Size * 0.75, Tint(0.7, 0.86, 0.66));
						continue;
					}
					if (S.Kind == KWoods && bSouth)
					{
						// (the south's woods: old olive groves on a loose lattice some 9 m, a tree missing here and there,
						// macchia in the gaps)
						const double Lat = 9.0, CA = std::cos(0.3), SA = std::sin(0.3);
						const double U = X * CA + Y * SA, V = -X * SA + Y * CA;
						const int64_t CI = static_cast<int64_t>(std::floor(U / Lat)), CJ = static_cast<int64_t>(std::floor(V / Lat));
						const uint64_t Cell = Hash(Seed ^ 0x0F1, static_cast<uint64_t>(CI), static_cast<uint64_t>(CJ));
						if (WoodCells.insert(Cell).second)
						{
							const double JU = 1.4 * (2.0 * Unit(Mix(Cell ^ 0x11)) - 1.0), JV = 1.4 * (2.0 * Unit(Mix(Cell ^ 0x12)) - 1.0);
							const double PU = (CI + 0.5) * Lat + JU, PV = (CJ + 0.5) * Lat + JV;
							const double PX = PU * CA - PV * SA, PY = PU * SA + PV * CA;
							const Spot Q = H.At(PX, PY);
							if (Q.Kind == KWoods && Q.Drawn != 7 && H.Free(PX, PY, 1.4))
							{
								const double Age = Unit(Mix(Cell ^ 0x13)), Silver = Unit(Mix(Cell ^ 0x14));
								const double Sz = 1.1 + 0.6 * Age;
								H.Plant(KitOf(KOlive), PX, PY, Yaw, Sz, Sz * (0.9 + 0.2 * Silver), Tint(0.92 + 0.1 * Silver, 0.95 + 0.08 * Silver, 0.92 + 0.12 * Silver));
							}
						}
						else if (Keep > 0.85) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.8, 1.4, Tall), Tint(Shade, Shade, Shade));
						continue;
					}
					if (S.Kind == KWoods)
					{
						if (Keep < 0.8)
						{
							// (holm oak, dark and evergreen; the deciduous oaks most of the wood, in leaf from April, bare
							// from December to March but for a few dead leaves)
							const double Which_ = R.Next();
							const bool bHolm = Which_ < 0.35;
							const double Sz = R.Range(0.48, 0.7);
							if (bHolm) H.Plant(LandField::Base::Oak, X, Y, Yaw, Sz, Sz * Lerp(0.85, 1.05, Tall), Tint(0.44 * Shade, 0.52 * Shade, 0.42 * Shade));
							else Deciduous(H, X, Y, Yaw, Sz / 0.6, Tall, Shade);
						}
						else H.Plant(KitOf(KMacchia), X, Y, Yaw, Size * 1.4, Size * 1.6, Tint(Shade, Shade, Shade));
						continue;
					}
					// (Sicily's and the south's wheat and fallow with trees standing in it here and there, olives and
					// carobs, some fields none, some a few to the hectare)
					if (C == Grain || C == Fallow || C == Plough)
					{
						static const double PerHa[5] = { 0.0, 0.0, 2.0, 4.0, 7.0 };
						const double Ha = PerHa[static_cast<int>(Unit(Mix(S.Key ^ 0x5CA7)) * 4.999)];
						if (Keep < Ha * 80.0 / 10000.0 && S.Edge > 5.0)
						{
							// (in the south carobs among them; on the mainland oaks and elms, bare in the winter)
							const double Which_ = R.Next();
							if (!bSouth && Which_ < 0.5) Deciduous(H, X, Y, Yaw, 0.75 * Size, Tall, Shade);
							else
							{
								const bool bCarob = bSouth && Which_ < 0.25;
								H.Plant(KitOf(KOlive), X, Y, Yaw, Size * (bCarob ? 1.5 : 1.35), Size * (bCarob ? 1.3 : 1.45), bCarob ? Tint(0.72, 0.84, 0.7) : Tint(1.04 * Dim, 1.07 * Dim, 1.05 * Dim));
							}
							continue;
						}
					}
					// (the south's field edges and roadsides: prickly pear, agave, now and then an agave's dry flower pole)
					if (bPears && C != Maquis && (S.Edge < 2.5 || (S.Lane > 2.6 && S.Lane < 6.0)) && Keep >= 0.5 && Keep < 0.65 && H.Free(X, Y, 1.4))
					{
						if (Keep < 0.5975) H.Plant(KitOf(KPear), X, Y, Yaw, Size * 0.95, Size * Lerp(0.95, 1.15, Tall), Tint(Shade, Shade, Shade));
						else if (Keep < 0.6375) H.Plant(KitOf(KAgave), X, Y, Yaw, Size, Size * Lerp(0.85, 1.15, Tall), Tint(Shade, Shade, Shade));
						else H.Plant(KitOf(KSpike), X, Y, Yaw, Size * 0.9, Lerp(0.9, 1.1, Tall), Tint(Shade, Shade, Shade));
						continue;
					}
					if (C == Maquis)
					{
						if (Keep < 0.79) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.7, 1.5, Tall), Tint(Shade, Shade, Shade));
						else if (Keep < 0.846) H.Plant(KitOf(KBoulder), X, Y, Yaw, Size, Size * 0.8, Tint(1.08, 1.06, 1.02));
						else if (Keep < 0.884) H.Plant(KitOf(KStones), X, Y, Yaw, Size, Size, Tint(1.1, 1.08, 1.04));
						continue;
					}
					if (C == Fallow)
					{
						// (macchia in clumps across it, some 14 a hectare, thicker along its edges; stones cleared into heaps)
						if (Keep < 0.105 || (S.Edge < 4.0 && Keep < 0.31)) H.Plant(KitOf(KMacchia), X, Y, Yaw, Size, Size * Lerp(0.7, 1.4, Tall), Tint(Shade, Shade, Shade));
						else if (Keep > 0.31 && Keep < 0.355) H.Plant(KitOf(KStones), X, Y, Yaw, Size * 1.1, Size, Tint(1.12, 1.1, 1.05));
						continue;
					}
					if ((C == Grain || C == Plough) && S.Edge < 3.0 && Keep < 0.019) H.Plant(KitOf(KStones), X, Y, Yaw, Size * 1.2, Size, Tint(1.12, 1.1, 1.05));
				}
				// the olives on the terraces' treads (Walls found them)
				for (const auto& [X, Y] : TerraceTrees)
				{
					const double Yaw = R.Range(0.0, 2.0 * Pi), Age = R.Next(), Silver = R.Next(), Tall = R.Range(0.9, 1.1);
					if (!H.Free(X, Y, 1.4)) continue;
					const Spot S = H.At(X, Y);
					if (S.Drawn == 7 || S.Drawn == 10 || S.Kind == KWoods || S.Kind == KWater || S.Kind == KVillage) continue;
					// (olives; in the south whole hillsides of almonds among them)
					if (bSouth && Noise(X / 260.0, Y / 260.0, Seed ^ 0xA1D) > 0.15)
					{
						Almond_(H, X, Y, Yaw, 0.75 + 0.2 * Age, Tall, 0.94 + 0.12 * Silver);
						continue;
					}
					const double Sz = 1.05 + 0.6 * Age;
					H.Plant(KitOf(KOlive), X, Y, Yaw, Sz, Sz * Tall, Tint(Dim * (0.92 + 0.1 * Silver), Dim * (0.95 + 0.08 * Silver), Dim * (0.92 + 0.12 * Silver)));
				}
				Lanes(H, R);
				HillTown(H, R);
			}

			/** Walk a parcel's lattice slot by slot out from where it was found (GA apart along the way (Ca, Sa) from its
			 * middle (OX, OY), GC across it, the rows wavering by Wave metres), calling Lay(X, Y, Spot) at each slot inside
			 * it. */
			template <typename F>
			static void Lattice(Host& H, const Spot& S, double OX, double OY, double Ca, double Sa, double GA, double GC, double Wave, F&& Lay)
			{
				const uint64_t Key = S.Key;
				const double SAlong = (S.X - OX) * Ca + (S.Y - OY) * Sa, SAcross = -(S.X - OX) * Sa + (S.Y - OY) * Ca;
				const double A0 = GA * Unit(Mix(Key ^ 0x0A3)), C0 = GC * Unit(Mix(Key ^ 0x0A4));
				auto Slot = [&](int I, int J, double& OutX, double& OutY) {
					const double Along = A0 + I * GA, Across = C0 + J * GC + Wave * Noise(Along / 45.0, J * 1.7, Key);
					OutX = OX + Along * Ca - Across * Sa;
					OutY = OY + Along * Sa + Across * Ca;
				};
				// (begun from the slots round where the parcel was found, so one found near its edge, its nearest slot
				// outside it, is walked all the same)
				std::unordered_set<int64_t> Done;
				std::vector<std::pair<int, int>> Todo;
				const int I0 = static_cast<int>(std::lround((SAlong - A0) / GA)), J0 = static_cast<int>(std::lround((SAcross - C0) / GC));
				for (int DI = -1; DI <= 1; ++DI)
					for (int DJ = -1; DJ <= 1; ++DJ)
					{
						Todo.push_back({ I0 + DI, J0 + DJ });
						Done.insert(static_cast<int64_t>(I0 + DI) * 100003 + (J0 + DJ));
					}
				while (!Todo.empty())
				{
					const auto [I, J] = Todo.back();
					Todo.pop_back();
					double PX = 0.0, PY = 0.0;
					Slot(I, J, PX, PY);
					const Spot Q = H.At(PX, PY);
					if (Q.Key != Key) continue;
					for (const auto& [DI, DJ] : { std::pair<int, int>{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
						if (Done.insert(static_cast<int64_t>(I + DI) * 100003 + (J + DJ)).second) Todo.push_back({ I + DI, J + DJ });
					Lay(PX, PY, Q);
				}
			}

			/** The groves: in each olive grove the trees on its own loose rows some ten metres apart (fewer than 150 a
			 * hectare), the rows wavering, a tree missing here and there, old and young; the almonds on a closer, straighter
			 * grid, smaller, their leaves a fresher green (bare in the winter, in blossom in Sicily's February). And the
			 * summer's stooks on the stubble, in rows (on most fields: some carted off already). Each parcel found from a
			 * coarse scan, then walked slot by slot from where it was found. */
			void Groves(Host& H, Rng& R)
			{
				const double B = H.Band();
				std::unordered_set<uint64_t> Seen;
				for (double SY = -B + 10.0; SY < FH + B; SY += 20.0)
					for (double SX = -B + 10.0; SX < FW + B; SX += 20.0)
					{
						const Spot S = H.At(SX, SY);
						// (the stooks on the stubble: a wheat field's, or the stubble strips of one worked in strips)
						const bool bStooks = Stage == 2 && (S.Crop == Grain || ((S.Crop == Fallow || S.Crop == Plough) && Unit(Mix(S.Key ^ 0x57A1)) <= 0.6));
						if ((S.Crop != Olive && S.Crop != Almond && !bStooks) || S.Key == 0 || S.Kind == KWoods || S.Kind == KVillage) continue;
						if (!Seen.insert(S.Key).second) continue;
						const uint64_t Key = S.Key;
						if (bStooks)
						{
							// (sheaves stooked in rows some 9 m apart, a stook every 7 m along them)
							if (Unit(Mix(Key ^ 0x5700)) > 0.85) continue;
							// (along the strips: the nearest road's way)
							double DC = 1.0, DS = 0.0;
							StripWay(S, DC, DS);
							const double Ca = std::cos(S.Angle), Sa = std::sin(S.Angle);
							const double OX = S.X - (S.Along * Ca - S.Across * Sa), OY = S.Y - (S.Along * Sa + S.Across * Ca);
							Lattice(H, S, OX, OY, DC, DS, 7.0, 9.0, 0.6, [&](double PX, double PY, const Spot& Q) {
								const double Miss = R.Next(), JX = R.Range(-0.8, 0.8), JY = R.Range(-0.8, 0.8), Yaw = R.Range(0.0, 2.0 * Pi), Sz = R.Range(0.9, 1.1), Tall = R.Range(0.92, 1.08);
								if (Miss < 0.15 || Q.Edge < 3.0 || CropAt(Q) != Grain) return;
								const double X = PX + JX, Y = PY + JY;
								if (H.Free(X, Y, 0.8)) H.Plant(KitOf(KStook), X, Y, Yaw, Sz, Sz * Tall, Tint(1.0, 1.0, 1.0));
							});
							continue;
						}
						const bool bAlmond = S.Crop == Almond;
						const bool bOld = S.Kind == KOpen;
						const double GA = bAlmond ? 6.5 + 1.0 * Unit(Mix(Key ^ 0x0A1)) : 8.5 + 2.5 * Unit(Mix(Key ^ 0x0A1));
						const double GC = bAlmond ? 7.0 + 1.0 * Unit(Mix(Key ^ 0x0A2)) : 9.5 + 2.0 * Unit(Mix(Key ^ 0x0A2));
						const double Ca = std::cos(S.Angle), Sa = std::sin(S.Angle);
						const double OX = S.X - (S.Along * Ca - S.Across * Sa), OY = S.Y - (S.Along * Sa + S.Across * Ca);
						Lattice(H, S, OX, OY, Ca, Sa, GA, GC, bAlmond ? 0.4 : 1.4, [&](double PX, double PY, const Spot& Q) {
							const double Miss = R.Next(), JX = R.Range(-1.4, 1.4), JY = R.Range(-1.4, 1.4), Yaw = R.Range(0.0, 2.0 * Pi);
							const double Age = R.Next(), Silver = R.Range(0.0, 1.0), Tall = R.Range(0.9, 1.12);
							if (Miss < (bAlmond ? 0.08 : bOld ? 0.3 : 0.12) || Q.Edge < 2.5) return;
							const double X = PX + (bAlmond ? 0.3 : 1.0) * JX, Y = PY + (bAlmond ? 0.3 : 1.0) * JY;
							if (!H.Free(X, Y, 1.4)) return;
							if (bAlmond)
							{
								Almond_(H, X, Y, Yaw, 0.72 + 0.22 * Age, Tall, 0.94 + 0.12 * Silver);
								return;
							}
							// (centuries old most of them, a few young ones replanted; the crowns speckled toward silver)
							const double Sz = Age < 0.15 ? 0.85 + 0.3 * Age / 0.15 : 1.2 + 0.55 * (Age - 0.15) / 0.85;
							H.Plant(KitOf(KOlive), X, Y, Yaw, Sz, Sz * Tall, Tint(Dim * (0.92 + 0.1 * Silver), Dim * (0.95 + 0.08 * Silver), Dim * (0.92 + 0.12 * Silver)));
						});
					}
			}

			/** An almond tree (Sz as the olive's): in leaf a fresher, yellower green than the olives; bare in the winter, its
			 * blossom pale over its twigs in Sicily's January and February. */
			void Almond_(Host& H, double X, double Y, double Yaw, double Sz, double Tall, double Shade) const
			{
				if (Leaf < 0.5)
				{
					const double Bloom = bSouth && (Fd.Month == 1 || Fd.Month == 2) ? 1.0 : 0.0;
					H.Plant(KitOf(KBare), X, Y, Yaw, 0.42 * Sz / 0.85, 0.42 * Sz / 0.85 * Tall, Tint(Lerp(1.0, 1.7, Bloom) * Shade, Lerp(1.0, 1.6, Bloom) * Shade, Lerp(1.0, 1.62, Bloom) * Shade));
					return;
				}
				H.Plant(KitOf(KOlive), X, Y, Yaw, Sz, Sz * Tall, Tint(0.98 * Shade, 1.1 * Shade, 0.76 * Shade));
			}

			/** A deciduous broadleaf tree (an oak, an elm; Sz as the bare tree's, some 10 m): bare from December to March
			 * (the bare tree), in leaf after (the bocage's oak, browning into the autumn). */
			void Deciduous(Host& H, double X, double Y, double Yaw, double Sz, double Tall, double Shade) const
			{
				if (Leaf < 0.5)
				{
					H.Plant(KitOf(KBare), X, Y, Yaw, Sz, Sz * Lerp(0.88, 1.08, Tall), Tint(Shade, Shade, Shade));
					return;
				}
				const double S = 0.6 * Sz;
				H.Plant(LandField::Base::Oak, X, Y, Yaw, S, S * Lerp(0.85, 1.05, Tall), Tint(Lerp(1.05, 0.62, Leaf) * Shade, Lerp(0.78, 0.7, Leaf) * Shade, 0.5 * Shade));
			}

			/** Along the roads: rows of cypress here and there, the telegraph line beside the main road. */
			void Lanes(Host& H, Rng& R)
			{
				size_t Main = 0;
				double Longest = 0.0;
				for (size_t L = 0; L < H.Lanes(); ++L)
				{
					const std::vector<std::pair<double, double>>& Line = H.LaneLine(L);
					double Len = 0.0;
					for (size_t K = 1; K < Line.size(); ++K) Len += std::hypot(Line[K].first - Line[K - 1].first, Line[K].second - Line[K - 1].second);
					if (Len > Longest)
					{
						Longest = Len;
						Main = L;
					}
					// a row of cypresses on one side, now and then (a villa's frontage, a chapel, a crossing); on the
					// mainland half the rows plane trees and elms, bare in the winter
					const double Half = H.LaneHalf(L);
					double S = 0.0, NextRow = R.Range(150.0, 400.0), RowEnd = -1.0;
					int Side = 1;
					bool bBroad = false;
					for (size_t K = 1; K < Line.size(); ++K)
					{
						const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
						if (DL < 1e-6) continue;
						S += DL;
						if (S > NextRow && RowEnd < 0.0)
						{
							RowEnd = S + R.Range(30.0, 90.0);
							const double Which_ = R.Next(), Kind_ = R.Next();
							Side = Which_ < 0.5 ? -1 : 1;
							bBroad = !bSouth && Kind_ < 0.5;
							if (bBroad) RowEnd += 60.0;
						}
						if (RowEnd > 0.0)
						{
							if (std::fmod(S, bBroad ? 9.0 : 7.0) < DL)
							{
								const double Off = Half + (bBroad ? 4.2 : 3.4);
								const double X = Line[K].first - DY / DL * Side * Off, Y = Line[K].second + DX / DL * Side * Off;
								const double Yaw = R.Range(0.0, 2.0 * Pi), Wide = R.Range(0.9, 1.1), Tall = R.Range(1.1, 1.5);
								if (H.Free(X, Y, 1.2) && H.At(X, Y).Drawn != 7)
								{
									if (bBroad) Deciduous(H, X, Y, Yaw, 0.85 * Wide, (Tall - 1.1) / 0.4, 1.0);
									else H.Plant(KitOf(KCypress), X, Y, Yaw, Wide, Tall, Tint(1.0, 1.0, 1.0));
								}
							}
							if (S > RowEnd)
							{
								RowEnd = -1.0;
								NextRow = S + R.Range(250.0, 600.0);
							}
						}
					}
				}
				if (H.Lanes() == 0) return;
				// the poles, 50 m apart
				const std::vector<std::pair<double, double>>& Line = H.LaneLine(Main);
				const double Off = H.LaneHalf(Main) + 3.2;
				double S = 0.0;
				for (size_t K = 1; K < Line.size(); ++K)
				{
					const double DX = Line[K].first - Line[K - 1].first, DY = Line[K].second - Line[K - 1].second, DL = std::hypot(DX, DY);
					if (DL < 1e-6) continue;
					S += DL;
					if (std::fmod(S, 50.0) >= DL) continue;
					const double X = Line[K].first + DY / DL * Off, Y = Line[K].second - DX / DL * Off;
					const double Tall = R.Range(0.97, 1.03);
					if (H.Free(X, Y, 0.6)) H.Plant(KitOf(KPole), X, Y, std::atan2(DY, DX) + 0.5 * Pi, 1.0, Tall, Tint(1.0, 1.0, 1.0));
				}
			}

			/** The hill town on the crest beyond the field: its houses packed on the top, its tower over them, cypresses
			 * on its slopes (seen kilometres off, under the haze). */
			void HillTown(Host& H, Rng& R)
			{
				std::vector<std::pair<double, double>> Laid;
				const double Reach = 150.0;
				for (int K = 0; K < 1400 && Laid.size() < 110; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = Reach * std::sqrt(R.Next());
					const double X = HillX + D * std::cos(A), Y = HillY + 10.0 + D * std::sin(A);
					const double Pick = R.Next(), Skew = R.Range(-0.15, 0.15), Turn_ = R.Next();
					const double Yaw = Skew + (Turn_ < 0.5 ? 0.0 : 0.5 * Pi) + 0.3;
					const double Room = Pick < 0.6 ? 9.5 : 8.0;
					// (on the crest's flat top, its front houses at the brow)
					bool bClear = std::hypot(X - HillX, Y - HillY + 20.0) > 14.0 && CrestC(X, Y) < 150.0;
					for (const auto& [QX, QY] : Laid)
						if (std::hypot(X - QX, Y - QY) < Room + 1.5) bClear = false;
					if (!bClear) continue;
					Laid.push_back({ X, Y });
					// (three and four storeys packed on the crest)
					const double Sc = R.Range(0.85, 1.0), Tall = R.Range(1.3, 1.9);
					if (Pick < 0.6) H.Plant(KitOf(KFarm), X, Y, Yaw, Sc, Tall, Tint(1.0, 1.0, 1.0));
					else H.Plant(KitOf(KHouseStone), X, Y, Yaw, Sc * 0.9, Tall * 1.2, Tint(1.0, 1.0, 1.0));
				}
				H.Plant(KitOf(KTower), HillX, HillY - 20.0, 0.3, 1.4, 1.6, Tint(1.0, 1.0, 1.0));
				for (int K = 0; K < 24; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(Reach + 10.0, Reach + 90.0);
					const double Yaw = R.Range(0.0, 2.0 * Pi), Tall = R.Range(1.1, 1.5);
					const double X = HillX + D * std::cos(A), Y = HillY + 10.0 + D * std::sin(A);
					if (CrestC(X, Y) < 300.0) H.Plant(KitOf(KCypress), X, Y, Yaw, 1.0, Tall, Tint(1.0, 1.0, 1.0));
				}
			}

			// ------------------------------------------------------------------ the effects

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "rough" || Ground == "rock") return 7; // (the hills' limestone: rock, never mud)
				return Wet > 0.6 && Else == 0 ? 4 : Else; // (the winter's earth is mud)
			}
			const char* Earth() const override { return "terrarossa"; }
			// (grey-ochre field stone, lit some 170 to 190 in the sun: the kit's 8C7E66 and A8987C between; its face in the
			// shade a dark grey-brown, never black. On trial, "medwall", warmer and a little paler: the wall the soldier's
			// eye sees in the fight is mostly this one, the kit's stone kept off the men's recorded ways. 160,146,118 lit
			// it 195 in the July lane, over the band, and on Sicily's pale summer ground from the general's height it read
			// 14 under the ground, a faint line; this, 183 in the lane and 20 under, while on Algiers' dark winter ground
			// it stays 26 over: one stone that reads on both)
			Tone WallStone() const override { return bStoneTrial ? Rgb(148, 132, 100) : Rgb(142, 130, 106); }
			double WallIvy() const override { return 0.08; }
			bool WallBroken() const override { return bWallTrial; }

		private:
			static constexpr double Margin = 300.0, FarmCell = 100.0;
			double FW = 0.0, FH = 0.0;
			double Dry = 1.0, Wet = 0.0, Green = 0.0, Leaf = 1.0, Turn = 0.0;
			int Stage = 2;
			bool bSouth = false, bPears = false, bSnowCaps = false;
			double Dim = 1.0; // (the olives' silver crowns a little darker under the mainland's winter light, wet)
			double HillX = 0.0, HillY = 0.0, HillTop = 0.0;
			std::vector<Exit> Exits;
			std::vector<Farm> Farms;
			std::vector<std::vector<uint32_t>> FarmGrid;
			int FarmNX = 0, FarmNY = 0;
			bool bTower = false, bCemetery = false;
			/** The walls on trial (Field::Tried("walltop")): broken along their tops, the cemetery's 1.2 to 1.5 m, not 2.1. */
			bool bWallTrial = false;
			/**
			 * The walls' stone on trial, the word "medwall" (the game's -TheaterLandTrial=medwall, the cloud painter's
			 * LAND_FIELD_TRIAL=medwall; it stacks with "walltop"): warm pale dry stone that draws the fields as the bocage's
			 * hedges draw Normandy's. Far off the kit's wall in two courses under a pale ragged cap, not six blocks that
			 * read as dark palings; near, the faces of its stones that look up lifted toward the cap's pale, so the tops
			 * catch a low sun; far off a darker foot under the pale courses; the stone paler and warmer (the kit's as
			 * made, not tinted to 0.8; the ground's own walls 148,132,100); and along three in four of the fields' walls
			 * and half the lanes' (on their field side), on one side, thickets of macchia and, in the south, prickly pear
			 * at the stone's foot, a dark mark beside the pale line from the general's height and from above (4,000
			 * at most; Sicily's and Algiers' lay some 980). Without the word, as before.
			 */
			bool bStoneTrial = false;
			int Fringed = 0; // (the fringe's bushes laid so far)
			std::vector<float> LiftGrid;
			double GridX0 = 0.0, GridY0 = 0.0, GridStep = 50.0;
			static constexpr double MotStep = 8.0, MotMargin = 400.0;
			std::vector<float> MotGrid; // (M, Mid, Warm, the soil's red: four to a point)
			int MNX = 0, MNY = 0;
			static constexpr double RoadStep = 100.0, RoadMargin = 400.0;
			std::vector<float> RoadDir; // (the nearest road's way by 100 m squares: its cosine and sine, two to a square)
			int RNX = 0, RNY = 0;
			int GridNX = 0, GridNY = 0;
			std::vector<std::pair<double, double>> TerraceTrees;
			double TowerX = 0.0, TowerY = 0.0, CemX = 0.0, CemY = 0.0, CemYaw = 0.0;
		};
	}

	std::unique_ptr<Region> MakeMed(const LandField::Field& F, uint64_t S) { return std::make_unique<Med>(F, S); }
}
