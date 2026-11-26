#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>

/**
 * The dry steppe east of the Volga: Mongolia and the Khalkhin Gol (Nomonhan, May to September 1939), the Kazakh steppe,
 * the Iranian plateau and the Horn. The production lead's theatre plan (production/polish-priorities-1122/theatres.md,
 * item 12): "Khalkhin Gol first, as a LandSteppe variant: short dry grass, no farms, dunes, the Khalkha bluffs and yurts".
 *
 *   - short bunch grass a hand or two high, grey-green in a wet August and straw-buff on the rises and in a dry year,
 *     with silvery wormwood in patches and feather grass's plumes where the ground is poorest; no fields, no hedges, no
 *     woods but willow scrub along the water;
 *   - sandy ground: grassed sand hills on the east bank (the dunes the Japanese held), their crests blown out to bare
 *     yellow sand, low hummocks where the grass holds the sand round the eye;
 *   - at Khalkhin Gol itself, the river's valley west of the field: a green floodplain with willows and the river in it,
 *     and beyond it the Mongolian bank's bluffs standing some 60 m over the east bank, their faces eroded sand;
 *   - herders' yurts in camps of two to five round the rules' town, a few low earth-roofed huts, an ovoo (a cairn of
 *     stones with a pole and blue cloth) on the highest rise; tracks of several wheel ruts side by side across the grass.
 *
 * Its kit is the Asia thread's battle models (Content/Models/Asia, from battle-models/asia): DryBoulder, DryOutcrop,
 * Willow and Reeds; the yurt, the ovoo and the hut it builds itself (Region::KitBuilt).
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Short = 0,   // short bunch grass
			Feather = 1, // feather grass on the poorest ground
			Worm = 2,    // wormwood
			Sandy = 3,   // a sand blow-out
			Meadow = 4   // a hay meadow in a hollow, greener and longer
		};

		enum Slot : int
		{
			KBoulder = 0,
			KOutcrop = 1,
			KWillow = 2,
			KReeds = 3,
			KYurt = 4, // (built: no file)
			KOvoo = 5, // (built)
			KHut = 6,  // (built)
			KShrub = 7 // pea-shrub, low, open and grey-green (built)
		};

		enum Clut : int
		{
			CTuft = 0,    // a short bunch-grass tuft
			CFeather = 1, // feather grass: fine silvery plumes leaning with the wind
			CWorm = 2,    // a wormwood cushion, silver-green
			CSward = 3,   // the sward: short bunch grass's fine blades, near-continuous low cover
			CHummock = 4, // a sand hummock held by a tuft
			CReed = 5     // a clump of reeds (round the eye only, as the frame budget asks of low growth)
		};

		LandField::Base KitOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Kit0) + K); }
		LandField::Base ClutOf(int K) { return static_cast<LandField::Base>(static_cast<int>(LandField::Base::Clut0) + K); }

		// (sRGB as painted, the art director's to check: the steppe's grass a step under the bocage's pastures in
		// green, greyer and more buff; the sand paler and warmer than the desert's serir)
		const Tone GrassGreen = Rgb(112, 120, 80);
		const Tone GrassGrey = Rgb(128, 130, 92);
		const Tone Buff = Rgb(156, 144, 100);
		const Tone Wormwood = Rgb(140, 146, 128);
		const Tone GreySand = Rgb(140, 134, 108);
		const Tone Awn = Rgb(192, 190, 172);
		const Tone Plume = Rgb(170, 166, 132);
		const Tone Sand = Rgb(196, 176, 132);
		const Tone SandDark = Rgb(166, 146, 106);
		const Tone Dust = Rgb(162, 148, 114);
		const Tone Rut = Rgb(124, 112, 88);
		const Tone Felt = Rgb(236, 232, 220);
		// (the south's summer ground: Iraq, Iran and the Horn burnt beige-ochre, the art director on 849c1bc1 asking some
		// 175,160,125 on the Mac for Baghdad in July, where the Mongolian summer's green read as a mint lawn)
		const Tone Ochre = Rgb(114, 96, 66);
		const Tone Straw = Rgb(132, 114, 80);

		/** A small stone of 15 triangles (as the desert's): a squat cone of five sides, its foot sunk. */
		void Chip(Writer& W, double CX, double CY, double R, double H, uint64_t S, const Tone& Lit, const Tone& Under)
		{
			const uint32_t Top = W.Vert(CX, CY, H, 0.0, 0.0, 1.0, Lit);
			uint32_t Up[5], Down[5];
			for (int K = 0; K < 5; ++K)
			{
				const double A = 2.0 * Pi * (K + 0.3 * Unit(Hash(S, 10 + K))) / 5.0;
				const double Push = 0.75 + 0.5 * Unit(Hash(S, 20 + K));
				const double X = std::cos(A), Y = std::sin(A);
				Up[K] = W.Vert(CX + R * Push * X, CY + R * Push * Y, 0.45 * H, X * 0.6, Y * 0.6, 0.8, Mixed(Under, Lit, 0.7));
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

		/** Points from First on lit as the sky lights the ground (normals up, leaning a little out): a thin blade's own
		 * normal, turned toward the eye from behind, faces down and draws it near black. */
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

		/** A tuft of Blades blades from a foot Spread across, High to High + Extra tall, leaning out Lean. */
		void Tuft(Writer& W, int Blades, double Spread, double High, double Extra, double Lean, double Width, uint64_t S, const Tone& Foot, const Tone& Tip)
		{
			for (int B = 0; B < Blades; ++B)
			{
				const double A = 2.0 * Pi * (B + 0.6 * Unit(Hash(S, 1, B))) / Blades;
				const double Out = Lean * (0.5 + Unit(Hash(S, 2, B))), Tall = High + Extra * Unit(Hash(S, 3, B));
				const double FX = Spread * std::cos(A) * Unit(Hash(S, 4, B)), FY = Spread * std::sin(A) * Unit(Hash(S, 4, B));
				W.Blade(FX, FY, 0.0, FX + Out * std::cos(A), FY + Out * std::sin(A), Tall, Width, Foot, Tip);
			}
		}

		/** A ring of Round points at radius R and height Z, normals out (and up by Up), one colour. */
		void Ring(Writer& W, int Round, double R, double Z, double Up, const Tone& C, uint32_t* Out)
		{
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round, X = std::cos(A), Y = std::sin(A);
				const double L = std::sqrt(1.0 + Up * Up);
				Out[K] = W.Vert(R * X, R * Y, Z, X / L, Y / L, Up / L, C);
			}
		}
		void Band(Writer& W, int Round, const uint32_t* Low, const uint32_t* High)
		{
			for (int K = 0; K < Round; ++K)
			{
				const int N = (K + 1) % Round;
				W.Tri(Low[K], Low[N], High[N]);
				W.Tri(Low[K], High[N], High[K]);
			}
		}

		class Dry final : public Region
		{
		public:
			Dry(const LandField::Field& F, uint64_t S) : Region(F, S)
			{
				// (the steppe's year: green from late May into August, the rains' month; buff from September; grey and
				// bleached in the long winter)
				const int M = F.Month;
				Green = (M >= 6 && M <= 8) ? 1.0 : (M == 5 || M == 9) ? 0.45 : 0.0;
				bWinter = M <= 3 || M >= 11;
				// (south of 40 north and west of the Tien Shan, Iraq, Iran and the Horn: the rains come in winter and spring,
				// and the summer burns the grass to straw on ochre ground; no bleached winter. The Horn's rains in April and
				// May, October and November)
				bSouth = F.Lat < 40.0 && F.Lon < 85.0;
				if (bSouth)
				{
					Green = F.Lat < 20.0 ? ((M == 4 || M == 5 || M == 10 || M == 11) ? 0.3 : 0.0)
						: (M >= 2 && M <= 4) ? 0.5 : (M == 1 || M == 5 || M == 12) ? 0.2 : 0.0;
					bWinter = false;
				}
				// (the Khalkhin Gol: the river and the Mongolian bank's bluffs west of the field)
				bKhalkha = F.Lat > 46.0 && F.Lat < 49.0 && F.Lon > 116.5 && F.Lon < 120.5;
				// (the herders' felt tents: Mongolia, Inner Mongolia and Dzungaria; the Kazakh and Iranian steppes have their
				// villages, which the rules' town draws as huts)
				bYurts = F.Lon > 85.0 && F.Lon < 125.0;
				bSandy = bYurts;
				// The sand and the wormwood read once onto a 12 m grid over the field and 1.2 km round it.
				CX0 = -1200.0;
				CY0 = -1200.0;
				CNX = static_cast<int>((F.Width() + 2400.0) / CoarseStep) + 2;
				CNY = static_cast<int>((F.Depth() + 2400.0) / CoarseStep) + 2;
				Coarse.resize(4 * static_cast<size_t>(CNX) * static_cast<size_t>(CNY));
				for (int J = 0; J < CNY; ++J)
					for (int I = 0; I < CNX; ++I)
					{
						const size_t K = 4 * (static_cast<size_t>(J) * static_cast<size_t>(CNX) + static_cast<size_t>(I));
						Coarse[K] = static_cast<float>(WormNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
						Coarse[K + 1] = static_cast<float>(SandNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
						Coarse[K + 2] = static_cast<float>(PatchNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
						Coarse[K + 3] = static_cast<float>(BareNoise(CX0 + I * CoarseStep, CY0 + J * CoarseStep));
					}
			}
			Look Which() const override { return Look::Dry; }

			// ---------------------------------------------------------------- the land

			double Outside(double X, double Y) const
			{
				const double Dx = std::max({ 0.0, -X, X - Fd.Width() }), Dy = std::max({ 0.0, -Y, Y - Fd.Depth() });
				return std::sqrt(Dx * Dx + Dy * Dy);
			}
			/** Wormwood's patches, 0 to 1: soft shapes tens of metres across (a noise bent by another, so no square cells). */
			double WormNoise(double X, double Y) const
			{
				const double WX = 40.0 * Noise(X / 120.0, Y / 120.0, Seed ^ 0xD1), WY = 40.0 * Noise(X / 120.0 + 3.7, Y / 120.0 - 2.2, Seed ^ 0xD2);
				const double U = 0.8 * (X + WX) - 0.6 * (Y + WY), V = 0.6 * (X + WX) + 0.8 * (Y + WY);
				return Smooth(0.0, 0.35, Fbm(U, V, 180.0, 3, Seed ^ 0xD3));
			}
			/** Sandy ground, 0 to 1: where the grass is thin on the sand, a few hundred metres across; on the Mongolian
			 * steppe much of it, elsewhere a little. */
			double SandNoise(double X, double Y) const
			{
				const double WX = 90.0 * Noise(X / 360.0, Y / 360.0, Seed ^ 0xD4), WY = 90.0 * Noise(X / 360.0 - 2.7, Y / 360.0 + 3.1, Seed ^ 0xD5);
				const double N = Fbm(0.8 * (X + WX) - 0.6 * (Y + WY), 0.6 * (X + WX) + 0.8 * (Y + WY), 300.0, 3, Seed ^ 0xD6);
				return bSandy ? Smooth(-0.05, 0.4, N) : 0.4 * Smooth(0.2, 0.5, N);
			}
			/** The grass's own patches, -1 to 1: soft shapes some 40 to 150 m across, bent and turned (no square cells),
			 * that the feather grass, the blow-outs and the parcels' own ground follow instead of the parcels' edges. */
			double PatchNoise(double X, double Y) const
			{
				const double WX = 60.0 * Noise(X / 180.0, Y / 180.0, Seed ^ 0xDA), WY = 60.0 * Noise(X / 180.0 - 1.9, Y / 180.0 + 4.3, Seed ^ 0xDB);
				const double U = 0.6 * (X + WX) + 0.8 * (Y + WY), V = -0.8 * (X + WX) + 0.6 * (Y + WY);
				return std::clamp(1.6 * Fbm(U, V, 150.0, 3, Seed ^ 0xDC), -1.0, 1.0);
			}
			/** The sward's thin places, -1 to 1: bare grey sand between the bunch grass in patches some 20 to 60 m across
			 * (bent, so no square cells); the tufts drift away from them. */
			double BareNoise(double X, double Y) const
			{
				const double WX = 20.0 * Noise(X / 70.0, Y / 70.0, Seed ^ 0xDD), WY = 20.0 * Noise(X / 70.0 + 2.3, Y / 70.0 - 1.1, Seed ^ 0xDE);
				const double U = 0.9 * (X + WX) - 0.43 * (Y + WY), V = 0.43 * (X + WX) + 0.9 * (Y + WY);
				return std::clamp(1.7 * Fbm(U, V, 40.0, 2, Seed ^ 0xDF), -1.0, 1.0);
			}
			double Read(double X, double Y, int Ch) const
			{
				const double U = (X - CX0) / CoarseStep, V = (Y - CY0) / CoarseStep;
				if (!(U >= 0.0 && V >= 0.0 && U < CNX - 1.0 && V < CNY - 1.0)) return Ch == 1 ? WormNoise(X, Y) : Ch == 2 ? SandNoise(X, Y) : Ch == 3 ? PatchNoise(X, Y) : BareNoise(X, Y);
				const int I = static_cast<int>(U), J = static_cast<int>(V);
				const double FU = U - I, FV = V - J;
				const size_t K = 4 * (static_cast<size_t>(J) * static_cast<size_t>(CNX) + static_cast<size_t>(I)) + static_cast<size_t>(Ch - 1);
				const size_t Row = 4 * static_cast<size_t>(CNX);
				const double A = Coarse[K], B = Coarse[K + 4], C = Coarse[K + Row], D = Coarse[K + Row + 4];
				return (A + (B - A) * FU) + ((C + (D - C) * FU) - (A + (B - A) * FU)) * FV;
			}

			double Relief(double X, double Y, double Z) const override
			{
				// (long low swells, and on the sand the grass's hummocks a hand or two high; under half a metre inside the
				// field, where the fight reads its own heights)
				const double Hum = bSandy ? 0.18 * Read(X, Y, 2) * Fbm(X, Y, 9.0, 1, Seed ^ 0xD7) : 0.0;
				return Z + 0.35 * Fbm(X, Y, 280.0, 2, Seed ^ 0xD8) + Hum;
			}
			/** The Khalkhin Gol's line, metres east of the field's west edge (negative: west of it), at Y. */
			double River(double Y) const { return -1150.0 + 160.0 * Noise(Y / 1400.0, 0.5, Seed ^ 0xE1) + 40.0 * Noise(Y / 300.0, 1.5, Seed ^ 0xE2); }
			/** The sand hills beyond the field: grassed dunes in belts, some 8 to 20 m. */
			double Dunes(double X, double Y) const
			{
				if (!bSandy) return 0.0;
				const double Belt = Smooth(-0.05, 0.35, Fbm(X, Y, 2400.0, 2, Seed ^ 0xE3));
				const double Hill = std::max(0.0, Fbm(0.9 * X + 0.3 * Y, 1.1 * Y - 0.2 * X, 240.0, 2, Seed ^ 0xE4) + 0.12);
				return Belt * 22.0 * Hill * Smooth(40.0, 400.0, Outside(X, Y));
			}
			/** A gully down the Khalkha bluffs at Y, 0 to 1 (some every 60 to 150 m). */
			double Gully(double Y) const { return Smooth(0.3, 0.65, Noise(Y / 70.0, 7.5, Seed ^ 0xEA)); }
			double Rolling(double X, double Y, double MeanZ) const override
			{
				double Z = MeanZ + 9.0 * Fbm(X, Y, 3400.0, 3, Seed ^ 0xB1) + 2.0 * Fbm(X, Y, 800.0, 2, Seed ^ 0xB2) + Dunes(X, Y);
				if (bKhalkha)
				{
					// (the east bank sinking some 12 m to the floodplain; the river in it; the Mongolian bank's bluffs rising
					// 50 to 70 m west of it over some 150 m, gullied, and the plateau beyond)
					const double D = X - River(Y);
					const double High = 62.0 * (1.0 + 0.18 * Noise(Y / 420.0, 3.5, Seed ^ 0xE5) + 0.08 * Noise(Y / 130.0, 5.5, Seed ^ 0xE9));
					Z += -12.0 * Smooth(700.0, 240.0, D) + High * Smooth(-230.0, -390.0, D) - 2.0 * Smooth(30.0, 10.0, std::fabs(D));
					// (gullies cut back into the bluffs' lip, breaking its line)
					Z -= 16.0 * Gully(Y) * Smooth(-240.0, -320.0, D) * Smooth(-480.0, -400.0, D);
					Z -= Dunes(X, Y) * Smooth(800.0, 300.0, D); // (no sand hills on the floodplain or the bluffs)
				}
				return Z;
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.0; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.2 : 2.6) + 1.6 * U; }
			double PlotMin() const override { return 90000.0; }
			double PlotMax() const override { return 220000.0; }
			double FarTilled() const override { return 0.0; }
			double WoodsEdge() const override { return 0.9; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Sandy;
				if (Kind == KField) return Meadow;
				return U < 0.65 ? Short : U < 0.85 ? Feather : Worm;
			}
			const char* CropWord(int C) const override
			{
				switch (C)
				{
				case Short: return "short grass";
				case Feather: return "feather grass";
				case Worm: return "wormwood";
				case Sandy: return "sand";
				case Meadow: return "meadow";
				default: return "";
				}
			}

			// ---------------------------------------------------------------- the paint

			/** The steppe by its land: grey-green grass, greener in the hollows and the wet month, buff on the rises and the
			 * sand, wormwood silvering it in patches, feather grass paler on the poorest ground, the sand showing through and
			 * blown bare in places. All of it from the land's own noises: the parcels' crops only lean on it, fading in some
			 * 40 m from their edges, so the steppe shows no field lines. No noise finer than some 30 m (the ground's points
			 * lie 4 m apart). Meadow, Rough: how much of the parcel's own (a hay meadow, a blow-out) is here, 0 to 1. */
			Tone Grass(double X, double Y, double Broad, double Meadow_, double Rough) const
			{
				const double Wm = Read(X, Y, 1), Sd = Read(X, Y, 2), Pt = Read(X, Y, 3);
				const double Wet = Clamp01(0.5 - 0.6 * Broad + 0.25 * Pt);
				Tone T = Mixed(GrassGrey, GrassGreen, Green * (0.35 + 0.65 * Wet));
				T = Mixed(T, Buff, (1.0 - 0.7 * Green) * (0.35 + 0.4 * Smooth(-0.2, 0.5, Broad)) + 0.12 * Sd);
				if (bWinter) T = Mixed(T, Rgb(170, 160, 128), 0.6);
				T = Mixed(T, GrassGreen, (0.15 + 0.15 * Green) * Meadow_);
				T = Mixed(T, Wormwood, 0.3 * Wm);
				// (bare grey sand between the bunch grass where the sward is thin)
				const double Bn = Read(X, Y, 4);
				T = Mixed(T, GreySand, 0.35 * Smooth(0.15, 0.7, Bn) * (1.0 - Meadow_));
				// (and the sward's own drifts between, olive where it grows thick: the ground's breakup at 10 to 40 m, which
				// the camera sees from above when the clutter has dropped out)
				T = Mixed(T, Rgb(98, 106, 72), 0.3 * Smooth(-0.1, -0.65, Bn));
				T = Mixed(T, Plume, 0.15 * Smooth(0.2, 0.7, Pt) * (1.0 - Meadow_));
				// (the south's burnt summer over it all but the hay meadows: ochre ground, straw in drifts)
				if (bSouth) T = Mixed(T, Mixed(Ochre, Straw, 0.3 + 0.3 * Pt), 0.9 * (1.0 - 0.7 * Green) * (1.0 - 0.5 * Meadow_));
				// (the sand: through the thin grass on it, and bare where the wind has blown it out, in blow-outs tens of
				// metres across on the sandiest ground)
				const double Bare = std::max(0.85 * Rough, BlownOut(Sd, Pt));
				T = Mixed(T, Mixed(SandDark, Sand, 0.5 + 0.4 * Pt), 0.12 * Sd + 0.8 * Bare);
				// (the grass's own light and dark: the patches a man sees running away from him)
				return Varied(T, 0.97 - 0.05 * Sd + 0.04 * Broad + 0.04 * Pt + 0.06 * Bn, 0.01 * Broad);
			}
			static double BlownOut(double Sd, double Pt) { return Smooth(0.6, 0.95, Sd) * Smooth(0.25, 0.7, -Pt); }
			/** How far a parcel's own ground reaches here: 0 at its edge to 1 some Reach metres in, its line wavering with
			 * the patches. */
			double Inside(const Spot& S, double Reach = 40.0) const { return Smooth(0.0, Reach, S.Edge + 0.5 * Reach * Read(S.X, S.Y, 3)); }
			/** The rules' rough ground's sand: blown out in patches inside the parcel, never the parcel's own shape. */
			double RoughSand(const Spot& S) const { return Inside(S) * Smooth(-0.3, 0.4, -Read(S.X, S.Y, 3)); }
			Tone Open(const Spot& S) const
			{
				// (beyond the band the parcels' edges are not measured: there the land's own noises alone, no parcel's sand)
				if (!S.bLaid) return Grass(S.X, S.Y, S.Broad, 0.0, 0.0);
				return Grass(S.X, S.Y, S.Broad, S.Kind == KField || S.Crop == Meadow ? Inside(S) : 0.0, S.Kind == KRough || S.Crop == Sandy ? RoughSand(S) : 0.0);
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a steppe track: several wheel ruts side by side in the grass, worn to dusty sand, grass between them
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Phase = Off / 0.9 + 0.5 * Noise(S.X / 45.0, S.Y / 45.0, Seed ^ 0xF7);
					const double Line = 2.0 * std::fabs(Phase - std::floor(Phase) - 0.5);
					const double RutK = Smooth(0.55, 0.9, Line) * Smooth(S.LaneHalf + 0.4, S.LaneHalf - 0.8, Off);
					const Tone G = Grass(S.X, S.Y, S.Broad, 0.0, 0.0);
					Tone T = Mixed(G, Dust, 0.35 * Smooth(S.LaneHalf + 1.0, 0.3 * S.LaneHalf, Off));
					return Mixed(T, Mixed(Dust, Rut, 0.4), 0.75 * RutK);
				}
				case KVillage:
				case KYard: return Mixed(Grass(S.X, S.Y, S.Broad, 0.0, 0.0), Dust, 0.35 * Smooth(-0.3, 0.4, Read(S.X, S.Y, 3))); // (trampled in patches round the tents, no edge to the plot)
				case KWater: return Rgb(74, 92, 98);
				case KMarsh: return Mixed(Grass(S.X, S.Y, S.Broad, 1.0, 0.0), Rgb(108, 120, 72), 0.8 * Inside(S, 20.0));
				case KWorks: return Mixed(Grass(S.X, S.Y, S.Broad, 0.0, 0.0), Mixed(Sand, SandDark, 0.5), 0.5 * Inside(S, 25.0) * Smooth(-0.5, 0.3, Read(S.X, S.Y, 4))); // (sand thrown up from the pits, in heaps)
				case KWoods: return Mixed(Grass(S.X, S.Y, S.Broad, 0.4, 0.0), Rgb(118, 120, 88), 0.3 * Inside(S, 25.0)); // (litter under the pea-shrub)
				default: break;
				}
				return Open(S);
			}

			Tone Far(const Spot& S, bool /*bWoods*/, double /*Gap*/) const override
			{
				Tone T = S.Kind == KWoods ? Colour(S) : Open(S);
				// (the sand hills: their crests and windward faces blown out to bare sand)
				const double Dn = Dunes(S.X, S.Y);
				if (Dn > 0.5) T = Mixed(T, Mixed(SandDark, Sand, 0.6), Smooth(6.0, 16.0, Dn) * Smooth(-0.3, 0.3, Read(S.X, S.Y, 3)));
				if (bKhalkha)
				{
					const double D = S.X - River(S.Y);
					// the floodplain: a green meadow, willow scrub dark along the river, the river grey-blue
					const double Flood = Smooth(320.0, 200.0, D) * Smooth(-260.0, -200.0, D);
					const Tone Plain = Mixed(Rgb(104, 120, 66), Rgb(64, 80, 46), Smooth(0.0, 0.5, Noise(S.X / 60.0, S.Y / 60.0, Seed ^ 0xE6)) * Smooth(140.0, 40.0, std::fabs(D)));
					T = Mixed(T, Plain, Flood);
					T = Mixed(T, Rgb(70, 90, 102), Smooth(26.0, 16.0, std::fabs(D)));
					// the bluffs' faces: grassed grey-green, darker than the plain (steep, the grass thin and shadowed), pale
					// sand scars only in places, the gullies dark down them; the commanding ground on the west horizon
					const double Face = Smooth(-215.0, -250.0, D) * Smooth(-430.0, -380.0, D);
					const double Scar = Smooth(0.45, 0.75, Noise(S.Y / 90.0, D / 60.0, Seed ^ 0xE7));
					const double Cut = std::max(Gully(S.Y), 0.6 * Smooth(0.2, 0.6, Noise(S.Y / 28.0, D / 40.0, Seed ^ 0xE8)));
					Tone Bank = Mixed(Rgb(82, 88, 64), Rgb(176, 152, 110), 0.8 * Scar);
					Bank = Mixed(Bank, Rgb(46, 44, 38), 0.85 * Cut);
					// (the foot band in its own shadow and scree, darkest)
					Bank = Mixed(Bank, Rgb(58, 58, 46), 0.6 * Smooth(-215.0, -245.0, D) * Smooth(-300.0, -265.0, D));
					T = Mixed(T, Bank, 0.95 * Face);
				}
				return T;
			}

			// ---------------------------------------------------------------- round the eye

			Grown Grow(const Spot& S, double Pick) const override
			{
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/ || S.Drawn == 10 /*wall*/) return Grown{};
				if (S.Kind == KLane) return S.Lane > -0.5 ? Grown{ ClutOf(CTuft), 0.5 } : Pick < 0.3 ? Grown{ ClutOf(CTuft), 0.12 } : Grown{};
				if (S.Kind == KVillage || S.Kind == KYard) return Pick < 0.5 ? Grown{ ClutOf(CTuft), 0.3 } : Grown{ ClutOf(CSward), 0.8 };
				if (S.Kind == KWorks) return Pick < 0.5 ? Grown{ ClutOf(CTuft), 0.15 } : Grown{};
				const double Sd = Read(S.X, S.Y, 2), Wm = Read(S.X, S.Y, 1), Pt = Read(S.X, S.Y, 3);
				const double Bare = std::max(S.Kind == KRough || S.Crop == Sandy ? 0.85 * RoughSand(S) : 0.0, BlownOut(Sd, Pt));
				if (Bare > 0.5)
				{
					if (Pick < 0.35) return Grown{ ClutOf(CHummock), 0.2 };
					return Pick < 0.7 ? Grown{ ClutOf(CTuft), 0.1 + 0.5 * (1.0 - Bare) } : Grown{ ClutOf(CSward), 0.6 * (1.0 - Bare) };
				}
				if (S.Kind == KMarsh) return Pick < 0.7 ? Grown{ ClutOf(CReed), 1.2 * Inside(S, 10.0) } : Grown{ ClutOf(CTuft), 1.5 };
				if (S.Kind == KWoods || S.Kind == KField) return Pick < 0.5 ? Grown{ ClutOf(CTuft), 1.6 } : Pick < 0.88 ? Grown{ ClutOf(CSward), 1.6 } : Grown{ ClutOf(CWorm), 0.3 };
				// the open steppe: the sward near-continuous, its bunch-grass tufts in drifts away from the thin places, bare
				// grey sand between; wormwood's silver cushions in their patches, feather grass on the poorest ground,
				// hummocks where the sand is
				const double Plumed = Smooth(0.2, 0.7, Pt), Thin = Smooth(0.15, 0.7, Read(S.X, S.Y, 4));
				const double Drift = 0.3 + Smooth(0.4, -0.4, Read(S.X, S.Y, 4));
				// (the south in its dry months: the sward and the feather grass sparse, the ground between them showing)
				const double Dry_ = bSouth ? 1.0 - Green : 0.0;
				if (Pick < 0.36) return Grown{ ClutOf(CTuft), 1.8 * Drift * (1.0 - 0.3 * Sd) * (1.0 - 0.3 * Plumed) * (1.0 - 0.5 * Dry_) };
				if (Pick < 0.78) return Grown{ ClutOf(CSward), 2.0 * (1.0 - 0.7 * Thin) * (1.0 - 0.2 * Sd) * (1.0 - 0.75 * Dry_) };
				if (Pick < 0.87) return Grown{ ClutOf(CWorm), 0.15 + 1.2 * Wm };
				if (Pick < 0.97) return Grown{ ClutOf(CFeather), (0.2 + 1.4 * Plumed) * (1.0 - 0.85 * Dry_) };
				return Grown{ ClutOf(CHummock), 0.3 * Smooth(0.4, 0.9, Sd) };
			}

			bool Dress(LandField::Placed& P, const Spot& /*S*/, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				// (the year's colour on the clutter: greener in the wet month, straw from September, bleached in winter)
				const double G = Green * (0.6 + 0.4 * U6);
				switch (K)
				{
				case CTuft:
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.7 * U5);
					P.Tint = bWinter ? Tint(1.08, 1.04, 0.98) : Tint(1.0 - 0.1 * G + 0.05 * U7, 1.0 + 0.04 * G, 0.95 - 0.08 * G);
					// (the south's dry tufts: half straw, half grey-green; their pale tips read as paper in the high sun)
					if (bSouth && Green < 0.25) P.Tint = U7 < 0.5 ? Tint(0.9 + 0.06 * U6, 0.76 + 0.05 * U6, 0.54) : Tint(0.74, 0.8, 0.68);
					return true;
				case CFeather:
					P.Scale = static_cast<float>(0.8 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Yaw = static_cast<float>(0.25 * Pi + 0.5 * (U7 - 0.5)); // (all leaning with the north-west wind)
					P.Tint = bSouth ? Tint(0.82 + 0.06 * U6, 0.68 + 0.05 * U6, 0.46) : Tint(0.97 + 0.06 * U6, 0.97 + 0.05 * U6, 0.95);
					return true;
				case CWorm:
					P.Scale = static_cast<float>(0.7 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.5 * U5);
					P.Tint = bSouth ? Tint(0.84 + 0.06 * U7, 0.88 + 0.05 * U7, 0.8) : Tint(0.96 + 0.08 * U7, 0.98 + 0.06 * U7, 0.98); // (the south's a dustier grey-green)
					return true;
				case CHummock:
					P.Scale = static_cast<float>(0.7 + 0.8 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Yaw = static_cast<float>(-0.25 * Pi + 0.6 * (U6 - 0.5));
					P.Tint = Tint(1.0, 1.0 - 0.03 * G, 0.98);
					return true;
				case CSward:
					P.Scale = static_cast<float>(0.8 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.6 + 0.8 * U5);
					P.Tint = bWinter ? Tint(1.08, 1.04, 0.98) : Tint(1.0 - 0.06 * G + 0.04 * U7, 1.0 + 0.03 * G, 0.98 - 0.05 * G);
					if (bSouth && Green < 0.25) P.Tint = Tint(0.9 + 0.04 * U7, 0.78 + 0.04 * U7, 0.58);
					return true;
				case CReed:
					P.Scale = static_cast<float>(0.8 + 0.5 * U4);
					P.ScaleZ = static_cast<float>(0.7 + 0.6 * U5);
					P.Tint = bWinter ? Tint(1.1, 1.02, 0.9) : Tint(1.0 - 0.06 * G, 1.0, 0.96);
					return true;
				default: return false;
				}
			}

			void ClutterMesh(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case CTuft:
					// a bunch-grass tuft a hand or two high: twenty-odd blades, grey-green at the foot, paler at the tips
					Tuft(W, 28, 0.09, 0.1, 0.16, 0.08, 0.018, 0x7F01, Rgb(118, 110, 86), Rgb(170, 166, 118)); // (dead thatch at the foot)
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 45.0f;
					break;
				case CFeather:
				{
					// feather grass: a low tuft and a dozen fine awns arching out downwind, silver at their tips
					Tuft(W, 10, 0.04, 0.08, 0.06, 0.04, 0.014, 0x7F02, Rgb(116, 110, 84), Rgb(156, 154, 110));
					for (int B = 0; B < 12; ++B)
					{
						const double A = 0.6 * (Unit(Hash(0x7F03, B)) - 0.5), Reach = 0.18 + 0.16 * Unit(Hash(0x7F04, B));
						const double High = 0.3 + 0.2 * Unit(Hash(0x7F05, B)), Side = 0.06 * (Unit(Hash(0x7F06, B)) - 0.5);
						W.Blade(0.0, Side, 0.02, Reach * std::cos(A), Side + Reach * std::sin(A), High, 0.008, Rgb(156, 154, 124), Awn);
					}
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 55.0f;
					break;
				}
				case CWorm:
				{
					// wormwood: a low rounded cushion of silver-green leaves, a few stems standing out of it
					Chip(W, 0.0, 0.0, 0.12, 0.05, 0x7F07, Rgb(134, 138, 124), Rgb(98, 102, 90));
					Tuft(W, 18, 0.1, 0.06, 0.06, 0.1, 0.024, 0x7F08, Rgb(112, 116, 102), Rgb(150, 154, 140));
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 50.0f;
					break;
				}
				case CSward:
					// the sward: thirty-odd fine blades a few fingers high over half a metre, grey-green, buff at the tips
					Tuft(W, 40, 0.36, 0.05, 0.09, 0.06, 0.014, 0x7F09, Rgb(104, 108, 78), Rgb(172, 162, 118));
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 32.0f;
					break;
				case CHummock:
				{
					// a hummock of sand the grass has caught, a hand high and a metre across, a tuft on its crown
					Chip(W, 0.0, 0.0, 0.4, 0.12, 0x7F0D, Mixed(SandDark, GrassGrey, 0.55), Mixed(SandDark, Rut, 0.5));
					const size_t Grass = M.Near.Points();
					Tuft(W, 16, 0.12, 0.2, 0.14, 0.07, 0.016, 0x7F0E, Rgb(100, 104, 66), Rgb(172, 166, 116));
					for (size_t P = Grass; P < M.Near.Points(); ++P) M.Near.Positions[3 * P + 2] += 0.12f;
					LitUp(M.Near, Grass);
					M.Paint = 2;
					M.FarFrom = 70.0f;
					break;
				}
				case CReed:
					// a clump of reeds a man high: thirty-odd stems, green at the foot, straw at the tips, a few heads
					Tuft(W, 32, 0.25, 1.3, 0.8, 0.18, 0.02, 0x7F10, Rgb(88, 100, 60), Rgb(176, 164, 112));
					LitUp(M.Near, 0);
					M.Paint = 2;
					M.FarFrom = 60.0f;
					break;
				default: break;
				}
			}

			// ---------------------------------------------------------------- the kit

			const char* Folder() const override { return "Asia"; }
			const char* KitName(int K) const override
			{
				switch (K)
				{
				case KBoulder: return "DryBoulder";
				case KOutcrop: return "DryOutcrop";
				case KWillow: return nullptr; // (the Asia set's willow reads as a savanna tree on the open steppe: the scrub is shrubs)
				case KYurt: return bYurts ? "Yurt" : nullptr;
				case KOvoo: return bYurts ? "Ovoo" : nullptr;
				case KHut: return "SteppeHut";
				case KShrub: return "PeaShrub";
				default: return nullptr;
				}
			}
			bool KitBuilt(int K) const override { return K == KYurt || K == KOvoo || K == KHut || K == KShrub; }
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
				case KBoulder: Stands(0.5f, 0.5f, 0.85f, 0.0f, 150.0f); break;
				case KOutcrop: Stands(0.5f, 0.5f, 0.8f, 0.0f, 250.0f); break;
				case KWillow:
					// (a heavy near level: plain from 70 m, a crown from 400 m)
					Stands(0.6f, 0.4f, 0.8f, 0.3f, 70.0f);
					M.bTree = true;
					break;
				case KReeds: M.FarFrom = 60.0f; break;
				case KYurt:
				case KHut: Stands(0.5f, 0.5f, 0.85f, 0.0f, 400.0f); break;
				case KShrub:
					M.Paint = 1;
					M.FarFrom = 120.0f;
					break;
				case KOvoo: Stands(0.45f, 0.45f, 0.6f, 0.0f, 300.0f); break;
				default: M.FarFrom = 90.0f; break;
				}
			}
			void KitPlain(int K, LandField::BaseMesh& M) const override
			{
				M.Near = LandField::Mesh();
				Writer W(M.Near);
				switch (K)
				{
				case KYurt:
				{
					// a ger: a felt drum 6.4 m across and 1.6 m high round its lattice wall, a shallow felt cone over it to
					// the crown ring at 2.8 m, the ropes round it dark bands, a painted door to the south, the crown's
					// smoke hole dark
					constexpr int Round = 16;
					uint32_t A[Round], B[Round], C[Round], D[Round], E[Round], F[Round];
					const Tone Wall = Mixed(Felt, Rgb(200, 192, 176), 0.25), RopeC = Rgb(120, 104, 84);
					Ring(W, Round, 3.25, -0.05, 0.0, Wall, A);
					Ring(W, Round, 3.2, 0.62, 0.0, Wall, B);
					Ring(W, Round, 3.2, 0.7, 0.0, RopeC, C);
					Ring(W, Round, 3.2, 1.58, 0.0, Wall, D);
					Band(W, Round, A, B);
					Band(W, Round, B, C);
					Band(W, Round, C, D);
					// the roof: from the wall's head out a hand, up to the crown ring
					Ring(W, Round, 3.32, 1.55, 0.35, Felt, E);
					Ring(W, Round, 0.62, 2.62, 0.35, Mixed(Felt, Rgb(180, 172, 156), 0.3), F);
					Band(W, Round, E, F);
					// the crown: a dark ring and its cap
					uint32_t G[Round];
					Ring(W, Round, 0.5, 2.78, 0.6, Rgb(150, 112, 70), G);
					Band(W, Round, F, G);
					const uint32_t Top = W.Vert(0.0, 0.0, 2.84, 0.0, 0.0, 1.0, Rgb(72, 62, 52));
					for (int Q = 0; Q < Round; ++Q) W.Tri(G[Q], G[(Q + 1) % Round], Top);
					// the door: south (-Y), painted orange-red in its frame
					W.Box(0.0, -3.24, 0.66, 0.48, 0.08, 0.66, 0.0, Rgb(176, 78, 42));
					W.Box(0.0, -3.27, 1.38, 0.56, 0.06, 0.07, 0.0, Rgb(150, 64, 36));
					M.Far = M.Near;
					break;
				}
				case KOvoo:
				{
					// an ovoo: a cone of piled stones, a birch pole standing out of it, scraps of blue silk on the pole
					W.Rock(0.0, 0.0, 1.5, 1.4, 0x0F00, Rgb(160, 152, 140), Rgb(102, 96, 88));
					W.Rock(0.9, 0.4, 0.5, 0.4, 0x0F01, Rgb(150, 142, 128), Rgb(98, 92, 84));
					W.Rock(-0.8, -0.6, 0.45, 0.35, 0x0F02, Rgb(166, 158, 144), Rgb(104, 98, 90));
					W.Box(0.0, 0.0, 2.3, 0.05, 0.05, 1.0, 0.3, Rgb(196, 190, 176));
					for (int Q = 0; Q < 5; ++Q)
					{
						const double Z = 2.4 + 0.18 * Q, Yaw = 1.3 * Q;
						W.Box(0.12 * std::cos(Yaw), 0.12 * std::sin(Yaw), Z, 0.12, 0.015, 0.05, Yaw, Q % 2 ? Rgb(64, 110, 186) : Rgb(96, 140, 200));
					}
					M.Far = M.Near;
					break;
				}
				case KHut:
					// a low hut of turf and logs, its walls mud-plastered, its flat roof of earth a hand over them, a dark door
					W.Box(0.0, 0.0, 1.1, 3.0, 2.2, 1.1, 0.0, Rgb(178, 160, 126));
					W.Box(0.0, 0.0, 2.3, 3.25, 2.45, 0.12, 0.0, Rgb(132, 120, 92), true);
					W.Box(0.0, -2.22, 0.85, 0.5, 0.04, 0.85, 0.0, Rgb(70, 58, 46));
					M.Far = M.Near;
					break;
				case KShrub:
					// pea-shrub: a loose spread of small grey-green leaf clumps on thin stems, wider than tall (about 2 m by
					// 1), light and sky between them
					for (int B = 0; B < 11; ++B)
					{
						const double A = 2.0 * Pi * (B + 0.5 * Unit(Hash(0x0F10, B))) / 11.0, D = B == 0 ? 0.0 : 0.25 + 0.7 * Unit(Hash(0x0F11, B));
						const double X = D * std::cos(A), Y = D * std::sin(A), Up = 0.35 + 0.55 * (1.0 - D) * Unit(Hash(0x0F12, B)), R = 0.16 + 0.12 * Unit(Hash(0x0F13, B));
						W.Box(0.5 * X, 0.5 * Y, 0.5 * Up, 0.015, 0.015, 0.5 * Up, A, Rgb(96, 88, 72));
						Chip(W, X, Y, R, 0.3, Hash(0x0F14, B), Rgb(136, 142, 108), Rgb(94, 100, 76));
						// (the clump lifted onto its stem)
						const size_t Last = M.Near.Points();
						for (size_t P = Last - 11; P < Last; ++P) M.Near.Positions[3 * P + 2] += static_cast<float>(Up);
					}
					M.Far = M.Near;
					break;
				case KWillow:
					W.Box(0.0, 0.0, 1.5, 0.2, 0.2, 1.5, 0.0, Rgb(94, 86, 74));
					W.Rock(0.0, 0.0, 3.0, 5.5, 0x0F03, Rgb(106, 126, 68), Rgb(60, 76, 44));
					M.Far = M.Near;
					break;
				case KReeds: W.Rock(0.0, 0.0, 1.2, 2.2, 0x0F04, Rgb(150, 148, 96), Rgb(110, 112, 70)); M.Far = M.Near; break;
				default: W.Rock(0.0, 0.0, 0.9, 0.8, 0x0F05, Rgb(164, 134, 104), Rgb(126, 102, 80)); M.Far = M.Near; break;
				}
				M.FarFrom = 400.0f;
			}

			// ---------------------------------------------------------------- what stands on it

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0xD5E5);
				int Built = 0;
				// A few low huts at the town's middle (a post, a store, a herders' winter quarters), facing south.
				const int Huts = bYurts ? 4 : 14;
				for (int K = 0; K < Huts * 4 && Built < Huts; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.0, 0.6 * TR), Turn = R.Range(-0.15, 0.15);
					if (H.House(KitOf(KHut), TX + D * std::cos(A), TY + D * std::sin(A), Turn, 6.5, 5.0)) ++Built;
				}
				// The herders' camps round it: two to five tents each, their doors to the south, the camps a few hundred
				// metres apart.
				if (bYurts)
				{
					for (int Camp = 0; Camp < 9; ++Camp)
					{
						const double A = R.Range(0.0, 2.0 * Pi), D = R.Range(0.3 * TR, TR + 350.0);
						const double CX = TX + D * std::cos(A), CY = TY + D * std::sin(A);
						const int Tents = 2 + R.Below(4);
						for (int T = 0; T < Tents; ++T)
						{
							const double Ox = R.Range(-22.0, 22.0), Oy = R.Range(-14.0, 14.0), Turn = R.Range(-0.25, 0.25);
							if (H.House(KitOf(KYurt), CX + Ox, CY + Oy, Turn, 7.0, 7.0)) ++Built;
						}
					}
					// The ovoo on the highest rise near the field's middle: the plain's landmark, as the hills' tops are.
					double BestX = 0.0, BestY = 0.0, BestZ = -1e9;
					for (int K = 0; K < 80; ++K)
					{
						const double X = R.Range(0.15 * H.Width(), 0.85 * H.Width()), Y = R.Range(0.15 * H.Depth(), 0.85 * H.Depth());
						if (!H.Free(X, Y, 4.0)) continue;
						const double Z = H.Land(X, Y);
						if (Z > BestZ)
						{
							BestZ = Z;
							BestX = X;
							BestY = Y;
						}
					}
					const double Yaw = R.Range(0.0, 2.0 * Pi);
					if (BestZ > -1e8) H.House(KitOf(KOvoo), BestX, BestY, Yaw, 4.0, 4.0);
				}
				H.Tally(Built, 0, 0);
			}

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0xD9A7);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// Boulders and outcrops: few on the sandy steppe, more on the rules' rough ground.
				const int Rocks = static_cast<int>(W * D / 10000.0 * 6.0);
				for (int K = 0; K < Rocks; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.5), Flat = R.Range(0.6, 1.0);
					const Spot S = H.At(X, Y);
					const double Share = S.Kind == KRough ? 0.4 : S.Kind == KOpen ? (bSandy ? 0.01 : 0.05) : 0.0;
					if (Keep >= Share) continue;
					const bool bBig = Keep < 0.2 * Share && S.Kind == KRough;
					if (!H.Free(X, Y, bBig ? 5.0 : 1.5)) continue;
					H.Plant(KitOf(bBig ? KOutcrop : KBoulder), X, Y, Yaw, Size, Size * Flat, Tint(1.0, 1.0, 1.0));
				}
				// Scrub where the rules have woods: pea-shrub, low (half a metre to a metre and a half), open and grey-green,
				// in loose patches (the art director: no dark balls scattered as a park; willows only along the river, which
				// the far ring paints). The marshes' reeds are clutter round the eye.
				const int Scrub = static_cast<int>(W * D / 10000.0 * 220.0);
				for (int K = 0; K < Scrub; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Grey = R.Range(0.82, 0.94), Low = R.Range(0.45, 0.85);
					const Spot S = H.At(X, Y);
					if (S.Kind == KWoods)
					{
						if (Keep < 0.3 * Smooth(-0.5, 0.2, Read(X, Y, 3)) && H.Free(X, Y, 1.6))
							H.Plant(KitOf(KShrub), X, Y, Yaw, 0.6 + 0.7 * Low, (0.6 + 0.7 * Low) * R.Range(0.7, 1.05), Tint(Grey * 1.1, Grey * 1.1, Grey * 1.06)); // (pea-shrub: low, open, grey-green)
					}
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "sand" || Ground == "desert") return 5;
				if (Ground == "rough" || Ground == "rock") return 7; // (the stony rises: rock)
				return Else == 4 && !bWinter && !(bSouth && Green > 0.0) ? 0 : Else; // (no mud on the summer steppe; the south's in its rains)
			}
			const char* Earth() const override { return bSandy ? "sand" : "loam"; }

		private:
			double Green = 0.0;
			bool bWinter = false, bKhalkha = false, bYurts = false, bSandy = false, bSouth = false;
			static constexpr double CoarseStep = 12.0;
			double CX0 = 0.0, CY0 = 0.0;
			int CNX = 0, CNY = 0;
			std::vector<float> Coarse;
		};
	}

	std::unique_ptr<Region> MakeDry(const LandField::Field& F, uint64_t S) { return std::make_unique<Dry>(F, S); }
}
