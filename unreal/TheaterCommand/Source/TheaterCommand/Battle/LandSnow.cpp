#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

/**
 * Snow over any ground (the Ardennes in December, Moscow, the Don, Karelia, Cassino's mountains): the look-targets'
 * winter (sections 0, 5 and 7). Snow sets the light: the open ground white, nearly as bright as an overcast sky, and the
 * contrast all in the dirt on it and in the dark things standing out of it: the roads' packed snow darker than the fields,
 * dirty down their middles and churned in their ruts; the trodden paths grey; mud at the gateways, in the yards and round
 * every house; black earth thrown up on the fieldworks; the dark woods (spruce with snow on its boughs, the bare broadleaf
 * a purplish-brown haze of twigs). The snow lies deeper in the lee of every field's edge (the hedge, the fence, the
 * bank) and thinner where the wind scours it off in long streaks; never one flat white: wind-shaped drifts 15 to 70 m
 * long, the freshest lightest, the hollows between cooler, old packed patches greyer and warmer (read off a coarse grid);
 * under trees it is patchy. By its Cover: a dusting (0.1
 * to 0.3: the field's furrows, its stubble and the grass's tips through it, dark patches where it is thin) up to deep snow
 * (0.8 and more: everything smooth and white, only tall things standing out of it).
 *
 * What lies on it: the clutter buried to the depth (only the tips of what stands taller out of it, dry and pale), the
 * flowers and the standing corn gone (it is winter); the broadleaf trees and the hedges' shrubs bare (their leaves turned
 * to sparse twigs, snow on their limbs' tops); snow on what faces up (roofs, walls' tops, the stones' tops, the spruce's
 * boughs, the kit's houses).
 */
namespace LandScape
{
	namespace
	{
		// (the snow at its true albedo, new snow some 0.9: the art director's first read found it grey, so it is authored as
		// white as fresh snow is, a little blue, never past blue over green 1.03; the shade's blue is the sky's)
		const Tone Fresh = Rgb(250, 252, 255);   // new snow
		const Tone Crust = Rgb(240, 243, 248);   // older, wind-packed
		const Tone FarWhite = Rgb(242, 245, 250);
		const Tone Packed = Rgb(192, 186, 174);  // packed road snow
		const Tone Trodden = Rgb(178, 170, 158); // trampled path snow
		const Tone Slush = Rgb(140, 128, 112);   // grey-brown slush
		const Tone Mud = Rgb(96, 82, 66);        // churned mud and slush
		const Tone Loam = Rgb(74, 62, 50);       // the Ardennes' and the bocage's thrown loam
		const Tone Black = Rgb(48, 42, 36);      // the steppe's chernozem on snow
		const Tone Ice = Rgb(158, 168, 176);
		const Tone Twig = Rgb(132, 120, 114);    // bare twig mass, a grey haze with a little purple-brown
		const Tone ThornTwig = Rgb(86, 72, 68);  // a hedge's bare thorn and hazel
		const Tone Straw = Rgb(186, 164, 116);   // dry grass standing out of the snow
		const Tone Buff = Rgb(160, 150, 120);    // the steppe's dead grass, grey-buff
		const Tone Bracken = Rgb(156, 98, 62);   // dead bracken

		inline double Lum(const Tone& C) { return 0.3 * C.R + 0.59 * C.G + 0.11 * C.B; }
		inline Tone OfByte(const LandField::Rgb8& C) { return Tone{ double(C.R), double(C.G), double(C.B) }; }
		inline bool Leafy(const LandField::Rgb8& C) { return int(C.G) > int(C.R) + 6; }

		/** Keep the triangles Keep says (by its index) and the points they use; the leaf cards' places on their picture kept
		 * with their points. */
		template <typename Fn>
		void KeepTris(LandField::Mesh& M, Fn&& Keep)
		{
			const size_t N = M.Points();
			const bool bUv = M.Uvs.size() == 2 * N && N > 0;
			std::vector<int64_t> Map(N, -1);
			LandField::Mesh Out;
			for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
			{
				if (!Keep(T / 3)) continue;
				for (int K = 0; K < 3; ++K)
				{
					const uint32_t I = M.Triangles[T + static_cast<size_t>(K)];
					if (Map[I] < 0)
					{
						Map[I] = static_cast<int64_t>(Out.Points());
						Out.Positions.insert(Out.Positions.end(), { M.Positions[3 * I], M.Positions[3 * I + 1], M.Positions[3 * I + 2] });
						Out.Normals.insert(Out.Normals.end(), { M.Normals[3 * I], M.Normals[3 * I + 1], M.Normals[3 * I + 2] });
						Out.Colours.push_back(M.Colours[I]);
						if (bUv) Out.Uvs.insert(Out.Uvs.end(), { M.Uvs[2 * I], M.Uvs[2 * I + 1] });
					}
					Out.Triangles.push_back(static_cast<uint32_t>(Map[I]));
				}
			}
			M = std::move(Out);
		}

		bool Named(const std::string& Name, const char* Part) { return Name.find(Part) != std::string::npos; }

		/** A crown's leafy triangles each drawn alone and shrunk about its middle by Shrink: far off, a stipple of twigs
		 * with the sky between, neither one solid mass nor a few big shards. */
		void Stipple(LandField::Mesh& M, double Shrink)
		{
			const size_t N = M.Points();
			if (M.Normals.size() < 3 * N || M.Positions.size() < 3 * N) return;
			const bool bUv = M.Uvs.size() == 2 * N && N > 0;
			const std::vector<uint32_t> Tris = M.Triangles;
			M.Triangles.clear();
			for (size_t T = 0; T + 2 < Tris.size(); T += 3)
			{
				const uint32_t I[3] = { Tris[T], Tris[T + 1], Tris[T + 2] };
				if (!(Leafy(M.Colours[I[0]]) || Leafy(M.Colours[I[1]]) || Leafy(M.Colours[I[2]])))
				{
					M.Triangles.insert(M.Triangles.end(), { I[0], I[1], I[2] });
					continue;
				}
				double Mid[3] = { 0.0, 0.0, 0.0 };
				for (uint32_t V : I)
					for (int C = 0; C < 3; ++C) Mid[C] += M.Positions[3 * V + static_cast<size_t>(C)] / 3.0;
				for (uint32_t V : I)
				{
					const uint32_t New = static_cast<uint32_t>(M.Points());
					for (int C = 0; C < 3; ++C) M.Positions.push_back(static_cast<float>(Mid[C] + Shrink * (M.Positions[3 * V + static_cast<size_t>(C)] - Mid[C])));
					for (int C = 0; C < 3; ++C)
					{
						const float Nc = M.Normals[3 * V + static_cast<size_t>(C)]; // (copied first: the push may move the store)
						M.Normals.push_back(Nc);
					}
					const LandField::Rgb8 Cv = M.Colours[V];
					M.Colours.push_back(Cv);
					if (bUv)
					{
						const float Uu = M.Uvs[2 * V], Vv = M.Uvs[2 * V + 1];
						M.Uvs.insert(M.Uvs.end(), { Uu, Vv });
					}
					M.Triangles.push_back(New);
				}
			}
			KeepTris(M, [](size_t) { return true; }); // (the points no triangle uses any more dropped)
		}
	}

	class Winter final : public Snow
	{
	public:
		Winter(double InCover, Look L, uint64_t InSeed) : Snow(InCover, L, InSeed)
		{
			// How much of the ground the snow hides on the whole, and how much its thin patches and drifts still show:
			// a dusting patchy, deep snow even.
			Lay = Lerp(0.42, 1.0, Smooth(0.06, 0.62, Cover));
			Patchy = 1.0 - 0.85 * Smooth(0.45, 0.9, Cover);
			LayFar = Lerp(0.55, 1.0, Smooth(0.05, 0.5, Cover));
			// (its depth on open ground, metres: a dusting a centimetre or two, a cover of 0.6 ten centimetres, the
			// Ardennes' late December 0.9 some 30, the deepest 45 on the level and drifted deeper)
			Depth = 0.45 * Cover * Cover * Cover;
			// the steppe's open ground scoured more and drifted more (Look::Steppe): thin on the level (10 to 30 cm), its
			// dry grass through it, the crests blown bare in patches even under a full cover; the woods' snow by their kind
			bOpenCountry = L == Look::Steppe;
			if (bOpenCountry)
			{
				Depth = 0.24 * Cover * Cover;
				Patchy = std::max(Patchy, 0.42);
			}
			bBroadleafWoods = L == Look::Bocage || L == Look::Steppe || L == Look::Med;
			Dirt = L == Look::Steppe ? Black : Loam;
			// The snow's own drifts read once onto a 10 m grid over the field and 600 m round it (the paint and the track
			// marks ask them at every step: five noises each is too dear), straight from the noise beyond it.
			Grid.resize(2 * static_cast<size_t>(GNX) * static_cast<size_t>(GNY));
			for (int J = 0; J < GNY; ++J)
				for (int I = 0; I < GNX; ++I)
				{
					double Dr = 0.0, Pk = 0.0;
					Drifts(GX0 + I * GStep, GY0 + J * GStep, Dr, Pk);
					const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I));
					Grid[K] = static_cast<float>(Dr);
					Grid[K + 1] = static_cast<float>(Pk);
				}
		}

		// ------------------------------------------------------------------ the drifts

		/**
		 * The wind's work on the snow, 15 to 70 m: Dr from -1 (a scoured hollow, the snow thin and the light cooler in it)
		 * to 1 (a drift, deep and freshest), drawn out three to one along the wind (west-north-west to east-south-east)
		 * and bent by a broader noise so its lattice never shows; Pk the old wind-packed patches (-1..1), greyer and
		 * warmer, turned another way.
		 */
		void Drifts(double X, double Y, double& Dr, double& Pk) const
		{
			const double WX = 45.0 * Noise(X / 170.0, Y / 170.0, Seed ^ 0xD71), WY = 45.0 * Noise(X / 170.0 + 3.7, Y / 170.0 - 2.9, Seed ^ 0xD72);
			const double PX = X + WX, PY = Y + WY;
			const double A = 0.94 * PX - 0.34 * PY, B = 0.34 * PX + 0.94 * PY;
			Dr = 0.65 * Noise(A / 70.0, B / 24.0, Seed ^ 0xD73) + 0.35 * Noise(A / 34.0 + 1.7, B / 18.0, Seed ^ 0xD74);
			const double U = 0.6 * PX + 0.8 * PY, V = -0.8 * PX + 0.6 * PY;
			Pk = Noise(U / 40.0, V / 40.0, Seed ^ 0xD75);
		}
		/** The drifts here, read between the grid's points. */
		void Read(double X, double Y, double& Dr, double& Pk) const
		{
			const double U = (X - GX0) / GStep, V = (Y - GY0) / GStep;
			if (!(U >= 0.0 && V >= 0.0 && U < GNX - 1.0 && V < GNY - 1.0))
			{
				Drifts(X, Y, Dr, Pk);
				return;
			}
			const int I = static_cast<int>(U), J = static_cast<int>(V);
			const double FU = U - I, FV = V - J;
			const size_t K = 2 * (static_cast<size_t>(J) * static_cast<size_t>(GNX) + static_cast<size_t>(I)), Row = 2 * static_cast<size_t>(GNX);
			auto Lerp2 = [&](size_t C) {
				const double A = Grid[K + C], B = Grid[K + 2 + C], Cc = Grid[K + Row + C], D = Grid[K + Row + 2 + C];
				return (A + (B - A) * FU) + ((Cc + (D - Cc) * FU) - (A + (B - A) * FU)) * FV;
			};
			Dr = Lerp2(0);
			Pk = Lerp2(1);
		}

		// ------------------------------------------------------------------ the ground

		/** Trampled and dirtied along the roads: a strip 2 to 7 m out from the floor beyond the plough's white bank, grey
		 * and broken in patches, where men walked beside the traffic and the carts pulled over. */
		Tone Trampled(const Tone& Out, const Spot& S) const
		{
			if (S.Lane > 7.0 || S.Lane < 0.0) return Out;
			const double Band = Smooth(7.0, 3.0, S.Lane) * Smooth(0.8, 2.0, S.Lane);
			const double Broken = Smooth(-0.3, 0.5, 0.6 * S.N2 + 0.5 * S.N1);
			return Mixed(Out, Mixed(Trodden, Slush, 0.15 + 0.3 * Smooth(-0.2, 0.6, S.N2)), 0.55 * Band * Broken * Smooth(0.05, 0.35, Cover));
		}

		/** The wind's long streaks across the open ground (from the west-north-west): 0 where it leaves the snow, 1 where it
		 * scours it thin. */
		double Scour(double X, double Y) const
		{
			const double A = 0.94 * X - 0.34 * Y, B = 0.34 * X + 0.94 * Y;
			const double N = Noise(A / 70.0, B / 11.0, Seed ^ 0x5C0B);
			return Smooth(bOpenCountry ? 0.05 : 0.25, bOpenCountry ? 0.55 : 0.75, N);
		}

		Tone Over(const Tone& C, const Spot& S) const override
		{
			// the snow's own white, never one flat white: freshest and lightest on the drifts, cooler in the scoured
			// hollows, greyer and a little warmer where it is old and wind-packed (from the grid, not a noise of its own:
			// the track marks ask this colour at every step of every tank)
			double Dr = 0.0, Pk = 0.0;
			Read(S.X, S.Y, Dr, Pk);
			Tone White = Mixed(Fresh, Crust, Clamp01(0.28 + 0.3 * Pk + 0.12 * S.Broad + 0.08 * S.N1));
			White = Mixed(White, Rgb(234, 226, 212), 0.55 * Smooth(0.05, 0.55, Pk));
			White = Mixed(White, Rgb(206, 220, 244), 0.75 * Smooth(-0.05, -0.5, Dr));
			White = Varied(White, 1.0 + 0.12 * Dr, 0.0);
			const double Drift = Smooth(7.0, 1.2, S.Edge);
			// (the wind's scoured patches: the hollows between the drifts, more where the broad noise lifts)
			const double Sc = Smooth(bOpenCountry ? -0.05 : 0.12, bOpenCountry ? 0.45 : 0.62, -0.55 * Dr + 0.45 * S.Broad + 0.25 * S.N1) * (1.0 - Drift);
			double Hide = Lay + Patchy * (-0.2 * S.N2 - 0.12 * S.N1 + 0.3 * Dr + 0.4 * Drift - (bOpenCountry ? 1.35 : 0.32) * Sc);
			Tone Out;
			switch (S.Kind)
			{
			case KLane:
				if (S.Lane < 0.0)
				{
					// A road: the packed snow polished in its ruts, dirty brown-grey down its middle, its sides ploughed up
					// into white banks; a dusting only wets it darker.
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double R1 = (Off - 0.9) / 0.4, R2 = (Off - (S.LaneHalf - 0.9)) / 0.4;
					const double Rut = std::exp(-R1 * R1) + (S.LaneHalf > 2.6 ? 0.6 * std::exp(-R2 * R2) : 0.0);
					// (a village's street trodden and mucky from side to side)
					const double Street = Smooth(3.0, 3.6, S.LaneHalf);
					const double Mid = Smooth(0.7, 0.0, Off);
					Tone Road = Mixed(Packed, Slush, 0.25 + 0.2 * S.N2 + 0.15 * S.N1);
					Road = Mixed(Road, Mud, std::min(0.75, 0.5 * Rut + 0.3 * Mid * (0.6 + 0.4 * S.N2) + 0.25 * Street * (0.5 + 0.5 * S.N1)));
					Road = Mixed(Road, White, 0.85 * Smooth(-0.9, -0.1, S.Lane));
					const Tone Wet = Varied(C, 0.78, 0.0);
					return Mixed(Wet, Road, Smooth(0.05, 0.4, Cover));
				}
				// its verges: the plough's and the shovels' banks, white, a little grey with what the road threw up
				Hide += 0.5 * Smooth(3.0, 0.4, S.Lane);
				Out = Mixed(C, Mixed(White, Packed, 0.3 * Smooth(1.5, 0.0, S.Lane)), Clamp01(Hide));
				Out = Trampled(Out, S);
				break;
			case KVillage:
			case KYard:
			{
				// gardens white, but trodden round the houses and in the yards; muck in the yards
				const double Worn = S.Kind == KYard ? 0.55 + 0.3 * S.N2 : 0.18 + 0.3 * Smooth(0.1, 0.7, S.N2);
				Out = Mixed(C, White, Clamp01(Hide));
				Out = Mixed(Out, Mixed(Trodden, Mud, S.Kind == KYard ? 0.45 : 0.2), Clamp01(Worn) * Smooth(0.05, 0.35, Cover));
				break;
			}
			case KWorks:
			{
				// fieldworks: spoil thrown out black over the snow in heaps, the ground between trodden grey in patches;
				// the snow clean again toward the works' edge (no square of grey on the map)
				const double In = Smooth(2.0, 14.0, S.Edge) * Smooth(-0.45, 0.35, 0.7 * S.N1 + 0.35 * S.Broad + 0.12 * S.N2);
				Out = Mixed(C, White, Clamp01(Hide));
				Out = Mixed(Out, Trodden, 0.4 * In);
				Out = Mixed(Out, Dirt, (0.15 + 0.45 * Smooth(0.0, 0.6, S.N2 + 0.5 * S.N1)) * In);
				break;
			}
			case KWoods:
				// under the trees: patchy, thinnest under the spruce, the floor's litter and needles through it
				Hide = (bBroadleafWoods ? 0.85 : 0.55) * Lay + (0.35 - 0.2 * Cover) * (-0.5 * S.N2 - 0.3 * S.N1) + 0.25 * Drift;
				Out = Mixed(C, Crust, Clamp01(Hide));
				break;
			case KWater:
				// the river and the ponds frozen: grey ice, snow lying on it in patches
				Out = Mixed(C, Ice, Smooth(0.15, 0.5, Cover));
				Out = Mixed(Out, White, Clamp01((Cover - 0.2) * 1.2 + 0.2 * S.N1));
				return Out;
			case KMarsh:
				// frozen marsh: snow between the reeds' tussocks, the reeds straw-pale through it
				Out = Mixed(Mixed(C, Straw, 0.4), White, Clamp01(0.82 * Hide - 0.1 * S.N2));
				break;
			case KRough:
				// rough ground's tussocks and scrub hold the snow unevenly; a balka's floor drifted full
				Hide += Patchy * (bOpenCountry ? 0.25 : -0.08 * S.N2);
				Out = Mixed(bOpenCountry ? Mixed(C, Buff, 0.6) : C, White, Clamp01(Hide));
				break;
			default:
				// the open field: smooth white; where the cover is thin, the ground through it (on the steppe its dead grass
				// grey-buff in the scoured patches, no plough's lands: the colour's bands taken out)
				// (rice country's frozen plots, painted dark grey ice by their region: only a dusting holds on them, the art
				// director, 06:57Z)
				if (Under == Look::Rice) Hide -= 0.72 * Smooth(66.0, 50.0, Lum(C));
				Out = Mixed(bOpenCountry ? Mixed(C, Buff, 0.7) : C, White, Clamp01(Hide));
				Out = Trampled(Out, S);
				break;
			}
			// What men and vehicles have done to it: churned tracks, grey paths, mud at the gateways and round the houses; the
			// tanks' tracks whole, the men's ground in islands where they gathered and crossed, the snow between them left (a
			// wash over the whole of their ground reads as a figure from above)
			const double Marked = Smooth(0.03, 0.3, Cover);
			const double Patches = Smooth(0.0, 0.55, 0.75 * S.N1 + 0.45 * S.Broad + 0.15 * S.N2);
			if (S.bVehicle) Out = Mixed(Out, Mixed(Slush, Mud, 0.45 + 0.35 * S.N2), 0.72 * Marked);
			if (S.bMan) Out = Mixed(Out, S.bVehicle ? Mud : Mixed(Trodden, Slush, 0.2 + 0.25 * S.N2), (S.bVehicle ? 0.3 : 0.5 * Patches) * Marked);
			if (S.bGate) Out = Mixed(Out, Mud, 0.55 * Marked);
			if (S.bNear) Out = Mixed(Out, Mixed(Trodden, Mud, 0.3 + 0.3 * S.N2), 0.45 * Marked);
			return Out;
		}

		Tone OverFar(const Tone& C, const Spot& S, bool bWoods) const override
		{
			if (bWoods)
			{
				// A wood far off: the spruce dark with snow on its boughs; the broadleaf bare, a purplish-brown haze of twigs
				// with the snow seen through it.
				const bool bConifer = !bBroadleafWoods && C.G > C.R + 3.0;
				if (bConifer) return Mixed(C, FarWhite, (0.14 + 0.1 * S.N1) * Smooth(0.05, 0.6, Cover));
				// (from above the snow on the floor shows through the bare crowns: a grey wood, not a brown one)
				const Tone Haze = Mixed(Varied(Twig, 0.9 + 0.1 * S.N1, 0.0), C, 0.2);
				return Mixed(Haze, FarWhite, (0.5 + 0.12 * S.N1) * Smooth(0.05, 0.7, Cover) + 0.1);
			}
			// The open country white, its parcels a shade apart; what is dark in it (hedges, a wood's edge, a village)
			// holding less of the snow, so the land's pattern still reads.
			const double Dark = Smooth(115.0, 60.0, Lum(C));
			const double Hide = LayFar * (1.0 - 0.45 * Dark) - 0.08 * Patchy * S.N1;
			// (the drifts out to 600 m round the field: the near ring not one flat white)
			double Dr = 0.0, Pk = 0.0;
			if (S.X > GX0 && S.Y > GY0 && S.X < GX0 + GStep * (GNX - 1) && S.Y < GY0 + GStep * (GNY - 1)) Read(S.X, S.Y, Dr, Pk);
			Tone White = Varied(FarWhite, 1.0 + 0.025 * S.Broad + 0.02 * S.N1 + 0.05 * Dr, 0.015 * Pk);
			White = Mixed(White, Rgb(212, 224, 245), 0.55 * Smooth(-0.05, -0.65, Dr));
			return Mixed(C, White, Clamp01(Hide));
		}

		// ------------------------------------------------------------------ the clutter

		/** The metres of a clutter thing as built (its mesh's height), for what stands out of the snow. */
		double Height(LandField::Base K) const
		{
			using B = LandField::Base;
			switch (K)
			{
			case B::Tuft: return 0.4;
			case B::TallGrass: return 1.0;
			case B::Fern: return 0.5;
			case B::Stone: return 0.12;
			case B::Stubble: return 0.13;
			case B::Sward: return 0.1;
			case B::Rosette: return 0.08;
			default: break;
			}
			if (K >= B::Clut0 && K <= B::Clut5)
			{
				// (the farmland's own: grass, stubble, dry stalks, clods, the young corn, a fence post; another ground's taken
				// as grass)
				if (Under == Look::Farmland)
				{
					static const double Farm[6] = { 0.38, 0.14, 0.85, 0.12, 0.12, 1.25 };
					return Farm[static_cast<int>(K) - static_cast<int>(B::Clut0)];
				}
				return 0.35;
			}
			return 0.3;
		}

		bool Clutter(LandField::Placed& P, const Spot& S) const override
		{
			using B = LandField::Base;
			// (it is winter: no flowers, no corn standing; the steppe's dead grass sparse, a fine stubble through the snow)
			if (P.Kind == B::Poppies || P.Kind == B::Flowers || P.Kind == B::Stalks || P.Kind == B::Wheat) return false;
			if (S.Kind == KWater) return false;
			if (P.Kind == B::Tuft || P.Kind == B::TallGrass || (Under == Look::Farmland && P.Kind == B::Clut0))
			{
				// the dead grass bent the wind's way (to the east-south-east), its snow tail in its lee: every tuft turned
				// the same way, give or take; its tint the dead grass's grey-buff, not July's straw
				const uint64_t Ht = Hash(Seed ^ 0x57A1, static_cast<uint64_t>(std::llround(P.X * 5.0)), static_cast<uint64_t>(std::llround(P.Y * 5.0)));
				const double UThin = Unit(Ht), UTurn = Unit(Hash(Ht, 1)), UShade = Unit(Hash(Ht, 2));
				if (bOpenCountry && UThin > 0.7) return false;
				if (bOpenCountry) P.Scale = static_cast<float>(P.Scale * 0.7);
				P.Yaw = static_cast<float>(WindYaw + 0.6 * (UTurn - 0.5));
				const double Shade = 0.94 + 0.12 * UShade;
				P.Tint = Tint(Shade, Shade, Shade * 1.02);
			}
			if (P.Kind == B::Trodden)
			{
				// a beaten path through it: the snow trodden down to a grey-brown slush and frozen mud
				P.Tint = Byte(Tone{ P.Tint.R * 1.22, P.Tint.G * 1.24, P.Tint.B * 1.36 });
				return true;
			}
			const double High = Height(P.Kind) * P.ScaleZ * (P.Kind == B::Stone ? P.Scale : 1.0);
			// the depth here: deeper in the lee of the field's edge, the lane's banks; scoured thin in the wind's streaks;
			// thin under the trees and on the trodden ways; each metre its own hummock or hollow
			double D = Depth;
			D *= 1.0 + 0.7 * Smooth(6.0, 1.2, S.Edge);
			D *= 1.0 - (bOpenCountry ? 0.6 : 0.45) * Scour(P.X, P.Y);
			if (S.Kind == KWoods) D *= bBroadleafWoods ? 0.75 : 0.5;
			if (S.bVehicle) D *= 0.3;
			else if (S.bMan) D *= 0.55;
			// (the snow's own hummocks and hollows, a few metres across: what shows, shows in clumps)
			const double U = Unit(Hash(Seed, static_cast<uint64_t>(std::llround(P.X * 7.0)), static_cast<uint64_t>(std::llround(P.Y * 7.0))));
			const double Hum = Noise(P.X / 3.5, P.Y / 3.5, Seed ^ 0x40C);
			D *= 0.5 + 0.7 * Smooth(-0.55, 0.55, Hum) + 1.3 * Smooth(0.3, 0.75, Hum) + 0.3 * (U - 0.5);
			const double Out = High - D;
			if (Out < 0.035 || Out < 0.12 * High) return false;
			// What stands out of it: its foot hidden (drawn only from the snow up), dry and pale, frosted.
			const double Show = Out / std::max(1e-3, High);
			P.ScaleZ = static_cast<float>(P.ScaleZ * std::max(0.2, Show));
			if (P.Kind == B::Sward || P.Kind == B::Rosette)
			{
				// (the short grass and the docks only through a dusting, few)
				if (U > 1.0 - 0.6 * Show) return false;
			}
			if (P.Kind == B::Stone) P.Scale = static_cast<float>(P.Scale * (0.75 + 0.25 * Show));
			return true;
		}

		// ------------------------------------------------------------------ what stands on it

		/** Snow on what faces up: Share of the way to white where its point faces straight up, none below Lo. */
		void Cap(LandField::Mesh& M, double Lo, double Share, const Tone& White) const
		{
			const size_t N = M.Points();
			if (M.Normals.size() < 3 * N) return;
			for (size_t K = 0; K < N; ++K)
			{
				const double Z = M.Normals[3 * K + 2];
				if (Z <= Lo) continue;
				const double T = Share * Smooth(Lo, Lo + 0.35, Z);
				M.Colours[K] = Byte(Mixed(OfByte(M.Colours[K]), White, T));
			}
		}

		/** A tuft's snow tail: a low ridge of drifted snow from its foot some half a metre downwind (its mesh's +X, the way the
		 * wind bends it), its two flanks catching and losing the light. */
		void Tail(LandField::Mesh& M) const
		{
			if (M.Normals.size() < M.Positions.size() || M.Positions.size() != 3 * M.Points()) return;
			const uint32_t First = static_cast<uint32_t>(M.Points());
			const float Pts[5][3] = { { -0.03f, -0.08f, 0.0f }, { -0.03f, 0.08f, 0.0f }, { 0.06f, 0.0f, 0.045f }, { 0.48f, 0.0f, 0.0f }, { 0.06f, 0.0f, 0.045f } };
			const float Nrm[5][3] = { { -0.15f, -0.35f, 0.92f }, { -0.15f, 0.35f, 0.92f }, { 0.0f, -0.3f, 0.95f }, { 0.1f, 0.0f, 0.99f }, { 0.0f, 0.3f, 0.95f } };
			for (int K = 0; K < 5; ++K)
			{
				for (int C = 0; C < 3; ++C)
				{
					M.Positions.push_back(Pts[K][C]);
					M.Normals.push_back(Nrm[K][C]);
				}
				M.Colours.push_back(Byte(Fresh));
			}
			// (the south flank, the north flank, the front; both windings, the painter culling either)
			const uint32_t Tri[3][3] = { { 0, 3, 2 }, { 1, 4, 3 }, { 0, 2, 1 } };
			for (const auto& T : Tri)
				for (int Way = 0; Way < 2; ++Way)
					for (int C = 0; C < 3; ++C) M.Triangles.push_back(First + T[Way == 0 ? C : 2 - C]);
		}

		/** The snow lying on a spruce's boughs as soft pads, not flat wedges: its points' normals turned up and out from the
		 * trunk (the light rolling over a rounded top to a shaded lip), the bough under it left as built. */
		void Pads(LandField::Mesh& M) const
		{
			const size_t N = M.Points();
			if (M.Normals.size() < 3 * N || M.Positions.size() < 3 * N) return;
			double Reach = 0.1;
			for (size_t K = 0; K < N; ++K) Reach = std::max(Reach, std::hypot(double(M.Positions[3 * K]), double(M.Positions[3 * K + 1])));
			for (size_t K = 0; K < N; ++K)
			{
				if (Lum(OfByte(M.Colours[K])) < 190.0) continue;
				const double X = M.Positions[3 * K], Y = M.Positions[3 * K + 1], R = std::max(1e-3, std::hypot(X, Y));
				const double Out = 0.55 * std::min(1.0, R / Reach);
				double NX = 0.3 * M.Normals[3 * K] + Out * X / R, NY = 0.3 * M.Normals[3 * K + 1] + Out * Y / R, NZ = 0.3 * M.Normals[3 * K + 2] + 0.8;
				const double L = std::max(1e-6, std::sqrt(NX * NX + NY * NY + NZ * NZ));
				M.Normals[3 * K] = static_cast<float>(NX / L);
				M.Normals[3 * K + 1] = static_cast<float>(NY / L);
				M.Normals[3 * K + 2] = static_cast<float>(NZ / L);
			}
		}

		/** The snow on a laden spruce's boughs with soft edges, near (the art director's Mac pass 1, B4 shot 3: under about
		 * 15 m the boughs broke into hard black and white shards). The model paints each triangle snow or bough whole, so
		 * where the two meet the edge is a hard line across a wedge metres long. Here each place on the boughs takes the
		 * share of snow of the triangles meeting there (by their area): a snow wedge stays white where only snow meets and
		 * greys toward the bough along its edges, the bough lightens toward the snow; the normals go the same share of the
		 * way to the pad's (Pads). The bough's own
		 * green a little frosted, the snow's light thrown up into it, so it is never near-black beside the snow. */
		void SoftPads(LandField::Mesh& M) const
		{
			const size_t N = M.Points();
			if (N == 0 || M.Normals.size() < 3 * N || M.Positions.size() < 3 * N || M.Colours.size() < N) return;
			const std::vector<float> Was = M.Normals;
			Pads(M);
			const std::vector<float> Pad = M.Normals;
			// (the places: points at the same spot, to a millimetre, are one place)
			std::vector<uint32_t> PlaceOf(N);
			{
				std::vector<std::pair<uint64_t, uint32_t>> Keys(N);
				for (size_t K = 0; K < N; ++K)
				{
					const auto Q = [&M, K](int C) { return static_cast<uint64_t>(static_cast<int64_t>(std::lround(M.Positions[3 * K + C] * 1000.0)) & 0x1FFFFF); };
					Keys[K] = { Q(0) | Q(1) << 21 | Q(2) << 42, static_cast<uint32_t>(K) };
				}
				std::sort(Keys.begin(), Keys.end());
				uint32_t Place = 0;
				for (size_t K = 0; K < N; ++K)
				{
					if (K > 0 && Keys[K].first != Keys[K - 1].first) ++Place;
					PlaceOf[Keys[K].second] = Place;
				}
			}
			const size_t Places = N == 0 ? 0 : *std::max_element(PlaceOf.begin(), PlaceOf.end()) + 1;
			struct Sum
			{
				double Snow = 0.0, All = 0.0;
				Tone White{}, Green{};
				double WhiteW = 0.0, GreenW = 0.0;
			};
			std::vector<Sum> At(Places);
			const auto Snowy = [&M](uint32_t K) { return Lum(OfByte(M.Colours[K])) >= 190.0; };
			for (size_t T = 0; T + 2 < M.Triangles.size(); T += 3)
			{
				const uint32_t I[3] = { M.Triangles[T], M.Triangles[T + 1], M.Triangles[T + 2] };
				if (I[0] >= N || I[1] >= N || I[2] >= N) continue;
				const float* A = &M.Positions[3 * I[0]];
				const float* B = &M.Positions[3 * I[1]];
				const float* C = &M.Positions[3 * I[2]];
				const double UX = B[0] - A[0], UY = B[1] - A[1], UZ = B[2] - A[2], VX = C[0] - A[0], VY = C[1] - A[1], VZ = C[2] - A[2];
				const double Area = 0.5 * std::sqrt((UY * VZ - UZ * VY) * (UY * VZ - UZ * VY) + (UZ * VX - UX * VZ) * (UZ * VX - UX * VZ) + (UX * VY - UY * VX) * (UX * VY - UY * VX)) + 1e-6;
				const bool bSnow = (Snowy(I[0]) ? 1 : 0) + (Snowy(I[1]) ? 1 : 0) + (Snowy(I[2]) ? 1 : 0) >= 2;
				for (const uint32_t K : I)
				{
					Sum& S = At[PlaceOf[K]];
					S.All += Area;
					const Tone C3 = OfByte(M.Colours[K]);
					if (bSnow)
					{
						S.Snow += Area;
						S.White = Mixed(S.White, C3, Area / (S.WhiteW + Area));
						S.WhiteW += Area;
					}
					else
					{
						S.Green = Mixed(S.Green, C3, Area / (S.GreenW + Area));
						S.GreenW += Area;
					}
				}
			}
			const Tone Frost = Rgb(118, 130, 126), Bough = Rgb(45, 69, 53), Lying = Rgb(230, 234, 238);
			for (size_t K = 0; K < N; ++K)
			{
				const Sum& S = At[PlaceOf[K]];
				if (S.All <= 0.0) continue;
				// (a snow point keeps most of its white, a bough's point takes a little of the snow beside it: the step
				// between them a third of what it was, each side shading into it)
				const double Share = Snowy(static_cast<uint32_t>(K)) ? 0.5 + 0.5 * S.Snow / S.All : 0.4 * S.Snow / S.All;
				const Tone Green = Mixed(S.GreenW > 0.0 ? S.Green : Bough, Frost, 0.3);
				const Tone White = S.WhiteW > 0.0 ? S.White : Lying;
				M.Colours[K] = Byte(Mixed(Green, White, Share));
				double NX = Lerp(Was[3 * K], Pad[3 * K], Share), NY = Lerp(Was[3 * K + 1], Pad[3 * K + 1], Share), NZ = Lerp(Was[3 * K + 2], Pad[3 * K + 2], Share);
				const double L = std::sqrt(NX * NX + NY * NY + NZ * NZ);
				if (L < 1e-6) continue;
				M.Normals[3 * K] = static_cast<float>(NX / L);
				M.Normals[3 * K + 1] = static_cast<float>(NY / L);
				M.Normals[3 * K + 2] = static_cast<float>(NZ / L);
			}
		}

		/** A broadleaf crown bare for the winter: its leaves gone, a share of the cards and sprigs left as the twig mass
		 * (their places kept, recoloured), the snow on its limbs' tops. Dense for a hedge's thorn, sparse for a tree. */
		void Bare(LandField::BaseMesh& M, const Tone& TwigOf, double Keep, double CapShare, uint64_t Salt, double Brown = 0.0) const
		{
			auto Twiggy = [&TwigOf](LandField::Mesh& Me) {
				for (LandField::Rgb8& C : Me.Colours)
				{
					const double L = std::clamp(Lum(OfByte(C)) / 88.0, 0.62, 1.3);
					C = Byte(Varied(TwigOf, L, 0.0));
				}
			};
			// the heart's clusters of leaves in the near mesh: gone (the limbs and the trunk stay)
			const std::vector<LandField::Rgb8>& NC = M.Near.Colours;
			const std::vector<uint32_t>& NT = M.Near.Triangles;
			KeepTris(M.Near, [&NC, &NT](size_t T) { return !(Leafy(NC[NT[3 * T]]) && Leafy(NC[NT[3 * T + 1]]) && Leafy(NC[NT[3 * T + 2]])); });
			// the leaf cards: a card in Keep left, as its twigs; the sprigs the same
			const uint64_t S = Seed ^ Salt;
			KeepTris(M.Cards, [S, Keep](size_t T) { return Unit(Hash(S, static_cast<uint64_t>(T / 2))) < Keep; });
			KeepTris(M.Sprigs, [S, Keep](size_t T) { return Unit(Hash(S ^ 0x5B, static_cast<uint64_t>(T / 8))) < Keep; });
			Twiggy(M.Cards);
			Twiggy(M.Sprigs);
			if (Brown > 0.0)
			{
				// (a share of the cards left as the dead leaves an oak holds, brown)
				for (LandField::Mesh* Me : { &M.Cards, &M.Sprigs })
				{
					const size_t Per = Me == &M.Cards ? 4 : 8;
					for (size_t P = 0; P < Me->Colours.size(); ++P)
						if (Unit(Hash(S ^ 0xB0, static_cast<uint64_t>(P / Per))) < Brown) Me->Colours[P] = Byte(Varied(Rgb(126, 96, 74), 0.9 + 0.2 * Unit(Hash(S ^ 0xB1, static_cast<uint64_t>(P / Per))), 0.0));
				}
			}
			// far off: its crown a stipple of the twigs' haze, every piece of it there but each drawn in small with the sky
			// between (a few of its big triangles left alone read as paper shards; all of them whole, as a leafy crown),
			// the soft grey-violet of a bare crown at a distance
			Stipple(M.Far, 0.55);
			// (with no snow lying, the leafless months' crowns: no frost's grey-violet, which reads as blossom on green
			// fields, but the twigs' own dark brown-grey a little softened by the air)
			const Tone Haze = Cover > 0.0 ? Rgb(156, 150, 158) : Rgb(108, 98, 92);
			for (LandField::Rgb8& C : M.Far.Colours)
				if (Leafy(C)) C = Byte(Varied(Mixed(TwigOf, Haze, 0.4), std::clamp(Lum(OfByte(C)) / 88.0, 0.9, 1.05), 0.0));
			const double Sn = CapShare * Smooth(0.05, 0.6, Cover);
			Cap(M.Near, 0.45, Sn, Crust);
			Cap(M.Cards, 0.55, 0.3 * Sn, Crust);
			Cap(M.Sprigs, 0.55, 0.3 * Sn, Crust);
			Cap(M.Far, 0.6, 0.25 * Sn, Crust);
		}

		/** A kit tree (its leaves solid in its own mesh, no cards) bare: most of its leaves gone near, the rest and the far
		 * crown the twigs' haze. */
		void BareKit(LandField::BaseMesh& M, uint64_t Salt) const
		{
			const uint64_t S = Seed ^ Salt;
			const std::vector<LandField::Rgb8>& NC = M.Near.Colours;
			const std::vector<uint32_t>& NT = M.Near.Triangles;
			KeepTris(M.Near, [&NC, &NT, S](size_t T) {
				const bool bLeaf = Leafy(NC[NT[3 * T]]) && Leafy(NC[NT[3 * T + 1]]) && Leafy(NC[NT[3 * T + 2]]);
				return !bLeaf || Unit(Hash(S, static_cast<uint64_t>(T))) < 0.3;
			});
			for (LandField::Mesh* Me : { &M.Near, &M.Far })
				for (LandField::Rgb8& C : Me->Colours)
					if (Leafy(C)) C = Byte(Varied(Twig, std::clamp(Lum(OfByte(C)) / 88.0, 0.7, 1.2), 0.0));
			const double Sn = 0.6 * Smooth(0.05, 0.6, Cover);
			Cap(M.Near, 0.5, Sn, Crust);
			Cap(M.Far, 0.5, Sn, Crust);
		}

		void Leafless(LandField::Base B, LandField::BaseMesh& M) const override
		{
			// (as Base bares them, the cover nought: no white on the limbs; a kit's middle level, still in leaf, dropped; the
			// twigs browner than under snow, where the frost greys them: a grey-violet twig mass on green fields reads as
			// blossom)
			using K = LandField::Base;
			const Tone Brown = Rgb(106, 94, 84), Thorn = Rgb(72, 60, 52);
			const bool bFrame = B == K::Oak || B == K::Beech || B == K::Elm || B == K::Ash || B == K::Pollard || B == K::Apple || B == K::ShrubA || B == K::ShrubB || B == K::ShrubC || B == K::Bramble || B == K::Birch;
			if (bFrame) M.Mid = LandField::Mesh();
			switch (B)
			{
			case K::Oak:
			case K::Beech:
				Bare(M, Brown, 0.2, 0.0, 0x0A + static_cast<uint64_t>(B), 0.08);
				return;
			case K::Elm:
			case K::Ash:
			case K::Pollard:
			case K::Apple:
				Bare(M, Brown, 0.16, 0.0, 0x0A + static_cast<uint64_t>(B));
				return;
			case K::ShrubA:
			case K::ShrubB:
			case K::Bramble:
				Bare(M, Thorn, 0.62, 0.0, 0x1A + static_cast<uint64_t>(B));
				return;
			case K::ShrubC:
				Bare(M, Mixed(Thorn, Rgb(40, 56, 40), 0.5), 0.7, 0.0, 0x1A + static_cast<uint64_t>(B));
				return;
			case K::Birch:
				if (M.Near.Tris() > 200) BareKit(M, 0x2B);
				return;
			default: break;
			}
			if (B >= K::Tuft) return;
			static const char* const Deciduous[] = { "Birch", "Oak", "Beech", "Poplar", "Willow", "Elm", "Lime", "Chestnut", "Apple", "Plane", "Vine", "Walnut", "Almond", "Fig" };
			for (const char* D : Deciduous)
				if (Named(M.Name, D) && !Named(M.Name, "Bare"))
				{
					M.Mid = LandField::Mesh();
					BareKit(M, 0x3C);
					return;
				}
		}

		void Base(LandField::Base B, LandField::BaseMesh& M) const override
		{
			using K = LandField::Base;
			const double Sn = Smooth(0.03, 0.5, Cover);
			switch (B)
			{
			case K::Oak:
			case K::Beech:
				// (the young oaks and beeches hold a few dead leaves, brown, through the winter)
				Bare(M, Twig, 0.2, 0.7, 0x0A + static_cast<uint64_t>(B), 0.08);
				return;
			case K::Elm:
			case K::Ash:
			case K::Pollard:
			case K::Apple:
				Bare(M, Twig, 0.16, 0.7, 0x0A + static_cast<uint64_t>(B));
				return;
			case K::ShrubA:
			case K::ShrubB:
			case K::Bramble:
				// the hedge's thorn, hazel and bramble: a dense dark mass of bare twigs, snow along its top
				Bare(M, ThornTwig, 0.62, 0.9, 0x1A + static_cast<uint64_t>(B));
				return;
			case K::ShrubC:
				// holly and elder: the holly keeps its leaves, the elder bare
				Bare(M, Mixed(ThornTwig, Rgb(40, 56, 40), 0.5), 0.7, 0.9, 0x1A + static_cast<uint64_t>(B));
				return;
			case K::Birch:
				// the steppe's birch (the kit's, not the plain stand-in): its leaves gone, its white bark left
				if (M.Near.Tris() > 200) BareKit(M, 0x2B);
				return;
			case K::Tuft:
			case K::TallGrass:
			{
				// the grass that stands out of the snow: winter-dry, straw (the steppe's grey-buff), not July's green; bent
				// over by the wind
				const Tone Dead = bOpenCountry ? Rgb(150, 144, 126) : Mixed(Straw, Buff, 0.55);
				for (LandField::Rgb8& C : M.Near.Colours) C = Byte(Mixed(OfByte(C), Varied(Dead, std::clamp(Lum(OfByte(C)) / 100.0, 0.75, 1.12), 0.0), 0.95));
				for (LandField::Mesh* Me : { &M.Near, &M.Far })
					for (size_t P = 0; P + 2 < Me->Positions.size(); P += 3)
					{
						const double Z = Me->Positions[P + 2];
						Me->Positions[P] = static_cast<float>(Me->Positions[P] + (B == K::Tuft ? 0.6 : 0.25) * Z * Z);
					}
				for (LandField::Rgb8& C : M.Far.Colours) C = Byte(Mixed(OfByte(C), Dead, 0.85));
				if (B == K::Tuft) Tail(M.Near);
				return;
			}
			case K::Fern:
				for (LandField::Rgb8& C : M.Near.Colours) C = Byte(Mixed(OfByte(C), Varied(Bracken, std::clamp(Lum(OfByte(C)) / 90.0, 0.7, 1.15), 0.0), 0.8));
				return;
			case K::Rosette:
			case K::Stubble:
				for (LandField::Rgb8& C : M.Near.Colours) C = Byte(Mixed(OfByte(C), Straw, 0.45));
				return;
			case K::Stone:
				Cap(M.Near, 0.35, 0.85 * Sn, Fresh);
				return;
			default: break;
			}
			// (the farmland's grass tuft, built dead and bent by its region: its snow tail)
			if (B == K::Clut0 && Under == Look::Farmland) Tail(M.Near);
			if (B >= K::Tuft) return; // (the other clutter as built; a region's own clutter is its to dress)
			// A kit model or a prop: a deciduous tree bare; an evergreen's boughs and everything's roof under snow (a model
			// already laden left as it is).
			if (Named(M.Name, "Snow"))
			{
				SoftPads(M.Near);
				Pads(M.Far);
				return;
			}
			static const char* const Deciduous[] = { "Birch", "Oak", "Beech", "Poplar", "Willow", "Elm", "Lime", "Chestnut", "Apple", "Plane", "Vine", "Walnut", "Almond", "Fig" };
			for (const char* D : Deciduous)
				if (Named(M.Name, D) && !Named(M.Name, "Bare"))
				{
					BareKit(M, 0x3C);
					return;
				}
			const bool bBough = Named(M.Name, "Spruce") || Named(M.Name, "Pine") || Named(M.Name, "Fir") || Named(M.Name, "Cypress");
			Cap(M.Near, bBough ? 0.2 : 0.5, (bBough ? 0.8 : 0.92) * Sn, Fresh);
			Cap(M.Far, bBough ? 0.2 : 0.5, (bBough ? 0.75 : 0.9) * Sn, Fresh);
			if (bBough)
			{
				Pads(M.Near);
				Pads(M.Far);
			}
			Cap(M.Cards, 0.45, 0.6 * Sn, Fresh);
			Cap(M.Sprigs, 0.45, 0.6 * Sn, Fresh);
		}

		void Built(LandField::Mesh& M) const override
		{
			// Snow on the roofs and the walls' tops: the steep pitches shed some in streaks, a slate showing through, the
			// gentle ones and the copings white.
			const size_t N = M.Points();
			if (M.Normals.size() < 3 * N || M.Positions.size() < 3 * N) return;
			const double Sn = Smooth(0.03, 0.45, Cover);
			for (size_t K = 0; K < N; ++K)
			{
				const double Z = M.Normals[3 * K + 2];
				if (Z <= 0.45) continue;
				const double X = M.Positions[3 * K], Y = M.Positions[3 * K + 1];
				const double Streak = Noise(X / 1.3, Y / 1.3, Seed ^ 0xB0F);
				const double Pitch = Smooth(0.45, 0.85, Z);
				const double T = Sn * Clamp01(Pitch * (0.92 + 0.25 * Streak) - (1.0 - Cover) * 0.25 * (1.0 - Pitch));
				M.Colours[K] = Byte(Mixed(OfByte(M.Colours[K]), Fresh, T));
			}
		}

	private:
		double Lay = 1.0, Patchy = 0.0, LayFar = 1.0, Depth = 0.0;
		/** The way the wind blows (to the east-south-east, along Scour's streaks), radians from east. */
		static constexpr double WindYaw = -0.347;
		/** The drifts' grid: 10 m, from 600 m south-west of the field's corner to 600 m beyond a 3 by 2 km field. */
		static constexpr double GStep = 10.0, GX0 = -600.0, GY0 = -600.0;
		static constexpr int GNX = 421, GNY = 321;
		std::vector<float> Grid;
		bool bOpenCountry = false, bBroadleafWoods = true;
		Tone Dirt = Loam;
	};

	std::unique_ptr<Snow> MakeSnowCover(double Cover, Look L, uint64_t S)
	{
		return std::make_unique<Winter>(Cover, L, S);
	}
}
