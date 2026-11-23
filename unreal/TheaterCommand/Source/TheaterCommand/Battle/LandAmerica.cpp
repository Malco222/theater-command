#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

/**
 * North American farm country: the corn belt, the wheat plains, the dairy country of the east and the Great Lakes, the
 * upper South (a sandbox's invasion of America, a war carried to Canada or the Middle West). Brian, 23 November 2026: "do
 * 1 and 3" (1: the three missing battlefields; North America's farmland second).
 *
 *   - the land laid out by the survey: fields square to the compass on the half-mile grid (Region::Surveyed), forties
 *     and eighties and twenties, each its crop of the month: the corn in rows a yard apart, knee high by July and over a
 *     man's head in August, dry and shocked in October, its stalks through the winter's snow; winter wheat green from
 *     the fall and gold in June or July, shocked, then stubble; oats; clover and alfalfa hay, cut twice and stacked;
 *     pasture with a lone bur oak; the plains' wheat and summer fallow, the east's hay and pasture;
 *   - barbed wire on its posts along the fields' lines and the roads, a worm fence of split rails in the east, weeds
 *     and grass in the fence rows; telephone poles along the roads, their wires;
 *   - farmsteads along the roads every half mile or so: a white clapboard house with its porch, a red barn under its
 *     gambrel roof, a stave silo, a windmill, a corn crib and a machine shed round the yard, a windbreak to the north
 *     and west, shade trees, an orchard;
 *   - a small town: brick and frame stores along main street, their awnings, frame houses on lots along the side streets
 *     under the elms, a white church and its steeple, the grain elevator at the edge of town;
 *   - woodlots of oak, maple, ash and elm, the creeks' trees.
 *
 * It reads no battle models: every house, barn and fence it builds itself (Region::KitBuilt); its trees are the frame's own
 * oak, elm, ash, beech (for the maples) and apple.
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Corn = 0,    // maize in rows a yard apart
			Wheat = 1,   // winter wheat
			Oats = 2,    // spring oats
			Hay = 3,     // clover, timothy and alfalfa, cut twice
			Pasture = 4, // grazing
			Fallow = 5   // ploughed and lying fallow (the plains' summer fallow; the fall's ploughing)
		};

		enum Slot : int
		{
			KHouse = 0,     // a white clapboard farmhouse, two storeys, its porch along the front
			KBarn = 1,      // a red barn under a gambrel roof
			KSilo = 2,      // a stave silo, domed
			KMill = 3,      // a windmill on its steel tower
			KCrib = 4,      // a slatted corn crib
			KShed = 5,      // a machine shed, its bays open
			KStore = 6,     // a brick store on main street, two storeys, its shop window and awning
			KChurch = 7,    // a white frame church and its steeple
			KElevator = 8,  // the grain elevator
			KPole = 9,      // a telephone pole, its crossarm and two wires to the next
			KPost = 10,     // a fence post, three strands of barbed wire to the next
			KRail = 11,     // a worm fence's zig and zag of split rails
			KShock = 12,    // a corn shock (a grain shock by its size and tint)
			KStack = 13,    // a haystack
			KHouse2 = 14,   // a gable-front house, a storey and a half
			KFront = 15     // a frame store behind its false front
		};
		constexpr double PostStep = 5.0, PoleStep = 40.0, RailStep = 6.4;
		// (the survey's quarter of a half-mile block: the lines its fields' fences may run on)
		constexpr double SurveyLine = 804.67 / 4.0;

		enum Clut : int
		{
			CGrass = 0,   // a grass tuft
			CCorn = 1,    // two rows of corn plants
			CGrain = 2,   // a clump of wheat or oats
			CStubble = 3, // stubble, the corn's stalks cut or broken
			CWeed = 4,    // the fence row's weeds
			CDry = 5      // the fall's standing dry corn, and its stalks left over the winter
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (sRGB as painted, the art director's to check)
		const Tone CornGreen = Rgb(56, 84, 42);
		const Tone CornDry = Rgb(160, 140, 98);
		const Tone CornStalk = Rgb(146, 128, 94);
		const Tone WheatGreen = Rgb(88, 118, 58);
		const Tone WheatGold = Rgb(184, 158, 94);
		const Tone StubbleC = Rgb(174, 156, 112);
		const Tone OatGreen = Rgb(98, 124, 66);
		const Tone OatGold = Rgb(176, 164, 110);
		const Tone HayGreen = Rgb(84, 114, 58);
		const Tone HayCut = Rgb(138, 138, 88);
		const Tone HayDormant = Rgb(128, 120, 84);
		const Tone Gravel = Rgb(166, 154, 130);
		const Tone YardDirt = Rgb(128, 112, 88);
		const Tone Litter = Rgb(108, 92, 70);
		const Tone White = Rgb(222, 220, 210);
		const Tone Trim = Rgb(232, 230, 222);
		const Tone BarnRed = Rgb(140, 52, 40);
		const Tone Shingle = Rgb(82, 78, 74);
		const Tone Tin = Rgb(126, 126, 122);
		const Tone Glass = Rgb(52, 58, 64);
		const Tone Brick = Rgb(146, 86, 64);

		struct V3
		{
			double X, Y, Z;
		};
		V3 Sub(V3 A, V3 B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		V3 Add(V3 A, V3 B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		V3 Times(V3 A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
		V3 Cross(V3 A, V3 B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		V3 Norm(V3 A)
		{
			const double L = std::sqrt(A.X * A.X + A.Y * A.Y + A.Z * A.Z);
			return L > 1e-12 ? V3{ A.X / L, A.Y / L, A.Z / L } : V3{ 0.0, 0.0, 1.0 };
		}

		/** A flat face of three or more corners (convex, in order), lit flat, turned to face Out. */
		void Face(Writer& W, std::initializer_list<V3> Corners, V3 Out, const Tone& C)
		{
			std::vector<V3> Q(Corners);
			V3 N = Norm(Cross(Sub(Q[1], Q[0]), Sub(Q[2], Q[0])));
			if (N.X * Out.X + N.Y * Out.Y + N.Z * Out.Z < 0.0)
			{
				std::reverse(Q.begin(), Q.end());
				N = { -N.X, -N.Y, -N.Z };
			}
			std::vector<uint32_t> I;
			for (const V3& P : Q) I.push_back(W.Vert(P.X, P.Y, P.Z, N.X, N.Y, N.Z, C));
			for (size_t K = 1; K + 1 < I.size(); ++K) W.Tri(I[0], I[K], I[K + 1]);
		}

		/** A window or a door: a flat pane a hand off a wall. Dir: 0 the -y wall, 1 +y, 2 -x, 3 +x; Plane the wall's y or x;
		 * A its middle along the wall, Z its middle's height, HA and HZ its half width and half height. */
		void Pane(Writer& W, int Dir, double Plane, double A, double Z, double HA, double HZ, const Tone& C)
		{
			const double E = 0.03;
			switch (Dir)
			{
			case 0: Face(W, { { A - HA, Plane - E, Z - HZ }, { A + HA, Plane - E, Z - HZ }, { A + HA, Plane - E, Z + HZ }, { A - HA, Plane - E, Z + HZ } }, { 0, -1, 0 }, C); break;
			case 1: Face(W, { { A - HA, Plane + E, Z - HZ }, { A + HA, Plane + E, Z - HZ }, { A + HA, Plane + E, Z + HZ }, { A - HA, Plane + E, Z + HZ } }, { 0, 1, 0 }, C); break;
			case 2: Face(W, { { Plane - E, A - HA, Z - HZ }, { Plane - E, A + HA, Z - HZ }, { Plane - E, A + HA, Z + HZ }, { Plane - E, A - HA, Z + HZ } }, { -1, 0, 0 }, C); break;
			default: Face(W, { { Plane + E, A - HA, Z - HZ }, { Plane + E, A + HA, Z - HZ }, { Plane + E, A + HA, Z + HZ }, { Plane + E, A - HA, Z + HZ } }, { 1, 0, 0 }, C); break;
			}
		}

		/** A window with its white frame round it (a pane a little larger behind). */
		void Window(Writer& W, int Dir, double Plane, double A, double Z, double HA, double HZ)
		{
			Pane(W, Dir, Plane, A, Z, HA + 0.08, HZ + 0.08, Trim);
			Pane(W, Dir, Plane + (Dir == 0 || Dir == 2 ? -0.01 : 0.01), A, Z, HA, HZ, Glass);
		}

		/** A gable roof along x over walls Long by Deep standing at CX, CY: its eaves at Eave, its ridge at Ridge, overhanging
		 * Over at the eaves and End at the gables; its gables' walls (Wall) closing it, its underside darker. */
		void Gable(Writer& W, double CX, double CY, double Long, double Deep, double Eave, double Ridge, double Over, double End, const Tone& Roof, const Tone& Wall)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep, Slope = (Ridge - Eave) / HD;
			const double X0 = CX - HL - End, X1 = CX + HL + End, YE = HD + Over, ZE = Eave - Over * Slope;
			const Tone Under = Varied(Roof, 0.6, 0.0);
			for (const double S : { -1.0, 1.0 })
			{
				Face(W, { { X0, CY + S * YE, ZE }, { X1, CY + S * YE, ZE }, { X1, CY, Ridge }, { X0, CY, Ridge } }, { 0.0, S, 1.0 }, Roof);
				Face(W, { { X0, CY + S * YE, ZE - 0.02 }, { X1, CY + S * YE, ZE - 0.02 }, { X1, CY, Ridge - 0.02 }, { X0, CY, Ridge - 0.02 } }, { 0.0, 0.0, -1.0 }, Under);
				Face(W, { { CX + S * HL, CY - HD, Eave }, { CX + S * HL, CY + HD, Eave }, { CX + S * HL, CY, Ridge } }, { S, 0.0, 0.0 }, Wall);
			}
		}

		/** A gambrel roof along x: steep from the eaves (Eave) to the knee (Knee, KneeIn in from the walls), then shallow to the
		 * ridge; its gables' walls closing it. */
		void Gambrel(Writer& W, double Long, double Deep, double Eave, double KneeIn, double Knee, double Ridge, double Over, double End, const Tone& Roof, const Tone& Wall)
		{
			const double HL = 0.5 * Long, HD = 0.5 * Deep, KY = HD - KneeIn, Steep = (Knee - Eave) / KneeIn;
			const double X0 = -HL - End, X1 = HL + End, YE = HD + Over, ZE = Eave - Over * Steep;
			for (const double S : { -1.0, 1.0 })
			{
				Face(W, { { X0, S * YE, ZE }, { X1, S * YE, ZE }, { X1, S * KY, Knee }, { X0, S * KY, Knee } }, { 0.0, S, 0.4 }, Roof);
				Face(W, { { X0, S * YE, ZE - 0.02 }, { X1, S * YE, ZE - 0.02 }, { X1, S * KY, Knee - 0.02 }, { X0, S * KY, Knee - 0.02 } }, { 0.0, 0.0, -1.0 }, Varied(Roof, 0.6, 0.0));
				Face(W, { { X0, S * KY, Knee }, { X1, S * KY, Knee }, { X1, 0.0, Ridge }, { X0, 0.0, Ridge } }, { 0.0, S, 1.0 }, Varied(Roof, 1.06, 0.0));
				Face(W, { { S * HL, -HD, Eave }, { S * HL, HD, Eave }, { S * HL, KY, Knee }, { S * HL, 0.0, Ridge }, { S * HL, -KY, Knee } }, { S, 0.0, 0.0 }, Wall);
			}
		}

		/** A round wall (a silo, a tank): Sides faces from Z0 to Z1, radius R, its normals out. */
		void Drum(Writer& W, double CX, double CY, double R0, double Z0, double R1, double Z1, int Sides, const Tone& C)
		{
			std::vector<uint32_t> A, B;
			const double Rise = Z1 - Z0, Run = R0 - R1, L = std::max(1e-9, std::hypot(Rise, Run));
			for (int K = 0; K <= Sides; ++K)
			{
				const double An = 2.0 * Pi * K / Sides, Ca = std::cos(An), Sa = std::sin(An);
				const double NX = Ca * Rise / L, NY = Sa * Rise / L, NZ = Run / L;
				A.push_back(W.Vert(CX + R0 * Ca, CY + R0 * Sa, Z0, NX, NY, NZ, C));
				B.push_back(W.Vert(CX + R1 * Ca, CY + R1 * Sa, Z1, NX, NY, NZ, C));
			}
			for (int K = 0; K < Sides; ++K)
			{
				W.Tri(A[static_cast<size_t>(K)], A[static_cast<size_t>(K) + 1], B[static_cast<size_t>(K) + 1]);
				W.Tri(A[static_cast<size_t>(K)], B[static_cast<size_t>(K) + 1], B[static_cast<size_t>(K)]);
			}
		}

		/** A beam of T square from A to B (a windmill's leg, a pole, a rail), its four long faces. */
		void Beam(Writer& W, V3 A, V3 B, double T, const Tone& C)
		{
			const V3 D = Norm(Sub(B, A));
			const V3 Up = std::fabs(D.Z) < 0.9 ? V3{ 0.0, 0.0, 1.0 } : V3{ 1.0, 0.0, 0.0 };
			const V3 S1 = Norm(Cross(D, Up)), S2 = Cross(S1, D);
			const double H = 0.5 * T;
			const V3 O[4] = { Add(Times(S1, H), Times(S2, H)), Add(Times(S1, -H), Times(S2, H)), Add(Times(S1, -H), Times(S2, -H)), Add(Times(S1, H), Times(S2, -H)) };
			for (int K = 0; K < 4; ++K)
			{
				const V3 P = O[K], Q = O[(K + 1) % 4];
				Face(W, { Add(A, P), Add(A, Q), Add(B, Q), Add(B, P) }, Add(P, Q), C);
			}
		}

		/** A tuft of Blades blades from a foot Spread across at (CX, CY), High to High + Extra tall, leaning out Lean. */
		void Tuft(Writer& W, double CX, double CY, int Blades, double Spread, double High, double Extra, double Lean, double Width, uint64_t S, const Tone& Foot, const Tone& Tip)
		{
			for (int B = 0; B < Blades; ++B)
			{
				const double A = 2.0 * Pi * (B + 0.6 * Unit(Hash(S, 1, B))) / Blades;
				const double Out = Lean * (0.5 + Unit(Hash(S, 2, B))), Tall = High + Extra * Unit(Hash(S, 3, B));
				const double FX = CX + Spread * std::cos(A) * Unit(Hash(S, 4, B)), FY = CY + Spread * std::sin(A) * Unit(Hash(S, 4, B));
				W.Blade(FX, FY, 0.0, FX + Out * std::cos(A), FY + Out * std::sin(A), Tall, Width, Foot, Tip);
			}
		}

		/** A farmstead as laid: its house, its yard's middle and reach (for the yard's ground), the way it faces. */
		struct Stead
		{
			double X = 0.0, Y = 0.0, R = 40.0;
		};

		class America final : public Region
		{
		public:
			America(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				Month = F.Month >= 1 && F.Month <= 12 ? F.Month : 7;
				// (the plains' wheat west of the 97th meridian and on the prairies of Canada; the east's and the Great Lakes'
				// dairy country; the upper South; the corn belt between)
				bPlains = F.Lon < -96.5;
				bEast = !bPlains && (F.Lon > -84.0 || (F.Lat > 44.5 && F.Lon > -93.5));
				bSouth = !bPlains && !bEast && F.Lat < 36.5;
				bSouth = bSouth || (F.Lat < 35.5 && F.Lon > -92.0);
				bBelt = !bPlains && !bEast && !bSouth;
				SnowLying = SnowOf(F, Look::America);
				bBare = Month >= 11 || Month <= 3;
				// the soil: the corn belt's black prairie loam, the plains' lighter brown, the east's brown, the South's red clay
				SoilC = bBelt ? Rgb(92, 80, 64) : bPlains ? Rgb(126, 108, 84) : bSouth ? Rgb(146, 98, 72) : Rgb(116, 98, 76);
				// the grass by the season: the winter's dormant tan, the spring's and the early summer's green, the plains' dry
				// buff from July
				switch (Month)
				{
				case 12: case 1: case 2: GrassT = Rgb(124, 116, 82); break;
				case 3: GrassT = Rgb(110, 112, 72); break;
				case 4: case 5: case 6: GrassT = Rgb(86, 112, 54); break;
				case 7: case 8: GrassT = bPlains ? Rgb(136, 128, 88) : Rgb(100, 114, 58); break;
				case 9: case 10: GrassT = bPlains ? Rgb(144, 132, 92) : Rgb(108, 112, 66); break;
				default: GrassT = Rgb(122, 116, 80); break;
				}
				double Sum = 0.0;
				for (const double H : F.Height) Sum += H;
				MeanZ = F.Height.empty() ? 0.0 : Sum / static_cast<double>(F.Height.size());
			}
			Look Which() const override { return Look::America; }
			bool Surveyed() const override { return true; }

			// ---------------------------------------------------------------- the year

			/** A field's own month, a fortnight either way (each farmer plants and cuts in his own week). */
			double MonthOf(uint64_t Key) const { return Month + 0.5 + 0.7 * (Unit(Hash(Key, 0x0A7E)) - 0.5); }
			/** The wheat's harvest: the plains' late June, the north's July. */
			double Harvest() const { return bPlains ? 6.75 : bSouth ? 6.5 : 7.3; }
			/** A corn field shocked in the fall (the rest picked standing). */
			static bool Shocked(uint64_t Key) { return Unit(Hash(Key, 0x5C0C)) < 0.5; }
			/** A corn or small-grain field ploughed in the fall (the rest left in its stalks or stubble till spring). */
			static bool FallPloughed(uint64_t Key) { return Unit(Hash(Key, 0xF411)) < 0.55; }
			/** A corn field ploughed in the fall: fewer (most picked fields left in their stalks for the cattle). */
			static bool CornPloughed(uint64_t Key) { return Unit(Hash(Key, 0xF411)) < 0.3; }

			/** What stands in a field this month: its floor's colour, its clutter, how thick, how tall (of the clutter's built
			 * height), and its tint's ripeness (0 green .. 1 ripe or dry). */
			struct Phase
			{
				Tone Floor;
				int Clut = -1;
				double Density = 0.0, High = 1.0, Ripe = 0.0;
			};
			Phase PhaseOf(int C, uint64_t Key) const
			{
				const double M = MonthOf(Key);
				const Tone Soil = Varied(SoilC, 0.96 + 0.08 * Unit(Hash(Key, 0x501)), 0.0);
				const Tone Harrowed = Varied(SoilC, 1.1, 0.01);
				Phase P;
				switch (C)
				{
				case Corn:
					if (M < 4.8 || M >= 12.6)
					{
						// (a picked field's stalks stand broken over the winter, the cattle turned into them)
						if (CornPloughed(Key)) P = { Soil, CStubble, 0.04, 0.6, 1.0 };
						else P = { Mixed(Soil, CornStalk, 0.4), CDry, 0.3, 0.5, 1.0 };
					}
					else if (M < 5.7) P = { Harrowed, CGrass, 0.35, 0.25, 0.0 };
					else if (M < 6.6) P = { Mixed(Soil, CornGreen, 0.35), CCorn, 0.5, 0.32 + 0.2 * (M - 5.7), 0.0 };
					else if (M < 7.3) P = { Mixed(Soil, CornGreen, 0.65), CCorn, 0.55, 0.55 + 0.6 * (M - 6.6), 0.0 };
					else if (M < 9.0) P = { Mixed(CornGreen, Varied(Soil, 0.8, 0.0), 0.2), CCorn, 0.55, 1.0, 0.0 };
					else if (M < 9.8) P = { Mixed(CornGreen, CornDry, 0.45), CCorn, 0.55, 1.0, 0.45 };
					else if (M < 11.4 && !Shocked(Key)) P = { Mixed(CornDry, Soil, 0.2), CDry, 0.5, 0.95, 1.0 };
					else if (!Shocked(Key) && !CornPloughed(Key)) P = { Mixed(Soil, CornStalk, 0.4), CDry, 0.3, 0.6, 1.0 }; // (picked: its stalks broken)
					else P = { Mixed(Soil, CornStalk, 0.5), CStubble, 0.5, 1.0, 1.0 };
					break;
				case Wheat:
				{
					const double H = Harvest();
					if (M < 2.9) P = { Mixed(Soil, Rgb(104, 108, 70), 0.4), CGrain, 0.6, 0.12, 0.6 };
					else if (M < 4.2) P = { Mixed(Soil, WheatGreen, 0.55), CGrain, 0.9, 0.25, 0.0 };
					else if (M < 5.6) P = { WheatGreen, CGrain, 1.1, 0.6, 0.0 };
					else if (M < H - 0.4) P = { Mixed(WheatGreen, WheatGold, 0.5), CGrain, 1.1, 0.95, 0.5 };
					else if (M < H + 0.3) P = { WheatGold, CGrain, 1.1, 1.0, 1.0 };
					else if (M < 9.3) P = { Mixed(StubbleC, Soil, 0.15 + 0.15 * (M - H)), CStubble, 0.9, 0.7, 1.0 };
					else if (M < 10.0) P = { Harrowed, CStubble, 0.04, 0.5, 1.0 };
					else P = { Mixed(Soil, WheatGreen, 0.3), CGrain, 0.8, 0.12, 0.0 };
					break;
				}
				case Oats:
					if (M < 3.8 || M >= 9.6)
					{
						if (FallPloughed(Key) || M < 3.8) P = { Soil, CStubble, 0.04, 0.5, 1.0 };
						else P = { Mixed(StubbleC, Soil, 0.45), CStubble, 0.7, 0.6, 1.0 };
					}
					else if (M < 4.6) P = { Harrowed, CGrass, 0.1, 0.2, 0.0 };
					else if (M < 5.6) P = { Mixed(Soil, OatGreen, 0.55), CGrain, 1.0, 0.3, 0.0 };
					else if (M < 6.8) P = { OatGreen, CGrain, 1.1, 0.8, 0.0 };
					else if (M < 7.6) P = { OatGold, CGrain, 1.1, 0.95, 0.8 };
					else P = { Mixed(Mixed(StubbleC, Soil, 0.15), HayGreen, 0.15 + 0.3 * (M - 7.6) / 2.0), CStubble, 0.9, 0.65, 1.0 }; // (the clover undersown in it greening)
					break;
				case Hay:
				{
					// (each field its own: the alfalfa's blue-green, the clover's and the timothy's yellower)
					const Tone G = Mixed(HayGreen, Rgb(78, 112, 72), Unit(Hash(Key, 0xA1F)));
					// (the sward's clumps grown tall: knee-high at the first cutting, the aftermath shorter)
					if (M < 3.5 || M >= 11.0) P = { Mixed(HayDormant, Soil, 0.2), CGrass, 1.0, 0.8, 1.0 };
					else if (M < 5.8) P = { G, CGrass, 1.2, 1.2 + 1.4 * (M - 3.5) / 2.3, 0.0 };
					else if (M < 6.5) P = { Mixed(HayCut, G, 0.3), CGrass, 1.0, 0.6, 0.6 };
					else if (M < 8.0) P = { G, CGrass, 1.15, 1.8, 0.0 };
					else if (M < 8.6) P = { Mixed(HayCut, G, 0.35), CGrass, 1.0, 0.6, 0.6 };
					else P = { Mixed(G, HayDormant, Smooth(9.0, 11.0, M) * 0.6), CGrass, 1.1, 1.4, Smooth(9.5, 11.0, M) };
					break;
				}
				case Fallow: P = { Varied(Soil, 1.04, 0.0), CWeed, 0.03, 0.6, 1.0 }; break;
				default: P = { GrassT, CGrass, 1.3, 1.0, 0.0 }; break;
				}
				return P;
			}

			// ---------------------------------------------------------------- the land

			double Rolling(double X, double Y, double InMeanZ) const override
			{
				// (the plains as flat as a table; the corn belt's long low swells; the east's hills)
				const double Amp = bPlains ? 5.0 : bEast ? 28.0 : bSouth ? 14.0 : 11.0;
				return InMeanZ + Amp * Fbm(X, Y, bEast ? 1400.0 : 1900.0, 3, Seed ^ 0xA3E1);
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.0; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 4.4 : 3.0) + 0.6 * U; }
			double PlotMin() const override { return bPlains ? 150000.0 : bEast ? 60000.0 : 90000.0; }
			double PlotMax() const override { return bPlains ? 400000.0 : bEast ? 200000.0 : 330000.0; }
			double FarTilled() const override { return bEast ? 0.5 : 0.72; }
			double WoodsEdge() const override { return bPlains ? 0.5 : bEast ? 0.24 : 0.36; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Pasture;
				if (Kind == KOpen)
				{
					// (the rules' open land here is the farm's as much as its grazing: most of it in the rotation)
					const double Grazing = bPlains ? 0.36 : bEast ? 0.34 : 0.26;
					if (U < Grazing) return U < 0.6 * Grazing ? Pasture : Hay;
					U = (U - Grazing) / (1.0 - Grazing);
				}
				struct Share
				{
					int C;
					double Part;
				};
				static const Share BeltShares[] = { { Corn, 0.42 }, { Oats, 0.2 }, { Hay, 0.18 }, { Wheat, 0.1 }, { Pasture, 0.05 }, { Fallow, 0.05 } };
				static const Share PlainShares[] = { { Wheat, 0.56 }, { Fallow, 0.16 }, { Corn, 0.1 }, { Pasture, 0.08 }, { Hay, 0.05 }, { Oats, 0.05 } };
				static const Share EastShares[] = { { Hay, 0.3 }, { Corn, 0.25 }, { Oats, 0.15 }, { Pasture, 0.15 }, { Wheat, 0.1 }, { Fallow, 0.05 } };
				static const Share SouthShares[] = { { Corn, 0.4 }, { Pasture, 0.2 }, { Hay, 0.1 }, { Oats, 0.1 }, { Wheat, 0.1 }, { Fallow, 0.1 } };
				const Share* Shares = bPlains ? PlainShares : bEast ? EastShares : bSouth ? SouthShares : BeltShares;
				double Run = 0.0;
				for (int K = 0; K < 6; ++K)
				{
					Run += Shares[K].Part;
					if (U < Run) return Shares[K].C;
				}
				return Shares[0].C;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Corn: return "corn";
				case Wheat: return "wheat";
				case Oats: return "oats";
				case Hay: return "hay";
				case Pasture: return "pasture";
				case Fallow: return "fallow";
				default: return "";
				}
			}
			bool IsCorn(int C) const override
			{
				// (the standing corn from July to its picking hides a man; the wheat and the oats ripe)
				if (C == Corn) return Month >= 7 && Month <= 10;
				if (C == Wheat) return Month == 6 || (Month == 7 && !bPlains);
				if (C == Oats) return Month == 7;
				return false;
			}

			// ---------------------------------------------------------------- the paint

			Tone GrassAt(const Spot& S) const
			{
				Tone T = Mixed(GrassT, Rgb(138, 128, 92), 0.12 * Smooth(0.0, 0.7, S.N1));
				return Varied(T, 0.96 + 0.05 * S.N2 + 0.03 * S.Broad, 0.0);
			}
			/** The town's streets (gravel), on the town's grid square to its main street; true and the metres from a street's
			 * middle where the place is on one. */
			double StreetGap(double X, double Y) const
			{
				if (!bTownGrid) return 99.0;
				const double DX = X - TownMX, DY = Y - TownMY;
				const double U = DX * TownCa + DY * TownSa, V = -DX * TownSa + DY * TownCa;
				if (std::hypot(U, V) > TownReach) return 99.0;
				const double FV = V / StreetStep - std::floor(V / StreetStep + 0.5);
				const double FU = U / CrossStep - std::floor(U / CrossStep + 0.5);
				const double DV = std::fabs(FV) * StreetStep, DU = std::fabs(FU) * CrossStep;
				// (not the main street itself: that is the lane)
				const double OnV = std::fabs(V) < 0.5 * StreetStep ? 99.0 : DV;
				return std::min(OnV, DU);
			}
			/** The farmstead's yard at a place: 0 outside, 1 at its middle. */
			double YardAt(double X, double Y) const
			{
				double Best = 0.0;
				for (const Stead& F : Steads)
				{
					const double D = std::hypot(X - F.X, Y - F.Y);
					if (D < F.R) Best = std::max(Best, Smooth(F.R, 0.55 * F.R, D));
				}
				return Best;
			}
			Tone FieldColour(const Spot& S) const
			{
				const Phase P = PhaseOf(S.Crop < 0 ? Pasture : S.Crop, S.Key);
				Tone T = P.Floor;
				if (S.Crop == Pasture || S.Crop < 0)
				{
					// (grazed: its cattle paths worn to the earth, its weedy patches)
					T = Mixed(GrassAt(S), Varied(SoilC, 1.05, 0.0), 0.12 * Smooth(0.45, 0.9, S.N2));
					T = Mixed(T, Rgb(118, 116, 74), 0.15 * Smooth(0.2, 0.8, S.N1));
				}
				// (each field its own shade, a soft breakup over it so it is not flat paint where the clutter has gone)
				T = Varied(T, (0.93 + 0.14 * Unit(Hash(S.Key, 0x11))) * (0.97 + 0.04 * S.N1 + 0.02 * S.Broad), 0.0);
				// (the headland the plough turned on, a few metres in; the fence row's grass along the edge)
				if (S.Crop != Pasture && S.Crop >= 0) T = Mixed(T, Varied(SoilC, 1.04, 0.0), 0.25 * Smooth(5.0, 2.0, S.Edge) * Smooth(0.6, 2.0, S.Edge));
				return Mixed(T, GrassAt(S), Smooth(2.0, 0.6, S.Edge));
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a gravel road: its crown pale, the wheels' two tracks darker, the loose gravel at its edges, the ditch's
					// grass beyond
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Track = std::exp(-std::pow((Off - 1.0) / 0.35, 2.0));
					Tone T = Varied(Gravel, 1.0 + 0.05 * S.N2, 0.0);
					T = Mixed(T, Varied(Gravel, 0.84, 0.01), 0.45 * Track);
					return Mixed(T, GrassAt(S), Smooth(S.LaneHalf - 0.5, S.LaneHalf + 0.8, Off));
				}
				case KVillage:
				{
					const double St = StreetGap(S.X, S.Y);
					// (the side streets' gravel; the lawns between, mown and green; the yards worn in patches)
					const Tone Lawn = Mixed(GrassAt(S), Rgb(84, 114, 56), bBare ? 0.0 : 0.35);
					const Tone T = Mixed(Lawn, YardDirt, 0.12 * Smooth(0.3, 0.8, S.N1));
					return Mixed(T, Varied(Gravel, 0.98 + 0.04 * S.N2, 0.0), Smooth(5.0, 3.6, St));
				}
				case KYard: return Mixed(GrassAt(S), Varied(YardDirt, 1.0 + 0.06 * S.N2, 0.0), 0.35 + 0.3 * Smooth(-0.3, 0.5, S.N1));
				case KWoods: return Mixed(GrassAt(S), Litter, (bBare || Month == 10 ? 0.45 : 0.22) + 0.2 * S.N1); // (the summer's woods floored green)
				case KWater: return Rgb(82, 96, 98);
				case KMarsh: return Mixed(GrassAt(S), Rgb(98, 104, 70), 0.4 + 0.2 * S.N1);
				case KWorks: return Mixed(GrassAt(S), Varied(SoilC, 0.9, 0.0), 0.6);
				default: break;
				}
				Tone T = FieldColour(S);
				// (a farmstead's yard: the lawn round the house, the barnyard's beaten earth)
				const double Yd = YardAt(S.X, S.Y);
				if (Yd > 0.0) T = Mixed(T, Mixed(GrassAt(S), Varied(YardDirt, 1.0 + 0.05 * S.N2, 0.0), 0.35 + 0.35 * Smooth(-0.2, 0.6, S.N1)), Yd);
				return T;
			}

			Tone Far(const Spot& S, bool bWoods, double /*Gap*/) const override
			{
				if (bWoods)
				{
					// (the woodlots' crowns by the season: the spring's fresh green, the summer's deep, the fall's russet and
					// gold, the winter's bare grey-brown)
					Tone T = Month >= 11 || Month <= 3 ? Rgb(112, 102, 94) : Month == 4 ? Rgb(104, 118, 72) : Month == 10 ? Rgb(126, 104, 62) : Rgb(66, 88, 46);
					return Varied(T, 0.95 + 0.1 * S.Broad, 0.0);
				}
				if (S.Kind == KField || S.Kind == KOpen || S.Kind == KRough) return FieldColour(S);
				return Colour(S);
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				const bool bSnow = SnowLying > 0.25;
				if (S.Kind == KLane) return S.Lane > 0.6 ? Grown{ ClutOf(CGrass), bSnow ? 0.3 : 1.0 } : Grown{};
				if (S.Kind == KVillage) return StreetGap(S.X, S.Y) < 4.0 ? Grown{} : Pick < 0.9 ? Grown{ ClutOf(CGrass), bSnow ? 0.2 : 1.1 } : Grown{ ClutOf(CWeed), 0.1 };
				if (S.Kind == KYard) return Pick < 0.7 ? Grown{ ClutOf(CGrass), 0.5 * Smooth(-0.3, 0.5, S.N1) + 0.1 } : Grown{ ClutOf(CWeed), 0.1 };
				if (S.Kind == KWorks) return Grown{ ClutOf(CGrass), 0.2 };
				if (S.Kind == KMarsh) return Pick < 0.6 ? Grown{ ClutOf(CWeed), 0.9 } : Grown{ ClutOf(CGrass), 1.2 };
				if (S.Kind == KWoods) return Pick < 0.7 ? Grown{ ClutOf(CGrass), 0.5 } : Grown{ ClutOf(CWeed), 0.25 };
				// the fence row: grass and weeds along the field's edge
				if (S.Edge < 1.4) return Pick < 0.5 ? Grown{ ClutOf(CWeed), bSnow ? 0.5 : 0.9 } : Grown{ ClutOf(CGrass), bSnow ? 0.4 : 1.2 };
				// a farmstead's yard: the lawn and the barnyard's grass
				if (YardAt(S.X, S.Y) > 0.3) return Pick < 0.8 ? Grown{ ClutOf(CGrass), bSnow ? 0.2 : 0.8 } : Grown{ ClutOf(CWeed), 0.1 };
				if (bSnow)
				{
					// under the snow: the corn's stalks and the stubble through it, dead grass here and there
					if (S.Crop == Corn) return CornPloughed(S.Key) ? Grown{ ClutOf(CStubble), 0.06 } : Grown{ ClutOf(CDry), 0.3 };
					if (S.Crop == Pasture || S.Crop == Hay || S.Crop < 0) return Pick < 0.4 ? Grown{ ClutOf(CGrass), 0.25 } : Grown{};
					return Grown{ ClutOf(CStubble), 0.15 };
				}
				const Phase P = PhaseOf(S.Crop < 0 ? Pasture : S.Crop, S.Key);
				if ((S.Crop == Pasture || S.Crop < 0) && Pick > 0.94) return Grown{ ClutOf(CWeed), 0.25 }; // (thistles and ironweed in the pasture)
				if (P.Clut < 0) return Grown{};
				return Grown{ ClutOf(P.Clut), P.Density };
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				const bool bField = S.Kind == KField || S.Kind == KOpen || S.Kind == KRough;
				const Phase Ph = bField && S.Crop >= 0 ? PhaseOf(S.Crop, S.Key) : Phase{};
				switch (K)
				{
				case CCorn:
				{
					// (the rows along the field's length, every plant a yard from the next; a thing stays where it is laid)
					P.Yaw = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = 1.0f;
					P.ScaleZ = static_cast<float>(Ph.High * (0.92 + 0.16 * U5));
					const double L = 0.94 + 0.12 * U6;
					// (the green leaves; drying from the foot up in September; the October's buff; the tint on the linear colour)
					P.Tint = Tint(Lerp(0.95, 1.85, Ph.Ripe) * L, Lerp(1.0, 1.32, Ph.Ripe) * L, Lerp(0.95, 1.3, Ph.Ripe) * L);
					return true;
				}
				case CGrain:
				{
					P.Yaw = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = static_cast<float>(0.85 + 0.3 * U4);
					P.ScaleZ = static_cast<float>(Ph.High * (0.9 + 0.2 * U5));
					const double L = 0.94 + 0.12 * U6;
					// (green to the wheat's gold, about 186, 160, 92 at its ears)
					P.Tint = Tint(Lerp(0.85, 1.75, Ph.Ripe) * L, Lerp(1.05, 1.3, Ph.Ripe) * L, Lerp(0.85, 0.95, Ph.Ripe) * L);
					return true;
				}
				case CStubble:
					P.Yaw = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = static_cast<float>(0.9 + 0.2 * U4);
					P.ScaleZ = static_cast<float>((S.Crop == Corn ? 1.0 : 0.55) * (0.8 + 0.4 * U5));
					P.Tint = SnowLying > 0.25 ? Tint(0.95 + 0.06 * U6, 0.92, 0.86) : Tint(1.0 + 0.06 * U6, 1.0, 0.95);
					return true;
				case CDry:
				{
					// (the picked or the shocked corn's rows, standing; broken to half over the winter)
					P.Yaw = static_cast<float>(S.Angle + (U7 < 0.5 ? 0.0 : Pi));
					P.Scale = 1.0f;
					P.ScaleZ = static_cast<float>((bField ? Ph.High : 0.5) * (0.88 + 0.24 * U5));
					const double L = 0.9 + 0.2 * U6;
					P.Tint = SnowLying > 0.25 ? Tint(0.92 * L, 0.88 * L, 0.84 * L) : Tint(L, L, 0.96 * L);
					return true;
				}
				case CWeed:
					P.Scale = static_cast<float>(0.8 + 0.4 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Tint = Tint(0.96 + 0.08 * U6, 0.98, 0.94);
					return true;
				case CGrass:
					P.Scale = static_cast<float>(0.6 + 0.7 * U4);
					P.ScaleZ = static_cast<float>((bField && S.Crop != Pasture && S.Crop >= 0 && S.Edge > 1.4 ? Ph.High : 1.0) * (0.6 + 0.7 * U5));
					if (bField && S.Crop == Hay && S.Edge > 1.4)
						P.Tint = Tint(Lerp(0.9, 1.4, Ph.Ripe) * (0.96 + 0.08 * U7), Lerp(1.02, 1.1, Ph.Ripe), Lerp(1.0, 0.86, Ph.Ripe));
					else P.Tint = Tint(0.96 + 0.08 * U7, 1.0 + 0.04 * U6, 0.94);
					return true;
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				const bool bDead = bBare || SnowLying > 0.25;
				switch (K)
				{
				case CGrass:
				{
					// a clump of sward, the season's colour (the winter's dormant buff)
					const Tone Foot = bDead ? Rgb(112, 100, 74) : Rgb(72, 90, 44), Tip = bDead ? Rgb(176, 160, 116) : Rgb(130, 150, 80);
					Tuft(W, 0.0, 0.0, 26, 0.25, 0.1, 0.2, 0.09, 0.017, 0x6A55, Foot, Tip);
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				}
				case CCorn:
				{
					// two rows a yard apart along x, two plants to a row (1.2 m of row): each a stalk 2.1 to 2.45 m with its
					// tassel, seven leaves arching out and down from it in turn, an ear at its middle (the field's tint makes it
					// the summer's green or the fall's buff)
					for (int R = 0; R < 2; ++R)
						for (int Pl = 0; Pl < 2; ++Pl)
						{
							const uint64_t Hs = Hash(0xC0B5, static_cast<uint64_t>(R * 2 + Pl));
							const double X = -0.3 + 0.6 * Pl + 0.12 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.46 + 0.92 * R + 0.06 * (Unit(Hash(Hs, 2)) - 0.5);
							const double Tall = 2.1 + 0.35 * Unit(Hash(Hs, 3));
							W.Blade(X, Y, 0.0, X + 0.01, Y, Tall, 0.05, Rgb(84, 102, 50), Rgb(112, 124, 66));
							W.Blade(X, Y, 0.0, X, Y + 0.01, Tall, 0.05, Rgb(84, 102, 50), Rgb(112, 124, 66));
							const double A0 = 2.0 * Pi * Unit(Hash(Hs, 4));
							for (int L = 0; L < 7; ++L)
							{
								const double Z = 0.3 + (Tall - 0.75) * L / 6.5;
								const double A = A0 + (L % 2 ? Pi : 0.0) + 0.5 * (Unit(Hash(Hs, 10 + L)) - 0.5);
								const double Out = (0.5 + 0.25 * Unit(Hash(Hs, 20 + L))) * (1.0 - 0.35 * L / 7.0);
								const double MX = X + 0.5 * Out * std::cos(A), MY = Y + 0.5 * Out * std::sin(A), MZ = Z + 0.26;
								const double TX = X + Out * std::cos(A), TY = Y + Out * std::sin(A), TZ = Z + 0.1;
								W.Blade(X, Y, Z, MX, MY, MZ, 0.08, Rgb(54, 80, 38), Rgb(74, 100, 48));
								W.Blade(MX, MY, MZ, TX, TY, TZ, 0.06, Rgb(74, 100, 48), Rgb(98, 118, 58));
							}
							for (int T = 0; T < 3; ++T)
							{
								const double A = 2.0 * Pi * T / 3.0 + A0;
								W.Blade(X, Y, Tall - 0.05, X + 0.1 * std::cos(A), Y + 0.1 * std::sin(A), Tall + 0.28, 0.02, Rgb(146, 134, 88), Rgb(182, 162, 108));
							}
							W.Bead(X + 0.05 * std::cos(A0), Y + 0.05 * std::sin(A0), 0.5 * Tall, 0.04, 0.2, Rgb(118, 136, 68), Rgb(98, 116, 58));
						}
					M.Paint = 2;
					M.FarFrom = 32.0f;
					// far off: each row a sheet of leaves the plants' height, the tassels' band over it (both faces)
					M.Far = LandField::Mesh();
					Writer F(M.Far);
					for (int R = 0; R < 2; ++R)
					{
						const double Y = -0.46 + 0.92 * R;
						for (const double S : { -1.0, 1.0 })
						{
							Face(F, { { -0.62, Y, 0.0 }, { 0.62, Y, 0.0 }, { 0.62, Y, 2.2 }, { -0.62, Y, 2.2 } }, { 0.0, S, 0.3 }, Rgb(62, 88, 44));
							Face(F, { { -0.62, Y, 2.2 }, { 0.62, Y, 2.2 }, { 0.5, Y, 2.36 }, { -0.5, Y, 2.36 } }, { 0.0, S, 0.3 }, Rgb(112, 112, 74));
						}
					}
					break;
				}
				case CGrain:
				{
					// a clump of wheat (or oats by the tint): fourteen stalks a metre tall from a small foot, each its ear
					for (int B = 0; B < 14; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x6E41, B)), D = 0.14 * Unit(Hash(0x6E42, B));
						const double X = D * std::cos(A), Y = D * std::sin(A), High = 0.85 + 0.2 * Unit(Hash(0x6E43, B));
						const double LX = 0.06 * std::cos(A), LY = 0.06 * std::sin(A);
						W.Blade(X, Y, 0.0, X + LX, Y + LY, High, 0.02, Rgb(92, 110, 54), Rgb(130, 140, 72));
						W.Bead(X + LX, Y + LY, High + 0.04, 0.012, 0.07, Rgb(150, 146, 80), Rgb(120, 122, 62));
						if (B % 2 == 0) W.Blade(X, Y, 0.0, X + 2.5 * LX - 0.04, Y + 2.5 * LY + 0.04, 0.35, 0.025, Rgb(80, 100, 48), Rgb(116, 132, 66));
					}
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				}
				case CStubble:
				{
					// stubble in its drill rows along x: the grain's short straws; the corn's broken stalks a hand and more
					// high in the same rows (by its height), a leaf or two lying
					for (int R = 0; R < 3; ++R)
						for (int B = 0; B < 6; ++B)
						{
							const uint64_t Hs = Hash(0x57B1, static_cast<uint64_t>(R * 6 + B));
							const double X = -0.5 + 0.2 * B + 0.06 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.35 + 0.35 * R;
							const double High = 0.14 + 0.16 * Unit(Hash(Hs, 2));
							W.Blade(X, Y, 0.0, X + 0.03 * (Unit(Hash(Hs, 3)) - 0.5), Y + 0.03 * (Unit(Hash(Hs, 4)) - 0.5), High, 0.035, Rgb(138, 120, 86), Rgb(184, 166, 120));
						}
					for (int B = 0; B < 3; ++B)
					{
						const double X = Unit(Hash(0x57B2, B)) - 0.5, Y = 0.8 * (Unit(Hash(0x57B3, B)) - 0.5), A = 2.0 * Pi * Unit(Hash(0x57B4, B));
						W.Blade(X, Y, 0.01, X + 0.45 * std::cos(A), Y + 0.45 * std::sin(A), 0.04, 0.06, Rgb(160, 142, 102), Rgb(178, 160, 116));
					}
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				}
				case CWeed:
				{
					// the fence row's weeds: goldenrod's, ironweed's and milkweed's stems from one foot, their heads (the
					// summer's green and gold, the winter's dry brown)
					const Tone StemF = bDead ? Rgb(110, 92, 66) : Rgb(74, 94, 46), StemT = bDead ? Rgb(146, 124, 88) : Rgb(104, 124, 62);
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x3EED, B)), Lean = 0.05 + 0.1 * Unit(Hash(0x3EEE, B)), High = 0.6 + 0.5 * Unit(Hash(0x3EEF, B));
						const double TX = Lean * std::cos(A), TY = Lean * std::sin(A);
						W.Blade(0.0, 0.0, 0.0, TX, TY, High, 0.03, StemF, StemT);
						W.Blade(0.0, 0.0, 0.2 * High, TX + 0.1 * std::cos(A + 1.0), TY + 0.1 * std::sin(A + 1.0), 0.6 * High, 0.05, StemF, StemT);
						const Tone Head = bDead ? Rgb(132, 112, 84) : B % 2 ? Rgb(178, 156, 62) : Rgb(120, 92, 110);
						W.Bead(TX, TY, High + 0.02, 0.05, 0.07, Head, Varied(Head, 0.75, 0.0));
					}
					M.Paint = 2;
					M.FarFrom = 55.0f;
					break;
				}
				case CDry:
				{
					// the corn dry in the fall: the CCorn's two rows of two, buff, its leaves hanging, its ears turned down in
					// their husks, the tassels broken dark (over the winter the same, broken to half its height by the tint)
					for (int R = 0; R < 2; ++R)
						for (int Pl = 0; Pl < 2; ++Pl)
						{
							const uint64_t Hs = Hash(0xD0B5, static_cast<uint64_t>(R * 2 + Pl));
							const double X = -0.3 + 0.6 * Pl + 0.12 * (Unit(Hash(Hs, 1)) - 0.5), Y = -0.46 + 0.92 * R + 0.06 * (Unit(Hash(Hs, 2)) - 0.5);
							const double Tall = 2.0 + 0.35 * Unit(Hash(Hs, 3));
							const double Lean = 0.08 * (Unit(Hash(Hs, 5)) - 0.5);
							W.Blade(X, Y, 0.0, X + Lean, Y + 0.01, Tall, 0.045, Rgb(132, 112, 80), Rgb(170, 150, 108));
							W.Blade(X, Y, 0.0, X + Lean + 0.01, Y, Tall, 0.045, Rgb(132, 112, 80), Rgb(170, 150, 108));
							const double A0 = 2.0 * Pi * Unit(Hash(Hs, 4));
							for (int L = 0; L < 6; ++L)
							{
								const double Z = 0.3 + (Tall - 0.7) * L / 5.5;
								const double A = A0 + (L % 2 ? Pi : 0.0) + 0.6 * (Unit(Hash(Hs, 10 + L)) - 0.5);
								const double Out = (0.35 + 0.2 * Unit(Hash(Hs, 20 + L))) * (1.0 - 0.3 * L / 6.0);
								const double MX = X + 0.6 * Out * std::cos(A), MY = Y + 0.6 * Out * std::sin(A), MZ = Z + 0.1;
								const double TX = X + Out * std::cos(A), TY = Y + Out * std::sin(A), TZ = Z - 0.3 - 0.2 * Unit(Hash(Hs, 30 + L));
								W.Blade(X, Y, Z, MX, MY, MZ, 0.06, Rgb(150, 128, 90), Rgb(172, 152, 108));
								W.Blade(MX, MY, MZ, TX, TY, TZ, 0.045, Rgb(172, 152, 108), Rgb(190, 172, 128));
							}
							for (int T = 0; T < 2; ++T)
							{
								const double A = Pi * T + A0;
								W.Blade(X + Lean, Y, Tall - 0.05, X + Lean + 0.08 * std::cos(A), Y + 0.08 * std::sin(A), Tall + 0.18, 0.02, Rgb(112, 92, 64), Rgb(128, 106, 74));
							}
							// (the ear hangs from its shank, husk buff)
							const double EX = X + 0.12 * std::cos(A0), EY = Y + 0.12 * std::sin(A0), EZ = 0.48 * Tall;
							W.Blade(X, Y, EZ + 0.1, EX, EY, EZ, 0.02, Rgb(150, 128, 90), Rgb(160, 138, 98));
							W.Bead(EX, EY, EZ - 0.1, 0.045, 0.2, Rgb(186, 166, 122), Rgb(160, 140, 100));
						}
					M.Paint = 2;
					M.FarFrom = 32.0f;
					M.Far = LandField::Mesh();
					Writer F(M.Far);
					for (int R = 0; R < 2; ++R)
					{
						const double Y = -0.46 + 0.92 * R;
						for (const double S : { -1.0, 1.0 })
						{
							Face(F, { { -0.62, Y, 0.0 }, { 0.62, Y, 0.0 }, { 0.62, Y, 2.1 }, { -0.62, Y, 2.1 } }, { 0.0, S, 0.3 }, Rgb(156, 136, 98));
							Face(F, { { -0.62, Y, 2.1 }, { 0.62, Y, 2.1 }, { 0.5, Y, 2.25 }, { -0.5, Y, 2.25 } }, { 0.0, S, 0.3 }, Rgb(120, 102, 74));
						}
					}
					break;
				}
				default: break;
				}
			}

			// ---------------------------------------------------------------- the kit (all built here)

			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KHouse: return "FarmHouse";
				case KBarn: return "RedBarn";
				case KSilo: return "StaveSilo";
				case KMill: return "Windmill";
				case KCrib: return "CornCrib";
				case KShed: return "MachineShed";
				case KStore: return "BrickStore";
				case KChurch: return "FrameChurch";
				case KElevator: return "GrainElevator";
				case KPole: return "PhonePole";
				case KPost: return "WirePost";
				case KRail: return "WormFence";
				case KShock: return "CornShock";
				case KStack: return "HayStack";
				case KHouse2: return "GableHouse";
				case KFront: return "FalseFront";
				default: return nullptr;
				}
			}
			bool KitBuilt(int K) const override { return K >= 0 && K < 16; }
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				auto Sight = [&M](float Mid, float Half, float Across) {
					M.bSight = true;
					M.SightMid = Mid;
					M.SightHalf = Half;
					M.SightAcross = Across;
				};
				switch (K)
				{
				case KHouse:
				case KHouse2:
				case KBarn:
				case KCrib:
				case KShed:
				case KStore:
				case KFront:
				case KChurch:
					M.FarFrom = 220.0f;
					Sight(0.5f, 0.5f, 0.85f);
					break;
				case KSilo:
				case KElevator:
					M.FarFrom = 400.0f;
					Sight(0.5f, 0.5f, 0.7f);
					break;
				case KMill: M.FarFrom = 160.0f; break;
				case KPole: M.FarFrom = 160.0f; break;
				case KPost: M.FarFrom = 45.0f; break;
				case KRail: M.FarFrom = 60.0f; break;
				case KShock:
					M.FarFrom = 120.0f;
					Sight(0.5f, 0.5f, 0.85f);
					break;
				case KStack:
					M.FarFrom = 160.0f;
					Sight(0.5f, 0.5f, 0.85f);
					break;
				default: break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				Writer W(M.Near);
				bool bFar = false; // (a far level of its own written below; else the near one drawn at every distance)
				switch (K)
				{
				case KHouse:
				{
					// a farmhouse 10.4 by 6.4 m and two storeys, white clapboard on a fieldstone footing, its gable ends to the
					// sides, a brick chimney in each; five windows up and four and the door down along its front (-y), a porch
					// the house's length under its own low roof
					const double By = 1.4; // (the body's middle: the porch in front of it within the footprint)
					W.Box(0.0, By, 0.3, 5.28, 3.28, 0.3, 0.0, Rgb(132, 126, 116));
					W.Box(0.0, By, 3.2, 5.2, 3.2, 2.6, 0.0, White);
					Gable(W, 0.0, By, 10.4, 6.4, 5.8, 8.4, 0.35, 0.25, Shingle, White);
					for (const double S : { -1.0, 1.0 }) W.Box(S * 4.55, By, 7.9, 0.34, 0.34, 1.4, 0.0, Brick);
					const double Front = By - 3.2, Back = By + 3.2;
					for (int Bay = 0; Bay < 5; ++Bay)
					{
						const double X = -4.0 + 2.0 * Bay;
						Window(W, 0, Front, X, 4.4, 0.42, 0.7);
						if (Bay != 2) Window(W, 0, Front, X, 1.75, 0.42, 0.7);
					}
					Pane(W, 0, Front, 0.0, 1.25, 0.5, 1.15, Rgb(86, 68, 52));
					for (int Bay = 0; Bay < 3; ++Bay)
					{
						Window(W, 1, Back, -3.0 + 3.0 * Bay, 4.4, 0.42, 0.7);
						Window(W, 1, Back, -3.0 + 3.0 * Bay, 1.75, 0.42, 0.7);
					}
					for (const int D : { 2, 3 })
						for (const double Yw : { By - 1.3, By + 1.3 })
						{
							Window(W, D, D == 2 ? -5.2 : 5.2, Yw, 4.4, 0.42, 0.7);
							Window(W, D, D == 2 ? -5.2 : 5.2, Yw, 1.75, 0.42, 0.7);
						}
					// the porch: its floor, its posts, its roof falling away from the wall
					W.Box(0.0, Front - 1.3, 0.3, 4.4, 1.3, 0.3, 0.0, Rgb(150, 144, 132));
					for (int Ps = 0; Ps < 4; ++Ps) W.Box(-4.1 + 2.733 * Ps, Front - 2.45, 1.65, 0.08, 0.08, 1.05, 0.0, Trim);
					Face(W, { { -4.6, Front, 3.35 }, { 4.6, Front, 3.35 }, { 4.6, Front - 2.8, 2.75 }, { -4.6, Front - 2.8, 2.75 } }, { 0.0, -0.2, 1.0 }, Shingle);
					Face(W, { { -4.6, Front, 3.33 }, { 4.6, Front, 3.33 }, { 4.6, Front - 2.8, 2.73 }, { -4.6, Front - 2.8, 2.73 } }, { 0.0, 0.0, -1.0 }, Rgb(180, 178, 170));
					W.Box(0.0, Front - 2.7, 2.65, 4.6, 0.1, 0.12, 0.0, Trim);
					break;
				}
				case KHouse2:
				{
					// a house a storey and a half, its gable to the street (-y), 7.2 wide and 9.6 deep, clapboard painted a
					// pale grey-buff, its porch across the gable end
					const Tone Paint = Rgb(204, 198, 180);
					const double By = 1.0;
					W.Box(0.0, By, 0.3, 3.68, 4.88, 0.3, 0.0, Rgb(132, 126, 116));
					W.Box(0.0, By, 2.6, 3.6, 4.8, 2.0, 0.0, Paint);
					// (the roof along y: built along x and its corners swapped)
					{
						const double HL = 4.8, HD = 3.6, Eave = 4.6, Ridge = 8.2, Over = 0.35, End = 0.3, Slope = (Ridge - Eave) / HD;
						const double Y0 = By - HL - End, Y1 = By + HL + End, XE = HD + Over, ZE = Eave - Over * Slope;
						for (const double S : { -1.0, 1.0 })
						{
							Face(W, { { S * XE, Y0, ZE }, { S * XE, Y1, ZE }, { 0.0, Y1, Ridge }, { 0.0, Y0, Ridge } }, { S, 0.0, 1.0 }, Shingle);
							Face(W, { { S * XE, Y0, ZE - 0.02 }, { S * XE, Y1, ZE - 0.02 }, { 0.0, Y1, Ridge - 0.02 }, { 0.0, Y0, Ridge - 0.02 } }, { 0.0, 0.0, -1.0 }, Varied(Shingle, 0.6, 0.0));
							Face(W, { { -HD, By + S * HL, Eave }, { HD, By + S * HL, Eave }, { 0.0, By + S * HL, Ridge } }, { 0.0, S, 0.0 }, Paint);
						}
					}
					W.Box(1.8, By + 1.5, 8.2, 0.3, 0.3, 0.9, 0.0, Brick);
					const double Front = By - 4.8;
					Window(W, 0, Front, -1.6, 1.75, 0.42, 0.7);
					Window(W, 0, Front, 1.6, 1.75, 0.42, 0.7);
					Window(W, 0, Front, 0.0, 5.6, 0.4, 0.6);
					Pane(W, 0, Front, 0.0, 1.25, 0.5, 1.15, Rgb(86, 68, 52));
					for (const int D : { 2, 3 })
						for (int Bay = 0; Bay < 3; ++Bay) Window(W, D, D == 2 ? -3.6 : 3.6, By - 3.0 + 3.0 * Bay, 1.75, 0.42, 0.7);
					W.Box(0.0, Front - 1.0, 0.3, 3.6, 1.0, 0.3, 0.0, Rgb(150, 144, 132));
					for (const double X : { -3.4, 3.4 }) W.Box(X, Front - 1.85, 1.6, 0.08, 0.08, 1.0, 0.0, Trim);
					Face(W, { { -3.7, Front, 3.0 }, { 3.7, Front, 3.0 }, { 3.7, Front - 2.1, 2.6 }, { -3.7, Front - 2.1, 2.6 } }, { 0.0, -0.2, 1.0 }, Shingle);
					Face(W, { { -3.7, Front, 2.98 }, { 3.7, Front, 2.98 }, { 3.7, Front - 2.1, 2.58 }, { -3.7, Front - 2.1, 2.58 } }, { 0.0, 0.0, -1.0 }, Rgb(180, 178, 170));
					break;
				}
				case KBarn:
				{
					// a barn 20 by 12.8 m: red board walls 5 m on a whitewashed stone footing, a gambrel roof of grey shingle
					// to 10.8 m, its big doors in the gable ends (white-trimmed, braced), the hay door and its hood over them,
					// a row of small windows down each side, a cupola on the ridge
					W.Box(0.0, 0.0, 0.45, 10.05, 6.45, 0.45, 0.0, Rgb(176, 170, 156));
					W.Box(0.0, 0.0, 2.95, 10.0, 6.4, 2.05, 0.0, BarnRed);
					Gambrel(W, 20.0, 12.8, 5.0, 2.2, 8.6, 10.8, 0.3, 0.3, Rgb(98, 96, 92), BarnRed);
					for (const double S : { -1.0, 1.0 })
					{
						const int D = S < 0.0 ? 2 : 3;
						const double Pl = S * 10.0;
						Pane(W, D, Pl, 0.0, 2.2, 2.2, 2.2, Trim);
						Pane(W, D, Pl + S * 0.01, 0.0, 2.2, 2.05, 2.05, Rgb(112, 40, 32));
						Beam(W, { Pl + S * 0.06, -2.0, 0.25 }, { Pl + S * 0.06, 2.0, 4.15 }, 0.12, Trim);
						Beam(W, { Pl + S * 0.06, 2.0, 0.25 }, { Pl + S * 0.06, -2.0, 4.15 }, 0.12, Trim);
						Pane(W, D, Pl, 0.0, 7.0, 1.1, 0.95, Trim);
						Pane(W, D, Pl + S * 0.01, 0.0, 7.0, 0.95, 0.8, Rgb(66, 40, 34));
						// the hay hood: a little gable out over the hay door from the peak
						Face(W, { { Pl, -1.2, 10.0 }, { Pl + S * 1.6, -1.2, 9.6 }, { Pl + S * 1.6, 0.0, 10.4 }, { Pl, 0.0, 10.8 } }, { 0.0, -0.5, 1.0 }, Rgb(98, 96, 92));
						Face(W, { { Pl, 1.2, 10.0 }, { Pl + S * 1.6, 1.2, 9.6 }, { Pl + S * 1.6, 0.0, 10.4 }, { Pl, 0.0, 10.8 } }, { 0.0, 0.5, 1.0 }, Rgb(98, 96, 92));
						Face(W, { { Pl + S * 1.6, -1.2, 9.6 }, { Pl + S * 1.6, 1.2, 9.6 }, { Pl + S * 1.6, 0.0, 10.4 } }, { S, 0.0, 0.0 }, BarnRed);
					}
					for (int Wn = 0; Wn < 5; ++Wn)
					{
						const double X = -7.6 + 3.8 * Wn;
						Window(W, 0, -6.4, X, 2.4, 0.4, 0.35);
						Window(W, 1, 6.4, X, 2.4, 0.4, 0.35);
					}
					W.Box(0.0, 0.0, 11.3, 0.7, 0.7, 0.55, 0.0, Trim);
					Drum(W, 0.0, 0.0, 1.0, 11.85, 0.05, 12.6, 4, Rgb(98, 96, 92));
					// far: the barn's walls, its roof and its doors
					bFar = true;
					Writer F(M.Far);
					F.Box(0.0, 0.0, 2.5, 10.0, 6.4, 2.5, 0.0, BarnRed);
					Gambrel(F, 20.0, 12.8, 5.0, 2.2, 8.6, 10.8, 0.3, 0.3, Rgb(98, 96, 92), BarnRed);
					for (const double S : { -1.0, 1.0 }) Pane(F, S < 0.0 ? 2 : 3, S * 10.0, 0.0, 2.2, 2.2, 2.2, Trim);
					break;
				}
				case KSilo:
				{
					// a concrete stave silo 4.2 m across and 12.5 m high, its steel hoops, a domed metal cap, the chute down its
					// side
					const Tone Stave = Rgb(186, 180, 166);
					Drum(W, 0.0, 0.0, 2.1, 0.0, 2.1, 12.5, 16, Stave);
					for (int H = 0; H < 6; ++H) Drum(W, 0.0, 0.0, 2.13, 1.2 + 2.0 * H, 2.13, 1.3 + 2.0 * H, 16, Rgb(110, 106, 98));
					Drum(W, 0.0, 0.0, 2.2, 12.5, 1.7, 13.6, 16, Rgb(132, 132, 128));
					Drum(W, 0.0, 0.0, 1.7, 13.6, 0.05, 14.3, 16, Rgb(132, 132, 128));
					W.Box(0.0, -2.25, 6.8, 0.3, 0.2, 5.9, 0.0, Rgb(160, 156, 148));
					bFar = true;
					Writer F(M.Far);
					Drum(F, 0.0, 0.0, 2.1, 0.0, 2.1, 12.5, 8, Stave);
					Drum(F, 0.0, 0.0, 2.2, 12.5, 0.05, 14.3, 8, Rgb(132, 132, 128));
					break;
				}
				case KMill:
				{
					// a windmill: a galvanised steel tower of four legs to 9.5 m, braced, a platform, the wheel of eighteen
					// sails 2.6 m across facing the wind (-y), its tail vane, the pump rod down the middle
					const Tone Steel = Rgb(150, 150, 146);
					for (const double SX : { -1.0, 1.0 })
						for (const double SY : { -1.0, 1.0 }) Beam(W, { SX * 1.3, SY * 1.3, 0.0 }, { SX * 0.25, SY * 0.25, 9.5 }, 0.07, Steel);
					for (const double Z : { 3.0, 6.2 })
					{
						const double Hf = 1.3 - (1.3 - 0.25) * Z / 9.5;
						Beam(W, { -Hf, -Hf, Z }, { Hf, -Hf, Z }, 0.04, Steel);
						Beam(W, { -Hf, Hf, Z }, { Hf, Hf, Z }, 0.04, Steel);
						Beam(W, { -Hf, -Hf, Z }, { -Hf, Hf, Z }, 0.04, Steel);
						Beam(W, { Hf, -Hf, Z }, { Hf, Hf, Z }, 0.04, Steel);
					}
					W.Box(0.0, 0.0, 9.55, 0.45, 0.45, 0.05, 0.0, Steel);
					Beam(W, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 9.5 }, 0.03, Rgb(120, 116, 108));
					W.Box(0.0, -0.3, 10.2, 0.18, 0.4, 0.18, 0.0, Steel);
					for (int Sl = 0; Sl < 18; ++Sl)
					{
						const double A = 2.0 * Pi * Sl / 18.0, Ca = std::cos(A), Sa = std::sin(A);
						const double R0 = 0.35, R1 = 1.3, Wd = 0.13;
						const V3 P0{ R0 * Ca - Wd * 0.5 * Sa, -0.75, 10.2 + R0 * Sa + Wd * 0.5 * Ca }, P1{ R1 * Ca - Wd * Sa, -0.7, 10.2 + R1 * Sa + Wd * Ca };
						const V3 P2{ R1 * Ca + Wd * Sa, -0.8, 10.2 + R1 * Sa - Wd * Ca }, P3{ R0 * Ca + Wd * 0.5 * Sa, -0.75, 10.2 + R0 * Sa - Wd * 0.5 * Ca };
						Face(W, { P0, P1, P2, P3 }, { 0.0, -1.0, 0.0 }, Steel);
						Face(W, { P0, P1, P2, P3 }, { 0.0, 1.0, 0.0 }, Varied(Steel, 0.8, 0.0));
					}
					Beam(W, { 0.0, -0.1, 10.2 }, { 0.0, 2.2, 10.25 }, 0.05, Steel);
					Face(W, { { 0.0, 1.6, 9.85 }, { 0.0, 2.6, 9.95 }, { 0.0, 2.6, 10.75 }, { 0.0, 1.6, 10.55 } }, { 1.0, 0.0, 0.0 }, Rgb(150, 56, 44));
					Face(W, { { 0.0, 1.6, 9.85 }, { 0.0, 2.6, 9.95 }, { 0.0, 2.6, 10.75 }, { 0.0, 1.6, 10.55 } }, { -1.0, 0.0, 0.0 }, Rgb(150, 56, 44));
					break;
				}
				case KCrib:
				{
					// a corn crib 8 by 3 m, its slatted walls 3.4 m (the gold of the ears through the slats), a gable roof of
					// tin
					W.Box(0.0, 0.0, 1.85, 4.0, 1.5, 1.55, 0.0, Rgb(178, 150, 78));
					for (int S = 0; S < 7; ++S) W.Box(0.0, 0.0, 0.45 + 0.46 * S, 4.03, 1.53, 0.12, 0.0, Rgb(132, 120, 102));
					for (const double X : { -4.0, 0.0, 4.0 }) W.Box(X, 0.0, 1.7, 0.1, 1.55, 1.7, 0.0, Rgb(120, 110, 94));
					Gable(W, 0.0, 0.0, 8.0, 3.0, 3.4, 4.8, 0.4, 0.3, Tin, Rgb(132, 120, 102));
					break;
				}
				case KShed:
				{
					// a machine shed 14 by 6.4 m, its board walls weathered grey, a shallow gable of tin, three bays open along
					// its front (-y), dark within
					const Tone Board = Rgb(150, 142, 128);
					W.Box(0.0, 0.0, 1.9, 7.0, 3.2, 1.9, 0.0, Board);
					Gable(W, 0.0, 0.0, 14.0, 6.4, 3.8, 5.3, 0.4, 0.3, Tin, Board);
					for (int B = 0; B < 3; ++B) Pane(W, 0, -3.2, -4.6 + 4.6 * B, 1.55, 1.9, 1.55, Rgb(48, 44, 40));
					break;
				}
				case KStore:
				{
					// a brick store on main street, 7.6 m wide and 16 deep, two storeys and a parapet to 8.6 m, a pale stone
					// cornice; along its front (-y) the shop's two windows and its door, the awning over them, three windows
					// up
					W.Box(0.0, 0.0, 3.8, 3.8, 8.0, 3.8, 0.0, Brick);
					W.Box(0.0, -7.6, 8.1, 3.8, 0.4, 0.5, 0.0, Brick);
					W.Box(0.0, -7.95, 8.0, 3.9, 0.12, 0.22, 0.0, Rgb(196, 186, 164));
					W.Box(0.0, -7.95, 3.85, 3.9, 0.1, 0.15, 0.0, Rgb(196, 186, 164));
					Pane(W, 0, -8.0, -2.0, 1.75, 1.3, 1.25, Glass);
					Pane(W, 0, -8.0, 2.0, 1.75, 1.3, 1.25, Glass);
					Pane(W, 0, -8.0, 0.0, 1.35, 0.5, 1.2, Rgb(70, 58, 48));
					for (int Wn = 0; Wn < 3; ++Wn) Window(W, 0, -8.0, -2.4 + 2.4 * Wn, 5.8, 0.42, 0.75);
					const Tone Awn = Unit(Hash(Seed, 0xA3A)) < 0.5 ? Rgb(64, 96, 70) : Rgb(150, 70, 56);
					Face(W, { { -3.7, -8.0, 3.4 }, { 3.7, -8.0, 3.4 }, { 3.7, -9.6, 2.85 }, { -3.7, -9.6, 2.85 } }, { 0.0, -0.3, 1.0 }, Awn);
					Face(W, { { -3.7, -8.0, 3.38 }, { 3.7, -8.0, 3.38 }, { 3.7, -9.6, 2.83 }, { -3.7, -9.6, 2.83 } }, { 0.0, 0.0, -1.0 }, Varied(Awn, 0.6, 0.0));
					for (int Wn = 0; Wn < 4; ++Wn) Window(W, 1, 8.0, -2.4 + 1.6 * Wn, 5.8, 0.4, 0.7);
					break;
				}
				case KFront:
				{
					// a frame store behind its square false front: 7 m wide and 14 deep, a gable roof behind the front's 7.4 m,
					// painted white; its shop window and door, two windows over them
					const Tone Paint = Rgb(214, 210, 198);
					W.Box(0.0, 0.0, 2.3, 3.5, 7.0, 2.3, 0.0, Paint);
					Gable(W, 0.0, 0.0, 7.0, 14.0, 4.6, 6.2, 0.2, 0.0, Tin, Paint);
					W.Box(0.0, -7.0, 3.7, 3.6, 0.1, 3.7, 0.0, Paint);
					W.Box(0.0, -7.1, 7.3, 3.7, 0.12, 0.12, 0.0, Rgb(176, 172, 160));
					Pane(W, 0, -7.1, -1.6, 1.7, 1.4, 1.1, Glass);
					Pane(W, 0, -7.1, 1.9, 1.3, 0.5, 1.2, Rgb(70, 58, 48));
					Window(W, 0, -7.1, -1.5, 4.9, 0.45, 0.75);
					Window(W, 0, -7.1, 1.5, 4.9, 0.45, 0.75);
					break;
				}
				case KChurch:
				{
					// a white frame church: its nave 16 m long (x) and 9 wide, walls 5.2 m, a steep gable roof to 9.6; the
					// steeple at its front (-x): a square tower to 12 m, its belfry's louvres, the spire to 21 m; tall windows
					// down each side
					W.Box(0.0, 0.0, 2.6, 8.0, 4.5, 2.6, 0.0, White);
					Gable(W, 0.0, 0.0, 16.0, 9.0, 5.2, 9.6, 0.3, 0.3, Shingle, White);
					W.Box(-8.6, 0.0, 6.0, 1.7, 1.7, 6.0, 0.0, White);
					Pane(W, 2, -10.3, 0.0, 10.4, 0.7, 0.8, Rgb(70, 66, 60));
					Pane(W, 0, -1.7, -8.6, 10.4, 0.7, 0.8, Rgb(70, 66, 60));
					Pane(W, 1, 1.7, -8.6, 10.4, 0.7, 0.8, Rgb(70, 66, 60));
					W.Box(-8.6, 0.0, 12.1, 1.85, 1.85, 0.12, 0.0, Trim);
					Drum(W, -8.6, 0.0, 2.0, 12.2, 0.05, 21.0, 4, Shingle);
					Pane(W, 2, -10.3, 0.0, 1.5, 0.75, 1.5, Rgb(86, 68, 52));
					for (int Wn = 0; Wn < 4; ++Wn)
					{
						const double X = -4.8 + 3.2 * Wn;
						Window(W, 0, -4.5, X, 2.9, 0.5, 1.4);
						Window(W, 1, 4.5, X, 2.9, 0.5, 1.4);
					}
					break;
				}
				case KElevator:
				{
					// a country grain elevator: its bin house 8 by 8 m and 22 high, the headhouse over it to 27.5 m under its
					// gable, the driveway's shed against its side; corrugated iron, rust-red, its name board pale
					const Tone Iron = Rgb(132, 64, 50);
					W.Box(-1.0, 0.0, 11.0, 4.0, 4.0, 11.0, 0.0, Iron);
					W.Box(-1.0, 0.0, 24.0, 2.0, 4.0, 2.0, 0.0, Iron);
					Gable(W, -1.0, 0.0, 4.0, 8.0, 26.0, 27.5, 0.2, 0.2, Tin, Iron);
					Gable(W, -1.0, 0.0, 8.0, 8.0, 22.0, 22.6, 0.25, 0.25, Tin, Iron);
					W.Box(5.0, 0.0, 2.8, 2.0, 3.0, 2.8, 0.0, Iron);
					Face(W, { { 3.0, -3.2, 5.9 }, { 7.2, -3.2, 5.0 }, { 7.2, 3.2, 5.0 }, { 3.0, 3.2, 5.9 } }, { 0.3, 0.0, 1.0 }, Tin);
					Pane(W, 0, -4.0, -1.0, 17.0, 3.0, 0.8, Rgb(206, 200, 184));
					Pane(W, 3, 7.0, 0.0, 2.0, 1.8, 2.0, Rgb(48, 44, 40));
					break;
				}
				case KPole:
				{
					// a telephone pole: cedar, 8.5 m, its crossarm, four glass insulators, two wires on to the next pole 40 m on
					// (+x)
					const Tone Cedar = Rgb(96, 84, 70);
					Beam(W, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 8.5 }, 0.22, Cedar);
					W.Box(0.0, 0.0, 7.9, 0.06, 0.8, 0.06, 0.0, Cedar);
					for (const double Y : { -0.7, -0.3, 0.3, 0.7 }) W.Bead(0.0, Y, 8.02, 0.035, 0.08, Rgb(110, 150, 140), Rgb(80, 110, 100));
					M.Far = M.Near;
					for (const double Y : { -0.7, 0.7 }) W.Box(0.5 * PoleStep, Y, 8.05, 0.5 * PoleStep, 0.006, 0.006, 0.0, Rgb(60, 60, 58));
					bFar = true;
					break;
				}
				case KPost:
				{
					// a split oak post, weathered, 1.3 m out of the ground; three strands of barbed wire to the next post 5 m on
					W.Box(0.0, 0.0, 0.6, 0.055, 0.05, 0.7, 0.0, Rgb(100, 88, 72));
					M.Far = M.Near;
					for (const double Z : { 0.55, 0.85, 1.15 }) W.Box(0.5 * PostStep, 0.0, Z, 0.5 * PostStep, 0.006, 0.006, 0.0, Rgb(84, 80, 76));
					bFar = true;
					break;
				}
				case KRail:
				{
					// a worm fence: two panels of split rails laid zig and zag at some 16 degrees to the line, six rails high,
					// their ends crossed at the corners, 6.4 m of line in all (+x)
					const Tone Rail = Rgb(128, 118, 104);
					for (int Pn = 0; Pn < 2; ++Pn)
					{
						const double X0 = Pn == 0 ? 0.0 : 3.2, Y0 = Pn == 0 ? -0.45 : 0.45, Y1 = -Y0;
						for (int R = 0; R < 6; ++R)
						{
							const double Z = 0.12 + 0.22 * R, Wob = 0.03 * (Unit(Hash(0x4A11, Pn * 6 + R)) - 0.5);
							Beam(W, { X0 - 0.15, Y0 + Wob, Z }, { X0 + 3.35, Y1 + Wob, Z + 0.02 }, 0.09, Varied(Rail, 0.9 + 0.2 * Unit(Hash(0x4A12, Pn * 6 + R)), 0.0));
						}
					}
					bFar = true;
					// (far off, three rails to a panel, both faces: the sky between them, no wall)
					Writer F(M.Far);
					for (int Pn = 0; Pn < 2; ++Pn)
					{
						const double X0 = Pn == 0 ? 0.0 : 3.2, Y0 = Pn == 0 ? -0.45 : 0.45;
						for (const double Z : { 0.2, 0.62, 1.04 })
							for (const double S : { -1.0, 1.0 }) Face(F, { { X0, Y0, Z }, { X0 + 3.2, -Y0, Z }, { X0 + 3.2, -Y0, Z + 0.14 }, { X0, Y0, Z + 0.14 } }, { -S * Y0, S, 0.0 }, Rail);
					}
					break;
				}
				case KShock:
				{
					// a corn shock: the stalks stood up round a core, tied a little over halfway, their tops flaring over the
					// tie; 1.9 m across its foot, 2.4 high (a grain shock of sheaves by its size and tint)
					const Tone Stalk = Rgb(166, 146, 104), Low = Rgb(132, 114, 82);
					Drum(W, 0.0, 0.0, 0.95, 0.0, 0.38, 1.55, 10, Low);
					Drum(W, 0.0, 0.0, 0.38, 1.55, 0.3, 1.7, 10, Rgb(120, 104, 74));
					Drum(W, 0.0, 0.0, 0.3, 1.7, 0.55, 2.4, 10, Stalk);
					for (int B = 0; B < 10; ++B)
					{
						const double A = 2.0 * Pi * B / 10.0 + 0.2;
						W.Blade(0.5 * std::cos(A), 0.5 * std::sin(A), 2.3, 0.75 * std::cos(A), 0.75 * std::sin(A), 2.55, 0.12, Stalk, Rgb(184, 166, 120));
					}
					bFar = true;
					Writer F(M.Far);
					Drum(F, 0.0, 0.0, 0.95, 0.0, 0.3, 1.7, 5, Low);
					Drum(F, 0.0, 0.0, 0.3, 1.7, 0.55, 2.4, 5, Stalk);
					break;
				}
				case KStack:
				{
					// a haystack: a long loaf of hay 6 m by 4 and 4 high, rounded over, weathered a little greyer on top
					const Tone Hayc = Rgb(166, 150, 104), Top = Rgb(150, 138, 104);
					constexpr int Ar = 8;
					std::vector<std::vector<uint32_t>> Ring;
					for (int I = 0; I <= 6; ++I)
					{
						const double X = -3.0 + 1.0 * I, Taper = std::sqrt(std::max(0.0, 1.0 - std::pow(X / 3.3, 4.0)));
						std::vector<uint32_t> Rw;
						for (int J = 0; J <= Ar; ++J)
						{
							const double A = Pi * J / Ar, Ca = std::cos(A), Sa = std::sin(A);
							const double Y = 2.0 * Ca * Taper, Z = 4.0 * Sa * Taper;
							Rw.push_back(W.Vert(X, Y, Z, 0.25 * X / 3.0, Ca, Sa, Mixed(Hayc, Top, Sa)));
						}
						Ring.push_back(Rw);
					}
					for (size_t I = 0; I + 1 < Ring.size(); ++I)
						for (size_t J = 0; J + 1 < Ring[I].size(); ++J)
						{
							W.Tri(Ring[I][J], Ring[I + 1][J], Ring[I + 1][J + 1]);
							W.Tri(Ring[I][J], Ring[I + 1][J + 1], Ring[I][J + 1]);
						}
					break;
				}
				default: W.Box(0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, Rgb(150, 146, 136)); break;
				}
				if (!bFar) M.Far = M.Near;
			}
			int DrawStep() const override { return 2; }

			// ---------------------------------------------------------------- what stands on it

			/** A lane's point and its heading at arc length At along it (metres), false past its end. */
			static bool Along(const std::vector<std::pair<double, double>>& L, double At, double& OX, double& OY, double& OA)
			{
				double S = 0.0;
				for (size_t K = 0; K + 1 < L.size(); ++K)
				{
					const double DX = L[K + 1].first - L[K].first, DY = L[K + 1].second - L[K].second, Lg = std::hypot(DX, DY);
					if (S + Lg >= At && Lg > 1e-9)
					{
						const double T = (At - S) / Lg;
						OX = L[K].first + DX * T;
						OY = L[K].second + DY * T;
						OA = std::atan2(DY, DX);
						return true;
					}
					S += Lg;
				}
				return false;
			}
			static double Length(const std::vector<std::pair<double, double>>& L)
			{
				double S = 0.0;
				for (size_t K = 0; K + 1 < L.size(); ++K) S += std::hypot(L[K + 1].first - L[K].first, L[K + 1].second - L[K].second);
				return S;
			}
			static bool Farmable(int Kind) { return Kind == KField || Kind == KOpen || Kind == KRough || Kind == KYard; }

			/** A farmstead with its house at (X, Y) facing the lane along heading A (the lane on its -y side as built: Side
			 * says which side of the lane it is), its yard behind it; false where its house does not fit. */
			bool Farmstead(Host& H, Rng& R, double X, double Y, double A, int& Built)
			{
				const double Ca = std::cos(A), Sa = std::sin(A);
				// (the yard's frame: U along the lane, V away from it)
				auto At = [&](double U, double V, double& OX, double& OY) {
					OX = X + U * Ca - V * Sa;
					OY = Y + U * Sa + V * Ca;
				};
				// (a farm's trees and yard stand some 100 m round its house: none laid in another's)
				for (const auto& [OX, OY] : Farmhouses)
					if (std::hypot(X - OX, Y - OY) < 240.0) return false;
				const bool bTwo = R.Next() < 0.7;
				if (!H.House(KitOf(bTwo ? KHouse : KHouse2), X, Y, bTwo ? A : A, bTwo ? 10.6 : 7.6, bTwo ? 9.4 : 12.0)) return false;
				++Built;
				Farmhouses.push_back({ X, Y });
				double PX = 0.0, PY = 0.0;
				// the barn behind the house, its gable end to the yard or its side, the silo at its end
				const double BU = R.Range(-22.0, 22.0), BV = R.Range(48.0, 62.0);
				const bool bAcross = R.Next() < 0.5;
				const double BA = bAcross ? A + 0.5 * Pi : A;
				At(BU, BV, PX, PY);
				if (H.House(KitOf(KBarn), PX, PY, BA, 20.6, 13.4))
				{
					++Built;
					const double End = R.Next() < 0.5 ? 1.0 : -1.0;
					const double SX = PX + End * 16.2 * std::cos(BA) - 2.0 * std::sin(BA), SY = PY + End * 16.2 * std::sin(BA) + 2.0 * std::cos(BA);
					if (H.House(KitOf(KSilo), SX, SY, BA, 4.6, 4.6)) ++Built;
				}
				// the crib and the shed round the yard, the windmill between the house and the barn
				At(BU + (BU > 0.0 ? -30.0 : 30.0), R.Range(30.0, 40.0), PX, PY);
				if (H.House(KitOf(KCrib), PX, PY, A + 0.5 * Pi, 8.8, 3.8)) ++Built;
				At(BU + (BU > 0.0 ? -28.0 : 28.0), R.Range(62.0, 72.0), PX, PY);
				if (H.House(KitOf(KShed), PX, PY, A + Pi, 14.6, 7.0)) ++Built;
				At(R.Range(-16.0, -8.0), R.Range(18.0, 26.0), PX, PY);
				if (H.Free(PX, PY, 2.0)) H.Plant(KitOf(KMill), PX, PY, A + R.Range(-0.4, 0.4), 1.0, 1.0, Tint(1.0, 1.0, 1.0));
				// the yard's ground
				At(0.0, 34.0, PX, PY);
				Steads.push_back(Stead{ PX, PY, 52.0 });
				// the trees: shade trees by the house, a windbreak behind it all, an orchard to one side (half the farms)
				const LandField::Rgb8 Leaf = Tint(1.0, 1.0, 1.0);
				for (int T = 0; T < 3; ++T)
				{
					At(R.Range(-14.0, 14.0), R.Range(-4.0, 12.0) + (T == 0 ? -1.0 : 0.0), PX, PY);
					const double Sc = R.Range(0.85, 1.15);
					if (H.Free(PX, PY, 3.0)) H.Plant(T == 1 ? LandField::Base::Elm : LandField::Base::Beech, PX, PY, R.Range(0.0, 2.0 * Pi), Sc, Sc, Leaf);
				}
				for (int Row = 0; Row < 2; ++Row)
					for (double U = -48.0; U <= 48.0; U += 7.0)
					{
						At(U + R.Range(-1.0, 1.0), 88.0 + 6.0 * Row + R.Range(-0.8, 0.8), PX, PY);
						const double Sc = R.Range(0.8, 1.1);
						const LandField::Base B = Row == 0 ? LandField::Base::Elm : R.Next() < 0.5 ? LandField::Base::Ash : LandField::Base::Oak;
						if (H.Free(PX, PY, 2.5)) H.Plant(B, PX, PY, R.Range(0.0, 2.0 * Pi), B == LandField::Base::Elm ? 0.75 * Sc : Sc, Sc, Leaf);
					}
				if (R.Next() < 0.5)
				{
					const double OU = BU > 0.0 ? -38.0 : 26.0;
					for (int I = 0; I < 4; ++I)
						for (int J = 0; J < 3; ++J)
						{
							At(OU + 8.0 * I, 8.0 + 8.0 * J, PX, PY);
							if (H.Free(PX, PY, 2.0)) H.Plant(LandField::Base::Apple, PX, PY, R.Range(0.0, 2.0 * Pi), 0.9, 0.9, Leaf);
						}
				}
				// a haystack or two by the barn
				for (int S = 0; S < 2; ++S)
				{
					At(BU + R.Range(-14.0, 14.0), BV + R.Range(14.0, 20.0), PX, PY);
					if (Month >= 7 || Month <= 3) Put(H, KitOf(KStack), PX, PY, 3.5, A + R.Range(-0.3, 0.3), R.Range(0.85, 1.1), R.Range(0.85, 1.1));
				}
				return true;
			}

			static void Put(Host& H, LandField::Base B, double X, double Y, double Room, double Yaw, double Scale, double ScaleZ)
			{
				if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, Scale, ScaleZ, Tint(1.0, 1.0, 1.0));
			}

			void Sites(Host& H) override
			{
				Steads.clear();
				Rng R(H.Seed() ^ 0xA3E7);
				int Built = 0, Farms = 0;
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				// The town: main street is the lane nearest its middle; its stores face it on both sides, the side streets
				// square to it.
				if (bTown)
				{
					size_t Main = H.Lanes();
					double Best = 1e18, MA = 0.0, MS = 0.0;
					for (size_t K = 0; K < H.Lanes(); ++K)
					{
						const auto& L = H.LaneLine(K);
						double S = 0.0;
						for (size_t I = 0; I + 1 < L.size(); ++I)
						{
							const double D = std::hypot(L[I].first - TX, L[I].second - TY);
							if (D < Best)
							{
								Best = D;
								Main = K;
								MA = std::atan2(L[I + 1].second - L[I].second, L[I + 1].first - L[I].first);
								MS = S;
								TownMX = L[I].first;
								TownMY = L[I].second;
							}
							S += std::hypot(L[I + 1].first - L[I].first, L[I + 1].second - L[I].second);
						}
					}
					if (Main < H.Lanes() && Best < TR)
					{
						bTownGrid = true;
						TownCa = std::cos(MA);
						TownSa = std::sin(MA);
						TownReach = TR * 1.05;
						const auto& L = H.LaneLine(Main);
						const double Half = H.LaneHalf(Main);
						// the stores, shoulder to shoulder for a few blocks of main street, a vacant lot here and there
						for (double U = -0.5 * TR; U <= 0.5 * TR; U += 8.0)
						{
							double X = 0.0, Y = 0.0, A = 0.0;
							if (!Along(L, MS + U, X, Y, A)) continue;
							for (const double Sd : { 1.0, -1.0 })
							{
								if (R.Next() < 0.12) continue;
								const bool bBrick = R.Next() < 0.6;
								const double Deep = bBrick ? 16.0 : 14.2, Set = Half + 1.8 + 0.5 * Deep;
								const double SX = X - Sd * std::sin(A) * Set, SY = Y + Sd * std::cos(A) * Set;
								if (H.House(KitOf(bBrick ? KStore : KFront), SX, SY, Sd > 0.0 ? A : A + Pi, bBrick ? 7.8 : 7.4, Deep)) ++Built;
							}
						}
						// the church a block off main street; the grain elevator at the edge of town by the road
						for (int K = 0; K < 30; ++K)
						{
							const double U = R.Range(-0.4, 0.4) * TR, V = (R.Next() < 0.5 ? 1.0 : -1.0) * R.Range(0.7, 1.3) * StreetStep;
							const double CX = TownMX + U * TownCa - V * TownSa, CY = TownMY + U * TownSa + V * TownCa;
							if (H.House(KitOf(KChurch), CX, CY, MA + (V > 0.0 ? 0.5 * Pi : -0.5 * Pi), 21.0, 10.0))
							{
								++Built;
								break;
							}
						}
						for (int K = 0; K < 20; ++K)
						{
							double X = 0.0, Y = 0.0, A = 0.0;
							const double U = (R.Next() < 0.5 ? 1.0 : -1.0) * R.Range(0.6, 0.95) * TR;
							if (!Along(L, MS + U, X, Y, A)) continue;
							const double Sd = R.Next() < 0.5 ? 1.0 : -1.0, Set = Half + 9.0;
							if (H.House(KitOf(KElevator), X - Sd * std::sin(A) * Set, Y + Sd * std::cos(A) * Set, Sd > 0.0 ? A : A + Pi, 14.4, 10.0))
							{
								++Built;
								break;
							}
						}
						// the houses on their lots along the side streets, facing them; along main street beyond the stores
						for (int St = -2; St <= 2; ++St)
						{
							if (St == 0) continue;
							for (const double Sd : { -1.0, 1.0 })
							{
								const double V = St * StreetStep + Sd * 15.0;
								for (double U = -TR; U <= TR; U += 21.0)
								{
									const double FU = U / CrossStep - std::floor(U / CrossStep + 0.5);
									if (std::fabs(FU) * CrossStep < 13.0) continue;
									const double Out = std::hypot(U, V) / TR;
									if (Out > 1.0 || R.Next() < 0.15 + 0.5 * Smooth(0.6, 1.0, Out)) continue;
									const double HX = TownMX + U * TownCa - V * TownSa, HY = TownMY + U * TownSa + V * TownCa;
									const bool bGable = R.Next() < 0.7;
									// (its front to the street: the street on its -y side)
									const double Yaw = Sd > 0.0 ? MA : MA + Pi;
									if (H.House(KitOf(bGable ? KHouse2 : KHouse), HX, HY, Yaw, bGable ? 7.6 : 10.6, bGable ? 12.0 : 9.4)) ++Built;
								}
							}
						}
						for (double U = 0.55 * TR; U <= 1.0 * TR; U += 22.0)
							for (const double Dir : { -1.0, 1.0 })
								for (const double Sd : { -1.0, 1.0 })
								{
									double X = 0.0, Y = 0.0, A = 0.0;
									if (!Along(L, MS + Dir * U, X, Y, A) || R.Next() < 0.3) continue;
									const double Set = Half + 12.0;
									if (H.House(KitOf(KHouse2), X - Sd * std::sin(A) * Set, Y + Sd * std::cos(A) * Set, Sd > 0.0 ? A : A + Pi, 7.6, 12.0)) ++Built;
								}
					}
				}
				// The farmsteads along the roads, every half mile or so, on one side or the other, out of town.
				for (size_t K = 0; K < H.Lanes(); ++K)
				{
					const auto& L = H.LaneLine(K);
					const double Total = Length(L), Half = H.LaneHalf(K);
					for (double S = R.Range(80.0, 400.0); S < Total; S += R.Range(420.0, 760.0))
					{
						double X = 0.0, Y = 0.0, A = 0.0;
						if (!Along(L, S, X, Y, A)) break;
						if (bTown && std::hypot(X - TX, Y - TY) < TR + 160.0) continue;
						const double Sd = R.Next() < 0.5 ? 1.0 : -1.0, Set = Half + 26.0;
						const double FX = X - Sd * std::sin(A) * Set, FY = Y + Sd * std::cos(A) * Set;
						if (!Farmable(H.At(FX, FY).Kind)) continue;
						if (Farmstead(H, R, FX, FY, Sd > 0.0 ? A : A + Pi, Built)) ++Farms;
					}
				}
				// (where the rules lay a farmyard, a farmstead on it too)
				for (int K = 0; K < 240 && Farms < 40; ++K)
				{
					const double X = R.Range(0.0, H.Width()), Y = R.Range(0.0, H.Depth());
					if (H.At(X, Y).Kind != KYard || YardAt(X, Y) > 0.0) continue;
					if (Farmstead(H, R, X, Y, R.Next() < 0.5 ? 0.0 : 0.5 * Pi, Built)) ++Farms;
				}
				// The rest on the section lines, a farm to a quarter section or so as the survey sold them, facing the line.
				const double Mile = 4.0 * SurveyLine;
				for (int Dir = 0; Dir < 2; ++Dir)
					for (double Line = 0.0; Line < (Dir == 0 ? H.Width() : H.Depth()) + 1.0; Line += Mile)
						for (double S = R.Range(60.0, 200.0); S < (Dir == 0 ? H.Depth() : H.Width()) && Farms < 40; S += 2.0 * SurveyLine)
						{
							if (R.Next() < 0.55) continue;
							const double X = Dir == 0 ? Line : S, Y = Dir == 0 ? S : Line, A = Dir == 0 ? 0.5 * Pi : 0.0;
							if (bTown && std::hypot(X - TX, Y - TY) < TR + 200.0) continue;
							const double Sd = R.Next() < 0.5 ? 1.0 : -1.0, Set = 22.0;
							const double FX = X - Sd * std::sin(A) * Set, FY = Y + Sd * std::cos(A) * Set;
							if (!Farmable(H.At(FX, FY).Kind) || YardAt(FX, FY) > 0.0) continue;
							if (Farmstead(H, R, FX, FY, Sd > 0.0 ? A : A + Pi, Built)) ++Farms;
						}
				H.Tally(Built, 0, Farms);
			}

			void Plants(Host& H) override
			{
				const double Bd = H.Band();
				const LandField::Rgb8 Leaf = Tint(1.0, 1.0, 1.0);
				// The woods, the creeks, the town's elms, a lone bur oak in the pasture: a tree to a cell of 8 m where it falls.
				{
					constexpr double Cell = 8.0;
					const int NI = static_cast<int>(std::ceil((H.Width() + 2.0 * Bd) / Cell)), NJ = static_cast<int>(std::ceil((H.Depth() + 2.0 * Bd) / Cell));
					const uint64_t Key0 = H.Seed() ^ 0xA3E5;
					for (int J = 0; J < NJ; ++J)
						for (int I = 0; I < NI; ++I)
						{
							Rng R(Hash(Key0, static_cast<uint64_t>(I), static_cast<uint64_t>(J)));
							const double X = -Bd + (I + R.Next()) * Cell, Y = -Bd + (J + R.Next()) * Cell;
							// (every draw in a statement of its own, in a set order: g++ and clang take a call's arguments in
							// different orders)
							const double Keep = R.Next(), Kind = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Sc = R.Range(0.8, 1.15), Sz = R.Range(0.85, 1.15);
							const Spot S = H.At(X, Y);
							if (S.Kind == KWater || S.Kind == KLane) continue;
							auto Tree = [&](double Room) {
								const LandField::Base B = Kind < 0.45 ? LandField::Base::Oak : Kind < 0.65 ? LandField::Base::Beech : Kind < 0.85 ? LandField::Base::Ash : LandField::Base::Elm;
								if (H.Free(X, Y, Room)) H.Plant(B, X, Y, Yaw, B == LandField::Base::Elm ? 0.8 * Sc : Sc, Sz, Leaf);
							};
							switch (S.Kind)
							{
							case KWoods:
								if (Keep < 0.3) Tree(2.2);
								break;
							case KMarsh:
								if (Keep < 0.05) Tree(2.2);
								break;
							case KVillage:
							{
								// (the elms along the side streets, the maples and oaks in the yards)
								const double St = StreetGap(X, Y);
								if (St > 5.5 && St < 9.0 && Keep < 0.45)
								{
									if (H.Free(X, Y, 2.5)) H.Plant(LandField::Base::Elm, X, Y, Yaw, 0.85 * Sc, Sz, Leaf);
								}
								else if (Keep < 0.03) Tree(3.0);
								break;
							}
							case KYard:
								if (Keep < 0.03) Tree(3.0);
								break;
							default:
								if (S.Kind == KOpen && S.Crop == Pasture && Keep < 0.0015)
								{
									if (H.Free(X, Y, 4.0)) H.Plant(LandField::Base::Oak, X, Y, Yaw, 1.25 * Sc, 1.1 * Sz, Leaf);
								}
								else if (S.Edge < 2.5 && Keep < 0.004) Tree(2.5); // (a tree let grow in the fence row)
								break;
							}
						}
				}
				Fences(H);
				Poles(H);
				Shocks(H);
			}

			/** The fences: barbed wire on its posts along the fields' lines where field meets field, a worm fence on some in
			 * the east; along the roads both sides, out of town. */
			void Fences(Host& H)
			{
				const double Bd = H.Band();
				auto Field = [](const Spot& S) { return S.Kind == KField || S.Kind == KOpen || S.Kind == KRough; };
				for (int Dir = 0; Dir < 2; ++Dir)
				{
					const double Span = Dir == 0 ? H.Width() : H.Depth(), Run = Dir == 0 ? H.Depth() : H.Width();
					for (int K = static_cast<int>(std::ceil(-Bd / SurveyLine)); K * SurveyLine < Span + Bd; ++K)
					{
						const double C = K * SurveyLine;
						for (double T = -Bd; T < Run + Bd; T += PostStep)
						{
							// (a stretch of a line between the survey's corners: its fence one kind all along)
							const int Stretch = static_cast<int>(std::floor(T / SurveyLine));
							const bool bRail = (bEast || bSouth) && Unit(Hash(Seed, 0xF3C1, static_cast<uint64_t>(K + 64) * 131 + static_cast<uint64_t>(Stretch + 64) + static_cast<uint64_t>(Dir) * 7919)) < 0.45;
							if (bRail && std::fmod(T + 4096.0 * RailStep, RailStep) >= PostStep) continue;
							const double X = Dir == 0 ? C : T, Y = Dir == 0 ? T : C;
							const Spot A = H.At(Dir == 0 ? X - 2.5 : X, Dir == 0 ? Y : Y - 2.5), B = H.At(Dir == 0 ? X + 2.5 : X, Dir == 0 ? Y : Y + 2.5);
							if (!Field(A) || !Field(B) || A.Key == B.Key) continue;
							if (!H.Free(X, Y, 0.5)) continue;
							const double Yaw = Dir == 0 ? 0.5 * Pi : 0.0;
							H.Plant(KitOf(bRail ? KRail : KPost), X, Y, Yaw, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						}
					}
				}
				// the roads' fences, out of town
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				for (size_t K = 0; K < H.Lanes(); ++K)
				{
					const auto& L = H.LaneLine(K);
					const double Total = Length(L), Half = H.LaneHalf(K);
					for (double S = 0.0; S + PostStep < Total; S += PostStep)
					{
						double X = 0.0, Y = 0.0, A = 0.0, X1 = 0.0, Y1 = 0.0, A1 = 0.0;
						if (!Along(L, S, X, Y, A) || !Along(L, S + PostStep, X1, Y1, A1)) break;
						if (bTown && std::hypot(X - TX, Y - TY) < TR + 60.0) continue;
						for (const double Sd : { -1.0, 1.0 })
						{
							const double Off = Half + 4.0;
							const double PX = X - Sd * std::sin(A) * Off, PY = Y + Sd * std::cos(A) * Off;
							const double QX = X1 - Sd * std::sin(A1) * Off, QY = Y1 + Sd * std::cos(A1) * Off;
							if (!Field(H.At(PX, PY)) || YardAt(PX, PY) > 0.2 || !H.Free(PX, PY, 0.5)) continue;
							H.Plant(KitOf(KPost), PX, PY, std::atan2(QY - PY, QX - PX), 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						}
					}
				}
			}

			/** The telephone line along each road, on one side, a pole every 40 m, its wires on to the next. */
			void Poles(Host& H)
			{
				for (size_t K = 0; K < H.Lanes(); ++K)
				{
					const auto& L = H.LaneLine(K);
					const double Total = Length(L), Half = H.LaneHalf(K), Sd = (K % 2) ? 1.0 : -1.0;
					for (double S = 10.0; S + PoleStep < Total; S += PoleStep)
					{
						double X = 0.0, Y = 0.0, A = 0.0, X1 = 0.0, Y1 = 0.0, A1 = 0.0;
						if (!Along(L, S, X, Y, A) || !Along(L, S + PoleStep, X1, Y1, A1)) break;
						const double Off = Half + 2.2;
						const double PX = X - Sd * std::sin(A) * Off, PY = Y + Sd * std::cos(A) * Off;
						const double QX = X1 - Sd * std::sin(A1) * Off, QY = Y1 + Sd * std::cos(A1) * Off;
						if (!H.Free(PX, PY, 0.6)) continue;
						H.Plant(KitOf(KPole), PX, PY, std::atan2(QY - PY, QX - PX), 1.0, 1.0, Tint(1.0, 1.0, 1.0));
					}
				}
			}

			/** The shocks standing in their rows: the corn's in October and November in the fields cut that way, the wheat's
			 * and the oats' for a month after their harvest; the hay's stacks after its cuttings. */
			void Shocks(Host& H)
			{
				const double Bd = H.Band();
				constexpr double Step = 12.0;
				const uint64_t Key0 = H.Seed() ^ 0x5C0D;
				const int NI = static_cast<int>(std::ceil((H.Width() + 2.0 * Bd) / Step)), NJ = static_cast<int>(std::ceil((H.Depth() + 2.0 * Bd) / Step));
				for (int J = 0; J < NJ; ++J)
					for (int I = 0; I < NI; ++I)
					{
						Rng R(Hash(Key0, static_cast<uint64_t>(I), static_cast<uint64_t>(J)));
						const double X = -Bd + (I + 0.5) * Step + R.Range(-0.8, 0.8), Y = -Bd + (J + 0.5) * Step + R.Range(-0.8, 0.8);
						const double Yaw = R.Range(0.0, 2.0 * Pi), Sc = R.Range(0.9, 1.1), Keep = R.Next();
						const Spot S = H.At(X, Y);
						if ((S.Kind != KField && S.Kind != KOpen) || S.Edge < 4.0 || SnowLying > 0.6) continue;
						const double M = MonthOf(S.Key);
						if (S.Crop == Corn && Shocked(S.Key) && M >= 9.8 && M < 12.4)
						{
							if (H.Free(X, Y, 1.2)) H.Plant(KitOf(KShock), X, Y, Yaw, Sc, Sc, Tint(1.0, 1.0, 1.0));
						}
						else if ((S.Crop == Wheat || S.Crop == Oats) && M >= (S.Crop == Wheat ? Harvest() + 0.3 : 7.6) && M < (S.Crop == Wheat ? Harvest() + 1.2 : 8.6))
						{
							// (the grain's shocks of sheaves, smaller, gold, two to a place)
							for (int K = 0; K < 2; ++K)
							{
								const double GX = X + (K ? 3.0 : -3.0), GY = Y;
								if (H.Free(GX, GY, 0.8)) H.Plant(KitOf(KShock), GX, GY, Yaw, 0.55 * Sc, 0.5 * Sc, Tint(1.18, 1.12, 0.95));
							}
						}
						else if (S.Crop == Hay && ((M >= 6.4 && M < 7.6) || (M >= 8.6 && M < 11.0)) && Keep < 0.012)
						{
							if (H.Free(X, Y, 3.5)) H.Plant(KitOf(KStack), X, Y, Yaw, 0.8 * Sc, 0.75 * Sc, Tint(1.0, 1.04, 0.96));
						}
					}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				// (the spring's thaw: the fields and the dirt roads mud)
				if ((Ground == "field" || Ground == "fields" || Ground == "road") && (Month == 3 || Month == 4)) return 4;
				return Else;
			}
			const char* Earth() const override { return bBelt ? "blackearth" : "loam"; }

		private:
			int Month = 7;
			bool bPlains = false, bEast = false, bSouth = false, bBelt = false, bBare = false;
			double SnowLying = 0.0, MeanZ = 0.0;
			Tone SoilC = Rgb(92, 80, 64), GrassT = Rgb(100, 114, 58);
			// (the town's grid, laid by Sites: square to main street, its side streets 110 m apart, its cross streets 120)
			static constexpr double StreetStep = 110.0, CrossStep = 120.0;
			bool bTownGrid = false;
			double TownMX = 0.0, TownMY = 0.0, TownCa = 1.0, TownSa = 0.0, TownReach = 0.0;
			std::vector<Stead> Steads;
			std::vector<std::pair<double, double>> Farmhouses;
		};
	}

	std::unique_ptr<Region> MakeAmerica(const LandField::Field& F, uint64_t S) { return std::make_unique<America>(F, S); }
}
