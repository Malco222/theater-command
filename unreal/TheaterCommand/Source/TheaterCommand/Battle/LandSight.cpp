#include "Battle/LandSight.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace LandSight
{
	namespace
	{
		/** A man's half-width round his rod. */
		constexpr double ManRound = 0.3;
		constexpr double ManCell = 3.0; // (the men's squares, metres: a man reaches no farther than this from his feet)
		/** A barrel's half-width (with its mantlet and muzzle brake). */
		constexpr double BarrelRound = 0.28;
		/** How high a house and a stone wall stand over the ground at their foot. */
		constexpr double HouseHigh = 7.0, WallHigh = 2.0;

		double RodDistance(const Vec& P, const Vec& A, const Vec& B)
		{
			const Vec Ab = B - A;
			const double Long2 = Ab.Dot(Ab);
			double U = Long2 > 1e-9 ? (P - A).Dot(Ab) / Long2 : 0.0;
			U = std::clamp(U, 0.0, 1.0);
			return (P - (A + Ab * U)).Length();
		}

		/** A field point into a body's own axes (X forward, Y right, Z up) about Origin, facing Facing. */
		Vec Local(const Vec& P, const Vec& Origin, double Facing)
		{
			const double Sn = std::sin(Facing), Cs = std::cos(Facing);
			const Vec D = P - Origin;
			return Vec(D.X * Sn + D.Y * Cs, D.X * Cs - D.Y * Sn, D.Z);
		}

		bool InBox(const Vec& L, const Vec& Lo, const Vec& Hi)
		{
			return L.X >= Lo.X && L.X <= Hi.X && L.Y >= Lo.Y && L.Y <= Hi.Y && L.Z >= Lo.Z && L.Z <= Hi.Z;
		}

		struct Shape
		{
			float Mid, Half, Across, Trunk; // shares of the base's height (Mid, Half), radius (Across), metres (Trunk)
		};

		/** How each kept base stands, as shares of its built size: shrubs and brambles a round bush from the ground up;
		 * the trees a crown over a trunk. Clutter (grass, corn, flowers, stones, ferns) is never kept: the camera's corn
		 * is World::Crop's; a gate is a few bars, seen through. */
		bool ShapeOf(LandField::Base B, Shape& Out)
		{
			using LandField::Base;
			switch (B)
			{
			case Base::ShrubA:
			case Base::ShrubB:
			case Base::ShrubC: Out = { 0.5f, 0.5f, 0.8f, 0.0f }; return true;
			case Base::Bramble: Out = { 0.45f, 0.5f, 0.8f, 0.0f }; return true;
			case Base::Oak:
			case Base::Elm:
			case Base::Ash:
			case Base::Beech: Out = { 0.66f, 0.34f, 0.85f, 0.35f }; return true;
			case Base::Pollard: Out = { 0.75f, 0.25f, 0.85f, 0.4f }; return true;
			case Base::Apple: Out = { 0.65f, 0.35f, 0.85f, 0.15f }; return true;
			case Base::Well: Out = { 0.5f, 0.5f, 0.9f, 0.0f }; return true;
			case Base::Cart: Out = { 0.5f, 0.5f, 0.35f, 0.0f }; return true;
			// the steppe's (a cottage stands as a house; poles, sweeps and wattle are seen through)
			case Base::Birch: Out = { 0.62f, 0.38f, 0.7f, 0.15f }; return true;
			case Base::Haystack: Out = { 0.42f, 0.45f, 0.85f, 0.0f }; return true;
			case Base::Chimney: Out = { 0.5f, 0.5f, 0.12f, 0.0f }; return true; // (the stove and its stack, in its ash)
			default: return false;
			}
		}
	}

	int MonthOf(const std::string& Date)
	{
		static const char* Names[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
		std::string Low;
		for (const char Ch : Date)
		{
			Low += static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
		}
		for (int K = 0; K < 12; ++K)
		{
			if (Low.find(Names[K]) != std::string::npos)
			{
				return K + 1;
			}
		}
		return 7;
	}

	LandField::Field FieldOf(const LandFeed::Recording& Rec)
	{
		LandField::Field F;
		const LandFeed::Field& G = Rec.Ground;
		F.Cols = G.Cols;
		F.Rows = G.Rows;
		F.Cell = G.Cell;
		F.Grounds = G.Grounds;
		F.Ground.assign(G.Ground.begin(), G.Ground.end());
		F.Height.assign(G.Height.begin(), G.Height.end());
		F.Storeys.assign(G.Storeys.begin(), G.Storeys.end());
		F.bTown = G.TownR > 0.0;
		F.TownX = G.TownX;
		F.TownY = G.TownY;
		F.TownR = G.TownR;
		F.Terrain = G.Terrain.empty() ? std::string("plains") : G.Terrain;
		F.Month = MonthOf(Rec.Date);
		F.Seed = Rec.Seed ? Rec.Seed : 1;
		// (the open steppe, where the recording lays it; the kit's models are the caller's to give, F.Steppe.Kit)
		if (G.Steppe.bOn)
		{
			LandField::SteppeLay& L = F.Steppe;
			L.bOn = true;
			L.Turn = G.Steppe.Turn;
			L.CX = G.Steppe.CX;
			L.CY = G.Steppe.CY;
			L.Xs = G.Steppe.Xs;
			L.Ys = G.Steppe.Ys;
			L.Shift = G.Steppe.Shift;
			for (const char Ch : G.Steppe.Patch) L.Patch.push_back(static_cast<uint8_t>(Ch >= '0' && Ch <= '9' ? Ch - '0' : 0));
			for (const char Ch : G.Steppe.Crop) L.Crop.push_back(static_cast<uint8_t>(Ch >= '0' && Ch <= '9' ? Ch - '0' : 0));
			L.Balka = G.Steppe.Balka;
			L.BalkaDeep = G.Steppe.BalkaDeep;
			L.Rail = G.Steppe.Rail;
			L.RailBank = G.Steppe.RailBank;
			L.HillX = G.Steppe.HillX;
			L.HillY = G.Steppe.HillY;
			L.HillR = G.Steppe.HillR;
			L.HillRise = G.Steppe.HillRise;
		}
		// (where the men and the vehicles go: a hedge they cross is opened there)
		for (const LandFeed::Track& T : Rec.Tracks)
		{
			if (T.bTwin || T.At.size() < 2)
			{
				continue;
			}
			LandField::Field::Way& W = F.Ways.emplace_back();
			W.HalfWidth = T.Role == "tank" ? 1.8 : T.Role == "gun" ? 1.4 : T.Role == "lorry" ? 1.3 : 0.6;
			for (const LandFeed::Point& P : T.At)
			{
				W.Points.emplace_back(P.X, P.Y);
			}
		}
		return F;
	}

	Machine Machine::Plain(int Track, const Vec& Root, double Heading, double Turret, double Long, double Wide, double High, bool bWithTurret)
	{
		Machine M;
		M.Track = Track;
		M.Root = Root;
		M.Heading = Heading;
		M.Turret = Turret;
		M.HullLo = Vec(-0.5 * Long, -0.5 * Wide, 0.0);
		M.HullHi = Vec(0.5 * Long, 0.5 * Wide, bWithTurret ? 0.65 * High : High);
		M.bTurret = bWithTurret;
		if (bWithTurret)
		{
			M.TurretLo = Vec(-0.28 * Long, -0.36 * Wide, 0.6 * High);
			M.TurretHi = Vec(0.22 * Long, 0.36 * Wide, High);
			const double Sn = std::sin(Turret), Cs = std::cos(Turret);
			const Vec Middle = Root + Vec(0.0, 0.0, 0.82 * High);
			M.Muzzle = Middle + Vec(Sn, Cs, 0.0) * (Long * 0.95);
			M.bMuzzle = true;
		}
		return M;
	}

	void Room::Lay(const LandField::Ground* InGround)
	{
		Ground = InGround;
		Kept.clear();
		First.clear();
		Order.clear();
		Cols = Rows = 0;
		if (!Ground)
		{
			return;
		}
		double Xa = 1e18, Ya = 1e18, Xb = -1e18, Yb = -1e18;
		for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
		{
			const LandField::Base Kind = static_cast<LandField::Base>(B);
			Shape S;
			if (!ShapeOf(Kind, S))
			{
				continue;
			}
			const LandField::BaseMesh& Built = Ground->BaseOf(Kind);
			for (const LandField::Placed& P : Ground->Instances(Kind))
			{
				Thing T;
				T.X = P.X;
				T.Y = P.Y;
				T.Z = P.Z;
				const float High = Built.Height * P.ScaleZ;
				T.Mid = P.Z + S.Mid * High;
				T.Half = S.Half * High;
				T.Across = S.Across * Built.Radius * P.Scale;
				T.Trunk = S.Trunk * P.Scale;
				Kept.push_back(T);
				Xa = std::min(Xa, static_cast<double>(P.X));
				Ya = std::min(Ya, static_cast<double>(P.Y));
				Xb = std::max(Xb, static_cast<double>(P.X));
				Yb = std::max(Yb, static_cast<double>(P.Y));
			}
		}
		if (Kept.empty())
		{
			return;
		}
		// The squares hold each thing by its middle; a question looks in its own square and the eight round it, so no
		// thing may reach farther than a square from its middle (the widest oak's crown is about 9 m).
		Cell = 10.0;
		X0 = Xa - Cell;
		Y0 = Ya - Cell;
		Cols = static_cast<int>(std::ceil((Xb - X0) / Cell)) + 2;
		Rows = static_cast<int>(std::ceil((Yb - Y0) / Cell)) + 2;
		First.assign(static_cast<size_t>(Cols) * static_cast<size_t>(Rows) + 1, 0u);
		auto CellOf = [&](const Thing& T)
		{
			const int C = std::clamp(static_cast<int>((T.X - X0) / Cell), 0, Cols - 1);
			const int R = std::clamp(static_cast<int>((T.Y - Y0) / Cell), 0, Rows - 1);
			return static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C);
		};
		for (const Thing& T : Kept)
		{
			++First[CellOf(T) + 1];
		}
		for (size_t K = 1; K < First.size(); ++K)
		{
			First[K] += First[K - 1];
		}
		Order.assign(Kept.size(), 0u);
		std::vector<uint32_t> Fill(First.begin(), First.end() - 1);
		for (size_t K = 0; K < Kept.size(); ++K)
		{
			Order[Fill[CellOf(Kept[K])]++] = static_cast<uint32_t>(K);
		}
	}

	void Room::ClearMovers()
	{
		Men.clear();
		Machines.clear();
		// (only the squares men stood in last time: the map keeps every square a man has ever stood in, empty)
		for (const int64_t Key : UsedSquares)
		{
			MenSquares[Key].clear();
		}
		UsedSquares.clear();
	}

	int64_t Room::ManSquare(double X, double Y)
	{
		const int64_t I = static_cast<int64_t>(std::floor(X / ManCell)), J = static_cast<int64_t>(std::floor(Y / ManCell));
		return (I << 32) ^ (J & 0xFFFFFFFFll);
	}

	void Room::Add(const Man& M)
	{
		const int64_t Key = ManSquare(M.Feet.X, M.Feet.Y);
		std::vector<uint32_t>& Square = MenSquares[Key];
		if (Square.empty())
		{
			UsedSquares.push_back(Key);
		}
		Square.push_back(static_cast<uint32_t>(Men.size()));
		Men.push_back(M);
	}

	void Room::Add(const Machine& M)
	{
		Machines.push_back(M);
	}

	Hit Room::At(const Vec& P, int Except) const
	{
		if (P.X != P.X || P.Y != P.Y || P.Z != P.Z)
		{
			return Hit::None;
		}
		for (const Machine& M : Machines)
		{
			if (M.Track == Except && Except >= 0)
			{
				continue;
			}
			// (a quick miss first: farther from her root than any part of her reaches)
			const Vec D = P - M.Root;
			if (D.X * D.X + D.Y * D.Y > 100.0)
			{
				continue;
			}
			if (InBox(Local(P, M.Root, M.Heading), M.HullLo, M.HullHi))
			{
				return Hit::Thing;
			}
			if (M.bTurret)
			{
				const Vec Lo = M.TurretLo, Hi = M.TurretHi;
				// (turned about the turret box's own middle, as it turns on its ring)
				const double Cu = 0.5 * (Lo.X + Hi.X), Cv = 0.5 * (Lo.Y + Hi.Y);
				const double Sn = std::sin(M.Heading), Cs = std::cos(M.Heading);
				const Vec Pivot = M.Root + Vec(Cu * Sn + Cv * Cs, Cu * Cs - Cv * Sn, 0.0);
				const Vec L = Local(P, Pivot, M.Turret);
				if (InBox(L, Vec(Lo.X - Cu, Lo.Y - Cv, Lo.Z), Vec(Hi.X - Cu, Hi.Y - Cv, Hi.Z)))
				{
					return Hit::Thing;
				}
				if (M.bMuzzle)
				{
					const Vec From = Pivot + Vec(0.0, 0.0, 0.5 * (Lo.Z + Hi.Z));
					if (RodDistance(P, From, M.Muzzle) < BarrelRound)
					{
						return Hit::Thing;
					}
				}
			}
			else if (M.bMuzzle)
			{
				// (a gun's barrel from over its axle to the muzzle)
				const Vec From = M.Root + Vec(0.0, 0.0, 0.8 * M.HullHi.Z);
				if (RodDistance(P, From, M.Muzzle) < BarrelRound)
				{
					return Hit::Thing;
				}
			}
		}
		if (Ground)
		{
			LandField::Drawn What = LandField::Drawn::Pasture;
			const double Z = Ground->HeightAt(P.X, P.Y, &What);
			if (What == LandField::Drawn::Floor && P.Z < Z + HouseHigh)
			{
				return Hit::Thing;
			}
			if (What == LandField::Drawn::Wall && P.Z < Z + WallHigh)
			{
				return Hit::Thing;
			}
		}
		if (Cols > 0)
		{
			const int C = static_cast<int>(std::floor((P.X - X0) / Cell));
			const int R = static_cast<int>(std::floor((P.Y - Y0) / Cell));
			for (int Rr = R - 1; Rr <= R + 1; ++Rr)
			{
				if (Rr < 0 || Rr >= Rows)
				{
					continue;
				}
				for (int Cc = C - 1; Cc <= C + 1; ++Cc)
				{
					if (Cc < 0 || Cc >= Cols)
					{
						continue;
					}
					const size_t Ix = static_cast<size_t>(Rr) * static_cast<size_t>(Cols) + static_cast<size_t>(Cc);
					for (uint32_t K = First[Ix]; K < First[Ix + 1]; ++K)
					{
						const Thing& T = Kept[Order[K]];
						const double Dx = P.X - T.X, Dy = P.Y - T.Y;
						const double Flat2 = Dx * Dx + Dy * Dy;
						if (T.Trunk > 0.0f && Flat2 < static_cast<double>(T.Trunk) * T.Trunk && P.Z >= T.Z - 0.2 && P.Z <= T.Mid)
						{
							return Hit::Thing;
						}
						const double A = T.Across, H = T.Half;
						if (A <= 0.0 || H <= 0.0 || Flat2 > A * A)
						{
							continue;
						}
						const double Dz = (P.Z - T.Mid) / H;
						if (Flat2 / (A * A) + Dz * Dz <= 1.0)
						{
							return Hit::Thing;
						}
					}
				}
			}
		}
		// (a man reaches no farther than 3 m from his feet, lying full length: the squares are that wide)
		const int64_t Ci = static_cast<int64_t>(std::floor(P.X / ManCell)), Cj = static_cast<int64_t>(std::floor(P.Y / ManCell));
		for (int64_t I = Ci - 1; I <= Ci + 1; ++I)
		{
			for (int64_t J = Cj - 1; J <= Cj + 1; ++J)
			{
				const auto Found = MenSquares.find((I << 32) ^ (J & 0xFFFFFFFFll));
				if (Found == MenSquares.end())
				{
					continue;
				}
				for (const uint32_t K : Found->second)
				{
					const Man& M = Men[K];
					if (M.Track == Except && Except >= 0)
					{
						continue;
					}
					const double Dx = P.X - M.Feet.X, Dy = P.Y - M.Feet.Y;
					if (Dx * Dx + Dy * Dy > 9.0)
					{
						continue;
					}
					if (RodDistance(P, M.Feet, M.Top) < ManRound)
					{
						return Hit::Man;
					}
				}
			}
		}
		return Hit::None;
	}
}
