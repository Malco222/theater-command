#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>

/**
 * The desert of the North African battles (Gazala, Tobruk, Sollum, Alamein, southern Tunisia; 1941-43) and the Middle
 * East's: stone, not dunes. The look-targets research (production/environments-1122/look-targets.md, section 1) sets it:
 *
 *   - a pale stony plain (serir) of fawn gravel, its pebbles half pale limestone and half varnished near black, with thin
 *     sand sheets in the hollows and bare rocky hamada on the rises and the rules' rough ground;
 *   - wadis 15 to 40 m wide with flat sandy floors, where the scrub grows thicker (under a metre deep inside the field,
 *     where the fight reads its own heights, deeper beyond it); low swells over hundreds of metres; a low escarpment on
 *     the far horizon;
 *   - camel thorn on its hummock metres apart, never continuous; dry grass tufts; pebbles and flat stones round the eye;
 *   - braided bands of vehicle ruts beside the tracks; telegraph poles 50 m apart along the main one; sangars of piled
 *     stone where the rules dig fieldworks; flat-roofed houses packed in streets round the town, a domed tomb on a rise;
 *     date palms only in southern Tunisia's oases.
 *
 * Its kit is the Mediterranean thread's battle models (Content/Models/Med, from battle-models/med).
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Serir = 0, // the stony plain: gravel and flints over hard sand
			Sand = 1,  // a sand sheet (the parcels' word only: the paint follows the land, not the parcels)
			Scrub = 2, // a flat of camel thorn and saltbush
			Rock = 3   // hamada: bare rock and rubble
		};

		enum Slot : int
		{
			KThorn = 0,
			KBoulder = 1,
			KHouse = 2,
			KHouseStone = 3,
			KSangar = 4,
			KPole = 5,
			KPalm = 6,
			KStones = 7,
			KHouseTwo = 8,
			KDome = 9
		};

		enum Clut : int
		{
			CGrass = 0,   // a dry grass tuft
			CPebbles = 1, // a few pebbles, fist-sized and smaller
			CSlab = 2,    // a flat stone a forearm across
			CTwigs = 3,   // a small dead shrub on its sand hummock, its sand tail behind it
			CGravel = 4   // a scatter of gravel, thumbnail-sized: the serir's speckle round the eye
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (written so the ground lands, lit at noon, on the art director's screen target for the desert: sand about
		// 190,172,140, red over green 1.08-1.12, blue over green 0.78-0.84; the sky's blue and the haze pale it, so the
		// paint is darker and warmer than what is seen)
		const Tone SerirMid = Rgb(183, 155, 111);
		const Tone SerirPale = Rgb(201, 178, 134);
		const Tone Varnish = Rgb(100, 76, 54);
		/** The varnished gravel's own: dark brown to black-brown, some of it red-brown (the art director's read, 18:30Z:
		 * about L 25-40, warm). */
		const Tone GravelDark = Rgb(84, 64, 48);
		const Tone GravelRed = Rgb(122, 78, 54);
		const Tone SandSheet = Rgb(198, 174, 130);
		const Tone Limestone = Rgb(192, 180, 152);
		const Tone Dust = Rgb(196, 178, 142);
		const Tone Rut = Rgb(148, 128, 96);
		const Tone LimeUnder = Rgb(150, 136, 110);
		const Tone VarnishUnder = Rgb(112, 96, 76);
		/** Where the prevailing north-west wind lays a sand tail: south-east of each stone and bush (radians from east). */
		constexpr double Lee = -0.25 * Pi;

		/** A small stone of 15 triangles: a squat cone of five sides, its foot sunk. */
		void Chip(Writer& W, double CX, double CY, double R, double H, uint64_t S, const Tone& Lit, const Tone& Under)
		{
			const double TX = CX + 0.2 * R * (Unit(Hash(S, 1)) - 0.5), TY = CY + 0.2 * R * (Unit(Hash(S, 2)) - 0.5);
			const uint32_t Top = W.Vert(TX, TY, H, 0.0, 0.0, 1.0, Lit);
			uint32_t Up[5], Down[5];
			for (int K = 0; K < 5; ++K)
			{
				const double A = 2.0 * Pi * (K + 0.3 * Unit(Hash(S, 10 + K))) / 5.0;
				const double Push = 0.75 + 0.5 * Unit(Hash(S, 20 + K));
				const double X = std::cos(A), Y = std::sin(A);
				Up[K] = W.Vert(CX + R * Push * X, CY + R * Push * Y, 0.45 * H, X * 0.7, Y * 0.7, 0.7, Mixed(Under, Lit, 0.7));
				Down[K] = W.Vert(CX + 1.1 * R * Push * X, CY + 1.1 * R * Push * Y, -0.3 * H, X, Y, 0.0, Under);
			}
			for (int K = 0; K < 5; ++K)
			{
				const int N = (K + 1) % 5;
				W.Tri(Top, Up[K], Up[N]);
				W.Tri(Up[K], Down[K], Down[N]);
				W.Tri(Up[K], Down[N], Up[N]);
			}
		}

		/** A grain of gravel: a low rounded dome of 5 triangles, wider than tall (a point's tent reads as teeth at the
		 * eye: the art director, 18:51Z), its foot in the ground. */
		void Grain(Writer& W, double CX, double CY, double R, double H, uint64_t S, const Tone& Top, const Tone& Side)
		{
			const uint32_t Apex = W.Vert(CX, CY, H, 0.0, 0.0, 1.0, Top);
			uint32_t Foot[5];
			const double A0 = 2.0 * Pi * Unit(Hash(S, 1));
			for (int K = 0; K < 5; ++K)
			{
				const double A = A0 + 2.0 * Pi * K / 5.0, X = std::cos(A), Y = std::sin(A);
				const double Push = 0.8 + 0.4 * Unit(Hash(S, 2 + K));
				Foot[K] = W.Vert(CX + R * Push * X, CY + 0.8 * R * Push * Y, -0.15 * H, X * 0.45, Y * 0.45, 0.9, Side);
			}
			for (int K = 0; K < 5; ++K) W.Tri(Apex, Foot[K], Foot[(K + 1) % 5]);
		}

		/** A sand tail in the lee of a stone or a bush: a low ridge along +X from Start to Start + Long, Wide across at its
		 * root, tapering out; Low high. 6 triangles. */
		void Tail(Writer& W, double Start, double Long, double Wide, double Low, const Tone& Ridge, const Tone& Flank)
		{
			const uint32_t A = W.Vert(Start, 0.0, Low, 0.0, 0.0, 1.0, Ridge);
			const uint32_t L = W.Vert(Start - 0.1 * Long, 0.5 * Wide, -0.02, 0.0, 0.5, 0.85, Flank);
			const uint32_t R = W.Vert(Start - 0.1 * Long, -0.5 * Wide, -0.02, 0.0, -0.5, 0.85, Flank);
			const uint32_t M = W.Vert(Start + 0.45 * Long, 0.0, 0.55 * Low, 0.1, 0.0, 1.0, Ridge);
			const uint32_t ML = W.Vert(Start + 0.4 * Long, 0.3 * Wide, -0.02, 0.0, 0.4, 0.9, Flank);
			const uint32_t MR = W.Vert(Start + 0.4 * Long, -0.3 * Wide, -0.02, 0.0, -0.4, 0.9, Flank);
			const uint32_t E = W.Vert(Start + Long, 0.0, -0.02, 0.2, 0.0, 0.98, Flank);
			W.Tri(A, L, ML);
			W.Tri(A, ML, M);
			W.Tri(A, M, MR);
			W.Tri(A, MR, R);
			W.Tri(M, ML, E);
			W.Tri(M, E, MR);
		}

		/** Blades' points from First on lit as the sky lights the ground (their normals up, leaning a little outward):
		 * a thin blade's own normal, flipped toward the eye from behind, faces down and draws it near black. */
		void LitUp(LandField::Mesh& M, size_t First)
		{
			for (size_t P = First; P < M.Points(); ++P)
			{
				const double X = M.Positions[3 * P], Y = M.Positions[3 * P + 1], L = std::hypot(X, Y);
				const double NX = L > 1e-6 ? 0.25 * X / L : 0.0, NY = L > 1e-6 ? 0.25 * Y / L : 0.0, N = std::sqrt(NX * NX + NY * NY + 1.0);
				M.Normals[3 * P] = static_cast<float>(NX / N);
				M.Normals[3 * P + 1] = static_cast<float>(NY / N);
				M.Normals[3 * P + 2] = static_cast<float>(1.0 / N);
			}
		}

		/** A kit house's model drawn round its own footprint's middle (the kit's houses stand off their origins). */
		void Centre(LandField::BaseMesh& M)
		{
			double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
			const std::vector<float>& P = M.Near.Positions;
			for (size_t K = 0; K + 2 < P.size(); K += 3)
			{
				X0 = std::min(X0, static_cast<double>(P[K]));
				X1 = std::max(X1, static_cast<double>(P[K]));
				Y0 = std::min(Y0, static_cast<double>(P[K + 1]));
				Y1 = std::max(Y1, static_cast<double>(P[K + 1]));
			}
			if (X0 > X1) return;
			const float CX = static_cast<float>(0.5 * (X0 + X1)), CY = static_cast<float>(0.5 * (Y0 + Y1));
			for (LandField::Mesh* Each : { &M.Near, &M.Far, &M.Cards, &M.Sprigs })
				for (size_t K = 0; K + 2 < Each->Positions.size(); K += 3)
				{
					Each->Positions[K] -= CX;
					Each->Positions[K + 1] -= CY;
				}
		}

		/** A kit model's paint changed: every point of From's colour (within a step or two) takes To's, its own
		 * shading kept (the near level and the far). */
		void Repaint(LandField::BaseMesh& M, const Tone& From, const Tone& To)
		{
			for (LandField::Mesh* Each : { &M.Near, &M.Far })
				for (LandField::Rgb8& C : Each->Colours)
				{
					if (std::fabs(C.R - From.R) > 5.0 || std::fabs(C.G - From.G) > 5.0 || std::fabs(C.B - From.B) > 5.0) continue;
					C = Byte(Tone{ To.R * C.R / From.R, To.G * C.G / From.G, To.B * C.B / From.B });
				}
		}

		class Desert final : public Region
		{
		public:
			Desert(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				// (spring after the winter rains: a thin green flush in the hollows, February to April; else straw and grey)
				bSpring = F.Month >= 2 && F.Month <= 4;
				// (southern Tunisia's oases have their date palms: Gabès, Mareth, El Hamma)
				bPalms = F.Lat < 34.6 && F.Lon < 12.0 && F.Lon > 7.0;
				// The wadis read once onto a 4 m grid over the field and 320 m round it (the paint and the track marks ask
				// them many times a frame: two noises each is too dear), straight from the noise beyond it.
				GX0 = -320.0;
				GY0 = -320.0;
				GNX = static_cast<int>((F.Width() + 640.0) / GridStep) + 2;
				GNY = static_cast<int>((F.Depth() + 640.0) / GridStep) + 2;
				Grid.resize(static_cast<size_t>(GNX) * static_cast<size_t>(GNY));
				for (int J = 0; J < GNY; ++J)
					for (int I = 0; I < GNX; ++I) Grid[static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)] = static_cast<float>(WadiNoise(GX0 + I * GridStep, GY0 + J * GridStep));
				// The gravel's patches, the sand sheets and the drifts on them, broader, on a 12 m grid over the field and
				// 1.2 km round it (the ring's first kilometre read from it too), three to a point.
				CX0 = -1200.0;
				CY0 = -1200.0;
				CNX = static_cast<int>((F.Width() + 2400.0) / CoarseStep) + 2;
				CNY = static_cast<int>((F.Depth() + 2400.0) / CoarseStep) + 2;
				Coarse.resize(3 * static_cast<size_t>(CNX) * static_cast<size_t>(CNY));
				for (int J = 0; J < CNY; ++J)
					for (int I = 0; I < CNX; ++I)
					{
						const size_t K = 3 * (static_cast<size_t>(J) * static_cast<size_t>(CNX) + static_cast<size_t>(I));
						Coarse[K] = static_cast<float>(VarnNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
						Coarse[K + 1] = static_cast<float>(SandNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
						Coarse[K + 2] = static_cast<float>(DriftNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
					}
			}
			Look Which() const override { return Look::Desert; }

			// ---------------------------------------------------------------- the land

			/** How far a place lies outside the laid field (0 inside). */
			double Outside(double X, double Y) const
			{
				const double Dx = std::max({ 0.0, -X, X - Fd.Width() }), Dy = std::max({ 0.0, -Y, Y - Fd.Depth() });
				return std::sqrt(Dx * Dx + Dy * Dy);
			}
			/** A wadi here: 1 on its floor's middle, 0 out on the plain. Its lines wander some 650 m apart; 15 to 40 m across. */
			double WadiNoise(double X, double Y) const
			{
				const double Warp = 0.35 * Noise(X / 230.0, Y / 230.0, Seed ^ 0xA2);
				const double N = Noise(X / 640.0 + Warp, Y / 640.0, Seed ^ 0xA1);
				return Smooth(0.032, 0.011, std::fabs(N));
			}
			/** The varnished gravel's patches: 1 where the pebbles lie thickest, 0 on the pale serir; organic, soft-edged
			 * shapes tens of metres across (a noise bent by another, so its lattice never shows as square cells), some
			 * four in ten of the plain. */
			double VarnNoise(double X, double Y) const
			{
				// (broad soft zones a few hundred metres across where the gravel lies thick, broken within by smaller
				// patches; never one size everywhere, which reads as camouflage from above)
				const double WX = 60.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0xF1), WY = 60.0 * Noise(X / 170.0 + 5.2, Y / 170.0 - 1.3, Seed ^ 0xF2);
				const double U = 0.8 * (X + WX) - 0.6 * (Y + WY), V = 0.6 * (X + WX) + 0.8 * (Y + WY);
				const double Zone = Fbm(U, V, 420.0, 2, Seed ^ 0xF8);
				const double Patch = Fbm(V, -U, 75.0, 3, Seed ^ 0xF3);
				return Smooth(-0.3, 0.3, 0.9 * Zone + 0.6 * Patch);
			}
			/** The sand sheets: 1 on a sheet, 0 off it; a quarter of the plain in soft shapes a few hundred metres long,
			 * drawn out a little along the wind (north-west to south-east). */
			double SandNoise(double X, double Y) const
			{
				const double WX = 110.0 * Noise(X / 340.0, Y / 340.0, Seed ^ 0xF4), WY = 110.0 * Noise(X / 340.0 - 2.7, Y / 340.0 + 3.1, Seed ^ 0xF5);
				const double U = 0.7071 * (X + WX) - 0.7071 * (Y + WY), V = 0.7071 * (X + WX) + 0.7071 * (Y + WY);
				return Smooth(0.1, 0.42, Fbm(0.8 * U, 1.2 * V, 280.0, 3, Seed ^ 0xF6));
			}
			/** The drifts on the plain: soft paler and darker streaks 10 to 40 m across, drawn out along the wind, where the
			 * sand lies a little deeper or the lag gravel shows through it (-1 to 1). They carry the ground's breakup from the
			 * battle camera's height, where the clutter is gone (the art director, 19:17Z). */
			double DriftNoise(double X, double Y) const
			{
				const double WX = 20.0 * Noise(X / 70.0, Y / 70.0, Seed ^ 0xF9), WY = 20.0 * Noise(X / 70.0 + 3.7, Y / 70.0 - 2.2, Seed ^ 0xFA);
				const double U = 0.7071 * (X + WX) - 0.7071 * (Y + WY), V = 0.7071 * (X + WX) + 0.7071 * (Y + WY);
				return std::clamp(1.7 * Fbm(0.6 * U, 1.4 * V, 34.0, 2, Seed ^ 0xFB), -1.0, 1.0);
			}
			/** The gravel's patch (Ch 1), the sand sheet (Ch 2) or the drift (Ch 3) here, read between the coarse grid's
			 * points; beyond it the drifts are lost in the haze (0). */
			double Read(double X, double Y, int Ch) const
			{
				const double U = (X - CX0) / CoarseStep, V = (Y - CY0) / CoarseStep;
				if (!(U >= 0.0 && V >= 0.0 && U < CNX - 1.0 && V < CNY - 1.0)) return Ch == 1 ? VarnNoise(X, Y) : Ch == 2 ? SandNoise(X, Y) : 0.0;
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = 3 * (static_cast<size_t>(J) * static_cast<size_t>(CNX) + static_cast<size_t>(I)) + static_cast<size_t>(Ch - 1);
				const size_t Row = 3 * static_cast<size_t>(CNX);
				const double A = Coarse[K], B = Coarse[K + 3], C = Coarse[K + Row], D = Coarse[K + Row + 3];
				return (A + (B - A) * FU) + ((C + (D - C) * FU) - (A + (B - A) * FU)) * FV;
			}
			double Wadi(double X, double Y) const
			{
				const double U = (X - GX0) / GridStep, V = (Y - GY0) / GridStep;
				if (!(U >= 0.0 && V >= 0.0 && U < GNX - 1.0 && V < GNY - 1.0)) return WadiNoise(X, Y);
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I);
				const double A = Grid[K], B = Grid[K + 1], C = Grid[K + static_cast<size_t>(GNX)], D = Grid[K + static_cast<size_t>(GNX) + 1];
				return (A + (B - A) * FU) + ((C + (D - C) * FU) - (A + (B - A) * FU)) * FV;
			}
			double Relief(double X, double Y, double Z) const override
			{
				// (low swells a few hundred metres across, enough to hide a hull at a kilometre; the wadis cut under a
				// metre inside the field, where the fight reads its own heights, and two or three beyond it)
				const double Deep = 0.9 + 1.1 * Smooth(0.0, 250.0, Outside(X, Y));
				return Z + 0.4 * Fbm(X, Y, 300.0, 2, Seed ^ 0xD1) - Deep * Wadi(X, Y);
			}
			/** The escarpment north of the field: metres north of its foot (negative south of it). */
			double Scarp(double X, double Y) const
			{
				const double Line = Fd.Depth() + 800.0 + 300.0 * Noise(X / 2600.0, 0.5, Seed ^ 0xE1) + 50.0 * Noise(X / 300.0, 1.5, Seed ^ 0xE2);
				return Y - Line;
			}
			double Rolling(double X, double Y, double MeanZ) const override
			{
				// (the plain undulating a metre or two, the deirs' shallow bowls, and the escarpment stepping up some 70 m
				// on the northern horizon, its top ragged: a limestone band over its scree)
				const double S = Scarp(X, Y);
				const double Up = (85.0 + 15.0 * Noise(X / 700.0, 2.5, Seed ^ 0xE3)) * (0.65 * Smooth(-110.0, 10.0, S) + 0.35 * Smooth(10.0, 45.0, S));
				return MeanZ + 6.0 * Fbm(X, Y, 3800.0, 3, Seed ^ 0xB1) + 1.5 * Fbm(X, Y, 900.0, 2, Seed ^ 0xB2) + Up;
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.0; } // (a track worn into the crust, no sunken lane)
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.5 : 3.2) + 2.0 * U; } // (a desert track braids wide)
			double PlotMin() const override { return 90000.0; }
			double PlotMax() const override { return 220000.0; }
			double FarTilled() const override { return 0.0; }
			double WoodsEdge() const override { return 0.9; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Rock;
				if (Kind == KField) return Scrub;
				return U < 0.7 ? Serir : U < 0.9 ? Sand : Scrub;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Serir: return "serir";
				case Sand: return "sand";
				case Scrub: return "scrub";
				case Rock: return "rock";
				default: return "";
				}
			}

			// ---------------------------------------------------------------- the paint

			/** The plain by its land, not its parcels: sand sheets in the hollows and the wadis, rock on the rises and the
			 * rules' rough, the stony serir between, its varnished gravel darkening it in patches (Varn) and the sheets
			 * paling it (Sand). No noise finer than some 30 m: the ground's points lie 4 m apart, and a finer one draws as
			 * square cells. */
			static Tone Plain(int Kind, double N1, double N2, double Broad, double W, double Varn, double Sand, double Drift)
			{
				// (the art director's read, 18:30Z: the varnished patches 25 to 35 per cent darker than the sand, the
				// gravel's dark brown to black-brown with some red-brown; the sheets bare pale sand; the rises limestone)
				const double Sandy = Clamp01(Sand + 0.9 * W);
				const double Rocky = Kind == KRough ? 0.85 : Smooth(-0.38, -0.62, Broad + 0.2 * N1) * (1.0 - W);
				Tone T = Mixed(SerirMid, SerirPale, 0.45 + 0.18 * N1);
				T = Mixed(T, Mixed(GravelDark, GravelRed, 0.35 + 0.3 * N1), 0.12 + 0.46 * Varn);
				T = Mixed(T, Mixed(Limestone, Varnish, 0.25), Rocky);
				T = Mixed(T, Varied(SandSheet, 1.0 + 0.03 * N1, 0.0), Sandy * (1.0 - 0.6 * Rocky));
				// (the drifts: on the sheets the lag gravel showing grey-brown through thin sand and the deeper sand paler; on
				// the serir the pavement a shade lighter or darker; soft, never spots)
				T = Mixed(T, Mixed(SerirMid, GravelDark, 0.3), 0.32 * Sandy * Smooth(-0.1, -0.7, Drift));
				return Varied(T, 1.0 + 0.04 * Broad + 0.012 * N2 + (0.07 + 0.06 * Sandy) * Drift, 0.015 * Broad);
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a desert track is no road (the art director's read, 18:30Z): the plain's own ground a little dusted,
					// braided into wandering ruts where wheels broke the crust, three to six across, feathered out into the
					// plain at its edge; no paler crown, no kerb
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Phase = Off / 0.85 + 0.6 * Noise(S.X / 38.0, S.Y / 38.0, Seed ^ 0xF7);
					const double Line = 2.0 * std::fabs(Phase - std::floor(Phase) - 0.5);
					const double RutK = Smooth(0.7, 0.95, Line) * Smooth(S.LaneHalf + 0.4, S.LaneHalf - 0.8, Off);
					Tone T = Plain(KOpen, S.N1, S.N2, S.Broad, Wadi(S.X, S.Y), Read(S.X, S.Y, 1), Read(S.X, S.Y, 2), Read(S.X, S.Y, 3));
					T = Mixed(T, Dust, 0.12 * Smooth(S.LaneHalf + 1.5, 0.4 * S.LaneHalf, Off));
					return Mixed(T, Rut, 0.5 * RutK);
				}
				case KVillage:
				case KYard: return Varied(Rgb(200, 180, 144), 1.0 + 0.05 * S.N2, 0.01);
				case KWater: return Rgb(84, 104, 104);
				case KMarsh: return Varied(Rgb(178, 172, 150), 1.0 + 0.06 * S.N1, 0.0); // (a sabkha: grey-white salt crust)
				case KWorks: return Varied(Mixed(Limestone, SerirMid, 0.5), 1.0 + 0.06 * S.N2, 0.0); // (stone thrown up from the rock)
				default: break;
				}
				const double W = Wadi(S.X, S.Y);
				Tone T = Plain(S.Kind, S.N1, S.N2, S.Broad, W, Read(S.X, S.Y, 1), Read(S.X, S.Y, 2), Read(S.X, S.Y, 3));
				if (bSpring) T = Mixed(T, Rgb(150, 150, 104), 0.22 * W + 0.08 * Smooth(0.3, 0.8, S.N1) * (1.0 - W));
				// (beside a track: the older wheels' spread, the broken crust a little paler than the plain, fading out)
				if (S.Lane < 30.0) T = Mixed(T, Dust, 0.08 * Smooth(30.0, 3.0, S.Lane) * (0.7 + 0.3 * S.N1));
				return T;
			}

			Tone Far(const Spot& S, bool /*bWoods*/, double /*Gap*/) const override
			{
				Tone T = Plain(S.Kind == KWoods ? static_cast<int>(KOpen) : S.Kind, S.N1, 0.0, S.Broad, 0.0, Read(S.X, S.Y, 1), Read(S.X, S.Y, 2), Read(S.X, S.Y, 3));
				// (the escarpment by its own contrast, which survives the haze better than its height: a scree slope at its
				// foot, a dark band of shade over it, and the cliff's pale sunlit limestone face above, lit 220 or more:
				// the art director, 18:51Z)
				const double Sc = Scarp(S.X, S.Y);
				const double Face = Smooth(-135.0, -95.0, Sc) * Smooth(20.0, -5.0, Sc);
				const Tone Scree = Mixed(Rgb(140, 118, 90), Rgb(112, 94, 72), Smooth(-125.0, -85.0, Sc));
				const Tone Shade = Mixed(Scree, Rgb(74, 60, 48), Smooth(-88.0, -74.0, Sc));
				return Mixed(T, Mixed(Shade, Rgb(230, 218, 190), Smooth(-66.0, -54.0, Sc)), 0.95 * Face);
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				if (S.Kind == KLane) return S.Lane > -0.4 ? Grown{ ClutOf(Pick < 0.5 ? CGravel : CPebbles), 0.3 } : Grown{};
				if (S.Kind == KVillage || S.Kind == KYard) return Pick < 0.7 ? Grown{ ClutOf(CPebbles), 0.15 } : Grown{ ClutOf(CTwigs), 0.05 };
				if (S.Kind == KRough)
				{
					if (Pick < 0.4) return Grown{ ClutOf(CGravel), 1.1 };
					return Pick < 0.75 ? Grown{ ClutOf(CPebbles), 0.6 } : Grown{ ClutOf(CSlab), 0.3 };
				}
				const double W = Wadi(S.X, S.Y);
				if (W > 0.25 || S.Kind == KWoods || S.Kind == KMarsh)
				{
					// (a wadi's sandy floor: dry grass and dead shrubs two to five times thicker than on the plain)
					if (Pick < 0.4) return Grown{ ClutOf(CGrass), 0.45 };
					if (Pick < 0.55) return Grown{ ClutOf(CTwigs), 0.3 };
					if (Pick < 0.8) return Grown{ ClutOf(CGravel), 0.3 };
					return Grown{ ClutOf(CPebbles), 0.15 };
				}
				// the serir: a pavement of varnished gravel, thickest on its dark patches and thinning to bare sand on the
				// sheets (the art director's read, 18:30Z), cobbles a few to the square metre, a flat stone in thirty square
				// metres, a dry tuft now and then
				const double Bare = Read(S.X, S.Y, 2), Dark = Read(S.X, S.Y, 1);
				const double Pave = (0.55 + 0.75 * Dark) * (1.0 - 0.85 * Bare);
				if (Pick < 0.55) return Grown{ ClutOf(CGravel), 1.9 * Pave };
				if (Pick < 0.8) return Grown{ ClutOf(CPebbles), 0.5 * Pave };
				if (Pick < 0.86) return Grown{ ClutOf(CSlab), 0.16 * (1.0 - 0.7 * Bare) };
				if (Pick < 0.95) return Grown{ ClutOf(CGrass), 0.25 + 0.15 * Bare };
				return Grown{ ClutOf(CTwigs), 0.18 };
			}

			bool Dress(LandField::Placed& P, const Spot& /*S*/, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				switch (K)
				{
				case CPebbles:
				case CGravel:
				{
					// (their stones dark and pale as built; a scatter a little lighter or darker as a whole)
					P.Scale = static_cast<float>(K == CGravel ? 0.75 + 0.5 * U4 : 0.8 + 0.7 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					const double Pale = 0.9 + 0.16 * U7;
					P.Tint = Tint(Pale * 1.01, Pale, Pale * 0.97);
					return true;
				}
				case CSlab:
				{
					// (fewer, bigger, nine in ten varnished dark on top, one in ten pale limestone; its sand to the lee)
					P.Scale = static_cast<float>(0.9 + 0.9 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Yaw = static_cast<float>(Lee + 0.5 * (U6 - 0.5));
					const double Pale = 0.94 + 0.12 * U7;
					P.Tint = U6 < 0.9 ? Tint(0.5 + 0.1 * U7, 0.42 + 0.08 * U7, 0.36 + 0.06 * U7) : Tint(Pale * 1.02, Pale, Pale * 0.95);
					return true;
				}
				case CGrass:
				{
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.5 * U5);
					P.Tint = bSpring && U6 < 0.5 ? Tint(0.84, 0.95, 0.74) : Tint(0.97 + 0.08 * U7, 0.97 + 0.05 * U7, 0.95);
					return true;
				}
				case CTwigs:
				{
					P.Scale = static_cast<float>(0.6 + 0.8 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.6 * U5);
					P.Yaw = static_cast<float>(Lee + 0.6 * (U7 - 0.5));
					P.Tint = Tint(0.92 + 0.12 * U6, 0.94 + 0.08 * U6, 0.94);
					return true;
				}
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				// (a sand tail is the sand's own colour, low, its flanks the plain's so its edge feathers out: one or two
				// lengths of its stone, not three or four, and never cream: the art director's read, 18:30Z)
				const Tone TailRidge = Rgb(192, 170, 128), TailFlank = Rgb(178, 156, 116);
				switch (K)
				{
				case CGrass:
				{
					// a dry tuft, pale straw, its foot darker: some twenty short blades from a tight foot, nearly upright
					for (int B = 0; B < 20; ++B)
					{
						const double A = 2.0 * Pi * (B + 0.5 * Unit(Hash(0x6A55, B))) / 20.0;
						const double Lean = 0.04 + 0.07 * Unit(Hash(0x6A56, B)), High = 0.09 + 0.12 * Unit(Hash(0x6A57, B));
						const double FX = 0.04 * std::cos(A), FY = 0.025 * std::sin(A);
						W.Blade(FX, FY, 0.0, FX + Lean * std::cos(A), FY + Lean * std::sin(A), High, 0.022, Rgb(124, 104, 72), Rgb(178, 152, 100));
					}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 40.0f;
					break;
				}
				case CPebbles:
				{
					// five cobbles, fist-sized and smaller: four varnished dark on top (one of them red-brown), one pale
					for (int B = 0; B < 5; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x9EB, B)), D = 0.24 * Unit(Hash(0x9EC, B));
						const double R = 0.035 + 0.05 * Unit(Hash(0x9ED, B));
						const Tone Top = B == 0 ? Limestone : B == 1 ? GravelRed : GravelDark;
						Chip(W, D * std::cos(A), D * std::sin(A), R, 0.42 * R, Hash(0x9EE, B), Top, B == 0 ? LimeUnder : VarnishUnder);
					}
					M.FarFrom = 30.0f;
					break;
				}
				case CGravel:
				{
					// two dozen grains a thumbnail to a walnut across, spread evenly over a metre and a half so the
					// pavement reads as a speckle, not as clusters: dark brown to black-brown, some red-brown, one in ten pale
					for (int B = 0; B < 24; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x6A7, B)), D = 0.8 * std::sqrt(Unit(Hash(0x6A8, B)));
						const double R = 0.014 + 0.026 * Unit(Hash(0x6A9, B));
						const double Which = Unit(Hash(0x6AA, B));
						const Tone Top = Which < 0.1 ? Limestone : Which < 0.3 ? GravelRed : Mixed(GravelDark, Rgb(62, 48, 38), Unit(Hash(0x6AC, B)));
						Grain(W, D * std::cos(A), D * std::sin(A), R, 0.4 * R, Hash(0x6AB, B), Top, Which < 0.1 ? LimeUnder : Mixed(Top, Rgb(40, 32, 26), 0.4));
					}
					M.FarFrom = 26.0f;
					break;
				}
				case CSlab:
				{
					// a flat stone a forearm across and a chip beside it (tinted dark or pale), its low tail to the lee
					Chip(W, 0.0, 0.0, 0.16, 0.06, 0x51AB, Limestone, LimeUnder);
					Chip(W, -0.15, 0.08, 0.05, 0.035, 0x51AC, Limestone, LimeUnder);
					Tail(W, 0.12, 0.3, 0.3, 0.04, TailRidge, TailFlank);
					M.FarFrom = 45.0f;
					break;
				}
				case CTwigs:
				{
					// a dead shrub: a dozen grey twigs splayed from a sand hummock's crown, its short sand tail to the lee
					Chip(W, 0.0, 0.0, 0.22, 0.08, 0x7A16, TailRidge, TailFlank);
					Tail(W, 0.14, 0.5, 0.36, 0.04, TailRidge, TailFlank);
					const size_t Twigs = M.Near.Points();
					for (int B = 0; B < 12; ++B)
					{
						const double A = 2.0 * Pi * Unit(Hash(0x7A17, B)), Out = 0.12 + 0.18 * Unit(Hash(0x7A18, B));
						const double High = 0.18 + 0.22 * Unit(Hash(0x7A19, B));
						W.Blade(0.0, 0.0, 0.05, Out * std::cos(A), Out * std::sin(A), High, 0.01, Rgb(92, 84, 72), Rgb(150, 142, 126));
					}
					LitUp(M.Near, Twigs);
					M.Paint = 2;
					M.FarFrom = 55.0f;
					break;
				}
				default: break;
				}
			}

			// ---------------------------------------------------------------- the kit

			const char* Folder() const override { return "Med"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KThorn: return "CamelThorn";
				case KBoulder: return "Boulder";
				case KHouse: return "FlatRoofHouse";
				case KHouseStone: return "FlatRoofHouseStone";
				case KSangar: return "Sangar";
				case KPole: return "TelegraphPole";
				case KPalm: return "DatePalm";
				case KStones: return "Stones";
				case KHouseTwo: return "FlatRoofHouseTwo";
				case KDome: return "DomeHouse";
				default: return nullptr;
				}
			}
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
				case KHouse:
				case KHouseStone:
				case KHouseTwo:
				case KDome:
					Centre(M);
					Stands(0.5f, 0.5f, 0.8f, 0.0f, 320.0f);
					// (a North African town, not grey concrete: whitewashed walls lit about 235-240, a few of ochre mud
					// brick about 200,165,115, the flat roofs pale; the art director's read, 18:30Z)
					if (K == KHouseStone)
					{
						Repaint(M, Rgb(0xE0, 0xD0, 0xAE), Rgb(206, 166, 112));
						Repaint(M, Rgb(0xE6, 0xD8, 0xBA), Rgb(214, 184, 140));
					}
					else
					{
						Repaint(M, Rgb(0xCE, 0xBA, 0x96), Rgb(246, 242, 232));
						Repaint(M, Rgb(0xE0, 0xD0, 0xAE), Rgb(246, 242, 232));
						Repaint(M, Rgb(0xD6, 0xC4, 0xA2), Rgb(234, 226, 210));
						Repaint(M, Rgb(0xE6, 0xD8, 0xBA), Rgb(234, 226, 210));
					}
					break;
				case KBoulder: Stands(0.5f, 0.5f, 0.85f, 0.0f, 200.0f); break;
				case KSangar: Stands(0.45f, 0.45f, 0.6f, 0.0f, 220.0f); break; // (a man kneels behind it)
				case KPalm:
				{
					// (its near level is five times an oak's: plain from 90 m, a crown from 400 m, as the bocage's trees)
					Stands(0.88f, 0.12f, 0.75f, 0.25f, 90.0f);
					M.bTree = true;
					// (its far level's fronds are thin strips that turn edge-on to a man's eye past a hundred metres and
					// leave a bare pole: a grove on the skyline read as a burnt forest. The mass of its crown inside them,
					// half the size of the game's own far crown, darker, and without its post, keeps a palm a palm.)
					const LandField::Mesh Ball = LandField::CrownOf(M.Far.Empty() ? M.Near : M.Far);
					if (Ball.Empty()) break;
					double Top = 0.0, CX = 0.0, CY = 0.0, CZ = 0.0, Lifted = 0.0;
					for (size_t P = 0; P < Ball.Points(); ++P) Top = std::max(Top, static_cast<double>(Ball.Positions[3 * P + 2]));
					for (size_t P = 0; P < Ball.Points(); ++P)
						if (Ball.Positions[3 * P + 2] >= 0.55 * Top)
						{
							CZ += Ball.Positions[3 * P + 2];
							Lifted += 1.0;
						}
					CZ = Lifted > 0.0 ? CZ / Lifted : Top;
					for (size_t P = 0; P < Ball.Points(); ++P)
					{
						CX += Ball.Positions[3 * P] / static_cast<double>(Ball.Points());
						CY += Ball.Positions[3 * P + 1] / static_cast<double>(Ball.Points());
					}
					LandField::Mesh& F = M.Far.Empty() ? M.Near : M.Far;
					const uint32_t Base0 = static_cast<uint32_t>(F.Points());
					for (size_t P = 0; P < Ball.Points(); ++P)
					{
						F.Positions.push_back(static_cast<float>(CX + 0.45 * (Ball.Positions[3 * P] - CX)));
						F.Positions.push_back(static_cast<float>(CY + 0.45 * (Ball.Positions[3 * P + 1] - CY)));
						F.Positions.push_back(static_cast<float>(CZ + 0.7 * (Ball.Positions[3 * P + 2] - CZ)));
						F.Normals.insert(F.Normals.end(), { Ball.Normals[3 * P], Ball.Normals[3 * P + 1], Ball.Normals[3 * P + 2] });
						const LandField::Rgb8 C = Ball.Colours[P];
						F.Colours.push_back(Byte(Tone{ 0.78 * C.R, 0.78 * C.G, 0.78 * C.B }));
					}
					if (!F.Uvs.empty()) F.Uvs.resize(2 * F.Points(), 0.0f);
					for (size_t T = 0; T + 2 < Ball.Triangles.size(); T += 3)
					{
						const uint32_t A = Ball.Triangles[T], B = Ball.Triangles[T + 1], C = Ball.Triangles[T + 2];
						const double Low = std::min({ Ball.Positions[3 * A + 2], Ball.Positions[3 * B + 2], Ball.Positions[3 * C + 2] });
						if (Low < 0.55 * Top) continue; // (not the post)
						F.Triangles.insert(F.Triangles.end(), { Base0 + A, Base0 + B, Base0 + C });
					}
					break;
				}
				case KThorn: M.FarFrom = 110.0f; break;
				case KPole: M.FarFrom = 200.0f; break;
				default: M.FarFrom = 90.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KHouse:
				case KHouseStone:
				case KHouseTwo:
				case KDome: W.Box(0.0, 0.0, 1.6, 4.0, 3.0, 1.6, 0.0, K == KHouseStone ? Rgb(206, 166, 112) : Rgb(240, 236, 226)); break;
				case KThorn: W.Rock(0.0, 0.0, 0.5, 0.45, 7, Rgb(132, 120, 80), Rgb(96, 88, 62)); break;
				case KBoulder:
				case KStones: W.Rock(0.0, 0.0, 0.9, 0.7, 11, Rgb(176, 150, 118), Rgb(120, 100, 80)); break;
				case KSangar: W.Box(0.0, 0.0, 0.4, 1.8, 0.4, 0.4, 0.0, Rgb(160, 140, 112)); break;
				default: W.Box(0.0, 0.0, 3.0, 0.12, 0.12, 3.0, 0.0, Rgb(110, 96, 80)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			Tone WallStone() const override { return Rgb(206, 194, 166); }
			double WallIvy() const override { return 0.0; }

			// ---------------------------------------------------------------- what stands on it

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0x51E5);
				int Built = 0;
				// The town: flat-roofed houses with their courtyards packed in blocks along a loose grid turned to the
				// town's own axis, a street every third house and every fourth row, the plots thinning toward its edge.
				const double Axis = R.Range(0.0, 0.5 * Pi);
				const double Ca = std::cos(Axis), Sa = std::sin(Axis);
				const double Step = 13.0, Reach = 0.95 * TR;
				const int N = static_cast<int>(Reach / Step) + 1;
				for (int J = -N; J <= N; ++J)
					for (int I = -N; I <= N; ++I)
					{
						if (((I % 3) + 3) % 3 == 2 || ((J % 4) + 4) % 4 == 3) continue; // (the streets)
						const double U = I * Step, V = J * Step;
						const double D = std::hypot(U, V) / Reach;
						const double Skip = R.Next(), Jx = R.Range(-1.2, 1.2), Jy = R.Range(-1.2, 1.2), Turn = R.Range(-0.06, 0.06), Kind = R.Next();
						if (D > 1.0 || Skip < 0.12 + 0.55 * D * D) continue;
						const double X = TX + U * Ca - V * Sa + Jx, Y = TY + U * Sa + V * Ca + Jy;
						const int Slot = Kind < 0.2 ? KHouseTwo : Kind < 0.36 ? KHouseStone : KHouse; // (one in six of mud brick)
						if (H.House(KitOf(Slot), X, Y, Axis + Turn + (Kind > 0.8 ? Pi : 0.0), 11.0, 11.0)) ++Built;
					}
				// A domed tomb (a marabout) on a rise outside the town: the plain's landmark.
				double BestX = 0.0, BestY = 0.0, BestZ = -1e9;
				for (int K = 0; K < 60; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(TR + 250.0, TR + 900.0);
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					if (X < 50.0 || Y < 50.0 || X > H.Width() - 50.0 || Y > H.Depth() - 50.0 || !H.Free(X, Y, 6.0)) continue;
					const double Z = H.Land(X, Y);
					if (Z > BestZ)
					{
						BestZ = Z;
						BestX = X;
						BestY = Y;
					}
				}
				const double DomeYaw = R.Range(0.0, 2.0 * Pi);
				if (BestZ > -1e8 && H.House(KitOf(KDome), BestX, BestY, DomeYaw, 8.0, 8.0)) ++Built;
				H.Tally(Built, 0, 0);
			}

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A47);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// (the kit's thorn is olive, as after the winter rains; from May it is straw-grey and grey-brown)
				const double ThornR = bSpring ? 0.98 : 1.13, ThornB = bSpring ? 1.0 : 1.07;
				// Camel thorn on its hummock: some 40 a hectare on the serir, a few hundred on the scrub flats and in the
				// wadis' floors; clumped by a slow noise, never continuous.
				const int Tries = static_cast<int>(W * D / 10000.0 * 320.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.65, 1.35), Tall = R.Range(0.75, 1.15), Grey = R.Range(0.92, 1.06);
					const Spot S = H.At(X, Y);
					if (S.Kind == KLane || S.Kind == KVillage || S.Kind == KYard || S.Kind == KWater) continue;
					const double Wd = Wadi(X, Y);
					const double Clump = 0.5 + 0.5 * Noise(X / 60.0, Y / 60.0, Seed ^ 0x7C);
					// (the art director's desert set: some 5 a hectare on the open plain, 6 on the rough, 8 under palms; the
					// scrub flats and the wadis' floors thicker, as the research has them)
					double Share = S.Kind == KRough ? 0.02 : S.Kind == KWoods ? 0.025 : S.Kind == KMarsh ? 0.008 : S.Crop == Scrub ? 0.2 : 0.016;
					Share = std::max(Share, 0.6 * Wd) * (0.35 + 1.3 * Clump * Clump);
					if (Keep >= Share || !H.Free(X, Y, 0.8)) continue;
					H.Plant(KitOf(KThorn), X, Y, Yaw, Size, Size * Tall, Tint(Grey * ThornR, Grey, Grey * ThornB));
				}
				// Loose stones and boulders, as the art director's desert set lays them: on the open plain some 22 patches of
				// stones a hectare and a boulder in two or three hectares; on the rough 60 and 14.
				const int Rocks = static_cast<int>(W * D / 10000.0 * 100.0);
				for (int K = 0; K < Rocks; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.4), Flat = R.Range(0.7, 1.1), Big = R.Range(0.7, 2.2);
					const Spot S = H.At(X, Y);
					const bool bRock = S.Kind == KRough;
					const bool bOpen = S.Kind == KOpen || S.Kind == KField;
					const double Stones = bRock ? 0.6 : bOpen ? 0.22 : 0.0, Boulders = Stones + (bRock ? 0.14 : bOpen ? 0.004 : 0.0);
					if (Keep >= Boulders) continue;
					const bool bBoulder = Keep >= Stones;
					if (!H.Free(X, Y, bBoulder ? 2.0 : 1.2)) continue;
					H.Plant(KitOf(bBoulder ? KBoulder : KStones), X, Y, Yaw, bBoulder ? Big : Size, (bBoulder ? Big : Size) * Flat, Tint(1.0, 1.0, 1.0));
				}
				// Sangars of piled stone where the rules dig: some 20 m apart, their open backs to the field's middle.
				{
					const double Cx = 0.5 * H.Width(), Cy = 0.5 * H.Depth();
					for (double Y = 10.0; Y < H.Depth(); Y += 24.0)
						for (double X = 10.0; X < H.Width(); X += 24.0)
						{
							const double Jx = R.Range(-6.0, 6.0), Jy = R.Range(-6.0, 6.0), Turn = R.Range(-0.5, 0.5);
							const double PX = X + Jx, PY = Y + Jy;
							if (H.At(PX, PY).Kind != KWorks || !H.Free(PX, PY, 2.5)) continue;
							H.Plant(KitOf(KSangar), PX, PY, std::atan2(Cy - PY, Cx - PX) + Pi + Turn, 1.0, 1.0, Tint(1.0, 1.0, 1.0));
						}
				}
				// Telegraph poles 55 m apart, 14 m off the longest track (the art director's desert set).
				size_t Longest = 0, Most = 0;
				for (size_t K = 0; K < H.Lanes(); ++K)
					if (H.LaneLine(K).size() > Most)
					{
						Most = H.LaneLine(K).size();
						Longest = K;
					}
				if (Most >= 2)
				{
					const std::vector<std::pair<double, double>>& L = H.LaneLine(Longest);
					const double Off = 14.0;
					double Since = 55.0;
					for (size_t K = 1; K < L.size(); ++K)
					{
						const double Dx = L[K].first - L[K - 1].first, Dy = L[K].second - L[K - 1].second, Len = std::hypot(Dx, Dy);
						Since += Len;
						if (Since < 55.0 || Len < 1e-6) continue;
						Since = 0.0;
						const double X = L[K].first - Dy / Len * Off, Y = L[K].second + Dx / Len * Off;
						const double Lean = R.Range(-0.04, 0.04), High = R.Range(0.95, 1.05);
						if (H.Free(X, Y, 0.4)) H.Plant(KitOf(KPole), X, Y, std::atan2(Dy, Dx) + Lean, 1.0, High, Tint(1.0, 1.0, 1.0));
					}
				}
				// Date palms: round the town's edge (some 14 a hectare in a ring 60 m deep), in the recording's woods as a palm
				// grove (95 a hectare), and in southern Tunisia an oasis garden by the town.
				double TX = 0.0, TY = 0.0, TR = 0.0;
				const bool bTown = H.Town(TX, TY, TR);
				if (bTown)
				{
					const int Edge = static_cast<int>(Pi * ((TR + 60.0) * (TR + 60.0) - TR * TR) / 10000.0 * 14.0);
					for (int K = 0; K < Edge; ++K)
					{
						const double A = R.Range(0.0, 2.0 * Pi), Dd = R.Range(TR, TR + 60.0), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.8, 1.2), Tall = R.Range(0.85, 1.15);
						const double X = TX + Dd * std::cos(A), Y = TY + Dd * std::sin(A);
						if (H.Free(X, Y, 3.0)) H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
				{
					const int Grove = static_cast<int>(W * D / 10000.0 * 95.0);
					for (int K = 0; K < Grove; ++K)
					{
						const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.8, 1.25), Tall = R.Range(0.85, 1.15);
						if (H.At(X, Y).Kind != KWoods || !H.Free(X, Y, 2.75)) continue;
						H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
				if (bPalms && bTown)
				{
					const double A = R.Range(0.0, 2.0 * Pi);
					const double GX = TX + (TR + 80.0) * std::cos(A), GY = TY + (TR + 80.0) * std::sin(A);
					for (int K = 0; K < 140; ++K)
					{
						const double X = GX + R.Range(-90.0, 90.0), Y = GY + R.Range(-90.0, 90.0), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.75, 1.15), Tall = R.Range(0.85, 1.15);
						if (H.Free(X, Y, 2.0)) H.Plant(KitOf(KPalm), X, Y, Yaw, Size, Size * Tall, Tint(1.0, 1.0, 1.0));
					}
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "woods" || Ground == "forest") return 5;
				if (Ground == "rough" || Ground == "rock") return 7; // (the rises' limestone and the escarpment: rock)
				return Else == 0 || Else == 4 ? 5 : Else;
			}
			const char* Earth() const override { return "sand"; }

		private:
			bool bSpring = false, bPalms = false;
			static constexpr double GridStep = 4.0;
			double GX0 = 0.0, GY0 = 0.0;
			int GNX = 0, GNY = 0;
			std::vector<float> Grid;
			static constexpr double CoarseStep = 12.0;
			double CX0 = 0.0, CY0 = 0.0;
			int CNX = 0, CNY = 0;
			std::vector<float> Coarse;
		};
	}

	std::unique_ptr<Region> MakeDesert(const LandField::Field& F, uint64_t S) { return std::make_unique<Desert>(F, S); }
}
