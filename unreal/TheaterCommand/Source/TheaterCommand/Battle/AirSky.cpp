#include "Battle/AirSky.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace AirSky
{
	// (picture colours, as the effects recipe gives its own: a sunlit cloud top a touch warm, its shaded side a cool
	// grey-blue, its flat base darker; the whites near the kit's glycol vapour, 236 to 240, which reads white in the game.
	// The art director's light and shade, 19 November 12:59Z: a down-sun side about 160 of brightness, a sunlit top 235
	// to 245, a base 140 to 155, darker than the sky behind it; a cloud's shadow on the sea 25 to 30 per cent darker.
	// The sunlit top's paint is above that mark on purpose: the haze and the game's exposure take it down (230 in game
	// on build 1722, the art director, 14:13Z; 226 to 231 through the battle grade on 1732, and at 252 there is no room
	// left in the paint: the scene's cloud ink lifts it, AirBattle.cpp's CloudBright, and the painter paints the
	// other three that much darker to keep them where they were, Painter::SetInk))
	const Rgb SunSide = Rgb::FromPicture(252, 250, 245);
	const Rgb ShadeSide = Rgb::FromPicture(162, 170, 184);
	const Rgb BaseSide = Rgb::FromPicture(134, 141, 154);
	const Rgb ShadowSide = Rgb::FromPicture(10, 16, 24);

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Clamp(double V, double A, double B) { return std::max(A, std::min(B, V)); }

		// (the painter's per-point arithmetic kept in this file, where it is inlined: a paint lays some 100,000 points,
		// and the effects kit's Mix and Unit are in its own file)
		inline Rgb Blend(const Rgb& From, const Rgb& To, double T)
		{
			const float W = static_cast<float>(Clamp(T, 0.0, 1.0));
			return Rgb(From.R + (To.R - From.R) * W, From.G + (To.G - From.G) * W, From.B + (To.B - From.B) * W);
		}

		// (a puff's own phase for its lumps, sin and cos of (index mod 97) x 0.61, looked up rather than worked out)
		const std::vector<double>& PhaseSines(bool bCos)
		{
			static const std::vector<double> Sines = [] { std::vector<double> Out; for (int K = 0; K < 97; ++K) Out.push_back(std::sin(K * 0.61)); return Out; }();
			static const std::vector<double> Cosines = [] { std::vector<double> Out; for (int K = 0; K < 97; ++K) Out.push_back(std::cos(K * 0.61)); return Out; }();
			return bCos ? Cosines : Sines;
		}

		double Smooth(double T)
		{
			T = Clamp(T, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		/** A number 0 to 1 for a lattice point, the same every time for the same seed. */
		double Hash(int64_t X, int64_t Y, uint64_t Seed)
		{
			uint64_t H = Seed ^ (static_cast<uint64_t>(X) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(Y) * 0xC2B2AE3D27D4EB4Full);
			H ^= H >> 33;
			H *= 0xFF51AFD7ED558CCDull;
			H ^= H >> 33;
			H *= 0xC4CEB9FE1A85EC53ull;
			H ^= H >> 33;
			return static_cast<double>(H >> 11) / 9007199254740992.0;
		}

		int64_t Wrap(int64_t I, int64_t N) { return ((I % N) + N) % N; }

		/** Smooth value noise, 0 to 1, with lattice points Scale metres apart, repeating every Cells points (so that, with
		 * Scale * Cells the laid square's side, it has no seam where the square repeats). */
		double Noise(double X, double Y, double Scale, int64_t Cells, uint64_t Seed)
		{
			const double U = X / Scale, V = Y / Scale;
			const double FU = std::floor(U), FV = std::floor(V);
			const int64_t IU = static_cast<int64_t>(FU), IV = static_cast<int64_t>(FV);
			const double A = Smooth(U - FU), B = Smooth(V - FV);
			const int64_t U0 = Wrap(IU, Cells), U1 = Wrap(IU + 1, Cells), V0 = Wrap(IV, Cells), V1 = Wrap(IV + 1, Cells);
			const double N00 = Hash(U0, V0, Seed), N10 = Hash(U1, V0, Seed), N01 = Hash(U0, V1, Seed), N11 = Hash(U1, V1, Seed);
			return (N00 * (1.0 - A) + N10 * A) * (1.0 - B) + (N01 * (1.0 - A) + N11 * A) * B;
		}

		/** The shortest way across the repeating square (a difference of laid places, folded into half a side). */
		double Across(double D) { return D - Period * std::round(D / Period); }

		std::atomic<uint64_t> Layings { 0 };
	}

	const Vec Wind(3.0, -2.0, 0.0);

	double PlaceDeck(double& Base, double& Top, double Band, double LowBand, std::string& OutWhy)
	{
		OutWhy.clear();
		if (Band > 0.0 && Band <= 2000.0 && LowBand <= 0.0)
		{
			// (a lone band at 2,000 m or lower is the lowest band)
			LowBand = Band;
			Band = 0.0;
		}
		const double Thick = std::max(300.0, Top - Base);
		const double Was = Top;
		double NewBase = Base, NewTop = Base + Thick;
		if (Band > 2000.0)
		{
			// (1.5 to 2.5 km below the bombers, its thickness kept; thinned only where keeping it would bring its base
			// within 300 m of the sea)
			const double Most = Band - 1500.0, Least = Band - 2500.0;
			if (NewTop > Most)
			{
				NewTop = Most;
				OutWhy = "below the band";
			}
			else if (NewTop < Least)
			{
				NewTop = Least;
				OutWhy = "up to the band";
			}
			NewBase = NewTop - Thick;
			if (NewBase < 300.0)
			{
				NewBase = 300.0;
				NewTop = std::max(NewTop, NewBase + 300.0);
			}
		}
		if (LowBand > 0.0 && NewBase < LowBand + 600.0)
		{
			// (the lowest band, 1,800 m: the deck over those bombers, its base 600 m above them; under a higher band
			// as well, thinned to fit, never under 300 m thick)
			NewBase = LowBand + 600.0;
			NewTop = NewBase + Thick;
			if (Band > 2000.0)
			{
				NewTop = std::max(NewBase + 300.0, std::min(NewTop, Band - 1500.0));
			}
			OutWhy = "above the band";
		}
		Base = NewBase;
		Top = NewTop;
		return Top - Was;
	}

	Vec Field::DriftAt(double AirSeconds) const
	{
		if (Sky.bDeck)
		{
			return Vec();
		}
		return Wind * std::max(0.0, AirSeconds);
	}

	Vec Field::ShiftNear(int CloudIndex, double X, double Y) const
	{
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		return Vec(Period * std::round((X - C.At.X) / Period), Period * std::round((Y - C.At.Y) / Period), 0.0);
	}

	double Field::LiftAt(int CloudIndex, double X, double Y) const
	{
		if (!Ground)
		{
			return 0.0;
		}
		LiftMemo* Memo = Sky.bDeck && static_cast<size_t>(CloudIndex) < DeckLifts.size() ? &DeckLifts[static_cast<size_t>(CloudIndex)] : nullptr;
		if (Memo && Memo->X == X && Memo->Y == Y)
		{
			return Memo->Lift;
		}
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		auto At = [this](double PX, double PY) { return std::max(0.0, Ground(PX, PY)); };
		// (the ground on a lattice 120 m apart, finer than a stage's 150 m cells, so that no cell is missed: under the
		// puffs' hearts, and beyond them out to their rims; each 220 m wider, a cell's width across its corners, so that
		// a cell the edge only clips is looked at wherever its lattice points fall in it)
		const double Step = 120.0;
		const double HeartOut = std::max(C.HeartReach, 0.25 * C.W) + 220.0, RimOut = std::max(HeartOut, std::max(C.RimReach, 0.5 * C.W) + 220.0);
		// (at most 64 lattice steps, 7.7 km, either way: no cloud laid is a fifth of that, and a broken field, a cloud of
		// infinite or unknown width, takes the ground under its middle alone, never a loop without end; the pre-check
		// on round 8)
		const double Cells = std::ceil(RimOut / Step);
		const int N = std::isfinite(Cells) ? static_cast<int>(std::min(64.0, std::max(0.0, Cells))) : 0;
		double UnderHearts = At(X, Y), UnderRims = 0.0;
		for (int I = -N; I <= N; ++I)
		{
			for (int J = -N; J <= N; ++J)
			{
				const double DX = I * Step, DY = J * Step, Far = DX * DX + DY * DY;
				if (Far > RimOut * RimOut)
				{
					continue;
				}
				const double Here = At(X + DX, Y + DY);
				if (Far <= HeartOut * HeartOut)
				{
					UnderHearts = std::max(UnderHearts, Here);
				}
				else
				{
					UnderRims = std::max(UnderRims, Here);
				}
			}
		}
		// (310 m over the highest found under the hearts, so 300 m over the ground between the points; 200 m under the
		// rims, which fade to nothing and hang a little under the flat base)
		const double Lift = std::max(0.0, std::max(UnderHearts + 310.0, UnderRims + 200.0) - C.At.Z);
		if (Memo)
		{
			Memo->X = X;
			Memo->Y = Y;
			Memo->Lift = Lift;
		}
		return Lift;
	}

	double Field::SquashFor(int CloudIndex, double Lift) const
	{
		if (!Sky.bDeck || Band <= 2000.0)
		{
			return 1.0;
		}
		// (a tile lifted so high that its top would come within 1.5 km of the bombers is thinned; with less than 300 m
		// left, it is not drawn and the mountain stands through the deck; never lower than the deck was laid, where the
		// lowest band put it closer)
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		const double Cap = std::max(Band - 1500.0, Top);
		const double Foot = C.At.Z + Lift;
		if (Foot + C.H <= Cap)
		{
			return 1.0;
		}
		const double Left = Cap - Foot;
		return Left < 300.0 ? 0.0 : Left / C.H;
	}

	double Field::ShownAt(int CloudIndex, double X, double Y) const
	{
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		const double R = std::hypot(X - TargetX, Y - TargetY);
		double Keep = 1.0;
		if (!Sky.bDeck)
		{
			// (the clearing over the target, by a cloud's nearest edge: a sixth as many clouds reaching within 1.5 km of
			// it, all of them from 4 km; a cloud carried in thins away over a fifth of the way, the ones fated first
			// going first)
			const double Edge = std::max(0.0, R - 0.45 * C.W);
			Keep = 0.15 + 0.85 * Smooth((Edge - 1500.0) / 2500.0);
		}
		else if (bShips)
		{
			if (C.bScrap)
			{
				return 1.0;
			}
			// (the ships' hole, ragged (the art director): its edge 3.6 km out give or take three tenths, in lobes and
			// longer along the wind; the tiles thin and break up over a kilometre across the edge, the ones fated first
			// going first, drawn thinner the nearer the hole; none within 2.6 km)
			const double Edge = HoleEdge(X, Y);
			Keep = Smooth((R - (Edge - 500.0)) / 1000.0) * Smooth((R - 2600.0) / 300.0);
			return Smooth((Keep - C.Fate) / 0.2 + 0.5) * (0.45 + 0.55 * Keep);
		}
		else
		{
			return 1.0;
		}
		return Smooth((Keep - C.Fate) / 0.2 + 0.5);
	}

	double Field::HoleEdge(double X, double Y) const
	{
		const double A = std::atan2(Y - TargetY, X - TargetX);
		const double G = 0.6 * std::cos(2.0 * (A - WindWay)) + 0.4 * std::cos(3.0 * A + HoleTurn);
		return 3600.0 * (1.0 + 0.3 * G);
	}

	Placed Field::Place(int CloudIndex, double X, double Y, const Vec& Drift) const
	{
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		const Vec Shift = ShiftNear(CloudIndex, X, Y);
		return PlaceLifted(CloudIndex, Shift, Drift, LiftAt(CloudIndex, C.At.X + Shift.X + Drift.X, C.At.Y + Shift.Y + Drift.Y));
	}

	Placed Field::PlaceLifted(int CloudIndex, const Vec& Shift, const Vec& Drift, double Lift) const
	{
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		Placed Out;
		Out.Shift = Shift;
		Out.Lift = Lift;
		Out.Squash = SquashFor(CloudIndex, Lift);
		Out.Show = Out.Squash > 0.0 ? ShownAt(CloudIndex, C.At.X + Shift.X + Drift.X, C.At.Y + Shift.Y + Drift.Y) : 0.0;
		return Out;
	}

	double Field::CoverOf(const Vec& Drift, double Inside, double Outside) const
	{
		int In = 0, Under = 0;
		const int Grid = 60;
		for (int GX = 0; GX < Grid; ++GX)
		{
			for (int GY = 0; GY < Grid; ++GY)
			{
				const double Dx = -Outside + (GX + 0.5) * 2.0 * Outside / Grid, Dy = -Outside + (GY + 0.5) * 2.0 * Outside / Grid;
				if (Dx * Dx + Dy * Dy < Inside * Inside)
				{
					continue;
				}
				++In;
				// (the point in the laid frame: where the wind has carried the field from)
				const double LX = TargetX + Dx - Drift.X, LY = TargetY + Dy - Drift.Y;
				for (size_t K = 0; K < Clouds.size(); ++K)
				{
					const Cloud& C = Clouds[K];
					const double Ex = Across(C.At.X - LX), Ey = Across(C.At.Y - LY), R = C.W * 0.5 * 0.9;
					if (Ex * Ex + Ey * Ey > R * R)
					{
						continue;
					}
					if (Place(static_cast<int>(K), LX, LY, Drift).Show >= 0.5)
					{
						++Under;
						break;
					}
				}
			}
		}
		return In ? static_cast<double>(Under) / In : 0.0;
	}

	double Field::DeckAt(double X, double Y, double* OutFoot) const
	{
		if (OutFoot)
		{
			*OutFoot = 0.0;
		}
		if (DeckN <= 0 || DeckFirst.size() != static_cast<size_t>(DeckN) * static_cast<size_t>(DeckN) + 1)
		{
			return 0.0;
		}
		double U = X - Origin.X, V = Y - Origin.Y;
		U -= Period * std::floor(U / Period);
		V -= Period * std::floor(V / Period);
		const int CU = std::min(DeckN - 1, std::max(0, static_cast<int>(U / DeckCell)));
		const int CV = std::min(DeckN - 1, std::max(0, static_cast<int>(V / DeckCell)));
		const size_t Cell = static_cast<size_t>(CV) * static_cast<size_t>(DeckN) + static_cast<size_t>(CU);
		// (the tiles whose top puff reaches over the point, nearest first; the first drawn there is the deck's)
		struct Near
		{
			double Far = 0.0;
			int Tile = 0;
			Vec Shift;
		};
		Near Found[16];
		int N = 0;
		for (int K = DeckFirst[Cell]; K < DeckFirst[Cell + 1] && N < 16; ++K)
		{
			const int Tile = DeckList[static_cast<size_t>(K)];
			const Cloud& C = Clouds[static_cast<size_t>(Tile)];
			const Vec Shift = ShiftNear(Tile, X, Y);
			const double Ex = C.At.X + Shift.X - X, Ey = C.At.Y + Shift.Y - Y, Reach8 = 0.8 * Puffs[static_cast<size_t>(C.First)].R;
			const double Far = Ex * Ex + Ey * Ey;
			if (Far <= Reach8 * Reach8)
			{
				Found[N++] = { Far, Tile, Shift };
			}
		}
		std::sort(Found, Found + N, [](const Near& A, const Near& B) { return A.Far < B.Far; });
		for (int K = 0; K < N; ++K)
		{
			const Cloud& C = Clouds[static_cast<size_t>(Found[K].Tile)];
			const Placed Here = PlaceLifted(Found[K].Tile, Found[K].Shift, Vec(), LiftAt(Found[K].Tile, C.At.X + Found[K].Shift.X, C.At.Y + Found[K].Shift.Y));
			if (Here.Squash <= 0.0 || Here.Show < 0.5)
			{
				continue;
			}
			const Puff& TopPuff = Puffs[static_cast<size_t>(C.First)];
			if (OutFoot)
			{
				*OutFoot = C.At.Z + Here.Lift;
			}
			return C.At.Z + Here.Lift + (TopPuff.At.Z - C.At.Z) * Here.Squash;
		}
		return 0.0;
	}

	Field Lay(const Day& D, const Placing& P)
	{
		Field F;
		F.Sky = D;
		F.TargetX = P.TargetX;
		F.TargetY = P.TargetY;
		F.Band = P.Band;
		F.bShips = P.bShips;
		F.MostPuffs = std::max(1, P.MostPuffs);
		F.Ground = P.Ground;
		F.Serial = ++Layings;
		double Base = std::max(150.0, D.Base);
		double Top = std::max(D.Top, Base + 300.0);
		if (D.bDeck)
		{
			F.Moved = PlaceDeck(Base, Top, P.Band, P.LowBand, F.MovedWhy);
		}
		F.Base = Base;
		F.Top = Top;
		F.DeckTop = D.bDeck ? Top : 0.0;
		F.Origin = Vec(P.TargetX - 0.5 * Period, P.TargetY - 0.5 * Period, 0.0);
		const double Thick = Top - Base;

		TheaterFx::Dice Roll(P.Seed * 0x9E3779B97F4A7C15ull + 0x5C1D);
		const uint64_t NoiseSeed = P.Seed * 0xD1B54A32D192ED03ull + 77;
		const Vec Sun = P.Sun.Unit(Vec(-0.35, -0.45, 0.82));
		const Vec SunFlat = Vec(Sun.X, Sun.Y, 0.0).Unit(Vec(0.0, -1.0, 0.0));
		F.WindWay = std::atan2(Wind.Y, Wind.X);
		F.HoleTurn = Roll.U(0.0, 2.0 * Pi);
		// (each puff as laid; the caller sets its edge and its shading where they differ from a soft one's. The puff is
		// the last laid only until the next is: the reference is not kept)
		auto Add = [&F](const Vec& At, double R, double Flat, double Lit, double Alpha, int Lod) -> Puff&
		{
			Puff& Each = F.Puffs.emplace_back();
			Each.At = At;
			Each.R = R;
			Each.Flat = Flat;
			Each.Lit = Clamp(Lit, 0.04, 1.0);
			Each.Alpha = Alpha;
			Each.Cloud = static_cast<int>(F.Clouds.size()) - 1;
			Each.Lod = Lod;
			return Each;
		};

		if (!D.bDeck)
		{
			// Scattered cumulus: about three eighths of the sky, thrown in at random over the laid square, kept apart
			// enough to read as single clouds (a few touch; apart across the square's edges too, as it repeats), until the
			// cover is reached. They are laid at the day's base; the painter lifts each over the ground where it is drawn,
			// and thins the ones that drift into the clearing over the target.
			// (a trade-wind sky, the art director: many small, some middling, a few big; in loose clusters with gaps, in
			// rows along the wind. A place is taken more readily where the rows and the clusters crowd: the rows a wave
			// across the wind that repeats with the square, the wind (3, -2) running along (3, -2) and across (2, 3), three
			// waves to the square's step that way, about 4 km apart; the clusters a broad noise, 3 km a cell)
			const double RowTurn = Roll.U(0.0, 2.0 * Pi);
			auto Crowd = [&F, RowTurn, NoiseSeed](double X, double Y)
			{
				const double LX = X - F.Origin.X, LY = Y - F.Origin.Y;
				const double Row = 0.5 + 0.5 * std::cos(2.0 * Pi * 3.0 * (2.0 * LX + 3.0 * LY) / Period + RowTurn);
				const double Clump = Noise(LX, LY, Period / 14.0, 14, NoiseSeed + 2);
				return 0.55 * Row + 0.45 * Clump;
			};
			const double Wanted = 0.39 * Period * Period;
			double Covered = 0.0;
			for (int Try = 0; Try < 120000 && Covered < Wanted; ++Try)
			{
				const double X = F.Origin.X + Roll.U(0.0, Period), Y = F.Origin.Y + Roll.U(0.0, Period);
				const double Kind = Roll.U();
				const double W = Kind < 0.55 ? Roll.U(300.0, 600.0) : Kind < 0.88 ? Roll.U(600.0, 1500.0) : Roll.U(1500.0, 2500.0);
				const bool bSmall = W < 600.0;
				if (Roll.U() > 0.08 + 0.92 * Smooth((Crowd(X, Y) - 0.3) / 0.45))
				{
					continue;
				}
				bool bClose = false;
				for (const Cloud& O : F.Clouds)
				{
					const double Dx = Across(O.At.X - X), Dy = Across(O.At.Y - Y), Apart = 0.55 * (O.W + W) * 0.5;
					if (Dx * Dx + Dy * Dy < Apart * Apart)
					{
						bClose = true;
						break;
					}
				}
				if (bClose)
				{
					continue;
				}
				// Its height: most shallow, a few to the day's tops, a fair-weather cumulus wider than it is tall (a small
				// one 150 m deep or more).
				const double U1 = Roll.U();
				const double Least = bSmall ? 150.0 : 250.0;
				double H = Thick * (0.3 + 0.7 * U1 * U1);
				H = std::min(H, std::max(Least, W * 0.75));
				H = std::max(H, std::min(Thick, Least));
				const double Foot = Base;
				// (layers close enough that the puffs of one overlap the next: no gap between a cloud's base and its top; a
				// small cloud two layers at most, of three puffs)
				const double Rb = Clamp(W * 0.25, 90.0, 480.0);
				const int Layers = static_cast<int>(Clamp(std::ceil(std::max(0.0, H - 0.5 * Rb - 0.55 * Rb) / (0.5 * Rb)) + 1.0, 2.0, bSmall ? 2.0 : 6.0));
				Cloud& C = F.Clouds.emplace_back();
				C.At = Vec(X, Y, Foot);
				C.W = W;
				C.H = H;
				C.First = static_cast<int>(F.Puffs.size());
				C.Fate = Roll.U(0.05, 0.85);
				double TopW = W * 0.5, TopR = Rb;
				for (int K = 0; K < Layers; ++K)
				{
					const double Hk = static_cast<double>(K) / (Layers - 1);
					const double Rk = Rb * (1.0 - 0.35 * Hk);
					// (its base layer narrower than the body over it, so that from above the lit dome hides it and no grey
					// plate shows round the cloud)
					const double Wk = W * 0.5 * (1.0 - 0.45 * std::pow(Hk, 1.5)) * (K == 0 ? 0.8 : 1.0);
					const double Flat = K == 0 ? 0.55 : 0.85;
					const double Low = Rk * Flat * 0.95, High = Rk * 0.85;
					const double Zk = Foot + Low + std::max(0.0, H - Low - High) * Hk;
					const int N = static_cast<int>(Clamp(std::round((Wk / Rk) * (Wk / Rk) * 1.0), K + 1 < Layers ? 2.0 : 1.0, bSmall ? 3.0 : 10.0));
					for (int J = 0; J < N; ++J)
					{
						const double A = Roll.U(0.0, 2.0 * Pi), Far = std::sqrt(Roll.U()) * std::max(0.0, Wk - 0.85 * Rk);
						const double PX = X + Far * std::cos(A), PY = Y + Far * std::sin(A);
						// (a shallow cloud's upper puffs kept within its height)
						const double PZ = std::min(Foot + H, Zk + (K > 0 ? Roll.U(-0.2, 0.2) * Rk : 0.0));
						const double Side = ((PX - X) * SunFlat.X + (PY - Y) * SunFlat.Y) / std::max(1.0, W * 0.5);
						const double Up = Clamp((PZ - Foot) / std::max(1.0, H), 0.0, 1.0);
						double Lit = 0.2 + 0.65 * Up + 0.25 * Side;
						if (K == 0)
						{
							Lit = std::min(Lit, 0.32);
						}
						// (far from the eye a layer is drawn with its first two puffs only, a cloud under 1.2 km across with its
						// first)
						const int Lod = J < (W < 1200.0 ? 1 : 2) ? 1 : 0;
						// (its base soft at the sides with a straight, crisp underside, solid enough to stand darker than the
						// sky behind it; the lumps over it crisp where the sun catches their edges)
						Puff& Lump = Add(Vec(PX, PY, PZ), Rk * Roll.U(0.7, 1.2), Flat, Lit, K == 0 ? 0.92 : 0.82, Lod);
						Lump.Soft = K == 0 ? 0.45 : 0.2;
						Lump.bFlatFoot = K == 0;
					}
					TopW = Wk;
					TopR = Rk;
				}
				// The cauliflower top: a few smaller lit turrets on a taller cloud.
				const int Turrets = H > 500.0 ? 1 + (H > 900.0 ? 1 : 0) + (Roll.U() < 0.5 ? 1 : 0) : 0;
				for (int J = 0; J < Turrets; ++J)
				{
					const double A = Roll.U(0.0, 2.0 * Pi), Far = Roll.U() * TopW * 0.6;
					const double PX = X + Far * std::cos(A), PY = Y + Far * std::sin(A);
					const double Rt = TopR * Roll.U(0.65, 0.85);
					const double Side = ((PX - X) * SunFlat.X + (PY - Y) * SunFlat.Y) / std::max(1.0, W * 0.5);
					Add(Vec(PX, PY, Foot + H - 0.75 * Rt), Rt, 0.9, 0.9 + 0.1 * Side, 0.8, 1).Soft = 0.18;
				}
				// Its veil: one broad, faint puff round the whole, so its lumps read as one body and not as separate balls
				// (the art director's sheet: never a few round puffs that read as cotton); kept within the body and faint,
				// so it lays no pale disc on the sea round the cloud (the art director: within 1.15 times the body, or half
				// as faint); a small cloud, a few lumps, needs none.
				// (no deeper than the cloud's flat base and 120 m more, at its faded rim: a flat base stays flat)
				if (!bSmall)
				{
					Add(Vec(X, Y, Foot + 0.4 * H), 0.46 * W, Clamp((0.4 * H + 120.0) / (0.46 * W), 0.15, 0.9), 0.55, 0.1, 2).Soft = 0.5;
				}
				// Its far body: farther than its lumps can be told apart, one soft lit puff over most of it, in its veil
				// (its rim 60 m under the base at most).
				Add(Vec(X, Y, Foot + 0.45 * H), 0.42 * W, Clamp(std::min(1.3 * H, 0.45 * H + 60.0) / (0.42 * W), 0.15, 0.9), 0.72, 0.72, 3);
				C.Count = static_cast<int>(F.Puffs.size()) - C.First;
				Covered += Pi * (W * 0.5) * (W * 0.5) * 0.85;
			}
		}
		else
		{
			// A deck: tiles on a jittered grid over the laid square, most of them kept (a few holes where a broad noise,
			// repeating with the square, is low), each a broad flattened puff for the lumpy lit top, darker where it meets
			// the next (a floor of cells, the art director: its troughs 12 to 18 per cent under its tops), up to three lumps
			// of mixed size above it, and on every other tile a grey flat puff for its base, seen from under it. The
			// painter lifts each over the ground where it is drawn, thins it for the bombers, and on a strike at ships
			// keeps a ragged hole over them, a few scraps of cloud in it; it does not drift, so the hole stays there.
			const int TilesAcross = 48;
			const double S = Period / TilesAcross;
			struct Spot
			{
				double X = 0.0, Y = 0.0, R = 0.0;
				int Index = 0;
			};
			std::vector<Spot> Spots;
			for (int GX = 0; GX < TilesAcross; ++GX)
			{
				for (int GY = 0; GY < TilesAcross; ++GY)
				{
					const double X = F.Origin.X + (GX + 0.5 + Roll.U(-0.35, 0.35)) * S;
					const double Y = F.Origin.Y + (GY + 0.5 + Roll.U(-0.35, 0.35)) * S;
					const double LX = X - F.Origin.X, LY = Y - F.Origin.Y;
					const double N = Noise(LX, LY, Period / 9.0, 9, NoiseSeed) * 0.65 + Noise(LX, LY, Period / 27.0, 27, NoiseSeed + 1) * 0.35;
					const double Fate = Roll.U(0.1, 0.9);
					const double R = S * Roll.U(0.62, 0.9);
					// (as many numbers drawn for every place, kept or not, so the laying stays the same where it is kept)
					const double LumpRoll = Roll.U();
					const int Lumps = LumpRoll < 0.35 ? 0 : LumpRoll < 0.7 ? 1 : LumpRoll < 0.9 ? 2 : 3;
					double LumpAt[3][3];
					for (double (&Each)[3] : LumpAt)
					{
						Each[0] = Roll.U(-0.42, 0.42);
						Each[1] = Roll.U(-0.42, 0.42);
						Each[2] = Roll.U(0.25, 0.6);
					}
					if (N < 0.22)
					{
						continue;
					}
					const double Foot = Base, Th = Thick;
					Cloud& C = F.Clouds.emplace_back();
					C.At = Vec(X, Y, Foot);
					C.W = 2.0 * R * 0.9;
					C.H = Th;
					C.First = static_cast<int>(F.Puffs.size());
					C.Fate = Fate;
					// (its top puff flattened so that its body stays within the tile's thickness, as wide as the tile is)
					const double FlatTop = std::min(0.6, 0.95 * Th / R);
					const double HalfTop = 0.55 * R * FlatTop;
					const double TopZ = std::min(Foot + Th, Foot + Th - 0.9 * HalfTop + (N - 0.5) * std::min(160.0, 0.2 * Th));
					Puff& Floor = Add(Vec(X, Y, TopZ), R, FlatTop, 0.9 + 0.1 * Clamp((N - 0.22) / 0.78, 0.0, 1.0), 0.85, 2);
					Floor.Soft = 0.3;
					Floor.Trough = 0.5;
					// (its lumps, small to middling, crisp in the sun; the first drawn out to FarAll, the rest near)
					for (int J = 0; J < Lumps; ++J)
					{
						const double M = std::min(R, 800.0) * LumpAt[J][2];
						Puff& Lump = Add(Vec(X + LumpAt[J][0] * R, Y + LumpAt[J][1] * R, std::min(TopZ + 0.8 * M, Foot + Th)), M, 0.8, 0.95, 0.75, J == 0 ? 1 : 0);
						Lump.Soft = 0.22;
					}
					if ((GX + GY) % 2 == 0)
					{
						// (its grey base: flat, its body's lower edge at the tile's foot, its underside straight)
						const double Rb = R * 1.1;
						const double FlatBase = Clamp(0.3 * std::min(Th, R) / (0.55 * Rb), 0.12, 0.5);
						Puff& Under = Add(Vec(X, Y, Foot + 0.55 * Rb * FlatBase), Rb, FlatBase, 0.12, 0.92, 0);
						Under.bFlatFoot = true;
					}
					C.Count = static_cast<int>(F.Puffs.size()) - C.First;
					Spots.push_back({ X, Y, R, static_cast<int>(F.Clouds.size()) - 1 });
				}
			}
			// A few scraps of cloud left in the ships' hole (the art director): small and thin, at the deck's top, none
			// within 2.4 km of the ships, the hole's edge 700 m off; not tiles of the deck (falls pass through them).
			if (P.bShips)
			{
				const int Scraps = 5 + static_cast<int>(Roll.U() * 3.0);
				for (int K = 0; K < Scraps; ++K)
				{
					const double A = Roll.U(0.0, 2.0 * Pi), W = Roll.U(260.0, 520.0), Out = Roll.U();
					const int Bits = 2 + (Roll.U() < 0.5 ? 1 : 0);
					double BitAt[3][3];
					for (double (&Each)[3] : BitAt)
					{
						Each[0] = Roll.U(-0.2, 0.2);
						Each[1] = Roll.U(-0.2, 0.2);
						Each[2] = Roll.U(0.45, 0.7);
					}
					// (its bits reach 0.63 of its width from its middle at most)
					const double Least = 2400.0 + 0.65 * W, Most = F.HoleEdge(P.TargetX + std::cos(A), P.TargetY + std::sin(A)) - 700.0;
					if (Most <= Least)
					{
						continue;
					}
					const double Rr = Least + Out * (Most - Least);
					const double X = P.TargetX + Rr * std::cos(A), Y = P.TargetY + Rr * std::sin(A);
					const double Th = std::min(Thick, std::max(120.0, 0.35 * W));
					Cloud& C = F.Clouds.emplace_back();
					C.At = Vec(X, Y, Top - Th);
					C.W = W;
					C.H = Th;
					C.First = static_cast<int>(F.Puffs.size());
					C.Fate = 0.0;
					C.bScrap = true;
					for (int J = 0; J < Bits; ++J)
					{
						const double Rj = 0.5 * W * BitAt[J][2];
						Puff& Bit = Add(Vec(X + BitAt[J][0] * W, Y + BitAt[J][1] * W, Top - 0.5 * Th), Rj, Clamp(0.5 * Th / Rj, 0.2, 0.7), 0.85, 0.6, J == 0 ? 1 : 0);
						Bit.Soft = 0.3;
					}
					C.Count = static_cast<int>(F.Puffs.size()) - C.First;
				}
			}
			// (where it is drawn, for falls: each cell of the square keeps the tiles whose top puff's eight tenths reach
			// over any of it, across the square's edges too; DeckAt takes the nearest of them drawn at the point)
			F.DeckN = static_cast<int>(std::round(Period / 240.0));
			F.DeckCell = Period / F.DeckN;
			const size_t Cells = static_cast<size_t>(F.DeckN) * static_cast<size_t>(F.DeckN);
			std::vector<std::vector<int>> ByCell(Cells);
			for (const Spot& T : Spots)
			{
				const double Spread = 0.8 * T.R;
				const double LX = T.X - F.Origin.X, LY = T.Y - F.Origin.Y;
				const int U0 = static_cast<int>(std::floor((LX - Spread) / F.DeckCell)), U1 = static_cast<int>(std::floor((LX + Spread) / F.DeckCell));
				const int V0 = static_cast<int>(std::floor((LY - Spread) / F.DeckCell)), V1 = static_cast<int>(std::floor((LY + Spread) / F.DeckCell));
				for (int V = V0; V <= V1; ++V)
				{
					for (int U = U0; U <= U1; ++U)
					{
						// (the nearest point of the cell to the tile's middle)
						const double Ex = std::max(0.0, std::fabs((U + 0.5) * F.DeckCell - LX) - 0.5 * F.DeckCell);
						const double Ey = std::max(0.0, std::fabs((V + 0.5) * F.DeckCell - LY) - 0.5 * F.DeckCell);
						if (Ex * Ex + Ey * Ey > Spread * Spread)
						{
							continue;
						}
						ByCell[static_cast<size_t>(Wrap(V, F.DeckN)) * static_cast<size_t>(F.DeckN) + static_cast<size_t>(Wrap(U, F.DeckN))].push_back(T.Index);
					}
				}
			}
			F.DeckFirst.assign(Cells + 1, 0);
			for (size_t Cell = 0; Cell < Cells; ++Cell)
			{
				F.DeckFirst[Cell + 1] = F.DeckFirst[Cell] + static_cast<int>(ByCell[Cell].size());
			}
			F.DeckList.reserve(static_cast<size_t>(F.DeckFirst[Cells]));
			for (const std::vector<int>& Each : ByCell)
			{
				F.DeckList.insert(F.DeckList.end(), Each.begin(), Each.end());
			}
		}

		// How far each cloud's puffs reach from its middle, for the ground under it.
		for (Cloud& C : F.Clouds)
		{
			for (int K = C.First; K < C.First + C.Count; ++K)
			{
				const Puff& Each = F.Puffs[static_cast<size_t>(K)];
				const double Off = std::hypot(Each.At.X - C.At.X, Each.At.Y - C.At.Y);
				C.HeartReach = std::max(C.HeartReach, Off + 0.55 * Each.R);
				C.RimReach = std::max(C.RimReach, Off + Each.R);
			}
		}
		if (D.bDeck)
		{
			F.DeckLifts.assign(F.Clouds.size(), Field::LiftMemo());
		}

		// The cover round the target, past its clearing (4 km for cumulus, 4.5 km for a deck's hole over ships).
		F.Cover = F.CoverOf(Vec(), 4500.0, 9000.0);
		return F;
	}

	Painter::Painter(int InSides)
		: SideCount(std::max(6, InSides))
	{
		for (int K = 0; K < SideCount; ++K)
		{
			const double A = 2.0 * Pi * K / SideCount;
			Cos.push_back(std::cos(A));
			Sin.push_back(std::sin(A));
			Cos3.push_back(std::cos(3.0 * A));
			Sin3.push_back(std::sin(3.0 * A));
			Cos2.push_back(std::cos(2.0 * A));
			Sin2.push_back(std::sin(2.0 * A));
		}
		for (int K = 0; K < 97; ++K)
		{
			const double Golden = K * 0.6180339887;
			const double A = (Golden - std::floor(Golden)) * 2.0 * Pi / SideCount;
			TurnCos.push_back(std::cos(A));
			TurnSin.push_back(std::sin(A));
		}
	}

	std::vector<int> Painter::Triangles(int Count) const
	{
		// (the effects kit's own shape: a middle, a heart ring and a rim ring, both faces of every triangle)
		std::vector<int> Out;
		Out.reserve(static_cast<size_t>(std::max(0, Count)) * SideCount * 18);
		const int Per = PerPuff();
		for (int Each = 0; Each < Count; ++Each)
		{
			const int First = Each * Per;
			for (int K = 0; K < SideCount; ++K)
			{
				const int Next = (K + 1) % SideCount;
				const int In0 = First + 1 + K, In1 = First + 1 + Next, Out0 = First + 1 + SideCount + K, Out1 = First + 1 + SideCount + Next;
				const int Tris[18] = { First, In1, In0, First, In0, In1, In0, In1, Out1, In0, Out1, In1, In0, Out1, Out0, In0, Out0, Out1 };
				Out.insert(Out.end(), Tris, Tris + 18);
			}
		}
		return Out;
	}

	double Part::Edge(double X, double Y, double Half) const
	{
		return std::max(0.0, std::hypot(X - AX, Y - AY) - Half);
	}

	int Painter::RoomFor(const Field& F) const
	{
		return Room > 0 ? Room : std::max(1, F.MostPuffs);
	}

	double Painter::Slack() const
	{
		return Loose > 0.0 ? LastSlack : Clamp(LastNear * 0.0045, 1.0, 100.0);
	}

	bool Painter::Stale(const Vec& Eye, const Vec& Drift) const
	{
		if (!bPainted)
		{
			return true;
		}
		// (a near room with nothing in it has nothing to turn until the rooms are drawn again)
		if (!Only.bAll && Only.bNear && LastDrawn == 0)
		{
			return false;
		}
		// (while a drawn cloud's lift eases over high ground, a paint for each metre the wind carries it, whatever the
		// eye does: its lift moves only as it drifts; a far room's, every 3 m, a lift's step there a few pixels at most)
		if (bEasing && (Drift - LastDrift).Length() > (Loose > 0.0 ? 3.0 : 1.0))
		{
			return true;
		}
		// (a far room's clouds thin over the target's clearing and their shadows slide over the land as the wind carries
		// them, whatever the eye does: a turn for every 15 m of carry)
		if (Loose > 0.0 && (Drift - LastDrift).Length() > 15.0)
		{
			return true;
		}
		const double Moved = ((Eye - Drift) - LastEye).Length();
		return Moved > Slack();
	}

	double Painter::ShownLift(int CloudIndex) const
	{
		return CloudIndex >= 0 && static_cast<size_t>(CloudIndex) < Mem->Lifts.size() ? Mem->Lifts[static_cast<size_t>(CloudIndex)].Shown : -1.0;
	}

	double Painter::Paint(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, std::vector<Vec>& OutPoints, std::vector<uint8_t>& OutRgba,
		std::vector<Vec>* OutShadows, std::vector<uint8_t>* OutShadowRgba)
	{
		const bool bApart = OutShadows && OutShadowRgba;
		const int Pool = RoomFor(F);
		const int Per = PerPuff();
		OutPoints.resize(static_cast<size_t>(Pool) * Per);
		OutRgba.resize(static_cast<size_t>(Pool) * Per * 4);
		const Vec Seen = Eye - Drift; // (the eye in the field's own frame)
		const size_t CloudCount = F.Clouds.size();
		if (Mem->Serial != F.Serial || Mem->Lifts.size() != CloudCount)
		{
			Mem->Lifts.assign(CloudCount, Kept());
			Mem->Serial = F.Serial;
		}
		Where.resize(CloudCount);
		Sorted.clear();
		Shaded.clear();
		bEasing = false;
		// (the way the wind carries the clouds, 250 m of it: a cloud's lift is the higher of the ground under it and
		// under where it will be, so it rises before high ground reaches it and the easing never lags under it)
		const Vec Carry = F.DriftAt(1.0);
		const Vec Lead = Carry.Length() > 1e-9 ? Carry * (250.0 / Carry.Length()) : Vec();
		const Vec SunWay = Sun.Unit(Vec(-0.35, -0.45, 0.82));
		// (a clear day's clouds lay their shadows on the sea and the land, down-sun, with the sun up by day: none under a
		// deck, whose sea lies in its shade, and none at night)
		const bool bShadows = !F.Sky.bDeck && SunWay.Z > 0.25 && Light.R + Light.G + Light.B > 1.5;
		for (size_t I = 0; I < CloudCount; ++I)
		{
			const Cloud& C = F.Clouds[I];
			const int Index = static_cast<int>(I);
			// (the copy of the square that puts it nearest the eye; past the reach, not drawn)
			const Vec Shift = F.ShiftNear(Index, Seen.X, Seen.Y);
			const double LX = C.At.X + Shift.X, LY = C.At.Y + Shift.Y;
			if (std::hypot(LX - Seen.X, LY - Seen.Y) > Reach + C.W)
			{
				continue;
			}
			// (the painter's own clouds, by where the cloud's near edge lies (Part); one that is not still lays its shadow,
			// for the painter given the shadows' room)
			const bool bMine = Only.Has(LX, LY, 0.5 * C.W);
			if (!bMine && !(bApart && bShadows))
			{
				continue;
			}
			// (lifted over the ground where it is drawn: the ground looked up again once its middle has moved 30 m, and
			// the lift drawn eased toward it as the cloud drifts, up one and a half metres a metre and down half as fast,
			// so a cloud carried over hills never hops; taken at once when first drawn or when its copy changes)
			const double WX = LX + Drift.X, WY = LY + Drift.Y;
			Kept& Lift = Mem->Lifts[I];
			const double Jump = std::hypot(WX - Lift.PX, WY - Lift.PY);
			bool bStillEasing = false;
			auto LiftHere = [&F, Index, &Lead](double X, double Y)
			{
				const double Now = F.LiftAt(Index, X, Y);
				return Lead.X == 0.0 && Lead.Y == 0.0 ? Now : std::max(Now, F.LiftAt(Index, X + Lead.X, Y + Lead.Y));
			};
			if (Lift.Shown < 0.0 || Jump > 1000.0)
			{
				Lift.X = WX;
				Lift.Y = WY;
				Lift.Target = LiftHere(WX, WY);
				Lift.Shown = Lift.Target;
			}
			else
			{
				if ((WX - Lift.X) * (WX - Lift.X) + (WY - Lift.Y) * (WY - Lift.Y) > 30.0 * 30.0)
				{
					Lift.X = WX;
					Lift.Y = WY;
					Lift.Target = LiftHere(WX, WY);
				}
				Lift.Shown += Clamp(Lift.Target - Lift.Shown, -0.75 * Jump, 1.5 * Jump);
				bStillEasing = std::fabs(Lift.Target - Lift.Shown) > 0.5;
			}
			Lift.PX = WX;
			Lift.PY = WY;
			Placed& Here = Where[I];
			Here = F.PlaceLifted(Index, Shift, Drift, Lift.Shown);
			if (Here.Show < 0.002)
			{
				continue;
			}
			// (thinning into the haze over the last third of the reach; past NearAll, its far puffs only, over its far body
			// as its core, fading in as the near puffs fade out, so that its few lumps read as one cloud; past FarAll its
			// far body alone)
			const Vec Middle(LX, LY, C.At.Z + Here.Lift + 0.5 * Here.Squash * C.H);
			const double Far = (Middle - Seen).Length();
			const double Haze = Smooth((Reach - Far) / (Reach / 3.0));
			if (Haze <= 0.0)
			{
				continue;
			}
			// (only a cloud that is drawn asks for the paints that ease it)
			bEasing = bEasing || (bMine && bStillEasing);
			const double NearShare = Smooth((NearAll + 1000.0 - Far) / 1000.0), FarShare = Smooth((FarAll + 1500.0 - Far) / 1500.0);
			for (int K = C.First; bMine && K < C.First + C.Count; ++K)
			{
				const Puff& Each = F.Puffs[static_cast<size_t>(K)];
				const double Fade = Haze * (Each.Lod == 3 ? 1.0 - NearShare : Each.Lod == 2 ? 1.0 : Each.Lod == 1 ? FarShare : NearShare);
				if (Fade * Here.Show * Each.Alpha < 0.002)
				{
					continue;
				}
				const Vec At(Each.At.X + Shift.X, Each.At.Y + Shift.Y, C.At.Z + Here.Lift + (Each.At.Z - C.At.Z) * Here.Squash);
				const Vec D = At - Seen;
				Sorted.push_back({ K, Index, D.Dot(D), Fade, false, Vec() });
			}
			// (its shadow, where the sun's way through its middle meets the sea: a dark disc of its body's breadth, 1.5 m
			// over the water and laid over the land where it falls there (the art director, on build 1722: over Oahu the
			// clouds floated with nothing under them); out to ShadowReach from the eye; a cloud 600 m across or more)
			// (a painter of one room lays none unless given the shadows' room: the far room's lays them all)
			if (bShadows && (bApart || Only.bAll) && !C.bScrap && C.W >= 600.0)
			{
				const double High = C.At.Z + Here.Lift + 0.45 * Here.Squash * C.H;
				const Vec Lies(LX - SunWay.X / SunWay.Z * High, LY - SunWay.Y / SunWay.Z * High, 1.5);
				const Vec D = Lies - Seen;
				const double Fade = Here.Show * Smooth((ShadowReach - D.Length()) / 2000.0);
				if (Fade * ShadowDark >= 0.002)
				{
					(bApart ? Shaded : Sorted).push_back({ C.First, Index, D.Dot(D), Fade, true, Lies });
				}
			}
		}
		// (far to near, as see-through things are drawn; with no room for them all, the farthest go)
		std::sort(Sorted.begin(), Sorted.end(), [](const Order& A, const Order& B) { return A.Deep > B.Deep; });
		const int Count = static_cast<int>(Sorted.size());
		LastDropped = std::max(0, Count - Pool);
		LastDrawn = Count - LastDropped;
		const int Spare = Pool - LastDrawn;
		auto PlacedAt = [&F, this](const Order& O)
		{
			if (O.bShadow)
			{
				return O.At;
			}
			const Puff& Each = F.Puffs[static_cast<size_t>(O.Index)];
			const Cloud& C = F.Clouds[static_cast<size_t>(O.Cloud)];
			const Placed& Here = Where[static_cast<size_t>(O.Cloud)];
			return Vec(Each.At.X + Here.Shift.X, Each.At.Y + Here.Shift.Y, C.At.Z + Here.Lift + (Each.At.Z - C.At.Z) * Here.Squash);
		};
		// (the room not needed: points where the farthest drawn puff is, nothing to fill)
		const Vec Idle = LastDrawn > 0 ? PlacedAt(Sorted[static_cast<size_t>(LastDropped)]) : Seen;
		for (size_t K = 0; K < static_cast<size_t>(Spare) * Per; ++K)
		{
			OutPoints[K] = Idle;
			OutRgba[K * 4 + 0] = 0;
			OutRgba[K * 4 + 1] = 0;
			OutRgba[K * 4 + 2] = 0;
			OutRgba[K * 4 + 3] = 0;
		}
		auto Byte = [](double V) { return static_cast<uint8_t>(Clamp(V, 0.0, 1.0) * 255.0 + 0.5); };
		auto PutIn = [&Byte](std::vector<Vec>& ToPoints, std::vector<uint8_t>& ToRgba, size_t At, const Vec& Point, const Rgb& Col, double A)
		{
			ToPoints[At] = Point;
			ToRgba[At * 4 + 0] = Byte(Col.R);
			ToRgba[At * 4 + 1] = Byte(Col.G);
			ToRgba[At * 4 + 2] = Byte(Col.B);
			ToRgba[At * 4 + 3] = Byte(A);
		};
		auto Put = [&OutPoints, &OutRgba, &PutIn](size_t At, const Vec& Point, const Rgb& Col, double A) { PutIn(OutPoints, OutRgba, At, Point, Col, A); };
		// (a cloud's colour for how much sun a point of it has: its grey base, its shaded side, its sunlit top, full from
		// 0.9, so a sunlit top grades over its upper part (the art director, 15:17Z); by the hour's light)
		// (under the scene's ink, the shaded side, the base and the shadows painted darker by as much as the ink lifts
		// them: only the sunlit top rises, the art director, 16:31Z)
		const Rgb BaseInked = BaseSide * (1.0 / Ink), ShadeInked = ShadeSide * (1.0 / Ink), ShadowInked = ShadowSide * (1.0 / Ink);
		auto Tone = [&Light, &BaseInked, &ShadeInked](double L)
		{
			const Rgb B = L < 0.35 ? Blend(BaseInked, ShadeInked, Clamp(L, 0.0, 0.35) / 0.35) : Blend(ShadeInked, SunSide, (Clamp(L, 0.35, 0.9) - 0.35) / 0.55);
			return Rgb(B.R * Light.R, B.G * Light.G, B.B * Light.B);
		};
		const Rgb Shade = Rgb(ShadowInked.R * Light.R, ShadowInked.G * Light.G, ShadowInked.B * Light.B);
		const Vec SunFlat = Vec(SunWay.X, SunWay.Y, 0.0).Unit(Vec(0.0, 1.0, 0.0));
		const double Stretch = Clamp(1.0 / std::max(0.25, SunWay.Z), 1.0, 1.8);
		double Near = 1e18;
		// (the loose measure: the least of each drawn puff's own slack, a rim 15 per cent past its radius for its lumps)
		double Slackest = 1e18;
		// (a shadow's points and the ground under each, kept from one shadow to the next)
		std::vector<Vec> Laid(static_cast<size_t>(Per));
		std::vector<double> Under(static_cast<size_t>(Per));
		// A cloud's shadow, into its slot (First) of the points and colours given.
		auto LayShadow = [&](const Order& O, std::vector<Vec>& ToPoints, std::vector<uint8_t>& ToRgba, size_t First)
		{
			const Cloud& Cl = F.Clouds[static_cast<size_t>(O.Cloud)];
			const Vec C = O.At;
			const size_t Phase = static_cast<size_t>(O.Index % 97);
			const double SinP = PhaseSines(false)[Phase], CosP = PhaseSines(true)[Phase];
			auto Lumps3 = [&](int K) { return Sin3[static_cast<size_t>(K)] * CosP + Cos3[static_cast<size_t>(K)] * SinP; };
			auto PutHere = [&](size_t At, const Vec& Point, const Rgb& Col, double A) { PutIn(ToPoints, ToRgba, At, Point, Col, A); };
			// A cloud's shadow: a dark disc, longer down-sun as the sun is lower, soft at its edge; on the sea flat,
			// 1.5 m over the water; over the land each of its points 25 m over the highest ground under it and under
			// its neighbours, so that the ground between them does not stand through it. (The ground is asked once
			// for each point: three times as many asks cost the painter about a millisecond at Pearl Harbor.)
			const double Broad = 0.42 * Cl.W;
			const Vec Along = SunFlat * (Broad * Stretch), Across = Vec(-SunFlat.Y, SunFlat.X, 0.0) * Broad;
			const double A = ShadowDark * O.Fade;
			Laid[0] = C;
			for (int K = 0; K < SideCount; ++K)
			{
				const Vec Way = (Along * Cos[static_cast<size_t>(K)] + Across * Sin[static_cast<size_t>(K)]) * (1.0 + 0.08 * Lumps3(K));
				Laid[static_cast<size_t>(1 + K)] = C + Way * 0.55;
				Laid[static_cast<size_t>(1 + SideCount + K)] = C + Way;
			}
			// (the ground under each point only where there is land under it: the middle and every other point of the
			// rim asked first, and on open sea, most shadows at Midway and many at Pearl Harbor, the disc lies flat)
			auto GroundAt = [&F, &Drift](const Vec& P) { return F.Ground ? std::max(0.0, F.Ground(P.X + Drift.X, P.Y + Drift.Y)) : 0.0; };
			bool bLand = false;
			for (int K = 0; K < SideCount && F.Ground && !bLand; K += 2)
			{
				bLand = GroundAt(Laid[static_cast<size_t>(1 + SideCount + K)]) > 0.5;
			}
			bLand = bLand || (F.Ground && GroundAt(C) > 0.5);
			for (size_t K = 0; K < Laid.size(); ++K)
			{
				Under[K] = bLand ? GroundAt(Laid[K]) : 0.0;
			}
			auto Lay = [](double Highest) { return Highest > 0.5 ? Highest + 25.0 : 1.5; };
			double Middle = Under[0];
			for (int K = 0; K < SideCount; ++K)
			{
				Middle = std::max(Middle, Under[static_cast<size_t>(1 + K)]);
			}
			PutHere(First, Vec(C.X, C.Y, Lay(Middle)), Shade, A);
			for (int K = 0; K < SideCount; ++K)
			{
				const size_t Heart = static_cast<size_t>(1 + K), Rim = static_cast<size_t>(1 + SideCount + K);
				const size_t HeartOn = static_cast<size_t>(1 + (K + 1) % SideCount), HeartBack = static_cast<size_t>(1 + (K + SideCount - 1) % SideCount);
				const size_t RimOn = static_cast<size_t>(1 + SideCount + (K + 1) % SideCount), RimBack = static_cast<size_t>(1 + SideCount + (K + SideCount - 1) % SideCount);
				const double HeartTop = std::max({ Under[Heart], Under[0], Under[HeartOn], Under[HeartBack], Under[Rim] });
				const double RimTop = std::max({ Under[Rim], Under[Heart], Under[RimOn], Under[RimBack] });
				PutHere(First + Heart, Vec(Laid[Heart].X, Laid[Heart].Y, Lay(HeartTop)), Shade, A * 0.8);
				PutHere(First + Rim, Vec(Laid[Rim].X, Laid[Rim].Y, Lay(RimTop)), Shade, 0.0);
			}
		};
		// (the sweep: only a real one, of a millimetre or more and not endless, changes the lens's fade)
		const double Sweep2 = Sweep.X * Sweep.X + Sweep.Y * Sweep.Y + Sweep.Z * Sweep.Z;
		const bool bSweep = std::isfinite(Sweep2) && Sweep2 > 1e-6;
		const double BySweep2 = bSweep ? 1.0 / Sweep2 : 0.0;
		for (int Slot = 0; Slot < LastDrawn; ++Slot)
		{
			const Order& O = Sorted[static_cast<size_t>(LastDropped + Slot)];
			const Puff& Each = F.Puffs[static_cast<size_t>(O.Index)];
			const Cloud& Cl = F.Clouds[static_cast<size_t>(O.Cloud)];
			const Placed& Here = Where[static_cast<size_t>(O.Cloud)];
			const Vec C = PlacedAt(O);
			const size_t First = static_cast<size_t>(Spare + Slot) * Per;
			// (a puff's own lumpiness: its rim pushed in and out a little, differently for each puff; a crisp edge a long
			// swell more, two to a turn (five to a turn, at sixteen points, drew a zigzag of corners: the art director,
			// 15:17Z). The turn's sines from the tables, the puff's own phase added: sin(3t + p) = sin 3t cos p + cos 3t
			// sin p)
			const size_t Phase = static_cast<size_t>(O.Index % 97);
			const double SinP = PhaseSines(false)[Phase], CosP = PhaseSines(true)[Phase], SinP2 = 2.0 * SinP * CosP, CosP2 = CosP * CosP - SinP * SinP;
			auto Lumps3 = [&](int K) { return Sin3[static_cast<size_t>(K)] * CosP + Cos3[static_cast<size_t>(K)] * SinP; };
			auto Lumps2 = [&](int K) { return Sin2[static_cast<size_t>(K)] * CosP2 + Cos2[static_cast<size_t>(K)] * SinP2; };
			const double TurnC = TurnCos[Phase], TurnS = TurnSin[Phase];
			if (O.bShadow)
			{
				LayShadow(O, OutPoints, OutRgba, First);
				continue;
			}
			const double Flat = std::max(0.08, Each.Flat * Here.Squash);
			Vec Fwd = C - Seen;
			const double Dist = Fwd.Length();
			Near = std::min(Near, Dist - Each.R);
			if (Loose > 0.0)
			{
				const double Across = 1.15 * Each.R;
				Slackest = std::min(Slackest, std::min(Loose * Dist * Dist * Dist / (Across * Across), LooseTurn * Dist));
			}
			Fwd = Fwd.Unit(Vec(0.0, 0.0, -1.0));
			Vec Right = Fwd.Cross(Vec(0.0, 0.0, 1.0));
			Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
			Vec Up = Right.Cross(Fwd);
			Up = Up - Vec(0.0, 0.0, Up.Z * (1.0 - Flat)); // (a flat base stays flat seen from the side)
			// (no disc across the lens as the eye passes through a cloud: it fades from 1.2 radii to 0.6; with a sweep, by
			// the nearest the eye comes to it along that way)
			double Lens = Dist;
			if (bSweep)
			{
				const Vec To = C - Seen;
				const double Along = Clamp((To.X * Sweep.X + To.Y * Sweep.Y + To.Z * Sweep.Z) * BySweep2, 0.0, 1.0);
				Lens = (To - Sweep * Along).Length();
			}
			const double Alpha = Here.Show * O.Fade * Each.Alpha * Clamp((Lens - 0.6 * Each.R) / (0.6 * Each.R), 0.0, 1.0);
			// Light and shade from the cloud's shape (the art director): each point of the puff lit as the point of the
			// cloud's rounded body it stands for, its way out from the body's middle to the sun; a deck's tile a flatter
			// body (a floor of cells), darker at its rim where it meets the next. Seen from under it, a cloud is its grey
			// base.
			const Vec Mid(Cl.At.X + Here.Shift.X, Cl.At.Y + Here.Shift.Y, Cl.At.Z + Here.Lift + 0.45 * Here.Squash * Cl.H);
			const double Wide = std::max(60.0, 0.5 * Cl.W * (F.Sky.bDeck && !Cl.bScrap ? 1.25 : 1.0)), Deep = std::max(60.0, 0.5 * Here.Squash * Cl.H);
			const double Below = Clamp((C.Z - Seen.Z) / (2.0 * Each.R), 0.0, 1.0);
			const double ByWide = 1.0 / Wide, ByDeep = 1.0 / Deep;
			// (and, a third of it, as the point of the puff's own round lump it is: lit across the lump, the side toward
			// the sun brighter, so a small cloud's sunlit cap is a matte gradient, not a white pearl: the art director,
			// 15:17Z. Facing is how much the lump's middle, turned to the eye, faces the sun.)
			const double Facing = -(Fwd.X * SunWay.X + Fwd.Y * SunWay.Y + Fwd.Z * SunWay.Z);
			auto LitAt = [&](const Vec& P, double Rim, double Own)
			{
				const double Ox = (P.X - Mid.X) * ByWide, Oy = (P.Y - Mid.Y) * ByWide, Oz = (P.Z - Mid.Z) * ByDeep;
				const double Long = std::sqrt(Ox * Ox + Oy * Oy + Oz * Oz);
				const double Body = Long > 1e-9 ? (Ox * SunWay.X + Oy * SunWay.Y + Oz * SunWay.Z) / Long : SunWay.Z;
				const double Shape = 0.5 + 0.5 * (0.55 * Body + 0.45 * Own);
				return Clamp(0.35 * Each.Lit + 0.65 * Shape - Each.Trough * Rim, 0.0, 1.0) * (1.0 - 0.6 * Below);
			};
			Put(First, C, Tone(LitAt(C, 0.0, Facing)), Alpha);
			const bool bCrisp = Each.Soft < 0.3;
			// (the sun's way across the picture of the puff: its edges toward the sun crisp, those away from it soft, so a
			// lump's shaded edge melts into the cloud under it; little of it when the sun is behind the eye or the puff)
			const double SunAcross = Right.X * SunWay.X + Right.Y * SunWay.Y + Right.Z * SunWay.Z;
			const double SunUp = Up.X * SunWay.X + Up.Y * SunWay.Y + Up.Z * SunWay.Z;
			const double SunInPicture = std::sqrt(SunAcross * SunAcross + SunUp * SunUp);
			// (and never a crisp edge narrower than about six thousandths of a radian, some six pixels across a 1,280 wide
			// picture: a small lump's crisp edge was a step of a hundred in a pixel or two, a "glossy pearl")
			const double LeastSoft = Clamp(0.006 * Dist / std::max(1.0, Each.R), 0.0, 0.7);
			// (how nearly level with the puff or under it the eye is: 1 level or under, 0 from a radius and a half above)
			const double Level = Clamp(1.0 - (Seen.Z - C.Z) / (1.5 * Each.R), 0.0, 1.0);
			for (int K = 0; K < SideCount; ++K)
			{
				const double TurnedC = Cos[static_cast<size_t>(K)] * TurnC - Sin[static_cast<size_t>(K)] * TurnS;
				const double TurnedS = Sin[static_cast<size_t>(K)] * TurnC + Cos[static_cast<size_t>(K)] * TurnS;
				Vec Way = Right * TurnedC + Up * TurnedS;
				double Soft = std::max(Each.Soft, LeastSoft);
				if (bCrisp && SunInPicture > 1e-6)
				{
					const double Away = Clamp(0.75 - 0.75 * (TurnedC * SunAcross + TurnedS * SunUp) / SunInPicture, 0.0, 1.0);
					Soft += (0.7 - Soft) * Away * Away * (3.0 - 2.0 * Away) * std::min(1.0, 2.0 * SunInPicture);
				}
				double Lump = 1.0 + (bCrisp ? 0.05 * Lumps3(K) + 0.04 * Lumps2(K) : 0.08 * Lumps3(K));
				if (Each.bFlatFoot && Way.Z < 0.0 && Level > 0.0)
				{
					// (a base's underside: straight and crisp, seen from the side or under it; from above, the dome hides it)
					Way = Vec(Way.X, Way.Y, Way.Z * (1.0 - 0.7 * Level));
					Soft += (0.12 - Soft) * Level;
					Lump += (1.0 - Lump) * Level;
				}
				const Vec Heart = C + Way * ((1.0 - Soft) * Each.R * Lump), Rim = C + Way * (Each.R * Lump);
				// (the lump's own facing at the heart ring and at the rim: out along the way, and toward the eye less)
				const double WaySun = Way.X * SunWay.X + Way.Y * SunWay.Y + Way.Z * SunWay.Z;
				const double Out = 1.0 - Soft;
				Put(First + 1 + K, Heart, Tone(LitAt(Heart, 0.5, Out * WaySun + std::sqrt(1.0 - Out * Out) * Facing)), Alpha * (1.0 - 0.45 * Soft));
				Put(First + 1 + SideCount + K, Rim, Tone(LitAt(Rim, 1.0, WaySun)), 0.0);
			}
		}
		LastShadows = 0;
		if (bApart)
		{
			// (the shadows' own room: far to near, the nearest MostShadows, the room not needed first)
			std::sort(Shaded.begin(), Shaded.end(), [](const Order& A, const Order& B) { return A.Deep > B.Deep; });
			const int ShadowCount = static_cast<int>(Shaded.size());
			const int ShadowDropped = std::max(0, ShadowCount - MostShadows);
			LastShadows = ShadowCount - ShadowDropped;
			const int ShadowSpare = MostShadows - LastShadows;
			OutShadows->resize(static_cast<size_t>(MostShadows) * Per);
			OutShadowRgba->resize(static_cast<size_t>(MostShadows) * Per * 4);
			const Vec ShadowIdle = LastShadows > 0 ? Shaded[static_cast<size_t>(ShadowDropped)].At : Seen;
			for (size_t K = 0; K < static_cast<size_t>(ShadowSpare) * Per; ++K)
			{
				PutIn(*OutShadows, *OutShadowRgba, K, ShadowIdle, Rgb(0.0f, 0.0f, 0.0f), 0.0);
			}
			for (int Slot = 0; Slot < LastShadows; ++Slot)
			{
				LayShadow(Shaded[static_cast<size_t>(ShadowDropped + Slot)], *OutShadows, *OutShadowRgba, static_cast<size_t>(ShadowSpare + Slot) * Per);
			}
		}
		bPainted = true;
		LastEye = Seen;
		LastDrift = Drift;
		// (nothing drawn: the next paint is wanted as soon as the eye moves, not never)
		LastNear = LastDrawn > 0 ? std::max(50.0, Near) : 50.0;
		// (the loose measure at most 400 m, and 50 m when nothing was drawn)
		LastSlack = LastDrawn > 0 ? Clamp(Slackest, 1.0, 400.0) : 50.0;
		return Near;
	}

	Rooms::Rooms(int InNearRoom, double InMostSplit, double InTau, double InTurn, int InFarRoom)
		: NearPuffs(std::max(16, InNearRoom)), MostSplit(std::max(500.0, InMostSplit))
	{
		Near.SetRoom(NearPuffs);
		Far.SetRoom(std::max(NearPuffs, InFarRoom));
		Near.ShareLifts(Far);
		Far.SetLoose(InTau, InTurn);
	}

	void Rooms::SetInk(double InInk)
	{
		Near.SetInk(InInk);
		Far.SetInk(InInk);
	}

	void Rooms::SetSweep(const Vec& InSweep)
	{
		Near.SetSweep(InSweep);
		Far.SetSweep(InSweep);
	}

	Rooms::Sent Rooms::Turn(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light)
	{
		const Sent What = Wants(Eye, Drift);
		Paint(What, F, Eye, Drift, Sun, Light);
		return What;
	}

	Rooms::Sent Rooms::Wants(const Vec& Eye, const Vec& Drift) const
	{
		if (!bBoth || Far.Stale(Eye, Drift))
		{
			return Sent::Both;
		}
		return Near.Stale(Eye, Drift) ? Sent::Near : Sent::None;
	}

	void Rooms::Paint(Sent What, const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light)
	{
		if (What == Sent::Both)
		{
			PaintBoth(F, Eye, Drift, Sun, Light);
		}
		else if (What == Sent::Near)
		{
			Near.Paint(F, Eye, Drift, Sun, Light, NearPoints, NearRgba);
		}
	}

	void Rooms::PaintBoth(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light)
	{
		const Vec Seen = Eye - Drift;
		Part Cut;
		Cut.bAll = false;
		Cut.AX = Seen.X;
		Cut.AY = Seen.Y;
		// (the line: the clouds nearest the eye across the ground, by their near edges, as many as nine tenths of the near
		// room holds, every puff of each counted; no farther than MostSplit)
		ByNear.clear();
		for (size_t I = 0; I < F.Clouds.size(); ++I)
		{
			const Cloud& C = F.Clouds[I];
			const Vec Shift = F.ShiftNear(static_cast<int>(I), Seen.X, Seen.Y);
			const double Across = Cut.Edge(C.At.X + Shift.X, C.At.Y + Shift.Y, 0.5 * C.W);
			if (Across < MostSplit)
			{
				ByNear.push_back({ Across, C.Count });
			}
		}
		std::sort(ByNear.begin(), ByNear.end());
		const int Fits = NearPuffs * 9 / 10;
		double Line = MostSplit;
		int Held = 0;
		for (const std::pair<double, int>& Each : ByNear)
		{
			if (Held + Each.second > Fits)
			{
				Line = Each.first;
				break;
			}
			Held += Each.second;
		}
		LastLine = Line;
		Cut.Split = Line;
		Cut.bNear = false;
		Far.SetPart(Cut);
		Far.Paint(F, Eye, Drift, Sun, Light, FarPoints, FarRgba, &Shadows, &ShadowRgba);
		Cut.bNear = true;
		Near.SetPart(Cut);
		Near.Paint(F, Eye, Drift, Sun, Light, NearPoints, NearRgba);
		bBoth = true;
	}
}
