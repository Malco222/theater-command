#include "Battle/AirSky.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace AirSky
{
	// (picture colours, as the effects recipe gives its own: a sunlit cloud top a touch warm, its shaded side a cool
	// grey-blue, its flat base darker; the whites near the kit's glycol vapour, 236 to 240, which reads white in the game)
	const Rgb SunSide = Rgb::FromPicture(252, 250, 245);
	const Rgb ShadeSide = Rgb::FromPicture(196, 204, 216);
	const Rgb BaseSide = Rgb::FromPicture(146, 154, 166);

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Clamp(double V, double A, double B) { return std::max(A, std::min(B, V)); }

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
		const int N = static_cast<int>(std::ceil(RimOut / Step));
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
			// (the ships' hole: none within 2.5 km, all from 4.5 km)
			Keep = Smooth((R - 2500.0) / 2000.0);
		}
		else
		{
			return 1.0;
		}
		return Smooth((Keep - C.Fate) / 0.2 + 0.5);
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
		auto Add = [&F](const Vec& At, double R, double Flat, double Lit, double Alpha, int Lod)
		{
			Puff& Each = F.Puffs.emplace_back();
			Each.At = At;
			Each.R = R;
			Each.Flat = Flat;
			Each.Lit = Clamp(Lit, 0.04, 1.0);
			Each.Alpha = Alpha;
			Each.Cloud = static_cast<int>(F.Clouds.size()) - 1;
			Each.Lod = Lod;
		};

		if (!D.bDeck)
		{
			// Scattered cumulus: about three eighths of the sky, thrown in at random over the laid square, kept apart
			// enough to read as single clouds (a few touch; apart across the square's edges too, as it repeats), until the
			// cover is reached. They are laid at the day's base; the painter lifts each over the ground where it is drawn,
			// and thins the ones that drift into the clearing over the target.
			const double Wanted = 0.39 * Period * Period;
			double Covered = 0.0;
			for (int Try = 0; Try < 60000 && Covered < Wanted; ++Try)
			{
				const double X = F.Origin.X + Roll.U(0.0, Period), Y = F.Origin.Y + Roll.U(0.0, Period);
				const double W = 650.0 * std::pow(3.2, std::pow(Roll.U(), 1.4));
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
				// Its height: most shallow, a few to the day's tops, a fair-weather cumulus wider than it is tall.
				const double U1 = Roll.U();
				double H = Thick * (0.3 + 0.7 * U1 * U1);
				H = std::min(H, std::max(250.0, W * 0.75));
				H = std::max(H, std::min(Thick, 250.0));
				const double Foot = Base;
				// (layers close enough that the puffs of one overlap the next: no gap between a cloud's base and its top)
				const double Rb = Clamp(W * 0.25, 120.0, 480.0);
				const int Layers = static_cast<int>(Clamp(std::ceil(std::max(0.0, H - 0.5 * Rb - 0.55 * Rb) / (0.5 * Rb)) + 1.0, 2.0, 6.0));
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
					const double Wk = W * 0.5 * (1.0 - 0.45 * std::pow(Hk, 1.5));
					const double Flat = K == 0 ? 0.55 : 0.85;
					const double Low = Rk * Flat * 0.95, High = Rk * 0.85;
					const double Zk = Foot + Low + std::max(0.0, H - Low - High) * Hk;
					const int N = static_cast<int>(Clamp(std::round((Wk / Rk) * (Wk / Rk) * 1.0), K + 1 < Layers ? 2.0 : 1.0, 10.0));
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
						// (far from the eye a layer is drawn with its first two puffs only)
						const int Lod = J < 2 ? 1 : 0;
						Add(Vec(PX, PY, PZ), Rk * Roll.U(0.7, 1.2), Flat, Lit, K == 0 ? 0.8 : 0.68, Lod);
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
					Add(Vec(PX, PY, Foot + H - 0.75 * Rt), Rt, 0.9, 0.9 + 0.1 * Side, 0.62, 1);
				}
				// Its veil: one broad, faint puff round the whole, so its lumps read as one body and not as separate balls
				// (the art director's sheet: never a few round puffs that read as cotton).
				// (no deeper than the cloud's flat base and 120 m more, at its faded rim: a flat base stays flat)
				Add(Vec(X, Y, Foot + 0.4 * H), 0.5 * W, Clamp((0.4 * H + 120.0) / (0.5 * W), 0.15, 0.9), 0.55, 0.18, 2);
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
			// repeating with the square, is low), each a broad flattened puff for the lumpy lit top, now and then a lump
			// above it, and on every other tile a grey flat puff for its base, seen from under it. The painter lifts each
			// over the ground where it is drawn, thins it for the bombers, and on a strike at ships keeps a hole over
			// them; it does not drift, so the hole stays there.
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
					const double R = S * Roll.U(0.62, 0.8);
					const bool bLump = Roll.U() < 0.25;
					const double LumpX = Roll.U(-0.3, 0.3), LumpY = Roll.U(-0.3, 0.3);
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
					Add(Vec(X, Y, TopZ), R, FlatTop, 0.78 + 0.22 * Clamp((N - 0.22) / 0.78, 0.0, 1.0), 0.85, 2);
					if (bLump)
					{
						const double M = std::min(R, 800.0);
						Add(Vec(X + LumpX * R, Y + LumpY * R, std::min(TopZ + 0.45 * M, Foot + Th)), 0.55 * M, 0.8, 0.95, 0.7, 0);
					}
					if ((GX + GY) % 2 == 0)
					{
						// (its grey base: flat, its body's lower edge at the tile's foot)
						const double Rb = R * 1.1;
						const double FlatBase = Clamp(0.3 * std::min(Th, R) / (0.55 * Rb), 0.12, 0.5);
						Add(Vec(X, Y, Foot + 0.55 * Rb * FlatBase), Rb, FlatBase, 0.12, 0.8, 0);
					}
					C.Count = static_cast<int>(F.Puffs.size()) - C.First;
					Spots.push_back({ X, Y, R, static_cast<int>(F.Clouds.size()) - 1 });
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

	bool Painter::Stale(const Vec& Eye, const Vec& Drift) const
	{
		if (!bPainted)
		{
			return true;
		}
		// (while a drawn cloud's lift eases over high ground, a paint for each metre the wind carries it, whatever the
		// eye does: its lift moves only as it drifts)
		if (bEasing && (Drift - LastDrift).Length() > 1.0)
		{
			return true;
		}
		const double Moved = ((Eye - Drift) - LastEye).Length();
		return Moved > Clamp(LastNear * 0.0045, 1.0, 100.0);
	}

	double Painter::ShownLift(int CloudIndex) const
	{
		return CloudIndex >= 0 && static_cast<size_t>(CloudIndex) < Lifts.size() ? Lifts[static_cast<size_t>(CloudIndex)].Shown : -1.0;
	}

	double Painter::Paint(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, std::vector<Vec>& OutPoints, std::vector<uint8_t>& OutRgba)
	{
		const int Pool = std::max(1, F.MostPuffs);
		const int Per = PerPuff();
		OutPoints.resize(static_cast<size_t>(Pool) * Per);
		OutRgba.resize(static_cast<size_t>(Pool) * Per * 4);
		const Vec Seen = Eye - Drift; // (the eye in the field's own frame)
		const size_t CloudCount = F.Clouds.size();
		if (LastSerial != F.Serial || Lifts.size() != CloudCount)
		{
			Lifts.assign(CloudCount, Kept());
			LastSerial = F.Serial;
		}
		Where.resize(CloudCount);
		Sorted.clear();
		bEasing = false;
		// (the way the wind carries the clouds, 250 m of it: a cloud's lift is the higher of the ground under it and
		// under where it will be, so it rises before high ground reaches it and the easing never lags under it)
		const Vec Carry = F.DriftAt(1.0);
		const Vec Lead = Carry.Length() > 1e-9 ? Carry * (250.0 / Carry.Length()) : Vec();
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
			// (lifted over the ground where it is drawn: the ground looked up again once its middle has moved 30 m, and
			// the lift drawn eased toward it as the cloud drifts, up one and a half metres a metre and down half as fast,
			// so a cloud carried over hills never hops; taken at once when first drawn or when its copy changes)
			const double WX = LX + Drift.X, WY = LY + Drift.Y;
			Kept& Lift = Lifts[I];
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
			// (thinning into the haze over the last third of the reach; past NearAll, its far puffs only; past FarAll its
			// far body, fading in as the rest fade out)
			const Vec Middle(LX, LY, C.At.Z + Here.Lift + 0.5 * Here.Squash * C.H);
			const double Far = (Middle - Seen).Length();
			const double Haze = Smooth((Reach - Far) / (Reach / 3.0));
			if (Haze <= 0.0)
			{
				continue;
			}
			// (only a cloud that is drawn asks for the paints that ease it)
			bEasing = bEasing || bStillEasing;
			const double NearShare = Smooth((NearAll + 1000.0 - Far) / 1000.0), FarShare = Smooth((FarAll + 1500.0 - Far) / 1500.0);
			for (int K = C.First; K < C.First + C.Count; ++K)
			{
				const Puff& Each = F.Puffs[static_cast<size_t>(K)];
				const double Fade = Haze * (Each.Lod == 3 ? 1.0 - FarShare : Each.Lod == 2 ? 1.0 : Each.Lod == 1 ? FarShare : NearShare);
				if (Fade * Here.Show * Each.Alpha < 0.002)
				{
					continue;
				}
				const Vec At(Each.At.X + Shift.X, Each.At.Y + Shift.Y, C.At.Z + Here.Lift + (Each.At.Z - C.At.Z) * Here.Squash);
				const Vec D = At - Seen;
				Sorted.push_back({ K, Index, D.Dot(D), Fade });
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
		const Vec SunWay = Sun.Unit(Vec(-0.35, -0.45, 0.82));
		auto Byte = [](double V) { return static_cast<uint8_t>(Clamp(V, 0.0, 1.0) * 255.0 + 0.5); };
		auto Put = [&OutPoints, &OutRgba, &Byte](size_t At, const Vec& Point, const Rgb& Col, double A)
		{
			OutPoints[At] = Point;
			OutRgba[At * 4 + 0] = Byte(Col.R);
			OutRgba[At * 4 + 1] = Byte(Col.G);
			OutRgba[At * 4 + 2] = Byte(Col.B);
			OutRgba[At * 4 + 3] = Byte(A);
		};
		double Near = 1e18;
		for (int Slot = 0; Slot < LastDrawn; ++Slot)
		{
			const Order& O = Sorted[static_cast<size_t>(LastDropped + Slot)];
			const Puff& Each = F.Puffs[static_cast<size_t>(O.Index)];
			const Placed& Here = Where[static_cast<size_t>(O.Cloud)];
			const Vec C = PlacedAt(O);
			const size_t First = static_cast<size_t>(Spare + Slot) * Per;
			const double Flat = std::max(0.08, Each.Flat * Here.Squash);
			Vec Fwd = C - Seen;
			const double Dist = Fwd.Length();
			Near = std::min(Near, Dist - Each.R);
			Fwd = Fwd.Unit(Vec(0.0, 0.0, -1.0));
			Vec Right = Fwd.Cross(Vec(0.0, 0.0, 1.0));
			Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
			Vec Up = Right.Cross(Fwd);
			Up = Up - Vec(0.0, 0.0, Up.Z * (1.0 - Flat)); // (a flat base stays flat seen from the side)
			// (no disc across the lens as the eye passes through a cloud: it fades from 1.2 radii to 0.6)
			const double Alpha = Here.Show * O.Fade * Each.Alpha * Clamp((Dist - 0.6 * Each.R) / (0.6 * Each.R), 0.0, 1.0);
			// (seen from under it, a cloud is its grey base; seen from above with the sun up, its tops all take the sun)
			const double Below = Clamp((C.Z - Seen.Z) / (2.0 * Each.R), 0.0, 1.0);
			const double Above = Clamp((Seen.Z - C.Z) / (3.0 * Each.R), 0.0, 1.0) * Clamp(SunWay.Z * 1.5, 0.0, 1.0);
			double Lit = Each.Lit * (1.0 - 0.6 * Below);
			Lit += (1.0 - Lit) * 0.45 * Above;
			Rgb Body = Lit < 0.35 ? BaseSide.Mix(ShadeSide, Lit / 0.35) : ShadeSide.Mix(SunSide, (Lit - 0.35) / 0.65);
			Body = Rgb(Body.R * Light.R, Body.G * Light.G, Body.B * Light.B);
			Put(First, C, Body, Alpha);
			// (a puff's own lumpiness: its rim pushed in and out a little, differently for each puff)
			const double Phase = static_cast<double>(O.Index % 97) * 0.61;
			for (int K = 0; K < SideCount; ++K)
			{
				const Vec Way = Right * Cos[static_cast<size_t>(K)] + Up * Sin[static_cast<size_t>(K)];
				const double Toward = Way.Dot(SunWay);
				const double Lump = 1.0 + 0.08 * std::sin(3.0 * (2.0 * Pi * K / SideCount) + Phase);
				Put(First + 1 + K, C + Way * (0.55 * Each.R), Body * (1.0 + 0.10 * Toward), Alpha * 0.8);
				Put(First + 1 + SideCount + K, C + Way * (Each.R * Lump), Body * (1.0 + 0.14 * Toward), 0.0);
			}
		}
		bPainted = true;
		LastEye = Seen;
		LastDrift = Drift;
		// (nothing drawn: the next paint is wanted as soon as the eye moves, not never)
		LastNear = LastDrawn > 0 ? std::max(50.0, Near) : 50.0;
		return Near;
	}
}
