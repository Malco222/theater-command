#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>
#include <cstring>

/**
 * Snow over any ground (the Ardennes in December, Moscow, the Don, Karelia, Cassino's mountains): the look-targets'
 * winter (sections 0, 5 and 7). Snow sets the light: the open ground white, nearly as bright as an overcast sky, and the
 * contrast all in the dirt on it and in the dark things standing out of it: the roads' packed snow darker than the fields,
 * dirty down their middles and churned in their ruts; the trodden paths grey; mud at the gateways, in the yards and round
 * every house; black earth thrown up on the fieldworks; the dark woods (spruce with snow on its boughs, the bare broadleaf
 * a purplish-brown haze of twigs). The snow lies deeper in the lee of every field's edge (the hedge, the fence, the
 * bank) and thinner where the wind scours it off in long streaks; under trees it is patchy. By its Cover: a dusting (0.1
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
		// (the art director's winter colours, section 7, authored a touch warmer than the table as the brief asks; the
		// snow's white a little under the sunlit figure, between it and the overcast's, so the grade has room)
		const Tone Fresh = Rgb(240, 243, 248);   // new snow, a little blue
		const Tone Crust = Rgb(226, 229, 232);   // older, wind-packed, a little grey
		const Tone FarWhite = Rgb(230, 233, 238);
		const Tone Packed = Rgb(192, 186, 174);  // packed road snow
		const Tone Trodden = Rgb(178, 170, 158); // trampled path snow
		const Tone Slush = Rgb(140, 128, 112);   // grey-brown slush
		const Tone Mud = Rgb(96, 82, 66);        // churned mud and slush
		const Tone Loam = Rgb(74, 62, 50);       // the Ardennes' and the bocage's thrown loam
		const Tone Black = Rgb(48, 42, 36);      // the steppe's chernozem on snow
		const Tone Ice = Rgb(158, 168, 176);
		const Tone Twig = Rgb(120, 100, 94);     // bare twig mass, purplish brown
		const Tone ThornTwig = Rgb(86, 72, 68);  // a hedge's bare thorn and hazel
		const Tone Straw = Rgb(186, 164, 116);   // dry grass standing out of the snow
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
		}

		// ------------------------------------------------------------------ the ground

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
			// the snow's own white: fresh, a little greyer where it is old and wind-packed (the 30 m patches)
			const Tone White = Mixed(Fresh, Crust, 0.35 + 0.35 * S.N1 + 0.15 * S.Broad);
			const double Drift = Smooth(7.0, 1.2, S.Edge);
			// (the wind's scoured patches from the Spot's own noises, not a noise of its own: the track marks ask this colour
			// at every step of every tank)
			const double Sc = Smooth(0.12, 0.62, 0.6 * S.N1 + 0.55 * S.Broad - 0.25 * S.N2) * (1.0 - Drift);
			double Hide = Lay + Patchy * (-0.26 * S.N2 - 0.16 * S.N1 + 0.4 * Drift - (bOpenCountry ? 0.5 : 0.32) * Sc);
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
				// fieldworks: spoil thrown out black over the snow, the ground between trodden grey
				Out = Mixed(C, White, Clamp01(Hide));
				Out = Mixed(Out, Trodden, 0.35);
				Out = Mixed(Out, Dirt, 0.25 + 0.35 * Smooth(-0.2, 0.6, S.N2 + 0.4 * S.N1));
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
				Out = Mixed(C, White, Clamp01(Hide));
				break;
			default:
				Out = Mixed(C, White, Clamp01(Hide));
				break;
			}
			// What men and vehicles have done to it: churned tracks, grey paths, mud at the gateways and round the houses.
			const double Marked = Smooth(0.03, 0.3, Cover);
			if (S.bVehicle) Out = Mixed(Out, Mixed(Slush, Mud, 0.45 + 0.35 * S.N2), 0.72 * Marked);
			if (S.bMan) Out = Mixed(Out, S.bVehicle ? Mud : Mixed(Trodden, Slush, 0.25 + 0.25 * S.N2), (S.bVehicle ? 0.3 : 0.5) * Marked);
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
				const Tone Haze = Mixed(Varied(Twig, 0.9 + 0.1 * S.N1, 0.0), C, 0.2);
				return Mixed(Haze, FarWhite, (0.28 + 0.12 * S.N1) * Smooth(0.05, 0.7, Cover) + 0.1);
			}
			// The open country white, its parcels a shade apart; what is dark in it (hedges, a wood's edge, a village)
			// holding less of the snow, so the land's pattern still reads.
			const double Dark = Smooth(115.0, 60.0, Lum(C));
			const double Hide = LayFar * (1.0 - 0.45 * Dark) - 0.08 * Patchy * S.N1;
			const Tone White = Varied(FarWhite, 1.0 + 0.025 * S.Broad + 0.02 * S.N1, 0.0);
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
			case B::Wheat: return 1.0;
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
			// (it is winter: no flowers; of the corn only the stalks the harvest never took, a few, grey and broken)
			if (P.Kind == B::Poppies || P.Kind == B::Flowers || P.Kind == B::Stalks) return false;
			if (S.Kind == KWater) return false;
			if (P.Kind == B::Wheat)
			{
				const double UKeep = Unit(Hash(Seed ^ 0x57A1, static_cast<uint64_t>(std::llround(P.X * 5.0)), static_cast<uint64_t>(std::llround(P.Y * 5.0))));
				const double Few = bOpenCountry ? 0.035 : 0.015;
				if (UKeep > Few) return false;
				// (the gold gone to a weathered grey-brown; broken off lower)
				P.Tint = Byte(Tone{ P.Tint.R * 0.67, P.Tint.G * 0.74, P.Tint.B * 1.1 });
				P.ScaleZ = static_cast<float>(P.ScaleZ * (0.5 + 0.3 * UKeep / Few));
				P.Scale = static_cast<float>(P.Scale * 0.7);
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

		/** A broadleaf crown bare for the winter: its leaves gone, a share of the cards and sprigs left as the twig mass
		 * (their places kept, recoloured), the snow on its limbs' tops. Dense for a hedge's thorn, sparse for a tree. */
		void Bare(LandField::BaseMesh& M, const Tone& TwigOf, double Keep, double CapShare, uint64_t Salt) const
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
			// far off: its crown's clusters thinned to a haze of twigs, a little paler than near
			const std::vector<LandField::Rgb8>& FC = M.Far.Colours;
			const std::vector<uint32_t>& FT = M.Far.Triangles;
			KeepTris(M.Far, [&FC, &FT, S](size_t T) {
				const bool bLeaf = Leafy(FC[FT[3 * T]]) || Leafy(FC[FT[3 * T + 1]]) || Leafy(FC[FT[3 * T + 2]]);
				return !bLeaf || Unit(Hash(S ^ 0xFA, static_cast<uint64_t>(T))) < 0.5;
			});
			for (LandField::Rgb8& C : M.Far.Colours)
				if (Leafy(C)) C = Byte(Varied(Mixed(TwigOf, Rgb(150, 140, 138), 0.2), std::clamp(Lum(OfByte(C)) / 88.0, 0.7, 1.2), 0.0));
			const double Sn = CapShare * Smooth(0.05, 0.6, Cover);
			Cap(M.Near, 0.45, Sn, Crust);
			Cap(M.Cards, 0.55, 0.6 * Sn, Crust);
			Cap(M.Sprigs, 0.55, 0.6 * Sn, Crust);
			Cap(M.Far, 0.55, 0.5 * Sn, Crust);
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

		void Base(LandField::Base B, LandField::BaseMesh& M) const override
		{
			using K = LandField::Base;
			const double Sn = Smooth(0.03, 0.5, Cover);
			switch (B)
			{
			case K::Oak:
			case K::Beech:
				// (the young oaks and beeches hold a few dead leaves, warmer, through the winter)
				Bare(M, Mixed(Twig, Rgb(150, 108, 72), 0.3), 0.3, 0.7, 0x0A + static_cast<uint64_t>(B));
				return;
			case K::Elm:
			case K::Ash:
			case K::Pollard:
			case K::Apple:
				Bare(M, Twig, 0.26, 0.7, 0x0A + static_cast<uint64_t>(B));
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
				// the grass that stands out of the snow: winter-dry, straw and grey-olive, not July's green
				for (LandField::Rgb8& C : M.Near.Colours) C = Byte(Mixed(OfByte(C), Varied(Straw, std::clamp(Lum(OfByte(C)) / 100.0, 0.7, 1.15), 0.0), 0.7));
				return;
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
			if (B >= K::Tuft) return; // (the other clutter as built; a region's own clutter is its to dress)
			// A kit model or a prop: a deciduous tree bare; an evergreen's boughs and everything's roof under snow (a model
			// already laden left as it is).
			if (Named(M.Name, "Snow")) return;
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
		bool bOpenCountry = false, bBroadleafWoods = true;
		Tone Dirt = Loam;
	};

	std::unique_ptr<Snow> MakeSnowCover(double Cover, Look L, uint64_t S)
	{
		return std::make_unique<Winter>(Cover, L, S);
	}
}
