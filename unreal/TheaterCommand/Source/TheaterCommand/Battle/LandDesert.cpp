#include "Battle/LandScape.h"

#include <algorithm>
#include <cmath>

/**
 * The desert (North Africa and the Middle East): a first, rough lay to prove LandScape's hooks, to be grown into the full
 * ground (the stony serir and its sand sheets, the escarpments and the wadis, the camel thorn, the coast road's poles, the
 * sangars, the flat-roofed villages). Kit from the Mediterranean thread's battle models (battle-models/med).
 */
namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		enum Crop : int
		{
			Serir = 0, // the stony plain: gravel and flints over hard sand
			Sand = 1,  // a sand sheet, drifted in low swells
			Scrub = 2, // a flat of camel thorn and saltbush where the water lies after rain
			Rock = 3   // bare rock and scree
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
			KStones = 7
		};

		class Desert final : public Region
		{
		public:
			Desert(const LandField::Field& F, uint64_t S) : Region(F, S) {}
			Look Which() const override { return Look::Desert; }

			double Relief(double X, double Y, double Z) const override
			{
				// (low swells of drifted sand a few hundred metres across, never dunes on these battlefields)
				return Z + 0.6 * Fbm(X, Y, 260.0, 2, Seed ^ 0xD1);
			}
			double Rolling(double X, double Y, double MeanZ) const override
			{
				return MeanZ + 10.0 * Fbm(X, Y, 3800.0, 3, Seed ^ 0xB1) + 2.0 * Fbm(X, Y, 900.0, 2, Seed ^ 0xB2);
			}
			double LaneDepth(double /*X*/, double /*Y*/) const override { return 0.06; }
			double LaneHalf(bool bVillage, double U) const override { return (bVillage ? 3.5 : 2.6) + 1.6 * U; }
			double PlotMin() const override { return 90000.0; }
			double PlotMax() const override { return 220000.0; }
			double FarTilled() const override { return 0.0; }
			double WoodsEdge() const override { return 0.9; }

			int CropOf(int Kind, double U) const override
			{
				if (Kind == KRough) return Rock;
				if (Kind == KField) return Scrub;
				return U < 0.62 ? Serir : U < 0.88 ? Sand : Scrub;
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

			Tone Ground(int C, double N1, double N2, double Broad) const
			{
				const Tone Gravel = Rgb(170, 140, 106), Fine = Rgb(206, 180, 134), Dark = Rgb(122, 98, 76), RockT = Rgb(164, 134, 104);
				Tone T;
				switch (C)
				{
				case Sand: T = Mixed(Fine, Rgb(214, 188, 138), 0.5 + 0.5 * N1); break;
				case Scrub: T = Mixed(Gravel, Rgb(150, 132, 100), 0.4 + 0.3 * N1); break;
				case Rock: T = Mixed(RockT, Dark, 0.3 + 0.3 * N1); break;
				default: T = Mixed(Gravel, Fine, 0.3 + 0.35 * N1); break;
				}
				// (the desert's varnished stones darken the serir in patches; the broad swell of colour across the plain)
				if (C == Serir) T = Mixed(T, Dark, 0.25 * Smooth(0.1, 0.6, N2));
				return Varied(T, 1.0 + 0.05 * N2 + 0.06 * Broad, 0.02 * Broad);
			}

			Tone Colour(const Spot& S) const override
			{
				switch (S.Kind)
				{
				case KLane:
				{
					// a track: the lorries' ruts pressed darker, the crown between them paler, dust either side
					const double Off = std::fabs(S.Lane + S.LaneHalf);
					const double Rut = Smooth(0.5, 0.15, std::fabs(Off - 0.85));
					const Tone Dust = Rgb(196, 170, 128), Pressed = Rgb(160, 134, 100);
					return Varied(Mixed(Dust, Pressed, 0.7 * Rut), 1.0 + 0.04 * S.N2, 0.0);
				}
				case KVillage:
				case KYard: return Varied(Rgb(190, 166, 128), 1.0 + 0.05 * S.N2, 0.01);
				case KWater: return Rgb(70, 86, 88);
				case KMarsh: return Varied(Rgb(150, 146, 112), 1.0 + 0.05 * S.N1, 0.0); // (a sabkha: salt crust and mud)
				case KWorks: return Varied(Rgb(176, 150, 112), 1.0 + 0.06 * S.N2, 0.0);
				case KWoods: return Varied(Rgb(150, 132, 96), 1.0 + 0.06 * S.N1, 0.0); // (a palm grove's floor)
				default: break;
				}
				const int C = S.Crop >= 0 ? S.Crop : Serir;
				return Ground(C, S.N1, S.N2, S.Broad);
			}

			Tone Far(const Spot& S, bool /*bWoods*/, double /*Gap*/) const override
			{
				const int C = S.Crop >= 0 ? S.Crop : Serir;
				return Ground(C, S.N1, 0.0, S.Broad);
			}

			Grown Grow(const Spot& S, double Pick) const override
			{
				Grown G;
				if (S.Kind == KWater || S.Drawn == 9 /*floor*/) return G;
				if (S.Kind == KLane) return S.Lane > -0.3 ? Grown{ LandField::Base::Stone, 0.25 } : G;
				const int C = S.Crop >= 0 ? S.Crop : Serir;
				switch (C)
				{
				case Sand: G = Pick < 0.9 ? Grown{ LandField::Base::Stone, 0.08 } : Grown{ LandField::Base::Tuft, 0.05 }; break;
				case Scrub: G = Pick < 0.5 ? Grown{ LandField::Base::Tuft, 0.35 } : Grown{ LandField::Base::Stone, 0.4 }; break;
				case Rock: G = Grown{ LandField::Base::Stone, 1.2 }; break;
				default: G = Pick < 0.94 ? Grown{ LandField::Base::Stone, 0.9 } : Grown{ LandField::Base::Tuft, 0.08 }; break;
				}
				return G;
			}

			bool Dress(LandField::Placed& P, const Spot& S, double U4, double U5, double U6, double U7) const override
			{
				(void)S;
				if (P.Kind == LandField::Base::Stone)
				{
					// (flints and limestone, many small, a few the size of a loaf; sun-bleached on top, varnished dark)
					P.Scale = static_cast<float>(0.35 + 1.1 * U4 * U4);
					P.ScaleZ = static_cast<float>(0.5 + 0.4 * U5);
					const double Dark = U6 < 0.35 ? 0.62 : 1.0;
					P.Tint = Tint(1.26 * Dark, 1.06 * Dark, 0.82 * Dark);
					return true;
				}
				if (P.Kind == LandField::Base::Tuft)
				{
					P.Scale = static_cast<float>(0.5 + 0.6 * U4);
					P.ScaleZ = static_cast<float>(0.4 + 0.3 * U5);
					const double Dry = U7;
					P.Tint = Tint(1.35 + 0.1 * Dry, 1.12 + 0.05 * Dry, 0.78);
					return true;
				}
				return false;
			}

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
				default: return nullptr;
				}
			}
			void KitLook(int K, LandField::BaseMesh& M) const override
			{
				switch (K)
				{
				case KHouse:
				case KHouseStone:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.9f;
					M.FarFrom = 300.0f;
					break;
				case KBoulder:
				case KSangar:
					M.bSight = true;
					M.SightMid = 0.5f;
					M.SightHalf = 0.5f;
					M.SightAcross = 0.85f;
					M.FarFrom = 200.0f;
					break;
				case KPalm:
					M.bSight = true;
					M.SightMid = 0.85f;
					M.SightHalf = 0.15f;
					M.SightAcross = 0.8f;
					M.SightTrunk = 0.3f;
					M.FarFrom = 250.0f;
					break;
				default: M.FarFrom = 120.0f; break;
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
					W.Box(0.0, 0.0, 1.6, 4.0, 3.0, 1.6, 0.0, Rgb(214, 198, 166));
					break;
				case KThorn: W.Rock(0.0, 0.0, 0.5, 0.45, 7, Rgb(132, 120, 80), Rgb(96, 88, 62)); break;
				case KBoulder:
				case KStones: W.Rock(0.0, 0.0, 0.9, 0.7, 11, Rgb(176, 150, 118), Rgb(120, 100, 80)); break;
				case KSangar: W.Box(0.0, 0.0, 0.4, 1.8, 0.4, 0.4, 0.0, Rgb(160, 140, 112)); break;
				default: W.Box(0.0, 0.0, 3.0, 0.12, 0.12, 3.0, 0.0, Rgb(110, 96, 80)); break;
				}
				M.Far = M.Near;
				M.FarFrom = 400.0f;
			}

			void Sites(Host& H) override
			{
				double TX = 0.0, TY = 0.0, TR = 0.0;
				if (!H.Town(TX, TY, TR)) return;
				Rng R(H.Seed() ^ 0x51E5);
				int Built = 0;
				// (flat-roofed houses packed round the town's middle, no street ruled straight)
				for (int K = 0; K < 400 && Built < 40; ++K)
				{
					const double A = R.Range(0.0, 2.0 * Pi), D = TR * std::sqrt(R.Next()) * 0.9;
					const double X = TX + D * std::cos(A), Y = TY + D * std::sin(A);
					const double Yaw = R.Range(-0.2, 0.2) + (R.Next() < 0.5 ? 0.0 : 0.5 * Pi);
					const bool bStone = R.Next() < 0.35;
					if (H.House(bStone ? LandField::Base::Kit3 : LandField::Base::Kit2, X, Y, Yaw, 8.0, 6.0)) ++Built;
				}
				H.Tally(Built, 0, 0);
			}

			void Plants(Host& H) override
			{
				Rng R(H.Seed() ^ 0x9A47);
				const double B = H.Band(), W = H.Width() + 2.0 * B, D = H.Depth() + 2.0 * B;
				// camel thorn and saltbush: some 30 a hectare on the scrub flats, 6 on the serir, a few on the sand
				const int Tries = static_cast<int>(W * D / 10000.0 * 40.0);
				for (int K = 0; K < Tries; ++K)
				{
					const double X = R.Range(-B, W - B), Y = R.Range(-B, D - B);
					const double Keep = R.Next(), Yaw = R.Range(0.0, 2.0 * Pi), Size = R.Range(0.7, 1.3);
					if (!H.Free(X, Y, 0.8)) continue;
					const Spot S = H.At(X, Y);
					const double Share = S.Kind == KRough ? 0.05 : S.Crop == Scrub ? 0.75 : S.Crop == Sand ? 0.05 : S.Crop == Serir ? 0.15 : 0.0;
					if (Keep >= Share) continue;
					if (S.Kind == KRough) H.Plant(LandField::Base::Kit1, X, Y, Yaw, Size, Size, Tint(1.0, 1.0, 1.0));
					else H.Plant(LandField::Base::Kit0, X, Y, Yaw, Size, Size * R.Range(0.8, 1.1), Tint(1.0, 1.0, 1.0));
				}
			}

			int Fx(const std::string& Ground, int Else) const override
			{
				if (Ground == "woods" || Ground == "forest") return 5;
				return Else == 0 || Else == 4 ? 5 : Else;
			}
			const char* Earth() const override { return "sand"; }
		};
	}

	std::unique_ptr<Region> MakeDesert(const LandField::Field& F, uint64_t S) { return std::make_unique<Desert>(F, S); }
}
