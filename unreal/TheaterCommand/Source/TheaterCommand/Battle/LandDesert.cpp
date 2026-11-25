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
			CGravel = 4,  // a scatter of gravel, thumbnail-sized: the serir's speckle round the eye
			CRuts = 5     // a piece of one wheel's or track's rut in the broken crust, 2.4 m long (the trial "trigh" only)
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
		/** The braided tracks' broken crust (the trial "trigh"): the pale silt the wheels crushed out of the pavement. */
		const Tone TrackDust = Rgb(204, 190, 164);
		/** A rut's piece as built: grey, its tint the ground's (Dress). */
		constexpr double RutGrey = 200.0;
		/** A rut's pieces (the trial "trigh"; metres, at Scale 1): each RutStep long, laid end to end at stations RutStep
		 * apart along its rut, so a piece meets each neighbour laid at its end and the rut runs on unbroken; a piece with
		 * one neighbour fades along its length to the ground's own colour toward the side it has none, and a piece with
		 * none is not laid. Across: RutWide of pressed floor and its lips, and RutFeather either side fading into the
		 * ground; RutPress the floor's share of the ground's light, RutLip the crushed lips' (linear). RutRamp the rise
		 * built along one (its copy's ScaleZ sets its slope). A try within RutBy of a rut's line lays its station's piece
		 * (so wide that some nine tries fall in a station's stretch and one is seldom missed). A piece lies where its
		 * strand is worn RutWorn or more, its line bending no tighter than a RutBend circle, at least RutClear over the
		 * ground under it and nowhere more than RutFloat over it; each strand's pieces RutLift and a little more over
		 * their line (the more the wider its vehicles' gauge), so two strands' ruts crossing are a few millimetres apart. */
		constexpr double RutStep = 2.4, RutWide = 0.26, RutFeather = 0.12, RutRamp = 0.01, RutPress = 0.84, RutLip = 1.02;
		constexpr double RutBy = 0.85, RutWorn = 0.35, RutBend = 70.0;
		constexpr double RutLift = 0.006, RutClear = 0.002, RutFloat = 0.023;
		/** The most the ground may bend under a rut's piece (its height 4 m either side of the piece's middle, along it
		 * and across, against twice the middle's), so the drawn ground's 4 m grid lies on the ground's own height
		 * there and the piece on it. */
		constexpr double RutKink = 0.03;
		/** sRGB (0 to 1) to linear light and back. */
		double Decode(double C) { return std::pow((std::clamp(C, 0.0, 1.0) + 0.055) / 1.055, 2.4); }
		double Encode(double L) { return 1.055 * std::pow(std::clamp(L, 0.0, 1.0), 1.0 / 2.4) - 0.055; }
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
				// (the braided tracks, on trial: laid in Sites, once the lanes are)
				bTracks = F.Tried("trigh");
				// (the field's own wadi with its line of scrub, and a low ridge beyond the field, on trial: "desertland")
				bLand = F.Tried("desertland");
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
			/** A wadi here: 1 on its floor's middle, 0 out on the plain. Its lines wander some 650 m apart; 15 to 40 m across.
			 * And the field's own (FieldWadi): one always crosses it. */
			double WadiNoise(double X, double Y) const
			{
				const double Warp = 0.35 * Noise(X / 230.0, Y / 230.0, Seed ^ 0xA2);
				const double N = Noise(X / 640.0 + Warp, Y / 640.0, Seed ^ 0xA1);
				return std::max(Smooth(0.032, 0.011, std::fabs(N)), FieldWadi(X, Y));
			}
			/** The field's own wadi (the art director's Mac pass 1, B5: "a plate of ground with no dune, wadi or scrub"; the
			 * noise's wadis some 650 m apart can miss a field): one crossing it from west to east between a quarter and three
			 * quarters of its depth, meandering 60 to 110 m either side over 350 to 550 m, its floor 10 to 22 m across and its
			 * banks a cut, 2.5 m from the plain's edge to the floor (the same depth as the noise's, under a metre in the field:
			 * Relief), so from a man's eye it is a shaded step and a line of scrub across the plain (Plants). 1 on the floor.
			 * On trial, "desertland" (0 without it). */
			double FieldWadi(double X, double Y) const
			{
				if (!bLand) return 0.0;
				const double Mid = Fd.Depth() * (0.25 + 0.5 * Unit(Hash(Seed, 0x3AD1)));
				const double Wave = 350.0 + 200.0 * Unit(Hash(Seed, 0x3AD2)), Swing = 60.0 + 50.0 * Unit(Hash(Seed, 0x3AD3));
				const double Line = Mid + Swing * std::sin(2.0 * Pi * X / Wave + 6.0 * Unit(Hash(Seed, 0x3AD4))) + 0.45 * Swing * Noise(X / 170.0, 0.5, Seed ^ 0x3AD5);
				const double Half = 5.0 + 6.0 * (0.5 + 0.5 * Noise(X / 260.0, 1.5, Seed ^ 0x3AD6));
				// (thinning out a kilometre beyond the field's ends, so it runs into the plain's own wadis or the haze)
				const double Ends = Smooth(-1300.0, -300.0, X) * Smooth(Fd.Width() + 1300.0, Fd.Width() + 300.0, X);
				return Ends * Smooth(Half + 1.5, Half - 1.0, std::fabs(Y - Line));
			}
			/** A low ridge out beyond the field (the art director, B5: "a low ridge between eye and horizon"): 12 to 22 m high
			 * some 300 to 700 m out from the field's edge, rising gently on the field's side over 350 m and falling over 500 m
			 * beyond, broken into pieces round the field (about half the horizon), and never on the escarpment's side, which has
			 * its own. In Relief, so all of its height stands (the rolling country is only blended in over 1.2 km). On trial,
			 * "desertland" (0 without it). */
			double Ridge(double X, double Y) const
			{
				const double D = Outside(X, Y);
				if (!bLand || D <= 0.0) return 0.0;
				const double Cx = 0.5 * Fd.Width(), Cy = 0.5 * Fd.Depth();
				const double A = std::atan2(Y - Cy, X - Cx), CA = std::cos(A), SA = std::sin(A);
				const double Out = 500.0 + 200.0 * Noise(1.8 * CA, 1.8 * SA, Seed ^ 0x51D1);
				const double High = 12.0 + 10.0 * (0.5 + 0.5 * Noise(3.1 * CA + 4.0, 3.1 * SA, Seed ^ 0x51D2));
				const double Piece = Smooth(-0.12, 0.18, Noise(2.4 * CA - 3.0, 2.4 * SA + 1.0, Seed ^ 0x51D3));
				const double T = D - Out;
				const double Across = T < 0.0 ? Smooth(-350.0, 0.0, T) : Smooth(500.0, 0.0, T);
				return High * Piece * Across * Smooth(-250.0, -600.0, Scarp(X, Y));
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
				return Z + 0.4 * Fbm(X, Y, 300.0, 2, Seed ^ 0xD1) - Deep * Wadi(X, Y) + Ridge(X, Y);
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
				return bTracks ? Braided(T, S, false) : T;
			}

			Tone Far(const Spot& S, bool /*bWoods*/, double /*Gap*/) const override
			{
				Tone T = Plain(S.Kind == KWoods ? static_cast<int>(KOpen) : S.Kind, S.N1, 0.0, S.Broad, 0.0, Read(S.X, S.Y, 1), Read(S.X, S.Y, 2), Read(S.X, S.Y, 3));
				if (bTracks) T = Braided(T, S, true); // (the tracks' corridors run on into the haze)
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
				// (on trial: the wheels' pair of ruts along each braided track's strand, every try near a rut's line laying its
				// station's piece, the gravel they crushed gone; their stations only on open, field or rough ground clear of
				// the lane's own worn edge (MakeTracks), and any try near one laying it, so that one is seldom missed. Thick
				// enough that no recorded way thins the tries)
				if (bTracks)
				{
					RutLaid Rl;
					if (RutPiece(S.X, S.Y, Rl)) return Grown{ ClutOf(CRuts), 6.0 };
				}
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

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				const int K = static_cast<int>(P.Kind) - static_cast<int>(LandField::Base::Clut0);
				switch (K)
				{
				case CRuts:
				{
					// (its station's piece, laid where its station is and the same whichever try lays it: square-ended where
					// it has both neighbours, else the mesh fading toward the one it lacks; built grey, tinted to the
					// ground's colour at its middle as the paint has it, so its edges are the ground's own and its floor the
					// ground pressed darker: the tint multiplies the linear colour)
					RutLaid Rl;
					if (!RutPiece(S.X, S.Y, Rl))
					{
						P.Scale = P.ScaleZ = 0.001f; // (never: Grow asked the same)
						return true;
					}
					P.Kind = Rl.Fade == 0 ? ClutOf(CRuts) : Rl.Fade > 0 ? LandField::Base::Trodden : LandField::Base::Stubble;
					P.X = static_cast<float>(Rl.X);
					P.Y = static_cast<float>(Rl.Y);
					P.Z = static_cast<float>(Rl.Z);
					P.Yaw = static_cast<float>(Rl.Yaw);
					P.Scale = static_cast<float>(Rl.Scale);
					P.ScaleZ = static_cast<float>(Rl.ScaleZ);
					Spot At = S;
					At.X = Rl.X;
					At.Y = Rl.Y;
					const Tone Under = Colour(At);
					const double Built = Decode(RutGrey / 255.0);
					P.Tint = Tint(Decode(Under.R / 255.0) / Built, Decode(Under.G / 255.0) / Built, Decode(Under.B / 255.0) / Built);
					return true;
				}
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
				case CRuts:
				{
					if (!bTracks) break;
					// a piece of a rut with both its neighbours, square to its ends (RutMesh); the end pieces, fading
					// ahead or behind, are the Trodden and Stubble bases' (OwnClutter)
					RutMesh(W, 0);
					M.FarFrom = 50.0f;
					break;
				}
				default: break;
				}
			}

			bool OwnClutter(LandField::Base B, LandField::BaseMesh& M) const override
			{
				// (on trial: a rut's end pieces, fading ahead (+x as built) in the cow path's slot and behind in the stubble's,
				// which the desert never lays; drawn as the ground's clutter, as the rut's other pieces are)
				if ((B != LandField::Base::Trodden && B != LandField::Base::Stubble) || !bTracks) return false;
				M.Near = LandField::Mesh();
				M.Far = LandField::Mesh();
				M.Mid = LandField::Mesh();
				M.Cards = LandField::Mesh();
				M.Sprigs = LandField::Mesh();
				Writer W(M.Near);
				RutMesh(W, B == LandField::Base::Trodden ? 1 : -1);
				M.FarFrom = 50.0f;
				return true;
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
			// (the trial "deserthouses": the town's flat-roofed houses at the vehicles' detail, LandHouse's Kind 10: limewashed, its
			// bare mud for the mud brick ones, or its main room of two storeys)
			bool KitHouse(int K, LandHouse::Spec& Out) const override
			{
				if (K != KHouse && K != KHouseStone && K != KHouseTwo) return false;
				Out.Kind = 10;
				Out.Roof = K == KHouseStone ? 1 : K == KHouseTwo ? 2 : 0;
				return true;
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
				if (bTracks) MakeTracks(H); // (the lanes are laid by now; the paint and the clutter ask the tracks later)
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
					// (the field's own wadi: a line of scrub along its floor and up its banks, thick and a third bigger, broken
					// only now and then, so it reads across the plain as a dark line; the art director, B5)
					const double Fw = FieldWadi(X, Y);
					const double Line = Fw * (0.55 + 0.45 * Smooth(-0.4, 0.1, Noise(X / 90.0, Y / 90.0, Seed ^ 0x7D)));
					Share = std::max(Share, 0.75 * Line);
					if (Keep >= Share || !H.Free(X, Y, 0.8)) continue;
					const double Big = 1.0 + 0.35 * Fw;
					H.Plant(KitOf(KThorn), X, Y, Yaw, Size * Big, Size * Big * Tall, Tint(Grey * ThornR, Grey, Grey * ThornB));
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

			// ---------------------------------------------------------------- the braided tracks (on trial)

			/**
			 * The trial "trigh" (Field::Tried("trigh"): the game's -TheaterLandTrial=trigh, the cloud test's
			 * LAND_FIELD_TRIAL=trigh): each desert track as the general sees it from his height, a braided band 40 to 300 m
			 * across of pale strands of broken crust, where the vehicles spread out over the plain beside the worn way to
			 * keep out of each other's dust and ruts, splitting and rejoining, and drawing in again where the way meets
			 * another, enters the town, turns hard or crosses a wadi. Each lane carries three to seven strands 6 to 10 m
			 * wide, wandering across its band, coming and going along it. Out past the field the track's dusty corridor
			 * runs on into the haze.
			 *
			 * The strands are laid once as lines of points 2 m apart, and an 8 m raster over the field and some 870 m round
			 * it holds each square's nearest point (a two-pass chamfer sweep, the nearest point carried, its own distance
			 * measured); the paint and the clutter ask the nearest strand's middle line from the four squares round a
			 * place. (The strands themselves are the raster's seeds, not the lanes: a place's distance to the union of the
			 * strands has no seam where two tracks' bands meet or a track's own band folds round a bend, which a frame
			 * taken from the nearest lane point would.)
			 *
			 * At a man's eye each strand carries one vehicle's pair of wheel ruts astride its middle (CRuts), each rut one
			 * width all its length (0.22 to 0.30 m of pressed floor, the strand's own, and a feathered edge either side)
			 * and laid in pieces end to end at stations along its line, each piece reaching from the rut's place half a
			 * station behind its own to the place half a station ahead, so neighbours share their ends: no gap, no
			 * overlap, no step between them. Each lies straight between those two places, tilted to the fall of the ground
			 * between them (a ramp built in, its copy's ScaleZ setting the slope) and lifted just clear of it. A rut gives
			 * out where the ground is too steep, too rough under a straight piece, where the line bends too hard, or where
			 * its strand is worn faint; the piece either side of a gap fades over its length to the ground's own colour,
			 * so a rut runs on unbroken where it runs and fades in and out where it does not, never a square end. (The
			 * clutter's tries fall at random: a station's piece is laid by each try within some 0.85 m of its stretch of
			 * the rut, all of them the same piece in the same place; some nine tries a station, so one is seldom
			 * missed, and the gravel the wheels crushed is gone from under and beside them.)
			 */
			struct Track
			{
				double D = 1e9;        // metres from the nearest strand's middle line
				double Lat = 0.0;      // the same, signed: to the left of its way positive
				double Half = 0.0;     // its half width
				double Strength = 0.0; // 0 to 1: how worn it is here (strands come and go)
				double Yaw = 0.0;      // its way (radians from east)
				double Gauge = 0.85;   // half its vehicle's gauge: its ruts this far either side of its middle
				double Wide = 0.26;    // its ruts' width
				double Arc = 0.0;      // metres along its middle line from where it starts
				size_t Seg = 0;        // the strand point its nearest stretch of line runs on from
			};

			/** One piece of a rut as laid: its middle and height, its way (turned half round where the ground falls along
			 * the rut, so its built ramp rises with it), its size, the ramp's tilt, and its fade as built: 0 none (square
			 * to both ends), 1 toward its +x end, -1 toward its -x end. */
			struct RutLaid
			{
				double X = 0.0, Y = 0.0, Z = 0.0, Yaw = 0.0, Scale = 1.0, ScaleZ = 1.0;
				int Fade = 0;
			};

			/** A rut's piece as it would lie at a station: its middle and way, its length (its two ends' places apart), and
			 * the line under it (its height at its middle and the ground's fall along it). */
			struct RutFit
			{
				double X = 0.0, Y = 0.0, Yaw = 0.0, Long = 0.0, Z = 0.0, Fall = 0.0;
			};

			/** The nearest strand within Reach metres of its edge to a place: false where there is none. */
			bool Nearest(double X, double Y, double Reach, Track& Out) const
			{
				if (TCell.empty()) return false;
				const double U = (X - TX0) / TrackStep - 0.5, V = (Y - TY0) / TrackStep - 0.5;
				if (!(U >= 0.0 && V >= 0.0 && U < TNX - 1.0 && V < TNY - 1.0)) return false;
				const int I0 = static_cast<int>(U), J0 = static_cast<int>(V);
				// (most of the plain lies far from every strand: the nearest square's own distance says so at once, less the
				// square's half diagonal and the slack of a point's distance for its line's)
				const size_t Mid = static_cast<size_t>(J0 + (V - J0 >= 0.5 ? 1 : 0)) * static_cast<size_t>(TNX) + static_cast<size_t>(I0 + (U - I0 >= 0.5 ? 1 : 0));
				if (TDist[Mid] > Reach + 5.0 + 8.0) return false;
				int Seen[4] = { -1, -1, -1, -1 };
				int Best = -1;
				double BestT = 0.0, BestD = 1e18;
				for (int K = 0; K < 4; ++K)
				{
					const int P = TCell[static_cast<size_t>(J0 + K / 2) * static_cast<size_t>(TNX) + static_cast<size_t>(I0 + K % 2)];
					if (P < 0 || P == Seen[0] || P == Seen[1] || P == Seen[2]) continue;
					Seen[K] = P;
					// (its line both ways from it: to the point before and the point after, where they are of its strand)
					for (int Way = 0; Way < 2; ++Way)
					{
						const int A = Way == 0 ? P - 1 : P;
						if (A < 0 || static_cast<size_t>(A) + 1 >= SX.size() || !SLink[static_cast<size_t>(A)])
						{
							const double D2 = (X - SX[static_cast<size_t>(P)]) * (X - SX[static_cast<size_t>(P)]) + (Y - SY[static_cast<size_t>(P)]) * (Y - SY[static_cast<size_t>(P)]);
							if (D2 < BestD)
							{
								BestD = D2;
								Best = P;
								BestT = 0.0;
							}
							continue;
						}
						const size_t Ai = static_cast<size_t>(A);
						const double Ax = SX[Ai], Ay = SY[Ai], Dx = SX[Ai + 1] - Ax, Dy = SY[Ai + 1] - Ay, L2 = Dx * Dx + Dy * Dy;
						const double T = L2 > 1e-9 ? std::clamp(((X - Ax) * Dx + (Y - Ay) * Dy) / L2, 0.0, 1.0) : 0.0;
						const double Ex = Ax + T * Dx - X, Ey = Ay + T * Dy - Y, D2 = Ex * Ex + Ey * Ey;
						if (D2 < BestD)
						{
							BestD = D2;
							Best = A;
							BestT = T;
						}
					}
				}
				if (Best < 0) return false;
				// (the raster holds each square's nearest point, some 8 m apart along a strand, and the stretches either side
				// of it reach only 2 m: walk on along the strand while a stretch ahead or behind lies nearer, so a place's own
				// stretch is found, not the end of the held point's, and its distance and its way along it are its own)
				for (int Walk = 0; Walk < 24; ++Walk)
				{
					bool bMoved = false;
					for (int Dir = -1; Dir <= 1 && !bMoved; Dir += 2)
					{
						const int A = Best + Dir;
						if (A < 0 || static_cast<size_t>(A) + 1 >= SX.size() || !SLink[static_cast<size_t>(A)]) continue;
						if (Dir > 0 && !SLink[static_cast<size_t>(Best)]) continue;
						const size_t Ai = static_cast<size_t>(A);
						const double Ax = SX[Ai], Ay = SY[Ai], Dx = SX[Ai + 1] - Ax, Dy = SY[Ai + 1] - Ay, L2 = Dx * Dx + Dy * Dy;
						const double T = L2 > 1e-9 ? std::clamp(((X - Ax) * Dx + (Y - Ay) * Dy) / L2, 0.0, 1.0) : 0.0;
						const double Ex = Ax + T * Dx - X, Ey = Ay + T * Dy - Y, D2 = Ex * Ex + Ey * Ey;
						if (D2 < BestD - 1e-9)
						{
							BestD = D2;
							Best = A;
							BestT = T;
							bMoved = true;
						}
					}
					if (!bMoved) break;
				}
				const size_t B = static_cast<size_t>(Best), B1 = BestT > 0.0 ? B + 1 : B;
				Out.D = std::sqrt(BestD);
				Out.Half = Lerp(SHalf[B], SHalf[B1], BestT);
				if (Out.D > Out.Half + Reach) return false;
				Out.Strength = Lerp(SStrength[B], SStrength[B1], BestT);
				Out.Yaw = SYaw[B];
				Out.Gauge = SGauge[B];
				Out.Wide = SWide[B];
				Out.Arc = Lerp(SArc[B], SArc[B1], BestT);
				Out.Seg = B;
				// (which side of its line: by the stretch it lies off, or at a strand's end by its way)
				const bool bOn = SLink[B] && B + 1 < SX.size();
				const double Wx = bOn ? SX[B + 1] - SX[B] : std::cos(SYaw[B]), Wy = bOn ? SY[B + 1] - SY[B] : std::sin(SYaw[B]);
				Out.Lat = Wx * (Y - SY[B]) - Wy * (X - SX[B]) >= 0.0 ? Out.D : -Out.D;
				return true;
			}

			/** The tracks' paint over the plain's (T) at a place: in the field each strand's pale broken crust and its
			 * darker worn middle, a faint dust round them; out over the band and the ring, where the ground's points lie
			 * 8 m and more apart and a strand would draw as a string of beads, the band's corridor of dust alone. */
			Tone Braided(Tone T, const Spot& S, bool bRing) const
			{
				Track Tr;
				if (!Nearest(S.X, S.Y, 26.0, Tr)) return T;
				// (fainter on the bare rock, under the palms and on the salt, which the vehicles keep off)
				const double On = Tr.Strength * (S.Kind == KRough ? 0.6 : S.Kind == KWoods ? 0.35 : S.Kind == KMarsh ? 0.3 : 1.0);
				const double One = bRing ? 0.0 : Smooth(110.0, 10.0, Outside(S.X, S.Y));
				// (the crust broken: palest over the varnished gravel's patches, whose dark pavement the wheels crush into
				// the pale silt under it; on a sand sheet a track barely shows)
				const double Varn = Read(S.X, S.Y, 1);
				T = Mixed(T, TrackDust, On * (0.16 - 0.04 * One) * Smooth(Tr.Half + 24.0, Tr.Half, Tr.D));
				// (each strand's crust broken more here and less there, over some 40 m)
				const double Body = One * Smooth(Tr.Half + 1.5, Tr.Half - 1.5, Tr.D);
				const double Broken = Body > 0.0 ? 0.72 + 0.28 * Noise(S.X / 40.0, S.Y / 40.0, Seed ^ 0x7B30) : 1.0;
				T = Mixed(T, TrackDust, On * Broken * (0.3 + 0.3 * Varn) * Body);
				// (its worn middle darker, a little wider than the ground's points lie apart so it never draws as beads; none
				// beside the lane's own floor, whose ruts are its own)
				return Mixed(T, Rut, On * One * 0.25 * Smooth(0.7 * Tr.Half + 1.0, 0.0, Tr.D) * Smooth(2.0, 8.0, S.Lane));
			}

			/** A place on a rut: its strand walked from point Seg to Arc metres along its middle line, then Side (1 the left
			 * rut, -1 the right) its gauge out; the ground under it as MakeTracks read it, its way and its wear. False off
			 * the strand's ends or where its ground was not read. (Wherever the walk starts on the strand it ends in the same
			 * stretch, so every try asking for a station gets the same numbers.) */
			bool OnRut(size_t Seg, double Arc, int Side, double& X, double& Y, double& Z, double& Yaw, double& Worn) const
			{
				size_t P = Seg;
				while (SArc[P] > Arc)
				{
					if (P == 0 || !SLink[P - 1]) return false;
					--P;
				}
				while (SLink[P] && SArc[P + 1] <= Arc) ++P;
				const size_t Q = SLink[P] ? P + 1 : P;
				if (Q == P && Arc > SArc[P]) return false;
				const double T = Q == P ? 0.0 : (Arc - SArc[P]) / std::max(1e-6, static_cast<double>(SArc[Q] - SArc[P]));
				const size_t Which = Side > 0 ? 0 : 1;
				const float ZP = SZ[2 * P + Which], ZQ = SZ[2 * Q + Which];
				if (std::isnan(ZP) || std::isnan(ZQ)) return false;
				Yaw = SYaw[P] + T * std::remainder(static_cast<double>(SYaw[Q] - SYaw[P]), 2.0 * Pi);
				const double G = Side * static_cast<double>(SGauge[P]);
				X = Lerp(SX[P], SX[Q], T) - std::sin(Yaw) * G;
				Y = Lerp(SY[P], SY[Q], T) + std::cos(Yaw) * G;
				Z = Lerp(ZP, ZQ, T);
				Worn = Lerp(SStrength[P], SStrength[Q], T);
				return true;
			}

			/** A rut's piece at station N as it would lie: from the rut's place at (N - 1/2) stations' spacing along its
			 * strand's middle line to its place at (N + 1/2), straight between them, so it meets its neighbours at their
			 * ends; false where none lies there (the rut's ground not read or off its strand, the strand worn faint, the
			 * ground too steep along or across, too rough under a straight piece to keep it both clear and close at Lift
			 * over its line, or the line bending too hard). */
			bool Station(size_t Seg, long N, int Side, double Scale, double Lift, RutFit& Out) const
			{
				if (N < 0) return false;
				const double Step = RutStep * Scale, A = static_cast<double>(N) * Step;
				// (its ends, its quarters and its middle: the ends give its line, the rest the ground under it)
				const double U[5] = { -0.5 * Step, -0.25 * Step, 0.0, 0.25 * Step, 0.5 * Step };
				double X[5], Y[5], Z[5], Yaw[5], Worn = 0.0;
				for (int K = 0; K < 5; ++K)
				{
					double Wk = 0.0;
					if (!OnRut(Seg, A + U[K], Side, X[K], Y[K], Z[K], Yaw[K], Wk)) return false;
					if (K == 2) Worn = Wk;
				}
				if (Worn < RutWorn) return false;
				Out.Long = std::hypot(X[4] - X[0], Y[4] - Y[0]);
				if (!(Out.Long > 0.5 * Step)) return false;
				Out.X = 0.5 * (X[0] + X[4]);
				Out.Y = 0.5 * (Y[0] + Y[4]);
				Out.Yaw = std::atan2(Y[4] - Y[0], X[4] - X[0]);
				Out.Z = 0.5 * (Z[0] + Z[4]);
				Out.Fall = (Z[4] - Z[0]) / Out.Long;
				if (std::fabs(Out.Fall) > 0.04) return false;
				if (std::fabs(std::remainder(Yaw[4] - Yaw[0], 2.0 * Pi)) * RutBend > Step) return false;
				// (the ground across it, from the other rut's beside it)
				double Xo = 0.0, Yo = 0.0, Zo = 0.0, Ao = 0.0, Wo = 0.0;
				if (!OnRut(Seg, A, -Side, Xo, Yo, Zo, Ao, Wo)) return false;
				const double Across = (Z[2] - Zo) / std::max(0.5, std::hypot(X[2] - Xo, Y[2] - Yo));
				if (std::fabs(Across) > 0.03) return false;
				// (clear of the ground's highest under its line and its edge's rise across it, and nowhere floating more
				// than RutFloat over it)
				const double Edge = std::fabs(Across) * (0.5 * RutWide + RutFeather) * Scale;
				double Hi = -1e9, Lo = 1e9;
				for (int K = 0; K < 5; ++K)
				{
					const double Dev = Z[K] - (Out.Z + Out.Fall * U[K] * Out.Long / Step);
					Hi = std::max(Hi, Dev);
					Lo = std::min(Lo, Dev);
				}
				return Lift >= Hi + Edge + RutClear && Lift - Lo + Edge <= RutFloat;
			}

			/** The rut's piece a clutter try at a place lays: false where it lays none. The try's station is the one whose
			 * stretch of its nearest strand it falls in, within RutBy of the nearer rut's line (MakeRuts laid them). */
			bool RutPiece(double X, double Y, RutLaid& Out) const
			{
				Track Tr;
				if (RutSt.empty() || !Nearest(X, Y, 0.0, Tr)) return false;
				if (std::fabs(std::fabs(Tr.Lat) - Tr.Gauge) > RutBy) return false;
				const uint32_t Sn = SNum[Tr.Seg];
				const long N = std::lround(Tr.Arc / (RutStep * Tr.Wide / RutWide));
				if (N < 0 || N >= static_cast<long>(RutCount[Sn])) return false;
				const RutLaid& R = RutSt[RutFirst[Sn] + (Tr.Lat >= 0.0 ? 0u : RutCount[Sn]) + static_cast<uint32_t>(N)];
				if (!(R.Scale > 0.0)) return false;
				Out = R;
				return true;
			}

			/** Each strand's ruts' pieces, once (after the raster: a station lies only where its strand is the nearest, at
			 * its middle and ends and out to the edge of the tries that lay it, so those tries are its own strand's; and
			 * where the ground bends no more than RutKink under it): a station's piece where it lies with one of its
			 * neighbours or both, fading toward the one it lacks. */
			void MakeRuts(Host& H)
			{
				RutSt.clear();
				RutFirst.clear();
				RutCount.clear();
				for (size_t P0 = 0; P0 < SX.size();)
				{
					size_t P1 = P0;
					while (SLink[P1]) ++P1;
					const double Scale = SWide[P0] / RutWide, Step = RutStep * Scale;
					const double Lift = RutLift + 0.006 * std::clamp((SGauge[P0] - 0.7) / 0.35, 0.0, 1.0);
					const uint32_t Count = static_cast<uint32_t>(std::floor(SArc[P1] / Step)) + 1;
					RutFirst.push_back(static_cast<uint32_t>(RutSt.size()));
					RutCount.push_back(Count);
					const uint32_t Sn = SNum[P0];
					auto Mine = [&](double X, double Y) {
						Track T;
						return Nearest(X, Y, 0.0, T) && SNum[T.Seg] == Sn;
					};
					for (int Side = 1; Side >= -1; Side -= 2)
					{
						std::vector<RutFit> Fit(Count);
						std::vector<uint8_t> Ok(Count, 0);
						size_t Cur = P0;
						for (uint32_t N = 0; N < Count; ++N)
						{
							const double A = static_cast<double>(N) * Step;
							while (Cur > P0 && SArc[Cur] > A - Step) --Cur;
							while (SLink[Cur] && SArc[Cur + 1] <= A - Step) ++Cur;
							RutFit& F = Fit[N];
							if (!Station(Cur, static_cast<long>(N), Side, Scale, Lift, F)) continue;
							const double Ca = std::cos(F.Yaw), Sa = std::sin(F.Yaw), Hl = 0.5 * F.Long, Out = Side * RutBy;
							if (!(Mine(F.X, F.Y) && Mine(F.X + Hl * Ca, F.Y + Hl * Sa) && Mine(F.X - Hl * Ca, F.Y - Hl * Sa) && Mine(F.X - Sa * Out, F.Y + Ca * Out))) continue;
							const double Zm = 2.0 * H.Top(F.X, F.Y), Ax = 4.0 * Ca, Ay = 4.0 * Sa;
							Ok[N] = std::fabs(H.Top(F.X + Ax, F.Y + Ay) + H.Top(F.X - Ax, F.Y - Ay) - Zm) <= RutKink
								&& std::fabs(H.Top(F.X - Ay, F.Y + Ax) + H.Top(F.X + Ay, F.Y - Ax) - Zm) <= RutKink;
						}
						for (uint32_t N = 0; N < Count; ++N)
						{
							RutLaid R;
							R.Scale = 0.0;
							const bool bBefore = N > 0 && Ok[N - 1], bAfter = N + 1 < Count && Ok[N + 1];
							if (Ok[N] && (bBefore || bAfter))
							{
								// (laid the other way round where the ground falls along its way, so its built ramp falls with it;
								// its fade toward the neighbour it lacks, ahead along its way or behind, as built)
								const RutFit& F = Fit[N];
								const bool bTurn = F.Fall < 0.0;
								R.X = F.X;
								R.Y = F.Y;
								R.Z = F.Z + Lift;
								R.Yaw = F.Yaw + (bTurn ? Pi : 0.0);
								R.Scale = F.Long / RutStep;
								R.ScaleZ = std::max(0.02, std::fabs(F.Fall) * R.Scale / RutRamp);
								R.Fade = bBefore && bAfter ? 0 : (bAfter ? -1 : 1) * (bTurn ? -1 : 1);
							}
							RutSt.push_back(R);
						}
					}
					P0 = P1 + 1;
				}
			}

			/** A rut's piece as built (Scale 1): RutStep long, grey, rising RutRamp along its length (its normals the
			 * ramp's, so a copy tilted by its ScaleZ is lit as the ground under it), flat across. Across it, from its
			 * middle out: a flat pressed floor (RutPress of the ground's light), the crushed lips (RutLip) at RutWide's
			 * edges, and RutFeather beyond them to its edges, the ground's own colour (the grey: the tint makes it the
			 * ground's). Fade 0: square to both ends; 1 or -1: fading along its whole length, eased, from whole at one end
			 * to the ground's own colour at its +x end (1) or its -x end (-1). Its quads split mirrored across its middle
			 * line (a row of points along it). */
			void RutMesh(Writer& W, int Fade) const
			{
				const double Hw = 0.5 * RutWide, L = 0.5 * RutStep;
				const double Ys[7] = { -(Hw + RutFeather), -Hw, -0.45 * Hw, 0.0, 0.45 * Hw, Hw, Hw + RutFeather };
				const double Share[7] = { 1.0, RutLip, RutPress, RutPress, RutPress, RutLip, 1.0 };
				const int Rows = Fade == 0 ? 2 : 5;
				const double Nz = 1.0 / std::hypot(RutRamp, 1.0), Ground = Decode(RutGrey / 255.0);
				uint32_t V[5][7];
				for (int R = 0; R < Rows; ++R)
				{
					const double T = static_cast<double>(R) / (Rows - 1), X = -L + 2.0 * L * T;
					const double F = Fade > 0 ? T : 1.0 - T, On = Fade == 0 ? 1.0 : 1.0 - F * F * (3.0 - 2.0 * F);
					for (int C = 0; C < 7; ++C)
					{
						const double Grey = 255.0 * Encode((1.0 + On * (Share[C] - 1.0)) * Ground);
						V[R][C] = W.Vert(X, Ys[C], RutRamp * X, -RutRamp * Nz, 0.0, Nz, Rgb(Grey, Grey, Grey));
					}
				}
				for (int R = 0; R + 1 < Rows; ++R)
					for (int C = 0; C < 6; ++C)
					{
						const uint32_t A = V[R][C], B = V[R + 1][C], Cc = V[R + 1][C + 1], D = V[R][C + 1];
						if (C < 3)
						{
							W.Tri(A, B, Cc);
							W.Tri(A, Cc, D);
						}
						else
						{
							W.Tri(A, B, D);
							W.Tri(B, Cc, D);
						}
					}
			}

			void MakeTracks(Host& H)
			{
				const double Wd = H.Width(), Dp = H.Depth(), Bd = H.Band();
				// (the raster: the field, its band, and the way the tracks run on past it)
				const double Margin = Bd + TrackOn + 220.0;
				TX0 = -Margin;
				TY0 = -Margin;
				TNX = static_cast<int>((Wd + 2.0 * Margin) / TrackStep) + 1;
				TNY = static_cast<int>((Dp + 2.0 * Margin) / TrackStep) + 1;
				double TwX = 0.0, TwY = 0.0, TwR = 0.0;
				const bool bTown = H.Town(TwX, TwY, TwR);
				uint32_t Strands = 0;
				for (size_t K = 0; K < H.Lanes(); ++K)
				{
					const std::vector<std::pair<double, double>>& L = H.LaneLine(K);
					if (L.size() < 12) continue;
					// The lane's line, run on straight out past an end beyond the field's edge (the track goes on into the
					// haze), each point with the metres it lies past the lane's own end.
					std::vector<double> PX, PY, Past;
					const bool bOutA = Outside(L.front().first, L.front().second) > 1.0, bOutB = Outside(L.back().first, L.back().second) > 1.0;
					auto Run = [&](bool bFront) {
						const std::pair<double, double>& E = bFront ? L.front() : L.back();
						const std::pair<double, double>& Q = bFront ? L[5] : L[L.size() - 6];
						const double Dx = E.first - Q.first, Dy = E.second - Q.second, Len = std::hypot(Dx, Dy);
						if (Len < 1e-6) return;
						for (int N = static_cast<int>(TrackOn / 2.0); N >= 1; --N)
						{
							const double M = bFront ? 2.0 * N : 2.0 * (static_cast<int>(TrackOn / 2.0) + 1 - N);
							PX.push_back(E.first + Dx / Len * M);
							PY.push_back(E.second + Dy / Len * M);
							Past.push_back(M);
						}
					};
					if (bOutA) Run(true);
					for (const std::pair<double, double>& P : L)
					{
						PX.push_back(P.first);
						PY.push_back(P.second);
						Past.push_back(0.0);
					}
					if (bOutB) Run(false);
					const int N = static_cast<int>(PX.size());
					std::vector<double> Along(static_cast<size_t>(N), 0.0);
					for (int P = 1; P < N; ++P) Along[static_cast<size_t>(P)] = Along[static_cast<size_t>(P) - 1] + std::hypot(PX[static_cast<size_t>(P)] - PX[static_cast<size_t>(P) - 1], PY[static_cast<size_t>(P)] - PY[static_cast<size_t>(P) - 1]);
					const double Total = Along.back();
					const double InnerA = bOutA ? 1e9 : 0.0, InnerB = bOutB ? 1e9 : Total;
					// Its middle line smoothed over some 160 m (the strands' frame: the lane's own small wander and bends gone),
					// its way and its bend; the band drawn in where it bends hard (no fold on the inside of a turn), at a
					// wadi's crossing, an end inside the field (a meeting with another way) and the town.
					std::vector<double> RX(static_cast<size_t>(N)), RY(static_cast<size_t>(N)), Way(static_cast<size_t>(N)), Cap(static_cast<size_t>(N)), Wet(static_cast<size_t>(N));
					{
						std::vector<double> SumX(static_cast<size_t>(N) + 1, 0.0), SumY(static_cast<size_t>(N) + 1, 0.0);
						for (int P = 0; P < N; ++P)
						{
							SumX[static_cast<size_t>(P) + 1] = SumX[static_cast<size_t>(P)] + PX[static_cast<size_t>(P)];
							SumY[static_cast<size_t>(P) + 1] = SumY[static_cast<size_t>(P)] + PY[static_cast<size_t>(P)];
						}
						for (int P = 0; P < N; ++P)
						{
							const int Hw = std::min({ 40, P, N - 1 - P });
							RX[static_cast<size_t>(P)] = (SumX[static_cast<size_t>(P + Hw + 1)] - SumX[static_cast<size_t>(P - Hw)]) / (2 * Hw + 1);
							RY[static_cast<size_t>(P)] = (SumY[static_cast<size_t>(P + Hw + 1)] - SumY[static_cast<size_t>(P - Hw)]) / (2 * Hw + 1);
						}
					}
					for (int P = 0; P < N; ++P)
					{
						const size_t A = static_cast<size_t>(std::max(0, P - 4)), B = static_cast<size_t>(std::min(N - 1, P + 4));
						Way[static_cast<size_t>(P)] = std::atan2(RY[B] - RY[A], RX[B] - RX[A]);
						Wet[static_cast<size_t>(P)] = Wadi(PX[static_cast<size_t>(P)], PY[static_cast<size_t>(P)]);
					}
					for (int P = 0; P < N; ++P)
					{
						const size_t A = static_cast<size_t>(std::max(0, P - 15)), B = static_cast<size_t>(std::min(N - 1, P + 15));
						double Turn = Way[B] - Way[A];
						Turn = std::fabs(std::remainder(Turn, 2.0 * Pi));
						const double Span = std::max(1.0, Along[B] - Along[A]);
						Cap[static_cast<size_t>(P)] = 0.8 * Span / std::max(Turn, 1e-4);
					}
					std::vector<double> Half(static_cast<size_t>(N));
					for (int P = 0; P < N; ++P)
					{
						const size_t Pi0 = static_cast<size_t>(P);
						// (each drawn in and let out again smoothly over its approach: a step in the band's width would kink
						// every strand across it)
						double Bend = 1e9, Ford = 0.0;
						for (int Q = std::max(0, P - 150); Q <= std::min(N - 1, P + 150); ++Q) Bend = std::min(Bend, Cap[static_cast<size_t>(Q)] + 0.6 * std::fabs(Along[static_cast<size_t>(Q)] - Along[Pi0]));
						for (int Q = std::max(0, P - 70); Q <= std::min(N - 1, P + 70); Q += 2) Ford = std::max(Ford, Wet[static_cast<size_t>(Q)] * Smooth(140.0, 50.0, std::fabs(Along[static_cast<size_t>(Q)] - Along[Pi0])));
						const double End = std::min(std::fabs(Along[Pi0] - InnerA), std::fabs(InnerB - Along[Pi0]));
						double Hb = 40.0 + 110.0 * Smooth(-0.6, 0.6, Noise(Along[Pi0] / 600.0, 3.1 * static_cast<double>(K) + 0.5, Seed ^ 0x7B10));
						Hb *= 0.12 + 0.88 * Smooth(0.0, 220.0, End);
						if (bTown) Hb *= 0.12 + 0.88 * Smooth(0.8 * TwR, TwR + 300.0, std::hypot(PX[Pi0] - TwX, PY[Pi0] - TwY));
						Hb *= 1.0 - 0.4 * Ford;
						Half[Pi0] = std::min(Hb, Bend);
					}
					// The strands: spread across the band and wandering across it over some 250 m, so they cross, split and
					// rejoin; each coming and going along the way on its own noise (the one nearest the middle, the main
					// way, seldom gone); out past the lane's end fading into the haze.
					const int Count = 3 + static_cast<int>(5.0 * Unit(Hash(Seed ^ 0x7B20, K)));
					for (int I = 0; I < Count; ++I)
					{
						const double Base = -0.85 + 1.7 * (I + 0.5) / Count;
						const double Bias = I == Count / 2 ? 0.35 : 0.1;
						const double HalfW = 3.0 + 2.0 * Unit(Hash(Seed ^ 0x7B21, K, static_cast<uint64_t>(I)));
						// (its ruts: a pair astride its middle, a lorry's gauge to a tank's, each its tyres' width)
						const double Gauge = 0.7 + 0.35 * Unit(Hash(Seed ^ 0x7B23, K, static_cast<uint64_t>(I)));
						const double Wide = 0.22 + 0.08 * Unit(Hash(Seed ^ 0x7B24, K, static_cast<uint64_t>(I)));
						std::vector<double> QX(static_cast<size_t>(N)), QY(static_cast<size_t>(N)), QS(static_cast<size_t>(N));
						for (int P = 0; P < N; ++P)
						{
							const size_t Pi0 = static_cast<size_t>(P);
							const double U = std::clamp(Base + 0.55 * Noise(Along[Pi0] / 250.0 + 13.1 * I, 3.7 * I + 11.3 * static_cast<double>(K), Seed ^ 0x7B11), -1.0, 1.0);
							const double Off = Half[Pi0] * U;
							QX[Pi0] = RX[Pi0] - std::sin(Way[Pi0]) * Off;
							QY[Pi0] = RY[Pi0] + std::cos(Way[Pi0]) * Off;
							QS[Pi0] = Smooth(TrackOn, 0.25 * TrackOn, Past[Pi0]) * Smooth(-0.25, 0.2, Noise(Along[Pi0] / 380.0 + 7.7 * I, 5.1 * I + 2.3 * static_cast<double>(K), Seed ^ 0x7B12) + Bias);
						}
						bool bOpen = false;
						double LastX = 0.0, LastY = 0.0, Arc = 0.0;
						for (int P = 0; P < N; ++P)
						{
							const size_t Pi0 = static_cast<size_t>(P);
							const double X = QX[Pi0], Y = QY[Pi0];
							const bool bIn = X > TX0 + 1.0 && Y > TY0 + 1.0 && X < TX0 + TNX * TrackStep - 1.0 && Y < TY0 + TNY * TrackStep - 1.0;
							if (QS[Pi0] < 0.02 || !bIn)
							{
								bOpen = false;
								continue;
							}
							if (bOpen && std::hypot(X - LastX, Y - LastY) < 0.5) continue; // (bunched on the inside of a bend)
							const size_t A = static_cast<size_t>(std::max(0, P - 1)), B = static_cast<size_t>(std::min(N - 1, P + 1));
							if (bOpen) SLink.back() = 1;
							Arc = bOpen ? Arc + std::hypot(X - LastX, Y - LastY) : 0.0;
							SX.push_back(static_cast<float>(X));
							SY.push_back(static_cast<float>(Y));
							SYaw.push_back(static_cast<float>(std::atan2(QY[B] - QY[A], QX[B] - QX[A])));
							SHalf.push_back(static_cast<float>(HalfW));
							SStrength.push_back(static_cast<float>(QS[Pi0]));
							SGauge.push_back(static_cast<float>(Gauge));
							SWide.push_back(static_cast<float>(Wide));
							SArc.push_back(static_cast<float>(Arc));
							if (!bOpen) ++Strands;
							SNum.push_back(Strands - 1);
							SLink.push_back(0);
							bOpen = true;
							LastX = X;
							LastY = Y;
						}
					}
				}
				// The ground under each point's two ruts, where the eye goes (the field and its band) and the strand is worn
				// enough to carry them: the ruts' pieces lie on it (Station).
				SZ.assign(2 * SX.size(), std::nanf(""));
				for (size_t P = 0; P < SX.size(); ++P)
				{
					if (SStrength[P] < RutWorn - 0.15 || Outside(SX[P], SY[P]) > Bd - 5.0) continue;
					const double Nx = -std::sin(SYaw[P]) * SGauge[P], Ny = std::cos(SYaw[P]) * SGauge[P];
					for (int Side = 0; Side < 2; ++Side)
					{
						// (open, field or rough ground, clear of the lane's own worn edge: nowhere else a rut)
						const double Rx = SX[P] + (Side == 0 ? Nx : -Nx), Ry = SY[P] + (Side == 0 ? Ny : -Ny);
						const Spot Sp = H.At(Rx, Ry);
						if ((Sp.Kind != KOpen && Sp.Kind != KField && Sp.Kind != KRough) || Sp.Lane <= 2.5) continue;
						SZ[2 * P + static_cast<size_t>(Side)] = static_cast<float>(H.Top(Rx, Ry));
					}
				}
				// The raster: each square's nearest strand point, carried in two sweeps (forward, then back), each square
				// measuring its own distance to its neighbours' points; none past TrackReach.
				const size_t Cells = static_cast<size_t>(TNX) * static_cast<size_t>(TNY);
				TCell.assign(Cells, -1);
				TDist.assign(Cells, 1e9f);
				auto CellMid = [this](int I, int J, double& X, double& Y) {
					X = TX0 + (I + 0.5) * TrackStep;
					Y = TY0 + (J + 0.5) * TrackStep;
				};
				for (size_t P = 0; P < SX.size(); ++P)
				{
					const int I = static_cast<int>((SX[P] - TX0) / TrackStep), J = static_cast<int>((SY[P] - TY0) / TrackStep);
					if (I < 0 || J < 0 || I >= TNX || J >= TNY) continue;
					double CX, CY;
					CellMid(I, J, CX, CY);
					const size_t C = static_cast<size_t>(J) * static_cast<size_t>(TNX) + static_cast<size_t>(I);
					const float D = static_cast<float>(std::hypot(SX[P] - CX, SY[P] - CY));
					if (D < TDist[C])
					{
						TDist[C] = D;
						TCell[C] = static_cast<int>(P);
					}
				}
				auto Take = [&](int I, int J, int NI, int NJ) {
					if (NI < 0 || NJ < 0 || NI >= TNX || NJ >= TNY) return;
					const int P = TCell[static_cast<size_t>(NJ) * static_cast<size_t>(TNX) + static_cast<size_t>(NI)];
					if (P < 0) return;
					double CX, CY;
					CellMid(I, J, CX, CY);
					const size_t C = static_cast<size_t>(J) * static_cast<size_t>(TNX) + static_cast<size_t>(I);
					const float D = static_cast<float>(std::hypot(SX[static_cast<size_t>(P)] - CX, SY[static_cast<size_t>(P)] - CY));
					if (D < TDist[C] && D < TrackReach)
					{
						TDist[C] = D;
						TCell[C] = P;
					}
				};
				for (int J = 0; J < TNY; ++J)
					for (int I = 0; I < TNX; ++I)
					{
						Take(I, J, I - 1, J);
						Take(I, J, I - 1, J - 1);
						Take(I, J, I, J - 1);
						Take(I, J, I + 1, J - 1);
					}
				for (int J = TNY - 1; J >= 0; --J)
					for (int I = TNX - 1; I >= 0; --I)
					{
						Take(I, J, I + 1, J);
						Take(I, J, I + 1, J + 1);
						Take(I, J, I, J + 1);
						Take(I, J, I - 1, J + 1);
					}
				MakeRuts(H);
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "woods" || Ground == "forest") return 5;
				if (Ground == "rough" || Ground == "rock") return 7; // (the rises' limestone and the escarpment: rock)
				return Else == 0 || Else == 4 ? 5 : Else;
			}
			const char* Earth() const override { return "sand"; }

		private:
			bool bSpring = false, bPalms = false, bTracks = false, bLand = false;
			// (the braided tracks, on trial: their strands' points, each with its way, half width, wear, its ruts' gauge and
			// width, the metres along it, the ground under its two ruts (left, right; NaN unread), and whether the next
			// point is of its strand; the raster of each square's nearest)
			static constexpr double TrackStep = 8.0;
			static constexpr double TrackOn = 400.0;    // (metres a track runs on past its lane's end beyond the field)
			static constexpr double TrackReach = 64.0;  // (metres from a strand's middle the raster carries it)
			std::vector<float> SX, SY, SYaw, SHalf, SStrength, SGauge, SWide, SArc, SZ;
			std::vector<uint8_t> SLink;
			std::vector<uint32_t> SNum;
			// (each strand's ruts' pieces at their stations, left rut then right, from its first point's station 0: as laid,
			// or not; RutFirst the strand's first, RutCount the stations a rut)
			std::vector<RutLaid> RutSt;
			std::vector<uint32_t> RutFirst, RutCount;
			double TX0 = 0.0, TY0 = 0.0;
			int TNX = 0, TNY = 0;
			std::vector<int> TCell;
			std::vector<float> TDist;
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
