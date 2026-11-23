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
		constexpr double MachineCell = 10.0; // (the machines' squares: no part of one reaches farther from her root)
		constexpr size_t ManBuckets = size_t(1) << 15, MachineBuckets = size_t(1) << 10;
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

		/** Whether a point is inside a machine: her hull, her turret as it is turned, her barrel (Except: a track left out). */
		bool MachineIn(const Machine& M, const Vec& P, int Except)
		{
			if (M.Track == Except && Except >= 0)
			{
				return false;
			}
			// (a quick miss first: farther from her root than any part of her reaches)
			const Vec D = P - M.Root;
			if (D.X * D.X + D.Y * D.Y > 100.0)
			{
				return false;
			}
			if (InBox(Local(P, M.Root, M.Heading), M.HullLo, M.HullHi))
			{
				return true;
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
					return true;
				}
				if (M.bMuzzle)
				{
					const Vec From = Pivot + Vec(0.0, 0.0, 0.5 * (Lo.Z + Hi.Z));
					if (RodDistance(P, From, M.Muzzle) < BarrelRound)
					{
						return true;
					}
				}
			}
			else if (M.bMuzzle)
			{
				// (a gun's barrel from over its axle to the muzzle)
				const Vec From = M.Root + Vec(0.0, 0.0, 0.8 * M.HullHi.Z);
				if (RodDistance(P, From, M.Muzzle) < BarrelRound)
				{
					return true;
				}
			}
			return false;
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
			case Base::Haycock: Out = { 0.45f, 0.45f, 0.75f, 0.0f }; return true; // (the bocage's mown hay, in cocks: a man kneels behind one)
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
		F.Weather = Rec.WeatherWord;
		F.Seed = Rec.Seed ? Rec.Seed : 1;
		// (the look of the land: the recording's word and snow where it names them, else by the place and the month)
		F.Look = G.Look;
		F.Snow = G.Snow;
		F.Lat = Rec.Weather.Lat;
		F.Lon = Rec.Weather.Lon;
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
			W.Track = static_cast<int>(&T - Rec.Tracks.data());
			for (const LandFeed::Point& P : T.At)
			{
				W.Points.emplace_back(P.X, P.Y);
			}
		}
		// (the hedge cutters' cuts: the hedge laid whole there, to be torn open when she bursts through)
		for (const LandFeed::Cut& C : Rec.Cuts)
		{
			LandField::Field::Cut& Into = F.Cuts.emplace_back();
			Into.X = C.X;
			Into.Y = C.Y;
			Into.Width = C.Width;
		}
		return F;
	}

	std::vector<LandFeed::Cut> CutsFromCrossings(const LandFeed::Recording& Rec, const LandField::Ground& G, int N)
	{
		std::vector<LandFeed::Cut> Out;
		if (N <= 0)
		{
			return Out;
		}
		std::vector<LandFeed::Cut> Each;
		for (const LandField::HedgeCrossing& C : G.Crossings())
		{
			// (a hedgerow's bank or a lane's: kinds 0 and 1; the attacker's tanks only, the side that carried the blades)
			if (C.Kind > 1 || C.Track < 0 || static_cast<size_t>(C.Track) >= Rec.Tracks.size())
			{
				continue;
			}
			const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(C.Track)];
			const LandFeed::Block* B = Rec.BlockOf(T.Block);
			if (T.Role != "tank" || !B || B->Side != 0 || T.At.size() < 2)
			{
				continue;
			}
			// (not where she drove through a field's gateway: no bank there to cut)
			bool bGate = false;
			for (const LandField::HedgeOpening& O : G.Openings())
			{
				bGate = bGate || (O.Kind == 0 && std::hypot(O.X - C.X, O.Y - C.Y) < 0.5 * (O.Width + 4.0) + 3.0);
			}
			if (bGate)
			{
				continue;
			}
			const size_t K = std::min(static_cast<size_t>(std::max(0.0, std::floor(C.Along))), T.At.size() - 2);
			const double F = std::clamp(C.Along - static_cast<double>(K), 0.0, 1.0);
			const LandFeed::Point& A = T.At[K];
			const LandFeed::Point& Bp = T.At[K + 1];
			const double When = A.T + (Bp.T - A.T) * F;
			LandFeed::Cut Cut;
			Cut.T1 = std::max(0.0, When - 0.3);
			Cut.T0 = std::max(0.0, Cut.T1 - 2.5);
			Cut.X = C.X;
			Cut.Y = C.Y;
			Cut.Yaw = std::atan2(Bp.Y - A.Y, Bp.X - A.X);
			Cut.Width = 4.0;
			Cut.Unit = C.Track;
			Each.push_back(Cut);
		}
		std::stable_sort(Each.begin(), Each.end(), [](const LandFeed::Cut& A, const LandFeed::Cut& B) { return A.T1 < B.T1; });
		for (const LandFeed::Cut& C : Each)
		{
			bool bNear = false;
			for (const LandFeed::Cut& O : Out)
			{
				bNear = bNear || std::hypot(C.X - O.X, C.Y - O.Y) < 80.0;
			}
			if (bNear)
			{
				continue;
			}
			Out.push_back(C);
			if (static_cast<int>(Out.size()) >= N)
			{
				break;
			}
		}
		return Out;
	}

	void PhonesFromBattle(LandFeed::Recording& Rec, double MendAfter)
	{
		if (!Rec.Wires.empty())
		{
			return;
		}
		// Each side's men's middle at the start: the line from the attacker's to the defender's is "back"; the
		// headquarters is the defender's key man (an officer, an observer, a gun's, mortar's or heavy gun's first man)
		// standing farthest back along it, the battery or command post behind the line.
		double AX = 0.0, AY = 0.0, DX = 0.0, DY = 0.0;
		int Attackers = 0, Defenders = 0;
		for (const LandFeed::Track& T : Rec.Tracks)
		{
			const LandFeed::Block* B = Rec.BlockOf(T.Block);
			if (!B || !T.IsMan() || T.At.empty())
			{
				continue;
			}
			(B->Side == 0 ? AX : DX) += T.At.front().X;
			(B->Side == 0 ? AY : DY) += T.At.front().Y;
			++(B->Side == 0 ? Attackers : Defenders);
		}
		if (Attackers == 0 || Defenders == 0)
		{
			return;
		}
		AX /= Attackers;
		AY /= Attackers;
		DX /= Defenders;
		DY /= Defenders;
		const double Apart = std::max(1e-6, std::hypot(DX - AX, DY - AY));
		const double Ux = (DX - AX) / Apart, Uy = (DY - AY) / Apart;
		auto BackOf = [&](const LandFeed::Point& P) { return (P.X - AX) * Ux + (P.Y - AY) * Uy; };
		int Hq = -1;
		std::vector<int> Ends;
		for (size_t K = 0; K < Rec.Tracks.size(); ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[K];
			const LandFeed::Block* B = Rec.BlockOf(T.Block);
			const bool bKey = T.Role == "officer" || T.Role == "fo" || T.Role == "mortar" || T.Role == "gun" || T.Role == "hmg";
			if (!B || B->Side != 1 || T.At.empty() || T.J != 0 || !bKey)
			{
				continue;
			}
			// (a heavy weapon's line goes to its first piece only)
			const bool bFirst = T.Role == "officer" || T.Role == "fo" || Rec.TrackIndexOf(T.Block, T.K - 1) < 0
				|| Rec.Tracks[static_cast<size_t>(Rec.TrackIndexOf(T.Block, T.K - 1))].Role != T.Role;
			if (Hq < 0 || BackOf(T.At.front()) > BackOf(Rec.Tracks[static_cast<size_t>(Hq)].At.front()))
			{
				Hq = static_cast<int>(K);
			}
			if (bFirst)
			{
				Ends.push_back(static_cast<int>(K));
			}
		}
		if (Hq < 0)
		{
			return;
		}
		// (the lines run first to the posts nearest the enemy: the companies and the observers)
		std::stable_sort(Ends.begin(), Ends.end(), [&](int A, int B) { return BackOf(Rec.Tracks[static_cast<size_t>(A)].At.front()) < BackOf(Rec.Tracks[static_cast<size_t>(B)].At.front()); });
		const LandFeed::Point& From = Rec.Tracks[static_cast<size_t>(Hq)].At.front();
		std::vector<std::pair<double, double>> Done = { { From.X, From.Y } };
		for (const int K : Ends)
		{
			const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(K)];
			const LandFeed::Point& To = T.At.front();
			const double Long = std::hypot(To.X - From.X, To.Y - From.Y);
			bool bNear = false;
			for (const auto& D : Done)
			{
				bNear = bNear || std::hypot(D.first - To.X, D.second - To.Y) < 30.0;
			}
			if (bNear || Rec.Wires.size() >= 6)
			{
				continue;
			}
			Done.push_back({ To.X, To.Y });
			LandFeed::Wire W;
			W.Id = static_cast<int>(Rec.Wires.size());
			W.Side = 1;
			W.Hq = Rec.Tracks[static_cast<size_t>(Hq)].Block;
			W.Unit = T.Block;
			// (a slow wander either side, each line its own, nothing at its two ends)
			const int Steps = std::max(2, static_cast<int>(std::ceil(Long / 5.0)));
			const double Nx = -(To.Y - From.Y) / Long, Ny = (To.X - From.X) / Long;
			const double Phase = 1.7 * static_cast<double>(W.Id), Wave = 1.0 + 0.6 * std::sin(2.3 * static_cast<double>(W.Id) + 0.4);
			for (int I = 0; I <= Steps; ++I)
			{
				const double F = static_cast<double>(I) / static_cast<double>(Steps);
				const double Off = std::sin(3.14159265358979 * F) * Wave * std::sin(Phase + F * Long / 23.0);
				W.Pts.push_back({ From.X + (To.X - From.X) * F + Nx * Off, From.Y + (To.Y - From.Y) * F + Ny * Off });
			}
			Rec.Wires.push_back(std::move(W));
		}
		// Each line cut by the rounds that land beside it while it is whole, mended MendAfter seconds on.
		std::vector<double> WholeFrom(Rec.Wires.size(), -1e9);
		for (const LandFeed::Round& R : Rec.Rounds)
		{
			if (R.R != LandFeed::Result::Miss || (R.W != "mortar" && R.W != "gun" && R.W != "tank"))
			{
				continue;
			}
			for (size_t K = 0; K < Rec.Wires.size(); ++K)
			{
				if (R.L < WholeFrom[K])
				{
					continue;
				}
				const std::vector<std::pair<double, double>>& P = Rec.Wires[K].Pts;
				double Best = 1e9, BX = 0.0, BY = 0.0;
				for (size_t I = 0; I + 1 < P.size(); ++I)
				{
					const double Dx = P[I + 1].first - P[I].first, Dy = P[I + 1].second - P[I].second;
					const double Len2 = std::max(1e-9, Dx * Dx + Dy * Dy);
					const double F = std::clamp(((R.X - P[I].first) * Dx + (R.Y - P[I].second) * Dy) / Len2, 0.0, 1.0);
					const double X = P[I].first + Dx * F, Y = P[I].second + Dy * F;
					const double D = std::hypot(R.X - X, R.Y - Y);
					if (D < Best)
					{
						Best = D;
						BX = X;
						BY = Y;
					}
				}
				if (Best > 5.0)
				{
					continue;
				}
				LandFeed::WireCut Cut;
				Cut.T = R.L;
				Cut.Wire = Rec.Wires[K].Id;
				Cut.X = BX;
				Cut.Y = BY;
				Cut.By = "shell";
				Rec.WireCuts.push_back(Cut);
				LandFeed::WireMend Mend;
				Mend.T = R.L + MendAfter;
				Mend.Wire = Cut.Wire;
				Rec.WireMends.push_back(Mend);
				WholeFrom[K] = Mend.T;
			}
		}
		std::stable_sort(Rec.WireCuts.begin(), Rec.WireCuts.end(), [](const LandFeed::WireCut& A, const LandFeed::WireCut& B) { return A.T < B.T; });
		std::stable_sort(Rec.WireMends.begin(), Rec.WireMends.end(), [](const LandFeed::WireMend& A, const LandFeed::WireMend& B) { return A.T < B.T; });
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
		Highest.clear();
		Lowest.clear();
		Cols = Rows = 0;
		if (!Ground)
		{
			return;
		}
		double Xa = 1e18, Ya = 1e18, Xb = -1e18, Yb = -1e18;
		for (int B = 0; B < static_cast<int>(LandField::Base::Count); ++B)
		{
			const LandField::Base Kind = static_cast<LandField::Base>(B);
			const LandField::BaseMesh& Built = Ground->BaseOf(Kind);
			Shape S;
			if (Built.bSight)
			{
				// (a region's kit: its own shape, LandScape's KitLook)
				S = { Built.SightMid, Built.SightHalf, Built.SightAcross, Built.SightTrunk };
			}
			else if (!ShapeOf(Kind, S))
			{
				continue;
			}
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
		// The squares hold every thing that reaches into them (its crown's or its trunk's round), and the highest and
		// lowest any of them stands there: a question looks in its own square alone, and not at all above or below what
		// stands in it. (The squares once held each thing by its middle and a question looked in nine 10 m squares, some
		// hundred things in a hedgerow's: the camera's own question was most of its time, 22 November.)
		auto Reach = [](const Thing& T) { return static_cast<double>(std::max(T.Across > 0.0f && T.Half > 0.0f ? T.Across : 0.0f, T.Trunk)) + 1e-4; };
		double Far = 0.0;
		for (const Thing& T : Kept)
		{
			Far = std::max(Far, Reach(T));
		}
		Cell = 4.0;
		X0 = Xa - Far - Cell;
		Y0 = Ya - Far - Cell;
		Cols = static_cast<int>(std::ceil((Xb + Far - X0) / Cell)) + 2;
		Rows = static_cast<int>(std::ceil((Yb + Far - Y0) / Cell)) + 2;
		const size_t Squares = static_cast<size_t>(Cols) * static_cast<size_t>(Rows);
		First.assign(Squares + 1, 0u);
		Highest.assign(Squares, -1e30f);
		Lowest.assign(Squares, 1e30f);
		// (each thing's squares: those its reach's box touches)
		auto Span = [&](const Thing& T, int& C0, int& C1, int& R0, int& R1)
		{
			const double R = Reach(T);
			C0 = std::clamp(static_cast<int>(std::floor((T.X - R - X0) / Cell)), 0, Cols - 1);
			C1 = std::clamp(static_cast<int>(std::floor((T.X + R - X0) / Cell)), 0, Cols - 1);
			R0 = std::clamp(static_cast<int>(std::floor((T.Y - R - Y0) / Cell)), 0, Rows - 1);
			R1 = std::clamp(static_cast<int>(std::floor((T.Y + R - Y0) / Cell)), 0, Rows - 1);
		};
		for (const Thing& T : Kept)
		{
			if (Reach(T) <= 1e-3)
			{
				continue;
			}
			// (what it fills, top to bottom: its crown, Mid Half either way; its trunk, from just under its foot to Mid)
			float Lo = 1e30f, Hi = -1e30f;
			if (T.Across > 0.0f && T.Half > 0.0f)
			{
				Lo = std::min(Lo, T.Mid - T.Half);
				Hi = std::max(Hi, T.Mid + T.Half);
			}
			if (T.Trunk > 0.0f)
			{
				Lo = std::min(Lo, T.Z - 0.2f);
				Hi = std::max(Hi, T.Mid);
			}
			int C0, C1, R0, R1;
			Span(T, C0, C1, R0, R1);
			for (int R = R0; R <= R1; ++R)
			{
				for (int C = C0; C <= C1; ++C)
				{
					const size_t Ix = static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C);
					++First[Ix + 1];
					Highest[Ix] = std::max(Highest[Ix], Hi + 1e-3f);
					Lowest[Ix] = std::min(Lowest[Ix], Lo - 1e-3f);
				}
			}
		}
		for (size_t K = 1; K < First.size(); ++K)
		{
			First[K] += First[K - 1];
		}
		Order.assign(First.back(), 0u);
		std::vector<uint32_t> Fill(First.begin(), First.end() - 1);
		for (size_t K = 0; K < Kept.size(); ++K)
		{
			if (Reach(Kept[K]) <= 1e-3)
			{
				continue;
			}
			int C0, C1, R0, R1;
			Span(Kept[K], C0, C1, R0, R1);
			for (int R = R0; R <= R1; ++R)
			{
				for (int C = C0; C <= C1; ++C)
				{
					Order[Fill[static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C)]++] = static_cast<uint32_t>(K);
				}
			}
		}
	}

	size_t Room::Bucket(int64_t I, int64_t J, size_t Count)
	{
		const uint64_t H = static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full;
		return static_cast<size_t>(H ^ (H >> 29)) & (Count - 1);
	}

	void Room::ClearMovers()
	{
		// (only the chains used last time: a few hundred, not the whole table)
		for (const uint32_t B : UsedMen)
		{
			ManHead[B] = -1;
		}
		for (const uint32_t B : UsedMachines)
		{
			MachineHead[B] = -1;
		}
		UsedMen.clear();
		UsedMachines.clear();
		Men.clear();
		ManNext.clear();
		Machines.clear();
		MachineNext.clear();
	}

	void Room::Add(const Man& M)
	{
		if (ManHead.empty())
		{
			ManHead.assign(ManBuckets, -1);
		}
		const size_t B = Bucket(static_cast<int64_t>(std::floor(M.Feet.X / ManCell)), static_cast<int64_t>(std::floor(M.Feet.Y / ManCell)), ManBuckets);
		if (ManHead[B] < 0)
		{
			UsedMen.push_back(static_cast<uint32_t>(B));
		}
		ManNext.push_back(ManHead[B]);
		ManHead[B] = static_cast<int32_t>(Men.size());
		Men.push_back(M);
	}

	void Room::Add(const Machine& M)
	{
		if (MachineHead.empty())
		{
			MachineHead.assign(MachineBuckets, -1);
		}
		const size_t B = Bucket(static_cast<int64_t>(std::floor(M.Root.X / MachineCell)), static_cast<int64_t>(std::floor(M.Root.Y / MachineCell)), MachineBuckets);
		if (MachineHead[B] < 0)
		{
			UsedMachines.push_back(static_cast<uint32_t>(B));
		}
		MachineNext.push_back(MachineHead[B]);
		MachineHead[B] = static_cast<int32_t>(Machines.size());
		Machines.push_back(M);
	}

	Hit Room::At(const Vec& P, int Except) const
	{
		if (P.X != P.X || P.Y != P.Y || P.Z != P.Z)
		{
			return Hit::None;
		}
		if (!MachineHead.empty())
		{
			const int64_t Ci = static_cast<int64_t>(std::floor(P.X / MachineCell)), Cj = static_cast<int64_t>(std::floor(P.Y / MachineCell));
			for (int64_t I = Ci - 1; I <= Ci + 1; ++I)
			{
				for (int64_t J = Cj - 1; J <= Cj + 1; ++J)
				{
					for (int32_t K = MachineHead[Bucket(I, J, MachineBuckets)]; K >= 0; K = MachineNext[static_cast<size_t>(K)])
					{
						if (MachineIn(Machines[static_cast<size_t>(K)], P, Except))
						{
							return Hit::Thing;
						}
					}
				}
			}
		}
		// (a house's floor or a wall's line: the field's word asked only where its raster says one may stand, a fifth of
		// the cost of a question saved where none does)
		if (Ground && Ground->MayBeBuilt(P.X, P.Y))
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
			if (C >= 0 && C < Cols && R >= 0 && R < Rows)
			{
				const size_t Ix = static_cast<size_t>(R) * static_cast<size_t>(Cols) + static_cast<size_t>(C);
				if (P.Z <= Highest[Ix] && P.Z >= Lowest[Ix])
				{
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
		// (a man reaches no farther than 3 m from his feet, lying full length: the squares are that wide; his rod's box
		// first, so a question over his head or beside him costs no distance worked out)
		if (!ManHead.empty())
		{
			const int64_t Ci = static_cast<int64_t>(std::floor(P.X / ManCell)), Cj = static_cast<int64_t>(std::floor(P.Y / ManCell));
			for (int64_t I = Ci - 1; I <= Ci + 1; ++I)
			{
				for (int64_t J = Cj - 1; J <= Cj + 1; ++J)
				{
					for (int32_t K = ManHead[Bucket(I, J, ManBuckets)]; K >= 0; K = ManNext[static_cast<size_t>(K)])
					{
						const Man& M = Men[static_cast<size_t>(K)];
						if (M.Track == Except && Except >= 0)
						{
							continue;
						}
						const double Dx = P.X - M.Feet.X, Dy = P.Y - M.Feet.Y;
						if (Dx * Dx + Dy * Dy > 9.0)
						{
							continue;
						}
						if (P.Z > std::max(M.Feet.Z, M.Top.Z) + ManRound || P.Z < std::min(M.Feet.Z, M.Top.Z) - ManRound ||
							P.X > std::max(M.Feet.X, M.Top.X) + ManRound || P.X < std::min(M.Feet.X, M.Top.X) - ManRound ||
							P.Y > std::max(M.Feet.Y, M.Top.Y) + ManRound || P.Y < std::min(M.Feet.Y, M.Top.Y) - ManRound)
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
		}
		return Hit::None;
	}
}
